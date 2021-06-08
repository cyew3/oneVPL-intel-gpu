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

#include "flexible_allocator_base.h"

namespace test
{
    TEST_P(FlexibleAllocator, ReallocSurface)
    {
        if (GetParam() & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
        {
#if defined(MFX_VA_WIN)
            mocks::dx11::mock_device(*(component->device.p),
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width * 2), UINT(vp.mfx.FrameInfo.Height * 2), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width * 2), UINT(vp.mfx.FrameInfo.Height * 2), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_SHARED }
            );
            mocks::dx11::mock_context(*context,
                std::make_tuple(D3D11_MAP_READ, buffer.data(), vp.mfx.FrameInfo.Width * 2)
            );
#elif defined(MFX_VA_LINUX)
            auto surface_attributes = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger,.value = {.i = VA_FOURCC_NV12} } };
            auto f = [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
                { return buffer.data(); };
            mocks::va::mock_display(*display.get(),
                std::make_tuple(
                    VA_RT_FORMAT_YUV420,
                    1280, 960,
                    std::make_tuple(surface_attributes),
                    f
                )
            );
#endif
        }

        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameInfo info{};
        info.FourCC = MFX_FOURCC_NV12;
        info.Width = req.Info.Width * 2;
        info.Height = req.Info.Height * 2;
        ASSERT_EQ(
            allocator->ReallocSurface(info, res.mids[0]),
            MFX_ERR_NONE
        );

        mfxFrameData data{};
        EXPECT_EQ(
            allocator->Lock(res.mids[0], &data, MFX_MAP_READ),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            data.PitchHigh,
            info.Width >> 16
        );
        EXPECT_EQ(
            data.Pitch,
            info.Width & 0xffff
        );
        EXPECT_EQ(
            (data.UV - data.Y) / data.Pitch,
            info.Height
        );
    }

    TEST_P(FlexibleAllocator, ReallocSurfaceInvalidMid)
    {
        mfxFrameInfo info{};
        info.FourCC = MFX_FOURCC_NV12;
        info.Width = req.Info.Width;
        info.Height = req.Info.Height;
        EXPECT_EQ(
            allocator->ReallocSurface(info, nullptr),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_P(FlexibleAllocator, ReallocSurfaceChangeFourCC)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameInfo info{};
        info.Width = req.Info.Width;
        info.Height = req.Info.Height;
        info.FourCC = MFX_FOURCC_P8_TEXTURE;
        EXPECT_EQ(
            allocator->ReallocSurface(info, res.mids[0]),
            MFX_ERR_INVALID_VIDEO_PARAM
        );
    }

    TEST_P(FlexibleAllocator, ReallocLockedSurface)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameInfo info{};
        info.FourCC = req.Info.FourCC;
        info.Width = req.Info.Width * 2;
        info.Height = req.Info.Height * 2;
        mfxFrameData data{};
        ASSERT_EQ(
            allocator->Lock(res.mids[0], &data, MFX_MAP_READ),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            allocator->ReallocSurface(info, res.mids[0]),
            MFX_ERR_LOCK_MEMORY
        );
    }

    struct FlexibleAllocatorRealloc : public FlexibleAllocatorBase,
        testing::WithParamInterface <std::tuple <mfxU16, std::tuple<mfxU16 mfxFrameInfo::*, mfxU16, mfxU16>>>
    {
        void SetUp() override
        {
            FlexibleAllocatorBase::SetUp();
            req.Type = std::get<0>(GetParam());
            SetAllocator(std::get<0>(GetParam()));
        }
    };

    std::vector<std::tuple<mfxU16 mfxFrameInfo::*, mfxU16, mfxU16>> Info = {
        {&mfxFrameInfo::BitDepthLuma, 0, 8},
        {&mfxFrameInfo::BitDepthChroma, 0, 8}
    };

    INSTANTIATE_TEST_SUITE_P(
        MemTypesCorrectInfo,
        FlexibleAllocatorRealloc,
        testing::Combine(
            testing::ValuesIn(memTypes),
            testing::ValuesIn(Info)
        )
    );

    TEST_P(FlexibleAllocatorRealloc, ReallocSurfaceChangeInfoFieldsFromZero)
    {

        auto infoValue = std::get<1>(GetParam());

        auto memberPtr = std::get<0>(infoValue);
        mfxU16 correctValue1 = std::get<1>(infoValue);
        mfxU16 correctValue2 = std::get<2>(infoValue);

        mfxFrameInfo correctInfo{};
        correctInfo = req.Info;
        req.Info.*memberPtr = correctInfo.*memberPtr = correctValue1;
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        correctInfo.*memberPtr = correctValue2;
        EXPECT_EQ(
            allocator->ReallocSurface(correctInfo, res.mids[0]),
            MFX_ERR_NONE
        );
    }

    struct FlexibleAllocatorWrongRealloc : public FlexibleAllocatorRealloc
    {
        void SetUp() override
        {
            FlexibleAllocatorRealloc::SetUp();
            if (req.Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
            {
#if defined(MFX_VA_WIN)
                mocks::dx11::mock_device(*(component->device.p), context.get(),
                    D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_P016, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED }
                );
#elif defined(MFX_VA_LINUX)
                auto surface_attributes = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger,.value = {.i = VA_FOURCC_P016} } };
                auto f = [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
                    { return buffer.data(); };
                mocks::va::mock_display(*display.get(),
                    std::make_tuple(
                        VA_FOURCC_P016,
                        640, 480,
                        std::make_tuple(surface_attributes),
                        f
                    )
                );
#endif
            }
            req.Info.FourCC = MFX_FOURCC_P016;
        }
    };

    std::vector<std::tuple<mfxU16 mfxFrameInfo::*, mfxU16, mfxU16>> wrongInfo = {
        { &mfxFrameInfo::BitDepthLuma, 0, 10 },
        { &mfxFrameInfo::BitDepthLuma, 12, 10 },
        { &mfxFrameInfo::BitDepthChroma, 0, 10 },
        { &mfxFrameInfo::BitDepthChroma, 12, 10 },
        { &mfxFrameInfo::Shift, 0, 1 },
        { &mfxFrameInfo::ChromaFormat, MFX_CHROMAFORMAT_YUV400, MFX_CHROMAFORMAT_YUV444 },
        { &mfxFrameInfo::ChromaFormat, MFX_CHROMAFORMAT_YUV444, MFX_CHROMAFORMAT_YUV400 }
    };

    INSTANTIATE_TEST_SUITE_P(
        MemTypesInfo,
        FlexibleAllocatorWrongRealloc,
        testing::Combine(
            testing::ValuesIn(memTypes),
            testing::ValuesIn(wrongInfo)
        )
    );

    TEST_P(FlexibleAllocatorWrongRealloc, ReallocSurfaceChangeNotAllowedInfoFields)
    {
        auto info = std::get<1>(GetParam());
        auto memberPtr = std::get<0>(info);
        auto memberCorrectValue = std::get<1>(info);
        auto memberWrongValue = std::get<2>(info);
        req.Info.*memberPtr = memberCorrectValue;
        mfxFrameInfo correctInfo = req.Info;
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};
        mfxFrameInfo wrongInfo = correctInfo;
        wrongInfo.*memberPtr = memberWrongValue;
        EXPECT_EQ(
            allocator->ReallocSurface(wrongInfo, res.mids[0]),
            MFX_ERR_INVALID_VIDEO_PARAM
        );
    }
}
#endif // MFX_ENABLE_UNIT_TEST_ALLOCATOR
