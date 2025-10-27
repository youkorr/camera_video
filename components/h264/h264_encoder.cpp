#include "h264_encoder.h"
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

#include "../mipi_dsi_cam/mipi_dsi_cam_video_devices.h"

// ✅ AJOUT : Déclaration des fonctions de création des devices
extern "C" {
  esp_err_t mipi_dsi_cam_video_init(void);
  esp_err_t mipi_dsi_cam_create_h264_device(bool high_profile);
}

namespace esphome {
namespace h264 {

static const char *TAG = "h264_encoder";

// Helper pour les ioctl
static inline bool xioctl(int fd, unsigned long req, void *arg) {
  int r;
  do { r = ioctl(fd, req, arg); } while (r == -1 && errno == EINTR);
  return r != -1;
}

H264Encoder::~H264Encoder() {
  if (this->initialized_) {
    this->deinit_internal_();
  }
}

void H264Encoder::setup() {
  ESP_LOGI(TAG, "Setting up H.264 encoder...");
  
  if (!this->camera_) {
    ESP_LOGE(TAG, "No camera configured");
    this->mark_failed();
    return;
  }
  
  if (this->init_internal_() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize H.264 encoder");
    this->mark_failed();
    return;
  }
  
  ESP_LOGI(TAG, "✅ H.264 encoder ready (bitrate=%u, GOP=%u)", 
           this->bitrate_, this->gop_size_);
}

void H264Encoder::dump_config() {
  ESP_LOGCONFIG(TAG, "H.264 Encoder:");
  ESP_LOGCONFIG(TAG, "  Bitrate: %u bps", this->bitrate_);
  ESP_LOGCONFIG(TAG, "  GOP Size: %u", this->gop_size_);
  ESP_LOGCONFIG(TAG, "  Camera: %s", this->camera_ ? this->camera_->get_name().c_str() : "None");
  ESP_LOGCONFIG(TAG, "  Status: %s", this->initialized_ ? "Initialized" : "Not initialized");
}

void H264Encoder::set_bitrate(uint32_t bitrate) {
  this->bitrate_ = bitrate;
  
  if (this->initialized_ && this->h264_fd_ >= 0) {
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    ctrl.value = (int)bitrate;
    if (!xioctl(this->h264_fd_, VIDIOC_S_CTRL, &ctrl)) {
      ESP_LOGW(TAG, "Failed to update bitrate");
    } else {
      ESP_LOGI(TAG, "Bitrate updated to %u", bitrate);
    }
  }
}

void H264Encoder::set_gop_size(uint32_t gop_size) {
  this->gop_size_ = std::max<uint32_t>(1, gop_size);
  
  if (this->initialized_ && this->h264_fd_ >= 0) {
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
    ctrl.value = (int)this->gop_size_;
    if (!xioctl(this->h264_fd_, VIDIOC_S_CTRL, &ctrl)) {
      ESP_LOGW(TAG, "Failed to update GOP size");
    } else {
      ESP_LOGI(TAG, "GOP size updated to %u", this->gop_size_);
    }
  }
}

esp_err_t H264Encoder::init_internal_() {
  if (this->initialized_) {
    ESP_LOGW(TAG, "H.264 encoder already initialized");
    return ESP_OK;
  }

  // ✅ AJOUT : Initialiser le système vidéo
  ESP_LOGI(TAG, "Initializing video subsystem...");
  esp_err_t ret = mipi_dsi_cam_video_init();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Video subsystem init returned: 0x%x", ret);
  }

  // ✅ AJOUT : Créer le device H.264 s'il n'existe pas
  ESP_LOGI(TAG, "Creating H.264 device...");
  ret = mipi_dsi_cam_create_h264_device(false);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Failed to create H.264 device: 0x%x (may already exist)", ret);
  }

  // Petit délai pour laisser le device se stabiliser
  delay(50);

  // Ouvrir le device H264
  this->h264_fd_ = open(ESP_VIDEO_H264_DEVICE_NAME, O_RDWR | O_NONBLOCK);
  if (this->h264_fd_ < 0) {
    ESP_LOGE(TAG, "Failed to open H264 device %s (errno=%d)", 
             ESP_VIDEO_H264_DEVICE_NAME, errno);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "✅ H.264 device opened (fd=%d)", this->h264_fd_);

  const uint32_t w = this->camera_->get_image_width();
  const uint32_t h = this->camera_->get_image_height();

  // Buffers applicatifs
  struct esp_video_buffer_info buffer_info = {
    .count = 3,
    .size = w * h * 2, // RGB565
    .align_size = 64,
    .caps = (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
    .memory_type = V4L2_MEMORY_USERPTR
  };
  
  this->input_buffer_ = esp_video_buffer_create(&buffer_info);
  if (!this->input_buffer_) {
    ESP_LOGE(TAG, "Failed to create input buffer");
    close(this->h264_fd_); 
    this->h264_fd_ = -1;
    return ESP_ERR_NO_MEM;
  }

  buffer_info.size = (w * h); // sortie H264
  this->output_buffer_ = esp_video_buffer_create(&buffer_info);
  if (!this->output_buffer_) {
    ESP_LOGE(TAG, "Failed to create output buffer");
    esp_video_buffer_destroy(this->input_buffer_); 
    this->input_buffer_ = nullptr;
    close(this->h264_fd_); 
    this->h264_fd_ = -1;
    return ESP_ERR_NO_MEM;
  }

  // Déclarer les variables AVANT les goto
  struct v4l2_format in_fmt = {};
  struct v4l2_format out_fmt = {};
  struct v4l2_streamparm parm = {};
  struct v4l2_requestbuffers req = {};
  struct v4l2_control c = {};

  // Config format input
  in_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  in_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  in_fmt.fmt.pix.width = w;
  in_fmt.fmt.pix.height = h;
  in_fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (!xioctl(this->h264_fd_, VIDIOC_S_FMT, &in_fmt)) {
    ESP_LOGE(TAG, "VIDIOC_S_FMT input failed");
    goto fail;
  }

  // Config format output
  out_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  out_fmt.fmt.pix.width = w;
  out_fmt.fmt.pix.height = h;
  out_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
  out_fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (!xioctl(this->h264_fd_, VIDIOC_S_FMT, &out_fmt)) {
    ESP_LOGE(TAG, "VIDIOC_S_FMT output failed");
    goto fail;
  }

  // FPS si supporté
  parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  parm.parm.output.timeperframe.numerator = 1;
  parm.parm.output.timeperframe.denominator = std::max<uint32_t>(1, this->camera_->get_fps());
  xioctl(this->h264_fd_, VIDIOC_S_PARM, &parm);

  // Demande de buffers USERPTR
  req.count = this->input_buffer_->info.count;
  req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  req.memory = V4L2_MEMORY_USERPTR;
  if (!xioctl(this->h264_fd_, VIDIOC_REQBUFS, &req)) {
    ESP_LOGE(TAG, "REQBUFS input failed");
    goto fail;
  }
  
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!xioctl(this->h264_fd_, VIDIOC_REQBUFS, &req)) {
    ESP_LOGE(TAG, "REQBUFS output failed");
    goto fail;
  }

  // Bitrate & GOP
  c.id = V4L2_CID_MPEG_VIDEO_BITRATE;
  c.value = (int)this->bitrate_;
  xioctl(this->h264_fd_, VIDIOC_S_CTRL, &c);
  
  c.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
  c.value = (int)this->gop_size_;
  xioctl(this->h264_fd_, VIDIOC_S_CTRL, &c);

  this->streaming_started_out_ = false;
  this->streaming_started_cap_ = false;
  this->initialized_ = true;
  
  ESP_LOGI(TAG, "✅ H.264 encoder initialized");
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
  if (this->h264_fd_ >= 0) { 
    close(this->h264_fd_); 
    this->h264_fd_ = -1; 
  }
  return ESP_FAIL;
}

esp_err_t H264Encoder::deinit_internal_() {
  if (!this->initialized_) return ESP_OK;

  if (this->h264_fd_ >= 0) {
    if (this->streaming_started_out_) {
      enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
      xioctl(this->h264_fd_, VIDIOC_STREAMOFF, &type);
      this->streaming_started_out_ = false;
    }
    if (this->streaming_started_cap_) {
      enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      xioctl(this->h264_fd_, VIDIOC_STREAMOFF, &type);
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
  if (this->h264_fd_ >= 0) { 
    close(this->h264_fd_); 
    this->h264_fd_ = -1; 
  }

  this->initialized_ = false;
  ESP_LOGI(TAG, "H.264 encoder deinitialized");
  return ESP_OK;
}

esp_err_t H264Encoder::encode_frame_with_buffer(
  struct esp_video_buffer_element *input_element,
  uint8_t **h264_data, size_t *h264_size, bool *is_keyframe) {
  if (!this->initialized_ || !input_element) return ESP_FAIL;
  return this->encode_internal_(
    input_element->buffer, input_element->valid_size, h264_data, h264_size, is_keyframe);
}

esp_err_t H264Encoder::encode_frame(
  const uint8_t *rgb_or_yuv_data, size_t in_size,
  uint8_t **h264_data, size_t *h264_size, bool *is_keyframe) {
  if (!this->initialized_) {
    ESP_LOGE(TAG, "Encoder not initialized");
    return ESP_FAIL;
  }
  if (!rgb_or_yuv_data || !h264_data || !h264_size) return ESP_ERR_INVALID_ARG;
  return this->encode_internal_(rgb_or_yuv_data, in_size, h264_data, h264_size, is_keyframe);
}

esp_err_t H264Encoder::encode_internal_(
  const uint8_t *src, size_t src_size,
  uint8_t **dst, size_t *dst_size, bool *is_keyframe) {

  // Input: USERPTR
  struct esp_video_buffer_element *in_elem = nullptr;
  for (int i = 0; i < this->input_buffer_->info.count; i++) {
    if (ELEMENT_IS_FREE(&this->input_buffer_->element[i])) {
      in_elem = &this->input_buffer_->element[i];
      ELEMENT_SET_ALLOCATED(in_elem);
      break;
    }
  }
  if (!in_elem) return ESP_ERR_NO_MEM;

  std::memcpy(in_elem->buffer, src, std::min(src_size, (size_t)this->input_buffer_->info.size));
  in_elem->valid_size = src_size;

  struct v4l2_buffer obuf = {};
  obuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  obuf.memory = V4L2_MEMORY_USERPTR;
  obuf.index = in_elem->index;
  obuf.m.userptr = reinterpret_cast<unsigned long>(in_elem->buffer);
  obuf.length = this->input_buffer_->info.size;
  obuf.bytesused = in_elem->valid_size;

  if (!xioctl(this->h264_fd_, VIDIOC_QBUF, &obuf)) {
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
    return ESP_ERR_NO_MEM;
  }

  struct v4l2_buffer cbuf = {};
  cbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  cbuf.memory = V4L2_MEMORY_USERPTR;
  cbuf.index = out_elem->index;
  cbuf.m.userptr = reinterpret_cast<unsigned long>(out_elem->buffer);
  cbuf.length = this->output_buffer_->info.size;

  if (!xioctl(this->h264_fd_, VIDIOC_QBUF, &cbuf)) {
    ELEMENT_SET_FREE(in_elem);
    ELEMENT_SET_FREE(out_elem);
    ESP_LOGE(TAG, "QBUF output failed");
    return ESP_FAIL;
  }

  // STREAMON si nécessaire
  if (!this->streaming_started_out_) {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (!xioctl(this->h264_fd_, VIDIOC_STREAMON, &t)) {
      ESP_LOGE(TAG, "STREAMON output failed");
      return ESP_FAIL;
    }
    this->streaming_started_out_ = true;
  }
  if (!this->streaming_started_cap_) {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!xioctl(this->h264_fd_, VIDIOC_STREAMON, &t)) {
      ESP_LOGE(TAG, "STREAMON capture failed");
      return ESP_FAIL;
    }
    this->streaming_started_cap_ = true;
  }

  // Déterminer si I-frame selon GOP/compteur
  bool i_frame = (this->frame_count_ % std::max<uint32_t>(1, this->gop_size_)) == 0u;

  // DQBUF CAPTURE
  if (!xioctl(this->h264_fd_, VIDIOC_DQBUF, &cbuf)) {
    ELEMENT_SET_FREE(in_elem);
    ELEMENT_SET_FREE(out_elem);
    ESP_LOGE(TAG, "DQBUF output failed");
    return ESP_FAIL;
  }

  // DQBUF OUTPUT
  if (!xioctl(this->h264_fd_, VIDIOC_DQBUF, &obuf)) {
    ESP_LOGW(TAG, "DQBUF input failed");
  }

  if (is_keyframe) *is_keyframe = i_frame;
  this->frame_count_++;

  *dst = out_elem->buffer;
  *dst_size = cbuf.bytesused;
  out_elem->valid_size = cbuf.bytesused;

  ELEMENT_SET_FREE(in_elem);

  ESP_LOGD(TAG, "Encoded H.264 frame %u (%s), %u bytes",
           this->frame_count_, i_frame ? "I-frame" : "P-frame", (unsigned)*dst_size);

  return ESP_OK;
}

} // namespace h264
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
