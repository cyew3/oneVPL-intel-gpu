/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_adapters.h"


//create proxied create chain  for each factory create call
//TCreator - type that every created object will be proxied by
//TFrom  - type that new factory originated from
template <class TCreator, class TFrom>
class FactoryChain
{
};

//allocators factory chaining
template <class TCreator>
class FactoryChain <TCreator, IAllocatorFactoryTmpl<typename TCreator::result_type> >
    : public InterfaceProxyBase< IAllocatorFactoryTmpl<typename TCreator::result_type> >
{
    typedef IAllocatorFactoryTmpl<typename TCreator::result_type> base_type;
    typedef InterfaceProxyBase<base_type> base;
public:
    typedef typename TCreator::result_type result_type;

    FactoryChain(base_type * pTarget)
        : base(pTarget)
    {
    }

    virtual result_type* CreateD3DAllocator()
    {
        return new TCreator(base::m_pTarget->CreateD3DAllocator());
    }
    virtual result_type* CreateD3D11Allocator()
    {
        return new TCreator(base::m_pTarget->CreateD3D11Allocator());
    }
    virtual result_type* CreateVAAPIAllocator()
    {
        return new TCreator(base::m_pTarget->CreateVAAPIAllocator());
    }
    virtual result_type* CreateSysMemAllocator()
    {
        return new TCreator(base::m_pTarget->CreateSysMemAllocator());
    }
};
