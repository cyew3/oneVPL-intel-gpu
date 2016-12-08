//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2016 Intel Corporation. All Rights Reserved.
//

#ifndef UMC_RESTRICTED_CODE

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#include "ippi.h"
#include "ipps.h"
#include "umc_mpeg2_enc_defs.h"

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Coding B picture ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

using namespace UMC;

void MPEG2VideoEncoderBase::meB(Ipp32s numTh)
{
  const Ipp8u *pRef[2]; // parity same/opposite
  const Ipp8u *pRec[2]; // parity same/opposite
  Ipp32s      i, j, ic, jc, k, pred_ind;
  Ipp16s      *pDiff;
  IppMotionVector2   vector[3][2] = {0,}; // top/bottom/frame F/B
  MB_prediction_info pred_info[2];
  MB_prediction_info *best = pred_info;
  MB_prediction_info *curr = pred_info + 1;
  // bounds in half pixels, border included
  Ipp32s      me_bound_left[2], me_bound_right[2];
  Ipp32s      me_bound_top[2], me_bound_bottom[2];
  Ipp32s      me_bound_2_top[2], me_bound_1_bottom[2];
  const Ipp8u *YBlock;
  const Ipp8u *UBlock;
  const Ipp8u *VBlock;
  Ipp32s      mean_frm[4], mean_fld[4];
  Ipp32s      start_y;
  Ipp32s      start_uv;
  Ipp32s      stop_y;
  Ipp32s      skip_flag;
  Ipp32s      dir0, dir1;

  dir0 = 2*B_count > P_distance ? 1 : 0;
  dir1 = 1 ^ dir0;
  CALC_START_STOP_ROWS

  best->pDiff = threadSpec[numTh].pDiff;
  curr->pDiff = threadSpec[numTh].pDiff1;

  if (!bQuantiserChanged) {
    Ipp32s mf, mb;
    mb = B_count - P_distance;
    mf = B_count;
    for(j=start_y; j < stop_y; j += 16)
    {
      for(i=0; i < encodeInfo.info.clip_info.width; i += 16, k++)
      {
        pMBInfo[k].MV[0][0].x = pMBInfo[k].MV_P[0].x*mf/256;
        pMBInfo[k].MV[0][0].y = pMBInfo[k].MV_P[0].y*mf/256;
        pMBInfo[k].MV[0][1].x = pMBInfo[k].MV_P[0].x*mb/256;
        pMBInfo[k].MV[0][1].y = pMBInfo[k].MV_P[0].y*mb/256;
        pMBInfo[k].MV[1][0].x = pMBInfo[k].MV_P[1].x*mf/256;
        pMBInfo[k].MV[1][0].y = pMBInfo[k].MV_P[1].y*mf/256;
        pMBInfo[k].MV[1][1].x = pMBInfo[k].MV_P[1].x*mb/256;
        pMBInfo[k].MV[1][1].y = pMBInfo[k].MV_P[1].y*mb/256;
      }
    }
    k = (encodeInfo.numThreads > 1) ? (start_y/16)*MBcountH : 0;
  }

  for (j = start_y, jc = start_uv; j < stop_y; j += 16, jc += BlkHeight_c)
  {
    ippsZero_8u((Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));
    BOUNDS_V(0,j)
    BOUNDS_V(1,j)
    if(picture_structure != MPEG2_FRAME_PICTURE) {
      BOUNDS_V_FIELD(0,j)
      BOUNDS_V_FIELD(1,j)
      //// to avoid reference below bottom in 16x8 mode when trying to skip MB
      //me_bound_1_bottom[0] = me_bound_bottom[0];
      //me_bound_1_bottom[1] = me_bound_bottom[1];
    }

    for(i=ic=0; i < encodeInfo.info.clip_info.width; i += 16, ic += BlkWidth_c)
    {
      Ipp32s cur_offset_src = i + j * YFramePitchSrc;
      Ipp32s cur_offset_c_src = ic + jc * UVFramePitchSrc;
      Ipp32s cur_offset_ref = i + j * YFramePitchRef;
      Ipp32s cur_offset_c_ref = ic + jc * UVFramePitchRef;
      Ipp32s cur_offset_rec = i + j * YFramePitchRec;
      Ipp32s cur_offset_c_rec = ic + jc * UVFramePitchRec;


      if (encodeInfo.info.color_format == NV12)
      {
        cur_offset_c_src *= 2;
        cur_offset_c_ref *= 2;
        cur_offset_c_rec *= 2;
      }

      YBlock = Y_src + cur_offset_src;
      UBlock = U_src + cur_offset_c_src;
      VBlock = V_src + cur_offset_c_src;

      //pRef[0] = YRefFrame[curr_field][DIR] + cur_offset;
      //pRef[1] = YRefFrame[1-curr_field][DIR] + cur_offset;

      if (bQuantiserChanged && !pMBInfo[k].skipped) {
        MFX_INTERNAL_CPY((Ipp8u*)threadSpec[numTh].PMV, (Ipp8u*)pMBInfo[k].MV, sizeof(threadSpec[0].PMV));
        k++;
        continue;
      }

      pMBInfo[k].skipped = 0;
      pMBInfo[k].mb_type = 0;
      pMBInfo[k].dct_type = DCT_FRAME;
      pMBInfo[k].prediction_type = MC_FRAME;

      // try skip MB
      if (i != 0 && i != MBcountH*16-16 &&
        !(pMBInfo[k-1].mb_type & MB_INTRA)
        && (!(pMBInfo[k - 1].mb_type & MB_FORWARD)
        || (i+((threadSpec[numTh].PMV[0][0].x+1)>>1)+15 < MBcountH*16 && j+((threadSpec[numTh].PMV[0][0].y+1)>>1)+15 < MBcountV*16))
        && (!(pMBInfo[k - 1].mb_type & MB_BACKWARD)
        || (i+((threadSpec[numTh].PMV[0][1].x+1)>>1)+15 < MBcountH*16 && j+((threadSpec[numTh].PMV[0][1].y+1)>>1)+15 < MBcountV*16)) )
      {
        Ipp32s blk;
        Ipp32s var, mean;
        Ipp32s mb_type;

        pMBInfo[k].mv_field_sel[2][0] = curr_field;
        pMBInfo[k].mv_field_sel[2][1] = curr_field;

        mb_type = pMBInfo[k - 1].mb_type & (MB_FORWARD | MB_BACKWARD);
        pMBInfo[k].mb_type = mb_type;

        pDiff = threadSpec[numTh].pDiff;

        SET_MOTION_VECTOR((&vector[2][0]), threadSpec[numTh].PMV[0][0].x, threadSpec[numTh].PMV[0][0].y,YFramePitchRec,UVFramePitchRec);
        SET_MOTION_VECTOR((&vector[2][1]), threadSpec[numTh].PMV[0][1].x, threadSpec[numTh].PMV[0][1].y,YFramePitchRec,UVFramePitchRec);

        if (mb_type == (MB_FORWARD|MB_BACKWARD))
        {
          GETDIFF_FRAME_FB(Y, Y, l, pDiff);
        }
        else if (mb_type & MB_FORWARD)
        {
          GETDIFF_FRAME(Y, Y, l, pDiff, 0);
        }
        else // if(mb_type & MB_BACKWARD)
        {
          GETDIFF_FRAME(Y, Y, l, pDiff, 1);
        }

        skip_flag = 1;
        for (blk = 0; blk < 4; blk++) {
          ippiVarSum8x8_16s32s_C1R(pDiff + frm_diff_off[blk], 32, &var, &mean);
          if(var > varThreshold) {
            skip_flag = 0;
            break;
          }
          if(mean >= meanThreshold || mean <= -meanThreshold) {
            skip_flag = 0;
            break;
          }
        }

        if (skip_flag) { // check UV
          if (encodeInfo.info.color_format == NV12) {
            if (mb_type == (MB_FORWARD|MB_BACKWARD))
            {
#ifdef IPP_NV12
              GETDIFF_FRAME_FB_NV12(U, V, Y, c, pDiff);
#else
              GETDIFF_FRAME_FB(U, Y, c, pDiff);
              GETDIFF_FRAME_FB(V, Y, c, pDiff);
#endif
            }
            else if (mb_type & MB_FORWARD)
            {
#ifdef IPP_NV12
              GETDIFF_FRAME_NV12(U, V, Y, c, pDiff, 0);
#else
              GETDIFF_FRAME(U, Y, c, pDiff, 0);
              GETDIFF_FRAME(V, Y, c, pDiff, 0);
#endif
            }
            else // if(mb_type & MB_BACKWARD)
            {
#ifdef IPP_NV12
              GETDIFF_FRAME_NV12(U, V, Y, c, pDiff, 1);
#else
              GETDIFF_FRAME(U, Y, c, pDiff, 1);
              GETDIFF_FRAME(V, Y, c, pDiff, 1);
#endif
            }
          } else {
            if (mb_type == (MB_FORWARD|MB_BACKWARD))
            {
              GETDIFF_FRAME_FB(U, UV, c, pDiff);
              GETDIFF_FRAME_FB(V, UV, c, pDiff);
            }
            else if (mb_type & MB_FORWARD)
            {
              GETDIFF_FRAME(U, UV, c, pDiff, 0);
              GETDIFF_FRAME(V, UV, c, pDiff, 0);
            }
            else // if(mb_type & MB_BACKWARD)
            {
              GETDIFF_FRAME(U, UV, c, pDiff, 1);
              GETDIFF_FRAME(V, UV, c, pDiff, 1);
            }
          }
          for (blk = 4; blk < block_count; blk++) {
            ippiVarSum8x8_16s32s_C1R(pDiff + frm_diff_off[blk], 32, &var, &mean);
            if(var > varThreshold) {
              skip_flag = 0;
              break;
            }
            if(mean >= meanThreshold || mean <= -meanThreshold) {
              skip_flag = 0;
              break;
            }
          }
        }

        if (skip_flag)
        {
          pMBInfo[k].skipped = 1;
          MFX_INTERNAL_CPY((Ipp8u*)pMBInfo[k].MV, (Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));
          k++;
          //MFX_INTERNAL_CPY((Ipp8u*)pMBInfo[k].MV, (Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));
          continue;
        }
      } // try skip

      pMBInfo[k].mb_type = 0;
      best->var_sum = (1 << 30);

      VARMEAN_FRAME_Y(curr->var, mean_frm, curr->var_sum);
      curr->var_sum = SCALE_VAR_INTRA(curr->var_sum);

      if(curr->var_sum < best->var_sum)
      {
        curr->mb_type = MB_INTRA;
        curr->dct_type = DCT_FRAME;
        SWAP_PTR(best, curr);
      }

      BOUNDS_H(dir0,i)
      ME_FRAME(dir0, curr->var_sum, curr->pDiff, curr->dct_type);
      curr->var_sum = SCALE_VAR(curr->var_sum, SC_VAR_1V);

      if(curr->var_sum < best->var_sum)
      {
        curr->mb_type = dir0 == 0 ? MB_FORWARD : MB_BACKWARD;
        curr->pred_type = MC_FRAME;
        SWAP_PTR(best, curr);
      }

      IF_GOOD_PRED(best->var, best->mean) {
        goto encodeMB;
      }

      if (!curr_frame_dct) {
        VARMEAN_FIELD_Y(curr->var, mean_fld, curr->var_sum);
        curr->var_sum = SCALE_VAR_INTRA(curr->var_sum);
        if(curr->var_sum < best->var_sum)
        {
          curr->mb_type = MB_INTRA;
          curr->dct_type = DCT_FIELD;
          SWAP_PTR(best, curr);
        }
      }

      if (!curr_frame_pred ) {
        ME_FIELD(dir0, curr->var_sum, curr->pDiff, curr->dct_type);
        curr->var_sum = SCALE_VAR(curr->var_sum, SC_VAR_2V);
        if(curr->var_sum < best->var_sum)
        {
          curr->mb_type = dir0 == 0 ? MB_FORWARD : MB_BACKWARD;
          curr->pred_type = MC_FIELD;
          SWAP_PTR(best, curr);
        }
      }

      // Backward vector search
      BOUNDS_H(dir1,i)
      ME_FRAME(dir1, curr->var_sum, curr->pDiff, curr->dct_type);
      curr->var_sum = SCALE_VAR(curr->var_sum, SC_VAR_1V);
      if(curr->var_sum < best->var_sum)
      {
        curr->mb_type = dir1 == 0 ? MB_FORWARD : MB_BACKWARD;
        curr->pred_type = MC_FRAME;
        SWAP_PTR(best, curr);

        IF_GOOD_PRED(best->var, best->mean) {
          goto encodeMB;
        }
      }

      if (!curr_frame_pred ) {
        ME_FIELD(dir1, curr->var_sum, curr->pDiff, curr->dct_type);
        curr->var_sum = SCALE_VAR(curr->var_sum, SC_VAR_2V);
        if(curr->var_sum < best->var_sum)
        {
          curr->mb_type = dir1 == 0 ? MB_FORWARD : MB_BACKWARD;
          curr->pred_type = MC_FIELD;
          SWAP_PTR(best, curr);
        }
      }

      // Bi-directional
      for (pred_ind = 0; pred_ind <= (curr_frame_pred ? 0 : 1); pred_ind++) {
        Ipp32s pred_type;
        Ipp32s scale_var;

        if (!pred_ind) {
          pred_type = MC_FRAME;
          GETDIFF_FRAME_FB(Y, Y, l, curr->pDiff);
          scale_var = SC_VAR_1VBI;
        } else {
          pred_type = MC_FIELD;
          GETDIFF_FIELD_FB(Y, Y, l, curr->pDiff);
          scale_var = SC_VAR_2VBI;
        }

        if (!curr_frame_dct) {
          Ipp32s var_fld = 0, var = 0;
          ippiFrameFieldSAD16x16_16s32s_C1R(curr->pDiff, BlkStride_l*2, &var, &var_fld);
          if (var_fld < var) {
            curr->dct_type = DCT_FIELD;
            VARMEAN_FIELD(curr->pDiff, curr->var, curr->mean, curr->var_sum);
          } else {
            curr->dct_type = DCT_FRAME;
            VARMEAN_FRAME(curr->pDiff, curr->var, curr->mean, curr->var_sum);
          }
        } else {
          curr->dct_type = DCT_FRAME;
          VARMEAN_FRAME(curr->pDiff, curr->var, curr->mean, curr->var_sum);
        }

        curr->var_sum = SCALE_VAR(curr->var_sum, scale_var);
        if(curr->var_sum < best->var_sum)
        {
          curr->mb_type = MB_FORWARD | MB_BACKWARD;
          curr->pred_type = pred_type;
          SWAP_PTR(best, curr);
        }
      }

encodeMB:
      pMBInfo[k].prediction_type = best->pred_type;
      pMBInfo[k].dct_type = best->dct_type;
      pMBInfo[k].mb_type = best->mb_type;

      if(pMBInfo[k].mb_type == MB_INTRA) {
        ippsZero_8u((Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV)); // zero vectors
        //ippsZero_8u((Ipp8u*)pMBInfo[k].MV, sizeof(pMBInfo[k].MV)); // zero vectors
      } else if (pMBInfo[k].prediction_type == MC_FRAME) {
        if(pMBInfo[k].mb_type & MB_FORWARD) {
          threadSpec[numTh].PMV[0][0].x = threadSpec[numTh].PMV[1][0].x = vector[2][0].x;
          threadSpec[numTh].PMV[0][0].y = threadSpec[numTh].PMV[1][0].y = vector[2][0].y;
        }
        if(pMBInfo[k].mb_type & MB_BACKWARD) {
          threadSpec[numTh].PMV[0][1].x = threadSpec[numTh].PMV[1][1].x = vector[2][1].x;
          threadSpec[numTh].PMV[0][1].y = threadSpec[numTh].PMV[1][1].y = vector[2][1].y;
        }
      } else if (pMBInfo[k].prediction_type == MC_FIELD) {
        Ipp32s mv_shift = (picture_structure == MPEG2_FRAME_PICTURE) ? 1 : 0;
        if(pMBInfo[k].mb_type & MB_FORWARD) {
          threadSpec[numTh].PMV[0][0].x = vector[0][0].x;
          threadSpec[numTh].PMV[0][0].y = vector[0][0].y << mv_shift;
          threadSpec[numTh].PMV[1][0].x = vector[1][0].x;
          threadSpec[numTh].PMV[1][0].y = vector[1][0].y << mv_shift;
        }
        if(pMBInfo[k].mb_type & MB_BACKWARD) {
          threadSpec[numTh].PMV[0][1].x = vector[0][1].x;
          threadSpec[numTh].PMV[0][1].y = vector[0][1].y << mv_shift;
          threadSpec[numTh].PMV[1][1].x = vector[1][1].x;
          threadSpec[numTh].PMV[1][1].y = vector[1][1].y << mv_shift;
        }
      }
      MFX_INTERNAL_CPY((Ipp8u*)pMBInfo[k].MV, (Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));

      k++;
    } // for(i)
  } // for(j)
}



void MPEG2VideoEncoderBase::putB(Ipp32s numTh)
{
  Ipp32s      i, j, ic, jc, k;
  Ipp32s      macroblock_address_increment;
  Ipp32s      dc_dct_pred[3];
  Ipp16s      *pMBlock, *pDiff;
  Ipp32s      Count[12], CodedBlockPattern;
  IppMotionVector2 vector[3][2] = {0,}; // top/bottom/frame F/B
  const Ipp8u *YBlock;
  const Ipp8u *UBlock;
  const Ipp8u *VBlock;
  Ipp32s      slice_past_intra_address;
  Ipp32s      slice_macroblock_address;
  Ipp32s      start_y;
  Ipp32s      start_uv;
  Ipp32s      stop_y;
  MB_prediction_info best[1];

  CALC_START_STOP_ROWS

  pDiff = threadSpec[numTh].pDiff;
  best->pDiff = pDiff;
  best->var[0] = INT_MAX;
  best->var[1] = INT_MAX;
  best->var[2] = INT_MAX;
  best->var[3] = INT_MAX;

  for (j = start_y, jc = start_uv; j < stop_y; j += 16, jc += BlkHeight_c)
  {
    PutSliceHeader(j >> 4, numTh);
    macroblock_address_increment = 1;
    slice_macroblock_address = 0;
    slice_past_intra_address = 0;

    // reset predictors at the start of slice
    dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = ResetTbl[intra_dc_precision];

    ippsZero_8u((Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));

    for(i=ic=0; i < encodeInfo.info.clip_info.width; i += 16, ic += BlkWidth_c)
    {
      Ipp32s cur_offset_src = i + j * YFramePitchSrc;
      Ipp32s cur_offset_c_src = ic + jc * UVFramePitchSrc;
      //Ipp32s cur_offset_ref = i + j * YFramePitchRef;
      //Ipp32s cur_offset_c_ref = ic + jc * UVFramePitchRef;
      //Ipp32s cur_offset_rec = i + j * YFramePitchRec;
      //Ipp32s cur_offset_c_rec = ic + jc * UVFramePitchRec;


      if (encodeInfo.info.color_format == NV12)
      {
        cur_offset_c_src *= 2;
        //cur_offset_c_ref *= 2;
        //cur_offset_c_rec *= 2;
      }

      YBlock = Y_src + cur_offset_src;
      UBlock = U_src + cur_offset_c_src;
      VBlock = V_src + cur_offset_c_src;

      //pRef[0] = YRefFrame[curr_field][DIR] + cur_offset;
      //pRef[1] = YRefFrame[1-curr_field][DIR] + cur_offset;

      slice_macroblock_address = i >> 4;

      // try skip MB
      if (pMBInfo[k].skipped )
      {
skip_macroblock:
        macroblock_address_increment++;
        MFX_INTERNAL_CPY((Ipp8u*)pMBInfo[k].MV, (Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));
        k++;
        //MFX_INTERNAL_CPY((Ipp8u*)pMBInfo[k].MV, (Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));
        continue;
      } // try skip

      if (pMBInfo[k].mb_type & MB_INTRA)
      {
        ippsZero_8u((Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));
        ippsZero_8u((Ipp8u*)pMBInfo[k].MV, sizeof(pMBInfo[k].MV));

        PutAddrIncrement(macroblock_address_increment, numTh);
        macroblock_address_increment = 1;

        if(slice_macroblock_address - slice_past_intra_address > 1)
          dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = ResetTbl[intra_dc_precision];

        slice_past_intra_address = (i >> 4);

        const Ipp8u *BlockSrc[3] = {YBlock, UBlock, VBlock};
        PutIntraMacroBlock(numTh, k, BlockSrc, NULL, dc_dct_pred);

        k++;
        continue;
      } //Intra macroblock

      ippsZero_8u((Ipp8u*)Count, sizeof(Count));
      CodedBlockPattern  = 0;

      best->dct_type = pMBInfo[k].dct_type;

      // Non-intra macroblock
      if(pMBInfo[k].prediction_type == MC_FRAME)
      {
        Ipp32s try_skip = (i != 0) && (i != MBcountH*16 - 16) &&
            (((pMBInfo[k].mb_type ^ pMBInfo[k-1].mb_type) & (MB_FORWARD | MB_BACKWARD)) == 0);
        SET_MOTION_VECTOR((&vector[2][0]), pMBInfo[k].MV[0][0].x, pMBInfo[k].MV[0][0].y,YFramePitchRec,UVFramePitchRec)
        SET_MOTION_VECTOR((&vector[2][1]), pMBInfo[k].MV[0][1].x, pMBInfo[k].MV[0][1].y,YFramePitchRec,UVFramePitchRec)
        if((pMBInfo[k].mb_type&MB_FORWARD)&&
          (pMBInfo[k].mb_type&MB_BACKWARD))
        {
          GETDIFF_FRAME_FB(Y, Y, l, pDiff);
          //VARMEAN_FRAME(pDiff, best->var, best->mean, best->var_sum);
          if (encodeInfo.info.color_format == NV12) {
#ifdef IPP_NV12
              GETDIFF_FRAME_FB_NV12(U, V, Y, c, pDiff);
#else
            GETDIFF_FRAME_FB(U, Y, c, pDiff);
            GETDIFF_FRAME_FB(V, Y, c, pDiff);
#endif
          } else {
            GETDIFF_FRAME_FB(U, UV, c, pDiff);
            GETDIFF_FRAME_FB(V, UV, c, pDiff);
          }

          NonIntraMBCoeffs(numTh, best, Count, &CodedBlockPattern);
          if(CodedBlockPattern == 0 && try_skip &&
              vector[2][0].x == threadSpec[numTh].PMV[0][0].x &&
              vector[2][0].y == threadSpec[numTh].PMV[0][0].y &&
              vector[2][1].x == threadSpec[numTh].PMV[0][1].x &&
              vector[2][1].y == threadSpec[numTh].PMV[0][1].y &&
              pMBInfo[k].mv_field_sel[2][0] == curr_field &&
              pMBInfo[k].mv_field_sel[2][1] == curr_field) {
            goto skip_macroblock;
          }

          PutAddrIncrement(macroblock_address_increment, numTh);
          macroblock_address_increment = 1;

          PUT_MB_MODES(MPEG2_B_PICTURE, MB_FORWARD | MB_BACKWARD);
          PutMV_FRAME(numTh, k, &vector[2][0], MB_FORWARD);
          PutMV_FRAME(numTh, k, &vector[2][1], MB_BACKWARD);
        }
        else if(pMBInfo[k].mb_type&MB_FORWARD)
        {
          GETDIFF_FRAME(Y, Y, l, pDiff, 0);
          //VARMEAN_FRAME(pDiff, best->var, best->mean, best->var_sum);
          if (encodeInfo.info.color_format == NV12) {
#ifdef IPP_NV12
              GETDIFF_FRAME_NV12(U, V, Y, c, pDiff, 0);
#else
            GETDIFF_FRAME(U, Y, c, pDiff, 0);
            GETDIFF_FRAME(V, Y, c, pDiff, 0);
#endif
          } else {
            GETDIFF_FRAME(U, UV, c, pDiff, 0);
            GETDIFF_FRAME(V, UV, c, pDiff, 0);
          }

          NonIntraMBCoeffs(numTh, best, Count, &CodedBlockPattern);
          if(CodedBlockPattern == 0 && try_skip &&
              vector[2][0].x == threadSpec[numTh].PMV[0][0].x &&
              vector[2][0].y == threadSpec[numTh].PMV[0][0].y &&
              pMBInfo[k].mv_field_sel[2][0] == curr_field) {
            goto skip_macroblock;
          }

          PutAddrIncrement(macroblock_address_increment, numTh);
          macroblock_address_increment = 1;

          PUT_MB_MODES(MPEG2_B_PICTURE, MB_FORWARD);
          PutMV_FRAME(numTh, k, &vector[2][0], MB_FORWARD);
        }
        else //pMBInfo.mb_type&MB_BACKWARD
        {
          GETDIFF_FRAME(Y, Y, l, pDiff, 1);
          //VARMEAN_FRAME(pDiff, best->var, best->mean, best->var_sum);
          if (encodeInfo.info.color_format == NV12) {
#ifdef IPP_NV12
              GETDIFF_FRAME_NV12(U, V, Y, c, pDiff, 1);
#else
            GETDIFF_FRAME(U, Y, c, pDiff, 1);
            GETDIFF_FRAME(V, Y, c, pDiff, 1);
#endif
          } else {
            GETDIFF_FRAME(U, UV, c, pDiff, 1);
            GETDIFF_FRAME(V, UV, c, pDiff, 1);
          }

          NonIntraMBCoeffs(numTh, best, Count, &CodedBlockPattern);
          if(CodedBlockPattern == 0 && try_skip &&
              vector[2][1].x == threadSpec[numTh].PMV[0][1].x &&
              vector[2][1].y == threadSpec[numTh].PMV[0][1].y &&
              pMBInfo[k].mv_field_sel[2][1] == curr_field) {
            goto skip_macroblock;
          }

          PutAddrIncrement(macroblock_address_increment, numTh);
          macroblock_address_increment = 1;

          PUT_MB_MODES(MPEG2_B_PICTURE, MB_BACKWARD);
          PutMV_FRAME(numTh, k, &vector[2][1], MB_BACKWARD);
        }
      }
      else //pMBInfo[k].prediction_type == MC_FIELD
      {
        if (picture_structure == MPEG2_FRAME_PICTURE) {
          SET_FIELD_VECTOR((&vector[0][0]), pMBInfo[k].MV[0][0].x, pMBInfo[k].MV[0][0].y >> 1,YFramePitchRec,UVFramePitchRec);
          SET_FIELD_VECTOR((&vector[1][0]), pMBInfo[k].MV[1][0].x, pMBInfo[k].MV[1][0].y >> 1,YFramePitchRec,UVFramePitchRec);
          SET_FIELD_VECTOR((&vector[0][1]), pMBInfo[k].MV[0][1].x, pMBInfo[k].MV[0][1].y >> 1,YFramePitchRec,UVFramePitchRec);
          SET_FIELD_VECTOR((&vector[1][1]), pMBInfo[k].MV[1][1].x, pMBInfo[k].MV[1][1].y >> 1,YFramePitchRec,UVFramePitchRec);
        } else {
          SET_MOTION_VECTOR((&vector[0][0]), pMBInfo[k].MV[0][0].x, pMBInfo[k].MV[0][0].y,YFramePitchRec,UVFramePitchRec);
          SET_MOTION_VECTOR((&vector[1][0]), pMBInfo[k].MV[1][0].x, pMBInfo[k].MV[1][0].y,YFramePitchRec,UVFramePitchRec);
          SET_MOTION_VECTOR((&vector[0][1]), pMBInfo[k].MV[0][1].x, pMBInfo[k].MV[0][1].y,YFramePitchRec,UVFramePitchRec);
          SET_MOTION_VECTOR((&vector[1][1]), pMBInfo[k].MV[1][1].x, pMBInfo[k].MV[1][1].y,YFramePitchRec,UVFramePitchRec);
        }
        if((pMBInfo[k].mb_type&MB_FORWARD)&&
          (pMBInfo[k].mb_type&MB_BACKWARD))
        {
          GETDIFF_FIELD_FB(Y, Y, l, pDiff);
          //VARMEAN_FIELD(pDiff, best->var, best->mean, best->var_sum);
          if (encodeInfo.info.color_format == NV12) {
#ifdef IPP_NV12
            GETDIFF_FIELD_FB_NV12(U, V, Y, c, pDiff);
#else
            GETDIFF_FIELD_FB(U, Y, c, pDiff);
            GETDIFF_FIELD_FB(V, Y, c, pDiff);
#endif
          } else {
            GETDIFF_FIELD_FB(U, UV, c, pDiff);
            GETDIFF_FIELD_FB(V, UV, c, pDiff);
          }

          NonIntraMBCoeffs(numTh, best, Count, &CodedBlockPattern);

          PutAddrIncrement(macroblock_address_increment, numTh);
          macroblock_address_increment = 1;

          PUT_MB_MODES(MPEG2_B_PICTURE, MB_FORWARD | MB_BACKWARD);
          PutMV_FIELD(numTh, k, &vector[0][0], &vector[1][0], MB_FORWARD);
          PutMV_FIELD(numTh, k, &vector[0][1], &vector[1][1], MB_BACKWARD);
        } else
          if(pMBInfo[k].mb_type&MB_FORWARD)
          {
            GETDIFF_FIELD(Y, Y, l, pDiff, 0);
            //VARMEAN_FIELD(pDiff, best->var, best->mean, best->var_sum);
            if (encodeInfo.info.color_format == NV12) {
#ifdef IPP_NV12
              GETDIFF_FIELD_NV12(U, V, Y, c, pDiff, 0);

#else
              GETDIFF_FIELD(U, Y, c, pDiff, 0);
              GETDIFF_FIELD(V, Y, c, pDiff, 0);
#endif
            } else {
              GETDIFF_FIELD(U, UV, c, pDiff, 0);
              GETDIFF_FIELD(V, UV, c, pDiff, 0);
            }

            NonIntraMBCoeffs(numTh, best, Count, &CodedBlockPattern);

            PutAddrIncrement(macroblock_address_increment, numTh);
            macroblock_address_increment = 1;

            PUT_MB_MODES(MPEG2_B_PICTURE, MB_FORWARD);
            PutMV_FIELD(numTh, k, &vector[0][0], &vector[1][0], MB_FORWARD);
          }
          else //pMBInfo.mb_type&MB_BACKWARD
          {
            GETDIFF_FIELD(Y, Y, l, pDiff, 1);
            //VARMEAN_FIELD(pDiff, best->var, best->mean, best->var_sum);
            if (encodeInfo.info.color_format == NV12) {
#ifdef IPP_NV12
              GETDIFF_FIELD_NV12(U, V, Y, c, pDiff, 1);

#else
              GETDIFF_FIELD(U, Y, c, pDiff, 1);
              GETDIFF_FIELD(V, Y, c, pDiff, 1);
#endif
            } else {
              GETDIFF_FIELD(U, UV, c, pDiff, 1);
              GETDIFF_FIELD(V, UV, c, pDiff, 1);
            }

            NonIntraMBCoeffs(numTh, best, Count, &CodedBlockPattern);

            PutAddrIncrement(macroblock_address_increment, numTh);
            macroblock_address_increment = 1;

            PUT_MB_MODES(MPEG2_B_PICTURE, MB_BACKWARD);
            PutMV_FIELD(numTh, k, &vector[0][1], &vector[1][1], MB_BACKWARD);
          }
      } //FIELD_PREDICTION

      PUT_BLOCK_PATTERN(CodedBlockPattern);

      pMBlock = threadSpec[numTh].pMBlock;
      for (Ipp32s loop = 0; loop < block_count; loop++)
      {
        if (Count[loop]) {
          PutNonIntraBlock(pMBlock, Count[loop], numTh);
        }
        pMBlock += 64;
      }

      MFX_INTERNAL_CPY((Ipp8u*)pMBInfo[k].MV, (Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));

      k++;
    } // for(i)
  } // for(j)
}

#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER
#endif // UMC_RESTRICTED_CODE
