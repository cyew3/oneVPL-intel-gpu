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

#if defined  (MFX_VA)
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
    , m_isDX11AuxBufferAllocated(false)
    , m_dx9FrameAllocator()
{}

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
#if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
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

    mfxStatus fcSts = MFX_ERR_NONE;

    if((srcMemType & MFX_MEMTYPE_FROM_VPPIN) && (dstMemType & MFX_MEMTYPE_FROM_VPPOUT))
        fcSts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface);
    else fcSts = Base::DoFastCopyExtended(&dstTempSurface, &srcTempSurface);

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

    // FIXME: currently, CM copy doesn`t work here due to  here we work with DX9 surfaces
    // But originally we created DX11 device
    bool canUseCMCopy = false; //m_bCmCopy ? CmCopyWrapper::CanUseCmCopy(pDst, pSrc) : false;

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        if (canUseCMCopy)
        {
            sts = MFX_ERR_NONE;
            mfxU32 counter = 0;
            do
            {
                sts = m_pCmCopy->CopyVideoToVideo(pDst, pSrc);

                if (sts != MFX_ERR_NONE)
                    Sleep(20);
            } while (sts != MFX_ERR_NONE && ++counter < 4); // waiting 80 ms, source surface may be locked by application

            MFX_CHECK_STS(sts);
        }
        else
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
    }
    else if (NULL != pSrc->Data.MemId && NULL != dstPtr)
    {
        mfxI64 verticalPitch = (mfxI64)(pDst->Data.UV - pDst->Data.Y);
        verticalPitch = (verticalPitch % pDst->Data.Pitch) ? 0 : verticalPitch / pDst->Data.Pitch;

        if (canUseCMCopy)
        {
            sts = m_pCmCopy->CopyVideoToSys(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }
        else
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

        if (canUseCMCopy)
        {
            sts = m_pCmCopy->CopySysToVideo(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }
        else
        {
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
    mfxHDL dstHandle = 0;
    mfxFrameData srcData = {};
    mfxFrameData dstData = {};
    HRESULT hr = S_OK;

    // We need to get right allocator to lock DX11 surfaces
    mfxMemId internalDX11Mid = dx11MemId;
    mfxFrameAllocator* pAlloc = GetAllocatorAndMid(internalDX11Mid);
    MFX_CHECK(pAlloc, MFX_ERR_INVALID_HANDLE);


    mfxStatus sts = m_dx9FrameAllocator.GetHDL(m_dx9FrameAllocator.pthis, dx9MemId, &dstHandle);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_LOCK_MEMORY);

    CComPtr<IDirect3DSurface9> dstFrame = reinterpret_cast<IDirect3DSurface9*>(dstHandle);

    D3DSURFACE_DESC dstDesc = {};

    hr = dstFrame->GetDesc(&dstDesc);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_UNDEFINED_BEHAVIOR);

    sts = m_dx9FrameAllocator.Lock(m_dx9FrameAllocator.pthis, dx9MemId, &dstData);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_LOCK_MEMORY);

    if (!MFX_SUCCEEDED(pAlloc->Lock(pAlloc->pthis, internalDX11Mid, &srcData)))
    {
        m_dx9FrameAllocator.Unlock(m_dx9FrameAllocator.pthis, dx9MemId, &dstData);
        MFX_RETURN(MFX_ERR_LOCK_MEMORY);
    }

    mfxFrameSurface1 dstTmpSurf = {}, srcTmpSurf = {};
    dstTmpSurf.Data = dstData;
    srcTmpSurf.Data = srcData;

    dstTmpSurf.Info.Width  = srcTmpSurf.Info.Width  = static_cast<mfxU16>(dstDesc.Width);
    dstTmpSurf.Info.Height = srcTmpSurf.Info.Height = static_cast<mfxU16>(dstDesc.Height);
    dstTmpSurf.Info.FourCC = srcTmpSurf.Info.FourCC = dstDesc.Format;

    sts = CoreDoSWFastCopy(dstTmpSurf, srcTmpSurf, COPY_SYS_TO_SYS);
    MFX_CHECK_STS(sts);

    sts = m_dx9FrameAllocator.Unlock(m_dx9FrameAllocator.pthis, dx9MemId, &dstData);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);

    pAlloc->Unlock(pAlloc->pthis, internalDX11Mid, &srcData);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_RETURN(MFX_ERR_NONE);
}

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::CopyDX9toDX11(const mfxMemId dx9MemId, const mfxMemId dx11MemId)
{
    mfxHDLPair dstHandle = {};
    mfxFrameData srcData = {};
    mfxFrameData dstData = {};

    // We need to get right allocator to lock DX11 surfaces
    mfxMemId internalDX11Mid = dx11MemId;
    mfxFrameAllocator* pAlloc = GetAllocatorAndMid(internalDX11Mid);
    MFX_CHECK(pAlloc, MFX_ERR_INVALID_HANDLE);

    mfxStatus sts = pAlloc->GetHDL(pAlloc->pthis, internalDX11Mid, &dstHandle.first);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_LOCK_MEMORY);

    CComPtr<ID3D11Texture2D> dstFrame = reinterpret_cast<ID3D11Texture2D*>(dstHandle.first);

    D3D11_TEXTURE2D_DESC dstDesc = {};

    dstFrame->GetDesc(&dstDesc);

    sts = pAlloc->Lock(pAlloc->pthis, internalDX11Mid, &dstData);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_LOCK_MEMORY);

    {
        std::lock_guard<std::mutex> guard(m_copyMutex);

        sts = m_dx9FrameAllocator.Lock(m_dx9FrameAllocator.pthis, dx9MemId, &srcData);

        if (MFX_ERR_NONE != sts)
        {
            pAlloc->Unlock(pAlloc->pthis, internalDX11Mid, &dstData);
            MFX_RETURN(MFX_ERR_LOCK_MEMORY);
        }

        //To read access we need only info, so we can unlock resourse.
        sts = m_dx9FrameAllocator.Unlock(m_dx9FrameAllocator.pthis, dx9MemId, nullptr);
        MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);
    }


    mfxFrameSurface1 dstTmpSurf = {}, srcTmpSurf = {};
    dstTmpSurf.Data = dstData;
    srcTmpSurf.Data = srcData;

    dstTmpSurf.Info.Width  = srcTmpSurf.Info.Width  = static_cast<mfxU16>(dstDesc.Width);
    dstTmpSurf.Info.Height = srcTmpSurf.Info.Height = static_cast<mfxU16>(dstDesc.Height);
    dstTmpSurf.Info.FourCC = srcTmpSurf.Info.FourCC = DXGItoMFX(dstDesc.Format);

    sts = CoreDoSWFastCopy(dstTmpSurf, srcTmpSurf, COPY_SYS_TO_SYS);
    MFX_CHECK_STS(sts);

    sts = pAlloc->Unlock(pAlloc->pthis, internalDX11Mid, &dstData);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_RETURN(MFX_ERR_NONE);
}

#endif //MFX_DX9ON11

#endif
