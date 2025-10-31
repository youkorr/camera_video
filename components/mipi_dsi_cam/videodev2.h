/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause) */
/*
 *  Video for Linux Two header file - Portable (ESP32/embedded) full user API
 *  - Autonome, sans include linux/*
 *  - Structures/fonctions publiques V4L2 usuelles (capture, overlay, events,
 *    crop/selection, framebuffer, metadata, JPEG/H.264 params, multiplanar, etc.)
 */

#ifndef __LINUX_VIDEODEV2_H
#define __LINUX_VIDEODEV2_H


#include "../lvgl_camera_display/ioctl.h"
#include "types.h"
#include "v4l2-common.h"
//#include "v4l2-controls.h"

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
    V4L2_FIELD_ANY = 0,           /* driver can choose from none,
                     top, bottom, interlaced
                     depending on whatever it thinks
                     is approximate ... */
    V4L2_FIELD_NONE       = 1,    /* this device has no fields ... */
    V4L2_FIELD_TOP        = 2,    /* top field only */
    V4L2_FIELD_BOTTOM     = 3,    /* bottom field only */
    V4L2_FIELD_INTERLACED = 4,    /* both fields interlaced */
    V4L2_FIELD_SEQ_TB     = 5,    /* both fields sequential into one
                     buffer, top-bottom order */
    V4L2_FIELD_SEQ_BT    = 6,     /* same as above + bottom-top order */
    V4L2_FIELD_ALTERNATE = 7,     /* both fields alternating into
                     separate buffers */
    V4L2_FIELD_INTERLACED_TB = 8, /* both fields interlaced, top field
                     first and the top field is
                     transmitted first */
    V4L2_FIELD_INTERLACED_BT = 9, /* both fields interlaced, top field
                     first and the bottom field is
                     transmitted first */
};

enum v4l2_buf_type {
    V4L2_BUF_TYPE_VIDEO_CAPTURE        = 1,
    V4L2_BUF_TYPE_VIDEO_OUTPUT         = 2,
    V4L2_BUF_TYPE_VIDEO_OVERLAY        = 3,
    V4L2_BUF_TYPE_VBI_CAPTURE          = 4,
    V4L2_BUF_TYPE_VBI_OUTPUT           = 5,
    V4L2_BUF_TYPE_SLICED_VBI_CAPTURE   = 6,
    V4L2_BUF_TYPE_SLICED_VBI_OUTPUT    = 7,
    V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY = 8,
    V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 9,
    V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE  = 10,
    V4L2_BUF_TYPE_SDR_CAPTURE          = 11,
    V4L2_BUF_TYPE_SDR_OUTPUT           = 12,
    V4L2_BUF_TYPE_META_CAPTURE         = 13,
    V4L2_BUF_TYPE_META_OUTPUT          = 14,
    /* Deprecated, do not use */
    V4L2_BUF_TYPE_PRIVATE = 0x80,
};

#define V4L2_TYPE_IS_MULTIPLANAR(type) \
    ((type) == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || (type) == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)

#define V4L2_TYPE_IS_OUTPUT(type)                                                             \
    ((type) == V4L2_BUF_TYPE_VIDEO_OUTPUT || (type) == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ||   \
     (type) == V4L2_BUF_TYPE_VIDEO_OVERLAY || (type) == V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY || \
     (type) == V4L2_BUF_TYPE_VBI_OUTPUT || (type) == V4L2_BUF_TYPE_SLICED_VBI_OUTPUT ||       \
     (type) == V4L2_BUF_TYPE_SDR_OUTPUT || (type) == V4L2_BUF_TYPE_META_OUTPUT)

#define V4L2_TYPE_IS_CAPTURE(type) (!V4L2_TYPE_IS_OUTPUT(type))

enum v4l2_memory {
    V4L2_MEMORY_MMAP    = 1,
    V4L2_MEMORY_USERPTR = 2,
    V4L2_MEMORY_OVERLAY = 3,
    V4L2_MEMORY_DMABUF  = 4
};

/* ===================== Colorimétrie ===================== */
enum v4l2_colorspace {
    /*
     * Default colorspace, i.e. let the driver figure it out.
     * Can only be used with video capture.
     */
    V4L2_COLORSPACE_DEFAULT = 0,

    /* SMPTE 170M: used for broadcast NTSC/PAL SDTV */
    V4L2_COLORSPACE_SMPTE170M = 1,

    /* Obsolete pre-1998 SMPTE 240M HDTV standard, superseded by Rec 709 */
    V4L2_COLORSPACE_SMPTE240M = 2,

    /* Rec.709: used for HDTV */
    V4L2_COLORSPACE_REC709 = 3,

    /*
     * Deprecated, do not use. No driver will ever return this. This was
     * based on a misunderstanding of the bt878 datasheet.
     */
    V4L2_COLORSPACE_BT878 = 4,

    /*
     * NTSC 1953 colorspace. This only makes sense when dealing with
     * really, really old NTSC recordings. Superseded by SMPTE 170M.
     */
    V4L2_COLORSPACE_470_SYSTEM_M = 5,

    /*
     * EBU Tech 3213 PAL/SECAM colorspace.
     */
    V4L2_COLORSPACE_470_SYSTEM_BG = 6,

    /*
     * Effectively shorthand for V4L2_COLORSPACE_SRGB, V4L2_YCBCR_ENC_601
     * and V4L2_QUANTIZATION_FULL_RANGE. To be used for (Motion-)JPEG.
     */
    V4L2_COLORSPACE_JPEG = 7,

    /* For RGB colorspaces such as produces by most webcams. */
    V4L2_COLORSPACE_SRGB = 8,

    /* opRGB colorspace */
    V4L2_COLORSPACE_OPRGB = 9,

    /* BT.2020 colorspace, used for UHDTV. */
    V4L2_COLORSPACE_BT2020 = 10,

    /* Raw colorspace: for RAW unprocessed images */
    V4L2_COLORSPACE_RAW = 11,

    /* DCI-P3 colorspace, used by cinema projectors */
    V4L2_COLORSPACE_DCI_P3 = 12,
};

/*
 * Determine how COLORSPACE_DEFAULT should map to a proper colorspace.
 * This depends on whether this is a SDTV image (use SMPTE 170M), an
 * HDTV image (use Rec. 709), or something else (use sRGB).
 */
#define V4L2_MAP_COLORSPACE_DEFAULT(is_sdtv, is_hdtv) \
    ((is_sdtv) ? V4L2_COLORSPACE_SMPTE170M : ((is_hdtv) ? V4L2_COLORSPACE_REC709 : V4L2_COLORSPACE_SRGB))
};
enum v4l2_xfer_func {
    /*
     * Mapping of V4L2_XFER_FUNC_DEFAULT to actual transfer functions
     * for the various colorspaces:
     *
     * V4L2_COLORSPACE_SMPTE170M, V4L2_COLORSPACE_470_SYSTEM_M,
     * V4L2_COLORSPACE_470_SYSTEM_BG, V4L2_COLORSPACE_REC709 and
     * V4L2_COLORSPACE_BT2020: V4L2_XFER_FUNC_709
     *
     * V4L2_COLORSPACE_SRGB, V4L2_COLORSPACE_JPEG: V4L2_XFER_FUNC_SRGB
     *
     * V4L2_COLORSPACE_OPRGB: V4L2_XFER_FUNC_OPRGB
     *
     * V4L2_COLORSPACE_SMPTE240M: V4L2_XFER_FUNC_SMPTE240M
     *
     * V4L2_COLORSPACE_RAW: V4L2_XFER_FUNC_NONE
     *
     * V4L2_COLORSPACE_DCI_P3: V4L2_XFER_FUNC_DCI_P3
     */
    V4L2_XFER_FUNC_DEFAULT   = 0,
    V4L2_XFER_FUNC_709       = 1,
    V4L2_XFER_FUNC_SRGB      = 2,
    V4L2_XFER_FUNC_OPRGB     = 3,
    V4L2_XFER_FUNC_SMPTE240M = 4,
    V4L2_XFER_FUNC_NONE      = 5,
    V4L2_XFER_FUNC_DCI_P3    = 6,
    V4L2_XFER_FUNC_SMPTE2084 = 7,
};

/*
 * Determine how XFER_FUNC_DEFAULT should map to a proper transfer function.
 * This depends on the colorspace.
 */
#define V4L2_MAP_XFER_FUNC_DEFAULT(colsp)                                                           \
    ((colsp) == V4L2_COLORSPACE_OPRGB                                                               \
         ? V4L2_XFER_FUNC_OPRGB                                                                     \
         : ((colsp) == V4L2_COLORSPACE_SMPTE240M                                                    \
                ? V4L2_XFER_FUNC_SMPTE240M                                                          \
                : ((colsp) == V4L2_COLORSPACE_DCI_P3                                                \
                       ? V4L2_XFER_FUNC_DCI_P3                                                      \
                       : ((colsp) == V4L2_COLORSPACE_RAW                                            \
                              ? V4L2_XFER_FUNC_NONE                                                 \
                              : ((colsp) == V4L2_COLORSPACE_SRGB || (colsp) == V4L2_COLORSPACE_JPEG \
                                     ? V4L2_XFER_FUNC_SRGB                                          \
                                     : V4L2_XFER_FUNC_709)))))

};
enum v4l2_ycbcr_encoding {
    /*
     * Mapping of V4L2_YCBCR_ENC_DEFAULT to actual encodings for the
     * various colorspaces:
     *
     * V4L2_COLORSPACE_SMPTE170M, V4L2_COLORSPACE_470_SYSTEM_M,
     * V4L2_COLORSPACE_470_SYSTEM_BG, V4L2_COLORSPACE_SRGB,
     * V4L2_COLORSPACE_OPRGB and V4L2_COLORSPACE_JPEG: V4L2_YCBCR_ENC_601
     *
     * V4L2_COLORSPACE_REC709 and V4L2_COLORSPACE_DCI_P3: V4L2_YCBCR_ENC_709
     *
     * V4L2_COLORSPACE_BT2020: V4L2_YCBCR_ENC_BT2020
     *
     * V4L2_COLORSPACE_SMPTE240M: V4L2_YCBCR_ENC_SMPTE240M
     */
    V4L2_YCBCR_ENC_DEFAULT = 0,

    /* ITU-R 601 -- SDTV */
    V4L2_YCBCR_ENC_601 = 1,

    /* Rec. 709 -- HDTV */
    V4L2_YCBCR_ENC_709 = 2,

    /* ITU-R 601/EN 61966-2-4 Extended Gamut -- SDTV */
    V4L2_YCBCR_ENC_XV601 = 3,

    /* Rec. 709/EN 61966-2-4 Extended Gamut -- HDTV */
    V4L2_YCBCR_ENC_XV709 = 4,

    /*
     * sYCC (Y'CbCr encoding of sRGB), identical to ENC_601. It was added
     * originally due to a misunderstanding of the sYCC standard. It should
     * not be used, instead use V4L2_YCBCR_ENC_601.
     */
    V4L2_YCBCR_ENC_SYCC = 5,

    /* BT.2020 Non-constant Luminance Y'CbCr */
    V4L2_YCBCR_ENC_BT2020 = 6,

    /* BT.2020 Constant Luminance Y'CbcCrc */
    V4L2_YCBCR_ENC_BT2020_CONST_LUM = 7,

    /* SMPTE 240M -- Obsolete HDTV */
    V4L2_YCBCR_ENC_SMPTE240M = 8,
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

#endif

#endif /* __LINUX_VIDEODEV2_H */














