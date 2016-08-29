/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_d3d9.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_sw_d3d9.h"
#include "mfx_utils.h"

namespace MfxCapture
{

#ifndef ALIGN16
#define ALIGN16(SZ) (((SZ + 15) >> 4) << 4) // round up to a multiple of 16
#endif

SW_D3D9_Capturer::SW_D3D9_Capturer(mfxCoreInterface* _core)
    : Capturer(SW_D3D9),
      m_pmfxCore(_core),
      m_bOwnDevice(false),
      m_bResize(false),
      m_width (0),
      m_height(0),
      m_CropW (0),
      m_CropH (0),
      m_DisplayIndex(0)
{
    m_hDirectXHandle = 0;
    memset(&m_core_par, 0, sizeof(m_core_par));
    m_pResizeBuffer = 0;
}

SW_D3D9_Capturer::~SW_D3D9_Capturer()
{
    Destroy();
}

mfxStatus SW_D3D9_Capturer::CreateVideoAccelerator( mfxVideoParam const & par, const mfxU32 targetId)
{
    HRESULT hres;

    mfxStatus mfxRes = MFX_ERR_NONE;

    //will own internal device manager
    memset(&m_core_par, 0, sizeof(m_core_par));
    if(m_pmfxCore)
        mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &m_core_par);

    //in case of partial acceleration try to use library device
    if( !m_pmfxCore || mfxRes || MFX_IMPL_HARDWARE != MFX_IMPL_BASETYPE(m_core_par.Impl) ||
        MFX_IMPL_VIA_D3D9 != (m_core_par.Impl & 0xF00))
        m_bOwnDevice = true;

    if(targetId /*&& 1 != dispIndex*/)
    {
        m_DisplayIndex = GetDisplayIndex(targetId);
        m_bOwnDevice = true;
    }

    if(!m_bOwnDevice)
    {
        mfxRes = AttachToLibraryDevice();
        if(mfxRes)
            m_bOwnDevice = true;
    }

    if(m_bOwnDevice)
    {
        mfxRes = CreateDeviceManager(m_DisplayIndex);
        MFX_CHECK_STS(mfxRes);
    }

    mfxU32 monitorW = 0;
    mfxU32 monitorH = 0;
    GetDisplayResolution(monitorW, monitorH);

    //use max(user_frame_size, display_resolution) for internal frame allocation
    mfxU16 width  = par.mfx.FrameInfo.Width;
    mfxU16 height = par.mfx.FrameInfo.Height;
    //mfxU16 CropW = par.mfx.FrameInfo.CropW ? par.mfx.FrameInfo.CropW : par.mfx.FrameInfo.Width;
    //mfxU16 CropH = par.mfx.FrameInfo.CropH ? par.mfx.FrameInfo.CropH : par.mfx.FrameInfo.Height;

    m_width  = width  = (mfxU16) (width  < monitorW ? monitorW : width );
    m_height = height = (mfxU16) (height < monitorH ? monitorH : height);
    m_CropW = monitorW;
    m_CropH = monitorH;

    if(MFX_FOURCC_NV12 == par.mfx.FrameInfo.FourCC)
    {
        try
        {
            m_pColorConverter.reset(new MFXVideoVPPColorSpaceConversion(0, &mfxRes));
        }
        catch(...)
        {
            Destroy();
            return MFX_ERR_MEMORY_ALLOC;
        }
        if(mfxRes)
        {
            Destroy();
            return mfxRes;
        }
        mfxFrameInfo in = par.mfx.FrameInfo;
        mfxFrameInfo out = par.mfx.FrameInfo;
        in.FourCC = MFX_FOURCC_RGB4;
        m_pColorConverter->Init(&in, &out);
    }

    if(/*monitorW != CropW || monitorH != CropH*/true) //always have a resizer for possible resolution change
    {
        try
        {
            m_pResizer.reset(new OwnResizeFilter());
        }
        catch(...)
        {
            Destroy();
            return MFX_ERR_MEMORY_ALLOC;
        }
        if(mfxRes)
        {
            Destroy();
            return mfxRes;
        }
        mfxFrameInfo in  = par.mfx.FrameInfo;
        mfxFrameInfo out = par.mfx.FrameInfo;

        in.CropW = width;
        in.CropH = height;
        in.Width  = ALIGN16(width);
        in.Height = ALIGN16(height);

        //in case of RGB32 output no CSC is needed
        //capture into own RGB32, resize into user RGB32
        //but in case of NV12 output a temp surface is needed
        //capture into own RGB32, _convert to own NV12_, resize to user NV12
        if(MFX_FOURCC_NV12 == par.mfx.FrameInfo.FourCC)
        {
            m_bResize = true;
            in.FourCC = MFX_FOURCC_NV12;
            in.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            out.FourCC = MFX_FOURCC_NV12;
            out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            mfxFrameSurface1 surf = {0,0,0,0,in,0};
            for(mfxU16 i = 0; i < max(1,par.AsyncDepth); ++i)
            {
                try
                {
                    surf.Data.Y = new(std::nothrow) mfxU8[surf.Info.Width * surf.Info.Height * 3 / 2];
                    if(!surf.Data.Y)
                    {
                        Destroy();
                        return MFX_ERR_MEMORY_ALLOC;
                    }
                    surf.Data.UV = surf.Data.Y + surf.Info.Width * surf.Info.Height;
                    surf.Data.Pitch = surf.Info.Width;
                    m_InternalNV12ResizeSurfPool.push_back(surf);
                }
                catch(...)
                {
                    Destroy();
                    return MFX_ERR_MEMORY_ALLOC;
                }
            }
        }
        else if(MFX_FOURCC_RGB4 == par.mfx.FrameInfo.FourCC || DXGI_FORMAT_AYUV == par.mfx.FrameInfo.FourCC || MFX_FOURCC_AYUV_RGB4 == par.mfx.FrameInfo.FourCC)
        {
            in.FourCC = MFX_FOURCC_RGB4;
            in.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            out.FourCC = MFX_FOURCC_RGB4;
            out.ChromaFormat = MFX_CHROMAFORMAT_YUV444;

            ////emulate driver behavior, no height resize in case of RGB4
            //out.Height = in.Height;
            //out.CropH  = in.CropH;

            m_bResize = true;
        }

        mfxRes = m_pResizer->Init(in, out);
        if(mfxRes)
        {
            Destroy();
            return mfxRes;
        }
    }

    //Create own surface pool
    for(mfxU16 i = 0; i < max(1,par.AsyncDepth); ++i)
    {
        IDirect3DSurface9* pSurface = 0;
        mfxFrameSurface1 surf = {0,0,0,0,par.mfx.FrameInfo,0};
        surf.Info.Width  = width;
        surf.Info.Height = height;
        surf.Info.CropW  = (mfxU16) m_CropW;
        surf.Info.CropH  = (mfxU16) m_CropH;
        surf.Info.CropW  = width;
        surf.Info.CropH  = height;
        surf.Info.FourCC = MFX_FOURCC_RGB4;
        surf.Info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        hres = m_pDirect3DDevice->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &pSurface, NULL);
        if(FAILED(hres) || !pSurface)
        {
            if(pSurface) pSurface->Release();
            Destroy();
            return MFX_ERR_DEVICE_FAILED;
        }
        surf.Data.MemId = pSurface;
        m_InternalSurfPool.push_back(surf);
    }

    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out, const mfxU32 )
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    HRESULT hres = S_OK;

    mfxRes = CreateDeviceManager();
    if(mfxRes)
    {
        Destroy();
        return MFX_ERR_UNSUPPORTED;
    }

    mfxU16 width  = in.mfx.FrameInfo.CropW ? in.mfx.FrameInfo.CropW : in.mfx.FrameInfo.Width;
    mfxU16 height = in.mfx.FrameInfo.CropH ? in.mfx.FrameInfo.CropH : in.mfx.FrameInfo.Height;
    IDirect3DSurface9* pSurface = 0;
    hres = m_pDirect3DDevice->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &pSurface, NULL);
    if(FAILED(hres) || !pSurface)
    {
        if(pSurface) pSurface->Release();
        Destroy();
        return MFX_ERR_UNSUPPORTED;
    }

    hres = m_pDirect3DDevice->GetFrontBufferData(0, pSurface);
    if(FAILED(hres))
    {
        pSurface->Release();
        Destroy();
        return MFX_ERR_UNSUPPORTED;
    }

    D3DLOCKED_RECT sLockRect;
    hres = pSurface->LockRect(&sLockRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
    if(FAILED(hres))
    {
        pSurface->Release();
        Destroy();
        return MFX_ERR_UNSUPPORTED;
    }

    pSurface->Release();

    mfxRes = CheckCapabilities(in, out);
    if(mfxRes)
    {
        Destroy();
        return MFX_ERR_UNSUPPORTED;
    }

    Destroy();
    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out)
{
    if(!(MFX_FOURCC_NV12      == in.mfx.FrameInfo.FourCC ||
         MFX_FOURCC_RGB4      == in.mfx.FrameInfo.FourCC ||
         MFX_FOURCC_AYUV_RGB4 == in.mfx.FrameInfo.FourCC ||
         DXGI_FORMAT_AYUV     == in.mfx.FrameInfo.FourCC ))
    {
        if(out) out->mfx.FrameInfo.FourCC = 0;
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::Destroy()
{
    if(m_pColorConverter.get())
        m_pColorConverter.reset(0);
    if(m_pResizer.get())
        m_pResizer.reset(0);
    if(m_InternalSurfPool.size())
    {
        for(std::list<mfxFrameSurface1>::iterator it = m_InternalSurfPool.begin(); it != m_InternalSurfPool.end(); ++it)
        {
            if((IDirect3DSurface9*) it->Data.MemId)
            {
                ((IDirect3DSurface9*) it->Data.MemId)->Release();
                it->Data.MemId = 0;
            }
        }
        m_InternalSurfPool.clear();
    }
    if(m_InternalNV12ResizeSurfPool.size())
    {
        for(std::list<mfxFrameSurface1>::iterator it = m_InternalNV12ResizeSurfPool.begin(); it != m_InternalNV12ResizeSurfPool.end(); ++it)
        {
            if(it->Data.Y)
            {
                delete[] it->Data.Y;
                it->Data.Y = 0;
            }
        }
        m_InternalNV12ResizeSurfPool.clear();
    }
    if(m_pResizeBuffer)
    {
        delete[] m_pResizeBuffer;
        m_pResizeBuffer = 0;
    }
    if(m_IntStatusList.size())
    {
        m_IntStatusList.clear();
    }
    if(m_pD3D)
    {
        m_pD3D.Release();
        m_pD3D = 0;
    }
    if(m_pDirect3DDeviceManager)
    {
        m_pDirect3DDeviceManager.Release();
        m_pDirect3DDeviceManager = 0;
    }
    if(m_pDirect3DDevice)
    {
        m_pDirect3DDevice.Release();
        m_pDirect3DDevice = 0;
    }
    if(m_pDirectXVideoService)
    {
        m_pDirectXVideoService.Release();
        m_pDirectXVideoService = 0;
    }
    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::BeginFrame( mfxFrameSurface1* pSurf)
{
    pSurf;
    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::EndFrame( )
{
    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber)
{
    HRESULT hr = S_OK;
    mfxStatus mfxRes = MFX_ERR_NONE;
    bool wrn = false;

    mfxRes = CheckResolutionChage();
    if(MFX_ERR_NONE > mfxRes)
        return mfxRes;
    else if(MFX_ERR_NONE < mfxRes)
        wrn = true;

    mfxU16 CropW = surface_work->Info.CropW ? surface_work->Info.CropW : surface_work->Info.Width;
    mfxU16 CropH = surface_work->Info.CropH ? surface_work->Info.CropH : surface_work->Info.Height;

    mfxFrameSurface1* pSurf = GetFreeInternalSurface();
    if(!pSurf)
        return MFX_ERR_DEVICE_FAILED;
    mfxRes = m_pmfxCore->IncreaseReference(m_pmfxCore->pthis,&pSurf->Data);
    if(mfxRes)
        return mfxRes;

    IDirect3DSurface9* pSurface = (IDirect3DSurface9*) pSurf->Data.MemId;
    if(!pSurface)
        return MFX_ERR_DEVICE_FAILED;

    hr = m_pDirect3DDevice->GetFrontBufferData(0, pSurface);
    if(FAILED(hr))
    {
        m_pmfxCore->DecreaseReference(m_pmfxCore->pthis,&pSurf->Data);
        return MFX_ERR_DEVICE_FAILED;
    }

    D3DLOCKED_RECT sLockRect;
    hr = pSurface->LockRect(&sLockRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
    if(FAILED(hr))
    {
        m_pmfxCore->DecreaseReference(m_pmfxCore->pthis,&pSurf->Data);
        return MFX_ERR_DEVICE_FAILED;
    }

    pSurf->Data.Pitch = (mfxU16)sLockRect.Pitch;
    pSurf->Data.B = (mfxU8 *)sLockRect.pBits;
    pSurf->Data.G = pSurf->Data.B + 1;
    pSurf->Data.R = pSurf->Data.B + 2;

    bool unlock = false;

    //Copy Frame
    if(MFX_FOURCC_RGB4 == surface_work->Info.FourCC || DXGI_FORMAT_AYUV == surface_work->Info.FourCC || MFX_FOURCC_AYUV_RGB4 == surface_work->Info.FourCC)
    {
        //mfxRes = MFX_ERR_UNKNOWN;
        //if(m_bFastCopy && m_pFastCopy.get())
        //{
        //    mfxRes = m_pFastCopy->CMCopySysToGpu(*pSurf, *surface_work);
        //}
        //if(mfxRes)
        //    m_bFastCopy = false;

        //if(mfxRes || !m_bFastCopy)
        //{
            unlock = surface_work->Data.B ? false : true;
            if(unlock)
            {
                mfxRes = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_work->Data.MemId, &surface_work->Data);
                if(mfxRes)
                    return MFX_ERR_LOCK_MEMORY;
            }

            if(/*m_bResize*/pSurf->Info.CropH != CropH || pSurf->Info.CropW != CropW)
            {
                mfxRes = m_pResizer->RunFrameVPP(*pSurf, *surface_work);
                if(mfxRes)
                    return mfxRes;
            }
            else
            {
                mfxU32 i = 0;
                mfxU32 src_h = pSurf->Info.CropH ? pSurf->Info.CropH : pSurf->Info.Height;
                mfxU32 src_w = pSurf->Info.CropW ? pSurf->Info.CropW : pSurf->Info.Width;
                mfxU32 dst_h = surface_work->Info.CropH ? surface_work->Info.CropH : surface_work->Info.Height;
                mfxU32 dst_w = surface_work->Info.CropW ? surface_work->Info.CropW : surface_work->Info.Width;
                if(src_h > dst_h || src_w > dst_w)
                {
                    src_h = dst_h;
                    src_w = dst_w;
                }
                mfxU32 src_pitch = pSurf->Data.Pitch;
                mfxU32 dst_pitch = surface_work->Data.Pitch;

                mfxU8* src   = pSurf->Data.B;
                mfxU8* dst   = surface_work->Data.B;

                for (i = 0; i < src_h; i++)
                {
                    memcpy_s(dst + i*dst_pitch, 4*dst_w, src + i*src_pitch, 4*src_w);
                }
            }
        //}
    }
    else if(MFX_FOURCC_NV12 == surface_work->Info.FourCC)
    {
        unlock = surface_work->Data.Y ? false : true;
        if(unlock)
        {
            mfxRes = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_work->Data.MemId, &surface_work->Data);
            if(mfxRes)
                return MFX_ERR_LOCK_MEMORY;
        }

        if(/*m_bResize*/pSurf->Info.CropH != CropH || pSurf->Info.CropW != CropW)
        {
            mfxFrameSurface1* pNV12Surf = GetFreeIntResizeSurface();
            if(!pNV12Surf)
                return MFX_ERR_DEVICE_FAILED;
            mfxRes = m_pmfxCore->IncreaseReference(m_pmfxCore->pthis,&pNV12Surf->Data);
            if(mfxRes)
                return mfxRes;

            pSurf->Info.FourCC = MFX_FOURCC_RGB4;
            FilterVPP::InternalParam param = {};
            param.inPicStruct = MFX_PICSTRUCT_PROGRESSIVE;

            if(!m_pColorConverter.get())
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            mfxRes = m_pColorConverter->RunFrameVPP(pSurf,pNV12Surf,&param);
            if(mfxRes)
                return MFX_ERR_DEVICE_FAILED;

            mfxRes = m_pResizer->RunFrameVPP(*pNV12Surf, *surface_work);
            if(mfxRes)
                return mfxRes;

            mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis,&pNV12Surf->Data);
            if(mfxRes)
                return mfxRes;
        }
        else
        {
            pSurf->Info.FourCC = MFX_FOURCC_RGB4;
            FilterVPP::InternalParam param = {};
            param.inPicStruct = MFX_PICSTRUCT_PROGRESSIVE;

            if(!m_pColorConverter.get())
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            mfxRes = m_pColorConverter->RunFrameVPP(pSurf,surface_work,&param);
            if(mfxRes)
                return MFX_ERR_DEVICE_FAILED;
        }
    }

    hr = pSurface->UnlockRect();
    if(FAILED(hr))
    {
        mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis,&pSurf->Data);
        return MFX_ERR_DEVICE_FAILED;
    }

    mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis,&pSurf->Data);
    if(mfxRes)
        return mfxRes;

    if(unlock)
    {
        mfxRes = m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_work->Data.MemId, &surface_work->Data);
        if(mfxRes)
            return MFX_ERR_LOCK_MEMORY;
    }

    DESKTOP_QUERY_STATUS_PARAMS status;
    status.StatusReportFeedbackNumber = StatusReportFeedbackNumber;
    status.uStatus = 0;
    m_IntStatusList.push_back(status);

    if(wrn)
        return MFX_WRN_VIDEO_PARAM_CHANGED;
    else
        return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList)
{
    if(StatusList.size())
        StatusList.clear();

    StatusList = m_IntStatusList;
    m_IntStatusList.clear();
    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::AttachToLibraryDevice()
{
    mfxStatus mfxRes;
    HRESULT hres;

    mfxHDL hdl = 0;
    mfxRes = m_pmfxCore->GetHandle(m_pmfxCore->pthis, MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&hdl);
    if(mfxRes || !hdl)
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    else
    {
        m_pDirect3DDeviceManager = (IDirect3DDeviceManager9*)hdl;

        hres = m_pDirect3DDeviceManager->OpenDeviceHandle(&m_hDirectXHandle);
        if(FAILED(hres))
        {
            m_pDirect3DDeviceManager.Release();
            return MFX_ERR_DEVICE_FAILED;
        }

        hres = m_pDirect3DDeviceManager->LockDevice(m_hDirectXHandle, &m_pDirect3DDevice, FALSE);
        if(FAILED(hres))
        {
            m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
            return MFX_ERR_DEVICE_FAILED;
        }

        hres = m_pDirect3DDevice->GetDirect3D(&m_pD3D);
        if(FAILED(hres))
        {
            m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
            return MFX_ERR_DEVICE_FAILED;
        }

        hres = m_pDirect3DDeviceManager->UnlockDevice(m_hDirectXHandle, FALSE);
        if(FAILED(hres))
        {
            m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
            return MFX_ERR_DEVICE_FAILED;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::CreateDeviceManager(const UINT AdapterID)
{
    m_pD3D.Attach(Direct3DCreate9(D3D_SDK_VERSION));

    if (!m_pD3D)
    {
        return MFX_ERR_NULL_PTR;
    }

    POINT point = {0, 0};
    HWND window = WindowFromPoint(point);

    D3DPRESENT_PARAMETERS d3dParams;
    memset(&d3dParams, 0, sizeof(d3dParams));
    d3dParams.Windowed = TRUE;
    d3dParams.hDeviceWindow = window;
    d3dParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dParams.Flags = D3DPRESENTFLAG_VIDEO;
    d3dParams.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    d3dParams.BackBufferCount = 1;
    d3dParams.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dParams.BackBufferWidth = 0;
    d3dParams.BackBufferHeight = 0;

    HRESULT hr = m_pD3D->CreateDevice(
        /*D3DADAPTER_DEFAULT*/AdapterID,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &d3dParams,
        &m_pDirect3DDevice);
    if (FAILED(hr) || !m_pDirect3DDevice)
        return MFX_ERR_DEVICE_FAILED;

    UINT resetToken = 0;
    hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &m_pDirect3DDeviceManager);
    if (FAILED(hr) || !m_pDirect3DDeviceManager)
        return MFX_ERR_DEVICE_FAILED;

    hr = m_pDirect3DDeviceManager->ResetDevice(m_pDirect3DDevice, resetToken);
    if (FAILED(hr))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    return MFX_ERR_NONE;
}

mfxFrameSurface1* SW_D3D9_Capturer::GetFreeInternalSurface()
{
    if(!m_InternalSurfPool.size())
        return 0;
    mfxFrameSurface1* pSurf = 0; 

    for(std::list<mfxFrameSurface1>::iterator it = m_InternalSurfPool.begin(); it != m_InternalSurfPool.end(); ++it)
    {
        if( !(*it).Data.Locked)
        {
            pSurf = &(*it);
            break;
        }
    }
    return pSurf;
}

mfxFrameSurface1* SW_D3D9_Capturer::GetFreeIntResizeSurface()
{
    if(!m_InternalNV12ResizeSurfPool.size())
        return 0;
    mfxFrameSurface1* pSurf = 0; 

    for(std::list<mfxFrameSurface1>::iterator it = m_InternalNV12ResizeSurfPool.begin(); it != m_InternalNV12ResizeSurfPool.end(); ++it)
    {
        if( !(*it).Data.Locked)
        {
            pSurf = &(*it);
            break;
        }
    }
    return pSurf;
}

mfxStatus SW_D3D9_Capturer::GetDisplayResolution( mfxU32& width, mfxU32& height)
{
    //in case of non-default DPI and if application are not DPI-aware, GetSystemMetrics may return unreal resolution

    //SetProcessDPIAware(); //may affect user application

    mfxU32 sysMetW = GetSystemMetrics(SM_CXSCREEN);
    mfxU32 sysMetH = GetSystemMetrics(SM_CYSCREEN);

    //call GetAdapterDisplayMode to cross validate with GetSystemMetrics
    HRESULT hr = E_FAIL;
    D3DDISPLAYMODE d3ddisplaymode;
    memset(&d3ddisplaymode, 0, sizeof(d3ddisplaymode));
    if(m_pD3D)
        hr = m_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&d3ddisplaymode);

    if(SUCCEEDED(hr) && (d3ddisplaymode.Width > sysMetW || d3ddisplaymode.Height > sysMetH))
    {
        sysMetW = d3ddisplaymode.Width ;
        sysMetH = d3ddisplaymode.Height;
    }

    width  = sysMetW;
    height = sysMetH;

    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::CheckResolutionChage()
{
    mfxU32 width = 0;
    mfxU32 height = 0;
    GetDisplayResolution(width, height);

    if(width && height)
    {
        if(width == m_CropW && height == m_CropH)
            return MFX_ERR_NONE;

        if(width > m_width || height > m_height)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

        if(width != m_CropW || height != m_CropH)
        {
            m_CropW  = width;
            m_CropH  = height;

            for(std::list<mfxFrameSurface1>::iterator it = m_InternalSurfPool.begin(); it != m_InternalSurfPool.end(); ++it)
            {
                (*it).Info.CropW = (mfxU16) m_CropW;
                (*it).Info.CropH = (mfxU16) m_CropH;
            }
            for(std::list<mfxFrameSurface1>::iterator it = m_InternalNV12ResizeSurfPool.begin(); it != m_InternalNV12ResizeSurfPool.end(); ++it)
            {
                (*it).Info.CropW = (mfxU16) m_CropW;
                (*it).Info.CropH = (mfxU16) m_CropH;
            }

            mfxFrameInfo info[2];
            memset(&info, 0, sizeof(info));
            if(!m_pResizer.get())
                return MFX_ERR_DEVICE_FAILED;

            m_pResizer->GetParam(info[0], info[1]);

            info[0].CropW = (mfxU16) m_CropW;
            info[0].CropH = (mfxU16) m_CropH;

            try
            {
                m_pResizer.reset(new OwnResizeFilter());
            } catch (...) {
                return MFX_ERR_DEVICE_FAILED;
            }

            mfxStatus mfxRes = m_pResizer->Init(info[0], info[1]);
            if(mfxRes)
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

            return MFX_WRN_VIDEO_PARAM_CHANGED;
        }
    }

    return MFX_ERR_NONE;
}

} //namespace MfxCapture