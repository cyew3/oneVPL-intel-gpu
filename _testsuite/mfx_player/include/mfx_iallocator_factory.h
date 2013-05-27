/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_iproxy.h"
#include "mfx_frame_allocator_rw.h"

//class factory for pipeline needs
template <class ret_type>
class IAllocatorFactoryTmpl
{
public:
    //typedef IAllocatorFactory facade;
    virtual ~IAllocatorFactoryTmpl(){}
    virtual ret_type* CreateD3DAllocator() = 0;
    virtual ret_type* CreateD3D11Allocator() = 0;
    virtual ret_type* CreateVAAPIAllocator() = 0;
    virtual ret_type* CreateSysMemAllocator() = 0;
};

//typedef to generic allocator factory
typedef IAllocatorFactoryTmpl<MFXFrameAllocator> IAllocatorFactory;
typedef IAllocatorFactoryTmpl<MFXFrameAllocatorRW> IAllocatorFactoryRW;

