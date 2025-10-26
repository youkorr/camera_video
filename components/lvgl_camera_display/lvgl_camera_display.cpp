#include "lvgl_camera_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

void LVGLCameraDisplay::setup() {
  ESP_LOGCONFIG(TAG, "🎥 LVGL Camera Display (Direct Mode)");

#ifdef USE_ESP32_VARIANT_ESP32P4
  // Récupérer la résolution depuis la caméra
  if (this->camera_) {
    this->width_ = this->camera_->get_image_width();
    this->height_ = this->camera_->get_image_height();
    ESP_LOGI(TAG, "📐 Using camera resolution: %ux%u", this->width_, this->height_);
  } else {
    ESP_LOGW(TAG, "⚠️  No camera linked");
    this->mark_failed();
    return;
  }

  // S'assurer que la caméra est initialisée et en streaming
  if (!this->camera_->is_streaming()) {
    ESP_LOGI(TAG, "Starting camera streaming...");
    if (!this->camera_->start_streaming()) {
      ESP_LOGE(TAG, "❌ Failed to start camera streaming");
      this->mark_failed();
      return;
    }
  }

  // Attendre que la caméra soit stable
  delay(200);

  // Initialiser PPA si transformations nécessaires
  if (this->mirror_x_ || this->mirror_y_ || this->rotation_ != ROTATION_0) {
    if (!this->init_ppa_()) {
      ESP_LOGE(TAG, "❌ Failed to initialize PPA");
      this->mark_failed();
      return;
    }
    ESP_LOGI(TAG, "✅ PPA initialized (rotation=%d°, mirror_x=%s, mirror_y=%s)",
             this->rotation_, this->mirror_x_ ? "ON" : "OFF", 
             this->mirror_y_ ? "ON" : "OFF");
  }

  ESP_LOGI(TAG, "✅ Camera display ready");
  ESP_LOGI(TAG, "   Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGI(TAG, "   Target FPS: %.1f", 1000.0f / this->update_interval_);
  ESP_LOGI(TAG, "   Mirror X: %s", this->mirror_x_ ? "ON" : "OFF");
#else
  ESP_LOGE(TAG, "❌ Requires ESP32-P4");
  this->mark_failed();
#endif
}

#ifdef USE_ESP32_VARIANT_ESP32P4

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

  if (!this->transform_buffer_) {
    ESP_LOGE(TAG, "Failed to allocate transform buffer");
    ppa_unregister_client(this->ppa_handle_);
    this->ppa_handle_ = nullptr;
    return false;
  }

  ESP_LOGI(TAG, "PPA transform buffer: %ux%u @ %u bytes", 
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

  // Configuration PPA identique à la démo M5Stack
  ppa_srm_oper_config_t srm_config = {};
  
  // Configuration source
  srm_config.in.buffer = (void*)src;
  srm_config.in.pic_w = this->width_;
  srm_config.in.pic_h = this->height_;
  srm_config.in.block_w = this->width_;
  srm_config.in.block_h = this->height_;
  srm_config.in.block_offset_x = 0;
  srm_config.in.block_offset_y = 0;
  srm_config.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  // Configuration destination
  uint16_t out_w = (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) 
                   ? this->height_ : this->width_;
  uint16_t out_h = (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270)
                   ? this->width_ : this->height_;
  
  srm_config.out.buffer = dst;
  srm_config.out.buffer_size = this->transform_buffer_size_;
  srm_config.out.pic_w = out_w;
  srm_config.out.pic_h = out_h;
  srm_config.out.block_offset_x = 0;
  srm_config.out.block_offset_y = 0;
  srm_config.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  // Configuration rotation et miroir (comme la démo M5Stack)
  srm_config.rotation_angle = (ppa_srm_rotation_angle_t)this->rotation_;
  srm_config.scale_x = 1.0f;
  srm_config.scale_y = 1.0f;
  srm_config.mirror_x = this->mirror_x_;
  srm_config.mirror_y = this->mirror_y_;
  srm_config.rgb_swap = false;
  srm_config.byte_swap = false;
  srm_config.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
  srm_config.alpha_fix_val = 0xFF;
  
  // Mode bloquant comme dans la démo
  srm_config.mode = PPA_TRANS_MODE_BLOCKING;
  
  // Exécuter la transformation
  esp_err_t ret = ppa_do_scale_rotate_mirror(this->ppa_handle_, &srm_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA transform failed: 0x%x", ret);
    return false;
  }
  
  return true;
}

#endif

void LVGLCameraDisplay::loop() {
#ifdef USE_ESP32_VARIANT_ESP32P4
  if (!this->camera_ || !this->camera_->is_streaming()) {
    return;
  }

  uint32_t now = millis();
  if (now - this->last_update_time_ < this->update_interval_) {
    return;
  }
  this->last_update_time_ = now;

  // Capturer une frame depuis la caméra (comme la démo M5Stack)
  if (!this->camera_->capture_frame()) {
    this->drop_count_++;
    return;
  }

  uint8_t *frame_data = this->camera_->get_image_data();
  if (!frame_data) {
    return;
  }

  // Appliquer les transformations PPA si nécessaire
  uint8_t *display_buffer = frame_data;
  
  if (this->ppa_handle_ && this->transform_buffer_) {
    if (this->transform_frame_(frame_data, this->transform_buffer_)) {
      display_buffer = this->transform_buffer_;
    }
  }

  // Afficher sur le canvas LVGL (exactement comme la démo M5Stack)
  if (this->canvas_obj_) {
    uint16_t canvas_width = (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270)
                            ? this->height_ : this->width_;
    uint16_t canvas_height = (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270)
                             ? this->width_ : this->height_;

    // Lock LVGL avant modification (comme la démo avec bsp_display_lock)
    lv_disp_t *disp = lv_obj_get_disp(this->canvas_obj_);
    if (disp) {
      _lv_disp_refr_timer(NULL);
    }

    // LVGL v8/v9 compatible : utiliser LV_IMG_CF_TRUE_COLOR pour RGB565
    lv_canvas_set_buffer(this->canvas_obj_, display_buffer, 
                         canvas_width, canvas_height, LV_IMG_CF_TRUE_COLOR);
    lv_obj_invalidate(this->canvas_obj_);
  }

  this->frame_count_++;

  // Log FPS périodiquement
  if (this->first_update_) {
    this->first_update_ = false;
    this->last_fps_time_ = now;
  } else if (now - this->last_fps_time_ >= 5000) {
    float fps = this->frame_count_ * 1000.0f / (now - this->last_fps_time_);
    float drop_rate = (this->drop_count_ * 100.0f) / (this->frame_count_ + this->drop_count_);
    ESP_LOGI(TAG, "📊 Display: %.1f FPS | Drops: %u (%.1f%%)", 
             fps, this->drop_count_, drop_rate);
    this->frame_count_ = 0;
    this->drop_count_ = 0;
    this->last_fps_time_ = now;
  }
#endif
}

void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "LVGL Camera Display:");
  ESP_LOGCONFIG(TAG, "  Camera: %s", this->camera_ ? "Connected" : "Not connected");
  ESP_LOGCONFIG(TAG, "  Resolution: %ux%u", this->width_, this->height_);
  ESP_LOGCONFIG(TAG, "  Update interval: %u ms", this->update_interval_);
  ESP_LOGCONFIG(TAG, "  Rotation: %d°", this->rotation_);
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", this->mirror_x_ ? "ON" : "OFF");
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", this->mirror_y_ ? "ON" : "OFF");
#ifdef USE_ESP32_VARIANT_ESP32P4
  ESP_LOGCONFIG(TAG, "  PPA: %s", this->ppa_handle_ ? "Enabled" : "Disabled");
#endif
}

void LVGLCameraDisplay::configure_canvas(lv_obj_t *canvas) {
  this->canvas_obj_ = canvas;
  ESP_LOGI(TAG, "Canvas configured for camera display");
}

}  // namespace lvgl_camera_display
}  // namespace esphome








