/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

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