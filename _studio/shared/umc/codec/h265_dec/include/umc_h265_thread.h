/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_THREAD_H
#define __UMC_H265_THREAD_H

#include <vector>

#include "umc_event.h"
#include "umc_thread.h"
#include "umc_mutex.h"

namespace UMC_HEVC_DECODER
{

class H265SegmentDecoderMultiThreaded;

class H265Thread : public UMC::Thread
{
public:
    H265Thread();
    virtual ~H265Thread();

    void Reset();

    UMC::Status Init(Ipp32s iNumber, H265SegmentDecoderMultiThreaded * segmentDecoder);

    H265SegmentDecoderMultiThreaded * GetSegmentDecoder();

    void Sleep();

    void Awake();

    void Release(void);

protected:

    Ipp32s m_iNumber;                                           // ordinal number of decoder
    UMC::Mutex m_mGuard;

    //
    // Threading routines
    //

    // Starting routine for decoding thread
    static Ipp32u VM_THREAD_CALLCONVENTION DecodingThreadRoutine(void *p);

    UMC::Event m_sleepEvent;
    UMC::Event m_doneEvent;

    volatile Ipp32s m_isSleepNow;
    volatile Ipp32s m_isDoneWait;
    volatile bool m_bQuit;                                     // quit flag for additional thread(s)
    UMC::Status m_Status;                                           // async return value

    H265SegmentDecoderMultiThreaded * m_segmentDecoder;

private:
    // we lock assignment operator to avoid any
    // accasionaly assignments
    H265Thread & operator = (H265Thread &)
    {
        return *this;
    } // H265Thread & operator = (H265Thread &)

};

class H265ThreadGroup
{
public:

    H265ThreadGroup();

    virtual ~H265ThreadGroup();

    void AddThread(H265Thread * thread);

    void RemoveThread(H265Thread * thread);

    void AwakeThreads();

    //void WaitThreads();

    void Reset();

    void Release();

    Ipp32u GetThreadNum() const;

private:
    typedef std::vector<H265Thread *> ThreadsList;
    ThreadsList m_threads;
    UMC::Mutex m_mGuard;
    bool  m_rejectAwake;
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_THREAD_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
