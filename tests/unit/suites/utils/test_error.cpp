// Copyright (c) 2020 Intel Corporation
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

#include "mfx_error.h"

namespace utils { namespace tests
{
    TEST(Error, Exception)
    {
        char const* msg = "TEXT";
        std::system_error e(mfx::make_error_code(MFX_ERR_ABORTED), msg);
        std::string what{ e.what() };

        EXPECT_THAT(
            what, testing::HasSubstr("operation aborted")
        );
        EXPECT_THAT(
            what, testing::HasSubstr(msg)
        );
    }

    struct Error
        : testing::TestWithParam<int>
    {};

    auto statuses = testing::Values(
          MFX_ERR_NONE
        , MFX_ERR_NULL_PTR
        , MFX_ERR_UNSUPPORTED
        , MFX_ERR_MEMORY_ALLOC
        , MFX_ERR_NOT_ENOUGH_BUFFER
        , MFX_ERR_INVALID_HANDLE
        , MFX_ERR_LOCK_MEMORY
        , MFX_ERR_NOT_INITIALIZED
        , MFX_ERR_NOT_FOUND
        , MFX_ERR_MORE_DATA
        , MFX_ERR_MORE_SURFACE
        , MFX_ERR_ABORTED
        , MFX_ERR_DEVICE_LOST
        , MFX_ERR_INCOMPATIBLE_VIDEO_PARAM
        , MFX_ERR_INVALID_VIDEO_PARAM
        , MFX_ERR_UNDEFINED_BEHAVIOR
        , MFX_ERR_DEVICE_FAILED
        , MFX_ERR_MORE_BITSTREAM
        , MFX_ERR_GPU_HANG
        , MFX_ERR_REALLOC_SURFACE
        , MFX_ERR_RESOURCE_MAPPED
        , MFX_WRN_IN_EXECUTION
        , MFX_WRN_DEVICE_BUSY
        , MFX_WRN_VIDEO_PARAM_CHANGED
        , MFX_WRN_PARTIAL_ACCELERATION
        , MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
        , MFX_WRN_VALUE_NOT_CHANGED
        , MFX_WRN_OUT_OF_RANGE
        , MFX_WRN_FILTER_SKIPPED
        , MFX_ERR_NONE_PARTIAL_OUTPUT
    );

    INSTANTIATE_TEST_SUITE_P(Status, Error, statuses);

    TEST_P(Error, MakeErrorCode)
    {
        auto p = static_cast<mfxStatus>(GetParam());
        std::error_code code = mfx::make_error_code(p);
        EXPECT_EQ(code.value(), int(p));
    }

    TEST_P(Error, Message)
    {
        auto p = static_cast<mfxStatus>(GetParam());
        std::error_code code = mfx::make_error_code(p);
        ASSERT_EQ(code.value(), int(p));

        std::string msg
            = code.message();

        EXPECT_TRUE(
            !msg.empty()
        );
    }

    struct Condition
        : testing::TestWithParam<std::pair<int, std::errc> >
    {};

    auto values = testing::Values(
        std::make_pair(MFX_ERR_UNSUPPORTED,              std::errc::not_supported),
        std::make_pair(MFX_ERR_NULL_PTR,                 std::errc::bad_address),
        std::make_pair(MFX_ERR_INVALID_HANDLE,           std::errc::bad_address),
        std::make_pair(MFX_ERR_MEMORY_ALLOC,             std::errc::not_enough_memory),
        std::make_pair(MFX_ERR_NOT_ENOUGH_BUFFER,        std::errc::no_buffer_space),
        std::make_pair(MFX_ERR_MORE_BITSTREAM,           std::errc::no_buffer_space),
        std::make_pair(MFX_ERR_NOT_FOUND,                std::errc::no_such_file_or_directory),
        std::make_pair(MFX_ERR_ABORTED,                  std::errc::operation_canceled),
        std::make_pair(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, std::errc::invalid_argument),
        std::make_pair(MFX_ERR_INVALID_VIDEO_PARAM,      std::errc::invalid_argument),
        std::make_pair(MFX_ERR_UNDEFINED_BEHAVIOR,       std::errc::state_not_recoverable),
        std::make_pair(MFX_ERR_DEVICE_FAILED,            std::errc::state_not_recoverable),
        std::make_pair(MFX_ERR_REALLOC_SURFACE,          std::errc::message_size),
        std::make_pair(MFX_ERR_RESOURCE_MAPPED,          std::errc::resource_deadlock_would_occur),
        std::make_pair(MFX_ERR_GPU_HANG,                 std::errc::device_or_resource_busy)
    );

    INSTANTIATE_TEST_SUITE_P(Status, Condition, values);

    TEST_P(Condition, Equivalent)
    {
        auto p = GetParam();
        std::error_code code = mfx::make_error_code(static_cast<mfxStatus>(p.first));
        EXPECT_EQ(
            code, p.second
        );
    }
} }
