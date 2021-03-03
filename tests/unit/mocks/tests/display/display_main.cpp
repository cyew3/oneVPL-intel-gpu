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

namespace mocks { namespace test { namespace va
{
    TEST(display, SetErrorCallback)
    {
        auto d = mocks::va::make_display();
        EXPECT_EQ(
            vaSetErrorCallback(d.get(), nullptr, nullptr),
            nullptr
        );
    }

    TEST(display, Initialize)
    {
        auto d = mocks::va::make_display();
        EXPECT_EQ(
            vaInitialize(d.get(), nullptr, nullptr),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );

        int major, minor;
        EXPECT_EQ(
            vaInitialize(d.get(), &major, &minor),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );

        d = mocks::va::make_display(
            std::make_tuple(34, 42)
        );

        EXPECT_EQ(
            vaInitialize(d.get(), &major, &minor),
            VA_STATUS_SUCCESS
        );
        EXPECT_EQ(
            major, 34
        );
        EXPECT_EQ(
            minor, 42
        );
    }

    TEST(display, Terminate)
    {
        auto d = mocks::va::make_display();
        EXPECT_EQ(
            vaTerminate(d.get()),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, QueryVendorString)
    {
        auto d = mocks::va::make_display();
        EXPECT_STREQ(
            vaQueryVendorString(d.get()),
            nullptr
        );

        d = mocks::va::make_display(
            "intel"
        );
        EXPECT_STREQ(
            vaQueryVendorString(d.get()),
            "intel"
        );
    }

} } }
