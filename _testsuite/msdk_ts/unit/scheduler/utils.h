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

namespace utils {

class Semaphore
{
public:
    Semaphore(int count = 0):
        m_cnt(count)
    {}

    void post() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cnt++;
        m_cond_var.notify_one();
    }

    void wait() {
        std::unique_lock<std::mutex> lock(m_mutex);
        while(0 == m_cnt)
            m_cond_var.wait(lock);
        m_cnt--;
    }

protected:
    int m_cnt;
    std::mutex m_mutex;
    std::condition_variable m_cond_var;
};

void compare(std::chrono::duration<int, std::milli> a, std::chrono::duration<int, std::milli> b)
{
    EXPECT_PRED2( [](mfxU32 a, mfxU32 b) { return a < b; }, a.count(), b.count());
}

} // namespace utils
