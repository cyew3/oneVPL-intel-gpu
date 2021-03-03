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

#include <va/va.h>

namespace mocks { namespace test { namespace va
{
    struct display : testing::TestWithParam<int> {};
    INSTANTIATE_TEST_SUITE_P(profiles, display, testing::Range(
        int(VAProfileMPEG2Simple), int(VAProfileHEVCSccMain444)
    ));

    TEST(display, MaxNumProfiles)
    {
        auto d = mocks::va::make_display();
        EXPECT_EQ(
            vaMaxNumProfiles(d.get()),
            0
        );

        d = mocks::va::make_display(
            std::make_tuple(VAProfileNone, VAEntrypointVLD)
        );
        EXPECT_EQ(
            vaMaxNumProfiles(d.get()),
            1
        );

        d = mocks::va::make_display(
            std::make_tuple(VAProfileMPEG2Main, VAEntrypointVLD),
            std::make_tuple(VAProfileH264Main, VAEntrypointVLD),
            std::make_tuple(VAProfileHEVCMain, VAEntrypointVLD)
        );
        EXPECT_EQ(
            vaMaxNumProfiles(d.get()),
            3
        );
    }

    TEST(display, QueryConfigProfilesNullProfile)
    {
        auto d = mocks::va::make_display(std::make_tuple(VAProfileNone, VAEntrypointVLD));
        int count;
        EXPECT_EQ(
            vaQueryConfigProfiles(d.get(), nullptr, &count),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, QueryConfigProfilesNullCount)
    {
        auto d = mocks::va::make_display(std::make_tuple(VAProfileNone, VAEntrypointVLD));
        VAProfile profiles[1];
        EXPECT_EQ(
            vaQueryConfigProfiles(d.get(), profiles, nullptr),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST_P(display, QueryConfigProfiles)
    {
        auto d = mocks::va::make_display(
            std::make_tuple(static_cast<VAProfile>(GetParam()), VAEntrypointVLD)
        );

        ASSERT_EQ(
            vaMaxNumProfiles(d.get()), 1
        );

        int count;
        VAProfile profile;
        EXPECT_EQ(
            vaQueryConfigProfiles(d.get(), &profile, &count),
            VA_STATUS_SUCCESS
        );
        EXPECT_EQ(
            count, 1
        );
        EXPECT_EQ(
            profile,
            GetParam()
        );
    }

    TEST(display, MaxNumEntrypoints)
    {
        auto d = mocks::va::make_display();
        EXPECT_EQ(
            vaMaxNumEntrypoints(d.get()),
            0
        );

        d = mocks::va::make_display(
            std::make_tuple(VAProfileNone, VAEntrypointVLD)
        );
        EXPECT_EQ(
            vaMaxNumEntrypoints(d.get()),
            1
        );

        d = mocks::va::make_display(
            std::make_tuple(VAProfileNone, VAEntrypointVLD),
            std::make_tuple(VAProfileNone, VAEntrypointEncSlice)
        );
        EXPECT_EQ(
            vaMaxNumEntrypoints(d.get()),
            2
        );
    }

    TEST(display, QueryConfigEntrypointsNoProfiles)
    {
        auto d = mocks::va::make_display();
        VAEntrypoint entrypoints[1];
        int count;

        EXPECT_EQ(
            vaQueryConfigEntrypoints(d.get(), VAProfileNone, entrypoints, &count),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, QueryConfigEntrypointsWrongProfile)
    {
        auto d = mocks::va::make_display(std::make_tuple(VAProfileNone, VAEntrypointVLD));
        VAEntrypoint entrypoints[1];
        int count;

        EXPECT_EQ(
            vaQueryConfigEntrypoints(d.get(), VAProfileMPEG2Simple, entrypoints, &count),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, QueryConfigEntrypointsNullEntrypoints)
    {
        auto d = mocks::va::make_display(std::make_tuple(VAProfileNone, VAEntrypointVLD));
        int count;

        EXPECT_EQ(
            vaQueryConfigEntrypoints(d.get(), VAProfileNone, nullptr, &count),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST(display, QueryConfigEntrypointsNullCount)
    {
        auto d = mocks::va::make_display(std::make_tuple(VAProfileNone, VAEntrypointVLD));
        VAEntrypoint entrypoints[1];

        EXPECT_EQ(
            vaQueryConfigEntrypoints(d.get(), VAProfileMPEG2Simple, entrypoints, nullptr),
            VA_STATUS_ERROR_INVALID_PARAMETER
        );
    }

    TEST_P(display, QueryConfigEntrypoints)
    {
        auto profile = static_cast<VAProfile>(GetParam());

        auto d = mocks::va::make_display(
            std::make_tuple(profile, VAEntrypointVLD),
            std::make_tuple(profile, VAEntrypointEncSlice)
        );

        ASSERT_EQ(
            vaMaxNumEntrypoints(d.get()), 2
        );

        VAEntrypoint entrypoints[2];
        int count;

        EXPECT_EQ(
            vaQueryConfigEntrypoints(d.get(), profile, entrypoints, &count),
            VA_STATUS_SUCCESS
        );
        EXPECT_EQ(
            count, 2
        );
        EXPECT_EQ(
            entrypoints[0],
            VAEntrypointVLD
        );
        EXPECT_EQ(
            entrypoints[1],
            VAEntrypointEncSlice
        );
    }

} } }
