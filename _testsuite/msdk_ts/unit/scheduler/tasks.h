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

#include "sync_objects.h"

namespace utils {

struct TaskParam
{
    TaskParam(mfxStatus sts)
      : sts_(sts)
    {}

    // task return value
    mfxStatus sts_;
    // task sync object
    std::shared_ptr<ISyncObject> sync_obj_;
};

struct ITask: public MFX_TASK
{
    ITask(TaskParam* param)
      : param_(param)
    {
        memset((MFX_TASK*)this, 0, sizeof(MFX_TASK));
    }
    virtual ~ITask() {}

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

struct WaitTask: public ITask
{
    WaitTask(TaskParam* param)
      : ITask(param)
    {
        pOwner = (void*)1;
        threadingPolicy = MFX_TASK_THREADING_INTER;
        entryPoint.pState = NULL;
        entryPoint.pParam = param_.get();
        entryPoint.requiredNumThreads = 1;
        entryPoint.pRoutineName = "TestTask";
        entryPoint.pRoutine = TestTask;
    }

    static mfxStatus TestTask(void * s, void * p, mfxU32, mfxU32);
};

mfxStatus WaitTask::TestTask(void * s, void * p, mfxU32, mfxU32)
{
    mfxStatus sts;
    TaskParam* param = (TaskParam*)p;

    sts = param->sts_;
    if (param->sync_obj_.get()) {
        param->sync_obj_->wait();
    }
    return sts;
}

} // namespace utils
