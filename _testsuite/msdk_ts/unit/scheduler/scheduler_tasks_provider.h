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
#include <mutex>
#include <thread>

#include "gtest/gtest.h"

#include "mfx_scheduler_core.h"
#include "libmfx_core_interface.h"

namespace utils {

struct ISyncObject
{
    virtual ~ISyncObject() {}
    virtual void wait()=0;
    // if known, returns 'true' and time required for the object synchronization
    virtual bool get_synctime(mfxU32 & msec)=0;
    // returns milliseconds tolerance of object synchronization time mistake
    // should be used in conjunction with get_synctime()
    virtual mfxU32 get_tolerance()=0;
};

struct SleepSyncObject: public ISyncObject
{
    SleepSyncObject(std::chrono::duration<int, std::micro> dur)
      : dur_(dur)
    {}

    virtual void wait() {
        std::this_thread::sleep_for(dur_);
    }

    virtual bool get_synctime(mfxU32 & msec) {
        msec = std::chrono::duration_cast<std::chrono::milliseconds>(dur_).count();
        return true;
    }

    virtual mfxU32 get_tolerance() {
        // TODO this may vary on the OS... consider another value for Windows
        return std::chrono::milliseconds(10).count();
    }

protected:
    std::chrono::duration<int, std::micro> dur_;
};

struct MultiframeSyncObject: public ISyncObject
{
    MultiframeSyncObject(int sync_cnt)
      : cnt_(0)
      , sync_cnt_(sync_cnt)
    {}

    virtual void wait() {
        std::unique_lock<std::mutex> lock(m_);

        if (++cnt_ < sync_cnt_) {
          cv_.wait(lock, [&]{ return !(cnt_ < sync_cnt_); });
        } else {
          cv_.notify_all();
        }
    }

    virtual bool get_synctime(mfxU32 & msec) {
        return false;
    }

    virtual mfxU32 get_tolerance() {
        return 0;
    }

protected:
    std::mutex m_;
    std::condition_variable cv_;
    int cnt_;
    int sync_cnt_;
};

struct TaskParam
{
    TaskParam(mfxStatus sts)
      : sts_(sts)
    {}

    TaskParam& operator=(const TaskParam& arg) {
        sts_ = arg.sts_;
        sync_obj_ = arg.sync_obj_;
    }

    // task return value
    mfxStatus sts_;
    // task sync object
    std::shared_ptr<ISyncObject> sync_obj_;
};

struct Task: public MFX_TASK
{
    Task(TaskParam* param)
      : param_(param)
    {
        memset((MFX_TASK*)this, 0, sizeof(MFX_TASK));
        pOwner = (void*)1;
        threadingPolicy = MFX_TASK_THREADING_INTER;
        entryPoint.pState = NULL;
        entryPoint.pParam = param_.get();
        entryPoint.requiredNumThreads = 1;
        entryPoint.pRoutineName = "TestTask";
        entryPoint.pRoutine = TestTask;
    }

    static mfxStatus TestTask(void * s, void * p, mfxU32, mfxU32);

    inline mfxStatus get_expected_status() {
      // scheduler ignores all warnings and considers them as WRN_IN_EXECUTION
      //if (param_->sts_ > MFX_ERR_NONE) return MFX_WRN_IN_EXECUTION;
      //return param_->sts_;
      switch (param_->sts_) {
        case MFX_ERR_NONE:
          return MFX_ERR_NONE;
        case MFX_ERR_UNKNOWN:
          return MFX_ERR_UNKNOWN;
        case MFX_ERR_DEVICE_FAILED:
          return MFX_ERR_DEVICE_FAILED;
        case MFX_ERR_GPU_HANG:
          return MFX_ERR_GPU_HANG;
        // TODO add all other mediasdk status codes
      }
      // TODO: what is scheduler behavior for unknown mfxStatus? right now scheduler reschedules task forever (ex.: mfxStatus(666)).
      return MFX_ERR_UNKNOWN;
    }

    inline bool get_expected_exectime(mfxU32 & msec) {
        return param_->sync_obj_->get_synctime(msec);
    }
    inline mfxU32 get_tolerance() {
        return param_->sync_obj_->get_tolerance();
    }
    
    std::unique_ptr<TaskParam> param_;
};

mfxStatus Task::TestTask(void * s, void * p, mfxU32, mfxU32)
{
    mfxStatus sts;
    TaskParam param = *(TaskParam*)p; // copy input data asap

    sts = param.sts_;
    if (param.sync_obj_.get()) {
        param.sync_obj_->wait();
    }
    return sts;
}

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
    virtual TaskParam* get() = 0;
    virtual bool stop() = 0;
};

struct SleepTaskProvider: public ITaskProvider
{
    SleepTaskProvider(mfxStatus sts, std::chrono::duration<int, std::micro> dur)
      : stop_(false)
      , sts_(sts)
      , dur_(dur)
    {}

    virtual TaskParam* get() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_) return NULL;
        TaskParam* param = new TaskParam(sts_);
        param->sync_obj_ = std::make_shared<SleepSyncObject>(dur_);
        return param;
    }

    virtual bool stop() {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
        return true;
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

    virtual TaskParam* get() {
        std::unique_lock<std::mutex> lock(mutex_);

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

    virtual bool stop() {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
        if (!obj_) {
            stopped_ = true;
            return true;
        }
        return false;
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

void compare(std::chrono::duration<int, std::milli> a, std::chrono::duration<int, std::milli> b)
{
    EXPECT_PRED2( [](mfxU32 a, mfxU32 b) { return a < b; }, a.count(), b.count());
}

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
        Task task(provider->get());
        mfxSyncPoint syncp;

        if (task.entryPoint.pParam) {
            auto task_start = std::chrono::high_resolution_clock::now();

            sts = core->AddTask(task, &syncp);
            EXPECT_EQ(MFX_ERR_NONE, sts);

            mfxStatus sts = core->Synchronize(syncp, dur.count());
            EXPECT_EQ(task.get_expected_status(), sts);

            mfxU32 msec_task_dur;
            if (task.get_expected_exectime(msec_task_dur)) {
                compare(
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - task_start),
                    std::chrono::milliseconds(msec_task_dur + task.get_tolerance()));
            }
        }

        if (std::chrono::high_resolution_clock::now() - start >= dur) {
            stop = provider->stop();
        }
    } while(!stop);
}

} // namespace utils
