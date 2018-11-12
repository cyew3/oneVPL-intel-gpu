// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
    H264Task(int32_t iThreadNumber)
        : m_iThreadNumber(iThreadNumber)
    {
        m_pSlice = NULL;
        m_pSlicesInfo = NULL;
        m_pBuffer = NULL;
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

    CoeffsPtrCommon m_pBuffer;                                  // (int16_t *) pointer to working buffer
    size_t          m_WrittenSize;

    H264SliceEx *m_pSlice;                                        // (H264Slice *) pointer to owning slice
    H264DecoderFrameInfo * m_pSlicesInfo;
    AutomaticUMCMutex    * m_taskPreparingGuard;

    int32_t m_mvsDistortion;
    int32_t m_iThreadNumber;                                     // (int32_t) owning thread number
    int32_t m_iFirstMB;                                          // (int32_t) first MB in slice
    int32_t m_iMaxMB;                                            // (int32_t) maximum MB number in owning slice
    int32_t m_iMBToProcess;                                      // (int32_t) number of MB to processing
    int32_t m_iTaskID;                                           // (int32_t) task identificator
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

    Status Init(int32_t numberOfBuffers, MemoryAllocator *pMemoryAllocator);

    void Reset();
    void Close();

    H264DecoderLocalMacroblockDescriptor & GetMBInfo(int32_t number);
    IntraType * GetIntraTypes(int32_t number);

    void AllocateMBInfo(int32_t number, uint32_t iMBCount);
    void AllocateMBIntraTypes(int32_t iIndex, int32_t iMBNumber);

    void AllocateBuffers(int32_t mb_count);

    bool LockFrameResource(H264DecoderFrame * frame);
    void UnlockFrameResource(H264DecoderFrame * frame);

    H264DecoderFrame * IsBusyByFrame(int32_t number);

    uint32_t GetCurrentResourceIndex();

    H264DecoderMBAddr * GetDefaultMBMapTable() const;

    H264CoeffsBuffer * AllocateCoeffBuffer(H264Slice * slice);
    void FreeCoeffBuffer(H264CoeffsBuffer *);

public:
    uint8_t         *m_pMBMap;
    H264DecoderMBAddr **next_mb_tables;//0 linear scan, 1,.. - bitstream defined scan (slice groups)

private:
    IntraType *(*m_ppMBIntraTypes);
    H264IntraTypesProp *m_piMBIntraProp;

    H264DecoderLocalMacroblockDescriptor *m_pMBInfo;

    int32_t m_numberOfBuffers;
    MemoryAllocator *m_pMemoryAllocator;

    int32_t          m_parsedDataLength;
    uint8_t          *m_pParsedData;
    MemID           m_midParsedData;       // (MemID) mem id for allocated parsed data

    uint32_t          m_currentResourceIndex;

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


    virtual bool Init(int32_t iConsumerNumber);
    virtual void Reset();
    virtual void Release();

    virtual bool IsEnoughForStartDecoding(bool force);

    // Get next working task
    virtual bool GetNextTaskInternal(H264Task *pTask);
    virtual void AddPerformedTask(H264Task *pTask);
    virtual bool PrepareFrame(H264DecoderFrame * pFrame);

protected:
    LocalResources m_localResourses;

    int32_t GetNumberOfTasks(bool details);

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

    virtual bool Init(int32_t iConsumerNumber);

    virtual bool GetNextTaskManySlices(H264DecoderFrameInfo * info, H264Task *pTask);

    // Get next working task
    virtual bool GetNextTaskInternal(H264Task *pTask);

    virtual void Release();
    virtual void Reset();

    virtual void AddPerformedTask(H264Task *pTask);

protected:
    using TaskBrokerSingleThread::CompleteFrame;

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
