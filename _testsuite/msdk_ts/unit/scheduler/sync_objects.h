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

/** \brief Sync objects abstract class
 *
 * Sync objects wait while specified condition(s) will not be met.
 * If possible, sync objects return estimation of how long they will wait as well
 * as possible mistake tolerance.
 */
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

} // namespace utils
