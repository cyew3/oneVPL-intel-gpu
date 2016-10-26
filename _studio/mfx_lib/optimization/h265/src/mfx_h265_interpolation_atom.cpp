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
// - templates used to reduce branches on Atom, so shift/offset values restricted to currently-used scale factors (see asserts below)
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_ATOM) && defined(MFX_TARGET_OPTIMIZATION_ATOM)

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
/* interleaved chroma interp coefficients, 8-bit, for offsets 1/8, 2/8, ... 7/8 */
ALIGN_DECL(16) static const signed char filtTabLumaFast_S8[3*2][16] = {
    {  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52 },
    {  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4 },

    {  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40 },
    {  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8 },

    {  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20 },
    {  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4 },
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
ALIGN_DECL(16) static const short filtTabLumaFast_S16[3*2][8] = {
    {  -4,  52,  -4,  52,  -4,  52,  -4,  52 },
    {  20,  -4,  20,  -4,  20,  -4,  20,  -4 },

    {  -8,  40,  -8,  40,  -8,  40,  -8,  40 },
    {  40,  -8,  40,  -8,  40,  -8,  40,  -8 },

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

template<int widthMul, int shift>
static void t_InterpLuma_s8_d16_H(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3;

    /* avoid Klocwork warnings */
    xmm1 = _mm_setzero_si128();
    xmm2 = _mm_setzero_si128();
    xmm3 = _mm_setzero_si128();

    coeffs = filtTabLuma_S8[4 * (tab_index-1)];

    /* calculate 8 outputs per inner loop, working horizontally - use shuffle/interleave instead of pshufb on Atom */
    do {
        for (col = 0; col < width; col += widthMul) {
            /* load 16 8-bit pixels, interleave for pmaddubsw */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));    /* [0,1,2,3...] */
            xmm2 = _mm_alignr_epi8(xmm2, xmm0, 1);              /* [1,2,3,4...] */
            xmm3 = xmm0;
            xmm0 = _mm_unpacklo_epi8(xmm0, xmm2);               /* [0,1,1,2,2,3...] */
            xmm3 = _mm_unpackhi_epi8(xmm3, xmm2);               /* [8,9,9,10,.....] */
            xmm1 = xmm3;
            xmm2 = xmm3;
            xmm1 = _mm_alignr_epi8(xmm1, xmm0, 4);              /* [2,3,3,4,4,5...] */
            xmm2 = _mm_alignr_epi8(xmm2, xmm0, 8);              /* [4,5,5,6,6,7...] */
            xmm3 = _mm_alignr_epi8(xmm3, xmm0, 12);             /* [6,7,7,8,8,9...] */

            /* packed (8*8 + 8*8) -> 16 */
            xmm0 = _mm_maddubs_epi16(xmm0, *(const __m128i *)(coeffs +  0));    /* coefs 0,1 */
            xmm1 = _mm_maddubs_epi16(xmm1, *(const __m128i *)(coeffs + 16));    /* coefs 2,3 */
            xmm2 = _mm_maddubs_epi16(xmm2, *(const __m128i *)(coeffs + 32));    /* coefs 4,5 */
            xmm3 = _mm_maddubs_epi16(xmm3, *(const __m128i *)(coeffs + 48));    /* coefs 6,7 */

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
    __m128i xmm0, xmm1, xmm2, xmm3;

    /* avoid Klocwork warnings */
    xmm1 = _mm_setzero_si128();
    xmm2 = _mm_setzero_si128();
    xmm3 = _mm_setzero_si128();

    coeffs = filtTabLumaFast_S8[2 * (tab_index-1)];

    /* calculate 8 outputs per inner loop, working horizontally - use shuffle/interleave instead of pshufb on Atom */
    do {
        
        for (col = 0; col < width; col += widthMul) {
            /* load 16 8-bit pixels, make offsets of 1,2,3 bytes */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));    /* [0,1,2,3...] */
            xmm1 = _mm_alignr_epi8(xmm1, xmm0, 1);              /* [1,2,3,4...] */
            xmm2 = _mm_alignr_epi8(xmm2, xmm0, 2);              /* [2,3,4,5...] */
            xmm3 = _mm_alignr_epi8(xmm3, xmm0, 3);              /* [3,4,5,6...] */

            /* interleave pixels */
            xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);              /* [0,1,1,2,2,3,3,4...] */
            xmm2 = _mm_unpacklo_epi16(xmm2, xmm3);              /* [2,3,3,4,4,5,5,6,...] */

            /* packed (8*8 + 8*8) -> 16 */
            xmm0 = _mm_maddubs_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
            xmm2 = _mm_maddubs_epi16(xmm2, *(__m128i *)(coeffs + 16));    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi16(xmm0, xmm2);
            if (shift > 0) {
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16( (1<<shift)>>1 ));
                xmm0 = _mm_srai_epi16(xmm0, shift);
            }

            /* store 8 16-bit words */
            if (widthMul == 8) {
                _mm_storeu_si128((__m128i *)(pDst + col), xmm0);
            } else if (widthMul == 4) 
                _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
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
    __m128i xmm0, xmm1, xmm2, xmm3;

    /* avoid Klocwork warnings */
    xmm1 = _mm_setzero_si128();
    xmm2 = _mm_setzero_si128();
    xmm3 = _mm_setzero_si128();

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];

    /* calculate 8 outputs per inner loop, working horizontally - use shuffle/interleave instead of pshufb on Atom */
    do {
        /* different versions for U/V interleaved or separate planes */
        if (plane == TEXT_CHROMA) {
            for (col = 0; col < width; col += widthMul) {
                /* load 16 8-bit pixels, make offsets of 2,4,6 bytes */
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));    /* [0,1,2,3...] */
                xmm1 = _mm_alignr_epi8(xmm1, xmm0, 2);              /* [2,3,4,5...] */
                xmm2 = _mm_alignr_epi8(xmm2, xmm0, 4);              /* [4,5,6,7...] */
                xmm3 = _mm_alignr_epi8(xmm3, xmm0, 6);              /* [6,7,8,9...] */

                /* interleave pixels */
                xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);               /* [0,2,1,3,2,4,3,5...] = [UUVVUUVV...] */
                xmm2 = _mm_unpacklo_epi8(xmm2, xmm3);               /* [4,6,5,7,6,8,7,9...] = [UUVVUUVV...] */

                /* packed (8*8 + 8*8) -> 16 */
                xmm0 = _mm_maddubs_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
                xmm2 = _mm_maddubs_epi16(xmm2, *(__m128i *)(coeffs + 16));    /* coefs 2,3 */

                /* sum intermediate values, add offset, shift off fraction bits */
                xmm0 = _mm_add_epi16(xmm0, xmm2);
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
        } else {
            for (col = 0; col < width; col += widthMul) {
                /* load 16 8-bit pixels, make offsets of 1,2,3 bytes */
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));    /* [0,1,2,3...] */
                xmm1 = _mm_alignr_epi8(xmm1, xmm0, 1);              /* [1,2,3,4...] */
                xmm2 = _mm_alignr_epi8(xmm2, xmm0, 2);              /* [2,3,4,5...] */
                xmm3 = _mm_alignr_epi8(xmm3, xmm0, 3);              /* [3,4,5,6...] */

                /* interleave pixels */
                xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);              /* [0,1,1,2,2,3,3,4...] */
                xmm2 = _mm_unpacklo_epi16(xmm2, xmm3);              /* [2,3,3,4,4,5,5,6,...] */

                /* packed (8*8 + 8*8) -> 16 */
                xmm0 = _mm_maddubs_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
                xmm2 = _mm_maddubs_epi16(xmm2, *(__m128i *)(coeffs + 16));    /* coefs 2,3 */

                /* sum intermediate values, add offset, shift off fraction bits */
                xmm0 = _mm_add_epi16(xmm0, xmm2);
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
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5;

    /* avoid Klocwork warnings */
    xmm1 = _mm_setzero_si128();
    xmm2 = _mm_setzero_si128();
    xmm3 = _mm_setzero_si128();

    coeffs = (const signed char *)filtTabLuma_S16[4 * (tab_index-1)];

    /* calculate 4 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 4) {
            /* load 16 8-bit pixels, interleave for pmaddubsw */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* words 0-7 */
            xmm1 = _mm_loadl_epi64((__m128i *)(pSrc + col + 8));    /* words 8-11 */
            xmm4 = xmm1;
            xmm5 = xmm1;
            xmm4 = _mm_alignr_epi8(xmm4, xmm0, 2);                  /* words 1-8 */
            xmm1 = _mm_alignr_epi8(xmm1, xmm0, 4);                  /* words 2-9 */
            xmm5 = _mm_alignr_epi8(xmm5, xmm0, 6);                  /* words 3-10 */

            xmm2 = xmm0;
            xmm3 = xmm1;
            xmm0 = _mm_unpacklo_epi16(xmm0, xmm4);                  /* [0,1,1,2,2,3,3,4] */
            xmm2 = _mm_unpackhi_epi16(xmm2, xmm4);                  /* [4,5,5,6,6,7,7,8] */
            xmm1 = _mm_unpacklo_epi16(xmm1, xmm5);                  /* [2,3,3,4,4,5,5,6] */
            xmm3 = _mm_unpackhi_epi16(xmm3, xmm5);                  /* [6,7,7,8,8,9,0,10] */

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
template<int shift, int offset>
static void t_InterpLumaFast_s16_d16_H(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3;

    /* avoid Klocwork warnings */
    xmm1 = _mm_setzero_si128();
    xmm2 = _mm_setzero_si128();
    xmm3 = _mm_setzero_si128();

    coeffs = (const signed char *)filtTabLumaFast_S16[2 * (tab_index-1)];

    /* calculate 4 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 4) {
            /* load 8 16-bit pixels, make offsets of 1,2,3 words */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* [0,1,2,3,4,5,6,7] */
            xmm2 = _mm_alignr_epi8(xmm2, xmm0, 2);                  /* [1,2,3,4,5,6,7,-] */
            xmm1 = _mm_alignr_epi8(xmm1, xmm0, 4);                  /* [2,3,4,5,6,7,-,-] */
            xmm3 = _mm_alignr_epi8(xmm3, xmm0, 6);                  /* [3,4,5,6,7,-,-,-] */

            /* interleave pixels */
            xmm0 = _mm_unpacklo_epi32(xmm0, xmm2);                  /* [0,1,1,2,2,3,3,4...] */
            xmm1 = _mm_unpacklo_epi32(xmm1, xmm3);                  /* [2,3,3,4,4,5,5,6,...] */

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
    __m128i xmm0, xmm1, xmm2, xmm3;

    /* avoid Klocwork warnings */
    xmm1 = _mm_setzero_si128();
    xmm2 = _mm_setzero_si128();
    xmm3 = _mm_setzero_si128();

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    /* calculate 4 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 4) {
            /* load, interleave 8 16-bit pixels */
            if (plane == TEXT_CHROMA) {
                /* load 8 16-bit pixels, make offsets of 2,4,6 words */
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* [0,1,2,3,4,5,6,7] */
                xmm2 = _mm_loadl_epi64((__m128i *)(pSrc + col + 8));    /* [8,9,10,11,-,-,-,-] */
                xmm1 = xmm2;
                xmm3 = xmm2;
                xmm2 = _mm_alignr_epi8(xmm2, xmm0, 4);                  /* [2,3,4,5,6,7,8,9] */
                xmm1 = _mm_alignr_epi8(xmm1, xmm0, 8);                  /* [4,5,6,7,8,9,10,11] */
                xmm3 = _mm_alignr_epi8(xmm3, xmm0, 12);                 /* [6,7,8,9,10,11,-,-] */

                /* interleave pixels */
                xmm0 = _mm_unpacklo_epi16(xmm0, xmm2);                  /* [0,2,1,3,2,4,3,5...] = [UUVVUUVV...] */
                xmm1 = _mm_unpacklo_epi16(xmm1, xmm3);                  /* [4,6,5,7,6,8,7,9...] = [UUVVUUVV...] */
            } else {
                /* load 8 16-bit pixels, make offsets of 1,2,3 words */
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* [0,1,2,3,4,5,6,7] */
                xmm2 = _mm_alignr_epi8(xmm2, xmm0, 2);                  /* [1,2,3,4,5,6,7,-] */
                xmm1 = _mm_alignr_epi8(xmm1, xmm0, 4);                  /* [2,3,4,5,6,7,-,-] */
                xmm3 = _mm_alignr_epi8(xmm3, xmm0, 6);                  /* [3,4,5,6,7,-,-,-] */

                /* interleave pixels */
                xmm0 = _mm_unpacklo_epi32(xmm0, xmm2);                  /* [0,1,1,2,2,3,3,4...] */
                xmm1 = _mm_unpacklo_epi32(xmm1, xmm3);                  /* [2,3,3,4,4,5,5,6,...] */
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

} // end namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
