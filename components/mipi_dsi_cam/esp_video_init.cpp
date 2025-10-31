#include "esp_video_init.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include <fcntl.h>
#include "../lvgl_camera_display/ioctl.h"
#include <errno.h>
#include <cstring>

static const char *TAG = "esp_video_init";

#define MAX_VIDEO_DEVICES 32
#define MAX_OPEN_FDS 64

// Structure pour le device vidéo
typedef struct {
    void *video_device;
    void *user_ctx;
    const void *ops;
    int ref_count;
} esp_video_vfs_t;

// Structure pour mapper fd VFS → device_num
typedef struct {
    int vfs_fd;          // Le fd système complet
    int local_fd;        // L'index dans notre table
    int device_num;
    bool in_use;
} fd_mapping_t;

static esp_video_vfs_t s_video_devices[MAX_VIDEO_DEVICES] = {0};
static fd_mapping_t s_fd_mappings[MAX_OPEN_FDS] = {0};
static bool s_vfs_registered = false;
static void *s_vfs_ctx = nullptr;
static int s_vfs_base_fd = -1;  // ✅ Pour tracker l'offset du VFS

// ✅ NOUVEAU : Table de correspondance simple device_num → local_fd pour accès rapide
static int s_device_to_local_fd[MAX_VIDEO_DEVICES];

// Fonctions de gestion du mapping
static int allocate_fd_mapping(int device_num, int local_fd) {
    if (local_fd < 0 || local_fd >= MAX_OPEN_FDS) {
        ESP_LOGE(TAG, "❌ Invalid local_fd: %d", local_fd);
        return -1;
    }
    
    s_fd_mappings[local_fd].local_fd = local_fd;
    s_fd_mappings[local_fd].device_num = device_num;
    s_fd_mappings[local_fd].in_use = true;
    s_fd_mappings[local_fd].vfs_fd = -1;  // Sera mis à jour plus tard
    
    // Enregistrer aussi dans la table inversée
    if (device_num >= 0 && device_num < MAX_VIDEO_DEVICES) {
        s_device_to_local_fd[device_num] = local_fd;
    }
    
    ESP_LOGI(TAG, "✅ Allocated mapping: local_fd=%d → device_num=%d", local_fd, device_num);
    return local_fd;
}

static int get_device_num_from_vfs_fd(int vfs_fd) {
    // Recherche directe par vfs_fd
    for (int i = 0; i < MAX_OPEN_FDS; i++) {
        if (s_fd_mappings[i].in_use && s_fd_mappings[i].vfs_fd == vfs_fd) {
            ESP_LOGV(TAG, "Found mapping: vfs_fd=%d → device_num=%d", 
                     vfs_fd, s_fd_mappings[i].device_num);
            return s_fd_mappings[i].device_num;
        }
    }
    return -1;
}

static int get_device_num_from_local_fd(int local_fd) {
    if (local_fd < 0 || local_fd >= MAX_OPEN_FDS) {
        return -1;
    }
    
    if (s_fd_mappings[local_fd].in_use) {
        return s_fd_mappings[local_fd].device_num;
    }
    
    return -1;
}

static void free_fd_mapping(int local_fd) {
    if (local_fd >= 0 && local_fd < MAX_OPEN_FDS) {
        int device_num = s_fd_mappings[local_fd].device_num;
        
        ESP_LOGI(TAG, "Freed mapping: local_fd=%d (vfs_fd=%d, device_num=%d)", 
                 local_fd, s_fd_mappings[local_fd].vfs_fd, device_num);
        
        s_fd_mappings[local_fd].in_use = false;
        s_fd_mappings[local_fd].device_num = -1;
        s_fd_mappings[local_fd].vfs_fd = -1;
        
        // Nettoyer la table inversée
        if (device_num >= 0 && device_num < MAX_VIDEO_DEVICES) {
            s_device_to_local_fd[device_num] = -1;
        }
    }
}

// ✅ FONCTION PUBLIQUE AMÉLIORÉE : Recherche intelligente du contexte V4L2
extern "C" void* get_v4l2_context_from_fd(int fd) {
    ESP_LOGV(TAG, "🔍 Looking up V4L2 context for fd=%d", fd);
    
    int device_num = -1;
    
    // Stratégie 1: Chercher par vfs_fd direct
    device_num = get_device_num_from_vfs_fd(fd);
    
    if (device_num < 0) {
        // Stratégie 2: Essayer fd comme local_fd
        device_num = get_device_num_from_local_fd(fd);
        
        if (device_num >= 0) {
            ESP_LOGD(TAG, "Found via local_fd: fd=%d → device_num=%d", fd, device_num);
        }
    }
    
    if (device_num < 0) {
        // Stratégie 3: Essayer différents offsets VFS (bruteforce intelligent)
        if (s_vfs_base_fd > 0) {
            // Si on connaît le base_fd, l'utiliser
            int potential_local_fd = fd - s_vfs_base_fd;
            device_num = get_device_num_from_local_fd(potential_local_fd);
            
            if (device_num >= 0) {
                ESP_LOGD(TAG, "Found using known base_fd=%d: vfs_fd=%d → local_fd=%d → device_num=%d",
                         s_vfs_base_fd, fd, potential_local_fd, device_num);
                
                // Mettre à jour le mapping pour accélérer les prochaines recherches
                s_fd_mappings[potential_local_fd].vfs_fd = fd;
            }
        } else {
            // Essayer différents offsets courants (ESP-IDF utilise typiquement 3-10)
            for (int offset = 0; offset <= 20; offset++) {
                int potential_local_fd = fd - offset;
                if (potential_local_fd >= 0 && potential_local_fd < MAX_OPEN_FDS) {
                    device_num = get_device_num_from_local_fd(potential_local_fd);
                    if (device_num >= 0) {
                        ESP_LOGI(TAG, "✅ Auto-detected VFS base_fd=%d: vfs_fd=%d → local_fd=%d → device_num=%d",
                                 offset, fd, potential_local_fd, device_num);
                        
                        // Sauvegarder le base_fd pour les prochaines fois
                        s_vfs_base_fd = offset;
                        
                        // Mettre à jour le mapping
                        s_fd_mappings[potential_local_fd].vfs_fd = fd;
                        break;
                    }
                }
            }
        }
    }
    
    if (device_num < 0) {
        ESP_LOGE(TAG, "❌ No device mapping for fd=%d", fd);
        
        // Debug : afficher tous les mappings actifs
        ESP_LOGI(TAG, "Active mappings:");
        bool found_any = false;
        for (int i = 0; i < MAX_OPEN_FDS; i++) {
            if (s_fd_mappings[i].in_use) {
                ESP_LOGI(TAG, "  [%d] local_fd=%d, vfs_fd=%d, device_num=%d", 
                         i, s_fd_mappings[i].local_fd, s_fd_mappings[i].vfs_fd,
                         s_fd_mappings[i].device_num);
                found_any = true;
            }
        }
        if (!found_any) {
            ESP_LOGI(TAG, "  (no active mappings)");
        }
        
        return nullptr;
    }
    
    if (!s_video_devices[device_num].video_device) {
        ESP_LOGE(TAG, "❌ Device %d not registered", device_num);
        return nullptr;
    }
    
    ESP_LOGV(TAG, "✅ Resolved fd=%d → device_num=%d → context=%p", 
             fd, device_num, s_video_devices[device_num].video_device);
    
    return s_video_devices[device_num].video_device;
}

// Forward declarations
static int video_open(const char *path, int flags, int mode);
static int video_close(int fd);
static ssize_t video_read(int fd, void *dst, size_t size);
static ssize_t video_write(int fd, const void *src, size_t size);
static int video_ioctl(int fd, int cmd, va_list args);

// Table des opérations VFS
static esp_vfs_t video_vfs;

static void init_video_vfs() {
    memset(&video_vfs, 0, sizeof(video_vfs));
    video_vfs.flags = ESP_VFS_FLAG_DEFAULT;
    video_vfs.write = &video_write;
    video_vfs.open = &video_open;
    video_vfs.close = &video_close;
    video_vfs.read = &video_read;
    video_vfs.ioctl = &video_ioctl;
}

static int video_open(const char *path, int flags, int mode) {
    ESP_LOGI(TAG, "📂 VFS open: %s (flags=0x%x)", path, flags);
    
    int device_num = -1;
    
    // Parser le chemin pour extraire le numéro de device
    const char *video_part = strstr(path, "video");
    if (video_part && sscanf(video_part, "video%d", &device_num) == 1) {
        if (device_num >= 0 && device_num < MAX_VIDEO_DEVICES) {
            if (s_video_devices[device_num].video_device) {
                // Trouver un local_fd libre
                int local_fd = -1;
                for (int i = 0; i < MAX_OPEN_FDS; i++) {
                    if (!s_fd_mappings[i].in_use) {
                        local_fd = i;
                        break;
                    }
                }
                
                if (local_fd < 0) {
                    errno = ENFILE;
                    return -1;
                }
                
                // Créer le mapping
                allocate_fd_mapping(device_num, local_fd);
                
                s_video_devices[device_num].ref_count++;
                
                ESP_LOGI(TAG, "✅ Opened %s → local_fd=%d, device_num=%d (refs=%d)", 
                         path, local_fd, device_num, s_video_devices[device_num].ref_count);
                
                return local_fd;  // Le VFS ajoutera son offset pour créer le vfs_fd
            } else {
                ESP_LOGE(TAG, "❌ Device video%d not registered", device_num);
            }
        } else {
            ESP_LOGE(TAG, "❌ Device number %d out of range", device_num);
        }
    } else {
        ESP_LOGE(TAG, "❌ Invalid path format: %s", path);
    }
    
    errno = ENOENT;
    return -1;
}

static int video_close(int local_fd) {
    int device_num = get_device_num_from_local_fd(local_fd);
    
    ESP_LOGI(TAG, "🔒 VFS close: local_fd=%d → device_num=%d", local_fd, device_num);
    
    if (device_num >= 0 && device_num < MAX_VIDEO_DEVICES) {
        if (s_video_devices[device_num].ref_count > 0) {
            s_video_devices[device_num].ref_count--;
            ESP_LOGI(TAG, "Closed video%d (refs=%d)", 
                     device_num, s_video_devices[device_num].ref_count);
        }
        free_fd_mapping(local_fd);
        return 0;
    }
    
    errno = EBADF;
    return -1;
}

static ssize_t video_read(int fd, void *dst, size_t size) {
    errno = EINVAL;
    return -1;
}

static ssize_t video_write(int fd, const void *src, size_t size) {
    errno = EINVAL;
    return -1;
}

#define IOCTL_TYPE(cmd) (((cmd) >> 8) & 0xFF)
#define IOCTL_NR(cmd)   ((cmd) & 0xFF)

static inline bool ioctl_match(int cmd, char type, int nr) {
    return (IOCTL_TYPE(cmd) == (unsigned char)type) && (IOCTL_NR(cmd) == nr);
}

static int video_ioctl(int local_fd, int cmd, va_list args) {
    int device_num = get_device_num_from_local_fd(local_fd);
    
    if (device_num < 0 || device_num >= MAX_VIDEO_DEVICES) {
        ESP_LOGE(TAG, "❌ Invalid local_fd=%d (device_num=%d)", local_fd, device_num);
        errno = EBADF;
        return -1;
    }
    
    esp_video_vfs_t *vfs_dev = &s_video_devices[device_num];
    if (!vfs_dev->video_device) {
        ESP_LOGE(TAG, "❌ No video device for device_num=%d", device_num);
        errno = ENODEV;
        return -1;
    }
    
    const esp_video_ops *ops = (const esp_video_ops*)vfs_dev->ops;
    void *arg = va_arg(args, void*);
    
    ESP_LOGV(TAG, "ioctl(local_fd=%d, device_num=%d, cmd=0x%08x) - type='%c' nr=%d", 
             local_fd, device_num, cmd, IOCTL_TYPE(cmd), IOCTL_NR(cmd));
    
    esp_err_t ret = ESP_OK;
    
    if (ioctl_match(cmd, 'V', 0)) {
        if (ops && ops->querycap) {
            ret = ops->querycap(vfs_dev->video_device, arg);
        }
    } else if (ioctl_match(cmd, 'V', 4)) {
        if (ops && ops->get_format) {
            ret = ops->get_format(vfs_dev->video_device, arg);
        }
    } else if (ioctl_match(cmd, 'V', 5)) {
        if (ops && ops->set_format) {
            ret = ops->set_format(vfs_dev->video_device, arg);
        }
    } else if (ioctl_match(cmd, 'V', 8)) {
        if (ops && ops->reqbufs) {
            ret = ops->reqbufs(vfs_dev->video_device, arg);
        }
    } else if (ioctl_match(cmd, 'V', 9)) {
        if (ops && ops->querybuf) {
            ret = ops->querybuf(vfs_dev->video_device, arg);
        }
    } else if (ioctl_match(cmd, 'V', 15)) {
        if (ops && ops->qbuf) {
            ret = ops->qbuf(vfs_dev->video_device, arg);
        }
    } else if (ioctl_match(cmd, 'V', 17)) {
        if (ops && ops->dqbuf) {
            ret = ops->dqbuf(vfs_dev->video_device, arg);
        }
    } else if (ioctl_match(cmd, 'V', 18)) {
        if (ops && ops->start) {
            uint32_t type = *(uint32_t*)arg;
            ret = ops->start(vfs_dev->video_device, type);
        }
    } else if (ioctl_match(cmd, 'V', 19)) {
        if (ops && ops->stop) {
            uint32_t type = *(uint32_t*)arg;
            ret = ops->stop(vfs_dev->video_device, type);
        }
    } else if (ioctl_match(cmd, 'V', 28)) {
        // VIDIOC_S_CTRL - ignorer silencieusement
        ret = ESP_OK;
    } else {
        ESP_LOGV(TAG, "Unhandled ioctl cmd=0x%08x", cmd);
        return 0;
    }
    
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NOT_FOUND) {
            errno = EAGAIN;
        } else if (ret == ESP_ERR_INVALID_STATE) {
            errno = EIO;
        } else if (ret == ESP_ERR_INVALID_ARG) {
            errno = EINVAL;
        } else {
            errno = EIO;
        }
        ESP_LOGV(TAG, "ioctl failed: ret=0x%x → errno=%d", ret, errno);
        return -1;
    }
    
    return 0;
}

extern "C" esp_err_t esp_video_register_device(int device_id, void *video_device, void *user_ctx, const void *ops) {
    if (device_id < 0 || device_id >= MAX_VIDEO_DEVICES) {
        ESP_LOGE(TAG, "❌ Invalid device_id: %d (must be 0-%d)", device_id, MAX_VIDEO_DEVICES-1);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_vfs_registered) {
        init_video_vfs();
        s_vfs_ctx = &s_video_devices;
        
        // Initialiser la table inversée
        for (int i = 0; i < MAX_VIDEO_DEVICES; i++) {
            s_device_to_local_fd[i] = -1;
        }
        
        esp_err_t ret = esp_vfs_register("/dev", &video_vfs, s_vfs_ctx);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to register VFS: 0x%x", ret);
            return ret;
        }
        s_vfs_registered = true;
        ESP_LOGI(TAG, "✅ Video VFS registered at /dev");
    }
    
    s_video_devices[device_id].video_device = video_device;
    s_video_devices[device_id].user_ctx = user_ctx;
    s_video_devices[device_id].ops = ops;
    s_video_devices[device_id].ref_count = 0;
    
    ESP_LOGI(TAG, "✅ Registered /dev/video%d (context: %p)", device_id, video_device);
    
    return ESP_OK;
}

extern "C" esp_err_t esp_video_init(const esp_video_init_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid esp_video_init() configuration");
        return ESP_ERR_INVALID_ARG;
    }

#if __has_include("esp_video_device_internal.h")
    ESP_LOGI(TAG, "Calling native Espressif video initialization...");
    return ::esp_video_init(config);
#else
    ESP_LOGI(TAG, "⚙️ esp_video_init() — ESPHome mode with VFS");
    ESP_LOGI(TAG, "CSI:  %s", config->csi ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "DVP:  %s", config->dvp ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "JPEG: %s", config->jpeg ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "ISP:  %s", config->isp ? "ENABLED" : "DISABLED");

    if (!s_vfs_registered) {
        init_video_vfs();
        s_vfs_ctx = &s_video_devices;
        
        // Initialiser la table inversée
        for (int i = 0; i < MAX_VIDEO_DEVICES; i++) {
            s_device_to_local_fd[i] = -1;
        }
        
        esp_err_t ret = esp_vfs_register("/dev", &video_vfs, s_vfs_ctx);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to register video VFS: 0x%x", ret);
            return ret;
        }
        s_vfs_registered = true;
        ESP_LOGI(TAG, "✅ Video VFS registered");
    }

    ESP_LOGI(TAG, "✅ esp_video_init() OK — VFS ready");
    return ESP_OK;
#endif
}

extern "C" esp_err_t esp_video_deinit(void)
{
    ESP_LOGI(TAG, "esp_video_deinit() called");
    
    for (int i = 0; i < MAX_VIDEO_DEVICES; i++) {
        s_video_devices[i].video_device = NULL;
        s_video_devices[i].user_ctx = NULL;
        s_video_devices[i].ops = NULL;
        s_video_devices[i].ref_count = 0;
        s_device_to_local_fd[i] = -1;
    }
    
    for (int i = 0; i < MAX_OPEN_FDS; i++) {
        s_fd_mappings[i].in_use = false;
        s_fd_mappings[i].device_num = -1;
        s_fd_mappings[i].vfs_fd = -1;
    }
    
    s_vfs_base_fd = -1;
    
    if (s_vfs_registered) {
        esp_vfs_unregister("/dev");
        s_vfs_registered = false;
    }
    
    return ESP_OK;
}
