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

#ifndef __VP8_BOOL_DEC_H__
#define __VP8_BOOL_DEC_H__

using namespace UMC;

namespace UMC
{

typedef struct _vp8BooleanDecoder
{
  Ipp32u range;
  Ipp32u value;
  Ipp32s bitcount;
  Ipp8u *pData;
  Ipp32s data_size;

  Ipp8u blContextLeft[9];
} vp8BooleanDecoder;


#define VP8_DECODE_BOOL(booldec, prob, ret) \
{ \
  Ipp32u split, split256; \
  Ipp32u range = (booldec)->range; \
  Ipp32u value = (booldec)->value; \
  split    = 1 + (((range - 1) * (prob)) >> 8); \
  split256 = (split << 8); \
  if (value < split256) \
  { \
    range = split; \
    ret   = 0; \
  } \
  else \
  { \
    range -= split; \
    value -= split256; \
    ret    = 1; \
  } \
  if (range < 128) \
  { \
    Ipp32u shift = vp8_range_normalization_shift[range >> 1]; \
    Ipp32s bitcount = (booldec)->bitcount; \
    range <<= shift; \
    value <<= shift; \
    bitcount += shift; \
    if (bitcount >= 8) \
    { \
      bitcount -= 8; \
      value    |= *((booldec)->pData) << bitcount; \
      (booldec)->pData++; \
    } \
    (booldec)->bitcount = bitcount; \
  } \
  (booldec)->range = range; \
  (booldec)->value = value; \
}



//#define VP8_DECODE_BOOL_PROB128_1st(booldec, ret) \
//{ \
//  Ipp32u split, split256; \
//  Ipp32u range = (booldec)->range; \
//  Ipp32u value = (booldec)->value; \
//  split    = (range + 1) >> 1; \
//  split256 = (split << 8); \
//  if (value < split256) \
//  { \
//    range = split; \
//    ret   = 0; \
//  } \
//  else \
//  { \
//    range -= split; \
//    value -= split256; \
//    ret    = 1; \
//  } \
//  { \
//    Ipp32s bitcount = (booldec)->bitcount + 1; \
//    range <<= 1; \
//    value <<= 1; \
//    if (bitcount == 8) \
//    { \
//      bitcount = 0; \
//      value   |= *((booldec)->pData); \
//      (booldec)->pData++; \
//    } \
//    (booldec)->bitcount = bitcount; \
//  } \
//  (booldec)->range = range; \
//  (booldec)->value = value; \
//}


// can be simplified further 
// if the 1st bit is 0, range becomes = split = 128 -> split = 64, we can simply read bits from the stream
// if the partition starts with 1, range = 254, split = 127, and everything is not so simple
#define VP8_DECODE_BOOL_PROB128(booldec, ret) \
{ \
  Ipp32u split256; \
  Ipp32u value = (booldec)->value; \
  split256 = ((booldec)->range << 7); \
  if (value < split256) \
  { \
    ret = 0; \
  } \
  else \
  { \
    value -= split256; \
    ret = 1; \
  } \
  Ipp32s bitcount = (booldec)->bitcount + 1; \
  value <<= 1; \
  if (bitcount == 8) \
  { \
    bitcount = 0; \
    value |= static_cast<Ipp32u>(*((booldec)->pData)); \
    (booldec)->pData++; \
  } \
  (booldec)->bitcount = bitcount; \
  (booldec)->value = value; \
}


Ipp32s vp8_ReadTree(vp8BooleanDecoder *pBooldec, const Ipp8s *pTree, const Ipp8u *pProb);


#define VP8_DECODE_BOOL_LOCAL(prob, ret) \
{ \
  Ipp32u split, split256; \
  split = 1 + (((range - 1) * (prob)) >> 8); \
  split256 = (split << 8); \
  if (value < split256) \
  { \
    range = split; \
    ret = 0; \
  } \
  else \
  { \
    range -= split; \
    value -= split256; \
    ret = 1; \
  } \
  if (range < 128) \
  { \
    Ipp32u shift = vp8_range_normalization_shift[range >> 1]; \
    range <<= shift; \
    value <<= shift; \
    bitcount += shift; \
    if (bitcount >= 8) \
    { \
      bitcount -= 8; \
      value |= (*pData) << bitcount; \
      pData++; \
    } \
  } \
}

} // namespace UMC

#endif // __VP8_BOOL_DEC_H__

#endif // MFX_ENABLE_VP8_VIDEO_DECODE
