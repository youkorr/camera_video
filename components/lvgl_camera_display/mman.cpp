#include "mman.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_cache.h"
#include "../mipi_dsi_cam/mipi_dsi_cam_v4l2_adapter.h"

static const char *TAG = "mman";

struct mmap_entry {
    void   *addr;
    size_t  length;
    int     vfs_fd;
    off_t   offset;
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

// Vérifie un minimum que le pointeur renvoyé est valide
static bool check_alignment(void *addr) {
    uintptr_t a = (uintptr_t)addr;
    if (a % 4 != 0) {
        ESP_LOGW(TAG, "⚠️  Address %p not 4-byte aligned", addr);
        return false;
    }
    return true;
}

// ----------- mmap() -----------
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    init_mmap_table();

    ESP_LOGI(TAG, "📍 mmap(): fd=%d offset=%ld length=%zu", fd, offset, length);

    // Récupérer le contexte caméra
    auto *ctx = (esphome::mipi_dsi_cam::MipiCameraV4L2Context*) get_v4l2_context_from_fd(fd);
    if (!ctx || !ctx->buffers) {
        ESP_LOGE(TAG, "❌ No V4L2 context for fd=%d", fd);
        errno = EBADF;
        return MAP_FAILED;
    }

    // Comme la démo M5Stack → offset = index du buffer
    uint32_t index = (uint32_t)offset;
    if (index >= ctx->buffer_count) {
        ESP_LOGE(TAG, "❌ Invalid buffer index %u (max %u)", index, ctx->buffer_count);
        errno = EINVAL;
        return MAP_FAILED;
    }

    // Récupérer le buffer matériel (DMA)
    void *buffer = ctx->buffers->element[index].buffer;
    size_t buf_length = ctx->buffers->element[index].size;  // <-- adapte ici si ton champ diffère

    if (!buffer) {
        ESP_LOGE(TAG, "❌ Buffer %u not allocated", index);
        errno = ENOMEM;
        return MAP_FAILED;
    }

    // Invalidation du cache pour s'assurer que les données DMA sont lisibles
    esp_cache_msync(buffer, buf_length, ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    check_alignment(buffer);

    // Enregistrer le mapping (émulation mmap)
    int slot = -1;
    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr == nullptr) {
            slot = i;
            mmap_table[i].addr = buffer;
            mmap_table[i].length = buf_length;
            mmap_table[i].vfs_fd = fd;
            mmap_table[i].offset = offset;
            break;
        }
    }

    if (slot < 0) {
        ESP_LOGE(TAG, "❌ mmap table full");
        errno = ENOMEM;
        return MAP_FAILED;
    }

    ESP_LOGI(TAG, "✅ mmap[%d]: fd=%d buffer[%u] → %p (%zu bytes)",
             slot, fd, index, buffer, buf_length);
    return buffer;
}

// ----------- munmap() -----------
int munmap(void *addr, size_t length) {
    init_mmap_table();

    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr == addr) {
            ESP_LOGD(TAG, "munmap[%d]: %p (fd=%d, offset=%ld, %zu bytes) — V4L2 managed",
                     i, addr, mmap_table[i].vfs_fd, mmap_table[i].offset, length);

            mmap_table[i].addr = nullptr;
            mmap_table[i].length = 0;
            mmap_table[i].vfs_fd = -1;
            mmap_table[i].offset = 0;
            return 0;
        }
    }

    ESP_LOGW(TAG, "❌ munmap: address %p not found", addr);
    errno = EINVAL;
    return -1;
}

#endif // USE_ESP32_VARIANT_ESP32P4


