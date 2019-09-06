// Copyright (c) 2019 Intel Corporation
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

#if defined (MFX_ENABLE_UNIT_TEST_MULTI_ADAPTER)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mocks/include/guid.h"

#include "mocks/include/dxgi/format.h"
#include "mocks/include/dxgi/device/factory.h"

#include "mocks/include/dx9/device/d3d.h"

#include "mocks/include/dx11/device/device.h"
#include "mocks/include/dx11/device/video.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/decoder.h"

#include "mfxadapter.h"

namespace test
{
    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID       = 42; //The value which is returned both as 'DeviceID' by adapter and by mocked registry.
                                        //Actual value doesn't matter here.

    struct QueryAdaptersDecode
        : public testing::Test
    {
        //need to mock D3D to avoid calling real device when 'MFX_D3D9_ENABLED' is defined
        std::unique_ptr<mocks::dx9::d3d>      stub;
        std::unique_ptr<mocks::registry>      registry;
        std::unique_ptr<mocks::dxgi::factory> factory;
        std::unique_ptr<mocks::dx11::device>  iGFX;

        CComPtr<IDXGIAdapter>                 adapter1;

        mfxVideoParam                         vp{};

        void SetUp() override
        {
            stub = mocks::dx9::make_d3d();
            ASSERT_NO_THROW({
                registry = mocks::mfx::dispatch_to(INTEL_VENDOR_ID, DEVICE_ID, 1);
            });

            factory = mocks::dxgi::make_factory(
                DXGI_ADAPTER_DESC1{ L"A0",   0x4234 },
                DXGI_ADAPTER_DESC1{ L"iGFX", INTEL_VENDOR_ID, DEVICE_ID },
                __uuidof(IDXGIFactory),
                __uuidof(IDXGIFactory1)
            );

            ASSERT_HRESULT_SUCCEEDED(
                factory->EnumAdapters(1, &adapter1)
            );

            vp.mfx.FrameInfo.Width  = 640;
            vp.mfx.FrameInfo.Height = 480;
            vp = mocks::mfx::make_param(
                mocks::fourcc::tag<MFX_FOURCC_NV12>{},
                mocks::mfx::make_param(mocks::guid<&DXVA_ModeHEVC_VLD_Main>{}, vp)
            );

            //let iGFX be SCL & support HEVC Main (AVC is implicitly supported for private reporting)
            iGFX = mocks::mfx::dx11::make_decoder(adapter1.p, nullptr,
                std::integral_constant<int, mocks::mfx::HW_SKL>{},
                std::make_tuple(mocks::guid<&DXVA_ModeHEVC_VLD_Main>{}, vp)
            );
        }
    };

    TEST_F(QueryAdaptersDecode, QueryBitstream)
    {
        //176x144 AVC Main SPS/PPS
        mfxU8 headers[] = {
            0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xE0, 0x0A, 0x96, 0x52, 0x85, 0x89, 0xC8, 0x00, 0x00, 0x00,
            0x01, 0x68, 0xC9, 0x23, 0xC8, 0x00, 0x00, 0x00, 0x01, 0x09, 0x10
        };

        mfxBitstream bs{};
        bs.Data = headers;
        bs.DataLength = bs.MaxLength = sizeof(headers);

        mfxU32 count;
        ASSERT_EQ(
            MFXQueryAdaptersNumber(&count),
            MFX_ERR_NONE
        );
        ASSERT_EQ(count, mfxU32(1));

        std::vector<mfxAdapterInfo> adapters(count);
        ASSERT_TRUE(!adapters.empty());

        mfxAdaptersInfo info{
            adapters.data(),
            mfxU32(adapters.size())
        };

        EXPECT_EQ(
            MFXQueryAdaptersDecode(&bs, MFX_CODEC_AVC, &info),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            info.NumActual, adapters.size()
        );
        EXPECT_EQ(
            adapters[0].Number, mfxU32(1)
        );
        EXPECT_EQ(
            adapters[0].Platform.CodeName, MFX_PLATFORM_SKYLAKE
        );
        EXPECT_EQ(
            adapters[0].Platform.DeviceId, DEVICE_ID
        );
        EXPECT_EQ(
            adapters[0].Platform.MediaAdapterType, MFX_MEDIA_INTEGRATED
        );
    }

    TEST_F(QueryAdaptersDecode, QueryDecodeNoSuitable)
    {
        mfxU32 count;
        ASSERT_EQ(
            MFXQueryAdaptersNumber(&count),
            MFX_ERR_NONE
        );
        ASSERT_EQ(count, mfxU32(1));

        std::vector<mfxAdapterInfo> adapters(count);
        ASSERT_TRUE(!adapters.empty());

        //make unsupported video param
        auto vpx = mocks::mfx::make_param(
            mocks::fourcc::tag<MFX_FOURCC_P010>{},
            mocks::mfx::make_param(mocks::guid<&DXVA_ModeHEVC_VLD_Main10>{}, vp)
        );

        mfxComponentInfo component{
            MFX_COMPONENT_DECODE,
            vpx
        };

         mfxAdaptersInfo info{
            adapters.data(),
            mfxU32(adapters.size())
        };

        EXPECT_EQ(
            MFXQueryAdapters(&component, &info),
            MFX_ERR_NOT_FOUND //Looks like we need MFX_ERR_UNSUPPORTED, but actually 'IsGuidSupported' returns not found 
        );
        EXPECT_EQ(
            info.NumActual, mfxU32(0)
        );
    }

    TEST_F(QueryAdaptersDecode, QueryDecode)
    {
        //add a couple of adapters to mocked factory
        mock_factory(*factory,
            DXGI_ADAPTER_DESC1{ L"A1",   0x4234 },
            DXGI_ADAPTER_DESC1{ L"dGFX", INTEL_VENDOR_ID, DEVICE_ID }
        );

        mfxU32 count;
        ASSERT_EQ(
            MFXQueryAdaptersNumber(&count),
            MFX_ERR_NONE
        );
        ASSERT_EQ(count, mfxU32(2));

        std::vector<mfxAdapterInfo> adapters(count);
        ASSERT_TRUE(!adapters.empty());

        mfxAdaptersInfo info{
            adapters.data(),
            mfxU32(adapters.size())
        };

        auto vp2 = mocks::mfx::make_param(
            mocks::fourcc::tag<MFX_FOURCC_P016>{},
            mocks::mfx::make_param(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_Main12Profile>{}, vp)
        );
        vp2.mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;

        CComPtr<IDXGIAdapter> adapter3;
        ASSERT_HRESULT_SUCCEEDED(
            factory->EnumAdapters(3, &adapter3)
        );

        //let dGFX be DG1 & support both HEVC Main & Main12
        auto dGFX = mocks::mfx::dx11::make_decoder(adapter3.p, nullptr,
            std::integral_constant<int, mocks::mfx::HW_DG1>{},
            std::make_tuple(mocks::guid<&DXVA_ModeHEVC_VLD_Main>{}, vp),
            std::make_tuple(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_Main12Profile>{}, vp2)
        );

        mfxComponentInfo component1{
            MFX_COMPONENT_DECODE,
            vp
        };

        EXPECT_EQ(
            MFXQueryAdapters(&component1, &info),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            info.NumActual, adapters.size()
        );
        EXPECT_EQ(
            adapters[0].Number, mfxU32(1)
        );
        EXPECT_EQ(
            adapters[0].Platform.CodeName, MFX_PLATFORM_SKYLAKE
        );
        EXPECT_EQ(
            adapters[0].Platform.DeviceId, DEVICE_ID
        );
        EXPECT_EQ(
            adapters[0].Platform.MediaAdapterType, MFX_MEDIA_INTEGRATED
        );

        mfxComponentInfo component2{
            MFX_COMPONENT_DECODE,
            vp2
        };

        EXPECT_EQ(
            MFXQueryAdapters(&component2, &info),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            info.NumActual, mfxU32(1)
        );
        EXPECT_EQ(
            adapters[0].Number, mfxU32(3)
        );
        EXPECT_EQ(
            adapters[0].Platform.CodeName, MFX_PLATFORM_TIGERLAKE
        );
        EXPECT_EQ(
            adapters[0].Platform.DeviceId, DEVICE_ID
        );
        EXPECT_EQ(
            adapters[0].Platform.MediaAdapterType, MFX_MEDIA_DISCRETE
        );
    }
}

#endif //MFX_ENABLE_UNIT_TEST_MULTI_ADAPTER