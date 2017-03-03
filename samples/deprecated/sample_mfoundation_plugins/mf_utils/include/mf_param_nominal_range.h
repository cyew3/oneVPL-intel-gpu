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

#include "mf_mfx_params_interface.h"


class MFParamNominalRange: public MFParamsWorker
{
public:
    MFParamNominalRange(MFParamsManager *pManager)
        : MFParamsWorker(pManager),
          m_NominalRange(MFNominalRange_Unknown)
    {
    }

    virtual ~MFParamNominalRange()
    {
    };

    //MFParamsWorker::

    virtual bool IsEnabled() const { return MFNominalRange_Unknown != m_NominalRange; }

    // caller is reponsible for releasing all collected mfxExtBuffers
    // MFParamsWorker is allowed to modify any mfxExtBuffer only during this call but not after
    STDMETHOD(UpdateVideoParam)(mfxVideoParam &videoParams, MFExtBufCollection &arrExtBuf) const
    {
        mfxExtVideoSignalInfo *pSignalInfo = NULL;
        HRESULT hr = PrepareExtBuf(videoParams, arrExtBuf, pSignalInfo);
        if (SUCCEEDED(hr))
        {
            ATLASSERT(NULL != pSignalInfo); // is guaranteed by return value
            if (MFNominalRange_0_255 == m_NominalRange)
                pSignalInfo->VideoFullRange = 1;
        }
        return hr;
    }

    virtual HRESULT Set(UINT32 nNominalRange)
    {
        HRESULT hr = S_OK;
        switch (nNominalRange)
        {
            break;
        case MFNominalRange_0_255:  //MFNominalRange_Normal
            m_NominalRange = (MFNominalRange)nNominalRange;
            break;
        case MFNominalRange_16_235: //MFNominalRange_Wide
            m_NominalRange = (MFNominalRange)nNominalRange;
            break;
        case MFNominalRange_48_208:
        case MFNominalRange_64_127:
        case MFNominalRange_Unknown:
        default:
            hr = E_INVALIDARG;
        };
        return hr;
    }

protected:
    MFNominalRange          m_NominalRange;
};