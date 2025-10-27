
#include "mipi_dsi_cam_encoders.h"
#include "mipi_dsi_cam.h"
#include "esphome/core/log.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <algorithm>
#include <cstring>

#include "../lvgl_camera_display/mman.h"  // si tu n’en as pas besoin, retire-le
#include "videodev2.h"                    // ta version complète fournie

#ifdef USE_ESP32_VARIANT_ESP32P4
namespace esphome {
namespace mipi_dsi_cam {

/* ===================== Utils ===================== */

static inline bool xioctl(int fd, unsigned long req, void *arg) {
  int r;
  do { r = ioctl(fd, req, arg); } while (r == -1 && errno == EINTR);
  return r != -1;
}

/* ===================== Consts / TAGs ===================== */

#ifdef MIPI_DSI_CAM_ENABLE_JPEG
static const char *TAG_JPEG = "mipi_dsi_cam.jpeg";
#endif
#ifdef MIPI_DSI_CAM_ENABLE_H264
static const char *TAG_H264 = "mipi_dsi_cam.h264";
#endif

/* ===================== JPEG Encoder ===================== */
#ifdef MIPI_DSI_CAM_ENABLE_JPEG

MipiDsiCamJPEGEncoder::MipiDsiCamJPEGEncoder(MipiDsiCam *camera)
: camera_(camera) {}

MipiDsiCamJPEGEncoder::~MipiDsiCamJPEGEncoder() {
  if (this->initialized_) {
    this->deinit();
  }
}

esp_err_t MipiDsiCamJPEGEncoder::init(uint8_t quality) {
  if (this->initialized_) {
    ESP_LOGW(TAG_JPEG, "JPEG encoder already initialized");
    return ESP_OK;
  }

  this->quality_ = std::max<uint8_t>(1, std::min<uint8_t>(quality, 100));

  // Ouvrir le device JPEG
  this->jpeg_fd_ = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDWR | O_NONBLOCK);
  if (this->jpeg_fd_ < 0) {
    ESP_LOGE(TAG_JPEG, "Failed to open JPEG device %s", ESP_VIDEO_JPEG_DEVICE_NAME);
    return ESP_FAIL;
  }

  const uint32_t w = this->camera_->get_image_width();
  const uint32_t h = this->camera_->get_image_height();

  // Buffers applicatifs
  struct esp_video_buffer_info buffer_info = {
    .count = 3,
    .size = w * h * 2, // RGB565
    .caps = (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
    .align_size = 64,
    .memory_type = V4L2_MEMORY_USERPTR
  };
  this->input_buffer_ = esp_video_buffer_create(&buffer_info);
  if (!this->input_buffer_) {
    ESP_LOGE(TAG_JPEG, "Failed to create input buffer");
    close(this->jpeg_fd_); this->jpeg_fd_ = -1;
    return ESP_ERR_NO_MEM;
  }

  buffer_info.size = w * h; // sortie JPEG worst-case approx (sera ajusté par DQBUF bytesused)
  this->output_buffer_ = esp_video_buffer_create(&buffer_info);
  if (!this->output_buffer_) {
    ESP_LOGE(TAG_JPEG, "Failed to create output buffer");
    esp_video_buffer_destroy(this->input_buffer_); this->input_buffer_ = nullptr;
    close(this->jpeg_fd_); this->jpeg_fd_ = -1;
    return ESP_ERR_NO_MEM;
  }

  // Config formats
  struct v4l2_format in_fmt = {};
  in_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  in_fmt.fmt.pix.width = w;
  in_fmt.fmt.pix.height = h;
  in_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  in_fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (!xioctl(this->jpeg_fd_, VIDIOC_S_FMT, &in_fmt)) {
    ESP_LOGE(TAG_JPEG, "VIDIOC_S_FMT input failed");
    goto fail;
  }

  struct v4l2_format out_fmt = {};
  out_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  out_fmt.fmt.pix.width = w;
  out_fmt.fmt.pix.height = h;
  out_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  out_fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (!xioctl(this->jpeg_fd_, VIDIOC_S_FMT, &out_fmt)) {
    ESP_LOGE(TAG_JPEG, "VIDIOC_S_FMT output failed");
    goto fail;
  }

  // FPS si supporté
  struct v4l2_streamparm parm = {};
  parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  parm.parm.output.timeperframe.numerator = 1;
  parm.parm.output.timeperframe.denominator = std::max<uint32_t>(1, this->camera_->get_fps());
  xioctl(this->jpeg_fd_, VIDIOC_S_PARM, &parm); // best-effort

  // Demande de buffers USERPTR
  struct v4l2_requestbuffers req = {};
  req.count = this->input_buffer_->info.count;
  req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  req.memory = V4L2_MEMORY_USERPTR;
  if (!xioctl(this->jpeg_fd_, VIDIOC_REQBUFS, &req)) {
    ESP_LOGE(TAG_JPEG, "REQBUFS input failed");
    goto fail;
  }
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!xioctl(this->jpeg_fd_, VIDIOC_REQBUFS, &req)) {
    ESP_LOGE(TAG_JPEG, "REQBUFS output failed");
    goto fail;
  }

  // Qualité JPEG
  struct v4l2_control ctrl = {};
  ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
  ctrl.value = this->quality_;
  xioctl(this->jpeg_fd_, VIDIOC_S_CTRL, &ctrl); // best-effort

  this->streaming_started_out_ = false;
  this->streaming_started_cap_ = false;
  this->initialized_ = true;
  ESP_LOGI(TAG_JPEG, "✅ JPEG encoder initialized");
  return ESP_OK;

fail:
  if (this->input_buffer_) { esp_video_buffer_destroy(this->input_buffer_); this->input_buffer_ = nullptr; }
  if (this->output_buffer_) { esp_video_buffer_destroy(this->output_buffer_); this->output_buffer_ = nullptr; }
  if (this->jpeg_fd_ >= 0) { close(this->jpeg_fd_); this->jpeg_fd_ = -1; }
  return ESP_FAIL;
}

esp_err_t MipiDsiCamJPEGEncoder::deinit() {
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

  if (this->input_buffer_) { esp_video_buffer_destroy(this->input_buffer_); this->input_buffer_ = nullptr; }
  if (this->output_buffer_) { esp_video_buffer_destroy(this->output_buffer_); this->output_buffer_ = nullptr; }
  if (this->jpeg_fd_ >= 0) { close(this->jpeg_fd_); this->jpeg_fd_ = -1; }

  this->initialized_ = false;
  ESP_LOGI(TAG_JPEG, "JPEG encoder deinitialized");
  return ESP_OK;
}

void MipiDsiCamJPEGEncoder::set_quality(uint8_t quality) {
  quality = std::max<uint8_t>(1, std::min<uint8_t>(quality, 100));
  this->quality_ = quality;
  if (this->initialized_ && this->jpeg_fd_ >= 0) {
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
    ctrl.value = quality;
    if (!xioctl(this->jpeg_fd_, VIDIOC_S_CTRL, &ctrl)) {
      ESP_LOGW(TAG_JPEG, "Failed to update JPEG quality");
    } else {
      ESP_LOGI(TAG_JPEG, "JPEG quality updated to %u", quality);
    }
  }
}

esp_err_t MipiDsiCamJPEGEncoder::encode_frame_with_buffer(
  struct esp_video_buffer_element *input_element,
  uint8_t **jpeg_data, size_t *jpeg_size) {
  if (!this->initialized_ || !input_element) return ESP_FAIL;
  return this->encode_internal_(input_element->buffer, input_element->valid_size, jpeg_data, jpeg_size);
}

esp_err_t MipiDsiCamJPEGEncoder::encode_frame(
  const uint8_t *rgb_data, size_t rgb_size, uint8_t **jpeg_data, size_t *jpeg_size) {
  if (!this->initialized_) {
    ESP_LOGE(TAG_JPEG, "Encoder not initialized");
    return ESP_FAIL;
  }
  if (!rgb_data || !jpeg_data || !jpeg_size) return ESP_ERR_INVALID_ARG;
  return this->encode_internal_(rgb_data, rgb_size, jpeg_data, jpeg_size);
}

esp_err_t MipiDsiCamJPEGEncoder::encode_internal_(
  const uint8_t *src, size_t src_size, uint8_t **dst, size_t *dst_size) {

  // input: USERPTR
  struct esp_video_buffer_element *in_elem = nullptr;
  for (int i = 0; i < this->input_buffer_->info.count; i++) {
    if (ELEMENT_IS_FREE(&this->input_buffer_->element[i])) {
      in_elem = &this->input_buffer_->element[i];
      ELEMENT_SET_QUEUED(in_elem);
      break;
    }
  }
  if (!in_elem) {
    ESP_LOGW(TAG_JPEG, "No free input buffer");
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
    ESP_LOGE(TAG_JPEG, "QBUF input failed");
    return ESP_FAIL;
  }

  // output: USERPTR
  struct esp_video_buffer_element *out_elem = nullptr;
  for (int i = 0; i < this->output_buffer_->info.count; i++) {
    if (ELEMENT_IS_FREE(&this->output_buffer_->element[i])) {
      out_elem = &this->output_buffer_->element[i];
      ELEMENT_SET_QUEUED(out_elem);
      break;
    }
  }
  if (!out_elem) {
    ELEMENT_SET_FREE(in_elem);
    ESP_LOGW(TAG_JPEG, "No free output buffer");
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
    ESP_LOGE(TAG_JPEG, "QBUF output failed");
    return ESP_FAIL;
  }

  // Start streaming if needed
  if (!this->streaming_started_out_) {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (!xioctl(this->jpeg_fd_, VIDIOC_STREAMON, &t)) {
      ESP_LOGE(TAG_JPEG, "STREAMON output failed");
      return ESP_FAIL;
    }
    this->streaming_started_out_ = true;
  }
  if (!this->streaming_started_cap_) {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!xioctl(this->jpeg_fd_, VIDIOC_STREAMON, &t)) {
      ESP_LOGE(TAG_JPEG, "STREAMON capture failed");
      return ESP_FAIL;
    }
    this->streaming_started_cap_ = true;
  }

  // Dequeue CAPTURE (encoded JPEG)
  if (!xioctl(this->jpeg_fd_, VIDIOC_DQBUF, &out_buf)) {
    ELEMENT_SET_FREE(in_elem);
    ELEMENT_SET_FREE(out_elem);
    ESP_LOGE(TAG_JPEG, "DQBUF output failed");
    return ESP_FAIL;
  }

  // Dequeue OUTPUT (consumed)
  if (!xioctl(this->jpeg_fd_, VIDIOC_DQBUF, &in_buf)) {
    // Non fatal pour certains drivers
    ESP_LOGW(TAG_JPEG, "DQBUF input failed");
  }

  *dst = out_elem->buffer;
  *dst_size = out_buf.bytesused;
  out_elem->valid_size = out_buf.bytesused;

  ELEMENT_SET_ACTIVE(out_elem);
  ELEMENT_SET_FREE(in_elem);

  return ESP_OK;
}
#endif  // MIPI_DSI_CAM_ENABLE_JPEG

/* ===================== H.264 Encoder ===================== */
#ifdef MIPI_DSI_CAM_ENABLE_H264

MipiDsiCamH264Encoder::MipiDsiCamH264Encoder(MipiDsiCam *camera)
: camera_(camera) {}

MipiDsiCamH264Encoder::~MipiDsiCamH264Encoder() {
  if (this->initialized_) {
    this->deinit();
  }
}

esp_err_t MipiDsiCamH264Encoder::init(uint32_t bitrate, uint32_t gop_size) {
  if (this->initialized_) {
    ESP_LOGW(TAG_H264, "H264 encoder already initialized");
    return ESP_OK;
  }

  this->bitrate_ = bitrate;
  this->gop_size_ = std::max<uint32_t>(1, gop_size);
  this->frame_count_ = 0;

  // Ouvrir device H264
  this->h264_fd_ = open(ESP_VIDEO_H264_DEVICE_NAME, O_RDWR | O_NONBLOCK);
  if (this->h264_fd_ < 0) {
    ESP_LOGE(TAG_H264, "Failed to open H264 device %s", ESP_VIDEO_H264_DEVICE_NAME);
    return ESP_FAIL;
  }

  const uint32_t w = this->camera_->get_image_width();
  const uint32_t h = this->camera_->get_image_height();

  struct esp_video_buffer_info buffer_info = {
    .count = 3,
    .size = w * h * 2, // RGB565 par défaut (voir NOTE ci-dessous)
    .caps = (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
    .align_size = 64,
    .memory_type = V4L2_MEMORY_USERPTR
  };
  this->input_buffer_ = esp_video_buffer_create(&buffer_info);
  if (!this->input_buffer_) {
    ESP_LOGE(TAG_H264, "Failed to create input buffer");
    close(this->h264_fd_); this->h264_fd_ = -1;
    return ESP_ERR_NO_MEM;
  }

  buffer_info.size = (w * h); // sortie H264 (taille indicative, bytesused donnera la vraie taille)
  this->output_buffer_ = esp_video_buffer_create(&buffer_info);
  if (!this->output_buffer_) {
    ESP_LOGE(TAG_H264, "Failed to create output buffer");
    esp_video_buffer_destroy(this->input_buffer_); this->input_buffer_ = nullptr;
    close(this->h264_fd_); this->h264_fd_ = -1;
    return ESP_ERR_NO_MEM;
  }

  // Formats
  struct v4l2_format in_fmt = {};
  in_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  // NOTE (recommandé P4): H.264 attend du YUV420. Si tu as du RGB565, fais une conversion amont.
  // in_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420; // recommandé
  // in_fmt.fmt.pix.bytesperline = 0; // driver-side
  // Pour compat immédiate avec ton pipeline existant :
  in_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;

  in_fmt.fmt.pix.width = w;
  in_fmt.fmt.pix.height = h;
  in_fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (!xioctl(this->h264_fd_, VIDIOC_S_FMT, &in_fmt)) {
    ESP_LOGE(TAG_H264, "VIDIOC_S_FMT input failed");
    goto fail;
  }

  struct v4l2_format out_fmt = {};
  out_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  out_fmt.fmt.pix.width = w;
  out_fmt.fmt.pix.height = h;
  out_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
  out_fmt.fmt.pix.field = V4L2_FIELD_NONE;
  if (!xioctl(this->h264_fd_, VIDIOC_S_FMT, &out_fmt)) {
    ESP_LOGE(TAG_H264, "VIDIOC_S_FMT output failed");
    goto fail;
  }

  // FPS si supporté
  struct v4l2_streamparm parm = {};
  parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  parm.parm.output.timeperframe.numerator = 1;
  parm.parm.output.timeperframe.denominator = std::max<uint32_t>(1, this->camera_->get_fps());
  xioctl(this->h264_fd_, VIDIOC_S_PARM, &parm); // best-effort

  // REQBUFS USERPTR
  struct v4l2_requestbuffers req = {};
  req.count = this->input_buffer_->info.count;
  req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  req.memory = V4L2_MEMORY_USERPTR;
  if (!xioctl(this->h264_fd_, VIDIOC_REQBUFS, &req)) {
    ESP_LOGE(TAG_H264, "REQBUFS input failed");
    goto fail;
  }
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (!xioctl(this->h264_fd_, VIDIOC_REQBUFS, &req)) {
    ESP_LOGE(TAG_H264, "REQBUFS output failed");
    goto fail;
  }

  // Bitrate & GOP
  {
    struct v4l2_control c = {};
    c.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    c.value = (int)this->bitrate_;
    xioctl(this->h264_fd_, VIDIOC_S_CTRL, &c);
    c.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
    c.value = (int)this->gop_size_;
    xioctl(this->h264_fd_, VIDIOC_S_CTRL, &c);
  }

  this->streaming_started_out_ = false;
  this->streaming_started_cap_ = false;
  this->initialized_ = true;
  ESP_LOGI(TAG_H264, "✅ H264 encoder initialized");
  return ESP_OK;

fail:
  if (this->input_buffer_) { esp_video_buffer_destroy(this->input_buffer_); this->input_buffer_ = nullptr; }
  if (this->output_buffer_) { esp_video_buffer_destroy(this->output_buffer_); this->output_buffer_ = nullptr; }
  if (this->h264_fd_ >= 0) { close(this->h264_fd_); this->h264_fd_ = -1; }
  return ESP_FAIL;
}

esp_err_t MipiDsiCamH264Encoder::deinit() {
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

  if (this->input_buffer_) { esp_video_buffer_destroy(this->input_buffer_); this->input_buffer_ = nullptr; }
  if (this->output_buffer_) { esp_video_buffer_destroy(this->output_buffer_); this->output_buffer_ = nullptr; }
  if (this->h264_fd_ >= 0) { close(this->h264_fd_); this->h264_fd_ = -1; }

  this->initialized_ = false;
  ESP_LOGI(TAG_H264, "H264 encoder deinitialized");
  return ESP_OK;
}

void MipiDsiCamH264Encoder::set_bitrate(uint32_t bitrate) {
  this->bitrate_ = bitrate;
  if (this->initialized_ && this->h264_fd_ >= 0) {
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    ctrl.value = (int)bitrate;
    if (!xioctl(this->h264_fd_, VIDIOC_S_CTRL, &ctrl)) {
      ESP_LOGW(TAG_H264, "Failed to update bitrate");
    } else {
      ESP_LOGI(TAG_H264, "Bitrate updated to %u", bitrate);
    }
  }
}

void MipiDsiCamH264Encoder::set_gop_size(uint32_t gop_size) {
  this->gop_size_ = std::max<uint32_t>(1, gop_size);
  if (this->initialized_ && this->h264_fd_ >= 0) {
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
    ctrl.value = (int)this->gop_size_;
    if (!xioctl(this->h264_fd_, VIDIOC_S_CTRL, &ctrl)) {
      ESP_LOGW(TAG_H264, "Failed to update GOP size");
    } else {
      ESP_LOGI(TAG_H264, "GOP size updated to %u", this->gop_size_);
    }
  }
}

esp_err_t MipiDsiCamH264Encoder::encode_frame_with_buffer(
  struct esp_video_buffer_element *input_element,
  uint8_t **h264_data, size_t *h264_size, bool *is_keyframe) {
  if (!this->initialized_ || !input_element) return ESP_FAIL;
  return this->encode_internal_(
    input_element->buffer, input_element->valid_size, h264_data, h264_size, is_keyframe);
}

esp_err_t MipiDsiCamH264Encoder::encode_frame(
  const uint8_t *rgb_or_yuv_data, size_t in_size,
  uint8_t **h264_data, size_t *h264_size, bool *is_keyframe) {
  if (!this->initialized_) {
    ESP_LOGE(TAG_H264, "Encoder not initialized");
    return ESP_FAIL;
  }
  if (!rgb_or_yuv_data || !h264_data || !h264_size) return ESP_ERR_INVALID_ARG;
  return this->encode_internal_(rgb_or_yuv_data, in_size, h264_data, h264_size, is_keyframe);
}

esp_err_t MipiDsiCamH264Encoder::encode_internal_(
  const uint8_t *src, size_t src_size,
  uint8_t **dst, size_t *dst_size, bool *is_keyframe) {

  // INPUT USERPTR
  struct esp_video_buffer_element *in_elem = nullptr;
  for (int i = 0; i < this->input_buffer_->info.count; i++) {
    if (ELEMENT_IS_FREE(&this->input_buffer_->element[i])) {
      in_elem = &this->input_buffer_->element[i];
      ELEMENT_SET_QUEUED(in_elem);
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
    ESP_LOGE(TAG_H264, "QBUF input failed");
    return ESP_FAIL;
  }

  // OUTPUT USERPTR
  struct esp_video_buffer_element *out_elem = nullptr;
  for (int i = 0; i < this->output_buffer_->info.count; i++) {
    if (ELEMENT_IS_FREE(&this->output_buffer_->element[i])) {
      out_elem = &this->output_buffer_->element[i];
      ELEMENT_SET_QUEUED(out_elem);
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
    ESP_LOGE(TAG_H264, "QBUF output failed");
    return ESP_FAIL;
  }

  // STREAMON si nécessaire
  if (!this->streaming_started_out_) {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (!xioctl(this->h264_fd_, VIDIOC_STREAMON, &t)) {
      ESP_LOGE(TAG_H264, "STREAMON output failed");
      return ESP_FAIL;
    }
    this->streaming_started_out_ = true;
  }
  if (!this->streaming_started_cap_) {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!xioctl(this->h264_fd_, VIDIOC_STREAMON, &t)) {
      ESP_LOGE(TAG_H264, "STREAMON capture failed");
      return ESP_FAIL;
    }
    this->streaming_started_cap_ = true;
  }

  // Déterminer si I-frame selon GOP/compteur (indicatif)
  bool i_frame = (this->frame_count_ % std::max<uint32_t>(1, this->gop_size_)) == 0u;

  // DQBUF CAPTURE : données H.264 encodées
  if (!xioctl(this->h264_fd_, VIDIOC_DQBUF, &cbuf)) {
    ELEMENT_SET_FREE(in_elem);
    ELEMENT_SET_FREE(out_elem);
    ESP_LOGE(TAG_H264, "DQBUF output failed");
    return ESP_FAIL;
  }

  // DQBUF OUTPUT : buffer consommé
  if (!xioctl(this->h264_fd_, VIDIOC_DQBUF, &obuf)) {
    ESP_LOGW(TAG_H264, "DQBUF input failed");
  }

  if (is_keyframe) *is_keyframe = i_frame;
  this->frame_count_++;

  *dst = out_elem->buffer;
  *dst_size = cbuf.bytesused;
  out_elem->valid_size = cbuf.bytesused;

  ELEMENT_SET_ACTIVE(out_elem);
  ELEMENT_SET_FREE(in_elem);

  ESP_LOGD(TAG_H264, "Encoded H264 frame %u (%s), %u bytes",
           this->frame_count_, i_frame ? "I-frame" : "P-frame", (unsigned)*dst_size);

  return ESP_OK;
}
#endif // MIPI_DSI_CAM_ENABLE_H264

} // namespace mipi_dsi_cam
} // namespace esphome
#endif // USE_ESP32_VARIANT_ESP32P4

