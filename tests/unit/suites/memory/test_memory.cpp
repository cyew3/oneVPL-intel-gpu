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

#if defined (MFX_ENABLE_UNIT_TEST_MEMORY)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"

#define MFX_ENABLE_H265_VIDEO_ENCODE

#include <dlfcn.h>

#include "mfxdispatcher.h"

#include "mocks/include/va/display/display.h"
#include "mocks/include/va/display/surface.h"
#include "mocks/include/va/display/context.h"
#include "mocks/include/va/ioctl.h"

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

    struct Memory
        : public ::testing::TestWithParam<std::pair<std::pair<VAProfile /*profile*/, VAEntrypoint /*entrypoint*/>, mfxU16 /*IO Pattern*/> >
    {
        static mfxImplDescription*                   impl;
        static std::vector<char>                     bufferImpl;

        mfxVideoParam                                vp{};
        mfxSession                                   session;
        mfxFrameSurface1*                            surface = nullptr;

        std::unique_ptr<mfxFrameAllocator>           allocator;
        std::vector<mfxU32>                          buffer;
        std::shared_ptr<mocks::va::display>          display;
        std::vector<std::vector<char>>               buffers;

        void SetUp() override
        {
            SetupImplDescription(bufferImpl);
            impl = reinterpret_cast<mfxImplDescription*>(bufferImpl.data());

            auto profile    = GetParam().first.first;
            auto entrypoint = GetParam().first.second;

            vp.mfx.FrameInfo.Width        = 640;
            vp.mfx.FrameInfo.Height       = 480;
            vp.mfx.CodecId                = MFX_CODEC_HEVC;
            vp.mfx.FrameInfo.BitDepthLuma = 8;
            vp.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            vp.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
            vp.mfx.CodecProfile           = MFX_PROFILE_HEVC_MAIN;
            vp.IOPattern                  = GetParam().second;

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
                    entrypoint == VAEntrypointVLD?
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

            buffer.resize(vp.mfx.FrameInfo.Width * vp.mfx.FrameInfo.Height);
            auto surface_attributes = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger,.value = {.i = VA_FOURCC_NV12} } };
            display = mocks::va::make_display(
                    std::make_tuple(0, 0),  // version

                    std::make_tuple(
                        VA_RT_FORMAT_YUV420,
                        640, 480,
                        std::make_tuple(surface_attributes),
                        // functor invoked when 'vaDeriveImage' is called
                        [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
                        { return buffer.data(); }
                    )
            );

            std::vector<VAConfigAttrib> attributes;
            if(entrypoint == VAEntrypointVLD)
            {
                attributes.insert(attributes.end(), {
                    VAConfigAttrib({VAConfigAttribMaxPictureWidth,  1920})
                    , VAConfigAttrib({VAConfigAttribMaxPictureHeight, 1080})
                    , VAConfigAttrib({VAConfigAttribRTFormat})
                    , VAConfigAttrib({VAConfigAttribDecSliceMode, 1})
                    , VAConfigAttrib({VAConfigAttribDecProcessing})
                    , VAConfigAttrib({VAConfigAttribEncryption})
                });
            }
            else if(entrypoint == VAEntrypointEncSlice)
            {
                attributes.insert(attributes.end(), {
                    VAConfigAttrib({VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420})
                    , VAConfigAttrib({VAConfigAttribRateControl, uint32_t(VA_RC_CQP)})
                    , VAConfigAttrib({VAConfigAttribEncIntraRefresh})
                    , VAConfigAttrib({VAConfigAttribMaxPictureWidth, 1920})
                    , VAConfigAttrib({VAConfigAttribMaxPictureHeight, 1080})
                    , VAConfigAttrib({VAConfigAttribEncQuantization})
                    , VAConfigAttrib({VAConfigAttribEncParallelRateControl})
                    , VAConfigAttrib({VAConfigAttribEncMaxRefFrames, (1<<16)+3})
                    , VAConfigAttrib({VAConfigAttribEncSliceStructure})
                    , VAConfigAttrib({VAConfigAttribEncROI})
                    , VAConfigAttrib({VAConfigAttribEncTileSupport})
                    , VAConfigAttrib({VAConfigAttribEncDirtyRect})
                    , VAConfigAttrib({VAConfigAttribMaxFrameSize})
                });
            }

            mocks::va::mock_display(*display.get(),
                std::make_tuple(profile,
                    entrypoint,
                    std::make_tuple(attributes.data(), attributes.size())
                ),

                std::make_tuple(
                    std::make_tuple(attributes.data(), attributes.size()),
                    640, 480, 0x1
                )
            );

            buffers.resize(5);
            buffers[0].resize(960000);
            buffers[1].resize(28);
            buffers[2].resize(24);
            buffers[3].resize(228);
            buffers[4].resize(64);

            mocks::va::make_context(display.get(),
                std::make_tuple(VAContextID(2), VAConfigID(0)),
                // for vaCreateBuffer
                std::make_tuple(
                    VAEncCodedBufferType,
                    960000,
                    1,
                    buffers[0].data()
                ),

                // for vaCreateBuffer and vaMapBuffer
                std::make_tuple(
                    VAEncMiscParameterBufferType,
                    28,
                    1,
                    buffers[1].data()
                ),
                // for vaCreateBuffer and vaMapBuffer
                std::make_tuple(
                    VAEncMiscParameterBufferType,
                    24,
                    1,
                    buffers[2].data()
                ),
                // for vaCreateBuffer and vaMapBuffer
                std::make_tuple(
                    VAEncMiscParameterBufferType,
                    228,
                    1,
                    buffers[3].data()
                ),
                // for vaCreateBuffer and vaMapBuffer
                std::make_tuple(
                    VAEncMiscParameterBufferType,
                    64,
                    1,
                    buffers[4].data()
                )
            );

            ASSERT_EQ(
                MFXVideoCORE_SetHandle(session, MFX_HANDLE_VA_DISPLAY, display.get()),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                entrypoint == VAEntrypointVLD ?
                MFXVideoDECODE_Init(session, &vp) :
                entrypoint == VAEntrypointEncSlice ?
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
            auto entrypoint = GetParam().first.second;
            return
                entrypoint == VAEntrypointVLD ?
                MFXMemory_GetSurfaceForDecode(session, surface) :
                entrypoint == VAEntrypointEncSlice ?
                MFXMemory_GetSurfaceForEncode(session, surface) :
                MFXMemory_GetSurfaceForVPP(session, surface);
        }
    };

    mfxImplDescription* Memory::impl = nullptr;
    std::vector<char> Memory::bufferImpl;
    INSTANTIATE_TEST_SUITE_P(
        Decode,
        Memory,
        testing::Values(
            std::make_pair(std::make_pair(VAProfileHEVCMain, VAEntrypointVLD), mfxU16(MFX_IOPATTERN_OUT_SYSTEM_MEMORY)),
            std::make_pair(std::make_pair(VAProfileHEVCMain, VAEntrypointVLD), mfxU16(MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        )
    );

    INSTANTIATE_TEST_SUITE_P(
        Encode,
        Memory,
        testing::Values(
            std::make_pair(std::make_pair(VAProfileHEVCMain, VAEntrypointEncSlice), mfxU16(MFX_IOPATTERN_IN_SYSTEM_MEMORY)),
            std::make_pair(std::make_pair(VAProfileHEVCMain, VAEntrypointEncSlice), mfxU16(MFX_IOPATTERN_IN_VIDEO_MEMORY))
        )
    );

    TEST_P(Memory, GetSurfaceNullSession)
    {
        ASSERT_EQ(
            MFXClose(session),
            MFX_ERR_NONE
        );

        mfxFrameSurface1* s = nullptr;
        EXPECT_EQ(
            GetSurface(&s),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_P(Memory, GetSurfaceComponentNotInitialized)
    {
        auto id = GetParam().first;
        auto entrypoint = GetParam().first.second;
        ASSERT_EQ(
            entrypoint == VAEntrypointVLD ?
            MFXVideoDECODE_Close(session) :
            entrypoint == VAEntrypointEncSlice ?
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
                type, MFX_RESOURCE_VA_SURFACE
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
                type, MFX_HANDLE_VA_DISPLAY
            );

            EXPECT_EQ(
                hdl, display.get()
            );
        }
    }
}

extern "C"
{
    mfxHDL* TestMFXQueryImplsDescription(mfxImplCapsDeliveryFormat /*format*/, mfxU32* num_impls)
    {
        test::Memory::impl->Dec.NumCodecs = 1;
        test::Memory::impl->Dec.Codecs[0].CodecID = MFX_CODEC_HEVC;
        test::Memory::impl->Dec.Codecs[0].NumProfiles = 1;
        test::Memory::impl->Dec.Codecs[0].Profiles[0].NumMemTypes = 1;
        test::Memory::impl->Dec.Codecs[0].Profiles[0].MemDesc[0].NumColorFormats = 1;

        test::Memory::impl->Enc.NumCodecs = 1;
        test::Memory::impl->Enc.Codecs[0].CodecID = MFX_CODEC_HEVC;
        test::Memory::impl->Enc.Codecs[0].NumProfiles = 1;
        test::Memory::impl->Enc.Codecs[0].Profiles[0].NumMemTypes = 1;
        test::Memory::impl->Enc.Codecs[0].Profiles[0].MemDesc[0].NumColorFormats = 1;
        *num_impls = 1;

        test::Memory::impl->Impl             = MFX_IMPL_TYPE_HARDWARE;
        test::Memory::impl->ApiVersion       = { MFX_VERSION_MINOR, MFX_VERSION_MAJOR };
        test::Memory::impl->VendorID         = 0x8086;
        test::Memory::impl->AccelerationMode = MFX_ACCEL_MODE_VIA_VAAPI;
        return reinterpret_cast<mfxHDL*>(&test::Memory::impl);
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

#endif // MFX_ENABLE_UNIT_TEST_MEMORY
