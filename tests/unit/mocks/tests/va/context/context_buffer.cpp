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
#include "mocks/include/va/context/context.h"
#include "mocks/include/va/resource/buffer.h"

namespace mocks { namespace test { namespace va
{
    TEST(context, CreateBufferWrongId)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(0), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );

        VABufferID id;
        EXPECT_NE(
            vaCreateBuffer(d.get(), VAContextID(42), VAPictureParameterBufferType, 1, 1, nullptr, &id),
            VA_STATUS_SUCCESS
        );
    }

    TEST(context, CreateBufferWrongType)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(0), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );

        VABufferID id;
        EXPECT_NE(
            vaCreateBuffer(d.get(), VAContextID(0), VAImageBufferType, 1, 1, nullptr, &id),
            VA_STATUS_SUCCESS
        );
    }

    TEST(context, CreateBufferWrongSize)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(0), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );

        VABufferID id;
        EXPECT_NE(
            vaCreateBuffer(d.get(), VAContextID(0), VAPictureParameterBufferType, 42, 1, nullptr, &id),
            VA_STATUS_SUCCESS
        );
    }

    TEST(context, CreateBufferWrongCount)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(0), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );

        VABufferID id;
        EXPECT_NE(
            vaCreateBuffer(d.get(), VAContextID(0), VAPictureParameterBufferType, 1, 42, nullptr, &id),
            VA_STATUS_SUCCESS
        );
    }

    TEST(context, CreateBufferNullData)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(0), VAConfigID(0)),
            std::make_tuple(VAPictureParameterBufferType, 1)
        );

        VABufferID id;
        EXPECT_EQ(
            vaCreateBuffer(d.get(), VAContextID(0), VAPictureParameterBufferType, 1, 1, nullptr, &id),
            VA_STATUS_SUCCESS
        );
    }

    TEST(context, CreateAndMapBufferNullDataOwnMemory)
    {
        char mem[] = {42};
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(2), VAConfigID(0)),
            std::make_tuple(
                VAEncMiscParameterBufferType, 1, 1,
                mem
            )
        );

        VABufferID id;
        ASSERT_EQ(
            vaCreateBuffer(d.get(), VAContextID(2), VAEncMiscParameterBufferType, 1, 1, nullptr, &id),
            VA_STATUS_SUCCESS
        );

        void* p = nullptr;
        ASSERT_EQ(
            vaMapBuffer(d.get(), id, &p),
            VA_STATUS_SUCCESS
        );

        EXPECT_EQ(
            p, mem
        );
    }

    TEST(context, CreateBuffer)
    {
        int dummy[2] = { 34, 42 };
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(0), VAConfigID(0)),
            std::make_tuple(
                VAPictureParameterBufferType,
                sizeof(dummy[0]),
                std::extent<decltype(dummy)>::value
            )
        );

        VABufferID id;
        EXPECT_EQ(
            vaCreateBuffer(d.get(), VAContextID(0), VAPictureParameterBufferType,
                sizeof(dummy[0]), std::extent<decltype(dummy)>::value, dummy,
                &id
            ), VA_STATUS_SUCCESS
        );
    }

    TEST(context, DestroyBufferWrongId)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(10), VAConfigID(0))
        );
        auto b = mocks::va::make_buffer(c.get());

        EXPECT_EQ(
            vaDestroyBuffer(d.get(), b->id + 1),
            VA_STATUS_ERROR_INVALID_BUFFER
        );
    }

    TEST(context, DestroyBufferTwoTimes)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(10), VAConfigID(0))
        );
        auto b = mocks::va::make_buffer(c.get());

        EXPECT_EQ(
            vaDestroyBuffer(d.get(), b->id),
            VA_STATUS_SUCCESS
        );

        EXPECT_NE(
            vaDestroyBuffer(d.get(), b->id),
            VA_STATUS_SUCCESS
        );
    }

} } }
