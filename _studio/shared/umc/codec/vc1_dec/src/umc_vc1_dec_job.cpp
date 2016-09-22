/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2016 Intel Corporation. All Rights Reserved.
//
//
//          VC-1 (VC1) decoder, Jobs for thread model
//
*/
#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_dec_job.h"
//#include "umc_vc1_dec_seq.h"
#include "umc_vc1_common_acintra.h"
#include "umc_vc1_common_acinter.h"
#include "umc_vc1_dec_frame_descr.h"

#include "umc_vc1_dec_time_statistics.h"

#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_common_blk_order_tbl.h"
#include "umc_vc1_common.h"
#include "umc_automatic_mutex.h"
#include "mfx_trace.h"

#ifndef UMC_RESTRICTED_CODE_VA
#if defined (UMC_VA_DXVA)
#include "umc_vc1_dec_frame_descr_va.h"
#endif
#endif


using namespace UMC;
using namespace UMC::VC1Common;
using namespace UMC::VC1Exceptions;


MBLayerDecode MBLayerDecode_tbl_Adv[12] = {
    //I pic
    (MBLayer_ProgressiveIpicture_Adv),
    (MBLayer_Frame_InterlaceIpicture),
    (MBLayer_Field_InterlaceIpicture),

    //P pic
    (MBLayer_ProgressivePpicture_Adv),
    (MBLayer_Frame_InterlacedPpicture),
    (MBLayer_Field_InterlacedPpicture),

    //B pic
    (MBLayer_ProgressiveBpicture_Adv),
    (MBLayer_Frame_InterlacedBpicture),
    (MBLayer_Field_InterlacedBpicture),

    //BI pic
    (MBLayer_ProgressiveIpicture_Adv),
    (MBLayer_Frame_InterlaceIpicture),
    (MBLayer_Field_InterlaceIpicture)
};

MBLayerDecode MBLayerDecode_tbl[12] = {
    //I pic
    (MBLayer_ProgressiveIpicture),
    (NULL),
    (NULL),
    //P pic
    (MBLayer_ProgressivePpicture),
    (NULL),
    (NULL),
    //B pic
    (MBLayer_ProgressiveBpicture),
    (NULL),
    (NULL),
     //BI pic
    (MBLayer_ProgressiveIpicture),
    (NULL),
    (NULL),

};

Deblock Deblock_tbl_Adv[12] = {
    //I pic
    (Deblocking_ProgressiveIpicture_Adv),
    (Deblocking_InterlaceFrameIpicture_Adv),
    (Deblocking_ProgressiveIpicture_Adv),

    //P pic
    (Deblocking_ProgressivePpicture_Adv),
    (Deblocking_InterlaceFramePpicture_Adv),
    (Deblocking_ProgressivePpicture_Adv),

    //B pic
    (Deblocking_ProgressiveIpicture_Adv),
    (Deblocking_InterlaceFramePpicture_Adv),
    (Deblocking_InterlaceFieldBpicture_Adv),

    //BI pic
    (Deblocking_ProgressiveIpicture_Adv),
    (Deblocking_InterlaceFramePpicture_Adv),
    (Deblocking_ProgressiveIpicture_Adv)
};

Deblock Deblock_tbl[12] = {
    //I pic
    (Deblocking_ProgressiveIpicture),
    (NULL),
    (NULL),

    //P pic
    (Deblocking_ProgressivePpicture),
    (NULL),
    (NULL),

    //B pic
    (Deblocking_ProgressiveIpicture),
    (NULL),
    (NULL),

    //BI pic
    (Deblocking_ProgressiveIpicture),
    (NULL),
    (NULL),
};

ProcessDiff ProcessDiff_Adv[2] = {
    (VC1ProcessDiffIntra),
    (VC1ProcessDiffInter)
};

MotionComp MotionComp_Adv[6] = {
    (PredictBlock_P),
    (PredictBlock_B),

    (PredictBlock_InterlacePPicture),
    (PredictBlock_InterlaceBPicture),

    (PredictBlock_InterlaceFieldPPicture),
    (PredictBlock_InterlaceFieldBPicture)
};

typedef void (*SaveMV)(VC1Context* pContext);
//SaveMV SaveMV_tbl[3] = {
//    (save_MV),
//    (save_MV_InterlaceFrame),
//    (save_MV_InterlaceField)
//};
typedef VC1Status (*B_MB_DECODE)(VC1Context* pContext);
static B_MB_DECODE B_MB_Dispatch_table[] = {
        (MBLayer_ProgressiveBpicture_NONDIRECT_Prediction),
        (MBLayer_ProgressiveBpicture_DIRECT_Prediction),
        (MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_Prediction),
        (MBLayer_ProgressiveBpicture_SKIP_DIRECT_Prediction)
};
static B_MB_DECODE B_MB_Dispatch_table_AdvProgr[] = {
        (MBLayer_ProgressiveBpicture_NONDIRECT_AdvPrediction),
        (MBLayer_ProgressiveBpicture_DIRECT_AdvPrediction),
        (MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_AdvPrediction),
        (MBLayer_ProgressiveBpicture_SKIP_DIRECT_AdvPrediction)
};
static B_MB_DECODE B_MB_Dispatch_table_InterlaceFrame[] = {
        (MBLayer_InterlaceFrameBpicture_NONDIRECT_Prediction),
        (MBLayer_InterlaceFrameBpicture_DIRECT_Prediction),
        (MBLayer_InterlaceFrameBpicture_SKIP_NONDIRECT_Prediction),
        (MBLayer_InterlaceFrameBpicture_SKIP_DIRECT_Prediction)
};
static B_MB_DECODE B_MB_Dispatch_table_InterlaceFields[] = {
        (MBLayer_InterlaceFieldBpicture_NONDIRECT_Predicition),
        (MBLayer_InterlaceFieldBpicture_DIRECT_Prediction)
};

typedef IppStatus (*VC1DeqIntra)(Ipp16s* pSrcDst, Ipp32s srcDstStep,
                                 Ipp32s doubleQuant, IppiSize* pDstSizeNZ);

VC1DeqIntra VC1DeqIntra_tbl[2] = {
    (VC1DeqIntra) (_own_ippiQuantInvIntraUniform_VC1_16s_C1IR),
    (VC1DeqIntra) (_own_ippiQuantInvIntraNonuniform_VC1_16s_C1IR)
 };


typedef IppStatus (*VC1DeqInter)(Ipp16s* pSrcDst, Ipp32s srcDstStep,
                                 Ipp32s doubleQuant, IppiSize roiSize,
                                 IppiSize* pDstSizeNZ);

VC1DeqInter VC1DeqInter_tbl[2] = {
    (VC1DeqInter) (_own_ippiQuantInvInterUniform_VC1_16s_C1IR),
    (VC1DeqInter) (_own_ippiQuantInvInterNonuniform_VC1_16s_C1IR),
};


//typedef void (*MBSmooth)(VC1Context* pContext, Ipp32s Height);

MBSmooth MBSmooth_tbl[16] = {
    //simple
        (Smoothing_I),
        (Smoothing_P),
        (NULL),
        (Smoothing_I),
        //main
        (Smoothing_I),
        (Smoothing_P),
        (NULL),
        (Smoothing_I),
        //reserved
        (NULL),
        (NULL),
        (NULL),
        (NULL),
        //Advanced
        (Smoothing_I_Adv),
        (Smoothing_P_Adv),
        (NULL),
        (Smoothing_I_Adv)

};

VC1Status VC1TaskProcessorUMC::VC1MVCalculation(VC1Context* pContext, VC1Task* pTask)
{
    B_MB_DECODE* currMVtable = 0;//B_MB_Dispatch_table;

    if (pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
    {
        switch (pContext->m_picLayerHeader->FCM)
        {
        case VC1_Progressive:
            currMVtable = B_MB_Dispatch_table_AdvProgr;
            break;
        case VC1_FrameInterlace:
            currMVtable = B_MB_Dispatch_table_InterlaceFrame;
            break;
        case VC1_FieldInterlace:
            currMVtable = B_MB_Dispatch_table_InterlaceFields;
            break;
        default:
            break;
        }
    }
    else
    {
        currMVtable = B_MB_Dispatch_table;
    }


    for (Ipp32s i=0; i < pTask->m_pSlice->MBRowsToDecode;i++)
    {
        for (Ipp32s j = 0; j <pContext->m_seqLayerHeader.widthMB; j++)
        {
            STATISTICS_START_TIME(m_timeStatistics->motion_vector_decoding_StartTime);
            if (pContext->m_pCurrMB->mbType != VC1_MB_INTRA)
                currMVtable[pContext->m_pCurrMB->SkipAndDirectFlag](pContext);
            STATISTICS_END_TIME(m_timeStatistics->motion_vector_decoding_StartTime,
                                m_timeStatistics->motion_vector_decoding_EndTime,
                                m_timeStatistics->motion_vector_decoding_TotalTime);

            ++pContext->m_pCurrMB;
            ++pContext->m_pSingleMB->m_currMBXpos;
         }
        pContext->m_pSingleMB->m_currMBXpos = 0;
        ++pContext->m_pSingleMB->m_currMBYpos;
        pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
    }
    return VC1_OK;
}
VC1Status VC1TaskProcessorUMC::VC1Decoding (VC1Context* pContext, VC1Task* pTask)
{
    volatile MBLayerDecode* pCurrMBTable = MBLayerDecode_tbl_Adv;
    if (pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
        pCurrMBTable = MBLayerDecode_tbl;




    if (pTask->m_pSlice->is_NewInSlice)
    {
        pContext->m_pSingleMB->slice_currMBYpos = 0;
        pContext->m_pSingleMB->EscInfo.levelSize = 0;
        pContext->m_pSingleMB->EscInfo.runSize = 0;


        if (pContext->m_picLayerHeader->m_PQuant_mode>0
            || pContext->m_picLayerHeader->PQUANT<=7)
        {
            pContext->m_pSingleMB->EscInfo.bEscapeMode3Tbl = VC1_ESCAPEMODE3_Conservative;
        }
        else
        {
            pContext->m_pSingleMB->EscInfo.bEscapeMode3Tbl = VC1_ESCAPEMODE3_Efficient;
        }
    }

    for (Ipp32s i=0; i < pTask->m_pSlice->MBRowsToDecode;i++)
    {
        for (Ipp32s j = 0; j < pContext->m_pSingleMB->widthMB; j++)
        {
            try // check decoding on MB level
            {
                pCurrMBTable[pContext->m_picLayerHeader->PTYPE*3 + pContext->m_picLayerHeader->FCM](pContext);
            }
            catch (vc1_exception ex)
            {
                exception_type e_type = ex.get_exception_type();
                if (e_type != vld)
                    throw ex;
                else
                {
                    m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].corrupted = ERROR_FRAME_MAJOR;
                    robust_profile profile = vc1_except_profiler::GetEnvDescript().m_Profile;
                    switch (profile)
                    {
                    case fast_err_detect:
                        // we should signalize about error only
                        ex.set_exception_type(internal_pipeline_error);
                        throw ex;
                        break;
                    case max_decoding:
                        //continue decoding cycle
                        break;
                    case smart_recon:
                        if (vc1_except_profiler::GetEnvDescript().m_SmartLevel != FrameLevel)
                        {
                            ProcessSmartException(vc1_except_profiler::GetEnvDescript().m_SmartLevel,
                                                  pContext,
                                                  pTask,
                                                  pContext->m_pCurrMB);
                            // we already decode it
                            //if (mbGroupLevel == vc1_except_profiler::GetEnvDescript().m_SmartLevel)
                            //{
                            //    i = pTask->m_pSlice->MBRowsToDecode;
                            //    j = pContext->m_pSingleMB->widthMB;
                            //}
                        }
                        else
                            throw ex;
                        break;
                    default:
                        break;
                     }
                }
            }
            // unexpected problems. The decoder works incorrect
            catch(...)
            {
            }
            ++pContext->m_pSingleMB->m_currMBXpos;

            pContext->m_pBlock += 8*8*6;

            ++pContext->m_pCurrMB;
            ++pContext->CurrDC;
        }
        pContext->m_pSingleMB->m_currMBXpos = 0;
        ++pContext->m_pSingleMB->m_currMBYpos;
        ++pContext->m_pSingleMB->slice_currMBYpos;
        
        pContext->CurrDC += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
        pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
        pContext->m_pBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
    }
    return VC1_OK;
}

VC1Status VC1TaskProcessorUMC::VC1ProcessDiff (VC1Context* pContext, VC1Task* pTask)
{
    IppiSize  roiSize;
    Ipp32u offset_table[] = {0,8,128,136};
    Ipp32u plane_offset = 0;
    Ipp32u IntraFlag;

    if (m_pStore->IsNeedSimlifyReconstruct(pTask->m_pSlice->m_picLayerHeader->PTYPE))
        AccelerReconstruct();

    if(pContext->m_picLayerHeader->PTYPE == VC1_P_FRAME)
    {
        for (Ipp32s i=0; i < pTask->m_pSlice->MBRowsToDecode;i++)
        {
            for (Ipp32s j = 0; j < pContext->m_pSingleMB->widthMB; j++)
            {
                // We dont need to process Skip MB
                if (!(pContext->m_pCurrMB->SkipAndDirectFlag & 2))
                {
                    IntraFlag = pContext->m_pCurrMB->IntraFlag;
                    for (Ipp32s blk_num = 0; blk_num<6 ;blk_num++)
                    {
                        pReconstructTbl[IntraFlag & 1](pContext,blk_num);
                        IntraFlag >>= 1;
                    }

                if (pContext->m_pCurrMB->mbType == VC1_MB_INTRA)
                {
                    //write to plane
                    STATISTICS_START_TIME(m_timeStatistics->write_plane_StartTime);

                    if (pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
                        write_Intraluma_to_interlace_frame_Adv(pContext->m_pCurrMB, pContext->m_pBlock);
                    else
                    {

                        roiSize.height = VC1_PIXEL_IN_LUMA;
                        roiSize.width = VC1_PIXEL_IN_LUMA;

                        ippiConvert_16s8u_C1R(pContext->m_pBlock,
                            VC1_PIXEL_IN_LUMA*2,
                            pContext->m_pCurrMB->currYPlane,
                            pContext->m_pCurrMB->currYPitch,
                            roiSize);
                    }

                    roiSize.height = VC1_PIXEL_IN_CHROMA;
                    roiSize.width = VC1_PIXEL_IN_CHROMA;

                    ippiConvert_16s8u_C1R(pContext->m_pBlock+64*4,
                        VC1_PIXEL_IN_CHROMA*2,
                        pContext->m_pCurrMB->currUPlane,
                        pContext->m_pCurrMB->currUPitch,
                        roiSize);

                    ippiConvert_16s8u_C1R(pContext->m_pBlock+64*5,
                        VC1_PIXEL_IN_CHROMA*2,
                        pContext->m_pCurrMB->currVPlane,
                        pContext->m_pCurrMB->currVPitch,
                        roiSize);

                    STATISTICS_END_TIME(m_timeStatistics->write_plane_StartTime,
                        m_timeStatistics->write_plane_EndTime,
                        m_timeStatistics->write_plane_TotalTime);
                }
                else
                {
                    roiSize.height = 8;
                    roiSize.width = 8;

                    //write to plane
                    STATISTICS_START_TIME(m_timeStatistics->write_plane_StartTime);

                    IntraFlag = pContext->m_pCurrMB->IntraFlag;
                    for (Ipp32s blk_num = 0; blk_num<4 ;blk_num++)
                    {
                        if (IntraFlag & 1) // from assignpattern
                        {
                            plane_offset = (blk_num&1)*8 + (blk_num&2)*4*pContext->m_pCurrMB->currYPitch;
                            ippiConvert_16s8u_C1R(&pContext->m_pBlock[offset_table[blk_num]],
                                VC1_PIXEL_IN_LUMA*2,
                                pContext->m_pCurrMB->currYPlane + plane_offset,
                                pContext->m_pCurrMB->currYPitch,
                                roiSize);
                        }
                        IntraFlag >>= 1;
                    }

                    if (IntraFlag & 1) // from assignpattern
                        ippiConvert_16s8u_C1R(pContext->m_pBlock+64*VC1_NUM_OF_LUMA,
                        VC1_PIXEL_IN_CHROMA*2,
                        pContext->m_pCurrMB->currUPlane,
                        pContext->m_pCurrMB->currUPitch,
                        roiSize);

                    if (IntraFlag & 2) // from assignpattern
                        ippiConvert_16s8u_C1R(pContext->m_pBlock+64*5,
                        VC1_PIXEL_IN_CHROMA*2,
                        pContext->m_pCurrMB->currVPlane,
                            pContext->m_pCurrMB->currVPitch,
                            roiSize);

                        STATISTICS_END_TIME(m_timeStatistics->write_plane_StartTime,
                            m_timeStatistics->write_plane_EndTime,
                            m_timeStatistics->write_plane_TotalTime);
                    }
                }
                ++pContext->m_pSingleMB->m_currMBXpos;
                pContext->m_pBlock += 8*8*6;
                ++pContext->m_pCurrMB;
                ++pContext->CurrDC;
            }

            pContext->m_pSingleMB->m_currMBXpos = 0;
            ++pContext->m_pSingleMB->m_currMBYpos;
            ++pContext->m_pSingleMB->slice_currMBYpos;
            pContext->CurrDC += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
            pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
            pContext->m_pBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;

        }
    }
    else if (pContext->m_picLayerHeader->PTYPE == VC1_B_FRAME)
    {
        for (Ipp32s i=0; i < pTask->m_pSlice->MBRowsToDecode;i++)
        {
            for (Ipp32s j = 0; j < pContext->m_pSingleMB->widthMB; j++)
            {
                // We dont need to process Skip MB
                if (!(pContext->m_pCurrMB->SkipAndDirectFlag & 2))
                {
                    if (pContext->m_pCurrMB->mbType == VC1_MB_INTRA)
                    {
                        for (Ipp32s blk_num = 0; blk_num<6 ;blk_num++)
                            pReconstructTbl[1](pContext,blk_num);

                    //write to plane
                    STATISTICS_START_TIME(m_timeStatistics->write_plane_StartTime);

                    if (pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
                        write_Intraluma_to_interlace_frame_Adv(pContext->m_pCurrMB, pContext->m_pBlock);
                    else
                    {

                        roiSize.height = VC1_PIXEL_IN_LUMA;
                        roiSize.width = VC1_PIXEL_IN_LUMA;

                        ippiConvert_16s8u_C1R(pContext->m_pBlock,
                            VC1_PIXEL_IN_LUMA*2,
                            pContext->m_pCurrMB->currYPlane,
                            pContext->m_pCurrMB->currYPitch,
                            roiSize);
                    }

                    roiSize.height = VC1_PIXEL_IN_CHROMA;
                    roiSize.width = VC1_PIXEL_IN_CHROMA;

                    ippiConvert_16s8u_C1R(pContext->m_pBlock+64*4,
                        VC1_PIXEL_IN_CHROMA*2,
                        pContext->m_pCurrMB->currUPlane,
                        pContext->m_pCurrMB->currUPitch,
                        roiSize);

                    ippiConvert_16s8u_C1R(pContext->m_pBlock+64*5,
                        VC1_PIXEL_IN_CHROMA*2,
                        pContext->m_pCurrMB->currVPlane,
                        pContext->m_pCurrMB->currVPitch,
                        roiSize);

                        STATISTICS_END_TIME(m_timeStatistics->write_plane_StartTime,
                            m_timeStatistics->write_plane_EndTime,
                            m_timeStatistics->write_plane_TotalTime);
                    }
                    else
                    {
                        for (Ipp32s blk_num = 0; blk_num<6 ;blk_num++)
                            pReconstructTbl[0](pContext,blk_num);
                    }

                }
                ++pContext->m_pSingleMB->m_currMBXpos;
                pContext->m_pBlock += 8*8*6;
                ++pContext->m_pCurrMB;
                ++pContext->CurrDC;
            }

            pContext->m_pSingleMB->m_currMBXpos = 0;
            ++pContext->m_pSingleMB->m_currMBYpos;
            ++pContext->m_pSingleMB->slice_currMBYpos;

            pContext->CurrDC += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
            pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
            pContext->m_pBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
        }

    }
    else // I, BI frames
    {
        for (Ipp32s i=0; i < pTask->m_pSlice->MBRowsToDecode;i++)
        {
            for (Ipp32s j = 0; j < pContext->m_pSingleMB->widthMB; j++)
            {

                for (Ipp32s blk_num = 0; blk_num<6 ;blk_num++)
                    pReconstructTbl[1](pContext,blk_num);

                //write to plane
                STATISTICS_START_TIME(m_timeStatistics->write_plane_StartTime);

                if (pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
                    write_Intraluma_to_interlace_frame_Adv(pContext->m_pCurrMB, pContext->m_pBlock);
                else
                {

                    roiSize.height = VC1_PIXEL_IN_LUMA;
                    roiSize.width = VC1_PIXEL_IN_LUMA;

                    ippiConvert_16s8u_C1R(pContext->m_pBlock,
                        VC1_PIXEL_IN_LUMA*2,
                        pContext->m_pCurrMB->currYPlane,
                        pContext->m_pCurrMB->currYPitch,
                        roiSize);
                }

                roiSize.height = VC1_PIXEL_IN_CHROMA;
                roiSize.width = VC1_PIXEL_IN_CHROMA;

                ippiConvert_16s8u_C1R(pContext->m_pBlock+64*4,
                    VC1_PIXEL_IN_CHROMA*2,
                    pContext->m_pCurrMB->currUPlane,
                    pContext->m_pCurrMB->currUPitch,
                    roiSize);

                ippiConvert_16s8u_C1R(pContext->m_pBlock+64*5,
                    VC1_PIXEL_IN_CHROMA*2,
                    pContext->m_pCurrMB->currVPlane,
                    pContext->m_pCurrMB->currVPitch,
                    roiSize);

                STATISTICS_END_TIME(m_timeStatistics->write_plane_StartTime,
                    m_timeStatistics->write_plane_EndTime,
                    m_timeStatistics->write_plane_TotalTime);

                ++pContext->m_pSingleMB->m_currMBXpos;
                pContext->m_pBlock += 8*8*6;
                ++pContext->m_pCurrMB;
                ++pContext->CurrDC;
            }

            pContext->m_pSingleMB->m_currMBXpos = 0;
            ++pContext->m_pSingleMB->m_currMBYpos;
            ++pContext->m_pSingleMB->slice_currMBYpos;

            pContext->CurrDC += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
            pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
            pContext->m_pBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
        }

    }
    return VC1_OK;
}


VC1Status VC1TaskProcessorUMC::VC1MotionCompensation(VC1Context* pContext,VC1Task* pTask)
{
    STATISTICS_START_TIME(m_timeStatistics->interpolation_StartTime);
    {
    pContext->interp_params_luma.srcStep   = pContext->m_pSingleMB->currYPitch;
    pContext->interp_params_chroma.srcStep = pContext->m_pSingleMB->currUPitch;
    pContext->interp_params_luma.dstStep   = pContext->m_pSingleMB->currYPitch;
    pContext->interp_params_chroma.dstStep = pContext->m_pSingleMB->currUPitch;
    }

    pContext->interp_params_luma.roundControl   = pContext->m_picLayerHeader->RNDCTRL;
    pContext->interp_params_chroma.roundControl = pContext->m_picLayerHeader->RNDCTRL;

    for (Ipp32s i=0; i < pTask->m_pSlice->MBRowsToDecode;i++)
    {
        for (Ipp32s j = 0; j < pContext->m_pSingleMB->widthMB; j++)
        {
            if(pContext->m_pCurrMB->mbType != VC1_MB_INTRA)
            {
                MotionComp_Adv[pContext->m_picLayerHeader->FCM*2+pContext->m_picLayerHeader->PTYPE-1](pContext);
            }

            if (pContext->m_picLayerHeader->PTYPE != VC1_B_FRAME)
            {
                Ipp16s* pSavedMV = &pContext->savedMV[(pContext->m_pSingleMB->m_currMBXpos +
                    pContext->m_pSingleMB->m_currMBYpos*pContext->m_seqLayerHeader.MaxWidthMB)*2*2];

                Ipp8u* pRefField = &pContext->savedMVSamePolarity[(pContext->m_pSingleMB->m_currMBXpos +
                    pContext->m_pSingleMB->m_currMBYpos*pContext->m_seqLayerHeader.MaxWidthMB)];

                PackDirectMVs(pContext->m_pCurrMB,
                    pSavedMV,
                    pContext->m_picLayerHeader->BottomField,
                    pRefField,
                    pContext->m_picLayerHeader->FCM);

            }
            ++pContext->m_pSingleMB->m_currMBXpos;
            ++pContext->m_pCurrMB;
        }
        pContext->m_pSingleMB->m_currMBXpos = 0;
        ++pContext->m_pSingleMB->m_currMBYpos;
        ++pContext->m_pSingleMB->slice_currMBYpos;

        pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
    }

    STATISTICS_END_TIME(m_timeStatistics->interpolation_StartTime,
        m_timeStatistics->interpolation_EndTime,
        m_timeStatistics->interpolation_TotalTime);
    return VC1_OK;
}

VC1Status VC1TaskProcessorUMC::VC1PrepPlane(VC1Context* pContext,VC1Task* pTask)
{
    static IppiSize  roiSize_8;
    static IppiSize  roiSize_16;

    roiSize_8.height = 8;
    roiSize_8.width = 8;

    roiSize_16.height = 16;
    roiSize_16.width = 16;

    //VC1MB* pStartMB = pContext->m_pCurrMB;

    if (pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
    {
        if(pContext->m_picLayerHeader->PTYPE == VC1_P_FRAME)
            CompensateInterlacePFrame(pContext, pTask);
        else if (pContext->m_picLayerHeader->PTYPE == VC1_B_FRAME)
            CompensateInterlaceBFrame(pContext, pTask);
     }
    else
    {
    STATISTICS_START_TIME(m_timeStatistics->mc_StartTime);
    if(pContext->m_picLayerHeader->PTYPE == VC1_P_FRAME)
    {
        for (Ipp32s i = 0; i < pTask->m_pSlice->MBRowsToDecode;i++)
        {
            for (Ipp32s j = 0; j < pContext->m_pSingleMB->widthMB; j++)
            {
                if(!pContext->m_pCurrMB->IntraFlag)
                {
                    // SkipMB
                        if (pContext->m_pCurrMB->SkipAndDirectFlag & 2)
                            ippiCopy_8u_C1R(pContext->m_pCurrMB->pInterpolLumaSrc[0],
                                            pContext->m_pCurrMB->InterpolsrcLumaStep[0],
                                            pContext->m_pCurrMB->currYPlane,
                                            pContext->m_pCurrMB->currYPitch,
                                            roiSize_16);
                        else
                            ippiMC16x16_8u_C1(pContext->m_pCurrMB->pInterpolLumaSrc[0],
                                              pContext->m_pCurrMB->InterpolsrcLumaStep[0],
                                              pContext->m_pBlock,
                                              VC1_PIXEL_IN_LUMA*2,
                                              pContext->m_pCurrMB->currYPlane,
                                              pContext->m_pCurrMB->currYPitch, 0, 0);


                     if (pContext->m_pCurrMB->SkipAndDirectFlag & 2)
                     {
                         ippiCopy_8u_C1R(pContext->m_pCurrMB->pInterpolChromaUSrc[0],
                                         pContext->m_pCurrMB->InterpolsrcChromaUStep[0],
                                         pContext->m_pCurrMB->currUPlane,
                                         pContext->m_pCurrMB->currUPitch,
                                         roiSize_8);
                         ippiCopy_8u_C1R(pContext->m_pCurrMB->pInterpolChromaVSrc[0],
                                         pContext->m_pCurrMB->InterpolsrcChromaVStep[0],
                                         pContext->m_pCurrMB->currVPlane,
                                         pContext->m_pCurrMB->currVPitch,
                                         roiSize_8);
                     }
                     else
                     {
                         ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolChromaUSrc[0],
                                         pContext->m_pCurrMB->InterpolsrcChromaUStep[0],
                                        &pContext->m_pBlock[4*64],
                                         VC1_PIXEL_IN_CHROMA*2,
                                         pContext->m_pCurrMB->currUPlane,
                                         pContext->m_pCurrMB->currUPitch, 0, 0);

                         ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolChromaVSrc[0],
                                         pContext->m_pCurrMB->InterpolsrcChromaVStep[0],
                                         &pContext->m_pBlock[5*64],
                                         VC1_PIXEL_IN_CHROMA*2,
                                         pContext->m_pCurrMB->currVPlane,
                                         pContext->m_pCurrMB->currVPitch, 0, 0);
                     }
                }
                else if(pContext->m_pCurrMB->mbType != VC1_MB_INTRA)
                {
                    static const Ipp32u offset_table[] = {0,8,128,136};
                    Ipp32u plane_offset = 0;

                        for (Ipp32s blk_num = 0; blk_num<4 ;blk_num++)
                        {
                            if (!(pContext->m_pCurrMB->IntraFlag & (1<<blk_num))) // from assignpattern
                            {
                                plane_offset = (blk_num&1)*8 + (blk_num&2)*4*pContext->m_pCurrMB->InterpolsrcLumaStep[0];
                                ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolLumaSrc[0] + plane_offset,
                                                pContext->m_pCurrMB->InterpolsrcLumaStep[0],
                                               &pContext->m_pBlock[offset_table[blk_num]],
                                                VC1_PIXEL_IN_LUMA*2,
                                                pContext->m_pCurrMB->currYPlane + plane_offset,
                                                pContext->m_pCurrMB->currYPitch, 0, 0);
                            }
                        }

                    if (!(pContext->m_pCurrMB->IntraFlag & (1<<4)))
                        ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolChromaUSrc[0],
                                        pContext->m_pCurrMB->InterpolsrcChromaUStep[0],
                                        &pContext->m_pBlock[4*64],
                                        VC1_PIXEL_IN_CHROMA*2,
                                        pContext->m_pCurrMB->currUPlane,
                                        pContext->m_pCurrMB->currUPitch, 0, 0);

                    if (!(pContext->m_pCurrMB->IntraFlag & (1<<5)))
                        ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolChromaVSrc[0],
                                        pContext->m_pCurrMB->InterpolsrcChromaVStep[0],
                                        &pContext->m_pBlock[5*64],
                                        VC1_PIXEL_IN_CHROMA*2,
                                        pContext->m_pCurrMB->currVPlane,
                                        pContext->m_pCurrMB->currVPitch, 0, 0);
                }
    #ifdef VC1_DEBUG_ON
                if (VC1_DEBUG&VC1_RESPEL)
                    VM_Debug::GetInstance(VC1DebugRoutine)._print_blocks(pContext);
    #endif

                ++pContext->m_pCurrMB;
                pContext->m_pBlock += 8*8*6;
                pContext->m_pSingleMB->m_currMBXpos++;
            }

            pContext->m_pSingleMB->m_currMBXpos = 0;
            ++pContext->m_pSingleMB->m_currMBYpos;
            ++pContext->m_pSingleMB->slice_currMBYpos;

            pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
            pContext->m_pBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
        }

    STATISTICS_END_TIME(m_timeStatistics->mc_StartTime,
        m_timeStatistics->mc_EndTime,
        m_timeStatistics->mc_TotalTime);
    }

    else //B Frames
        if(pContext->m_picLayerHeader->PTYPE == VC1_B_FRAME)
        {
        for (Ipp32s i=0; i < pTask->m_pSlice->MBRowsToDecode;i++)
        {
            for (Ipp32s j = 0; j < pContext->m_pSingleMB->widthMB; j++)
            {
                if(!pContext->m_pCurrMB->IntraFlag)
                {
                    if(pContext->m_pCurrMB->pInterpolLumaSrc[0] &&
                        pContext->m_pCurrMB->pInterpolLumaSrc[1])
                    {

                        // Skip MB
                        if (pContext->m_pCurrMB->SkipAndDirectFlag & 2)
                            ippiAverage16x16_8u_C1R(pContext->m_pCurrMB->pInterpolLumaSrc[0],
                            pContext->m_pCurrMB->InterpolsrcLumaStep[0],
                            pContext->m_pCurrMB->pInterpolLumaSrc[1],
                            pContext->m_pCurrMB->InterpolsrcLumaStep[1],
                            pContext->m_pCurrMB->currYPlane,
                            pContext->m_pCurrMB->currYPitch);
                        else
                            ippiMC16x16B_8u_C1(pContext->m_pCurrMB->pInterpolLumaSrc[0],
                            pContext->m_pCurrMB->InterpolsrcLumaStep[0], 0,
                            pContext->m_pCurrMB->pInterpolLumaSrc[1],
                            pContext->m_pCurrMB->InterpolsrcLumaStep[1], 0,
                            pContext->m_pBlock,  VC1_PIXEL_IN_LUMA*2,
                            pContext->m_pCurrMB->currYPlane,
                            pContext->m_pCurrMB->currYPitch, 0);

                    }
                    else
                    {
                        Ipp32u back = (pContext->m_pCurrMB->pInterpolLumaSrc[0])?0:1;
                        //SkipMB
                        if (pContext->m_pCurrMB->SkipAndDirectFlag & 2)
                            ippiCopy_8u_C1R(pContext->m_pCurrMB->pInterpolLumaSrc[back],
                            pContext->m_pCurrMB->InterpolsrcLumaStep[back],
                            pContext->m_pCurrMB->currYPlane,
                            pContext->m_pCurrMB->currYPitch,
                            roiSize_16);
                        else
                            ippiMC16x16_8u_C1(pContext->m_pCurrMB->pInterpolLumaSrc[back],
                            pContext->m_pCurrMB->InterpolsrcLumaStep[back],
                            pContext->m_pBlock,
                            VC1_PIXEL_IN_LUMA*2,
                            pContext->m_pCurrMB->currYPlane,
                            pContext->m_pCurrMB->currYPitch, 0, 0);
                    }

                    if(pContext->m_pCurrMB->pInterpolChromaUSrc[0] &&
                        pContext->m_pCurrMB->pInterpolChromaUSrc[1])
                    {

                        if (pContext->m_pCurrMB->SkipAndDirectFlag & 2)
                        {
                            ippiAverage8x8_8u_C1R(pContext->m_pCurrMB->pInterpolChromaUSrc[0],
                                pContext->m_pCurrMB->InterpolsrcChromaUStep[0],
                                pContext->m_pCurrMB->pInterpolChromaUSrc[1],
                                pContext->m_pCurrMB->InterpolsrcChromaUStep[1],
                                pContext->m_pCurrMB->currUPlane,
                                pContext->m_pCurrMB->currUPitch);

                            ippiAverage8x8_8u_C1R(pContext->m_pCurrMB->pInterpolChromaVSrc[0],
                                pContext->m_pCurrMB->InterpolsrcChromaVStep[0],
                                pContext->m_pCurrMB->pInterpolChromaVSrc[1],
                                pContext->m_pCurrMB->InterpolsrcChromaVStep[1],
                                pContext->m_pCurrMB->currVPlane,
                                pContext->m_pCurrMB->currVPitch);
                        }
                        else
                        {
                            ippiMC8x8B_8u_C1(pContext->m_pCurrMB->pInterpolChromaUSrc[0],
                                pContext->m_pCurrMB->InterpolsrcChromaUStep[0], 0,
                                pContext->m_pCurrMB->pInterpolChromaUSrc[1],
                                pContext->m_pCurrMB->InterpolsrcChromaUStep[1], 0,
                                &pContext->m_pBlock[4*64], VC1_PIXEL_IN_CHROMA*2,
                                pContext->m_pCurrMB->currUPlane,
                                pContext->m_pCurrMB->currUPitch, 0);
                            ippiMC8x8B_8u_C1(pContext->m_pCurrMB->pInterpolChromaVSrc[0],
                                pContext->m_pCurrMB->InterpolsrcChromaVStep[0], 0,
                                pContext->m_pCurrMB->pInterpolChromaVSrc[1],
                                pContext->m_pCurrMB->InterpolsrcChromaVStep[1], 0,
                                &pContext->m_pBlock[5*64], VC1_PIXEL_IN_CHROMA*2,
                                pContext->m_pCurrMB->currVPlane,
                                pContext->m_pCurrMB->currVPitch, 0);
                        }
                    }
                    else
                    {
                        Ipp32u back = (pContext->m_pCurrMB->pInterpolChromaUSrc[0])?0:1;

                        // chroma
                        if (pContext->m_pCurrMB->SkipAndDirectFlag & 2)
                        {
                            ippiCopy_8u_C1R(pContext->m_pCurrMB->pInterpolChromaUSrc[back],
                                pContext->m_pCurrMB->InterpolsrcChromaUStep[back],
                                pContext->m_pCurrMB->currUPlane,
                                pContext->m_pCurrMB->currUPitch,
                                roiSize_8);
                            ippiCopy_8u_C1R(pContext->m_pCurrMB->pInterpolChromaVSrc[back],
                                pContext->m_pCurrMB->InterpolsrcChromaVStep[back],
                                pContext->m_pCurrMB->currVPlane,
                                pContext->m_pCurrMB->currVPitch,
                                roiSize_8);
                        }
                        else
                        {
                            ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolChromaUSrc[back],
                                pContext->m_pCurrMB->InterpolsrcChromaUStep[back],
                                &pContext->m_pBlock[4*64],
                                VC1_PIXEL_IN_CHROMA*2,
                                pContext->m_pCurrMB->currUPlane,
                                pContext->m_pCurrMB->currUPitch, 0, 0);
                            ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolChromaVSrc[back],
                                pContext->m_pCurrMB->InterpolsrcChromaVStep[back],
                                &pContext->m_pBlock[5*64],
                                VC1_PIXEL_IN_CHROMA*2,
                                pContext->m_pCurrMB->currVPlane,
                                pContext->m_pCurrMB->currVPitch, 0, 0);
                        }
                    }
                }

                ++pContext->m_pCurrMB;
                pContext->m_pBlock += 8*8*6;
                pContext->m_pSingleMB->m_currMBXpos++;
            }

            pContext->m_pSingleMB->m_currMBXpos = 0;
            ++pContext->m_pSingleMB->m_currMBYpos;
            ++pContext->m_pSingleMB->slice_currMBYpos;
            
                pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
                pContext->m_pBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
        }
    STATISTICS_END_TIME(m_timeStatistics->mc_StartTime,
                        m_timeStatistics->mc_EndTime,
                        m_timeStatistics->mc_TotalTime);
    }
    }

    // Smoothing
    STATISTICS_START_TIME(m_timeStatistics->smoothing_StartTime);
    pContext->m_pSingleMB->m_currMBXpos = 0;
    pContext->m_pSingleMB->m_currMBYpos = pTask->m_pSlice->MBStartRow;
    pContext->m_pBlock = pTask->m_pBlock;
    pContext->m_pCurrMB = &pContext->m_MBs[pTask->m_pSlice->MBStartRow*pContext->m_seqLayerHeader.MaxWidthMB];

    if (pContext->m_picLayerHeader->PTYPE !=VC1_B_FRAME)
    {
       MBSmooth_tbl[pContext->m_seqLayerHeader.PROFILE*4 + pContext->m_picLayerHeader->PTYPE](pContext,pTask->m_pSlice->MBRowsToDecode);
    }
    STATISTICS_END_TIME(m_timeStatistics->smoothing_StartTime,
            m_timeStatistics->smoothing_EndTime,
            m_timeStatistics->smoothing_TotalTime);

    ////set 0 on not coded part of outbut in case if MAX coded size != coded size
    //if((pContext->m_seqLayerHeader.MaxWidthMB != pContext->m_seqLayerHeader.widthMB) || 
    //    (pContext->m_seqLayerHeader.MaxHeightMB != pContext->m_seqLayerHeader.heightMB))
    //{
    //    Ipp32u i = 0;
    //    Ipp32u j = 0;
    //    Ipp8u* Yptr = pStartMB->currYPlane;
    //    Ipp8u* Uptr = pStartMB->currUPlane;
    //    Ipp8u* Vptr = pStartMB->currVPlane;

    //    Ipp32u YPitch = pStartMB->currYPitch;
    //    Ipp32u UPitch = pStartMB->currUPitch;
    //    Ipp32u VPitch = pStartMB->currVPitch;

    //    Ipp32u codedWidth =  2 * (pContext->m_seqLayerHeader.CODED_WIDTH +1);
    //            
    //    for(i = 0; i < pTask->m_pSlice->MBRowsToDecode; i++)
    //    {
    //        for(j = 0; j < VC1_PIXEL_IN_LUMA; j++)
    //        {
    //            memset(Yptr +  codedWidth, 0,   YPitch -  codedWidth);
    //            Yptr += YPitch;
    //        }
    //        for(j = 0; j < VC1_PIXEL_IN_CHROMA; j++)
    //        {
    //            memset(Uptr +  codedWidth/2, 0, UPitch -  codedWidth/2);
    //            memset(Vptr +  codedWidth/2, 0, VPitch -  codedWidth/2);

    //            Uptr += UPitch;
    //            Vptr += VPitch;
    //        }
    //    }
    //    if (pContext->m_pSingleMB->m_currMBYpos + pTask->m_pSlice->MBRowsToDecode 
    //        == pContext->m_seqLayerHeader.heightMB)
    //    {
    //        for(i = pContext->m_seqLayerHeader.heightMB; i < pContext->m_seqLayerHeader.MaxHeightMB; i++)
    //        {
    //            for(j = 0; j < VC1_PIXEL_IN_LUMA; j++)
    //            {
    //                memset(Yptr, 0,   YPitch);
    //                Yptr += YPitch;
    //            }
    //            for(j = 0; j < VC1_PIXEL_IN_CHROMA; j++)
    //           {
    //                memset(Uptr, 0, UPitch);
    //                memset(Vptr, 0, VPitch);

    //                Uptr += UPitch;
    //                Vptr += VPitch;
    //            }
    //        }
    //    }

    //}

#ifdef VC1_DEBUG_ON
    pContext->m_pSingleMB->m_currMBXpos = 0;
    pContext->m_pSingleMB->m_currMBYpos = pTask->m_pSlice->MBStartRow;
    pContext->m_pBlock = pTask->m_pBlock;
    //pContext->m_pPredBlock = pTask->m_pPredBlock;
    pContext->m_pCurrMB = &pContext->m_MBs[pTask->m_pSlice->MBStartRow*m_pContext->m_seqLayerHeader.MaxWidthMB];

    for (Ipp32s i=0;   i < pTask->m_pSlice->MBRowsToDecode;i++)
    {
        for (Ipp32s j = 0; j < pContext->m_pSingleMB->widthMB; j++)
        {
            if (VC1_DEBUG&VC1_SMOOTHINT)
                VM_Debug::GetInstance(VC1DebugRoutine)._print_macroblocks(pContext);

            ++pContext->m_pSingleMB->m_currMBXpos;
            pContext->m_pBlock += 8*8*6;
            ++pContext->m_pCurrMB;
        }

        pContext->m_pSingleMB->m_currMBXpos = 0;
        ++pContext->m_pSingleMB->m_currMBYpos;
        ++pContext->m_pSingleMB->slice_currMBYpos;

        pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
        pContext->m_pBlock += (sMB->MaxWidthtMB - sMB->widthMB)*8*8*6;
    }
#endif

    return VC1_OK;
}


VC1Status VC1TaskProcessorUMC::VC1Deblocking (VC1Context* pContext, VC1Task* pTask)
{
    if (m_pStore->IsNeedPostProcFrame(pTask->m_pSlice->m_picLayerHeader->PTYPE))
    {
        pContext->DeblockInfo.is_last_deblock = 0;
        pContext->DeblockInfo.HeightMB = pTask->m_pSlice->MBRowsToDecode;
        pContext->DeblockInfo.start_pos = pTask->m_pSlice->MBStartRow;
        Deblock* pDeblock = Deblock_tbl_Adv;
        Ipp32s MBHeight = 0;

        if (pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
            pDeblock = Deblock_tbl;

        if (VC1_IS_NO_TOP_MB(pContext->m_pCurrMB->LeftTopRightPositionFlag))
        {
            pContext->DeblockInfo.HeightMB +=1;
            pContext->DeblockInfo.start_pos -=1;
            pContext->m_pCurrMB -= pContext->m_pSingleMB->MaxWidthMB;
        }
        pContext->iPrevDblkStartPos = pContext->DeblockInfo.start_pos - pTask->m_pSlice->iPrevDblkStartPos;
        MBHeight = pContext->DeblockInfo.HeightMB - 1;

        if (pTask->m_pSlice->is_LastInSlice)
        {
            pContext->DeblockInfo.is_last_deblock = 1;
            ++MBHeight;
        }

STATISTICS_START_TIME(m_timeStatistics->deblocking_StartTime);

        pDeblock[pContext->m_picLayerHeader->PTYPE*3 + pContext->m_picLayerHeader->FCM](pContext);

STATISTICS_END_TIME(m_timeStatistics->deblocking_StartTime,
                    m_timeStatistics->deblocking_EndTime,
                    m_timeStatistics->deblocking_TotalTime);
    }

    return VC1_OK;
}

Status VC1TaskProcessorUMC::Init(VC1Context* pContext,
                              Ipp32s iNumber,
                              VC1TaskStore*      pStore,
                              MemoryAllocator* pMemoryAllocator)
{
    m_iNumber = iNumber;
    pContext->iNumber = iNumber;
    m_pMemoryAllocator = pMemoryAllocator;
    m_pStore = pStore;


   Ipp8u* pBuf;
   if (m_pMemoryAllocator->Alloc(&m_iMemContextID,
                                  align_value<Ipp32u>(sizeof(VC1Context))+ align_value<Ipp32u>(sizeof(VC1SingletonMB)),
                                  UMC_ALLOC_PERSISTENT,
                                  16) != UMC_OK)
                                  return false;

    pBuf = (Ipp8u*)m_pMemoryAllocator->Lock(m_iMemContextID);
    m_pContext = (VC1Context*)pBuf;
    m_pSingleMB = (VC1SingletonMB*)(pBuf +  align_value<Ipp32u>(sizeof(VC1Context)));
    memset(m_pContext, 0, sizeof(VC1Context));
    memset(m_pSingleMB, 0, sizeof(VC1SingletonMB));

    m_pContext->savedMV = pContext->savedMV;
    m_pContext->savedMVSamePolarity = pContext->savedMVSamePolarity;

    m_pSingleMB->heightMB = pContext->m_seqLayerHeader.heightMB;
    m_pSingleMB->widthMB = pContext->m_seqLayerHeader.widthMB;

    m_pSingleMB->MaxHeightMB = pContext->m_seqLayerHeader.MaxHeightMB;
    m_pSingleMB->MaxWidthMB = pContext->m_seqLayerHeader.MaxWidthMB;


    return UMC_OK;
}

Status VC1TaskProcessorUMC::InitPicParamJob()
{
    return UMC_OK;
}

void VC1TaskProcessorUMC::InitContextForNextTask(VC1Task* pTask)
{
    m_pContext->interp_params_luma.roundControl = m_pContext->m_picLayerHeader->RNDCTRL;
    m_pContext->interp_params_chroma.roundControl = m_pContext->m_picLayerHeader->RNDCTRL;
    m_pContext->m_pCurrMB = &m_pContext->m_MBs[pTask->m_pSlice->MBStartRow*m_pContext->m_seqLayerHeader.MaxWidthMB];
    m_pContext->CurrDC = &m_pContext->DCACParams[pTask->m_pSlice->MBStartRow*m_pContext->m_seqLayerHeader.MaxWidthMB];
    m_pContext->m_pSingleMB->m_currMBXpos = 0;
    m_pContext->m_picLayerHeader = pTask->m_pSlice->m_picLayerHeader;
    m_pContext->m_pSingleMB->m_currMBYpos = pTask->m_pSlice->MBStartRow;
    m_pContext->m_pSingleMB->slice_currMBYpos = pTask->m_pSlice->MBStartRow;
    m_pContext->m_pSingleMB->EscInfo = pTask->m_pSlice->EscInfo;

    if ((m_pContext->m_picLayerHeader->CurrField)&&(m_pContext->m_pSingleMB->slice_currMBYpos))
        m_pContext->m_pSingleMB->slice_currMBYpos -= (m_pContext->m_seqLayerHeader.heightMB+1)/2;
    m_pContext->m_pBlock = pTask->m_pBlock;
}

Status VC1TaskProcessorUMC::process()
{
    VC1Task* task;
    VC1FrameDescriptor*    pFrameDS;
    //VC1TaskStore* pStore = VC1TaskStore::GetInstance(VC1Routine);
#ifdef  VC1_THREAD_STATISTIC
    task->pJob = this;
#endif
    bool isFrameComplete = false;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VC1Dec_work");
    // Routine task
    if (m_pStore->GetNextTaskMainThread(&pFrameDS,&task,m_iNumber,isFrameComplete))
    {
        if (task)
        {
            ippsCopy_8u((Ipp8u*)pFrameDS->m_pContext,(Ipp8u*)m_pContext,sizeof(VC1Context));
            // we don't copy this field from pFrameDS
            m_pContext->m_pSingleMB =  m_pSingleMB;

            // coded sizes differs from max sizes
            if (m_pContext->m_seqLayerHeader.widthMB != m_pContext->m_pSingleMB->widthMB)
            {
                //printf("changed\n");
                m_pContext->m_pSingleMB->widthMB = m_pContext->m_seqLayerHeader.widthMB;
                m_pContext->m_pSingleMB->MaxWidthMB = m_pContext->m_seqLayerHeader.MaxWidthMB;
            }
            if (m_pContext->m_seqLayerHeader.heightMB != m_pContext->m_pSingleMB->heightMB)
            {
                //printf("changed\n");
                m_pContext->m_pSingleMB->heightMB    = m_pContext->m_seqLayerHeader.heightMB;
                m_pContext->m_pSingleMB->MaxHeightMB = m_pContext->m_seqLayerHeader.MaxHeightMB;
            }
             
            m_pContext->m_pSingleMB->currYPitch = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_iYPitch;
            m_pContext->m_pSingleMB->currUPitch = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_iUPitch;
            m_pContext->m_pSingleMB->currVPitch = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_iVPitch;

            m_pContext->m_pSingleMB->currYPlane = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_pY;
            m_pContext->m_pSingleMB->currUPlane = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_pU;
            m_pContext->m_pSingleMB->currVPlane = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_pV;


            m_pContext->interp_params_luma.roundControl = m_pContext->m_picLayerHeader->RNDCTRL;
            m_pContext->interp_params_chroma.roundControl = m_pContext->m_picLayerHeader->RNDCTRL;
            
            m_pContext->m_pCurrMB = &m_pContext->m_MBs[task->m_pSlice->MBStartRow*m_pContext->m_seqLayerHeader.MaxWidthMB];
            m_pContext->CurrDC = &m_pContext->DCACParams[task->m_pSlice->MBStartRow*m_pContext->m_seqLayerHeader.MaxWidthMB];
            m_pContext->m_pSingleMB->m_currMBXpos = 0;
            m_pContext->m_picLayerHeader = task->m_pSlice->m_picLayerHeader;
            m_pContext->m_pSingleMB->m_currMBYpos = task->m_pSlice->MBStartRow;
            m_pContext->m_pSingleMB->slice_currMBYpos = task->m_pSlice->MBStartRow;
            m_pContext->m_pSingleMB->EscInfo = task->m_pSlice->EscInfo;


            if ((m_pContext->m_picLayerHeader->CurrField)&&(m_pContext->m_pSingleMB->slice_currMBYpos))
                m_pContext->m_pSingleMB->slice_currMBYpos -= (m_pContext->m_seqLayerHeader.heightMB+1)/2;

            if(m_pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
            {
                m_pContext->m_pSingleMB->heightMB = m_pContext->m_seqLayerHeader.heightMB + (m_pContext->m_seqLayerHeader.heightMB & 1);
                m_pContext->m_pSingleMB->MaxHeightMB = m_pContext->m_seqLayerHeader.MaxHeightMB + (m_pContext->m_seqLayerHeader.MaxHeightMB & 1);
            }
            else
                 m_pContext->m_pSingleMB->heightMB = m_pContext->m_seqLayerHeader.heightMB;

            m_pContext->m_pBlock = task->m_pBlock;

            m_pContext->m_bitstream.pBitstream = task->m_pSlice->m_pstart;
            m_pContext->m_bitstream.bitOffset = task->m_pSlice->m_bitOffset;

            try
            {
                (this->*(task->pMulti))(m_pContext,task);

                task->m_pSlice->m_pstart = m_pContext->m_bitstream.pBitstream;
                task->m_pSlice->m_bitOffset = m_pContext->m_bitstream.bitOffset;
                task->m_pSlice->EscInfo = m_pContext->m_pSingleMB->EscInfo;
                m_pStore->AddPerfomedTask(task,pFrameDS);
            }
            catch (vc1_exception ex)
            {
                exception_type e_type = ex.get_exception_type();;
                if (e_type != vld)
                {
                    m_pStore->AddInvalidPerformedDS(pFrameDS);
                    m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].corrupted = ERROR_FRAME_MINOR;
                }
                else
                {
                    //VLD exception. FrameLevel
                    DecodeSkippicture(pFrameDS->m_pContext);
                    // need to stop all threads
                    m_pStore->AddPerformedDS(pFrameDS);
                    m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].corrupted = ERROR_FRAME_MAJOR;
                }
            }
            return UMC_ERR_NOT_ENOUGH_DATA; // means that piece of work has done
        }
        if (m_pStore->HaveTasksToBeDone())
            return UMC_WRN_INFO_NOT_READY; // means that no ready tasks but tasks present
        else
            return UMC_LAST_FRAME; // means that all descriptors are free. Important for last frames processing
    }
    return (isFrameComplete)?UMC_OK:UMC_ERR_NOT_ENOUGH_DATA;
}
void VC1TaskProcessorUMC::Release() // free mem
{
   if(m_pMemoryAllocator)
   {
       if (m_iMemContextID != (MemID)-1)
       {
           m_pMemoryAllocator->Unlock(m_iMemContextID);
           m_pMemoryAllocator->Free(m_iMemContextID);
           m_iMemContextID = (MemID)-1;
       }

   }
}
void   VC1TaskProcessorUMC::WriteDiffs(VC1Context* pContext)
{
    IppiSize  roiSize;
    Ipp32u IntraFlag;
    Ipp32u offset_table[] = {0,8,128,136};
    Ipp32u plane_offset = 0;

    if (pContext->m_pCurrMB->mbType == VC1_MB_INTRA)
    {
        //write to plane
        STATISTICS_START_TIME(m_timeStatistics->write_plane_StartTime);

        if (pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
            write_Intraluma_to_interlace_frame_Adv(pContext->m_pCurrMB, pContext->m_pBlock);
        else
        {

            roiSize.height = VC1_PIXEL_IN_LUMA;
            roiSize.width = VC1_PIXEL_IN_LUMA;

            ippiConvert_16s8u_C1R(pContext->m_pBlock,
                VC1_PIXEL_IN_LUMA*2,
                pContext->m_pCurrMB->currYPlane,
                pContext->m_pCurrMB->currYPitch,
                roiSize);
        }

        roiSize.height = VC1_PIXEL_IN_CHROMA;
        roiSize.width = VC1_PIXEL_IN_CHROMA;

        ippiConvert_16s8u_C1R(pContext->m_pBlock+64*4,
            VC1_PIXEL_IN_CHROMA*2,
            pContext->m_pCurrMB->currUPlane,
            pContext->m_pCurrMB->currUPitch,
            roiSize);

        ippiConvert_16s8u_C1R(pContext->m_pBlock+64*5,
            VC1_PIXEL_IN_CHROMA*2,
            pContext->m_pCurrMB->currVPlane,
            pContext->m_pCurrMB->currVPitch,
            roiSize);

        STATISTICS_END_TIME(m_timeStatistics->write_plane_StartTime,
            m_timeStatistics->write_plane_EndTime,
            m_timeStatistics->write_plane_TotalTime);
    }
    else
    {
        roiSize.height = 8;
        roiSize.width = 8;

        //write to plane
        STATISTICS_START_TIME(m_timeStatistics->write_plane_StartTime);

        IntraFlag = pContext->m_pCurrMB->IntraFlag;
        for (Ipp32s blk_num = 0; blk_num<4 ;blk_num++)
        {
            if (IntraFlag & 1) // from assignpattern
            {
                plane_offset = (blk_num&1)*8 + (blk_num&2)*4*pContext->m_pCurrMB->currYPitch;
                ippiConvert_16s8u_C1R(&pContext->m_pBlock[offset_table[blk_num]],
                    VC1_PIXEL_IN_LUMA*2,
                    pContext->m_pCurrMB->currYPlane + plane_offset,
                    pContext->m_pCurrMB->currYPitch,
                    roiSize);
            }
            IntraFlag >>= 1;
        }

        if (IntraFlag & 1) // from assignpattern
            ippiConvert_16s8u_C1R(pContext->m_pBlock+64*VC1_NUM_OF_LUMA,
            VC1_PIXEL_IN_CHROMA*2,
            pContext->m_pCurrMB->currUPlane,
            pContext->m_pCurrMB->currUPitch,
            roiSize);

        if (IntraFlag & 2) // from assignpattern
            ippiConvert_16s8u_C1R(pContext->m_pBlock+64*5,
            VC1_PIXEL_IN_CHROMA*2,
            pContext->m_pCurrMB->currVPlane,
            pContext->m_pCurrMB->currVPitch,
            roiSize);

        STATISTICS_END_TIME(m_timeStatistics->write_plane_StartTime,
            m_timeStatistics->write_plane_EndTime,
            m_timeStatistics->write_plane_TotalTime);
    }
}
void VC1TaskProcessorUMC::ProcessSmartException (SmartLevel exLevel, VC1Context* pContext, VC1Task* pTask, VC1MB* pCurrMB)
{
    if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE))
    {
        if (exLevel == mbOneLevel)
        {

            // we should replace MB with reference
            pCurrMB->mbType = VC1_MB_1MV_INTER;
            pCurrMB->m_pBlocks[0].mv[0][0] = 0;
            pCurrMB->m_pBlocks[0].mv[0][1] = 0;
            pCurrMB->m_pBlocks[0].mv_bottom[0][0] = 0;
            pCurrMB->m_pBlocks[0].mv_bottom[0][1] = 0;
            pCurrMB->IntraFlag = 0;
        }
        else if (exLevel == mbGroupLevel)
        {
            VC1MB* pStartMB = &m_pContext->m_MBs[pTask->m_pSlice->MBStartRow*m_pContext->m_seqLayerHeader.MaxWidthMB];
            Ipp32u numOfMarkMB = pTask->m_pSlice->MBRowsToDecode*m_pContext->m_seqLayerHeader.MaxWidthMB;
            for (Ipp32u i = 0; i < numOfMarkMB; i++)
            {
                pStartMB->mbType = VC1_MB_1MV_INTER;
                pStartMB->m_pBlocks[0].mv[0][0] = 0;
                pStartMB->m_pBlocks[0].mv[0][1] = 0;
                pStartMB->m_pBlocks[0].mv_bottom[0][0] = 0;
                pStartMB->m_pBlocks[0].mv_bottom[0][1] = 0;
                pStartMB->IntraFlag = 0;
                ++pStartMB;
            }

        }
    }
}
void VC1TaskProcessorUMC::CompensateInterlacePFrame(VC1Context* pContext, VC1Task *pTask)
{
    static IppiSize  roiSize_8;

    roiSize_8.height = 8;
    roiSize_8.width = 8;

    STATISTICS_START_TIME(m_timeStatistics->mc_StartTime);
    for (Ipp32s i = 0; i < pTask->m_pSlice->MBRowsToDecode;i++)
    {
        for (Ipp32s j = 0; j < pContext->m_pSingleMB->widthMB; j++)
        {
            if(!pContext->m_pCurrMB->IntraFlag)
            {
                if (pContext->m_pCurrMB->pInterpolLumaSrc[0])
                    write_Interluma_to_interlace_frame_MC_Adv(pContext->m_pCurrMB,
                                                              pContext->m_pCurrMB->pInterpolLumaSrc[0],
                                                              pContext->m_pCurrMB->currYPitch,
                                                              pContext->m_pBlock);
                else if (!(pContext->m_pCurrMB->SkipAndDirectFlag & 2))
                    write_Interluma_to_interlace_frame_MC_Adv_Copy   (pContext->m_pCurrMB,
                                                                      pContext->m_pBlock);


                if (pContext->m_pCurrMB->SkipAndDirectFlag & 2)
                {
                    ippiCopy_8u_C1R(pContext->m_pCurrMB->pInterpolChromaUSrc[0],
                        pContext->m_pCurrMB->InterpolsrcChromaUStep[0],
                        pContext->m_pCurrMB->currUPlane,
                        pContext->m_pCurrMB->currUPitch,
                        roiSize_8);
                    ippiCopy_8u_C1R(pContext->m_pCurrMB->pInterpolChromaVSrc[0],
                        pContext->m_pCurrMB->InterpolsrcChromaVStep[0],
                        pContext->m_pCurrMB->currVPlane,
                        pContext->m_pCurrMB->currVPitch,
                        roiSize_8);
                }
                else
                {
                    ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolChromaUSrc[0],
                        pContext->m_pCurrMB->InterpolsrcChromaUStep[0],
                        &pContext->m_pBlock[4*64],
                        VC1_PIXEL_IN_CHROMA*2,
                        pContext->m_pCurrMB->currUPlane,
                        pContext->m_pCurrMB->currUPitch, 0, 0);

                    ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolChromaVSrc[0],
                        pContext->m_pCurrMB->InterpolsrcChromaVStep[0],
                        &pContext->m_pBlock[5*64],
                        VC1_PIXEL_IN_CHROMA*2,
                        pContext->m_pCurrMB->currVPlane,
                        pContext->m_pCurrMB->currVPitch, 0, 0);
                }
            }


            ++pContext->m_pCurrMB;
            pContext->m_pBlock += 8*8*6;
            pContext->m_pSingleMB->m_currMBXpos++;
        }


        pContext->m_pSingleMB->m_currMBXpos = 0;
        ++pContext->m_pSingleMB->m_currMBYpos;
        ++pContext->m_pSingleMB->slice_currMBYpos;
        pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
        pContext->m_pBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
    }


    STATISTICS_END_TIME(m_timeStatistics->mc_StartTime,
        m_timeStatistics->mc_EndTime,
        m_timeStatistics->mc_TotalTime);

    STATISTICS_END_TIME(m_timeStatistics->mc_StartTime,
        m_timeStatistics->mc_EndTime,
        m_timeStatistics->mc_TotalTime);
    //STATISTICS_START_TIME(m_timeStatistics->smoothing_StartTime);
    //pContext->m_pSingleMB->m_currMBXpos = 0;
    //pContext->m_pSingleMB->m_currMBYpos = pTask->m_pSlice->MBStartRow;
    //pContext->m_pBlock = pTask->m_pBlock;
    //pContext->m_pCurrMB = &pContext->m_MBs[pTask->m_pSlice->MBStartRow*pContext->m_seqLayerHeader.widthMB];

    //if (m_pStore->IsNeedPostProcFrame(pTask->m_pSlice->m_picLayerHeader->PTYPE))
    //{
    //    MBSmooth_tbl[pContext->m_seqLayerHeader.PROFILE*4 + pContext->m_picLayerHeader->PTYPE](pContext,pTask->m_pSlice->MBRowsToDecode);
    //}
    //STATISTICS_END_TIME(m_timeStatistics->smoothing_StartTime,
    //    m_timeStatistics->smoothing_EndTime,
    //    m_timeStatistics->smoothing_TotalTime);
}
void VC1TaskProcessorUMC::CompensateInterlaceBFrame(VC1Context* pContext, VC1Task *pTask)
{
    static IppiSize  roiSize_8;

    roiSize_8.height = 8;
    roiSize_8.width = 8;

    STATISTICS_START_TIME(m_timeStatistics->mc_StartTime);
    for (Ipp32s i = 0; i < pTask->m_pSlice->MBRowsToDecode;i++)
    {
        for (Ipp32s j = 0; j < pContext->m_pSingleMB->widthMB; j++)
        {

            if(!pContext->m_pCurrMB->IntraFlag)
            {
                if((pContext->m_pCurrMB->pInterpolLumaSrc[0] &&
                    pContext->m_pCurrMB->pInterpolLumaSrc[1] ))

                {
                    if (!(pContext->m_pCurrMB->SkipAndDirectFlag & 2))
                        write_Interluma_to_interlace_frame_MC_Adv_Copy(pContext->m_pCurrMB,pContext->m_pBlock);
                }
                else
                {
                    Ipp32u back = (pContext->m_pCurrMB->pInterpolLumaSrc[0])?0:1;
                    if (pContext->m_pCurrMB->pInterpolLumaSrc[back] == pContext->m_pCurrMB->currYPlane)
                    {
                        if (!(pContext->m_pCurrMB->SkipAndDirectFlag & 2))
                            write_Interluma_to_interlace_frame_MC_Adv_Copy(pContext->m_pCurrMB,pContext->m_pBlock);
                    }
                    else
                    {
                        write_Interluma_to_interlace_frame_MC_Adv(pContext->m_pCurrMB,
                                                                  pContext->m_pCurrMB->pInterpolLumaSrc[back],
                                                                  pContext->m_pCurrMB->InterpolsrcLumaStep[back],
                                                                  pContext->m_pBlock);
                    }

                }

                if(pContext->m_pCurrMB->pInterpolChromaUSrc[0] &&
                   pContext->m_pCurrMB->pInterpolChromaUSrc[1])
                {
                    if (pContext->m_pCurrMB->SkipAndDirectFlag & 2)
                    {
                        ippiAverage8x8_8u_C1R(pContext->m_pCurrMB->pInterpolChromaUSrc[0],
                            pContext->m_pCurrMB->InterpolsrcChromaUStep[0],
                            pContext->m_pCurrMB->pInterpolChromaUSrc[1],
                            pContext->m_pCurrMB->InterpolsrcChromaUStep[1],
                            pContext->m_pCurrMB->currUPlane,
                            pContext->m_pCurrMB->currUPitch);
                        ippiAverage8x8_8u_C1R(pContext->m_pCurrMB->pInterpolChromaVSrc[0],
                            pContext->m_pCurrMB->InterpolsrcChromaVStep[0],
                            pContext->m_pCurrMB->pInterpolChromaVSrc[1],
                            pContext->m_pCurrMB->InterpolsrcChromaVStep[1],
                            pContext->m_pCurrMB->currVPlane,
                            pContext->m_pCurrMB->currVPitch);
                    }
                    else
                    {
                        ippiMC8x8B_8u_C1(pContext->m_pCurrMB->pInterpolChromaUSrc[0],
                            pContext->m_pCurrMB->InterpolsrcChromaUStep[0], 0,
                            pContext->m_pCurrMB->pInterpolChromaUSrc[1],
                            pContext->m_pCurrMB->InterpolsrcChromaUStep[1], 0,
                            &pContext->m_pBlock[4*64], VC1_PIXEL_IN_CHROMA*2,
                            pContext->m_pCurrMB->currUPlane,
                            pContext->m_pCurrMB->currUPitch, 0);
                        ippiMC8x8B_8u_C1(pContext->m_pCurrMB->pInterpolChromaVSrc[0],
                            pContext->m_pCurrMB->InterpolsrcChromaVStep[0], 0,
                            pContext->m_pCurrMB->pInterpolChromaVSrc[1],
                            pContext->m_pCurrMB->InterpolsrcChromaVStep[1], 0,
                            &pContext->m_pBlock[5*64], VC1_PIXEL_IN_CHROMA*2,
                            pContext->m_pCurrMB->currVPlane,
                            pContext->m_pCurrMB->currVPitch, 0);
                    }
                }
                else
                {
                    Ipp32u back = (pContext->m_pCurrMB->pInterpolLumaSrc[0])?0:1;

                    // chroma
                    if (pContext->m_pCurrMB->SkipAndDirectFlag & 2)
                    {
                        ippiCopy_8u_C1R(pContext->m_pCurrMB->pInterpolChromaUSrc[back],
                            pContext->m_pCurrMB->InterpolsrcChromaUStep[back],
                            pContext->m_pCurrMB->currUPlane,
                            pContext->m_pCurrMB->currUPitch,
                            roiSize_8);
                        ippiCopy_8u_C1R(pContext->m_pCurrMB->pInterpolChromaVSrc[back],
                            pContext->m_pCurrMB->InterpolsrcChromaVStep[back],
                            pContext->m_pCurrMB->currVPlane,
                            pContext->m_pCurrMB->currVPitch,
                            roiSize_8);
                    }
                    else
                    {
                        ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolChromaUSrc[back],
                            pContext->m_pCurrMB->InterpolsrcChromaUStep[back],
                            &pContext->m_pBlock[4*64],
                            VC1_PIXEL_IN_CHROMA*2,
                            pContext->m_pCurrMB->currUPlane,
                            pContext->m_pCurrMB->currUPitch, 0, 0);
                        ippiMC8x8_8u_C1(pContext->m_pCurrMB->pInterpolChromaVSrc[back],
                            pContext->m_pCurrMB->InterpolsrcChromaVStep[back],
                            &pContext->m_pBlock[5*64],
                            VC1_PIXEL_IN_CHROMA*2,
                            pContext->m_pCurrMB->currVPlane,
                            pContext->m_pCurrMB->currVPitch, 0, 0);
                    }
                }
            }

            ++pContext->m_pCurrMB;
            pContext->m_pBlock += 8*8*6;
            pContext->m_pSingleMB->m_currMBXpos++;
        }

        pContext->m_pSingleMB->m_currMBXpos = 0;
        ++pContext->m_pSingleMB->m_currMBYpos;
        ++pContext->m_pSingleMB->slice_currMBYpos;
         pContext->m_pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
         pContext->m_pBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
    }
    STATISTICS_END_TIME(m_timeStatistics->mc_StartTime,
        m_timeStatistics->mc_EndTime,
        m_timeStatistics->mc_TotalTime);
}

#endif
