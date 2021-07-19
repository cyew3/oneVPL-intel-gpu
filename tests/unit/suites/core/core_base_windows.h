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

#pragma once

#if defined(MFX_ENABLE_UNIT_TEST_CORE)

#define MFX_API_FUNCTION_IMPL(NAME, RTYPE, ARGS_DECL, ARGS) RTYPE NAME ARGS_DECL;

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfx_config.h"
#include "mfxvideo++.h"

#include "encoding_ddi.h"
#include "libmfx_allocator_d3d11.h"

#include "mocks/include/dxgi/format.h"
#include "mocks/include/dxgi/device/factory.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/decoder.h"

#include "libmfx_core_factory.h"
#include "libmfx_core_vaapi.h"

namespace test
{
    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID = 0x4F80; // MFX_HW_DG2

    struct coreBase :
        public testing::Test
    {
        std::unique_ptr<VideoCORE>                   core;

        std::unique_ptr<mocks::registry>             registry;
        std::unique_ptr<mocks::dxgi::factory>        factory;
        std::unique_ptr<mocks::dx11::context>        context;
        std::unique_ptr<mocks::mfx::dx11::component> component;

        mfxVideoParam                                vp{};

        mfxFrameSurface1*                            src = nullptr;
        mfxFrameSurface1*                            dst = nullptr;

        std::unique_ptr<FrameAllocatorBase>          allocator;
        std::vector<mfxU32>                          buffer;

        static int constexpr                         type = MFX_HW_DG2;

        virtual ~coreBase()
        {
            if (src && src->FrameInterface && src->FrameInterface->Release)
                std::ignore = src->FrameInterface->Release(src);

            if (dst && dst->FrameInterface && dst->FrameInterface->Release)
                std::ignore = dst->FrameInterface->Release(dst);
        }

        void SetUp() override
        {
            ASSERT_NO_THROW({
                registry = mocks::mfx::dispatch_to(INTEL_VENDOR_ID, DEVICE_ID, 1);
            });

            factory = mocks::dxgi::make_factory(
                DXGI_ADAPTER_DESC1{ L"iGFX", INTEL_VENDOR_ID, DEVICE_ID },
                __uuidof(IDXGIFactory), __uuidof(IDXGIFactory1)
            );

            vp.mfx.FrameInfo.Width  = 640;
            vp.mfx.FrameInfo.Height = 480;
            ///TEMP: need to infer profile from given GUID
            vp.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;

            vp = mocks::mfx::make_param(
                MFX_FOURCC_NV12,
                mocks::mfx::make_param(DXVA_ModeHEVC_VLD_Main, vp)
            );

            buffer.resize(vp.mfx.FrameInfo.Width * vp.mfx.FrameInfo.Height * 8);

            component.reset(
                new mocks::mfx::dx11::component(type, nullptr, vp)
            );

            mock_context(*(component->context.p),
                std::make_tuple(D3D11_MAP_READ, buffer.data(), vp.mfx.FrameInfo.Width),
                std::make_tuple(D3D11_MAP_WRITE, buffer.data(), vp.mfx.FrameInfo.Width),
                std::make_tuple(D3D11_MAP_READ_WRITE, buffer.data(), vp.mfx.FrameInfo.Width)
            );

            mock_device(*(component->device.p), component->context.p,
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_SHARED }
            );

            core.reset(
                FactoryCORE::CreateCORE(MFX_HW_D3D11, 0, 0, nullptr)
            );

            buffer.resize(vp.mfx.FrameInfo.Width * vp.mfx.FrameInfo.Height * 4);
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

    struct coreMain :
        public coreBase
    {
        void SetUp() override
        {
            coreBase::SetUp();
            allocator.reset(
                new FlexibleFrameAllocatorHW_D3D11{ component->device.p }
            );

            ASSERT_EQ(
                allocator->CreateSurface(MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME, vp.mfx.FrameInfo, dst),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                allocator->CreateSurface(MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME, vp.mfx.FrameInfo, src),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                src->FrameInterface->Map(src, MFX_MAP_READ),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                dst->FrameInterface->Map(dst, MFX_MAP_READ),
                MFX_ERR_NONE
            );
        }
    };
}

#endif // MFX_ENABLE_UNIT_TEST_CORE
