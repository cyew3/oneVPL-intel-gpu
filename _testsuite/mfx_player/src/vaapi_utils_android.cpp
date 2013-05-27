/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifdef LIBVA_SUPPORT
#if defined(ANDROID)

/* Undefining ANDROID here we exclude part of va_android.h file in which
 * Android-specific headers are touched. This permit us to build code under
 * Android NDK.
 */
#undef ANDROID
#include <va/va_android.h>
#define ANDROID

#include "vaapi_utils.h"

typedef unsigned int vaapiAndroidDisplay;

#define VAAPI_ANDROID_DEFAULT_DISPLAY 0x18c34078

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
    vaapiAndroidDisplay* display = NULL;

    if (isVAInitialized()) return MFX_ERR_NONE;

    m_display = display = (vaapiAndroidDisplay*)malloc(sizeof(vaapiAndroidDisplay));
    if (NULL == m_display) sts = MFX_ERR_NOT_INITIALIZED;
    else *display = VAAPI_ANDROID_DEFAULT_DISPLAY;

    if (MFX_ERR_NONE == sts)
    {
        m_va_dpy = vaGetDisplay(m_display);
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
        free(m_display);
        m_display = NULL;
    }
}

#endif // #if defined(ANDROID)
#endif // #ifdef LIBVA_SUPPORT
