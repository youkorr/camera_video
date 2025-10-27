#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#ifdef USE_ESP32_VARIANT_ESP32P4
#include <fcntl.h>
#include "ioctl.h"
#include "mman.h"
//#include "../lvgl_camera_display/mman.h"
#include "../mipi_dsi_cam/videodev2.h"
#include "../mipi_dsi_cam/esp_video_device.h"
#include "../mipi_dsi_cam/mipi_dsi_cam.h"
#include "driver/ppa.h"
#endif

namespace esphome {
namespace lvgl_camera_display {

class LVGLCameraDisplay : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_camera(camera::Camera *cam) { this->camera_ = cam; }
  void configure_canvas(lv_obj_t *canvas) { this->canvas_obj_ = canvas; }
  void set_update_interval(uint32_t ms) { this->update_interval_ = ms; }
  void set_rotation(uint8_t r) { this->rotation_ = r; }
  void set_mirror_x(bool m) { this->mirror_x_ = m; }
  void set_mirror_y(bool m) { this->mirror_y_ = m; }
  void set_direct_mode(bool d) { this->direct_mode_ = d; }
  void set_use_ppa(bool u) { this->use_ppa_ = u; }

 protected:
  // --- Configuration ---
  camera::Camera *camera_{nullptr};
  lv_obj_t *canvas_obj_{nullptr};
  uint32_t update_interval_{33};
  uint8_t rotation_{0};
  bool mirror_x_{false};
  bool mirror_y_{false};
  bool direct_mode_{false};
  bool use_ppa_{false};

  // --- V4L2 ---
  const char *video_device_ = "/dev/video0";
  int video_fd_{-1};
  static constexpr uint8_t VIDEO_BUFFER_COUNT = 2;
  uint8_t *mmap_buffers_[VIDEO_BUFFER_COUNT] = {nullptr};
  size_t buffer_length_{0};
  bool v4l2_streaming_{false};
  int current_buffer_index_{-1};
  uint16_t width_{0}, height_{0};

  // --- PPA ---
  ppa_client_handle_t ppa_handle_{nullptr};
  uint8_t *transform_buffer_{nullptr};
  size_t transform_buffer_size_{0};

  // --- Statistiques ---
  uint32_t last_update_time_{0};
  uint32_t last_fps_time_{0};
  uint32_t frame_count_{0};
  uint32_t drop_count_{0};
  bool first_update_{true};

  // --- Méthodes internes ---
  bool open_v4l2_device_();
  bool setup_v4l2_format_();
  bool setup_v4l2_buffers_();
  bool start_v4l2_streaming_();
  bool capture_v4l2_frame_(uint8_t **frame_data);
  void release_v4l2_frame_();
  void cleanup_v4l2_();

  bool init_ppa_();
  void deinit_ppa_();
  bool transform_frame_(const uint8_t *src, uint8_t *dst);
  void direct_display_(uint8_t *buffer, uint16_t w, uint16_t h);
};

}  // namespace lvgl_camera_display
}  // namespace esphome







