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

#if defined(MFX_ONEVPL)
#if defined (MFX_ENABLE_UNIT_TEST_ALLOCATOR)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
#include "mfx_session.h"

#include "encoding_ddi.h"
#include "libmfx_allocator_d3d11.h"

#define ENABLE_MFX_INTEL_GUID_DECODE
#define ENABLE_MFX_INTEL_GUID_ENCODE
#define ENABLE_MFX_INTEL_GUID_PRIVATE

#include "mocks/include/dxgi/format.h"
#include "mocks/include/dxgi/device/factory.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/decoder.h"
#include "mocks/include/mfx/dx11/encoder.h"

#include "mocks/include/dx9/device/d3d.h"

#include <future>

namespace test
{
    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID = 42; //The value which is returned both as 'DeviceID' by adapter and by mocked registry.
                                  //Actual value doesn't matter here.

    struct Memory
        : public ::testing::TestWithParam<std::pair<GUID, mfxU16 /*IO Pattern*/> >
    {
        std::unique_ptr<mocks::registry>             registry;
        std::unique_ptr<mocks::dxgi::factory>        factory;
        std::unique_ptr<mocks::mfx::dx11::component> component;

        mfxVideoParam                                vp{};
        MFXVideoSession                              session;
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

            auto constexpr type = mocks::mfx::HW_SCL;
            auto id = GetParam().first;
            if (mocks::dxva::is_decoder(id))
                component = mocks::mfx::dx11::make_decoder(type, nullptr, vp);
            else if (mocks::dxva::is_encoder(id))
                component = mocks::mfx::dx11::make_encoder(type, nullptr, vp);
            else
                component.reset(new mocks::mfx::dx11::component(type, nullptr, vp));

            ASSERT_EQ(
                session.InitEx(
                    mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                ),
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

    TEST_P(Memory, GetSurfaceNullSession)
    {
        session.Close();

        mfxFrameSurface1* s = nullptr;
        EXPECT_EQ(
            GetSurface(&s),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_P(Memory, GetSurfaceComponentNotInitialized)
    {
        auto id = GetParam().first;

        ASSERT_EQ(
            mocks::dxva::is_decoder(id) ?
            MFXVideoDECODE_Close(session) :
            mocks::dxva::is_encoder(id) ?
            MFXVideoENCODE_Close(session) :
            MFXVideoVPP_Close(session),
            MFX_ERR_NONE
        );

        mfxFrameSurface1* s = nullptr;
        EXPECT_EQ(
            GetSurface(&s),
            MFX_ERR_NOT_INITIALIZED
        );
    }

    TEST_P(Memory, MapSurfaceNullSurface)
    {
        EXPECT_EQ(
            surface->FrameInterface->Map(nullptr, MFX_MAP_READ),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, MapSurfaceRead)
    {
        EXPECT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ),
            MFX_ERR_NONE
        );
        EXPECT_NE(
            surface->Data.UV,
            nullptr
        );
    }

    TEST_P(Memory, MapSurfaceWrite)
    {
        EXPECT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_WRITE),
            MFX_ERR_NONE
        );

        EXPECT_NE(
            surface->Data.UV,
            nullptr
        );
    }

    TEST_P(Memory, MapSurfaceReadWrite)
    {
        EXPECT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ_WRITE),
            MFX_ERR_NONE
        );

        EXPECT_NE(
            surface->Data.UV,
            nullptr
        );
    }

    TEST_P(Memory, MapSurfaceWrongFlags)
    {
        EXPECT_EQ(
            surface->FrameInterface->Map(surface, 0x42),
            MFX_ERR_LOCK_MEMORY
        );
    }

    TEST_P(Memory, MapReadReadSameThread)
    {
        ASSERT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ),
            MFX_ERR_NONE
        );
    }

    TEST_P(Memory, UnmapSurfaceNullSurface)
    {
        EXPECT_EQ(
            surface->FrameInterface->Unmap(nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, UnmapSurfaceNotMapped)
    {
        EXPECT_EQ(
            surface->FrameInterface->Unmap(surface),
            MFX_ERR_UNDEFINED_BEHAVIOR
        );
    }

    TEST_P(Memory, UnmapSurface)
    {
        ASSERT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            surface->FrameInterface->Unmap(surface),
            MFX_ERR_NONE
        );
        EXPECT_FALSE(
            surface->Data.Y ||
            surface->Data.U ||
            surface->Data.V ||
            surface->Data.A
        );
    }

    TEST_P(Memory, ReleaseSurfaceNullSurface)
    {
        EXPECT_EQ(
            surface->FrameInterface->Release(nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, ReleaseMappedSurface)
    {
        ASSERT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            surface->FrameInterface->Release(surface),
            MFX_ERR_NONE
        );
    }

    TEST_P(Memory, ReleaseSurface)
    {
        EXPECT_EQ(
            surface->FrameInterface->Release(surface),
            MFX_ERR_NONE
        );
    }

    TEST_P(Memory, GetNativeHandleNullSurface)
    {
        mfxHDL hdl;
        mfxResourceType type;
        EXPECT_EQ(
            surface->FrameInterface->GetNativeHandle(nullptr, &hdl, &type),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetNativeHandleNullResource)
    {
        mfxResourceType type;
        EXPECT_EQ(
            surface->FrameInterface->GetNativeHandle(surface, nullptr, &type),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetNativeHandleNullType)
    {
        mfxResourceType type;
        mfxHDL hdl;
        EXPECT_EQ(
            surface->FrameInterface->GetNativeHandle(surface, &hdl, nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetNativeHandle)
    {
        mfxHDL hdl;
        mfxResourceType type;
        if (GetParam().second & (MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
            EXPECT_EQ(
                surface->FrameInterface->GetNativeHandle(surface, &hdl, &type),
                MFX_ERR_UNSUPPORTED
            );
        else
        {
            EXPECT_EQ(
                surface->FrameInterface->GetNativeHandle(surface, &hdl, &type),
                MFX_ERR_NONE
            );
            EXPECT_EQ(
                type, MFX_RESOURCE_DX11_TEXTURE
            );
        }
    }

    TEST_P(Memory, GetDeviceHandleNullSurface)
    {
        mfxHDL hdl;
        mfxHandleType type;
        EXPECT_EQ(
            surface->FrameInterface->GetDeviceHandle(nullptr, &hdl, &type),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetDeviceHandleNullDevice)
    {
        mfxHandleType type;
        EXPECT_EQ(
            surface->FrameInterface->GetDeviceHandle(surface, nullptr, &type),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetDeviceHandleNullType)
    {
        mfxResourceType type;
        mfxHDL hdl;
        EXPECT_EQ(
            surface->FrameInterface->GetDeviceHandle(surface, &hdl, nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetDeviceHandle)
    {
        mfxHDL hdl;
        mfxHandleType type;
        if (GetParam().second & (MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
            EXPECT_EQ(
                surface->FrameInterface->GetDeviceHandle(surface, &hdl, &type),
                MFX_ERR_UNSUPPORTED
            );
        else
        {
            EXPECT_EQ(
                surface->FrameInterface->GetDeviceHandle(surface, &hdl, &type),
                MFX_ERR_NONE
            );
            EXPECT_EQ(
                type, MFX_HANDLE_D3D11_DEVICE
            );
            EXPECT_EQ(
                hdl, component->device.p
            );
        }
    }
}

#endif // MFX_ENABLE_UNIT_TEST_ALLOCATOR
#endif // MFX_ONEVPL
