/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifdef LIBVA_SUPPORT

#include "vaapi_utils.h"
#include <dlfcn.h>
#include <stdexcept>
#include <iostream>

//#if defined(LIBVA_DRM_SUPPORT)
#include "vaapi_utils_drm.h"
//#elif defined(LIBVA_X11_SUPPORT)
#include "vaapi_utils_x11.h"
//#endif

namespace MfxLoader
{

    SimpleLoader::SimpleLoader(const char * name)
    {
        dlerror();
        so_handle = dlopen(name, RTLD_GLOBAL | RTLD_NOW);
        if (NULL == so_handle)
        {
            std::cerr << dlerror() << std::endl;
            throw std::runtime_error("Can't load library");
        }
    }

    void * SimpleLoader::GetFunction(const char * name)
    {
        void * fn_ptr = dlsym(so_handle, name);
        if (!fn_ptr)
            throw std::runtime_error("Can't find function");
        return fn_ptr;
    }

    SimpleLoader::~SimpleLoader()
    {
        if (so_handle)
            dlclose(so_handle);
    }

#define SIMPLE_LOADER_STRINGIFY1( x) #x
#define SIMPLE_LOADER_STRINGIFY(x) SIMPLE_LOADER_STRINGIFY1(x)
#define SIMPLE_LOADER_DECORATOR1(fun,suffix) fun ## _ ## suffix
#define SIMPLE_LOADER_DECORATOR(fun,suffix) SIMPLE_LOADER_DECORATOR1(fun,suffix)


    // Following macro applied on vaInitialize will give:  vaInitialize((vaInitialize_type)lib.GetFunction("vaInitialize"))
#define SIMPLE_LOADER_FUNCTION(name) name( (SIMPLE_LOADER_DECORATOR(name, type)) lib.GetFunction(SIMPLE_LOADER_STRINGIFY(name)) )


#if defined(LIBVA_SUPPORT)
    VA_Proxy::VA_Proxy()
#if defined(ANDROID)
        : lib("libva.so")
#else
        : lib("libva.so.2")
#endif
        , SIMPLE_LOADER_FUNCTION(vaInitialize)
        , SIMPLE_LOADER_FUNCTION(vaTerminate)
        , SIMPLE_LOADER_FUNCTION(vaCreateSurfaces)
        , SIMPLE_LOADER_FUNCTION(vaDestroySurfaces)
        , SIMPLE_LOADER_FUNCTION(vaCreateBuffer)
        , SIMPLE_LOADER_FUNCTION(vaDestroyBuffer)
        , SIMPLE_LOADER_FUNCTION(vaMapBuffer)
        , SIMPLE_LOADER_FUNCTION(vaUnmapBuffer)
        , SIMPLE_LOADER_FUNCTION(vaDeriveImage)
        , SIMPLE_LOADER_FUNCTION(vaDestroyImage)
        , SIMPLE_LOADER_FUNCTION(vaSyncSurface)
    {
    }

    VA_Proxy::~VA_Proxy()
    {}

#endif

#if defined(LIBVA_DRM_SUPPORT)
    VA_DRMProxy::VA_DRMProxy()
        : lib("libva-drm.so.2")
        , SIMPLE_LOADER_FUNCTION(vaGetDisplayDRM)
    {
    }

    VA_DRMProxy::~VA_DRMProxy()
    {}
#endif

#if defined(LIBVA_X11_SUPPORT)
    VA_X11Proxy::VA_X11Proxy()
        : lib("libva-x11.so.2")
        , SIMPLE_LOADER_FUNCTION(vaGetDisplay)
        , SIMPLE_LOADER_FUNCTION(vaPutSurface)
    {
    }

    VA_X11Proxy::~VA_X11Proxy()
    {}

    XLib_Proxy::XLib_Proxy()
        : lib("libX11.so")
        , SIMPLE_LOADER_FUNCTION(XOpenDisplay)
        , SIMPLE_LOADER_FUNCTION(XCloseDisplay)
        , SIMPLE_LOADER_FUNCTION(XCreateSimpleWindow)
        , SIMPLE_LOADER_FUNCTION(XMapWindow)
        , SIMPLE_LOADER_FUNCTION(XSync)
        , SIMPLE_LOADER_FUNCTION(XDestroyWindow)
        , SIMPLE_LOADER_FUNCTION(XResizeWindow)

    {}

    XLib_Proxy::~XLib_Proxy()
    {}


#endif

#undef SIMPLE_LOADER_FUNCTION

} // MfxLoader


mfxStatus va_to_mfx_status(VAStatus va_res)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    switch (va_res)
    {
    case VA_STATUS_SUCCESS:
        mfxRes = MFX_ERR_NONE;
        break;
    case VA_STATUS_ERROR_ALLOCATION_FAILED:
        mfxRes = MFX_ERR_MEMORY_ALLOC;
        break;
    case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
    case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
    case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
    case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
    case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
    case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
    case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
        mfxRes = MFX_ERR_UNSUPPORTED;
        break;
    case VA_STATUS_ERROR_INVALID_DISPLAY:
    case VA_STATUS_ERROR_INVALID_CONFIG:
    case VA_STATUS_ERROR_INVALID_CONTEXT:
    case VA_STATUS_ERROR_INVALID_SURFACE:
    case VA_STATUS_ERROR_INVALID_BUFFER:
    case VA_STATUS_ERROR_INVALID_IMAGE:
    case VA_STATUS_ERROR_INVALID_SUBPICTURE:
        mfxRes = MFX_ERR_NOT_INITIALIZED;
        break;
    case VA_STATUS_ERROR_INVALID_PARAMETER:
        mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
    default:
        mfxRes = MFX_ERR_UNKNOWN;
        break;
    }
    return mfxRes;
}

#if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT)
CLibVA* CreateLibVA(int type)
{
    CLibVA * libva = 0;
    switch (type)
    {
    case MFX_LIBVA_DRM:
#if defined(LIBVA_DRM_SUPPORT)
        libva = new DRMLibVA;
#endif
        break;

    case MFX_LIBVA_X11:
#if defined(LIBVA_X11_SUPPORT)
        libva = new X11LibVA;
#endif
        break;

    case MFX_LIBVA_AUTO:
#if defined(LIBVA_X11_SUPPORT)
        try
        {
            libva = new X11LibVA;
        }
        catch (std::exception&)
        {
            libva = 0;
        }
#endif
#if defined(LIBVA_DRM_SUPPORT)
        if (!libva)
        {
            try
            {
                libva = new DRMLibVA;
            }
            catch (std::exception&)
            {
                libva = 0;
            }
        }
#endif
        break;
    } // switch(type)

    return libva;
}
#endif // #if defined(LIBVA_DRM_SUPPORT) || defined(LIBVA_X11_SUPPORT)

#endif // #ifdef LIBVA_SUPPORT
