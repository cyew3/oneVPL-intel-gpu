/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfxdefs.h"
#include "mfxstructures.h"

inline mfxU16 From0to1(mfxU16 nBRCParamMultiplier) { return nBRCParamMultiplier ? nBRCParamMultiplier : 1; }

extern void UpdateBRCParamMultiplier(mfxInfoMFX &mfx, mfxU16 nBRCParamMultiplier);
//updates BRCParamMultiplier if needed and returns the recalculated value
extern mfxU16 CheckBRCu16Limit(mfxInfoMFX &mfx, mfxU32 nValue);

inline mfxU32 MfxGetInitialDelayInKB(const mfxInfoMFX &mfx) { return mfx.InitialDelayInKB * From0to1(mfx.BRCParamMultiplier); }
inline mfxU32 MfxGetBufferSizeInKB(const mfxInfoMFX &mfx) { return mfx.BufferSizeInKB * From0to1(mfx.BRCParamMultiplier); }
inline mfxU32 MfxGetTargetKbps(const mfxInfoMFX &mfx) { return mfx.TargetKbps * From0to1(mfx.BRCParamMultiplier); }
inline mfxU32 MfxGetMaxKbps(const mfxInfoMFX &mfx) { return mfx.MaxKbps * From0to1(mfx.MaxKbps); }

inline void MfxSetInitialDelayInKB(mfxInfoMFX &mfx, mfxU32 nValue) { mfx.InitialDelayInKB = CheckBRCu16Limit(mfx, nValue); }
inline void MfxSetBufferSizeInKB(mfxInfoMFX &mfx, mfxU32 nValue) { mfx.BufferSizeInKB = CheckBRCu16Limit(mfx, nValue); }
inline void MfxSetTargetKbps(mfxInfoMFX &mfx, mfxU32 nValue) { mfx.TargetKbps = CheckBRCu16Limit(mfx, nValue); }
inline void MfxSetMaxKbps(mfxInfoMFX &mfx, mfxU32 nValue) { mfx.MaxKbps = CheckBRCu16Limit(mfx, nValue); }

inline void MfxCopyBRCparams(mfxInfoMFX &mfxResult, mfxInfoMFX &mfxSource)
{
    mfxResult.BRCParamMultiplier = mfxSource.BRCParamMultiplier;
    mfxResult.InitialDelayInKB   = mfxSource.InitialDelayInKB;
    mfxResult.BufferSizeInKB     = mfxSource.BufferSizeInKB;
    mfxResult.TargetKbps         = mfxSource.TargetKbps;
    mfxResult.MaxKbps            = mfxSource.MaxKbps;
}