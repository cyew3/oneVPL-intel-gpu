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

static h263_Status h263_PredictDecode4MV_PB(h263_Info *pInfo, IppMotionVector *mv,
                                            IppMotionVector *mvF, IppMotionVector *mvB, IppMotionVector *mvFPrev,
                                            Ipp32s mb_type, Ipp32s mvdb, Ipp32s trd, Ipp32s trb, Ipp32s fourMV)
{
  h263_Status status = H263_STATUS_OK;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s mvdx = 0, mvdy = 0;
  Ipp32s i, numMV = fourMV ? 4 : 1;
  Ipp32s leftlim = VPic->modes.UMV ? -63 : -32;
  Ipp32s rightlim = VPic->modes.UMV ? 63 : 31;

  if (mvdb) {
    if (!VPic->oppmodes.UMV) {
      status = h263_DecodeMVD(pInfo, &mvdx, &mvdy, 1);
    } else {
      status = h263_DecodeMVD_umvplus(pInfo, &mvdx, &mvdy);
    }
  }

  if (mb_type != IPPVC_MBTYPE_FORWARD) {
    for (i = 0; i < numMV; i++) {
      mvF[i].dx = (Ipp16s)(mv[i].dx * trb / trd);
      mvF[i].dy = (Ipp16s)(mv[i].dy * trb / trd);
    }
  } else
    mvF[0] = *mvFPrev;

  if (mb_type == -1) { /* PB-frame */
    if (!VPic->oppmodes.UMV) {
      for (i = 0; i < numMV; i++) {
        reconstructMV(&mvF[i], mvdx, mvdy, leftlim, rightlim);
        mvB[i].dx = (Ipp16s)(mvdx ? (mvF[i].dx - mv[i].dx) : ((trb - trd) * mv[i].dx / trd));
        mvB[i].dy = (Ipp16s)(mvdy ? (mvF[i].dy - mv[i].dy) : ((trb - trd) * mv[i].dy / trd));
      }
    } else {
      for (i = 0; i < numMV; i++) {
        mvF[i].dx = mvF[i].dx + (Ipp16s)mvdx;
        mvF[i].dy = mvF[i].dy + (Ipp16s)mvdy;
        mvB[i].dx = (Ipp16s)(mvdx ? (mvF[i].dx - mv[i].dx) : ((trb - trd) * mv[i].dx / trd));
        mvB[i].dy = (Ipp16s)(mvdy ? (mvF[i].dy - mv[i].dy) : ((trb - trd) * mv[i].dy / trd));
      }
    }
  } else if (mb_type == IPPVC_MBTYPE_INTERPOLATE) {
    for (i = 0; i < numMV; i++) {
      mvB[i].dx = (Ipp16s)((trb - trd) * mv[i].dx / trd);
      mvB[i].dy = (Ipp16s)((trb - trd) * mv[i].dy / trd);
    }
  } else { /* IPPVC_MBTYPE_FORWARD */
    if (!VPic->oppmodes.UMV) {
      reconstructMV(&mvF[0], mvdx, mvdy, leftlim, rightlim);
    } else {
      mvF[0].dx = mvF[0].dx + (Ipp16s)mvdx;
      mvF[0].dy = mvF[0].dy + (Ipp16s)mvdy;
    }
    *mvFPrev = mvF[0];
  }
  return status;
}

/*
//  decode H.263++ P-Picture
*/
h263_Status h263_DecodeFrame_P(h263_Info* pInfo, h263_Frame *rFrame)
{
  __ALIGN16(Ipp16s, coeffMB, 64*6);
  __ALIGN16(Ipp16s, coeffMB_b, 64*6);
  __ALIGN16(Ipp16s, coeffMB_bI, 64*2);
  __ALIGN16(Ipp8u, predMB, 64*6);
  Ipp32s          quant, quant_c, bquant = 0, bquant_c = 0, i, dx, dy;
  Ipp32s          stepYr, stepYc, stepCbr, stepCbc, stepCrr, stepCrc, mbPerRow, mbPerCol;
  Ipp32s          colNum, rowNum, stepF[6];
  Ipp8u           *pYc, *pCbc, *pCrc, *pYr, *pCbr, *pCrr, *pF[6];
  Ipp8u           *pYn = 0, *pCbn = 0, *pCrn = 0;
  Ipp32s          stepYn = 0, stepCbn = 0, stepCrn = 0;
  Ipp32s          mb_not_coded, mb_type = 0, cbpc = 0, cbpy = 0, cbpyPrev = 0;
  Ipp32s          rt;
  IppiRect        limitRectL, limitRectC;
  IppMotionVector mvCur[4], mvCbCr = {0,0}, mvPrev[4];
  IppMotionVector mvBack[4], mvForw[4], mvBackPrev[4], mvForwPrev[4], mvFCbCr = {0,0}, mvBCbCr = {0,0}, mvFCbCrPrev = {0,0}, mvBCbCrPrev, mvForwPred;
  h263_MacroBlock *pMBinfo;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s gob_header_present = 1;
  Ipp32s cbpb = 0, mvdb, bmb_type = 0, cbpbPrev = 0, bmb_typePrev = 0;
  Ipp32s predMode = 0;
  h263_Status status;
  Ipp32s num_rows_per_gob, num_gobs, row;
  Ipp32s k;
  Ipp32s fourMVmode = VPic->modes.advPred | VPic->oppmodes.advPred | VPic->oppmodes.deblockFilt;
  Ipp32s obmcFlag = VPic->modes.advPred | VPic->oppmodes.advPred;
  Ipp32s pbFlag = VPic->picture_coding_type == H263_PIC_TYPE_PB || VPic->picture_coding_type == H263_PIC_TYPE_iPB;
  Ipp32s trd = VPic->temporal_reference - VPic->prev_temporal_reference;
  Ipp32s trb = VPic->temporal_reference_B;
  Ipp32s fourMVmb = 0;
  h263_Status sts;

#ifndef UMC_H263_DISABLE_SORENSON
   fourMVmode |= pInfo->sorenson;
#endif
  if (trd < 0)
    trd += VPic->PCF ? 1024 : 256;

  stepYc = pInfo->VideoSequence.cFrame.stepY;
  stepYr = rFrame->stepY;
  stepCbc = pInfo->VideoSequence.cFrame.stepCb;
  stepCbr = rFrame->stepCb;
  stepCrc = pInfo->VideoSequence.cFrame.stepCr;
  stepCrr = rFrame->stepCr;
  pYc = pInfo->VideoSequence.cFrame.pY;
  pCbc = pInfo->VideoSequence.cFrame.pCb;
  pCrc = pInfo->VideoSequence.cFrame.pCr;
  pYr = rFrame->pY;
  pCbr = rFrame->pCb;
  pCrr = rFrame->pCr;
  stepF[0] = stepF[1] = stepF[2] = stepF[3] = stepYc; stepF[4] = stepCbc; stepF[5] = stepCrc;

  if (pbFlag) {
    stepYn = pInfo->VideoSequence.nFrame.stepY;
    stepCbn = pInfo->VideoSequence.nFrame.stepCb;
    stepCrn = pInfo->VideoSequence.nFrame.stepCr;
    pYn = pInfo->VideoSequence.nFrame.pY;
    pCbn = pInfo->VideoSequence.nFrame.pCb;
    pCrn = pInfo->VideoSequence.nFrame.pCr;
  }

  mbPerRow = pInfo->VideoSequence.VideoPicture.MacroBlockPerRow;
  mbPerCol = pInfo->VideoSequence.VideoPicture.MacroBlockPerCol;

  num_gobs = pInfo->VideoSequence.VideoPicture.num_gobs_in_pic;
  num_rows_per_gob = pInfo->VideoSequence.VideoPicture.RowsPerGOB;

  /* Bounding rectangle for MV limitation */

  limitRectL.x = -16;
  limitRectL.y = -16;
  limitRectL.width = pInfo->VideoSequence.VideoPicture.MacroBlockPerRow * 16 + 32;
  limitRectL.height = pInfo->VideoSequence.VideoPicture.MacroBlockPerCol * 16 + 32;
  limitRectC.x = -8;
  limitRectC.y = -8;
  limitRectC.width = pInfo->VideoSequence.VideoPicture.MacroBlockPerRow * 8 + 16;
  limitRectC.height = pInfo->VideoSequence.VideoPicture.MacroBlockPerCol * 8 + 16;

  rt = VPic->rtype;

  pMBinfo = pInfo->VideoSequence.MBinfo;

  {
    Ipp32s              frGOB;
    h263_IntraPredBlock *b = pInfo->VideoSequence.IntraPredBuff.block;

    quant = VPic->pic_quant;
    pInfo->VideoSequence.VideoPicture.gob_number = 0;
    dy = 0;
    frGOB = 0;
    if (VPic->oppmodes.deblockFilt)
      ippsZero_8u(pInfo->VideoSequence.GOBboundary, mbPerCol);

    for (i = 0; i < num_gobs; i++) {
      VPic->gob_number = i;
      rowNum = i*num_rows_per_gob;
      colNum = 0;

      if (i) {
ErrRet_1:
        gob_header_present = h263_ParseGOBHeader(pInfo);
        if (gob_header_present < 0) {
          h263_Error("Error: Invalid GOB header");
          goto Err_1;
//          return H263_STATUS_PARSE_ERROR;
        }

        if (VPic->gob_number > 30)
          return H263_STATUS_OK; /* EOS or EOSBS */

        if (gob_header_present) {
          Ipp32s nummbs2copy = (VPic->gob_number * num_rows_per_gob - rowNum) * mbPerRow - colNum;
          h263_CopyMacroBlocks(&pInfo->VideoSequence.rFrame, &pInfo->VideoSequence.cFrame, mbPerRow, rowNum, colNum, nummbs2copy);
          if (pbFlag)
            h263_CopyMacroBlocks(&pInfo->VideoSequence.rFrame, &pInfo->VideoSequence.nFrame, mbPerRow, rowNum, colNum, nummbs2copy);
          rowNum = num_rows_per_gob * VPic->gob_number;
          colNum = 0;
          pYc = pInfo->VideoSequence.cFrame.pY + (stepYc << 4) * rowNum;
          pCbc = pInfo->VideoSequence.cFrame.pCb + (stepCbc << 3) * rowNum;
          pCrc = pInfo->VideoSequence.cFrame.pCr + (stepCrc << 3) * rowNum;
          pYr = pInfo->VideoSequence.rFrame.pY + (stepYr << 4) * rowNum;
          pCbr = pInfo->VideoSequence.rFrame.pCb + (stepCbr << 3) * rowNum;
          pCrr = pInfo->VideoSequence.rFrame.pCr + (stepCrr << 3) * rowNum;
          if (pbFlag) {
            pYn = pInfo->VideoSequence.nFrame.pY + (stepYn << 4) * rowNum;
            pCbn = pInfo->VideoSequence.nFrame.pCb + (stepCbn << 3) * rowNum;
            pCrn = pInfo->VideoSequence.nFrame.pCr + (stepCrn << 3) * rowNum;
          }
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

      if (VPic->picture_coding_type == H263_PIC_TYPE_PB || VPic->picture_coding_type == H263_PIC_TYPE_iPB) {
        bquant_c = bquant = (VPic->dbquant + 5)*quant >> 2;
        h263_CLIPR(bquant, 31);
        if (VPic->oppmodes.modQuant)
          bquant_c = h263_quant_c[bquant];
      }

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
            if (h263_DecodeMCBPC_P(pInfo, &mb_type, &cbpc, 1) != H263_STATUS_OK)
              goto Err_1;
          } while (mb_type == IPPVC_MB_STUFFING);

          if (mb_not_coded) {
            if (!obmcFlag) {
              ippiCopy16x16_8u_C1R(pYr, stepYr, pYc, stepYc);
              ippiCopy8x8_8u_C1R(pCbr, stepCbr, pCbc, stepCbc);
              ippiCopy8x8_8u_C1R(pCrr, stepCrr, pCrc, stepCrc);
              if (pbFlag) {
                ippiCopy16x16_8u_C1R(pYr, stepYr, pYn, stepYn);
                ippiCopy8x8_8u_C1R(pCbr, stepCbr, pCbn, stepCbn);
                ippiCopy8x8_8u_C1R(pCrr, stepCrr, pCrn, stepCrn);
              }
            }
            mb_type = pMBinfo->type = IPPVC_MB_STUFFING; /* to indicate mb_not_coded */
            h263_Zero4MV(pMBinfo->mv);
            mvForwPred.dx = mvForwPred.dy = 0;
            if (VPic->oppmodes.advIntra) {
              for (k = 0; k < 6; k++)
                b[(colNum + 1)*6 + k].dct_dc = -1;
            }
            h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB_NOTCODED);
          } else {

            pMBinfo->type = (Ipp8u)mb_type;

            if (VPic->oppmodes.advIntra && (mb_type == IPPVC_MBTYPE_INTRA || mb_type == IPPVC_MBTYPE_INTRA_Q))
              h263_AdvI_PredMode(pInfo, predMode);

            mvdb = cbpb = 0;
            if (VPic->picture_coding_type == H263_PIC_TYPE_PB) {
              if (h263_GetBit(pInfo)) {
                mvdb = 1;
                cbpb = h263_GetBit(pInfo);
              } else {
                mvForw[0].dx = mvForw[0].dy = mvBack[0].dx = mvBack[0].dy = 0;
              }
              bmb_type = -1;
              h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB_INTERPOLATE_PB);
            } else if (VPic->picture_coding_type == H263_PIC_TYPE_iPB) {
              h263_DecodeMODB_iPB(pInfo, &bmb_type, &cbpb, &mvdb);
              if (!mvdb || colNum == 0)
                mvForwPred.dx = mvForwPred.dy = 0;
            }
            if (cbpb)
              cbpb = h263_GetBits9(pInfo, 6);

            if (VPic->oppmodes.altInterVLC && cbpc == 3) {
              if (h263_DecodeCBPY_I(pInfo, &cbpy) != H263_STATUS_OK)
                goto Err_1;
            } else {
              if (h263_DecodeCBPY_P(pInfo, &cbpy, mb_type) != H263_STATUS_OK)
                goto Err_1;
            }

            if (mb_type == IPPVC_MBTYPE_INTRA_Q || mb_type == IPPVC_MBTYPE_INTER_Q || mb_type == IPPVC_MBTYPE_INTER4V_Q) {
              if (!VPic->oppmodes.modQuant) {
                h263_UpdateQuant(pInfo, VPic->pic_quant);
                quant_c = VPic->pic_quant;
              } else {
                quant_c = h263_UpdateQuant_Mod(pInfo);
              }
              quant = VPic->pic_quant;
              if (VPic->picture_coding_type == H263_PIC_TYPE_PB || VPic->picture_coding_type == H263_PIC_TYPE_iPB) {
                bquant_c = bquant = (VPic->dbquant + 5)*quant >> 2;
                h263_CLIPR(bquant, 31);
                if (VPic->oppmodes.modQuant)
                  bquant_c = h263_quant_c[bquant];
              }
            }

            pMBinfo->quant = (Ipp8u)quant;

            if (mb_type == IPPVC_MBTYPE_INTRA || mb_type == IPPVC_MBTYPE_INTRA_Q) {
              pF[0] = pYc; pF[1] = pYc + 8; pF[2] = pYc + 8 * stepYc; pF[3] = pYc + 8 * stepYc + 8; pF[4] = pCbc; pF[5] = pCrc;

              if (VPic->picture_coding_type == H263_PIC_TYPE_PB || (VPic->picture_coding_type == H263_PIC_TYPE_iPB && bmb_type == IPPVC_MBTYPE_INTERPOLATE)) {
                if (h263_PredictDecodeMV(pInfo, pMBinfo, frGOB, rowNum, colNum, fourMVmode) != H263_STATUS_OK) {
                  h263_Error("Error when decoding motion vector");
                  goto Err_1;
                }
                if (fourMVmode) {
                  pMBinfo->mv[1] = pMBinfo->mv[2] = pMBinfo->mv[3] = pMBinfo->mv[0];
                }
              } else {
                if (!fourMVmode)
                  pMBinfo->mv[0].dx = pMBinfo->mv[0].dy = 0;
                else {
                  pMBinfo->type |= 0x80; /* to indicate to OBMC that no MV is present */
                  h263_Zero4MV(pMBinfo->mv);
                }
              }

              if (pbFlag && bmb_type != IPPVC_MBTYPE_BACKWARD) {
                if (h263_PredictDecode4MV_PB(pInfo, &pMBinfo->mv[0], &mvForw[0], &mvBack[0], &mvForwPred,
                    bmb_type, mvdb, trd, trb, 0) != H263_STATUS_OK) {
                  h263_Error("Error when decoding MVDB");
                  goto Err_1;
                }
                h263_LimitMV(mvForw, mvForw, &limitRectL, dx, dy, 16);
                h263_ComputeChromaMV(&mvForw[0], &mvFCbCr);
                if (bmb_type != IPPVC_MBTYPE_FORWARD) {
                  h263_LimitMV(mvBack, mvBack, &limitRectL, dx, dy, 16);
                  h263_ComputeChromaMV(&mvBack[0], &mvBCbCr);
                }
              }

              if (!VPic->oppmodes.advIntra)
                status = h263_DecodeMacroBlockIntra(pInfo, (cbpy << 2) + cbpc, quant, quant_c, pF, stepF);
              else
                status = h263_DecodeMacroBlockIntra_AdvI(pInfo, colNum, (cbpy << 2) + cbpc, quant, quant_c, predMode, pF, stepF);
              if (status != H263_STATUS_OK) {
                h263_Error("Error when decoding coefficients of Intra block");
                goto Err_1;
              }
              if (pbFlag) {
                Ipp8u *pRefY, *pRefCb, *pRefCr;
                Ipp32s  stepLum, stepChr;

                if (bmb_type != IPPVC_MBTYPE_FORWARD) {
                  if (bmb_type != IPPVC_MBTYPE_BACKWARD) {

                    h263_BidirPredMacroblock(pYr, stepYr, pCbr, pCrr, stepCbr, pYc, stepYc, pCbc, pCrc, stepCbc,
                                             predMB, &mvForw[0], &mvFCbCr, &mvBack[0], &mvBCbCr, 0);

                    pRefY  =  predMB;         stepLum = 16;
                    pRefCb =  predMB + 4*64;  stepChr = 8;
                    pRefCr =  predMB + 5*64;
                  } else {
                    pRefY  =  pYc;   stepLum = stepYc;
                    pRefCb =  pCbc;  stepChr = stepCbc;
                    pRefCr =  pCrc;
                  }

                  h263_DecodeMCBlockInter_B(pInfo, bquant, cbpb & 32, pRefY, stepLum, pYn, stepYn, coeffMB_bI);
                  h263_DecodeMCBlockInter_B(pInfo, bquant, cbpb & 16, pRefY+8, stepLum, pYn+8, stepYn, coeffMB_bI);
                  h263_DecodeMCBlockInter_B(pInfo, bquant, cbpb & 8, pRefY+8*stepLum, stepLum, pYn+8*stepYn, stepYn, coeffMB_bI);
                  h263_DecodeMCBlockInter_B(pInfo, bquant, cbpb & 4, pRefY+8+8*stepLum, stepLum, pYn+8+8*stepYn, stepYn, coeffMB_bI);
                  h263_DecodeMCBlockInter_B(pInfo, bquant_c, cbpb & 2, pRefCb, stepChr, pCbn, stepCbn, coeffMB_bI);
                  h263_DecodeMCBlockInter_B(pInfo, bquant_c, cbpb & 1, pRefCr, stepChr, pCrn, stepCrn, coeffMB_bI);

                } else {
                  if (cbpb & 60) {
                    h263_DecodeMCBlockInter(pInfo, bquant, cbpb & 32, pYr, pYn, stepYr, coeffMB_bI, &mvForw[0], 0);
                    h263_DecodeMCBlockInter(pInfo, bquant, cbpb & 16, pYr+8, pYn+8, stepYr, coeffMB_bI, &mvForw[0], 0);
                    h263_DecodeMCBlockInter(pInfo, bquant, cbpb & 8, pYr+stepYr*8, pYn+stepYn*8, stepYr, coeffMB_bI, &mvForw[0], 0);
                    h263_DecodeMCBlockInter(pInfo, bquant, cbpb & 4, pYr+8+stepYr*8, pYn+8+stepYn*8, stepYr, coeffMB_bI, &mvForw[0], 0);
                  } else {
                    h263_Copy16x16HP_8u(pYr, stepYr, pYn, stepYn, &mvForw[0], 0);
                    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                  }
                  h263_DecodeMCBlockInter(pInfo, bquant_c, cbpb & 2, pCbr, pCbn, stepCbr, coeffMB_bI, &mvFCbCr, 0);
                  h263_DecodeMCBlockInter(pInfo, bquant_c, cbpb & 1, pCrr, pCrn, stepCrr, coeffMB_bI, &mvFCbCr, 0);
                }
              }
            } else { /* INTER MB */

              fourMVmb = (mb_type == IPPVC_MBTYPE_INTER4V || mb_type == IPPVC_MBTYPE_INTER4V_Q);

              if (VPic->oppmodes.advIntra) {
                for (k = 0; k < 6; k++)
                  b[(colNum + 1)*6 + k].dct_dc = -1;
              }

              if (!fourMVmb) {
                if (h263_PredictDecodeMV(pInfo, pMBinfo, frGOB, rowNum, colNum, fourMVmode) != H263_STATUS_OK) {
                  h263_Error("Error when decoding motion vector");
                  goto Err_1;
                }
                h263_LimitMV(&pMBinfo->mv[0], &mvCur[0], &limitRectL, dx, dy, 16);
                h263_ComputeChromaMV(&mvCur[0], &mvCbCr);
                if (fourMVmode) {
                  mvCur[1] = mvCur[2] = mvCur[3] = mvCur[0];
                  pMBinfo->mv[1] = pMBinfo->mv[2] = pMBinfo->mv[3] = pMBinfo->mv[0];
                }
                if (pbFlag && bmb_type != IPPVC_MBTYPE_BACKWARD) {
                  if (h263_PredictDecode4MV_PB(pInfo, &pMBinfo->mv[0], &mvForw[0], &mvBack[0], &mvForwPred,
                      bmb_type, mvdb, trd, trb, 0) != H263_STATUS_OK) {
                    h263_Error("Error when decoding MVDB");
                    goto Err_1;
                  }
                  h263_LimitMV(mvForw, mvForw, &limitRectL, dx, dy, 16);
                  h263_ComputeChromaMV(&mvForw[0], &mvFCbCr);
                  if (bmb_type != IPPVC_MBTYPE_FORWARD) {
                    h263_LimitMV(mvBack, mvBack, &limitRectL, dx, dy, 16);
                    h263_ComputeChromaMV(&mvBack[0], &mvBCbCr);
                  }
                }
              } else {
                if (h263_PredictDecode4MV(pInfo, pMBinfo, frGOB, rowNum, colNum) != H263_STATUS_OK) {
                  h263_Error("Error when decoding motion vectors");
                  goto Err_1;
                }
                h263_Limit4MV(pMBinfo->mv, mvCur, &limitRectL, dx, dy, 8);
                h263_ComputeChroma4MV(pMBinfo->mv, &mvCbCr);
                h263_LimitMV(&mvCbCr, &mvCbCr, &limitRectC, dx >> 1, dy >> 1, 8);

                if (pbFlag && bmb_type != IPPVC_MBTYPE_BACKWARD) {
                  if (h263_PredictDecode4MV_PB(pInfo, &pMBinfo->mv[0], &mvForw[0], &mvBack[0], &mvForwPred,
                      bmb_type, mvdb, trd, trb, 1) != H263_STATUS_OK) {
                    h263_Error("Error when decoding MVDB");
                    goto Err_1;
                  }
                  h263_Limit4MV(mvForw, mvForw, &limitRectL, dx, dy, 8);
                  if (bmb_type != IPPVC_MBTYPE_FORWARD) {
                    h263_Limit4MV(mvBack, mvBack, &limitRectL, dx, dy, 8);
                    h263_ComputeChroma4MV(&mvBack[0], &mvBCbCr);
                    h263_LimitMV(&mvBCbCr, &mvBCbCr, &limitRectC, dx >> 1, dy >> 1, 8);
                    h263_ComputeChroma4MV(&mvForw[0], &mvFCbCr);
                  } else
                    h263_ComputeChromaMV(&mvForw[0], &mvFCbCr);
                  h263_LimitMV(&mvFCbCr, &mvFCbCr, &limitRectC, dx >> 1, dy >> 1, 8);
                }
              }

              /* decode and MC blocks */
              if (!obmcFlag) {
                if (fourMVmb) {
                  h263_DecodeMCBlockInter(pInfo, quant, cbpy & 8, pYr, pYc, stepYr, coeffMB, &mvCur[0], rt);
                  h263_DecodeMCBlockInter(pInfo, quant, cbpy & 4, pYr+8, pYc+8, stepYr, coeffMB, &mvCur[1], rt);
                  h263_DecodeMCBlockInter(pInfo, quant, cbpy & 2, pYr+stepYr*8, pYc+stepYc*8, stepYr, coeffMB, &mvCur[2], rt);
                  h263_DecodeMCBlockInter(pInfo, quant, cbpy & 1, pYr+8+stepYr*8, pYc+8+stepYc*8, stepYr, coeffMB, &mvCur[3], rt);
                } else if (cbpy) {
                  h263_DecodeMCBlockInter(pInfo, quant, cbpy & 8, pYr, pYc, stepYr, coeffMB, &mvCur[0], rt);
                  h263_DecodeMCBlockInter(pInfo, quant, cbpy & 4, pYr+8, pYc+8, stepYr, coeffMB, &mvCur[0], rt);
                  h263_DecodeMCBlockInter(pInfo, quant, cbpy & 2, pYr+stepYr*8, pYc+stepYc*8, stepYr, coeffMB, &mvCur[0], rt);
                  h263_DecodeMCBlockInter(pInfo, quant, cbpy & 1, pYr+8+stepYr*8, pYc+8+stepYc*8, stepYr, coeffMB, &mvCur[0], rt);
                } else {
                  h263_Copy16x16HP_8u(pYr, stepYr, pYc, stepYc, &mvCur[0], rt);
                  h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                  h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                  h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                  h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                }
                h263_DecodeMCBlockInter(pInfo, quant_c, cbpc & 2, pCbr, pCbc, stepCbr, coeffMB, &mvCbCr, rt);
                h263_DecodeMCBlockInter(pInfo, quant_c, cbpc & 1, pCrr, pCrc, stepCrr, coeffMB, &mvCbCr, rt);

                if (pbFlag) {
                  Ipp8u *pRefY, *pRefCb, *pRefCr;
                  Ipp32s stepLum, stepChr;

                  if (bmb_type != IPPVC_MBTYPE_FORWARD) {
                    if (bmb_type != IPPVC_MBTYPE_BACKWARD) {

                      h263_BidirPredMacroblock(pYr, stepYr, pCbr, pCrr, stepCbr, pYc, stepYc, pCbc, pCrc, stepCbc,
                                              predMB, &mvForw[0], &mvFCbCr, &mvBack[0], &mvBCbCr, fourMVmb);

                      pRefY  =  predMB;         stepLum = 16;
                      pRefCb =  predMB + 4*64;  stepChr = 8;
                      pRefCr =  predMB + 5*64;
                    } else {
                      pRefY  =  pYc;   stepLum = stepYc;
                      pRefCb =  pCbc;  stepChr = stepCbc;
                      pRefCr =  pCrc;
                    }
                    h263_DecodeMCBlockInter_B(pInfo, bquant, cbpb & 32, pRefY, stepLum, pYn, stepYn, coeffMB_b);
                    h263_DecodeMCBlockInter_B(pInfo, bquant, cbpb & 16, pRefY+8, stepLum, pYn+8, stepYn, coeffMB_b);
                    h263_DecodeMCBlockInter_B(pInfo, bquant, cbpb & 8, pRefY+8*stepLum, stepLum, pYn+8*stepYn, stepYn, coeffMB_b);
                    h263_DecodeMCBlockInter_B(pInfo, bquant, cbpb & 4, pRefY+8+8*stepLum, stepLum, pYn+8+8*stepYn, stepYn, coeffMB_b);
                    h263_DecodeMCBlockInter_B(pInfo, bquant_c, cbpb & 2, pRefCb, stepChr, pCbn, stepCbn, coeffMB_b);
                    h263_DecodeMCBlockInter_B(pInfo, bquant_c, cbpb & 1, pRefCr, stepChr, pCrn, stepCrn, coeffMB_b);

                  } else {
                    if (cbpb >> 2) {
                      h263_DecodeMCBlockInter(pInfo, bquant, cbpb & 32, pYr, pYn, stepYr, coeffMB_b, &mvForw[0], 0);
                      h263_DecodeMCBlockInter(pInfo, bquant, cbpb & 16, pYr+8, pYn+8, stepYr, coeffMB_b, &mvForw[0], 0);
                      h263_DecodeMCBlockInter(pInfo, bquant, cbpb & 8, pYr+stepYr*8, pYn+stepYn*8, stepYr, coeffMB_b, &mvForw[0], 0);
                      h263_DecodeMCBlockInter(pInfo, bquant, cbpb & 4, pYr+8+stepYr*8, pYn+8+stepYn*8, stepYr, coeffMB_b, &mvForw[0], 0);
                    } else {
                      h263_Copy16x16HP_8u(pYr, stepYr, pYn, stepYn, &mvForw[0], 0);
                      h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                      h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                      h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                      h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC);
                    }
                    h263_DecodeMCBlockInter(pInfo, bquant_c, cbpb & 2, pCbr, pCbn, stepCbr, coeffMB_b, &mvFCbCr, 0);
                    h263_DecodeMCBlockInter(pInfo, bquant_c, cbpb & 1, pCrr, pCrn, stepCrr, coeffMB_b, &mvFCbCr, 0);
                  }
                }
              }
            }
          }

         if (obmcFlag) {
            /* OBMC for previous MB */
            if (colNum > 0) {
              if ((pMBinfo[-1].type & 0x7F) != IPPVC_MBTYPE_INTRA && (pMBinfo[-1].type & 0x7F) != IPPVC_MBTYPE_INTRA_Q) {
                /* type may have been ORed with 0x80 */
                h263_OBMC(pInfo, pMBinfo - 1, mvPrev, colNum - 1, rowNum, limitRectL, pYc - 16, stepYc, pYr - 16,
                          stepYr, cbpyPrev, coeffMB);

                if (pbFlag) {
                  if (pMBinfo[-1].type == IPPVC_MB_STUFFING) {
                    IppiSize roi;
                    roi.height = roi.width = 16;
                    ippiCopy16x16_8u_C1R(pYc - 16, stepYc, pYn - 16, stepYn);
                    ippiAddBackPredPB_H263_8u_C1R(pYr - 16, stepYr, roi, pYn - 16, stepYn, 0);
                    roi.height = roi.width = 8;
                    ippiCopy8x8_8u_C1R(pCbc - 8, stepCbc, pCbn - 8, stepCbn);
                    ippiAddBackPredPB_H263_8u_C1R(pCbr - 8, stepCbr, roi, pCbn - 8, stepCbn, 0);
                    ippiCopy8x8_8u_C1R(pCrc - 8, stepCrc, pCrn - 8, stepCrn);
                    ippiAddBackPredPB_H263_8u_C1R(pCrr - 8, stepCrr, roi, pCrn - 8, stepCrn, 0);
                  } else {
                    Ipp8u *pRefY, *pRefCb, *pRefCr;
                    Ipp32s  stepLum, stepChr;
                    Ipp32s prev4MVmb = (pMBinfo[-1].type == IPPVC_MBTYPE_INTER4V || pMBinfo[-1].type == IPPVC_MBTYPE_INTER4V_Q);

                    if (bmb_typePrev != IPPVC_MBTYPE_FORWARD) {
                      if (bmb_typePrev != IPPVC_MBTYPE_BACKWARD) {

                        h263_BidirPredMacroblock(pYr - 16, stepYr, pCbr - 8, pCrr - 8, stepCbr, pYc - 16, stepYc,
                                                 pCbc - 8, pCrc - 8, stepCbc,
                                                 predMB, &mvForwPrev[0], &mvFCbCrPrev, &mvBackPrev[0], &mvBCbCrPrev, prev4MVmb);

                        pRefY  =  predMB;         stepLum = 16;
                        pRefCb =  predMB + 4*64;  stepChr = 8;
                        pRefCr =  predMB + 5*64;
                      } else {
                        pRefY  =  pYc - 16;  stepLum = stepYc;
                        pRefCb =  pCbc - 8;  stepChr = stepCbc;
                        pRefCr =  pCrc - 8;
                      }
                      h263_MC(cbpbPrev & 32, pRefY, stepLum, pYn-16, stepYn, coeffMB_b);
                      h263_MC(cbpbPrev & 16, pRefY+8, stepLum, pYn-16+8, stepYn, coeffMB_b + 64);
                      h263_MC(cbpbPrev & 8, pRefY+8*stepLum, stepLum, pYn-16+8*stepYn, stepYn, coeffMB_b + 2*64);
                      h263_MC(cbpbPrev & 4, pRefY+8+8*stepLum, stepLum, pYn-16+8+8*stepYn, stepYn, coeffMB_b + 3*64);
                      h263_MC(cbpbPrev & 2, pRefCb, stepChr, pCbn-8, stepCbn, coeffMB_b + 4*64);
                      h263_MC(cbpbPrev & 1, pRefCr, stepChr, pCrn-8, stepCrn, coeffMB_b + 5*64);
                    } else {
                      h263_MC_HP(cbpbPrev & 32, pYr-16, stepYr, pYn-16, stepYn, coeffMB_b, &mvForwPrev[0], 0);
                      h263_MC_HP(cbpbPrev & 16, pYr-16+8, stepYr, pYn-16+8, stepYn, coeffMB_b + 64, &mvForwPrev[0], 0);
                      h263_MC_HP(cbpbPrev & 8, pYr-16+8*stepYr, stepYr, pYn-16+8*stepYn, stepYn, coeffMB_b + 2*64, &mvForwPrev[0], 0);
                      h263_MC_HP(cbpbPrev & 4, pYr-16+8+8*stepYr, stepYr, pYn-16+8+8*stepYn, stepYn, coeffMB_b + 3*64, &mvForwPrev[0], 0);
                      h263_MC_HP(cbpbPrev & 2, pCbr-8, stepCbr, pCbn-8, stepCbn, coeffMB_b + 4*64, &mvFCbCrPrev, 0);
                      h263_MC_HP(cbpbPrev & 1, pCrr-8, stepCrr, pCrn-8, stepCrn, coeffMB_b + 5*64, &mvFCbCrPrev, 0);
                    }
                  }
                }
              }
            }
            if (mb_type != IPPVC_MBTYPE_INTRA && mb_type != IPPVC_MBTYPE_INTRA_Q) {
              if (mb_not_coded) {
                cbpyPrev = 0;
                h263_Zero4MV(mvPrev);
                ippiCopy8x8_8u_C1R(pCbr, stepCbr, pCbc, stepCbc);
                ippiCopy8x8_8u_C1R(pCrr, stepCrr, pCrc, stepCrc);
              } else {
                cbpyPrev = cbpy;
                mvPrev[0] = mvCur[0]; mvPrev[1] = mvCur[1]; mvPrev[2] = mvCur[2]; mvPrev[3] = mvCur[3];
                if (h263_DecodeMacroBlockInter(pInfo, coeffMB, quant, quant_c, (cbpy << 2) + cbpc) != H263_STATUS_OK) {
                  h263_Error("Error when decoding coefficients of Inter block");
                  goto Err_1;
                }
                h263_MC_HP(cbpc & 2, pCbr, stepCbr, pCbc, stepCbc, coeffMB+256, &mvCbCr, rt);
                h263_MC_HP(cbpc & 1, pCrr, stepCrr, pCrc, stepCrc, coeffMB+320, &mvCbCr, rt);

                if (pbFlag) {
                  cbpbPrev = cbpb;
                  bmb_typePrev = bmb_type;
                  if (bmb_type == -1 || bmb_type == IPPVC_MBTYPE_INTERPOLATE) {
                    mvBackPrev[0] = mvBack[0];
                    if (fourMVmb) {
                      mvBackPrev[1] = mvBack[1]; mvBackPrev[2] = mvBack[2]; mvBackPrev[3] = mvBack[3];
                      mvForwPrev[1] = mvForw[1]; mvForwPrev[2] = mvForw[2]; mvForwPrev[3] = mvForw[3];
                    }
                    mvBCbCrPrev = mvBCbCr;
                  }
                  mvForwPrev[0] = mvForw[0];
                  mvFCbCrPrev = mvFCbCr;

                  if (h263_DecodeMacroBlockInter(pInfo, coeffMB_b, bquant, bquant_c, cbpb) != H263_STATUS_OK) {
                    h263_Error("Error when decoding coefficients of Inter block");
                    goto Err_1;
                  }
                }
              }

              if (colNum == mbPerRow - 1) {
                h263_OBMC(pInfo, pMBinfo, mvPrev, colNum, rowNum, limitRectL, pYc, stepYc, pYr, stepYr, cbpyPrev, coeffMB);

                if (pbFlag) {
                  Ipp8u *pRefY, *pRefCb, *pRefCr;
                  if (mb_not_coded) {
                    IppiSize roi;
                    roi.height = roi.width = 16;
                    ippiCopy16x16_8u_C1R(pYc, stepYc, pYn, stepYn);
                    ippiAddBackPredPB_H263_8u_C1R(pYr, stepYr, roi, pYn, stepYn, 0);
                    roi.height = roi.width = 8;
                    ippiCopy8x8_8u_C1R(pCbc, stepCbc, pCbn, stepCbn);
                    ippiAddBackPredPB_H263_8u_C1R(pCbr, stepCbr, roi, pCbn, stepCbn, 0);
                    ippiCopy8x8_8u_C1R(pCrc, stepCrc, pCrn, stepCrn);
                    ippiAddBackPredPB_H263_8u_C1R(pCrr, stepCrr, roi, pCrn, stepCrn, 0);
                  } else {
                    Ipp32s  stepLum, stepChr;
                    if (bmb_typePrev != IPPVC_MBTYPE_FORWARD) {
                      if (bmb_typePrev != IPPVC_MBTYPE_BACKWARD) {

                        h263_BidirPredMacroblock(pYr, stepYr, pCbr, pCrr, stepCbr, pYc, stepYc, pCbc, pCrc, stepCbc,
                                                 predMB, &mvForwPrev[0], &mvFCbCrPrev, &mvBackPrev[0], &mvBCbCrPrev, fourMVmb);

                        pRefY  =  predMB;         stepLum = 16;
                        pRefCb =  predMB + 4*64;  stepChr = 8;
                        pRefCr =  predMB + 5*64;
                      } else {
                        pRefY  =  pYc;  stepLum = stepYc;
                        pRefCb =  pCbc;  stepChr = stepCbc;
                        pRefCr =  pCrc;
                      }
                      h263_MC(cbpbPrev & 32, pRefY, stepLum, pYn, stepYn, coeffMB_b);
                      h263_MC(cbpbPrev & 16, pRefY+8, stepLum, pYn+8, stepYn, coeffMB_b + 64);
                      h263_MC(cbpbPrev & 8, pRefY+8*stepLum, stepLum, pYn+8*stepYn, stepYn, coeffMB_b + 2*64);
                      h263_MC(cbpbPrev & 4, pRefY+8+8*stepLum, stepLum, pYn+8+8*stepYn, stepYn, coeffMB_b + 3*64);
                      h263_MC(cbpbPrev & 2, pRefCb, stepChr, pCbn, stepCbn, coeffMB_b + 4*64);
                      h263_MC(cbpbPrev & 1, pRefCr, stepChr, pCrn, stepCrn, coeffMB_b + 5*64);

                    } else {
                      h263_MC_HP(cbpbPrev & 32, pYr, stepYr, pYn, stepYn, coeffMB_b, &mvForwPrev[0], 0);
                      h263_MC_HP(cbpbPrev & 16, pYr+8, stepYr, pYn+8, stepYn, coeffMB_b + 64, &mvForwPrev[0], 0);
                      h263_MC_HP(cbpbPrev & 8, pYr+8*stepYr, stepYr, pYn+8*stepYn, stepYn, coeffMB_b + 2*64, &mvForwPrev[0], 0);
                      h263_MC_HP(cbpbPrev & 4, pYr+8+8*stepYr, stepYr, pYn+8+8*stepYn, stepYn, coeffMB_b + 3*64, &mvForwPrev[0], 0);
                      h263_MC_HP(cbpbPrev & 2, pCbr, stepCbr, pCbn, stepCbn, coeffMB_b + 4*64, &mvFCbCrPrev, 0);
                      h263_MC_HP(cbpbPrev & 1, pCrr, stepCrr, pCrn, stepCrn, coeffMB_b + 5*64, &mvFCbCrPrev, 0);
                    }
                  }
                }
              }
            }
          }

          h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB);

          if (VPic->oppmodes.deblockFilt && !mb_not_coded) {
            H263_MB_INTERNAL_HOR_DEBLOCKING_LUM(pYc, stepYc, quant);
          }

          pYc += 16; pCrc += 8; pCbc += 8;
          pYr += 16; pCrr += 8; pCbr += 8;
          if (pbFlag) {
            pYn += 16; pCrn += 8; pCbn += 8;
          }
          pMBinfo++;
        }
        pYc += 2 * H263_NUM_EXT_MB * 16 + (stepYc << 4) - stepYc;
        pCbc += 2 * H263_NUM_EXT_MB * 8 + (stepCbc << 3) - stepCbc;
        pCrc += 2 * H263_NUM_EXT_MB * 8 + (stepCrc << 3) - stepCrc;
        pYr += 2 * H263_NUM_EXT_MB * 16 + (stepYr << 4) - stepYr;
        pCbr += 2 * H263_NUM_EXT_MB * 8 + (stepCbr << 3) - stepCbr;
        pCrr += 2 * H263_NUM_EXT_MB * 8 + (stepCrr << 3) - stepCrr;
        if (pbFlag) {
          pYn += 2 * H263_NUM_EXT_MB * 16 + (stepYn << 4) - stepYn;
          pCbn += 2 * H263_NUM_EXT_MB * 8 + (stepCbn << 3) - stepCbn;
          pCrn += 2 * H263_NUM_EXT_MB * 8 + (stepCrn << 3) - stepCrn;
        }
      }
    }
    h263_AlignBits(pInfo);

    /* Deblocking filtering */
    if (VPic->oppmodes.deblockFilt)
      h263_DeblockingFilter_P(pInfo, &pInfo->VideoSequence.cFrame);

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
}

#ifdef _OMP_KARABAS

static h263_Status h263_DecodeFrame_P_DecodeSlice(h263_Info* pInfo, Ipp32s curRow, h263_MacroBlockMT* pMBinfoMT)
{
  Ipp32s          quant, quant_c, bquant, bquant_c;
  Ipp32s          mb_not_coded, mb_type = 0, cbpc = 0, cbpy = 0;
  Ipp32s          mbPerRow;
  h263_MacroBlock *pMBinfo;
  IppMotionVector mvForwPred;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s cbpb = 0, mvdb, bmb_type = 0;
  Ipp32s predMode = 0;
  h263_Status status;
  Ipp32s k;
  Ipp32s fourMVmode = VPic->modes.advPred | VPic->oppmodes.advPred | VPic->oppmodes.deblockFilt;
  Ipp32s pbFlag = VPic->picture_coding_type == H263_PIC_TYPE_PB || VPic->picture_coding_type == H263_PIC_TYPE_iPB;
  Ipp32s trd = VPic->temporal_reference - VPic->prev_temporal_reference;
  Ipp32s trb = VPic->temporal_reference_B;
  Ipp32s frGOB;
  h263_IntraPredBlock *b = pInfo->VideoSequence.IntraPredBuff.block;
  Ipp32s colNum;
  Ipp32s fourMVmb;

#ifndef UMC_H263_DISABLE_SORENSON
  fourMVmode |= pInfo->sorenson;
#endif

  if (trd < 0)
    trd += VPic->PCF ? 1024 : 256;

  mbPerRow = VPic->MacroBlockPerRow;
  pMBinfo = pInfo->VideoSequence.MBinfo + curRow * mbPerRow;

  frGOB = pInfo->VideoSequence.GOBboundary[curRow] ? curRow : 0;

  quant = quant_c = VPic->pic_quant;
  if (VPic->oppmodes.modQuant)
    quant_c = h263_quant_c[quant];

  if (VPic->picture_coding_type == H263_PIC_TYPE_PB || VPic->picture_coding_type == H263_PIC_TYPE_iPB) {
    bquant_c = bquant = (VPic->dbquant + 5)*quant >> 2;
    h263_CLIPR(bquant, 31);
    if (VPic->oppmodes.modQuant)
      bquant_c = h263_quant_c[bquant];
  }

  for (colNum = 0; colNum < mbPerRow; colNum++) {
    do {
      mb_not_coded = h263_GetBit(pInfo);
      if (mb_not_coded)
        break;
      if (h263_DecodeMCBPC_P(pInfo, &mb_type, &cbpc, 1) != H263_STATUS_OK)
        return H263_STATUS_ERROR;
    } while (mb_type == IPPVC_MB_STUFFING);

    if (mb_not_coded) {

      mb_type = pMBinfo->type = IPPVC_MB_STUFFING; /* to indicate mb_not_coded */

      h263_Zero4MV(pMBinfo->mv);
      mvForwPred.dx = mvForwPred.dy = 0;
      if (VPic->oppmodes.advIntra) {
        for (k = 0; k < 6; k++)
          b[(colNum + 1)*6 + k].dct_dc = -1;
      }
      h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB_NOTCODED);
    } else {

      if (VPic->oppmodes.advIntra && (mb_type == IPPVC_MBTYPE_INTRA || mb_type == IPPVC_MBTYPE_INTRA_Q))
        h263_AdvI_PredMode(pInfo, predMode);

      mvdb = cbpb = 0;
      if (VPic->picture_coding_type == H263_PIC_TYPE_PB) {
        if (h263_GetBit(pInfo)) {
          mvdb = 1;
          cbpb = h263_GetBit(pInfo);
        } else {
          pMBinfoMT->mvF[0].dx = pMBinfoMT->mvF[0].dy = pMBinfoMT->mvB[0].dx = pMBinfoMT->mvB[0].dy = 0;
        }
        bmb_type = -1;
      } else if (VPic->picture_coding_type == H263_PIC_TYPE_iPB) {
        h263_DecodeMODB_iPB(pInfo, &bmb_type, &cbpb, &mvdb);
        if (!mvdb || colNum == 0)
          mvForwPred.dx = mvForwPred.dy = 0;
      }
      if (cbpb)
        cbpb = h263_GetBits9(pInfo, 6);

      if (VPic->oppmodes.altInterVLC && cbpc == 3) {
        if (h263_DecodeCBPY_I(pInfo, &cbpy) != H263_STATUS_OK)
          return H263_STATUS_ERROR;
      } else {
        if (h263_DecodeCBPY_P(pInfo, &cbpy, mb_type) != H263_STATUS_OK)
          return H263_STATUS_ERROR;
      }

      pMBinfo->type = (Ipp8u)mb_type;
      pMBinfoMT->cbpc = cbpc;
      pMBinfoMT->cbpy = cbpy;
      pMBinfoMT->cbpb = cbpb;
      pMBinfoMT->bmb_type = bmb_type;
      fourMVmb = (mb_type == IPPVC_MBTYPE_INTER4V || mb_type == IPPVC_MBTYPE_INTER4V_Q);

      if (mb_type == IPPVC_MBTYPE_INTRA_Q || mb_type == IPPVC_MBTYPE_INTER_Q || mb_type == IPPVC_MBTYPE_INTER4V_Q) {
        if (!VPic->oppmodes.modQuant) {
          h263_UpdateQuant(pInfo, VPic->pic_quant);
          quant_c = VPic->pic_quant;
        } else {
          quant_c = h263_UpdateQuant_Mod(pInfo);
        }
        quant = VPic->pic_quant;
        if (VPic->picture_coding_type == H263_PIC_TYPE_PB || VPic->picture_coding_type == H263_PIC_TYPE_iPB) {
          bquant_c = bquant = (VPic->dbquant + 5)*quant >> 2;
          h263_CLIPR(bquant, 31);
          if (VPic->oppmodes.modQuant)
            bquant_c = h263_quant_c[bquant];
        }
      }
      pMBinfo->quant = (Ipp8u)quant;

      if (mb_type == IPPVC_MBTYPE_INTRA || mb_type == IPPVC_MBTYPE_INTRA_Q) {
        if (VPic->picture_coding_type == H263_PIC_TYPE_PB || (VPic->picture_coding_type == H263_PIC_TYPE_iPB && bmb_type == IPPVC_MBTYPE_INTERPOLATE)) {
          if (h263_PredictDecodeMV(pInfo, pMBinfo, frGOB, curRow, colNum, fourMVmode) != H263_STATUS_OK) {
            h263_Error("Error when decoding motion vector");
            return H263_STATUS_ERROR;
          }
          if (fourMVmode) {
            pMBinfo->mv[1] = pMBinfo->mv[2] = pMBinfo->mv[3] = pMBinfo->mv[0];
          }
        } else {
          if (!fourMVmode)
            pMBinfo->mv[0].dx = pMBinfo->mv[0].dy = 0;
          else {
            h263_Zero4MV(pMBinfo->mv);
            pMBinfo->type |= 0x80;
          }
        }

        if (pbFlag && bmb_type != IPPVC_MBTYPE_BACKWARD) {
          if (h263_PredictDecode4MV_PB(pInfo, &pMBinfo->mv[0], &pMBinfoMT->mvF[0], &pMBinfoMT->mvB[0], &mvForwPred,
              bmb_type, mvdb, trd, trb, 0) != H263_STATUS_OK) {
            h263_Error("Error when decoding MVDB");
            return H263_STATUS_ERROR;
          }
        }

        status = h263_DecodeDequantMacroBlockIntra(pInfo, colNum, (cbpy << 2) + cbpc, quant, quant_c, predMode,
                                                   pMBinfoMT->dctCoeffs, pMBinfoMT->lnz);
        if (status != H263_STATUS_OK) {
          h263_Error("Error when decoding coefficients of Intra block");
          return H263_STATUS_ERROR;
        }

        if (pbFlag) {
          status = h263_DecodeDequantMacroBlockInter(pInfo, cbpb, bquant, bquant_c,
                                                     pMBinfoMT->dctCoeffs_B, &pMBinfoMT->lnz[6]);
          if (status != H263_STATUS_OK) {
            h263_Error("Error when decoding coefficients of a B-block");
            return H263_STATUS_ERROR;
          }
        }
      } else { /* INTER MB */

        if (VPic->oppmodes.advIntra) {
          for (k = 0; k < 6; k++)
            b[(colNum + 1)*6 + k].dct_dc = -1;
        }

        if (!fourMVmb) {
          if (h263_PredictDecodeMV(pInfo, pMBinfo, frGOB, curRow, colNum, fourMVmode) != H263_STATUS_OK) {
            h263_Error("Error when decoding motion vector");
            return H263_STATUS_ERROR;
          }
          if (fourMVmode) {
            pMBinfo->mv[1] = pMBinfo->mv[2] = pMBinfo->mv[3] = pMBinfo->mv[0];
          }

          if (pbFlag && bmb_type != IPPVC_MBTYPE_BACKWARD) {
            if (h263_PredictDecode4MV_PB(pInfo, &pMBinfo->mv[0], &pMBinfoMT->mvF[0], &pMBinfoMT->mvB[0], &mvForwPred,
                bmb_type, mvdb, trd, trb, 0) != H263_STATUS_OK) {
              h263_Error("Error when decoding MVDB");
              return H263_STATUS_ERROR;
            }
          }
        } else {
          if (h263_PredictDecode4MV(pInfo, pMBinfo, frGOB, curRow, colNum) != H263_STATUS_OK) {
            h263_Error("Error when decoding motion vectors");
            return H263_STATUS_ERROR;
          }

          if (pbFlag && bmb_type != IPPVC_MBTYPE_BACKWARD) {
            if (h263_PredictDecode4MV_PB(pInfo, &pMBinfo->mv[0], &pMBinfoMT->mvF[0], &pMBinfoMT->mvB[0], &mvForwPred,
                bmb_type, mvdb, trd, trb, 1) != H263_STATUS_OK) {
              h263_Error("Error when decoding MVDB");
              return H263_STATUS_ERROR;
            }
          }
        }

        status = h263_DecodeDequantMacroBlockInter(pInfo, (cbpy << 2) | cbpc, quant, quant_c,
                                                    pMBinfoMT->dctCoeffs, &pMBinfoMT->lnz[0]);
        if (status != H263_STATUS_OK) {
          h263_Error("Error when decoding coefficients of a P-block");
          return H263_STATUS_ERROR;
        }

        if (pbFlag) {
          status = h263_DecodeDequantMacroBlockInter(pInfo, cbpb, bquant, bquant_c,
                                                     pMBinfoMT->dctCoeffs_B, &pMBinfoMT->lnz[6]);
          if (status != H263_STATUS_OK) {
            h263_Error("Error when decoding coefficients of a B-block");
            return H263_STATUS_ERROR;
          }
        }

      }
    }

    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB);
    pMBinfo++;
    pMBinfoMT++;
  }
  // skip stuffing
  while (h263_ShowBits(pInfo, 10) == 1)
    h263_FlushBits(pInfo, 10);
  return H263_STATUS_OK;
}

/*********************************************************************************/

static void h263_DecodeFrame_P_ReconSlice(h263_Info* pInfo, Ipp32s curRow, h263_MacroBlockMT* pMBinfoMT, h263_Frame *rFrame)
{
  __ALIGN16(Ipp8u, predMB, 64*6);
  Ipp32s          i, j, dx, dy;
  Ipp32s          stepYr, stepYc, stepCbr, stepCbc, stepCrr, stepCrc, mbPerRow;
  Ipp32s          stepF[6];
  Ipp8u           *pYc, *pCbc, *pCrc, *pYr, *pCbr, *pCrr, *pF[6];
  Ipp8u           *pYn, *pCbn, *pCrn;
  Ipp32s          stepYn, stepCbn, stepCrn;
  Ipp32s          mb_not_coded, mb_type, cbpc, cbpy;
  h263_MacroBlock *pMBinfo;
  Ipp32s          rt;
  IppiRect        limitRectL, limitRectC;
  IppMotionVector mvCur[4], mvCbCr;
  IppMotionVector *mvBack, *mvForw, mvFCbCr, mvBCbCr;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s cbpb, bmb_type;
  Ipp32s fourMVmode = VPic->modes.advPred | VPic->oppmodes.advPred | VPic->oppmodes.deblockFilt;
  Ipp32s obmcFlag = VPic->modes.advPred | VPic->oppmodes.advPred;
  Ipp32s pbFlag = VPic->picture_coding_type == H263_PIC_TYPE_PB || VPic->picture_coding_type == H263_PIC_TYPE_iPB;
  Ipp16s* coeffMB;
  Ipp32s fourMVmb;

#ifndef UMC_H263_DISABLE_SORENSON
  fourMVmode |= pInfo->sorenson;
#endif

  stepYc = pInfo->VideoSequence.cFrame.stepY;
  stepYr = rFrame->stepY;
  stepCbc = pInfo->VideoSequence.cFrame.stepCb;
  stepCbr = rFrame->stepCb;
  stepCrc = pInfo->VideoSequence.cFrame.stepCr;
  stepCrr = rFrame->stepCr;


  pYc = pInfo->VideoSequence.cFrame.pY + curRow*16*stepYc;
  pCbc = pInfo->VideoSequence.cFrame.pCb + curRow*8*stepCbc;
  pCrc = pInfo->VideoSequence.cFrame.pCr + curRow*8*stepCrc;
  pYr = rFrame->pY + curRow*16*stepYr;
  pCbr = rFrame->pCb + curRow*8*stepCbr;
  pCrr = rFrame->pCr + curRow*8*stepCrr;
  stepF[0] = stepF[1] = stepF[2] = stepF[3] = stepYc; stepF[4] = stepCbc; stepF[5] = stepCrc;

  if (pbFlag) {
    stepYn = pInfo->VideoSequence.nFrame.stepY;
    stepCbn = pInfo->VideoSequence.nFrame.stepCb;
    stepCrn = pInfo->VideoSequence.nFrame.stepCr;
    pYn = pInfo->VideoSequence.nFrame.pY + curRow*16*stepYn;
    pCbn = pInfo->VideoSequence.nFrame.pCb + curRow*8*stepCbn;
    pCrn = pInfo->VideoSequence.nFrame.pCr + curRow*8*stepCrn;
  }

  mbPerRow = VPic->MacroBlockPerRow;
  dy = curRow * 16;
  dx = 0;

  // Bounding rectangles for MV limitation
  limitRectL.x = - 16;
  limitRectL.y = - 16;
  limitRectL.width = VPic->MacroBlockPerRow * 16 + 32;
  limitRectL.height = VPic->MacroBlockPerCol * 16 + 32;
  limitRectC.x = -8;
  limitRectC.y = -8;
  limitRectC.width = VPic->MacroBlockPerRow * 8 + 16;
  limitRectC.height = VPic->MacroBlockPerCol * 8 + 16;

  pMBinfo = pInfo->VideoSequence.MBinfo + curRow * mbPerRow;

  rt = VPic->rtype;

  for (j = 0; j < mbPerRow; j ++) {
    mb_not_coded = (pMBinfo->type == IPPVC_MB_STUFFING);
    mb_type = pMBinfo->type;
    coeffMB = pMBinfoMT->dctCoeffs;
    cbpy = pMBinfoMT->cbpy;
    cbpc = pMBinfoMT->cbpc;

    fourMVmb = (mb_type == IPPVC_MBTYPE_INTER4V || mb_type == IPPVC_MBTYPE_INTER4V_Q);

    if ((mb_type & 0x7F) == IPPVC_MBTYPE_INTRA || (mb_type & 0x7F) == IPPVC_MBTYPE_INTRA_Q) {
      pF[0] = pYc; pF[1] = pYc + 8; pF[2] = pYc + 8 * stepYc; pF[3] = pYc + 8 * stepYc + 8; pF[4] = pCbc; pF[5] = pCrc;
      for (i = 0; i < 6; i ++) {
        if (pMBinfoMT->lnz[i] > 0)
          ippiDCT8x8Inv_16s8u_C1R(&coeffMB[i*64], pF[i], stepF[i]);
        else
          h263_Set8x8_8u(pF[i], stepF[i], (Ipp8u)((coeffMB[i*64] + 4) >> 3));
      }

    } else  {
      if (mb_not_coded && !obmcFlag) {
        ippiCopy16x16_8u_C1R(pYr, stepYr, pYc, stepYc);
        ippiCopy8x8_8u_C1R(pCbr, stepCbr, pCbc, stepCbc);
        ippiCopy8x8_8u_C1R(pCrr, stepCrr, pCrc, stepCrc);
        if (pbFlag) {
          ippiCopy16x16_8u_C1R(pYr, stepYr, pYn, stepYn);
          ippiCopy8x8_8u_C1R(pCbr, stepCbr, pCbn, stepCbn);
          ippiCopy8x8_8u_C1R(pCrr, stepCrr, pCrn, stepCrn);
        }
      } else {
        if (!mb_not_coded) {
          if (!fourMVmb) {
            h263_LimitMV(pMBinfo->mv, mvCur, &limitRectL, dx, dy, 16);
            h263_ComputeChromaMV(mvCur, &mvCbCr);
            if (fourMVmode) {
              mvCur[1] = mvCur[2] = mvCur[3] = mvCur[0];
            }
          } else {
            h263_Limit4MV(pMBinfo->mv, mvCur, &limitRectL, dx, dy, 8);
            h263_ComputeChroma4MV(pMBinfo->mv, &mvCbCr);
            h263_LimitMV(&mvCbCr, &mvCbCr, &limitRectC, dx >> 1, dy >> 1, 8);
          }
          h263_DCT_MacroBlockInter(coeffMB, pMBinfoMT->lnz, (cbpy << 2) + cbpc);


          if (!obmcFlag) {
            if (fourMVmb) {
              h263_MC_HP(cbpy & 8, pYr, stepYr, pYc, stepYc, coeffMB, &mvCur[0], rt);
              h263_MC_HP(cbpy & 4, pYr+8, stepYr, pYc+8, stepYc, coeffMB + 64, &mvCur[1], rt);
              h263_MC_HP(cbpy & 2, pYr+stepYr*8, stepYr, pYc+stepYc*8, stepYc, coeffMB + 2*64, &mvCur[2], rt);
              h263_MC_HP(cbpy & 1, pYr+8+stepYr*8, stepYr, pYc+8+stepYc*8, stepYc, coeffMB + 3*64, &mvCur[3], rt);
            } else if (cbpy) {
              h263_MC_HP(cbpy & 8, pYr, stepYr, pYc, stepYc, coeffMB, &mvCur[0], rt);
              h263_MC_HP(cbpy & 4, pYr+8, stepYr, pYc+8, stepYc, coeffMB + 64, &mvCur[0], rt);
              h263_MC_HP(cbpy & 2, pYr+stepYr*8, stepYr, pYc+stepYc*8, stepYc, coeffMB + 2*64, &mvCur[0], rt);
              h263_MC_HP(cbpy & 1, pYr+8+stepYr*8, stepYr, pYc+8+stepYc*8, stepYc, coeffMB + 3*64, &mvCur[0], rt);
            } else {
              h263_Copy16x16HP_8u(pYr, stepYr, pYc, stepYc, &mvCur[0], rt);
            }
            h263_MC_HP(cbpc & 2, pCbr, stepCbr, pCbc, stepCbc, coeffMB + 4*64, &mvCbCr, rt);
            h263_MC_HP(cbpc & 1, pCrr, stepCrr, pCrc, stepCrc, coeffMB + 5*64, &mvCbCr, rt);
          }
        }
        if (obmcFlag) {
          if (mb_not_coded) {
            ippiCopy8x8_8u_C1R(pCbr, stepCbr, pCbc, stepCbc);
            ippiCopy8x8_8u_C1R(pCrr, stepCrr, pCrc, stepCrc);
            cbpy = 0;
            h263_Zero4MV(mvCur);
          } else {
            h263_MC_HP(cbpc & 2, pCbr, stepCbr, pCbc, stepCbc, coeffMB+256, &mvCbCr, rt);
            h263_MC_HP(cbpc & 1, pCrr, stepCrr, pCrc, stepCrc, coeffMB+320, &mvCbCr, rt);
          }
          h263_OBMC(pInfo, pMBinfo, mvCur, j, curRow, limitRectL, pYc, stepYc, pYr, stepYr, cbpy, coeffMB);
        }
      }
    }

    if (pbFlag) {
      if (mb_not_coded) {
        if (obmcFlag) {
          IppiSize roi;
          roi.height = roi.width = 16;
          ippiCopy16x16_8u_C1R(pYc, stepYc, pYn, stepYn);
          ippiAddBackPredPB_H263_8u_C1R(pYr, stepYr, roi, pYn, stepYn, 0);
          roi.height = roi.width = 8;
          ippiCopy8x8_8u_C1R(pCbc, stepCbc, pCbn, stepCbn);
          ippiAddBackPredPB_H263_8u_C1R(pCbr, stepCbr, roi, pCbn, stepCbn, 0);
          ippiCopy8x8_8u_C1R(pCrc, stepCrc, pCrn, stepCrn);
          ippiAddBackPredPB_H263_8u_C1R(pCrr, stepCrr, roi, pCrn, stepCrn, 0);
        }
      } else {
        Ipp8u *pRefY, *pRefCb, *pRefCr;
        Ipp32s  stepLum, stepChr;

        cbpb = pMBinfoMT->cbpb;
        bmb_type = pMBinfoMT->bmb_type;
        mvBack = pMBinfoMT->mvB;
        mvForw = pMBinfoMT->mvF;

        if (bmb_type != IPPVC_MBTYPE_BACKWARD) {
          if (bmb_type == IPPVC_MBTYPE_FORWARD) {
            h263_LimitMV(mvForw, mvForw, &limitRectL, dx, dy, 16);
            h263_ComputeChromaMV(&mvForw[0], &mvFCbCr);
          } else {
            if (!fourMVmb) {
              h263_LimitMV(mvForw, mvForw, &limitRectL, dx, dy, 16);
              h263_LimitMV(mvBack, mvBack, &limitRectL, dx, dy, 16);
              h263_ComputeChromaMV(&mvBack[0], &mvBCbCr);
              h263_ComputeChromaMV(&mvForw[0], &mvFCbCr);
            } else {
              h263_Limit4MV(mvForw, mvForw, &limitRectL, dx, dy, 8);
              h263_Limit4MV(mvBack, mvBack, &limitRectL, dx, dy, 8);
              h263_ComputeChroma4MV(&mvForw[0], &mvFCbCr);
              h263_LimitMV(&mvFCbCr, &mvFCbCr, &limitRectC, dx >> 1, dy >> 1, 8);
              h263_ComputeChroma4MV(&mvBack[0], &mvBCbCr);
              h263_LimitMV(&mvBCbCr, &mvBCbCr, &limitRectC, dx >> 1, dy >> 1, 8);
            }
          }
        }

        h263_DCT_MacroBlockInter(pMBinfoMT->dctCoeffs_B, &pMBinfoMT->lnz[6], cbpb);

        if (bmb_type != IPPVC_MBTYPE_FORWARD) {
          if (bmb_type != IPPVC_MBTYPE_BACKWARD) {

            h263_BidirPredMacroblock(pYr, stepYr, pCbr, pCrr, stepCbr, pYc, stepYc, pCbc, pCrc, stepCbc,
                                    predMB, &mvForw[0], &mvFCbCr, &mvBack[0], &mvBCbCr, fourMVmb);

            pRefY  =  predMB;         stepLum = 16;
            pRefCb =  predMB + 4*64;  stepChr = 8;
            pRefCr =  predMB + 5*64;
          } else {
            pRefY  =  pYc;   stepLum = stepYc;
            pRefCb =  pCbc;  stepChr = stepCbc;
            pRefCr =  pCrc;
          }
          h263_MC(cbpb & 32, pRefY, stepLum, pYn, stepYn, pMBinfoMT->dctCoeffs_B);
          h263_MC(cbpb & 16, pRefY+8, stepLum, pYn+8, stepYn, pMBinfoMT->dctCoeffs_B + 64);
          h263_MC(cbpb & 8, pRefY+8*stepLum, stepLum, pYn+8*stepYn, stepYn, pMBinfoMT->dctCoeffs_B + 2*64);
          h263_MC(cbpb & 4, pRefY+8+8*stepLum, stepLum, pYn+8+8*stepYn, stepYn, pMBinfoMT->dctCoeffs_B + 3*64);
          h263_MC(cbpb & 2, pRefCb, stepChr, pCbn, stepCbn, pMBinfoMT->dctCoeffs_B + 4*64);
          h263_MC(cbpb & 1, pRefCr, stepChr, pCrn, stepCrn, pMBinfoMT->dctCoeffs_B + 5*64);
        } else {
          h263_MC_HP(cbpb & 32, pYr, stepYr, pYn, stepYn, pMBinfoMT->dctCoeffs_B, &mvForw[0], 0);
          h263_MC_HP(cbpb & 16, pYr+8, stepYr, pYn+8, stepYn, pMBinfoMT->dctCoeffs_B + 64, &mvForw[0], 0);
          h263_MC_HP(cbpb & 8, pYr+stepYr*8, stepYr, pYn+stepYn*8, stepYn, pMBinfoMT->dctCoeffs_B + 2*64, &mvForw[0], 0);
          h263_MC_HP(cbpb & 4, pYr+8+stepYr*8, stepYr, pYn+8+stepYn*8, stepYn, pMBinfoMT->dctCoeffs_B + 3*64, &mvForw[0], 0);
          h263_MC_HP(cbpb & 2, pCbr, stepCbr, pCbn, stepCbn, pMBinfoMT->dctCoeffs_B + 4*64, &mvFCbCr, 0);
          h263_MC_HP(cbpb & 1, pCrr, stepCrr, pCrn, stepCrn, pMBinfoMT->dctCoeffs_B + 5*64, &mvFCbCr, 0);
        }
      }
    }

    if (VPic->oppmodes.deblockFilt) {
      Ipp32s quant;
      if (!mb_not_coded) {
        quant = pMBinfo->quant;
        H263_MB_INTERNAL_HOR_DEBLOCKING_LUM(pYc, stepYc, quant);
      }
    }

    pMBinfo++;
    pMBinfoMT++;
    pYc += 16; pCrc += 8; pCbc += 8;
    pYr += 16; pCrr += 8; pCbr += 8;
    if (pbFlag) {
      pYn += 16; pCrn += 8; pCbn += 8;
    }
    dx += 16;
  }
}

/*********************************************************************************/

h263_Status h263_DecodeFrame_P_MT(h263_Info* pInfo, h263_Frame *rFrame)
{
  Ipp32s     i;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s     mbPerCol = VPic->MacroBlockPerCol;
  Ipp32s     mbPerRow = VPic->MacroBlockPerRow;
  h263_Status sts = H263_STATUS_OK;
  Ipp32s num_rows_per_gob = VPic->RowsPerGOB;
  h263_IntraPredBlock *b;
  Ipp32s     sBitOff, sQuant;
  Ipp8u      *sBufPtr;

  sBufPtr = pInfo->bufptr;
  sBitOff = pInfo->bitoff;
  sQuant  = pInfo->VideoSequence.VideoPicture.pic_quant;

  // reset Intra prediction buffer
  b = pInfo->VideoSequence.IntraPredBuff.block;
  for (i = 0; i <= mbPerRow; i ++) {
    b[i*6+0].dct_dc = b[i*6+1].dct_dc = b[i*6+2].dct_dc = b[i*6+3].dct_dc = b[i*6+4].dct_dc = b[i*6+5].dct_dc = -1;
  }
  i = 0;

  ippsZero_8u(pInfo->VideoSequence.GOBboundary, mbPerCol);

#ifdef _OPENMP
#pragma  omp parallel shared(pInfo, i, mbPerCol, mbPerRow, sts) num_threads(pInfo->number_threads)
#endif
  {
    Ipp32s             idThread = 0;   /* the thread id of the calling thread. */
    Ipp32s             curRow;
    h263_MacroBlockMT* pMBinfoMT;

#ifdef _OPENMP
    idThread = omp_get_thread_num();
    vm_set_current_thread_priority(pInfo->mTreadPriority);
#endif
    pMBinfoMT = pInfo->pMBinfoMT + mbPerRow * idThread;
    curRow = i;

    for (;;) {
#ifdef _OPENMP
#pragma omp critical(DECODE_SLICE)
#endif
      {
        Ipp32s gob_header_present;
        Ipp32s k;

        curRow = i;
        i++;

        if (curRow < mbPerCol) {
          if (curRow) {
            VPic->gob_number = curRow;
            gob_header_present = h263_ParseGOBHeader(pInfo);

            if (gob_header_present < 0)
              sts = H263_STATUS_PARSE_ERROR;
            else if (gob_header_present) {
              if (VPic->gob_number > 30)
                curRow = mbPerCol; /* EOS or EOSBS */
              else {
                if (VPic->gob_number * num_rows_per_gob - curRow > 0) {
                  ippsSet_8u(127, pInfo->VideoSequence.GOBboundary + curRow, VPic->gob_number * num_rows_per_gob - curRow);
                  curRow = VPic->gob_number * num_rows_per_gob;
                }
                pInfo->VideoSequence.GOBboundary[curRow] = 1;
              }
            }
            if (gob_header_present && VPic->oppmodes.advIntra) {
              for (k = 0; k <= mbPerRow; k++) {
                b[k*6+0].dct_dc = b[k*6+1].dct_dc = b[k*6+2].dct_dc = b[k*6+3].dct_dc = b[k*6+4].dct_dc = b[k*6+5].dct_dc = -1;
              }
            }
          }
          if (sts == H263_STATUS_OK)
            if (h263_DecodeFrame_P_DecodeSlice(pInfo, curRow, pMBinfoMT) != H263_STATUS_OK) {
              i = curRow = mbPerCol;
              sts = H263_STATUS_ERROR;
            }
        }
      }
      if ((curRow >= mbPerCol) || (sts != H263_STATUS_OK))
        break;
      h263_DecodeFrame_P_ReconSlice(pInfo, curRow, pMBinfoMT, rFrame);
    }
  }

  if (sts != H263_STATUS_OK && !pInfo->stopOnErr) {
    pInfo->bufptr = sBufPtr;
    pInfo->bitoff = sBitOff;
    pInfo->VideoSequence.VideoPicture.pic_quant = sQuant;

    sts = h263_DecodeFrame_P(pInfo, rFrame);
  } else {
    h263_AlignBits(pInfo);
    if (VPic->oppmodes.deblockFilt) {
      Ipp32s curRow;
#ifdef _OPENMP
#pragma  omp parallel for shared(pInfo, mbPerCol) /* num_threads(pInfo->number_threads) */
#endif
      for (curRow = 1; curRow < mbPerCol; curRow++) {
        h263_DeblockingFilterHor_P_MT(pInfo, curRow);
      }

#ifdef _OPENMP
#pragma  omp parallel for shared(pInfo, mbPerCol) /* num_threads(pInfo->number_threads) */
#endif
      for (curRow = 0; curRow < mbPerCol; curRow++) {
        h263_DeblockingFilterVer_P_MT(pInfo, curRow);
      }
    }
  }
  return sts;
}

#endif /* _OMP_KARABAS */
