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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mocks/include/common.h"
#include "mocks/include/mfx/core.h"

namespace mocks { namespace test { namespace mfx
{
    TEST(core, GetHandleWrongType)
    {
        auto core = mocks::mfx::make_core(
            std::make_tuple(MFX_HANDLE_D3D11_DEVICE, mfxHDL(42))
        );

        mfxHDL hdl;
        EXPECT_EQ(
            core->GetHandle(MFX_HANDLE_CM_DEVICE, &hdl),
            MFX_ERR_NOT_FOUND
        );
    }

    TEST(core, GetHandleNullHandle)
    {
        auto core = mocks::mfx::make_core(
            std::make_tuple(MFX_HANDLE_D3D11_DEVICE, mfxHDL(42))
        );

        EXPECT_EQ(
            core->GetHandle(MFX_HANDLE_D3D11_DEVICE, nullptr),
            MFX_ERR_NULL_PTR
        );
    }

	TEST(core, GetHandle)
    {
        auto core = mocks::mfx::make_core(
            std::make_tuple(MFX_HANDLE_D3D11_DEVICE, mfxHDL(42))
        );

        mfxHDL hdl;
        EXPECT_EQ(
            core->GetHandle(MFX_HANDLE_D3D11_DEVICE, &hdl),
            MFX_ERR_NONE
        );
    }

    TEST(core, GetHWType)
    {
        auto core = mocks::mfx::make_core(
            MFX_HW_ICL
        );

        EXPECT_EQ(
            core->GetHWType(), MFX_HW_ICL
        );
    }

    TEST(core, GetVAType)
    {
        auto core = mocks::mfx::make_core(
            MFX_HW_D3D11
        );

        EXPECT_EQ(
            core->GetVAType(), MFX_HW_D3D11
        );
    }

    TEST(core, SetCoreId)
    {
        auto core = mocks::mfx::make_core();

        EXPECT_FALSE(
            core->SetCoreId(42)
        );

        mock_core(*core, std::make_tuple(42, true));
        EXPECT_TRUE(
            core->SetCoreId(42)
        );
        EXPECT_FALSE(
            core->SetCoreId(1)
        );
    }

    TEST(core, QueryCoreInterfaceNoInterface)
    {
        auto core = mocks::mfx::make_core();
        EXPECT_EQ(
            core->QueryCoreInterface(MFX_GUID{}), nullptr
        );
    }

    TEST(core, QueryCoreInterface)
    {
        auto ptr = reinterpret_cast<void*>(42);
        MFX_GUID guid {1, 2, 3 };
        auto core = mocks::mfx::make_core(
            std::make_tuple(guid, ptr)
        );

        EXPECT_EQ(
            core->QueryCoreInterface(guid), ptr
        );
    }

    TEST(core, GetFrameHDLInvalidMid)
    {
        auto core = mocks::mfx::make_core(
            std::make_tuple(mfxMemId(42), mfxHDL(42), true)
        );

        mfxHDL hdl;
        EXPECT_EQ(
            core->GetFrameHDL(nullptr, &hdl, true),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST(core, GetFrameHDLNullHdl)
    {
        auto core = mocks::mfx::make_core(
            std::make_tuple(mfxMemId(42), mfxHDL(42), true)
        );

        EXPECT_EQ(
            core->GetFrameHDL(mfxMemId(42), nullptr, true),
            MFX_ERR_NULL_PTR
        );
    }

    TEST(core, GetFrameHDL)
    {
        auto core = mocks::mfx::make_core(
            std::make_tuple(mfxMemId(42), mfxHDL(42), true)
        );

        mfxHDL hdl;
        EXPECT_EQ(
            core->GetFrameHDL(mfxMemId(42), &hdl, true),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            hdl, mfxHDL(42)
        );
    }

    TEST(core, AllocFramesNullRequest)
    {
        auto core = mocks::mfx::make_core(
            std::make_tuple(mfxFrameAllocRequest{}, mfxFrameAllocResponse{})
        );

        mfxFrameAllocResponse response;
        EXPECT_EQ(
            core->AllocFrames(nullptr, &response, true),
            MFX_ERR_NULL_PTR
        );
    }

    TEST(core, AllocFramesNullResponse)
    {
        mfxFrameAllocRequest request{};
        auto core = mocks::mfx::make_core(
            std::make_tuple(request, mfxFrameAllocResponse{})
        );

        EXPECT_EQ(
            core->AllocFrames(&request, nullptr, true),
            MFX_ERR_NULL_PTR
        );
    }

    TEST(core, AllocFrames)
    {
        mfxFrameAllocRequest  request{};
        auto core = mocks::mfx::make_core(
            std::make_tuple(request, mfxFrameAllocResponse{ 42 })
        );

        mfxFrameAllocResponse response;
        EXPECT_EQ(
            core->AllocFrames(&request, &response, true),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            response.AllocId,
            42
        );
    }

} } }
