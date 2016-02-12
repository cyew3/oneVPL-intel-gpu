/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2016 Intel Corporation. All Rights Reserved.
//
//
//          VC-1 (VC1) decoder, Task processing base on H264
//
*/

#include "umc_defs.h"
#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_dec_task.h"
//#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_time_statistics.h"
#include "umc_vc1_dec_intens_comp_tbl.h"
#include "umc_vc1_video_decoder.h"
#include "umc_frame_data.h"

#include "mfx_trace.h"

#ifndef UMC_RESTRICTED_CODE_VA
#include "umc_va_base.h"
#include "umc_vc1_dec_frame_descr_va.h"
#endif


static const Ipp32u FrameReadyTable[9] = {0, 0x5,     0x55,     0x555,     0x5555,
                                          0x55555, 0x555555, 0x5555555, 0x55555555};

namespace UMC
{
    VC1TaskStore::VC1TaskStore(MemoryAllocator *pMemoryAllocator):m_pCommonQueue(NULL),
                                                                  m_pAdditionalQueue(NULL),
                                                                  m_iConsumerNumber(0),
                                                                  m_pTasksInQueue(NULL),
                                                                  m_iCurrentTaskID(0),
                                                                  m_pSlicesInQueue(NULL),
                                                                  m_pDescriptorQueue(NULL),
                                                                  m_pOutFrameQueue(NULL),
                                                                  m_iNumFramesProcessing(0),
                                                                  m_iNumDSActiveinQueue(0),
                                                                  pMainVC1Decoder(NULL),
                                                                  m_lNextFrameCounter(1),
                                                                  m_lNextRefFrameCounter(1),
                                                                  m_lFirstRefFrameCounter(1),
                                                                  m_iNumDecodeRefFrames(0),
                                                                  m_pPrefDS(NULL),
                                                                  m_pDSIndicate(NULL),
                                                                  m_pDSIndicateSwap(NULL),
                                                                  //m_bNeedToCompField(false),
                                                                  m_iRangeMapIndex(0),
                                                                  m_bIsNeedToDecode(true),
                                                                  m_eStreamDef(VC1_HD_STREAM),
                                                                  m_pMemoryAllocator(NULL),
                                                                  m_CurrIndex(-1),
                                                                  m_PrevIndex(-1),
                                                                  m_NextIndex(-1),
                                                                  m_BFrameIndex(-1),
                                                                  m_iICIndex(-1),
                                                                  m_iICIndexB(-1),
                                                                  m_pMainQueueTasksState(NULL),
                                                                  m_pAdditionaQueueTasksState(NULL),
                                                                  m_iIntStructID((MemID)-1),
                                                                  m_iTSHeapID((MemID)-1),
                                                                  m_pSHeap(NULL),
                                                                  m_FrameType(NONE_PICTURE),
                                                                  m_bIsLastFramesMode(false)
    {
        vm_mutex_set_invalid(&m_mDSGuard);
        m_pMemoryAllocator = pMemoryAllocator;
    }

    VC1TaskStore::~VC1TaskStore()
    {
        Ipp32u i;

        IntIndexes.clear();
        AssocIdx.clear();

        if (vm_mutex_is_valid(&m_mDSGuard))
            vm_mutex_destroy(&m_mDSGuard);

        for (i = 0; i < m_iNumFramesProcessing; i++)
        {
            vm_mutex_destroy(m_pGuardGet[i]);
            m_pGuardGet[i] = 0;
            vm_mutex_destroy(m_pGuardAdd[i]);
            m_pGuardAdd[i] = 0;
        }
        for (i = 0; i <  m_iConsumerNumber; i++)
            m_eWaiting[i]->~Event();

        if(m_pMemoryAllocator)
        {

            if (m_pDescriptorQueue)
            {
                for (i = 0; i < m_iNumFramesProcessing; i++)
                    m_pDescriptorQueue[i]->Release();
            }

            if (static_cast<int>(m_iIntStructID) != -1)
            {
                m_pMemoryAllocator->Unlock(m_iIntStructID);
                m_pMemoryAllocator->Free(m_iIntStructID);
                m_iIntStructID = (MemID)-1;
            }

            if (static_cast<int>(m_iTSHeapID) != -1)
            {
                m_pMemoryAllocator->Unlock(m_iTSHeapID);
                m_pMemoryAllocator->Free(m_iTSHeapID);
                m_iTSHeapID = (MemID)-1;
            }
        }
    }

   bool VC1TaskStore::Init(Ipp32u iConsumerNumber,
                           Ipp32u iMaxFramesInParallel,
                           VC1VideoDecoder* pVC1Decoder)
    {
        Ipp32u i;
        m_iNumDSActiveinQueue = 0;
        m_iRangeMapIndex   =  iMaxFramesInParallel + VC1NUMREFFRAMES -1;
        pMainVC1Decoder = pVC1Decoder;
        m_iConsumerNumber = iConsumerNumber;
        m_iNumFramesProcessing = iMaxFramesInParallel;

        if (!m_pDSIndicate)
        {
            Ipp8u* ptr = NULL;
            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32u)*64);
            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32u)*64);
            ptr += align_value<Ipp32u>(sizeof(VC1TSHeap));

            if (m_pMemoryAllocator->Alloc(&m_iIntStructID,
                (size_t)ptr,
                UMC_ALLOC_PERSISTENT,
                16) != UMC_OK)
                return false;

            m_pDSIndicate = (Ipp32s*)(m_pMemoryAllocator->Lock(m_iIntStructID));
            memset(m_pDSIndicate,0,size_t(ptr));
            ptr = (Ipp8u*)m_pDSIndicate;

            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
            m_pDSIndicateSwap = (Ipp32s*)ptr;

            ptr +=  align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
            m_pTasksInQueue = (Ipp32u*)ptr;

            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
            m_pSlicesInQueue = (Ipp32u*)ptr;

            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
            m_pMainQueueTasksState = (Ipp32u*)ptr;

            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32u)*64);
            m_pAdditionaQueueTasksState = (Ipp32u*)ptr;

            ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32u)*64);
            m_pSHeap = (VC1TSHeap*)ptr;

        }
        //CalculateHeapSize();


// New Allocation
        {
            // Heap Allocation
            {
                Ipp32u heapSize = CalculateHeapSize();

                if (m_pMemoryAllocator->Alloc(&m_iTSHeapID,
                    heapSize,//250000*iConsumerNumber,
                    UMC_ALLOC_PERSISTENT,
                    16) != UMC_OK)
                    return false;

                m_pSHeap = new(m_pSHeap) VC1TSHeap((Ipp8u*)m_pMemoryAllocator->Lock(m_iTSHeapID),heapSize);
            }

            m_pSHeap->s_new(&m_pGuardGet,m_iNumFramesProcessing);
            m_pSHeap->s_new(&m_pGuardAdd,m_iNumFramesProcessing);
            m_pSHeap->s_new(&m_pCommonQueue,m_iNumFramesProcessing);
            m_pSHeap->s_new(&m_pAdditionalQueue,m_iNumFramesProcessing);
            //??????????
            //m_pSHeap->s_new(&m_pGuardGet,m_iNumFramesProcessing);
            //m_pSHeap->s_new(&m_pGuardAdd,m_iNumFramesProcessing);


            for (i = 0; i < m_iNumFramesProcessing; i++)
            {
                m_pDSIndicate[i] = i;
                m_pSHeap->s_new(&m_pCommonQueue[i],VC1SLICEINPARAL);
                m_pSHeap->s_new(&m_pAdditionalQueue[i],VC1SLICEINPARAL);
                m_pSHeap->s_new(&m_pGuardGet[i]);
                m_pSHeap->s_new(&m_pGuardAdd[i]);

                vm_mutex_set_invalid(m_pGuardGet[i]);
                vm_mutex_set_invalid(m_pGuardAdd[i]);
                if (VM_OK != vm_mutex_init(m_pGuardGet[i]))
                    return false;
                if (VM_OK != vm_mutex_init(m_pGuardAdd[i]))
                    return false;
            }
        }
        CreateTaskQueues();

        if (0 == vm_mutex_is_valid(&m_mDSGuard))
        {
            if (VM_OK != vm_mutex_init(&m_mDSGuard))
                return false;
        }
        m_pSHeap->s_new(&m_eWaiting,iConsumerNumber);
        // initilaize event(s)
        for (i = 0; i < iConsumerNumber; i += 1)
        {
            {
                m_pSHeap->s_new(&(m_eWaiting[i]));
                if (UMC_OK != m_eWaiting[i]->Init(0, 0))
                    return false;
            }
        }
        return true;
    }

    void VC1TaskStore::SetDefinition(VC1SequenceLayerHeader*    seqLayerHeader)
    {
        if ((seqLayerHeader->heightMB*seqLayerHeader->widthMB*16*16) > (1280*720) )
        {
            m_eStreamDef = VC1_HD_STREAM;
        }
        else if ((seqLayerHeader->heightMB*seqLayerHeader->widthMB*16*16) > (320*240))
        {
            m_eStreamDef = VC1_MD_STREAM;
        }
        else
            m_eStreamDef = VC1_SD_STREAM;


    }
    bool VC1TaskStore::Reset()
    {
        Ipp32u i = 0;

        //close
        m_bIsLastFramesMode = false;
        FreeIndexQueue();
        ResetDSQueue();

        IntIndexes.clear();
        AssocIdx.clear();

        if (vm_mutex_is_valid(&m_mDSGuard))
            vm_mutex_destroy(&m_mDSGuard);

        for (i = 0; i < m_iNumFramesProcessing; i++)
        {
            vm_mutex_destroy(m_pGuardGet[i]);
            m_pGuardGet[i] = 0;
            vm_mutex_destroy(m_pGuardAdd[i]);
            m_pGuardAdd[i] = 0;
        }
        for (i = 0; i <  m_iConsumerNumber; i++)
            m_eWaiting[i]->~Event();

        if(m_pMemoryAllocator)
        {
            if (m_pDescriptorQueue)
            {
                for (i = 0; i < m_iNumFramesProcessing; i++)
                    m_pDescriptorQueue[i]->Release();
            }

            if (static_cast<int>(m_iIntStructID) != -1)
            {
                m_pMemoryAllocator->Unlock(m_iIntStructID);
                m_pMemoryAllocator->Free(m_iIntStructID);
                m_iIntStructID = (MemID)-1;
            }

            if (static_cast<int>(m_iTSHeapID) != -1)
            {
                m_pMemoryAllocator->Unlock(m_iTSHeapID);
                m_pMemoryAllocator->Free(m_iTSHeapID);
                m_iTSHeapID = (MemID)-1;
            }

            m_pDSIndicate = 0;
            m_pDSIndicateSwap = 0;

            //Init
            // this not full initialization, need also call CreateDSQueue, CreateOutBuffersQueue,SetDefinition and FillIdxVector
            // all functions need special input params

            m_iNumDSActiveinQueue = 0;

            if (!m_pDSIndicate)
            {
                Ipp8u* ptr = NULL;
                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32u)*64);
                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32u)*64);
                ptr += align_value<Ipp32u>(sizeof(VC1TSHeap));

                if (m_pMemoryAllocator->Alloc(&m_iIntStructID,
                    (size_t)ptr,
                    UMC_ALLOC_PERSISTENT,
                    16) != UMC_OK)
                    return false;

                m_pDSIndicate = (Ipp32s*)(m_pMemoryAllocator->Lock(m_iIntStructID));
                memset(m_pDSIndicate,0,size_t(ptr));
                ptr = (Ipp8u*)m_pDSIndicate;

                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
                m_pDSIndicateSwap = (Ipp32s*)ptr;

                ptr +=  align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
                m_pTasksInQueue = (Ipp32u*)ptr;

                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
                m_pSlicesInQueue = (Ipp32u*)ptr;

                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32s));
                m_pMainQueueTasksState = (Ipp32u*)ptr;

                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32u)*64);
                m_pAdditionaQueueTasksState = (Ipp32u*)ptr;

                ptr += align_value<Ipp32u>(m_iNumFramesProcessing*sizeof(Ipp32u)*64);
                m_pSHeap = (VC1TSHeap*)ptr;

            }

            //CalculateHeapSize();
            // New Allocation
            {
                // Heap Allocation
                {
                    Ipp32u heapSize = CalculateHeapSize();

                    if (m_pMemoryAllocator->Alloc(&m_iTSHeapID,
                        heapSize,
                        UMC_ALLOC_PERSISTENT,
                        16) != UMC_OK)
                        return false;

                    m_pSHeap = new(m_pSHeap) VC1TSHeap((Ipp8u*)m_pMemoryAllocator->Lock(m_iTSHeapID),heapSize);             
                }

                m_pSHeap->s_new(&m_pGuardGet,m_iNumFramesProcessing);
                m_pSHeap->s_new(&m_pGuardAdd,m_iNumFramesProcessing);
                m_pSHeap->s_new(&m_pCommonQueue,m_iNumFramesProcessing);
                m_pSHeap->s_new(&m_pAdditionalQueue,m_iNumFramesProcessing);
 
                for (i = 0; i < m_iNumFramesProcessing; i++)
                {
                    m_pDSIndicate[i] = i;
                    m_pSHeap->s_new(&m_pCommonQueue[i],VC1SLICEINPARAL);
                    m_pSHeap->s_new(&m_pAdditionalQueue[i],VC1SLICEINPARAL);
                    m_pSHeap->s_new(&m_pGuardGet[i]);
                    m_pSHeap->s_new(&m_pGuardAdd[i]);

                    vm_mutex_set_invalid(m_pGuardGet[i]);
                    vm_mutex_set_invalid(m_pGuardAdd[i]);
                    if (VM_OK != vm_mutex_init(m_pGuardGet[i]))
                        return false;
                    if (VM_OK != vm_mutex_init(m_pGuardAdd[i]))
                        return false;
                }
            }

        }

        CreateTaskQueues();

        if (0 == vm_mutex_is_valid(&m_mDSGuard))
        {
            if (VM_OK != vm_mutex_init(&m_mDSGuard))
                return false;
        }
        m_pSHeap->s_new(&m_eWaiting,m_iConsumerNumber);
        // initilaize event(s)
        for (i = 0; i < m_iConsumerNumber; i += 1)
        {
            {
                m_pSHeap->s_new(&(m_eWaiting[i]));
                if (UMC_OK != m_eWaiting[i]->Init(0, 0))
                    return false;
            }
        }
        
        SetBFrameIndex(-1);
        SetCurrIndex(-1);
        SetRangeMapIndex(-1);
        SetPrevIndex(-1);
        SetNextIndex(-1);

        return true;
    }

    //void VC1TaskStore::ResetQueue(Ipp32u qID)
    //{
        //m_pCommonQueue[qID]->Reset();
        //m_pAdditionalQueue[qID]->Reset();
        // need to reset m_iTasksInQueue - for every queue
    //}

    inline bool VC1TaskStore::IsMainTaskPrepareForProcess(Ipp32u FrameID, Ipp32u TaskID)
    {
        return ( ((m_pMainQueueTasksState[(FrameID <<6) + (TaskID/8)] >> ((TaskID%8) << 2) )&0x7) == 0x4);

    }
    inline bool VC1TaskStore::IsAdditionalTaskPrepareForProcess(Ipp32u FrameID, Ipp32u TaskID)
    {
        return ( ((m_pAdditionaQueueTasksState[(FrameID <<6) + (TaskID/8)] >> ((TaskID%8) << 2) )&0x7) == 0x4);
    }
    inline bool VC1TaskStore::IsFrameReadyToDisplay(Ipp32u FrameID)
    {
        Ipp32u NumDwordsForTasks = m_pTasksInQueue[FrameID]/8 + 1;
        Ipp32u RemainTasksMask = FrameReadyTable[m_pTasksInQueue[FrameID]%8];
        Ipp32u i;
        for (i = (FrameID << 6); i < (FrameID << 6) + NumDwordsForTasks - 1; i++)
        {
            if ((FrameReadyTable[8] != m_pMainQueueTasksState[i])||
                (FrameReadyTable[8] != m_pAdditionaQueueTasksState[i]))
                return false;
        }
        if ((RemainTasksMask == (m_pMainQueueTasksState[i])&&
           (RemainTasksMask == (m_pAdditionaQueueTasksState[i]))))
           return true;
        else
            return false;

    }
    // main queue processing
    inline void VC1TaskStore::SetTaskAsReady_MQ(Ipp32u FrameID, Ipp32u TaskID)
    {
        Ipp32u *pCurrentDword = &m_pMainQueueTasksState[(FrameID <<6) + (TaskID/8)];
        *pCurrentDword |= 0x4 << ((TaskID%8) << 2);

    }
    inline void VC1TaskStore::SetTaskAsNotReady_MQ(Ipp32u FrameID, Ipp32u TaskID)
    {
        Ipp32u *pCurrentDword = &m_pMainQueueTasksState[(FrameID <<6) + (TaskID/8)];
        *pCurrentDword &= 0xFFFFFFFF - (0x4 << ((TaskID%8) << 2));

    }
    inline void VC1TaskStore::SetTaskAsDone_MQ(Ipp32u FrameID, Ipp32u TaskID)
    {
        Ipp32u *pCurrentDword = &m_pMainQueueTasksState[(FrameID <<6) + (TaskID/8)];
        *pCurrentDword |= 0x1 << ((TaskID%8) << 2);
    }
    inline void VC1TaskStore::EnableProcessBit_MQ(Ipp32u FrameID, Ipp32u TaskID)
    {
        Ipp32u *pCurrentDword = &m_pMainQueueTasksState[(FrameID <<6) + (TaskID/8)];
        *pCurrentDword |= 0x2 << ((TaskID%8) << 2);
    }
    inline void VC1TaskStore::DisableProcessBit_MQ(Ipp32u FrameID, Ipp32u TaskID)
    {
        Ipp32u *pCurrentDword = &m_pMainQueueTasksState[(FrameID <<6) + (TaskID/8)];
        *pCurrentDword &= 0xFFFFFFFF - (0x2 << ((TaskID%8) << 2));
    }

    // additional queue processing
    inline void VC1TaskStore::SetTaskAsReady_AQ(Ipp32u FrameID, Ipp32u TaskID)
    {
        Ipp32u *pCurrentDword = &m_pAdditionaQueueTasksState[(FrameID <<6) + (TaskID/8)];
        *pCurrentDword |= 0x4 << ((TaskID%8) << 2);

    }
    inline void VC1TaskStore::SetTaskAsNotReady_AQ(Ipp32u FrameID, Ipp32u TaskID)
    {
        Ipp32u *pCurrentDword = &m_pAdditionaQueueTasksState[(FrameID <<6) + (TaskID/8)];
        *pCurrentDword &= 0xFFFFFFFF - (0x4 << ((TaskID%8) << 2));

    }
    inline void VC1TaskStore::SetTaskAsDone_AQ(Ipp32u FrameID, Ipp32u TaskID)
    {
        Ipp32u *pCurrentDword = &m_pAdditionaQueueTasksState[(FrameID <<6) + (TaskID/8)];
        *pCurrentDword |= 0x1 << ((TaskID%8) << 2);
    }
    inline void VC1TaskStore::EnableProcessBit_AQ(Ipp32u FrameID, Ipp32u TaskID)
    {
        Ipp32u *pCurrentDword = &m_pAdditionaQueueTasksState[(FrameID <<6) + (TaskID/8)];
        *pCurrentDword |= 0x2 << ((TaskID%8) << 2);
    }
    inline void VC1TaskStore::DisableProcessBit_AQ(Ipp32u FrameID, Ipp32u TaskID)
    {
        Ipp32u *pCurrentDword = &m_pAdditionaQueueTasksState[(FrameID <<6) + (TaskID/8)];
        *pCurrentDword &= 0xFFFFFFFF - (0x2 << ((TaskID%8) << 2));
    }

    Ipp32u VC1TaskStore::CalculateHeapSize()
    {

        Ipp32u Size = 0;
        Ipp32u counter = 0;
        Ipp32u counter2 = 0;

        Size += align_value<Ipp32u>(sizeof(vm_mutex**)*m_iNumFramesProcessing); //m_pGuardGet
        Size += align_value<Ipp32u>(sizeof(vm_mutex**)*m_iNumFramesProcessing); //m_pGuardAdd
        Size += align_value<Ipp32u>(sizeof(VC1Task***)*m_iNumFramesProcessing); //m_pCommonQueue
        Size += align_value<Ipp32u>(sizeof(VC1Task***)*m_iNumFramesProcessing); //m_pAdditionalQueue

        Size += align_value<Ipp32u>(sizeof(VC1FrameDescriptor*)*(m_iNumFramesProcessing));

        for (counter = 0; counter < m_iNumFramesProcessing; counter++)
        {
            Size += align_value<Ipp32u>(sizeof(VC1Task**)*VC1SLICEINPARAL); //m_pCommonQueue
            Size += align_value<Ipp32u>(sizeof(VC1Task**)*VC1SLICEINPARAL); //m_pAdditionalQueue
            Size += align_value<Ipp32u>(sizeof(vm_mutex)); //m_pGuardGet
            Size += align_value<Ipp32u>(sizeof(vm_mutex)); //m_pGuardAdd
#ifndef UMC_RESTRICTED_CODE_VA
#ifdef UMC_VA_DXVA
            if (pMainVC1Decoder->m_va)
            {
                if (pMainVC1Decoder->m_va->IsIntelCustomGUID())
                {
                    if(pMainVC1Decoder->m_va->GetProtectedVA())
                        Size += align_value<Ipp32u>(sizeof(VC1FrameDescriptorVA_Protected<VC1PackerDXVA_Protected>));
                    else
                        Size += align_value<Ipp32u>(sizeof(VC1FrameDescriptorVA_EagleLake<VC1PackerDXVA_EagleLake>));
                }
                else
                    Size += align_value<Ipp32u>(sizeof(VC1FrameDescriptorVA<VC1PackerDXVA>));
            }
            else
#endif
#ifdef UMC_VA_LINUX
            if (pMainVC1Decoder->m_va)
            {
                Size += align_value<Ipp32u>(sizeof(VC1FrameDescriptorVA_Linux<VC1PackerLVA>));
            }
            else
#endif
#endif
            Size += align_value<Ipp32u>(sizeof(VC1FrameDescriptor));
        }

        for (counter=0; counter < m_iNumFramesProcessing; counter++)
        {
            for (counter2 = 0; counter2 < VC1SLICEINPARAL; counter2++)
            {
                Size += align_value<Ipp32u>(sizeof(VC1Task)); //m_pCommonQueue
                Size += align_value<Ipp32u>(sizeof(SliceParams));
                Size += align_value<Ipp32u>(sizeof(VC1Task)); //m_pAdditionalQueue
                Size += align_value<Ipp32u>(sizeof(SliceParams));
            }
        }

        Size += align_value<Ipp32u>(sizeof(Event**)*m_iConsumerNumber);

        for (counter = 0; counter < m_iConsumerNumber; counter++)
        {
            Size += align_value<Ipp32u>(sizeof(Event));
        }

        Size += align_value<Ipp32u>(sizeof(VC1FrameIndexProc**)*(m_iNumFramesProcessing+2));

        for (counter = 0; counter < m_iNumFramesProcessing+2; counter++)
        {
            Size += align_value<Ipp32u>(sizeof(VC1FrameIndexProc));
        }



        return Size;
    }


    VC1PictureLayerHeader* VC1TaskStore::GetFirstInSecondField(Ipp32u qID)
    {
        for (Ipp32u i = 0; i < m_pTasksInQueue[qID]; i += 1)
        {
            if ((m_pCommonQueue[qID])[i]->m_isFirstInSecondSlice)
                return (m_pCommonQueue[qID])[i]->m_pSlice->m_picLayerHeader;
        }
        return NULL;
    }
    bool VC1TaskStore::CreateTaskQueues()
    {
        Ipp32u i,j;
        Ipp8u* pBuf;
        for (j=0; j < m_iNumFramesProcessing; j++)
        {
            for (i=0; i < VC1SLICEINPARAL; i++)
            {
                pBuf = m_pSHeap->s_alloc<VC1Task>();
                if (pBuf)
                {
                    m_pCommonQueue[j][i] = new(pBuf) VC1Task(0,i);
                }
                m_pSHeap->s_new(&(m_pCommonQueue[j][i]->m_pSlice));
                memset(m_pCommonQueue[j][i]->m_pSlice, 0, sizeof(SliceParams));


                pBuf = m_pSHeap->s_alloc<VC1Task>();
                if (pBuf)
                {
                    m_pAdditionalQueue[j][i] = new(pBuf) VC1Task(0,i);
                }
                m_pSHeap->s_new(&(m_pAdditionalQueue[j][i]->m_pSlice));
                memset(m_pAdditionalQueue[j][i]->m_pSlice, 0, sizeof(SliceParams));

            }
        }
        return true;

    }
    //void VC1TaskStore::ReleaseTaskQueues()
    //{
    //    Ipp32u i,j;
    //    for (j=0; j < m_iNumFramesProcessing; j++)
    //    {
    //        for (i=0; i < VC1SLICEINPARAL; i++)
    //        {
    //            // main queue
    //           if ((m_pCommonQueue[j])[i])
    //           {
    //               if ((m_pCommonQueue[j])[i]->m_pSlice)
    //               {
    //                   delete((m_pCommonQueue[j])[i]->m_pSlice);
    //                   (m_pCommonQueue[j])[i]->m_pSlice = NULL;
    //               }
    //           }

    //           // add queue
    //               if ((m_pAdditionalQueue[j])[i])
    //               {
    //                   if ((m_pAdditionalQueue[j])[i]->m_pSlice)
    //                   {
    //                       delete((m_pAdditionalQueue[j])[i]->m_pSlice);
    //                       (m_pAdditionalQueue[j])[i]->m_pSlice = NULL;
    //                   }
    //               }

    //        }
    //    }
    //}


    bool VC1TaskStore::AddSampleTask(VC1Task* _pTask, Ipp32u qID)
    {
        Ipp32u widthMB = m_pDescriptorQueue[qID]->m_pContext->m_seqLayerHeader.MaxWidthMB;
        Ipp16u curMBrow = _pTask->m_pSlice->MBStartRow;

        if (0 == curMBrow)
        {
            m_pTasksInQueue[qID] = 0;
            m_pSlicesInQueue[qID] = 0;
            memset(m_pMainQueueTasksState + (qID << 6),0,64*sizeof(Ipp32u));
            memset(m_pAdditionaQueueTasksState + (qID << 6),0,64*sizeof(Ipp32u));
        }


        bool isFirstSlieceDecodeTask = true;
        //VM_ASSERT(qID < m_iConsumerNumber);

        while (curMBrow  < _pTask->m_pSlice->MBEndRow)
        {
            //VC1Task* pTask = new VC1Task(_pTask->m_iThreadNumber,m_pTasksInQueue[qID]);
            //pTask->m_pSlice = (SliceParams*)ippsMalloc_8u(sizeof(SliceParams));
            VC1Task* pTask = (m_pCommonQueue[qID])[m_pTasksInQueue[qID]];
            //pTask->m_bDone = false;
            //pTask->m_bInProgress = false;
            //pTask->m_bIsReady = false;
            pTask->m_eTasktype = VC1Decode;
            pTask->m_isFirstInSecondSlice = false;
            pTask->m_isFieldReady = false;

            memset(pTask->m_pSlice, 0, sizeof(SliceParams));

            _pTask->m_pSlice->is_NewInSlice = isFirstSlieceDecodeTask;

            if (isFirstSlieceDecodeTask)
            {
                if (m_pSlicesInQueue[qID] < VC1SLICEINPARAL)
                {
                    //pTask->m_bIsReady = true;
                    SetTaskAsReady_MQ(qID,m_pTasksInQueue[qID]);

                }

                isFirstSlieceDecodeTask = false;

                if (_pTask->m_isFirstInSecondSlice)
                {
                    m_pDescriptorQueue[qID]->m_iActiveTasksInFirstField = m_pTasksInQueue[qID] - 1;
                    pTask->m_isFirstInSecondSlice = _pTask->m_isFirstInSecondSlice;
                }
            }

            pTask->m_isFieldReady = _pTask->m_isFieldReady;

            *pTask->m_pSlice = *_pTask->m_pSlice;
            pTask->m_pSlice->MBStartRow = curMBrow;
            pTask->m_pSlice->MBEndRow = curMBrow + VC1MBQUANT;
            pTask->m_pSlice->MBRowsToDecode = VC1MBQUANT;
            pTask->pMulti = &VC1TaskProcessorUMC::VC1Decoding;


            if ((pTask->m_pSlice->MBRowsToDecode + pTask->m_pSlice->MBStartRow) > _pTask->m_pSlice->MBEndRow)
            {
                pTask->m_pSlice->MBEndRow = _pTask->m_pSlice->MBEndRow;
                pTask->m_pSlice->MBRowsToDecode = pTask->m_pSlice->MBEndRow - pTask->m_pSlice->MBStartRow;
            }
            (m_pCommonQueue[qID])[m_pTasksInQueue[qID]] = pTask;
            (m_pCommonQueue[qID])[m_pTasksInQueue[qID]]->m_pBlock = m_pDescriptorQueue[qID]->m_pDiffMem + widthMB*pTask->m_pSlice->MBStartRow*8*8*6;
            (m_pCommonQueue[qID])[m_pTasksInQueue[qID]]->m_pSlice->is_LastInSlice = false;
            ++m_pTasksInQueue[qID];
            curMBrow += VC1MBQUANT;
            ++m_iCurrentTaskID;
        }
        (m_pCommonQueue[qID])[m_pTasksInQueue[qID]-1]->m_pSlice->is_LastInSlice = true;
        ++m_pSlicesInQueue[qID];

       return true;
    }
    void VC1TaskStore::DistributeTasks(Ipp32u qID)
    {
//        Ipp32u widthMB = m_pDescriptorQueue[qID]->m_pContext->m_seqLayerHeader.widthMB;
        //AutomaticMutex guard(*m_pGuardGet[0]);
        for (Ipp32u i = 0; i < m_pTasksInQueue[qID]; i++)
        {
            //VC1Task* pTask = new VC1Task(0,(m_pCommonQueue[qID])[i]->m_iTaskID);
            //pTask->m_pSlice = (SliceParams*)ippsMalloc_8u(sizeof(SliceParams));
            VC1Task* pTask = (m_pAdditionalQueue[qID])[i];
            //pTask->m_bDone = false;
            //pTask->m_bInProgress = false;
            //pTask->m_bIsReady = false;
            pTask->m_isFirstInSecondSlice = false;
            pTask->m_isFieldReady = false;

            memset(pTask->m_pSlice, 0, sizeof(SliceParams));
            *(pTask->m_pSlice) = *((m_pCommonQueue[qID])[i]->m_pSlice);

            pTask->m_isFirstInSecondSlice = (m_pCommonQueue[qID])[i]->m_isFirstInSecondSlice;


            if (pTask->m_pSlice->is_NewInSlice)
                pTask->m_pSlice->iPrevDblkStartPos = -1;
            else
                pTask->m_pSlice->iPrevDblkStartPos = (m_pCommonQueue[qID])[i-1]->m_pSlice->MBStartRow-1;
            if (i > 0)
            {
                if ((m_pCommonQueue[qID])[(i-1)]->m_pSlice->is_NewInSlice)
                {
                    pTask->m_pSlice->iPrevDblkStartPos += 1;
                }
            }

            (m_pAdditionalQueue[qID])[i] = pTask;
            (m_pAdditionalQueue[qID])[i]->m_pBlock = (m_pCommonQueue[qID])[i]->m_pBlock;
            (m_pAdditionalQueue[qID])[i]->m_isFieldReady = (m_pCommonQueue[qID])[i]->m_isFieldReady;
            (m_pAdditionalQueue[qID])[i]->m_isFirstInSecondSlice = (m_pCommonQueue[qID])[i]->m_isFirstInSecondSlice;

            switch((m_pCommonQueue[qID])[i]->m_pSlice->m_picLayerHeader->PTYPE)
            {
            case VC1_B_FRAME:
                (m_pAdditionalQueue[qID])[i]->pMulti = &VC1TaskProcessorUMC::VC1MVCalculation;
                (m_pAdditionalQueue[qID])[i]->m_eTasktype = VC1MVCalculate;
                break;
            case VC1_P_FRAME:
                (m_pAdditionalQueue[qID])[i]->pMulti = &VC1TaskProcessorUMC::VC1MotionCompensation;
                (m_pAdditionalQueue[qID])[i]->m_eTasktype = VC1MC;
                break;
            case VC1_I_FRAME:
            case VC1_BI_FRAME:
                (m_pAdditionalQueue[qID])[i]->pMulti = &VC1TaskProcessorUMC::VC1PrepPlane;
                (m_pAdditionalQueue[qID])[i]->m_eTasktype = VC1PreparePlane;
                break;
            default:
                break;
            }
            ++m_iCurrentTaskID;
        }
    }

    bool VC1TaskStore::GetNextTask(VC1FrameDescriptor **pFrameDS, VC1Task** pTask, Ipp32u qID)
    {
        if (m_iConsumerNumber <= 2)
        {
                // intensity compensation and H/W accelerator are special cases
                if (pMainVC1Decoder->m_pContext->m_bIntensityCompensation)
                {
                    return GetNextTaskManyCPU_HD(pFrameDS,pTask,qID);
                }
                else
                    return GetNextTaskDualCPU(pFrameDS,pTask,qID);
        }
        else if (m_iConsumerNumber <= 4)
        {
            if ((m_eStreamDef == VC1_HD_STREAM)||(m_eStreamDef == VC1_SD_STREAM))
            {
                return GetNextTaskManyCPU_HD(pFrameDS,pTask,qID);
            }
            else
                return GetNextTaskManyCPU_MD(pFrameDS,pTask,qID);
        }
        else
            return GetNextTaskManyCPU_HD(pFrameDS,pTask,qID);

    }
    bool VC1TaskStore::GetNextTaskDualCPU(VC1FrameDescriptor **pFrameDS, VC1Task** pTask, Ipp32u qID)
    {
STATISTICS_START_TIME(m_timeStatistics->GetNextTask_StartTime);

        Ipp32u curFrame = qID;
        Ipp32u frameCount;
        Ipp32u i;
        for (Ipp32u count = 0; count < m_iNumFramesProcessing; count++) // for 2-core CPU
        {
            frameCount = m_pDSIndicate[curFrame];
STATISTICS_START_TIME(m_timeStatistics->GetNextTask_StartTime);
            {
            AutomaticMutex guard(*m_pGuardGet[frameCount]);

            //find in own queue
            if (m_pDescriptorQueue[frameCount]->m_bIsReadyToProcess)
            {
                *pFrameDS = m_pDescriptorQueue[frameCount];
                // find in main queue
                for (i =0; i < m_pTasksInQueue[frameCount];i++ )
                {
                    if (IsMainTaskPrepareForProcess(frameCount,i))
                    {
                            *pTask = (m_pCommonQueue[frameCount])[i];
                            //(m_pCommonQueue[frameCount])[i]->m_bInProgress = true;

                            EnableProcessBit_MQ(frameCount,i);

                            STATISTIC_PARAL_ENTRY(pTask,pTask->m_eTasktype,VC1GetTask);
                            STATISTICS_END_TIME(m_timeStatistics->GetNextTask_StartTime,
                                m_timeStatistics->GetNextTask_EndTime,
                                m_timeStatistics->GetNextTask_TotalTime);
                            return true;
                    }
                    else if (IsAdditionalTaskPrepareForProcess(frameCount,i))
                    {
                        *pTask = (m_pAdditionalQueue[frameCount])[i];
                        //(m_pAdditionalQueue[frameCount])[i]->m_bInProgress = true;
                        EnableProcessBit_AQ(frameCount,i);

STATISTIC_PARAL_ENTRY(pTask,pTask->m_eTasktype,VC1AddPerfomrd);
STATISTICS_END_TIME(m_timeStatistics->GetNextTask_StartTime,
                    m_timeStatistics->GetNextTask_EndTime,
                    m_timeStatistics->GetNextTask_TotalTime);
                        return true;
                    }
                }
                //for (i =0; i < m_pTasksInQueue[frameCount];i++ )
                //{
                //    if (((m_pCommonQueue[frameCount])[i]->m_bDone == false)||
                //        ((m_pAdditionalQueue[frameCount])[i]->m_bDone == false) )
                //        break;
                //}
                if (IsFrameReadyToDisplay(frameCount))
                {
                    m_pDescriptorQueue[frameCount]->m_bIsReadyToProcess = false;
                    m_pDescriptorQueue[frameCount]->m_bIsReadyToDisplay = true;
                    *pTask = NULL;
                    return false;
                }
            }
            guard.Unlock();
            }

            ++curFrame;
            if (curFrame == m_iNumFramesProcessing)
                curFrame = 0;

        }
        STATISTICS_END_TIME(m_timeStatistics->GetNextTask_StartTime,
                            m_timeStatistics->GetNextTask_EndTime,
                            m_timeStatistics->GetNextTask_TotalTime);

        *pTask = NULL;
        return true;
    }

    bool VC1TaskStore::GetNextTaskManyCPU_HD(VC1FrameDescriptor **pFrameDS, VC1Task** pTask, Ipp32u qID)
    {
        STATISTICS_START_TIME(m_timeStatistics->GetNextTask_StartTime);

        Ipp32u StartFrame = (m_iConsumerNumber <= 2)?0:1;
        Ipp32s frameCount = m_pPrefDS->m_iSelfID;


        Ipp32u i =0;
        // Get Task from First frame in Queue
        if (((qID<(m_iConsumerNumber>>1)))||(m_iConsumerNumber <= 2))
        {
            AutomaticMutex guard(*m_pGuardGet[frameCount]);
            if (m_pDescriptorQueue[frameCount]->m_bIsReadyToProcess)
            {
                *pFrameDS = m_pDescriptorQueue[frameCount];
                // find in main queue
                for (i =0; i < m_pTasksInQueue[frameCount];i++ )
                {
                    if (IsMainTaskPrepareForProcess(frameCount,i))
                    {
                        *pTask = (m_pCommonQueue[frameCount])[i];
                        EnableProcessBit_MQ(frameCount,i);
                        //(m_pCommonQueue[frameCount])[i]->m_bInProgress = true;
                        //(m_pCommonQueue[frameCount])[i]->m_bIsReady = true;

                        STATISTIC_PARAL_ENTRY(pTask,pTask->m_eTasktype,VC1GetTask);
                        STATISTICS_END_TIME(m_timeStatistics->GetNextTask_StartTime,
                            m_timeStatistics->GetNextTask_EndTime,
                            m_timeStatistics->GetNextTask_TotalTime);
                        return true;
                    }
                    else if (IsAdditionalTaskPrepareForProcess(frameCount,i))
                    {
                        *pTask = (m_pAdditionalQueue[frameCount])[i];
                        EnableProcessBit_AQ(frameCount,i);
                        //(m_pAdditionalQueue[frameCount])[i]->m_bInProgress = true;
                        //(m_pAdditionalQueue[frameCount])[i]->m_bIsReady = true;

                        STATISTIC_PARAL_ENTRY(pTask,pTask->m_eTasktype,VC1AddPerfomrd);
                        STATISTICS_END_TIME(m_timeStatistics->GetNextTask_StartTime,
                            m_timeStatistics->GetNextTask_EndTime,
                            m_timeStatistics->GetNextTask_TotalTime);
                        return true;
                    }
                }
            }


            if (m_pDescriptorQueue[frameCount]->m_bIsReadyToProcess &&
                IsFrameReadyToDisplay(frameCount))
            {
                m_pDescriptorQueue[frameCount]->m_bIsReadyToProcess = false;
                m_pDescriptorQueue[frameCount]->m_bIsReadyToDisplay = true;
                *pTask = NULL;
                STATISTICS_END_TIME(m_timeStatistics->GetNextTask_StartTime,
                    m_timeStatistics->GetNextTask_EndTime,
                    m_timeStatistics->GetNextTask_TotalTime);
                return false;
            }
            guard.Unlock();
        }

        Ipp32u curFrame = qID;
        for (Ipp32u count = StartFrame; count < m_iNumFramesProcessing; count++) // for 2-core CPU
        {
            frameCount = m_pDSIndicate[count];
            STATISTICS_START_TIME(m_timeStatistics->GetNextTask_StartTime);
            //if (curPriority == m_pDescriptorQueue[frameCount]->m_iPriority)
            {
                AutomaticMutex guard(*m_pGuardGet[frameCount]);

                //find in own queue
                if (m_pDescriptorQueue[frameCount]->m_bIsReadyToProcess)
                {
                    *pFrameDS = m_pDescriptorQueue[frameCount];
                    // find in main queue
                    for (i =0; i < m_pTasksInQueue[frameCount];i++ )
                    {
                        if (IsMainTaskPrepareForProcess(frameCount,i))
                        {
                            *pTask = (m_pCommonQueue[frameCount])[i];
                            EnableProcessBit_MQ(frameCount,i);
                            //(m_pCommonQueue[frameCount])[i]->m_bInProgress = true;
                            //(m_pCommonQueue[frameCount])[i]->m_bIsReady = true;

                            STATISTIC_PARAL_ENTRY(pTask,pTask->m_eTasktype,VC1GetTask);
                            STATISTICS_END_TIME(m_timeStatistics->GetNextTask_StartTime,
                                m_timeStatistics->GetNextTask_EndTime,
                                m_timeStatistics->GetNextTask_TotalTime);
                            return true;
                        }
                        else if (IsAdditionalTaskPrepareForProcess(frameCount,i))
                        {
                            *pTask = (m_pAdditionalQueue[frameCount])[i];
                            EnableProcessBit_AQ(frameCount,i);
                            //(m_pAdditionalQueue[frameCount])[i]->m_bInProgress = true;
                            //(m_pAdditionalQueue[frameCount])[i]->m_bIsReady = true;

                            STATISTIC_PARAL_ENTRY(pTask,pTask->m_eTasktype,VC1AddPerfomrd);
                            STATISTICS_END_TIME(m_timeStatistics->GetNextTask_StartTime,
                                m_timeStatistics->GetNextTask_EndTime,
                                m_timeStatistics->GetNextTask_TotalTime);
                            return true;
                        }
                    }
                    if (IsFrameReadyToDisplay(frameCount))
                    {
                        m_pDescriptorQueue[frameCount]->m_bIsReadyToProcess = false;
                        m_pDescriptorQueue[frameCount]->m_bIsReadyToDisplay = true;
                        *pTask = NULL;
                        STATISTICS_END_TIME(m_timeStatistics->GetNextTask_StartTime,
                            m_timeStatistics->GetNextTask_EndTime,
                            m_timeStatistics->GetNextTask_TotalTime);
                        return false;
                    }
                }
                guard.Unlock();
            }
            ++curFrame;
            if (curFrame > m_iNumFramesProcessing)
                curFrame = 0;

        }
        STATISTICS_END_TIME(m_timeStatistics->GetNextTask_StartTime,
            m_timeStatistics->GetNextTask_EndTime,
            m_timeStatistics->GetNextTask_TotalTime);

        *pTask = NULL;
        return true;
    }
    bool VC1TaskStore::GetNextTaskManyCPU_MD(VC1FrameDescriptor **pFrameDS, VC1Task** pTask, Ipp32u qID)
    {
        //same pipeline as for Dual case
        return GetNextTaskDualCPU(pFrameDS,pTask,qID);

    }
    bool VC1TaskStore::GetNextTaskMainThread(VC1FrameDescriptor **pFrameDS, VC1Task** pTask, Ipp32u qID, bool &isFrameComplete)
    {
        if (IsPerfomedDS())
        {
            isFrameComplete = true;
            return false;
        }
        return GetNextTask(pFrameDS,pTask,qID);
    }
bool VC1TaskStore::AddPerfomedTask(VC1Task* pTask, VC1FrameDescriptor *pFrameDS)
    {
STATISTICS_START_TIME(m_timeStatistics->AddPerfomed_StartTime);
         Ipp32u qID = pFrameDS->m_iSelfID;
        //AutomaticMutex guard(*m_pGuardGet[qID]);

        // check error(s)
        if ((NULL == pTask) || (0 >= m_pTasksInQueue[qID])) //!!!!!!!!!!
            return false;
        VC1TaskTypes NextStateTypeofTask;
        Ipp32u i = pTask->m_iTaskID;

        if (pTask->m_eTasktype <= VC1Reconstruct)
        {

                    //AutomaticMutex guard(*m_pGuardGet[qID]);
                    AutomaticMutex guard(*m_pGuardGet[qID]);
                    NextStateTypeofTask = (m_pCommonQueue[qID])[i]->switch_task();
                    switch (NextStateTypeofTask)
                    {
                    case VC1Reconstruct:
                        //(m_pCommonQueue[qID])[i]->m_bIsReady = true;
                        SetTaskAsReady_MQ(qID, i);
                        if ((i+1) < m_pTasksInQueue[qID])
                        {
                            if (!(m_pCommonQueue[qID])[i+1]->m_pSlice->is_NewInSlice)
                            {
                                (m_pCommonQueue[qID])[i+1]->m_pSlice->m_pstart = pTask->m_pSlice->m_pstart;
                                (m_pCommonQueue[qID])[i+1]->m_pSlice->m_bitOffset = pTask->m_pSlice->m_bitOffset;
                                (m_pCommonQueue[qID])[i+1]->m_pSlice->EscInfo = pTask->m_pSlice->EscInfo;
                                //(m_pCommonQueue[qID])[i+1]->m_bIsReady = true;
                                SetTaskAsReady_MQ(qID, i+1);
                            }
                        }
                        if ((m_pDescriptorQueue[qID]->m_bIsReferenceReady)&&
                            ((m_pAdditionalQueue[qID])[i]->m_isFieldReady))
                        {
                            switch((m_pAdditionalQueue[qID])[i]->m_pSlice->m_picLayerHeader->PTYPE)
                            {
                            case VC1_P_FRAME:
                                //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                                 SetTaskAsReady_AQ(qID, i);
                                break;
                            case VC1_B_FRAME:
                                if (i > 0)
                                {
                                    if (((m_pAdditionalQueue[qID])[i]->m_pSlice->is_NewInSlice)||
                                        ((m_pAdditionalQueue[qID])[i-1]->m_eTasktype > VC1MVCalculate ))
                                    {
                                        //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                                        SetTaskAsReady_AQ(qID, i);
                                    }
                                }
                                else
                                {
                                    //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                                    SetTaskAsReady_AQ(qID, i);
                                }
                                break;
                            default:
                                break;
                            }
                        }
                        break;
                    case VC1Complete:
                        if ((m_pAdditionalQueue[qID])[i]->m_eTasktype == VC1PreparePlane)
                        {
                            (m_pAdditionalQueue[qID])[i]->m_pBlock = (m_pCommonQueue[qID])[i]->m_pBlock;
                            if (i>0)
                            {
                                if (((m_pAdditionalQueue[qID])[i-1]->m_eTasktype > VC1PreparePlane)||
                                    ((m_pCommonQueue[qID])[i]->m_pSlice->is_NewInSlice))
                                {
                                    //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                                    SetTaskAsReady_AQ(qID, i);
                                }
                            }
                            else
                            {
                                //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                                SetTaskAsReady_AQ(qID, i);

                            }

                        }
                        //(m_pCommonQueue[qID])[i]->m_bDone = true;
                        SetTaskAsDone_MQ(qID, i);
                        //printf("finish %d task \n", i);
                        break;
                    default:
                        VM_ASSERT(0);

                    }
                    //printf("complete %d task \n", i);
STATISTIC_PARAL_ENTRY(pTask,pTask->m_eTasktype,VC1AddPerfomrd);

                    //(m_pCommonQueue[qID])[i]->m_bInProgress = false;
                    DisableProcessBit_MQ(qID,i);

STATISTICS_END_TIME(m_timeStatistics->AddPerfomed_StartTime,
                    m_timeStatistics->AddPerfomed_EndTime,
                    m_timeStatistics->AddPerfomed_TotalTime);
                    guard.Unlock();
                    //WakeUP();
                    return true;
        }
        else
        {
                    AutomaticMutex guard(*m_pGuardGet[qID]);
                    NextStateTypeofTask = (m_pAdditionalQueue[qID])[i]->switch_task();
                    switch (NextStateTypeofTask)
                    {
                    case VC1MC:
                        //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                        SetTaskAsReady_AQ(qID, i);
                        if ((i+1) < m_pTasksInQueue[qID])
                        {
                            if ( ((m_pCommonQueue[qID])[i+1]->m_eTasktype > VC1Decode)&&
                                 (!(m_pCommonQueue[qID])[i+1]->m_pSlice->is_NewInSlice)&&
                                ((m_pAdditionalQueue[qID])[i+1]->m_isFieldReady))
                            {
                                //(m_pAdditionalQueue[qID])[i+1]->m_bIsReady = true;
                                SetTaskAsReady_AQ(qID, i+1);
                            }
                        }
                        break;
                    case VC1PreparePlane:
                        if ((VC1Complete == (m_pCommonQueue[qID])[i]->m_eTasktype) )
                        {
                            (m_pAdditionalQueue[qID])[i]->m_pBlock = (m_pCommonQueue[qID])[i]->m_pBlock;
                            if (i > 0)
                            {
                                if ( ((m_pAdditionalQueue[qID])[i-1]->m_eTasktype > VC1PreparePlane)||
                                    ((m_pCommonQueue[qID])[i]->m_pSlice->is_NewInSlice))
                                {

                                    //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                                    SetTaskAsReady_AQ(qID, i);
                                }

                                else
                                {
                                    //(m_pAdditionalQueue[qID])[i]->m_bIsReady = false;
                                    SetTaskAsNotReady_AQ(qID, i);
                                }

                            }
                            else
                            {
                                //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                                SetTaskAsReady_AQ(qID, i);
                            }

                        } else
                        {
                            //(m_pAdditionalQueue[qID])[i]->m_bIsReady = false;
                            SetTaskAsNotReady_AQ(qID, i);
                        }

                        break;
                    case VC1Deblock:

                        if ((i+1) < m_pTasksInQueue[qID])
                        {

                           /* if (!(m_pAdditionalQueue[qID])[i+1]->m_pSlice->is_NewInSlice)
                                (m_pAdditionalQueue[qID])[i+1]->m_pSlice->smooth_info= pTask->m_pSlice->smooth_info;*/

                            if (((m_pAdditionalQueue[qID])[i+1]->m_eTasktype == VC1PreparePlane)&&
                                ((m_pCommonQueue[qID])[i+1]->m_eTasktype == VC1Complete)&&
                                (!(m_pCommonQueue[qID])[i+1]->m_pSlice->is_NewInSlice))
                            {
                                //(m_pAdditionalQueue[qID])[i+1]->m_bIsReady = true;
                                SetTaskAsReady_AQ(qID, i+1);
                            }


                        }

                        if (pTask->m_pSlice->m_picLayerHeader->FCM == VC1_FrameInterlace) // In case of Interlace Frames slices are depends
                        {
                            Ipp8u DeblockMask = 0;  // 0 bit - Down edge, 1 bit - UP edge
                            if (i > 0)
                            {
                                if ((m_pAdditionalQueue[qID])[i]->m_pSlice->is_LastInSlice)
                                {
                                    //if ((m_pAdditionalQueue[qID])[i+1])
                                    if ((i+1) < m_pTasksInQueue[qID])
                                    {
                                        if ((m_pAdditionalQueue[qID])[i+1]->m_eTasktype == VC1Deblock)
                                            DeblockMask |= 1;
                                    } else
                                        DeblockMask |= 1;
                                } else
                                    DeblockMask |= 1;

                                if ((m_pAdditionalQueue[qID])[i-1]->m_eTasktype == VC1Complete)
                                    DeblockMask |= 2;

                                if ((m_pAdditionalQueue[qID])[i]->m_pSlice->is_NewInSlice)
                                {
                                    if (i > 1)
                                    {
                                        if (((m_pAdditionalQueue[qID])[i-1]->m_eTasktype == VC1Deblock)&&
                                            ((m_pAdditionalQueue[qID])[i-2]->m_eTasktype == VC1Complete))
                                        {
                                            //(m_pAdditionalQueue[qID])[i-1]->m_bIsReady = true;
                                            SetTaskAsReady_AQ(qID, i-1);
                                        }
                                    }
                                    else if ((m_pAdditionalQueue[qID])[i-1]->m_eTasktype == VC1Deblock)
                                    {
                                         //(m_pAdditionalQueue[qID])[i-1]->m_bIsReady = true;
                                         SetTaskAsReady_AQ(qID, i-1);
                                    }
                                }


                            } else
                            {
                                if ((m_pAdditionalQueue[qID])[i]->m_pSlice->is_LastInSlice)
                                {
                                    if ((i+1) < m_pTasksInQueue[qID])
                                    {
                                        if ((m_pAdditionalQueue[qID])[i+1]->m_eTasktype == VC1Deblock)
                                            DeblockMask = 3;
                                    } else
                                        DeblockMask = 3;
                                } else
                                    DeblockMask = 3;

                            }

                            if (3 == DeblockMask)
                            {
                                //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                                SetTaskAsReady_AQ(qID, i);
                            }
                            else
                            {
                                //(m_pAdditionalQueue[qID])[i]->m_bIsReady = false;
                                SetTaskAsNotReady_AQ(qID, i);
                            }
                        }
                        else
                        {
                            if (i > 0)
                            {
                                if ((m_pAdditionalQueue[qID])[i-1]->m_eTasktype == VC1Complete)//||
                                    //((m_pAdditionalQueue[qID])[i]->m_pSlice->is_NewInSlice))
                                {
                                    //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                                    SetTaskAsReady_AQ(qID, i);
                                }
                                else
                                {
                                    //(m_pAdditionalQueue[qID])[i]->m_bIsReady = false;
                                    SetTaskAsNotReady_AQ(qID, i);
                                }
                            }
                            else
                            {
                                //(m_pAdditionalQueue[qID])[i]->m_bIsReady = true;
                                SetTaskAsReady_AQ(qID, i);
                            }
                        }
                        break;
                    case VC1Complete:
                        if (!((m_pCommonQueue[qID])[0]->IsDeblocking((m_pCommonQueue[qID])[0]->m_pSlice->slice_settings)))
                        {
                                    if ((i+1) < m_pTasksInQueue[qID])
                            {
                                /*if (!(m_pAdditionalQueue[qID])[i+1]->m_pSlice->is_NewInSlice)
                                    (m_pAdditionalQueue[qID])[i+1]->m_pSlice->smooth_info= pTask->m_pSlice->smooth_info;*/

                                if (((m_pAdditionalQueue[qID])[i+1]->m_eTasktype == VC1PreparePlane)&&
                                    ((m_pCommonQueue[qID])[i+1]->m_eTasktype == VC1Complete)&&
                                    (!(m_pCommonQueue[qID])[i+1]->m_pSlice->is_NewInSlice))
                                {
                                    //(m_pAdditionalQueue[qID])[i+1]->m_bIsReady = true;
                                    SetTaskAsReady_AQ(qID, i+1);
                                }

                            }
                        }

                        if (pTask->m_pSlice->m_picLayerHeader->FCM == VC1_FrameInterlace) // Bad case of Interlace Frames slices are depends
                        {
                            if ((i+1) < m_pTasksInQueue[qID])
                            {
                                if ((i+2) < m_pTasksInQueue[qID])
                                {
                                    if ((m_pAdditionalQueue[qID])[i+1]->m_pSlice->is_LastInSlice)
                                    {
                                        if (((m_pAdditionalQueue[qID])[i+2]->m_eTasktype == VC1Deblock)&&
                                            ((m_pAdditionalQueue[qID])[i+1]->m_eTasktype == VC1Deblock))
                                        {
                                            //(m_pAdditionalQueue[qID])[i+1]->m_bIsReady = true;
                                            SetTaskAsReady_AQ(qID, i+1);
                                        }
                                    } else if ((m_pAdditionalQueue[qID])[i+1]->m_eTasktype == VC1Deblock)
                                    {
                                        //(m_pAdditionalQueue[qID])[i+1]->m_bIsReady = true;
                                        SetTaskAsReady_AQ(qID, i+1);
                                    }
                                }
                                else if ((m_pAdditionalQueue[qID])[i+1]->m_eTasktype == VC1Deblock)
                                {
                                    //(m_pAdditionalQueue[qID])[i+1]->m_bIsReady = true;
                                    SetTaskAsReady_AQ(qID, i+1);
                                }
                            }

                        }
                        else if ((i+1) < m_pTasksInQueue[qID])
                        {
                            if (((m_pAdditionalQueue[qID])[i+1]->m_eTasktype == VC1Deblock)&&
                                //(!(m_pAdditionalQueue[qID])[i+1]->m_pSlice->is_NewInSlice))
                                ((((m_pAdditionalQueue[qID])[i+1]->m_isFieldReady)) ||
                                VC1_IS_NOT_PRED((m_pAdditionalQueue[qID])[i+1]->m_pSlice->m_picLayerHeader->PTYPE)) )
                            {
                                //(m_pAdditionalQueue[qID])[i+1]->m_bIsReady = true;
                                SetTaskAsReady_AQ(qID, i+1);
                            }
                            //if ((m_pAdditionalQueue[qID])[i+1]->m_isFirstInSecondSlice)
                            if ((m_pDescriptorQueue[qID]->m_pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)&&
                                (m_pDescriptorQueue[qID]->m_iActiveTasksInFirstField == 0))
                            {
                                Ipp32u count;
                                VC1Context* pContext = m_pDescriptorQueue[qID]->m_pContext;
                                VC1PictureLayerHeader * picLayerHeader = GetFirstInSecondField(qID);
                                if (picLayerHeader)
                                {
                                    if ((pContext->m_bIntensityCompensation)&&
                                        (picLayerHeader->PTYPE == VC1_P_FRAME)&&
                                        (m_pDescriptorQueue[qID]->m_bIsReferenceReady))
                                    {
                                        UpdateICTablesForSecondField(m_pDescriptorQueue[qID]->m_pContext);
                                    }
                                    else
                                    {
                                        if ((VC1_IS_REFERENCE(picLayerHeader->PTYPE)&&
                                            (m_pDescriptorQueue[qID]->m_bIsReferenceReady)))
                                            pContext->m_bIntensityCompensation = 0;

                                    }
                                }

                                if (m_pDescriptorQueue[qID]->m_pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                                {
                                    for (count = i+1; count < m_pTasksInQueue[qID];count++)
                                    {
                                        if (((m_pCommonQueue[qID])[count]->m_eTasktype > VC1Decode)&&
                                            m_pDescriptorQueue[qID]->m_bIsReferenceReady)
                                        {
                                            //(m_pAdditionalQueue[qID])[count]->m_bIsReady = true;
                                            SetTaskAsReady_AQ(qID, count);
                                        }
                                        (m_pAdditionalQueue[qID])[count]->m_isFieldReady = true;
                                    }
                                }
                                // B frame. MVcalculate is a sequence task
                                else if ((m_pAdditionalQueue[qID])[i+1]->m_pSlice->m_picLayerHeader->PTYPE == VC1_B_FRAME)
                                {
                                    for (count = i+1; count < m_pTasksInQueue[qID];count++)
                                    {
                                        if ((( (m_pCommonQueue[qID])[count]->m_eTasktype > VC1Decode)&&
                                            ((m_pCommonQueue[qID])[count])->m_pSlice->is_NewInSlice)&&
                                            m_pDescriptorQueue[qID]->m_bIsReferenceReady)
                                        {
                                            //(m_pAdditionalQueue[qID])[count]->m_bIsReady = true;
                                            SetTaskAsReady_AQ(qID, count);
                                        }
                                        (m_pAdditionalQueue[qID])[count]->m_isFieldReady = true;
                                    }
                                }
                            }
                        }
                        --m_pDescriptorQueue[qID]->m_iActiveTasksInFirstField;
                        //(m_pAdditionalQueue[qID])[i]->m_bDone = true;
                        SetTaskAsDone_AQ(qID, i);
                        break;
                    default:
                        VM_ASSERT(0);
                    }
                    //printf("complete %d task \n", i);
STATISTIC_PARAL_ENTRY(pTask,pTask->m_eTasktype,VC1AddPerfomrd);

                    //(m_pAdditionalQueue[qID])[i]->m_bInProgress = false;
                    DisableProcessBit_AQ(qID,i);

STATISTICS_END_TIME(m_timeStatistics->AddPerfomed_StartTime,
                    m_timeStatistics->AddPerfomed_EndTime,
                    m_timeStatistics->AddPerfomed_TotalTime);
                    guard.Unlock();
                    //WakeUP();
                    return true;
        }
    }
#ifndef UMC_RESTRICTED_CODE_VA
    bool VC1TaskStore::CreateDSQueue(VC1Context* pContext,
                                     VideoAccelerator* va)
    {
        Ipp32u i;
        m_pSHeap->s_new(&m_pDescriptorQueue,m_iNumFramesProcessing);
        for (i = 0; i < m_iNumFramesProcessing; i++)
        {
            if (va)
            {
                if ((va->m_Profile & VA_SW) == 0)
                {
#ifdef UMC_VA_DXVA

                    Ipp8u* pBuf;
                    if (va->IsIntelCustomGUID())
                    {
                        if(va->GetProtectedVA())
                        {
                            pBuf = m_pSHeap->s_alloc<VC1FrameDescriptorVA_Protected<VC1PackerDXVA_Protected>>();
                            m_pDescriptorQueue[i] = new(pBuf) VC1FrameDescriptorVA_Protected<VC1PackerDXVA_Protected>(m_pMemoryAllocator,va);
                        }
                        else
                        {
                            pBuf = m_pSHeap->s_alloc<VC1FrameDescriptorVA_EagleLake<VC1PackerDXVA_EagleLake>>();
                            m_pDescriptorQueue[i] = new(pBuf) VC1FrameDescriptorVA_EagleLake<VC1PackerDXVA_EagleLake>(m_pMemoryAllocator,va);
                        }
                    }
                    else
                    {
                        Ipp8u* pBuf;
                        pBuf = m_pSHeap->s_alloc<VC1FrameDescriptorVA<VC1PackerDXVA>>();
                        m_pDescriptorQueue[i] = new(pBuf) VC1FrameDescriptorVA<VC1PackerDXVA>(m_pMemoryAllocator,va);
                    }
#endif

#ifdef UMC_VA_LINUX
                    Ipp8u* pBuf;
                    pBuf = m_pSHeap->s_alloc<VC1FrameDescriptorVA_Linux<VC1PackerLVA> >();
                    m_pDescriptorQueue[i] = new(pBuf) VC1FrameDescriptorVA_Linux<VC1PackerLVA>(m_pMemoryAllocator,va);
#endif
                }
                else
                    m_pDescriptorQueue[i] = new VC1FrameDescriptor(m_pMemoryAllocator);

            }
            else
                m_pDescriptorQueue[i] = new VC1FrameDescriptor(m_pMemoryAllocator);

            if (!m_pDescriptorQueue[i])
                return false;

            m_pDescriptorQueue[i]->Init(i,
                                        pContext,
                                        this,
                                        true,
                                        0);

        }
        m_pPrefDS =  m_pDescriptorQueue[0];
        return true;
    }
#endif
    bool VC1TaskStore::CreateDSQueue(VC1Context* pContext,
                                     bool IsReorder,
                                     Ipp16s* pResidBuf)
    {
        Ipp32u i;
        Ipp8u* pBuf;
        m_pSHeap->s_new(&m_pDescriptorQueue,m_iNumFramesProcessing);
        for (i = 0; i < m_iNumFramesProcessing; i++)
        {
            pBuf = m_pSHeap->s_alloc<VC1FrameDescriptor>();
            m_pDescriptorQueue[i] = new(pBuf) VC1FrameDescriptor(m_pMemoryAllocator);
            m_pDescriptorQueue[i]->Init(i,pContext,this,IsReorder,pResidBuf);
        }
        m_pPrefDS =  m_pDescriptorQueue[0];
        return true;
    }

    bool VC1TaskStore::SetNewSHParams(VC1Context* pContext)
    {
         Ipp32u i = 0;
        for (i = 0; i < m_iNumFramesProcessing; i++)
        {
            m_pDescriptorQueue[i]->SetNewSHParams(pContext);
        }

        return true;
    }

    void VC1TaskStore::ResetDSQueue()
    {
        Ipp32u i;
        for (i = 0; i < m_iNumFramesProcessing; i++)
            m_pDescriptorQueue[i]->Reset();
        m_lNextFrameCounter = 1;
        m_lNextRefFrameCounter = 1;

    }
    inline bool VC1TaskStore::IsPerfomedDS()
    {
        Ipp32u i;
        AutomaticMutex guard(m_mDSGuard);
        for (i = 0; i < m_iNumFramesProcessing; i++)
        {
            if ((m_pDescriptorQueue[i]->m_bIsReadyToDisplay)&&
                (m_lNextFrameCounter == m_pDescriptorQueue[i]->m_iFrameCounter) &&
                !m_pDescriptorQueue[i]->m_bIsBusy)
            {
                m_pDescriptorQueue[i]->m_bIsBusy = true;
                return true;
            }
        }
        return false;
    }
    void VC1TaskStore::FreeBusyDescriptor()
    {
        AutomaticMutex guard(m_mDSGuard);
        Ipp32u i;
        for (i = 0; i < m_iNumFramesProcessing; i++)
        {
            if (m_pDescriptorQueue[i]->m_bIsBusy)
            {
                m_pDescriptorQueue[i]->m_bIsBusy = false;
            }
        }
    }
    bool VC1TaskStore::IsProcessingDS()
    {
        AutomaticMutex guard(m_mDSGuard);
        Ipp32u i;
        for (i = 0; i < m_iNumFramesProcessing; i++)
        {
            if (!m_pDescriptorQueue[i]->m_bIsReadyToLoad)
                return true;
        }
        return false;
    }
    bool VC1TaskStore::HaveTasksToBeDone()
    {
        AutomaticMutex guard(m_mDSGuard);
        if (m_bIsLastFramesMode)
        {
            bool sts = IsProcessingDS();
            if (false == sts)
            {
                m_bIsLastFramesMode = false;
                return false;
            }
            return true;
        }
        else
            return true;

    }
    VC1FrameDescriptor* VC1TaskStore::GetStartDS()
    {
        //AutomaticMutex guard(m_mDSGuard);
        Ipp32u i;
        VC1FrameDescriptor* pCurrDescriptor = m_pDescriptorQueue[0];
        for (i = 0; i < m_iNumFramesProcessing-1; i++)
        {
            if (pCurrDescriptor->m_iFrameCounter > m_pDescriptorQueue[i+1]->m_iFrameCounter)
                pCurrDescriptor = m_pDescriptorQueue[i+1];

        }
        return pCurrDescriptor;
    }

    VC1FrameDescriptor* VC1TaskStore::GetLastDS()
    {
        //AutomaticMutex guard(m_mDSGuard);
        Ipp32u i;
        VC1FrameDescriptor* pCurrDescriptor = m_pDescriptorQueue[0];
        for (i = 0; i < m_iNumFramesProcessing-1; i++)
        {
            if (pCurrDescriptor->m_iFrameCounter < m_pDescriptorQueue[i+1]->m_iFrameCounter)
                pCurrDescriptor = m_pDescriptorQueue[i+1];

        }
        return pCurrDescriptor;
    }
    VC1FrameDescriptor* VC1TaskStore::GetFirstDS()
    {
        Ipp32u i;
        for (i = 0; i < m_iNumFramesProcessing; i++)
        {
            if (m_pDescriptorQueue[i]->m_iFrameCounter == m_lNextFrameCounter)
                return m_pDescriptorQueue[i];

        }
        return NULL;
    }

    void VC1TaskStore::CreateOutBuffersQueue()
    {
        Ipp32u i;
        Ipp8u* pBuf;
        m_pSHeap->s_new(&m_pOutFrameQueue,m_iNumFramesProcessing+2);
        for(i = 0; i < m_iNumFramesProcessing+2; i++)
        {
            pBuf = m_pSHeap->s_alloc<VC1FrameIndexProc>();
            m_pOutFrameQueue[i] = new(pBuf) VC1FrameIndexProc;
        }
    }
    void VC1TaskStore::OpenNextFrames(VC1FrameDescriptor* pDS,VC1FrameDescriptor** pPrevDS,Ipp32s* CurrRefDst, Ipp32s* CurBDst)
    {
        AutomaticMutex guard(m_mDSGuard);
        Ipp32u i = 0;
        bool isReadyReference = ((pDS->m_pContext->m_picLayerHeader->PTYPE != VC1_B_FRAME)&&
                                 (pDS->m_pContext->m_picLayerHeader->PTYPE != VC1_BI_FRAME))?(true):(false);

        if (isReadyReference)
            --(*CurrRefDst);
        else
            --(*CurBDst);

        *pPrevDS = NULL;

        // Swap frames to decode according to priorities
        ippsCopy_32s(m_pDSIndicate,m_pDSIndicateSwap,m_iNumFramesProcessing);
        ippsCopy_32s(m_pDSIndicateSwap+1,m_pDSIndicate,m_iNumFramesProcessing-1);
        m_pDSIndicate[m_iNumFramesProcessing-1] = m_pDSIndicateSwap[0];

        m_pPrefDS = GetPreferedDS();
        //if  (!VC1_IS_REFERENCE(m_pPrefDS->m_pContext->m_picLayerHeader->PTYPE))
        //    GetNextRefDS(&m_pPrefDS);



        for (i = 0; i < m_iNumFramesProcessing; i++)
        {
            if (m_pDescriptorQueue[i]->m_bIsReadyToProcess)
            {
                if (isReadyReference)
                {
                    --(m_pDescriptorQueue[i]->m_iRefFramesDst);
                    if ((0 == m_pDescriptorQueue[i]->m_iRefFramesDst)&&
                        ((!m_pDescriptorQueue[i]->m_pContext->m_bIntensityCompensation&&
                          !pDS->m_pContext->m_seqLayerHeader.RANGERED)||
                         (0 == m_pDescriptorQueue[i]->m_iBFramesDst)))
                    {
                        WakeTasksInAlienQueue(m_pDescriptorQueue[i],pPrevDS);
                    }
                }
                else // only for BAD case of intensity compensation
                {
                    --(m_pDescriptorQueue[i]->m_iBFramesDst);
                    if ((0 == m_pDescriptorQueue[i]->m_iBFramesDst)&&
                        (0 == m_pDescriptorQueue[i]->m_iRefFramesDst)&&
                        ((m_pDescriptorQueue[i]->m_pContext->m_bIntensityCompensation)||
                         m_pDescriptorQueue[i]->m_pContext->m_seqLayerHeader.RANGERED))
                    {
                        WakeTasksInAlienQueue(m_pDescriptorQueue[i],pPrevDS);
                    }
                }
            }
            else if ((m_pDescriptorQueue[i]->m_bIsSkippedFrame)&&
                       (pDS->m_iSelfID != i))
            {
                if (isReadyReference)
                {
                    --(m_pDescriptorQueue[i]->m_iRefFramesDst);
                    if (0 == m_pDescriptorQueue[i]->m_iRefFramesDst)
                        m_pDescriptorQueue[i]->m_bIsReferenceReady = true;

                }
                if (m_pDescriptorQueue[i]->m_bIsReferenceReady)
                    //(m_pDescriptorQueue[i]->m_bIsBReady))
                {
                    m_pDescriptorQueue[i]->m_bIsReadyToDisplay = true;
                }
            }
        }
    }

    void VC1TaskStore::SetDstForFrameAdv(VC1FrameDescriptor* pDS, Ipp32s* CurrRefDst, Ipp32s* CurBDst)
    {
        AutomaticMutex guard(m_mDSGuard);
        bool isFrameReference = ((pDS->m_pContext->m_picLayerHeader->PTYPE != VC1_B_FRAME)&&
                                 (pDS->m_pContext->m_picLayerHeader->PTYPE != VC1_BI_FRAME)&&
                                 (!pDS->isSpecialBSkipFrame()))?(true):(false);

        pDS->m_iRefFramesDst = *CurrRefDst;
        pDS->m_iBFramesDst = *CurBDst;

        if ((0 == pDS->m_iRefFramesDst)&&
            ((!pDS->m_pContext->m_bIntensityCompensation)||
             (0 == pDS->m_iBFramesDst)))
        {
            pDS->m_bIsReferenceReady = true;
        }
        else
            pDS->m_bIsReferenceReady = false;

        if (((pDS->m_pContext->m_InitPicLayer->PTYPE == VC1_I_FRAME)&&
            (pDS->m_pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME))||
            ((pDS->m_pContext->m_InitPicLayer->PTYPE == VC1_BI_FRAME)&&
            (pDS->m_pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)))
            pDS->m_bIsReferenceReady = true;

        if ((pDS->m_bIsSkippedFrame)&&(pDS->m_bIsReferenceReady))
        {
            pDS->m_bIsReadyToDisplay = true;
        }

        //if (pDS->m_pContext->m_bNeedToUseCompBuffer)
        //    m_bNeedToCompField = true;
        //else if (VC1_IS_REFERENCE(pDS->m_pContext->m_picLayerHeader->PTYPE))
        //    m_bNeedToCompField = false;

        if (isFrameReference)
            ++(*CurrRefDst);
        else
            ++(*CurBDst);

        if (pDS->m_pContext->m_picLayerHeader->PTYPE != VC1_SKIPPED_FRAME)
            pDS->m_bIsReadyToProcess = true;
        else
            pDS->m_bIsReadyToProcess = false;

    }
    void VC1TaskStore::SetDstForFrame(VC1FrameDescriptor* pDS, Ipp32s* CurrRefDst, Ipp32s* CurBDst)
    {
        AutomaticMutex guard(m_mDSGuard);

        bool isFrameReference = ((pDS->m_pContext->m_picLayerHeader->PTYPE != VC1_B_FRAME)&&
                                 (pDS->m_pContext->m_picLayerHeader->PTYPE != VC1_BI_FRAME)&&
                                 (!pDS->isSpecialBSkipFrame()))?(true):(false);

        pDS->m_iRefFramesDst = *CurrRefDst;
        pDS->m_iBFramesDst = *CurBDst;

        if ((0 == pDS->m_iRefFramesDst)&&
            ((!pDS->m_pContext->m_bIntensityCompensation
               &&!pDS->m_pContext->m_seqLayerHeader.RANGERED)||
             (0 == pDS->m_iBFramesDst)))
        {
            pDS->m_bIsReferenceReady = true;

            if ((pDS->m_pContext->m_seqLayerHeader.RANGERED)&&(VC1_IS_PRED(pDS->m_pContext->m_picLayerHeader->PTYPE)))
                RangeRefFrame(pDS->m_pContext);

        }
        else
            pDS->m_bIsReferenceReady = false;

        if ((pDS->m_pContext->m_InitPicLayer->PTYPE == VC1_I_FRAME)||
            (pDS->m_pContext->m_InitPicLayer->PTYPE == VC1_BI_FRAME))
            pDS->m_bIsReferenceReady = true;

        if ((pDS->m_bIsSkippedFrame)&&(pDS->m_bIsReferenceReady))
        {
            pDS->m_bIsReadyToDisplay = true;
        }

        if (isFrameReference)
            ++(*CurrRefDst);
        else
            ++(*CurBDst);

        if (pDS->m_pContext->m_picLayerHeader->PTYPE != VC1_SKIPPED_FRAME)
            pDS->m_bIsReadyToProcess = true;
        else
            pDS->m_bIsReadyToProcess = false;

    }

    void VC1TaskStore::WakeTasksInAlienQueue(VC1FrameDescriptor* pDS, VC1FrameDescriptor** pPrevDS)
    {
        AutomaticMutex guard(*m_pGuardGet[pDS->m_iSelfID]);
        pDS->m_bIsReferenceReady = true;
        if (pDS->m_pContext->m_bIntensityCompensation)
        {
            VC1Context* pContext = pDS->m_pContext;

            if ((pContext->m_picLayerHeader->PTypeField1 == VC1_P_FRAME)||
                (pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)||
                (pContext->m_picLayerHeader->PTYPE == VC1_P_FRAME))
            {
                *pPrevDS = pDS;
                pDS->m_bIsReferenceReady = false;
            }
        }
        if ((pDS->m_pContext->m_seqLayerHeader.RANGERED)&&
            (VC1_IS_PRED(pDS->m_pContext->m_picLayerHeader->PTYPE)))
        {
            *pPrevDS = pDS;
            pDS->m_bIsReferenceReady = false;
        }
        if (pDS->m_bIsReferenceReady)
        {
            for(Ipp32u i =0; i < m_pTasksInQueue[pDS->m_iSelfID]; i++)
            {
                if ( ((m_pCommonQueue[pDS->m_iSelfID])[i]->m_eTasktype > VC1Decode )&&
                    ((m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_isFieldReady))
                {
                    if (((m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_eTasktype == VC1MVCalculate)&&
                        ( (m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_pSlice->is_NewInSlice))
                    {
                        //(m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_bIsReady = true;
                        SetTaskAsReady_AQ(pDS->m_iSelfID, i);
                    }

                    else if ((m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_eTasktype == VC1MC )
                    {
                        //(m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_bIsReady = true;
                        SetTaskAsReady_AQ(pDS->m_iSelfID, i);
                    }
                }
            }
        //}
    }

    }
    void VC1TaskStore::WakeTasksInAlienQueue(VC1FrameDescriptor* pDS)
    {
        AutomaticMutex guard(*m_pGuardGet[pDS->m_iSelfID]);
        // Reference
        pDS->m_bIsReferenceReady = true;

        for(Ipp32u i =0; i < m_pTasksInQueue[pDS->m_iSelfID]; i++)
        {
            if ( ((m_pCommonQueue[pDS->m_iSelfID])[i]->m_eTasktype > VC1Decode )&&
                ((m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_isFieldReady))
            {
                if (((m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_eTasktype == VC1MVCalculate)&&
                    ( (m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_pSlice->is_NewInSlice))
                {
                    //(m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_bIsReady = true;
                    SetTaskAsReady_AQ(pDS->m_iSelfID, i);
                }

                else if ((m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_eTasktype == VC1MC )
                {
                    //(m_pAdditionalQueue[pDS->m_iSelfID])[i]->m_bIsReady = true;
                    SetTaskAsReady_AQ(pDS->m_iSelfID, i);
                }
            }
        }
    }

    void VC1TaskStore::CompensateDSInQueue(VC1FrameDescriptor* pDS)
    {
        AutomaticMutex guard(*m_pGuardGet[pDS->m_iSelfID]);
        if (pDS->m_pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
        {
            if (pDS->m_iActiveTasksInFirstField == -1)
            {
                //VC1CompensateFrame(pDS->m_pContext,GetFirstInSecondField(pDS->m_iSelfID));
                 UpdateICTablesForSecondField(pDS->m_pContext);

            }
        }
    }
    Ipp32s VC1TaskStore::GetInternalIndex() // for B/BI frames
    {
        Ipp32u i;
        for (i = 0; i < m_iNumFramesProcessing+2; i++)
        {
            if (!m_pOutFrameQueue[i]->m_bIsBusy)
            {
                m_pOutFrameQueue[i]->m_bIsBusy = true;
                return i;
            }
        }
        return -1;
    }
    Ipp32s VC1TaskStore::GetRefInternalIndex() // for I/P/Skipped frames
    {
        Ipp32u i;
        for (i = 0; i < m_iNumFramesProcessing+2; i++)
        {
            if (!m_pOutFrameQueue[i]->m_bIsBusy)
            {
                m_pOutFrameQueue[i]->m_bIsBusy = true;
                m_pOutFrameQueue[i]->m_lRefCounter = m_lNextRefFrameCounter;
                ++m_lNextRefFrameCounter;
                return i;
            }
        }
        return -1;
    }
    Ipp32s VC1TaskStore::GetSPrevIndex()
    {
        Ipp32u i;
        for (i = 0; i < m_iNumFramesProcessing+2; i++)
        {
            if (m_pOutFrameQueue[i]->m_lRefCounter == (m_lNextRefFrameCounter-1))
               return i;
        }
        return -1;

    }
    Ipp32s VC1TaskStore::GetSNextIndex()
    {
        Ipp32u i;
        for (i = 0; i < m_iNumFramesProcessing+2; i++)
        {
            if (m_pOutFrameQueue[i]->m_lRefCounter == (m_lNextRefFrameCounter-2))
                return i;

        }
        return -1;
    }
    Ipp32s VC1TaskStore::GetDisplayIndex()
    {
        Ipp32u i;
        for (i = 0; i < m_iNumFramesProcessing+2; i++)
        {
            if (m_pOutFrameQueue[i]->m_lRefCounter == (m_lNextRefFrameCounter-1))
            {
                return i;
            }
        }
        return -1;
    }
    void   VC1TaskStore::FreeIndexQueue()
    {
        if (!pMainVC1Decoder->m_pExtFrameAllocator)
        {
            Ipp32u i;
            for (i = 0; i < m_iNumFramesProcessing+2; i++)
            {
                m_pOutFrameQueue[i]->m_lRefCounter = 1;
                m_pOutFrameQueue[i]->m_bIsBusy = false;
            }
        }
    }
    void VC1TaskStore::FreeFirstReference()
    {
        if (!pMainVC1Decoder->m_pExtFrameAllocator)
        {
            Ipp32u i;
            ++m_iNumDecodeRefFrames;
            for (i = 0; i < m_iNumFramesProcessing+2; i++)
            {
                if ((m_pOutFrameQueue[i]->m_lRefCounter == m_lFirstRefFrameCounter)&&
                    (m_iNumDecodeRefFrames > 2))
                {
                    ++m_lFirstRefFrameCounter;
                    m_pOutFrameQueue[i]->m_bIsBusy = false;
                    return;
                }
            }
        }
    }
    void VC1TaskStore::FreeBFrame(Ipp32u Index)
    {
        if (!pMainVC1Decoder->m_pExtFrameAllocator)
            m_pOutFrameQueue[Index]->m_bIsBusy = false;
    }

    VC1TaskTypes VC1Task::switch_task()
    {
        switch (m_eTasktype)
        {
// type of tasks in main queue
        case VC1Decode:
            //this->m_pSlice->is_NewInSlice = false;
            m_eTasktype = VC1Reconstruct;
            pMulti = &VC1TaskProcessorUMC::VC1ProcessDiff;
            return m_eTasktype;
        //case VC1Dequant:
        //    m_eTasktype = VC1Reconstruct;
        //    pMulti = &VC1TaskProcessor::VC1ProcessDiff;
        //    return m_eTasktype;
        case VC1Reconstruct:
            m_eTasktype = VC1Complete;
            return VC1Complete;

// type of tasks in additional
        case VC1MVCalculate:
            m_eTasktype = VC1MC;
            pMulti = &VC1TaskProcessorUMC::VC1MotionCompensation;
            return m_eTasktype;
        case VC1MC:
            m_eTasktype = VC1PreparePlane;
            pMulti = &VC1TaskProcessorUMC::VC1PrepPlane;
            return m_eTasktype;
        case VC1PreparePlane:
            //this->m_pSlice->is_NewInSlice = false;
            if (IsDeblocking(m_pSlice->slice_settings))
            {
                m_eTasktype = VC1Deblock;
                pMulti = &VC1TaskProcessorUMC::VC1Deblocking;
            }
            else
                m_eTasktype = VC1Complete;
            return m_eTasktype;
        case VC1Deblock:
            m_eTasktype = VC1Complete;
            return m_eTasktype;
        default:
            VM_ASSERT(0);
        }
        return m_eTasktype;
     }

    void VC1Task::setSliceParams(VC1Context* pContext)
    {
        this->m_pSlice->slice_settings = VC1Decode ;

        if ( (pContext->m_picLayerHeader->PTYPE == VC1_P_FRAME) ||
            (pContext->m_picLayerHeader->PTYPE == VC1_B_FRAME) )
            this->m_pSlice->slice_settings |= VC1Reconstruct;

        if (pContext->m_seqLayerHeader.LOOPFILTER)
            this->m_pSlice->slice_settings |= VC1Deblock;
    }
    void VC1TaskStore::SetExternalSurface(VC1Context* pContext, const FrameData* pFrameData, FrameMemID index)
    {
        pContext->m_frmBuff.m_pFrames[index].m_pY = pFrameData->GetPlaneMemoryInfo(0)->m_planePtr;
        pContext->m_frmBuff.m_pFrames[index].m_pU = pFrameData->GetPlaneMemoryInfo(1)->m_planePtr;
        pContext->m_frmBuff.m_pFrames[index].m_pV = pFrameData->GetPlaneMemoryInfo(2)->m_planePtr;

        pContext->m_frmBuff.m_pFrames[index].m_iYPitch = (Ipp32u)pFrameData->GetPlaneMemoryInfo(0)->m_pitch;
        pContext->m_frmBuff.m_pFrames[index].m_iUPitch = (Ipp32u)pFrameData->GetPlaneMemoryInfo(1)->m_pitch;
        pContext->m_frmBuff.m_pFrames[index].m_iVPitch = (Ipp32u)pFrameData->GetPlaneMemoryInfo(2)->m_pitch;

        pContext->m_frmBuff.m_pFrames[index].pRANGE_MAPY = &pContext->m_frmBuff.m_pFrames[index].RANGE_MAPY;
    }
    FrameMemID VC1TaskStore::LockSurface(FrameMemID* mid, bool isSkip)
    {
        // B frames TBD
        if (pMainVC1Decoder->m_pExtFrameAllocator)
        {
            Status sts;
            Ipp32s Idx;
            const FrameData* pFrameData;
            VideoDataInfo Info;
            Ipp32u h = pMainVC1Decoder->m_pCurrentOut->GetHeight();
            Ipp32u w = pMainVC1Decoder->m_pCurrentOut->GetWidth();
            Info.Init(w, h, YUV420);
            sts = pMainVC1Decoder->m_pExtFrameAllocator->Alloc(mid, &Info, 0);
            if (UMC_OK != sts)
                throw  VC1Exceptions::vc1_exception( VC1Exceptions::mem_allocation_er);
            sts = pMainVC1Decoder->m_pExtFrameAllocator->IncreaseReference(*mid);
            if (UMC_OK != sts)
                throw  VC1Exceptions::vc1_exception( VC1Exceptions::mem_allocation_er);
            Idx = LockAndAssocFreeIdx(*mid);
            if (Idx < 0)
                return Idx;
            if (pMainVC1Decoder->m_va)
            {
                if (!isSkip)
                {
                    if (pMainVC1Decoder->m_va->m_Platform != VA_LINUX) // on Linux we call BeginFrame() inside VC1PackPicParams()
                        sts = pMainVC1Decoder->m_va->BeginFrame(*mid);

                    if (UMC_OK == sts)
                        pMainVC1Decoder->m_pContext->m_frmBuff.m_pFrames[Idx].pRANGE_MAPY = &pMainVC1Decoder->m_pContext->m_frmBuff.m_pFrames[Idx].RANGE_MAPY;
                    else
                        return -1;
                }
            }
            else
            {
                pFrameData = pMainVC1Decoder->m_pExtFrameAllocator->Lock(*mid);
                if (!pFrameData)
                {
                    throw  VC1Exceptions::vc1_exception( VC1Exceptions::mem_allocation_er);
                }
                // if VideoAccelaration it is reason to call BeginFrame
                SetExternalSurface(pMainVC1Decoder->m_pContext, pFrameData, Idx);
            }
            *mid = Idx;
            return 0;
        }
        else
            return GetRefInternalIndex();
    }
    void VC1TaskStore::UnLockSurface(Ipp32s memID)
    {
        // B frames TBD
        if (pMainVC1Decoder->m_pExtFrameAllocator)
        {
            if (pMainVC1Decoder->m_va &&  memID > -1)
                pMainVC1Decoder->m_pExtFrameAllocator->DecreaseReference(memID);
            else 
            {
                FrameMemID mid = UnLockIdx(memID);
                if (mid > -1)
                {
                    // Unlock for SW only
                    pMainVC1Decoder->m_pExtFrameAllocator->Unlock(mid);
                    pMainVC1Decoder->m_pExtFrameAllocator->DecreaseReference(mid);// free ?
                }
            }
        }
        else
            FreeFirstReference();
    }
    void VC1TaskStore::FillIdxVector(Ipp32u size)
    {
        for (Ipp32u i = 0; i < size; i++)
            IntIndexes.push_back(true);
        
    }
    Ipp32s VC1TaskStore::LockAndAssocFreeIdx(FrameMemID mid)
    {
        if (pMainVC1Decoder->m_va)
            return mid;
        for (Ipp32u i = 0; i < IntIndexes.size(); i++)
        {
            if (IntIndexes[i])
            {
                IntIndexes[i] = false;
                AssocIdx.insert(std::pair<Ipp32u, FrameMemID>(i, mid));
                return i;
            }
        }
        return -1;
    }
    void VC1TaskStore::UnLockAllAssocIdx()
    {
        for (Ipp32u i = 0; i < IntIndexes.size(); i++)
            UnLockSurface(i);
    }
    FrameMemID   VC1TaskStore::UnLockIdx(Ipp32u Idx)
    {
        if (pMainVC1Decoder->m_va)
            return Idx;

        if (Idx >= IntIndexes.size())
            return -1;
        if (IntIndexes[Idx]) // already free
            return -1;
        else
        {
            std::map<Ipp32u, FrameMemID>::iterator it;
            IntIndexes[Idx] = true;
            it = AssocIdx.find(Idx);
            FrameMemID memID = it->second;
            AssocIdx.erase(it);
            return memID;
        }

    }
    FrameMemID  VC1TaskStore::GetIdx(Ipp32u Idx)
    {
        if (pMainVC1Decoder->m_va)
            return Idx;
        if (Idx >= IntIndexes.size())
            return -1;
        if (IntIndexes[Idx]) // already free
            return -1;
        return AssocIdx.find(Idx)->second;
    }
    FrameMemID VC1TaskStore::GetCurrIndex(void)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return GetCurrIndex_VA();
        else
            return -1;
    }
    FrameMemID VC1TaskStore::GetPrevIndex(void)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return GetPrevIndex_VA();
        else
            return GetSPrevIndex();
    }
    FrameMemID VC1TaskStore::GetNextIndex(void)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return GetNextIndex_VA();
        else
            return GetSNextIndex();
    }
    FrameMemID VC1TaskStore::GetBFrameIndex(void) 
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return GetBFrameIndex_VA();
        else
            return -1;
    }
    FrameMemID VC1TaskStore::GetRangeMapIndex(void)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return GetICIndex_VA();
        else
            return -1;
    }
    FrameMemID VC1TaskStore::GetRangeMapIndexB(void)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return GetICIndex_VA();
        else
            return -1;
    }
    void VC1TaskStore::SetCurrIndex(FrameMemID Index)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return SetCurrIndex_VA(Index);
    }
    void VC1TaskStore::SetPrevIndex(FrameMemID Index)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return SetPrevIndex_VA(Index);
    }
    void VC1TaskStore::SetNextIndex(FrameMemID Index)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return SetNextIndex_VA(Index);
    }
    void VC1TaskStore::SetBFrameIndex(FrameMemID Index)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return SetBFrameIndex_VA(Index);
    }
    void VC1TaskStore::SetRangeMapIndex(FrameMemID Index)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return SetICIndex_VA(Index);
    }
    void VC1TaskStore::SetRangeMapIndexB(FrameMemID Index)
    {
        if (pMainVC1Decoder->m_pExtFrameAllocator)
            return SetICIndexB_VA(Index);
    }

}// namespace UMC
#endif 

