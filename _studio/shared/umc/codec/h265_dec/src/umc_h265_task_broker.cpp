/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include <cstdarg>
#include "umc_h265_task_broker.h"
#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_heap.h"
#include "umc_h265_task_supplier.h"
#include "umc_h265_frame_list.h"

#include "mfx_trace.h"

//#define ECHO
//#define ECHO_DEB
//#define VM_DEBUG
//#define _TASK_MESSAGE

//#define TIME

//#define LIGHT_SYNC

//static Ipp32s lock_failed_H265 = 0;
//static Ipp32s sleep_count = 0;

#if 0

#undef DEBUG_PRINT

#if defined ECHO || defined TIME
#define DEBUG_PRINT(x) Trace x
#else
#define DEBUG_PRINT(x)
#endif

#if 0
#define DEBUG_PRINT1(x) Trace x
#else
#define DEBUG_PRINT1(x)
#endif


#else
#undef DEBUG_PRINT
#undef DEBUG_PRINT1
#define DEBUG_PRINT(x)
#define DEBUG_PRINT1(x)
#endif

/*__declspec(naked)
int TryRealGet(void *)
{
    __asm
    {
        mov eax, 01h
        xor edx, edx
        mov ecx, dword ptr [esp + 04h]
        lock cmpxchg dword ptr [ecx], edx
        setz al
        movzx eax, al
        ret
    }
}

Ipp32s TryToGet(void *p)
{
    Ipp32s res = TryRealGet(p);
    if (!res)
        lock_failed_H265++;
    return res;
}*/

namespace UMC_HEVC_DECODER
{

void PrintInfoStatus(H265DecoderFrameInfo * info)
{
    if (!info)
        return;

    printf("needtoCheck - %d. status - %d\n", info->GetRefAU() != 0, info->GetStatus());
    for (Ipp32u i = 0; i < info->GetSliceCount(); i++)
    {
        printf("slice - %u\n", i);
        H265Slice * pSlice = info->GetSlice(i);
        printf("POC - %d, \n", info->m_pFrame->m_PicOrderCnt);
        printf("cur to dec - %d\n", pSlice->m_iCurMBToDec);
        printf("cur to rec - %d\n", pSlice->m_iCurMBToRec);
        printf("cur to deb - %d\n", pSlice->m_iCurMBToDeb);
        printf("dec, rec, deb vacant - %d, %d, %d\n", pSlice->m_bDecVacant, pSlice->m_bRecVacant, pSlice->m_bDebVacant);
        printf("in/out buffers available - %d, %d\n", pSlice->m_CoeffsBuffers.IsInputAvailable(), pSlice->m_CoeffsBuffers.IsOutputAvailable());
        fflush(stdout);
    }
}
/*
void PrintAllInfos(H265DecoderFrame * frame)
{
    printf("all info\n");
    for (; frame; frame = frame->future())
    {
        H265DecoderFrameInfo *slicesInfo = frame->GetAU();

        if (slicesInfo->GetAllSliceCount())
        {
            printf("poc - %d\n", slicesInfo->m_pFrame->m_PicOrderCnt[0]);
            PrintInfoStatus(slicesInfo);
        }
    }

    printf("all info end\n");
}*/

static
Ipp32s GetDecodingOffset(H265DecoderFrameInfo * , Ipp32s &)
{
    return 0;
}


#ifdef TIME

static
class CTimer
{
public:

    enum
    {
        NUMBER_OF_THREADS = 4
    };

    CTimer(void)
    {
        memset(m_iDecTime, 0, sizeof(m_iDecTime));
        memset(m_iRecTime, 0, sizeof(m_iRecTime));
        memset(m_iDebTime, 0, sizeof(m_iDebTime));
        memset(m_iDebTimeT, 0, sizeof(m_iDebTimeT));
        memset(m_iSleepTime, 0, sizeof(m_iSleepTime));
        memset(m_iDebSleepTime, 0, sizeof(m_iDebSleepTime));
    }

    ~CTimer(void)
    {
        Ipp32s i;

        for (i = 0; i < NUMBER_OF_THREADS; i += 1)
        {
            DEBUG_PRINT((VM_STRING("%d: dec - % 8d, rec - % 8d, deb - % 8d, deb th - % 8d (sleep - % 8d, in deb - % 8d)\n"),
                    i,
                    (Ipp32s) (m_iDecTime[i] / 1000000),
                    (Ipp32s) (m_iRecTime[i] / 1000000),
                    (Ipp32s) (m_iDebTime[i] / 1000000),
                    (Ipp32s) (m_iDebTimeT[i] / 1000000),
                    (Ipp32s) (m_iSleepTime[i] / 1000000),
                    (Ipp32s) (m_iDebSleepTime[i] / 1000000)));
        }

        Ipp64s iAll = 0;

        for (i = 0; i < NUMBER_OF_THREADS; i += 1)
        {
            iAll += m_iDecTime[i]+
                           m_iRecTime[i]+
                           m_iDebTime[i]+
                           m_iDebTimeT[i]+
                           m_iSleepTime[i];
        }

        for (i = 0; i < NUMBER_OF_THREADS; i += 1)
        {
            DEBUG_PRINT((VM_STRING("%d: dec - % 8d%%, rec - % 8d%%, deb - % 8d%%, deb th - % 8d%%\n"),
                    i,
                    (Ipp32s) (100 * m_iDecTime[i] / iAll),
                    (Ipp32s) (100 * m_iRecTime[i] / iAll),
                    (Ipp32s) (100 * m_iDebTime[i] / iAll),
                    (Ipp32s) (100 * m_iDebTimeT[i] / iAll)));
        }
    }

    void ThreadStarted(Ipp32s iThread)
    {
        QueryPerformanceCounter((LARGE_INTEGER *) m_iStartTime + iThread);
    }

    void ThreadFinished(Ipp32s iThread, Ipp32s iTask)
    {
        Ipp64s iEnd;

        QueryPerformanceCounter((LARGE_INTEGER *) &iEnd);

        switch (iTask)
        {
        case TASK_DEC_NEXT:
        case TASK_DEC_H265:
            m_iDecTime[iThread] += iEnd - m_iStartTime[iThread];
            break;

        case TASK_REC_NEXT:
        case TASK_REC_H265:
            m_iRecTime[iThread] += iEnd - m_iStartTime[iThread];
            break;

        case TASK_DEB_H265:
            m_iDebTime[iThread] += iEnd - m_iStartTime[iThread];
            break;

        case TASK_DEB_THREADED_H265:
        case TASK_DEB_FRAME_THREADED_H265:
            m_iDebTimeT[iThread] += iEnd - m_iStartTime[iThread];
            break;

        default:
            break;
        };
    }

    void DebSleepStart(Ipp32s iThread)
    {
        QueryPerformanceCounter((LARGE_INTEGER *) m_iStartTime + iThread);
    }

    void DebSleepStop(Ipp32s iThread)
    {
        Ipp64s iEnd;

        QueryPerformanceCounter((LARGE_INTEGER *) &iEnd);

        m_iDebSleepTime[iThread] += iEnd - m_iStartTime[iThread];
    }

    void SleepStart(Ipp32s iThread)
    {
        QueryPerformanceCounter((LARGE_INTEGER *) m_iStartTime + iThread);
    }

    void SleepStop(Ipp32s iThread)
    {
        Ipp64s iEnd;

        QueryPerformanceCounter((LARGE_INTEGER *) &iEnd);

        m_iSleepTime[iThread] += iEnd - m_iStartTime[iThread];
    }

protected:
    Ipp64s m_iDecTime[NUMBER_OF_THREADS];
    Ipp64s m_iRecTime[NUMBER_OF_THREADS];
    Ipp64s m_iDebTime[NUMBER_OF_THREADS];
    Ipp64s m_iDebTimeT[NUMBER_OF_THREADS];
    Ipp64s m_iDebSleepTime[NUMBER_OF_THREADS];
    Ipp64s m_iSleepTime[NUMBER_OF_THREADS];
    Ipp64s m_iStartTime[NUMBER_OF_THREADS];

} timer;

class CStarter
{
public:
    CStarter(Ipp32s iThread)
    {
        m_iThread = iThread;
    }
    ~CStarter(void)
    {
        timer.ThreadStarted(m_iThread);
    }

protected:
    Ipp32s m_iThread;
};

#endif // TIME


TaskBroker_H265::TaskBroker_H265(TaskSupplier_H265 * pTaskSupplier)
    : m_pTaskSupplier(pTaskSupplier)
    , m_iConsumerNumber(0)
    , m_FirstAU(0)
    , m_nWaitingThreads(0)
    , m_IsShouldQuit(false)
    , m_isExistMainThread(true)
{
    Release();
}

TaskBroker_H265::~TaskBroker_H265()
{
    Release();
}

bool TaskBroker_H265::Init(Ipp32s iConsumerNumber, bool isExistMainThread)
{
    Release();

    m_eWaiting.clear();
    m_Completed.Init(0, 0);

    // we keep this variable due some optimizations purposes
    m_iConsumerNumber = iConsumerNumber;
    m_IsShouldQuit = false;
    m_isExistMainThread = isExistMainThread;

    m_nWaitingThreads = 0;

    m_eWaiting.resize(m_iConsumerNumber);

    // initilaize event(s)
    for (Ipp32s i = 0; i < m_iConsumerNumber; i += 1)
    {
        if (UMC::UMC_OK != m_eWaiting[i].Init(0, 0))
            return false;
    }

    return true;
}

void TaskBroker_H265::Reset()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    m_FirstAU = 0;
    m_IsShouldQuit = true;

    for (FrameQueue::iterator iter = m_decodingQueue.begin(); iter != m_decodingQueue.end(); ++iter)
    {
        m_pTaskSupplier->UnlockFrameResource(*iter);
    }

    m_decodingQueue.clear();
    m_completedQueue.clear();

    // awake threads
    for (Ipp32s i = 0; i < m_iConsumerNumber; i += 1)
    {
        m_eWaiting[i].Set();
    }
}

void TaskBroker_H265::Release()
{
    Reset();

    AwakeThreads();
    m_nWaitingThreads = 0;
}

void TaskBroker_H265::WaitFrameCompletion()
{
    m_Completed.Wait(500);
}

void TaskBroker_H265::Lock()
{
    m_mGuard.Lock();
    /*if ((m_mGuard.TryLock() != UMC_OK))
    {
        lock_failed_H265++;
        m_mGuard.Lock();
    }*/
}

void TaskBroker_H265::Unlock()
{
    m_mGuard.Unlock();
}

void TaskBroker_H265::SleepThread(Ipp32s threadNumber)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "AVCDec_sleep");

#ifdef TIME
    timer.SleepStart(threadNumber);
#endif // TIME

    /*printf("\n\nstart sleeping for thrad - %d\n", threadNumber);
    PrintInfoStatus(m_FirstAU);
    printf("additional - \n");
    PrintInfoStatus(m_AdditionalInfo);
    printf("end\n");*/

    m_nWaitingThreads |= (1 << threadNumber);

    DEBUG_PRINT((VM_STRING("(%d) sleep\n"), threadNumber));
#ifndef LIGHT_SYNC
    Unlock();
#endif

    // sleep_count++;

    m_eWaiting[threadNumber].Wait();

#ifndef LIGHT_SYNC
    Lock();
#endif

    m_nWaitingThreads ^= (1 << threadNumber);

#ifdef TIME
    timer.SleepStop(threadNumber);
#endif // TIME
}

void TaskBroker_H265::AwakeThreads()
{
    DEBUG_PRINT((VM_STRING("awaken threads\n")));
    m_pTaskSupplier->m_threadGroup.AwakeThreads();

    if (!m_nWaitingThreads)
        return;

    Ipp32s iMask = 1;
    for (Ipp32s i = 0; i < m_iConsumerNumber; i += 1)
    {
        if (m_nWaitingThreads & iMask)
        {
            m_eWaiting[i].Set();
        }

        iMask <<= 1;
    }
}

bool TaskBroker_H265::PrepareFrame(H265DecoderFrame * pFrame)
{
    if (!pFrame || pFrame->m_iResourceNumber < 0)
    {
        return true;
    }

    if (pFrame->prepared)
        return true;

    H265DecoderFrame * resourceHolder = m_pTaskSupplier->IsBusyByFrame(pFrame->m_iResourceNumber);
    if (resourceHolder && resourceHolder != pFrame)
        return false;

    if (!m_pTaskSupplier->LockFrameResource(pFrame))
        return false;

    if (!pFrame->prepared &&
        (pFrame->GetAU()->GetStatus() == H265DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU()->GetStatus() == H265DecoderFrameInfo::STATUS_STARTED))
    {
        pFrame->prepared = true;
    }

    DEBUG_PRINT((VM_STRING("Prepare frame - %d, %d\n"), pFrame->m_PicOrderCnt, pFrame->m_iResourceNumber));

    return true;
}

bool TaskBroker_H265::GetPreparationTask(H265DecoderFrameInfo * info)
{
    H265DecoderFrame * pFrame = info->m_pFrame;

    if (info->m_prepared)
        return false;

    if (info != pFrame->GetAU() && pFrame->GetAU()->m_prepared != 2)
        return false;

    info->m_prepared = 1;

    Unlock();

    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; i++)
    {
        H265Slice * pSlice = info->GetSlice(i);
        // reset CABAC engine
        pSlice->GetBitStream()->InitializeDecodingEngine_CABAC();
        pSlice->InitializeContexts();
    }

    Lock();
    info->m_prepared = 2;
    return false;
}

bool TaskBroker_H265::AddFrameToDecoding(H265DecoderFrame * frame)
{
    if (!frame || frame->IsDecodingStarted() || !IsExistTasks(frame))
        return false;

    VM_ASSERT(frame->IsFrameExist());

    UMC::AutomaticUMCMutex guard(m_mGuard);

#ifdef VM_DEBUG
    FrameQueue::iterator iter = m_decodingQueue.begin();
    FrameQueue::iterator end_iter = m_decodingQueue.end();

    for (; iter != end_iter; ++iter)
    {
        if ((*iter) == frame)
        {
            VM_ASSERT(false);
        }
    }
#endif

    m_decodingQueue.push_back(frame);
    frame->StartDecoding();
    return true;
}

void TaskBroker_H265::AwakeCompleteWaiter()
{
    m_Completed.Set();
}


void TaskBroker_H265::RemoveAU(H265DecoderFrameInfo * toRemove)
{
    H265DecoderFrameInfo * temp = m_FirstAU;

    if (!temp)
        return;

    H265DecoderFrameInfo * reference = 0;

    for (; temp; temp = temp->GetNextAU())
    {
        if (temp == toRemove)
            break;

        if (temp->IsReference())
            reference = temp;
    }

    if (!temp)
        return;

    if (temp->GetPrevAU())
        temp->GetPrevAU()->SetNextAU(temp->GetNextAU());

    if (temp->GetNextAU())
        temp->GetNextAU()->SetPrevAU(temp->GetPrevAU());

    H265DecoderFrameInfo * temp1 = temp->GetNextAU();

    temp->SetPrevAU(0);
    temp->SetNextAU(0);

    if (temp == m_FirstAU)
        m_FirstAU = temp1;

    if (temp1)
    {
        temp = temp1;

        for (; temp; temp = temp->GetNextAU())
        {
            if (temp->GetRefAU())
                temp->SetRefAU(reference);

            if (temp->IsReference())
                break;
        }
    }
}

void TaskBroker_H265::CompleteFrame(H265DecoderFrame * frame)
{
    if (!frame || m_decodingQueue.empty() || !frame->IsFullFrame())
        return;

    if (!IsFrameCompleted(frame) || frame->IsDecodingCompleted())
        return;

    m_pTaskSupplier->UnlockFrameResource(frame);
    if (frame == m_decodingQueue.front())
    {
        RemoveAU(frame->GetAU());
        m_decodingQueue.pop_front();
    }
    else
    {
        RemoveAU(frame->GetAU());
        m_decodingQueue.remove(frame);
    }

    frame->CompleteDecoding();

    if (m_isExistMainThread && m_iConsumerNumber > 1)
        m_completedQueue.push_back(frame);

    AwakeCompleteWaiter();
}

void TaskBroker_H265::SwitchCurrentAU()
{
    if (!m_FirstAU || !m_FirstAU->IsCompleted())
        return;

    DEBUG_PRINT((VM_STRING("Switch current AU - is_completed %d, poc - %d, m_FirstAU - %X\n"), m_FirstAU->IsCompleted(), m_FirstAU->m_pFrame->m_PicOrderCnt, m_FirstAU));

    while (m_FirstAU)
    {
        if (!IsFrameCompleted(m_FirstAU->m_pFrame))
        {
            if (m_FirstAU->IsCompleted())
            {
                m_FirstAU = m_FirstAU->GetNextAU();
                VM_ASSERT(!m_FirstAU || !IsFrameCompleted(m_FirstAU->m_pFrame));
                continue;
            }

            m_FirstAU->SetPrevAU(0);

            H265DecoderFrameInfo * temp = m_FirstAU;
            for (; temp; temp = temp->GetNextAU())
            {
                temp->SetRefAU(0);

                if (temp->IsReference())
                    break;
            }
            break;
        }

        H265DecoderFrameInfo* completed = m_FirstAU;
        m_FirstAU = m_FirstAU->GetNextAU();
        if (m_FirstAU && m_FirstAU->m_pFrame == completed->m_pFrame)
            m_FirstAU = m_FirstAU->GetNextAU();
        CompleteFrame(completed->m_pFrame);
    }

    InitAUs();

    if (m_FirstAU)
        AwakeThreads();
}

void TaskBroker_H265::Start()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    FrameQueue::iterator iter = m_decodingQueue.begin();

    for (; iter != m_decodingQueue.end(); ++iter)
    {
        H265DecoderFrame * frame = *iter;

        if (IsFrameCompleted(frame))
        {
            CompleteFrame(frame);
            iter = m_decodingQueue.begin();
            if (iter == m_decodingQueue.end()) // avoid ++iter operation
                break;
        }
    }

    InitAUs();
    AwakeThreads();
    DEBUG_PRINT((VM_STRING("Start\n")));
}

H265DecoderFrameInfo * TaskBroker_H265::FindAU()
{
    FrameQueue::iterator iter = m_decodingQueue.begin();
    FrameQueue::iterator end_iter = m_decodingQueue.end();

    for (; iter != end_iter; ++iter)
    {
        H265DecoderFrame * frame = *iter;

        H265DecoderFrameInfo *slicesInfo = frame->GetAU();

        if (slicesInfo->GetSliceCount())
        {
            if (slicesInfo->GetStatus() == H265DecoderFrameInfo::STATUS_FILLED)
                return slicesInfo;
        }
    }

    return 0;
}

void TaskBroker_H265::InitAUs()
{
    H265DecoderFrameInfo * pPrev;
    H265DecoderFrameInfo * refAU;

    if (!m_FirstAU)
    {
        m_FirstAU = FindAU();
        if (!m_FirstAU)
            return;

        if (!PrepareFrame(m_FirstAU->m_pFrame))
        {
            m_FirstAU = 0;
            return;
        }

        m_FirstAU->SetStatus(H265DecoderFrameInfo::STATUS_STARTED);

        pPrev = m_FirstAU;
        m_FirstAU->SetPrevAU(0);
        m_FirstAU->SetNextAU(0);
        m_FirstAU->SetRefAU(0);

        refAU = 0;
        if (m_FirstAU->IsReference())
        {
            refAU = m_FirstAU;
        }
    }
    else
    {
        pPrev = m_FirstAU;
        refAU = 0;

        pPrev->SetRefAU(0);
        pPrev->SetPrevAU(0);

        if (m_FirstAU->IsReference())
        {
            refAU = pPrev;
        }

        while (pPrev->GetNextAU())
        {
            pPrev = pPrev->GetNextAU();

            if (!refAU)
                pPrev->SetRefAU(0);

            if (pPrev->IsReference())
            {
                refAU = pPrev;
            }
        };
    }

    H265DecoderFrameInfo * pTemp = FindAU();
    for (; pTemp; )
    {
        if (!PrepareFrame(pTemp->m_pFrame))
        {
            pPrev->SetNextAU(0);
            break;
        }

        pTemp->SetStatus(H265DecoderFrameInfo::STATUS_STARTED);
        pTemp->SetNextAU(0);
        pTemp->SetPrevAU(pPrev);

        /*if (pTemp->IsIntraAU())
            pTemp->SetRefAU(0);
        else*/
        pTemp->SetRefAU(refAU);

        if (pTemp->IsReference())
        {
            refAU = pTemp;
        }

        pPrev->SetNextAU(pTemp);
        pPrev = pTemp;
        pTemp = FindAU();
    }
}

bool TaskBroker_H265::IsFrameCompleted(H265DecoderFrame * pFrame) const
{
    if (!pFrame)
        return true;

    if (!pFrame->GetAU()->IsCompleted())
        return false;

    //pFrame->GetAU(0)->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED); //quuu
    H265DecoderFrameInfo::FillnessStatus status1 = pFrame->GetAU()->GetStatus();

    bool ret;
    switch (status1)
    {
    case H265DecoderFrameInfo::STATUS_NONE:
        ret = true;
        break;
    case H265DecoderFrameInfo::STATUS_NOT_FILLED:
        ret = false;
        break;
    case H265DecoderFrameInfo::STATUS_COMPLETED:
        ret = true;
        break;
    default:
        ret = pFrame->GetAU()->IsCompleted();
        break;
    }

    if (ret)
    {
        m_pTaskSupplier->UnlockFrameResource(pFrame);
    }

    return ret;
}

Ipp32s TaskBroker_H265::GetNumberOfSlicesToReconstruct(H265DecoderFrameInfo * info, bool bOnlyReadySlices)
{
    Ipp32s i, iCount = 0;

    Ipp32s sliceCount = info->GetSliceCount();

    for (i = 0; i < sliceCount; i ++)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if (pSlice->m_iMaxMB > pSlice->m_iCurMBToRec)
        {
            // we count up all decoding slices
            if ((false == bOnlyReadySlices) ||
            // or only ready to reconstruct
                ((pSlice->m_bRecVacant) && (pSlice->m_CoeffsBuffers.IsOutputAvailable())))
                iCount += 1;
        }
    }

    return iCount;

} // Ipp32s TaskBroker_H265::GetNumberOfSlicesToReconstruct(void)

bool TaskBroker_H265::IsFrameDeblocked(H265DecoderFrameInfo * info)
{
    // this is guarded function, safe to touch any variable

    Ipp32s i;

    // there is nothing to do
    Ipp32s sliceCount = info->GetSliceCount();
    if (0 == sliceCount)
        return true;

    for (i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if ((pSlice) &&
            (false == pSlice->m_bDeblocked))
            return false;
    }

    return true;

} // bool TaskBroker_H265::IsFrameDeblocked(void)

Ipp32s TaskBroker_H265::GetNumberOfSlicesToDecode(H265DecoderFrameInfo * info)
{
    Ipp32s i, iCount = 0;

    Ipp32s sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if (pSlice->m_iMaxMB > pSlice->m_iCurMBToDec)
            iCount += 1;
    }

    return iCount;

} // Ipp32s TaskBroker_H265::GetNumberOfSlicesToDecode(void)

bool TaskBroker_H265::GetNextTask(H265Task *pTask)
{
#ifndef LIGHT_SYNC
    UMC::AutomaticUMCMutex guard(m_mGuard);
#endif

    pTask->m_taskPreparingGuard = &guard;

    // check error(s)
    if (/*!m_FirstAU ||*/ m_IsShouldQuit)
    {
        AwakeCompleteWaiter();
        return false;
    }

    bool res = GetNextTaskInternal(pTask);
    return res;
} // bool TaskBroker_H265::GetNextTask(H265Task *pTask)

bool TaskBroker_H265::GetNextSlice(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    // check error(s)
    if (!info)
        return false;

    if (info->GetRefAU() != 0)
        return false;

    if (GetPreparationTask(info))
        return true;

    if (info->m_prepared != 2)
        return false;

    if (GetNextSliceToDecoding(info, pTask))
        return true;

    if (info->IsNeedDeblocking())
    {
        if (GetNextSliceToDeblocking(info, pTask))
            return true;
    }

    return GetSAOTask(info, pTask);
} // bool TaskBroker_H265::GetNextSlice(H265Task *pTask)

bool TaskBroker_H265::GetSAOTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    H265Slice *pSlice = info->GetSlice(0);
    H265SliceHeader *sliceHeder = pSlice->GetSliceHeader();

    if (pSlice->m_bSAOed)
        return false;

    if (pSlice->GetSeqParam()->sample_adaptive_offset_enabled_flag && (sliceHeder->m_SaoEnabledFlag || sliceHeder->m_SaoEnabledFlagChroma))
    {
        InitTask(info, pTask, pSlice);
        pTask->m_iFirstMB = 0;
        pTask->m_iMBToProcess = pSlice->m_iAvailableMB;
        pTask->m_iTaskID = TASK_SAO_H265;
        pTask->m_pBuffer = NULL;
        pTask->pFunction = &H265SegmentDecoderMultiThreaded::SAOFrameTask;

        return true;
    }

    Ipp32s sliceCount = m_FirstAU->GetSliceCount();

    for (Ipp32s i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pTemp = m_FirstAU->GetSlice(i);

        pTemp->m_bSAOed = true;
    }

    return false;
}

void TaskBroker_H265::InitTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)
{
    pTask->m_bDone = false;
    pTask->m_bError = false;
    pTask->m_iMaxMB = pSlice->m_iMaxMB;
    pTask->m_pSlice = pSlice;
    pTask->m_pSlicesInfo = info;
}

bool TaskBroker_H265::GetNextSliceToDecoding(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable

    Ipp32s i;
    bool bDoDeblocking;

    // skip some slices, more suitable for first thread
    // and first slice is always reserved for first slice decoder
    /*if (pTask->m_iThreadNumber)
    {
        i = IPP_MAX(1, GetNumberOfSlicesFromCurrentFrame() / m_iConsumerNumber);
        bDoDeblocking = false;
    }
    else
    {
        i = 0;
        bDoDeblocking = true;
    }*/

    i = 0;
    bDoDeblocking = false;

    // find first uncompressed slice
    Ipp32s sliceCount = info->GetSliceCount();
    for (; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if ((false == pSlice->m_bInProcess) &&
            (false == pSlice->m_bDecoded))
        {
            InitTask(info, pTask, pSlice);
            pTask->m_iFirstMB = pSlice->m_iFirstMB;
            pTask->m_iMBToProcess = IPP_MIN(pSlice->m_iMaxMB - pSlice->m_iFirstMB, pSlice->m_iAvailableMB);
            pTask->m_iTaskID = TASK_PROCESS_H265;
            pTask->m_pBuffer = NULL;
            pTask->pFunction = &H265SegmentDecoderMultiThreaded::ProcessSlice;
            // we can do deblocking only on independent slices or
            // when all previous slices are deblocked
            //if (pSlice->getLoopFilterDisable())
            //    bDoDeblocking = true;
            pSlice->m_bPrevDeblocked = false;
            pSlice->m_bInProcess = true;
            pSlice->m_bDecVacant = 0;
            pSlice->m_bRecVacant = 0;
            pSlice->m_bDebVacant = 0;

#ifdef ECHO
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) slice dec - % 4d to % 4d\n"), pTask->m_iThreadNumber, pSlice->m_pCurrentFrame->m_PicOrderCnt[0],
                pSlice->m_pCurrentFrame->m_viewId, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

            return true;
        }
    }

    return false;

} // bool TaskBroker_H265::GetNextSliceToDecoding(H265Task *pTask)

bool TaskBroker_H265::GetNextSliceToDeblocking(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable

    Ipp32s sliceCount = info->GetSliceCount();

    Ipp32s i;
    bool bPrevDeblocked = true;

    for (i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        // we can do deblocking only on vacant slices
        if ((false == pSlice->m_bInProcess) &&
            (true == pSlice->m_bDecoded) &&
            (false == pSlice->m_bDeblocked))
        {
            // we can do this only when previous slice was deblocked
            if (true == bPrevDeblocked)
            {
                InitTask(info, pTask, pSlice);
                pTask->m_iFirstMB = pSlice->m_iFirstMB;
                pTask->m_iMBToProcess = pSlice->m_iMaxMB - pSlice->m_iFirstMB;
                pTask->m_iTaskID = TASK_DEB_SLICE_H265;
                pTask->m_pBuffer = NULL;
                pTask->pFunction = &H265SegmentDecoderMultiThreaded::DeblockSegmentTask;

                pSlice->m_bPrevDeblocked = true;
                pSlice->m_bInProcess = true;
                pSlice->m_bDebVacant = 0;

#ifdef ECHO_DEB
                DEBUG_PRINT((VM_STRING("(%d) slice deb - % 4d to % 4d\n"), pTask->m_iThreadNumber,
                    pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO_DEB

                return true;
            }
        }

        // save previous slices deblocking condition
        if (false == pSlice->m_bDeblocked)
            bPrevDeblocked = false;
    }

    return false;

} // bool TaskBroker_H265::GetNextSliceToDeblocking(H265Task *pTask)

void TaskBroker_H265::AddPerformedTask(H265Task *pTask)
{
#ifndef LIGHT_SYNC
    UMC::AutomaticUMCMutex guard(m_mGuard);
#endif

    H265Slice *pSlice = pTask->m_pSlice;
    H265DecoderFrameInfo * info = pTask->m_pSlicesInfo;

#if defined(ECHO)
        switch (pTask->m_iTaskID)
        {
        case TASK_PROCESS_H265:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) slice dec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_viewId, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEC_H265:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) dec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_viewId, (Ipp32s)(pSlice->IsBottomField()),  pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_REC_H265:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) rec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_viewId, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEC_REC_H265:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) dec_rec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_viewId, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEB_H265:
#if defined(ECHO_DEB)
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) deb done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_viewId, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
#else
        case TASK_DEB_FRAME_H265:
        case TASK_DEB_FRAME_THREADED_H265:
        case TASK_DEB_SLICE_H265:
        case TASK_DEB_THREADED_H265:
#endif // defined(ECHO_DEB)
            break;
        default:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) default task done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_viewId, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        }
#endif // defined(ECHO)

#ifdef TIME
    timer.ThreadFinished(pTask->m_iThreadNumber, pTask->m_iTaskID);
#endif // TIME

    // when whole slice was processed
    if (TASK_PROCESS_H265 == pTask->m_iTaskID)
    {
        // slice is deblocked only when deblocking was available
        // check condition for frame deblocking
        //if (DEBLOCK_FILTER_ON_NO_SLICE_EDGES_H265 == pSlice->m_SliceHeader.disable_deblocking_filter_idc)
        //m_bDoFrameDeblocking = false; // DEBUG : ADB

        if (false == pSlice->m_bDeblocked)
            pSlice->m_bDeblocked = pSlice->m_bPrevDeblocked;

        if (!info->IsNeedDeblocking())
        {
            pSlice->m_bDeblocked = true;
        }
        // slice is decoded
        pSlice->m_bDecoded = true;
        pSlice->m_bDecVacant = 0;
        pSlice->m_bRecVacant = 0;
        pSlice->m_bDebVacant = 1;
        pSlice->m_bInProcess = false;
    }
    else if (TASK_DEB_SLICE_H265 == pTask->m_iTaskID)
    {
        pSlice->m_bDebVacant = 1;
        pSlice->m_bDeblocked = 1;
        pSlice->m_bInProcess = false;
    }
    else if (TASK_DEB_FRAME_H265 == pTask->m_iTaskID)
    {
        Ipp32s sliceCount = m_FirstAU->GetSliceCount();

        // frame is deblocked
        for (Ipp32s i = 0; i < sliceCount; i += 1)
        {
            H265Slice *pTemp = m_FirstAU->GetSlice(i);

            pTemp->m_bDebVacant = 1;
            pTemp->m_bDeblocked = true;
            pTemp->m_bInProcess = false;
        }
    }
    else if (TASK_SAO_H265 == pTask->m_iTaskID)
    {
        Ipp32s sliceCount = m_FirstAU->GetSliceCount();

        for (Ipp32s i = 0; i < sliceCount; i += 1)
        {
            H265Slice *pTemp = m_FirstAU->GetSlice(i);

            pTemp->m_bSAOed = true;
        }
    }
    else
    {
        switch (pTask->m_iTaskID)
        {
        case TASK_DEC_H265:
            {
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToDec);

            pSlice->m_iCurMBToDec += pTask->m_iMBToProcess;
            // move filled buffer to reconstruct queue
            pSlice->m_CoeffsBuffers.UnLockInputBuffer(pTask->m_WrittenSize);
            pSlice->m_MVsDistortion = pTask->m_mvsDistortion;

            bool isReadyIncrease = (pTask->m_iFirstMB == info->m_iDecMBReady);
            if (isReadyIncrease)
            {
                info->m_iDecMBReady += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                if (isReadyIncrease)
                    info->m_iDecMBReady = pSlice->m_iMaxMB;
                pSlice->m_iMaxMB = IPP_MIN(pSlice->m_iCurMBToDec, pSlice->m_iMaxMB);
                pSlice->m_bError = true;
            }
            else
            {
                pSlice->m_iMaxMB = pTask->m_iMaxMB;
            }

            if (pSlice->m_iCurMBToDec >= pSlice->m_iMaxMB)
            {
                pSlice->m_bDecVacant = 0;
                if (isReadyIncrease)
                {
                    Ipp32s pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    if (pos >= 0)
                    {
                        H265Slice * pNextSlice = info->GetSlice(++pos);
                        for (; pNextSlice; pNextSlice = info->GetSlice(++pos))
                        {
                           info->m_iDecMBReady = pNextSlice->m_iCurMBToDec;
                           if (pNextSlice->m_iCurMBToDec < pNextSlice->m_iMaxMB)
                               break;
                        }
                    }
                }
            }
            else
            {
                pSlice->m_bDecVacant = 1;
            }

            DEBUG_PRINT((VM_STRING("(%d) (%d) dec ready %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, info->m_iDecMBReady));
            }
            break;

        case TASK_REC_H265:
            {
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToRec);

            pSlice->m_iCurMBToRec += pTask->m_iMBToProcess;

            bool isReadyIncrease = (pTask->m_iFirstMB == info->m_iRecMBReady) && pSlice->m_bDeblocked;
            if (isReadyIncrease)
            {
                info->m_iRecMBReady += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                if (isReadyIncrease)
                {
                    info->m_iRecMBReady = pSlice->m_iMaxMB;
                }

                pSlice->m_iMaxMB = IPP_MIN(pSlice->m_iCurMBToRec, pSlice->m_iMaxMB);
                pSlice->m_bError = true;
            }

            pSlice->m_CoeffsBuffers.UnLockOutputBuffer();

            if (pSlice->m_iMaxMB <= pSlice->m_iCurMBToRec)
            {
                if (isReadyIncrease)
                {
                    Ipp32s pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    if (pos >= 0)
                    {
                        H265Slice * pNextSlice = info->GetSlice(pos + 1);
                        if (pNextSlice)
                        {
                            if (pNextSlice->m_bDeblocked)
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToRec;
                            else
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToDeb;

                            info->RemoveSlice(pos);
                        }
                    }
                }

                pSlice->m_bRecVacant = 0;
                pSlice->m_bDecoded = true;

                // check condition for frame deblocking
                //if (DEBLOCK_FILTER_ON_NO_SLICE_EDGES_H265 == pSlice->m_SliceHeader.disable_deblocking_filter_idc)
                //    m_bDoFrameDeblocking = false;  // DEBUG : ADB
            }
            else
            {
                pSlice->m_bRecVacant = 1;
            }
            DEBUG_PRINT((VM_STRING("(%d) (%d) rec ready %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, info->m_iRecMBReady));
            }
            break;

        case TASK_DEC_REC_H265:
            {
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToDec);
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToRec);

            pSlice->m_iCurMBToDec += pTask->m_iMBToProcess;
            pSlice->m_iCurMBToRec += pTask->m_iMBToProcess;

            bool isReadyIncreaseDec = (pTask->m_iFirstMB == info->m_iDecMBReady);
            bool isReadyIncreaseRec = (pTask->m_iFirstMB == info->m_iRecMBReady);

            pSlice->m_iMaxMB = pTask->m_iMaxMB;

            if (isReadyIncreaseDec)
                info->m_iDecMBReady += pTask->m_iMBToProcess;

            if (isReadyIncreaseRec && pSlice->m_bDeblocked)
            {
                info->m_iRecMBReady += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                if (isReadyIncreaseDec)
                    info->m_iDecMBReady = pSlice->m_iMaxMB;

                if (isReadyIncreaseRec && pSlice->m_bDeblocked)
                    info->m_iRecMBReady = pSlice->m_iMaxMB;

                pSlice->m_iMaxMB = IPP_MIN(pSlice->m_iCurMBToDec, pSlice->m_iMaxMB);
                pSlice->m_iCurMBToRec = pSlice->m_iCurMBToDec;
                pSlice->m_bError = true;
            }

            if (pSlice->m_iCurMBToDec >= pSlice->m_iMaxMB)
            {
                VM_ASSERT(pSlice->m_iCurMBToRec >= pSlice->m_iMaxMB);

                if (isReadyIncreaseDec || isReadyIncreaseRec)
                {
                    Ipp32s pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    VM_ASSERT(pos >= 0);
                    H265Slice * pNextSlice = info->GetSlice(pos + 1);
                    if (pNextSlice)
                    {
                        if (isReadyIncreaseDec)
                        {
                            Ipp32s pos1 = pos;
                            H265Slice * pTmpSlice = info->GetSlice(++pos1);

                            for (; pTmpSlice; pTmpSlice = info->GetSlice(++pos1))
                            {
                               info->m_iDecMBReady = pTmpSlice->m_iCurMBToDec;
                               if (pTmpSlice->m_iCurMBToDec < pTmpSlice->m_iMaxMB)
                                   break;
                            }
                        }

                        if (isReadyIncreaseRec && pSlice->m_bDeblocked)
                        {
                            if (pNextSlice->m_bDeblocked)
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToRec;
                            else
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToDeb;

                            info->RemoveSlice(pos);
                        }
                    }
                }

                pSlice->m_bDecVacant = 0;
                pSlice->m_bRecVacant = 0;
                pSlice->m_bDecoded = true;
            }
            else
            {
                pSlice->m_bDecVacant = 1;
                pSlice->m_bRecVacant = 1;
            }

            DEBUG_PRINT((VM_STRING("(%d) (%d) (%d) dec_rec ready %d %d\n"), pTask->m_iThreadNumber, (Ipp32s)(info != m_FirstAU), info->m_pFrame->m_PicOrderCnt, info->m_iDecMBReady, info->m_iRecMBReady));
            }
            break;

        case TASK_DEB_H265:
            {
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToDeb);

            pSlice->m_iCurMBToDeb += pTask->m_iMBToProcess;

            bool isReadyIncrease = (pTask->m_iFirstMB == info->m_iRecMBReady);
            if (isReadyIncrease)
            {
                info->m_iRecMBReady += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                pSlice->m_iCurMBToDeb = pSlice->m_iMaxMB;
                info->m_iRecMBReady = pSlice->m_iCurMBToRec;
                pSlice->m_bError = true;
            }

            if (pSlice->m_iMaxMB <= pSlice->m_iCurMBToDeb)
            {
                Ipp32s pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                if (isReadyIncrease)
                {
                    VM_ASSERT(pos >= 0);
                    H265Slice * pNextSlice = info->GetSlice(pos + 1);
                    if (pNextSlice)
                    {
                        if (pNextSlice->m_bDeblocked)
                            info->m_iRecMBReady = pNextSlice->m_iCurMBToRec;
                        else
                            info->m_iRecMBReady = pNextSlice->m_iCurMBToDeb;
                    }
                }

                info->RemoveSlice(pos);

                pSlice->m_bDeblocked = true;
                pSlice->m_bInProcess = false;
                pSlice->m_bDecVacant = 0;
                pSlice->m_bRecVacant = 0;
            }
            else
            {
                pSlice->m_bDebVacant = 1;
            }

            DEBUG_PRINT((VM_STRING("(%d) (%d) after deb ready %d %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, info->m_iDecMBReady, info->m_iRecMBReady));
            }
            break;
#if 0
        case TASK_DEB_THREADED_H265:
            pSlice->m_DebTools.SetProcessedMB(pTask);
            if (pSlice->m_DebTools.IsDeblockingDone())
                pSlice->m_bDeblocked = true;

            break;

        case TASK_DEB_FRAME_THREADED_H265:
            m_DebTools.SetProcessedMB(pTask);
            if (m_DebTools.IsDeblockingDone())
            {
                Ipp32s i;

                // frame is deblocked
                for (i = 0; i < GetNumberOfSlicesFromCurrentFrame(); i += 1)
                {
                    H265Slice *pTemp = GetSliceFromCurrentFrame(i);

                    pTemp->m_bDeblocked = true;
                }
            }

            break;
#endif

        default:
            // illegal case
            VM_ASSERT(false);
            break;
        }
    }

} // void TaskBroker_H265::AddPerformedTask(H265Task *pTask)

#define Check_Status(status)  ((status == H265DecoderFrameInfo::STATUS_FILLED) || (status == H265DecoderFrameInfo::STATUS_STARTED))

Ipp32s TaskBroker_H265::GetNumberOfTasks(void)
{
    Ipp32s au_count = 0;

    H265DecoderFrameInfo * temp = m_FirstAU;

    for (; temp ; temp = temp->GetNextAU())
    {
        if (temp->GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
            continue;

        au_count += 2;
    }

    return au_count;
}

bool TaskBroker_H265::IsEnoughForStartDecoding(bool force)

{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    InitAUs();
    Ipp32s au_count = GetNumberOfTasks();

    Ipp32s additional_tasks = m_iConsumerNumber;
    if (m_iConsumerNumber > 6)
        additional_tasks -= 1;
    //au_count++; //moe
    if ((au_count >> 1) >= additional_tasks || (force && au_count))
        return true;

    return false;
}

bool TaskBroker_H265::IsExistTasks(H265DecoderFrame * frame)
{
    H265DecoderFrameInfo *slicesInfo = frame->GetAU();

    return Check_Status(slicesInfo->GetStatus());
}

TaskBrokerSingleThread_H265::TaskBrokerSingleThread_H265(TaskSupplier_H265 * pTaskSupplier)
    : TaskBroker_H265(pTaskSupplier)
{
}

// Get next working task
bool TaskBrokerSingleThread_H265::GetNextTaskInternal(H265Task *pTask)
{
    while (false == m_IsShouldQuit)
    {
        if (!m_FirstAU)
        {
            AwakeCompleteWaiter();
            if (m_isExistMainThread)
                break;
        }
        else
        {
            if (GetNextSlice(m_FirstAU, pTask))
                return true;

            if (m_FirstAU->IsCompleted())
            {
                bool frameCompleted = IsFrameCompleted(m_FirstAU->m_pFrame);

                SwitchCurrentAU();

                if (frameCompleted)
                {
                    if (m_isExistMainThread)
                    {
                        return false;
                    }
                }
            }

            if (GetNextSlice(m_FirstAU, pTask))
                return true;
        }

        break;
    }

    return false;
}

TaskBrokerTwoThread_H265::TaskBrokerTwoThread_H265(TaskSupplier_H265 * pTaskSupplier)
    : TaskBrokerSingleThread_H265(pTaskSupplier)
{
}

bool TaskBrokerTwoThread_H265::Init(Ipp32s iConsumerNumber, bool isExistMainThread)
{
    if (!TaskBroker_H265::Init(iConsumerNumber, isExistMainThread))
        return false;

    return true;
}

void TaskBrokerTwoThread_H265::Reset()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread_H265::Reset();
}

void TaskBrokerTwoThread_H265::Release()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread_H265::Release();
}

void TaskBrokerTwoThread_H265::CompleteFrame()
{
    if (!m_FirstAU)
        return;

    DEBUG_PRINT((VM_STRING("frame completed - poc - %d\n"), m_FirstAU->m_pFrame->m_PicOrderCnt));
    SwitchCurrentAU();
}

bool TaskBrokerTwoThread_H265::GetNextTaskInternal(H265Task *pTask)
{
#ifdef TIME
    CStarter start(pTask->m_iThreadNumber);
#endif // TIME

    while (false == m_IsShouldQuit)
    {
        if (m_isExistMainThread)
        {
            if (!pTask->m_iThreadNumber && !m_completedQueue.empty())
            {
                CompleteFrame(); // here SwitchAU and awake
                m_completedQueue.clear();
                return false;
            }
        }

        if (!m_FirstAU)
        {
            if (m_isExistMainThread)
            {
                if (!pTask->m_iThreadNumber)
                {
                    SwitchCurrentAU();
                    m_completedQueue.clear();
                    return false;
                }

                AwakeThreads();
            }
            else
            {
                AwakeCompleteWaiter();
            }
        }
        else
        {
            for (H265DecoderFrameInfo *pTemp = m_FirstAU; pTemp; pTemp = pTemp->GetNextAU())
            {
                if (GetNextTaskManySlices(pTemp, pTask))
                {
                    return true;
                }
            }
        }

        //SleepThread(pTask->m_iThreadNumber);
        break;
    };

    if (!pTask->m_iThreadNumber && m_isExistMainThread) // check for slice groups only. need to review
    {
        m_completedQueue.clear();
    }

    return false;
}

bool TaskBrokerTwoThread_H265::WrapDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)
{
    VM_ASSERT(pSlice);
#ifdef LIGHT_SYNC
    Ipp32s locked = TryToGet(&pSlice->m_bDecVacant);
    if (!locked)
        return false;
#endif

    // this is guarded function, safe to touch any variable
    if ((1 == pSlice->m_bDecVacant) &&
        (pSlice->m_CoeffsBuffers.IsInputAvailable()))
    {
        pSlice->m_bInProcess = true;
        pSlice->m_bDecVacant = 0;

        pTask->m_taskPreparingGuard->Unlock();
        Ipp32s iMBWidth = pSlice->GetMBRowWidth();

        // error handling
        /*if (0 >= ((signed) pSlice->m_BitStream.BytesLeft()))
        {
            pSlice->m_iMaxMB = pSlice->m_iCurMBToDec;
            return false;
        }*/

        InitTask(info, pTask, pSlice);
        pTask->m_iFirstMB = pSlice->m_iCurMBToDec;
        pTask->m_WrittenSize = 0;
        pTask->m_iMBToProcess = IPP_MIN(pSlice->m_iCurMBToDec -
                                    (pSlice->m_iCurMBToDec % iMBWidth) +
                                    iMBWidth,
                                    pSlice->m_iMaxMB) - pSlice->m_iCurMBToDec;

        pTask->m_iMBToProcess = IPP_MIN(pTask->m_iMBToProcess, pSlice->m_iAvailableMB);
        pTask->m_iTaskID = TASK_DEC_H265;
        pTask->m_pBuffer = (H265CoeffsPtrCommon)pSlice->m_CoeffsBuffers.LockInputBuffer();
        pTask->pFunction = &H265SegmentDecoderMultiThreaded::DecodeSegment;

#ifdef ECHO
        DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) dec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
            info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_viewId, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

        return true;
    }
#ifdef LIGHT_SYNC
    else
    {
        pSlice->m_bDecVacant = 1;
    }
#endif

    return false;

} // bool TaskBrokerTwoThread_H265::WrapDecodingTask(H265Task *pTask, H265Slice *pSlice)

bool TaskBrokerTwoThread_H265::WrapReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)
{
    VM_ASSERT(pSlice);
    // this is guarded function, safe to touch any variable

#ifdef LIGHT_SYNC
    Ipp32s locked = TryToGet(&pSlice->m_bRecVacant);
    if (!locked)
        return false;
#endif

    if ((1 == pSlice->m_bRecVacant) &&
        (pSlice->m_CoeffsBuffers.IsOutputAvailable()))
    {
        pSlice->m_bRecVacant = 0;

        pTask->m_taskPreparingGuard->Unlock();
        Ipp32s iMBWidth = pSlice->GetMBRowWidth();

        InitTask(info, pTask, pSlice);
        pTask->m_iFirstMB = pSlice->m_iCurMBToRec;
        pTask->m_iMBToProcess = IPP_MIN(pSlice->m_iCurMBToRec -
                                    (pSlice->m_iCurMBToRec % iMBWidth) +
                                    iMBWidth,
                                    pSlice->m_iMaxMB) - pSlice->m_iCurMBToRec;

        pTask->m_iTaskID = TASK_REC_H265;
        Ipp8u* pointer;
        size_t size;
        pSlice->m_CoeffsBuffers.LockOutputBuffer(pointer, size);
        pTask->m_pBuffer = ((H265CoeffsPtrCommon) pointer);
        pTask->pFunction = &H265SegmentDecoderMultiThreaded::ReconstructSegment;

#ifdef ECHO
        DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d)  (%d) rec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
            info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_viewId, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

        return true;
    }
#ifdef LIGHT_SYNC
    else
    {
        pSlice->m_bRecVacant = 1;
    }
#endif

    return false;

} // bool TaskBrokerTwoThread_H265::WrapReconstructTask(H265Task *pTaskm H265Slice *pSlice)

bool TaskBrokerTwoThread_H265::WrapDecRecTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)
{
    VM_ASSERT(pSlice);
    // this is guarded function, safe to touch any variable

#ifdef LIGHT_SYNC
    Ipp32s locked = TryToGet(&pSlice->m_bRecVacant);
    if (!locked)
        return false;
#endif

    if ((1 == pSlice->m_bRecVacant) && (1 == pSlice->m_bDecVacant) &&
        (pSlice->m_iCurMBToDec == pSlice->m_iCurMBToRec) &&
        (pSlice->m_CoeffsBuffers.IsInputAvailable()))
    {
        pSlice->m_bRecVacant = 0;
        pSlice->m_bDecVacant = 0;

        pTask->m_taskPreparingGuard->Unlock();
        Ipp32s iMBWidth = pSlice->GetMBRowWidth();

        InitTask(info, pTask, pSlice);
        pTask->m_iFirstMB = pSlice->m_iCurMBToDec;
        pTask->m_WrittenSize = 0;
        pTask->m_iMBToProcess = IPP_MIN(pSlice->m_iCurMBToDec -
                                    (pSlice->m_iCurMBToDec % iMBWidth) +
                                    iMBWidth,
                                    pSlice->m_iMaxMB) - pSlice->m_iCurMBToDec;

        pTask->m_iTaskID = TASK_DEC_REC_H265;
        pTask->m_pBuffer = 0;
        pTask->pFunction = &H265SegmentDecoderMultiThreaded::DecRecSegment;

#ifdef ECHO
        DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) dec_rec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
            info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_viewId, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

        return true;
    }
#ifdef LIGHT_SYNC
    else
    {
        pSlice->m_bDecVacant = 1;
        pSlice->m_bRecVacant = 1;
    }
#endif

    return false;

} // bool TaskBrokerTwoThread_H265::WrapDecRecTask(H265Task *pTaskm H265Slice *pSlice)

bool TaskBrokerTwoThread_H265::GetDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    Ipp32s i;

    H265DecoderFrameInfo * refAU = info->GetRefAU();
    bool is_need_check = refAU != 0;
    Ipp32s readyCount = 0;
    Ipp32s additionalLines = 0;

    if (is_need_check)
    {
        if (refAU->GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
        {
            is_need_check = false;
        }
        else
        {
            readyCount = refAU->m_iDecMBReady;
            additionalLines = GetDecodingOffset(info, readyCount);
        }
    }

    Ipp32s sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if (pSlice->m_bDecVacant != 1)
            continue;

        if (is_need_check)
        {
            if (pSlice->m_iCurMBToDec + (1 + additionalLines)*pSlice->GetMBRowWidth() > readyCount)
                break;
        }

        if (WrapDecodingTask(info, pTask, pSlice))
            return true;
    }

    return false;

} // bool TaskBrokerTwoThread_H265::GetDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask)

bool TaskBrokerTwoThread_H265::GetReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable

    H265DecoderFrameInfo * refAU = info->GetRefAU();
    bool is_need_check = refAU != 0;
    Ipp32s readyCount = 0;
    Ipp32s additionalLines = 0;

    if (is_need_check)
    {
        if (refAU->GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
        {
            is_need_check = false;
        }
        else
        {
            readyCount = refAU->m_iRecMBReady;
            additionalLines = GetDecodingOffset(info, readyCount);
        }
    }

    Ipp32s i;
    Ipp32s sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if (pSlice->m_bRecVacant != 1 ||
            !pSlice->m_CoeffsBuffers.IsOutputAvailable())
        {
            continue;
        }

        if (is_need_check)
        {
            Ipp32s k = (( (pSlice->m_MVsDistortion + 15) / 16) + 1); // +2 - (1 - for padding, 2 - current line)
            k += refAU->IsNeedDeblocking() ? 1 : 0;
            if (pSlice->m_iCurMBToRec + (k + additionalLines)*pSlice->GetMBRowWidth() >= readyCount)
                break;
        }

        if (WrapReconstructTask(info, pTask, pSlice))
        {
            return true;
        }
    }

    return false;

} // bool TaskBrokerTwoThread_H265::GetReconstructTask(H265Task *pTask)

bool TaskBrokerTwoThread_H265::GetDecRecTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    if (info->GetRefAU() || info->GetSliceCount() == 1)
        return false;

    Ipp32s i;
    Ipp32s sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if (pSlice->m_bRecVacant != 1 || pSlice->m_bDecVacant != 1)
        {
            continue;
        }

        if (WrapDecRecTask(info, pTask, pSlice))
        {
            return true;
        }
    }

    return false;
}

bool TaskBrokerTwoThread_H265::GetDeblockingTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    Ipp32s i;
    bool bPrevDeblocked = true;

    Ipp32s sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        Ipp32s iMBWidth = pSlice->GetMBRowWidth(); // DEBUG : always deblock two lines !!!
        Ipp32s iAvailableToDeblock;
        Ipp32s iDebUnit = 1;

        iAvailableToDeblock = pSlice->m_iCurMBToRec -
                              pSlice->m_iCurMBToDeb;

        if ((false == pSlice->m_bDeblocked) &&
            ((true == bPrevDeblocked) || (false == pSlice->getLFCrossSliceBoundaryFlag())) &&
            (1 == pSlice->m_bDebVacant) &&
            ((iAvailableToDeblock > iMBWidth) || (pSlice->m_iCurMBToRec >= pSlice->m_iMaxMB)))
        {
            pSlice->m_bDebVacant = 0;

            pTask->m_taskPreparingGuard->Unlock();
            InitTask(info, pTask, pSlice);
            pTask->m_iFirstMB = pSlice->m_iCurMBToDeb;

            {
                pTask->m_iMBToProcess = IPP_MIN(iMBWidth - (pSlice->m_iCurMBToDeb % iMBWidth),
                                            iAvailableToDeblock);
                pTask->m_iMBToProcess = IPP_MAX(pTask->m_iMBToProcess,
                                            iDebUnit);
                pTask->m_iMBToProcess = UMC::align_value<Ipp32s> (pTask->m_iMBToProcess, iDebUnit);
            }

            pTask->m_iTaskID = TASK_DEB_H265;
            pTask->pFunction = &H265SegmentDecoderMultiThreaded::DeblockSegmentTask;

#ifdef ECHO_DEB
            DEBUG_PRINT((VM_STRING("(%d) deb - % 4d to % 4d\n"), pTask->m_iThreadNumber,
                pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO_DEB

            return true;
        }
        else
        {
            if ((0 >= iAvailableToDeblock) && (pSlice->m_iCurMBToRec >= pSlice->m_iMaxMB))
            {
                pSlice->m_bDeblocked = true;
                bPrevDeblocked = true;
            }
        }

        // save previous slices deblocking condition
        if (false == pSlice->m_bDeblocked || pSlice->m_iCurMBToRec < pSlice->m_iMaxMB)
        {
            bPrevDeblocked = false;
            break; // for mbaff deblocking could be performaed outside boundaries.
        }
    }

    return false;

} // bool TaskBrokerTwoThread_H265::GetDeblockingTask(H265Task *pTask)

bool TaskBrokerTwoThread_H265::GetNextTaskManySlices(H265DecoderFrameInfo * info, H265Task *pTask)
{
    if (!info)
        return false;

    if (GetPreparationTask(info))
        return true;

    if (info->m_prepared != 2)
        return false;

    if (info->IsNeedDeblocking())
    {
        if (true == GetDeblockingTask(info, pTask))
            return true;
    }

    // try to get reconstruct task from main queue
    if (GetReconstructTask(info, pTask))
    {
        return true;
    }

    if (GetDecRecTask(info, pTask))
    {
        return true;
    }

    // try to get decoding task from main frame
    if (GetDecodingTask(info, pTask))
    {
        return true;
    }

    return false;
}

void TaskBrokerTwoThread_H265::AddPerformedTask(H265Task *pTask)
{
#ifndef LIGHT_SYNC
    UMC::AutomaticUMCMutex guard(m_mGuard);
#endif

    TaskBrokerSingleThread_H265::AddPerformedTask(pTask);

    if (pTask->m_pSlicesInfo->IsCompleted())
    {
        pTask->m_pSlicesInfo->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
    }

    SwitchCurrentAU();
    AwakeThreads();
} // void TaskBrokerTwoThread_H265::AddPerformedTask(H265Task *pTask)

bool TaskBrokerTwoThread_H265::GetFrameDeblockingTaskThreaded(H265DecoderFrameInfo * , H265Task *)
{
#if 1
    return false;
#else
    // this is guarded function, safe to touch any variable
    Ipp32s sliceCount = info->GetSliceCount();
    H265Slice * firstSlice = info->GetSlice(0);

    if (m_bFirstDebThreadedCall)
    {
        Ipp32s i;
        Ipp32s iFirstMB = -1, iMaxMB = -1;
        Ipp32s iMBWidth = firstSlice->GetMBRowWidth();
        Ipp32s iDebUnit = 1;
        bool bError = false;

        // check all other threads are sleep
        for (i = 0; i < sliceCount; i++)
        {
            H265Slice *pSlice = info->GetSlice(i);

            if ((pSlice) && (0 == pSlice->m_bDebVacant))
                return false;
            if (pSlice->m_bError)
                bError = true;
        }

        // handle little slices
        if ((iDebUnit * 2 > iMBWidth / m_iConsumerNumber) || bError)
            return GetDeblockingTask(pTask);

        // calculate deblocking range
        for (i = 0; i < sliceCount; i++)
        {
            H265Slice *pSlice = info->GetSlice(i);

            if ((pSlice) &&
                (false == pSlice->m_bDeblocked))
            {
                // find deblocking range
                if (-1 == iFirstMB)
                    iFirstMB = pSlice->m_iCurMBToDeb;
                if (iMaxMB < pSlice->m_iMaxMB)
                    iMaxMB = pSlice->m_iMaxMB;
            }
        }

        m_DebTools.Reset(iFirstMB,
                         iMaxMB,
                         iDebUnit,
                         iMBWidth);

        m_bFirstDebThreadedCall = false;
    }

    // get next piece to deblock
    if (false == m_DebTools.GetMBToProcess(pTask))
        return false;

    // correct task to slice range
    {
        Ipp32s i;

        for (i = 0; i < sliceCount; i += 1)
        {
            H265Slice *pSlice = info->GetSlice(i);

            if ((pTask->m_iFirstMB >= pSlice->m_iFirstMB) &&
                (pTask->m_iFirstMB < pSlice->m_iMaxMB))
            {
                pTask->m_iTaskID = TASK_DEB_FRAME_THREADED_H265;
                pTask->m_pSlice = pSlice;
                if (pTask->m_iFirstMB + pTask->m_iMBToProcess > pSlice->m_iMaxMB)
                    pTask->m_iMBToProcess = pSlice->m_iMaxMB - pTask->m_iFirstMB;
                break;
            }
        }
    }

    return true;
#endif
} // bool TaskBrokerTwoThread_H265::GetFrameDeblockingTaskThreaded(H265Task *pTask)

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
