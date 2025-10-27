#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_camera/esp32_camera.h"
#include "lvgl.h"

#ifdef USE_PPA_ACCELERATION
#include "esp_ppa.h"
#endif

namespace esphome {
namespace lvgl_camera_display {

class LVGLCameraDisplay : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_camera(esp32_camera::ESP32Camera *camera) { camera_ = camera; }
  void set_update_interval(uint32_t interval) { update_interval_ = interval; }
  void set_direct_mode(bool direct) { direct_mode_ = direct; }
  void set_use_ppa(bool use_ppa) { use_ppa_ = use_ppa; }
  void set_rotation(uint16_t rotation) { rotation_ = rotation; }
  void set_mirror_x(bool mirror) { mirror_x_ = mirror; }
  void set_mirror_y(bool mirror) { mirror_y_ = mirror; }

 protected:
  void init_lvgl_display_();
  void update_frame_();
  void process_frame_with_ppa_(camera::CameraImage *image);
  void process_frame_cpu_(camera::CameraImage *image);

  esp32_camera::ESP32Camera *camera_{nullptr};
  lv_obj_t *img_obj_{nullptr};
  lv_img_dsc_t *img_dsc_{nullptr};
  
  uint32_t update_interval_{33}; // ~30 FPS par défaut
  uint32_t last_update_{0};
  
  bool direct_mode_{true};
  bool use_ppa_{true};
  uint16_t rotation_{0};
  bool mirror_x_{false};
  bool mirror_y_{true};
  
  uint8_t *framebuffer_{nullptr};
  size_t framebuffer_size_{0};

#ifdef USE_PPA_ACCELERATION
  ppa_client_handle_t ppa_client_{nullptr};
  ppa_srm_oper_config_t srm_config_{};
#endif
};

void LVGLCameraDisplay::setup() {
  ESP_LOGI("lvgl_camera_display", "Setting up LVGL Camera Display...");
  
  if (camera_ == nullptr) {
    ESP_LOGE("lvgl_camera_display", "Camera not configured!");
    this->mark_failed();
    return;
  }

#ifdef USE_PPA_ACCELERATION
  if (use_ppa_) {
    ESP_LOGI("lvgl_camera_display", "Initializing PPA acceleration...");
    
    ppa_client_config_t ppa_cfg = {
      .oper_type = PPA_OPERATION_SRM,
      .max_pending_trans_num = 1,
    };
    
    esp_err_t ret = ppa_register_client(&ppa_cfg, &ppa_client_);
    if (ret != ESP_OK) {
      ESP_LOGW("lvgl_camera_display", "PPA initialization failed: %d, falling back to CPU", ret);
      use_ppa_ = false;
    } else {
      ESP_LOGI("lvgl_camera_display", "PPA client registered successfully");
    }
  }
#else
  if (use_ppa_) {
    ESP_LOGW("lvgl_camera_display", "PPA not available, USE_PPA_ACCELERATION not defined");
    use_ppa_ = false;
  }
#endif

  // Initialiser le display LVGL
  this->init_lvgl_display_();
}

void LVGLCameraDisplay::init_lvgl_display_() {
  ESP_LOGI("lvgl_camera_display", "Initializing LVGL display objects...");
  
  // Créer l'objet image LVGL
  img_obj_ = lv_img_create(lv_scr_act());
  
  if (img_obj_ == nullptr) {
    ESP_LOGE("lvgl_camera_display", "Failed to create LVGL image object!");
    return;
  }

  // Allouer le descripteur d'image
  img_dsc_ = (lv_img_dsc_t *)heap_caps_malloc(sizeof(lv_img_dsc_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  
  if (img_dsc_ == nullptr) {
    ESP_LOGE("lvgl_camera_display", "Failed to allocate image descriptor!");
    return;
  }

  // Configuration initiale (sera mise à jour avec la première frame)
  img_dsc_->header.cf = LV_IMG_CF_TRUE_COLOR;
  img_dsc_->header.always_zero = 0;
  img_dsc_->header.reserved = 0;
  img_dsc_->header.w = 0;
  img_dsc_->header.h = 0;
  img_dsc_->data = nullptr;
  img_dsc_->data_size = 0;

  lv_img_set_src(img_obj_, img_dsc_);
  
  // Appliquer la rotation
  lv_img_set_angle(img_obj_, rotation_ * 10); // LVGL utilise 0.1 degré comme unité
  
  ESP_LOGI("lvgl_camera_display", "LVGL display initialized (direct_mode=%d, use_ppa=%d)", 
           direct_mode_, use_ppa_);
}

void LVGLCameraDisplay::loop() {
  uint32_t now = millis();
  
  // Limitation du framerate
  if (now - last_update_ < update_interval_) {
    return;
  }
  
  last_update_ = now;
  this->update_frame_();
}

void LVGLCameraDisplay::update_frame_() {
  if (camera_ == nullptr || img_obj_ == nullptr) {
    return;
  }

  // Récupérer une frame de la caméra
  auto image = camera_->get_current_image();
  
  if (!image) {
    return;
  }

  // Traiter la frame selon le mode
  if (use_ppa_ && direct_mode_) {
#ifdef USE_PPA_ACCELERATION
    this->process_frame_with_ppa_(image.get());
#else
    this->process_frame_cpu_(image.get());
#endif
  } else {
    this->process_frame_cpu_(image.get());
  }
}

#ifdef USE_PPA_ACCELERATION
void LVGLCameraDisplay::process_frame_with_ppa_(camera::CameraImage *image) {
  if (ppa_client_ == nullptr || image == nullptr) {
    return;
  }

  size_t len = image->get_data_length();
  const uint8_t *data = image->get_data_buffer();
  
  if (data == nullptr || len == 0) {
    return;
  }

  // Allouer le framebuffer si nécessaire
  if (framebuffer_ == nullptr || framebuffer_size_ != len) {
    if (framebuffer_ != nullptr) {
      heap_caps_free(framebuffer_);
    }
    
    framebuffer_ = (uint8_t *)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    framebuffer_size_ = len;
    
    if (framebuffer_ == nullptr) {
      ESP_LOGE("lvgl_camera_display", "Failed to allocate framebuffer!");
      return;
    }
  }

  // Configuration de la transformation PPA
  memset(&srm_config_, 0, sizeof(ppa_srm_oper_config_t));
  
  srm_config_.in.buffer = (void *)data;
  srm_config_.in.pic_w = image->get_width();
  srm_config_.in.pic_h = image->get_height();
  srm_config_.in.block_w = image->get_width();
  srm_config_.in.block_h = image->get_height();
  srm_config_.in.block_offset_x = 0;
  srm_config_.in.block_offset_y = 0;
  srm_config_.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  srm_config_.out.buffer = (void *)framebuffer_;
  srm_config_.out.buffer_size = framebuffer_size_;
  srm_config_.out.pic_w = image->get_width();
  srm_config_.out.pic_h = image->get_height();
  srm_config_.out.block_offset_x = 0;
  srm_config_.out.block_offset_y = 0;
  srm_config_.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

  // Appliquer les transformations mirror
  srm_config_.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  srm_config_.scale_x = 1.0f;
  srm_config_.scale_y = 1.0f;
  srm_config_.mirror_x = mirror_x_;
  srm_config_.mirror_y = mirror_y_;

  // Appliquer la rotation
  if (rotation_ == 90) {
    srm_config_.rotation_angle = PPA_SRM_ROTATION_ANGLE_90;
  } else if (rotation_ == 180) {
    srm_config_.rotation_angle = PPA_SRM_ROTATION_ANGLE_180;
  } else if (rotation_ == 270) {
    srm_config_.rotation_angle = PPA_SRM_ROTATION_ANGLE_270;
  }

  // Lancer la transformation PPA
  esp_err_t ret = ppa_do_scale_rotate_mirror(ppa_client_, &srm_config_);
  
  if (ret != ESP_OK) {
    ESP_LOGW("lvgl_camera_display", "PPA operation failed: %d", ret);
    return;
  }

  // Mettre à jour le descripteur d'image LVGL
  img_dsc_->header.w = image->get_width();
  img_dsc_->header.h = image->get_height();
  img_dsc_->data = framebuffer_;
  img_dsc_->data_size = framebuffer_size_;

  // Forcer LVGL à redessiner
  lv_img_set_src(img_obj_, img_dsc_);
  lv_obj_invalidate(img_obj_);
}
#endif

void LVGLCameraDisplay::process_frame_cpu_(camera::CameraImage *image) {
  if (image == nullptr) {
    return;
  }

  size_t len = image->get_data_length();
  const uint8_t *data = image->get_data_buffer();
  
  if (data == nullptr || len == 0) {
    return;
  }

  // Allouer le framebuffer si nécessaire
  if (framebuffer_ == nullptr || framebuffer_size_ != len) {
    if (framebuffer_ != nullptr) {
      heap_caps_free(framebuffer_);
    }
    
    framebuffer_ = (uint8_t *)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    framebuffer_size_ = len;
    
    if (framebuffer_ == nullptr) {
      ESP_LOGE("lvgl_camera_display", "Failed to allocate framebuffer!");
      return;
    }
  }

  // Copie CPU simple (peut être optimisée avec des transformations manuelles)
  memcpy(framebuffer_, data, len);

  // TODO: Implémenter les transformations CPU pour mirror_x, mirror_y si nécessaire
  
  // Mettre à jour le descripteur d'image LVGL
  img_dsc_->header.w = image->get_width();
  img_dsc_->header.h = image->get_height();
  img_dsc_->data = framebuffer_;
  img_dsc_->data_size = framebuffer_size_;

  // Forcer LVGL à redessiner
  lv_img_set_src(img_obj_, img_dsc_);
  lv_obj_invalidate(img_obj_);
}

void LVGLCameraDisplay::dump_config() {
  ESP_LOGCONFIG("lvgl_camera_display", "LVGL Camera Display:");
  ESP_LOGCONFIG("lvgl_camera_display", "  Update Interval: %u ms", update_interval_);
  ESP_LOGCONFIG("lvgl_camera_display", "  Direct Mode: %s", direct_mode_ ? "YES" : "NO");
  ESP_LOGCONFIG("lvgl_camera_display", "  Use PPA: %s", use_ppa_ ? "YES" : "NO");
  ESP_LOGCONFIG("lvgl_camera_display", "  Rotation: %d°", rotation_);
  ESP_LOGCONFIG("lvgl_camera_display", "  Mirror X: %s", mirror_x_ ? "YES" : "NO");
  ESP_LOGCONFIG("lvgl_camera_display", "  Mirror Y: %s", mirror_y_ ? "YES" : "NO");
  
#ifdef USE_PPA_ACCELERATION
  ESP_LOGCONFIG("lvgl_camera_display", "  PPA Support: COMPILED");
#else
  ESP_LOGCONFIG("lvgl_camera_display", "  PPA Support: NOT COMPILED");
#endif
}

}  // namespace lvgl_camera_display
}  // namespace esphome

















