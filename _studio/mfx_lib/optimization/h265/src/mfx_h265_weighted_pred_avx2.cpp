/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/

/*
// HEVC weighted prediction, implemented with AVX2 packed multiply-add 
// 
// NOTES:
// - requires AVX2
// - assumes that input weighting parameters (w[], round[]) fit in 16 bits (looks like they are actually clipped to 8 bits - see section 7.4.7 in spec)
// - assumes that height is multiple of 2 (processes 2 rows at a time for AVX2)
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_AVX2) && defined(MFX_TARGET_OPTIMIZATION_AVX2)

#define Clip3(m_Min, m_Max, m_Value) ( (m_Value) < (m_Min) ? (m_Min) : ( (m_Value) > (m_Max) ? (m_Max) : (m_Value) ) )

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

namespace MFX_HEVC_PP
{

void MAKE_NAME(h265_CopyWeighted_S16U8)(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round)
{
    Ipp32s x, y, xt;
    __m128i xmm_shift;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    VM_ASSERT((Height & 0x01) == 0);
    
    _mm256_zeroupper();

    /* use pmaddwd with [w*src + round*1] for each 32-bit result */
    ymm1 = _mm256_set1_epi16(1);
    ymm2 = _mm256_set1_epi32( (round[0] << 16) | (w[0] & 0x0000ffff) );
    ymm3 = _mm256_set1_epi32(o[0]);
    xmm_shift = _mm_cvtsi32_si128(logWD[0]);

    /* luma */
    xt = Width & (~0x03);
    for (y = 0; y < (Ipp32s)Height; y += 2) {
        for (x = 0; x < xt; x += 4) {
            ymm0 = mm256(_mm_loadl_epi64((__m128i *)(pSrc + 0*SrcStrideY + x)));      /* row 0: load 4 16-bit pixels */
            ymm7 = mm256(_mm_loadl_epi64((__m128i *)(pSrc + 1*SrcStrideY + x)));      /* row 1: load 4 16-bit pixels */
            ymm0 = _mm256_permute2x128_si256(ymm0, ymm7, 0x20);

            ymm0 = _mm256_unpacklo_epi16(ymm0, ymm1);              /* interleave with 1's */
            ymm0 = _mm256_madd_epi16(ymm0, ymm2);                  /* w*src + round*1 --> 32-bit results */
            ymm0 = _mm256_sra_epi32(ymm0, xmm_shift);              /* >> logWD[0] */
            ymm0 = _mm256_add_epi32(ymm0, ymm3);                   /* + o[0] */

            /* clip to 8 bits, store 4 pixels */
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);
            ymm0 = _mm256_packus_epi16(ymm0, ymm0);
            ymm7 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);

            *(int *)(pDst + 0*DstStrideY + x) = _mm_cvtsi128_si32(mm128(ymm0));
            *(int *)(pDst + 1*DstStrideY + x) = _mm_cvtsi128_si32(mm128(ymm7));
        }
        pSrc += 2*SrcStrideY;
        pDst += 2*DstStrideY;
    }

    /* handle tails */
    if (xt < (Ipp32s)Width) {
        pSrc -= Height*SrcStrideY;
        pDst -= Height*DstStrideY;
        for (y = 0; y < (Ipp32s)Height; y++) {
            for (x = xt; x < (Ipp32s)Width; x++) {
                pDst[x] = (Ipp8u)Clip3(0, 255, ((w[0] * pSrc[x] + round[0]) >> logWD[0]) + o[0]);
            }
            pSrc += SrcStrideY;
            pDst += DstStrideY;
        }
    }

    /* chroma (interleaved U/V) */
    ymm2 = _mm256_set1_epi32( (round[1] << 16) | (w[1] & 0x0000ffff) );
    ymm3 = _mm256_set1_epi32(o[1]);

    ymm5 = _mm256_set1_epi32( (round[2] << 16) | (w[2] & 0x0000ffff) );
    ymm6 = _mm256_set1_epi32(o[2]);

    xmm_shift = _mm_unpacklo_epi64(_mm_cvtsi32_si128(logWD[1]), _mm_cvtsi32_si128(logWD[2]));
    ymm7 = _mm256_set1_epi16(1);

    xt = Width & (~0x07);
    for (y = 0; y < (Ipp32s)Height / 2; y += 2) {
        for (x = 0; x < xt; x += 8) {
            /* deinterleave U/V (necessary because final >> (logWD) may not be the same) then interleave with constant 1's */
            ymm0 = mm256(_mm_loadu_si128((__m128i *)(pSrcUV + 0*SrcStrideC + x)));      /* row 0: load 8 16-bit pixels, interleaved UVUVUVUV */
            ymm1 = mm256(_mm_loadu_si128((__m128i *)(pSrcUV + 1*SrcStrideC + x)));      /* row 1: load 8 16-bit pixels, interleaved UVUVUVUV */
            ymm0 = _mm256_permute2x128_si256(ymm0, ymm1, 0x20);

            ymm0 = _mm256_shufflelo_epi16(ymm0, 0xd8);             /* U0 U1 V0 V1 U2 V2 U3 V3 */
            ymm0 = _mm256_shufflehi_epi16(ymm0, 0xd8);             /* U0 U1 V0 V1 U2 U3 V2 V3 */
            ymm0 = _mm256_shuffle_epi32(ymm0, 0xd8);               /* U0 U1 U2 U3 V0 V1 V2 V3 */
            ymm1 = ymm0;
            ymm0 = _mm256_unpacklo_epi16(ymm0, ymm7);              /* U0  1 U1  1 U2  1 U3  1 */
            ymm1 = _mm256_unpackhi_epi16(ymm1, ymm7);              /* V0  1 V1  1 V2  1 V3  1 */

            /* process U */
            ymm0 = _mm256_madd_epi16(ymm0, ymm2);                  /* w*src + round*1 --> 32-bit results */
            ymm0 = _mm256_sra_epi32(ymm0, xmm_shift);              /* >> logWD[1] */
            ymm0 = _mm256_add_epi32(ymm0, ymm3);                   /* + o[1] */
            xmm_shift = _mm_alignr_epi8(xmm_shift, xmm_shift, 8);  /* flip logWD[1] and logWD[2] (recycling register) */

            /* repeat for V */
            ymm1 = _mm256_madd_epi16(ymm1, ymm5);
            ymm1 = _mm256_sra_epi32(ymm1, xmm_shift);
            ymm1 = _mm256_add_epi32(ymm1, ymm6);
            xmm_shift = _mm_alignr_epi8(xmm_shift, xmm_shift, 8);

            /* clip to 8 bits, interleave U/V, store 8 pixels */
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);
            ymm1 = _mm256_packs_epi32(ymm1, ymm1);
            ymm0 = _mm256_unpacklo_epi16(ymm0, ymm1);
            ymm0 = _mm256_packus_epi16(ymm0, ymm0);
            ymm1 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);

            _mm_storel_epi64((__m128i *)(pDstUV + 0*DstStrideC + x), mm128(ymm0));
            _mm_storel_epi64((__m128i *)(pDstUV + 1*DstStrideC + x), mm128(ymm1));
        }
        pSrcUV += 2*SrcStrideC;
        pDstUV += 2*DstStrideC;
    }

    /* handle tails */
    if (xt < (Ipp32s)Width) {
        pSrcUV -= (Height / 2)*SrcStrideC;
        pDstUV -= (Height / 2)*DstStrideC;
        for (y = 0; y < (Ipp32s)Height / 2; y++) {
            for (x = xt; x < (Ipp32s)Width; x += 2) {
                pDstUV[x + 0] = (Ipp8u)Clip3(0, 255, ((w[1] * pSrcUV[x + 0] + round[1]) >> logWD[1]) + o[1]);
                pDstUV[x + 1] = (Ipp8u)Clip3(0, 255, ((w[2] * pSrcUV[x + 1] + round[2]) >> logWD[2]) + o[2]);
            }
            pSrcUV += SrcStrideC;
            pDstUV += DstStrideC;
        }
    }
}

void MAKE_NAME(h265_CopyWeightedBidi_S16U8)(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round)
{
    Ipp32s x, y, xt;
    __m128i xmm_shift;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    VM_ASSERT((Height & 0x01) == 0);

    _mm256_zeroupper();

    /* use pmaddwd with [w0*src0 + w1*src1] for each 32-bit result */
    ymm2 = _mm256_set1_epi32( (w1[0] << 16) | (w0[0] & 0x0000ffff) );
    ymm3 = _mm256_set1_epi32(round[0]);
    xmm_shift = _mm_cvtsi32_si128(logWD[0]);

    /* luma */
    xt = Width & (~0x03);
    for (y = 0; y < (Ipp32s)Height; y += 2) {
        for (x = 0; x < xt; x += 4) {
            ymm0 = mm256(_mm_loadl_epi64((__m128i *)(pSrc0 + 0*SrcStride0Y + x)));      /* row 0: load 4 16-bit pixels (src0) */
            ymm7 = mm256(_mm_loadl_epi64((__m128i *)(pSrc0 + 1*SrcStride0Y + x)));      /* row 1: load 4 16-bit pixels (src0) */
            ymm0 = _mm256_permute2x128_si256(ymm0, ymm7, 0x20);

            ymm1 = mm256(_mm_loadl_epi64((__m128i *)(pSrc1 + 0*SrcStride1Y + x)));      /* row 0: load 4 16-bit pixels (src1) */
            ymm7 = mm256(_mm_loadl_epi64((__m128i *)(pSrc1 + 1*SrcStride1Y + x)));      /* row 1: load 4 16-bit pixels (src1) */
            ymm1 = _mm256_permute2x128_si256(ymm1, ymm7, 0x20);

            ymm0 = _mm256_unpacklo_epi16(ymm0, ymm1);              /* interleave src0, src1 */

            ymm0 = _mm256_madd_epi16(ymm0, ymm2);                  /* w0*src0 + w1*src1 --> 32-bit results */
            ymm0 = _mm256_add_epi32(ymm0, ymm3);                   /* + round[0] */
            ymm0 = _mm256_sra_epi32(ymm0, xmm_shift);              /* >> logWD[0] */

            /* clip to 8 bits, store 4 pixels */
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);
            ymm0 = _mm256_packus_epi16(ymm0, ymm0);
            ymm7 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);

            *(int *)(pDst + 0*DstStrideY + x) = _mm_cvtsi128_si32(mm128(ymm0));
            *(int *)(pDst + 1*DstStrideY + x) = _mm_cvtsi128_si32(mm128(ymm7));
        }
        pSrc0 += 2*SrcStride0Y;
        pSrc1 += 2*SrcStride1Y;
        pDst  += 2*DstStrideY;
    }

    /* handle tails */
    if (xt < (Ipp32s)Width) {
        pSrc0 -= Height*SrcStride0Y;
        pSrc1 -= Height*SrcStride1Y;
        pDst  -= Height*DstStrideY;
        for (y = 0; y < (Ipp32s)Height; y++) {
            for (x = xt; x < (Ipp32s)Width; x++) {
                pDst[x] = (Ipp8u)Clip3(0, 255, (w0[0] * pSrc0[x] + w1[0] * pSrc1[x] + round[0]) >> logWD[0]);
            }
            pSrc0 += SrcStride0Y;
            pSrc1 += SrcStride1Y;
            pDst  += DstStrideY;
        }
    }

    ymm2 = _mm256_set1_epi32( (w1[1] << 16) | (w0[1] & 0x0000ffff) );
    ymm3 = _mm256_set1_epi32(round[1]);

    ymm5 = _mm256_set1_epi32( (w1[2] << 16) | (w0[2] & 0x0000ffff) );
    ymm6 = _mm256_set1_epi32(round[2]);

    xmm_shift = _mm_unpacklo_epi64(_mm_cvtsi32_si128(logWD[1]), _mm_cvtsi32_si128(logWD[2]));

    xt = Width & (~0x07);
    for (y = 0; y < (Ipp32s)Height / 2; y += 2) {
        for (x = 0; x < xt; x += 8) {
            /* deinterleave U/V (necessary because final >> (logWD) may not be the same) then interleave src0 and src1 */
            ymm0 = mm256(_mm_loadu_si128((__m128i *)(pSrcUV0 + 0*SrcStride0C + x)));   /* src0: load 8 16-bit pixels, interleaved UVUVUVUV (row 0) */
            ymm7 = mm256(_mm_loadu_si128((__m128i *)(pSrcUV0 + 1*SrcStride0C + x)));   /* src0: load 8 16-bit pixels, interleaved UVUVUVUV (row 1) */
            ymm0 = _mm256_permute2x128_si256(ymm0, ymm7, 0x20);

            ymm1 = mm256(_mm_loadu_si128((__m128i *)(pSrcUV1 + 0*SrcStride1C + x)));   /* src1: load 8 16-bit pixels, interleaved UVUVUVUV (row 0) */
            ymm7 = mm256(_mm_loadu_si128((__m128i *)(pSrcUV1 + 1*SrcStride1C + x)));   /* src1: load 8 16-bit pixels, interleaved UVUVUVUV (row 1) */
            ymm1 = _mm256_permute2x128_si256(ymm1, ymm7, 0x20);

            ymm0 = _mm256_shufflelo_epi16(ymm0, 0xd8);             /* src0: U0 U1 V0 V1 U2 V2 U3 V3 */
            ymm0 = _mm256_shufflehi_epi16(ymm0, 0xd8);             /* src0: U0 U1 V0 V1 U2 U3 V2 V3 */
            ymm0 = _mm256_shuffle_epi32(ymm0, 0xd8);               /* src0: U0 U1 U2 U3 V0 V1 V2 V3 */
            ymm7 = ymm0;

            ymm1 = _mm256_shufflelo_epi16(ymm1, 0xd8);             /* src1: U0 U1 V0 V1 U2 V2 U3 V3 */
            ymm1 = _mm256_shufflehi_epi16(ymm1, 0xd8);             /* src1: U0 U1 V0 V1 U2 U3 V2 V3 */
            ymm1 = _mm256_shuffle_epi32(ymm1, 0xd8);               /* src1: U0 U1 U2 U3 V0 V1 V2 V3 */

            ymm7 = _mm256_unpackhi_epi16(ymm7, ymm1);              /* interleave src0, src1 (V) */ 
            ymm0 = _mm256_unpacklo_epi16(ymm0, ymm1);              /* interleave src0, src1 (U) */ 

            /* process U */
            ymm0 = _mm256_madd_epi16(ymm0, ymm2);                  /* w0*src0 + w1*src1 --> 32-bit results */
            ymm0 = _mm256_add_epi32(ymm0, ymm3);                   /* + round[1] */
            ymm0 = _mm256_sra_epi32(ymm0, xmm_shift);              /* >> logWD[1] */
            xmm_shift = _mm_alignr_epi8(xmm_shift, xmm_shift, 8);  /* flip logWD[1] and logWD[2] (recycling register) */

            /* repeat for V */
            ymm7 = _mm256_madd_epi16(ymm7, ymm5);
            ymm7 = _mm256_add_epi32(ymm7, ymm6);
            ymm7 = _mm256_sra_epi32(ymm7, xmm_shift);
            xmm_shift = _mm_alignr_epi8(xmm_shift, xmm_shift, 8);

            /* clip to 8 bits, interleave U/V, store 8 pixels */
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);
            ymm7 = _mm256_packs_epi32(ymm7, ymm7);
            ymm0 = _mm256_unpacklo_epi16(ymm0, ymm7);
            ymm0 = _mm256_packus_epi16(ymm0, ymm0);
            ymm7 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);

            _mm_storel_epi64((__m128i *)(pDstUV + 0*DstStrideC + x), mm128(ymm0));
            _mm_storel_epi64((__m128i *)(pDstUV + 1*DstStrideC + x), mm128(ymm7));
        }
        pSrcUV0 += 2*SrcStride0C;
        pSrcUV1 += 2*SrcStride1C;
        pDstUV  += 2*DstStrideC;
    }

    /* handle tails */
    if (xt < (Ipp32s)Width) {
        pSrcUV0 -= (Height/2)*SrcStride0C;
        pSrcUV1 -= (Height/2)*SrcStride1C;
        pDstUV  -= (Height/2)*DstStrideC;
        for (y = 0; y < (Ipp32s)Height / 2; y++) {
            for (x = xt; x < (Ipp32s)Width; x += 2) {
                pDstUV[x + 0] = (Ipp8u)Clip3(0, 255, (w0[1] * pSrcUV0[x + 0] + w1[1] * pSrcUV1[x + 0] + round[1]) >> logWD[1]);
                pDstUV[x + 1] = (Ipp8u)Clip3(0, 255, (w0[2] * pSrcUV0[x + 1] + w1[2] * pSrcUV1[x + 1] + round[2]) >> logWD[2]);
            }
            pSrcUV0 += SrcStride0C;
            pSrcUV1 += SrcStride1C;
            pDstUV  += DstStrideC;
        }
    }
}

} // end namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) || (defined(MFX_TARGET_OPTIMIZATION_SSSE3) && defined (MFX_EMULATE_SSSE3)) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
