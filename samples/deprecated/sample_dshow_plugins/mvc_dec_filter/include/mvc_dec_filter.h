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

#define MSDK_ZERO_MEMORY(VAR) {memset(&VAR, 0, sizeof(VAR));}

class CMVCDecVideoFilter :  public CDecVideoFilter
{
public:

    DECLARE_IUNKNOWN;
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    CMVCDecVideoFilter(TCHAR *tszName,LPUNKNOWN punk,HRESULT *phr);
    ~CMVCDecVideoFilter(void);

    HRESULT     GetMediaType(int iPosition, CMediaType *pMediaType);
    HRESULT     CheckInputType(const CMediaType *mtIn);
    HRESULT     Receive(IMediaSample *pSample);

protected:

    // external buffers creation
    mfxStatus   CreateExtMVCBuffers();
    void        DeleteExtBuffers();
    // external buffers allocation
    mfxStatus   AllocateExtMVCBuffers(mfxVideoParam *par);
    void        DeallocateExtMVCBuffers();
    //
    void        AttachExtParam(mfxVideoParam *par);
    void        SetExtBuffersFlag()       { m_bIsExtBuffers = true; }

    //Add custom params
    HRESULT     AttachCustomCodecParams(mfxVideoParam* pParams);

protected:
    bool                m_bIsExtBuffers; // indicates if external buffers were allocated
    mfxExtBuffer**      m_ppExtBuffers;
    mfxU16              m_nNumExtBuffers;
};

