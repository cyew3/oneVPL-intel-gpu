/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)

#include "mfx_vaapi_device.h"

#if defined(LIBVA_DRM_SUPPORT)
    static DRMLibVA g_LibVA;
#elif defined(LIBVA_X11_SUPPORT)
    static X11LibVA g_LibVA;
#endif

IHWDevice* CreateVAAPIDevice(void)
{
#if defined(LIBVA_DRM_SUPPORT)
    return new MFXVAAPIDeviceDRM(&g_LibVA);
#elif defined(LIBVA_X11_SUPPORT)
    return new MFXVAAPIDeviceX11(&g_LibVA);
#endif
}

#if defined(LIBVA_X11_SUPPORT)

#include "mfx_pipeline_defs.h"


mfxStatus MFXVAAPIDeviceX11::Init(mfxU32 nAdapter,
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

    return MFX_ERR_NONE;
}

mfxStatus MFXVAAPIDeviceX11::Reset(WindowHandle /*hDeviceWindow*/, RECT /* drawRect */, bool /*bWindowed*/)
{
    return MFX_ERR_NONE;
}

mfxStatus MFXVAAPIDeviceX11::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    if ((MFX_HANDLE_VA_DISPLAY == type) && (NULL != pHdl))
    {
        if (m_pX11LibVA) *pHdl = m_pX11LibVA->GetVADisplay();
        return MFX_ERR_NONE;
    }
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXVAAPIDeviceX11::RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * /*Palloc*/)
{
    VAStatus va_res = VA_STATUS_SUCCESS;
    mfxStatus sts = MFX_ERR_NONE;

    //if (!pSurface) return MFX_ERR_NONE; // on EOS

    vaapiMemId * memId = (vaapiMemId*)(pSurface->Data.MemId);
    if (!memId || !memId->m_surface) return sts;
    VASurfaceID surface = *memId->m_surface;

    XResizeWindow((Display*)(m_pX11LibVA ? m_pX11LibVA->GetXDisplay() : NULL), m_draw, pSurface->Info.CropW, pSurface->Info.CropH);

    va_res = vaPutSurface(m_pX11LibVA ? m_pX11LibVA->GetVADisplay() : NULL,
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
    XSync((Display*)(m_pX11LibVA ? m_pX11LibVA->GetXDisplay() : NULL), False);
  return MFX_ERR_NONE;
}

#endif // #if defined(LIBVA_X11_SUPPORT)
#endif // #if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
