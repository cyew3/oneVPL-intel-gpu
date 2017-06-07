/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2017 Intel Corporation. All Rights Reserved.

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

class MFXVAAPIDeviceAndroid : public IHWDevice
{
public:
    MFXVAAPIDeviceAndroid(AndroidLibVA *pAndroidLibVA):
        m_pAndroidLibVA(pAndroidLibVA)
        {
            if (!m_pAndroidLibVA)
            {
                throw std::bad_alloc();
            }
        }
    virtual ~MFXVAAPIDeviceAndroid() { }

    virtual mfxStatus Init( mfxU32 nAdapter
                          , WindowHandle android_dpy
                          , bool bIsWindowed
                          , mfxU32 renderTargetFmt
                          , int backBufferCount
                          , const vm_char *pDvxva2LibName
                          , bool)
    { return MFX_ERR_NONE; }

    virtual mfxStatus Reset(WindowHandle /*hDeviceWindow*/, RECT /*drawRect*/, bool /*bWindowed*/) { return MFX_ERR_NONE; }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl)
    {
        if ((MFX_HANDLE_VA_DISPLAY == type) && (NULL != pHdl))
        {
            if (m_pAndroidLibVA) *pHdl = m_pAndroidLibVA->GetVADisplay();
            return MFX_ERR_NONE;
        }
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *pAlloc)  { return MFX_ERR_NONE; }
    virtual void Close() { }

private:
    AndroidLibVA *m_pAndroidLibVA;
};

IHWDevice* CreateVAAPIDevice(int type);

#endif // #ifdef LIBVA_ANDROID_SUPPORT
#endif // #if defined(ANDROID)

#endif //__MFX_VAAPI_DEVICE_ANDROID_H
