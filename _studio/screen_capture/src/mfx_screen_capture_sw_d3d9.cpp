/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_d3d9.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_sw_d3d9.h"
#include "mfx_utils.h"

namespace MfxCapture
{

SW_D3D9_Capturer::SW_D3D9_Capturer(mfxCoreInterface* _core)
    :m_pmfxCore(_core)
{
    Mode = SW_D3D9;
}

SW_D3D9_Capturer::~SW_D3D9_Capturer()
{
    Destroy();
}

mfxStatus SW_D3D9_Capturer::CreateVideoAccelerator( mfxVideoParam const & par)
{
    HRESULT hres;

    mfxStatus mfxRes = MFX_ERR_NONE;

    mfxU16 width = 0;
    mfxU16 height = 0;
    width  = par.mfx.FrameInfo.CropW ? par.mfx.FrameInfo.CropW : par.mfx.FrameInfo.Width;
    height = par.mfx.FrameInfo.CropH ? par.mfx.FrameInfo.CropH : par.mfx.FrameInfo.Height;

    //will own internal device manager
    mfxRes = CreateDeviceManager();
    MFX_CHECK_STS(mfxRes);

    if(MFX_FOURCC_NV12 == par.mfx.FrameInfo.FourCC)
    {
        m_pColorConverter.reset(new MFXVideoVPPColorSpaceConversion(0, &mfxRes));
        if(mfxRes)
        {
            Destroy();
            return mfxRes;
        }
        mfxFrameInfo in = par.mfx.FrameInfo;
        mfxFrameInfo out = par.mfx.FrameInfo;
        in.FourCC = MFX_FOURCC_RGB4;
        m_pColorConverter.get()->Init(&in, &out);
    }

    //Create own surface pool
    for(mfxU16 i = 0; i < max(1,par.AsyncDepth); ++i)
    {
        IDirect3DSurface9* pSurface = 0;
        mfxFrameSurface1 surf = {0,0,0,0,par.mfx.FrameInfo,0};
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

mfxStatus SW_D3D9_Capturer::QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out)
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
    if(!(MFX_FOURCC_NV12 == in.mfx.FrameInfo.FourCC ||
         MFX_FOURCC_RGB4 == in.mfx.FrameInfo.FourCC))
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

mfxStatus SW_D3D9_Capturer::BeginFrame( mfxMemId MemId)
{
    MemId;
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

    mfxFrameSurface1 temp_surface;
    memset(&temp_surface,0,sizeof(temp_surface));

    temp_surface.Info = surface_work->Info;
    temp_surface.Info.Width = temp_surface.Info.CropW = pSurf->Info.CropW;
    temp_surface.Info.Height = temp_surface.Info.CropH = pSurf->Info.CropH;
    temp_surface.Data.Pitch = (mfxU16)sLockRect.Pitch;
    temp_surface.Data.B = (mfxU8 *)sLockRect.pBits;
    temp_surface.Data.G = temp_surface.Data.B + 1;
    temp_surface.Data.R = temp_surface.Data.B + 2;

    bool unlock = surface_work->Data.Y ? false : true;
    if(unlock)
    {
        mfxRes = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_work->Data.MemId, &surface_work->Data);
        if(mfxRes) return MFX_ERR_LOCK_MEMORY;
    }

    //Copy Frame
    if(MFX_FOURCC_RGB4 == surface_work->Info.FourCC)
    {
        mfxU32 i = 0;
        mfxU32 h = 1080;
        mfxU32 w = temp_surface.Info.Width;
        mfxU32 src_pitch = temp_surface.Data.Pitch;
        mfxU32 dst_pitch = surface_work->Data.Pitch;

        mfxU8* src   = temp_surface.Data.B;
        mfxU8* dst   = surface_work->Data.B;
        for (i = 0; i < h; i++)
        {
            memcpy_s(dst + i*dst_pitch, 4*w, src + i*src_pitch, 4*w);
        }
    }
    else if(MFX_FOURCC_NV12 == surface_work->Info.FourCC)
    {
        temp_surface.Info.FourCC = MFX_FOURCC_RGB4;
        FilterVPP::InternalParam param = {};
        param.inPicStruct = MFX_PICSTRUCT_PROGRESSIVE;

        if(!m_pColorConverter.get())
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        mfxRes = m_pColorConverter.get()->RunFrameVPP(&temp_surface,surface_work,&param);
        if(mfxRes)
            return MFX_ERR_DEVICE_FAILED;
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
        if(mfxRes) return MFX_ERR_LOCK_MEMORY;
    }

    DESKTOP_QUERY_STATUS_PARAMS status;
    status.StatusReportFeedbackNumber = StatusReportFeedbackNumber;
    status.uStatus = 0;
    m_IntStatusList.push_back(status);

    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList)
{

    StatusList = m_IntStatusList;
    m_IntStatusList.clear();
    return MFX_ERR_NONE;
}

mfxStatus SW_D3D9_Capturer::CreateDeviceManager()
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
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &d3dParams,
        &m_pDirect3DDevice);
    if (FAILED(hr) || !m_pDirect3DDevice)
        return MFX_ERR_NULL_PTR;

    UINT resetToken = 0;
    hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &m_pDirect3DDeviceManager);
    if (FAILED(hr) || !m_pDirect3DDeviceManager)
        return MFX_ERR_NULL_PTR;

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


} //namespace MfxCapture