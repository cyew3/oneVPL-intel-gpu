// Copyright (c) 2005-2019 Intel Corporation
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

#ifndef __UMC_THREADED_DEMUXER_H__
#define __UMC_THREADED_DEMUXER_H__

#define VM_THREAD_CATCHCRASH

#include "vm_thread.h"
#include "umc_event.h"
#include "umc_mutex.h"
#include "umc_media_data.h"
#include "umc_splitter.h"
#include "umc_demuxer.h"
#include <thread>

namespace UMC
{
    class Demuxer;
    struct RulesMatchingState;
    Ipp32u VM_THREAD_CALLCONVENTION ThreadRoutineStarter(void* u);

    class ThreadedDemuxer : public Splitter
    {
        DYNAMIC_CAST_DECL(ThreadedDemuxer, Splitter)

    public:
        ThreadedDemuxer();
        virtual ~ThreadedDemuxer();
        virtual Status Init(SplitterParams& init);
        virtual Status Close(void);
        virtual Status EnableTrack(Ipp32u nTrack, Ipp32s iState);
        virtual Status Run(void);
        virtual Status Stop(void);

        // trick-modes
        virtual Status SetRate(Ipp64f rate);
        virtual Status SetTimePosition(Ipp64f timePos);
        virtual Status GetTimePosition(Ipp64f& timePos);
        virtual void AlterQuality(Ipp64f time);

        // getting data
        virtual Status GetNextData(MediaData *data, Ipp32u uiTrack);
        virtual Status CheckNextData(MediaData *data, Ipp32u uiTrack);

        // getting info
        virtual Status GetInfo(SplitterInfo** ppInfo);

    protected:
        Status AnalyzeParams(SplitterParams *pParams);
        virtual Demuxer* CreateCoreDemux() const;
        void TerminateInit(void);
        bool TryTrackByRules(Ipp32u uiTrack);
        Ipp32u ThreadRoutine();
        friend Ipp32u VM_THREAD_CALLCONVENTION ThreadRoutineStarter(void* u);

        // pointer to the core demuxing object
        // this pointer is used to check if object initialized
        Demuxer *m_pDemuxer;
        // signaled when PSI is changed
        Event *m_pOnPSIChangeEvent;
        // internal thread
        std::thread m_DemuxerThread;
        // signaled when init finished
        Event m_OnInit;
        // signaled when blocking buffer is unlocked
        Event m_OnUnlock;
        // PID of blocking buffer
        Ipp32s m_iBlockingPID;
        // saved initialization flags
        Ipp32u m_uiFlags;
        // stop flag
        bool m_bStop;
        // end of stream flag
        bool m_bEndOfStream;
        // indicates than new track is enabled
        bool m_bAutoEnable;
        // syncro for changing of flag m_bAutoEnable
        Mutex m_OnAutoEnable;
        // rules matching state, contains rules as well
        RulesMatchingState *m_pRulesState;
    };
}

#endif /* __UMC_THREADED_DEMUXER_H__ */
