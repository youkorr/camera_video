#pragma once

#ifdef MIPI_DSI_CAM_ENABLE_ISP_PIPELINE

#include "mipi_dsi_cam.h"

#ifdef USE_ESP32_VARIANT_ESP32P4

extern "C" {
  #include "driver/isp.h"
  #include "driver/isp_bf.h"
  #include "driver/isp_ccm.h"
  #include "driver/isp_sharpen.h"
  #include "driver/isp_gamma.h"
  #include "driver/isp_demosaic.h"
  #include "driver/isp_color.h"
}

namespace esphome {
namespace mipi_dsi_cam {

/**
 * @brief Pipeline ISP avancé pour mipi_dsi_cam
 * 
 * Cette classe gère un pipeline ISP complet avec :
 * - Bayer Filter (BF) pour réduction de bruit
 * - Color Correction Matrix (CCM) pour balance des blancs
 * - Demosaicing pour conversion Bayer -> RGB
 * - Sharpen pour amélioration de la netteté
 * - Gamma correction
 * - Color management (luminosité, contraste, saturation, teinte)
 * - Auto White Balance (AWB)
 * - Auto Exposure (AE)
 * - Histogramme
 */
class MipiDsiCamISPPipeline {
 public:
  MipiDsiCamISPPipeline(MipiDsiCam *camera);
  ~MipiDsiCamISPPipeline();
  
  /**
   * @brief Initialise le pipeline ISP complet
   * @return ESP_OK si réussi
   */
  esp_err_t init();
  
  /**
   * @brief Démarre le pipeline ISP
   * @return ESP_OK si réussi
   */
  esp_err_t start();
  
  /**
   * @brief Arrête le pipeline ISP
   * @return ESP_OK si réussi
   */
  esp_err_t stop();
  
  /**
   * @brief Libère les ressources ISP
   * @return ESP_OK si réussi
   */
  esp_err_t deinit();
  
  // ===== Configuration Bayer Filter =====
  
  /**
   * @brief Active/désactive le filtre Bayer (réduction de bruit)
   */
  esp_err_t enable_bayer_filter(bool enable);
  
  /**
   * @brief Configure le niveau de débruitage (0-20)
   */
  esp_err_t set_denoising_level(uint8_t level);
  
  // ===== Configuration CCM (Color Correction Matrix) =====
  
  /**
   * @brief Active/désactive la correction colorimétrique
   */
  esp_err_t enable_ccm(bool enable);
  
  /**
   * @brief Configure la matrice CCM 3x3
   * @param matrix Matrice 3x3 de correction des couleurs
   */
  esp_err_t set_ccm_matrix(float matrix[3][3]);
  
  /**
   * @brief Configure la balance des blancs manuelle
   * @param red_gain Gain rouge (0.5 - 4.0)
   * @param green_gain Gain vert (0.5 - 4.0)
   * @param blue_gain Gain bleu (0.5 - 4.0)
   */
  esp_err_t set_white_balance(float red_gain, float green_gain, float blue_gain);
  
  // ===== Configuration Sharpen =====
  
  /**
   * @brief Active/désactive l'amélioration de la netteté
   */
  esp_err_t enable_sharpen(bool enable);
  
  /**
   * @brief Configure les paramètres de netteté
   * @param h_thresh Seuil hautes fréquences (0-255)
   * @param l_thresh Seuil basses fréquences (0-255)
   * @param h_coeff Coefficient hautes fréquences (0.0-1.0)
   * @param m_coeff Coefficient moyennes fréquences (0.0-1.0)
   */
  esp_err_t set_sharpen_params(uint8_t h_thresh, uint8_t l_thresh, 
                                float h_coeff, float m_coeff);
  
  // ===== Configuration Gamma =====
  
  /**
   * @brief Active/désactive la correction gamma
   */
  esp_err_t enable_gamma(bool enable);
  
  /**
   * @brief Configure la courbe gamma
   * @param gamma Valeur gamma (0.1 - 5.0), 1.0
