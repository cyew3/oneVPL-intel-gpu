/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include <thread>
#include <vector>
#include <limits>

#include "gtest/gtest.h"

#include "scheduler_tasks_provider.h"

struct TaskConfig
{
    // test name which will be used in SCOPED_TRACE
    const char* scope_;
    // status which task should return
    mfxStatus sts_;
    // task duration before it will return sts_
    std::chrono::duration<int, std::milli> dur_;
    // task duration mistake tolerance
    std::chrono::duration<int, std::milli> tolerance_;
};

class TasksTest : public ::testing::TestWithParam<::TaskConfig> {
protected:
    TasksTest() {
        core_ = new mfxSchedulerCore;
    }

    ~TasksTest() {
        core_->Release();
    }

    mfxSchedulerCore* core_;
};

TEST_P(TasksTest, SingleTask) {
    ::TaskConfig task_cfg = GetParam();
    SCOPED_TRACE(task_cfg.scope_);

    mfxStatus sts;
    mfxSyncPoint syncp;
    utils::Task task(new utils::TaskParam(task_cfg.sts_));

    task.param_->sync_obj_ = std::make_shared<utils::SleepSyncObject>(task_cfg.dur_);

    sts = core_->Initialize(NULL);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    sts = core_->AddTask(task, &syncp);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    auto start = std::chrono::high_resolution_clock::now();

    sts = core_->Synchronize(syncp, 0);
    EXPECT_EQ(MFX_WRN_IN_EXECUTION, sts);

    mfxU32 msec_task_dur;
    ASSERT_TRUE(task.get_expected_exectime(msec_task_dur));

    sts = core_->Synchronize(syncp, msec_task_dur/2);
    EXPECT_EQ(MFX_WRN_IN_EXECUTION, sts);

    sts = core_->Synchronize(syncp, msec_task_dur*2);
    EXPECT_EQ(task.get_expected_status(), sts);

    utils::compare(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start),
        task_cfg.dur_ + std::chrono::milliseconds(task.get_tolerance()));

    sts = core_->Reset();
    EXPECT_EQ(MFX_ERR_NONE, sts);
}

TEST_P(TasksTest, MultipleWaiters) {
    ::TaskConfig task_cfg = GetParam();
    SCOPED_TRACE(task_cfg.scope_);

    mfxStatus sts;
    mfxSyncPoint syncp;
    utils::Task task(new utils::TaskParam(task_cfg.sts_));

    task.param_->sync_obj_ = std::make_shared<utils::SleepSyncObject>(task_cfg.dur_);

    sts = core_->Initialize(NULL);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    sts = core_->AddTask(task, &syncp);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    mfxU32 msec_task_dur;
    ASSERT_TRUE(task.get_expected_exectime(msec_task_dur));

    auto start = std::chrono::high_resolution_clock::now();
    auto syncf = [&] {
        mfxStatus sts = core_->Synchronize(syncp, 0);
        EXPECT_EQ(MFX_WRN_IN_EXECUTION, sts);

        sts = core_->Synchronize(syncp, msec_task_dur/2);
        EXPECT_EQ(MFX_WRN_IN_EXECUTION, sts);

        sts = core_->Synchronize(syncp, msec_task_dur*2);
        EXPECT_EQ(task.get_expected_status(), sts);
    };

    std::thread th1(syncf);
    std::thread th2(syncf);
    std::thread th3(syncf);
    std::thread th4(syncf);

    th1.join();
    th2.join();
    th3.join();
    th4.join();

    utils::compare(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start),
        task_cfg.dur_ + std::chrono::milliseconds(task.get_tolerance()));

    sts = core_->Reset();
    EXPECT_EQ(MFX_ERR_NONE, sts);
}

TEST_P(TasksTest, SingleThread) {
    ::TaskConfig task_cfg = GetParam();
    SCOPED_TRACE(task_cfg.scope_);

    mfxStatus sts;
    mfxSyncPoint syncp;
    utils::Semaphore sem;
    utils::Task task(new utils::TaskParam(task_cfg.sts_));
    MFX_SCHEDULER_PARAM sched_params = { MFX_SINGLE_THREAD, 0, NULL, };

    task.param_->sync_obj_ = std::make_shared<utils::SleepSyncObject>(task_cfg.dur_);

    // Start waiter thread
    auto syncf = [&] {
        sem.wait();
        mfxStatus sts = core_->Synchronize(syncp, std::numeric_limits<mfxU32>::max());
        EXPECT_EQ(MFX_ERR_NONE, sts);
    };
    std::thread sync_th(syncf);

    sts = core_->Initialize(&sched_params);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    sts = core_->AddTask(task, &syncp);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    sts = core_->Synchronize(syncp, 0);
    EXPECT_EQ(task_cfg.sts_, sts);

    auto start = std::chrono::high_resolution_clock::now();
    sem.post();
    sync_th.join();
    utils::compare(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start),
            task_cfg.dur_ + std::chrono::milliseconds(task.get_tolerance()));

    sts = core_->Reset();
    EXPECT_EQ(MFX_ERR_NONE, sts);
}

TEST_F(TasksTest, MultiframeHerd) {
    mfxStatus sts;
    utils::MultiframeTaskProvider task_provider(MFX_ERR_NONE, 4);

    sts = core_->Initialize(NULL);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    std::thread th1(utils::TaskProducer, core_, &task_provider, std::chrono::seconds(1));
    std::thread th2(utils::TaskProducer, core_, &task_provider, std::chrono::seconds(1));
    std::thread th3(utils::TaskProducer, core_, &task_provider, std::chrono::seconds(1));
    std::thread th4(utils::TaskProducer, core_, &task_provider, std::chrono::seconds(1));

    th1.join();
    th2.join();
    th3.join();
    th4.join();

    sts = core_->Reset();
    EXPECT_EQ(MFX_ERR_NONE, sts);
}

TEST_F(TasksTest, FlowOfErrorsInParallelThreads) {
    mfxStatus sts;
    utils::SleepTaskProvider succeed_tasks_provider(MFX_ERR_NONE, std::chrono::milliseconds(1));
    utils::SleepTaskProvider failed_tasks_provider(MFX_ERR_UNKNOWN, std::chrono::milliseconds(1));

    sts = core_->Initialize(NULL);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    std::thread th1(utils::TaskProducer, core_, &succeed_tasks_provider, std::chrono::seconds(1));
    std::thread th2(utils::TaskProducer, core_, &failed_tasks_provider, std::chrono::seconds(1));
    std::thread th3(utils::TaskProducer, core_, &succeed_tasks_provider, std::chrono::seconds(1));
    std::thread th4(utils::TaskProducer, core_, &failed_tasks_provider, std::chrono::seconds(1));

    th1.join();
    th2.join();
    th3.join();
    th4.join();

    sts = core_->Reset();
    EXPECT_EQ(MFX_ERR_NONE, sts);
}

TEST_F(TasksTest, TasksLatency) {
    mfxStatus sts;
    utils::SleepTaskProvider tasks_provider(MFX_ERR_NONE, std::chrono::milliseconds(20));

    sts = core_->Initialize(NULL);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    std::thread th1(utils::TaskProducer, core_, &tasks_provider, std::chrono::seconds(1));
    std::thread th2(utils::TaskProducer, core_, &tasks_provider, std::chrono::seconds(1));
    std::thread th3(utils::TaskProducer, core_, &tasks_provider, std::chrono::seconds(1));
    std::thread th4(utils::TaskProducer, core_, &tasks_provider, std::chrono::seconds(1));

    th1.join();
    th2.join();
    th3.join();
    th4.join();

    sts = core_->Reset();
    EXPECT_EQ(MFX_ERR_NONE, sts);
}

static std::vector<::TaskConfig> configs({
    { "Succeeded 100ms Task", MFX_ERR_NONE, std::chrono::milliseconds(100) },
    { "Succeeded 250ms Task", MFX_ERR_NONE, std::chrono::milliseconds(250) },
    { "Succeeded 1024ms Task", MFX_ERR_NONE, std::chrono::milliseconds(1024) },
    { "Failed MFX_ERR_UNKNOWN 100ms Task", MFX_ERR_UNKNOWN, std::chrono::milliseconds(100) },
    { "Failed MFX_ERR_UNKNOWN 250ms Task", MFX_ERR_UNKNOWN, std::chrono::milliseconds(250) },
    { "Failed MFX_ERR_UNKNOWN 1024ms Task", MFX_ERR_UNKNOWN, std::chrono::milliseconds(1024) },
});

INSTANTIATE_TEST_CASE_P(SingleTask, TasksTest, ::testing::ValuesIn(configs));
INSTANTIATE_TEST_CASE_P(MultipleWaiters, TasksTest, ::testing::ValuesIn(configs));
INSTANTIATE_TEST_CASE_P(SingleThread, TasksTest, ::testing::ValuesIn(configs));
