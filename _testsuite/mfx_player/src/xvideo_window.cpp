/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
#ifdef VAAPI_SURFACES_SUPPORT

#include "mfx_pipeline_defs.h"
#include "xvideo_window.h"

XVideoWindow::XVideoWindow()
{
    m_Hwnd          = NULL;
}

bool XVideoWindow::Initialize(const InitParams &refInit)
{

    char* currentDisplay = getenv("DISPLAY");
    m_px11Display = NULL;
    if (currentDisplay)
        m_px11Display = XOpenDisplay(currentDisplay);
    else
        m_px11Display = XOpenDisplay(VAAPI_X_DEFAULT_DISPLAY);

    mfxU32 ScreenNumber = DefaultScreen(m_px11Display);
    mfxU32 displayWidth = DisplayWidth(m_px11Display, ScreenNumber);
    mfxU32 displayHeight = DisplayHeight(m_px11Display, ScreenNumber);

    int w = 1;
    int h = 1;
    int x = 0;
    int y = 0;

    if (refInit.nX && refInit.nY)
    {
        w = (displayWidth)  / refInit.nX;
        h = (displayHeight)  / refInit.nY;
        x = w * (refInit.nPosition % refInit.nX) /*+ m_RectWindow.left*/;
        y = h * (refInit.nPosition / refInit.nY) /*+ m_RectWindow.top*/;
    }

    m_Hwnd = XCreateSimpleWindow(m_px11Display,
                            RootWindow(m_px11Display, ScreenNumber),
                            x,
                            y,
                            w,
                            h,
                            0,
                            0,
                            WhitePixel(m_px11Display, ScreenNumber));

    if (!m_Hwnd)
    {
        return false;
    }

    XMapWindow(m_px11Display, m_Hwnd);
    XSync(m_px11Display, False);

    return true;
}

#endif // #ifdef VAAPI_SURFACES_SUPPORT
#endif // #if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
