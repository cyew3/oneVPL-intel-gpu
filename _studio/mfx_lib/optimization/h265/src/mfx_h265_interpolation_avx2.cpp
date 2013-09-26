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
// HEVC interpolation filters, implemented with AVX2 (256-bit integer instructions)
// 
// NOTES:
// - requires AVX2
// - to avoid SSE/AVX transition penalties, ensure that AVX2 code is wrapped with _mm256_zeroupper() 
//     (include explicitly on function entry, ICL should automatically add before function return)
// - functions assume that width = multiple of 4 (LUMA) or multiple of 2 (CHROMA), height > 0
// - input data is read in chunks of up to 24 bytes regardless of width, so assume it's safe to read up to 24 bytes beyond right edge of block 
//     (data not used, just must be allocated memory - pad inbuf as needed)
// - best intrinsic performance requires up to date Intel compiler (check compiler output to make sure no unnecessary stack spills!)
// - if compiler register allocates poorly, may need to turn /Qipo OFF for this file
// - picture/buffer averaging is done as separate pass after filtering to allow simpler filter kernels 
//     (pays off in performance by reducing branches for the most common modes, also makes code easier to read/change)
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 

#include <immintrin.h>
#include <assert.h>

/* workaround for compiler bug (see h265_tr_quant_opt.cpp) */
#ifdef NDEBUG 
  #define MM_LOAD_EPI64(x) (*(__m128i*)(x))
#else
  #define MM_LOAD_EPI64(x) _mm_loadl_epi64( (__m128i*)(x))
#endif

namespace MFX_HEVC_PP
{

enum EnumPlane
{
    TEXT_LUMA = 0,
    TEXT_CHROMA,
    TEXT_CHROMA_U,
    TEXT_CHROMA_V,
};

/* interleaved luma interp coefficients, 8-bit, for offsets 1/4, 2/4, 3/4 */
ALIGN_DECL(32) static const signed char filtTabLuma_S8[3*4][32] = {
    {  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58 },
    {  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5 },
    {   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0 },

    {  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40 },
    {  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11 },
    {   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1 },

    {   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1 },
    {  -5,   17, -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17 },
    {  58,  -10, 58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10 },
    {   4,   -1,  4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1 },

};

/* interleaved chroma interp coefficients, 8-bit, for offsets 1/8, 2/8, ... 7/8 */
ALIGN_DECL(32) static const signed char filtTabChroma_S8[7*2][32] = {
    {  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58 }, 
    {  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2 },

    {  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54 },
    {  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2 },

    {  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46 },
    {  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4 },

    {  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36 },
    {  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4 },

    {  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28 },
    {  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6 },

    {  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16 },
    {  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4 },

    {  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10 },
    {  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2 },
};

/* interleaved luma interp coefficients, 16-bit, for offsets 1/4, 2/4, 3/4 */
ALIGN_DECL(32) static const short filtTabLuma_S16[3*4][16] = {
    {  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58 },
    {  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5 },
    {   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0 },

    {  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40 },
    {  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11 },
    {   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1 },

    {   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1 },
    {  -5,   17, -5,  17,  -5,  17,  -5,  17,  -5,   17, -5,  17,  -5,  17,  -5,  17 },
    {  58,  -10, 58, -10,  58, -10,  58, -10,  58,  -10, 58, -10,  58, -10,  58, -10 },
    {   4,   -1,  4,  -1,   4,  -1,   4,  -1,   4,   -1,  4,  -1,   4,  -1,   4,  -1 },

};

/* interleaved chroma interp coefficients, 16-bit, for offsets 1/8, 2/8, ... 7/8 */
ALIGN_DECL(32) static const short filtTabChroma_S16[7*2][16] = {
    {  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58 },
    {  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2 },

    {  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54 },
    {  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2 },

    {  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46 },
    {  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4 },

    {  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36 },
    {  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4 },

    {  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28 },
    {  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6 },

    {  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16 },
    {  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4 },

    {  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10 },
    {  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2 },
};

/* pshufb table for luma, 8-bit horizontal filtering, identical shuffle within each 128-bit lane */
ALIGN_DECL(32) static const signed char shufTabPlane[4][32] = {
    {  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8 },
    {  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10 },
    {  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12,  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12 },
    {  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14 },
};

/* pshufb table for chroma, 8-bit horizontal filtering, identical shuffle within each 128-bit lane */
ALIGN_DECL(32) static const signed char shufTabIntUV[2][32] = {
    {  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7,  9,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7,  9 },
    {  4,  6,  5,  7,  6,  8,  7,  9,  8, 10,  9, 11, 10, 12, 11, 13,  4,  6,  5,  7,  6,  8,  7,  9,  8, 10,  9, 11, 10, 12, 11, 13 },
};

/* luma, horizontal, 8-bit input, 16-bit output
 * AVX2 path used when width = multiple of 16
 */
void MAKE_NAME(h265_InterpLuma_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;

    _mm256_zeroupper();

    if ((width & 0x0f) == 0) {
        __m256i ymm0, ymm1, ymm2, ymm3;
        __m256i ymm_offset;
        __m128i xmm_shift;

        coeffs = filtTabLuma_S8[4 * (tab_index-1)];

        ymm_offset = _mm256_set1_epi16(offset);
        xmm_shift = _mm_cvtsi32_si128(shift);

        /* calculate 16 outputs per inner loop, working horizontally */
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            col = width;

            while (col > 0) {
                /* load 24 8-bit pixels, permute into ymm lanes as [0-7 | 8-15 | 8-15 | 16-23] */
                ymm3 = _mm256_permute4x64_epi64(*(__m256i *)pSrc, 0x94);

                /* interleave pixels */
                ymm0 = _mm256_shuffle_epi8(ymm3, *(__m256i *)(shufTabPlane[0]));
                ymm1 = _mm256_shuffle_epi8(ymm3, *(__m256i *)(shufTabPlane[1]));
                ymm2 = _mm256_shuffle_epi8(ymm3, *(__m256i *)(shufTabPlane[2]));
                ymm3 = _mm256_shuffle_epi8(ymm3, *(__m256i *)(shufTabPlane[3]));

                /* packed (8*8 + 8*8) -> 16 */
                ymm0 = _mm256_maddubs_epi16(ymm0, *(__m256i *)(coeffs +  0));    /* coefs 0,1 */
                ymm1 = _mm256_maddubs_epi16(ymm1, *(__m256i *)(coeffs + 32));    /* coefs 2,3 */
                ymm2 = _mm256_maddubs_epi16(ymm2, *(__m256i *)(coeffs + 64));    /* coefs 4,5 */
                ymm3 = _mm256_maddubs_epi16(ymm3, *(__m256i *)(coeffs + 96));    /* coefs 6,7 */

                /* sum intermediate values, add offset, shift off fraction bits */
                ymm0 = _mm256_add_epi16(ymm0, ymm1);
                ymm0 = _mm256_add_epi16(ymm0, ymm2);
                ymm0 = _mm256_add_epi16(ymm0, ymm3);
                ymm0 = _mm256_add_epi16(ymm0, ymm_offset);
                ymm0 = _mm256_sra_epi16(ymm0, xmm_shift);

                /* store 16 16-bit words */
                _mm256_storeu_si256((__m256i *)pDst, ymm0);
                pSrc += 16;
                pDst += 16;
                col -= 16;
            }

            pSrcRef += srcPitch;
            pDstRef += dstPitch;
        } while (--height);
    } else {
        __m128i xmm0, xmm1, xmm2, xmm3;
        __m128i xmm_offset, xmm_shift;

        coeffs = filtTabLuma_S8[4 * (tab_index-1)];

        xmm_offset = _mm_set1_epi16(offset);
        xmm_shift = _mm_cvtsi32_si128(shift);

        /* calculate 8 outputs per inner loop, working horizontally */
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            col = width;

            while (col > 0) {
                /* load 16 8-bit pixels */
                xmm0 = _mm_loadu_si128((__m128i *)pSrc);
                xmm1 = xmm0;
                xmm2 = xmm0;
                xmm3 = xmm0;

                /* interleave pixels */
                xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabPlane[0]));
                xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabPlane[1]));
                xmm2 = _mm_shuffle_epi8(xmm2, *(__m128i *)(shufTabPlane[2]));
                xmm3 = _mm_shuffle_epi8(xmm3, *(__m128i *)(shufTabPlane[3]));

                /* packed (8*8 + 8*8) -> 16 */
                xmm0 = _mm_maddubs_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
                xmm1 = _mm_maddubs_epi16(xmm1, *(__m128i *)(coeffs + 32));    /* coefs 2,3 */
                xmm2 = _mm_maddubs_epi16(xmm2, *(__m128i *)(coeffs + 64));    /* coefs 4,5 */
                xmm3 = _mm_maddubs_epi16(xmm3, *(__m128i *)(coeffs + 96));    /* coefs 6,7 */

                /* sum intermediate values, add offset, shift off fraction bits */
                xmm0 = _mm_add_epi16(xmm0, xmm1);
                xmm0 = _mm_add_epi16(xmm0, xmm2);
                xmm0 = _mm_add_epi16(xmm0, xmm3);
                xmm0 = _mm_add_epi16(xmm0, xmm_offset);
                xmm0 = _mm_sra_epi16(xmm0, xmm_shift);

                /* store 8 16-bit words */
                if (col >= 8) {
                    _mm_storeu_si128((__m128i *)pDst, xmm0);
                    pSrc += 8;
                    pDst += 8;
                    col -= 8;
                    continue;
                }

                /* store 4 16-bit words */
                _mm_storel_epi64((__m128i *)pDst, xmm0);
                break;
            }

            pSrcRef += srcPitch;
            pDstRef += dstPitch;
        } while (--height);
    }
}

/* chroma, horizontal, 8-bit input, 16-bit output 
 * AVX2 path used when width = multiple of 16
 */
void MAKE_NAME(h265_InterpChroma_s8_d16_H) ( INTERP_S8_D16_PARAMETERS_LIST, int plane )
{
    int col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    const signed char* shufTab;

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];
    if (plane == TEXT_CHROMA)
        shufTab = (signed char *)shufTabIntUV;
    else 
        shufTab = (signed char *)shufTabPlane;

    _mm256_zeroupper();

    if ((width & 0x0f) == 0) {
        __m256i ymm0, ymm1, ymm2, ymm3;
        __m256i ymm_offset;
        __m128i xmm_shift;

        ymm_offset = _mm256_set1_epi16(offset);
        xmm_shift = _mm_cvtsi32_si128(shift);

        ymm2 = _mm256_load_si256((__m256i *)(coeffs +  0));
        ymm3 = _mm256_load_si256((__m256i *)(coeffs + 32));

        /* calculate 16 outputs per inner loop, working horizontally */
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            col = width;

            while (col > 0) {
                /* load 24 8-bit pixels, permute into ymm lanes as [0-7 | 8-15 | 8-15 | 16-23] */
                ymm1 = _mm256_permute4x64_epi64(*(__m256i *)pSrc, 0x94);

                /* interleave pixels */
                ymm0 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTab +  0));
                ymm1 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTab + 32));

                /* packed (8*8 + 8*8) -> 16 */
                ymm0 = _mm256_maddubs_epi16(ymm0, ymm2);    /* coefs 0,1 */
                ymm1 = _mm256_maddubs_epi16(ymm1, ymm3);    /* coefs 2,3 */

                /* sum intermediate values, add offset, shift off fraction bits */
                ymm0 = _mm256_add_epi16(ymm0, ymm1);
                ymm0 = _mm256_add_epi16(ymm0, ymm_offset);
                ymm0 = _mm256_sra_epi16(ymm0, xmm_shift);

                /* store 16 16-bit words */
                _mm256_storeu_si256((__m256i *)pDst, ymm0);
                pSrc += 16;
                pDst += 16;
                col -= 16;
            }

            pSrcRef += srcPitch;
            pDstRef += dstPitch;
        } while (--height);
    } else {
        __m128i xmm0, xmm1, xmm2, xmm3;
        __m128i xmm_offset, xmm_shift;

        xmm_offset = _mm_set1_epi16(offset);
        xmm_shift = _mm_cvtsi32_si128(shift);

        xmm2 = _mm_load_si128((__m128i *)(coeffs +  0));
        xmm3 = _mm_load_si128((__m128i *)(coeffs + 32));

        /* calculate 8 outputs per inner loop, working horizontally */
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            col = width;

            while (col > 0) {
                /* load 16 8-bit pixels */
                xmm0 = _mm_loadu_si128((__m128i *)pSrc);
                xmm1 = xmm0;

                /* interleave pixels */
                xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTab +  0));
                xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTab + 32));

                /* packed (8*8 + 8*8) -> 16 */
                xmm0 = _mm_maddubs_epi16(xmm0, xmm2);    /* coefs 0,1 */
                xmm1 = _mm_maddubs_epi16(xmm1, xmm3);    /* coefs 2,3 */

                /* sum intermediate values, add offset, shift off fraction bits */
                xmm0 = _mm_add_epi16(xmm0, xmm1);
                xmm0 = _mm_add_epi16(xmm0, xmm_offset);
                xmm0 = _mm_sra_epi16(xmm0, xmm_shift);

                /* store 8 16-bit words */
                if (col >= 8) {
                    _mm_storeu_si128((__m128i *)pDst, xmm0);
                    pSrc += 8;
                    pDst += 8;
                    col -= 8;
                    continue;
                }

                /* store 2, 4, or 6 16-bit words - should compile to single compare with jg, je, jl */
                if (col > 4) {
                    _mm_storel_epi64((__m128i *)pDst, xmm0);
                    *(int *)(pDst+4) = _mm_extract_epi32(xmm0, 2);
                } else if (col == 4) {
                    _mm_storel_epi64((__m128i *)pDst, xmm0);
                } else {
                    *(int *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                }
                break;
            }

            pSrcRef += srcPitch;
            pDstRef += dstPitch;
        } while (--height);
    }
}

/* luma, vertical, 8-bit input, 16-bit output
 * AVX2 path used when width = multiple of 16
 */
void MAKE_NAME(h265_InterpLuma_s8_d16_V)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    ALIGN_DECL(32) signed char offsetTab[32], shiftTab[16];

    coeffs = filtTabLuma_S8[4 * (tab_index-1)];

    _mm256_zeroupper();

    if ((width & 0x0f) == 0) {
        __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
        __m128i xmm_shift;

        ymm0 = _mm256_set1_epi16(offset);
        _mm256_store_si256((__m256i*)offsetTab, ymm0);

        xmm_shift = _mm_cvtsi32_si128(shift);
        _mm_store_si128((__m128i*)shiftTab, xmm_shift);

        /* calculate 16 outputs per inner loop, working vertically */
        col = width;
        do {
            row = height;
            pSrc = pSrcRef;
            pDst = pDstRef;

            /* start by loading 16 8-bit pixels from rows 0-6 */
            ymm0 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 0*srcPitch), 0x10);
            ymm1 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 1*srcPitch), 0x10);
            ymm2 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 2*srcPitch), 0x10);
            ymm3 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 3*srcPitch), 0x10);
            ymm4 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 4*srcPitch), 0x10);
            ymm5 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 5*srcPitch), 0x10);
            ymm6 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 6*srcPitch), 0x10);

            do {
                /* interleave rows 0,1 and multiply by coefs 0,1 */
                ymm0 = _mm256_unpacklo_epi8(ymm0, ymm1);
                ymm0 = _mm256_maddubs_epi16(ymm0, *(__m256i*)(coeffs +  0));

                /* interleave rows 2,3 and multiply by coefs 2,3 */
                ymm7 = _mm256_unpacklo_epi8(ymm2, ymm3);
                ymm7 = _mm256_maddubs_epi16(ymm7, *(__m256i*)(coeffs + 32));
                ymm0 = _mm256_add_epi16(ymm0, ymm7);

                /* interleave rows 4,5 and multiply by coefs 4,5 */
                ymm7 = _mm256_unpacklo_epi8(ymm4, ymm5);
                ymm7 = _mm256_maddubs_epi16(ymm7, *(__m256i*)(coeffs + 64));
                ymm0 = _mm256_add_epi16(ymm0, ymm7);

                /* interleave rows 6,7 and multiply by coefs 6,7 */
                ymm7 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 7*srcPitch), 0x10);
                ymm6 = _mm256_unpacklo_epi8(ymm6, ymm7);
                ymm6 = _mm256_maddubs_epi16(ymm6, *(__m256i*)(coeffs + 96));
                ymm0 = _mm256_add_epi16(ymm0, ymm6);
                ymm6 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 6*srcPitch), 0x10);

                /* add offset, shift off fraction bits */
                ymm0 = _mm256_add_epi16(ymm0, *(__m256i*)offsetTab);
                ymm0 = _mm256_sra_epi16(ymm0, *(__m128i*)shiftTab);

                /* store 16 16-bit words */
                _mm256_storeu_si256((__m256i*)pDst, ymm0);

                /* shift row registers (1->0, 2->1, etc.) */
                ymm0 = ymm1;
                ymm1 = ymm2;
                ymm2 = ymm3;
                ymm3 = ymm4;
                ymm4 = ymm5;
                ymm5 = ymm6;
                ymm6 = ymm7;

                pSrc += srcPitch;
                pDst += dstPitch;
            } while (--row);

            col -= 16;
            pSrcRef += 16;
            pDstRef += 16;
        } while (col > 0);
    } else {
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

        /* cache offset and shift registers since we will run out of xmm registers */
        xmm0 = _mm_set1_epi16(offset);
        _mm_store_si128((__m128i*)offsetTab, xmm0);

        xmm0 = _mm_cvtsi32_si128(shift);
        _mm_store_si128((__m128i*)shiftTab, xmm0);

        /* calculate 8 outputs per inner loop, working vertically */
        col = width;
        do {
            row = height;
            pSrc = pSrcRef;
            pDst = pDstRef;

            /* start by loading 8 8-bit pixels from rows 0-6 */
            xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
            xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
            xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));
            xmm3 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
            xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
            xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 5*srcPitch));
            xmm6 = _mm_loadl_epi64((__m128i*)(pSrc + 6*srcPitch));

            do {
                /* interleave rows 0,1 and multiply by coefs 0,1 */
                xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);
                xmm0 = _mm_maddubs_epi16(xmm0, *(__m128i*)(coeffs +  0));

                /* interleave rows 2,3 and multiply by coefs 2,3 */
                xmm7 = xmm2;
                xmm7 = _mm_unpacklo_epi8(xmm7, xmm3);
                xmm7 = _mm_maddubs_epi16(xmm7, *(__m128i*)(coeffs + 32));
                xmm0 = _mm_add_epi16(xmm0, xmm7);

                /* interleave rows 4,5 and multiply by coefs 4,5 */
                xmm7 = xmm4;
                xmm7 = _mm_unpacklo_epi8(xmm7, xmm5);
                xmm7 = _mm_maddubs_epi16(xmm7, *(__m128i*)(coeffs + 64));
                xmm0 = _mm_add_epi16(xmm0, xmm7);

                /* interleave rows 6,7 and multiply by coefs 6,7 */
                xmm7 = _mm_loadl_epi64((__m128i*)(pSrc + 7*srcPitch));    /* load row 7 */
                xmm6 = _mm_unpacklo_epi8(xmm6, xmm7);
                xmm6 = _mm_maddubs_epi16(xmm6, *(__m128i*)(coeffs + 96));
                xmm0 = _mm_add_epi16(xmm0, xmm6);
                xmm6 = _mm_loadl_epi64((__m128i*)(pSrc + 6*srcPitch));    /* save for next time, not enough registers to avoid reload */

                /* add offset, shift off fraction bits */
                xmm0 = _mm_add_epi16(xmm0, *(__m128i*)offsetTab);
                xmm0 = _mm_sra_epi16(xmm0, *(__m128i*)shiftTab);

                /* store 4 or 8 16-bit words */
                if (col >= 8)
                    _mm_storeu_si128((__m128i*)pDst, xmm0);
                else
                    _mm_storel_epi64((__m128i*)pDst, xmm0);

                /* shift row registers (1->0, 2->1, etc.) */
                xmm0 = xmm1;
                xmm1 = xmm2;
                xmm2 = xmm3;
                xmm3 = xmm4;
                xmm4 = xmm5;
                xmm5 = xmm6;
                xmm6 = xmm7;

                pSrc += srcPitch;
                pDst += dstPitch;
            } while (--row);

            col -= 8;
            pSrcRef += 8;
            pDstRef += 8;
        } while (col > 0);
    }
}

/* chroma, vertical, 8-bit input, 16-bit output
 * AVX2 path used when width = multiple of 16
 */
void MAKE_NAME(h265_InterpChroma_s8_d16_V)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];

    _mm256_zeroupper();

    if ((width & 0x0f) == 0) {
        __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm7;
        __m256i ymm_offset;
        __m128i xmm_shift;

        ymm_offset = _mm256_set1_epi16(offset);
        xmm_shift = _mm_cvtsi32_si128(shift);

        ymm4 = _mm256_load_si256((__m256i*)(coeffs +  0));
        ymm5 = _mm256_load_si256((__m256i*)(coeffs + 32));

        /* calculate 16 outputs per inner loop, working vertically */
        col = width;
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            row = height;

            /* start by loading 16 8-bit pixels from rows 0-2 */
            ymm0 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 0*srcPitch), 0x10);
            ymm1 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 1*srcPitch), 0x10);
            ymm2 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 2*srcPitch), 0x10);

            do {
                /* load row 3 */
                ymm3 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 3*srcPitch), 0x10);

                /* interleave rows 0,1 and multiply by coefs 0,1 */
                ymm0 = _mm256_unpacklo_epi8(ymm0, ymm1);
                ymm0 = _mm256_maddubs_epi16(ymm0, ymm4);

                /* interleave rows 2,3 and multiply by coefs 2,3 */
                ymm7 = _mm256_unpacklo_epi8(ymm2, ymm3);
                ymm7 = _mm256_maddubs_epi16(ymm7, ymm5);
                ymm0 = _mm256_add_epi16(ymm0, ymm7);

                /* add offset, shift off fraction bits */
                ymm0 = _mm256_add_epi16(ymm0, ymm_offset);
                ymm0 = _mm256_sra_epi16(ymm0, xmm_shift);

                _mm256_storeu_si256((__m256i*)pDst, ymm0);

                /* shift row registers (1->0, 2->1, etc.) */
                ymm0 = ymm1;
                ymm1 = ymm2;
                ymm2 = ymm3;

                pSrc += srcPitch;
                pDst += dstPitch;
            } while (--row);

            col -= 16;
            pSrcRef += 16;
            pDstRef += 16;
        } while (col > 0);
    } else {
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm7;
        __m128i xmm_offset, xmm_shift;

        xmm_offset = _mm_set1_epi16(offset);
        xmm_shift = _mm_cvtsi32_si128(shift);

        xmm4 = _mm_load_si128((__m128i *)(coeffs +  0));
        xmm5 = _mm_load_si128((__m128i *)(coeffs + 32));

        /* calculate 8 outputs per inner loop, working vertically */
        col = width;
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            row = height;

            /* start by loading 8 8-bit pixels from rows 0-2 */
            xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
            xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
            xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));

            do {
                /* load row 3 */
                xmm3 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));

                /* interleave rows 0,1 and multiply by coefs 0,1 */
                xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);
                xmm0 = _mm_maddubs_epi16(xmm0, xmm4);

                /* interleave rows 2,3 and multiply by coefs 2,3 */
                xmm7 = xmm2;
                xmm7 = _mm_unpacklo_epi8(xmm7, xmm3);
                xmm7 = _mm_maddubs_epi16(xmm7, xmm5);
                xmm0 = _mm_add_epi16(xmm0, xmm7);

                /* add offset, shift off fraction bits */
                xmm0 = _mm_add_epi16(xmm0, xmm_offset);
                xmm0 = _mm_sra_epi16(xmm0, xmm_shift);

                /* store 2, 4, 6 or 8 16-bit words */
                if (col >= 8) {
                    _mm_storeu_si128((__m128i*)pDst, xmm0);
                } else if (col == 6) {
                    _mm_storel_epi64((__m128i *)pDst, xmm0);
                    *(int *)(pDst+4) = _mm_extract_epi32(xmm0, 2);
                } else if (col == 4) {
                    _mm_storel_epi64((__m128i*)pDst, xmm0);
                } else {
                    *(int *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                }

                /* shift row registers (1->0, 2->1, etc.) */
                xmm0 = xmm1;
                xmm1 = xmm2;
                xmm2 = xmm3;

                pSrc += srcPitch;
                pDst += dstPitch;
            } while (--row);

            col -= 8;
            pSrcRef += 8;
            pDstRef += 8;
        } while (col > 0);
    }
}

/* luma, vertical, 16-bit input, 16-bit output
 * AVX2 path used when width = multiple of 8
 */
void MAKE_NAME(h265_InterpLuma_s16_d16_V)(INTERP_S16_D16_PARAMETERS_LIST)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    ALIGN_DECL(32) signed char offsetTab[32], shiftTab[16];

    coeffs = (const signed char *)filtTabLuma_S16[4 * (tab_index-1)];

    _mm256_zeroupper();

    if ((width & 0x07) == 0) {
        __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
        __m128i xmm_shift;

        ymm0 = _mm256_set1_epi32(offset);
        _mm256_store_si256((__m256i*)offsetTab, ymm0);

        xmm_shift = _mm_cvtsi32_si128(shift);
        _mm_storeu_si128((__m128i*)shiftTab, xmm_shift);

        /* calculate 8 outputs per inner loop, working vertically */
        col = width;
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            row = height;

            /* start by loading 8 16-bit pixels from rows 0-6 */
            ymm0 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 0*srcPitch), 0x10);
            ymm1 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 1*srcPitch), 0x10);
            ymm2 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 2*srcPitch), 0x10);
            ymm3 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 3*srcPitch), 0x10);
            ymm4 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 4*srcPitch), 0x10);
            ymm5 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 5*srcPitch), 0x10);
            ymm6 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 6*srcPitch), 0x10);

            do {
                /* interleave rows 0,1 and multiply by coefs 0,1 */
                ymm0 = _mm256_unpacklo_epi16(ymm0, ymm1);
                ymm0 = _mm256_madd_epi16(ymm0, *(__m256i*)(coeffs +  0));

                /* interleave rows 2,3 and multiply by coefs 2,3 */
                ymm7 = _mm256_unpacklo_epi16(ymm2, ymm3);
                ymm7 = _mm256_madd_epi16(ymm7, *(__m256i*)(coeffs + 32));
                ymm0 = _mm256_add_epi32(ymm0, ymm7);

                /* interleave rows 4,5 and multiply by coefs 4,5 */
                ymm7 = _mm256_unpacklo_epi16(ymm4, ymm5);
                ymm7 = _mm256_madd_epi16(ymm7, *(__m256i*)(coeffs + 64));
                ymm0 = _mm256_add_epi32(ymm0, ymm7);

                /* interleave rows 6,7 and multiply by coefs 6,7 */
                ymm7 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 7*srcPitch), 0x10);
                ymm6 = _mm256_unpacklo_epi16(ymm6, ymm7);
                ymm6 = _mm256_madd_epi16(ymm6, *(__m256i*)(coeffs + 96));
                ymm0 = _mm256_add_epi32(ymm0, ymm6);
                ymm6 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 6*srcPitch), 0x10);

                /* add offset, shift off fraction bits */
                ymm0 = _mm256_add_epi32(ymm0, *(__m256i*)offsetTab);
                ymm0 = _mm256_sra_epi32(ymm0, *(__m128i*)shiftTab);
                ymm0 = _mm256_packs_epi32(ymm0, ymm0);

                ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
                _mm_storeu_si128((__m128i*)pDst, _mm256_castsi256_si128(ymm0));

                /* shift row registers (1->0, 2->1, etc.) */
                ymm0 = ymm1;
                ymm1 = ymm2;
                ymm2 = ymm3;
                ymm3 = ymm4;
                ymm4 = ymm5;
                ymm5 = ymm6;
                ymm6 = ymm7;

                pSrc += srcPitch;
                pDst += dstPitch;
            } while (--row);

            col -= 8;
            pSrcRef += 8;
            pDstRef += 8;
        } while (col > 0);
    } else {
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

        xmm0 = _mm_set1_epi32(offset);
        _mm_storeu_si128((__m128i*)offsetTab, xmm0);

        xmm0 = _mm_cvtsi32_si128(shift);
        _mm_storeu_si128((__m128i*)shiftTab, xmm0);

        /* always calculates 4 outputs per inner loop, working vertically */
        col = width;
        do {
            row = height;
            pSrc = pSrcRef;
            pDst = pDstRef;

            /* start by loading 4 16-bit pixels from rows 0-6 */
            xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
            xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
            xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));
            xmm3 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
            xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
            xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 5*srcPitch));
            xmm6 = _mm_loadl_epi64((__m128i*)(pSrc + 6*srcPitch));

            do {
                /* interleave rows 0,1 and multiply by coefs 0,1 */
                xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);
                xmm0 = _mm_madd_epi16(xmm0, *(__m128i*)(coeffs +  0));

                /* interleave rows 2,3 and multiply by coefs 2,3 */
                xmm7 = xmm2;
                xmm7 = _mm_unpacklo_epi16(xmm7, xmm3);
                xmm7 = _mm_madd_epi16(xmm7, *(__m128i*)(coeffs + 32));
                xmm0 = _mm_add_epi32(xmm0, xmm7);

                /* interleave rows 4,5 and multiply by coefs 4,5 */
                xmm7 = xmm4;
                xmm7 = _mm_unpacklo_epi16(xmm7, xmm5);
                xmm7 = _mm_madd_epi16(xmm7, *(__m128i*)(coeffs + 64));
                xmm0 = _mm_add_epi32(xmm0, xmm7);

                /* interleave rows 6,7 and multiply by coefs 6,7 */
                xmm7 = _mm_loadl_epi64((__m128i*)(pSrc + 7*srcPitch));    /* load row 7 */
                xmm6 = _mm_unpacklo_epi16(xmm6, xmm7);
                xmm6 = _mm_madd_epi16(xmm6, *(__m128i*)(coeffs + 96));
                xmm0 = _mm_add_epi32(xmm0, xmm6);
                xmm6 = _mm_loadl_epi64((__m128i*)(pSrc + 6*srcPitch));    /* save for next time, not enough registers to avoid reload */

                /* add offset, shift off fraction bits, clip from 32 to 16 bits */
                xmm0 = _mm_add_epi32(xmm0, *(__m128i*)offsetTab);
                xmm0 = _mm_sra_epi32(xmm0, *(__m128i*)shiftTab);
                xmm0 = _mm_packs_epi32(xmm0, xmm0);

                /* always store 4 16-bit values */
                _mm_storel_epi64((__m128i*)pDst, xmm0);

                /* shift row registers (1->0, 2->1, etc.) */
                xmm0 = xmm1;
                xmm1 = xmm2;
                xmm2 = xmm3;
                xmm3 = xmm4;
                xmm4 = xmm5;
                xmm5 = xmm6;
                xmm6 = xmm7;

                pSrc += srcPitch;
                pDst += dstPitch;
            } while (--row);

            col -= 4;
            pSrcRef += 4;
            pDstRef += 4;
        } while (col > 0);
    }
}

/* chroma, vertical, 16-bit input, 16-bit output
 * AVX2 path used when width = multiple of 8
 */
void MAKE_NAME(h265_InterpChroma_s16_d16_V)(INTERP_S16_D16_PARAMETERS_LIST)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm7;    /* let compiler decide how to use xmm vs. stack */
    __m128i xmm_offset, xmm_shift;

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    _mm256_zeroupper();

    if ((width & 0x07) == 0) {
        __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm7;
        __m256i ymm_offset;
        __m128i xmm_shift;

        ymm_offset = _mm256_set1_epi32(offset);
        xmm_shift = _mm_cvtsi32_si128(shift);

        ymm4 = _mm256_load_si256((__m256i*)(coeffs +  0));
        ymm5 = _mm256_load_si256((__m256i*)(coeffs + 32));

        /* calculate 8 outputs per inner loop, working vertically */
        col = width;
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            row = height;

            /* start by loading 8 16-bit pixels from rows 0-2 */
            ymm0 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 0*srcPitch), 0x10);
            ymm1 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 1*srcPitch), 0x10);
            ymm2 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 2*srcPitch), 0x10);

            do {
                /* load row 3 */
                ymm3 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 3*srcPitch), 0x10);

                /* interleave rows 0,1 and multiply by coefs 0,1 */
                ymm0 = _mm256_unpacklo_epi16(ymm0, ymm1);
                ymm0 = _mm256_madd_epi16(ymm0, ymm4);

                /* interleave rows 2,3 and multiply by coefs 2,3 */
                ymm7 = _mm256_unpacklo_epi16(ymm2, ymm3);
                ymm7 = _mm256_madd_epi16(ymm7, ymm5);
                ymm0 = _mm256_add_epi32(ymm0, ymm7);

                /* add offset, shift off fraction bits */
                ymm0 = _mm256_add_epi32(ymm0, ymm_offset);
                ymm0 = _mm256_sra_epi32(ymm0, xmm_shift);
                ymm0 = _mm256_packs_epi32(ymm0, ymm0);

                ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
                _mm_storeu_si128((__m128i*)pDst, _mm256_castsi256_si128(ymm0));

                /* shift row registers (1->0, 2->1, etc.) */
                ymm0 = ymm1;
                ymm1 = ymm2;
                ymm2 = ymm3;

                pSrc += srcPitch;
                pDst += dstPitch;
            } while (--row);

            col -= 8;
            pSrcRef += 8;
            pDstRef += 8;
        } while (col > 0);
    } else {
        xmm_offset = _mm_set1_epi32(offset);
        xmm_shift = _mm_cvtsi32_si128(shift);

        xmm4 = _mm_load_si128((__m128i *)(coeffs +  0));
        xmm5 = _mm_load_si128((__m128i *)(coeffs + 32));

        /* always calculates 4 outputs per inner loop, working vertically */
        col = width;
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            row = height;

            /* start by loading 4 16-bit pixels from rows 0-2 */
            xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
            xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
            xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));

            do {
                /* load row 3 */
                xmm3 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));

                /* interleave rows 0,1 and multiply by coefs 0,1 */
                xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);
                xmm0 = _mm_madd_epi16(xmm0, xmm4);

                /* interleave rows 2,3 and multiply by coefs 2,3 */
                xmm7 = xmm2;
                xmm7 = _mm_unpacklo_epi16(xmm7, xmm3);
                xmm7 = _mm_madd_epi16(xmm7, xmm5);
                xmm0 = _mm_add_epi32(xmm0, xmm7);

                /* add offset, shift off fraction bits, clip from 32 to 16 bits */
                xmm0 = _mm_add_epi32(xmm0, xmm_offset);
                xmm0 = _mm_sra_epi32(xmm0, xmm_shift);
                xmm0 = _mm_packs_epi32(xmm0, xmm0);

                /* store 2 or 4 16-bit values */
                if (col >= 4)
                    _mm_storel_epi64((__m128i*)pDst, xmm0);
                else
                    *(int *)(pDst+0) = _mm_cvtsi128_si32(xmm0);

                /* shift row registers (1->0, 2->1, etc.) */
                xmm0 = xmm1;
                xmm1 = xmm2;
                xmm2 = xmm3;

                pSrc += srcPitch;
                pDst += dstPitch;
            } while (--row);

            col -= 4;
            pSrcRef += 4;
            pDstRef += 4;
        } while (col > 0);
    }
}

/* mode: AVERAGE_NO, clip/pack 16-bit output to 8-bit */
void MAKE_NAME(h265_AverageModeN)(INTERP_AVG_NONE_PARAMETERS_LIST)
{
    int col, off;
    short *pSrcRef = pSrc;
    unsigned char *pDstRef = pDst;

    _mm256_zeroupper();

    if ((width & 0x0f) == 0) {
        __m256i ymm0;

        do {
            col = width;
            off = 0;
            while (col > 0) {
                /* load 16 16-bit pixels, clip to 8-bit */
                ymm0 = _mm256_loadu_si256((__m256i *)(pSrc + off));
                ymm0 = _mm256_packus_epi16(ymm0, ymm0);
                ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
                _mm_storeu_si128((__m128i*)(pDst + off), _mm256_castsi256_si128(ymm0));

                off += 16;
                col -= 16;
            }
            pSrc += srcPitch;
            pDst += dstPitch;
        } while (--height);
    } else {
        __m128i xmm0;

        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            col = width;

            while (col > 0) {
                /* load 8 16-bit pixels, clip to 8-bit */
                xmm0 = _mm_loadu_si128((__m128i *)pSrc);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);

                /* store 8 pixels */
                if (col >= 8) {
                    _mm_storel_epi64((__m128i*)pDst, xmm0);
                    pSrc += 8;
                    pDst += 8;
                    col -= 8;
                    continue;
                }

                /* store 2, 4, or 6 pixels - should compile to single compare with jg, je, jl */
                if (col > 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                    *(short *)(pDst+4) = (short)_mm_extract_epi16(xmm0, 2);
                } else if (col == 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                } else {
                    *(short *)(pDst+0) = (short)_mm_extract_epi16(xmm0, 0);
                }
                break;
            }

            pSrcRef += srcPitch;
            pDstRef += dstPitch;
        } while (--height);
    }
}

/* mode: AVERAGE_FROM_PIC, load 8-bit pixels, extend to 16-bit, add to current output, clip/pack 16-bit to 8-bit */
void MAKE_NAME(h265_AverageModeP)(INTERP_AVG_PIC_PARAMETERS_LIST)
{
    int col, off;
    short *pSrcRef = pSrc;
    unsigned char *pDstRef = pDst, *pAvgRef = pAvg;

    _mm256_zeroupper();

    if ((width & 0x0f) == 0) {
        __m128i xmm1;
        __m256i ymm0, ymm1, ymm7;

        ymm7 = _mm256_set1_epi16(1 << 6);
        do {
            col = width;
            off = 0;
            while (col > 0) {
                /* load 16 16-bit pixels from source */
                ymm0 = _mm256_loadu_si256((__m256i *)(pSrc + off));

                /* load 16 8-bit pixels from avg buffer, zero extend to 16-bit, normalize fraction bits */
                xmm1 = _mm_loadu_si128((__m128i *)(pAvg + off));
                ymm1 = _mm256_cvtepu8_epi16(xmm1);
                ymm1 = _mm256_slli_epi16(ymm1, 6);

                /* add, round, clip back to 8 bits */
                ymm1 = _mm256_adds_epi16(ymm1, ymm7);
                ymm0 = _mm256_adds_epi16(ymm0, ymm1);
                ymm0 = _mm256_srai_epi16(ymm0, 7);
                ymm0 = _mm256_packus_epi16(ymm0, ymm0);

                ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
                _mm_storeu_si128((__m128i*)(pDst + off), _mm256_castsi256_si128(ymm0));

                off += 16;
                col -= 16;
            }

            pSrc += srcPitch;
            pDst += dstPitch;
            pAvg += avgPitch;
        } while (--height);
    } else {
        __m128i xmm0, xmm1, xmm7;

        xmm7 = _mm_set1_epi16(1 << 6);
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            pAvg = pAvgRef;
            col = width;

            while (col > 0) {
                /* load 8 16-bit pixels from source */
                xmm0 = _mm_loadu_si128((__m128i *)pSrc);

                /* load 8 8-bit pixels from avg buffer, zero extend to 16-bit, normalize fraction bits */
                xmm1 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(pAvg));    
                xmm1 = _mm_slli_epi16(xmm1, 6);

                /* add, round, clip back to 8 bits */
                xmm1 = _mm_adds_epi16(xmm1, xmm7);
                xmm0 = _mm_adds_epi16(xmm0, xmm1);
                xmm0 = _mm_srai_epi16(xmm0, 7);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);

                /* store 8 pixels */
                if (col >= 8) {
                    _mm_storel_epi64((__m128i*)pDst, xmm0);
                    pSrc += 8;
                    pDst += 8;
                    pAvg += 8;
                    col -= 8;
                    continue;
                }

                /* store 2, 4, or 6 pixels - should compile to single compare with jg, je, jl */
                if (col > 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                    *(short *)(pDst+4) = (short)_mm_extract_epi16(xmm0, 2);
                } else if (col == 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                } else {
                    *(short *)(pDst+0) = (short)_mm_extract_epi16(xmm0, 0);
                }
                break;
            }

            pSrcRef += srcPitch;
            pDstRef += dstPitch;
            pAvgRef += avgPitch;
        } while (--height);
    }
}

/* mode: AVERAGE_FROM_BUF, load 16-bit pixels, add to current output, clip/pack 16-bit to 8-bit */
void MAKE_NAME(h265_AverageModeB)(INTERP_AVG_BUF_PARAMETERS_LIST)
{
    int col, off;
    short *pSrcRef = pSrc, *pAvgRef = pAvg;
    unsigned char *pDstRef = pDst;

    _mm256_zeroupper();

    if ((width & 0x0f) == 0) {
        __m256i ymm0, ymm1, ymm7;

        ymm7 = _mm256_set1_epi16(1 << 6);
        do {
            col = width;
            off = 0;
            while (col > 0) {
                /* load 16 16-bit pixels from source and from avg */
                ymm0 = _mm256_loadu_si256((__m256i *)(pSrc + off));
                ymm1 = _mm256_loadu_si256((__m256i *)(pAvg + off));

                /* add, round, clip back to 8 bits */
                ymm1 = _mm256_adds_epi16(ymm1, ymm7);
                ymm0 = _mm256_adds_epi16(ymm0, ymm1);
                ymm0 = _mm256_srai_epi16(ymm0, 7);
                ymm0 = _mm256_packus_epi16(ymm0, ymm0);

                ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
                _mm_storeu_si128((__m128i*)(pDst + off), _mm256_castsi256_si128(ymm0));

                off += 16;
                col -= 16;
            }

            pSrc += srcPitch;
            pDst += dstPitch;
            pAvg += avgPitch;
        } while (--height);
    } else {
        __m128i xmm0, xmm1, xmm7;

        xmm7 = _mm_set1_epi16(1 << 6);
        do {
            pSrc = pSrcRef;
            pDst = pDstRef;
            pAvg = pAvgRef;
            col = width;

            while (col > 0) {
                /* load 8 16-bit pixels from source and from avg */
                xmm0 = _mm_loadu_si128((__m128i *)pSrc);
                xmm1 = _mm_loadu_si128((__m128i *)pAvg);

                /* add, round, clip back to 8 bits */
                xmm1 = _mm_adds_epi16(xmm1, xmm7);
                xmm0 = _mm_adds_epi16(xmm0, xmm1);
                xmm0 = _mm_srai_epi16(xmm0, 7);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);

                /* store 8 pixels */
                if (col >= 8) {
                    _mm_storel_epi64((__m128i*)pDst, xmm0);
                    pSrc += 8;
                    pDst += 8;
                    pAvg += 8;
                    col -= 8;
                    continue;
                }

                /* store 2, 4, or 6 pixels - should compile to single compare with jg, je, jl */
                if (col > 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                    *(short *)(pDst+4) = (short)_mm_extract_epi16(xmm0, 2);
                } else if (col == 4) {
                    *(int   *)(pDst+0) = _mm_cvtsi128_si32(xmm0);
                } else {
                    *(short *)(pDst+0) = (short)_mm_extract_epi16(xmm0, 0);
                }
                break;
            }

            pSrcRef += srcPitch;
            pDstRef += dstPitch;
            pAvgRef += avgPitch;
        } while (--height);
    }
}

} // end namespace MFX_HEVC_PP

#endif //#if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
