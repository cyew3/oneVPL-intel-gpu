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

#include "mocks/include/va/display/context.h"
#include "mocks/include/va/resource/buffer.h"

namespace mocks { namespace test { namespace va
{
    TEST(resource, MapBufferWrongId)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(100), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );

        char buffer;
        auto b = mocks::va::make_buffer(c.get(),
            std::make_tuple(VAPictureParameterBufferType, sizeof(buffer), &buffer)
        );

        void* p = nullptr;
        EXPECT_EQ(
            vaMapBuffer(d.get(), b->id + 1, &p),
            VA_STATUS_ERROR_INVALID_BUFFER
        );
    }

    TEST(resource, MapBufferNullPtr)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(100), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );

        char buffer;
        auto b = mocks::va::make_buffer(c.get(),
            std::make_tuple(VAPictureParameterBufferType, sizeof(buffer), &buffer)
        );

        EXPECT_EQ(
            vaMapBuffer(d.get(), b->id, nullptr),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(resource, MapBufferWithBuffer)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(100), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );

        VAPictureParameterBufferMPEG2 buffer;
        auto b = mocks::va::make_buffer(c.get(),
            std::make_tuple(VAPictureParameterBufferType, sizeof(buffer), &buffer)
        );

        void* p = nullptr;
        EXPECT_EQ(
            vaMapBuffer(d.get(), b->id, &p),
            VA_STATUS_SUCCESS
        );
        EXPECT_TRUE(
            mocks::equal(p, &buffer)
        );
    }

    TEST(resource, MapBufferWithFunctor)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(100), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );

        int dummy = 42;
        auto b = mocks::va::make_buffer(c.get(),
            std::make_tuple(VAPictureParameterBufferType, sizeof(dummy),
                [&](VABufferType, size_t /*size*/)
                { return &dummy; }
            )
        );

        void* p = nullptr;
        EXPECT_EQ(
            vaMapBuffer(d.get(), b->id, &p),
            VA_STATUS_SUCCESS
        );
        EXPECT_TRUE(
            mocks::equal(p, &dummy)
        );
    }

    TEST(resource, UnmapBufferWrongId)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(100), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );
        auto b = mocks::va::make_buffer(c.get());

        EXPECT_EQ(
            vaUnmapBuffer(d.get(), b->id + 1),
            VA_STATUS_ERROR_INVALID_BUFFER
        );
    }

    TEST(resource, UnmapBuffer)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(100), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );
        auto b = mocks::va::make_buffer(c.get());

        EXPECT_EQ(
            vaUnmapBuffer(d.get(), b->id),
            VA_STATUS_SUCCESS
        );
    }

} } }
