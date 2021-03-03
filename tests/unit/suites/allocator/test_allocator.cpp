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

#if defined (MFX_ENABLE_UNIT_TEST_ALLOCATOR)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#define ENABLE_MOCKS_MFX
#if defined (MFX_D3D11_ENABLED)
#include "mocks/include/mfx/dx11/allocator.h"
#endif

#if defined (MFX_VA_LINUX)
#include "mocks/include/mfx/va/allocator.h"
#endif

namespace test {
    template <typename T>
    struct allocator
        : public ::testing::Test {};

#if defined(MFX_D3D9_ENABLED) && defined(MOCKS_D3D9_SUPPORTED)
    template <>
    struct allocator<mocks::mfx::dx9::allocator>
        : testing::Test
    {
        std::unique_ptr<mocks::mfx::dx9::allocator> alloc;

        void SetUp() override
        {
            alloc = mocks::mfx::dx9::make_allocator();
        }
    };
#endif

#if defined(MFX_D3D11_ENABLED)
    template <>
    struct allocator<mocks::mfx::dx11::allocator>
        : testing::Test
    {
        std::unique_ptr<mocks::mfx::dx11::allocator> alloc;

        void SetUp() override
        {
            alloc = mocks::mfx::dx11::make_allocator();
        }
    };
#endif

#if defined(MFX_VA_LINUX)
    template <>
    struct allocator<mocks::mfx::va::allocator>
        : testing::Test
    {
        std::shared_ptr<mocks::va::display>        display;
        std::shared_ptr<mocks::mfx::va::allocator> alloc;

        void SetUp() override
        {
            display = mocks::va::make_display();
            alloc = mocks::mfx::va::make_allocator(display.get());
        }
    };
#endif

    using allocator_types = testing::Types <
#if defined(MFX_D3D9_ENABLED) && defined(MOCKS_D3D9_SUPPORTED)
        mocks::mfx::dx9::allocator
#endif
#if defined(MFX_D3D11_ENABLED)
        mocks::mfx::dx11::allocator
#endif
#if defined (MFX_VA_LINUX)
        mocks::mfx::va::allocator
#endif
    >;

    TYPED_TEST_CASE(allocator, allocator_types);

    TYPED_TEST(allocator, Init)
    {
        UMC::FrameAllocatorParams p{};
        EXPECT_EQ(
            this->alloc->Init(&p),
            UMC::UMC_OK
        );
    }

    TYPED_TEST(allocator, Close)
    {
        EXPECT_EQ(
            this->alloc->Close(),
            UMC::UMC_OK
        );
    }

    TYPED_TEST(allocator, Reset)
    {
        EXPECT_EQ(
            this->alloc->Reset(),
            UMC::UMC_OK
        );
    }

    TYPED_TEST(allocator, Alloc)
    {
        EXPECT_EQ(
            this->alloc->Alloc(nullptr, nullptr, 0),
            UMC::UMC_ERR_NULL_PTR
        );

        UMC::FrameMemID id;
        EXPECT_EQ(
            this->alloc->Alloc(&id, nullptr, 0),
            UMC::UMC_ERR_NULL_PTR
        );

        UMC::VideoDataInfo vi{};
        mock_allocator(*this->alloc,
            std::make_tuple(1, vi)
        );

        EXPECT_EQ(
            this->alloc->Alloc(&id, &vi, 0),
            UMC::UMC_OK
        );
        EXPECT_NE(id, UMC::FRAME_MID_INVALID);

        EXPECT_EQ(
            this->alloc->Alloc(&id, &vi, 0),
            UMC::UMC_ERR_ALLOC
        );
    }

    TYPED_TEST(allocator, GetFrameHandle)
    {
        EXPECT_EQ(
            this->alloc->GetFrameHandle(UMC::FRAME_MID_INVALID, nullptr),
            UMC::UMC_ERR_ALLOC
        );

        EXPECT_EQ(
            this->alloc->GetFrameHandle(0, nullptr),
            UMC::UMC_ERR_ALLOC
        );

        mfxHDLPair handle;
        EXPECT_EQ(
            this->alloc->GetFrameHandle(0, &handle),
            UMC::UMC_ERR_ALLOC
        );

        UMC::VideoDataInfo vi{};
        mock_allocator(*this->alloc,
            std::make_tuple(1, vi)
        );

        EXPECT_EQ(
            this->alloc->GetFrameHandle(0, &handle),
            UMC::UMC_ERR_ALLOC
        );

        UMC::FrameMemID id;
        ASSERT_EQ(
            this->alloc->Alloc(&id, &vi, 0),
            UMC::UMC_OK
        );

        EXPECT_EQ(
            this->alloc->GetFrameHandle(id, &handle),
            UMC::UMC_OK
        );
        EXPECT_NE(handle.first, nullptr);

        EXPECT_EQ(
            this->alloc->GetFrameHandle(UMC::FRAME_MID_INVALID, &handle),
            UMC::UMC_ERR_ALLOC
        );
    }

    TYPED_TEST(allocator, Lock)
    {
        EXPECT_EQ(
            this->alloc->Lock(0),
            nullptr
        );

        UMC::VideoDataInfo vi{};
        vi.Init(42, 42, UMC::P010);
        mock_allocator(*this->alloc,
            std::make_tuple(1, vi)
        );

        EXPECT_EQ(
            this->alloc->Lock(0),
            nullptr
        );

        UMC::FrameMemID id;
        ASSERT_EQ(
            this->alloc->Alloc(&id, &vi, 0),
            UMC::UMC_OK
        );

        auto fd = this->alloc->Lock(id);
        EXPECT_NE(fd, nullptr);
        EXPECT_EQ(
            fd->GetInfo()->GetWidth(), vi.GetWidth()
        );
        EXPECT_EQ(
            fd->GetInfo()->GetHeight(), vi.GetHeight()
        );
        EXPECT_EQ(
            fd->GetInfo()->GetColorFormat(), vi.GetColorFormat()
        );

        EXPECT_EQ(
            this->alloc->Lock(1),
            nullptr
        );
        EXPECT_EQ(
            this->alloc->Lock(UMC::FRAME_MID_INVALID),
            nullptr
        );
    }

    TYPED_TEST(allocator, Unlock)
    {
        EXPECT_EQ(
            this->alloc->Unlock(0),
            UMC::UMC_ERR_FAILED
        );

        UMC::VideoDataInfo vi{};
        mock_allocator(*this->alloc,
            std::make_tuple(1, vi)
        );

        EXPECT_EQ(
            this->alloc->Unlock(0),
            UMC::UMC_ERR_FAILED
        );

        UMC::FrameMemID id;
        ASSERT_EQ(
            this->alloc->Alloc(&id, &vi, 0),
            UMC::UMC_OK
        );

        EXPECT_EQ(
            this->alloc->Unlock(id),
            UMC::UMC_OK
        );
        EXPECT_EQ(
            this->alloc->Unlock(1),
            UMC::UMC_ERR_FAILED
        );
        EXPECT_EQ(
            this->alloc->Unlock(UMC::FRAME_MID_INVALID),
            UMC::UMC_ERR_FAILED
        );
    }

    TYPED_TEST(allocator, IncreaseReference)
    {
        EXPECT_EQ(
            this->alloc->IncreaseReference(0),
            UMC::UMC_ERR_FAILED
        );

        UMC::VideoDataInfo vi{};
        mock_allocator(*this->alloc,
            std::make_tuple(1, vi)
        );

        EXPECT_EQ(
            this->alloc->IncreaseReference(0),
            UMC::UMC_ERR_FAILED
        );

        UMC::FrameMemID id;
        ASSERT_EQ(
            this->alloc->Alloc(&id, &vi, 0),
            UMC::UMC_OK
        );

        EXPECT_EQ(
            this->alloc->IncreaseReference(id),
            UMC::UMC_OK
        );
        EXPECT_EQ(
            this->alloc->IncreaseReference(1),
            UMC::UMC_ERR_FAILED
        );
        EXPECT_EQ(
            this->alloc->IncreaseReference(UMC::FRAME_MID_INVALID),
            UMC::UMC_ERR_FAILED
        );
    }

    TYPED_TEST(allocator, DecreaseReference)
    {
        EXPECT_EQ(
            this->alloc->DecreaseReference(0),
            UMC::UMC_ERR_FAILED
        );

        UMC::VideoDataInfo vi{};
        mock_allocator(*this->alloc,
            std::make_tuple(1, vi)
        );

        EXPECT_EQ(
            this->alloc->DecreaseReference(0),
            UMC::UMC_ERR_FAILED
        );

        UMC::FrameMemID id;
        ASSERT_EQ(
            this->alloc->Alloc(&id, &vi, 0),
            UMC::UMC_OK
        );

        EXPECT_EQ(
            this->alloc->DecreaseReference(id),
            UMC::UMC_OK
        );
        EXPECT_EQ(
            this->alloc->DecreaseReference(1),
            UMC::UMC_ERR_FAILED
        );
        EXPECT_EQ(
            this->alloc->DecreaseReference(UMC::FRAME_MID_INVALID),
            UMC::UMC_ERR_FAILED
        );
    }
}

#endif  // MFX_ENABLE_UNIT_TEST_ALLOCATOR