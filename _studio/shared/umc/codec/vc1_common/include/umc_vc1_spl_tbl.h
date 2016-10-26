//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_VC1_SPL_TBL_H__
#define __UMC_VC1_SPL_TBL_H__

#include "ippdefs.h"

typedef struct
{
    Ipp16u  width;
    Ipp16u  height;

}AspectRatio;

extern  AspectRatio AspectRatioTable[16];
extern Ipp64f FrameRateNumerator[256];
extern Ipp64f FrameRateDenomerator[16];

extern Ipp32u bMax_LevelLimits[4][5];

#endif  //__UMC_VC1_SPL_TBL_H__
#endif //UMC_ENABLE_VC1_SPLITTER || UMC_ENABLE_VC1_VIDEO_DECODER
