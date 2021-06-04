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
#include "av1ehw_base_alloc.h"
#include "av1ehw_base_superres.h"

#include "libmfx_core_factory.h"

#include <cstdlib>

using namespace AV1EHW;
using namespace AV1EHW::Base;

namespace av1e {
    namespace tests
    {
        struct FeatureBlocksSuperres
            : testing::Test
        {
            FeatureBlocks   blocks{};
            Superres        superres;
            StorageRW       storage;
            VideoCORE*      core = nullptr;

            FeatureBlocksSuperres()
                : superres(FEATURE_SUPERRES)
            {}

            void SetUp() override
            {
                srand(time(nullptr));
                eMFXVAType type;
#if defined(MFX_VA_WIN)
                type = MFX_HW_D3D11;
#else
                type = MFX_HW_VAAPI;
#endif
                core = FactoryCORE::CreateCORE(type, 0, 0, nullptr);
                storage.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(*core));

                superres.Init(INIT | RUNTIME, blocks);

                auto& vp = Glob::VideoParam::GetOrConstruct(storage);
                vp.AsyncDepth = 1;
                vp.mfx.GopPicSize = 1;
                vp.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
                vp.mfx.FrameInfo.Width = 4096;
                vp.mfx.FrameInfo.Height = 2160;

                vp.NewEB(MFX_EXTBUFF_AV1_PARAM, false);

                mfxExtAV1Param& av1Par = ExtBuffer::Get(vp);
                av1Par.FrameWidth = vp.mfx.FrameInfo.Width;
                av1Par.FrameHeight = vp.mfx.FrameInfo.Height;

                auto p_sh = MfxFeatureBlocks::make_storable<SH>();
                auto p_fh = MfxFeatureBlocks::make_storable<FH>();
                storage.Insert(Glob::SH::Key, std::move(p_sh));
                storage.Insert(Glob::FH::Key, std::move(p_fh));

                auto &caps = Glob::EncodeCaps::GetOrConstruct(storage);
                caps.SuperResSupport = 1;
            }
        };

        TEST_F(FeatureBlocksSuperres, CheckAndFix)
        {
            auto& vp = Glob::VideoParam::Get(storage);
            mfxExtAV1Param& av1Par = ExtBuffer::Get(vp);

            auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
            auto blk = FeatureBlocks::Get(queue, { FEATURE_SUPERRES, Superres::BLK_CheckAndFix });

            av1Par.EnableSuperres = MFX_CODINGOPTION_ON;
            av1Par.SuperresScaleDenominator = 8;
            ASSERT_EQ(
                blk->Call(vp, vp, storage),
                MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            );
            ASSERT_EQ(
                av1Par.SuperresScaleDenominator,
                16);

            av1Par.EnableSuperres = MFX_CODINGOPTION_ON;
            av1Par.SuperresScaleDenominator = 17;
            ASSERT_EQ(
                blk->Call(vp, vp, storage),
                MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            );
            ASSERT_EQ(
                av1Par.SuperresScaleDenominator,
                16);

            av1Par.EnableSuperres = MFX_CODINGOPTION_OFF;
            av1Par.SuperresScaleDenominator = 5;
            ASSERT_EQ(
                blk->Call(vp, vp, storage),
                MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            );
            ASSERT_EQ(
                av1Par.SuperresScaleDenominator,
                8);

            av1Par.EnableSuperres = MFX_CODINGOPTION_ON;
            av1Par.SuperresScaleDenominator = 9;
            ASSERT_EQ(
                blk->Call(vp, vp, storage),
                MFX_ERR_NONE
            );
            ASSERT_EQ(
                av1Par.SuperresScaleDenominator,
                9);

            av1Par.EnableSuperres = MFX_CODINGOPTION_OFF;
            av1Par.SuperresScaleDenominator = 8;
            ASSERT_EQ(
                blk->Call(vp, vp, storage),
                MFX_ERR_NONE
            );
            ASSERT_EQ(
                av1Par.SuperresScaleDenominator,
                8);
        }

        TEST_F(FeatureBlocksSuperres, SetDefaults)
        {
            auto& vp = Glob::VideoParam::Get(storage);
            mfxExtAV1Param& av1Par = ExtBuffer::Get(vp);

            auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_SetDefaults>::Get(blocks);
            auto blk = FeatureBlocks::Get(queue, { FEATURE_SUPERRES, Superres::BLK_SetDefaults });

            av1Par.EnableSuperres = 0;
            av1Par.SuperresScaleDenominator = 0;
            blk->Call(vp, storage, storage);
            ASSERT_EQ(
                av1Par.EnableSuperres,
                MFX_CODINGOPTION_OFF);
            ASSERT_EQ(
                av1Par.SuperresScaleDenominator,
                8);

            av1Par.EnableSuperres = MFX_CODINGOPTION_ON;
            av1Par.SuperresScaleDenominator = 0;
            blk->Call(vp, storage, storage);
            ASSERT_EQ(
                av1Par.EnableSuperres,
                MFX_CODINGOPTION_ON);
            ASSERT_EQ(
                av1Par.SuperresScaleDenominator,
                16);
        }

        TEST_F(FeatureBlocksSuperres, SetSH)
        {
            auto& vp = Glob::VideoParam::Get(storage);
            mfxExtAV1Param& av1Par = ExtBuffer::Get(vp);
            auto& sh = Glob::SH::Get(storage);

            auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            auto blk = FeatureBlocks::Get(queue, { FEATURE_SUPERRES, Superres::BLK_SetSH });

            uint32_t superresEn = std::rand() % 2;
            av1Par.EnableSuperres = (mfxU8)(superresEn ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
            ASSERT_EQ(
                blk->Call(storage, storage),
                MFX_ERR_NONE
            );
            ASSERT_EQ(
                sh.enable_superres,
                superresEn
            );
        }

        TEST_F(FeatureBlocksSuperres, SetFH)
        {
            auto& vp = Glob::VideoParam::Get(storage);
            mfxExtAV1Param& av1Par = ExtBuffer::Get(vp);
            auto& fh = Glob::FH::Get(storage);

            auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            auto blk = FeatureBlocks::Get(queue, { FEATURE_SUPERRES, Superres::BLK_SetFH });

            av1Par.EnableSuperres = MFX_CODINGOPTION_ON;
            av1Par.SuperresScaleDenominator = 11;
            ASSERT_EQ(
                blk->Call(storage, storage),
                MFX_ERR_NONE
            );
            ASSERT_EQ(
                fh.use_superres,
                1
            );
            ASSERT_EQ(
                fh.frame_size_override_flag,
                1
            );
            ASSERT_EQ(
                fh.SuperresDenom,
                11
            );
            ASSERT_EQ(
                fh.UpscaledWidth,
                av1Par.FrameWidth
            );
        }
    }
}
