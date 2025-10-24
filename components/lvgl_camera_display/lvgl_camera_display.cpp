#include "lvgl_camera_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

// ======================= CONFIG ==========================
#define CAMERA_WIDTH  1280
#define CAMERA_HEIGHT 720
#define CAMERA_FPS_MS 33
#define CAMERA_PATH   "/dev/video0"
#define VIDEO_BUFFER_COUNT 3
#define MEMORY_TYPE V4L2_MEMORY_MMAP

// =========================================================

bool LVGLCameraDisplay::open_video_device_() {
  ESP_LOGI(TAG, "🚀 esp_video_init(): creating virtual V4L2 device (%s)", CAMERA_PATH);

  // ==== Étape 1 : Configuration CSI ====
  static esp_video_init_csi_config_t csi_config = {
      .sccb_config =
          {
              .init_sccb  = false,
              .i2c_handle = nullptr,
              .freq       = 400000,
          },
      .reset_pin = -1,
      .pwdn_pin  = -1,
  };

  csi_config.sccb_config.i2c_handle = bsp_i2c_get_handle();

  esp_video_init_config_t cam_config = {
      .csi  = &csi_config,
      .dvp  = nullptr,
      .jpeg = nullptr,
      .isp  = nullptr,
  };

  esp_err_t ret = esp_video_init(&cam_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ esp_video_init failed: %s", esp_err_to_name(ret));
    return false;
  }

  // ==== Étape 2 : Vérification du device /dev/video0 ====
  int retries = 0;
  while (access(CAMERA_PATH, F_OK) != 0 && retries++ < 10) {
    ESP_LOGW(TAG, "⏳ Waiting for %s to be created...", CAMERA_PATH);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (access(CAMERA_PATH, F_OK) != 0) {
    ESP_LOGE(TAG, "❌ Device %s not found after esp_video_init", CAMERA_PATH);
    return false;
  }

  // ==== Étape 3 : Ouverture du périphérique ====
  this->video_fd_ = open(CAMERA_PATH, O_RDONLY);
  if (this->video_fd_ < 0) {
    ESP_LOGE(TAG, "❌ Failed to open %s: errno=%d", CAMERA_PATH, errno);
    return false;
  }

  ESP_LOGI(TAG, "✅ Opened %s successfully (fd=%d)", CAMERA_PATH, this->video_fd_);

  // ==== Étape 4 : Vérifier les capacités ====
  struct v4l2_capability cap;
  if (ioctl(this->video_fd_, VIDIOC_QUERYCAP, &cap) != 0) {
    ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed");
    close(this->video_fd_);
    return false;
  }

  ESP_LOGI(TAG, "V4L2 driver: %s | card: %s", cap.driver, cap.card);

  return true;
}

bool LVGLCameraDisplay::setup_buffers_() {
  struct v4l2_requestbuffers req = {0};
  req.count = VIDEO_BUFFER_COUNT;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = MEMORY_TYPE;

  if (ioctl(this->video_fd_, VIDIOC_REQBUFS, &req) != 0) {
    ESP_LOGE(TAG, "VIDIOC_REQBUFS failed");
    return false;
  }

  for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = MEMORY_TYPE;
    buf.index = i;

    if (ioctl(this->video_fd_, VIDIOC_QUERYBUF, &buf) != 0) {
      ESP_LOGE(TAG, "VIDIOC_QUERYBUF failed for buffer %d", i);
      return false;
    }

    this->buffers_[i].length = buf.length;
    this->buffers_[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                   this->video_fd_, buf.m.offset);

    if (this->buffers_[i].start == MAP_FAILED) {
      ESP_LOGE(TAG, "mmap failed for buffer %d", i);
      return false;
    }

    if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) != 0) {
      ESP_LOGE(TAG, "VIDIOC_QBUF failed for buffer %d", i);
      return false;
    }
  }

  ESP_LOGI(TAG, "✅ Buffers initialized successfully");
  return true;
}

bool LVGLCameraDisplay::start_streaming_() {
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->video_fd_, VIDIOC_STREAMON, &type) != 0) {
    ESP_LOGE(TAG, "VIDIOC_STREAMON failed");
    return false;
  }

  ESP_LOGI(TAG, "🎥 Streaming started");
  return true;
}

bool LVGLCameraDisplay::capture_frame_() {
  struct v4l2_buffer buf = {0};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = MEMORY_TYPE;

  if (ioctl(this->video_fd_, VIDIOC_DQBUF, &buf) != 0) {
    ESP_LOGE(TAG, "VIDIOC_DQBUF failed");
    return false;
  }

  uint8_t *frame_data = (uint8_t *) this->buffers_[buf.index].start;

  if (this->canvas_) {
    lv_canvas_set_buffer(this->canvas_, frame_data, CAMERA_WIDTH, CAMERA_HEIGHT, LV_COLOR_FORMAT_RGB565);
  }

  if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) != 0) {
    ESP_LOGE(TAG, "VIDIOC_QBUF failed");
  }

  return true;
}

void LVGLCameraDisplay::cleanup_() {
  if (this->video_fd_ >= 0) {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(this->video_fd_, VIDIOC_STREAMOFF, &type);
    close(this->video_fd_);
    this->video_fd_ = -1;
  }

  for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
    if (this->buffers_[i].start) {
      munmap(this->buffers_[i].start, this->buffers_[i].length);
      this->buffers_[i].start = nullptr;
    }
  }

  ESP_LOGI(TAG, "🧹 Video cleaned up");
}

// =========================================================

void LVGLCameraDisplay::setup() {
  ESP_LOGI(TAG, "🎥 LVGL Camera Display (V4L2 Pipeline)");

  if (!this->open_video_device_()) {
    ESP_LOGE(TAG, "❌ Failed to open video device");
    this->mark_failed();
    return;
  }

  if (!this->setup_buffers_()) {
    ESP_LOGE(TAG, "❌ Failed to setup buffers");
    this->mark_failed();
    return;
  }

  if (!this->start_streaming_()) {
    ESP_LOGE(TAG, "❌ Failed to start streaming");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "✅ LVGL camera initialized successfully");
}

void LVGLCameraDisplay::loop() {
  this->capture_frame_();
}

void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "LVGL Camera Display:");
  ESP_LOGCONFIG(TAG, "  Mode: V4L2 Pipeline (ESP32-P4)");
  ESP_LOGCONFIG(TAG, "  Device: %s", CAMERA_PATH);
  ESP_LOGCONFIG(TAG, "  Resolution: %ux%u", CAMERA_WIDTH, CAMERA_HEIGHT);
  ESP_LOGCONFIG(TAG, "  Buffers: %d (mmap zero-copy)", VIDEO_BUFFER_COUNT);
  ESP_LOGCONFIG(TAG, "  PPA: DISABLED");
  ESP_LOGCONFIG(TAG, "  Canvas: YES");
  ESP_LOGCONFIG(TAG, "  Rotation: 0°");
  ESP_LOGCONFIG(TAG, "  Mirror X: YES");
  ESP_LOGCONFIG(TAG, "  Mirror Y: NO");
  ESP_LOGCONFIG(TAG, "  Update interval: %d ms", CAMERA_FPS_MS);
}

void LVGLCameraDisplay::configure_canvas(lv_obj_t *canvas) {
  this->canvas_ = canvas;
}

}  // namespace lvgl_camera_display
}  // namespace esphome

#endif






