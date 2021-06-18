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

#include <libmfx_core_d3d9on11.h>
#include <libmfx_core_d3d11.h>
#include <libmfx_core_d3d9.h>

#pragma warning(disable : 4456)
static const GUID Subresource_GUID = { 0x423c3acf, 0x752d, 0x4cd7, { 0x84, 0x10, 0x72, 0xf5, 0x36, 0xb8, 0xc4, 0x9d } };
// {79F852D7-41B4-4ED7-9D6E-5EAF3D8D0B55}
static const GUID IsUnkownSubresource_GUID =
{ 0x79f852d7, 0x41b4, 0x4ed7, { 0x9d, 0x6e, 0x5e, 0xaf, 0x3d, 0x8d, 0xb, 0x55 } };

template class D3D9ON11VideoCORE_T<D3D11VideoCORE>;

std::mutex D3D9ON11VideoCORE_T<D3D11VideoCORE>::m_copyMutexD3d9;

template <class Base>
D3D9ON11VideoCORE_T<Base>::D3D9ON11VideoCORE_T(
    const mfxU32 adapterNum
    , const mfxU32 numThreadsAvailable
    , const mfxSession session
)
    : D3D11VideoCORE(adapterNum, numThreadsAvailable, session)
    , m_hDirectXHandle(INVALID_HANDLE_VALUE)
    , m_pDirect3DDeviceManager(nullptr)
{
}

template <class Base>
D3D9ON11VideoCORE_T<Base>::~D3D9ON11VideoCORE_T()
{
    CloseHandle(m_FenceEvent);
    m_FenceEvent = nullptr;
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
mfxStatus D3D9ON11VideoCORE_T<Base>::CreateVA(mfxVideoParam* param, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response, UMC::FrameAllocator* allocator)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = Base::CreateVA(param, request, response, allocator);
    MFX_CHECK_STS(sts);

    if (param->IOPattern & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
    {
        // Getting access to external suraces
        mfxFrameAllocRequest r = {};
        r.Info = request->Info;
        r.Type = MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
        r.NumFrameSuggested = r.NumFrameMin = 1;

        mfxFrameAllocResponse externalResponse = {};

        sts = (*m_FrameAllocator.frameAllocator.Alloc)(m_FrameAllocator.frameAllocator.pthis, &r, &externalResponse);
        MFX_CHECK_STS(sts);

        std::vector<IDirect3DSurface9*> renderTargets(externalResponse.NumFrameActual);
        for (mfxU32 i = 0; i < externalResponse.NumFrameActual; i++)
        {
            MFX_SAFE_CALL((*m_FrameAllocator.frameAllocator.GetHDL)(m_FrameAllocator.frameAllocator.pthis, externalResponse.mids[i], (mfxHDL*)(&renderTargets[i])));
            UINT subresourceIdx = i;
            MFX_CHECK(MFX_SUCCEEDED(renderTargets[i]->SetPrivateData(Subresource_GUID, &subresourceIdx, sizeof(subresourceIdx), 0)), MFX_ERR_DEVICE_LOST);
        }
        sts = (*m_FrameAllocator.frameAllocator.Free)(m_FrameAllocator.frameAllocator.pthis, &externalResponse);
        MFX_CHECK_STS(sts);
    }
    return MFX_ERR_NONE;
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

            //  It works since Windows 20H2
            hr = pD3DDevice->QueryInterface(IID_PPV_ARGS(&m_d3dOn12));
            if (SUCCEEDED(hr))
            {
                MFX_CHECK(SUCCEEDED(m_d3dOn12->GetD3D12Device(IID_PPV_ARGS(&m_d3d12))), MFX_ERR_DEVICE_FAILED);

                D3D12_COMMAND_QUEUE_DESC queueDesc = { D3D12_COMMAND_LIST_TYPE_COPY, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
                MFX_CHECK(SUCCEEDED(m_d3d12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue))), MFX_ERR_DEVICE_FAILED);
                MFX_CHECK(SUCCEEDED(m_d3d12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_commandAllocator))), MFX_ERR_DEVICE_FAILED);
                MFX_CHECK(SUCCEEDED(m_d3d12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))), MFX_ERR_DEVICE_FAILED);
                m_FenceEvent = CreateEvent(nullptr, false, false, nullptr);
                MFX_CHECK(m_FenceEvent, MFX_ERR_DEVICE_FAILED);
                MFX_CHECK(SUCCEEDED(m_d3d12->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_commandAllocator, nullptr, IID_PPV_ARGS(&m_commandList))), MFX_ERR_DEVICE_FAILED);
                MFX_CHECK(SUCCEEDED(m_commandList->Close()), MFX_ERR_DEVICE_FAILED);
            }
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
            fcSts = CopyDX9toDX11(pDst->Data.MemId, pSrc->Data.MemId);
        else
            fcSts = CopyDX11toDX9(pDst->Data.MemId, pSrc->Data.MemId);
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


UINT8 inline D3D12GetFormatPlaneCount(ID3D12Device* d, DXGI_FORMAT f)
{
    D3D12_FEATURE_DATA_FORMAT_INFO i = { f, 0 };
    if (FAILED(d->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &i, sizeof(i))))
        return 0;
    return i.PlaneCount;
};

UINT inline D3D12CalcSubresource(UINT mipSlice, UINT arraySlice, UINT planeSlice, UINT mipLevels, UINT arraySize)
{
    return mipSlice + arraySlice * mipLevels + planeSlice * mipLevels * arraySize;
};


bool CanUseD3D9on12Interop(CComPtr<IDirect3DDevice9On12>& device, CComPtr<ID3D12CommandQueue> & queue, IDirect3DSurface9* frame, CComPtr<ID3D12Resource>& resource, UINT& subresourceIdx)
{
    if (nullptr == device)
        return false;

    bool IsUnkownSubresourceIdx = false;
    DWORD size = sizeof(IsUnkownSubresourceIdx);
    if (SUCCEEDED(frame->GetPrivateData(IsUnkownSubresource_GUID, &IsUnkownSubresourceIdx, &size)) && IsUnkownSubresourceIdx)
    {
        return false;
    }

    MFX_CHECK_WITH_THROW(SUCCEEDED(device->UnwrapUnderlyingResource(frame, queue, IID_PPV_ARGS(&resource))), MFX_ERR_DEVICE_FAILED, mfx::mfxStatus_exception(MFX_ERR_DEVICE_FAILED));
    auto desc = resource->GetDesc();
    if (desc.DepthOrArraySize > 1)
    {
        DWORD size = sizeof(subresourceIdx);
        if (FAILED(frame->GetPrivateData(Subresource_GUID, &subresourceIdx, &size)))
        {
            const bool IsUnkownSubresourceIdx = true;
            MFX_CHECK_WITH_THROW(SUCCEEDED(frame->SetPrivateData(IsUnkownSubresource_GUID, &IsUnkownSubresourceIdx, sizeof(IsUnkownSubresourceIdx), 0)), MFX_ERR_DEVICE_FAILED, mfx::mfxStatus_exception(MFX_ERR_DEVICE_FAILED));
            MFX_CHECK_WITH_THROW(SUCCEEDED(device->ReturnUnderlyingResource(frame, 0, nullptr, nullptr)), MFX_ERR_DEVICE_FAILED, mfx::mfxStatus_exception(MFX_ERR_DEVICE_FAILED));
            return false;
        }
    }

    return true;
}

mfxStatus GetD3D12Resource(CComPtr<ID3D12Device>& device, ID3D11Texture2D* frame, CComPtr<ID3D12Resource>& resource)
{
    UINT dataSize = sizeof(resource);
    HRESULT hr = frame->GetPrivateData(__uuidof(resource), &dataSize, &resource);
    if (DXGI_ERROR_NOT_FOUND == hr)
    {
        CComPtr<IDXGIResource1> dxgiResource;
        MFX_CHECK(SUCCEEDED(frame->QueryInterface(IID_PPV_ARGS(&dxgiResource))), MFX_ERR_DEVICE_FAILED);

        HANDLE h;
        MFX_CHECK(SUCCEEDED(dxgiResource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &h)), MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(SUCCEEDED(device->OpenSharedHandle(h, IID_PPV_ARGS(&resource))), MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(CloseHandle(h), MFX_ERR_DEVICE_FAILED);

        // Cache a retrieved d3d12 resource not to create it again
        hr = frame->SetPrivateDataInterface(__uuidof(resource), resource);
    }
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::CopyD3D12(CComPtr<ID3D12Resource>& dst, UINT dstSubresourceIdx, CComPtr<ID3D12Resource>& src, UINT srcSubresourceIdx)
{
    {
        std::lock_guard<std::mutex> guard(m_copyMutexD3d12);
        ID3D12Pageable* p[] = { dst, src };
        MFX_CHECK(SUCCEEDED(m_d3d12->MakeResident(2, p)), MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(SUCCEEDED(m_commandAllocator->Reset()), MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(SUCCEEDED(m_commandList->Reset(m_commandAllocator, nullptr)), MFX_ERR_DEVICE_FAILED);

        const auto srcDesc = src->GetDesc();
        const auto dstDesc = dst->GetDesc();
        const auto planeCount = D3D12GetFormatPlaneCount(m_d3d12, srcDesc.Format);
        for (auto i = 0; i < planeCount; ++i)
        {
            D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
            D3D12_TEXTURE_COPY_LOCATION dstLocation = {};

            srcLocation.pResource = src;
            srcLocation.SubresourceIndex = D3D12CalcSubresource(0, srcSubresourceIdx, i, srcDesc.MipLevels, srcDesc.DepthOrArraySize);
            srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

            dstLocation.pResource = dst;
            dstLocation.SubresourceIndex = D3D12CalcSubresource(0, dstSubresourceIdx, i, dstDesc.MipLevels, dstDesc.DepthOrArraySize);
            dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

            m_commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
        }
        MFX_CHECK(SUCCEEDED(m_commandList->Close()), MFX_ERR_DEVICE_FAILED);

        ID3D12CommandList* cmdLists[] = { m_commandList };
        m_commandQueue->ExecuteCommandLists(1, cmdLists);
        MFX_CHECK(SUCCEEDED(m_commandQueue->Signal(m_fence, m_signalValue)), MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(SUCCEEDED(m_fence->SetEventOnCompletion(m_signalValue, m_FenceEvent)), MFX_ERR_DEVICE_FAILED);

        ++m_signalValue;
    }

    auto dw = WaitForSingleObject(m_FenceEvent, 1000);
    MFX_CHECK(dw == WAIT_OBJECT_0, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::CopyDX11toDX9(mfxMemId dx9MemId, mfxMemId dx11MemId)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CopyDX11toDX9");
    ETW_NEW_EVENT(MFX_TRACE_HOTSPOT_COPY_DX11_TO_DX9, 0, make_event_data(), [&]() { return make_event_data();});

    auto allocator = GetAllocatorAndMid(dx11MemId);
    MFX_CHECK(allocator, MFX_ERR_INVALID_HANDLE);

    mfxHDLPair srcHandle = {};
    MFX_SAFE_CALL(allocator->GetHDL(allocator->pthis, dx11MemId, &srcHandle.first));

    mfxHDL dstHandle = nullptr;
    MFX_SAFE_CALL((*m_FrameAllocator.frameAllocator.GetHDL)(m_FrameAllocator.frameAllocator.pthis, dx9MemId, &dstHandle));
    auto dstFrame = reinterpret_cast<IDirect3DSurface9*>(dstHandle);
    CComPtr<ID3D12Resource> dstd3d12;
    UINT dstSubresourceIdx = 0;

    bool useD3D9on12Interop = false;
    try
    {
        useD3D9on12Interop = CanUseD3D9on12Interop(m_d3dOn12, m_commandQueue, dstFrame, dstd3d12, dstSubresourceIdx);
    }
    catch (const mfx::mfxStatus_exception& ex)
    {
        MFX_CHECK_STS(ex.sts);
    }

    if (!useD3D9on12Interop)
    {
        // Slow copy path
        D3DSURFACE_DESC desc = {};
        MFX_CHECK(SUCCEEDED(dstFrame->GetDesc(&desc)), MFX_ERR_DEVICE_FAILED);

        D3DLOCKED_RECT rect;
        MFX_CHECK(SUCCEEDED(dstFrame->LockRect(&rect, NULL, NULL)), MFX_ERR_LOCK_MEMORY);

        mfxFrameData dstData = {};
        MFX_SAFE_CALL(mfxDefaultAllocatorD3D9::SetFrameData(desc, rect, &dstData));

        mfxFrameSurface1 srcTmpSurf = {};
        srcTmpSurf.Data.MemId = &srcHandle;

        mfxFrameSurface1 dstTmpSurf = {};
        dstTmpSurf.Data = dstData;
        dstTmpSurf.Info.Width = srcTmpSurf.Info.Width = static_cast<mfxU16>(desc.Width);
        dstTmpSurf.Info.Height = srcTmpSurf.Info.Height = static_cast<mfxU16>(desc.Height);
        dstTmpSurf.Info.FourCC = srcTmpSurf.Info.FourCC = desc.Format;

        MFX_SAFE_CALL(Base::DoFastCopyExtended(&dstTmpSurf, &srcTmpSurf));
        MFX_CHECK(SUCCEEDED(dstFrame->UnlockRect()), MFX_ERR_DEVICE_FAILED);

        return MFX_ERR_NONE;
    }

    // Fast copy path
    auto srcd3d11 = reinterpret_cast<ID3D11Texture2D*>(srcHandle.first);
    const auto srcSubresourceIdx = (UINT)(size_t)srcHandle.second;

    CComPtr<ID3D12Resource> srcd3d12;
    MFX_SAFE_CALL(GetD3D12Resource(m_d3d12, srcd3d11, srcd3d12));
    
    MFX_SAFE_CALL(CopyD3D12(dstd3d12, dstSubresourceIdx, srcd3d12, srcSubresourceIdx));

    MFX_CHECK(SUCCEEDED(m_d3dOn12->ReturnUnderlyingResource(dstFrame, 0, nullptr, nullptr)), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

template <class Base>
mfxStatus D3D9ON11VideoCORE_T<Base>::CopyDX9toDX11(mfxMemId dx11MemId, mfxMemId dx9MemId)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CopyDX9toDX11");
    ETW_NEW_EVENT(MFX_TRACE_HOTSPOT_COPY_DX9_TO_DX11, 0, make_event_data(), [&]() { return make_event_data();});

    auto allocator = GetAllocatorAndMid(dx11MemId);
    MFX_CHECK(allocator, MFX_ERR_INVALID_HANDLE);

    mfxHDLPair dstHandle = {};
    MFX_SAFE_CALL(allocator->GetHDL(allocator->pthis, dx11MemId, &dstHandle.first));

    mfxHDL srcHandle = {};
    MFX_SAFE_CALL((*m_FrameAllocator.frameAllocator.GetHDL)(m_FrameAllocator.frameAllocator.pthis, dx9MemId, &srcHandle));
    auto srcFrame = reinterpret_cast<IDirect3DSurface9*>(srcHandle);
    CComPtr<ID3D12Resource> srcd3d12;
    UINT srcSubresourceIdx = 0;

    bool useD3D9on12Interop = false;
    try
    {
        useD3D9on12Interop = CanUseD3D9on12Interop(m_d3dOn12, m_commandQueue, srcFrame, srcd3d12, srcSubresourceIdx);
    }
    catch (const mfx::mfxStatus_exception& ex)
    {
        MFX_CHECK_STS(ex.sts);
    }

    if (!useD3D9on12Interop)
    {
        // Slow copy path
        std::lock_guard<std::mutex> guard(m_copyMutexD3d9);

        D3DSURFACE_DESC desc = {};
        MFX_CHECK(SUCCEEDED(srcFrame->GetDesc(&desc)), MFX_ERR_DEVICE_FAILED);

        D3DLOCKED_RECT rect;
        MFX_CHECK(SUCCEEDED(srcFrame->LockRect(&rect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY)), MFX_ERR_LOCK_MEMORY);

        mfxFrameData srcData = {};
        MFX_SAFE_CALL(mfxDefaultAllocatorD3D9::SetFrameData(desc, rect, &srcData));

        mfxFrameSurface1 srcTmpSurf = {};
        srcTmpSurf.Data = srcData;

        mfxFrameSurface1 dstTmpSurf = {};
        dstTmpSurf.Data.MemId = &dstHandle;
        dstTmpSurf.Info.Width = srcTmpSurf.Info.Width = static_cast<mfxU16>(desc.Width);
        dstTmpSurf.Info.Height = srcTmpSurf.Info.Height = static_cast<mfxU16>(desc.Height);
        dstTmpSurf.Info.FourCC = srcTmpSurf.Info.FourCC = desc.Format;

        MFX_SAFE_CALL(Base::DoFastCopyExtended(&dstTmpSurf, &srcTmpSurf));
        MFX_CHECK(SUCCEEDED(srcFrame->UnlockRect()), MFX_ERR_DEVICE_FAILED);

        return MFX_ERR_NONE;
    }

    // Fast copy path
    auto dstd3d11 = reinterpret_cast<ID3D11Texture2D*>(dstHandle.first);
    CComPtr<ID3D12Resource> dstd3d12;
    const auto dstSubresourceIdx = (UINT)(size_t)dstHandle.second;

    MFX_SAFE_CALL(GetD3D12Resource(m_d3d12, dstd3d11, dstd3d12));

    MFX_SAFE_CALL(CopyD3D12(dstd3d12, dstSubresourceIdx, srcd3d12, srcSubresourceIdx));

    MFX_CHECK(SUCCEEDED(m_d3dOn12->ReturnUnderlyingResource(srcFrame, 0, nullptr, nullptr)), MFX_ERR_DEVICE_FAILED);

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
        (request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET || request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET) &&
        (request->Info.FourCC != MFX_FOURCC_P8_TEXTURE))
        request->Type |= MFX_MEMTYPE_SHARED_RESOURCE;

    return Base::AllocFrames(request, response, isNeedCopy);
}

#endif //MFX_DX9ON11

