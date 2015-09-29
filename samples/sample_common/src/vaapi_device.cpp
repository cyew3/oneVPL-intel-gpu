/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2015 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT) || defined(LIBVA_ANDROID_SUPPORT)

#include "vaapi_device.h"

#if defined(LIBVA_WAYLAND_SUPPORT)
#include "class_wayland.h"
#endif

#if defined(LIBVA_X11_SUPPORT)

#include <va/va_x11.h>
#include <X11/Xlib.h>
#include "vaapi_allocator.h"

#define VAAPI_GET_X_DISPLAY(_display) (Display*)(_display)
#define VAAPI_GET_X_WINDOW(_window) (Window*)(_window)


CVAAPIDeviceX11::~CVAAPIDeviceX11(void)
{
    Close();
}

mfxStatus CVAAPIDeviceX11::Init(mfxHDL hWindow, mfxU16 nViews, mfxU32 nAdapterNum)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    Window* window = NULL;

    if (nViews)
    {
        if (MFX_ERR_NONE == mfx_res)
        {
            m_window = window = (Window*)malloc(sizeof(Window));
            if (!m_window) mfx_res = MFX_ERR_MEMORY_ALLOC;
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            Display* display = VAAPI_GET_X_DISPLAY(m_X11LibVA.GetXDisplay());
            MfxLoader::XLib_Proxy & x11lib = m_X11LibVA.GetX11();
            mfxU32 screen_number = DefaultScreen(display);

            *window = x11lib.XCreateSimpleWindow(
                display,
                RootWindow(display, screen_number),
                0,
                0,
                100,
                100,
                0,
                0,
                WhitePixel(display, screen_number));

            if (!(*window)) mfx_res = MFX_ERR_UNKNOWN;
            else
            {
                x11lib.XMapWindow(display, *window);
                x11lib.XSync(display, False);
            }
        }
    }
    return mfx_res;
}

void CVAAPIDeviceX11::Close(void)
{
    if (m_window)
    {
        Display* display = VAAPI_GET_X_DISPLAY(m_X11LibVA.GetXDisplay());
        Window* window = VAAPI_GET_X_WINDOW(m_window);

        MfxLoader::XLib_Proxy & x11lib = m_X11LibVA.GetX11();
        x11lib.XDestroyWindow(display, *window);

        free(m_window);
        m_window = NULL;
    }
}

mfxStatus CVAAPIDeviceX11::Reset(void)
{
    return MFX_ERR_NONE;
}

mfxStatus CVAAPIDeviceX11::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    if ((MFX_HANDLE_VA_DISPLAY == type) && (NULL != pHdl))
    {
        *pHdl = m_X11LibVA.GetVADisplay();

        return MFX_ERR_NONE;
    }

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CVAAPIDeviceX11::SetHandle(mfxHandleType type, mfxHDL hdl)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CVAAPIDeviceX11::RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * /*pmfxAlloc*/)
{
    VAStatus va_res = VA_STATUS_SUCCESS;
    mfxStatus mfx_res = MFX_ERR_NONE;
    vaapiMemId * memId = NULL;
    VASurfaceID surface;
    Display* display = VAAPI_GET_X_DISPLAY(m_X11LibVA.GetXDisplay());
    Window* window = VAAPI_GET_X_WINDOW(m_window);

    if(!window || !(*window)) mfx_res = MFX_ERR_NOT_INITIALIZED;
    // should MFX_ERR_NONE be returned below considuring situation as EOS?
    if ((MFX_ERR_NONE == mfx_res) && NULL == pSurface) mfx_res = MFX_ERR_NULL_PTR;
    if (MFX_ERR_NONE == mfx_res)
    {
        memId = (vaapiMemId*)(pSurface->Data.MemId);
        if (!memId || !memId->m_surface) mfx_res = MFX_ERR_NULL_PTR;
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        surface = *memId->m_surface;

        MfxLoader::XLib_Proxy & x11lib = m_X11LibVA.GetX11();
        x11lib.XResizeWindow(display, *window, pSurface->Info.CropW, pSurface->Info.CropH);

        MfxLoader::VA_X11Proxy & vax11lib = m_X11LibVA.GetVAX11();
        vax11lib.vaPutSurface(
            m_X11LibVA.GetVADisplay(),
            surface,
            *window,
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

        mfx_res = va_to_mfx_status(va_res);
        x11lib.XSync(display, False);
    }
    return mfx_res;
}
#endif

#if defined(LIBVA_WAYLAND_SUPPORT)
#include "wayland-drm-client-protocol.h"

CVAAPIDeviceWayland::~CVAAPIDeviceWayland(void)
{
    Close();
}

mfxStatus CVAAPIDeviceWayland::Init(mfxHDL hWindow, mfxU16 nViews, mfxU32 nAdapterNum)
{
    mfxStatus mfx_res = MFX_ERR_NONE;

    if(nViews)
    {
        m_Wayland = (Wayland*)m_WaylandClient.WaylandCreate();
        if(!m_Wayland->InitDisplay()) {
            return MFX_ERR_DEVICE_FAILED;
        }

        if(NULL == m_Wayland->GetDisplay())
        {
            mfx_res = MFX_ERR_UNKNOWN;
            return mfx_res;
        }
       if(-1 == m_Wayland->DisplayRoundtrip())
        {
            mfx_res = MFX_ERR_UNKNOWN;
            return mfx_res;
        }
        if(!m_Wayland->CreateSurface())
        {
            mfx_res = MFX_ERR_UNKNOWN;
            return mfx_res;
        }
    }
    return mfx_res;
}

mfxStatus CVAAPIDeviceWayland::RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * /*pmfxAlloc*/)
{
    uint32_t drm_format;
    int offsets[3], pitches[3];
    mfxStatus mfx_res = MFX_ERR_NONE;
    vaapiMemId * memId = NULL;
    struct wl_buffer *m_wl_buffer = NULL;
    if(NULL==pSurface) {
        mfx_res = MFX_ERR_UNKNOWN;
        return mfx_res;
    }
    m_Wayland->Sync();
    memId = (vaapiMemId*)(pSurface->Data.MemId);

    if (pSurface->Info.FourCC == MFX_FOURCC_NV12)
    {
        drm_format = WL_DRM_FORMAT_NV12;
    } else if(pSurface->Info.FourCC = MFX_FOURCC_RGB4)
    {
        drm_format = WL_DRM_FORMAT_ARGB8888;
    }

    offsets[0] = memId->m_image.offsets[0];
    offsets[1] = memId->m_image.offsets[1];
    offsets[2] = memId->m_image.offsets[2];
    pitches[0] = memId->m_image.pitches[0];
    pitches[1] = memId->m_image.pitches[1];
    pitches[2] = memId->m_image.pitches[2];
    m_wl_buffer = m_Wayland->CreatePrimeBuffer(memId->m_buffer_info.handle
      , pSurface->Info.CropW
      , pSurface->Info.CropH
      , drm_format
      , offsets
      , pitches);
    if(NULL == m_wl_buffer)
    {
            msdk_printf("\nCan't wrap flink to wl_buffer\n");
            mfx_res = MFX_ERR_UNKNOWN;
            return mfx_res;
    }

    m_Wayland->RenderBuffer(m_wl_buffer, 0, 0, pSurface->Info.CropW, pSurface->Info.CropH);
    return mfx_res;
}

void CVAAPIDeviceWayland::SetRenderWinPosSize(mfxU32 x, mfxU32 y, mfxU32 w, mfxU32 h)
{
    /* NOT IMPLEMENTED */
}

void CVAAPIDeviceWayland::Close(void)
{
    m_Wayland->FreeSurface();
}

CHWDevice* CreateVAAPIDevice(void)
{
    return new CVAAPIDeviceWayland();
}

#endif // LIBVA_WAYLAND_SUPPORT

#if defined(LIBVA_DRM_SUPPORT)

CVAAPIDeviceDRM::CVAAPIDeviceDRM(int type)
    : m_DRMLibVA(type)
    , m_rndr(NULL)
{
}

CVAAPIDeviceDRM::~CVAAPIDeviceDRM(void)
{
  MSDK_SAFE_DELETE(m_rndr);
}

mfxStatus CVAAPIDeviceDRM::Init(mfxHDL hWindow, mfxU16 nViews, mfxU32 nAdapterNum)
{
    if (0 == nViews) {
        return MFX_ERR_NONE;
    }
    if (1 == nViews) {
        if (m_DRMLibVA.getBackendType() == MFX_LIBVA_DRM_RENDERNODE) {
          return MFX_ERR_NONE;
        }
        mfxI32 * monitorType = (mfxI32*)hWindow;
        if (!monitorType) return MFX_ERR_INVALID_VIDEO_PARAM;
        try {
            m_rndr = new drmRenderer(m_DRMLibVA.getFD(), *monitorType);
        } catch(...) {
            msdk_printf(MSDK_STRING("vaapi_device: failed to initialize drmrender\n"));
            return MFX_ERR_UNKNOWN;
        }
        return MFX_ERR_NONE;
    }
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CVAAPIDeviceDRM::RenderFrame(mfxFrameSurface1 * pSurface, mfxFrameAllocator * pmfxAlloc)
{
    return (m_rndr)? m_rndr->render(pSurface): MFX_ERR_NONE;
}

#endif

#if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT) || defined (LIBVA_WAYLAND_SUPPORT)

CHWDevice* CreateVAAPIDevice(int type)
{
    CHWDevice * device = NULL;

    switch (type)
    {
    case MFX_LIBVA_DRM_RENDERNODE:
    case MFX_LIBVA_DRM_MODESET:
#if defined(LIBVA_DRM_SUPPORT)
        try
        {
            device = new CVAAPIDeviceDRM(type);
        }
        catch (std::exception&)
        {
            device = NULL;
        }
#endif
        break;

    case MFX_LIBVA_X11:
#if defined(LIBVA_X11_SUPPORT)
        try
        {
            device = new CVAAPIDeviceX11;
        }
        catch (std::exception&)
        {
            device = NULL;
        }
#endif
    case MFX_LIBVA_WAYLAND:
#if defined(LIBVA_WAYLAND_SUPPORT)
        device = new CVAAPIDeviceWayland;
#endif
        break;
    case MFX_LIBVA_AUTO:
#if defined(LIBVA_X11_SUPPORT)
        try
        {
            device = new CVAAPIDeviceX11;
        }
        catch (std::exception&)
        {
            device = NULL;
        }
#endif
#if defined(LIBVA_DRM_SUPPORT)
        if (!device)
        {
            try
            {
                device = new CVAAPIDeviceDRM(type);
            }
            catch (std::exception&)
            {
                device = NULL;
            }
        }
#endif
        break;
    } // switch(type)

    return device;
}

#elif defined(LIBVA_ANDROID_SUPPORT)

CHWDevice* CreateVAAPIDevice(int)
{
    return new CVAAPIDeviceAndroid();
}

#endif

#endif //#if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT) || defined(LIBVA_ANDROID_SUPPORT)
