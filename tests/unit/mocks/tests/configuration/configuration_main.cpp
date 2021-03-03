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

#include "mocks/include/va/configuration/configuration.h"

namespace mocks { namespace test { namespace va
{
    TEST(display, QuerySurfaceAttributesWrongConfig)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_config(d.get(), VAConfigID(0));

        VASurfaceAttrib attributes[1] = {};
        unsigned int count;

        EXPECT_EQ(
            vaQuerySurfaceAttributes(d.get(), VAConfigID(42), attributes, &count),
            VA_STATUS_ERROR_INVALID_CONFIG
        );
    }

    TEST(display, QuerySurfaceAttributesNullCount)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_config(d.get(), VAConfigID(0),
            VASurfaceAttrib{ VASurfaceAttribMinWidth, 34 }
        );

        VASurfaceAttrib attributes[1] = {};

        EXPECT_EQ(
            vaQuerySurfaceAttributes(d.get(), c->id, attributes, nullptr),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, QuerySurfaceAttributesCount)
    {
        auto d = mocks::va::make_display();

        auto c = mocks::va::make_config(d.get(), VAConfigID(0),
            std::make_tuple(
                VASurfaceAttrib{ VASurfaceAttribMinWidth, 34 },
                VASurfaceAttrib{ VASurfaceAttribMaxWidth, 42 }
            )
        );

        unsigned int count;

        EXPECT_EQ(
            vaQuerySurfaceAttributes(d.get(), c->id, nullptr, &count),
            VA_STATUS_SUCCESS
        );
        EXPECT_EQ(
            count, 2
        );
    }

    TEST(display, QuerySurfaceAttributes)
    {
        auto d = mocks::va::make_display();

        VASurfaceAttrib attributes[] = {
            { VASurfaceAttribMinWidth, 34 },
            { VASurfaceAttribMaxWidth, 42 }
        };
        auto constexpr count = std::extent<decltype(attributes)>::value;
        auto c = mocks::va::make_config(d.get(), VAConfigID(0),
            std::make_tuple(attributes, count)
        );

        VASurfaceAttrib xattributes[count];
        unsigned int xcount;

        EXPECT_EQ(
            vaQuerySurfaceAttributes(d.get(), c->id, xattributes, &xcount),
            VA_STATUS_SUCCESS
        );
        EXPECT_EQ(
            xcount, count
        );

        EXPECT_TRUE(
            mocks::equal(attributes[0], VASurfaceAttrib{ VASurfaceAttribMinWidth, 34 })
        );
        EXPECT_TRUE(
            mocks::equal(attributes[1], VASurfaceAttrib{ VASurfaceAttribMaxWidth, 42 })
        );
    }

} } }
