//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

// Service function for inverse Transform 32x32 AVX2

// comment from Nablet guys:
// This is part of function DCTInverse32x32<_avx2>(). Unfortunately, because of some 
// bug in Intel Compiler (probably) it can not be included in the same source file. 
// Even with turned off interprocedural optimization.

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 

#pragma warning (disable : 4310 ) /* disable cast truncates constant value */

namespace MFX_HEVC_PP
{

// Reordering temporal buffer
// +---+---+---+---+---+---+---+---+       +---+------
// | A | B | C | D | E | F | G | H |   ->  | A |
// +---+---+---+---+---+---+---+---+       +---+
// |                               |       | C |
// |                               |       +---+
// |                               |       | E |
// |                               |       +---+
// |                               |       | G |

#define LOAD_8x16_UNPACK(addr)                                \
      y7 = _mm256_lddqu_si256((const __m256i *)(addr + 0*2)); \
      y1 = _mm256_lddqu_si256((const __m256i *)(addr + 1*2)); \
      y0 = _mm256_unpacklo_epi16(y7, y1);                     \
      y1 = _mm256_unpackhi_epi16(y7, y1);                     \
      y7 = _mm256_lddqu_si256((const __m256i *)(addr + 2*2)); \
      y3 = _mm256_lddqu_si256((const __m256i *)(addr + 3*2)); \
      y2 = _mm256_unpacklo_epi16(y7, y3);                     \
      y3 = _mm256_unpackhi_epi16(y7, y3);                     \
      y7 = _mm256_lddqu_si256((const __m256i *)(addr + 4*2)); \
      y5 = _mm256_lddqu_si256((const __m256i *)(addr + 5*2)); \
      y4 = _mm256_unpacklo_epi16(y7, y5);                     \
      y5 = _mm256_unpackhi_epi16(y7, y5);                     \
      y8 = _mm256_lddqu_si256((const __m256i *)(addr + 6*2)); \
      y7 = _mm256_lddqu_si256((const __m256i *)(addr + 7*2)); \
      y6 = _mm256_unpacklo_epi16(y8, y7);                     \
      y7 = _mm256_unpackhi_epi16(y8, y7);

#define SAVE_UNPACK_32(a1, a2, a3, a4)                \
      y8 = _mm256_unpacklo_epi32(y0, y2);             \
      y9 = _mm256_unpacklo_epi64(y4, y6);             \
      y2 = _mm256_unpackhi_epi32(y0, y2);             \
      y4 = _mm256_unpackhi_epi64(y4, y6);             \
                                                      \
      out[a1*8]         = _mm256_castsi256_si128(y8); \
      out[a1*8 + 1]     = _mm256_castsi256_si128(y9); \
      out[(a1+1)*8]     = _mm256_castsi256_si128(y2); \
      out[(a1+1)*8 + 1] = _mm256_castsi256_si128(y4); \
                                                      \
      out[a3*8]         = _mm256_extracti128_si256(y8, 1); \
      out[a3*8 + 1]     = _mm256_extracti128_si256(y9, 1); \
      out[(a3+1)*8]     = _mm256_extracti128_si256(y2, 1); \
      out[(a3+1)*8 + 1] = _mm256_extracti128_si256(y4, 1); \
                                                      \
      y0 = _mm256_unpacklo_epi32(y1, y3);             \
      y2 = _mm256_unpacklo_epi64(y5, y7);             \
      y1 = _mm256_unpackhi_epi32(y1, y3);             \
      y5 = _mm256_unpackhi_epi64(y5, y7);             \
                                                      \
      out[a2*8]         = _mm256_castsi256_si128(y0); \
      out[a2*8 + 1]     = _mm256_castsi256_si128(y2); \
      out[(a2+1)*8]     = _mm256_castsi256_si128(y1); \
      out[(a2+1)*8 + 1] = _mm256_castsi256_si128(y5); \
                                                      \
      out[a4*8]         = _mm256_extracti128_si256(y0, 1); \
      out[a4*8 + 1]     = _mm256_extracti128_si256(y2, 1); \
      out[(a4+1)*8]     = _mm256_extracti128_si256(y1, 1); \
      out[(a4+1)*8 + 1] = _mm256_extracti128_si256(y5, 1);

#define SAVE_UNPACK_64(a1, a2, a3, a4)                \
      y8 = _mm256_unpacklo_epi64(y0, y2);             \
      y9 = _mm256_unpacklo_epi64(y4, y6);             \
      y2 = _mm256_unpackhi_epi64(y0, y2);             \
      y4 = _mm256_unpackhi_epi64(y4, y6);             \
                                                      \
      out[a1*8]         = _mm256_castsi256_si128(y8); \
      out[a1*8 + 1]     = _mm256_castsi256_si128(y9); \
      out[(a1+1)*8]     = _mm256_castsi256_si128(y2); \
      out[(a1+1)*8 + 1] = _mm256_castsi256_si128(y4); \
                                                      \
      out[a3*8]         = _mm256_extracti128_si256(y8, 1); \
      out[a3*8 + 1]     = _mm256_extracti128_si256(y9, 1); \
      out[(a3+1)*8]     = _mm256_extracti128_si256(y2, 1); \
      out[(a3+1)*8 + 1] = _mm256_extracti128_si256(y4, 1); \
                                                      \
      y0 = _mm256_unpacklo_epi64(y1, y3);             \
      y2 = _mm256_unpacklo_epi64(y5, y7);             \
      y1 = _mm256_unpackhi_epi64(y1, y3);             \
      y5 = _mm256_unpackhi_epi64(y5, y7);             \
                                                      \
      out[a2*8]         = _mm256_castsi256_si128(y0); \
      out[a2*8 + 1]     = _mm256_castsi256_si128(y2); \
      out[(a2+1)*8]     = _mm256_castsi256_si128(y1); \
      out[(a2+1)*8 + 1] = _mm256_castsi256_si128(y5); \
                                                      \
      out[a4*8]         = _mm256_extracti128_si256(y0, 1); \
      out[a4*8 + 1]     = _mm256_extracti128_si256(y2, 1); \
      out[(a4+1)*8]     = _mm256_extracti128_si256(y1, 1); \
      out[(a4+1)*8 + 1] = _mm256_extracti128_si256(y5, 1);


void reordering(signed short* H265_RESTRICT dest, const signed short* H265_RESTRICT src)
{
   __m256i y0, y1, y2, y3, y4, y5, y6, y7, y8, y9;
   __m128i * out = (__m128i *)dest;
   __m256i * iny  = (__m256i *)src;
   _mm256_zeroall();
   {
      // load buffer short[8x16], rotate every 4x4, interleave, save ..
      LOAD_8x16_UNPACK(iny)
      SAVE_UNPACK_32(0, 14, 2, 12)
      LOAD_8x16_UNPACK(iny + 1)
      SAVE_UNPACK_32(4, 10, 6, 8)
      iny += 16;
      out +=  2;
   }

   for (int i=0; i<3; i++)
   {
      LOAD_8x16_UNPACK(iny)
      SAVE_UNPACK_64(0, 14, 2, 12)
      LOAD_8x16_UNPACK(iny + 1)
      SAVE_UNPACK_64(4, 10, 6, 8)
      iny += 16;
      out +=  2;
   }
}

} // end namespace MFX_HEVC_PP

#endif //#if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
