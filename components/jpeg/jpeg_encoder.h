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
namespace jpeg {

/**
 * @brief Encodeur JPEG pour ESP32-P4
 * 
 * Utilise l'accélérateur matériel JPEG via V4L2
 */
class JPEGEncoder : public Component {
 public:
  JPEGEncoder() = default;
  ~JPEGEncoder();
  
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }
  
  /**
   * @brief Configure la caméra source
   */
  void set_camera(mipi_dsi_cam::MipiDsiCam *camera) { this->camera_ = camera; }
  
  /**
   * @brief Configure la qualité JPEG (1-100)
   */
  void set_quality(uint8_t quality);
  
  /**
   * @brief Obtient la qualité actuelle
   */
  uint8_t get_quality() const { return this->quality_; }
  
  /**
   * @brief Encode une frame RGB565 en JPEG (avec video buffer)
   */
  esp_err_t encode_frame_with_buffer(struct esp_video_buffer_element *input_element,
                                     uint8_t **jpeg_data,
                                     size_t *jpeg_size);
  
  /**
   * @brief Encode une frame RGB565 en JPEG (mode direct)
   */
  esp_err_t encode_frame(const uint8_t *rgb_data, 
                         size_t rgb_size,
                         uint8_t **jpeg_data, 
                         size_t *jpeg_size);
  
  /**
   * @brief Vérifie si l'encodeur est initialisé
   */
  bool is_initialized() const { return this->initialized_; }
  
 protected:
  mipi_dsi_cam::MipiDsiCam *camera_{nullptr};
  int jpeg_fd_{-1};
  bool initialized_{false};
  uint8_t quality_{80};
  
  // Buffers video pour input/output
  struct esp_video_buffer *input_buffer_{nullptr};
  struct esp_video_buffer *output_buffer_{nullptr};
  
  bool streaming_started_out_{false};
  bool streaming_started_cap_{false};
  
  esp_err_t init_internal_();
  esp_err_t deinit_internal_();
  esp_err_t encode_internal_(const uint8_t *src, size_t src_size,
                             uint8_t **dst, size_t *dst_size);
};

} // namespace jpeg
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
