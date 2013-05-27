/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_iallocator_factory.h"
#include "mfx_utility_allocators.h"
#include "mfx_component_params.h"
#include "mock_mfx_frame_allocator.h"
#include "mock_allocator_factory.h"
#include "mfx_factory_chain.h"

//rw factory would be a base for all proxied factories
typedef FactoryChain<LCCheckFrameAllocator, RWAllocatorFactory::root> LLCFactoryRW;
typedef FactoryChain<LockRWEnabledFrameAllocator, RWAllocatorFactory::root> RWEnabledFactory;


SUITE(Allocator_factory)
{
    TEST(create_rw_alloc)
    {
        MockAllocatorFactory *maf = new MockAllocatorFactory;
        MockMFXFrameAllocator *ma = new MockMFXFrameAllocator;
        TEST_METHOD_TYPE(MockMFXFrameAllocator::LockFrame) result;

        result.ret_val = (mfxStatus)11;
        ma->_LockFrame.WillReturn(result);

        TEST_METHOD_TYPE(MockAllocatorFactory::CreateD3D11Allocator) result2;
        result2.ret_val = ma;
        maf->_CreateD3D11Allocator.WillReturn(result2);

        RWAllocatorFactory rwfac (maf);
        
        std::auto_ptr<MFXFrameAllocatorRW> rwAlloc ( rwfac.CreateD3D11Allocator());

        CHECK_EQUAL(11, rwAlloc->LockFrame(0, 0));
    }
    
    TEST(create_lcc_alloc_unlock_firstly)
    {
        MockAllocatorFactory *maf = new MockAllocatorFactory;
        MockMFXFrameAllocator *ma = new MockMFXFrameAllocator;
        TEST_METHOD_TYPE(MockMFXFrameAllocator::UnlockFrame) result;

        result.ret_val = (mfxStatus)11;
        ma->_UnlockFrame.WillReturn(result);

        TEST_METHOD_TYPE(MockAllocatorFactory::CreateD3D11Allocator) result2;
        result2.ret_val = ma;
        maf->_CreateD3D11Allocator.WillReturn(result2);

        LLCFactoryRW lccFactory (new RWAllocatorFactory(maf));
        std::auto_ptr<MFXFrameAllocatorRW> rccAlloc ( lccFactory.CreateD3D11Allocator());
        CHECK_EQUAL(MFX_ERR_UNKNOWN, rccAlloc->UnlockFrame(0, 0));
    }

    TEST(create_lcc_alloc_lock_and_2unlocks)
    {
        MockAllocatorFactory *maf = new MockAllocatorFactory;
        MockMFXFrameAllocator *ma = new MockMFXFrameAllocator;
        TEST_METHOD_TYPE(MockMFXFrameAllocator::LockFrame) result;

        result.ret_val = (mfxStatus)11;
        ma->_LockFrame.WillReturn(result);

        TEST_METHOD_TYPE(MockAllocatorFactory::CreateD3D11Allocator) result2;
        result2.ret_val = ma;
        maf->_CreateD3D11Allocator.WillReturn(result2);

        //RWAllocatorFactory rwfac (maf);
        LLCFactoryRW lccFactory (new RWAllocatorFactory(maf));
        std::auto_ptr<MFXFrameAllocatorRW> rccAlloc ( lccFactory.CreateD3D11Allocator());

        CHECK_EQUAL(11, rccAlloc->LockFrame(0, 0));
        CHECK_EQUAL(MFX_ERR_NONE, rccAlloc->UnlockFrame(0, 0));
        CHECK_EQUAL(MFX_ERR_UNKNOWN, rccAlloc->UnlockFrame(0, 0));
    }

    TEST(create_true_lock_rw_alloc)
    {
        MockAllocatorFactory *maf = new MockAllocatorFactory;
        MockMFXFrameAllocator *ma = new MockMFXFrameAllocator;
        TEST_METHOD_TYPE(MockMFXFrameAllocator::LockFrame) result;

        result.ret_val = (mfxStatus)11;
        ma->_LockFrame.WillReturn(result);

        TEST_METHOD_TYPE(MockAllocatorFactory::CreateD3D11Allocator) result2;
        result2.ret_val = ma;
        maf->_CreateD3D11Allocator.WillReturn(result2);

        //RWAllocatorFactory rwfac (maf);
        RWEnabledFactory rweFactory (new RWAllocatorFactory(maf));
        std::auto_ptr<MFXFrameAllocatorRW> rccAlloc ( rweFactory.CreateD3D11Allocator());

        CHECK_EQUAL(11, rccAlloc->LockFrameRW(0, 0, MFXReadWriteMid::read));
        ma->_LockFrame.WasCalled(&result);

        
        CHECK_EQUAL(1u, (uintptr_t)result.value0 >> (std::numeric_limits<uintptr_t>::digits - 1));
    }

    //that they can be stored and called using base
    TEST(create_alloc_factory_base)
    {
        MockAllocatorFactory *maf = new MockAllocatorFactory;
        MockMFXFrameAllocator *ma = new MockMFXFrameAllocator;
        TEST_METHOD_TYPE(MockMFXFrameAllocator::LockFrame) result;

        result.ret_val = (mfxStatus)11;
        ma->_LockFrame.WillReturn(result);

        TEST_METHOD_TYPE(MockAllocatorFactory::CreateD3D11Allocator) result2;
        result2.ret_val = ma;
        maf->_CreateD3D11Allocator.WillReturn(result2);

        std::auto_ptr<RWAllocatorFactory::root> p (new RWEnabledFactory(new LLCFactoryRW( new RWAllocatorFactory(maf))));
        std::auto_ptr<MFXFrameAllocatorRW> rccAlloc ( p->CreateD3D11Allocator());

        CHECK_EQUAL(11, rccAlloc->LockFrameRW(0, 0, MFXReadWriteMid::read));
        ma->_LockFrame.WasCalled(&result);

        //rwenabled interface
        CHECK_EQUAL(1u, (uintptr_t)result.value0 >> (std::numeric_limits<uintptr_t>::digits - 1));
        
        CHECK_EQUAL(MFX_ERR_NONE, rccAlloc->UnlockFrameRW(0, 0, MFXReadWriteMid::read));
        //lcc interface
        CHECK_EQUAL(MFX_ERR_UNKNOWN, rccAlloc->UnlockFrameRW(0, 0, MFXReadWriteMid::read));
        
    }

}