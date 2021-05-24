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

#include <thread>
#include <condition_variable>
#include <mutex>

namespace test
{
    TEST_P(FlexibleAllocator, LockInvalidMid)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        EXPECT_EQ(
            allocator->Lock(nullptr, &data, MFX_MAP_READ_WRITE),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_P(FlexibleAllocator, LockWrongFlags)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        EXPECT_EQ(
            allocator->Lock(res.mids[0], &data, 78),
            MFX_ERR_LOCK_MEMORY
        );
    }

    TEST_P(FlexibleAllocator, Unlock)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        ASSERT_EQ(
            allocator->Lock(res.mids[0], &data, MFX_MAP_READ_WRITE),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            allocator->Unlock(res.mids[0], &data),
            MFX_ERR_NONE
        );

        EXPECT_EQ(data.Y, nullptr);
        EXPECT_EQ(data.U, nullptr);
        EXPECT_EQ(data.V, nullptr);

        EXPECT_EQ(data.PitchHigh, 0);
        EXPECT_EQ(data.Pitch, 0);
    }

    TEST_P(FlexibleAllocator, UnlockInvalidMid)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        ASSERT_EQ(
            allocator->Lock(res.mids[0], &data, MFX_MAP_READ_WRITE),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            allocator->Unlock(nullptr, &data),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_P(FlexibleAllocator, UnlockNotLocked)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        EXPECT_EQ(
            allocator->Unlock(res.mids[0], &data),
            MFX_ERR_UNDEFINED_BEHAVIOR
        );
    }

    TEST_P(FlexibleAllocator, LockTwiceThenUnlockTwice)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        ASSERT_EQ(
            allocator->Lock(res.mids[0], &data, MFX_MAP_READ),
            MFX_ERR_NONE
        );

        ASSERT_EQ(
            allocator->Lock(res.mids[0], &data, MFX_MAP_READ),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            allocator->Unlock(res.mids[0], &data),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            allocator->Unlock(res.mids[0], &data),
            MFX_ERR_NONE
        );
    }

    struct FlexibleAllocatorLock : public FlexibleAllocatorBase,
        testing::WithParamInterface <std::tuple <mfxU16, mfxU32>>
    {
        void SetUp() override
        {
            FlexibleAllocatorBase::SetUp();
            req.Type = std::get<0>(GetParam());
#if defined(MFX_VA_WIN)
            if(req.Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
                req.Type |= MFX_MEMTYPE_SHARED_RESOURCE;
#endif
            SetAllocator(std::get<0>(GetParam()));
        }
    };

    std::vector<mfxU32> lockFlags{
        MFX_MAP_READ,
        MFX_MAP_WRITE,
        MFX_MAP_READ_WRITE,
        MFX_MAP_READ | MFX_MAP_NOWAIT
    };


    INSTANTIATE_TEST_SUITE_P(
        MemTypesFlags,
        FlexibleAllocatorLock,
        testing::Combine(
            testing::ValuesIn(memTypes),
            testing::ValuesIn(lockFlags)
        )
    );

    TEST_P(FlexibleAllocatorLock, Lock)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        EXPECT_EQ(
            allocator->Lock(res.mids[0], &data, std::get<1>(GetParam())),
            MFX_ERR_NONE
        );

        // check mfxFrameData for MFX_FOURCC_NV12
        EXPECT_NE(data.Y, nullptr);
        EXPECT_NE(data.U, nullptr);
        EXPECT_NE(data.V, nullptr);

        EXPECT_EQ(
            data.PitchHigh,
            req.Info.Width >> 16
        );
        EXPECT_EQ(
            data.Pitch,
            req.Info.Width & 0xffff
        );
    }

    struct FlexibleAllocatorLockRead : FlexibleAllocatorLock {};

    std::vector<mfxU32> lockReadFlags{
        MFX_MAP_READ,
        MFX_MAP_READ | MFX_MAP_NOWAIT
    };

    INSTANTIATE_TEST_SUITE_P(
        MemTypesFlags,
        FlexibleAllocatorLockRead,
        testing::Combine(
            testing::ValuesIn(memTypes),
            testing::ValuesIn(lockReadFlags)
        )
    );

    TEST_P(FlexibleAllocatorLockRead, LockReadAfterLockRead)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        ASSERT_EQ(
            allocator->Lock(res.mids[0], &data, MFX_MAP_READ),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            allocator->Lock(res.mids[0], &data, std::get<1>(GetParam())),
            MFX_ERR_NONE
        );
    }

    struct FlexibleAllocatorLockAfterWrite : FlexibleAllocatorLock {};

    std::vector<mfxU32> lockWriteFlags{
        MFX_MAP_WRITE,
        MFX_MAP_READ_WRITE
    };

    INSTANTIATE_TEST_SUITE_P(
        MemTypesFlags,
        FlexibleAllocatorLockAfterWrite,
        testing::Combine(
            testing::ValuesIn(memTypes),
            testing::ValuesIn(lockWriteFlags)
        )
    );

    TEST_P(FlexibleAllocatorLockAfterWrite, LockReadNoWaitAfterLockWrite)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        ASSERT_EQ(
            allocator->Lock(res.mids[0], &data, std::get<1>(GetParam())),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            allocator->Lock(res.mids[0], &data, MFX_MAP_READ | MFX_MAP_NOWAIT),
            MFX_ERR_RESOURCE_MAPPED
        );
    }

    TEST_P(FlexibleAllocatorLockAfterWrite, LockReadAfterLockWriteTwoThreads)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );

        std::mutex mtx;
        // Prevent second thread from execution
        std::unique_lock<std::mutex> lock(mtx);

        // Spawn second thread
        std::thread thr([&]() {
            mfxFrameData data{};

            // Second thread waits for mutex release
            std::lock_guard<std::mutex> guard(mtx);
            EXPECT_EQ(
                allocator->Lock(res.mids[0], &data, MFX_MAP_READ),
                MFX_ERR_NONE
            );
        });

        mfxFrameData data{};
        EXPECT_EQ(
            allocator->Lock(res.mids[0], &data, std::get<1>(GetParam())),
            MFX_ERR_NONE
        );

        // Release mutex and let second thread to acquire surface's lock
        lock.unlock();
        std::this_thread::yield(); // give priority of execution to second thread

        EXPECT_EQ(
            allocator->Unlock(res.mids[0], &data),
            MFX_ERR_NONE
        );
        thr.join();
    }

    struct FlexibleAllocatorLockTwice : public FlexibleAllocatorBase,
        testing::WithParamInterface <std::tuple <mfxU16, mfxU32, mfxU32>>
    {
        void SetUp() override
        {
            FlexibleAllocatorBase::SetUp();
            req.Type = std::get<0>(GetParam());
#if defined(MFX_VA_WIN)
            if(req.Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
                req.Type |= MFX_MEMTYPE_SHARED_RESOURCE;
#endif
            SetAllocator(std::get<0>(GetParam()));
        }
    };

    std::vector<mfxU32> lockTwiceFlags{
        MFX_MAP_READ,
        MFX_MAP_WRITE,
        MFX_MAP_READ_WRITE
    };

    std::vector<mfxU32> lockTwiceWriteFlags{
        MFX_MAP_WRITE,
        MFX_MAP_READ_WRITE,
        MFX_MAP_WRITE | MFX_MAP_NOWAIT,
        MFX_MAP_READ_WRITE | MFX_MAP_NOWAIT
    };

    INSTANTIATE_TEST_SUITE_P(
        MemTypesFlagsTwice,
        FlexibleAllocatorLockTwice,
        testing::Combine(
            testing::ValuesIn(memTypes),
            testing::ValuesIn(lockTwiceFlags),
            testing::ValuesIn(lockTwiceWriteFlags)
        )
    );

    TEST_P(FlexibleAllocatorLockTwice, LockWriteAfterLock)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        EXPECT_EQ(
            allocator->Lock(res.mids[0], &data, std::get<1>(GetParam())),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            allocator->Lock(res.mids[0], &data, std::get<2>(GetParam())),
            MFX_ERR_LOCK_MEMORY
        );

    }
}
#endif // MFX_ENABLE_UNIT_TEST_ALLOCATOR
#endif
