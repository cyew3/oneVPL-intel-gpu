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

#ifndef __UMC_H264_TASK_BROKER_H
#define __UMC_H264_TASK_BROKER_H

#include <vector>
#include <list>

#include "umc_h264_dec_defs_dec.h"
#include "umc_h264_slice_decoding.h"
#include "umc_h264_heap.h"

namespace UMC
{

class TaskSupplier;
class H264Task;

/****************************************************************************************************/
// TaskBroker
/****************************************************************************************************/
class TaskBroker
{
public:
    TaskBroker(TaskSupplier * pTaskSupplier);

    virtual bool Init(Ipp32s iConsumerNumber);
    virtual ~TaskBroker();

    virtual bool AddFrameToDecoding(H264DecoderFrame * pFrame);

    virtual bool IsEnoughForStartDecoding(bool force);

    // Get next working task
    virtual bool GetNextTask(H264Task *pTask);

    virtual void Reset();
    virtual void Release();

    // Task was performed
    virtual void AddPerformedTask(H264Task *pTask);

    virtual void Start();

    void Lock();
    void Unlock();

    TaskSupplier * m_pTaskSupplier;

protected:

    bool IsFrameCompleted(H264DecoderFrame * pFrame);

    virtual bool GetNextTaskInternal(H264Task *pTask) = 0;
    virtual bool PrepareFrame(H264DecoderFrame * pFrame) = 0;

    void InitAUs();
    H264DecoderFrameInfo * FindAU();
    void SwitchCurrentAU();
    virtual void CompleteFrame(H264DecoderFrame * frame);
    void RemoveAU(H264DecoderFrameInfo * toRemove);

    Ipp32s m_iConsumerNumber;

    H264DecoderFrameInfo * m_FirstAU;

    bool m_IsShouldQuit;

    typedef std::list<H264DecoderFrame *> FrameQueue;
    FrameQueue m_decodingQueue;
    FrameQueue m_completedQueue;

    Mutex m_mGuard;
};

} // namespace UMC

#endif // __UMC_H264_TASK_BROKER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
