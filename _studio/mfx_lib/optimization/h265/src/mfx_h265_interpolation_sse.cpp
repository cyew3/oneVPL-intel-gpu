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
// HEVC interpolation filters, implemented with SSE packed multiply-add 
// 
// NOTES:
// - requires at least SSE4.1 (pextrw, pmovzxbw)
// - functions assume that width = multiple of 4 (LUMA) or multiple of 2 (CHROMA), height > 0
// - input data is read in chunks of 8 or 16 bytes regardless of width, so assume it's safe to read up to 16 bytes beyond right edge of block 
//     (data not used, just must be allocated memory - pad inbuf as needed)
// - best intrinsic performance requires up to date Intel compiler (check compiler output to make sure no unnecessary stack spills!)
// - if compiler register allocates poorly, may need to turn /Qipo OFF for this file
// - picture/buffer averaging is done as separate pass after filtering to allow simpler filter kernels 
//     (pays off in performance by reducing branches for the most common modes, also makes code easier to read/change)
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)

#include <immintrin.h>
#include <assert.h>

/* workaround for compiler bug (see h265_tr_quant_opt.cpp) */
#ifdef NDEBUG 
  #define MM_LOAD_EPI64(x) (*(__m128i*)(x))
#else
  #define MM_LOAD_EPI64(x) _mm_loadl_epi64( (__m128i*)(x))
#endif

namespace MFX_HEVC_COMMON
{
    enum EnumPlane
    {
        TEXT_LUMA = 0,
        TEXT_CHROMA,
        TEXT_CHROMA_U,
        TEXT_CHROMA_V,
    };

    //--------------
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

/* pshufb table for chroma, 8-bit horizontal filtering */
ALIGN_DECL(16) static const signed char shufTabIntUV[2][16] = {
    {  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7,  9 },
    {  4,  6,  5,  7,  6,  8,  7,  9,  8, 10,  9, 11, 10, 12, 11, 13 },
};

/* luma, horizontal, 8-bit input, 16-bit output */
static void InterpLuma_s8_d16_H(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int shift, short offset)
{
    int col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3;        /* should compile without stack spills */
    __m128i xmm_offset, xmm_shift;

    coeffs = filtTabLuma_S8[4 * (tab_index-1)];

    xmm_offset = _mm_cvtsi32_si128( ((unsigned int)offset << 16) | (unsigned int)(offset) );
    xmm_offset = _mm_shuffle_epi32(xmm_offset, 0x00);
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
            xmm1 = _mm_maddubs_epi16(xmm1, *(__m128i *)(coeffs + 16));    /* coefs 2,3 */
            xmm2 = _mm_maddubs_epi16(xmm2, *(__m128i *)(coeffs + 32));    /* coefs 4,5 */
            xmm3 = _mm_maddubs_epi16(xmm3, *(__m128i *)(coeffs + 48));    /* coefs 6,7 */

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

/* chroma, horizontal, 8-bit input, 16-bit output */
static void InterpChroma_s8_d16_H(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int shift, short offset, int plane)
{
    int col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    const signed char* shufTab;
    __m128i xmm0, xmm1, xmm2, xmm3;        /* should compile without stack spills */
    __m128i xmm_offset, xmm_shift;

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];
    if (plane == TEXT_CHROMA)
        shufTab = (signed char *)shufTabIntUV;
    else 
        shufTab = (signed char *)shufTabPlane;

    xmm_offset = _mm_cvtsi32_si128( ((unsigned int)offset << 16) | (unsigned int)(offset) );
    xmm_offset = _mm_shuffle_epi32(xmm_offset, 0x00);
    xmm_shift = _mm_cvtsi32_si128(shift);

    xmm2 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm3 = _mm_load_si128((__m128i *)(coeffs + 16));

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
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTab + 16));

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

/* luma, vertical, 8-bit input, 16-bit output 
 * NOTE: ICL seems to spill a couple of xmm registers to stack unnecessarily
 */
static void InterpLuma_s8_d16_V(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int shift, short offset)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    ALIGN_DECL(16) signed char offsetTab[16], shiftTab[16];

    coeffs = filtTabLuma_S8[4 * (tab_index-1)];

    /* cache offset and shift registers since we will run out of xmm registers */
    xmm0 = _mm_cvtsi32_si128( ((unsigned int)offset << 16) | (unsigned int)(offset) );
    xmm0 = _mm_shuffle_epi32(xmm0, 0x00);
    _mm_storeu_si128((__m128i*)offsetTab, xmm0);

    xmm0 = _mm_cvtsi32_si128(shift);
    _mm_storeu_si128((__m128i*)shiftTab, xmm0);

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
            xmm7 = _mm_maddubs_epi16(xmm7, *(__m128i*)(coeffs + 16));
            xmm0 = _mm_add_epi16(xmm0, xmm7);

            /* interleave rows 4,5 and multiply by coefs 4,5 */
            xmm7 = xmm4;
            xmm7 = _mm_unpacklo_epi8(xmm7, xmm5);
            xmm7 = _mm_maddubs_epi16(xmm7, *(__m128i*)(coeffs + 32));
            xmm0 = _mm_add_epi16(xmm0, xmm7);

            /* interleave rows 6,7 and multiply by coefs 6,7 */
            xmm7 = _mm_loadl_epi64((__m128i*)(pSrc + 7*srcPitch));    /* load row 7 */
            xmm6 = _mm_unpacklo_epi8(xmm6, xmm7);
            xmm6 = _mm_maddubs_epi16(xmm6, *(__m128i*)(coeffs + 48));
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

/* luma, vertical, 8-bit input, 16-bit output */
static void InterpChroma_s8_d16_V(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int shift, short offset)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm7;    /* let compiler decide how to use xmm vs. stack */
    __m128i xmm_offset, xmm_shift;

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];

    xmm_offset = _mm_cvtsi32_si128( ((unsigned int)offset << 16) | (unsigned int)(offset) );
    xmm_offset = _mm_shuffle_epi32(xmm_offset, 0x00);
    xmm_shift = _mm_cvtsi32_si128(shift);

    xmm4 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm5 = _mm_load_si128((__m128i *)(coeffs + 16));

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

/* luma, vertical, 16-bit input, 16-bit output */
static void InterpLuma_s16_d16_V(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int shift, short offset)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    ALIGN_DECL(16) signed char offsetTab[16], shiftTab[16];

    coeffs = (const signed char *)filtTabLuma_S16[4 * (tab_index-1)];

    xmm0 = _mm_cvtsi32_si128(offset);
    xmm0 = _mm_shuffle_epi32(xmm0, 0x00);
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
            xmm7 = _mm_madd_epi16(xmm7, *(__m128i*)(coeffs + 16));
            xmm0 = _mm_add_epi32(xmm0, xmm7);

            /* interleave rows 4,5 and multiply by coefs 4,5 */
            xmm7 = xmm4;
            xmm7 = _mm_unpacklo_epi16(xmm7, xmm5);
            xmm7 = _mm_madd_epi16(xmm7, *(__m128i*)(coeffs + 32));
            xmm0 = _mm_add_epi32(xmm0, xmm7);

            /* interleave rows 6,7 and multiply by coefs 6,7 */
            xmm7 = _mm_loadl_epi64((__m128i*)(pSrc + 7*srcPitch));    /* load row 7 */
            xmm6 = _mm_unpacklo_epi16(xmm6, xmm7);
            xmm6 = _mm_madd_epi16(xmm6, *(__m128i*)(coeffs + 48));
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

/* chroma, vertical, 16-bit input, 16-bit output */
static void InterpChroma_s16_d16_V(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int shift, short offset)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm7;    /* let compiler decide how to use xmm vs. stack */
    __m128i xmm_offset, xmm_shift;

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    xmm_offset = _mm_cvtsi32_si128(offset);
    xmm_offset = _mm_shuffle_epi32(xmm_offset, 0x00);
    xmm_shift = _mm_cvtsi32_si128(shift);

    xmm4 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm5 = _mm_load_si128((__m128i *)(coeffs + 16));

    /* always calculates 4 outputs per inner loop, working vertically */
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

/* mode: AVERAGE_NO, just clip/pack 16-bit output to 8-bit 
 * NOTE: could be optimized more, but is not used very often in practice
 */
static void AverageModeN(short *pSrc, unsigned int srcPitch, unsigned char *pDst, unsigned int dstPitch, int width, int height)
{
    int col;
    short *pSrcRef = pSrc;
    unsigned char *pDstRef = pDst;
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

/* mode: AVERAGE_FROM_PIC, load 8-bit pixels, extend to 16-bit, add to current output, clip/pack 16-bit to 8-bit */
static void AverageModeP(short *pSrc, unsigned int srcPitch, unsigned char *pAvg, unsigned int avgPitch, unsigned char *pDst, unsigned int dstPitch, int width, int height)
{
    int col;
    short *pSrcRef = pSrc;
    unsigned char *pDstRef = pDst, *pAvgRef = pAvg;
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

/* mode: AVERAGE_FROM_BUF, load 16-bit pixels, add to current output, clip/pack 16-bit to 8-bit */
static void AverageModeB(short *pSrc, unsigned int srcPitch, short *pAvg, unsigned int avgPitch, unsigned char *pDst, unsigned int dstPitch, int width, int height)
{
    int col;
    short *pSrcRef = pSrc, *pAvgRef = pAvg;
    unsigned char *pDstRef = pDst;
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

/* this is the most heavily used path (~50% of total pixels) and most highly optimized 
 * read in 8-bit pixels and apply H or V filter, save output to 16-bit buffer (for second filtering pass or for averaging with reference)
 */
void Interp_S8_NoAvg(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, 
                   int tab_index, int width, int height, int shift, short offset, int dir, int plane)
{
    assert( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    if (plane == TEXT_LUMA) {
        if (dir == INTERP_HOR)
            InterpLuma_s8_d16_H(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
        else
            InterpLuma_s8_d16_V(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
    } else {
        if (dir == INTERP_HOR)
            InterpChroma_s8_d16_H(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset, plane);
        else
            InterpChroma_s8_d16_V(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
    }
}

/* typically used for ~15% of total pixels, does vertical filter pass only */
void Interp_S16_NoAvg(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, 
                   int tab_index, int width, int height, int shift, short offset, int dir, int plane)
{
    assert( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    /* only V is supported/needed */
    if (dir != INTERP_VER)
        return;

    if (plane == TEXT_LUMA)
        InterpLuma_s16_d16_V(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
    else
        InterpChroma_s16_d16_V(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, shift, offset);
}

/* NOTE: average functions assume maximum block size of 64x64, including 7 extra output rows for H pass */

/* typically used for ~15% of total pixels, does first-pass horizontal or vertical filter, optionally with average against reference */
void Interp_S8_WithAvg(const unsigned char* pSrc, unsigned int srcPitch, unsigned char *pDst, unsigned int dstPitch, void *pvAvg, unsigned int avgPitch, int avgMode, 
                int tab_index, int width, int height, int shift, short offset, int dir, int plane)
{
    /* pretty big stack buffer, probably want to pass this in (allocate on heap) */
    ALIGN_DECL(16) short tmpBuf[(64+8)*(64)];
    
    assert( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    if (plane == TEXT_LUMA) {
        if (dir == INTERP_HOR)
            InterpLuma_s8_d16_H(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset);
        else
            InterpLuma_s8_d16_V(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset);
    } else {
        if (dir == INTERP_HOR)
            InterpChroma_s8_d16_H(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset, plane);
        else
            InterpChroma_s8_d16_V(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset);
    }

    if (avgMode == AVERAGE_NO)
        AverageModeN(tmpBuf, 64, pDst, dstPitch, width, height);
    else if (avgMode == AVERAGE_FROM_PIC)
        AverageModeP(tmpBuf, 64, (unsigned char *)pvAvg, avgPitch, pDst, dstPitch, width, height);
    else if (avgMode == AVERAGE_FROM_BUF)
        AverageModeB(tmpBuf, 64, (short *)pvAvg, avgPitch, pDst, dstPitch, width, height);
}


/* typically used for ~20% of total pixels, does second-pass vertical filter only, optionally with average against reference */
void Interp_S16_WithAvg(const short* pSrc, unsigned int srcPitch, unsigned char *pDst, unsigned int dstPitch, void *pvAvg, unsigned int avgPitch, int avgMode, 
                   int tab_index, int width, int height, int shift, short offset, int dir, int plane)
{
    /* pretty big stack buffer, probably want to pass this in (allocate on heap) */
    ALIGN_DECL(16) short tmpBuf[(64+8)*(64)];

    /* only V is supported/needed */
    if (dir != INTERP_VER)
        return;

    assert( ( (plane == TEXT_LUMA) && ((width & 0x3) == 0) ) || ( (plane != TEXT_LUMA) && ((width & 0x1) == 0) ) );

    if (plane == TEXT_LUMA)
        InterpLuma_s16_d16_V(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset);
    else
        InterpChroma_s16_d16_V(pSrc, srcPitch, tmpBuf, 64, tab_index, width, height, shift, offset);

    if (avgMode == AVERAGE_NO)
        AverageModeN(tmpBuf, 64, pDst, dstPitch, width, height);
    else if (avgMode == AVERAGE_FROM_PIC)
        AverageModeP(tmpBuf, 64, (unsigned char *)pvAvg, avgPitch, pDst, dstPitch, width, height);
    else if (avgMode == AVERAGE_FROM_BUF)
        AverageModeB(tmpBuf, 64, (short *)pvAvg, avgPitch, pDst, dstPitch, width, height);
}
    //--------------
} // end namespace MFX_HEVC_COMMON

#endif //#if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
