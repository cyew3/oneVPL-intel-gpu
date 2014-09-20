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
// HEVC forward quantization, optimized with SSE
// 
// NOTES:
// - requires SSE4.1 (pmulld)
// - assumes that len is multiple of 4
// - assumes that (offset >> shift) == 0 (for psign: assume input 0 stays 0 after quantization)
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_ATOM) && defined(MFX_TARGET_OPTIMIZATION_ATOM) || \
    defined(MFX_MAKENAME_SSE4) && defined(MFX_TARGET_OPTIMIZATION_SSE4)

namespace MFX_HEVC_PP
{
    void H265_FASTCALL MAKE_NAME(h265_QuantFwd_16s)(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift)
    {
        __m128i xmm0, xmm1, xmm3, xmm4, xmm5;

        VM_ASSERT((len & 0x03) == 0);
        VM_ASSERT((offset >> shift) == 0);

        xmm3 = _mm_set1_epi32(scale);
        xmm4 = _mm_set1_epi32(offset);
        xmm5 = _mm_cvtsi32_si128(shift);

        for (Ipp32s i = 0; i < len; i += 4) {
            // load 16-bit coefs, expand to 32 bits
            xmm0 = _mm_loadl_epi64((__m128i *)&pSrc[i]);
            xmm0 = _mm_cvtepi16_epi32(xmm0);
            xmm1 = _mm_abs_epi32(xmm0);

            // qval = (aval * scale + offset) >> shift; 
            xmm1 = _mm_mullo_epi32(xmm1, xmm3);
            xmm1 = _mm_add_epi32(xmm1, xmm4);
            xmm1 = _mm_sra_epi32(xmm1, xmm5);

            // pDst = clip((qval ^ sign) - sign)
            xmm1 = _mm_sign_epi32(xmm1, xmm0);
            xmm1 = _mm_packs_epi32(xmm1, xmm1);
            _mm_storel_epi64((__m128i *)&pDst[i], xmm1);
        }
    } // void h265_QuantFwd_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scaleLevel, int scaleOffset, int scale)


    Ipp32s H265_FASTCALL MAKE_NAME(h265_QuantFwd_SBH_16s)(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift)
    {
        Ipp32s abs_sum = 0;
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
        
        VM_ASSERT((len & 0x03) == 0);
        VM_ASSERT((offset >> shift) == 0);

        xmm3 = _mm_set1_epi32(scale);
        xmm4 = _mm_set1_epi32(offset);
        xmm5 = _mm_cvtsi32_si128(shift);
        xmm6 = _mm_cvtsi32_si128(shift - 8);
        xmm7 = _mm_setzero_si128();

        for (Ipp32s i = 0; i < len; i += 4) {
            // load 16-bit coefs, expand to 32 bits
            xmm0 = _mm_loadl_epi64((__m128i *)&pSrc[i]);
            xmm0 = _mm_cvtepi16_epi32(xmm0);

            // qval = (aval * scale + offset) >> shift; 
            xmm1 = _mm_abs_epi32(xmm0);
            xmm1 = _mm_mullo_epi32(xmm1, xmm3);
            xmm2 = xmm1;                                 // save copy of aval*scale
            xmm1 = _mm_add_epi32(xmm1, xmm4);
            xmm1 = _mm_sra_epi32(xmm1, xmm5);            // xmm1 = qval

            // pDelta = (aval * scale - (qval << shift)) >> (shift-8)
            xmm1 = _mm_sll_epi32(xmm1, xmm5);            // qval <<= shift
            xmm2 = _mm_sub_epi32(xmm2, xmm1);            // aval*scale - (qval<<shift)
            xmm1 = _mm_sra_epi32(xmm1, xmm5);            // qval >>= shift (restore)
            xmm2 = _mm_sra_epi32(xmm2, xmm6);            // >> (shift - 8)
            xmm7 = _mm_add_epi32(xmm7, xmm1);            // abs_sum += qval

            // pDst = clip((qval ^ sign) - sign)
            xmm1 = _mm_sign_epi32(xmm1, xmm0);
            xmm1 = _mm_packs_epi32(xmm1, xmm1);

            _mm_storel_epi64((__m128i *)&pDst[i], xmm1);
            _mm_storeu_si128((__m128i *)&pDelta[i], xmm2);
        }

        // return overall abs_sum
        xmm7 = _mm_hadd_epi32(xmm7, xmm7);
        xmm7 = _mm_hadd_epi32(xmm7, xmm7);
        abs_sum = _mm_cvtsi128_si32(xmm7);

        return abs_sum;
    } // Ipp32s h265_QuantFwd_SBH_16s(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scaleLevel, int scaleOffset, int scale)

    void H265_FASTCALL MAKE_NAME(h265_Quant_zCost_16s)(const Ipp16s* pSrc, Ipp32u* qLevels, Ipp64s* zlCosts, Ipp32s len, Ipp32s qScale, Ipp32s qoffset, Ipp32s qbits, Ipp32s rdScale0)
    {
        int i;
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm6, xmm7, xmm10, xmm11, xmm12, xmm13;

        VM_ASSERT((len & 0x03) == 0);

        xmm10 = _mm_set1_epi32(qScale);
        xmm11 = _mm_set1_epi32(qoffset);
        xmm12 = _mm_cvtsi32_si128(qbits);
        xmm13 = _mm_setr_epi32 (rdScale0, 0, rdScale0, 0);

        for (i = 0; i < len; i += 4) 
        {
            // load 16-bit coefs, expand to 32 bits
            xmm4 = _mm_loadl_epi64((__m128i *)&pSrc[i]);    // r0:= [c3, c2, c1, c0], r1:=0x0
            xmm3 = _mm_cvtepi16_epi32(xmm4);                // r0=c0 r1=c1, r2=c2, r3=c3 are 4 signed-32bit data
            xmm7 = _mm_abs_epi32(xmm3);                     // r0=abs(c0) r1=abs(c1), r2=abs(c2), r3=abs(c3)

            // qval = (aval * scale + offset) >> shift; 
            xmm6 = _mm_mullo_epi32(xmm7, xmm10);            // r0=c0*qScale, r1=c1*qScale, r2=c2*qScale, r3=c3*qScale
            xmm2 = _mm_add_epi32(xmm6, xmm11);              // r0=c0*scale+offset, r1=c1*scale+offset, ...
            xmm2 = _mm_sra_epi32(xmm2, xmm12);              // r0=q0=(c0*scale+offset)>>shift, r1=(c1*scale+offset)>>shift, ...
            _mm_storeu_si128((__m128i *)&qLevels[i], xmm2);

            // zero level cost = aLevel * aLevel * rdScale0
            xmm1 = _mm_mullo_epi32(xmm7, xmm7);             // r0=c0*c0, r1=c1*c1, r2=c2*c2, r3=c3*c3
            xmm0 = _mm_cvtepi32_epi64(xmm1);                // r0=c0*c0, r1=0, r2=c1*c1, r3=0
            xmm0 = _mm_mul_epi32(xmm0, xmm13); 
            _mm_storeu_si128((__m128i *)&zlCosts[i], xmm0);

            xmm1 = _mm_srli_si128 (xmm1, 8);
            xmm4 = _mm_cvtepi32_epi64(xmm1);                // r0=c2*c2, r1=0, r2=c3*c3, r3=0
            xmm1 = _mm_mul_epi32(xmm4, xmm13); 
            _mm_storeu_si128((__m128i *)&zlCosts[i+2], xmm1);
        }
    }

} // end namespace MFX_HEVC_PP

#endif // #if defined (MFX_TARGET_OPTIMIZATION_PX) || (MFX_TARGET_OPTIMIZATION_SSE4) || (MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */
