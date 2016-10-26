//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "vp8_dec_defs.h"
#include "umc_vp8_decoder.h"

using namespace UMC;

namespace UMC
{

const Ipp32s vp8_quant_dc[VP8_MAX_QP + 1 + 32] =
{
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,

  4, 5, 6, 7, 8, 9, 10, 10, 11, 12, 13, 14, 15, 16, 17, 17,
  18, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 28,
  29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40, 41, 42, 43,
  44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
  59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
  75, 76, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
  91, 93, 95, 96, 98, 100, 101, 102, 104, 106, 108, 110, 112, 114, 116, 118,
  122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 143, 145, 148, 151, 154, 157,

  157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157
};


const Ipp32s vp8_quant_ac[VP8_MAX_QP + 1 + 32] =
{
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,

  4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
  20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
  36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53, 54, 55, 56, 57, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76,
  78, 80, 82, 84, 86, 88, 90, 92, 94, 96, 98, 100, 102, 104, 106, 108,
  110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 146, 149, 152,
  155, 158, 161, 164, 167, 170, 173, 177, 181, 185, 189, 193, 197, 201, 205, 209,
  213, 217, 221, 225, 229, 234, 239, 245, 249, 254, 259, 264, 269, 274, 279, 284,

  284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284
};


const Ipp32s vp8_quant_dc2[VP8_MAX_QP + 1 + 32] =
{
  4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2,

  4*2, 5*2, 6*2, 7*2, 8*2, 9*2, 10*2, 10*2, 11*2, 12*2, 13*2, 14*2, 15*2, 16*2, 17*2, 17*2,
  18*2, 19*2, 20*2, 20*2, 21*2, 21*2, 22*2, 22*2, 23*2, 23*2, 24*2, 25*2, 25*2, 26*2, 27*2, 28*2,
  29*2, 30*2, 31*2, 32*2, 33*2, 34*2, 35*2, 36*2, 37*2, 37*2, 38*2, 39*2, 40*2, 41*2, 42*2, 43*2,
  44*2, 45*2, 46*2, 46*2, 47*2, 48*2, 49*2, 50*2, 51*2, 52*2, 53*2, 54*2, 55*2, 56*2, 57*2, 58*2,
  59*2, 60*2, 61*2, 62*2, 63*2, 64*2, 65*2, 66*2, 67*2, 68*2, 69*2, 70*2, 71*2, 72*2, 73*2, 74*2,
  75*2, 76*2, 76*2, 77*2, 78*2, 79*2, 80*2, 81*2, 82*2, 83*2, 84*2, 85*2, 86*2, 87*2, 88*2, 89*2,
  91*2, 93*2, 95*2, 96*2, 98*2, 100*2, 101*2, 102*2, 104*2, 106*2, 108*2, 110*2, 112*2, 114*2, 116*2, 118*2,
  122*2, 124*2, 126*2, 128*2, 130*2, 132*2, 134*2, 136*2, 138*2, 140*2, 143*2, 145*2, 148*2, 151*2, 154*2, 157*2,

  157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2
};

const Ipp32s vp8_quant_ac2[VP8_MAX_QP + 1 + 32] =
{
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,

  8, 8, 6*155/100, 7*155/100, 8*155/100, 9*155/100, 10*155/100, 11*155/100, 12*155/100, 13*155/100, 14*155/100, 15*155/100, 16*155/100, 17*155/100, 18*155/100, 19*155/100,
  20*155/100, 21*155/100, 22*155/100, 23*155/100, 24*155/100, 25*155/100, 26*155/100, 27*155/100, 28*155/100, 29*155/100, 30*155/100, 31*155/100, 32*155/100, 33*155/100, 34*155/100, 35*155/100,
  36*155/100, 37*155/100, 38*155/100, 39*155/100, 40*155/100, 41*155/100, 42*155/100, 43*155/100, 44*155/100, 45*155/100, 46*155/100, 47*155/100, 48*155/100, 49*155/100, 50*155/100, 51*155/100,
  52*155/100, 53*155/100, 54*155/100, 55*155/100, 56*155/100, 57*155/100, 58*155/100, 60*155/100, 62*155/100, 64*155/100, 66*155/100, 68*155/100, 70*155/100, 72*155/100, 74*155/100, 76*155/100,
  78*155/100, 80*155/100, 82*155/100, 84*155/100, 86*155/100, 88*155/100, 90*155/100, 92*155/100, 94*155/100, 96*155/100, 98*155/100, 100*155/100, 102*155/100, 104*155/100, 106*155/100, 108*155/100,
  110*155/100, 112*155/100, 114*155/100, 116*155/100, 119*155/100, 122*155/100, 125*155/100, 128*155/100, 131*155/100, 134*155/100, 137*155/100, 140*155/100, 143*155/100, 146*155/100, 149*155/100, 152*155/100,
  155*155/100, 158*155/100, 161*155/100, 164*155/100, 167*155/100, 170*155/100, 173*155/100, 177*155/100, 181*155/100, 185*155/100, 189*155/100, 193*155/100, 197*155/100, 201*155/100, 205*155/100, 209*155/100,
  213*155/100, 217*155/100, 221*155/100, 225*155/100, 229*155/100, 234*155/100, 239*155/100, 245*155/100, 249*155/100, 254*155/100, 259*155/100, 264*155/100, 269*155/100, 274*155/100, 279*155/100, 284*155/100,

  284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284
};


const Ipp32s vp8_quant_dc_uv[VP8_MAX_QP + 1 + 32] =
{
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,

  4, 5, 6, 7, 8, 9, 10, 10, 11, 12, 13, 14, 15, 16, 17, 17,
  18, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 28,
  29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40, 41, 42, 43,
  44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
  59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
  75, 76, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
  91, 93, 95, 96, 98, 100, 101, 102, 104, 106, 108, 110, 112, 114, 116, 118,
  122, 124, 126, 128, 130, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,

  132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132
};



#define DECODE_DELTA_QP(res, shift) \
{ \
  Ipp32u mask = (1 << (shift - 1)); \
  Ipp32s val; \
  if (bits & mask) \
  { \
    bits = (bits << 5) | DecodeValue_Prob128(pBooldec, 5); \
    val  = (Ipp32s)((bits >> shift) & 0xF); \
    res  = (bits & mask) ? -val : val; \
  } \
}


Status VP8VideoDecoder::DecodeInitDequantization(void)
{
  vp8BooleanDecoder *pBooldec = &m_BoolDecoder[0];
  Ipp8u bits;

  m_QuantInfo.yacQP = (Ipp32s)DecodeValue_Prob128(pBooldec, 7);

  //m_QuantInfo.pYacQP = vp8_quant_ac + 16; set in Init

  m_QuantInfo.ydcDeltaQP  = 0;
  m_QuantInfo.y2dcDeltaQP = 0;
  m_QuantInfo.y2acDeltaQP = 0;
  m_QuantInfo.uvdcDeltaQP = 0;
  m_QuantInfo.uvacDeltaQP = 0;

  bits = DecodeValue_Prob128(pBooldec, 5);

  if (bits)
  {
    DECODE_DELTA_QP(m_QuantInfo.ydcDeltaQP,  5)
    DECODE_DELTA_QP(m_QuantInfo.y2dcDeltaQP, 4)
    DECODE_DELTA_QP(m_QuantInfo.y2acDeltaQP, 3)
    DECODE_DELTA_QP(m_QuantInfo.uvdcDeltaQP, 2)
    DECODE_DELTA_QP(m_QuantInfo.uvacDeltaQP, 1)
  }

  Ipp32s qp;

  for(Ipp32s segment_id = 0; segment_id < VP8_MAX_NUM_OF_SEGMENTS; segment_id++)
  {
    if (m_FrameInfo.segmentationEnabled)
    {
      if (m_FrameInfo.segmentAbsMode)
        qp = m_FrameInfo.segmentFeatureData[VP8_ALT_QUANT][segment_id];
      else
      {
        qp = m_QuantInfo.yacQP + m_FrameInfo.segmentFeatureData[VP8_ALT_QUANT][segment_id];
        vp8_CLIP(qp, 0, VP8_MAX_QP);
      }
    }
    else
      qp = m_QuantInfo.yacQP;

    m_QuantInfo.yacQ[segment_id]  = vp8_quant_ac[qp  + 16];
    m_QuantInfo.ydcQ[segment_id]  = vp8_quant_dc[qp  + 16 + m_QuantInfo.ydcDeltaQP];

    m_QuantInfo.y2acQ[segment_id] = vp8_quant_ac2[qp + 16 + m_QuantInfo.y2acDeltaQP];
    m_QuantInfo.y2dcQ[segment_id] = vp8_quant_dc2[qp + 16 + m_QuantInfo.y2dcDeltaQP];

    m_QuantInfo.uvacQ[segment_id] = vp8_quant_ac[qp    + 16 + m_QuantInfo.uvacDeltaQP];
    m_QuantInfo.uvdcQ[segment_id] = vp8_quant_dc_uv[qp + 16 + m_QuantInfo.uvdcDeltaQP];

  }

  return UMC_OK;
} // VP8VideoDecoder::DecodeInitDequantization()


void VP8VideoDecoder::DequantMbCoeffs(vp8_MbInfo* pMb)
{
  vp8_MbInfo      *currMb = pMb;
  vp8_QuantInfo   *qi     = &m_QuantInfo;

  Ipp16s* pCoefs     = 0;
  Ipp32s  b          = 0;
  Ipp32s  firstCoeff = 0;
  Ipp32s  c          = 0;

  if(VP8_MV_SPLIT != currMb->mode && VP8_MB_B_PRED != currMb->mode)
  {
    pCoefs = currMb->coeffs;

    // DC for Y2
    pCoefs[0] = (Ipp16s)(pCoefs[0] * qi->y2dcQ[currMb->segmentID]);

    // AC for Y2
    for(c = 1; c < 16; c++)
      pCoefs[c] =  (Ipp16s)(pCoefs[c] * qi->y2acQ[currMb->segmentID]);

    firstCoeff = 1;
  }

  pCoefs = currMb->coeffs + 4*4; // skip first 4x4 block (Y2)

  // first 16 Y coefs block`s
  for(b = 0; b < 16; b++)
  {
    // DC for Y
    if(!firstCoeff) 
      pCoefs[0] = (Ipp16s)(pCoefs[0] * qi->ydcQ[currMb->segmentID]);

    // AC for Y
    for(c = 1; c < 16; c++)
      pCoefs[c] =  (Ipp16s)(pCoefs[c] * qi->yacQ[currMb->segmentID]);

    pCoefs += 16;
  }

  // 8 UV coefs block`s
  for(b = 16; b < 24; b++)
  {
    // DC for UV
    pCoefs[0] = (Ipp16s)(pCoefs[0] * qi->uvdcQ[currMb->segmentID]);

     // AC for UV
    for(c = 1; c < 16; c++)
      pCoefs[c] = (Ipp16s)(pCoefs[c] * qi->uvacQ[currMb->segmentID]);

    pCoefs += 16;
  }

  return;
}  // VP8VideoDecoder::DequantMBCoeffs()

} // namespace UMC

#endif // UMC_ENABLE_VP8_VIDEO_DECODER
