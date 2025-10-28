#include "lvgl_camera_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32_VARIANT_ESP32P4
#include "esp_cache.h"
#endif

extern "C" void register_fd_to_device(int real_fd, int temp_fd);

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

// ✅ Descripteur d'image global pour le canvas (LVGL 8)
static lv_img_dsc_t camera_img_dsc;

void LVGLCameraDisplay::setup() {
  ESP_LOGCONFIG(TAG, "🎥 LVGL Camera Display (Task-based)");
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->direct_mode_ ? "DIRECT (DMA/PPA)" : "CANVAS (software)");
  ESP_LOGCONFIG(TAG, "  PPA: %s", this->use_ppa_ ? "ENABLED" : "DISABLED");

#ifdef USE_ESP32_VARIANT_ESP32P4
  if (this->camera_) {
    this->width_ = this->camera_->get_image_width();
    this->height_ = this->camera_->get_image_height();
    ESP_LOGI(TAG, "📐 Using camera resolution: %ux%u", this->width_, this->height_);
  } else {
    ESP_LOGW(TAG, "⚠️  No camera linked");
    this->mark_failed();
    return;
  }

  if (!this->camera_->is_streaming()) {
    ESP_LOGI(TAG, "Starting camera streaming...");
    if (!this->camera_->start_streaming()) {
      ESP_LOGE(TAG, "❌ Failed to start camera streaming");
      this->mark_failed();
      return;
    }
  }

  if (!this->camera_->get_v4l2_adapter()) {
    ESP_LOGI(TAG, "Enabling V4L2 adapter...");
    this->camera_->enable_v4l2_adapter();
    delay(100);
  }

  delay(200);

  if (!this->open_v4l2_device_()) {
    ESP_LOGE(TAG, "❌ Failed to open V4L2 device");
    this->mark_failed();
    return;
  }

  if (!this->setup_v4l2_format_()) {
    ESP_LOGE(TAG, "❌ Failed to setup V4L2 format");
    this->mark_failed();
    return;
  }

  if (!this->setup_v4l2_buffers_()) {
    ESP_LOGE(TAG, "❌ Failed to setup V4L2 buffers");
    this->mark_failed();
    return;
  }

  if (!this->start_v4l2_streaming_()) {
    ESP_LOGE(TAG, "❌ Failed to start V4L2 streaming");
    this->mark_failed();
    return;
  }

  if (this->use_ppa_) {
    if (!this->init_ppa_()) {
      ESP_LOGE(TAG, "❌ Failed to initialize PPA");
      this->use_ppa_ = false;
      ESP_LOGW(TAG, "⚠️  Continuing without PPA");
    } else {
      ESP_LOGI(TAG, "✅ PPA initialized (rotation=%d°, mirror_x=%s, mirror_y=%s)",
               this->rotation_, this->mirror_x_ ? "ON" : "OFF", 
               this->mirror_y_ ? "ON" : "OFF");
    }
  }

  if (this->direct_mode_) {
    if (!this->init_direct_mode_()) {
      ESP_LOGE(TAG, "❌ Failed to initialize direct mode");
      if (!this->canvas_obj_) {
        ESP_LOGE(TAG, "❌ No canvas configured, cannot fallback");
        this->mark_failed();
        return;
      }
      ESP_LOGW(TAG, "⚠️  Falling back to canvas mode");
      this->direct_mode_ = false;
    } else {
      ESP_LOGI(TAG, "✅ Direct mode initialized");
    }
  }

  // ✅ CRITIQUE : Créer le mutex pour protéger l'accès au display
  this->display_mutex_ = xSemaphoreCreateMutex();
  if (!this->display_mutex_) {
    ESP_LOGE(TAG, "❌ Failed to create display mutex");
    this->mark_failed();
    return;
  }

  // ✅ CRITIQUE : Créer la tâche de capture dédiée (comme M5Stack)
  this->task_running_ = true;
  BaseType_t ret = xTaskCreatePinnedToCore(
    capture_task_,              // Fonction de la tâche
    "camera_capture",           // Nom
    8 * 1024,                   // Stack size (8KB comme M5Stack)
    this,                       // Paramètre (pointeur vers this)
    5,                          // Priorité 5 (comme M5Stack)
    &this->capture_task_handle_,// Handle
    1                           // Core 1 (comme M5Stack)
  );

  if (ret != pdPASS) {
    ESP_LOGE(TAG, "❌ Failed to create capture task");
    this->task_running_ = false;
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "✅ V4L2 pipeline ready");
  ESP_LOGI(TAG, "   Device: %s", this->video_device_);
  ESP_LOGI(TAG, "   Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGI(TAG, "   Target FPS: %.1f", 1000.0f / this->update_interval_);
  ESP_LOGI(TAG, "   Buffers: %d x %u bytes", VIDEO_BUFFER_COUNT, this->buffer_length_);
  ESP_LOGI(TAG, "   PPA: %s", this->use_ppa_ ? "ENABLED" : "DISABLED");
  ESP_LOGI(TAG, "   Mode: %s", this->direct_mode_ ? "DIRECT" : "CANVAS");
  ESP_LOGI(TAG, "   ✅ Capture task running on core 1");
#else
  ESP_LOGE(TAG, "❌ V4L2 pipeline requires ESP32-P4");
  this->mark_failed();
#endif
}

#ifdef USE_ESP32_VARIANT_ESP32P4

// ✅ NOUVEAU : Tâche de capture (comme M5Stack)
void LVGLCameraDisplay::capture_task_(void *param) {
  LVGLCameraDisplay *self = (LVGLCameraDisplay *)param;
  ESP_LOGI(TAG, "📹 Capture task started on core %d", xPortGetCoreID());
  self->capture_loop_();
  ESP_LOGI(TAG, "📹 Capture task ended");
  vTaskDelete(NULL);
}

// ✅ NOUVEAU : Boucle de capture (VRAIE solution 30 FPS)
void LVGLCameraDisplay::capture_loop_() {
  uint32_t frame_count = 0;
  uint32_t drop_count = 0;
  uint32_t last_fps_time = millis();

  while (this->task_running_) {
    uint8_t *frame_data = nullptr;
    
    // Capturer une frame
    if (!this->capture_v4l2_frame_(&frame_data)) {
      drop_count++;
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    if (!frame_data) {
      this->release_v4l2_frame_();
      continue;
    }

    // Mode canvas
    if (!this->direct_mode_ && this->canvas_obj_) {
      uint8_t *display_buffer = frame_data;
      uint16_t canvas_width = this->width_;
      uint16_t canvas_height = this->height_;
      
      // Transform avec PPA si nécessaire
      if (this->use_ppa_ && this->ppa_handle_ && this->transform_buffer_ &&
          (this->rotation_ != ROTATION_0 || this->mirror_x_ || this->mirror_y_)) {
        
        if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
          canvas_width = this->height_;
          canvas_height = this->width_;
        }
        
        if (this->transform_frame_(frame_data, this->transform_buffer_)) {
          display_buffer = this->transform_buffer_;
        }
      }

      // ✅ CRITIQUE : Lock display (comme M5Stack avec bsp_display_lock)
      if (xSemaphoreTake(this->display_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Mettre à jour le canvas avec l'API image
        camera_img_dsc.header.always_zero = 0;
        camera_img_dsc.header.w = canvas_width;
        camera_img_dsc.header.h = canvas_height;
        camera_img_dsc.data_size = canvas_width * canvas_height * 2;
        camera_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;  // LVGL 8
        camera_img_dsc.data = display_buffer;
        
        lv_img_set_src(this->canvas_obj_, &camera_img_dsc);
        lv_obj_invalidate(this->canvas_obj_);
        
        xSemaphoreGive(this->display_mutex_);
      }
    }
    // Mode direct
    else if (this->direct_mode_ && this->lvgl_framebuffer_) {
      bool success = false;
      
      uint8_t *dest_buffer = this->aligned_buffer_ ? this->aligned_buffer_ : this->lvgl_framebuffer_;
      size_t dest_size = this->aligned_buffer_ ? this->aligned_buffer_size_ : this->lvgl_framebuffer_size_;
      
      if (this->use_ppa_ && this->ppa_handle_ && 
          (this->rotation_ != ROTATION_0 || this->mirror_x_ || this->mirror_y_)) {
        
        success = this->transform_frame_(frame_data, dest_buffer);
        
        if (success) {
          esp_err_t ret;
          if (this->aligned_buffer_) {
            ret = esp_cache_msync(dest_buffer, dest_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
          } else {
            ret = esp_cache_msync(dest_buffer, dest_size, 
                                  ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
          }
          
          if (this->aligned_buffer_) {
            memcpy(this->lvgl_framebuffer_, this->aligned_buffer_, this->lvgl_framebuffer_size_);
          }
        }
      }
      
      if (!success) {
        size_t copy_size = std::min(this->buffer_length_, this->lvgl_framebuffer_size_);
        memcpy(this->lvgl_framebuffer_, frame_data, copy_size);
      }

      // ✅ Lock pour invalider l'écran
      if (xSemaphoreTake(this->display_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
        lv_obj_invalidate(lv_scr_act());
        xSemaphoreGive(this->display_mutex_);
      }
    }

    this->release_v4l2_frame_();
    frame_count++;

    // Statistiques FPS toutes les 5 secondes
    uint32_t now = millis();
    if (now - last_fps_time >= 5000) {
      float fps = frame_count * 1000.0f / (now - last_fps_time);
      float drop_rate = (drop_count * 100.0f) / (frame_count + drop_count);
      ESP_LOGI(TAG, "📊 Display (%s): %.1f FPS | Drops: %u (%.1f%%)", 
               this->direct_mode_ ? "DIRECT" : "CANVAS",
               fps, drop_count, drop_rate);
      frame_count = 0;
      drop_count = 0;
      last_fps_time = now;
    }

    // ✅ Délai comme M5Stack (10ms = 100 FPS max)
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ... (fonctions V4L2 inchangées - je les mets pour avoir le fichier complet) ...

bool LVGLCameraDisplay::open_v4l2_device_() {
  ESP_LOGI(TAG, "Opening V4L2 device: %s", this->video_device_);
  
  this->video_fd_ = open(this->video_device_, O_RDONLY);
  if (this->video_fd_ < 0) {
    ESP_LOGE(TAG, "Failed to open %s: errno=%d", this->video_device_, errno);
    return false;
  }

  ESP_LOGI(TAG, "✅ V4L2 device opened (fd=%d)", this->video_fd_);
  
  struct v4l2_capability cap;
  if (ioctl(this->video_fd_, VIDIOC_QUERYCAP, &cap) < 0) {
    ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed");
    close(this->video_fd_);
    this->video_fd_ = -1;
    return false;
  }

  ESP_LOGI(TAG, "V4L2 Capabilities:");
  ESP_LOGI(TAG, "  Driver: %s", cap.driver);
  ESP_LOGI(TAG, "  Card: %s", cap.card);
  ESP_LOGI(TAG, "  Version: %u.%u.%u",
           (cap.version >> 16) & 0xFF,
           (cap.version >> 8) & 0xFF,
           cap.version & 0xFF);

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    ESP_LOGE(TAG, "Device does not support video capture");
    close(this->video_fd_);
    this->video_fd_ = -1;
    return false;
  }

  if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    ESP_LOGE(TAG, "Device does not support streaming");
    close(this->video_fd_);
    this->video_fd_ = -1;
    return false;
  }

  return true;
}

bool LVGLCameraDisplay::setup_v4l2_format_() {
  ESP_LOGI(TAG, "Setting V4L2 format: %ux%u RGB565", this->width_, this->height_);

  struct v4l2_format fmt = {};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  
  if (ioctl(this->video_fd_, VIDIOC_G_FMT, &fmt) < 0) {
    ESP_LOGE(TAG, "VIDIOC_G_FMT failed");
    return false;
  }

  ESP_LOGI(TAG, "Current format: %ux%u", fmt.fmt.pix.width, fmt.fmt.pix.height);

  if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565 ||
      fmt.fmt.pix.width != this->width_ ||
      fmt.fmt.pix.height != this->height_) {
    
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = this->width_;
    fmt.fmt.pix.height = this->height_;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(this->video_fd_, VIDIOC_S_FMT, &fmt) < 0) {
      ESP_LOGE(TAG, "VIDIOC_S_FMT failed: errno=%d", errno);
      return false;
    }
  }

  if (ioctl(this->video_fd_, VIDIOC_G_FMT, &fmt) < 0) {
    ESP_LOGE(TAG, "VIDIOC_G_FMT failed");
    return false;
  }

  this->buffer_length_ = fmt.fmt.pix.sizeimage;
  ESP_LOGI(TAG, "✅ V4L2 format set: %ux%u, buffer size=%u bytes",
           fmt.fmt.pix.width, fmt.fmt.pix.height, this->buffer_length_);

  return true;
}

bool LVGLCameraDisplay::setup_v4l2_buffers_() {
  ESP_LOGI(TAG, "Requesting %d V4L2 buffers...", VIDEO_BUFFER_COUNT);

  struct v4l2_requestbuffers req = {};
  req.count = VIDEO_BUFFER_COUNT;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->video_fd_, VIDIOC_REQBUFS, &req) < 0) {
    ESP_LOGE(TAG, "VIDIOC_REQBUFS failed: errno=%d", errno);
    return false;
  }

  if (req.count < VIDEO_BUFFER_COUNT) {
    ESP_LOGW(TAG, "Only got %u buffers (requested %d)", req.count, VIDEO_BUFFER_COUNT);
  }

  ESP_LOGI(TAG, "Allocated %u buffers", req.count);

  for (uint32_t i = 0; i < req.count; i++) {
    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (ioctl(this->video_fd_, VIDIOC_QUERYBUF, &buf) < 0) {
      ESP_LOGE(TAG, "VIDIOC_QUERYBUF failed for buffer %u", i);
      return false;
    }

    this->mmap_buffers_[i] = (uint8_t*)mmap(
      NULL,
      buf.length,
      PROT_READ | PROT_WRITE,
      MAP_SHARED,
      this->video_fd_,
      buf.m.offset
    );

    if (this->mmap_buffers_[i] == MAP_FAILED) {
      ESP_LOGE(TAG, "mmap failed for buffer %u: errno=%d", i, errno);
      return false;
    }

    ESP_LOGD(TAG, "Buffer %u: mapped at %p, length=%u, offset=%u",
             i, this->mmap_buffers_[i], buf.length, buf.m.offset);

    if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) < 0) {
      ESP_LOGE(TAG, "VIDIOC_QBUF failed for buffer %u: errno=%d", i, errno);
      return false;
    }
    
    ESP_LOGD(TAG, "Buffer %u queued", i);
  }

  ESP_LOGI(TAG, "✅ V4L2 buffers ready (all %u buffers queued)", req.count);
  return true;
}

bool LVGLCameraDisplay::start_v4l2_streaming_() {
  ESP_LOGI(TAG, "Starting V4L2 streaming...");

  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (ioctl(this->video_fd_, VIDIOC_STREAMON, &type) < 0) {
    ESP_LOGE(TAG, "VIDIOC_STREAMON failed: errno=%d", errno);
    return false;
  }

  this->v4l2_streaming_ = true;
  ESP_LOGI(TAG, "✅ V4L2 streaming started");
  return true;
}

bool LVGLCameraDisplay::capture_v4l2_frame_(uint8_t **frame_data) {
  if (!this->v4l2_streaming_) {
    return false;
  }

  struct v4l2_buffer buf = {};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (ioctl(this->video_fd_, VIDIOC_DQBUF, &buf) < 0) {
    if (errno == EAGAIN) {
      return false;
    }
    ESP_LOGE(TAG, "VIDIOC_DQBUF failed: errno=%d", errno);
    return false;
  }

  *frame_data = this->mmap_buffers_[buf.index];
  this->current_buffer_index_ = buf.index;

  return true;
}

void LVGLCameraDisplay::release_v4l2_frame_() {
  if (this->current_buffer_index_ >= 0) {
    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = this->current_buffer_index_;

    if (ioctl(this->video_fd_, VIDIOC_QBUF, &buf) < 0) {
      ESP_LOGW(TAG, "VIDIOC_QBUF failed");
    }
    
    this->current_buffer_index_ = -1;
  }
}

void LVGLCameraDisplay::cleanup_v4l2_() {
  if (this->v4l2_streaming_) {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(this->video_fd_, VIDIOC_STREAMOFF, &type);
    this->v4l2_streaming_ = false;
  }

  for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
    if (this->mmap_buffers_[i] && this->mmap_buffers_[i] != MAP_FAILED) {
      munmap(this->mmap_buffers_[i], this->buffer_length_);
      this->mmap_buffers_[i] = nullptr;
    }
  }
  
  if (this->work_buffer_) {
    heap_caps_free(this->work_buffer_);
    this->work_buffer_ = nullptr;
  }
  
  if (this->aligned_buffer_) {
    heap_caps_free(this->aligned_buffer_);
    this->aligned_buffer_ = nullptr;
  }

  if (this->video_fd_ >= 0) {
    close(this->video_fd_);
    this->video_fd_ = -1;
  }

  this->deinit_ppa_();
}

bool LVGLCameraDisplay::init_ppa_() {
  ESP_LOGI(TAG, "Initializing PPA...");
  
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
  
  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    std::swap(width, height);
  }

  if (!this->direct_mode_) {
    this->transform_buffer_size_ = width * height * 2;
    this->transform_buffer_ = (uint8_t*)heap_caps_aligned_alloc(
      64,
      this->transform_buffer_size_,
      MALLOC_CAP_SPIRAM
    );

    if (!this->transform_buffer_) {
      ESP_LOGE(TAG, "Failed to allocate transform buffer");
      ppa_unregister_client(this->ppa_handle_);
      this->ppa_handle_ = nullptr;
      return false;
    }

    ESP_LOGI(TAG, "PPA transform buffer: %ux%u @ %u bytes", 
             width, height, this->transform_buffer_size_);
  } else {
    ESP_LOGI(TAG, "PPA will write directly to LVGL framebuffer");
  }
  
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

  ppa_srm_oper_config_t srm_config = {};
  
  srm_config.in.buffer = (void*)src;
  srm_config.in.pic_w = this->width_;
  srm_config.in.pic_h = this->height_;
  srm_config.in.block_w = this->width_;
  srm_config.in.block_h = this->height_;
  srm_config.in.block_offset_x = 0;
  srm_config.in.block_offset_y = 0;
  srm_config.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  uint16_t out_w = (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) 
                   ? this->height_ : this->width_;
  uint16_t out_h = (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270)
                   ? this->width_ : this->height_;
  
  srm_config.out.buffer = dst;
  srm_config.out.buffer_size = out_w * out_h * 2;
  srm_config.out.pic_w = out_w;
  srm_config.out.pic_h = out_h;
  srm_config.out.block_offset_x = 0;
  srm_config.out.block_offset_y = 0;
  srm_config.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  srm_config.rotation_angle = (ppa_srm_rotation_angle_t)this->rotation_;
  srm_config.scale_x = 1.0f;
  srm_config.scale_y = 1.0f;
  srm_config.mirror_x = this->mirror_x_;
  srm_config.mirror_y = this->mirror_y_;
  srm_config.rgb_swap = false;
  srm_config.byte_swap = false;
  srm_config.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
  srm_config.alpha_fix_val = 0xFF;
  srm_config.mode = PPA_TRANS_MODE_BLOCKING;
  
  esp_err_t ret = ppa_do_scale_rotate_mirror(this->ppa_handle_, &srm_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA transform failed: 0x%x", ret);
    return false;
  }
  
  return true;
}

bool LVGLCameraDisplay::init_direct_mode_() {
  ESP_LOGI(TAG, "Initializing direct display mode...");
  
  this->lvgl_display_ = lv_disp_get_default();
  if (!this->lvgl_display_) {
    ESP_LOGE(TAG, "No LVGL display found");
    return false;
  }
  
  this->lvgl_draw_buf_ = lv_disp_get_draw_buf(this->lvgl_display_);
  if (!this->lvgl_draw_buf_) {
    ESP_LOGE(TAG, "No LVGL draw buffer found");
    return false;
  }
  
  this->lvgl_framebuffer_ = (uint8_t*)this->lvgl_draw_buf_->buf1;
  if (!this->lvgl_framebuffer_) {
    ESP_LOGE(TAG, "No LVGL framebuffer found");
    return false;
  }
  
  uint16_t fb_width = this->width_;
  uint16_t fb_height = this->height_;
  
  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    std::swap(fb_width, fb_height);
  }
  
  this->lvgl_framebuffer_size_ = fb_width * fb_height * 2;
  
  uintptr_t fb_addr = (uintptr_t)this->lvgl_framebuffer_;
  bool is_aligned = (fb_addr % 64 == 0);
  bool size_aligned = (this->lvgl_framebuffer_size_ % 64 == 0);
  
  if (!is_aligned || !size_aligned) {
    ESP_LOGW(TAG, "⚠️  Framebuffer alignment issue:");
    ESP_LOGW(TAG, "   Address: %p (aligned: %s)", 
             this->lvgl_framebuffer_, is_aligned ? "YES" : "NO");
    ESP_LOGW(TAG, "   Size: %u bytes (aligned: %s)", 
             this->lvgl_framebuffer_size_, size_aligned ? "YES" : "NO");
    
    this->aligned_buffer_size_ = (this->lvgl_framebuffer_size_ + 63) & ~63;
    this->aligned_buffer_ = (uint8_t*)heap_caps_aligned_alloc(
      64,
      this->aligned_buffer_size_,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED
    );
    
    if (!this->aligned_buffer_) {
      ESP_LOGE(TAG, "❌ Failed to allocate aligned buffer");
      return false;
    }
    
    ESP_LOGI(TAG, "✅ Created aligned intermediate buffer (%u bytes)", 
             this->aligned_buffer_size_);
  }
  
  ESP_LOGI(TAG, "✅ Direct mode ready:");
  ESP_LOGI(TAG, "   Framebuffer: %p (%u bytes)", 
           this->lvgl_framebuffer_, this->lvgl_framebuffer_size_);
  ESP_LOGI(TAG, "   Resolution: %ux%u", fb_width, fb_height);
  ESP_LOGI(TAG, "   Alignment: %s%s", 
           is_aligned ? "64-byte (optimal)" : "UNALIGNED", 
           this->aligned_buffer_ ? " - using intermediate buffer" : "");
  
  return true;
}

void LVGLCameraDisplay::update_direct_mode_() {
  // ⚠️ Cette fonction n'est plus utilisée avec la tâche dédiée
  ESP_LOGW(TAG, "update_direct_mode_() should not be called with task-based capture");
}

void LVGLCameraDisplay::update_canvas_mode_() {
  // ⚠️ Cette fonction n'est plus utilisée avec la tâche dédiée
  ESP_LOGW(TAG, "update_canvas_mode_() should not be called with task-based capture");
}

void LVGLCameraDisplay::stop_capture() {
  ESP_LOGI(TAG, "Stopping capture task...");
  this->task_running_ = false;
  
  if (this->capture_task_handle_) {
    // Attendre que la tâche se termine (max 2 secondes)
    uint32_t timeout = 2000;
    uint32_t start = millis();
    while (eTaskGetState(this->capture_task_handle_) != eDeleted && 
           (millis() - start) < timeout) {
      delay(10);
    }
    this->capture_task_handle_ = nullptr;
  }
  
  this->cleanup_v4l2_();
  
  if (this->display_mutex_) {
    vSemaphoreDelete(this->display_mutex_);
    this->display_mutex_ = nullptr;
  }
  
  ESP_LOGI(TAG, "✅ Capture stopped");
}

#endif

// ✅ loop() ne fait plus rien - tout est dans la tâche
void LVGLCameraDisplay::loop() {
  // La capture se fait dans la tâche dédiée
  // Cette fonction est vide maintenant
}

void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "LVGL Camera Display (Task-based):");
  ESP_LOGCONFIG(TAG, "  Camera: %s", this->camera_ ? "Connected" : "Not connected");
  ESP_LOGCONFIG(TAG, "  Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGCONFIG(TAG, "  Update interval: %u ms", this->update_interval_);
  ESP_LOGCONFIG(TAG, "  Rotation: %d°", this->rotation_);
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", this->mirror_x_ ? "ON" : "OFF");
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", this->mirror_y_ ? "ON" : "OFF");
  ESP_LOGCONFIG(TAG, "  Display mode: %s", this->direct_mode_ ? "DIRECT (hardware)" : "CANVAS (software)");
  ESP_LOGCONFIG(TAG, "  PPA: %s", this->use_ppa_ ? "ENABLED" : "DISABLED");
  ESP_LOGCONFIG(TAG, "  Capture task: %s", this->task_running_ ? "RUNNING" : "STOPPED");
#ifdef USE_ESP32_VARIANT_ESP32P4
  ESP_LOGCONFIG(TAG, "  V4L2 Device: %s", this->video_device_);
  ESP_LOGCONFIG(TAG, "  V4L2 FD: %d", this->video_fd_);
#endif
}

void LVGLCameraDisplay::configure_canvas(lv_obj_t *canvas) {
  this->canvas_obj_ = canvas;
  ESP_LOGI(TAG, "Canvas configured for camera display");
}

}  // namespace lvgl_camera_display
}  // namespace esphome









