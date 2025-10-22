#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#ifdef USE_ESP32_VARIANT_ESP32P4
#include <fcntl.h>
#include <sys/ioctl.h>
#include "mman.h"
#include "../mipi_dsi_cam/videodev2.h"
#include "../mipi_dsi_cam/esp_video_device.h"
#include "../mipi_dsi_cam/mipi_dsi_cam.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#endif

namespace esphome {
namespace lvgl_camera_display {

// Configuration
#define VIDEO_BUFFER_COUNT 3  // Triple buffering comme dans votre exemple
#define MEMORY_TYPE V4L2_MEMORY_MMAP

enum Rotation {
  ROTATION_0 = 0,
  ROTATION_90 = 90,
  ROTATION_180 = 180,
  ROTATION_270 = 270
};

class LVGLCameraDisplay : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // Configuration
  void set_video_device(const char *dev) { this->video_device_ = dev; }
  void set_width(uint16_t width) { this->width_ = width; }
  void set_height(uint16_t height) { this->height_ = height; }
  void configure_canvas(lv_obj_t *canvas);
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ = interval_ms; }
  
  // Transformations PPA
  void set_rotation(Rotation rotation) { this->rotation_ = rotation; }
  void set_mirror_x(bool enable) { this->mirror_x_ = enable; }
  void set_mirror_y(bool enable) { this->mirror_y_ = enable; }
  
  // Stats
  uint32_t get_frame_count() const { return this->frame_count_; }

 protected:
#ifdef USE_ESP32_VARIANT_ESP32P4
  // V4L2 device
  int video_fd_{-1};
  const char *video_device_{ESP_VIDEO_MIPI_CSI_DEVICE_NAME};  // "/dev/video0"
  
  // Buffers mmappés (zero-copy)
  uint8_t *mmap_buffers_[VIDEO_BUFFER_COUNT]{nullptr};
  size_t buffer_length_{0};
  
  // PPA pour transformations
  ppa_client_handle_t ppa_handle_{nullptr};
  uint8_t *transform_buffer_{nullptr};
  size_t transform_buffer_size_{0};
  
  // Méthodes V4L2
  bool open_video_device_();
  bool setup_buffers_();
  bool start_streaming_();
  bool capture_frame_();
  void cleanup_();
  
  // PPA
  bool init_ppa_();
  void deinit_ppa_();
  bool transform_frame_(const uint8_t *src, uint8_t *dst);
#endif

  // Canvas LVGL
  lv_obj_t *canvas_obj_{nullptr};
  
  // Configuration
  uint16_t width_{1280};
  uint16_t height_{720};
  Rotation rotation_{ROTATION_0};
  bool mirror_x_{false};
  bool mirror_y_{false};
  uint32_t update_interval_{16};  // ~60 FPS
  
  // Stats
  uint32_t frame_count_{0};
  uint32_t last_update_time_{0};
  uint32_t last_fps_time_{0};
  
  // Flags
  bool first_update_{true};
  bool streaming_{false};
  uint8_t *last_display_buffer_{nullptr};
};

}  // namespace lvgl_camera_display
}  // namespace esphome
