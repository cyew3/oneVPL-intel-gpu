//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#include "h263.h"
#include "h263dec.h"

/*
//  Purpose:  decode H.263 EP-Frame
*/
h263_Status h263_DecodeFrame_EP(h263_Info* pInfo, h263_EnhancedLayer *Layer, h263_Frame *r_Frame)
{
  __ALIGN16(Ipp16s, coeffMB, 64*2);
  __ALIGN16(Ipp8u, tmpMB, 64*4);
  Ipp8u           *pYc, *pCbc, *pCrc, *pYl, *pCbl, *pCrl, *pYr, *pCbr, *pCrr, *pF[6];
  Ipp32s          stepYc, stepYl, stepCbc, stepCbl, stepCrc, stepCrl;
  Ipp32s          stepYr, stepCbr, stepCrr, mbPerRow, mbPerCol, stepF[6];
  Ipp32s          dx, dy, colNum, rowNum;
  IppiRect        limitRectL;
  Ipp32s          mb_type = 0, cbpy, cbpc;
  Ipp32s          quant, quant_c;
  h263_MacroBlock *pMBinfo;
  Ipp32s          frGOB;
  Ipp32s          rt;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s gob_header_present = 1;
  Ipp32s predMode = 0;
  h263_Status status;
  Ipp32s num_rows_per_gob, num_gobs, row;
  Ipp32s i, k;
  Ipp32s cbpFlag = 0, dquantFlag = 0;
  IppMotionVector mvCbCr, mvCur;
  h263_IntraPredBlock  *b = pInfo->VideoSequence.IntraPredBuff.block;
  Ipp32s mb_not_coded;
  IppiSize roiSize;
  h263_Status sts;

  mbPerRow = pInfo->VideoSequence.VideoPicture.MacroBlockPerRow;
  mbPerCol = pInfo->VideoSequence.VideoPicture.MacroBlockPerCol;

/* l - reference frame in the lower layer */
  stepYc = Layer->c_Frame.stepY;
  stepYl = Layer->l_Frame->stepY;
  stepYr = r_Frame->stepY;
  stepCbc = Layer->c_Frame.stepCb;
  stepCbl = Layer->l_Frame->stepCb;
  stepCbr = r_Frame->stepCb;
  stepCrc = Layer->c_Frame.stepCr;
  stepCrl = Layer->l_Frame->stepCr;
  stepCrr = r_Frame->stepCr;
  pYc = Layer->c_Frame.pY;
  pCbc = Layer->c_Frame.pCb;
  pCrc = Layer->c_Frame.pCr;
  pYl = Layer->l_Frame->pY;
  pCbl = Layer->l_Frame->pCb;
  pCrl = Layer->l_Frame->pCr;
  pYr = r_Frame->pY;
  pCbr = r_Frame->pCb;
  pCrr = r_Frame->pCr;
  stepF[0] = stepF[1] = stepF[2] = stepF[3] = stepYc; stepF[4] = stepCbc; stepF[5] = stepCrc;

  // Bounding rectangles for MV limitation
  limitRectL.x = -16;
  limitRectL.y = -16;
  limitRectL.width = pInfo->VideoSequence.VideoPicture.MacroBlockPerRow * 16 + 32;
  limitRectL.height = pInfo->VideoSequence.VideoPicture.MacroBlockPerCol * 16 + 32;
  quant = pInfo->VideoSequence.VideoPicture.pic_quant;
  colNum = rowNum = 0;
  pMBinfo = pInfo->VideoSequence.MBinfo;

  quant = VPic->pic_quant;
  pInfo->VideoSequence.VideoPicture.gob_number = 0;
  num_gobs = pInfo->VideoSequence.VideoPicture.num_gobs_in_pic;
  num_rows_per_gob = pInfo->VideoSequence.VideoPicture.RowsPerGOB;

  rt = VPic->rtype;

  dy = 0;
  frGOB = 0;
  if (VPic->oppmodes.deblockFilt)
    ippsZero_8u(pInfo->VideoSequence.GOBboundary, mbPerCol);

  for (i = 0; i < num_gobs; i++) {
    VPic->gob_number = i;

    if (i) {
ErrRet_1:
      gob_header_present = h263_ParseGOBHeader(pInfo);
      if (gob_header_present < 0) {
        h263_Error("Error: Invalid GOB header");
        goto Err_1;
      }

      if (VPic->gob_number > 30)
        return H263_STATUS_OK; /* EOS or EOSBS */

      if (gob_header_present) {
        Ipp32s nummbs2copy = (VPic->gob_number * num_rows_per_gob - rowNum) * mbPerRow - colNum;
        h263_CopyMacroBlocks(Layer->l_Frame, &Layer->c_Frame, mbPerRow, rowNum, colNum, nummbs2copy);
        rowNum = num_rows_per_gob * VPic->gob_number;
        colNum = 0;
        pYc = Layer->c_Frame.pY + (stepYc << 4) * rowNum;
        pCbc = Layer->c_Frame.pCb + (stepCbc << 3) * rowNum;
        pCrc = Layer->c_Frame.pCr + (stepCrc << 3) * rowNum;
        pYr = r_Frame->pY + (stepYr << 4) * rowNum;
        pCbr = r_Frame->pCb + (stepCbr << 3) * rowNum;
        pCrr = r_Frame->pCr + (stepCrr << 3) * rowNum;
        pYl = Layer->l_Frame->pY + (stepYl << 4) * rowNum;
        pCbl = Layer->l_Frame->pCb + (stepCbl << 3) * rowNum;
        pCrl = Layer->l_Frame->pCr + (stepCrl << 3) * rowNum;
        pMBinfo = pInfo->VideoSequence.MBinfo + rowNum * mbPerRow;
        if (VPic->gob_number - i > 0)
          ippsSet_8u(127, pInfo->VideoSequence.GOBboundary + i*num_rows_per_gob, (VPic->gob_number - i) * num_rows_per_gob);
        i = VPic->gob_number;
      }
      if (gob_header_present) {
        frGOB = i*num_rows_per_gob;
        pInfo->VideoSequence.GOBboundary[frGOB] = 1;
      }
    }

    quant = quant_c = VPic->pic_quant;
    if (VPic->oppmodes.modQuant)
      quant_c = h263_quant_c[quant];

    h263_CLIPR(num_rows_per_gob, (mbPerCol - i*num_rows_per_gob));

    if (gob_header_present && VPic->oppmodes.advIntra) {
      for (k = 0; k <= mbPerRow; k++) {
        b[k*6+0].dct_dc = b[k*6+1].dct_dc = b[k*6+2].dct_dc = b[k*6+3].dct_dc = b[k*6+4].dct_dc = b[k*6+5].dct_dc = -1;
      }
    }

    for (row = 0; row < num_rows_per_gob; row++) {
      rowNum = i*num_rows_per_gob + row;
      dy = rowNum * 16;
      for (colNum = 0; colNum < mbPerRow; colNum++) {
        dx = colNum * 16;
        do {
          mb_not_coded = h263_GetBit(pInfo);
          if (mb_not_coded)
            break;
          if (h263_DecodeMBTYPE_EP(pInfo, &mb_type, &cbpFlag, &dquantFlag) != H263_STATUS_OK)
            goto Err_1;
        } while (mb_type == IPPVC_MB_STUFFING);

        if (mb_not_coded) {
          ippiCopy16x16_8u_C1R(pYr, stepYr, pYc, stepYc);
          ippiCopy8x8_8u_C1R(pCbr, stepCbr, pCbc, stepCbc);
          ippiCopy8x8_8u_C1R(pCrr, stepCrr, pCrc, stepCrc);

          mb_type = pMBinfo->type = IPPVC_MB_STUFFING; /* to indicate mb_not_coded */

          pMBinfo->mv[0].dx = pMBinfo->mv[0].dy = 0;
          if (VPic->oppmodes.advIntra) {
            for (k = 0; k < 6; k++)
              b[(colNum + 1)*6 + k].dct_dc = -1;
          }
          h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB_NOTCODED);

        } else if (mb_type == IPPVC_MBTYPE_INTRA || mb_type == IPPVC_MBTYPE_INTRA_Q) {
          pF[0] = pYc; pF[1] = pYc + 8; pF[2] = pYc + 8 * stepYc; pF[3] = pYc + 8 * stepYc + 8; pF[4] = pCbc; pF[5] = pCrc;

          if (VPic->oppmodes.advIntra)
            h263_AdvI_PredMode(pInfo, predMode);

          h263_Decode_CBPC_B(pInfo, cbpc);

          if (h263_DecodeCBPY_I(pInfo, &cbpy) != H263_STATUS_OK)
            goto Err_1;

          if (mb_type == IPPVC_MBTYPE_INTRA_Q) {
            if (!VPic->oppmodes.modQuant) {
              h263_UpdateQuant(pInfo, VPic->pic_quant);
              quant_c = VPic->pic_quant;
            } else {
              quant_c = h263_UpdateQuant_Mod(pInfo);
            }
            quant = VPic->pic_quant;
          }

          pMBinfo->mv[0].dx = pMBinfo->mv[0].dy = 0;

          if (!VPic->oppmodes.advIntra)
            status = h263_DecodeMacroBlockIntra(pInfo, (cbpy << 2) + cbpc, quant, quant_c, pF, stepF);
          else
            status = h263_DecodeMacroBlockIntra_AdvI(pInfo, colNum, (cbpy << 2) + cbpc, quant, quant_c, predMode, pF, stepF);
          if (status != H263_STATUS_OK) {
            h263_Error("Error when decoding coefficients of an Intra block in EP-frame");
            return H263_STATUS_ERROR;
          }

        } else {

          if (VPic->oppmodes.advIntra) {
            for (k = 0; k < 6; k++)
              b[(colNum + 1)*6 + k].dct_dc = -1;
          }

          if (cbpFlag) {
            h263_Decode_CBPC_B(pInfo, cbpc);

            if ((VPic->oppmodes.altInterVLC && cbpc == 3) || mb_type == IPPVC_MBTYPE_BACKWARD || mb_type == IPPVC_MBTYPE_INTERPOLATE) {
              if (h263_DecodeCBPY_I(pInfo, &cbpy) != H263_STATUS_OK)
                goto Err_1;
            } else {
              if (h263_DecodeCBPY_B(pInfo, &cbpy, mb_type) != H263_STATUS_OK)
                goto Err_1;
            }
          } else
            cbpc = cbpy = 0;

          if (dquantFlag) {
            if (!VPic->oppmodes.modQuant) {
              h263_UpdateQuant(pInfo, VPic->pic_quant);
              quant_c = VPic->pic_quant;
            } else {
              quant_c = h263_UpdateQuant_Mod(pInfo);
            }
            quant = VPic->pic_quant;
          }

          if (mb_type == IPPVC_MBTYPE_BACKWARD) {
            pMBinfo->mv[0].dx = pMBinfo->mv[0].dy = 0;
            if (cbpy) {
              h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 8, pYl, stepYl, pYc, stepYc, coeffMB);
              h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 4, pYl+8, stepYl, pYc+8, stepYc, coeffMB);
              h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 2, pYl+8*stepYl, stepYl, pYc+8*stepYc, stepYc, coeffMB);
              h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 1, pYl+8+8*stepYl, stepYl, pYc+8+8*stepYc, stepYc, coeffMB);
            } else {
              ippiCopy16x16_8u_C1R(pYl, stepYl, pYc, stepYc);
              h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
              h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
              h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
              h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
            }
            h263_DecodeMCBlockInter_B(pInfo, quant_c, cbpc & 2, pCbl, stepCbl, pCbc, stepCbc, coeffMB);
            h263_DecodeMCBlockInter_B(pInfo, quant_c, cbpc & 1, pCrl, stepCrl, pCrc, stepCrc, coeffMB);
          } else {
            if (h263_PredictDecodeMV(pInfo, pMBinfo, frGOB, rowNum, colNum, 0) != H263_STATUS_OK) {
              h263_Error("Error when decoding motion vector");
              goto Err_1;
            }
            h263_LimitMV(&pMBinfo->mv[0], &mvCur, &limitRectL, dx, dy, 16);
            h263_ComputeChromaMV(&mvCur, &mvCbCr);

            if (mb_type == IPPVC_MBTYPE_FORWARD) {
              if (cbpy) {
                h263_DecodeMCBlockInter(pInfo, quant, cbpy & 8, pYr, pYc, stepYc, coeffMB, &mvCur, rt);
                h263_DecodeMCBlockInter(pInfo, quant, cbpy & 4, pYr+8, pYc+8, stepYc, coeffMB, &mvCur, rt);
                h263_DecodeMCBlockInter(pInfo, quant, cbpy & 2, pYr+stepYr*8, pYc+stepYc*8, stepYc, coeffMB, &mvCur, rt);
                h263_DecodeMCBlockInter(pInfo, quant, cbpy & 1, pYr+8+stepYr*8, pYc+8+stepYc*8, stepYc, coeffMB, &mvCur, rt);
              } else {
                h263_Copy16x16HP_8u(pYr, stepYr, pYc, stepYc, &mvCur, rt);
                h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
              }
              h263_DecodeMCBlockInter(pInfo, quant_c, cbpc & 2, pCbr, pCbc, stepCbc, coeffMB, &mvCbCr, rt);
              h263_DecodeMCBlockInter(pInfo, quant_c, cbpc & 1, pCrr, pCrc, stepCrc, coeffMB, &mvCbCr, rt);

            } else { /* IPPVC_MBTYPE_INTERPOLATE */
              roiSize.width = roiSize.height = 16;
              if (cbpy) {
                h263_Copy16x16HP_8u(pYr, stepYr, tmpMB, 16, &mvCur, rt);
                ippiAddBackPredPB_H263_8u_C1R(pYl, stepYl, roiSize, tmpMB, 16, 0);
                h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 8, tmpMB, 16, pYc, stepYc, coeffMB);
                h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 4, tmpMB+8, 16, pYc+8, stepYc, coeffMB);
                h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 2, tmpMB+8*16, 16, pYc+8*stepYc, stepYc, coeffMB);
                h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 1, tmpMB+8+8*16, 16, pYc+8+8*stepYc, stepYc, coeffMB);
              } else {
                h263_Copy16x16HP_8u(pYr, stepYr, pYc, stepYc, &mvCur, rt);
                ippiAddBackPredPB_H263_8u_C1R(pYl, stepYl, roiSize, pYc, stepYc, 0);
                h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
              }

              roiSize.width = roiSize.height = 8;
              h263_Copy8x8HP_8u(pCbr, stepCbr, tmpMB, 8, &mvCbCr, rt);
              ippiAddBackPredPB_H263_8u_C1R(pCbl, stepCbl, roiSize, tmpMB, 8, 0);
              h263_DecodeMCBlockInter_B(pInfo, quant_c, cbpc & 2, tmpMB, 8, pCbc, stepCbc, coeffMB);

              h263_Copy8x8HP_8u(pCrr, stepCrr, tmpMB, 8, &mvCbCr, rt);
              ippiAddBackPredPB_H263_8u_C1R(pCrl, stepCrl, roiSize, tmpMB, 8, 0);
              h263_DecodeMCBlockInter_B(pInfo, quant_c, cbpc & 1, tmpMB, 8, pCrc, stepCrc, coeffMB);
            }
          }
        }

        if (VPic->oppmodes.deblockFilt && !mb_not_coded) {
          H263_MB_INTERNAL_HOR_DEBLOCKING_LUM(pYc, stepYc, quant);
        }

        pYc += 16; pCrc += 8; pCbc += 8;
        pYr += 16; pCrr += 8; pCbr += 8;
        pYl += 16; pCrl += 8; pCbl += 8;
        pMBinfo++;
      }
      pYc += 2 * H263_NUM_EXT_MB * 16 + (stepYc << 4) - stepYc;
      pCbc += 2 * H263_NUM_EXT_MB * 8 + (stepCbc << 3) - stepCbc;
      pCrc += 2 * H263_NUM_EXT_MB * 8 + (stepCrc << 3) - stepCrc;
      pYr += 2 * H263_NUM_EXT_MB * 16 + (stepYr << 4) - stepYr;
      pCbr += 2 * H263_NUM_EXT_MB * 8 + (stepCbr << 3) - stepCbr;
      pCrr += 2 * H263_NUM_EXT_MB * 8 + (stepCrr << 3) - stepCrr;
      pYl += 2 * H263_NUM_EXT_MB * 16 + (stepYl << 4) - stepYl;
      pCbl += 2 * H263_NUM_EXT_MB * 8 + (stepCbl << 3) - stepCbl;
      pCrl += 2 * H263_NUM_EXT_MB * 8 + (stepCrl << 3) - stepCrl;
    }
  }

  h263_AlignBits(pInfo);

  /* Deblocking filtering */
  if (VPic->oppmodes.deblockFilt)
    h263_DeblockingFilter_P(pInfo, &Layer->c_Frame);

  return H263_STATUS_OK;

Err_1:
  sts = H263_STATUS_ERROR;
  if (pInfo->stopOnErr)
    return sts;
  if (!h263_SeekGOBStartCodePtr(pInfo)) {
    h263_CopyMacroBlocks(Layer->l_Frame, &Layer->c_Frame, mbPerRow, rowNum, colNum,  (mbPerCol - rowNum) * mbPerRow - colNum);
    return sts;
  }
  goto ErrRet_1;
}
