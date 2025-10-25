#include "lvgl_camera_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <algorithm>
#include <cstring>
#include <cerrno>

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

void LVGLCameraDisplay::setup() {
  ESP_LOGCONFIG(TAG, "🎥 LVGL Camera Display (V4L2 Pipeline)");

#ifdef USE_ESP32_VARIANT_ESP32P4
  // Récupérer la résolution depuis la caméra
  if (this->camera_) {
    this->width_ = this->camera_->get_image_width();
    this->height_ = this->camera_->get_image_height();
    ESP_LOGI(TAG, "📐 Using camera resolution: %ux%u", this->width_, this->height_);
  } else {
    ESP_LOGW(TAG, "⚠️  No camera linked, using default resolution %ux%u", 
    ESP_LOGW(TAG, "⚠️  No camera linked, using default resolution %ux%u",
             this->width_, this->height_);
  }

  uint16_t display_width = this->width_;
  uint16_t display_height = this->height_;
  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    std::swap(display_width, display_height);
  }

  if (!this->allocate_display_buffer_(display_width, display_height)) {
    ESP_LOGE(TAG, "❌ Failed to allocate LVGL display buffer");
    this->status_set_error("failed to allocate LVGL display buffer");
    this->mark_failed();
    this->cleanup_();
    return;
  }

  // Ouvrir le device vidéo
  if (!this->open_video_device_()) {
    ESP_LOGE(TAG, "❌ Failed to open video device");
    this->status_set_error("failed to open video device");
    this->mark_failed();
    this->cleanup_();
    return;
  }

  // Setup des buffers mmap
  if (!this->setup_buffers_()) {
    ESP_LOGE(TAG, "❌ Failed to setup buffers");
    this->status_set_error("failed to setup video buffers");
    this->mark_failed();
    this->cleanup_();
    return;
  }

  // Initialiser PPA si transformations
  if (this->rotation_ != ROTATION_0 || this->mirror_x_ || this->mirror_y_) {
    if (!this->init_ppa_()) {
      ESP_LOGE(TAG, "❌ Failed to initialize PPA");
      this->status_set_error("failed to initialize PPA");
      this->mark_failed();
      this->cleanup_();
      return;
    }
    ESP_LOGI(TAG, "✅ PPA initialized (rotation=%d°, mirror_x=%s, mirror_y=%s)",
             this->rotation_, this->mirror_x_ ? "ON" : "OFF", 
             this->mirror_y_ ? "ON" : "OFF");
  }

  // Démarrer le streaming
  if (!this->start_streaming_()) {
    ESP_LOGE(TAG, "❌ Failed to start streaming");
    this->status_set_error("failed to start video streaming");
    this->mark_failed();
    this->cleanup_();
    return;
  }

  ESP_LOGI(TAG, "✅ V4L2 pipeline ready");
  ESP_LOGI(TAG, "   Device: %s", this->video_device_);
  ESP_LOGI(TAG, "   Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGI(TAG, "   Buffers: %d (mmap zero-copy)", VIDEO_BUFFER_COUNT);
  ESP_LOGI(TAG, "   Buffer size: %zu bytes", this->buffer_length_);
  ESP_LOGI(TAG, "   Target FPS: %.1f", 1000.0f / this->update_interval_);
#else
  ESP_LOGE(TAG, "❌ V4L2 pipeline requires ESP32-P4");
  this->status_set_error("unsupported platform – ESP32-P4 required");
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
    return false;
  }

  // Query capabilities
  if (ioctl(this->video_fd_, VIDIOC_QUERYCAP, &cap) != 0) {
    ESP_LOGE(TAG, "Failed to query capabilities");
    close(this->video_fd_);
    return false;
  }

  ESP_LOGI(TAG, "V4L2 driver: %s", cap.driver);
  ESP_LOGI(TAG, "V4L2 card: %s", cap.card);
  ESP_LOGI(TAG, "V4L2 version: %d.%d.%d", 
           (cap.version >> 16) & 0xFF,
           (cap.version >> 8) & 0xFF,
           cap.version & 0xFF);

  // Get current format
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->video_fd_, VIDIOC_G_FMT, &fmt) != 0) {
    ESP_LOGE(TAG, "Failed to get format");
    close(this->video_fd_);
@@ -342,50 +369,51 @@ bool LVGLCameraDisplay::transform_frame_(const uint8_t *src, uint8_t *dst) {

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
    if (this->mmap_buffers_[i]) {
      munmap(this->mmap_buffers_[i], this->buffer_length_);
      this->mmap_buffers_[i] = nullptr;
    }
  }

  if (this->video_fd_ >= 0) {
    close(this->video_fd_);
    this->video_fd_ = -1;
  }

  this->deinit_ppa_();
  this->free_display_buffer_();
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
@@ -407,128 +435,222 @@ bool LVGLCameraDisplay::capture_frame_() {
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
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->video_fd_, VIDIOC_DQBUF_LOCAL, &buf) != 0) {
    if (errno == EAGAIN) {
      return false;
    }
    ESP_LOGE(TAG, "Failed to dequeue buffer: %d", errno);
    return false;
  }

  // Pointer vers les données (zero-copy!)
  uint8_t *frame_data = this->mmap_buffers_[buf.index];
  uint16_t width = this->width_;
  uint16_t height = this->height_;

  if (this->first_update_) {
    ESP_LOGI(TAG, "🖼️  First frame:");
    ESP_LOGI(TAG, "   Buffer index: %d", buf.index);
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
  // S'assurer que le cache CPU voit les nouvelles données de la caméra
  size_t sync_length = frame_data == this->transform_buffer_ ? this->transform_buffer_size_ : this->buffer_length_;
  size_t frame_bytes = buf.bytesused > 0 ? buf.bytesused : static_cast<size_t>(width) * height * 2;
  if (sync_length > 0 && frame_bytes > 0) {
    frame_bytes = std::min(frame_bytes, sync_length);
    esp_cache_msync(frame_data, frame_bytes, ESP_CACHE_MSYNC_FLAG_INVALIDATE);
  }

  if (!this->canvas_buffer_ready_) {
    this->configure_canvas_buffer_();
  }

    this->last_display_buffer_ = frame_data;
    lv_obj_invalidate(this->canvas_obj_);
  if (!this->allocate_display_buffer_(width, height)) {
    if (!this->display_buffer_warning_logged_) {
      ESP_LOGW(TAG, "Skipping frame copy - display buffer unavailable");
      this->display_buffer_warning_logged_ = true;
    }
  } else {
    this->display_buffer_warning_logged_ = false;
    size_t copy_bytes = std::min(frame_bytes, this->display_buffer_size_);
    if (copy_bytes > 0) {
      memcpy(this->display_buffer_, frame_data, copy_bytes);
      lv_obj_invalidate(this->canvas_obj_);
    }
  }

  // QBUF - Remettre le buffer disponible
  if (ioctl(this->video_fd_, VIDIOC_QBUF_LOCAL, &buf) != 0) {
    ESP_LOGE(TAG, "Failed to queue buffer: %d", errno);
    return false;
  }

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
  ESP_LOGCONFIG(TAG, "  PPA: %s", this->ppa_handle_ ? "ENABLED" : "DISABLED");
  ESP_LOGCONFIG(TAG, "  Display buffer: %ux%u (%zu bytes)",
                this->display_width_, this->display_height_, this->display_buffer_size_);
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

#ifdef USE_ESP32_VARIANT_ESP32P4
    this->configure_canvas_buffer_();
#endif
  }
}

#ifdef USE_ESP32_VARIANT_ESP32P4
bool LVGLCameraDisplay::allocate_display_buffer_(uint16_t width, uint16_t height) {
  if (width == 0 || height == 0) {
    return false;
  }

  size_t required = static_cast<size_t>(width) * static_cast<size_t>(height) * 2;

  if (this->display_buffer_ != nullptr && required == this->display_buffer_size_) {
    if (this->display_width_ != width || this->display_height_ != height) {
      this->display_width_ = width;
      this->display_height_ = height;
      this->configure_canvas_buffer_();
    }
    return true;
  }

  this->free_display_buffer_();

  this->display_buffer_ = static_cast<uint8_t *>(heap_caps_aligned_alloc(64, required, MALLOC_CAP_SPIRAM));
  if (this->display_buffer_ == nullptr) {
    this->display_buffer_ = static_cast<uint8_t *>(heap_caps_aligned_alloc(64, required, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }

  if (this->display_buffer_ == nullptr) {
    this->display_buffer_size_ = 0;
    this->display_width_ = 0;
    this->display_height_ = 0;
    ESP_LOGE(TAG, "Failed to allocate %zu bytes for LVGL display buffer", required);
    return false;
  }

  this->display_buffer_size_ = required;
  this->display_width_ = width;
  this->display_height_ = height;
  memset(this->display_buffer_, 0, this->display_buffer_size_);
  this->configure_canvas_buffer_();
  return true;
}

void LVGLCameraDisplay::free_display_buffer_() {
  if (this->display_buffer_ != nullptr) {
    heap_caps_free(this->display_buffer_);
    this->display_buffer_ = nullptr;
  }
  this->display_buffer_size_ = 0;
  this->display_width_ = 0;
  this->display_height_ = 0;
  this->canvas_buffer_ready_ = false;
  this->display_buffer_warning_logged_ = false;
}

void LVGLCameraDisplay::configure_canvas_buffer_() {
  if (!this->canvas_obj_ || !this->display_buffer_ || this->display_width_ == 0 || this->display_height_ == 0) {
    this->canvas_buffer_ready_ = false;
    return;
  }

#if LV_VERSION_CHECK(9, 0, 0)
  lv_canvas_set_buffer(this->canvas_obj_, this->display_buffer_, this->display_width_, this->display_height_,
                       LV_COLOR_FORMAT_RGB565);
#else
  lv_canvas_set_buffer(this->canvas_obj_, this->display_buffer_, this->display_width_, this->display_height_,
                       LV_IMG_CF_TRUE_COLOR);
#endif

  lv_obj_set_size(this->canvas_obj_, this->display_width_, this->display_height_);
  ESP_LOGI(TAG, "   Canvas ready: %ux%u buffer @ %p", this->display_width_, this->display_height_, this->display_buffer_);
  this->canvas_buffer_ready_ = true;
  lv_obj_invalidate(this->canvas_obj_);
}
#endif

}  // namespace lvgl_camera_display
}  // namespace esphome









