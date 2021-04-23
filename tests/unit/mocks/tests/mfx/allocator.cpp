// Copyright (c) 2021 Intel Corporation
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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mocks/include/mfx/allocator.h"

namespace mocks { namespace test { namespace mfx
{
    struct allocator
        : testing::Test
    {
        mfxFrameAllocRequest                   request{};
        mfxFrameData                           data{};
        std::vector<mfxMemId>                  mids;
        std::unique_ptr<mocks::mfx::allocator> alloc;

        void SetUp() override
        {
            request.AllocId           = 42;
            request.NumFrameSuggested = 2;
            data.MemType =
                MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
            data.Pitch = 42;
            data.Y = reinterpret_cast<mfxU8*>(this);

            mids.resize(request.NumFrameSuggested);
            std::iota(std::begin(mids), std::end(mids), this);

            alloc = mocks::mfx::make_allocator(
                std::make_tuple(
                    request,
                    mfxFrameAllocResponse{
                        request.AllocId, {}, mids.data(), mfxU16(mids.size())
                    },
                    data
                )
            );
        }
    };

    TEST_F(allocator, AllocNullRequest)
    {
        mfxFrameAllocResponse r;
        EXPECT_EQ(
            alloc->Alloc(nullptr, &r),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(allocator, AllocNullResponse)
    {
        EXPECT_EQ(
            alloc->Alloc(&request, nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(allocator, Alloc)
    {
        mfxFrameAllocResponse r;
        EXPECT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            request.AllocId, r.AllocId
        );
        EXPECT_THAT(
            mids, testing::ElementsAreArray(r.mids, r.NumFrameActual)
        );
    }

    TEST_F(allocator, LockNullFrameData)
    {
        mfxFrameAllocResponse r;
        ASSERT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            alloc->Lock(r.mids[1], nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(allocator, LockWrongId)
    {
        mfxFrameAllocResponse r;
        ASSERT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        mfxFrameData fd;
        EXPECT_EQ(
            alloc->Lock(mfxMemId(42), &fd),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_F(allocator, Lock)
    {
        mfxFrameAllocResponse r;
        ASSERT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        mfxFrameData fd;
        EXPECT_EQ(
            alloc->Lock(r.mids[1], &fd),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            fd.Pitch, data.Pitch
        );
        EXPECT_EQ(
            fd.Y, data.Y
        );
    }

    TEST_F(allocator, GetHDLWrongId)
    {
        mfxFrameAllocResponse r;
        ASSERT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        mfxHDL hdl;
        EXPECT_EQ(
            alloc->GetHDL(mfxMemId(42), &hdl),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_F(allocator, GetHDLNullHdl)
    {
        mfxFrameAllocResponse r;
        ASSERT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            alloc->GetHDL(r.mids[0], nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(allocator, GetHDL)
    {
        mfxFrameAllocResponse r;
        ASSERT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        mfxHDL hdl;
        EXPECT_EQ(
            alloc->GetHDL(r.mids[1], &hdl),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            hdl, this + 1
        );
    }

    TEST_F(allocator, FreeNullResponce)
    {
        mfxFrameAllocResponse r;
        ASSERT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            alloc->Free(nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(allocator, FreeWrongMids)
    {
        mfxFrameAllocResponse r;
        ASSERT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        mfxFrameAllocResponse r2 = r;
        std::vector<mfxMemId> mids;
        mids.assign(r2.NumFrameActual, mfxMemId(42));
        r2.mids = mids.data();

        EXPECT_EQ(
            alloc->Free(&r2),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_F(allocator, Free)
    {
        mfxFrameAllocResponse r;
        ASSERT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            alloc->Free(&r),
            MFX_ERR_NONE
        );
    }

    TEST_F(allocator, FreeReorderedMids)
    {
        mfxFrameAllocResponse r;
        ASSERT_EQ(
            alloc->Alloc(&request, &r),
            MFX_ERR_NONE
        );

        mfxFrameAllocResponse r2 = r;
        std::vector<mfxMemId> mids(r2.mids, r2.mids + r2.NumFrameActual);
        r2.mids = mids.data();
        std::rotate(
            r2.mids,
            r2.mids + r2.NumFrameActual / 2,
            r2.mids + r2.NumFrameActual
        );

        EXPECT_EQ(
            alloc->Free(&r2),
            MFX_ERR_NONE
        );
    }

} } }
