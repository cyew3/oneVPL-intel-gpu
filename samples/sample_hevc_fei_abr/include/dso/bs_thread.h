/******************************************************************************\
Copyright (c) 2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>
#include <algorithm>
#include <exception>

namespace BsThread
{

typedef enum
{
      DONE = 0
    , WORKING
    , WAITING
    , QUEUED
    , LOST
    , FAILED
} State;

typedef State Routine(void*, unsigned int entryCnt);
typedef unsigned int SyncPoint;

struct Task
{
    Routine* Execute;
    void* param;
    unsigned int id;
    int priority;
    unsigned int n;
    std::list<Task*> dependent;
    unsigned int blocked;
    bool detach;
    State state;
    std::mutex mtx;
    std::condition_variable cv;
};

struct Thread
{
    std::mutex mtx;
    std::condition_variable cv;
    std::thread thread;
    Task* task;
    bool terminate;
    unsigned int id;
};

inline bool Ready(State s)
{
    return (s == DONE || s == FAILED || s == LOST);
}

// sync required
class TaskQueueOverflow : public std::exception
{
public:
    TaskQueueOverflow() {/*printf("TaskQueueOverflow\n"); fflush(stdout);*/}
    ~TaskQueueOverflow() {}
};

class Scheduler
{
private:
    std::list<Thread>       m_thread;
    std::list<Task>         m_task;
    std::recursive_mutex    m_mtx;
    std::condition_variable_any m_cv;
    unsigned int            m_id;
    size_t                  m_depth;
    unsigned int            m_locked;

    static void Execute (Thread& self, Scheduler& sync);
    static void Update  (Scheduler& self, Thread* thread);

    void _AbortDependent(Task& task);

public:
    Scheduler();
    ~Scheduler();

    void Init(unsigned int nThreads, unsigned int depth = 128);
    void Close();

    SyncPoint   Submit  (Routine* routine, void* par, int priority, unsigned int nDep, SyncPoint *dep);
    State       Sync    (SyncPoint sp, unsigned int waitMS, bool keepStat = false);
    bool        Abort   (SyncPoint sp, unsigned int waitMS);

    State   AddDependency   (SyncPoint task, unsigned int nDep, SyncPoint *dep);
    void    Detach          (SyncPoint task); // no sync for this task
    bool    WaitForAny      (unsigned int waitMS);
};

};