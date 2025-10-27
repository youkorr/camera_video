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
  
  // ✅ NOUVEAU : Système de séquence
  uint32_t last_frame_sequence;  // Dernière séquence servie à l'application
  
  // Statistiques
  uint32_t frame_count;
  uint32_t drop_count;
  uint32_t total_dqbuf_calls;
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
};

/**
 * @brief Adaptateur V4L2 pour mipi_dsi_cam
 * 
 * Implémentation complète de l'interface V4L2 standard
 */
class MipiDsiCamV4L2Adapter {
 public:
  MipiDsiCamV4L2Adapter(MipiDsiCam *camera);
  ~MipiDsiCamV4L2Adapter();
  
  /**
   * @brief Initialise l'interface V4L2
   * @return ESP_OK si réussi
   */
  esp_err_t init();
  
  /**
   * @brief Libère les ressources V4L2
   * @return ESP_OK si réussi
   */
  esp_err_t deinit();
  
  /**
   * @brief Retourne le pointeur vers le device V4L2
   */
  void* get_video_device() { return &this->context_; }
  
  /**
   * @brief Vérifie si l'adaptateur est initialisé
   */
  bool is_initialized() const { return this->initialized_; }
  
  // ===== Callbacks V4L2 (publics pour esp_video_ops) =====
  
  static esp_err_t v4l2_init(void *video);
  static esp_err_t v4l2_deinit(void *video);
  static esp_err_t v4l2_start(void *video, uint32_t type);
  static esp_err_t v4l2_stop(void *video, uint32_t type);
  static esp_err_t v4l2_enum_format(void *video, uint32_t type, 
                                     uint32_t index, uint32_t *pixel_format);
  static esp_err_t v4l2_set_format(void *video, const void *format);
  static esp_err_t v4l2_get_format(void *video, void *format);
  static esp_err_t v4l2_reqbufs(void *video, void *reqbufs);
  static esp_err_t v4l2_querybuf(void *video, void *buffer);
  static esp_err_t v4l2_qbuf(void *video, void *buffer);
  static esp_err_t v4l2_dqbuf(void *video, void *buffer);
  static esp_err_t v4l2_querycap(void *video, void *cap);
  
 protected:
  MipiCameraV4L2Context context_;
  bool initialized_{false};
  
  // Table des opérations V4L2
  static const esp_video_ops s_video_ops;
};

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4


