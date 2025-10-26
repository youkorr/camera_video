#include "mman.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include "esp_log.h"
#include "../mipi_dsi_cam/mipi_dsi_cam_v4l2_adapter.h"
// Forward declaration des structures V4L2
namespace esphome {
namespace mipi_dsi_cam {
struct MipiCameraV4L2Context;
}
}

// Fonction externe pour obtenir le contexte V4L2 depuis un fd
extern "C" void* get_v4l2_context_from_fd(int fd);

static const char *TAG = "mman";

// Table simple de mapping pour ESP32-P4
struct mmap_entry {
    void *addr;
    size_t length;
    int fd;
    off_t offset;
};

#define MAX_MMAP_ENTRIES 32
static struct mmap_entry mmap_table[MAX_MMAP_ENTRIES];
static bool mmap_table_init = false;

static void init_mmap_table() {
    if (!mmap_table_init) {
        memset(mmap_table, 0, sizeof(mmap_table));
        mmap_table_init = true;
    }
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    init_mmap_table();
    
    ESP_LOGI(TAG, "mmap() called: fd=%d, offset=%ld, length=%zu", fd, offset, length);
    
    // Pour V4L2, récupérer le buffer existant depuis le contexte
    void *v4l2_ctx = get_v4l2_context_from_fd(fd);
    if (!v4l2_ctx) {
        ESP_LOGE(TAG, "❌ No V4L2 context for fd=%d", fd);
        errno = EBADF;
        return MAP_FAILED;
    }
    
    // Cast vers le type concret
    esphome::mipi_dsi_cam::MipiCameraV4L2Context *ctx = 
        (esphome::mipi_dsi_cam::MipiCameraV4L2Context*)v4l2_ctx;
    
    // Vérifier que les buffers existent
    if (!ctx->buffers) {
        ESP_LOGE(TAG, "❌ No buffers allocated in V4L2 context");
        errno = EINVAL;
        return MAP_FAILED;
    }
    
    // L'offset correspond à l'index du buffer dans V4L2
    uint32_t buffer_index = (uint32_t)offset;
    
    if (buffer_index >= ctx->buffer_count) {
        ESP_LOGE(TAG, "❌ Buffer index %u out of range (max: %u)", 
                 buffer_index, ctx->buffer_count);
        errno = EINVAL;
        return MAP_FAILED;
    }
    
    // Récupérer le pointeur du buffer existant
    void *buffer = ctx->buffers->element[buffer_index].buffer;
    
    if (!buffer) {
        ESP_LOGE(TAG, "❌ Buffer %u not allocated", buffer_index);
        errno = ENOMEM;
        return MAP_FAILED;
    }
    
    // Trouver un slot libre dans la table mmap
    int slot = -1;
    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr == nullptr) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        ESP_LOGE(TAG, "❌ mmap table full");
        errno = ENOMEM;
        return MAP_FAILED;
    }
    
    // Enregistrer dans la table (pour munmap)
    mmap_table[slot].addr = buffer;
    mmap_table[slot].length = length;
    mmap_table[slot].fd = fd;
    mmap_table[slot].offset = offset;
    
    ESP_LOGI(TAG, "✅ mmap: fd=%d buffer[%u] -> %p (%zu bytes)", 
             fd, buffer_index, buffer, length);
    
    return buffer;
}

int munmap(void *addr, size_t length) {
    init_mmap_table();
    
    // Chercher l'entrée dans la table
    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr == addr) {
            // NE PAS libérer le buffer - il appartient au V4L2 adapter
            mmap_table[i].addr = nullptr;
            mmap_table[i].length = 0;
            mmap_table[i].fd = -1;
            mmap_table[i].offset = 0;
            
            ESP_LOGD(TAG, "munmap: %p (not freed, belongs to V4L2)", addr);
            return 0;
        }
    }
    
    ESP_LOGW(TAG, "munmap: address %p not found in table", addr);
    errno = EINVAL;
    return -1;
}

#endif // USE_ESP32_VARIANT_ESP32P4
