// Copyright (c) 2007-2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#if defined  (MFX_D3D11_ENABLED) && defined (MFX_DX9ON11)
#include <windows.h>
#include <initguid.h>
#include <DXGI.h>

#include "mfxpcp.h"
#include <libmfx_core_d3d9on11.h>
#include <libmfx_core_d3d11.h>
#include <libmfx_core_d3d9.h>
#include "mfx_utils.h"
#include "mfx_session.h"
#include "libmfx_core_interface.h"
#include "umc_va_dxva2_protected.h"
#include "ippi.h"

#include "mfx_umc_alloc_wrapper.h"
#include "mfx_common_decode_int.h"
#include "libmfx_core_hw.h"

#include "cm_mem_copy.h"

#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N','V','1','2')
#define D3DFMT_P010 (D3DFORMAT)MAKEFOURCC('P','0','1','0')
#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y','V','1','2')
#define D3DFMT_IMC3 (D3DFORMAT)MAKEFOURCC('I','M','C','3')

#define D3DFMT_YUV400   (D3DFORMAT)MAKEFOURCC('4','0','0','P')
#define D3DFMT_YUV411   (D3DFORMAT)MAKEFOURCC('4','1','1','P')
#define D3DFMT_YUV422H  (D3DFORMAT)MAKEFOURCC('4','2','2','H')
#define D3DFMT_YUV422V  (D3DFORMAT)MAKEFOURCC('4','2','2','V')
#define D3DFMT_YUV444   (D3DFORMAT)MAKEFOURCC('4','4','4','P')

#define MFX_FOURCC_P8_MBDATA  (D3DFORMAT)MFX_MAKEFOURCC('P','8','M','B')

#define DXGI_FORMAT_BGGR MAKEFOURCC('I','R','W','0')
#define DXGI_FORMAT_RGGB MAKEFOURCC('I','R','W','1')
#define DXGI_FORMAT_GRBG MAKEFOURCC('I','R','W','2')
#define DXGI_FORMAT_GBRG MAKEFOURCC('I','R','W','3')

template class D3D9ON11VideoCORE_T<D3D11VideoCORE>;

std::mutex D3D9ON11VideoCORE_T<D3D11VideoCORE>::m_copyMutex;

template <class Base>
D3D9ON11VideoCORE_T<Base>::D3D9ON11VideoCORE_T(
    const mfxU32 adapterNum
    , const mfxU32 numThreadsAvailable
    , const mfxSession session
)
    : D3D11VideoCORE(adapterNum, numThreadsAvailable, session)
    , m_hDirectXHandle(INVALID_HANDLE_VALUE)
    , m_pDirect3DDeviceManager(nullptr)
    , m_hasDX9FrameAllocator(false)
    , m_dx9FrameAllocator()
{
    m_bCmUseCache = false;
}

template <class Base>
D3D9ON11VideoCORE_T<Base>::~D3D9ON11VideoCORE_T() {}

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::CreateVA(mfxVideoParam* param, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response, UMC::FrameAllocator* allocator)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9ON11VideoCORE_T<Base>::CreateVA");
    mfxStatus sts = MFX_ERR_NONE;

    sts = Base::CreateVA(param, request, response, allocator);
    MFX_CHECK_STS(sts);

    MFX_CHECK(!(param->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY) && !(param->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY), MFX_ERR_NONE);

    if (MFX_CODEC_AVC == param->mfx.CodecId ||
        MFX_CODEC_VC1 == param->mfx.CodecId ||
        MFX_CODEC_MPEG2 == param->mfx.CodecId)
        return MFX_ERR_NONE;

#ifdef MFX_ENABLE_MJPEG_VIDEO_DECODE
    // Wrap surfaces for this case will be allocated in VPP path
    if(param->mfx.CodecId == MFX_CODEC_JPEG && dynamic_cast<SurfaceSourceJPEG*>(allocator))
        return MFX_ERR_NONE;
#endif

    // Need to clear mapping tables to prevent incorrect decode wrap
    // when application inits decoder second time
    m_dx9MemIdMap.clear();
    m_dx9MemIdUsed.clear();

    sts = ConvertUMCStatusToMfx(m_pAccelerator->CreateWrapBuffers(this, request, response));
    MFX_CHECK_STS(sts);

    return sts;
}

template <class Base>
void D3D9ON11VideoCORE_T<Base>::ReleaseHandle()
{
    if (m_hdl)
    {
        ((IUnknown*)m_hdl)->Release();
        m_bUseExtManager = false;
        m_hdl = 0;
    }
}

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::SetHandle(mfxHandleType type, mfxHDL handle)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    try
    {
        // SetHandle should be first since 1.6 version
        bool isRequeredEarlySetHandle = (m_session->m_version.Major > 1 ||
            (m_session->m_version.Major == 1 && m_session->m_version.Minor >= 6));

        {
            MFX_CHECK(type == MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9,            MFX_ERR_INVALID_HANDLE)
            MFX_CHECK(!isRequeredEarlySetHandle || !m_pDirect3DDeviceManager, MFX_ERR_UNDEFINED_BEHAVIOR)
        }

        try
        {
#if defined (MFX_ENABLE_VPP)
            m_vpp_hw_resmng.Close();
#endif

            if (m_pDirect3DDeviceManager)
                Close();

            m_pDirect3DDeviceManager = (IDirect3DDeviceManager9*)handle;

            IDirect3DDevice9* pD3DDevice;
            D3DDEVICE_CREATION_PARAMETERS pParameters;
            HRESULT hr = m_pDirect3DDeviceManager->OpenDeviceHandle(&m_hDirectXHandle);
            if (FAILED(hr))
            {
                ReleaseHandle();
                MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
            }

            hr = m_pDirect3DDeviceManager->LockDevice(m_hDirectXHandle, &pD3DDevice, true);
            if (FAILED(hr))
            {
                m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
                ReleaseHandle();
                MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
            }

            hr = pD3DDevice->GetCreationParameters(&pParameters);
            if (FAILED(hr))
            {
                m_pDirect3DDeviceManager->UnlockDevice(m_hDirectXHandle, true);
                m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
                pD3DDevice->Release();
                ReleaseHandle();
                MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
            }

            if (!(pParameters.BehaviorFlags & D3DCREATE_MULTITHREADED))
            {
                m_pDirect3DDeviceManager->UnlockDevice(m_hDirectXHandle, true);
                m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
                pD3DDevice->Release();
                ReleaseHandle();
                MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
            }

            m_pDirect3DDeviceManager->UnlockDevice(m_hDirectXHandle, true);
            m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
            pD3DDevice->Release();
        }
        catch (...)
        {
            ReleaseHandle();
            MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        mfxStatus sts = InternalCreateDevice();
        MFX_CHECK_STS(sts);

        m_hdl =  m_pD11Device;

        m_bUseExtManager = true;

        return MFX_ERR_NONE;
    }
    catch (...)
    {
        ReleaseHandle();
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    return MFX_ERR_NONE;
}


template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::DoFastCopyWrapper(mfxFrameSurface1* pDst, mfxU16 dstMemType, mfxFrameSurface1* pSrc, mfxU16 srcMemType)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE_T<Base>::DoFastCopyWrapper");
    mfxStatus sts;

    mfxHDLPair srcHandle = {}, dstHandle = {};
    mfxMemId srcMemId, dstMemId;

    mfxFrameSurface1 srcTempSurface, dstTempSurface;

    memset(&srcTempSurface, 0, sizeof(mfxFrameSurface1));
    memset(&dstTempSurface, 0, sizeof(mfxFrameSurface1));

    // save original mem ids
    srcMemId = pSrc->Data.MemId;
    dstMemId = pDst->Data.MemId;

    mfxU8* srcPtr = GetFramePointer(pSrc->Info.FourCC, pSrc->Data);
    mfxU8* dstPtr = GetFramePointer(pDst->Info.FourCC, pDst->Data);

    srcTempSurface.Info = pSrc->Info;
    dstTempSurface.Info = pDst->Info;

    bool isSrcLocked = false;
    bool isDstLocked = false;

    if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        if (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == srcPtr)
            {
                sts = LockExternalFrame(srcMemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);

                isSrcLocked = true;
            }
            else
            {
                srcTempSurface.Data = pSrc->Data;
                srcTempSurface.Data.MemId = 0;
            }
        }
        else if (srcMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetExternalFrameHDL(srcMemId, (mfxHDL*)&srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = &srcHandle;
        }
    }
    else if (srcMemType & MFX_MEMTYPE_INTERNAL_FRAME)
    {
        if (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == srcPtr)
            {
                sts = LockFrame(srcMemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);

                isSrcLocked = true;
            }
            else
            {
                srcTempSurface.Data = pSrc->Data;
                srcTempSurface.Data.MemId = 0;
            }
        }
        else if (srcMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetFrameHDL(srcMemId, (mfxHDL*)&srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = &srcHandle;
        }
    }

    if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        if (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == dstPtr)
            {
                sts = LockExternalFrame(dstMemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);

                isDstLocked = true;
            }
            else
            {
                dstTempSurface.Data = pDst->Data;
                dstTempSurface.Data.MemId = 0;
            }
        }
        else if (dstMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetExternalFrameHDL(dstMemId, (mfxHDL*)&dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = &dstHandle;
        }
    }
    else if (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME)
    {
        if (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == dstPtr)
            {
                sts = LockFrame(dstMemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);

                isDstLocked = true;
            }
            else
            {
                dstTempSurface.Data = pDst->Data;
                dstTempSurface.Data.MemId = 0;
            }
        }
        else if (dstMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetFrameHDL(dstMemId, (mfxHDL*)&dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = &dstHandle;
        }
    }

    bool video2video = (srcMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET) && (dstMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET);
    bool sys2sys = (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY) && (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY);
    mfxStatus fcSts = MFX_ERR_NONE;
    if (video2video)
    {
        if ((srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME) && (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME))
            fcSts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface);       // d3d9
        else if ((srcMemType & MFX_MEMTYPE_INTERNAL_FRAME) && (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME))
            fcSts = Base::DoFastCopyExtended(&dstTempSurface, &srcTempSurface); // d3d11
        else if ((srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME) && (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME))
            fcSts = CopyDX9toDX11(pSrc->Data.MemId, pDst->Data.MemId);
        else
            fcSts = CopyDX11toDX9(pSrc->Data.MemId, pDst->Data.MemId);
    }
    else if (sys2sys)
    {
        sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_SYS);
    }
    else // src or dst in video memory
    {
        bool isD3D9Memory = (srcMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET) ? srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME : dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME;
        if (isD3D9Memory)
            fcSts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface);
        else
            fcSts = Base::DoFastCopyExtended(&dstTempSurface, &srcTempSurface);
    }

    if (true == isSrcLocked)
    {
        if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            sts = UnlockExternalFrame(srcMemId, &srcTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
        else if (srcMemType & MFX_MEMTYPE_INTERNAL_FRAME)
        {
            sts = UnlockFrame(srcMemId, &srcTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
    }

    if (true == isDstLocked)
    {
        if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            sts = UnlockExternalFrame(dstMemId, &dstTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
        else if (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME)
        {
            sts = UnlockFrame(dstMemId, &dstTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
    }

    return fcSts;
}

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::DoFastCopyExtended(mfxFrameSurface1* pDst, mfxFrameSurface1* pSrc)
{
    mfxStatus sts;

    mfxU8* srcPtr;
    mfxU8* dstPtr;

    sts = GetFramePointerChecked(pSrc->Info, pSrc->Data, &srcPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);
    sts = GetFramePointerChecked(pDst->Info, pDst->Data, &dstPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);

    // check that only memId or pointer are passed
    // otherwise don't know which type of memory copying is requested
    if (
        (NULL != dstPtr && NULL != pDst->Data.MemId) ||
        (NULL != srcPtr && NULL != pSrc->Data.MemId)
        )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    IppiSize roi = { IPP_MIN(pSrc->Info.Width, pDst->Info.Width), IPP_MIN(pSrc->Info.Height, pDst->Info.Height) };

    // check that region of interest is valid
    if (0 == roi.width || 0 == roi.height)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    HRESULT hRes;
    D3DSURFACE_DESC sSurfDesc;
    D3DLOCKED_RECT  sLockRect;

    mfxU32 srcPitch = pSrc->Data.PitchLow + ((mfxU32)pSrc->Data.PitchHigh << 16);
    mfxU32 dstPitch = pDst->Data.PitchLow + ((mfxU32)pDst->Data.PitchHigh << 16);

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        IDirect3DDevice9* direct3DDevice;
        hRes = m_pDirect3DDeviceManager->LockDevice(m_hDirectXHandle, &direct3DDevice, true);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

        const tagRECT rect = { 0, 0, roi.width, roi.height };

        HRESULT stretchRectResult = S_OK;
        mfxU32 counter = 0;
        do
        {
            stretchRectResult = direct3DDevice->StretchRect((IDirect3DSurface9*)((mfxHDLPair*)pSrc->Data.MemId)->first, &rect,
                (IDirect3DSurface9*)((mfxHDLPair*)pDst->Data.MemId)->first, &rect,
                D3DTEXF_LINEAR);
            if (FAILED(stretchRectResult))
                Sleep(20);
        } while (FAILED(stretchRectResult) && ++counter < 4); // waiting 80 ms, source surface may be locked by application

        hRes = m_pDirect3DDeviceManager->UnlockDevice(m_hDirectXHandle, false);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

        direct3DDevice->Release();

        MFX_CHECK(SUCCEEDED(stretchRectResult), MFX_ERR_DEVICE_FAILED);
    }
    else if (NULL != pSrc->Data.MemId && NULL != dstPtr)
    {
        IDirect3DSurface9* pSurface = (IDirect3DSurface9*)((mfxHDLPair*)pSrc->Data.MemId)->first;

        hRes = pSurface->GetDesc(&sSurfDesc);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "FastCopySSE");
        hRes |= pSurface->LockRect(&sLockRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_LOCK_MEMORY);
        srcPitch = sLockRect.Pitch;
        sts = mfxDefaultAllocatorD3D9::SetFrameData(sSurfDesc, sLockRect, &pSrc->Data);
        MFX_CHECK_STS(sts);

        mfxMemId saveMemId = pSrc->Data.MemId;
        pSrc->Data.MemId = 0;

        sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_VIDEO_TO_SYS); // sw copy
        MFX_CHECK_STS(sts);

        pSrc->Data.MemId = saveMemId;
        MFX_CHECK_STS(sts);

        hRes = pSurface->UnlockRect();
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
    }
    else if (NULL != srcPtr && NULL != dstPtr)
    {
        // system memories were passed
        // use common way to copy frames

        sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_SYS); // sw copy
        MFX_CHECK_STS(sts);
    }
    else if (NULL != srcPtr && NULL != pDst->Data.MemId)
    {
        // source are placed in system memory, destination is in video memory
        // use common way to copy frames from system to video, most faster
        IDirect3DSurface9* pSurface = (IDirect3DSurface9*)((mfxHDLPair*)pDst->Data.MemId)->first;

        hRes = pSurface->GetDesc(&sSurfDesc);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

        hRes |= pSurface->LockRect(&sLockRect, NULL, NULL);
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_LOCK_MEMORY);

        dstPitch = sLockRect.Pitch;
        sts = mfxDefaultAllocatorD3D9::SetFrameData(sSurfDesc, sLockRect, &pDst->Data);
        MFX_CHECK_STS(sts);

        mfxMemId saveMemId = pDst->Data.MemId;
        pDst->Data.MemId = 0;
        if (pSrc->Info.FourCC == MFX_FOURCC_YV12)
        {
            sts = ConvertYV12toNV12SW(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }

        sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_VIDEO); // sw copy
        MFX_CHECK_STS(sts);

        pDst->Data.MemId = saveMemId;
        MFX_CHECK_STS(sts);

        hRes = pSurface->UnlockRect();
        MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;

} // mfxStatus D3D9VideoCORE::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::GetHandle(mfxHandleType type, mfxHDL *handle)
{
    MFX_CHECK_NULL_PTR1(handle);

    if (type == MFX_HANDLE_D3D9_DEVICE_MANAGER)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        MFX_CHECK(m_pDirect3DDeviceManager, MFX_ERR_NOT_FOUND);
        *handle = m_pDirect3DDeviceManager;

        return MFX_ERR_NONE;
    }
    else
    {
        mfxStatus sts = Base::GetHandle(type, handle);
        MFX_CHECK_STS(sts);

        return MFX_ERR_NONE;
    }
}

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::SetFrameAllocator(mfxFrameAllocator* allocator)
{
    MFX_CHECK(allocator, MFX_ERR_NONE);

    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK(!m_hasDX9FrameAllocator && !m_bSetExtFrameAlloc, MFX_ERR_UNDEFINED_BEHAVIOR);

    m_dx9FrameAllocator = *allocator;
    m_hasDX9FrameAllocator = true;

    mfxStatus sts = Base::SetFrameAllocator(allocator);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

template <class Base>
mfxMemId D3D9ON11VideoCORE_T<Base>::WrapSurface(mfxMemId dx9Surface, const mfxFrameAllocResponse& dx11Surfaces, bool isNeedRewrap)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    if (!dx11Surfaces.mids) return 0;

    auto it = m_dx9MemIdMap.find(dx9Surface);
    if (it == m_dx9MemIdMap.end() || isNeedRewrap)
    {
        for (int i = 0; i < dx11Surfaces.NumFrameActual; i++)
        {
            // Means that we were not map this surface previously.
            if (m_dx9MemIdUsed.find(dx11Surfaces.mids[i]) == m_dx9MemIdUsed.end())
            {
                m_dx9MemIdUsed.insert(std::pair<mfxMemId, mfxMemId>(dx11Surfaces.mids[i], dx9Surface));
                m_dx9MemIdMap.insert(std::pair<mfxMemId, mfxMemId>(dx9Surface, dx11Surfaces.mids[i]));
                return dx11Surfaces.mids[i];
            }
        }

        return 0;
    }

    return it->second;
}

template <class Base>
mfxMemId D3D9ON11VideoCORE_T<Base>::UnWrapSurface(mfxMemId wrappedSurface, bool isNeedUnwrap)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    if (m_dx9MemIdMap.empty()) return wrappedSurface;
    // We try to find dx11 surface id by dx9 surface id.
    auto it = m_dx9MemIdMap.find(wrappedSurface);
    if (it != m_dx9MemIdMap.end())
    {
        mfxMemId dx11MemId = it->second;
        if (!m_session->m_pDECODE || isNeedUnwrap && m_session->m_pVPP)
        {
            m_dx9MemIdUsed.erase(dx11MemId);
            m_dx9MemIdMap.erase(wrappedSurface);
        }
        return dx11MemId;
    }
    return 0;
}


static mfxU32 DXGItoMFX(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_P010:
        return MFX_FOURCC_P010;

    case DXGI_FORMAT_NV12:
        return MFX_FOURCC_NV12;

    case DXGI_FORMAT_YUY2:
        return D3DFMT_YUY2;

    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return MFX_FOURCC_RGB4;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return MFX_FOURCC_BGR4;

    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return MFX_FOURCC_A2RGB10;

    case DXGI_FORMAT_AYUV:
        return MFX_FOURCC_AYUV;

#if (MFX_VERSION >= 1027)
    case DXGI_FORMAT_Y210 :
        return MFX_FOURCC_Y210;
    case DXGI_FORMAT_Y410:
        return MFX_FOURCC_Y410;
#endif

#if (MFX_VERSION >= 1031)
    case DXGI_FORMAT_P016:
        return MFX_FOURCC_P016;
    case DXGI_FORMAT_Y216 :
        return MFX_FOURCC_Y216;
    case DXGI_FORMAT_Y416:
        return MFX_FOURCC_Y416;
#endif
    default:
        return 0;
    }

} // mfxDefaultAllocatorD3D11::MFXtoDXGI(mfxU32 format)

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::CopyDX11toDX9(const mfxMemId dx11MemId, const mfxMemId dx9MemId)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CopyDX11toDX9");
    ETW_NEW_EVENT(MFX_TRACE_HOTSPOT_COPY_DX11_TO_DX9, 0, make_event_data(), [&]() { return make_event_data();});

    // We need to get right allocator to lock DX11 surfaces
    mfxMemId internalDX11Mid = dx11MemId;
    mfxFrameAllocator* pAlloc = GetAllocatorAndMid(internalDX11Mid);
    MFX_CHECK(pAlloc, MFX_ERR_INVALID_HANDLE);

    mfxHDL dstHandle = 0;
    mfxStatus sts = m_dx9FrameAllocator.GetHDL(m_dx9FrameAllocator.pthis, dx9MemId, &dstHandle);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_LOCK_MEMORY);
    CComPtr<IDirect3DSurface9> dstFrame = reinterpret_cast<IDirect3DSurface9*>(dstHandle);

    D3DSURFACE_DESC dstDesc = {};
    HRESULT hr = dstFrame->GetDesc(&dstDesc);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    D3DLOCKED_RECT lockRect;
    hr = dstFrame->LockRect(&lockRect, NULL, NULL);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_LOCK_MEMORY);

    mfxFrameData dstData = {};
    sts = mfxDefaultAllocatorD3D9::SetFrameData(dstDesc, lockRect, &dstData);
    MFX_CHECK_STS(sts);

    mfxHDLPair srcHandle = {};
    sts = pAlloc->GetHDL(pAlloc->pthis, internalDX11Mid, &srcHandle.first);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_LOCK_MEMORY);

    mfxFrameSurface1 srcTmpSurf = {};
    srcTmpSurf.Data.MemId = &srcHandle;

    mfxFrameSurface1 dstTmpSurf = {};
    dstTmpSurf.Data = dstData;
    dstTmpSurf.Info.Width  = srcTmpSurf.Info.Width  = static_cast<mfxU16>(dstDesc.Width);
    dstTmpSurf.Info.Height = srcTmpSurf.Info.Height = static_cast<mfxU16>(dstDesc.Height);
    dstTmpSurf.Info.FourCC = srcTmpSurf.Info.FourCC = dstDesc.Format;

    sts = Base::DoFastCopyExtended(&dstTmpSurf, &srcTmpSurf);
    MFX_CHECK_STS(sts);

    hr = dstFrame->UnlockRect();
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::CopyDX9toDX11(const mfxMemId dx9MemId, const mfxMemId dx11MemId)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CopyDX9toDX11");
    ETW_NEW_EVENT(MFX_TRACE_HOTSPOT_COPY_DX9_TO_DX11, 0, make_event_data(), [&]() { return make_event_data();});

    // We need to get right allocator to lock DX11 surfaces
    mfxMemId internalDX11Mid = dx11MemId;
    mfxFrameAllocator* pAlloc = GetAllocatorAndMid(internalDX11Mid);
    MFX_CHECK(pAlloc, MFX_ERR_INVALID_HANDLE);

    mfxHDLPair dstHandle = {};
    mfxStatus sts = pAlloc->GetHDL(pAlloc->pthis, internalDX11Mid, &dstHandle.first);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_LOCK_MEMORY);
    CComPtr<ID3D11Texture2D> dstFrame = reinterpret_cast<ID3D11Texture2D*>(dstHandle.first);

    D3D11_TEXTURE2D_DESC dstDesc = {};
    dstFrame->GetDesc(&dstDesc);

    {
        std::lock_guard<std::mutex> guard(m_copyMutex);
        mfxHDL srcHandle = {};
        sts = m_dx9FrameAllocator.GetHDL(m_dx9FrameAllocator.pthis, dx9MemId, &srcHandle);
        MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_LOCK_MEMORY);
        CComPtr<IDirect3DSurface9> srcFrame = reinterpret_cast<IDirect3DSurface9*>(srcHandle);

        D3DSURFACE_DESC srcDesc = {};
        HRESULT hr = srcFrame->GetDesc(&srcDesc);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        D3DLOCKED_RECT  lockRect;
        hr = srcFrame->LockRect(&lockRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_LOCK_MEMORY);
        mfxFrameData srcData = {};
        sts = mfxDefaultAllocatorD3D9::SetFrameData(srcDesc, lockRect, &srcData);
        MFX_CHECK_STS(sts);

        mfxFrameSurface1 srcTmpSurf = {};
        srcTmpSurf.Data = srcData;

        mfxFrameSurface1 dstTmpSurf = {};
        dstTmpSurf.Data.MemId = &dstHandle;
        dstTmpSurf.Info.Width = srcTmpSurf.Info.Width = static_cast<mfxU16>(dstDesc.Width);
        dstTmpSurf.Info.Height = srcTmpSurf.Info.Height = static_cast<mfxU16>(dstDesc.Height);
        dstTmpSurf.Info.FourCC = srcTmpSurf.Info.FourCC = DXGItoMFX(dstDesc.Format);

        sts = Base::DoFastCopyExtended(&dstTmpSurf, &srcTmpSurf);
        MFX_CHECK_STS(sts);

        hr = srcFrame->UnlockRect();
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;
}

template <class Base>
bool D3D9ON11VideoCORE_T<Base>::m_IsD3D9On11Core = true;

template <class Base>
void* D3D9ON11VideoCORE_T<Base>::QueryCoreInterface(const MFX_GUID& guid)
{
    if (MFXI_IS_CORED3D9ON11_GUID == guid)
    {
        return (void*)&m_IsD3D9On11Core;
    }

    return Base::QueryCoreInterface(guid);
}

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::AllocFrames(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response, bool isNeedCopy)
{
    if ((request->Type & MFX_MEMTYPE_INTERNAL_FRAME) &&
        (request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET || request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET))
        request->Type |= MFX_MEMTYPE_SHARED_RESOURCE;

    return Base::AllocFrames(request, response, isNeedCopy);
}

MfxWrapController::MfxWrapController()
    : m_pDX9ON11Core(nullptr)
    , m_originalReqType(0)
{
}

MfxWrapController::~MfxWrapController()
{
    if (m_responseQueue.size())
    {
        for (size_t i = 0; i < m_responseQueue.size(); i++)
            m_pDX9ON11Core->FreeFrames(&m_responseQueue[i]);
    }

    m_mids.clear();
    m_locked.clear();
}

mfxStatus MfxWrapController::Alloc(VideoCORE* core, const mfxU16& numFrameMin, const mfxVideoParam& par, const mfxU16& reqType)
{
    MFX_CHECK_NULL_PTR1(core);
    m_pDX9ON11Core = dynamic_cast<D3D9ON11VideoCORE*>(core);

    MFX_CHECK(isDX9ON11Wrapper() && par.IOPattern != MFX_IOPATTERN_IN_SYSTEM_MEMORY, MFX_ERR_NONE);

    mfxFrameAllocRequest req = {};

    mfxStatus sts = ConfigureRequest(req, par, numFrameMin, reqType);
    MFX_CHECK_STS(sts);

    mfxFrameAllocRequest tmp = req;
    tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

    m_responseQueue.resize(req.NumFrameMin);
    m_mids.resize(req.NumFrameMin);

    for (int i = 0; i < req.NumFrameMin; i++)
    {
        sts = m_pDX9ON11Core->AllocFrames(&tmp, &m_responseQueue[i]);
        MFX_CHECK_STS(sts);
        m_mids[i] = m_responseQueue[i].mids[0];
    }

    m_locked.resize(req.NumFrameMin, 0);

    return MFX_ERR_NONE;
}

mfxStatus MfxWrapController::WrapSurface(mfxFrameSurface1* dx9Surface, mfxHDL* frameHDL)
{
    MFX_CHECK(isDX9ON11Wrapper() && m_responseQueue.size(), MFX_ERR_NONE);
    MFX_CHECK_NULL_PTR1(dx9Surface);

    mfxMemId dx11MemId = 0;

    mfxStatus mfxRes = AcquireResource(dx11MemId);
    MFX_CHECK_STS(mfxRes);

    if (IsInputSurface())
    {
        mfxRes = m_pDX9ON11Core->IncreaseReference(&dx9Surface->Data);
        MFX_CHECK_STS(mfxRes);

        mfxRes = m_pDX9ON11Core->CopyDX9toDX11(dx9Surface->Data.MemId, dx11MemId);
        MFX_CHECK_STS(mfxRes);

        mfxRes = m_pDX9ON11Core->DecreaseReference(&dx9Surface->Data);
        MFX_CHECK_STS(mfxRes);
    }

    m_dx9Todx11.insert(std::make_pair(dx9Surface->Data.MemId, dx11MemId));

    mfxRes = m_pDX9ON11Core->GetFrameHDL(dx11MemId, frameHDL);
    MFX_CHECK_STS(mfxRes);

    return MFX_ERR_NONE;
}

mfxStatus MfxWrapController::UnwrapSurface(mfxFrameSurface1* dx9Surface)
{
    MFX_CHECK(isDX9ON11Wrapper() && m_responseQueue.size(), MFX_ERR_NONE);
    MFX_CHECK_NULL_PTR1(dx9Surface);
    MFX_CHECK(!m_dx9Todx11.empty(), MFX_ERR_UNDEFINED_BEHAVIOR);

    auto it = m_dx9Todx11.find(dx9Surface->Data.MemId);
    MFX_CHECK(it != m_dx9Todx11.end(), MFX_ERR_INVALID_HANDLE);

    mfxMemId dx11MemId = it->second;
    m_dx9Todx11.erase(dx9Surface->Data.MemId);

    mfxStatus mfxRes = MFX_ERR_NONE;
    if (IsOutputSurface())
    {
        mfxRes = m_pDX9ON11Core->IncreaseReference(&dx9Surface->Data);
        MFX_CHECK_STS(mfxRes);

        mfxRes = m_pDX9ON11Core->CopyDX11toDX9(dx11MemId, dx9Surface->Data.MemId);
        MFX_CHECK_STS(mfxRes);

        mfxRes = m_pDX9ON11Core->DecreaseReference(&dx9Surface->Data);
        MFX_CHECK_STS(mfxRes);
    }

    mfxRes = ReleaseResource(dx11MemId);
    MFX_CHECK_STS(mfxRes);

    return MFX_ERR_NONE;
}


bool MfxWrapController::isDX9ON11Wrapper()
{
    return m_pDX9ON11Core != nullptr;
}

mfxStatus MfxWrapController::Lock(mfxU32 idx)
{
    MFX_CHECK(idx < m_locked.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_locked[idx] != 0xffffffff, MFX_ERR_LOCK_MEMORY);

    ++m_locked[idx];

    return MFX_ERR_NONE;
}

bool MfxWrapController::IsLocked(mfxU32 idx)
{
    MFX_CHECK(idx < m_locked.size(), true); //In case of index is higher than locked size - return "locked" and prevent unexpected usage

    return m_locked[idx] > 0;
}


mfxStatus MfxWrapController::Unlock(mfxU32 idx)
{
    MFX_CHECK(idx < m_locked.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_locked[idx] != 0, MFX_ERR_LOCK_MEMORY);

    --m_locked[idx];

    return MFX_ERR_NONE;
}

mfxStatus MfxWrapController::AcquireResource(mfxMemId& dx11MemId)
{
    mfxU32 idx = 0;
    bool isFound = false;
    for (idx = 0; idx < m_locked.size(); idx++)
    {
        if (!IsLocked(idx))
        {
            isFound = true;
            break;
        }
    }
    MFX_CHECK(isFound, MFX_ERR_NOT_FOUND);

    mfxStatus sts = Lock(idx);
    MFX_CHECK_STS(sts);

    dx11MemId = m_mids[idx];

    return MFX_ERR_NONE;
}

mfxStatus MfxWrapController::ReleaseResource(mfxMemId dx11MemId)
{
    for (mfxU32 idx = 0; idx < m_mids.size(); idx++)
    {
        if (m_mids[idx] == dx11MemId)
        {
            mfxStatus sts = Unlock(idx);
            MFX_RETURN(sts);
        }
    }
    MFX_RETURN(MFX_ERR_NOT_FOUND);
}

mfxStatus MfxWrapController::ConfigureRequest(mfxFrameAllocRequest& req, const mfxVideoParam& par, const mfxU16& numFrameMin, const mfxU16& reqType)
{
    m_originalReqType = reqType;
    req.Type |= reqType;
    switch (reqType)
    {
    case MFX_MEMTYPE_FROM_ENCODE:
    case MFX_MEMTYPE_FROM_DECODE:
        req.Info = par.mfx.FrameInfo;
        req.Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SHARED_RESOURCE;
        break;
    case MFX_MEMTYPE_FROM_VPPIN:
        req.Info = par.vpp.In;
        if (par.vpp.In.FourCC == MFX_FOURCC_YV12)
            req.Info.FourCC = MFX_FOURCC_NV12;
        req.Type |= MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SHARED_RESOURCE;
        break;
    case MFX_MEMTYPE_FROM_VPPOUT:
        req.Info = par.vpp.Out;
        req.Type |= MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SHARED_RESOURCE;
        break;
    default:
        return MFX_ERR_MEMORY_ALLOC;
    }

    req.NumFrameMin = req.NumFrameSuggested = numFrameMin;

    return MFX_ERR_NONE;
}

bool MfxWrapController::IsInputSurface()
{
    switch (m_originalReqType)
    {
    case MFX_MEMTYPE_FROM_ENCODE:
    case MFX_MEMTYPE_FROM_VPPIN:
        return true;
    default:
        return false;
    }
}

bool MfxWrapController::IsOutputSurface()
{
    return !IsInputSurface();
}

#endif //MFX_DX9ON11

