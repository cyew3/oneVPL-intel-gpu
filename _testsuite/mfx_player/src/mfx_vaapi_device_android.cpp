/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(ANDROID)
#ifdef VAAPI_SURFACES_SUPPORT

#include "mfx_pipeline_defs.h"
#include "mfx_vaapi_device_android.h"
#include "vaapi_utils.h"

#define VAAPI_ANDROID_DEFAULT_DISPLAY 0x18c34078

MFXVAAPIDevice::MFXVAAPIDevice()
{
    m_android_display = NULL;
    m_va_dpy = NULL;
}

mfxStatus MFXVAAPIDevice::Init(mfxU32 nAdapter,
                               WindowHandle handle,
                               bool bIsWindowed,
                               mfxU32 VIDEO_RENDER_TARGET_FORMAT,
                               int BACK_BUFFER_COUNT,
                               const vm_char *pDXVA2LIBNAME,
                               bool )
{
    VAStatus va_res = VA_STATUS_SUCCESS;
    mfxStatus mfx_res = MFX_ERR_NONE;
    int major_version = 0, minor_version = 0;

    if (NULL == handle)
    {
        m_android_display = (vaapiAndroidDisplay*)malloc(sizeof(vaapiAndroidDisplay));
        if (NULL == m_android_display) mfx_res = MFX_ERR_NOT_INITIALIZED;
        else *m_android_display = VAAPI_ANDROID_DEFAULT_DISPLAY;
    }
    else m_android_display = (vaapiAndroidDisplay*)handle;

    if (NULL == m_android_display) mfx_res = MFX_ERR_NOT_INITIALIZED;
    if (MFX_ERR_NONE == mfx_res)
    {
        m_va_dpy = vaGetDisplay(m_android_display);
        va_res = vaInitialize(m_va_dpy, &major_version, &minor_version);
        mfx_res = va_to_mfx_status(va_res);
    }
    return mfx_res;
}

mfxStatus MFXVAAPIDevice::Reset(bool bWindowed)
{
    return MFX_ERR_NONE;
}

void MFXVAAPIDevice::Close()
{
    if (m_va_dpy)
    {
        vaTerminate(m_va_dpy);
        m_va_dpy = NULL;
    }
    if (NULL != m_android_display)
    {
        free(m_android_display);
        m_android_display = NULL;
    }
}

MFXVAAPIDevice::~MFXVAAPIDevice()
{
    Close();
}


mfxStatus MFXVAAPIDevice::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    if ((MFX_HANDLE_LINUX_VA_DISPLAY == type) && (NULL != pHdl))
    {
        *pHdl = m_va_dpy;

        return MFX_ERR_NONE;
    }
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXVAAPIDevice::RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * /*pAlloc*/)
{
    return MFX_ERR_UNSUPPORTED;
}

#endif // #ifdef VAAPI_SURFACES_SUPPORT
#endif // #if defined(ANDROID)
