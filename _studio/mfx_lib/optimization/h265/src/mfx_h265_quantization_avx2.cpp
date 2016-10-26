//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

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
    namespace QuantFwdDetails {
        template <int len> inline Ipp8u Impl(const Ipp16s* pSrc, Ipp16s* pDst, int scale_, int offset_, int shift_)
        {
            __m256i scale  = _mm256_set1_epi16(scale_);
            __m256i offset = _mm256_set1_epi32(offset_);
            __m128i shift  = _mm_cvtsi32_si128(shift_);
            __m256i cbf = _mm256_setzero_si256();
#ifdef __INTEL_COMPILER
            //#pragma unroll(4)
#endif
            for (Ipp32s i = 0; i < len; i += 16, pSrc += 16, pDst += 16) {
                __m256i src = _mm256_load_si256((__m256i *)pSrc); // load 16-bit coefs
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
                cbf = _mm256_or_si256(cbf, dst);
                _mm256_store_si256((__m256i *)pDst, dst); // store
            }
            return (Ipp8u)!_mm256_testz_si256(cbf, cbf);
        }
    }

    Ipp8u H265_FASTCALL MAKE_NAME(h265_QuantFwd_16s)(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale_, int offset_, int shift_)
    {
        assert((offset_ >> shift_) == 0);
        switch (len) {
        case 32*32: return QuantFwdDetails::Impl<32*32>(pSrc, pDst, scale_, offset_, shift_);
        case 16*16: return QuantFwdDetails::Impl<16*16>(pSrc, pDst, scale_, offset_, shift_);
        case 8*8:   return QuantFwdDetails::Impl<8*8>(pSrc, pDst, scale_, offset_, shift_);
        case 4*4:   return QuantFwdDetails::Impl<4*4>(pSrc, pDst, scale_, offset_, shift_);
        default: assert(0); return 0;
        }
    }


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

    namespace QuantzCostDetails {
        template <Ipp32s len>
        Ipp64s H265_FASTCALL Impl(const Ipp16s* pSrc, Ipp16u* qLevels, Ipp64s* zlCosts, Ipp32s qScale, Ipp32s qoffset, Ipp32s qbits, Ipp32s rdScale0)
        {
            __m256i scale = _mm256_set1_epi16(qScale);
            __m256i offset = _mm256_set1_epi32(qoffset);
            __m128i shift = _mm_cvtsi32_si128(qbits);
            __m256i rdscale = _mm256_setr_epi32(rdScale0, 0, rdScale0, 0, rdScale0, 0, rdScale0, 0);
            __m256i src, src1, src2, lo, hi, dst1, dst2;
            __m256i totalZeroCost = _mm256_setzero_si256();

#ifdef __INTEL_COMPILER
            #pragma unroll(4)
#endif
            for (Ipp32s i = 0; i < len; i += 16) {
                src = _mm256_load_si256((__m256i *)(pSrc+i));
                src = _mm256_abs_epi16(src);
                lo = _mm256_mullo_epi16(src, scale);
                hi = _mm256_mulhi_epi16(src, scale);
                dst1 = _mm256_unpacklo_epi16(lo, hi);
                dst2 = _mm256_unpackhi_epi16(lo, hi);
                dst1 = _mm256_add_epi32(dst1, offset);
                dst1 = _mm256_sra_epi32(dst1, shift);
                dst2 = _mm256_add_epi32(dst2, offset);
                dst2 = _mm256_sra_epi32(dst2, shift);
                dst1 = _mm256_packus_epi32(dst1, dst2);
                _mm256_store_si256((__m256i *)(qLevels+i), dst1);

                lo = _mm256_mullo_epi16(src, src);
                hi = _mm256_mulhi_epi16(src, src);
                src1 = _mm256_unpacklo_epi16(lo, hi);
                src2 = _mm256_unpackhi_epi16(lo, hi);

                dst1 = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(src1));
                dst1 = _mm256_mul_epi32(dst1, rdscale);
                totalZeroCost = _mm256_add_epi64(totalZeroCost, dst1);
                _mm256_store_si256((__m256i *)(zlCosts+i), dst1);
                dst1 = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(src2));
                dst1 = _mm256_mul_epi32(dst1, rdscale);
                totalZeroCost = _mm256_add_epi64(totalZeroCost, dst1);
                _mm256_store_si256((__m256i *)(zlCosts+i+4), dst1);
                src1 = _mm256_permute2x128_si256(src1, src1, 1);
                dst1 = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(src1));
                dst1 = _mm256_mul_epi32(dst1, rdscale);
                totalZeroCost = _mm256_add_epi64(totalZeroCost, dst1);
                _mm256_store_si256((__m256i *)(zlCosts+i+8), dst1);
                src2 = _mm256_permute2x128_si256(src2, src2, 1);
                dst1 = _mm256_cvtepu32_epi64(_mm256_castsi256_si128(src2));
                dst1 = _mm256_mul_epi32(dst1, rdscale);
                totalZeroCost = _mm256_add_epi64(totalZeroCost, dst1);
                _mm256_store_si256((__m256i *)(zlCosts+i+12), dst1);
            }
            __m128i subtot = _mm_add_epi64(_mm256_castsi256_si128(totalZeroCost), _mm256_extracti128_si256(totalZeroCost, 1));
            return _mm_extract_epi64(subtot, 0) + _mm_extract_epi64(subtot, 1);
        }
    };

    Ipp64s H265_FASTCALL MAKE_NAME(h265_Quant_zCost_16s)(const Ipp16s* pSrc, Ipp16u* qLevels, Ipp64s* zlCosts, Ipp32s len, Ipp32s qScale, Ipp32s qoffset, Ipp32s qbits, Ipp32s rdScale0)
    {
        switch (len) {
        case 32*32: return QuantzCostDetails::Impl<32*32>(pSrc, qLevels, zlCosts, qScale, qoffset, qbits, rdScale0);
        case 16*16: return QuantzCostDetails::Impl<16*16>(pSrc, qLevels, zlCosts, qScale, qoffset, qbits, rdScale0);
        case 8*8:   return QuantzCostDetails::Impl<8*8>(pSrc, qLevels, zlCosts, qScale, qoffset, qbits, rdScale0);
        case 4*4:   return QuantzCostDetails::Impl<4*4>(pSrc, qLevels, zlCosts, qScale, qoffset, qbits, rdScale0);
        default: assert(0); return 0;
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
