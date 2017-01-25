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

#include "umc_vc1_video_decoder_sw.h"
#include "umc_video_data.h"
#include "umc_media_data_ex.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_dec_seq.h"
#include "vm_sys_info.h"
#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_dec_task.h"

#include "umc_memory_allocator.h"
#include "umc_vc1_common.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_exception.h"

#include "umc_vc1_common_dc_tbl.h"
#include "umc_vc1_common_cbpcy_tbl.h"
#include "umc_vc1_dec_run_level_tbl.h"
#include "umc_vc1_common_tables.h"
#include "umc_vc1_common_mvdiff_tbl.h"
#include "umc_vc1_common_ttmb_tbl.h"
#include "umc_vc1_common_interlace_mb_mode_tables.h"
#include "umc_vc1_common_interlace_mv_tables.h"
#include "umc_vc1_common_interlaced_cbpcy_tables.h"
#include "umc_vc1_common_mv_block_pattern_tables.h"
#include "umc_vc1_common_tables_adv.h"
#include "umc_vc1_common_acintra.h"
#include "umc_vc1_common_acinter.h"

#include "umc_vc1_huffman.h"

using namespace UMC;
using namespace UMC::VC1Common;
using namespace UMC::VC1Exceptions;

void SetDecodingTables(VC1Context* pContext)
{
    //////////////////////////////////////////////////////////////////////////
    //////////////////////Intra Decoding Sets/////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    pContext->m_vlcTbl->LowMotionIntraACDecodeSet.pRLTable = 0;
    pContext->m_vlcTbl->LowMotionIntraACDecodeSet.pDeltaLevelLast0 = VC1_LowMotionIntraDeltaLevelLast0;
    pContext->m_vlcTbl->LowMotionIntraACDecodeSet.pDeltaLevelLast1 = VC1_LowMotionIntraDeltaLevelLast1;
    pContext->m_vlcTbl->LowMotionIntraACDecodeSet.pDeltaRunLast0 = VC1_LowMotionIntraDeltaRunLast0;
    pContext->m_vlcTbl->LowMotionIntraACDecodeSet.pDeltaRunLast1 = VC1_LowMotionIntraDeltaRunLast1;


    pContext->m_vlcTbl->HighMotionIntraACDecodeSet.pRLTable = 0;
    pContext->m_vlcTbl->HighMotionIntraACDecodeSet.pDeltaLevelLast0 = VC1_HighMotionIntraDeltaLevelLast0;
    pContext->m_vlcTbl->HighMotionIntraACDecodeSet.pDeltaLevelLast1 = VC1_HighMotionIntraDeltaLevelLast1;
    pContext->m_vlcTbl->HighMotionIntraACDecodeSet.pDeltaRunLast0 = VC1_HighMotionIntraDeltaRunLast0;
    pContext->m_vlcTbl->HighMotionIntraACDecodeSet.pDeltaRunLast1 = VC1_HighMotionIntraDeltaRunLast1;

    pContext->m_vlcTbl->MidRateIntraACDecodeSet.pRLTable = 0;
    pContext->m_vlcTbl->MidRateIntraACDecodeSet.pDeltaLevelLast0 = VC1_MidRateIntraDeltaLevelLast0;
    pContext->m_vlcTbl->MidRateIntraACDecodeSet.pDeltaLevelLast1 = VC1_MidRateIntraDeltaLevelLast1;
    pContext->m_vlcTbl->MidRateIntraACDecodeSet.pDeltaRunLast0 = VC1_MidRateIntraDeltaRunLast0;
    pContext->m_vlcTbl->MidRateIntraACDecodeSet.pDeltaRunLast1 = VC1_MidRateIntraDeltaRunLast1;

    pContext->m_vlcTbl->HighRateIntraACDecodeSet.pRLTable = 0;
    pContext->m_vlcTbl->HighRateIntraACDecodeSet.pDeltaLevelLast0 = VC1_HighRateIntraDeltaLevelLast0;
    pContext->m_vlcTbl->HighRateIntraACDecodeSet.pDeltaLevelLast1 = VC1_HighRateIntraDeltaLevelLast1;
    pContext->m_vlcTbl->HighRateIntraACDecodeSet.pDeltaRunLast0 = VC1_HighRateIntraDeltaRunLast0;
    pContext->m_vlcTbl->HighRateIntraACDecodeSet.pDeltaRunLast1 = VC1_HighRateIntraDeltaRunLast1;



    pContext->m_vlcTbl->IntraACDecodeSetPQINDEXle7[0] = &pContext->m_vlcTbl->HighRateIntraACDecodeSet;
    pContext->m_vlcTbl->IntraACDecodeSetPQINDEXle7[1] = &pContext->m_vlcTbl->HighMotionIntraACDecodeSet;
    pContext->m_vlcTbl->IntraACDecodeSetPQINDEXle7[2] = &pContext->m_vlcTbl->MidRateIntraACDecodeSet;



    pContext->m_vlcTbl->IntraACDecodeSetPQINDEXgt7[0] = &pContext->m_vlcTbl->LowMotionIntraACDecodeSet;
    pContext->m_vlcTbl->IntraACDecodeSetPQINDEXgt7[1] = &pContext->m_vlcTbl->HighMotionIntraACDecodeSet;
    pContext->m_vlcTbl->IntraACDecodeSetPQINDEXgt7[2] = &pContext->m_vlcTbl->MidRateIntraACDecodeSet;



    //////////////////////////////////////////////////////////////////////////
    //////////////////////Inter Decoding Sets/////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    pContext->m_vlcTbl->LowMotionInterACDecodeSet.pRLTable = 0;
    pContext->m_vlcTbl->LowMotionInterACDecodeSet.pDeltaLevelLast0 = VC1_LowMotionInterDeltaLevelLast0;
    pContext->m_vlcTbl->LowMotionInterACDecodeSet.pDeltaLevelLast1 = VC1_LowMotionInterDeltaLevelLast1;
    pContext->m_vlcTbl->LowMotionInterACDecodeSet.pDeltaRunLast0 = VC1_LowMotionInterDeltaRunLast0;
    pContext->m_vlcTbl->LowMotionInterACDecodeSet.pDeltaRunLast1 = VC1_LowMotionInterDeltaRunLast1;

    pContext->m_vlcTbl->HighMotionInterACDecodeSet.pRLTable = 0;
    pContext->m_vlcTbl->HighMotionInterACDecodeSet.pDeltaLevelLast0 = VC1_HighMotionInterDeltaLevelLast0;
    pContext->m_vlcTbl->HighMotionInterACDecodeSet.pDeltaLevelLast1 = VC1_HighMotionInterDeltaLevelLast1;
    pContext->m_vlcTbl->HighMotionInterACDecodeSet.pDeltaRunLast0 = VC1_HighMotionInterDeltaRunLast0;
    pContext->m_vlcTbl->HighMotionInterACDecodeSet.pDeltaRunLast1 = VC1_HighMotionInterDeltaRunLast1;

    pContext->m_vlcTbl->MidRateInterACDecodeSet.pRLTable = 0;
    pContext->m_vlcTbl->MidRateInterACDecodeSet.pDeltaLevelLast0 = VC1_MidRateInterDeltaLevelLast0;
    pContext->m_vlcTbl->MidRateInterACDecodeSet.pDeltaLevelLast1 = VC1_MidRateInterDeltaLevelLast1;
    pContext->m_vlcTbl->MidRateInterACDecodeSet.pDeltaRunLast0 = VC1_MidRateInterDeltaRunLast0;
    pContext->m_vlcTbl->MidRateInterACDecodeSet.pDeltaRunLast1 = VC1_MidRateInterDeltaRunLast1;

    pContext->m_vlcTbl->HighRateInterACDecodeSet.pRLTable = 0;
    pContext->m_vlcTbl->HighRateInterACDecodeSet.pDeltaLevelLast0 = VC1_HighRateInterDeltaLevelLast0;
    pContext->m_vlcTbl->HighRateInterACDecodeSet.pDeltaLevelLast1 = VC1_HighRateInterDeltaLevelLast1;
    pContext->m_vlcTbl->HighRateInterACDecodeSet.pDeltaRunLast0 = VC1_HighRateInterDeltaRunLast0;
    pContext->m_vlcTbl->HighRateInterACDecodeSet.pDeltaRunLast1 = VC1_HighRateInterDeltaRunLast1;

    pContext->m_vlcTbl->InterACDecodeSetPQINDEXle7[0] = &pContext->m_vlcTbl->HighRateInterACDecodeSet;
    pContext->m_vlcTbl->InterACDecodeSetPQINDEXle7[1] = &pContext->m_vlcTbl->HighMotionInterACDecodeSet;
    pContext->m_vlcTbl->InterACDecodeSetPQINDEXle7[2] = &pContext->m_vlcTbl->MidRateInterACDecodeSet;



    pContext->m_vlcTbl->InterACDecodeSetPQINDEXgt7[0] = &pContext->m_vlcTbl->LowMotionInterACDecodeSet;
    pContext->m_vlcTbl->InterACDecodeSetPQINDEXgt7[1] = &pContext->m_vlcTbl->HighMotionInterACDecodeSet;
    pContext->m_vlcTbl->InterACDecodeSetPQINDEXgt7[2] = &pContext->m_vlcTbl->MidRateInterACDecodeSet;



};

Ipp32s InitCommonTables(VC1Context* pContext)
{
    //MOTION DC DIFF
    if (0 != HuffmanTableInitAlloc(
        VC1_LowMotionLumaDCDiff, &pContext->m_vlcTbl->m_pLowMotionLumaDCDiff))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_HighMotionLumaDCDiff, &pContext->m_vlcTbl->m_pHighMotionLumaDCDiff))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_LowMotionChromaDCDiff, &pContext->m_vlcTbl->m_pLowMotionChromaDCDiff))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_HighMotionChromaDCDiff, &pContext->m_vlcTbl->m_pHighMotionChromaDCDiff))
        return 0;

    //CBPCY
    if (0 != HuffmanTableInitAlloc(
        VC1_CBPCY_Ipic, &pContext->m_vlcTbl->m_pCBPCY_Ipic))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_CBPCY_PBpic_tbl0,
        &pContext->m_vlcTbl->CBPCY_PB_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_CBPCY_PBpic_tbl1,
        &pContext->m_vlcTbl->CBPCY_PB_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_CBPCY_PBpic_tbl2,
        &pContext->m_vlcTbl->CBPCY_PB_TABLES[2]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_CBPCY_PBpic_tbl3,
        &pContext->m_vlcTbl->CBPCY_PB_TABLES[3]))
        return 0;

    //DEDCODE INDEX TABLES
    if (0 != HuffmanTableInitAlloc(
        VC1_LowMotionIntraAC,
        &pContext->m_vlcTbl->LowMotionIntraACDecodeSet.pRLTable))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_HighMotionIntraAC,
        &pContext->m_vlcTbl->HighMotionIntraACDecodeSet.pRLTable))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_LowMotionInterAC,
        &pContext->m_vlcTbl->LowMotionInterACDecodeSet.pRLTable))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_HighMotionInterAC,
        &pContext->m_vlcTbl->HighMotionInterACDecodeSet.pRLTable))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MidRateIntraAC,
        &pContext->m_vlcTbl->MidRateIntraACDecodeSet.pRLTable))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MidRateInterAC,
        &pContext->m_vlcTbl->MidRateInterACDecodeSet.pRLTable))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_HighRateIntraAC,
        &pContext->m_vlcTbl->HighRateIntraACDecodeSet.pRLTable))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_HighRateInterAC,
        &pContext->m_vlcTbl->HighRateInterACDecodeSet.pRLTable))
        return 0;

    //BITPLANE
    if (0 != HuffmanTableInitAlloc(
        VC1_Bitplane_IMODE_tbl,
        &pContext->m_vlcTbl->m_Bitplane_IMODE))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_BitplaneTaledbitsTbl,
        &pContext->m_vlcTbl->m_BitplaneTaledbits))
        return 0;

    //BFRACTION
    if (0 != HuffmanRunLevelTableInitAlloc(
        VC1_BFraction_tbl,
        &pContext->m_vlcTbl->BFRACTION))
        return 0;

    //MV DIFF PB TABLES
    if (0 != HuffmanTableInitAlloc(
        VC1_Progressive_MV_Diff_tbl0,
        &pContext->m_vlcTbl->MVDIFF_PB_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Progressive_MV_Diff_tbl1,
        &pContext->m_vlcTbl->MVDIFF_PB_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Progressive_MV_Diff_tbl2,
        &pContext->m_vlcTbl->MVDIFF_PB_TABLES[2]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Progressive_MV_Diff_tbl3,
        &pContext->m_vlcTbl->MVDIFF_PB_TABLES[3]))
        return 0;

    //TTMB PB TABLES
    if (0 != HuffmanTableInitAlloc(
        VC1_LowRateTTMB,
        &pContext->m_vlcTbl->TTMB_PB_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MediumRateTTMB,
        &pContext->m_vlcTbl->TTMB_PB_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_HighRateTTMB,
        &pContext->m_vlcTbl->TTMB_PB_TABLES[2]))
        return 0;

    //TTBLK PB TABLES
    if (0 != HuffmanTableInitAlloc(
        VC1_LowRateTTBLK,
        &pContext->m_vlcTbl->TTBLK_PB_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MediumRateTTBLK,
        &pContext->m_vlcTbl->TTBLK_PB_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_HighRateTTBLK,
        &pContext->m_vlcTbl->TTBLK_PB_TABLES[2]))
        return 0;

    //SUB BLOCK PATTERN
    if (0 != HuffmanTableInitAlloc(
        VC1_HighRateSBP,
        &pContext->m_vlcTbl->SBP_PB_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MediumRateSBP,
        &pContext->m_vlcTbl->SBP_PB_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_LowRateSBP,
        &pContext->m_vlcTbl->SBP_PB_TABLES[2]))
        return 0;

    return 1;
}

Ipp32s InitInterlacedTables(VC1Context* pContext)
{
    //MB MODE
    if (0 != HuffmanTableInitAlloc(
        VC1_4MV_MB_Mode_PBPic_Table0,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_4MV_MB_Mode_PBPic_Table1,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_4MV_MB_Mode_PBPic_Table2,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[2]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_4MV_MB_Mode_PBPic_Table3,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[3]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Non4MV_MB_Mode_PBPic_Table0,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[4]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Non4MV_MB_Mode_PBPic_Table1,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[5]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Non4MV_MB_Mode_PBPic_Table2,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[6]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Non4MV_MB_Mode_PBPic_Table3,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[7]))
        return 0;

    //MV TABLES
    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable0,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable1,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable2,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[2]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable3,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[3]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable4,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[4]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable5,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[5]))
        return 0;
    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable6,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[6]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable7,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[7]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable8,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[8]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable9,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[9]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable10,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[10]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedMVDifTable11,
        &pContext->m_vlcTbl->MV_INTERLACE_TABLES[11]))
        return 0;

    //CBPCY
    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedCBPCYTable0,
        &pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedCBPCYTable1,
        &pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedCBPCYTable2,
        &pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[2]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedCBPCYTable3,
        &pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[3]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedCBPCYTable4,
        &pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[4]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedCBPCYTable5,
        &pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[5]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedCBPCYTable6,
        &pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[6]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_InterlacedCBPCYTable7,
        &pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[7]))
        return 0;

    //2 MV BLOCK PATTERN P,B PICTURES
    if (0 != HuffmanTableInitAlloc(
        VC1_MV2BlockPatternTable0,
        &pContext->m_vlcTbl->MV2BP_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MV2BlockPatternTable1,
        &pContext->m_vlcTbl->MV2BP_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MV2BlockPatternTable2,
        &pContext->m_vlcTbl->MV2BP_TABLES[2]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MV2BlockPatternTable3,
        &pContext->m_vlcTbl->MV2BP_TABLES[3]))
        return 0;

    //4 MV BLOCK PATTERN
    if (0 != HuffmanTableInitAlloc(
        VC1_MV4BlockPatternTable0,
        &pContext->m_vlcTbl->MV4BP_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MV4BlockPatternTable1,
        &pContext->m_vlcTbl->MV4BP_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MV4BlockPatternTable2,
        &pContext->m_vlcTbl->MV4BP_TABLES[2]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_MV4BlockPatternTable3,
        &pContext->m_vlcTbl->MV4BP_TABLES[3]))
        return 0;

    //MB MODE TABLES
    if (0 != HuffmanTableInitAlloc(
        VC1_1MV_MB_ModeTable0,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_1MV_MB_ModeTable1,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_1MV_MB_ModeTable2,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[2]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_1MV_MB_ModeTable3,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[3]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_1MV_MB_ModeTable4,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[4]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_1MV_MB_ModeTable5,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[5]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_1MV_MB_ModeTable6,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[6]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_1MV_MB_ModeTable7,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[7]))
        return 0;

    //MIXEDMV MB TABLES
    if (0 != HuffmanTableInitAlloc(
        VC1_Mixed_MV_MB_ModeTable0,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[0]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Mixed_MV_MB_ModeTable1,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[1]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Mixed_MV_MB_ModeTable2,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[2]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Mixed_MV_MB_ModeTable3,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[3]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Mixed_MV_MB_ModeTable4,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[4]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Mixed_MV_MB_ModeTable5,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[5]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Mixed_MV_MB_ModeTable6,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[6]))
        return 0;

    if (0 != HuffmanTableInitAlloc(
        VC1_Mixed_MV_MB_ModeTable7,
        &pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[7]))
        return 0;

    //REFDIST
    if (0 != HuffmanTableInitAlloc(
        VC1_FieldRefdistTable,
        &pContext->m_vlcTbl->REFDIST_TABLE))
        return 0;

    return 1;
}

VC1VideoDecoderSW::VC1VideoDecoderSW():
    m_pdecoder(NULL),
    m_bIsFrameToOut(false),
    m_iRefFramesDst(0),
    m_iBFramesDst(0),
    m_pPrevDescriptor(NULL),
    m_pStoreSW(0)
{
}

VC1VideoDecoderSW::~VC1VideoDecoderSW()
{
    Close();
}


Status VC1VideoDecoderSW::Init(BaseCodecParams *pInit)
{
    Status umcRes = UMC_OK;
    umcRes = VC1VideoDecoder::Init(pInit);
    if (umcRes != UMC_OK)
        return umcRes;

    VideoDecoderParams *init = DynamicCast<VideoDecoderParams, BaseCodecParams>(pInit);
    if(!init)
        return UMC_ERR_INIT;

    try
    // memory allocation and Init all env for frames/tasks store - VC1TaskStore object
    {
        m_pStoreSW = (VC1TaskStoreSW*)m_pHeap->s_alloc<VC1TaskStoreSW>();
        m_pStore = m_pStoreSW;
        m_pStore = new(m_pStore) VC1TaskStoreSW(m_pMemoryAllocator);

        if (!m_pStore->Init(m_iThreadDecoderNum,
                            m_iMaxFramesInProcessing,
                            this) )
            return UMC_ERR_ALLOC;


        m_pStoreSW->CreateDSQueue(m_pContext,m_bIsReorder,0);

        m_pStoreSW->SetDefinition(&m_pContext->m_seqLayerHeader);
        m_pStoreSW->FillIdxVector(2 * (m_iMaxFramesInProcessing + VC1NUMREFFRAMES));

        // create thread decoders for multithreading support
        {
            m_pHeap->s_new(&m_pdecoder,m_iThreadDecoderNum);
            memset(m_pdecoder, 0, sizeof(VC1ThreadDecoder*) * m_iThreadDecoderNum);

            for (Ipp32u i = 0; i < m_iThreadDecoderNum; i += 1)
            {
                m_pHeap->s_new(&m_pdecoder[i]);
            }

            for (Ipp32u i = 0;i < m_iThreadDecoderNum;i += 1)
            {
                if (UMC_OK != m_pdecoder[i]->Init(m_pContext,i,m_pStore,m_pMemoryAllocator,NULL))
                    return UMC_ERR_INIT;
            }
        }
    }
    catch(...)
    {
        // only allocation errors here
        Close();
        return UMC_ERR_ALLOC;
    }

    //internal decoding flags
    m_iRefFramesDst = 0;
    m_iBFramesDst = 0;

    return umcRes;
}

Status  VC1VideoDecoderSW::Reset()
{
    m_bIsFrameToOut = false;
    m_iRefFramesDst = 0;
    m_iBFramesDst = 0;

    Status sts = VC1VideoDecoder::Reset();

    if (sts != UMC_OK)
        return sts;

    if (m_pContext->m_pSingleMB)
    {
        m_pContext->m_pSingleMB->m_currMBXpos = -1;
        m_pContext->m_pSingleMB->m_currMBYpos = 0;
    }

    m_pContext->m_pCurrMB = &m_pContext->m_MBs[0];
    m_pContext->CurrDC = m_pContext->DCACParams;

    if (m_pStore && m_pStoreSW)
    {
        if (!m_pStore->Reset())
            return UMC_ERR_NOT_INITIALIZED;

        m_pStoreSW->CreateDSQueue(&m_pInitContext, m_bIsReorder, 0);
        m_pStoreSW->SetDefinition(&(m_pInitContext.m_seqLayerHeader));
        m_pStoreSW->FillIdxVector(2 * (m_iMaxFramesInProcessing + VC1NUMREFFRAMES));
    }

    return sts;
}

Ipp32u VC1VideoDecoderSW::CalculateHeapSize()
{
    Ipp32u Size = 0;
    Ipp32u counter = 0;

    Size += align_value<Ipp32u>(sizeof(VC1TaskStoreSW));
    Size += align_value<Ipp32u>(sizeof(VC1ThreadDecoder**)*m_iThreadDecoderNum);
    Size += align_value<Ipp32u>(sizeof(Frame)*(2*m_iMaxFramesInProcessing + 2*VC1NUMREFFRAMES));

    for (counter = 0; counter < m_iThreadDecoderNum; counter += 1)
    {
        Size += align_value<Ipp32u>(sizeof(VC1ThreadDecoder));
    }

    Size += align_value<Ipp32u>(sizeof(MediaDataEx));

    return Size;
}

Status VC1VideoDecoderSW::Close(void)
{
    Status umcRes = UMC_OK;

    m_AllocBuffer = 0;

    // reset all values 
    umcRes = Reset();

    if(m_pdecoder)
    {
        for(Ipp32u i = 0; i < m_iThreadDecoderNum; i += 1)
        {
            if(m_pdecoder[i])
            {
                delete m_pdecoder[i];
                m_pdecoder[i] = 0;
            }
        }
        m_pdecoder = NULL;
    }

    if (m_pStore)
    {
        delete m_pStore;
        m_pStore = 0;
    }
    
    FreeAlloc(m_pContext);

    if(m_pMemoryAllocator)
    {
        if (static_cast<int>(m_iMemContextID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iMemContextID);
            m_pMemoryAllocator->Free(m_iMemContextID);
            m_iMemContextID = (MemID)-1;
        }

        if (static_cast<int>(m_iHeapID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iHeapID);
            m_pMemoryAllocator->Free(m_iHeapID);
            m_iHeapID = (MemID)-1;
        }

        if (static_cast<int>(m_iFrameBufferID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iFrameBufferID);
            m_pMemoryAllocator->Free(m_iFrameBufferID);
            m_iFrameBufferID = (MemID)-1;
        }
    }

    m_pContext = NULL;
    m_dataBuffer = NULL;
    m_stCodes = NULL;
    m_frameData = NULL;
    m_pHeap = NULL;

    memset(&m_pInitContext,0,sizeof(VC1Context));

    m_pMemoryAllocator = 0;

#if defined (_WIN32) && (_DEBUG)
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).Release();
#endif
#endif

    m_iThreadDecoderNum = 0;
    m_decoderInitFlag = 0;
    m_decoderFlags = 0;
    return umcRes;
}

Status VC1VideoDecoderSW::VC1DecodeFrame(MediaData* in, VideoData* out_data)
{
    Status umcRes = UMC_OK;
    bool skip = false;

    VC1FrameDescriptor *pCurrDescriptor = NULL;

    m_pStore->GetReadyDS(&pCurrDescriptor);

    if (NULL == pCurrDescriptor)
        throw vc1_exception(internal_pipeline_error);


    try // check input frame for valid
    {
        umcRes = pCurrDescriptor->preProcData(m_pContext,
                                              GetCurrentFrameSize(),
                                              m_lFrameCount,
                                              skip);
        if (UMC_OK != umcRes)
        {

            if (UMC_ERR_NOT_ENOUGH_DATA == umcRes)
                throw vc1_exception(invalid_stream);
            else
                throw vc1_exception(internal_pipeline_error);
        }

        if (VC1_SKIPPED_FRAME == pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE)
            out_data->SetFrameType(D_PICTURE); // means skipped 
        else
            out_data->SetFrameType(I_PICTURE);

        if (!pCurrDescriptor->isEmpty())//check descriptor correctness
            pCurrDescriptor->VC1FrameDescriptor::processFrame(m_pContext->m_Offsets,m_pContext->m_values);

    }
    catch (vc1_exception ex)
    {
        if (invalid_stream == ex.get_exception_type())
        {
            if (fast_err_detect == vc1_except_profiler::GetEnvDescript().m_Profile)
                m_pStore->AddInvalidPerformedDS(pCurrDescriptor);
            else if (fast_decoding == vc1_except_profiler::GetEnvDescript().m_Profile)
                m_pStore->ResetPerformedDS(pCurrDescriptor);
            else
                // Error - let speak about it 
                m_pStore->ResetPerformedDS(pCurrDescriptor);
            // for smart decoding we should decode and reconstruct frame according to standard pipeline

            return  UMC_ERR_NOT_ENOUGH_DATA;
        }
        else if (mem_allocation_er == ex.get_exception_type())
            return UMC_ERR_LOCK;
        else
            return UMC_ERR_FAILED;
    }

    // Update round ctrl field of seq header.
    m_pContext->m_seqLayerHeader.RNDCTRL = pCurrDescriptor->m_pContext->m_seqLayerHeader.RNDCTRL;


    // for Intensity compensation om multi-threading model
    if (VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE))
    {
        m_pContext->m_bIntensityCompensation = pCurrDescriptor->m_pContext->m_bIntensityCompensation;
    }


    if (!pCurrDescriptor->isEmpty())//check descriptor correctness
    {
        if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
            m_pStoreSW->SetDstForFrameAdv(pCurrDescriptor,&m_iRefFramesDst,&m_iBFramesDst);
        else
            m_pStoreSW->SetDstForFrame(pCurrDescriptor,&m_iRefFramesDst,&m_iBFramesDst);
    }

    if (m_lFrameCount == 1)
    {
        if (m_bIsReorder)
            m_bLastFrameNeedDisplay = true;

        m_bIsFrameToOut = false;

    }
    else
        m_bIsFrameToOut = true;


    if (!m_bIsFrameToOut && (UMC_OK == umcRes))
        umcRes = UMC_ERR_NOT_ENOUGH_DATA;

    m_pCurrentIn->MoveDataPointer(m_pContext->m_FrameSize);
    return umcRes;
}

bool VC1VideoDecoderSW::InitTables(VC1Context* pContext)
{
    SetDecodingTables(pContext);

    if (!InitCommonTables(pContext))
        return false;

    if (pContext->m_seqLayerHeader.INTERLACE)
        if (!InitInterlacedTables(pContext))
            return false;

    return true;
}

void VC1VideoDecoderSW::FreeTables(VC1Context* pContext)
{
    Ipp32s i;

    if (pContext->m_vlcTbl->m_pLowMotionLumaDCDiff)
    {
        HuffmanTableFree(pContext->m_vlcTbl->m_pLowMotionLumaDCDiff);
        pContext->m_vlcTbl->m_pLowMotionLumaDCDiff = NULL;
    }

    if (pContext->m_vlcTbl->m_pHighMotionLumaDCDiff)
    {
        HuffmanTableFree(pContext->m_vlcTbl->m_pHighMotionLumaDCDiff);
        pContext->m_vlcTbl->m_pHighMotionLumaDCDiff = NULL;
    }

    if (pContext->m_vlcTbl->m_pLowMotionChromaDCDiff)
    {
        HuffmanTableFree(pContext->m_vlcTbl->m_pLowMotionChromaDCDiff);
        pContext->m_vlcTbl->m_pLowMotionChromaDCDiff = NULL;
    }

    if (pContext->m_vlcTbl->m_pHighMotionChromaDCDiff)
    {
        HuffmanTableFree(pContext->m_vlcTbl->m_pHighMotionChromaDCDiff);
        pContext->m_vlcTbl->m_pHighMotionChromaDCDiff = NULL;
    }

    if (pContext->m_vlcTbl->m_pCBPCY_Ipic)
    {
        HuffmanTableFree(pContext->m_vlcTbl->m_pCBPCY_Ipic);
        pContext->m_vlcTbl->m_pCBPCY_Ipic = NULL;
    }

    if (pContext->m_vlcTbl->LowMotionInterACDecodeSet.pRLTable)
    {
        HuffmanTableFree(pContext->m_vlcTbl->LowMotionInterACDecodeSet.pRLTable);
        pContext->m_vlcTbl->LowMotionInterACDecodeSet.pRLTable = NULL;
    }

    if (pContext->m_vlcTbl->HighMotionInterACDecodeSet.pRLTable)
    {
        HuffmanTableFree(pContext->m_vlcTbl->HighMotionInterACDecodeSet.pRLTable);
        pContext->m_vlcTbl->HighMotionInterACDecodeSet.pRLTable = NULL;
    }

    if (pContext->m_vlcTbl->LowMotionIntraACDecodeSet.pRLTable)
    {
        HuffmanTableFree(pContext->m_vlcTbl->LowMotionIntraACDecodeSet.pRLTable);
        pContext->m_vlcTbl->LowMotionIntraACDecodeSet.pRLTable = NULL;
    }

    if (pContext->m_vlcTbl->HighMotionIntraACDecodeSet.pRLTable)
    {
        HuffmanTableFree(pContext->m_vlcTbl->HighMotionIntraACDecodeSet.pRLTable);
        pContext->m_vlcTbl->HighMotionIntraACDecodeSet.pRLTable = NULL;
    }


    if (pContext->m_vlcTbl->MidRateIntraACDecodeSet.pRLTable)
    {
        HuffmanTableFree(pContext->m_vlcTbl->MidRateIntraACDecodeSet.pRLTable);
        pContext->m_vlcTbl->MidRateIntraACDecodeSet.pRLTable = NULL;
    }

    if (pContext->m_vlcTbl->MidRateInterACDecodeSet.pRLTable)
    {
        HuffmanTableFree(pContext->m_vlcTbl->MidRateInterACDecodeSet.pRLTable);
        pContext->m_vlcTbl->MidRateInterACDecodeSet.pRLTable = NULL;
    }

    if (pContext->m_vlcTbl->HighRateIntraACDecodeSet.pRLTable)
    {
        HuffmanTableFree(pContext->m_vlcTbl->HighRateIntraACDecodeSet.pRLTable);
        pContext->m_vlcTbl->HighRateIntraACDecodeSet.pRLTable = NULL;
    }

    if (pContext->m_vlcTbl->HighRateInterACDecodeSet.pRLTable)
    {
        HuffmanTableFree(pContext->m_vlcTbl->HighRateInterACDecodeSet.pRLTable);
        pContext->m_vlcTbl->HighRateInterACDecodeSet.pRLTable = NULL;
    }

    if (pContext->m_vlcTbl->m_Bitplane_IMODE)
    {
        HuffmanTableFree(pContext->m_vlcTbl->m_Bitplane_IMODE);
        pContext->m_vlcTbl->m_Bitplane_IMODE = NULL;
    }

    if (pContext->m_vlcTbl->m_BitplaneTaledbits)
    {
        HuffmanTableFree(pContext->m_vlcTbl->m_BitplaneTaledbits);
        pContext->m_vlcTbl->m_BitplaneTaledbits = NULL;
    }

    if (pContext->m_vlcTbl->REFDIST_TABLE)
    {
        HuffmanTableFree(pContext->m_vlcTbl->REFDIST_TABLE);
        pContext->m_vlcTbl->REFDIST_TABLE = NULL;
    }


    {
        for (i = 0; i < 4; i++)
        {
            if (pContext->m_vlcTbl->MVDIFF_PB_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->MVDIFF_PB_TABLES[i]);
                pContext->m_vlcTbl->MVDIFF_PB_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 4; i++)
        {
            if (pContext->m_vlcTbl->CBPCY_PB_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->CBPCY_PB_TABLES[i]);
                pContext->m_vlcTbl->CBPCY_PB_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 3; i++)
        {
            if (pContext->m_vlcTbl->TTMB_PB_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->TTMB_PB_TABLES[i]);
                pContext->m_vlcTbl->TTMB_PB_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 8; i++)
        {
            if (pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[i]);
                pContext->m_vlcTbl->MBMODE_INTERLACE_FRAME_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 4; i++)
        {
            if (pContext->m_vlcTbl->MV2BP_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->MV2BP_TABLES[i]);
                pContext->m_vlcTbl->MV2BP_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 3; i++)
        {
            if (pContext->m_vlcTbl->TTBLK_PB_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->TTBLK_PB_TABLES[i]);
                pContext->m_vlcTbl->TTBLK_PB_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 3; i++)
        {
            if (pContext->m_vlcTbl->SBP_PB_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->SBP_PB_TABLES[i]);
                pContext->m_vlcTbl->SBP_PB_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 4; i++)
        {
            if (pContext->m_vlcTbl->MV4BP_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->MV4BP_TABLES[i]);
                pContext->m_vlcTbl->MV4BP_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 8; i++)
        {
            if (pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[i]);
                pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 8; i++)
        {
            if (pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[i]);
                pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 8; i++)
        {
            if (pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[i]);
                pContext->m_vlcTbl->MBMODE_INTERLACE_FIELD_MIXED_TABLES[i] = NULL;
            }
        }
    }

    {
        for (i = 0; i < 12; i++)
        {
            if (pContext->m_vlcTbl->MV_INTERLACE_TABLES[i])
            {
                HuffmanTableFree(pContext->m_vlcTbl->MV_INTERLACE_TABLES[i]);
                pContext->m_vlcTbl->MV_INTERLACE_TABLES[i] = 0;
            }
        }
    }

    if (pContext->m_vlcTbl->BFRACTION)
    {
        HuffmanTableFree(pContext->m_vlcTbl->BFRACTION);
        pContext->m_vlcTbl->BFRACTION = NULL;
    }
}

bool VC1VideoDecoderSW::InitAlloc(VC1Context* pContext, Ipp32u MaxFrameNum)
{
    if(!InitTables(pContext))
        return false;

    //frames allocation
    Ipp32s i;
    Ipp32s n_references = MaxFrameNum + VC1NUMREFFRAMES;

    for(i = 0; i < n_references; i++)
    {
        pContext->m_frmBuff.m_pFrames[i].RANGE_MAPY  = -1;
        pContext->m_frmBuff.m_pFrames[i].RANGE_MAPUV = -1;
        pContext->m_frmBuff.m_pFrames[i].pRANGE_MAPY = &pContext->m_frmBuff.m_pFrames[i].RANGE_MAPY;
        pContext->m_frmBuff.m_pFrames[i].LumaTablePrev[0] = 0;
        pContext->m_frmBuff.m_pFrames[i].LumaTablePrev[1] = 0;
        pContext->m_frmBuff.m_pFrames[i].LumaTableCurr[0] = 0;
        pContext->m_frmBuff.m_pFrames[i].LumaTableCurr[1] = 0;
        pContext->m_frmBuff.m_pFrames[i].ChromaTablePrev[0] = 0;
        pContext->m_frmBuff.m_pFrames[i].ChromaTablePrev[1] = 0;
        pContext->m_frmBuff.m_pFrames[i].ChromaTableCurr[0] = 0;
        pContext->m_frmBuff.m_pFrames[i].ChromaTableCurr[1] = 0;
    }

    pContext->m_frmBuff.m_iDisplayIndex = -1;
    pContext->m_frmBuff.m_iCurrIndex    =  0;
    pContext->m_frmBuff.m_iPrevIndex    = -1;
    pContext->m_frmBuff.m_iNextIndex    = -1;
    pContext->m_frmBuff.m_iRangeMapIndex   =  MaxFrameNum + VC1NUMREFFRAMES - 1;
    pContext->m_frmBuff.m_iToFreeIndex = -1;
    return true;
}

bool VC1VideoDecoderSW::InitVAEnvironment()
{
    if (!m_pContext->m_frmBuff.m_pFrames)
    {
        m_pHeap->s_new_one(&m_pContext->m_frmBuff.m_pFrames, 2 * (m_iMaxFramesInProcessing + VC1NUMREFFRAMES));
        if (!m_pContext->m_frmBuff.m_pFrames)
            return false;
    }
    return true;
}

Status VC1VideoDecoderSW::GetAndProcessPerformedDS(MediaData* in, VideoData* out_data, UMC::VC1FrameDescriptor **pPerfDescriptor)
{
    VC1FrameDescriptor *pCurrDescriptor = NULL;

    Status umcRes = UMC_OK;

    {
        if (m_pStore->GetPerformedDS(&pCurrDescriptor))
        {
            m_pDescrToDisplay = pCurrDescriptor;

            if (!pCurrDescriptor->isDescriptorValid())
            {
                umcRes = UMC_ERR_FAILED;
            }
            else
            {

                if (VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_InitPicLayer->PTYPE))
                {
                    Ipp16u heightMB = m_pContext->m_seqLayerHeader.MaxHeightMB;
                    if (pCurrDescriptor->m_pContext->m_InitPicLayer->FCM == VC1_FieldInterlace)
                    {
                        heightMB = m_pContext->m_seqLayerHeader.MaxHeightMB + (m_pContext->m_seqLayerHeader.MaxHeightMB & 1);
                        MFX_INTERNAL_CPY(m_pContext->savedMVSamePolarity_Curr,
                            pCurrDescriptor->m_pContext->savedMVSamePolarity,
                            heightMB*m_pContext->m_seqLayerHeader.MaxWidthMB);
                    }

                    MFX_INTERNAL_CPY(m_pContext->savedMV_Curr, 
                        pCurrDescriptor->m_pContext->savedMV,
                        heightMB*m_pContext->m_seqLayerHeader.MaxWidthMB * 2 * 2*sizeof(Ipp16s));
                }

                if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
                    PrepareForNextFrame(pCurrDescriptor->m_pContext);

                if (pCurrDescriptor->m_iFrameCounter > 1)
                {
                    m_pStoreSW->OpenNextFrames(pCurrDescriptor, &m_pPrevDescriptor, &m_iRefFramesDst, &m_iBFramesDst);
                }
                else
                {
                    m_pStoreSW->OpenNextFrames(pCurrDescriptor, &m_pPrevDescriptor, &m_iRefFramesDst, &m_iBFramesDst);
                    if (umcRes == UMC_OK)
                        umcRes = UMC_ERR_NOT_ENOUGH_DATA;
                }
                m_pContext->m_picLayerHeader = m_pContext->m_InitPicLayer;
            }
        }
    }
    *pPerfDescriptor = pCurrDescriptor;
    return umcRes;

}

Status VC1VideoDecoderSW::ProcessPrevFrame(VC1FrameDescriptor *pCurrDescriptor, MediaData* in, VideoData* out_data)
{
    Status     umcRes = UMC::UMC_OK;
    if (m_pPrevDescriptor)
    {
        if (m_pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
        {
            RangeRefFrame(m_pPrevDescriptor->m_pContext);
        }
        else
            m_pStoreSW->CompensateDSInQueue(m_pPrevDescriptor);

        m_pStoreSW->WakeTasksInAlienQueue(m_pPrevDescriptor);
    }
    return umcRes;
}

FrameMemID  VC1VideoDecoderSW::ProcessQueuesForNextFrame(bool& isSkip, mfxU16& Corrupted)
{
    FrameMemID currIndx = -1;
    Status     umcRes = UMC_OK;
    UMC::VC1FrameDescriptor *pCurrDescriptor = NULL;
    UMC::VC1FrameDescriptor *pPerformedDescriptor = NULL;
    pCurrDescriptor = m_pStore->GetFirstDS();

    m_RMIndexToFree = -1;
    m_CurrIndexToFree = -1;

    if (pCurrDescriptor)
    {
        SetCorrupted(pCurrDescriptor, Corrupted);
        if (pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
        {
            isSkip = true;
            m_pStore->GetReadySkippedDS(&pCurrDescriptor);

            if (!pCurrDescriptor)
                return -1; // error

            if (!pCurrDescriptor->isDescriptorValid())
            {
                return m_pStore->GetIdx(currIndx);
            }
            else
            {
                m_pDescrToDisplay = pCurrDescriptor;
                if (pCurrDescriptor->isSpecialBSkipFrame())
                {
                    pCurrDescriptor->m_pContext->m_frmBuff.m_iDisplayIndex = pCurrDescriptor->m_pContext->m_frmBuff.m_iPrevIndex;
                    pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE = VC1_B_FRAME;
                }
                else
                {
                    m_pStore->UnLockSurface(pCurrDescriptor->m_pContext->m_frmBuff.m_iToSkipCoping);
                    if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
                        PrepareForNextFrame(pCurrDescriptor->m_pContext);
                }
                m_pStoreSW->OpenNextFrames(pCurrDescriptor, &m_pPrevDescriptor, &m_iRefFramesDst, &m_iBFramesDst);
                currIndx = m_pStore->GetIdx(pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex);
            }

            // Range Map
            if ((pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG) ||
                (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG) ||
                (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGERED))
            {
                currIndx = m_pStore->GetIdx(pCurrDescriptor->m_pContext->m_frmBuff.m_iRangeMapIndex);
            }
            return currIndx;
        }
        else
        {
            umcRes = GetAndProcessPerformedDS(0, 0, &pPerformedDescriptor);
            currIndx = m_pStore->GetIdx(pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex);

            if ((UMC_OK == umcRes) ||
                (UMC_ERR_NOT_ENOUGH_DATA == umcRes))
            {
                //Range Map
                if ((pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG) ||
                    (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG) ||
                    (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGERED))
                {
                    RangeMapping(pCurrDescriptor->m_pContext, pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex, pCurrDescriptor->m_pContext->m_frmBuff.m_iRangeMapIndex);
                    currIndx = m_pStore->GetIdx(pCurrDescriptor->m_pContext->m_frmBuff.m_iRangeMapIndex);
                }
            }
            umcRes = ProcessPrevFrame(0, 0, 0);
            if (umcRes != UMC_OK)
                return currIndx;


            // Asynchrony Unlock
            if (!VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE))
            {
                m_CurrIndexToFree = pCurrDescriptor->m_pContext->m_frmBuff.m_iDisplayIndex;
                if ((pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG) ||
                    (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG) ||
                    (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGERED))
                {
                    m_RMIndexToFree = pCurrDescriptor->m_pContext->m_frmBuff.m_iRangeMapIndex;
                }
            }
            else
            {
                if (pCurrDescriptor->m_pContext->m_frmBuff.m_iToFreeIndex > -1)
                {
                    m_CurrIndexToFree = pCurrDescriptor->m_pContext->m_frmBuff.m_iToFreeIndex;
                    if ((pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG) ||
                        (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG) ||
                        (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGERED))
                    {
                        m_RMIndexToFree = pCurrDescriptor->m_pContext->m_frmBuff.m_iRangeMapIndexPrev;
                    }
                }

            }
        }
    }

    return currIndx;
}

UMC::FrameMemID VC1VideoDecoderSW::GetSkippedIndex(bool isIn)
{
    return VC1VideoDecoder::GetSkippedIndex(m_pStore->GetFirstDS(), isIn);
}

Status VC1VideoDecoderSW::RunThread(int threadNumber)
{
    return m_pdecoder[threadNumber]->process();
}

FrameMemID  VC1VideoDecoderSW::GetDisplayIndex(bool isDecodeOrder, bool isSamePolarSurf)
{
    UMC::VC1FrameDescriptor *pCurrDescriptor = 0;
    pCurrDescriptor = m_pStore->GetLastDS();
    if (!pCurrDescriptor)
        return -1;

    if ((VC1_SKIPPED_FRAME == pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE) && isSamePolarSurf)
    {
        return m_pStore->GetIdx(pCurrDescriptor->m_pContext->m_frmBuff.m_iToSkipCoping);
    }

    return VC1VideoDecoder::GetDisplayIndex(isDecodeOrder, isSamePolarSurf);
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER

