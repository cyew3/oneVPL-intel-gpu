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
//  Purpose:  decode H.263 EI-Frame
*/
h263_Status h263_DecodeFrame_EI(h263_Info* pInfo, h263_EnhancedLayer *Layer)
{
  __ALIGN16(Ipp16s, coeffMB, 64*2);
  Ipp8u           *pYc, *pCbc, *pCrc;
  Ipp32s          stepYc, stepCbc, stepCrc;
  Ipp32s          stepYr, stepCbr, stepCrr, mbPerRow, mbPerCol, stepF[6];
  Ipp8u           *pYr, *pCbr, *pCrr, *pF[6];
  Ipp32s          colNum = 0, rowNum = 0;
  Ipp32s          mb_type = 0, cbpy, cbpc = 0;
  Ipp32s          quant, quant_c;
  h263_MacroBlock *pMBinfo;
  Ipp32s          frGOB;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s gob_header_present = 1;
  Ipp32s predMode = 0;
  h263_Status status;
  Ipp32s num_rows_per_gob, num_gobs;
  Ipp32s i, k;
  h263_IntraPredBlock  *b = pInfo->VideoSequence.IntraPredBuff.block;
  Ipp32s mb_not_coded;
  h263_Status sts;

  mbPerRow = VPic->MacroBlockPerRow;
  mbPerCol = VPic->MacroBlockPerCol;

  stepYc = Layer->c_Frame.stepY;
  stepYr = Layer->l_Frame->stepY;
  stepCbc = Layer->c_Frame.stepCb;
  stepCbr = Layer->l_Frame->stepCb;
  stepCrc = Layer->c_Frame.stepCr;
  stepCrr = Layer->l_Frame->stepCr;
  pYc = Layer->c_Frame.pY;
  pCbc = Layer->c_Frame.pCb;
  pCrc = Layer->c_Frame.pCr;
  pYr = Layer->l_Frame->pY;
  pCbr = Layer->l_Frame->pCb;
  pCrr = Layer->l_Frame->pCr;
  stepF[0] = stepF[1] = stepF[2] = stepF[3] = stepYc; stepF[4] = stepCbc; stepF[5] = stepCrc;

  quant = VPic->pic_quant;
  pMBinfo = pInfo->VideoSequence.MBinfo;

  quant = VPic->pic_quant;
  VPic->gob_number = 0;
  num_gobs = VPic->num_gobs_in_pic;
  num_rows_per_gob = VPic->RowsPerGOB;

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
        pYr = Layer->l_Frame->pY + (stepYr << 4) * rowNum;
        pCbr = Layer->l_Frame->pCb + (stepCbr << 3) * rowNum;
        pCrr = Layer->l_Frame->pCr + (stepCrr << 3) * rowNum;
        pMBinfo = pInfo->VideoSequence.MBinfo + rowNum * mbPerRow;
        ippsSet_8u(127, pInfo->VideoSequence.GOBboundary + i*num_rows_per_gob, (VPic->gob_number - i) * num_rows_per_gob);
        i = VPic->gob_number;
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

    for (rowNum = 0; rowNum < num_rows_per_gob; rowNum++) {
      for (colNum = 0; colNum < mbPerRow; colNum++) {
        do {
          mb_not_coded = h263_GetBit(pInfo);
          if (mb_not_coded)
            break;
          if (h263_DecodeMCBPC_EI(pInfo, &mb_type, &cbpc) != H263_STATUS_OK)
            goto Err_1;
        } while (mb_type == IPPVC_MB_STUFFING);

        if (mb_not_coded) {

          ippiCopy16x16_8u_C1R(pYr, stepYr, pYc, stepYc);
          ippiCopy8x8_8u_C1R(pCbr, stepCbr, pCbc, stepCbc);
          ippiCopy8x8_8u_C1R(pCrr, stepCrr, pCrc, stepCrc);

          mb_type = pMBinfo->type = IPPVC_MB_STUFFING; /* to indicate mb_not_coded */
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

          if (h263_DecodeCBPY_I(pInfo, &cbpy) != H263_STATUS_OK)
            goto Err_1;

          if (mb_type == IPPVC_MBTYPE_INTRA_Q || mb_type == IPPVC_MBTYPE_INTER_Q) {
            if (!VPic->oppmodes.modQuant) {
              h263_UpdateQuant(pInfo, VPic->pic_quant);
              quant_c = VPic->pic_quant;
            } else {
              quant_c = h263_UpdateQuant_Mod(pInfo);
            }
            quant = VPic->pic_quant;
          }

          if (mb_type == IPPVC_MBTYPE_INTRA || mb_type == IPPVC_MBTYPE_INTRA_Q) {
            pF[0] = pYc; pF[1] = pYc + 8; pF[2] = pYc + 8 * stepYc; pF[3] = pYc + 8 * stepYc + 8; pF[4] = pCbc; pF[5] = pCrc;
            if (!VPic->oppmodes.advIntra)
              status = h263_DecodeMacroBlockIntra(pInfo, (cbpy << 2) + cbpc, quant, quant_c, pF, stepF);
            else
              status = h263_DecodeMacroBlockIntra_AdvI(pInfo, colNum, (cbpy << 2) + cbpc, quant, quant_c, predMode, pF, stepF);
            if (status != H263_STATUS_OK) {
              h263_Error("Error when decoding coefficients of Intra block");
              goto Err_1;
            }
          } else { /* UPWARD predicted MB */
            if (cbpy) {
              h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 8, pYr, stepYr, pYc, stepYc, coeffMB);
              h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 4, pYr+8, stepYr, pYc+8, stepYc, coeffMB);
              h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 2, pYr+8*stepYr, stepYr, pYc+8*stepYc, stepYc, coeffMB);
              h263_DecodeMCBlockInter_B(pInfo, quant, cbpy & 1, pYr+8+8*stepYr, stepYr, pYc+8+8*stepYc, stepYc, coeffMB);
            } else
              ippiCopy16x16_8u_C1R(pYr, stepYr, pYc, stepYc);
            h263_DecodeMCBlockInter_B(pInfo, quant_c, cbpc & 2, pCbr, stepCbr, pCbc, stepCbc, coeffMB);
            h263_DecodeMCBlockInter_B(pInfo, quant_c, cbpc & 1, pCrr, stepCrr, pCrc, stepCrc, coeffMB);
          }
        }

        if (VPic->oppmodes.deblockFilt) {
          H263_MB_INTERNAL_HOR_DEBLOCKING_LUM(pYc, stepYc, quant);
        }

        pYc += 16; pCrc += 8; pCbc += 8;
        pYr += 16; pCrr += 8; pCbr += 8;
        pMBinfo++;
      }
      pYc += 2 * H263_NUM_EXT_MB * 16 + (stepYc << 4) - stepYc;
      pCbc += 2 * H263_NUM_EXT_MB * 8 + (stepCbc << 3) - stepCbc;
      pCrc += 2 * H263_NUM_EXT_MB * 8 + (stepCrc << 3) - stepCrc;
      pYr += 2 * H263_NUM_EXT_MB * 16 + (stepYr << 4) - stepYr;
      pCbr += 2 * H263_NUM_EXT_MB * 8 + (stepCbr << 3) - stepCbr;
      pCrr += 2 * H263_NUM_EXT_MB * 8 + (stepCrr << 3) - stepCrr;
    }
  }

  h263_AlignBits(pInfo);

  /* Deblocking filtering */
  if (VPic->oppmodes.deblockFilt)
    h263_DeblockingFilter_I(pInfo, &Layer->c_Frame);

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
