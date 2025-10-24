#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

// === Inclure les implémentations locales pour ioctl/mmap ===
#include "ioctl.h"
#include "mman.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "../mipi_dsi_cam/videodev2.h"
#include "../mipi_dsi_cam/esp_video_device.h"
#include "../mipi_dsi_cam/mipi_dsi_cam.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#endif

namespace esphome {
namespace lvgl_camera_display {

#define VIDEO_BUFFER_COUNT 2

enum Rotation {
  ROTATION_0   = 0,
  ROTATION_90  = 90,
  ROTATION_180 = 180,
  ROTATION_270 = 270
};

class LVGLCameraDisplay : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_camera(mipi_dsi_cam::MipiDsiCam *camera) { this->camera_ = camera; }
  void set_video_device(const char *dev) { this->video_device_ = dev; }
  void set_width(uint16_t width) { this->width_ = width; }
  void set_height(uint16_t height) { this->height_ = height; }
  void configure_canvas(lv_obj_t *canvas);
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ = interval_ms; }

  void set_rotation(Rotation rotation) { this->rotation_ = rotation; }
  void set_mirror_x(bool enable) { this->mirror_x_ = enable; }
  void set_mirror_y(bool enable) { this->mirror_y_ = enable; }

  uint32_t get_frame_count() const { return this->frame_count_; }

 protected:
  mipi_dsi_cam::MipiDsiCam *camera_{nullptr};

#ifdef USE_ESP32_VARIANT_ESP32P4
  int video_fd_{-1};
  const char *video_device_{ESP_VIDEO_MIPI_CSI_DEVICE_NAME};

  uint8_t *mmap_buffers_[VIDEO_BUFFER_COUNT]{nullptr};
  size_t buffer_length_{0};

  ppa_client_handle_t ppa_handle_{nullptr};
  uint8_t *transform_buffer_{nullptr};
  size_t transform_buffer_size_{0};

  bool open_video_device_();
  bool setup_buffers_();
  bool start_streaming_();
  bool capture_frame_();
  void cleanup_();

  bool init_ppa_();
  void deinit_ppa_();
  bool transform_frame_(const uint8_t *src, uint8_t *dst);
#endif

  lv_obj_t *canvas_obj_{nullptr};

  uint16_t width_{1280};
  uint16_t height_{720};
  Rotation rotation_{ROTATION_0};
  bool mirror_x_{false};
  bool mirror_y_{false};
  uint32_t update_interval_{33};

  uint32_t frame_count_{0};
  uint32_t last_update_time_{0};
  uint32_t last_fps_time_{0};

  bool first_update_{true};
  bool streaming_{false};
  uint8_t *last_display_buffer_{nullptr};
};

}  // namespace lvgl_camera_display
}  // namespace esphome


