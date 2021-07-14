// Copyright (c) 2021 Intel Corporation
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
#if defined (MFX_ENABLE_UNIT_TEST_MEMORY)

#define MFX_API_FUNCTION_IMPL(NAME, RTYPE, ARGS_DECL, ARGS) RTYPE NAME ARGS_DECL;

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfx_config.h"
#include "mfxvideo++.h"

#define MFX_ENABLE_H265_VIDEO_ENCODE

#include "mfxdispatcher.h"

#include "encoding_ddi.h"

#include <initguid.h>
#include "mocks/include/mfx/guids.h"

#include "mocks/include/dxgi/format.h"
#include "mocks/include/dxgi/device/factory.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/decoder.h"
#include "mocks/include/mfx/dx11/encoder.h"

namespace test
{
    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID = 0xFF20; // MFX_HW_TGL_LP

    struct Memory
        : public ::testing::TestWithParam<std::pair<GUID, mfxU16 /*IO Pattern*/> >
    {
        std::unique_ptr<mocks::registry>             registry;
        std::unique_ptr<mocks::dxgi::factory>        factory;
        std::unique_ptr<mocks::mfx::dx11::component> component;

        mfxVideoParam                                vp{};
        mfxSession                                   session;
        mfxFrameSurface1*                            surface = nullptr;

        std::unique_ptr<mfxFrameAllocator>           allocator;
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

            vp.IOPattern = GetParam().second;

            vp.mfx.FrameInfo.Width = 640;
            vp.mfx.FrameInfo.Height = 480;

            ///TEMP: need to infer profile from given GUID
            vp.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;

            vp = mocks::mfx::make_param(
                MFX_FOURCC_NV12,
                mocks::mfx::make_param(GetParam().first, vp)
            );

            auto constexpr type = mocks::mfx::HW_TGL_LP;
            auto id = GetParam().first;
            if (mocks::dxva::is_decoder(id))
                component = mocks::mfx::dx11::make_decoder(type, nullptr, vp);
            else if (mocks::dxva::is_encoder(id))
                component = mocks::mfx::dx11::make_encoder(type, nullptr, vp);
            else
                component.reset(new mocks::mfx::dx11::component(type, nullptr, vp));

            mfxLoader loader = MFXLoad();
            ASSERT_TRUE(loader != nullptr);

            mfxConfig cfg = MFXCreateConfig(loader);
            ASSERT_TRUE(cfg != nullptr);

            mfxVariant impl_value{};
            impl_value.Type = MFX_VARIANT_TYPE_U32;
            impl_value.Data.U32 = MFX_CODEC_HEVC;

            ASSERT_EQ(
                MFXSetConfigFilterProperty(
                    cfg,
                    mocks::dxva::is_decoder(id) ?
                        (mfxU8 *)"mfxImplDescription.mfxDecoderDescription.decoder.CodecID"
                        : (mfxU8 *)"mfxImplDescription.mfxEncoderDescription.encoder.CodecID",
                    impl_value
                ),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                MFXCreateSession(loader, 0, &session),
                MFX_ERR_NONE
            );

            buffer.resize(vp.mfx.FrameInfo.Width*vp.mfx.FrameInfo.Height);
            // for ID3D11DeviceContext::Map (buffer.data() will be returned as a result)
            mocks::dx11::mock_context(*(component->context.p),
                std::make_tuple(D3D11_MAP_READ, buffer.data(), vp.mfx.FrameInfo.Width),
                std::make_tuple(D3D11_MAP_WRITE, buffer.data(), vp.mfx.FrameInfo.Width),
                std::make_tuple(D3D11_MAP_READ_WRITE, buffer.data(), vp.mfx.FrameInfo.Width)
            );

            // for MFXVideoDecodeInit (for ID3D11Device::CreateBuffer)
            mocks::dx11::mock_device(*(component->device.p), component->context.p,
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED }
            );

            // for MFXVideoENCODE_Init (for ID3D11Device::CreateBuffer)
            mocks::dx11::mock_device(*(component->device.p),
                D3D11_BUFFER_DESC{ UINT(vp.mfx.FrameInfo.Width) * UINT(vp.mfx.FrameInfo.Height * 2), D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0u, 0u }
            );

            ASSERT_EQ(
                mocks::dxva::is_decoder(id) ?
                MFXVideoDECODE_Init(session, &vp) :
                mocks::dxva::is_encoder(id) ?
                MFXVideoENCODE_Init(session, &vp) :
                MFXVideoVPP_Init(session, &vp),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                GetSurface(&surface),
                MFX_ERR_NONE
            );
        }
        void TearDown() override
        {
            ASSERT_EQ(
                MFXClose(session),
                MFX_ERR_NONE
            );
        }

        mfxStatus GetSurface(mfxFrameSurface1** surface)
        {
            auto id = GetParam().first;
            return
                mocks::dxva::is_decoder(id) ?
                MFXMemory_GetSurfaceForDecode(session, surface) :
                mocks::dxva::is_encoder(id) ?
                MFXMemory_GetSurfaceForEncode(session, surface) :
                MFXMemory_GetSurfaceForVPP(session, surface);
        }
    };

    INSTANTIATE_TEST_SUITE_P(
        Decode,
        Memory,
        testing::Values(
            std::make_pair(DXVA_ModeHEVC_VLD_Main, mfxU16(MFX_IOPATTERN_OUT_SYSTEM_MEMORY)),
            std::make_pair(DXVA_ModeHEVC_VLD_Main, mfxU16(MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        )
    );

    INSTANTIATE_TEST_SUITE_P(
        Encode,
        Memory,
        testing::Values(
            std::make_pair(DXVA2_Intel_Encode_HEVC_Main, mfxU16(MFX_IOPATTERN_IN_SYSTEM_MEMORY)),
            std::make_pair(DXVA2_Intel_Encode_HEVC_Main, mfxU16(MFX_IOPATTERN_IN_VIDEO_MEMORY))
        )
    );
}

#endif // MFX_ENABLE_UNIT_TEST_MEMORY
#endif // MFX_ONEVPL
