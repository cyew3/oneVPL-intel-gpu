/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef _MFX_H265_FEI_H
#define _MFX_H265_FEI_H

#include "mfxdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *mfxFEIH265;
typedef void *mfxFEISyncPoint;

#define FEI_INTERP_BORDER       4
#define FEI_MAX_NUM_REF_FRAMES  8/*4*/
#define FEI_MAX_INTRA_MODES     33  /* intra angular prediction modes (excluding DC and planar) */

enum FEIBlockSize {
    FEI_256x256 = 0,
    FEI_128x128 = 1,
    FEI_64x64 = 2,
    FEI_32x32 = 3,
    FEI_32x16 = 4,
    FEI_16x32 = 5,
    FEI_16x16 = 6,
    FEI_8x8   = 9,

    FEI_BLK_MAX = 12,
};

/* must correspond with public API! */
enum UnsupportedBlockSizes {
    FEI_16x8_US  = 7,
    FEI_8x16_US  = 8,
    FEI_8x4_US   = 10,
    FEI_4x8_US   = 11,
};

enum FEIOperation
{
    FEI_NOP         = 0x00,
    FEI_INTRA_MODE  = 0x01,
    FEI_INTRA_DIST  = 0x02,
    FEI_INTER_ME    = 0x04,
    FEI_INTERPOLATE = 0x08,
};

enum FEIFrameType
{
    FEI_I_FRAME = 1,
    FEI_P_FRAME = 2,
    FEI_B_FRAME = 4,
};

typedef struct
{
    mfxU16 Dist;
    mfxU16 reserved;
} mfxFEIH265IntraDist;

/* input parameters - set once at init */
typedef struct
{
    mfxI32 Width;
    mfxI32 Height;
    mfxI32 MaxCUSize;
    mfxI32 MPMode;
    mfxI32 NumRefFrames;
    mfxI32 NumIntraModes;
} mfxFEIH265Param;

/* basic info for current and reference frames */
typedef struct
{
    mfxU8* YPlane;
    mfxI32 YPitch;
    mfxI32 EncOrder;
} mfxFEIH265Frame;

/* FEI input - update before each call to ProcessFrameAsync */
typedef struct
{
    FEIOperation FEIOp;
    FEIFrameType FrameType;
    mfxI32 RefIdx;

    mfxFEIH265Frame FEIFrameIn;
    mfxFEIH265Frame FEIFrameRef;

} mfxFEIH265Input;

/* FEI output for current frame - can be used directly in SW encoder */
typedef struct
{
    /* intra processing pads to multiple of 16 if needed */
    mfxI32                PaddedWidth;
    mfxI32                PaddedHeight;

    /* ranked list of IntraMaxModes modes for each block size */
    mfxI32                IntraMaxModes;
    mfxI32              * IntraModes4x4;
    mfxI32              * IntraModes8x8;
    mfxI32              * IntraModes16x16;
    mfxI32              * IntraModes32x32;
    mfxU32                IntraPitch4x4;
    mfxU32                IntraPitch8x8;
    mfxU32                IntraPitch16x16;
    mfxU32                IntraPitch32x32;

    /* intra distortion estiamates (16x16 grid) */
    mfxI32                IntraPitch;
    mfxFEIH265IntraDist * IntraDist;

    /* motion vector estimates for multiple block sizes */
    mfxI32                PitchDist[FEI_BLK_MAX];
    mfxI32                PitchMV[FEI_BLK_MAX];
    mfxU32              * Dist[FEI_MAX_NUM_REF_FRAMES][FEI_BLK_MAX];
    mfxI16Pair          * MV[FEI_MAX_NUM_REF_FRAMES][FEI_BLK_MAX];

    /* half-pel interpolated planes - 4-pixel border added on all sides */
    mfxI32                InterpolateWidth;
    mfxI32                InterpolateHeight;
    mfxI32                InterpolatePitch;
    mfxU8               * Interp[FEI_MAX_NUM_REF_FRAMES][3];

} mfxFEIH265Output;

/* FEI functions - LIB interface */
// VideoCore* is needed for multisession mode to have its own CmDevice in each session (number of Cm surfases is restricted)
mfxStatus H265FEI_Init(mfxFEIH265 *feih265, mfxFEIH265Param *param, void *core = NULL);
mfxStatus H265FEI_Close(mfxFEIH265 feih265);
mfxStatus H265FEI_ProcessFrameAsync(mfxFEIH265 feih265, mfxFEIH265Input *in, mfxFEIH265Output *out, mfxFEISyncPoint *syncp);
mfxStatus H265FEI_SyncOperation(mfxFEIH265 feih265, mfxFEISyncPoint syncp, mfxU32 wait);

#ifdef __cplusplus
}
#endif

#endif  /* _MFX_H265_FEI_H */
