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

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_LP_LOOKAHEAD)

#include "hevcehw_base_lpla_enc.h"
#include "hevcehw_base_task.h"
#include "hevcehw_base_ddi_packer_win.h"
#include "hevcehw_base_packer.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;
using namespace HEVCEHW::Windows::Base;

void LpLookAheadEnc::Query1WithCaps(const FeatureBlocks & /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckLPLA
        , [this](const mfxVideoParam& /*in*/, mfxVideoParam& par, StorageRW& global) -> mfxStatus
    {
        mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
        MFX_CHECK(pCO2, MFX_ERR_NONE);
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        MFX_CHECK(pCO3, MFX_ERR_NONE);
        auto& caps = Glob::EncodeCaps::GetOrConstruct(global);

        //for game streaming scenario, if option enable lowpower lookahead, check encoder's capability
        if (pCO3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING && pCO2->LookAheadDepth > 0 &&
            (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR))
        {
            MFX_CHECK(caps.LookaheadBRCSupport, MFX_ERR_INVALID_VIDEO_PARAM);
            bIsLpLookAheadSupported = true;
        }

        return MFX_ERR_NONE;
    });
}

void LpLookAheadEnc::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_Init
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        mfxExtLplaParam& extLplaParam = ExtBuffer::Get(par);
        bAnalysis = extLplaParam.LookAheadDepth > 0;

        MFX_CHECK(!bAnalysis && !bInitialized && bIsLpLookAheadSupported, MFX_ERR_NONE);

        mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
        MFX_CHECK(pCO2, MFX_ERR_NONE);

        //create and initialize lowpower lookahead module
        auto& core = Glob::VideoCore::Get(global);
        pLpLookAhead.reset(new MfxLpLookAhead(&core));

        // create ext buffer to set the lookahead data buffer
        lplaParam = par;
        extBufLPLA.Header.BufferId  = MFX_EXTBUFF_LP_LOOKAHEAD;
        extBufLPLA.Header.BufferSz  = sizeof(extBufLPLA);
        extBufLPLA.LookAheadDepth   = pCO2->LookAheadDepth;
        extBufLPLA.InitialDelayInKB = par.mfx.InitialDelayInKB;
        extBufLPLA.BufferSizeInKB   = par.mfx.BufferSizeInKB;
        extBufLPLA.TargetKbps       = par.mfx.TargetKbps;

        extBuffers[0]         = &extBufLPLA.Header;
        lplaParam.NumExtParam = 1;
        lplaParam.ExtParam    = &extBuffers[0];

        LADepth = extBufLPLA.LookAheadDepth;

        mfxStatus sts= pLpLookAhead->Init(&lplaParam);
        if (sts == MFX_ERR_NONE)
        {
            bIsLpLookaheadEnabled = true;
        }

        return sts;
    });

    Push(BLK_SetCallChains
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& ddiCC = DDIPacker::CC::GetOrConstruct(global);
        using TCC = DDIPacker::CallChains;

        auto& packerCC = Packer::CC::GetOrConstruct(global);
        using TPackerCC = Packer::CallChains;

        // Update LPLA related sequence params.
        ddiCC.UpdateLPLAEncSPS.Push([this](
            TCC::TUpdateLPLAEncSPS::TExt
            , const StorageR& global
            , ENCODE_SET_SEQUENCE_PARAMETERS_HEVC& sps)
        {
            MFX_CHECK(!bAnalysis, MFX_ERR_NONE);

            auto& par = Glob::VideoParam::Get(global);
            const mfxExtCodingOption2& CO2 = ExtBuffer::Get(par);
            sps.LookaheadDepth = (UCHAR)CO2.LookAheadDepth;

            return MFX_ERR_NONE;
        });

        // Update LPLA realted picture params.
        ddiCC.UpdateLPLAEncPPS.Push([this](
            TCC::TUpdateLPLAEncPPS::TExt
            , const StorageR& /*global*/
            , const StorageR& s_task
            , ENCODE_SET_PICTURE_PARAMETERS_HEVC& pps)
        {
            MFX_CHECK(!bAnalysis, MFX_ERR_NONE);

            const auto& task = Task::Common::Get(s_task);
            if (task.LplaStatus.TargetFrameSize > 0)
            {
                pps.TargetFrameSize = task.LplaStatus.TargetFrameSize;
                pps.QpModulationStrength = task.LplaStatus.QpModulation;
            }
            else
            {
                pps.TargetFrameSize = 0;
                pps.QpModulationStrength = 0;
            }

            return MFX_ERR_NONE;
        });

        // Check when it is needed to pack cqm PPS to driver.
        ddiCC.PackCqmPPS.Push([this](
            TCC::TPackCqmPPS::TExt
            , const StorageR& /*global*/
            , const StorageR& s_task)
        {
            auto& task = Task::Common::Get(s_task);
            return !bAnalysis && bIsLpLookaheadEnabled && (task.InsertHeaders & INSERT_PPS);
        });

        // Check whether to pack cqm PPS header.
        packerCC.PackCqmHeader.Push([this](
            TPackerCC::TPackCqmHeader::TExt
            , StorageW* /*global*/)
        {
            bool bPack = !bAnalysis && bIsLpLookaheadEnabled;
            return bPack;
        });

        // Update LPLA related slice header params.
        packerCC.UpdateSH.Push([](
            TPackerCC::TUpdateSH::TExt
            , StorageW& s_task)
        {
            auto& slice = Task::SSH::Get(s_task);
            auto& task = Task::Common::Get(s_task);
            if (task.LplaStatus.CqmHint == CQM_HINT_USE_CUST_MATRIX)
                slice.pic_parameter_set_id = 1;
        });

        return MFX_ERR_NONE;
    });

    // Add S_LA_SUBMIT and S_LA_QUERY stages for LPLA
    Push(BLK_AddTask
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        MFX_CHECK(bIsLpLookaheadEnabled, MFX_ERR_NONE);

        auto& taskMgrIface = TaskManager::TMInterface::Get(global);
        auto& tm = taskMgrIface.Manager;

        S_LA_SUBMIT = tm.AddStage(tm.S_PREPARE);
        S_LA_QUERY = tm.AddStage(S_LA_SUBMIT);

        auto  LASubmit = [&](
            TaskManager::ExtTMInterface::TAsyncStage::TExt
            , StorageW& /*global*/
            , StorageW& /*s_task*/) -> mfxStatus
        {
            std::unique_lock<std::mutex> closeGuard(tm.m_closeMtx);
            // If In LookAhead Pass, it should be moved to the next stage
            if (bAnalysis)
            {
                StorageW* pTask = tm.GetTask(tm.Stage(S_LA_SUBMIT));
                tm.MoveTaskForward(tm.Stage(S_LA_SUBMIT), tm.FixedTask(*pTask));
                return MFX_ERR_NONE;
            }

            while (StorageW* pTask = tm.GetTask(tm.Stage(S_LA_SUBMIT)))
            {
                auto& rtask = Task::Common::Get(*pTask);
                pLpLookAhead->Submit(rtask.pSurfIn);
                tm.MoveTaskForward(tm.Stage(S_LA_SUBMIT), tm.FixedTask(*pTask));
            }

            return MFX_ERR_NONE;
        };

        auto  LAQuery = [&](
            TaskManager::ExtTMInterface::TAsyncStage::TExt
            , StorageW& /*global*/
            , StorageW& s_task) -> mfxStatus
        {
            std::unique_lock<std::mutex> closeGuard(tm.m_closeMtx);
            // If In LookAhead Pass, it should be moved to the next stage
            if (bAnalysis)
            {
                StorageW* pTask = tm.GetTask(tm.Stage(S_LA_QUERY));
                tm.MoveTaskForward(tm.Stage(S_LA_QUERY), tm.FixedTask(*pTask));
                return MFX_ERR_NONE;
            }

            // Delay For LookAhead Depth
            if (!bEncRun)
            {
                bEncRun = tm.m_stages.at(tm.Stage(S_LA_QUERY)).size() >= LADepth;
            }
            MFX_CHECK(bEncRun, MFX_ERR_NONE);

            StorageW* pTask = tm.GetTask(tm.Stage(S_LA_QUERY));
            auto& task = Task::Common::Get(s_task);
            mfxLplastatus laStatus = {};
            mfxStatus sts = pLpLookAhead->Query(laStatus);

            if (sts == MFX_ERR_NONE)
            {
                task.LplaStatus = laStatus;
            }
            tm.MoveTaskForward(tm.Stage(S_LA_QUERY), tm.FixedTask(*pTask));

            return MFX_ERR_NONE;
        };

        taskMgrIface.AsyncStages[tm.Stage(S_LA_SUBMIT)].Push(LASubmit);
        taskMgrIface.AsyncStages[tm.Stage(S_LA_QUERY)].Push(LAQuery);

        // Extend Num of tasks and size of buffer.
        taskMgrIface.ResourceExtra += LADepth;

        return MFX_ERR_NONE;
    });

    Push(BLK_UpdateTask
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& taskMgrIface = TaskManager::TMInterface::Get(global);

        auto  UpdateTask = [&](
            TaskManager::ExtTMInterface::TUpdateTask::TExt
            , StorageW& srcTask
            , StorageW* dstTask) -> mfxStatus
        {
            MFX_CHECK(bIsLpLookAheadSupported, MFX_ERR_NONE);
            auto& src_task = Task::Common::Get(srcTask);

            if (dstTask)
            {
                auto& dst_task = Task::Common::Get(*dstTask);
                dst_task.LplaStatus = src_task.LplaStatus;
            }
            else if (!bAnalysis && src_task.LplaStatus.TargetFrameSize > 0)
            {
                pLpLookAhead->SetStatus(&src_task.LplaStatus);
            }
            return MFX_ERR_NONE;
        };
        taskMgrIface.UpdateTask.Push(UpdateTask);

        return MFX_ERR_NONE;
    });
}

void LpLookAheadEnc::Close(const FeatureBlocks& /*blocks*/, TPushCLS Push)
{
    Push(BLK_Close
        , [this](StorageW& /*global*/) -> mfxStatus
    {
        mfxStatus sts = MFX_ERR_NONE;

        if (pLpLookAhead && !bAnalysis)
        {
            sts = pLpLookAhead->Close();
        }

        return sts;
    });
}


#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
