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

#include "mfx_filter_externals.h"
#include "sample_utils.h"

#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "memory.h"

#include "strmif.h"

#include <stdio.h>
#include <tchar.h>
#include <list>

#include "bs_adapter.h"
#include "codec_presets.h"

#include <d3d9.h>
#include <mfapi.h>
#include <dxva2api.h>
#include <evr.h>
#include <mferror.h>

mfxStatus   InitMfxFrameSurface     (mfxFrameSurface1* pSurface, mfxFrameInfo* pInfo, IMediaSample *pSample);

//functions for converting mfxVideoParam to filters internal format
//definitions are here, implementation are in particular filter
mfxStatus CopyMFXToEncoderParams(IConfigureVideoEncoder::Params* pParams, mfxVideoParam* pmfxParams);
mfxStatus CopyEncoderToMFXParams(IConfigureVideoEncoder::Params* pParams, mfxVideoParam* pmfxParams);

mfxU64 ConvertReferenceTime2MFXTime(REFERENCE_TIME rtTime);

class CTimeManager
{
public:

    CTimeManager() : m_dFrameRate(0), m_nFrameNumber(0), m_nBitrate(0) {};

    void Init(DOUBLE dFrameRate)
    {
        m_dFrameRate = dFrameRate;
    };
    void Reset()
    {
        m_nFrameNumber = 0;
    };
    INT GetEncodedFramesCount()
    {
        return m_nFrameNumber;
    };
    DOUBLE GetFrameRate()
    {
        return m_dFrameRate;
    };
    INT GetBitrate()
    {
        return m_nBitrate;
    };
    void SetEncodedFramesCount(INT nFrameNumber)
    {
        m_nFrameNumber = nFrameNumber;
    };

    void SetBitrate(INT nBitrate)
    {
        m_nBitrate = nBitrate;
    };
    HRESULT GetTime(REFERENCE_TIME* dStart, REFERENCE_TIME* dStop)
    {
        if (!dStart || !dStop)
            return E_POINTER;
        if (!m_dFrameRate)
            return E_FAIL;

        m_nFrameNumber++;

        *dStart = (REFERENCE_TIME)((1.0/m_dFrameRate * (m_nFrameNumber - 1)) * 1e7);
        *dStop  = (REFERENCE_TIME)(*dStart + (1.0/m_dFrameRate) * 1e7);

        return S_OK;
    };

private:
    DOUBLE      m_dFrameRate;
    INT         m_nFrameNumber;
    INT         m_nBitrate;
};
