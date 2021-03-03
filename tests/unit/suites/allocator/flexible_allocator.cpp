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

#include <tuple>
#include <algorithm>

namespace test
{
    INSTANTIATE_TEST_SUITE_P(
        MemTypes,
        FlexibleAllocator,
        testing::ValuesIn(memTypes)
    );

    TEST_P(FlexibleAllocator, Alloc)
    {
        req.NumFrameMin = req.NumFrameSuggested = 5;
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            res.AllocId,
            req.AllocId
        );
        EXPECT_EQ(
            res.NumFrameActual, 5
        );
        EXPECT_EQ(
            res.MemType,
            req.Type
        );

        mfxMemId mids[5] = { 0 };
        std::copy(res.mids, res.mids + res.NumFrameActual, mids);
        std::sort(mids, mids + res.NumFrameActual);
        EXPECT_EQ(
            std::unique(mids, mids + res.NumFrameActual) - mids,
            res.NumFrameActual
        );
        EXPECT_NE(
            mids[0],
            mfxMemId(0)
        );
    }

    TEST_P(FlexibleAllocator, AllocNullFrames)
    {
        req.NumFrameMin = req.NumFrameSuggested = 0;
        EXPECT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
    }

    TEST_P(FlexibleAllocator, AllocNotNullInfoFields)
    {
        req.Info.BitDepthLuma = req.Info.BitDepthChroma = 8;
        req.Info.Shift = 1;
        EXPECT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
    }

    TEST_P(FlexibleAllocator, AllocInvalidFourCC)
    {
        req.Info.FourCC = -1;
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_P(FlexibleAllocator, AllocWrongTypeVidSys)
    {
        if (req.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            req.Type = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME;
        else
            req.Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_INTERNAL_FRAME;
        EXPECT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_P(FlexibleAllocator, TryAllocExternal)
    {
        req.Type &= 0xfff0;
        req.Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

        EXPECT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_P(FlexibleAllocator, TryAllocNoExtInt)
    {
        req.Type &= 0xfff0;

        EXPECT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );

        mfxU16 expectedType = MFX_MEMTYPE_INTERNAL_FRAME;
#if (defined(_WIN32) || defined(_WIN64))
        if (req.Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
            expectedType |= MFX_MEMTYPE_SHARED_RESOURCE;
#endif

        EXPECT_EQ(
            res.MemType & 0xf,
            expectedType
        );
    }

    TEST_P(FlexibleAllocator, Free)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            allocator->Free(res),
            MFX_ERR_NONE
        );
    }

    TEST_P(FlexibleAllocator, FreeAfterRemove)
    {
        req.NumFrameMin = req.NumFrameSuggested = 5;
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        allocator->Remove(res.mids[1]);
        EXPECT_EQ(
            allocator->Free(res),
            MFX_ERR_NONE
        );
    }

    TEST_P(FlexibleAllocator, PartialFree)
    {
        req.NumFrameMin = req.NumFrameSuggested = 10;
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameAllocResponse res2 = res;

        res2.NumFrameActual = 5;
        EXPECT_EQ(
            allocator->Free(res2),
            MFX_ERR_INVALID_HANDLE
        );
        EXPECT_EQ(
            allocator->Free(res),
            MFX_ERR_NONE
        );

    }

    TEST_P(FlexibleAllocator, FreeWithoutAlloc)
    {
        EXPECT_EQ(
            allocator->Free(res),
            MFX_ERR_NOT_FOUND
        );
    }

    TEST_P(FlexibleAllocator, GetHDL)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        if (GetParam() & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            mfxHDL handle{};
            ASSERT_EQ(
                allocator->GetHDL(res.mids[0], handle),
                MFX_ERR_NONE
            );
            EXPECT_EQ(
                res.mids[0],
                handle
            );
        }
        else
        {
            mfxHDLPair handlePair{};
            ASSERT_EQ(
                allocator->GetHDL(res.mids[0], reinterpret_cast<mfxHDL&>(handlePair)),
                MFX_ERR_NONE
            );
            EXPECT_NE(
                handlePair.first,
                nullptr
            );
            EXPECT_EQ(
                handlePair.second,
                reinterpret_cast<mfxHDL>(0)
            );
        }
    }

    TEST_P(FlexibleAllocator, GetHDLInvalidMid)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxHDL handle{};
        EXPECT_EQ(
            allocator->GetHDL(nullptr, handle),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_P(FlexibleAllocator, CreateSurface)
    {
        mfxFrameInfo info{};
        info.FourCC = MFX_FOURCC_NV12;
        info.Width = req.Info.Width;
        info.Height = req.Info.Height;
        mfxFrameSurface1* surface{};
        ASSERT_EQ(
            allocator->CreateSurface(GetParam(), info, surface),
            MFX_ERR_NONE
        );
        ASSERT_NE(
            surface,
            nullptr
        );
        EXPECT_EQ(
            surface->Info.FourCC,
            info.FourCC
        );
        EXPECT_EQ(
            surface->Info.Width,
            info.Width
        );
        EXPECT_EQ(
            surface->Info.Height,
            info.Height
        );
        EXPECT_EQ(
            surface->Data.MemType,
            req.Type
        );
        ASSERT_NE(
            surface->FrameInterface,
            nullptr
        );
        ASSERT_NE(
            surface->FrameInterface->GetRefCounter,
            nullptr
        );

        mfxU32 counter = 0;
        ASSERT_EQ(
            surface->FrameInterface->GetRefCounter(surface, &counter),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            counter,
            1u
        );
    }

    TEST_P(FlexibleAllocator, CreateSurfaceWrongType)
    {
        mfxFrameInfo info{};
        info.FourCC = MFX_FOURCC_NV12;
        info.Width = req.Info.Width;
        info.Height = req.Info.Height;
        mfxFrameSurface1* surface{};
        mfxU16 wrongType = mfxU16((GetParam() & MFX_MEMTYPE_SYSTEM_MEMORY) ? MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET : MFX_MEMTYPE_SYSTEM_MEMORY);
        EXPECT_EQ(
            allocator->CreateSurface(wrongType, info, surface),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_P(FlexibleAllocator, SetDevice)
    {
        allocator.reset();
        if (GetParam() & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            allocator.reset(
                new FlexibleFrameAllocatorSW
            );
        }
        else if (GetParam() & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
        {
#if (defined(_WIN32) || defined(_WIN64))
            allocator.reset(
                new FlexibleFrameAllocatorHW_D3D11
            );
#elif defined(__linux__)
            allocator.reset(
                new FlexibleFrameAllocatorHW_VAAPI
            );
#endif
        }
#if (defined(_WIN32) || defined(_WIN64))
        allocator->SetDevice(component->device.p);
#elif defined(__linux__)
        allocator->SetDevice(display.get());
#endif
        EXPECT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
    }

    TEST_P(FlexibleAllocator, Remove)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        allocator->Remove(res.mids[0]);
        mfxFrameData data{};
        EXPECT_EQ(
            allocator->Lock(res.mids[0], &data, MFX_MAP_READ),
            MFX_ERR_NOT_FOUND
        );
    }
}
#endif // MFX_ENABLE_UNIT_TEST_ALLOCATOR
#endif
