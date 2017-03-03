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

class MFParamCabac: public MFParamsWorker
{
public:
    MFParamCabac(MFParamsManager *pManager)
        : MFParamsWorker(pManager),
          m_nCAVLC(MFX_CODINGOPTION_UNKNOWN),
          m_nCodecProfile(0)
    {
    }

    virtual ~MFParamCabac()
    {
    };

    //MFParamsWorker::

    //virtual HRESULT HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender)
    //{ return E_FAIL; }


    virtual bool IsEnabled() const { return true; }

    STDMETHOD(UpdateVideoParam)(mfxVideoParam &videoParams, MFExtBufCollection &arrExtBuf) const
    {
        mfxExtCodingOption *pExtCodingOption = NULL;
        HRESULT hr = PrepareExtBuf(videoParams, arrExtBuf, pExtCodingOption);
        if (SUCCEEDED(hr))
        {
            ATLASSERT(NULL != pExtCodingOption); // is guaranteed by return value
            pExtCodingOption->CAVLC = m_nCAVLC;
        }
        return hr;
    }

    //Endpoint of ICodecAPI:

    virtual HRESULT Set(VARIANT_BOOL value)
    {
        HRESULT hr = S_OK;
        if (value != VARIANT_TRUE && value != VARIANT_FALSE)
        {
            ATLASSERT(value == VARIANT_TRUE || value == VARIANT_FALSE);
            hr = E_INVALIDARG;
        }
        else
        {
            m_nCAVLC = (mfxU16)(value ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);
        }
        return hr;
    }

    virtual VARIANT_BOOL Get(HRESULT *pHR)
    {
        VARIANT_BOOL bResult = VARIANT_FALSE;
        if (MFX_CODINGOPTION_OFF != m_nCAVLC && MFX_CODINGOPTION_ON != m_nCAVLC)
        {
            if (NULL != pHR)
                *pHR = VFW_E_CODECAPI_NO_CURRENT_VALUE;
        }
        else
        {
            bResult = (MFX_CODINGOPTION_ON == m_nCAVLC ? VARIANT_FALSE : VARIANT_TRUE);
            *pHR = S_OK;
        }
        return bResult;
    }

    virtual HRESULT IsSupported()
    {
        return S_OK;
    }

    virtual HRESULT IsModifiable()
    {
        return (ProfileSupportsCabac() ? S_OK : S_FALSE);
    }

//TODO: remove this by implementing local syncronized copies of mfxVideoParam in MFParamsWorker
    void SetCodecProfile(mfxU16 nCodecProfile)
    {
        m_nCodecProfile = nCodecProfile;
        if (!ProfileSupportsCabac() && MFX_CODINGOPTION_OFF == m_nCAVLC)
        {
            m_nCAVLC = MFX_CODINGOPTION_ON;
        }
    }

protected:
    mfxU16 m_nCAVLC;
    mfxU16 m_nCodecProfile;

    bool ProfileSupportsCabac() const
    {
        return MFX_PROFILE_AVC_BASELINE != m_nCodecProfile &&
               MFX_PROFILE_AVC_CONSTRAINED_BASELINE  != m_nCodecProfile &&
               MFX_PROFILE_AVC_EXTENDED != m_nCodecProfile;
    }
};