/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_VAAPI_DEVICE_ANDROID_H
#define __MFX_VAAPI_DEVICE_ANDROID_H

#if defined(ANDROID)
#ifdef LIBVA_ANDROID_SUPPORT

#include <va/va.h>
/* Undefining ANDROID here we exclude part of va_android.h file in which
 * Android-specific headers are touched. This permit us to build code under
 * Android NDK.
 */
#undef ANDROID
#include <va/va_android.h>
#define ANDROID

#include "vaapi_utils.h"
#include "vaapi_utils_android.h"
#include "vaapi_allocator.h"
#include "mfx_ihw_device.h"

#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/Surface.h>

#include <ui/DisplayInfo.h>
#include <ui/GraphicBufferMapper.h>

class MFXVAAPIDeviceAndroid : public IHWDevice
{
public:
    MFXVAAPIDeviceAndroid(AndroidLibVA *pAndroidLibVA);
    virtual ~MFXVAAPIDeviceAndroid();

    virtual mfxStatus Init(mfxU32 nAdapter,
                           WindowHandle android_dpy,
                           bool bIsWindowed,
                           mfxU32 renderTargetFmt,
                           int backBufferCount,
                           const vm_char *pDvxva2LibName,
                           bool);
    virtual void Close();

    virtual mfxStatus Reset(WindowHandle hDeviceWindow, RECT drawRect, bool bWindowed);
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);

    virtual mfxStatus RenderFrame(mfxFrameSurface1 *pSrf, mfxFrameAllocator *pAlloc);

private:
    mfxStatus InitWindow();
    mfxStatus DisplayOneFrameSysMem(mfxFrameSurface1 *pSrf);
    mfxStatus ReconfigurateWindow(mfxFrameSurface1 *pSrf);

    mfxStatus CopySurfaceNV12SysMem(mfxFrameSurface1 *pSrf, int bufferStride, int bufferHeight, void *pDst);

private:
    AndroidLibVA *m_pAndroidLibVA;

    android::sp<android::SurfaceComposerClient> m_surfaceComposerClient;
    android::sp<android::SurfaceControl>        m_surfaceControl;
    android::sp<ANativeWindow>                  m_nativeWindow;

    int m_bufferWidth;
    int m_bufferHeight;
    int m_bufferColorFormat;

    bool m_bIsInitWindow;
};

IHWDevice* CreateVAAPIDevice(const std::string& devicePath, int type);

#endif // #ifdef LIBVA_ANDROID_SUPPORT
#endif // #if defined(ANDROID)

#endif //__MFX_VAAPI_DEVICE_ANDROID_H
