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
#include "mfx_common.h"

#include "libmfx_allocator.h"

#include "libmfx_allocator_vaapi.h"

#include "mocks/include/va/display/display.h"
#include "mocks/include/va/display/surface.h"

#define FORCE_UNDEFINED_SYMBOL(x) static void* __ ## x ## _fp =(void*)&x;
FORCE_UNDEFINED_SYMBOL(vaCreateSurfaces)
FORCE_UNDEFINED_SYMBOL(vaCreateBuffer)
FORCE_UNDEFINED_SYMBOL(vaDeriveImage)
FORCE_UNDEFINED_SYMBOL(vaMapBuffer)
FORCE_UNDEFINED_SYMBOL(vaUnmapBuffer)
FORCE_UNDEFINED_SYMBOL(vaDestroyImage)

namespace test
{
    struct ScopedLock
        : testing::Test
    {
        std::shared_ptr<mocks::va::display> display;
        std::unique_ptr<SurfaceScopedLock>  scoped_lock;
        std::vector<char>                   buffer;
        VASurfaceID                         surface_id;
        void SetUp() override
        {
            buffer.resize(640 * 480 * 8);
            auto surface_attributes = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger,.value = {.i = VA_FOURCC_NV12} } };
            auto f = [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
            { return buffer.data(); };

            display = mocks::va::make_display(
                std::make_tuple(0, 0),  // version

                std::make_tuple(
                    VA_RT_FORMAT_YUV420,
                    640, 480,
                    std::make_tuple(surface_attributes),
                    f
                )
            );

            ASSERT_EQ(
                vaCreateSurfaces(display.get(), VA_RT_FORMAT_YUV420, 640, 480, &surface_id, 1, &surface_attributes, 1),
                VA_STATUS_SUCCESS
            );

            scoped_lock.reset(
                new SurfaceScopedLock(display.get(), surface_id)
            );
        }
    };

    TEST_F(ScopedLock, DeriveImageWithoutRelease)
    {
        EXPECT_CALL(*display, DestroyImage(testing::_)).Times(1);
        EXPECT_EQ(
            scoped_lock->DeriveImage(),
            MFX_ERR_NONE
        );
    }

    TEST_F(ScopedLock, MapWithoutRelease)
    {
        EXPECT_CALL(*display, DestroyImage(testing::_)).Times(1);
        EXPECT_CALL(*display, UnmapBuffer(testing::_)).Times(1);
        ASSERT_EQ(
            scoped_lock->DeriveImage(),
            MFX_ERR_NONE
        );

        mfxU8* ptr;
        EXPECT_EQ(
            scoped_lock->Map(ptr),
            MFX_ERR_NONE
        );
    }
}

#endif // MFX_ENABLE_UNIT_TEST_ALLOCATOR
#endif
