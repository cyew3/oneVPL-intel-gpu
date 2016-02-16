/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

/////////////////////////////////////////////////

*/

#if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
#ifdef LIBVA_X11_SUPPORT

#include "vm_strings.h"

#include "mfx_pipeline_defs.h"
#include "mfx_vaapi_render.h"

#define GWL_USERDATA        (-21)

#define WINDOW_NAME  VM_STRING("mfx_player")


mfxStatus GetHDL(mfxHDL /*pthis*/, mfxMemId mid, mfxHDL *handle)
{
    *handle = mid;
    return MFX_ERR_NONE;
}

ScreenVAAPIRender::ScreenVAAPIRender(IVideoSession *core
                          , mfxStatus *status
                          , const InitParams &refParams)
    : MFXVideoRender(core, status)
    , m_initParams(refParams)
    , m_pCore(core)
    , m_mfxFrameInfo()
    , m_bSetup()
{
}

mfxStatus ScreenVAAPIRender::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
#ifdef BUFFERING_SIZE
    request->Info = par->mfx.FrameInfo;
    request->NumFrameMin = request->NumFrameSuggested = 1 + BUFFERING_SIZE;
#else
    par;
    request;
#endif
    //request->Type = 
    return MFX_ERR_NONE;
}

mfxStatus ScreenVAAPIRender::SetupDevice()
{
    if (m_bSetup)
        return MFX_ERR_NONE;

    XVideoWindow::InitParams iParams = m_initParams.window;
    iParams.pTitle      = WINDOW_NAME;

    // Create window
    m_pWindow->Initialize(iParams);
    if (NULL == (m_Hwnd = m_pWindow->GetWindowHandle()))
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (NULL == (m_Dpy = (Display *)m_pWindow->GetDisplay()))
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (!InitializeDevice())
    {
        return MFX_ERR_UNKNOWN;
    }
    
    m_bSetup = true;

    return MFX_ERR_NONE;
}

mfxStatus ScreenVAAPIRender::Init(mfxVideoParam *par, const vm_char * /*pFilename*/)
{
    MFX_CHECK_STS(SetupDevice());

    m_mfxFrameInfo = par->mfx.FrameInfo;

    return MFX_ERR_NONE;
}

bool ScreenVAAPIRender::InitializeDevice()
{
    // Init D3D device manager
    if (MFX_ERR_NONE != m_initParams.pDevice->Init(0, m_Dpy, true == m_pWindow->isWindowed(), 0, 1, NULL))
    {
        //DBGMSG((TEXT("MFXD3DDevice::Init failed\n")));
        return false;
    }

    if (MFX_ERR_NONE != m_initParams.pDevice->Init(1, m_Hwnd, true == m_pWindow->isWindowed(), 0, 1, NULL))
    {
        //DBGMSG((TEXT("MFXD3DDevice::Init failed\n")));
        return false;
    }

    return true;
}

bool ScreenVAAPIRender::ResetDevice()
{

        if (MFX_ERR_NONE == m_initParams.pDevice->Reset(NULL, NULL, true == m_pWindow->isWindowed()))
        {
            return true;
        }
        m_initParams.pDevice->Close();
        //DestroyD3D9();
    //}

    if (InitializeDevice()/* && MFX_ERR_NONE == m_pVPP->Init(&m_VPPParams)*/)
    {
        return true;
    }

    //
    // Fallback to Window mode, if failed to initialize Fullscreen mode.
    //
    if (m_pWindow->isWindowed())
    {
        return false;
    }

//    m_pVPP->Close();
    m_initParams.pDevice->Close();
    //DestroyD3D9();
/*
    if (!m_pWindow->ChangeFullscreenMode(FALSE))
    {
        return FALSE;
    }
*/
    if (InitializeDevice()/* && MFX_ERR_NONE == m_pVPP->Init(&m_VPPParams)*/)
    {
        return true;
    }

    return true;
}


mfxStatus ScreenVAAPIRender::RenderFrame(mfxFrameSurface1 *pSurface, mfxEncodeCtrl * pCtrl)
{
    if (!pSurface) return MFX_ERR_NONE; // on EOS

    // Render frame
    if (!m_bSetup) MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_STS(m_initParams.pDevice->RenderFrame(pSurface, m_pCore->GetFrameAllocator()));

    return MFXVideoRender::RenderFrame(pSurface, pCtrl);
}

mfxStatus ScreenVAAPIRender::GetDevice(IHWDevice **ppDevice)
{
    //need to create since this call looks like the first one to d3d device
    MFX_CHECK_STS(SetupDevice());
    MFX_CHECK_POINTER(ppDevice);
    
    *ppDevice = m_initParams.pDevice.get();
    return MFX_ERR_NONE;
}

ScreenVAAPIRender::~ScreenVAAPIRender()
{
}

#endif // #ifdef LIBVA_X11_SUPPORT
#endif // #if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
