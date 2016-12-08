//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined MFX_ENABLE_MPEG2_VIDEO_DECODE

#include "mfx_mpeg2_dec_common.h"
#include "mfx_enc_common.h"

void GetMfxFrameRate(mfxU8 umcFrameRateCode, mfxU32 *frameRateNom, mfxU32 *frameRateDenom)
{
    switch (umcFrameRateCode)
    {
        case 0: *frameRateNom = 30; *frameRateDenom = 1; break;
        case 1: *frameRateNom = 24000; *frameRateDenom = 1001; break;
        case 2: *frameRateNom = 24; *frameRateDenom = 1; break;
        case 3: *frameRateNom = 25; *frameRateDenom = 1; break;
        case 4: *frameRateNom = 30000; *frameRateDenom = 1001; break;
        case 5: *frameRateNom = 30; *frameRateDenom = 1; break;
        case 6: *frameRateNom = 50; *frameRateDenom = 1; break;
        case 7: *frameRateNom = 60000; *frameRateDenom = 1001; break;
        case 8: *frameRateNom = 60; *frameRateDenom = 1; break;
        default: *frameRateNom = 30; *frameRateDenom = 1;
    }
    return;
}

#endif
