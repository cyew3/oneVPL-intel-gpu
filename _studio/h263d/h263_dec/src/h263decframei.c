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
//  decode mcbpc and set MBtype and ChromaPattern
*/
static h263_Status h263_DecodeMCBPC_I(h263_Info* pInfo, Ipp32s *mbType, Ipp32s *mbPattern)
{
  Ipp32u      code;
  Ipp32s      type, pattern, fb;

  code = h263_ShowBits9(pInfo, 9);
  if (code == 1) {
    type = IPPVC_MB_STUFFING;
    pattern = 0;
    fb = 9;
  } else if (code >= 64) {
    type = IPPVC_MBTYPE_INTRA;
    pattern = code >> 6;
    if (pattern >= 4) {
      pattern = 0;
      fb = 1;
    } else
      fb = 3;
  } else {
    type = IPPVC_MBTYPE_INTRA_Q;
    pattern = code >> 3;
    if (pattern >= 4) {
      pattern = 0;
      fb = 4;
    } else if (code >= 8) {
      fb = 6;
    } else {
      h263_Error("Error when decoding mcbpc of I-Frame macroblock");
      return H263_STATUS_ERROR;
    }
  }
  h263_FlushBits(pInfo, fb);
  *mbType = type;
  *mbPattern = pattern;
  if (type == IPPVC_MBTYPE_INTRA)
    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB_INTRA);
  else if (type == IPPVC_MBTYPE_INTRA_Q)
    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB_INTRA_Q);
  return H263_STATUS_OK;
}

/*
//  decode I-Frame
*/

h263_Status h263_DecodeFrame_I(h263_Info* pInfo)
{
  Ipp32s  quant, quant_c, i, j = 0, mbPerRow, mbPerCol, mb_type, cbpc, cbpy, predMode = 0;
  Ipp32s  stepYc, stepCbc, stepCrc, stepF[6];
  Ipp32s  num_gobs, num_rows_per_gob, rowNum = 0, row;
  Ipp8u  *pF[6];
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s gob_header_present = 1;
  Ipp32s k;
  h263_MacroBlock *pMBinfo = pInfo->VideoSequence.MBinfo;
  h263_Status sts;
  h263_IntraPredBlock *b = pInfo->VideoSequence.IntraPredBuff.block;

  stepYc = pInfo->VideoSequence.cFrame.stepY;
  stepCbc = pInfo->VideoSequence.cFrame.stepCb;
  stepCrc = pInfo->VideoSequence.cFrame.stepCr;
  stepYc = pInfo->VideoSequence.cFrame.stepY;
  mbPerRow = pInfo->VideoSequence.VideoPicture.MacroBlockPerRow;
  mbPerCol = pInfo->VideoSequence.VideoPicture.MacroBlockPerCol;

  num_gobs = pInfo->VideoSequence.VideoPicture.num_gobs_in_pic;
  num_rows_per_gob = pInfo->VideoSequence.VideoPicture.RowsPerGOB;

  stepF[0] = stepF[1] = stepF[2] = stepF[3] = stepYc; stepF[4] = stepCbc; stepF[5] = stepCrc;
  pF[0] = pInfo->VideoSequence.cFrame.pY; pF[1] = pInfo->VideoSequence.cFrame.pY + 8;
  pF[2] = pInfo->VideoSequence.cFrame.pY + 8 * stepYc; pF[3] = pInfo->VideoSequence.cFrame.pY + 8 * stepYc + 8;
  pF[4] = pInfo->VideoSequence.cFrame.pCb; pF[5] = pInfo->VideoSequence.cFrame.pCr;

  VPic->gob_number = 0;
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
        Ipp32s nummbs2copy = (VPic->gob_number * num_rows_per_gob - rowNum) * mbPerRow - j;
        h263_CopyMacroBlocks(&pInfo->VideoSequence.rFrame, &pInfo->VideoSequence.cFrame, mbPerRow, rowNum, j, nummbs2copy);
        rowNum = num_rows_per_gob * VPic->gob_number;
        j = 0;
        pF[0] = pInfo->VideoSequence.cFrame.pY + (stepYc << 4) * rowNum;
        pF[1] = pF[0] + 8;
        pF[2] = pF[0] + 8 * stepYc;
        pF[3] = pF[2] + 8;
        pF[4] = pInfo->VideoSequence.cFrame.pCb + 8 * stepCbc * rowNum;
        pF[5] = pInfo->VideoSequence.cFrame.pCr + 8 * stepCrc * rowNum;
        pMBinfo = pInfo->VideoSequence.MBinfo + rowNum * mbPerRow;
        ippsSet_8u(127, pInfo->VideoSequence.GOBboundary + i*num_rows_per_gob, (VPic->gob_number - i) * num_rows_per_gob);
        i = VPic->gob_number;
        pInfo->VideoSequence.GOBboundary[i*num_rows_per_gob] = 1;
      }
    }

    h263_CLIPR(num_rows_per_gob, (mbPerCol - i*num_rows_per_gob));

    quant = quant_c = VPic->pic_quant;
    if (VPic->oppmodes.modQuant)
      quant_c = h263_quant_c[quant];

    if (gob_header_present && VPic->oppmodes.advIntra) {
      for (k = 0; k <= mbPerRow; k++) {
        b[k*6+0].dct_dc = b[k*6+1].dct_dc = b[k*6+2].dct_dc = b[k*6+3].dct_dc = b[k*6+4].dct_dc = b[k*6+5].dct_dc = -1;
      }
    }

    for (row = 0; row < num_rows_per_gob; row++) {
      rowNum = i*num_rows_per_gob + row;
      for (j = 0; j < mbPerRow; j++) {
        do {
          if (h263_DecodeMCBPC_I(pInfo, &mb_type, &cbpc) != H263_STATUS_OK)
            goto Err_1;
        } while (mb_type == IPPVC_MB_STUFFING);

        if (VPic->oppmodes.advIntra)
          h263_AdvI_PredMode(pInfo, predMode);

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

        if (VPic->oppmodes.advIntra)
          sts = h263_DecodeMacroBlockIntra_AdvI(pInfo, j, (cbpy << 2) + cbpc, quant, quant_c, predMode, pF, stepF);
        else
          sts = h263_DecodeMacroBlockIntra(pInfo, (cbpy << 2) + cbpc, quant, quant_c, pF, stepF);
        if (sts != H263_STATUS_OK) {
          h263_Error("Error when decoding coefficients of Intra block");
          goto Err_1;
        }

        if (VPic->oppmodes.deblockFilt) {
          H263_MB_INTERNAL_HOR_DEBLOCKING_LUM(pF[0], stepF[0], quant);
        }

        pF[0] += 16; pF[1] += 16; pF[2] += 16; pF[3] += 16; pF[4] += 8; pF[5] += 8;
        VPic->pic_quant = quant;
        pMBinfo->quant = (Ipp8u)quant;
        pMBinfo++;
        h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB);
      }
      pF[0] += 2 * H263_NUM_EXT_MB * 16 + (stepYc << 4) - stepYc;
      pF[1] += 2 * H263_NUM_EXT_MB * 16 + (stepYc << 4) - stepYc;
      pF[2] += 2 * H263_NUM_EXT_MB * 16 + (stepYc << 4) - stepYc;
      pF[3] += 2 * H263_NUM_EXT_MB * 16 + (stepYc << 4) - stepYc;
      pF[4] += 2 * H263_NUM_EXT_MB * 8 + (stepCbc << 3) - stepCbc;
      pF[5] += 2 * H263_NUM_EXT_MB * 8 + (stepCrc << 3) - stepCrc;
    }
  }

  h263_AlignBits(pInfo);

  /* Deblocking filtering */
  if (VPic->oppmodes.deblockFilt)
    h263_DeblockingFilter_I(pInfo, &pInfo->VideoSequence.cFrame);

  return H263_STATUS_OK;

Err_1:
  sts = H263_STATUS_ERROR;
  if (pInfo->stopOnErr)
    return sts;
  if (!h263_SeekGOBStartCodePtr(pInfo)) {
    h263_CopyMacroBlocks(&pInfo->VideoSequence.rFrame, &pInfo->VideoSequence.cFrame, mbPerRow, rowNum, j, (mbPerCol - rowNum) * mbPerRow - j);
    return sts;
  }
  goto ErrRet_1;


}

#ifdef _OMP_KARABAS

static h263_Status h263_DecodeFrame_I_DecodeSlice(h263_Info* pInfo, Ipp32s curRow, h263_MacroBlockMT* pMBinfoMT)
{
  Ipp32s          quant, quant_c;
  Ipp32s          mb_type = 0, cbpc = 0, cbpy = 0;
  Ipp32s          mbPerRow;
  h263_MacroBlock *pMBinfo;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s predMode = 0;
  h263_Status status;
  Ipp32s colNum;

  mbPerRow = VPic->MacroBlockPerRow;
  pMBinfo = pInfo->VideoSequence.MBinfo + curRow * mbPerRow;

  quant = quant_c = VPic->pic_quant;
  if (VPic->oppmodes.modQuant)
    quant_c = h263_quant_c[quant];

  for (colNum = 0; colNum < mbPerRow; colNum++) {
    do {
      if (h263_DecodeMCBPC_I(pInfo, &mb_type, &cbpc) != H263_STATUS_OK)
        return H263_STATUS_ERROR;
    } while (mb_type == IPPVC_MB_STUFFING);

    if (VPic->oppmodes.advIntra)
      h263_AdvI_PredMode(pInfo, predMode);

    if (h263_DecodeCBPY_I(pInfo, &cbpy) != H263_STATUS_OK)
      return H263_STATUS_ERROR;

//    pMBinfo->type = (Ipp8u)mb_type;
    pMBinfoMT->cbpc = cbpc;
    pMBinfoMT->cbpy = cbpy;

    if (mb_type == IPPVC_MBTYPE_INTRA_Q) {
      if (!VPic->oppmodes.modQuant) {
        h263_UpdateQuant(pInfo, VPic->pic_quant);
        quant_c = VPic->pic_quant;
      } else {
        quant_c = h263_UpdateQuant_Mod(pInfo);
      }
      quant = VPic->pic_quant;
    }
    pMBinfo->quant = (Ipp8u)quant;

    status = h263_DecodeDequantMacroBlockIntra(pInfo, colNum, (cbpy << 2) + cbpc, quant, quant_c, predMode,
                                                pMBinfoMT->dctCoeffs, pMBinfoMT->lnz);
    if (status != H263_STATUS_OK) {
      h263_Error("Error when decoding coefficients of Intra block");
      return H263_STATUS_ERROR;
    }

    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nMB);
    pMBinfo++;
    pMBinfoMT++;
  }

  // skip stuffing
  while (h263_ShowBits(pInfo, 9) == 1)
    h263_FlushBits(pInfo, 9);
  return H263_STATUS_OK;
}

/*********************************************************************************/

static void h263_DecodeFrame_I_ReconSlice(h263_Info* pInfo, Ipp32s curRow, h263_MacroBlockMT* pMBinfoMT)
{
  Ipp32s          i, j;
  Ipp32s          stepYc, stepCbc, stepCrc, mbPerRow;
  Ipp32s          stepF[6];
  Ipp8u           *pYc, *pCbc, *pCrc, *pF[6];
  h263_MacroBlock *pMBinfo;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp16s* coeffMB;

  stepYc = pInfo->VideoSequence.cFrame.stepY;
  stepCbc = pInfo->VideoSequence.cFrame.stepCb;
  stepCrc = pInfo->VideoSequence.cFrame.stepCr;

  pYc = pInfo->VideoSequence.cFrame.pY + curRow*16*stepYc;
  pCbc = pInfo->VideoSequence.cFrame.pCb + curRow*8*stepCbc;
  pCrc = pInfo->VideoSequence.cFrame.pCr + curRow*8*stepCrc;
  stepF[0] = stepF[1] = stepF[2] = stepF[3] = stepYc; stepF[4] = stepCbc; stepF[5] = stepCrc;

  mbPerRow = VPic->MacroBlockPerRow;
  pMBinfo = pInfo->VideoSequence.MBinfo + curRow * mbPerRow;

  for (j = 0; j < mbPerRow; j++) {
    coeffMB = pMBinfoMT->dctCoeffs;

    pF[0] = pYc; pF[1] = pYc + 8; pF[2] = pYc + 8 * stepYc; pF[3] = pYc + 8 * stepYc + 8; pF[4] = pCbc; pF[5] = pCrc;
    for (i = 0; i < 6; i++) {
      if (pMBinfoMT->lnz[i] > 0)
        ippiDCT8x8Inv_16s8u_C1R(&coeffMB[i*64], pF[i], stepF[i]);
      else
        h263_Set8x8_8u(pF[i], stepF[i], (Ipp8u)((coeffMB[i*64] + 4) >> 3));
    }

    if (VPic->oppmodes.deblockFilt) {
      Ipp32s quant;
      quant = pMBinfo->quant;
      H263_MB_INTERNAL_HOR_DEBLOCKING_LUM(pYc, stepYc, quant);
    }

    pMBinfo++;
    pMBinfoMT++;
    pYc += 16; pCrc += 8; pCbc += 8;
  }
}

/*********************************************************************************/

h263_Status h263_DecodeFrame_I_MT(h263_Info* pInfo)
{
  Ipp32s     i;
  h263_VideoPicture *VPic = &pInfo->VideoSequence.VideoPicture;
  Ipp32s     mbPerCol = VPic->MacroBlockPerCol;
  Ipp32s     mbPerRow = VPic->MacroBlockPerRow;
  h263_Status sts = H263_STATUS_OK;
  Ipp32s num_rows_per_gob = VPic->RowsPerGOB;
  h263_IntraPredBlock *b;
  Ipp32s     sBitOff;
  Ipp8u      *sBufPtr;

  sBufPtr = pInfo->bufptr;
  sBitOff = pInfo->bitoff;

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
            if (h263_DecodeFrame_I_DecodeSlice(pInfo, curRow, pMBinfoMT) != H263_STATUS_OK) {
              i = curRow = mbPerCol;
              sts = H263_STATUS_ERROR;
            }
        }
      }
      if ((curRow >= mbPerCol) || (sts != H263_STATUS_OK))
        break;

      h263_DecodeFrame_I_ReconSlice(pInfo, curRow, pMBinfoMT);
    }
  }
  if (sts != H263_STATUS_OK && !pInfo->stopOnErr) {
    pInfo->bufptr = sBufPtr;
    pInfo->bitoff = sBitOff;
    sts = h263_DecodeFrame_I(pInfo);
  } else {
    h263_AlignBits(pInfo);
    if (VPic->oppmodes.deblockFilt) {
      Ipp32s curRow;
#ifdef _OPENMP
#pragma  omp parallel for shared(pInfo, mbPerCol) /* num_threads(pInfo->number_threads) */
#endif
      for (curRow = 1; curRow < mbPerCol; curRow++) {
        h263_DeblockingFilterHor_I_MT(pInfo, curRow);
      }

#ifdef _OPENMP
#pragma  omp parallel for shared(pInfo, mbPerCol) /* num_threads(pInfo->number_threads) */
#endif
      for (curRow = 0; curRow < mbPerCol; curRow++) {
        h263_DeblockingFilterVer_I_MT(pInfo, curRow);
      }
    }
  }
  return sts;
}
#endif
