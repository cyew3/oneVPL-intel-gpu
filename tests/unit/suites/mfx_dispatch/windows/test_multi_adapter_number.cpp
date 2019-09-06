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

#include "mocks/include/mfx/platform.h"
#include "mocks/include/mfx/traits.h"

#include "mfxadapter.h"

namespace test
{
    int constexpr INTEL_VENDOR_ID = 0x8086;

    struct QueryAdaptersNumber
        : public testing::Test
    {
        //need to mock D3D to avoid calling real device when 'MFX_D3D9_ENABLED' is defined
        std::unique_ptr<mocks::dx9::d3d> stub;

        void SetUp() override
        {
            stub = mocks::dx9::make_d3d();
        }
    };

    TEST_F(QueryAdaptersNumber, NoAdapters)
    {
        auto f = mocks::dxgi::make_factory( //no adapters
            __uuidof(IDXGIFactory1)
        );

        mfxU32 count;
        EXPECT_EQ(
            MFXQueryAdaptersNumber(&count),
            MFX_ERR_NONE
        );
        EXPECT_EQ(count, mfxU32(0));
    }

    TEST_F(QueryAdaptersNumber, NoIntelAdapters)
    {
        auto f = mocks::dxgi::make_factory( //no usable adapters
            DXGI_ADAPTER_DESC1{ L"A0",  0x4234 },
            DXGI_ADAPTER_DESC1{ L"A1",  0x3442 },
            __uuidof(IDXGIFactory1)
        );

        mfxU32 count;
        EXPECT_EQ(
            MFXQueryAdaptersNumber(&count),
            MFX_ERR_NONE
        );
        EXPECT_EQ(count, mfxU32(0));
    }

    TEST_F(QueryAdaptersNumber, AdaptersNumber)
    {
        auto f = mocks::dxgi::make_factory( //Only 3 usable adapters
            DXGI_ADAPTER_DESC1{ L"A0",  INTEL_VENDOR_ID },
            DXGI_ADAPTER_DESC1{ L"A1",  0x4234 },
            DXGI_ADAPTER_DESC1{ L"A2",  INTEL_VENDOR_ID },
            DXGI_ADAPTER_DESC1{ L"A3",  0x3442 },
            DXGI_ADAPTER_DESC1{ L"A4",  INTEL_VENDOR_ID },
            __uuidof(IDXGIFactory1)
        );

        mfxU32 count;
        EXPECT_EQ(
            MFXQueryAdaptersNumber(&count),
            MFX_ERR_NONE
        );
        EXPECT_EQ(count, mfxU32(3));
    }
}

#endif //MFX_ENABLE_UNIT_TEST_MULTI_ADAPTER