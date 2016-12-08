//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_VC1_DEC_FRAME_DESCR_H_
#define __UMC_VC1_DEC_FRAME_DESCR_H_

#include "vm_types.h"
#include "umc_structures.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_seq.h"
#include "umc_media_data_ex.h"

#include "umc_va_base.h"

#include "umc_memory_allocator.h"

namespace UMC
{
    class VC1TaskStore;
    class VC1FrameDescriptor
    {
        friend class VC1TaskStore;
        DYNAMIC_CAST_DECL_BASE(VC1FrameDescriptor);

    public:
        // Default constructor
        VC1FrameDescriptor(MemoryAllocator* pMemoryAllocator):m_pContext(NULL),
                                                              m_ipContextID((MemID)-1),
                                                              m_ipBufferStartID((MemID)-1),
                                                              m_iPicHeaderID((MemID)-1),
                                                              m_iBitplaneID((MemID)-1),
                                                              m_iFrameCounter(0),
                                                              m_bIsWarningStream(false),
                                                              m_bIsFieldAbsent(false),

                                                              m_iRemainActiveTasks(0),
                                                              m_iSelfID(0),
                                                              m_bIsReadyToLoad(true),
                                                              m_iRefFramesDst(0),
                                                              m_iBFramesDst(0),
                                                              m_bIsReferenceReady(false),
                                                              m_bIsBReady(false),
                                                              m_bIsReadyToDisplay(false),
                                                              m_bIsBusy(false),
                                                              m_bIsReadyToProcess(false),
                                                              m_bIsSkippedFrame(false),
                                                              m_pDiffMem(NULL),
                                                              m_iActiveTasksInFirstField(0),
                                                              m_pStore(NULL),
                                                              m_pMemoryAllocator(NULL),
                                                              m_iDiffMemID((MemID)-1),
                                                              m_iMBsMemID((MemID)-1),
                                                              m_iDCACParamsMemID((MemID)-1),
                                                              m_iInernBufferID((MemID)-1),
                                                              m_iMemContextID((MemID)-1),
                                                              m_bIsValidFrame(true),
                                                              m_bBframeDelay(false),
                                                              m_bIsSpecialBSkipFrame(false),
                                                              m_bIsReorder(true)

        {
            m_pMemoryAllocator = pMemoryAllocator;
        }

        virtual bool Init                (Ipp32u         DescriporID,
                                          VC1Context*    pContext,
                                          VC1TaskStore*  pStore,
                                          bool           IsReorder,
                                          Ipp16s*        pResidBuf);
        virtual void Release();
        void         Reset();
        void         processFrame        (Ipp32u*  pOffsets,
                                          Ipp32u*  pValues);

        virtual Status       preProcData         (VC1Context*            pContext,
                                                  Ipp32u                 bufferSize,
                                                  Ipp64u                 frameCount,
                                                  bool                   isWMV, bool& skip);


        bool isDescriptorValid()
        {
            return m_bIsValidFrame;
        }

        bool isEmpty()
        {
            return m_bIsReadyToLoad;
        }

        bool SetNewSHParams(VC1Context* pContext);

        virtual ~VC1FrameDescriptor()
        {
            Release();
        }

        bool IsNeedToDelayBFrame()
        {
            bool pBframeTemp = m_bBframeDelay;
            m_bBframeDelay = false;
            return pBframeTemp;
        }
        bool isSpecialBSkipFrame()
        {
            return m_bIsSpecialBSkipFrame;
        }
        virtual bool isVADescriptor() {return false;}

        Ipp32u GetID() {return m_iSelfID;}

        // Just I/F needs
        virtual void PrepareVLDVABuffers(Ipp32u*  pOffsets,
                                         Ipp32u*  pValues,
                                         Ipp8u*   pOriginalData,
                                         MediaDataEx::_MediaDataEx* pOrigStCodes){};


        VC1Context*                m_pContext;
        UMC::MemID                 m_ipContextID;
        UMC::MemID                 m_ipBufferStartID;
        UMC::MemID                 m_iPicHeaderID;
        UMC::MemID                 m_iBitplaneID;
        Ipp64u                     m_iFrameCounter;
        bool                       m_bIsWarningStream;
        bool                       m_bIsFieldAbsent;

    protected:

        virtual Status SetPictureIndices     (Ipp32u PTYPE, bool& skip);

        Ipp32u                     m_iRemainActiveTasks;
        Ipp32u                     m_iSelfID;
        bool                       m_bIsReadyToLoad;
        Ipp32s                     m_iRefFramesDst;
        Ipp32s                     m_iBFramesDst;
        bool                       m_bIsReferenceReady;
        bool                       m_bIsBReady;
        bool                       m_bIsReadyToDisplay;
        bool                       m_bIsBusy;
        bool                       m_bIsReadyToProcess;
        bool                       m_bIsSkippedFrame;
        Ipp16s*                    m_pDiffMem;
        Ipp32s                     m_iActiveTasksInFirstField;

        VC1TaskStore*              m_pStore;

        MemoryAllocator*           m_pMemoryAllocator; // (MemoryAllocator*) pointer to memory allocator
        UMC::MemID                 m_iDiffMemID;       // diffrences memory
        UMC::MemID                 m_iMBsMemID;
        UMC::MemID                 m_iDCACParamsMemID;
        UMC::MemID                 m_iInernBufferID;   // diffrences memory
        UMC::MemID                 m_iMemContextID;


        bool                       m_bIsValidFrame; // no errors during decoding
        bool                       m_bBframeDelay;
        bool                       m_bIsSpecialBSkipFrame;

        bool                       m_bIsReorder;

    };
}
#endif //__umc_vc1_dec_frame_descr_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
