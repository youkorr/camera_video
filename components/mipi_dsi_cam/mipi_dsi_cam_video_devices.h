/*
 * SPDX-FileCopyrightText: 2024
 * SPDX-License-Identifier: MIT
 *
 * Header pour l'initialisation et la création des devices esp-video
 * compatibles ESP32-P4 (JPEG + H.264)
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le sous-système esp-video.
 *
 * Doit être appelé avant toute création de device.
 */
esp_err_t mipi_dsi_cam_video_init(void);

/**
 * @brief Crée le périphérique JPEG (/dev/video10).
 *
 * @param user_ctx Contexte utilisateur optionnel (pointeur).
 * @return ESP_OK en cas de succès.
 */
esp_err_t mipi_dsi_cam_create_jpeg_device(void *user_ctx);

/**
 * @brief Crée le périphérique H.264 (/dev/video11).
 *
 * @param high_profile Active le profil H.264 High si true.
 * @return ESP_OK en cas de succès.
 */
esp_err_t mipi_dsi_cam_create_h264_device(bool high_profile);

#ifdef __cplusplus
}
#endif
