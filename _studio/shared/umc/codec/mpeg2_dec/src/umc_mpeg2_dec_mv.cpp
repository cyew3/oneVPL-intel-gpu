//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2011 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_mpeg2_dec_defs_sw.h"
#include "umc_mpeg2_dec_sw.h"

#pragma warning(disable: 4244)

using namespace UMC;

#define DECL_MV_DERIVED(k) \
    Ipp16s *c_mv     = &video->vector[2*(k)]; \
    Ipp16s *c_pmv    = &video->PMV[2*(k)];

Status MPEG2VideoDecoderSW::update_mv(Ipp16s *pval, Ipp32s s, VideoContext *video, int task_num)
{
  Ipp32s val = *pval;
  Ipp32s residual, mcode;
  DECODE_VLC(mcode, video->bs, vlcMotionVector);
  UPDATE_MV(val, mcode, s, task_num);
  *pval = (Ipp16s)val;
  return UMC_OK;
}

Status MPEG2VideoDecoderSW::mv_decode(Ipp32s r, Ipp32s s, VideoContext *video, int task_num)
{
    DECL_MV_DERIVED(2*r+s)
    Ipp32s residual, mcode;
    Ipp32s framefieldflag = (video->prediction_type == IPPVC_MC_FIELD) &&
                            (PictureHeader[task_num].picture_structure == FRAME_PICTURE);

    // Decode x vector
    c_mv[0] = c_pmv[0];
    if (IS_NEXTBIT1(video->bs)) {
      SKIP_BITS(video->bs, 1)
    } else {
      DECODE_VLC(mcode, video->bs, vlcMotionVector);
      Ipp32s cmv = c_mv[0];
      UPDATE_MV(cmv, mcode, 2*s, task_num);
      c_pmv[0] = c_mv[0] = (Ipp16s)cmv;
    }

    // Decode y vector
    c_mv[1] = (Ipp16s)(c_pmv[1] >> framefieldflag); // frame y to field
    if (IS_NEXTBIT1(video->bs)) {
      SKIP_BITS(video->bs, 1)
    } else {
      DECODE_VLC(mcode, video->bs, vlcMotionVector);
      Ipp32s cmv = c_mv[1];
      UPDATE_MV(cmv, mcode, 2*s + 1, task_num);
      c_mv[1] = (Ipp16s)cmv;
    }
    c_pmv[1] = (Ipp16s)(c_mv[1] << framefieldflag); // field y back to frame

    return UMC_OK;
}

Status MPEG2VideoDecoderSW::mv_decode_dp(VideoContext *video, int task_num)
{
  DECL_MV_DERIVED(0)
  Ipp32s residual, mcode;
  Ipp32s dmv0 = 0, dmv1 = 0;
  Ipp32s ispos0, ispos1;

  // Decode x vector
  DECODE_VLC(mcode, video->bs, vlcMotionVector);
  c_mv[0] = c_pmv[0];
  if(mcode) {
    Ipp32s cmv = c_mv[0];
    UPDATE_MV(cmv,mcode, 0, task_num);
    c_pmv[0] = c_mv[0] = (Ipp16s)cmv;
  }
  //get dmv0
  GET_1BIT(video->bs, dmv0);
  if(dmv0 == 1)
  {
    GET_1BIT(video->bs, dmv0);
    dmv0 = (dmv0 == 1)?-1:1;
  }

  // Decode y vector
  if (PictureHeader[task_num].picture_structure == FRAME_PICTURE)
    c_pmv[1] >>= 1;
  DECODE_VLC(mcode, video->bs, vlcMotionVector);
  c_mv[1] = c_pmv[1];
  if(mcode) {
    Ipp32s cmv = c_mv[1];
    UPDATE_MV(cmv, mcode, 1, task_num);
    c_pmv[1] = c_mv[1] = (Ipp16s)cmv;
  }
  //get dmv1
  GET_1BIT(video->bs, dmv1);
  if(dmv1 == 1)
  {
    GET_1BIT(video->bs, dmv1);
    dmv1 = (dmv1 == 1)?-1:1;
  }

  //Dual Prime Arithmetic
  ispos0 = c_pmv[0]>=0;
  ispos1 = c_pmv[1]>=0;
  if (PictureHeader[task_num].picture_structure == FRAME_PICTURE)
  {
    if (PictureHeader[task_num].top_field_first    )
    {
      /* vector for prediction of top field from bottom field */
      c_mv[4] = (Ipp16s)(((c_pmv[0] + ispos0)>>1) + dmv0);
      c_mv[5] = (Ipp16s)(((c_pmv[1] + ispos1)>>1) + dmv1 - 1);

      /* vector for prediction of bottom field from top field */
      c_mv[6] = (Ipp16s)(((3*c_pmv[0] + ispos0)>>1) + dmv0);
      c_mv[7] = (Ipp16s)(((3*c_pmv[1] + ispos1)>>1) + dmv1 + 1);
    }
    else
    {
      /* vector for prediction of top field from bottom field */
      c_mv[4] = (Ipp16s)(((3*c_pmv[0] + ispos0)>>1) + dmv0);
      c_mv[5] = (Ipp16s)(((3*c_pmv[1] + ispos1)>>1) + dmv1 - 1);

      /* vector for prediction of bottom field from top field */
      c_mv[6] = (Ipp16s)(((c_pmv[0] + ispos0)>>1) + dmv0);
      c_mv[7] = (Ipp16s)(((c_pmv[1] + ispos1)>>1) + dmv1 + 1);
    }

    c_pmv[1] <<= 1;
    c_pmv[4] = c_pmv[0];
    c_pmv[5] = c_pmv[1];
  }
  else
  {
    //Dual Prime Arithmetic
    // vector for prediction from field of opposite parity
    c_mv[4] = (Ipp16s)(((c_pmv[0]+ispos0)>>1) + dmv0);
    c_mv[5] = (Ipp16s)(((c_pmv[1]+ispos1)>>1) + dmv1);

    /* correct for vertical field shift */
    if (PictureHeader[task_num].picture_structure == TOP_FIELD)
      c_mv[5]--;
    else
      c_mv[5]++;
  }
  c_pmv[4] = c_pmv[0];
  c_pmv[5] = c_pmv[1];

  return UMC_OK;
}

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
