//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include <cstdarg>
#include "umc_h265_task_broker.h"
#include "umc_h265_task_supplier.h"
#include "umc_h265_frame_list.h"

#include "umc_h265_debug.h"

#ifndef MFX_VA
#include "umc_h265_segment_decoder_mt.h"
#endif

//#define ECHO
//#define ECHO_DEB

#if 0

#undef DEBUG_PRINT

#if defined ECHO
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

namespace UMC_HEVC_DECODER
{
H265Slice* FindSliceByCUAddr(H265DecoderFrameInfo * info, Ipp32s firstCUToProcess);
TileThreadingInfo * FindTileForProcess(H265DecoderFrameInfo * info, Ipp32s taskID, bool isDecRec = false);

#if defined ECHO
const vm_char * GetTaskString(H265Task * task)
{
    const vm_char * taskIdStr;
    switch(task->m_iTaskID)
    {
    case TASK_SAO_H265:
        taskIdStr = VM_STRING("SAO");
        break;
    case TASK_DEB_H265:
        taskIdStr = VM_STRING("deb");
        break;
    case TASK_DEC_H265:
        taskIdStr = VM_STRING("dec");
        break;
    case TASK_REC_H265:
        taskIdStr = VM_STRING("rec");
        break;
    case TASK_DEC_REC_H265:
        taskIdStr = VM_STRING("dec rec");
        break;
    case TASK_PROCESS_H265:
        taskIdStr = VM_STRING("slice");
        break;

    default:
        taskIdStr = VM_STRING("unknown task id");
        break;
    }

    return taskIdStr;
}

bool filterPrinting(H265Task * task, H265DecoderFrameInfo* info)
{
    if (task->m_pSlice)
    {
        if (task->m_pSlice->GetCurrentFrame()->m_UID != 26)
            return false;
    }
    return true;
}

inline void PrintTask(H265Task * task)
{
    H265DecoderFrameInfo* info = task->m_pSlicesInfo;
    if (!filterPrinting(task, info))
        return;

    const vm_char * taskIdStr = GetTaskString(task);

#ifndef ECHO_DEB
    if (task->m_iTaskID == TASK_DEB_H265 || task->m_iTaskID == TASK_SAO_H265)
        return ;
#endif

    if (info)
        DEBUG_PRINT((VM_STRING("(thr - %d) (poc - %d) (uid - %d) %s - % 4d to % 4d\n"), task->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, info->m_pFrame->m_UID, taskIdStr, task->m_iFirstMB, task->m_iFirstMB + task->m_iMBToProcess));
    else
        DEBUG_PRINT((VM_STRING("(thr - %d) %s - % 4d to % 4d\n"), task->m_iThreadNumber, taskIdStr, task->m_iFirstMB, task->m_iFirstMB + task->m_iMBToProcess));
}

inline void PrintTaskDone(H265Task * task, H265DecoderFrameInfo* info)
{
    if (!filterPrinting(task, info))
        return;

    const vm_char * taskIdStr = GetTaskString(task);

    DEBUG_PRINT((VM_STRING("(thr - %d) (poc - %d) (uid - %d) %s done % 4d to % 4d - error - %d\n"), task->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, info->m_pFrame->m_UID,
        taskIdStr, task->m_iFirstMB, task->m_iFirstMB + task->m_iMBToProcess, task->m_bError));
}

inline void PrintTaskDoneAdditional(H265Task * task, H265DecoderFrameInfo* info)
{
    if (!filterPrinting(task, info))
        return;

    const vm_char * taskIdStr = GetTaskString(task);

    Ipp32s readyIndex = DEC_PROCESS_ID;
    switch(task->m_iTaskID)
    {
    case TASK_DEC_H265:
        readyIndex = DEC_PROCESS_ID;
        break;
    case TASK_REC_H265:
        readyIndex = REC_PROCESS_ID;
        break;
    case TASK_DEB_H265:
        readyIndex = DEB_PROCESS_ID;
        break;
    case TASK_SAO_H265:
        readyIndex = SAO_PROCESS_ID;
        break;
    }

    if (task->m_pSlicesInfo->m_hasTiles)
    {
        DEBUG_PRINT((VM_STRING("(%d) (poc - %d) (uid - %d) %s ready %d\n"), task->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, info->m_pFrame->m_UID, taskIdStr, task->m_pSlicesInfo->m_curCUToProcess[readyIndex]));
    }
    else
    {
        if (task->m_iTaskID == TASK_SAO_H265)
            DEBUG_PRINT((VM_STRING("(%d) (poc - %d) (uid - %d) %s ready %d\n"), task->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, info->m_pFrame->m_UID, taskIdStr, info->m_curCUToProcess[SAO_PROCESS_ID]));
        else
            DEBUG_PRINT((VM_STRING("(%d) (poc - %d) (uid - %d) %s ready %d\n"), task->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt, info->m_pFrame->m_UID, taskIdStr, task->m_pSlice->processInfo.m_curCUToProcess[readyIndex]));
    }
}

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
        printf("cur to dec - %d\n", pSlice->processInfo.m_curCUToProcess[DEC_PROCESS_ID]);
        printf("cur to rec - %d\n", pSlice->processInfo.m_curCUToProcess[REC_PROCESS_ID]);
        printf("cur to deb - %d\n", pSlice->processInfo.m_curCUToProcess[DEB_PROCESS_ID]);
        printf("dec, rec, deb vacant - %d, %d, %d\n", pSlice->processInfo.m_processInProgress[DEC_PROCESS_ID], pSlice->processInfo.m_processInProgress[REC_PROCESS_ID], pSlice->processInfo.m_processInProgress[DEB_PROCESS_ID]);
        fflush(stdout);
    }
}
#endif

TaskBroker_H265::TaskBroker_H265(TaskSupplier_H265 * pTaskSupplier)
    : m_pTaskSupplier(pTaskSupplier)
    , m_iConsumerNumber(0)
    , m_FirstAU(0)
    , m_IsShouldQuit(false)
{
    Release();
}

TaskBroker_H265::~TaskBroker_H265()
{
    Release();
}

// Initialize task broker with threads number
bool TaskBroker_H265::Init(Ipp32s iConsumerNumber)
{
    Release();

    // we keep this variable due some optimizations purposes
    m_iConsumerNumber = iConsumerNumber;
    m_IsShouldQuit = false;
    return true;
}

// Reset to default values, stop all activity
void TaskBroker_H265::Reset()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    m_FirstAU = 0;
    m_IsShouldQuit = true;

    m_decodingQueue.clear();
    m_completedQueue.clear();
}

// Release resources
void TaskBroker_H265::Release()
{
    Reset();
}

// Lock synchronization mutex
void TaskBroker_H265::Lock()
{
    m_mGuard.Lock();
}

// Uncock synchronization mutex
void TaskBroker_H265::Unlock()
{
    m_mGuard.Unlock();
}

// Check whether frame is prepared
bool TaskBroker_H265::PrepareFrame(H265DecoderFrame * pFrame)
{
    if (!pFrame || pFrame->prepared)
    {
        return true;
    }

    if (pFrame->prepared)
        return true;

    if (pFrame->GetAU()->GetStatus() == H265DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU()->GetStatus() == H265DecoderFrameInfo::STATUS_STARTED)
    {
        pFrame->prepared = true;
    }

    DEBUG_PRINT((VM_STRING("Prepare frame - %d\n"), pFrame->m_PicOrderCnt));

    return true;
}

// Add frame to decoding queue
bool TaskBroker_H265::AddFrameToDecoding(H265DecoderFrame * frame)
{
    if (!frame || frame->IsDecodingStarted() || !IsExistTasks(frame))
        return false;

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

// Remove access unit from the linked list of frames
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

// Finish frame decoding
void TaskBroker_H265::CompleteFrame(H265DecoderFrame * frame)
{
    if (!frame || m_decodingQueue.empty() || !frame->IsFullFrame())
        return;

    if (!IsFrameCompleted(frame) || frame->IsDecodingCompleted())
        return;

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
}

// Update access units list finishing completed frames
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
}

// Wakes up working threads to start working on new tasks
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
    DEBUG_PRINT((VM_STRING("Start\n")));
}

// Find an access unit which has all slices found
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

// Try to find an access unit which to decode next
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

        pPrev->SetPrevAU(0);
        pPrev->SetRefAU(0);

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

// Returns whether frame decoding is finished
bool TaskBroker_H265::IsFrameCompleted(H265DecoderFrame * pFrame) const
{
    if (!pFrame)
        return true;

    H265DecoderFrameInfo::FillnessStatus status = pFrame->GetAU()->GetStatus();

    bool ret;
    switch (status)
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

    return ret;
}

// Tries to find a new task for asynchronous processing
bool TaskBroker_H265::GetNextTask(H265Task *pTask)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

#ifndef MFX_VA
    pTask->m_taskPreparingGuard = &guard;
#endif

    bool res = GetNextTaskInternal(pTask);
#ifdef ECHO
    if (res)
        PrintTask(pTask);
#endif // ECHO

    return res;
} // bool TaskBroker_H265::GetNextTask(H265Task *pTask)

// Calculate frame state after a task has been finished
void TaskBroker_H265::AddPerformedTask(H265Task *)
{
}

#define Check_Status(status)  ((status == H265DecoderFrameInfo::STATUS_FILLED) || (status == H265DecoderFrameInfo::STATUS_STARTED))

// Returns number of access units available in the list but not processed yet
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

// Returns whether enough bitstream data is evailable to start an asynchronous task
bool TaskBroker_H265::IsEnoughForStartDecoding(bool )
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    InitAUs();
    return m_FirstAU != 0;
}

// Returns whether there is some work available for specified frame
bool TaskBroker_H265::IsExistTasks(H265DecoderFrame * frame)
{
    H265DecoderFrameInfo *slicesInfo = frame->GetAU();

    return Check_Status(slicesInfo->GetStatus());
}

#ifndef MFX_VA

TaskBrokerSingleThread_H265::TaskBrokerSingleThread_H265(TaskSupplier_H265 * pTaskSupplier)
    : TaskBroker_H265(pTaskSupplier)
{
}

// Initialize task object with default values
void TaskBrokerSingleThread_H265::InitTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)
{
    pTask->m_bError = false;
    pTask->m_iMaxMB = pSlice ? pSlice->m_iMaxMB : 0;
    pTask->m_pSlice = pSlice;
    pTask->m_pSlicesInfo = info;
    pTask->m_threadingInfo = 0;
}

// Returns whether enough bitstream data is evailable to start an asynchronous task
bool TaskBrokerSingleThread_H265::IsEnoughForStartDecoding(bool force)
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

// Get next working task
bool TaskBrokerSingleThread_H265::GetNextTaskInternal(H265Task *pTask)
{
    while (false == m_IsShouldQuit)
    {
        if (m_FirstAU)
        {
            if (GetNextSlice(m_FirstAU, pTask))
                return true;

            if (IsFrameCompleted(m_FirstAU->m_pFrame))
            {
                SwitchCurrentAU();
            }
        }

        break;
    }

    return false;
}

// Initialize frame slice and tile start coordinates
bool TaskBrokerSingleThread_H265::GetPreparationTask(H265DecoderFrameInfo * info)
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
        sliceHeader->m_sliceSegmentCurStartCUAddr = pFrame->m_CodingData->GetInverseCUOrderMap(sliceHeader->m_sliceSegmentCurStartCUAddr / numPartitions) * numPartitions;
        sliceHeader->m_sliceSegmentCurEndCUAddr = pFrame->m_CodingData->GetInverseCUOrderMap(sliceHeader->m_sliceSegmentCurEndCUAddr / numPartitions) * numPartitions;
    }

    info->FillTileInfo();

    Lock();

    info->m_prepared = 2;
    return false;
}

// Calculate frame state after a task has been finished
void TaskBrokerSingleThread_H265::AddPerformedTask(H265Task *pTask)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    H265Slice *pSlice = pTask->m_pSlice;
    H265DecoderFrameInfo * info = pTask->m_pSlicesInfo;

    CUProcessInfo * processInfo = pTask->m_threadingInfo ? &pTask->m_threadingInfo->processInfo : &pSlice->processInfo;

#if defined(ECHO)
    PrintTaskDone(pTask, info);
#endif // defined(ECHO)
    
    switch (pTask->m_iTaskID)
    {
    case TASK_PROCESS_H265: // when whole slice was processed
        {
            // slice is decoded
            processInfo->m_processInProgress[DEC_PROCESS_ID] = 0;
            processInfo->m_isCompleted = true;

            if (pTask->m_bError || pSlice->m_bError)
            {
                pSlice->m_iMaxMB = IPP_MIN(pTask->m_iMaxMB, pSlice->m_iMaxMB);
                pSlice->m_bError = true;
            }

            bool isCompleted = true;
            Ipp32s sliceCount = info->GetSliceCount();
            for (Ipp32s i = 0; i < sliceCount; i += 1)
            {
                H265Slice *pTemp = info->GetSlice(i);

                if (!pTemp->processInfo.m_isCompleted)
                    isCompleted = false;
            }

            if (isCompleted)
            {
                info->m_curCUToProcess[DEC_PROCESS_ID] = info->m_pFrame->getCD()->m_NumCUsInFrame;
                info->m_curCUToProcess[REC_PROCESS_ID] = info->m_pFrame->getCD()->m_NumCUsInFrame;
            }
        }
        break;

    case TASK_DEC_H265:
        if (pTask->m_pSlicesInfo->m_hasTiles) // tile case
        {
            TileThreadingInfo *tileInfo = pTask->m_threadingInfo;
            processInfo->m_processInProgress[DEC_PROCESS_ID] = 0;

            processInfo->m_curCUToProcess[DEC_PROCESS_ID] += pTask->m_iMBToProcess;
            pTask->m_context->m_mvsDistortion = pTask->m_context->m_mvsDistortionTemp;
            pTask->m_context->m_coeffBuffer.UnLockInputBuffer(pTask->m_WrittenSize);

            if (processInfo->m_curCUToProcess[DEC_PROCESS_ID] > tileInfo->processInfo.maxCU)
            {
                //tileInfo->isCompleted = true;
                processInfo->m_processInProgress[DEC_PROCESS_ID] = 1;
            }

            if (info->m_curCUToProcess[DEC_PROCESS_ID] < info->m_pFrame->getCD()->m_NumCUsInFrame)
            {
                // find first tile
                Ipp32u firstTileRowNumber = info->m_pFrame->getCD()->getTileIdxMap(info->m_curCUToProcess[DEC_PROCESS_ID]);

                for (;;)
                {
                    bool isCompleted = true;
                    Ipp32s currCUAddr = info->m_curCUToProcess[DEC_PROCESS_ID];
                    for (Ipp32u i = firstTileRowNumber; i < firstTileRowNumber + pSlice->GetPicParam()->num_tile_columns; i++)
                    {
                        TileThreadingInfo & tileInfo = info->m_tilesThreadingInfo[i];
                        currCUAddr += tileInfo.processInfo.m_width;

                        if (tileInfo.processInfo.m_curCUToProcess[DEC_PROCESS_ID] == info->m_tilesThreadingInfo[i].processInfo.firstCU)
                        {
                            isCompleted = false;
                            break;
                        }

                        Ipp32s tileCUCompleted = info->m_pFrame->getCD()->getCUOrderMap(tileInfo.processInfo.m_curCUToProcess[DEC_PROCESS_ID] - 1);
                        if (tileCUCompleted < currCUAddr - 1)
                        {
                            isCompleted = false;
                            break;
                        }
                    }

                    if (isCompleted)
                    {
                        info->m_curCUToProcess[DEC_PROCESS_ID] += info->m_pFrame->getCD()->m_WidthInCU;
                    }
                    else
                        break;
                        
                    if (info->m_curCUToProcess[DEC_PROCESS_ID] == info->m_pFrame->getCD()->m_NumCUsInFrame)
                        break;

                    firstTileRowNumber = info->m_pFrame->getCD()->getTileIdxMap(info->m_curCUToProcess[DEC_PROCESS_ID]);
                }
            }
        }
        else
        {
            VM_ASSERT(pTask->m_iFirstMB == processInfo->m_curCUToProcess[DEC_PROCESS_ID]);

            processInfo->m_curCUToProcess[DEC_PROCESS_ID] += pTask->m_iMBToProcess;
            pTask->m_context->m_mvsDistortion = pTask->m_context->m_mvsDistortionTemp;

            pTask->m_context->m_coeffBuffer.UnLockInputBuffer(pTask->m_WrittenSize);

            bool isReadyIncrease = (pTask->m_iFirstMB == info->m_curCUToProcess[DEC_PROCESS_ID]);
            if (isReadyIncrease)
            {
                info->m_curCUToProcess[DEC_PROCESS_ID] += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                if (isReadyIncrease)
                    info->m_curCUToProcess[DEC_PROCESS_ID] = pSlice->m_iMaxMB;

                processInfo->maxCU = processInfo->m_curCUToProcess[DEC_PROCESS_ID];
                processInfo->m_curCUToProcess[DEC_PROCESS_ID] = pSlice->m_iMaxMB;
                pSlice->m_bError = true;
            }
            else
            {
                pSlice->m_iMaxMB = pTask->m_iMaxMB;
            }

            if (processInfo->m_curCUToProcess[DEC_PROCESS_ID] >= processInfo->maxCU)
            {
                processInfo->m_processInProgress[DEC_PROCESS_ID] = 1; // completed
                if (isReadyIncrease)
                {
                    Ipp32s pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    if (pos >= 0)
                    {
                        H265Slice * pNextSlice = info->GetSlice(++pos);
                        for (; pNextSlice; pNextSlice = info->GetSlice(++pos))
                        {
                            info->m_curCUToProcess[DEC_PROCESS_ID] = pNextSlice->processInfo.m_curCUToProcess[DEC_PROCESS_ID];
                            if (pNextSlice->processInfo.m_curCUToProcess[DEC_PROCESS_ID] < pNextSlice->m_iMaxMB)
                                break;
                        }
                    }
                }
            }
            else
            {
                processInfo->m_processInProgress[DEC_PROCESS_ID] = 0;
            }
        }
        break;

    case TASK_REC_H265:
        if (pTask->m_pSlicesInfo->m_hasTiles) // tile case
        {
            TileThreadingInfo *tileInfo = pTask->m_threadingInfo;
            processInfo->m_processInProgress[REC_PROCESS_ID] = 0;

            processInfo->m_curCUToProcess[REC_PROCESS_ID] += pTask->m_iMBToProcess;

            if (processInfo->m_curCUToProcess[REC_PROCESS_ID] > tileInfo->processInfo.maxCU)
            {
                processInfo->m_isCompleted = true;
                processInfo->m_processInProgress[REC_PROCESS_ID] = 1;
            }

            pTask->m_context->m_coeffBuffer.UnLockOutputBuffer();

            // find first tile
            if (info->m_curCUToProcess[REC_PROCESS_ID] < info->m_pFrame->getCD()->m_NumCUsInFrame)
            {
                Ipp32u firstTileRowNumber = info->m_pFrame->getCD()->getTileIdxMap(info->m_curCUToProcess[REC_PROCESS_ID]);

                for (;;)
                {
                    bool isCompleted = true;
                    Ipp32s currCUAddr = info->m_curCUToProcess[REC_PROCESS_ID];
                    for (Ipp32u i = firstTileRowNumber; i < firstTileRowNumber + pSlice->GetPicParam()->num_tile_columns; i++)
                    {
                        currCUAddr += info->m_tilesThreadingInfo[i].processInfo.m_width;

                        if (info->m_tilesThreadingInfo[i].processInfo.m_curCUToProcess[REC_PROCESS_ID] == info->m_tilesThreadingInfo[i].processInfo.firstCU)
                        {
                            isCompleted = false;
                            break;
                        }

                        Ipp32s tileCUCompleted = info->m_pFrame->getCD()->getCUOrderMap(info->m_tilesThreadingInfo[i].processInfo.m_curCUToProcess[REC_PROCESS_ID] - 1);
                        if (tileCUCompleted < currCUAddr - 1)
                        {
                            isCompleted = false;
                            break;
                        }
                    }

                    if (isCompleted)
                    {
                        info->m_curCUToProcess[REC_PROCESS_ID] += info->m_pFrame->getCD()->m_WidthInCU;
                    }
                    else
                        break;

                    if (info->m_curCUToProcess[REC_PROCESS_ID] == info->m_pFrame->getCD()->m_NumCUsInFrame)
                        break;

                    firstTileRowNumber = info->m_pFrame->getCD()->getTileIdxMap(info->m_curCUToProcess[REC_PROCESS_ID]);
                }
            }
        }
        else
        {
            VM_ASSERT(pTask->m_iFirstMB == processInfo->m_curCUToProcess[REC_PROCESS_ID]);

            processInfo->m_curCUToProcess[REC_PROCESS_ID] += pTask->m_iMBToProcess;

            bool isReadyIncrease = pTask->m_iFirstMB == info->m_curCUToProcess[REC_PROCESS_ID];

            if (isReadyIncrease)
                info->m_curCUToProcess[REC_PROCESS_ID] += pTask->m_iMBToProcess;

            pTask->m_context->m_coeffBuffer.UnLockOutputBuffer();

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                if (isReadyIncrease)
                {
                    info->m_curCUToProcess[REC_PROCESS_ID] = pSlice->m_iMaxMB;
                }

                processInfo->m_curCUToProcess[REC_PROCESS_ID] = pSlice->m_iMaxMB;
                pSlice->m_bError = true;
            }

            if (processInfo->maxCU <= processInfo->m_curCUToProcess[REC_PROCESS_ID])
            {
                if (isReadyIncrease)
                {
                    Ipp32s pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    if (pos >= 0)
                    {
                        H265Slice * pNextSlice = info->GetSlice(++pos);
                        for (; pNextSlice; pNextSlice = info->GetSlice(++pos))
                        {
                            info->m_curCUToProcess[REC_PROCESS_ID] = pNextSlice->processInfo.m_curCUToProcess[REC_PROCESS_ID];
                            if (pNextSlice->processInfo.m_curCUToProcess[REC_PROCESS_ID] < pNextSlice->processInfo.maxCU)
                                break;
                        }
                    }
                }

                processInfo->m_processInProgress[REC_PROCESS_ID] = 1; // completed
                processInfo->m_isCompleted = true;
            }
            else
            {
                processInfo->m_processInProgress[REC_PROCESS_ID] = 0;
            }
        }
        break;

    case TASK_DEC_REC_H265:
        if (pTask->m_pSlicesInfo->m_hasTiles) // tile case
        {
            TileThreadingInfo *tileInfo = pTask->m_threadingInfo;
            processInfo->m_processInProgress[DEC_PROCESS_ID] = 0;
            processInfo->m_processInProgress[REC_PROCESS_ID] = 0;

            processInfo->m_curCUToProcess[DEC_PROCESS_ID] += pTask->m_iMBToProcess;
            processInfo->m_curCUToProcess[REC_PROCESS_ID] += pTask->m_iMBToProcess;

            if (processInfo->m_curCUToProcess[DEC_PROCESS_ID] > tileInfo->processInfo.maxCU)
            {
                processInfo->m_isCompleted = true;
                processInfo->m_processInProgress[DEC_PROCESS_ID] = 1;
                processInfo->m_processInProgress[REC_PROCESS_ID] = 1;
            }

            // find first tile
            Ipp32u firstTileRowNumber = info->m_pFrame->getCD()->getTileIdxMap(info->m_curCUToProcess[DEC_PROCESS_ID]);

            for (;;)
            {

                bool isCompleted = true;
                Ipp32s currCUAddr = info->m_curCUToProcess[DEC_PROCESS_ID];
                for (Ipp32u i = firstTileRowNumber; i < firstTileRowNumber + pSlice->GetPicParam()->num_tile_columns; i++)
                {
                    TileThreadingInfo & tileInfo = info->m_tilesThreadingInfo[i];
                    currCUAddr += tileInfo.processInfo.m_width;

                    if (tileInfo.processInfo.m_curCUToProcess[DEC_PROCESS_ID] == tileInfo.processInfo.firstCU)
                    {
                        isCompleted = false;
                        break;
                    }

                    Ipp32s tileCUCompleted = info->m_pFrame->getCD()->getCUOrderMap(tileInfo.processInfo.m_curCUToProcess[DEC_PROCESS_ID] - 1);
                    if (tileCUCompleted < currCUAddr - 1)
                    {
                        isCompleted = false;
                        break;
                    }
                }

                if (isCompleted)
                {
                    info->m_curCUToProcess[DEC_PROCESS_ID] += info->m_pFrame->getCD()->m_WidthInCU;
                }
                else
                    break;

                if (info->m_curCUToProcess[DEC_PROCESS_ID] == info->m_pFrame->getCD()->m_NumCUsInFrame)
                    break;

                firstTileRowNumber = info->m_pFrame->getCD()->getTileIdxMap(info->m_curCUToProcess[DEC_PROCESS_ID]);
            }

            // find first tile
            firstTileRowNumber = info->m_pFrame->getCD()->getTileIdxMap(info->m_curCUToProcess[REC_PROCESS_ID]);

            for (;;)
            {
                bool isCompleted = true;
                Ipp32s currCUAddr = info->m_curCUToProcess[REC_PROCESS_ID];
                for (Ipp32u i = firstTileRowNumber; i < firstTileRowNumber + pSlice->GetPicParam()->num_tile_columns; i++)
                {
                    TileThreadingInfo & tileInfo = info->m_tilesThreadingInfo[i];
                    currCUAddr += tileInfo.processInfo.m_width;

                    if (tileInfo.processInfo.m_curCUToProcess[REC_PROCESS_ID] == tileInfo.processInfo.firstCU)
                    {
                        isCompleted = false;
                        break;
                    }

                    Ipp32s tileCUCompleted = info->m_pFrame->getCD()->getCUOrderMap(tileInfo.processInfo.m_curCUToProcess[REC_PROCESS_ID] - 1);
                    if (tileCUCompleted < currCUAddr - 1)
                    {
                        isCompleted = false;
                        break;
                    }
                }

                if (isCompleted)
                {
                    info->m_curCUToProcess[REC_PROCESS_ID] += info->m_pFrame->getCD()->m_WidthInCU;
                }
                else
                    break;

                if (info->m_curCUToProcess[REC_PROCESS_ID] == info->m_pFrame->getCD()->m_NumCUsInFrame)
                    break;

                firstTileRowNumber = info->m_pFrame->getCD()->getTileIdxMap(info->m_curCUToProcess[REC_PROCESS_ID]);
            }
        }
        else
        {
            VM_ASSERT(pTask->m_iFirstMB == processInfo->m_curCUToProcess[DEC_PROCESS_ID]);
            VM_ASSERT(pTask->m_iFirstMB == processInfo->m_curCUToProcess[REC_PROCESS_ID]);

            processInfo->m_curCUToProcess[DEC_PROCESS_ID] += pTask->m_iMBToProcess;
            processInfo->m_curCUToProcess[REC_PROCESS_ID] += pTask->m_iMBToProcess;

            bool isReadyIncreaseDec = pTask->m_iFirstMB == info->m_curCUToProcess[DEC_PROCESS_ID];
            bool isReadyIncreaseRec = pTask->m_iFirstMB == info->m_curCUToProcess[REC_PROCESS_ID];

            if (isReadyIncreaseDec)
                info->m_curCUToProcess[DEC_PROCESS_ID] += pTask->m_iMBToProcess;

            if (isReadyIncreaseRec)
            {
                info->m_curCUToProcess[REC_PROCESS_ID] += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                if (isReadyIncreaseDec)
                    info->m_curCUToProcess[DEC_PROCESS_ID] = pSlice->m_iMaxMB;

                if (isReadyIncreaseRec)
                    info->m_curCUToProcess[REC_PROCESS_ID] = pSlice->m_iMaxMB;

                processInfo->maxCU = processInfo->m_curCUToProcess[DEC_PROCESS_ID];
                processInfo->m_curCUToProcess[REC_PROCESS_ID] = processInfo->m_curCUToProcess[DEC_PROCESS_ID] = pSlice->m_iMaxMB;
                processInfo->m_isCompleted = true;
                pSlice->m_bError = true;
            }
            else
            {
                pSlice->m_iMaxMB = pTask->m_iMaxMB;
            }

            if (processInfo->m_curCUToProcess[DEC_PROCESS_ID] >= pSlice->m_iMaxMB)
            {
                VM_ASSERT(processInfo->m_curCUToProcess[REC_PROCESS_ID] >= pSlice->m_iMaxMB);

                processInfo->m_isCompleted = true;

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
                                info->m_curCUToProcess[DEC_PROCESS_ID] = pTmpSlice->processInfo.m_curCUToProcess[DEC_PROCESS_ID];
                                if (pTmpSlice->processInfo.m_curCUToProcess[DEC_PROCESS_ID] < pTmpSlice->m_iMaxMB)
                                    break;
                            }
                        }

                        if (isReadyIncreaseRec)
                        {
                            Ipp32s pos1 = pos;
                            H265Slice * pTmpSlice = info->GetSlice(++pos1);

                            for (; pTmpSlice; pTmpSlice = info->GetSlice(++pos1))
                            {
                                info->m_curCUToProcess[REC_PROCESS_ID] = pTmpSlice->processInfo.m_curCUToProcess[REC_PROCESS_ID];
                                if (pTmpSlice->processInfo.m_curCUToProcess[REC_PROCESS_ID] < pTmpSlice->m_iMaxMB)
                                    break;
                            }
                        }
                    }
                }

                processInfo->m_processInProgress[DEC_PROCESS_ID] = 1; // completed
                processInfo->m_processInProgress[REC_PROCESS_ID] = 1;
            }
            else
            {
                processInfo->m_processInProgress[DEC_PROCESS_ID] = 0;
                processInfo->m_processInProgress[REC_PROCESS_ID] = 0;
            }
        }
        break;

    case TASK_DEB_H265:
        {
        if (pTask->m_pSlicesInfo->m_hasTiles || info->m_processInProgress[DEB_PROCESS_ID]) // tile case or single thread frame's deblocking
        {
            info->m_processInProgress[DEB_PROCESS_ID] = 0;
            info->m_curCUToProcess[DEB_PROCESS_ID] += pTask->m_iMBToProcess;
        }
        else
        {
            processInfo->m_curCUToProcess[DEB_PROCESS_ID] += pTask->m_iMBToProcess;
            info->m_processInProgress[DEB_PROCESS_ID] = 0;

            bool isReadyIncrease = pTask->m_iFirstMB == info->m_curCUToProcess[DEB_PROCESS_ID];
            if (isReadyIncrease)
            {
                info->m_curCUToProcess[DEB_PROCESS_ID] += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                processInfo->m_curCUToProcess[DEB_PROCESS_ID] = pSlice->m_iMaxMB;
                if (isReadyIncrease)
                {
                    info->m_curCUToProcess[DEB_PROCESS_ID] = pSlice->m_iMaxMB;
                }
                pSlice->m_bError = true;
            }

            if (pSlice->m_iMaxMB <= processInfo->m_curCUToProcess[DEB_PROCESS_ID])
            {
                Ipp32s pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                if (isReadyIncrease && pos >= 0)
                {
                    H265Slice * pNextSlice = info->GetSlice(++pos);
                    for (; pNextSlice; pNextSlice = info->GetSlice(++pos))
                    {
                        info->m_curCUToProcess[DEB_PROCESS_ID] = pNextSlice->processInfo.m_curCUToProcess[DEB_PROCESS_ID];
                        if (pNextSlice->processInfo.m_curCUToProcess[DEB_PROCESS_ID] < pNextSlice->m_iMaxMB)
                            break;
                    }
                }
            }
            else
            {
                processInfo->m_processInProgress[DEB_PROCESS_ID] = 0;
            }
        }
        }
        break;
    case TASK_SAO_H265:
        {
            VM_ASSERT(pTask->m_iFirstMB == info->m_curCUToProcess[SAO_PROCESS_ID]);
            info->m_curCUToProcess[SAO_PROCESS_ID] += pTask->m_iMBToProcess;

            if (info->m_curCUToProcess[SAO_PROCESS_ID] < info->m_pFrame->getCD()->m_NumCUsInFrame)
                info->m_processInProgress[SAO_PROCESS_ID] = 0;
        }
        break;
    default:
        break;
    }

#if defined(ECHO)
    PrintTaskDoneAdditional(pTask, info);
#endif

}

// Tries to find a portion of decoding+reconstruction work in the specified access unit and initializes a task object for it
bool TaskBrokerSingleThread_H265::GetNextSliceToDecoding(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);

        if (!pSlice->processInfo.m_processInProgress[DEC_PROCESS_ID] && !pSlice->processInfo.m_isCompleted)
        {
            InitTask(info, pTask, pSlice);
            pTask->m_iFirstMB = pSlice->m_iFirstMB;
            pTask->m_iMBToProcess = pSlice->m_iMaxMB - pSlice->m_iFirstMB;
            pTask->m_iTaskID = TASK_PROCESS_H265;
            pTask->m_pBuffer = 0;
            pSlice->processInfo.m_processInProgress[DEC_PROCESS_ID] = 1;

#ifdef ECHO
            DEBUG_PRINT((VM_STRING("(%d) (%d) slice dec - % 4d to % 4d\n"), pTask->m_iThreadNumber, pSlice->m_pCurrentFrame->m_PicOrderCnt, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

            return true;
        }
    }

    return false;

} // bool TaskBroker_H265::GetNextSliceToDecoding(H265Task *pTask)

// Tries to find a portion of deblocking filtering work in the specified access unit and initializes a task object for it
bool TaskBrokerSingleThread_H265::GetNextSliceToDeblocking(H265DecoderFrameInfo * info, H265Task *pTask)
{
    if (!info->IsNeedDeblocking() || info->m_processInProgress[DEB_PROCESS_ID])
        return false;

    Ipp32s iMBWidth = info->m_pFrame->getCD()->m_WidthInCU;
    Ipp32s availableToProcess = info->m_curCUToProcess[REC_PROCESS_ID] - info->m_curCUToProcess[DEB_PROCESS_ID];

    if (availableToProcess > iMBWidth || (availableToProcess && info->m_curCUToProcess[REC_PROCESS_ID] >= info->m_pFrame->getCD()->m_NumCUsInFrame))
    {
        info->m_processInProgress[DEB_PROCESS_ID] = 1;

        pTask->m_taskPreparingGuard->Unlock();
        InitTask(info, pTask, info->GetAnySlice());
        pTask->m_iFirstMB = info->m_curCUToProcess[DEB_PROCESS_ID];
        pTask->m_iMBToProcess = IPP_MIN(iMBWidth - (info->m_curCUToProcess[DEB_PROCESS_ID] % iMBWidth), availableToProcess);

        pTask->m_iTaskID = TASK_DEB_H265;

#ifdef ECHO_DEB
        PrintTask(pTask);
#endif // ECHO_DEB

        return true;
    }

    return false;

} // bool TaskBroker_H265::GetNextSliceToDeblocking(H265Task *pTask)

// Tries to find a new slice to work on in specified access unit and initializes a task onject for it
bool TaskBrokerSingleThread_H265::GetNextSlice(H265DecoderFrameInfo * info, H265Task *pTask)
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

    if (GetNextSliceToDeblocking(info, pTask))
        return true;

    return GetSAOFrameTask(info, pTask);
} // bool TaskBroker_H265::GetNextSlice(H265Task *pTask)

// Tries to find a portion of SAO filtering work in the specified access unit and initializes a task object for it
bool TaskBrokerSingleThread_H265::GetSAOFrameTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    if (!info->IsNeedSAO() || info->m_processInProgress[SAO_PROCESS_ID])
        return false;

    Ipp32s iMBWidth = info->m_pFrame->getCD()->m_WidthInCU;
    Ipp32s prevProcessMBReady = info->IsNeedDeblocking() ? DEB_PROCESS_ID : REC_PROCESS_ID;
    Ipp32s availableToProcess = info->m_curCUToProcess[prevProcessMBReady] - info->m_curCUToProcess[SAO_PROCESS_ID];

    if (availableToProcess > iMBWidth || (availableToProcess && info->m_curCUToProcess[prevProcessMBReady] >= info->m_pFrame->getCD()->m_NumCUsInFrame))
    {
        info->m_processInProgress[SAO_PROCESS_ID] = 1;

        InitTask(info, pTask, info->GetAnySlice());
        pTask->m_iFirstMB = info->m_curCUToProcess[SAO_PROCESS_ID];
        pTask->m_iMBToProcess = IPP_MIN(iMBWidth - (info->m_curCUToProcess[SAO_PROCESS_ID] % iMBWidth), availableToProcess);

        pTask->m_iTaskID = TASK_SAO_H265;

#ifdef ECHO_DEB
        PrintTask(pTask);
#endif // ECHO_DEB

        return true;
    }

    return false;
}

TaskBrokerTwoThread_H265::TaskBrokerTwoThread_H265(TaskSupplier_H265 * pTaskSupplier)
    : TaskBrokerSingleThread_H265(pTaskSupplier)
{
}

bool TaskBrokerTwoThread_H265::Init(Ipp32s iConsumerNumber)
{
    if (!TaskBroker_H265::Init(iConsumerNumber))
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

// Update access units list finishing completed frames
void TaskBrokerTwoThread_H265::CompleteFrame()
{
    if (!m_FirstAU)
        return;

    DEBUG_PRINT((VM_STRING("frame completed - poc - %d\n"), m_FirstAU->m_pFrame->m_PicOrderCnt));
    SwitchCurrentAU();
}

// Try to find a new task and initialize a task object for it
bool TaskBrokerTwoThread_H265::GetNextTaskInternal(H265Task *pTask)
{
    if (m_IsShouldQuit)
        return false;

#if 0 // for mt debug
    H265DecoderFrameInfo * prev = m_FirstAU;
    for (H265DecoderFrameInfo *info = m_FirstAU; info; info = info->GetNextAU())
    {
        prev = info;
    }

    if (prev)
    {
        for (H265DecoderFrameInfo *info = prev; info; info = info->GetPrevAU())
        {
            if (GetNextTaskManySlices(info, pTask))
                return true;
        }

    }
#else
    for (H265DecoderFrameInfo *info = m_FirstAU; info; info = info->GetNextAU())
    {
        if (GetNextTaskManySlices(info, pTask))
            return true;
    }
#endif

    return false;
}

// Initialize CTB range for decoding task
bool TaskBrokerTwoThread_H265::WrapDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)
{
    VM_ASSERT(pSlice);

    CUProcessInfo * processInfo = &pSlice->processInfo;

    // this is guarded function, safe to touch any variable
    if (processInfo->m_processInProgress[DEC_PROCESS_ID])
        return false;

    InitTask(info, pTask, pSlice);
    pTask->m_iTaskID = TASK_DEC_H265;

    if (!GetResources(pTask))
        return false;

    pSlice->processInfo.m_processInProgress[DEC_PROCESS_ID] = 1;

    //Ipp32s firstAdujstedCUAddr = pTask->m_iFirstMB - tileToWrap->firstCUAddr;

    Ipp32s width = processInfo->m_width;
    pTask->m_iFirstMB = processInfo->m_curCUToProcess[DEC_PROCESS_ID];
    pTask->m_iMBToProcess = IPP_MIN(processInfo->m_curCUToProcess[DEC_PROCESS_ID] -
                                    (processInfo->m_curCUToProcess[DEC_PROCESS_ID] % width) +
                                    width,
                                    pSlice->m_iMaxMB) - processInfo->m_curCUToProcess[DEC_PROCESS_ID];

    pTask->m_taskPreparingGuard->Unlock();
    return true;

} // bool TaskBrokerTwoThread_H265::WrapDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)

// Initialize CTB range for reconstruct task
bool TaskBrokerTwoThread_H265::WrapReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)
{
    VM_ASSERT(pSlice);
    // this is guarded function, safe to touch any variable
    CUProcessInfo * processInfo = &pSlice->processInfo;
    if (processInfo->m_processInProgress[REC_PROCESS_ID])
        return false;

    pTask->m_iTaskID = TASK_REC_H265;
    InitTask(info, pTask, pSlice);

    if (!GetResources(pTask))
        return false;

    processInfo->m_processInProgress[REC_PROCESS_ID] = 1;

    Ipp32s width = processInfo->m_width;
    pTask->m_iFirstMB = processInfo->m_curCUToProcess[REC_PROCESS_ID];
    pTask->m_iMBToProcess = IPP_MIN(processInfo->m_curCUToProcess[REC_PROCESS_ID] - (processInfo->m_curCUToProcess[REC_PROCESS_ID] % width) + width,
                                    processInfo->maxCU) - processInfo->m_curCUToProcess[REC_PROCESS_ID];

    pTask->m_taskPreparingGuard->Unlock();
    return true;

} // bool TaskBrokerTwoThread_H265::WrapReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)

// Initialize CTB range for decoding+reconstruct task
bool TaskBrokerTwoThread_H265::WrapDecRecTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice)
{
    VM_ASSERT(pSlice);

    // this is guarded function, safe to touch any variable
    CUProcessInfo * processInfo = &pSlice->processInfo;

    if (processInfo->m_processInProgress[DEC_PROCESS_ID] || processInfo->m_processInProgress[REC_PROCESS_ID] ||
        processInfo->m_curCUToProcess[DEC_PROCESS_ID] != processInfo->m_curCUToProcess[REC_PROCESS_ID])
        return false;

    InitTask(info, pTask, pSlice);
    pTask->m_iTaskID = TASK_DEC_REC_H265;

    if (!GetResources(pTask))
        return false;

    processInfo->m_processInProgress[DEC_PROCESS_ID] = 1;
    processInfo->m_processInProgress[REC_PROCESS_ID] = 1;

    Ipp32s width = processInfo->m_width;
    pTask->m_iFirstMB = processInfo->m_curCUToProcess[DEC_PROCESS_ID];
    pTask->m_iMBToProcess = IPP_MIN(pTask->m_iFirstMB -
                                    (pTask->m_iFirstMB % width) +
                                    width,
                                    pSlice->m_iMaxMB) - pTask->m_iFirstMB;

    pTask->m_taskPreparingGuard->Unlock();
    return true;
}

// Initialize decoding+reconstruct task object
bool TaskBrokerTwoThread_H265::GetDecRecTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    if (info->GetRefAU() || info->GetSliceCount() == 1)
        return false;

    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; ++i)
    {
        H265Slice *pSlice = info->GetSlice(i);
        CUProcessInfo * processInfo = &pSlice->processInfo;

        if (processInfo->m_processInProgress[REC_PROCESS_ID] || processInfo->m_processInProgress[DEC_PROCESS_ID])
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

// Initialize tile decoding task
bool TaskBrokerTwoThread_H265::GetDecodingTileTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    H265DecoderFrameInfo * refAU = info->GetRefAU();
    if (refAU != 0 && refAU->GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
    {
        refAU = 0;
    }

    TileThreadingInfo * tileToWrap = FindTileForProcess(info, DEC_PROCESS_ID);
    if (!tileToWrap)
        return false;

    // wrap task
    CUProcessInfo * processInfo = &tileToWrap->processInfo;
    H265Slice *slice = FindSliceByCUAddr(info, processInfo->m_curCUToProcess[DEC_PROCESS_ID]);

    if (refAU != 0)// && slice->GetSliceHeader()->slice_temporal_mvp_enabled_flag)
    {
        // if slice_temporal_mvp_enabled_flag equals zero we do not depend from collocated mv info
        Ipp32s rasterOrderCUAddr = info->m_pFrame->getCD()->getCUOrderMap(processInfo->m_curCUToProcess[DEC_PROCESS_ID]);
        if (rasterOrderCUAddr + (Ipp32s)info->m_pFrame->getCD()->m_WidthInCU >= refAU->m_curCUToProcess[DEC_PROCESS_ID])
            return false;
    }

    VM_ASSERT(slice);

    InitTask(info, pTask, slice);
    pTask->m_iTaskID = TASK_DEC_H265;
    pTask->m_threadingInfo = tileToWrap;
    pTask->m_iFirstMB = processInfo->m_curCUToProcess[DEC_PROCESS_ID];

    if (!GetResources(pTask))
        return false;

    processInfo->m_processInProgress[DEC_PROCESS_ID] = 1;

    Ipp32s firstAdujstedCUAddr = pTask->m_iFirstMB - tileToWrap->processInfo.firstCU;

    pTask->m_iMBToProcess = IPP_MIN(firstAdujstedCUAddr - (firstAdujstedCUAddr % processInfo->m_width) + processInfo->m_width,
                            slice->m_iMaxMB - tileToWrap->processInfo.firstCU) - firstAdujstedCUAddr;
    VM_ASSERT(pTask->m_iMBToProcess <= processInfo->m_width);

    pTask->m_taskPreparingGuard->Unlock();
    return true;
}

// Initialize tile reconstruct task
bool TaskBrokerTwoThread_H265::GetReconstructTileTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    H265DecoderFrameInfo * refAU = info->GetRefAU();

    if (refAU != 0 && refAU->GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
        refAU = 0;

    TileThreadingInfo * tileToWrap = FindTileForProcess(info, REC_PROCESS_ID);
    if (!tileToWrap)
        return false;

    // wrap task
    CUProcessInfo * processInfo = &tileToWrap->processInfo;    
    H265Slice *slice = FindSliceByCUAddr(info, processInfo->m_curCUToProcess[REC_PROCESS_ID]);
    VM_ASSERT(slice);

    VM_ASSERT(tileToWrap->m_context);
    if (refAU != 0)
    {
        Ipp32u readyRefCount = IPP_MIN(refAU->m_curCUToProcess[REC_PROCESS_ID], IPP_MIN(refAU->m_curCUToProcess[DEB_PROCESS_ID], refAU->m_curCUToProcess[SAO_PROCESS_ID]));
        Ipp32s rasterOrderCUAddr = info->m_pFrame->getCD()->getCUOrderMap(processInfo->m_curCUToProcess[REC_PROCESS_ID]);
        Ipp32s cuSize = info->m_pFrame->getCD()->m_MaxCUWidth;
        Ipp32s distortion = tileToWrap->m_context->m_mvsDistortion + (refAU->IsNeedDeblocking() ? 4 : 0) + 8;
        Ipp32s k = ( (distortion + cuSize - 1) / cuSize) + 1; // +2 - (1 - for padding, 2 - current line)
        if (rasterOrderCUAddr + k*info->m_pFrame->getCD()->m_WidthInCU >= readyRefCount)
            return false;
    }

    InitTask(info, pTask, slice);
    pTask->m_iTaskID = TASK_REC_H265;
    pTask->m_threadingInfo = tileToWrap;
    pTask->m_iFirstMB = processInfo->m_curCUToProcess[REC_PROCESS_ID];

    if (!GetResources(pTask))
        return false;

    processInfo->m_processInProgress[REC_PROCESS_ID] = 1;

    Ipp32s firstAdujstedCUAddr = pTask->m_iFirstMB - tileToWrap->processInfo.firstCU;
    pTask->m_iMBToProcess = IPP_MIN(firstAdujstedCUAddr - (firstAdujstedCUAddr % processInfo->m_width) + processInfo->m_width,
                            slice->m_iMaxMB - tileToWrap->processInfo.firstCU) - firstAdujstedCUAddr;
    VM_ASSERT(pTask->m_iMBToProcess <= processInfo->m_width);

    pTask->m_taskPreparingGuard->Unlock();
    return true;
}

// Initialize tile decoding+reconstruct task
bool TaskBrokerTwoThread_H265::GetDecRecTileTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    if (info->GetRefAU())
        return false;

    TileThreadingInfo * tileToWrap = FindTileForProcess(info, DEC_PROCESS_ID, true);
    if (!tileToWrap)
        return false;

    // wrap task
    CUProcessInfo * processInfo = &tileToWrap->processInfo;

    H265Slice *slice = FindSliceByCUAddr(info, processInfo->m_curCUToProcess[DEC_PROCESS_ID]);
    VM_ASSERT(slice);

    InitTask(info, pTask, slice);
    pTask->m_iTaskID = TASK_DEC_REC_H265;
    pTask->m_threadingInfo = tileToWrap;
    pTask->m_iFirstMB = processInfo->m_curCUToProcess[DEC_PROCESS_ID];

    if (!GetResources(pTask))
        return false;

    processInfo->m_processInProgress[DEC_PROCESS_ID] = 1;
    processInfo->m_processInProgress[REC_PROCESS_ID] = 1;

    Ipp32s firstAdujstedCUAddr = pTask->m_iFirstMB - tileToWrap->processInfo.firstCU;
    pTask->m_iMBToProcess = IPP_MIN(firstAdujstedCUAddr - (firstAdujstedCUAddr % processInfo->m_width) + processInfo->m_width,
                            slice->m_iMaxMB - tileToWrap->processInfo.firstCU) - firstAdujstedCUAddr;
    VM_ASSERT(pTask->m_iMBToProcess <= processInfo->m_width);
    pTask->m_taskPreparingGuard->Unlock();
    return true;
}

// Initialize decoding task object
bool TaskBrokerTwoThread_H265::GetDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    H265DecoderFrameInfo * refAU = info->GetRefAU();
    if (refAU != 0 && refAU->GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
        refAU = 0;

    // this is guarded function, safe to touch   any variable
    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);
        CUProcessInfo * processInfo = &pSlice->processInfo;

        if (processInfo->m_processInProgress[DEC_PROCESS_ID])
            continue;

        if (refAU != 0)// && pSlice->GetSliceHeader()->slice_temporal_mvp_enabled_flag)
        {
            if (processInfo->m_curCUToProcess[DEC_PROCESS_ID] + (Ipp32s)info->m_pFrame->getCD()->m_WidthInCU >= refAU->m_curCUToProcess[DEC_PROCESS_ID])
                break;
        }

        if (WrapDecodingTask(info, pTask, pSlice))
            return true;
    }

    return false;

} // bool TaskBrokerTwoThread_H265::GetDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask)

// Initialize reconstruct task object
bool TaskBrokerTwoThread_H265::GetReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    H265DecoderFrameInfo * refAU = info->GetRefAU();

    if (refAU != 0 && refAU->GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
        refAU = 0;

    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);
        CUProcessInfo * processInfo = &pSlice->processInfo;

        if (processInfo->m_processInProgress[REC_PROCESS_ID] || processInfo->m_curCUToProcess[REC_PROCESS_ID] >= processInfo->m_curCUToProcess[DEC_PROCESS_ID])
            continue;

        if (refAU != 0)
        {
            Ipp32s readyRefCount = IPP_MIN(refAU->m_curCUToProcess[REC_PROCESS_ID], IPP_MIN(refAU->m_curCUToProcess[DEB_PROCESS_ID], refAU->m_curCUToProcess[SAO_PROCESS_ID]));
            Ipp32s cuSize = info->m_pFrame->getCD()->m_MaxCUWidth;
            Ipp32s distortion = pSlice->m_context->m_mvsDistortion + (refAU->IsNeedDeblocking() ? 4 : 0) + 8;
            Ipp32s k = ( (distortion + cuSize - 1) / cuSize) + 1; // +2 - (1 - for padding, 2 - current line)
            if (processInfo->m_curCUToProcess[REC_PROCESS_ID] + k*(Ipp32s)info->m_pFrame->getCD()->m_WidthInCU >= readyRefCount)
                break;
        }

        if (WrapReconstructTask(info, pTask, pSlice))
        {
            return true;
        }
    }

    return false;

} // bool TaskBrokerTwoThread_H265::GetReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask)

// Initialize deblocking task object
bool TaskBrokerTwoThread_H265::GetDeblockingTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    if (!info->IsNeedDeblocking())
        return false;

    // this is guarded function, safe to touch any variable
    bool bPrevDeblocked = true;

    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; i += 1)
    {
        H265Slice *pSlice = info->GetSlice(i);
        CUProcessInfo * processInfo = &pSlice->processInfo;

        Ipp32s iMBWidth = info->m_pFrame->getCD()->m_WidthInCU;
        Ipp32s iAvailableToDeblock = processInfo->m_curCUToProcess[REC_PROCESS_ID] - processInfo->m_curCUToProcess[DEB_PROCESS_ID];

        if ((bPrevDeblocked || !pSlice->GetSliceHeader()->slice_loop_filter_across_slices_enabled_flag) &&
            !processInfo->m_processInProgress[DEB_PROCESS_ID] &&
            (iAvailableToDeblock > iMBWidth || processInfo->m_isCompleted))
        {
            processInfo->m_processInProgress[DEB_PROCESS_ID] = 1;

            pTask->m_taskPreparingGuard->Unlock();
            InitTask(info, pTask, pSlice);
            pTask->m_iFirstMB = processInfo->m_curCUToProcess[DEB_PROCESS_ID];
            pTask->m_iMBToProcess = IPP_MIN(iMBWidth - (processInfo->m_curCUToProcess[DEB_PROCESS_ID] % iMBWidth), iAvailableToDeblock);

            pTask->m_iTaskID = TASK_DEB_H265;
            return true;
        }

        if (processInfo->m_curCUToProcess[DEB_PROCESS_ID] < pSlice->m_iMaxMB || !processInfo->m_isCompleted)
        {
            break;
        }
    }

    return false;

} // bool TaskBrokerTwoThread_H265::GetDeblockingTask(H265Task *pTask)

// Initialize SAO task object
bool TaskBrokerTwoThread_H265::GetSAOTask(H265DecoderFrameInfo * info, H265Task *pTask)
{
    if (!info->IsNeedSAO() || info->m_processInProgress[SAO_PROCESS_ID])
        return false;

    // this is guarded function, safe to touch any variable
    Ipp32s iMBWidth = info->m_pFrame->getCD()->m_WidthInCU;
    Ipp32s nextMBtoSAO = IPP_MIN(iMBWidth * (info->m_curCUToProcess[SAO_PROCESS_ID] / iMBWidth) + 2*iMBWidth, info->m_pFrame->getCD()->m_NumCUsInFrame);
    Ipp32s sliceCount = info->GetSliceCount();

    for (Ipp32s i = 0; i < sliceCount; i += 1)
    {
        H265Slice *slice = info->GetSlice(i);

        if (slice->m_iMaxMB < info->m_curCUToProcess[SAO_PROCESS_ID])
            continue;

        if (slice->m_iFirstMB > nextMBtoSAO)
            break;

        Ipp32s prevCUAddrReady = IPP_MIN(info->m_curCUToProcess[REC_PROCESS_ID], info->m_curCUToProcess[DEB_PROCESS_ID]);

        if (prevCUAddrReady <= nextMBtoSAO && prevCUAddrReady < slice->m_iMaxMB)
            return false;
    }

    H265Slice *slice = info->GetSlice(0);

    info->m_processInProgress[SAO_PROCESS_ID] = 1;
    pTask->m_taskPreparingGuard->Unlock();
    InitTask(info, pTask, slice);
    pTask->m_iFirstMB = info->m_curCUToProcess[SAO_PROCESS_ID];
    pTask->m_iMBToProcess = iMBWidth;

    pTask->m_iTaskID = TASK_SAO_H265;
    return true;
}

// Initialize tile deblocking task object
bool TaskBrokerTwoThread_H265::GetDeblockingTaskTile(H265DecoderFrameInfo * info, H265Task *pTask)
{
    // this is guarded function, safe to touch any variable
    if (!info->IsNeedDeblocking() || info->m_processInProgress[DEB_PROCESS_ID])
        return false;

    Ipp32s iMBWidth = info->m_pFrame->getCD()->m_WidthInCU;
    Ipp32s availableToProcess = info->m_curCUToProcess[REC_PROCESS_ID] - info->m_curCUToProcess[DEB_PROCESS_ID];

    if (availableToProcess > iMBWidth || (availableToProcess && info->m_curCUToProcess[REC_PROCESS_ID] >= info->m_pFrame->getCD()->m_NumCUsInFrame))
    {
        info->m_processInProgress[DEB_PROCESS_ID] = 1;

        pTask->m_taskPreparingGuard->Unlock();
        InitTask(info, pTask, info->GetAnySlice());
        pTask->m_iFirstMB = info->m_curCUToProcess[DEB_PROCESS_ID];
        pTask->m_iMBToProcess = IPP_MIN(iMBWidth - (info->m_curCUToProcess[DEB_PROCESS_ID] % iMBWidth), availableToProcess);

        pTask->m_iTaskID = TASK_DEB_H265;
        return true;
    }

    return false;

} // bool TaskBrokerTwoThread_H265::GetDeblockingTask(H265Task *pTask)

// Initialize tile SAO task object
bool TaskBrokerTwoThread_H265::GetSAOTaskTile(H265DecoderFrameInfo * info, H265Task *pTask)
{
    if (!info->IsNeedSAO() || info->m_processInProgress[SAO_PROCESS_ID])
        return false;

    // this is guarded function, safe to touch any variable
    Ipp32s iMBWidth = info->m_pFrame->getCD()->m_WidthInCU;
    Ipp32s prevCUAddrReady = IPP_MIN(info->m_curCUToProcess[REC_PROCESS_ID], info->m_curCUToProcess[DEB_PROCESS_ID]);
    Ipp32s availableToProcess = prevCUAddrReady - info->m_curCUToProcess[SAO_PROCESS_ID];

    if (availableToProcess >= 2*iMBWidth || (availableToProcess && prevCUAddrReady >= info->m_pFrame->getCD()->m_NumCUsInFrame))
    {
        info->m_processInProgress[SAO_PROCESS_ID] = 1;

        pTask->m_taskPreparingGuard->Unlock();
        InitTask(info, pTask, info->GetAnySlice());
        pTask->m_iFirstMB = info->m_curCUToProcess[SAO_PROCESS_ID];
        pTask->m_iMBToProcess = IPP_MIN(iMBWidth - (info->m_curCUToProcess[SAO_PROCESS_ID] % iMBWidth), availableToProcess);

        pTask->m_iTaskID = TASK_SAO_H265;
        return true;
    }

    return false;
}

// Select a new task for available frame and initialize a task object for it
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
        if (pTask->m_iThreadNumber || info != m_FirstAU)
            return false;

        return TaskBrokerSingleThread_H265::GetNextTaskInternal(pTask);
    }

    if (info->m_hasTiles)
    {
        if (GetSAOTaskTile(info, pTask))
            return true;

        if (GetDeblockingTaskTile(info, pTask))
            return true;

        if (GetDecRecTileTask(info, pTask))
            return true;

        if (GetReconstructTileTask(info, pTask))
            return true;

        if (GetDecodingTileTask(info, pTask))
            return true;
    }
    else
    {
        if (GetSAOTaskTile(info, pTask))
            return true;

        if (GetDeblockingTask(info, pTask))
            return true;

        if (GetDecRecTask(info, pTask))
            return true;

        if (GetReconstructTask(info, pTask))
            return true;

        if (GetDecodingTask(info, pTask))
            return true;
    }

    return false;
}

// Calculate frame state after a task has been finished
void TaskBrokerTwoThread_H265::AddPerformedTask(H265Task *pTask)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread_H265::AddPerformedTask(pTask);

#if 0
    if (pTask->m_pSlicesInfo->m_hasTiles) // Need to fix this issue
    {
        bool completed = true;
        for (Ipp32u i = 0; i < pTask->m_pSlicesInfo->m_tilesThreadingInfo.size(); i ++)
        {
            if (!pTask->m_pSlicesInfo->m_tilesThreadingInfo[i].processInfo.m_isCompleted)
            {
                completed = false;
                break;
            }
        }

        if (completed)
        {
            pTask->m_pSlicesInfo->m_curCUToProcess[DEC_PROCESS_ID] = pTask->m_pSlicesInfo->m_pFrame->getCD()->m_NumCUsInFrame;
            pTask->m_pSlicesInfo->m_curCUToProcess[REC_PROCESS_ID] = pTask->m_pSlicesInfo->m_pFrame->getCD()->m_NumCUsInFrame;
        }
    }
#endif

    if (pTask->m_pSlicesInfo->IsCompleted())
    {
        pTask->m_pSlicesInfo->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
    }

    FreeResources(pTask);

    SwitchCurrentAU();
} // void TaskBrokerTwoThread_H265::AddPerformedTask(H265Task *pTask)

// Check if task can be started and initialize decoding context if necessary
bool TaskBrokerTwoThread_H265::GetResources(H265Task *pTask)
{
    bool needAllocateContext = false;
    bool needInitializeContext = false;

    switch (pTask->m_iTaskID)
    {
    case TASK_DEC_REC_H265:
    case TASK_DEC_H265:
        if (pTask->m_pSlicesInfo->m_hasTiles)
        {
            if (!pTask->m_threadingInfo->m_context)
            {
                needAllocateContext = true;
            }
            else
            {
                if (pTask->m_threadingInfo->processInfo.firstCU != pTask->m_iFirstMB && pTask->m_pSlice->GetFirstMB() == pTask->m_iFirstMB)
                { // new slice of same tile. Need to reinitialize context
                    if (pTask->m_threadingInfo->processInfo.m_curCUToProcess[DEC_PROCESS_ID] != pTask->m_iFirstMB ||
                        pTask->m_threadingInfo->processInfo.m_curCUToProcess[REC_PROCESS_ID] != pTask->m_iFirstMB)
                        return false;
                    needInitializeContext = true;
                }
            }
            VM_ASSERT(!pTask->m_pSlice->m_context);
            pTask->m_context = pTask->m_threadingInfo->m_context;
        }
        else
        {
            needAllocateContext = !pTask->m_pSlice->m_context;
            pTask->m_context = pTask->m_pSlice->m_context;
        }

        if (pTask->m_context && !pTask->m_context->m_coeffBuffer.IsInputAvailable()) // there is no room for new coeffs
            return false;

        break;
    case TASK_REC_H265:
        if (pTask->m_pSlicesInfo->m_hasTiles)
        {
            VM_ASSERT(pTask->m_threadingInfo->m_context);
            pTask->m_context = pTask->m_threadingInfo->m_context;
        }
        else
        {
            VM_ASSERT(pTask->m_pSlice->m_context);
            pTask->m_context = pTask->m_pSlice->m_context;
        }

        if (pTask->m_context && !pTask->m_context->m_coeffBuffer.IsOutputAvailable())
            return false;

        break;
    }

    if (needAllocateContext)
    {
        pTask->m_context = m_pTaskSupplier->GetObjHeap()->AllocateObject<DecodingContext>();
        pTask->m_context->IncrementReference();
        pTask->m_context->Init(pTask);

        if (pTask->m_iTaskID == TASK_DEC_H265 || pTask->m_iTaskID == TASK_DEC_REC_H265)
        {
            if (pTask->m_pSlicesInfo->m_hasTiles)
                pTask->m_threadingInfo->m_context = pTask->m_context;
            else
                pTask->m_pSlice->m_context = pTask->m_context;
        }
    }

    if (needInitializeContext)
    {
        pTask->m_context->Init(pTask);
    }

    switch (pTask->m_iTaskID)
    {
    case TASK_DEC_REC_H265:    
    case TASK_DEC_H265:
        pTask->m_pBuffer = (CoeffsPtr)pTask->m_context->m_coeffBuffer.LockInputBuffer();
        break;
    case TASK_REC_H265:
        {
            Ipp8u* pointer;
            size_t size;
            pTask->m_context->m_coeffBuffer.LockOutputBuffer(pointer, size);
            pTask->m_pBuffer = (CoeffsPtr)pointer;
        }
        break;
    }

    return true;
}

// Deallocate decoding context if necessary
void TaskBrokerTwoThread_H265::FreeResources(H265Task *pTask)
{
    bool needDeallocateContext = false;
    switch (pTask->m_iTaskID)
    {
    case TASK_DEC_REC_H265:
    case TASK_DEC_H265:
    case TASK_REC_H265:
        if (pTask->m_pSlicesInfo->m_hasTiles)
        {
            needDeallocateContext = pTask->m_threadingInfo->processInfo.m_isCompleted;
            if (needDeallocateContext)
                pTask->m_threadingInfo->m_context = 0;
        }
        else
        {
            needDeallocateContext = pTask->m_pSlice->processInfo.m_isCompleted; // fully decoded
            if (needDeallocateContext)
                pTask->m_pSlice->m_context = 0;
        }
        break;
    }

    if (needDeallocateContext)
    {
        pTask->m_context->DecrementReference();
        pTask->m_context = 0;
    }
}

H265Slice * FindSliceByCUAddr(H265DecoderFrameInfo * info, Ipp32s firstCUToProcess)
{
    Ipp32s sliceCount = info->GetSliceCount();
    for (Ipp32s i = 0; i < sliceCount; ++i)
    {
        H265Slice *slice = info->GetSlice(i);
        if (slice->GetFirstMB() <= firstCUToProcess && slice->m_iMaxMB > firstCUToProcess)
        {
            return slice;
        }
    }

    return 0;
}

TileThreadingInfo * FindTileForProcess(H265DecoderFrameInfo * info, Ipp32s taskID, bool isDecRec)
{
    const H265PicParamSet * pps = info->GetAnySlice()->GetPicParam();

    VM_ASSERT(info->m_tilesThreadingInfo.size() == pps->getNumTiles());

    TileThreadingInfo * tileToWrap = 0;
    Ipp32s minLine = 0;
    Ipp32u currentTile = 0;
    for (Ipp32u row = 0; row < pps->num_tile_rows; row++)
    {
        for (Ipp32u column = 0; column < pps->num_tile_columns; column++)
        {
            TileThreadingInfo * tileInfo = &info->m_tilesThreadingInfo[currentTile++];
            const CUProcessInfo & processInfo = tileInfo->processInfo;

            if (processInfo.m_processInProgress[taskID])
                continue;

            // additional check by taskID
            if (taskID == REC_PROCESS_ID && processInfo.m_curCUToProcess[REC_PROCESS_ID] >= processInfo.m_curCUToProcess[DEC_PROCESS_ID])
                continue;

            if (isDecRec && (processInfo.m_processInProgress[REC_PROCESS_ID] || processInfo.m_curCUToProcess[DEC_PROCESS_ID] != processInfo.m_curCUToProcess[REC_PROCESS_ID]))
                continue;

            if (!tileToWrap)
            {
                tileToWrap = tileInfo;
                minLine = (tileToWrap->processInfo.m_curCUToProcess[taskID] - tileToWrap->processInfo.firstCU) / tileToWrap->processInfo.m_width;
                continue;
            }

            // choose best tile with min completed line of CU
            Ipp32s line = (processInfo.m_curCUToProcess[taskID] - tileInfo->processInfo.firstCU) / processInfo.m_width;
            if (line < minLine)
            {
                tileToWrap = tileInfo;
                minLine = line;
            }
        }

        if (tileToWrap)
            break;
    }

    return tileToWrap;
}

#endif // MFX_VA

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
