#include "mman.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_cache.h"
#include "../mipi_dsi_cam/mipi_dsi_cam_v4l2_adapter.h"

namespace esphome {
namespace mipi_dsi_cam {
struct MipiCameraV4L2Context;
}
}

extern "C" void* get_v4l2_context_from_fd(int fd);
extern "C" void  esp_video_update_vfs_fd(int local_fd, int vfs_fd); // (non utilisé ici)

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

// (optionnel) petite section critique si FreeRTOS
#if defined(portMUX_INITIALIZER_UNLOCKED)
static portMUX_TYPE s_mmap_lock = portMUX_INITIALIZER_UNLOCKED;
#define MMAP_LOCK()   taskENTER_CRITICAL(&s_mmap_lock)
#define MMAP_UNLOCK() taskEXIT_CRITICAL(&s_mmap_lock)
#else
#define MMAP_LOCK()
#define MMAP_UNLOCK()
#endif

static void init_mmap_table() {
    if (!mmap_table_init) {
        memset(mmap_table, 0, sizeof(mmap_table));
        mmap_table_init = true;
        ESP_LOGI(TAG, "✅ mmap table initialized (%d entries)", MAX_MMAP_ENTRIES);
    }
}

// ✅ Vérifier l'alignement mémoire pour esp_cache_msync
static bool check_alignment(void *addr, size_t /*length*/) {
    uintptr_t addr_int = (uintptr_t)addr;

    if (addr_int % 4 != 0) {
        ESP_LOGE(TAG, "❌ Address %p not 4-byte aligned", addr);
        return false;
    }

    // (plages indicatives ESP32-P4, c’est juste un warning informatif)
    bool in_psram = (addr_int >= 0x3C000000 && addr_int < 0x40000000);
    bool in_sram  = (addr_int >= 0x4FF00000 && addr_int < 0x50000000);
    if (!in_psram && !in_sram) {
        ESP_LOGW(TAG, "⚠️  Address %p not in expected memory range (PSRAM/SRAM)", addr);
    }
    return true;
}

// -------- Types côté contexte caméra (adapte si tes noms diffèrent) --------
namespace esphome { namespace mipi_dsi_cam {
struct MipiCameraV4L2Buffer {
    void   *buffer;   // adresse DMA
    size_t  length;   // taille réelle du buffer
    off_t   offset;   // <-- stocke ici le m.offset de VIDIOC_QUERYBUF
};
struct MipiCameraV4L2BufferList {
    MipiCameraV4L2Buffer *element; // tableau
};
struct MipiCameraV4L2Context {
    MipiCameraV4L2BufferList *buffers;
    uint32_t buffer_count;
};
}} // namespaces

// Recherche stricte par offset V4L2 (m.offset)
static int find_buffer_index_by_offset(
    const esphome::mipi_dsi_cam::MipiCameraV4L2Context *ctx,
    off_t off)
{
    if (!ctx || !ctx->buffers || !ctx->buffers->element) return -1;
    for (uint32_t i = 0; i < ctx->buffer_count; ++i) {
        if (ctx->buffers->element[i].offset == off)
            return (int)i;
    }
    return -1;
}

#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif

void *mmap(void * /*addr*/, size_t length, int prot, int flags, int fd, off_t offset) {
    init_mmap_table();

    ESP_LOGI(TAG, "📍 mmap(): vfs_fd=%d, offset=%lld, length=%zu, prot=0x%x, flags=0x%x",
             fd, (long long)offset, length, prot, flags);

    if (!(flags & MAP_SHARED)) {
        ESP_LOGW(TAG, "⚠️ mmap without MAP_SHARED — V4L2 expects shared buffers");
    }

    // Récupérer le contexte via fd
    void *v4l2_ctx = get_v4l2_context_from_fd(fd);
    if (!v4l2_ctx) {
        ESP_LOGE(TAG, "❌ No V4L2 context for vfs_fd=%d", fd);
        errno = EBADF;
        return MAP_FAILED;
    }

    auto *ctx = static_cast<esphome::mipi_dsi_cam::MipiCameraV4L2Context*>(v4l2_ctx);
    if (!ctx->buffers || !ctx->buffer_count) {
        ESP_LOGE(TAG, "❌ No buffers allocated in V4L2 context");
        errno = EINVAL;
        return MAP_FAILED;
    }

    // 1) Cas “Linux-like” : offset exact (buf.m.offset)
    int idx = find_buffer_index_by_offset(ctx, offset);

    // 2) Fallback sûr : si offset ressemble à un index
    if (idx < 0 && offset >= 0 && (uint64_t)offset < ctx->buffer_count) {
        idx = (int)offset;
        ESP_LOGW(TAG, "⚠️ Using offset=%lld as index=%d (fallback)",
                 (long long)offset, idx);
    }

    if (idx < 0 || (uint32_t)idx >= ctx->buffer_count) {
        ESP_LOGE(TAG, "❌ Cannot resolve buffer from offset=%lld (count=%u)",
                 (long long)offset, ctx->buffer_count);
        errno = EINVAL;
        return MAP_FAILED;
    }

    // Récupère le buffer et sa taille réelle
    void   *buffer     = ctx->buffers->element[idx].buffer;
    size_t  buf_length = ctx->buffers->element[idx].length;

    if (!buffer) {
        ESP_LOGE(TAG, "❌ Buffer %d not allocated", idx);
        errno = ENOMEM;
        return MAP_FAILED;
    }

    // Clamp de sécurité de la longueur
    if (length == 0 || length > buf_length) {
        ESP_LOGW(TAG, "⚠️ Requested length=%zu clamped to buffer length=%zu (idx=%d)",
                 length, buf_length, idx);
        length = buf_length;
    }

    // Alignement (warning seulement)
    if (!check_alignment(buffer, length)) {
        ESP_LOGW(TAG, "⚠️ Buffer %d at %p may have alignment issues", idx, buffer);
    }

    // Invalidation cache: la caméra a écrit en DMA → CPU doit invalider avant lecture
    // (si tu as un pipeline écriture CPU -> périphérique, fais plutôt un WRITEBACK)
    esp_err_t ce = esp_cache_msync(buffer, length, ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    if (ce != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ cache invalidate failed (err=%d)", (int)ce);
    }

    // Enregistre le mapping (émulation mmap)
    int slot = -1;
    MMAP_LOCK();
    for (int i = 0; i < MAX_MMAP_ENTRIES; ++i) {
        if (mmap_table[i].addr == nullptr) {
            slot = i;
            mmap_table[i].addr   = buffer;
            mmap_table[i].length = length;
            mmap_table[i].vfs_fd = fd;
            mmap_table[i].offset = offset; // on garde l’offset V4L2 original
            break;
        }
    }
    MMAP_UNLOCK();

    if (slot < 0) {
        ESP_LOGE(TAG, "❌ mmap table full (%d entries)", MAX_MMAP_ENTRIES);
        errno = ENOMEM;
        return MAP_FAILED;
    }

    ESP_LOGI(TAG, "✅ mmap[%d]: vfs_fd=%d buffer[%d] → %p (%zu bytes)",
             slot, fd, idx, buffer, length);
    ESP_LOGD(TAG, "   info: offset=%lld, aligned=%s",
             (long long)offset, (((uintptr_t)buffer & 0x3) == 0) ? "YES" : "NO");

    // Sur ESP on “émule” mmap: on renvoie l’adresse DMA directement
    return buffer;
}

int munmap(void *addr, size_t length) {
    init_mmap_table();

    MMAP_LOCK();
    int found = -1;
    for (int i = 0; i < MAX_MMAP_ENTRIES; ++i) {
        if (mmap_table[i].addr == addr) {
            found = i;
            break;
        }
    }

    if (found >= 0) {
        // Si CPU a écrit dans le buffer et que le périph doit relire, tu peux faire un WRITEBACK ici.
        // On ne connaît pas les prot/flags par entrée — si tu veux affiner, ajoute-les dans mmap_entry.
        // esp_cache_msync(addr, mmap_table[found].length, ESP_CACHE_MSYNC_FLAG_WRITEBACK);

        ESP_LOGD(TAG, "munmap[%d]: %p (vfs_fd=%d, offset=%lld, %zu bytes) - not freed (V4L2 managed)",
                 found,
                 addr,
                 mmap_table[found].vfs_fd,
                 (long long)mmap_table[found].offset,
                 length);

        mmap_table[found].addr   = nullptr;
        mmap_table[found].length = 0;
        mmap_table[found].vfs_fd = -1;
        mmap_table[found].offset = 0;

        MMAP_UNLOCK();
        return 0;
    }
    MMAP_UNLOCK();

    ESP_LOGW(TAG, "❌ munmap: address %p not found in table (length=%zu)", addr, length);

    // Debug : afficher tous les mappings actifs
    bool found_any = false;
    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr != nullptr) {
            if (!found_any) {
                ESP_LOGI(TAG, "Active mmap entries:");
                found_any = true;
            }
            ESP_LOGI(TAG, "  [%d] %p (%zu bytes, vfs_fd=%d, offset=%lld)",
                     i,
                     mmap_table[i].addr,
                     mmap_table[i].length,
                     mmap_table[i].vfs_fd,
                     (long long)mmap_table[i].offset);
        }
    }
    if (!found_any) {
        ESP_LOGI(TAG, "No active mmap entries");
    }

    errno = EINVAL;
    return -1;
}

#endif // USE_ESP32_VARIANT_ESP32P4

