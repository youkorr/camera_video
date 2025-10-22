#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/lvgl/lvgl.h"
#include "esp_video_buffer.h"

#ifdef USE_ESP32_VARIANT_ESP32P4
#include "hal/ppa_hal.h"
#include "driver/ppa.h"
#endif

// Forward declarations
#ifdef MIPI_DSI_CAM_ENABLE_JPEG
#include "mipi_dsi_cam_encoders.h"
#endif

namespace esphome {
namespace lvgl_camera_display {

// Nombre de buffers pour le double/triple buffering
#define VIDEO_BUFFER_COUNT 2  // Peut être augmenté à 3 pour triple buffering

// Définir les macros pour gérer l'état des buffers
#define ELEMENT_IS_FREE(e)    ((e)->flags == 0)
#define ELEMENT_IS_QUEUED(e)  ((e)->flags & (1 << 0))
#define ELEMENT_IS_ACTIVE(e)  ((e)->flags & (1 << 1))
#define ELEMENT_SET_FREE(e)   ((e)->flags = 0)
#define ELEMENT_SET_QUEUED(e) ((e)->flags = (1 << 0))
#define ELEMENT_SET_ACTIVE(e) ((e)->flags = (1 << 1))

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
  void set_camera(void *camera) { this->camera_ = (CameraComponent*)camera; }
  void configure_canvas(lv_obj_t *canvas);
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ = interval_ms; }
  
  // Transformations
  void set_rotation(Rotation rotation) { this->rotation_ = rotation; }
  void set_mirror_x(bool enable) { this->mirror_x_ = enable; }
  void set_mirror_y(bool enable) { this->mirror_y_ = enable; }
  
  // Encodage
  void set_jpeg_output(bool enable) { this->jpeg_output_ = enable; }
  void set_h264_output(bool enable) { this->h264_output_ = enable; }
  void set_jpeg_quality(uint8_t quality) { this->jpeg_quality_ = quality; }
  void set_h264_bitrate(uint32_t bitrate) { this->h264_bitrate_ = bitrate; }
  void set_h264_gop_size(uint32_t gop) { this->h264_gop_size_ = gop; }

  // Stats
  uint32_t get_frame_count() const { return this->frame_count_; }

 protected:
  // Camera abstraction (adapter votre classe de caméra)
  struct CameraComponent {
    virtual bool is_streaming() = 0;
    virtual bool capture_frame() = 0;
    virtual uint8_t* get_image_data() = 0;
    virtual uint16_t get_image_width() = 0;
    virtual uint16_t get_image_height() = 0;
  };

  CameraComponent *camera_{nullptr};
  lv_obj_t *canvas_obj_{nullptr};

  // Video buffer system
  struct esp_video_buffer *video_buffer_{nullptr};
  int current_buffer_index_{0};
  uint8_t *last_display_buffer_{nullptr};
  
  bool init_video_buffer_();
  void cleanup_video_buffer_();
  bool capture_to_video_buffer_();
  void update_canvas_();

#ifdef USE_ESP32_VARIANT_ESP32P4
  // PPA (Pixel Processing Accelerator) pour transformations hardware
  ppa_client_handle_t ppa_handle_{nullptr};
  uint8_t *transform_buffer_{nullptr};
  size_t transform_buffer_size_{0};
  
  bool init_ppa_();
  void deinit_ppa_();
  bool transform_frame_(const uint8_t *src, uint8_t *dst);
#endif

#ifdef MIPI_DSI_CAM_ENABLE_JPEG
  MipiDsiCamJPEGEncoder *jpeg_encoder_{nullptr};
  void init_jpeg_encoder_();
  void encode_jpeg_async_(const uint8_t *data, size_t size);
#endif

#ifdef MIPI_DSI_CAM_ENABLE_H264
  MipiDsiCamH264Encoder *h264_encoder_{nullptr};
  void init_h264_encoder_();
  void encode_h264_async_(const uint8_t *data, size_t size);
#endif

  // Configuration
  Rotation rotation_{ROTATION_0};
  bool mirror_x_{false};
  bool mirror_y_{false};
  uint32_t update_interval_{33};  // ~30 FPS par défaut
  
  // Encodeurs
  bool jpeg_output_{false};
  bool h264_output_{false};
  uint8_t jpeg_quality_{80};
  uint32_t h264_bitrate_{2000000};  // 2 Mbps
  uint32_t h264_gop_size_{30};
  
  // Stats
  uint32_t frame_count_{0};
  uint32_t last_update_time_{0};
  uint32_t last_fps_time_{0};
  
  // Flags
  bool first_update_{true};
  bool canvas_warning_shown_{false};
};

}  // namespace lvgl_camera_display
}  // namespace esphome
