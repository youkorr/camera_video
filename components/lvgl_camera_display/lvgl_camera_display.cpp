#include "lvgl_camera_display.h"
#include "esphome/core/log.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

#ifdef USE_ESP32_VARIANT_ESP32P4
#include <sys/time.h>
#include "esp_heap_caps.h"
#endif

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

void LVGLCameraDisplay::setup() {
  ESP_LOGCONFIG(TAG, "🎥 LVGL Camera Display (V4L2 Pipeline)");

#ifdef USE_ESP32_VARIANT_ESP32P4
  // Récupérer la résolution depuis la caméra
  this->frame_count_ = 0;
  this->last_update_time_ = 0;
  this->last_fps_time_ = 0;
  this->first_update_ = true;
  this->streaming_ = false;
  this->camera_streaming_started_ = false;
  this->last_display_buffer_ = nullptr;

  if (this->camera_) {
    this->width_ = this->camera_->get_image_width();
    this->height_ = this->camera_->get_image_height();
    ESP_LOGI(TAG, "📐 Using camera resolution: %ux%u", this->width_, this->height_);
  } else {
    ESP_LOGW(TAG, "⚠️  No camera linked, using default resolution %ux%u", 
    ESP_LOGW(TAG, "⚠️  No camera linked, using default resolution %ux%u",
             this->width_, this->height_);
  }

  // Ouvrir le device vidéo
  if (!this->open_video_device_()) {
    ESP_LOGE(TAG, "❌ Failed to open video device");
    this->mark_failed();
    return;
  }

  // Setup des buffers mmap
  if (!this->setup_buffers_()) {
    ESP_LOGE(TAG, "❌ Failed to setup buffers");
    this->cleanup_();
    this->mark_failed();
    return;
  }

  // Initialiser PPA si transformations
  if (this->rotation_ != ROTATION_0 || this->mirror_x_ || this->mirror_y_) {
    if (!this->init_ppa_()) {
      ESP_LOGE(TAG, "❌ Failed to initialize PPA");
      this->cleanup_();
      this->mark_failed();
      return;
    }

    ESP_LOGI(TAG, "✅ PPA initialized (rotation=%d°, mirror_x=%s, mirror_y=%s)",
             this->rotation_, this->mirror_x_ ? "ON" : "OFF", 
             this->rotation_, this->mirror_x_ ? "ON" : "OFF",
             this->mirror_y_ ? "ON" : "OFF");
  }

  // Démarrer le streaming
  if (!this->start_streaming_()) {
    ESP_LOGE(TAG, "❌ Failed to start streaming");
    this->cleanup_();
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "✅ V4L2 pipeline ready");
  ESP_LOGI(TAG, "   Device: %s", this->video_device_);
  ESP_LOGI(TAG, "   Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGI(TAG, "   Buffers: %d (mmap zero-copy)", VIDEO_BUFFER_COUNT);
  ESP_LOGI(TAG, "   Buffer size: %zu bytes", this->buffer_length_);
  ESP_LOGI(TAG, "   Buffers: %zu/%zu (mmap zero-copy)",
           this->buffer_count_, kVideoBufferCount);
  for (size_t i = 0; i < this->buffer_count_; i++) {
    ESP_LOGI(TAG, "     [%zu] addr=%p size=%zu", i,
             this->mmap_buffers_[i], this->buffer_lengths_[i]);
  }
  ESP_LOGI(TAG, "   Target FPS: %.1f", 1000.0f / this->update_interval_);
#else
  ESP_LOGE(TAG, "❌ V4L2 pipeline requires ESP32-P4");
  this->mark_failed();
#endif
}

#ifdef USE_ESP32_VARIANT_ESP32P4
bool LVGLCameraDisplay::open_video_device_() {
  struct v4l2_capability cap;
  struct v4l2_format fmt;

  // Ouvrir /dev/video0
  this->video_fd_ = open(this->video_device_, O_RDONLY);
  this->video_fd_ = open(this->video_device_, O_RDWR | O_NONBLOCK);
  if (this->video_fd_ < 0) {
    ESP_LOGE(TAG, "Failed to open %s: %d", this->video_device_, errno);
    ESP_LOGE(TAG, "Failed to open %s: %s", this->video_device_, strerror(errno));
    return false;
  }

  // Query capabilities
  if (ioctl(this->video_fd_, VIDIOC_QUERYCAP, &cap) != 0) {
    ESP_LOGE(TAG, "Failed to query capabilities");
    ESP_LOGE(TAG, "Failed to query capabilities: %s", strerror(errno));
    close(this->video_fd_);
    this->video_fd_ = -1;
    return false;
  }

  ESP_LOGI(TAG, "V4L2 driver: %s", cap.driver);
  ESP_LOGI(TAG, "V4L2 card: %s", cap.card);
  ESP_LOGI(TAG, "V4L2 version: %d.%d.%d", 
  ESP_LOGI(TAG, "V4L2 version: %d.%d.%d",
           (cap.version >> 16) & 0xFF,
           (cap.version >> 8) & 0xFF,
           cap.version & 0xFF);

  // Get current format
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->video_fd_, VIDIOC_G_FMT, &fmt) != 0) {
    ESP_LOGE(TAG, "Failed to get format");
    ESP_LOGE(TAG, "Failed to get format: %s", strerror(errno));
    close(this->video_fd_);
    this->video_fd_ = -1;
    return false;
  }

  // Vérifier/ajuster le format
  if (fmt.fmt.pix.width != this->width_ || 
      fmt.fmt.pix.height != this->height_ ||
  if (fmt.fmt.pix.width != this->width_ || fmt.fmt.pix.height != this->height_ ||
      fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
    
    fmt.fmt.pix.width = this->width_;
    fmt.fmt.pix.height = this->height_;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    

    if (ioctl(this->video_fd_, VIDIOC_S_FMT, &fmt) != 0) {
      ESP_LOGE(TAG, "Failed to set format");
      ESP_LOGE(TAG, "Failed to set format: %s", strerror(errno));
      close(this->video_fd_);
      this->video_fd_ = -1;
      return false;
    }
  }

  ESP_LOGI(TAG, "Format: %ux%u RGB565", fmt.fmt.pix.width, fmt.fmt.pix.height);
  
  return true;
}

bool LVGLCameraDisplay::setup_buffers_() {
  // Structures V4L2 simplifiées (compatibles avec videodev2.h fourni)
  struct {
    __u32 count;
    __u32 type;
    __u32 memory;
    __u32 reserved[2];
  } req;
  
  // Demander les buffers
  for (size_t i = 0; i < kVideoBufferCount; i++) {
    this->mmap_buffers_[i] = nullptr;
    this->buffer_lengths_[i] = 0;
  }

  this->buffer_count_ = 0;

  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  req.count = VIDEO_BUFFER_COUNT;
  req.count = kVideoBufferCount;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  
  // VIDIOC_REQBUFS = _IOWR('V', 8, struct v4l2_requestbuffers)
  #define VIDIOC_REQBUFS_LOCAL _IOWR('V', 8, decltype(req))
  
  if (ioctl(this->video_fd_, VIDIOC_REQBUFS_LOCAL, &req) != 0) {
    ESP_LOGE(TAG, "Failed to request buffers: %d", errno);

  if (ioctl(this->video_fd_, VIDIOC_REQBUFS, &req) != 0) {
    ESP_LOGE(TAG, "Failed to request buffers: %s", strerror(errno));
    return false;
  }

  ESP_LOGI(TAG, "Requested %d buffers, got %d", VIDEO_BUFFER_COUNT, req.count);

  // Structure buffer simplifiée
  struct {
    __u32 index;
    __u32 type;
    __u32 bytesused;
    __u32 flags;
    __u32 field;
    struct timeval timestamp;
    struct {
      __u32 offset;
      __u32 length;
    } m;
    __u32 length;
    __u32 reserved2;
    __u32 reserved;
    __u32 memory;
  } buf;
  
  // VIDIOC_QUERYBUF = _IOWR('V', 9, struct v4l2_buffer)
  #define VIDIOC_QUERYBUF_LOCAL _IOWR('V', 9, decltype(buf))
  // VIDIOC_QBUF = _IOWR('V', 15, struct v4l2_buffer)
  #define VIDIOC_QBUF_LOCAL _IOWR('V', 15, decltype(buf))

  // Mapper chaque buffer
  for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
  if (req.count == 0) {
    ESP_LOGE(TAG, "Video device provided zero buffers");
    return false;
  }

  if (req.count < kVideoBufferCount) {
    ESP_LOGW(TAG, "Video device only provided %u buffers (requested %zu)",
             req.count, kVideoBufferCount);
  }

  const size_t requested = std::min<size_t>(req.count, kVideoBufferCount);
  size_t mapped = 0;

  for (size_t i = 0; i < requested; i++) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    
    // Query buffer info
    if (ioctl(this->video_fd_, VIDIOC_QUERYBUF_LOCAL, &buf) != 0) {
      ESP_LOGE(TAG, "Failed to query buffer %d: %d", i, errno);
      return false;
    buf.index = static_cast<__u32>(i);

    if (ioctl(this->video_fd_, VIDIOC_QUERYBUF, &buf) != 0) {
      ESP_LOGE(TAG, "Failed to query buffer %zu: %s", i, strerror(errno));
      break;
    }

    // mmap le buffer (ZERO-COPY!)
    this->mmap_buffers_[i] = (uint8_t*)mmap(
      NULL,
      buf.length,
      PROT_READ | PROT_WRITE,
      MAP_SHARED,
      this->video_fd_,
      buf.m.offset
    );

    if (this->mmap_buffers_[i] == MAP_FAILED) {
      ESP_LOGE(TAG, "Failed to mmap buffer %d: %d", i, errno);
      return false;
    auto *addr = static_cast<uint8_t *>(mmap(nullptr, buf.length,
                                             PROT_READ | PROT_WRITE,
                                             MAP_SHARED, this->video_fd_,
                                             buf.m.offset));
    if (addr == MAP_FAILED) {
      ESP_LOGE(TAG, "Failed to mmap buffer %zu: %s", i, strerror(errno));
      break;
    }

    this->buffer_length_ = buf.length;
    
    ESP_LOGI(TAG, "Buffer %d mapped at %p (size=%zu)", 
             i, this->mmap_buffers_[i], this->buffer_length_);
    this->mmap_buffers_[i] = addr;
    this->buffer_lengths_[i] = buf.length;
    mapped++;

    // Queue le buffer
    if (ioctl(this->video_fd_, VIDIOC_QBUF_LOCAL, &buf) != 0) {
      ESP_LOGE(TAG, "Failed to queue buffer %d: %d", i, errno);
      return false;
    if (!this->queue_buffer_(buf.index)) {
      break;
    }
  }

  if (mapped == 0 || mapped != requested) {
    for (size_t j = 0; j < mapped; j++) {
      munmap(this->mmap_buffers_[j], this->buffer_lengths_[j]);
      this->mmap_buffers_[j] = nullptr;
      this->buffer_lengths_[j] = 0;
    }
    return false;
  }

  this->buffer_count_ = mapped;
  return true;
}

bool LVGLCameraDisplay::start_streaming_() {
  if (this->buffer_count_ == 0) {
    ESP_LOGE(TAG, "Cannot start streaming without mapped buffers");
    return false;
  }

  if (!this->ensure_camera_streaming_()) {
    ESP_LOGE(TAG, "Failed to start underlying camera");
    return false;
  }

  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  
  if (ioctl(this->video_fd_, VIDIOC_STREAMON, &type) != 0) {
    ESP_LOGE(TAG, "Failed to start streaming: %d", errno);
    ESP_LOGE(TAG, "Failed to start streaming: %s", strerror(errno));
    return false;
  }

  this->streaming_ = true;
  ESP_LOGI(TAG, "✅ Streaming started");
  
  return true;
}

bool LVGLCameraDisplay::init_ppa_() {
  ppa_client_config_t ppa_config = {
    .oper_type = PPA_OPERATION_SRM,
    .max_pending_trans_num = 1,
  };
  

  esp_err_t ret = ppa_register_client(&ppa_config, &this->ppa_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA register failed: 0x%x", ret);
    return false;
  }

  uint16_t width = this->width_;
  uint16_t height = this->height_;
  
  // Ajuster dimensions selon rotation

  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    std::swap(width, height);
  }

  this->transform_buffer_size_ = width * height * 2;  // RGB565
  this->transform_buffer_ = (uint8_t*)heap_caps_aligned_alloc(
    64,
    this->transform_buffer_size_,
    MALLOC_CAP_SPIRAM
  );
  this->transform_buffer_ = static_cast<uint8_t *>(heap_caps_aligned_alloc(
    64, this->transform_buffer_size_, MALLOC_CAP_SPIRAM));

  if (!this->transform_buffer_) {
    ESP_LOGE(TAG, "Failed to allocate transform buffer");
    ppa_unregister_client(this->ppa_handle_);
    this->ppa_handle_ = nullptr;
    return false;
  }

  ESP_LOGI(TAG, "PPA transform buffer: %ux%u (%zu bytes)", 
  ESP_LOGI(TAG, "PPA transform buffer: %ux%u (%zu bytes)",
           width, height, this->transform_buffer_size_);
  
  return true;
}

void LVGLCameraDisplay::deinit_ppa_() {
  if (this->transform_buffer_) {
    heap_caps_free(this->transform_buffer_);
    this->transform_buffer_ = nullptr;
  }
  

  if (this->ppa_handle_) {
    ppa_unregister_client(this->ppa_handle_);
    this->ppa_handle_ = nullptr;
  }
}

bool LVGLCameraDisplay::transform_frame_(const uint8_t *src, uint8_t *dst) {
  if (!this->ppa_handle_ || !src || !dst) {
    return false;
  }

  uint16_t src_width = this->width_;
  uint16_t src_height = this->height_;
  uint16_t dst_width = src_width;
  uint16_t dst_height = src_height;

  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    std::swap(dst_width, dst_height);
  }

  // Convertir rotation
  ppa_srm_rotation_angle_t ppa_rotation;
  switch (this->rotation_) {
    case ROTATION_0:   ppa_rotation = PPA_SRM_ROTATION_ANGLE_0; break;
    case ROTATION_90:  ppa_rotation = PPA_SRM_ROTATION_ANGLE_90; break;
    case ROTATION_180: ppa_rotation = PPA_SRM_ROTATION_ANGLE_180; break;
    case ROTATION_270: ppa_rotation = PPA_SRM_ROTATION_ANGLE_270; break;
    default:           ppa_rotation = PPA_SRM_ROTATION_ANGLE_0; break;
  }

  // Configuration PPA
  ppa_srm_oper_config_t srm_config = {
    .in = {
      .buffer = const_cast<uint8_t*>(src),
      .buffer = const_cast<uint8_t *>(src),
      .pic_w = src_width,
      .pic_h = src_height,
      .block_w = src_width,
      .block_h = src_height,
      .block_offset_x = 0,
      .block_offset_y = 0,
      .srm_cm = PPA_SRM_COLOR_MODE_RGB565
      .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
    },
    .out = {
      .buffer = dst,
      .buffer_size = this->transform_buffer_size_,
      .pic_w = dst_width,
      .pic_h = dst_height,
      .block_offset_x = 0,
      .block_offset_y = 0,
      .srm_cm = PPA_SRM_COLOR_MODE_RGB565
      .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
    },
    .rotation_angle = ppa_rotation,
    .scale_x = 1.0f,
    .scale_y = 1.0f,
    .mirror_x = this->mirror_x_,
    .mirror_y = this->mirror_y_,
    .rgb_swap = false,
    .byte_swap = false,
    .mode = PPA_TRANS_MODE_BLOCKING
    .mode = PPA_TRANS_MODE_BLOCKING,
  };

  esp_err_t ret = ppa_do_scale_rotate_mirror(this->ppa_handle_, &srm_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA transformation failed: 0x%x", ret);
    return false;
  }

  return true;
}

void LVGLCameraDisplay::cleanup_() {
  if (this->streaming_) {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(this->video_fd_, VIDIOC_STREAMOFF, &type);
    this->streaming_ = false;
  }

  // Unmapper les buffers
  for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
  for (size_t i = 0; i < this->buffer_count_; i++) {
    if (this->mmap_buffers_[i]) {
      munmap(this->mmap_buffers_[i], this->buffer_length_);
      munmap(this->mmap_buffers_[i], this->buffer_lengths_[i]);
      this->mmap_buffers_[i] = nullptr;
    }
    this->buffer_lengths_[i] = 0;
  }

  for (size_t i = this->buffer_count_; i < kVideoBufferCount; i++) {
    this->mmap_buffers_[i] = nullptr;
    this->buffer_lengths_[i] = 0;
  }

  this->buffer_count_ = 0;
  this->last_display_buffer_ = nullptr;

  if (this->camera_streaming_started_ && this->camera_) {
    this->camera_->stop_streaming();
  }
  this->camera_streaming_started_ = false;

  if (this->video_fd_ >= 0) {
    close(this->video_fd_);
    this->video_fd_ = -1;
  }

  this->deinit_ppa_();
}
#endif

void LVGLCameraDisplay::loop() {
#ifdef USE_ESP32_VARIANT_ESP32P4
  if (!this->streaming_ || !this->canvas_obj_) {
    return;
  }

  uint32_t now = millis();
  
  // Throttle selon update_interval
  if (now - this->last_update_time_ < this->update_interval_) {
    return;
  }

  // Capturer une frame
  if (this->capture_frame_()) {
    this->frame_count_++;
    this->last_update_time_ = now;

    // Logger FPS toutes les 100 frames
    if (this->frame_count_ % 100 == 0 && this->frame_count_ > 0) {
      if (this->last_fps_time_ > 0) {
        float elapsed = (now - this->last_fps_time_) / 1000.0f;
        float fps = 100.0f / elapsed;
        ESP_LOGI(TAG, "🎞️  Display FPS: %.2f | Frames: %u", fps, this->frame_count_);
      }
      this->last_fps_time_ = now;
    }
  }
#endif
}

#ifdef USE_ESP32_VARIANT_ESP32P4
bool LVGLCameraDisplay::capture_frame_() {
  // Structure buffer simplifiée
  struct {
    __u32 index;
    __u32 type;
    __u32 bytesused;
    __u32 flags;
    __u32 field;
    struct timeval timestamp;
    struct {
      __u32 offset;
      __u32 length;
    } m;
    __u32 length;
    __u32 reserved2;
    __u32 reserved;
    __u32 memory;
  } buf;

  // VIDIOC_DQBUF = _IOWR('V', 17, struct v4l2_buffer)
  #define VIDIOC_DQBUF_LOCAL _IOWR('V', 17, decltype(buf))
  // VIDIOC_QBUF = _IOWR('V', 15, struct v4l2_buffer)
  #define VIDIOC_QBUF_LOCAL _IOWR('V', 15, decltype(buf))

  // DQBUF - Récupérer un buffer rempli
  if (!this->streaming_ || this->buffer_count_ == 0) {
    return false;
  }

  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->video_fd_, VIDIOC_DQBUF_LOCAL, &buf) != 0) {
    ESP_LOGE(TAG, "Failed to dequeue buffer: %d", errno);
  if (ioctl(this->video_fd_, VIDIOC_DQBUF, &buf) != 0) {
    if (errno == EAGAIN || errno == EINTR) {
      return false;
    }

    ESP_LOGE(TAG, "Failed to dequeue buffer: %s", strerror(errno));
    return false;
  }

  if (buf.index >= this->buffer_count_) {
    ESP_LOGE(TAG, "Dequeued buffer index %u exceeds mapped buffer count %zu",
             buf.index, this->buffer_count_);
    return false;
  }

  // Pointer vers les données (zero-copy!)
  uint8_t *frame_data = this->mmap_buffers_[buf.index];
  uint16_t width = this->width_;
  uint16_t height = this->height_;

  if (this->first_update_) {
    ESP_LOGI(TAG, "🖼️  First frame:");
    ESP_LOGI(TAG, "   Buffer index: %d", buf.index);
    ESP_LOGI(TAG, "   Buffer index: %u", buf.index);
    ESP_LOGI(TAG, "   Buffer address: %p", frame_data);
    ESP_LOGI(TAG, "   Bytes used: %u", buf.bytesused);
    this->first_update_ = false;
  }

  // Transformation PPA si nécessaire
  if (this->ppa_handle_ && this->transform_buffer_) {
    if (this->transform_frame_(frame_data, this->transform_buffer_)) {
      frame_data = this->transform_buffer_;
      

      if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
        std::swap(width, height);
      }
    }
  }

  // Mettre à jour LVGL
  if (this->last_display_buffer_ != frame_data) {
    #if LV_VERSION_CHECK(9, 0, 0)
      lv_canvas_set_buffer(this->canvas_obj_, frame_data, width, height, 
                           LV_COLOR_FORMAT_RGB565);
    #else
      lv_canvas_set_buffer(this->canvas_obj_, frame_data, width, height, 
                           LV_IMG_CF_TRUE_COLOR);
    #endif
#if defined(LV_VERSION_CHECK)
#if LV_VERSION_CHECK(9, 0, 0)
    lv_canvas_set_buffer(this->canvas_obj_, frame_data, width, height,
                         LV_COLOR_FORMAT_RGB565);
#else
    lv_canvas_set_buffer(this->canvas_obj_, frame_data, width, height,
                         LV_IMG_CF_TRUE_COLOR);
#endif
#else
    lv_canvas_set_buffer(this->canvas_obj_, frame_data, width, height,
                         LV_IMG_CF_TRUE_COLOR);
#endif

    this->last_display_buffer_ = frame_data;
    lv_obj_invalidate(this->canvas_obj_);
  }

  // QBUF - Remettre le buffer disponible
  if (ioctl(this->video_fd_, VIDIOC_QBUF_LOCAL, &buf) != 0) {
    ESP_LOGE(TAG, "Failed to queue buffer: %d", errno);
  if (!this->queue_buffer_(buf.index)) {
    return false;
  }

  return true;
}

bool LVGLCameraDisplay::queue_buffer_(uint32_t index) {
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = index;

  if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) != 0) {
    ESP_LOGE(TAG, "Failed to queue buffer %u: %s", index, strerror(errno));
    return false;
  }

  return true;
}

bool LVGLCameraDisplay::ensure_camera_streaming_() {
  if (!this->camera_) {
    return true;
  }

  if (this->camera_->is_streaming()) {
    return true;
  }

  if (!this->camera_->start_streaming()) {
    ESP_LOGE(TAG, "Camera start_streaming() returned false");
    return false;
  }

  this->camera_streaming_started_ = true;
  return true;
}
#endif

void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "LVGL Camera Display:");
#ifdef USE_ESP32_VARIANT_ESP32P4
  ESP_LOGCONFIG(TAG, "  Mode: V4L2 Pipeline (ESP32-P4)");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->video_device_);
  ESP_LOGCONFIG(TAG, "  Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGCONFIG(TAG, "  Buffers: %d (mmap zero-copy)", VIDEO_BUFFER_COUNT);
  ESP_LOGCONFIG(TAG, "  Buffers: %zu/%zu (mmap zero-copy)",
                this->buffer_count_, kVideoBufferCount);
  ESP_LOGCONFIG(TAG, "  PPA: %s", this->ppa_handle_ ? "ENABLED" : "DISABLED");
#else
  ESP_LOGCONFIG(TAG, "  Mode: NOT SUPPORTED (ESP32-P4 required)");
#endif
  ESP_LOGCONFIG(TAG, "  Canvas: %s", this->canvas_obj_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Rotation: %d°", this->rotation_);
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", this->mirror_x_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", this->mirror_y_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Update interval: %u ms", this->update_interval_);
  

  if (this->camera_) {
    ESP_LOGCONFIG(TAG, "  Camera linked: YES (%ux%u)", 
                  this->camera_->get_image_width(), 
    ESP_LOGCONFIG(TAG, "  Camera linked: YES (%ux%u)",
                  this->camera_->get_image_width(),
                  this->camera_->get_image_height());
  } else {
    ESP_LOGCONFIG(TAG, "  Camera linked: NO");
  }
}

void LVGLCameraDisplay::configure_canvas(lv_obj_t *canvas) {
  this->canvas_obj_ = canvas;
  ESP_LOGI(TAG, "🎨 Canvas configured: %p", canvas);

  if (canvas != nullptr) {
    lv_coord_t w = lv_obj_get_width(canvas);
    lv_coord_t h = lv_obj_get_height(canvas);
    ESP_LOGI(TAG, "   Canvas size: %dx%d", w, h);
    
    // Optimisations LVGL

    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_TRANSP, 0);
  }
}

}  // namespace lvgl_camera_display
}  // namespace esphome
