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
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "libmfx_allocator.h"

class SurfArrayTest : public ::testing::Test
{
protected:
    SurfArrayTest()
    {
        m_surfArray = mfxSurfaceArrayImpl::CreateSurfaceArray();
    }

    ~SurfArrayTest()
    {
        mfxU32 cnt = 0;
        m_surfArray->GetRefCounter(m_surfArray, &cnt);
        while (cnt > 1)
        {
            m_surfArray->Release(m_surfArray);
            m_surfArray->GetRefCounter(m_surfArray, &cnt);
        }

        m_surfArray->Release(m_surfArray);
    }

    mfxSurfaceArray* m_surfArray;
};

TEST_F(SurfArrayTest, GetRefCounter)
{
    mfxU32 cnt = 0;
    mfxStatus sts = m_surfArray->GetRefCounter(m_surfArray, &cnt);

    EXPECT_EQ(sts, MFX_ERR_NONE);
    EXPECT_EQ(cnt, 1U);
}

TEST_F(SurfArrayTest, AddRef)
{
    mfxStatus sts = m_surfArray->AddRef(m_surfArray);
    EXPECT_EQ(sts, MFX_ERR_NONE);

    mfxU32 cnt = 0;
    sts = m_surfArray->GetRefCounter(m_surfArray, &cnt);
    EXPECT_EQ(sts, MFX_ERR_NONE);
    EXPECT_EQ(cnt, 2U);
}

TEST_F(SurfArrayTest, AddRefRelease)
{
    mfxStatus sts = m_surfArray->AddRef(m_surfArray);
    EXPECT_EQ(sts, MFX_ERR_NONE);

    mfxU32 cnt = 0;
    sts = m_surfArray->GetRefCounter(m_surfArray, &cnt);
    EXPECT_EQ(sts, MFX_ERR_NONE);
    EXPECT_EQ(cnt, 2U);

    sts = m_surfArray->Release(m_surfArray);
    EXPECT_EQ(sts, MFX_ERR_NONE);
    sts = m_surfArray->GetRefCounter(m_surfArray, &cnt);
    EXPECT_EQ(sts, MFX_ERR_NONE);
    EXPECT_EQ(cnt, 1U);
}

TEST_F(SurfArrayTest, CheckVersion)
{
    EXPECT_EQ(MFX_STRUCT_VERSION(m_surfArray->Version.Major, m_surfArray->Version.Minor), MFX_SURFACEARRAY_VERSION);
}

TEST_F(SurfArrayTest, SurfArrayNull)
{
    mfxStatus sts = m_surfArray->AddRef(nullptr);
    EXPECT_EQ(sts, MFX_ERR_NULL_PTR);
}

TEST_F(SurfArrayTest, InvalidHDL) {
    auto surfArray_copy = *m_surfArray;
    surfArray_copy.Context = nullptr;

    mfxStatus sts = surfArray_copy.AddRef(&surfArray_copy);
    EXPECT_EQ(sts, MFX_ERR_INVALID_HANDLE);
}

class SurfArrayImplTest : public ::testing::Test
{
protected:
    SurfArrayImplTest()
    {
        m_surfArray = mfxSurfaceArrayImpl::CreateSurfaceArray();
    }

    ~SurfArrayImplTest()
    {
        mfxU32 cnt = 0;
        ((mfxSurfaceArray*)m_surfArray)->GetRefCounter(m_surfArray, &cnt);
        while (cnt > 1)
        {
            ((mfxSurfaceArray*)m_surfArray)->Release(m_surfArray);
            ((mfxSurfaceArray*)m_surfArray)->GetRefCounter(m_surfArray, &cnt);
        }

        ((mfxSurfaceArray*)m_surfArray)->Release(m_surfArray);
    }

    mfxSurfaceArrayImpl* m_surfArray;
};

TEST_F(SurfArrayImplTest, AddSurface)
{
    ASSERT_EQ(m_surfArray->NumSurfaces, 0U);

    mfxFrameSurface1 srf;
    m_surfArray->AddSurface(&srf);

    ASSERT_NE(m_surfArray->Surfaces, nullptr);
    EXPECT_EQ(m_surfArray->Surfaces[0], &srf);
    EXPECT_EQ(m_surfArray->NumSurfaces, 1);
}

TEST_F(SurfArrayImplTest, AddSurfaceInSequence)
{
    ASSERT_EQ(m_surfArray->NumSurfaces, 0);

    mfxFrameSurface1 srf;
    m_surfArray->AddSurface(&srf);
    mfxFrameSurface1 srf2;
    m_surfArray->AddSurface(&srf2);

    ASSERT_NE(m_surfArray->Surfaces, nullptr);
    EXPECT_EQ(m_surfArray->Surfaces[0], &srf);
    EXPECT_EQ(m_surfArray->Surfaces[1], &srf2);
    EXPECT_EQ(m_surfArray->NumSurfaces, 2);
}
#endif