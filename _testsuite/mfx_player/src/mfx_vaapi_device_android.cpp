/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(ANDROID)
#ifdef LIBVA_ANDROID_SUPPORT

#include "mfx_pipeline_defs.h"
#include "mfx_vaapi_device_android.h"
#include "vaapi_utils.h"
#include "mfx_renders.h"

#include <i915_private_android_types.h>

using namespace android;

static AndroidLibVA g_LibVA;

IHWDevice* CreateVAAPIDevice(int type)
{
    (void)type;
    return new MFXVAAPIDeviceAndroid(&g_LibVA);
}

MFXVAAPIDeviceAndroid::MFXVAAPIDeviceAndroid(AndroidLibVA *pAndroidLibVA) :
    m_pAndroidLibVA(pAndroidLibVA),
    m_bufferWidth(0),
    m_bufferHeight(0),
    m_bufferColorFormat(HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL),
    m_bIsInitWindow(false)
{
    if (!m_pAndroidLibVA)
    {
        throw std::bad_alloc();
    }
}

MFXVAAPIDeviceAndroid::~MFXVAAPIDeviceAndroid()
{
}

mfxStatus MFXVAAPIDeviceAndroid::Init(mfxU32 /*nAdapter*/,
                                      WindowHandle /*android_dpy*/,
                                      bool /*bIsWindowed*/,
                                      mfxU32 renderTargetFmt,
                                      int /*backBufferCount*/,
                                      const vm_char * /*pDvxva2LibName*/,
                                      bool)
{
    if (MFX_SCREEN_RENDER == renderTargetFmt && !m_bIsInitWindow)
    {
        MFX_CHECK_STS(InitWindow());
    }

    return MFX_ERR_NONE;
}

void MFXVAAPIDeviceAndroid::Close()
{
}

mfxStatus MFXVAAPIDeviceAndroid::Reset(WindowHandle /*hDeviceWindow*/, RECT /*drawRect*/, bool /*bWindowed*/)
{
    return MFX_ERR_NONE;
}

mfxStatus MFXVAAPIDeviceAndroid::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    if ((MFX_HANDLE_VA_DISPLAY == type) && (NULL != pHdl))
    {
        if (m_pAndroidLibVA) *pHdl = m_pAndroidLibVA->GetVADisplay();
        return MFX_ERR_NONE;
    }
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXVAAPIDeviceAndroid::RenderFrame(mfxFrameSurface1 *pSrf, mfxFrameAllocator * /*pAlloc*/)
{
    MFX_CHECK_STS(DisplayOneFrameSysMem(pSrf));

    return MFX_ERR_NONE;
}


mfxStatus MFXVAAPIDeviceAndroid::InitWindow()
{
    m_surfaceComposerClient = new SurfaceComposerClient();

    if (android::OK != m_surfaceComposerClient->initCheck())
    {
        return MFX_ERR_UNKNOWN;
    }

    sp<IBinder> display(SurfaceComposerClient::getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain));

    DisplayInfo info;
    SurfaceComposerClient::getDisplayInfo(display, &info);

    m_bufferWidth  = info.w;
    m_bufferHeight = info.h;

    m_surfaceControl = m_surfaceComposerClient->createSurface(String8("mfx_player"), m_bufferWidth, m_bufferHeight, m_bufferColorFormat, 0);
    if (!m_surfaceControl->isValid())
    {
        return MFX_ERR_UNKNOWN;
    }

    //SurfaceComposerClient::openGlobalTransaction();
    //m_surfaceControl->setLayer(INT_MAX);
    //m_surfaceControl->show();
    //SurfaceComposerClient::closeGlobalTransaction();

    m_nativeWindow = m_surfaceControl->getSurface();

    if (0 != native_window_set_scaling_mode(m_nativeWindow.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW))
    {
        return MFX_ERR_UNKNOWN;
    }

    if (0 != native_window_api_connect(m_nativeWindow.get(), NATIVE_WINDOW_API_CPU))
    {
        return MFX_ERR_UNKNOWN;
    }

    m_bIsInitWindow = true;
    return MFX_ERR_NONE;
}

mfxStatus MFXVAAPIDeviceAndroid::DisplayOneFrameSysMem(mfxFrameSurface1 *pSrf)
{
    if (!m_bIsInitWindow)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (!pSrf)
    {
        return MFX_ERR_NULL_PTR;
    }

    MFX_CHECK_STS(ReconfigurateWindow(pSrf));

    ANativeWindowBuffer* buf;
    int fence = -1;
    if (0 != m_nativeWindow->dequeueBuffer(m_nativeWindow.get(), &buf, &fence))
    {
        return MFX_ERR_UNKNOWN;
    }

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds(0, 0, pSrf->Info.Width, pSrf->Info.Height);

    void *pDst = NULL;
    bool bIsLocked = false;
    if (0 != mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &pDst) || !pDst)
    {
        return MFX_ERR_LOCK_MEMORY;
    }
    else
    {
        bIsLocked = true;
    }

    switch (pSrf->Info.FourCC)
    {
        case MFX_FOURCC_NV12:
            MFX_CHECK_STS(CopySurfaceNV12SysMem(pSrf, buf->stride, buf->height, pDst));
            break;
        default:
            return MFX_ERR_UNSUPPORTED;
            break;
    }

    if (bIsLocked)
    {
        mapper.unlock(buf->handle);
        bIsLocked = false;
    }

    if (0 != m_nativeWindow->queueBuffer(m_nativeWindow.get(), buf, fence))
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVAAPIDeviceAndroid::ReconfigurateWindow(mfxFrameSurface1 *pSrf)
{
    if (!m_bIsInitWindow)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (!pSrf)
    {
        return MFX_ERR_NULL_PTR;
    }

    int surfWidth  = pSrf->Info.Width;
    int surfHeight = pSrf->Info.Height;

    if (surfWidth  != m_bufferWidth ||
        surfHeight != m_bufferHeight)
    {
        if (0 == native_window_set_buffers_dimensions(m_nativeWindow.get(), surfWidth, surfHeight))
        {
            m_bufferWidth  = surfWidth;
            m_bufferHeight = surfHeight;
        }
        else
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    int colorFormat = 0;
    switch (pSrf->Info.FourCC)
    {
        case MFX_FOURCC_NV12:
            colorFormat = HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL;
            break;
        default:
            return MFX_ERR_UNSUPPORTED;
            break;
    }

    if (colorFormat != m_bufferColorFormat)
    {
        if (0 == native_window_set_buffers_format(m_nativeWindow.get(), colorFormat))
        {
            m_bufferColorFormat = colorFormat;
        }
        else
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVAAPIDeviceAndroid::CopySurfaceNV12SysMem(mfxFrameSurface1 *pSrf, int bufferStride, int bufferHeight, void *pDst)
{
    if (!pSrf || !pDst)
    {
        return MFX_ERR_NULL_PTR;
    }

    if (!pSrf->Data.Y || !pSrf->Data.UV)
    {
        return MFX_ERR_NULL_PTR;
    }

    int surfWidth  = pSrf->Info.Width;
    int surfHeight = pSrf->Info.Height;
    int surfPitch  = pSrf->Data.Pitch;

    int heightOffset = bufferHeight;
    if (0 != heightOffset % 64)
    {
        heightOffset += (64 - (bufferHeight % 64));
    }

    uint8_t *pDstY  = (uint8_t *)pDst;
    uint8_t *pDstUV = pDstY + bufferStride * heightOffset;

    uint8_t *pSrcY  = pSrf->Data.Y;
    uint8_t *pSrcUV = pSrf->Data.UV;

    for (int i = 0; i < surfHeight; i++)
    {
        for (int j = 0; j < surfWidth; j++)
        {
            pDstY[j] = pSrcY[j];
        }
        pDstY += bufferStride;
        pSrcY += surfPitch;
    }

    for (int i = 0; i < surfHeight / 2; i++)
    {
        for (int j = 0; j < surfWidth; j++)
        {
            pDstUV[j] = pSrcUV[j];
        }
        pDstUV += bufferStride;
        pSrcUV += surfPitch;
    }

    return MFX_ERR_NONE;
}

#endif // #ifdef LIBVA_ANDROID_SUPPORT
#endif // #if defined(ANDROID)
