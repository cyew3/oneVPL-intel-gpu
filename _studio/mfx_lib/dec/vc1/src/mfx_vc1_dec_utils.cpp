/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2011 Intel Corporation. All Rights Reserved.
//          MFX VC-1 DEC utils
//
*/
#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_DEC)
#include "mfx_vc1_dec_utils.h"
#include "umc_vc1_dec_seq.h"

namespace MfxVC1DECUnpacking
{

    void      TranslateMfxToUmcIQT(mfxMbCode*      pMbCode,
                                   VC1MB*          pCurMB,
                                   VC1DCMBParam*   pCurrDC)
    {
        Ipp32u IntraCounter = 0;
        if (pMbCode->VC1.IntraMbFlag == 1)
        {
            pCurMB->mbType = VC1_MB_INTRA;
            pCurMB->IntraFlag = 63;
        }
        else
            pCurMB->IntraFlag = 0;

        pCurMB->Overlap = pMbCode->VC1.OverlapFilter;
        //pMbCode->VC1.FieldMbFlag = VC1_IS_MVFIELD(pCurMB->mbType); TBD
        pCurMB->FIELDTX = pMbCode->VC1.TransformFlag;
        pCurMB->m_cbpBits = 0;


        UnPackCodedBlockPattern(pCurMB,pMbCode);


        // Non zero coeffs flag.  TBD
        //for (count = 0; count < 5; count++)
        //    pCurMB->m_cbpBits |= (pMbCode->VC1.NzCoefCount[count] & 1) << (5-count);

        //if ((pMbCode->VC1.MbType5Bits == MFX_TYPE_SKIP_16X16_0)||
        //    (pMbCode->VC1.MbType5Bits == MFX_TYPE_SKIP_16X16_1)||
        //    (pMbCode->VC1.MbType5Bits == MFX_TYPE_SKIP_16X16_2))
        //    pCurMB->SkipAndDirectFlag = 1;

        //// TBD
        //pCurMB->SkipAndDirectFlag = (pMbCode->VC1.Skip8x8Flag == 0xF)?1:0;
        pCurMB->SkipAndDirectFlag = CalculateSkipDirectFlag(pMbCode->VC1.MbType5Bits, pMbCode->VC1.Skip8x8Flag);

        if (pMbCode->VC1.IntraMbFlag != 1)
        {
            for (Ipp32u count = 0; count < 4; count++)
            {
                switch((pMbCode->VC1.SubMbShape >> ((3 - count)*2) & 0x03))
                {
                case 0:
                    //if (pContext->m_picLayerHeader->PTYPE == VC1_P_FRAME) TBD
                    {
                        switch ((pMbCode->VC1.SubMbPredMode >> ((3 - count)*2) & 0x03))
                        {
                        case 0:
                            pCurMB->m_pBlocks[count].blkType = VC1_BLK_INTRA;
                            pCurMB->IntraFlag |= 1 << count;
                            IntraCounter++;
                            break;
                        case 3:
                            pCurMB->m_pBlocks[count].blkType = VC1_BLK_INTER8X8;
                            break;
                        default:
                            break;
                        }
                    }
                    break;
                case 1:
                    pCurMB->m_pBlocks[count].blkType = VC1_BLK_INTER8X4;
                    break;
                case 2:
                    pCurMB->m_pBlocks[count].blkType = VC1_BLK_INTER4X8;
                    break;
                case 3:
                    pCurMB->m_pBlocks[count].blkType = VC1_BLK_INTER4X4;
                    break;
                default:
                    break;

                }
            }
            switch(pMbCode->VC1.SubMbShapeU)
            {
            case 0:
                if (IntraCounter >2)
                {
                    pCurMB->m_pBlocks[4].blkType = VC1_BLK_INTRA;
                    pCurMB->IntraFlag |= 1 << 4;
                }
                else
                {
                    pCurMB->m_pBlocks[4].blkType = VC1_BLK_INTER8X8;
                }
                break;
            case 1:
                pCurMB->m_pBlocks[4].blkType = VC1_BLK_INTER8X4;
                pCurMB->m_pBlocks[5].blkType = VC1_BLK_INTER8X4;
                break;
            case 2:
                pCurMB->m_pBlocks[4].blkType = VC1_BLK_INTER4X8;
                pCurMB->m_pBlocks[5].blkType = VC1_BLK_INTER4X8;
                break;
            case 3:
                pCurMB->m_pBlocks[4].blkType = VC1_BLK_INTER4X4;
                pCurMB->m_pBlocks[5].blkType = VC1_BLK_INTER4X4;
                break;
            default:
                break;

            }
            switch(pMbCode->VC1.SubMbShapeV)
            {
            case 0:
                if (IntraCounter >2)
                {
                    pCurMB->m_pBlocks[5].blkType = VC1_BLK_INTRA;
                    pCurMB->IntraFlag |= 1 << 5;
                }
                else
                {
                    pCurMB->m_pBlocks[5].blkType = VC1_BLK_INTER8X8;
                }
                break;
            case 1:
                pCurMB->m_pBlocks[5].blkType = VC1_BLK_INTER8X4;
                break;
            case 2:
                pCurMB->m_pBlocks[5].blkType = VC1_BLK_INTER4X8;
                break;
            case 3:
                pCurMB->m_pBlocks[5].blkType = VC1_BLK_INTER4X4;
                break;
            default:
                break;

            }
        }
        else
        {
            for (Ipp32u count = 0; count < 6; count++)
            {
                pCurMB->m_pBlocks[count].blkType = VC1_BLK_INTRA;
            }
        }

        pCurrDC->DoubleQuant = pMbCode->VC1.QpScaleCode;
        pCurrDC->DCStepSize = pMbCode->VC1.QpScaleType;
    }
    void      BiasDetermination(VC1Context*              pContext)
    {
        pContext->m_pCurrMB->bias = 0;
        if (pContext->m_seqLayerHeader->PROFILE != VC1_PROFILE_ADVANCED)
        {
            if (pContext->m_picLayerHeader->PQUANT >= 9)
            {
                pContext->m_pCurrMB->Overlap = (Ipp8u)pContext->m_seqLayerHeader->OVERLAP;
                pContext->m_pCurrMB->bias = 128 * pContext->m_pCurrMB->Overlap;
            }
            else
                pContext->m_pCurrMB->Overlap =0;
        }
    }
    void      TranslateMfxToUmcPredDec(mfxMbCode*      pMbCode,
                                       VC1MB*          pCurMB)
    {
        pCurMB->Overlap = pMbCode->VC1.OverlapFilter;

        if (pMbCode->VC1.IntraMbFlag)
        {
            pCurMB->mbType =  VC1_MB_INTRA;
            return;
        }
        pCurMB->mbType = 0;
        pCurMB->SkipAndDirectFlag = 0;
       // MFX_TYPE_SKIP_16X16_0 == MFX_TYPE_INTER_16X16_0 ?????????????????????????
        switch (pMbCode->VC1.MbType & 0x7F)
        {
        case MFX_MBTYPE_INTER_8X8_0:
            pCurMB->mbType |= VC1_MB_4MV_INTER;
            pCurMB->mbType |=  VC1_MB_FORWARD;
            break;
        case MFX_MBTYPE_INTER_FIELD_8X8_00:
            pCurMB->mbType |= VC1_MB_4MV_FIELD_INTER;
            pCurMB->mbType |=  VC1_MB_FORWARD;
            break;
        case MFX_MBTYPE_INTRA_FIELD_VC1:
            pCurMB->mbType = VC1_MB_INTRA;
            break;
       // case MFX_TYPE_SKIP_16X16_0:
        case MFX_MBTYPE_INTER_16X16_0:
            pCurMB->mbType |= VC1_MB_1MV_INTER;
            pCurMB->mbType |=  VC1_MB_FORWARD;
            break;
        //case MFX_TYPE_SKIP_16X16_1:
        case MFX_MBTYPE_INTER_16X16_1:
            pCurMB->mbType |= VC1_MB_1MV_INTER;
            pCurMB->mbType |= VC1_MB_BACKWARD;
            break;
        //case MFX_TYPE_SKIP_16X16_2:
        case MFX_MBTYPE_INTER_16X16_2:
            pCurMB->mbType |= VC1_MB_1MV_INTER;
            pCurMB->mbType |= VC1_MB_INTERP;
            break;
        case MFX_MBTYPE_INTER_16X16_DIR:
            pCurMB->mbType |= VC1_MB_DIRECT;
            pCurMB->mbType |= VC1_MB_1MV_INTER;
        case MFX_MBTYPE_INTER_16X8_00:
            pCurMB->mbType |= VC1_MB_2MV_INTER;
            pCurMB->mbType |=  VC1_MB_FORWARD;
            break;
        case MFX_MBTYPE_INTER_FIELD_16X8_00:
            pCurMB->mbType |= VC1_MB_2MV_INTER;
            pCurMB->mbType |=  VC1_MB_FORWARD;
            break;
            // add field MV
        default:
            break;
        }

        //unpack MV's
        if (!VC1_IS_MVFIELD(pCurMB->mbType))
        {
            // forward motion vectors
            pCurMB->m_pBlocks[0].mv_bottom[0][0] = pCurMB->m_pBlocks[0].mv[0][0] = pMbCode->VC1.MV[0][0];
            pCurMB->m_pBlocks[0].mv_bottom[0][1] = pCurMB->m_pBlocks[0].mv[0][1] = pMbCode->VC1.MV[0][1];
            pCurMB->m_pBlocks[1].mv_bottom[0][0] = pCurMB->m_pBlocks[1].mv[0][0] = pMbCode->VC1.MV[1][0];
            pCurMB->m_pBlocks[1].mv_bottom[0][1] = pCurMB->m_pBlocks[1].mv[0][1] = pMbCode->VC1.MV[1][1];
            pCurMB->m_pBlocks[2].mv_bottom[0][0] = pCurMB->m_pBlocks[2].mv[0][0] = pMbCode->VC1.MV[2][0];
            pCurMB->m_pBlocks[2].mv_bottom[0][1] = pCurMB->m_pBlocks[2].mv[0][1] = pMbCode->VC1.MV[2][1];
            pCurMB->m_pBlocks[3].mv_bottom[0][0] = pCurMB->m_pBlocks[3].mv[0][0] = pMbCode->VC1.MV[3][0];
            pCurMB->m_pBlocks[3].mv_bottom[0][1] = pCurMB->m_pBlocks[3].mv[0][1] = pMbCode->VC1.MV[3][1];

            // backward motion vectors
            pCurMB->m_pBlocks[0].mv[1][0] = pCurMB->m_pBlocks[0].mv[1][0] = pMbCode->VC1.MV[4][0];
            pCurMB->m_pBlocks[0].mv[1][1] = pCurMB->m_pBlocks[0].mv[1][1] = pMbCode->VC1.MV[4][1];
            pCurMB->m_pBlocks[1].mv[1][0] = pCurMB->m_pBlocks[1].mv[1][0] = pMbCode->VC1.MV[5][0];
            pCurMB->m_pBlocks[1].mv[1][1] = pCurMB->m_pBlocks[1].mv[1][1] = pMbCode->VC1.MV[5][1];
            pCurMB->m_pBlocks[2].mv[1][0] = pCurMB->m_pBlocks[2].mv[1][0] = pMbCode->VC1.MV[6][0];
            pCurMB->m_pBlocks[2].mv[1][1] = pCurMB->m_pBlocks[2].mv[1][1] = pMbCode->VC1.MV[6][1];
            pCurMB->m_pBlocks[3].mv[1][0] = pCurMB->m_pBlocks[3].mv[1][0] = pMbCode->VC1.MV[7][0];
            pCurMB->m_pBlocks[3].mv[1][1] = pCurMB->m_pBlocks[3].mv[1][1] = pMbCode->VC1.MV[7][1];
        }
        else if (VC1_MB_2MV_INTER == VC1_GET_MBTYPE(pCurMB->mbType))
        {
            Ipp32u count;
            for (count = 0; count < 4; count++)
            {
            // forward motion vectors. Top/Bottom Field
            pCurMB->m_pBlocks[count].mv[0][0] = pMbCode->VC1.MV[0][0];
            pCurMB->m_pBlocks[count].mv[0][1] = pMbCode->VC1.MV[0][1];
            pCurMB->m_pBlocks[count].mv_bottom[0][0] = pMbCode->VC1.MV[1][0];
            pCurMB->m_pBlocks[count].mv_bottom[0][1] = pMbCode->VC1.MV[1][1];

            // backward motion vectors. Top/Bottom Field
            pCurMB->m_pBlocks[count].mv[1][0] = pMbCode->VC1.MV[2][0];
            pCurMB->m_pBlocks[count].mv[1][1] = pMbCode->VC1.MV[2][1];
            pCurMB->m_pBlocks[count].mv_bottom[1][0] = pMbCode->VC1.MV[3][0];
            pCurMB->m_pBlocks[count].mv_bottom[1][1] = pMbCode->VC1.MV[3][1];
            }
        }
        else if (VC1_MB_4MV_FIELD_INTER == VC1_GET_MBTYPE(pCurMB->mbType)) // P frames only
        {
            //// forward motion vectors. Left Top/Bottom Field
            pCurMB->m_pBlocks[2].mv[0][0] = pCurMB->m_pBlocks[0].mv[0][0] = pMbCode->VC1.MV[0][0];
            pCurMB->m_pBlocks[2].mv[0][1] = pCurMB->m_pBlocks[0].mv[0][1] = pMbCode->VC1.MV[0][1];
            pCurMB->m_pBlocks[2].mv_bottom[0][0] = pCurMB->m_pBlocks[0].mv_bottom[0][0] = pMbCode->VC1.MV[1][0];
            pCurMB->m_pBlocks[2].mv_bottom[0][1] = pCurMB->m_pBlocks[0].mv_bottom[0][1]=  pMbCode->VC1.MV[1][1];

            // forward motion vectors. Left Top/Bottom Field
            pCurMB->m_pBlocks[3].mv[0][0] = pCurMB->m_pBlocks[1].mv[0][0] = pMbCode->VC1.MV[2][0];
            pCurMB->m_pBlocks[3].mv[0][1] = pCurMB->m_pBlocks[1].mv[0][1] = pMbCode->VC1.MV[2][1];
            pCurMB->m_pBlocks[3].mv_bottom[0][0] = pCurMB->m_pBlocks[1].mv_bottom[0][0] = pMbCode->VC1.MV[3][0];
            pCurMB->m_pBlocks[3].mv_bottom[0][1] = pCurMB->m_pBlocks[1].mv_bottom[0][1] = pMbCode->VC1.MV[3][1];

        }
        //Intra pattern
        //pCurMB->IntraFlag = pMbCode->VC1.SubMbPredMode;
        //pCurMB->SkipAndDirectFlag = pMbCode->VC1.Skip8x8Flag;
        pCurMB->SkipAndDirectFlag = CalculateSkipDirectFlag(pMbCode->VC1.MbType5Bits, pMbCode->VC1.Skip8x8Flag);
    }


    // recon functionality. For B frames - f/b direction has been processed
    void GetReconPMbNonInterlace(VC1Context*     pContext,
                                 VC1MB*          pCurMB,
                                 Ipp16s*         pResid)
    {
        IppiSize  roiSize;

        roiSize.height = VC1_PIXEL_IN_LUMA;
        roiSize.width = VC1_PIXEL_IN_LUMA;

        if (pCurMB->mbType == VC1_MB_INTRA)
        {

            ippiConvert_16s8u_C1R(pContext->m_pBlock,
                VC1_PIXEL_IN_LUMA*2,
                pCurMB->currYPlane,
                pCurMB->currYPitch,
                roiSize);

            roiSize.height = VC1_PIXEL_IN_CHROMA;
            roiSize.width = VC1_PIXEL_IN_CHROMA;

            ippiConvert_16s8u_C1R(pContext->m_pBlock+64*4,
                VC1_PIXEL_IN_CHROMA*2,
                pCurMB->currUPlane,
                pCurMB->currUPitch,
                roiSize);

            ippiConvert_16s8u_C1R(pContext->m_pBlock+64*5,
                VC1_PIXEL_IN_CHROMA*2,
                pCurMB->currVPlane,
                pCurMB->currVPitch,
                roiSize);
        }
        else if (!pCurMB->IntraFlag)
        {
            ippiMC16x16_8u_C1(pCurMB->currYPlane,
                pCurMB->currYPitch,
                pContext->m_pBlock,
                VC1_PIXEL_IN_LUMA*2,
                pCurMB->currYPlane,
                pCurMB->currYPitch, 0, 0);

            ippiMC8x8_8u_C1(pCurMB->currUPlane,
                pCurMB->currUPitch,
                &pContext->m_pBlock[4*64],
                VC1_PIXEL_IN_CHROMA*2,
                pCurMB->currUPlane,
                pCurMB->currUPitch, 0, 0);

            ippiMC8x8_8u_C1(pCurMB->currVPlane,
                pCurMB->currVPitch,
                &pContext->m_pBlock[5*64],
                VC1_PIXEL_IN_CHROMA*2,
                pCurMB->currVPlane,
                pCurMB->currVPitch, 0, 0);

        }
        else // Intra and Inter Blocks in
        {
            static const Ipp32u offset_table[] = {0,8,128,136};
            Ipp32u plane_offset = 0;

            roiSize.height = VC1_PIXEL_IN_CHROMA;
            roiSize.width = VC1_PIXEL_IN_CHROMA;

            for (Ipp32s blk_num = 0; blk_num<4 ;blk_num++)
            {
                plane_offset = (blk_num&1)*8 + (blk_num&2)*4*pCurMB->currYPitch;
                if (!(pCurMB->IntraFlag & (1<<blk_num))) // from assignpattern
                {
                    ippiMC8x8_8u_C1(pCurMB->currYPlane + plane_offset,
                                    pCurMB->currYPitch,
                                    &pContext->m_pBlock[offset_table[blk_num]],
                                    VC1_PIXEL_IN_LUMA*2,
                                    pCurMB->currYPlane + plane_offset,
                                    pCurMB->currYPitch, 0, 0);
                }
                else
                {
                    ippiConvert_16s8u_C1R(&pContext->m_pBlock[offset_table[blk_num]],
                                          VC1_PIXEL_IN_LUMA*2,
                                          pCurMB->currYPlane + plane_offset,
                                          pCurMB->currYPitch,
                                          roiSize);
                }
            }



            if (!(pCurMB->IntraFlag & (1<<4)))
                ippiMC8x8_8u_C1(pCurMB->currUPlane,
                                pCurMB->currUPitch,
                                &pContext->m_pBlock[4*64],
                                VC1_PIXEL_IN_CHROMA*2,
                                pCurMB->currUPlane,
                                pCurMB->currUPitch, 0, 0);
            else
                ippiConvert_16s8u_C1R(pContext->m_pBlock+64*4,
                                      VC1_PIXEL_IN_CHROMA*2,
                                      pCurMB->currUPlane,
                                      pCurMB->currUPitch,
                                      roiSize);

            if (!(pCurMB->IntraFlag & (1<<5)))
                ippiMC8x8_8u_C1(pCurMB->currVPlane,
                pCurMB->currVPitch,
                &pContext->m_pBlock[5*64],
                VC1_PIXEL_IN_CHROMA*2,
                pCurMB->currVPlane,
                pCurMB->currVPitch, 0, 0);
            else
                ippiConvert_16s8u_C1R(pContext->m_pBlock+64*5,
                                      VC1_PIXEL_IN_CHROMA*2,
                                      pCurMB->currVPlane,
                                      pCurMB->currVPitch,
                                      roiSize);

        }
    }
    void GetReconPMbInterlace(VC1Context*     pContext,
                              VC1MB*          pCurMB,
                              Ipp16s*         pResid)
    {
        IppiSize  roiSize;

        roiSize.height = VC1_PIXEL_IN_LUMA;
        roiSize.width = VC1_PIXEL_IN_LUMA;

        if (pCurMB->mbType == VC1_MB_INTRA)
        {
            write_Intraluma_to_interlace_frame_Adv(pContext->m_pCurrMB, pContext->m_pBlock);

            roiSize.height = VC1_PIXEL_IN_CHROMA;
            roiSize.width = VC1_PIXEL_IN_CHROMA;

            ippiConvert_16s8u_C1R(pContext->m_pBlock+64*4,
                VC1_PIXEL_IN_CHROMA*2,
                pCurMB->currUPlane,
                pCurMB->currUPitch,
                roiSize);

            ippiConvert_16s8u_C1R(pContext->m_pBlock+64*5,
                VC1_PIXEL_IN_CHROMA*2,
                pCurMB->currVPlane,
                pCurMB->currVPitch,
                roiSize);
        }
        else
        {
            write_Interluma_to_interlace_frame_MC_Adv_Copy(pContext->m_pCurrMB,
                                                           pContext->m_pBlock);

            ippiMC8x8_8u_C1(pCurMB->currUPlane,
                            pCurMB->currUPitch,
                            &pContext->m_pBlock[4*64],
                            VC1_PIXEL_IN_CHROMA*2,
                            pCurMB->currUPlane,
                            pCurMB->currUPitch, 0, 0);

            ippiMC8x8_8u_C1(pCurMB->currVPlane,
                pCurMB->currVPitch,
                &pContext->m_pBlock[5*64],
                VC1_PIXEL_IN_CHROMA*2,
                pCurMB->currVPlane,
                pCurMB->currVPitch, 0, 0);

        }
    }
    void  UnPackCodedBlockPattern(VC1MB*  pCurMB, mfxMbCode*      pMbCode)
    {
        // LUMA component packing
        for (Ipp32u blk_num = 0; blk_num < 4; blk_num++)
        {
            Ipp32u blkSPattern = (pMbCode->VC1.CodedPattern4x4Y >> (3-blk_num)*4) & 0xF;

            if (blkSPattern)
            {
                pCurMB->m_cbpBits |= 1 << (5 - blk_num);
                pCurMB->m_pBlocks[blk_num].SBlkPattern = blkSPattern;
            }
            else
                pCurMB->m_pBlocks[blk_num].SBlkPattern = 0;

        }

        // U packing
        pCurMB->m_cbpBits |= (pMbCode->VC1.CodedPattern4x4U & 0xF)?2:0;
        pCurMB->m_pBlocks[4].SBlkPattern = pMbCode->VC1.CodedPattern4x4U;
        // V packing
        pCurMB->m_cbpBits |= (pMbCode->VC1.CodedPattern4x4V & 0xF)?1:0;
        pCurMB->m_pBlocks[5].SBlkPattern = pMbCode->VC1.CodedPattern4x4V;

    }
    Ipp32u CalculateSkipDirectFlag(mfxU8 mbType, mfxU8 Skip8x8Flag)
    {
        Ipp32u SDflag = 0;
        SDflag = (mbType == MFX_MBTYPE_INTER_16X16_DIR)?1:0;
        //if ((mbType == MFX_MBTYPE_SKIP_16X16_0)||
        //    (mbType== MFX_MBTYPE_SKIP_16X16_1)||
        //    (mbType == MFX_MBTYPE_SKIP_16X16_2))
        //    SDflag |= 2;

        //possible situation for SKIP&Direct
        if (Skip8x8Flag)
            SDflag |= 2;

        return SDflag;
    }
}
#endif
