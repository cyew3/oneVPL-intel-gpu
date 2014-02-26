/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/

/*
// HEVC forward quantization, optimized with AVX2
// 
// NOTES:
// - requires AVX2
// - assumes that len is multiple of 8
// - assumes that (offset >> shift) == 0 (for psign: assume input 0 stays 0 after quantization)
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_AVX2) && defined(MFX_TARGET_OPTIMIZATION_AVX2)

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

namespace MFX_HEVC_PP
{
    void H265_FASTCALL MAKE_NAME(h265_QuantFwd_16s)(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift)
    {
        __m128i xmm_shift, xmm_src;
        __m256i ymm0, ymm1, ymm3, ymm4;

        VM_ASSERT((len & 0x07) == 0);
        VM_ASSERT((offset >> shift) == 0);

        _mm256_zeroupper();

        ymm3 = _mm256_set1_epi32(scale);
        ymm4 = _mm256_set1_epi32(offset);
        xmm_shift = _mm_cvtsi32_si128(shift);

        for (Ipp32s i = 0; i < len; i += 8) {
            // load 16-bit coefs, expand to 32 bits
            xmm_src = _mm_loadu_si128((__m128i *)&pSrc[i]);
            ymm0 = _mm256_cvtepi16_epi32(xmm_src);
            ymm1 = _mm256_abs_epi32(ymm0);

            // qval = (aval * scale + offset) >> shift; 
            ymm1 = _mm256_mullo_epi32(ymm1, ymm3);
            ymm1 = _mm256_add_epi32(ymm1, ymm4);
            ymm1 = _mm256_sra_epi32(ymm1, xmm_shift);

            // pDst = clip((qval ^ sign) - sign)
            ymm1 = _mm256_sign_epi32(ymm1, ymm0);
            ymm1 = _mm256_packs_epi32(ymm1, ymm1);
            ymm1 = _mm256_permute4x64_epi64(ymm1, 0xd8);

            _mm_storeu_si128((__m128i *)&pDst[i], mm128(ymm1));
        }
    } // void h265_QuantFwd_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scaleLevel, int scaleOffset, int scale)


    Ipp32s H265_FASTCALL MAKE_NAME(h265_QuantFwd_SBH_16s)(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift)
    {
        Ipp32s abs_sum = 0;
        __m128i xmm_shift, xmm_shift_m8, xmm_src;
        __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm7;

        VM_ASSERT((len & 0x07) == 0);
        VM_ASSERT((offset >> shift) == 0);

        ymm3 = _mm256_set1_epi32(scale);
        ymm4 = _mm256_set1_epi32(offset);
        ymm7 = _mm256_setzero_si256();

        xmm_shift    = _mm_cvtsi32_si128(shift);
        xmm_shift_m8 = _mm_cvtsi32_si128(shift - 8);

        for (Ipp32s i = 0; i < len; i += 8) {
            // load 16-bit coefs, expand to 32 bits
            xmm_src = _mm_loadu_si128((__m128i *)&pSrc[i]);
            ymm0 = _mm256_cvtepi16_epi32(xmm_src);
            ymm1 = _mm256_abs_epi32(ymm0);

            // qval = (aval * scale + offset) >> shift; 
            ymm1 = _mm256_abs_epi32(ymm0);
            ymm1 = _mm256_mullo_epi32(ymm1, ymm3);
            ymm2 = ymm1;                                 // save copy of aval*scale
            ymm1 = _mm256_add_epi32(ymm1, ymm4);
            ymm1 = _mm256_sra_epi32(ymm1, xmm_shift);    // ymm1 = qval

            // pDelta = (aval * scale - (qval << shift)) >> (shift-8)
            ymm1 = _mm256_sll_epi32(ymm1, xmm_shift);    // qval <<= shift
            ymm2 = _mm256_sub_epi32(ymm2, ymm1);         // aval*scale - (qval<<shift)
            ymm1 = _mm256_sra_epi32(ymm1, xmm_shift);    // qval >>= shift (restore)
            ymm2 = _mm256_sra_epi32(ymm2, xmm_shift_m8); // >> (shift - 8)
            ymm7 = _mm256_add_epi32(ymm7, ymm1);         // abs_sum += qval

            // pDst = clip((qval ^ sign) - sign)
            ymm1 = _mm256_sign_epi32(ymm1, ymm0);
            ymm1 = _mm256_packs_epi32(ymm1, ymm1);
            ymm1 = _mm256_permute4x64_epi64(ymm1, 0xd8);

            _mm_storeu_si128((__m128i *)&pDst[i], mm128(ymm1));
            _mm256_storeu_si256((__m256i *)&pDelta[i], ymm2);
        }

        // return overall abs_sum
        ymm7 = _mm256_hadd_epi32(ymm7, ymm7);
        ymm7 = _mm256_permute4x64_epi64(ymm7, 0xd8);
        ymm7 = _mm256_hadd_epi32(ymm7, ymm7);
        ymm7 = _mm256_hadd_epi32(ymm7, ymm7);
        abs_sum = _mm_cvtsi128_si32(mm128(ymm7));

        return abs_sum;
    } // Ipp32s h265_QuantFwd_SBH_16s(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scaleLevel, int scaleOffset, int scale)

} // end namespace MFX_HEVC_PP

#endif // #if defined (MFX_TARGET_OPTIMIZATION_PX) || (MFX_TARGET_OPTIMIZATION_SSE4) || (MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */
