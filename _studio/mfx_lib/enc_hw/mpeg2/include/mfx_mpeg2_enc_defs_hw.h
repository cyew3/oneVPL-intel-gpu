//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_MPEG2_ENC_DEFS_H_
#define _MFX_MPEG2_ENC_DEFS_H_

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC)

//#include "mfx_mpeg2_enc.h"
#include "ippvc.h"

#define DCT_FRAME  0
#define DCT_FIELD  1

#define PICTURE_START_CODE 0x100L
#define SLICE_MIN_START    0x101L
#define SLICE_MAX_START    0x1AFL
#define USER_START_CODE    0x1B2L
#define SEQ_START_CODE     0x1B3L
#define EXT_START_CODE     0x1B5L
#define SEQ_END_CODE       0x1B7L
#define GOP_START_CODE     0x1B8L
#define ISO_END_CODE       0x1B9L
#define PACK_START_CODE    0x1BAL
#define SYSTEM_START_CODE  0x1BBL

///* picture_structure ISO/IEC 13818-2, 6.3.10 table 6-14 */
//enum
//{
//  TOP_FIELD     = 1,
//  BOTTOM_FIELD  = 2,
//  FRAME_PICTURE = 3
//};

/* macroblock type */
#define MB_INTRA    1
#define MB_PATTERN  2
#define MB_BACKWARD 4
#define MB_FORWARD  8
#define MB_QUANT    16

/* motion_type */
#define MC_FIELD 1
#define MC_FRAME 2
#define MC_16X8  2
#define MC_DMV   3

/* extension start code IDs */

#define SEQ_ID       1
#define DISP_ID      2
#define QUANT_ID     3
#define SEQSCAL_ID   5
#define PANSCAN_ID   7
#define CODING_ID    8
#define SPATSCAL_ID  9
#define TEMPSCAL_ID 10

#define OFF_Y    0
#define OFF_U  256
#define OFF_V  512
#define BlkStride_l  16
#define BlkStride_c  16
#define BlkWidth_l   16
#define BlkHeight_l  16
#define BlkStride_l  16

#define func_getdiff_frame_l  ippiGetDiff16x16_8u16s_C1
#define func_getdiff_field_l  ippiGetDiff16x8_8u16s_C1
#define func_getdiffB_frame_l ippiGetDiff16x16B_8u16s_C1
#define func_getdiffB_field_l ippiGetDiff16x8B_8u16s_C1
#define func_mc_frame_l  ippiMC16x16_8u_C1
#define func_mc_field_l  ippiMC16x8_8u_C1
#define func_mcB_frame_l ippiMC16x16B_8u_C1
#define func_mcB_field_l ippiMC16x8B_8u_C1

typedef struct {
  Ipp32s mb_type;
  Ipp32s dct_type;
  Ipp32s pred_type;
  Ipp32s var_sum;
  Ipp32s var[4];
  Ipp32s mean[4];
  Ipp16s *pDiff;
} MB_prediction_info;

#define SET_MOTION_VECTOR(vectorF, mv_x, mv_y) {                          \
  Ipp32s x_l = mv_x;                                                      \
  Ipp32s y_l = mv_y;                                                      \
  Ipp32s i_c = (BlkWidth_c  == 16) ? i : (i >> 1);                        \
  Ipp32s j_c = (BlkHeight_c == 16) ? j : (j >> 1);                        \
  Ipp32s x_c = (BlkWidth_c  == 16) ? x_l : (x_l/2);                       \
  Ipp32s y_c = (BlkHeight_c == 16) ? y_l : (y_l/2);                       \
  vectorF->x = x_l;                                                       \
  vectorF->y = y_l;                                                       \
  vectorF->mctype_l = ((x_l & 1) << 3) | ((y_l & 1) << 2);                \
  vectorF->mctype_c = ((x_c & 1) << 3) | ((y_c & 1) << 2);                \
  vectorF->offset_l = (i   + (x_l >> 1)) + (j   + (y_l >> 1)) * state->YFrameHSize;  \
  vectorF->offset_c = (i_c + (x_c >> 1)) + (j_c + (y_c >> 1)) * state->UVFrameHSize; \
  if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) vectorF->offset_c *= 2; \
}

// doubt about field pictures
#define SET_FIELD_VECTOR(vectorF, mv_x, mv_y) {                           \
  Ipp32s x_l = mv_x;                                                      \
  Ipp32s y_l = mv_y;                                                      \
  Ipp32s i_c = (BlkWidth_c  == 16) ? i : (i >> 1);                        \
  Ipp32s j_c = (BlkHeight_c == 16) ? j : (j >> 1);                        \
  Ipp32s x_c = (BlkWidth_c  == 16) ? x_l : (x_l/2);                       \
  Ipp32s y_c = (BlkHeight_c == 16) ? y_l : (y_l/2);                       \
  vectorF->x = x_l;                                                       \
  vectorF->y = y_l;                                                       \
  vectorF->mctype_l = ((x_l & 1) << 3) | ((y_l & 1) << 2);                \
  vectorF->mctype_c = ((x_c & 1) << 3) | ((y_c & 1) << 2);                \
  vectorF->offset_l = (i   + (x_l >> 1)) + (j   + (y_l &~ 1) ) * state->YFrameHSize; \
  vectorF->offset_c = (i_c + (x_c >> 1)) + (j_c + (y_c &~ 1) ) * state->UVFrameHSize;\
  if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) vectorF->offset_c *= 2; \
}

#define GETDIFF_FRAME(X, CC, C, pDiff, DIR) \
  func_getdiff_frame_##C(                   \
    X##Block,                               \
    CC##FrameHSize,                         \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[2][DIR]][DIR] + vector[2][DIR].offset_##C, \
    CC##FrameHSize,                         \
    pDiff + OFF_##X,                        \
    2*BlkStride_##C,                        \
    0, 0, vector[2][DIR].mctype_##C, 0)

#define GETDIFF_FRAME_FB(X, CC, C, pDiff)   \
  func_getdiffB_frame_##C(X##Block,         \
    CC##FrameHSize,                         \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[2][0]][0] + vector[2][0].offset_##C, \
    CC##FrameHSize,                         \
    vector[2][0].mctype_##C,                \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[2][1]][1] + vector[2][1].offset_##C, \
    CC##FrameHSize,                         \
    vector[2][1].mctype_##C,                \
    pDiff + OFF_##X,                        \
    2*BlkStride_##C,                        \
    ippRndZero)

#define GETDIFF_FIELD(X, CC, C, pDiff, DIR)            \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {          \
  func_getdiff_field_##C(X##Block,                     \
    2*CC##FrameHSize,                                  \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[0][DIR]][DIR] +vector[0][DIR].offset_##C, \
    2*CC##FrameHSize,                                  \
    pDiff + OFF_##X,                                   \
    4*BlkStride_##C,                                   \
    0, 0, vector[0][DIR].mctype_##C, 0);               \
                                                       \
  func_getdiff_field_##C(X##Block + CC##FrameHSize,    \
    2*CC##FrameHSize,                                  \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[1][DIR]][DIR] + vector[1][DIR].offset_##C, \
    2*CC##FrameHSize,                                  \
    pDiff + OFF_##X + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    0, 0, vector[1][DIR].mctype_##C, 0);               \
} else {                                               \
  func_getdiff_field_##C(X##Block,                     \
    CC##FrameHSize,                                    \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[0][DIR]][DIR] + vector[0][DIR].offset_##C, \
    CC##FrameHSize,                                    \
    pDiff + OFF_##X,                                   \
    2*BlkStride_##C,                                   \
    0, 0, vector[0][DIR].mctype_##C, 0);               \
                                                       \
  func_getdiff_field_##C(X##Block + (BlkHeight_##C/2)*CC##FrameHSize, \
    CC##FrameHSize,                                    \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[1][DIR]][DIR] + vector[1][DIR].offset_##C + (BlkHeight_##C/2)*CC##FrameHSize, \
    CC##FrameHSize,                                    \
    pDiff + OFF_##X + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                                   \
    0, 0, vector[1][DIR].mctype_##C, 0);               \
}

#define GETDIFF_FIELD_FB(X, CC, C, pDiff)              \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {          \
  func_getdiffB_field_##C(X##Block,                    \
    2*CC##FrameHSize,                                  \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C, \
    2*CC##FrameHSize,                                  \
    vector[0][0].mctype_##C,                           \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[0][1]][1] + vector[0][1].offset_##C, \
    2*CC##FrameHSize,                                  \
    vector[0][1].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    4*BlkStride_##C,                                   \
    ippRndZero);                                       \
                                                       \
  func_getdiffB_field_##C(X##Block + CC##FrameHSize,   \
    2*CC##FrameHSize,                                  \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C, \
    2*CC##FrameHSize,                                  \
    vector[1][0].mctype_##C,                           \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[1][1]][1] + vector[1][1].offset_##C, \
    2*CC##FrameHSize,                                  \
    vector[1][1].mctype_##C,                           \
    pDiff + OFF_##X + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    ippRndZero);                                       \
} else {                                               \
  func_getdiffB_field_##C(X##Block,                    \
    CC##FrameHSize,                                    \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C,\
    CC##FrameHSize,                                    \
    vector[0][0].mctype_##C,                           \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[0][1]][1] + vector[0][1].offset_##C,\
    CC##FrameHSize,                                    \
    vector[0][1].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    2*BlkStride_##C,                                   \
    ippRndZero);                                       \
                                                       \
  func_getdiffB_field_##C(                             \
    X##Block + (BlkHeight_##C/2)*CC##FrameHSize,       \
    CC##FrameHSize,                                    \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C + (BlkHeight_##C/2)*CC##FrameHSize,  \
    CC##FrameHSize,                                    \
    vector[1][0].mctype_##C,                           \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[1][1]][1] + vector[1][1].offset_##C + (BlkHeight_##C/2)*CC##FrameHSize, \
    CC##FrameHSize,                                    \
    vector[1][1].mctype_##C,                           \
    pDiff + OFF_##X + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                                   \
    ippRndZero);                                       \
}

#define MC_FRAME_F(X, CC, C, pDiff) \
  func_mc_frame_##C(                \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[2][0]][0] + vector[2][0].offset_##C, \
    CC##FrameHSize,                 \
    pDiff + OFF_##X,                \
    2*BlkStride_##C,                \
    X##BlockRec,                    \
    CC##FrameHSize,                 \
    vector[2][0].mctype_##C,        \
    (IppRoundMode)0 )

#define MC_FIELD_F(X, CC, C, pDiff)            \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {  \
  func_mc_field_##C(                           \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C, \
    2*CC##FrameHSize,                          \
    pDiff + OFF_##X,                           \
    4*BlkStride_##C,                           \
    X##BlockRec,                               \
    2*CC##FrameHSize,                          \
    vector[0][0].mctype_##C,                   \
    (IppRoundMode)0 );                         \
                                               \
  func_mc_field_##C(                           \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C, \
    2*CC##FrameHSize,                          \
    pDiff + OFF_##X + BlkStride_##C,           \
    4*BlkStride_##C,                           \
    X##BlockRec + CC##FrameHSize,              \
    2*CC##FrameHSize,                          \
    vector[1][0].mctype_##C,                   \
    (IppRoundMode)0 );                         \
} else {                                       \
  func_mc_field_##C(                           \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C, \
    CC##FrameHSize,                            \
    pDiff + OFF_##X,                           \
    2*BlkStride_##C,                           \
    X##BlockRec,                               \
    CC##FrameHSize,                            \
    vector[0][0].mctype_##C,                   \
    (IppRoundMode)0 );                         \
                                               \
  func_mc_field_##C(                           \
    state->X##RecFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C + (BlkHeight_##C/2)*CC##FrameHSize, \
    CC##FrameHSize,                            \
    pDiff + OFF_##X + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                           \
    X##BlockRec + (BlkHeight_##C/2)*CC##FrameHSize, \
    CC##FrameHSize,                            \
    vector[1][0].mctype_##C,                   \
    (IppRoundMode)0 );                         \
}

#define BOUNDS_H(DIR, xoff)                                      \
  me_bound_left[DIR] = 2*xoff - mvmax[DIR*2+0];                  \
  if (me_bound_left[DIR] < 0)                                    \
  {                                                              \
    me_bound_left[DIR] = 0;                                      \
  }                                                              \
  me_bound_right[DIR] = 2*xoff + mvmax[DIR*2+0] - 1;             \
  if (me_bound_right[DIR] > MBcountH*32 - 32) {                  \
    me_bound_right[DIR] = IPP_MAX(2*xoff, MBcountH*32 - 32);     \
  }                                                              \
  me_bound_left[DIR] -= 2*xoff;                                  \
  me_bound_right[DIR] -= 2*xoff;

#define BOUNDS_V(DIR, yoff)                                      \
  me_bound_top[DIR] = 2*yoff - mvmax[DIR*2+1];                   \
  if( me_bound_top[DIR] < 0 )                                    \
  {                                                              \
    me_bound_top[DIR] = 0;                                       \
  }                                                              \
  me_bound_bottom[DIR] = 2*yoff + mvmax[DIR*2+1] - 1;            \
  if (me_bound_bottom[DIR] > 2*16*MBcountV - 32) {               \
    me_bound_bottom[DIR] = IPP_MAX(2*yoff, 2*16*MBcountV - 32);  \
  }                                                              \
  me_bound_top[DIR] -= 2*yoff;                                   \
  me_bound_bottom[DIR] -= 2*yoff;

// internal border for 16x8 prediction
#define BOUNDS_V_FIELD(DIR, yoff)                                \
  me_bound_1_bottom[DIR] = 2*yoff + mvmax[DIR*2+1] - 1;          \
  if (me_bound_1_bottom[DIR] > 2*16*MBcountV - 2*8) {            \
   me_bound_1_bottom[DIR] = IPP_MAX(2*yoff, 2*16*MBcountV) - 2*8;\
  }                                                              \
  me_bound_2_top[DIR] = 2*yoff + 2*8 - mvmax[DIR*2+1];           \
  if( me_bound_2_top[DIR] < 0 )                                  \
  {                                                              \
    me_bound_2_top[DIR] = 0;                                     \
  }                                                              \
  me_bound_1_bottom[DIR] -= 2*yoff;                              \
  me_bound_2_top[DIR] -= 2*yoff + 2*8;

#define VARMEAN_FRAME(pDiff, vardiff, meandiff, _vardiff)             \
  ippiVarSum8x8_16s32s_C1R(pDiff    , 32, &vardiff[0], &meandiff[0]); \
  ippiVarSum8x8_16s32s_C1R(pDiff+8  , 32, &vardiff[1], &meandiff[1]); \
  ippiVarSum8x8_16s32s_C1R(pDiff+128, 32, &vardiff[2], &meandiff[2]); \
  ippiVarSum8x8_16s32s_C1R(pDiff+136, 32, &vardiff[3], &meandiff[3]); \
  _vardiff = vardiff[0] + vardiff[1] + vardiff[2] + vardiff[3]

#define VARMEAN_FIELD(pDiff, vardiff, meandiff, _vardiff)             \
  ippiVarSum8x8_16s32s_C1R(pDiff   ,  64, &vardiff[0], &meandiff[0]); \
  ippiVarSum8x8_16s32s_C1R(pDiff+8 ,  64, &vardiff[1], &meandiff[1]); \
  ippiVarSum8x8_16s32s_C1R(pDiff+16,  64, &vardiff[2], &meandiff[2]); \
  ippiVarSum8x8_16s32s_C1R(pDiff+24,  64, &vardiff[3], &meandiff[3]); \
  _vardiff = vardiff[0] + vardiff[1] + vardiff[2] + vardiff[3]

#define VARMEAN_FRAME_Y(vardiff, meandiff, _vardiff, step)                       \
  ippiVarSum8x8_8u32s_C1R(YBlock             , step, &vardiff[0], &meandiff[0]); \
  ippiVarSum8x8_8u32s_C1R(YBlock + 8         , step, &vardiff[1], &meandiff[1]); \
  ippiVarSum8x8_8u32s_C1R(YBlock + 8*step    , step, &vardiff[2], &meandiff[2]); \
  ippiVarSum8x8_8u32s_C1R(YBlock + 8*step + 8, step, &vardiff[3], &meandiff[3]); \
  _vardiff = vardiff[0] + vardiff[1] + vardiff[2] + vardiff[3]

#define VARMEAN_FIELD_Y(vardiff, meandiff, _vardiff, step)                       \
  ippiVarSum8x8_8u32s_C1R(YBlock,            2*step, &vardiff[0], &meandiff[0]); \
  ippiVarSum8x8_8u32s_C1R(YBlock + 8,        2*step, &vardiff[1], &meandiff[1]); \
  ippiVarSum8x8_8u32s_C1R(YBlock + step,     2*step, &vardiff[2], &meandiff[2]); \
  ippiVarSum8x8_8u32s_C1R(YBlock + step + 8, 2*step, &vardiff[3], &meandiff[3]); \
  _vardiff = vardiff[0] + vardiff[1] + vardiff[2] + vardiff[3]

// scale parameters for frame/field or intra/predicted selection
// use because of counting bits for vectors encoding

#define SC_VAR_INTRA 16
#define SC_VAR_1V    16
#define SC_VAR_2V    17
#define SC_VAR_1VBI  16
#define SC_VAR_2VBI  17

#define SCALE_VAR(val, scale) ( (val)*(scale) )
#define SCALE_VAR_INTRA(val)         \
  ( i==0 ? (val+1)*SC_VAR_INTRA      \
  : (pMBInfo[k-1].mb_type==MB_INTRA  \
  ? val*SC_VAR_INTRA                 \
  :(val+2)*SC_VAR_INTRA)             \
  )

#define SWAP_PTR(ptr0, ptr1) { \
  MB_prediction_info *tmp_ptr = ptr0; \
  ptr0 = ptr1; \
  ptr1 = tmp_ptr; \
}

#define IF_GOOD_PRED(vardiff, meandiff)       \
  if(bestpred->mb_type != MB_INTRA &&         \
     vardiff[0] <= varThreshold &&            \
     vardiff[1] <= varThreshold &&            \
     vardiff[2] <= varThreshold &&            \
     vardiff[3] <= varThreshold)              \
    if(meandiff[0] < meanThreshold/8 &&       \
       meandiff[0] > -meanThreshold/8)        \
      if(meandiff[1] < meanThreshold/8 &&     \
         meandiff[1] > -meanThreshold/8)      \
        if(meandiff[2] < meanThreshold/8 &&   \
           meandiff[2] > -meanThreshold/8)    \
          if(meandiff[3] < meanThreshold/8 && \
             meandiff[3] > -meanThreshold/8)

#define ME_FRAME(DIR, vardiff_res, pDiff, dct_type)            \
{                                                              \
  Ipp32s vardiff_tmp[4], meandiff_tmp[4], _vardiff;            \
  Ipp32s curr_field = m_cuc->FrameParam->MPEG2.SecondFieldFlag;\
  mfxStatus sts;                                               \
                                                               \
  pRef[0] = state->YRecFrame[curr_field][DIR] + cur_offset;    \
  pRef[1] = state->YRecFrame[1-curr_field][DIR] + cur_offset;  \
  pRec[0] = state->YRecFrame[curr_field][DIR] + cur_offset;    \
  pRec[1] = state->YRecFrame[1-curr_field][DIR] + cur_offset;  \
  dct_type = DCT_FRAME;                                        \
                                                               \
  sts = MotionEstimation_Frame( pRef[state->ipflag],           \
    pRec[state->ipflag],                                       \
    state->YFrameHSize,                                        \
    YBlock,                                                    \
    state->YFrameHSize,                                        \
    mean_frm,                                                  \
    currpred->var,                                             \
    currpred->mean,                                            \
    me_bound_left[DIR],                                        \
    me_bound_right[DIR],                                       \
    me_bound_top[DIR],                                         \
    me_bound_bottom[DIR],                                      \
    PMV[0][DIR],                                               \
    pMBInfo[k].MV[0][DIR],                                     \
    (j != 0) ? pMBInfo[k - MBcountH].MV[0][DIR] : MV_ZERO,     \
    &vector[2][DIR],                                           \
    state,                                                     \
    i, j,                                                      \
    &pMBInfo[k].mv_field_sel[2][DIR],                          \
    NULL, state->ipflag, &vardiff_res);                        \
    MFX_CHECK_STS(sts);                                        \
  if(m_cuc->FrameParam->MPEG2.FieldPicFlag && !state->ipflag){ \
    sts = MotionEstimation_Frame( pRef[1],                     \
      pRec[1],                                                 \
      state->YFrameHSize,                                      \
      YBlock,                                                  \
      state->YFrameHSize,                                      \
      mean_frm,                                                \
      currpred->var,                                           \
      currpred->mean,                                          \
      me_bound_left[DIR],                                      \
      me_bound_right[DIR],                                     \
      me_bound_top[DIR],                                       \
      me_bound_bottom[DIR],                                    \
      PMV[1][DIR],                                             \
      pMBInfo[k].MV[1][DIR],                                   \
      (j != 0) ? pMBInfo[k - MBcountH].MV[1][DIR] : MV_ZERO,   \
      &vector[2][DIR],                                         \
      state,                                                   \
      i, j,                                                    \
      &pMBInfo[k].mv_field_sel[2][DIR],                        \
      &vardiff_res, 1, NULL);                                  \
    MFX_CHECK_STS(sts);                                        \
  }                                                            \
  else if (!state->curr_frame_dct) {                           \
    GETDIFF_FRAME(Y, Y, l, pDiff, DIR);                        \
                                                               \
    VARMEAN_FIELD(pDiff, vardiff_tmp, meandiff_tmp, _vardiff); \
    {                                                          \
      Ipp32s var_fld = 0, var = 0;                             \
      ippiFrameFieldSAD16x16_16s32s_C1R(pDiff, BlkStride_l*2, &var, &var_fld);  \
      if (var_fld < var) {                                     \
        dct_type = DCT_FIELD;                                  \
        vardiff_res = _vardiff;                                \
        ippsCopy_8u((Ipp8u*)vardiff_tmp, (Ipp8u*)currpred->var, sizeof(currpred->var)); \
        ippsCopy_8u((Ipp8u*)meandiff_tmp, (Ipp8u*)currpred->mean, sizeof(currpred->mean)); \
      }                                                        \
    }                                                          \
  }                                                            \
}

#define ME_FIELD(DIR, vardiff_fld, pDiff, dct_type)            \
{                                                              \
  Ipp32s vardiff_tmp[4], meandiff_tmp[4], _vardiff;            \
  Ipp32s diff_f[2][2]; /* [srcfld][reffld] */                  \
  mfxStatus sts;                                               \
                                                               \
  if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {                \
    sts = MotionEstimation_Field( pRef[0],                     \
      pRec[0],                                                 \
      2*state->YFrameHSize,                                    \
      YBlock,                                                  \
      2*state->YFrameHSize,                                    \
      mean_fld,                                                \
      currpred->var,                                           \
      currpred->mean,                                          \
      me_bound_left[DIR],                                      \
      me_bound_right[DIR],                                     \
      (me_bound_top[DIR] + 0) >> 1,                            \
      me_bound_bottom[DIR] >> 1,                               \
      PMV[0][DIR],                                             \
      pMBInfo[k].MV[0][DIR],                                   \
      (j != 0) ? pMBInfo[k - MBcountH].MV[0][DIR] : MV_ZERO,   \
      &vector[0][DIR],                                         \
      state,                                                   \
      i, j,                                                    \
      &pMBInfo[k].mv_field_sel[0][DIR],                        \
      NULL, 0, &diff_f[0][0]);                                 \
    MFX_CHECK_STS(sts);                                        \
    sts = MotionEstimation_Field( pRef[1],                     \
      pRec[1],                                                 \
      2*state->YFrameHSize,                                    \
      YBlock,                                                  \
      2*state->YFrameHSize,                                    \
      mean_fld,                                                \
      currpred->var,                                           \
      currpred->mean,                                          \
      me_bound_left[DIR],                                      \
      me_bound_right[DIR],                                     \
      (me_bound_top[DIR] + 0) >> 1,                            \
      me_bound_bottom[DIR] >> 1,                               \
      PMV[0][DIR],                                             \
      pMBInfo[k].MV[0][DIR],                                   \
      (j != 0) ? pMBInfo[k - MBcountH].MV[0][DIR] : MV_ZERO,   \
      &vector[0][DIR],                                         \
      state,                                                   \
      i, j,                                                    \
      &pMBInfo[k].mv_field_sel[0][DIR],                        \
      &diff_f[0][0], 1, &diff_f[0][1]);                        \
    MFX_CHECK_STS(sts);                                        \
                                                               \
    sts = MotionEstimation_Field( pRef[0],                     \
      pRec[0],                                                 \
      2*state->YFrameHSize,                                    \
      YBlock + state->YFrameHSize,                             \
      2*state->YFrameHSize,                                    \
      mean_fld + 2,                                            \
      currpred->var + 2,                                       \
      currpred->mean + 2,                                      \
      me_bound_left[DIR],                                      \
      me_bound_right[DIR],                                     \
      (me_bound_top[DIR] + 0) >> 1,                            \
      me_bound_bottom[DIR] >> 1,                               \
      PMV[1][DIR],                                             \
      pMBInfo[k].MV[1][DIR],                                   \
      (j != 0) ? pMBInfo[k - MBcountH].MV[1][DIR]  : MV_ZERO,  \
      &vector[1][DIR],                                         \
      state,                                                   \
      i, j,                                                    \
      &pMBInfo[k].mv_field_sel[1][DIR],                        \
      NULL, 0, &diff_f[1][0]);                                 \
    MFX_CHECK_STS(sts);                                        \
    sts = MotionEstimation_Field( pRef[1],                     \
      pRec[1],                                                 \
      2*state->YFrameHSize,                                    \
      YBlock + state->YFrameHSize,                             \
      2*state->YFrameHSize,                                    \
      mean_fld + 2,                                            \
      currpred->var + 2,                                       \
      currpred->mean + 2,                                      \
      me_bound_left[DIR],                                      \
      me_bound_right[DIR],                                     \
      (me_bound_top[DIR] + 0) >> 1,                            \
      me_bound_bottom[DIR] >> 1,                               \
      PMV[1][DIR],                                             \
      pMBInfo[k].MV[1][DIR],                                   \
      (j != 0) ? pMBInfo[k - MBcountH].MV[1][DIR] : MV_ZERO,   \
      &vector[1][DIR],                                         \
      state,                                                   \
      i, j,                                                    \
      &pMBInfo[k].mv_field_sel[1][DIR],                        \
      &diff_f[1][0], 1, &diff_f[1][1]);                        \
    MFX_CHECK_STS(sts);                                        \
    vardiff_fld = diff_f[0][0] + diff_f[1][0];                 \
    dct_type = DCT_FIELD;                                      \
                                                               \
  } else {                                                     \
    sts = MotionEstimation_FieldPict(pRef[state->ipflag],      \
      pRec[state->ipflag],                                     \
      state->YFrameHSize,                                      \
      YBlock,                                                  \
      state->YFrameHSize,                                      \
      mean_frm,                                                \
      currpred->var,                                           \
      currpred->mean,                                          \
      me_bound_left[DIR],                                      \
      me_bound_right[DIR],                                     \
      me_bound_top[DIR],                                       \
      me_bound_1_bottom[DIR],                                  \
      PMV[0][DIR],                                             \
      pMBInfo[k].MV[0][DIR],                                   \
      (j != 0) ? pMBInfo[k - MBcountH].MV[0][DIR] : MV_ZERO,   \
      &(vector[0][DIR]),                                       \
      state,                                                   \
      i, j,                                                    \
      &pMBInfo[k].mv_field_sel[0][DIR],                        \
      NULL, state->ipflag, &diff_f[0][0]);                     \
    MFX_CHECK_STS(sts);                                        \
    if(!state->ipflag)                                         \
     sts = MotionEstimation_FieldPict(pRef[1],                 \
      pRec[1],                                                 \
      state->YFrameHSize,                                      \
      YBlock,                                                  \
      state->YFrameHSize,                                      \
      mean_frm,                                                \
      currpred->var,                                           \
      currpred->mean,                                          \
      me_bound_left[DIR],                                      \
      me_bound_right[DIR],                                     \
      me_bound_top[DIR],                                       \
      me_bound_1_bottom[DIR],                                  \
      PMV[0][DIR],                                             \
      pMBInfo[k].MV[0][DIR],                                   \
      (j != 0) ? pMBInfo[k - MBcountH].MV[0][DIR] : MV_ZERO,   \
      &(vector[0][DIR]),                                       \
      state,                                                   \
      i, j,                                                    \
      &pMBInfo[k].mv_field_sel[0][DIR],                        \
      &diff_f[0][0], 1, &diff_f[0][1]);                        \
    MFX_CHECK_STS(sts);                                        \
    sts = MotionEstimation_FieldPict(pRef[state->ipflag] + 8*state->YFrameHSize, \
      pRec[state->ipflag] + 8*YFrameHSize,                     \
      state->YFrameHSize,                                      \
      YBlock + 8*state->YFrameHSize,                           \
      state->YFrameHSize,                                      \
      mean_frm + 2,                                            \
      currpred->var + 2,                                       \
      currpred->mean + 2,                                      \
      me_bound_left[DIR],                                      \
      me_bound_right[DIR],                                     \
      me_bound_2_top[DIR],                                     \
      me_bound_bottom[DIR],                                    \
      PMV[1][DIR],                                             \
      pMBInfo[k].MV[1][DIR],                                   \
      (j != 0) ? pMBInfo[k - MBcountH].MV[1][DIR] : MV_ZERO,   \
      &(vector[1][DIR]),                                       \
      state,                                                   \
      i, j,                                                    \
      &pMBInfo[k].mv_field_sel[1][DIR],                        \
      NULL, state->ipflag, &diff_f[1][0]);                     \
    MFX_CHECK_STS(sts);                                        \
    if(!state->ipflag)                                         \
     sts = MotionEstimation_FieldPict(pRef[1] + 8*state->YFrameHSize, \
      pRec[1] + 8*state->YFrameHSize,                          \
      state->YFrameHSize,                                      \
      YBlock + 8*state->YFrameHSize,                           \
      YFrameHSize,                                             \
      mean_frm + 2,                                            \
      currpred->var + 2,                                       \
      currpred->mean + 2,                                      \
      me_bound_left[DIR],                                      \
      me_bound_right[DIR],                                     \
      me_bound_2_top[DIR],                                     \
      me_bound_bottom[DIR],                                    \
      PMV[1][DIR],                                             \
      pMBInfo[k].MV[1][DIR],                                   \
      (j != 0) ? pMBInfo[k - MBcountH].MV[1][DIR] : MV_ZERO,   \
      &(vector[1][DIR]),                                       \
      state,                                                   \
      i, j,                                                    \
      &pMBInfo[k].mv_field_sel[1][DIR],                        \
      &diff_f[1][0], 1, &diff_f[1][1]);                        \
    MFX_CHECK_STS(sts);                                        \
    vardiff_fld = diff_f[0][0] + diff_f[1][0];                 \
    dct_type = DCT_FRAME;                                      \
                                                               \
  }                                                            \
  GETDIFF_FIELD(Y, Y, l, pDiff, DIR);                          \
                                                               \
  if( !m_cuc->FrameParam->MPEG2.FieldPicFlag ) {               \
    VARMEAN_FRAME(pDiff, vardiff_tmp, meandiff_tmp, _vardiff); \
    {                                                          \
      Ipp32s var_fld = 0, var = 0;                             \
      ippiFrameFieldSAD16x16_16s32s_C1R(pDiff, BlkStride_l*2, &var, &var_fld); \
      if (var < var_fld) {                                     \
        dct_type = DCT_FRAME;                                  \
        vardiff_fld = _vardiff;                                \
        ippsCopy_8u((Ipp8u*)vardiff_tmp, (Ipp8u*)currpred->var, sizeof(currpred->var)); \
        ippsCopy_8u((Ipp8u*)meandiff_tmp, (Ipp8u*)currpred->mean, sizeof(currpred->mean)); \
      }                                                        \
    }                                                          \
    /*if(_vardiff <= vardiff_fld) {                            \
      dct_type = DCT_FRAME;                                    \
      vardiff_fld = _vardiff;                                  \
    }*/                                                        \
  }                                                            \
}

/*********************************************************/

#endif // MFX_ENABLE_MPEG2_VIDEO_ENC
#endif // _MFX_MPEG2_ENC_DEFS_H_
