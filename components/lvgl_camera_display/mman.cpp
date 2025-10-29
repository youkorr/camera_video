#include "mman.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include "esp_log.h"
#include "esp_cache.h"
#include "../mipi_dsi_cam/mipi_dsi_cam_v4l2_adapter.h"
#include "../linux/v4l2-common.h"
#include "../linux/v4l2-controls.h"

namespace esphome {
namespace mipi_dsi_cam {
struct MipiCameraV4L2Context;
}
}

extern "C" void* get_v4l2_context_from_fd(int fd);
extern "C" void esp_video_update_vfs_fd(int local_fd, int vfs_fd);

static const char *TAG = "mman";

struct mmap_entry {
    void *addr;
    size_t length;
    int vfs_fd;
    off_t offset;
};

#define MAX_MMAP_ENTRIES 64
static struct mmap_entry mmap_table[MAX_MMAP_ENTRIES];
static bool mmap_table_init = false;

static void init_mmap_table() {
    if (!mmap_table_init) {
        memset(mmap_table, 0, sizeof(mmap_table));
        mmap_table_init = true;
        ESP_LOGI(TAG, "✅ mmap table initialized (%d entries)", MAX_MMAP_ENTRIES);
    }
}

// ✅ Vérifier l'alignement mémoire pour esp_cache_msync
static bool check_alignment(void *addr, size_t length) {
    uintptr_t addr_int = (uintptr_t)addr;
    
    // Les adresses doivent être alignées sur 4 bytes minimum
    if (addr_int % 4 != 0) {
        ESP_LOGE(TAG, "❌ Address %p not 4-byte aligned", addr);
        return false;
    }
    
    // Vérifier que l'adresse est dans une zone mémoire valide
    // PSRAM: 0x3C000000 - 0x40000000 (ESP32-P4)
    // SRAM: 0x4FF00000 - 0x50000000
    bool in_psram = (addr_int >= 0x3C000000 && addr_int < 0x40000000);
    bool in_sram = (addr_int >= 0x4FF00000 && addr_int < 0x50000000);
    
    if (!in_psram && !in_sram) {
        ESP_LOGW(TAG, "⚠️  Address %p not in expected memory range (PSRAM or SRAM)", addr);
    }
    
    return true;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    init_mmap_table();
    
    ESP_LOGI(TAG, "📍 mmap() called: vfs_fd=%d, offset=%ld, length=%zu", fd, offset, length);
    
    // ✅ fd est maintenant le vfs_fd complet passé par l'appelant
    void *v4l2_ctx = get_v4l2_context_from_fd(fd);
    if (!v4l2_ctx) {
        ESP_LOGE(TAG, "❌ No V4L2 context for vfs_fd=%d", fd);
        
        // Debug : essayer de lire le contexte avec différents fd
        ESP_LOGI(TAG, "Trying alternative fd lookups...");
        for (int test_fd = fd - 5; test_fd <= fd + 5; test_fd++) {
            void *test_ctx = get_v4l2_context_from_fd(test_fd);
            if (test_ctx) {
                ESP_LOGI(TAG, "  ✓ Found context at fd=%d (offset %+d from original)", 
                         test_fd, test_fd - fd);
            }
        }
        
        errno = EBADF;
        return MAP_FAILED;
    }
    
    esphome::mipi_dsi_cam::MipiCameraV4L2Context *ctx = 
        (esphome::mipi_dsi_cam::MipiCameraV4L2Context*)v4l2_ctx;
    
    if (!ctx->buffers) {
        ESP_LOGE(TAG, "❌ No buffers allocated in V4L2 context");
        errno = EINVAL;
        return MAP_FAILED;
    }
    
    uint32_t buffer_index = (uint32_t)offset;
    
    if (buffer_index >= ctx->buffer_count) {
        ESP_LOGE(TAG, "❌ Buffer index %u out of range (max: %u)", 
                 buffer_index, ctx->buffer_count);
        errno = EINVAL;
        return MAP_FAILED;
    }
    
    void *buffer = ctx->buffers->element[buffer_index].buffer;
    
    if (!buffer) {
        ESP_LOGE(TAG, "❌ Buffer %u not allocated", buffer_index);
        errno = ENOMEM;
        return MAP_FAILED;
    }
    
    // ✅ Vérifier l'alignement du buffer
    if (!check_alignment(buffer, length)) {
        ESP_LOGW(TAG, "⚠️  Buffer %u at %p may have alignment issues", buffer_index, buffer);
    }
    
    // Chercher un slot libre dans la table
    int slot = -1;
    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr == nullptr) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        ESP_LOGE(TAG, "❌ mmap table full (%d entries)", MAX_MMAP_ENTRIES);
        errno = ENOMEM;
        return MAP_FAILED;
    }
    
    // Enregistrer le mapping
    mmap_table[slot].addr = buffer;
    mmap_table[slot].length = length;
    mmap_table[slot].vfs_fd = fd;
    mmap_table[slot].offset = offset;
    
    ESP_LOGI(TAG, "✅ mmap[%d]: vfs_fd=%d buffer[%u] → %p (%zu bytes)", 
             slot, fd, buffer_index, buffer, length);
    
    // Log mémoire
    ESP_LOGD(TAG, "   Buffer info: offset=%ld, addr=%p, aligned=%s", 
             offset, buffer, ((uintptr_t)buffer % 4 == 0) ? "YES" : "NO");
    
    return buffer;
}

int munmap(void *addr, size_t length) {
    init_mmap_table();
    
    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr == addr) {
            ESP_LOGD(TAG, "munmap[%d]: %p (vfs_fd=%d, offset=%ld, %zu bytes) - not freed (V4L2 managed)", 
                     i, addr, mmap_table[i].vfs_fd, mmap_table[i].offset, length);
            
            mmap_table[i].addr = nullptr;
            mmap_table[i].length = 0;
            mmap_table[i].vfs_fd = -1;
            mmap_table[i].offset = 0;
            
            return 0;
        }
    }
    
    ESP_LOGW(TAG, "❌ munmap: address %p not found in table (length=%zu)", addr, length);
    
    // Debug : afficher tous les mappings actifs
    bool found_any = false;
    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr != nullptr) {
            if (!found_any) {
                ESP_LOGI(TAG, "Active mmap entries:");
                found_any = true;
            }
            ESP_LOGI(TAG, "  [%d] %p (%zu bytes, vfs_fd=%d, offset=%ld)", 
                     i, mmap_table[i].addr, mmap_table[i].length,
                     mmap_table[i].vfs_fd, mmap_table[i].offset);
        }
    }
    
    if (!found_any) {
        ESP_LOGI(TAG, "No active mmap entries");
    }
    
    errno = EINVAL;
    return -1;
}

#endif // USE_ESP32_VARIANT_ESP32P4

