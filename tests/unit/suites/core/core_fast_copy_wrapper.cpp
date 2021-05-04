// Copyright (c) 2020-2021 Intel Corporation
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

#if defined(MFX_ENABLE_UNIT_TEST_CORE)

#if (defined(_WIN32) || defined(_WIN64))
    #include "core_base_windows.h"
#elif defined(__linux__)
    #include "core_base_linux.h"
#endif

#include "libmfx_allocator.h"
#include <vector>

namespace test
{

    TEST_F(coreMain, DoFastCopyWrapperNullptrDst)
    {
        EXPECT_EQ(
            core->DoFastCopyWrapper(nullptr, MFX_MEMTYPE_SYSTEM_MEMORY, src, MFX_MEMTYPE_SYSTEM_MEMORY),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreMain, DoFastCopyWrapperNullptrSrc)
    {
        EXPECT_EQ(
            core->DoFastCopyWrapper(dst, MFX_MEMTYPE_SYSTEM_MEMORY, nullptr, MFX_MEMTYPE_SYSTEM_MEMORY),
            MFX_ERR_NULL_PTR
        );
    }

    struct coreCopy
        : public coreBase
        , testing::WithParamInterface<std::tuple<mfxU16, mfxU16>>
    {

        static testing::MockFunction<mfxStatus(mfxFrameSurface1*, mfxU32)>* mockPtr;
        testing::MockFunction<mfxStatus(mfxFrameSurface1*, mfxU32)>         mock;
        mfxU16                                                              memtype;
        void SetUp() override
        {
            mockPtr = &mock;
            coreBase::SetUp();

            auto memtype = std::get<0>(GetParam());

            SetAllocator(memtype);

            ASSERT_EQ(
                allocator->CreateSurface(memtype, info, dst),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                allocator->CreateSurface(memtype, info, src),
                MFX_ERR_NONE
            );

            memtype |= std::get<1>(GetParam());

            ASSERT_EQ(
                src->FrameInterface->Map(src, MFX_MAP_READ),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                dst->FrameInterface->Map(dst, MFX_MAP_READ),
                MFX_ERR_NONE
            );

            ON_CALL(mock, Call(testing::_, testing::_))
                .WillByDefault(testing::Invoke(&mfxFrameSurfaceInterfaceImpl::Map_impl));
            dst->FrameInterface->Map = src->FrameInterface->Map = &Map;
        }

        void TearDown() override
        {
            mockPtr = nullptr;
        }

        static mfxStatus Map(mfxFrameSurface1* surface, mfxU32 flags)
        {
            return mockPtr->Call(surface, flags);
        }
    };

    testing::MockFunction<mfxStatus(mfxFrameSurface1*, mfxU32)>* coreCopy::mockPtr = nullptr;

    std::vector<mfxU16> memory =
    {
        MFX_MEMTYPE_SYSTEM_MEMORY
      , MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET
    };

    std::vector<mfxU16> frame =
    {
        MFX_MEMTYPE_INTERNAL_FRAME
      , MFX_MEMTYPE_EXTERNAL_FRAME
    };

    INSTANTIATE_TEST_SUITE_P(
        memtype,
        coreCopy,
        testing::Combine(
            testing::ValuesIn(memory),
            testing::ValuesIn(frame)
        )
    );

    TEST_P(coreCopy, DoFastCopyWrapperSystemMemory)
    {
        EXPECT_EQ(
            core->DoFastCopyWrapper(dst, MFX_MEMTYPE_SYSTEM_MEMORY, src, MFX_MEMTYPE_SYSTEM_MEMORY),
            MFX_ERR_NONE
        );
    }

    TEST_P(coreCopy, DoFastCopyWrapper)
    {
        EXPECT_CALL(mock, Call(testing::_, testing::_)).Times(0);

        EXPECT_EQ(
            core->DoFastCopyWrapper(dst, memtype, src, memtype),
            MFX_ERR_NONE
        );
    }

    TEST_P(coreCopy, DoFastCopyWrapperZeroMemIdZeroPtrs)
    {
        EXPECT_CALL(mock, Call(testing::_, testing::_)).Times(0);
        mfxFrameSurface1 emptydst{}, emptysrc{};
        EXPECT_EQ(
            core->DoFastCopyWrapper(&emptydst, memtype, &emptysrc, memtype),
            MFX_ERR_LOCK_MEMORY
        );
    }

    TEST_P(coreCopy, DoFastCopyWrapperZeroMemIdNonZeroPtrs)
    {
        EXPECT_CALL(mock, Call(testing::_, testing::_)).Times(0);
        dst->Data.MemId = src->Data.MemId = nullptr;
        EXPECT_EQ(
            core->DoFastCopyWrapper(dst, memtype, src, memtype),
            MFX_ERR_NONE
        );
    }

    TEST_P(coreCopy, DoFastCopyWrapperNonZeroMemIdZeroPtrs)
    {
        ASSERT_EQ(
            dst->FrameInterface->Unmap(dst),
            MFX_ERR_NONE
        );
        ASSERT_EQ(
            src->FrameInterface->Unmap(src),
            MFX_ERR_NONE
        );

        EXPECT_CALL(mock, Call(testing::_, testing::_)).Times(2);

        EXPECT_EQ(
            core->DoFastCopyWrapper(dst, memtype, src, memtype),
            MFX_ERR_NONE
        );
    }

}

#endif // MFX_ENABLE_UNIT_TEST_CORE