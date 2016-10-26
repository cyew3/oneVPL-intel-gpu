//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2011 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_MPEG2_PAK_TABS_H_
#define _MFX_MPEG2_PAK_TABS_H_

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_PAK)

#include "mfx_mpeg2_pak.h"
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

/* picture_structure ISO/IEC 13818-2, 6.3.10 table 6-14 */
enum
{
  MPEG2_TOP_FIELD     = 1,
  MPEG2_BOTTOM_FIELD  = 2,
  MPEG2_FRAME_PICTURE = 3
};

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

typedef struct _IppMotionVector2
{
  Ipp32s x;
  Ipp32s y;
  Ipp32s mctype_l;
  Ipp32s offset_l;
  Ipp32s mctype_c;
  Ipp32s offset_c;
} IppMotionVector2;

#define SET_MOTION_VECTOR(vectorF, mv_x, mv_y) {                              \
  Ipp32s x_l = mv_x;                                                          \
  Ipp32s y_l = mv_y;                                                          \
  Ipp32s i_c = (BlkWidth_c  == 16) ? i : (i >> 1);                            \
  Ipp32s j_c = (BlkHeight_c == 16) ? j : (j >> 1);                            \
  Ipp32s x_c = (BlkWidth_c  == 16) ? x_l : (x_l/2);                           \
  Ipp32s y_c = (BlkHeight_c == 16) ? y_l : (y_l/2);                           \
  vectorF->x = x_l;                                                           \
  vectorF->y = y_l;                                                           \
  vectorF->mctype_l = ((x_l & 1) << 3) | ((y_l & 1) << 2);                    \
  vectorF->mctype_c = ((x_c & 1) << 3) | ((y_c & 1) << 2);                    \
  vectorF->offset_l = (i   + (x_l >> 1)) + (j   + (y_l >> 1)) * state->YRefFrameHSize;  \
  vectorF->offset_c = (i_c + (x_c >> 1)) + (j_c + (y_c >> 1)) * state->UVRefFrameHSize; \
  if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) vectorF->offset_c *= 2;     \
}

// doubt about field pictures
#define SET_FIELD_VECTOR(vectorF, mv_x, mv_y) {                               \
  Ipp32s x_l = mv_x;                                                          \
  Ipp32s y_l = mv_y;                                                          \
  Ipp32s i_c = (BlkWidth_c  == 16) ? i : (i >> 1);                            \
  Ipp32s j_c = (BlkHeight_c == 16) ? j : (j >> 1);                            \
  Ipp32s x_c = (BlkWidth_c  == 16) ? x_l : (x_l/2);                           \
  Ipp32s y_c = (BlkHeight_c == 16) ? y_l : (y_l/2);                           \
  vectorF->x = x_l;                                                           \
  vectorF->y = y_l;                                                           \
  vectorF->mctype_l = ((x_l & 1) << 3) | ((y_l & 1) << 2);                    \
  vectorF->mctype_c = ((x_c & 1) << 3) | ((y_c & 1) << 2);                    \
  vectorF->offset_l = (i   + (x_l >> 1)) + (j   + (y_l &~ 1) ) * state->YRefFrameHSize; \
  vectorF->offset_c = (i_c + (x_c >> 1)) + (j_c + (y_c &~ 1) ) * state->UVRefFrameHSize;\
  if (m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) vectorF->offset_c *= 2;     \
}

#define GETDIFF_FRAME(X, CC, C, pDiff, DIR) \
  func_getdiff_frame_##C(                   \
    X##SrcBlock,                            \
    state->CC##SrcFrameHSize,               \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[2][DIR]][DIR] + vector[2][DIR].offset_##C, \
    state->CC##RefFrameHSize,               \
    pDiff + OFF_##X,                        \
    2*BlkStride_##C,                        \
    0, 0, vector[2][DIR].mctype_##C, 0)

#define GETDIFF_FRAME_NV12(X, XX, CC, C, pDiff, DIR) \
  func_getdiff_frame_nv12_##C(              \
    X##SrcBlock,                               \
    state->CC##SrcFrameHSize,                      \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[2][DIR]][DIR] + vector[2][DIR].offset_##C, \
    state->CC##RefFrameHSize,                      \
    pDiff + OFF_##X,                        \
    2*BlkStride_##C,                        \
    pDiff + OFF_##XX,                       \
    2*BlkStride_##C,                        \
    vector[2][DIR].mctype_##C, 0)
#define GETDIFF_FRAME_FB(X, CC, C, pDiff)   \
  func_getdiffB_frame_##C(X##SrcBlock,      \
    state->CC##SrcFrameHSize,               \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[2][0]][0] + vector[2][0].offset_##C, \
    state->CC##RefFrameHSize,               \
    vector[2][0].mctype_##C,                \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[2][1]][1] + vector[2][1].offset_##C, \
    state->CC##RefFrameHSize,               \
    vector[2][1].mctype_##C,                \
    pDiff + OFF_##X,                        \
    2*BlkStride_##C,                        \
    ippRndZero)

#define GETDIFF_FRAME_FB_NV12(X, XX, CC, C, pDiff)   \
  func_getdiffB_frame_nv12_##C(X##SrcBlock,         \
    state->CC##SrcFrameHSize,                       \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[2][0]][0] + vector[2][0].offset_##C, \
    state->CC##RefFrameHSize,                      \
    vector[2][0].mctype_##C,                \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[2][1]][1] + vector[2][1].offset_##C, \
    state->CC##RefFrameHSize,                      \
    vector[2][1].mctype_##C,                \
    pDiff + OFF_##X,                        \
    2*BlkStride_##C,                        \
    pDiff + OFF_##XX,                        \
    2*BlkStride_##C,                        \
    ippRndZero)
#define GETDIFF_FIELD(X, CC, C, pDiff, DIR)            \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {          \
  func_getdiff_field_##C(X##SrcBlock,                  \
    2*state->CC##SrcFrameHSize,                        \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][DIR]][DIR] +vector[0][DIR].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    pDiff + OFF_##X,                                   \
    4*BlkStride_##C,                                   \
    0, 0, vector[0][DIR].mctype_##C, 0);               \
                                                       \
  func_getdiff_field_##C(X##SrcBlock + state->CC##SrcFrameHSize,\
    2*state->CC##SrcFrameHSize,                        \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][DIR]][DIR] + vector[1][DIR].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    pDiff + OFF_##X + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    0, 0, vector[1][DIR].mctype_##C, 0);               \
} else {                                               \
  func_getdiff_field_##C(X##SrcBlock,                  \
    state->CC##SrcFrameHSize,                                 \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][DIR]][DIR] + vector[0][DIR].offset_##C, \
    state->CC##RefFrameHSize,                                 \
    pDiff + OFF_##X,                                   \
    2*BlkStride_##C,                                   \
    0, 0, vector[0][DIR].mctype_##C, 0);               \
                                                       \
  func_getdiff_field_##C(X##SrcBlock + (BlkHeight_##C/2)*state->CC##SrcFrameHSize, \
    state->CC##SrcFrameHSize,                                    \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][DIR]][DIR] + vector[1][DIR].offset_##C + (BlkHeight_##C/2)*state->CC##RefFrameHSize, \
    state->CC##RefFrameHSize,                                    \
    pDiff + OFF_##X + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                                   \
    0, 0, vector[1][DIR].mctype_##C, 0);               \
}

#define GETDIFF_FIELD_NV12(X, XX, CC, C, pDiff, DIR)            \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {              \
  func_getdiff_field_nv12_##C(X##SrcBlock,                     \
    2*state->CC##SrcFrameHSize,                                  \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][DIR]][DIR] +vector[0][DIR].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    pDiff + OFF_##X,                                   \
    4*BlkStride_##C,                                   \
    pDiff + OFF_##XX,                                   \
    4*BlkStride_##C,                                   \
    vector[0][DIR].mctype_##C, 0);               \
                                                       \
  func_getdiff_field_nv12_##C(X##SrcBlock + state->CC##SrcFrameHSize,    \
    2*state->CC##SrcFrameHSize,                                  \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][DIR]][DIR] + vector[1][DIR].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    pDiff + OFF_##X + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    pDiff + OFF_##XX + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    vector[1][DIR].mctype_##C, 0);               \
} else {                                               \
  func_getdiff_field_nv12_##C(X##SrcBlock,                     \
    state->CC##SrcFrameHSize,                                    \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][DIR]][DIR] + vector[0][DIR].offset_##C, \
    state->CC##RefFrameHSize,                                    \
    pDiff + OFF_##X,                                   \
    2*BlkStride_##C,                                   \
    pDiff + OFF_##XX,                                   \
    2*BlkStride_##C,                                   \
    vector[0][DIR].mctype_##C, 0);               \
                                                       \
  func_getdiff_field_nv12_##C(X##SrcBlock + (BlkHeight_##C/2)*state->CC##SrcFrameHSize, \
    state->CC##SrcFrameHSize,                                    \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][DIR]][DIR] + vector[1][DIR].offset_##C + (BlkHeight_##C/2)*state->CC##RefFrameHSize, \
    state->CC##RefFrameHSize,                                    \
    pDiff + OFF_##X + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                                   \
    pDiff + OFF_##XX + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                                   \
    vector[1][DIR].mctype_##C, 0);               \
}

#ifdef MPEG2_USE_DUAL_PRIME

#define GETDIFF_FIELD_DP(X, CC, C, pDiff)              \
if (picture_structure == MPEG2_FRAME_PICTURE) {              \
  func_getdiffB_field_##C(X##SrcBlock,                    \
    2*state->CC##SrcFrameHSize,                                  \
    state->X##RefFrame[0][0] + vector[0][0].offset_##C,       \
    2*state->CC##RefFrameHSize,                                  \
    vector[0][0].mctype_##C,                           \
    state->X##RefFrame[1][0] + vector[1][0].offset_##C,       \
    2*state->CC##RefFrameHSize,                                  \
    vector[1][0].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    4*BlkStride_##C,                                   \
    ippRndZero);                                       \
                                                       \
  func_getdiffB_field_##C(X##SrcBlock + state->CC##SrcFrameHSize,   \
    2*state->CC##SrcFrameHSize,                                  \
    state->X##RefFrame[1][0] + vector[0][0].offset_##C,       \
    2*state->CC##RefFrameHSize,                                  \
    vector[0][0].mctype_##C,                           \
    state->X##RefFrame[0][0] + vector[2][0].offset_##C,       \
    2*state->CC##RefFrameHSize,                                  \
    vector[2][0].mctype_##C,                           \
    pDiff + OFF_##X + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    ippRndZero);                                       \
} else {                                               \
  func_getdiffB_frame_##C(X##SrcBlock,                    \
    state->CC##SrcFrameHSize,                                    \
    state->X##RefFrame[curr_field][0] + vector[0][0].offset_##C,       \
    state->CC##RefFrameHSize,                                    \
    vector[0][0].mctype_##C,                           \
    state->X##RefFrame[1-curr_field][0] + vector[1][0].offset_##C,       \
    state->CC##RefFrameHSize,                                    \
    vector[1][0].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    2*BlkStride_##C,                                   \
    ippRndZero);                                       \
}

#define GETDIFF_FIELD_DP_NV12(X, XX, CC, C, pDiff)              \
if (picture_structure == MPEG2_FRAME_PICTURE) {              \
  func_getdiffB_field_nv12_##C(X##SrcBlock,                    \
    2*state->CC##SrcFrameHSize,                                  \
    state->X##RefFrame[0][0] + vector[0][0].offset_##C,       \
    2*state->CC##RefFrameHSize,                                  \
    vector[0][0].mctype_##C,                           \
    state->X##RefFrame[1][0] + vector[1][0].offset_##C,       \
    2*state->CC##RefFrameHSize,                                  \
    vector[1][0].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    4*BlkStride_##C,                                   \
    pDiff + OFF_##XX,                                   \
    4*BlkStride_##C,                                   \
    ippRndZero);                                       \
                                                       \
  func_getdiffB_field_nv12_##C(X##SrcBlock + state->CC##SrcFrameHSize,   \
    2*state->CC##SrcFrameHSize,                                  \
    state->X##RefFrame[1][0] + vector[0][0].offset_##C,       \
    2*state->CC##RefFrameHSize,                                \
    vector[0][0].mctype_##C,                           \
    state->X##RefFrame[0][0] + vector[2][0].offset_##C,       \
    2*state->CC##RefFrameHSize,                                  \
    vector[2][0].mctype_##C,                           \
    pDiff + OFF_##X + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    pDiff + OFF_##XX + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    ippRndZero);                                       \
} else {                                               \
  func_getdiffB_frame_nv12_##C(X##SrcBlock,                    \
    state->CC##SrcFrameHSize,                                    \
    state->X##RefFrame[curr_field][0] + vector[0][0].offset_##C,       \
    state->CC##RefFrameHSize,                                    \
    vector[0][0].mctype_##C,                           \
    state->X##RefFrame[1-curr_field][0] + vector[1][0].offset_##C,       \
    state->CC##RefFrameHSize,                                    \
    vector[1][0].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    2*BlkStride_##C,                                   \
    pDiff + OFF_##XX,                                   \
    2*BlkStride_##C,                                   \
    ippRndZero);                                       \
}
#endif //MPEG2_USE_DUAL_PRIME
#define GETDIFF_FIELD_FB(X, CC, C, pDiff)              \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {          \
  func_getdiffB_field_##C(X##SrcBlock,                    \
    2*state->CC##SrcFrameHSize,                                  \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    vector[0][0].mctype_##C,                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][1]][1] + vector[0][1].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    vector[0][1].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    4*BlkStride_##C,                                   \
    ippRndZero);                                       \
                                                       \
  func_getdiffB_field_##C(X##SrcBlock + state->CC##SrcFrameHSize,   \
    2*state->CC##SrcFrameHSize,                                  \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    vector[1][0].mctype_##C,                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][1]][1] + vector[1][1].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    vector[1][1].mctype_##C,                           \
    pDiff + OFF_##X + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    ippRndZero);                                       \
} else {                                               \
  func_getdiffB_field_##C(X##SrcBlock,                    \
    state->CC##SrcFrameHSize,                                    \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C,\
    state->CC##RefFrameHSize,                                    \
    vector[0][0].mctype_##C,                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][1]][1] + vector[0][1].offset_##C,\
    state->CC##RefFrameHSize,                                    \
    vector[0][1].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    2*BlkStride_##C,                                   \
    ippRndZero);                                       \
                                                       \
  func_getdiffB_field_##C(                             \
    X##SrcBlock + (BlkHeight_##C/2)*state->CC##SrcFrameHSize,       \
    state->CC##SrcFrameHSize,                                    \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C + (BlkHeight_##C/2)*state->CC##RefFrameHSize,  \
    state->CC##RefFrameHSize,                                    \
    vector[1][0].mctype_##C,                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][1]][1] + vector[1][1].offset_##C + (BlkHeight_##C/2)*state->CC##RefFrameHSize, \
    state->CC##RefFrameHSize,                                    \
    vector[1][1].mctype_##C,                           \
    pDiff + OFF_##X + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                                   \
    ippRndZero);                                       \
}

#define GETDIFF_FIELD_FB_NV12(X, XX, CC, C, pDiff)              \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {              \
  func_getdiffB_field_nv12_##C(X##SrcBlock,                    \
    2*state->CC##SrcFrameHSize,                                  \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    vector[0][0].mctype_##C,                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][1]][1] + vector[0][1].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    vector[0][1].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    4*BlkStride_##C,                                   \
    pDiff + OFF_##XX,                                   \
    4*BlkStride_##C,                                   \
    ippRndZero);                                       \
                                                       \
  func_getdiffB_field_nv12_##C(X##SrcBlock + state->CC##SrcFrameHSize,   \
    2*state->CC##SrcFrameHSize,                                  \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    vector[1][0].mctype_##C,                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][1]][1] + vector[1][1].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    vector[1][1].mctype_##C,                           \
    pDiff + OFF_##X + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    pDiff + OFF_##XX + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    ippRndZero);                                       \
} else {                                               \
  func_getdiffB_field_nv12_##C(X##SrcBlock,                    \
    state->CC##SrcFrameHSize,                                    \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C,\
    state->CC##RefFrameHSize,                                    \
    vector[0][0].mctype_##C,                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][1]][1] + vector[0][1].offset_##C,\
    state->CC##RefFrameHSize,                                    \
    vector[0][1].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    2*BlkStride_##C,                                   \
    pDiff + OFF_##XX,                                   \
    2*BlkStride_##C,                                   \
    ippRndZero);                                       \
                                                       \
  func_getdiffB_field_nv12_##C(                             \
    X##SrcBlock + (BlkHeight_##C/2)*state->CC##SrcFrameHSize,       \
    state->CC##SrcFrameHSize,                                    \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C + (BlkHeight_##C/2)*state->CC##RefFrameHSize,  \
    state->CC##RefFrameHSize,                                    \
    vector[1][0].mctype_##C,                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][1]][1] + vector[1][1].offset_##C + (BlkHeight_##C/2)*state->CC##RefFrameHSize, \
    state->CC##RefFrameHSize,                                    \
    vector[1][1].mctype_##C,                           \
    pDiff + OFF_##X + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                                   \
    pDiff + OFF_##XX + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                                   \
    ippRndZero);                                       \
}
#define MC_FRAME_F(X, CC, C, pDiff) \
  func_mc_frame_##C(                \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[2][0]][0] + vector[2][0].offset_##C, \
    state->CC##RefFrameHSize,       \
    pDiff + OFF_##X,                \
    2*BlkStride_##C,                \
    X##OutBlock,                    \
    state->CC##OutFrameHSize,       \
    vector[2][0].mctype_##C,        \
    (IppRoundMode)0 )

#define MC_FRAME_F_NV12(X, XX, CC, C, pDiff) \
  func_mc_frame_nv12_##C(                \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[2][0]][0] + vector[2][0].offset_##C, \
    state->CC##RefFrameHSize,                 \
    pDiff + OFF_##X,                \
    pDiff + OFF_##XX,                \
    2*BlkStride_##C,                \
    X##OutBlock,                    \
    state->CC##OutFrameHSize,                 \
    vector[2][0].mctype_##C,        \
    (IppRoundMode)0 )
#define MC_FIELD_F(X, CC, C, pDiff)            \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {  \
  func_mc_field_##C(                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C, \
    2*state->CC##RefFrameHSize,                \
    pDiff + OFF_##X,                           \
    4*BlkStride_##C,                           \
    X##OutBlock,                               \
    2*state->CC##OutFrameHSize,                \
    vector[0][0].mctype_##C,                   \
    (IppRoundMode)0 );                         \
                                               \
  func_mc_field_##C(                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C, \
    2*state->CC##RefFrameHSize,                \
    pDiff + OFF_##X + BlkStride_##C,           \
    4*BlkStride_##C,                           \
    X##OutBlock + state->CC##OutFrameHSize,    \
    2*state->CC##OutFrameHSize,                \
    vector[1][0].mctype_##C,                   \
    (IppRoundMode)0 );                         \
} else {                                       \
  func_mc_field_##C(                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C, \
    state->CC##RefFrameHSize,                  \
    pDiff + OFF_##X,                           \
    2*BlkStride_##C,                           \
    X##OutBlock,                               \
    state->CC##OutFrameHSize,                  \
    vector[0][0].mctype_##C,                   \
    (IppRoundMode)0 );                         \
                                               \
  func_mc_field_##C(                           \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C + (BlkHeight_##C/2)*state->CC##RefFrameHSize, \
    state->CC##RefFrameHSize,                  \
    pDiff + OFF_##X + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                           \
    X##OutBlock + (BlkHeight_##C/2)*state->CC##OutFrameHSize, \
    state->CC##OutFrameHSize,                            \
    vector[1][0].mctype_##C,                   \
    (IppRoundMode)0 );                         \
}

#define MC_FIELD_F_NV12(X, XX,CC, C, pDiff)                    \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {              \
  func_mc_field_nv12_##C(                                   \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    pDiff + OFF_##X,                                   \
    pDiff + OFF_##XX,                                  \
    4*BlkStride_##C,                                   \
    X##OutBlock,                                       \
    2*state->CC##OutFrameHSize,                                  \
    vector[0][0].mctype_##C,                           \
    (IppRoundMode)0 );                                 \
                                                       \
  func_mc_field_nv12_##C(                                   \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C, \
    2*state->CC##RefFrameHSize,                                  \
    pDiff + OFF_##X + BlkStride_##C,                   \
    pDiff + OFF_##XX + BlkStride_##C,                  \
    4*BlkStride_##C,                                   \
    X##OutBlock + state->CC##OutFrameHSize,                      \
    2*state->CC##OutFrameHSize,                                  \
    vector[1][0].mctype_##C,                           \
    (IppRoundMode)0 );                                 \
} else {                                               \
  func_mc_field_nv12_##C(                                   \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[0][0]][0] + vector[0][0].offset_##C, \
    state->CC##RefFrameHSize,                                    \
    pDiff + OFF_##X,                                   \
    pDiff + OFF_##XX,                                  \
    2*BlkStride_##C,                                   \
    X##OutBlock,                                       \
    state->CC##OutFrameHSize,                                    \
    vector[0][0].mctype_##C,                           \
    (IppRoundMode)0 );                                 \
                                                       \
  func_mc_field_nv12_##C(                                   \
    state->X##RefFrame[mbinfo[curr].mv_field_sel[1][0]][0] + vector[1][0].offset_##C + (BlkHeight_##C/2)*state->CC##RefFrameHSize, \
    state->CC##RefFrameHSize,                                    \
    pDiff + OFF_##X + (BlkHeight_##C/2)*BlkStride_##C, \
    pDiff + OFF_##XX + (BlkHeight_##C/2)*BlkStride_##C, \
    2*BlkStride_##C,                                   \
    X##OutBlock + (BlkHeight_##C/2)*state->CC##OutFrameHSize,    \
    state->CC##OutFrameHSize,                                    \
    vector[1][0].mctype_##C,                           \
    (IppRoundMode)0 );                                 \
}

#ifdef MPEG2_USE_DUAL_PRIME
#define MC_FIELD_DP(X, CC, C, pDiff)                   \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {              \
  func_mcB_field_##C(                                  \
    X##RecFrame[0][0] + vector[0][0].offset_##C,       \
    2*CC##FramePitchRec,                                  \
    vector[0][0].mctype_##C,                           \
    X##RecFrame[1][0] + vector[1][0].offset_##C,       \
    2*CC##FramePitchRec,                                  \
    vector[1][0].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    4*BlkStride_##C,                                   \
    X##BlockRec,                                       \
    2*CC##FramePitchRec,                                  \
    (IppRoundMode)0 );                                 \
                                                       \
  func_mcB_field_##C(                                  \
    X##RecFrame[1][0] + vector[0][0].offset_##C,       \
    2*CC##FramePitchRec,                                  \
    vector[0][0].mctype_##C,                           \
    X##RecFrame[0][0] + vector[2][0].offset_##C,       \
    2*CC##FramePitchRec,                                  \
    vector[2][0].mctype_##C,                           \
    pDiff + OFF_##X + BlkStride_##C,                   \
    4*BlkStride_##C,                                   \
    X##BlockRec + CC##FramePitchRec,                      \
    2*CC##FramePitchRec,                                  \
    (IppRoundMode)0 );                                 \
}else{                                                 \
  func_mcB_frame_##C(                                  \
    X##RecFrame[curr_field][0] + vector[0][0].offset_##C,       \
    CC##FramePitchRec,                                    \
    vector[0][0].mctype_##C,                           \
    X##RecFrame[1-curr_field][0] + vector[1][0].offset_##C,       \
    CC##FramePitchRec,                                    \
    vector[1][0].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    2*BlkStride_##C,                                   \
    X##BlockRec,                                       \
    CC##FramePitchRec,                                    \
    (IppRoundMode)0 );                                 \
}

#define MC_FIELD_DP_NV12(X, XX, CC, C, pDiff)          \
if (!m_cuc->FrameParam->MPEG2.FieldPicFlag) {              \
  func_mcB_field_nv12_##C(                                  \
    X##RecFrame[0][0] + vector[0][0].offset_##C,       \
    2*CC##FramePitchRec,                                  \
    vector[0][0].mctype_##C,                           \
    X##RecFrame[1][0] + vector[1][0].offset_##C,       \
    2*CC##FramePitchRec,                                  \
    vector[1][0].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    pDiff + OFF_##XX,                                  \
    4*BlkStride_##C,                                   \
    X##BlockRec,                                       \
    2*CC##FramePitchRec,                                  \
    (IppRoundMode)0 );                                 \
                                                       \
  func_mcB_field_nv12_##C(                                  \
    X##RecFrame[1][0] + vector[0][0].offset_##C,       \
    2*CC##FramePitchRec,                                  \
    vector[0][0].mctype_##C,                           \
    X##RecFrame[0][0] + vector[2][0].offset_##C,       \
    2*CC##FramePitchRec,                                  \
    vector[2][0].mctype_##C,                           \
    pDiff + OFF_##X + BlkStride_##C,                   \
    pDiff + OFF_##XX + BlkStride_##C,                  \
    4*BlkStride_##C,                                   \
    X##BlockRec + CC##FramePitchRec,                      \
    2*CC##FramePitchRec,                                  \
    (IppRoundMode)0 );                                 \
}else{                                                 \
  func_mcB_frame_nv12_##C(                                  \
    X##RecFrame[curr_field][0] + vector[0][0].offset_##C,       \
    CC##FramePitchRec,                                    \
    vector[0][0].mctype_##C,                           \
    X##RecFrame[1-curr_field][0] + vector[1][0].offset_##C,       \
    CC##FramePitchRec,                                    \
    vector[1][0].mctype_##C,                           \
    pDiff + OFF_##X,                                   \
    pDiff + OFF_##XX,                                  \
    2*BlkStride_##C,                                   \
    X##BlockRec,                                       \
    CC##FramePitchRec,                                    \
    (IppRoundMode)0 );                                 \
}

#endif //MPEG2_USE_DUAL_PRIME
// Note: right and bottom borders are excluding
#define SET_BUFFER(bb, mfxBS)     \
  bb.start_pointer = (Ipp8u*)(mfxBS->Data + mfxBS->DataOffset + mfxBS->DataLength);   \
  *(Ipp32u*)bb.start_pointer = 0; \
  bb.bit_offset = 32;             \
  bb.bytelen = (Ipp32s)(mfxBS->MaxLength - mfxBS->DataOffset - mfxBS->DataLength);    \
  bb.current_pointer = (Ipp32u*)bb.start_pointer;

#ifdef _BIG_ENDIAN_
#define BSWAP(x) (x)
#else // Little endian
#define BSWAP(x) (Ipp32u)(((x) << 24) + (((x)&0xff00) << 8) + (((x) >> 8)&0xff00) + ((x) >> 24))
#endif

#define ippiPutBits(pBitStream,offset,val,len)      \
{                                                   \
  Ipp32s tmpcnt;                                    \
  Ipp32u r_tmp;                                     \
  /*CHECK_ippiPutBits(offset,val,len)*/                 \
  tmpcnt = (offset) - (len);                        \
  if(tmpcnt < 0)                                    \
  {                                                 \
    r_tmp = (pBitStream)[0] | ((val) >> (-tmpcnt)); \
    (pBitStream)[0] = BSWAP(r_tmp);                 \
    (pBitStream)++;                                 \
    (pBitStream)[0] = (val) << (32 + tmpcnt);       \
    (offset) = 32 + tmpcnt;                         \
  }                                                 \
  else                                              \
  {                                                 \
    (pBitStream)[0] |= (val) << tmpcnt;             \
    (offset) = tmpcnt;                              \
  }                                                 \
}
#define SAFE_MODE
class MPEG2_Exceptions
{
private:
    mfxStatus m_sts;
public:
    MPEG2_Exceptions(mfxStatus sts): m_sts (sts) {}    
    mfxStatus GetType()
    {
        return m_sts;
    }
};
#ifdef SAFE_MODE

#define CHECK_BUFFER_SIZE()                                                                                                             \
 if (((Ipp8u*)bb.current_pointer - (Ipp8u*)bb.start_pointer) > bb.bytelen - 8)                                                          \
 {                                                                                                                                      \
    throw MPEG2_Exceptions(MFX_ERR_NOT_ENOUGH_BUFFER);                                                                                  \
 }                                                                                                                                      \
 
#define CHECK_BUFFER_STATUS(sts) UMC_CHECK_STATUS(sts)

#else

#define CHECK_BUFFER_SIZE(nTh)
#define CHECK_BUFFER_STATUS(sts)

#endif

#define PUT_BITS(VAL, NBITS)        \
 {CHECK_BUFFER_SIZE()               \
  ippiPutBits(bb.current_pointer,   \
  bb.bit_offset,                    \
  VAL, NBITS)}

#define PUT_START_CODE(scode) {             \
  CHECK_BUFFER_SIZE()                       \
  Ipp32u code1, code2;                      \
  Ipp32s off = bb.bit_offset &~ 7;          \
  code1 = bb.current_pointer[0];            \
  code2 = 0;                                \
  if(off > 0) code1 |= (scode) >> (32-off); \
  if(off < 32) code2 = (scode) << off;      \
  bb.current_pointer[0] = BSWAP(code1);     \
  bb.current_pointer++;                     \
  bb.current_pointer[0] = code2;            \
  bb.bit_offset = off;                      \
}

#define FLUSH_BITSTREAM(bs, mfxBS)                  \
if (bs.bit_offset != 32) {                          \
  (bs.current_pointer)[0] = BSWAP((bs.current_pointer)[0]); \
  bs.bit_offset &= ~7;                              \
}

#define BITPOS(bs) \
  ((Ipp32s)((Ipp8u*)bs.current_pointer - (Ipp8u*)bs.start_pointer)*8 + 32 - bs.bit_offset)


typedef struct _VLCode_8u // variable length code
{
  mfxU8 code; // right justified
  mfxU8 len;
} VLCode_8u;

// const tables
extern const mfxU8 mfx_mpeg2_default_intra_qm[64];
extern const mfxI32 mfx_mpeg2_ZigZagScan[64];
extern const mfxI32 mfx_mpeg2_AlternateScan[64];
extern const mfxI32 mfx_mpeg2_Val_QScale[2][32];
extern const mfxI32 mfx_mpeg2_color_index[12];
extern const VLCode_8u mfx_mpeg2_CBP_VLC_Tbl[64];
extern const VLCode_8u mfx_mpeg2_AddrIncrementTbl[35];
extern const VLCode_8u mfx_mpeg2_mbtypetab[3][32];
extern const VLCode_8u mfx_mpeg2_MV_VLC_Tbl[33];
extern const IppVCHuffmanSpec_32u mfx_mpeg2_Y_DC_Tbl[12];
extern const IppVCHuffmanSpec_32u mfx_mpeg2_Cr_DC_Tbl[12];
extern const mfxI32 mfx_mpeg2_ResetTbl[4];
extern const Ipp32s mfx_mpeg2_dct_coeff_next_RL[];
extern const Ipp32s mfx_mpeg2_Table15[];
extern const IppiPoint mfx_mpeg2_MV_ZERO;

#endif // MFX_ENABLE_MPEG2_VIDEO_ENCODER
#endif // _MFX_MPEG2_PAK_TABS_H_
