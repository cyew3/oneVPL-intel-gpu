// Copyright (c) 2019-2020 Intel Corporation
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

using namespace AV1EHW;
using namespace AV1EHW::Base;

namespace av1e {
    namespace tests
    {
        #define IVF_SEQ_HEADER_SIZE_BYTES 32

        struct FeatureBlocksPacker
            : testing::Test
        {
            FeatureBlocks blocks{};
            Packer        packer;
            Tile          tile;
            StorageRW     storage;

            FeatureBlocksPacker()
                : packer(FEATURE_PACKER)
                , tile(FEATURE_TILE)
            {}

            void SetUp() override
            {
                packer.Init(INIT | RUNTIME, blocks);
                tile.Init(INIT | RUNTIME, blocks);
                Glob::EncodeCaps::GetOrConstruct(storage);
            }

            void InitFeature()
            {
                auto& queueQIS = FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks);
                auto block = FeatureBlocks::Get(queueQIS, { FEATURE_PACKER, Packer::BLK_Init });
                ASSERT_EQ(
                    block->Call(storage, storage),
                    MFX_ERR_NONE
                );

                const bool is_header_key_found_in_storage = storage.Contains(Glob::PackedHeaders::Key);
                EXPECT_EQ(
                    is_header_key_found_in_storage, true
                );
            }
        };

        TEST_F(FeatureBlocksPacker, InitAlloc)
        {
            InitFeature();
        }

        TEST_F(FeatureBlocksPacker, SubmitTask)
        {
            // initing Packer if not already inited
            const bool is_header_key_found_in_storage = storage.Contains(Glob::PackedHeaders::Key);
            if (!is_header_key_found_in_storage)
            {
                InitFeature();
            }

            // creating param structs that are used by Packer
            auto p_video_par = MfxFeatureBlocks::make_storable<ExtBuffer::Wrapper<mfxVideoParam>>();
            auto p_sh = MfxFeatureBlocks::make_storable<SH>();
            auto p_fh = MfxFeatureBlocks::make_storable<FH>();

            // filling param structs with mandatory params
            const mfxU16 width = p_fh->FrameWidth = p_sh->max_frame_width = 1920;
            p_video_par->mfx.FrameInfo.Width = static_cast<uint16_t>(width);
            const mfxU16 height = p_fh->FrameHeight = p_sh->max_frame_height = 1080;
            p_video_par->mfx.FrameInfo.Height = static_cast<uint16_t>(height);
            p_sh->enable_order_hint = true;
            p_fh->frame_type = KEY_FRAME;
            const uint32_t base_q_idx = p_fh->quantization_params.base_q_idx = 50;

            // putting param structs to the storage to let Packer use them
            storage.Insert(Glob::FH::Key, std::move(p_fh));
            storage.Insert(Glob::SH::Key, std::move(p_sh));

            StorageRW storage_task;
            auto p_task_par = MfxFeatureBlocks::make_storable<TaskCommonPar>();
            p_task_par->FrameType = MFX_FRAMETYPE_I;
            p_task_par->InsertHeaders = INSERT_IVF_SEQ | INSERT_IVF_FRM;
            storage_task.Insert(Task::Common::Key, std::move(p_task_par));

            p_video_par->Attach(MFX_EXTBUFF_AV1_PARAM, false);
            mfxExtAV1Param& av1Par = ExtBuffer::Get(*p_video_par);

            av1Par.UniformTileSpacing = 1;
            av1Par.FrameHeight = height;
            av1Par.FrameWidth = width;
            av1Par.NumTileColumns = 1;
            av1Par.NumTileRows = 1;
            av1Par.TileWidthInSB[0] = CeilDiv(width, static_cast<mfxU16>(SB_SIZE));
            av1Par.TileHeightInSB[0] = CeilDiv(height, static_cast<mfxU16>(SB_SIZE));

            storage.Insert(Glob::VideoParam::Key, std::move(p_video_par));

            auto& queueII = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            auto& setTileInfo = FeatureBlocks::Get(queueII, { FEATURE_TILE, Tile::BLK_SetTileInfo });
            ASSERT_EQ(
                setTileInfo->Call(storage, storage_task),
                MFX_ERR_NONE
            );

            auto p_task_fh = MfxFeatureBlocks::make_storable<FH>(Glob::FH::Get(storage));
            storage_task.Insert(Task::FH::Key, std::move(p_task_fh));

            // calling Packer's SubmitTack routing
            auto& queueST = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
            auto& block = FeatureBlocks::Get(queueST, { FEATURE_PACKER, Packer::BLK_SubmitTask });
            ASSERT_EQ(
                block->Call(storage, storage_task),
                MFX_ERR_NONE
            );

            // requesting Packer's result (prepared frame's header)
            auto& ph = Glob::PackedHeaders::Get(storage);

            // adding dummy frame size for the internal BSParser checks
            mfxU32 ivfFrameHeader[3] = { 10000, 0, 0x00000000 };
            mfxU8 * pIVFPicHeader = ph.IVF.pData + IVF_SEQ_HEADER_SIZE_BYTES;
            std::copy(std::begin(ivfFrameHeader), std::end(ivfFrameHeader), reinterpret_cast <mfxU32*> (pIVFPicHeader));

            // checkign header with BSParser
            BS_AV1_parser parser;
            parser.set_buffer(ph.IVF.pData, (ph.IVF.BitLen + ph.SPS.BitLen + ph.PPS.BitLen) / 8);

            BSErr parser_err = parser.parse_next_unit();

            // finally some checks that header params are expected values
            ASSERT_EQ(parser_err && (parser_err != BS_ERR_MORE_DATA), false);
            auto pHeader = static_cast<BS_AV1::Frame *>(parser.get_header());
            ASSERT_EQ(pHeader->fh.FrameWidth, width);
            ASSERT_EQ(pHeader->fh.FrameHeight, height);
            ASSERT_EQ(pHeader->fh.quantization_params.base_q_idx, base_q_idx);
        }
    }
}