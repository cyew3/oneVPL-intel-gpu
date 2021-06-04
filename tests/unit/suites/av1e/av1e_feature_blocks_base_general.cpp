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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
#include "mfx_session.h"
#include "av1ehw_base_alloc.h"
#include "av1ehw_base_general.h"

#include "libmfx_core_factory.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

mfxStatus APIImpl_MFXQueryVersion(_mfxSession*, mfxVersion*) {
    return MFX_ERR_NONE;
}

namespace av1e {
    namespace tests
    {
        struct FeatureBlocksGeneral
            : testing::Test
        {
            FeatureBlocks   blocks{};
            General         general;
            StorageRW       global;
            StorageRW       local;
            VideoCORE*      core = nullptr;
            _mfxSession     session{0};

            FeatureBlocksGeneral()
                : general(FEATURE_GENERAL)
            {}

            void SetUp() override
            {
                eMFXVAType type;
#if defined(MFX_VA_WIN)
                type = MFX_HW_D3D11;
#else
                type = MFX_HW_VAAPI;
#endif
                core = FactoryCORE::CreateCORE(type, 0, 0, &session);
                session.m_pOperatorCore = new OperatorCORE(core);

                global.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(*core));

                general.Init(INIT | RUNTIME, blocks);

                Glob::EncodeCaps::GetOrConstruct(global);
                Glob::Defaults::GetOrConstruct(global);

                auto& queueQ1 = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks);
                auto setDefault = FeatureBlocks::Get(queueQ1, { FEATURE_GENERAL, General::BLK_SetDefaultsCallChain });

                auto& vp = Glob::VideoParam::GetOrConstruct(global);
                vp.AsyncDepth = 1;
                vp.mfx.GopPicSize = 1;
                vp.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
                vp.mfx.FrameInfo.Width = 640;
                vp.mfx.FrameInfo.Height = 480;

                EXPECT_EQ(
                    setDefault->Call(vp, vp, global),
                    MFX_ERR_NONE
                );
            }
        };

        TEST_F(FeatureBlocksGeneral, InitTask)
        {
            auto& queueAT = FeatureBlocks::BQ<FeatureBlocks::BQ_AllocTask>::Get(blocks);
            auto const& allocTask = FeatureBlocks::Get(queueAT, { FEATURE_GENERAL, General::BLK_AllocTask });

            auto& vp = Glob::VideoParam::GetOrConstruct(global);
            const mfxU16 numRefFrame = 2;
            vp.mfx.NumRefFrame = numRefFrame;
            EXPECT_EQ(
                allocTask->Call(global, local),
                MFX_ERR_NONE
            );

            // calling General's InitTask routing
            auto& queueIT = FeatureBlocks::BQ<FeatureBlocks::BQ_InitTask>::Get(blocks);
            auto const& block = FeatureBlocks::Get(queueIT, { FEATURE_GENERAL, General::BLK_InitTask });

            mfxEncodeCtrl* pCtrl = nullptr;
            mfxFrameSurface1 surf{};
            surf.Data.Locked = 0;
            mfxBitstream bs;
            EXPECT_EQ(
                block->Call(pCtrl, &surf, &bs, global, local),
                MFX_ERR_NONE
            );

            // let's check if task was configured properly
            EXPECT_EQ(surf.Data.Locked, 1);

            auto& tpar = Task::Common::Get(local);
            EXPECT_EQ(tpar.DPB.size(), numRefFrame);
        }

        TEST_F(FeatureBlocksGeneral, ConfigureTask)
        {
            local.Insert(Task::Common::Key, MfxFeatureBlocks::make_storable<TaskCommonPar>());

            auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_PreReorderTask>::Get(blocks);
            auto block = FeatureBlocks::Get(queue, { FEATURE_GENERAL, General::BLK_PrepareTask });

            EXPECT_EQ(
                block->Call(global, local),
                MFX_ERR_NONE
            );
            auto& tpar = Task::Common::Get(local);
            EXPECT_EQ(tpar.DisplayOrder, 0);
        }

        TEST_F(FeatureBlocksGeneral, ConfigureTaskIncreaseDisplayOrder)
        {
            local.Insert(Task::Common::Key, MfxFeatureBlocks::make_storable<TaskCommonPar>());

            auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_PreReorderTask>::Get(blocks);
            auto block = FeatureBlocks::Get(queue, { FEATURE_GENERAL, General::BLK_PrepareTask });

            EXPECT_EQ(
                block->Call(global, local),
                MFX_ERR_NONE
            );

            EXPECT_EQ(
                block->Call(global, local),
                MFX_ERR_NONE
            );
            auto& tpar = Task::Common::Get(local);
            EXPECT_EQ(tpar.DisplayOrder, 1);
        }

        TEST_F(FeatureBlocksGeneral, SetDefaults)
        {
            auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitExternal>::Get(blocks);
            auto block = FeatureBlocks::Get(queue, { FEATURE_GENERAL, General::BLK_SetDefaults });

            auto& vpOld = Glob::VideoParam::GetOrConstruct(global);
            EXPECT_EQ(vpOld.mfx.TargetUsage, 0);
            EXPECT_EQ(vpOld.mfx.QPI, 0);

            // Todo: Mock HW-dependency which blocks call SetDefaults block
            auto& vp = Glob::VideoParam::Get(global);
            auto& caps = Glob::EncodeCaps::Get(global);
            auto& defchain = Glob::Defaults::Get(global);
            general.SetDefaults(vp, Defaults::Param(vp, caps, MFX_HW_DG2, defchain), false);

            // check that after the Call() the default params are set
            // Todo: Do we need to test all default params? Or several of them are enough
            auto& vpNew = Glob::VideoParam::GetOrConstruct(global);
            EXPECT_EQ(vpNew.mfx.TargetUsage, MFX_TARGETUSAGE_4);
            EXPECT_EQ(vpNew.mfx.QPI, 0);
        }

        TEST_F(FeatureBlocksGeneral, SetFH)
        {
            auto& vp   = Glob::VideoParam::Get(global);
            vp.NewEB(MFX_EXTBUFF_AV1_PARAM, false);
            vp.NewEB(MFX_EXTBUFF_AV1_AUXDATA, false);
            vp.NewEB(MFX_EXTBUFF_CODING_OPTION3, false);
            vp.NewEB(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS, false);

            mfxExtAV1Param& av1Par          = ExtBuffer::Get(vp);
            av1Par.FrameHeight              = 480;
            av1Par.FrameWidth               = 640;
            av1Par.StillPictureMode         = MFX_CODINGOPTION_OFF;
            av1Par.EnableSuperres           = MFX_CODINGOPTION_ON;
            av1Par.SuperresScaleDenominator = 11;
            av1Par.EnableCdef               = MFX_CODINGOPTION_OFF;
            av1Par.EnableRestoration        = MFX_CODINGOPTION_ON;
            av1Par.InterpFilter             = MFX_AV1_INTERP_EIGHTTAP;
            av1Par.DisableCdfUpdate         = MFX_CODINGOPTION_OFF;
            av1Par.DisableFrameEndUpdateCdf = MFX_CODINGOPTION_OFF;

            mfxExtAV1AuxData& av1AuxPar     = ExtBuffer::Get(vp);
            av1AuxPar.EnableOrderHint       = MFX_CODINGOPTION_OFF;
            av1AuxPar.ErrorResilientMode    = MFX_CODINGOPTION_OFF;

            auto& caps = Glob::EncodeCaps::Get(global);
            auto& sh   = Glob::SH::GetOrConstruct(global);
            auto& fh   = Glob::FH::GetOrConstruct(global);
            general.SetSH(vp, MFX_HW_DG2, caps, sh);
            general.SetFH(vp, MFX_HW_DG2, sh, fh);

            for (auto i = 0; i < MAX_MB_PLANE; i++)
            {
                EXPECT_EQ(fh.lr_params.lr_type[i], RESTORE_WIENER);
            }
            EXPECT_EQ(fh.lr_params.lr_unit_shift, 0);
            EXPECT_EQ(fh.lr_params.lr_unit_extra_shift, 0);
            EXPECT_EQ(fh.lr_params.lr_uv_shift, 1);
            EXPECT_EQ(fh.RenderWidth, av1Par.FrameWidth);
            EXPECT_EQ(fh.FrameWidth, GetActualEncodeWidth(av1Par));
        }

        TEST_F(FeatureBlocksGeneral, CheckTemporalLayers)
        {
            auto& vp = Glob::VideoParam::Get(global);
            vp.NewEB(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS, false);
            mfxExtAvcTemporalLayers& tl = ExtBuffer::Get(vp);

            // check zero TL
            tl.Layer[0].Scale = 0;
            EXPECT_EQ(
                general.CheckTemporalLayers(vp),
                MFX_ERR_NONE);

            // check base layer
            tl.Layer[0].Scale = 1;
            EXPECT_EQ(
                general.CheckTemporalLayers(vp),
                MFX_ERR_NONE);

            // check 2x ratio between the frame rates of the current temporal layer and the base layer
            for (auto i = 1; i < MAX_NUM_TEMPORAL_LAYERS; i++)
            {
                tl.Layer[i].Scale = tl.Layer[i - 1].Scale << 1;
                EXPECT_EQ(
                    general.CheckTemporalLayers(vp),
                    MFX_ERR_NONE);
            }

            // check flexible (but valid) ratio between the frame rates of the current temporal layer and the base layer
            std::for_each(tl.Layer, tl.Layer + MAX_NUM_TEMPORAL_LAYERS, [](auto& t){ t.Scale = 0; });
            mfxU16 const scale[] = { 1, 3, 6, 18, 72, 144, 720, 1440 };
            for (auto i = 0; i < MAX_NUM_TEMPORAL_LAYERS; i++)
            {
                tl.Layer[i].Scale = scale[i];
                EXPECT_EQ(
                    general.CheckTemporalLayers(vp),
                    MFX_ERR_NONE);
            }

            // check invalid ratio between the frame rates of the current temporal layer and the base layer
            // case with the same frame rate for both base and advanced layers
            std::for_each(tl.Layer, tl.Layer + MAX_NUM_TEMPORAL_LAYERS, [](auto& t) { t.Scale = 0; });
            tl.Layer[0].Scale = 2;
            tl.Layer[1].Scale = 2;
            EXPECT_EQ(
                general.CheckTemporalLayers(vp),
                MFX_ERR_UNSUPPORTED);

            // case when base layer has higher frame rate than advanced one
            tl.Layer[0].Scale = 4;
            tl.Layer[1].Scale = 2;
            EXPECT_EQ(
                general.CheckTemporalLayers(vp),
                MFX_ERR_UNSUPPORTED);
        }
    }
}
