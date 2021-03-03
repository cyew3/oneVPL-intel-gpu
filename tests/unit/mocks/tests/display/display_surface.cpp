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

#include "mocks/include/va/display/surface.h"

namespace mocks { namespace test { namespace va
{
    static auto dummy = 42;
    auto f = [](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
        { return &dummy; };

    TEST(display, CreateSurfaceWrongFormat)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                f
            )
        );

        VASurfaceID surface = VA_INVALID_SURFACE;
        EXPECT_NE(
            vaCreateSurfaces(d.get(), 6, 32, 32, &surface, 1, nullptr, 0),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateSurfaceWrongResolution)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                f
            )
        );

        VASurfaceID surface = VA_INVALID_SURFACE;
        EXPECT_NE(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 40, 32, &surface, 1, nullptr, 0),
            VA_STATUS_SUCCESS
        );

        EXPECT_NE(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 32, 40, &surface, 1, nullptr, 0),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateSurfaceNullSurfaceIdsNonZeroCount)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                f
            )
        );

        EXPECT_NE(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 32, 32, nullptr, 1, nullptr, 0),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateSurfaceZeroCount)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                f
            )
        );

        VASurfaceID surface = VA_INVALID_SURFACE;
        EXPECT_NE(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 32, 32, &surface, 0, nullptr, 0),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateSurfaceNullSurfaceIdsZeroCount)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                f
            )
        );

        EXPECT_NE(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 32, 32, nullptr, 0, nullptr, 0),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateSurfaceNullAttriburesNonZeroCount)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                f
            )
        );

        VASurfaceID surface = VA_INVALID_SURFACE;
        EXPECT_NE(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 32, 32, &surface, 1, nullptr, 1),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateSurfaceAttriburesZeroCount)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                f
            )
        );

        VASurfaceID surface = VA_INVALID_SURFACE;
        VASurfaceAttrib attrib[2]= {};
        EXPECT_EQ(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 32, 32, &surface, 1, attrib, 0),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateSurfaceUnsupportedAttribures)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                std::make_tuple(
                    VASurfaceAttrib{ VASurfaceAttribPixelFormat },
                    VASurfaceAttrib{ VASurfaceAttribMemoryType }
                ),
                f
            )
        );

        VASurfaceID surface = VA_INVALID_SURFACE;
        VASurfaceAttrib attrib[2]= {};
        EXPECT_NE(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 32, 32, &surface, 1, attrib, 2),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateSurfaces)
    {
        VASurfaceAttrib attrib[2]= {};
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                5, 5,
                f
            ),
            std::make_tuple(
                VA_RT_FORMAT_RGB32,
                15, 15,
                std::make_tuple(attrib, std::extent<decltype(attrib)>::value),
                f
            )
        );

        std::vector<VASurfaceID> surfaces(10, VA_INVALID_SURFACE);
        EXPECT_EQ(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 5, 5, surfaces.data(), surfaces.size(), nullptr, 0),
            VA_STATUS_SUCCESS
        );
        EXPECT_EQ(
            std::count_if(surfaces.begin(), surfaces.end(),
                [](VASurfaceID id) { return id != VA_INVALID_SURFACE; }
            ), surfaces.size()
        );
    }

    TEST(display, CreateSurfaceWithPixelFormatAttribute)
    {
        auto attr = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger, .value = {.i = VA_FOURCC_P010}} };
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                std::make_tuple(
                    attr
                ),
                f
            )
        );

        VASurfaceID surface = VA_INVALID_SURFACE;
        auto attr1 = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger, .value = {.i = VA_FOURCC_NV12}} };
        EXPECT_NE(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 32, 32, &surface, 1, &attr1, 1),
            VA_STATUS_SUCCESS
        );

        auto attr2 = attr;
        EXPECT_EQ(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 32, 32, &surface, 1, &attr2, 1),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateSurfaceChooseRightFunctor)
    {
        static auto dummy1 = 42;
        auto f1 = [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
            { return &dummy1; };

        auto attr  = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger, .value = {.i = VA_FOURCC_NV12}} };
        auto attr1 = VASurfaceAttrib{ VASurfaceAttribPixelFormat, VA_SURFACE_ATTRIB_SETTABLE, {VAGenericValueTypeInteger, .value = {.i = VA_FOURCC_P010}} };
        auto d = mocks::va::make_display(
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                std::make_tuple(
                    attr
                ),
                f
            ),
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                32, 32,
                std::make_tuple(
                    attr1
                ),
                f1
            )
        );

        VASurfaceID surface = VA_INVALID_SURFACE;
        EXPECT_EQ(
            vaCreateSurfaces(d.get(), VA_RT_FORMAT_YUV420, 32, 32, &surface, 1, &attr, 1),
            VA_STATUS_SUCCESS
        );

        VAImage image = {};
        EXPECT_EQ(
            vaDeriveImage(d.get(), surface, &image),
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
    }

    TEST(display, DestroySurfaceWrongId)
    {
        auto d = mocks::va::make_display();
        auto s = mocks::va::make_surface(d.get(),
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                5, 5
            )
        );

        VASurfaceID id = s->id + 1;
        EXPECT_EQ(
            vaDestroySurfaces(d.get(), &id, 1),
            VA_STATUS_ERROR_INVALID_SURFACE
        );
    }

    TEST(display, DestroySurface)
    {
        auto d = mocks::va::make_display();
        auto s = mocks::va::make_surface(d.get(),
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                5, 5
            )
        );

        VASurfaceID id = s->id;
        EXPECT_EQ(
            vaDestroySurfaces(d.get(), &id, 1),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, DestroySurfaces)
    {
        auto d = mocks::va::make_display();
        auto s = mocks::va::make_surface(d.get());
        auto s1 = mocks::va::make_surface(d.get(),
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                5, 5
            )
        );

        VASurfaceID ids[] = { s->id, s1->id };
        EXPECT_EQ(
            vaDestroySurfaces(d.get(), ids, 2),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, DestroySurfacesWrongAndRightIds)
    {
        auto d = mocks::va::make_display();
        auto s = mocks::va::make_surface(d.get(),
            std::make_tuple(
                VA_RT_FORMAT_YUV420,
                5, 5
            )
        );

        VASurfaceID ids[] = { s->id, s->id + 1 } ;
        EXPECT_EQ(
            vaDestroySurfaces(d.get(), ids, 2),
            VA_STATUS_ERROR_INVALID_SURFACE
        );
    }

} } }
