/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include <list>
#include "mfxdefs.h"
#include "umc_event.h"
#include "umc_thread.h"
#include "umc_mutex.h"

#ifndef WIN32
    #include <sys/types.h>
#endif

namespace MFXThread
{
    class Task {
        friend class ThreadPool;
    protected:
        UMC::Event * mTaskReady;
        mfxStatus mStatus;
    public:
        Task(UMC::Event & taskReady)
            : mTaskReady(&taskReady)
            , mStatus(MFX_WRN_IN_EXECUTION) {
        }
        virtual ~Task() {}

        mfxStatus Synhronize(int timeout) {
            return UMC::UMC_OK == mTaskReady->Wait(timeout)? mStatus : MFX_WRN_IN_EXECUTION;
        }
        
    protected:
        void Execute() {
            mStatus = InvokeFunctor();
        }
        virtual  mfxStatus InvokeFunctor()  = 0;
    };

    template <class Functor>
    class TaskTmpl : public Task {
        Functor mFunctor;
    public:
        TaskTmpl(UMC::Event & taskReady, const Functor & fnc) 
            : Task(taskReady), mFunctor(fnc) {
        }
        virtual  mfxStatus InvokeFunctor() {
            return mFunctor();
        }
    };

    class ThreadPool {
        UMC::Thread mThread;
        UMC::Event mWakeupThread;
        UMC::Event mTaskReady;
        UMC::Mutex mQueAccess;
        std::list<mfx_shared_ptr<Task> > mTaskQueue;
        bool mQuit;
        mfxU32 mThreadId;
    public:
        ThreadPool () 
            : mQuit()
            , mThreadId() {
            if (UMC::UMC_OK != mThread.Create(Runner, this)) {
                throw std::runtime_error("cannot create new thread");
            }
            //autoreset event
            mWakeupThread.Init(0, 0);
            
            //manual reset event
            mTaskReady.Init(1, 0);
        }
        ~ThreadPool() {
            mQuit = true;
            mThread.Wait();
        }
        template <class Functor>
        mfx_shared_ptr<Task> Queue(const Functor & callback) { 
            UMC::AutomaticUMCMutex guard (mQueAccess);
            mTaskReady.Reset();
            mTaskQueue.push_back(new TaskTmpl<Functor>(mTaskReady, callback));
            mfx_shared_ptr<Task> LastTask = mTaskQueue.back();
            mWakeupThread.Set();
            return LastTask;
        }

        bool IsThreadCall() {
            return __GetThreadId() == mThreadId;
        }
    private:
        mfxU32 __GetThreadId() {
#ifdef WIN32
            return GetCurrentThreadId() ;
#else
            return getid();
#endif
        }
        void LocalRunner() {
            mThreadId = __GetThreadId();
            for(;!mQuit;) {
                if (mTaskQueue.empty()) 
                {
                    mWakeupThread.Wait(10);
                    continue;
                }
                mfx_shared_ptr<Task> task;
                {
                    UMC::AutomaticUMCMutex guard (mQueAccess);
                    task = mTaskQueue.front();
                    mTaskQueue.pop_front();
                }
                task->Execute();
                mTaskReady.Set();
            }
        }
        static Ipp32u VM_THREAD_CALLCONVENTION Runner(void * threadPoolPtr) {
            ((ThreadPool*)(threadPoolPtr))->LocalRunner();
            return 0;
        }
    };
}