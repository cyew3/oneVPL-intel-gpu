/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/
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
class H264DecoderFrameList;

/****************************************************************************************************/
// Resources
/****************************************************************************************************/
class LocalResources
{
public:

    LocalResources();
    virtual ~LocalResources();

    Status Init(Ipp32s numberOfBuffers, MemoryAllocator *pMemoryAllocator);

    void Reset();
    void Close();

    H264DecoderLocalMacroblockDescriptor & GetMBInfo(Ipp32s number);
    IntraType * GetIntraTypes(Ipp32s number);

    void AllocateMBInfo(Ipp32s number, Ipp32u iMBCount);
    void AllocateMBIntraTypes(Ipp32s iIndex, Ipp32s iMBNumber);

    void AllocateBuffers(Ipp32s mb_count);

    bool LockFrameResource(H264DecoderFrame * frame);
    void UnlockFrameResource(H264DecoderFrame * frame);

    H264DecoderFrame * IsBusyByFrame(Ipp32s number);

    Ipp32u GetCurrentResourceIndex();

    H264DecoderMBAddr * GetDefaultMBMapTable() const;

    H264CoeffsBuffer * AllocateCoeffBuffer(H264Slice * slice);
    void FreeCoeffBuffer(H264CoeffsBuffer *);

public:
    Ipp8u         *m_pMBMap;
    H264DecoderMBAddr **next_mb_tables;//0 linear scan, 1,.. - bitstream defined scan (slice groups)

private:
    IntraType *(*m_ppMBIntraTypes);
    H264IntraTypesProp *m_piMBIntraProp;

    H264DecoderLocalMacroblockDescriptor *m_pMBInfo;

    Ipp32s m_numberOfBuffers;
    MemoryAllocator *m_pMemoryAllocator;

    Ipp32s          m_parsedDataLength;
    Ipp8u          *m_pParsedData;
    MemID           m_midParsedData;       // (MemID) mem id for allocated parsed data

    Ipp32u          m_currentResourceIndex;

    H264_Heap_Objects  m_ObjHeap;


    void DeallocateBuffers();

    LocalResources(const LocalResources &s);     // no copy CTR
    LocalResources & operator=(const LocalResources &s );

};

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
    bool IsExistTasks(H264DecoderFrame * frame);

    // Get next working task
    virtual bool GetNextTask(H264Task *pTask);

    virtual void Reset();
    virtual void Release();

    // Task was performed
    virtual void AddPerformedTask(H264Task *pTask);

    virtual void Start();

    virtual bool PrepareFrame(H264DecoderFrame * pFrame);

    void Lock();
    void Unlock();

    LocalResources * GetLocalResource();

    TaskSupplier * m_pTaskSupplier;

protected:

    Ipp32s GetNumberOfTasks(bool details);
    bool IsFrameCompleted(H264DecoderFrame * pFrame);

    virtual bool GetNextTaskInternal(H264Task *pTask) = 0;

    bool GetNextSlice(H264DecoderFrameInfo * info, H264Task *pTask);
    bool GetNextSliceToDecoding(H264DecoderFrameInfo * info, H264Task *pTask);

    // Get next available slice to deblocking
    bool GetNextSliceToDeblocking(H264DecoderFrameInfo * info, H264Task *pTask);

    bool GetPreparationTask(H264DecoderFrameInfo * info);

    void InitTask(H264DecoderFrameInfo * info, H264Task *pTask, H264Slice *pSlice);

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
    Event m_Completed;

    LocalResources m_localResourses;
};

#ifndef MFX_VA

class TaskBrokerSingleThread : public TaskBroker
{
public:
    TaskBrokerSingleThread(TaskSupplier * pTaskSupplier);

    // Get next working task
    virtual bool GetNextTaskInternal(H264Task *pTask);
};

class TaskBrokerTwoThread : public TaskBrokerSingleThread
{
public:

    TaskBrokerTwoThread(TaskSupplier * pTaskSupplier);

    virtual bool Init(Ipp32s iConsumerNumber);

    virtual bool GetNextTaskManySlices(H264DecoderFrameInfo * info, H264Task *pTask);

    // Get next working task
    virtual bool GetNextTaskInternal(H264Task *pTask);

    virtual void Release();
    virtual void Reset();

    virtual void AddPerformedTask(H264Task *pTask);

private:

    bool WrapDecodingTask(H264DecoderFrameInfo * info, H264Task *pTask, H264Slice *pSlice);
    bool WrapReconstructTask(H264DecoderFrameInfo * info, H264Task *pTask, H264Slice *pSlice);
    bool WrapDecRecTask(H264DecoderFrameInfo * info, H264Task *pTask, H264Slice *pSlice);

    bool GetDecRecTask(H264DecoderFrameInfo * info, H264Task *pTask);
    bool GetDecodingTask(H264DecoderFrameInfo * info, H264Task *pTask);
    bool GetReconstructTask(H264DecoderFrameInfo * info, H264Task *pTask);
    bool GetDeblockingTask(H264DecoderFrameInfo * info, H264Task *pTask);
    bool GetFrameDeblockingTaskThreaded(H264DecoderFrameInfo * info, H264Task *pTask);
#if defined (__ICL)
#pragma warning(disable:1125)
#endif
    void CompleteFrame();
};

#endif
} // namespace UMC

#endif // __UMC_H264_TASK_BROKER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
