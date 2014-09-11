/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __VIDEO_WINDOW_H
#define __VIDEO_WINDOW_H

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <d3d9.h>
#include <tchar.h>
#include <strsafe.h>


/////////////////////////////////////////////////////////////////////////

class VideoWindow
{
public:
    struct InitParams
    {
        BOOL    bFullscreen;
        DWORD   windowsStyle;
        int     nX;
        int     nY;
        int     nPosition;
        int     nMonitor;
        RECT    directLocation;
        WNDPROC pWindowProc;
        const vm_char *pTitle;
        LPCTSTR  pOverlayText;
        int      OverlayTextSize;

        InitParams( BOOL  bFullscreen = false
                  , const vm_char *pOverlayText = NULL
                  , int OverlayTextSize = 0
                  , DWORD windowsStyle = WS_OVERLAPPEDWINDOW
                  , int nX = 0
                  , int nY = 0
                  , int nPosition = 0
                  , const vm_char *pTitle = NULL
                  , WNDPROC pWindowProc = NULL
                  , int nMonitor = 0) :
            directLocation()
        {
            this->pTitle          = pTitle;
            this->bFullscreen     = bFullscreen ;
            this->pWindowProc     = pWindowProc;
            this->windowsStyle    = windowsStyle ;
            this->nX              = nX ;
            this->nY              = nY ;
            this->nPosition       = nPosition;
            this->nMonitor        = nMonitor ;
            this->pOverlayText    = pOverlayText;
            this->OverlayTextSize = OverlayTextSize;
        }
    };
    VideoWindow();
    virtual ~VideoWindow() { if (m_Hwnd)DestroyWindow(m_Hwnd);}

    BOOL Initialize( const InitParams &refInit);
    HWND GetWindowHandle() { return m_Hwnd; }
    BOOL isWindowed() { return m_bWindowed; }
    BOOL ChangeFullscreenMode(BOOL bFullscreen = TRUE);

private:
    static BOOL CALLBACK MonitorEnumProc(
                                HMONITOR hMonitor,
                                HDC hdcMonitor,
                                LPRECT lprcMonitor,
                                LPARAM dwData);
    int  m_nMonitorCurrent;
    int  m_nMonitorRequired;
    HWND m_Hwnd;
    BOOL m_bWindowed;
    BOOL m_bInModeChange;
    RECT m_RectWindow;
};

#endif // #if defined(_WIN32) || defined(_WIN64)

#endif // __VIDEO_WINDOW_H
