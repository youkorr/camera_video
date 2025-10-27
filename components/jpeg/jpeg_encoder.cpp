#include "jpeg_encoder.h"
#include "../mipi_dsi_cam/mipi_dsi_cam.h"
#include "esphome/core/log.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <algorithm>
#include <cstring>

#include "../lvgl_camera_display/mman.h"
#include "../mipi_dsi_cam/videodev2.h"
#include "../mipi_dsi_cam/esp_video_device.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

// ✅ CORRECTION : Inclure le header au lieu de redéclarer
#include "../mipi_dsi_cam/mipi_dsi_cam_video_devices.h"

namespace esphome {
namespace jpeg {

static const char *TAG = "jpeg_encoder";

// Helper pour les ioctl
static inline bool xioctl(int fd, unsigned long req, void *arg) {
  int r;
  do { r = ioctl(fd, req, arg); } while (r == -1 && errno == EINTR);
  return r != -1;
}

JPEGEncoder::~JPEGEncoder() {
  if (this->initialized_) {
    this->deinit_internal_();
  }
}

void JPEGEncoder::setup() {
  ESP_LOGI(TAG, "Setting up JPEG encoder...");
  
  if (!this->camera_) {
    ESP_LOGE(TAG, "No camera configured");
    this->mark_failed();
    return;
  }
  
  if (this->init_internal_() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize JPEG encoder");
    this->mark_failed();
    return;
  }
  
  ESP_LOGI(TAG, "✅ JPEG encoder ready (quality=%u)", this->quality_);
}

void JPEGEncoder::dump_config() {
  ESP_LOGCONFIG(TAG, "JPEG Encoder:");
  ESP_LOGCONFIG(TAG, "  Quality: %u", this->quality_);
  ESP_LOGCONFIG(TAG, "  Camera: %s", this->camera_ ? this->camera_->get_name().c_str() : "None");
  ESP_LOGCONFIG(TAG, "  Status: %s", this->initialized_ ? "Initialized" : "Not initialized");
}

void JPEGEncoder::set_quality(uint8_t quality) {
  quality = std::max<uint8_t>(1, std::min<uint8_t>(quality, 100));
  this->quality_ = quality;
  
  if (this->initialized_ && this->jpeg_fd_ >= 0) {
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
    ctrl.value = quality;
    if (!xioctl(this->jpeg_fd_, VIDIOC_S_CTRL, &ctrl)) {
      ESP_LOGW(TAG, "Failed to update JPEG quality");
    } else {
      ESP_LOGI(TAG, "JPEG quality updated to %u", quality);
    }
  }
}

esp_err_t JPEGEncoder::init_internal_() {
  if (this->initialized_) {
    ESP_LOGW(TAG, "JPEG encoder already initialized");
    return ESP_OK;
  }

  // ✅ AJOUT : Initialiser le système vidéo
  ESP_LOGI(TAG, "Initializing video subsystem...");
  esp_err_t ret = mipi_dsi_cam_video_init();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Video subsystem init returned: 0x%x", ret);
  }

  // ✅ AJOUT : Créer le device JPEG s'il n'existe pas
  ESP_LOGI(TAG, "Creating JPEG device...");
  ret = mipi_dsi_cam_create_jpeg_device(this->camera_);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Failed to create JPEG device: 0x%x (may already exist)", ret);
  }

  // Petit délai pour laisser le device se stabiliser
  delay(50);

  // Ouvrir le device JPEG
  this->jpeg_fd_ = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDWR | O_NONBLOCK);
  if (this->jpeg_fd_ < 0) {
    ESP_LOGE(TAG, "Failed to open JPEG device %s (errno=%d)", 
             ESP_VIDEO_JPEG_DEVICE_NAME, errno);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "✅ JPEG device opened (fd=%d)", this->jpeg_fd_);

  const uint32_t w = this->camera_->get_image_width();
  const uint32_t h = this->camera_->get_image_height();

  struct esp_video_buffer_info buffer_info = {
    .count = 3,
    .size = w * h * 2,
    .align_size = 64,
    .caps = (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
    .memory_type = V4L2_MEMORY_USERPTR
  };
  
  this->input_buffer_ = esp_video_buffer_create(&buffer_info);
  if (!this->input_buffer_) {
    ESP_LOGE(TAG, "Failed to create input buffer");
    close(this->jpeg_fd_); 
    this->jpeg_fd_ = -1;
    return ESP_ERR_NO_MEM;
  }

  buffer_info.size = w * h;
  this->output_buffer_ = esp_video_buffer_create(&buffer_info);
  if (!this->output_buffer_) {
    ESP_LOGE(TAG, "Failed to create output buffer");
    esp_video_buffer_destroy(this->input_buffer_); 
    this->input_buffer_ = nullptr;
    close(this->jpeg_fd_); 
    this->jpeg_fd_ = -1;
    return ESP_ERR_NO_MEM;
  }

  // Déclarer AVANT les goto
  struct v4l2_format in_fmt = {};
  struct v4l2_format out_fmt = {};
  struct v4l2_streamparm parm = {};
  struct v4l2_requestbuffers req = {};
  struct v4l2_control ctrl = {};

  // Config input format
  in_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  in_fmt.fmt.pix.width = w;
  in_fmt.fmt.pix.height = h;
  in_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  in_fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (!xioctl(this->jpeg_fd_, VIDIOC_S_FMT, &in_fmt)) {
    ESP_LOGE(TAG, "VIDIOC_S_FMT input failed");
    goto fail;
  }

  // Config output format
  out_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  out_fmt.fmt.pix.width = w;
  out_fmt.fmt.pix.height = h;
  out_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  out_fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (!xioctl(this->jpeg_fd_, VIDIOC_S_FMT, &out_fmt)) {
    ESP_LOGE(TAG, "VIDIOC_S_FMT output failed");
    goto fail;
  }

  // FPS
  parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  parm.parm.output.timeperframe.numerator = 1;
  parm.parm.output.timeperframe.denominator = std::max<uint32_t>(1, this->camera_->get_fps());
  xioctl(this->jpeg_fd_, VIDIOC_S_PARM, &parm);

  // Request buffers
  req.count = this->input_buffer_->info.count;
  req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  req.memory = V4L2_MEMORY_USERPTR;
  if (!xioctl(this->jpeg_fd_, VIDIOC_REQBUFS, &req)) {
    ESP_LOGE(TAG, "REQBUFS input failed");
    goto fail;
  }
  
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!xioctl(this->jpeg_fd_, VIDIOC_REQBUFS, &req)) {
    ESP_LOGE(TAG, "REQBUFS output failed");
    goto fail;
  }

  // Qualité JPEG
  ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
  ctrl.value = this->quality_;
  xioctl(this->jpeg_fd_, VIDIOC_S_CTRL, &ctrl);

  this->streaming_started_out_ = false;
  this->streaming_started_cap_ = false;
  this->initialized_ = true;
  
  ESP_LOGI(TAG, "✅ JPEG encoder initialized");
  return ESP_OK;

fail:
  if (this->input_buffer_) { 
    esp_video_buffer_destroy(this->input_buffer_); 
    this->input_buffer_ = nullptr; 
  }
  if (this->output_buffer_) { 
    esp_video_buffer_destroy(this->output_buffer_); 
    this->output_buffer_ = nullptr; 
  }
  if (this->jpeg_fd_ >= 0) { 
    close(this->jpeg_fd_); 
    this->jpeg_fd_ = -1; 
  }
  return ESP_FAIL;
}

esp_err_t JPEGEncoder::deinit_internal_() {
  if (!this->initialized_) return ESP_OK;

  if (this->jpeg_fd_ >= 0) {
    if (this->streaming_started_out_) {
      enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
      xioctl(this->jpeg_fd_, VIDIOC_STREAMOFF, &type);
      this->streaming_started_out_ = false;
    }
    if (this->streaming_started_cap_) {
      enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      xioctl(this->jpeg_fd_, VIDIOC_STREAMOFF, &type);
      this->streaming_started_cap_ = false;
    }
  }

  if (this->input_buffer_) { 
    esp_video_buffer_destroy(this->input_buffer_); 
    this->input_buffer_ = nullptr; 
  }
  if (this->output_buffer_) { 
    esp_video_buffer_destroy(this->output_buffer_); 
    this->output_buffer_ = nullptr; 
  }
  if (this->jpeg_fd_ >= 0) { 
    close(this->jpeg_fd_); 
    this->jpeg_fd_ = -1; 
  }

  this->initialized_ = false;
  ESP_LOGI(TAG, "JPEG encoder deinitialized");
  return ESP_OK;
}

esp_err_t JPEGEncoder::encode_frame_with_buffer(
  struct esp_video_buffer_element *input_element,
  uint8_t **jpeg_data, size_t *jpeg_size) {
  if (!this->initialized_ || !input_element) return ESP_FAIL;
  return this->encode_internal_(input_element->buffer, input_element->valid_size, jpeg_data, jpeg_size);
}

esp_err_t JPEGEncoder::encode_frame(
  const uint8_t *rgb_data, size_t rgb_size, uint8_t **jpeg_data, size_t *jpeg_size) {
  if (!this->initialized_) {
    ESP_LOGE(TAG, "Encoder not initialized");
    return ESP_FAIL;
  }
  if (!rgb_data || !jpeg_data || !jpeg_size) return ESP_ERR_INVALID_ARG;
  return this->encode_internal_(rgb_data, rgb_size, jpeg_data, jpeg_size);
}

esp_err_t JPEGEncoder::encode_internal_(
  const uint8_t *src, size_t src_size, uint8_t **dst, size_t *dst_size) {

  // Input: USERPTR
  struct esp_video_buffer_element *in_elem = nullptr;
  for (int i = 0; i < this->input_buffer_->info.count; i++) {
    if (ELEMENT_IS_FREE(&this->input_buffer_->element[i])) {
      in_elem = &this->input_buffer_->element[i];
      ELEMENT_SET_ALLOCATED(in_elem);
      break;
    }
  }
  if (!in_elem) {
    ESP_LOGW(TAG, "No free input buffer");
    return ESP_ERR_NO_MEM;
  }
  
  std::memcpy(in_elem->buffer, src, std::min(src_size, (size_t)this->input_buffer_->info.size));
  in_elem->valid_size = src_size;

  struct v4l2_buffer in_buf = {};
  in_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  in_buf.memory = V4L2_MEMORY_USERPTR;
  in_buf.index = in_elem->index;
  in_buf.m.userptr = reinterpret_cast<unsigned long>(in_elem->buffer);
  in_buf.length = this->input_buffer_->info.size;
  in_buf.bytesused = in_elem->valid_size;

  if (!xioctl(this->jpeg_fd_, VIDIOC_QBUF, &in_buf)) {
    ELEMENT_SET_FREE(in_elem);
    ESP_LOGE(TAG, "QBUF input failed");
    return ESP_FAIL;
  }

  // Output: USERPTR
  struct esp_video_buffer_element *out_elem = nullptr;
  for (int i = 0; i < this->output_buffer_->info.count; i++) {
    if (ELEMENT_IS_FREE(&this->output_buffer_->element[i])) {
      out_elem = &this->output_buffer_->element[i];
      ELEMENT_SET_ALLOCATED(out_elem);
      break;
    }
  }
  if (!out_elem) {
    ELEMENT_SET_FREE(in_elem);
    ESP_LOGW(TAG, "No free output buffer");
    return ESP_ERR_NO_MEM;
  }

  struct v4l2_buffer out_buf = {};
  out_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  out_buf.memory = V4L2_MEMORY_USERPTR;
  out_buf.index = out_elem->index;
  out_buf.m.userptr = reinterpret_cast<unsigned long>(out_elem->buffer);
  out_buf.length = this->output_buffer_->info.size;

  if (!xioctl(this->jpeg_fd_, VIDIOC_QBUF, &out_buf)) {
    ELEMENT_SET_FREE(in_elem);
    ELEMENT_SET_FREE(out_elem);
    ESP_LOGE(TAG, "QBUF output failed");
    return ESP_FAIL;
  }

  // Start streaming if needed
  if (!this->streaming_started_out_) {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (!xioctl(this->jpeg_fd_, VIDIOC_STREAMON, &t)) {
      ESP_LOGE(TAG, "STREAMON output failed");
      return ESP_FAIL;
    }
    this->streaming_started_out_ = true;
  }
  if (!this->streaming_started_cap_) {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!xioctl(this->jpeg_fd_, VIDIOC_STREAMON, &t)) {
      ESP_LOGE(TAG, "STREAMON capture failed");
      return ESP_FAIL;
    }
    this->streaming_started_cap_ = true;
  }

  // Dequeue CAPTURE
  if (!xioctl(this->jpeg_fd_, VIDIOC_DQBUF, &out_buf)) {
    ELEMENT_SET_FREE(in_elem);
    ELEMENT_SET_FREE(out_elem);
    ESP_LOGE(TAG, "DQBUF output failed");
    return ESP_FAIL;
  }

  // Dequeue OUTPUT
  if (!xioctl(this->jpeg_fd_, VIDIOC_DQBUF, &in_buf)) {
    ESP_LOGW(TAG, "DQBUF input failed");
  }

  *dst = out_elem->buffer;
  *dst_size = out_buf.bytesused;
  out_elem->valid_size = out_buf.bytesused;

  ELEMENT_SET_FREE(in_elem);

  return ESP_OK;
}

} // namespace jpeg
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
