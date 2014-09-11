/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

/////////////////////////////////////////////////

Keyboard:
    Alt + Enter : Mode change between Window mode and Fullscreen mode.
    END : Enable/Disable the frame drop debug spew.
*/

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <mmsystem.h>

//name was marked as #pragma deprecated
#pragma warning (disable : 4995)
#include <tchar.h>
#include <strsafe.h>
#include <initguid.h>
#include <d3d9.h>
//nameless structs are exist
///ze option also adds warning on its own
#pragma warning (disable : 4201 4995)
#include <dxva2api.h>

#include "mfx_pipeline_defs.h"
#include "mfx_screen_render.h"

#define GWL_USERDATA        (-21)

#define WINDOW_NAME  _T("mfx_player")

const D3DFORMAT VIDEO_RENDER_TARGET_FORMAT = D3DFMT_X8R8G8B8;

const UINT DWM_BUFFER_COUNT  = 4;

//
// Type definitions.
//

typedef HRESULT (WINAPI * PFNDWMISCOMPOSITIONENABLED)(
    __out BOOL* pfEnabled
    );

typedef HRESULT (WINAPI * PFNDWMENABLECOMPOSITION)(
    UINT uCompositionAction
    );


typedef HRESULT (WINAPI * PFNDWMGETCOMPOSITIONTIMINGINFO)(
    __in HWND hwnd,
    __out DWM_TIMING_INFO* pTimingInfo
    );

typedef HRESULT (WINAPI * PFNDWMSETPRESENTPARAMETERS)(
    __in HWND hwnd,
    __inout DWM_PRESENT_PARAMETERS* pPresentParams
    );


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    
    ScreenRender* pRender = (ScreenRender*)(GetWindowLongPtr(hwnd, GWL_USERDATA));
    if (pRender)
    {
        switch (uMsg)
        {
            HANDLE_MSG(hwnd, WM_DESTROY, pRender->OnDestroy);
            HANDLE_MSG(hwnd, WM_KEYDOWN, pRender->OnKey);
            HANDLE_MSG(hwnd, WM_PAINT,   pRender->OnPaint);
            HANDLE_MSG(hwnd, WM_SIZE,    pRender->OnSize);
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

mfxStatus GetHDL(mfxHDL /*pthis*/, mfxMemId mid, mfxHDL *handle)
{
    *handle = mid;
    return MFX_ERR_NONE;
}

ScreenRender::ScreenRender(IVideoSession *core
                          , mfxStatus *status
                          , const InitParams &refParams)
    : MFXVideoRender(core, status)
    , m_initParams(refParams)
    , m_pCore(core)
    , m_bSetup()
    , m_mfxFrameInfo()
    , m_bDwmQueuingDisabledPrinted()
    , m_bSetWindowText(refParams.window.windowsStyle != WS_POPUP)
{
}


mfxStatus ScreenRender::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    request->Info = par->mfx.FrameInfo;

#ifdef BUFFERING_SIZE
    request->NumFrameMin = request->NumFrameSuggested = 1 + BUFFERING_SIZE;
    request->NumFrameSuggested = request->NumFrameMin;
#else
    request->NumFrameMin = 1;
    request->NumFrameSuggested = 1;
#endif
    //request->Type = 
    return MFX_ERR_NONE;
}

mfxStatus ScreenRender::SetupDevice()
{
    if (m_bSetup)
        return MFX_ERR_NONE;

    m_Hwnd                           = NULL;
    m_hRgb9rastDLL                   = NULL;
    m_hDwmApiDLL                     = NULL;
    m_pfnD3D9GetSWInfo               = NULL;
    m_pfnDwmIsCompositionEnabled     = NULL;
    m_pfnDwmGetCompositionTimingInfo = NULL;
    m_pfnDwmSetPresentParameters     = NULL;
    m_bTimerSet                      = FALSE;
    m_hTimer                         = NULL;
    m_StartSysTime                   = 0;
    m_PreviousTime                   = 0;
    m_bDspFrameDrop                  = FALSE;
    m_bDwmQueuing                    = FALSE;
    m_FrameCount                     = 0;
    m_LastFrameCount                 = 0;
    m_LastTime                       = 0;
    m_OverlayTextSize                = 0;
    m_nAdapter                       = m_initParams.nAdapter;

    VideoWindow::InitParams iParams = m_initParams.window;
    iParams.pTitle      = WINDOW_NAME;
    iParams.pWindowProc = WindowProc;

    m_OverlayText     = iParams.pOverlayText;
    m_OverlayTextSize = iParams.OverlayTextSize;
    font = CreateFont(m_OverlayTextSize, 0, 0, 0, 300, false, false, false, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Verdana");
    // Create window
    m_pWindow->Initialize(iParams);
    if (NULL == (m_Hwnd = m_pWindow->GetWindowHandle()))
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    SetWindowLong(m_Hwnd, GWL_USERDATA, PtrToLong(this));

    if (!InitializeModule() || !InitializeDevice())
    {
        return MFX_ERR_UNKNOWN;
    }
    
    m_bSetup = true;

    return MFX_ERR_NONE;
}

mfxStatus ScreenRender::Init(mfxVideoParam *par, const TCHAR * /*pFilename*/)
{
    MFX_CHECK_STS(SetupDevice());

    m_mfxFrameInfo = par->mfx.FrameInfo;

    if (!InitializeTimer())
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

BOOL ScreenRender::RegisterSoftwareRasterizer()
{
    HRESULT hr = S_OK;

    if (!m_hRgb9rastDLL)
    {
        return FALSE;
    }

    //hr = m_pD3D9->RegisterSoftwareDevice(m_pfnD3D9GetSWInfo);

    if (FAILED(hr))
    {
        MFX_TRACE_ERR(VM_STRING("RegisterSoftwareDevice failed with error 0x") << std::hex<<hr);
        return FALSE;
    }

    return TRUE;
}

BOOL ScreenRender::InitializeDevice()
{
    //HRESULT hr;

    // Init D3D device manager
    if (MFX_ERR_NONE != m_initParams.pDevice->Reset(m_Hwnd, m_initParams.window.directLocation, TRUE == m_pWindow->isWindowed()))
    {
        MFX_TRACE_ERR(VM_STRING("MFXD3DDevice::Reset failed\n"));
        return FALSE;
    }
    //if (MFX_ERR_NONE != m_initParams.pDevice->Init(m_nAdapter, m_Hwnd, TRUE == m_pWindow->isWindowed(), VIDEO_RENDER_TARGET_FORMAT, 1, NULL))
    //{
    //    DBGMSG((TEXT("MFXD3DDevice::Init failed\n")));
    //    return FALSE;
    //}

    // set to both sessions
    //MFXVideoCORE_SetHandle(m_pHWCore, MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, m_pDeviceManager);
    //mfxSession pExtCore = m_pCore->operator mfxSession();
    //MFXVideoCORE_SetHandle(pExtCore, MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, m_pDeviceManager);

    //
    // Retrieve a back buffer as the video render target.
    //
    //hr = m_pD3DD9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_pRenderSurface);
    //if (FAILED(hr))
    //{
    //    DBGMSG((TEXT("GetBackBuffer failed with error 0x%x.\n"), hr));
    //    return FALSE;
    //}

    return TRUE;
}

BOOL ScreenRender::EnableDwmQueuing()
{
    HRESULT hr;

    //
    // DWM is not available.
    //
    if (!m_hDwmApiDLL)
    {
        return TRUE;
    }

    //
    // Check to see if DWM is currently enabled.
    //
    BOOL bDWM = FALSE;

    hr = ((PFNDWMISCOMPOSITIONENABLED) m_pfnDwmIsCompositionEnabled)(&bDWM);

    if (FAILED(hr))
    {
        MFX_TRACE_ERR(VM_STRING("DwmIsCompositionEnabled failed with error 0x") << std::hex<<hr);
        return FALSE;
    }

    //
    // DWM queuing is disabled when DWM is disabled.
    //
    if (!bDWM)
    {
        hr = ((PFNDWMENABLECOMPOSITION)m_pfnDwmEnableComposition )(DWM_EC_ENABLECOMPOSITION);
        if (FAILED(hr))
        {
            if (!m_bDwmQueuingDisabledPrinted) {
                MFX_TRACE_ERR(VM_STRING("DwmEnableComposition failed with error 0x") << std::hex<<hr);
            }
        }
        hr = ((PFNDWMISCOMPOSITIONENABLED) m_pfnDwmIsCompositionEnabled)(&bDWM);
    }
    
    if (!bDWM)
    {
        m_bDwmQueuing = FALSE;
        
        if (!m_bDwmQueuingDisabledPrinted) {
            PrintInfo(VM_STRING("DwmIsCompositionEnabled"), VM_STRING("FALSE"));
            m_bDwmQueuingDisabledPrinted = TRUE;
        }

        return TRUE;
    }

    m_bDwmQueuingDisabledPrinted = FALSE;

    //
    // DWM queuing is enabled already.
    //
    if (m_bDwmQueuing)
    {
        return TRUE;
    }

    //
    // Retrieve DWM refresh count of the last vsync.
    //
    DWM_TIMING_INFO dwmti = {0};

    dwmti.cbSize = sizeof(dwmti);

    hr = ((PFNDWMGETCOMPOSITIONTIMINGINFO) m_pfnDwmGetCompositionTimingInfo)(NULL, &dwmti);

    if (FAILED(hr))
    {
        MFX_TRACE_ERR(VM_STRING("DwmGetCompositionTimingInfo failed with error 0x") << std::hex<<hr);
        return FALSE;
    }

    //
    // Enable DWM queuing from the next refresh.
    //
    DWM_PRESENT_PARAMETERS dwmpp = {0};

    dwmpp.cbSize             = sizeof(dwmpp);
    dwmpp.fQueue             = TRUE;
    dwmpp.cRefreshStart      = dwmti.cRefresh + 1;
    dwmpp.cBuffer            = DWM_BUFFER_COUNT;
    dwmpp.fUseSourceRate     = TRUE;
    dwmpp.rateSource.uiNumerator  = 5000;
    dwmpp.rateSource.uiDenominator = 1;
    //dwmpp.cRefreshesPerFrame = 1;
    dwmpp.eSampling          = DWM_SOURCE_FRAME_SAMPLING_POINT;

    hr = ((PFNDWMSETPRESENTPARAMETERS) m_pfnDwmSetPresentParameters)(m_Hwnd, &dwmpp);

    if (FAILED(hr))
    {
        MFX_TRACE_ERR(VM_STRING("DwmSetPresentParameters failed with error 0x") << std::hex<<hr);
        return FALSE;
    }

    //
    // DWM queuing is enabled.
    //
    m_bDwmQueuing = TRUE;

    return TRUE;
}


BOOL ScreenRender::ResetDevice()
{

    RECT dummyRect = {0};
    if (MFX_ERR_NONE == m_initParams.pDevice->Reset(m_Hwnd, dummyRect, TRUE == m_pWindow->isWindowed()))
    {
        return TRUE;
    }
    m_initParams.pDevice->Close();

    if (InitializeDevice())
    {
        return TRUE;
    }

    //
    // Fallback to Window mode, if failed to initialize Fullscreen mode.
    //
    if (m_pWindow->isWindowed())
    {
        return FALSE;
    }

    m_initParams.pDevice->Close();

    if (!m_pWindow->ChangeFullscreenMode(FALSE))
    {
        return FALSE;
    }

    if (InitializeDevice())
    {
        return TRUE;
    }

    return FALSE;
}


LONGLONG ScreenRender::GetTimestamp()
{
    DWORD  currentTime;
    DWORD  currentSysTime    = timeGetTime();
    mfxF64 framerate         = GetFrameRate(&m_mfxFrameInfo);
    mfxF64 fframerate_MSPF   = ((1000.0 + framerate / 2.0) / framerate);
    UINT   framerate_100NSPF = (UINT)(fframerate_MSPF * 10000.0);

    if (m_StartSysTime > currentSysTime)
    {
        currentTime = currentSysTime + (0xFFFFFFFF - m_StartSysTime);
    }
    else
    {
        currentTime = currentSysTime - m_StartSysTime;
    }

    DWORD frame = (DWORD)(currentTime / fframerate_MSPF);
    DWORD delta = (DWORD)((currentTime - m_PreviousTime) / fframerate_MSPF);

    if (delta > 1)
    {
        if (m_bDspFrameDrop)
        {
            MFX_TRACE_INFO(VM_STRING("Frame(s) dropped ") << delta - 1);
        }
    }

    if (delta > 0)
    {
        m_PreviousTime = currentTime;
    }

    return frame * LONGLONG(framerate_100NSPF);
}

mfxStatus ScreenRender::RenderFrame(mfxFrameSurface1 *pSurface, mfxEncodeCtrl * pCtrl)
{
    MSG msg = {0};
    if (!pSurface) return MFX_ERR_NONE; // on EOS

    m_FrameCount++;
    LONGLONG CurrentTime = 0;
    GetSystemTimeAsFileTime((FILETIME*)&CurrentTime);
    double time_diff = (double)(CurrentTime - m_LastTime)*1e-7;
    if ((!m_LastTime || time_diff > 1) && m_bSetWindowText)
    {
        char window_title[MAX_PATH];
        sprintf(window_title, "mfx_player, %.0f fps", (m_FrameCount - m_LastFrameCount)/time_diff);
        SetWindowTextA(m_Hwnd, window_title);
        m_LastTime = CurrentTime;
        m_LastFrameCount = m_FrameCount;
    }

    bool stopProcessing = FALSE;
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if ( ( msg.message == WM_KEYDOWN &&  msg.wParam == VK_SPACE ) ||
                 ( msg.message == WM_LBUTTONDOWN  ) )
            {
                stopProcessing = stopProcessing ? FALSE : TRUE;
                continue;
            }

            if (PreTranslateMessage(msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);

            continue;
        }

        if ( ! stopProcessing )
            break;
    }


    if (msg.message == WM_QUIT)
    {
        return MFX_ERR_UNKNOWN;
    }

    // Render frame
    if (!m_bSetup) MFX_ERR_NOT_INITIALIZED;


    RECT rect;
    GetClientRect(m_Hwnd, &rect);
    if (IsRectEmpty(&rect))
    {
        return MFX_ERR_UNKNOWN;
    }

    //
    // Re-enable DWM queuing if it is not enabled.
    //
    //EnableDwmQueuing();
    
    MFX_CHECK_STS(m_initParams.pDevice->RenderFrame(pSurface, m_pCore->GetFrameAllocator()));
    if ( m_OverlayTextSize )
    {
        HDC hdc;
        hdc = GetDC(m_Hwnd);
        SetBkMode(hdc, TRANSPARENT);
        (HFONT)SelectObject( hdc, font );
        SetTextColor( hdc, RGB(255, 255, 255));
        DrawTextEx(hdc,
                   (LPWSTR)m_OverlayText, 
                   (int)_tcslen(m_OverlayText),
                   &rect,
                   DT_BOTTOM | DT_CENTER | DT_SINGLELINE ,
                   NULL);

        ReleaseDC(m_Hwnd, hdc);
        UpdateWindow(m_Hwnd);
    }
    return MFXVideoRender::RenderFrame(pSurface, pCtrl);
}


VOID ScreenRender::OnDestroy(HWND /*hwnd*/)
{
    PostQuitMessage(0);
}


VOID ScreenRender::OnKey(HWND hwnd, UINT vk, BOOL fDown, int /*cRepeat*/, UINT /*flags*/)
{
    if (!fDown)
    {
        return;
    }

    if (vk == VK_ESCAPE)
    {
        DestroyWindow(hwnd);
        return;
    }

    if (!m_bSetup)
    {
        return;
    }

    switch (vk)
    {
    case VK_END :
        m_bDspFrameDrop = !m_bDspFrameDrop;
        break;
    }
}


VOID ScreenRender::OnPaint(HWND hwnd)
{
    ValidateRect(hwnd , NULL);

//    ProcessVideo();
}


VOID ScreenRender::OnSize(HWND hwnd, UINT /*state*/, int /*cx*/, int /*cy*/)
{
    if (!m_bSetup)
    {
        return;
    }

    RECT rect;
    GetClientRect(hwnd, &rect);

    if (IsRectEmpty(&rect))
    {
        return;
    }

    //
    // Do not reset the device while the mode change is in progress.
    //
    /*if (m_bInModeChange)
    {
        return;
    }*/

    if (!ResetDevice())
    {
        DestroyWindow(hwnd);
        return;
    }

    InvalidateRect(hwnd , NULL , FALSE);
}


BOOL ScreenRender::InitializeTimer()
{
    m_hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

    if (!m_hTimer)
    {
        MFX_TRACE_ERR(VM_STRING("CreateWaitableTimer failed with error ") << GetLastError());
        return FALSE;
    }

    LARGE_INTEGER li = {0};

    mfxF64 framerate         = GetFrameRate(&m_mfxFrameInfo);
    UINT   framerate_MSPF    = (UINT)((1000.0 + framerate / 2.0) / framerate);

    if (!SetWaitableTimer(m_hTimer,
                          &li,
                          framerate_MSPF,
                          NULL,
                          NULL,
                          FALSE))
    {
        MFX_TRACE_ERR(VM_STRING("SetWaitableTimer failed with error ") << GetLastError());
        return FALSE;
    }

    m_bTimerSet = (timeBeginPeriod(1) == TIMERR_NOERROR);

    m_StartSysTime = timeGetTime();

    return TRUE;
}


VOID ScreenRender::DestroyTimer()
{
    if (m_bTimerSet)
    {
        timeEndPeriod(1);
        m_bTimerSet = FALSE;
    }

    if (m_hTimer)
    {
        CloseHandle(m_hTimer);
        m_hTimer = NULL;
    }
}


BOOL ScreenRender::PreTranslateMessage(const MSG& msg)
{
    //
    // Only interested in Alt + Enter.
    //
    if (msg.message != WM_SYSKEYDOWN || msg.wParam != VK_RETURN)
    {
        return FALSE;
    }

    if (!m_bSetup)
    {
        return TRUE;
    }

    RECT rect;
    GetClientRect(msg.hwnd, &rect);

    if (IsRectEmpty(&rect))
    {
        return TRUE;
    }

    // Change window mode
    if (!m_pWindow->ChangeFullscreenMode(m_pWindow->isWindowed()))
    {
        return FALSE;
    }

    if (ResetDevice())
    {
        return TRUE;
    }

    DestroyWindow(msg.hwnd);
    return TRUE;
}


BOOL ScreenRender::InitializeModule()
{
    //
    // Load these DLLs dynamically because these may not be available prior to Vista.
    //
    m_hRgb9rastDLL = LoadLibrary(TEXT("rgb9rast.dll"));

    if (!m_hRgb9rastDLL)
    {
        MFX_TRACE_ERR(VM_STRING("LoadLibrary(rgb9rast.dll) failed with error ") << GetLastError());
        goto SKIP_RGB9RAST;
    }

    m_pfnD3D9GetSWInfo = GetProcAddress(m_hRgb9rastDLL, "D3D9GetSWInfo");

    if (!m_pfnD3D9GetSWInfo)
    {
        MFX_TRACE_ERR(VM_STRING("GetProcAddress(D3D9GetSWInfo) failed with error ") << GetLastError());
        return FALSE;
    }

SKIP_RGB9RAST:

    m_hDwmApiDLL = LoadLibrary(TEXT("dwmapi.dll"));

    if (!m_hDwmApiDLL)
    {
        MFX_TRACE_ERR(VM_STRING("LoadLibrary(dwmapi.dll) failed with error ") << GetLastError());
        goto SKIP_DWMAPI;
    }

    m_pfnDwmIsCompositionEnabled = GetProcAddress(m_hDwmApiDLL, "DwmIsCompositionEnabled");

    if (!m_pfnDwmIsCompositionEnabled)
    {
        MFX_TRACE_ERR(VM_STRING("GetProcAddress(DwmIsCompositionEnabled) failed with error ") << GetLastError());
        return FALSE;
    }

    m_pfnDwmEnableComposition = GetProcAddress(m_hDwmApiDLL, "DwmEnableComposition");

    if (!m_pfnDwmEnableComposition)
    {
        MFX_TRACE_ERR(VM_STRING("GetProcAddress(DwmEnableComposition) failed with error ") << GetLastError());
        return FALSE;
    }

    m_pfnDwmGetCompositionTimingInfo = GetProcAddress(m_hDwmApiDLL, "DwmGetCompositionTimingInfo");

    if (!m_pfnDwmGetCompositionTimingInfo)
    {
        MFX_TRACE_ERR(VM_STRING("GetProcAddress(DwmGetCompositionTimingInfo) failed with error ") << GetLastError());
        return FALSE;
    }

    m_pfnDwmSetPresentParameters = GetProcAddress(m_hDwmApiDLL, "DwmSetPresentParameters");

    if (!m_pfnDwmSetPresentParameters)
    {
        MFX_TRACE_ERR(VM_STRING("GetProcAddress(DwmSetPresentParameters) failed with error ") << GetLastError());
        return FALSE;
    }

SKIP_DWMAPI:

    return TRUE;
}

mfxStatus ScreenRender::GetDevice(IHWDevice **ppDevice)
{
    if (m_initParams.mThreadPool) {
        MFX_CHECK_STS(m_initParams.mThreadPool->Queue(
            bind_any(std::mem_fun(&ScreenRender::SetupDevice), this))->Synhronize(MFX_INFINITE));
    } else {
        //need to create since this call looks like the first one to d3d device
        MFX_CHECK_STS(SetupDevice());
    }
    MFX_CHECK_POINTER(ppDevice);
    
    *ppDevice = m_initParams.pDevice.get();
    
    return MFX_ERR_NONE;

}

ScreenRender::~ScreenRender()
{
//    SAFE_DELETE(m_pVPP);
//    MFXClose(m_pHWCore);
      DestroyTimer();
      //DestroyD3D9();
}

#endif // #if defined(_WIN32) || defined(_WIN64)
