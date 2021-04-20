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

#pragma once
#if defined(MFX_ONEVPL)
#if defined (MFX_ENABLE_UNIT_TEST_ALLOCATOR)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
#include "mfx_common.h"

#include "libmfx_allocator.h"

#if defined(MFX_VA_WIN)

#include "mfx_session.h"
#include "encoding_ddi.h"

#include "libmfx_allocator_d3d11.h"

#include "mfxpcp.h"

#define ENABLE_MFX_INTEL_GUID_DECODE
#define ENABLE_MFX_INTEL_GUID_ENCODE
#define ENABLE_MFX_INTEL_GUID_PRIVATE

#include "mocks/include/dxgi/format.h"
#include "mocks/include/dxgi/device/factory.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/decoder.h"
#include "mocks/include/mfx/dx11/encoder.h"

#elif defined(MFX_VA_LINUX)
#include "libmfx_allocator_vaapi.h"

#include "mocks/include/va/display/display.h"
#include "mocks/include/va/display/surface.h"
#endif

namespace test
{
#if defined(MFX_VA_WIN)
    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID = 42; //The value which is returned both as 'DeviceID' by adapter and by mocked registry.
                                        //Actual value doesn't matter here.
    struct FlexibleAllocatorBase
        : public testing::Test
    {

        std::unique_ptr<mocks::registry>             registry;
        std::unique_ptr<mocks::dxgi::factory>        factory;
        std::unique_ptr<mocks::dx11::context>        context;
        std::unique_ptr<mocks::mfx::dx11::component> component;

        mfxVideoParam                                vp{};
        mfxFrameAllocRequest                         req{};
        mfxFrameAllocResponse                        res{};
        std::unique_ptr<FrameAllocatorBase>          allocator;
        std::vector<mfxU32>                          buffer;

        void SetUp() override
        {
            ASSERT_NO_THROW({
                registry = mocks::mfx::dispatch_to(INTEL_VENDOR_ID, DEVICE_ID, 1);
                });

            factory = mocks::dxgi::make_factory(
                DXGI_ADAPTER_DESC1{ L"iGFX", INTEL_VENDOR_ID, DEVICE_ID },
                __uuidof(IDXGIFactory), __uuidof(IDXGIFactory1)
            );
            vp.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            vp.mfx.FrameInfo.Width = 640;
            vp.mfx.FrameInfo.Height = 480;
            ///TEMP: need to infer profile from given GUID
            vp.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
            vp.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

            buffer.resize(vp.mfx.FrameInfo.Width * vp.mfx.FrameInfo.Height * 8);
            context = mocks::dx11::make_context(
                std::make_tuple(D3D11_MAP_READ, buffer.data(), vp.mfx.FrameInfo.Width),
                std::make_tuple(D3D11_MAP_WRITE, buffer.data(), vp.mfx.FrameInfo.Width),
                std::make_tuple(D3D11_MAP_READ_WRITE, buffer.data(), vp.mfx.FrameInfo.Width)
            );

            auto constexpr type = mocks::mfx::HW_SCL;
            component.reset(
                new mocks::mfx::dx11::component(type, nullptr, vp)
            );
            mocks::dx11::mock_device(*(component->device.p), context.get(),
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_SHARED }
            );
            req.AllocId = 1;
            req.Info.FourCC = MFX_FOURCC_NV12;
            req.Info.Width = vp.mfx.FrameInfo.Width;
            req.Info.Height = vp.mfx.FrameInfo.Height;
            req.NumFrameMin = req.NumFrameSuggested = 1;
        }

        void SetAllocator(mfxU16 memType)
        {
            if (memType & MFX_MEMTYPE_SYSTEM_MEMORY)
            {
                allocator.reset(
                    new FlexibleFrameAllocatorSW{ component->device.p }
                );
            }
            else if (memType & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
            {
                allocator.reset(
                    new FlexibleFrameAllocatorHW_D3D11{ component->device.p }
                );
            }
        }
    };
#elif defined(MFX_VA_LINUX)
    struct FlexibleAllocatorBase
        : public testing::Test
    {
        mfxFrameAllocRequest                req{};
        mfxFrameAllocResponse               res{};
        std::shared_ptr<mocks::va::display> display;
        std::unique_ptr<FrameAllocatorBase> allocator;
        std::vector<int>                    buffer;

        void SetUp() override
        {
            buffer.resize(640*480*8);
            auto surface_attributes = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger,.value = {.i = VA_FOURCC_NV12} } };
            auto f = [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
            { return buffer.data(); };
            display = mocks::va::make_display(
                std::make_tuple(0, 0),  // version

                std::make_tuple(
                    VA_RT_FORMAT_YUV420,
                    640, 480,
                    std::make_tuple(surface_attributes),
                    f
                )
            );

            req.AllocId = 1;
            req.Info.FourCC = MFX_FOURCC_NV12;
            req.Info.Width = 640;
            req.Info.Height = 480;
            req.NumFrameMin = req.NumFrameSuggested = 1;
        }

        void SetAllocator(mfxU16 memType)
        {
            if (memType & MFX_MEMTYPE_SYSTEM_MEMORY)
            {
                allocator.reset(
                    new FlexibleFrameAllocatorSW{ display.get() }
                );
            }
            else if (memType & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
            {
                allocator.reset(
                    new FlexibleFrameAllocatorHW_VAAPI{ display.get() }
                );
            }
        }
    };
#endif

    struct FlexibleAllocator
        : public FlexibleAllocatorBase
        , public testing::WithParamInterface<mfxU16>
    {
        void SetUp() override
        {
            FlexibleAllocatorBase::SetUp();
            req.Type = GetParam();
#if (defined(_WIN32) || defined(_WIN64))
            if(req.Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
                req.Type |= MFX_MEMTYPE_SHARED_RESOURCE;
#endif
            SetAllocator(GetParam());
        }
    };

    static std::vector<mfxU16> memTypes =
    {
        MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_INTERNAL_FRAME,
        MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME
    };
}
#endif // MFX_ENABLE_UNIT_TEST_ALLOCATOR
#endif
