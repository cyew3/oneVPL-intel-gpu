//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2011 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#include "ippi.h"
#include "umc_mpeg2_enc_defs.h"
#include "mfx_timing.h"

using namespace UMC;

void MPEG2VideoEncoderBase::NonIntraMBCoeffs(Ipp32s numTh, MB_prediction_info *prediction_info, Ipp32s *Count, Ipp32s *pCodedBlockPattern)
{
  Ipp16s *pDiff = prediction_info->pDiff;
  Ipp32s *var = prediction_info->var;
  Ipp32s *mean = prediction_info->mean;
  Ipp16s *pMBlock = threadSpec[numTh].pMBlock;
  const Ipp32s *dct_step, *diff_off;
  Ipp32s loop;
  Ipp32s CodedBlockPattern = 0;

  switch(nLimitedMBMode)
  {
   case 3:
   case 2:
     *pCodedBlockPattern = 0;
     return;
  }
  if (prediction_info->dct_type == DCT_FRAME) {
    dct_step = frm_dct_step;
    diff_off = frm_diff_off;
  } else {
    dct_step = fld_dct_step;
    diff_off = fld_diff_off;
  }

  for (loop = 0; loop < block_count; loop++) {
    CodedBlockPattern = (CodedBlockPattern << 1);
    if (loop < 4 )
      ippiVarSum8x8_16s32s_C1R(pDiff + diff_off[loop], dct_step[loop], &var[loop], &mean[loop]);
    if (loop < 4 && var[loop] < varThreshold && abs(mean[loop]) < meanThreshold) {
      Count[loop] = 0;
    } else {
      ippiDCT8x8Fwd_16s_C1R(pDiff + diff_off[loop], dct_step[loop], pMBlock);

#ifndef UMC_RESTRICTED_CODE
      //if (quantiser_scale_value > 100) {
      //  Ipp32s freq = 4;
      //  Ipp32s i;
      //  if(quantiser_scale_value == 112) freq = 2;
      //  for(; freq<8; freq++) {
      //    pMBlock[freq*9] = 0;
      //    for(i=0;i<freq;i++)
      //      pMBlock[freq*8+i] = pMBlock[i*8+freq] = 0;
      //  }
      //}
#endif // UMC_RESTRICTED_CODE
      ippiQuant_MPEG2_16s_C1I(pMBlock, quantiser_scale_value, InvNonIntraQMatrix, &Count[loop]);
      if (Count[loop] != 0) CodedBlockPattern++;
    }
    pMBlock += 64;
  }

  *pCodedBlockPattern = CodedBlockPattern;

}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Coding current picture ////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void MPEG2VideoEncoderBase::encodePicture(Ipp32s numTh)
{
    MFX::AutoTimer timer("MPEG2Enc_work");
    threadSpec[numTh].sts = UMC_OK;

    try
    {
        switch (picture_coding_type) {
        case MPEG2_I_PICTURE:
          encodeI(numTh);
          break;
        case MPEG2_P_PICTURE:
          encodeP(numTh);
    #ifndef UMC_RESTRICTED_CODE
          //meP(numTh);
          //putP(numTh);
    #endif // UMC_RESTRICTED_CODE
          break;
        case MPEG2_B_PICTURE:
          encodeB(numTh);
    #ifndef UMC_RESTRICTED_CODE
          //meB(numTh);
          //putB(numTh);
    #endif // UMC_RESTRICTED_CODE
          break;
        }
    }
    catch (MPEG2_Exceptions excep)
    {
        threadSpec[numTh].sts = excep.GetType();
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Coding I picture ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void MPEG2VideoEncoderBase::encodeI(Ipp32s numTh)
{
  Ipp32s i, j, ic, jc, k;
  Ipp32s start_y;
  Ipp32s start_uv;
  Ipp32s stop_y;
  threadSpec[numTh].fieldCount = 0;

  CALC_START_STOP_ROWS

  for(j=start_y, jc= start_uv; j < stop_y; j += 16, jc += BlkHeight_c)
  {
    Ipp32s dc_init = ResetTbl[intra_dc_precision];
    Ipp32s dc_dct_pred[3] = {dc_init, dc_init, dc_init};
    //Ipp32s tmp_dc_dct_pred[2][3] = {{dc_init, dc_init, dc_init}, {dc_init, dc_init, dc_init}};

    PutSliceHeader(j >> 4, numTh);

    for(i=ic=0; i < encodeInfo.info.clip_info.width; i += 16, ic += BlkWidth_c)
    {
      const Ipp8u *BlockSrc[3];
      Ipp8u       *BlockRec[3];
      Ipp32s cur_offset_src   = i  + j  * YFramePitchSrc;
      Ipp32s cur_offset_c_src = ic + jc * UVFramePitchSrc;
      Ipp32s cur_offset_rec   = i  + j  * YFramePitchRec;
      Ipp32s cur_offset_c_rec = ic + jc * UVFramePitchRec;

      if (encodeInfo.info.color_format == NV12)
      {
        cur_offset_c_src *= 2;
        cur_offset_c_rec *= 2;

      }

      BlockSrc[0] = Y_src + cur_offset_src;
      BlockSrc[1] = U_src + cur_offset_c_src;
      BlockSrc[2] = V_src + cur_offset_c_src;
      BlockRec[0] = YRecFrame[curr_field][1] + cur_offset_rec;
      BlockRec[1] = URecFrame[curr_field][1] + cur_offset_c_rec;
      BlockRec[2] = VRecFrame[curr_field][1] + cur_offset_c_rec;

      pMBInfo[k].prediction_type = MC_FRAME;
      pMBInfo[k].mb_type         = MB_INTRA;
      pMBInfo[k].dct_type        = DCT_FRAME;

#ifndef UMC_RESTRICTED_CODE
//ippiVariance16x16_8u32s(BlockSrc[0], YFrameHSize, &pMBInfo[k].var_sum);
//      Ipp32s size;
//size = BITPOS(threadSpec[numTh]);
#endif // UMC_RESTRICTED_CODE

      if (!curr_frame_dct) {
        Ipp32s var_fld = 0, var = 0;
        ippiFrameFieldSAD16x16_8u32s_C1R(BlockSrc[0], YFramePitchSrc, &var, &var_fld);

        if (var_fld < var) {
          pMBInfo[k].dct_type = DCT_FIELD;
          threadSpec[numTh].fieldCount ++;
        } else {
          threadSpec[numTh].fieldCount --;
        }
      }

#ifndef UMC_RESTRICTED_CODE
      //Ipp32s sz0 = IntraMBSize(k, BlockSrc, tmp_dc_dct_pred[0], 1, curr_scan, 0);
      //Ipp32s sz1 = IntraMBSize(k, BlockSrc, tmp_dc_dct_pred[1], 1, curr_scan, 1);
#endif // UMC_RESTRICTED_CODE

      // macroblock in I picture cannot be skipped
      PutAddrIncrement(1 , numTh);

      PutIntraMacroBlock(numTh, k, BlockSrc, BlockRec, dc_dct_pred);
#ifndef UMC_RESTRICTED_CODE
//size = BITPOS(threadSpec[numTh]) - size;
//pMBInfo[k].pred_quality[0] = size;
#endif // UMC_RESTRICTED_CODE

      k++;
    } //for(i)
  } //for(j)
}

#ifndef UMC_RESTRICTED_CODE
void MPEG2VideoEncoderBase::analyzeI(Ipp32s numTh)
{
  Ipp32s i, j, ic, jc, k;
  Ipp32s start_y;
  Ipp32s start_uv;
  Ipp32s stop_y;
  threadSpec[numTh].fieldCount = 0;
#ifndef UMC_RESTRICTED_CODE
  threadSpec[numTh].sizeVLCtab0 = 0;
  threadSpec[numTh].sizeVLCtab1 = 0;

  //Ipp16s *blocks = ippsMalloc_16s(numMB*block_count*64);
  //Ipp16s *blk = blocks;
  //Ipp32s *numCoeffs = ippsMalloc_32s(numMB*block_count);
  //Ipp32s *nums = numCoeffs;
  //Ipp32s sz0 = 0, sz1 = 0;
#endif // UMC_RESTRICTED_CODE

  CALC_START_STOP_ROWS

  for(j=start_y, jc= start_uv; j < stop_y; j += 16, jc += BlkHeight_c) {
    //Ipp32s dc_init = ResetTbl[intra_dc_precision];
    //Ipp32s dc_dct_pred[2][3] = {{dc_init, dc_init, dc_init}, {dc_init, dc_init, dc_init}};

    for(i=ic=0; i < encodeInfo.info.clip_info.width; i += 16, ic += BlkWidth_c)
    {
      const Ipp8u *BlockSrc[3];
      Ipp32s cur_offset_src   = i  + j  * YFramePitchSrc;
      Ipp32s cur_offset_c_src = ic + jc * UVFramePitchSrc;
      if (encodeInfo.info.color_format == NV12)
        cur_offset_c_src *= 2;

      BlockSrc[0] = Y_src + cur_offset_src;
      BlockSrc[1] = U_src + cur_offset_c_src;
      BlockSrc[2] = V_src + cur_offset_c_src;

      pMBInfo[k].prediction_type = MC_FRAME;
      pMBInfo[k].mb_type         = MB_INTRA;
      pMBInfo[k].dct_type        = DCT_FRAME;

      if (!curr_frame_dct) {
        Ipp32s var_fld = 0, var = 0;
        ippiFrameFieldSAD16x16_8u32s_C1R(BlockSrc[0], YFramePitchSrc, &var, &var_fld);

        if (var_fld < var) {
          pMBInfo[k].dct_type = DCT_FIELD;
          threadSpec[numTh].fieldCount ++;
        } else {
          threadSpec[numTh].fieldCount --;
        }
      }

#ifndef UMC_RESTRICTED_CODE
      //sz0 += IntraMBSize(k, BlockSrc, dc_dct_pred[0], 1, curr_scan, 0);
      //sz1 += IntraMBSize(k, BlockSrc, dc_dct_pred[1], 1, curr_scan, 1);
#endif // UMC_RESTRICTED_CODE

      k++;
    } //for(i)
  } //for(j)
#ifndef UMC_RESTRICTED_CODE
  //threadSpec[numTh].sizeVLCtab0 = sz0;
  //threadSpec[numTh].sizeVLCtab1 = sz1;
#endif // UMC_RESTRICTED_CODE
}
#endif // UMC_RESTRICTED_CODE

#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER
