//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_dec_run_level_tbl.h"
#include "umc_vc1_common_mvdiff_tbl.h"
#include "umc_vc1_common_interlace_mv_tables.h"
#include "umc_vc1_common_defs.h"
#include "ippdefs.h"

#include "umc_vc1_dec_debug.h"

void ChooseDCTable(VC1Context* pContext, Ipp32s transDCtableIndex)
{
    Ipp32s *lumaTable_lut[]  ={pContext->m_vlcTbl->m_pLowMotionLumaDCDiff,
                               pContext->m_vlcTbl->m_pHighMotionLumaDCDiff};
    Ipp32s *chromaTable_lut[]={pContext->m_vlcTbl->m_pLowMotionChromaDCDiff,
                               pContext->m_vlcTbl->m_pHighMotionChromaDCDiff};

    pContext->m_picLayerHeader->m_pCurrLumaDCDiff   = lumaTable_lut[transDCtableIndex];
    pContext->m_picLayerHeader->m_pCurrChromaDCDiff = chromaTable_lut[transDCtableIndex];
}

void ChooseACTable(VC1Context* pContext,
                   Ipp32s transACtableIndex1,
                   Ipp32s transACtableIndex2)
{
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    IppiACDecodeSet_VC1* IntraACDecodeSet_lut[] =
    {
        &pContext->m_vlcTbl->HighRateIntraACDecodeSet, &pContext->m_vlcTbl->HighMotionIntraACDecodeSet,
        &pContext->m_vlcTbl->MidRateIntraACDecodeSet,  &pContext->m_vlcTbl->LowMotionIntraACDecodeSet,
        &pContext->m_vlcTbl->HighMotionIntraACDecodeSet, &pContext->m_vlcTbl->MidRateIntraACDecodeSet
    };
    IppiACDecodeSet_VC1* IntrerACDecodeSet_lut[] =
    {
        &pContext->m_vlcTbl->HighRateInterACDecodeSet,     &pContext->m_vlcTbl->HighMotionInterACDecodeSet,
        &pContext->m_vlcTbl->MidRateInterACDecodeSet,      &pContext->m_vlcTbl->LowMotionInterACDecodeSet,
        &pContext->m_vlcTbl->HighMotionInterACDecodeSet,   &pContext->m_vlcTbl->MidRateInterACDecodeSet
    };

    if(picLayerHeader->PQINDEX <= 8)
    {
        picLayerHeader->m_pCurrIntraACDecSet = IntraACDecodeSet_lut[transACtableIndex2];
        picLayerHeader->m_pCurrInterACDecSet = IntrerACDecodeSet_lut[transACtableIndex1];
#ifdef VC1_DEBUG_ON
        switch(transACtableIndex2)
        {
        case 0:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighRateIntra\n"));
            break;
        case 1:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighMotionIntra\n"));
            break;
        case 2:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("MidRateIntra\n"));
            break;
        case 3:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("LowMotionIntra\n"));
            break;
        case 4:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighMotionIntra\n"));
            break;
        case 5:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("MidRateIntra\n"));
            break;
        };
        switch(transACtableIndex1)
        {
        case 0:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighRateInter\n"));
            break;
        case 1:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighMotionInter\n"));
            break;
        case 2:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("MidRateInter\n"));
            break;
        case 3:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("LowMotionInter\n"));
            break;
        case 4:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighMotionInter\n"));
            break;
        case 5:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("MidRateInter\n"));
            break;
        };
#endif
    }
    else
    {
        picLayerHeader->m_pCurrIntraACDecSet = IntraACDecodeSet_lut[transACtableIndex2+3];
        picLayerHeader->m_pCurrInterACDecSet = IntrerACDecodeSet_lut[transACtableIndex1+3];
#ifdef VC1_DEBUG_ON
        switch(transACtableIndex2)
        {
        case 0:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighRateIntra\n"));
            break;
        case 1:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighMotionIntra\n"));
            break;
        case 2:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("MidRateIntra\n"));
            break;
        case 3:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("LowMotionIntra\n"));
            break;
        case 4:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighMotionIntra\n"));
            break;
        case 5:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("MidRateIntra\n"));
            break;
        };
        switch(transACtableIndex1)
        {
        case 0:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighRateInter\n"));
            break;
        case 1:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighMotionInter\n"));
            break;
        case 2:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("MidRateInter\n"));
            break;
        case 3:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("LowMotionInter\n"));
            break;
        case 4:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("HighMotionInter\n"));
            break;
        case 5:
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_TABLES,
                                                    VM_STRING("MidRateInter\n"));
            break;
        };
#endif
    }
}


void ChooseTTMB_TTBLK_SBP(VC1Context* pContext)
{
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    VC1VLCTables* VLCTables = pContext->m_vlcTbl;

    if( picLayerHeader->PQUANT < 5)
    {
        picLayerHeader->m_pCurrTTMBtbl = VLCTables->TTMB_PB_TABLES[2];
        picLayerHeader->m_pCurrTTBLKtbl = VLCTables->TTBLK_PB_TABLES[2];
        picLayerHeader->m_pCurrSBPtbl = VLCTables->SBP_PB_TABLES[0];
    }
    else if(picLayerHeader->PQUANT < 13)
    {
        picLayerHeader->m_pCurrTTMBtbl = VLCTables->TTMB_PB_TABLES[1];
        picLayerHeader->m_pCurrTTBLKtbl =VLCTables->TTBLK_PB_TABLES[1];
        picLayerHeader->m_pCurrSBPtbl = VLCTables->SBP_PB_TABLES[1];
    }
    else
    {
        picLayerHeader->m_pCurrTTMBtbl = VLCTables->TTMB_PB_TABLES[0];
        picLayerHeader->m_pCurrTTBLKtbl = VLCTables->TTBLK_PB_TABLES[0];
        picLayerHeader->m_pCurrSBPtbl = VLCTables->SBP_PB_TABLES[2];
    }
}

void ChooseMBModeInterlaceFrame(VC1Context* pContext,
                                Ipp32u MV4SWITCH,
                                Ipp32u MBMODETAB)
{
  if(MV4SWITCH == 1)
  {
    pContext->m_picLayerHeader->m_pMBMode =
        pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[MBMODETAB];
  }
  else
  {
    pContext->m_picLayerHeader->m_pMBMode =
        pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[MBMODETAB+4];
  }
}


void ChooseMBModeInterlaceField(VC1Context* pContext, Ipp32s MBMODETAB)
{
  if(pContext->m_picLayerHeader->MVMODE == VC1_MVMODE_MIXED_MV)
  {
    pContext->m_picLayerHeader->m_pMBMode =
        pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[MBMODETAB];
  }
  else
  {
    pContext->m_picLayerHeader->m_pMBMode =
        pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[MBMODETAB];
  }
}

 void ChoosePredScaleValuePPictbl(VC1PictureLayerHeader* picLayerHeader)
 {
    if(picLayerHeader->CurrField == 0)
    {
        picLayerHeader->m_pCurrPredScaleValuePPictbl
            = &VC1_PredictScaleValuesPPicTbl1[picLayerHeader->REFDIST];
    }
    else
        picLayerHeader->m_pCurrPredScaleValuePPictbl
            = &VC1_PredictScaleValuesPPicTbl2[picLayerHeader->REFDIST];
 }

 void ChoosePredScaleValueBPictbl(VC1PictureLayerHeader* picLayerHeader)
 {

     Ipp32u FREFDIST = ((picLayerHeader->ScaleFactor * picLayerHeader->REFDIST) >> 8);
     Ipp32s BREFDIST = picLayerHeader->REFDIST - FREFDIST - 1;

     if (BREFDIST < 0)
         BREFDIST = 0;

     picLayerHeader->m_pCurrPredScaleValueB_BPictbl = &VC1_PredictScaleValuesBPicTbl1[BREFDIST];
     picLayerHeader->m_pCurrPredScaleValueP_BPictbl[1] = &VC1_PredictScaleValuesPPicTbl1[BREFDIST];


    if(picLayerHeader->CurrField == 0)
    {
        picLayerHeader->m_pCurrPredScaleValueP_BPictbl[0] = &VC1_PredictScaleValuesPPicTbl1[FREFDIST];
    }
    else
    {
        picLayerHeader->m_pCurrPredScaleValueP_BPictbl[0] = &VC1_PredictScaleValuesPPicTbl2[FREFDIST];
    }

 }

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
