#include "mman.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include "esp_log.h"
#include "../mipi_dsi_cam/mipi_dsi_cam_v4l2_adapter.h"

namespace esphome {
namespace mipi_dsi_cam {
struct MipiCameraV4L2Context;
}
}

extern "C" void* get_v4l2_context_from_fd(int fd);

static const char *TAG = "mman";

struct mmap_entry {
    void *addr;
    size_t length;
    int fd;
    off_t offset;
};

#define MAX_MMAP_ENTRIES 64  // ✅ Augmenter pour supporter plus de buffers
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
    
    // ✅ Utiliser get_v4l2_context_from_fd() avec le vrai fd
    void *v4l2_ctx = get_v4l2_context_from_fd(fd);
    if (!v4l2_ctx) {
        ESP_LOGE(TAG, "❌ No V4L2 context for fd=%d", fd);
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
    
    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr == addr) {
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

