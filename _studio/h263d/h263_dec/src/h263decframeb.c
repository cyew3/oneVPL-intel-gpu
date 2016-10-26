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

static h263_Status h263_PredictDecodeMV_B(h263_Info *pInfo, IppMotionVector *mvPred, IppMotionVector *mvCurr,
                                          Ipp32s frGOB, Ipp32s y, Ipp32s x)
{
  IppMotionVector *mvLeft, *mvTop, *mvRight;
  h263_Status status;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s  mbInRow = VPic->MacroBlockPerRow;

  mvLeft  = &mvPred[x-1];
  mvTop   = &mvPred[x];
  mvRight = &mvPred[x+1];
  if (y == frGOB && x == 0) {
    mvCurr->dx = mvCurr->dy = 0;
  } else if (x == 0) {
    mvCurr->dx = h263_Median(0, mvTop->dx, mvRight->dx);
    mvCurr->dy = h263_Median(0, mvTop->dy, mvRight->dy);
  } else if (y == frGOB) {
    *mvCurr = *mvLeft;
  } else if (x == mbInRow - 1) {
    mvCurr->dx = h263_Median(0, mvLeft->dx, mvTop->dx);
    mvCurr->dy = h263_Median(0, mvLeft->dy, mvTop->dy);
  } else {
    mvCurr->dx = h263_Median(mvLeft->dx, mvTop->dx, mvRight->dx);
    mvCurr->dy = h263_Median(mvLeft->dy, mvTop->dy, mvRight->dy);
  }

  if (!VPic->oppmodes.UMV && !VPic->modes.UMV) {
    status = h263_DecodeMV(pInfo, mvCurr, -32, 31);
  } else if (!VPic->oppmodes.UMV) {
    status = h263_DecodeMV(pInfo, mvCurr, -63, 63);
  } else {
    Ipp32s mvdx, mvdy;
    status = h263_DecodeMVD_umvplus(pInfo, &mvdx, &mvdy);
    mvCurr->dx = mvCurr->dx + (Ipp16s)mvdx;
    mvCurr->dy = mvCurr->dy + (Ipp16s)mvdy;
  }

  *mvTop = *mvCurr; /* updating the predictor */

  return status;
}

static void h263_CalculateMV_Direct(IppMotionVector *mvC, IppMotionVector *mvF, IppMotionVector *mvB,
                                    IppMotionVector *mvCbCrF, IppMotionVector *mvCbCrB,
                                    Ipp32s trd, Ipp32s trb, Ipp32s mb_typeC, Ipp32s dx, Ipp32s dy,
                                    IppiRect *limitRectL, IppiRect *limitRectC)
{
  Ipp32s i;
  if (trd == 0) {
    if (trb)
      trd = trb;
    else
      trb = trd = 1;
  }
  if (mb_typeC == IPPVC_MBTYPE_INTER4V || mb_typeC == IPPVC_MBTYPE_INTER4V_Q) {
    for (i = 0; i < 4; i++) {
      mvF[i].dx = (Ipp16s)(mvC[i].dx * trb / trd);
      mvF[i].dy = (Ipp16s)(mvC[i].dy * trb / trd);
      mvB[i].dx = (Ipp16s)((trb - trd) * mvC[i].dx / trd);
      mvB[i].dy = (Ipp16s)((trb - trd) * mvC[i].dy / trd);
    }
    h263_Limit4MV(mvF, mvF, limitRectL, dx, dy, 8);
    h263_Limit4MV(mvB, mvB, limitRectL, dx, dy, 8);
    h263_ComputeChroma4MV(mvF, mvCbCrF);
    h263_ComputeChroma4MV(mvB, mvCbCrB);
    h263_LimitMV(mvCbCrF, mvCbCrF, limitRectC, dx >> 1, dy >> 1, 8);
    h263_LimitMV(mvCbCrB, mvCbCrB, limitRectC, dx >> 1, dy >> 1, 8);
  } else {
    mvF[0].dx = (Ipp16s)(mvC[0].dx * trb / trd);
    mvF[0].dy = (Ipp16s)(mvC[0].dy * trb / trd);
    mvB[0].dx = (Ipp16s)((trb - trd) * mvC[0].dx / trd);
    mvB[0].dy = (Ipp16s)((trb - trd) * mvC[0].dy / trd);
    h263_LimitMV(mvF, mvF, limitRectL, dx, dy, 16);
    h263_LimitMV(mvB, mvB, limitRectL, dx, dy, 16);
    h263_ComputeChromaMV(mvF, mvCbCrF);
    h263_ComputeChromaMV(mvB, mvCbCrB);
  }
}

/*
//  Purpose:  decode H.263 B-Frame
*/
h263_Status h263_DecodeFrame_B(h263_Info* pInfo, h263_EnhancedLayer *Layer, h263_Frame *rFrame)
{
  __ALIGN16(Ipp16s, coeffMB, 64*2);
  __ALIGN16(Ipp8u, tmpMB, 64*4*2);
  Ipp8u           *pYc, *pCbc, *pCrc, *pYn, *pCbn, *pCrn, *pYr, *pCbr, *pCrr, *pF[6];
  Ipp32s          stepYc, stepYn, stepCbc, stepCbn, stepCrc, stepCrn;
  Ipp32s          stepYr, stepCbr, stepCrr, mbPerRow, mbPerCol, stepF[6];
  Ipp32s          dx, dy, TRB, TRD, colNum, rowNum;
  IppiRect        limitRectL, limitRectC;
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
  IppMotionVector mvCbCrF = {0,0}, mvCbCrB = {0,0}, mvForw[4], mvBack[4];
  IppMotionVector *predMVF, *predMVB;
  h263_IntraPredBlock  *b = pInfo->VideoSequence.IntraPredBuff.block;
  Ipp32s mb_not_coded;
  IppiSize roiSize;
  h263_Status sts;

  mbPerRow = pInfo->VideoSequence.VideoPicture.MacroBlockPerRow;
  mbPerCol = pInfo->VideoSequence.VideoPicture.MacroBlockPerCol;
  predMVF = &pInfo->VideoSequence.Bmv[0];
  predMVB = &pInfo->VideoSequence.Bmv[mbPerRow];

  /* B-frame in n, next (in displaying order) P - c, previous - r */
  if (VPic->enh_layer_num == 1) {
    stepYn = pInfo->VideoSequence.nFrame.stepY;
    stepCbn = pInfo->VideoSequence.nFrame.stepCb;
    stepCrn = pInfo->VideoSequence.nFrame.stepCr;
    pYn = pInfo->VideoSequence.nFrame.pY;
    pCbn = pInfo->VideoSequence.nFrame.pCb;
    pCrn = pInfo->VideoSequence.nFrame.pCr;
  } else {
    stepYn = Layer->n_Frame.stepY;
    stepCbn = Layer->n_Frame.stepCb;
    stepCrn = Layer->n_Frame.stepCr;
    pYn = Layer->n_Frame.pY;
    pCbn = Layer->n_Frame.pCb;
    pCrn = Layer->n_Frame.pCr;
  }

  if (VPic->ref_layer_num == 1) {
    stepYc = pInfo->VideoSequence.cFrame.stepY;
    stepCbc = pInfo->VideoSequence.cFrame.stepCb;
    stepCrc = pInfo->VideoSequence.cFrame.stepCr;
    pYc = pInfo->VideoSequence.cFrame.pY;
    pCbc = pInfo->VideoSequence.cFrame.pCb;
    pCrc = pInfo->VideoSequence.cFrame.pCr;
  } else {
    h263_EnhancedLayer *refLayer = pInfo->VideoSequence.enhLayers[VPic->ref_layer_num - 2];
    stepYc = refLayer->c_Frame.stepY;
    stepCbc = refLayer->c_Frame.stepCb;
    stepCrc = refLayer->c_Frame.stepCr;
    pYc = refLayer->c_Frame.pY;
    pCbc = refLayer->c_Frame.pCb;
    pCrc = refLayer->c_Frame.pCr;
  }

  stepYr = rFrame->stepY;
  stepCbr = rFrame->stepCb;
  stepCrr = rFrame->stepCr;
  pYr = rFrame->pY;
  pCbr = rFrame->pCb;
  pCrr = rFrame->pCr;


  stepF[0] = stepF[1] = stepF[2] = stepF[3] = stepYc; stepF[4] = stepCbc; stepF[5] = stepCrc;

  // Bounding rectangles for MV limitation
  limitRectL.x = -16;
  limitRectL.y = -16;
  limitRectL.width = pInfo->VideoSequence.VideoPicture.MacroBlockPerRow * 16 + 32;
  limitRectL.height = pInfo->VideoSequence.VideoPicture.MacroBlockPerCol * 16 + 32;
  limitRectC.x = -8;
  limitRectC.y = -8;
  limitRectC.width = pInfo->VideoSequence.VideoPicture.MacroBlockPerRow * 8 + 16;
  limitRectC.height = pInfo->VideoSequence.VideoPicture.MacroBlockPerCol * 8 + 16;
  quant = pInfo->VideoSequence.VideoPicture.pic_quant;
  colNum = rowNum = 0;
  TRD = pInfo->VideoSequence.TRD;
  TRB = pInfo->VideoSequence.TRB;
  pMBinfo = pInfo->VideoSequence.MBinfo;

  quant = VPic->pic_quant;
  pInfo->VideoSequence.VideoPicture.gob_number = 0;
  num_gobs = pInfo->VideoSequence.VideoPicture.num_gobs_in_pic;
  num_rows_per_gob = pInfo->VideoSequence.VideoPicture.RowsPerGOB;

  rt = VPic->rtype;

  dy = 0;
  frGOB = 0;

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
        h263_CopyMacroBlocks(&pInfo->VideoSequence.rFrame, &pInfo->VideoSequence.nFrame, mbPerRow, rowNum, colNum, nummbs2copy);
        rowNum = num_rows_per_gob * VPic->gob_number;
        colNum = 0;
        pYc = pInfo->VideoSequence.cFrame.pY + (stepYc << 4) * rowNum;
        pCbc = pInfo->VideoSequence.cFrame.pCb + (stepCbc << 3) * rowNum;
        pCrc = pInfo->VideoSequence.cFrame.pCr + (stepCrc << 3) * rowNum;
        pYr = pInfo->VideoSequence.rFrame.pY + (stepYr << 4) * rowNum;
        pCbr = pInfo->VideoSequence.rFrame.pCb + (stepCbr << 3) * rowNum;
        pCrr = pInfo->VideoSequence.rFrame.pCr + (stepCrr << 3) * rowNum;
        pYn = pInfo->VideoSequence.nFrame.pY + (stepYn << 4) * rowNum;
        pCbn = pInfo->VideoSequence.nFrame.pCb + (stepCbn << 3) * rowNum;
        pCrn = pInfo->VideoSequence.nFrame.pCr + (stepCrn << 3) * rowNum;
        pMBinfo = pInfo->VideoSequence.MBinfo + rowNum * mbPerRow;
        if (VPic->gob_number > i)
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
          if (h263_DecodeMBTYPE_B(pInfo, &mb_type, &cbpFlag, &dquantFlag) != H263_STATUS_OK)
            goto Err_1;
        } while (mb_type == IPPVC_MB_STUFFING);

        if (mb_not_coded) {

          h263_CalculateMV_Direct(pMBinfo->mv, mvForw, mvBack, &mvCbCrF, &mvCbCrB, TRD, TRB, pMBinfo->type,
                                  dx, dy, &limitRectL, &limitRectC);

          if (pMBinfo->type == IPPVC_MBTYPE_INTER4V_Q || pMBinfo->type == IPPVC_MBTYPE_INTER4V) {
            h263_Copy8x8HP_8u(pYr, stepYr, tmpMB, 16, &mvForw[0], rt);
            h263_Copy8x8HP_8u(pYc, stepYc, pYn, stepYn, &mvBack[0], rt);
            h263_Copy8x8HP_8u(pYr + 8, stepYr, tmpMB + 8, 16, &mvForw[1], rt);
            h263_Copy8x8HP_8u(pYc + 8, stepYc, pYn + 8, stepYn, &mvBack[1], rt);
            h263_Copy8x8HP_8u(pYr + 8*stepYr, stepYr, tmpMB + 8*16, 16, &mvForw[2], rt);
            h263_Copy8x8HP_8u(pYc + 8*stepYc, stepYc, pYn + 8*stepYn, stepYn, &mvBack[2], rt);
            h263_Copy8x8HP_8u(pYr + 8*stepYr + 8, stepYr, tmpMB + 8 + 8*16, 16, &mvForw[3], rt);
            h263_Copy8x8HP_8u(pYc + 8*stepYc + 8, stepYc, pYn + 8*stepYn + 8, stepYn, &mvBack[3], rt);
          } else {
            h263_Copy16x16HP_8u(pYr, stepYr, tmpMB, 16, &mvForw[0], rt);
            h263_Copy16x16HP_8u(pYc, stepYc, pYn, stepYn, &mvBack[0], rt);
          }

          roiSize.width = roiSize.height = 16;
          ippiAddBackPredPB_H263_8u_C1R(tmpMB, 16, roiSize, pYn, stepYn, 0);

          h263_Copy8x8HP_8u(pCbr, stepCbr, tmpMB, 8, &mvCbCrF, rt);
          h263_Copy8x8HP_8u(pCbc, stepCbc, pCbn, stepCbn, &mvCbCrB, rt);
          roiSize.width = roiSize.height = 8;
          ippiAddBackPredPB_H263_8u_C1R(tmpMB, 8, roiSize, pCbn, stepCbn, 0);

          h263_Copy8x8HP_8u(pCrr, stepCrr, tmpMB, 8, &mvCbCrF, rt);
          h263_Copy8x8HP_8u(pCrc, stepCrc, pCrn, stepCrn, &mvCbCrB, rt);
          ippiAddBackPredPB_H263_8u_C1R(tmpMB, 8, roiSize, pCrn, stepCrn, 0);

          predMVF[colNum].dx = predMVF[colNum].dy = predMVB[colNum].dx = predMVB[colNum].dy = 0;

          if (VPic->oppmodes.advIntra) {
            for (k = 0; k < 6; k++)
              b[(colNum + 1)*6 + k].dct_dc = -1;
          }
          h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB_NOTCODED);

        } else {

          if (VPic->oppmodes.advIntra) {
            if (mb_type == IPPVC_MBTYPE_INTRA || mb_type == IPPVC_MBTYPE_INTRA_Q) {
              h263_AdvI_PredMode(pInfo, predMode);
            } else {
              for (k = 0; k < 6; k++)
                b[(colNum + 1)*6 + k].dct_dc = -1;
            }
          }

          if (cbpFlag) {
            h263_Decode_CBPC_B(pInfo, cbpc);

            if (VPic->oppmodes.altInterVLC && cbpc == 3) { /* ??? */
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

          if (mb_type == IPPVC_MBTYPE_FORWARD || mb_type == IPPVC_MBTYPE_INTERPOLATE) {
            if (h263_PredictDecodeMV_B(pInfo, predMVF, &mvForw[0], frGOB, rowNum, colNum) != H263_STATUS_OK) {
              h263_Error("Error when decoding B-frame forward motion vector");
              goto Err_1;
            }
            h263_LimitMV(&mvForw[0], &mvForw[0], &limitRectL, dx, dy, 16);
            h263_ComputeChromaMV(&mvForw[0], &mvCbCrF);
          } else
            predMVF[colNum].dx = predMVF[colNum].dy = 0;

          if (mb_type == IPPVC_MBTYPE_BACKWARD || mb_type == IPPVC_MBTYPE_INTERPOLATE) {
            if (h263_PredictDecodeMV_B(pInfo,  predMVB, &mvBack[0], frGOB, rowNum, colNum) != H263_STATUS_OK) {
              h263_Error("Error when decoding B-frame backward motion vector");
              goto Err_1;
            }
            h263_LimitMV(&mvBack[0], &mvBack[0], &limitRectL, dx, dy, 16);
            h263_ComputeChromaMV(&mvBack[0], &mvCbCrB);
          } else
            predMVB[colNum].dx = predMVB[colNum].dy = 0;

          if (mb_type == IPPVC_MBTYPE_DIRECT) {
            h263_CalculateMV_Direct(pMBinfo->mv, mvForw, mvBack, &mvCbCrF, &mvCbCrB, TRD, TRB, pMBinfo->type,
                                    dx, dy, &limitRectL, &limitRectC);
          }

          if (mb_type == IPPVC_MBTYPE_FORWARD || mb_type == IPPVC_MBTYPE_BACKWARD) {
            IppMotionVector *mvLum, *mvChr;
            Ipp8u *pY, *pCb, *pCr;
            Ipp32s stepY;

            if (mb_type == IPPVC_MBTYPE_FORWARD) {
              pY = pYr; pCb = pCbr; pCr = pCrr;
              stepY = stepYr;
              mvLum = &mvForw[0]; mvChr = &mvCbCrF;
            } else {
              pY = pYc; pCb = pCbc; pCr = pCrc;
              stepY = stepYc;
              mvLum = &mvBack[0]; mvChr = &mvCbCrB;
            }

            if (cbpy) {
              h263_DecodeMCBlockInter(pInfo, quant, cbpy & 8, pY, pYn, stepYn, coeffMB, mvLum, rt);
              h263_DecodeMCBlockInter(pInfo, quant, cbpy & 4, pY+8, pYn+8, stepYn, coeffMB, mvLum, rt);
              h263_DecodeMCBlockInter(pInfo, quant, cbpy & 2, pY+stepY*8, pYn+stepYn*8, stepYn, coeffMB, mvLum, rt);
              h263_DecodeMCBlockInter(pInfo, quant, cbpy & 1, pY+8+stepY*8, pYn+8+stepYn*8, stepYn, coeffMB, mvLum, rt);
            } else {
              h263_Copy16x16HP_8u(pY, stepY, pYn, stepYn, mvLum, rt);
              h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
              h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
              h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
              h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
            }
            h263_DecodeMCBlockInter(pInfo, quant_c, cbpc & 2, pCb, pCbn, stepCbn, coeffMB, mvChr, rt);
            h263_DecodeMCBlockInter(pInfo, quant_c, cbpc & 1, pCr, pCrn, stepCrn, coeffMB, mvChr, rt);

          } else if (mb_type == IPPVC_MBTYPE_INTERPOLATE || mb_type == IPPVC_MBTYPE_DIRECT) {

            if ((mb_type == IPPVC_MBTYPE_DIRECT) && (pMBinfo->type == IPPVC_MBTYPE_INTER4V_Q || pMBinfo->type == IPPVC_MBTYPE_INTER4V)) {
              h263_Copy8x8HP_8u(pYr, stepYr, tmpMB, 16, &mvForw[0], rt);
              h263_Copy8x8HP_8u(pYc, stepYc, tmpMB + 4*64, 16, &mvBack[0], rt);
              h263_Copy8x8HP_8u(pYr + 8, stepYr, tmpMB + 8, 16, &mvForw[1], rt);
              h263_Copy8x8HP_8u(pYc + 8, stepYc, tmpMB + 4*64 + 8, 16, &mvBack[1], rt);
              h263_Copy8x8HP_8u(pYr + 8*stepYr, stepYr, tmpMB + 8*16, 16, &mvForw[2], rt);
              h263_Copy8x8HP_8u(pYc + 8*stepYc, stepYc, tmpMB + 4*64 + 8*16, 16, &mvBack[2], rt);
              h263_Copy8x8HP_8u(pYr + 8*stepYr + 8, stepYr, tmpMB + 8 + 8*16, 16, &mvForw[3], rt);
              h263_Copy8x8HP_8u(pYc + 8*stepYc + 8, stepYc, tmpMB + 4*64 + 8 + 8*16, 16, &mvBack[3], rt);
            } else {
              h263_Copy16x16HP_8u(pYr, stepYr, tmpMB, 16, &mvForw[0], rt);
              h263_Copy16x16HP_8u(pYc, stepYc, tmpMB + 4*64, 16, &mvBack[0], rt);
            }

            roiSize.width = roiSize.height = 16;
            ippiAddBackPredPB_H263_8u_C1R(tmpMB + 4*64, 16, roiSize, tmpMB, 16, 0);
            h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 8, tmpMB, 16, pYn, stepYn, coeffMB);
            h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 4, tmpMB+8, 16, pYn+8, stepYn, coeffMB);
            h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 2, tmpMB+8*16, 16, pYn+8*stepYn, stepYn, coeffMB);
            h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 1, tmpMB+8+8*16, 16, pYn+8+8*stepYn, stepYn, coeffMB);

            h263_Copy8x8HP_8u(pCbr, stepCbr, tmpMB, 8, &mvCbCrF, rt);
            h263_Copy8x8HP_8u(pCbc, stepCbc, tmpMB + 64, 8, &mvCbCrB, rt);
            roiSize.width = roiSize.height = 8;
            ippiAddBackPredPB_H263_8u_C1R(tmpMB + 64, 8, roiSize, tmpMB, 8, 0);
            h263_DecodeMCBlockInter_B(pInfo, quant_c, cbpc & 2, tmpMB, 8, pCbn, stepCbn, coeffMB);

            h263_Copy8x8HP_8u(pCrr, stepCrr, tmpMB, 8, &mvCbCrF, rt);
            h263_Copy8x8HP_8u(pCrc, stepCrc, tmpMB + 64, 8, &mvCbCrB, rt);
            ippiAddBackPredPB_H263_8u_C1R(tmpMB + 64, 8, roiSize, tmpMB, 8, 0);
            h263_DecodeMCBlockInter_B(pInfo, quant_c, cbpc & 1, tmpMB, 8, pCrn, stepCrn, coeffMB);

          } else { /* INTRA MB */
            pF[0] = pYn; pF[1] = pYn + 8; pF[2] = pYn + 8 * stepYn; pF[3] = pYn + 8 * stepYn + 8; pF[4] = pCbn; pF[5] = pCrn;

            if (!VPic->oppmodes.advIntra)
              status = h263_DecodeMacroBlockIntra(pInfo, (cbpy << 2) + cbpc, quant, quant_c, pF, stepF);
            else
              status = h263_DecodeMacroBlockIntra_AdvI(pInfo, colNum, (cbpy << 2) + cbpc, quant, quant_c, predMode, pF, stepF);
            if (status != H263_STATUS_OK) {
              h263_Error("Error when decoding coefficients of Intra block");
              goto Err_1;
            }
          }
        }
        pYc += 16; pCrc += 8; pCbc += 8;
        pYr += 16; pCrr += 8; pCbr += 8;
        pYn += 16; pCrn += 8; pCbn += 8;
        pMBinfo++;
      }
      pYc += 2 * H263_NUM_EXT_MB * 16 + (stepYc << 4) - stepYc;
      pCbc += 2 * H263_NUM_EXT_MB * 8 + (stepCbc << 3) - stepCbc;
      pCrc += 2 * H263_NUM_EXT_MB * 8 + (stepCrc << 3) - stepCrc;
      pYr += 2 * H263_NUM_EXT_MB * 16 + (stepYr << 4) - stepYr;
      pCbr += 2 * H263_NUM_EXT_MB * 8 + (stepCbr << 3) - stepCbr;
      pCrr += 2 * H263_NUM_EXT_MB * 8 + (stepCrr << 3) - stepCrr;
      pYn += 2 * H263_NUM_EXT_MB * 16 + (stepYn << 4) - stepYn;
      pCbn += 2 * H263_NUM_EXT_MB * 8 + (stepCbn << 3) - stepCbn;
      pCrn += 2 * H263_NUM_EXT_MB * 8 + (stepCrn << 3) - stepCrn;
    }
  }

  h263_AlignBits(pInfo);
  return H263_STATUS_OK;

Err_1:
  sts = H263_STATUS_ERROR;
  if (pInfo->stopOnErr)
    return sts;
  if (!h263_SeekGOBStartCodePtr(pInfo)) {
    h263_CopyMacroBlocks(&pInfo->VideoSequence.rFrame, &pInfo->VideoSequence.cFrame, mbPerRow, rowNum, colNum,  (mbPerCol - rowNum) * mbPerRow - colNum);
    return sts;
  }
  goto ErrRet_1;
}
