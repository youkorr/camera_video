/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"
//#include "esp_cam_sensor_types.h"
//#include "driver/jpeg_encode.h"
#include "esp_video_device.h"
//#include "hal/cam_ctlr_types.h"
#include "videodev2.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MIPI-CSI state
 */
typedef struct esp_video_csi_state {
    uint32_t lane_bitrate_mbps; /*!< MIPI-CSI data lane bitrate in Mbps */
    uint8_t lane_num;           /*!< MIPI-CSI data lane number */
    cam_ctlr_color_t in_color;  /*!< MIPI-CSI input(from camera sensor) data color format */
    cam_ctlr_color_t out_color; /*!< MIPI-CSI output(based on ISP output) data color format */
    uint8_t out_bpp;            /*!< MIPI-CSI output data color format bit per pixel */
    bool line_sync;             /*!< true: line has start and end packet; false. line has no start and end packet */
    bool bypass_isp; /*!< true: ISP directly output data from input port with processing. false: ISP output processed
                        data by pipeline  */
} esp_video_csi_state_t;

/**
 * @brief Create MIPI CSI video device
 *
 * @param cam_dev camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
esp_err_t esp_video_create_csi_video_device(esp_cam_sensor_device_t *cam_dev);
#endif

/**
 * @brief Create DVP video device
 *
 * @param cam_dev camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
esp_err_t esp_video_create_dvp_video_device(esp_cam_sensor_device_t *cam_dev);
#endif

/**
 * @brief Create H.264 video device
 *
 * @param hw_codec true: hardware H.264, false: software H.264(has not supported)
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#ifdef CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE
esp_err_t esp_video_create_h264_video_device(bool hw_codec);
#endif

/**
 * @brief Create JPEG video device
 *
 * @param enc_handle JPEG encoder driver handle,
 *      - NULL, JPEG video device will create JPEG encoder driver handle by itself
 *      - Not null, JPEG video device will use this handle instead of creating JPEG encoder driver handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#ifdef CONFIG_ESP_VIDEO_ENABLE_JPEG_VIDEO_DEVICE
esp_err_t esp_video_create_jpeg_video_device(jpeg_encoder_handle_t enc_handle);
#endif

#if CONFIG_ESP_VIDEO_ENABLE_ISP
/**
 * @brief Start ISP process based on MIPI-CSI state
 *
 * @param state MIPI-CSI state object
 * @param state MIPI-CSI V4L2 capture format
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_start_by_csi(const esp_video_csi_state_t *state, const struct v4l2_format *format);

/**
 * @brief Stop ISP process
 *
 * @param bypass true: bypass ISP and MIPI-CSI output sensor original data, false: ISP process image
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_stop(bool bypass);

/**
 * @brief Enumerate ISP supported output pixel format
 *
 * @param index        Enumerated number index
 * @param pixel_format Supported output pixel format
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_enum_format(uint32_t index, uint32_t *pixel_format);

/**
 * @brief Check if input format is valid
 *
 * @param format V4L2 format object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_check_format(const struct v4l2_format *format);

#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
/**
 * @brief Create ISP video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_isp_video_device(void);

#define VIDEO_PRIV_DATA(t, v) ((t)(v)->priv)

#define STREAM_FORMAT(s)   (&(s)->format)
#define STREAM_BUF_INFO(s) (&(s)->buf_info)

#define STREAM_BUFFER_SIZE(s) (STREAM_BUF_INFO(s)->size)

#define SET_BUF_INFO(bi, s, a, c) \
    {                             \
        (bi)->size       = (s);   \
        (bi)->align_size = (a);   \
        (bi)->caps       = (c);   \
    }

#define SET_FORMAT_WIDTH(_fmt, _width)    \
    {                                     \
        (_fmt)->fmt.pix.width = (_width); \
    }

#define SET_FORMAT_HEIGHT(_fmt, _height)    \
    {                                       \
        (_fmt)->fmt.pix.height = (_height); \
    }

#define SET_FORMAT_PIXEL_FORMAT(_fmt, _pixel_format)   \
    {                                                  \
        (_fmt)->fmt.pix.pixelformat = (_pixel_format); \
    }

#define GET_FORMAT_WIDTH(_fmt) ((_fmt)->fmt.pix.width)

#define GET_FORMAT_HEIGHT(_fmt) ((_fmt)->fmt.pix.height)

#define GET_FORMAT_PIXEL_FORMAT(_fmt) ((_fmt)->fmt.pix.pixelformat)

#define SET_STREAM_BUF_INFO(st, s, a, c) SET_BUF_INFO(STREAM_BUF_INFO(st), s, a, c)

#define SET_STREAM_FORMAT_WIDTH(st, w) SET_FORMAT_WIDTH(STREAM_FORMAT(st), w)

#define SET_STREAM_FORMAT_HEIGHT(st, h) SET_FORMAT_HEIGHT(STREAM_FORMAT(st), h)

#define SET_STREAM_FORMAT_PIXEL_FORMAT(st, f) SET_FORMAT_PIXEL_FORMAT(STREAM_FORMAT(st), f)

#define GET_STREAM_FORMAT_WIDTH(st) GET_FORMAT_WIDTH(STREAM_FORMAT(st))

#define GET_STREAM_FORMAT_HEIGHT(st) GET_FORMAT_HEIGHT(STREAM_FORMAT(st))

#define GET_STREAM_FORMAT_PIXEL_FORMAT(st) GET_FORMAT_PIXEL_FORMAT(STREAM_FORMAT(st))

/* video capture operations */

#define CAPTURE_VIDEO_STREAM(v)   ((v)->stream)
#define CAPTURE_VIDEO_BUF_SIZE(v) STREAM_BUFFER_SIZE(CAPTURE_VIDEO_STREAM(v))

#define CAPTURE_VIDEO_DONE_BUF(v, b, n) esp_video_done_buffer(v, V4L2_BUF_TYPE_VIDEO_CAPTURE, b, n)

#define CAPTURE_VIDEO_SET_FORMAT_WIDTH(v, w) SET_STREAM_FORMAT_WIDTH(CAPTURE_VIDEO_STREAM(v), w)

#define CAPTURE_VIDEO_SET_FORMAT_HEIGHT(v, h) SET_STREAM_FORMAT_HEIGHT(CAPTURE_VIDEO_STREAM(v), h)

#define CAPTURE_VIDEO_SET_FORMAT_PIXEL_FORMAT(v, f) SET_STREAM_FORMAT_PIXEL_FORMAT(CAPTURE_VIDEO_STREAM(v), f)

#define CAPTURE_VIDEO_GET_FORMAT_WIDTH(v) GET_STREAM_FORMAT_WIDTH(CAPTURE_VIDEO_STREAM(v))

#define CAPTURE_VIDEO_GET_FORMAT_HEIGHT(v) GET_STREAM_FORMAT_HEIGHT(CAPTURE_VIDEO_STREAM(v))

#define CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(v) GET_STREAM_FORMAT_PIXEL_FORMAT(CAPTURE_VIDEO_STREAM(v))

#define CAPTURE_VIDEO_SET_FORMAT(v, w, h, f)         \
    {                                                \
        CAPTURE_VIDEO_SET_FORMAT_WIDTH(v, w);        \
        CAPTURE_VIDEO_SET_FORMAT_HEIGHT(v, h);       \
        CAPTURE_VIDEO_SET_FORMAT_PIXEL_FORMAT(v, f); \
    }

#define CAPTURE_VIDEO_SET_BUF_INFO(v, s, a, c) SET_STREAM_BUF_INFO(CAPTURE_VIDEO_STREAM(v), s, a, c)

#define CAPTURE_VIDEO_GET_QUEUED_BUF(v) esp_video_get_queued_buffer(v, V4L2_BUF_TYPE_VIDEO_CAPTURE);

#define CAPTURE_VIDEO_QUEUE_ELEMENT(v, e) esp_video_queue_element(v, V4L2_BUF_TYPE_VIDEO_CAPTURE, e)

#define CAPTURE_VIDEO_GET_QUEUED_ELEMENT(v) esp_video_get_queued_element(v, V4L2_BUF_TYPE_VIDEO_CAPTURE)

/* video M2M operations */

#define M2M_VIDEO_CAPTURE_STREAM(v) (&(v)->stream[0])
#define M2M_VIDEO_OUTPUT_STREAM(v)  (&(v)->stream[1])

#define M2M_VIDEO_CAPTURE_BUF_SIZE(v) STREAM_BUFFER_SIZE(M2M_VIDEO_CAPTURE_STREAM(v))
#define M2M_VIDEO_OUTPUT_BUF_SIZE(v)  STREAM_BUFFER_SIZE(M2M_VIDEO_OUTPUT_STREAM(v))

#define M2M_VIDEO_DONE_CAPTURE_BUF(v, b, n) esp_video_done_buffer(v, V4L2_BUF_TYPE_VIDEO_CAPTURE, b, n)
#define M2M_VIDEO_DONE_OUTPUT_BUF(v, b, n)  esp_video_done_buffer(v, V4L2_BUF_TYPE_VIDEO_OUTPUT, b, n)

#define M2M_VIDEO_SET_CAPTURE_FORMAT_WIDTH(v, w) SET_STREAM_FORMAT_WIDTH(M2M_VIDEO_CAPTURE_STREAM(v), w)

#define M2M_VIDEO_SET_OUTPUT_FORMAT_WIDTH(v, w) SET_STREAM_FORMAT_WIDTH(M2M_VIDEO_OUTPUT_STREAM(v), w)

#define M2M_VIDEO_SET_CAPTURE_FORMAT_HEIGHT(v, h) SET_STREAM_FORMAT_HEIGHT(M2M_VIDEO_CAPTURE_STREAM(v), h)

#define M2M_VIDEO_SET_OUTPUT_FORMAT_HEIGHT(v, h) SET_STREAM_FORMAT_HEIGHT(M2M_VIDEO_OUTPUT_STREAM(v), h)

#define M2M_VIDEO_SET_CAPTURE_FORMAT_PIXEL_FORMAT(v, f) SET_STREAM_FORMAT_PIXEL_FORMAT(M2M_VIDEO_CAPTURE_STREAM(v), f)

#define M2M_VIDEO_SET_OUTPUT_FORMAT_PIXEL_FORMAT(v, f) SET_STREAM_FORMAT_PIXEL_FORMAT(M2M_VIDEO_OUTPUT_STREAM(v), f)

#define M2M_VIDEO_GET_CAPTURE_FORMAT_WIDTH(v) GET_STREAM_FORMAT_WIDTH(M2M_VIDEO_CAPTURE_STREAM(v))

#define M2M_VIDEO_GET_OUTPUT_FORMAT_WIDTH(v) GET_STREAM_FORMAT_WIDTH(M2M_VIDEO_OUTPUT_STREAM(v))

#define M2M_VIDEO_GET_CAPTURE_FORMAT_HEIGHT(v) GET_STREAM_FORMAT_HEIGHT(M2M_VIDEO_CAPTURE_STREAM(v))

#define M2M_VIDEO_GET_OUTPUT_FORMAT_HEIGHT(v) GET_STREAM_FORMAT_HEIGHT(M2M_VIDEO_OUTPUT_STREAM(v))

#define M2M_VIDEO_GET_CAPTURE_FORMAT_PIXEL_FORMAT(v) GET_STREAM_FORMAT_PIXEL_FORMAT(M2M_VIDEO_CAPTURE_STREAM(v))

#define M2M_VIDEO_GET_OUTPUT_FORMAT_PIXEL_FORMAT(v) GET_STREAM_FORMAT_PIXEL_FORMAT(M2M_VIDEO_OUTPUT_STREAM(v))

#define M2M_VIDEO_SET_CAPTURE_FORMAT(v, w, h, f)         \
    {                                                    \
        M2M_VIDEO_SET_CAPTURE_FORMAT_WIDTH(v, w);        \
        M2M_VIDEO_SET_CAPTURE_FORMAT_HEIGHT(v, h);       \
        M2M_VIDEO_SET_CAPTURE_FORMAT_PIXEL_FORMAT(v, f); \
    }

#define M2M_VIDEO_SET_OUTPUT_FORMAT(v, w, h, f)         \
    {                                                   \
        M2M_VIDEO_SET_OUTPUT_FORMAT_WIDTH(v, w);        \
        M2M_VIDEO_SET_OUTPUT_FORMAT_HEIGHT(v, h);       \
        M2M_VIDEO_SET_OUTPUT_FORMAT_PIXEL_FORMAT(v, f); \
    }

#define M2M_VIDEO_SET_CAPTURE_BUF_INFO(v, s, a, c) SET_STREAM_BUF_INFO(M2M_VIDEO_CAPTURE_STREAM(v), s, a, c)

#define M2M_VIDEO_SET_OUTPUT_BUF_INFO(v, s, a, c) SET_STREAM_BUF_INFO(M2M_VIDEO_OUTPUT_STREAM(v), s, a, c)

#define M2M_VIDEO_GET_CAPTURE_QUEUED_BUF(v) esp_video_get_queued_buffer(v, V4L2_BUF_TYPE_VIDEO_CAPTURE);

#define M2M_VIDEO_GET_OUTPUT_QUEUED_BUF(v) esp_video_get_queued_buffer(v, V4L2_BUF_TYPE_VIDEO_OUTPUT);

#define M2M_VIDEO_QUEUE_CAPTURE_ELEMENT(v, e) esp_video_queue_element(v, V4L2_BUF_TYPE_VIDEO_CAPTURE, e)

#define M2M_VIDEO_QUEUE_OUTPUT_ELEMENT(v, e) esp_video_queue_element(v, V4L2_BUF_TYPE_VIDEO_OUTPUT, e)

#define M2M_VIDEO_GET_CAPTURE_QUEUED_ELEMENT(v) esp_video_get_queued_element(v, V4L2_BUF_TYPE_VIDEO_CAPTURE)

#define M2M_VIDEO_GET_OUTPUT_QUEUED_ELEMENT(v) esp_video_get_queued_element(v, V4L2_BUF_TYPE_VIDEO_OUTPUT)

/* video meta operations */

#define META_VIDEO_STREAM(v)   ((v)->stream)
#define META_VIDEO_BUF_SIZE(v) STREAM_BUFFER_SIZE(CAPTURE_VIDEO_STREAM(v))

#define META_VIDEO_GET_FORMAT_WIDTH(v) GET_STREAM_FORMAT_WIDTH(META_VIDEO_STREAM(v))

#define META_VIDEO_GET_FORMAT_HEIGHT(v) GET_STREAM_FORMAT_HEIGHT(META_VIDEO_STREAM(v))

#define META_VIDEO_GET_FORMAT_PIXEL_FORMAT(v) GET_STREAM_FORMAT_PIXEL_FORMAT(META_VIDEO_STREAM(v))

#define META_VIDEO_SET_FORMAT_WIDTH(v, w) SET_STREAM_FORMAT_WIDTH(META_VIDEO_STREAM(v), w)

#define META_VIDEO_SET_FORMAT_HEIGHT(v, h) SET_STREAM_FORMAT_HEIGHT(META_VIDEO_STREAM(v), h)

#define META_VIDEO_SET_FORMAT_PIXEL_FORMAT(v, f) SET_STREAM_FORMAT_PIXEL_FORMAT(META_VIDEO_STREAM(v), f)

#define META_VIDEO_SET_FORMAT(v, w, h, f)         \
    {                                             \
        META_VIDEO_SET_FORMAT_WIDTH(v, w);        \
        META_VIDEO_SET_FORMAT_HEIGHT(v, h);       \
        META_VIDEO_SET_FORMAT_PIXEL_FORMAT(v, f); \
    }

#define META_VIDEO_SET_BUF_INFO(v, s, a, c) SET_STREAM_BUF_INFO(META_VIDEO_STREAM(v), s, a, c)

#define META_VIDEO_GET_QUEUED_ELEMENT(v) esp_video_get_queued_element(v, V4L2_BUF_TYPE_META_CAPTURE)

#define META_VIDEO_DONE_BUF(v, b, n) esp_video_done_buffer(v, V4L2_BUF_TYPE_META_CAPTURE, (uint8_t *)b, n)

/**
 * @brief Video event.
 */
enum esp_video_event {
    ESP_VIDEO_BUFFER_VALID = 0, /*!< Video buffer is freed and it can be allocated by video device */
    ESP_VIDEO_M2M_TRIGGER,      /*!< Trigger M2M video device transforming event */
};

struct esp_video;

/**
 * @brief M2M video device process function
 *
 * @param video         Video object
 * @param src           Source data buffer
 * @param src_size      Source data size in byte
 * @param dst           Destination buffer
 * @param dst_size      Destination buffer maximum size
 * @param dst_out_size  Actual destination data size
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
typedef esp_err_t (*esp_video_m2m_process_t)(struct esp_video *video, uint8_t *src, uint32_t src_size, uint8_t *dst,
                                             uint32_t dst_size, uint32_t *dst_out_size);

/**
 * @brief Video operations object.
 */


#endif
#endif

#ifdef __cplusplus
}
#endif
