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

#include "av1ehw_g12_alloc.h"
#include "av1ehw_g12_general.h"
#include "av1ehw_g12_task.h"

namespace AV1EHW
{
namespace Gen12
{
    struct FeatureBlocksTask
        : testing::Test
    {
        MFXVideoSession                       session;

        FeatureBlocks                         blocks{};
        General                               general;
        TaskManager                           taskMgr;
        StorageRW                             strg;
        StorageRW                             local;

        FeatureBlocksTask()
            : general(FEATURE_GENERAL)
            , taskMgr(FEATURE_TASK_MANAGER)
        {

        }

        void SetUp() override
        {
            ASSERT_EQ(
                session.InitEx(
                    mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                ),
                MFX_ERR_NONE
            );

            auto s = static_cast<_mfxSession*>(session);
            strg.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(*(s->m_pCORE.get())));

            auto& vp = Glob::VideoParam::GetOrConstruct(strg);
            vp.AsyncDepth = 1;
            vp.mfx.GopPicSize = 1;

            general.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);
            taskMgr.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);

            Glob::EncodeCaps::GetOrConstruct(strg);

            auto& queueII = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            auto block = FeatureBlocks::Get(queueII, { FEATURE_GENERAL, General::BLK_SetReorder });
            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );
        };

        void TearDown() override
        {
            strg.Clear();
            local.Clear();
        }
    };

    TEST_F(FeatureBlocksTask, FrameSubmitNoSurf)
    {
        auto& queueFS = FeatureBlocks::BQ<FeatureBlocks::BQ_FrameSubmit>::Get(blocks);
        auto block = FeatureBlocks::Get(queueFS, { FEATURE_TASK_MANAGER, TaskManager::BLK_NewTask });

        mfxEncodeCtrl* pCtrl = nullptr;
        mfxFrameSurface1* pSurf = nullptr;
        mfxBitstream bs{};

        ASSERT_EQ(
            block->Call(pCtrl, pSurf, bs, strg, local),
            MFX_ERR_MORE_DATA
        );
    }

    TEST_F(FeatureBlocksTask, FrameSubmitNoTask)
    {
        auto& queueFS = FeatureBlocks::BQ<FeatureBlocks::BQ_FrameSubmit>::Get(blocks);
        auto block = FeatureBlocks::Get(queueFS, { FEATURE_TASK_MANAGER, TaskManager::BLK_NewTask });

        mfxEncodeCtrl ctrl = {};
        mfxFrameSurface1 surf = {};
        mfxBitstream bs{};

        ASSERT_EQ(
            block->Call(&ctrl, &surf, bs, strg, local),
            MFX_WRN_DEVICE_BUSY
        );
    }

    // TODO: The test is disabled because of move to common TaskManager and Allocator. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksTask, FrameSubmitNeedMoreData)
    {
        auto& queueIA = FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks);
        auto init = FeatureBlocks::Get(queueIA, { FEATURE_TASK_MANAGER, TaskManager::BLK_Init });

        auto& reorder = Glob::Reorder::Get(strg);
        reorder.BufferSize = 2;

        std::unique_ptr<IAllocation> pAlloc(new MfxFrameAllocResponse(Glob::VideoCore::Get(strg)));
        strg.Insert(Glob::AllocBS::Key, std::move(pAlloc));

        ASSERT_EQ(
            init->Call(strg, strg),
            MFX_ERR_NONE
        );

        auto& queueFS = FeatureBlocks::BQ<FeatureBlocks::BQ_FrameSubmit>::Get(blocks);
        auto block = FeatureBlocks::Get(queueFS, { FEATURE_TASK_MANAGER, TaskManager::BLK_NewTask });

        mfxEncodeCtrl ctrl = {};
        mfxFrameSurface1 surf = {};
        mfxBitstream bs{};

        ASSERT_EQ(
            block->Call(&ctrl, &surf, bs, strg, local),
            MFX_ERR_MORE_DATA_SUBMIT_TASK
        );
    }*/

    TEST_F(FeatureBlocksTask, AsyncRoutinePrepTaskWrongStage)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_PrepareTask });

        auto& task = Task::Common::GetOrConstruct(local);
        task.stage = S_SUBMIT;
        ASSERT_EQ(
            block->Call(strg, local),
            MFX_ERR_NONE
        );
    }

    TEST_F(FeatureBlocksTask, AsyncRoutinePrepTaskNoSurf)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_PrepareTask });

        auto& task = Task::Common::GetOrConstruct(local);
        task.stage = S_PREPARE;

        ASSERT_EQ(
            task.pSurfIn,
            nullptr
        );

        ASSERT_EQ(
            block->Call(strg, local),
            MFX_ERR_NONE
        );
    }

    // TODO: The test is disabled because of move to common TaskManager and Allocator. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksTask, AsyncRoutinePrepTaskNoTask)
    {
        auto& queueQ1 = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks);
        auto setDefault = FeatureBlocks::Get(queueQ1, { FEATURE_GENERAL, General::BLK_SetDefaultsCallChain });
        auto& vp = Glob::VideoParam::GetOrConstruct(strg);

        ASSERT_EQ(
            setDefault->Call(vp, vp, strg),
            MFX_ERR_NONE
        );

        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_PrepareTask });

        auto& task = Task::Common::GetOrConstruct(local);
        task.stage = S_PREPARE;
        mfxFrameSurface1 surf;
        task.pSurfIn = &surf;

        ASSERT_NE(
            task.pSurfIn,
            nullptr
        );

        ASSERT_EQ(
            block->Call(strg, local),
            MFX_ERR_UNDEFINED_BEHAVIOR
        );
    }*/

    TEST_F(FeatureBlocksTask, AsyncRoutineSubmitTaskNoTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_SubmitTask });

        ASSERT_EQ(
            block->Call(strg, strg),
            MFX_ERR_NONE
        );
    }

    inline void UpdateStage(mfxU32& stage, const mfxU16 from, const mfxU16 to)
    {
        stage |= (1 << from);
        stage &= ~(0xffffffff << to);
    }

    struct FeatureBlocksTaskStatic
        : testing::Test
    {
        static MFXVideoSession           session;

        static FeatureBlocks             blocks;
        static General                   general;
        static TaskManager               taskMgr;
        static StorageRW                 strg;

        static StorageRW                 local1;
        static mfxEncodeCtrl             ctrl1;
        static mfxFrameSurface1          surf1;
        static mfxBitstream              bs1;
        static mfxU32                    stage1;

        static StorageRW                 local2;
        static mfxEncodeCtrl             ctrl2;
        static mfxFrameSurface1          surf2;
        static mfxBitstream              bs2;
        static mfxU32                    stage2;

        static StorageRW                 fake;
        static mfxU32                    fakeStage;

        static void SetUpTestCase()
        {
            ASSERT_EQ(
                session.InitEx(
                    mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                ),
                MFX_ERR_NONE
            );

            auto s = static_cast<_mfxSession*>(session);
            strg.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(*(s->m_pCORE.get())));

            auto& vp = Glob::VideoParam::GetOrConstruct(strg);
            vp.AsyncDepth = 1;
            vp.mfx.GopPicSize = 1;

            general.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);
            taskMgr.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);

            Glob::EncodeCaps::GetOrConstruct(strg);

            auto& queueII = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            auto block = FeatureBlocks::Get(queueII, { FEATURE_GENERAL, General::BLK_SetReorder });
            ASSERT_EQ(
                block->Call(strg, strg),
                MFX_ERR_NONE
            );
        }

        static void TearDownTestCase()
        {
            strg.Clear();
            local1.Clear();
            local2.Clear();
            fake.Clear();
        }
    };

    MFXVideoSession FeatureBlocksTaskStatic::session{};
    FeatureBlocks FeatureBlocksTaskStatic::blocks{};
    General FeatureBlocksTaskStatic::general(FEATURE_GENERAL);
    TaskManager FeatureBlocksTaskStatic::taskMgr(FEATURE_TASK_MANAGER);
    StorageRW FeatureBlocksTaskStatic::strg{};

    StorageRW FeatureBlocksTaskStatic::local1{};
    mfxEncodeCtrl FeatureBlocksTaskStatic::ctrl1{};
    mfxFrameSurface1 FeatureBlocksTaskStatic::surf1{};
    mfxBitstream FeatureBlocksTaskStatic::bs1{};
    mfxU32 FeatureBlocksTaskStatic::stage1{S_NEW};

    StorageRW FeatureBlocksTaskStatic::local2{};
    mfxEncodeCtrl FeatureBlocksTaskStatic::ctrl2{};
    mfxFrameSurface1 FeatureBlocksTaskStatic::surf2{};
    mfxBitstream FeatureBlocksTaskStatic::bs2{};
    mfxU32 FeatureBlocksTaskStatic::stage2{S_NEW};

    StorageRW FeatureBlocksTaskStatic::fake{};
    mfxU32 FeatureBlocksTaskStatic::fakeStage{S_NEW};

    // TODO: The test is disabled because of move to common TaskManager and Allocator. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksTaskStatic, InitAlloc)
    {
        auto& queueQ1 = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks);
        auto setDefault = FeatureBlocks::Get(queueQ1, { FEATURE_GENERAL, General::BLK_SetDefaultsCallChain });
        auto& vp = Glob::VideoParam::GetOrConstruct(strg);

        auto& reorder = Glob::Reorder::Get(strg);
        reorder.BufferSize = 2;

        ASSERT_EQ(
            setDefault->Call(vp, vp, strg),
            MFX_ERR_NONE
        );

        auto& queueIA = FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks);
        auto block = FeatureBlocks::Get(queueIA, { FEATURE_TASK_MANAGER, TaskManager::BLK_Init });

        std::unique_ptr<IAllocation> pAlloc(new MfxFrameAllocResponse(Glob::VideoCore::Get(strg)));
        auto& res = const_cast<mfxFrameAllocResponse&>(pAlloc->Response());
        res.NumFrameActual = 3;
        strg.Insert(Glob::AllocBS::Key, std::move(pAlloc));

        ASSERT_EQ(
            block->Call(strg, strg),
            MFX_ERR_NONE
        );
    }*/

    // TODO: The test is disabled because of move to common TaskManager and Allocator. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksTaskStatic, FrameSubmit)
    {
        auto& queueFS = FeatureBlocks::BQ<FeatureBlocks::BQ_FrameSubmit>::Get(blocks);
        auto block = FeatureBlocks::Get(queueFS, { FEATURE_TASK_MANAGER, TaskManager::BLK_NewTask });

        ASSERT_EQ(
            block->Call(&ctrl1, &surf1, bs1, strg, local1),
            MFX_ERR_MORE_DATA_SUBMIT_TASK
        );

        UpdateStage(stage1, S_NEW, S_PREPARE);

        auto& taskStrg1 = Tmp::CurrTask::Get(local1);
        auto& task1 = Task::Common::Get(taskStrg1);
        ASSERT_EQ(
            task1.stage,
            stage1
        );

        ASSERT_EQ(
            block->Call(&ctrl2, &surf2, bs2, strg, local2),
            MFX_ERR_MORE_DATA_SUBMIT_TASK
        );

        UpdateStage(stage2, S_NEW, S_PREPARE);

        auto& taskStrg2 = Tmp::CurrTask::Get(local2);
        auto& task2 = Task::Common::Get(taskStrg2);
        ASSERT_EQ(
            task2.stage,
            stage2
        );

        ASSERT_EQ(
            block->Call(&ctrl2, &surf2, bs2, strg, fake),
            MFX_ERR_NONE
        );

        UpdateStage(fakeStage, S_NEW, S_PREPARE);

        auto& fakeTaskStrg = Tmp::CurrTask::Get(fake);
        auto& fakeTask = Task::Common::Get(fakeTaskStrg);
        ASSERT_EQ(
            fakeTask.stage,
            fakeStage
        );
    }*/

    // TODO: The test is disabled because of move to common TaskManager and Allocator. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksTaskStatic, AsyncRoutinePrepTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_PrepareTask });

        auto& taskStrg = Tmp::CurrTask::Get(local1);
        auto& task = Task::Common::Get(taskStrg);

        ASSERT_EQ(
            task.stage,
            S_PREPARE
        );

        ASSERT_NE(
            task.pSurfIn,
            nullptr
        );

        ASSERT_EQ(
            block->Call(strg, taskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage1, S_PREPARE, S_REORDER);

        ASSERT_EQ(
            task.stage,
            stage1
        );
    }*/

    // TODO: The test is disabled because of move to common TaskManager and Allocator. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksTaskStatic, AsyncRoutineReorderTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_ReorderTask });

        auto& taskStrg = Tmp::CurrTask::Get(local1);
        auto& task = Task::Common::Get(taskStrg);

        //Clear PostReorderTask queue to avoid create Rec and BS surface...
        auto& queuePostRT = FeatureBlocks::BQ<FeatureBlocks::BQ_PostReorderTask>::Get(blocks);
        queuePostRT.clear();

        ASSERT_EQ(
            block->Call(strg, taskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage1, S_REORDER, S_SUBMIT);

        ASSERT_EQ(
            task.stage,
            stage1
        );
    }*/

    // TODO: The test is disabled because of move to common TaskManager and Allocator. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksTaskStatic, AsyncRoutineSubmitOneTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_SubmitTask });

        //Clear SubmitTask queue (including General::BLK_GetRawHDL, General::BLK_CopySysToRaw)
        auto& queueST = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        queueST.clear();

        ASSERT_EQ(
            block->Call(strg, strg),
            MFX_ERR_NONE
        );

        UpdateStage(stage1, S_SUBMIT, S_QUERY);

        auto& taskStrg = Tmp::CurrTask::Get(local1);
        auto& task = Task::Common::Get(taskStrg);
        ASSERT_EQ(
            task.stage,
            stage1
        );
    }*/

    // TODO: The test is disabled because of move to common TaskManager and Allocator. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksTaskStatic, AsyncRoutineSubmitAnotherTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto prepTask = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_PrepareTask });

        auto& taskStrg = Tmp::CurrTask::Get(local2);
        auto& task = Task::Common::Get(taskStrg);

        ASSERT_EQ(
            task.stage,
            S_PREPARE
        );

        ASSERT_NE(
            task.pSurfIn,
            nullptr
        );

        ASSERT_EQ(
            prepTask->Call(strg, taskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage2, S_PREPARE, S_REORDER);

        ASSERT_EQ(
            task.stage,
            stage2
        );

        auto reorderTask = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_ReorderTask });

        ASSERT_EQ(
            reorderTask->Call(strg, taskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage2, S_REORDER, S_SUBMIT);

        ASSERT_EQ(
            task.stage,
            stage2
        );

        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_SubmitTask });

        ASSERT_EQ(
            block->Call(strg, strg),
            MFX_ERR_NONE
        );

        UpdateStage(stage2, S_SUBMIT, S_QUERY);

        ASSERT_EQ(
            task.stage,
            stage2
        );
    }*/

    // TODO: The test is disabled because of move to common TaskManager and Allocator. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksTaskStatic, AsyncRoutineQueryOneTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_QueryTask });

        //Clear QueryTask queue (including General::BLK_CopyBS, General::BLK_UpdateBSInfo)
        auto& queueQT = FeatureBlocks::BQ<FeatureBlocks::BQ_QueryTask>::Get(blocks);
        queueQT.clear();
        //Clear FreeTask queue (including General::BLK_FreeTask)
        auto& queueFT = FeatureBlocks::BQ<FeatureBlocks::BQ_FreeTask>::Get(blocks);
        queueFT.clear();

        auto& fakeTaskStrg = Tmp::CurrTask::Get(fake);

        ASSERT_EQ(
            block->Call(strg, fakeTaskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage1, S_QUERY, S_NEW);

        auto& taskStrg = Tmp::CurrTask::Get(local1);
        auto& task = Task::Common::Get(taskStrg);
        ASSERT_EQ(
            task.stage,
            stage1
        );
    }*/

    // TODO: The test is disabled because of move to common TaskManager and Allocator. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksTaskStatic, AsyncRoutineQueryMoreTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_QueryTask });

        auto& fakeTaskStrg = Tmp::CurrTask::Get(fake);
        auto& fakeTask = Task::Common::Get(fakeTaskStrg);
        fakeTask.pSurfIn = nullptr; // Only in this case will fake task moved into New status

        ASSERT_EQ(
            fakeTask.stage,
            fakeStage
        );

        ASSERT_EQ(
            block->Call(strg, fakeTaskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage2, S_QUERY, S_NEW);
        UpdateStage(fakeStage, S_PREPARE, S_NEW);

        auto& taskStrg = Tmp::CurrTask::Get(local2);
        auto& task = Task::Common::Get(taskStrg);
        ASSERT_EQ(
            task.stage,
            stage2
        );

        ASSERT_EQ(
            fakeTask.stage,
            fakeStage
        );

        ASSERT_EQ(
            block->Call(strg, fakeTaskStrg),
            MFX_ERR_UNDEFINED_BEHAVIOR
        );
    }*/

} //Gen12
} //AV1EHW
