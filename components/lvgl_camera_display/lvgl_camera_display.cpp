#include "lvgl_camera_display.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <cstring>
#include <cerrno>
#include <sys/stat.h>  // pour stat() dans l’attente active

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

// =======================================================================
// Utils internes
// =======================================================================
static inline bool is_rotation_90_or_270(LVGLCameraDisplay::Rotation r) {
  return (r == LVGLCameraDisplay::ROTATION_90 || r == LVGLCameraDisplay::ROTATION_270);
}

// =======================================================================
// Setup principal
// =======================================================================
void LVGLCameraDisplay::setup() {
  ESP_LOGCONFIG(TAG, "🎥 LVGL Camera Display (V4L2 Pipeline)");

  // Prendre la résolution depuis la caméra si elle est liée (indicatif)
  if (this->camera_) {
    this->width_  = this->camera_->get_image_width();
    this->height_ = this->camera_->get_image_height();
    ESP_LOGI(TAG, "📐 Using camera resolution: %ux%u (from sensor)", this->width_, this->height_);
  } else {
    ESP_LOGW(TAG, "⚠️  No camera linked, using current defaults %ux%u", this->width_, this->height_);
  }

  // --- Attente active pour /dev/video0, créé par mipi_dsi_cam (enable_v4l2: true) ---
  const char *dev_path = ESP_VIDEO_MIPI_CSI_DEVICE_NAME; // "/dev/video0"
  struct stat st {};
  const int timeout_ms = 3000;  // max 3 secondes
  int waited_ms = 0;

  while (waited_ms < timeout_ms) {
    if (stat(dev_path, &st) == 0) {
      ESP_LOGI(TAG, "✅ %s detected after %d ms", dev_path, waited_ms);
      break;
    }
    delay(100);
    waited_ms += 100;
  }

  if (waited_ms >= timeout_ms) {
    ESP_LOGE(TAG, "❌ %s not found after %d ms", dev_path, timeout_ms);
    this->mark_failed();
    return;
  }

  // Ouvrir le device vidéo
  if (!this->open_video_device_()) {
    ESP_LOGE(TAG, "❌ Failed to open video device");
    this->mark_failed();
    return;
  }

  // Configurer/valider format & récupérer résolution effective depuis le driver
  if (!this->negotiate_format_()) {
    ESP_LOGE(TAG, "❌ Failed to negotiate video format");
    this->cleanup_();
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

  // Initialiser PPA si transformations demandées
  if (this->rotation_ != ROTATION_0 || this->mirror_x_ || this->mirror_y_) {
    if (!this->init_ppa_()) {
      ESP_LOGE(TAG, "❌ Failed to initialize PPA");
      this->cleanup_();
      this->mark_failed();
      return;
    }
    ESP_LOGI(TAG, "✅ PPA initialized (rotation=%d°, mirror_x=%s, mirror_y=%s)",
             (int) this->rotation_,
             this->mirror_x_ ? "ON" : "OFF",
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
  ESP_LOGI(TAG, "   Resolution (from driver): %ux%u", this->width_, this->height_);
  ESP_LOGI(TAG, "   Buffers: %d (mmap zero-copy)", VIDEO_BUFFER_COUNT);
  ESP_LOGI(TAG, "   Buffer size: %zu bytes", this->buffer_length_);
  ESP_LOGI(TAG, "   Target FPS: %.1f", 1000.0f / this->update_interval_);
}

// =======================================================================
// Ouverture / format
// =======================================================================
bool LVGLCameraDisplay::open_video_device_() {
  this->video_fd_ = open(this->video_device_, O_RDWR);
  if (this->video_fd_ < 0) {
    ESP_LOGE(TAG, "Failed to open %s: errno=%d", this->video_device_, errno);
    return false;
  }

  struct v4l2_capability cap {};
  if (ioctl(this->video_fd_, VIDIOC_QUERYCAP, &cap) != 0) {
    ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed (errno=%d)", errno);
    close(this->video_fd_);
    this->video_fd_ = -1;
    return false;
  }

  ESP_LOGI(TAG, "V4L2 driver: %s | card: %s | bus: %s", cap.driver, cap.card, cap.bus_info);
  return true;
}

bool LVGLCameraDisplay::negotiate_format_() {
  struct v4l2_format fmt {};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->video_fd_, VIDIOC_G_FMT, &fmt) != 0) {
    ESP_LOGE(TAG, "VIDIOC_G_FMT failed (errno=%d)", errno);
    return false;
  }

  // Si pas RGB565, tenter de forcer RGB565
  if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
    ESP_LOGW(TAG, "Pixel format is 0x%08X, switching to RGB565", fmt.fmt.pix.pixelformat);
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    if (ioctl(this->video_fd_, VIDIOC_S_FMT, &fmt) != 0) {
      ESP_LOGE(TAG, "VIDIOC_S_FMT failed to set RGB565 (errno=%d)", errno);
      return false;
    }
    // Relire pour confirmer le format final
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(this->video_fd_, VIDIOC_G_FMT, &fmt) != 0) {
      ESP_LOGE(TAG, "VIDIOC_G_FMT (re-read) failed (errno=%d)", errno);
      return false;
    }
  }

  // Mettre à jour la résolution effective depuis le driver
  this->width_  = fmt.fmt.pix.width;
  this->height_ = fmt.fmt.pix.height;

  ESP_LOGI(TAG, "Format confirmed: %ux%u RGB565 (bytesperline=%u sizeimage=%u)",
           fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.bytesperline, fmt.fmt.pix.sizeimage);

  return true;
}

// =======================================================================
// Configuration des buffers mmap
// =======================================================================
bool LVGLCameraDisplay::setup_buffers_() {
  struct v4l2_requestbuffers req {};
  req.count  = VIDEO_BUFFER_COUNT;
  req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->video_fd_, VIDIOC_REQBUFS, &req) != 0) {
    ESP_LOGE(TAG, "VIDIOC_REQBUFS failed (errno=%d)", errno);
    return false;
  }

  if (req.count < VIDEO_BUFFER_COUNT) {
    ESP_LOGW(TAG, "Driver returned only %u buffers (requested %u)", req.count, (unsigned) VIDEO_BUFFER_COUNT);
  }

  // Mapper chaque buffer
  for (int i = 0; i < (int) req.count; i++) {
    struct v4l2_buffer buf {};
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = i;

    if (ioctl(this->video_fd_, VIDIOC_QUERYBUF, &buf) != 0) {
      ESP_LOGE(TAG, "VIDIOC_QUERYBUF[%d] failed (errno=%d)", i, errno);
      return false;
    }

    this->mmap_buffers_[i] = (uint8_t *) mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                              this->video_fd_, buf.m.offset);
    if (this->mmap_buffers_[i] == MAP_FAILED) {
      ESP_LOGE(TAG, "mmap[%d] failed (errno=%d)", i, errno);
      this->mmap_buffers_[i] = nullptr;
      return false;
    }

    this->buffer_length_ = buf.length;
    ESP_LOGI(TAG, "Buffer %d mapped at %p (size=%zu)", i, this->mmap_buffers_[i], this->buffer_length_);

    if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) != 0) {
      ESP_LOGE(TAG, "VIDIOC_QBUF[%d] failed (errno=%d)", i, errno);
      return false;
    }
  }

  return true;
}

// =======================================================================
// PPA (rotation / miroir)
// =======================================================================
bool LVGLCameraDisplay::init_ppa_() {
  ppa_client_config_t ppa_config = {
    .oper_type = PPA_OPERATION_SRM,
    .max_pending_trans_num = 1,
  };

  esp_err_t ret = ppa_register_client(&ppa_config, &this->ppa_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA register failed: 0x%x", ret);
    this->ppa_handle_ = nullptr;
    return false;
  }

  uint16_t out_w = this->width_;
  uint16_t out_h = this->height_;

  if (is_rotation_90_or_270(this->rotation_)) {
    std::swap(out_w, out_h);
  }

  this->transform_buffer_size_ = (size_t) out_w * out_h * 2; // RGB565
  this->transform_buffer_ = (uint8_t *) heap_caps_aligned_alloc(64, this->transform_buffer_size_, MALLOC_CAP_SPIRAM);
  if (!this->transform_buffer_) {
    ESP_LOGE(TAG, "Failed to allocate transform buffer (%zu bytes)", this->transform_buffer_size_);
    ppa_unregister_client(this->ppa_handle_);
    this->ppa_handle_ = nullptr;
    return false;
  }

  ESP_LOGI(TAG, "PPA transform buffer: %ux%u (%zu bytes)", out_w, out_h, this->transform_buffer_size_);
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
  if (!this->ppa_handle_ || !src || !dst) return false;

  uint16_t src_w = this->width_;
  uint16_t src_h = this->height_;
  uint16_t dst_w = src_w;
  uint16_t dst_h = src_h;

  if (is_rotation_90_or_270(this->rotation_)) {
    std::swap(dst_w, dst_h);
  }

  ppa_srm_rotation_angle_t angle = PPA_SRM_ROTATION_ANGLE_0;
  switch (this->rotation_) {
    case ROTATION_90:  angle = PPA_SRM_ROTATION_ANGLE_90;  break;
    case ROTATION_180: angle = PPA_SRM_ROTATION_ANGLE_180; break;
    case ROTATION_270: angle = PPA_SRM_ROTATION_ANGLE_270; break;
    default:           angle = PPA_SRM_ROTATION_ANGLE_0;   break;
  }

  ppa_srm_oper_config_t srm_cfg = {
    .in = {
      .buffer         = const_cast<uint8_t *>(src),
      .pic_w          = src_w,
      .pic_h          = src_h,
      .block_w        = src_w,
      .block_h        = src_h,
      .block_offset_x = 0,
      .block_offset_y = 0,
      .srm_cm         = PPA_SRM_COLOR_MODE_RGB565
    },
    .out = {
      .buffer         = dst,
      .buffer_size    = (uint32_t) this->transform_buffer_size_,
      .pic_w          = dst_w,
      .pic_h          = dst_h,
      .block_offset_x = 0,
      .block_offset_y = 0,
      .srm_cm         = PPA_SRM_COLOR_MODE_RGB565
    },
    .rotation_angle = angle,
    .scale_x        = 1.0f,
    .scale_y        = 1.0f,
    .mirror_x       = this->mirror_x_,
    .mirror_y       = this->mirror_y_,
    .rgb_swap       = false,
    .byte_swap      = false,
    .mode           = PPA_TRANS_MODE_BLOCKING
  };

  esp_err_t ret = ppa_do_scale_rotate_mirror(this->ppa_handle_, &srm_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA transformation failed: 0x%x", ret);
    return false;
  }
  return true;
}

// =======================================================================
// Démarrage / arrêt du streaming
// =======================================================================
bool LVGLCameraDisplay::start_streaming_() {
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->video_fd_, VIDIOC_STREAMON, &type) != 0) {
    ESP_LOGE(TAG, "VIDIOC_STREAMON failed (errno=%d)", errno);
    return false;
  }
  this->streaming_ = true;
  ESP_LOGI(TAG, "📸 Streaming started");
  return true;
}

// =======================================================================
// Capture d'une image et affichage sur LVGL (avec PPA si demandé)
// =======================================================================
bool LVGLCameraDisplay::capture_frame_() {
  if (!this->streaming_ || !this->canvas_obj_) return true; // rien à faire mais pas une erreur bloquante

  // Throttle via update_interval_
  uint32_t now = millis();
  if (now - this->last_update_time_ < this->update_interval_) {
    return true;
  }

  struct v4l2_buffer buf {};
  buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->video_fd_, VIDIOC_DQBUF, &buf) != 0) {
    ESP_LOGE(TAG, "VIDIOC_DQBUF failed (errno=%d)", errno);
    return false;
  }

  uint8_t *frame_ptr = this->mmap_buffers_[buf.index];
  uint16_t out_w = this->width_;
  uint16_t out_h = this->height_;

  // PPA si rotation/mirror
  if ((this->rotation_ != ROTATION_0 || this->mirror_x_ || this->mirror_y_) &&
      this->ppa_handle_ && this->transform_buffer_) {

    if (!this->transform_frame_(frame_ptr, this->transform_buffer_)) {
      // Si PPA échoue, on tombe back au buffer brut
      ESP_LOGW(TAG, "PPA failed; fallback to direct frame");
    } else {
      frame_ptr = this->transform_buffer_;
      if (is_rotation_90_or_270(this->rotation_)) {
        std::swap(out_w, out_h);
      }
    }
  }

  // Mettre à jour LVGL canvas (buffer pointant sur frame_ptr)
#if LV_VERSION_CHECK(9, 0, 0)
  lv_canvas_set_buffer(this->canvas_obj_, frame_ptr, out_w, out_h, LV_COLOR_FORMAT_RGB565);
#else
  lv_canvas_set_buffer(this->canvas_obj_, frame_ptr, out_w, out_h, LV_IMG_CF_TRUE_COLOR);
#endif
  lv_obj_invalidate(this->canvas_obj_);

  // Re-queue
  if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) != 0) {
    ESP_LOGE(TAG, "VIDIOC_QBUF failed (errno=%d)", errno);
    return false;
  }

  this->last_update_time_ = now;
  this->frame_count_++;

  // Log FPS toutes les 100 frames
  if (this->frame_count_ % 100 == 0) {
    if (this->last_fps_time_ > 0) {
      float elapsed = (now - this->last_fps_time_) / 1000.0f;
      float fps = 100.0f / (elapsed > 0 ? elapsed : 1.0f);
      ESP_LOGI(TAG, "🎞️  Display FPS: %.2f | Frames: %u", fps, this->frame_count_);
    }
    this->last_fps_time_ = now;
  }

  return true;
}

// =======================================================================
// Nettoyage et fermeture
// =======================================================================
void LVGLCameraDisplay::cleanup_() {
  if (this->streaming_) {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(this->video_fd_, VIDIOC_STREAMOFF, &type);
    this->streaming_ = false;
  }

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
}

// =======================================================================
// Boucle principale
// =======================================================================
void LVGLCameraDisplay::loop() {
  (void) this->capture_frame_();  // on continue même si une frame rate
}

// =======================================================================
// Dump config
// =======================================================================
void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "LVGL Camera Display:");
  ESP_LOGCONFIG(TAG, "  Mode: V4L2 Pipeline (ESP32-P4)");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->video_device_);
  ESP_LOGCONFIG(TAG, "  Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGCONFIG(TAG, "  Buffers: %d (mmap zero-copy)", VIDEO_BUFFER_COUNT);
  ESP_LOGCONFIG(TAG, "  PPA: %s", this->ppa_handle_ ? "ENABLED" : "DISABLED");
  ESP_LOGCONFIG(TAG, "  Canvas: %s", this->canvas_obj_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Rotation: %d°", (int) this->rotation_);
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", this->mirror_x_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", this->mirror_y_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Update interval: %u ms", this->update_interval_);
  if (this->camera_) {
    ESP_LOGCONFIG(TAG, "  Camera linked: YES (%ux%u)",
                  this->camera_->get_image_width(), this->camera_->get_image_height());
  } else {
    ESP_LOGCONFIG(TAG, "  Camera linked: NO");
  }
}

// =======================================================================
// Association du canvas LVGL
// =======================================================================
void LVGLCameraDisplay::configure_canvas(lv_obj_t *canvas) {
  this->canvas_obj_ = canvas;
  ESP_LOGI(TAG, "🎨 Canvas configured: %p", canvas);

  if (canvas != nullptr) {
    lv_coord_t w = lv_obj_get_width(canvas);
    lv_coord_t h = lv_obj_get_height(canvas);
    ESP_LOGI(TAG, "   Canvas size: %dx%d", (int) w, (int) h);

    // Optimisations LVGL
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_TRANSP, 0);
  }
}

}  // namespace lvgl_camera_display
}  // namespace esphome

#else
// Plateforme non supportée
#include "esphome/core/log.h"
namespace esphome { namespace lvgl_camera_display {
static const char *const TAG = "lvgl_camera_display";
void LVGLCameraDisplay::setup(){ ESP_LOGE(TAG, "❌ V4L2 pipeline requires ESP32-P4"); this->mark_failed(); }
void LVGLCameraDisplay::loop() {}
void LVGLCameraDisplay::dump_config(){ ESP_LOGCONFIG(TAG, "  Mode: NOT SUPPORTED (ESP32-P4 required)"); }
void LVGLCameraDisplay::configure_canvas(lv_obj_t*) {}
}}
#endif








