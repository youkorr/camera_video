#include "mipi_dsi_cam_video_devices.h"
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

static const char *TAG = "video_devices";

// ===== Contextes des encodeurs =====
struct JPEGDeviceContext {
  void *user_ctx;
  bool streaming;
  uint32_t width;
  uint32_t height;
  uint8_t quality;
};

struct H264DeviceContext {
  bool high_profile;
  bool streaming;
  uint32_t width;
  uint32_t height;
  uint32_t bitrate;
  uint32_t gop_size;
};

static JPEGDeviceContext s_jpeg_ctx = {0};
static H264DeviceContext s_h264_ctx = {0};

// ===== Callbacks JPEG =====
static esp_err_t jpeg_init(void *video) {
  ESP_LOGI(TAG, "JPEG encoder init");
  return ESP_OK;
}

static esp_err_t jpeg_start(void *video, uint32_t type) {
  JPEGDeviceContext *ctx = (JPEGDeviceContext*)video;
  ctx->streaming = true;
  ESP_LOGI(TAG, "JPEG streaming started");
  return ESP_OK;
}

static esp_err_t jpeg_stop(void *video, uint32_t type) {
  JPEGDeviceContext *ctx = (JPEGDeviceContext*)video;
  ctx->streaming = false;
  ESP_LOGI(TAG, "JPEG streaming stopped");
  return ESP_OK;
}

static esp_err_t jpeg_set_format(void *video, const void *format) {
  JPEGDeviceContext *ctx = (JPEGDeviceContext*)video;
  const struct v4l2_format *fmt = (const struct v4l2_format*)format;
  
  ctx->width = fmt->fmt.pix.width;
  ctx->height = fmt->fmt.pix.height;
  
  ESP_LOGI(TAG, "JPEG format: %ux%u", ctx->width, ctx->height);
  return ESP_OK;
}

// Table des opérations JPEG
static const esp_video_ops jpeg_ops = {
  .init = jpeg_init,
  .deinit = nullptr,
  .start = jpeg_start,
  .stop = jpeg_stop,
  .enum_format = nullptr,
  .set_format = jpeg_set_format,
  .get_format = nullptr,
  .reqbufs = nullptr,
  .querybuf = nullptr,
  .qbuf = nullptr,
  .dqbuf = nullptr,
  .querycap = nullptr,
};

// ===== Callbacks H.264 (similaire) =====
static esp_err_t h264_init(void *video) {
  ESP_LOGI(TAG, "H.264 encoder init");
  return ESP_OK;
}

static esp_err_t h264_start(void *video, uint32_t type) {
  H264DeviceContext *ctx = (H264DeviceContext*)video;
  ctx->streaming = true;
  ESP_LOGI(TAG, "H.264 streaming started");
  return ESP_OK;
}

static esp_err_t h264_stop(void *video, uint32_t type) {
  H264DeviceContext *ctx = (H264DeviceContext*)video;
  ctx->streaming = false;
  ESP_LOGI(TAG, "H.264 streaming stopped");
  return ESP_OK;
}

static const esp_video_ops h264_ops = {
  .init = h264_init,
  .deinit = nullptr,
  .start = h264_start,
  .stop = h264_stop,
  .enum_format = nullptr,
  .set_format = nullptr,
  .get_format = nullptr,
  .reqbufs = nullptr,
  .querybuf = nullptr,
  .qbuf = nullptr,
  .dqbuf = nullptr,
  .querycap = nullptr,
};

// ===== API Publique =====
esp_err_t mipi_dsi_cam_video_init(void) {
  esp_video_init_config_t config = {
    .csi = nullptr,
    .dvp = nullptr,
    .jpeg = nullptr,
    .isp = nullptr,
  };
  
  return esp_video_init(&config);
}

esp_err_t mipi_dsi_cam_create_jpeg_device(void *user_ctx) {
  s_jpeg_ctx.user_ctx = user_ctx;
  s_jpeg_ctx.quality = 80;
  
  esp_err_t ret = esp_video_register_device(
    ESP_VIDEO_JPEG_DEVICE_ID,
    &s_jpeg_ctx,
    user_ctx,
    &jpeg_ops
  );
  
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "✅ Created /dev/video%d (JPEG)", ESP_VIDEO_JPEG_DEVICE_ID);
  }
  
  return ret;
}

esp_err_t mipi_dsi_cam_create_h264_device(bool high_profile) {
  s_h264_ctx.high_profile = high_profile;
  s_h264_ctx.bitrate = 2000000;
  s_h264_ctx.gop_size = 30;
  
  esp_err_t ret = esp_video_register_device(
    ESP_VIDEO_H264_DEVICE_ID,
    &s_h264_ctx,
    nullptr,
    &h264_ops
  );
  
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "✅ Created /dev/video%d (H.264)", ESP_VIDEO_H264_DEVICE_ID);
  }
  
  return ret;
}

#endif // USE_ESP32_VARIANT_ESP32P4
