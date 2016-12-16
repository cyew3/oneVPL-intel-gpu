/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#pragma once

#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

#include "gtest/gtest.h"

#include "mfx_scheduler_core.h"
#include "libmfx_core_interface.h"

#include "sync_objects.h"
#include "tasks.h"
#include "utils.h"

namespace utils {

/** \breif Abstract class returning mediasdk scheduler tasks
 *
 * By calling get() user ontains task parameter object which can be used to
 * schedule a task. User is responsible to call 'delete' to release memory
 *
 * By calling stop() user reqests provider to stop produce
 * more tasks, however, provider will really stop once it will return 'true'
 * from the stop() function. Until then user may continue to get tasks with
 * get(), though tasks object needs to be explicitly checked for NULL.
 */
struct ITaskProvider
{
    virtual ~ITaskProvider() {}
    // Task provider itself is responsible to cleanup tasks on destruction,
    // so caller should do nothing to cleanup the task.
    virtual ITask* get() = 0;
    virtual bool stop() = 0;

    std::list< std::shared_ptr<ITask> > garbage_;
};

struct SleepTaskProvider: public ITaskProvider
{
    SleepTaskProvider(mfxStatus sts, std::chrono::duration<int, std::micro> dur)
      : stop_(false)
      , sts_(sts)
      , dur_(dur)
    {}

    virtual ITask* get() {
        std::unique_lock<std::mutex> lock(mutex_);
        std::shared_ptr<WaitTask> task = std::make_shared<WaitTask>(get_params());
        garbage_.push_back(task);
        return task.get();
    }

    virtual bool stop() {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
        return true;
    }

protected:
    virtual TaskParam* get_params() {
        if (stop_) return NULL;
        TaskParam* param = new TaskParam(sts_);
        param->sync_obj_ = std::make_shared<SleepSyncObject>(dur_);
        return param;
    }

protected:
    std::mutex mutex_;
    bool stop_;
    mfxStatus sts_;
    std::chrono::duration<int, std::micro> dur_;
};

struct MultiframeTaskProvider: public ITaskProvider
{
    MultiframeTaskProvider(mfxStatus sts, int sync_cnt)
      : stopped_(false)
      , stop_(false)
      , sts_(sts)
      , cnt_(0)
      , sync_cnt_(sync_cnt)
      , obj_(NULL)
    {}

    virtual ITask* get() {
        std::unique_lock<std::mutex> lock(mutex_);
        std::shared_ptr<WaitTask> task = std::make_shared<WaitTask>(get_params());
        garbage_.push_back(task);
        return task.get();
    }

    virtual bool stop() {
        stop_ = true;
        if (!obj_) {
            stopped_ = true;
            return true;
        }
        return false;
    }

protected:
    virtual TaskParam* get_params() {
        if (stopped_) return NULL;

        TaskParam* param = new TaskParam(sts_);

        if (!obj_) {
          obj_ = std::make_shared<MultiframeSyncObject>(sync_cnt_);
        }
        ++cnt_;
        param->sync_obj_ = obj_;
        if (cnt_ >= sync_cnt_) {
          cnt_ = 0;
          obj_ = NULL;
        }
        return param;
    }

protected:
    std::mutex mutex_;
    bool stopped_;
    bool stop_;
    mfxStatus sts_;
    int cnt_;
    int sync_cnt_;
    std::shared_ptr<MultiframeSyncObject> obj_;
};

static void TaskProducer(
    mfxSchedulerCore * core,
    ITaskProvider * provider,
    std::chrono::duration<int, std::milli> dur)
{
    mfxStatus sts;

    ASSERT_TRUE(core);
    ASSERT_TRUE(provider);

    auto start = std::chrono::high_resolution_clock::now();
    bool stop = false;
    do {
        ITask* task = provider->get();
        mfxSyncPoint syncp;

        if (task->entryPoint.pParam) {
            auto task_start = std::chrono::high_resolution_clock::now();

            sts = core->AddTask(*task, &syncp);
            EXPECT_EQ(MFX_ERR_NONE, sts);

            mfxStatus sts = core->Synchronize(syncp, dur.count());
            EXPECT_EQ(task->get_expected_status(), sts);

            mfxU32 msec_task_dur;
            if (task->get_expected_exectime(msec_task_dur)) {
                compare(
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - task_start),
                    std::chrono::milliseconds(msec_task_dur + task->get_tolerance()));
            }
        }

        if (std::chrono::high_resolution_clock::now() - start >= dur) {
            stop = provider->stop();
        }
    } while(!stop);
}

} // namespace utils
