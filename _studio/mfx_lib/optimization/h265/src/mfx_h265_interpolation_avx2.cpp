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
// - requires that width = multiple of 4 (LUMA) or multiple of 2 (CHROMA)
// - requires that height = multiple of 2 for VERTICAL filters (processes 2 rows at a time to reduce shuffles)
// - horizontal filters can take odd heights, e.g. generating support pixels for vertical pass
// - input data is read in chunks of up to 24 bytes regardless of width, so assume it's safe to read up to 24 bytes beyond right edge of block 
//     (data not used, just must be allocated memory - pad inbuf as needed)
// - might require /Qsfalign option to ensure that ICL keeps 16-byte aligned stack when storing xmm registers
//     - have observed crashes when compiler (ICL 13) copies data from aligned ROM tables to non-aligned location on stack (compiler bug?)
// - templates used to reduce branches, so shift/offset values restricted to currently-used scale factors (see asserts below)
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 

#include <immintrin.h>

#ifdef __INTEL_COMPILER
/* disable warning: unused function parameter (offset not needed with template implementation) */
#pragma warning( disable : 869 )
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
template<int shift>
static void t_InterpLuma_s8_d16_H_AVX2(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3;

    _mm256_zeroupper();

    coeffs = filtTabLuma_S8[4 * (tab_index-1)];

    /* calculate 16 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 16) {
            /* load 24 8-bit pixels, permute into ymm lanes as [0-7 | 8-15 | 8-15 | 16-23] */
            ymm3 = _mm256_permute4x64_epi64(*(__m256i *)(pSrc + col), 0x94);

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
            if (shift > 0) {
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16( (1<<shift)>>1 ));  /* i.e. offset = 1 << (shift-1) (avoid false warning about negative shift) */
                ymm0 = _mm256_srai_epi16(ymm0, shift);
            }

            /* store 16 16-bit words */
            _mm256_storeu_si256((__m256i *)(pDst + col), ymm0);
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

template<int widthMul, int shift>
static void t_InterpLuma_s8_d16_H(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5;

    coeffs = filtTabLuma_S8[4 * (tab_index-1)];

    /* calculate 8 outputs per inner loop, working horizontally - use shuffle/interleave instead of pshufb on Atom */
    do {
        for (col = 0; col < width; col += widthMul) {
            /* load 16 8-bit pixels */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
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
            if (shift > 0) {
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16( (1<<shift)>>1 ));  /* i.e. offset = 1 << (shift-1) (avoid false warning about negative shift) */
                xmm0 = _mm_srai_epi16(xmm0, shift);
            }

            if (widthMul == 8) {
                /* store 8 16-bit words */
                _mm_storeu_si128((__m128i *)(pDst + col), xmm0);
            } else if (widthMul == 4) {
                /* store 4 16-bit words */
                _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
            }
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

/* luma, horizontal, 8-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLuma_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x03) == 0 );

    /* fast path - width multiple of 16 */
    if ((width & 0x0f) == 0) {
        if (shift == 0)
            t_InterpLuma_s8_d16_H_AVX2<0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLuma_s8_d16_H_AVX2<6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        return;
    }

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpLuma_s8_d16_H<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLuma_s8_d16_H<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        pSrc += width;
        pDst += width;
    }

    if (rem > 0) {
        if (shift == 0)
            t_InterpLuma_s8_d16_H<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpLuma_s8_d16_H<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    }
}

/* chroma, horizontal, 8-bit input, 16-bit output 
 * AVX2 path used when width = multiple of 16
 */
template<int shift>
static void t_InterpChroma_s8_d16_H_AVX2(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int plane )
{
    int col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    const signed char* shufTab;
    __m256i ymm0, ymm1, ymm2, ymm3;

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];
    if (plane == TEXT_CHROMA)
        shufTab = (signed char *)shufTabIntUV;
    else 
        shufTab = (signed char *)shufTabPlane;

    _mm256_zeroupper();

    ymm2 = _mm256_load_si256((__m256i *)(coeffs +  0));
    ymm3 = _mm256_load_si256((__m256i *)(coeffs + 32));

    /* calculate 16 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 16) {
            /* load 24 8-bit pixels, permute into ymm lanes as [0-7 | 8-15 | 8-15 | 16-23] */
            ymm1 = _mm256_permute4x64_epi64(*(__m256i *)(pSrc + col), 0x94);

            /* interleave pixels */
            ymm0 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTab +  0));
            ymm1 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTab + 32));

            /* packed (8*8 + 8*8) -> 16 */
            ymm0 = _mm256_maddubs_epi16(ymm0, ymm2);    /* coefs 0,1 */
            ymm1 = _mm256_maddubs_epi16(ymm1, ymm3);    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            ymm0 = _mm256_add_epi16(ymm0, ymm1);
            if (shift > 0) {
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm0 = _mm256_srai_epi16(ymm0, shift);
            }

            /* store 16 16-bit words */
            _mm256_storeu_si256((__m256i *)(pDst + col), ymm0);
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

template<int widthMul, int shift>
static void t_InterpChroma_s8_d16_H(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int plane )
{
    int col;
    const signed char* coeffs;
    const signed char* shufTab;
    __m128i xmm0, xmm1, xmm2, xmm3;

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];
    if (plane == TEXT_CHROMA)
        shufTab = (signed char *)shufTabIntUV;
    else 
        shufTab = (signed char *)shufTabPlane;

    xmm2 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm3 = _mm_load_si128((__m128i *)(coeffs + 32));

    /* calculate 8 outputs per inner loop, working horizontally - use shuffle/interleave instead of pshufb on Atom */
    do {
        /* same version for U/V interleaved or separate planes */
        for (col = 0; col < width; col += widthMul) {
            /* load 16 8-bit pixels */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
            xmm1 = xmm0;

            /* interleave pixels */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTab +  0));
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTab + 32));

            /* packed (8*8 + 8*8) -> 16 */
            xmm0 = _mm_maddubs_epi16(xmm0, xmm2);    /* coefs 0,1 */
            xmm1 = _mm_maddubs_epi16(xmm1, xmm3);    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi16(xmm0, xmm1);
            if (shift > 0) {
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16( (1<<shift)>>1 ));
                xmm0 = _mm_srai_epi16(xmm0, shift);
            }

            /* store 8 16-bit words */
            if (widthMul == 8) {
                _mm_storeu_si128((__m128i *)(pDst + col), xmm0);
            } else if (widthMul == 6) {
                *(int *)(pDst+col+0) = _mm_extract_epi32(xmm0, 0);
                *(int *)(pDst+col+2) = _mm_extract_epi32(xmm0, 1);
                *(int *)(pDst+col+4) = _mm_extract_epi32(xmm0, 2);
            } else if (widthMul == 4) {
                _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
            } else if (widthMul == 2) {
                *(int *)(pDst+col+0) = _mm_cvtsi128_si32(xmm0);
            }
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

/* chroma, horizontal, 8-bit input, 16-bit output */
void MAKE_NAME(h265_InterpChroma_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST, int plane)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x01) == 0 );

    /* fast path - width multiple of 16 */
    if ((width & 0x0f) == 0) {
        if (shift == 0)
            t_InterpChroma_s8_d16_H_AVX2<0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, plane);
        else if (shift == 6)
            t_InterpChroma_s8_d16_H_AVX2<6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, plane);
        return;
    }

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpChroma_s8_d16_H<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, plane);
        else if (shift == 6)
            t_InterpChroma_s8_d16_H<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, plane);
        pSrc += width;
        pDst += width;
    }

    if (rem == 4) {
        if (shift == 0)
            t_InterpChroma_s8_d16_H<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
        else if (shift == 6)
            t_InterpChroma_s8_d16_H<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
    } else if (rem == 2) {
        if (shift == 0)
            t_InterpChroma_s8_d16_H<2,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
        else if (shift == 6)
            t_InterpChroma_s8_d16_H<2,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
    } else if (rem == 6) {
        if (shift == 0)
            t_InterpChroma_s8_d16_H<6,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
        else if (shift == 6)
            t_InterpChroma_s8_d16_H<6,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
    }
}

/* luma, vertical, 8-bit input, 16-bit output
 * AVX2 path used when width = multiple of 16
 */
template<int shift>
static void t_InterpLuma_s8_d16_V_AVX2(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    coeffs = filtTabLuma_S8[4 * (tab_index-1)];

    _mm256_zeroupper();

    /* calculate 16 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 16) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* start by loading 16 8-bit pixels from rows 0-6 */
        ymm0 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 0*srcPitch), 0x10);
        ymm4 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 1*srcPitch), 0x10);
        ymm1 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 2*srcPitch), 0x10);
        ymm5 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 3*srcPitch), 0x10);
        ymm2 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 4*srcPitch), 0x10);
        ymm6 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 5*srcPitch), 0x10);

        ymm0 = _mm256_unpacklo_epi8(ymm0, ymm4);
        ymm1 = _mm256_unpacklo_epi8(ymm1, ymm5);
        ymm2 = _mm256_unpacklo_epi8(ymm2, ymm6);

        /* process even rows first (reduce row interleaving) */
        for (row = 0; row < height; row += 2) {
            ymm3 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 6*srcPitch), 0x10);
            ymm7 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 7*srcPitch), 0x10);
            ymm3 = _mm256_unpacklo_epi8(ymm3, ymm7);
    
            ymm4 = ymm1;
            ymm5 = ymm2;
            ymm6 = ymm3;

            /* multiply interleaved rows by interleaved coefs, sum into acc */
            ymm0 = _mm256_maddubs_epi16(ymm0, *(__m256i*)(coeffs +  0));
            ymm4 = _mm256_maddubs_epi16(ymm4, *(__m256i*)(coeffs + 32));
            ymm0 = _mm256_add_epi16(ymm0, ymm4);
            ymm5 = _mm256_maddubs_epi16(ymm5, *(__m256i*)(coeffs + 64));
            ymm0 = _mm256_add_epi16(ymm0, ymm5);
            ymm6 = _mm256_maddubs_epi16(ymm6, *(__m256i*)(coeffs + 96));
            ymm0 = _mm256_add_epi16(ymm0, ymm6);

            /* add offset, shift off fraction bits */
            if (shift > 0) {
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm0 = _mm256_srai_epi16(ymm0, shift);
            }

            /* store 16 16-bit words */
            _mm256_storeu_si256((__m256i*)pDst, ymm0);

            /* shift interleaved row registers (23->01, 45->23, etc.) */
            ymm0 = ymm1;
            ymm1 = ymm2;
            ymm2 = ymm3;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }

        /* reset pointers to overall row 1 */
        pSrc = pSrcRef + srcPitch;
        pDst = pDstRef + dstPitch;

        /* load 8 8-bit pixels from rows 0-5 */
        ymm0 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 0*srcPitch), 0x10);
        ymm4 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 1*srcPitch), 0x10);
        ymm1 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 2*srcPitch), 0x10);
        ymm5 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 3*srcPitch), 0x10);
        ymm2 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 4*srcPitch), 0x10);
        ymm6 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 5*srcPitch), 0x10);

        ymm0 = _mm256_unpacklo_epi8(ymm0, ymm4);
        ymm1 = _mm256_unpacklo_epi8(ymm1, ymm5);
        ymm2 = _mm256_unpacklo_epi8(ymm2, ymm6);

        /* process odd rows */
        for (row = 1; row < height; row += 2) {
            ymm3 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 6*srcPitch), 0x10);
            ymm7 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 7*srcPitch), 0x10);
            ymm3 = _mm256_unpacklo_epi8(ymm3, ymm7);
    
            ymm4 = ymm1;
            ymm5 = ymm2;
            ymm6 = ymm3;

            /* multiply interleaved rows by interleaved coefs, sum into acc */
            ymm0 = _mm256_maddubs_epi16(ymm0, *(__m256i*)(coeffs +  0));
            ymm4 = _mm256_maddubs_epi16(ymm4, *(__m256i*)(coeffs + 32));
            ymm0 = _mm256_add_epi16(ymm0, ymm4);
            ymm5 = _mm256_maddubs_epi16(ymm5, *(__m256i*)(coeffs + 64));
            ymm0 = _mm256_add_epi16(ymm0, ymm5);
            ymm6 = _mm256_maddubs_epi16(ymm6, *(__m256i*)(coeffs + 96));
            ymm0 = _mm256_add_epi16(ymm0, ymm6);

            /* add offset, shift off fraction bits */
            if (shift > 0) {
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm0 = _mm256_srai_epi16(ymm0, shift);
            }

            /* store 16 16-bit words */
            _mm256_storeu_si256((__m256i*)pDst, ymm0);

            /* shift interleaved row registers (23->01, 45->23, etc.) */
            ymm0 = ymm1;
            ymm1 = ymm2;
            ymm2 = ymm3;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += 16;
        pDstRef += 16;
    }
}

template<int widthMul, int shift>
static void t_InterpLuma_s8_d16_V(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = filtTabLuma_S8[4 * (tab_index-1)];

    for (col = 0; col < width; col += widthMul) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-5 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));
        xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
        xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
        xmm6 = _mm_loadl_epi64((__m128i*)(pSrc + 5*srcPitch));
        
        xmm0 = _mm_unpacklo_epi8(xmm0, xmm4);    /* interleave 01 */
        xmm1 = _mm_unpacklo_epi8(xmm1, xmm5);    /* interleave 23 */
        xmm2 = _mm_unpacklo_epi8(xmm2, xmm6);    /* interleave 45 */

        /* process even rows first (reduce row interleaving) */
        for (row = 0; row < height; row += 2) {
            xmm3 = _mm_loadl_epi64((__m128i*)(pSrc + 6*srcPitch));
            xmm7 = _mm_loadl_epi64((__m128i*)(pSrc + 7*srcPitch));
            xmm3 = _mm_unpacklo_epi8(xmm3, xmm7);    /* interleave 67 */

            xmm4 = xmm1;
            xmm5 = xmm2;
            xmm6 = xmm3;

            /* multiply interleaved rows by interleaved coefs, sum into acc */
            xmm0 = _mm_maddubs_epi16(xmm0, *(__m128i*)(coeffs +  0));
            xmm4 = _mm_maddubs_epi16(xmm4, *(__m128i*)(coeffs + 32));
            xmm0 = _mm_add_epi16(xmm0, xmm4);
            xmm5 = _mm_maddubs_epi16(xmm5, *(__m128i*)(coeffs + 64));
            xmm0 = _mm_add_epi16(xmm0, xmm5);
            xmm6 = _mm_maddubs_epi16(xmm6, *(__m128i*)(coeffs + 96));
            xmm0 = _mm_add_epi16(xmm0, xmm6);

            /* add offset, shift off fraction bits */
            if (shift > 0) {
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16( (1<<shift)>>1 ));
                xmm0 = _mm_srai_epi16(xmm0, shift);
            }

            /* store 4 or 8 16-bit words */
            if (widthMul == 8)
                _mm_storeu_si128((__m128i*)pDst, xmm0);
            else if (widthMul == 4)
                _mm_storel_epi64((__m128i*)pDst, xmm0);

            /* shift interleaved row registers (23->01, 45->23, etc.) */
            xmm0 = xmm1;
            xmm1 = xmm2;
            xmm2 = xmm3;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }

        /* reset pointers to overall row 1 */
        pSrc = pSrcRef + srcPitch;
        pDst = pDstRef + dstPitch;

        /* load 8 8-bit pixels from rows 0-5 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));
        xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
        xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
        xmm6 = _mm_loadl_epi64((__m128i*)(pSrc + 5*srcPitch));
        
        xmm0 = _mm_unpacklo_epi8(xmm0, xmm4);    /* interleave 01 */
        xmm1 = _mm_unpacklo_epi8(xmm1, xmm5);    /* interleave 23 */
        xmm2 = _mm_unpacklo_epi8(xmm2, xmm6);    /* interleave 45 */

        /* process odd rows */
        for (row = 1; row < height; row += 2) {
            xmm3 = _mm_loadl_epi64((__m128i*)(pSrc + 6*srcPitch));
            xmm7 = _mm_loadl_epi64((__m128i*)(pSrc + 7*srcPitch));
            xmm3 = _mm_unpacklo_epi8(xmm3, xmm7);    /* interleave 67 */

            xmm4 = xmm1;
            xmm5 = xmm2;
            xmm6 = xmm3;

            /* multiply interleaved rows by interleaved coefs, sum into acc */
            xmm0 = _mm_maddubs_epi16(xmm0, *(__m128i*)(coeffs +  0));
            xmm4 = _mm_maddubs_epi16(xmm4, *(__m128i*)(coeffs + 32));
            xmm0 = _mm_add_epi16(xmm0, xmm4);
            xmm5 = _mm_maddubs_epi16(xmm5, *(__m128i*)(coeffs + 64));
            xmm0 = _mm_add_epi16(xmm0, xmm5);
            xmm6 = _mm_maddubs_epi16(xmm6, *(__m128i*)(coeffs + 96));
            xmm0 = _mm_add_epi16(xmm0, xmm6);

            /* add offset, shift off fraction bits */
            if (shift > 0) {
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16( (1<<shift)>>1 ));
                xmm0 = _mm_srai_epi16(xmm0, shift);
            }

            /* store 4 or 8 16-bit words */
            if (widthMul == 8)
                _mm_storeu_si128((__m128i*)pDst, xmm0);
            else if (widthMul == 4)
                _mm_storel_epi64((__m128i*)pDst, xmm0);

            /* shift interleaved row registers (23->01, 45->23, etc.) */
            xmm0 = xmm1;
            xmm1 = xmm2;
            xmm2 = xmm3;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += widthMul;
        pDstRef += widthMul;
    }
}

/* luma, vertical, 8-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLuma_s8_d16_V)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( ((width & 0x03) == 0) && ((height & 0x01) == 0) );

    /* fast path - width multiple of 16 */
    if ((width & 0x0f) == 0) {
        if (shift == 0)
            t_InterpLuma_s8_d16_V_AVX2<0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLuma_s8_d16_V_AVX2<6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        return;
    }

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpLuma_s8_d16_V<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLuma_s8_d16_V<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        pSrc += width;
        pDst += width;
    }

    if (rem > 0) {
        if (shift == 0)
            t_InterpLuma_s8_d16_V<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpLuma_s8_d16_V<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    }
}

/* chroma, vertical, 8-bit input, 16-bit output
 * AVX2 path used when width = multiple of 16
 */
template<int shift>
static void t_InterpChroma_s8_d16_V_AVX2(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];

    _mm256_zeroupper();

    ymm6 = _mm256_load_si256((__m256i*)(coeffs +  0));
    ymm7 = _mm256_load_si256((__m256i*)(coeffs + 32));

    /* calculate 16 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 16) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* start by loading 16 8-bit pixels from rows 0-2 */
        ymm0 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 0*srcPitch), 0x10);
        ymm4 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 1*srcPitch), 0x10);
        ymm1 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 2*srcPitch), 0x10);

        ymm0 = _mm256_unpacklo_epi8(ymm0, ymm4);
        ymm4 = _mm256_unpacklo_epi8(ymm4, ymm1);

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            ymm5 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 3*srcPitch), 0x10);
            ymm2 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 4*srcPitch), 0x10);
            ymm1 = _mm256_unpacklo_epi8(ymm1, ymm5);
            ymm5 = _mm256_unpacklo_epi8(ymm5, ymm2);

            ymm0 = _mm256_maddubs_epi16(ymm0, ymm6);
            ymm4 = _mm256_maddubs_epi16(ymm4, ymm6);

            ymm3 = ymm1;
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm0 = _mm256_add_epi16(ymm0, ymm3);

            ymm3 = ymm5;
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm4 = _mm256_add_epi16(ymm4, ymm3);

            /* add offset, shift off fraction bits */
            if (shift > 0) {
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm0 = _mm256_srai_epi16(ymm0, shift);
                ymm4 = _mm256_add_epi16(ymm4, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm4 = _mm256_srai_epi16(ymm4, shift);
            }

            _mm256_storeu_si256((__m256i*)(pDst + 0*dstPitch), ymm0);
            _mm256_storeu_si256((__m256i*)(pDst + 1*dstPitch), ymm4);

            /* shift row registers (1->0, 2->1, etc.) */
            ymm0 = ymm1;
            ymm4 = ymm5;
            ymm1 = ymm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += 16;
        pDstRef += 16;
    }
}

template<int widthMul, int shift>
static void t_InterpChroma_s8_d16_V(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = (const signed char *)filtTabChroma_S8[2 * (tab_index-1)];

    xmm6 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm7 = _mm_load_si128((__m128i *)(coeffs + 32));

    for (col = 0; col < width; col += widthMul) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-2 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));

        xmm0 = _mm_unpacklo_epi8(xmm0, xmm4);    /* interleave 01 */
        xmm4 = _mm_unpacklo_epi8(xmm4, xmm1);    /* interleave 12 */

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
            xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
            xmm1 = _mm_unpacklo_epi8(xmm1, xmm5);    /* interleave 23 */
            xmm5 = _mm_unpacklo_epi8(xmm5, xmm2);    /* interleave 34 */

            xmm0 = _mm_maddubs_epi16(xmm0, xmm6);
            xmm4 = _mm_maddubs_epi16(xmm4, xmm6);

            xmm3 = xmm1;
            xmm3 = _mm_maddubs_epi16(xmm3, xmm7);
            xmm0 = _mm_add_epi16(xmm0, xmm3);

            xmm3 = xmm5;
            xmm3 = _mm_maddubs_epi16(xmm3, xmm7);
            xmm4 = _mm_add_epi16(xmm4, xmm3);
            
            /* add offset, shift off fraction bits */
            if (shift > 0) {
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16( (1<<shift)>>1 ));
                xmm4 = _mm_add_epi16(xmm4, _mm_set1_epi16( (1<<shift)>>1 ));
            }
            xmm0 = _mm_srai_epi16(xmm0, shift);
            xmm4 = _mm_srai_epi16(xmm4, shift);

            /* store 2, 4, 6 or 8 16-bit words */
            if (widthMul == 8) {
                _mm_storeu_si128((__m128i *)(pDst + 0*dstPitch), xmm0);
                _mm_storeu_si128((__m128i *)(pDst + 1*dstPitch), xmm4);
            } else if (widthMul == 6) {
                *(int *)(pDst+0*dstPitch+0) = _mm_extract_epi32(xmm0, 0);
                *(int *)(pDst+0*dstPitch+2) = _mm_extract_epi32(xmm0, 1);
                *(int *)(pDst+0*dstPitch+4) = _mm_extract_epi32(xmm0, 2);
                
                *(int *)(pDst+1*dstPitch+0) = _mm_extract_epi32(xmm4, 0);
                *(int *)(pDst+1*dstPitch+2) = _mm_extract_epi32(xmm4, 1);
                *(int *)(pDst+1*dstPitch+4) = _mm_extract_epi32(xmm4, 2);
            } else if (widthMul == 4) {
                _mm_storel_epi64((__m128i *)(pDst + 0*dstPitch), xmm0);
                _mm_storel_epi64((__m128i *)(pDst + 1*dstPitch), xmm4);
            } else if (widthMul == 2) {
                *(int *)(pDst+0*dstPitch+0) = _mm_cvtsi128_si32(xmm0);
                *(int *)(pDst+1*dstPitch+0) = _mm_cvtsi128_si32(xmm4);
            }

            /* shift interleaved row registers */
            xmm0 = xmm1;
            xmm4 = xmm5;
            xmm1 = xmm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += widthMul;
        pDstRef += widthMul;
    }
}

/* chroma, vertical, 8-bit input, 16-bit output */
void MAKE_NAME(h265_InterpChroma_s8_d16_V) ( INTERP_S8_D16_PARAMETERS_LIST )
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( ((width & 0x01) == 0) && ((height & 0x01) == 0) );

    /* fast path - width multiple of 16 */
    if ((width & 0x0f) == 0) {
        if (shift == 0)
            t_InterpChroma_s8_d16_V_AVX2<0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpChroma_s8_d16_V_AVX2<6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        return;
    }

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpChroma_s8_d16_V<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpChroma_s8_d16_V<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        pSrc += width;
        pDst += width;
    }

    if (rem == 4) {
        if (shift == 0)
            t_InterpChroma_s8_d16_V<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpChroma_s8_d16_V<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    } else if (rem == 2) {
        if (shift == 0)
            t_InterpChroma_s8_d16_V<2,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpChroma_s8_d16_V<2,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    } else if (rem == 6) {
        if (shift == 0)
            t_InterpChroma_s8_d16_V<6,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpChroma_s8_d16_V<6,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    }
}

/* luma, vertical, 16-bit input, 16-bit output
 * AVX2 path used when width = multiple of 8
 */
template<int shift>
static void t_InterpLuma_s16_d16_V_AVX2(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    coeffs = (const signed char *)filtTabLuma_S16[4 * (tab_index-1)];

    _mm256_zeroupper();

    /* calculate 8 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 8) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 16-bit pixels from rows 0-6 */
        ymm0 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 0*srcPitch), 0x10);
        ymm4 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 1*srcPitch), 0x10);
        ymm1 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 2*srcPitch), 0x10);
        ymm5 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 3*srcPitch), 0x10);
        ymm2 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 4*srcPitch), 0x10);
        ymm6 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 5*srcPitch), 0x10);

        ymm0 = _mm256_unpacklo_epi16(ymm0, ymm4);
        ymm1 = _mm256_unpacklo_epi16(ymm1, ymm5);
        ymm2 = _mm256_unpacklo_epi16(ymm2, ymm6);

        /* process even rows first (reduce row interleaving) */
        for (row = 0; row < height; row += 2) {
            ymm3 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 6*srcPitch), 0x10);
            ymm7 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 7*srcPitch), 0x10);
            ymm3 = _mm256_unpacklo_epi16(ymm3, ymm7);

            ymm4 = ymm1;
            ymm5 = ymm2;
            ymm6 = ymm3;

            /* multiply interleaved rows by interleaved coefs, sum into acc */
            ymm0 = _mm256_madd_epi16(ymm0, *(__m256i*)(coeffs +  0));
            ymm4 = _mm256_madd_epi16(ymm4, *(__m256i*)(coeffs + 32));
            ymm0 = _mm256_add_epi32(ymm0, ymm4);
            ymm5 = _mm256_madd_epi16(ymm5, *(__m256i*)(coeffs + 64));
            ymm0 = _mm256_add_epi32(ymm0, ymm5);
            ymm6 = _mm256_madd_epi16(ymm6, *(__m256i*)(coeffs + 96));
            ymm0 = _mm256_add_epi32(ymm0, ymm6);

            /* add offset, shift off fraction bits */
            if (shift == 12)
                ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(1 << (shift - 1)));
            ymm0 = _mm256_srai_epi32(ymm0, shift);
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);

            ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
            _mm_storeu_si128((__m128i*)pDst, _mm256_castsi256_si128(ymm0));

            /* shift row registers (1->0, 2->1, etc.) */
            ymm0 = ymm1;
            ymm1 = ymm2;
            ymm2 = ymm3;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }

        /* reset pointers to overall row 1 */
        pSrc = pSrcRef + srcPitch;
        pDst = pDstRef + dstPitch;

        /* load 8 16-bit pixels from rows 0-6 */
        ymm0 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 0*srcPitch), 0x10);
        ymm4 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 1*srcPitch), 0x10);
        ymm1 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 2*srcPitch), 0x10);
        ymm5 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 3*srcPitch), 0x10);
        ymm2 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 4*srcPitch), 0x10);
        ymm6 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 5*srcPitch), 0x10);

        ymm0 = _mm256_unpacklo_epi16(ymm0, ymm4);
        ymm1 = _mm256_unpacklo_epi16(ymm1, ymm5);
        ymm2 = _mm256_unpacklo_epi16(ymm2, ymm6);

        /* process odd rows */
        for (row = 1; row < height; row += 2) {
            ymm3 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 6*srcPitch), 0x10);
            ymm7 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 7*srcPitch), 0x10);
            ymm3 = _mm256_unpacklo_epi16(ymm3, ymm7);

            ymm4 = ymm1;
            ymm5 = ymm2;
            ymm6 = ymm3;

            /* multiply interleaved rows by interleaved coefs, sum into acc */
            ymm0 = _mm256_madd_epi16(ymm0, *(__m256i*)(coeffs +  0));
            ymm4 = _mm256_madd_epi16(ymm4, *(__m256i*)(coeffs + 32));
            ymm0 = _mm256_add_epi32(ymm0, ymm4);
            ymm5 = _mm256_madd_epi16(ymm5, *(__m256i*)(coeffs + 64));
            ymm0 = _mm256_add_epi32(ymm0, ymm5);
            ymm6 = _mm256_madd_epi16(ymm6, *(__m256i*)(coeffs + 96));
            ymm0 = _mm256_add_epi32(ymm0, ymm6);


            /* add offset, shift off fraction bits */
            if (shift == 12)
                ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(1 << (shift - 1)));
            ymm0 = _mm256_srai_epi32(ymm0, shift);
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);

            ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
            _mm_storeu_si128((__m128i*)pDst, _mm256_castsi256_si128(ymm0));

            /* shift row registers (1->0, 2->1, etc.) */
            ymm0 = ymm1;
            ymm1 = ymm2;
            ymm2 = ymm3;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        
        pSrcRef += 8;
        pDstRef += 8;
    }
}

template<int shift>
static void t_InterpLuma_s16_d16_V(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = (const signed char *)filtTabLuma_S16[4 * (tab_index-1)];

    /* always calculates 4 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 4) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 4 16-bit pixels from rows 0-6 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));
        xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
        xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
        xmm6 = _mm_loadl_epi64((__m128i*)(pSrc + 5*srcPitch));
        
        xmm0 = _mm_unpacklo_epi16(xmm0, xmm4);    /* interleave 01 */
        xmm1 = _mm_unpacklo_epi16(xmm1, xmm5);    /* interleave 23 */
        xmm2 = _mm_unpacklo_epi16(xmm2, xmm6);    /* interleave 45 */

        /* process even rows first (reduce row interleaving) */
        for (row = 0; row < height; row += 2) {
            xmm3 = _mm_loadl_epi64((__m128i*)(pSrc + 6*srcPitch));
            xmm7 = _mm_loadl_epi64((__m128i*)(pSrc + 7*srcPitch));
            xmm3 = _mm_unpacklo_epi16(xmm3, xmm7);    /* interleave 67 */

            xmm4 = xmm1;
            xmm5 = xmm2;
            xmm6 = xmm3;

            /* multiply interleaved rows by interleaved coefs, sum into acc */
            xmm0 = _mm_madd_epi16(xmm0, *(__m128i*)(coeffs +  0));
            xmm4 = _mm_madd_epi16(xmm4, *(__m128i*)(coeffs + 32));
            xmm0 = _mm_add_epi32(xmm0, xmm4);
            xmm5 = _mm_madd_epi16(xmm5, *(__m128i*)(coeffs + 64));
            xmm0 = _mm_add_epi32(xmm0, xmm5);
            xmm6 = _mm_madd_epi16(xmm6, *(__m128i*)(coeffs + 96));
            xmm0 = _mm_add_epi32(xmm0, xmm6);

            /* add offset, shift off fraction bits, clip from 32 to 16 bits */
            if (shift == 12)
                xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(1 << (shift - 1)));
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);

            /* always store 4 16-bit values */
            _mm_storel_epi64((__m128i*)pDst, xmm0);

            /* shift interleaved row registers */
            xmm0 = xmm1;
            xmm1 = xmm2;
            xmm2 = xmm3;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }

        /* reset pointers to overall row 1 */
        pSrc = pSrcRef + srcPitch;
        pDst = pDstRef + dstPitch;

        /* load 4 16-bit pixels from rows 0-6 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));
        xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
        xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
        xmm6 = _mm_loadl_epi64((__m128i*)(pSrc + 5*srcPitch));
        
        xmm0 = _mm_unpacklo_epi16(xmm0, xmm4);    /* interleave 01 */
        xmm1 = _mm_unpacklo_epi16(xmm1, xmm5);    /* interleave 23 */
        xmm2 = _mm_unpacklo_epi16(xmm2, xmm6);    /* interleave 45 */

        /* process odd rows */
        for (row = 1; row < height; row += 2) {
            xmm3 = _mm_loadl_epi64((__m128i*)(pSrc + 6*srcPitch));
            xmm7 = _mm_loadl_epi64((__m128i*)(pSrc + 7*srcPitch));
            xmm3 = _mm_unpacklo_epi16(xmm3, xmm7);    /* interleave 67 */

            xmm4 = xmm1;
            xmm5 = xmm2;
            xmm6 = xmm3;

            /* multiply interleaved rows by interleaved coefs, sum into acc */
            xmm0 = _mm_madd_epi16(xmm0, *(__m128i*)(coeffs +  0));
            xmm4 = _mm_madd_epi16(xmm4, *(__m128i*)(coeffs + 32));
            xmm0 = _mm_add_epi32(xmm0, xmm4);
            xmm5 = _mm_madd_epi16(xmm5, *(__m128i*)(coeffs + 64));
            xmm0 = _mm_add_epi32(xmm0, xmm5);
            xmm6 = _mm_madd_epi16(xmm6, *(__m128i*)(coeffs + 96));
            xmm0 = _mm_add_epi32(xmm0, xmm6);

            /* add offset, shift off fraction bits, clip from 32 to 16 bits */
            if (shift == 12)
                xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(1 << (shift - 1)));
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);

            /* always store 4 16-bit values */
            _mm_storel_epi64((__m128i*)pDst, xmm0);

            /* shift interleaved row registers */
            xmm0 = xmm1;
            xmm1 = xmm2;
            xmm2 = xmm3;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += 4;
        pDstRef += 4;
    }
}

/* luma, vertical, 16-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLuma_s16_d16_V)(INTERP_S16_D16_PARAMETERS_LIST)
{
    VM_ASSERT( (shift == 6 && offset == 0) || (shift == 12 && offset == (1 << (shift-1))) );
    VM_ASSERT( ((width & 0x03) == 0) && ((height & 0x01) == 0) );

    /* fast path - width multiple of 8 */
    if ((width & 0x07) == 0) {
        if (shift == 6)
            t_InterpLuma_s16_d16_V_AVX2< 6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 12)
            t_InterpLuma_s16_d16_V_AVX2<12>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        return;
    }

    if (shift == 6)
        t_InterpLuma_s16_d16_V< 6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
    else if (shift == 12)
        t_InterpLuma_s16_d16_V<12>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
}

/* chroma, vertical, 16-bit input, 16-bit output
 * AVX2 path used when width = multiple of 8
 */
template<int shift>
static void t_InterpChroma_s16_d16_V_AVX2(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    _mm256_zeroupper();

    ymm6 = _mm256_load_si256((__m256i*)(coeffs +  0));
    ymm7 = _mm256_load_si256((__m256i*)(coeffs + 32));

    /* calculate 8 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 8) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-2 */
        ymm0 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 0*srcPitch), 0x10);
        ymm4 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 1*srcPitch), 0x10);
        ymm1 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 2*srcPitch), 0x10);

        ymm0 = _mm256_unpacklo_epi16(ymm0, ymm4);
        ymm4 = _mm256_unpacklo_epi16(ymm4, ymm1);

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            /* load row 3 */
            ymm5 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 3*srcPitch), 0x10);
            ymm2 = _mm256_permute4x64_epi64(*(__m256i*)(pSrc + 4*srcPitch), 0x10);
            ymm1 = _mm256_unpacklo_epi16(ymm1, ymm5);
            ymm5 = _mm256_unpacklo_epi16(ymm5, ymm2);

            ymm0 = _mm256_madd_epi16(ymm0, ymm6);
            ymm4 = _mm256_madd_epi16(ymm4, ymm6);

            ymm3 = ymm1;
            ymm3 = _mm256_madd_epi16(ymm3, ymm7);
            ymm0 = _mm256_add_epi32(ymm0, ymm3);

            ymm3 = ymm5;
            ymm3 = _mm256_madd_epi16(ymm3, ymm7);
            ymm4 = _mm256_add_epi32(ymm4, ymm3);

            /* add offset, shift off fraction bits */
            if (shift == 12) {
                ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(1 << (shift - 1)));
                ymm4 = _mm256_add_epi32(ymm4, _mm256_set1_epi32(1 << (shift - 1)));
            }
            ymm0 = _mm256_srai_epi32(ymm0, shift);
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);
            ymm4 = _mm256_srai_epi32(ymm4, shift);
            ymm4 = _mm256_packs_epi32(ymm4, ymm4);

            ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
            ymm4 = _mm256_permute4x64_epi64(ymm4, 0x08);

            _mm_storeu_si128((__m128i*)(pDst + 0*dstPitch), _mm256_castsi256_si128(ymm0));
            _mm_storeu_si128((__m128i*)(pDst + 1*dstPitch), _mm256_castsi256_si128(ymm4));

            /* shift interleaved row registers */
            ymm0 = ymm1;
            ymm4 = ymm5;
            ymm1 = ymm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += 8;
        pDstRef += 8;
    }
}

template<int widthMul, int shift>
static void t_InterpChroma_s16_d16_V(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    xmm6 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm7 = _mm_load_si128((__m128i *)(coeffs + 32));

    /* always calculates 4 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += widthMul) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-2 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));

        xmm0 = _mm_unpacklo_epi16(xmm0, xmm4);    /* interleave 01 */
        xmm4 = _mm_unpacklo_epi16(xmm4, xmm1);    /* interleave 12 */

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
            xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
            xmm1 = _mm_unpacklo_epi16(xmm1, xmm5);    /* interleave 23 */
            xmm5 = _mm_unpacklo_epi16(xmm5, xmm2);    /* interleave 34 */

            xmm0 = _mm_madd_epi16(xmm0, xmm6);
            xmm4 = _mm_madd_epi16(xmm4, xmm6);

            xmm3 = xmm1;
            xmm3 = _mm_madd_epi16(xmm3, xmm7);
            xmm0 = _mm_add_epi32(xmm0, xmm3);

            xmm3 = xmm5;
            xmm3 = _mm_madd_epi16(xmm3, xmm7);
            xmm4 = _mm_add_epi32(xmm4, xmm3);
            
            /* add offset, shift off fraction bits, clip from 32 to 16 bits */
            if (shift == 12) {
                xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(1 << (shift - 1)));
                xmm4 = _mm_add_epi32(xmm4, _mm_set1_epi32(1 << (shift - 1)));
            }
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);
            xmm4 = _mm_srai_epi32(xmm4, shift);
            xmm4 = _mm_packs_epi32(xmm4, xmm4);

            /* store 2 or 4 16-bit values */
            if (widthMul == 4) {
                _mm_storel_epi64((__m128i*)(pDst + 0*dstPitch), xmm0);
                _mm_storel_epi64((__m128i*)(pDst + 1*dstPitch), xmm4);
            } else if (widthMul == 2) {
                *(int *)(pDst + 0*dstPitch) = _mm_cvtsi128_si32(xmm0);
                *(int *)(pDst + 1*dstPitch) = _mm_cvtsi128_si32(xmm4);
            }

            /* shift interleaved row registers */
            xmm0 = xmm1;
            xmm4 = xmm5;
            xmm1 = xmm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += widthMul;
        pDstRef += widthMul;
    }
}

/* chroma, vertical, 16-bit input, 16-bit output */
void MAKE_NAME(h265_InterpChroma_s16_d16_V)(INTERP_S16_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 6 && offset == 0) || (shift == 12 && offset == (1 << (shift-1))) );
    VM_ASSERT( ((width & 0x01) == 0) && ((height & 0x01) == 0) );

    /* fast path - width multiple of 8 */
    if ((width & 0x07) == 0) {
        if (shift == 6)
            t_InterpChroma_s16_d16_V_AVX2< 6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 12)
            t_InterpChroma_s16_d16_V_AVX2<12>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        return;
    }

    rem = (width & 0x03);

    width -= rem;
    if (width > 0) {
        if (shift == 6)
            t_InterpChroma_s16_d16_V<4,  6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 12)
            t_InterpChroma_s16_d16_V<4, 12>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        pSrc += width;
        pDst += width;
    }

    if (rem == 2) {
        if (shift == 6)
            t_InterpChroma_s16_d16_V<2,  6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 12)
            t_InterpChroma_s16_d16_V<2, 12>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    }
}

/* single kernel for all 3 averaging modes 
 * template parameter avgBytes = [0,1,2] determines mode at compile time (no branches in inner loop)
 */
template <int widthMul, int avgBytes>
static void t_AverageMode(short *pSrc, unsigned int srcPitch, void *vpAvg, unsigned int avgPitch, unsigned char *pDst, unsigned int dstPitch, int width, int height)
{
    int col;
    unsigned char *pAvgC;
    short *pAvgS;
    __m128i xmm0, xmm1, xmm7;
    __m256i ymm0, ymm1, ymm7;

    _mm256_zeroupper();

    if (avgBytes == 1)
        pAvgC = (unsigned char *)vpAvg;
    else if (avgBytes == 2)
        pAvgS = (short *)vpAvg;

    do {
        if (widthMul == 16) {
            /* multiple of 16 */
            ymm7 = _mm256_set1_epi16(1 << 6);
            for (col = 0; col < width; col += widthMul) {
                /* load 16 16-bit pixels from source */
                ymm0 = _mm256_loadu_si256((__m256i *)(pSrc + col));

                if (avgBytes == 1) {
                    /* load 16 8-bit pixels from avg buffer, zero extend to 16-bit, normalize fraction bits */
                    xmm1 = _mm_loadu_si128((__m128i *)(pAvgC + col));
                    ymm1 = _mm256_cvtepu8_epi16(xmm1);
                    ymm1 = _mm256_slli_epi16(ymm1, 6);

                    ymm1 = _mm256_adds_epi16(ymm1, ymm7);
                    ymm0 = _mm256_adds_epi16(ymm0, ymm1);
                    ymm0 = _mm256_srai_epi16(ymm0, 7);
                } else if (avgBytes == 2) {
                    /* load 16 16-bit pixels from from avg buffer */
                    ymm1 = _mm256_loadu_si256((__m256i *)(pAvgS + col));

                    ymm1 = _mm256_adds_epi16(ymm1, ymm7);
                    ymm0 = _mm256_adds_epi16(ymm0, ymm1);
                    ymm0 = _mm256_srai_epi16(ymm0, 7);
                }
                
                ymm0 = _mm256_packus_epi16(ymm0, ymm0);
                ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
                _mm_storeu_si128((__m128i*)(pDst + col), _mm256_castsi256_si128(ymm0));
            }

        } else {
            /* multiple of 8 or less */
            xmm7 = _mm_set1_epi16(1 << 6);
            for (col = 0; col < width; col += widthMul) {
                /* load 8 16-bit pixels from source */
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
                
                if (avgBytes == 1) {
                    /* load 8 8-bit pixels from avg buffer, zero extend to 16-bit, normalize fraction bits */
                    xmm1 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(pAvgC + col));    
                    xmm1 = _mm_slli_epi16(xmm1, 6);

                    xmm1 = _mm_adds_epi16(xmm1, xmm7);
                    xmm0 = _mm_adds_epi16(xmm0, xmm1);
                    xmm0 = _mm_srai_epi16(xmm0, 7);
                } else if (avgBytes == 2) {
                    /* load 8 16-bit pixels from from avg buffer */
                    xmm1 = _mm_loadu_si128((__m128i *)(pAvgS + col));

                    xmm1 = _mm_adds_epi16(xmm1, xmm7);
                    xmm0 = _mm_adds_epi16(xmm0, xmm1);
                    xmm0 = _mm_srai_epi16(xmm0, 7);
                }

                xmm0 = _mm_packus_epi16(xmm0, xmm0);

                /* store 8 pixels */
                if (widthMul == 8) {
                    _mm_storel_epi64((__m128i*)(pDst + col), xmm0);
                } else if (widthMul == 6) {
                    *(short *)(pDst+col+0) = (short)_mm_extract_epi16(xmm0, 0);
                    *(short *)(pDst+col+2) = (short)_mm_extract_epi16(xmm0, 1);
                    *(short *)(pDst+col+4) = (short)_mm_extract_epi16(xmm0, 2);
                } else if (widthMul == 4) {
                    *(int   *)(pDst+col+0) = _mm_cvtsi128_si32(xmm0);
                } else if (widthMul == 2) {
                    *(short *)(pDst+col+0) = (short)_mm_extract_epi16(xmm0, 0);
                }
            }
        }

        pSrc += srcPitch;
        pDst += dstPitch;

        if (avgBytes == 1)
            pAvgC += avgPitch;
        else if (avgBytes == 2)
            pAvgS += avgPitch;
    } while (--height);
}
    
/* mode: AVERAGE_NO, just clip/pack 16-bit output to 8-bit */
void MAKE_NAME(h265_AverageModeN)(INTERP_AVG_NONE_PARAMETERS_LIST)
{
    if ( (width & 0x0f) == 0 ) {
        /* very fast path - multiple of 16 */
        t_AverageMode<16, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, width, height);
        return;
    } else if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode< 8, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, width, height);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode< 4, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 4, height); 
        return;
    case 12: 
        t_AverageMode< 8, 0>(pSrc,   srcPitch, 0, 0, pDst,   dstPitch, 8, height); 
        t_AverageMode< 4, 0>(pSrc+8, srcPitch, 0, 0, pDst+8, dstPitch, 4, height); 
        return;
    case  2: 
        t_AverageMode< 2, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 2, height); 
        return;
    case  6: 
        t_AverageMode< 6, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 6, height); 
        return;
    }
}

/* mode: AVERAGE_FROM_PIC, load 8-bit pixels, extend to 16-bit, add to current output, clip/pack 16-bit to 8-bit */
void MAKE_NAME(h265_AverageModeP)(INTERP_AVG_PIC_PARAMETERS_LIST)
{
    if ( (width & 0x0f) == 0 ) {
        /* very fast path - multiple of 16 */
        t_AverageMode<16, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
        return;
    } else if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode< 8, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode< 4, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 4, height); 
        return;
    case 12: 
        t_AverageMode< 8, sizeof(unsigned char)>(pSrc,   srcPitch, pAvg,   avgPitch, pDst,   dstPitch, 8, height); 
        t_AverageMode< 4, sizeof(unsigned char)>(pSrc+8, srcPitch, pAvg+8, avgPitch, pDst+8, dstPitch, 4, height); 
        return;
    case  2: 
        t_AverageMode< 2, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 2, height); 
        return;
    case  6: 
        t_AverageMode< 6, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 6, height); 
        return;
    }
}


/* mode: AVERAGE_FROM_BUF, load 16-bit pixels, add to current output, clip/pack 16-bit to 8-bit */
void MAKE_NAME(h265_AverageModeB)(INTERP_AVG_BUF_PARAMETERS_LIST)
{
    if ( (width & 0x0f) == 0 ) {
        /* very fast path - multiple of 16 */
        t_AverageMode<16, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
        return;
    } else if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode< 8, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode< 4, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 4, height); 
        return;
    case 12: 
        t_AverageMode< 8, sizeof(short)>(pSrc,   srcPitch, pAvg,   avgPitch, pDst,   dstPitch, 8, height); 
        t_AverageMode< 4, sizeof(short)>(pSrc+8, srcPitch, pAvg+8, avgPitch, pDst+8, dstPitch, 4, height); 
        return;
    case  2: 
        t_AverageMode< 2, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 2, height); 
        return;
    case  6: 
        t_AverageMode< 6, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 6, height); 
        return;
    }
}


} // end namespace MFX_HEVC_PP

#endif //#if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
