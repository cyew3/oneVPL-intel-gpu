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

#include "ehw_resources_pool.h"
#include "av1ehw_base_alloc.h"
#include "av1ehw_base_general.h"
#include "av1ehw_base_task.h"

namespace AV1EHW
{
namespace Base
{
    class ResPoolStub: public MfxEncodeHW::ResPool {
    public:
        ResPoolStub(VideoCORE& core);

        virtual mfxStatus Alloc(
         const mfxFrameAllocRequest & request
        , bool isCopyRequired)
        {
            auto req = request;
            req.NumFrameSuggested = req.NumFrameMin;

            m_response.NumFrameActual = req.NumFrameMin;
            MFX_CHECK(m_response.NumFrameActual >= req.NumFrameMin, MFX_ERR_MEMORY_ALLOC);

            m_locked.resize(req.NumFrameMin, 0);
            std::fill(m_locked.begin(), m_locked.end(), 0);

            m_flag.resize(req.NumFrameMin, 0);
            std::fill(m_flag.begin(), m_flag.end(), 0);

            m_info                    = req.Info;
            m_numFrameActual          = m_response.NumFrameActual;
            m_response.NumFrameActual = req.NumFrameMin;
            m_bExternal               = false;
            m_bOpaque                 = false;

            return MFX_ERR_NONE;
        }

    protected:
        ResPoolStub(ResPoolStub const &) = delete;
        ResPoolStub & operator =(ResPoolStub const &) = delete;
    };

    ResPoolStub::ResPoolStub(VideoCORE& core)
    : MfxEncodeHW::ResPool::ResPool(core)
    {
    }

    template<typename TR, typename ...TArgs>
    inline typename CallChain<TR, TArgs...>::TInt
    WrapCC(TR(MfxEncodeHW::ResPool::*pfn)(TArgs...), MfxEncodeHW::ResPool* pAlloc)
    {
        return [=](typename CallChain<TR, TArgs...>::TExt, TArgs ...arg)
        {
            return (pAlloc->*pfn)(arg...);
        };
    }

    template<typename TR, typename ...TArgs>
    inline typename CallChain<TR, TArgs...>::TInt
    WrapCC(TR(MfxEncodeHW::ResPool::*pfn)(TArgs...) const, const MfxEncodeHW::ResPool* pAlloc)
    {
        return [=](typename CallChain<TR, TArgs...>::TExt, TArgs ...arg)
        {
            return (pAlloc->*pfn)(arg...);
        };
    }

    IAllocation* MakeAlloc(std::unique_ptr<ResPoolStub>&& upAlloc)
    {
        std::unique_ptr<IAllocation> pIAlloc(new IAllocation);

        bool bInvalid = !(pIAlloc && upAlloc);
        if (bInvalid)
            return nullptr;

        auto pAlloc = upAlloc.release();
        pIAlloc->m_pthis.reset(pAlloc);

    #define WRAP_CC(X) pIAlloc->X.Push(WrapCC(&MfxEncodeHW::ResPool::X, pAlloc))
        WRAP_CC(Alloc);
        WRAP_CC(AllocOpaque);
        WRAP_CC(GetResponse);
        WRAP_CC(GetInfo);
        WRAP_CC(Release);
        WRAP_CC(ClearFlag);
        WRAP_CC(SetFlag);
        WRAP_CC(GetFlag);
        WRAP_CC(UnlockAll);

        pIAlloc->Acquire.Push([=](IAllocation::TAcquire::TExt) -> Resource
        {
            auto     res0 = pAlloc->Acquire();
            Resource res1;
            res1.Idx = res0.Idx;
            res1.Mid = res0.Mid;
            return res1;
        });

        return pIAlloc.release();
    }

    auto CreateAllocator = [](VideoCORE& core) -> IAllocation*
    {
        return MakeAlloc(std::unique_ptr<ResPoolStub>(new ResPoolStub(core)));
    };

    struct FeatureBlocksTask
        : testing::Test
    {
        MFXVideoSession                       session;

        FeatureBlocks                         blocks{};
        General                               general;
        Allocator                             alloc;
        TaskManager                           taskMgr;
        StorageRW                             global;
        StorageRW                             local;
        StorageRW                             taskStrg;

        FeatureBlocksTask()
            : general(FEATURE_GENERAL)
            , alloc(FEATURE_ALLOCATOR)
            , taskMgr(FEATURE_TASK_MANAGER)
        {

        }

        void SetUp() override
        {
            EXPECT_EQ(
                session.InitEx(
                    mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                ),
                MFX_ERR_NONE
            );

            auto s = static_cast<_mfxSession*>(session);
            global.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(*(s->m_pCORE.get())));

            auto& vp = Glob::VideoParam::GetOrConstruct(global);
            vp.AsyncDepth = 1;
            vp.mfx.GopPicSize = 1;
            vp.mfx.FrameInfo.Width = 64;
            vp.mfx.FrameInfo.Height = 64;
            vp.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

            general.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);
            alloc.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);
            taskMgr.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);

            Glob::EncodeCaps::GetOrConstruct(global);

            auto& queueII = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            auto setReorder = FeatureBlocks::Get(queueII, { FEATURE_GENERAL, General::BLK_SetReorder });
            EXPECT_EQ(
                setReorder->Call(global, global),
                MFX_ERR_NONE
            );

            local.Insert(Tmp::MakeAlloc::Key, new Tmp::MakeAlloc::TRef(CreateAllocator));

            auto pInfo = make_storable<mfxFrameAllocRequest>(mfxFrameAllocRequest{});
            pInfo->Info.Width = 64;
            pInfo->Info.Height = 64;
            pInfo->Info.FourCC = MFX_FOURCC_NV12;
            local.Insert(Tmp::BSAllocInfo::Key, std::move(pInfo));

            vp.NewEB(MFX_EXTBUFF_AV1_PARAM, false);
            vp.NewEB(MFX_EXTBUFF_CODING_OPTION3, false);

            auto& queueIA = FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks);
            auto allocBS = FeatureBlocks::Get(queueIA, { FEATURE_GENERAL, General::BLK_AllocBS });
            EXPECT_EQ(
                allocBS->Call(global, local),
                MFX_ERR_NONE
            );
        };

        void TearDown() override
        {

        }
    };

    TEST_F(FeatureBlocksTask, FrameSubmitNoSurf)
    {
        auto& queueFS = FeatureBlocks::BQ<FeatureBlocks::BQ_FrameSubmit>::Get(blocks);
        auto block = FeatureBlocks::Get(queueFS, { FEATURE_TASK_MANAGER, TaskManager::BLK_NewTask });

        mfxEncodeCtrl* pCtrl = nullptr;
        mfxFrameSurface1* pSurf = nullptr;
        mfxBitstream bs{};
        EXPECT_EQ(
            block->Call(pCtrl, pSurf, bs, global, local),
            MFX_ERR_MORE_DATA
        );
    }

    TEST_F(FeatureBlocksTask, FrameSubmitNoTask)
    {
        auto& queueFS = FeatureBlocks::BQ<FeatureBlocks::BQ_FrameSubmit>::Get(blocks);
        auto block = FeatureBlocks::Get(queueFS, { FEATURE_TASK_MANAGER, TaskManager::BLK_NewTask });

        mfxEncodeCtrl ctrl{};
        mfxFrameSurface1 surf{};
        mfxBitstream bs{};
        EXPECT_EQ(
            block->Call(&ctrl, &surf, bs, global, local),
            MFX_WRN_DEVICE_BUSY
        );
    }

    TEST_F(FeatureBlocksTask, FrameSubmitNeedMoreData)
    {
        auto& queueIA = FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks);
        auto init = FeatureBlocks::Get(queueIA, { FEATURE_TASK_MANAGER, TaskManager::BLK_Init });

        auto& reorder = Glob::Reorder::Get(global);
        reorder.BufferSize = 2;
        EXPECT_EQ(
            init->Call(global, local),
            MFX_ERR_NONE
        );

        auto& queueFS = FeatureBlocks::BQ<FeatureBlocks::BQ_FrameSubmit>::Get(blocks);
        auto block = FeatureBlocks::Get(queueFS, { FEATURE_TASK_MANAGER, TaskManager::BLK_NewTask });

        mfxEncodeCtrl ctrl = {};
        mfxFrameSurface1 surf = {};
        mfxBitstream bs{};
        EXPECT_EQ(
            block->Call(&ctrl, &surf, bs, global, local),
            MFX_ERR_MORE_DATA_SUBMIT_TASK
        );
    }

    TEST_F(FeatureBlocksTask, AsyncRoutinePrepTaskNotReady)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_PrepareTask });

        auto& task = Task::Common::GetOrConstruct(taskStrg);
        task.stage = S_SUBMIT;
        EXPECT_EQ(
            block->Call(global, taskStrg),
            MFX_ERR_NONE
        );
    }

    TEST_F(FeatureBlocksTask, AsyncRoutinePrepTaskNoSurf)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_PrepareTask });

        auto& task = Task::Common::GetOrConstruct(taskStrg);
        task.stage = S_PREPARE;

        ASSERT_EQ(
            task.pSurfIn,
            nullptr
        );

        EXPECT_EQ(
            block->Call(global, taskStrg),
            MFX_ERR_NONE
        );
    }

    TEST_F(FeatureBlocksTask, AsyncRoutinePrepTaskNoTask)
    {
        auto& queueQ1 = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks);
        auto setDefault = FeatureBlocks::Get(queueQ1, { FEATURE_GENERAL, General::BLK_SetDefaultsCallChain });
        auto& vp = Glob::VideoParam::GetOrConstruct(global);
        EXPECT_EQ(
            setDefault->Call(vp, vp, global),
            MFX_ERR_NONE
        );

        auto& queueIA = FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks);
        auto initTaskMgr = FeatureBlocks::Get(queueIA, { FEATURE_TASK_MANAGER, TaskManager::BLK_Init });
        EXPECT_EQ(
            initTaskMgr->Call(global, local),
            MFX_ERR_NONE
        );

        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_PrepareTask });

        auto& task = Task::Common::GetOrConstruct(taskStrg);
        task.stage = S_PREPARE;
        mfxFrameSurface1 surf;
        task.pSurfIn = &surf;

        ASSERT_NE(
            task.pSurfIn,
            nullptr
        );

        EXPECT_EQ(
            block->Call(global, taskStrg),
            MFX_ERR_UNDEFINED_BEHAVIOR
        );
    }

    TEST_F(FeatureBlocksTask, AsyncRoutineSubmitTaskNoTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_SubmitTask });
        EXPECT_EQ(
            block->Call(global, global),
            MFX_ERR_NONE
        );
    }

    inline void UpdateStage(mfxU32& stage, const mfxU16 from, const mfxU16 to)
    {
        stage |= (1 << from);
        stage &= ~(0xffffffff << to);
    }

    // this class is defined w/ static fields as below test cases have dependency and need to keep context
    struct FeatureBlocksTaskStatic
        : testing::Test
    {
        static MFXVideoSession           session;

        static FeatureBlocks             blocks;
        static General                   general;
        static Allocator                 alloc;
        static TaskManager               taskMgr;
        static StorageRW                 global;

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
            EXPECT_EQ(
                session.InitEx(
                    mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                ),
                MFX_ERR_NONE
            );

            auto s = static_cast<_mfxSession*>(session);
            global.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(*(s->m_pCORE.get())));

            auto& vp = Glob::VideoParam::GetOrConstruct(global);
            vp.AsyncDepth = 1;
            vp.mfx.GopPicSize = 1;
            vp.mfx.FrameInfo.Width = 64;
            vp.mfx.FrameInfo.Height = 64;
            vp.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

            general.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);
            alloc.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);
            taskMgr.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);

            Glob::EncodeCaps::GetOrConstruct(global);

            auto& queueII = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
            auto setReorder = FeatureBlocks::Get(queueII, { FEATURE_GENERAL, General::BLK_SetReorder });
            EXPECT_EQ(
                setReorder->Call(global, global),
                MFX_ERR_NONE
            );

            local1.Insert(Tmp::MakeAlloc::Key, new Tmp::MakeAlloc::TRef(CreateAllocator));
            auto pInfo = make_storable<mfxFrameAllocRequest>(mfxFrameAllocRequest{});
            pInfo->Info.Width = 64;
            pInfo->Info.Height = 64;
            pInfo->Info.FourCC = MFX_FOURCC_NV12;
            pInfo->NumFrameMin = 2;
            local1.Insert(Tmp::BSAllocInfo::Key, std::move(pInfo));

            vp.NewEB(MFX_EXTBUFF_AV1_PARAM, false);
            vp.NewEB(MFX_EXTBUFF_CODING_OPTION3, false);

            auto& queueIA = FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks);
            auto allocBS = FeatureBlocks::Get(queueIA, { FEATURE_GENERAL, General::BLK_AllocBS });
            EXPECT_EQ(
                allocBS->Call(global, local1),
                MFX_ERR_NONE
            );
        }

        static void TearDownTestCase()
        {
            global.Clear();
            local1.Clear();
            local2.Clear();
            fake.Clear();
        }
    };

    MFXVideoSession FeatureBlocksTaskStatic::session{};
    FeatureBlocks FeatureBlocksTaskStatic::blocks{};
    General FeatureBlocksTaskStatic::general(FEATURE_GENERAL);
    Allocator FeatureBlocksTaskStatic::alloc(FEATURE_ALLOCATOR);
    TaskManager FeatureBlocksTaskStatic::taskMgr(FEATURE_TASK_MANAGER);
    StorageRW FeatureBlocksTaskStatic::global{};

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

    TEST_F(FeatureBlocksTaskStatic, InitAlloc)
    {
        auto& queueQ1 = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks);
        auto setDefault = FeatureBlocks::Get(queueQ1, { FEATURE_GENERAL, General::BLK_SetDefaultsCallChain });
        auto& vp = Glob::VideoParam::GetOrConstruct(global);

        auto& reorder = Glob::Reorder::Get(global);
        reorder.BufferSize = 2;
        EXPECT_EQ(
            setDefault->Call(vp, vp, global),
            MFX_ERR_NONE
        );

        auto& queueIA = FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks);
        auto block = FeatureBlocks::Get(queueIA, { FEATURE_TASK_MANAGER, TaskManager::BLK_Init });
        EXPECT_EQ(
            block->Call(global, local1),
            MFX_ERR_NONE
        );
    }

    TEST_F(FeatureBlocksTaskStatic, FrameSubmit)
    {
        auto& queueFS = FeatureBlocks::BQ<FeatureBlocks::BQ_FrameSubmit>::Get(blocks);
        auto block = FeatureBlocks::Get(queueFS, { FEATURE_TASK_MANAGER, TaskManager::BLK_NewTask });
        EXPECT_EQ(
            block->Call(&ctrl1, &surf1, bs1, global, local1),
            MFX_ERR_MORE_DATA_SUBMIT_TASK
        );

        UpdateStage(stage1, S_NEW, S_PREPARE);

        auto& taskStrg1 = Tmp::CurrTask::Get(local1);
        auto& task1 = Task::Common::Get(taskStrg1);
        ASSERT_EQ(
            task1.stage,
            stage1
        );

        EXPECT_EQ(
            block->Call(&ctrl2, &surf2, bs2, global, local2),
            MFX_ERR_MORE_DATA_SUBMIT_TASK
        );

        UpdateStage(stage2, S_NEW, S_PREPARE);

        auto& taskStrg2 = Tmp::CurrTask::Get(local2);
        auto& task2 = Task::Common::Get(taskStrg2);
        ASSERT_EQ(
            task2.stage,
            stage2
        );

        EXPECT_EQ(
            block->Call(&ctrl2, &surf2, bs2, global, fake),
            MFX_ERR_NONE
        );

        UpdateStage(fakeStage, S_NEW, S_PREPARE);

        auto& fakeTaskStrg = Tmp::CurrTask::Get(fake);
        auto& fakeTask = Task::Common::Get(fakeTaskStrg);
        EXPECT_EQ(
            fakeTask.stage,
            fakeStage
        );
    }

    TEST_F(FeatureBlocksTaskStatic, AsyncRoutinePrepTask)
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

        EXPECT_EQ(
            block->Call(global, taskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage1, S_PREPARE, S_REORDER);

        EXPECT_EQ(
            task.stage,
            stage1
        );
    }

    TEST_F(FeatureBlocksTaskStatic, AsyncRoutineReorderTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_ReorderTask });

        auto& taskStrg = Tmp::CurrTask::Get(local1);
        auto& task = Task::Common::Get(taskStrg);

        //Clear PostReorderTask queue to avoid create Rec and BS surface...
        auto& queuePostRT = FeatureBlocks::BQ<FeatureBlocks::BQ_PostReorderTask>::Get(blocks);
        queuePostRT.clear();
        EXPECT_EQ(
            block->Call(global, taskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage1, S_REORDER, S_SUBMIT);

        EXPECT_EQ(
            task.stage,
            stage1
        );
    }

    TEST_F(FeatureBlocksTaskStatic, AsyncRoutineSubmitOneTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_SubmitTask });

        //Clear SubmitTask queue (including General::BLK_GetRawHDL, General::BLK_CopySysToRaw)
        auto& queueST = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        queueST.clear();

        auto& taskStrg = Tmp::CurrTask::Get(local1);
        EXPECT_EQ(
            block->Call(global, taskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage1, S_SUBMIT, S_QUERY);

        auto& task = Task::Common::Get(taskStrg);
        EXPECT_EQ(
            task.stage,
            stage1
        );
    }

    TEST_F(FeatureBlocksTaskStatic, AsyncRoutineSubmitAnotherTask)
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

        EXPECT_EQ(
            prepTask->Call(global, taskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage2, S_PREPARE, S_REORDER);

        EXPECT_EQ(
            task.stage,
            stage2
        );

        auto reorderTask = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_ReorderTask });
        EXPECT_EQ(
            reorderTask->Call(global, taskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage2, S_REORDER, S_SUBMIT);

        EXPECT_EQ(
            task.stage,
            stage2
        );

        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_SubmitTask });
        EXPECT_EQ(
            block->Call(global, taskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage2, S_SUBMIT, S_QUERY);

        EXPECT_EQ(
            task.stage,
            stage2
        );
    }

    TEST_F(FeatureBlocksTaskStatic, AsyncRoutineQueryOneTask)
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

        EXPECT_EQ(
            block->Call(global, fakeTaskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage1, S_QUERY, S_NEW);

        auto& taskStrg = Tmp::CurrTask::Get(local1);
        auto& task = Task::Common::Get(taskStrg);
        EXPECT_EQ(
            task.stage,
            stage1
        );
    }

    TEST_F(FeatureBlocksTaskStatic, AsyncRoutineQueryMoreTask)
    {
        auto& queueAR = FeatureBlocks::BQ<FeatureBlocks::BQ_AsyncRoutine>::Get(blocks);
        auto block = FeatureBlocks::Get(queueAR, { FEATURE_TASK_MANAGER, TaskManager::BLK_QueryTask });

        auto& fakeTaskStrg = Tmp::CurrTask::Get(fake);
        auto& fakeTask = Task::Common::Get(fakeTaskStrg);
        fakeTask.pSurfIn = nullptr; // Only in this case will fake task moved into New status
        EXPECT_EQ(
            fakeTask.stage,
            fakeStage
        );

        EXPECT_EQ(
            block->Call(global, fakeTaskStrg),
            MFX_ERR_NONE
        );

        UpdateStage(stage2, S_QUERY, S_NEW);
        UpdateStage(fakeStage, S_PREPARE, S_NEW);

        auto& taskStrg = Tmp::CurrTask::Get(local2);
        auto& task = Task::Common::Get(taskStrg);
        EXPECT_EQ(
            task.stage,
            stage2
        );

        EXPECT_EQ(
            fakeTask.stage,
            fakeStage
        );

        EXPECT_EQ(
            block->Call(global, fakeTaskStrg),
            MFX_ERR_UNDEFINED_BEHAVIOR
        );
    }

} //Base
} //AV1EHW
