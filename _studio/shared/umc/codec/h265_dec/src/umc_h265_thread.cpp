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

#include <algorithm>

#include "umc_h265_thread.h"
#include "umc_h265_segment_decoder_mt.h"

namespace UMC_HEVC_DECODER
{

H265Thread::H265Thread()
    : m_isSleepNow(0)
    , m_isDoneWait(0)
    , m_bQuit(false)
{
} // H265Thread() :

H265Thread::~H265Thread()
{
    Release();
} // ~H265Thread(void)

// Returns stored decoder instance
H265SegmentDecoderBase * H265Thread::GetSegmentDecoder()
{
    return m_segmentDecoder;
}

// Initialize decoder thread
UMC::Status H265Thread::Init(Ipp32s iNumber, H265SegmentDecoderBase * segmentDecoder)
{
    // release object before initialization
    Release();

    // save thread number(s)
    m_iNumber = iNumber;

    m_segmentDecoder = segmentDecoder;

    // threading tools
    m_bQuit = false;

    // start decoding thread
    if (UMC::UMC_OK != Create(DecodingThreadRoutine, this))
        return UMC::UMC_ERR_INIT;

    m_isSleepNow = 0;
    m_isDoneWait = 0;
    m_sleepEvent.Init(0, 0);
    m_doneEvent.Init(0, 0);

    return UMC::UMC_OK;
} // Status Init(Ipp32u nNumber)

// Sleep until task broker wakes the thread
void H265Thread::Sleep()
{
    m_mGuard.Lock();
    m_isSleepNow = 1;
    m_mGuard.Unlock();

    m_sleepEvent.Wait();
}

// Wake up the thread
void H265Thread::Awake()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (m_isSleepNow)
    {
        m_sleepEvent.Set();
        m_isSleepNow = 0;
    }
}

// Reset state
void H265Thread::Reset()
{
    // threading tools
    if (IsValid())
    {
        {
        UMC::AutomaticUMCMutex guard(m_mGuard);
        if (m_isSleepNow)
            return;

        m_isDoneWait = 1;
        }

        m_doneEvent.Wait();
        //VM_ASSERT(m_isSleepNow);
    }
} // void Reset(void)

// Release resources
void H265Thread::Release()
{
    Reset();

    if (IsValid())
    {
        m_bQuit = true;
        Awake();
        Wait();
    }

    Close();
} // void Release(void)

// Starting routine for decoding thread
Ipp32u VM_THREAD_CALLCONVENTION H265Thread::DecodingThreadRoutine(void *p)
{
    H265Thread *pObj = (H265Thread *) p;
    H265SegmentDecoderBase * segmentDecoder = pObj->GetSegmentDecoder();

    // check error(s)
    if (NULL == p)
        return 0x0baad;

    try
    {
        while (false == pObj->m_bQuit) // do segment decoding
        {
            try
            {
                pObj->m_Status = segmentDecoder->ProcessSegment();
            }
            catch(...)
            {
                // do nothing
            }

            if (pObj->m_Status == UMC::UMC_ERR_NOT_ENOUGH_DATA)
            {
                pObj->m_mGuard.Lock();
                pObj->m_isSleepNow = 1;
                if (pObj->m_isDoneWait)
                {
                    pObj->m_doneEvent.Set();
                    pObj->m_isDoneWait = 0;
                }

                pObj->m_mGuard.Unlock();
                pObj->Sleep();
            }
        }
    }
    catch(...)
    {

    }

    return 0x0264dec0 + pObj->m_iNumber;

} // Ipp32u DecodingThreadRoutine(void *p)


H265ThreadGroup::H265ThreadGroup()
    : m_rejectAwake(false)
{
}

H265ThreadGroup::~H265ThreadGroup()
{
    Release();
}

// Release memory and resources
void H265ThreadGroup::Release()
{
    m_rejectAwake = true;

    for (Ipp32u i = 0; i < m_threads.size(); i++)
    {
        m_threads[i]->Release();
    }

    for (Ipp32u i = 0; i < m_threads.size(); i++)
    {
        delete m_threads[i];
    }

    m_threads.clear();
    m_rejectAwake = false;
}

// Reset threads state
void H265ThreadGroup::Reset()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    m_rejectAwake = true;

    for (Ipp32u i = 0; i < m_threads.size(); i++)
    {
        m_threads[i]->Reset();
    }

    m_rejectAwake = false;
}

// Add a new thread to the group
void H265ThreadGroup::AddThread(H265Thread * thread)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    m_threads.push_back(thread);
}

// Wake all threads in group
void H265ThreadGroup::AwakeThreads()
{
    if (m_rejectAwake)
        return;

    UMC::AutomaticUMCMutex guard(m_mGuard);

    for (Ipp32u i = 0; i < m_threads.size(); i++)
    {
        m_threads[i]->Awake();
    }
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
