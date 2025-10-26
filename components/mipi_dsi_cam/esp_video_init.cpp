#include "esp_video_init.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

static const char *TAG = "esp_video_init";

// Structure pour le device vidéo
typedef struct {
    void *video_device;
    void *user_ctx;
    const void *ops;  // pointeur vers esp_video_ops
    int ref_count;
} esp_video_vfs_t;

static esp_video_vfs_t s_video_devices[4] = {0};
static bool s_vfs_registered = false;

// Forward declarations des opérations VFS
static int video_open(const char *path, int flags, int mode);
static int video_close(int fd);
static ssize_t video_read(int fd, void *dst, size_t size);
static ssize_t video_write(int fd, const void *src, size_t size);
static int video_ioctl(int fd, int cmd, va_list args);

// Table des opérations VFS
static const esp_vfs_t video_vfs = {
    .flags = ESP_VFS_FLAG_DEFAULT,
    .write = &video_write,
    .open = &video_open,
    .fstat = NULL,
    .close = &video_close,
    .read = &video_read,
    .fcntl = NULL,
    .ioctl = &video_ioctl,
    .fsync = NULL,
};

// Implémentation VFS
static int video_open(const char *path, int flags, int mode) {
    ESP_LOGI(TAG, "VFS open: %s (flags=0x%x)", path, flags);
    
    // Extraire le numéro de device depuis /dev/videoN
    int device_num = -1;
    if (sscanf(path, "/dev/video%d", &device_num) == 1) {
        if (device_num >= 0 && device_num < 4) {
            if (s_video_devices[device_num].video_device) {
                s_video_devices[device_num].ref_count++;
                // Retourner un fd fictif basé sur le device number
                int fd = 100 + device_num;
                ESP_LOGI(TAG, "✅ Opened video%d -> fd=%d", device_num, fd);
                return fd;
            }
        }
    }
    
    ESP_LOGE(TAG, "❌ Failed to open %s", path, flags);
    errno = ENOENT;
    return -1;
}

static int video_close(int fd) {
    int device_num = fd - 100;
    if (device_num >= 0 && device_num < 4) {
        if (s_video_devices[device_num].ref_count > 0) {
            s_video_devices[device_num].ref_count--;
            ESP_LOGI(TAG, "Closed video%d (refs=%d)", 
                     device_num, s_video_devices[device_num].ref_count);
        }
        return 0;
    }
    errno = EBADF;
    return -1;
}

static ssize_t video_read(int fd, void *dst, size_t size) {
    // Lecture non supportée pour les devices vidéo (utiliser mmap)
    errno = EINVAL;
    return -1;
}

static ssize_t video_write(int fd, const void *src, size_t size) {
    // Écriture non supportée
    errno = EINVAL;
    return -1;
}

// Structure pour les opérations video
struct esp_video_ops {
    esp_err_t (*init)(void *video);
    esp_err_t (*deinit)(void *video);
    esp_err_t (*start)(void *video, uint32_t type);
    esp_err_t (*stop)(void *video, uint32_t type);
    esp_err_t (*enum_format)(void *video, uint32_t type, uint32_t index, uint32_t *pixel_format);
    esp_err_t (*set_format)(void *video, const void *format);
    esp_err_t (*get_format)(void *video, void *format);
    esp_err_t (*reqbufs)(void *video, void *reqbufs);
    esp_err_t (*querybuf)(void *video, void *buffer);
    esp_err_t (*qbuf)(void *video, void *buffer);
    esp_err_t (*dqbuf)(void *video, void *buffer);
    esp_err_t (*querycap)(void *video, void *cap);
};

static int video_ioctl(int fd, int cmd, va_list args) {
    int device_num = fd - 100;
    
    if (device_num < 0 || device_num >= 4) {
        errno = EBADF;
        return -1;
    }
    
    esp_video_vfs_t *vfs_dev = &s_video_devices[device_num];
    if (!vfs_dev->video_device) {
        errno = ENODEV;
        return -1;
    }
    
    const esp_video_ops *ops = (const esp_video_ops*)vfs_dev->ops;
    void *arg = va_arg(args, void*);
    
    ESP_LOGV(TAG, "ioctl(fd=%d, cmd=0x%x) on video%d", fd, cmd, device_num);
    
    // Router les commandes V4L2 vers les opérations appropriées
    esp_err_t ret = ESP_OK;
    
    switch (cmd) {
        case 0xc0505600:  // VIDIOC_QUERYCAP
            if (ops && ops->querycap) {
                ret = ops->querycap(vfs_dev->video_device, arg);
            }
            break;
            
        case 0xc0cc5604:  // VIDIOC_G_FMT
            if (ops && ops->get_format) {
                ret = ops->get_format(vfs_dev->video_device, arg);
            }
            break;
            
        case 0xc0cc5605:  // VIDIOC_S_FMT
            if (ops && ops->set_format) {
                ret = ops->set_format(vfs_dev->video_device, arg);
            }
            break;
            
        case 0xc0145608:  // VIDIOC_REQBUFS
            if (ops && ops->reqbufs) {
                ret = ops->reqbufs(vfs_dev->video_device, arg);
            }
            break;
            
        case 0xc0585609:  // VIDIOC_QUERYBUF
            if (ops && ops->querybuf) {
                ret = ops->querybuf(vfs_dev->video_device, arg);
            }
            break;
            
        case 0xc058560f:  // VIDIOC_QBUF
            if (ops && ops->qbuf) {
                ret = ops->qbuf(vfs_dev->video_device, arg);
            }
            break;
            
        case 0xc0585611:  // VIDIOC_DQBUF
            if (ops && ops->dqbuf) {
                ret = ops->dqbuf(vfs_dev->video_device, arg);
            }
            break;
            
        case 0x40045612:  // VIDIOC_STREAMON
            if (ops && ops->start) {
                uint32_t type = *(uint32_t*)arg;
                ret = ops->start(vfs_dev->video_device, type);
            }
            break;
            
        case 0x40045613:  // VIDIOC_STREAMOFF
            if (ops && ops->stop) {
                uint32_t type = *(uint32_t*)arg;
                ret = ops->stop(vfs_dev->video_device, type);
            }
            break;
            
        default:
            ESP_LOGW(TAG, "Unhandled ioctl 0x%x", cmd);
            return 0;  // Succès par défaut pour les commandes non implémentées
    }
    
    if (ret != ESP_OK) {
        errno = EIO;
        return -1;
    }
    
    return 0;
}

// Fonction pour enregistrer un device vidéo
esp_err_t esp_video_register_device(int device_id, void *video_device, void *user_ctx, const void *ops) {
    if (device_id < 0 || device_id >= 4) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_vfs_registered) {
        esp_err_t ret = esp_vfs_register("/dev", &video_vfs, NULL);
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
    
    ESP_LOGI(TAG, "✅ Registered /dev/video%d", device_id);
    
    return ESP_OK;
}

extern "C" esp_err_t esp_video_init(const esp_video_init_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid esp_video_init() configuration");
        return ESP_ERR_INVALID_ARG;
    }

#if __has_include("esp_video_device_internal.h")
    // Version SDK Espressif complète (automatique)
    ESP_LOGI(TAG, "Calling native Espressif video initialization...");
    return ::esp_video_init(config);
#else
    // Version ESPHome avec VFS complet
    ESP_LOGI(TAG, "⚙️ esp_video_init() — ESPHome mode with VFS");
    ESP_LOGI(TAG, "CSI:  %s", config->csi ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "DVP:  %s", config->dvp ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "JPEG: %s", config->jpeg ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "ISP:  %s", config->isp ? "ENABLED" : "DISABLED");

    // Enregistrer le VFS pour /dev si pas déjà fait
    if (!s_vfs_registered) {
        esp_err_t ret = esp_vfs_register("/dev", &video_vfs, NULL);
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
    
    // Nettoyer les devices
    for (int i = 0; i < 4; i++) {
        s_video_devices[i].video_device = NULL;
        s_video_devices[i].user_ctx = NULL;
        s_video_devices[i].ops = NULL;
        s_video_devices[i].ref_count = 0;
    }
    
    if (s_vfs_registered) {
        esp_vfs_unregister("/dev");
        s_vfs_registered = false;
    }
    
    return ESP_OK;
}
