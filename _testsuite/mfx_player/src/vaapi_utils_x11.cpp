/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifdef LIBVA_SUPPORT
#if defined(LINUX32) || defined(LINUX64)


#include <stdlib.h>
#include <va/va_x11.h>
#include "vaapi_utils.h"

#define VAAPI_X_DEFAULT_DISPLAY ":0.0"

#define VAAPI_GET_X_DISPLAY(_display) (Display*)(_display)

CLibVA::CLibVA(void):
    m_display(NULL),
    m_va_dpy(NULL),
    m_bVAInitialized(false)
{
}

CLibVA::~CLibVA(void)
{
    Close();
}

mfxStatus CLibVA::Init(void)
{
    VAStatus va_res = VA_STATUS_SUCCESS;
    mfxStatus sts = MFX_ERR_NONE;
    int major_version = 0, minor_version = 0;
    char* currentDisplay = getenv("DISPLAY");

    if (isVAInitialized()) return MFX_ERR_NONE;

    if (currentDisplay) m_display = XOpenDisplay(currentDisplay);
    else m_display = XOpenDisplay(VAAPI_X_DEFAULT_DISPLAY);

    if (NULL == m_display) sts = MFX_ERR_NOT_INITIALIZED;
    if (MFX_ERR_NONE == sts)
    {
        m_va_dpy = vaGetDisplay(VAAPI_GET_X_DISPLAY(m_display));
        va_res = vaInitialize(m_va_dpy, &major_version, &minor_version);
        sts = va_to_mfx_status(va_res);
    }
    if (MFX_ERR_NONE == sts) m_bVAInitialized = true;
    return sts;
}

void CLibVA::Close(void)
{
    if (m_bVAInitialized)
    {
        vaTerminate(m_va_dpy);
        m_va_dpy = NULL;
    }
    if (NULL != m_display)
    {
        XCloseDisplay(VAAPI_GET_X_DISPLAY(m_display));
        m_display = NULL;
    }
}

#endif // #if defined(LINUX32) || defined(LINUX64)
#endif // #ifdef LIBVA_SUPPORT
