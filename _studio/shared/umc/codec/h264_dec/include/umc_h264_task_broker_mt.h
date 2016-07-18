/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_TASK_BROKER_MT_H
#define __UMC_H264_TASK_BROKER_MT_H

#include "umc_h264_task_broker.h"

namespace UMC
{
class TaskSupplier;
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
class TaskBrokerSingleThread : public TaskBroker
{
public:
    TaskBrokerSingleThread(TaskSupplier * pTaskSupplier);


    virtual bool Init(Ipp32s iConsumerNumber);
    virtual void Reset();
    virtual void Release();

    virtual bool IsEnoughForStartDecoding(bool force);

    // Get next working task
    virtual bool GetNextTaskInternal(H264Task *pTask);
    virtual void AddPerformedTask(H264Task *pTask);
    virtual bool PrepareFrame(H264DecoderFrame * pFrame);

protected:
    LocalResources m_localResourses;

    Ipp32s GetNumberOfTasks(bool details);

    virtual void CompleteFrame(H264DecoderFrame * frame);

    void InitTask(H264DecoderFrameInfo * info, H264Task *pTask, H264Slice *pSlice);
    bool GetPreparationTask(H264DecoderFrameInfo * info);

    bool GetNextSlice(H264DecoderFrameInfo * info, H264Task *pTask);
    bool GetNextSliceToDecoding(H264DecoderFrameInfo * info, H264Task *pTask);

    // Get next available slice to deblocking
    bool GetNextSliceToDeblocking(H264DecoderFrameInfo * info, H264Task *pTask);
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

} // namespace UMC

#endif // __UMC_H264_TASK_BROKER_MT_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
