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

#ifndef __UMC_H265_TASK_BROKER_H
#define __UMC_H265_TASK_BROKER_H

#include <vector>
#include <list>

#include "umc_event.h"

#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_heap.h"

namespace UMC_HEVC_DECODER
{
class H265DecoderFrameInfo;
class H265DecoderFrameList;
class H265Slice;
class H265Task;
class TaskSupplier_H265;

class TaskBroker_H265
{
public:
    TaskBroker_H265(TaskSupplier_H265 * pTaskSupplier);

    virtual bool Init(Ipp32s iConsumerNumber, bool isExistMainThread);
    virtual ~TaskBroker_H265();

    virtual void WaitFrameCompletion();
    void AwakeCompleteWaiter();
    virtual bool AddFrameToDecoding(H265DecoderFrame * pFrame);

    virtual bool IsEnoughForStartDecoding(bool force);
    bool IsExistTasks(H265DecoderFrame * frame);

    // Get next working task
    virtual bool GetNextTask(H265Task *pTask);

    virtual void Reset();
    virtual void Release();

    // Task was performed
    virtual void AddPerformedTask(H265Task *pTask);

    virtual void Start();

    virtual bool PrepareFrame(H265DecoderFrame * pFrame);

    void Lock();
    void Unlock();

    TaskSupplier_H265 * m_pTaskSupplier;

protected:

    void SleepThread(Ipp32s threadNumber);

    Ipp32s GetNumberOfTasks(void);
    bool IsFrameCompleted(H265DecoderFrame * pFrame) const;

    virtual bool GetNextTaskInternal(H265Task *pTask) = 0;

    bool GetNextSlice(H265DecoderFrameInfo * info, H265Task *pTask);
    bool GetNextSliceToDecoding(H265DecoderFrameInfo * info, H265Task *pTask);

    // Get next available slice to deblocking
    bool GetNextSliceToDeblocking(H265DecoderFrameInfo * info, H265Task *pTask);

    bool GetPreparationTask(H265DecoderFrameInfo * info);

    bool GetSAOFrameTask(H265DecoderFrameInfo * info, H265Task *pTask);

    void InitTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice);

    void InitAUs();
    H265DecoderFrameInfo * FindAU();
    void SwitchCurrentAU();
    virtual void CompleteFrame(H265DecoderFrame * frame);
    void RemoveAU(H265DecoderFrameInfo * toRemove);

    Ipp32s m_iConsumerNumber;

    H265DecoderFrameInfo * m_FirstAU;

    std::vector<UMC::Event> m_eWaiting;                          // waiting threads events
    volatile Ipp32u m_nWaitingThreads;                      // mask of waiting threads

    bool m_IsShouldQuit;

    virtual void AwakeThreads();

    typedef std::list<H265DecoderFrame *> FrameQueue;
    FrameQueue m_decodingQueue;
    FrameQueue m_completedQueue;

    UMC::Mutex m_mGuard;
    UMC::Event m_Completed;

    bool m_isExistMainThread;
};

class TaskBrokerSingleThread_H265 : public TaskBroker_H265
{
public:
    TaskBrokerSingleThread_H265(TaskSupplier_H265 * pTaskSupplier);

    // Get next working task
    virtual bool GetNextTaskInternal(H265Task *pTask);
};

class TaskBrokerTwoThread_H265 : public TaskBrokerSingleThread_H265
{
public:

    TaskBrokerTwoThread_H265(TaskSupplier_H265 * pTaskSupplier);

    virtual bool Init(Ipp32s iConsumerNumber, bool isExistMainThread);

    virtual bool GetNextTaskManySlices(H265DecoderFrameInfo * info, H265Task *pTask);

    // Get next working task
    virtual bool GetNextTaskInternal(H265Task *pTask);

    virtual void Release();
    virtual void Reset();

    virtual void AddPerformedTask(H265Task *pTask);

private:

    bool WrapDecRecTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice);
    bool WrapDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice);
    bool WrapReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice);

    bool GetDecRecTask(H265DecoderFrameInfo * info, H265Task *pTask);
    bool GetDeblockingTask(H265DecoderFrameInfo * info, H265Task *pTask);
    bool GetDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask);
    bool GetReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask);
    bool GetSAOTask(H265DecoderFrameInfo * info, H265Task *pTask);

    bool GetDecodingTileTask(H265DecoderFrameInfo * info, H265Task *pTask);
    bool GetReconstructTileTask(H265DecoderFrameInfo * info, H265Task *pTask);
    bool GetDecRecTileTask(H265DecoderFrameInfo * info, H265Task *pTask);
    bool GetDeblockingTaskTile(H265DecoderFrameInfo * info, H265Task *pTask);
    bool GetSAOTaskTile(H265DecoderFrameInfo * info, H265Task *pTask);

    bool GetResources(H265Task *pTask);
    void FreeResources(H265Task *pTask);

#if defined (__ICL)
#pragma warning(disable:1125)
#endif
    void CompleteFrame();
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_TASK_BROKER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER

