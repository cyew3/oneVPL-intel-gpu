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

#if defined(MFX_ENABLE_UNIT_TEST_CORE)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
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

#include "mocks/include/dx9/device/d3d.h"

#include <vector>

#undef MFX_PROTECTED_FEATURE_DISABLE

namespace test
{
    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID = 42; //The value which is returned both as 'DeviceID' by adapter and by mocked registry.
                                  //Actual value doesn't matter here.
    struct coreHandleBase :
        public testing::Test
    {
        std::unique_ptr<mocks::registry>             registry;
        std::unique_ptr<mocks::dxgi::factory>        factory;
        std::unique_ptr<mocks::mfx::dx11::component> component;


        mfxVideoParam                                vp{};
        MFXVideoSession                              session;
        std::vector<mfxU32>                          buffer;

        mfxHDL                                       hdl{};
        mfxExtBuffer*                                ext_buffer_ptr;
        mfxExtBuffer                                 ext_buffer{};
        void SetUp() override
        {
            ASSERT_NO_THROW({
                registry = mocks::mfx::dispatch_to(INTEL_VENDOR_ID, DEVICE_ID, 1);
            });

            factory = mocks::dxgi::make_factory(
                DXGI_ADAPTER_DESC1{ L"iGFX", INTEL_VENDOR_ID, DEVICE_ID },
                __uuidof(IDXGIFactory), __uuidof(IDXGIFactory1)
            );

            ASSERT_EQ(
                session.InitEx(
                    mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                ),
                MFX_ERR_NONE
            );

            vp.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            vp.mfx.FrameInfo.Width = 352;
            vp.mfx.FrameInfo.Height = 288;
            vp.mfx.FrameInfo.CropW = 352;
            vp.mfx.FrameInfo.CropH = 288;
            vp.mfx.FrameInfo.FrameRateExtN = 30;
            vp.mfx.FrameInfo.FrameRateExtD = 1;
            vp.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            vp.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            vp.mfx.CodecId = MFX_CODEC_AVC;
            vp.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
            vp.mfx.CodecLevel = MFX_LEVEL_AVC_41;
            vp.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;


            buffer.resize(vp.mfx.FrameInfo.Width * vp.mfx.FrameInfo.Height * 4);
        }

        void EndInitialization()
        {
            auto constexpr type = mocks::mfx::HW_CFL;
            component = mocks::mfx::dx11::make_decoder(type, nullptr, vp);

            ASSERT_EQ(
                MFXVideoDECODE_Init(session, &vp),
                MFX_ERR_NONE
            );
        }
    };

    struct coreHandle
        : public coreHandleBase
        , public testing::WithParamInterface<mfxHandleType>
    {
        void SetUp() override
        {
            coreHandleBase::SetUp();
            if (GetParam() == MFX_HANDLE_DECODER_DRIVER_HANDLE)
            {
                vp.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
                vp.Protected = MFX_PROTECTION_PAVP;
                ext_buffer = { MFX_EXTBUFF_PAVP_OPTION, 64 };
                ext_buffer_ptr = &ext_buffer;
                vp.ExtParam = &ext_buffer_ptr;
                vp.NumExtParam = 1;
            }
            EndInitialization();
        }
    };

    INSTANTIATE_TEST_SUITE_P(
        Handles,
        coreHandle,
        testing::Values(
            //MFX_HANDLE_DECODER_DRIVER_HANDLE,
            MFX_HANDLE_VIDEO_DECODER
        )
    );

    TEST_P(coreHandle, GetHandle)
    {
        EXPECT_EQ(
            MFXVideoCORE_GetHandle(mfxSession(session), GetParam(), &hdl),
            MFX_ERR_NONE
        );
    }

    TEST_P(coreHandle, SetHandle)
    {
        ASSERT_EQ(
            MFXVideoCORE_GetHandle(mfxSession(session), GetParam(), &hdl),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            MFXVideoCORE_SetHandle(mfxSession(session), GetParam(), &hdl),
            MFX_ERR_INVALID_HANDLE
        );
    }

    struct coreHandleMain
        : public coreHandleBase
    {
        void SetUp() override
        {
            coreHandleBase::SetUp();
            EndInitialization();
        }
    };

    TEST_F(coreHandleMain, GetHandleNullptrSession)
    {
        EXPECT_EQ(
            MFXVideoCORE_GetHandle(nullptr, static_cast<mfxHandleType>(MFX_HANDLE_VIDEO_DECODER), &hdl),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_F(coreHandleMain, GetHandleUninitializedSession)
    {
        session.Close();
        EXPECT_EQ(
            MFXVideoCORE_GetHandle(session, static_cast<mfxHandleType>(MFX_HANDLE_VIDEO_DECODER), &hdl),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_F(coreHandleMain, GetHandleNullHandleType)
    {
        EXPECT_EQ(
            MFXVideoCORE_GetHandle(session, static_cast<mfxHandleType>(0), &hdl),
            MFX_ERR_NOT_FOUND
        );
    }

    TEST_F(coreHandleMain, GetHandleUndefinedHandleType)
    {
        EXPECT_EQ(
            MFXVideoCORE_GetHandle(session, static_cast<mfxHandleType>(9), &hdl),
            MFX_ERR_NOT_FOUND
        );
    }

    TEST_F(coreHandleMain, GetHandleNullptrHandle)
    {
        EXPECT_EQ(
            MFXVideoCORE_GetHandle(session, static_cast<mfxHandleType>(MFX_HANDLE_VIDEO_DECODER), nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreHandleMain, SetHandleNullptrSession)
    {
        EXPECT_EQ(
            MFXVideoCORE_SetHandle(nullptr, static_cast<mfxHandleType>(MFX_HANDLE_VIDEO_DECODER), &hdl),
            MFX_ERR_INVALID_HANDLE
        );
    }


    TEST_F(coreHandleMain, SetHandleUninitializedSession)
    {
        session.Close();
        EXPECT_EQ(
            MFXVideoCORE_SetHandle(session, static_cast<mfxHandleType>(MFX_HANDLE_VIDEO_DECODER), &hdl),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_F(coreHandleMain, SetHandleNullHandleType)
    {
        EXPECT_EQ(
            MFXVideoCORE_SetHandle(session, static_cast<mfxHandleType>(0), &hdl),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_F(coreHandleMain, SetHandleUndefinedHandleType)
    {
        EXPECT_EQ(
            MFXVideoCORE_SetHandle(session, static_cast<mfxHandleType>(9), &hdl),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_F(coreHandleMain, SetHandleNullptrHandle)
    {
        EXPECT_EQ(
            MFXVideoCORE_SetHandle(session, static_cast<mfxHandleType>(MFX_HANDLE_VIDEO_DECODER), nullptr),
            MFX_ERR_INVALID_HANDLE
        );
    }
}

#endif // MFX_ENABLE_UNIT_TEST_CORE
