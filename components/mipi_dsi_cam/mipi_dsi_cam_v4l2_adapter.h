#pragma once

#include "mipi_dsi_cam.h"
#include "esp_video_buffer.h"
#include "esp_video_init.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

extern "C" {
  #include "videodev2.h"
  #include "v4l2-controls.h"
  #include "v4l2-common.h"
}

// Enregistrement du device V4L2 côté esp_video
extern "C" esp_err_t esp_video_register_device(int device_id,
                                               void *video_device,
                                               void *user_ctx,
                                               const void *ops);

namespace esphome {
namespace mipi_dsi_cam {

/**
 * Contexte V4L2 associé à la caméra MIPI.
 * On ne redéfinit PAS les structures des buffers : on pointe vers celles d'esp_video_buffer.h
 */
struct MipiCameraV4L2Context {
  // Pointeur vers la caméra (driver propriétaire)
  MipiDsiCam *camera{nullptr};

  // Pointeur opaque vers la structure "device" exposée à esp_video
  void *video_device{nullptr};

  // Format courant
  uint32_t width{0};
  uint32_t height{0};
  uint32_t pixelformat{0};

  // État streaming
  bool streaming{false};

  // Buffers vidéo gérés par la couche esp_video
  // esp_video_buffer est défini par esp_video_buffer.h et expose typiquement :
  //   element[i].buffer (void*)
  //   element[i].size   (size_t)  // le nom peut varier selon ta version, adapter dans mman.cpp si besoin
  esp_video_buffer *buffers{nullptr};
  uint32_t buffer_count{0};
  uint32_t queued_count{0};

  // Anti-duplicata / stats
  uint32_t last_frame_sequence{0};
  uint32_t frame_count{0};
  uint32_t drop_count{0};
  uint32_t total_dqbuf_calls{0};
};

/**
 * Table d’opérations attendue par esp_video (callbacks type V4L2)
 * Les signatures restent en void* pour compatibilité C.
 */
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
 * Adaptateur V4L2 complet pour MipiDsiCam
 * - N’enregistre pas de nouveaux types de buffers : s’appuie sur esp_video_buffer.h
 * - Compatible avec un mmap() qui traite buf.m.offset comme un index (démo M5Stack)
 */
class MipiDsiCamV4L2Adapter {
 public:
  explicit MipiDsiCamV4L2Adapter(MipiDsiCam *camera);
  ~MipiDsiCamV4L2Adapter();

  // Init/Deinit
  esp_err_t init();
  esp_err_t deinit();

  // Accès au device V4L2 exposé
  void* get_video_device() { return static_cast<void*>(&this->context_); }

  bool is_initialized() const { return this->initialized_; }

  // ==== Callbacks passés dans esp_video_ops (C-compatible) ====
  static esp_err_t v4l2_init(void *video);
  static esp_err_t v4l2_deinit(void *video);
  static esp_err_t v4l2_start(void *video, uint32_t type);
  static esp_err_t v4l2_stop(void *video, uint32_t type);
  static esp_err_t v4l2_enum_format(void *video, uint32_t type, uint32_t index, uint32_t *pixel_format);
  static esp_err_t v4l2_set_format(void *video, const void *format);
  static esp_err_t v4l2_get_format(void *video, void *format);
  static esp_err_t v4l2_reqbufs(void *video, void *reqbufs);
  static esp_err_t v4l2_querybuf(void *video, void *buffer);
  static esp_err_t v4l2_qbuf(void *video, void *buffer);
  static esp_err_t v4l2_dqbuf(void *video, void *buffer);
  static esp_err_t v4l2_querycap(void *video, void *cap);

 protected:
  MipiCameraV4L2Context context_{};
  bool initialized_{false};

  // Table d’opérations (une seule instance)
  static const esp_video_ops s_video_ops;
};

} // namespace mipi_dsi_cam
} // namespace esphome

#endif // USE_ESP32_VARIANT_ESP32P4



