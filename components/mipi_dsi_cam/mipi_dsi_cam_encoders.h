#pragma once

#ifdef MIPI_DSI_CAM_ENABLE_JPEG
#ifdef MIPI_DSI_CAM_ENABLE_H264

#include "esphome/core/component.h"
#include "esp_video_buffer.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include "videodev2.h"

// Devices ESP32-P4
#define ESP_VIDEO_JPEG_DEVICE_NAME  "/dev/video0"
#define ESP_VIDEO_H264_DEVICE_NAME  "/dev/video1"

#ifdef USE_ESP32_VARIANT_ESP32P4

namespace esphome {
namespace mipi_dsi_cam {

// Forward declaration
class MipiDsiCam;

// ========================================
// JPEG Encoder avec esp_video_buffer
// ========================================
class MipiDsiCamJPEGEncoder {
 public:
  MipiDsiCamJPEGEncoder(MipiDsiCam *camera);
  ~MipiDsiCamJPEGEncoder();
  
  esp_err_t init(uint8_t quality = 80);
  esp_err_t deinit();
  
  // Encoder une frame RGB565 en JPEG (avec video buffer)
  esp_err_t encode_frame_with_buffer(struct esp_video_buffer_element *input_element,
                                     uint8_t **jpeg_data,
                                     size_t *jpeg_size);
  
  // Encoder une frame RGB565 en JPEG (mode direct)
  esp_err_t encode_frame(const uint8_t *rgb_data, 
                         size_t rgb_size,
                         uint8_t **jpeg_data, 
                         size_t *jpeg_size);
  
  void set_quality(uint8_t quality);
  uint8_t get_quality() const { return this->quality_; }
  bool is_initialized() const { return this->initialized_; }
  
 protected:
  MipiDsiCam *camera_;
  int jpeg_fd_{-1};
  bool initialized_{false};
  uint8_t quality_{80};
  
  // Buffers video pour input/output
  struct esp_video_buffer *input_buffer_{nullptr};
  struct esp_video_buffer *output_buffer_{nullptr};
  
  uint8_t *jpeg_buffer_{nullptr};
  size_t jpeg_buffer_size_{0};
  
  bool setup_v4l2_buffers_();
  esp_err_t encode_internal_(const uint8_t *src, size_t src_size,
                             uint8_t **dst, size_t *dst_size);
};

// ========================================
// H264 Encoder avec esp_video_buffer
// ========================================
class MipiDsiCamH264Encoder {
 public:
  MipiDsiCamH264Encoder(MipiDsiCam *camera);
  ~MipiDsiCamH264Encoder();
  
  esp_err_t init(uint32_t bitrate = 2000000, uint32_t gop_size = 30);
  esp_err_t deinit();
  
  // Encoder une frame RGB565 en H264 (avec video buffer)
  esp_err_t encode_frame_with_buffer(struct esp_video_buffer_element *input_element,
                                     uint8_t **h264_data,
                                     size_t *h264_size,
                                     bool *is_keyframe = nullptr);
  
  // Encoder une frame RGB565 en H264 (mode direct)
  esp_err_t encode_frame(const uint8_t *rgb_data,
                         size_t rgb_size,
                         uint8_t **h264_data,
                         size_t *h264_size,
                         bool *is_keyframe = nullptr);
  
  void set_bitrate(uint32_t bitrate);
  void set_gop_size(uint32_t gop_size);
  
  uint32_t get_bitrate() const { return this->bitrate_; }
  uint32_t get_gop_size() const { return this->gop_size_; }
  bool is_initialized() const { return this->initialized_; }
  
 protected:
  MipiDsiCam *camera_;
  int h264_fd_{-1};
  bool initialized_{false};
  uint32_t bitrate_{2000000};
  uint32_t gop_size_{30};
  uint32_t frame_count_{0};
  
  // Buffers video pour input/output
  struct esp_video_buffer *input_buffer_{nullptr};
  struct esp_video_buffer *output_buffer_{nullptr};
  
  uint8_t *h264_buffer_{nullptr};
  size_t h264_buffer_size_{0};
  
  bool setup_v4l2_buffers_();
  esp_err_t encode_internal_(const uint8_t *src, size_t src_size,
                             uint8_t **dst, size_t *dst_size, 
                             bool *is_keyframe);
};

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
#endif // MIPI_DSI_CAM_ENABLE_H264
#endif // MIPI_DSI_CAM_ENABLE_JPEG
