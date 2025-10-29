

components/mipi_dsi_cam/videodev2.h
+1190
-281

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
#include <time.h>

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

typedef __u64 v4l2_std_id;
typedef __u32 __le32;

#ifndef __be32
typedef __u32 __be32;
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
@@ -193,524 +201,1425 @@ struct v4l2_capability {
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

struct v4l2_bt_timings {
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
    __u32 interlaced;
    __u32 polarities;
    __u64 pixelclock;
    __u32 hfrontporch;
    __u32 hsync;
    __u32 hbackporch;
    __u32 vfrontporch;
    __u32 vsync;
    __u32 vbackporch;
    __u32 il_vfrontporch;
    __u32 il_vsync;
    __u32 il_vbackporch;
    __u32 standards;
    __u32 flags;
    struct v4l2_fract picture_aspect;
    __u8 cea861_vic;
    __u8 hdmi_vic;
    __u8 reserved[46];
} __attribute__((packed));

/* --- metadata format --- */
struct v4l2_meta_format {
    __u32 dataformat;  /* FOURCC de métadonnées (p.ex. 'META') */
    __u32 buffersize;  /* taille max (bytes) */
/* Interlaced or progressive format */
#define V4L2_DV_PROGRESSIVE 0
#define V4L2_DV_INTERLACED  1

/* Polarities. If bit is not set, it is assumed to be negative polarity */
#define V4L2_DV_VSYNC_POS_POL 0x00000001
#define V4L2_DV_HSYNC_POS_POL 0x00000002

/* Timings standards */
#define V4L2_DV_BT_STD_CEA861 (1 << 0) /* CEA-861 Digital TV Profile */
#define V4L2_DV_BT_STD_DMT    (1 << 1) /* VESA Discrete Monitor Timings */
#define V4L2_DV_BT_STD_CVT    (1 << 2) /* VESA Coordinated Video Timings */
#define V4L2_DV_BT_STD_GTF    (1 << 3) /* VESA Generalized Timings Formula */
#define V4L2_DV_BT_STD_SDI    (1 << 4) /* SDI Timings */

/* Flags */

/*
 * CVT/GTF specific: timing uses reduced blanking (CVT) or the 'Secondary
 * GTF' curve (GTF). In both cases the horizontal and/or vertical blanking
 * intervals are reduced, allowing a higher resolution over the same
 * bandwidth. This is a read-only flag.
 */
#define V4L2_DV_FL_REDUCED_BLANKING (1 << 0)
/*
 * CEA-861 specific: set for CEA-861 formats with a framerate of a multiple
 * of six. These formats can be optionally played at 1 / 1.001 speed.
 * This is a read-only flag.
 */
#define V4L2_DV_FL_CAN_REDUCE_FPS (1 << 1)
/*
 * CEA-861 specific: only valid for video transmitters, the flag is cleared
 * by receivers.
 * If the framerate of the format is a multiple of six, then the pixelclock
 * used to set up the transmitter is divided by 1.001 to make it compatible
 * with 60 Hz based standards such as NTSC and PAL-M that use a framerate of
 * 29.97 Hz. Otherwise this flag is cleared. If the transmitter can't generate
 * such frequencies, then the flag will also be cleared.
 */
#define V4L2_DV_FL_REDUCED_FPS (1 << 2)
/*
 * Specific to interlaced formats: if set, then field 1 is really one half-line
 * longer and field 2 is really one half-line shorter, so each field has
 * exactly the same number of half-lines. Whether half-lines can be detected
 * or used depends on the hardware.
 */
#define V4L2_DV_FL_HALF_LINE (1 << 3)
/*
 * If set, then this is a Consumer Electronics (CE) video format. Such formats
 * differ from other formats (commonly called IT formats) in that if RGB
 * encoding is used then by default the RGB values use limited range (i.e.
 * use the range 16-235) as opposed to 0-255. All formats defined in CEA-861
 * except for the 640x480 format are CE formats.
 */
#define V4L2_DV_FL_IS_CE_VIDEO (1 << 4)
/* Some formats like SMPTE-125M have an interlaced signal with a odd
 * total height. For these formats, if this flag is set, the first
 * field has the extra line. If not, it is the second field.
 */
#define V4L2_DV_FL_FIRST_FIELD_EXTRA_LINE (1 << 5)
/*
 * If set, then the picture_aspect field is valid. Otherwise assume that the
 * pixels are square, so the picture aspect ratio is the same as the width to
 * height ratio.
 */
#define V4L2_DV_FL_HAS_PICTURE_ASPECT (1 << 6)
/*
 * If set, then the cea861_vic field is valid and contains the Video
 * Identification Code as per the CEA-861 standard.
 */
#define V4L2_DV_FL_HAS_CEA861_VIC (1 << 7)
/*
 * If set, then the hdmi_vic field is valid and contains the Video
 * Identification Code as per the HDMI standard (HDMI Vendor Specific
 * InfoFrame).
 */
#define V4L2_DV_FL_HAS_HDMI_VIC (1 << 8)
/*
 * CEA-861 specific: only valid for video receivers.
 * If set, then HW can detect the difference between regular FPS and
 * 1000/1001 FPS. Note: This flag is only valid for HDMI VIC codes with
 * the V4L2_DV_FL_CAN_REDUCE_FPS flag set.
 */
#define V4L2_DV_FL_CAN_DETECT_REDUCED_FPS (1 << 9)

/* A few useful defines to calculate the total blanking and frame sizes */
#define V4L2_DV_BT_BLANKING_WIDTH(bt) ((bt)->hfrontporch + (bt)->hsync + (bt)->hbackporch)
#define V4L2_DV_BT_FRAME_WIDTH(bt)    ((bt)->width + V4L2_DV_BT_BLANKING_WIDTH(bt))
#define V4L2_DV_BT_BLANKING_HEIGHT(bt) \
    ((bt)->vfrontporch + (bt)->vsync + (bt)->vbackporch + (bt)->il_vfrontporch + (bt)->il_vsync + (bt)->il_vbackporch)
#define V4L2_DV_BT_FRAME_HEIGHT(bt) ((bt)->height + V4L2_DV_BT_BLANKING_HEIGHT(bt))

/** struct v4l2_dv_timings - DV timings
 * @type:the type of the timings
 * @bt:BT656/1120 timings
 */
struct v4l2_dv_timings {
    __u32 type;
    union {
        struct v4l2_bt_timings bt;
        __u32 reserved[32];
    };
} __attribute__((packed));

/* Values for the type field */
#define V4L2_DV_BT_656_1120 0 /* BT.656/1120 timing type */

/** struct v4l2_enum_dv_timings - DV timings enumeration
 * @index:enumeration index
 * @pad:the pad number for which to enumerate timings (used with
 *         v4l-subdev nodes only)
 * @reserved:must be zeroed
 * @timings:the timings for the given index
 */
struct v4l2_enum_dv_timings {
    __u32 index;
    __u32 pad;
    __u32 reserved[2];
    struct v4l2_dv_timings timings;
};

/* --- overlay/window/framebuffer --- */
struct v4l2_clip {
    struct v4l2_rect c;
    struct v4l2_clip *next;
/** struct v4l2_bt_timings_cap - BT.656/BT.1120 timing capabilities
 * @min_width:width in pixels
 * @max_width:width in pixels
 * @min_height:height in lines
 * @max_height:height in lines
 * @min_pixelclock:Pixel clock in HZ. Ex. 74.25MHz->74250000
 * @max_pixelclock:Pixel clock in HZ. Ex. 74.25MHz->74250000
 * @standards:Supported standards
 * @capabilities:Supported capabilities
 * @reserved:Must be zeroed
 */
struct v4l2_bt_timings_cap {
    __u32 min_width;
    __u32 max_width;
    __u32 min_height;
    __u32 max_height;
    __u64 min_pixelclock;
    __u64 max_pixelclock;
    __u32 standards;
    __u32 capabilities;
    __u32 reserved[16];
} __attribute__((packed));

/* Supports interlaced formats */
#define V4L2_DV_BT_CAP_INTERLACED (1 << 0)
/* Supports progressive formats */
#define V4L2_DV_BT_CAP_PROGRESSIVE (1 << 1)
/* Supports CVT/GTF reduced blanking */
#define V4L2_DV_BT_CAP_REDUCED_BLANKING (1 << 2)
/* Supports custom formats */
#define V4L2_DV_BT_CAP_CUSTOM (1 << 3)

/** struct v4l2_dv_timings_cap - DV timings capabilities
 * @type:the type of the timings (same as in struct v4l2_dv_timings)
 * @pad:the pad number for which to query capabilities (used with
 *         v4l-subdev nodes only)
 * @bt:the BT656/1120 timings capabilities
 */
struct v4l2_dv_timings_cap {
    __u32 type;
    __u32 pad;
    __u32 reserved[2];
    union {
        struct v4l2_bt_timings_cap bt;
        __u32 raw_data[32];
    };
};
struct v4l2_window {
    struct v4l2_rect  w;
    __u32 field;
    __u32 chromakey;
    struct v4l2_clip *clips;
    __u32 clipcount;
    void *bitmap;
    __u8  global_alpha;

/*
 *	V I D E O   I N P U T S
 */
struct v4l2_input {
    __u32 index;    /*  Which input */
    __u8 name[32];  /*  Label */
    __u32 type;     /*  Type of input */
    __u32 audioset; /*  Associated audios (bitfield) */
    __u32 tuner;    /*  enum v4l2_tuner_type */
    v4l2_std_id std;
    __u32 status;
    __u32 capabilities;
    __u32 reserved[3];
};
struct v4l2_framebuffer {

/*  Values for the 'type' field */
#define V4L2_INPUT_TYPE_TUNER  1
#define V4L2_INPUT_TYPE_CAMERA 2
#define V4L2_INPUT_TYPE_TOUCH  3

/* field 'status' - general */
#define V4L2_IN_ST_NO_POWER  0x00000001 /* Attached device is off */
#define V4L2_IN_ST_NO_SIGNAL 0x00000002
#define V4L2_IN_ST_NO_COLOR  0x00000004

/* field 'status' - sensor orientation */
/* If sensor is mounted upside down set both bits */
#define V4L2_IN_ST_HFLIP 0x00000010 /* Frames are flipped horizontally */
#define V4L2_IN_ST_VFLIP 0x00000020 /* Frames are flipped vertically */

/* field 'status' - analog */
#define V4L2_IN_ST_NO_H_LOCK   0x00000100 /* No horizontal sync lock */
#define V4L2_IN_ST_COLOR_KILL  0x00000200 /* Color killer is active */
#define V4L2_IN_ST_NO_V_LOCK   0x00000400 /* No vertical sync lock */
#define V4L2_IN_ST_NO_STD_LOCK 0x00000800 /* No standard format lock */

/* field 'status' - digital */
#define V4L2_IN_ST_NO_SYNC    0x00010000 /* No synchronization lock */
#define V4L2_IN_ST_NO_EQU     0x00020000 /* No equalizer lock */
#define V4L2_IN_ST_NO_CARRIER 0x00040000 /* Carrier recovery failed */

/* field 'status' - VCR and set-top box */
#define V4L2_IN_ST_MACROVISION 0x01000000 /* Macrovision detected */
#define V4L2_IN_ST_NO_ACCESS   0x02000000 /* Conditional access denied */
#define V4L2_IN_ST_VTR         0x04000000 /* VTR time constant */

/* capabilities flags */
#define V4L2_IN_CAP_DV_TIMINGS     0x00000002             /* Supports S_DV_TIMINGS */
#define V4L2_IN_CAP_CUSTOM_TIMINGS V4L2_IN_CAP_DV_TIMINGS /* For compatibility */
#define V4L2_IN_CAP_STD            0x00000004             /* Supports S_STD */
#define V4L2_IN_CAP_NATIVE_SIZE    0x00000008             /* Supports setting native size */

/*
 *	V I D E O   O U T P U T S
 */
struct v4l2_output {
    __u32 index;     /*  Which output */
    __u8 name[32];   /*  Label */
    __u32 type;      /*  Type of output */
    __u32 audioset;  /*  Associated audios (bitfield) */
    __u32 modulator; /*  Associated modulator */
    v4l2_std_id std;
    __u32 capabilities;
    __u32 reserved[3];
};
/*  Values for the 'type' field */
#define V4L2_OUTPUT_TYPE_MODULATOR        1
#define V4L2_OUTPUT_TYPE_ANALOG           2
#define V4L2_OUTPUT_TYPE_ANALOGVGAOVERLAY 3

/* capabilities flags */
#define V4L2_OUT_CAP_DV_TIMINGS     0x00000002              /* Supports S_DV_TIMINGS */
#define V4L2_OUT_CAP_CUSTOM_TIMINGS V4L2_OUT_CAP_DV_TIMINGS /* For compatibility */
#define V4L2_OUT_CAP_STD            0x00000004              /* Supports S_STD */
#define V4L2_OUT_CAP_NATIVE_SIZE    0x00000008              /* Supports setting native size */

/*
 *	C O N T R O L S
 */
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
        struct v4l2_area *p_area;
        struct v4l2_ctrl_h264_sps *p_h264_sps;
        struct v4l2_ctrl_h264_pps *p_h264_pps;
        struct v4l2_ctrl_h264_scaling_matrix *p_h264_scaling_matrix;
        struct v4l2_ctrl_h264_pred_weights *p_h264_pred_weights;
        struct v4l2_ctrl_h264_slice_params *p_h264_slice_params;
        struct v4l2_ctrl_h264_decode_params *p_h264_decode_params;
        struct v4l2_ctrl_fwht_params *p_fwht_params;
        struct v4l2_ctrl_vp8_frame *p_vp8_frame;
        struct v4l2_ctrl_mpeg2_sequence *p_mpeg2_sequence;
        struct v4l2_ctrl_mpeg2_picture *p_mpeg2_picture;
        struct v4l2_ctrl_mpeg2_quantisation *p_mpeg2_quantisation;
        struct v4l2_ctrl_vp9_compressed_hdr *p_vp9_compressed_hdr_probs;
        struct v4l2_ctrl_vp9_frame *p_vp9_frame;
        void *ptr;
    };
} __attribute__((packed));

struct v4l2_ext_controls {
    union {
        __u32 ctrl_class;
        __u32 which;
    };
    __u32 count;
    __u32 error_idx;
    __s32 request_fd;
    __u32 reserved[1];
    struct v4l2_ext_control *controls;
};

#define V4L2_CTRL_ID_MASK           (0x0fffffff)
#define V4L2_CTRL_ID2CLASS(id)      ((id)&0x0fff0000UL)
#define V4L2_CTRL_ID2WHICH(id)      ((id)&0x0fff0000UL)
#define V4L2_CTRL_DRIVER_PRIV(id)   (((id)&0xffff) >= 0x1000)
#define V4L2_CTRL_MAX_DIMS          (4)
#define V4L2_CTRL_WHICH_CUR_VAL     0
#define V4L2_CTRL_WHICH_DEF_VAL     0x0f000000
#define V4L2_CTRL_WHICH_REQUEST_VAL 0x0f010000

enum v4l2_ctrl_type {
    V4L2_CTRL_TYPE_INTEGER      = 1,
    V4L2_CTRL_TYPE_BOOLEAN      = 2,
    V4L2_CTRL_TYPE_MENU         = 3,
    V4L2_CTRL_TYPE_BUTTON       = 4,
    V4L2_CTRL_TYPE_INTEGER64    = 5,
    V4L2_CTRL_TYPE_CTRL_CLASS   = 6,
    V4L2_CTRL_TYPE_STRING       = 7,
    V4L2_CTRL_TYPE_BITMASK      = 8,
    V4L2_CTRL_TYPE_INTEGER_MENU = 9,

    /* Compound types are >= 0x0100 */
    V4L2_CTRL_COMPOUND_TYPES = 0x0100,
    V4L2_CTRL_TYPE_U8        = 0x0100,
    V4L2_CTRL_TYPE_U16       = 0x0101,
    V4L2_CTRL_TYPE_U32       = 0x0102,
    V4L2_CTRL_TYPE_AREA      = 0x0106,

    V4L2_CTRL_TYPE_HDR10_CLL_INFO          = 0x0110,
    V4L2_CTRL_TYPE_HDR10_MASTERING_DISPLAY = 0x0111,

    V4L2_CTRL_TYPE_H264_SPS            = 0x0200,
    V4L2_CTRL_TYPE_H264_PPS            = 0x0201,
    V4L2_CTRL_TYPE_H264_SCALING_MATRIX = 0x0202,
    V4L2_CTRL_TYPE_H264_SLICE_PARAMS   = 0x0203,
    V4L2_CTRL_TYPE_H264_DECODE_PARAMS  = 0x0204,
    V4L2_CTRL_TYPE_H264_PRED_WEIGHTS   = 0x0205,

    V4L2_CTRL_TYPE_FWHT_PARAMS = 0x0220,

    V4L2_CTRL_TYPE_VP8_FRAME = 0x0240,

    V4L2_CTRL_TYPE_MPEG2_QUANTISATION = 0x0250,
    V4L2_CTRL_TYPE_MPEG2_SEQUENCE     = 0x0251,
    V4L2_CTRL_TYPE_MPEG2_PICTURE      = 0x0252,

    V4L2_CTRL_TYPE_VP9_COMPRESSED_HDR = 0x0260,
    V4L2_CTRL_TYPE_VP9_FRAME          = 0x0261,
};

/*  Used in the VIDIOC_QUERYCTRL ioctl for querying controls */
struct v4l2_queryctrl {
    __u32 id;
    __u32 type;    /* enum v4l2_ctrl_type */
    __u8 name[32]; /* Whatever */
    __s32 minimum; /* Note signedness */
    __s32 maximum;
    __s32 step;
    __s32 default_value;
    __u32 flags;
    __u32 reserved[2];
};

/*  Used in the VIDIOC_QUERY_EXT_CTRL ioctl for querying extended controls */
struct v4l2_query_ext_ctrl {
    __u32 id;
    __u32 type;
    char name[32];
    __s64 minimum;
    __s64 maximum;
    __u64 step;
    __s64 default_value;
    __u32 flags;
    __u32 elem_size;
    __u32 elems;
    __u32 nr_of_dims;
    __u32 dims[V4L2_CTRL_MAX_DIMS];
    __u32 reserved[32];
};

/*  Used in the VIDIOC_QUERYMENU ioctl for querying menu items */
struct v4l2_querymenu {
    __u32 id;
    __u32 index;
    union {
        __u8 name[32]; /* Whatever */
        __s64 value;
    };
    __u32 reserved;
} __attribute__((packed));

/*  Control flags  */
#define V4L2_CTRL_FLAG_DISABLED         0x0001
#define V4L2_CTRL_FLAG_GRABBED          0x0002
#define V4L2_CTRL_FLAG_READ_ONLY        0x0004
#define V4L2_CTRL_FLAG_UPDATE           0x0008
#define V4L2_CTRL_FLAG_INACTIVE         0x0010
#define V4L2_CTRL_FLAG_SLIDER           0x0020
#define V4L2_CTRL_FLAG_WRITE_ONLY       0x0040
#define V4L2_CTRL_FLAG_VOLATILE         0x0080
#define V4L2_CTRL_FLAG_HAS_PAYLOAD      0x0100
#define V4L2_CTRL_FLAG_EXECUTE_ON_WRITE 0x0200
#define V4L2_CTRL_FLAG_MODIFY_LAYOUT    0x0400

/*  Query flags, to be ORed with the control ID */
#define V4L2_CTRL_FLAG_NEXT_CTRL     0x80000000
#define V4L2_CTRL_FLAG_NEXT_COMPOUND 0x40000000

/*  User-class control IDs defined by V4L2 */
#define V4L2_CID_MAX_CTRLS 1024
/*  IDs reserved for driver specific controls */
#define V4L2_CID_PRIVATE_BASE 0x08000000

#define V4L2_CID_BASE               0x00980900
#define V4L2_CID_BRIGHTNESS         (V4L2_CID_BASE + 0)
#define V4L2_CID_CONTRAST           (V4L2_CID_BASE + 1)
#define V4L2_CID_SATURATION         (V4L2_CID_BASE + 2)
#define V4L2_CID_HUE                (V4L2_CID_BASE + 3)
#define V4L2_CID_RED_BALANCE        (V4L2_CID_BASE + 14)
#define V4L2_CID_BLUE_BALANCE       (V4L2_CID_BASE + 15)
#define V4L2_CID_GAMMA              (V4L2_CID_BASE + 16)
#define V4L2_CID_EXPOSURE           (V4L2_CID_BASE + 17)
#define V4L2_CID_AUTOGAIN           (V4L2_CID_BASE + 18)
#define V4L2_CID_GAIN               (V4L2_CID_BASE + 19)
#define V4L2_CID_AUTO_WHITE_BALANCE (V4L2_CID_BASE + 12)
#define V4L2_CID_AUTO_EXPOSURE_BIAS (V4L2_CID_BASE + 26)

/*
 *	T U N I N G
 */
struct v4l2_tuner {
    __u32 index;
    __u8 name[32];
    __u32 type; /* enum v4l2_tuner_type */
    __u32 capability;
    __u32 rangelow;
    __u32 rangehigh;
    __u32 rxsubchans;
    __u32 audmode;
    __s32 signal;
    __s32 afc;
    __u32 reserved[4];
};

struct v4l2_modulator {
    __u32 index;
    __u8 name[32];
    __u32 capability;
    __u32 rangelow;
    __u32 rangehigh;
    __u32 txsubchans;
    __u32 type; /* enum v4l2_tuner_type */
    __u32 reserved[3];
};

/*  Flags for the 'capability' field */
#define V4L2_TUNER_CAP_LOW             0x0001
#define V4L2_TUNER_CAP_NORM            0x0002
#define V4L2_TUNER_CAP_HWSEEK_BOUNDED  0x0004
#define V4L2_TUNER_CAP_HWSEEK_WRAP     0x0008
#define V4L2_TUNER_CAP_STEREO          0x0010
#define V4L2_TUNER_CAP_LANG2           0x0020
#define V4L2_TUNER_CAP_SAP             0x0020
#define V4L2_TUNER_CAP_LANG1           0x0040
#define V4L2_TUNER_CAP_RDS             0x0080
#define V4L2_TUNER_CAP_RDS_BLOCK_IO    0x0100
#define V4L2_TUNER_CAP_RDS_CONTROLS    0x0200
#define V4L2_TUNER_CAP_FREQ_BANDS      0x0400
#define V4L2_TUNER_CAP_HWSEEK_PROG_LIM 0x0800
#define V4L2_TUNER_CAP_1HZ             0x1000

/*  Flags for the 'rxsubchans' field */
#define V4L2_TUNER_SUB_MONO   0x0001
#define V4L2_TUNER_SUB_STEREO 0x0002
#define V4L2_TUNER_SUB_LANG2  0x0004
#define V4L2_TUNER_SUB_SAP    0x0004
#define V4L2_TUNER_SUB_LANG1  0x0008
#define V4L2_TUNER_SUB_RDS    0x0010

/*  Values for the 'audmode' field */
#define V4L2_TUNER_MODE_MONO        0x0000
#define V4L2_TUNER_MODE_STEREO      0x0001
#define V4L2_TUNER_MODE_LANG2       0x0002
#define V4L2_TUNER_MODE_SAP         0x0002
#define V4L2_TUNER_MODE_LANG1       0x0003
#define V4L2_TUNER_MODE_LANG1_LANG2 0x0004

struct v4l2_frequency {
    __u32 tuner;
    __u32 type; /* enum v4l2_tuner_type */
    __u32 frequency;
    __u32 reserved[8];
};

#define V4L2_BAND_MODULATION_VSB (1 << 1)
#define V4L2_BAND_MODULATION_FM  (1 << 2)
#define V4L2_BAND_MODULATION_AM  (1 << 3)

struct v4l2_frequency_band {
    __u32 tuner;
    __u32 type; /* enum v4l2_tuner_type */
    __u32 index;
    __u32 capability;
    __u32 rangelow;
    __u32 rangehigh;
    __u32 modulation;
    __u32 reserved[9];
};

struct v4l2_hw_freq_seek {
    __u32 tuner;
    __u32 type; /* enum v4l2_tuner_type */
    __u32 seek_upward;
    __u32 wrap_around;
    __u32 spacing;
    __u32 rangelow;
    __u32 rangehigh;
    __u32 reserved[5];
};

/*
 *	R D S
 */

struct v4l2_rds_data {
    __u8 lsb;
    __u8 msb;
    __u8 block;
} __attribute__((packed));

#define V4L2_RDS_BLOCK_MSK     0x7
#define V4L2_RDS_BLOCK_A       0
#define V4L2_RDS_BLOCK_B       1
#define V4L2_RDS_BLOCK_C       2
#define V4L2_RDS_BLOCK_D       3
#define V4L2_RDS_BLOCK_C_ALT   4
#define V4L2_RDS_BLOCK_INVALID 7

#define V4L2_RDS_BLOCK_CORRECTED 0x40
#define V4L2_RDS_BLOCK_ERROR     0x80

/*
 *	A U D I O
 */
struct v4l2_audio {
    __u32 index;
    __u8 name[32];
    __u32 capability;
    __u32 mode;
    __u32 reserved[2];
};

/*  Flags for the 'capability' field */
#define V4L2_AUDCAP_STEREO 0x00001
#define V4L2_AUDCAP_AVL    0x00002

/*  Flags for the 'mode' field */
#define V4L2_AUDMODE_AVL 0x00001

struct v4l2_audioout {
    __u32 index;
    __u8 name[32];
    __u32 capability;
    __u32 mode;
    __u32 reserved[2];
};

/*
 *	M P E G   S E R V I C E S
 */
#if 1
#define V4L2_ENC_IDX_FRAME_I    (0)
#define V4L2_ENC_IDX_FRAME_P    (1)
#define V4L2_ENC_IDX_FRAME_B    (2)
#define V4L2_ENC_IDX_FRAME_MASK (0xf)

struct v4l2_enc_idx_entry {
    __u64 offset;
    __u64 pts;
    __u32 length;
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
    __u32 reserved[2];
};

/* --- format union --- */
struct v4l2_format {
    __u32 type; /* enum v4l2_buf_type */
#define V4L2_ENC_IDX_ENTRIES (64)
struct v4l2_enc_idx {
    __u32 entries;
    __u32 entries_cap;
    __u32 reserved[4];
    struct v4l2_enc_idx_entry entry[V4L2_ENC_IDX_ENTRIES];
};

#define V4L2_ENC_CMD_START  (0)
#define V4L2_ENC_CMD_STOP   (1)
#define V4L2_ENC_CMD_PAUSE  (2)
#define V4L2_ENC_CMD_RESUME (3)

/* Flags for V4L2_ENC_CMD_STOP */
#define V4L2_ENC_CMD_STOP_AT_GOP_END (1 << 0)

struct v4l2_encoder_cmd {
    __u32 cmd;
    __u32 flags;
    union {
        struct v4l2_pix_format        pix;
        struct v4l2_pix_format_mplane pix_mp;
        struct v4l2_window            win;
        struct v4l2_meta_format       meta;
        __u8 raw_data[200];
    } fmt;
        struct {
            __u32 data[8];
        } raw;
    };
};

/* Decoder commands */
#define V4L2_DEC_CMD_START  (0)
#define V4L2_DEC_CMD_STOP   (1)
#define V4L2_DEC_CMD_PAUSE  (2)
#define V4L2_DEC_CMD_RESUME (3)
#define V4L2_DEC_CMD_FLUSH  (4)

/* Flags for V4L2_DEC_CMD_START */
#define V4L2_DEC_CMD_START_MUTE_AUDIO (1 << 0)

/* Flags for V4L2_DEC_CMD_PAUSE */
#define V4L2_DEC_CMD_PAUSE_TO_BLACK (1 << 0)

/* Flags for V4L2_DEC_CMD_STOP */
#define V4L2_DEC_CMD_STOP_TO_BLACK    (1 << 0)
#define V4L2_DEC_CMD_STOP_IMMEDIATELY (1 << 1)

/* Play format requirements (returned by the driver): */

/* The decoder has no special format requirements */
#define V4L2_DEC_START_FMT_NONE (0)
/* The decoder requires full GOPs */
#define V4L2_DEC_START_FMT_GOP (1)

/* The structure must be zeroed before use by the application
   This ensures it can be extended safely in the future. */
struct v4l2_decoder_cmd {
    __u32 cmd;
    __u32 flags;
    union {
        struct {
            __u64 pts;
        } stop;

        struct {
            /* 0 or 1000 specifies normal speed,
               1 specifies forward single stepping,
               -1 specifies backward single stepping,
               >1: playback at speed/1000 of the normal speed,
               <-1: reverse playback at (-speed/1000) of the normal speed. */
            __s32 speed;
            __u32 format;
        } start;

        struct {
            __u32 data[16];
        } raw;
    };
};
#endif

/*
 *	D A T A   S E R V I C E S   ( V B I )
 *	Data services API by Michael Schimek
 */

/* Raw VBI */
struct v4l2_vbi_format {
    __u32 sampling_rate; /* in 1 Hz */
    __u32 offset;
    __u32 samples_per_line;
    __u32 sample_format; /* V4L2_PIX_FMT_* */
    __s32 start[2];
    __u32 count[2];
    __u32 flags;       /* V4L2_VBI_* */
    __u32 reserved[2]; /* must be zero */
};

/*  VBI flags  */
#define V4L2_VBI_UNSYNC     (1 << 0)
#define V4L2_VBI_INTERLACED (1 << 1)

/* ITU-R start lines for each field */
#define V4L2_VBI_ITU_525_F1_START (1)
#define V4L2_VBI_ITU_525_F2_START (264)
#define V4L2_VBI_ITU_625_F1_START (1)
#define V4L2_VBI_ITU_625_F2_START (314)

/* Sliced VBI
 *
 *    This implements is a proposal V4L2 API to allow SLICED VBI
 * required for some hardware encoders. It should change without
 * notice in the definitive implementation.
 */

struct v4l2_sliced_vbi_format {
    __u16 service_set;
    /* service_lines[0][...] specifies lines 0-23 (1-23 used) of the first field
       service_lines[1][...] specifies lines 0-23 (1-23 used) of the second field
                 (equals frame lines 313-336 for 625 line video
                  standards, 263-286 for 525 line standards) */
    __u16 service_lines[2][24];
    __u32 io_size;
    __u32 reserved[2]; /* must be zero */
};

/* Teletext World System Teletext
   (WST), defined on ITU-R BT.653-2 */
#define V4L2_SLICED_TELETEXT_B (0x0001)
/* Video Program System, defined on ETS 300 231*/
#define V4L2_SLICED_VPS (0x0400)
/* Closed Caption, defined on EIA-608 */
#define V4L2_SLICED_CAPTION_525 (0x1000)
/* Wide Screen System, defined on ITU-R BT1119.1 */
#define V4L2_SLICED_WSS_625 (0x4000)

#define V4L2_SLICED_VBI_525 (V4L2_SLICED_CAPTION_525)
#define V4L2_SLICED_VBI_625 (V4L2_SLICED_TELETEXT_B | V4L2_SLICED_VPS | V4L2_SLICED_WSS_625)

struct v4l2_sliced_vbi_cap {
    __u16 service_set;
    /* service_lines[0][...] specifies lines 0-23 (1-23 used) of the first field
       service_lines[1][...] specifies lines 0-23 (1-23 used) of the second field
                 (equals frame lines 313-336 for 625 line video
                  standards, 263-286 for 525 line standards) */
    __u16 service_lines[2][24];
    __u32 type;        /* enum v4l2_buf_type */
    __u32 reserved[3]; /* must be 0 */
};

struct v4l2_sliced_vbi_data {
    __u32 id;
    __u32 field;    /* 0: first field, 1: second field */
    __u32 line;     /* 1-23 */
    __u32 reserved; /* must be 0 */
    __u8 data[48];
};

/*
 * Sliced VBI data inserted into MPEG Streams
 */

/*
 * V4L2_MPEG_STREAM_VBI_FMT_IVTV:
 *
 * Structure of payload contained in an MPEG 2 Private Stream 1 PES Packet in an
 * MPEG-2 Program Pack that contains V4L2_MPEG_STREAM_VBI_FMT_IVTV Sliced VBI
 * data
 *
 * Note, the MPEG-2 Program Pack and Private Stream 1 PES packet header
 * definitions are not included here.  See the MPEG-2 specifications for details
 * on these headers.
 */

/* Line type IDs */
#define V4L2_MPEG_VBI_IVTV_TELETEXT_B  (1)
#define V4L2_MPEG_VBI_IVTV_CAPTION_525 (4)
#define V4L2_MPEG_VBI_IVTV_WSS_625     (5)
#define V4L2_MPEG_VBI_IVTV_VPS         (7)

struct v4l2_mpeg_vbi_itv0_line {
    __u8 id;       /* One of V4L2_MPEG_VBI_IVTV_* above */
    __u8 data[42]; /* Sliced VBI data for the line */
} __attribute__((packed));

struct v4l2_mpeg_vbi_itv0 {
    __le32 linemask[2]; /* Bitmasks of VBI service lines present */
    struct v4l2_mpeg_vbi_itv0_line line[35];
} __attribute__((packed));

struct v4l2_mpeg_vbi_ITV0 {
    struct v4l2_mpeg_vbi_itv0_line line[36];
} __attribute__((packed));

#define V4L2_MPEG_VBI_IVTV_MAGIC0 "itv0"
#define V4L2_MPEG_VBI_IVTV_MAGIC1 "ITV0"

struct v4l2_mpeg_vbi_fmt_ivtv {
    __u8 magic[4];
    union {
        struct v4l2_mpeg_vbi_itv0 itv0;
        struct v4l2_mpeg_vbi_ITV0 ITV0;
    };
} __attribute__((packed));

/* ===================== Énumération formats / tailles / cadences ===================== */
struct v4l2_fmtdesc {
    __u32 index;
    __u32 type;          /* enum v4l2_buf_type */
    __u32 flags;
    __u8  description[32];
    __u8 description[32];
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

struct v4l2_frmsize_discrete {
    __u32 width;
    __u32 height;
};

struct v4l2_frmsize_stepwise {
    __u32 min_width,  max_width,  step_width;
    __u32 min_height, max_height, step_height;
    __u32 min_width;
    __u32 max_width;
    __u32 step_width;
    __u32 min_height;
    __u32 max_height;
    __u32 step_height;
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

struct v4l2_standard {
    __u32 index;
    v4l2_std_id id;
    __u8 name[24];
    struct v4l2_fract frameperiod;
    __u32 framelines;
    __u32 reserved[4];
};

struct v4l2_edid {
    __u32 pad;
    __u32 start_block;
    __u32 blocks;
    __u32 reserved[5];
    __u8 *edid;
};

/* ===================== Crop / Selection ===================== */
struct v4l2_cropcap {
    __u32 type;  /* enum v4l2_buf_type */
    struct v4l2_area   pixelaspect;
    struct v4l2_rect   bounds;
    struct v4l2_rect   defrect;
    struct v4l2_fract  def_frame_interval;
    struct v4l2_area  pixelaspect;
    struct v4l2_rect  bounds;
    struct v4l2_rect  defrect;
    struct v4l2_fract def_frame_interval;
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
    V4L2_SEL_TGT_CROP_DEFAULT,
    V4L2_SEL_TGT_COMPOSE_ACTIVE,
    V4L2_SEL_TGT_COMPOSE_DEFAULT,
    V4L2_SEL_TGT_CROP_BOUNDS,
    V4L2_SEL_TGT_COMPOSE_BOUNDS
};
#define V4L2_SEL_FLAG_GE        0x0001
#define V4L2_SEL_FLAG_LE        0x0002
#define V4L2_SEL_FLAG_KEEP_CONFIG 0x0004

#define V4L2_SEL_FLAG_GE           0x0001
#define V4L2_SEL_FLAG_LE           0x0002
#define V4L2_SEL_FLAG_KEEP_CONFIG  0x0004

struct v4l2_selection {
    __u32 type;
    __u32 target;   /* enum v4l2_selection_target */
    __u32 flags;    /* V4L2_SEL_FLAG_* */
    __u32 target; /* enum v4l2_selection_target */
    __u32 flags;  /* V4L2_SEL_FLAG_* */
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
#define V4L2_BUF_FLAG_MAPPED          0x00000001
#define V4L2_BUF_FLAG_QUEUED          0x00000002
#define V4L2_BUF_FLAG_DONE            0x00000004
#define V4L2_BUF_FLAG_KEYFRAME        0x00000008
#define V4L2_BUF_FLAG_PFRAME          0x00000010
#define V4L2_BUF_FLAG_BFRAME          0x00000020
#define V4L2_BUF_FLAG_ERROR           0x00000040
#define V4L2_BUF_FLAG_TIMECODE        0x00000100
#define V4L2_BUF_FLAG_PREPARED        0x00000400
#define V4L2_BUF_FLAG_NO_CACHE_INVALIDATE 0x00000800
#define V4L2_BUF_FLAG_NO_CACHE_CLEAN      0x00001000
#define V4L2_BUF_FLAG_TIMESTAMP_MASK  0x0000e000

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
        __u32 offset;
        unsigned long userptr;
        struct v4l2_plane *planes; /* multi-planar */
        struct v4l2_plane *planes;
        __s32 fd;
    } m;
    __u32 length;   /* nb de planes pour mplane */
    __u32 length;
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
struct v4l2_exportbuffer {
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
    __u32 plane;
    __u32 flags;
    __s32 fd;
    __u32 reserved[11];
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
struct v4l2_captureparm {
    __u32 capability;
    __u32 capturemode;
    struct v4l2_fract timeperframe;
    __u32 extendedmode;
    __u32 readbuffers;
    __u32 reserved[4];
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
struct v4l2_outputparm {
    __u32 capability;
    __u32 outputmode;
    struct v4l2_fract timeperframe;
    __u32 extendedmode;
    __u32 writebuffers;
    __u32 reserved[4];
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
#define V4L2_CID_MPEG_BASE 0x00990900

#define V4L2_CID_MPEG_VIDEO_BITRATE       (V4L2_CID_MPEG_BASE + 207)
#define V4L2_CID_MPEG_VIDEO_GOP_SIZE      (V4L2_CID_MPEG_BASE + 210)
#define V4L2_CID_JPEG_COMPRESSION_QUALITY (V4L2_CID_MPEG_BASE + 500)

#define V4L2_CID_MPEG_VIDEO_H264_PROFILE    (V4L2_CID_MPEG_BASE + 264)
#define V4L2_CID_MPEG_VIDEO_H264_LEVEL      (V4L2_CID_MPEG_BASE + 265)
#define V4L2_CID_MPEG_VIDEO_H264_I_PERIOD   (V4L2_CID_MPEG_BASE + 266)
#define V4L2_CID_MPEG_VIDEO_H264_MIN_QP     (V4L2_CID_MPEG_BASE + 267)
#define V4L2_CID_MPEG_VIDEO_H264_MAX_QP     (V4L2_CID_MPEG_BASE + 268)
#define V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP (V4L2_CID_MPEG_BASE + 269)
#define V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP (V4L2_CID_MPEG_BASE + 270)
#define V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP (V4L2_CID_MPEG_BASE + 271)
#define V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE (V4L2_CID_MPEG_BASE + 274)

struct v4l2_jpegcompression {
    int quality;
    int APPn;                /* 0 à 15 */
    int APPn;
    int APP_len;
    char *APP_data;
    int COM_len;
    char *COM_data;
    __u32 jpeg_markers;      /* masques pour marquers à inclure */
    __u32 jpeg_markers;
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
/*
 *	A G G R E G A T E   S T R U C T U R E S
 */

struct v4l2_clip {
    struct v4l2_rect c;
    struct v4l2_clip *next;
};

/* Sous-structs utiles (ctrl) */
struct v4l2_window {
    struct v4l2_rect w;
    __u32 field;
    __u32 chromakey;
    struct v4l2_clip *clips;
    __u32 clipcount;
    void *bitmap;
    __u8 global_alpha;
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

/**
 * struct v4l2_plane_pix_format - additional, per-plane format definition
 * @sizeimage:maximum size in bytes required for data, for which
 *             this plane will be used
 * @bytesperline:distance in bytes between the leftmost pixels in two
 *             adjacent lines
 * @reserved:drivers and applications must zero this array
 */
struct v4l2_plane_pix_format {
    __u32 sizeimage;
    __u32 bytesperline;
    __u16 reserved[6];
} __attribute__((packed));

/**
 * struct v4l2_pix_format_mplane - multiplanar format definition
 * @width:image width in pixels
 * @height:image height in pixels
 * @pixelformat:little endian four character code (fourcc)
 * @field:enum v4l2_field; field order (for interlaced video)
 * @colorspace:enum v4l2_colorspace; supplemental to pixelformat
 * @plane_fmt:per-plane information
 * @num_planes:number of planes for this format
 * @flags:format flags (V4L2_PIX_FMT_FLAG_*)
 * @ycbcr_enc:enum v4l2_ycbcr_encoding, Y'CbCr encoding
 * @hsv_enc:enum v4l2_hsv_encoding, HSV encoding
 * @quantization:enum v4l2_quantization, colorspace quantization
 * @xfer_func:enum v4l2_xfer_func, colorspace transfer function
 * @reserved:drivers and applications must zero this array
 */
struct v4l2_pix_format_mplane {
    __u32 width;
    __u32 height;
    __u32 pixelformat;
    __u32 field;
    __u32 colorspace;

    struct v4l2_plane_pix_format plane_fmt[VIDEO_MAX_PLANES];
    __u8 num_planes;
    __u8 flags;
    union {
        __u8 ycbcr_enc;
        __u8 hsv_enc;
    };
    __u8 quantization;
    __u8 xfer_func;
    __u8 reserved[7];
} __attribute__((packed));

struct v4l2_sdr_format {
    __u32 pixelformat;
    __u32 buffersize;
    __u8 reserved[24];
} __attribute__((packed));

struct v4l2_meta_format {
    __u32 dataformat;
    __u32 buffersize;
} __attribute__((packed));

struct v4l2_format {
    __u32 type;
    union {
        struct v4l2_pix_format pix;           /* V4L2_BUF_TYPE_VIDEO_CAPTURE */
        struct v4l2_pix_format_mplane pix_mp; /* V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
        struct v4l2_window win;               /* V4L2_BUF_TYPE_VIDEO_OVERLAY */
        struct v4l2_vbi_format vbi;           /* V4L2_BUF_TYPE_VBI_CAPTURE */
        struct v4l2_sliced_vbi_format sliced; /* V4L2_BUF_TYPE_SLICED_VBI_CAPTURE */
        struct v4l2_sdr_format sdr;           /* V4L2_BUF_TYPE_SDR_CAPTURE */
        struct v4l2_meta_format meta;         /* V4L2_BUF_TYPE_META_CAPTURE */
        __u8 raw_data[200];                   /* user-defined */
    } fmt;
};

struct v4l2_streamparm {
    __u32 type;
    union {
        struct v4l2_captureparm capture;
        struct v4l2_outputparm output;
        __u8 raw_data[200];
    } parm;
};

#define V4L2_EVENT_ALL           0
#define V4L2_EVENT_VSYNC         1
#define V4L2_EVENT_EOS           2
#define V4L2_EVENT_CTRL          3
#define V4L2_EVENT_FRAME_SYNC    4
#define V4L2_EVENT_SOURCE_CHANGE 5
#define V4L2_EVENT_MOTION_DET    6
#define V4L2_EVENT_PRIVATE_START 0x08000000

struct v4l2_event_vsync {
    __u8 field;
} __attribute__((packed));

#define V4L2_EVENT_CTRL_CH_VALUE (1 << 0)
#define V4L2_EVENT_CTRL_CH_FLAGS (1 << 1)
#define V4L2_EVENT_CTRL_CH_RANGE (1 << 2)

struct v4l2_event_ctrl {
    __u32 changes;
    __u32 type;
    union {
        __s32 value;
        __s64 value64;
    };
    __u32 flags;
    __s32 value;
    __s64 value64;
    __u32 id;
    __u32 reserved[3];
    __s32 minimum;
    __s32 maximum;
    __s32 step;
    __s32 default_value;
};

struct v4l2_event_frame_sync {
    __u32 frame_sequence;
};

#define V4L2_EVENT_SRC_CH_RESOLUTION (1 << 0)

struct v4l2_event_src_change {
    __u32 changes;
};

#define V4L2_EVENT_MD_FL_HAVE_FRAME_SEQ (1 << 0)

struct v4l2_event_motion_det {
    __u32 flags;
    __u32 frame_sequence;
    __u32 region_mask;
};

/* Évènement générique portable (timeval au lieu de timespec) */
struct v4l2_event {
    __u32 type;
    union {
        struct v4l2_event_vsync vsync;
        struct v4l2_event_ctrl ctrl;
        struct v4l2_event_frame_sync frame_sync;
        struct v4l2_event_src_change src_change;
        struct v4l2_event_motion_det motion_det;
        __u8 data[64];
    } u;
    __u32 pending;
    __u32 sequence;
    struct timeval timestamp;
    struct timespec timestamp;
    __u32 id;
    __u32 reserved[8];
};

#define V4L2_EVENT_SUB_FL_SEND_INITIAL  0x0001
#define V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK 0x0002
#define V4L2_EVENT_SUB_FL_SEND_INITIAL   (1 << 0)
#define V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK (1 << 1)

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
#define V4L2_CHIP_MATCH_BRIDGE 0
#define V4L2_CHIP_MATCH_SUBDEV 4
#define V4L2_CHIP_MATCH_HOST       V4L2_CHIP_MATCH_BRIDGE
#define V4L2_CHIP_MATCH_I2C_DRIVER 1
#define V4L2_CHIP_MATCH_I2C_ADDR   2
#define V4L2_CHIP_MATCH_AC97       3

struct v4l2_dbg_match {
    __u32 type;
    union {
        __u32 addr;
        char name[32];
    };
} __attribute__((packed));

struct v4l2_dbg_register {
    struct v4l2_dbg_match match;
    __u32 size;
    __u64 reg;
    __u64 val;
} __attribute__((packed));

#define V4L2_CHIP_FL_READABLE (1 << 0)
#define V4L2_CHIP_FL_WRITABLE (1 << 1)

struct v4l2_dbg_chip_info {
    struct v4l2_dbg_match match;
    char name[32];
    __u32 flags;
    __u32 reserved[32];
} __attribute__((packed));

struct v4l2_create_buffers {
    __u32 index;
    __u32 count;
    __u32 memory;
    struct v4l2_format format;
    __u32 capabilities;
    __u32 flags;
    __u32 reserved[6];
};

#define VIDIOC_QUERYCAP            _IOR('V', 0, struct v4l2_capability)
#define VIDIOC_ENUM_FMT            _IOWR('V', 2, struct v4l2_fmtdesc)
#define VIDIOC_G_FMT               _IOWR('V', 4, struct v4l2_format)
#define VIDIOC_S_FMT               _IOWR('V', 5, struct v4l2_format)
#define VIDIOC_REQBUFS             _IOWR('V', 8, struct v4l2_requestbuffers)
#define VIDIOC_QUERYBUF            _IOWR('V', 9, struct v4l2_buffer)
#define VIDIOC_G_FBUF              _IOR('V', 10, struct v4l2_framebuffer)
#define VIDIOC_S_FBUF              _IOW('V', 11, struct v4l2_framebuffer)
#define VIDIOC_OVERLAY             _IOW('V', 14, int)
#define VIDIOC_QBUF                _IOWR('V', 15, struct v4l2_buffer)
#define VIDIOC_EXPBUF              _IOWR('V', 16, struct v4l2_exportbuffer)
#define VIDIOC_DQBUF               _IOWR('V', 17, struct v4l2_buffer)
#define VIDIOC_STREAMON            _IOW('V', 18, int)
#define VIDIOC_STREAMOFF           _IOW('V', 19, int)
#define VIDIOC_G_PARM              _IOWR('V', 21, struct v4l2_streamparm)
#define VIDIOC_S_PARM              _IOWR('V', 22, struct v4l2_streamparm)
#define VIDIOC_G_STD               _IOR('V', 23, v4l2_std_id)
#define VIDIOC_S_STD               _IOW('V', 24, v4l2_std_id)
#define VIDIOC_ENUMSTD             _IOWR('V', 25, struct v4l2_standard)
#define VIDIOC_ENUMINPUT           _IOWR('V', 26, struct v4l2_input)
#define VIDIOC_G_CTRL              _IOWR('V', 27, struct v4l2_control)
#define VIDIOC_S_CTRL              _IOWR('V', 28, struct v4l2_control)
#define VIDIOC_G_TUNER             _IOWR('V', 29, struct v4l2_tuner)
#define VIDIOC_S_TUNER             _IOW('V', 30, struct v4l2_tuner)
#define VIDIOC_G_AUDIO             _IOR('V', 33, struct v4l2_audio)
#define VIDIOC_S_AUDIO             _IOW('V', 34, struct v4l2_audio)
#define VIDIOC_QUERYCTRL           _IOWR('V', 36, struct v4l2_queryctrl)
#define VIDIOC_QUERYMENU           _IOWR('V', 37, struct v4l2_querymenu)
#define VIDIOC_G_INPUT             _IOR('V', 38, int)
#define VIDIOC_S_INPUT             _IOWR('V', 39, int)
#define VIDIOC_G_EDID              _IOWR('V', 40, struct v4l2_edid)
#define VIDIOC_S_EDID              _IOWR('V', 41, struct v4l2_edid)
#define VIDIOC_G_OUTPUT            _IOR('V', 46, int)
#define VIDIOC_S_OUTPUT            _IOWR('V', 47, int)
#define VIDIOC_ENUMOUTPUT          _IOWR('V', 48, struct v4l2_output)
#define VIDIOC_G_AUDOUT            _IOR('V', 49, struct v4l2_audioout)
#define VIDIOC_S_AUDOUT            _IOW('V', 50, struct v4l2_audioout)
#define VIDIOC_G_MODULATOR         _IOWR('V', 54, struct v4l2_modulator)
#define VIDIOC_S_MODULATOR         _IOW('V', 55, struct v4l2_modulator)
#define VIDIOC_G_FREQUENCY         _IOWR('V', 56, struct v4l2_frequency)
#define VIDIOC_S_FREQUENCY         _IOW('V', 57, struct v4l2_frequency)
#define VIDIOC_CROPCAP             _IOWR('V', 58, struct v4l2_cropcap)
#define VIDIOC_G_CROP              _IOWR('V', 59, struct v4l2_crop)
#define VIDIOC_S_CROP              _IOW('V', 60, struct v4l2_crop)
#define VIDIOC_G_JPEGCOMP          _IOR('V', 61, struct v4l2_jpegcompression)
#define VIDIOC_S_JPEGCOMP          _IOW('V', 62, struct v4l2_jpegcompression)
#define VIDIOC_QUERYSTD            _IOR('V', 63, v4l2_std_id)
#define VIDIOC_TRY_FMT             _IOWR('V', 64, struct v4l2_format)
#define VIDIOC_ENUMAUDIO           _IOWR('V', 65, struct v4l2_audio)
#define VIDIOC_ENUMAUDOUT          _IOWR('V', 66, struct v4l2_audioout)
#define VIDIOC_G_PRIORITY          _IOR('V', 67, __u32)
#define VIDIOC_S_PRIORITY          _IOW('V', 68, __u32)
#define VIDIOC_G_SLICED_VBI_CAP    _IOWR('V', 69, struct v4l2_sliced_vbi_cap)
#define VIDIOC_LOG_STATUS          _IO('V', 70)
#define VIDIOC_G_EXT_CTRLS         _IOWR('V', 71, struct v4l2_ext_controls)
#define VIDIOC_S_EXT_CTRLS         _IOWR('V', 72, struct v4l2_ext_controls)
#define VIDIOC_TRY_EXT_CTRLS       _IOWR('V', 73, struct v4l2_ext_controls)
#define VIDIOC_ENUM_FRAMESIZES     _IOWR('V', 74, struct v4l2_frmsizeenum)
#define VIDIOC_ENUM_FRAMEINTERVALS _IOWR('V', 75, struct v4l2_frmivalenum)
#define VIDIOC_G_ENC_INDEX         _IOR('V', 76, struct v4l2_enc_idx)
#define VIDIOC_ENCODER_CMD         _IOWR('V', 77, struct v4l2_encoder_cmd)
#define VIDIOC_TRY_ENCODER_CMD     _IOWR('V', 78, struct v4l2_encoder_cmd)
#define VIDIOC_DBG_S_REGISTER      _IOW('V', 79, struct v4l2_dbg_register)
#define VIDIOC_DBG_G_REGISTER      _IOWR('V', 80, struct v4l2_dbg_register)
#define VIDIOC_S_HW_FREQ_SEEK      _IOW('V', 82, struct v4l2_hw_freq_seek)
#define VIDIOC_S_DV_TIMINGS        _IOWR('V', 87, struct v4l2_dv_timings)
#define VIDIOC_G_DV_TIMINGS        _IOWR('V', 88, struct v4l2_dv_timings)
#define VIDIOC_DQEVENT             _IOR('V', 89, struct v4l2_event)
#define VIDIOC_SUBSCRIBE_EVENT     _IOW('V', 90, struct v4l2_event_subscription)
#define VIDIOC_UNSUBSCRIBE_EVENT   _IOW('V', 91, struct v4l2_event_subscription)
#define VIDIOC_CREATE_BUFS         _IOWR('V', 92, struct v4l2_create_buffers)
#define VIDIOC_PREPARE_BUF         _IOWR('V', 93, struct v4l2_buffer)
#define VIDIOC_G_SELECTION         _IOWR('V', 94, struct v4l2_selection)
#define VIDIOC_S_SELECTION         _IOWR('V', 95, struct v4l2_selection)
#define VIDIOC_DECODER_CMD         _IOWR('V', 96, struct v4l2_decoder_cmd)
#define VIDIOC_TRY_DECODER_CMD     _IOWR('V', 97, struct v4l2_decoder_cmd)
#define VIDIOC_ENUM_DV_TIMINGS     _IOWR('V', 98, struct v4l2_enum_dv_timings)
#define VIDIOC_QUERY_DV_TIMINGS    _IOR('V', 99, struct v4l2_dv_timings)
#define VIDIOC_DV_TIMINGS_CAP      _IOWR('V', 100, struct v4l2_dv_timings_cap)
#define VIDIOC_ENUM_FREQ_BANDS     _IOWR('V', 101, struct v4l2_frequency_band)
#define VIDIOC_DBG_G_CHIP_INFO     _IOWR('V', 102, struct v4l2_dbg_chip_info)
#define VIDIOC_QUERY_EXT_CTRL      _IOWR('V', 103, struct v4l2_query_ext_ctrl)

#define BASE_VIDIOC_PRIVATE 192
#define V4L2_PIX_FMT_HM12             V4L2_PIX_FMT_NV12_16L16
#define V4L2_PIX_FMT_SUNXI_TILED_NV12 V4L2_PIX_FMT_NV12_32L32

#ifdef __cplusplus
}
#endif

#endif /* __LINUX_VIDEODEV2_H */



