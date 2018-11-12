// Copyright (c) 2002-2018 Intel Corporation
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

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC)


//#include "vm_time.h"
#include "mfx_mpeg2_enc.h"


#define SAD_FUNC_16x16 \
  ippiSAD16x16_8u32s(pBlock,     \
                     BlockStep,  \
                     ref_data,   \
                     RefStep,    \
                     &MAD,       \
                     flag)

#define SAD_FUNC_16x8 \
  ippiSAD16x8_8u32s_C1R(pBlock,      \
                        BlockStep,   \
                        ref_data,    \
                        RefStep,     \
                        &MAD,        \
                        flag)

/*#define SAD_FUNC_16x16_fullpix      \
  ippiSAD16x8_8u32s_C1R(pBlock,     \
                    2*BlockStep,    \
                    ref_data,       \
                    2*RefStep,      \
                    &MAD,           \
                    0)*/
#define SAD_FUNC_16x16_fullpix \
  SAD_FUNC_16x16

#define SAD_FUNC_16x8_fullpix \
  SAD_FUNC_16x8

#define VARMEAN_FUNC_16x16                  \
  ippiVarMeanDiff16x16_8u32s_C1R(           \
                          pBlock,           \
                          BlockStep,        \
                          ref_data,         \
                          RefStep,          \
                          pSrcMean,         \
                          Var[!min_index],  \
                          Mean[!min_index], \
                          flag);            \
  MAD = Var[!min_index][0] + Var[!min_index][1] + \
        Var[!min_index][2] + Var[!min_index][3]

#define VARMEAN_FUNC_16x8                   \
  ippiVarMeanDiff16x8_8u32s_C1R(            \
                          pBlock,           \
                          BlockStep,        \
                          ref_data,         \
                          RefStep,          \
                          pSrcMean,         \
                          Var[!min_index],  \
                          Mean[!min_index], \
                          flag);            \
  MAD = Var[!min_index][0] + Var[!min_index][1]

#define CMP_MAD                                            \
if(MAD < BestMAD)                                          \
{                                                          \
  min_point = 1;                                           \
  BestMAD = MAD;                                           \
  min_index = !min_index;                                  \
  XMIN = XN;                                               \
  YMIN = YN;                                               \
}


/***********************************************************/

#define ME_FUNC     MotionEstimation_Frame
#define FIELD_FLAG  0
#define ME_STEP_X   2
#define ME_STEP_Y   2
#define ME_MIN_STEP 2
#define NUM_BLOCKS  4

#define TRY_POSITION_REC(FUNC)                               \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)          \
{                                                            \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id;      \
  ref_data = (pRecFld + (XN >> 1) + (YN >> 1)*RefStep);      \
  flag = ((XN & 1) << 3) | ((YN & 1) << 2);                  \
                                                             \
  FUNC##_16x16;                                              \
                                                             \
  CMP_MAD                                                    \
}

#define TRY_POSITION_REF(FUNC)                               \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)          \
{                                                            \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id;      \
  ref_data = (pRefFld + (XN >> 1) + (YN >> 1)*RefStep);      \
  flag = 0;                                                  \
                                                             \
  FUNC##_16x16_fullpix;                                      \
                                                             \
  CMP_MAD                                                    \
}

#include "mfx_mpeg2_enc_me.h"

/***********************************************************/

#undef  ME_FUNC
#define ME_FUNC  MotionEstimation_FieldPict

#undef  NUM_BLOCKS
#define NUM_BLOCKS  2

#undef  TRY_POSITION_REC
#define TRY_POSITION_REC(FUNC) \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)     \
{                                                       \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id; \
  ref_data = (pRecFld + (XN >> 1) + (YN >> 1)*RefStep); \
  flag = ((XN & 1) << 3) | ((YN & 1) << 2);             \
                                                        \
  FUNC##_16x8;                                          \
                                                        \
  CMP_MAD                                               \
}

#undef  TRY_POSITION_REF
#define TRY_POSITION_REF(FUNC) \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)     \
{                                                       \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id; \
  ref_data = (pRefFld + (XN >> 1) + (YN >> 1)*RefStep); \
  flag = 0;                                             \
                                                        \
  FUNC##_16x8_fullpix;                                  \
                                                        \
  CMP_MAD                                               \
}

#include "mfx_mpeg2_enc_me.h"

/***********************************************************/

#undef  ME_FUNC
#define ME_FUNC  MotionEstimation_Field

#undef  FIELD_FLAG
#define FIELD_FLAG  1

#undef  NUM_BLOCKS
#define NUM_BLOCKS  2

#undef  TRY_POSITION_REC
#define TRY_POSITION_REC(FUNC) \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)      \
{                                                        \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id;  \
  ref_data = (pRecFld + (XN >> 1) + (YN >> 1)*RefStep);  \
  flag = ((XN & 1) << 3) | ((YN & 1) << 2);              \
                                                         \
  FUNC##_16x8;                                           \
                                                         \
  CMP_MAD                                                \
}

#undef  TRY_POSITION_REF
#define TRY_POSITION_REF(FUNC) \
if (me_matrix[YN*me_matrix_w + XN] != me_matrix_id)      \
{                                                        \
  me_matrix[YN*me_matrix_w + XN] = (Ipp8u)me_matrix_id;  \
  ref_data = (pRefFld + (XN >> 1) + (YN >> 1)*RefStep);  \
  flag = 0;                                              \
                                                         \
  FUNC##_16x8_fullpix;                                   \
                                                         \
  CMP_MAD                                                \
}

#include "mfx_mpeg2_enc_me.h"

/***********************************************************/


#endif // MFX_ENABLE_MPEG2_VIDEO_ENC
