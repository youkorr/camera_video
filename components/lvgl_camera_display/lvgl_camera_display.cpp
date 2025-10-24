#include "lvgl_camera_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

void LVGLCameraDisplay::setup() {
  ESP_LOGCONFIG(TAG, "🎥 LVGL Camera Display (V4L2 Pipeline)");

#ifdef USE_ESP32_VARIANT_ESP32P4
  // Récupérer la résolution depuis la caméra si dispo
  if (this->camera_) {
    this->width_  = this->camera_->get_image_width();
    this->height_ = this->camera_->get_image_height();
    ESP_LOGI(TAG, "📐 Using camera resolution: %ux%u", this->width_, this->height_);
  } else {
    ESP_LOGW(TAG, "⚠️  No camera linked, using default resolution %ux%u", this->width_, this->height_);
  }

  // 1) Initialiser la "stack vidéo" (crée le device virtuel /dev/video0)
  if (!this->init_video_stack_()) {
    ESP_LOGE(TAG, "❌ esp_video_init() failed (video stack not ready)");
    this->mark_failed();
    return;
  }

  // 2) Ouvrir le device et configurer format si besoin
  if (!this->open_video_device_()) {
    ESP_LOGE(TAG, "❌ Failed to open video device");
    this->mark_failed();
    return;
  }

  // 3) Préparer les buffers mmap (REQBUFS/QUERYBUF/mmap/QBUF)
  if (!this->setup_buffers_()) {
    ESP_LOGE(TAG, "❌ Failed to setup buffers");
    this->mark_failed();
    return;
  }

  // 4) Initialiser PPA si transformations
  if (this->rotation_ != ROTATION_0 || this->mirror_x_ || this->mirror_y_) {
    if (!this->init_ppa_()) {
      ESP_LOGE(TAG, "❌ Failed to initialize PPA");
      this->mark_failed();
      return;
    }
    ESP_LOGI(TAG, "✅ PPA initialized (rotation=%d°, mirror_x=%s, mirror_y=%s)",
             this->rotation_, this->mirror_x_ ? "ON" : "OFF",
             this->mirror_y_ ? "ON" : "OFF");
  }

  // 5) Démarrer le streaming
  if (!this->start_streaming_()) {
    ESP_LOGE(TAG, "❌ Failed to start streaming");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "✅ V4L2 pipeline ready");
  ESP_LOGI(TAG, "   Device: %s", this->video_device_path_);
  ESP_LOGI(TAG, "   Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGI(TAG, "   Buffers: %d (mmap zero-copy)", VIDEO_BUFFER_COUNT);
  ESP_LOGI(TAG, "   Buffer size: %zu bytes", this->buffer_length_);
  ESP_LOGI(TAG, "   Target FPS: %.1f", 1000.0f / this->update_interval_);

#else
  ESP_LOGE(TAG, "❌ V4L2 pipeline requires ESP32-P4");
  this->mark_failed();
#endif
}

void LVGLCameraDisplay::loop() {
#ifdef USE_ESP32_VARIANT_ESP32P4
  if (!this->streaming_ || !this->canvas_obj_) {
    return;
  }

  const uint32_t now = millis();
  if (now - this->last_update_time_ < this->update_interval_) {
    return;
  }

  if (this->capture_frame_()) {
    this->frame_count_++;
    this->last_update_time_ = now;

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

void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "LVGL Camera Display:");
#ifdef USE_ESP32_VARIANT_ESP32P4
  ESP_LOGCONFIG(TAG, "  Mode: V4L2 Pipeline (ESP32-P4)");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->video_device_path_);
  ESP_LOGCONFIG(TAG, "  Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGCONFIG(TAG, "  Buffers: %d (mmap zero-copy)", VIDEO_BUFFER_COUNT);
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

#ifdef USE_ESP32_VARIANT_ESP32P4
bool LVGLCameraDisplay::init_video_stack_() {
  // Appel inspiré de la démo M5Stack : on initialise le sous-système vidéo
  // Si tu as un handle I2C déjà prêt, tu peux l’injecter ici (init_sccb=false + i2c_handle).
  static esp_video_init_csi_config_t csi_cfg = {
      .sccb_config = {
          .init_sccb = false,
          .i2c_handle = NULL,  // si tu as un handle I2C existant, affecte-le ici
          .freq = 400000,
      },
      .reset_pin = -1,
      .pwdn_pin  = -1,
  };

  esp_video_init_config_t init_cfg = {
      .csi  = &csi_cfg,
      .dvp  = NULL,
      .jpeg = NULL,
      .isp  = NULL,
  };

  ESP_LOGI(TAG, "🚀 esp_video_init(): creating virtual V4L2 device (%s)", ESP_VIDEO_MIPI_CSI_DEVICE_NAME);
  esp_err_t err = esp_video_init(&init_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_video_init() failed: 0x%x", err);
    return false;
  }
  return true;
}

bool LVGLCameraDisplay::open_video_device_() {
  struct v4l2_capability cap;
  struct v4l2_format fmt;

  // Ouvrir le device virtuel créé par esp_video_init()
  this->video_fd_ = open(this->video_device_path_, O_RDONLY);
  if (this->video_fd_ < 0) {
    ESP_LOGE(TAG, "Failed to open %s: errno=%d", this->video_device_path_, errno);
    return false;
  }

  // Query capabilities
  if (ioctl(this->video_fd_, VIDIOC_QUERYCAP, &cap) != 0) {
    ESP_LOGE(TAG, "Failed to query capabilities (errno=%d)", errno);
    close(this->video_fd_);
    this->video_fd_ = -1;
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
    ESP_LOGE(TAG, "Failed to get format (errno=%d)", errno);
    close(this->video_fd_);
    this->video_fd_ = -1;
    return false;
  }

  // Ajuster le format si incompatible
  bool need_set = false;
  if (fmt.fmt.pix.width != this->width_ || fmt.fmt.pix.height != this->height_) {
    fmt.fmt.pix.width  = this->width_;
    fmt.fmt.pix.height = this->height_;
    need_set = true;
  }
  if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    need_set = true;
  }

  if (need_set) {
    if (ioctl(this->video_fd_, VIDIOC_S_FMT, &fmt) != 0) {
      ESP_LOGE(TAG, "Failed to set format (errno=%d)", errno);
      close(this->video_fd_);
      this->video_fd_ = -1;
      return false;
    }
  }

  ESP_LOGI(TAG, "Format: %ux%u RGB565", fmt.fmt.pix.width, fmt.fmt.pix.height);
  this->buffer_length_ = fmt.fmt.pix.sizeimage ? fmt.fmt.pix.sizeimage : (this->width_ * this->height_ * 2);
  return true;
}

bool LVGLCameraDisplay::setup_buffers_() {
  // Demande de buffers
  struct v4l2_requestbuffers req;
  memset(&req, 0, sizeof(req));
  req.count  = VIDEO_BUFFER_COUNT;
  req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = MEMORY_TYPE;

  if (ioctl(this->video_fd_, VIDIOC_REQBUFS, &req) != 0) {
    ESP_LOGE(TAG, "Failed to request buffers: errno=%d", errno);
    return false;
  }

  if (req.count < VIDEO_BUFFER_COUNT) {
    ESP_LOGW(TAG, "Driver returned only %u buffers (requested %u)", req.count, VIDEO_BUFFER_COUNT);
  }

  // Mapper chaque buffer
  for (uint32_t i = 0; i < req.count && i < VIDEO_BUFFER_COUNT; i++) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = MEMORY_TYPE;
    buf.index  = i;

    if (ioctl(this->video_fd_, VIDIOC_QUERYBUF, &buf) != 0) {
      ESP_LOGE(TAG, "Failed to query buffer %u: errno=%d", i, errno);
      return false;
    }

    void *addr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, this->video_fd_, buf.m.offset);
    if (addr == MAP_FAILED || addr == nullptr) {
      ESP_LOGE(TAG, "Failed to mmap buffer %u: errno=%d", i, errno);
      return false;
    }

    this->mmap_buffers_[i]  = reinterpret_cast<uint8_t*>(addr);
    this->mmap_lengths_[i]  = buf.length;
    this->buffer_length_    = buf.length;

    ESP_LOGI(TAG, "Buffer %u mapped at %p (size=%zu)", i, this->mmap_buffers_[i], this->mmap_lengths_[i]);

    if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) != 0) {
      ESP_LOGE(TAG, "Failed to queue buffer %u: errno=%d", i, errno);
      return false;
    }
  }

  return true;
}

bool LVGLCameraDisplay::start_streaming_() {
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->video_fd_, VIDIOC_STREAMON, &type) != 0) {
    ESP_LOGE(TAG, "Failed to start streaming: errno=%d", errno);
    return false;
  }
  this->streaming_ = true;
  ESP_LOGI(TAG, "✅ Streaming started");
  return true;
}

bool LVGLCameraDisplay::init_ppa_() {
  ppa_client_config_t cfg = {
      .oper_type = PPA_OPERATION_SRM,
      .max_pending_trans_num = 1,
  };
  esp_err_t ret = ppa_register_client(&cfg, &this->ppa_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA register failed: 0x%x", ret);
    this->ppa_handle_ = nullptr;
    return false;
  }

  // Buffer cible (rotation <= peut permuter largeur/hauteur)
  uint16_t out_w = this->width_;
  uint16_t out_h = this->height_;
  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    uint16_t tmp = out_w;
    out_w = out_h; out_h = tmp;
  }
  this->transform_buffer_size_ = out_w * out_h * 2; // RGB565
  this->transform_buffer_ = (uint8_t*)heap_caps_aligned_alloc(64, this->transform_buffer_size_, MALLOC_CAP_SPIRAM);
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

bool LVGLCameraDisplay::transform_frame_(const uint8_t *src, uint8_t *dst, uint16_t &w, uint16_t &h) {
  if (!this->ppa_handle_ || !src || !dst) return false;

  uint16_t src_w = this->width_;
  uint16_t src_h = this->height_;
  uint16_t dst_w = src_w;
  uint16_t dst_h = src_h;

  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    dst_w = src_h;
    dst_h = src_w;
  }

  ppa_srm_rotation_angle_t ppa_rotation;
  switch (this->rotation_) {
    case ROTATION_0:   ppa_rotation = PPA_SRM_ROTATION_ANGLE_0; break;
    case ROTATION_90:  ppa_rotation = PPA_SRM_ROTATION_ANGLE_90; break;
    case ROTATION_180: ppa_rotation = PPA_SRM_ROTATION_ANGLE_180; break;
    case ROTATION_270: ppa_rotation = PPA_SRM_ROTATION_ANGLE_270; break;
    default:           ppa_rotation = PPA_SRM_ROTATION_ANGLE_0; break;
  }

  ppa_srm_oper_config_t srm = {
    .in  = {
      .buffer         = const_cast<uint8_t*>(src),
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
      .buffer_size    = this->transform_buffer_size_,
      .pic_w          = dst_w,
      .pic_h          = dst_h,
      .block_offset_x = 0,
      .block_offset_y = 0,
      .srm_cm         = PPA_SRM_COLOR_MODE_RGB565
    },
    .rotation_angle = ppa_rotation,
    .scale_x        = 1.0f,
    .scale_y        = 1.0f,
    .mirror_x       = this->mirror_x_,
    .mirror_y       = this->mirror_y_,
    .rgb_swap       = false,
    .byte_swap      = false,
    .mode           = PPA_TRANS_MODE_BLOCKING
  };

  esp_err_t ret = ppa_do_scale_rotate_mirror(this->ppa_handle_, &srm);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA transformation failed: 0x%x", ret);
    return false;
  }

  w = dst_w;
  h = dst_h;
  return true;
}

bool LVGLCameraDisplay::capture_frame_() {
  // DQBUF
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = MEMORY_TYPE;

  if (ioctl(this->video_fd_, VIDIOC_DQBUF, &buf) != 0) {
    ESP_LOGE(TAG, "Failed to dequeue buffer: errno=%d", errno);
    return false;
  }

  uint8_t *frame_data = this->mmap_buffers_[buf.index];
  uint16_t disp_w = this->width_;
  uint16_t disp_h = this->height_;

  if (this->first_update_) {
    ESP_LOGI(TAG, "🖼️  First frame:");
    ESP_LOGI(TAG, "   Buffer index: %u", buf.index);
    ESP_LOGI(TAG, "   Buffer address: %p", frame_data);
    ESP_LOGI(TAG, "   Bytes used: %u", buf.bytesused);
    this->first_update_ = false;
  }

  // Transformation PPA si nécessaire
  if (this->ppa_handle_ && this->transform_buffer_) {
    if (this->transform_frame_(frame_data, this->transform_buffer_, disp_w, disp_h)) {
      frame_data = this->transform_buffer_;
    }
  } else {
    // Si pas de PPA mais miroir X demandé => LVGL ne gère pas, on laisse brut (ou prévoir copie CPU si nécessaire)
  }

  // Mettre à jour LVGL: on recharge le buffer si l'adresse change (canvas non-copié)
  if (this->last_display_buffer_ != frame_data) {
#if LV_VERSION_CHECK(9, 0, 0)
    lv_canvas_set_buffer(this->canvas_obj_, frame_data, disp_w, disp_h, LV_COLOR_FORMAT_RGB565);
#else
    lv_canvas_set_buffer(this->canvas_obj_, frame_data, disp_w, disp_h, LV_IMG_CF_TRUE_COLOR);
#endif
    this->last_display_buffer_ = frame_data;
    lv_obj_invalidate(this->canvas_obj_);
  }

  // QBUF
  if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) != 0) {
    ESP_LOGE(TAG, "Failed to queue buffer: errno=%d", errno);
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

  for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
    if (this->mmap_buffers_[i]) {
      if (this->mmap_lengths_[i]) {
        munmap(this->mmap_buffers_[i], this->mmap_lengths_[i]);
      }
      this->mmap_buffers_[i] = nullptr;
      this->mmap_lengths_[i] = 0;
    }
  }

  if (this->video_fd_ >= 0) {
    close(this->video_fd_);
    this->video_fd_ = -1;
  }

  this->deinit_ppa_();
}
#endif  // USE_ESP32_VARIANT_ESP32P4

}  // namespace lvgl_camera_display
}  // namespace esphome





