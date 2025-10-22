#include "lvgl_camera_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "../mipi_dsi_cam/esp_video_buffer.h"

namespace esphome {
namespace lvgl_camera_display {

static const char *const TAG = "lvgl_camera_display";

void LVGLCameraDisplay::setup() {
  ESP_LOGCONFIG(TAG, "🎥 LVGL Camera Display (Optimized with Video Buffer)");

  if (this->camera_ == nullptr) {
    ESP_LOGE(TAG, "❌ Camera not configured");
    this->mark_failed();
    return;
  }

  // Initialiser le système de video buffer pour double buffering
  if (!this->init_video_buffer_()) {
    ESP_LOGE(TAG, "❌ Video buffer initialization failed");
    this->mark_failed();
    return;
  }

#ifdef USE_ESP32_VARIANT_ESP32P4
  // Initialiser PPA si rotation/mirror est activé
  if (this->rotation_ != ROTATION_0 || this->mirror_x_ || this->mirror_y_) {
    if (!this->init_ppa_()) {
      ESP_LOGE(TAG, "❌ PPA initialization failed");
      this->mark_failed();
      return;
    }
    ESP_LOGI(TAG, "✅ PPA initialized (rotation=%d°, mirror_x=%s, mirror_y=%s)",
             this->rotation_, this->mirror_x_ ? "ON" : "OFF", 
             this->mirror_y_ ? "ON" : "OFF");
  }
#endif

  // Initialiser les encoders si activés
  if (this->jpeg_output_) {
    this->init_jpeg_encoder_();
  }
  if (this->h264_output_) {
    this->init_h264_encoder_();
  }

  ESP_LOGI(TAG, "✅ Display initialized");
  ESP_LOGI(TAG, "   Update interval: %u ms", this->update_interval_);
  ESP_LOGI(TAG, "   Video buffers: %d (double buffering)", VIDEO_BUFFER_COUNT);
}

bool LVGLCameraDisplay::init_video_buffer_() {
  uint16_t width = this->camera_->get_image_width();
  uint16_t height = this->camera_->get_image_height();
  
  // Configuration du video buffer (double buffering)
  struct esp_video_buffer_info buffer_info = {
    .count = VIDEO_BUFFER_COUNT,  // 2 ou 3 buffers
    .size = width * height * 2,   // RGB565 = 2 bytes/pixel
    .caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
    .align_size = 64,  // Alignement pour DMA
    .memory_type = V4L2_MEMORY_MMAP
  };
  
  this->video_buffer_ = esp_video_buffer_create(&buffer_info);
  if (!this->video_buffer_) {
    ESP_LOGE(TAG, "Failed to create video buffer");
    return false;
  }
  
  ESP_LOGI(TAG, "✅ Video buffer created: %d buffers of %zu bytes", 
           VIDEO_BUFFER_COUNT, buffer_info.size);
  
  // Initialiser l'index du buffer actif
  this->current_buffer_index_ = 0;
  
  return true;
}

void LVGLCameraDisplay::cleanup_video_buffer_() {
  if (this->video_buffer_) {
    esp_video_buffer_destroy(this->video_buffer_);
    this->video_buffer_ = nullptr;
  }
}

#ifdef USE_ESP32_VARIANT_ESP32P4
bool LVGLCameraDisplay::init_ppa_() {
  // Configuration du PPA (inchangée)
  ppa_client_config_t ppa_config = {
    .oper_type = PPA_OPERATION_SRM,
    .max_pending_trans_num = 1,
  };
  
  esp_err_t ret = ppa_register_client(&ppa_config, &this->ppa_handle_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA register failed: 0x%x", ret);
    return false;
  }
  
  uint16_t width = this->camera_->get_image_width();
  uint16_t height = this->camera_->get_image_height();
  
  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    std::swap(width, height);
  }
  
  this->transform_buffer_size_ = width * height * 2;
  this->transform_buffer_ = (uint8_t*)heap_caps_aligned_alloc(
    64, this->transform_buffer_size_, MALLOC_CAP_SPIRAM
  );
  
  if (!this->transform_buffer_) {
    ESP_LOGE(TAG, "Failed to allocate transform buffer");
    ppa_unregister_client(this->ppa_handle_);
    this->ppa_handle_ = nullptr;
    return false;
  }
  
  ESP_LOGI(TAG, "PPA transform buffer allocated: %ux%u", width, height);
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
  
  uint16_t src_width = this->camera_->get_image_width();
  uint16_t src_height = this->camera_->get_image_height();
  
  uint16_t dst_width = src_width;
  uint16_t dst_height = src_height;
  
  if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
    std::swap(dst_width, dst_height);
  }
  
  ppa_srm_rotation_angle_t ppa_rotation;
  switch (this->rotation_) {
    case ROTATION_0:   ppa_rotation = PPA_SRM_ROTATION_ANGLE_0; break;
    case ROTATION_90:  ppa_rotation = PPA_SRM_ROTATION_ANGLE_90; break;
    case ROTATION_180: ppa_rotation = PPA_SRM_ROTATION_ANGLE_180; break;
    case ROTATION_270: ppa_rotation = PPA_SRM_ROTATION_ANGLE_270; break;
    default:           ppa_rotation = PPA_SRM_ROTATION_ANGLE_0; break;
  }
  
  ppa_srm_oper_config_t srm_config = {};
  
  srm_config.in.buffer = const_cast<uint8_t*>(src);
  srm_config.in.pic_w = src_width;
  srm_config.in.pic_h = src_height;
  srm_config.in.block_w = src_width;
  srm_config.in.block_h = src_height;
  srm_config.in.block_offset_x = 0;
  srm_config.in.block_offset_y = 0;
  srm_config.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  srm_config.out.buffer = dst;
  srm_config.out.buffer_size = this->transform_buffer_size_;
  srm_config.out.pic_w = dst_width;
  srm_config.out.pic_h = dst_height;
  srm_config.out.block_offset_x = 0;
  srm_config.out.block_offset_y = 0;
  srm_config.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  srm_config.rotation_angle = ppa_rotation;
  srm_config.scale_x = 1.0f;
  srm_config.scale_y = 1.0f;
  srm_config.mirror_x = this->mirror_x_;
  srm_config.mirror_y = this->mirror_y_;
  srm_config.rgb_swap = false;
  srm_config.byte_swap = false;
  srm_config.mode = PPA_TRANS_MODE_BLOCKING;
  
  esp_err_t ret = ppa_do_scale_rotate_mirror(this->ppa_handle_, &srm_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "PPA transformation failed: 0x%x", ret);
    return false;
  }
  
  return true;
}
#endif

void LVGLCameraDisplay::loop() {
  if (!this->camera_->is_streaming() || !this->video_buffer_) {
    return;
  }
  
  uint32_t now = millis();
  
  // Throttle la capture selon l'intervalle configuré
  if (now - this->last_update_time_ < this->update_interval_) {
    return;
  }
  
  // Capturer la frame dans le buffer vidéo
  if (this->capture_to_video_buffer_()) {
    this->update_canvas_();
    this->frame_count_++;
    this->last_update_time_ = now;

    // Logger FPS toutes les 100 frames
    if (this->frame_count_ % 100 == 0) {
      if (this->last_fps_time_ > 0) {
        float elapsed = (now - this->last_fps_time_) / 1000.0f;
        float fps = 100.0f / elapsed;
        ESP_LOGI(TAG, "🎞️  Display FPS: %.2f | %u frames total", fps, this->frame_count_);
      }
      this->last_fps_time_ = now;
    }
  }
}

bool LVGLCameraDisplay::capture_to_video_buffer_() {
  if (!this->video_buffer_) {
    return false;
  }
  
  // Récupérer un buffer libre
  struct esp_video_buffer_element *element = nullptr;
  for (int i = 0; i < this->video_buffer_->info.count; i++) {
    if (ELEMENT_IS_FREE(&this->video_buffer_->element[i])) {
      element = &this->video_buffer_->element[i];
      ELEMENT_SET_QUEUED(element);
      break;
    }
  }
  
  if (!element) {
    // Aucun buffer libre, skip cette frame
    return false;
  }
  
  // Capturer depuis la caméra
  if (!this->camera_->capture_frame()) {
    ELEMENT_SET_FREE(element);
    return false;
  }
  
  uint8_t* camera_data = this->camera_->get_image_data();
  if (!camera_data) {
    ELEMENT_SET_FREE(element);
    return false;
  }
  
  // Copier les données dans le buffer vidéo
  size_t copy_size = this->video_buffer_->info.size;
  memcpy(element->buffer, camera_data, copy_size);
  element->valid_size = copy_size;
  
  // Marquer comme prêt pour l'affichage
  ELEMENT_SET_ACTIVE(element);
  
  // Mettre à jour l'index du buffer actif
  this->current_buffer_index_ = element->index;
  
  return true;
}

void LVGLCameraDisplay::update_canvas_() {
  if (this->camera_ == nullptr || this->canvas_obj_ == nullptr || !this->video_buffer_) {
    if (!this->canvas_warning_shown_) {
      ESP_LOGW(TAG, "❌ Canvas or video buffer null");
      this->canvas_warning_shown_ = true;
    }
    return;
  }

  // Récupérer le buffer actif
  struct esp_video_buffer_element *element = 
    &this->video_buffer_->element[this->current_buffer_index_];
  
  if (!ELEMENT_IS_ACTIVE(element)) {
    return;
  }

  uint8_t* img_data = element->buffer;
  uint16_t width = this->camera_->get_image_width();
  uint16_t height = this->camera_->get_image_height();

  if (this->first_update_) {
    ESP_LOGI(TAG, "🖼️  First canvas update:");
    ESP_LOGI(TAG, "   Dimensions: %ux%u", width, height);
    ESP_LOGI(TAG, "   Buffer: %p", img_data);
    ESP_LOGI(TAG, "   Using video buffer index: %d", this->current_buffer_index_);
    this->first_update_ = false;
  }

#ifdef USE_ESP32_VARIANT_ESP32P4
  // Transformation PPA si nécessaire
  if (this->ppa_handle_ && this->transform_buffer_) {
    if (this->transform_frame_(img_data, this->transform_buffer_)) {
      img_data = this->transform_buffer_;
      
      if (this->rotation_ == ROTATION_90 || this->rotation_ == ROTATION_270) {
        std::swap(width, height);
      }
    }
  }
#endif

  // Encoder en JPEG/H264 si activé (en tâche de fond pour ne pas bloquer)
  if (this->jpeg_output_) {
    this->encode_jpeg_async_(img_data, width * height * 2);
  }
  if (this->h264_output_) {
    this->encode_h264_async_(img_data, width * height * 2);
  }

  // Mettre à jour le canvas LVGL uniquement si le buffer a changé
  if (this->last_display_buffer_ != img_data) {
    #if LV_VERSION_CHECK(9, 0, 0)
      lv_canvas_set_buffer(this->canvas_obj_, img_data, width, height, 
                           LV_COLOR_FORMAT_RGB565);
    #else
      lv_canvas_set_buffer(this->canvas_obj_, img_data, width, height, 
                           LV_IMG_CF_TRUE_COLOR);
    #endif
    
    this->last_display_buffer_ = img_data;
    
    // Invalider seulement après changement de buffer
    lv_obj_invalidate(this->canvas_obj_);
  }
}

void LVGLCameraDisplay::init_jpeg_encoder_() {
#ifdef MIPI_DSI_CAM_ENABLE_JPEG
  if (this->jpeg_encoder_) {
    return;
  }
  
  this->jpeg_encoder_ = new MipiDsiCamJPEGEncoder(this->camera_);
  if (this->jpeg_encoder_->init(this->jpeg_quality_) == ESP_OK) {
    ESP_LOGI(TAG, "✅ JPEG encoder initialized (quality=%u)", this->jpeg_quality_);
  } else {
    delete this->jpeg_encoder_;
    this->jpeg_encoder_ = nullptr;
    ESP_LOGE(TAG, "❌ JPEG encoder initialization failed");
  }
#endif
}

void LVGLCameraDisplay::init_h264_encoder_() {
#ifdef MIPI_DSI_CAM_ENABLE_H264
  if (this->h264_encoder_) {
    return;
  }
  
  this->h264_encoder_ = new MipiDsiCamH264Encoder(this->camera_);
  if (this->h264_encoder_->init(this->h264_bitrate_, this->h264_gop_size_) == ESP_OK) {
    ESP_LOGI(TAG, "✅ H264 encoder initialized (bitrate=%u)", this->h264_bitrate_);
  } else {
    delete this->h264_encoder_;
    this->h264_encoder_ = nullptr;
    ESP_LOGE(TAG, "❌ H264 encoder initialization failed");
  }
#endif
}

void LVGLCameraDisplay::encode_jpeg_async_(const uint8_t *data, size_t size) {
#ifdef MIPI_DSI_CAM_ENABLE_JPEG
  if (!this->jpeg_encoder_) {
    return;
  }
  
  // Encoder en JPEG (asynchrone pour ne pas bloquer l'affichage)
  uint8_t *jpeg_data = nullptr;
  size_t jpeg_size = 0;
  
  if (this->jpeg_encoder_->encode_frame(data, size, &jpeg_data, &jpeg_size) == ESP_OK) {
    // Publier via sensor/callback si implémenté
    // TODO: Appeler un callback ou publier les données JPEG
  }
#endif
}

void LVGLCameraDisplay::encode_h264_async_(const uint8_t *data, size_t size) {
#ifdef MIPI_DSI_CAM_ENABLE_H264
  if (!this->h264_encoder_) {
    return;
  }
  
  uint8_t *h264_data = nullptr;
  size_t h264_size = 0;
  bool is_keyframe = false;
  
  if (this->h264_encoder_->encode_frame(data, size, &h264_data, &h264_size, &is_keyframe) == ESP_OK) {
    // Publier via sensor/callback si implémenté
    // TODO: Appeler un callback ou publier les données H264
  }
#endif
}

void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "LVGL Camera Display:");
  ESP_LOGCONFIG(TAG, "  Mode: Double buffering with esp_video_buffer");
  ESP_LOGCONFIG(TAG, "  Buffers: %d", VIDEO_BUFFER_COUNT);
  ESP_LOGCONFIG(TAG, "  Canvas: %s", this->canvas_obj_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Rotation: %d°", this->rotation_);
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", this->mirror_x_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", this->mirror_y_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  JPEG Output: %s", this->jpeg_output_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  H264 Output: %s", this->h264_output_ ? "YES" : "NO");
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
    
    // Activer le cache de rendu si disponible (LVGL v9)
    #if LV_VERSION_CHECK(9, 0, 0)
      lv_obj_set_style_bg_opa(canvas, LV_OPA_TRANSP, 0);
    #endif
  }
}

}  // namespace lvgl_camera_display
}  // namespace esphome
