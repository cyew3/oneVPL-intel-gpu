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

#ifndef __UMC_H264_TASK_BROKER_MT_H
#define __UMC_H264_TASK_BROKER_MT_H

#include "umc_h264_task_broker.h"
#include "umc_h264_dec_structures.h"

namespace UMC
{
class H264SliceEx;
class TaskSupplier;

class H264Task
{
public:
    // Default constructor
    H264Task(Ipp32s iThreadNumber)
        : m_iThreadNumber(iThreadNumber)
    {
        m_pSlice = 0;

        m_pBuffer = 0;
        m_WrittenSize = 0;

        m_iFirstMB = -1;
        m_iMaxMB = -1;
        m_iMBToProcess = -1;
        m_iTaskID = 0;
        m_bDone = false;
        m_bError = false;
        m_mvsDistortion = 0;
        m_taskPreparingGuard = 0;
    }

    CoeffsPtrCommon m_pBuffer;                                  // (Ipp16s *) pointer to working buffer
    size_t          m_WrittenSize;

    H264SliceEx *m_pSlice;                                        // (H264Slice *) pointer to owning slice
    H264DecoderFrameInfo * m_pSlicesInfo;
    AutomaticUMCMutex    * m_taskPreparingGuard;

    Ipp32s m_mvsDistortion;
    Ipp32s m_iThreadNumber;                                     // (Ipp32s) owning thread number
    Ipp32s m_iFirstMB;                                          // (Ipp32s) first MB in slice
    Ipp32s m_iMaxMB;                                            // (Ipp32s) maximum MB number in owning slice
    Ipp32s m_iMBToProcess;                                      // (Ipp32s) number of MB to processing
    Ipp32s m_iTaskID;                                           // (Ipp32s) task identificator
    bool m_bDone;                                               // (bool) task was done
    bool m_bError;                                              // (bool) there is a error
};

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

    void InitTask(H264DecoderFrameInfo * info, H264Task *pTask, H264SliceEx *pSlice);
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

    bool WrapDecodingTask(H264DecoderFrameInfo * info, H264Task *pTask, H264SliceEx *pSlice);
    bool WrapReconstructTask(H264DecoderFrameInfo * info, H264Task *pTask, H264SliceEx *pSlice);
    bool WrapDecRecTask(H264DecoderFrameInfo * info, H264Task *pTask, H264SliceEx *pSlice);

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
