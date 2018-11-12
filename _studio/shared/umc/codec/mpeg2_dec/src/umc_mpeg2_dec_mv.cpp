// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_mpeg2_dec_defs_sw.h"
#include "umc_mpeg2_dec_sw.h"

// turn off the "conversion from 'type1' to 'type2', possible loss of data" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4244)
#endif

using namespace UMC;

#define DECL_MV_DERIVED(k) \
    int16_t *c_mv     = &video->vector[2*(k)]; \
    int16_t *c_pmv    = &video->PMV[2*(k)];

Status MPEG2VideoDecoderSW::update_mv(int16_t *pval, int32_t s, VideoContext *video, int task_num)
{
  int32_t val = *pval;
  int32_t residual, mcode;
  DECODE_VLC(mcode, video->bs, vlcMotionVector);
  UPDATE_MV(val, mcode, s, task_num);
  *pval = (int16_t)val;
  return UMC_OK;
}

Status MPEG2VideoDecoderSW::mv_decode(int32_t r, int32_t s, VideoContext *video, int task_num)
{
    DECL_MV_DERIVED(2*r+s)
    int32_t residual, mcode;
    int32_t framefieldflag = (video->prediction_type == IPPVC_MC_FIELD) &&
                            (PictureHeader[task_num].picture_structure == FRAME_PICTURE);

    // Decode x vector
    c_mv[0] = c_pmv[0];
    if (IS_NEXTBIT1(video->bs)) {
      SKIP_BITS(video->bs, 1)
    } else {
      DECODE_VLC(mcode, video->bs, vlcMotionVector);
      int32_t cmv = c_mv[0];
      UPDATE_MV(cmv, mcode, 2*s, task_num);
      c_pmv[0] = c_mv[0] = (int16_t)cmv;
    }

    // Decode y vector
    c_mv[1] = (int16_t)(c_pmv[1] >> framefieldflag); // frame y to field
    if (IS_NEXTBIT1(video->bs)) {
      SKIP_BITS(video->bs, 1)
    } else {
      DECODE_VLC(mcode, video->bs, vlcMotionVector);
      int32_t cmv = c_mv[1];
      UPDATE_MV(cmv, mcode, 2*s + 1, task_num);
      c_mv[1] = (int16_t)cmv;
    }
    c_pmv[1] = (int16_t)(c_mv[1] << framefieldflag); // field y back to frame

    return UMC_OK;
}

Status MPEG2VideoDecoderSW::mv_decode_dp(VideoContext *video, int task_num)
{
  DECL_MV_DERIVED(0)
  int32_t residual, mcode;
  int32_t dmv0 = 0, dmv1 = 0;
  int32_t ispos0, ispos1;

  // Decode x vector
  DECODE_VLC(mcode, video->bs, vlcMotionVector);
  c_mv[0] = c_pmv[0];
  if(mcode) {
    int32_t cmv = c_mv[0];
    UPDATE_MV(cmv,mcode, 0, task_num);
    c_pmv[0] = c_mv[0] = (int16_t)cmv;
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
    int32_t cmv = c_mv[1];
    UPDATE_MV(cmv, mcode, 1, task_num);
    c_pmv[1] = c_mv[1] = (int16_t)cmv;
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
      c_mv[4] = (int16_t)(((c_pmv[0] + ispos0)>>1) + dmv0);
      c_mv[5] = (int16_t)(((c_pmv[1] + ispos1)>>1) + dmv1 - 1);

      /* vector for prediction of bottom field from top field */
      c_mv[6] = (int16_t)(((3*c_pmv[0] + ispos0)>>1) + dmv0);
      c_mv[7] = (int16_t)(((3*c_pmv[1] + ispos1)>>1) + dmv1 + 1);
    }
    else
    {
      /* vector for prediction of top field from bottom field */
      c_mv[4] = (int16_t)(((3*c_pmv[0] + ispos0)>>1) + dmv0);
      c_mv[5] = (int16_t)(((3*c_pmv[1] + ispos1)>>1) + dmv1 - 1);

      /* vector for prediction of bottom field from top field */
      c_mv[6] = (int16_t)(((c_pmv[0] + ispos0)>>1) + dmv0);
      c_mv[7] = (int16_t)(((c_pmv[1] + ispos1)>>1) + dmv1 + 1);
    }

    c_pmv[1] <<= 1;
    c_pmv[4] = c_pmv[0];
    c_pmv[5] = c_pmv[1];
  }
  else
  {
    //Dual Prime Arithmetic
    // vector for prediction from field of opposite parity
    c_mv[4] = (int16_t)(((c_pmv[0]+ispos0)>>1) + dmv0);
    c_mv[5] = (int16_t)(((c_pmv[1]+ispos1)>>1) + dmv1);

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
