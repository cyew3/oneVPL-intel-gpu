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

#include "mocks/include/va/display/image.h"

namespace mocks { namespace test { namespace va
{
    static auto dummy = 42;
    auto f = [](VAImageFormat, size_t /*width*/, size_t /*height*/)
        { return &dummy; };

    TEST(display, CreateImageNullFormat)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(VAImageFormat{}, 32, 32, f
            )
        );

        VAImage image = {VA_INVALID_ID};
        EXPECT_NE(
            vaCreateImage(d.get(), nullptr, 32, 32, &image),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateImageNullImage)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(VAImageFormat{}, 32, 32, f
            )
        );

        VAImageFormat format = {};
        EXPECT_NE(
            vaCreateImage(d.get(), &format, 32, 32, nullptr),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateImageWrongResolution)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(VAImageFormat{}, 32, 32, f
            )
        );

        VAImageFormat format = {};
        VAImage image = {VA_INVALID_ID};
        EXPECT_NE(
            vaCreateImage(d.get(), &format, 40, 40, &image),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateImageWrongFormat)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(VAImageFormat { VA_FOURCC_NV12, VA_LSB_FIRST/*, ...*/ } , 32, 32, f)
        );

        VAImageFormat format = {VA_FOURCC_NV12, VA_MSB_FIRST/*, ...*/ };
        VAImage image = {VA_INVALID_ID};
        EXPECT_NE(
            vaCreateImage(d.get(), &format, 32, 32, &image),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateImage)
    {
        VAImageFormat xformat = { VA_FOURCC_NV12, VA_LSB_FIRST/*, ...*/ };
        auto d = mocks::va::make_display(
            std::make_tuple(xformat, 32, 32, f)
        );

        VAImageFormat format = xformat;
        VAImage image = {VA_INVALID_ID};
        EXPECT_EQ(
            vaCreateImage(d.get(), &format, 32, 32, &image),
            VA_STATUS_SUCCESS
        );
        EXPECT_NE(
            image.image_id, VA_INVALID_ID
        );
        EXPECT_EQ(
            image.format, format
        );
    }

    TEST(display, DestroyImageWrongId)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(VAImageFormat { VA_FOURCC_NV12, VA_LSB_FIRST/*, ...*/ }, 32, 32, f)
        );
        auto i = mocks::va::make_image(d.get());

        EXPECT_NE(
            vaDestroyImage(d.get(), i->id + 1),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, DestroyImage)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(VAImageFormat { VA_FOURCC_NV12, VA_LSB_FIRST/*, ...*/ }, 32, 32, f)
        );
        auto i = mocks::va::make_image(d.get());

        EXPECT_EQ(
            vaDestroyImage(d.get(), i->id),
            VA_STATUS_SUCCESS
        );
    }

} } }

#include "mocks/include/va/display/context.h"
#include "mocks/include/va/resource/buffer.h"

namespace mocks { namespace test { namespace va
{
    TEST(display, CreateDestroyImagesAndCheckBuffers)
    {
        auto dummy1 = 55;
        auto f1 = [&](VAImageFormat, size_t /*width*/, size_t /*height*/)
        { return &dummy1; };

        VAImageFormat xformat = { VA_FOURCC_NV12, VA_LSB_FIRST/*, ...*/ };
        VAImageFormat xformat1 = { VA_FOURCC_YV12, VA_LSB_FIRST/*, ...*/ };
        auto d = mocks::va::make_display(
            std::make_tuple(xformat, 32, 32, f),
            std::make_tuple(xformat1, 32, 32, f1)
        );

        VAImageFormat format = xformat;
        VAImage image = {VA_INVALID_ID};
        EXPECT_EQ(
            vaCreateImage(d.get(), &format, 32, 32, &image),
            VA_STATUS_SUCCESS
        );
        EXPECT_NE(
            image.image_id, VA_INVALID_ID
        );
        EXPECT_EQ(
            image.format, format
        );
        EXPECT_NE(
            image.buf, VA_INVALID_ID
        );

        void* p = nullptr;
        EXPECT_EQ(
            vaMapBuffer(d.get(), image.buf, &p),
            VA_STATUS_SUCCESS
        );
        EXPECT_EQ(
            p, &dummy
        );

        EXPECT_EQ(
            vaDestroyImage(d.get(), image.image_id),
            VA_STATUS_SUCCESS
        );
        /**************************************************/
        VAImageFormat format1 = xformat1;
        VAImage image1 = {VA_INVALID_ID};
        EXPECT_EQ(
            vaCreateImage(d.get(), &format1, 32, 32, &image1),
            VA_STATUS_SUCCESS
        );
        EXPECT_NE(
            image1.image_id, VA_INVALID_ID
        );
        EXPECT_EQ(
            image1.format, format1
        );
        EXPECT_NE(
            image1.buf, VA_INVALID_ID
        );

        void* p1 = nullptr;
        EXPECT_EQ(
            vaMapBuffer(d.get(), image1.buf, &p1),
            VA_STATUS_SUCCESS
        );
        EXPECT_EQ(
            p1, &dummy1
        );

        EXPECT_EQ(
            vaUnmapBuffer(d.get(), image1.buf),
            VA_STATUS_SUCCESS
        );

        EXPECT_EQ(
            vaDestroyImage(d.get(), image1.image_id),
            VA_STATUS_SUCCESS
        );

        EXPECT_NE(
            vaMapBuffer(d.get(), image1.buf, &p1),
            VA_STATUS_SUCCESS
        );
    }

} } }
