//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#include <ipps.h>
#define ME_NEW

#if defined(ME_FUNC) && defined(TRY_POSITION_REF) && defined(TRY_POSITION_REC)

#if defined (__ICL)
//transfer of control bypasses initialization of.
// goto operator has possible issue with crossplatform
#pragma warning(disable:589)
#endif

#if defined (__ICL)
//non-pointer conversion from "unsigned __int64" to "Ipp32s={signed int}" may lose significant bits
#pragma warning(disable:2259)
#endif
Ipp32s MPEG2VideoEncoderBase::ME_FUNC(ME_PARAMS)
{
  Ipp32s       step_hor, step_ver, step_min;
  Ipp32s       BestMAD = INT_MAX, MAD = INT_MAX;
  Ipp32s       X, Y, XN, YN, XMIN = 0, YMIN = 0;
  Ipp32s       min_point;
  const Ipp8u* ref_data;
  Ipp32s       flag;
  Ipp32s       Var[2][4], Mean[2][4];
  Ipp32s       min_index = 0;
  Ipp32s       me_matrix_w;
  Ipp32s       me_matrix_h;
  Ipp8u       *me_matrix;
  Ipp32s       me_matrix_id;
  Ipp32s       k, num_steps, num_steps1;
  Ipp32s       me_alg_num;
  Ipp32s       field_select = 0;
  Ipp32s       first_search = 1;
  //vm_tick      t_start;

  //t_start = GET_TICKS;

  mpeg2_assert(limit_top <= 0);
  mpeg2_assert(limit_left <= 0);
  mpeg2_assert(limit_bottom >= 0);
  mpeg2_assert(limit_right >= 0);

  /* matrix of search points */
  me_matrix_w = limit_right + 1 - limit_left;
  me_matrix_h = limit_bottom + 1 - limit_top;
  if (me_matrix_w*me_matrix_h > th->me_matrix_size) {
    if (th->me_matrix_buff) MP2_FREE(th->me_matrix_buff);
    th->me_matrix_size = me_matrix_w*me_matrix_h;
    th->me_matrix_buff = MP2_ALLOC(Ipp8u, th->me_matrix_size);
    if (NULL == th->me_matrix_buff)
      goto end_me;
    ippsZero_8u(th->me_matrix_buff, th->me_matrix_size);
    th->me_matrix_id = 0;
  }
  me_matrix = th->me_matrix_buff - limit_top*me_matrix_w - limit_left;
  me_matrix_id = th->me_matrix_id;

  /* new ID for matrix of search points */
  me_matrix_id = (me_matrix_id + 1) & 255;
  if (!me_matrix_id) {
    ippsZero_8u(th->me_matrix_buff, th->me_matrix_size);
    me_matrix_id = 1;
  }
  th->me_matrix_id = me_matrix_id;

#ifndef UMC_RESTRICTED_CODE
  //{
  //  Ipp32s l = limit_left   >> 1;
  //  Ipp32s r = (limit_right+1)  >> 1;
  //  Ipp32s t = limit_top    >> 1;
  //  Ipp32s b = (limit_bottom+1) >> 1;

  //  MVSearch(pRecFld + l + t*RefStep, RefStep, pBlock, NUM_BLOCKS*4,
  //    l, t, r, b, &X, &Y);
  //  goto exit_me;
  //}
#endif // UMC_RESTRICTED_CODE

  /* search algorithm */
  me_alg_num = encodeInfo.me_alg_num;
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
    step_hor = (limit_right - limit_left)>>3;
    step_ver = (limit_bottom - limit_top)>>3;
    if (step_hor < 2) step_hor = 2;
    if (step_ver < 2) step_ver = 2;
    step_hor = step_ver = IPP_MAX(step_hor, step_ver);
    if(step_hor & 1)
      step_hor = step_ver = step_hor+1;
    break;
  }

#ifndef UMC_RESTRICTED_CODE
  //{
  //  Ipp32s l = limit_left   >> 1;
  //  Ipp32s r = (limit_right+1)  >> 1;
  //  Ipp32s t = limit_top    >> 1;
  //  Ipp32s b = (limit_bottom+1) >> 1;

  //  MVSearchPh(pRecFld, RefStep, pBlock, NUM_BLOCKS*4,
  //    l, t, r, b, &X, &Y);
  //  BestMAD = INT_MAX;
  //  XN = X;
  //  YN = Y;
  //  //me_matrix[YN*me_matrix_w + XN] = 0;
  //  TRY_POSITION_REC(SAD_FUNC);
  //  //goto find_hp;
  //}
#endif // UMC_RESTRICTED_CODE

  num_steps1 = 1;
  X = 0;
  Y = 0;

  /************** SEARCH LOOP **************/
  /* initial point */
  BestMAD = INT_MAX;
#ifdef MPEG2_USE_DUAL_PRIME
  if(dpflag)//to predict from opposite field in DP mode use special direction
  {
    Ipp32s sX,sY,isposX,isposY;
    Ipp32s only_hp = 0;//use only half pix prediction

    if(picture_structure == MPEG2_FRAME_PICTURE)
    {
      if(th->fld_vec_count==2||th->fld_vec_count==3)//top->bottom||bottom->top
      {
        sX=th->dp_vec[1].x;
        sY=th->dp_vec[1].y;
        isposX=sX>=0;
        isposY=sY>=0;

        if(th->fld_vec_count==3-top_field_first)//top->bottom (top_field_first==1)
        {
          X = (sX+isposX)>>1;
          Y = ((sY+isposY)>>1)-2*top_field_first+1;
          only_hp = 1;
        }
        else if(th->fld_vec_count==2+top_field_first)//bottom->top (top_field_first==1)
        {
          X = (3*sX+isposX)>>1;
          Y = ((3*sY+isposY)>>1)+2*top_field_first-1;
          only_hp = 1;
        }
      }
      else if(th->fld_vec_count==1 || th->fld_vec_count==4)//same parity prediction
      {
        //try frame prediction
        XN = th->dp_vec[0].x &~ 1;
        YN = (th->dp_vec[0].y>>1) &~ 1;
        if (XN >= limit_left && XN <= limit_right &&
            YN >= limit_top  && YN <= limit_bottom) {
          TRY_POSITION_REF(SAD_FUNC);
        }
        //try (0,0)
        X = 0;
        Y = 0;
      }
    }
    else
    {
      if(th->fld_vec_count==3 || th->fld_vec_count==4)
      {
        sX=th->dp_vec[2].x;
        sY=th->dp_vec[2].y;
        isposX=sX>=0;
        isposY=sY>=0;

        X = (sX+isposX)>>1;
        Y = (sY+isposY)>>1;
        if(picture_structure == MPEG2_TOP_FIELD) Y--;
        else Y++;
        only_hp = 1;
      }
      else if(th->fld_vec_count==2 || th->fld_vec_count==5)//same parity prediction
      {
        //try frame prediction
        XN = th->dp_vec[0].x &~ 1;
        YN = th->dp_vec[0].y &~ 1;
        if (XN >= limit_left && XN <= limit_right &&
            YN >= limit_top  && YN <= limit_bottom) {
          TRY_POSITION_REF(SAD_FUNC);
        }
        //opposite frame prediction
        XN = (th->dp_vec[1].x*2) &~ 1;
        if(picture_structure == MPEG2_TOP_FIELD) YN = ((th->dp_vec[1].y-1)*2) &~ 1;
        else YN = ((th->dp_vec[1].y+1)*2) &~ 1;
        if (XN >= limit_left && XN <= limit_right &&
            YN >= limit_top  && YN <= limit_bottom) {
          TRY_POSITION_REF(SAD_FUNC);
        }
        //try (0,0)
        X = 0;
        Y = 0;
      }
    }

    if(only_hp)
    {
      //first_search = 0;
      //step_hor = step_ver = 0;

      if(X < limit_left)X = limit_left;
      else if(X > limit_right)X = limit_right;

      if(Y < limit_top)Y = limit_top;
      else if(Y > limit_bottom)Y = limit_bottom;
    }
  }
#endif //MPEG2_USE_DUAL_PRIME
  XN = X;
  YN = Y;
  TRY_POSITION_REF(SAD_FUNC);

#ifdef MPEG2_USE_DUAL_PRIME
  if(picture_structure != MPEG2_FRAME_PICTURE && th->fld_vec_count == 1)
    th->field_diff_SAD += MAD;
#endif //MPEG2_USE_DUAL_PRIME

  if (first_search) {
    // first_search = 0;
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

  field_select = curr_field ^ parity;

  if (currMAD != NULL) {
    if (BestMAD >= *currMAD)
      goto end_me;
    *currMAD = BestMAD;
  }
  if (pDstVar != NULL) {
    ippsCopy_8u((Ipp8u*)Var[min_index], (Ipp8u*)pDstVar, NUM_BLOCKS*sizeof(*pDstVar));
  }
  if (pDstMean != NULL) {
    ippsCopy_8u((Ipp8u*)Mean[min_index], (Ipp8u*)pDstMean, NUM_BLOCKS*sizeof(*pDstMean));
  }
#if FIELD_FLAG == 0
  SET_MOTION_VECTOR(vector, XMIN, YMIN,YFramePitchRef,UVFramePitchRef );
#else
  SET_FIELD_VECTOR(vector, XMIN, YMIN,YFramePitchRef,UVFramePitchRef);
#endif
  *vertical_field_select = field_select;

end_me:
#ifdef MPEG2_DEBUG_CODE
  t_end = GET_TICKS;
  motion_estimation_time += (Ipp64s)(t_end - t_start);
#endif

#ifdef MPEG2_USE_DUAL_PRIME
  if(th->fld_vec_count<6)
  {
    th->dp_vec[th->fld_vec_count].x=XMIN;
    th->dp_vec[th->fld_vec_count].y=YMIN;
    th->fld_vec_count++;
  }
#endif//MPEG2_USE_DUAL_PRIME

  return BestMAD;
}

#endif

#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER
