// Copyright (c) 2019 Intel Corporation
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

#include "feature_blocks/mfx_feature_blocks_utils.h"

namespace hevce { namespace tests
{
    struct foo
    {
        virtual mfxStatus Call(int) = 0;
    };

    struct block
    {
        using TCall = std::unary_function<void, mfxStatus>;

        int* value = nullptr;
        foo* f     = nullptr;
        block(int* v, foo* f) : value(v), f(f) {}

        template <typename ...Args>
        mfxStatus Call(Args&& ...args) const
        {
            assert(value);
            auto sts = f->Call(std::forward<Args>(args)...);
            ++*value;
            return sts;
        }
    };

    struct foo1 : foo { MOCK_METHOD1(Call, mfxStatus(int)); };
    struct foo2 : foo { MOCK_METHOD1(Call, mfxStatus(int)); };
    struct foo3 : foo { MOCK_METHOD1(Call, mfxStatus(int)); };

    TEST(BlocksUtilsRun, RunBlocksInOrder)
    {

        foo1 f1; EXPECT_CALL(f1, Call(testing::Eq(0))).WillOnce(testing::Return(MFX_ERR_NONE));
        foo2 f2; EXPECT_CALL(f2, Call(testing::Eq(1))).WillOnce(testing::Return(MFX_ERR_NONE));
        foo3 f3; EXPECT_CALL(f3, Call(testing::Eq(2))).WillOnce(testing::Return(MFX_ERR_NONE));

        int value = 0;
        std::vector<block> queue{
            block(&value, &f1), block(&value, &f2), block(&value, &f3)
        };

        EXPECT_EQ(
            MfxFeatureBlocks::RunBlocks(
                MfxFeatureBlocks::IgnoreSts,
                queue, value
            ),
            MFX_ERR_NONE
        );
        EXPECT_EQ(
            value, 3
        );
    }

    TEST(BlocksUtilsRun, RunBlocksStopAt)
    {
        foo1 f1; EXPECT_CALL(f1, Call(testing::Eq(0))).WillOnce(testing::Return(MFX_ERR_NONE));
        foo2 f2; EXPECT_CALL(f2, Call(testing::Eq(1))).WillOnce(testing::Return(MFX_ERR_DEVICE_FAILED));
        foo3 f3; EXPECT_CALL(f3, Call(testing::_)).Times(0);

        int value = 0;
        std::vector<block> queue{
            block(&value, &f1), block(&value, &f2), block(&value, &f3)
        };

        auto StopAt = [](mfxStatus sts) { return sts != MFX_ERR_NONE; };
        EXPECT_EQ(
            MfxFeatureBlocks::RunBlocks(
                StopAt,
                queue, value
            ),
            MFX_ERR_DEVICE_FAILED
        );
        EXPECT_EQ(
            value, 2
        );
    }

    TEST(BlocksUtilsRun, RunBlocksCallThrows)
    {
        foo1 f1; EXPECT_CALL(f1, Call(testing::Eq(1))).WillOnce(testing::Return(MFX_ERR_NONE));
        foo2 f2; EXPECT_CALL(f2, Call(testing::Eq(2))).WillOnce(
            testing::InvokeWithoutArgs([]() ->mfxStatus { throw std::exception("exception"); })
        );
        foo3 f3; EXPECT_CALL(f3, Call(testing::_)).Times(0);

        int value = 1;
        std::vector<block> queue{
            block(&value, &f1), block(&value, &f2), block(&value, &f3)
        };

        EXPECT_EQ(
            MfxFeatureBlocks::RunBlocks(
                MfxFeatureBlocks::IgnoreSts,
                queue, value
            ),
            MFX_ERR_UNKNOWN
        );
        EXPECT_EQ(
            value, 2
        );
    }

    TEST(BlocksUtilsRun, RunBlocksReturnWorstStatusError)
    {
        foo1 f1; EXPECT_CALL(f1, Call(testing::Eq(1))).WillOnce(testing::Return(MFX_ERR_UNKNOWN));
        foo2 f2; EXPECT_CALL(f2, Call(testing::Eq(2))).WillOnce(testing::Return(MFX_ERR_NULL_PTR));
        foo3 f3; EXPECT_CALL(f3, Call(testing::Eq(3))).WillOnce(testing::Return(MFX_ERR_UNSUPPORTED));

        int value = 1;
        std::vector<block> queue{
            block(&value, &f1), block(&value, &f2), block(&value, &f3)
        };

        EXPECT_EQ(
            MfxFeatureBlocks::RunBlocks(
                MfxFeatureBlocks::IgnoreSts,
                queue, value
            ),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST(BlocksUtilsRun, RunBlocksReturnWorstStatusWarning)
    {
        foo1 f1; EXPECT_CALL(f1, Call(testing::Eq(1))).WillOnce(testing::Return(MFX_WRN_IN_EXECUTION));
        foo2 f2; EXPECT_CALL(f2, Call(testing::Eq(2))).WillOnce(testing::Return(MFX_ERR_NONE));
        foo3 f3; EXPECT_CALL(f3, Call(testing::Eq(3))).WillOnce(testing::Return(MFX_WRN_DEVICE_BUSY));

        int value = 1;
        std::vector<block> queue{
            block(&value, &f1), block(&value, &f2), block(&value, &f3)
        };

        EXPECT_EQ(
            MfxFeatureBlocks::RunBlocks(
                MfxFeatureBlocks::IgnoreSts,
                queue, value
            ),
            MFX_WRN_IN_EXECUTION
        );
    }

    TEST(BlocksUtilsRun, RunBlocksReturnWorstStatusNoError)
    {
        foo1 f1; EXPECT_CALL(f1, Call(testing::Eq(1))).WillOnce(testing::Return(MFX_ERR_NONE));
        foo2 f2; EXPECT_CALL(f2, Call(testing::Eq(2))).WillOnce(testing::Return(MFX_ERR_NONE));
        foo3 f3; EXPECT_CALL(f3, Call(testing::Eq(3))).WillOnce(testing::Return(MFX_ERR_NONE));

        int value = 1;
        std::vector<block> queue{
            block(&value, &f1), block(&value, &f2), block(&value, &f3)
        };

        EXPECT_EQ(
            MfxFeatureBlocks::RunBlocks(
                MfxFeatureBlocks::IgnoreSts,
                queue, value
            ),
            MFX_ERR_NONE
        );
    }

} }