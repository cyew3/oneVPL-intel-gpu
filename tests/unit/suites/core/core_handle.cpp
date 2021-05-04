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

#define MFX_API_FUNCTION_IMPL(NAME, RTYPE, ARGS_DECL, ARGS) RTYPE NAME ARGS_DECL;

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"

#include "encoding_ddi.h"
#include "mfxpavp.h"

#include <dlfcn.h>

#include "mfxdispatcher.h"

#if (defined(_WIN32) || defined(_WIN64))
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

#elif defined(__linux__)
#include "mocks/include/va/display/display.h"
#include "mocks/include/va/display/surface.h"
#include "mocks/include/va/display/context.h"
#include "libmfx_core_vaapi.h"
#endif

#include <vector>
#include "libmfx_core.h"

#undef MFX_PROTECTED_FEATURE_DISABLE

namespace test
{
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

#if (defined(_WIN32) || defined(_WIN64))

    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID = 42; //The value which is returned both as 'DeviceID' by adapter and by mocked registry.
                                  //Actual value doesn't matter here.
#endif

    struct coreHandleBase :
        public testing::Test
    {
        static mfxImplDescription*                   impl;
        static std::vector<char>                     bufferImpl;
#if (defined(_WIN32) || defined(_WIN64))
        std::unique_ptr<mocks::registry>             registry;
        std::unique_ptr<mocks::dxgi::factory>        factory;
        std::unique_ptr<mocks::mfx::dx11::component> component;
#elif defined(__linux__)
        std::shared_ptr<mocks::va::display>          display;
#endif
        mfxVideoParam                                vp{};
        mfxSession                                   session;
        std::vector<mfxU32>                          buffer;

        mfxHDL                                       hdl{};
        mfxExtBuffer*                                ext_buffer_ptr;
        mfxExtBuffer                                 ext_buffer{};

        void SetUp() override
        {
            SetupImplDescription(bufferImpl);
            impl = reinterpret_cast<mfxImplDescription*>(bufferImpl.data());
#if (defined(_WIN32) || defined(_WIN64))
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
#endif
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
                    (mfxU8 *)"mfxImplDescription.mfxDecoderDescription.decoder.CodecID",
                    impl_value
                ),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                MFXCreateSession(loader, 0, &session),
                MFX_ERR_NONE
            );
        }

        void EndInitialization()
        {
#if (defined(_WIN32) || defined(_WIN64))
            auto constexpr type = mocks::mfx::HW_CFL;
            component = mocks::mfx::dx11::make_decoder(type, nullptr, vp);
#elif defined(__linux__)
            auto surface_attributes = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger,.value = {.i = VA_FOURCC_NV12} } };
            auto f = [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
            { return buffer.data(); };

            constexpr int count = 6;
            VAConfigAttrib attributes[count] = {
                VAConfigAttrib({VAConfigAttribMaxPictureWidth,  1920}),
                VAConfigAttrib({VAConfigAttribMaxPictureHeight, 1080}),
                VAConfigAttrib({VAConfigAttribRTFormat}),
                VAConfigAttrib({VAConfigAttribDecSliceMode, 1}),
                VAConfigAttrib({VAConfigAttribDecProcessing}),
                VAConfigAttrib({VAConfigAttribEncryption})

            };

            display = mocks::va::make_display(
                std::make_tuple(0, 0),  // version

                std::make_tuple(
                    VA_RT_FORMAT_YUV420,
                    352, 288,
                    std::make_tuple(surface_attributes),
                    f
                ),

                std::make_tuple(VAProfileH264High,
                                VAEntrypointVLD,
                                std::make_tuple(attributes, count)
                ),

                std::make_tuple(
                    std::make_tuple(attributes, count),
                    352, 288, 0x1
                )
            );

            ASSERT_EQ(
                MFXVideoCORE_SetHandle(session, MFX_HANDLE_VA_DISPLAY, display.get()),
                MFX_ERR_NONE
            );
#endif
            ASSERT_EQ(
                MFXVideoDECODE_Init(session, &vp),
                MFX_ERR_NONE
            );
        }
    };


    mfxImplDescription* coreHandleBase::impl = nullptr;
    std::vector<char> coreHandleBase::bufferImpl;

#if (defined(_WIN32) || defined(_WIN64))

    struct coreHandle
        : public coreHandleBase
        , public testing::WithParamInterface<mfxHandleType>
    {
        void SetUp() override
        {
            coreHandleBase::SetUp();
            if (GetParam() == static_cast<mfxHandleType>(MFX_HANDLE_DECODER_DRIVER_HANDLE))
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
            // MFX_HANDLE_DECODER_DRIVER_HANDLE,
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

#endif

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
        MFXClose(session);
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
        MFXClose(session);
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

extern "C"
{
    mfxHDL* TestMFXQueryImplsDescription(mfxImplCapsDeliveryFormat /*format*/, mfxU32* num_impls)
    {
        test::coreHandleBase::impl->Dec.NumCodecs = 1;
        test::coreHandleBase::impl->Dec.Codecs[0].CodecID = MFX_CODEC_HEVC;
        test::coreHandleBase::impl->Dec.Codecs[0].NumProfiles = 1;
        test::coreHandleBase::impl->Dec.Codecs[0].Profiles[0].NumMemTypes = 1;
        test::coreHandleBase::impl->Dec.Codecs[0].Profiles[0].MemDesc[0].NumColorFormats = 1;

        test::coreHandleBase::impl->Enc.NumCodecs = 1;
        test::coreHandleBase::impl->Enc.Codecs[0].CodecID = MFX_CODEC_HEVC;
        test::coreHandleBase::impl->Enc.Codecs[0].NumProfiles = 1;
        test::coreHandleBase::impl->Enc.Codecs[0].Profiles[0].NumMemTypes = 1;
        test::coreHandleBase::impl->Enc.Codecs[0].Profiles[0].MemDesc[0].NumColorFormats = 1;
        *num_impls = 1;

        test::coreHandleBase::impl->Impl             = MFX_IMPL_TYPE_HARDWARE;
        test::coreHandleBase::impl->ApiVersion       = { MFX_VERSION_MINOR, MFX_VERSION_MAJOR };
        test::coreHandleBase::impl->VendorID         = 0x8086;
        test::coreHandleBase::impl->AccelerationMode = MFX_ACCEL_MODE_VIA_VAAPI;
        return reinterpret_cast<mfxHDL*>(&test::coreHandleBase::impl);
    }

    extern void* _dl_sym(void*, const char*, ...);

    // MFXQueryImplsDescription is interface function between dispatcher and runtime and thereof it is not
    // exported by dispatcher and invoked internally by its pointer (dlopen + dlsym). So, we need to hook
    // dlsym to return pointer to our TestMFXQueryImplsDescription.
    void* dlsym(void * handle, const char * name)
    {
        static void* (*libc_dlsym)(void * handle, const char * name);
        if (libc_dlsym == NULL) {
            libc_dlsym = (void* (*)(void * handle, const char * name))_dl_sym(RTLD_NEXT, "dlsym", dlsym);
        }
        if(!strcmp(name, "MFXQueryImplsDescription"))
            return (void*)(TestMFXQueryImplsDescription);
        return libc_dlsym(handle, name);
    }
}


#endif // MFX_ENABLE_UNIT_TEST_CORE
