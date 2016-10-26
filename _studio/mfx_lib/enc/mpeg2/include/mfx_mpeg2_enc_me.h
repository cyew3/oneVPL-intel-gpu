//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC)

#include <stdlib.h>
#include <limits.h>
#include <ipps.h>
#include "mfx_mpeg2_enc_defs.h"

#define ME_NEW

#if defined(ME_FUNC) && defined(TRY_POSITION_REF) && defined(TRY_POSITION_REC)

mfxStatus MFXVideoENCMPEG2::ME_FUNC(ME_PARAMS)
{
  Ipp32s       step_hor, step_ver, step_min;
  Ipp32s       BestMAD, MAD;
  Ipp32s       X, Y, XN, YN, XMIN = 0, YMIN = 0;
  Ipp32s       min_point;
  const Ipp8u* ref_data;
  Ipp32s       flag;
  Ipp32s       Var[2][4], Mean[2][4];
  Ipp32s       min_index = 0;
  mfxU32       me_matrix_w;
  mfxU32       me_matrix_h;
  mfxU8*       me_matrix;
  //Ipp32s       me_matrix_id;
  Ipp32s       k, num_steps, num_steps1;
  Ipp32s       me_alg_num;
  Ipp32s       field_select = 0;
  Ipp32s       first_search = 1;
  //vm_tick      t_start /*, t_end*/;

  //t_start = GET_TICKS;

  MFX_CHECK_COND(limit_top <= 0);
  MFX_CHECK_COND(limit_left <= 0);
  MFX_CHECK_COND(limit_bottom >= 0);
  MFX_CHECK_COND(limit_right >= 0);

  /* matrix of search points */
  me_matrix_w = (mfxU32)(limit_right + 1 - limit_left);
  me_matrix_h = (mfxU32)(limit_bottom + 1 - limit_top);
  if (me_matrix_w*me_matrix_h > me_matrix_active_size) {
    MFX_CHECK_COND(me_matrix_w*me_matrix_h <= me_matrix_size);
    me_matrix_active_size = me_matrix_w*me_matrix_h;
    ippsZero_8u(me_matrix_buff, me_matrix_active_size);
    me_matrix_id = 0;
  }
  me_matrix = me_matrix_buff - limit_top*me_matrix_w - limit_left;
  //me_matrix_id = th->me_matrix_id;

  /* new ID for matrix of search points */
  me_matrix_id = (me_matrix_id + 1) & 255;
  if (!me_matrix_id) {
    ippsZero_8u(me_matrix_buff, me_matrix_active_size);
    me_matrix_id = 1;
  }
  //th->me_matrix_id = me_matrix_id;


  /* search algorithm */
  me_alg_num = m_me_alg_num;
  //me_alg_num = 9;
  if (me_alg_num >= 10) { // no half-pixel
    me_alg_num -= 10;
    step_min = 2;
  } else {
    step_min = 1;
  }

  if (me_alg_num == 9) { // full search
    BestMAD = INT_MAX;
    for (YN = limit_top; YN <= limit_bottom; YN += 2) {
      for (XN = limit_left; XN <= limit_right; XN += 2) {
        //TRY_POSITION(VARMEAN_FUNC);
        TRY_POSITION_REF(SAD_FUNC);
      }
    }
    X = XMIN;
    Y = YMIN;
    if(step_min > 1)
      goto exit_me;
    BestMAD = INT_MAX;
    me_matrix[YMIN*me_matrix_w + XMIN] = 0;
    for (YN = Y-1; YN <= Y+1; YN += 1) {
      for (XN = X-1; XN <= X+1; XN += 1) {
        if (XN >= limit_left && XN <= limit_right &&
            YN >= limit_top  && YN <= limit_bottom) {
          TRY_POSITION_REC(SAD_FUNC);
        }
      }
    }
    X = XMIN;
    Y = YMIN;
    goto exit_me;
  }

#if 0
  if (Var[min_index][0] <= varThreshold
    && Var[min_index][1] <= varThreshold
#if NUM_BLOCKS == 4
    && Var[min_index][2] <= varThreshold
    && Var[min_index][3] <= varThreshold
#endif /* NUM_BLOCKS == 4 */
    )
  {
    goto exit_me;
  }
#endif

  if (me_alg_num == 3) { // combined search
    if (/*picture_coding_type != MPEG2_P_PICTURE ||*/ (((i>>4) - (j>>4) ) & 7) != 1 ) {
      me_alg_num = 1; // local search
    } else {
      me_alg_num = 2; // global logarithmic search
    }
  }
  //me_alg_num = 2; // global logarithmic search

  switch (me_alg_num) {
  default:
  case 1: // local search
    num_steps = INT_MAX;
    step_hor = step_ver = 2;
    break;
  case 2: // global logarithmic search
    num_steps = 2;
    //step_hor = IPP_MIN(X - limit_right, X - limit_left)/2;
    //step_ver = IPP_MIN(Y - limit_bottom, Y - limit_top)/2;
    step_hor = (limit_right - limit_left)>>3;
    step_ver = (limit_bottom - limit_top)>>3;
    if (step_hor < 2) step_hor = 2;
    if (step_ver < 2) step_ver = 2;
    step_hor = step_ver = IPP_MAX(step_hor, step_ver);
    if(step_hor & 1)
      step_hor = step_ver = step_hor+1;
    break;
  }

  num_steps1 = 1;
  X = 0;
  Y = 0;

  /************** SEARCH LOOP **************/
  /* initial point */
  BestMAD = INT_MAX;
  XN = X;
  YN = Y;
  TRY_POSITION_REF(SAD_FUNC);

  if (first_search) {
    first_search = 0;
    XN = InitialMV0.x;
    YN = InitialMV0.y >> FIELD_FLAG;
    XN = XN &~ 1;
    YN = YN &~ 1;
    if (XN >= limit_left && XN <= limit_right &&
        YN >= limit_top  && YN <= limit_bottom) {
      TRY_POSITION_REF(SAD_FUNC);
    }
    /*if (P_distance == 1)*/ { // without B-frames
      XN = InitialMV1.x;
      YN = InitialMV1.y >> FIELD_FLAG;
      XN = XN &~ 1;
      YN = YN &~ 1;
      if (XN >= limit_left && XN <= limit_right &&
          YN >= limit_top  && YN <= limit_bottom) {
        TRY_POSITION_REF(SAD_FUNC);
      }
    }
    XN = InitialMV2.x;
    YN = InitialMV2.y >> FIELD_FLAG;
    XN = XN &~ 1;
    YN = YN &~ 1;
    if (XN >= limit_left && XN <= limit_right &&
        YN >= limit_top  && YN <= limit_bottom) {
      TRY_POSITION_REF(SAD_FUNC);
    }
    X = XMIN;
    Y = YMIN;

    /*if (InitialMV1 != MV_ZERO) {
    k - MBcountH*/
  }

//  BestMAD -= 100;

  while (step_hor >= step_min || step_ver >= step_min)
  {
    for (k = 0; k < num_steps; k++) {
      min_point = 0;

      YN = Y;
      XN = X - step_hor;
      if (XN >= limit_left)
      {
        TRY_POSITION_REF(SAD_FUNC);
      }
      XN = X + step_hor;
      if (XN <= limit_right)
      {
        TRY_POSITION_REF(SAD_FUNC);
      }

      XN = X;
      YN = Y - step_ver;
      if (YN >= limit_top)
      {
        TRY_POSITION_REF(SAD_FUNC);
      }
      YN = Y + step_ver;
      if (YN <= limit_bottom)
      {
        TRY_POSITION_REF(SAD_FUNC);
      }

      X = XMIN;
      Y = YMIN;

      if (!min_point) break;
    }

    step_hor >>= 1;
    step_ver >>= 1;
#ifdef ME_NEW
    if(step_hor==1 && step_ver==1)
      break;
    if(step_hor & 1)
      step_hor = step_ver = step_hor+1;
#endif
    num_steps = num_steps1;
  }

#ifdef ME_NEW
#ifdef ME_REF_ORIGINAL
  BestMAD = INT_MAX;
  XN = X;
  YN = Y;
  me_matrix[YN*me_matrix_w + XN] = 0;
  TRY_POSITION_REC(SAD_FUNC);
#endif /* ME_REF_ORIGINAL */
//find_hp:
  for (k = 0; k < 2; k++) {
      min_point = 0;

      YN = Y;
      XN = X - 1;
      if (XN >= limit_left)
      {
        TRY_POSITION_REC(SAD_FUNC);
      }
      XN = X + 1;
      if (XN <= limit_right)
      {
        TRY_POSITION_REC(SAD_FUNC);
      }

      XN = X;
      YN = Y - 1;
      if (YN >= limit_top)
      {
        TRY_POSITION_REC(SAD_FUNC);
      }
      YN = Y + 1;
      if (YN <= limit_bottom)
      {
        TRY_POSITION_REC(SAD_FUNC);
      }

      X = XMIN;
      Y = YMIN;

      if (!min_point) break;
  }
#endif

exit_me:
  // metric VAR=SUM(x*x)
  BestMAD = INT_MAX;
  XN = X;
  YN = Y;
  me_matrix[YN*me_matrix_w + XN] = 0;
  TRY_POSITION_REC(VARMEAN_FUNC);

  field_select = m_cuc->FrameParam->MPEG2.BottomFieldFlag ^ parity;

  if (currMAD != NULL) {
    if (BestMAD >= *currMAD)
      goto end_me;
    *currMAD = BestMAD;
  }
  if (pDstVar != NULL) {
    MFX_INTERNAL_CPY(pDstVar,  Var[min_index],  NUM_BLOCKS*sizeof(*pDstVar));
  }
  if (pDstMean != NULL) {
    MFX_INTERNAL_CPY(pDstMean, Mean[min_index], NUM_BLOCKS*sizeof(*pDstMean));
  }
#if FIELD_FLAG == 0
  SET_MOTION_VECTOR(vector, XMIN, YMIN);
#else
  SET_FIELD_VECTOR(vector, XMIN, YMIN);
#endif
  *vertical_field_select = field_select;

end_me:
#ifdef MPEG2_DEBUG_CODE
  t_end = GET_TICKS;
  motion_estimation_time += (Ipp64s)(t_end - t_start);
#endif

  if (resVar != NULL)
    *resVar = BestMAD;
  return MFX_ERR_NONE;
}

#endif

#endif // MFX_ENABLE_MPEG2_VIDEO_ENC
