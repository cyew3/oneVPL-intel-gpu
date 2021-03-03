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

#if defined(MFX_ENABLE_UNIT_TEST_CORE)

#include "core_base.h"

namespace test
{

    TEST_F(coreMain, DoFastCopyWrapperNullptrDst)
    {
        EXPECT_EQ(
            s->m_pCORE->DoFastCopyWrapper(nullptr, MFX_MEMTYPE_SYSTEM_MEMORY, src, MFX_MEMTYPE_SYSTEM_MEMORY),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreMain, DoFastCopyWrapperNullptrSrc)
    {
        EXPECT_EQ(
            s->m_pCORE->DoFastCopyWrapper(dst, MFX_MEMTYPE_SYSTEM_MEMORY, nullptr, MFX_MEMTYPE_SYSTEM_MEMORY),
            MFX_ERR_NULL_PTR
        );
    }

    struct coreCopy
        : public coreBase
        , testing::WithParamInterface<std::tuple<mfxU16, mfxU16>>
    {
        mfxU16                                                                   memtype;
        mfxU16                                                                   iopattern;

        static testing::MockFunction<mfxStatus(mfxFrameSurface1*, mfxU32)>*      mockPtr;
        testing::MockFunction<mfxStatus(mfxFrameSurface1*, mfxU32)>              mock;

        void SetUp() override
        {
            mockPtr = &mock;
            coreBase::SetUp();
            iopattern = vp.IOPattern = std::get<0>(GetParam());
            auto constexpr type = mocks::mfx::HW_SCL;
            if (std::get<0>(GetParam()) == MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
                component = mocks::mfx::dx11::make_decoder(type, nullptr, vp);
            else
                component = mocks::mfx::dx11::make_encoder(type, nullptr, vp);

            EndInitialization(MFX_FOURCC_NV12, std::get<0>(GetParam()));

            ON_CALL(mock, Call(testing::_, testing::_))
                .WillByDefault(testing::Invoke(&mfxFrameSurfaceInterfaceImpl::Map_impl));
            dst->FrameInterface->Map = src->FrameInterface->Map = &Map;
            memtype = std::get<1>(GetParam());
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

    std::vector<mfxU16> IOPatterns =
    {
        MFX_IOPATTERN_OUT_SYSTEM_MEMORY
      , MFX_IOPATTERN_IN_VIDEO_MEMORY
    };

    std::vector<mfxU16> memory =
    {
        MFX_MEMTYPE_INTERNAL_FRAME
      , MFX_MEMTYPE_EXTERNAL_FRAME
    };

    INSTANTIATE_TEST_SUITE_P(
        IOPatterns,
        coreCopy,
        testing::Combine(
            testing::ValuesIn(IOPatterns),
            testing::ValuesIn(memory)
        )
    );

    TEST_P(coreCopy, DoFastCopyWrapperSystemMemory)
    {
        EXPECT_EQ(
            s->m_pCORE->DoFastCopyWrapper(dst, MFX_MEMTYPE_SYSTEM_MEMORY, src, MFX_MEMTYPE_SYSTEM_MEMORY),
            MFX_ERR_NONE
        );
    }

    TEST_P(coreCopy, DoFastCopyWrapper)
    {
        EXPECT_CALL(mock, Call(testing::_, testing::_)).Times(0);
        EXPECT_EQ(
            s->m_pCORE->DoFastCopyWrapper(dst, memtype, src, memtype),
            MFX_ERR_NONE
        );
    }

    TEST_P(coreCopy, DoFastCopyWrapperZeroMemIdZeroPtrs)
    {
        EXPECT_CALL(mock, Call(testing::_, testing::_)).Times(0);
        mfxFrameSurface1 emptydst{}, emptysrc{};
        EXPECT_EQ(
            s->m_pCORE->DoFastCopyWrapper(&emptydst, memtype, &emptysrc, memtype),
            memtype == MFX_MEMTYPE_INTERNAL_FRAME ?
            MFX_ERR_LOCK_MEMORY :
            MFX_ERR_UNDEFINED_BEHAVIOR
        );
    }

    TEST_P(coreCopy, DoFastCopyWrapperZeroMemIdNonZeroPtrs)
    {
        EXPECT_CALL(mock, Call(testing::_, testing::_)).Times(0);
        dst->Data.MemId = src->Data.MemId = nullptr;
        EXPECT_EQ(
            s->m_pCORE->DoFastCopyWrapper(dst, memtype, src, memtype),
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
            s->m_pCORE->DoFastCopyWrapper(dst, memtype, src, memtype),
            MFX_ERR_NONE
        );
    }

}

#endif // MFX_ENABLE_UNIT_TEST_CORE