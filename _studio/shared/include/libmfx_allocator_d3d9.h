/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2012 Intel Corporation. All Rights Reserved.

File Name: libmfx_allocator_d3d9.h

\* ****************************************************************************** */
#include "mfx_common.h"
#if defined  (MFX_VA_WIN)
#ifndef _LIBMFX_ALLOCATOR_D3D9_H_
#define _LIBMFX_ALLOCATOR_D3D9_H_

// disable the "nonstandard extension used : nameless struct/union" warning
// dxva2api.h warning
#pragma warning(disable: 4201)



#include <d3d9.h>
#include <dxva2api.h>



#include "libmfx_allocator.h"

// Internal Allocators 
namespace mfxDefaultAllocatorD3D9
{

    mfxStatus AllocFramesHW(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus LockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    mfxStatus GetHDLHW(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    mfxStatus UnlockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0);
    mfxStatus FreeFramesHW(mfxHDL pthis, mfxFrameAllocResponse *response);

    class mfxWideHWFrameAllocator : public  mfxBaseWideFrameAllocator
    {
    public:
        std::vector<IDirect3DSurface9*> m_SrfQueue;
        mfxWideHWFrameAllocator(mfxU16 type, IDirectXVideoDecoderService *pExtDirectXVideoService);
        virtual ~mfxWideHWFrameAllocator(void){};
        IDirectXVideoDecoderService *pDirectXVideoService;
    };
}

#endif
#endif

