/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2011 Intel Corporation. All Rights Reserved.
//
*/

#include "streams.h"
#include <initguid.h>
#include <tchar.h>
#include <stdio.h>
#include <dvdmedia.h>

#include "mpeg2_dec_filter.h"
#include "filter_defs.h"
#include "mfx_filter_register.h"


CMPEG2DecVideoFilter::CMPEG2DecVideoFilter(TCHAR *tszName,LPUNKNOWN punk,HRESULT *phr) :
CDecVideoFilter(tszName,punk, FILTER_GUID,phr)
{
    m_bstrFilterName = FILTER_NAME;
    m_mfxParamsVideo.mfx.CodecId = MFX_CODEC_MPEG2;
}

CUnknown * WINAPI CMPEG2DecVideoFilter::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    CMPEG2DecVideoFilter *pNewObject = new CMPEG2DecVideoFilter(FILTER_NAME, punk, phr);

    if (NULL == pNewObject)
    {
        *phr = E_OUTOFMEMORY;
    }

    return pNewObject;
}

HRESULT CMPEG2DecVideoFilter::CheckInputType(const CMediaType *mtIn)
{
    MSDK_CHECK_POINTER(mtIn, E_POINTER);

    if ((MEDIASUBTYPE_MPEG2_VIDEO != *mtIn->Subtype()))
    {
        return VFW_E_INVALIDMEDIATYPE;
    }   

    return CDecVideoFilter::CheckInputType(mtIn);
};