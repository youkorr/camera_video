#include "mipi_dsi_cam_v4l2_adapter.h"
#include "esp_video_init.h"
#include "esp_video_buffer.h"
#include "esphome/core/log.h"
#include <cstring>

#ifdef USE_ESP32_VARIANT_ESP32P4

namespace esphome {
namespace mipi_dsi_cam {

static const char *TAG = "mipi_dsi_cam.v4l2";

// Table des opérations V4L2
const esp_video_ops MipiDsiCamV4L2Adapter::s_video_ops = {
    .init = MipiDsiCamV4L2Adapter::v4l2_init,
    .deinit = MipiDsiCamV4L2Adapter::v4l2_deinit,
    .start = MipiDsiCamV4L2Adapter::v4l2_start,
    .stop = MipiDsiCamV4L2Adapter::v4l2_stop,
    .enum_format = MipiDsiCamV4L2Adapter::v4l2_enum_format,
    .set_format = MipiDsiCamV4L2Adapter::v4l2_set_format,
    .get_format = MipiDsiCamV4L2Adapter::v4l2_get_format,
    .reqbufs = MipiDsiCamV4L2Adapter::v4l2_reqbufs,
    .querybuf = MipiDsiCamV4L2Adapter::v4l2_querybuf,
    .qbuf = MipiDsiCamV4L2Adapter::v4l2_qbuf,
    .dqbuf = MipiDsiCamV4L2Adapter::v4l2_dqbuf,
    .querycap = MipiDsiCamV4L2Adapter::v4l2_querycap,
};

MipiDsiCamV4L2Adapter::MipiDsiCamV4L2Adapter(MipiDsiCam *camera) {
    memset(&this->context_, 0, sizeof(this->context_));
    this->context_.camera = camera;
    this->context_.width = camera->get_image_width();
    this->context_.height = camera->get_image_height();
    this->context_.pixelformat = V4L2_PIX_FMT_RGB565;
    this->context_.streaming = false;
    this->context_.buffers = nullptr;
    this->context_.buffer_count = 0;
    this->context_.queued_count = 0;
    this->context_.frame_count = 0;
    this->context_.drop_count = 0;
}

MipiDsiCamV4L2Adapter::~MipiDsiCamV4L2Adapter() {
    if (this->initialized_) {
        this->deinit();
    }
}

esp_err_t MipiDsiCamV4L2Adapter::init() {
    if (this->initialized_) {
        ESP_LOGW(TAG, "V4L2 adapter already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "🎬 Initializing V4L2 adapter...");
    
    // Enregistrer le device dans le VFS
    esp_err_t ret = esp_video_register_device(
        0,  // device_id = 0 pour /dev/video0
        &this->context_,
        this->context_.camera,
        &s_video_ops
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to register video device: 0x%x", ret);
        return ret;
    }
    
    this->context_.video_device = &this->context_;
    this->initialized_ = true;
    
    ESP_LOGI(TAG, "✅ V4L2 adapter initialized successfully");
    ESP_LOGI(TAG, "   Device: /dev/video0");
    ESP_LOGI(TAG, "   Camera: %s", this->context_.camera->get_name().c_str());
    ESP_LOGI(TAG, "   Resolution: %ux%u", 
             this->context_.camera->get_image_width(),
             this->context_.camera->get_image_height());
    
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::deinit() {
    if (this->initialized_) {
        // Arrêter le streaming si actif
        if (this->context_.streaming) {
            v4l2_stop(&this->context_, V4L2_BUF_TYPE_VIDEO_CAPTURE);
        }
        
        // Libérer les buffers
        if (this->context_.buffers) {
            esp_video_buffer_destroy(this->context_.buffers);
            this->context_.buffers = nullptr;
        }
        
        this->initialized_ = false;
        ESP_LOGI(TAG, "V4L2 adapter deinitialized");
    }
    return ESP_OK;
}

// ===== Implémentation des callbacks V4L2 =====

esp_err_t MipiDsiCamV4L2Adapter::v4l2_init(void *video) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    ESP_LOGI(TAG, "V4L2 init callback");
    ESP_LOGI(TAG, "  Camera: %s", ctx->camera->get_name().c_str());
    ESP_LOGI(TAG, "  Resolution: %ux%u", ctx->width, ctx->height);
    ESP_LOGI(TAG, "  Format: RGB565");
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_deinit(void *video) {
    ESP_LOGI(TAG, "V4L2 deinit callback");
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_start(void *video, uint32_t type) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    MipiDsiCam *cam = ctx->camera;
    
    ESP_LOGI(TAG, "V4L2 start streaming (type=%u)", type);
    
    if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        ESP_LOGE(TAG, "❌ Unsupported buffer type: %u", type);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    if (ctx->streaming) {
        ESP_LOGW(TAG, "⚠️  Already streaming");
        return ESP_OK;
    }
    
    // Vérifier qu'on a des buffers alloués
    if (!ctx->buffers || ctx->buffer_count == 0) {
        ESP_LOGE(TAG, "❌ No buffers allocated. Call VIDIOC_REQBUFS first");
        return ESP_FAIL;
    }
    
    // Démarrer le streaming de la caméra
    if (!cam->is_streaming()) {
        if (!cam->start_streaming()) {
            ESP_LOGE(TAG, "❌ Failed to start camera streaming");
            return ESP_FAIL;
        }
    }
    
    ctx->streaming = true;
    ctx->frame_count = 0;
    ctx->drop_count = 0;
    
    ESP_LOGI(TAG, "✅ Streaming started via V4L2 (%u buffers)", ctx->buffer_count);
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_stop(void *video, uint32_t type) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    MipiDsiCam *cam = ctx->camera;
    
    ESP_LOGI(TAG, "V4L2 stop streaming (type=%u)", type);
    
    if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    if (!ctx->streaming) {
        ESP_LOGW(TAG, "⚠️  Not streaming");
        return ESP_OK;
    }
    
    // Arrêter le streaming de la caméra si nécessaire
    if (cam->is_streaming()) {
        cam->stop_streaming();
    }
    
    ctx->streaming = false;
    
    // Reset des buffers
    if (ctx->buffers) {
        esp_video_buffer_reset(ctx->buffers);
        ctx->queued_count = 0;
    }
    
    ESP_LOGI(TAG, "✅ Streaming stopped (frames: %u, drops: %u)", 
             ctx->frame_count, ctx->drop_count);
    
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_enum_format(void *video, uint32_t type, 
                                                   uint32_t index, uint32_t *pixel_format) {
    if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Formats supportés par notre caméra
    static const uint32_t formats[] = {
        V4L2_PIX_FMT_RGB565,   // Format principal
        V4L2_PIX_FMT_SBGGR8,   // RAW8 si besoin
    };
    
    if (index >= sizeof(formats) / sizeof(formats[0])) {
        return ESP_ERR_NOT_FOUND;
    }
    
    *pixel_format = formats[index];
    ESP_LOGD(TAG, "V4L2 enum_format[%u] = 0x%08X", index, *pixel_format);
    
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_set_format(void *video, const void *format) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    const struct v4l2_format *fmt = (const struct v4l2_format*)format;
    
    if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        ESP_LOGE(TAG, "❌ Unsupported buffer type: %u", fmt->type);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    const struct v4l2_pix_format *pix = &fmt->fmt.pix;
    
    ESP_LOGI(TAG, "V4L2 set_format:");
    ESP_LOGI(TAG, "  Resolution: %ux%u", pix->width, pix->height);
    ESP_LOGI(TAG, "  Format: 0x%08X", pix->pixelformat);
    
    // Vérifier que le format demandé correspond au capteur
    if (pix->width != ctx->camera->get_image_width() || 
        pix->height != ctx->camera->get_image_height()) {
        ESP_LOGE(TAG, "❌ Resolution mismatch: requested %ux%u, camera is %ux%u",
                 pix->width, pix->height, 
                 ctx->camera->get_image_width(), ctx->camera->get_image_height());
        return ESP_ERR_INVALID_ARG;
    }
    
    // Vérifier le format de pixel
    if (pix->pixelformat != V4L2_PIX_FMT_RGB565 &&
        pix->pixelformat != V4L2_PIX_FMT_SBGGR8) {
        ESP_LOGE(TAG, "❌ Unsupported pixel format: 0x%08X", pix->pixelformat);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    ctx->width = pix->width;
    ctx->height = pix->height;
    ctx->pixelformat = pix->pixelformat;
    
    ESP_LOGI(TAG, "✅ Format accepted");
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_get_format(void *video, void *format) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    struct v4l2_format *fmt = (struct v4l2_format*)format;
    
    if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    struct v4l2_pix_format *pix = &fmt->fmt.pix;
    
    pix->width = ctx->width;
    pix->height = ctx->height;
    pix->pixelformat = ctx->pixelformat;
    pix->field = V4L2_FIELD_NONE;
    pix->bytesperline = ctx->width * 2; // RGB565 = 2 bytes/pixel
    pix->sizeimage = ctx->width * ctx->height * 2;
    pix->colorspace = V4L2_COLORSPACE_SRGB;
    
    ESP_LOGD(TAG, "V4L2 get_format: %ux%u, format=0x%08X", 
             pix->width, pix->height, pix->pixelformat);
    
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_reqbufs(void *video, void *reqbufs) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    struct v4l2_requestbuffers *req = (struct v4l2_requestbuffers*)reqbufs;
    
    ESP_LOGI(TAG, "V4L2 reqbufs: count=%u, type=%u, memory=%u", 
             req->count, req->type, req->memory);
    
    if (req->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        ESP_LOGE(TAG, "❌ Unsupported buffer type: %u", req->type);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    if (req->memory != V4L2_MEMORY_MMAP) {
        ESP_LOGE(TAG, "❌ Only MMAP memory supported");
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Vérifier qu'on n'est pas en streaming
    if (ctx->streaming) {
        ESP_LOGE(TAG, "❌ Cannot change buffers while streaming");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Libérer les anciens buffers
    if (ctx->buffers) {
        esp_video_buffer_destroy(ctx->buffers);
        ctx->buffers = nullptr;
        ctx->buffer_count = 0;
    }
    
    // Créer les nouveaux buffers
    if (req->count > 0) {
        if (req->count > 8) {
            ESP_LOGW(TAG, "⚠️  Limiting buffer count from %u to 8", req->count);
            req->count = 8;
        }
        
        struct esp_video_buffer_info buffer_info = {
            .count = req->count,
            .size = ctx->width * ctx->height * 2,  // RGB565
            .align_size = 128, //64
            .caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
            .memory_type = V4L2_MEMORY_MMAP
        };
        
        ctx->buffers = esp_video_buffer_create(&buffer_info);
        if (!ctx->buffers) {
            ESP_LOGE(TAG, "❌ Failed to create video buffers");
            return ESP_ERR_NO_MEM;
        }
        
        ctx->buffer_count = req->count;
        ctx->queued_count = 0;
        
        ESP_LOGI(TAG, "✅ Created %u buffers of %u bytes each", 
                 req->count, buffer_info.size);
    } else {
        ESP_LOGI(TAG, "✅ Freed all buffers");
    }
    
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_querybuf(void *video, void *buffer) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    struct v4l2_buffer *buf = (struct v4l2_buffer*)buffer;
    
    if (!ctx->buffers || buf->index >= ctx->buffer_count) {
        ESP_LOGE(TAG, "❌ Invalid buffer index: %u (max: %u)", 
                 buf->index, ctx->buffer_count);
        return ESP_ERR_INVALID_ARG;
    }
    
    struct esp_video_buffer_element *elem = &ctx->buffers->element[buf->index];
    
    buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf->memory = V4L2_MEMORY_MMAP;
    buf->length = ctx->buffers->info.size;
    buf->m.offset = buf->index;  // Utiliser l'index comme offset
    
    // Indiquer l'état du buffer
    if (ELEMENT_IS_FREE(elem)) {
        buf->flags = 0;  // Buffer disponible
    } else {
        buf->flags = V4L2_BUF_FLAG_QUEUED;  // Buffer en file d'attente
    }
    
    ESP_LOGD(TAG, "V4L2 querybuf[%u]: length=%u, offset=%u, flags=0x%x", 
             buf->index, buf->length, buf->m.offset, buf->flags);
    
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_qbuf(void *video, void *buffer) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    struct v4l2_buffer *buf = (struct v4l2_buffer*)buffer;
    
    if (!ctx->buffers || buf->index >= ctx->buffer_count) {
        ESP_LOGE(TAG, "❌ Invalid buffer index: %u", buf->index);
        return ESP_ERR_INVALID_ARG;
    }
    
    struct esp_video_buffer_element *elem = &ctx->buffers->element[buf->index];
    
    if (!ELEMENT_IS_FREE(elem)) {
        ESP_LOGW(TAG, "⚠️  Buffer %u already queued", buf->index);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Marquer le buffer comme prêt à recevoir des données
    ELEMENT_SET_FREE(elem);
    // Marquer le buffer comme possédé par le driver (en attente de capture)
    ELEMENT_SET_ALLOCATED(elem);
    elem->valid_size = 0;
    ctx->queued_count++;
    
    ESP_LOGV(TAG, "V4L2 qbuf[%u] (queued: %u/%u)", 
             buf->index, ctx->queued_count, ctx->buffer_count);
    
    return ESP_OK;
}
    

esp_err_t MipiDsiCamV4L2Adapter::v4l2_dqbuf(void *video, void *buffer) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    struct v4l2_buffer *buf = (struct v4l2_buffer*)buffer;
    
    if (!ctx->buffers || !ctx->streaming) {
        ESP_LOGE(TAG, "❌ Not streaming or no buffers");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Vérifier qu'il y a des buffers queued
    if (ctx->queued_count == 0) {
        ESP_LOGV(TAG, "No buffers queued");
        return ESP_ERR_NOT_FOUND;  // Sera converti en EAGAIN par le VFS
    }
    
    // Capturer une nouvelle frame depuis la caméra
    if (!ctx->camera->capture_frame()) {
        // Pas de frame disponible - comportement non-bloquant
        ESP_LOGV(TAG, "No frame available from camera");
        return ESP_ERR_NOT_FOUND;  // Sera converti en EAGAIN
    }
    
    // Copier les données dans un buffer queued
    uint8_t *camera_data = ctx->camera->get_image_data();
    size_t camera_size = ctx->camera->get_image_size();
    
    if (!camera_data) {
        ESP_LOGE(TAG, "❌ No camera data available");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Trouver un buffer QUEUED (ALLOCATED = owned by driver)
    struct esp_video_buffer_element *elem = nullptr;
    for (uint32_t i = 0; i < ctx->buffer_count; i++) {
        if (!ELEMENT_IS_FREE(&ctx->buffers->element[i])) {
            elem = &ctx->buffers->element[i];
            break;
        }
    }
    
    if (!elem) {
        ctx->drop_count++;
        ESP_LOGW(TAG, "⚠️  No queued buffer available (dropped frames: %u)", ctx->drop_count);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Copier les données
    size_t copy_size = std::min(camera_size, static_cast<size_t>(ctx->buffers->info.size));
    memcpy(elem->buffer, camera_data, copy_size);
    elem->valid_size = copy_size;
    
    // Remplir la structure buffer
    buf->index = elem->index;
    buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf->bytesused = copy_size;
    buf->flags = 0;
    buf->field = V4L2_FIELD_NONE;
    buf->memory = V4L2_MEMORY_MMAP;
    buf->m.offset = elem->index;
    buf->length = ctx->buffers->info.size;
    buf->sequence = ctx->frame_count;
    
    // Timestamp
    gettimeofday(&buf->timestamp, nullptr);
    
    // Marquer le buffer comme dequeued (owned by application)
    ELEMENT_SET_FREE(elem);
    if (ctx->queued_count > 0) {
        ctx->queued_count--;
    }
    ctx->frame_count++;
    
    ESP_LOGD(TAG, "V4L2 dqbuf[%u]: %u bytes (frame %u, queued: %u/%u)", 
             elem->index, copy_size, ctx->frame_count, 
             ctx->queued_count, ctx->buffer_count);
    
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_querycap(void *video, void *cap) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    struct v4l2_capability *capability = (struct v4l2_capability*)cap;
    
    memset(capability, 0, sizeof(*capability));
    
    strncpy((char*)capability->driver, "mipi_dsi_cam", sizeof(capability->driver) - 1);
    strncpy((char*)capability->card, ctx->camera->get_name().c_str(), sizeof(capability->card) - 1);
    strncpy((char*)capability->bus_info, "CSI-MIPI", sizeof(capability->bus_info) - 1);
    
    capability->version = 0x00010000;  // Version 1.0.0
    capability->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
    capability->device_caps = capability->capabilities;
    
    ESP_LOGI(TAG, "V4L2 querycap:");
    ESP_LOGI(TAG, "  Driver: %s", capability->driver);
    ESP_LOGI(TAG, "  Card: %s", capability->card);
    ESP_LOGI(TAG, "  Version: %u.%u.%u", 
             (capability->version >> 16) & 0xFF,
             (capability->version >> 8) & 0xFF,
             capability->version & 0xFF);
    
    return ESP_OK;
}

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
