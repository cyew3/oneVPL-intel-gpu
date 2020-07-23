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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
#include "mfx_session.h"
#include "av1ehw_base_alloc.h"
#include "av1ehw_base_general.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

namespace av1e {
    namespace tests
    {
        struct FeatureBlocksGeneral
            : testing::Test
        {
            FeatureBlocks   blocks{};
            General         general;
            StorageRW       storage;
            MFXVideoSession session;

            FeatureBlocksGeneral()
                : general(FEATURE_GENERAL)
            {}

            void SetUp() override
            {
                ASSERT_EQ(
                    session.InitEx(
                        mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                    ),
                    MFX_ERR_NONE
                );

                auto s = static_cast<_mfxSession*>(session);
                storage.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(*(s->m_pCORE.get())));

                general.Init(INIT | RUNTIME, blocks);

                Glob::EncodeCaps::GetOrConstruct(storage);
                Glob::Defaults::GetOrConstruct(storage);

                auto& queueQ1 = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks);
                auto setDefault = FeatureBlocks::Get(queueQ1, { FEATURE_GENERAL, General::BLK_SetDefaultsCallChain });

                auto& vp = Glob::VideoParam::GetOrConstruct(storage);
                vp.AsyncDepth = 1;
                vp.mfx.GopPicSize = 1;
                vp.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
                vp.mfx.FrameInfo.Width = 640;
                vp.mfx.FrameInfo.Height = 480;

                ASSERT_EQ(
                    setDefault->Call(vp, vp, storage),
                    MFX_ERR_NONE
                );
            }
        };

        TEST_F(FeatureBlocksGeneral, InitTask)
        {
            mfxEncodeCtrl* pCtrl = nullptr;
            mfxFrameSurface1 pSurf;
            mfxBitstream pBs;
            StorageRW task;

            auto &video_par = Glob::VideoParam::GetOrConstruct(storage);
            const mfxU16 num_ref_frame_test_value = 2;
            video_par.mfx.NumRefFrame = num_ref_frame_test_value;

            pSurf.Data.Locked = 0;

            auto& queueAT = FeatureBlocks::BQ<FeatureBlocks::BQ_AllocTask>::Get(blocks);
            auto& allocTask = FeatureBlocks::Get(queueAT, { FEATURE_GENERAL, General::BLK_AllocTask });
            ASSERT_EQ(
                allocTask->Call(storage, task),
                MFX_ERR_NONE
            );

            // calling General's InitTask routing
            auto& queueIT = FeatureBlocks::BQ<FeatureBlocks::BQ_InitTask>::Get(blocks);
            auto& block = FeatureBlocks::Get(queueIT, { FEATURE_GENERAL, General::BLK_InitTask });
            ASSERT_EQ(
                block->Call(pCtrl, &pSurf, &pBs, storage, task),
                MFX_ERR_NONE
            );

            // let's check if task was configured properly
            ASSERT_EQ(pSurf.Data.Locked, 1);
            auto& tpar = Task::Common::Get(task);
            ASSERT_EQ(tpar.DPB.size(), num_ref_frame_test_value);
        }

        TEST_F(FeatureBlocksGeneral, ConfigureTask)
        {
            StorageRW task;
            auto p_task_par = MfxFeatureBlocks::make_storable<TaskCommonPar>();
            task.Insert(Task::Common::Key, std::move(p_task_par));

            auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_PreReorderTask>::Get(blocks);
            auto block = FeatureBlocks::Get(queue, { FEATURE_GENERAL, General::BLK_PrepareTask });

            ASSERT_EQ(
                block->Call(storage, task),
                MFX_ERR_NONE
            );
            auto& tpar = Task::Common::Get(task);
            ASSERT_EQ(tpar.DisplayOrder, 0);

            // call second time to check if DisplayOrder is increased
            ASSERT_EQ(
                block->Call(storage, task),
                MFX_ERR_NONE
            );
            auto& tpar2 = Task::Common::Get(task);
            ASSERT_EQ(tpar2.DisplayOrder, 1);
        }

        TEST_F(FeatureBlocksGeneral, SetDefaults)
        {
            StorageRW task;

            auto& queue = FeatureBlocks::BQ<FeatureBlocks::BQ_InitExternal>::Get(blocks);
            auto block = FeatureBlocks::Get(queue, { FEATURE_GENERAL, General::BLK_SetDefaults });

            auto &vp_before = Glob::VideoParam::GetOrConstruct(storage);
            ASSERT_EQ(vp_before.mfx.TargetUsage, 0);
            ASSERT_EQ(vp_before.mfx.QPI, 0);

            // cannot call SetDefaults() as a block due to HW-dependency
            auto& vp = Glob::VideoParam::Get(storage);
            auto& caps = Glob::EncodeCaps::Get(storage);
            auto& defchain = Glob::Defaults::Get(storage);
            general.SetDefaults(vp, Defaults::Param(vp, caps, MFX_HW_DG2, defchain), false);

            // check that after the Call() the default params are set
            auto &vp_after = Glob::VideoParam::GetOrConstruct(storage);
            ASSERT_EQ(vp_after.mfx.TargetUsage, MFX_TARGETUSAGE_4);
            const mfxU16 default_QPI = 128;
            ASSERT_EQ(vp_after.mfx.QPI, default_QPI);
        }

        TEST_F(FeatureBlocksGeneral, SetFH)
        {
            auto& vp   = Glob::VideoParam::Get(storage);
            auto& caps = Glob::EncodeCaps::Get(storage);
            auto p_sh  = MfxFeatureBlocks::make_storable<SH>();
            auto p_fh  = MfxFeatureBlocks::make_storable<FH>();

            vp.NewEB(MFX_EXTBUFF_AV1_PARAM, false);
            vp.NewEB(MFX_EXTBUFF_AV1_AUXDATA, false);
            vp.NewEB(MFX_EXTBUFF_CODING_OPTION3, false);
            mfxExtAV1Param& av1Par      = ExtBuffer::Get(vp);
            mfxExtAV1AuxData& av1AuxPar = ExtBuffer::Get(vp);
            mfxExtCodingOption3& pCO3   = ExtBuffer::Get(vp);

            av1Par.StillPictureMode         = MFX_CODINGOPTION_OFF;
            av1Par.EnableSuperres           = MFX_CODINGOPTION_OFF;
            av1Par.EnableCdef               = MFX_CODINGOPTION_OFF;
            av1Par.EnableRestoration        = MFX_CODINGOPTION_ON;
            av1Par.InterpFilter             = MFX_AV1_INTERP_EIGHTTAP;
            av1Par.DisableCdfUpdate         = MFX_CODINGOPTION_OFF;
            av1Par.DisableFrameEndUpdateCdf = MFX_CODINGOPTION_OFF;
            av1AuxPar.EnableOrderHint       = MFX_CODINGOPTION_OFF;
            av1AuxPar.ErrorResilientMode    = MFX_CODINGOPTION_OFF;

            general.SetSH(vp, MFX_HW_DG2, caps, *p_sh);
            storage.Insert(Glob::SH::Key, std::move(p_sh));
            auto& sh = Glob::SH::Get(storage);
            general.SetFH(vp, MFX_HW_DG2, sh, *p_fh);

            // check loop restoration parameters in frame header
            for (auto i = 0; i < MAX_MB_PLANE; i++)
            {
                ASSERT_EQ(p_fh->lr_params.lr_type[i], RESTORE_WIENER);
            }
            ASSERT_EQ(p_fh->lr_params.lr_unit_shift, 0);
            ASSERT_EQ(p_fh->lr_params.lr_unit_extra_shift, 0);
            ASSERT_EQ(p_fh->lr_params.lr_uv_shift, 1);
        }
    }
}
