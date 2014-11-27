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
#include "mfxfeih265.h"     /* inherit public API - this file should only contain non-public definitions */

//#define SAVE_FEI_STATE      /* enabling will save FEI parameters and output to txt files for unit testing with sample app */

#ifdef __cplusplus
extern "C" {
#endif

typedef void *mfxFEIH265;
typedef void *mfxFEISyncPoint;

/* input parameters - set once at init */
typedef struct
{
    mfxU32 Width;
    mfxU32 Height;
    mfxU32 MaxCUSize;
    mfxU32 MPMode;
    mfxU32 NumRefFrames;
    mfxU32 NumIntraModes;
} mfxFEIH265Param;

/* basic info for current and reference frames */
typedef struct
{
    mfxU8* YPlane;
    mfxU32 YPitch;
    mfxU32 PicOrder;
    mfxU32 EncOrder;
} mfxFEIH265Frame;

/* FEI input - update before each call to ProcessFrameAsync */
typedef struct
{
    mfxU32 FEIOp;
    mfxU32 FrameType;
    mfxU32 RefIdx;

    mfxFEIH265Frame FEIFrameIn;
    mfxFEIH265Frame FEIFrameRef;

} mfxFEIH265Input;

/* must correspond with public API! */
enum UnsupportedBlockSizes {
    MFX_FEI_H265_BLK_8x4_US   = 10,
    MFX_FEI_H265_BLK_4x8_US   = 11,

    /* only used internally */
    MFX_FEI_H265_BLK_256x256 = 0,
    MFX_FEI_H265_BLK_128x128 = 1,
    MFX_FEI_H265_BLK_64x64 = 2,

    /* public API allocates 64 slots so this could be increased later (update SyncCurrent()) */
    MFX_FEI_H265_BLK_MAX      = 12,
};


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
