//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "vp8_dec_defs.h"
#include "umc_vp8_mfx_decode.h"

using namespace UMC;

namespace UMC
{

const Ipp8u vp8_range_normalization_shift[64] = 
{
  7, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

void VP8VideoDecoderSoftware::UpdateCoeffProbabilitites(void)
{
  Ipp32s i;
  Ipp32s j;
  Ipp32s k;
  Ipp32s l;

  vp8BooleanDecoder *pBoolDec = &m_boolDecoder[0];

  for (i = 0; i < VP8_NUM_COEFF_PLANES; i++)
  {
    for (j = 0; j < VP8_NUM_COEFF_BANDS; j++)
    {
      for (k = 0; k < VP8_NUM_LOCAL_COMPLEXITIES; k++)
      {
        for (l = 0; l < VP8_NUM_COEFF_NODES; l++)
        {
          Ipp8u flag;
          Ipp8u prob = vp8_coeff_update_probs[i][j][k][l];

          VP8_DECODE_BOOL(pBoolDec, prob, flag);

          if (flag)
            m_frameProbs.coeff_probs[i][j][k][l] = (Ipp8u)DecodeValue(pBoolDec,128, 8);
        }
      }
    }
  }
} // VP8VideoDecoderSoftware::UpdateCoeffProbabilitites()


Status VP8VideoDecoderSoftware::InitBooleanDecoder(Ipp8u *pBitStream, Ipp32s dataSize, Ipp32s dec_number)
{
  if(0 == pBitStream)
    return UMC_ERR_NULL_PTR; 

  if (dec_number >= VP8_MAX_NUMBER_OF_PARTITIONS)
    return UMC_ERR_INVALID_PARAMS;

  if (dataSize < 1)
    return UMC_ERR_INVALID_PARAMS;


  dataSize = IPP_MIN(dataSize, 2);

  vp8BooleanDecoder *pBooldec = &m_boolDecoder[dec_number];

  pBooldec->pData     = pBitStream;
  pBooldec->data_size = dataSize;
  pBooldec->range     = 255;
  pBooldec->bitcount  = 0;
  pBooldec->value     = 0;

  for(Ipp8u i = 0; i < dataSize; i++)
    pBooldec->value = (pBooldec->value <<  (8*i)) | (pBooldec->pData[i]);

  pBooldec->pData    += dataSize;

  return UMC_OK;
}

Status VP8VideoDecoderSoftware::UpdateSegmentation(vp8BooleanDecoder *pBooldec)
{
  Ipp32u bits;

  Ipp32s i;
  Ipp32s j;
  Ipp32s res;

  bits = DecodeValue_Prob128(pBooldec, 2);

  m_frameInfo.updateSegmentMap  = (Ipp8u)bits >> 1;
  m_frameInfo.updateSegmentData = (Ipp8u)bits & 1;

  if (m_frameInfo.updateSegmentData)
  {
    VP8_DECODE_BOOL_PROB128(pBooldec, m_frameInfo.segmentAbsMode);
    UMC_SET_ZERO(m_frameInfo.segmentFeatureData);

    for (i = 0; i < VP8_NUM_OF_MB_FEATURES; i++)
    {
      for (j = 0; j < VP8_MAX_NUM_OF_SEGMENTS; j++)
      {
        VP8_DECODE_BOOL_PROB128(pBooldec, bits);

        if (bits)
        {
          bits = DecodeValue_Prob128(pBooldec, 8-i); // 7 bits for ALT_QUANT, 6 - for ALT_LOOP_FILTER; +sign
          res = bits >> 1;

          if (bits & 1)
            res = -res;

          m_frameInfo.segmentFeatureData[i][j] = (Ipp8s)res;
        }
      }
    }
  }

  if (m_frameInfo.updateSegmentMap)
  {
    for (i = 0; i < VP8_NUM_OF_SEGMENT_TREE_PROBS; i++)
    {
      VP8_DECODE_BOOL_PROB128(pBooldec, bits);

      if (bits)
        bits = DecodeValue_Prob128(pBooldec, 8);
      else
        bits = 255;

      m_frameInfo.segmentTreeProbabilities[i] = (Ipp8u)bits;
    }
  }

  return UMC_OK;
} // VP8VideoDecoderSoftware::UpdateSegmentation()

Status VP8VideoDecoderSoftware::UpdateLoopFilterDeltas(vp8BooleanDecoder *pBooldec)
{
  Ipp8u  bits;
  Ipp32s i;
  Ipp32s res;

  for (i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
  {
    VP8_DECODE_BOOL(pBooldec, 128, bits);
    if (bits)
    {
      bits = DecodeValue_Prob128(pBooldec, 7);
      res = bits >> 1;

      if (bits & 1)
        res = -res;

      m_frameInfo.refLoopFilterDeltas[i] = (Ipp8s)res;
    }
  }

  for (i = 0; i < VP8_NUM_OF_MODE_LF_DELTAS; i++)
  {
    VP8_DECODE_BOOL(pBooldec, 128, bits);
    if (bits)
    {
      bits = DecodeValue_Prob128(pBooldec, 7);
      res = bits >> 1;

      if (bits & 1)
        res = -res;

      m_frameInfo.modeLoopFilterDeltas[i] = (Ipp8s)res;
    }
  }

  return UMC_OK;
} // VP8VideoDecoderSoftware::UpdateLoopFilterDeltas()

/*Ipp32s vp8_ReadTree(vp8BooleanDecoder *pBooldec, const Ipp8s *pTree, const Ipp8u *pProb)
{
  Ipp32s i;
  Ipp8u bit;
  for (i = 0;;)
  {
    VP8_DECODE_BOOL(pBooldec, pProb[i >> 1], bit);
    i = pTree[i + bit];

    if (i <= 0)
      break;
  }

  return -i;
} // vp8_ReadTree
*/


Ipp8u VP8VideoDecoderSoftware::DecodeValue_Prob128(vp8BooleanDecoder *pBooldec, Ipp32u numbits)
{
  Ipp32s n;

  Ipp8u val = 0;
  Ipp8u bit = 0;

  for(n = numbits - 1; n >= 0; n--)
  {
    VP8_DECODE_BOOL(pBooldec,128, bit);
    val = val << 1 | bit;
  }

  return val;
}

Ipp8u VP8VideoDecoderSoftware::DecodeValue(vp8BooleanDecoder *pBooldec, Ipp8u prob, Ipp32u numbits)
{
  Ipp32s n;

  Ipp8u val = 0;
  Ipp8u bit = 0;

  for(n = numbits - 1; n >= 0; n--)
  {
    VP8_DECODE_BOOL(pBooldec, prob, bit);
    val = val << 1 | bit;
  }

  return val;
}


} // namespace UMC
#endif // UMC_ENABLE_VP8_VIDEO_DECODER
