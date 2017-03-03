/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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