/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2015 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __VAAPI_ALLOCATOR_H__
#define __VAAPI_ALLOCATOR_H__

#if defined(LIBVA_SUPPORT)

#include <stdlib.h>
#include <va/va.h>
#include <va/va_drmcommon.h>

#include "base_allocator.h"
#include "vaapi_utils.h"

// VAAPI Allocator internal Mem ID
struct vaapiMemId
{
    VASurfaceID* m_surface;
    VAImage      m_image;
    // variables for VAAPI Allocator internal color conversion
    unsigned int m_fourcc;
    mfxU8*       m_sys_buffer;
    mfxU8*       m_va_buffer;
    // buffer info to support surface export
    VABufferInfo m_buffer_info;
};

namespace MfxLoader
{
    class VA_Proxy;
}

struct vaapiAllocatorParams : mfxAllocatorParams
{
    enum {
      DONOT_EXPORT = 0,
      FLINK,
      PRIME
    };

    vaapiAllocatorParams()
      : m_dpy(NULL)
      , m_export_mode(DONOT_EXPORT)
    {}

    VADisplay m_dpy;
    mfxU32 m_export_mode;
};

class vaapiFrameAllocator: public BaseFrameAllocator
{
public:
    vaapiFrameAllocator();
    virtual ~vaapiFrameAllocator();

    virtual mfxStatus Init(mfxAllocatorParams *pParams);
    virtual mfxStatus Close();

protected:
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    VADisplay m_dpy;
    MfxLoader::VA_Proxy * m_libva;
    mfxU32 m_export_mode;
};

#endif //#if defined(LIBVA_SUPPORT)

#endif // __VAAPI_ALLOCATOR_H__
