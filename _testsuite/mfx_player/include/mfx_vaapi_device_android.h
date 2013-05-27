/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_VAAPI_DEVICE_ANDROID_H
#define __MFX_VAAPI_DEVICE_ANDROID_H

#if defined(ANDROID)
#ifdef VAAPI_SURFACES_SUPPORT

#include <va/va.h>
/* Undefining ANDROID here we exclude part of va_android.h file in which
 * Android-specific headers are touched. This permit us to build code under
 * Android NDK.
 */
#undef ANDROID
#include <va/va_android.h>
#define ANDROID

#include "vaapi_utils.h"
#include "vaapi_allocator.h"
#include "mfx_ihw_device.h"

typedef unsigned int vaapiAndroidDisplay;

class MFXVAAPIDevice : public IHWDevice
{
public:
    MFXVAAPIDevice();
    virtual ~MFXVAAPIDevice();

    virtual mfxStatus Init( mfxU32 nAdapter
                          , WindowHandle android_dpy
                          , bool bIsWindowed
                          , mfxU32 renderTargetFmt
                          , int backBufferCount
                          , const vm_char *pDvxva2LibName
                          , bool);
    virtual mfxStatus Reset(bool bWindowed);
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *pAlloc);
    virtual void Close() ;

private:
    vaapiAndroidDisplay *m_android_display;
    VADisplay m_va_dpy;
};

#endif // #ifdef VAAPI_SURFACES_SUPPORT
#endif // #if defined(ANDROID)

#endif //__MFX_VAAPI_DEVICE_ANDROID_H
