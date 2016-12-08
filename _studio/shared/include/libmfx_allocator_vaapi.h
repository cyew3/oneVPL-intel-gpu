//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_VA_LINUX)

#ifndef _LIBMFX_ALLOCATOR_VAAPI_H_
#define _LIBMFX_ALLOCATOR_VAAPI_H_

#include <va/va.h>

#include "mfxvideo++int.h"
#include "libmfx_allocator.h"

// VAAPI Allocator internal Mem ID
struct vaapiMemIdInt
{
    VASurfaceID* m_surface;
    VAImage m_image;
    unsigned int m_fourcc;
};

// Internal Allocators 
namespace mfxDefaultAllocatorVAAPI
{
    mfxStatus AllocFramesHW(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus LockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    mfxStatus GetHDLHW(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    mfxStatus UnlockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0);
    mfxStatus FreeFramesHW(mfxHDL pthis, mfxFrameAllocResponse *response);

    mfxStatus SetFrameData(const VAImage &va_image, mfxU32 mfx_fourcc, mfxU8* pBuffer, mfxFrameData*  ptr);

    class mfxWideHWFrameAllocator : public  mfxBaseWideFrameAllocator
    {
    public:
        mfxWideHWFrameAllocator(mfxU16 type, mfxHDL handle);
        virtual ~mfxWideHWFrameAllocator(void){};

        VADisplay* pVADisplay;

//        VASurfaceID* m_SrfQueue;
//        vaapiMemIdInt* m_SrfQueue;

        mfxU32 m_DecId;
    };

} //  namespace mfxDefaultAllocatorVAAPI

#endif // LIBMFX_ALLOCATOR_VAAPI_H_
#endif // (MFX_VA_LINUX)
/* EOF */
