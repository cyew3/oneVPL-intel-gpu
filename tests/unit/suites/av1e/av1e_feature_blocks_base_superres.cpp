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

                vp.NewEB(MFX_EXTBUFF_AV1_RESOLUTION_PARAM, false);
                vp.NewEB(MFX_EXTBUFF_AV1_AUXDATA, false);

                mfxExtAV1ResolutionParam& rsPar = ExtBuffer::Get(vp);
                rsPar.FrameWidth                = vp.mfx.FrameInfo.Width;
                rsPar.FrameHeight               = vp.mfx.FrameInfo.Height;

                auto pSH = MfxFeatureBlocks::make_storable<SH>();
                auto pFH = MfxFeatureBlocks::make_storable<FH>();
                storage.Insert(Glob::SH::Key, std::move(pSH));
                storage.Insert(Glob::FH::Key, std::move(pFH));

                auto& caps = Glob::EncodeCaps::GetOrConstruct(storage);
                caps.SuperResSupport = 1;
            }
        };

        TEST_F(FeatureBlocksSuperres, CheckAndFix)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_SUPERRES, Superres::BLK_CheckAndFix });
            auto&       vp    = Glob::VideoParam::Get(storage);

            mfxExtAV1AuxData& auxPar        = ExtBuffer::Get(vp);
            auxPar.EnableSuperres           = MFX_CODINGOPTION_ON;
            auxPar.SuperresScaleDenominator = 8;
            ASSERT_EQ(
                block->Call(vp, vp, storage),
                MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            );
            ASSERT_EQ(
                auxPar.SuperresScaleDenominator,
                16);

            auxPar.EnableSuperres           = MFX_CODINGOPTION_ON;
            auxPar.SuperresScaleDenominator = 17;
            ASSERT_EQ(
                block->Call(vp, vp, storage),
                MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            );
            ASSERT_EQ(
                auxPar.SuperresScaleDenominator,
                16);

            auxPar.EnableSuperres           = MFX_CODINGOPTION_OFF;
            auxPar.SuperresScaleDenominator = 5;
            ASSERT_EQ(
                block->Call(vp, vp, storage),
                MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            );
            ASSERT_EQ(
                auxPar.SuperresScaleDenominator,
                8);

            auxPar.EnableSuperres           = MFX_CODINGOPTION_ON;
            auxPar.SuperresScaleDenominator = 9;
            ASSERT_EQ(
                block->Call(vp, vp, storage),
                MFX_ERR_NONE
            );
            ASSERT_EQ(
                auxPar.SuperresScaleDenominator,
                9);

            auxPar.EnableSuperres           = MFX_CODINGOPTION_OFF;
            auxPar.SuperresScaleDenominator = 8;
            ASSERT_EQ(
                block->Call(vp, vp, storage),
                MFX_ERR_NONE
            );
            ASSERT_EQ(
                auxPar.SuperresScaleDenominator,
                8);
        }

        TEST_F(FeatureBlocksSuperres, SetDefaults)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_SetDefaults>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_SUPERRES, Superres::BLK_SetDefaults });
            auto&       vp    = Glob::VideoParam::Get(storage);

            mfxExtAV1AuxData& auxPar        = ExtBuffer::Get(vp);
            auxPar.EnableSuperres           = 0;
            auxPar.SuperresScaleDenominator = 0;
            block->Call(vp, storage, storage);
            ASSERT_EQ(
                auxPar.EnableSuperres,
                MFX_CODINGOPTION_OFF);
            ASSERT_EQ(
                auxPar.SuperresScaleDenominator,
                8);

            auxPar.EnableSuperres           = MFX_CODINGOPTION_ON;
            auxPar.SuperresScaleDenominator = 0;
            block->Call(vp, storage, storage);
            ASSERT_EQ(
                auxPar.EnableSuperres,
                MFX_CODINGOPTION_ON);
            ASSERT_EQ(
                auxPar.SuperresScaleDenominator,
                16);
        }

        TEST_F(FeatureBlocksSuperres, SetSH)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_SUPERRES, Superres::BLK_SetSH });
            auto&       vp    = Glob::VideoParam::Get(storage);

            mfxExtAV1AuxData& auxPar    = ExtBuffer::Get(vp);
            auto&             sh        = Glob::SH::Get(storage);
            bool              isEnabled = static_cast<bool>(std::rand() % 2);
            auxPar.EnableSuperres       = (mfxU8)(isEnabled ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
            ASSERT_EQ(
                block->Call(storage, storage),
                MFX_ERR_NONE
            );
            ASSERT_EQ(
                sh.enable_superres,
                isEnabled
            );
        }

        TEST_F(FeatureBlocksSuperres, SetFH)
        {
            const auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            const auto& block = FeatureBlocks::Get(queue, { FEATURE_SUPERRES, Superres::BLK_SetFH });
            auto&       vp    = Glob::VideoParam::Get(storage);

            mfxExtAV1AuxData& auxPar        = ExtBuffer::Get(vp);
            auto&             fh            = Glob::FH::Get(storage);
            auxPar.EnableSuperres           = MFX_CODINGOPTION_ON;
            auxPar.SuperresScaleDenominator = 11;
            ASSERT_EQ(
                block->Call(storage, storage),
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

            mfxExtAV1ResolutionParam& rsPar = ExtBuffer::Get(vp);
            ASSERT_EQ(
                fh.UpscaledWidth,
                rsPar.FrameWidth
            );
        }
    }
}
