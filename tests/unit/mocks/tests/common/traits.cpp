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

#include "mocks/include/common.h"

namespace mocks { namespace test { namespace common
{
    template <typename T>
    void test_matcher_true(T)
    {
        static_assert(
            is_matcher<T>::value,
            "Type \'T\' must be a gmock::Matcher"
        );
    }

    TEST(is_matcher, True)
    {
        test_matcher_true(testing::_);
        test_matcher_true(testing::IsNull());
        test_matcher_true(testing::Eq(42));
        test_matcher_true(testing::Matcher<double>{});
        test_matcher_true(testing::AllOf(testing::NotNull(), testing::Truly([](int) { return true; })));
        test_matcher_true(testing::Contains(testing::Eq(42)));

        struct foo {};
        struct bar : testing::Matcher<foo> {};
        test_matcher_true(bar{});
    }

    template <typename T>
    void test_matcher_false(T)
    {
        static_assert(
            !is_matcher<T>::value,
            "Type \'T\' must not be a gmock::Matcher"
        );
    }

    TEST(is_matcher, False)
    {
        test_matcher_false(42);
        test_matcher_false(std::string{});

        struct foo {};
        test_matcher_false(foo{});

        struct bar : private testing::Matcher<int> {};
        test_matcher_false(bar{});
    }

} } }
