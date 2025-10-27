#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#ifdef USE_ESP32_VARIANT_ESP32P4
#include <fcntl.h>
#include "ioctl.h"
#include "mman.h"
#include <errno.h>
#include "driver/ppa.h"
#include "../mipi_dsi_cam/videodev2.h"
#include "../mipi_dsi_cam/esp_video_device.h"
#include "../mipi_dsi_cam/mipi_dsi_cam.h"
#endif

namespace esphome {
namespace lvgl_camera_display {

using namespace esphome::mipi_dsi_cam;

enum RotationAngle {
  ROTATION_0 = 0,
  ROTATION_90,
  ROTATION_180,
  ROTATION_270
};

class LVGLCameraDisplay : public Component {
 public:
  // ==== Méthodes principales ====
  void setup() override;
  void loop() override;
  void dump_config() override;

  // ==== Setters YAML ====
  void set_camera(MipiDsiCam *cam) { this->camera_ = cam; }
  void set_update_interval(uint32_t interval) { this->update_interval_ = interval; }
  void set_rotation(RotationAngle rot) { this->rotation_ = rot; }
  void set_mirror_x(bool mirror) { this->mirror_x_ = mirror; }
  void set_mirror_y(bool mirror) { this->mirror_y_ = mirror; }
  void set_direct_mode(bool direct) { this->direct_mode_ = direct; }
  void set_use_ppa(bool enable) { this->use_ppa_ = enable; }

  // ==== LVGL ====
  void configure_canvas(lv_obj_t *canvas) { this->canvas_obj_ = canvas; }

 protected:
  // ==== Référence caméra ====
  MipiDsiCam *camera_{nullptr};

  // ==== Objet LVGL (facultatif) ====
  lv_obj_t *canvas_obj_{nullptr};

  // ==== Configuration ====
  uint32_t update_interval_{33};
  RotationAngle rotation_{ROTATION_0};
  bool mirror_x_{false};
  bool mirror_y_{false};
  bool direct_mode_{false};
  bool use_ppa_{true};

  // ==== État ====
  uint32_t width_{0};
  uint32_t height_{0};
  uint32_t last_update_time_{0};
  uint32_t frame_count_{0};
  uint32_t drop_count_{0};
  uint32_t last_fps_time_{0};
  bool first_update_{true};

#ifdef USE_ESP32_VARIANT_ESP32P4
  // ==== Interface V4L2 ====
  const char *video_device_ = ESP_VIDEO_MIPI_CSI_DEVICE_NAME;  // ✅ correction principale
  int video_fd_{-1};
  static constexpr int VIDEO_BUFFER_COUNT = 2;
  uint8_t *mmap_buffers_[VIDEO_BUFFER_COUNT]{};
  size_t buffer_length_{0};
  bool v4l2_streaming_{false};
  int current_buffer_index_{-1};

  // ==== Accélération matérielle PPA ====
  ppa_client_handle_t ppa_handle_{nullptr};
  uint8_t *transform_buffer_{nullptr};
  size_t transform_buffer_size_{0};

  // ==== Fonctions internes (implémentées dans .cpp) ====
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

  // ==== Nouveau : rendu direct ====
  void direct_display_(uint8_t *buffer, uint16_t w, uint16_t h);  // ✅ ajouté
#endif
};

}  // namespace lvgl_camera_display
}  // namespace esphome










