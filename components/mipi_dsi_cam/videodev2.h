/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 *  Video for Linux Two header file - Portable (ESP32/embedded) full user API
 *  - Autonome, sans include linux/*
 *  - Structures/fonctions publiques V4L2 usuelles (capture, overlay, events,
 *    crop/selection, framebuffer, metadata, JPEG/H.264 params, multiplanar, etc.)
 */

#ifndef __LINUX_VIDEODEV2_H
#define __LINUX_VIDEODEV2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/time.h>

/* ===================== Types de base ===================== */
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

/* ===================== IOCTL légers (compat) ===================== */
#ifndef _IOC
#define _IOC_NRBITS     8
#define _IOC_TYPEBITS   8
#define _IOC_SIZEBITS   14
#define _IOC_DIRBITS    2

#define _IOC_NRMASK     ((1 << _IOC_NRBITS)-1)
#define _IOC_TYPEMASK   ((1 << _IOC_TYPEBITS)-1)
#define _IOC_SIZEMASK   ((1 << _IOC_SIZEBITS)-1)
#define _IOC_DIRMASK    ((1 << _IOC_DIRBITS)-1)

#define _IOC_DIRSHIFT   (30)
#define _IOC_TYPESHIFT  (8)
#define _IOC_NRSHIFT    (0)
#define _IOC_SIZESHIFT  (16)

#define _IOC_NONE       0U
#define _IOC_WRITE      1U
#define _IOC_READ       2U

#define _IOC(dir,type,nr,size) \
    (((dir)  << _IOC_DIRSHIFT)  | \
     ((type) << _IOC_TYPESHIFT) | \
     ((nr)   << _IOC_NRSHIFT)   | \
     ((size) << _IOC_SIZESHIFT))

#define _IO(type,nr)           _IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,size)     _IOC(_IOC_READ,(type),(nr),sizeof(size))
#define _IOW(type,nr,size)     _IOC(_IOC_WRITE,(type),(nr),sizeof(size))
#define _IOWR(type,nr,size)    _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))
#endif

/* ===================== Constantes générales ===================== */
#define VIDEO_MAX_FRAME   32
#define VIDEO_MAX_PLANES   8

/* FOURCC */
#define v4l2_fourcc(a,b,c,d)    ((__u32)(a) | ((__u32)(b)<<8) | ((__u32)(c)<<16) | ((__u32)(d)<<24))
#define v4l2_fourcc_be(a,b,c,d) (v4l2_fourcc(a,b,c,d) | (1U<<31))

/* ===================== Capabilities ===================== */
#define V4L2_CAP_VIDEO_CAPTURE        0x00000001
#define V4L2_CAP_VIDEO_OUTPUT         0x00000002
#define V4L2_CAP_VIDEO_OVERLAY        0x00000004
#define V4L2_CAP_VBI_CAPTURE          0x00000010
#define V4L2_CAP_VBI_OUTPUT           0x00000020
#define V4L2_CAP_RDS_CAPTURE          0x00000100
#define V4L2_CAP_TUNER                0x00010000
#define V4L2_CAP_AUDIO                0x00020000
#define V4L2_CAP_RADIO                0x00040000
#define V4L2_CAP_READWRITE            0x01000000
#define V4L2_CAP_ASYNCIO              0x02000000
#define V4L2_CAP_STREAMING            0x04000000
#define V4L2_CAP_META_CAPTURE         0x00800000
#define V4L2_CAP_VIDEO_CAPTURE_MPLANE 0x00001000
#define V4L2_CAP_VIDEO_OUTPUT_MPLANE  0x00002000
#define V4L2_CAP_VIDEO_M2M            0x00008000
#define V4L2_CAP_VIDEO_M2M_MPLANE     0x00004000
#define V4L2_CAP_DEVICE_CAPS          0x80000000
#define V4L2_MODE_HIGHQUALITY 0x0001

/* ===================== Fields / types / mémoire ===================== */
enum v4l2_field {
    V4L2_FIELD_ANY = 0,
    V4L2_FIELD_NONE = 1,
    V4L2_FIELD_TOP = 2,
    V4L2_FIELD_BOTTOM = 3,
    V4L2_FIELD_INTERLACED = 4,
    V4L2_FIELD_SEQ_TB = 5,
    V4L2_FIELD_SEQ_BT = 6,
    V4L2_FIELD_ALTERNATE = 7,
    V4L2_FIELD_INTERLACED_TB = 8,
    V4L2_FIELD_INTERLACED_BT = 9
};

enum v4l2_buf_type {
    V4L2_BUF_TYPE_VIDEO_CAPTURE        = 1,
    V4L2_BUF_TYPE_VIDEO_OUTPUT         = 2,
    V4L2_BUF_TYPE_VIDEO_OVERLAY        = 3,
    V4L2_BUF_TYPE_VBI_CAPTURE          = 4,
    V4L2_BUF_TYPE_VBI_OUTPUT           = 5,
    V4L2_BUF_TYPE_SLICED_VBI_CAPTURE   = 6,
    V4L2_BUF_TYPE_SLICED_VBI_OUTPUT    = 7,
    V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 9,
    V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE  = 10,
    V4L2_BUF_TYPE_SDR_CAPTURE          = 11,
    V4L2_BUF_TYPE_SDR_OUTPUT           = 12,
    V4L2_BUF_TYPE_META_CAPTURE         = 13,
    V4L2_BUF_TYPE_META_OUTPUT          = 14
};

#define V4L2_TYPE_IS_MULTIPLANAR(t) \
   ((t)==V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || (t)==V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
#define V4L2_TYPE_IS_OUTPUT(t) \
   ((t)==V4L2_BUF_TYPE_VIDEO_OUTPUT || (t)==V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE || \
    (t)==V4L2_BUF_TYPE_VIDEO_OVERLAY || (t)==V4L2_BUF_TYPE_SDR_OUTPUT || \
    (t)==V4L2_BUF_TYPE_META_OUTPUT)

enum v4l2_memory {
    V4L2_MEMORY_MMAP    = 1,
    V4L2_MEMORY_USERPTR = 2,
    V4L2_MEMORY_OVERLAY = 3,
    V4L2_MEMORY_DMABUF  = 4
};

/* ===================== Colorimétrie ===================== */
enum v4l2_colorspace {
    V4L2_COLORSPACE_DEFAULT = 0,
    V4L2_COLORSPACE_SMPTE170M = 1,
    V4L2_COLORSPACE_SMPTE240M = 2,
    V4L2_COLORSPACE_REC709 = 3,
    V4L2_COLORSPACE_BT878 = 4,
    V4L2_COLORSPACE_470_SYSTEM_M = 5,
    V4L2_COLORSPACE_470_SYSTEM_BG = 6,
    V4L2_COLORSPACE_JPEG = 7,
    V4L2_COLORSPACE_SRGB = 8,
    V4L2_COLORSPACE_OPRGB = 9,
    V4L2_COLORSPACE_BT2020 = 10,
    V4L2_COLORSPACE_RAW = 11,
    V4L2_COLORSPACE_DCI_P3 = 12
};
enum v4l2_xfer_func {
    V4L2_XFER_FUNC_DEFAULT   = 0,
    V4L2_XFER_FUNC_709       = 1,
    V4L2_XFER_FUNC_SRGB      = 2,
    V4L2_XFER_FUNC_OPRGB     = 3,
    V4L2_XFER_FUNC_SMPTE240M = 4,
    V4L2_XFER_FUNC_NONE      = 5,
    V4L2_XFER_FUNC_DCI_P3    = 6,
    V4L2_XFER_FUNC_SMPTE2084 = 7
};
enum v4l2_ycbcr_encoding {
    V4L2_YCBCR_ENC_DEFAULT = 0,
    V4L2_YCBCR_ENC_601 = 1,
    V4L2_YCBCR_ENC_709 = 2,
    V4L2_YCBCR_ENC_XV601 = 3,
    V4L2_YCBCR_ENC_XV709 = 4,
    V4L2_YCBCR_ENC_SYCC  = 5,
    V4L2_YCBCR_ENC_BT2020 = 6,
    V4L2_YCBCR_ENC_BT2020_CONST_LUM = 7,
    V4L2_YCBCR_ENC_SMPTE240M = 8
};
enum v4l2_hsv_encoding {
    V4L2_HSV_ENC_180 = 128,
    V4L2_HSV_ENC_256 = 129
};
enum v4l2_quantization {
    V4L2_QUANTIZATION_DEFAULT    = 0,
    V4L2_QUANTIZATION_FULL_RANGE = 1,
    V4L2_QUANTIZATION_LIM_RANGE  = 2
};

struct v4l2_rect  { __s32 left; __s32 top; __u32 width; __u32 height; };
struct v4l2_fract { __u32 numerator; __u32 denominator; };
struct v4l2_area  { __u32 width; __u32 height; };

/* ===================== Caps / formats ===================== */
struct v4l2_capability {
    __u8  driver[16];
    __u8  card[32];
    __u8  bus_info[32];
    __u32 version;
    __u32 capabilities;
    __u32 device_caps;
    __u32 reserved[3];
};
/* Values for 'capabilities' field */
#define V4L2_CAP_VIDEO_CAPTURE        0x00000001 /* Is a video capture device */
#define V4L2_CAP_VIDEO_OUTPUT         0x00000002 /* Is a video output device */
#define V4L2_CAP_VIDEO_OVERLAY        0x00000004 /* Can do video overlay */
#define V4L2_CAP_VBI_CAPTURE          0x00000010 /* Is a raw VBI capture device */
#define V4L2_CAP_VBI_OUTPUT           0x00000020 /* Is a raw VBI output device */
#define V4L2_CAP_SLICED_VBI_CAPTURE   0x00000040 /* Is a sliced VBI capture device */
#define V4L2_CAP_SLICED_VBI_OUTPUT    0x00000080 /* Is a sliced VBI output device */
#define V4L2_CAP_RDS_CAPTURE          0x00000100 /* RDS data capture */
#define V4L2_CAP_VIDEO_OUTPUT_OVERLAY 0x00000200 /* Can do video output overlay */
#define V4L2_CAP_HW_FREQ_SEEK         0x00000400 /* Can do hardware frequency seek  */
#define V4L2_CAP_RDS_OUTPUT           0x00000800 /* Is an RDS encoder */

/* Is a video capture device that supports multiplanar formats */
#define V4L2_CAP_VIDEO_CAPTURE_MPLANE 0x00001000
/* Is a video output device that supports multiplanar formats */
#define V4L2_CAP_VIDEO_OUTPUT_MPLANE 0x00002000
/* Is a video mem-to-mem device that supports multiplanar formats */
#define V4L2_CAP_VIDEO_M2M_MPLANE 0x00004000
/* Is a video mem-to-mem device */
#define V4L2_CAP_VIDEO_M2M 0x00008000

#define V4L2_CAP_TUNER     0x00010000 /* has a tuner */
#define V4L2_CAP_AUDIO     0x00020000 /* has audio support */
#define V4L2_CAP_RADIO     0x00040000 /* is a radio device */
#define V4L2_CAP_MODULATOR 0x00080000 /* has a modulator */

#define V4L2_CAP_SDR_CAPTURE    0x00100000 /* Is a SDR capture device */
#define V4L2_CAP_EXT_PIX_FORMAT 0x00200000 /* Supports the extended pixel format */
#define V4L2_CAP_SDR_OUTPUT     0x00400000 /* Is a SDR output device */
#define V4L2_CAP_META_CAPTURE   0x00800000 /* Is a metadata capture device */

#define V4L2_CAP_READWRITE   0x01000000 /* read/write systemcalls */
#define V4L2_CAP_ASYNCIO     0x02000000 /* async I/O */
#define V4L2_CAP_STREAMING   0x04000000 /* streaming I/O ioctls */
#define V4L2_CAP_META_OUTPUT 0x08000000 /* Is a metadata output device */

#define V4L2_CAP_TOUCH 0x10000000 /* Is a touch device */

#define V4L2_CAP_IO_MC 0x20000000 /* Is input/output controlled by the media controller */

#define V4L2_CAP_DEVICE_CAPS 0x80000000 /* sets device capabilities field */
/* --- single-plane pix --- */
struct v4l2_pix_format {
    __u32 width;
    __u32 height;
    __u32 pixelformat;  /* FOURCC */
    __u32 field;        /* enum v4l2_field */
    __u32 bytesperline; /* 0 si non utilisé */
    __u32 sizeimage;
    __u32 colorspace;   /* enum v4l2_colorspace */
    __u32 priv;
    __u32 flags;
    union {
        __u32 ycbcr_enc; /* enum v4l2_ycbcr_encoding */
        __u32 hsv_enc;   /* enum v4l2_hsv_encoding */
    };
    __u32 quantization; /* enum v4l2_quantization */
    __u32 xfer_func;    /* enum v4l2_xfer_func */
};
/*      Pixel format         FOURCC                          depth  Description  */

/* RGB formats (1 or 2 bytes per pixel) */
#define V4L2_PIX_FMT_RGB332   v4l2_fourcc('R', 'G', 'B', '1')    /*  8  RGB-3-3-2     */
#define V4L2_PIX_FMT_RGB444   v4l2_fourcc('R', '4', '4', '4')    /* 16  xxxxrrrr ggggbbbb */
#define V4L2_PIX_FMT_ARGB444  v4l2_fourcc('A', 'R', '1', '2')    /* 16  aaaarrrr ggggbbbb */
#define V4L2_PIX_FMT_XRGB444  v4l2_fourcc('X', 'R', '1', '2')    /* 16  xxxxrrrr ggggbbbb */
#define V4L2_PIX_FMT_RGBA444  v4l2_fourcc('R', 'A', '1', '2')    /* 16  rrrrgggg bbbbaaaa */
#define V4L2_PIX_FMT_RGBX444  v4l2_fourcc('R', 'X', '1', '2')    /* 16  rrrrgggg bbbbxxxx */
#define V4L2_PIX_FMT_ABGR444  v4l2_fourcc('A', 'B', '1', '2')    /* 16  aaaabbbb ggggrrrr */
#define V4L2_PIX_FMT_XBGR444  v4l2_fourcc('X', 'B', '1', '2')    /* 16  xxxxbbbb ggggrrrr */
#define V4L2_PIX_FMT_BGRA444  v4l2_fourcc('G', 'A', '1', '2')    /* 16  bbbbgggg rrrraaaa */
#define V4L2_PIX_FMT_BGRX444  v4l2_fourcc('B', 'X', '1', '2')    /* 16  bbbbgggg rrrrxxxx */
#define V4L2_PIX_FMT_RGB555   v4l2_fourcc('R', 'G', 'B', 'O')    /* 16  RGB-5-5-5     */
#define V4L2_PIX_FMT_ARGB555  v4l2_fourcc('A', 'R', '1', '5')    /* 16  ARGB-1-5-5-5  */
#define V4L2_PIX_FMT_XRGB555  v4l2_fourcc('X', 'R', '1', '5')    /* 16  XRGB-1-5-5-5  */
#define V4L2_PIX_FMT_RGBA555  v4l2_fourcc('R', 'A', '1', '5')    /* 16  RGBA-5-5-5-1  */
#define V4L2_PIX_FMT_RGBX555  v4l2_fourcc('R', 'X', '1', '5')    /* 16  RGBX-5-5-5-1  */
#define V4L2_PIX_FMT_ABGR555  v4l2_fourcc('A', 'B', '1', '5')    /* 16  ABGR-1-5-5-5  */
#define V4L2_PIX_FMT_XBGR555  v4l2_fourcc('X', 'B', '1', '5')    /* 16  XBGR-1-5-5-5  */
#define V4L2_PIX_FMT_BGRA555  v4l2_fourcc('B', 'A', '1', '5')    /* 16  BGRA-5-5-5-1  */
#define V4L2_PIX_FMT_BGRX555  v4l2_fourcc('B', 'X', '1', '5')    /* 16  BGRX-5-5-5-1  */
#define V4L2_PIX_FMT_RGB565   v4l2_fourcc('R', 'G', 'B', 'P')    /* 16  RGB-5-6-5     */
#define V4L2_PIX_FMT_RGB555X  v4l2_fourcc('R', 'G', 'B', 'Q')    /* 16  RGB-5-5-5 BE  */
#define V4L2_PIX_FMT_ARGB555X v4l2_fourcc_be('A', 'R', '1', '5') /* 16  ARGB-5-5-5 BE */
#define V4L2_PIX_FMT_XRGB555X v4l2_fourcc_be('X', 'R', '1', '5') /* 16  XRGB-5-5-5 BE */
#define V4L2_PIX_FMT_RGB565X  v4l2_fourcc('R', 'G', 'B', 'R')    /* 16  RGB-5-6-5 BE  */

/* RGB formats (3 or 4 bytes per pixel) */
#define V4L2_PIX_FMT_BGR666 v4l2_fourcc('B', 'G', 'R', 'H') /* 18  BGR-6-6-6	  */
#define V4L2_PIX_FMT_BGR24  v4l2_fourcc('B', 'G', 'R', '3') /* 24  BGR-8-8-8     */
#define V4L2_PIX_FMT_RGB24  v4l2_fourcc('R', 'G', 'B', '3') /* 24  RGB-8-8-8     */
#define V4L2_PIX_FMT_BGR32  v4l2_fourcc('B', 'G', 'R', '4') /* 32  BGR-8-8-8-8   */
#define V4L2_PIX_FMT_ABGR32 v4l2_fourcc('A', 'R', '2', '4') /* 32  BGRA-8-8-8-8  */
#define V4L2_PIX_FMT_XBGR32 v4l2_fourcc('X', 'R', '2', '4') /* 32  BGRX-8-8-8-8  */
#define V4L2_PIX_FMT_BGRA32 v4l2_fourcc('R', 'A', '2', '4') /* 32  ABGR-8-8-8-8  */
#define V4L2_PIX_FMT_BGRX32 v4l2_fourcc('R', 'X', '2', '4') /* 32  XBGR-8-8-8-8  */
#define V4L2_PIX_FMT_RGB32  v4l2_fourcc('R', 'G', 'B', '4') /* 32  RGB-8-8-8-8   */
#define V4L2_PIX_FMT_RGBA32 v4l2_fourcc('A', 'B', '2', '4') /* 32  RGBA-8-8-8-8  */
#define V4L2_PIX_FMT_RGBX32 v4l2_fourcc('X', 'B', '2', '4') /* 32  RGBX-8-8-8-8  */
#define V4L2_PIX_FMT_ARGB32 v4l2_fourcc('B', 'A', '2', '4') /* 32  ARGB-8-8-8-8  */
#define V4L2_PIX_FMT_XRGB32 v4l2_fourcc('B', 'X', '2', '4') /* 32  XRGB-8-8-8-8  */

/* Grey formats */
#define V4L2_PIX_FMT_GREY   v4l2_fourcc('G', 'R', 'E', 'Y')    /*  8  Greyscale     */
#define V4L2_PIX_FMT_Y4     v4l2_fourcc('Y', '0', '4', ' ')    /*  4  Greyscale     */
#define V4L2_PIX_FMT_Y6     v4l2_fourcc('Y', '0', '6', ' ')    /*  6  Greyscale     */
#define V4L2_PIX_FMT_Y10    v4l2_fourcc('Y', '1', '0', ' ')    /* 10  Greyscale     */
#define V4L2_PIX_FMT_Y12    v4l2_fourcc('Y', '1', '2', ' ')    /* 12  Greyscale     */
#define V4L2_PIX_FMT_Y14    v4l2_fourcc('Y', '1', '4', ' ')    /* 14  Greyscale     */
#define V4L2_PIX_FMT_Y16    v4l2_fourcc('Y', '1', '6', ' ')    /* 16  Greyscale     */
#define V4L2_PIX_FMT_Y16_BE v4l2_fourcc_be('Y', '1', '6', ' ') /* 16  Greyscale BE  */

/* Grey bit-packed formats */
#define V4L2_PIX_FMT_Y10BPACK v4l2_fourcc('Y', '1', '0', 'B') /* 10  Greyscale bit-packed */
#define V4L2_PIX_FMT_Y10P     v4l2_fourcc('Y', '1', '0', 'P') /* 10  Greyscale, MIPI RAW10 packed */
#define V4L2_PIX_FMT_IPU3_Y10 v4l2_fourcc('i', 'p', '3', 'y') /* IPU3 packed 10-bit greyscale */

/* Palette formats */
#define V4L2_PIX_FMT_PAL8 v4l2_fourcc('P', 'A', 'L', '8') /*  8  8-bit palette */

/* Chrominance formats */
#define V4L2_PIX_FMT_UV8 v4l2_fourcc('U', 'V', '8', ' ') /*  8  UV 4:4 */

/* Luminance+Chrominance formats */
#define V4L2_PIX_FMT_YUYV   v4l2_fourcc('Y', 'U', 'Y', 'V') /* 16  YUV 4:2:2     */
#define V4L2_PIX_FMT_YYUV   v4l2_fourcc('Y', 'Y', 'U', 'V') /* 16  YUV 4:2:2     */
#define V4L2_PIX_FMT_YVYU   v4l2_fourcc('Y', 'V', 'Y', 'U') /* 16 YVU 4:2:2 */
#define V4L2_PIX_FMT_UYVY   v4l2_fourcc('U', 'Y', 'V', 'Y') /* 16  YUV 4:2:2     */
#define V4L2_PIX_FMT_VYUY   v4l2_fourcc('V', 'Y', 'U', 'Y') /* 16  YUV 4:2:2     */
#define V4L2_PIX_FMT_Y41P   v4l2_fourcc('Y', '4', '1', 'P') /* 12  YUV 4:1:1     */
#define V4L2_PIX_FMT_YUV444 v4l2_fourcc('Y', '4', '4', '4') /* 16  xxxxyyyy uuuuvvvv */
#define V4L2_PIX_FMT_YUV555 v4l2_fourcc('Y', 'U', 'V', 'O') /* 16  YUV-5-5-5     */
#define V4L2_PIX_FMT_YUV565 v4l2_fourcc('Y', 'U', 'V', 'P') /* 16  YUV-5-6-5     */
#define V4L2_PIX_FMT_YUV24  v4l2_fourcc('Y', 'U', 'V', '3') /* 24  YUV-8-8-8     */
#define V4L2_PIX_FMT_YUV32  v4l2_fourcc('Y', 'U', 'V', '4') /* 32  YUV-8-8-8-8   */
#define V4L2_PIX_FMT_AYUV32 v4l2_fourcc('A', 'Y', 'U', 'V') /* 32  AYUV-8-8-8-8  */
#define V4L2_PIX_FMT_XYUV32 v4l2_fourcc('X', 'Y', 'U', 'V') /* 32  XYUV-8-8-8-8  */
#define V4L2_PIX_FMT_VUYA32 v4l2_fourcc('V', 'U', 'Y', 'A') /* 32  VUYA-8-8-8-8  */
#define V4L2_PIX_FMT_VUYX32 v4l2_fourcc('V', 'U', 'Y', 'X') /* 32  VUYX-8-8-8-8  */
#define V4L2_PIX_FMT_YUVA32 v4l2_fourcc('Y', 'U', 'V', 'A') /* 32  YUVA-8-8-8-8  */
#define V4L2_PIX_FMT_YUVX32 v4l2_fourcc('Y', 'U', 'V', 'X') /* 32  YUVX-8-8-8-8  */
#define V4L2_PIX_FMT_M420   v4l2_fourcc('M', '4', '2', '0') /* 12  YUV 4:2:0 2 lines y, 1 line uv interleaved */

/* two planes -- one Y, one Cr + Cb interleaved  */
#define V4L2_PIX_FMT_NV12 v4l2_fourcc('N', 'V', '1', '2') /* 12  Y/CbCr 4:2:0  */
#define V4L2_PIX_FMT_NV21 v4l2_fourcc('N', 'V', '2', '1') /* 12  Y/CrCb 4:2:0  */
#define V4L2_PIX_FMT_NV16 v4l2_fourcc('N', 'V', '1', '6') /* 16  Y/CbCr 4:2:2  */
#define V4L2_PIX_FMT_NV61 v4l2_fourcc('N', 'V', '6', '1') /* 16  Y/CrCb 4:2:2  */
#define V4L2_PIX_FMT_NV24 v4l2_fourcc('N', 'V', '2', '4') /* 24  Y/CbCr 4:4:4  */
#define V4L2_PIX_FMT_NV42 v4l2_fourcc('N', 'V', '4', '2') /* 24  Y/CrCb 4:4:4  */

/* two non contiguous planes - one Y, one Cr + Cb interleaved  */
#define V4L2_PIX_FMT_NV12M v4l2_fourcc('N', 'M', '1', '2') /* 12  Y/CbCr 4:2:0  */
#define V4L2_PIX_FMT_NV21M v4l2_fourcc('N', 'M', '2', '1') /* 21  Y/CrCb 4:2:0  */
#define V4L2_PIX_FMT_NV16M v4l2_fourcc('N', 'M', '1', '6') /* 16  Y/CbCr 4:2:2  */
#define V4L2_PIX_FMT_NV61M v4l2_fourcc('N', 'M', '6', '1') /* 16  Y/CrCb 4:2:2  */

/* three planes - Y Cb, Cr */
#define V4L2_PIX_FMT_YUV410  v4l2_fourcc('Y', 'U', 'V', '9') /*  9  YUV 4:1:0     */
#define V4L2_PIX_FMT_YVU410  v4l2_fourcc('Y', 'V', 'U', '9') /*  9  YVU 4:1:0     */
#define V4L2_PIX_FMT_YUV411P v4l2_fourcc('4', '1', '1', 'P') /* 12  YVU411 planar */
#define V4L2_PIX_FMT_YUV420  v4l2_fourcc('Y', 'U', '1', '2') /* 12  YUV 4:2:0     */
#define V4L2_PIX_FMT_YVU420  v4l2_fourcc('Y', 'V', '1', '2') /* 12  YVU 4:2:0     */
#define V4L2_PIX_FMT_YUV422P v4l2_fourcc('4', '2', '2', 'P') /* 16  YVU422 planar */

/* three non contiguous planes - Y, Cb, Cr */
#define V4L2_PIX_FMT_YUV420M v4l2_fourcc('Y', 'M', '1', '2') /* 12  YUV420 planar */
#define V4L2_PIX_FMT_YVU420M v4l2_fourcc('Y', 'M', '2', '1') /* 12  YVU420 planar */
#define V4L2_PIX_FMT_YUV422M v4l2_fourcc('Y', 'M', '1', '6') /* 16  YUV422 planar */
#define V4L2_PIX_FMT_YVU422M v4l2_fourcc('Y', 'M', '6', '1') /* 16  YVU422 planar */
#define V4L2_PIX_FMT_YUV444M v4l2_fourcc('Y', 'M', '2', '4') /* 24  YUV444 planar */
#define V4L2_PIX_FMT_YVU444M v4l2_fourcc('Y', 'M', '4', '2') /* 24  YVU444 planar */

/* Tiled YUV formats */
#define V4L2_PIX_FMT_NV12_4L4   v4l2_fourcc('V', 'T', '1', '2') /* 12  Y/CbCr 4:2:0  4x4 tiles */
#define V4L2_PIX_FMT_NV12_16L16 v4l2_fourcc('H', 'M', '1', '2') /* 12  Y/CbCr 4:2:0 16x16 tiles */
#define V4L2_PIX_FMT_NV12_32L32 v4l2_fourcc('S', 'T', '1', '2') /* 12  Y/CbCr 4:2:0 32x32 tiles */

/* Tiled YUV formats, non contiguous planes */
#define V4L2_PIX_FMT_NV12MT           v4l2_fourcc('T', 'M', '1', '2')    /* 12  Y/CbCr 4:2:0 64x32 tiles */
#define V4L2_PIX_FMT_NV12MT_16X16     v4l2_fourcc('V', 'M', '1', '2')    /* 12  Y/CbCr 4:2:0 16x16 tiles */
#define V4L2_PIX_FMT_NV12M_8L128      v4l2_fourcc('N', 'A', '1', '2')    /* Y/CbCr 4:2:0 8x128 tiles */
#define V4L2_PIX_FMT_NV12M_10BE_8L128 v4l2_fourcc_be('N', 'T', '1', '2') /* Y/CbCr 4:2:0 10-bit 8x128 tiles */

/* Bayer formats - see http://www.siliconimaging.com/RGB%20Bayer.htm */
#define V4L2_PIX_FMT_SBGGR8  v4l2_fourcc('B', 'A', '8', '1') /*  8  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG8  v4l2_fourcc('G', 'B', 'R', 'G') /*  8  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_SGRBG8  v4l2_fourcc('G', 'R', 'B', 'G') /*  8  GRGR.. BGBG.. */
#define V4L2_PIX_FMT_SRGGB8  v4l2_fourcc('R', 'G', 'G', 'B') /*  8  RGRG.. GBGB.. */
#define V4L2_PIX_FMT_SBGGR10 v4l2_fourcc('B', 'G', '1', '0') /* 10  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG10 v4l2_fourcc('G', 'B', '1', '0') /* 10  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_SGRBG10 v4l2_fourcc('B', 'A', '1', '0') /* 10  GRGR.. BGBG.. */
#define V4L2_PIX_FMT_SRGGB10 v4l2_fourcc('R', 'G', '1', '0') /* 10  RGRG.. GBGB.. */
                                                             /* 10bit raw bayer packed, 5 bytes for every 4 pixels */
#define V4L2_PIX_FMT_SBGGR10P v4l2_fourcc('p', 'B', 'A', 'A')
#define V4L2_PIX_FMT_SGBRG10P v4l2_fourcc('p', 'G', 'A', 'A')
#define V4L2_PIX_FMT_SGRBG10P v4l2_fourcc('p', 'g', 'A', 'A')
#define V4L2_PIX_FMT_SRGGB10P v4l2_fourcc('p', 'R', 'A', 'A')
/* 10bit raw bayer a-law compressed to 8 bits */
#define V4L2_PIX_FMT_SBGGR10ALAW8 v4l2_fourcc('a', 'B', 'A', '8')
#define V4L2_PIX_FMT_SGBRG10ALAW8 v4l2_fourcc('a', 'G', 'A', '8')
#define V4L2_PIX_FMT_SGRBG10ALAW8 v4l2_fourcc('a', 'g', 'A', '8')
#define V4L2_PIX_FMT_SRGGB10ALAW8 v4l2_fourcc('a', 'R', 'A', '8')
/* 10bit raw bayer DPCM compressed to 8 bits */
#define V4L2_PIX_FMT_SBGGR10DPCM8 v4l2_fourcc('b', 'B', 'A', '8')
#define V4L2_PIX_FMT_SGBRG10DPCM8 v4l2_fourcc('b', 'G', 'A', '8')
#define V4L2_PIX_FMT_SGRBG10DPCM8 v4l2_fourcc('B', 'D', '1', '0')
#define V4L2_PIX_FMT_SRGGB10DPCM8 v4l2_fourcc('b', 'R', 'A', '8')
#define V4L2_PIX_FMT_SBGGR12      v4l2_fourcc('B', 'G', '1', '2') /* 12  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG12      v4l2_fourcc('G', 'B', '1', '2') /* 12  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_SGRBG12      v4l2_fourcc('B', 'A', '1', '2') /* 12  GRGR.. BGBG.. */
#define V4L2_PIX_FMT_SRGGB12      v4l2_fourcc('R', 'G', '1', '2') /* 12  RGRG.. GBGB.. */
/* 12bit raw bayer packed, 6 bytes for every 4 pixels */
#define V4L2_PIX_FMT_SBGGR12P v4l2_fourcc('p', 'B', 'C', 'C')
#define V4L2_PIX_FMT_SGBRG12P v4l2_fourcc('p', 'G', 'C', 'C')
#define V4L2_PIX_FMT_SGRBG12P v4l2_fourcc('p', 'g', 'C', 'C')
#define V4L2_PIX_FMT_SRGGB12P v4l2_fourcc('p', 'R', 'C', 'C')
#define V4L2_PIX_FMT_SBGGR14  v4l2_fourcc('B', 'G', '1', '4') /* 14  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG14  v4l2_fourcc('G', 'B', '1', '4') /* 14  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_SGRBG14  v4l2_fourcc('G', 'R', '1', '4') /* 14  GRGR.. BGBG.. */
#define V4L2_PIX_FMT_SRGGB14  v4l2_fourcc('R', 'G', '1', '4') /* 14  RGRG.. GBGB.. */
                                                              /* 14bit raw bayer packed, 7 bytes for every 4 pixels */
#define V4L2_PIX_FMT_SBGGR14P v4l2_fourcc('p', 'B', 'E', 'E')
#define V4L2_PIX_FMT_SGBRG14P v4l2_fourcc('p', 'G', 'E', 'E')
#define V4L2_PIX_FMT_SGRBG14P v4l2_fourcc('p', 'g', 'E', 'E')
#define V4L2_PIX_FMT_SRGGB14P v4l2_fourcc('p', 'R', 'E', 'E')
#define V4L2_PIX_FMT_SBGGR16  v4l2_fourcc('B', 'Y', 'R', '2') /* 16  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG16  v4l2_fourcc('G', 'B', '1', '6') /* 16  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_SGRBG16  v4l2_fourcc('G', 'R', '1', '6') /* 16  GRGR.. BGBG.. */
#define V4L2_PIX_FMT_SRGGB16  v4l2_fourcc('R', 'G', '1', '6') /* 16  RGRG.. GBGB.. */

/* HSV formats */
#define V4L2_PIX_FMT_HSV24 v4l2_fourcc('H', 'S', 'V', '3')
#define V4L2_PIX_FMT_HSV32 v4l2_fourcc('H', 'S', 'V', '4')

/* compressed formats */
#define V4L2_PIX_FMT_MJPEG          v4l2_fourcc('M', 'J', 'P', 'G') /* Motion-JPEG   */
#define V4L2_PIX_FMT_JPEG           v4l2_fourcc('J', 'P', 'E', 'G') /* JFIF JPEG     */
#define V4L2_PIX_FMT_DV             v4l2_fourcc('d', 'v', 's', 'd') /* 1394          */
#define V4L2_PIX_FMT_MPEG           v4l2_fourcc('M', 'P', 'E', 'G') /* MPEG-1/2/4 Multiplexed */
#define V4L2_PIX_FMT_H264           v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */
#define V4L2_PIX_FMT_H264_NO_SC     v4l2_fourcc('A', 'V', 'C', '1') /* H264 without start codes */
#define V4L2_PIX_FMT_H264_MVC       v4l2_fourcc('M', '2', '6', '4') /* H264 MVC */
#define V4L2_PIX_FMT_H263           v4l2_fourcc('H', '2', '6', '3') /* H263          */
#define V4L2_PIX_FMT_MPEG1          v4l2_fourcc('M', 'P', 'G', '1') /* MPEG-1 ES     */
#define V4L2_PIX_FMT_MPEG2          v4l2_fourcc('M', 'P', 'G', '2') /* MPEG-2 ES     */
#define V4L2_PIX_FMT_MPEG2_SLICE    v4l2_fourcc('M', 'G', '2', 'S') /* MPEG-2 parsed slice data */
#define V4L2_PIX_FMT_MPEG4          v4l2_fourcc('M', 'P', 'G', '4') /* MPEG-4 part 2 ES */
#define V4L2_PIX_FMT_XVID           v4l2_fourcc('X', 'V', 'I', 'D') /* Xvid           */
#define V4L2_PIX_FMT_VC1_ANNEX_G    v4l2_fourcc('V', 'C', '1', 'G') /* SMPTE 421M Annex G compliant stream */
#define V4L2_PIX_FMT_VC1_ANNEX_L    v4l2_fourcc('V', 'C', '1', 'L') /* SMPTE 421M Annex L compliant stream */
#define V4L2_PIX_FMT_VP8            v4l2_fourcc('V', 'P', '8', '0') /* VP8 */
#define V4L2_PIX_FMT_VP8_FRAME      v4l2_fourcc('V', 'P', '8', 'F') /* VP8 parsed frame */
#define V4L2_PIX_FMT_VP9            v4l2_fourcc('V', 'P', '9', '0') /* VP9 */
#define V4L2_PIX_FMT_VP9_FRAME      v4l2_fourcc('V', 'P', '9', 'F') /* VP9 parsed frame */
#define V4L2_PIX_FMT_HEVC           v4l2_fourcc('H', 'E', 'V', 'C') /* HEVC aka H.265 */
#define V4L2_PIX_FMT_FWHT           v4l2_fourcc('F', 'W', 'H', 'T') /* Fast Walsh Hadamard Transform (vicodec) */
#define V4L2_PIX_FMT_FWHT_STATELESS v4l2_fourcc('S', 'F', 'W', 'H') /* Stateless FWHT (vicodec) */
#define V4L2_PIX_FMT_H264_SLICE     v4l2_fourcc('S', '2', '6', '4') /* H264 parsed slices */

/*  Vendor-specific formats   */
#define V4L2_PIX_FMT_CPIA1        v4l2_fourcc('C', 'P', 'I', 'A') /* cpia1 YUV */
#define V4L2_PIX_FMT_WNVA         v4l2_fourcc('W', 'N', 'V', 'A') /* Winnov hw compress */
#define V4L2_PIX_FMT_SN9C10X      v4l2_fourcc('S', '9', '1', '0') /* SN9C10x compression */
#define V4L2_PIX_FMT_SN9C20X_I420 v4l2_fourcc('S', '9', '2', '0') /* SN9C20x YUV 4:2:0 */
#define V4L2_PIX_FMT_PWC1         v4l2_fourcc('P', 'W', 'C', '1') /* pwc older webcam */
#define V4L2_PIX_FMT_PWC2         v4l2_fourcc('P', 'W', 'C', '2') /* pwc newer webcam */
#define V4L2_PIX_FMT_ET61X251     v4l2_fourcc('E', '6', '2', '5') /* ET61X251 compression */
#define V4L2_PIX_FMT_SPCA501      v4l2_fourcc('S', '5', '0', '1') /* YUYV per line */
#define V4L2_PIX_FMT_SPCA505      v4l2_fourcc('S', '5', '0', '5') /* YYUV per line */
#define V4L2_PIX_FMT_SPCA508      v4l2_fourcc('S', '5', '0', '8') /* YUVY per line */
#define V4L2_PIX_FMT_SPCA561      v4l2_fourcc('S', '5', '6', '1') /* compressed GBRG bayer */
#define V4L2_PIX_FMT_PAC207       v4l2_fourcc('P', '2', '0', '7') /* compressed BGGR bayer */
#define V4L2_PIX_FMT_MR97310A     v4l2_fourcc('M', '3', '1', '0') /* compressed BGGR bayer */
#define V4L2_PIX_FMT_JL2005BCD    v4l2_fourcc('J', 'L', '2', '0') /* compressed RGGB bayer */
#define V4L2_PIX_FMT_SN9C2028     v4l2_fourcc('S', 'O', 'N', 'X') /* compressed GBRG bayer */
#define V4L2_PIX_FMT_SQ905C       v4l2_fourcc('9', '0', '5', 'C') /* compressed RGGB bayer */
#define V4L2_PIX_FMT_PJPG         v4l2_fourcc('P', 'J', 'P', 'G') /* Pixart 73xx JPEG */
#define V4L2_PIX_FMT_OV511        v4l2_fourcc('O', '5', '1', '1') /* ov511 JPEG */
#define V4L2_PIX_FMT_OV518        v4l2_fourcc('O', '5', '1', '8') /* ov518 JPEG */
#define V4L2_PIX_FMT_STV0680      v4l2_fourcc('S', '6', '8', '0') /* stv0680 bayer */
#define V4L2_PIX_FMT_TM6000       v4l2_fourcc('T', 'M', '6', '0') /* tm5600/tm60x0 */
#define V4L2_PIX_FMT_CIT_YYVYUY   v4l2_fourcc('C', 'I', 'T', 'V') /* one line of Y then 1 line of VYUY */
#define V4L2_PIX_FMT_KONICA420    v4l2_fourcc('K', 'O', 'N', 'I') /* YUV420 planar in blocks of 256 pixels */
#define V4L2_PIX_FMT_JPGL         v4l2_fourcc('J', 'P', 'G', 'L') /* JPEG-Lite */
#define V4L2_PIX_FMT_SE401        v4l2_fourcc('S', '4', '0', '1') /* se401 janggu compressed rgb */
#define V4L2_PIX_FMT_S5C_UYVY_JPG v4l2_fourcc('S', '5', 'C', 'I') /* S5C73M3 interleaved UYVY/JPEG */
#define V4L2_PIX_FMT_Y8I          v4l2_fourcc('Y', '8', 'I', ' ') /* Greyscale 8-bit L/R interleaved */
#define V4L2_PIX_FMT_Y12I         v4l2_fourcc('Y', '1', '2', 'I') /* Greyscale 12-bit L/R interleaved */
#define V4L2_PIX_FMT_Z16          v4l2_fourcc('Z', '1', '6', ' ') /* Depth data 16-bit */
#define V4L2_PIX_FMT_MT21C        v4l2_fourcc('M', 'T', '2', '1') /* Mediatek compressed block mode  */
#define V4L2_PIX_FMT_MM21         v4l2_fourcc('M', 'M', '2', '1') /* Mediatek 8-bit block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_INZI         v4l2_fourcc('I', 'N', 'Z', 'I') /* Intel Planar Greyscale 10-bit and Depth 16-bit */
#define V4L2_PIX_FMT_CNF4         v4l2_fourcc('C', 'N', 'F', '4') /* Intel 4-bit packed depth confidence information */
#define V4L2_PIX_FMT_HI240        v4l2_fourcc('H', 'I', '2', '4') /* BTTV 8-bit dithered RGB */
#define V4L2_PIX_FMT_QC08C        v4l2_fourcc('Q', '0', '8', 'C') /* Qualcomm 8-bit compressed */
#define V4L2_PIX_FMT_QC10C        v4l2_fourcc('Q', '1', '0', 'C') /* Qualcomm 10-bit compressed */

/* 10bit raw packed, 32 bytes for every 25 pixels, last LSB 6 bits unused */
#define V4L2_PIX_FMT_IPU3_SBGGR10 v4l2_fourcc('i', 'p', '3', 'b') /* IPU3 packed 10-bit BGGR bayer */
#define V4L2_PIX_FMT_IPU3_SGBRG10 v4l2_fourcc('i', 'p', '3', 'g') /* IPU3 packed 10-bit GBRG bayer */
#define V4L2_PIX_FMT_IPU3_SGRBG10 v4l2_fourcc('i', 'p', '3', 'G') /* IPU3 packed 10-bit GRBG bayer */
#define V4L2_PIX_FMT_IPU3_SRGGB10 v4l2_fourcc('i', 'p', '3', 'r') /* IPU3 packed 10-bit RGGB bayer */

/* SDR formats - used only for Software Defined Radio devices */
#define V4L2_SDR_FMT_CU8     v4l2_fourcc('C', 'U', '0', '8') /* IQ u8 */
#define V4L2_SDR_FMT_CU16LE  v4l2_fourcc('C', 'U', '1', '6') /* IQ u16le */
#define V4L2_SDR_FMT_CS8     v4l2_fourcc('C', 'S', '0', '8') /* complex s8 */
#define V4L2_SDR_FMT_CS14LE  v4l2_fourcc('C', 'S', '1', '4') /* complex s14le */
#define V4L2_SDR_FMT_RU12LE  v4l2_fourcc('R', 'U', '1', '2') /* real u12le */
#define V4L2_SDR_FMT_PCU16BE v4l2_fourcc('P', 'C', '1', '6') /* planar complex u16be */
#define V4L2_SDR_FMT_PCU18BE v4l2_fourcc('P', 'C', '1', '8') /* planar complex u18be */
#define V4L2_SDR_FMT_PCU20BE v4l2_fourcc('P', 'C', '2', '0') /* planar complex u20be */

/* Touch formats - used for Touch devices */
#define V4L2_TCH_FMT_DELTA_TD16 v4l2_fourcc('T', 'D', '1', '6') /* 16-bit signed deltas */
#define V4L2_TCH_FMT_DELTA_TD08 v4l2_fourcc('T', 'D', '0', '8') /* 8-bit signed deltas */
#define V4L2_TCH_FMT_TU16       v4l2_fourcc('T', 'U', '1', '6') /* 16-bit unsigned touch data */
#define V4L2_TCH_FMT_TU08       v4l2_fourcc('T', 'U', '0', '8') /* 8-bit unsigned touch data */

/* Meta-data formats */
#define V4L2_META_FMT_VSP1_HGO          v4l2_fourcc('V', 'S', 'P', 'H') /* R-Car VSP1 1-D Histogram */
#define V4L2_META_FMT_VSP1_HGT          v4l2_fourcc('V', 'S', 'P', 'T') /* R-Car VSP1 2-D Histogram */
#define V4L2_META_FMT_UVC               v4l2_fourcc('U', 'V', 'C', 'H') /* UVC Payload Header metadata */
#define V4L2_META_FMT_D4XX              v4l2_fourcc('D', '4', 'X', 'X') /* D4XX Payload Header metadata */
#define V4L2_META_FMT_VIVID             v4l2_fourcc('V', 'I', 'V', 'D') /* Vivid Metadata */
#define V4L2_META_FMT_SENSOR_DATA       v4l2_fourcc('S', 'E', 'N', 'S') /* Sensor Ancillary metadata */
#define V4L2_META_FMT_BCM2835_ISP_STATS v4l2_fourcc('B', 'S', 'T', 'A') /* BCM2835 ISP image statistics output */

/* Vendor specific - used for RK_ISP1 camera sub-system */
#define V4L2_META_FMT_RK_ISP1_PARAMS  v4l2_fourcc('R', 'K', '1', 'P') /* Rockchip ISP1 3A Parameters */
#define V4L2_META_FMT_RK_ISP1_STAT_3A v4l2_fourcc('R', 'K', '1', 'S') /* Rockchip ISP1 3A Statistics */

/* priv field value to indicates that subsequent fields are valid. */
#define V4L2_PIX_FMT_PRIV_MAGIC 0xfeedcafe

/* Flags */
#define V4L2_PIX_FMT_FLAG_PREMUL_ALPHA 0x00000001
#define V4L2_PIX_FMT_FLAG_SET_CSC      0x00000002
/* --- multiplanar pix --- */
struct v4l2_plane_pix_format {
    __u32 sizeimage;
    __u32 bytesperline;
    __u16 reserved[6];
} __attribute__((packed));

struct v4l2_pix_format_mplane {
    __u32 width;
    __u32 height;
    __u32 pixelformat;
    __u32 field;
    __u32 colorspace;
    struct v4l2_plane_pix_format plane_fmt[VIDEO_MAX_PLANES];
    __u8  num_planes;
    __u8  flags;
    __u8  ycbcr_enc;
    __u8  quantization;
    __u8  xfer_func;
    __u8  reserved[7];
} __attribute__((packed));
/* RGB formats (1 or 2 bytes per pixel) */
#define V4L2_PIX_FMT_RGB332   v4l2_fourcc('R', 'G', 'B', '1')    /*  8  RGB-3-3-2     */
#define V4L2_PIX_FMT_RGB444   v4l2_fourcc('R', '4', '4', '4')    /* 16  xxxxrrrr ggggbbbb */
#define V4L2_PIX_FMT_ARGB444  v4l2_fourcc('A', 'R', '1', '2')    /* 16  aaaarrrr ggggbbbb */
#define V4L2_PIX_FMT_XRGB444  v4l2_fourcc('X', 'R', '1', '2')    /* 16  xxxxrrrr ggggbbbb */
#define V4L2_PIX_FMT_RGBA444  v4l2_fourcc('R', 'A', '1', '2')    /* 16  rrrrgggg bbbbaaaa */
#define V4L2_PIX_FMT_RGBX444  v4l2_fourcc('R', 'X', '1', '2')    /* 16  rrrrgggg bbbbxxxx */
#define V4L2_PIX_FMT_ABGR444  v4l2_fourcc('A', 'B', '1', '2')    /* 16  aaaabbbb ggggrrrr */
#define V4L2_PIX_FMT_XBGR444  v4l2_fourcc('X', 'B', '1', '2')    /* 16  xxxxbbbb ggggrrrr */
#define V4L2_PIX_FMT_BGRA444  v4l2_fourcc('G', 'A', '1', '2')    /* 16  bbbbgggg rrrraaaa */
#define V4L2_PIX_FMT_BGRX444  v4l2_fourcc('B', 'X', '1', '2')    /* 16  bbbbgggg rrrrxxxx */
#define V4L2_PIX_FMT_RGB555   v4l2_fourcc('R', 'G', 'B', 'O')    /* 16  RGB-5-5-5     */
#define V4L2_PIX_FMT_ARGB555  v4l2_fourcc('A', 'R', '1', '5')    /* 16  ARGB-1-5-5-5  */
#define V4L2_PIX_FMT_XRGB555  v4l2_fourcc('X', 'R', '1', '5')    /* 16  XRGB-1-5-5-5  */
#define V4L2_PIX_FMT_RGBA555  v4l2_fourcc('R', 'A', '1', '5')    /* 16  RGBA-5-5-5-1  */
#define V4L2_PIX_FMT_RGBX555  v4l2_fourcc('R', 'X', '1', '5')    /* 16  RGBX-5-5-5-1  */
#define V4L2_PIX_FMT_ABGR555  v4l2_fourcc('A', 'B', '1', '5')    /* 16  ABGR-1-5-5-5  */
#define V4L2_PIX_FMT_XBGR555  v4l2_fourcc('X', 'B', '1', '5')    /* 16  XBGR-1-5-5-5  */
#define V4L2_PIX_FMT_BGRA555  v4l2_fourcc('B', 'A', '1', '5')    /* 16  BGRA-5-5-5-1  */
#define V4L2_PIX_FMT_BGRX555  v4l2_fourcc('B', 'X', '1', '5')    /* 16  BGRX-5-5-5-1  */
#define V4L2_PIX_FMT_RGB565   v4l2_fourcc('R', 'G', 'B', 'P')    /* 16  RGB-5-6-5     */
#define V4L2_PIX_FMT_RGB555X  v4l2_fourcc('R', 'G', 'B', 'Q')    /* 16  RGB-5-5-5 BE  */
#define V4L2_PIX_FMT_ARGB555X v4l2_fourcc_be('A', 'R', '1', '5') /* 16  ARGB-5-5-5 BE */
#define V4L2_PIX_FMT_XRGB555X v4l2_fourcc_be('X', 'R', '1', '5') /* 16  XRGB-5-5-5 BE */
#define V4L2_PIX_FMT_RGB565X  v4l2_fourcc('R', 'G', 'B', 'R')    /* 16  RGB-5-6-5 BE  */

/* RGB formats (3 or 4 bytes per pixel) */
#define V4L2_PIX_FMT_BGR666 v4l2_fourcc('B', 'G', 'R', 'H') /* 18  BGR-6-6-6	  */
#define V4L2_PIX_FMT_BGR24  v4l2_fourcc('B', 'G', 'R', '3') /* 24  BGR-8-8-8     */
#define V4L2_PIX_FMT_RGB24  v4l2_fourcc('R', 'G', 'B', '3') /* 24  RGB-8-8-8     */
#define V4L2_PIX_FMT_BGR32  v4l2_fourcc('B', 'G', 'R', '4') /* 32  BGR-8-8-8-8   */
#define V4L2_PIX_FMT_ABGR32 v4l2_fourcc('A', 'R', '2', '4') /* 32  BGRA-8-8-8-8  */
#define V4L2_PIX_FMT_XBGR32 v4l2_fourcc('X', 'R', '2', '4') /* 32  BGRX-8-8-8-8  */
#define V4L2_PIX_FMT_BGRA32 v4l2_fourcc('R', 'A', '2', '4') /* 32  ABGR-8-8-8-8  */
#define V4L2_PIX_FMT_BGRX32 v4l2_fourcc('R', 'X', '2', '4') /* 32  XBGR-8-8-8-8  */
#define V4L2_PIX_FMT_RGB32  v4l2_fourcc('R', 'G', 'B', '4') /* 32  RGB-8-8-8-8   */
#define V4L2_PIX_FMT_RGBA32 v4l2_fourcc('A', 'B', '2', '4') /* 32  RGBA-8-8-8-8  */
#define V4L2_PIX_FMT_RGBX32 v4l2_fourcc('X', 'B', '2', '4') /* 32  RGBX-8-8-8-8  */
#define V4L2_PIX_FMT_ARGB32 v4l2_fourcc('B', 'A', '2', '4') /* 32  ARGB-8-8-8-8  */
#define V4L2_PIX_FMT_XRGB32 v4l2_fourcc('B', 'X', '2', '4') /* 32  XRGB-8-8-8-8  */

/* Grey formats */
#define V4L2_PIX_FMT_GREY   v4l2_fourcc('G', 'R', 'E', 'Y')    /*  8  Greyscale     */
#define V4L2_PIX_FMT_Y4     v4l2_fourcc('Y', '0', '4', ' ')    /*  4  Greyscale     */
#define V4L2_PIX_FMT_Y6     v4l2_fourcc('Y', '0', '6', ' ')    /*  6  Greyscale     */
#define V4L2_PIX_FMT_Y10    v4l2_fourcc('Y', '1', '0', ' ')    /* 10  Greyscale     */
#define V4L2_PIX_FMT_Y12    v4l2_fourcc('Y', '1', '2', ' ')    /* 12  Greyscale     */
#define V4L2_PIX_FMT_Y14    v4l2_fourcc('Y', '1', '4', ' ')    /* 14  Greyscale     */
#define V4L2_PIX_FMT_Y16    v4l2_fourcc('Y', '1', '6', ' ')    /* 16  Greyscale     */
#define V4L2_PIX_FMT_Y16_BE v4l2_fourcc_be('Y', '1', '6', ' ') /* 16  Greyscale BE  */

/* Grey bit-packed formats */
#define V4L2_PIX_FMT_Y10BPACK v4l2_fourcc('Y', '1', '0', 'B') /* 10  Greyscale bit-packed */
#define V4L2_PIX_FMT_Y10P     v4l2_fourcc('Y', '1', '0', 'P') /* 10  Greyscale, MIPI RAW10 packed */
#define V4L2_PIX_FMT_IPU3_Y10 v4l2_fourcc('i', 'p', '3', 'y') /* IPU3 packed 10-bit greyscale */

/* Palette formats */
#define V4L2_PIX_FMT_PAL8 v4l2_fourcc('P', 'A', 'L', '8') /*  8  8-bit palette */

/* Chrominance formats */
#define V4L2_PIX_FMT_UV8 v4l2_fourcc('U', 'V', '8', ' ') /*  8  UV 4:4 */

/* Luminance+Chrominance formats */
#define V4L2_PIX_FMT_YUYV   v4l2_fourcc('Y', 'U', 'Y', 'V') /* 16  YUV 4:2:2     */
#define V4L2_PIX_FMT_YYUV   v4l2_fourcc('Y', 'Y', 'U', 'V') /* 16  YUV 4:2:2     */
#define V4L2_PIX_FMT_YVYU   v4l2_fourcc('Y', 'V', 'Y', 'U') /* 16 YVU 4:2:2 */
#define V4L2_PIX_FMT_UYVY   v4l2_fourcc('U', 'Y', 'V', 'Y') /* 16  YUV 4:2:2     */
#define V4L2_PIX_FMT_VYUY   v4l2_fourcc('V', 'Y', 'U', 'Y') /* 16  YUV 4:2:2     */
#define V4L2_PIX_FMT_Y41P   v4l2_fourcc('Y', '4', '1', 'P') /* 12  YUV 4:1:1     */
#define V4L2_PIX_FMT_YUV444 v4l2_fourcc('Y', '4', '4', '4') /* 16  xxxxyyyy uuuuvvvv */
#define V4L2_PIX_FMT_YUV555 v4l2_fourcc('Y', 'U', 'V', 'O') /* 16  YUV-5-5-5     */
#define V4L2_PIX_FMT_YUV565 v4l2_fourcc('Y', 'U', 'V', 'P') /* 16  YUV-5-6-5     */
#define V4L2_PIX_FMT_YUV24  v4l2_fourcc('Y', 'U', 'V', '3') /* 24  YUV-8-8-8     */
#define V4L2_PIX_FMT_YUV32  v4l2_fourcc('Y', 'U', 'V', '4') /* 32  YUV-8-8-8-8   */
#define V4L2_PIX_FMT_AYUV32 v4l2_fourcc('A', 'Y', 'U', 'V') /* 32  AYUV-8-8-8-8  */
#define V4L2_PIX_FMT_XYUV32 v4l2_fourcc('X', 'Y', 'U', 'V') /* 32  XYUV-8-8-8-8  */
#define V4L2_PIX_FMT_VUYA32 v4l2_fourcc('V', 'U', 'Y', 'A') /* 32  VUYA-8-8-8-8  */
#define V4L2_PIX_FMT_VUYX32 v4l2_fourcc('V', 'U', 'Y', 'X') /* 32  VUYX-8-8-8-8  */
#define V4L2_PIX_FMT_YUVA32 v4l2_fourcc('Y', 'U', 'V', 'A') /* 32  YUVA-8-8-8-8  */
#define V4L2_PIX_FMT_YUVX32 v4l2_fourcc('Y', 'U', 'V', 'X') /* 32  YUVX-8-8-8-8  */
#define V4L2_PIX_FMT_M420   v4l2_fourcc('M', '4', '2', '0') /* 12  YUV 4:2:0 2 lines y, 1 line uv interleaved */

/* two planes -- one Y, one Cr + Cb interleaved  */
#define V4L2_PIX_FMT_NV12 v4l2_fourcc('N', 'V', '1', '2') /* 12  Y/CbCr 4:2:0  */
#define V4L2_PIX_FMT_NV21 v4l2_fourcc('N', 'V', '2', '1') /* 12  Y/CrCb 4:2:0  */
#define V4L2_PIX_FMT_NV16 v4l2_fourcc('N', 'V', '1', '6') /* 16  Y/CbCr 4:2:2  */
#define V4L2_PIX_FMT_NV61 v4l2_fourcc('N', 'V', '6', '1') /* 16  Y/CrCb 4:2:2  */
#define V4L2_PIX_FMT_NV24 v4l2_fourcc('N', 'V', '2', '4') /* 24  Y/CbCr 4:4:4  */
#define V4L2_PIX_FMT_NV42 v4l2_fourcc('N', 'V', '4', '2') /* 24  Y/CrCb 4:4:4  */

/* two non contiguous planes - one Y, one Cr + Cb interleaved  */
#define V4L2_PIX_FMT_NV12M v4l2_fourcc('N', 'M', '1', '2') /* 12  Y/CbCr 4:2:0  */
#define V4L2_PIX_FMT_NV21M v4l2_fourcc('N', 'M', '2', '1') /* 21  Y/CrCb 4:2:0  */
#define V4L2_PIX_FMT_NV16M v4l2_fourcc('N', 'M', '1', '6') /* 16  Y/CbCr 4:2:2  */
#define V4L2_PIX_FMT_NV61M v4l2_fourcc('N', 'M', '6', '1') /* 16  Y/CrCb 4:2:2  */

/* three planes - Y Cb, Cr */
#define V4L2_PIX_FMT_YUV410  v4l2_fourcc('Y', 'U', 'V', '9') /*  9  YUV 4:1:0     */
#define V4L2_PIX_FMT_YVU410  v4l2_fourcc('Y', 'V', 'U', '9') /*  9  YVU 4:1:0     */
#define V4L2_PIX_FMT_YUV411P v4l2_fourcc('4', '1', '1', 'P') /* 12  YVU411 planar */
#define V4L2_PIX_FMT_YUV420  v4l2_fourcc('Y', 'U', '1', '2') /* 12  YUV 4:2:0     */
#define V4L2_PIX_FMT_YVU420  v4l2_fourcc('Y', 'V', '1', '2') /* 12  YVU 4:2:0     */
#define V4L2_PIX_FMT_YUV422P v4l2_fourcc('4', '2', '2', 'P') /* 16  YVU422 planar */

/* three non contiguous planes - Y, Cb, Cr */
#define V4L2_PIX_FMT_YUV420M v4l2_fourcc('Y', 'M', '1', '2') /* 12  YUV420 planar */
#define V4L2_PIX_FMT_YVU420M v4l2_fourcc('Y', 'M', '2', '1') /* 12  YVU420 planar */
#define V4L2_PIX_FMT_YUV422M v4l2_fourcc('Y', 'M', '1', '6') /* 16  YUV422 planar */
#define V4L2_PIX_FMT_YVU422M v4l2_fourcc('Y', 'M', '6', '1') /* 16  YVU422 planar */
#define V4L2_PIX_FMT_YUV444M v4l2_fourcc('Y', 'M', '2', '4') /* 24  YUV444 planar */
#define V4L2_PIX_FMT_YVU444M v4l2_fourcc('Y', 'M', '4', '2') /* 24  YVU444 planar */

/* Tiled YUV formats */
#define V4L2_PIX_FMT_NV12_4L4   v4l2_fourcc('V', 'T', '1', '2') /* 12  Y/CbCr 4:2:0  4x4 tiles */
#define V4L2_PIX_FMT_NV12_16L16 v4l2_fourcc('H', 'M', '1', '2') /* 12  Y/CbCr 4:2:0 16x16 tiles */
#define V4L2_PIX_FMT_NV12_32L32 v4l2_fourcc('S', 'T', '1', '2') /* 12  Y/CbCr 4:2:0 32x32 tiles */

/* Tiled YUV formats, non contiguous planes */
#define V4L2_PIX_FMT_NV12MT           v4l2_fourcc('T', 'M', '1', '2')    /* 12  Y/CbCr 4:2:0 64x32 tiles */
#define V4L2_PIX_FMT_NV12MT_16X16     v4l2_fourcc('V', 'M', '1', '2')    /* 12  Y/CbCr 4:2:0 16x16 tiles */
#define V4L2_PIX_FMT_NV12M_8L128      v4l2_fourcc('N', 'A', '1', '2')    /* Y/CbCr 4:2:0 8x128 tiles */
#define V4L2_PIX_FMT_NV12M_10BE_8L128 v4l2_fourcc_be('N', 'T', '1', '2') /* Y/CbCr 4:2:0 10-bit 8x128 tiles */

/* Bayer formats - see http://www.siliconimaging.com/RGB%20Bayer.htm */
#define V4L2_PIX_FMT_SBGGR8  v4l2_fourcc('B', 'A', '8', '1') /*  8  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG8  v4l2_fourcc('G', 'B', 'R', 'G') /*  8  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_SGRBG8  v4l2_fourcc('G', 'R', 'B', 'G') /*  8  GRGR.. BGBG.. */
#define V4L2_PIX_FMT_SRGGB8  v4l2_fourcc('R', 'G', 'G', 'B') /*  8  RGRG.. GBGB.. */
#define V4L2_PIX_FMT_SBGGR10 v4l2_fourcc('B', 'G', '1', '0') /* 10  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG10 v4l2_fourcc('G', 'B', '1', '0') /* 10  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_SGRBG10 v4l2_fourcc('B', 'A', '1', '0') /* 10  GRGR.. BGBG.. */
#define V4L2_PIX_FMT_SRGGB10 v4l2_fourcc('R', 'G', '1', '0') /* 10  RGRG.. GBGB.. */
                                                             /* 10bit raw bayer packed, 5 bytes for every 4 pixels */
#define V4L2_PIX_FMT_SBGGR10P v4l2_fourcc('p', 'B', 'A', 'A')
#define V4L2_PIX_FMT_SGBRG10P v4l2_fourcc('p', 'G', 'A', 'A')
#define V4L2_PIX_FMT_SGRBG10P v4l2_fourcc('p', 'g', 'A', 'A')
#define V4L2_PIX_FMT_SRGGB10P v4l2_fourcc('p', 'R', 'A', 'A')
/* 10bit raw bayer a-law compressed to 8 bits */
#define V4L2_PIX_FMT_SBGGR10ALAW8 v4l2_fourcc('a', 'B', 'A', '8')
#define V4L2_PIX_FMT_SGBRG10ALAW8 v4l2_fourcc('a', 'G', 'A', '8')
#define V4L2_PIX_FMT_SGRBG10ALAW8 v4l2_fourcc('a', 'g', 'A', '8')
#define V4L2_PIX_FMT_SRGGB10ALAW8 v4l2_fourcc('a', 'R', 'A', '8')
/* 10bit raw bayer DPCM compressed to 8 bits */
#define V4L2_PIX_FMT_SBGGR10DPCM8 v4l2_fourcc('b', 'B', 'A', '8')
#define V4L2_PIX_FMT_SGBRG10DPCM8 v4l2_fourcc('b', 'G', 'A', '8')
#define V4L2_PIX_FMT_SGRBG10DPCM8 v4l2_fourcc('B', 'D', '1', '0')
#define V4L2_PIX_FMT_SRGGB10DPCM8 v4l2_fourcc('b', 'R', 'A', '8')
#define V4L2_PIX_FMT_SBGGR12      v4l2_fourcc('B', 'G', '1', '2') /* 12  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG12      v4l2_fourcc('G', 'B', '1', '2') /* 12  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_SGRBG12      v4l2_fourcc('B', 'A', '1', '2') /* 12  GRGR.. BGBG.. */
#define V4L2_PIX_FMT_SRGGB12      v4l2_fourcc('R', 'G', '1', '2') /* 12  RGRG.. GBGB.. */
/* 12bit raw bayer packed, 6 bytes for every 4 pixels */
#define V4L2_PIX_FMT_SBGGR12P v4l2_fourcc('p', 'B', 'C', 'C')
#define V4L2_PIX_FMT_SGBRG12P v4l2_fourcc('p', 'G', 'C', 'C')
#define V4L2_PIX_FMT_SGRBG12P v4l2_fourcc('p', 'g', 'C', 'C')
#define V4L2_PIX_FMT_SRGGB12P v4l2_fourcc('p', 'R', 'C', 'C')
#define V4L2_PIX_FMT_SBGGR14  v4l2_fourcc('B', 'G', '1', '4') /* 14  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG14  v4l2_fourcc('G', 'B', '1', '4') /* 14  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_SGRBG14  v4l2_fourcc('G', 'R', '1', '4') /* 14  GRGR.. BGBG.. */
#define V4L2_PIX_FMT_SRGGB14  v4l2_fourcc('R', 'G', '1', '4') /* 14  RGRG.. GBGB.. */
                                                              /* 14bit raw bayer packed, 7 bytes for every 4 pixels */
#define V4L2_PIX_FMT_SBGGR14P v4l2_fourcc('p', 'B', 'E', 'E')
#define V4L2_PIX_FMT_SGBRG14P v4l2_fourcc('p', 'G', 'E', 'E')
#define V4L2_PIX_FMT_SGRBG14P v4l2_fourcc('p', 'g', 'E', 'E')
#define V4L2_PIX_FMT_SRGGB14P v4l2_fourcc('p', 'R', 'E', 'E')
#define V4L2_PIX_FMT_SBGGR16  v4l2_fourcc('B', 'Y', 'R', '2') /* 16  BGBG.. GRGR.. */
#define V4L2_PIX_FMT_SGBRG16  v4l2_fourcc('G', 'B', '1', '6') /* 16  GBGB.. RGRG.. */
#define V4L2_PIX_FMT_SGRBG16  v4l2_fourcc('G', 'R', '1', '6') /* 16  GRGR.. BGBG.. */
#define V4L2_PIX_FMT_SRGGB16  v4l2_fourcc('R', 'G', '1', '6') /* 16  RGRG.. GBGB.. */

/* HSV formats */
#define V4L2_PIX_FMT_HSV24 v4l2_fourcc('H', 'S', 'V', '3')
#define V4L2_PIX_FMT_HSV32 v4l2_fourcc('H', 'S', 'V', '4')

/* compressed formats */
#define V4L2_PIX_FMT_MJPEG          v4l2_fourcc('M', 'J', 'P', 'G') /* Motion-JPEG   */
#define V4L2_PIX_FMT_JPEG           v4l2_fourcc('J', 'P', 'E', 'G') /* JFIF JPEG     */
#define V4L2_PIX_FMT_DV             v4l2_fourcc('d', 'v', 's', 'd') /* 1394          */
#define V4L2_PIX_FMT_MPEG           v4l2_fourcc('M', 'P', 'E', 'G') /* MPEG-1/2/4 Multiplexed */
#define V4L2_PIX_FMT_H264           v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */
#define V4L2_PIX_FMT_H264_NO_SC     v4l2_fourcc('A', 'V', 'C', '1') /* H264 without start codes */
#define V4L2_PIX_FMT_H264_MVC       v4l2_fourcc('M', '2', '6', '4') /* H264 MVC */
#define V4L2_PIX_FMT_H263           v4l2_fourcc('H', '2', '6', '3') /* H263          */
#define V4L2_PIX_FMT_MPEG1          v4l2_fourcc('M', 'P', 'G', '1') /* MPEG-1 ES     */
#define V4L2_PIX_FMT_MPEG2          v4l2_fourcc('M', 'P', 'G', '2') /* MPEG-2 ES     */
#define V4L2_PIX_FMT_MPEG2_SLICE    v4l2_fourcc('M', 'G', '2', 'S') /* MPEG-2 parsed slice data */
#define V4L2_PIX_FMT_MPEG4          v4l2_fourcc('M', 'P', 'G', '4') /* MPEG-4 part 2 ES */
#define V4L2_PIX_FMT_XVID           v4l2_fourcc('X', 'V', 'I', 'D') /* Xvid           */
#define V4L2_PIX_FMT_VC1_ANNEX_G    v4l2_fourcc('V', 'C', '1', 'G') /* SMPTE 421M Annex G compliant stream */
#define V4L2_PIX_FMT_VC1_ANNEX_L    v4l2_fourcc('V', 'C', '1', 'L') /* SMPTE 421M Annex L compliant stream */
#define V4L2_PIX_FMT_VP8            v4l2_fourcc('V', 'P', '8', '0') /* VP8 */
#define V4L2_PIX_FMT_VP8_FRAME      v4l2_fourcc('V', 'P', '8', 'F') /* VP8 parsed frame */
#define V4L2_PIX_FMT_VP9            v4l2_fourcc('V', 'P', '9', '0') /* VP9 */
#define V4L2_PIX_FMT_VP9_FRAME      v4l2_fourcc('V', 'P', '9', 'F') /* VP9 parsed frame */
#define V4L2_PIX_FMT_HEVC           v4l2_fourcc('H', 'E', 'V', 'C') /* HEVC aka H.265 */
#define V4L2_PIX_FMT_FWHT           v4l2_fourcc('F', 'W', 'H', 'T') /* Fast Walsh Hadamard Transform (vicodec) */
#define V4L2_PIX_FMT_FWHT_STATELESS v4l2_fourcc('S', 'F', 'W', 'H') /* Stateless FWHT (vicodec) */
#define V4L2_PIX_FMT_H264_SLICE     v4l2_fourcc('S', '2', '6', '4') /* H264 parsed slices */

/*  Vendor-specific formats   */
#define V4L2_PIX_FMT_CPIA1        v4l2_fourcc('C', 'P', 'I', 'A') /* cpia1 YUV */
#define V4L2_PIX_FMT_WNVA         v4l2_fourcc('W', 'N', 'V', 'A') /* Winnov hw compress */
#define V4L2_PIX_FMT_SN9C10X      v4l2_fourcc('S', '9', '1', '0') /* SN9C10x compression */
#define V4L2_PIX_FMT_SN9C20X_I420 v4l2_fourcc('S', '9', '2', '0') /* SN9C20x YUV 4:2:0 */
#define V4L2_PIX_FMT_PWC1         v4l2_fourcc('P', 'W', 'C', '1') /* pwc older webcam */
#define V4L2_PIX_FMT_PWC2         v4l2_fourcc('P', 'W', 'C', '2') /* pwc newer webcam */
#define V4L2_PIX_FMT_ET61X251     v4l2_fourcc('E', '6', '2', '5') /* ET61X251 compression */
#define V4L2_PIX_FMT_SPCA501      v4l2_fourcc('S', '5', '0', '1') /* YUYV per line */
#define V4L2_PIX_FMT_SPCA505      v4l2_fourcc('S', '5', '0', '5') /* YYUV per line */
#define V4L2_PIX_FMT_SPCA508      v4l2_fourcc('S', '5', '0', '8') /* YUVY per line */
#define V4L2_PIX_FMT_SPCA561      v4l2_fourcc('S', '5', '6', '1') /* compressed GBRG bayer */
#define V4L2_PIX_FMT_PAC207       v4l2_fourcc('P', '2', '0', '7') /* compressed BGGR bayer */
#define V4L2_PIX_FMT_MR97310A     v4l2_fourcc('M', '3', '1', '0') /* compressed BGGR bayer */
#define V4L2_PIX_FMT_JL2005BCD    v4l2_fourcc('J', 'L', '2', '0') /* compressed RGGB bayer */
#define V4L2_PIX_FMT_SN9C2028     v4l2_fourcc('S', 'O', 'N', 'X') /* compressed GBRG bayer */
#define V4L2_PIX_FMT_SQ905C       v4l2_fourcc('9', '0', '5', 'C') /* compressed RGGB bayer */
#define V4L2_PIX_FMT_PJPG         v4l2_fourcc('P', 'J', 'P', 'G') /* Pixart 73xx JPEG */
#define V4L2_PIX_FMT_OV511        v4l2_fourcc('O', '5', '1', '1') /* ov511 JPEG */
#define V4L2_PIX_FMT_OV518        v4l2_fourcc('O', '5', '1', '8') /* ov518 JPEG */
#define V4L2_PIX_FMT_STV0680      v4l2_fourcc('S', '6', '8', '0') /* stv0680 bayer */
#define V4L2_PIX_FMT_TM6000       v4l2_fourcc('T', 'M', '6', '0') /* tm5600/tm60x0 */
#define V4L2_PIX_FMT_CIT_YYVYUY   v4l2_fourcc('C', 'I', 'T', 'V') /* one line of Y then 1 line of VYUY */
#define V4L2_PIX_FMT_KONICA420    v4l2_fourcc('K', 'O', 'N', 'I') /* YUV420 planar in blocks of 256 pixels */
#define V4L2_PIX_FMT_JPGL         v4l2_fourcc('J', 'P', 'G', 'L') /* JPEG-Lite */
#define V4L2_PIX_FMT_SE401        v4l2_fourcc('S', '4', '0', '1') /* se401 janggu compressed rgb */
#define V4L2_PIX_FMT_S5C_UYVY_JPG v4l2_fourcc('S', '5', 'C', 'I') /* S5C73M3 interleaved UYVY/JPEG */
#define V4L2_PIX_FMT_Y8I          v4l2_fourcc('Y', '8', 'I', ' ') /* Greyscale 8-bit L/R interleaved */
#define V4L2_PIX_FMT_Y12I         v4l2_fourcc('Y', '1', '2', 'I') /* Greyscale 12-bit L/R interleaved */
#define V4L2_PIX_FMT_Z16          v4l2_fourcc('Z', '1', '6', ' ') /* Depth data 16-bit */
#define V4L2_PIX_FMT_MT21C        v4l2_fourcc('M', 'T', '2', '1') /* Mediatek compressed block mode  */
#define V4L2_PIX_FMT_MM21         v4l2_fourcc('M', 'M', '2', '1') /* Mediatek 8-bit block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_INZI         v4l2_fourcc('I', 'N', 'Z', 'I') /* Intel Planar Greyscale 10-bit and Depth 16-bit */
#define V4L2_PIX_FMT_CNF4         v4l2_fourcc('C', 'N', 'F', '4') /* Intel 4-bit packed depth confidence information */
#define V4L2_PIX_FMT_HI240        v4l2_fourcc('H', 'I', '2', '4') /* BTTV 8-bit dithered RGB */
#define V4L2_PIX_FMT_QC08C        v4l2_fourcc('Q', '0', '8', 'C') /* Qualcomm 8-bit compressed */
#define V4L2_PIX_FMT_QC10C        v4l2_fourcc('Q', '1', '0', 'C') /* Qualcomm 10-bit compressed */

/* 10bit raw packed, 32 bytes for every 25 pixels, last LSB 6 bits unused */
#define V4L2_PIX_FMT_IPU3_SBGGR10 v4l2_fourcc('i', 'p', '3', 'b') /* IPU3 packed 10-bit BGGR bayer */
#define V4L2_PIX_FMT_IPU3_SGBRG10 v4l2_fourcc('i', 'p', '3', 'g') /* IPU3 packed 10-bit GBRG bayer */
#define V4L2_PIX_FMT_IPU3_SGRBG10 v4l2_fourcc('i', 'p', '3', 'G') /* IPU3 packed 10-bit GRBG bayer */
#define V4L2_PIX_FMT_IPU3_SRGGB10 v4l2_fourcc('i', 'p', '3', 'r') /* IPU3 packed 10-bit RGGB bayer */

/* SDR formats - used only for Software Defined Radio devices */
#define V4L2_SDR_FMT_CU8     v4l2_fourcc('C', 'U', '0', '8') /* IQ u8 */
#define V4L2_SDR_FMT_CU16LE  v4l2_fourcc('C', 'U', '1', '6') /* IQ u16le */
#define V4L2_SDR_FMT_CS8     v4l2_fourcc('C', 'S', '0', '8') /* complex s8 */
#define V4L2_SDR_FMT_CS14LE  v4l2_fourcc('C', 'S', '1', '4') /* complex s14le */
#define V4L2_SDR_FMT_RU12LE  v4l2_fourcc('R', 'U', '1', '2') /* real u12le */
#define V4L2_SDR_FMT_PCU16BE v4l2_fourcc('P', 'C', '1', '6') /* planar complex u16be */
#define V4L2_SDR_FMT_PCU18BE v4l2_fourcc('P', 'C', '1', '8') /* planar complex u18be */
#define V4L2_SDR_FMT_PCU20BE v4l2_fourcc('P', 'C', '2', '0') /* planar complex u20be */

/* Touch formats - used for Touch devices */
#define V4L2_TCH_FMT_DELTA_TD16 v4l2_fourcc('T', 'D', '1', '6') /* 16-bit signed deltas */
#define V4L2_TCH_FMT_DELTA_TD08 v4l2_fourcc('T', 'D', '0', '8') /* 8-bit signed deltas */
#define V4L2_TCH_FMT_TU16       v4l2_fourcc('T', 'U', '1', '6') /* 16-bit unsigned touch data */
#define V4L2_TCH_FMT_TU08       v4l2_fourcc('T', 'U', '0', '8') /* 8-bit unsigned touch data */

/* Meta-data formats */
#define V4L2_META_FMT_VSP1_HGO          v4l2_fourcc('V', 'S', 'P', 'H') /* R-Car VSP1 1-D Histogram */
#define V4L2_META_FMT_VSP1_HGT          v4l2_fourcc('V', 'S', 'P', 'T') /* R-Car VSP1 2-D Histogram */
#define V4L2_META_FMT_UVC               v4l2_fourcc('U', 'V', 'C', 'H') /* UVC Payload Header metadata */
#define V4L2_META_FMT_D4XX              v4l2_fourcc('D', '4', 'X', 'X') /* D4XX Payload Header metadata */
#define V4L2_META_FMT_VIVID             v4l2_fourcc('V', 'I', 'V', 'D') /* Vivid Metadata */
#define V4L2_META_FMT_SENSOR_DATA       v4l2_fourcc('S', 'E', 'N', 'S') /* Sensor Ancillary metadata */
#define V4L2_META_FMT_BCM2835_ISP_STATS v4l2_fourcc('B', 'S', 'T', 'A') /* BCM2835 ISP image statistics output */

/* Vendor specific - used for RK_ISP1 camera sub-system */
#define V4L2_META_FMT_RK_ISP1_PARAMS  v4l2_fourcc('R', 'K', '1', 'P') /* Rockchip ISP1 3A Parameters */
#define V4L2_META_FMT_RK_ISP1_STAT_3A v4l2_fourcc('R', 'K', '1', 'S') /* Rockchip ISP1 3A Statistics */

/* priv field value to indicates that subsequent fields are valid. */
#define V4L2_PIX_FMT_PRIV_MAGIC 0xfeedcafe

/* Flags */
#define V4L2_PIX_FMT_FLAG_PREMUL_ALPHA 0x00000001
#define V4L2_PIX_FMT_FLAG_SET_CSC      0x00000002
/* --- metadata format --- */
struct v4l2_meta_format {
    __u32 dataformat;  /* FOURCC de métadonnées (p.ex. 'META') */
    __u32 buffersize;  /* taille max (bytes) */
};

/* --- overlay/window/framebuffer --- */
struct v4l2_clip {
    struct v4l2_rect c;
    struct v4l2_clip *next;
};
struct v4l2_window {
    struct v4l2_rect  w;
    __u32 field;
    __u32 chromakey;
    struct v4l2_clip *clips;
    __u32 clipcount;
    void *bitmap;
    __u8  global_alpha;
};
struct v4l2_framebuffer {
    __u32 capability;
    __u32 flags;
    void *base;
    struct {
        __u32 width;
        __u32 height;
        __u32 pixelformat;
        __u32 field;
        __u32 bytesperline;
        __u32 sizeimage;
        __u32 colorspace;
        __u32 priv;
    } fmt;
};

/* --- format union --- */
struct v4l2_format {
    __u32 type; /* enum v4l2_buf_type */
    union {
        struct v4l2_pix_format        pix;
        struct v4l2_pix_format_mplane pix_mp;
        struct v4l2_window            win;
        struct v4l2_meta_format       meta;
        __u8 raw_data[200];
    } fmt;
};

/* ===================== Énumération formats / tailles / cadences ===================== */
struct v4l2_fmtdesc {
    __u32 index;
    __u32 type;          /* enum v4l2_buf_type */
    __u32 flags;
    __u8  description[32];
    __u32 pixelformat;
    __u32 mbus_code;
    __u32 reserved[3];
};
#define V4L2_FMT_FLAG_COMPRESSED             0x0001
#define V4L2_FMT_FLAG_EMULATED               0x0002
#define V4L2_FMT_FLAG_CONTINUOUS_BYTESTREAM  0x0004
#define V4L2_FMT_FLAG_DYN_RESOLUTION         0x0008
#define V4L2_FMT_FLAG_ENC_CAP_FRAME_INTERVAL 0x0010

enum v4l2_frmsizetypes {
    V4L2_FRMSIZE_TYPE_DISCRETE   = 1,
    V4L2_FRMSIZE_TYPE_CONTINUOUS = 2,
    V4L2_FRMSIZE_TYPE_STEPWISE   = 3
};
struct v4l2_frmsize_discrete { __u32 width; __u32 height; };
struct v4l2_frmsize_stepwise {
    __u32 min_width,  max_width,  step_width;
    __u32 min_height, max_height, step_height;
};
struct v4l2_frmsizeenum {
    __u32 index;
    __u32 pixel_format;
    __u32 type;
    union {
        struct v4l2_frmsize_discrete discrete;
        struct v4l2_frmsize_stepwise stepwise;
    };
    __u32 reserved[2];
};

enum v4l2_frmivaltypes {
    V4L2_FRMIVAL_TYPE_DISCRETE   = 1,
    V4L2_FRMIVAL_TYPE_CONTINUOUS = 2,
    V4L2_FRMIVAL_TYPE_STEPWISE   = 3
};
struct v4l2_frmival_stepwise {
    struct v4l2_fract min;
    struct v4l2_fract max;
    struct v4l2_fract step;
};
struct v4l2_frmivalenum {
    __u32 index;
    __u32 pixel_format;
    __u32 width;
    __u32 height;
    __u32 type;
    union {
        struct v4l2_fract discrete;
        struct v4l2_frmival_stepwise stepwise;
    };
    __u32 reserved[2];
};

/* ===================== Crop / Selection ===================== */
struct v4l2_cropcap {
    __u32 type;  /* enum v4l2_buf_type */
    struct v4l2_area   pixelaspect;
    struct v4l2_rect   bounds;
    struct v4l2_rect   defrect;
    struct v4l2_fract  def_frame_interval;
};

struct v4l2_crop {
    __u32 type;
    struct v4l2_rect c;
};

/* v4l2_selection */
enum v4l2_selection_target {
    V4L2_SEL_TGT_CROP_ACTIVE = 0,
    V4L2_SEL_TGT_CROP_DEFAULT = 1,
    V4L2_SEL_TGT_COMPOSE_ACTIVE = 2,
    V4L2_SEL_TGT_COMPOSE_DEFAULT = 3,
    V4L2_SEL_TGT_CROP_BOUNDS = 4,
    V4L2_SEL_TGT_COMPOSE_BOUNDS = 5
};
#define V4L2_SEL_FLAG_GE        0x0001
#define V4L2_SEL_FLAG_LE        0x0002
#define V4L2_SEL_FLAG_KEEP_CONFIG 0x0004

struct v4l2_selection {
    __u32 type;
    __u32 target;   /* enum v4l2_selection_target */
    __u32 flags;    /* V4L2_SEL_FLAG_* */
    struct v4l2_rect r;
    __u32 reserved[9];
};

/* ===================== Buffering / streaming ===================== */
#define V4L2_BUF_FLAG_MAPPED         0x00000001
#define V4L2_BUF_FLAG_QUEUED         0x00000002
#define V4L2_BUF_FLAG_DONE           0x00000004
#define V4L2_BUF_FLAG_KEYFRAME       0x00000008
#define V4L2_BUF_FLAG_PFRAME         0x00000010
#define V4L2_BUF_FLAG_BFRAME         0x00000020
#define V4L2_BUF_FLAG_ERROR          0x00000040
#define V4L2_BUF_FLAG_TIMESTAMP_MASK 0x0000e000

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
/*  Type  */
#define V4L2_TC_TYPE_24FPS 1
#define V4L2_TC_TYPE_25FPS 2
#define V4L2_TC_TYPE_30FPS 3
#define V4L2_TC_TYPE_50FPS 4
#define V4L2_TC_TYPE_60FPS 5

/*  Flags  */
#define V4L2_TC_FLAG_DROPFRAME       0x0001 /* "drop-frame" mode */
#define V4L2_TC_FLAG_COLORFRAME      0x0002
#define V4L2_TC_USERBITS_field       0x000C
#define V4L2_TC_USERBITS_USERDEFINED 0x0000
#define V4L2_TC_USERBITS_8BITCHARS   0x0008
/* The above is based on SMPTE timecodes */
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
        __u32 offset;              /* single-planar */
        unsigned long userptr;
        struct v4l2_plane *planes; /* multi-planar */
        __s32 fd;
    } m;
    __u32 length;   /* nb de planes pour mplane */
    __u32 reserved2;
    __u32 reserved;
};

struct v4l2_captureparm {
    __u32 capability;       /* Supported modes */
    __u32 capturemode;      /* Current mode */
    struct v4l2_fract timeperframe;  /* Time per frame in seconds */
    __u32 extendedmode;     /* Driver-specific extensions */
    __u32 readbuffers;      /* # of buffers for read */
    __u32 reserved[4];
};

struct v4l2_outputparm {
    __u32 capability;       /* Supported modes */
    __u32 outputmode;       /* Current mode */
    struct v4l2_fract timeperframe;  /* Time per frame in seconds */
    __u32 extendedmode;     /* Driver-specific extensions */
    __u32 writebuffers;     /* # of buffers for write */
    __u32 reserved[4];
};

struct v4l2_streamparm {
    __u32 type;             /* enum v4l2_buf_type */
    union {
        struct v4l2_captureparm capture;
        struct v4l2_outputparm  output;
        __u8 raw_data[200];
    } parm;  /* ✅ CORRECTION : ajouter ".parm" explicitement */
};

/* ===================== Contrôles ===================== */
#define V4L2_CTRL_TYPE_INTEGER      1
#define V4L2_CTRL_TYPE_BOOLEAN      2
#define V4L2_CTRL_TYPE_MENU         3
#define V4L2_CTRL_TYPE_BUTTON       4
#define V4L2_CTRL_TYPE_INTEGER64    5
#define V4L2_CTRL_TYPE_CTRL_CLASS   6
#define V4L2_CTRL_TYPE_STRING       7
#define V4L2_CTRL_TYPE_BITMASK      8
#define V4L2_CTRL_TYPE_INTEGER_MENU 9

#define V4L2_CID_BASE               0x00980900
#define V4L2_CID_BRIGHTNESS         (V4L2_CID_BASE + 0)
#define V4L2_CID_CONTRAST           (V4L2_CID_BASE + 1)
#define V4L2_CID_SATURATION         (V4L2_CID_BASE + 2)
#define V4L2_CID_HUE                (V4L2_CID_BASE + 3)
#define V4L2_CID_RED_BALANCE        (V4L2_CID_BASE + 14)
#define V4L2_CID_BLUE_BALANCE       (V4L2_CID_BASE + 15)
#define V4L2_CID_EXPOSURE           (V4L2_CID_BASE + 17)
#define V4L2_CID_GAIN               (V4L2_CID_BASE + 19)
#define V4L2_CID_AUTO_EXPOSURE_BIAS (V4L2_CID_BASE + 26)

struct v4l2_control {
    __u32 id;
    __s32 value;
};

/* Query (legacy) & menu */
struct v4l2_queryctrl {
    __u32 id;
    __u32 type;
    __u8  name[32];
    __s32 minimum;
    __s32 maximum;
    __s32 step;
    __s32 default_value;
    __u32 flags;
    __u32 reserved[2];
};
struct v4l2_querymenu {
    __u32 id;
    __u32 index;
    __u8  name[32];
    __s64 value;
    __u32 reserved;
};

/* Ext controls (modern) */
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

/* ===================== JPEG / MPEG / H.264 ===================== */
#define V4L2_CID_MPEG_BASE                     0x00990900

/* Extraits génériques */
#define V4L2_CID_MPEG_VIDEO_BITRATE            (V4L2_CID_MPEG_BASE + 207)
#define V4L2_CID_MPEG_VIDEO_GOP_SIZE           (V4L2_CID_MPEG_BASE + 210)
#define V4L2_CID_JPEG_COMPRESSION_QUALITY      (V4L2_CID_MPEG_BASE + 500)

/* H.264 (sous-ensemble utile) */
#define V4L2_CID_MPEG_VIDEO_H264_PROFILE       (V4L2_CID_MPEG_BASE + 264)
#define V4L2_CID_MPEG_VIDEO_H264_LEVEL         (V4L2_CID_MPEG_BASE + 265)
#define V4L2_CID_MPEG_VIDEO_H264_I_PERIOD      (V4L2_CID_MPEG_BASE + 266)
#define V4L2_CID_MPEG_VIDEO_H264_MIN_QP        (V4L2_CID_MPEG_BASE + 267)
#define V4L2_CID_MPEG_VIDEO_H264_MAX_QP        (V4L2_CID_MPEG_BASE + 268)
#define V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP    (V4L2_CID_MPEG_BASE + 269)
#define V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP    (V4L2_CID_MPEG_BASE + 270)
#define V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP    (V4L2_CID_MPEG_BASE + 271)
#define V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE  (V4L2_CID_MPEG_BASE + 274)

/* Structure JPEG (legacy mais pratique) */
struct v4l2_jpegcompression {
    int quality;
    int APPn;                /* 0 à 15 */
    int APP_len;
    char *APP_data;
    int COM_len;
    char *COM_data;
    __u32 jpeg_markers;      /* masques pour marquers à inclure */
    __u32 reserved[3];
};

/* ===================== Pixels formats (sélection large) ===================== */
#define V4L2_PIX_FMT_RGB332     v4l2_fourcc('R','G','B','1')
#define V4L2_PIX_FMT_RGB444     v4l2_fourcc('R','4','4','4')
#define V4L2_PIX_FMT_RGB555     v4l2_fourcc('R','G','B','O')
#define V4L2_PIX_FMT_RGB565     v4l2_fourcc('R','G','B','P')
#define V4L2_PIX_FMT_RGB24      v4l2_fourcc('R','G','B','3')
#define V4L2_PIX_FMT_BGR24      v4l2_fourcc('B','G','R','3')
#define V4L2_PIX_FMT_ARGB32     v4l2_fourcc('A','R','2','4')
#define V4L2_PIX_FMT_BGRA32     v4l2_fourcc('B','G','R','A')
#define V4L2_PIX_FMT_GREY       v4l2_fourcc('G','R','E','Y')
#define V4L2_PIX_FMT_YUYV       v4l2_fourcc('Y','U','Y','V')
#define V4L2_PIX_FMT_UYVY       v4l2_fourcc('U','Y','V','Y')
#define V4L2_PIX_FMT_YUV422P    v4l2_fourcc('4','2','2','P')
#define V4L2_PIX_FMT_YUV420     v4l2_fourcc('Y','U','1','2')
#define V4L2_PIX_FMT_YVU420     v4l2_fourcc('Y','V','1','2')
#define V4L2_PIX_FMT_NV12       v4l2_fourcc('N','V','1','2')
#define V4L2_PIX_FMT_NV21       v4l2_fourcc('N','V','2','1')
#define V4L2_PIX_FMT_MJPEG      v4l2_fourcc('M','J','P','G')
#define V4L2_PIX_FMT_JPEG       v4l2_fourcc('J','P','E','G')
#define V4L2_PIX_FMT_H264       v4l2_fourcc('H','2','6','4')
#define V4L2_PIX_FMT_H264_NO_SC v4l2_fourcc('A','V','C','1')
#define V4L2_PIX_FMT_SBGGR8     v4l2_fourcc('B','A','8','1')
#define V4L2_PIX_FMT_SBGGR10    v4l2_fourcc('B','G','1','0')

/* ===================== Events ===================== */
enum v4l2_event_type {
    V4L2_EVENT_ALL            = 0,
    V4L2_EVENT_VSYNC          = 1,
    V4L2_EVENT_EOS            = 2,
    V4L2_EVENT_CTRL           = 3,
    V4L2_EVENT_FRAME_SYNC     = 4,
    V4L2_EVENT_SOURCE_CHANGE  = 5,
    V4L2_EVENT_MOTION_DET     = 6
};

/* Sous-structs utiles (ctrl) */
struct v4l2_event_ctrl {
    __u32 changes;
    __u32 type;
    __u32 flags;
    __s32 value;
    __s64 value64;
    __u32 id;
    __u32 reserved[3];
};

/* Évènement générique portable (timeval au lieu de timespec) */
struct v4l2_event {
    __u32 type;
    union {
        struct v4l2_event_ctrl ctrl;
        __u8 data[64];
    } u;
    __u32 pending;
    __u32 sequence;
    struct timeval timestamp;
    __u32 id;
    __u32 reserved[8];
};

#define V4L2_EVENT_SUB_FL_SEND_INITIAL  0x0001
#define V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK 0x0002

struct v4l2_event_subscription {
    __u32 type;
    __u32 id;
    __u32 flags;
    __u32 reserved[5];
};

/* ===================== IOCTL ===================== */
/* Capabilities / format */
#define VIDIOC_QUERYCAP              _IOR ('V',  0, struct v4l2_capability)
#define VIDIOC_ENUM_FMT              _IOWR('V',  2, struct v4l2_fmtdesc)
#define VIDIOC_G_FMT                 _IOWR('V',  4, struct v4l2_format)
#define VIDIOC_S_FMT                 _IOWR('V',  5, struct v4l2_format)
#define VIDIOC_TRY_FMT               _IOWR('V', 64, struct v4l2_format)

/* Framebuffer & overlay */
#define VIDIOC_G_FBUF                _IOR ('V', 10, struct v4l2_framebuffer)
#define VIDIOC_S_FBUF                _IOW ('V', 11, struct v4l2_framebuffer)
#define VIDIOC_OVERLAY               _IOW ('V', 14, int)

/* Buffers & streaming */
#define VIDIOC_REQBUFS               _IOWR('V',  8, struct v4l2_requestbuffers)
#define VIDIOC_QUERYBUF              _IOWR('V',  9, struct v4l2_buffer)
#define VIDIOC_QBUF                  _IOWR('V', 15, struct v4l2_buffer)
#define VIDIOC_DQBUF                 _IOWR('V', 17, struct v4l2_buffer)
#define VIDIOC_STREAMON              _IOW ('V', 18, int)
#define VIDIOC_STREAMOFF             _IOW ('V', 19, int)

/* Contrôles */
#define VIDIOC_QUERYCTRL             _IOWR('V', 36, struct v4l2_queryctrl)
#define VIDIOC_QUERYMENU             _IOWR('V', 37, struct v4l2_querymenu)
#define VIDIOC_G_CTRL                _IOWR('V', 27, struct v4l2_control)
#define VIDIOC_S_CTRL                _IOWR('V', 28, struct v4l2_control)
#define VIDIOC_G_EXT_CTRLS           _IOWR('V', 71, struct v4l2_ext_controls)
#define VIDIOC_S_EXT_CTRLS           _IOWR('V', 72, struct v4l2_ext_controls)
#define VIDIOC_TRY_EXT_CTRLS         _IOWR('V', 73, struct v4l2_ext_controls)
#define VIDIOC_QUERY_EXT_CTRL        _IOWR('V',103, struct v4l2_query_ext_ctrl)

/* Cropping / selection */
#define VIDIOC_CROPCAP               _IOWR('V', 58, struct v4l2_cropcap)
#define VIDIOC_G_CROP                _IOWR('V', 59, struct v4l2_crop)
#define VIDIOC_S_CROP                _IOWR('V', 60, struct v4l2_crop)
#define VIDIOC_G_SELECTION           _IOWR('V',121, struct v4l2_selection)
#define VIDIOC_S_SELECTION           _IOWR('V',122, struct v4l2_selection)

/* Tailles & cadences */
#define VIDIOC_ENUM_FRAMESIZES       _IOWR('V', 74, struct v4l2_frmsizeenum)
#define VIDIOC_ENUM_FRAMEINTERVALS   _IOWR('V', 75, struct v4l2_frmivalenum)
#define VIDIOC_G_PARM                _IOWR('V', 21, struct v4l2_streamparm)
#define VIDIOC_S_PARM                _IOWR('V', 22, struct v4l2_streamparm)

/* Events */
#define VIDIOC_DQEVENT               _IOR ('V', 89, struct v4l2_event)
#define VIDIOC_SUBSCRIBE_EVENT       _IOW ('V', 90, struct v4l2_event_subscription)
#define VIDIOC_UNSUBSCRIBE_EVENT     _IOW ('V', 91, struct v4l2_event_subscription)

/* ===================== (fin IOCTL) ===================== */

#ifdef __cplusplus
}
#endif

#endif /* __LINUX_VIDEODEV2_H */



