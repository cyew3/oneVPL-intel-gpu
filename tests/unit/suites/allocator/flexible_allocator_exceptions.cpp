// Copyright (c) 2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#if defined(MFX_ONEVPL)
#if defined (MFX_ENABLE_UNIT_TEST_ALLOCATOR)

#include "flexible_allocator_base.h"

namespace test
{
    struct FlexibleAllocatorFake : public FlexibleAllocatorBase
    {
        struct mfxFrameSurface1_fake : public mfxFrameSurface1_sw
        {
            mfxFrameSurface1_fake(const mfxFrameInfo & info, mfxU16 type, mfxMemId mid, mfxHDL staging_adapter, mfxHDL device, mfxU32 context, FrameAllocatorBase& allocator) :
                mfxFrameSurface1_sw(info, type, mid, staging_adapter, device, context, allocator)
            {
                throw - 1;
            }
        };

        void SetUp() override
        {
            FlexibleAllocatorBase::SetUp();
            req.Type = MFX_MEMTYPE_SYSTEM_MEMORY;
#if defined(MFX_VA_WIN)
            allocator.reset(
                new FlexibleFrameAllocator<mfxFrameSurface1_fake, staging_adapter_stub>{ component->device.p }
            );
#elif defined(MFX_VA_LINUX)
            allocator.reset(
                new FlexibleFrameAllocator<mfxFrameSurface1_fake, staging_adapter_stub>{ display.get() }
            );
#endif
        }
    };

    TEST_F(FlexibleAllocatorFake, AllocNonMfxStatusException)
    {
        EXPECT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_MEMORY_ALLOC
        );
    }

    TEST_F(FlexibleAllocatorFake, CreateSurfaceNonMfxStatusException)
    {
        mfxFrameSurface1* surface{};
        EXPECT_EQ(
            allocator->CreateSurface(MFX_MEMTYPE_SYSTEM_MEMORY, req.Info, surface),
            MFX_ERR_MEMORY_ALLOC
        );
    }
}
#endif // MFX_ENABLE_UNIT_TEST_ALLOCATOR
#endif
