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
