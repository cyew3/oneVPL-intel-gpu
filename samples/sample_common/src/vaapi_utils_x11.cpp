/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2015 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(LIBVA_X11_SUPPORT)

#include "sample_defs.h"
#include "vaapi_utils_x11.h"

#include <dlfcn.h>
#if defined(X11_DRI3_SUPPORT)
#include <fcntl.h>
#endif

#define VAAPI_X_DEFAULT_DISPLAY ":0.0"

const char* PROCESSING_DRIVER_NAME="iHD";
const char* RENDERING_DRIVER_NAME="i965";

X11LibVA::X11LibVA(void)
    : CLibVA(MFX_LIBVA_X11)
    , m_display(0)
{
    VAStatus va_res = VA_STATUS_SUCCESS;
    mfxStatus sts = MFX_ERR_NONE;
    int major_version = 0, minor_version = 0;
    char* currentDisplay = getenv("DISPLAY");

#if !defined(X11_DRI3_SUPPORT)
    try
    {
        if (currentDisplay)
            m_display = m_x11lib.XOpenDisplay(currentDisplay);
        else
            m_display = m_x11lib.XOpenDisplay(VAAPI_X_DEFAULT_DISPLAY);

        if (NULL == m_display) sts = MFX_ERR_NOT_INITIALIZED;
        if (MFX_ERR_NONE == sts)
        {
            m_va_dpy = m_vax11lib.vaGetDisplay(m_display);
            m_va_dpy_render = m_vax11lib.vaGetDisplay(m_display);

            if (!m_va_dpy || !m_va_dpy_render)
            {
                m_x11lib.XCloseDisplay(m_display);
                sts = MFX_ERR_NULL_PTR;
            }
        }
        if (MFX_ERR_NONE == sts)
        {
            if (!setenv("LIBVA_DRIVER_NAME", PROCESSING_DRIVER_NAME, 1)) {
            va_res = m_libva.vaInitialize(m_va_dpy, &major_version, &minor_version);
            sts = va_to_mfx_status(va_res);
            if (MFX_ERR_NONE != sts) {
                m_x11lib.XCloseDisplay(m_display);
                }
                unsetenv("LIBVA_DRIVER_NAME");
            } else {
                sts = MFX_ERR_NOT_INITIALIZED;
            }
        }
        if (MFX_ERR_NONE == sts)
        {
        if (!msdk_strcmp(PROCESSING_DRIVER_NAME, RENDERING_DRIVER_NAME)) {
            // same driver
            m_va_dpy_render = m_va_dpy;
        } else {
            m_va_dpy_render = m_vax11lib.vaGetDisplay(m_display);
            if (!m_va_dpy_render)
            {
                m_libva.vaTerminate(m_va_dpy);
                m_x11lib.XCloseDisplay(m_display);
                sts = MFX_ERR_NULL_PTR;
            }
            if (!setenv("LIBVA_DRIVER_NAME", RENDERING_DRIVER_NAME, 1)) {
                va_res = m_libva.vaInitialize(m_va_dpy_render, &major_version, &minor_version);
                sts = va_to_mfx_status(va_res);
                if (MFX_ERR_NONE != sts)
                {
                    m_libva.vaTerminate(m_va_dpy);
                    m_x11lib.XCloseDisplay(m_display);
                }
                unsetenv("LIBVA_DRIVER_NAME");
            }
            }
        }
    }
#else
    try
    {
        if (currentDisplay)
            m_display = m_x11lib.XOpenDisplay(currentDisplay);
        else
            m_display = m_x11lib.XOpenDisplay(VAAPI_X_DEFAULT_DISPLAY);

        if (NULL == m_display) sts = MFX_ERR_NOT_INITIALIZED;
        if (MFX_ERR_NONE == sts)
        {
            m_va_dpy = m_vax11lib.vaGetDisplay(m_display);

            if (!m_va_dpy)
            {
                m_x11lib.XCloseDisplay(m_display);
                sts = MFX_ERR_NULL_PTR;
            }
        }
        if (MFX_ERR_NONE == sts)
        {
            va_res = m_libva.vaInitialize(m_va_dpy, &major_version, &minor_version);
            sts = va_to_mfx_status(va_res);
            if (MFX_ERR_NONE != sts)
            {
                m_x11lib.XCloseDisplay(m_display);
            }
        }
    }
#endif // X11_DRI3_SUPPORT
    catch(std::exception& )
    {
        sts = MFX_ERR_NOT_INITIALIZED;
    }

    if (MFX_ERR_NONE != sts) throw std::bad_alloc();
}

X11LibVA::~X11LibVA(void)
{
#if !defined(X11_DRI3_SUPPORT)
    if (m_va_dpy_render && (m_va_dpy_render != m_va_dpy))
    {
        m_libva.vaTerminate(m_va_dpy_render);
    }
#endif
    if (m_va_dpy)
    {
        m_libva.vaTerminate(m_va_dpy);
    }
    if (m_display)
    {
        m_x11lib.XCloseDisplay(m_display);
    }
}

#endif // #if defined(LIBVA_X11_SUPPORT)
