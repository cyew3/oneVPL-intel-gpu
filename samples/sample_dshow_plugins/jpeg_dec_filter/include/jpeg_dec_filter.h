/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2011 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "mfx_filter_guid.h"
#include "mfx_video_dec_filter.h"
#include "mfxjpeg.h"

class CJPEGDecVideoFilter :  public CDecVideoFilter
{
public:

    DECLARE_IUNKNOWN;
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    CJPEGDecVideoFilter(TCHAR *tszName,LPUNKNOWN punk,HRESULT *phr);

    HRESULT CheckInputType(const CMediaType *mtIn);
};

