#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#ifdef USE_ESP32_VARIANT_ESP32P4
#include <fcntl.h>
#include <unistd.h>
#include "ioctl.h"       // local header (défini dans ce dossier)
#include "mman.h"        // local header (défini dans ce dossier)
#include "../mipi_dsi_cam/videodev2.h"
#include "../mipi_dsi_cam/esp_video_device.h"
#include "../mipi_dsi_cam/mipi_dsi_cam.h"
#include "../mipi_dsi_cam/esp_video_init.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "bsp/esp-bsp.h" // pour bsp_i2c_get_handle()
#endif

namespace esphome {
namespace lvgl_camera_display {

// ================= Configuration ====================
#define VIDEO_BUFFER_COUNT 3  // triple buffering
#define MEMORY_TYPE V4L2_MEMORY_MMAP

// Structure tampon mémoire
struct VideoBuffer {
  void *start = nullptr;
  size_t length = 0;
};

// ====================================================
class LVGLCameraDisplay : public Component {
 public:
  // --- Méthodes ESPHome ---
  void setup() override;
  void loop() override;
  void dump_config() override;

  // --- Canvas LVGL ---
  void configure_canvas(lv_obj_t *canvas);

 protected:
  // --- Méthodes internes ---
  bool open_video_device_();
  bool setup_buffers_();
  bool start_streaming_();
  bool capture_frame_();
  void cleanup_();

  // --- Membres internes ---
  int video_fd_ = -1;
  VideoBuffer buffers_[VIDEO_BUFFER_COUNT];
  lv_obj_t *canvas_ = nullptr;
};

}  // namespace lvgl_camera_display
}  // namespace esphome






