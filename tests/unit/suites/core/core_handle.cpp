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

#if defined(MFX_ENABLE_UNIT_TEST_CORE)

#include "mfxpavp.h"

#if (defined(_WIN32) || defined(_WIN64))
#include "core_base_windows.h"
#elif defined(__linux__)
#include "core_base_linux.h"
#include <dlfcn.h>
#endif

#include "libmfx_core.h"

#undef MFX_PROTECTED_FEATURE_DISABLE

namespace test
{
#if defined(MFX_VA_LINUX)
    inline
    void SetupImplDescription(std::vector<char>& bufferImpl)
    {
        bufferImpl.resize(
            sizeof(mfxImplDescription) +
            sizeof(mfxDecoderDescription::decoder) +
            sizeof(mfxDecoderDescription::decoder::decprofile) +
            sizeof(mfxDecoderDescription::decoder::decprofile::decmemdesc) +
            sizeof(mfxU32) +
            sizeof(mfxEncoderDescription::encoder) +
            sizeof(mfxEncoderDescription::encoder::encprofile) +
            sizeof(mfxEncoderDescription::encoder::encprofile::encmemdesc) +
            sizeof(mfxU32), 0
        );

        auto ptr = bufferImpl.data();
        auto impl = reinterpret_cast<mfxImplDescription*>(ptr);
        ptr += sizeof(mfxImplDescription);
        impl->Dec.Codecs = reinterpret_cast<mfxDecoderDescription::decoder*>(ptr);
        ptr += sizeof(mfxDecoderDescription::decoder);
        impl->Dec.Codecs[0].Profiles = reinterpret_cast<mfxDecoderDescription::decoder::decprofile*>(ptr);
        ptr += sizeof(mfxDecoderDescription::decoder::decprofile);
        impl->Dec.Codecs[0].Profiles[0].MemDesc = reinterpret_cast<mfxDecoderDescription::decoder::decprofile::decmemdesc*>(ptr);
        ptr += sizeof(mfxDecoderDescription::decoder::decprofile::decmemdesc);
        impl->Dec.Codecs[0].Profiles[0].MemDesc[0].ColorFormats = reinterpret_cast<mfxU32*>(ptr);

        ptr += sizeof(mfxU32);
        impl->Enc.Codecs = reinterpret_cast<mfxEncoderDescription::encoder*>(ptr);
        ptr += sizeof(mfxEncoderDescription::encoder);
        impl->Enc.Codecs[0].Profiles = reinterpret_cast<mfxEncoderDescription::encoder::encprofile*>(ptr);
        ptr += sizeof(mfxEncoderDescription::encoder::encprofile);
        impl->Enc.Codecs[0].Profiles[0].MemDesc = reinterpret_cast<mfxEncoderDescription::encoder::encprofile::encmemdesc*>(ptr);
        ptr += sizeof(mfxEncoderDescription::encoder::encprofile::encmemdesc);
        impl->Enc.Codecs[0].Profiles[0].MemDesc[0].ColorFormats = reinterpret_cast<mfxU32*>(ptr);
    }

#endif
    struct coreHandleBase :
        public coreBase
    {
        static mfxImplDescription*                   impl;
        static std::vector<char>                     bufferImpl;

        _mfxSession*                                 session;
        mfxHDL                                       hdl;
        mfxHDL                                       hdlInit;
        mfxHandleType                                hdlType;
        mfxExtBuffer*                                ext_buffer_ptr;
        mfxExtBuffer                                 ext_buffer{};

        static int constexpr                         type = MFX_HW_DG2;

        void SetUp() override
        {
#if defined(MFX_VA_LINUX)
            SetupImplDescription(bufferImpl);
            impl = reinterpret_cast<mfxImplDescription*>(bufferImpl.data());
#elif defined(MFX_VA_WIN)
            ASSERT_NO_THROW({
                registry = mocks::mfx::dispatch_to(INTEL_VENDOR_ID, DEVICE_ID, 1);
            });

            factory = mocks::dxgi::make_factory(
                DXGI_ADAPTER_DESC1{ L"iGFX", INTEL_VENDOR_ID, DEVICE_ID },
                __uuidof(IDXGIFactory), __uuidof(IDXGIFactory1)
            );
#endif
            vp.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            vp.mfx.FrameInfo.Width = 640;
            vp.mfx.FrameInfo.Height = 480;
            vp.mfx.FrameInfo.CropW = 640;
            vp.mfx.FrameInfo.CropH = 480;
            vp.mfx.FrameInfo.FrameRateExtN = 30;
            vp.mfx.FrameInfo.FrameRateExtD = 1;
            vp.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            vp.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            vp.mfx.CodecId = MFX_CODEC_HEVC;
            vp.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
            vp.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

            buffer.resize(vp.mfx.FrameInfo.Width * vp.mfx.FrameInfo.Height * 4);
        }

        void EndInitialization()
        {
#if defined(MFX_VA_WIN)
            auto constexpr type = mocks::mfx::HW_TGL_LP;

            component = mocks::mfx::dx11::make_decoder(type, nullptr, vp);

            mock_context(*(component->context.p),
                std::make_tuple(D3D11_MAP_READ, buffer.data(), vp.mfx.FrameInfo.Width),
                std::make_tuple(D3D11_MAP_WRITE, buffer.data(), vp.mfx.FrameInfo.Width),
                std::make_tuple(D3D11_MAP_READ_WRITE, buffer.data(), vp.mfx.FrameInfo.Width)
            );

            mocks::dx11::mock_device(*(component->device.p),
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED }
            );

            session = new _mfxSession(1);
            session->m_version.Major = 2;
            session->m_version.Minor = 0;

            core.reset(
                FactoryCORE::CreateCORE(MFX_HW_D3D11, 0, 0, (mfxSession)session)
            );

            hdlType = static_cast<mfxHandleType>(MFX_HANDLE_D3D11_DEVICE);
            hdlInit = component->device.p;
#elif defined(MFX_VA_LINUX)
            auto surface_attributes = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger,.value = {.i = VA_FOURCC_NV12} } };
            display = mocks::va::make_display(
                std::make_tuple(0, 0),  // version

                std::make_tuple(
                    VA_RT_FORMAT_YUV420,
                    640, 480,
                    std::make_tuple(surface_attributes),
                    [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
                    { return buffer.data(); }
                )
            );

            core.reset(
                FactoryCORE::CreateCORE(MFX_HW_VAAPI, 0, 0, nullptr)
            );

            hdlType = static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY);
            hdlInit = display.get();
#endif
        }
    };

    mfxImplDescription* coreHandleBase::impl = nullptr;
    std::vector<char> coreHandleBase::bufferImpl;

    struct coreHandle
        : public coreHandleBase
        , public testing::WithParamInterface<std::tuple<mfxU32, mfxStatus, mfxStatus, mfxStatus>>
    {
        void SetUp() override
        {
            coreHandleBase::SetUp();
            if (std::get<0>(GetParam()) == MFX_HANDLE_DECODER_DRIVER_HANDLE)
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

    static std::vector<std::tuple<mfxU32, mfxStatus, mfxStatus, mfxStatus>> Cases = {
        //{ handle type,                                SetHandle,              GetHandle,         SetHandle (repeat)           }
          { 0,                                          MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       },
          { MFX_HANDLE_D3D9_DEVICE_MANAGER,             MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       },
          { MFX_HANDLE_DECODER_DRIVER_HANDLE,           MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       },
#if defined(MFX_VA_WIN)
          { MFX_HANDLE_D3D11_DEVICE,                    MFX_ERR_NONE,           MFX_ERR_NONE,      MFX_ERR_UNDEFINED_BEHAVIOR   },
          { MFX_HANDLE_VA_DISPLAY,                      MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       },
#elif defined(MFX_VA_LINUX)
          { MFX_HANDLE_D3D11_DEVICE,                    MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       },
          { MFX_HANDLE_VA_DISPLAY,                      MFX_ERR_NONE,           MFX_ERR_NONE,      MFX_ERR_UNDEFINED_BEHAVIOR   },
#endif
          { MFX_HANDLE_VIDEO_DECODER,                   MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       },
          { MFX_HANDLE_VA_CONFIG_ID,                    MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       },
          { MFX_HANDLE_VA_CONTEXT_ID,                   MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       },
          { MFX_HANDLE_CM_DEVICE,                       MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       },
          { MFX_HANDLE_HDDLUNITE_WORKLOADCONTEXT,       MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       },
          { 42,                                         MFX_ERR_INVALID_HANDLE, MFX_ERR_NOT_FOUND, MFX_ERR_INVALID_HANDLE       }
    };

    INSTANTIATE_TEST_SUITE_P(
        Handles,
        coreHandle,
        testing::ValuesIn(Cases)
    );

    TEST_P(coreHandle, SetHandle)
    {
        EXPECT_EQ(
            core->SetHandle(static_cast<mfxHandleType>(std::get<0>(GetParam())), hdlInit),
            std::get<1>(GetParam())
        );
    }

    TEST_P(coreHandle, GetHandle)
    {
        ASSERT_EQ(
            core->SetHandle(hdlType, hdlInit),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            core->GetHandle(static_cast<mfxHandleType>(std::get<0>(GetParam())), &hdl),
            std::get<2>(GetParam())
        );
    }

    TEST_P(coreHandle, GetHandleBeforeSetHandle)
    {
        EXPECT_EQ(
            core->GetHandle(static_cast<mfxHandleType>(std::get<0>(GetParam())), &hdl),
            MFX_ERR_NOT_FOUND
        );
    }

    TEST_P(coreHandle, SetHandleRepeat)
    {
        ASSERT_EQ(
            core->SetHandle(hdlType, hdlInit),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            core->SetHandle(static_cast<mfxHandleType>(std::get<0>(GetParam())), &hdl),
            std::get<3>(GetParam())
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

    TEST_F(coreHandleMain, GetHandleNullptrHandle)
    {
        ASSERT_EQ(
            core->SetHandle(hdlType, hdlInit),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            core->GetHandle(hdlType, nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreHandleMain, SetHandleNullptrHandle)
    {
        EXPECT_EQ(
            core->SetHandle(hdlType, nullptr),
            MFX_ERR_INVALID_HANDLE
        );
    }
}

#endif // MFX_ENABLE_UNIT_TEST_CORE
