#include "esp_video_init.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include <fcntl.h>
#include "../lvgl_camera_display/ioctl.h"
#include <errno.h>
#include <cstring>

static const char *TAG = "esp_video_init";

// Structure pour le device vidéo
typedef struct {
    void *video_device;
    void *user_ctx;
    const void *ops;
    int ref_count;
} esp_video_vfs_t;

static esp_video_vfs_t s_video_devices[4] = {0};
static bool s_vfs_registered = false;

// ✅ Contexte global pour le VFS (pointeur vers notre tableau de devices)
static void *s_vfs_ctx = nullptr;

// ✅ FONCTION HELPER : Extraire device_num depuis un fd VFS
// ESP-IDF encode le fd comme : (vfs_index << 12) | local_fd
// On va utiliser local_fd comme device_num
static int get_device_num_from_vfs_fd(int fd) {
    // Le fd local est dans les 12 bits de poids faible
    int local_fd = fd & 0xFFF;
    
    ESP_LOGD(TAG, "get_device_num_from_vfs_fd: fd=%d (0x%x) -> local_fd=%d", 
             fd, fd, local_fd);
    
    // On a encodé device_num dans local_fd
    if (local_fd >= 0 && local_fd < 4) {
        return local_fd;
    }
    
    return -1;
}

// ✅ FONCTION HELPER : Récupérer le contexte V4L2 depuis un fd
extern "C" void* get_v4l2_context_from_fd(int fd) {
    int device_num = get_device_num_from_vfs_fd(fd);
    
    if (device_num < 0 || device_num >= 4) {
        ESP_LOGE(TAG, "❌ Invalid or unmapped fd: %d (device_num=%d)", fd, device_num);
        return nullptr;
    }
    
    if (!s_video_devices[device_num].video_device) {
        ESP_LOGE(TAG, "❌ No video device registered for device_num=%d", device_num);
        return nullptr;
    }
    
    ESP_LOGD(TAG, "✅ Retrieved V4L2 context for fd=%d (device_num=%d) -> %p", 
             fd, device_num, s_video_devices[device_num].video_device);
    
    return s_video_devices[device_num].video_device;
}

// Forward declarations des opérations VFS
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

// Implémentation VFS
static int video_open(const char *path, int flags, int mode) {
    ESP_LOGI(TAG, "VFS open: %s (flags=0x%x)", path, flags);
    
    // Extraire le numéro de device depuis /videoN
    int device_num = -1;
    if (sscanf(path, "/video%d", &device_num) == 1) {
        if (device_num >= 0 && device_num < 4) {
            if (s_video_devices[device_num].video_device) {
                s_video_devices[device_num].ref_count++;
                
                ESP_LOGI(TAG, "✅ Opening video%d -> returning fd=%d (refs=%d)", 
                         device_num, device_num, s_video_devices[device_num].ref_count);
                
                // ✅ IMPORTANT: Retourner directement device_num
                // Le VFS va l'encoder, mais on pourra le retrouver dans les 12 bits bas
                return device_num;
            }
        }
    }
    
    ESP_LOGE(TAG, "❌ Failed to open %s", path);
    errno = ENOENT;
    return -1;
}

static int video_close(int fd) {
    int device_num = get_device_num_from_vfs_fd(fd);
    
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
    errno = EINVAL;
    return -1;
}

static ssize_t video_write(int fd, const void *src, size_t size) {
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

#define IOCTL_TYPE(cmd) (((cmd) >> 8) & 0xFF)
#define IOCTL_NR(cmd)   ((cmd) & 0xFF)

static inline bool ioctl_match(int cmd, char type, int nr) {
    return (IOCTL_TYPE(cmd) == (unsigned char)type) && (IOCTL_NR(cmd) == nr);
}

static int video_ioctl(int fd, int cmd, va_list args) {
    int device_num = get_device_num_from_vfs_fd(fd);
    
    if (device_num < 0 || device_num >= 4) {
        ESP_LOGE(TAG, "❌ Invalid fd=%d (device_num=%d)", fd, device_num);
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
    
    ESP_LOGV(TAG, "ioctl(fd=%d, device_num=%d, cmd=0x%08x) - type='%c' nr=%d", 
             fd, device_num, cmd, IOCTL_TYPE(cmd), IOCTL_NR(cmd));
    
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
    if (device_id < 0 || device_id >= 4) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_vfs_registered) {
        init_video_vfs();
        // ✅ Passer le contexte (notre tableau de devices)
        s_vfs_ctx = &s_video_devices;
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

