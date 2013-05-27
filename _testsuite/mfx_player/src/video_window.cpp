/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_pipeline_defs.h"
#include "video_window.h"

static const vm_char CLASS_NAME[] = TEXT("Window Class");

#if defined(DBG) || defined(DEBUG) || defined(_DEBUG)
VOID DbgPrint(PCTSTR format, ...)
{
    va_list args;
    va_start(args, format);

    vm_char string[MAX_PATH];

    if (SUCCEEDED(StringCbVPrintf(string, sizeof(string), format, args)))
    {
        OutputDebugString(string);
        vm_string_printf(string);
    }
    else
    {
        DebugBreak();
    }
}
#endif

VideoWindow::VideoWindow()
    : m_nMonitorCurrent()
    , m_nMonitorRequired()
    , m_Hwnd()
    , m_bWindowed(TRUE)
    , m_bInModeChange()
    , m_RectWindow()
{
}

BOOL CALLBACK VideoWindow::MonitorEnumProc(HMONITOR /*hMonitor*/,
                                           HDC /*hdcMonitor*/,
                                           LPRECT lprcMonitor,
                                           LPARAM dwData)
{
    VideoWindow * pWindow = reinterpret_cast<VideoWindow *>(dwData);
    RECT r = {0};
    if (NULL == lprcMonitor)
        lprcMonitor = &r;

    vm_char monitor_name[1024];

    if (pWindow->m_nMonitorCurrent == pWindow->m_nMonitorRequired)
    {
        pWindow->m_RectWindow = *lprcMonitor;
        vm_string_sprintf_s(monitor_name, MFX_ARRAY_SIZE(monitor_name), VM_STRING("MONITOR* %d"), pWindow->m_nMonitorCurrent++);
    }
    else
    {
        vm_string_sprintf_s(monitor_name, MFX_ARRAY_SIZE(monitor_name), VM_STRING("MONITOR  %d"), pWindow->m_nMonitorCurrent++);
    }
    
    PrintInfo(monitor_name, VM_STRING("%d,%d,%d,%d\n"), lprcMonitor->left, lprcMonitor->top, lprcMonitor->right, lprcMonitor->bottom);

    return TRUE;
}

BOOL VideoWindow::Initialize(const InitParams &refInit)
{
    WNDCLASS wc = {0};

    wc.lpfnWndProc   = refInit.pWindowProc;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc))
    {
        DBGMSG((TEXT("RegisterClass failed with error %d.\n"), GetLastError()));
        return FALSE;
    }
    
    int w = CW_USEDEFAULT; 
    int h = CW_USEDEFAULT; 
    int x = CW_USEDEFAULT; 
    int y = CW_USEDEFAULT; 

    m_nMonitorRequired = refInit.nMonitor;
    EnumDisplayMonitors(NULL, NULL, &VideoWindow::MonitorEnumProc, (LPARAM)this);

    if (refInit.nX && refInit.nY )
    {
        w = (m_RectWindow.right - m_RectWindow.left)  / refInit.nX;
        h = (m_RectWindow.bottom - m_RectWindow.top)  / refInit.nY;
        x = w * (refInit.nPosition % refInit.nX) + m_RectWindow.left;
        y = h * (refInit.nPosition / refInit.nX) + m_RectWindow.top;
    }
    //location specified to match video resolution
    else if (refInit.directLocation.right != 0)
    {
        RECT rect (refInit.directLocation);
        AdjustWindowRect(&rect, refInit.windowsStyle, FALSE);
        w = rect.right - rect.left;
        h = rect.bottom - rect.top;
        x = m_RectWindow.left;
        y = m_RectWindow.top;
    }

    PrintInfo(VM_STRING("Rendering Window"), VM_STRING("%d,%d,%d,%d\n"), x, y, x+w, y+w);
    
    //
    // Start in Window mode regardless of the initial mode.
    //
    m_Hwnd = CreateWindow(CLASS_NAME,
                          refInit.pTitle,
                          refInit.windowsStyle,
                          x,
                          y,
                          w,
                          h,
                          NULL,
                          NULL,
                          GetModuleHandle(NULL),
                          NULL);

    if (!m_Hwnd)
    {
        DBGMSG((TEXT("CreateWindow failed with error %d.\n"), GetLastError()));
        return FALSE;
    }

    ::ShowWindow(m_Hwnd, SW_SHOW);
    ::UpdateWindow(m_Hwnd);

    //
    // Change the window from Window mode to Fullscreen mode.
    //
    if (refInit.bFullscreen)
    {
        if (!ChangeFullscreenMode(TRUE))
        {
            //
            // If failed, revert to Window mode.
            //
            if (!ChangeFullscreenMode(FALSE))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

BOOL VideoWindow::ChangeFullscreenMode(BOOL bFullscreen)
{
    //
    // Mark the mode change in progress to prevent the device is being reset in OnSize.
    // This is because these API calls below will generate WM_SIZE messages.
    //
    m_bInModeChange = TRUE;

    m_bWindowed = !bFullscreen;

    if (bFullscreen)
    {
        //
        // Save the window position.
        //
        if (!GetWindowRect(m_Hwnd, &m_RectWindow))
        {
            DBGMSG((TEXT("GetWindowRect failed with error %d.\n"), GetLastError()));
            return FALSE;
        }

        if (!SetWindowLongPtr(m_Hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE))
        {
            DBGMSG((TEXT("SetWindowLongPtr failed with error %d.\n"), GetLastError()));
            return FALSE;
        }

        if (!::SetWindowPos(m_Hwnd,
                          HWND_NOTOPMOST,
                          0,
                          0,
                          GetSystemMetrics(SM_CXSCREEN),
                          GetSystemMetrics(SM_CYSCREEN),
                          SWP_FRAMECHANGED))
        {
            DBGMSG((TEXT("SetWindowPos failed with error %d.\n"), GetLastError()));
            return FALSE;
        }
    }
    else
    {
        if (!SetWindowLongPtr(m_Hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE))
        {
            DBGMSG((TEXT("SetWindowLongPtr failed with error %d.\n"), GetLastError()));
            return FALSE;
        }

        //
        // Restore the window position.
        //
        if (!::SetWindowPos(m_Hwnd,
                          HWND_NOTOPMOST,
                          m_RectWindow.left,
                          m_RectWindow.top,
                          m_RectWindow.right - m_RectWindow.left,
                          m_RectWindow.bottom - m_RectWindow.top,
                          SWP_FRAMECHANGED))
        {
            DBGMSG((TEXT("SetWindowPos failed with error %d.\n"), GetLastError()));
            return FALSE;
        }
    }

    m_bInModeChange = FALSE;

    

    return TRUE;
}

#endif // #if defined(_WIN32) || defined(_WIN64)
