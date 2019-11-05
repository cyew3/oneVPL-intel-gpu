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
    TEST(BlocksUtilsStorage, Empty)
    {
        MfxFeatureBlocks::StorageRW storage;
        EXPECT_TRUE(storage.Empty());
    }

    TEST(BlocksUtilsStorage, TryInsert)
    {
        struct value
            : MfxFeatureBlocks::Storable
        {};

        MfxFeatureBlocks::StorageRW storage;
        EXPECT_TRUE(
            storage.TryInsert(0, std::make_unique<value>())
        );
    }

    TEST(BlocksUtilsStorage, TryInsertExistingKey)
    {
        struct value
            : MfxFeatureBlocks::Storable
        {};

        MfxFeatureBlocks::StorageRW storage;
        ASSERT_TRUE(
            storage.TryInsert(0, std::make_unique<value>())
        );

        EXPECT_FALSE(
            storage.TryInsert(0, std::make_unique<value>())
        );
    }

    TEST(BlocksUtilsStorage, Insert)
    {
        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new int{ 42 });

        EXPECT_FALSE(storage.Empty());
    }

    TEST(BlocksUtilsStorage, InsertExisting)
    {
        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new int{ 42 });

        EXPECT_THROW(
            storage.Insert(0, new int{ 34 }),
            std::logic_error
        );
    }

    TEST(BlocksUtilsStorage, Erase)
    {
        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new int(42));
        ASSERT_TRUE(storage.Contains(0));
        storage.Erase(0);

        EXPECT_FALSE(storage.Contains(0));
    }

    TEST(BlocksUtilsStorage, EraseDestruct)
    {
        struct value
            : MfxFeatureBlocks::Storable
        {
            ~value() { OnDestruct(); }
            MOCK_CONST_METHOD0(OnDestruct, void());
        };

        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new value());

        auto& v = storage.Read<value>(0);
        EXPECT_CALL(v, OnDestruct())
            .WillOnce(testing::Return());

        storage.Erase(0);
    }

    TEST(BlocksUtilsStorage, Clear)
    {
        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new int(42));
        storage.Insert(1, new int(42));
        ASSERT_FALSE(storage.Empty());

        storage.Clear();

        EXPECT_TRUE(storage.Empty());
    }

    TEST(BlocksUtilsStorage, Contains)
    {
        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new int(42));

        EXPECT_TRUE(storage.Contains(0));
    }

    TEST(BlocksUtilsStorage, ContainsNoKey)
    {
        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new int(42));
        ASSERT_FALSE(storage.Empty());

        EXPECT_FALSE(storage.Contains(1));
    }

    TEST(BlocksUtilsStorage, ContainsEmpty)
    {
        MfxFeatureBlocks::StorageRW storage;
        EXPECT_FALSE(storage.Contains(0));
    }

    TEST(BlocksUtilsStorage, Read)
    {
        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new int(42));

        EXPECT_EQ(
            storage.Read<MfxFeatureBlocks::StorableRef<int> >(0), 42
        );
    }

    TEST(BlocksUtilsStorage, ReadEmpty)
    {
        MfxFeatureBlocks::StorageRW storage;
        EXPECT_THROW(
            storage.Read<MfxFeatureBlocks::StorableRef<int> >(0),
            std::logic_error
        );
    }

    TEST(BlocksUtilsStorage, ReadNoKey)
    {
        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new int(42));

        EXPECT_THROW(
            storage.Read<MfxFeatureBlocks::StorableRef<int> >(1),
            std::logic_error
        );
    }

    TEST(BlocksUtilsStorage, Write)
    {
        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new int(42));

        EXPECT_EQ(
            storage.Write<MfxFeatureBlocks::StorableRef<int> >(0), 42
        );
    }

    TEST(BlocksUtilsStorage, WriteEmpty)
    {
        MfxFeatureBlocks::StorageRW storage;
        EXPECT_THROW(
            storage.Write<MfxFeatureBlocks::StorableRef<int> >(0),
            std::logic_error
        );
    }

    TEST(BlocksUtilsStorage, WriteNoKey)
    {
        MfxFeatureBlocks::StorageRW storage;
        storage.Insert(0, new int(42));

        EXPECT_THROW(
            storage.Write<MfxFeatureBlocks::StorableRef<int> >(1),
            std::logic_error
        );
    }

} }