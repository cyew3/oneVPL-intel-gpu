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

File: mf_yuv_ibuf.cpp

Purpose: define code for support of input buffering in Intel MediaFoundation
encoder and vpp plug-ins.

*********************************************************************************/
#include "mf_yuv_ibuf.h"
#include "mf_vbuf.h"
#include "mf_hw_platform.h"
#if MFX_D3D11_SUPPORT
#include "d3d11_allocator.h"
#endif

/*------------------------------------------------------------------------------*/

#undef  MFX_TRACE_CATEGORY
#define MFX_TRACE_CATEGORY _T("mfx_mft_unk")

/*------------------------------------------------------------------------------*/

MFYuvInSurface::MFYuvInSurface(void):
    m_pSample(NULL),
    m_pMediaBuffer(NULL),
    m_p2DBuffer(NULL),
    m_pComMfxSurface(NULL),
    m_bAllocated(false),
    m_bLocked(false),
    m_bReleased(true),
    m_bFake(false),
    m_bInitialized(false),
    m_pVideo16(NULL),
    m_nVideo16AllocatedSize(0),
    m_nPitch(0),
    m_pDevice(),
    m_pAlloc(),
    m_EncodeCtrl(NULL),
    m_pMFAllocator(),
    //falback always true for non VLV platform
    m_bFallBackToNonCMCopy(HWPlatform::Type() != HWPlatform::VLV)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    memset((void*)&m_mfxSurface, 0, sizeof(mfxFrameSurface1));
    memset((void*)&m_mfxInSurface, 0, sizeof(mfxFrameSurface1));
}

/*--------------------------------------------------------------------*/

MFYuvInSurface::~MFYuvInSurface(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (m_bLocked && m_pMediaBuffer)
    {
        m_pMediaBuffer->Unlock();
        m_bLocked = false;
    }
    if (m_bLocked && m_p2DBuffer)
    {
        m_p2DBuffer->Unlock2D();
        m_bLocked = false;
    }
    SAFE_RELEASE(m_pComMfxSurface);
    SAFE_RELEASE(m_pMediaBuffer);
    SAFE_RELEASE(m_p2DBuffer);
    SAFE_RELEASE(m_pSample);
    if (NULL != m_pVideo16)
    {
        SAFE_FREE(m_pVideo16);
        MFX_LTRACE_P(MF_TL_NOTES, m_pVideo16);
        m_nVideo16AllocatedSize = 0;
    }
    m_EncodeCtrl.Free();

    Close();
}

/*--------------------------------------------------------------------*/

// Notes:
//  - pInfo - on surface with such info MediaSDK can work
//  - pInInfo - this info of surface which actually arrived
//  - pInfo may differ from pInInfo only in width and height fields,
// cropping parameters are equal; besides width and height in pInInfo
// always le than in pInfo
mfxStatus MFYuvInSurface::Init(mfxFrameInfo* pInfo,
                               mfxFrameInfo* pInInfo,
                               mfxMemId memid,
                               bool bAlloc,
                               IUnknown * pDevice,
                               MFFrameAllocator *pAlloc)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    CHECK_EXPRESSION(!m_bInitialized, MFX_ERR_ABORTED);
    CHECK_POINTER(pInfo, MFX_ERR_NULL_PTR);
    if (memid)
        bAlloc = false; // no allocations in HW mode

    memcpy_s(&(m_mfxSurface.Info), sizeof(m_mfxSurface.Info), pInfo, sizeof(mfxFrameInfo));
    if (pInInfo)
        memcpy_s(&(m_mfxInSurface.Info), sizeof(m_mfxInSurface.Info), pInInfo, sizeof(mfxFrameInfo));
    else
        memcpy_s(&(m_mfxInSurface.Info), sizeof(m_mfxInSurface.Info), pInfo, sizeof(mfxFrameInfo));

    CHECK_EXPRESSION((MFX_FOURCC_NV12 == m_mfxSurface.Info.FourCC ||
                      MFX_FOURCC_YUY2 == m_mfxSurface.Info.FourCC), MFX_ERR_ABORTED);
    CHECK_EXPRESSION((MFX_CHROMAFORMAT_YUV420 == m_mfxSurface.Info.ChromaFormat ||
                      MFX_CHROMAFORMAT_YUV422 == m_mfxSurface.Info.ChromaFormat), MFX_ERR_ABORTED);

    memset(&(m_mfxSurface.Data),0,sizeof(mfxFrameData));

    if (memid)
    {
        m_bAllocated = true;
        m_mfxSurface.Data.MemId = memid;
    }
    else
    {
        mfxU16 align = m_gMemAlignDec; // default: request for [vpp/enc in] srf from our decoder

        if (bAlloc)
            align = m_gMemAlignEnc; // request for [vpp out] = [enc in] srf
        else if (pInInfo)
            align = m_gMemAlignVpp; // request for [vpp/enc in] srf from not our decoder
        m_nPitch = (mfxU16)(MEM_ALIGN(m_mfxSurface.Info.Width, align));

        if (bAlloc || pInInfo &&
                      (m_mfxInSurface.Info.Width < m_mfxSurface.Info.Width) ||
                      (m_mfxInSurface.Info.Height < m_mfxSurface.Info.Height))
        {
            switch(m_mfxSurface.Info.FourCC)
            {
                case MFX_FOURCC_NV12:
                    MFX_LTRACE_S(MF_TL_NOTES, "NV12: m_pVideo16 = calloc(3*m_nPitch*m_mfxSurface.Info.Height/2)...");
                    m_nVideo16AllocatedSize = 3*m_nPitch*m_mfxSurface.Info.Height/2;
                    break;
                case MFX_FOURCC_YUY2:
                    MFX_LTRACE_S(MF_TL_NOTES, "YUY2: m_pVideo16 = calloc(2*m_nPitch*m_mfxSurface.Info.Height)...");
                    m_nVideo16AllocatedSize = 2*m_nPitch*m_mfxSurface.Info.Height;
                    break;
            };
            MFX_LTRACE_I(MF_TL_NOTES, m_nVideo16AllocatedSize);
            MFX_LTRACE_I(MF_TL_NOTES, m_nPitch);
            MFX_LTRACE_I(MF_TL_NOTES, m_mfxSurface.Info.Height);
            m_pVideo16 = (mfxU8*)calloc(m_nVideo16AllocatedSize,sizeof(mfxU8));
            MFX_LTRACE_P(MF_TL_NOTES, m_pVideo16);
            if (NULL == m_pVideo16)
            {
                m_nVideo16AllocatedSize = 0;
                ATLASSERT(false);
                MFX_LTRACE_S(MF_TL_NOTES, "error allocating memory, return MFX_ERR_MEMORY_ALLOC");
                return MFX_ERR_MEMORY_ALLOC;
            }
        }
        if (bAlloc)
        {
            m_bAllocated = true;

            switch(m_mfxSurface.Info.FourCC)
            {
                case MFX_FOURCC_NV12:
                    m_mfxSurface.Data.Y     = m_pVideo16;
                    m_mfxSurface.Data.UV    = m_pVideo16 + m_nPitch * m_mfxSurface.Info.Height;
                    m_mfxSurface.Data.Pitch = m_nPitch;
                    break;
                case MFX_FOURCC_YUY2:
                    m_mfxSurface.Data.Y     = m_pVideo16;
                    m_mfxSurface.Data.U     = m_pVideo16 + 1;
                    m_mfxSurface.Data.V     = m_pVideo16 + 3;
                    m_mfxSurface.Data.Pitch = 2 * m_nPitch;
                    break;
            };
        }
    }
    m_pDevice = pDevice;
    m_pAlloc = (NULL != pAlloc ? pAlloc->GetMFXAllocator() : NULL);
    m_pMFAllocator = pAlloc;
    CHECK_EXPRESSION(MFX_ERR_NONE == sts, sts);
    m_bInitialized = true;
    MFX_LTRACE_S(MF_TL_GENERAL, "MFX_ERR_NONE");
    return MFX_ERR_NONE;
}

/*--------------------------------------------------------------------*/

void MFYuvInSurface::Close(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    m_bInitialized = false;
    if (NULL != m_pVideo16)
    {
        SAFE_FREE(m_pVideo16);
        MFX_LTRACE_P(MF_TL_NOTES, m_pVideo16);
        m_nVideo16AllocatedSize = 0;
    }
}

/*--------------------------------------------------------------------*/

void MFYuvInSurface::DropDataToFile(void)
{
#if !MFX_D3D11_SUPPORT
    if (IsFileSet())
    {
        MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
        mfxHDL handle;
        m_pAlloc->GetHDL(m_pAlloc->pthis, memId, &handle);
        IDirect3DSurface9* pSurface = reinterpret_cast<IDirect3DSurface9*>(handle);

        if (pSurface)
        {
            HRESULT hr_sts = S_OK;
            D3DSURFACE_DESC d3d_desc;
            D3DLOCKED_RECT  d3d_rect = {NULL,0};

            memset(&d3d_desc, 0, sizeof(D3DSURFACE_DESC));
            if (SUCCEEDED(hr_sts))
                hr_sts = pSurface->GetDesc(&d3d_desc);
            if (SUCCEEDED(hr_sts))
                hr_sts = pSurface->LockRect(&d3d_rect, NULL, D3DLOCK_READONLY);

            MFX_LTRACE_D(MF_TL_GENERAL, d3d_desc.Format);
            MFX_LTRACE_D(MF_TL_GENERAL, MAKEFOURCC('N', 'V', '1', '2'));
            MFX_LTRACE_2(MF_TL_GENERAL, "HW surface: Width x Height = ", "%d x %d", d3d_desc.Width, d3d_desc.Height);
            MFX_LTRACE_I(MF_TL_GENERAL, d3d_rect.Pitch);

            if (SUCCEEDED(hr_sts) && (MAKEFOURCC('N', 'V', '1', '2') == d3d_desc.Format))
            {
                mfxFrameInfo info;

                memset(&info, 0, sizeof(mfxFrameInfo));
                info.Width  = (mfxU16)d3d_desc.Width;
                info.Height = (mfxU16)d3d_desc.Height;
                info.CropW  = (mfxU16)d3d_desc.Width;
                info.CropH  = (mfxU16)d3d_desc.Height;
                DumpYuvFromNv12Data((mfxU8*)d3d_rect.pBits, &info, d3d_rect.Pitch);
            }
            if (SUCCEEDED(hr_sts))
                hr_sts = pSurface->UnlockRect();
            MFX_LTRACE_D(MF_TL_GENERAL, hr_sts);
        }
        else
        {
            DumpYuvFromNv12Data(m_mfxSurface.Data.Y, &(m_mfxInSurface.Info), m_nPitch);
        }
    }
#endif
}
/*--------------------------------------------------------------------*/

mfxU32 GetPlaneSize(mfxU16 nChromaFormat, mfxU32 nFourCC, mfxU16 nPitch, mfxU16 nHeight)
{
    //number of bits per pixel*2
    mfxU32 bpp_m2 = 1;
    bool isGray = MFX_CHROMAFORMAT_MONOCHROME == nChromaFormat;
    if (isGray)
    {
        bpp_m2 = 2;
    }
    else
    {
        switch (nFourCC)
        {
        case MFX_FOURCC_YV12:
        case MFX_FOURCC_NV12:
            bpp_m2 = 3;
            break;
        case MFX_FOURCC_RGB3:
            bpp_m2 = 6;
            break;
        case MFX_FOURCC_RGB4:
            bpp_m2 = 8;
            break;
        case MFX_FOURCC_YUY2:
            bpp_m2 = 4;
            break;
        default:
            bpp_m2 = 8;
            break;
        }
    }

    return (bpp_m2 * nPitch * nHeight) >> 1;
}

/*--------------------------------------------------------------------*/

HRESULT MFYuvInSurface::LoadD3D11()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    HRESULT hr = S_OK;
#if MFX_D3D11_SUPPORT
    //most of cases below dereferece m_pAlloc. May require changes if MF_D3D11_COPYSURFACES is not defined.
    ATLASSERT(NULL != m_pAlloc);
    CHECK_POINTER(m_pAlloc, E_POINTER);

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameSurface1* srf = &m_mfxSurface;

    CComQIPtr<ID3D11Device> pD3d11Device(m_pDevice);
    MFX_LTRACE_P(MF_TL_GENERAL, pD3d11Device.p);
    if (!pD3d11Device)
    {
        return E_FAIL;
    }
    CComPtr<ID3D11DeviceContext> pCtx;
    pD3d11Device->GetImmediateContext(&pCtx);
    MFX_LTRACE_P(MF_TL_GENERAL, pCtx.p);
    if (!pCtx)
    {
        return E_FAIL;
    }

    CComQIPtr<IMFDXGIBuffer> dxgiBuffer(m_pMediaBuffer);
    CComPtr<ID3D11Texture2D> p2DTexture;

    if (!dxgiBuffer)
    {
        MFX_AUTO_LTRACE(MF_TL_GENERAL, "copy from system memory");
        MFX_LTRACE_WS(MF_TL_GENERAL, L"QueryInterface IMFDXGIBuffer - failed, copying input using system memory");

        if (MFX_ERR_NONE != (sts = m_pAlloc->Lock(m_pAlloc->pthis, MFXReadWriteMid(srf->Data.MemId, MFXReadWriteMid::write), &srf->Data)))
        {
            MFX_LTRACE_1(MF_TL_GENERAL, "m_pAlloc->Lock()=", "%d", sts);
            return E_FAIL;
        }


        if (NULL != m_pVideo16)
        {
            // must not get here
            MFX_AUTO_LTRACE(MF_TL_GENERAL, "NULL != m_pVideo16 before m_pVideo16 = srf->Data.Y");
            ATLASSERT(false);
            hr = E_FAIL;
        }
        else
        {
            //special hack to no touch loadsw function behavior
            m_pVideo16 = srf->Data.Y;
            m_nPitch = srf->Data.Pitch;

            //allocator creates surface of a bigger resolution, so to provide offset to uv plane we have to cpecify resolution
            srf->Info.Height = (srf->Data.UV - srf->Data.Y) / srf->Data.Pitch;
            m_nVideo16AllocatedSize = GetPlaneSize(srf->Info.ChromaFormat, srf->Info.FourCC, m_nPitch, srf->Info.Height);

            hr = LoadSW();

            //parameters clearing
            m_bLocked = false;
            m_pVideo16 = NULL;
            m_nVideo16AllocatedSize = 0;
            MFX_LTRACE_D(MF_TL_GENERAL, hr);
        }

        sts = m_pAlloc->Unlock(m_pAlloc->pthis, MFXReadWriteMid(srf->Data.MemId, MFXReadWriteMid::write), &srf->Data);
    }
#ifdef MF_D3D11_COPYSURFACES
    else
    {
        mfxHDLPair pair;
        m_pAlloc->GetHDL(m_pAlloc->pthis, srf->Data.MemId, (mfxHDL*)&pair);


        UINT srcSubresourceIdx = 0u;
        hr = dxgiBuffer->GetResource(IID_ID3D11Texture2D, (LPVOID*)&p2DTexture);
        if (FAILED(hr))
        {
            MFX_LTRACE_1(MF_TL_GENERAL, "dxgiBuffer->GetResource()=", "%d", hr);
            return hr;
        }
        hr = dxgiBuffer->GetSubresourceIndex(&srcSubresourceIdx);
        if (FAILED(hr))
        {
            MFX_LTRACE_1(MF_TL_GENERAL, "dxgiBuffer->GetSubresourceIndex()=", "%d", hr);
            return hr;
        }

#ifdef CM_COPY_RESOURCE
       if (!m_bFallBackToNonCMCopy && m_pMFAllocator)
       {
            MFX_AUTO_LTRACE(MF_TL_GENERAL, "CmCopyVideoToVideoMemory");
            CmCopyWrapper<ID3D11Device> &cmCopier = m_pMFAllocator->CreateCMCopyWrapper(pD3d11Device);
            mfxStatus sts;
            if (MFX_ERR_NONE != (sts = cmCopier.CopyVideoToVideoMemory(CComPtr <ID3D11Texture2D> ((ID3D11Texture2D*)pair.first), (int)pair.second, p2DTexture, srcSubresourceIdx))) {
                MFX_LTRACE_1(MF_TL_GENERAL, "cmCopier.CopyVideoToVideoMemory=", "%d", sts);
                m_bFallBackToNonCMCopy = true;
            }
       }
       if (m_bFallBackToNonCMCopy || !m_pMFAllocator)
#endif
        {
            MFX_AUTO_LTRACE(MF_TL_GENERAL, "CopySubresourceRegion");
            pCtx->CopySubresourceRegion((ID3D11Texture2D*)pair.first, (UINT)pair.second, 0, 0, 0, p2DTexture, srcSubresourceIdx, NULL);
        }
    }

    if (IsFileSet())
    {
        if (p2DTexture.p)
        {
            mfxFrameData data;
            mfxStatus sts;
            sts = m_pAlloc->Lock(m_pAlloc->pthis, srf->Data.MemId, &data);

            D3D11_TEXTURE2D_DESC desc;
            p2DTexture->GetDesc(&desc);

            mfxFrameInfo info = {0};
            info.Width = info.CropW = desc.Width;
            info.Height = info.CropH = desc.Height;
            DumpYuvFromNv12Data((mfxU8*)data.Y, &info, data.Pitch);

            sts = m_pAlloc->Unlock(m_pAlloc->pthis, srf->Data.MemId, &data);
        }
        else
        {
            //data after copying
            if (MFX_ERR_NONE != (sts = m_pAlloc->Lock(m_pAlloc->pthis, srf->Data.MemId, &srf->Data)))
            {
                MFX_LTRACE_1(MF_TL_GENERAL, "m_pAlloc->Lock()=", "%d", sts);
                return E_FAIL;
            }

            DumpYuvFromNv12Data((mfxU8*)srf->Data.Y, &srf->Info, srf->Data.Pitch);

            sts = m_pAlloc->Unlock(m_pAlloc->pthis, srf->Data.MemId, &srf->Data);
        }
    }
#else // ifdef MF_D3D11_COPYSURFACES
    else
    {
        m_dxgiBuffer = dxgiBuffer;
        srf->Data.MemId = m_dxgiBuffer.p;
    }
#endif// ifdef MF_D3D11_COPYSURFACES

#endif// if MFX_D3D11_SUPPORT
    return hr;
}

/*--------------------------------------------------------------------*/

HRESULT MFYuvInSurface::LoadSW(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    mfxU8*  pData = NULL;
    mfxU32  uiDataLen = 0, uiDataSize = 0;
    LONG    uiPitch = 0;
    mfxU16  nHeight = 0;
    mfxU16  nOriginalHeight = 0;
    mfxU16  nCropX = 0, nCropY  = 0;
    mfxU16  nCropW = 0, nCropH  = 0;

    if (SUCCEEDED(hr))
    {
        m_pMediaBuffer->QueryInterface(IID_IMF2DBuffer, (void**)&m_p2DBuffer);
        if (m_p2DBuffer)
        {
            hr = m_p2DBuffer->Lock2D((BYTE**)&pData, &uiPitch);
        }
        if (FAILED(hr))
        {
            MFX_LTRACE_1(MF_TL_GENERAL, "m_p2DBuffer->Lock2D((BYTE**)&pData, &uiPitch)=", "%d", hr);
            hr = m_p2DBuffer->Unlock2D();
            SAFE_RELEASE(m_p2DBuffer);
        }
        if (NULL == m_p2DBuffer)
        {
            hr = m_pMediaBuffer->Lock((BYTE**)&pData, (DWORD*)&uiDataLen, (DWORD*)&uiDataSize);
            if (FAILED(hr))
            {
                MFX_LTRACE_1(MF_TL_GENERAL, "m_pMediaBuffer->Lock((BYTE**)&pData, (DWORD*)&uiDataLen, (DWORD*)&uiDataSize)=", "%d", hr);
            }
            uiPitch = m_mfxInSurface.Info.Width;
            ATLASSERT(uiDataSize == (mfxU32)(m_mfxInSurface.Info.Width)*m_mfxInSurface.Info.Height*3/2);
        }
    }
    m_bLocked = SUCCEEDED(hr);
    if (SUCCEEDED(hr) && (uiPitch != m_nPitch))
    {
        // should not occur in case of our decoder-(vpp)-encoder
        if (!m_pVideo16)
        {
            MFX_LTRACE_S(MF_TL_NOTES, "m_pVideo16 = calloc(3*m_nPitch*m_mfxSurface.Info.Height/2)...");
            MFX_LTRACE_I(MF_TL_NOTES, m_nPitch);
            MFX_LTRACE_I(MF_TL_NOTES, m_mfxSurface.Info.Height);
            m_nVideo16AllocatedSize = 3*m_nPitch*m_mfxSurface.Info.Height/2;
            MFX_LTRACE_I(MF_TL_NOTES, m_nVideo16AllocatedSize);
            m_pVideo16 = (mfxU8*)calloc(m_nVideo16AllocatedSize,sizeof(mfxU8));
            MFX_LTRACE_P(MF_TL_NOTES, m_pVideo16);
            if (NULL == m_pVideo16)
            {
                m_nVideo16AllocatedSize = 0;
                ATLASSERT(false);
                MFX_LTRACE_S(MF_TL_NOTES, "error allocating memory, return MFX_ERR_MEMORY_ALLOC");
                return MFX_ERR_MEMORY_ALLOC;
            }
        }
    }
    if (SUCCEEDED(hr))
    {
        nHeight  = m_mfxSurface.Info.Height;
        nOriginalHeight = m_mfxInSurface.Info.Height;
        nCropX   = m_mfxSurface.Info.CropX;
        nCropY   = m_mfxSurface.Info.CropY;
        nCropW   = m_mfxSurface.Info.CropW;
        nCropH   = m_mfxSurface.Info.CropH;

        switch(m_mfxSurface.Info.FourCC)
        {
        case MFX_FOURCC_NV12:
            {
                if (!m_pVideo16)
                {
                    m_mfxSurface.Data.Y  = pData;
                    m_mfxSurface.Data.UV = pData + m_nPitch * nHeight;
                }
                else
                {
                    mfxU16 i = 0;

                    m_mfxSurface.Data.Y  = m_pVideo16;
                    m_mfxSurface.Data.UV = m_pVideo16 + m_nPitch * nHeight;
                    m_mfxInSurface.Data.Y  = pData;
                    m_mfxInSurface.Data.UV = pData + uiPitch * nOriginalHeight;
                    for (i = 0; i < nCropH/2; ++i)
                    {
                        // copying Y
                        memcpy_s(m_mfxSurface.Data.Y + nCropX + (nCropY + i)*m_nPitch,
                                 m_nVideo16AllocatedSize - (nCropX + (nCropY + i)*m_nPitch),
                                 m_mfxInSurface.Data.Y + nCropX + (nCropY + i)*uiPitch,
                                 nCropW);
                        // copying UV
                        memcpy_s(m_mfxSurface.Data.UV + nCropX + (nCropY/2 + i)*m_nPitch,
                                 m_nVideo16AllocatedSize - m_nPitch * nHeight - (nCropX + (nCropY/2 + i)*m_nPitch),
                                 m_mfxInSurface.Data.UV + nCropX + (nCropY/2 + i)*uiPitch,
                                 nCropW);
                    }
                    for (i = nCropH/2; i < nCropH; ++i)
                    {
                        // copying Y
                        memcpy_s(m_mfxSurface.Data.Y + nCropX + (nCropY + i)*m_nPitch,
                                 m_nVideo16AllocatedSize - (nCropX + (nCropY + i)*m_nPitch),
                                 m_mfxInSurface.Data.Y + nCropX + (nCropY + i)*uiPitch,
                                 nCropW);
                    }
                }

                //Set Pitch
                m_mfxSurface.Data.Pitch = m_nPitch; //factor = 1 for NV12 and YV12
                break;
            }
        case MFX_FOURCC_YUY2:
            {
                if (!m_pVideo16)
                {
                    m_mfxSurface.Data.Y = pData;
                    m_mfxSurface.Data.U = pData + 1;
                    m_mfxSurface.Data.V = pData + 3;
                }
                else
                {
                    m_mfxSurface.Data.Y = m_pVideo16;
                    m_mfxSurface.Data.U = m_pVideo16 + 1;
                    m_mfxSurface.Data.V = m_pVideo16 + 3;

                    m_mfxInSurface.Data.Y = pData;
                    m_mfxInSurface.Data.U = pData + 1;
                    m_mfxInSurface.Data.V = pData + 3;

                    for (mfxU16 i = 0; i < nCropH; ++i)
                    {
                        memcpy_s(m_mfxSurface.Data.Y + nCropX + (nCropY + i) * 2 * m_nPitch,
                                 m_nVideo16AllocatedSize - (nCropX + (nCropY + i) * 2 * m_nPitch),
                                 m_mfxInSurface.Data.Y + nCropX + (nCropY + i) * 2 * uiPitch,
                                 2 * nCropW);
                    }
                }

                //Set Pitch
                m_mfxSurface.Data.Pitch = 2 * m_nPitch; //factor = 2 for YUY2

                break;
            }
        default:
            {
                // will not occurr
                break;
            }
        }
        DropDataToFile();
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*--------------------------------------------------------------------*/

HRESULT MFYuvInSurface::PseudoLoad(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    CHECK_EXPRESSION(m_bInitialized, E_FAIL);
    CHECK_EXPRESSION(!m_mfxSurface.Data.Locked, E_FAIL);

    m_bReleased = false;

    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*--------------------------------------------------------------------*/

HRESULT MFYuvInSurface::Load(IMFSample* pSample, bool bIsIntetnalSurface)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT  hr = S_OK;
    DWORD    cBuffers = 0;
    LONGLONG hnsTime = -1;
    UINT32 bSingleField = false; // (!) should be false
    UINT32 bInterlaced = false, bBFF = false, bRepeated = false;
    mfxFrameSurface1* srf = &m_mfxSurface;

    CHECK_EXPRESSION(m_bInitialized, E_FAIL);
#if !MFX_D3D11_SUPPORT
    CHECK_EXPRESSION(!m_bAllocated, E_FAIL); // we do not support loading under alloc mode
#endif
    CHECK_EXPRESSION(!m_bLocked, E_FAIL);
    CHECK_POINTER(!m_pComMfxSurface, E_FAIL);
    CHECK_POINTER(pSample, E_POINTER);

    pSample->AddRef();
    m_pSample = pSample;
    m_bReleased = false;

    if (SUCCEEDED(hr))
        pSample->GetUINT32(MFSampleExtension_SingleField, &bSingleField);
    CHECK_EXPRESSION(!bSingleField, E_FAIL); // single fields are not supported
    if (SUCCEEDED(hr))
        hr = pSample->GetBufferCount(&cBuffers);
    ATLASSERT(1 == cBuffers);
    CHECK_EXPRESSION(1 == cBuffers, E_FAIL);

    if (SUCCEEDED(hr) && bIsIntetnalSurface)
    {
        HRESULT hr_sts = S_OK;
        UINT32 bFake = false;

        hr_sts = m_pSample->GetUnknown(MF_MT_MFX_FRAME_SRF,
                                       IID_IMfxFrameSurface,
                                       (void**)&m_pComMfxSurface);
        MFX_LTRACE_D(MF_TL_GENERAL, hr_sts);

        if (SUCCEEDED(m_pSample->GetUINT32(MF_MT_FAKE_SRF, &bFake)))
        {
            m_bFake = (TRUE == bFake)? true: false;
        }
        else m_bFake = false;
    }
    if (NULL == m_pComMfxSurface)
    { // loading data from sample
        if (SUCCEEDED(hr) && srf->Data.Locked)
            hr = E_FAIL;
        if (SUCCEEDED(hr))
            hr = pSample->GetBufferByIndex(0, &m_pMediaBuffer);

        if (SUCCEEDED(hr))
#if !MFX_D3D11_SUPPORT
            hr = LoadSW();
#else
            hr = LoadD3D11();
#endif
        // loading time
        if (SUCCEEDED(pSample->GetSampleTime(&hnsTime)))
        {
            srf->Data.TimeStamp = REF2MFX_TIME(hnsTime);
        }
        else
        {
            srf->Data.TimeStamp = MF_TIME_STAMP_INVALID;
        }
        pSample->GetUINT32(MFSampleExtension_Interlaced, &bInterlaced);
        MFX_LTRACE_I(MF_TL_GENERAL, bInterlaced);
        if (bInterlaced)
        { // interlacing
            pSample->GetUINT32(MFSampleExtension_BottomFieldFirst, &bBFF);
            pSample->GetUINT32(MFSampleExtension_RepeatFirstField, &bRepeated);

            MFX_LTRACE_I(MF_TL_GENERAL, bBFF);
            MFX_LTRACE_I(MF_TL_GENERAL, bRepeated);

            srf->Info.PicStruct = (bBFF)? (mfxU16)MFX_PICSTRUCT_FIELD_BFF:
                                          (mfxU16)MFX_PICSTRUCT_FIELD_TFF;
            if (bRepeated)
                srf->Info.PicStruct |= MFX_PICSTRUCT_FIELD_REPEATED;
        }
        else
        {
            srf->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }
    }

//in copy mode we dont use IMF2DBuffer and IMFMediaBuffer interfaces of input sample any more
#if MFX_D3D11_SUPPORT
    #ifdef MF_D3D11_COPYSURFACES
        SAFE_RELEASE(m_pMediaBuffer);
        SAFE_RELEASE(m_pSample);
        SAFE_RELEASE(m_p2DBuffer);
    #endif
#endif

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*--------------------------------------------------------------------*/
// Release of objects
// Surface should be unlocked by encoder or vpp

// bIgnoreLock=true is used when called from MFPluginVpp::ResetCodec

HRESULT MFYuvInSurface::Release(bool bIgnoreLock)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxFrameSurface1* srf = (m_pComMfxSurface)? m_pComMfxSurface->GetMfxFrameSurface(): &m_mfxSurface;

    CHECK_EXPRESSION(m_bInitialized, E_FAIL);
    CHECK_EXPRESSION(!m_bReleased, S_OK);
    if (!bIgnoreLock)
        CHECK_EXPRESSION(!srf->Data.Locked, E_FAIL); // this check can (and maybe should - for better performance) be deleted

#if !MFX_D3D11_SUPPORT
    if (!m_bAllocated)
#endif
    {
        if (m_bLocked && m_pMediaBuffer)
        {
            m_pMediaBuffer->Unlock();
            m_bLocked = false;
        }
        if (m_bLocked && m_p2DBuffer)
        {
            m_p2DBuffer->Unlock2D();
            m_bLocked = false;
        }
#if MFX_D3D11_SUPPORT
        m_dxgiBuffer.Release();
#endif
        SAFE_RELEASE(m_pComMfxSurface);
        SAFE_RELEASE(m_pMediaBuffer);
        SAFE_RELEASE(m_p2DBuffer);
        SAFE_RELEASE(m_pSample);
        m_EncodeCtrl.Free();
    }
    m_bReleased = true;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}
