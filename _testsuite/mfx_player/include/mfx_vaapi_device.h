/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_VAAPI_DEVICE_H
#define __MFX_VAAPI_DEVICE_H

#if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)
#ifdef VAAPI_SURFACES_SUPPORT

#include "mfx_ihw_device.h"
#include "vaapi_allocator.h"
#include <va/va.h>
#include <va/va_x11.h>
#include <X11/Xlib.h>

#define VAAPI_X_DEFAULT_DISPLAY ":0.0"

class MFXVAAPIDevice : public IHWDevice
{
public:
    MFXVAAPIDevice();
    virtual ~MFXVAAPIDevice();

    virtual mfxStatus Init( mfxU32 nAdapter
                          , WindowHandle x11_dpy
                          , bool bIsWindowed
                          , mfxU32 renderTargetFmt
                          , int backBufferCount
                          , const vm_char *pDvxva2LibName
                          , bool);
    virtual mfxStatus Reset(bool bWindowed);
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *Palloc);
    virtual void Close() ;

private:
    Display *m_x11_display;
    VADisplay va_dpy;
    Window m_draw;
};

#endif // #ifdef VAAPI_SURFACES_SUPPORT
#endif // #if (defined(LINUX32) || defined(LINUX64)) && !defined(ANDROID)

#endif //__MFX_VAAPI_DEVICE_H
