/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

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