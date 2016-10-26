//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2011 Intel Corporation. All Rights Reserved.
//

#ifndef UMC_RESTRICTED_CODE

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#include "ippi.h"
#include "ipps.h"
#include "umc_mpeg2_enc_defs.h"

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Coding P picture ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

using namespace UMC;

void MPEG2VideoEncoderBase::meP( Ipp32s numTh)
{
  const Ipp8u *pRef[2]; // parity same/opposite
  const Ipp8u *pRec[2]; // parity same/opposite
  Ipp32s      i, j, ic, jc, k, blk;
  IppMotionVector2 vector[3][1] = {0,};
  // bounds in half pixels, border included
  Ipp32s      me_bound_left[1], me_bound_right[1];
  Ipp32s      me_bound_top[1], me_bound_bottom[1];
  Ipp32s      me_bound_2_top[1], me_bound_1_bottom[1];
  MB_prediction_info pred_info[2];
  MB_prediction_info *best = pred_info;
  MB_prediction_info *curr = pred_info + 1;
  Ipp32s      mean_frm[4], mean_fld[4];
  Ipp32s      start_y;
  Ipp32s      start_uv;
  Ipp32s      stop_y;
  Ipp32s      skip_flag;

  CALC_START_STOP_ROWS

  best->pDiff = threadSpec[numTh].pDiff;
  curr->pDiff = threadSpec[numTh].pDiff1;

  if (!bQuantiserChanged) {
    Ipp32s mf; // *2 to support IP case
    mf = ipflag ? 1 : 2*P_distance;

    for(j=start_y; j < stop_y; j += 16)
    {
      for(i=0; i < encodeInfo.info.clip_info.width; i += 16, k++)
      {
        //Ipp32s x, y, m;
        //if(i+1 >= encodeInfo.info.clip_info.width) {
        //  pMBInfo[k].MV[0][0] = pMBInfo[k-1].MV_P[0];
        //  pMBInfo[k].MV[1][0] = pMBInfo[k-1].MV_P[1];
        //} else {
        //  pMBInfo[k].MV[0][0] = pMBInfo[k+1].MV_P[0];
        //  pMBInfo[k].MV[1][0] = pMBInfo[k+1].MV_P[1];
        //}
        pMBInfo[k].MV[0][0].x = pMBInfo[k].MV_P[0].x*mf/512;
        pMBInfo[k].MV[0][0].y = pMBInfo[k].MV_P[0].y*mf/512;
        pMBInfo[k].MV[1][0].x = pMBInfo[k].MV_P[1].x*mf/512;
        pMBInfo[k].MV[1][0].y = pMBInfo[k].MV_P[1].y*mf/512;
        //if(i>0) {
        //  pMBInfo[k].MV[0][0] = pMBInfo[k-1].MV[0][0];
        //  pMBInfo[k].MV[1][0] = pMBInfo[k-1].MV[1][0];
        //} else if (j>start_y) {
        //  pMBInfo[k].MV[0][0] = pMBInfo[k-MBcountH].MV[0][0];
        //  pMBInfo[k].MV[1][0] = pMBInfo[k-MBcountH].MV[1][0];
        //} else {
        //  pMBInfo[k].MV[0][0].x = 0;
        //  pMBInfo[k].MV[0][0].y = 0;
        //  pMBInfo[k].MV[1][0].x = 0;
        //  pMBInfo[k].MV[1][0].y = 0;
        //}
        //for(y=((j==start_y)?0:-1); y<=((j+16<stop_y)?1:0); y++ ) {
        //  for(x=((i==0)?0:-1); x<=((i+16<encodeInfo.info.clip_info.width)?1:0); x++ ) {
        //    m = k + x + y*MBcountH;
        //    if(m!=k && PDIST(pMBInfo[m].MV_P[0], pMBInfo[k].MV_P[0]) <=10) {
        //      pMBInfo[k].MV[0][0] = pMBInfo[k].MV_P[0];
        //    }
        //    if(m!=k && PDIST(pMBInfo[m].MV_P[1], pMBInfo[k].MV_P[1]) <=10) {
        //      pMBInfo[k].MV[1][0] = pMBInfo[k].MV_P[1];
        //    }
        //  }
        //}

      }
    }
    k = (encodeInfo.numThreads > 1) ? (start_y/16)*MBcountH : 0;
  }

#ifdef MPEG2_USE_DUAL_PRIME
  if(!bQuantiserChanged)threadSpec[numTh].field_diff_SAD = 0;
#endif //MPEG2_USE_DUAL_PRIME

  for(j=start_y, jc= start_uv; j < stop_y; j += 16, jc += BlkHeight_c)
  {
    ippsZero_8u((Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));
    BOUNDS_V(0,j)
    BOUNDS_V_FIELD(0,j)

    for(i=ic=0; i < encodeInfo.info.clip_info.width; i += 16, ic += BlkWidth_c)
    {
      Ipp32s cur_offset_src   = i  + j  * YFramePitchSrc;
      Ipp32s cur_offset_c_src = ic + jc * UVFramePitchSrc;
      Ipp32s cur_offset_ref   = i  + j  * YFramePitchRef;
      Ipp32s cur_offset_c_ref = ic + jc * UVFramePitchRef;
      Ipp32s cur_offset_rec   = i  + j  * YFramePitchRec;
      Ipp32s cur_offset_c_rec = ic + jc * UVFramePitchRec;

      if (encodeInfo.info.color_format == NV12)
      {
        cur_offset_c_src *= 2;
        cur_offset_c_ref *= 2;
        cur_offset_c_rec *= 2;
      }

      const Ipp8u *YBlock = Y_src + cur_offset_src;
      const Ipp8u *UBlock = U_src + cur_offset_c_src;
      const Ipp8u *VBlock = V_src + cur_offset_c_src;

      pRef[0] = YRefFrame[curr_field][0] + cur_offset_ref;   // same parity
      pRef[1] = YRefFrame[1-curr_field][0] + cur_offset_ref; // opposite parity
      pRec[0] = YRecFrame[curr_field][0] + cur_offset_rec;   // same parity
      pRec[1] = YRecFrame[1-curr_field][0] + cur_offset_rec; // opposite parity

      if (bQuantiserChanged && !pMBInfo[k].skipped) {
        k++;
        continue;
      }

      pMBInfo[k].mb_type = 0;

      // try to skip
      if (i != 0 && i != MBcountH*16 - 16 && !ipflag)
      {
        Ipp32s sad;
        Ipp32s sad_u,sad_v;
        //ippiSAD16x16_8u32s(YRefFrame[curr_field][0] + cur_offset, YFrameHSize,
        //  YBlock, YFrameHSize, &sad, 0);
        //skip_flag = (sad < quantiser_scale_value*256/8);

        Ipp16s* pDiff = threadSpec[numTh].pDiff;

        ippiGetDiff16x16_8u16s_C1(YBlock, YFramePitchSrc, pRef[0], YFramePitchRef,
          pDiff, 32, 0, 0, 0, 0);

        skip_flag = 1;

        for (blk = 0; blk < 4; blk++) {
          Ipp32s var, mean;
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
        if (skip_flag) {
          skip_flag = 0;

          if(encodeInfo.info.color_format == NV12)
          {
#ifdef IPP_NV12
            ippiSAD8x8_8u32s_C2R(UBlock, UVFramePitchSrc*2,
                URefFrame[curr_field][0] + cur_offset_c_ref, UVFramePitchRef*2,
                &sad_u, &sad_v, 0);
            if(sad_u < sadThreshold && sad_v < sadThreshold)
                  skip_flag = 1;
#else
            tmp_ippiSAD8x8_8u32s_C2R(URefFrame[curr_field][0] + cur_offset_c_ref, UVFramePitchRef*2,
              UBlock, UVFramePitchSrc*2, &sad_u, 0);
            if(sad_u < sadThreshold)
                tmp_ippiSAD8x8_8u32s_C2R(VRefFrame[curr_field][0] + cur_offset_c_ref, UVFramePitchRef*2,
                    VBlock, UVFramePitchSrc*2, &sad_v, 0);
            if(sad_v < sadThreshold)
                skip_flag = 1;
#endif
          }//if(encodeInfo.info.color_format == NV12)
          else
          {
            ippiSAD8x8_8u32s_C1R(URefFrame[curr_field][0] + cur_offset_c_ref, UVFramePitchRef,
              UBlock, UVFramePitchSrc, &sad, 0);
            if(sad < sadThreshold)
            {
                ippiSAD8x8_8u32s_C1R(VRefFrame[curr_field][0] + cur_offset_c_ref, UVFramePitchRef,
                                VBlock, UVFramePitchSrc, &sad, 0);
                if(sad < sadThreshold)
                     skip_flag = 1;
            }
          }
        }//if (skip_flag)
        if (skip_flag) {
          // skip this macroblock
          // no DCT coefficients and neither first nor last macroblock of slice and no motion
          ippsZero_8u((Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV)); // zero vectors
          ippsZero_8u((Ipp8u*)pMBInfo[k].MV, sizeof(pMBInfo[k].MV)); // zero vectors
          pMBInfo[k].mb_type = MB_FORWARD;
          pMBInfo[k].skipped = 1; // skipped type

          //macroblock_address_increment++;
          k++;
          continue;
        }
      }
      pMBInfo[k].skipped = 0;

      best->var_sum = (1 << 30);

      curr->mb_type = MB_INTRA;
      curr->dct_type = DCT_FRAME;
      //if (!curr_frame_dct) {
      //  Ipp32s var_fld = 0, var = 0;
      //  ippiFrameFieldSAD16x16_8u32s_C1R(YBlock, YFrameHSize, &var, &var_fld);

      //  if (var_fld < var) {
      //    VARMEAN_FIELD_Y(curr->var, mean_fld, curr->var_sum);
      //    curr->dct_type = DCT_FIELD;
      //  }
      //}
      // Frame type
      if(curr->dct_type == DCT_FRAME) {
        VARMEAN_FRAME_Y(curr->var, mean_frm, curr->var_sum);
      }
      curr->var_sum = SCALE_VAR_INTRA(curr->var_sum);
      if (curr->var_sum < best->var_sum)
      {
        SWAP_PTR(best, curr);
      }

#ifdef MPEG2_USE_DUAL_PRIME
      threadSpec[numTh].fld_vec_count  = 0;
#endif //MPEG2_USE_DUAL_PRIME

      BOUNDS_H(0,i)
      ME_FRAME(0, curr->var_sum, curr->pDiff, curr->dct_type);
      curr->var_sum = SCALE_VAR(curr->var_sum, SC_VAR_1V);

      if(curr->var_sum < best->var_sum)
      {
        curr->mb_type = MB_FORWARD;
        curr->pred_type = MC_FRAME;
        SWAP_PTR(best, curr);
      }

      if (!curr_frame_pred ) {
        IF_GOOD_PRED(best->var, best->mean) {
          goto encodeMB;
        }
      }

      // Field type
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

      if (!curr_frame_pred && (vector[2][0].x | vector[2][0].y)) {

        ME_FIELD(0, curr->var_sum, curr->pDiff, curr->dct_type);
        curr->var_sum = SCALE_VAR(curr->var_sum, SC_VAR_2V);

        if(curr->var_sum < best->var_sum)
        {
          curr->mb_type = MB_FORWARD;
          curr->pred_type = MC_FIELD;
          SWAP_PTR(best, curr);
        }
      }

#ifdef MPEG2_USE_DUAL_PRIME
      if(dpflag)
      {
        tryDPpredict(pRef,YFramePitchRef,YBlock,YFramePitchSrc,pMBInfo[k].dpMV[curr_field],&pMBInfo[k].dpDMV[curr_field],threadSpec[numTh].dp_vec,curr,me_bound_left[0],me_bound_right[0],me_bound_top[0],me_bound_bottom[0]);

        curr->var_sum=SCALE_VAR(curr->var_sum, SC_VAR_1V-1);//SCALE_VAR(bestVarSum, SC_VAR_1V);
        if(curr->var_sum < best->var_sum)
        {
          curr->mb_type = MB_FORWARD;
          curr->pred_type = MC_DMV;
          SWAP_PTR(best, curr);

          if(picture_structure==MPEG2_FRAME_PICTURE)
          {
            SET_FIELD_VECTOR(vector[0], pMBInfo[k].dpMV[curr_field][0].x, pMBInfo[k].dpMV[curr_field][0].y,YFramePitchRef,UVFramePitchRef);
            //SET_FIELD_VECTOR(vector[1], pMBInfo[k].dpMV[curr_field][1].x, pMBInfo[k].dpMV[curr_field][1].y);
            //SET_FIELD_VECTOR(vector[2], pMBInfo[k].dpMV[curr_field][2].x, pMBInfo[k].dpMV[curr_field][2].y);
            best->dct_type = DCT_FIELD;
          }else{
            SET_MOTION_VECTOR(vector[0], pMBInfo[k].dpMV[curr_field][0].x, pMBInfo[k].dpMV[curr_field][0].y,YFramePitchRef,UVFramePitchRef);
            //SET_MOTION_VECTOR(vector[1], pMBInfo[k].dpMV[curr_field][1].x, pMBInfo[k].dpMV[curr_field][1].y);
            //SET_MOTION_VECTOR(vector[2], pMBInfo[k].dpMV[curr_field][2].x, pMBInfo[k].dpMV[curr_field][2].y);
            best->dct_type = DCT_FRAME;
          }
        }
      }//enable dual prime
#endif //MPEG2_USE_DUAL_PRIME

encodeMB:
      pMBInfo[k].prediction_type = best->pred_type;
      pMBInfo[k].dct_type = best->dct_type;
      pMBInfo[k].mb_type = best->mb_type;
      if(pMBInfo[k].mb_type == MB_INTRA) {
        ippsZero_8u((Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV)); // zero vectors
        ippsZero_8u((Ipp8u*)pMBInfo[k].MV, sizeof(pMBInfo[k].MV)); // zero vectors
      } else if (pMBInfo[k].prediction_type == MC_FRAME) {
        threadSpec[numTh].PMV[0][0].x = threadSpec[numTh].PMV[1][0].x = vector[2][0].x;
        threadSpec[numTh].PMV[0][0].y = threadSpec[numTh].PMV[1][0].y = vector[2][0].y;
        pMBInfo[k].MV[0][0].x = pMBInfo[k].MV[1][0].x = vector[2][0].x;
        pMBInfo[k].MV[0][0].y = pMBInfo[k].MV[1][0].y = vector[2][0].y;
#ifdef MPEG2_USE_DUAL_PRIME
      } else if (pMBInfo[k].prediction_type == MC_DMV) {
        Ipp32s mv_shift = (picture_structure == MPEG2_FRAME_PICTURE) ? 1 : 0;
        threadSpec[numTh].PMV[0][0].x = threadSpec[numTh].PMV[1][0].x = vector[0][0].x;
        threadSpec[numTh].PMV[0][0].y = threadSpec[numTh].PMV[1][0].y = vector[0][0].y << mv_shift;
        pMBInfo[k].MV[0][0].x = pMBInfo[k].MV[1][0].x = vector[0][0].x;
        pMBInfo[k].MV[0][0].y = pMBInfo[k].MV[1][0].y = vector[0][0].y << mv_shift;
#endif //MPEG2_USE_DUAL_PRIME
      } else if (pMBInfo[k].prediction_type == MC_FIELD) {
        Ipp32s mv_shift = (picture_structure == MPEG2_FRAME_PICTURE) ? 1 : 0;
        threadSpec[numTh].PMV[0][0].x = vector[0][0].x;
        threadSpec[numTh].PMV[0][0].y = vector[0][0].y << mv_shift;
        threadSpec[numTh].PMV[1][0].x = vector[1][0].x;
        threadSpec[numTh].PMV[1][0].y = vector[1][0].y << mv_shift;
        pMBInfo[k].MV[0][0].x = vector[0][0].x;
        pMBInfo[k].MV[0][0].y = vector[0][0].y << mv_shift;
        pMBInfo[k].MV[1][0].x = vector[1][0].x;
        pMBInfo[k].MV[1][0].y = vector[1][0].y << mv_shift;
      }

      k++;
    } // for(i)
  } // for(j)

  //threadSpec[numTh].numIntra = numIntra;
}

void MPEG2VideoEncoderBase::putP( Ipp32s numTh)
{
  const Ipp8u *pRef[2]; // parity same/opposite
  //const Ipp8u *pRec[2]; // parity same/opposite
  Ipp16s      *pMBlock, *pDiff;
  Ipp32s      i, j, ic, jc, k, blk;
  Ipp32s      macroblock_address_increment;
  Ipp32s      Count[12], CodedBlockPattern;
  IppMotionVector2 vector[3][1] = {0,};
  IppiSize    roi8x8 = {8,8};
  Ipp32s      slice_past_intra_address;
  Ipp32s      slice_macroblock_address;
  Ipp32s      dc_dct_pred[3];
  IppiSize    roi;
  Ipp32s      start_y;
  Ipp32s      start_uv;
  Ipp32s      stop_y;
  //Ipp32s      skip_flag;
  Ipp32s      numIntra = 0;
#ifdef MPEG2_USE_DUAL_PRIME
  Ipp32s      numInterFrame = 0;
  Ipp32s      numInterField = 0;
#endif // MPEG2_USE_DUAL_PRIME
  MB_prediction_info best[1];

  CALC_START_STOP_ROWS

  pDiff = threadSpec[numTh].pDiff;
  best->pDiff = pDiff;
  best->var[0] = INT_MAX;
  best->var[1] = INT_MAX;
  best->var[2] = INT_MAX;
  best->var[3] = INT_MAX;

  for(j=start_y, jc= start_uv; j < stop_y; j += 16, jc += BlkHeight_c)
  {
    PutSliceHeader( j >> 4, numTh);
    macroblock_address_increment = 1;
    slice_macroblock_address = 0;
    slice_past_intra_address = 0;

    // reset predictors at the start of slice
    dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = ResetTbl[intra_dc_precision];

    ippsZero_8u((Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));

    for(i=ic=0; i < encodeInfo.info.clip_info.width; i += 16, ic += BlkWidth_c)
    {
      Ipp32s cur_offset_src   = i  + j  * YFramePitchSrc;
      Ipp32s cur_offset_c_src = ic + jc * UVFramePitchSrc;
      Ipp32s cur_offset_rec   = i  + j  * YFramePitchRec;
      Ipp32s cur_offset_c_rec = ic + jc * UVFramePitchRec;
      Ipp32s cur_offset_ref   = i  + j  * YFramePitchRef;
      Ipp32s cur_offset_c_ref = ic + jc * UVFramePitchRef;

      if (encodeInfo.info.color_format == NV12)
      {
        cur_offset_c_src *= 2;
        cur_offset_c_rec *= 2;
        cur_offset_c_ref *= 2;
      }
      Ipp8u *YBlockRec = YRecFrame[curr_field][1] + cur_offset_rec;
      Ipp8u *UBlockRec = URecFrame[curr_field][1] + cur_offset_c_rec;
      Ipp8u *VBlockRec = VRecFrame[curr_field][1] + cur_offset_c_rec;
      const Ipp8u *YBlock = Y_src + cur_offset_src;
      const Ipp8u *UBlock = U_src + cur_offset_c_src;
      const Ipp8u *VBlock = V_src + cur_offset_c_src;

      pRef[0] = YRefFrame[curr_field][0] + cur_offset_ref;   // same parity
      pRef[1] = YRefFrame[1-curr_field][0] + cur_offset_ref; // opposite parity
      //pRec[0] = YRecFrame[curr_field][0] + cur_offset;   // same parity
      //pRec[1] = YRecFrame[1-curr_field][0] + cur_offset; // opposite parity
      slice_macroblock_address = i >> 4;

      memset(Count, 0, sizeof(Count));

      // try to skip
      if (pMBInfo[k].skipped == 1) {
skip_macroblock:
        // skip this macroblock
        // no DCT coefficients and neither first nor last macroblock of slice and no motion
        ippsZero_8u((Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV)); // zero vectors
        ippsZero_8u((Ipp8u*)pMBInfo[k].MV, sizeof(pMBInfo[k].MV)); // zero vectors
        //pMBInfo[k].mb_type = 0; // skipped type

        roi.width = BlkWidth_l;
        roi.height = BlkHeight_l;
        ippiCopy_8u_C1R(YRecFrame[curr_field][0] + cur_offset_rec, YFramePitchRec, YBlockRec, YFramePitchRec, roi);
        if(encodeInfo.info.color_format == NV12) {
          roi.width = 16;
          roi.height = 8;
          ippiCopy_8u_C1R(URecFrame[curr_field][0] + cur_offset_c_rec, UVFramePitchRec*2, UBlockRec, UVFramePitchRec*2, roi);
        } else {
          roi.width = BlkWidth_c;
          roi.height = BlkHeight_c;
          ippiCopy_8u_C1R(URecFrame[curr_field][0] + cur_offset_c_rec, UVFramePitchRec, UBlockRec, UVFramePitchRec, roi);
          ippiCopy_8u_C1R(VRecFrame[curr_field][0] + cur_offset_c_rec, UVFramePitchRec, VBlockRec, UVFramePitchRec, roi);
        }

        macroblock_address_increment++;
        k++;
        continue;
      }

//encodeMB:
      pDiff = threadSpec[numTh].pDiff;

      if (pMBInfo[k].mb_type & MB_INTRA)
      { // intra
        ippsZero_8u((Ipp8u*)threadSpec[numTh].PMV, sizeof(threadSpec[0].PMV));
        ippsZero_8u((Ipp8u*)pMBInfo[k].MV, sizeof(pMBInfo[k].MV));

        PutAddrIncrement(macroblock_address_increment, numTh);
        macroblock_address_increment = 1;

        if(slice_macroblock_address - slice_past_intra_address > 1) {
          dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = ResetTbl[intra_dc_precision];
        }

        slice_past_intra_address = (i >> 4);

        const Ipp8u *BlockSrc[3] = {YBlock, UBlock, VBlock};
        Ipp8u *BlockRec[3] = {YBlockRec, UBlockRec, VBlockRec};

        PutIntraMacroBlock(numTh, k, BlockSrc, BlockRec, dc_dct_pred);

        numIntra++;
        k++;
        continue;
      } //intra

      //non-intra
      best->dct_type = pMBInfo[k].dct_type;

      best->pred_type = pMBInfo[k].prediction_type;
      best->mb_type = pMBInfo[k].mb_type; // needed?

      //non-intra
      if (pMBInfo[k].prediction_type == MC_FRAME)
      {
        SET_MOTION_VECTOR((&vector[2][0]), pMBInfo[k].MV[0][0].x, pMBInfo[k].MV[0][0].y,YFramePitchRec,UVFramePitchRec)
        //if (curr_frame_dct) {
          GETDIFF_FRAME(Y, Y, l, pDiff, 0);
        //}
        if(encodeInfo.info.color_format == NV12) {
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

        if (!CodedBlockPattern) {
          if (!(vector[2][0].x | vector[2][0].y)) { // no motion
            if (i != 0 && i != MBcountH*16 - 16 &&
              (picture_structure == MPEG2_FRAME_PICTURE || // probably first cond isn't needed
              pMBInfo[k].mv_field_sel[2][0] == curr_field)) // wrong when ipflag==1
            {
                pMBInfo[k].skipped = 1;
              goto skip_macroblock;
            }
          }
        }

        PutAddrIncrement(macroblock_address_increment, numTh);
        macroblock_address_increment = 1;

        if (!(vector[2][0].x | vector[2][0].y) && CodedBlockPattern &&
          ( picture_structure == MPEG2_FRAME_PICTURE ||
          pMBInfo[k].mv_field_sel[2][0] == curr_field) ) { // no motion
          PUT_MB_MODE_NO_MV(MPEG2_P_PICTURE);
          PutMV_FRAME(numTh, k, &vector[2][0], 0);
        } else {
          PUT_MB_MODES(MPEG2_P_PICTURE, MB_FORWARD);
          PutMV_FRAME(numTh, k, &vector[2][0], MB_FORWARD);
#ifdef MPEG2_USE_DUAL_PRIME
          numInterFrame++;
#endif // MPEG2_USE_DUAL_PRIME
        }
      }
#ifdef MPEG2_USE_DUAL_PRIME
      else if (pMBInfo[k].prediction_type == MC_DMV)
      {
        if(picture_structure==MPEG2_FRAME_PICTURE)
        {
          SET_FIELD_VECTOR(vector[0], pMBInfo[k].dpMV[curr_field][0].x, pMBInfo[k].dpMV[curr_field][0].y,YFramePitchRec,UVFramePitchRec);
          SET_FIELD_VECTOR(vector[1], pMBInfo[k].dpMV[curr_field][1].x, pMBInfo[k].dpMV[curr_field][1].y,YFramePitchRec,UVFramePitchRec);
          SET_FIELD_VECTOR(vector[2], pMBInfo[k].dpMV[curr_field][2].x, pMBInfo[k].dpMV[curr_field][2].y,YFramePitchRec,UVFramePitchRec);
          best->dct_type = DCT_FIELD;
        }else{
          SET_MOTION_VECTOR(vector[0], pMBInfo[k].dpMV[curr_field][0].x, pMBInfo[k].dpMV[curr_field][0].y,YFramePitchRec,UVFramePitchRec);
          SET_MOTION_VECTOR(vector[1], pMBInfo[k].dpMV[curr_field][1].x, pMBInfo[k].dpMV[curr_field][1].y,YFramePitchRec,UVFramePitchRec);
          SET_MOTION_VECTOR(vector[2], pMBInfo[k].dpMV[curr_field][2].x, pMBInfo[k].dpMV[curr_field][2].y,YFramePitchRec,UVFramePitchRec);
          best->dct_type = DCT_FRAME;
        }

        GETDIFF_FIELD_DP(Y, Y, l, pDiff);
        if(encodeInfo.info.color_format == NV12) {
#ifdef IPP_NV12
          GETDIFF_FIELD_DP_NV12(U, V, Y, c, pDiff);
#else
          GETDIFF_FIELD_DP(U, Y, c, pDiff);
          GETDIFF_FIELD_DP(V, Y, c, pDiff);
#endif
        } else {
          GETDIFF_FIELD_DP(U, UV, c, pDiff);
          GETDIFF_FIELD_DP(V, UV, c, pDiff);
        }

        NonIntraMBCoeffs(numTh, best, Count, &CodedBlockPattern);

        PutAddrIncrement(macroblock_address_increment, numTh);
        macroblock_address_increment = 1;

        PUT_MB_MODES(MPEG2_P_PICTURE, MB_FORWARD);
        PutMV_DP(numTh, k, &vector[0][0],&(pMBInfo[k].dpDMV[curr_field]));
      }
#endif //MPEG2_USE_DUAL_PRIME
      else
      {//FIELD prediction
        if (picture_structure == MPEG2_FRAME_PICTURE) {
          SET_FIELD_VECTOR((&vector[0][0]), pMBInfo[k].MV[0][0].x, pMBInfo[k].MV[0][0].y >> 1,YFramePitchRec,UVFramePitchRec);
          SET_FIELD_VECTOR((&vector[1][0]), pMBInfo[k].MV[1][0].x, pMBInfo[k].MV[1][0].y >> 1,YFramePitchRec,UVFramePitchRec);
        } else {
          SET_MOTION_VECTOR((&vector[0][0]), pMBInfo[k].MV[0][0].x, pMBInfo[k].MV[0][0].y,YFramePitchRec,UVFramePitchRec);
          SET_MOTION_VECTOR((&vector[1][0]), pMBInfo[k].MV[1][0].x, pMBInfo[k].MV[1][0].y,YFramePitchRec,UVFramePitchRec);
        }
        //!!! Cases with No motion and Skipped macroblocks are included in FRAME prediction!!!
        GETDIFF_FIELD(Y, Y, l, pDiff, 0);
        //VARMEAN_FIELD(pDiff, best->var, best->mean, best->var_sum);
        if(encodeInfo.info.color_format == NV12) {
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

        PUT_MB_MODES(MPEG2_P_PICTURE, MB_FORWARD);
        PutMV_FIELD(numTh, k, &vector[0][0], &vector[1][0], MB_FORWARD);
#ifdef MPEG2_USE_DUAL_PRIME
        numInterField++;
#endif // MPEG2_USE_DUAL_PRIME
      }

      PUT_BLOCK_PATTERN(CodedBlockPattern);

      // Put blocks and decode
      const Ipp32s *dct_step, *diff_off;
      if (pMBInfo[k].dct_type == DCT_FIELD)
      {
        dct_step = fld_dct_step;
        diff_off = fld_diff_off;
      } else {
        dct_step = frm_dct_step;
        diff_off = frm_diff_off;
      }
      pMBlock = threadSpec[numTh].pMBlock;
      for (blk = 0; blk < block_count; blk++, pMBlock += 64) {
        pDiff = threadSpec[numTh].pDiff + diff_off[blk];
        if (!Count[blk]) {
          ippiSet_16s_C1R(0, pDiff, dct_step[blk], roi8x8);
          continue;
        }
        PutNonIntraBlock(pMBlock, Count[blk], numTh);
        ippiQuantInv_MPEG2_16s_C1I(pMBlock, quantiser_scale_value, NonIntraQMatrix);
        ippiDCT8x8Inv_16s_C1R(pMBlock, pDiff, dct_step[blk]);
      }

      // Motion compensation
      pDiff = threadSpec[numTh].pDiff;

      if(pMBInfo[k].prediction_type == MC_FRAME)
      {
        MC_FRAME_F(Y, Y, l, pDiff);
        if(encodeInfo.info.color_format == NV12) {
#ifdef IPP_NV12
          MC_FRAME_F_NV12(U, V, Y, c, pDiff);
#else
          MC_FRAME_F(U, Y, c, pDiff);
          MC_FRAME_F(V, Y, c, pDiff);
#endif
        } else {
          MC_FRAME_F(U, UV, c, pDiff);
          MC_FRAME_F(V, UV, c, pDiff);
        }
      }
#ifdef MPEG2_USE_DUAL_PRIME
      else if(pMBInfo[k].prediction_type == MC_DMV){
        MC_FIELD_DP(Y, Y, l, pDiff);
        if(encodeInfo.info.color_format == NV12) {
#ifdef IPP_NV12
          MC_FIELD_DP_NV12(U, V, Y, c, pDiff);
#else
          MC_FIELD_DP(U, Y, c, pDiff);
          MC_FIELD_DP(V, Y, c, pDiff);
#endif
        } else {
          MC_FIELD_DP(U, UV, c, pDiff);
          MC_FIELD_DP(V, UV, c, pDiff);
        }
      }
#endif //MPEG2_USE_DUAL_PRIME
      else {
        MC_FIELD_F(Y, Y, l, pDiff);
        if(encodeInfo.info.color_format == NV12) {
#ifdef IPP_NV12
          MC_FIELD_F_NV12(U, V, Y, c, pDiff);
#else
          MC_FIELD_F(U, Y, c, pDiff);
          MC_FIELD_F(V, Y, c, pDiff);
#endif
        } else {
          MC_FIELD_F(U, UV, c, pDiff);
          MC_FIELD_F(V, UV, c, pDiff);
        }
      }

      ippsCopy_8u((Ipp8u*)threadSpec[numTh].PMV, (Ipp8u*)pMBInfo[k].MV, sizeof(threadSpec[0].PMV));

      k++;
    } // for(i)
  } // for(j)

#ifdef MPEG2_USE_DUAL_PRIME
  threadSpec[numTh].numInterFrame = numInterFrame;
  threadSpec[numTh].numInterField = numInterField;
#endif // MPEG2_USE_DUAL_PRIME
  threadSpec[numTh].numIntra = numIntra;
  Ipp32s mul = ipflag ? 0x200 : 0x100/P_distance; // double for IP case
  k = (encodeInfo.numThreads > 1) ? (start_y/16)*MBcountH : 0;
  for(j=start_y; j < stop_y; j += 16)
  {
      for(i=0; i < encodeInfo.info.clip_info.width; i += 16, k++)
      {
        //Ipp32s count0 = 0, count1 = 0;
        //Ipp32s x, y, m, bestm=k, cnt, bestcnt=0;
        //for(y=((j==start_y)?0:-1); y<=((j+16<stop_y)?1:0); y++ ) {
        //  for(x=((i==0)?0:-1); x<=((i+16<encodeInfo.info.clip_info.width)?1:0); x++ ) {
        //    m = k + x + y*MBcountH;
        //    if (m!=k) {
        //      Ipp32s pd0 = PDIST(pMBInfo[m].MV[0][0], pMBInfo[k].MV[0][0]);
        //      Ipp32s pd1 = PDIST(pMBInfo[m].MV[1][0], pMBInfo[k].MV[1][0]);
        //      count0 += ( pd0 <= 5);
        //      count1 += ( pd1 <= 5);
        //      if(m<k) {
        //        cnt = pMBInfo[m].pred_quality[0] + pMBInfo[m].pred_quality[1];
        //        if (bestcnt > cnt) {
        //          bestcnt = cnt;
        //          bestm = m;
        //        }
        //      }
        //    }
        //  }
        //}
        //if (count0 > 1) {
        //  pMBInfo[k].MV_P[0].x = pMBInfo[k].MV[0][0].x*mul;
        //  pMBInfo[k].MV_P[0].y = pMBInfo[k].MV[0][0].y*mul;
        //} else {
        //  pMBInfo[k].MV_P[0] = pMBInfo[bestm].MV_P[0];
        //  //pMBInfo[k].MV_P[0].x = 0;
        //  //pMBInfo[k].MV_P[0].y = 0;
        //}
        //if (count1 > 1) {
        //  pMBInfo[k].MV_P[1].x = pMBInfo[k].MV[1][0].x*mul;
        //  pMBInfo[k].MV_P[1].y = pMBInfo[k].MV[1][0].y*mul;
        //} else {
        //  pMBInfo[k].MV_P[1] = pMBInfo[bestm].MV_P[1];
        //  //pMBInfo[k].MV_P[1].x = 0;
        //  //pMBInfo[k].MV_P[1].y = 0;
        //}
        //pMBInfo[k].pred_quality[0] = count0;
        //pMBInfo[k].pred_quality[1] = count1;
        pMBInfo[k].MV_P[0].x = pMBInfo[k].MV[0][0].x*mul;
        pMBInfo[k].MV_P[0].y = pMBInfo[k].MV[0][0].y*mul;
        pMBInfo[k].MV_P[1].x = pMBInfo[k].MV[1][0].x*mul;
        pMBInfo[k].MV_P[1].y = pMBInfo[k].MV[1][0].y*mul;
      }
  }
}

#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER
#endif // UMC_RESTRICTED_CODE
