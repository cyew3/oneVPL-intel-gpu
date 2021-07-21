// Copyright (c) 2020-2021 Intel Corporation
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
#include "av1ehw_base_tile.h"
#include "mfx_utils.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

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

            void InitExtBuffers(ExtBuffer::Param<mfxVideoParam>& vp)
            {
                vp.NewEB(MFX_EXTBUFF_AV1_RESOLUTION_PARAM, false);
                vp.NewEB(MFX_EXTBUFF_AV1_TILE_PARAM, false);
                vp.NewEB(MFX_EXTBUFF_AV1_AUXDATA, false);

                mfxExtAV1ResolutionParam& rsPar = ExtBuffer::Get(vp);
                rsPar.FrameWidth                = 7680;
                rsPar.FrameHeight               = 4320;
            }
        };

        FeatureBlocks FeatureBlocksTileSingleTile::blocks{};
        Tile FeatureBlocksTileSingleTile::tile(FEATURE_TILE);
        StorageRW FeatureBlocksTileSingleTile::strg{};

        TEST_F(FeatureBlocksTileSingleTile, CheckAndFix)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_CheckAndFix });
            auto&       vp    = Glob::VideoParam::Get(strg);

            InitExtBuffers(vp);
            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_ERR_NONE
            );
        }

        TEST_F(FeatureBlocksTileSingleTile, SetDefaults)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_SetDefaults>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetDefaults });
            auto&       vp    = Glob::VideoParam::Get(strg);

            block->Call(vp, strg, strg);

            mfxExtAV1ResolutionParam& rsPar = ExtBuffer::Get(vp);
            mfxExtAV1TileParam& tilePar     = ExtBuffer::Get(vp);
            mfxExtAV1AuxData&   auxPar      = ExtBuffer::Get(vp);
            ASSERT_EQ(tilePar.NumTileColumns, 1);
            ASSERT_EQ(tilePar.NumTileRows, 1);
            ASSERT_EQ(auxPar.UniformTileSpacing, MFX_CODINGOPTION_ON);

            ASSERT_EQ(
                auxPar.TileWidthInSB[0],
                mfx::CeilDiv(rsPar.FrameWidth, SB_SIZE)
            );

            ASSERT_EQ(
                auxPar.TileHeightInSB[0],
                mfx::CeilDiv(rsPar.FrameHeight, SB_SIZE)
            );

            ASSERT_EQ(
                auxPar.ContextUpdateTileIdPlus1,
                tilePar.NumTileColumns * tilePar.NumTileRows
            );
        }

        TEST_F(FeatureBlocksTileSingleTile, SetTileInfo)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileInfo });
            const auto& vp    = Glob::VideoParam::Get(strg);

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_UNDEFINED_BEHAVIOR
            );

            const auto& fh = Glob::FH::GetOrConstruct(strg);
            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            const mfxExtAV1TileParam& tilePar = ExtBuffer::Get(vp);
            const mfxExtAV1AuxData&   auxPar  = ExtBuffer::Get(vp);
            ASSERT_EQ(fh.tile_info.TileCols, tilePar.NumTileColumns);

            for (auto i = 0; i < tilePar.NumTileColumns; i++)
            {
                ASSERT_EQ(fh.tile_info.TileWidthInSB[i], auxPar.TileWidthInSB[i]);
            }

            ASSERT_EQ(fh.tile_info.TileRows, tilePar.NumTileRows);

            for (auto i = 0; i < tilePar.NumTileRows; i++)
            {
                ASSERT_EQ(fh.tile_info.TileHeightInSB[i], auxPar.TileHeightInSB[i]);
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

            const auto &infos = Glob::TileGroups::Get(strg);
            ASSERT_EQ(
                infos.size(),
                1
            );

            ASSERT_EQ(infos[0].TgStart, 0);

            const auto &fh = Glob::FH::Get(strg);
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

            void InitExtBuffers(ExtBuffer::Param<mfxVideoParam>& vp)
            {
                vp.NewEB(MFX_EXTBUFF_AV1_RESOLUTION_PARAM, false);
                vp.NewEB(MFX_EXTBUFF_AV1_TILE_PARAM, false);
                vp.NewEB(MFX_EXTBUFF_AV1_AUXDATA, false);

                mfxExtAV1ResolutionParam& rsPar = ExtBuffer::Get(vp);
                rsPar.FrameWidth                = 7680;
                rsPar.FrameHeight               = 4320;

                mfxExtAV1AuxData& auxPar        = ExtBuffer::Get(vp);
                auxPar.UniformTileSpacing       = MFX_CODINGOPTION_ON;

                mfxExtAV1TileParam& tilePar     = ExtBuffer::Get(vp);
                tilePar.NumTileColumns          = 2;
                tilePar.NumTileRows             = 2;

                ASSERT_EQ(
                    mfx::CeilDiv(rsPar.FrameWidth, SB_SIZE) % tilePar.NumTileColumns,
                    0
                );

                ASSERT_EQ(
                    mfx::CeilDiv(rsPar.FrameHeight, SB_SIZE) % tilePar.NumTileRows,
                    0
                );

                for (auto i = 0; i < tilePar.NumTileColumns; i++)
                    auxPar.TileWidthInSB[i] = mfx::CeilDiv(rsPar.FrameWidth, SB_SIZE) / tilePar.NumTileColumns;

                for (auto i = 0; i < tilePar.NumTileRows; i++)
                    auxPar.TileHeightInSB[i] = mfx::CeilDiv(rsPar.FrameHeight, SB_SIZE) / tilePar.NumTileRows;
            }
        };

        FeatureBlocks FeatureBlocksTileMultiTile::blocks{};
        Tile FeatureBlocksTileMultiTile::tile(FEATURE_TILE);
        StorageRW FeatureBlocksTileMultiTile::strg{};

        TEST_F(FeatureBlocksTileMultiTile, CheckTileWithParam)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_CheckAndFix });
            auto&       vp    = Glob::VideoParam::Get(strg);

            InitExtBuffers(vp);
            mfxExtAV1AuxData& auxPar = ExtBuffer::Get(vp);

            // Sum of Tile heights not equals to image heights
            auxPar.TileHeightInSB[1]++;
            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_ERR_UNSUPPORTED
            );

            // Maximum height exceeds spec limitation
            InitExtBuffers(vp);
            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_ERR_UNSUPPORTED
            );

            InitExtBuffers(vp);
            mfxExtAV1ResolutionParam& rsPar   = ExtBuffer::Get(vp);
            mfxExtAV1TileParam&       tilePar = ExtBuffer::Get(vp);
            tilePar.NumTileRows               = 4;

            ASSERT_EQ(
                mfx::CeilDiv(rsPar.FrameHeight, SB_SIZE) % tilePar.NumTileRows,
                0
            );

            for (auto i = 0; i < tilePar.NumTileRows; i++)
                auxPar.TileHeightInSB[i] = mfx::CeilDiv(rsPar.FrameHeight, SB_SIZE) / tilePar.NumTileRows;

            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_ERR_NONE
            );

            // ContextUpdateTileID exceeds Tiles number
            mfxU16 numTileRows              = tilePar.NumTileRows;
            auxPar.ContextUpdateTileIdPlus1 = static_cast<mfxU8>(tilePar.NumTileColumns * tilePar.NumTileRows) + 1;

            // NumTileRows could be infered from TileHeightInSB array
            tilePar.NumTileRows = 0;
            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            );

            // But NumTileRows will not be corrected here
            ASSERT_EQ(tilePar.NumTileRows, 0);
            tilePar.NumTileRows = numTileRows;

            ASSERT_EQ(
                auxPar.ContextUpdateTileIdPlus1,
                tilePar.NumTileColumns * tilePar.NumTileRows
            );
        }

        TEST_F(FeatureBlocksTileMultiTile, SetDefaults)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_SetDefaults>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetDefaults });
            auto&       vp    = Glob::VideoParam::Get(strg);

            mfxExtAV1TileParam& tilePar = ExtBuffer::Get(vp);
            mfxU16 numTileColumns       = tilePar.NumTileColumns;
            mfxU16 numTileRows          = tilePar.NumTileRows;
            tilePar.NumTileColumns      = 0;
            tilePar.NumTileRows         = 0;

            block->Call(vp, strg, strg);

            ASSERT_EQ(tilePar.NumTileColumns, numTileColumns);
            ASSERT_EQ(tilePar.NumTileRows, numTileRows);
        }

        TEST_F(FeatureBlocksTileMultiTile, SetTileInfo)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileInfo });
            auto&       vp    = Glob::VideoParam::Get(strg);

            mfxExtAV1ResolutionParam& rsPar   = ExtBuffer::Get(vp);
            mfxExtAV1TileParam&       tilePar = ExtBuffer::Get(vp);
            mfxExtAV1AuxData&         auxPar  = ExtBuffer::Get(vp);
            const auto&               fh      = Glob::FH::GetOrConstruct(strg);
            auxPar.ContextUpdateTileIdPlus1   = 3;

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                fh.sbCols,
                mfx::CeilDiv(rsPar.FrameWidth, SB_SIZE)
            );

            ASSERT_EQ(
                fh.sbRows,
                mfx::CeilDiv(rsPar.FrameHeight, SB_SIZE)
            );

            ASSERT_EQ(fh.tile_info.TileCols, tilePar.NumTileColumns);

            for (auto i = 0; i < tilePar.NumTileColumns; i++)
            {
                ASSERT_EQ(fh.tile_info.TileWidthInSB[i], auxPar.TileWidthInSB[i]);
            }

            ASSERT_EQ(fh.tile_info.TileRows, tilePar.NumTileRows);

            for (auto i = 0; i < tilePar.NumTileRows; i++)
            {
                ASSERT_EQ(fh.tile_info.TileHeightInSB[i], auxPar.TileHeightInSB[i]);
            }

            ASSERT_EQ(
                fh.tile_info.context_update_tile_id,
                auxPar.ContextUpdateTileIdPlus1 - 1
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

            const auto &fh = Glob::FH::Get(strg);
            ASSERT_EQ(
                infos[0].TgEnd,
                fh.tile_info.TileCols * fh.tile_info.TileRows - 1
            );
        }

        TEST_F(FeatureBlocksTileMultiTile, SetMultiTileGroups)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileGroups });
            auto&       vp    = Glob::VideoParam::Get(strg);

            mfxExtAV1TileParam& tilePar = ExtBuffer::Get(vp);
            tilePar.NumTileGroups       = 3;

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            ASSERT_TRUE(strg.Contains(Glob::TileGroups::Key));

            const auto& infos = Glob::TileGroups::Get(strg);
            ASSERT_EQ(
                infos.size(),
                tilePar.NumTileGroups
            );

            const TileGroupInfos tileGroups = { {0, 1}, {2, 3}, {4, 7} };
            for (auto i = 0; i < infos.size(); i++)
            {
                ASSERT_EQ(infos[i].TgStart, tileGroups[i].TgStart);
                ASSERT_EQ(infos[i].TgEnd, tileGroups[i].TgEnd);
            }
        }

        struct FeatureBlocksTileSingleTileSuperres
            : testing::Test
        {
            static FeatureBlocks   blocks;
            static Tile            tile;
            static StorageRW       strg;

            const static mfxU32 upscaledWidth = 7680;
            static mfxU32 downscaledWidth;

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

            void InitExtBuffers(ExtBuffer::Param<mfxVideoParam>& vp)
            {
                vp.NewEB(MFX_EXTBUFF_AV1_RESOLUTION_PARAM, false);
                vp.NewEB(MFX_EXTBUFF_AV1_TILE_PARAM, false);
                vp.NewEB(MFX_EXTBUFF_AV1_AUXDATA, false);

                mfxExtAV1ResolutionParam& rsPar  = ExtBuffer::Get(vp);
                rsPar.FrameWidth                 = 7680;
                rsPar.FrameHeight                = 4320;

                mfxExtAV1AuxData& auxPar         = ExtBuffer::Get(vp);
                auxPar.EnableSuperres            = MFX_CODINGOPTION_ON;
                auxPar.SuperresScaleDenominator  = 11;
                downscaledWidth                  = GetActualEncodeWidth(upscaledWidth, &auxPar);
            }
        };

        FeatureBlocks FeatureBlocksTileSingleTileSuperres::blocks{};
        Tile FeatureBlocksTileSingleTileSuperres::tile(FEATURE_TILE);
        StorageRW FeatureBlocksTileSingleTileSuperres::strg{};
        mfxU32 FeatureBlocksTileSingleTileSuperres::downscaledWidth = 0;

        TEST_F(FeatureBlocksTileSingleTileSuperres, CheckAndFix)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_CheckAndFix });
            auto&       vp    = Glob::VideoParam::Get(strg);

            InitExtBuffers(vp);
            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_ERR_NONE
            );
        }

        TEST_F(FeatureBlocksTileSingleTileSuperres, SetDefaults)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_SetDefaults>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetDefaults });
            auto&       vp    = Glob::VideoParam::Get(strg);

            block->Call(vp, strg, strg);

            mfxExtAV1TileParam& tilePar = ExtBuffer::Get(vp);
            ASSERT_EQ(tilePar.NumTileColumns, 1);
            ASSERT_EQ(tilePar.NumTileRows, 1);

            mfxExtAV1AuxData& auxPar = ExtBuffer::Get(vp);
            ASSERT_EQ(auxPar.UniformTileSpacing, MFX_CODINGOPTION_ON);

            ASSERT_EQ(
                auxPar.TileWidthInSB[0],
                mfx::CeilDiv(downscaledWidth, SB_SIZE)
            );

            mfxExtAV1ResolutionParam& rsPar = ExtBuffer::Get(vp);
            ASSERT_EQ(
                auxPar.TileHeightInSB[0],
                mfx::CeilDiv(rsPar.FrameHeight, SB_SIZE)
            );

            ASSERT_EQ(
                auxPar.ContextUpdateTileIdPlus1,
                tilePar.NumTileColumns * tilePar.NumTileRows
            );
        }

        TEST_F(FeatureBlocksTileSingleTileSuperres, SetTileInfo)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileInfo });
            const auto& vp    = Glob::VideoParam::Get(strg);

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_UNDEFINED_BEHAVIOR
            );

            const auto& fh = Glob::FH::GetOrConstruct(strg);
            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            const mfxExtAV1TileParam& tilePar = ExtBuffer::Get(vp);
            const mfxExtAV1AuxData&   auxPar  = ExtBuffer::Get(vp);

            ASSERT_EQ(fh.tile_info.TileCols, tilePar.NumTileColumns);

            for (auto i = 0; i < tilePar.NumTileColumns; i++)
            {
                ASSERT_EQ(fh.tile_info.TileWidthInSB[i], auxPar.TileWidthInSB[i]);
            }

            ASSERT_EQ(fh.tile_info.TileRows, tilePar.NumTileRows);

            for (auto i = 0; i < tilePar.NumTileRows; i++)
            {
                ASSERT_EQ(fh.tile_info.TileHeightInSB[i], auxPar.TileHeightInSB[i]);
            }
        }

        TEST_F(FeatureBlocksTileSingleTileSuperres, SetTileGroups)
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

        struct FeatureBlocksTileMultiTileSuperres
            : testing::Test
        {
            static FeatureBlocks   blocks;
            static Tile            tile;
            static StorageRW       strg;

            const static mfxU32 upscaledWidth = 7680;
            static mfxU32 downscaledWidth;

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

            void InitExtBuffers(ExtBuffer::Param<mfxVideoParam>& vp)
            {
                vp.NewEB(MFX_EXTBUFF_AV1_RESOLUTION_PARAM, false);
                vp.NewEB(MFX_EXTBUFF_AV1_TILE_PARAM, false);
                vp.NewEB(MFX_EXTBUFF_AV1_AUXDATA, false);

                mfxExtAV1ResolutionParam& rsPar  = ExtBuffer::Get(vp);
                rsPar.FrameWidth                 = upscaledWidth;
                rsPar.FrameHeight                = 4320;

                mfxExtAV1AuxData& auxPar         = ExtBuffer::Get(vp);
                auxPar.UniformTileSpacing        = MFX_CODINGOPTION_ON;
                auxPar.EnableSuperres            = MFX_CODINGOPTION_ON;
                auxPar.SuperresScaleDenominator  = 16;
                downscaledWidth                  = GetActualEncodeWidth(upscaledWidth, &auxPar);

                mfxExtAV1TileParam& tilePar      = ExtBuffer::Get(vp);
                tilePar.NumTileColumns           = 2;
                tilePar.NumTileRows              = 2;

                ASSERT_EQ(
                    mfx::CeilDiv(downscaledWidth, SB_SIZE) % tilePar.NumTileColumns,
                    0
                );

                ASSERT_EQ(
                    mfx::CeilDiv(rsPar.FrameHeight, SB_SIZE) % tilePar.NumTileRows,
                    0
                );

                for (auto i = 0; i < tilePar.NumTileColumns; i++)
                    auxPar.TileWidthInSB[i] = mfx::CeilDiv(downscaledWidth, SB_SIZE) / tilePar.NumTileColumns;

                for (auto i = 0; i < tilePar.NumTileRows; i++)
                    auxPar.TileHeightInSB[i] = mfx::CeilDiv(rsPar.FrameHeight, SB_SIZE) / tilePar.NumTileRows;
            }
        };

        FeatureBlocks FeatureBlocksTileMultiTileSuperres::blocks{};
        Tile FeatureBlocksTileMultiTileSuperres::tile(FEATURE_TILE);
        StorageRW FeatureBlocksTileMultiTileSuperres::strg{};
        mfxU32 FeatureBlocksTileMultiTileSuperres::downscaledWidth = 0;

        TEST_F(FeatureBlocksTileMultiTileSuperres, CheckTileWithParam)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_CheckAndFix });
            auto&       vp    = Glob::VideoParam::Get(strg);

            // Maximum height exceeds spec limitation, but after superres, it's within the limitation
            InitExtBuffers(vp);
            ASSERT_EQ(
                block->Call(vp, vp, strg),
                MFX_ERR_NONE
            );
        }

        TEST_F(FeatureBlocksTileMultiTileSuperres, SetDefaults)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_SetDefaults>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetDefaults });
            auto&       vp    = Glob::VideoParam::Get(strg);

            mfxExtAV1TileParam& tilePar        = ExtBuffer::Get(vp);
            mfxU16              numTileColumns = tilePar.NumTileColumns;
            mfxU16              numTileRows    = tilePar.NumTileRows;

            tilePar.NumTileColumns = 0;
            tilePar.NumTileRows    = 0;

            block->Call(vp, strg, strg);

            ASSERT_EQ(tilePar.NumTileColumns, numTileColumns);
            ASSERT_EQ(tilePar.NumTileRows, numTileRows);
        }

        TEST_F(FeatureBlocksTileMultiTileSuperres, SetTileInfo)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileInfo });
            auto&       vp    = Glob::VideoParam::Get(strg);

            mfxExtAV1ResolutionParam& rsPar   = ExtBuffer::Get(vp);
            mfxExtAV1TileParam&       tilePar = ExtBuffer::Get(vp);
            mfxExtAV1AuxData&         auxPar  = ExtBuffer::Get(vp);
            const auto&               fh      = Glob::FH::GetOrConstruct(strg);
            auxPar.ContextUpdateTileIdPlus1   = 3;

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            ASSERT_EQ(
                fh.sbCols,
                mfx::CeilDiv(downscaledWidth, SB_SIZE)
            );

            ASSERT_EQ(
                fh.sbRows,
                mfx::CeilDiv(rsPar.FrameHeight, SB_SIZE)
            );

            ASSERT_EQ(fh.tile_info.TileCols, tilePar.NumTileColumns);

            for (auto i = 0; i < tilePar.NumTileColumns; i++)
            {
                ASSERT_EQ(fh.tile_info.TileWidthInSB[i], auxPar.TileWidthInSB[i]);
            }

            ASSERT_EQ(fh.tile_info.TileRows, tilePar.NumTileRows);

            for (auto i = 0; i < tilePar.NumTileRows; i++)
            {
                ASSERT_EQ(fh.tile_info.TileHeightInSB[i], auxPar.TileHeightInSB[i]);
            }

            ASSERT_EQ(
                fh.tile_info.context_update_tile_id,
                auxPar.ContextUpdateTileIdPlus1 - 1
            );

            ASSERT_EQ(fh.tile_info.TileColsLog2, 1);
            ASSERT_EQ(fh.tile_info.TileRowsLog2, 1);
            ASSERT_EQ(fh.tile_info.tileLimits.MaxTileHeightSb, 34); // Calculated by algorithm in Av1 spec
        }

        TEST_F(FeatureBlocksTileMultiTileSuperres, SetTileGroups)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileGroups });

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            ASSERT_TRUE(strg.Contains(Glob::TileGroups::Key));

            const auto &infos = Glob::TileGroups::Get(strg);
            ASSERT_EQ(
                infos.size(),
                1
            );

            ASSERT_EQ(infos[0].TgStart, 0);

            const auto &fh = Glob::FH::Get(strg);
            ASSERT_EQ(
                infos[0].TgEnd,
                fh.tile_info.TileCols * fh.tile_info.TileRows - 1
            );
        }

        TEST_F(FeatureBlocksTileMultiTileSuperres, SetMultiTileGroups)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_TILE, Tile::BLK_SetTileGroups });
            auto&       vp    = Glob::VideoParam::Get(strg);

            mfxExtAV1TileParam& tilePar = ExtBuffer::Get(vp);
            tilePar.NumTileGroups       = 3;

            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );

            ASSERT_TRUE(strg.Contains(Glob::TileGroups::Key));

            const auto &infos = Glob::TileGroups::Get(strg);
            ASSERT_EQ(
                infos.size(),
                tilePar.NumTileGroups
            );

            const TileGroupInfos tileGroups = { {0, 0}, {1, 1}, {2, 3} };
            for (auto i = 0; i < infos.size(); i++)
            {
                ASSERT_EQ(infos[i].TgStart, tileGroups[i].TgStart);
                ASSERT_EQ(infos[i].TgEnd, tileGroups[i].TgEnd);
            }
        }
    }
}
