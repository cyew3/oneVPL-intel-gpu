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
    TEST_F(coreMain, DoFastCopyNullptrDst)
    {
        EXPECT_EQ(
            s->m_pCORE->DoFastCopy(nullptr, src),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreMain, DoFastCopyNullptrSrc)
    {
        EXPECT_EQ(
            s->m_pCORE->DoFastCopy(dst, nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreMain, DoFastCopyNotMappedDst)
    {
        ASSERT_EQ(
            dst->FrameInterface->Unmap(dst),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            s->m_pCORE->DoFastCopy(dst, src),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreMain, DoFastCopyNotMappedSrc)
    {
        ASSERT_EQ(
            src->FrameInterface->Unmap(src),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            s->m_pCORE->DoFastCopy(dst, src),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreMain, DoFastCopyExtended)
    {
        dst->Data.MemId = src->Data.MemId = nullptr;
        EXPECT_EQ(
            s->m_pCORE->DoFastCopyExtended(dst, src),
            MFX_ERR_NONE
        );
    }

    TEST_F(coreMain, DoFastCopyExtendedNullptrDst)
    {
        EXPECT_EQ(
            s->m_pCORE->DoFastCopyExtended(nullptr, src),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreMain, DoFastCopyExtendedNullptrSrc)
    {
        EXPECT_EQ(
            s->m_pCORE->DoFastCopyExtended(dst, nullptr),
            MFX_ERR_NULL_PTR
        );
    }
}

#endif // MFX_ENABLE_UNIT_TEST_CORE
