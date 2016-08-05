/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_TASK_BROKER_H
#define __UMC_H265_TASK_BROKER_H

#include <vector>
#include <list>

#include "umc_h265_dec_defs.h"
#include "umc_h265_heap.h"
#include "umc_h265_segment_decoder_base.h"

namespace UMC_HEVC_DECODER
{
class H265DecoderFrameInfo;
class H265DecoderFrameList;
class H265Slice;
class TaskSupplier_H265;

class DecodingContext;
struct TileThreadingInfo;

class H265Task;

// Decoder task scheduler class
class TaskBroker_H265
{
public:
    TaskBroker_H265(TaskSupplier_H265 * pTaskSupplier);

    // Initialize task broker with threads number
    virtual bool Init(Ipp32s iConsumerNumber);
    virtual ~TaskBroker_H265();

    // Add frame to decoding queue
    virtual bool AddFrameToDecoding(H265DecoderFrame * pFrame);

    // Returns whether enough bitstream data is evailable to start an asynchronous task
    virtual bool IsEnoughForStartDecoding(bool force);
    // Returns whether there is some work available for specified frame
    bool IsExistTasks(H265DecoderFrame * frame);

    // Tries to find a new task for asynchronous processing
    virtual bool GetNextTask(H265Task *pTask);

    // Reset to default values, stop all activity
    virtual void Reset();
    // Release resources
    virtual void Release();

    // Calculate frame state after a task has been finished
    virtual void AddPerformedTask(H265Task *pTask);

    // Wakes up working threads to start working on new tasks
    virtual void Start();

    // Check whether frame is prepared
    virtual bool PrepareFrame(H265DecoderFrame * pFrame);

    // Lock synchronization mutex
    void Lock();
    // Unlock synchronization mutex
    void Unlock();

    TaskSupplier_H265 * m_pTaskSupplier;

protected:

    // Returns number of access units available in the list but not processed yet
    Ipp32s GetNumberOfTasks(void);
    // Returns whether frame decoding is finished
    bool IsFrameCompleted(H265DecoderFrame * pFrame) const;

    virtual bool GetNextTaskInternal(H265Task *)
    {
        return false;
    }

    // Try to find an access unit which to decode next
    void InitAUs();
    // Find an access unit which has all slices found
    H265DecoderFrameInfo * FindAU();
    void SwitchCurrentAU();
    // Finish frame decoding
    virtual void CompleteFrame(H265DecoderFrame * frame);
    // Remove access unit from the linked list of frames
    void RemoveAU(H265DecoderFrameInfo * toRemove);

    Ipp32s m_iConsumerNumber;

    H265DecoderFrameInfo * m_FirstAU;

    bool m_IsShouldQuit;

    typedef std::list<H265DecoderFrame *> FrameQueue;
    FrameQueue m_decodingQueue;
    FrameQueue m_completedQueue;

    UMC::Mutex m_mGuard;
};

#ifndef MFX_VA
// Task ID enumerator
enum
{
    TASK_PROCESS_H265  = 0, // whole slice is decoded and reconstructed
    TASK_DEC_H265, // piece of slice is decoded
    TASK_REC_H265, // piece of slice is reconstructed
    TASK_DEB_H265, // piece of slice is deblocked
    TASK_DEC_REC_H265, // piece of slice is decoded and reconstructed
    TASK_SAO_H265  // piece of slice is saoed
};


// Asynchronous task descriptor class
class H265Task
{
public:
    // Default constructor
    H265Task(Ipp32s iThreadNumber)
        : m_iThreadNumber(iThreadNumber)
    {
        m_pSlice = 0;

        m_pBuffer = 0;
        m_WrittenSize = 0;

        m_iFirstMB = -1;
        m_iMaxMB = -1;
        m_iMBToProcess = -1;
        m_iTaskID = 0;
        m_bError = false;
        m_taskPreparingGuard = 0;
        m_context = 0;
        m_threadingInfo = 0;
    }

    CoeffsPtr m_pBuffer;                                  // (Ipp16s *) pointer to working buffer
    size_t          m_WrittenSize;

    DecodingContext * m_context;
    H265Slice *m_pSlice;                                        // (H265Slice *) pointer to owning slice
    TileThreadingInfo * m_threadingInfo;
    H265DecoderFrameInfo * m_pSlicesInfo;
    UMC::AutomaticUMCMutex    * m_taskPreparingGuard;

    Ipp32s m_iThreadNumber;                                     // (Ipp32s) owning thread number
    Ipp32s m_iFirstMB;                                          // (Ipp32s) first MB in slice
    Ipp32s m_iMaxMB;                                            // (Ipp32s) maximum MB number in owning slice
    Ipp32s m_iMBToProcess;                                      // (Ipp32s) number of MB to processing
    Ipp32s m_iTaskID;                                           // (Ipp32s) task identificator
    bool m_bError;                                              // (bool) there is a error
};

// Task broker which uses one thread only
class TaskBrokerSingleThread_H265 : public TaskBroker_H265
{
public:
    TaskBrokerSingleThread_H265(TaskSupplier_H265 * pTaskSupplier);

    // Get next working task
    virtual bool GetNextTaskInternal(H265Task *pTask);
    // Calculate frame state after a task has been finished
    virtual void AddPerformedTask(H265Task *pTask);
    // Returns whether enough bitstream data is evailable to start an asynchronous task
    virtual bool IsEnoughForStartDecoding(bool force);

protected:

    // Initialize task object with default values
    void InitTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice);
    // Tries to find a new slice to work on in specified access unit and initializes a task for it
    bool GetNextSlice(H265DecoderFrameInfo * info, H265Task *pTask);
    // Tries to find a portion of decoding+reconstruction work in the specified access unit and initializes a task object for it
    bool GetNextSliceToDecoding(H265DecoderFrameInfo * info, H265Task *pTask);

    // Tries to find a portion of deblocking filtering work in the specified access unit and initializes a task object for it
    bool GetNextSliceToDeblocking(H265DecoderFrameInfo * info, H265Task *pTask);

    // Tries to find a portion of SAO filtering work in the specified access unit and initializes a task object for it
    bool GetSAOFrameTask(H265DecoderFrameInfo * info, H265Task *pTask);

    // Initialize frame slice and tile start coordinates
    bool GetPreparationTask(H265DecoderFrameInfo * info);
};

// Task broker which uses multiple threads
class TaskBrokerTwoThread_H265 : public TaskBrokerSingleThread_H265
{
public:

    TaskBrokerTwoThread_H265(TaskSupplier_H265 * pTaskSupplier);

    virtual bool Init(Ipp32s iConsumerNumber);

    // Select a new task for available frame and initialize a task object for it
    virtual bool GetNextTaskManySlices(H265DecoderFrameInfo * info, H265Task *pTask);

    // Try to find a new task and initialize a task object for it
    virtual bool GetNextTaskInternal(H265Task *pTask);

    virtual void Release();
    virtual void Reset();

    // Calculate frame state after a task has been finished
    virtual void AddPerformedTask(H265Task *pTask);

private:

    // Initialize CTB range for decoding+reconstruct task
    bool WrapDecRecTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice);
    // Initialize CTB range for decoding task
    bool WrapDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice);
    // Initialize CTB range for reconstruct task
    bool WrapReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask, H265Slice *pSlice);

    // Initialize decoding+reconstruct task object
    bool GetDecRecTask(H265DecoderFrameInfo * info, H265Task *pTask);
    // Initialize deblocking task object
    bool GetDeblockingTask(H265DecoderFrameInfo * info, H265Task *pTask);
    // Initialize decoding task object
    bool GetDecodingTask(H265DecoderFrameInfo * info, H265Task *pTask);
    // Initialize reconstruct task object
    bool GetReconstructTask(H265DecoderFrameInfo * info, H265Task *pTask);
    // Initialize SAO task object
    bool GetSAOTask(H265DecoderFrameInfo * info, H265Task *pTask);

    // Initialize tile decoding task object
    bool GetDecodingTileTask(H265DecoderFrameInfo * info, H265Task *pTask);
    // Initialize tile reconstruct task object
    bool GetReconstructTileTask(H265DecoderFrameInfo * info, H265Task *pTask);
    // Initialize tile decoding+reconstruct task object
    bool GetDecRecTileTask(H265DecoderFrameInfo * info, H265Task *pTask);
    // Initialize tile deblocking task object
    bool GetDeblockingTaskTile(H265DecoderFrameInfo * info, H265Task *pTask);
    // Initialize tile SAO task object
    bool GetSAOTaskTile(H265DecoderFrameInfo * info, H265Task *pTask);

    // Check if task can be started and initialize decoding context if necessary
    bool GetResources(H265Task *pTask);
    // Deallocate decoding context if necessary
    void FreeResources(H265Task *pTask);

    // Update access units list finishing completed frames
    void CompleteFrame();
};

#endif // MFX_VA

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_TASK_BROKER_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
