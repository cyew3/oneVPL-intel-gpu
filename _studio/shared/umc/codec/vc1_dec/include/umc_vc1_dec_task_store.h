// Copyright (c) 2004-2019 Intel Corporation
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
#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#ifndef __UMC_UMC_VC1_DEC_TASK_STORE_H_
#define __UMC_UMC_VC1_DEC_TASK_STORE_H_

#include <new>
#include <vector>
#include <map>
#include <mutex>
#include <memory>

#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_frame_descr.h"
#include "umc_vc1_dec_skipping.h"
#include "umc_vc1_dec_exception.h"
#include "umc_frame_allocator.h"

#ifdef ALLOW_SW_VC1_FALLBACK
#include "umc_vc1_dec_task.h"
#endif // #ifdef ALLOW_SW_VC1_FALLBACK

namespace UMC
{
    typedef enum
    {
        VC1_HD_STREAM = 0,
        VC1_MD_STREAM = 1,
        VC1_SD_STREAM = 2
    } VC1_STREAM_DEFINITION;

    class VC1TSHeap
    {
    public:
        VC1TSHeap(uint8_t* pBuf, int32_t BufSize):m_pBuf(pBuf),
                  m_iRemSize(BufSize)
        {
        };
        virtual ~VC1TSHeap()
        {
        };

        template <class T>
            void s_new(T*** pObj, uint32_t size)
        {
            int32_t size_T = (int32_t)(sizeof(pObj)*size);
            if (m_iRemSize - mfx::align_value<int32_t>(size_T) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                *pObj = new(m_pBuf)T*[size];
                m_pBuf     += mfx::align_value<int32_t>(size_T);
                m_iRemSize -= mfx::align_value<int32_t>(size_T);
            }
        };
        template <class T>
        void s_new_one(T** pObj, uint32_t size)
        {
            int32_t size_T = (int32_t)(sizeof(T)*size);
            if (m_iRemSize - mfx::align_value<int32_t>(size_T) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                *pObj = new(m_pBuf)T[size];
                m_pBuf     += mfx::align_value<int32_t>(size_T);
                m_iRemSize -= mfx::align_value<int32_t>(size_T);
            }
        };
        template <class T>
            void s_new(T** pObj, uint32_t size)
        {
            int32_t size_T = sizeof(T);
            if ((m_iRemSize - mfx::align_value<int32_t>(size_T)*((int32_t)size)) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                for(uint32_t i = 0; i < size; i++)
                {
                    pObj[i] = new(m_pBuf)(T);
                    m_pBuf     += mfx::align_value<int32_t>(size_T);
                    m_iRemSize -= mfx::align_value<int32_t>(size_T);
                }
            }

        };
        template <class Tbase,
                  class T,
                  class Arg>
            void s_new(Tbase** pObj,
                       uint32_t size,
                       Arg*   pArg)
        {
            int32_t size_T = sizeof(T);
            if ((m_iRemSize - mfx::align_value<int32_t>(size_T)*((int32_t)size)) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                for(uint32_t i = 0; i < size; i++)
                {
                    pObj[i] = new(m_pBuf)(T)(pArg);
                    m_pBuf     += mfx::align_value<int32_t>(size_T);
                    m_iRemSize -= mfx::align_value<int32_t>(size_T);
                }
            }

        };

        template <class T>
            void s_new(T** pObj)
        {
            int32_t size_T = sizeof(T);
            if ((m_iRemSize - mfx::align_value<int32_t>(size_T)) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                *pObj = new(m_pBuf)(T);
                m_pBuf     += mfx::align_value<int32_t>(size_T);
                m_iRemSize -= mfx::align_value<int32_t>(size_T);
            }
        }
        template <class T>
            uint8_t* s_alloc()
        {
            uint8_t* pRet;
            int32_t size_T = sizeof(T);
            if ((m_iRemSize - mfx::align_value<int32_t>(size_T)) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                pRet = m_pBuf;
                m_pBuf     += mfx::align_value<int32_t>(size_T);
                m_iRemSize -= mfx::align_value<int32_t>(size_T);
                return pRet;
            }
        }
    private:
        uint8_t*  m_pBuf;
        int32_t  m_iRemSize;

    };

    class VC1VideoDecoder;
    class VC1TaskStore: public VC1Skipping::VC1SkipMaster
    {
    public:

        VC1TaskStore(MemoryAllocator *pMemoryAllocator);

        virtual  ~VC1TaskStore();

    private:
        // Declare private copy constructor to avoid accidental assignment
        // and klocwork complaining.
        VC1TaskStore(const VC1TaskStore &);
        VC1TaskStore & operator = (const VC1TaskStore &);

    public:

        virtual bool     Init(uint32_t iConsumerNumber,
                      uint32_t iMaxFramesInParallel,
                      VC1VideoDecoder* pVC1Decoder);

        virtual bool Reset();

        bool CreateDSQueue(VC1Context* pContext, VideoAccelerator* va);

        bool SetNewSHParams(VC1Context* pContext);
        void ResetDSQueue();
        bool IsReadyDS()
        {
            uint32_t i;
            std::lock_guard<std::mutex> guard(m_mDSGuard);
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                if (m_pDescriptorQueue[i]->m_bIsReadyToLoad)
                    return true;
            }
            return false;
        }

        template <class Descriptor>
        void GetReadyDS(Descriptor** pDS)
        {
            std::lock_guard<std::mutex> guardDS(m_mDSGuard);
            uint32_t i;
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                std::lock_guard<std::mutex> guard(*m_pGuardGet[i]);
                if (m_pDescriptorQueue[i]->m_bIsReadyToLoad)
                {
                    m_pDescriptorQueue[i]->m_bIsReadyToLoad  = false;
                    m_pDescriptorQueue[i]->m_iActiveTasksInFirstField = 0;
                    *pDS = (Descriptor*)m_pDescriptorQueue[i];
                    ++m_iNumDSActiveinQueue;
                    return;
                }
            }
            *pDS = NULL;
        }

        template <class Descriptor>
        bool GetPerformedDS(Descriptor** pDS)
        {
            uint32_t i;
            std::lock_guard<std::mutex> guardDS(m_mDSGuard);
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                std::lock_guard<std::mutex> guard(*m_pGuardGet[i]);
                if (m_pDescriptorQueue[i]->m_bIsReadyToDisplay)
                {
                    if ((m_lNextFrameCounter == m_pDescriptorQueue[i]->m_iFrameCounter)&&
                        (!m_pDescriptorQueue[i]->m_bIsSkippedFrame))
                    {
                        m_pDescriptorQueue[i]->m_bIsReadyToDisplay = false;
                        m_pDescriptorQueue[i]->m_bIsReadyToLoad = true;
                        m_pDescriptorQueue[i]->m_bIsBusy = false;
                        --m_iNumDSActiveinQueue;
                        ++m_lNextFrameCounter;
                        *pDS = (Descriptor*)m_pDescriptorQueue[i];
                        return true;
                    }
                }
            }
            *pDS = NULL;
            return false;
        }
        // need for correct work VideoAcceleration
        void SetFirstBusyDescriptorAsReady()
        {
            uint32_t i;
            std::lock_guard<std::mutex> guardDS(m_mDSGuard);
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                std::lock_guard<std::mutex> guard(*m_pGuardGet[i]);
                if ((!m_pDescriptorQueue[i]->m_bIsReadyToDisplay)&&
                     (m_pDescriptorQueue[i]->m_iFrameCounter == m_lNextFrameCounter))
                {
                    m_pDescriptorQueue[i]->m_bIsReadyToDisplay = true;
                    m_pDescriptorQueue[i]->m_bIsReferenceReady = true;
                    m_pDescriptorQueue[i]->m_bIsBusy = true;
                    return;
                }
            }
        }
        void ResetPerformedDS(VC1FrameDescriptor* pDS)
        {
            pDS->m_bIsReadyToDisplay = false;
            pDS->m_bIsReadyToLoad = true;
            pDS->m_bIsBusy = false;
            --m_iNumDSActiveinQueue;
            uint32_t i;
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                if (!m_pDescriptorQueue[i]->m_bIsReadyToLoad)
                    ++m_pDescriptorQueue[i]->m_iFrameCounter;
            }
            ++m_lNextFrameCounter;
        }

        void AddInvalidPerformedDS(VC1FrameDescriptor* pDS)
        {
            pDS->m_bIsReadyToProcess = false;
            pDS->m_bIsReadyToDisplay = true;
            pDS->m_bIsValidFrame = false;
        }

        VC1FrameDescriptor* GetLastDS();
        VC1FrameDescriptor* GetFirstDS();

        template <class Descriptor>
        bool GetReadySkippedDS(Descriptor** pDS)
        {
            uint32_t i;
            std::lock_guard<std::mutex> guardDS(m_mDSGuard);
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                std::lock_guard<std::mutex> guard(*m_pGuardGet[i]);
                if ((m_pDescriptorQueue[i]->m_bIsReferenceReady)&&
                    (m_pDescriptorQueue[i]->m_bIsSkippedFrame)&&
                    (m_lNextFrameCounter == m_pDescriptorQueue[i]->m_iFrameCounter))
                {
                    *pDS = (Descriptor*)m_pDescriptorQueue[i];
                    m_pDescriptorQueue[i]->m_bIsReadyToDisplay = false;
                    m_pDescriptorQueue[i]->m_bIsReadyToLoad = true;
                    m_pDescriptorQueue[i]->m_bIsSkippedFrame = false;
                    m_pDescriptorQueue[i]->m_bIsBusy = false;
                    --m_iNumDSActiveinQueue;
                    ++m_lNextFrameCounter;
                    return true;
                }
            }
            *pDS = NULL;
            return false;
        }

        // CommonAllocation
        virtual FrameMemID LockSurface(FrameMemID* mid, bool isSkip = false);
        virtual void UnLockSurface(FrameMemID memID);

        FrameMemID GetPrevIndex(void);
        FrameMemID GetNextIndex(void);
        FrameMemID GetBFrameIndex(void) ;
        FrameMemID GetRangeMapIndex(void);

        void SetCurrIndex(FrameMemID Index);
        void SetPrevIndex(FrameMemID Index);
        void SetNextIndex(FrameMemID Index);
        void SetBFrameIndex(FrameMemID Index);
        void SetRangeMapIndex(FrameMemID Index);

        virtual FrameMemID  GetIdx(uint32_t Idx);

        void SeLastFramesMode() {m_bIsLastFramesMode = true;}

    protected:

        virtual int32_t      LockAndAssocFreeIdx(FrameMemID mid);
        virtual FrameMemID  UnLockIdx(uint32_t Idx);

        virtual uint32_t CalculateHeapSize();

        uint32_t  m_iConsumerNumber;

        VC1FrameDescriptor** m_pDescriptorQueue;

        uint32_t m_iNumFramesProcessing;
        uint32_t m_iNumDSActiveinQueue;

        std::mutex m_mDSGuard;

        std::vector<std::unique_ptr<std::mutex>> m_pGuardGet;

        VC1VideoDecoder* pMainVC1Decoder;
        unsigned long long m_lNextFrameCounter;
        VC1FrameDescriptor* m_pPrefDS;
        int32_t m_iRangeMapIndex;

        MemoryAllocator*           m_pMemoryAllocator; // (MemoryAllocator*) pointer to memory allocator

        // for correct VA support
        // H/W manage index by itself
        int32_t m_CurrIndex;
        int32_t m_PrevIndex;
        int32_t m_NextIndex;
        int32_t m_BFrameIndex;
        int32_t m_iICIndex;
        int32_t m_iICIndexB;

        UMC::MemID     m_iTSHeapID;

        VC1TSHeap* m_pSHeap;

        bool          m_bIsLastFramesMode;
    };

#ifdef ALLOW_SW_VC1_FALLBACK
    class VC1TaskStoreSW : public VC1TaskStore
    {
    public:
        VC1TaskStoreSW(MemoryAllocator *pMemoryAllocator);

        virtual  ~VC1TaskStoreSW();

        virtual bool     Init(uint32_t iConsumerNumber,
            uint32_t iMaxFramesInParallel,
            VC1VideoDecoder* pVC1Decoder);

        virtual bool Reset();

    private:
        // Declare private copy constructor to avoid accidental assignment
        // and klocwork complaining.
        VC1TaskStoreSW(const VC1TaskStoreSW &);

        VC1TaskStore & operator = (const VC1TaskStoreSW &);

        VC1Task*** m_pCommonQueue;
        VC1Task*** m_pAdditionalQueue;

        uint32_t* m_pTasksInQueue;
        uint32_t* m_pSlicesInQueue;

        VC1_STREAM_DEFINITION m_eStreamDef;
        int32_t* m_pDSIndicate;
        int32_t* m_pDSIndicateSwap;

        uint32_t  m_iCurrentTaskID;

        // each DWORD of m_pMainQueueTasksState and m_pAdditionaQueueTasksState contains state of 8 Tasks. Maximum number of tasks is 512 (row number)
        // Task state occupiers for 4 bits. xxxx
        // 0-th bit isDone
        // 1-th bit isProcess
        // 2-th bit isReady
        // 3-th bit Reserved
        uint32_t* m_pMainQueueTasksState;
        uint32_t* m_pAdditionaQueueTasksState;

        UMC::MemID     m_iIntStructID;

        std::vector<bool>             IntIndexes;
        std::map<uint32_t, FrameMemID>  AssocIdx;

        virtual uint32_t CalculateHeapSize();
        void SetExternalSurface(VC1Context* pContext, const FrameData* pFrameData, FrameMemID index);
        virtual int32_t      LockAndAssocFreeIdx(FrameMemID mid);
        virtual FrameMemID  UnLockIdx(uint32_t Idx);
        virtual FrameMemID  GetIdx(uint32_t Idx);

        inline VC1FrameDescriptor* GetPreferedDS()
        {
            VC1FrameDescriptor* pCurrDescriptor = m_pDescriptorQueue[0];
            uint32_t i;
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                if (m_pDescriptorQueue[i]->m_bIsReadyToProcess)
                {
                    pCurrDescriptor = m_pDescriptorQueue[i];
                    break;
                }
            }
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                if ((m_pDescriptorQueue[i]->m_bIsReadyToProcess) &&
                    (m_pDescriptorQueue[i]->m_iFrameCounter < pCurrDescriptor->m_iFrameCounter))
                    pCurrDescriptor = m_pDescriptorQueue[i];
            }
            return pCurrDescriptor;
        };

        inline bool IsMainTaskPrepareForProcess(uint32_t FrameID, uint32_t);
        inline bool IsAdditionalTaskPrepareForProcess(uint32_t FrameID, uint32_t TaskID);
        inline bool IsFrameReadyToDisplay(uint32_t FrameID);

        // main queue processing
        inline void SetTaskAsReady_MQ(uint32_t FrameID, uint32_t TaskID);
        inline void SetTaskAsNotReady_MQ(uint32_t FrameID, uint32_t TaskID);

        inline void SetTaskAsDone_MQ(uint32_t FrameID, uint32_t TaskID);
        inline void EnableProcessBit_MQ(uint32_t FrameID, uint32_t TaskID);
        inline void DisableProcessBit_MQ(uint32_t FrameID, uint32_t TaskID);
        // additional queue processing
        inline void SetTaskAsReady_AQ(uint32_t FrameID, uint32_t TaskID);
        inline void SetTaskAsNotReady_AQ(uint32_t FrameID, uint32_t TaskID);
        inline void SetTaskAsDone_AQ(uint32_t FrameID, uint32_t TaskID);
        inline void EnableProcessBit_AQ(uint32_t FrameID, uint32_t TaskID);
        inline void DisableProcessBit_AQ(uint32_t FrameID, uint32_t TaskID);

    public:
        virtual FrameMemID LockSurface(FrameMemID* mid, bool isSkip = false);
        virtual void UnLockSurface(FrameMemID memID);

        void OpenNextFrames(VC1FrameDescriptor* pDS, VC1FrameDescriptor** pPrevDS, int32_t* CurrRefDst, int32_t* CurBDst);
        void SetDstForFrameAdv(VC1FrameDescriptor* pDS, int32_t* CurrRefDst, int32_t* CurBDst);
        void SetDstForFrame(VC1FrameDescriptor* pDS, int32_t* CurrRefDst, int32_t* CurBDst);

        void FillIdxVector(uint32_t size);

        bool CreateDSQueue(VC1Context* pContext, bool IsReorder, int16_t* pResidBuf); //pResidBuf - memory for residual coeffs

        void SetDefinition(VC1SequenceLayerHeader* seqLayerHeader);

        // threads wake up
        void WakeTasksInAlienQueue(VC1FrameDescriptor* pDS, VC1FrameDescriptor** pPrevDS);
        void WakeTasksInAlienQueue(VC1FrameDescriptor* pDS);
        void CompensateDSInQueue(VC1FrameDescriptor* pDS);

        bool CreateTaskQueues();

        void AddPerformedDS(VC1FrameDescriptor* pDS)
        {
            pDS->m_bIsReadyToProcess = false;
            pDS->m_bIsReadyToDisplay = true;
        }

        inline bool IsPerfomedDS();
        bool IsProcessingDS();
        bool HaveTasksToBeDone();
        void FreeBusyDescriptor();

        bool AddSampleTask(VC1Task* _pTask, uint32_t qID);
        void DistributeTasks(uint32_t qID);
        bool GetNextTask(VC1FrameDescriptor **pFrameDS,
            VC1Task **pTask,
            uint32_t qID);
        bool GetNextTaskMainThread(VC1FrameDescriptor **pFrameDS,
            VC1Task** pTask,
            uint32_t    qID,
            bool      &isFrameComplete);

        // pipeline for Dual CPU cases.
        bool GetNextTaskDualCPU(VC1FrameDescriptor **pFrameDS,
            VC1Task **pTask,
            uint32_t qID);

        // pipeline for 4-CPU cases, high-defenition streams.
        bool GetNextTaskManyCPU_HD(VC1FrameDescriptor **pFrameDS,
            VC1Task **pTask,
            uint32_t qID);

        // pipeline for 4-CPU cases, medium/small-defenition streams.
        bool GetNextTaskManyCPU_MD(VC1FrameDescriptor **pFrameDS,
            VC1Task **pTask,
            uint32_t qID);

        bool AddPerfomedTask(VC1Task* pTask, VC1FrameDescriptor *pFrameDS);

        VC1PictureLayerHeader* GetFirstInSecondField(uint32_t qID);
    };
#endif // #ifdef ALLOW_SW_VC1_FALLBACK
}

#endif //__umc_umc_vc1_dec_task_store_H__
#endif //MFX_ENABLE_VC1_VIDEO_DECODE
