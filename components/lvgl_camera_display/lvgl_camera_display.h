#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#ifdef USE_ESP32_VARIANT_ESP32P4
#include <fcntl.h>
#include <unistd.h>

// IMPORTANT : ces 2 fichiers sont LOCAUX (dans le même dossier lvgl_camera_display)
#include "ioctl.h"
#include "mman.h"

// V4L2 et pile vidéo (versions locales de ton dépôt)
#include "../mipi_dsi_cam/videodev2.h"
#include "../mipi_dsi_cam/esp_video_device.h"
#include "../mipi_dsi_cam/mipi_dsi_cam.h"
#include "../mipi_dsi_cam/esp_video_init.h"

// Optionnel (si tu actives le PPA quelque part)
#include "driver/ppa.h"
#include "esp_cache.h"
#endif

namespace esphome {
namespace lvgl_camera_display {

// ======================================================
// Configuration bas-niveau (mêmes valeurs que le .cpp)
// ======================================================
#define VIDEO_BUFFER_COUNT 3
#define MEMORY_TYPE V4L2_MEMORY_MMAP

// Tampon mmap
struct VideoBuffer {
  void *start = nullptr;
  size_t length = 0;
};

class LVGLCameraDisplay : public Component {
 public:
  // Hooks ESPHome
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Canvas LVGL à afficher (objet lvgl passé depuis ailleurs)
  void configure_canvas(lv_obj_t *canvas);

 protected:
  // ----- Pipeline V4L2 -----
  bool open_video_device_();
  bool setup_buffers_();
  bool start_streaming_();
  bool capture_frame_();
  void cleanup_();

  // ----- État interne -----
  int video_fd_{-1};
  VideoBuffer buffers_[VIDEO_BUFFER_COUNT];
  lv_obj_t *canvas_{nullptr};
};

}  // namespace lvgl_camera_display
}  // namespace esphome







