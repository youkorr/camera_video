/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __has_include
  #if __has_include("driver/i2c_master.h")
    #include "driver/i2c_master.h"
  #else
    typedef void *i2c_master_bus_handle_t;
  #endif

  #if __has_include("esp_cam_ctlr_dvp.h")
    #include "esp_cam_ctlr_dvp.h"
  #else
    typedef struct {
        int pclk_io;
        int vsync_io;
        int href_io;
        int data_io[8];
        int xclk_io;
    } esp_cam_ctlr_dvp_pin_config_t;
  #endif

  #if __has_include("driver/jpeg_encode.h")
    #include "driver/jpeg_encode.h"
  #else
    typedef void *jpeg_encoder_handle_t;
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** SCCB initialization configuration **/
typedef struct esp_video_init_sccb_config {
    bool init_sccb;
    union {
        struct {
            uint8_t port;
            uint8_t scl_pin;
            uint8_t sda_pin;
        } i2c_config;
        i2c_master_bus_handle_t i2c_handle;
    };
    uint32_t freq;
} esp_video_init_sccb_config_t;

/** MIPI CSI configuration **/
typedef struct esp_video_init_csi_config {
    esp_video_init_sccb_config_t sccb_config;
    int8_t reset_pin;
    int8_t pwdn_pin;
} esp_video_init_csi_config_t;

/** DVP configuration **/
typedef struct esp_video_init_dvp_config {
    esp_video_init_sccb_config_t sccb_config;
    int8_t reset_pin;
    int8_t pwdn_pin;
    esp_cam_ctlr_dvp_pin_config_t dvp_pin;
    uint32_t xclk_freq;
} esp_video_init_dvp_config_t;

/** JPEG configuration **/
typedef struct esp_video_init_jpeg_config {
    jpeg_encoder_handle_t enc_handle;
} esp_video_init_jpeg_config_t;

/** ISP configuration **/
typedef struct esp_video_init_isp_config {
    int ipa_nums;
    const char **ipa_names;
} esp_video_init_isp_config_t;

/** Global initialization configuration **/
typedef struct esp_video_init_config {
    const esp_video_init_csi_config_t *csi;
    const esp_video_init_dvp_config_t *dvp;
    const esp_video_init_jpeg_config_t *jpeg;
    const esp_video_init_isp_config_t *isp;
} esp_video_init_config_t;

/** ✅ AJOUT : Structure pour les opérations V4L2 **/
// struct esp_video_ops {
  // esp_err_t (*init)(void *video);
  // esp_err_t (*deinit)(void *video);
  // esp_err_t (*start)(void *video, uint32_t type);
  // esp_err_t (*stop)(void *video, uint32_t type);
  // esp_err_t (*enum_format)(void *video, uint32_t type, uint32_t index, uint32_t *pixel_format);
  // esp_err_t (*set_format)(void *video, const void *format);
  // esp_err_t (*get_format)(void *video, void *format);
  // esp_err_t (*reqbufs)(void *video, void *reqbufs);
  // esp_err_t (*querybuf)(void *video, void *buffer);
  // esp_err_t (*qbuf)(void *video, void *buffer);
  // esp_err_t (*dqbuf)(void *video, void *buffer);
  // esp_err_t (*querycap)(void *video, void *cap);
};

/**
 * @brief Initialize video hardware and software (I2C, MIPI CSI, etc.)
 */
esp_err_t esp_video_init(const esp_video_init_config_t *config);

/**
 * @brief Deinitialize and free video hardware resources.
 */
esp_err_t esp_video_deinit(void);

/**
 * @brief Register a video device with the VFS system
 * 
 * @param device_id Device number (0-3)
 * @param video_device Pointer to device context
 * @param user_ctx User context pointer
 * @param ops Pointer to operations table
 * @return ESP_OK on success
 */
esp_err_t esp_video_register_device(int device_id, void *video_device, 
                                    void *user_ctx, const void *ops);

#ifdef __cplusplus
}
#endif

