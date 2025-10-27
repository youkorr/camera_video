#pragma once

#include "esphome/core/component.h"
#include "../mipi_dsi_cam/esp_video_buffer.h"
#include <fcntl.h>
#include "../lvgl_camera_display/ioctl.h"
#include "../mipi_dsi_cam/videodev2.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

// Forward declaration
namespace esphome {
namespace mipi_dsi_cam {
class MipiDsiCam;
}
}

namespace esphome {
namespace h264 {

/**
 * @brief Encodeur H.264 pour ESP32-P4
 * 
 * Utilise l'accélérateur matériel H.264 via V4L2
 */
class H264Encoder : public Component {
 public:
  H264Encoder() = default;
  ~H264Encoder();
  
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }
  
  /**
   * @brief Configure la caméra source
   */
  void set_camera(mipi_dsi_cam::MipiDsiCam *camera) { this->camera_ = camera; }
  
  /**
   * @brief Configure le bitrate (bps)
   */
  void set_bitrate(uint32_t bitrate);
  
  /**
   * @brief Configure la taille du GOP (Group of Pictures)
   */
  void set_gop_size(uint32_t gop_size);
  
  /**
   * @brief Obtient le bitrate actuel
   */
  uint32_t get_bitrate() const { return this->bitrate_; }
  
  /**
   * @brief Obtient la taille du GOP actuelle
   */
  uint32_t get_gop_size() const { return this->gop_size_; }
  
  /**
   * @brief Encode une frame RGB565 en H.264 (avec video buffer)
   */
  esp_err_t encode_frame_with_buffer(struct esp_video_buffer_element *input_element,
                                     uint8_t **h264_data,
                                     size_t *h264_size,
                                     bool *is_keyframe = nullptr);
  
  /**
   * @brief Encode une frame RGB565 en H.264 (mode direct)
   */
  esp_err_t encode_frame(const uint8_t *rgb_data,
                         size_t rgb_size,
                         uint8_t **h264_data,
                         size_t *h264_size,
                         bool *is_keyframe = nullptr);
  
  /**
   * @brief Vérifie si l'encodeur est initialisé
   */
  bool is_initialized() const { return this->initialized_; }
  
 protected:
  mipi_dsi_cam::MipiDsiCam *camera_{nullptr};
  int h264_fd_{-1};
  bool initialized_{false};
  uint32_t bitrate_{2000000};
  uint32_t gop_size_{30};
  uint32_t frame_count_{0};
  
  // Buffers video pour input/output
  struct esp_video_buffer *input_buffer_{nullptr};
  struct esp_video_buffer *output_buffer_{nullptr};
  
  bool streaming_started_out_{false};
  bool streaming_started_cap_{false};
  
  esp_err_t init_internal_();
  esp_err_t deinit_internal_();
  esp_err_t encode_internal_(const uint8_t *src, size_t src_size,
                             uint8_t **dst, size_t *dst_size, 
                             bool *is_keyframe);
};

} // namespace h264
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
