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

#ifndef __UMC_VC1_DEC_TASK_H_
#define __UMC_VC1_DEC_TASK_H_

#include "vm_types.h"
#include "umc_structures.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_seq.h"


namespace UMC
{
    enum SelfUMCStatus
    {
        UMC_LAST_FRAME = 10
    };

    class VC1TaskProcessorUMC;

    typedef enum
    {
        VC1Decode        = 1,
        //VC1Dequant       = 2,
        VC1Reconstruct   = 2,
        VC1MVCalculate   = 4,
        VC1MC            = 8,
        VC1PreparePlane  = 16,
        VC1Deblock       = 32,
        VC1Complete      = 64
    } VC1TaskTypes;

#pragma pack(16)

    class VC1Task
    {
    public:
        // Default constructor
        VC1Task(Ipp32s iThreadNumber,Ipp32s TaskID):m_pSlice(NULL),
                                                    m_iThreadNumber(iThreadNumber),
                                                    m_iTaskID(TaskID),
                                                    m_eTasktype(VC1Decode),
                                                    m_pBlock(NULL),
                                                    m_pPredBlock(NULL),
                                                    m_pSrcToSwap(NULL),
                                                    m_uDataSizeToSwap(0),
                                                    m_isFirstInSecondSlice(false),
                                                    m_isFieldReady(false),
                                                    pMulti(NULL)
          {
          };
          VC1Task(Ipp32s iThreadNumber):m_pSlice(NULL),
                                        m_iThreadNumber(iThreadNumber),
                                        m_iTaskID(0),
                                        m_eTasktype(VC1Decode),
                                        m_pBlock(NULL),
                                        m_pPredBlock(NULL),
                                        m_pSrcToSwap(NULL),
                                        m_uDataSizeToSwap(0),
                                        m_isFirstInSecondSlice(false),
                                        m_isFieldReady(false),
                                        pMulti(NULL)

          {
          };

          Ipp32u IsDecoding (Ipp32u _task_settings) {return _task_settings&VC1Decode;}
          Ipp32u IsDeblocking(Ipp32u _task_settings) {return _task_settings&VC1Deblock;}
          void setSliceParams(VC1Context* pContext);

#ifdef ALLOW_SW_VC1_FALLBACK
          VC1TaskTypes switch_task();
#endif // #ifdef ALLOW_SW_VC1_FALLBACK

          SliceParams* m_pSlice;                                        //

          Ipp32s m_iThreadNumber;                                     // (Ipp32s) owning thread number
          Ipp32s m_iTaskID;                                           // (Ipp32s) task identificator
          VC1TaskTypes m_eTasktype;
          Ipp16s*      m_pBlock;
          Ipp8u*       m_pPredBlock;
          Ipp8u*       m_pSrcToSwap;
          Ipp32u       m_uDataSizeToSwap;
          bool         m_isFirstInSecondSlice;
          bool         m_isFieldReady;
          VC1Status (VC1TaskProcessorUMC::*pMulti)(VC1Context* pContext, VC1Task* pTask);
    };

#pragma pack()

}
#endif //__umc_vc1_dec_task_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
