#include "lvgl_camera_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

#include <fcntl.h>
#include "ioctl.h"
#include "mman.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>

// V4L2 + device name viennent de ton arborescence
#include "../mipi_dsi_cam/videodev2.h"
#include "../mipi_dsi_cam/esp_video_device.h"  // ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#include "driver/ppa.h"

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

// ===================== SETUP =====================
void LVGLCameraDisplay::setup() {
  ESP_LOGCONFIG(TAG, "🎥 LVGL Camera Display (esp-video / V4L2 path)");

  // Résolution depuis ta caméra si liée (pas obligatoire)
  if (this->camera_) {
    this->width_  = this->camera_->get_image_width();
    this->height_ = this->camera_->get_image_height();
    ESP_LOGI(TAG, "📐 Using camera resolution: %ux%u", this->width_, this->height_);
  } else {
    ESP_LOGW(TAG, "⚠️  No camera linked, defaulting to %ux%u", this->width_, this->height_);
  }

  // (Important) l’adaptateur V4L2 crée /dev/video0 : on laisse le temps si besoin
  bool opened = false;
  for (int i = 0; i < 20; i++) {  // ~2s max
    if (this->open_video_device_()) { opened = true; break; }
    if (errno != ENOENT) break;
    ESP_LOGW(TAG, "Waiting for %s...", this->video_device_);
    delay(100);
  }
  if (!opened) {
    ESP_LOGE(TAG, "❌ Failed to open video device");
    this->mark_failed();
    return;
  }

  if (!this->setup_buffers_()) {
    ESP_LOGE(TAG, "❌ Failed to setup buffers");
    this->cleanup_();
    this->mark_failed();
    return;
  }

  if (this->rotation_ != ROTATION_0 || this->mirror_x_ || this->mirror_y_) {
    if (!this->init_ppa_()) {
      ESP_LOGE(TAG, "❌ Failed to initialize PPA");
      this->cleanup_();
      this->mark_failed();
      return;
    }
    ESP_LOGI(TAG, "✅ PPA initialized (rotation=%d°, mirror_x=%s, mirror_y=%s)",
             this->rotation_, this->mirror_x_ ? "ON" : "OFF", this->mirror_y_ ? "ON" : "OFF");
  }

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
  ESP_LOGI(TAG, "   Target FPS: %.1f", 1000.0f / this->update_interval_);
}

// ===================== LOOP =====================
void LVGLCameraDisplay::loop() {
  if (!this->streaming_ || !this->canvas_obj_) return;

  uint32_t now = millis();
  if (now - this->last_update_time_ < this->update_interval_) return;

  if (this->capture_frame_()) {
    this->frame_count_++;
    this->last_update_time_ = now;

    if (this->frame_count_ % 100 == 0) {
      if (this->last_fps_time_ > 0) {
        float elapsed = (now - this->last_fps_time_) / 1000.0f;
        float fps = 100.0f / (elapsed > 0 ? elapsed : 1.0f);
        ESP_LOGI(TAG, "🎞️  Display FPS: %.2f | Frames: %u", fps, this->frame_count_);
      }
      this->last_fps_time_ = now;
    }
  }
}

// ===================== CONFIG DUMP =====================
void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "LVGL Camera Display:");
  ESP_LOGCONFIG(TAG, "  Mode: esp-video V4L2");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->video_device_);
  ESP_LOGCONFIG(TAG, "  Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGCONFIG(TAG, "  Buffers: %d (mmap zero-copy)", VIDEO_BUFFER_COUNT);
  ESP_LOGCONFIG(TAG, "  PPA: %s", this->ppa_handle_ ? "ENABLED" : "DISABLED");
  ESP_LOGCONFIG(TAG, "  Canvas: %s", this->canvas_obj_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Rotation: %d°", this->rotation_);
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", this->mirror_x_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", this->mirror_y_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Update interval: %u ms", this->update_interval_);
  ESP_LOGCONFIG(TAG, "  Camera linked: %s", this->camera_ ? "YES" : "NO");
}

// ===================== CANVAS =====================
void LVGLCameraDisplay::configure_canvas(lv_obj_t *canvas) {
  this->canvas_obj_ = canvas;
  ESP_LOGI(TAG, "🎨 Canvas configured: %p", canvas);
  if (canvas) {
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_TRANSP, 0);
  }
}

// ===================== ESP-VIDEO STYLE (V4L2) =====================

bool LVGLCameraDisplay::open_video_device_() {
  // Comme ta démo: open(O_RDONLY) fonctionne, mais on préfère O_RDWR|O_NONBLOCK pour DQBUF non-bloquant
  this->video_fd_ = open(this->video_device_, O_RDWR | O_NONBLOCK);
  if (this->video_fd_ < 0) {
    // errno est pertinent (ENOENT si pas encore créé)
    return false;
  }

  // (Optionnel) Log capabilities
  struct v4l2_capability cap{};
  if (ioctl(this->video_fd_, VIDIOC_QUERYCAP, &cap) == 0) {
    ESP_LOGI(TAG, "version: %u.%u.%u | driver:%s | card:%s | bus:%s",
             (uint16_t)(cap.version >> 16), (uint8_t)(cap.version >> 8), (uint8_t)cap.version,
             (const char*)cap.driver, (const char*)cap.card, (const char*)cap.bus_info);
  }

  // Récupérer/forcer le format (RGB565)
  struct v4l2_format fmt{};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->video_fd_, VIDIOC_G_FMT, &fmt) != 0) {
    ESP_LOGW(TAG, "Failed to get format, will try to set");
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  }

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
  fmt.fmt.pix.field = V4L2_FIELD_NONE;

  if (need_set) {
    if (ioctl(this->video_fd_, VIDIOC_S_FMT, &fmt) != 0) {
      ESP_LOGE(TAG, "Failed to set format");
      close(this->video_fd_);
      this->video_fd_ = -1;
      return false;
    }
  }

  ESP_LOGI(TAG, "Format: %ux%u RGB565", fmt.fmt.pix.width, fmt.fmt.pix.height);
  return true;
}

bool LVGLCameraDisplay::setup_buffers_() {
  // Demander N buffers en MMAP (exactement comme ta démo)
  struct v4l2_requestbuffers req{};
  req.count  = VIDEO_BUFFER_COUNT;
  req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(this->video_fd_, VIDIOC_REQBUFS, &req) != 0 || req.count < VIDEO_BUFFER_COUNT) {
    ESP_LOGE(TAG, "Failed to request buffers (got %u)", req.count);
    return false;
  }

  for (int i = 0; i < (int)req.count; i++) {
    struct v4l2_buffer buf{};
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = (uint32_t)i;

    if (ioctl(this->video_fd_, VIDIOC_QUERYBUF, &buf) != 0) {
      ESP_LOGE(TAG, "Failed to query buffer %d: %d", i, errno);
      return false;
    }

    void *ptr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, this->video_fd_, buf.m.offset);
    if (ptr == MAP_FAILED) {
      ESP_LOGE(TAG, "Failed to mmap buffer %d: %d", i, errno);
      return false;
    }

    this->mmap_buffers_[i] = static_cast<uint8_t*>(ptr);
    this->buffer_length_   = buf.length;

    if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) != 0) {
      ESP_LOGE(TAG, "Failed to queue buffer %d: %d", i, errno);
      return false;
    }

    ESP_LOGI(TAG, "Buffer %d mapped at %p (size=%zu)", i, ptr, (size_t)buf.length);
  }

  return true;
}

bool LVGLCameraDisplay::start_streaming_() {
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->video_fd_, VIDIOC_STREAMON, &type) != 0) {
    ESP_LOGE(TAG, "Failed to start streaming: %d", errno);
    return false;
  }
  this->streaming_ = true;
  ESP_LOGI(TAG, "✅ Streaming started");
  return true;
}

bool LVGLCameraDisplay::capture_frame_() {
  struct v4l2_buffer buf{};
  buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->video_fd_, VIDIOC_DQBUF, &buf) != 0) {
    if (errno != EAGAIN) ESP_LOGE(TAG, "Failed to dequeue buffer: %d", errno);
    return false;
  }

  uint8_t *frame_data = this->mmap_buffers_[buf.index];
  uint16_t w = this->width_;
  uint16_t h = this->height_;

  if (this->first_update_) {
    ESP_LOGI(TAG, "🖼️  First frame: index=%u, addr=%p, bytes=%u", buf.index, frame_data, buf.bytesused);
    this->first_update_ = false;
  }

  // PPA (rotation / miroir) — même logique que ta démo
  if (this->ppa_handle_ && this->transform_buffer_) {
    if (this->transform_frame_(frame_data, this->transform_buffer_)) {
      frame_data = this->transform_buffer_;
      if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
        std::swap(w, h);
      }
    }
  }

  // LVGL — met à jour le canvas
#if LV_VERSION_CHECK(9, 0, 0)
  lv_canvas_set_buffer(this->canvas_obj_, frame_data, w, h, LV_COLOR_FORMAT_RGB565);
#else
  lv_canvas_set_buffer(this->canvas_obj_, frame_data, w, h, LV_IMG_CF_TRUE_COLOR);
#endif
  lv_obj_invalidate(this->canvas_obj_);

  // Re-queue pour la prochaine frame
  if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) != 0) {
    ESP_LOGE(TAG, "Failed to queue buffer: %d", errno);
    return false;
  }

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
    return false;
  }

  uint16_t tw = this->width_, th = this->height_;
  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) std::swap(tw, th);

  this->transform_buffer_size_ = (size_t)tw * th * 2;  // RGB565
  this->transform_buffer_ = (uint8_t*) heap_caps_aligned_alloc(64, this->transform_buffer_size_, MALLOC_CAP_SPIRAM);
  if (!this->transform_buffer_) {
    ESP_LOGE(TAG, "Failed to allocate transform buffer");
    ppa_unregister_client(this->ppa_handle_);
    this->ppa_handle_ = nullptr;
    return false;
  }

  ESP_LOGI(TAG, "PPA transform buffer: %ux%u (%zu bytes)", tw, th, this->transform_buffer_size_);
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

  uint16_t src_w = this->width_, src_h = this->height_;
  uint16_t dst_w = src_w, dst_h = src_h;
  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) std::swap(dst_w, dst_h);

  ppa_srm_rotation_angle_t rot;
  switch (this->rotation_) {
    case ROTATION_0:   rot = PPA_SRM_ROTATION_ANGLE_0; break;
    case ROTATION_90:  rot = PPA_SRM_ROTATION_ANGLE_90; break;
    case ROTATION_180: rot = PPA_SRM_ROTATION_ANGLE_180; break;
    case ROTATION_270: rot = PPA_SRM_ROTATION_ANGLE_270; break;
    default:           rot = PPA_SRM_ROTATION_ANGLE_0; break;
  }

  ppa_srm_oper_config_t srm = {
    .in = {
      .buffer = const_cast<uint8_t*>(src),
      .pic_w = src_w, .pic_h = src_h,
      .block_w = src_w, .block_h = src_h,
      .block_offset_x = 0, .block_offset_y = 0,
      .srm_cm = PPA_SRM_COLOR_MODE_RGB565
    },
    .out = {
      .buffer = dst,
      .buffer_size = this->transform_buffer_size_,
      .pic_w = dst_w, .pic_h = dst_h,
      .block_offset_x = 0, .block_offset_y = 0,
      .srm_cm = PPA_SRM_COLOR_MODE_RGB565
    },
    .rotation_angle = rot,
    .scale_x = 1.0f, .scale_y = 1.0f,
    .mirror_x = this->mirror_x_, .mirror_y = this->mirror_y_,
    .rgb_swap = false, .byte_swap = false,
    .mode = PPA_TRANS_MODE_BLOCKING
  };

  esp_err_t ret = ppa_do_scale_rotate_mirror(this->ppa_handle_, &srm);
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

}  // namespace lvgl_camera_display
}  // namespace esphome

#else  // !USE_ESP32_VARIANT_ESP32P4
// Rien (plateforme non supportée)
#endif

