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
#include "assert.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_AVX2) && defined(MFX_TARGET_OPTIMIZATION_AVX2)

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

namespace MFX_HEVC_PP
{
    void H265_FASTCALL MAKE_NAME(h265_QuantFwd_16s)(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale_, int offset_, int shift_)
    {
        assert((len & 0xf) == 0);
        assert((offset_ >> shift_) == 0);

        __m256i scale  = _mm256_set1_epi16(scale_);
        __m256i offset = _mm256_set1_epi32(offset_);
        __m128i shift  = _mm_cvtsi32_si128(shift_);

        for (Ipp32s i = 0; i < len; i += 16, pSrc += 16, pDst += 16) {
            __m256i src = _mm256_loadu_si256((__m256i *)pSrc); // load 16-bit coefs
            __m256i lo = _mm256_mullo_epi16(src, scale); // *=scale
            __m256i hi = _mm256_mulhi_epi16(src, scale);
            __m256i half1 = _mm256_unpacklo_epi16(lo, hi);
            __m256i half2 = _mm256_unpackhi_epi16(lo, hi);
            __m256i half1Abs = _mm256_abs_epi32(half1); // remove sign
            __m256i half2Abs = _mm256_abs_epi32(half2);
            half1Abs = _mm256_add_epi32(half1Abs, offset); // +=offset
            half2Abs = _mm256_add_epi32(half2Abs, offset);
            half1Abs = _mm256_sra_epi32(half1Abs, shift); // >>=shift
            half2Abs = _mm256_sra_epi32(half2Abs, shift);
            half1 = _mm256_sign_epi32(half1Abs, half1); // restore sign
            half2 = _mm256_sign_epi32(half2Abs, half2);
            __m256i dst = _mm256_packs_epi32(half1, half2); // pack
            _mm256_storeu_si256((__m256i *)pDst, dst); // store
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

    void H265_FASTCALL MAKE_NAME(h265_Quant_zCost_16s)(const Ipp16s* pSrc, Ipp32u* qLevels, Ipp64s* zlCosts, Ipp32s len, Ipp32s qScale, Ipp32s qoffset, Ipp32s qbits, Ipp32s rdScale0)
    {
        int i;
        __m256i ymm0, ymm1, ymm2, ymm3, ymm6, ymm7, ymm10, ymm11, ymm13;
        __m128i xmm4, xmm12;

        VM_ASSERT((len & 0x03) == 0);
        _mm256_zeroupper();
        ymm10 = _mm256_set1_epi32(qScale);
        ymm11 = _mm256_set1_epi32(qoffset);
        xmm12 = _mm_cvtsi32_si128(qbits);
        ymm13 = _mm256_setr_epi32 (rdScale0, 0, rdScale0, 0, rdScale0, 0, rdScale0, 0);

        for (i = 0; i < len; i += 8) 
        {
            // load 16-bit coefs, expand to 32 bits
            xmm4 = _mm_loadu_si128((__m128i *)&pSrc[i]);       // r0:= [c7, c6, c5, c4, c3, c2, c1, c0], r1:=0x0
            ymm3 = _mm256_cvtepi16_epi32(xmm4);                // r0=c0 r1=c1, r2=c2, r3=c3 are 4 signed-32bit data
            ymm7 = _mm256_abs_epi32(ymm3);                     // r0=abs(c0) r1=abs(c1), r2=abs(c2), r3=abs(c3)

            // qval = (aval * scale + offset) >> shift; 
            ymm6 = _mm256_mullo_epi32(ymm7, ymm10);            // r0=c0*qScale, r1=c1*qScale, r2=c2*qScale, r3=c3*qScale
            ymm2 = _mm256_add_epi32(ymm6, ymm11);              // r0=c0*scale+offset, r1=c1*scale+offset, ...
            ymm2 = _mm256_sra_epi32(ymm2, xmm12);              // r0=q0=(c0*scale+offset)>>shift, r1=(c1*scale+offset)>>shift, ...
            _mm256_storeu_si256((__m256i *)&qLevels[i], ymm2);

            // zero level cost = aLevel * aLevel * rdScale0
            ymm1 = _mm256_mullo_epi32(ymm7, ymm7);             // r0=c0*c0, r1=c1*c1, r2=c2*c2, r3=c3*c3
            ymm0 = _mm256_cvtepi32_epi64(mm128(ymm1));
            ymm0 = _mm256_mul_epi32(ymm0, ymm13);              // r0=c0*c0, r1=0, r2=c1*c1, r3=0
            _mm256_storeu_si256((__m256i *)&zlCosts[i], ymm0);

            ymm1 = _mm256_permute2x128_si256(ymm1, ymm1, 1);
            ymm0 = _mm256_cvtepi32_epi64(mm128(ymm1));
            ymm0 = _mm256_mul_epi32(ymm0, ymm13);              // r0=c2*c2, r1=0, r2=c3*c3, r3=0
            _mm256_storeu_si256((__m256i *)&zlCosts[i+4], ymm0);
        }
    }

    void H265_FASTCALL h265_QuantInv_16s_avx2(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale_, int offset_, int shift_)
    {
        assert((len & 0xf) == 0);
        __m256i offset = _mm256_set1_epi32(offset_);
        __m128i shift = _mm_cvtsi32_si128(shift_);

        if ((scale_ >> 15) == 0) {
            __m256i scale = _mm256_set1_epi16(scale_);
            for (Ipp32s i = 0; i < len; i += 16) {
                __m256i src = _mm256_loadu_si256((__m256i *)&pSrc[i]); // load 16-bit coefs
                __m256i lo = _mm256_mullo_epi16(src, scale); // *=scale
                __m256i hi = _mm256_mulhi_epi16(src, scale);
                __m256i half1 = _mm256_unpacklo_epi16(lo, hi);
                __m256i half2 = _mm256_unpackhi_epi16(lo, hi);
                half1 = _mm256_add_epi32(half1, offset); // +=offset
                half2 = _mm256_add_epi32(half2, offset);
                half1 = _mm256_sra_epi32(half1, shift); // >>=shift
                half2 = _mm256_sra_epi32(half2, shift);
                __m256i dst = _mm256_packs_epi32(half1, half2); // pack
                _mm256_storeu_si256((__m256i *)&pDst[i], dst); // store
            }
        } else {
            __m256i scale = _mm256_set1_epi32(scale_);
            for (Ipp32s i = 0; i < len; i += 8) {
                __m128i src16 = _mm_loadu_si128((__m128i *)&pSrc[i]); // load 16-bit coefs
                __m256i src32 = _mm256_cvtepi16_epi32(src16); // expand to 32 bits
                src32 = _mm256_mullo_epi32(src32, scale); // (coeff * scale + offset) >> shift;
                src32 = _mm256_add_epi32(src32, offset);
                src32 = _mm256_sra_epi32(src32, shift);
                src32 = _mm256_packs_epi32(src32, src32); // saturate to 16s
                src32 = _mm256_permute4x64_epi64(src32, 0xd8);
                _mm_storeu_si128((__m128i *)&pDst[i], mm128(src32)); // store
            }
        }
    }

} // end namespace MFX_HEVC_PP

#endif // #if defined (MFX_TARGET_OPTIMIZATION_PX) || (MFX_TARGET_OPTIMIZATION_SSE4) || (MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */
