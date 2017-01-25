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

#ifndef __UMC_VC1_DEC_JOB_H__
#define __UMC_VC1_DEC_JOB_H__

#include "umc_vc1_common_defs.h"
#include "umc_structures.h"
#include "umc_vc1_dec_task.h"
#include "umc_vc1_dec_exception.h"
#include "umc_memory_allocator.h"
#include "umc_vc1_dec_seq.h"
#include "umc_va_base.h"

typedef VC1Status (*MBLayerDecode)(VC1Context* pContext);
extern MBLayerDecode MBLayerDecode_tbl_Adv[];
extern MBLayerDecode MBLayerDecode_tbl[];


typedef void (*Deblock)(VC1Context* pContext);
extern Deblock Deblock_tbl_Adv[];
extern Deblock Deblock_tbl[];

typedef VC1Status (*ProcessDiff)(VC1Context* pContext, Ipp32s blk_num);
extern ProcessDiff ProcessDiff_Adv[];

typedef VC1Status (*MotionComp)(VC1Context* pContex);
extern MotionComp MotionComp_Adv[];

typedef void (*MBSmooth)(VC1Context* pContext, Ipp32s Height);
extern MBSmooth MBSmooth_tbl[];

namespace UMC
{
    class VC1Task;
    class VC1TaskStoreSW;

    class VC1TaskProcessor
    {
    public:
        virtual Status process() = 0;
        VC1TaskProcessor(){};
        virtual ~VC1TaskProcessor() {};
    };

    class VC1TaskProcessorUMC : public VC1TaskProcessor
    {
    public:
        VC1TaskProcessorUMC(): m_iNumber(0),
                            m_iMemContextID((MemID)-1),
                            m_pContext(NULL),
                            m_pSingleMB(NULL),
                            m_pStore(NULL),
                            m_pMemoryAllocator(NULL)
        {

            pReconstructTbl[0] = &VC1ProcessDiffInter;
            pReconstructTbl[1] = &VC1ProcessDiffIntra;
        };
        virtual ~VC1TaskProcessorUMC() { Release ();}
        virtual Status Init(VC1Context* pContext,
                            Ipp32s iNumber,
                            VC1TaskStoreSW* pStore,
                            MemoryAllocator* pMemoryAllocator);
        virtual Status process();

        inline virtual void AccelerReconstruct()
        {
            pReconstructTbl[0] = &VC1ProcessDiffSpeedUpInter;
            pReconstructTbl[1] = &VC1ProcessDiffSpeedUpIntra;
        };

        virtual VC1Status VC1Decoding                                      (VC1Context* pContext, VC1Task* pTask);
        virtual VC1Status VC1ProcessDiff                                   (VC1Context* pContext, VC1Task* pTask);
        virtual VC1Status VC1Deblocking                                    (VC1Context* pContext, VC1Task* pTask);
        virtual VC1Status VC1MotionCompensation                            (VC1Context* pContext, VC1Task* pTask);
        virtual VC1Status VC1MVCalculation                                 (VC1Context* pContext, VC1Task* pTask);
        virtual VC1Status VC1PrepPlane                                     (VC1Context* pContext, VC1Task* pTask);

   protected:

        void   Release ();
        Status InitPicParamJob();
        void   InitContextForNextTask(VC1Task* pTask);

        void   WriteDiffs(VC1Context* pContext);
        void   ProcessSmartException                                       (VC1Exceptions::SmartLevel exLevel, VC1Context* pContext, VC1Task* pTask, VC1MB* pCurrMB);

        VC1Status (*pReconstructTbl[2])(VC1Context* pContext, Ipp32s blk_num);

        void CompensateInterlacePFrame(VC1Context* pContext, VC1Task *pTask);
        void CompensateInterlaceBFrame(VC1Context* pContext, VC1Task *pTask);

        Ipp32s                  m_iNumber;
        UMC::MemID              m_iMemContextID;
        VC1Context*             m_pContext;
        VC1SingletonMB*         m_pSingleMB;
        VC1TaskStoreSW*          m_pStore;
        MemoryAllocator*        m_pMemoryAllocator; // (MemoryAllocator*) pointer to memory allocator
    };

}

#endif //__umc_vc1_dec_job_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
