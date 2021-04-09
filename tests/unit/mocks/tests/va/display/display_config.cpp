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
    TEST(display, MaxNumConfigAttributes)
    {
        auto d = mocks::va::make_display();
        EXPECT_EQ(
            vaMaxNumConfigAttributes(d.get()),
            0
        );

        d = mocks::va::make_display(
            std::make_tuple(
                VAProfileNone, VAEntrypointVLD,
                std::make_tuple(VAConfigAttrib{ VAConfigAttribRTFormat })
            )
        );

        EXPECT_EQ(
            vaMaxNumConfigAttributes(d.get()),
            1
        );

        d = mocks::va::make_display(
            std::make_tuple(
                VAProfileNone, VAEntrypointVLD,
                std::make_tuple(
                    VAConfigAttrib{ VAConfigAttribRTFormat },
                    VAConfigAttrib{ VAConfigAttribDecProcessing }
                )
            )
        );

        EXPECT_EQ(
            vaMaxNumConfigAttributes(d.get()),
            2
        );
    }

    TEST(display, GetConfigAttributesWrongProfile)
    {
        auto d = mocks::va::make_display();
        VAConfigAttrib attributes[1] = {};

        EXPECT_EQ(
            vaGetConfigAttributes(d.get(), VAProfileNone, VAEntrypointVLD, attributes, 1),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, GetConfigAttributesWrongEntrypoint)
    {
        auto d = mocks::va::make_display(std::make_tuple(VAProfileNone, VAEntrypointVLD));
        VAConfigAttrib attributes[1] = {};

        EXPECT_EQ(
            vaGetConfigAttributes(d.get(), VAProfileNone, VAEntrypointEncSlice, attributes, 1),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, GetConfigAttributesNullAttrib)
    {
        auto d = mocks::va::make_display(std::make_tuple(VAProfileNone, VAEntrypointVLD));

        EXPECT_EQ(
            vaGetConfigAttributes(d.get(), VAProfileNone, VAEntrypointEncSlice, nullptr, 0),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, GetConfigAttributesZeroAttrib)
    {
        auto d = mocks::va::make_display(std::make_tuple(VAProfileNone, VAEntrypointVLD));
        VAConfigAttrib attributes[1] = { VAConfigAttribDecSliceMode };

        EXPECT_EQ(
            vaGetConfigAttributes(d.get(), VAProfileNone, VAEntrypointVLD, attributes, 0),
            VA_STATUS_SUCCESS
        );

        EXPECT_TRUE(
            mocks::equal(attributes[0], VAConfigAttrib{ VAConfigAttribDecSliceMode })
        );
    }

    TEST(display, GetConfigAttributes)
    {
        size_t constexpr count = 2;
        VAConfigAttrib attributes[count] = {{ VAConfigAttribRTFormat, 34 }, { VAConfigAttribDecProcessing, 42 } };

        auto d = mocks::va::make_display(
            std::make_tuple(
                VAProfileNone,
                VAEntrypointVLD,
                std::make_tuple(attributes[0], attributes[1])
            )
        );

        attributes[0] = { VAConfigAttribRTFormat };
        attributes[1] = { VAConfigAttribDecProcessing };
        EXPECT_EQ(
            vaGetConfigAttributes(d.get(), VAProfileNone, VAEntrypointVLD, attributes, count),
            VA_STATUS_SUCCESS
        );
        EXPECT_TRUE(
            mocks::equal(attributes[0], VAConfigAttrib{ VAConfigAttribRTFormat, 34 })
        );
        EXPECT_TRUE(
            mocks::equal(attributes[1], VAConfigAttrib{ VAConfigAttribDecProcessing, 42 })
        );
    }

    TEST(display, GetConfigAttributesDuplicatedAttrib)
    {
        size_t constexpr count = 2;
        VAConfigAttrib attributes[count] = {{ VAConfigAttribRTFormat, 34 }, { VAConfigAttribDecProcessing, 42 } };

        auto d = mocks::va::make_display(
            std::make_tuple(
                VAProfileNone,
                VAEntrypointVLD,
                std::make_tuple(attributes[0], attributes[1])
            )
        );

        attributes[0] = attributes[1] = { VAConfigAttribDecProcessing };
        EXPECT_EQ(
            vaGetConfigAttributes(d.get(), VAProfileNone, VAEntrypointVLD, attributes, count),
            VA_STATUS_SUCCESS
        );
        EXPECT_TRUE(
            mocks::equal(attributes[0], VAConfigAttrib{ VAConfigAttribDecProcessing, 42 })
        );
        EXPECT_TRUE(
            mocks::equal(attributes[1], VAConfigAttrib{ VAConfigAttribDecProcessing, 42 })
        );
    }

    TEST(display, GetConfigAttributesReorderedAttrib)
    {
        size_t constexpr count = 2;
        VAConfigAttrib attributes[count] = {{ VAConfigAttribRTFormat, 34 }, { VAConfigAttribDecProcessing, 42 } };

        auto d = mocks::va::make_display(
            std::make_tuple(
                VAProfileNone,
                VAEntrypointVLD,
                std::make_tuple(attributes[0], attributes[1])
            )
        );

        std::swap(attributes[0], attributes[1]);
        EXPECT_EQ(
            vaGetConfigAttributes(d.get(), VAProfileNone, VAEntrypointVLD, attributes, count),
            VA_STATUS_SUCCESS
        );
        EXPECT_EQ(
            attributes[0].type, VAConfigAttribDecProcessing
        );
        EXPECT_EQ(
            attributes[0].value, 42
        );
        EXPECT_EQ(
            attributes[1].type, VAConfigAttribRTFormat
        );
        EXPECT_EQ(
            attributes[1].value, 34
        );
    }

    TEST(display, GetConfigAttributesUnsupportedAttrib)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VAProfileNone,
                VAEntrypointVLD,
                std::make_tuple(
                    VAConfigAttrib{ VAConfigAttribDecProcessing, 10 }
                )
            )
        );

        size_t constexpr count = 2;
        VAConfigAttrib attributes[count] = {};
        attributes[0].type = VAConfigAttribRTFormat;
        attributes[1].type = VAConfigAttribDecProcessing;

        EXPECT_EQ(
            vaGetConfigAttributes(d.get(), VAProfileNone, VAEntrypointVLD, attributes, count),
            VA_STATUS_SUCCESS
        );
        EXPECT_TRUE(
            mocks::equal(attributes[0], VAConfigAttrib{ VAConfigAttribRTFormat, VA_ATTRIB_NOT_SUPPORTED })
        );
        EXPECT_TRUE(
            mocks::equal(attributes[1], VAConfigAttrib{ VAConfigAttribDecProcessing, 10 })
        );
    }

} } }
