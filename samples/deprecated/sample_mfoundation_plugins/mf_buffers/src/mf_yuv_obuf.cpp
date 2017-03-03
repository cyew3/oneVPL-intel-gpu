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

/*********************************************************************************

File: mf_yuv_obuf.cpp

Purpose: define code for support of output buffering in Intel MediaFoundation
decoder and vpp plug-ins.

*********************************************************************************/
#include "mf_yuv_obuf.h"

/*------------------------------------------------------------------------------*/

#undef  MFX_TRACE_CATEGORY
#define MFX_TRACE_CATEGORY _T("mfx_mft_unk")

/*------------------------------------------------------------------------------*/
// MFSurface class

MFYuvOutSurface::MFYuvOutSurface(void):
    m_pAlloc(NULL),
    m_bIsD3D9(),
    m_State(stSurfaceFree),
    m_pMediaBuffer(NULL),
    m_pSample(NULL),
    m_p2DBuffer(NULL),
    m_pComMfxSurface(NULL),
    m_bLocked(false),
    m_bInitialized(false),
    m_bDoNotAlloc(false),
    m_bFake(false),
    m_bGap(false),
    m_pSamplesPool(NULL),
    m_nPitch(0)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
}

/*------------------------------------------------------------------------------*/

MFYuvOutSurface::~MFYuvOutSurface(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (m_bLocked && m_p2DBuffer)
    {
        m_p2DBuffer->Unlock2D();
        m_bLocked = false;
    }
    SAFE_RELEASE(m_pComMfxSurface);
    SAFE_RELEASE(m_p2DBuffer);
    SAFE_RELEASE(m_pSample);
    SAFE_RELEASE(m_pMediaBuffer);
    SAFE_RELEASE(m_pSamplesPool);
    Close();
}

/*------------------------------------------------------------------------------*/

mfxStatus MFYuvOutSurface::Init(mfxFrameInfo*  pInfo,
                                MFSamplesPool* pSamplesPool,
                                mfxFrameAllocator *pAlloc,
                                bool isD3D9)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    CHECK_EXPRESSION(!m_bInitialized, MFX_ERR_ABORTED);
    CHECK_POINTER(pInfo, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pSamplesPool, MFX_ERR_NULL_PTR);
    CHECK_EXPRESSION((MFX_FOURCC_NV12 == pInfo->FourCC ||
                      MFX_FOURCC_YUY2 == pInfo->FourCC), MFX_ERR_ABORTED);
    CHECK_EXPRESSION((MFX_CHROMAFORMAT_YUV420 == pInfo->ChromaFormat ||
                      MFX_CHROMAFORMAT_YUV422 == pInfo->ChromaFormat), MFX_ERR_ABORTED);

    if (!m_pComMfxSurface &&
        FAILED(MFCreateMfxFrameSurface(&m_pComMfxSurface)))
    {
        sts = MFX_ERR_MEMORY_ALLOC;
    }
    if (MFX_ERR_NONE == sts)
    {
        mfxFrameSurface1* srf = m_pComMfxSurface->GetMfxFrameSurface();

        memcpy_s(&(srf->Info), sizeof(srf->Info), pInfo, sizeof(mfxFrameInfo));
        memset(&(srf->Data),0,sizeof(mfxFrameData));

        m_nPitch = (mfxU16)(MEM_ALIGN(pInfo->Width, m_gMemAlignDec));
        MFX_LTRACE_I(MF_TL_GENERAL, pInfo->Width);
        MFX_LTRACE_I(MF_TL_GENERAL, m_nPitch);

        pSamplesPool->AddRef();
        m_pSamplesPool = pSamplesPool;

        m_bInitialized = true;
    }

    m_pAlloc = pAlloc;
    m_bIsD3D9 = isD3D9;

    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

HRESULT MFYuvOutSurface::Pretend(mfxU16 w, mfxU16 h)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

    if (m_p2DBuffer)
    {
        MFVideoBuffer *pVideoBuffer = NULL;

        hr = m_p2DBuffer->QueryInterface(IID_MFVideoBuffer, (void**)&pVideoBuffer);
        if (SUCCEEDED(hr) && pVideoBuffer)
        {
            bool flag = (w && h)? true: false;
            pVideoBuffer->PretendOn(flag);
            if (flag) hr = pVideoBuffer->Pretend(w, h);
        }
        SAFE_RELEASE(pVideoBuffer);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFYuvOutSurface::AllocD3D9(mfxMemId memId)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    mfxStatus sts = MFX_ERR_NONE;
    HRESULT hr = S_OK;
    mfxFrameSurface1* srf = NULL;
    IDirect3DSurface9* pSurface = NULL;

    CHECK_POINTER(memId, MFX_ERR_NULL_PTR);
    if (!m_bDoNotAlloc)
    {
        MFX_LTRACE_S(MF_TL_PERF, "HW Sample created");

        if (SUCCEEDED(hr) && !m_pComMfxSurface) hr = E_FAIL;
        if (SUCCEEDED(hr))
        {
            mfxHDL handle;
            m_pAlloc->GetHDL(m_pAlloc->pthis, memId, &handle);
            pSurface = reinterpret_cast<IDirect3DSurface9*>(handle);
            if (!pSurface) hr = E_FAIL;
        }
        if (SUCCEEDED(hr)) hr = MFCreateVideoSampleFromSurface(pSurface, &m_pSample);
        if (SUCCEEDED(hr)) hr = m_pSamplesPool->AddSample(m_pSample);
        if (SUCCEEDED(hr)) hr = m_pSample->SetUnknown(MF_MT_MFX_FRAME_SRF, m_pComMfxSurface);
        if (SUCCEEDED(hr))
        {
            srf = m_pComMfxSurface->GetMfxFrameSurface();
            srf->Data.MemId = memId;

            m_bDoNotAlloc = true;
            m_State = stSurfaceReady;
        }
        else
        {
            SAFE_RELEASE(m_pSample);
        }
    }
    else hr = E_FAIL;
    if (FAILED(hr)) sts = MFX_ERR_UNKNOWN;
    MFX_LTRACE_I(MF_TL_PERF, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

#if MFX_D3D11_SUPPORT
mfxStatus MFYuvOutSurface::AllocD3D11(mfxMemId memId, ID3D11Texture2D* p2DTexture)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    mfxStatus sts = MFX_ERR_NONE;
    HRESULT hr = S_OK;
    mfxFrameSurface1* srf = NULL;

    if (!m_bDoNotAlloc)
    {
        MFX_LTRACE_S(MF_TL_PERF, "HW Sample created");
        if (SUCCEEDED(hr) && !m_pComMfxSurface) hr = E_FAIL;
        if (SUCCEEDED(hr)) hr = MFCreateDXGISurfaceBuffer(IID_ID3D11Texture2D, p2DTexture, 0, false, &m_pMediaBuffer);
        if (SUCCEEDED(hr)) hr = MFCreateVideoSampleFromSurface(NULL, &m_pSample);
        if (SUCCEEDED(hr)) hr = m_pSample->AddBuffer(m_pMediaBuffer);
        if (SUCCEEDED(hr)) hr = m_pSamplesPool->AddSample(m_pSample);
        if (SUCCEEDED(hr)) hr = m_pSample->SetUnknown(MF_MT_MFX_FRAME_SRF, m_pComMfxSurface);
        if (SUCCEEDED(hr))
        {
            srf = m_pComMfxSurface->GetMfxFrameSurface();
            srf->Data.MemId = memId;

            m_bDoNotAlloc = true;
            m_State = stSurfaceReady;
        }
        else
        {
            SAFE_RELEASE(m_pSample);
            SAFE_RELEASE(m_pMediaBuffer);
        }
    }
    else hr = E_FAIL;
    if (FAILED(hr)) sts = MFX_ERR_UNKNOWN;
    MFX_LTRACE_I(MF_TL_PERF, sts);
    return sts;
}
#endif

/*------------------------------------------------------------------------------*/

mfxStatus MFYuvOutSurface::AllocSW(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    HRESULT hr = S_OK;
    mfxU8*  pData = NULL;
    LONG    uiPitch = 0;
    mfxU16  nWidth = 0, nHeight = 0;
    IMFMediaBuffer* pMediaBuffer = NULL;
    mfxFrameSurface1* srf = NULL;

    // trying to obtain surface from pool
    if (!m_bDoNotAlloc)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "SW Sample created");
        if (SUCCEEDED(hr) && !m_pComMfxSurface) hr = E_FAIL;
        // trying to obtain pointer to mfx surface
        if (SUCCEEDED(hr))
        {
            srf = m_pComMfxSurface->GetMfxFrameSurface();
            nWidth  = srf->Info.Width;
            nHeight = srf->Info.Height;
        }
        if (SUCCEEDED(hr)) hr = MFCreateVideoSampleFromSurface(NULL, &m_pSample);
        if (SUCCEEDED(hr)) hr = MFCreateVideoBuffer(nWidth, nHeight, m_nPitch, srf->Info.FourCC, &pMediaBuffer);
        if (SUCCEEDED(hr)) hr = m_pSample->AddBuffer(pMediaBuffer);
        if (SUCCEEDED(hr)) hr = m_pSamplesPool->AddSample(m_pSample);
        if (SUCCEEDED(hr)) hr = m_pSample->SetUnknown(MF_MT_MFX_FRAME_SRF, m_pComMfxSurface);
        if (SUCCEEDED(hr)) hr = pMediaBuffer->QueryInterface(IID_IMF2DBuffer, (void**)&m_p2DBuffer);
        SAFE_RELEASE(pMediaBuffer);
        // "Pretend" is workaround for MSFT components: removing any pretending for our code
        if (SUCCEEDED(hr)) hr = Pretend(0, 0);
        if (SUCCEEDED(hr)) hr = m_p2DBuffer->Lock2D((BYTE**)&pData, &uiPitch);

        m_bLocked = SUCCEEDED(hr);

        CHECK_EXPRESSION(SUCCEEDED(hr), MFX_ERR_UNKNOWN);
        CHECK_EXPRESSION(uiPitch == m_nPitch, MFX_ERR_UNKNOWN); // should not occur

        switch(srf->Info.FourCC)
        {
        case MFX_FOURCC_NV12:
            {
                srf->Data.Y  = pData;
                srf->Data.UV = pData + m_nPitch * nHeight;
            }
        }
        srf->Data.Pitch = m_nPitch;
        if (SUCCEEDED(hr))
        {
            m_bDoNotAlloc = true;
            m_State = stSurfaceReady;
        }
        else
        {
            SAFE_RELEASE(m_pSample);
        }
    }
    else hr = E_FAIL;
    if (FAILED(hr)) sts = MFX_ERR_UNKNOWN;
    MFX_LTRACE_I(MF_TL_PERF, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFYuvOutSurface::Alloc(mfxMemId memid)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    HRESULT hr = S_OK;

    CHECK_EXPRESSION(m_bInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_LTRACE_I(MF_TL_GENERAL, m_State);
    m_pSample = m_pSamplesPool->GetSample();
    if (m_pSample)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "Sample taken from pool");
        if (SUCCEEDED(hr)) hr = m_pSamplesPool->AddSample(m_pSample);
        if (SUCCEEDED(hr)) hr = m_pSample->GetUnknown(MF_MT_MFX_FRAME_SRF,
                                                      IID_IMfxFrameSurface,
                                                      (void**)&m_pComMfxSurface);
        if (SUCCEEDED(hr))
        {
            mfxFrameSurface1* srf = m_pComMfxSurface->GetMfxFrameSurface();
            if (srf->Data.Locked)
            {
                m_State = stSurfaceLocked;
                sts = MFX_ERR_LOCK_MEMORY;
            }
            else m_State = stSurfaceReady;
        }
        else
        {
            SAFE_RELEASE(m_pSample);
            SAFE_RELEASE(m_pMediaBuffer);
            SAFE_RELEASE(m_pComMfxSurface);
            sts = MFX_ERR_UNKNOWN;
        }
    }
    else
    {
#if MFX_D3D11_SUPPORT
        if(!m_bIsD3D9)
        {
            mfxHDLPair handlePair;
            ID3D11Texture2D * p2DTexture = NULL;

            if(memid)
            {
                m_pAlloc->GetHDL(m_pAlloc->pthis, memid, (mfxHDL *)&handlePair);
                p2DTexture = reinterpret_cast<ID3D11Texture2D *>(handlePair.first);
            }

            if (p2DTexture)
            {
                    p2DTexture->AddRef();
                    sts = AllocD3D11(memid, p2DTexture);
            }
            else sts = AllocSW();

            SAFE_RELEASE(p2DTexture);
        }
        else
#endif
        {
            if (memid) sts = AllocD3D9(memid);
            else sts = AllocSW();
        }
    }
    MFX_LTRACE_I(MF_TL_GENERAL, m_State);
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

void MFYuvOutSurface::Close(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    m_bInitialized = false;
}

/*------------------------------------------------------------------------------*/

HRESULT MFYuvOutSurface::Sync(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    mfxFrameSurface1* srf = NULL;

    CHECK_EXPRESSION(m_bInitialized, E_FAIL);
    CHECK_POINTER(m_pComMfxSurface, E_POINTER); // should not occur

    srf = m_pComMfxSurface->GetMfxFrameSurface();
    if (!m_bFake)
    {
        if (!(srf->Data.MemId))
        {
            // SW only part
            switch(srf->Info.FourCC)
            {
            case MFX_FOURCC_NV12:
                DumpYuvFromNv12Data(srf->Data.Y, &(srf->Info), m_nPitch);
                break;
            case MFX_FOURCC_YUY2:
                DumpYuvFromYUY2Data(&(srf->Data), &(srf->Info));
                break;
            default:
                // will not occurr
                break;
            }
            // here sample should be already created and buffer locked
            if (m_bLocked)
            {
                m_p2DBuffer->Unlock2D();
                m_bLocked = false;
            }
        }
        else if (IsFileSet() && (MFX_FOURCC_NV12 == srf->Info.FourCC || MFX_FOURCC_YUY2 == srf->Info.FourCC))
        {
            mfxFrameInfo * pInfo = &srf->Info;
            mfxFrameData * pData = &srf->Data;
            mfxStatus sts = MFX_ERR_NONE;
            sts = m_pAlloc->Lock(m_pAlloc->pthis, srf->Data.MemId, &srf->Data);

            if(MFX_ERR_NONE == sts)
            {
                switch(srf->Info.FourCC)
                {
                    case MFX_FOURCC_NV12:
                    {
                        DumpYuvFromNv12Data(srf->Data.Y, pInfo, pData->Pitch);
                        break;
                    }
                    case MFX_FOURCC_YUY2:
                    {
                        DumpYuvFromYUY2Data(pData, pInfo);
                        break;
                    }
                }
            }
            if(MFX_ERR_NONE == sts) sts = m_pAlloc->Unlock(m_pAlloc->pthis, srf->Data.MemId, &srf->Data);
            MFX_LTRACE_I(MF_TL_GENERAL, sts);

        }
    }
    if (SUCCEEDED(hr) && m_pSample)
    {
        HRESULT hr_sts = S_OK;
        mfxF64 framerate = (mfxF64)srf->Info.FrameRateExtN/(mfxF64)srf->Info.FrameRateExtD;

        mfxU16 imode = srf->Info.PicStruct;
        UINT32 bInterlaced = (((MFX_PICSTRUCT_FIELD_TFF & imode) ||
                               (MFX_PICSTRUCT_FIELD_BFF & imode)) &&
                               !(MFX_PICSTRUCT_PROGRESSIVE & imode))? TRUE: FALSE;
        UINT32 bBFF = (MFX_PICSTRUCT_FIELD_BFF & imode)? TRUE: FALSE;
        UINT32 bRepeated = (MFX_PICSTRUCT_FIELD_REPEATED & imode)? TRUE: FALSE;

        MFX_LTRACE_I(MF_TL_GENERAL, bInterlaced);
        MFX_LTRACE_I(MF_TL_GENERAL, bBFF);
        MFX_LTRACE_I(MF_TL_GENERAL, bRepeated);

        // we will always set these attributes, overwise MF core may set them and canflict may occur
        hr_sts = m_pSample->SetUINT32(MFSampleExtension_Interlaced, bInterlaced);
        if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
        hr_sts = m_pSample->SetUINT32(MFSampleExtension_BottomFieldFirst, bBFF);
        if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
        hr_sts = m_pSample->SetUINT32(MFSampleExtension_RepeatFirstField, bRepeated);
        if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;

        if (MF_TIME_STAMP_INVALID != srf->Data.TimeStamp)
        {
            hr_sts = m_pSample->SetSampleTime(MFX2REF_TIME(srf->Data.TimeStamp));
            if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
            MFX_LTRACE_F(MF_TL_GENERAL, REF2SEC_TIME(MFX2REF_TIME(srf->Data.TimeStamp)));
        }
        else
        {
            hr_sts = m_pSample->SetSampleTime(SEC2REF_TIME(0));
            if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
            MFX_LTRACE_S(MF_TL_GENERAL, "invalid time stamp (SampleTime = 0.0)");
        }

        REFERENCE_TIME duration = 0;
        if (FAILED(m_pSample->GetSampleDuration(&duration)))
        {
            hr_sts = m_pSample->SetSampleDuration(SEC2REF_TIME(1/framerate));
            if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
        }

        hr_sts = m_pSample->SetUINT32(MF_MT_FAKE_SRF, (m_bFake)? TRUE: FALSE);
        if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;

        hr_sts = m_pSample->SetUINT32(MFSampleExtension_Discontinuity, (m_bGap)? TRUE: FALSE);
        if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;

        hr_sts = m_pSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
        if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;

        if (SUCCEEDED(hr))
        {
            mfxU8 bitsPerPixel = MFX_FOURCC_NV12 == srf->Info.FourCC ? 12 : (MFX_FOURCC_YUY2 == srf->Info.FourCC ? 16 : 0);

            IMFMediaBuffer* pMediaBuffer = NULL;
            hr_sts = m_pSample->GetBufferByIndex(0, &pMediaBuffer);
            if (SUCCEEDED(hr_sts) && pMediaBuffer)
            {
                mfxU32 mediaBufferLength = srf->Info.Width * srf->Info.Height * bitsPerPixel / 8;
                hr_sts = pMediaBuffer->SetCurrentLength(mediaBufferLength);
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
                MFX_LTRACE_I(MF_TL_GENERAL, mediaBufferLength);
            }
        }
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/
// Release of objects
// Surface should be unlocked by decoder or vpp

HRESULT MFYuvOutSurface::Release(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    mfxStatus sts = MFX_ERR_NONE;

    CHECK_EXPRESSION(m_bInitialized, E_FAIL);
    MFX_LTRACE_I(MF_TL_GENERAL, m_State);
    switch (m_State)
    {
    case stSurfaceFree:
        {
            // allocating new surface
            sts = Alloc(NULL);
            if (MFX_ERR_NONE != sts) hr = E_FAIL;
        }
        break;
    case stSurfaceLocked:
        {
            mfxFrameSurface1* srf = m_pComMfxSurface->GetMfxFrameSurface();
            if (srf->Data.Locked)
            {
                hr = E_FAIL;
                MFX_LTRACE_S(MF_TL_NOTES, "Surface is locked!");
            }
            else m_State = stSurfaceReady;
        }
        break;
    case stSurfaceReady:
        {
            mfxFrameSurface1* srf = m_pComMfxSurface->GetMfxFrameSurface();
            if (srf->Data.Locked)
            {
                m_State = stSurfaceLocked;
                hr = E_FAIL;
                MFX_LTRACE_S(MF_TL_NOTES, "Surface is locked!");
            }
        }
        break;
    case stSurfaceNotDisplayed:
        {
            hr = E_FAIL;
        }
        break;
    case stSurfaceDisplayed:
        {
            if (m_bLocked && m_p2DBuffer)
            {
                m_p2DBuffer->Unlock2D();
                m_bLocked = false;
            }
            SAFE_RELEASE(m_pComMfxSurface);
            SAFE_RELEASE(m_p2DBuffer);
            SAFE_RELEASE(m_pSample);
            SAFE_RELEASE(m_pMediaBuffer);
            m_State = stSurfaceFree;

            // allocating new surface
            sts = Alloc(NULL);
            if (MFX_ERR_NONE != sts) hr = E_FAIL;
        }
        break;
    };
    MFX_LTRACE_I(MF_TL_GENERAL, m_State);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}
