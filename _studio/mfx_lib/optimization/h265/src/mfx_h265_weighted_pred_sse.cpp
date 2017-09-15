//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2017 Intel Corporation. All Rights Reserved.
//

/*
// HEVC weighted prediction, implemented with SSE packed multiply-add 
// 
// NOTES:
// - requires at least S-SSE3
// - assumes that input weighting parameters (w[], round[]) fit in 16 bits (looks like they are actually clipped to 8 bits - see section 7.4.7 in spec)
// - SSE implementation approximately 5x faster than C reference
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_ATOM) && defined(MFX_TARGET_OPTIMIZATION_ATOM) || \
    defined(MFX_MAKENAME_SSE4) && defined(MFX_TARGET_OPTIMIZATION_SSE4) || \
    defined(MFX_MAKENAME_SSSE3) && defined(MFX_TARGET_OPTIMIZATION_SSSE3)

#define Clip3(m_Min, m_Max, m_Value) ( (m_Value) < (m_Min) ? (m_Min) : ( (m_Value) > (m_Max) ? (m_Max) : (m_Value) ) )

#ifdef MFX_EMULATE_SSSE3
#include "mfx_ssse3_emulation.h"
#endif

namespace MFX_HEVC_PP
{

template <int bitDepth, typename DstCoeffsType>
static void h265_CopyWeighted_Kernel(Ipp16s* pSrc, Ipp16s* pSrcUV, DstCoeffsType* pDst, DstCoeffsType* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth_chroma)
{
    Ipp32s x, y;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    __m128i xmm_min, xmm_max;

    /* use pmaddwd with [w*src + round*1] for each 32-bit result */
    xmm1 = _mm_set1_epi16(1);
    xmm2 = _mm_set1_epi32( (round[0] << 16) | (w[0] & 0x0000ffff) );
    xmm3 = _mm_set1_epi32(o[0]);
    xmm4 = _mm_cvtsi32_si128(logWD[0]);

    xmm_min = _mm_setzero_si128();
    xmm_max = _mm_set1_epi16((1 << bitDepth) - 1);

    /* luma */
    for (y = 0; y < (Ipp32s)Height; y++) {
        for (x = 0; x < ((Ipp32s)Width - 3); x += 4) {
            xmm0 = _mm_loadl_epi64((__m128i *)(pSrc + x));      /* load 4 16-bit pixels */
            xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);              /* interleave with 1's */
            xmm0 = _mm_madd_epi16(xmm0, xmm2);                  /* w*src + round*1 --> 32-bit results */
            xmm0 = _mm_sra_epi32(xmm0, xmm4);                   /* >> logWD[0] */
            xmm0 = _mm_add_epi32(xmm0, xmm3);                   /* + o[0] */

            if (sizeof(DstCoeffsType) == 1) {
                /* clip to 8 bits, store 4 pixels */
                xmm0 = _mm_packs_epi32(xmm0, xmm0);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);
                *(int *)(pDst + x) = _mm_cvtsi128_si32(xmm0);
            } else {
                /* clip to range [0, 2^bitDepth), store 4 16-bit pixels */
                xmm0 = _mm_packs_epi32(xmm0, xmm0);
                xmm0 = _mm_max_epi16(xmm0, xmm_min);
                xmm0 = _mm_min_epi16(xmm0, xmm_max);
                _mm_storel_epi64((__m128i *)(pDst + x), xmm0);
            }
        }

        for ( ; x < (Ipp32s)Width; x++) {
            pDst[x] = (DstCoeffsType)Clip3(0, (1 << bitDepth) - 1, ((w[0] * pSrc[x] + round[0]) >> logWD[0]) + o[0]);
        }
        pSrc += SrcStrideY;
        pDst += DstStrideY;
    }

    /* chroma (interleaved U/V) */
    xmm2 = _mm_set1_epi32( (round[1] << 16) | (w[1] & 0x0000ffff) );
    xmm3 = _mm_set1_epi32(o[1]);

    xmm5 = _mm_set1_epi32( (round[2] << 16) | (w[2] & 0x0000ffff) );
    xmm6 = _mm_set1_epi32(o[2]);

    xmm4 = _mm_cvtsi32_si128(logWD[1]);
    xmm7 = _mm_cvtsi32_si128(logWD[2]);
    xmm4 = _mm_unpacklo_epi64(xmm4, xmm7);
    xmm7 = _mm_set1_epi16(1);

    xmm_max = _mm_set1_epi16((1 << bit_depth_chroma) - 1);

    Height >>= !isChroma422;

    for (y = 0; y < (Ipp32s)Height; y++) {
        for (x = 0; x < (Ipp32s)Width - 2*3; x += 8) {
            /* deinterleave U/V (necessary because final >> (logWD) may not be the same) then interleave with constant 1's */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrcUV + x));    /* load 8 16-bit pixels, interleaved UVUVUVUV */
            xmm0 = _mm_shufflelo_epi16(xmm0, 0xd8);             /* U0 U1 V0 V1 U2 V2 U3 V3 */
            xmm0 = _mm_shufflehi_epi16(xmm0, 0xd8);             /* U0 U1 V0 V1 U2 U3 V2 V3 */
            xmm0 = _mm_shuffle_epi32(xmm0, 0xd8);               /* U0 U1 U2 U3 V0 V1 V2 V3 */
            xmm1 = xmm0;
            xmm0 = _mm_unpacklo_epi16(xmm0, xmm7);              /* U0  1 U1  1 U2  1 U3  1 */
            xmm1 = _mm_unpackhi_epi16(xmm1, xmm7);              /* V0  1 V1  1 V2  1 V3  1 */

            /* process U */
            xmm0 = _mm_madd_epi16(xmm0, xmm2);                  /* w*src + round*1 --> 32-bit results */
            xmm0 = _mm_sra_epi32(xmm0, xmm4);                   /* >> logWD[1] */
            xmm0 = _mm_add_epi32(xmm0, xmm3);                   /* + o[1] */
            xmm4 = _mm_alignr_epi8(xmm4, xmm4, 8);              /* flip logWD[1] and logWD[2] (recycling register) */

            /* repeat for V */
            xmm1 = _mm_madd_epi16(xmm1, xmm5);
            xmm1 = _mm_sra_epi32(xmm1, xmm4);
            xmm1 = _mm_add_epi32(xmm1, xmm6);
            xmm4 = _mm_alignr_epi8(xmm4, xmm4, 8);

            if (sizeof(DstCoeffsType) == 1) {
                /* clip to 8 bits, interleave U/V, store 8 pixels */
                xmm0 = _mm_packs_epi32(xmm0, xmm0);
                xmm1 = _mm_packs_epi32(xmm1, xmm1);
                xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);
                _mm_storel_epi64((__m128i *)(pDstUV + x), xmm0);
            } else {
                /* clip to range [0, 2^bitDepth), interleave U/V, store 8 16-bit pixels */
                xmm0 = _mm_packs_epi32(xmm0, xmm0);
                xmm1 = _mm_packs_epi32(xmm1, xmm1);
                xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);
                xmm0 = _mm_max_epi16(xmm0, xmm_min);
                xmm0 = _mm_min_epi16(xmm0, xmm_max);
                _mm_storeu_si128((__m128i *)(pDstUV + x), xmm0);
            }
        }

        for ( ; x < (Ipp32s)Width; x += 2) {
            pDstUV[x + 0] = (DstCoeffsType)Clip3(0, (1 << bit_depth_chroma) - 1, ((w[1] * pSrcUV[x + 0] + round[1]) >> logWD[1]) + o[1]);
            pDstUV[x + 1] = (DstCoeffsType)Clip3(0, (1 << bit_depth_chroma) - 1, ((w[2] * pSrcUV[x + 1] + round[2]) >> logWD[2]) + o[2]);
        }
        pSrcUV += SrcStrideC;
        pDstUV += DstStrideC;
    }
}

void MAKE_NAME(h265_CopyWeighted_S16U8)(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round)
{
    h265_CopyWeighted_Kernel<8, Ipp8u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, 8);
}
    
void MAKE_NAME(h265_CopyWeighted_S16U16)(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma)
{
    if (bit_depth == 9)
        h265_CopyWeighted_Kernel< 9, Ipp16u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, bit_depth_chroma);
    else if (bit_depth == 10)
        h265_CopyWeighted_Kernel<10, Ipp16u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, bit_depth_chroma);
    else if (bit_depth == 8)
        h265_CopyWeighted_Kernel<8, Ipp16u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, bit_depth_chroma);
    else if (bit_depth == 11)
        h265_CopyWeighted_Kernel<11, Ipp16u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, bit_depth_chroma);
    else if (bit_depth == 12)
        h265_CopyWeighted_Kernel<12, Ipp16u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, bit_depth_chroma);

}

template <int bitDepth, typename DstCoeffsType>
static void h265_CopyWeightedBidi_Kernel(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, DstCoeffsType* pDst, DstCoeffsType* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth_chroma)
{
    Ipp32s x, y;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    __m128i xmm_min, xmm_max;

    /* use pmaddwd with [w0*src0 + w1*src1] for each 32-bit result */
    xmm2 = _mm_set1_epi32( (w1[0] << 16) | (w0[0] & 0x0000ffff) );
    xmm3 = _mm_set1_epi32(round[0]);
    xmm4 = _mm_cvtsi32_si128(logWD[0]);

    xmm_min = _mm_setzero_si128();
    xmm_max = _mm_set1_epi16((1 << bitDepth) - 1);

    /* luma */
    for (y = 0; y < (Ipp32s)Height; y++) {
        for (x = 0; x < ((Ipp32s)Width - 3); x += 4) {
            xmm0 = _mm_loadl_epi64((__m128i *)(pSrc0 + x));     /* load 4 16-bit pixels (src0) */
            xmm1 = _mm_loadl_epi64((__m128i *)(pSrc1 + x));     /* load 4 16-bit pixels (src1) */
            xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);              /* interleave src0, src1 */

            xmm0 = _mm_madd_epi16(xmm0, xmm2);                  /* w0*src0 + w1*src1 --> 32-bit results */
            xmm0 = _mm_add_epi32(xmm0, xmm3);                   /* + round[0] */
            xmm0 = _mm_sra_epi32(xmm0, xmm4);                   /* >> logWD[0] */

            if (sizeof(DstCoeffsType) == 1) {
                /* clip to 8 bits, store 4 pixels */
                xmm0 = _mm_packs_epi32(xmm0, xmm0);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);
                *(int *)(pDst + x) = _mm_cvtsi128_si32(xmm0);
            } else {
                /* clip to range [0, 2^bit_depth), store 4 16-bit pixels */
                xmm0 = _mm_packs_epi32(xmm0, xmm0);
                xmm0 = _mm_max_epi16(xmm0, xmm_min);
                xmm0 = _mm_min_epi16(xmm0, xmm_max);
                _mm_storel_epi64((__m128i *)(pDst + x), xmm0);
            }
        }

        for ( ; x < (Ipp32s)Width; x++) {
            pDst[x] = (DstCoeffsType)Clip3(0, (1 << bitDepth) - 1, (w0[0] * pSrc0[x] + w1[0] * pSrc1[x] + round[0]) >> logWD[0]);
        }
        pSrc0 += SrcStride0Y;
        pSrc1 += SrcStride1Y;
        pDst += DstStrideY;
    }

    xmm2 = _mm_set1_epi32( (w1[1] << 16) | (w0[1] & 0x0000ffff) );
    xmm3 = _mm_set1_epi32(round[1]);

    xmm5 = _mm_set1_epi32( (w1[2] << 16) | (w0[2] & 0x0000ffff) );
    xmm6 = _mm_set1_epi32(round[2]);

    xmm4 = _mm_cvtsi32_si128(logWD[1]);
    xmm7 = _mm_cvtsi32_si128(logWD[2]);
    xmm4 = _mm_unpacklo_epi64(xmm4, xmm7);

    xmm_max = _mm_set1_epi16((1 << bit_depth_chroma) - 1);

    Height >>= !isChroma422;

    for (y = 0; y < (Ipp32s)Height; y++) {
        for (x = 0; x < (Ipp32s)Width - 2*3; x += 8) {
            /* deinterleave U/V (necessary because final >> (logWD) may not be the same) then interleave src0 and src1 */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrcUV0 + x));   /* src0: load 8 16-bit pixels, interleaved UVUVUVUV */
            xmm1 = _mm_loadu_si128((__m128i *)(pSrcUV1 + x));   /* src1: load 8 16-bit pixels, interleaved UVUVUVUV */

            xmm0 = _mm_shufflelo_epi16(xmm0, 0xd8);             /* src0: U0 U1 V0 V1 U2 V2 U3 V3 */
            xmm0 = _mm_shufflehi_epi16(xmm0, 0xd8);             /* src0: U0 U1 V0 V1 U2 U3 V2 V3 */
            xmm0 = _mm_shuffle_epi32(xmm0, 0xd8);               /* src0: U0 U1 U2 U3 V0 V1 V2 V3 */
            xmm7 = xmm0;

            xmm1 = _mm_shufflelo_epi16(xmm1, 0xd8);             /* src1: U0 U1 V0 V1 U2 V2 U3 V3 */
            xmm1 = _mm_shufflehi_epi16(xmm1, 0xd8);             /* src1: U0 U1 V0 V1 U2 U3 V2 V3 */
            xmm1 = _mm_shuffle_epi32(xmm1, 0xd8);               /* src1: U0 U1 U2 U3 V0 V1 V2 V3 */

            xmm7 = _mm_unpackhi_epi16(xmm7, xmm1);              /* interleave src0, src1 (V) */ 
            xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);              /* interleave src0, src1 (U) */ 

            /* process U */
            xmm0 = _mm_madd_epi16(xmm0, xmm2);                  /* w0*src0 + w1*src1 --> 32-bit results */
            xmm0 = _mm_add_epi32(xmm0, xmm3);                   /* + round[1] */
            xmm0 = _mm_sra_epi32(xmm0, xmm4);                   /* >> logWD[1] */
            xmm4 = _mm_alignr_epi8(xmm4, xmm4, 8);              /* flip logWD[1] and logWD[2] (recycling register) */

            /* repeat for V */
            xmm7 = _mm_madd_epi16(xmm7, xmm5);
            xmm7 = _mm_add_epi32(xmm7, xmm6);
            xmm7 = _mm_sra_epi32(xmm7, xmm4);
            xmm4 = _mm_alignr_epi8(xmm4, xmm4, 8);

            if (sizeof(DstCoeffsType) == 1) {
                /* clip to 8 bits, interleave U/V, store 8 pixels */
                xmm0 = _mm_packs_epi32(xmm0, xmm0);
                xmm7 = _mm_packs_epi32(xmm7, xmm7);
                xmm0 = _mm_unpacklo_epi16(xmm0, xmm7);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);
                _mm_storel_epi64((__m128i *)(pDstUV + x), xmm0);
            } else {
                /* clip to range [0, 2^bit_depth), interleave U/V, store 8 16-bit pixels */
                xmm0 = _mm_packs_epi32(xmm0, xmm0);
                xmm7 = _mm_packs_epi32(xmm7, xmm7);
                xmm0 = _mm_unpacklo_epi16(xmm0, xmm7);
                xmm0 = _mm_max_epi16(xmm0, xmm_min);
                xmm0 = _mm_min_epi16(xmm0, xmm_max);
                _mm_storeu_si128((__m128i *)(pDstUV + x), xmm0);
            }
        }

        for ( ; x < (Ipp32s)Width; x += 2) {
            pDstUV[x + 0] = (DstCoeffsType)Clip3(0, (1 << bit_depth_chroma) - 1, (w0[1] * pSrcUV0[x + 0] + w1[1] * pSrcUV1[x + 0] + round[1]) >> logWD[1]);
            pDstUV[x + 1] = (DstCoeffsType)Clip3(0, (1 << bit_depth_chroma) - 1, (w0[2] * pSrcUV0[x + 1] + w1[2] * pSrcUV1[x + 1] + round[2]) >> logWD[2]);
        }
        pSrcUV0 += SrcStride0C;
        pSrcUV1 += SrcStride1C;
        pDstUV += DstStrideC;
    }
}

void MAKE_NAME(h265_CopyWeightedBidi_S16U8)(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round)
{
    h265_CopyWeightedBidi_Kernel< 8, Ipp8u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, 8);
}

void MAKE_NAME(h265_CopyWeightedBidi_S16U16)(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma)
{
    if (bit_depth == 9)
        h265_CopyWeightedBidi_Kernel< 9, Ipp16u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, bit_depth_chroma);
    else if (bit_depth == 10)
        h265_CopyWeightedBidi_Kernel<10, Ipp16u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, bit_depth_chroma);
    else if (bit_depth == 8)
        h265_CopyWeightedBidi_Kernel<8, Ipp16u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, bit_depth_chroma);
    else if (bit_depth == 11)
        h265_CopyWeightedBidi_Kernel<11, Ipp16u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, bit_depth_chroma);
    else if (bit_depth == 12)
        h265_CopyWeightedBidi_Kernel<12, Ipp16u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, bit_depth_chroma);
}

} // end namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
