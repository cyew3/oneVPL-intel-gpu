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

#define ENABLE_MFX_INTEL_GUID_DECODE
#define ENABLE_MFX_INTEL_GUID_ENCODE
#define ENABLE_MFX_INTEL_GUID_PRIVATE

#include "mocks/include/dxgi/format.h"
#include "mocks/include/dxgi/device/factory.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/decoder.h"
#include "mocks/include/mfx/dx11/encoder.h"

#include "mocks/include/dx9/device/d3d.h"


namespace test
{
    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID = 42; //The value which is returned both as 'DeviceID' by adapter and by mocked registry.
                                  //Actual value doesn't matter here.

    inline DXGI_FORMAT GetDXGI(int fourcc)
    {
        switch (fourcc)
        {
        case MFX_FOURCC_YV12: return DXGI_FORMAT_420_OPAQUE;
        default: return mocks::dxgi::to_native(fourcc);
        }
    }

    inline int GetPitch(int fourcc, int width, int height)
    {
        switch (fourcc)
        {
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_YV12: return width;
        case MFX_FOURCC_YUY2: return width * 2;
        case MFX_FOURCC_P8:   return width * height;
        default:              return width;
        }
    }

    struct coreBase :
        public testing::Test
    {
        std::unique_ptr<mocks::registry>             registry;
        std::unique_ptr<mocks::dxgi::factory>        factory;
        std::unique_ptr<mocks::dx11::context>        context;
        std::unique_ptr<mocks::mfx::dx11::component> component;

        mfxVideoParam                                vp{};
        MFXVideoSession                              session;
        _mfxSession*                                 s;

        mfxFrameSurface1*                            src = nullptr;
        mfxFrameSurface1*                            dst = nullptr;

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

            vp.mfx.FrameInfo.Width = 640;
            vp.mfx.FrameInfo.Height = 480;
            ///TEMP: need to infer profile from given GUID
            vp.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;

            vp = mocks::mfx::make_param(
                MFX_FOURCC_NV12,
                mocks::mfx::make_param(DXVA_ModeHEVC_VLD_Main, vp)
            );

            ASSERT_EQ(
                session.InitEx(
                    mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                ),
                MFX_ERR_NONE
            );
            s = static_cast<_mfxSession*>(session);

            buffer.resize(vp.mfx.FrameInfo.Width * vp.mfx.FrameInfo.Height * 4);
        }

        void EndInitialization(mfxU32 fourcc, int IOPattern)
        {

            int pitch = GetPitch(fourcc, vp.mfx.FrameInfo.Width, vp.mfx.FrameInfo.Height);
            mocks::dx11::mock_context(*(component->context.p),
                std::make_tuple(D3D11_MAP_READ, buffer.data(), pitch),
                std::make_tuple(D3D11_MAP_WRITE, buffer.data(), pitch),
                std::make_tuple(D3D11_MAP_READ_WRITE, buffer.data(), pitch)
            );

            mocks::dx11::mock_device(*(component->device.p), component->context.p,
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, GetDXGI(fourcc), {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, GetDXGI(fourcc), {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, GetDXGI(fourcc), {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, GetDXGI(fourcc), {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, GetDXGI(fourcc), {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, D3D11_RESOURCE_MISC_SHARED },
                D3D11_BUFFER_DESC{ UINT(vp.mfx.FrameInfo.Width) * UINT(vp.mfx.FrameInfo.Height * 2), D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0u, 0u }
            );

            if (IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            {
                ASSERT_EQ(
                    MFXVideoDECODE_Init(session, &vp),
                    MFX_ERR_NONE
                );
                ASSERT_EQ(
                    MFXMemory_GetSurfaceForDecode(session, &src),
                    MFX_ERR_NONE
                );
                ASSERT_EQ(
                    MFXMemory_GetSurfaceForDecode(session, &dst),
                    MFX_ERR_NONE
                );
            }
            else
            {
                ASSERT_EQ(
                    MFXVideoENCODE_Init(session, &vp),
                    MFX_ERR_NONE
                );
                ASSERT_EQ(
                    MFXMemory_GetSurfaceForEncode(session, &src),
                    MFX_ERR_NONE
                );
                ASSERT_EQ(
                    MFXMemory_GetSurfaceForEncode(session, &dst),
                    MFX_ERR_NONE
                );
            }
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

    struct coreMain :
        public coreBase
    {
        void SetUp() override
        {
            coreBase::SetUp();
            vp.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            auto constexpr type = mocks::mfx::HW_SCL;
            component = mocks::mfx::dx11::make_decoder(type, nullptr, vp);
            EndInitialization(MFX_FOURCC_NV12, MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
        }
    };
}

#endif // MFX_ENABLE_UNIT_TEST_CORE
