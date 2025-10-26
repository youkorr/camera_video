#include "esp_video.h"
#include "esp_log.h"
#include "esp_video_device.h"  // ton header

static const char *TAG = "mipi_dsi_cam_video";

esp_err_t mipi_dsi_cam_video_init(void)
{
    ESP_LOGI(TAG, "Initializing esp-video subsystem...");
    esp_err_t ret = esp_video_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_video_init() failed: %d", ret);
        return ret;
    }
    return ESP_OK;
}

// ====================================================
//  Création des périphériques /dev/video10 et /dev/video11
// ====================================================

esp_err_t mipi_dsi_cam_create_jpeg_device(void *user_ctx)
{
    ESP_LOGI(TAG, "Registering JPEG encoder device (%s)", ESP_VIDEO_JPEG_DEVICE_NAME);

    esp_video_device_config_t cfg = {
        .device_name  = ESP_VIDEO_JPEG_DEVICE_NAME,
        .driver_name  = "jpeg_encoder",
        .frame_width  = 1280,
        .frame_height = 720,
        .pixel_format = ESP_VIDEO_PIX_FMT_RGB565,
        .priv_data    = user_ctx,
    };

    return esp_video_register_device(&cfg);
}

esp_err_t mipi_dsi_cam_create_h264_device(bool high_profile)
{
    ESP_LOGI(TAG, "Registering H.264 encoder device (%s)", ESP_VIDEO_H264_DEVICE_NAME);

    esp_video_device_config_t cfg = {
        .device_name  = ESP_VIDEO_H264_DEVICE_NAME,
        .driver_name  = high_profile ? "h264_encoder_hp" : "h264_encoder",
        .frame_width  = 1280,
        .frame_height = 720,
        .pixel_format = ESP_VIDEO_PIX_FMT_RGB565,
    };

    return esp_video_register_device(&cfg);
}
