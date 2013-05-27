/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
#ifdef VAAPI_SURFACES_SUPPORT

#include "mfx_pipeline_defs.h"
#include "mfx_vaapi_device.h"
#include "vaapi_utils.h"

MFXVAAPIDevice::MFXVAAPIDevice()
{
    m_x11_display = NULL;
    va_dpy = NULL;
}

mfxStatus MFXVAAPIDevice::Init(mfxU32 nAdapter,
                               WindowHandle handle,
                               bool bIsWindowed,
                               mfxU32 VIDEO_RENDER_TARGET_FORMAT,
                               int BACK_BUFFER_COUNT,
                               const vm_char *pDXVA2LIBNAME,
                               bool )
{
    if (1 == nAdapter)
    {
        m_draw = (Window)handle;
        return MFX_ERR_NONE;
    }

    VAStatus va_res = VA_STATUS_SUCCESS;
    mfxStatus mfx_res = MFX_ERR_NONE;
    int major_version = 0, minor_version = 0;

    if (NULL == handle)
    {
        char* currentDisplay = getenv("DISPLAY");
        if (currentDisplay)
            m_x11_display = XOpenDisplay(currentDisplay);
        else
            m_x11_display = XOpenDisplay(VAAPI_X_DEFAULT_DISPLAY);
    }
    else m_x11_display = (Display*)handle;

    if (NULL == m_x11_display) mfx_res = MFX_ERR_NOT_INITIALIZED;
    if (MFX_ERR_NONE == mfx_res)
    {
        va_dpy = vaGetDisplay(m_x11_display);
        va_res = vaInitialize(va_dpy, &major_version, &minor_version);
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
    if (va_dpy)
    {
        vaTerminate(va_dpy);
        va_dpy = NULL;
    }
    if (m_x11_display)
    {
        XCloseDisplay(m_x11_display);
        m_x11_display = NULL;
    }
}

MFXVAAPIDevice::~MFXVAAPIDevice()
{
    Close();
}


mfxStatus MFXVAAPIDevice::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    if (MFX_HANDLE_VA_DISPLAY == type && pHdl != NULL)
    {
        *pHdl = va_dpy;

        return MFX_ERR_NONE;
    }
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXVAAPIDevice::RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * /*Palloc*/)
{
    VAStatus va_res = VA_STATUS_SUCCESS;
    mfxStatus sts = MFX_ERR_NONE;

    //if (!pSurface) return MFX_ERR_NONE; // on EOS

    vaapiMemId * memId = (vaapiMemId*)(pSurface->Data.MemId);
    if (!memId || !memId->m_surface) return sts;
    VASurfaceID surface = *memId->m_surface;
    
    XResizeWindow(m_x11_display, m_draw, pSurface->Info.CropW, pSurface->Info.CropH);
    
    va_res = vaPutSurface(va_dpy,
                      surface,
                      m_draw,
                      pSurface->Info.CropX,
                      pSurface->Info.CropY,
                      pSurface->Info.CropX + pSurface->Info.CropW,
                      pSurface->Info.CropY + pSurface->Info.CropH,
                      pSurface->Info.CropX,
                      pSurface->Info.CropY,
                      pSurface->Info.CropX + pSurface->Info.CropW,
                      pSurface->Info.CropY + pSurface->Info.CropH,
                      NULL,
                      0,
                      VA_FRAME_PICTURE);

    sts = va_to_mfx_status(va_res);
    XSync(m_x11_display, False);
  return MFX_ERR_NONE;
}

#endif // #ifdef VAAPI_SURFACES_SUPPORT
#endif // #if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
