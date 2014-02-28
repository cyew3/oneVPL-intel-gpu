/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mf_param_brc_multiplier.h"
#include "mf_guids.h"

/*------------------------------------------------------------------------------*/

void UpdateBRCParamMultiplier(mfxInfoMFX &mfx, mfxU16 nBRCParamMultiplier)
{
    nBRCParamMultiplier           = From0to1(nBRCParamMultiplier);
    mfxU16 nMfxBRCParamMultiplier = From0to1(mfx.BRCParamMultiplier);
    if (nMfxBRCParamMultiplier != nBRCParamMultiplier)
    {
        mfxF32 fScale = (mfxF32)(nMfxBRCParamMultiplier) / nBRCParamMultiplier;
        mfx.InitialDelayInKB = (mfxU16)ceil(mfx.InitialDelayInKB * fScale);
        mfx.BufferSizeInKB   = (mfxU16)ceil(mfx.BufferSizeInKB * fScale);
        mfx.TargetKbps       = (mfxU16)ceil(mfx.TargetKbps * fScale);
        mfx.MaxKbps          = (mfxU16)ceil(mfx.MaxKbps * fScale);
        mfx.BRCParamMultiplier = nBRCParamMultiplier;
    }
}

/*------------------------------------------------------------------------------*/

mfxU16 CheckBRCu16Limit(mfxInfoMFX &mfx, mfxU32 nValue)
{
    mfxU16 nBRCParamMultiplier = From0to1(mfx.BRCParamMultiplier);
    //max possible value is max(mfxU16)
    while ( (mfxU32)ceil((mfxF32)nValue/nBRCParamMultiplier) > _UI16_MAX)
    {
        nBRCParamMultiplier *= 10;
    }
    UpdateBRCParamMultiplier(mfx, nBRCParamMultiplier);
    return (mfxU16)ceil((mfxF32)nValue/nBRCParamMultiplier);
}

/*------------------------------------------------------------------------------*/
