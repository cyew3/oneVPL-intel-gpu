// Copyright (c) 2019-2021 Intel Corporation
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

#define _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "bs_parser++.h"

#include "av1ehw_base.h"
#include "av1ehw_base_packer.h"
#include "av1ehw_base_tile.h"

#include "mfx_utils.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

namespace av1e {
    namespace tests
    {
        const uint32_t IVF_SEQ_HEADER_SIZE_BYTES = 32;
        const uint32_t TEST_WIDTH                = 1920;
        const uint32_t TEST_HEIGHT               = 1080;
        const uint32_t TEST_QP                   = 50;

        struct FeatureBlocksPacker
            : testing::Test
        {
            FeatureBlocks blocks{};
            Packer        packer;
            Tile          tile;
            StorageRW     global;

            FeatureBlocksPacker()
                : packer(FEATURE_PACKER)
                , tile(FEATURE_TILE)
            {}

            void SetUp() override
            {
                packer.Init(INIT | RUNTIME, blocks);
                tile.Init(INIT | RUNTIME, blocks);
                Glob::EncodeCaps::GetOrConstruct(global);
            }
        };

        TEST_F(FeatureBlocksPacker, InitAlloc)
        {
            auto const& block = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks),
                { FEATURE_PACKER, Packer::BLK_Init });

            ASSERT_TRUE(!global.Contains(Glob::PackedHeaders::Key));
                EXPECT_EQ(
                block->Call(global, global),
                MFX_ERR_NONE
            );
            EXPECT_TRUE(global.Contains(Glob::PackedHeaders::Key));
        }

        TEST_F(FeatureBlocksPacker, SubmitTask)
        {
            Glob::PackedHeaders::GetOrConstruct(global);

            // creating param structs that are used by Packer
            auto& vp = Glob::VideoParam::GetOrConstruct(global);
            auto& sh = Glob::SH::GetOrConstruct(global);
            auto& fh = Glob::FH::GetOrConstruct(global);

            // filling param structs with mandatory params
            vp.mfx.FrameInfo.Width  = fh.UpscaledWidth = fh.FrameWidth = sh.max_frame_width = TEST_WIDTH;
            vp.mfx.FrameInfo.Height = fh.FrameHeight = sh.max_frame_height = TEST_HEIGHT;

            sh.enable_order_hint              = true;
            fh.frame_type                     = KEY_FRAME;
            fh.quantization_params.base_q_idx = TEST_QP;

            StorageRW taskStrg;
            auto& tpar          = Task::Common::GetOrConstruct(taskStrg);
            tpar.FrameType      = MFX_FRAMETYPE_I;
            tpar.InsertHeaders  = INSERT_IVF_SEQ | INSERT_IVF_FRM;

            vp.NewEB(MFX_EXTBUFF_AV1_RESOLUTION_PARAM, false);
            vp.NewEB(MFX_EXTBUFF_AV1_TILE_PARAM, false);
            vp.NewEB(MFX_EXTBUFF_AV1_AUXDATA, false);
            mfxExtAV1ResolutionParam& rsPar   = ExtBuffer::Get(vp);
            mfxExtAV1TileParam&       tilePar = ExtBuffer::Get(vp);
            mfxExtAV1AuxData&         auxPar  = ExtBuffer::Get(vp);
            rsPar.FrameWidth                  = TEST_WIDTH;
            rsPar.FrameHeight                 = TEST_HEIGHT;
            tilePar.NumTileColumns            = 1;
            tilePar.NumTileRows               = 1;
            auxPar.UniformTileSpacing         = 1;
            auxPar.TileWidthInSB[0]           = mfx::CeilDiv(mfxU16(TEST_WIDTH), mfxU16(SB_SIZE));
            auxPar.TileHeightInSB[0]          = mfx::CeilDiv(mfxU16(TEST_HEIGHT), mfxU16(SB_SIZE));

            auto const& setTileInfo = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks),
                { FEATURE_TILE, Tile::BLK_SetTileInfo });
            EXPECT_EQ(
                setTileInfo->Call(global, taskStrg),
                MFX_ERR_NONE
            );

            auto& taskFH = Task::FH::GetOrConstruct(taskStrg);
            taskFH       = fh;

            // calling Packer's SubmitTack routing
            auto const& block = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks),
                { FEATURE_PACKER, Packer::BLK_SubmitTask });
            EXPECT_EQ(
                block->Call(global, taskStrg),
                MFX_ERR_NONE
            );

            // requesting Packer's result (prepared frame's header)
            auto& ph = Glob::PackedHeaders::Get(global);

            // adding dummy frame size for the internal BSParser checks
            mfxU32 ivfFrameHeader[3] = { 10000, 0, 0x00000000 };
            mfxU8* pIVFPicHeader     = ph.IVF.pData + IVF_SEQ_HEADER_SIZE_BYTES;
            std::copy(std::begin(ivfFrameHeader), std::end(ivfFrameHeader), reinterpret_cast <mfxU32*> (pIVFPicHeader));

            // checkign header with BSParser
            BS_AV1_parser parser;
            parser.set_buffer(ph.IVF.pData, (ph.IVF.BitLen + ph.SPS.BitLen + ph.PPS.BitLen) / 8);

            BSErr parser_err = parser.parse_next_unit();

            // finally some checks that header params are expected values
            EXPECT_EQ(parser_err && (parser_err != BS_ERR_MORE_DATA), false);
            auto pHeader = static_cast<BS_AV1::Frame *>(parser.get_header());
            EXPECT_EQ(pHeader->fh.UpscaledWidth, TEST_WIDTH);
            EXPECT_EQ(pHeader->fh.FrameHeight, TEST_HEIGHT);
            EXPECT_EQ(pHeader->fh.quantization_params.base_q_idx, TEST_QP);
        }
    }
}
