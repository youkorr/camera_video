#include "mipi_dsi_cam_v4l2_adapter.h"
#include "esp_video_init.h"
#include "esp_video_buffer.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstring>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

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
extern "C" esp_err_t esp_video_register_device(int device_id, void *video_device,
                                                void *user_ctx, const void *ops);

namespace {

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

static const esp_video_ops s_video_ops = {
    MipiDsiCamV4L2Adapter::v4l2_init,
    MipiDsiCamV4L2Adapter::v4l2_deinit,
    MipiDsiCamV4L2Adapter::v4l2_start,
    MipiDsiCamV4L2Adapter::v4l2_stop,
    MipiDsiCamV4L2Adapter::v4l2_enum_format,
    MipiDsiCamV4L2Adapter::v4l2_set_format,
    MipiDsiCamV4L2Adapter::v4l2_get_format,
    MipiDsiCamV4L2Adapter::v4l2_reqbufs,
    MipiDsiCamV4L2Adapter::v4l2_querybuf,
    MipiDsiCamV4L2Adapter::v4l2_qbuf,
    MipiDsiCamV4L2Adapter::v4l2_dqbuf,
    MipiDsiCamV4L2Adapter::v4l2_querycap,
};

}  // namespace

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
@@ -127,82 +154,85 @@ esp_err_t MipiDsiCamV4L2Adapter::v4l2_start(void *video, uint32_t type) {
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
    
    ctx->last_frame_sequence = ctx->camera->get_frame_sequence();

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

    ctx->last_frame_sequence = ctx->camera->get_frame_sequence();
    
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
@@ -389,111 +419,114 @@ esp_err_t MipiDsiCamV4L2Adapter::v4l2_qbuf(void *video, void *buffer) {
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
    // Capturer une nouvelle frame depuis la caméra.
    // Attendre légèrement pour éviter de signaler des drops côté application
    // lorsque la caméra fonctionne à un framerate plus bas que le client.
    const int64_t timeout_us = 50 * 1000;  // ~50 ms ≈ 20 FPS minimum garanti
    const int64_t deadline = esp_timer_get_time() + timeout_us;

    while (!ctx->camera->acquire_frame(ctx->last_frame_sequence)) {
        if (esp_timer_get_time() >= deadline) {
            ESP_LOGV(TAG, "No new frame available (seq=%u)", ctx->last_frame_sequence);
            return ESP_ERR_NOT_FOUND;  // Sera converti en EAGAIN
        }
        vTaskDelay(pdMS_TO_TICKS(1));
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

