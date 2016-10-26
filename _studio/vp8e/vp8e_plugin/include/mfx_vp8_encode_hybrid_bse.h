//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_VP8_ENCODE_HYBRID_BSE_H_
#define _MFX_VP8_ENCODE_HYBRID_BSE_H_

#include "mfx_common.h"
#include "mfx_vp8_encode_utils_hw.h"
#include "mfx_vp8_encode_utils_hybrid_hw.h"

#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE_HW) && defined(MFX_VA)

#if defined (MFX_VA_LINUX)
#include <emmintrin.h>
#include <tmmintrin.h>
static inline int _BitScanReverse(unsigned long* index, unsigned long mask)
{
    *index =  63 - __builtin_clzl(mask & 0xffffffff);
    return (mask & 0xffffffff) ? 1 : 0;
}
#endif

namespace MFX_VP8ENC
{
    typedef unsigned char    PixType;
    typedef short            CoeffsType;
    typedef unsigned char    Vp8Prob;
    typedef unsigned char    U8;
    typedef char             I8;
    typedef short            I16;
    typedef unsigned short   U16;
    typedef unsigned int     U32;
    typedef int              I32;
    typedef unsigned int     UL32;
    typedef int              L32;
    typedef float            F32;
    typedef double           F64;
    typedef unsigned long long U64;
    typedef long long        I64;

    typedef union { /* MV type */
        struct {
            I16  mvx;
            I16  mvy;
        } MV;
        I32 s32;
    } MvTypeVp8;

    typedef struct { /* HLD macroblock data object */
        U32        data[4];
        CoeffsType mbCoeff[25][16];
    } HLDMbDataVp8;

    typedef struct {
        MvTypeVp8 MV[16];
    } HLDMvDataVp8;

    typedef struct { /* Truncated macroblock codes for VP8 BSE */
        union {
            U32     DWord0;
            struct {
                U32  RefFrameMode       : 2;    /* 0 - intra, 1 - last, 2 - gold, 3 - altref */
                U32  SegmentId          : 2;    /* Segment Id */
                U32  CoeffSkip          : 1;    /* Is all coefficient are zero */
                U32  MbXcnt             : 10;   /* MB column */
                U32  MbYcnt             : 10;   /* MB row */
                U32  MbMode             : 3;    /* Intra / Inter MB mode 0 - 4, depends on RefFrameMode */
                U32  MbSubmode          : 2;    /* Intra UV or Inter partition, depends on RefFrameMode */
                U32  Y2AboveNotZero     : 1;    /* Non-zero Y2 block for this or next above Mb with Y2 */
                U32  Y2LeftNotZero      : 1;    /* Non-zero Y2 block for this or next left Mb with Y2 */
            };
        };
        U32 nonZeroCoeff;                       /* Mask of non-zero coefficients in 4x4 sub-blocks */
        union {
            U32     DWord1;
            struct {
                U16     Y4x4Modes0;             /* IntraY4x4, modes for subblocks 0 - 3 (4 x 4bits)*/
                U16     Y4x4Modes1;             /* IntraY4x4, modes for subblocks 4 - 7 (4 x 4bits)*/
            };
            union {
                U32     InterSplitModes;        /* Inter split subblock modes (16 x 2 bits) */
            };
        };
        union {
            struct {
                U16     Y4x4Modes2;             /* IntraY4x4, modes for subblocks 8 - 11 (4 x 4bits)*/
                U16     Y4x4Modes3;             /* IntraY4x4, modes for subblocks 12 - 15 (4 x 4bits)*/
            };
            struct {
                MvTypeVp8   NewMV4x4[16];       /* New inter MVs (un-packed form) */
            };
            struct {
                MvTypeVp8   NewMV16;            /* VP8_MV_NEW vector placeholder */
            };
        };
    } TrMbCodeVp8;

    enum {
        VP8_INTRA_FRAME = 0,
        VP8_LAST_FRAME,
        VP8_GOLDEN_FRAME,
        VP8_ALTREF_FRAME,
        VP8_NUM_OF_REF_FRAMES,
        VP8_ORIGINAL_FRAME = VP8_NUM_OF_REF_FRAMES,
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
}

namespace MFX_VP8ENC
{
    extern const unsigned char vp8_ClampTbl[768];

#define vp8_clamp8s(x) vp8_ClampTbl[256 + 128+ (x)] - 128
#define vp8_clamp8u(x) vp8_ClampTbl[256 + (x)]
#define VP8_LIMIT(a,l,t) ((a)<(l)?(l):((a)>(t))?(t):(a))

    const Vp8Prob segTreeProbsC[VP8_NUM_OF_SEGMENT_TREE_PROBS] = {255, 255, 255};
    const Vp8Prob vp8_kf_mb_mode_y_probs[VP8_NUM_MB_MODES_Y - 1]   = {145, 156, 163, 128};
    const Vp8Prob vp8_kf_mb_mode_uv_probs[VP8_NUM_MB_MODES_UV - 1] = {142, 114, 183};
    const Vp8Prob vp8_kf_block_mode_probs[VP8_NUM_INTRA_BLOCK_MODES][VP8_NUM_INTRA_BLOCK_MODES][VP8_NUM_INTRA_BLOCK_MODES-1] = 
    {
        {
            { 231, 120, 48, 89, 115, 113, 120, 152, 112},
            { 175, 69, 143, 80, 85, 82, 72, 155, 103},
            { 56, 58, 10, 171, 218, 189, 17, 13, 152},
            { 152, 179, 64, 126, 170, 118, 46, 70, 95},
            { 144, 71, 10, 38, 171, 213, 144, 34, 26},
            { 114, 26, 17, 163, 44, 195, 21, 10, 173},
            { 121, 24, 80, 195, 26, 62, 44, 64, 85},
            { 170, 46, 55, 19, 136, 160, 33, 206, 71},
            { 63, 20, 8, 114, 114, 208, 12, 9, 226},
            { 81, 40, 11, 96, 182, 84, 29, 16, 36}
        },
        {
            { 88, 88, 147, 150, 42, 46, 45, 196, 205},
            { 39, 53, 200, 87, 26, 21, 43, 232, 171},
            { 56, 34, 51, 104, 114, 102, 29, 93, 77},
            { 43, 97, 183, 117, 85, 38, 35, 179, 61},
            { 107, 54, 32, 26, 51, 1, 81, 43, 31},
            { 39, 28, 85, 171, 58, 165, 90, 98, 64},
            { 34, 22, 116, 206, 23, 34, 43, 166, 73},
            { 68, 25, 106, 22, 64, 171, 36, 225, 114},
            { 34, 19, 21, 102, 132, 188, 16, 76, 124},
            { 62, 18, 78, 95, 85, 57, 50, 48, 51}
        },
        {
            { 193, 101, 35, 159, 215, 111, 89, 46, 111},
            { 112, 113, 77, 85, 179, 255, 38, 120, 114},
            { 40, 42, 1, 196, 245, 209, 10, 25, 109},
            { 60, 148, 31, 172, 219, 228, 21, 18, 111},
            { 100, 80, 8, 43, 154, 1, 51, 26, 71},
            { 88, 43, 29, 140, 166, 213, 37, 43, 154},
            { 61, 63, 30, 155, 67, 45, 68, 1, 209},
            { 142, 78, 78, 16, 255, 128, 34, 197, 171},
            { 41, 40, 5, 102, 211, 183, 4, 1, 221},
            { 51, 50, 17, 168, 209, 192, 23, 25, 82}
        },
        {
            { 134, 183, 89, 137, 98, 101, 106, 165, 148},
            { 66, 102, 167, 99, 74, 62, 40, 234, 128},
            { 41, 53, 9, 178, 241, 141, 26, 8, 107},
            { 72, 187, 100, 130, 157, 111, 32, 75, 80},
            { 104, 79, 12, 27, 217, 255, 87, 17, 7},
            { 74, 43, 26, 146, 73, 166, 49, 23, 157},
            { 65, 38, 105, 160, 51, 52, 31, 115, 128},
            { 87, 68, 71, 44, 114, 51, 15, 186, 23},
            { 47, 41, 14, 110, 182, 183, 21, 17, 194},
            { 66, 45, 25, 102, 197, 189, 23, 18, 22}
        },
        {
            { 125, 98, 42, 88, 104, 85, 117, 175, 82},
            { 75, 79, 123, 47, 51, 128, 81, 171, 1},
            { 57, 17, 5, 71, 102, 57, 53, 41, 49},
            { 95, 84, 53, 89, 128, 100, 113, 101, 45},
            { 115, 21, 2, 10, 102, 255, 166, 23, 6},
            { 38, 33, 13, 121, 57, 73, 26, 1, 85},
            { 41, 10, 67, 138, 77, 110, 90, 47, 114},
            { 101, 29, 16, 10, 85, 128, 101, 196, 26},
            { 57, 18, 10, 102, 102, 213, 34, 20, 43},
            { 117, 20, 15, 36, 163, 128, 68, 1, 26}
        },
        {
            { 138, 31, 36, 171, 27, 166, 38, 44, 229},
            { 63, 59, 90, 180, 59, 166, 93, 73, 154},
            { 40, 40, 21, 116, 143, 209, 34, 39, 175},
            { 67, 87, 58, 169, 82, 115, 26, 59, 179},
            { 57, 46, 22, 24, 128, 1, 54, 17, 37},
            { 47, 15, 16, 183, 34, 223, 49, 45, 183},
            { 46, 17, 33, 183, 6, 98, 15, 32, 183},
            { 65, 32, 73, 115, 28, 128, 23, 128, 205},
            { 40, 3, 9, 115, 51, 192, 18, 6, 223},
            { 87, 37, 9, 115, 59, 77, 64, 21, 47}
        },
        {
            { 104, 55, 44, 218, 9, 54, 53, 130, 226},
            { 54, 57, 112, 184, 5, 41, 38, 166, 213},
            { 30, 34, 26, 133, 152, 116, 10, 32, 134},
            { 64, 90, 70, 205, 40, 41, 23, 26, 57},
            { 75, 32, 12, 51, 192, 255, 160, 43, 51},
            { 39, 19, 53, 221, 26, 114, 32, 73, 255},
            { 31, 9, 65, 234, 2, 15, 1, 118, 73},
            { 88, 31, 35, 67, 102, 85, 55, 186, 85},
            { 56, 21, 23, 111, 59, 205, 45, 37, 192},
            { 55, 38, 70, 124, 73, 102, 1, 34, 98}
        },
        {
            { 102, 61, 71, 37, 34, 53, 31, 243, 192},
            { 68, 45, 128, 34, 1, 47, 11, 245, 171},
            { 62, 17, 19, 70, 146, 85, 55, 62, 70},
            { 69, 60, 71, 38, 73, 119, 28, 222, 37},
            { 75, 15, 9, 9, 64, 255, 184, 119, 16},
            { 37, 43, 37, 154, 100, 163, 85, 160, 1},
            { 63, 9, 92, 136, 28, 64, 32, 201, 85},
            { 86, 6, 28, 5, 64, 255, 25, 248, 1},
            { 56, 8, 17, 132, 137, 255, 55, 116, 128},
            { 58, 15, 20, 82, 135, 57, 26, 121, 40}
        },
        {
            { 164, 50, 31, 137, 154, 133, 25, 35, 218},
            { 86, 40, 64, 135, 148, 224, 45, 183, 128},
            { 22, 26, 17, 131, 240, 154, 14, 1, 209},
            { 51, 103, 44, 131, 131, 123, 31, 6, 158},
            { 83, 12, 13, 54, 192, 255, 68, 47, 28},
            { 45, 16, 21, 91, 64, 222, 7, 1, 197},
            { 56, 21, 39, 155, 60, 138, 23, 102, 213},
            { 85, 26, 85, 85, 128, 128, 32, 146, 171},
            { 18, 11, 7, 63, 144, 171, 4, 4, 246},
            { 35, 27, 10, 146, 174, 171, 12, 26, 128}
        },
        {
            { 190, 80, 35, 99, 180, 80, 126, 54, 45},
            { 101, 75, 128, 139, 118, 146, 116, 128, 85},
            { 56, 41, 15, 176, 236, 85, 37, 9, 62},
            { 85, 126, 47, 87, 176, 51, 41, 20, 32},
            { 146, 36, 19, 30, 171, 255, 97, 27, 20},
            { 71, 30, 17, 119, 118, 255, 17, 18, 138},
            { 101, 38, 60, 138, 55, 70, 43, 26, 142},
            { 138, 45, 61, 62, 219, 1, 81, 188, 64},
            { 32, 41, 20, 117, 151, 142, 20, 21, 163},
            { 112, 19, 12, 61, 195, 128, 48, 4, 24}
        }
    };

    const Vp8Prob vp8_mb_mode_y_probs[VP8_NUM_MB_MODES_Y - 1] = {112, 86, 140, 37};
    const Vp8Prob vp8_mb_mode_uv_probs[VP8_NUM_MB_MODES_UV - 1] = {162, 101, 204};
    const Vp8Prob vp8_block_mode_probs[VP8_NUM_INTRA_BLOCK_MODES - 1] = { 120, 90, 79, 133, 87, 85, 80, 111, 151 };

    const U16 mbmodecost[VP8_MB_B_PRED+1+VP8_NUM_MV_MODES] = {657,    869,    915,    917,    208,    0,      0,      0,      0,      0}; 

    const Vp8Prob vp8_default_coeff_probs[VP8_NUM_COEFF_PLANES][VP8_NUM_COEFF_BANDS][VP8_NUM_LOCAL_COMPLEXITIES][VP8_NUM_COEFF_NODES] =
    {
        {
            {
                { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
                { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
                { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
            },
            {
                { 253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128},
                { 189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128},
                { 106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128}
            },
            {
                { 1, 98, 248, 255, 236, 226, 255, 255, 128, 128, 128},
                { 181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128},
                { 78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128}
            },
            {
                { 1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128},
                { 184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128},
                { 77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128}
            },
            {
                { 1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128},
                { 170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128},
                { 37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128}
            },
            {
                { 1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128},
                { 207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128},
                { 102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128}
            },
            {
                { 1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128},
                { 177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128},
                { 80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128}
            },
            {
                { 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
                { 246, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
                { 255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
            }
        },
        {
            {
                { 198, 35, 237, 223, 193, 187, 162, 160, 145, 155, 62},
                { 131, 45, 198, 221, 172, 176, 220, 157, 252, 221, 1},
                { 68, 47, 146, 208, 149, 167, 221, 162, 255, 223, 128}
            },
            {
                { 1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128},
                { 184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128},
                { 81, 99, 181, 242, 176, 190, 249, 202, 255, 255, 128}
            },
            {
                { 1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128},
                { 99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128},
                { 23, 91, 163, 242, 170, 187, 247, 210, 255, 255, 128}
            },
            {
                { 1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128},
                { 109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128},
                { 44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128}
            },
            {
                { 1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128},
                { 94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128},
                { 22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128}
            },
            {
                { 1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128},
                { 124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128},
                { 35, 77, 181, 251, 193, 211, 255, 205, 128, 128, 128}
            },
            {
                { 1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128},
                { 121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128},
                { 45, 99, 188, 251, 195, 217, 255, 224, 128, 128, 128}
            },
            {
                { 1, 1, 251, 255, 213, 255, 128, 128, 128, 128, 128},
                { 203, 1, 248, 255, 255, 128, 128, 128, 128, 128, 128},
                { 137, 1, 177, 255, 224, 255, 128, 128, 128, 128, 128}
            }
        },
        {
            {
                { 253, 9, 248, 251, 207, 208, 255, 192, 128, 128, 128},
                { 175, 13, 224, 243, 193, 185, 249, 198, 255, 255, 128},
                { 73, 17, 171, 221, 161, 179, 236, 167, 255, 234, 128}
            },
            {
                { 1, 95, 247, 253, 212, 183, 255, 255, 128, 128, 128},
                { 239, 90, 244, 250, 211, 209, 255, 255, 128, 128, 128},
                { 155, 77, 195, 248, 188, 195, 255, 255, 128, 128, 128}
            },
            {
                { 1, 24, 239, 251, 218, 219, 255, 205, 128, 128, 128},
                { 201, 51, 219, 255, 196, 186, 128, 128, 128, 128, 128},
                { 69, 46, 190, 239, 201, 218, 255, 228, 128, 128, 128}
            },
            {
                { 1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128},
                { 223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128},
                { 141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128}
            },
            {
                { 1, 16, 248, 255, 255, 128, 128, 128, 128, 128, 128},
                { 190, 36, 230, 255, 236, 255, 128, 128, 128, 128, 128},
                { 149, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128}
            },
            {
                { 1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128},
                { 247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128},
                { 240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128}
            },
            {
                { 1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128},
                { 213, 62, 250, 255, 255, 128, 128, 128, 128, 128, 128},
                { 55, 93, 255, 128, 128, 128, 128, 128, 128, 128, 128}
            },
            {
                { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
                { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
                { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
            }
        },
        {
            {
                { 202, 24, 213, 235, 186, 191, 220, 160, 240, 175, 255},
                { 126, 38, 182, 232, 169, 184, 228, 174, 255, 187, 128},
                { 61, 46, 138, 219, 151, 178, 240, 170, 255, 216, 128}
            },
            {
                { 1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128},
                { 166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128},
                { 39, 77, 162, 232, 172, 180, 245, 178, 255, 255, 128}
            },
            {
                { 1, 52, 220, 246, 198, 199, 249, 220, 255, 255, 128},
                { 124, 74, 191, 243, 183, 193, 250, 221, 255, 255, 128},
                { 24, 71, 130, 219, 154, 170, 243, 182, 255, 255, 128}
            },
            {
                { 1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128},
                { 149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128},
                { 28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128}
            },
            {
                { 1, 81, 230, 252, 204, 203, 255, 192, 128, 128, 128},
                { 123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128},
                { 20, 95, 153, 243, 164, 173, 255, 203, 128, 128, 128}
            },
            {
                { 1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128},
                { 168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128},
                { 47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128}
            },
            {
                { 1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128},
                { 141, 84, 213, 252, 201, 202, 255, 219, 128, 128, 128},
                { 42, 80, 160, 240, 162, 185, 255, 205, 128, 128, 128}
            },
            {
                { 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
                { 244, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
                { 238, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128}
            }
        }
    };

    const Vp8Prob vp8_coeff_update_probs[VP8_NUM_COEFF_PLANES][VP8_NUM_COEFF_BANDS][VP8_NUM_LOCAL_COMPLEXITIES][VP8_NUM_COEFF_NODES] =
    {
        {
            {
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255},
                { 249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255},
                { 234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255},
                { 250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255},
                { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            }
        },
        {
            {
                { 217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255},
                { 234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255}
            },
            {
                { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255},
                { 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            }
        },
        {
            {
                { 186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255},
                { 234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255},
                { 251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255}
            },
            {
                { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            }
        },
        {
            {
                { 248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255},
                { 248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255},
                { 246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255},
                { 252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255},
                { 248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255},
                { 253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255},
                { 252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255},
                { 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            },
            {
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
                { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
            }
        }
    };

    const Vp8Prob vp8_mv_update_probs[2][VP8_NUM_MV_PROBS] =
    {
        { 237, 246, 253, 253, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 250, 250, 252, 254, 254 },
        { 231, 243, 245, 253, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 251, 251, 254, 254, 254
        }
    };

    const Vp8Prob vp8_default_mv_contexts[2][VP8_NUM_MV_PROBS] =
    {
        { 162, 128, 225, 146, 172, 147, 214, 39, 156, 128, 129, 132, 75, 145, 178, 206, 239, 254, 254 },
        { 164, 128, 204, 170, 119, 235, 140, 230, 228, 128, 130, 130, 74, 148, 180, 203, 236, 254, 254 }
    };

    const I8 vp8_mb_mode_y_tree[2*(VP8_NUM_MB_MODES_Y - 1)] =
    {
        -VP8_MB_DC_PRED /*0*/, 2, 4, 6, -VP8_MB_V_PRED /*100*/, -VP8_MB_H_PRED /*101*/, -VP8_MB_TM_PRED /*110*/, -VP8_MB_B_PRED /*111*/
    };
    const U8 vp8_mb_mode_y_map[VP8_NUM_MB_MODES_Y] = {0x00, 0x01, 0x05, 0x03, 0x07};

    const I8 vp8_kf_mb_mode_y_tree[2*(VP8_NUM_MB_MODES_Y - 1)] =
    {
        -VP8_MB_B_PRED /*0*/, 2, 4, 6, -VP8_MB_DC_PRED /*100*/, -VP8_MB_V_PRED /*101*/, -VP8_MB_H_PRED /*110*/, -VP8_MB_TM_PRED /*111*/
    };
    const U8 vp8_kf_mb_mode_y_map[VP8_NUM_MB_MODES_Y] = {0x01, 0x05, 0x03, 0x07, 0x00};

    const I8 vp8_mb_mode_uv_tree[2*(VP8_NUM_MB_MODES_UV - 1)] =
    {
        -VP8_MB_DC_PRED /*0*/, 2, -VP8_MB_V_PRED /*10*/, 4, -VP8_MB_H_PRED /*110*/, -VP8_MB_TM_PRED /*111*/
    };
    const U8 vp8_mb_mode_uv_map[VP8_NUM_MB_MODES_UV] = {0x00, 0x01, 0x03, 0x07};

    const I8 vp8_block_mode_tree[2*(VP8_NUM_INTRA_BLOCK_MODES - 1)] =
    {
        -VP8_B_DC_PRED /*0*/, 2, -VP8_B_TM_PRED /*10*/, 4, -VP8_B_VE_PRED /*110*/, 6, 8, 12,
        -VP8_B_HE_PRED /*11100*/, 10, -VP8_B_RD_PRED /*111010*/, -VP8_B_VR_PRED /*111011*/, -VP8_B_LD_PRED /*11110*/, 14,
        -VP8_B_VL_PRED /*111110*/, 16, -VP8_B_HD_PRED /*1111110*/, -VP8_B_HU_PRED /*1111111*/
    };
    const U8 vp8_block_mode_map[VP8_NUM_INTRA_BLOCK_MODES] = {0x00, 0x03, 0x07, 0x01, 0x0F, 0x17, 0x37, 0x1F, 0x3F, 0x7F};

    const U8 vp8_coefBands[16] = { 0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7 };
    const U8 vp8_zigzag_scan[16] = { 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 };

    const I8 vp8_token_tree[2*(VP8_NUM_DCT_TOKENS - 1)] =
    {
        -VP8_DCTEOB /*0*/, 2, -VP8_DCTVAL_0 /*10*/, 4, -VP8_DCTVAL_1 /*110*/, 6, 8, 12, -VP8_DCTVAL_2 /*11100*/, 10,
        -VP8_DCTVAL_3 /*111010*/, -VP8_DCTVAL_4 /*111011*/, 14, 16, -VP8_DCTCAT_1 /*111100*/, -VP8_DCTCAT_2 /*111101*/, 18, 20,
        -VP8_DCTCAT_3 /*1111100*/, -VP8_DCTCAT_4 /*1111101*/, -VP8_DCTCAT_5 /*1111110*/, -VP8_DCTCAT_6 /*1111111*/
    };
    const U8 vp8_token_map[VP8_NUM_DCT_TOKENS] = {0x01, 0x03, 0x07, 0x17, 0x37, 0x0F, 0x2F, 0x1F, 0x5f, 0x3F, 0x7F, 0x00};

    const Vp8Prob vp8_dctExtra_prob[6][11] = 
    {
        { 159,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0},
        { 165, 145,   0,   0,   0,   0,   0,   0,   0,   0,   0},
        { 173, 148, 140,   0,   0,   0,   0,   0,   0,   0,   0},
        { 176, 155, 140, 135,   0,   0,   0,   0,   0,   0,   0},
        { 180, 157, 141, 134, 130,   0,   0,   0,   0,   0,   0},
        { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129}
    };

    const I8 vp8_mvmode_tree[2*(VP8_NUM_MV_MODES-1)] =
    {
        -VP8_MV_ZERO /*0*/, 2, -VP8_MV_NEAREST /*10*/, 4, -VP8_MV_NEAR/*110*/, 6, -VP8_MV_NEW/*1110*/, -VP8_MV_SPLIT/*1111*/
    };
    const U8 vp8_mvmode_map[VP8_NUM_MV_MODES] = {0x01, 0x03, 0x00, 0x07, 0x0F};
    const Vp8Prob vp8_mvmode_contexts[6][4] =
    {
        {  7,    1,   1, 143},
        { 14,   18,  14, 107},
        { 135,  64,  57,  68},
        {  60,  56, 128,  65},
        { 159, 134, 128,  34},
        { 234, 188, 128,  28}
    };

    const I8 vp8_mv_partitions_tree[2*(VP8_MV_NUM_PARTITIONS - 1)] = 
    {
        -VP8_MV_16 /*0*/, 2, -VP8_MV_QUARTERS /*10*/, 4, -VP8_MV_TOP_BOTTOM /*110*/, -VP8_MV_LEFT_RIGHT /*111*/
    };
    const U8 vp8_mv_partitions_map[VP8_MV_NUM_PARTITIONS] = { 0x03, 0x07, 0x01, 0x00 };
    const Vp8Prob vp8_mv_partitions_probs[3] = { 110, 111, 150 };

    const I8 vp8_block_mv_tree[2*(VP8_NUM_B_MV_MODES - 1)] =
    {
        -VP8_B_MV_LEFT /*0*/, 2, -VP8_B_MV_ABOVE /*10*/, 4, -VP8_B_MV_ZERO /*110*/, -VP8_B_MV_NEW /*111*/
    };
    const U8 vp8_block_mv_map[VP8_NUM_B_MV_MODES] = { 0x00, 0x01, 0x03, 0x07 };
    const U8 vp8_block_mv_probs[5][VP8_NUM_B_MV_MODES - 1] =
    {
        {147, 136, 18},
        {106, 145,  1},
        {179, 121,  1},
        {223,   1, 34},
        {208,   1,  1}
    };

    const I8 vp8_short_mv_tree[2*(VP8_MV_LONG - VP8_MV_SHORT)] =
    {
        2, 8, 4, 6, 0 /*000*/, -1 /*001*/, -2 /*010*/, -3 /*011*/, 10, 12, -4 /*100*/, -5 /*101*/, -6 /*110*/, -7 /*111*/
    };
    const U8 vp8_short_mv_map[VP8_MV_LONG - VP8_MV_SHORT + 1] = { 0x00, 0x04, 0x02, 0x06, 0x01, 0x05, 0x03, 0x07 };

    const Vp8Prob vp8_block_mv_default_prob[VP8_NUM_B_MV_MODES-1] = { 180, 162, 25};

    typedef struct _vp8CPBECState {
        U32   a;          // Lower interval value
        U32   l;          // Interval length
        I32   cnt;        // Decimal point position
        U8   *pData;      // Current position in bitstream
    } vp8BoolEncState;

    inline void vp8EncodeInit(vp8BoolEncState *e, U8 *ptr)
    {
        e->a = 0; e->l=255; e->cnt = -24; e->pData = ptr; *e->pData = 0;
    }

    VPX_FORCEINLINE void vp8EncodeBit(vp8BoolEncState *e, U8 b, Vp8Prob p)
    {
        U32 l   = e->l;
        I32 a   = e->a;
        I32 cnt = e->cnt;
        U32 spl = 1 + (((l - 1) * p) >> 8);

        if(b) { a += spl; l -= spl; } else { l = spl; }

        if(l<128) { /* Renormalization required */
            unsigned long shf;
            _BitScanReverse( &shf, l );
            shf = 7 - shf;
            l <<= shf; cnt += shf;
            if(cnt>=0) {
                U32 sb = -e->cnt;
                U32 v  = a >> (24 - sb);
                if (v&0x0100) {
                    U8 *pD = e->pData-1;
                    while(*pD==0xFF) {*pD = 0; pD--;};
                    (*pD)++;
                }
                *e->pData++ = (U8)v;
                a <<= sb; a &= 0xFFFFFF; shf = cnt; cnt -=8;
            }
            a <<= shf; e->cnt = cnt;
        }
        e->l = l; e->a = a;
    }

    VPX_FORCEINLINE void vp8EncodeBitEP(vp8BoolEncState *e, U8 b)
    {
        U32 l   = e->l;
        I32 a   = e->a;
        I32 cnt = e->cnt;
        U32 spl = 1 + ((l - 1) >> 1);

        if(b) { a += spl; l -= spl; } else { l = spl; }

        if(l<128) { /* Renormalization required */
            unsigned long shf;
            _BitScanReverse( &shf, l );
            shf = 7 - shf;
            l <<= shf; cnt += shf;
            if(cnt>=0) {
                U32 sb = -e->cnt;
                U32 v  = a >> (24 - sb);
                if (v&0x0100) {
                    U8 *pD = e->pData-1;
                    while(*pD==0xFF) {*pD = 0; pD--;};
                    (*pD)++;
                }
                *e->pData++ = (U8)v;
                a <<= sb; a &= 0xFFFFFF; shf = cnt; cnt -=8;
            }
            a <<= shf; e->cnt = cnt;
        }
        e->l = l; e->a = a;
    }

    VPX_FORCEINLINE void vp8EncodeFlush(vp8BoolEncState *e)
    {
        for (I32 i = 0; i < 32; i++) vp8EncodeBitEP(e, 0);
    }

    VPX_FORCEINLINE void vp8EncodeValue(vp8BoolEncState *e, U32 v, U32 bc)
    {
        for (I32 i = bc - 1; i >= 0; i--) vp8EncodeBitEP(e, (v >> i)&0x01);
    }

    VPX_FORCEINLINE void vp8EncodeValueIfNE(vp8BoolEncState *e, I32 v, I32 v1, U32 bc, bool sign, U8 prob)
    {
        if(v==v1) {
            vp8EncodeBit(e, 0, prob);
        } else {
            vp8EncodeBit(e, 1, prob);
            vp8EncodeValue(e, (v>0)?v:-v, bc);
            if(sign) vp8EncodeBitEP(e, (v>0)?0:1);
        }
    };

    VPX_FORCEINLINE void vp8EncodeDeltaIfNE(vp8BoolEncState *e, I32 v, I32 v1, U32 bc)
    {
        if(v==v1) {
            vp8EncodeBitEP(e, 0);
        } else {
            vp8EncodeBitEP(e, 1);
            v -= v1;
            if(v>0) {
                vp8EncodeValue(e, v, bc);
                vp8EncodeBitEP(e, 0);
            } else {
                vp8EncodeValue(e, -v, bc);
                vp8EncodeBitEP(e, 1);
            }
        }
    };

    VPX_FORCEINLINE void vp8EncodeTreeToken(vp8BoolEncState *e, I32 v, const I8 *tree, const U8 *prob, I8 start)
    {
        I8 i = start;
        U8 bit;

        do {
            bit = v & 1; v>>=1;
            vp8EncodeBit(e, bit, prob[i>>1]);
            i = tree[i+bit];
        } while(i>0);
    };

    VPX_FORCEINLINE void vp8EncodeTokenValue(vp8BoolEncState *e, const U8 *prob, U8 val)
    {
        switch (val) {
        case 0:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 0, prob[1]); break;
        case 1:
            vp8EncodeBit(e, 0, prob[1]); break;
        case 2:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 0, prob[2]); break;
        case 3:
            vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 0, prob[2]); break;
        case 4:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 1, prob[1]);
            vp8EncodeBit(e, 1, prob[2]); vp8EncodeBit(e, 0, prob[3]); vp8EncodeBit(e, 0, prob[4]); break;
        case 5:
            vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]);
            vp8EncodeBit(e, 0, prob[3]); vp8EncodeBit(e, 0, prob[4]); break;
        case 6:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]);
            vp8EncodeBit(e, 0, prob[3]); vp8EncodeBit(e, 1, prob[4]); vp8EncodeBit(e, 0, prob[5]); break;
        case 7:
            vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]); vp8EncodeBit(e, 0, prob[3]);
            vp8EncodeBit(e, 1, prob[4]); vp8EncodeBit(e, 0, prob[5]); break;
        case 8:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]);
            vp8EncodeBit(e, 0, prob[3]); vp8EncodeBit(e, 1, prob[4]); vp8EncodeBit(e, 1, prob[5]); break;
        case 9:
            vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]); vp8EncodeBit(e, 0, prob[3]);
            vp8EncodeBit(e, 1, prob[4]); vp8EncodeBit(e, 1, prob[5]); break;
        case 10:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]);
            vp8EncodeBit(e, 1, prob[3]); vp8EncodeBit(e, 0, prob[6]); vp8EncodeBit(e, 0, prob[7]); break;
        case 11:
            vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]); vp8EncodeBit(e, 1, prob[3]);
            vp8EncodeBit(e, 0, prob[6]); vp8EncodeBit(e, 0, prob[7]); break;
        case 12:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]);
            vp8EncodeBit(e, 1, prob[3]); vp8EncodeBit(e, 0, prob[6]); vp8EncodeBit(e, 1, prob[7]); break;
        case 13:
            vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]); vp8EncodeBit(e, 1, prob[3]);
            vp8EncodeBit(e, 0, prob[6]); vp8EncodeBit(e, 1, prob[7]); break;
        case 14:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]);
            vp8EncodeBit(e, 1, prob[3]); vp8EncodeBit(e, 1, prob[6]); vp8EncodeBit(e, 0, prob[8]); vp8EncodeBit(e, 0, prob[9]); break;
        case 15:
            vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]); vp8EncodeBit(e, 1, prob[3]);
            vp8EncodeBit(e, 1, prob[6]); vp8EncodeBit(e, 0, prob[8]); vp8EncodeBit(e, 0, prob[9]); break;
        case 16:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]);
            vp8EncodeBit(e, 1, prob[3]); vp8EncodeBit(e, 1, prob[6]); vp8EncodeBit(e, 0, prob[8]); vp8EncodeBit(e, 1, prob[9]); break;
        case 17:
            vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]); vp8EncodeBit(e, 1, prob[3]);
            vp8EncodeBit(e, 1, prob[6]); vp8EncodeBit(e, 0, prob[8]); vp8EncodeBit(e, 1, prob[9]); break;
        case 18:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]);
            vp8EncodeBit(e, 1, prob[3]); vp8EncodeBit(e, 1, prob[6]); vp8EncodeBit(e, 1, prob[8]); vp8EncodeBit(e, 0, prob[10]); break;
        case 19:
            vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]); vp8EncodeBit(e, 1, prob[3]);
            vp8EncodeBit(e, 1, prob[6]); vp8EncodeBit(e, 1, prob[8]); vp8EncodeBit(e, 0, prob[10]); break;
        case 20:
            vp8EncodeBit(e, 1, prob[0]); vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]);
            vp8EncodeBit(e, 1, prob[3]); vp8EncodeBit(e, 1, prob[6]); vp8EncodeBit(e, 1, prob[8]); vp8EncodeBit(e, 1, prob[10]); break;
        case 21:
            vp8EncodeBit(e, 1, prob[1]); vp8EncodeBit(e, 1, prob[2]); vp8EncodeBit(e, 1, prob[3]);
            vp8EncodeBit(e, 1, prob[6]); vp8EncodeBit(e, 1, prob[8]); vp8EncodeBit(e, 1, prob[10]); break;
        default:
            break;
        }
    }

    VPX_FORCEINLINE void vp8EncodeToken(vp8BoolEncState *e, U16 v, U8 lastZero, const U8 *prob)
    {
        U8      cat = (U8)(v >> 12);

        vp8EncodeTokenValue(e, prob, ((U8)cat<<1) | lastZero);

        switch(cat)
        {
        case 0:
            return;
        case 1:
        case 2:
        case 3:
        case 4:
            break;
        case 5:
            vp8EncodeBit(e, (v>>1)&1, 159);
            break;
        case 6:
            vp8EncodeBit(e, (v>>2)&1, 165);
            vp8EncodeBit(e, (v>>1)&1, 145);
            break;
        case 7:
            vp8EncodeBit(e, (v>>3)&1, 173);
            vp8EncodeBit(e, (v>>2)&1, 148);
            vp8EncodeBit(e, (v>>1)&1, 140);
            break;
        case 8:
            vp8EncodeBit(e, (v>>4)&1, 176);
            vp8EncodeBit(e, (v>>3)&1, 155);
            vp8EncodeBit(e, (v>>2)&1, 140);
            vp8EncodeBit(e, (v>>1)&1, 135);
            break;
        case 9:
            vp8EncodeBit(e, (v>>5)&1, 180);
            vp8EncodeBit(e, (v>>4)&1, 157);
            vp8EncodeBit(e, (v>>3)&1, 141);
            vp8EncodeBit(e, (v>>2)&1, 134);
            vp8EncodeBit(e, (v>>1)&1, 130);
            break;
        default:
            vp8EncodeBit(e,(v>>11)&1, 254);
            vp8EncodeBit(e,(v>>10)&1, 254);
            vp8EncodeBit(e, (v>>9)&1, 243);
            vp8EncodeBit(e, (v>>8)&1, 230);
            vp8EncodeBit(e, (v>>7)&1, 196);
            vp8EncodeBit(e, (v>>6)&1, 177);
            vp8EncodeBit(e, (v>>5)&1, 153);
            vp8EncodeBit(e, (v>>4)&1, 140);
            vp8EncodeBit(e, (v>>3)&1, 133);
            vp8EncodeBit(e, (v>>2)&1, 130);
            vp8EncodeBit(e, (v>>1)&1, 129);
            break;
        }
        vp8EncodeBitEP(e, v & 0x1);
    };

    VPX_FORCEINLINE void vp8EncodeEobToken(vp8BoolEncState *e, const U8 *prob)
    {
        vp8EncodeBit(e, 0, prob[0]);
    };

    VPX_FORCEINLINE void vp8EncodeMvComponent(vp8BoolEncState *e, I16 v, const U8 *prob)
    {
        U16     absv = (v<0)?-v:v;
        I32     i;

        if(absv<VP8_MV_LONG-1) {
            vp8EncodeBit(e, 0, prob[VP8_MV_IS_SHORT]);
            vp8EncodeTreeToken(e, vp8_short_mv_map[absv], vp8_short_mv_tree, prob+VP8_MV_SHORT, 0);
        } else {
            vp8EncodeBit(e, 1, prob[VP8_MV_IS_SHORT]);
            for(i=0;i<3;i++)
                vp8EncodeBit(e, (absv >> i) & 1, prob[VP8_MV_LONG + i]);
            for (i = VP8_MV_LONG_BITS - 1; i > 3; i--)
                vp8EncodeBit(e, (absv >> i) & 1, prob[VP8_MV_LONG + i]);
            if (absv & 0xFFF0)
                vp8EncodeBit(e, (absv >> 3) & 1, prob[VP8_MV_LONG + 3]);
        }
        if(absv!=0) 
            vp8EncodeBit(e, (absv!=v), prob[VP8_MV_SIGN]);
    };

    class Vp8EncoderParams
    {
    public:
        Vp8EncoderParams() {};
        ~Vp8EncoderParams() {};

        U32 SrcWidth;           // (I32) width of source media
        U32 SrcHeight;          // (I32) height of source media
        U32 FrameRateNom;
        U32 FrameRateDeNom;
        U32 NumFramesToEncode;  // (I32) number of frames to encode
        U32 fSizeInMBs;         // derived frame size in Mbs

        U32 EncMode;
        U32 NumP;               // Distance between Key frames
        U32 Version;            //
        U32 EnableSeg;          //
        U32 EnableSkip;         //
        U32 NumTokenPartitions; //
        U32 LoopFilterType;     //
        U32 SharpnessLevel;     //
        U32 KeyFrameQP[4];      // Fixed QP for K frame, all segments
        U32 PFrameQP[4];        // Fixed QP for P frame, all segments
        I32 CoeffTypeQPDelta[5];       // QP deltas for coefficient type [YDC, Y2AC Y2DC, UAC, UDC]
        U32 LoopFilterLevel[4]; // Loop filter level, all segments
        I32 LoopFilterRefTypeDelta[4];  // Loop filter level delta for ref types
        I32 LoopFilterMbModeDelta[4];   // Look filter level delta for MB types
    };

    class Vp8FrameParamsEnc
    {
    public:
        U32     StartCode;
        union {
            U32  FrameCtrl1;
            struct {
                U32 FrameType       : 1;
                U32 Version         : 3;
                U32 ShowFrame       : 1;
                U32 HorSizeCode     : 2;
                U32 VerSizeCode     : 2;
                U32 ColorSpace      : 1;
                U32 ClampType       : 1;
                U32 PartitionNumL2  : 2;
                U32 SegOn           : 1;
                U32 MBSegUpdate     : 1;
                U32 SegFeatUpdate   : 1;
                U32 SegFeatMode     : 1;
                U32 LoopFilterType  : 1;
                U32 LoopFilterLevel : 6;
                U32 SharpnessLevel  : 3;
                U32 LoopFilterAdjOn : 1;
                U32 MbNoCoeffSkip   : 1;
                U32 RefreshEntropyP : 1;
            };
        };
        union {
            U32  FrameCtrl2;
            struct {
                U32 GoldenCopyFlag  : 2; /* 0 - no_ref, 1 - last, 2 - alt, 3 - cur */
                U32 AltRefCopyFlag  : 2; /* 0 - no_ref, 1 - last, 2 - gld, 3 - cur */
                U32 LastFrameUpdate : 1;
                U32 SignBiasGolden  : 1;
                U32 SignBiasAltRef  : 1;
                U32 Intra16PUpdate  : 1;
                U32 IntraUVPUpdate  : 1;
            };
        };
        I8      segFeatureData[VP8_NUM_OF_MB_FEATURES][VP8_MAX_NUM_OF_SEGMENTS];
        I8      segFeatureData_pf[VP8_NUM_OF_MB_FEATURES][VP8_MAX_NUM_OF_SEGMENTS];
        I8      refLFDeltas[VP8_NUM_OF_REF_FRAMES];
        I8      refLFDeltas_pf[VP8_NUM_OF_REF_FRAMES];
        I8      modeLFDeltas[VP8_NUM_OF_MODE_LF_DELTAS];
        I8      modeLFDeltas_pf[VP8_NUM_OF_MODE_LF_DELTAS];

        //    U8      qIndexes[VP8_NUM_COEFF_QUANTIZERS];

        U8      qIndex;
        I8      qDelta[VP8_NUM_COEFF_QUANTIZERS-1];

        struct {
            Vp8Prob prSkipFalse;
            Vp8Prob prIntra;
            Vp8Prob prLast;
            Vp8Prob prGolden;
            Vp8Prob segTreeProbs[VP8_NUM_OF_SEGMENT_TREE_PROBS];
            Vp8Prob mbModeProbY[VP8_NUM_MB_MODES_Y - 1];
            Vp8Prob mbModeProbUV[VP8_NUM_MB_MODES_UV - 1];
            Vp8Prob mvContexts[2][VP8_NUM_MV_PROBS];
            Vp8Prob coeff_probs[VP8_NUM_COEFF_PLANES][VP8_NUM_COEFF_BANDS][VP8_NUM_LOCAL_COMPLEXITIES][VP8_NUM_COEFF_NODES];
            /* Previous frame probabilities stored for incremental conditional update */
            Vp8Prob mbModeProbY_pf[VP8_NUM_MB_MODES_Y - 1];
            Vp8Prob mbModeProbUV_pf[VP8_NUM_MB_MODES_UV - 1];
            Vp8Prob mvContexts_pf[2][VP8_NUM_MV_PROBS];
            Vp8Prob coeff_probs_pf[VP8_NUM_COEFF_PLANES][VP8_NUM_COEFF_BANDS][VP8_NUM_LOCAL_COMPLEXITIES][VP8_NUM_COEFF_NODES];
        };

        Vp8FrameParamsEnc():FrameCtrl1(0),FrameCtrl2(0){};
    };

#define QP_BYTE_OFFSET      0
#define LFLEVEL_BYTE_OFFSET 4

    inline void ReadBRCStatusReport(mfxU8 * MBDataMemory, Vp8FrameParamsEnc &ctrl, const MBDATA_LAYOUT &layout)
    {
        if (layout.MB_CODE_offset != 0)
        {
            // QP and loop filter levels are passed in the beginning of MB data surface
            ctrl.qIndex = MBDataMemory[QP_BYTE_OFFSET];
            ctrl.LoopFilterLevel = MBDataMemory[LFLEVEL_BYTE_OFFSET];
        }
    }

    struct MbUVPred {
        U8 ULeftPred[8];
        U8 UTopPred[8];
        U8 UTopLeftPred;
        U8 VLeftPred[8];
        U8 VTopPred[8];
        U8 VTopLeftPred;
    };

    inline void clampMvComponent(MvTypeVp8 &mv, I16 T, I16 B, I16 L, I16 R)
    {
        if(mv.MV.mvx < L)
            mv.MV.mvx = L;
        else if(mv.MV.mvx > R)
            mv.MV.mvx = R;

        if(mv.MV.mvy < T)
            mv.MV.mvy = T;
        else if(mv.MV.mvy > B)
            mv.MV.mvy = B;
    }

    typedef struct {
        U32 absValHist[VP8_MV_LONG - VP8_MV_SHORT + 1]; /* Histogram of Tree based counters for short MVs defining evnCnt[2..9) */
        U32 evnCnt[2][VP8_NUM_MV_PROBS];                /* Event counters */
    } Vp8MvCounters;

    typedef struct {
        /* Known to PAK */
        U32 mbSkipFalse;
        U32 mbTokenHist[VP8_NUM_COEFF_PLANES][VP8_NUM_COEFF_BANDS][VP8_NUM_LOCAL_COMPLEXITIES][16];
        /* Known to ENC/PAK*/
        U32 mbRefHist[VP8_NUM_OF_REF_FRAMES];
        U32 mbSegHist[VP8_MAX_NUM_OF_SEGMENTS];
        U32 mbModeYHist[VP8_NUM_MB_MODES_Y];
        U32 mbModeUVHist[VP8_NUM_MB_MODES_UV];
        Vp8MvCounters mbMVs[2];
    } Vp8Counters;

    enum {
        MODE_INTRA_NONPRED = 0,
        MODE_INTRA_16x16,
        MODE_INTRA_8x8,
        MODE_INTRA_4x4
    };

    enum {
        MODE_INTER_16x16 = 0,
        MODE_INTER_16x8,
        MODE_INTER_8x8,
        MODE_INTER_4x4
    };

    class Vp8CoreBSE
    {
    private:
        Vp8EncoderParams  m_Params;
        Vp8FrameParamsEnc m_ctrl;
        mfxU32            m_encoded_frame_num;
        mfxBitstream     *m_pBitstream;
        mfxU8            *m_pPartitions[VP8_MAX_NUM_OF_PARTITIONS+1];
        vp8BoolEncState   m_BoolEncStates[VP8_MAX_NUM_OF_PARTITIONS+1];
        U32               m_fSizeInMBs;
        MbUVPred         *m_UVPreds;
        U8                m_fRefBias[4];

        Vp8Counters       m_cnt1;
        TrMbCodeVp8      *m_Mbs1;
        TrMbCodeVp8       m_externalMb1;
        U16              *m_TokensE[VP8_MAX_NUM_OF_PARTITIONS];
        U16              *m_TokensB[VP8_MAX_NUM_OF_PARTITIONS];

        VP8HybridCosts    m_costs;

        mfxStatus InitVp8VideoParam(const mfxVideoParam &video);
        void DetermineFrameType(const sFrameParams &frameParams);
        void PrepareFrameControls(const sFrameParams &frameParams);
        void PrepareFrameControlsHW(void *conf);
        void EncodeFirstPartition(void);
        void Tokenization(void);
        void EncodeTokenPartitions(void);
        void EncodeFrameMBs(void);
        void EncodeKeyFrameMB(void);
        void UpdateEntropyModel(void);
        void CalculateCosts(void);
        mfxStatus OutputBitstream(void);
        void TokenizeAndCnt(TaskHybridDDI *pTask, MBDATA_LAYOUT const & layout, mfxCoreInterface * pCore
            );
        void EncodeTokens( const CoeffsType un_zigzag_coeffs[], I32 firstCoeff, I32 lastCoeff, U8 cntx1, U8 cntx3, U8 part );

    public:
        Vp8CoreBSE();
        ~Vp8CoreBSE();
        mfxStatus Init(const mfxVideoParam &video);
        mfxStatus Reset(const mfxVideoParam &video);
        mfxStatus SetNextFrame(mfxFrameSurface1 * pSurface, bool bExternal, const sFrameParams &frameParams, mfxU32 frameNum);
        mfxStatus RunBSP(bool bIVFHeaders, bool bAddSH, mfxBitstream *bs, TaskHybridDDI *pTask, MBDATA_LAYOUT const & layout, mfxCoreInterface * pCore
            );

        VP8HybridCosts GetUpdatedCosts();
    };
}

#endif
#endif
