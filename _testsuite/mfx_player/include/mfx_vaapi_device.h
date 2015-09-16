/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2015 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_VAAPI_DEVICE_H
#define __MFX_VAAPI_DEVICE_H

#if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
#if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT)

//#include "vaapi_utils.h"



#include "mfx_ihw_device.h"
#include "vaapi_allocator.h"
#include <va/va.h>


#if defined(LIBVA_DRM_SUPPORT)
#include "vaapi_utils_drm.h"
class MFXVAAPIDeviceDRM : public IHWDevice
{
public:

    MFXVAAPIDeviceDRM() : m_pDRMLibVA(&pDRMLibVA) {}

    virtual ~MFXVAAPIDeviceDRM() { }

    virtual mfxStatus Init( mfxU32 nAdapter
                          , WindowHandle x11_dpy
                          , bool bIsWindowed
                          , mfxU32 renderTargetFmt
                          , int backBufferCount
                          , const vm_char *pDvxva2LibName
                          , bool)
    { return MFX_ERR_NONE; }
    virtual mfxStatus Reset(WindowHandle /*hDeviceWindow*/, RECT drawRect, bool /*bWindowed*/) { return MFX_ERR_NONE; }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl)
    {
        if ((MFX_HANDLE_VA_DISPLAY == type) && (NULL != pHdl))
        {
            if (m_pDRMLibVA) *pHdl = m_pDRMLibVA->GetVADisplay();
            return MFX_ERR_NONE;
        }
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *Palloc) { return MFX_ERR_NONE; }
    virtual void Close() { }

protected:
    DRMLibVA *m_pDRMLibVA;
    DRMLibVA pDRMLibVA;
};
#endif

#if defined(LIBVA_X11_SUPPORT)
#include "vaapi_utils_x11.h"
#include <va/va_x11.h>
#include <X11/Xlib.h>

class MFXVAAPIDeviceX11 : public IHWDevice
{
public:
    MFXVAAPIDeviceX11()
        : m_draw(0),
        m_pX11LibVA(&m_X11LibVA){}

    virtual ~MFXVAAPIDeviceX11() { }

    virtual mfxStatus Init( mfxU32 nAdapter
                          , WindowHandle x11_dpy
                          , bool bIsWindowed
                          , mfxU32 renderTargetFmt
                          , int backBufferCount
                          , const vm_char *pDvxva2LibName
                          , bool);
    virtual mfxStatus Reset(WindowHandle hDeviceWindow, RECT drawRect, bool bWindowed);
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *Palloc);
    virtual void Close() { }

private:
    Window m_draw;
    X11LibVA *m_pX11LibVA;
    X11LibVA m_X11LibVA;
};
#endif

IHWDevice* CreateVAAPIDevice(int type = MFX_LIBVA_DRM);


#endif // #if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT)
#endif // #if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)

#endif //__MFX_VAAPI_DEVICE_H
