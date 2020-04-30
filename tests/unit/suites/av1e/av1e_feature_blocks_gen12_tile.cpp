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

#include "mfxvideo++.h"
#include "mfx_session.h"
#include "av1ehw_g12_tile.h"

using namespace AV1EHW;
using namespace AV1EHW::Gen12;

namespace av1e {
    namespace tests
    {
        struct FeatureBlocksTileSingleTile
            : testing::Test
        {
            static FeatureBlocks   blocks;
            static Tile            tile;
            static StorageRW       strg;

            static void SetUpTestCase()
            {
                tile.Init(INIT | RUNTIME, blocks);

                Glob::VideoParam::GetOrConstruct(strg);
                Glob::EncodeCaps::GetOrConstruct(strg);
            }

            static void TearDownTestCase()
            {
                strg.Clear();
            }

            void InitAV1Param(ExtBuffer::Param<mfxVideoParam>& vp)
            {
                mfxExtAV1Param* pAV1Par = ExtBuffer::Get(vp);

                pAV1Par->FrameWidth = 7680;
                pAV1Par->FrameHeight = 4320;
            }
        };

        FeatureBlocks FeatureBlocksTileSingleTile::blocks{};
        Tile FeatureBlocksTileSingleTile::tile(FEATURE_TILE);
        StorageRW FeatureBlocksTileSingleTile::strg{};

        TEST_F(FeatureBlocksTileSingleTile, CheckAndFix)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
            const auto block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_CheckAndFix });
            auto& vp = Glob::VideoParam::Get(strg);

            vp.NewEB(MFX_EXTBUFF_AV1_PARAM, false);
            InitAV1Param(vp);
            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_ERR_NONE
            );
        }

        TEST_F(FeatureBlocksTileSingleTile, SetDefaults)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_SetDefaults>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetDefaults });
            auto& vp = Glob::VideoParam::Get(strg);

            block->Call(vp, strg, strg);

            mfxExtAV1Param* pAV1Par = ExtBuffer::Get(vp);

            ASSERT_EQ(pAV1Par->UniformTileSpacing, MFX_CODINGOPTION_ON);
            ASSERT_EQ(pAV1Par->NumTileColumns, 1);
            ASSERT_EQ(pAV1Par->NumTileRows, 1);

            ASSERT_EQ(
                pAV1Par->TileWidthInSB[0],
                CeilDiv(pAV1Par->FrameWidth, SB_SIZE)
            );

            ASSERT_EQ(
                pAV1Par->TileHeightInSB[0],
                CeilDiv(pAV1Par->FrameHeight, SB_SIZE)
            );

            ASSERT_EQ(
                pAV1Par->ContextUpdateTileIdPlus1,
                pAV1Par->NumTileColumns * pAV1Par->NumTileRows
            );
        }

        TEST_F(FeatureBlocksTileSingleTile, SetTileInfo)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileInfo });

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_UNDEFINED_BEHAVIOR
            );

            const auto& fh = Glob::FH::GetOrConstruct(strg);

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            const auto& vp = Glob::VideoParam::Get(strg);
            const mfxExtAV1Param* pAV1Par = ExtBuffer::Get(vp);

            ASSERT_EQ(fh.tile_info.TileCols, pAV1Par->NumTileColumns);

            for (auto i = 0; i < pAV1Par->NumTileColumns; i++)
            {
                ASSERT_EQ(fh.tile_info.TileWidthInSB[i], pAV1Par->TileWidthInSB[i]);
            }

            ASSERT_EQ(fh.tile_info.TileRows, pAV1Par->NumTileRows);

            for (auto i = 0; i < pAV1Par->NumTileRows; i++)
            {
                ASSERT_EQ(fh.tile_info.TileHeightInSB[i], pAV1Par->TileHeightInSB[i]);
            }
        }

        TEST_F(FeatureBlocksTileSingleTile, SetTileGroups)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileGroups });

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            ASSERT_TRUE(strg.Contains(Glob::TileGroups::Key));

            const auto& infos = Glob::TileGroups::Get(strg);
            ASSERT_EQ(
                infos.size(),
                1
            );

            ASSERT_EQ(infos[0].TgStart, 0);

            const auto& fh = Glob::FH::Get(strg);
            ASSERT_EQ(
                infos[0].TgEnd,
                fh.tile_info.TileCols * fh.tile_info.TileRows - 1
            );
        }

        struct FeatureBlocksTileMultiTile
            : testing::Test
        {
            static FeatureBlocks   blocks;
            static Tile            tile;
            static StorageRW       strg;

            static void SetUpTestCase()
            {
                tile.Init(INIT | RUNTIME, blocks);

                Glob::VideoParam::GetOrConstruct(strg);
                Glob::EncodeCaps::GetOrConstruct(strg);
            }

            static void TearDownTestCase()
            {
                strg.Clear();
            }

            void InitAV1Param(ExtBuffer::Param<mfxVideoParam>& vp)
            {
                mfxExtAV1Param* pAV1Par = ExtBuffer::Get(vp);

                pAV1Par->FrameWidth = 7680;
                pAV1Par->FrameHeight = 4320;

                pAV1Par->UniformTileSpacing = MFX_CODINGOPTION_ON;

                pAV1Par->NumTileColumns = 2;
                pAV1Par->NumTileRows = 2;

                ASSERT_EQ(
                    CeilDiv(pAV1Par->FrameWidth, SB_SIZE) % pAV1Par->NumTileColumns,
                    0
                );

                ASSERT_EQ(
                    CeilDiv(pAV1Par->FrameHeight, SB_SIZE) % pAV1Par->NumTileRows,
                    0
                );

                for (auto i = 0; i < pAV1Par->NumTileColumns; i++)
                    pAV1Par->TileWidthInSB[i] = CeilDiv(pAV1Par->FrameWidth, SB_SIZE) / pAV1Par->NumTileColumns;

                for (auto i = 0; i < pAV1Par->NumTileRows; i++)
                    pAV1Par->TileHeightInSB[i] = CeilDiv(pAV1Par->FrameHeight, SB_SIZE) / pAV1Par->NumTileRows;
            }
        };

        FeatureBlocks FeatureBlocksTileMultiTile::blocks{};
        Tile FeatureBlocksTileMultiTile::tile(FEATURE_TILE);
        StorageRW FeatureBlocksTileMultiTile::strg{};

        TEST_F(FeatureBlocksTileMultiTile, CheckTileWithParam)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_CheckAndFix });
            auto& vp = Glob::VideoParam::Get(strg);

            vp.NewEB(MFX_EXTBUFF_AV1_PARAM, false);
            InitAV1Param(vp);
            mfxExtAV1Param* pAV1Par = ExtBuffer::Get(vp);

            // Sum of Tile heights not equals to image heights
            pAV1Par->TileHeightInSB[1]++;
            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_ERR_UNSUPPORTED
            );

            // Maximum height exceeds spec limitation
            InitAV1Param(vp);
            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_ERR_UNSUPPORTED
            );

            InitAV1Param(vp);
            pAV1Par->NumTileRows = 4;
            ASSERT_EQ(
                CeilDiv(pAV1Par->FrameHeight, SB_SIZE) % pAV1Par->NumTileRows,
                0
            );
            for (auto i = 0; i < pAV1Par->NumTileRows; i++)
                pAV1Par->TileHeightInSB[i] = CeilDiv(pAV1Par->FrameHeight, SB_SIZE) / pAV1Par->NumTileRows;

            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_ERR_NONE
            );

            // ContextUpdateTileID exceeds Tiles number
            mfxU16 numTileRows = pAV1Par->NumTileRows;
            pAV1Par->ContextUpdateTileIdPlus1 = static_cast<mfxU8>(pAV1Par->NumTileColumns * pAV1Par->NumTileRows) + 1;

            // NumTileRows could be infered from TileHeightInSB array
            pAV1Par->NumTileRows = 0;
            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            );

            // But NumTileRows will not be corrected here
            ASSERT_EQ(pAV1Par->NumTileRows, 0);
            pAV1Par->NumTileRows = numTileRows;

            ASSERT_EQ(
                pAV1Par->ContextUpdateTileIdPlus1,
                pAV1Par->NumTileColumns * pAV1Par->NumTileRows
            );
        }

        TEST_F(FeatureBlocksTileMultiTile, SetDefaults)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_SetDefaults>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetDefaults });
            auto& vp = Glob::VideoParam::Get(strg);

            mfxExtAV1Param* pAV1Par = ExtBuffer::Get(vp);
            mfxU16 numTileColumns = pAV1Par->NumTileColumns;
            mfxU16 numTileRows = pAV1Par->NumTileRows;

            pAV1Par->NumTileColumns = 0;
            pAV1Par->NumTileRows = 0;

            block->Call(vp, strg, strg);

            ASSERT_EQ(pAV1Par->NumTileColumns, numTileColumns);
            ASSERT_EQ(pAV1Par->NumTileRows, numTileRows);
        }

        TEST_F(FeatureBlocksTileMultiTile, SetTileInfo)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileInfo });
            auto& vp = Glob::VideoParam::Get(strg);

            mfxExtAV1Param* pAV1Par = ExtBuffer::Get(vp);
            const auto& fh = Glob::FH::GetOrConstruct(strg);
            pAV1Par->ContextUpdateTileIdPlus1 = 3;

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                fh.sbCols,
                CeilDiv(pAV1Par->FrameWidth, SB_SIZE)
            );

            ASSERT_EQ(
                fh.sbRows,
                CeilDiv(pAV1Par->FrameHeight, SB_SIZE)
            );

            ASSERT_EQ(fh.tile_info.TileCols, pAV1Par->NumTileColumns);

            for (auto i = 0; i < pAV1Par->NumTileColumns; i++)
            {
                ASSERT_EQ(fh.tile_info.TileWidthInSB[i], pAV1Par->TileWidthInSB[i]);
            }

            ASSERT_EQ(fh.tile_info.TileRows, pAV1Par->NumTileRows);

            for (auto i = 0; i < pAV1Par->NumTileRows; i++)
            {
                ASSERT_EQ(fh.tile_info.TileHeightInSB[i], pAV1Par->TileHeightInSB[i]);
            }

            ASSERT_EQ(
                fh.tile_info.context_update_tile_id,
                pAV1Par->ContextUpdateTileIdPlus1 - 1
            );

            ASSERT_EQ(fh.tile_info.TileColsLog2, 1);
            ASSERT_EQ(fh.tile_info.TileRowsLog2, 2);
            ASSERT_EQ(fh.tile_info.tileLimits.MaxTileHeightSb, 17); // Calculated by algorithm in Av1 spec
        }

        TEST_F(FeatureBlocksTileMultiTile, SetTileGroups)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileGroups });

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            ASSERT_TRUE(strg.Contains(Glob::TileGroups::Key));

            const auto& infos = Glob::TileGroups::Get(strg);
            ASSERT_EQ(
                infos.size(),
                1
            );

            ASSERT_EQ(infos[0].TgStart, 0);

            const auto& fh = Glob::FH::Get(strg);
            ASSERT_EQ(
                infos[0].TgEnd,
                fh.tile_info.TileCols * fh.tile_info.TileRows - 1
            );
        }

        TEST_F(FeatureBlocksTileMultiTile, SetMultiTileGroups)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileGroups });

            auto& vp = Glob::VideoParam::Get(strg);
            mfxExtAV1Param* pAV1Par = ExtBuffer::Get(vp);
            pAV1Par->NumTileGroups = 3;

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            ASSERT_TRUE(strg.Contains(Glob::TileGroups::Key));

            const auto& infos = Glob::TileGroups::Get(strg);
            ASSERT_EQ(
                infos.size(),
                pAV1Par->NumTileGroups
            );

            const TileGroupInfos tileGroups = { {0, 1}, {2, 3}, {4, 7} };
            for (auto i = 0; i < infos.size(); i++)
            {
                ASSERT_EQ(infos[i].TgStart, tileGroups[i].TgStart);
                ASSERT_EQ(infos[i].TgEnd, tileGroups[i].TgEnd);
            }
        }
    }
}
