/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.

File Name: mfx_allocators_generic.h

*******************************************************************************/

#pragma once

#include "mfx_iallocator_factory.h"
#include "mfx_d3d11_allocator.h"
#include "mfx_d3d_allocator.h"
#include "vaapi_allocator.h"
#include "mfx_sysmem_allocator.h"
#include "mfx_utility_allocators.h"

class DefaultAllocatorFactory
    : public IAllocatorFactory
{
public:
    virtual MFXFrameAllocator* CreateD3DAllocator()
    {
#ifdef D3D_SURFACES_SUPPORT
        return new D3DFrameAllocator();
#else
        return NULL;
#endif
    }
    virtual MFXFrameAllocator* CreateD3D11Allocator()
    {
#if MFX_D3D11_SUPPORT
        return new D3D11FrameAllocator();
#else
        return NULL;
#endif
    }
    virtual MFXFrameAllocator* CreateVAAPIAllocator()
    {
#ifdef LIBVA_SUPPORT
        return new vaapiFrameAllocator();
#else
        return NULL;
#endif
    }
    virtual MFXFrameAllocator* CreateSysMemAllocator()
    {
        return new SysMemFrameAllocator();
    }
};
