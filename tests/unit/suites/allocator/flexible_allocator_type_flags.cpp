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
    struct FlexibleAllocatorTypeFlags : public FlexibleAllocatorBase,
        testing::WithParamInterface <std::tuple<mfxU32, mfxU32>>
    {
        void SetUp() override
        {
            FlexibleAllocatorBase::SetUp();
            req.Info.FourCC = std::get<1>(GetParam());
            req.Type = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | std::get<0>(GetParam());
#if defined(MFX_VA_WIN)
            mocks::dx11::mock_device(*(component->device.p), context.get(),
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_YUY2, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_HW_PROTECTED | D3D11_RESOURCE_MISC_SHARED }
            );
#elif defined(MFX_VA_LINUX)
            VASurfaceAttrib surface_attributes[] = { VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger, .value = {.i = VA_FOURCC_ABGR } } },
                                                     VASurfaceAttrib{ VASurfaceAttribUsageHint,   VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger, .value = {.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER } } }
                                                   };
            auto f = [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
            {return buffer.data();};

            mocks::va::mock_display(*display.get(),
                std::make_tuple(
                    VA_RT_FORMAT_RGB32,
                    640, 480,
                    std::make_tuple(surface_attributes, 2),
                    f
                )
            );
#endif
            SetAllocator(MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET);
        }
    };
    std::vector<std::tuple<mfxU32, mfxU32>> typeFlags = {
#if defined(MFX_VA_WIN)
        { MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME, MFX_FOURCC_NV12 },
        { MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET, MFX_FOURCC_NV12 },
        { MFX_MEMTYPE_FROM_VPPIN, MFX_FOURCC_YUY2 },
        { MFX_MEMTYPE_FROM_VPPIN, MFX_FOURCC_NV12 },
        { MFX_MEMTYPE_FROM_VPPOUT, MFX_FOURCC_NV12 },
        { MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET, MFX_FOURCC_NV12 },
        { MFX_MEMTYPE_SHARED_RESOURCE, MFX_FOURCC_NV12 },
#if !defined(OPEN_SOURCE)
        { MFX_MEMTYPE_PROTECTED, MFX_FOURCC_NV12 }
#endif
#elif defined(MFX_VA_LINUX)
        { MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET | MFX_MEMTYPE_FROM_ENCODE, MFX_FOURCC_BGR4 }
#endif
    };

    INSTANTIATE_TEST_SUITE_P(
        MemTypesFlags,
        FlexibleAllocatorTypeFlags,
        testing::ValuesIn(typeFlags)
    );

    TEST_P(FlexibleAllocatorTypeFlags, AllocWithWdifferentTypeFlags)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
    }
}
#endif // MFX_ENABLE_UNIT_TEST_ALLOCATOR
#endif