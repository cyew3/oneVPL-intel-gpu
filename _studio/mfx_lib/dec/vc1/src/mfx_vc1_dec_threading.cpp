/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2011 Intel Corporation. All Rights Reserved.
//          MFX VC-1 DEC threading support
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_DEC)

#include "mfx_vc1_dec_threading.h"
#include "mfx_vc1_dec_utils.h"
#include "mfx_vc1_enc_ex_param_buf.h"
#include "mfx_enc_common.h"



using namespace MfxVC1DECUnpacking;
using namespace MFXVC1DecCommon;
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

VC1DECIQT::VC1DECIQT()
{
    m_pReconstructTbl[0] = &VC1ProcessDiffInter;
    m_pReconstructTbl[1] = &VC1ProcessDiffIntra;
}


mfxStatus VC1DECIQT::ProcessJob()
{
    //Unpacking to self VC1Context container

    mfxMbCode*      pMbCode;
    VC1MB*          pCurMB;

    // TBD
    //mfxI16  index  =  GetExBufferIndex(m_pCUC, MFX_LABEL_RESCOEFF, MFX_CUC_VC1_RESIDAUALBUF);
    mfxI16  index  =  0;
    ExtVC1Residual* pResidual = (ExtVC1Residual*)m_pCUC->ExtBuffer[index];
    Ipp16s* pResDif = pResidual->pRes;

    // set needed params
    m_pContext->m_pSingleMB =  &m_SingleMB;
    //m_pContext->CurrDC = &m_pContext->DCACParams[m_MBStartRow*m_pContext->m_seqLayerHeader->widthMB];

    m_pContext->m_pSingleMB->m_currMBXpos = 0;
    m_pContext->m_pSingleMB->m_currMBYpos =  m_MBStartRow;
    m_pContext->m_pSingleMB->slice_currMBYpos = m_MBEndRow;

    for (Ipp32s i = 0; i < m_MBRowsToDecode; i++)
    {
        for (Ipp32s j = 0; j < m_pContext->m_seqLayerHeader->widthMB; j++)
        {

            m_pContext->CurrDC = &m_pContext->DCACParams[(m_MBStartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j];
            pMbCode = &m_pCUC->MbParam->Mb[(m_MBStartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j];
            m_pContext->m_pCurrMB = pCurMB = &m_pContext->m_MBs[(m_MBStartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j];
            m_pContext->m_pBlock = pResDif + GetWordResOffset(pMbCode); // because offset in double words
            TranslateMfxToUmcIQT(pMbCode, pCurMB, m_pContext->CurrDC);
            BiasDetermination(m_pContext);
            //pCurMB->IntraFlag = pMbCode->SubMbPredMode;

            if(m_pContext->m_picLayerHeader->PTYPE == VC1_P_FRAME)
            {
                // We dont need to process Skip MB - TBD
                {
                    Ipp32u IntraFlag = pCurMB->IntraFlag;
                    for (Ipp32s blk_num = 0; blk_num<6 ;blk_num++)
                    {
                        m_pReconstructTbl[IntraFlag & 1](m_pContext,blk_num);
                        IntraFlag >>= 1;
                    }
                }
            }
            else
            {
                {
                    if (pCurMB->IntraFlag)
                    {
                        for (Ipp32s blk_num = 0; blk_num<6 ;blk_num++)
                            m_pReconstructTbl[1](m_pContext,blk_num);
                    }
                    else
                    {
                        for (Ipp32s blk_num = 0; blk_num<6 ;blk_num++)
                            m_pReconstructTbl[0](m_pContext,blk_num);
                    }
                }
            }
            ++m_pContext->m_pSingleMB->m_currMBXpos;
        }
        m_pContext->m_pSingleMB->m_currMBXpos = 0;
        ++m_pContext->m_pSingleMB->m_currMBYpos;
        ++m_pContext->m_pSingleMB->slice_currMBYpos;
    }
    m_bIsDone = true;
    m_bIsReady = false;
    return MFX_ERR_NONE;

}

VC1DECPredDEC::VC1DECPredDEC() // TBD need to set depend on the function type
{
}

mfxStatus VC1DECPredDEC::ProcessJob()
{
    mfxMbCode*      pMbCode;

    Ipp8u*          pY;
    Ipp8u*          pU;
    Ipp8u*          pV;

    Ipp32u          pitch;



    /////////////////////////////////
    //Need to correct
    //if(m_pContext->m_picLayerHeader->FCM != VC1_FieldInterlace)
    {
        m_pContext->interp_params_luma.srcStep   = m_pContext->m_frmBuff.m_pFrames[1].m_iYPitch;
        m_pContext->interp_params_chroma.srcStep = m_pContext->m_frmBuff.m_pFrames[1].m_iUPitch;
        m_pContext->interp_params_luma.dstStep   = m_pContext->m_frmBuff.m_pFrames[0].m_iYPitch;
        m_pContext->interp_params_chroma.dstStep = m_pContext->m_frmBuff.m_pFrames[0].m_iUPitch;

    }
    //else
    //{
    //    m_pContext->interp_params_luma.srcStep   = 2*m_pContext->m_pSingleMB->currYPitch;
    //    m_pContext->interp_params_chroma.srcStep = 2*m_pContext->m_pSingleMB->currUPitch;
    //    m_pContext->interp_params_luma.dstStep   = 2*m_pContext->m_pSingleMB->currYPitch;
    //    m_pContext->interp_params_chroma.dstStep = 2*m_pContext->m_pSingleMB->currUPitch;
    //}

    if (!m_bIsFullMode) // use surface as output in case of full profile
    {
        pY =  m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->Y;
        pU = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->U;
        pV = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->V;

        pitch = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->Pitch;
    }
    else
    {
        pY = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_pY;
        pU = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_pU;
        pV = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_pV;

        pitch = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_iYPitch;
    }


    m_pContext->m_pSingleMB->m_currMBXpos = 0;
    m_pContext->m_pSingleMB->m_currMBYpos = m_MBStartRow;//m_pCurrSlice->MBStartRow;
    m_pContext->m_pSingleMB->slice_currMBYpos = m_MBStartRow;
    m_pContext->m_pSingleMB->widthMB = m_pContext->m_seqLayerHeader->widthMB;
    m_pContext->m_pSingleMB->heightMB = m_pContext->m_seqLayerHeader->heightMB;

    m_pContext->interp_params_luma.roundControl   = m_pContext->m_picLayerHeader->RNDCTRL;
    m_pContext->interp_params_chroma.roundControl = m_pContext->m_picLayerHeader->RNDCTRL;

    for (Ipp32s i=0; i < m_MBRowsToDecode;i++)
    {
        for (Ipp32s j = 0; j < m_pContext->m_seqLayerHeader->widthMB; j++)
        {
            pMbCode = &m_pCUC->MbParam->Mb[(m_MBStartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j];
            m_pContext->m_pCurrMB = &m_pContext->m_MBs[(m_MBStartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j];
            TranslateMfxToUmcPredDec(pMbCode, m_pContext->m_pCurrMB);

            if(m_pContext->m_pCurrMB->mbType != VC1_MB_INTRA)
            {

                SetMBPlanesInternal(pY,
                                    pU,
                                    pV,
                                    pitch,
                                    m_pContext->m_pCurrMB,
                                    m_pContext->m_pSingleMB->m_currMBXpos,
                                    m_pContext->m_pSingleMB->m_currMBYpos);

                UMC_MFX_CHECK_STS(MotionComp_Adv[m_pContext->m_picLayerHeader->FCM * 2+
                                                 m_pContext->m_picLayerHeader->PTYPE-1](m_pContext));
            }

            if (m_pContext->m_picLayerHeader->PTYPE != VC1_B_FRAME)
            {
                mfxI16* pMV = &m_pContext->savedMV_Curr[2*2*(m_pContext->m_pSingleMB->m_currMBXpos + m_pContext->m_seqLayerHeader->widthMB*m_pContext->m_pSingleMB->m_currMBYpos)];
                mfxU8*  pbRefField = &m_pContext->savedMVSamePolarity_Curr[(m_pContext->m_pSingleMB->m_currMBXpos + m_pContext->m_seqLayerHeader->widthMB*m_pContext->m_pSingleMB->m_currMBYpos)];

                PackDirectMVs(m_pContext->m_pCurrMB,
                              pMV,
                              m_pContext->m_picLayerHeader->BottomField,
                              pbRefField,
                              m_pContext->m_picLayerHeader->FCM);
            }
            ++m_pContext->m_pSingleMB->m_currMBXpos;
        }
        m_pContext->m_pSingleMB->m_currMBXpos = 0;
        ++m_pContext->m_pSingleMB->m_currMBYpos;
        ++m_pContext->m_pSingleMB->slice_currMBYpos;
    }
    m_bIsDone = true;
    m_bIsReady = false;
    return MFX_ERR_NONE;
}
VC1DECGetRecon::VC1DECGetRecon()
{
}

mfxStatus VC1DECGetRecon::ProcessJob()
{
    mfxMbCode*      pMbCode;
    VC1MB*          pCurMB;
    //mfxI16  index  =  GetExBufferIndex(m_pCUC, MFX_LABEL_RESCOEFF, MFX_CUC_VC1_RESIDAUALBUF);
    mfxI16  index  =  1;
    ExtVC1Residual* pResidual = (ExtVC1Residual*)m_pCUC->ExtBuffer[index];
    Ipp16s* pResDif = pResidual->pRes;

    Ipp8u*          pY;
    Ipp8u*          pU;
    Ipp8u*          pV;

    Ipp32u          pitch;

    static IppiSize  roiSize_8;
    static IppiSize  roiSize_16;

    roiSize_8.height = 8;
    roiSize_8.width = 8;

    roiSize_16.height = 16;
    roiSize_16.width = 16;



    // set needed params
    m_pContext->m_pSingleMB =  &m_SingleMB;
    m_pContext->m_pSingleMB->m_currMBXpos = 0;
    m_pContext->m_pSingleMB->m_currMBYpos =  m_MBStartRow;
    m_pContext->m_pSingleMB->slice_currMBYpos = m_MBStartRow;


    if (m_bIsFirstInSlice)
        m_pContext->m_pSingleMB->slice_currMBYpos = 0;
    if (!m_bIsFullMode) // use surface as output in case of full profile
    {
        pY =  m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->Y;
        pU = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->U;
        pV = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->V;

        pitch = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->Pitch;
    }
    else
    {
        pY = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_pY;
        pU = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_pU;
        pV = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_pV;

        pitch = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_iYPitch;
    }


    {
        for (Ipp32s i=0; i < m_MBRowsToDecode;i++)
        {
            for (Ipp32s j = 0; j < m_pContext->m_seqLayerHeader->widthMB; j++)
            {
                pMbCode = &m_pCUC->MbParam->Mb[(m_MBStartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j];
                m_pContext->m_pCurrMB = pCurMB = &m_pContext->m_MBs[(m_MBStartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j];
                if (!m_bIsFullMode) // in other case we already filled the VC1Context structure
                  TranslateMfxToUmcPredDec(pMbCode, pCurMB);

                //need for deblocking in I/BI frames
                m_pContext->m_pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(m_pContext->m_pSingleMB);
                m_pContext->m_pBlock = pResDif + GetWordResOffset(pMbCode);
                SetMBPlanesInternal(pY,
                                    pU,
                                    pV,
                                    pitch,
                                    m_pContext->m_pCurrMB,
                                    m_pContext->m_pSingleMB->m_currMBXpos,
                                    m_pContext->m_pSingleMB->m_currMBYpos);

                if (m_pContext->m_picLayerHeader->FCM != VC1_FrameInterlace)
                    GetReconPMbNonInterlace(m_pContext, pCurMB,m_pContext->m_pBlock);
                else
                    GetReconPMbInterlace(m_pContext, pCurMB,m_pContext->m_pBlock);
                //else TBD

                ++m_pContext->m_pSingleMB->m_currMBXpos;
            }
            m_pContext->m_pSingleMB->m_currMBXpos = 0;
            ++m_pContext->m_pSingleMB->m_currMBYpos;
            ++m_pContext->m_pSingleMB->slice_currMBYpos;
        }
    }
    if ((m_pContext->m_picLayerHeader->PTYPE !=VC1_B_FRAME) &&
        (bit_get(m_pCUC->FrameParam->VC1.PicDeblocked,5,1)))
    {
        pMbCode = &m_pCUC->MbParam->Mb[m_MBStartRow * m_pContext->m_seqLayerHeader->widthMB];
        m_pContext->m_pSingleMB->m_currMBXpos = 0;
        m_pContext->m_pSingleMB->m_currMBYpos = m_MBStartRow;
        m_pContext->m_pBlock = pResDif + GetWordResOffset(pMbCode);
        m_pContext->m_pCurrMB = &m_pContext->m_MBs[m_MBStartRow * m_pContext->m_seqLayerHeader->widthMB];
        MBSmooth_tbl[m_pContext->m_seqLayerHeader->PROFILE*4 + m_pContext->m_picLayerHeader->PTYPE](m_pContext,m_MBRowsToDecode);
    }
    m_bIsDone = true;
    m_bIsReady = false;
    // TBD
    return MFX_ERR_NONE;
}
VC1DECPredILDB::VC1DECPredILDB():iPrevDblkStartPos(0)

{
}
mfxStatus VC1DECPredILDB::Init(MfxVC1ThreadUnitParams*  pUnitParams,
                               MfxVC1ThreadUnitParams*  pPrevUnitParams,
                               Ipp32s                   threadOwn,
                               bool                     isReadyToProcess)
{
    mfxStatus sts;
    sts = VC1TaskMfxBase::Init(pUnitParams, pPrevUnitParams, threadOwn, isReadyToProcess);
    MFX_CHECK_STS(sts);
    if (pUnitParams->isFirstInSlice)
        iPrevDblkStartPos = -1;
    else
        iPrevDblkStartPos = pPrevUnitParams->MBStartRow - 1;


    if ((pUnitParams->MBStartRow > 0)&&
        (pPrevUnitParams->isFirstInSlice))
        iPrevDblkStartPos += 1;

    return sts;
}
mfxStatus VC1DECPredILDB::ProcessJob()
{
    mfxMbCode*      pMbCode;
    Deblock* pDeblock = Deblock_tbl_Adv;

    Ipp8u*          pY;
    Ipp8u*          pU;
    Ipp8u*          pV;

    Ipp32u          pitch;


    m_pContext->DeblockInfo.is_last_deblock = 0;
    m_pContext->DeblockInfo.HeightMB = m_MBRowsToDecode;
    m_pContext->DeblockInfo.start_pos = m_MBStartRow;
    Ipp32s MBHeight = 0;
    m_pContext->m_pSingleMB =  &m_SingleMB;
    m_pContext->m_pSingleMB->m_currMBXpos = 0;
    Ipp32u StartRow;

    //Ipp32u StartRow = (0 == m_MBStartRow)?(0):(m_MBStartRow - 1); //m_MBStartRow;

    if (m_bIsFirstInSlice)
    {
        m_pContext->m_pSingleMB->slice_currMBYpos = 0;
        StartRow = m_MBStartRow;
    }
    else
    {
        m_pContext->m_pSingleMB->slice_currMBYpos = m_MBStartRow;
        StartRow = m_MBStartRow - 1;
    }
    m_pContext->m_pSingleMB->m_currMBYpos =  StartRow;

    if (!m_bIsFullMode) // use surface as output in case of full profile
    {
        pY =  m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->Y;
        pU = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->U;
        pV = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->V;

        pitch = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.RecFrameLabel]->Pitch;
    }
    else
    {
        pY =  m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.CurrFrameLabel]->Y;
        pU = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.CurrFrameLabel]->U;
        pV = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.CurrFrameLabel]->V;

        pitch = m_pCUC->FrameSurface->Data[m_pCUC->FrameParam->VC1.CurrFrameLabel]->Pitch;

    }
    //else - we already prepared this data



    if (!m_bIsFullMode) // in other case we already filled the VC1Context structure
    {
        for (Ipp32s i=0; i < (m_MBRowsToDecode);i++) // (m_MBRowsToDecode)
        {
            for (Ipp32s j = 0; j < m_pContext->m_seqLayerHeader->widthMB; j++)
            {
                m_pContext->CurrDC = &m_pContext->DCACParams[(m_MBStartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j];
                pMbCode = &m_pCUC->MbParam->Mb[(StartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j];
                m_pContext->m_pCurrMB = &m_pContext->m_MBs[(StartRow+i)*(m_pContext->m_seqLayerHeader->widthMB)+j];
                //need for deblocking
                TranslateMfxToUmcIQT(pMbCode, m_pContext->m_pCurrMB, m_pContext->CurrDC);
                TranslateMfxToUmcPredDec(pMbCode, m_pContext->m_pCurrMB);

                SetMBPlanesInternal(pY,
                                    pU,
                                    pV,
                                    pitch,
                                    m_pContext->m_pCurrMB,
                                    m_pContext->m_pSingleMB->m_currMBXpos,
                                    m_pContext->m_pSingleMB->m_currMBYpos);

                m_pContext->m_pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(m_pContext->m_pSingleMB);

                ++m_pContext->m_pSingleMB->m_currMBXpos;
                ++m_pContext->m_pCurrMB;
            }

            m_pContext->m_pSingleMB->m_currMBXpos = 0;
            ++m_pContext->m_pSingleMB->m_currMBYpos;
            ++m_pContext->m_pSingleMB->slice_currMBYpos;
        }
    }

    m_pContext->m_pCurrMB = &m_pContext->m_MBs[m_MBStartRow * m_pContext->m_seqLayerHeader->widthMB];
    if (m_pContext->m_seqLayerHeader->PROFILE != VC1_PROFILE_ADVANCED)
        pDeblock = Deblock_tbl;

    // TBD - need to set LeftTopRightPositionFlag
    if (VC1_IS_NO_TOP_MB(m_pContext->m_pCurrMB->LeftTopRightPositionFlag))
    {
        m_pContext->DeblockInfo.HeightMB +=1;
        m_pContext->DeblockInfo.start_pos -=1;
        m_pContext->m_pCurrMB -= m_pContext->m_seqLayerHeader->widthMB;
    }
    // TBD for interlace frames - it is bug of VC-1 standard. The neib slices depends from each other
    m_pContext->iPrevDblkStartPos = m_pContext->DeblockInfo.start_pos - iPrevDblkStartPos;
    MBHeight = m_pContext->DeblockInfo.HeightMB - 1;

    if (m_bIsLastInSlice)
    {
        m_pContext->DeblockInfo.is_last_deblock = 1;
        ++MBHeight;
    }
    pDeblock[m_pContext->m_picLayerHeader->PTYPE*3 + m_pContext->m_picLayerHeader->FCM](m_pContext);

    m_bIsDone = true;
    m_bIsReady = false;
    return MFX_ERR_NONE;
}
mfxStatus VC1MfxTQueueDecFullDec::FormalizeSliceTaskGroup(MfxVC1ThreadUnitParams *pSliceParams)
{
    switch(m_CurFuncID)
    {
    case IQT:
        pSliceParams->isFullMode = false;
        m_IqtQ.FormalizeSliceTaskGroup(pSliceParams);
        break;
    case PredDec:
        pSliceParams->isFullMode = false;
        if (VC1_IS_NOT_PRED(pSliceParams->pContext->m_picLayerHeader->PTYPE))
            m_PredDec.FormalizeSliceTaskGroup(pSliceParams);
        else
            return MFX_ERR_UNSUPPORTED;
        break;
    case GetRecon:
        pSliceParams->isFullMode = false;
        m_GReconQ.FormalizeSliceTaskGroup(pSliceParams,true);
        break;
    case RunILDBL:
        pSliceParams->isFullMode = false;
        if (!pSliceParams->pContext->m_seqLayerHeader->LOOPFILTER)
            m_DeblcQ.FormalizeSliceTaskGroup(pSliceParams,true);
        else
            return MFX_ERR_UNSUPPORTED;
        break;
    case Full:
        pSliceParams->isFullMode = true;
        m_IqtQ.FormalizeSliceTaskGroup(pSliceParams);
        if (!VC1_IS_NOT_PRED(pSliceParams->pContext->m_picLayerHeader->PTYPE))
        {
            m_PredDec.SetQueueAsReady();
            m_PredDec.FormalizeSliceTaskGroup(pSliceParams);
        }
        else
            m_PredDec.SetQueueAsPerformed();
        //should be opened only after the first couple
        m_GReconQ.FormalizeSliceTaskGroup(pSliceParams,false);
        if (pSliceParams->pContext->m_seqLayerHeader->LOOPFILTER)
        {
            m_PredDec.SetQueueAsReady();
            m_DeblcQ.FormalizeSliceTaskGroup(pSliceParams,false);
        }
        else
            m_DeblcQ.SetQueueAsPerformed();
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;

}

VC1TaskMfxBase*  VC1MfxTQueueDecFullDec::GetNextStaticTask(Ipp32s threadID)
{
    VC1TaskMfxBase* pTaskBase;
    // need to modify for correct threading support
    switch(m_CurFuncID)
    {
    case IQT:
        return m_IqtQ.GetNextStaticTask(threadID);
    case PredDec:
        return m_PredDec.GetNextStaticTask(threadID);
    case GetRecon:
        return m_GReconQ.GetNextStaticTask(threadID);
    case RunILDBL:
        return m_DeblcQ.GetNextStaticTask(threadID);
        break;
    case Full:
        pTaskBase = m_IqtQ.GetNextStaticTask(threadID);
        if (pTaskBase)
            return pTaskBase;
        pTaskBase = m_PredDec.GetNextStaticTask(threadID);
        if (pTaskBase)
            return pTaskBase;
        pTaskBase = m_GReconQ.GetNextStaticTask(threadID); //TBD only in debug needs
        if (pTaskBase)
            return pTaskBase;
        pTaskBase = m_DeblcQ.GetNextStaticTask(threadID);
        if (pTaskBase)
            return pTaskBase;
    default:
        return 0;
    }
}

bool    VC1MfxTQueueDecFullDec::IsFuncComplte()
{
    switch(m_CurFuncID)
    {
    case IQT:
        return m_IqtQ.IsFuncComplte();
    case PredDec:
        return m_PredDec.IsFuncComplte();
    case GetRecon:
        return m_GReconQ.IsFuncComplte();
    case RunILDBL:
        return m_DeblcQ.IsFuncComplte();
        break;
    case Full:
        {
            if (!m_IqtQ.IsFuncComplte())
                return false;
            if (!m_PredDec.IsFuncComplte())
                return false;
            if (!m_GReconQ.IsFuncComplte())
                return false;
            if (!m_DeblcQ.IsFuncComplte())
                return false;
            return true;
        }
     default:
        return false;
    }

}

#endif
