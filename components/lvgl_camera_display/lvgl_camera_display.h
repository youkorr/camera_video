#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <fcntl.h>
#include "ioctl.h"   // version locale corrigée
#include "mman.h"    // version locale corrigée
#include "../mipi_dsi_cam/videodev2.h"
#include "../mipi_dsi_cam/esp_video_device.h"
#include "../mipi_dsi_cam/mipi_dsi_cam.h"
#include "driver/ppa.h"
#include "esp_cache.h"
//#include <sys/types.h>

#endif  // USE_ESP32_VARIANT_ESP32P4

namespace esphome {
namespace lvgl_camera_display {

#define VIDEO_BUFFER_COUNT 3

class LVGLCameraDisplay : public esphome::Component {
 public:
  // --- Types internes ---
  enum Rotation {
    ROTATION_0 = 0,
    ROTATION_90 = 90,
    ROTATION_180 = 180,
    ROTATION_270 = 270
  };

  // --- Méthodes de base ---
  void setup() override;
  void loop() override;
  void dump_config() override;
  void configure_canvas(lv_obj_t *canvas);

  // --- Liens caméra / LVGL ---
  void set_camera(esphome::mipi_dsi_cam::MipiDSICam *cam) { this->camera_ = cam; }
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ = interval_ms; }
  void set_rotation(Rotation rot) { this->rotation_ = rot; }
  void set_mirror_x(bool enabled) { this->mirror_x_ = enabled; }
  void set_mirror_y(bool enabled) { this->mirror_y_ = enabled; }

 protected:
#ifdef USE_ESP32_VARIANT_ESP32P4
  // --- Variables principales ---
  esphome::mipi_dsi_cam::MipiDSICam *camera_{nullptr};
  lv_obj_t *canvas_obj_{nullptr};

  int video_fd_{-1};
  const char *video_device_ = ESP_VIDEO_MIPI_CSI_DEVICE_NAME;

  uint32_t width_{0};
  uint32_t height_{0};

  uint8_t *mmap_buffers_[VIDEO_BUFFER_COUNT] = {nullptr};
  size_t buffer_length_{0};

  bool streaming_{false};

  // --- PPA (rotation/miroir matériel) ---
  ppa_client_handle_t ppa_handle_{nullptr};
  uint8_t *transform_buffer_{nullptr};
  size_t transform_buffer_size_{0};

  // --- Configuration logique ---
  Rotation rotation_{ROTATION_0};
  bool mirror_x_{false};
  bool mirror_y_{false};
  uint32_t update_interval_{33};

  // --- Stats ---
  uint32_t last_update_time_{0};
  uint32_t frame_count_{0};
  uint32_t last_fps_time_{0};

  // --- Méthodes internes ---
  bool open_video_device_();
  bool negotiate_format_();
  bool setup_buffers_();
  bool start_streaming_();
  bool capture_frame_();
  void cleanup_();

  bool init_ppa_();
  void deinit_ppa_();
  bool transform_frame_(const uint8_t *src, uint8_t *dst);

#endif  // USE_ESP32_VARIANT_ESP32P4
};

}  // namespace lvgl_camera_display
}  // namespace esphome








