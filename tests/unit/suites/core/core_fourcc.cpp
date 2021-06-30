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

#if (defined(_WIN32) || defined(_WIN64))
    #include "core_base_windows.h"
#elif defined(__linux__)
    #include "core_base_linux.h"
    #include "mocks/include/mfx/detail/platform.hxx"
    #include "mocks/include/va/display/context.h"
    #include "mocks/include/va/context/context.h"
    #include "mocks/include/va/format.h"
#endif

namespace test
{

    struct coreFourcc
        : public coreBase
        , public testing::WithParamInterface<int>
    {
        void SetUp() override
        {
            coreBase::SetUp();

            int fourcc = GetParam();
            mfxU16 memtype = MFX_MEMTYPE_SYSTEM_MEMORY;

#if defined(MFX_VA_WIN)
            auto constexpr guid = mocks::guid<&DXVA2_ModeH264_VLD_NoFGT>{};
            vp = mocks::mfx::make_param(
                fourcc,
                mocks::mfx::make_param(guid, vp)
            );

            mocks::mfx::dx11::mock_component(type, *(component->device.p),
                std::make_tuple(guid, vp)
            );

#elif defined(MFX_VA_LINUX)
            vp.mfx.FrameInfo.FourCC = fourcc;
            
            auto va_fourcc = mocks::va::to_native(fourcc);
            auto format = mocks::va::to_native_rt(fourcc);

            auto surface_attributes = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger,.value = {.i = int(va_fourcc)} } };

            display = mocks::va::make_display(
                std::make_tuple(0, 0),  // version

                std::make_tuple(
                    format,
                    640, 480,
                    std::make_tuple(surface_attributes),
                    [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
                    { return buffer.data(); }
                )
            );
            if(fourcc == MFX_FOURCC_P8)
            {
                memtype |= MFX_MEMTYPE_FROM_ENCODE;
                buffer.resize((640 * 480 * 400) / 256);
                mocks::va::make_context(display.get(),
                    std::make_tuple(VAContextID(0), VAConfigID(0)),
                    std::make_tuple(
                        VAEncCodedBufferType,
                        (640 * 480 * 400) / 256,
                        1,
                        buffer.data()
                    )
                );
            }
#endif
            SetAllocator(memtype);

            ASSERT_EQ(
                allocator->CreateSurface(memtype, vp.mfx.FrameInfo, dst),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                allocator->CreateSurface(memtype, vp.mfx.FrameInfo, src),
                MFX_ERR_NONE
            );

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

    std::vector<int> FourCC =
    {
          MFX_FOURCC_NV12
        , MFX_FOURCC_YUY2
        , MFX_FOURCC_YV12
        , MFX_FOURCC_P8
    };

    INSTANTIATE_TEST_SUITE_P(
        FourCC,
        coreFourcc,
        testing::ValuesIn(FourCC)
    );

    TEST_P(coreFourcc, DoFastCopy)
    {
        EXPECT_EQ(
            core->DoFastCopy(dst, src),
            MFX_ERR_NONE
        );
    }
}

#endif // MFX_ENABLE_UNIT_TEST_CORE
