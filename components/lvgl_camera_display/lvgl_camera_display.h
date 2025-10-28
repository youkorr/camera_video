#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#ifdef USE_ESP32_VARIANT_ESP32P4
#include <fcntl.h>
#include "ioctl.h"
#include "mman.h"
#include "../mipi_dsi_cam/videodev2.h"
#include "../mipi_dsi_cam/esp_video_device.h"
#include "../mipi_dsi_cam/mipi_dsi_cam.h"
#include "driver/ppa.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#endif

namespace esphome {
namespace lvgl_camera_display {

#define VIDEO_BUFFER_COUNT 2

enum RotationAngle {
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
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_camera(mipi_dsi_cam::MipiDsiCam *camera) { this->camera_ = camera; }
  void configure_canvas(lv_obj_t *canvas);
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ = interval_ms; }
  
  void set_rotation(RotationAngle rotation) { this->rotation_ = rotation; }
  void set_mirror_x(bool enable) { this->mirror_x_ = enable; }
  void set_mirror_y(bool enable) { this->mirror_y_ = enable; }
  
  void set_direct_mode(bool enable) { this->direct_mode_ = enable; }
  void set_use_ppa(bool enable) { this->use_ppa_ = enable; }

  void stop_capture();

 protected:
  mipi_dsi_cam::MipiDsiCam *camera_{nullptr};
  
#ifdef USE_ESP32_VARIANT_ESP32P4
  int video_fd_{-1};
  const char *video_device_{ESP_VIDEO_MIPI_CSI_DEVICE_NAME};
  
  uint8_t *mmap_buffers_[VIDEO_BUFFER_COUNT]{nullptr};
  size_t buffer_length_{0};
  int current_buffer_index_{-1};
  uint8_t *work_buffer_{nullptr};
  
  ppa_client_handle_t ppa_handle_{nullptr};
  uint8_t *transform_buffer_{nullptr};
  size_t transform_buffer_size_{0};
  
  lv_disp_t *lvgl_display_{nullptr};
  lv_disp_draw_buf_t *lvgl_draw_buf_{nullptr};
  uint8_t *lvgl_framebuffer_{nullptr};
  size_t lvgl_framebuffer_size_{0};
  
  uint8_t *aligned_buffer_{nullptr};
  size_t aligned_buffer_size_{0};
  
  TaskHandle_t capture_task_handle_{nullptr};
  volatile bool task_running_{false};
  SemaphoreHandle_t lvgl_mutex_{nullptr};
  
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
  
  bool init_direct_mode_();
  
  static void capture_task_(void *param);
  void capture_loop_();
#endif

  lv_obj_t *canvas_obj_{nullptr};
  
  uint16_t width_{1280};
  uint16_t height_{720};
  RotationAngle rotation_{ROTATION_0};
  bool mirror_x_{false};
  bool mirror_y_{false};
  bool direct_mode_{false};
  bool use_ppa_{true};
  uint32_t update_interval_{33};
  
  uint32_t frame_count_{0};
  uint32_t drop_count_{0};
  bool v4l2_streaming_{false};
};

}  // namespace lvgl_camera_display
}  // namespace esphome
























