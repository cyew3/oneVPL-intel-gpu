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

/* must correspond with public API! */
enum UnsupportedBlockSizes {
    FEI_16x8_US  = 7,
    FEI_8x16_US  = 8,
    FEI_8x4_US   = 10,
    FEI_4x8_US   = 11,
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
