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
#include "mocks/include/va/display/display.h"
#include "mocks/include/va/display/context.h"

namespace mocks { namespace test { namespace va
{
    TEST(display, CreateContextWrongConfig)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                std::make_tuple(VAConfigAttrib{ VAConfigAttribRTFormat }),
                42, 42, 0
            )
        );
        auto c = mocks::va::make_config(d.get(), VAConfigID(0));

        VAContextID id;
        EXPECT_EQ(
            vaCreateContext(d.get(), c->id + 1, 42, 42, 0, nullptr, 0, &id),
            VA_STATUS_ERROR_INVALID_CONFIG
        );
    }

    TEST(display, CreateContextWrongResolution)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VAProfileNone,
                VAEntrypointVLD
            ),
            std::make_tuple(
                42, 42, 0
            )
        );
        auto c = mocks::va::make_config(d.get(),
            std::make_tuple(VAProfileNone, VAEntrypointVLD)
        );

        VAContextID id;
        EXPECT_NE(
            vaCreateContext(d.get(), c->id, 34, 42, 0, nullptr, 0, &id),
            VA_STATUS_SUCCESS
        );
        EXPECT_NE(
            vaCreateContext(d.get(), c->id, 42, 34, 0, nullptr, 0, &id),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateContextWrongFlags)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                std::make_tuple(VAConfigAttrib{ VAConfigAttribRTFormat }),
                42, 42, 0
            )
        );
        auto c = mocks::va::make_config(d.get(), VAConfigID(0));

        VAContextID id;
        EXPECT_EQ(
            vaCreateContext(d.get(), c->id, 42, 42, 0x3, nullptr, 0, &id),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, CreateContextNullId)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                std::make_tuple(VAConfigAttrib{ VAConfigAttribRTFormat }),
                42, 42, 0
            )
        );
        auto c = mocks::va::make_config(d.get(), VAConfigID(0));

        EXPECT_EQ(
            vaCreateContext(d.get(), c->id, 42, 42, 0, nullptr, 0, nullptr),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, CreateContextZeroSurfracesNonZeroCount)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                std::make_tuple(VAConfigAttrib{ VAConfigAttribRTFormat }),
                42, 42, 0
            )
        );
        auto c = mocks::va::make_config(d.get(), VAConfigID(0));

        VAContextID id;
        EXPECT_EQ(
            vaCreateContext(d.get(), c->id, 42, 42, 0, nullptr, 1, &id),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, CreateContextNonZeroSurfracesZeroCount)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VAProfileNone,
                VAEntrypointVLD
            ),
            std::make_tuple(
                std::make_tuple(VAConfigAttrib{ VAConfigAttribRTFormat }),
                42, 42, 0
            )
        );
        auto c = mocks::va::make_config(d.get(),
            std::make_tuple(VAProfileNone, VAEntrypointVLD)
        );

        VAContextID id;
        VASurfaceID surfaces[2]= {};
        EXPECT_EQ(
            vaCreateContext(d.get(), c->id, 42, 42, 0, surfaces, 0, &id),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateContextZeroSurfaces)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(
                VAProfileNone,
                VAEntrypointVLD
            ),
            std::make_tuple(
                std::make_tuple(VAConfigAttrib{ VAConfigAttribRTFormat }),
                42, 42, 0
            )
        );
        auto c = mocks::va::make_config(d.get(),
            std::make_tuple(VAProfileNone, VAEntrypointVLD)
        );

        VAContextID id;
        EXPECT_EQ(
            vaCreateContext(d.get(), c->id, 42, 42, 0, nullptr, 0, &id),
            VA_STATUS_SUCCESS
        );
    }

    TEST(display, CreateContext)
    {
        VAConfigAttrib attributes[] = {
            { VAConfigAttribRTFormat},
            { VAConfigAttribDecProcessing}
        };
        auto constexpr count = std::extent<decltype(attributes)>::value;

        auto d = mocks::va::make_display(
            std::make_tuple(
                VAProfileNone,
                VAEntrypointVLD,
                std::make_tuple(attributes, count)
            ),
            std::make_tuple(
                std::make_tuple(attributes, count),
                42, 42, 0x3
            )
        );
        auto c = mocks::va::make_config(d.get(),
            std::make_tuple(VAProfileNone, VAEntrypointVLD),
            VAConfigID(0)
        );

        VAContextID id;
        VASurfaceID surfaces[2]= {};
        EXPECT_EQ(
            vaCreateContext(d.get(), c->id, 42, 42, 0x3, surfaces, std::extent<decltype(surfaces)>::value, &id),
            VA_STATUS_SUCCESS
        );
    }

    TEST(context, DestroyContextWrongId)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(10), VAConfigID(0))
        );

        EXPECT_EQ(
            vaDestroyContext(d.get(), c->id + 1),
            VA_STATUS_ERROR_INVALID_CONTEXT
        );
    }

    TEST(display, DestroyContext)
    {
        auto d = mocks::va::make_display();
        auto c = mocks::va::make_context(d.get(),
            std::make_tuple(VAContextID(10), VAConfigID(0))
        );

        EXPECT_EQ(
            vaDestroyContext(d.get(), c->id),
            VA_STATUS_SUCCESS
        );
    }

} } }
