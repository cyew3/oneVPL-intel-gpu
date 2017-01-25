//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_UMC_VC1_DEC_TASK_STORE_H_
#define __UMC_UMC_VC1_DEC_TASK_STORE_H_

#include <new>
#include <vector>
#include <map>

#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_frame_descr.h"
#include "vm_types.h"
#include "umc_event.h"
#include "umc_automatic_mutex.h"
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
        VC1TSHeap(Ipp8u* pBuf, Ipp32s BufSize):m_pBuf(pBuf),
                  m_iRemSize(BufSize)
        {
        };
        virtual ~VC1TSHeap()
        {
        };

        template <class T>
            void s_new(T*** pObj, Ipp32u size)
        {
            Ipp32s size_T = (Ipp32s)(sizeof(pObj)*size);
            if (m_iRemSize - align_value<Ipp32s>(size_T) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                *pObj = new(m_pBuf)T*[size];
                m_pBuf += align_value<Ipp32s>(size_T);
                m_iRemSize -= align_value<Ipp32s>(size_T);
            }
        };
        template <class T>
        void s_new_one(T** pObj, Ipp32u size)
        {
            Ipp32s size_T = (Ipp32s)(sizeof(T)*size);
            if (m_iRemSize - align_value<Ipp32s>(size_T) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                *pObj = new(m_pBuf)T[size];
                m_pBuf += align_value<Ipp32s>(size_T);
                m_iRemSize -= align_value<Ipp32s>(size_T);
            }
        };
        template <class T>
            void s_new(T** pObj, Ipp32u size)
        {
            Ipp32s size_T = sizeof(T);
            if ((m_iRemSize - align_value<Ipp32s>(size_T)*size) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                for(Ipp32u i = 0; i < size; i++)
                {
                    pObj[i] = new(m_pBuf)(T);
                    m_pBuf += align_value<Ipp32s>(size_T);
                    m_iRemSize -= align_value<Ipp32s>(size_T);
                }
            }

        };
        template <class Tbase,
                  class T,
                  class Arg>
            void s_new(Tbase** pObj,
                       Ipp32u size,
                       Arg*   pArg)
        {
            Ipp32s size_T = sizeof(T);
            if ((m_iRemSize - align_value<Ipp32s>(size_T)*size) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                for(Ipp32u i = 0; i < size; i++)
                {
                    pObj[i] = new(m_pBuf)(T)(pArg);
                    m_pBuf += align_value<Ipp32s>(size_T);
                    m_iRemSize -= align_value<Ipp32s>(size_T);
                }
            }

        };

        template <class T>
            void s_new(T** pObj)
        {
            Ipp32s size_T = sizeof(T);
            if ((m_iRemSize - align_value<Ipp32s>(size_T)) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                *pObj = new(m_pBuf)(T);
                m_pBuf += align_value<Ipp32s>(size_T);
                m_iRemSize -= align_value<Ipp32s>(size_T);
            }
        }
        template <class T>
            Ipp8u* s_alloc()
        {
            Ipp8u* pRet;
            Ipp32s size_T = sizeof(T);
            if ((m_iRemSize - align_value<Ipp32s>(size_T)) < 0)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            else
            {
                pRet = m_pBuf;
                m_pBuf += align_value<Ipp32s>(size_T);
                m_iRemSize -= align_value<Ipp32s>(size_T);
                return pRet;
            }
        }
    private:
        Ipp8u*  m_pBuf;
        Ipp32s  m_iRemSize;

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

        void* operator new(size_t size, void* p)
        {
            if (!p)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
            return new(p) Ipp8u[size];
        };

        // external memory management. No need to delete memory
        void operator delete(void *p) THROWSEXCEPTION
        {
            //Anyway its incorrect when we trying free null pointer
            if (!p)
                throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
        };

        void operator delete(void *, void *) THROWSEXCEPTION
        {
            // delete for system exceptions case
            throw VC1Exceptions::vc1_exception(VC1Exceptions::mem_allocation_er);
        };

        virtual bool     Init(Ipp32u iConsumerNumber,
                      Ipp32u iMaxFramesInParallel,
                      VC1VideoDecoder* pVC1Decoder);

        virtual bool Reset();

        bool CreateDSQueue(VC1Context* pContext, VideoAccelerator* va);

        bool SetNewSHParams(VC1Context* pContext);
        void ResetDSQueue();
        bool IsReadyDS()
        {
            Ipp32u i;
            AutomaticMutex guard(m_mDSGuard);
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
            AutomaticMutex guardDS(m_mDSGuard);
            Ipp32u i;
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                AutomaticMutex guard(*m_pGuardGet[i]);
                if (m_pDescriptorQueue[i]->m_bIsReadyToLoad)
                {
                    m_pDescriptorQueue[i]->m_bIsReadyToLoad  = false;
                    m_pDescriptorQueue[i]->m_iActiveTasksInFirstField = 0;
                    *pDS = (Descriptor*)m_pDescriptorQueue[i];
                    ++m_iNumDSActiveinQueue;
                    return;
                }
                guard.Unlock();
            }
            *pDS = NULL;
        }

        template <class Descriptor>
        bool GetPerformedDS(Descriptor** pDS)
        {
            Ipp32u i;
            AutomaticMutex guardDS(m_mDSGuard);
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                AutomaticMutex guard(*m_pGuardGet[i]);
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
                guard.Unlock();
            }
            *pDS = NULL;
            return false;
        }
        // need for correct work VideoAcceleration
        void SetFirstBusyDescriptorAsReady()
        {
            Ipp32u i;
            AutomaticMutex guardDS(m_mDSGuard);
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                AutomaticMutex guard(*m_pGuardGet[i]);
                if ((!m_pDescriptorQueue[i]->m_bIsReadyToDisplay)&&
                     (m_pDescriptorQueue[i]->m_iFrameCounter == m_lNextFrameCounter))
                {
                    m_pDescriptorQueue[i]->m_bIsReadyToDisplay = true;
                    m_pDescriptorQueue[i]->m_bIsReferenceReady = true;
                    m_pDescriptorQueue[i]->m_bIsBusy = true;
                    return;
                }
                guard.Unlock();
            }
        }
        void ResetPerformedDS(VC1FrameDescriptor* pDS)
        {
            pDS->m_bIsReadyToDisplay = false;
            pDS->m_bIsReadyToLoad = true;
            pDS->m_bIsBusy = false; 
            --m_iNumDSActiveinQueue;
            Ipp32u i;
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
            Ipp32u i;
            AutomaticMutex guardDS(m_mDSGuard);
            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                AutomaticMutex guard(*m_pGuardGet[i]);
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
                guard.Unlock();
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

        virtual FrameMemID  GetIdx(Ipp32u Idx);

        void SeLastFramesMode() {m_bIsLastFramesMode = true;}

    protected:

        virtual Ipp32s      LockAndAssocFreeIdx(FrameMemID mid);
        virtual FrameMemID  UnLockIdx(Ipp32u Idx);

        virtual Ipp32u CalculateHeapSize();

        Ipp32u  m_iConsumerNumber;

        VC1FrameDescriptor** m_pDescriptorQueue;

        Ipp32u m_iNumFramesProcessing;
        Ipp32u m_iNumDSActiveinQueue;

        vm_mutex m_mDSGuard;

        vm_mutex** m_pGuardGet;

        VC1VideoDecoder* pMainVC1Decoder;
        Ipp64u m_lNextFrameCounter;
        VC1FrameDescriptor* m_pPrefDS;
        Ipp32s m_iRangeMapIndex;

        MemoryAllocator*           m_pMemoryAllocator; // (MemoryAllocator*) pointer to memory allocator

        // for correct VA support
        // H/W manage index by itself
        Ipp32s m_CurrIndex;
        Ipp32s m_PrevIndex;
        Ipp32s m_NextIndex;
        Ipp32s m_BFrameIndex;
        Ipp32s m_iICIndex;
        Ipp32s m_iICIndexB;

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

        virtual bool     Init(Ipp32u iConsumerNumber,
            Ipp32u iMaxFramesInParallel,
            VC1VideoDecoder* pVC1Decoder);

        virtual bool Reset();

    private:
        // Declare private copy constructor to avoid accidental assignment
        // and klocwork complaining.
        VC1TaskStoreSW(const VC1TaskStoreSW &);

        VC1TaskStore & operator = (const VC1TaskStoreSW &);

        VC1Task*** m_pCommonQueue;
        VC1Task*** m_pAdditionalQueue;

        Ipp32u* m_pTasksInQueue;
        Ipp32u* m_pSlicesInQueue;

        VC1_STREAM_DEFINITION m_eStreamDef;
        Ipp32s* m_pDSIndicate;
        Ipp32s* m_pDSIndicateSwap;

        Ipp32u  m_iCurrentTaskID;

        // each DWORD of m_pMainQueueTasksState and m_pAdditionaQueueTasksState contains state of 8 Tasks. Maximum number of tasks is 512 (row number)
        // Task state occupiers for 4 bits. xxxx
        // 0-th bit isDone
        // 1-th bit isProcess
        // 2-th bit isReady
        // 3-th bit Reserved
        Ipp32u* m_pMainQueueTasksState;
        Ipp32u* m_pAdditionaQueueTasksState;

        UMC::MemID     m_iIntStructID;

        std::vector<bool>             IntIndexes;
        std::map<Ipp32u, FrameMemID>  AssocIdx;

        virtual Ipp32u CalculateHeapSize();
        void SetExternalSurface(VC1Context* pContext, const FrameData* pFrameData, FrameMemID index);
        virtual Ipp32s      LockAndAssocFreeIdx(FrameMemID mid);
        virtual FrameMemID  UnLockIdx(Ipp32u Idx);
        virtual FrameMemID  GetIdx(Ipp32u Idx);

        inline VC1FrameDescriptor* GetPreferedDS()
        {
            VC1FrameDescriptor* pCurrDescriptor = m_pDescriptorQueue[0];
            Ipp32u i;
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

        inline bool IsMainTaskPrepareForProcess(Ipp32u FrameID, Ipp32u);
        inline bool IsAdditionalTaskPrepareForProcess(Ipp32u FrameID, Ipp32u TaskID);
        inline bool IsFrameReadyToDisplay(Ipp32u FrameID);

        // main queue processing
        inline void SetTaskAsReady_MQ(Ipp32u FrameID, Ipp32u TaskID);
        inline void SetTaskAsNotReady_MQ(Ipp32u FrameID, Ipp32u TaskID);

        inline void SetTaskAsDone_MQ(Ipp32u FrameID, Ipp32u TaskID);
        inline void EnableProcessBit_MQ(Ipp32u FrameID, Ipp32u TaskID);
        inline void DisableProcessBit_MQ(Ipp32u FrameID, Ipp32u TaskID);
        // additional queue processing
        inline void SetTaskAsReady_AQ(Ipp32u FrameID, Ipp32u TaskID);
        inline void SetTaskAsNotReady_AQ(Ipp32u FrameID, Ipp32u TaskID);
        inline void SetTaskAsDone_AQ(Ipp32u FrameID, Ipp32u TaskID);
        inline void EnableProcessBit_AQ(Ipp32u FrameID, Ipp32u TaskID);
        inline void DisableProcessBit_AQ(Ipp32u FrameID, Ipp32u TaskID);

    public:
        virtual FrameMemID LockSurface(FrameMemID* mid, bool isSkip = false);
        virtual void UnLockSurface(FrameMemID memID);

        void OpenNextFrames(VC1FrameDescriptor* pDS, VC1FrameDescriptor** pPrevDS, Ipp32s* CurrRefDst, Ipp32s* CurBDst);
        void SetDstForFrameAdv(VC1FrameDescriptor* pDS, Ipp32s* CurrRefDst, Ipp32s* CurBDst);
        void SetDstForFrame(VC1FrameDescriptor* pDS, Ipp32s* CurrRefDst, Ipp32s* CurBDst);

        void FillIdxVector(Ipp32u size);

        bool CreateDSQueue(VC1Context* pContext, bool IsReorder, Ipp16s* pResidBuf); //pResidBuf - memory for residual coeffs

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

        bool AddSampleTask(VC1Task* _pTask, Ipp32u qID);
        void DistributeTasks(Ipp32u qID);
        bool GetNextTask(VC1FrameDescriptor **pFrameDS,
            VC1Task **pTask,
            Ipp32u qID);
        bool GetNextTaskMainThread(VC1FrameDescriptor **pFrameDS,
            VC1Task** pTask,
            Ipp32u    qID,
            bool      &isFrameComplete);

        // pipeline for Dual CPU cases.
        bool GetNextTaskDualCPU(VC1FrameDescriptor **pFrameDS,
            VC1Task **pTask,
            Ipp32u qID);

        // pipeline for 4-CPU cases, high-defenition streams.
        bool GetNextTaskManyCPU_HD(VC1FrameDescriptor **pFrameDS,
            VC1Task **pTask,
            Ipp32u qID);

        // pipeline for 4-CPU cases, medium/small-defenition streams.
        bool GetNextTaskManyCPU_MD(VC1FrameDescriptor **pFrameDS,
            VC1Task **pTask,
            Ipp32u qID);

        bool AddPerfomedTask(VC1Task* pTask, VC1FrameDescriptor *pFrameDS);

        VC1PictureLayerHeader* GetFirstInSecondField(Ipp32u qID);
    };
#endif // #ifdef ALLOW_SW_VC1_FALLBACK
}

#endif //__umc_umc_vc1_dec_task_store_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
