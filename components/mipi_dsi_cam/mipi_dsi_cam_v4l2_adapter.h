#pragma once

#include "mipi_dsi_cam.h"
#include "esp_video_buffer.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

extern "C" {
  #include "videodev2.h"
}

// Déclaration de la fonction d'enregistrement du device
extern "C" esp_err_t esp_video_register_device(int device_id, void *video_device, 
                                                void *user_ctx, const void *ops);

namespace esphome {
namespace mipi_dsi_cam {

// Structure complète pour le contexte V4L2
struct MipiCameraV4L2Context {
  MipiDsiCam *camera;
  void *video_device;
  
  // Configuration du format
  uint32_t width;
  uint32_t height;
  uint32_t pixelformat;
  
  // État du streaming
  bool streaming;
  
  // Gestion des buffers
  struct esp_video_buffer *buffers;
  uint32_t buffer_count;
  uint32_t queued_count;
  uint32_t *queue_order;
  uint32_t queue_head;
  uint32_t queue_tail;
  
  // Statistiques
  uint32_t frame_count;
  uint32_t drop_count;
};

// Structure pour les opérations V4L2
struct esp_video_ops {
  esp_err_t (*init)(void *video);
  esp_err_t (*deinit)(void *video);
  esp_err_t (*start)(void *video, uint32_t type);
  esp_err_t (*stop)(void *video, uint32_t type);
  esp_err_t (*enum_format)(void *video, uint32_t type, uint32_t index, uint32_t *pixel_format);
  esp_err_t (*set_format)(void *video, const void *format);
  esp_err_t (*get_format)(void *video, void *format);
  esp_err_t (*reqbufs)(void *video, void *reqbufs);
  esp_err_t (*querybuf)(void *video, void *buffer);
  esp_err_t (*qbuf)(void *video, void *buffer);
  esp_err_t (*dqbuf)(void *video, void *buffer);
  esp_err_t (*querycap)(void *video, void *cap);
  
 protected:
  MipiCameraV4L2Context context_;
  bool initialized_{false};
  
  // Table des opérations V4L2
  static const esp_video_ops s_video_ops;
};

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4
