//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include <cstdarg>
#include "umc_h264_task_broker.h"
#include "umc_h264_heap.h"
#include "umc_h264_task_supplier.h"
#include "umc_h264_frame_list.h"

#include "umc_h264_dec_debug.h"
#include "mfx_trace.h"

//#define ECHO_DEB
//#define VM_DEBUG

namespace UMC
{

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
    return true;
}

void TaskBroker::Reset()
{
    AutomaticUMCMutex guard(m_mGuard);
    m_FirstAU = 0;
    m_IsShouldQuit = true;

    m_decodingQueue.clear();
    m_completedQueue.clear();
}

void TaskBroker::Release()
{
    Reset();
}

/*LocalResources * TaskBroker::GetLocalResource()
{
    return &m_localResourses;
}*/

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

bool TaskBroker::AddFrameToDecoding(H264DecoderFrame * frame)
{
    if (!frame || frame->IsDecodingStarted())
        return false;

    H264DecoderLayer::FillnessStatus status = frame->GetAU(0)->GetStatus();
    if (!((status == H264DecoderFrameInfo::STATUS_FILLED) || (status == H264DecoderFrameInfo::STATUS_STARTED)))
        return false;

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
    }

    return 0;
}

void TaskBroker::InitAUs()
{
    H264DecoderFrameInfo * pPrev;
    H264DecoderFrameInfo * refAU;

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

    H264DecoderFrameInfo * pTemp = FindAU();
    for (; pTemp; )
    {
        if (!PrepareFrame(pTemp->m_pFrame))
        {
            pPrev->SetNextAU(0);
            break;
        }

        if (pPrev->IsInterviewReference() && pPrev != refAU)
        {
            if (pPrev->m_pFrame->m_viewId != pTemp->m_pFrame->m_viewId)
            {
                pPrev->SetReference(true);
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

    return ret;
}

bool TaskBroker::GetNextTask(H264Task *pTask)
{
    return GetNextTaskInternal(pTask);
} // bool TaskBroker::GetNextTask(H264Task *pTask)

void TaskBroker::AddPerformedTask(H264Task *)
{
    // nothing to do
} // void TaskBroker::AddPerformedTask(H264Task *pTask)

bool TaskBroker::IsEnoughForStartDecoding(bool )
{
    AutomaticUMCMutex guard(m_mGuard);

    InitAUs();
    return m_FirstAU != 0;
}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
