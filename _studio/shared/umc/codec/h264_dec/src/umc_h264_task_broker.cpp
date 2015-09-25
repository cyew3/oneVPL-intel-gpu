/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include <cstdarg>
#include "umc_h264_task_broker.h"
#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_heap.h"
#include "umc_h264_task_supplier.h"
#include "umc_h264_frame_list.h"

#include "umc_h264_dec_debug.h"
#include "mfx_trace.h"

//#define ECHO
//#define ECHO_DEB
//#define VM_DEBUG
//#define _TASK_MESSAGE

//#define TIME

//static Ipp32s lock_failed = 0;
//static Ipp32s sleep_count = 0;

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
        lock_failed++;
    return res;
}*/

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

namespace UMC
{

void PrintInfoStatus(H264DecoderFrameInfo * info)
{
    if (!info)
        return;

    //printf("%x, field - %d, POC - (%d, %d), viewId - (%d), refAU - %x\n", info, info == info->m_pFrame->GetAU(0) ? 0 : 1, info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_PicOrderCnt[1], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, info->GetRefAU());

    /*printf("needtoCheck - %d. status - %d\n", info->GetRefAU() != 0, info->GetStatus());
    printf("POC - (%d, %d), \n", info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_PicOrderCnt[1]);
    printf("ViewId - (%d), \n", info->m_pFrame->m_viewId);

    for (Ipp32u i = 0; i < info->GetSliceCount(); i++)
    {
        printf("slice - %d\n", i);
        H264Slice * pSlice = info->GetSlice(i);
        printf("cur to dec - %d\n", pSlice->m_iCurMBToDec);
        printf("cur to rec - %d\n", pSlice->m_iCurMBToRec);
        printf("cur to deb - %d\n", pSlice->m_iCurMBToDeb);
        printf("dec, rec, deb vacant - %d, %d, %d\n", pSlice->m_bDecVacant, pSlice->m_bRecVacant, pSlice->m_bDebVacant);
        fflush(stdout);
    }*/
}
/*
void PrintAllInfos(H264DecoderFrame * frame)
{
    printf("all info\n");
    for (; frame; frame = frame->future())
    {
        H264DecoderFrameInfo *slicesInfo = frame->GetAU(0);

        if (slicesInfo->GetSliceCount())
        {
            printf("poc - %d\n", slicesInfo->m_pFrame->m_PicOrderCnt[0]);
            if (slicesInfo->IsField())
            {
                PrintInfoStatus(slicesInfo);
                PrintInfoStatus(frame->GetAU(1));
            }
            else
            {
                PrintInfoStatus(slicesInfo);
            }
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
        case TASK_DEC:
            m_iDecTime[iThread] += iEnd - m_iStartTime[iThread];
            break;

        case TASK_REC_NEXT:
        case TASK_REC:
            m_iRecTime[iThread] += iEnd - m_iStartTime[iThread];
            break;

        case TASK_DEB:
            m_iDebTime[iThread] += iEnd - m_iStartTime[iThread];
            break;

        case TASK_DEB_THREADED:
        case TASK_DEB_FRAME_THREADED:
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


TaskBroker::TaskBroker(TaskSupplier * pTaskSupplier)
    : m_pTaskSupplier(pTaskSupplier)
    , m_iConsumerNumber(0)
    , m_FirstAU(0)
    , m_IsShouldQuit(false)
{
    Release();
}

TaskBroker::~TaskBroker()
{
    Release();
}

bool TaskBroker::Init(Ipp32s iConsumerNumber)
{
    Release();

    // we keep this variable due some optimizations purposes
    m_iConsumerNumber = iConsumerNumber;
    m_IsShouldQuit = false;

    m_localResourses.Init(m_iConsumerNumber, m_pTaskSupplier->m_pMemoryAllocator);
    return true;
}

void TaskBroker::Reset()
{
    AutomaticUMCMutex guard(m_mGuard);
    m_FirstAU = 0;
    m_IsShouldQuit = true;

    for (FrameQueue::iterator iter = m_decodingQueue.begin(); iter != m_decodingQueue.end(); ++iter)
    {
        m_localResourses.UnlockFrameResource(*iter);
    }

    m_decodingQueue.clear();
    m_completedQueue.clear();

    m_localResourses.Reset();
}

void TaskBroker::Release()
{
    Reset();

    m_localResourses.Close();
}

LocalResources * TaskBroker::GetLocalResource()
{
    return &m_localResourses;
}

void TaskBroker::Lock()
{
    m_mGuard.Lock();
    /*if ((m_mGuard.TryLock() != UMC_OK))
    {
        lock_failed++;
        m_mGuard.Lock();
    }*/
}

void TaskBroker::Unlock()
{
    m_mGuard.Unlock();
}

bool TaskBroker::PrepareFrame(H264DecoderFrame * pFrame)
{
    if (!pFrame)
    {
        return true;
    }

    if (pFrame->prepared[0] && pFrame->prepared[1])
        return true;

    if (pFrame->m_iResourceNumber == -1)
        pFrame->m_iResourceNumber = m_localResourses.GetCurrentResourceIndex();

    H264DecoderFrame * resourceHolder = m_localResourses.IsBusyByFrame(pFrame->m_iResourceNumber);
    if (resourceHolder && resourceHolder != pFrame)
        return false;

    if (!m_localResourses.LockFrameResource(pFrame))
        return false;

    if (!pFrame->prepared[0] &&
        (pFrame->GetAU(0)->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU(0)->GetStatus() == H264DecoderFrameInfo::STATUS_STARTED))
    {
        pFrame->prepared[0] = true;
    }

    if (!pFrame->prepared[1] &&
        (pFrame->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_STARTED))
    {
        pFrame->prepared[1] = true;
    }

    DEBUG_PRINT((VM_STRING("Prepare frame - %d, %d, m_viewId - %d\n"), pFrame->m_PicOrderCnt[0], pFrame->m_iResourceNumber, pFrame->m_viewId));

    return true;
}

bool TaskBroker::GetPreparationTask(H264DecoderFrameInfo * info)
{
    H264DecoderFrame * pFrame = info->m_pFrame;

    if (info->m_prepared)
        return false;

    if (info != pFrame->GetAU(0) && pFrame->GetAU(0)->m_prepared != 2)
        return false;

    info->m_prepared = 1;

    if (info == pFrame->GetAU(0))
    {
        VM_ASSERT(pFrame->m_iResourceNumber != -1);
        Ipp32s iMBCount = pFrame->totalMBs << (pFrame->m_PictureStructureForDec < FRM_STRUCTURE ? 1 : 0);

        m_localResourses.AllocateBuffers(iMBCount);
    }

    Unlock();

    try
    {
        if (info == pFrame->GetAU(0))
        {
            VM_ASSERT(pFrame->m_iResourceNumber != -1);
            Ipp32s iMBCount = pFrame->totalMBs << (pFrame->m_PictureStructureForDec < FRM_STRUCTURE ? 1 : 0);

            // allocate decoding data
            m_localResourses.AllocateMBInfo(pFrame->m_iResourceNumber, iMBCount);

            // allocate macroblock intra types
            m_localResourses.AllocateMBIntraTypes(pFrame->m_iResourceNumber, iMBCount);

            H264DecoderFrameInfo * slicesInfo = pFrame->GetAU(0);
            Ipp32s sliceCount = slicesInfo->GetSliceCount();
            for (Ipp32s i = 0; i < sliceCount; i++)
            {
                H264Slice * pSlice = slicesInfo->GetSlice(i);
                pSlice->InitializeContexts();
                pSlice->m_mbinfo = &m_localResourses.GetMBInfo(pFrame->m_iResourceNumber);
                pSlice->m_pMBIntraTypes = m_localResourses.GetIntraTypes(pFrame->m_iResourceNumber);
            }

            // reset frame global data
            if (slicesInfo->IsSliceGroups())
            {
                memset(pFrame->m_mbinfo.mbs, 0, iMBCount*sizeof(H264DecoderMacroblockGlobalInfo));
                memset(pFrame->m_mbinfo.MV[0], 0, iMBCount*sizeof(H264DecoderMacroblockMVs));
                memset(pFrame->m_mbinfo.MV[1], 0, iMBCount*sizeof(H264DecoderMacroblockMVs));
            }
            else
            {
                if (slicesInfo->m_isBExist && slicesInfo->m_isPExist)
                {
                    for (Ipp32s i = 0; i < sliceCount; i++)
                    {
                        H264Slice * pSlice = slicesInfo->GetSlice(i);
                        if (pSlice->GetSliceHeader()->slice_type == PREDSLICE)
                            memset(pFrame->m_mbinfo.MV[1] + pSlice->GetFirstMBNumber(), 0, pSlice->GetMBCount()*sizeof(H264DecoderMacroblockMVs));
                    }
                }
            }

            if (slicesInfo->IsSliceGroups())
            {
                Lock();
                m_pTaskSupplier->SetMBMap(0, pFrame, &m_localResourses);
                Unlock();
            }
        }
        else
        {
            Ipp32s sliceCount = info->GetSliceCount();
            for (Ipp32s i = 0; i < sliceCount; i++)
            {
                H264Slice * pSlice = info->GetSlice(i);
                pSlice->InitializeContexts();
                pSlice->m_mbinfo = &m_localResourses.GetMBInfo(pFrame->m_iResourceNumber);
                pSlice->m_pMBIntraTypes = m_localResourses.GetIntraTypes(pFrame->m_iResourceNumber);
            }

            if (!info->IsSliceGroups() && info->m_isBExist && info->m_isPExist)
            {
                for (Ipp32s i = 0; i < sliceCount; i++)
                {
                    H264Slice * pSlice = info->GetSlice(i);
                    if (pSlice->GetSliceHeader()->slice_type == PREDSLICE)
                        memset(pFrame->m_mbinfo.MV[1] + pSlice->GetFirstMBNumber(), 0, pSlice->GetMBCount()*sizeof(H264DecoderMacroblockMVs));
                }
            }
        }
    }
    catch(...)
    {
        Lock();

        Ipp32s sliceCount = info->GetSliceCount();
        for (Ipp32s i = 0; i < sliceCount; i++)
        {
            H264Slice * pSlice = info->GetSlice(i);
            pSlice->m_bInProcess = false;
            pSlice->m_bDecVacant = 0;
            pSlice->m_bRecVacant = 0;
            pSlice->m_bDebVacant = 0;
            pSlice->CompleteDecoding();
            pSlice->m_bDecoded = true;
            pSlice->m_bDeblocked = true;
        }

        info->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);

        info->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);

        info->m_prepared = 2;
        return false;

    }

    Lock();
    info->m_prepared = 2;
    return false;
}

bool TaskBroker::AddFrameToDecoding(H264DecoderFrame * frame)
{
    if (!frame || frame->IsDecodingStarted() || !IsExistTasks(frame))
        return false;

    VM_ASSERT(frame->IsFrameExist());

    AutomaticUMCMutex guard(m_mGuard);

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

void TaskBroker::RemoveAU(H264DecoderFrameInfo * toRemove)
{
    H264DecoderFrameInfo * temp = m_FirstAU;

    if (!temp)
        return;

    H264DecoderFrameInfo * reference = 0;

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

    H264DecoderFrameInfo * temp1 = temp->GetNextAU();

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

void TaskBroker::CompleteFrame(H264DecoderFrame * frame)
{
    if (!frame || m_decodingQueue.empty() || !frame->IsFullFrame())
        return;

    if (!IsFrameCompleted(frame) || frame->IsDecodingCompleted())
        return;

    m_localResourses.UnlockFrameResource(frame);
    if (frame == m_decodingQueue.front())
    {
        RemoveAU(frame->GetAU(0));
        RemoveAU(frame->GetAU(1));
        m_decodingQueue.pop_front();
    }
    else
    {
        RemoveAU(frame->GetAU(0));
        RemoveAU(frame->GetAU(1));
        m_decodingQueue.remove(frame);
    }

    frame->CompleteDecoding();
}

void TaskBroker::SwitchCurrentAU()
{
    if (!m_FirstAU || !m_FirstAU->IsCompleted())
        return;

    DEBUG_PRINT((VM_STRING("Switch current AU - is_completed %d, poc - %d, m_FirstAU - %X\n"), m_FirstAU->IsCompleted(), m_FirstAU->m_pFrame->m_PicOrderCnt[0], m_FirstAU));

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

            H264DecoderFrameInfo * temp = m_FirstAU;
            for (; temp; temp = temp->GetNextAU())
            {
                temp->SetRefAU(0);

                if (temp->IsReference())
                    break;
            }
            break;
        }

        H264DecoderFrameInfo* completed = m_FirstAU;
        m_FirstAU = m_FirstAU->GetNextAU();
        if (m_FirstAU && m_FirstAU->m_pFrame == completed->m_pFrame)
            m_FirstAU = m_FirstAU->GetNextAU();
        CompleteFrame(completed->m_pFrame);
    }

    InitAUs();
}

void TaskBroker::Start()
{
    AutomaticUMCMutex guard(m_mGuard);

    FrameQueue::iterator iter = m_decodingQueue.begin();

    for (; iter != m_decodingQueue.end(); ++iter)
    {
        H264DecoderFrame * frame = *iter;

        if (IsFrameCompleted(frame))
        {
            CompleteFrame(frame);
            iter = m_decodingQueue.begin();
            if (iter == m_decodingQueue.end()) // avoid ++iter operation
                break;
        }
    }

    InitAUs();
    DEBUG_PRINT((VM_STRING("Start\n")));
}

H264DecoderFrameInfo * TaskBroker::FindAU()
{
    FrameQueue::iterator iter = m_decodingQueue.begin();
    FrameQueue::iterator end_iter = m_decodingQueue.end();

    for (; iter != end_iter; ++iter)
    {
        H264DecoderFrame * frame = *iter;

        H264DecoderFrameInfo *slicesInfo = frame->GetAU(0);

        if (slicesInfo->GetSliceCount())
        {
            if (slicesInfo->IsField())
            {
                if (slicesInfo->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED)
                    return slicesInfo;

                if (frame->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED)
                    return frame->GetAU(1);
            }
            else
            {
                if (slicesInfo->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED)
                    return slicesInfo;
            }
        }

        
        H264DecoderFrame *temp = UMC::GetAuxiliaryFrame(frame);
        if (temp)
        {
            H264DecoderFrameInfo *pSliceInfo = temp->GetAU(0);

            if (pSliceInfo->GetSliceCount())
            {
                if (pSliceInfo->IsField())
                {
                    if (pSliceInfo->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED)
                        return pSliceInfo;

                    if (temp->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED)
                        return temp->GetAU(1);
                }
                else
                {
                    if (pSliceInfo->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED)
                        return pSliceInfo;
                }
            }
        }
    }

    return 0;
}

void TaskBroker::InitAUs()
{
    H264DecoderFrameInfo * pPrev;
    H264DecoderFrameInfo * refAU;
    H264DecoderFrameInfo * refAUAuxiliary;

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

        m_FirstAU->SetStatus(H264DecoderFrameInfo::STATUS_STARTED);

        pPrev = m_FirstAU;
        m_FirstAU->SetPrevAU(0);
        m_FirstAU->SetNextAU(0);
        m_FirstAU->SetRefAU(0);

        refAU = 0;
        refAUAuxiliary = 0;
        if (m_FirstAU->IsReference())
        {
            if (m_FirstAU->IsAuxiliary())
                refAUAuxiliary = m_FirstAU;
            else
                refAU = m_FirstAU;
        }
    }
    else
    {
        pPrev = m_FirstAU;
        refAUAuxiliary = 0;
        refAU = 0;

        pPrev->SetRefAU(0);
        pPrev->SetPrevAU(0);

        if (m_FirstAU->IsReference())
        {
            if (pPrev->IsAuxiliary())
                refAUAuxiliary = pPrev;
            else
                refAU = pPrev;
        }

        while (pPrev->GetNextAU())
        {
            pPrev = pPrev->GetNextAU();

            if (pPrev->IsAuxiliary())
            {
                if (!refAUAuxiliary)
                    pPrev->SetRefAU(0);
            }
            else
            {
                if (!refAU)
                    pPrev->SetRefAU(0);
            }

            if (pPrev->IsReference())
            {
                if (pPrev->IsAuxiliary())
                    refAUAuxiliary = pPrev;
                else
                    refAU = pPrev;
            }
        };
    }

    H264DecoderFrameInfo * pTemp = FindAU();
    for (; pTemp; )
    {
        if (!PrepareFrame(pTemp->m_pFrame))
        {
            pPrev->SetNextAU(0);
            break;
        }

        if (pPrev->IsInterviewReference() && pPrev != refAU && pPrev != refAUAuxiliary)
        {
            if (pPrev->m_pFrame->m_viewId != pTemp->m_pFrame->m_viewId)
            {
                pPrev->SetReference(true);
                if (pPrev->IsAuxiliary())
                    refAUAuxiliary = pPrev;
                else
                    refAU = pPrev;

                if (pPrev->GetPrevAU() && pPrev->m_pFrame == pPrev->GetPrevAU()->m_pFrame)
                {
                    pPrev->GetPrevAU()->SetReference(true);
                    pPrev->SetRefAU(pPrev->GetPrevAU());
                }
            }
        }

        pTemp->SetStatus(H264DecoderFrameInfo::STATUS_STARTED);
        pTemp->SetNextAU(0);
        pTemp->SetPrevAU(pPrev);

        /*if (pTemp->IsIntraAU())
            pTemp->SetRefAU(0);
        else*/
        if (pTemp->IsAuxiliary())
            pTemp->SetRefAU(refAUAuxiliary);
        else
            pTemp->SetRefAU(refAU);

        if (pTemp->IsReference())
        {
            if (pTemp->IsAuxiliary())
                refAUAuxiliary = pTemp;
            else
                refAU = pTemp;
        }

        pPrev->SetNextAU(pTemp);
        pPrev = pTemp;
        pTemp = FindAU();
    }
}

bool TaskBroker::IsFrameCompleted(H264DecoderFrame * pFrame)
{
    if (!pFrame)
        return true;

    if (!pFrame->GetAU(0)->IsCompleted())
        return false;

    //pFrame->GetAU(0)->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED); //quuu
    H264DecoderFrameInfo::FillnessStatus status1 = pFrame->GetAU(1)->GetStatus();

    bool ret;
    switch (status1)
    {
    case H264DecoderFrameInfo::STATUS_NONE:
        ret = true;
        break;
    case H264DecoderFrameInfo::STATUS_NOT_FILLED:
        ret = false;
        break;
    case H264DecoderFrameInfo::STATUS_COMPLETED:
        ret = true;
        break;
    default:
        ret = pFrame->GetAU(1)->IsCompleted();
        break;
    }

    if (ret && !pFrame->IsAuxiliaryFrame())
    {
        ret = IsFrameCompleted(UMC::GetAuxiliaryFrame(pFrame));
    }

    if (ret)
    {
        m_localResourses.UnlockFrameResource(pFrame);
    }

    return ret;
}

bool TaskBroker::GetNextTask(H264Task *pTask)
{
    AutomaticUMCMutex guard(m_mGuard);

    // check error(s)
    if (/*!m_FirstAU ||*/ m_IsShouldQuit)
    {
        return false;
    }

    pTask->m_taskPreparingGuard = &guard;

    bool res = GetNextTaskInternal(pTask);
    return res;
} // bool TaskBroker::GetNextTask(H264Task *pTask)

bool TaskBroker::GetNextSlice(H264DecoderFrameInfo * info, H264Task *pTask)
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

    // try to get slice to decode
    /*if ((false == GetSliceFromCurrentFrame(0)->IsSliceGroups()) ||
        (0 == pTask->m_iThreadNumber))
    {
        if (GetNextSliceToDecoding(pTask))
            return true;
    }*/

    // try to get slice to deblock
    //if ((false == GetSliceFromCurrentFrame(0)->IsSliceGroups()) ||
      //  (0 == pTask->m_iThreadNumber))
    if (info->IsNeedDeblocking())
        return GetNextSliceToDeblocking(info, pTask);

    return false;
} // bool TaskBroker::GetNextSlice(H264Task *pTask)

void TaskBroker::InitTask(H264DecoderFrameInfo * info, H264Task *pTask, H264Slice *pSlice)
{
    pTask->m_bDone = false;
    pTask->m_bError = false;
    pTask->m_iMaxMB = pSlice->m_iMaxMB;
    pTask->m_pSlice = pSlice;
    pTask->m_pSlicesInfo = info;
}

bool TaskBroker::GetNextSliceToDecoding(H264DecoderFrameInfo * info, H264Task *pTask)
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
        H264Slice *pSlice = info->GetSlice(i);

        if ((false == pSlice->m_bInProcess) &&
            (false == pSlice->m_bDecoded))
        {
            InitTask(info, pTask, pSlice);
            pTask->m_iFirstMB = pSlice->m_iFirstMB;
            pTask->m_iMBToProcess = IPP_MIN(pSlice->m_iMaxMB - pSlice->m_iFirstMB, pSlice->m_iAvailableMB);
            pTask->m_iTaskID = TASK_PROCESS;
            pTask->m_pBuffer = NULL;
            pTask->pFunction = &H264SegmentDecoderMultiThreaded::ProcessSlice;
            // we can do deblocking only on independent slices or
            // when all previous slices are deblocked
            if (DEBLOCK_FILTER_ON != pSlice->GetSliceHeader()->disable_deblocking_filter_idc)
                bDoDeblocking = true;
            pSlice->m_bPrevDeblocked = bDoDeblocking;
            pSlice->m_bInProcess = true;
            pSlice->m_bDecVacant = 0;
            pSlice->m_bRecVacant = 0;
            pSlice->m_bDebVacant = 0;

#ifdef ECHO
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) slice dec - % 4d to % 4d\n"), pTask->m_iThreadNumber, pSlice->m_pCurrentFrame->m_PicOrderCnt[0],
                pSlice->GetSliceHeader()->nal_ext.mvc.view_id, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

            return true;
        }
    }

    return false;

} // bool TaskBroker::GetNextSliceToDecoding(H264Task *pTask)

bool TaskBroker::GetNextSliceToDeblocking(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable
    Ipp32s sliceCount = info->GetSliceCount();
    bool bSliceGroups = info->GetSlice(sliceCount - 1)->IsSliceGroups();

    // slice group deblocking
    if (bSliceGroups)
    {
        bool isError = false;
        for (Ipp32s i = 0; i < sliceCount; i += 1)
        {
            H264Slice *pSlice = info->GetSlice(i);

            if (pSlice->m_bInProcess || !pSlice->m_bDecoded)
                return false;

            if (pSlice->m_bError)
                isError = true;
        }

        bool bNothingToDo = true;
        for (Ipp32s i = 0; i < sliceCount; i += 1)
        {
            H264Slice *pSlice = info->GetSlice(i);

            if (pSlice->m_bDeblocked)
                continue;

            bNothingToDo = false;
        }

        // we already deblocked
        if (bNothingToDo)
            return false;

        H264Slice *pSlice = info->GetSlice(sliceCount - 1);
        pSlice->m_bInProcess = true;
        pSlice->m_bDebVacant = 0;
        InitTask(info, pTask, pSlice);
        pTask->m_bError = isError;
        pTask->m_iFirstMB = 0;
        Ipp32s iMBInFrame = (pSlice->m_iMBWidth * pSlice->m_iMBHeight) /
                            ((pSlice->GetSliceHeader()->field_pic_flag) ? (2) : (1));
        pTask->m_iMaxMB = iMBInFrame;
        pTask->m_iMBToProcess = iMBInFrame;

        pTask->m_iTaskID = TASK_DEB_FRAME;
        pTask->m_pBuffer = 0;
        pTask->pFunction = &H264SegmentDecoderMultiThreaded::DeblockSegmentTask;

#ifdef ECHO_DEB
        DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) frame deb - % 4d to % 4d\n"), pTask->m_iThreadNumber, pSlice->m_pCurrentFrame->m_PicOrderCnt[0],
                pSlice->GetSliceHeader()->nal_ext.mvc.view_id, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO_DEB

        return true;
    }
    else
    {
        Ipp32s i;
        bool bPrevDeblocked = true;

        for (i = 0; i < sliceCount; i += 1)
        {
            H264Slice *pSlice = info->GetSlice(i);

            // we can do deblocking only on vacant slices
            if ((false == pSlice->m_bInProcess) &&
                (true == pSlice->m_bDecoded) &&
                (false == pSlice->m_bDeblocked))
            {
                // we can do this only when previous slice was deblocked or
                // deblocking isn't going through slice boundaries
                if ((true == bPrevDeblocked) ||
                    (false == pSlice->DeblockThroughBoundaries()))
                {
                    InitTask(info, pTask, pSlice);
                    pTask->m_iFirstMB = pSlice->m_iFirstMB;
                    pTask->m_iMBToProcess = pSlice->m_iMaxMB - pSlice->m_iFirstMB;
                    pTask->m_iTaskID = TASK_DEB_SLICE;
                    pTask->m_pBuffer = NULL;
                    pTask->pFunction = &H264SegmentDecoderMultiThreaded::DeblockSegmentTask;

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
    }

    return false;

} // bool TaskBroker::GetNextSliceToDeblocking(H264Task *pTask)

void TaskBroker::AddPerformedTask(H264Task *pTask)
{
    AutomaticUMCMutex guard(m_mGuard);

    H264Slice *pSlice = pTask->m_pSlice;
    H264DecoderFrameInfo * info = pTask->m_pSlicesInfo;

#if defined(ECHO)
        switch (pTask->m_iTaskID)
        {
        case TASK_PROCESS:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) slice dec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEC:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) dec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()),  pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_REC:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) rec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEC_REC:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) dec_rec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEB:
#if defined(ECHO_DEB)
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) deb done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
#else
        case TASK_DEB_FRAME:
        case TASK_DEB_FRAME_THREADED:
        case TASK_DEB_SLICE:
        case TASK_DEB_THREADED:
#endif // defined(ECHO_DEB)
            break;
        default:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) default task done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        }
#endif // defined(ECHO)

#ifdef TIME
    timer.ThreadFinished(pTask->m_iThreadNumber, pTask->m_iTaskID);
#endif // TIME

    // when whole slice was processed
    if (TASK_PROCESS == pTask->m_iTaskID)
    {
        // it is possible only in "slice group" mode
        if (pSlice->IsSliceGroups())
        {
            pSlice->m_iMaxMB = pTask->m_iMaxMB;
            pSlice->m_iAvailableMB -= pTask->m_iMBToProcess;

            if (pTask->m_bError)
            {
                pSlice->m_bError = true;
                pSlice->m_bDeblocked = true;
            }

            /*
            // correct remain uncompressed macroblock count.
            // we can't relay on slice number cause of field pictures.
            if (pSlice->m_iAvailableMB)
            {
                Ipp32s pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                VM_ASSERT(pos >= 0);
                H264Slice * pNextSlice = info->GetSlice(pos + 1);
                if (pNextSlice)
                {
                    //pNextSlice->m_iAvailableMB = pSlice->m_iAvailableMB;
                }
            }*/
        }
        else
        { // slice is deblocked only when deblocking was available
            // check condition for frame deblocking
            //if (DEBLOCK_FILTER_ON_NO_SLICE_EDGES == pSlice->GetSliceHeader()->disable_deblocking_filter_idc)
                //m_bDoFrameDeblocking = false; // DEBUG : ADB

            // error handling
            pSlice->m_iMaxMB = pTask->m_iMaxMB;
            if (pTask->m_bError)
            {
                pSlice->m_bError = true;
            }

            if (false == pSlice->m_bDeblocked)
                pSlice->m_bDeblocked = pSlice->m_bPrevDeblocked;
        }
        // slice is decoded
        pSlice->m_bDecVacant = 0;
        pSlice->m_bRecVacant = 0;
        pSlice->m_bDebVacant = 1;
        pSlice->m_bInProcess = false;
        pSlice->CompleteDecoding();
        pSlice->m_bDecoded = true;
    }
    else if (TASK_DEB_SLICE == pTask->m_iTaskID)
    {
        pSlice->m_bDebVacant = 1;
        pSlice->m_bDeblocked = 1;
        pSlice->m_bInProcess = false;
    }
    else if (TASK_DEB_FRAME == pTask->m_iTaskID)
    {
        Ipp32s sliceCount = m_FirstAU->GetSliceCount();

        // frame is deblocked
        for (Ipp32s i = 0; i < sliceCount; i += 1)
        {
            H264Slice *pTemp = m_FirstAU->GetSlice(i);

            pTemp->m_bDebVacant = 1;
            pTemp->m_bInProcess = false;
            pTemp->CompleteDecoding();
            pTemp->m_bDeblocked = true;
        }
    }
    else
    {
        switch (pTask->m_iTaskID)
        {
        case TASK_DEC:
            {
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToDec);

            pSlice->m_iCurMBToDec += pTask->m_iMBToProcess;
            // move filled buffer to reconstruct queue

            VM_ASSERT(pSlice->GetCoeffsBuffers() || pSlice->m_bError);

            if (pSlice->GetCoeffsBuffers())
                pSlice->GetCoeffsBuffers()->UnLockInputBuffer(pTask->m_WrittenSize);

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
                        H264Slice * pNextSlice = info->GetSlice(++pos);
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

            DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) dec ready %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()), info->m_iDecMBReady));
            }
            break;

        case TASK_REC:
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

                //pSlice->m_iMaxMB = IPP_MIN(pSlice->m_iCurMBToRec, pSlice->m_iMaxMB);
                pSlice->m_bError = true;
            }

            VM_ASSERT(pSlice->GetCoeffsBuffers() || pSlice->m_bError);
            if (pSlice->GetCoeffsBuffers())
                pSlice->GetCoeffsBuffers()->UnLockOutputBuffer();

            if (pSlice->m_iMaxMB <= pSlice->m_iCurMBToRec)
            {
                if (isReadyIncrease)
                {
                    Ipp32s pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    if (pos >= 0)
                    {
                        H264Slice * pNextSlice = info->GetSlice(pos + 1);
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
                pSlice->CompleteDecoding();
                pSlice->m_bDecoded = true;

                // check condition for frame deblocking
                //if (DEBLOCK_FILTER_ON_NO_SLICE_EDGES == pSlice->GetSliceHeader()->disable_deblocking_filter_idc)
                //    m_bDoFrameDeblocking = false;  // DEBUG : ADB
            }
            else
            {
                pSlice->m_bRecVacant = 1;
            }
            DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) rec ready %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()), info->m_iRecMBReady));
            }
            break;

        case TASK_DEC_REC:
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
                    H264Slice * pNextSlice = info->GetSlice(pos + 1);
                    if (pNextSlice)
                    {
                        if (isReadyIncreaseDec)
                        {
                            Ipp32s pos1 = pos;
                            H264Slice * pTmpSlice = info->GetSlice(++pos1);

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
                pSlice->CompleteDecoding();
                pSlice->m_bDecoded = true;
            }
            else
            {
                pSlice->m_bDecVacant = 1;
                pSlice->m_bRecVacant = 1;
            }

            DEBUG_PRINT((VM_STRING("(%d) (%d) (%d) (viewId - %d) (%d) dec_rec ready %d %d\n"), pTask->m_iThreadNumber, (Ipp32s)(info != m_FirstAU), info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()), info->m_iDecMBReady, info->m_iRecMBReady));
            }
            break;

        case TASK_DEB:
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
                if (pSlice->m_bDecoded)
                {
                    Ipp32s pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    if (isReadyIncrease)
                    {
                        VM_ASSERT(pos >= 0);
                        H264Slice * pNextSlice = info->GetSlice(pos + 1);
                        if (pNextSlice)
                        {
                            if (pNextSlice->m_bDeblocked)
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToRec;
                            else
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToDeb;
                        }
                    }

                    info->RemoveSlice(pos);

                    pSlice->m_bInProcess = false;
                    pSlice->m_bDecVacant = 0;
                    pSlice->m_bRecVacant = 0;
                    pSlice->CompleteDecoding();
                }
                pSlice->m_bDeblocked = true;
            }
            else
            {
                pSlice->m_bDebVacant = 1;
            }

            DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) after deb ready %d %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()), info->m_iDecMBReady, info->m_iRecMBReady));
            }
            break;
#if 0
        case TASK_DEB_THREADED:
            pSlice->m_DebTools.SetProcessedMB(pTask);
            if (pSlice->m_DebTools.IsDeblockingDone())
                pSlice->m_bDeblocked = true;

            break;

        case TASK_DEB_FRAME_THREADED:
            m_DebTools.SetProcessedMB(pTask);
            if (m_DebTools.IsDeblockingDone())
            {
                Ipp32s i;

                // frame is deblocked
                for (i = 0; i < GetNumberOfSlicesFromCurrentFrame(); i += 1)
                {
                    H264Slice *pTemp = GetSliceFromCurrentFrame(i);

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

} // void TaskBroker::AddPerformedTask(H264Task *pTask)

#define Check_Status(status)  ((status == H264DecoderFrameInfo::STATUS_FILLED) || (status == H264DecoderFrameInfo::STATUS_STARTED))

Ipp32s TaskBroker::GetNumberOfTasks(bool details)
{
    Ipp32s au_count = 0;

    H264DecoderFrameInfo * temp = m_FirstAU;

    for (; temp ; temp = temp->GetNextAU())
    {
        if (temp->GetStatus() == H264DecoderFrameInfo::STATUS_COMPLETED)
            continue;

        if (details)
        {
            au_count += temp->GetSliceCount();
        }
        else
        {
            if (!temp->IsField())
            {
                au_count++;
            }

            au_count++;
        }
    }

    return au_count;
}

bool TaskBroker::IsEnoughForStartDecoding(bool force)
{
    AutomaticUMCMutex guard(m_mGuard);

    InitAUs();
    Ipp32s au_count = GetNumberOfTasks(false);

    Ipp32s additional_tasks = m_iConsumerNumber;
    if (m_iConsumerNumber > 6)
        additional_tasks -= 1;

    if ((au_count >> 1) >= additional_tasks || (force && au_count))
        return true;

    return false;
}

bool TaskBroker::IsExistTasks(H264DecoderFrame * frame)
{
    H264DecoderFrameInfo *slicesInfo = frame->GetAU(0);

    return Check_Status(slicesInfo->GetStatus());
}

static Ipp32s GetDecodingOffset(H264DecoderFrameInfo * curInfo, Ipp32s &readyCount)
{
    Ipp32s offset = 0;

    H264DecoderFrameInfo * refInfo = curInfo->GetRefAU();
    VM_ASSERT(refInfo);

    if (curInfo->m_pFrame != refInfo->m_pFrame)
    {
        switch(curInfo->m_pFrame->m_PictureStructureForDec)
        {
        case FRM_STRUCTURE:
            if (refInfo->m_pFrame->m_PictureStructureForDec == FLD_STRUCTURE)
            {
                readyCount *= 2;
                offset++;
            }
            else
            {
            }
            break;
        case AFRM_STRUCTURE:
            if (refInfo->m_pFrame->m_PictureStructureForDec == FLD_STRUCTURE)
            {
                readyCount *= 2;
                offset++;
            }
            else
            {
            }
            break;
        case FLD_STRUCTURE:
            switch(refInfo->m_pFrame->m_PictureStructureForDec)
            {
            case FLD_STRUCTURE:
                break;
            case AFRM_STRUCTURE:
            case FRM_STRUCTURE:
                readyCount /= 2;
                offset++;
                break;
            }
            break;
        }
    }

    return offset;
}

TaskBrokerSingleThread::TaskBrokerSingleThread(TaskSupplier * pTaskSupplier)
    : TaskBroker(pTaskSupplier)
{
}

// Get next working task
bool TaskBrokerSingleThread::GetNextTaskInternal(H264Task *pTask)
{
    while (false == m_IsShouldQuit)
    {
        if (m_FirstAU)
        {
            if (GetNextSlice(m_FirstAU, pTask))
                return true;

            if (m_FirstAU->IsCompleted())
            {
                SwitchCurrentAU();
            }

            if (GetNextSlice(m_FirstAU, pTask))
                return true;
        }

        break;
    }

    return false;
}

TaskBrokerTwoThread::TaskBrokerTwoThread(TaskSupplier * pTaskSupplier)
    : TaskBrokerSingleThread(pTaskSupplier)
{
}

bool TaskBrokerTwoThread::Init(Ipp32s iConsumerNumber)
{
    if (!TaskBroker::Init(iConsumerNumber))
        return false;

    return true;
}

void TaskBrokerTwoThread::Reset()
{
    AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread::Reset();
}

void TaskBrokerTwoThread::Release()
{
    AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread::Release();
}

void TaskBrokerTwoThread::CompleteFrame()
{
    if (!m_FirstAU)
        return;

    DEBUG_PRINT((VM_STRING("frame completed - poc - %d\n"), m_FirstAU->m_pFrame->m_PicOrderCnt[0]));
    SwitchCurrentAU();
}

bool TaskBrokerTwoThread::GetNextTaskInternal(H264Task *pTask)
{
#ifdef TIME
    CStarter start(pTask->m_iThreadNumber);
#endif // TIME

    while (false == m_IsShouldQuit)
    {
        if (m_FirstAU)
        {
            for (H264DecoderFrameInfo *pTemp = m_FirstAU; pTemp; pTemp = pTemp->GetNextAU())
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

    return false;
}

bool TaskBrokerTwoThread::WrapDecodingTask(H264DecoderFrameInfo * info, H264Task *pTask, H264Slice *pSlice)
{
    VM_ASSERT(pSlice);

    if (!pSlice->GetCoeffsBuffers())
        pSlice->m_coeffsBuffers = m_localResourses.AllocateCoeffBuffer(pSlice);

    // this is guarded function, safe to touch any variable
    if (1 == pSlice->m_bDecVacant && pSlice->GetCoeffsBuffers()->IsInputAvailable())
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
        pTask->m_iTaskID = TASK_DEC;
        pTask->m_pBuffer = (UMC::CoeffsPtrCommon)pSlice->GetCoeffsBuffers()->LockInputBuffer();
        pTask->pFunction = &H264SegmentDecoderMultiThreaded::DecodeSegment;

#ifdef ECHO
        DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) dec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
            info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

        return true;
    }

    return false;

} // bool TaskBrokerTwoThread::WrapDecodingTask(H264Task *pTask, H264Slice *pSlice)

bool TaskBrokerTwoThread::WrapReconstructTask(H264DecoderFrameInfo * info, H264Task *pTask, H264Slice *pSlice)
{
    VM_ASSERT(pSlice);
    // this is guarded function, safe to touch any variable

    if (1 == pSlice->m_bRecVacant && pSlice->GetCoeffsBuffers()->IsOutputAvailable())
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

        pTask->m_iTaskID = TASK_REC;
        Ipp8u* pointer = NULL;
        size_t size;
        pSlice->GetCoeffsBuffers()->LockOutputBuffer(pointer, size);
        pTask->m_pBuffer = ((UMC::CoeffsPtrCommon) pointer);
        pTask->pFunction = &H264SegmentDecoderMultiThreaded::ReconstructSegment;

#ifdef ECHO
        DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d)  (%d) rec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
            info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

        return true;
    }

    return false;

} // bool TaskBrokerTwoThread::WrapReconstructTask(H264Task *pTaskm H264Slice *pSlice)

bool TaskBrokerTwoThread::WrapDecRecTask(H264DecoderFrameInfo * info, H264Task *pTask, H264Slice *pSlice)
{
    VM_ASSERT(pSlice);
    // this is guarded function, safe to touch any variable

    if (1 == pSlice->m_bRecVacant && 1 == pSlice->m_bDecVacant &&
        pSlice->m_iCurMBToDec == pSlice->m_iCurMBToRec)
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

        pTask->m_iTaskID = TASK_DEC_REC;
        pTask->m_pBuffer = 0;
        pTask->pFunction = &H264SegmentDecoderMultiThreaded::DecRecSegment;

#ifdef ECHO
        DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) dec_rec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
            info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (Ipp32s)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

        return true;
    }

    return false;

} // bool TaskBrokerTwoThread::WrapDecRecTask(H264Task *pTaskm H264Slice *pSlice)

bool TaskBrokerTwoThread::GetDecodingTask(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable
    Ipp32s i;

    H264DecoderFrameInfo * refAU = info->GetRefAU();
    bool is_need_check = refAU != 0;
    Ipp32s readyCount = 0;
    Ipp32s additionalLines = 0;

    if (is_need_check)
    {
        if (refAU->GetStatus() == H264DecoderFrameInfo::STATUS_COMPLETED)
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
        H264Slice *pSlice = info->GetSlice(i);

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

} // bool TaskBrokerTwoThread::GetDecodingTask(H264DecoderFrameInfo * info, H264Task *pTask)

bool TaskBrokerTwoThread::GetReconstructTask(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable

    H264DecoderFrameInfo * refAU = info->GetRefAU();
    bool is_need_check = refAU != 0;
    Ipp32s readyCount = 0;
    Ipp32s additionalLines = 0;

    if (is_need_check)
    {
        if (refAU->GetStatus() == H264DecoderFrameInfo::STATUS_COMPLETED)
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
        H264Slice *pSlice = info->GetSlice(i);

        if (pSlice->m_bRecVacant && !pSlice->GetCoeffsBuffers())
            break;

        if (pSlice->m_bRecVacant != 1 || !pSlice->GetCoeffsBuffers() ||
            !pSlice->GetCoeffsBuffers()->IsOutputAvailable())
        {
            continue;
        }

        if (is_need_check)
        {
            Ipp32s distortion = pSlice->m_MVsDistortion + (refAU->IsNeedDeblocking() ? 4 : 0);
            Ipp32s k = ( (distortion + 15) / 16) + 1; // +2 - (1 - for padding, 2 - current line)
            if (pSlice->m_iCurMBToRec + (k + additionalLines)*pSlice->GetMBRowWidth() >= readyCount)
                break;
        }

        if (WrapReconstructTask(info, pTask, pSlice))
        {
            return true;
        }
    }

    return false;

} // bool TaskBrokerTwoThread::GetReconstructTask(H264Task *pTask)

bool TaskBrokerTwoThread::GetDecRecTask(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable
    if (info->GetRefAU() || info->GetSliceCount() == 1)
        return false;

    Ipp32s i;
    Ipp32s sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H264Slice *pSlice = info->GetSlice(i);

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

bool TaskBrokerTwoThread::GetDeblockingTask(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable
    Ipp32s i;
    bool bPrevDeblocked = true;

    Ipp32s sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H264Slice *pSlice = info->GetSlice(i);

        Ipp32s iMBWidth = pSlice->GetMBRowWidth(); // DEBUG : always deblock two lines !!!
        Ipp32s iAvailableToDeblock;
        Ipp32s iDebUnit = (pSlice->GetSliceHeader()->MbaffFrameFlag) ? (2) : (1);

        iAvailableToDeblock = pSlice->m_iCurMBToRec -
                              pSlice->m_iCurMBToDeb;

        if ((false == pSlice->m_bDeblocked) &&
            ((true == bPrevDeblocked) || (false == pSlice->DeblockThroughBoundaries())) &&
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
                pTask->m_iMBToProcess = align_value<Ipp32s> (pTask->m_iMBToProcess, iDebUnit);
            }

            pTask->m_iTaskID = TASK_DEB;
            pTask->pFunction = &H264SegmentDecoderMultiThreaded::DeblockSegmentTask;

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

} // bool TaskBrokerTwoThread::GetDeblockingTask(H264Task *pTask)

bool TaskBrokerTwoThread::GetNextTaskManySlices(H264DecoderFrameInfo * info, H264Task *pTask)
{
    if (!info)
        return false;

    if (GetPreparationTask(info))
        return true;

    if (info->m_prepared != 2)
        return false;

    if (info->IsSliceGroups())
    {
        return GetNextSlice(info, pTask);
    }

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

void TaskBrokerTwoThread::AddPerformedTask(H264Task *pTask)
{
    AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread::AddPerformedTask(pTask);

    if (pTask->m_pSlicesInfo->IsCompleted())
    {
        pTask->m_pSlicesInfo->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
    }

    SwitchCurrentAU();
} // void TaskBrokerTwoThread::AddPerformedTask(H264Task *pTask)

bool TaskBrokerTwoThread::GetFrameDeblockingTaskThreaded(H264DecoderFrameInfo * , H264Task *)
{
#if 1
    return false;
#else
    // this is guarded function, safe to touch any variable
    Ipp32s sliceCount = info->GetSliceCount();
    H264Slice * firstSlice = info->GetSlice(0);

    if (m_bFirstDebThreadedCall)
    {
        Ipp32s i;
        Ipp32s iFirstMB = -1, iMaxMB = -1;
        Ipp32s iMBWidth = firstSlice->GetMBRowWidth();
        Ipp32s iDebUnit = (firstSlice->GetSliceHeader()->MbaffFrameFlag) ? (2) : (1);
        bool bError = false;

        // check all other threads are sleep
        for (i = 0; i < sliceCount; i++)
        {
            H264Slice *pSlice = info->GetSlice(i);

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
            H264Slice *pSlice = info->GetSlice(i);

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
            H264Slice *pSlice = info->GetSlice(i);

            if ((pTask->m_iFirstMB >= pSlice->m_iFirstMB) &&
                (pTask->m_iFirstMB < pSlice->m_iMaxMB))
            {
                pTask->m_iTaskID = TASK_DEB_FRAME_THREADED;
                pTask->m_pSlice = pSlice;
                if (pTask->m_iFirstMB + pTask->m_iMBToProcess > pSlice->m_iMaxMB)
                    pTask->m_iMBToProcess = pSlice->m_iMaxMB - pTask->m_iFirstMB;
                break;
            }
        }
    }

    return true;
#endif
} // bool TaskBrokerTwoThread::GetFrameDeblockingTaskThreaded(H264Task *pTask)


/****************************************************************************************************/
// Resources
/****************************************************************************************************/

LocalResources::LocalResources()
    : next_mb_tables(0)

    , m_ppMBIntraTypes(0)
    , m_piMBIntraProp(0)
    , m_pMBInfo(0)
    , m_numberOfBuffers(0)
    , m_pMemoryAllocator(0)
    , m_parsedDataLength(0)
    , m_pParsedData(0)
    , m_midParsedData(0)
    , m_currentResourceIndex(0)
{
}

LocalResources::~LocalResources()
{
    Close();
}

Status LocalResources::Init(Ipp32s numberOfBuffers, MemoryAllocator *pMemoryAllocator)
{
    Close();

    m_numberOfBuffers = numberOfBuffers;
    m_pMemoryAllocator = pMemoryAllocator;

    m_pMBInfo = new H264DecoderLocalMacroblockDescriptor[numberOfBuffers];
    if (NULL == m_pMBInfo)
        return UMC_ERR_ALLOC;

    m_ppMBIntraTypes = new Ipp32u *[numberOfBuffers];
    if (NULL == m_ppMBIntraTypes)
        return UMC_ERR_ALLOC;
    memset(m_ppMBIntraTypes, 0, sizeof(Ipp32u *)*numberOfBuffers);

    // allocate intra MB types array's sizes
    m_piMBIntraProp = new H264IntraTypesProp[numberOfBuffers];
    if (NULL == m_piMBIntraProp)
        return UMC_ERR_ALLOC;
    memset(m_piMBIntraProp, 0, sizeof(H264IntraTypesProp) * numberOfBuffers);

    next_mb_tables = new H264DecoderMBAddr *[numberOfBuffers + 1];
    return UMC_OK;
}

void LocalResources::Reset()
{
    for (Ipp32s i = 0; i < m_numberOfBuffers; i++)
    {
        m_pMBInfo[i].m_isBusy = false;
        m_pMBInfo[i].m_pFrame = 0;
    }

    m_currentResourceIndex = 0;
}

void LocalResources::Close()
{
    if (m_ppMBIntraTypes)
    {
        delete[] m_ppMBIntraTypes;
        m_ppMBIntraTypes = 0;
    }

    if (m_piMBIntraProp)
    {
        for (Ipp32s i = 0; i < m_numberOfBuffers; i++)
        {
            if (m_piMBIntraProp[i].m_nSize == 0)
                continue;
            m_pMemoryAllocator->Unlock(m_piMBIntraProp[i].m_mid);
            m_pMemoryAllocator->Free(m_piMBIntraProp[i].m_mid);
        }

        delete[] m_piMBIntraProp;
        m_piMBIntraProp = 0;
    }

    delete[] m_pMBInfo;
    m_pMBInfo = 0;

    delete[] next_mb_tables;
    next_mb_tables = 0;

    DeallocateBuffers();

    m_numberOfBuffers = 0;
    m_pMemoryAllocator = 0;
    m_currentResourceIndex = 0;
}

Ipp32u LocalResources::GetCurrentResourceIndex()
{
    Ipp32s index = m_currentResourceIndex % (m_numberOfBuffers);
    m_currentResourceIndex++;
    return index;
}

bool LocalResources::LockFrameResource(H264DecoderFrame * frame)
{
    Ipp32s number = frame->m_iResourceNumber;
    if (m_pMBInfo[number].m_isBusy)
        return m_pMBInfo[number].m_pFrame == frame;

    m_pMBInfo[number].m_isBusy = true;
    m_pMBInfo[number].m_pFrame = frame;
    return true;
}

void LocalResources::UnlockFrameResource(H264DecoderFrame * frame)
{
    Ipp32s number = frame->m_iResourceNumber;
    if (number < 0 || m_pMBInfo[number].m_pFrame != frame)
        return;

    m_pMBInfo[number].m_isBusy = false;
    m_pMBInfo[number].m_pFrame = 0;
}

H264DecoderFrame * LocalResources::IsBusyByFrame(Ipp32s number)
{
    return m_pMBInfo[number].m_pFrame;
}

H264CoeffsBuffer * LocalResources::AllocateCoeffBuffer(H264Slice * slice)
{
    H264CoeffsBuffer * coeffsBuffers = m_ObjHeap.AllocateObject<H264CoeffsBuffer>();
    coeffsBuffers->IncrementReference();


    Ipp32s iMBRowSize = slice->GetMBRowWidth();
    Ipp32s iMBRowBuffers;
    Ipp32s bit_depth_luma, bit_depth_chroma;
    if (slice->GetSliceHeader()->is_auxiliary)
    {
        bit_depth_luma = slice->GetSeqParamEx()->bit_depth_aux;
        bit_depth_chroma = 8;
    } else {
        bit_depth_luma = slice->GetSeqParam()->bit_depth_luma;
        bit_depth_chroma = slice->GetSeqParam()->bit_depth_chroma;
    }

    Ipp32s isU16Mode = (bit_depth_luma > 8 || bit_depth_chroma > 8) ? 2 : 1;

    // decide number of buffers
    if (slice->m_iMaxMB - slice->m_iFirstMB > iMBRowSize)
    {
        iMBRowBuffers = (slice->m_iMaxMB - slice->m_iFirstMB + iMBRowSize - 1) / iMBRowSize;
        iMBRowBuffers = IPP_MIN(MINIMUM_NUMBER_OF_ROWS, iMBRowBuffers);
    }
    else
    {
        iMBRowSize = slice->m_iMaxMB - slice->m_iFirstMB;
        iMBRowBuffers = 1;
    }

    coeffsBuffers->Init(iMBRowBuffers, (Ipp32s)sizeof(Ipp16s) * isU16Mode * (iMBRowSize * COEFFICIENTS_BUFFER_SIZE + DEFAULT_ALIGN_VALUE));
    coeffsBuffers->Reset();
    return coeffsBuffers;
}

void LocalResources::FreeCoeffBuffer(H264CoeffsBuffer *buffer)
{
    if (buffer)
    {
        buffer->Reset();
        m_ObjHeap.FreeObject(buffer);
    }
}

void LocalResources::AllocateMBIntraTypes(Ipp32s iIndex, Ipp32s iMBNumber)
{
    if ((0 == m_ppMBIntraTypes[iIndex]) ||
        (m_piMBIntraProp[iIndex].m_nSize < iMBNumber))
    {
        size_t nSize;
        // delete previously allocated array
        if (m_ppMBIntraTypes[iIndex])
        {
            m_pMemoryAllocator->Unlock(m_piMBIntraProp[iIndex].m_mid);
            m_pMemoryAllocator->Free(m_piMBIntraProp[iIndex].m_mid);
        }
        m_ppMBIntraTypes[iIndex] = NULL;
        m_piMBIntraProp[iIndex].Reset();

        nSize = iMBNumber * NUM_INTRA_TYPE_ELEMENTS * sizeof(IntraType);
        if (UMC_OK != m_pMemoryAllocator->Alloc(&(m_piMBIntraProp[iIndex].m_mid),
                                                nSize,
                                                UMC_ALLOC_PERSISTENT))
            throw h264_exception(UMC_ERR_ALLOC);
        m_piMBIntraProp[iIndex].m_nSize = (Ipp32s) iMBNumber;
        m_ppMBIntraTypes[iIndex] = (Ipp32u *) m_pMemoryAllocator->Lock(m_piMBIntraProp[iIndex].m_mid);
    }
}

H264DecoderLocalMacroblockDescriptor & LocalResources::GetMBInfo(Ipp32s number)
{
    return m_pMBInfo[number];
}

void LocalResources::AllocateMBInfo(Ipp32s number, Ipp32u iMBCount)
{
    if (!m_pMBInfo[number].Allocate(iMBCount, m_pMemoryAllocator))
        throw h264_exception(UMC_ERR_ALLOC);
}

IntraType * LocalResources::GetIntraTypes(Ipp32s number)
{
    return m_ppMBIntraTypes[number];
}

H264DecoderMBAddr * LocalResources::GetDefaultMBMapTable() const
{
    return next_mb_tables[0];
}

void LocalResources::AllocateBuffers(Ipp32s mb_count)
{
    Ipp32s     next_mb_size = (Ipp32s)(mb_count*sizeof(H264DecoderMBAddr));
    Ipp32s     totalSize = (m_numberOfBuffers + 1)*next_mb_size + mb_count + 128; // 128 used for alignments

    // Reallocate our buffer if its size is not appropriate.
    if (m_parsedDataLength < totalSize)
    {
        DeallocateBuffers();

        if (m_pMemoryAllocator->Alloc(&m_midParsedData,
                                      totalSize,
                                      UMC_ALLOC_PERSISTENT))
            throw h264_exception(UMC_ERR_ALLOC);

        m_pParsedData = (Ipp8u *) m_pMemoryAllocator->Lock(m_midParsedData);
        ippsZero_8u(m_pParsedData, totalSize);

        m_parsedDataLength = totalSize;
    }
    else
        return;

    size_t     offset = 0;

    m_pMBMap = align_pointer<Ipp8u *> (m_pParsedData);
    offset += mb_count;

    if (offset & 0x7)
        offset = (offset + 7) & ~7;
    next_mb_tables[0] = align_pointer<H264DecoderMBAddr *> (m_pParsedData + offset);

    //initialize first table
    for (Ipp32s i = 0; i < mb_count; i++)
        next_mb_tables[0][i] = i + 1; // simple linear scan

    offset += next_mb_size;

    for (Ipp32s i = 1; i <= m_numberOfBuffers; i++)
    {
        if (offset & 0x7)
            offset = (offset + 7) & ~7;

        next_mb_tables[i] = align_pointer<H264DecoderMBAddr *> (m_pParsedData + offset);
        offset += next_mb_size;
    }
}

void LocalResources::DeallocateBuffers()
{
    if (m_pParsedData)
    {
        // Free the old buffer.
        m_pMemoryAllocator->Unlock(m_midParsedData);
        m_pMemoryAllocator->Free(m_midParsedData);
        m_pParsedData = 0;
        m_midParsedData = 0;
    }

    m_parsedDataLength = 0;
}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
