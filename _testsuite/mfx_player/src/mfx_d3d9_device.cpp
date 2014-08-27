/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_pipeline_defs.h"
#include "mfx_d3d9_device.h"
#include "atlbase.h"

MFXD3D9Device::MFXD3D9Device()
    : m_D3DPP()
    , m_pLibD3D9()
    , m_pLibDXVA2()
    , m_pD3D9Ex()
    , m_pD3DD9Ex()
    , m_pDeviceManager()
    , m_bModeFullscreen(false)
{
}

mfxStatus MFXD3D9Device::Init(mfxU32 nAdapter,
                              WindowHandle hWindow,
                              bool bIsWindowed,
                              mfxU32 VIDEO_RENDER_TARGET_FORMAT,
                              int BACK_BUFFER_COUNT,
                              const vm_char *pDXVA2libname,
                              bool )
{
    HRESULT hr;
    D3DDISPLAYMODEEX fsc = {0};
    D3DDISPLAYMODEEX *fullscreen = 0;

    hr = myDirect3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9Ex);
    if (FAILED(hr))
    {
        PipelineTrace((VM_STRING("myDirect3DCreate9Ex failed with error 0x%x.\n"), hr));
    }

    if (!m_pD3D9Ex)
    {
        MFX_TRACE_ERR(VM_STRING("Direct3DCreate9 failed"));
        return MFX_ERR_UNKNOWN;
    }

    //
    // Register the inbox software rasterizer if software D3D9 could be used.
    // CreateDevice(D3DDEVTYPE_SW) will fail if software device is not registered.
    //
    //RegisterSoftwareRasterizer();

    if (bIsWindowed)
    {
        RECT r;
        GetClientRect((HWND)hWindow, &r);
        int x = GetSystemMetrics(SM_CXSCREEN);
        int y = GetSystemMetrics(SM_CYSCREEN);
        m_D3DPP.BackBufferWidth  = min(r.right - r.left, x);
        m_D3DPP.BackBufferHeight = min(r.bottom - r.top, y);
        m_D3DPP.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
        fullscreen = NULL;
    }
    else
    {
        m_D3DPP.BackBufferWidth  = GetSystemMetrics(SM_CXSCREEN);
        m_D3DPP.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);

        fsc.Format = (D3DFORMAT)VIDEO_RENDER_TARGET_FORMAT;
        fsc.Height = m_D3DPP.BackBufferHeight;
        fsc.Width  = m_D3DPP.BackBufferWidth;
        fsc.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
        fsc.Size = sizeof(D3DDISPLAYMODEEX);

        /* Proper framerate must be specified */
        m_D3DPP.FullScreen_RefreshRateInHz = 60;
        fsc.RefreshRate                    = 60;

        fullscreen = &fsc;
        m_bModeFullscreen = true;
    }

    m_D3DPP.BackBufferFormat           = (D3DFORMAT)VIDEO_RENDER_TARGET_FORMAT;
    m_D3DPP.BackBufferCount            = BACK_BUFFER_COUNT;
    m_D3DPP.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
    m_D3DPP.hDeviceWindow              = (HWND)hWindow;
    m_D3DPP.Windowed                   = bIsWindowed;
    m_D3DPP.Flags                      = D3DPRESENTFLAG_VIDEO;
    m_D3DPP.PresentationInterval       = D3DPRESENT_INTERVAL_DEFAULT;

    //
    // Mark the back buffer lockable if software DXVA2 could be used.
    // This is because software DXVA2 device requires a lockable render target
    // for the optimal performance.
    //
    //if (true) //m_vpp.m_bDXVA2SW
    {
        m_D3DPP.Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    }

    AdjustD3DPP(&m_D3DPP);
    //
    // First try to create a hardware D3D9 device.
    //
    //if (true) //m_bD3D9HW
    {
        hr = m_pD3D9Ex->CreateDeviceEx(nAdapter,
            D3DDEVTYPE_HAL,
            (HWND)hWindow,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING |
            D3DCREATE_FPU_PRESERVE |
            D3DCREATE_MULTITHREADED,
            &m_D3DPP,
            fullscreen,
            &m_pD3DD9Ex);

        if (FAILED(hr))
        {
            MFX_TRACE_INFO(VM_STRING("CreateDevice(HAL) failed with error 0x") << std::hex<<hr);
        }
    }

    //
    // Next try to create a software D3D9 device.
    //
    if (!m_pD3DD9Ex)
    {
        //RegisterSoftwareRasterizer();

        hr = m_pD3D9Ex->CreateDeviceEx(nAdapter,
            D3DDEVTYPE_SW,
            (HWND)hWindow,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &m_D3DPP,
            fullscreen,
            &m_pD3DD9Ex);

        if (FAILED(hr))
        {
            MFX_TRACE_INFO(VM_STRING("CreateDevice(SW) failed with error 0x") << std::hex<<hr);
        }
    }

    // Note: DXVA2 emulators may work without D3D device
    if (!m_pD3DD9Ex && (NULL == pDXVA2libname || 0 == vm_string_strlen(pDXVA2libname)))
    {
        return MFX_ERR_UNKNOWN;
    }

    // create IDirect3DDeviceManager9
    UINT ResetToken = 0;
    hr = myDXVA2CreateDirect3DDeviceManager9(&ResetToken, &m_pDeviceManager, pDXVA2libname);
    if (FAILED(hr) || !m_pDeviceManager) 
    {
        MFX_TRACE_ERR(VM_STRING("DXVA2CreateDirect3DDeviceManager9 failed with error 0x") << std::hex<<hr);
        return MFX_ERR_UNKNOWN;
    }

    hr = m_pDeviceManager->ResetDevice(m_pD3DD9Ex, ResetToken);
    if (FAILED(hr))
    {
        MFX_TRACE_ERR(VM_STRING("ResetDevice failed with error 0x") << std::hex<<hr);
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXD3D9Device::Reset(WindowHandle hWindow,
                               bool bWindowed)
{
    HRESULT hr;

    if (m_pD3DD9Ex)
    {
        m_D3DPP.Windowed = bWindowed;
        if (m_D3DPP.Windowed)
        {
            RECT r;
            GetClientRect((HWND)hWindow, &r);
            int x = GetSystemMetrics(SM_CXSCREEN);
            int y = GetSystemMetrics(SM_CYSCREEN);
            m_D3DPP.BackBufferWidth  = min(r.right - r.left, x);
            m_D3DPP.BackBufferHeight = min(r.bottom - r.top, y);
        }
        else
        {
            m_D3DPP.BackBufferWidth  = GetSystemMetrics(SM_CXSCREEN);
            m_D3DPP.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
        }
        m_D3DPP.hDeviceWindow              = (HWND)hWindow;
    }

    AdjustD3DPP(&m_D3DPP);

    //
    // Reset will change the parameters, so use a copy instead.
    //
    D3DPRESENT_PARAMETERS d3dpp = m_D3DPP;

    hr = m_pD3DD9Ex->Reset(&d3dpp);

    if (FAILED(hr))
    {
        MFX_TRACE_ERR(VM_STRING("Reset failed with error 0x") << std::hex<<hr);
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

void MFXD3D9Device::Close()
{
    MFX_SAFE_RELEASE(m_pDeviceManager);
    MFX_SAFE_RELEASE(m_pD3DD9Ex);
    MFX_SAFE_RELEASE(m_pD3D9Ex);
}

MFXD3D9Device::~MFXD3D9Device()
{
    Close();
    //FreeLibrary(m_pLibD3D9);
    //FreeLibrary(m_pLibDXVA2);
}

typedef HRESULT (STDAPICALLTYPE *FUNC1)(UINT SDKVersion, IDirect3D9Ex**d);
typedef HRESULT (STDAPICALLTYPE *FUNC2)(__out UINT* pResetToken,
    __deref_out IDirect3DDeviceManager9** ppDeviceManager);

HRESULT MFXD3D9Device::myDirect3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex**d)
{
    m_pLibD3D9 = LoadLibrary(_T("d3d9.dll"));
    if (NULL == m_pLibD3D9) 
    { 
        MFX_TRACE_ERR(VM_STRING("LoadLibrary(\"d3d9.dll\")) failed with error ") << GetLastError());
        return NULL;
    }

    FUNC1 pFunc = (FUNC1)GetProcAddress(m_pLibD3D9, "Direct3DCreate9Ex"); 
    if (NULL == pFunc) 
    { 
        MFX_TRACE_ERR(VM_STRING("GetProcAddress(\"Direct3DCreate9\")  failed with error ")<< GetLastError());
        return NULL;
    }

    return pFunc(SDKVersion, d);
}

HRESULT MFXD3D9Device::myDXVA2CreateDirect3DDeviceManager9(UINT* pResetToken,
    IDirect3DDeviceManager9** ppDeviceManager,
    const vm_char *pDXVA2LibName)
{
    if (NULL == pDXVA2LibName || 0 == vm_string_strlen(pDXVA2LibName))
    {
        pDXVA2LibName = VM_STRING("dxva2.dll");
    }

    m_pLibDXVA2 = LoadLibrary(pDXVA2LibName);
    if (NULL == m_pLibDXVA2) 
    { 
        MFX_TRACE_ERR(VM_STRING("LoadLibrary(\"") << pDXVA2LibName <<VM_STRING("\") failed with error ") << GetLastError());
        return NULL;
    }

    FUNC2 pFunc = (FUNC2)GetProcAddress(m_pLibDXVA2, "DXVA2CreateDirect3DDeviceManager9");
    if (NULL == pFunc) 
    { 
        MFX_TRACE_ERR(VM_STRING("GetProcAddress(\"DXVA2CreateDirect3DDeviceManager9\")  failed with error ")<< GetLastError());
        return NULL;
    }

    {
        std::list<tstring> allmodules;
        std::list<tstring>::iterator it;
        GetLoadedModulesList(allmodules);
        for (it = allmodules.begin(); it != allmodules.end(); it++)
        {
            if (it->find(_T("dxva")) != tstring::npos)
            {
                PrintDllInfo(_T("DXVA DLL"), it->c_str());
            }
            if (it->find(_T("xn0_")) != tstring::npos)
            {
                PrintDllInfo(_T("XN DLL"), it->c_str());
            }
        }
    }

    return pFunc(pResetToken, ppDeviceManager);
}

mfxStatus MFXD3D9Device::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    if (MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 == type && pHdl != NULL)
    {
        *pHdl = m_pDeviceManager;

        return MFX_ERR_NONE;
    }
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXD3D9Device::RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * /*Palloc*/)
{
    //
    // Check the current status of D3D9 device.
    //
    HRESULT hr = m_pD3DD9Ex->TestCooperativeLevel();

    switch (hr)
    {
        case D3D_OK :
            break;

        case D3DERR_DEVICELOST :
        {
            MFX_TRACE_ERR(VM_STRING("TestCooperativeLevel returned D3DERR_DEVICELOST"));
            return MFX_ERR_DEVICE_LOST;
        }

        case D3DERR_DEVICENOTRESET :
        {
            MFX_TRACE_ERR(VM_STRING("TestCooperativeLevel returned D3DERR_DEVICENOTRESET"));
            //if (!ResetDevice())
            {
                return MFX_ERR_UNKNOWN;
            }
            break;
        }

        default :
        {
            MFX_TRACE_ERR(VM_STRING("TestCooperativeLevel failed with error 0x") << std::hex<<hr);
            return MFX_ERR_UNKNOWN;
        }
    }

    //RECT target;
    //GetClientRect(m_Hwnd, &target);

    // input mfxFrameSurface1
    //in = *pSurface;
    //in.Data.TimeStamp = GetTimestamp();
    //// convert MemId to IDirect3DSurface9 pointer
    //mfxFrameAllocator *pFrameAllocator = m_pCore->GetFrameAllocator();
    //if (!in.Data.Y && in.Data.MemId && pFrameAllocator && pFrameAllocator->GetHDL)
    //{
    //     MFX_CHECK_STS(pFrameAllocator->GetHDL(pFrameAllocator->pthis, in.Data.MemId, &in.Data.MemId));
    //}

    // output mfxFrameSurface1
    /*out.Data.MemId  = m_pRenderSurface;
    out.Info.Width  = (mfxU16)target.right;
    out.Info.Height = (mfxU16)target.bottom;*/

    RECT source = { pSurface->Info.CropX
                  , pSurface->Info.CropY
                  , pSurface->Info.CropX + pSurface->Info.CropW
                  , pSurface->Info.CropY + pSurface->Info.CropH};
    
    CComPtr<IDirect3DSurface9> pBackBuffer;
    hr = m_pD3DD9Ex->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
    
    D3DSURFACE_DESC dsc ;
    pBackBuffer->GetDesc(&dsc);

    double dTargetAspect = (double)dsc.Width / dsc.Height;
    double dSrcAspect = 
        (double)pSurface->Info.CropW * (0 == pSurface->Info.AspectRatioW ? 1 : pSurface->Info.AspectRatioW) / 
        (double)pSurface->Info.CropH / (0 == pSurface->Info.AspectRatioH ? 1 : pSurface->Info.AspectRatioH);
    
    RECT dest;
    if (dSrcAspect >dTargetAspect)
    {
        dest.left = 0;
        dest.right = dsc.Width;
        dest.top = 0;
        dest.bottom = dsc.Height;
    }
    else
    {
        dest.top = 0;
        dest.bottom = dsc.Height;
        int width = (int) ( dsc.Height * dSrcAspect);
        dest.left = (dsc.Width - width) / 2;
        dest.right = dest.left + width;
    }
    RECT targetRect = {0};
    GetClientRect(m_D3DPP.hDeviceWindow, &targetRect);

    {
        MPA_TRACE("D3DRender::StretchRect");
        hr = m_pD3DD9Ex->StretchRect((IDirect3DSurface9*)pSurface->Data.MemId, NULL, pBackBuffer, NULL, D3DTEXF_NONE);
        if (FAILED(hr))
        {
            MFX_TRACE_ERR(VM_STRING("StretchRect failed with error 0x") << std::hex<<hr);
            return MFX_ERR_UNKNOWN;
        }
    }

    {
        MPA_TRACE("D3DRender::Present");

        if ( IsOverlay() )
        {
            // Overlay mode requires specifying source/dest rects.
            // Otherwise it fails with invalid args error.
            hr = m_pD3DD9Ex->Present(&source, &dest, NULL, NULL);
        }
        else
        {
            hr = m_pD3DD9Ex->Present(NULL, NULL, NULL, NULL);
        }
        if (FAILED(hr))
        {
            MFX_TRACE_ERR(VM_STRING("Present failed with error 0x") << std::hex<<hr);
            return MFX_ERR_UNKNOWN;
        }
    }

    return MFX_ERR_NONE;
}

#endif // #if defined(_WIN32) || defined(_WIN64)

