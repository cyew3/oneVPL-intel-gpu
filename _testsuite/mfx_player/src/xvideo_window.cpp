/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2020 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
#ifdef LIBVA_X11_SUPPORT

#include "mfx_pipeline_defs.h"
#include "xvideo_window.h"
#include "vaapi_utils.h"

#include "vaapi_utils_drm.h"
#include "vaapi_utils_x11.h"

XVideoWindow::XVideoWindow()
    : m_pX11LibVA(new X11LibVA)
{
    m_Hwnd          = 0;
    m_px11Display   = nullptr;
}

XVideoWindow::~XVideoWindow()
{
    delete m_pX11LibVA;
}

bool XVideoWindow::Initialize(const InitParams &refInit)
{
    MfxLoader::XLib_Proxy & x11lib = m_pX11LibVA->GetX11();

    char* currentDisplay = getenv("DISPLAY");
    m_px11Display = NULL;
    if (currentDisplay)
        m_px11Display = x11lib.XOpenDisplay(currentDisplay);
    else
        m_px11Display = x11lib.XOpenDisplay(VAAPI_X_DEFAULT_DISPLAY);

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

    m_Hwnd = x11lib.XCreateSimpleWindow(m_px11Display,
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

    x11lib.XMapWindow(m_px11Display, m_Hwnd);
    x11lib.XSync(m_px11Display, False);

    return true;
}

#endif // #ifdef LIBVA_X11_SUPPORT
#endif // #if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
