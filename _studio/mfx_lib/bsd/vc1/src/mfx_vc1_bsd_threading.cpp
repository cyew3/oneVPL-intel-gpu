/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2011 Intel Corporation. All Rights Reserved.
//          MFX VC-1 BSD threading support
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_BSD)

#include "mfx_vc1_bsd_threading.h"
#include "mfx_vc1_bsd_utils.h"
#include "umc_vc1_dec_seq.h"

typedef VC1Status (*B_MB_DECODE)(VC1Context* pContext);

static  B_MB_DECODE B_MB_Dispatch_table[] = {
        (MBLayer_ProgressiveBpicture_NONDIRECT_Prediction),
        (MBLayer_ProgressiveBpicture_DIRECT_Prediction),
        (MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_Prediction),
        (MBLayer_ProgressiveBpicture_SKIP_DIRECT_Prediction)
};
static  B_MB_DECODE B_MB_Dispatch_table_AdvProgr[] = {
        (MBLayer_ProgressiveBpicture_NONDIRECT_AdvPrediction),
        (MBLayer_ProgressiveBpicture_DIRECT_AdvPrediction),
        (MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_AdvPrediction),
        (MBLayer_ProgressiveBpicture_SKIP_DIRECT_AdvPrediction)
};

// Task queue
mfxStatus VC1MfxTQueueBsd::Init()
{
    return MFX_ERR_NONE;
}
mfxStatus VC1MfxTQueueBsd::FormalizeSliceTaskGroup(MfxVC1ThreadUnitParamsBSD* pSliceParams)
{
    MFX_CHECK_NULL_PTR1(pSliceParams);
    Ipp16u curMBrow = pSliceParams->BaseSlice.MBStartRow;
    // parameters fro single task
    MfxVC1ThreadUnitParamsBSD TaskParams;

    bool isFirstSlieceDecodeTask = true;
    while (curMBrow  < pSliceParams->BaseSlice.MBEndRow)
    {
        //VC1BSDRunBSD pTask = m_pRunBsdQueue[m_iTasksInQueue];
        TaskParams.BaseSlice.isFirstInSlice = isFirstSlieceDecodeTask;

        if (isFirstSlieceDecodeTask)
            isFirstSlieceDecodeTask = false;

        TaskParams.BaseSlice.MBStartRow = curMBrow;
        TaskParams.BaseSlice.MBEndRow = curMBrow + VC1MBQUANT;
        TaskParams.BaseSlice.MBRowsToDecode = VC1MBQUANT;


        if ((TaskParams.BaseSlice.MBRowsToDecode + TaskParams.BaseSlice.MBStartRow) > pSliceParams->BaseSlice.MBEndRow)
        {
            TaskParams.BaseSlice.MBEndRow = pSliceParams->BaseSlice.MBEndRow;
            TaskParams.BaseSlice.MBRowsToDecode = TaskParams.BaseSlice.MBEndRow - pSliceParams->BaseSlice.MBStartRow;
        }
        TaskParams.BaseSlice.isLastInSlice = false; // need to correct
        TaskParams.BaseSlice.pContext = pSliceParams->BaseSlice.pContext;
        TaskParams.BaseSlice.pCUC = pSliceParams->BaseSlice.pCUC;

        m_pRunBsdQueue[m_iTasksInQueue].Init(&TaskParams.BaseSlice,
                                             0,
                                             0,
                                             TaskParams.BaseSlice.isFirstInSlice);

        TaskParams.pBS = pSliceParams->pBS;
        TaskParams.BitOffset = pSliceParams->BitOffset;
        TaskParams.pPicLayerHeader = pSliceParams->pPicLayerHeader;
        TaskParams.pVlcTbl = pSliceParams->pVlcTbl;


        m_pRunBsdQueue[m_iTasksInQueue].InitSpecific(&TaskParams);

        ++m_iTasksInQueue;
        curMBrow += VC1MBQUANT;
        ++m_iCurrentTaskID;
    }
    return MFX_ERR_NONE;
}



VC1TaskMfxBase* VC1MfxTQueueBsd::GetNextStaticTask(Ipp32s threadID)
{
    Ipp32s count;
    for(count = 0; count < m_iTasksInQueue; count++)
    {
        if (m_pRunBsdQueue[count].IsReadyToWork()&&
            (m_pRunBsdQueue[count].GetThreadOwner() == threadID))
            return &m_pRunBsdQueue[count];
        else if (count > 0)
        {
            if ((m_pRunBsdQueue[count-1].IsDone()&&
                (!m_pRunBsdQueue[count].IsReadyToWork())&&
                (!m_pRunBsdQueue[count].IsDone())&&
                (m_pRunBsdQueue[count].GetThreadOwner() == threadID)))
            {
                m_pRunBsdQueue[count].SetDependenceParams(&m_pRunBsdQueue[count-1]);
                return &m_pRunBsdQueue[count];
            }
        }
    }
    return 0;
}
VC1TaskMfxBase* VC1MfxTQueueBsd::GetNextDynamicTask(Ipp32s threadID)
{
    // not implemented yet
    return 0;
}
bool VC1MfxTQueueBsd::IsFuncComplte()
{
    Ipp32s count;
    for(count = 0; count < m_iTasksInQueue; count++)
    {
        if (!m_pRunBsdQueue[count].IsDone())
            return false;
    }
    m_iTasksInQueue = 0;
    return true;
}
mfxStatus VC1BSDRunBSD::ProcessJob()
{

    volatile MBLayerDecode* pCurrMBTable = MBLayerDecode_tbl_Adv;
    if (m_pContext->m_seqLayerHeader->PROFILE != VC1_PROFILE_ADVANCED)
        pCurrMBTable = MBLayerDecode_tbl;
    // Routine task

    // we don't copy this field from pFrameDS
    m_pContext->m_pSingleMB =  &m_SingleMB;
    m_pContext->m_pCurrMB = &m_pContext->m_MBs[m_MBStartRow*m_pContext->m_seqLayerHeader->widthMB];
    m_pContext->CurrDC = &m_pContext->DCACParams[m_MBStartRow*m_pContext->m_seqLayerHeader->widthMB];

    m_pContext->m_pSingleMB->m_currMBXpos = 0;
    m_pContext->m_pSingleMB->m_currMBYpos =  m_MBStartRow;
    m_pContext->m_pSingleMB->slice_currMBYpos = m_MBEndRow;

    //TBD. Escape info
    //m_pContext->m_pSingleMB->EscInfo = task->m_pSlice->EscInfo;

    if ((m_pContext->m_picLayerHeader->CurrField)&&(m_pContext->m_pSingleMB->slice_currMBYpos))
        m_pContext->m_pSingleMB->slice_currMBYpos -= m_pContext->m_seqLayerHeader->heightMB/2;

    //m_pContext->m_pBlock = m_pResDBuffer;

    m_pContext->m_bitstream.pBitstream = m_pBS;
    m_pContext->m_bitstream.bitOffset =  m_bitOffset;
    m_pContext->m_picLayerHeader = m_picLayerHeader;
    m_pContext->m_pSingleMB->widthMB = m_pContext->m_seqLayerHeader->widthMB;
    m_pContext->m_pSingleMB->heightMB = m_pContext->m_seqLayerHeader->heightMB;
    m_pContext->m_pSingleMB->EscInfo = m_EscInfo;

    if ( m_bIsFirstInSlice)
    {
        m_pContext->m_pSingleMB->slice_currMBYpos = 0;
        m_pContext->m_pSingleMB->EscInfo.levelSize = 0;
        m_pContext->m_pSingleMB->EscInfo.runSize = 0;


        if ((m_pContext->m_picLayerHeader->m_PQuant_mode > 0) ||
            (m_pContext->m_picLayerHeader->PQUANT <= 7))
            m_pContext->m_pSingleMB->EscInfo.bEscapeMode3Tbl = VC1_ESCAPEMODE3_Conservative;
        else
            m_pContext->m_pSingleMB->EscInfo.bEscapeMode3Tbl = VC1_ESCAPEMODE3_Efficient;
    }

    try
    {
        for (Ipp32s i=0; i < m_MBRowsToDecode;i++)
        {
            for (Ipp32s j = 0; j < m_pContext->m_seqLayerHeader->widthMB; j++)
            {
                //printf("m_pContext->m_pCurrMB->m_pBlocks[0].mv[0][1] %d\n",m_pContext->m_pCurrMB->m_pBlocks[0].mv[0][1]);
                m_pContext->m_pSingleMB->ACPRED = 0;
                pCurrMBTable[m_pContext->m_picLayerHeader->PTYPE*3 + m_pContext->m_picLayerHeader->FCM](m_pContext);
                // If B frame and non-direct we can calculate MV's in BSD
                if (m_pContext->m_picLayerHeader->PTYPE == VC1_B_FRAME)
                {
                    if (m_pContext->m_seqLayerHeader->PROFILE != VC1_PROFILE_ADVANCED )
                        B_MB_Dispatch_table[m_pContext->m_pCurrMB->SkipAndDirectFlag](m_pContext);
                    else
                        B_MB_Dispatch_table_AdvProgr[m_pContext->m_pCurrMB->SkipAndDirectFlag](m_pContext);
                }

                MfxVC1BSDPacking::FillParamsForOneMB(m_pContext, m_pContext->m_pCurrMB,
                                                     m_pContext->m_pSingleMB,
                                                     &m_pCUC->MbParam->Mb[(m_MBStartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j]);


                ++m_pContext->m_pSingleMB->m_currMBXpos;
                m_pContext->m_pBlock += 8*8*6;
                ++m_pContext->m_pCurrMB;
                ++m_pContext->CurrDC;
            }
            m_pContext->m_pSingleMB->m_currMBXpos = 0;
            ++m_pContext->m_pSingleMB->m_currMBYpos;
            ++m_pContext->m_pSingleMB->slice_currMBYpos;
        }

        m_pBS = m_pContext->m_bitstream.pBitstream;
        m_bitOffset = m_pContext->m_bitstream.bitOffset;
        //TBD
        //task->m_pSlice->EscInfo = m_pContext->m_pSingleMB->EscInfo;
        m_bIsDone = true;
        m_bIsReady = false;
        m_EscInfo = m_pContext->m_pSingleMB->EscInfo;
    }
    catch (...)
    {
        return MFX_ERR_NULL_PTR;
    }
    return MFX_ERR_NONE;
}
// Task
void  VC1BSDRunBSD::SetDependenceParams(VC1BSDRunBSD* pPrev)
{
    m_pBS = pPrev->m_pBS;
    m_bitOffset = pPrev->m_bitOffset;
    m_EscInfo = pPrev->m_EscInfo;
}
mfxStatus VC1BSDRunBSD::InitSpecific(MfxVC1ThreadUnitParamsBSD* pInitParams)
{
    MFX_CHECK_NULL_PTR1(pInitParams);
    m_pBS = pInitParams->pBS;
    m_bitOffset = pInitParams->BitOffset;
    m_picLayerHeader = pInitParams->pPicLayerHeader;
    m_vlcTbl = pInitParams->pVlcTbl;
    return MFX_ERR_NONE;
}



#endif
