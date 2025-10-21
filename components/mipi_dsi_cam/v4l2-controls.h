/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 *  Video for Linux Two controls header file - ESP-IDF compatible version
 *  Simplified for camera use on ESP32-P4
 */

#ifndef __LINUX_V4L2_CONTROLS_H
#define __LINUX_V4L2_CONTROLS_H

#include "types.h"

/* Control classes */
#define V4L2_CTRL_CLASS_USER            0x00980000 /* Old-style 'user' controls */
#define V4L2_CTRL_CLASS_CODEC           0x00990000 /* Stateful codec controls */
#define V4L2_CTRL_CLASS_CAMERA          0x009a0000 /* Camera class controls */
#define V4L2_CTRL_CLASS_FM_TX           0x009b0000 /* FM Modulator controls */
#define V4L2_CTRL_CLASS_FLASH           0x009c0000 /* Camera flash controls */
#define V4L2_CTRL_CLASS_JPEG            0x009d0000 /* JPEG-compression controls */
#define V4L2_CTRL_CLASS_IMAGE_SOURCE    0x009e0000 /* Image source controls */
#define V4L2_CTRL_CLASS_IMAGE_PROC      0x009f0000 /* Image processing controls */

/* User-class control IDs */
#define V4L2_CID_BASE               (V4L2_CTRL_CLASS_USER | 0x900)
#define V4L2_CID_USER_BASE          V4L2_CID_BASE
#define V4L2_CID_USER_CLASS         (V4L2_CTRL_CLASS_USER | 1)

/* Basic camera controls */
#define V4L2_CID_BRIGHTNESS         (V4L2_CID_BASE + 0)
#define V4L2_CID_CONTRAST           (V4L2_CID_BASE + 1)
#define V4L2_CID_SATURATION         (V4L2_CID_BASE + 2)
#define V4L2_CID_HUE                (V4L2_CID_BASE + 3)

/* White balance controls */
#define V4L2_CID_AUTO_WHITE_BALANCE (V4L2_CID_BASE + 12)
#define V4L2_CID_DO_WHITE_BALANCE   (V4L2_CID_BASE + 13)
#define V4L2_CID_RED_BALANCE        (V4L2_CID_BASE + 14)
#define V4L2_CID_BLUE_BALANCE       (V4L2_CID_BASE + 15)
#define V4L2_CID_GAMMA              (V4L2_CID_BASE + 16)

/* Exposure controls */
#define V4L2_CID_EXPOSURE           (V4L2_CID_BASE + 17)
#define V4L2_CID_AUTOGAIN           (V4L2_CID_BASE + 18)
#define V4L2_CID_GAIN               (V4L2_CID_BASE + 19)

/* Image orientation */
#define V4L2_CID_HFLIP              (V4L2_CID_BASE + 20)
#define V4L2_CID_VFLIP              (V4L2_CID_BASE + 21)

/* Power line frequency */
#define V4L2_CID_POWER_LINE_FREQUENCY (V4L2_CID_BASE + 24)
enum v4l2_power_line_frequency {
    V4L2_CID_POWER_LINE_FREQUENCY_DISABLED = 0,
    V4L2_CID_POWER_LINE_FREQUENCY_50HZ     = 1,
    V4L2_CID_POWER_LINE_FREQUENCY_60HZ     = 2,
    V4L2_CID_POWER_LINE_FREQUENCY_AUTO     = 3,
};

/* Additional controls */
#define V4L2_CID_HUE_AUTO                  (V4L2_CID_BASE + 25)
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE (V4L2_CID_BASE + 26)
#define V4L2_CID_SHARPNESS                 (V4L2_CID_BASE + 27)
#define V4L2_CID_BACKLIGHT_COMPENSATION    (V4L2_CID_BASE + 28)
#define V4L2_CID_CHROMA_AGC                (V4L2_CID_BASE + 29)

/* Color effects */
#define V4L2_CID_COLORFX                   (V4L2_CID_BASE + 31)
enum v4l2_colorfx {
    V4L2_COLORFX_NONE         = 0,
    V4L2_COLORFX_BW           = 1,
    V4L2_COLORFX_SEPIA        = 2,
    V4L2_COLORFX_NEGATIVE     = 3,
    V4L2_COLORFX_EMBOSS       = 4,
    V4L2_COLORFX_SKETCH       = 5,
    V4L2_COLORFX_SKY_BLUE     = 6,
    V4L2_COLORFX_GRASS_GREEN  = 7,
    V4L2_COLORFX_SKIN_WHITEN  = 8,
    V4L2_COLORFX_VIVID        = 9,
};

#define V4L2_CID_AUTOBRIGHTNESS   (V4L2_CID_BASE + 32)
#define V4L2_CID_ROTATE           (V4L2_CID_BASE + 34)
#define V4L2_CID_BG_COLOR         (V4L2_CID_BASE + 35)

/* Camera class control IDs */
#define V4L2_CID_CAMERA_CLASS_BASE  (V4L2_CTRL_CLASS_CAMERA | 0x900)
#define V4L2_CID_CAMERA_CLASS       (V4L2_CTRL_CLASS_CAMERA | 1)

/* Exposure controls (camera class) */
#define V4L2_CID_EXPOSURE_ABSOLUTE        (V4L2_CID_CAMERA_CLASS_BASE + 17)
#define V4L2_CID_EXPOSURE_AUTO            (V4L2_CID_CAMERA_CLASS_BASE + 1)
enum v4l2_exposure_auto_type {
    V4L2_EXPOSURE_AUTO = 0,
    V4L2_EXPOSURE_MANUAL = 1,
    V4L2_EXPOSURE_SHUTTER_PRIORITY = 2,
    V4L2_EXPOSURE_APERTURE_PRIORITY = 3,
};
#define V4L2_CID_EXPOSURE_AUTO_PRIORITY   (V4L2_CID_CAMERA_CLASS_BASE + 18)

/* Auto-focus controls */
#define V4L2_CID_AUTO_FOCUS_START         (V4L2_CID_CAMERA_CLASS_BASE + 28)
#define V4L2_CID_AUTO_FOCUS_STOP          (V4L2_CID_CAMERA_CLASS_BASE + 29)
#define V4L2_CID_AUTO_FOCUS_STATUS        (V4L2_CID_CAMERA_CLASS_BASE + 30)
#define V4L2_CID_AUTO_FOCUS_RANGE         (V4L2_CID_CAMERA_CLASS_BASE + 31)

/* White balance preset */
#define V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE (V4L2_CID_CAMERA_CLASS_BASE + 32)
enum v4l2_auto_n_preset_white_balance {
    V4L2_WHITE_BALANCE_MANUAL       = 0,
    V4L2_WHITE_BALANCE_AUTO         = 1,
    V4L2_WHITE_BALANCE_INCANDESCENT = 2,
    V4L2_WHITE_BALANCE_FLUORESCENT  = 3,
    V4L2_WHITE_BALANCE_FLUORESCENT_H = 4,
    V4L2_WHITE_BALANCE_HORIZON      = 5,
    V4L2_WHITE_BALANCE_DAYLIGHT     = 6,
    V4L2_WHITE_BALANCE_FLASH        = 7,
    V4L2_WHITE_BALANCE_CLOUDY       = 8,
    V4L2_WHITE_BALANCE_SHADE        = 9,
};

/* ISO sensitivity */
#define V4L2_CID_ISO_SENSITIVITY          (V4L2_CID_CAMERA_CLASS_BASE + 33)
#define V4L2_CID_ISO_SENSITIVITY_AUTO     (V4L2_CID_CAMERA_CLASS_BASE + 34)
enum v4l2_iso_sensitivity_auto_type {
    V4L2_ISO_SENSITIVITY_MANUAL = 0,
    V4L2_ISO_SENSITIVITY_AUTO   = 1,
};

/* Scene mode */
#define V4L2_CID_SCENE_MODE               (V4L2_CID_CAMERA_CLASS_BASE + 35)
enum v4l2_scene_mode {
    V4L2_SCENE_MODE_NONE              = 0,
    V4L2_SCENE_MODE_BACKLIGHT         = 1,
    V4L2_SCENE_MODE_BEACH_SNOW        = 2,
    V4L2_SCENE_MODE_CANDLE_LIGHT      = 3,
    V4L2_SCENE_MODE_DAWN_DUSK         = 4,
    V4L2_SCENE_MODE_FALL_COLORS       = 5,
    V4L2_SCENE_MODE_FIREWORKS         = 6,
    V4L2_SCENE_MODE_LANDSCAPE         = 7,
    V4L2_SCENE_MODE_NIGHT             = 8,
    V4L2_SCENE_MODE_PARTY_INDOOR      = 9,
    V4L2_SCENE_MODE_PORTRAIT          = 10,
    V4L2_SCENE_MODE_SPORTS            = 11,
    V4L2_SCENE_MODE_SUNSET            = 12,
    V4L2_SCENE_MODE_TEXT              = 13,
};

/* Test pattern */
#define V4L2_CID_TEST_PATTERN             (V4L2_CID_CAMERA_CLASS_BASE + 41)
enum v4l2_test_pattern {
    V4L2_TEST_PATTERN_DISABLED        = 0,
    V4L2_TEST_PATTERN_COLOR_BARS      = 1,
    V4L2_TEST_PATTERN_SOLID_COLOR     = 2,
};

/* Image processing controls */
#define V4L2_CID_IMAGE_PROC_CLASS_BASE    (V4L2_CTRL_CLASS_IMAGE_PROC | 0x900)
#define V4L2_CID_IMAGE_PROC_CLASS         (V4L2_CTRL_CLASS_IMAGE_PROC | 1)

/* Link frequency (for camera sensors) */
#define V4L2_CID_LINK_FREQ                (V4L2_CID_IMAGE_PROC_CLASS_BASE + 1)
#define V4L2_CID_PIXEL_RATE               (V4L2_CID_IMAGE_PROC_CLASS_BASE + 2)

/* Digital gain */
#define V4L2_CID_DIGITAL_GAIN             (V4L2_CID_IMAGE_PROC_CLASS_BASE + 5)

/* Analogue gain */
#define V4L2_CID_ANALOGUE_GAIN            (V4L2_CID_IMAGE_PROC_CLASS_BASE + 7)

/* Flash controls */
#define V4L2_CID_FLASH_CLASS_BASE         (V4L2_CTRL_CLASS_FLASH | 0x900)
#define V4L2_CID_FLASH_CLASS              (V4L2_CTRL_CLASS_FLASH | 1)

#define V4L2_CID_FLASH_LED_MODE           (V4L2_CID_FLASH_CLASS_BASE + 1)
enum v4l2_flash_led_mode {
    V4L2_FLASH_LED_MODE_NONE    = 0,
    V4L2_FLASH_LED_MODE_FLASH   = 1,
    V4L2_FLASH_LED_MODE_TORCH   = 2,
};

/* Auto exposure target/bias */
#define V4L2_CID_AUTO_EXPOSURE_BIAS       (V4L2_CID_CAMERA_CLASS_BASE + 42)

#endif /* __LINUX_V4L2_CONTROLS_H */
