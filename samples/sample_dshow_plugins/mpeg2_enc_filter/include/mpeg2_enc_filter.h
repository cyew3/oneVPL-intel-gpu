/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2011 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "mfx_video_enc_filter.h"
#include "mpeg2_enc_proppage.h"

/////////////////////////////////////////////////////////////////////////////
// define GUIDs
/////////////////////////////////////////////////////////////////////////////

class CMPEG2EncVideoFilter : public CEncVideoFilter
{
public:

    DECLARE_IUNKNOWN;
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    CMPEG2EncVideoFilter(TCHAR *tszName,LPUNKNOWN punk,HRESULT *phr);
    ~CMPEG2EncVideoFilter();

    // Fills a counted array of GUID values where each GUID specifies the CLSID of each property page that can be displayed
    STDMETHODIMP        GetPages(CAUUID *pPages);

    HRESULT             CheckInputType(const CMediaType* mtIn);
    
protected:

    // Fill m_EncoderParams with default values
    void                SetDefaultParams(void);
    // Fill m_EncoderParams with values from registry if available, 
    // otherwise write current m_EncoderParams values to registry
    void                ReadParamsFromRegistry(void);
    // Write current m_EncoderParams values to registry
    void                WriteParamsToRegistry(void);
    void                ReadFrameRateFromRegistry(mfxU32 &iFrameRate);
    void                UpdateRegistry(void) 
    {CopyMFXToEncoderParams(&m_EncoderParams,&m_mfxParamsVideo); WriteParamsToRegistry(); CEncVideoFilter::UpdateRegistry();};

    // Crops are not supported by MPEG2 standard, so we need to use VPP
    virtual bool        EncResetRequired(const mfxFrameInfo *pNewFrameInfo);
};
