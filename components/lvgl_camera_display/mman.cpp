#include "mman.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <fcntl.h>
#include <errno.h>
#include "esp_log.h"

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
    
    // Pour V4L2 sur ESP32-P4, mmap retourne directement l'adresse du buffer DMA
    // Ces buffers sont pré-alloués par le driver vidéo
    
    // Trouver un slot libre
    int slot = -1;
    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr == nullptr) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        ESP_LOGE(TAG, "mmap table full");
        errno = ENOMEM;
        return MAP_FAILED;
    }
    
    // Pour V4L2, on utilise l'offset comme index de buffer
    // Le driver vidéo a déjà alloué les buffers en mémoire SPIRAM
    // On retourne simplement un pointeur calculé
    
    // Note: Ceci est une simplification. Dans un vrai driver V4L2,
    // mmap() interagirait avec le driver pour obtenir l'adresse réelle
    // du buffer DMA via un appel ioctl spécial.
    
    // Pour l'instant, on alloue nous-même le buffer
    void *buffer = heap_caps_aligned_alloc(64, length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate mmap buffer");
        errno = ENOMEM;
        return MAP_FAILED;
    }
    
    // Enregistrer dans la table
    mmap_table[slot].addr = buffer;
    mmap_table[slot].length = length;
    mmap_table[slot].fd = fd;
    mmap_table[slot].offset = offset;
    
    ESP_LOGD(TAG, "mmap: fd=%d offset=%ld length=%zu -> %p", fd, offset, length, buffer);
    
    return buffer;
}

int munmap(void *addr, size_t length) {
    init_mmap_table();
    
    // Chercher l'entrée dans la table
    for (int i = 0; i < MAX_MMAP_ENTRIES; i++) {
        if (mmap_table[i].addr == addr) {
            heap_caps_free(addr);
            mmap_table[i].addr = nullptr;
            mmap_table[i].length = 0;
            mmap_table[i].fd = -1;
            mmap_table[i].offset = 0;
            
            ESP_LOGD(TAG, "munmap: %p length=%zu", addr, length);
            return 0;
        }
    }
    
    ESP_LOGW(TAG, "munmap: address %p not found in table", addr);
    errno = EINVAL;
    return -1;
}

#endif // USE_ESP32_VARIANT_ESP32P4
