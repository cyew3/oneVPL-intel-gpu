//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2012 Intel Corporation. All Rights Reserved.
//

#include "mfx_brc_common.h"
#include "mfx_enc_common.h"

#if defined(MFX_ENABLE_VIDEO_BRC_COMMON)

mfxStatus ConvertVideoParam_Brc(const mfxVideoParam *parMFX, UMC::VideoBrcParams *parUMC)
{
    MFX_CHECK_COND(parMFX != NULL);
    MFX_CHECK_COND((parMFX->mfx.FrameInfo.CropX + parMFX->mfx.FrameInfo.CropW) <= parMFX->mfx.FrameInfo.Width);
    MFX_CHECK_COND((parMFX->mfx.FrameInfo.CropY + parMFX->mfx.FrameInfo.CropH) <= parMFX->mfx.FrameInfo.Height);

    switch (parMFX->mfx.RateControlMethod) {
    case MFX_RATECONTROL_CBR:  parUMC->BRCMode = UMC::BRC_CBR; break;
    case MFX_RATECONTROL_AVBR: parUMC->BRCMode = UMC::BRC_AVBR; break;
    default:
    case MFX_RATECONTROL_VBR:  parUMC->BRCMode = UMC::BRC_VBR; break;
    }

    mfxU16 brcParamMultiplier = parMFX->mfx.BRCParamMultiplier ? parMFX->mfx.BRCParamMultiplier : 1;

    parUMC->targetBitrate = parMFX->mfx.TargetKbps * brcParamMultiplier * BRC_BITS_IN_KBIT;

    if (parUMC->BRCMode == UMC::BRC_AVBR)
    {
        parUMC->accuracy = parMFX->mfx.Accuracy;
        parUMC->convergence = parMFX->mfx.Convergence;
        parUMC->HRDBufferSizeBytes = 0;
        parUMC->HRDInitialDelayBytes = 0;
        parUMC->maxBitrate = parMFX->mfx.TargetKbps * brcParamMultiplier * BRC_BITS_IN_KBIT;
    }
    else
    {
        parUMC->maxBitrate = parMFX->mfx.MaxKbps * brcParamMultiplier * BRC_BITS_IN_KBIT;
        parUMC->HRDBufferSizeBytes = parMFX->mfx.BufferSizeInKB * brcParamMultiplier * BRC_BYTES_IN_KBYTE;
        parUMC->HRDInitialDelayBytes = parMFX->mfx.InitialDelayInKB * brcParamMultiplier * BRC_BYTES_IN_KBYTE;
    }

    parUMC->info.clip_info.width = parMFX->mfx.FrameInfo.Width;
    parUMC->info.clip_info.height = parMFX->mfx.FrameInfo.Height;

    parUMC->GOPPicSize = parMFX->mfx.GopPicSize;
    parUMC->GOPRefDist = parMFX->mfx.GopRefDist;

    parUMC->info.framerate = CalculateUMCFramerate(parMFX->mfx.FrameInfo.FrameRateExtN, parMFX->mfx.FrameInfo.FrameRateExtD);
    parUMC->frameRateExtD = parMFX->mfx.FrameInfo.FrameRateExtD;
    parUMC->frameRateExtN = parMFX->mfx.FrameInfo.FrameRateExtN;
    if (parUMC->info.framerate <= 0) {
        parUMC->info.framerate = 30;
        parUMC->frameRateExtD = 1;
        parUMC->frameRateExtN = 30;
    }

    return MFX_ERR_NONE;
}

#endif // defined(MFX_ENABLE_VIDEO_BRC_COMMON)
