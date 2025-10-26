/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 *  Video for Linux Two header file - ESP32-P4 version
 *  Version complète compatible ESPHome & esp-video
 */

#ifndef __LINUX_VIDEODEV2_H
#define __LINUX_VIDEODEV2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>
#include <stdint.h>
#include "../lvgl_camera_display/ioctl.h"

/* ========== DÉFINITIONS DE TYPES ========== */
#ifndef __u8
typedef uint8_t  __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int8_t   __s8;
typedef int16_t  __s16;
typedef int32_t  __s32;
typedef int64_t  __s64;
#endif

/* ========== MACROS IOCTL ========== */
#ifndef _IOC
#define _IOC(dir,type,nr,size) (((dir) << 30) | ((type) << 8) | (nr) | ((size) << 16))
#define _IO(type,nr)           _IOC(0,(type),(nr),0)
#define _IOR(type,nr,size)     _IOC(1,(type),(nr),sizeof(size))
#define _IOW(type,nr,size)     _IOC(2,(type),(nr),sizeof(size))
#define _IOWR(type,nr,size)    _IOC(3,(type),(nr),sizeof(size))
#endif

/* ========== CONSTANTES GÉNÉRALES ========== */
#define VIDEO_MAX_FRAME  32
#define VIDEO_MAX_PLANES 8

#define v4l2_fourcc(a,b,c,d) ((__u32)(a) | ((__u32)(b)<<8) | ((__u32)(c)<<16) | ((__u32)(d)<<24))
#define v4l2_fourcc_be(a,b,c,d) (v4l2_fourcc(a,b,c,d) | (1U<<31))

/* ========== BUFFER FLAGS ========== */
#define V4L2_BUF_FLAG_MAPPED         0x00000001
#define V4L2_BUF_FLAG_QUEUED         0x00000002
#define V4L2_BUF_FLAG_DONE           0x00000004
#define V4L2_BUF_FLAG_KEYFRAME       0x00000008
#define V4L2_BUF_FLAG_PFRAME         0x00000010
#define V4L2_BUF_FLAG_BFRAME         0x00000020
#define V4L2_BUF_FLAG_ERROR          0x00000040
#define V4L2_BUF_FLAG_TIMESTAMP_MASK 0x0000e000

/* ========== CAPABILITY FLAGS ========== */
#define V4L2_CAP_VIDEO_CAPTURE      0x00000001
#define V4L2_CAP_VIDEO_OUTPUT       0x00000002
#define V4L2_CAP_VIDEO_OVERLAY      0x00000004
#define V4L2_CAP_STREAMING          0x04000000
#define V4L2_CAP_READWRITE          0x01000000
#define V4L2_CAP_DEVICE_CAPS        0x80000000

/* ========== ENUMÉRATIONS ========== */
enum v4l2_field {
    V4L2_FIELD_ANY = 0,
    V4L2_FIELD_NONE = 1,
    V4L2_FIELD_TOP = 2,
    V4L2_FIELD_BOTTOM = 3,
    V4L2_FIELD_INTERLACED = 4
};

enum v4l2_buf_type {
    V4L2_BUF_TYPE_VIDEO_CAPTURE = 1,
    V4L2_BUF_TYPE_VIDEO_OUTPUT = 2,
    V4L2_BUF_TYPE_VIDEO_OVERLAY = 3,
    V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 9,
    V4L2_BUF_TYPE_META_CAPTURE = 13
};

enum v4l2_memory {
    V4L2_MEMORY_MMAP = 1,
    V4L2_MEMORY_USERPTR = 2,
    V4L2_MEMORY_OVERLAY = 3,
    V4L2_MEMORY_DMABUF = 4
};

/* ========== STRUCTURES PRINCIPALES ========== */
struct v4l2_capability {
    __u8 driver[16];
    __u8 card[32];
    __u8 bus_info[32];
    __u32 version;
    __u32 capabilities;
    __u32 device_caps;
    __u32 reserved[3];
};

struct v4l2_pix_format {
    __u32 width;
    __u32 height;
    __u32 pixelformat;
    __u32 field;
    __u32 bytesperline;
    __u32 sizeimage;
    __u32 colorspace;
    __u32 priv;
    __u32 flags;
    __u32 ycbcr_enc;
    __u32 quantization;
    __u32 xfer_func;
};

struct v4l2_format {
    __u32 type;
    union {
        struct v4l2_pix_format pix;
        __u8 raw_data[200];
    } fmt;
};

/* ========== BUFFER HANDLING ========== */
struct v4l2_requestbuffers {
    __u32 count;
    __u32 type;
    __u32 memory;
    __u32 reserved[2];
};

struct v4l2_timecode {
    __u32 type;
    __u32 flags;
    __u8 frames;
    __u8 seconds;
    __u8 minutes;
    __u8 hours;
    __u8 userbits[4];
};

struct v4l2_plane {
    __u32 bytesused;
    __u32 length;
    union {
        __u32 mem_offset;
        unsigned long userptr;
        __s32 fd;
    } m;
    __u32 data_offset;
    __u32 reserved[11];
};

struct v4l2_buffer {
    __u32 index;
    __u32 type;
    __u32 bytesused;
    __u32 flags;
    __u32 field;
    struct timeval timestamp;
    struct v4l2_timecode timecode;
    __u32 sequence;
    __u32 memory;
    union {
        __u32 offset;
        unsigned long userptr;
        struct v4l2_plane *planes;
        __s32 fd;
    } m;
    __u32 length;
    __u32 reserved2;
    __u32 reserved;
};

/* ========== PIXEL FORMATS ========== */
#define V4L2_PIX_FMT_RGB565 v4l2_fourcc('R','G','B','P')
#define V4L2_PIX_FMT_RGB24  v4l2_fourcc('R','G','B','3')
#define V4L2_PIX_FMT_YUV422P v4l2_fourcc('4','2','2','P')
#define V4L2_PIX_FMT_YUV420  v4l2_fourcc('Y','U','1','2')
#define V4L2_PIX_FMT_SBGGR8  v4l2_fourcc('B','A','8','1')
#define V4L2_PIX_FMT_SBGGR10 v4l2_fourcc('B','G','1','0')
#define V4L2_PIX_FMT_JPEG    v4l2_fourcc('J','P','E','G')
#define V4L2_PIX_FMT_H264    v4l2_fourcc('H','2','6','4')

/* ========== CONTROL TYPES ========== */
#define V4L2_CTRL_TYPE_INTEGER      1
#define V4L2_CTRL_TYPE_BOOLEAN      2
#define V4L2_CTRL_TYPE_MENU         3
#define V4L2_CTRL_TYPE_BUTTON       4
#define V4L2_CTRL_TYPE_INTEGER64    5
#define V4L2_CTRL_TYPE_CTRL_CLASS   6
#define V4L2_CTRL_TYPE_STRING       7
#define V4L2_CTRL_TYPE_BITMASK      8
#define V4L2_CTRL_TYPE_INTEGER_MENU 9

/* ========== CONTROL IDS ========== */
#define V4L2_CID_BASE               0x00980900
#define V4L2_CID_BRIGHTNESS         (V4L2_CID_BASE + 0)
#define V4L2_CID_CONTRAST           (V4L2_CID_BASE + 1)
#define V4L2_CID_SATURATION         (V4L2_CID_BASE + 2)
#define V4L2_CID_HUE                (V4L2_CID_BASE + 3)
#define V4L2_CID_EXPOSURE           (V4L2_CID_BASE + 17)
#define V4L2_CID_GAIN               (V4L2_CID_BASE + 19)
#define V4L2_CID_AUTO_EXPOSURE_BIAS (V4L2_CID_BASE + 26)
#define V4L2_CID_RED_BALANCE        (V4L2_CID_BASE + 14)
#define V4L2_CID_BLUE_BALANCE       (V4L2_CID_BASE + 15)

/* JPEG & H264 control IDs */
#define V4L2_CID_MPEG_BASE                     0x00990900
#define V4L2_CID_MPEG_VIDEO_BITRATE            (V4L2_CID_MPEG_BASE + 207)
#define V4L2_CID_MPEG_VIDEO_GOP_SIZE           (V4L2_CID_MPEG_BASE + 210)
#define V4L2_CID_JPEG_COMPRESSION_QUALITY      (V4L2_CID_MPEG_BASE + 500)

/* ========== EXT CONTROL STRUCTURES ========== */
struct v4l2_control {
    __u32 id;
    __s32 value;
};

struct v4l2_ext_control {
    __u32 id;
    __u32 size;
    __u32 reserved2[1];
    union {
        __s32 value;
        __s64 value64;
        char *string;
        __u8 *p_u8;
        __u16 *p_u16;
        __u32 *p_u32;
        void *ptr;
    };
} __attribute__ ((packed));

struct v4l2_ext_controls {
    __u32 ctrl_class;
    __u32 count;
    __u32 error_idx;
    __u32 reserved[2];
    struct v4l2_ext_control *controls;
};

struct v4l2_query_ext_ctrl {
    __u32 id;
    __u32 type;
    __u64 minimum;
    __u64 maximum;
    __u64 step;
    __u64 default_value;
    __u32 flags;
    __u32 elem_size;
    __u32 elems;
    __u32 nr_of_dims;
    __u32 dims[4];
    __u32 reserved[32];
    char name[32];
};

/* ========== COLORSPACES ========== */
#define V4L2_COLORSPACE_SRGB 8
#define V4L2_COLORSPACE_REC709 1
#define V4L2_COLORSPACE_JPEG 7
#define V4L2_COLORSPACE_RAW 10

/* ========== IOCTL COMMANDS ========== */
#define VIDIOC_QUERYCAP   _IOR('V',  0, struct v4l2_capability)
#define VIDIOC_G_FMT      _IOWR('V', 4, struct v4l2_format)
#define VIDIOC_S_FMT      _IOWR('V', 5, struct v4l2_format)
#define VIDIOC_REQBUFS    _IOWR('V', 8, struct v4l2_requestbuffers)
#define VIDIOC_QUERYBUF   _IOWR('V', 9, struct v4l2_buffer)
#define VIDIOC_G_CTRL     _IOWR('V',27, struct v4l2_control)
#define VIDIOC_S_CTRL     _IOWR('V',28, struct v4l2_control)
#define VIDIOC_QBUF       _IOWR('V',15, struct v4l2_buffer)
#define VIDIOC_DQBUF      _IOWR('V',17, struct v4l2_buffer)
#define VIDIOC_STREAMON   _IOW ('V',18, int)
#define VIDIOC_STREAMOFF  _IOW ('V',19, int)
#define VIDIOC_G_EXT_CTRLS _IOWR('V',71, struct v4l2_ext_controls)
#define VIDIOC_S_EXT_CTRLS _IOWR('V',72, struct v4l2_ext_controls)
#define VIDIOC_QUERY_EXT_CTRL _IOWR('V',103, struct v4l2_query_ext_ctrl)

#ifdef __cplusplus
}
#endif

#endif /* __LINUX_VIDEODEV2_H */


