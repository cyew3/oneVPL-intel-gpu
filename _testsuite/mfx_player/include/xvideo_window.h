/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __XVIDEO_WINDOW_H
#define __XVIDEO_WINDOW_H

#if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
#ifdef LIBVA_X11_SUPPORT

#include <X11/Xlib.h>
#define VAAPI_X_DEFAULT_DISPLAY ":0.0"

#define MFX_ZERO_MEM(x) memset(&x, 0, sizeof(x))

class XVideoWindow
{
public:
    struct InitParams
    {
        bool    bFullscreen;
        int     nX;
        int     nY;
        int     nPosition;
        const vm_char *pTitle;

        InitParams( bool  bFullscreen = false
                  , int nX = 0
                  , int nY = 0
                  , int nPosition = 0
                  , const vm_char *pTitle = NULL)
        {
            this->pTitle        = pTitle;
            this->bFullscreen   = bFullscreen ;
            this->nX            = nX ;
            this->nY            = nY ;
            this->nPosition     = nPosition;
        }
    };
    XVideoWindow();
    virtual ~XVideoWindow() { /*if (m_Hwnd)DestroyWindow(m_Hwnd);*/}

    bool Initialize( const InitParams &refInit);
    mfxHDL GetWindowHandle() { return (mfxHDL)m_Hwnd; }
    mfxHDL GetDisplay() { return (mfxHDL)m_px11Display; }
    bool isWindowed() { return true; }
   // bool ChangeFullscreenMode(BOOL bFullscreen = TRUE);

private:

    Display *m_px11Display;
    Window m_Hwnd;
};

#endif // #ifdef LIBVA_X11_SUPPORT
#endif // #if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)

#endif // __XVIDEO_WINDOW_H
