/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __VAAPI_UTILS_H__
#define __VAAPI_UTILS_H__

#ifdef LIBVA_SUPPORT

#include "vaapi_utils.h"
#include <va/va.h>

#if defined(LIBVA_DRM_SUPPORT)
#include <va/va_drm.h>
#endif
#if defined(LIBVA_X11_SUPPORT)
#include <va/va_x11.h>
#endif

#include "sample_defs.h"

#include "sample_utils.h"
#include "vm/thread_defs.h"


enum LibVABackend
{
    MFX_LIBVA_AUTO,
    MFX_LIBVA_DRM,
    MFX_LIBVA_X11,
    MFX_LIBVA_WAYLAND
};


namespace MfxLoader
{

    class SimpleLoader
    {
    public:
        SimpleLoader(const char * name);

        void * GetFunction(const char * name);

        ~SimpleLoader();

    private:
        SimpleLoader(SimpleLoader&);
        void operator=(SimpleLoader&);

        void * so_handle;
    };

#ifdef LIBVA_SUPPORT
    class VA_Proxy
    {
    private:
        SimpleLoader lib; // should appear first in member list

    public:
        typedef VAStatus (*vaInitialize_type)(VADisplay, int *, int *);
        typedef VAStatus (*vaTerminate_type)(VADisplay);
        typedef VAStatus (*vaCreateSurfaces_type)(VADisplay, unsigned int,
            unsigned int, unsigned int, VASurfaceID *, unsigned int,
            VASurfaceAttrib *, unsigned int);
        typedef VAStatus (*vaDestroySurfaces_type)(VADisplay, VASurfaceID *, int);
        typedef VAStatus (*vaCreateBuffer_type)(VADisplay, VAContextID,
            VABufferType, unsigned int, unsigned int, void *, VABufferID *);
        typedef VAStatus (*vaDestroyBuffer_type)(VADisplay, VABufferID);
        typedef VAStatus (*vaMapBuffer_type)(VADisplay, VABufferID, void **pbuf);
        typedef VAStatus (*vaUnmapBuffer_type)(VADisplay, VABufferID);
        typedef VAStatus (*vaDeriveImage_type)(VADisplay, VASurfaceID, VAImage *);
        typedef VAStatus (*vaDestroyImage_type)(VADisplay, VAImageID);


        VA_Proxy();
        ~VA_Proxy();

        const vaInitialize_type      vaInitialize;
        const vaTerminate_type       vaTerminate;
        const vaCreateSurfaces_type  vaCreateSurfaces;
        const vaDestroySurfaces_type vaDestroySurfaces;
        const vaCreateBuffer_type    vaCreateBuffer;
        const vaDestroyBuffer_type   vaDestroyBuffer;
        const vaMapBuffer_type       vaMapBuffer;
        const vaUnmapBuffer_type     vaUnmapBuffer;
        const vaDeriveImage_type     vaDeriveImage;
        const vaDestroyImage_type    vaDestroyImage;
    };
#endif

#if defined(LIBVA_DRM_SUPPORT)
    class VA_DRMProxy
    {
    private:
        SimpleLoader lib; // should appear first in member list

    public:
        typedef VADisplay (*vaGetDisplayDRM_type)(int);


        VA_DRMProxy();
        ~VA_DRMProxy();

        const vaGetDisplayDRM_type vaGetDisplayDRM;
    };
#endif

#if defined(LIBVA_X11_SUPPORT)
    class VA_X11Proxy
    {
    private:
        SimpleLoader lib; // should appear first in member list

    public:
        typedef VADisplay (*vaGetDisplay_type)(Display*);
        typedef VAStatus (*vaPutSurface_type)(
            VADisplay, VASurfaceID,
            Drawable,
            short, short,
            unsigned short, unsigned short,
            short, short,
            unsigned short, unsigned short,
            VARectangle *,
            unsigned int, unsigned int);

        VA_X11Proxy();
        ~VA_X11Proxy();

        const vaGetDisplay_type vaGetDisplay;
        const vaPutSurface_type vaPutSurface;
    };

    class XLib_Proxy
    {
    private:
        SimpleLoader lib; // should appear first in member list

    public:
        typedef Display* (*XOpenDisplay_type) (const char*);
        typedef int (*XCloseDisplay_type)(Display*);
        typedef Window (*XCreateSimpleWindow_type)(Display *,
            Window, int, int,
            unsigned int, unsigned int,
            unsigned int, unsigned long,
            unsigned long);
        typedef int (*XMapWindow_type)(Display*, Window);
        typedef int (*XSync_type)(Display*, Bool);
        typedef int (*XDestroyWindow_type)(Display*, Window);
        typedef int (*XResizeWindow_type)(Display *, Window, unsigned int, unsigned int);

        XLib_Proxy();
        ~XLib_Proxy();

        const XOpenDisplay_type        XOpenDisplay;
        const XCloseDisplay_type       XCloseDisplay;
        const XCreateSimpleWindow_type XCreateSimpleWindow;
        const XMapWindow_type          XMapWindow;
        const XSync_type               XSync;
        const XDestroyWindow_type      XDestroyWindow;
        const XResizeWindow_type       XResizeWindow;
    };
#endif
} // namespace MfxLoader


class CLibVA
{
public:
    virtual ~CLibVA(void) {};

    VADisplay GetVADisplay(void) { return m_va_dpy; }
    const MfxLoader::VA_Proxy m_libva;

protected:
    CLibVA(void) :
        m_va_dpy(NULL)
    {}
    VADisplay m_va_dpy;

private:
    DISALLOW_COPY_AND_ASSIGN(CLibVA);
};


CLibVA* CreateLibVA(int type = MFX_LIBVA_DRM);


mfxStatus va_to_mfx_status(VAStatus va_res);

#endif // #ifdef LIBVA_SUPPORT

#endif // #ifndef __VAAPI_UTILS_H__
