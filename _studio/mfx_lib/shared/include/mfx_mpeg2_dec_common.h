//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_MPEG2_DEC_COMMON_H__
#define __MFX_MPEG2_DEC_COMMON_H__

#include "mfx_common.h"
#if defined MFX_ENABLE_MPEG2_VIDEO_DECODE

#include "mfx_common_int.h"
#include "mfxvideo.h"
#include "umc_mpeg2_dec_defs.h"

#define MFX_PROFILE_MPEG1 8

void GetMfxFrameRate(mfxU8 umcFrameRateCode, mfxU32 *frameRateNom, mfxU32 *frameRateDenom);
inline bool IsMpeg2StartCodeEx(const mfxU8* p);

template<class T> inline
T AlignValue(T value, mfxU32 divisor)
{
    return static_cast<T>(((value + divisor - 1) / divisor) * divisor);
}

#endif

#endif //__MFX_MPEG2_DEC_COMMON_H__
