/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: libmfx_allocator_d3d11.h

\* ****************************************************************************** */
#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)
#ifndef _LIBMFX_ALLOCATOR_D3D11_H_
#define _LIBMFX_ALLOCATOR_D3D11_H_

// disable the "nonstandard extension used : nameless struct/union" warning
// dxva2api.h warning
#pragma warning(disable: 4201)


#include <d3d9.h>
#include <d3d11.h>
#include "libmfx_allocator.h"

// Internal Allocators 
namespace mfxDefaultAllocatorD3D11
{


    DXGI_FORMAT MFXtoDXGI(mfxU32 format);

    mfxStatus AllocFramesHW(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus LockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    mfxStatus GetHDLHW(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    mfxStatus UnlockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0);
    mfxStatus FreeFramesHW(mfxHDL pthis, mfxFrameAllocResponse *response);

    class mfxWideHWFrameAllocator : public  mfxBaseWideFrameAllocator
    {
    public:
        std::vector<ID3D11Texture2D*> m_SrfPool; // array of pointers
        ID3D11Texture2D* m_StagingSrfPool;
        mfxWideHWFrameAllocator(mfxU16 type, ID3D11Device *pD11Device, ID3D11DeviceContext *pD11DeviceContext);
        virtual ~mfxWideHWFrameAllocator(void){};

        //we are sure that Device & Context already queryied
        ID3D11Device            *m_pD11Device;
        ID3D11DeviceContext     *m_pD11DeviceContext;

        mfxU32 m_NumSurface;

        bool isBufferKeeper;

    };

}

#endif
#endif
#endif
