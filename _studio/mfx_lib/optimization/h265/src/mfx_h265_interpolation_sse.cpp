//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

/*
// HEVC interpolation filters, implemented with SSE packed multiply-add, optimized for Atom
// 
// NOTES:
// - requires at least SSE4.1 (pextrw, pmovzxbw)
// - requires that width = multiple of 4 (LUMA) or multiple of 2 (CHROMA)
// - requires that height = multiple of 2 for VERTICAL filters (processes 2 rows at a time to reduce shuffles)
// - horizontal filters can take odd heights, e.g. generating support pixels for vertical pass
// - input data is read in chunks of 8 or 16 bytes regardless of width, so assume it's safe to read up to 16 bytes beyond right edge of block 
//     (data not used, just must be allocated memory - pad inbuf as needed)
// - might require /Qsfalign option to ensure that ICL keeps 16-byte aligned stack when storing xmm registers
//     - have observed crashes when compiler (ICL 13) copies data from aligned ROM tables to non-aligned location on stack (compiler bug?)
// - templates used to reduce branches, so shift/offset values restricted to currently-used scale factors (see asserts below)
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_MAKENAME_ATOM)
    #define MFX_SSE_OPTIMIZATION_FOR_ATOM
#endif

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_SSE4) && defined(MFX_TARGET_OPTIMIZATION_SSE4) || \
    defined(MFX_MAKENAME_SSSE3) && defined(MFX_TARGET_OPTIMIZATION_SSSE3) || \
    defined(MFX_MAKENAME_SSSE3) && defined(MFX_TARGET_OPTIMIZATION_AVX2) || \
    defined(MFX_MAKENAME_ATOM) && defined(MFX_TARGET_OPTIMIZATION_ATOM)

#include <immintrin.h>

#ifdef __INTEL_COMPILER
/* disable warning: unused function parameter (offset not needed with template implementation) */
#pragma warning( disable : 869 )
#endif

#ifdef MFX_EMULATE_SSSE3
#include "mfx_ssse3_emulation.h"
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
ALIGN_DECL(16) static const signed char filtTabLuma_S8[3*4][16] = {
    {  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58 },
    {  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5 },
    {   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0 },

    {  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40 },
    {  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11 },
    {   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1 },

    {   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1 },
    {  -5,   17, -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17 },
    {  58,  -10, 58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10 },
    {   4,   -1,  4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1 },

};

//kolya
/* interleaved luma interp coefficients, 8-bit, for offsets 1/4, 2/4, 3/4 */
ALIGN_DECL(16) static const signed char filtTabLumaFast_S8[3*2][16] = {
    {  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52 },
    {  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4 },

    {  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40 },
    {  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8 },

    {  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20 },
    {  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4 }
};

/* interleaved chroma interp coefficients, 8-bit, for offsets 1/8, 2/8, ... 7/8 */
ALIGN_DECL(16) static const signed char filtTabChroma_S8[7*2][16] = {
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

/* interleaved luma interp coefficients, 16-bit, for offsets 1/4, 2/4, 3/4 */
ALIGN_DECL(16) static const short filtTabLuma_S16[3*4][8] = {
    {  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -10,  58, -10,  58, -10,  58, -10,  58 },
    {  17,  -5,  17,  -5,  17,  -5,  17,  -5 },
    {   1,   0,   1,   0,   1,   0,   1,   0 },

    {  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -11,  40, -11,  40, -11,  40, -11,  40 },
    {  40, -11,  40, -11,  40, -11,  40, -11 },
    {   4,  -1,   4,  -1,   4,  -1,   4,  -1 },

    {   0,   1,   0,   1,   0,   1,   0,   1 },
    {  -5,   17, -5,  17,  -5,  17,  -5,  17 },
    {  58,  -10, 58, -10,  58, -10,  58, -10 },
    {   4,   -1,  4,  -1,   4,  -1,   4,  -1 },

};

//kolya
/* interleaved chroma interp coefficients, 16-bit, for offsets 1/8, 2/8, ... 7/8 */
ALIGN_DECL(16) static const short filtTabLumaFast_S16[3*2][8] = {
    {  -4,  52,  -4,  52,  -4,  52,  -4,  52 },
    {  20,  -4,  20,  -4,  20,  -4,  20,  -4 },

    { -8,  40,  -8,  40,  -8,  40,  -8,  40 },
    { 40,  -8,  40,  -8,  40,  -8,  40,  -8 },

    {  -4,  20,  -4,  20,  -4,  20,  -4,  20 },
    {  52,  -4,  52,  -4,  52,  -4,  52,  -4 }
};

/* interleaved chroma interp coefficients, 16-bit, for offsets 1/8, 2/8, ... 7/8 */
ALIGN_DECL(16) static const short filtTabChroma_S16[7*2][8] = {
    {  -2,  58,  -2,  58,  -2,  58,  -2,  58 },
    {  10,  -2,  10,  -2,  10,  -2,  10,  -2 },

    {  -4,  54,  -4,  54,  -4,  54,  -4,  54 },
    {  16,  -2,  16,  -2,  16,  -2,  16,  -2 },

    {  -6,  46,  -6,  46,  -6,  46,  -6,  46 },
    {  28,  -4,  28,  -4,  28,  -4,  28,  -4 },

    {  -4,  36,  -4,  36,  -4,  36,  -4,  36 },
    {  36,  -4,  36,  -4,  36,  -4,  36,  -4 },

    {  -4,  28,  -4,  28,  -4,  28,  -4,  28 }, 
    {  46,  -6,  46,  -6,  46,  -6,  46,  -6 },

    {  -2,  16,  -2,  16,  -2,  16,  -2,  16 },
    {  54,  -4,  54,  -4,  54,  -4,  54,  -4 },

    {  -2,  10,  -2,  10,  -2,  10,  -2,  10 },
    {  58,  -2,  58,  -2,  58,  -2,  58,  -2 },
};

/* pshufb table for luma, 8-bit horizontal filtering */
ALIGN_DECL(16) static const signed char shufTabPlane[4][16] = {
    {  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8 },
    {  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10 },
    {  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12 },
    {  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14 },
};

/* pshufb table for luma, 16-bit horizontal filtering */
ALIGN_DECL(16) static const signed char shufTabPlane16[2][16] = {
    {  0,  1,  2,  3,  2,  3,  4,  5,  4,  5,  6,  7,  6,  7,  8,  9 },
    {  4,  5,  6,  7,  6,  7,  8,  9,  8,  9, 10, 11, 10, 11, 12, 13 },
};

/* pshufb table for interleaved chroma, 8-bit horizontal filtering */
ALIGN_DECL(16) static const signed char shufTabIntUV[2][16] = {
    {  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7,  9 },
    {  4,  6,  5,  7,  6,  8,  7,  9,  8, 10,  9, 11, 10, 12, 11, 13 },
};

/* pshufb table for interleaved chroma, 16-bit horizontal filtering */
ALIGN_DECL(16) static const signed char shufTabIntUV16[2][16] = {
    {  0,  1,  4,  5,  2,  3,  6,  7,  4,  5,  8,  9,  6,  7, 10, 11 },
    {  0,  1,  4,  5,  2,  3,  6,  7,  4,  5,  8,  9,  6,  7, 10, 11 },
};

template<int widthMul, int shift>
static void t_InterpLuma_s8_d16_H(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3;

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
            xmm1 = _mm_maddubs_epi16(xmm1, *(__m128i *)(coeffs + 16));    /* coefs 2,3 */
            xmm2 = _mm_maddubs_epi16(xmm2, *(__m128i *)(coeffs + 32));    /* coefs 4,5 */
            xmm3 = _mm_maddubs_epi16(xmm3, *(__m128i *)(coeffs + 48));    /* coefs 6,7 */

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

//kolya
template<int widthMul, int shift>
static void t_InterpLumaFast_s8_d16_H(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    const signed char* shufTab;
    __m128i xmm0, xmm1, xmm2, xmm3;

    coeffs = filtTabLumaFast_S8[2 * (tab_index-1)];
    shufTab = (signed char *)shufTabPlane;

    xmm2 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm3 = _mm_load_si128((__m128i *)(coeffs + 16));

    /* calculate 8 outputs per inner loop, working horizontally - use shuffle/interleave instead of pshufb on Atom */
    do {
        for (col = 0; col < width; col += widthMul) {
            /* load 16 8-bit pixels */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
            xmm1 = xmm0;

            /* interleave pixels */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTab +  0));
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTab + 16));

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
            if (widthMul == 8)
                _mm_storeu_si128((__m128i *)(pDst + col), xmm0);
            else if (widthMul == 4)
                _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

#if !defined MFX_SSE_OPTIMIZATION_FOR_ATOM
/* luma, horizontal, 8-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLuma_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x03) == 0 );

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

//kolya
void MAKE_NAME(h265_InterpLumaFast_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x03) == 0 );

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpLumaFast_s8_d16_H<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLumaFast_s8_d16_H<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        pSrc += width;
        pDst += width;
    }

    if (rem > 0) {
        if (shift == 0)
            t_InterpLumaFast_s8_d16_H<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpLumaFast_s8_d16_H<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    }
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
    xmm3 = _mm_load_si128((__m128i *)(coeffs + 16));

    /* calculate 8 outputs per inner loop, working horizontally - use shuffle/interleave instead of pshufb on Atom */
    do {
        /* same version for U/V interleaved or separate planes */
        for (col = 0; col < width; col += widthMul) {
            /* load 16 8-bit pixels */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
            xmm1 = xmm0;

            /* interleave pixels */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTab +  0));
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTab + 16));

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

template<int shift, int offset>
static void t_InterpLuma_s16_d16_H(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3;

    coeffs = (const signed char *)filtTabLuma_S16[4 * (tab_index-1)];

    /* calculate 4 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 4) {
            /* load 8 16-bit pixels */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* words 0-7 */
            xmm2 = _mm_loadl_epi64((__m128i *)(pSrc + col + 8));    /* words 8-11 */
            xmm2 = _mm_alignr_epi8(xmm2, xmm0, 8);                  /* words 4-11 */
            xmm1 = xmm0;
            xmm3 = xmm2;

            /* interleave pixels */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabPlane16[0]));
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabPlane16[1]));
            xmm2 = _mm_shuffle_epi8(xmm2, *(__m128i *)(shufTabPlane16[0]));
            xmm3 = _mm_shuffle_epi8(xmm3, *(__m128i *)(shufTabPlane16[1]));

            /* packed (16*16 + 16*16) -> 32 */
            xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
            xmm1 = _mm_madd_epi16(xmm1, *(__m128i *)(coeffs + 16));    /* coefs 2,3 */
            xmm2 = _mm_madd_epi16(xmm2, *(__m128i *)(coeffs + 32));    /* coefs 4,5 */
            xmm3 = _mm_madd_epi16(xmm3, *(__m128i *)(coeffs + 48));    /* coefs 6,7 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi32(xmm0, xmm1);
            xmm0 = _mm_add_epi32(xmm0, xmm2);
            xmm0 = _mm_add_epi32(xmm0, xmm3);
            if (shift == 6) {
                xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            } 
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);

            /* store 4 16-bit words */
            _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

//kolya
//TODO: not verified, needed 10-bit to test
template<int shift, int offset>
static void t_InterpLumaFast_s16_d16_H(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1;

    coeffs = (const signed char *)filtTabLumaFast_S16[2 * (tab_index-1)];

    /* calculate 4 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 4) {
            /* load, interleave 8 16-bit pixels */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* words 0-7 */
            xmm1 = xmm0;
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabPlane16[0]));
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabPlane16[1]));


            /* packed (16*16 + 16*16) -> 32 */
            xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
            xmm1 = _mm_madd_epi16(xmm1, *(__m128i *)(coeffs + 16));    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi32(xmm0, xmm1);
            if (shift == 6) {
                xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            } 
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);

            _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

/* luma, horizontal, 16-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLuma_s16_d16_H)(INTERP_S16_D16_PARAMETERS_LIST)
{
    VM_ASSERT( (shift <= 2 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x03) == 0 );

    switch (shift) {
    case 6:  t_InterpLuma_s16_d16_H<6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

    case 0:  t_InterpLuma_s16_d16_H<0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 1:  t_InterpLuma_s16_d16_H<1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 2:  t_InterpLuma_s16_d16_H<2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    }
}

//kolya
void MAKE_NAME(h265_InterpLumaFast_s16_d16_H)(INTERP_S16_D16_PARAMETERS_LIST)
{
    VM_ASSERT( (shift <= 2 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x03) == 0 );

    switch (shift) {
    case 6:  t_InterpLumaFast_s16_d16_H<6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

    case 0:  t_InterpLumaFast_s16_d16_H<0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 1:  t_InterpLumaFast_s16_d16_H<1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 2:  t_InterpLumaFast_s16_d16_H<2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    }
}

template<int plane, int widthMul, int shift, int offset>
static void t_InterpChroma_s16_d16_H(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1;

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    /* calculate 4 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 4) {
            /* load, interleave 8 16-bit pixels */
            if (plane == TEXT_CHROMA) {
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* words 0-7 */
                xmm1 = _mm_loadl_epi64((__m128i *)(pSrc + col + 8));    /* words 8-11 */
                xmm1 = _mm_alignr_epi8(xmm1, xmm0, 8);                  /* words 4-11 */
                xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabIntUV16[0]));
                xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabIntUV16[1]));
            } else {
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* words 0-7 */
                xmm1 = xmm0;
                xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabPlane16[0]));
                xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabPlane16[1]));
            }

            /* packed (16*16 + 16*16) -> 32 */
            xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
            xmm1 = _mm_madd_epi16(xmm1, *(__m128i *)(coeffs + 16));    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi32(xmm0, xmm1);
            if (shift == 6) {
                xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            } 
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);

            if (widthMul == 4) {
                /* store 4 16-bit words */
                _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
            } else if (widthMul == 2) {
                /* store 2 16-bit words */
                *(int *)(pDst+col+0) = _mm_cvtsi128_si32(xmm0);
            }
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

/* chroma, horizontal, 16-bit input, 16-bit output */
void MAKE_NAME(h265_InterpChroma_s16_d16_H)(INTERP_S16_D16_PARAMETERS_LIST, int plane)
{
    int rem;

    VM_ASSERT( (shift <= 2 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x01) == 0 );

    rem = (width & 0x03);
    width -= rem;

    if (plane == TEXT_CHROMA) {
        if (width > 0) {
            switch (shift) {
            case 6:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   4, 6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

            case 0:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   4, 0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 1:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   4, 1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 2:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   4, 2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            }
            pSrc += width;
            pDst += width;
        }

        if (rem == 2) {
            switch (shift) {
            case 6:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   2, 6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;

            case 0:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   2, 0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            case 1:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   2, 1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            case 2:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   2, 2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            }
        }
    } else {
        if (width > 0) {
            switch (shift) {
            case 6:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 4, 6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

            case 0:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 4, 0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 1:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 4, 1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 2:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 4, 2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            }
            pSrc += width;
            pDst += width;
        }

        if (rem == 2) {
            switch (shift) {
            case 6:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 2, 6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;

            case 0:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 2, 0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            case 1:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 2, 1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            case 2:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 2, 2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            }
        }
    }
}
#endif //!defined MFX_SSE_OPTIMIZATION_FOR_ATOM

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
            xmm4 = _mm_maddubs_epi16(xmm4, *(__m128i*)(coeffs + 16));
            xmm0 = _mm_add_epi16(xmm0, xmm4);
            xmm5 = _mm_maddubs_epi16(xmm5, *(__m128i*)(coeffs + 32));
            xmm0 = _mm_add_epi16(xmm0, xmm5);
            xmm6 = _mm_maddubs_epi16(xmm6, *(__m128i*)(coeffs + 48));
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
            xmm4 = _mm_maddubs_epi16(xmm4, *(__m128i*)(coeffs + 16));
            xmm0 = _mm_add_epi16(xmm0, xmm4);
            xmm5 = _mm_maddubs_epi16(xmm5, *(__m128i*)(coeffs + 32));
            xmm0 = _mm_add_epi16(xmm0, xmm5);
            xmm6 = _mm_maddubs_epi16(xmm6, *(__m128i*)(coeffs + 48));
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

//kolya
template<int widthMul, int shift>
static void t_InterpLumaFast_s8_d16_V(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = filtTabLumaFast_S8[2 * (tab_index-1)];

    for (col = 0; col < width; col += widthMul) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-2 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));

        xmm6 = _mm_load_si128((__m128i *)(coeffs +  0));
        xmm7 = _mm_load_si128((__m128i *)(coeffs + 16));

        xmm0 = _mm_unpacklo_epi8(xmm0, xmm4);    /* interleave 01 */
        xmm4 = _mm_unpacklo_epi8(xmm4, xmm1);    /* interleave 12 */

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
                xmm0 = _mm_srai_epi16(xmm0, shift);
                xmm4 = _mm_srai_epi16(xmm4, shift);
            }

            /* store 4 or 8 16-bit words */
            if (widthMul == 8)
            {
                _mm_storeu_si128((__m128i *)(pDst + 0*dstPitch), xmm0);
                _mm_storeu_si128((__m128i *)(pDst + 1*dstPitch), xmm4);
            }
            else if (widthMul == 4)
            {
                _mm_storel_epi64((__m128i *)(pDst + 0*dstPitch), xmm0);
                _mm_storel_epi64((__m128i *)(pDst + 1*dstPitch), xmm4);
            }
            /* shift interleaved row registers (23->01, 45->23, etc.) */
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

/* luma, vertical, 8-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLuma_s8_d16_V)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( ((width & 0x03) == 0) && ((height & 0x01) == 0) );

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

//kolya
void MAKE_NAME(h265_InterpLumaFast_s8_d16_V)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( ((width & 0x03) == 0) && ((height & 0x01) == 0) );

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpLumaFast_s8_d16_V<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLumaFast_s8_d16_V<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        pSrc += width;
        pDst += width;
    }

    if (rem > 0) {
        if (shift == 0)
            t_InterpLumaFast_s8_d16_V<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpLumaFast_s8_d16_V<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
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
    xmm7 = _mm_load_si128((__m128i *)(coeffs + 16));

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

template<int shift, int offset>
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
            xmm4 = _mm_madd_epi16(xmm4, *(__m128i*)(coeffs + 16));
            xmm0 = _mm_add_epi32(xmm0, xmm4);
            xmm5 = _mm_madd_epi16(xmm5, *(__m128i*)(coeffs + 32));
            xmm0 = _mm_add_epi32(xmm0, xmm5);
            xmm6 = _mm_madd_epi16(xmm6, *(__m128i*)(coeffs + 48));
            xmm0 = _mm_add_epi32(xmm0, xmm6);

            /* add offset, shift off fraction bits, clip from 32 to 16 bits */
            if (shift > 0)
            {
            xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            xmm0 = _mm_srai_epi32(xmm0, shift);
            }
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
            xmm4 = _mm_madd_epi16(xmm4, *(__m128i*)(coeffs + 16));
            xmm0 = _mm_add_epi32(xmm0, xmm4);
            xmm5 = _mm_madd_epi16(xmm5, *(__m128i*)(coeffs + 32));
            xmm0 = _mm_add_epi32(xmm0, xmm5);
            xmm6 = _mm_madd_epi16(xmm6, *(__m128i*)(coeffs + 48));
            xmm0 = _mm_add_epi32(xmm0, xmm6);

            /* add offset, shift off fraction bits, clip from 32 to 16 bits */
            if (shift > 0)
            {
            xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            xmm0 = _mm_srai_epi32(xmm0, shift);
            }
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

//kolya
template<int shift, int offset>
static void t_InterpLumaFast_s16_d16_V(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = (const signed char *)filtTabLumaFast_S16[2 * (tab_index-1)];

    xmm6 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm7 = _mm_load_si128((__m128i *)(coeffs + 16));

    /* always calculates 4 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 4) {
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
            xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            xmm4 = _mm_add_epi32(xmm4, _mm_set1_epi32(offset));
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);
            xmm4 = _mm_srai_epi32(xmm4, shift);
            xmm4 = _mm_packs_epi32(xmm4, xmm4);

            _mm_storel_epi64((__m128i*)(pDst + 0*dstPitch), xmm0);
            _mm_storel_epi64((__m128i*)(pDst + 1*dstPitch), xmm4);

            /* shift interleaved row registers */
            xmm0 = xmm1;
            xmm4 = xmm5;
            xmm1 = xmm2;

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
    if (shift == 6) {
        VM_ASSERT(offset == 0 || offset == 32);
    } else if (shift >= 10 && shift <= 12) {
        VM_ASSERT(offset == (1 << (shift-1)));
    } else if (shift <= 2) {
        VM_ASSERT(offset == 0);
    } else {
        VM_ASSERT(0);    /* error - not supported */
    }
    VM_ASSERT( ((width & 0x03) == 0) && ((height & 0x01) == 0) );

    switch (shift) {
    case 6:
        if (offset == 0)        t_InterpLuma_s16_d16_V< 6,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else   t_InterpLuma_s16_d16_V< 6,   32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        break;
    
    case 12:                    t_InterpLuma_s16_d16_V<12, 2048>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    }

    switch (shift)
    {
    case 10:                    t_InterpLuma_s16_d16_V<10,  512>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 11:                    t_InterpLuma_s16_d16_V<11, 1024>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case  1:                    t_InterpLuma_s16_d16_V< 1,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case  2:                    t_InterpLuma_s16_d16_V< 2,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case  0:                    t_InterpLuma_s16_d16_V< 0,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    }
}

//kolya
void MAKE_NAME(h265_InterpLumaFast_s16_d16_V)(INTERP_S16_D16_PARAMETERS_LIST)
{
    if (shift == 6) {
        VM_ASSERT(offset == 0 || offset == 32);
    } else if (shift >= 10 && shift <= 12) {
        VM_ASSERT(offset == (1 << (shift-1)));
    } else if (shift <= 2) {
        VM_ASSERT(offset == 0);
    } else {
        VM_ASSERT(0);    /* error - not supported */
    }
    VM_ASSERT( ((width & 0x03) == 0) && ((height & 0x01) == 0) );

    switch (shift) {
    case 6:
        if (offset == 0)        t_InterpLumaFast_s16_d16_V< 6,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else   t_InterpLumaFast_s16_d16_V< 6,   32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        break;
    
    case 12:                    t_InterpLumaFast_s16_d16_V<12, 2048>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    }

    switch (shift)
    {
    case 10:                    t_InterpLumaFast_s16_d16_V<10,  512>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 11:                    t_InterpLumaFast_s16_d16_V<11, 1024>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case  1:                    t_InterpLumaFast_s16_d16_V< 1,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case  2:                    t_InterpLumaFast_s16_d16_V< 2,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case  0:                    t_InterpLumaFast_s16_d16_V< 0,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    }
}

template<int widthMul, int shift, int offset>
static void t_InterpChroma_s16_d16_V(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    xmm6 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm7 = _mm_load_si128((__m128i *)(coeffs + 16));

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
            xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            xmm4 = _mm_add_epi32(xmm4, _mm_set1_epi32(offset));
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

    if (shift == 6) {
        VM_ASSERT(offset == 0 || offset == 32);
    } else if (shift >= 10 && shift <= 12) {
        VM_ASSERT(offset == (1 << (shift-1)));
    } else if (shift <= 2) {
        VM_ASSERT(offset == 0);
    } else {
        VM_ASSERT(0);    /* error - not supported */
    }
    VM_ASSERT( ((width & 0x01) == 0) && ((height & 0x01) == 0) );

    rem = (width & 0x03);

    width -= rem;
    if (width > 0) {
        switch (shift) {
        case 6:
            if (offset == 0)        t_InterpChroma_s16_d16_V<4,  6,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
            else if (offset == 32)  t_InterpChroma_s16_d16_V<4,  6,   32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
            break;
    
        case 10:                    t_InterpChroma_s16_d16_V<4, 10,  512>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 11:                    t_InterpChroma_s16_d16_V<4, 11, 1024>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 12:                    t_InterpChroma_s16_d16_V<4, 12, 2048>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    
        case  1:                    t_InterpChroma_s16_d16_V<4,  1,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case  2:                    t_InterpChroma_s16_d16_V<4,  2,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case  0:                    t_InterpChroma_s16_d16_V<4,  0,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
       }
        pSrc += width;
        pDst += width;
    }

    if (rem == 2) {
        switch (shift) {
        case 6:
            if (offset == 0)        t_InterpChroma_s16_d16_V<2,  6,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);
            else if (offset == 32)  t_InterpChroma_s16_d16_V<2,  6,   32>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);
            break;
    
        case 10:                    t_InterpChroma_s16_d16_V<2, 10,  512>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
        case 11:                    t_InterpChroma_s16_d16_V<2, 11, 1024>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
        case 12:                    t_InterpChroma_s16_d16_V<2, 12, 2048>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
    
        case  1:                    t_InterpChroma_s16_d16_V<2,  1,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
        case  2:                    t_InterpChroma_s16_d16_V<2,  2,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
        case  0:                    t_InterpChroma_s16_d16_V<2,  0,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
        }
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

    if (avgBytes == 1)
        pAvgC = (unsigned char *)vpAvg;
    else if (avgBytes == 2)
        pAvgS = (short *)vpAvg;

    xmm7 = _mm_set1_epi16(1 << 6);
    do {
        for (col = 0; col < width; col += widthMul) {
            /* load 8 16-bit pixels from source */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
                
            if (avgBytes == 1) {
                /* load 8 8-bit pixels from avg buffer, zero extend to 16-bit, normalize fraction bits */
                xmm1 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(pAvgC + col));    
                xmm1 = _mm_slli_epi16(xmm1, 6);

                xmm1 = _mm_adds_epi16(xmm1, xmm0);
                xmm0 = _mm_adds_epi16(xmm7, xmm1);
                xmm0 = _mm_srai_epi16(xmm0, 7);
            } else if (avgBytes == 2) {
                /* load 8 16-bit pixels from from avg buffer */
                xmm1 = _mm_loadu_si128((__m128i *)(pAvgS + col));

                xmm1 = _mm_adds_epi16(xmm1, xmm0);
                xmm0 = _mm_adds_epi16(xmm7, xmm1);
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
    if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode<8, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, width, height);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode<4, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 4, height); 
        return;
    case 12: 
        t_AverageMode<8, 0>(pSrc,   srcPitch, 0, 0, pDst,   dstPitch, 8, height); 
        t_AverageMode<4, 0>(pSrc+8, srcPitch, 0, 0, pDst+8, dstPitch, 4, height); 
        return;
    case  2: 
        t_AverageMode<2, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 2, height); 
        return;
    case  6: 
        t_AverageMode<6, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 6, height); 
        return;
    }
}

/* mode: AVERAGE_FROM_PIC, load 8-bit pixels, extend to 16-bit, add to current output, clip/pack 16-bit to 8-bit */
void MAKE_NAME(h265_AverageModeP)(INTERP_AVG_PIC_PARAMETERS_LIST)
{
    if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode<8, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode<4, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 4, height); 
        return;
    case 12: 
        t_AverageMode<8, sizeof(unsigned char)>(pSrc,   srcPitch, pAvg,   avgPitch, pDst,   dstPitch, 8, height); 
        t_AverageMode<4, sizeof(unsigned char)>(pSrc+8, srcPitch, pAvg+8, avgPitch, pDst+8, dstPitch, 4, height); 
        return;
    case  2: 
        t_AverageMode<2, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 2, height); 
        return;
    case  6: 
        t_AverageMode<6, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 6, height); 
        return;
    }
}


/* mode: AVERAGE_FROM_BUF, load 16-bit pixels, add to current output, clip/pack 16-bit to 8-bit */
void MAKE_NAME(h265_AverageModeB)(INTERP_AVG_BUF_PARAMETERS_LIST)
{
    if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode<8, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode<4, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 4, height); 
        return;
    case 12: 
        t_AverageMode<8, sizeof(short)>(pSrc,   srcPitch, pAvg,   avgPitch, pDst,   dstPitch, 8, height); 
        t_AverageMode<4, sizeof(short)>(pSrc+8, srcPitch, pAvg+8, avgPitch, pDst+8, dstPitch, 4, height); 
        return;
    case  2: 
        t_AverageMode<2, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 2, height); 
        return;
    case  6: 
        t_AverageMode<6, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 6, height); 
        return;
    }
}

/* single kernel for all 3 averaging modes 
 * template parameter avgMode = [0,1,2] determines mode at compile time (no branches in inner loop)
 */
template <int widthMul, int avgMode, int bitDepth>
static void t_AverageMode_U16_Kernel(short *pSrc, unsigned int srcPitch, unsigned short *pAvg, unsigned int avgPitch, unsigned short *pDst, unsigned int dstPitch, int width, int height)
{
    int col;
    const int shift = 15 - bitDepth;
    __m128i xmm0, xmm1, xmm4, xmm5, xmm7;

    xmm4 = _mm_setzero_si128();                   /* min */
    xmm5 = _mm_set1_epi16((1 << bitDepth) - 1);   /* max */
    xmm7 = _mm_set1_epi16(1 << (shift - 1));      /* round */

    do {
        for (col = 0; col < width; col += widthMul) {
            /* load 8 16-bit pixels from source */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
                
            if (avgMode == 1) {
                /* mode P: load 8 16-bit pixels from avg buffer, normalize and add */
                xmm1 = _mm_loadu_si128((__m128i *)(pAvg + col));
                xmm1 = _mm_slli_epi16(xmm1, shift - 1);
                xmm1 = _mm_adds_epi16(xmm1, xmm0);
                xmm0 = _mm_adds_epi16(xmm7, xmm1);
                xmm0 = _mm_srai_epi16(xmm0, shift);
            } else if (avgMode == 2) {
                /* mode B: load 8 16-bit pixels from from avg buffer */
                xmm1 = _mm_loadu_si128((__m128i *)(pAvg + col));
                xmm1 = _mm_adds_epi16(xmm1, xmm0);
                xmm0 = _mm_adds_epi16(xmm7, xmm1);
                xmm0 = _mm_srai_epi16(xmm0, shift);
            }
            xmm0 = _mm_max_epi16(xmm0, xmm4); 
            xmm0 = _mm_min_epi16(xmm0, xmm5);

            /* store 2,4,6,8 16-bit pixels */
            if (widthMul == 8) {
                _mm_storeu_si128((__m128i*)(pDst + col), xmm0);
            } else if (widthMul == 6) {
                _mm_storel_epi64((__m128i*)(pDst + col), xmm0);
                *(short *)(pDst+col+4) = (short)_mm_extract_epi16(xmm0, 4);
                *(short *)(pDst+col+5) = (short)_mm_extract_epi16(xmm0, 5);
            } else if (widthMul == 4) {
                _mm_storel_epi64((__m128i*)(pDst + col), xmm0);
            } else if (widthMul == 2) {
                *(short *)(pDst+col+0) = (short)_mm_extract_epi16(xmm0, 0);
                *(short *)(pDst+col+1) = (short)_mm_extract_epi16(xmm0, 1);
            }
        }
        pSrc += srcPitch;
        pDst += dstPitch;
        pAvg += avgPitch;

    } while (--height);
}

/* wrapper to make bit depth const, allow >> by immediate in kernel (faster) */
template <int widthMul, int avgMode>
static void t_AverageMode_U16(short *pSrc, unsigned int srcPitch, unsigned short *pAvg, unsigned int avgPitch, unsigned short *pDst, unsigned int dstPitch, int width, int height, int bitDepth)
{
    if (bitDepth == 9)
        t_AverageMode_U16_Kernel<widthMul, avgMode,  9>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
    else if (bitDepth == 10)
        t_AverageMode_U16_Kernel<widthMul, avgMode, 10>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
    else if (bitDepth == 8)
        t_AverageMode_U16_Kernel<widthMul, avgMode, 8>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
    else if (bitDepth == 11)
        t_AverageMode_U16_Kernel<widthMul, avgMode, 11>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
    else if (bitDepth == 12)
        t_AverageMode_U16_Kernel<widthMul, avgMode, 12>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
}

/* mode: AVERAGE_NO, just clip/pack 16-bit output to [0, 2^bitDepth) */
void MAKE_NAME(h265_AverageModeN_U16)(INTERP_AVG_NONE_PARAMETERS_LIST_U16)
{
    if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode_U16<8, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, width, height, bit_depth);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode_U16<4, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 4, height, bit_depth); 
        return;
    case 12: 
        t_AverageMode_U16<8, 0>(pSrc,   srcPitch, 0, 0, pDst,   dstPitch, 8, height, bit_depth); 
        t_AverageMode_U16<4, 0>(pSrc+8, srcPitch, 0, 0, pDst+8, dstPitch, 4, height, bit_depth); 
        return;
    case  2: 
        t_AverageMode_U16<2, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 2, height, bit_depth); 
        return;
    case  6: 
        t_AverageMode_U16<6, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 6, height, bit_depth); 
        return;
    }
}

/* mode: AVERAGE_FROM_PIC, load 16-bit pixels, add to current output, clip/pack 16-bit output to [0, 2^bitDepth) */
void MAKE_NAME(h265_AverageModeP_U16)(INTERP_AVG_PIC_PARAMETERS_LIST_U16)
{
    if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode_U16<8, 1>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height, bit_depth);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode_U16<4, 1>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 4, height, bit_depth); 
        return;
    case 12: 
        t_AverageMode_U16<8, 1>(pSrc,   srcPitch, pAvg,   avgPitch, pDst,   dstPitch, 8, height, bit_depth); 
        t_AverageMode_U16<4, 1>(pSrc+8, srcPitch, pAvg+8, avgPitch, pDst+8, dstPitch, 4, height, bit_depth); 
        return;
    case  2: 
        t_AverageMode_U16<2, 1>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 2, height, bit_depth); 
        return;
    case  6: 
        t_AverageMode_U16<6, 1>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 6, height, bit_depth); 
        return;
    }
}


/* mode: AVERAGE_FROM_BUF, load 16-bit pixels, add to current output, clip/pack 16-bit output to [0, 2^bitDepth) */
void MAKE_NAME(h265_AverageModeB_U16)(INTERP_AVG_BUF_PARAMETERS_LIST_U16)
{
    if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode_U16<8, 2>(pSrc, srcPitch, (unsigned short *)pAvg, avgPitch, pDst, dstPitch, width, height, bit_depth);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode_U16<4, 2>(pSrc, srcPitch, (unsigned short *)pAvg, avgPitch, pDst, dstPitch, 4, height, bit_depth); 
        return;
    case 12: 
        t_AverageMode_U16<8, 2>(pSrc,   srcPitch, (unsigned short *)pAvg,   avgPitch, pDst,   dstPitch, 8, height, bit_depth); 
        t_AverageMode_U16<4, 2>(pSrc+8, srcPitch, (unsigned short *)pAvg+8, avgPitch, pDst+8, dstPitch, 4, height, bit_depth); 
        return;
    case  2: 
        t_AverageMode_U16<2, 2>(pSrc, srcPitch, (unsigned short *)pAvg, avgPitch, pDst, dstPitch, 2, height, bit_depth); 
        return;
    case  6: 
        t_AverageMode_U16<6, 2>(pSrc, srcPitch, (unsigned short *)pAvg, avgPitch, pDst, dstPitch, 6, height, bit_depth); 
        return;
    }
}


// pack intermediate interpolation pels s16 to u8/u16 [ dst = Saturate( (src + 32) >> 6, 0, (1 << bitDepth) - 1 ) ]
template <int shift, typename PixType>
void h265_InterpLumaPack_kernel(const short *src, int pitchSrc, PixType *dst, int pitchDst, int width, int height, int bitDepth)
{
    width = (width + 7) & ~7; // 128-bit registers
    //VM_ASSERT(width <= pitchSrc);
    //VM_ASSERT(width <= pitchDst);

    __m128i line, minPel, maxPel;
    __m128i offset = _mm_set1_epi16(1 << (shift - 1));

    if (sizeof(PixType) == 2) {
        minPel = _mm_setzero_si128();
        maxPel = _mm_set1_epi16((1 << bitDepth) - 1);
    }

    for (; height > 0; height--, src += pitchSrc, dst += pitchDst) {
        for (Ipp32s col = 0; col < width; col += 8) {
            line = _mm_loadu_si128((__m128i *)(src + col));

            line = _mm_add_epi16(line, offset);
            line = _mm_srai_epi16(line, shift);

            if (sizeof(PixType) == 1) {
                line = _mm_packus_epi16(line, line);
                _mm_storel_epi64((__m128i*)(dst + col), line);
            }
            else {
                line = _mm_max_epi16(line, minPel); 
                line = _mm_min_epi16(line, maxPel);
                _mm_storeu_si128((__m128i*)(dst + col), line);
            }
        }
    }
}


// pack intermediate interpolation pels s16 to u8/u16 [ dst = Saturate( (src + 32) >> 6, 0, (1 << bitDepth) - 1 ) ]
template <typename PixType>
void MAKE_NAME(h265_InterpLumaPack)(const short *src, int pitchSrc, PixType *dst, int pitchDst, int width, int height, int bitDepth)
{
    switch (bitDepth) {
    case  8: h265_InterpLumaPack_kernel<6>(src, pitchSrc, dst, pitchDst, width, height,  8); break; // (+32) >> 6
    case  9: h265_InterpLumaPack_kernel<5>(src, pitchSrc, dst, pitchDst, width, height,  9); break; // (+16) >> 5
    case 10: h265_InterpLumaPack_kernel<4>(src, pitchSrc, dst, pitchDst, width, height, 10); break; // ( +8) >> 4
    default: VM_ASSERT(!"unexpected shift");
    }
}
template void MAKE_NAME(h265_InterpLumaPack)<unsigned char >(const short*, int, unsigned char  *dst, int, int, int, int);
template void MAKE_NAME(h265_InterpLumaPack)<unsigned short>(const short*, int, unsigned short *dst, int, int, int, int);

void MAKE_NAME(h265_ConvertShiftR)(const short *src, int pitchSrc, unsigned char *dst, int pitchDst, int width, int height, int rshift)
{
    width = (width + 7) & ~7; // 128-bit registers
    //VM_ASSERT(width <= pitchSrc);
    //VM_ASSERT(width <= pitchDst);

    __m128i line;
    __m128i offset = _mm_set1_epi16(1 << (rshift - 1));

    for (; height > 0; height--, src += pitchSrc, dst += pitchDst) {
        for (Ipp32s col = 0; col < width; col += 8) {
            line = _mm_loadu_si128((__m128i *)(src + col));

            line = _mm_add_epi16(line, offset);
            line = _mm_srai_epi16(line, rshift);

            line = _mm_packus_epi16(line, line);
            _mm_storel_epi64((__m128i*)(dst + col), line);
        }
    }
}

} // end namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
