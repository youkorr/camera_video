#include "lvgl_camera_display.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

void LVGLCameraDisplay::setup() {
#ifdef USE_ESP32_VARIANT_ESP32P4
  ESP_LOGCONFIG(TAG, "🎥 Initialisation LVGL Camera Display (V4L2)");

  if (!this->camera_) {
    ESP_LOGE(TAG, "Aucune caméra associée !");
    this->mark_failed();
    return;
  }

  this->width_ = this->camera_->get_image_width();
  this->height_ = this->camera_->get_image_height();
  ESP_LOGI(TAG, "Résolution détectée : %ux%u", this->width_, this->height_);

  if (!this->camera_->is_streaming()) {
    ESP_LOGI(TAG, "Démarrage du flux caméra...");
    if (!this->camera_->start_streaming()) {
      ESP_LOGE(TAG, "Impossible de démarrer le streaming");
      this->mark_failed();
      return;
    }
  }

  if (!this->open_v4l2_device_() || !this->setup_v4l2_format_() ||
      !this->setup_v4l2_buffers_() || !this->start_v4l2_streaming_()) {
    this->mark_failed();
    return;
  }

  if (this->use_ppa_) {
    if (this->init_ppa_()) {
      ESP_LOGI(TAG, "✅ PPA initialisé (rotation=%d, mirror_x=%d, mirror_y=%d)",
               this->rotation_, this->mirror_x_, this->mirror_y_);
    } else {
      ESP_LOGW(TAG, "⚠️ PPA désactivé (initialisation échouée)");
      this->use_ppa_ = false;
    }
  }

  ESP_LOGI(TAG, "✅ V4L2 prêt (%s)", this->video_device_);
  ESP_LOGI(TAG, "Mode : %s", this->direct_mode_ ? "Direct framebuffer" : "Canvas LVGL");
#else
  ESP_LOGE(TAG, "Ce composant nécessite l'ESP32-P4 !");
  this->mark_failed();
#endif
}

#ifdef USE_ESP32_VARIANT_ESP32P4

// ---------- V4L2  ----------
bool LVGLCameraDisplay::open_v4l2_device_() {
  this->video_fd_ = open(this->video_device_, O_RDONLY);
  if (this->video_fd_ < 0) {
    ESP_LOGE(TAG, "Erreur ouverture %s", this->video_device_);
    return false;
  }
  ESP_LOGI(TAG, "V4L2 device ouvert (fd=%d)", this->video_fd_);
  return true;
}

bool LVGLCameraDisplay::setup_v4l2_format_() {
  struct v4l2_format fmt = {};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = this->width_;
  fmt.fmt.pix.height = this->height_;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;

  if (ioctl(this->video_fd_, VIDIOC_S_FMT, &fmt) < 0) {
    ESP_LOGE(TAG, "VIDIOC_S_FMT failed");
    return false;
  }

  this->buffer_length_ = fmt.fmt.pix.sizeimage;
  return true;
}

bool LVGLCameraDisplay::setup_v4l2_buffers_() {
  struct v4l2_requestbuffers req = {};
  req.count = VIDEO_BUFFER_COUNT;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(this->video_fd_, VIDIOC_REQBUFS, &req) < 0) return false;

  for (uint32_t i = 0; i < req.count; i++) {
    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    ioctl(this->video_fd_, VIDIOC_QUERYBUF, &buf);
    this->mmap_buffers_[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                             MAP_SHARED, this->video_fd_, buf.m.offset);
    ioctl(this->video_fd_, VIDIOC_QBUF, &buf);
  }
  return true;
}

bool LVGLCameraDisplay::start_v4l2_streaming_() {
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->video_fd_, VIDIOC_STREAMON, &type) < 0) return false;
  this->v4l2_streaming_ = true;
  return true;
}

bool LVGLCameraDisplay::capture_v4l2_frame_(uint8_t **frame_data) {
  struct v4l2_buffer buf = {};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->video_fd_, VIDIOC_DQBUF, &buf) < 0) return false;
  *frame_data = this->mmap_buffers_[buf.index];
  this->current_buffer_index_ = buf.index;
  return true;
}

void LVGLCameraDisplay::release_v4l2_frame_() {
  if (this->current_buffer_index_ < 0) return;
  struct v4l2_buffer buf = {};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = this->current_buffer_index_;
  ioctl(this->video_fd_, VIDIOC_QBUF, &buf);
  this->current_buffer_index_ = -1;
}

// ---------- PPA ----------
bool LVGLCameraDisplay::init_ppa_() {
  ppa_client_config_t cfg = {.oper_type = PPA_OPERATION_SRM, .max_pending_trans_num = 1};
  if (ppa_register_client(&cfg, &this->ppa_handle_) != ESP_OK) return false;

  this->transform_buffer_size_ = this->width_ * this->height_ * 2;
  this->transform_buffer_ =
      (uint8_t *)heap_caps_aligned_alloc(64, this->transform_buffer_size_, MALLOC_CAP_SPIRAM);
  return this->transform_buffer_ != nullptr;
}

void LVGLCameraDisplay::deinit_ppa_() {
  if (this->transform_buffer_) heap_caps_free(this->transform_buffer_);
  if (this->ppa_handle_) ppa_unregister_client(this->ppa_handle_);
}

bool LVGLCameraDisplay::transform_frame_(const uint8_t *src, uint8_t *dst) {
  if (!this->ppa_handle_) return false;

  ppa_srm_oper_config_t srm{};
  srm.in.buffer = (void *)src;
  srm.in.pic_w = this->width_;
  srm.in.pic_h = this->height_;
  srm.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

  srm.out.buffer = dst;
  srm.out.pic_w = this->width_;
  srm.out.pic_h = this->height_;
  srm.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  srm.mode = PPA_TRANS_MODE_BLOCKING;
  srm.rotation_angle = (ppa_srm_rotation_angle_t)this->rotation_;
  srm.mirror_x = this->mirror_x_;
  srm.mirror_y = this->mirror_y_;

  return (ppa_do_scale_rotate_mirror(this->ppa_handle_, &srm) == ESP_OK);
}

// ---------- Direct display ----------
void LVGLCameraDisplay::direct_display_(uint8_t *buffer, uint16_t w, uint16_t h) {
  lv_disp_t *disp = lv_disp_get_default();
  lv_disp_drv_t *drv = (lv_disp_drv_t *)disp->driver;
  void *fb = drv->draw_buf->buf_act;

  if (this->use_ppa_ && this->ppa_handle_) {
    this->transform_frame_(buffer, (uint8_t *)fb);
  } else {
    memcpy(fb, buffer, w * h * 2);
  }

  lv_disp_flush_ready(drv);
}

#endif  // USE_ESP32_VARIANT_ESP32P4

// ---------- Boucle principale ----------
void LVGLCameraDisplay::loop() {
#ifdef USE_ESP32_VARIANT_ESP32P4
  if (!this->v4l2_streaming_) return;
  uint32_t now = millis();
  if (now - this->last_update_time_ < this->update_interval_) return;
  this->last_update_time_ = now;

  uint8_t *frame_data = nullptr;
  if (!this->capture_v4l2_frame_(&frame_data) || !frame_data) return;

  uint8_t *display_buffer = frame_data;
  if (this->use_ppa_ && this->transform_buffer_) {
    if (this->transform_frame_(frame_data, this->transform_buffer_)) {
      display_buffer = this->transform_buffer_;
    }
  }

  if (this->direct_mode_) {
    // 🚀 Mode direct PPA
    this->direct_display_(display_buffer, this->width_, this->height_);
  } else if (this->canvas_obj_) {
    // 🐢 Mode canvas LVGL
    lv_canvas_set_buffer(this->canvas_obj_, display_buffer,
                         this->width_, this->height_, LV_IMG_CF_TRUE_COLOR);
    lv_obj_invalidate(this->canvas_obj_);
  }

  this->release_v4l2_frame_();

  // Log FPS
  this->frame_count_++;
  if (this->first_update_) {
    this->first_update_ = false;
    this->last_fps_time_ = now;
  } else if (now - this->last_fps_time_ > 5000) {
    float fps = this->frame_count_ * 1000.0f / (now - this->last_fps_time_);
    float drop = (this->drop_count_ * 100.0f) / (this->frame_count_ + this->drop_count_);
    ESP_LOGI(TAG, "📊 Display: %.1f FPS | Drops: %.1f%%", fps, drop);
    this->frame_count_ = this->drop_count_ = 0;
    this->last_fps_time_ = now;
  }
#endif
}

void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "LVGL Camera Display:");
  ESP_LOGCONFIG(TAG, "  Mode direct: %s", this->direct_mode_ ? "Oui" : "Non");
  ESP_LOGCONFIG(TAG, "  PPA: %s", this->use_ppa_ ? "Activé" : "Désactivé");
  ESP_LOGCONFIG(TAG, "  Résolution: %ux%u", this->width_, this->height_);
  ESP_LOGCONFIG(TAG, "  Intervalle: %ums", this->update_interval_);
}

}  // namespace lvgl_camera_display
}  // namespace esphome









