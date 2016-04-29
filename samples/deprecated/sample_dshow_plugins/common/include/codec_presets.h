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
#ifndef __CODEC_PRESETS_H
#define __CODEC_PRESETS_H

#include "mfxvideo.h"
#include "tchar.h"
#include "sample_utils.h"

class CodecPreset
{
public:
    enum
    {
          PRESET_UNKNOWN      =  -1  // undefined preset
        , PRESET_USER_DEFINED        // User Defined (by setting encoder parameters)
        , PRESET_BALANCED            // Optimal speed and quality
        , PRESET_BEST_QUALITY        // Best quality setting
        , PRESET_FAST                // Fast encoding with reasonable quality
        , PRESET_LOW_LATENCY
    };

    //setts up all necessary parameters for encoder according specified preset
    static mfxStatus    VParamsFromPreset(mfxVideoParam & vParams , mfxU32 preset);
    static TCHAR*       Preset2Str(mfxU32);
    static mfxU32       Str2Preset(const TCHAR *);
    static mfxU16       PresetsNum(void);
    static mfxF64       CalcFramerate(mfxU32 nFrameRateExtN , mfxU32 nFrameRateExtD);

protected:
    static mfxF64       CalcBitrate(mfxU32 preset, mfxF64 at, PartiallyLinearFNC * pfnc);
};

#endif//__CODEC_PRESETS_H
