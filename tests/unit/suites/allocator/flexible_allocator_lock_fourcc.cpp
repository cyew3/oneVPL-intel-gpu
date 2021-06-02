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

#if defined(MFX_ONEVPL)
#if defined (MFX_ENABLE_UNIT_TEST_ALLOCATOR)

#include "flexible_allocator_base.h"

#if defined(MFX_VA_LINUX)
#include "mocks/include/va/format.h"
#include "mocks/include/mfx/platform.h"
#include "mocks/include/va/display/context.h"
#include "mocks/include/va/context/context.h"
#endif

#include <tuple>
#include <vector>
#include <algorithm>

namespace test
{
    int constexpr WIDTH  = 640;
    int constexpr HEIGHT = 480;
#if defined(MFX_VA_WIN)
    inline DXGI_FORMAT get_dxgi(mfxU32 FourCC)
    {
        switch (FourCC)
        {
        case MFX_FOURCC_IMC3:
        case MFX_FOURCC_YUV400:
        case MFX_FOURCC_YUV411:
        case MFX_FOURCC_YUV422H:
        case MFX_FOURCC_YUV422V:
        case MFX_FOURCC_YUV444:
        case MFX_FOURCC_RGBP:
            return DXGI_FORMAT_420_OPAQUE;
        case MFX_FOURCC_R16_BGGR:
        case MFX_FOURCC_R16_RGGB:
        case MFX_FOURCC_R16_GRBG:
        case MFX_FOURCC_R16_GBRG:
            return DXGI_FORMAT_R16_TYPELESS;
        default:
            return mocks::dxgi::to_native(FourCC);
        }
    }

    inline mfxU32 get_bind_flags(mfxU32 FourCC)
    {
        switch (FourCC)
        {
        case MFX_FOURCC_RGBP:       return D3D11_BIND_RENDER_TARGET;
        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_BGR4:
        case MFX_FOURCC_A2RGB10:    return D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        case MFX_FOURCC_NV12:
        case MFX_FOURCC_P010:
        case MFX_FOURCC_YUY2:
        case MFX_FOURCC_AYUV:
        case MFX_FOURCC_Y210:
        case MFX_FOURCC_Y410:
        case MFX_FOURCC_P016:
        case MFX_FOURCC_Y216:
        case MFX_FOURCC_Y416:
        case MFX_FOURCC_ARGB16:
        case MFX_FOURCC_ABGR16:     return D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
        case MFX_FOURCC_IMC3:
        case MFX_FOURCC_YUV400:
        case MFX_FOURCC_YUV411:
        case MFX_FOURCC_YUV422H:
        case MFX_FOURCC_YUV422V:
        case MFX_FOURCC_YUV444:     return D3D11_BIND_DECODER;

        case MFX_FOURCC_P8:
        case MFX_FOURCC_P8_TEXTURE:
        default:                    return 0;
        }
    }

#elif defined(MFX_VA_LINUX)
    enum {
        MFX_FOURCC_VP8_NV12   = MFX_MAKEFOURCC('V', 'P', '8', 'N'),
        MFX_FOURCC_VP8_MBDATA = MFX_MAKEFOURCC('V', 'P', '8', 'M'),
        MFX_FOURCC_VP8_SEGMAP = MFX_MAKEFOURCC('V', 'P', '8', 'S'),
    };

    inline int32_t mfx_to_va(mfxU32 FourCC)
    {
        switch (FourCC)
        {
        case MFX_FOURCC_VP8_NV12:
            return VA_FOURCC_NV12;
        case MFX_FOURCC_VP8_MBDATA:
        case MFX_FOURCC_VP8_SEGMAP:
            return VA_FOURCC_P208;
        default:
            return mocks::va::to_native(FourCC);
        }
    }

    inline mfxU32 get_format(mfxU32 FourCC)
    {
        switch (FourCC)
        {
        case MFX_FOURCC_VP8_NV12:
            return VA_FOURCC_NV12;
        case MFX_FOURCC_VP8_MBDATA:
            return VA_FOURCC_P208;
        case MFX_FOURCC_VP8_SEGMAP:
            return VA_FOURCC_P208;
        default:
            return mocks::va::to_native_rt(FourCC);
        }
    }
#endif

    struct FlexibleAllocatorFourCCBase : public FlexibleAllocatorBase,
        testing::WithParamInterface < std::tuple < mfxU32,
        std::tuple<mfxU32, mfxStatus, mfxU32, mfxU8* mfxFrameData::*, mfxU8* mfxFrameData::*, int, mfxU8* mfxFrameData::*, int, mfxU8* mfxFrameData::*, int>>>
    {
        mfxU32          platform;
        mfxU32          type;
        mfxU32          fourcc;
        mfxU32          pitch;

        void SetUp() override
        {
            FlexibleAllocatorBase::SetUp();
            platform = std::get<0>(GetParam());
            type = (platform == 0) ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
            auto infoValue = std::get<1>(GetParam());
            fourcc = std::get<0>(infoValue);
            pitch = std::get<2>(infoValue);

            req.Type = static_cast<mfxU16>(type);
            if (fourcc == MFX_FOURCC_P8_TEXTURE)
            {
                req.Type |= MFX_MEMTYPE_FROM_ENCODE;
            }
            else if (fourcc == MFX_FOURCC_RGBP || fourcc == MFX_FOURCC_RGB3)
            {
                req.Type |= MFX_MEMTYPE_FROM_VPPOUT;
            }
            else if (fourcc == MFX_FOURCC_IMC3)
            {
                req.Type |= MFX_MEMTYPE_FROM_DECODE;
            }

            req.Info.Width = WIDTH;
            req.Info.Height = HEIGHT;
            req.Info.FourCC = fourcc;
        }
    };

#if defined(MFX_VA_WIN)
    struct FlexibleAllocatorFourCC : public FlexibleAllocatorFourCCBase
    {
        void SetUp() override
        {
            FlexibleAllocatorFourCCBase::SetUp();
            auto bindFlags = get_bind_flags(fourcc);

            // for ID3D11DeviceContext::Map (buffer.data() will be returned as a result)
            mocks::dx11::mock_context(*context.get(),
                std::make_tuple(D3D11_MAP_READ_WRITE, buffer.data(), pitch)
            );

            vp.mfx.FrameInfo.Width = WIDTH;
            vp.mfx.FrameInfo.Height = HEIGHT;
            vp.mfx.FrameInfo.FourCC = fourcc;

            if (platform > 0)
                req.Type |= MFX_MEMTYPE_SHARED_RESOURCE;

            component.reset(
                new mocks::mfx::dx11::component(mocks::mfx::HW_TGL_LP, nullptr, vp)
            );
            //for D3D11Device::CreateTexture2D
            mocks::dx11::mock_device(*(component->device.p), context.get(),
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, get_dxgi(fourcc), {1, 0}, D3D11_USAGE_DEFAULT, bindFlags, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, get_dxgi(fourcc), {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_SHARED }
            );
            if (fourcc == MFX_FOURCC_P8)
            {
                req.Type |= MFX_MEMTYPE_FROM_ENCODE;

                // for ID3D11Device::CreateBuffer
                mocks::dx11::mock_device(*(component->device.p),
                    D3D11_BUFFER_DESC{ mfxU32(req.Info.Width * req.Info.Height), D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0u, 0u }
                );
            }
            else if (fourcc == MFX_FOURCC_R16_RGGB || fourcc == MFX_FOURCC_R16_BGGR || fourcc == MFX_FOURCC_R16_GBRG || fourcc == MFX_FOURCC_R16_GRBG)
            {
                //for D3D11Device::CreateTexture2D
                mocks::dx11::mock_device(*(component->device.p),
                    D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, get_dxgi(fourcc), {1, 0}, D3D11_USAGE_DEFAULT, bindFlags, 0, 0 },
                    D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, get_dxgi(fourcc), {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0 },
                    D3D11_BUFFER_DESC{ mfxU32(0x58), D3D11_USAGE_STAGING, 0u, D3D11_CPU_ACCESS_READ, 0u, 0u }
                );
            }
            SetAllocator(static_cast<mfxU16>(type));
        }
    };

#elif defined(MFX_VA_LINUX)
    struct FlexibleAllocatorFourCC : public FlexibleAllocatorFourCCBase
    {
        void SetUp() override
        {
            FlexibleAllocatorFourCCBase::SetUp();
            auto format = get_format(fourcc);
            auto f = [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
            { return buffer.data(); };

            std::vector<VASurfaceAttrib> surface_attributes;
            surface_attributes.push_back(VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger,.value = {.i = mfx_to_va(fourcc)} } });
            if (fourcc == MFX_FOURCC_VP8_NV12)
            {
                surface_attributes.push_back(VASurfaceAttrib{ VASurfaceAttribUsageHint, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger,.value = {.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER } } });
            }

            // for vaCreateSurfaces
            mocks::va::mock_display(*display.get(),
                std::make_tuple(
                    format,
                    WIDTH, HEIGHT,
                    std::make_tuple(surface_attributes.data(), surface_attributes.size()),
                    f
                )
            );

            if (fourcc == MFX_FOURCC_P8)
            {
                req.Type |= MFX_MEMTYPE_FROM_ENCODE;
                // for vaCreateBuffer
                buffer.resize((WIDTH * HEIGHT * 400) / 256);
                mocks::va::make_context(display.get(),
                    std::make_tuple(VAContextID(req.AllocId), VAConfigID(0)),
                    std::make_tuple(
                        VAEncCodedBufferType,
                        (WIDTH * HEIGHT * 400) / 256,
                        1,
                        buffer.data()
                    )
                );
            }
            else if (fourcc == MFX_FOURCC_VP8_SEGMAP)
            {
                // for vaCreateBuffer
                mocks::va::make_context(display.get(),
                    std::make_tuple(VAContextID(req.AllocId), VAConfigID(0)),
                    std::make_tuple(
                        VAEncMacroblockMapBufferType,
                        WIDTH,
                        HEIGHT,
                        buffer.data()
                    )
                );
            }
            SetAllocator(static_cast<mfxU16>(type));
        }
    };
#endif

    static std::vector<std::tuple<
        mfxU32,                      // FourCC,
        mfxStatus,                   // return status for Lock
        mfxU32,                      // pitch,
        mfxU8* mfxFrameData::*,      // ptr1
        mfxU8* mfxFrameData::*, int, // ptr2, offset between ptr2 and ptr1 (in bytes)
        mfxU8* mfxFrameData::*, int, // ptr3, offset between ptr3 and ptr1 (in bytes)
        mfxU8* mfxFrameData::*, int  // ptr4, offset between ptr4 and ptr1 (in bytes)
        >> FourCC_common = {
          { MFX_FOURCC_NV12,    MFX_ERR_NONE, WIDTH,     &mfxFrameData::Y, &mfxFrameData::U, WIDTH * HEIGHT,     &mfxFrameData::V, WIDTH * HEIGHT + 1,     nullptr,          0 } // 0
        , { MFX_FOURCC_P010,    MFX_ERR_NONE, WIDTH * 2, &mfxFrameData::Y, &mfxFrameData::U, WIDTH * HEIGHT * 2, &mfxFrameData::V, WIDTH * HEIGHT * 2 + 2, nullptr,          0 } // 1
        , { MFX_FOURCC_YUY2,    MFX_ERR_NONE, WIDTH * 2, &mfxFrameData::Y, &mfxFrameData::U, 1,                  &mfxFrameData::V, 3,                      nullptr,          0 } // 2
        , { MFX_FOURCC_RGB4,    MFX_ERR_NONE, WIDTH * 4, &mfxFrameData::B, &mfxFrameData::G, 1,                  &mfxFrameData::R, 2,                      &mfxFrameData::A, 3 } // 3
        , { MFX_FOURCC_BGR4,    MFX_ERR_NONE, WIDTH * 4, &mfxFrameData::R, &mfxFrameData::G, 1,                  &mfxFrameData::B, 2,                      &mfxFrameData::A, 3 } // 4
        , { MFX_FOURCC_AYUV,    MFX_ERR_NONE, WIDTH * 4, &mfxFrameData::V, &mfxFrameData::U, 1,                  &mfxFrameData::Y, 2,                      &mfxFrameData::A, 3 } // 5
        , { MFX_FOURCC_A2RGB10, MFX_ERR_NONE, WIDTH * 4, &mfxFrameData::R, &mfxFrameData::G, 0,                  &mfxFrameData::B, 0,                      &mfxFrameData::A, 0 } // 6
    #if (MFX_VERSION >= 1027)
        , { MFX_FOURCC_Y210,    MFX_ERR_NONE, WIDTH * 4, &mfxFrameData::Y, &mfxFrameData::U, 2,                  &mfxFrameData::V, 6,                      nullptr,          0 } // 7
        , { MFX_FOURCC_Y410,    MFX_ERR_NONE, WIDTH * 4, &mfxFrameData::U, nullptr,          0,                  nullptr,          0,                      nullptr,          0 } // 8
    #endif
    #if (MFX_VERSION >= 1031)
        , { MFX_FOURCC_P016,    MFX_ERR_NONE, WIDTH * 2, &mfxFrameData::Y, &mfxFrameData::U, WIDTH * HEIGHT * 2, &mfxFrameData::V, WIDTH * HEIGHT * 2 + 2, nullptr,          0 } // 9
        , { MFX_FOURCC_Y216,    MFX_ERR_NONE, WIDTH * 4, &mfxFrameData::Y, &mfxFrameData::U, 2,                  &mfxFrameData::V, 6,                      nullptr,          0 } // 10
        , { MFX_FOURCC_Y416,    MFX_ERR_NONE, WIDTH * 8, &mfxFrameData::U, &mfxFrameData::Y, 2,                  &mfxFrameData::V, 4,                      &mfxFrameData::A, 6 } // 11
    #endif

    };

    static std::vector<std::tuple<
        mfxU32,                      // FourCC,
        mfxStatus,                   // return status for Lock
        mfxU32,                      // pitch,
        mfxU8* mfxFrameData::*,      // ptr1
        mfxU8* mfxFrameData::*, int, // ptr2, offset between ptr2 and ptr1 (in bytes)
        mfxU8* mfxFrameData::*, int, // ptr3, offset between ptr3 and ptr1 (in bytes)
        mfxU8* mfxFrameData::*, int  // ptr4, offset between ptr4 and ptr1 (in bytes)
        >> FourCC_SW = {
          { MFX_FOURCC_P8,         MFX_ERR_NONE,        WIDTH * HEIGHT, &mfxFrameData::Y, nullptr,          0,                  nullptr,          0,                      nullptr, 0 } // 12
        , { MFX_FOURCC_P8_TEXTURE, MFX_ERR_NONE,        WIDTH,          &mfxFrameData::Y, nullptr,          0,                  nullptr,          0,                      nullptr, 0 } // 13
        , { MFX_FOURCC_YV12,       MFX_ERR_NONE,        WIDTH,          &mfxFrameData::Y, &mfxFrameData::V, WIDTH * HEIGHT,     &mfxFrameData::U, WIDTH * HEIGHT * 5 / 4, nullptr, 0 } // 14
    #if defined (MFX_ENABLE_FOURCC_RGB565)
        , { MFX_FOURCC_RGB565,     MFX_ERR_NONE,        WIDTH * 2,      &mfxFrameData::B, &mfxFrameData::G, 0,                  &mfxFrameData::R, 0,                      nullptr, 0 } // 15
    #endif
        , { MFX_FOURCC_P210,       MFX_ERR_NONE,        WIDTH * 2,      &mfxFrameData::Y, &mfxFrameData::U, WIDTH * HEIGHT * 2, &mfxFrameData::V, WIDTH * HEIGHT * 2 + 2, nullptr, 0 } // 16
        , { MFX_FOURCC_RGB3,       MFX_ERR_NONE,        WIDTH * 3,      &mfxFrameData::B, &mfxFrameData::G, 1,                  &mfxFrameData::R, 2,                      nullptr, 0 } // 17
        , { MFX_FOURCC_IMC3,       MFX_ERR_UNSUPPORTED, 0,              nullptr,          nullptr,          0,                  nullptr,          0,                      nullptr, 0 } // 18
    #ifdef MFX_ENABLE_RGBP
        , { MFX_FOURCC_RGBP,    MFX_ERR_NONE,           WIDTH,          &mfxFrameData::B, &mfxFrameData::G, WIDTH* HEIGHT,      &mfxFrameData::R, WIDTH* HEIGHT * 2,      nullptr, 0 } // 19
    #endif
    };

#if defined(MFX_VA_WIN)
    static std::vector<std::tuple<
        mfxU32,                      // FourCC,
        mfxStatus,                   // return status for Lock
        mfxU32,                      // pitch,
        mfxU8* mfxFrameData::*,      // ptr1
        mfxU8* mfxFrameData::*, int, // ptr2, offset between ptr2 and ptr1 (in bytes)
        mfxU8* mfxFrameData::*, int, // ptr3, offset between ptr3 and ptr1 (in bytes)
        mfxU8* mfxFrameData::*, int  // ptr4, offset between ptr4 and ptr1 (in bytes)
        >> FourCC_HW_D3D11 = {
          { MFX_FOURCC_P8,         MFX_ERR_NONE,        WIDTH * HEIGHT, &mfxFrameData::Y, nullptr, 0, nullptr, 0, nullptr, 0 } // 12
        , { MFX_FOURCC_P8_TEXTURE, MFX_ERR_NONE,        WIDTH,          &mfxFrameData::Y, nullptr, 0, nullptr, 0, nullptr, 0 } // 13
        , { MFX_FOURCC_IMC3,       MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 14
        , { MFX_FOURCC_YUV400,     MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 15
        , { MFX_FOURCC_YUV411,     MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 16
        , { MFX_FOURCC_YUV422H,    MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 17
        , { MFX_FOURCC_YUV422V,    MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 18
        , { MFX_FOURCC_R16_RGGB,   MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 19
        , { MFX_FOURCC_R16_BGGR,   MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 20
        , { MFX_FOURCC_R16_GBRG,   MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 21
        , { MFX_FOURCC_R16_GRBG,   MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 22
        , { MFX_FOURCC_ARGB16,     MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 23
        , { MFX_FOURCC_ABGR16,     MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 24
    #ifdef MFX_ENABLE_RGBP
        , { MFX_FOURCC_RGBP,       MFX_ERR_LOCK_MEMORY, 0,              nullptr,          nullptr, 0, nullptr, 0, nullptr, 0 } // 25
    #endif
    };
#endif

#if defined(MFX_VA_LINUX)
    static std::vector<std::tuple<
        mfxU32,                      // FourCC,
        mfxStatus,                   // return status for Lock
        mfxU32,                      // pitch,
        mfxU8* mfxFrameData::*,      // ptr1
        mfxU8* mfxFrameData::*, int, // ptr2, offset between ptr2 and ptr1 (in bytes)
        mfxU8* mfxFrameData::*, int, // ptr3, offset between ptr3 and ptr1 (in bytes)
        mfxU8* mfxFrameData::*, int  // ptr4, offset between ptr4 and ptr1 (in bytes)
        >> FourCC_HW_VAAPI = {
        // TO DO: properly analyze case for MFX_FOURCC_P8
        { MFX_FOURCC_P8,         MFX_ERR_NONE, WIDTH * HEIGHT * 400LL / (16 * 16), &mfxFrameData::Y, nullptr,          0,              nullptr,          0,                      nullptr, 0 } // 12
      , { MFX_FOURCC_YV12,       MFX_ERR_NONE, WIDTH,                              &mfxFrameData::Y, &mfxFrameData::V, WIDTH * HEIGHT, &mfxFrameData::U, WIDTH * HEIGHT * 5 / 4, nullptr, 0 } // 13
  #if defined (MFX_ENABLE_FOURCC_RGB565)
      , { MFX_FOURCC_RGB565,     MFX_ERR_NONE, WIDTH * 2,                          &mfxFrameData::B, &mfxFrameData::G, 0             , &mfxFrameData::R, 0,                      nullptr, 0 } // 14
  #endif
      , { MFX_FOURCC_UYVY,       MFX_ERR_NONE, WIDTH * 2,                          &mfxFrameData::U, &mfxFrameData::Y, 1,              &mfxFrameData::V, 2,                      nullptr, 0 } // 15
      , { MFX_FOURCC_VP8_NV12,   MFX_ERR_NONE, WIDTH,                              &mfxFrameData::Y, &mfxFrameData::U, WIDTH * HEIGHT, &mfxFrameData::V, WIDTH * HEIGHT + 1,     nullptr, 0 } // 16
      , { MFX_FOURCC_VP8_MBDATA, MFX_ERR_NONE, WIDTH * HEIGHT,                     &mfxFrameData::Y, nullptr,          0,              nullptr,          0,                      nullptr, 0 } // 17
      , { MFX_FOURCC_VP8_SEGMAP, MFX_ERR_NONE, WIDTH,                              &mfxFrameData::Y, nullptr,          0,              nullptr,          0,                      nullptr, 0 } // 18
    #ifdef MFX_ENABLE_RGBP
      , { MFX_FOURCC_RGBP,       MFX_ERR_NONE, WIDTH,                              &mfxFrameData::B, &mfxFrameData::G, WIDTH * HEIGHT, &mfxFrameData::R, WIDTH * HEIGHT * 2,     nullptr, 0 } // 19
    #endif
    };
#endif


    static std::vector<mfxU32> platforms =
    {
        mocks::mfx::HW_TGL_LP // MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET
    };

    template<typename T>
    T merge(T& a, T& b)
    {
        T c = a;
        std::copy(b.begin(), b.end(), std::back_inserter(c));
        return c;
    }

    INSTANTIATE_TEST_SUITE_P(
        MemTypesFourccSW,
        FlexibleAllocatorFourCC,
        testing::Combine(
            testing::Values(0),
            testing::ValuesIn(merge(FourCC_common, FourCC_SW))
        )
    );

#if defined(MFX_VA_WIN)
    INSTANTIATE_TEST_SUITE_P(
        MemTypesFourccHWD3D11,
        FlexibleAllocatorFourCC,
        testing::Combine(
            testing::ValuesIn(platforms),
            testing::ValuesIn(merge(FourCC_common, FourCC_HW_D3D11))
        )
    );

#elif defined(MFX_VA_LINUX)
    INSTANTIATE_TEST_SUITE_P(
        MemTypesFourccHWVAAPI,
        FlexibleAllocatorFourCC,
        testing::Combine(
            testing::ValuesIn(platforms),
            testing::ValuesIn(merge(FourCC_common, FourCC_HW_VAAPI))
        )
    );
#endif


    TEST_P(FlexibleAllocatorFourCC, LockDifferentFourCC)
    {
        ASSERT_EQ(
            allocator->Alloc(req, res),
            MFX_ERR_NONE
        );
        mfxFrameData data{};

        auto info = std::get<1>(GetParam());
        mfxStatus status = std::get<1>(info);

        ASSERT_EQ(
            allocator->Lock(res.mids[0], &data, MFX_MAP_READ_WRITE),
            status
        );

        if (status == MFX_ERR_NONE)
        {

            EXPECT_EQ(
                data.PitchHigh,
                pitch >> 16
            );

            EXPECT_EQ(
                data.Pitch,
                pitch & 0xffff
            );

            std::vector<mfxU8* mfxFrameData::*> vptr = { &mfxFrameData::Y, &mfxFrameData::U, &mfxFrameData::V, &mfxFrameData::A };

            auto ptr1 = std::get<3>(info);
            vptr.erase(std::find(vptr.begin(), vptr.end(), ptr1));

            if (req.Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
            {
                auto expected_buffer = (mfxU8*)buffer.data();
#if defined(MFX_VA_LINUX) // TEMP
                if(fourcc == MFX_FOURCC_P8)
                    expected_buffer = nullptr;
#endif
                EXPECT_EQ(
                    data.*ptr1,
                    expected_buffer
                );
            }
            else
            {
                EXPECT_NE(
                    data.*ptr1,
                    nullptr
                );
            }

            std::vector<mfxU8* mfxFrameData::*> ptrs = { std::get<4>(info), std::get<6>(info), std::get<8>(info) };
            int offset[] = { std::get<5>(info), std::get<7>(info), std::get<9>(info) };
            int count = std::count_if(ptrs.begin(), ptrs.end(), [](mfxU8* mfxFrameData::* ptr) { return ptr != nullptr; } );
            for (int i = 0; i < count; i++)
            {
                EXPECT_EQ(
                    data.*ptrs[i] - data.*ptr1,
                    offset[i]
                );
                vptr.erase(std::find(vptr.begin(), vptr.end(), ptrs[i]));
            }
            EXPECT_TRUE(
                std::all_of(
                    vptr.begin(), vptr.end(),
                    [&data](mfxU8* mfxFrameData::* ptr) {return data.*ptr == nullptr; }
                )
            );
        }
    }
}
#endif // MFX_ENABLE_UNIT_TEST_ALLOCATOR
#endif
