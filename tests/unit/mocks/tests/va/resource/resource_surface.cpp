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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mocks/include/va/display/display.h"
#include "mocks/include/va/resource/surface.h"

namespace mocks { namespace test { namespace va
{
    TEST(resource, DeriveImageWrongId)
    {
        auto d = mocks::va::make_display();
        auto s = mocks::va::make_surface(d.get(),
            std::make_tuple(
                VA_FOURCC_NV12, 16, 16
            )
        );

        VAImage image = {};
        EXPECT_NE(
            vaDeriveImage(d.get(), s->id + 1, &image),
            VA_STATUS_SUCCESS
        );
    }

    TEST(resource, DeriveImageNullPtr)
    {
        auto d = mocks::va::make_display();
        auto s = mocks::va::make_surface(d.get(),
            std::make_tuple(
                VA_FOURCC_NV12, 16, 16
            )
        );

        EXPECT_NE(
            vaDeriveImage(d.get(), s->id, nullptr),
            VA_STATUS_SUCCESS
        );
    }

    TEST(resource, DeriveImage)
    {
        auto dummy = 42;
        auto d = mocks::va::make_display();
        auto s = mocks::va::make_surface(d.get(),
            std::make_tuple(
                VA_RT_FORMAT_YUV420, VA_FOURCC_NV12,
                16, 32,
                &dummy
            )
        );

        VAImage image = {};
        EXPECT_EQ(
            vaDeriveImage(d.get(), s->id, &image),
            VA_STATUS_SUCCESS
        );
        EXPECT_EQ(
            image.width, 16
        );
        EXPECT_EQ(
            image.height, 32
        );
        EXPECT_EQ(
            image.format.fourcc, VA_FOURCC_NV12
        );
    }
} } }

#include "mocks/include/va/resource/buffer.h"
#include "mocks/include/va/resource/image.h"
namespace mocks { namespace test { namespace va
{
    TEST(resource, MultipleDeriveImageDestroyImage)
    {
        auto dummy = 42;
        auto f = [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
        { return &dummy; };

        auto d = mocks::va::make_display();
        auto s = make_surface(d.get(),
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                VA_FOURCC_NV12,
                32, 32,
                f
            )
        );

        for (auto j = 0; j < 2; ++j)
        {
            VAImage image = {};
            EXPECT_EQ(
                vaDeriveImage(d.get(), s->id, &image),
                VA_STATUS_SUCCESS
            );

            void* p = nullptr;
            EXPECT_EQ(
                vaMapBuffer(d.get(), image.buf, &p),
                VA_STATUS_SUCCESS
            );
            EXPECT_TRUE(
                mocks::equal(p, &dummy)
            );
            EXPECT_EQ(
                vaUnmapBuffer(d.get(), image.buf),
                VA_STATUS_SUCCESS
            );
            EXPECT_EQ(
                vaDestroyImage(d.get(), image.image_id),
                VA_STATUS_SUCCESS
            );
        }
    }

} } }
