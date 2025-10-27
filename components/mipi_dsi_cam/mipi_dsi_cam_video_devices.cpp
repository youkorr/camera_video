#include "mipi_dsi_cam_video_devices.h"
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "videodev2.h"  // ✅ AJOUT : pour v4l2_format
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

static esp_err_t jpeg_deinit(void *video) {
  ESP_LOGI(TAG, "JPEG encoder deinit");
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

static esp_err_t jpeg_get_format(void *video, void *format) {
  JPEGDeviceContext *ctx = (JPEGDeviceContext*)video;
  struct v4l2_format *fmt = (struct v4l2_format*)format;
  
  if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  
  fmt->fmt.pix.width = ctx->width;
  fmt->fmt.pix.height = ctx->height;
  fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  fmt->fmt.pix.field = V4L2_FIELD_NONE;
  fmt->fmt.pix.bytesperline = 0;
  fmt->fmt.pix.sizeimage = ctx->width * ctx->height;
  
  return ESP_OK;
}

static esp_err_t jpeg_querycap(void *video, void *cap) {
  struct v4l2_capability *capability = (struct v4l2_capability*)cap;
  
  memset(capability, 0, sizeof(*capability));
  strncpy((char*)capability->driver, "esp_jpeg", sizeof(capability->driver) - 1);
  strncpy((char*)capability->card, "ESP JPEG Encoder", sizeof(capability->card) - 1);
  strncpy((char*)capability->bus_info, "platform:esp-jpeg", sizeof(capability->bus_info) - 1);
  
  capability->version = 0x00010000;
  capability->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
  capability->device_caps = capability->capabilities;
  
  return ESP_OK;
}

// ✅ Déclaration AVANT utilisation
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

// Table des opérations JPEG
static const esp_video_ops jpeg_ops = {
  .init = jpeg_init,
  .deinit = jpeg_deinit,
  .start = jpeg_start,
  .stop = jpeg_stop,
  .enum_format = nullptr,
  .set_format = jpeg_set_format,
  .get_format = jpeg_get_format,
  .reqbufs = nullptr,
  .querybuf = nullptr,
  .qbuf = nullptr,
  .dqbuf = nullptr,
  .querycap = jpeg_querycap,
};

// ===== Callbacks H.264 =====
static esp_err_t h264_init(void *video) {
  ESP_LOGI(TAG, "H.264 encoder init");
  return ESP_OK;
}

static esp_err_t h264_deinit(void *video) {
  ESP_LOGI(TAG, "H.264 encoder deinit");
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

static esp_err_t h264_set_format(void *video, const void *format) {
  H264DeviceContext *ctx = (H264DeviceContext*)video;
  const struct v4l2_format *fmt = (const struct v4l2_format*)format;
  
  ctx->width = fmt->fmt.pix.width;
  ctx->height = fmt->fmt.pix.height;
  
  ESP_LOGI(TAG, "H.264 format: %ux%u", ctx->width, ctx->height);
  return ESP_OK;
}

static esp_err_t h264_get_format(void *video, void *format) {
  H264DeviceContext *ctx = (H264DeviceContext*)video;
  struct v4l2_format *fmt = (struct v4l2_format*)format;
  
  if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  
  fmt->fmt.pix.width = ctx->width;
  fmt->fmt.pix.height = ctx->height;
  fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
  fmt->fmt.pix.field = V4L2_FIELD_NONE;
  fmt->fmt.pix.bytesperline = 0;
  fmt->fmt.pix.sizeimage = ctx->width * ctx->height;
  
  return ESP_OK;
}

static esp_err_t h264_querycap(void *video, void *cap) {
  struct v4l2_capability *capability = (struct v4l2_capability*)cap;
  
  memset(capability, 0, sizeof(*capability));
  strncpy((char*)capability->driver, "esp_h264", sizeof(capability->driver) - 1);
  strncpy((char*)capability->card, "ESP H.264 Encoder", sizeof(capability->card) - 1);
  strncpy((char*)capability->bus_info, "platform:esp-h264", sizeof(capability->bus_info) - 1);
  
  capability->version = 0x00010000;
  capability->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
  capability->device_caps = capability->capabilities;
  
  return ESP_OK;
}

// Table des opérations H.264
static const esp_video_ops h264_ops = {
  .init = h264_init,
  .deinit = h264_deinit,
  .start = h264_start,
  .stop = h264_stop,
  .enum_format = nullptr,
  .set_format = h264_set_format,
  .get_format = h264_get_format,
  .reqbufs = nullptr,
  .querybuf = nullptr,
  .qbuf = nullptr,
  .dqbuf = nullptr,
  .querycap = h264_querycap,
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
  s_jpeg_ctx.width = 1280;
  s_jpeg_ctx.height = 720;
  s_jpeg_ctx.streaming = false;
  
  esp_err_t ret = esp_video_register_device(
    ESP_VIDEO_JPEG_DEVICE_ID,
    &s_jpeg_ctx,
    user_ctx,
    &jpeg_ops
  );
  
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "✅ Created /dev/video%d (JPEG)", ESP_VIDEO_JPEG_DEVICE_ID);
  } else {
    ESP_LOGE(TAG, "❌ Failed to create JPEG device: 0x%x", ret);
  }
  
  return ret;
}

esp_err_t mipi_dsi_cam_create_h264_device(bool high_profile) {
  s_h264_ctx.high_profile = high_profile;
  s_h264_ctx.bitrate = 2000000;
  s_h264_ctx.gop_size = 30;
  s_h264_ctx.width = 1280;
  s_h264_ctx.height = 720;
  s_h264_ctx.streaming = false;
  
  esp_err_t ret = esp_video_register_device(
    ESP_VIDEO_H264_DEVICE_ID,
    &s_h264_ctx,
    nullptr,
    &h264_ops
  );
  
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "✅ Created /dev/video%d (H.264)", ESP_VIDEO_H264_DEVICE_ID);
  } else {
    ESP_LOGE(TAG, "❌ Failed to create H.264 device: 0x%x", ret);
  }
  
  return ret;
}

#endif // USE_ESP32_VARIANT_ESP32P4
