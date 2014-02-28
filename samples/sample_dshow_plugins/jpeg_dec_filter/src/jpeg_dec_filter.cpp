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

#include "jpeg_dec_filter.h"
#include "filter_defs.h"
#include "mfx_filter_register.h"

#define MIN_REQUIRED_API_VERSION_MINOR 3
#define MIN_REQUIRED_API_VERSION_MAJOR 1

CJPEGDecVideoFilter::CJPEGDecVideoFilter(TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr) :
    CDecVideoFilter(tszName,punk, FILTER_GUID, phr, MIN_REQUIRED_API_VERSION_MINOR, MIN_REQUIRED_API_VERSION_MAJOR)
{
    m_bstrFilterName = FILTER_NAME;
    m_mfxParamsVideo.mfx.CodecId = MFX_CODEC_JPEG;
}

CUnknown * WINAPI CJPEGDecVideoFilter::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    CJPEGDecVideoFilter *pNewObject = new CJPEGDecVideoFilter(FILTER_NAME, punk, phr);

    if (NULL == pNewObject)
    {
        *phr = E_OUTOFMEMORY;
    }

    return pNewObject;
}

HRESULT CJPEGDecVideoFilter::CheckInputType(const CMediaType *mtIn)
{
    MSDK_CHECK_POINTER(mtIn, E_POINTER);

    if (MEDIASUBTYPE_MJPG != *mtIn->Subtype())
    {
        return VFW_E_INVALIDMEDIATYPE;
    }

    return CDecVideoFilter::CheckInputType(mtIn);
};