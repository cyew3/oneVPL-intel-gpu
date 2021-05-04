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

#pragma once

#if defined(MFX_ENABLE_UNIT_TEST_CORE)

#define MFX_API_FUNCTION_IMPL(NAME, RTYPE, ARGS_DECL, ARGS) RTYPE NAME ARGS_DECL;

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
#include "mfxdispatcher.h"

#include "mocks/include/va/display/display.h"
#include "mocks/include/va/display/surface.h"
#include "mocks/include/va/display/context.h"

#include "libmfx_allocator_vaapi.h"
#include "libmfx_core_factory.h"
#include "libmfx_core_vaapi.h"

namespace test
{
    struct coreBase :
        public testing::Test
    {
        std::shared_ptr<mocks::va::display> display;
        VideoCORE*                          core{};
        std::unique_ptr<FrameAllocatorBase> allocator;

        mfxFrameInfo                        info{};
        mfxFrameSurface1*                   src = nullptr;
        mfxFrameSurface1*                   dst = nullptr;

        std::vector<mfxU32>                 buffer;

        void SetUp() override
        {
            info.Width        = 640;
            info.Height       = 480;
            info.FourCC       = MFX_FOURCC_NV12;

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

            core = FactoryCORE::CreateCORE(MFX_HW_VAAPI, 0, 0, nullptr);

            buffer.resize(info.Width * info.Height * 4);
            ASSERT_EQ(
                core->SetHandle(MFX_HANDLE_VA_DISPLAY, display.get()),
                MFX_ERR_NONE
            );
        }

        void SetAllocator(mfxU16 memType)
        {
            if (memType & MFX_MEMTYPE_SYSTEM_MEMORY)
            {
                allocator.reset(
                    new FlexibleFrameAllocatorSW{ display.get() }
                );
            }
            else if (memType & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
            {
                allocator.reset(
                    new FlexibleFrameAllocatorHW_VAAPI{ display.get() }
                );
            }
        }
    };

    struct coreMain :
        public coreBase
    {
        void SetUp() override
        {
            coreBase::SetUp();

            allocator.reset(
                new FlexibleFrameAllocatorHW_VAAPI{ display.get() }
            );

            ASSERT_EQ(
                allocator->CreateSurface(MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME, info, dst),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                allocator->CreateSurface(MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME, info, src),
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
}

#endif // MFX_ENABLE_UNIT_TEST_CORE
