/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2008 Intel Corporation. All Rights Reserved.
//          MFX VC-1 BSD utils
//
*/
#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_BSD)

#include "mfx_vc1_bsd_utils.h"
#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_dec_exception.h"
#include "mfx_vc1_bsd_threading.h"

using namespace UMC;
using namespace UMC::VC1Exceptions;
//using namespace MfxVC1BSDPacking;

mfxStatus MfxVC1BSDPacking::FillParamsForOneMB(VC1Context*     pContext,
                                               VC1MB*          pCurMB,
                                               VC1SingletonMB* pSingleMB,
                                               mfxMbCode*      pMbCode)

{
    MFX_CHECK_NULL_PTR1(pCurMB);
    MFX_CHECK_NULL_PTR1(pMbCode);

    Ipp32u count = 0;

    //TBD - need to syncronize header to last document

    //pMbCode->VC1.Skip8x8Flag = 0; - TBD

    if ((pSingleMB->slice_currMBYpos == 0)&&
        (pSingleMB->m_currMBXpos == 0))
        pMbCode->VC1.FirstMbFlag = 1;

    pMbCode->VC1.MbType = ConvertMBTypeTo5bitMXF(pContext,pCurMB);
    pMbCode->VC1.IntraMbFlag = (pCurMB->mbType == VC1_MB_INTRA)?1:0;
    pMbCode->VC1.FieldMbFlag = VC1_IS_MVFIELD(pCurMB->mbType);
    pMbCode->VC1.TransformFlag = pCurMB->FIELDTX;
    pMbCode->VC1.Skip8x8Flag = (pCurMB->SkipAndDirectFlag & 2) ? (0xf):(0);
    pMbCode->VC1.SubMbPredMode = 0;
    pMbCode->VC1.SubMbShape = 0;
    // for P frames
    //if (!(pCurMB->SkipAndDirectFlag & 2))
    {
        for (; count < 4; count++)
        {
            switch(pCurMB->m_pBlocks[count].blkType)
            {
            case VC1_BLK_INTRA_TOP:
            case VC1_BLK_INTRA_LEFT:
            case VC1_BLK_INTRA:
                // nothing to do
                break;
            case VC1_BLK_INTER:
            case VC1_BLK_INTER8X8:
                //if (pContext->m_picLayerHeader->PTYPE == VC1_P_FRAME)
                    pMbCode->VC1.SubMbPredMode |= 3 << (3 - count)*2;
                break;
            case VC1_BLK_INTER8X4:
                pMbCode->VC1.SubMbShape |= 1 << (3 - count)*2;
                break;
            case VC1_BLK_INTER4X8:
                pMbCode->VC1.SubMbShape |= 2 << (3 - count)*2;
                break;
             case VC1_BLK_INTER4X4:
                pMbCode->VC1.SubMbShape |= 3 << (3 - count)*2;
                break;
             default:
                 break;
            }
        }
    }
    //Chroma U, V
    {
        switch(pCurMB->m_pBlocks[4].blkType)
        {
        case VC1_BLK_INTRA_TOP:
        case VC1_BLK_INTRA_LEFT:
        case VC1_BLK_INTRA:
        case VC1_BLK_INTER:
        case VC1_BLK_INTER8X8:
            pMbCode->VC1.SubMbShapeU = 0;
            break;
        case VC1_BLK_INTER8X4:
            pMbCode->VC1.SubMbShapeU = 1;
            break;
        case VC1_BLK_INTER4X8:
            pMbCode->VC1.SubMbShapeU = 2;
              break;
        case VC1_BLK_INTER4X4:
            pMbCode->VC1.SubMbShapeU = 3;
            break;
        default:
            break;
        }
        switch(pCurMB->m_pBlocks[5].blkType)
        {
        case VC1_BLK_INTRA_TOP:
        case VC1_BLK_INTRA_LEFT:
        case VC1_BLK_INTRA:
        case VC1_BLK_INTER:
        case VC1_BLK_INTER8X8:
            pMbCode->VC1.SubMbShapeV = 0;
            break;
        case VC1_BLK_INTER8X4:
            pMbCode->VC1.SubMbShapeV = 1;
            break;
        case VC1_BLK_INTER4X8:
            pMbCode->VC1.SubMbShapeV = 2;
            break;
        case VC1_BLK_INTER4X4:
            pMbCode->VC1.SubMbShapeV = 3;
            break;
        default:
            break;
        }
    }
    //pMbCode->VC1.SubMbPredMode = pCurMB->IntraFlag;

    // Should be add
    //if (pCurMB->SkipAndDirectFlag & 2) // skipped MB
    //    pMbCode->VC1.SubMbPredMode
    //TBD
    //MFX_SUBTYPE_SKIP_16X8_TOP 30h
    //MFX_SUBTYPE_SKIP_16X8_BTM C0h

    pMbCode->VC1.MbXcnt = (mfxU8)pSingleMB->m_currMBXpos;
    pMbCode->VC1.MbYcnt = (mfxU8)pSingleMB->m_currMBYpos;

    // May be determinate depend on current MB?
    //?????
    //pMbCode->VC1.MvUnpackedFlag = MFX_UNPACK_8X8_B;

    //offset for residual
    pMbCode->VC1.MbDataOffsetUnit = 8;
    pMbCode->VC1.MbDataOffset = (mfxU16)((sizeof mfxU16)*(pSingleMB->m_currMBXpos +
                                                      pSingleMB->m_currMBYpos *
                                                      pContext->m_seqLayerHeader->widthMB)*8*8*6  >> pMbCode->VC1.MbDataOffsetUnit); //- offset in double words

    // no need in this buffer
    //pMbCode->VC1.MbUserOffset32b = 0;
    //pMbCode->VC1.MvDataOffset32b = 0;

    //????
    //pMbCode->VC1.ExtResidDiffFlag = 1; //BSD prepares residual data for DEC

    PackCodedBlockPattern(pCurMB,pMbCode);

    pMbCode->VC1.OverlapFilter = pCurMB->Overlap;


    //possible need to fill picture RefPicSelect. It is complex task for VC-1. DEC ?

    //pack MV
    if (!VC1_IS_MVFIELD(pCurMB->mbType))
    {
        // forward motion vectors
        pMbCode->VC1.MV[0][0] = pCurMB->m_pBlocks[0].mv[0][0];
        pMbCode->VC1.MV[0][1] = pCurMB->m_pBlocks[0].mv[0][1];
        pMbCode->VC1.MV[1][0] = pCurMB->m_pBlocks[1].mv[0][0];
        pMbCode->VC1.MV[1][1] = pCurMB->m_pBlocks[1].mv[0][1];
        pMbCode->VC1.MV[2][0] = pCurMB->m_pBlocks[2].mv[0][0];
        pMbCode->VC1.MV[2][1] = pCurMB->m_pBlocks[2].mv[0][1];
        pMbCode->VC1.MV[3][0] = pCurMB->m_pBlocks[3].mv[0][0];
        pMbCode->VC1.MV[3][1] = pCurMB->m_pBlocks[3].mv[0][1];

        // backward motion vectors
        pMbCode->VC1.MV[4][0] = pCurMB->m_pBlocks[0].mv[1][0];
        pMbCode->VC1.MV[4][1] = pCurMB->m_pBlocks[0].mv[1][1];
        pMbCode->VC1.MV[5][0] = pCurMB->m_pBlocks[1].mv[1][0];
        pMbCode->VC1.MV[5][1] = pCurMB->m_pBlocks[1].mv[1][1];
        pMbCode->VC1.MV[6][0] = pCurMB->m_pBlocks[2].mv[1][0];
        pMbCode->VC1.MV[6][1] = pCurMB->m_pBlocks[2].mv[1][1];
        pMbCode->VC1.MV[7][0] = pCurMB->m_pBlocks[3].mv[1][0];
        pMbCode->VC1.MV[7][1] = pCurMB->m_pBlocks[3].mv[1][1];
    }
    else if (VC1_MB_2MV_INTER == VC1_GET_MBTYPE(pCurMB->mbType))
    {
        // forward motion vectors. Top/Bottom Field
        pMbCode->VC1.MV[0][0] = pCurMB->m_pBlocks[0].mv[0][0];
        pMbCode->VC1.MV[0][1] = pCurMB->m_pBlocks[0].mv[0][1];
        pMbCode->VC1.MV[1][0] = pCurMB->m_pBlocks[0].mv_bottom[0][0];
        pMbCode->VC1.MV[1][1] = pCurMB->m_pBlocks[0].mv_bottom[0][1];
        // backward motion vectors. Top/Bottom Field
        pMbCode->VC1.MV[2][0] = pCurMB->m_pBlocks[0].mv[1][0];
        pMbCode->VC1.MV[2][1] = pCurMB->m_pBlocks[0].mv[1][1];
        pMbCode->VC1.MV[3][0] = pCurMB->m_pBlocks[0].mv_bottom[1][0];
        pMbCode->VC1.MV[3][1] = pCurMB->m_pBlocks[0].mv_bottom[1][1];
    }
    else if (VC1_MB_4MV_FIELD_INTER == VC1_GET_MBTYPE(pCurMB->mbType)) // P frames only
    {
        // forward motion vectors. Left Top/Bottom Field
        pMbCode->VC1.MV[0][0] = pCurMB->m_pBlocks[0].mv[0][0];
        pMbCode->VC1.MV[0][1] = pCurMB->m_pBlocks[0].mv[0][1];
        pMbCode->VC1.MV[1][0] = pCurMB->m_pBlocks[2].mv_bottom[0][0];
        pMbCode->VC1.MV[1][1] = pCurMB->m_pBlocks[2].mv_bottom[0][1];

        // forward motion vectors. Left Top/Bottom Field
        pMbCode->VC1.MV[2][0] = pCurMB->m_pBlocks[1].mv[0][0];
        pMbCode->VC1.MV[2][1] = pCurMB->m_pBlocks[1].mv[0][1];
        pMbCode->VC1.MV[3][0] = pCurMB->m_pBlocks[3].mv_bottom[0][0];
        pMbCode->VC1.MV[3][1] = pCurMB->m_pBlocks[3].mv_bottom[0][1];
    }

    //quantization parameters. should be defined
    pMbCode->VC1.QpScaleCode = (mfxU8)pContext->CurrDC->DoubleQuant;
    pMbCode->VC1.QpScaleType = (mfxU8)pContext->CurrDC->DCStepSize;
    // Non zero coeffs flag
    //v.092 no consist NzCoefCount
    //for (count = 0; count < 5; count++)
    //    pMbCode->VC1.NzCoefCount[count] = Ipp8u(pCurMB->m_cbpBits & (1<<(5-count)));

    return MFX_ERR_NONE;
}
mfxU8 MfxVC1BSDPacking::ConvertMBTypeTo5bitMXF(VC1Context* pContext,
                                               VC1MB*     pCurMB)
{
    if (VC1_GET_MBTYPE(pCurMB->mbType) == VC1_MB_4MV_INTER)
    {
        return MFX_MBTYPE_INTER_8X8_0;
    }
    if ((VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)||
        (VC1_Progressive  == pContext->m_picLayerHeader->FCM))
    {
        if (VC1_IS_NOT_PRED(pContext->m_picLayerHeader->PTYPE))
        {
            return MFX_MBTYPE_INTRA_FIELD_VC1;
        }
        else if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if (pCurMB->SkipAndDirectFlag & 2) // Skipped
                return MFX_MBTYPE_SKIP_16X16_0;
            else
                return MFX_MBTYPE_INTER_16X16_0;
        }
        else // B frames
        {
            switch(VC1_GET_PREDICT(pCurMB->mbType))
            {
            case VC1_MB_DIRECT:
                return MFX_MBTYPE_INTER_16X16_DIR;
                break;
            case VC1_MB_FORWARD:
                if (pCurMB->SkipAndDirectFlag & 2)
                    return MFX_MBTYPE_SKIP_16X16_0;
                else
                    return MFX_MBTYPE_INTER_16X16_0;
            case VC1_MB_BACKWARD:
                if (pCurMB->SkipAndDirectFlag & 2)
                    return MFX_MBTYPE_SKIP_16X16_1;
                else
                    return MFX_MBTYPE_INTER_16X16_1;
            case VC1_MB_INTERP:
                 if (pCurMB->SkipAndDirectFlag & 2)
                    return MFX_MBTYPE_SKIP_16X16_2;
                else
                    return MFX_MBTYPE_INTER_16X16_2;
            default:
                break;
            }
        }
    }
    else // interlace frame
    {
        if (VC1_IS_NOT_PRED(pContext->m_picLayerHeader->PTYPE))
            return MFX_MBTYPE_INTRA_FIELD_VC1;
        else if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if (pCurMB->SkipAndDirectFlag & 2) // Skipped
                return MFX_MBTYPE_SKIP_16X16_0;
            switch (VC1_GET_MBTYPE(pCurMB->mbType))
            {
                case VC1_MB_1MV_INTER:
                    return MFX_MBTYPE_INTER_16X16_0;
                case VC1_MB_2MV_INTER:
                    return MFX_MBTYPE_INTER_FIELD_16X8_00;
                case VC1_MB_4MV_INTER://???????????????
                    return MFX_MBTYPE_INTER_8X8_0;
                case VC1_MB_4MV_FIELD_INTER:
                    return MFX_MBTYPE_INTER_FIELD_8X8_00;
                default:
                    break;
            }
        }
        else if (VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if (VC1_IS_NOT_PRED(pContext->m_picLayerHeader->PTYPE))
                return MFX_MBTYPE_INTRA_VC1;

            switch(VC1_GET_PREDICT(pCurMB->mbType))
            {
            case VC1_MB_DIRECT:
                return MFX_MBTYPE_INTER_16X16_DIR;
                break;
            case VC1_MB_FORWARD:
                if (VC1_MB_2MV_INTER == VC1_GET_MBTYPE(pCurMB->mbType))
                    return MFX_MBTYPE_INTER_16X8_00;
                else
                    return MFX_MBTYPE_INTER_16X16_0; // 1-MV
            case VC1_MB_BACKWARD:
                if (VC1_MB_2MV_INTER == VC1_GET_MBTYPE(pCurMB->mbType))
                    return MFX_MBTYPE_INTER_16X8_01;
                else
                    return MFX_MBTYPE_INTER_16X16_1; // 1-MV
            case VC1_MB_INTERP:
                if (VC1_MB_2MV_INTER == VC1_GET_MBTYPE(pCurMB->mbType))
                    return MFX_MBTYPE_INTER_16X8_11;
                else
                    return MFX_MBTYPE_INTER_16X16_2; // 1-MV
            default:
                break;
            }
            return 0;
            //TBD
        }
    }
    return 0;
}




void  MfxVC1BSDPacking::PackCodedBlockPattern(VC1MB*     pCurMB, mfxMbCode*      pMbCode)
{
    // LUMA component packing
    pMbCode->VC1.CodedPattern4x4Y = 0;
    for (Ipp32u blk_num = 0; blk_num < 4; blk_num++)
    {
        if (pCurMB->m_cbpBits & (1<<(5-blk_num)))
        {
            if ((!pCurMB->IntraFlag)&&
                (!pCurMB->m_pBlocks[blk_num].SBlkPattern))
                pMbCode->VC1.CodedPattern4x4Y |= 0xF << (3 - blk_num)*4;
            else
                pMbCode->VC1.CodedPattern4x4Y |= pCurMB->m_pBlocks[blk_num].SBlkPattern << (3 - blk_num)*4;
        }
    }

    // U packing
    pMbCode->VC1.CodedPattern4x4U = (mfxU16)((pCurMB->m_cbpBits & 2)?(pCurMB->m_pBlocks[4].SBlkPattern):0);
    // V packing
    pMbCode->VC1.CodedPattern4x4V = (mfxU16)((pCurMB->m_cbpBits & 1)?(pCurMB->m_pBlocks[5].SBlkPattern):0);

}
#endif
