// ============================================
// CORRECTIONS : Logs réduits dans DQBUF
// ============================================

#include "mipi_dsi_cam_v4l2_adapter.h"
#include "esp_video_init.h"
#include "esp_video_buffer.h"
#include "esphome/core/log.h"
#include <cstring>
#include <algorithm>

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
    this->context_.last_frame_sequence = 0;
    this->context_.total_dqbuf_calls = 0;
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
    
    esp_err_t ret = esp_video_register_device(
        0,
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
        if (this->context_.streaming) {
            v4l2_stop(&this->context_, V4L2_BUF_TYPE_VIDEO_CAPTURE);
        }
        
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
    
    if (!ctx->buffers || ctx->buffer_count == 0) {
        ESP_LOGE(TAG, "❌ No buffers allocated. Call VIDIOC_REQBUFS first");
        return ESP_FAIL;
    }
    
    if (!cam->is_streaming()) {
        if (!cam->start_streaming()) {
            ESP_LOGE(TAG, "❌ Failed to start camera streaming");
            return ESP_FAIL;
        }
    }
    
    ctx->streaming = true;
    ctx->frame_count = 0;
    ctx->drop_count = 0;
    ctx->total_dqbuf_calls = 0;
    ctx->last_frame_sequence = cam->get_frame_sequence();
    
    ESP_LOGI(TAG, "✅ Streaming started via V4L2 (%u buffers, initial_seq=%u)", 
             ctx->buffer_count, ctx->last_frame_sequence);
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
    
    if (cam->is_streaming()) {
        cam->stop_streaming();
    }
    
    ctx->streaming = false;
    
    if (ctx->buffers) {
        esp_video_buffer_reset(ctx->buffers);
        ctx->queued_count = 0;
    }
    
    ctx->last_frame_sequence = 0;
    
    ESP_LOGI(TAG, "✅ Streaming stopped (frames: %u, drops: %u, dqbuf_calls: %u)", 
             ctx->frame_count, ctx->drop_count, ctx->total_dqbuf_calls);
    
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_enum_format(void *video, uint32_t type, 
                                                   uint32_t index, uint32_t *pixel_format) {
    if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    static const uint32_t formats[] = {
        V4L2_PIX_FMT_RGB565,
        V4L2_PIX_FMT_SBGGR8,
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
    
    if (pix->width != ctx->camera->get_image_width() || 
        pix->height != ctx->camera->get_image_height()) {
        ESP_LOGE(TAG, "❌ Resolution mismatch: requested %ux%u, camera is %ux%u",
                 pix->width, pix->height, 
                 ctx->camera->get_image_width(), ctx->camera->get_image_height());
        return ESP_ERR_INVALID_ARG;
    }
    
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
    pix->bytesperline = ctx->width * 2;
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
    
    if (ctx->streaming) {
        ESP_LOGE(TAG, "❌ Cannot change buffers while streaming");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (ctx->buffers) {
        esp_video_buffer_destroy(ctx->buffers);
        ctx->buffers = nullptr;
        ctx->buffer_count = 0;
    }
    
    if (req->count > 0) {
        if (req->count > 8) {
            ESP_LOGW(TAG, "⚠️  Limiting buffer count from %u to 8", req->count);
            req->count = 8;
        }
        
        struct esp_video_buffer_info buffer_info = {
            .count = req->count,
            .size = ctx->width * ctx->height * 2,
            .align_size = 64,
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
    buf->m.offset = buf->index;
    
    if (ELEMENT_IS_FREE(elem)) {
        buf->flags = 0;
    } else {
        buf->flags = V4L2_BUF_FLAG_QUEUED;
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
    
    ELEMENT_SET_FREE(elem);
    ELEMENT_SET_ALLOCATED(elem);
    elem->valid_size = 0;
    ctx->queued_count++;
    
    // ✅ CORRECTION : LOGV au lieu de LOGV
    ESP_LOGV(TAG, "V4L2 qbuf[%u] (queued: %u/%u)", 
             buf->index, ctx->queued_count, ctx->buffer_count);
    
    return ESP_OK;
}

// ✅ CORRECTION MAJEURE : Logs réduits dans DQBUF
esp_err_t MipiDsiCamV4L2Adapter::v4l2_dqbuf(void *video, void *buffer) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    struct v4l2_buffer *buf = (struct v4l2_buffer*)buffer;
    
    ctx->total_dqbuf_calls++;
    
    if (!ctx->buffers || !ctx->streaming) {
        ESP_LOGE(TAG, "❌ Not streaming or no buffers");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (ctx->queued_count == 0) {
        // ✅ CORRECTION : Supprimer ce log complètement car il arrive trop souvent
        return ESP_ERR_NOT_FOUND;
    }
    
    if (!ctx->camera->acquire_frame(ctx->last_frame_sequence)) {
        // ✅ CORRECTION : Log seulement tous les 1000 appels au lieu de 100
        if (ctx->total_dqbuf_calls % 1000 == 0) {
            ESP_LOGD(TAG, "DQBUF: Waiting for new frame (last_seq=%u, cam_seq=%u, calls=%u)",
                     ctx->last_frame_sequence,
                     ctx->camera->get_frame_sequence(),
                     ctx->total_dqbuf_calls);
        }
        return ESP_ERR_NOT_FOUND;
    }
    
    uint8_t *camera_data = ctx->camera->get_image_data();
    size_t camera_size = ctx->camera->get_image_size();
    uint32_t current_sequence = ctx->camera->get_current_sequence();
    
    if (!camera_data) {
        ctx->camera->release_frame();
        ESP_LOGE(TAG, "❌ DQBUF: No camera data after acquire");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (current_sequence <= ctx->last_frame_sequence) {
        ctx->camera->release_frame();
        ESP_LOGE(TAG, "❌ DQBUF: Sequence inconsistency! current=%u, last=%u",
                 current_sequence, ctx->last_frame_sequence);
        ctx->drop_count++;
        return ESP_ERR_INVALID_STATE;
    }
    
    struct esp_video_buffer_element *elem = nullptr;
    for (uint32_t i = 0; i < ctx->buffer_count; i++) {
        if (!ELEMENT_IS_FREE(&ctx->buffers->element[i])) {
            elem = &ctx->buffers->element[i];
            break;
        }
    }
    
    if (!elem) {
        ctx->camera->release_frame();
        ctx->drop_count++;
        ESP_LOGW(TAG, "⚠️  No queued buffer available (dropped frames: %u)", ctx->drop_count);
        return ESP_ERR_NOT_FOUND;
    }
    
    size_t copy_size = std::min(camera_size, static_cast<size_t>(ctx->buffers->info.size));
    memcpy(elem->buffer, camera_data, copy_size);
    elem->valid_size = copy_size;
    
    ctx->camera->release_frame();
    
    buf->index = elem->index;
    buf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf->bytesused = copy_size;
    buf->flags = 0;
    buf->field = V4L2_FIELD_NONE;
    buf->memory = V4L2_MEMORY_MMAP;
    buf->m.offset = elem->index;
    buf->length = ctx->buffers->info.size;
    buf->sequence = current_sequence;
    
    gettimeofday(&buf->timestamp, nullptr);
    
    ELEMENT_SET_FREE(elem);
    if (ctx->queued_count > 0) {
        ctx->queued_count--;
    }
    
    ctx->last_frame_sequence = current_sequence;
    ctx->frame_count++;
    
    // ✅ CORRECTION : Log seulement toutes les 300 frames (10 secondes à 30fps) au lieu de 30
    if (ctx->frame_count % 300 == 0) {
        ESP_LOGI(TAG, "✅ DQBUF stats: %u frames, %u drops, %u/%u queued",
                 ctx->frame_count, ctx->drop_count,
                 ctx->queued_count, ctx->buffer_count);
    }
    
    return ESP_OK;
}

esp_err_t MipiDsiCamV4L2Adapter::v4l2_querycap(void *video, void *cap) {
    MipiCameraV4L2Context *ctx = (MipiCameraV4L2Context*)video;
    struct v4l2_capability *capability = (struct v4l2_capability*)cap;
    
    memset(capability, 0, sizeof(*capability));
    
    strncpy((char*)capability->driver, "mipi_dsi_cam", sizeof(capability->driver) - 1);
    strncpy((char*)capability->card, ctx->camera->get_name().c_str(), sizeof(capability->card) - 1);
    strncpy((char*)capability->bus_info, "CSI-MIPI", sizeof(capability->bus_info) - 1);
    
    capability->version = 0x00010000;
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

