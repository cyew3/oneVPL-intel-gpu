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

#include "mfx_samples_config.h"

#include "streams.h"
#include <initguid.h>
#include <tchar.h>
#include <stdio.h>
#include <dvdmedia.h>

#include "mvc_dec_filter.h"
#include "filter_defs.h"
#include "mfx_filter_register.h"

#include "mfxmvc.h"

#define MIN_REQUIRED_API_VERSION_MINOR 3
#define MIN_REQUIRED_API_VERSION_MAJOR 1

CMVCDecVideoFilter::CMVCDecVideoFilter(TCHAR *tszName,LPUNKNOWN punk, HRESULT *phr) :
    CDecVideoFilter(tszName,punk, FILTER_GUID, phr, MIN_REQUIRED_API_VERSION_MINOR, MIN_REQUIRED_API_VERSION_MAJOR)
{
    m_bstrFilterName = FILTER_NAME;
    m_mfxParamsVideo.mfx.CodecId = MFX_CODEC_AVC;
    m_bIsExtBuffers = false;

    CreateExtMVCBuffers();
}

CMVCDecVideoFilter::~CMVCDecVideoFilter(void)
{
    DeallocateExtMVCBuffers();
    DeleteExtBuffers();
};

CUnknown * WINAPI CMVCDecVideoFilter::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    CMVCDecVideoFilter *pNewObject = new CMVCDecVideoFilter(FILTER_NAME, punk, phr);

    if (NULL == pNewObject)
    {
        *phr = E_OUTOFMEMORY;
    }

    return pNewObject;
}

HRESULT CMVCDecVideoFilter::CheckInputType(const CMediaType *mtIn)
{
    MSDK_CHECK_POINTER(mtIn, E_POINTER);

    if (MEDIASUBTYPE_H264 != *mtIn->Subtype())
    {
        return VFW_E_INVALIDMEDIATYPE;
    }

    return CDecVideoFilter::CheckInputType(mtIn);
};

HRESULT CMVCDecVideoFilter::GetMediaType(int iPosition, CMediaType* pmt)
{
    HRESULT hr = CDecVideoFilter::GetMediaType(iPosition, pmt);

    if (SUCCEEDED(hr))
    {
        VIDEOINFOHEADER2 *pDstVIH = NULL;
        pDstVIH = reinterpret_cast<VIDEOINFOHEADER2*> (pmt->pbFormat);
        MSDK_CHECK_POINTER(pDstVIH, E_UNEXPECTED);

        pDstVIH->dwInterlaceFlags = 0;
    }

    return hr;
}

void CMVCDecVideoFilter::AttachExtParam(mfxVideoParam *par)
{
    par->ExtParam = m_ppExtBuffers;
    par->NumExtParam = m_nNumExtBuffers;
}

mfxStatus CMVCDecVideoFilter::CreateExtMVCBuffers()
{
    std::auto_ptr<mfxExtMVCSeqDesc> pExtMVCSeqDesc (new mfxExtMVCSeqDesc());
    MSDK_CHECK_POINTER(pExtMVCSeqDesc.get(), MFX_ERR_MEMORY_ALLOC);
    pExtMVCSeqDesc->Header.BufferId = MFX_EXTBUFF_MVC_SEQ_DESC;
    pExtMVCSeqDesc->Header.BufferSz = sizeof(mfxExtMVCSeqDesc);

    m_ppExtBuffers = new mfxExtBuffer* [1];
    MSDK_CHECK_POINTER(m_ppExtBuffers, MFX_ERR_MEMORY_ALLOC);

    m_ppExtBuffers[0] = (mfxExtBuffer*) pExtMVCSeqDesc.release();
    m_nNumExtBuffers = 1;

    return MFX_ERR_NONE;
}

void CMVCDecVideoFilter::DeleteExtBuffers()
{
    if (m_ppExtBuffers != NULL)
    {
        for (mfxU32 i = 0; i < m_nNumExtBuffers; i++)
        {
            if (m_ppExtBuffers[i] != NULL)
            {
                MSDK_SAFE_DELETE(m_ppExtBuffers[i]);
            }
        }
    }

    MSDK_SAFE_DELETE_ARRAY(m_ppExtBuffers);
}

mfxStatus CMVCDecVideoFilter::AllocateExtMVCBuffers(mfxVideoParam *par)
{
    mfxU32 i = 0;

    DeallocateExtMVCBuffers();

    mfxExtMVCSeqDesc* pExtMVCBuffer = (mfxExtMVCSeqDesc*) par->ExtParam[0];
    MSDK_CHECK_POINTER(pExtMVCBuffer, MFX_ERR_MEMORY_ALLOC);

    pExtMVCBuffer->View = new mfxMVCViewDependency[pExtMVCBuffer->NumView];
    MSDK_CHECK_POINTER(pExtMVCBuffer->View, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < pExtMVCBuffer->NumView; ++i)
    {
        MSDK_ZERO_MEMORY(pExtMVCBuffer->View[i]);
    }
    pExtMVCBuffer->NumViewAlloc = pExtMVCBuffer->NumView;

    pExtMVCBuffer->ViewId = new mfxU16[pExtMVCBuffer->NumViewId];
    MSDK_CHECK_POINTER(pExtMVCBuffer->ViewId, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < pExtMVCBuffer->NumViewId; ++i)
    {
        MSDK_ZERO_MEMORY(pExtMVCBuffer->ViewId[i]);
    }
    pExtMVCBuffer->NumViewIdAlloc = pExtMVCBuffer->NumViewId;

    pExtMVCBuffer->OP = new mfxMVCOperationPoint[pExtMVCBuffer->NumOP];
    MSDK_CHECK_POINTER(pExtMVCBuffer->OP, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < pExtMVCBuffer->NumOP; ++i)
    {
        MSDK_ZERO_MEMORY(pExtMVCBuffer->OP[i]);
    }
    pExtMVCBuffer->NumOPAlloc = pExtMVCBuffer->NumOP;

    return MFX_ERR_NONE;
}

void CMVCDecVideoFilter::DeallocateExtMVCBuffers()
{
    mfxExtMVCSeqDesc* pExtMVCBuffer = NULL;

    if (m_mfxParamsVideo.NumExtParam > 0 &&
        m_mfxParamsVideo.ExtParam &&
        m_mfxParamsVideo.ExtParam[0])
    {
        pExtMVCBuffer = (mfxExtMVCSeqDesc*) m_mfxParamsVideo.ExtParam[0];
    }

    if (pExtMVCBuffer != NULL)
    {
        MSDK_SAFE_DELETE_ARRAY(pExtMVCBuffer->View);
        MSDK_SAFE_DELETE_ARRAY(pExtMVCBuffer->ViewId);
        MSDK_SAFE_DELETE_ARRAY(pExtMVCBuffer->OP);
    }

    if (m_mfxParamsVideo.ExtParam)
    {
        MSDK_SAFE_DELETE(m_mfxParamsVideo.ExtParam[0]);
    }

    m_bIsExtBuffers = false;
}

HRESULT CMVCDecVideoFilter::Receive(IMediaSample* pSample)
{
    HRESULT         hr = S_OK;
    mfxVideoParam   VideoParams;
    mfxStatus       sts = MFX_ERR_NONE;
    mfxBitstream    mfxBS;

    CAutoLock cObjectLock(&m_csLock);

    // check error(s)
    if (NULL == pSample || m_bFlushing)
    {
        return S_OK;
    }

    if (0 == pSample->GetActualDataLength())
    {
        return S_FALSE;
    }

    if (m_pDecoder && !m_pDecoder->GetHeaderDecoded() && !m_bStop)
    {
        memset(&mfxBS, 0, sizeof(mfxBitstream));
        m_pFrameConstructor->ConstructFrame(pSample, &mfxBS);

        memset(&VideoParams, 0, sizeof(mfxVideoParam));
        VideoParams.mfx.CodecId = m_mfxParamsVideo.mfx.CodecId;
        VideoParams.AsyncDepth = (mfxU16)m_bLowLatencyMode;

        AttachExtParam(&VideoParams);

        sts = m_pDecoder->DecodeHeader(&mfxBS, &VideoParams);

        if (MFX_ERR_NONE == sts)
        {
            mfxIMPL impl;
            m_pDecoder->QueryIMPL(&impl);
            VideoParams.IOPattern = (mfxU16)((m_pDecoder->GetMemoryType()) ? MFX_IOPATTERN_OUT_VIDEO_MEMORY : MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

            m_pDecoder->Close();
            sts = m_pDecoder->Init(&VideoParams, m_nPitch);
        }

        if (MFX_ERR_NONE == sts)
        {
            MSDK_MEMCPY_VAR(m_mfxParamsVideo, &VideoParams, sizeof(mfxVideoParam));
        }
        else
        {
            hr = E_FAIL;
        }

        if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
        {
            m_pFrameConstructor->SaveResidialData(&mfxBS);

            sts = AllocateExtMVCBuffers(&VideoParams);

            MSDK_SAFE_DELETE_ARRAY(mfxBS.Data);

            if (S_OK != sts)
            {
                hr = E_FAIL;
            }

            return hr;
        }
    }

    if (SUCCEEDED(hr))
    {
        hr = CDecVideoFilter::Receive(pSample);
    }

    return hr;
}

HRESULT CMVCDecVideoFilter::AttachCustomCodecParams(mfxVideoParam* pParams)
{
    if (NULL == pParams)
    {
        return E_POINTER;
    }

    AttachExtParam(pParams);
    return S_OK;
};
