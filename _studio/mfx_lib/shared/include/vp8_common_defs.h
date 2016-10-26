//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#ifndef __VP8_COMMON_DEFS_H__
#define __VP8_COMMON_DEFS_H__

#include <stdlib.h>
#include "mfxdefs.h"

#pragma warning(disable: 4201)

typedef union { /* MV type */
    struct {
        mfxI16  mvx;
        mfxI16  mvy;
    } MV;
    mfxI32 s32;
} MvTypeVp8;

typedef mfxI16    CoeffsType;

typedef struct { 
    union {
        mfxU32     DWord0;
        struct {
            mfxU32  RefFrameMode       : 2;    /* 0 - intra, 1 - last, 2 - gold, 3 - altref */
            mfxU32  SegmentId          : 2;    /* Segment Id */
            mfxU32  CoeffSkip          : 1;    /* Is all coefficient are zero */
            mfxU32  MbXcnt             : 10;   /* MB column */
            mfxU32  MbYcnt             : 10;   /* MB row */
            mfxU32  MbMode             : 3;    /* Intra / Inter MB mode 0 - 4, depends on RefFrameMode */
            mfxU32  MbSubmode          : 2;    /* Intra UV or Inter partition, depends on RefFrameMode */
            mfxU32  Y2AboveNotZero     : 1;    /* Non-zero Y2 block for this or next above Mb with Y2 */
            mfxU32  Y2LeftNotZero      : 1;    /* Non-zero Y2 block for this or next left Mb with Y2 */
        };
    };
    union {
        struct {
            mfxU16     Y4x4Modes0;             /* IntraY4x4, modes for subblocks 0 - 3 (4 x 4bits)*/
            mfxU16     Y4x4Modes1;             /* IntraY4x4, modes for subblocks 4 - 7 (4 x 4bits)*/
        };
        mfxU32     InterSplitModes;        /* Inter split subblock modes (16 x 2 bits) */
    };
    union {
        struct {
            mfxU16     Y4x4Modes2;             /* IntraY4x4, modes for subblocks 8 - 11 (4 x 4bits)*/
            mfxU16     Y4x4Modes3;             /* IntraY4x4, modes for subblocks 12 - 15 (4 x 4bits)*/
        };
        struct {
            MvTypeVp8   NewMV4x4[16];       /* New inter MVs (un-packed form) */
        };
        struct {
            MvTypeVp8   NewMV16;            /* VP8_MV_NEW vector placeholder */
            CoeffsType  Y2mbCoeff[16];      /* Y2 Mb coefficients  */
        };
    };
    CoeffsType mbCoeff[24][16];             /* YUV Mb coefficients */
} mfxMbCodeVP8;
typedef mfxMbCodeVP8 MbCodeVp8;

typedef mfxU32 U32;
typedef mfxU16 U16;
typedef mfxU8  U8;
typedef mfxI32 I32;
typedef mfxI16 I16;
typedef mfxI8  I8;
typedef U8     PixType;

typedef struct {
    short   x,y;    // integer pair
}mfxI16PAIR;
typedef mfxI16PAIR I16PAIR;

//typedef mfxU32 CoeffsType;// aya: undefined in my version.


typedef unsigned char Vp8Prob;

enum {
    VP8_INTRA_FRAME = 0,
    VP8_LAST_FRAME,
    VP8_GOLDEN_FRAME,
    VP8_ALTREF_FRAME,
    VP8_NUM_OF_REF_FRAMES,
    VP8_ORIGINAL_FRAME = VP8_NUM_OF_REF_FRAMES,
    VP8_PREILF_FRAME,
    VP8_NUM_OF_USED_FRAMES
};

enum
{
    VP8_MB_DC_PRED = 0, /* predict DC using row above and column to the left */
    VP8_MB_V_PRED,      /* predict rows using row above */
    VP8_MB_H_PRED,      /* predict columns using column to the left */
    VP8_MB_TM_PRED,     /* propagate second differences a la "true motion" */
    VP8_MB_B_PRED,      /* each Y block is independently predicted */
    VP8_NUM_MB_MODES_UV = VP8_MB_B_PRED, /* first four modes apply to chroma */
    VP8_NUM_MB_MODES_Y  /* all modes apply to luma */
};

enum
{
    VP8_MV_NEAREST,                        /* use "nearest" motion vector for entire MB */
    VP8_MV_NEAR,                           /* use "next nearest" */
    VP8_MV_ZERO,                           /* use zero */
    VP8_MV_NEW,                            /* use explicit offset from implicit */
    VP8_MV_SPLIT,                          /* use multiple motion vectors */
    VP8_NUM_MV_MODES
};

enum
{
    VP8_B_DC_PRED = 0, /* predict DC using row above and column to the left */
    VP8_B_VE_PRED,     /* predict rows using row above */
    VP8_B_HE_PRED,     /* predict columns using column to the left */
    VP8_B_TM_PRED,     /* propagate second differences a la "true motion" */
    VP8_B_LD_PRED,     /* southwest (left and down) 45 degree diagonal prediction */
    VP8_B_RD_PRED,     /* southeast (right and down) "" */
    VP8_B_VR_PRED,     /* SSE (vertical right) diagonal prediction */
    VP8_B_VL_PRED,     /* SSW (vertical left) "" */
    VP8_B_HD_PRED,     /* ESE (horizontal down) "" */
    VP8_B_HU_PRED,     /* ENE (horizontal up) "" */
    VP8_NUM_INTRA_BLOCK_MODES
};

enum
{
    VP8_B_MV_LEFT,
    VP8_B_MV_ABOVE,
    VP8_B_MV_ZERO,
    VP8_B_MV_NEW,
    VP8_NUM_B_MV_MODES
};

enum
{
    VP8_MV_TOP_BOTTOM = 0, /* two pieces {0...7} and {8...15} */
    VP8_MV_LEFT_RIGHT,     /* {0,1,4,5,8,9,12,13} and {2,3,6,7,10,11,14,15} */
    VP8_MV_QUARTERS,       /* {0,1,4,5}, {2,3,6,7}, {8,9,12,13}, {10,11,14,15} */
    VP8_MV_16,             /* every subblock gets its own vector {0} ... {15} */
    VP8_MV_NUM_PARTITIONS
};

enum
{
    VP8_COEFF_PLANE_Y_AFTER_Y2 = 0,
    VP8_COEFF_PLANE_Y2,
    VP8_COEFF_PLANE_UV,
    VP8_COEFF_PLANE_Y,
    VP8_NUM_COEFF_PLANES
};

enum
{
  VP8_MV_IS_SHORT  = 0,
  VP8_MV_SIGN,
  VP8_MV_SHORT,
  VP8_MV_LONG      = 9,
  VP8_MV_LONG_BITS = 10,
  VP8_NUM_MV_PROBS = VP8_MV_LONG + VP8_MV_LONG_BITS
};

enum {
    VP8_DCTVAL_0    = 0,
    VP8_DCTVAL_1,
    VP8_DCTVAL_2,
    VP8_DCTVAL_3,
    VP8_DCTVAL_4,
    VP8_DCTCAT_1,
    VP8_DCTCAT_2,
    VP8_DCTCAT_3,
    VP8_DCTCAT_4,
    VP8_DCTCAT_5,
    VP8_DCTCAT_6,
    VP8_DCTEOB,
    VP8_NUM_DCT_TOKENS
};

enum {
    VP8_Y_AC,
    VP8_Y_DC,
    VP8_Y2_AC,
    VP8_Y2_DC,
    VP8_UV_AC,
    VP8_UV_DC,
    VP8_NUM_COEFF_QUANTIZERS
};

#define VP8_NUM_COEFF_BANDS             8
#define VP8_NUM_LOCAL_COMPLEXITIES      3
#define VP8_NUM_COEFF_NODES             11
#define VP8_PADDING                     32
#define VP8_MAX_NUM_OF_PARTITIONS       8
#define VP8_NUM_OF_MB_FEATURES          2
#define VP8_MAX_NUM_OF_SEGMENTS         4
#define VP8_NUM_OF_SEGMENT_TREE_PROBS   3
#define VP8_NUM_OF_MODE_LF_DELTAS       4
#define VP8_MIN_QP                      0
#define VP8_MAX_QP                      127
#define VP8_MAX_LF_LEVEL                63

extern const unsigned char vp8_ClampTbl[768];

#define vp8_clamp8s(x) vp8_ClampTbl[256 + 128+ (x)] - 128
#define vp8_clamp8u(x) vp8_ClampTbl[256 + (x)]
#define VP8_LIMIT(a,l,t) ((a)<(l)?(l):((a)>(t))?(t):(a))

#endif //__VP8_COMMON_DEFS_H__