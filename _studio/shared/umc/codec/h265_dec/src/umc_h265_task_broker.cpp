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

#include "umc_h265_dec_debug.h"

//#define ECHO
//#define ECHO_DEB
//#define VM_DEBUG
//#define _TASK_MESSAGE

//#define TIME

//static Ipp32s lock_failed_H265 = 0;
//static Ipp32s sleep_count = 0;

#define NUM_CU_TO_PROCESS 30

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

    printf("status - %d\n", info->GetStatus());
    for (Ipp32u i = 0; i < info->GetSliceCount(); i++)
    {
        printf("slice - %u\n", i);
        H265Slice * pSlice = info->GetSlice(i);
        printf("POC - %d, \n", info->m_pFrame->m_PicOrderCnt);
        printf("cur to dec - %d\n", pSlice->m_iCurMBToDec);
        printf("cur to rec - %d\n", pSlice->m_iCurMBToRec);
        printf("cur to deb - %d\n", pSlice->m_iCurMBToDeb);
        printf("dec, rec, deb vacant - %d, %d, %d\n", pSlice->m_bDecVacant, pSlice->m_bRecVacant, pSlice->m_bDebVacant);
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
    Unlock();

    // sleep_count++;

    m_eWaiting[threadNumber].Wait();

    Lock();

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
        H265SliceHeader * sliceHeader = pSlice->GetSliceHeader();

        Ipp32s numPartitions = pFrame->m_CodingData->m_NumPartitions;
        sliceHeader->SliceCurStartCUAddr = pFrame->m_CodingData->GetInverseCUOrderMap(sliceHeader->SliceCurStartCUAddr / numPartitions) * numPartitions;
        sliceHeader->SliceCurEndCUAddr = pFrame->m_CodingData->GetInverseCUOrderMap(sliceHeader->SliceCurEndCUAddr / numPartitions) * numPartitions;
        sliceHeader->m_sliceSegmentCurStartCUAddr = pFrame->m_CodingData->GetInverseCUOrderMap(sliceHeader->m_sliceSegmentCurStartCUAddr / numPartitions) * numPartitions;
        sliceHeader->m_sliceSegmentCurEndCUAddr = pFrame->m_CodingData->GetInverseCUOrderMap(sliceHeader->m_sliceSegmentCurEndCUAddr / numPartitions) * numPartitions;

        /*size_t currOffset = pSlice->GetSliceHeader()->m_HeaderBitstreamOffset;
        for (Ipp32u tile = 0; tile < pSlice->getTileLocationCount(); tile++)
        {
            pSlice->setTileLocation(tile, pSlice->getTileLocation(tile) + currOffset);
        }*/

        // reset CABAC engine
        //pSlice->GetBitStream()->InitializeDecodingEngine_CABAC();
        //pSlice->InitializeContexts();
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

    for (; temp; temp = temp->GetNextAU())
    {
        if (temp == toRemove)
            break;
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

    while (m_FirstAU && m_FirstAU->IsCompleted())
    {
        DEBUG_PRINT((VM_STRING("Switch current AU - is_completed %d, poc - %d, m_FirstAU - %X\n"), m_FirstAU->IsCompleted(), m_FirstAU->m_pFrame->m_PicOrderCnt, m_FirstAU));

        H265DecoderFrameInfo* completed = m_FirstAU;
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
    }
    else
    {
        pPrev = m_FirstAU;
        pPrev->SetPrevAU(0);

        while (pPrev->GetNextAU())
        {
            pPrev = pPrev->GetNextAU();
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

bool TaskBroker_H265::GetNextTask(H265Task *pTask)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

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
        Ipp32s sliceCount = info->GetSliceCount();
        for (Ipp32s i = 0; i < sliceCount; i += 1)
        {
            H265Slice *pTemp = info->GetSlice(i);
            if (pTemp->m_bInProcess || !pTemp->m_bDeblocked || !pTemp->m_bDecoded)
                return false;
        }

        InitTask(info, pTask, pSlice);
        pTask->m_iFirstMB = 0;
        pTask->m_iMBToProcess = pSlice->m_iAvailableMB;
        pTask->m_iTaskID = TASK_SAO_H265;
        pTask->m_pBuffer = NULL;
        pTask->pFunction = &H265SegmentDecoderMultiThreaded::SAOFrameTask;

        for (Ipp32s i = 0; i < sliceCount; i += 1)
        {
            H265Slice *pTemp = info->GetSlice(i);
            pTemp->m_bInProcess = true;
        }

        return true;
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
    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if (!pSlice->m_bInProcess && !pSlice->m_bDecoded)
        {
            InitTask(info, pTask, pSlice);
            pTask->m_iFirstMB = pSlice->m_iFirstMB;
            pTask->m_iMBToProcess = IPP_MIN(pSlice->m_iMaxMB - pSlice->m_iFirstMB, pSlice->m_iAvailableMB);
            pTask->m_iTaskID = TASK_PROCESS_H265;
            pTask->m_pBuffer = NULL;
            pTask->pFunction = &H265SegmentDecoderMultiThreaded::ProcessSlice;
            pSlice->m_bInProcess = true;
            pSlice->m_bDecVacant = 0;
            pSlice->m_bRecVacant = 0;
            pSlice->m_bDebVacant = 0;

#ifdef ECHO
            DEBUG_PRINT((VM_STRING("(%d) (%d) slice dec - % 4d to % 4d\n"), pTask->m_iThreadNumber, pSlice->m_pCurrentFrame->m_PicOrderCnt, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
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
    bool bPrevDeblocked = true;

    for (Ipp32s i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        // we can do deblocking only on vacant slices
        if (!pSlice->m_bInProcess && pSlice->m_bDecoded && !pSlice->m_bDeblocked)
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
        if (!pSlice->m_bDeblocked)
            bPrevDeblocked = false;
    }

    return false;

} // bool TaskBroker_H265::GetNextSliceToDeblocking(H265Task *pTask)

void TaskBroker_H265::AddPerformedTask(H265Task *pTask)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    H265Slice *pSlice = pTask->m_pSlice;
    H265DecoderFrameInfo * info = pTask->m_pSlicesInfo;

#if defined(ECHO)
        switch (pTask->m_iTaskID)
        {
        case TASK_PROCESS_H265:
            DEBUG_PRINT((VM_STRING("(%d) (%d) slice dec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEC_H265:
            DEBUG_PRINT((VM_STRING("(%d) (%d)  dec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_REC_H265:
            DEBUG_PRINT((VM_STRING("(%d) (%d) rec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEC_REC_H265:
            DEBUG_PRINT((VM_STRING("(%d) (%d) dec_rec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEB_H265:
#if defined(ECHO_DEB)
            DEBUG_PRINT((VM_STRING("(%d) (%d) deb done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
#else
        case TASK_DEB_SLICE_H265:
#endif // defined(ECHO_DEB)
            break;
        default:
            DEBUG_PRINT((VM_STRING("(%d) (%d) default task done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
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
    else if (TASK_SAO_H265 == pTask->m_iTaskID)
    {
        Ipp32s sliceCount = info->GetSliceCount();

        for (Ipp32s i = 0; i < sliceCount; i += 1)
        {
            H265Slice *pTemp = info->GetSlice(i);

            pTemp->m_bSAOed = true;
            pTemp->m_bInProcess = false;
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

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
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
            }
            else
            {
                pSlice->m_bDecVacant = 1;
            }

            DEBUG_PRINT((VM_STRING("(%d) (%d) dec ready %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, pSlice->m_iCurMBToDec));
            }
            break;

        case TASK_REC_H265:
            {
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToRec);

            pSlice->m_iCurMBToRec += pTask->m_iMBToProcess;

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                pSlice->m_iMaxMB = IPP_MIN(pSlice->m_iCurMBToRec, pSlice->m_iMaxMB);
                pSlice->m_bError = true;
            }

            if (pSlice->m_iMaxMB <= pSlice->m_iCurMBToRec)
            {
                pSlice->m_bRecVacant = 0;
                pSlice->m_bDecoded = true;
            }
            else
            {
                pSlice->m_bRecVacant = 1;
            }
            DEBUG_PRINT((VM_STRING("(%d) (%d) rec ready %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, pSlice->m_iCurMBToRec));
            }
            break;
        case TASK_DEC_REC_H265:
            {
                pSlice->m_curTileRec++;

                // error handling
                if (pTask->m_bError || pSlice->m_bError)
                {
                    pSlice->m_curTileRec = pSlice->getTileLocationCount();
                    pSlice->m_bError = true;
                }

                if (pSlice->m_curTileRec >= pSlice->getTileLocationCount())
                {
                    pSlice->m_bDecVacant = 0;
                    pSlice->m_bRecVacant = 0;
                    pSlice->m_bDecoded = true;
                }

                DEBUG_PRINT((VM_STRING("(%d) (%d) (%d) dec_rec ready tiles - %d\n"), pTask->m_iThreadNumber, (Ipp32s)(info != m_FirstAU), info->m_pFrame->m_PicOrderCnt, pSlice->m_curTileRec));
            }
            break;

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
#if 0
        if (pTask->m_iThreadNumber)
            return false;

        return TaskBrokerSingleThread_H265::GetNextTaskInternal(pTask);
#else
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
            if (GetNextTaskManySlices(m_FirstAU, pTask))
                return true;
        }

        break;
#endif
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

    // this is guarded function, safe to touch any variable
    if (!pSlice->m_bDecVacant)
        return false;

    pSlice->m_bInProcess = true;
    pSlice->m_bDecVacant = 0;

    Ipp32s maxMB = IPP_MIN(pSlice->m_iCurMBToDec + NUM_CU_TO_PROCESS, pSlice->m_iMaxMB);
    InitTask(info, pTask, pSlice);
    pTask->m_iFirstMB = pSlice->m_iCurMBToDec;
    pTask->m_iMBToProcess = maxMB - pTask->m_iFirstMB;
    pTask->m_iTaskID = TASK_DEC_H265;
    pTask->pFunction = &H265SegmentDecoderMultiThreaded::DecodeSegment;

    pTask->m_taskPreparingGuard->Unlock();

#ifdef ECHO
    DEBUG_PRINT((VM_STRING("(%d) (%d) dec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
        info->m_pFrame->m_PicOrderCnt, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

    return true;

} // bool TaskBrokerTwoThread::WrapDecodingTask(H264Task *pTask, H264Slice *pSlice)

bool TaskBrokerTwoThread_H265::WrapReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)
{
    VM_ASSERT(pSlice);
    // this is guarded function, safe to touch any variable
    if (!pSlice->m_bRecVacant)
        return false;

    pSlice->m_bRecVacant = 0;

    Ipp32s maxMB = NUM_CU_TO_PROCESS;
    InitTask(info, pTask, pSlice);
    pTask->m_iFirstMB = pSlice->m_iCurMBToRec;
    pTask->m_iMBToProcess = IPP_MIN(maxMB, pSlice->m_iCurMBToDec - pTask->m_iFirstMB);

    pTask->m_taskPreparingGuard->Unlock();

    pTask->m_iTaskID = TASK_REC_H265;
    pTask->pFunction = &H265SegmentDecoderMultiThreaded::ReconstructSegment;

#ifdef ECHO
    DEBUG_PRINT((VM_STRING("(%d) (%d) rec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
        info->m_pFrame->m_PicOrderCnt, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

    return true;

} // bool TaskBrokerTwoThread::WrapReconstructTask(H264Task *pTaskm H264Slice *pSlice)

bool TaskBrokerTwoThread_H265::WrapDecRecTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)
{
    VM_ASSERT(pSlice);
    // this is guarded function, safe to touch any variable
    pSlice->m_curTileDec++;
    pTask->m_iFirstMB = pSlice->m_curTileDec;

    pTask->m_taskPreparingGuard->Unlock();

    InitTask(info, pTask, pSlice);
    pTask->m_iTaskID = TASK_DEC_REC_H265;
    pTask->pFunction = &H265SegmentDecoderMultiThreaded::DecRecSegment;

#ifdef ECHO
    DEBUG_PRINT((VM_STRING("(%d) (%d) dec_rec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
        info->m_pFrame->m_PicOrderCnt,  pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

    return true;

} // bool TaskBrokerTwoThread_H265::WrapDecRecTask(H265Task *pTaskm H265Slice *pSlice)

bool TaskBrokerTwoThread_H265::GetDecRecTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; ++i)
    {
        H265Slice *pSlice = info->GetSlice(i);

        Ipp32s tilesCount = pSlice->getTileLocationCount();

        if (pSlice->m_bRecVacant != 1 || pSlice->m_curTileDec >= tilesCount)
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

bool TaskBrokerTwoThread_H265::GetDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if (pSlice->m_bDecVacant != 1)
            continue;

        if (WrapDecodingTask(info, pTask, pSlice))
            return true;
    }

    return false;

} // bool TaskBrokerTwoThread::GetDecodingTask(H264DecoderFrameInfo * info, H264Task *pTask)

bool TaskBrokerTwoThread_H265::GetReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable

    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if (!pSlice->m_bRecVacant || pSlice->m_iCurMBToRec >= pSlice->m_iCurMBToDec)
            continue;

        if (WrapReconstructTask(info, pTask, pSlice))
        {
            return true;
        }
    }

    return false;

} // bool TaskBrokerTwoThread::GetReconstructTask(H264Task *pTask)

bool TaskBrokerTwoThread_H265::GetDeblockingTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    Ipp32s i;
    bool bPrevDeblocked = true;

    Ipp32s sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);
        Ipp32s iAvailableToDeblock = pSlice->m_bDecoded ? pSlice->m_iMaxMB : pSlice->m_iCurMBToRec - pSlice->m_iCurMBToDeb;

        if (!pSlice->m_bDeblocked &&
            (bPrevDeblocked || !pSlice->getLFCrossSliceBoundaryFlag()) &&
            pSlice->m_bDebVacant &&
            (iAvailableToDeblock > 0 || pSlice->m_iCurMBToRec >= pSlice->m_iMaxMB))
        {
            pSlice->m_bDebVacant = 0;

            pTask->m_taskPreparingGuard->Unlock();
            InitTask(info, pTask, pSlice);
            pTask->m_iFirstMB = pSlice->m_iCurMBToDeb;
            pTask->m_iMBToProcess = iAvailableToDeblock;

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
            if (0 >= iAvailableToDeblock && pSlice->m_iCurMBToRec >= pSlice->m_iMaxMB)
            {
                pSlice->m_bDeblocked = true;
                bPrevDeblocked = true;
            }
        }

        // save previous slices deblocking condition
        if (!pSlice->m_bDeblocked || pSlice->m_iCurMBToRec < pSlice->m_iMaxMB)
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

    if (info->HasDependentSliceSegments())
    {
        if (pTask->m_iThreadNumber)
            return false;

        return TaskBrokerSingleThread_H265::GetNextTaskInternal(pTask);
    }

    if (info->IsNeedDeblocking())
    {
        if (GetNextSliceToDeblocking(info, pTask))
        //if (GetDeblockingTask(info, pTask))
            return true;
    }

    if (GetSAOTask(info, pTask))
        return true;

#if 0
    if (GetReconstructTask(info, pTask))
        return true;

    if (GetDecodingTask(info, pTask))
        return true;
#else
    if (GetDecRecTask(info, pTask))
        return true;
#endif

    return false;
}

void TaskBrokerTwoThread_H265::AddPerformedTask(H265Task *pTask)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread_H265::AddPerformedTask(pTask);

    if (pTask->m_pSlicesInfo->IsCompleted())
    {
        pTask->m_pSlicesInfo->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
    }

    SwitchCurrentAU();
    AwakeThreads();
} // void TaskBrokerTwoThread_H265::AddPerformedTask(H265Task *pTask)


} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
