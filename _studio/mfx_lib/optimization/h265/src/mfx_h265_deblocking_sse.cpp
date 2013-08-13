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

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM)

#ifndef Clip3
#define Clip3( m_Min, m_Max, m_Value) ( (m_Value) < (m_Min) ? (m_Min) : ( (m_Value) > (m_Max) ? (m_Max) : (m_Value) ) )
#endif

namespace MFX_HEVC_COMMON
{

enum FilterType
{
    VERT_FILT = 0,
    HOR_FILT = 1,
};

/* tc and beta tables from section 8.7 in spec */
static const Ipp32s tcTable[54] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
    3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  9, 10, 11, 13,
    14, 16, 18, 20, 22, 24
};

static const Ipp32s betaTable[52] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
    26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
    58, 60, 62, 64
};

/* byte shuffle tables for reorganizing filtered outputs - try to minimize use of pshufb on Atom (slow) */
ALIGN_DECL(16) static const signed char shufTabH[16] = {1, 3, 5, 7, 0, 2, 4, 6, 8, 10, 12, 14, 9, 11, 13, 15};
ALIGN_DECL(16) static const signed char shufTabV[16] = {1, 0, 8, 9, 3, 2, 10, 11, 5, 4, 12, 13, 7, 6, 14, 15};
ALIGN_DECL(16) static const signed char shufTabP[16] = {3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12};

ALIGN_DECL(16) static const signed short add8[8] = {8, 8, 8, 8, 8, 8, 8, 8};
ALIGN_DECL(16) static const signed short add4[8] = {4, 4, 4, 4, 4, 4, 4, 4};
ALIGN_DECL(16) static const signed short add2[8] = {2, 2, 2, 2, 2, 2, 2, 2};
ALIGN_DECL(16) static const signed short add1[8] = {1, 1, 1, 1, 1, 1, 1, 1};

/* luma deblocking kernel
 * optimized with SSE to process all 4 pixels in single pass
 * operates in-place on a single 4-pixel edge (vertical or horizontal) within an 8x8 block
 * assumes 8-bit data
 * in standalone unit test (IVB) appx. 3.0x speedup vs. C reference (H edges), 2.8x speedup (V edges)
 * returns strongFiltering flag for collecting stats/timing (0 = normal, 1 = strong, 2 = filtering was skipped)
 */
Ipp32s h265_FilterEdgeLuma_8u_I_SSE(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir)
{
    Ipp32s tcIdx, bIdx, tc, beta, sideThreshhold;
    Ipp32s d0, d3, dq, dp, d;
    Ipp32s ds0, ds3, dm0, dm3;
    Ipp32s strongFiltering = 2; /* 2 = no filtering, only for tracking statistics */
    Ipp32u writeMask;

    /* algorithms designed to fit in 8 xmm registers - check compiler output to ensure it's allocating well */
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    /* parameters to decide strong/weak/no filtering */
    tcIdx = Clip3(0, 53, edge->qp + 2 * (edge->strength - 1) + edge->tcOffset);
    bIdx = Clip3(0, 51, edge->qp + edge->betaOffset);
    tc =  tcTable[tcIdx];
    beta = betaTable[bIdx];
    sideThreshhold = (beta + (beta >> 1)) >> 3;

    if (dir == VERT_FILT) {
        /* vertical edge: load rows 0 and 3 (8 pixels) to determine whether to apply filtering */
        xmm1 = _mm_loadl_epi64((__m128i *)(srcDst + 0*srcDstStride - 4));
        xmm3 = _mm_loadl_epi64((__m128i *)(srcDst + 3*srcDstStride - 4));
        xmm4 = xmm1;

        /* interleave rows 0 and 3, shuffle pixels to process horizontal data vertically */
        xmm4 = _mm_unpacklo_epi8(xmm4, xmm3);   /* 00112233 44556677 */
        xmm5 = _mm_shufflelo_epi16(xmm4, 0x01); /* 11000000 44556677 */
        xmm6 = _mm_shufflelo_epi16(xmm4, 0x02); /* 22000000 44556677 */
        xmm6 = _mm_shufflehi_epi16(xmm6, 0x01); /* 22000000 55000000 */
        xmm7 = _mm_shufflelo_epi16(xmm4, 0x03); /* 33000000 44556677 */
        xmm7 = _mm_shufflehi_epi16(xmm7, 0x02); /* 33000000 66000000 */

        xmm5 = _mm_shuffle_epi32(xmm5, 0x08);   /* 11004455 --------*/
        xmm6 = _mm_shuffle_epi32(xmm6, 0x08);   /* 22005500 --------*/
        xmm7 = _mm_shuffle_epi32(xmm7, 0x08);   /* 33006600 --------*/

        /* expand to 16 bits */
        xmm5 = _mm_cvtepu8_epi16(xmm5);         /* 1 1 0 0 4 4 5 5 */
        xmm6 = _mm_cvtepu8_epi16(xmm6);         /* 2 2 0 0 5 5 0 0 */
        xmm7 = _mm_cvtepu8_epi16(xmm7);         /* 3 3 0 0 6 6 0 0 */

        /* abs(p2-2*p1+p0), abs(q0-2*q1+q2) - both rows */
        xmm5 = _mm_sub_epi16(xmm5, xmm6);
        xmm5 = _mm_sub_epi16(xmm5, xmm6);
        xmm5 = _mm_add_epi16(xmm5, xmm7);
        xmm5 = _mm_abs_epi16(xmm5);             /* dp0 dp3 --- --- dq0 dq3 --- --- */
        xmm6 = _mm_shuffle_epi32(xmm5, 0x02);   /* dq0 dq3 --- --- --- --- --- --- */
        xmm7 = _mm_shuffle_epi32(xmm5, 0x08);   /* dp0 dp3 dq0 dq3 --- --- --- --- */

        /* horizontal add typically slow but avoids extra shuffles (use rarely) */
        xmm7 = _mm_hadd_epi16(xmm7, xmm7);      /* dp = dp0 + dp3 | dq = dq0 + dq3 */
        xmm5 = _mm_add_epi16(xmm5, xmm6);       /* d0 d3 */

        xmm5 = _mm_unpacklo_epi32(xmm5, xmm7);  /* d0 d3 dp dq */
        xmm5 = _mm_cvtepi16_epi32(xmm5);        /* expand to 32 bits */
        xmm6 = _mm_shuffle_epi32(xmm5, 0x01);
        xmm7 = _mm_shuffle_epi32(xmm5, 0x02);
        xmm4 = _mm_shuffle_epi32(xmm5, 0x03);

        /* use movd instead of pextrw/d (slow on Atom) */
        d0 = _mm_cvtsi128_si32(xmm5);
        d3 = _mm_cvtsi128_si32(xmm6);
        dp = _mm_cvtsi128_si32(xmm7);
        dq = _mm_cvtsi128_si32(xmm4);

        d = d0 + d3;

        /* early exit */
        if (d >= beta)
            return strongFiltering;

        /* load rows 1 and 2 (8 pixels), xmm1, xmm3 still have original 8-bit rows 0,3 */
        xmm0 = _mm_loadl_epi64((__m128i *)(srcDst + 1*srcDstStride - 4));
        xmm2 = _mm_loadl_epi64((__m128i *)(srcDst + 2*srcDstStride - 4));
        xmm4 = xmm1;

        /* interleave rows 0 and 3, shuffle pixels to determine strong/weak filter */
        xmm4 = _mm_unpacklo_epi8(xmm4, xmm3);   /* 00112233 44556677 */
        xmm6 = _mm_shufflelo_epi16(xmm4, 0x0c); /* 00330000 44556677 */
        xmm6 = _mm_shufflehi_epi16(xmm6, 0x0c); /* 00330000 44774444 */
        xmm6 = _mm_shuffle_epi32(xmm6, 0x08);   /* 00334477 --------*/
        xmm6 = _mm_cvtepu8_epi16(xmm6);         /* 0 0 3 3 4 4 7 7 */

        /* flip order of subtraction for top half, since taking abs() anyway */
        xmm4 = _mm_shuffle_epi32(xmm6, 0x08);   /* 0 0 4 4 0 0 0 0 */
        xmm5 = _mm_shuffle_epi32(xmm6, 0x05);   /* 3 3 3 3 0 0 0 0 */
        xmm4 = _mm_sub_epi16(xmm4, xmm5);
        xmm4 = _mm_abs_epi16(xmm4);             /* abs(0-3) | abs (4-3) */

        xmm7 = _mm_shuffle_epi32(xmm6, 0x02);   /* 4 4 0 0 0 0 0 0 */
        xmm5 = _mm_shuffle_epi32(xmm6, 0x03);   /* 7 7 0 0 0 0 0 0 */
        xmm7 = _mm_sub_epi16(xmm7, xmm5);       /* (0-0) in words 2,3 cancels out */
        xmm7 = _mm_abs_epi16(xmm7);             /* abs (4-7) */
        xmm4 = _mm_add_epi16(xmm4, xmm7);       /* ds0 | ds3 | dm0 | dm3 */

        /* expand to 32 bits, shuffle for movd */
        xmm4 = _mm_cvtepi16_epi32(xmm4);
        xmm5 = _mm_shuffle_epi32(xmm4, 0x01);
        xmm6 = _mm_shuffle_epi32(xmm4, 0x02);
        xmm7 = _mm_shuffle_epi32(xmm4, 0x03);

        ds0 = _mm_cvtsi128_si32(xmm4);
        ds3 = _mm_cvtsi128_si32(xmm5);
        dm0 = _mm_cvtsi128_si32(xmm6);
        dm3 = _mm_cvtsi128_si32(xmm7);

        /* input: xmm1 = row0, xmm0 = row1, xmm2 = row2 xmm3 = row3 
         * shuffle data into form for common kernel
         */
        xmm1 = _mm_unpacklo_epi8(xmm1, xmm0);   /* interleave row 0,1 */
        xmm2 = _mm_unpacklo_epi8(xmm2, xmm3);   /* interleave row 2,3 */
        xmm4 = xmm1;

        xmm1 = _mm_unpacklo_epi16(xmm1, xmm2);  /* interleave row 0,1,2,3 (low half) */
        xmm4 = _mm_unpackhi_epi16(xmm4, xmm2);  /* interleave row 0,1,2,3 (high half) */

        xmm4 = _mm_shuffle_epi32(xmm4, 0x1b);   /* reverse order of 32-bit words (7654) */
        xmm3 = xmm1;
        xmm3 = _mm_unpacklo_epi32(xmm3, xmm4);  /* 0000 7777 1111 6666 */
        xmm1 = _mm_unpackhi_epi32(xmm1, xmm4);  /* 2222 5555 3333 4444 */

        xmm2 = _mm_alignr_epi8(xmm2, xmm3, 8);  /* 1111 6666 ---- ---- */
        xmm0 = _mm_alignr_epi8(xmm0, xmm1, 8);  /* 3333 4444 ---- ---- */

        /* expand to 16 bits */
        xmm0 = _mm_cvtepu8_epi16(xmm0);         /* 3 3 3 3 4 4 4 4 */
        xmm1 = _mm_cvtepu8_epi16(xmm1);         /* 2 2 2 2 5 5 5 5 */
        xmm2 = _mm_cvtepu8_epi16(xmm2);         /* 1 1 1 1 6 6 6 6 */
        xmm3 = _mm_cvtepu8_epi16(xmm3);         /* 0 0 0 0 7 7 7 7 */
    } else {
        /* horizontal edge: load 6 rows of 4 pixels, expand to 16 bits, shuffle into 3 xmm registers */
        xmm2 = _mm_cvtsi32_si128(*(int *)(srcDst - 3*srcDstStride));
        xmm1 = _mm_cvtsi32_si128(*(int *)(srcDst - 2*srcDstStride));
        xmm0 = _mm_cvtsi32_si128(*(int *)(srcDst - 1*srcDstStride));

        xmm0 = _mm_insert_epi32(xmm0, *(int *)(srcDst + 0*srcDstStride), 1);
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(srcDst + 1*srcDstStride), 1);
        xmm2 = _mm_insert_epi32(xmm2, *(int *)(srcDst + 2*srcDstStride), 1);

        /* expand to 16 bits */
        xmm0 = _mm_cvtepu8_epi16(xmm0);         /* 3 3 3 3 4 4 4 4 */
        xmm1 = _mm_cvtepu8_epi16(xmm1);         /* 2 2 2 2 5 5 5 5 */
        xmm2 = _mm_cvtepu8_epi16(xmm2);         /* 1 1 1 1 6 6 6 6 */

        /* abs(p2-2*p1+p0), abs(q0-2*q1+q2) - only care about columns 0,3 in each section */
        xmm4 = xmm0;
        xmm4 = _mm_sub_epi16(xmm4, xmm1);
        xmm4 = _mm_sub_epi16(xmm4, xmm1);
        xmm4 = _mm_add_epi16(xmm4, xmm2);
        xmm4 = _mm_abs_epi16(xmm4);             /* dp0 --- --- dp3 dq0 --- --- dq3 */
        xmm4 = _mm_shufflelo_epi16(xmm4, 0x0c); /* dp0 dp3 --- --- dq0 --- --- dq3 */
        xmm4 = _mm_shufflehi_epi16(xmm4, 0x0c); /* dp0 dp3 --- --- dq0 dq3 --- --- */

        xmm6 = _mm_shuffle_epi32(xmm4, 0x02);   /* dq0 dq3 --- --- --- --- --- --- */
        xmm7 = _mm_shuffle_epi32(xmm4, 0x08);   /* dp0 dp3 dq0 dq3 --- --- --- --- */

        xmm7 = _mm_hadd_epi16(xmm7, xmm7);      /* dp = dp0 + dp3 | dq = dq0 + dq3 */
        xmm4 = _mm_add_epi16(xmm4, xmm6);       /* d0 d3 */

        /* expand to 32 bits, shuffle for movd */
        xmm4 = _mm_unpacklo_epi32(xmm4, xmm7);  /* d0 d3 dp dq (32-bit) */
        xmm4 = _mm_cvtepi16_epi32(xmm4);
        xmm5 = _mm_shuffle_epi32(xmm4, 0x01);
        xmm6 = _mm_shuffle_epi32(xmm4, 0x02);
        xmm7 = _mm_shuffle_epi32(xmm4, 0x03);

        d0 = _mm_cvtsi128_si32(xmm4);
        d3 = _mm_cvtsi128_si32(xmm5);
        dp = _mm_cvtsi128_si32(xmm6);
        dq = _mm_cvtsi128_si32(xmm7);

        d = d0 + d3;

        /* early exit */
        if (d >= beta)
            return strongFiltering;

        /* load/shuffle first and last row (p3/q3) */
        xmm3 = _mm_cvtsi32_si128(*(int *)(srcDst - 4*srcDstStride));
        xmm3 = _mm_insert_epi32(xmm3, *(int *)(srcDst + 3*srcDstStride), 1);
        xmm3 = _mm_cvtepu8_epi16(xmm3);         /* 0 0 0 0 7 7 7 7 */

        /* assume xmm0-2 still have original rows 1-6 (shuffled) */
        xmm4 = xmm3;
        xmm4 = _mm_sub_epi16(xmm4, xmm0);       /* abs(0-3) | abs(7-4) */
        xmm4 = _mm_abs_epi16(xmm4);
        xmm5 = _mm_shuffle_epi32(xmm4, 0x0e);   /* abs(7-4) | --- */
        xmm4 = _mm_add_epi16(xmm4, xmm5);       /* ds0 --- --- ds3 --- --- --- --- */

        xmm5 = _mm_shuffle_epi32(xmm0, 0x0e);   /* 4 4 4 4 - - - - */
        xmm5 = _mm_sub_epi16(xmm5, xmm0);       /* 4-3 */
        xmm5 = _mm_abs_epi16(xmm5);             /* dm0 --- --- dm3 --- --- --- --- */

        xmm4 = _mm_shufflelo_epi16(xmm4, 0x0c); /* ds0 ds3 --- --- --- --- --- --- */
        xmm5 = _mm_shufflelo_epi16(xmm5, 0x0c); /* dm0 dm3 --- --- --- --- --- --- */
        xmm4 = _mm_cvtepi16_epi32(xmm4);
        xmm5 = _mm_cvtepi16_epi32(xmm5);
        xmm6 = _mm_shuffle_epi32(xmm4, 0x01);
        xmm7 = _mm_shuffle_epi32(xmm5, 0x01);

        /* expand to 32 bits, shuffle for movd */
        ds0 = _mm_cvtsi128_si32(xmm4);
        dm0 = _mm_cvtsi128_si32(xmm5);
        ds3 = _mm_cvtsi128_si32(xmm6);
        dm3 = _mm_cvtsi128_si32(xmm7);
    }

    strongFiltering = 0;
    if ((ds0 < (beta >> 3)) && (2*d0 < (beta >> 2)) && (dm0 < ((tc * 5 + 1) >> 1)) && (ds3 < (beta >> 3)) && (2*d3 < (beta >> 2)) && (dm3 < ((tc * 5 + 1) >> 1)))
        strongFiltering = 1;

    /* common kernel for strong filter */
    if (strongFiltering) {
        /* input: xmm0 = [3 3 3 3 4 4 4 4] xmm1 = [2 2 2 2 5 5 5 5] xmm2 = [1 1 1 1 6 6 6 6] xmm3 = [0 0 0 0 7 7 7 7] */
        xmm4 = _mm_shuffle_epi32(xmm0, 0xee);
        xmm4 = _mm_add_epi16(xmm4, xmm0);       /* 3+4 = p0+q0 */
        xmm4 = _mm_shuffle_epi32(xmm4, 0x44);   /* p0+q0 | p0+q0 */
        xmm4 = _mm_add_epi16(xmm4, xmm1);       /* p1+p0+q0 | q1+q0+p0*/

        xmm5 = _mm_shuffle_epi32(xmm1, 0x4e);   /* 5 5 5 5 2 2 2 2 (reverse q1/p1) */

        /* for clipping to [x-2*tc, x+2*tc] use single register and add 2*tc twice to convert - to + */
        xmm7 = _mm_set1_epi16((short)(tc*2));
        xmm6 = xmm0;
        xmm6 = _mm_subs_epi16(xmm6, xmm7);      /* p0-2*tc | q0-2*tc */

        /* p0 = (p2 + 2*(p1 + p0 + q0) + q1 + 4) >> 3 
         * q0 = (q2 + 2*(q1 + q0 + p0) + p1 + 4) >> 3
         */
        xmm0 = xmm2;
        xmm0 = _mm_adds_epi16(xmm0, xmm4);
        xmm0 = _mm_adds_epi16(xmm0, xmm4);
        xmm0 = _mm_adds_epi16(xmm0, xmm5);
        xmm0 = _mm_adds_epi16(xmm0, *(__m128i *)add4);
        xmm0 = _mm_srai_epi16(xmm0, 3);
        xmm0 = _mm_max_epi16(xmm0, xmm6);       /* clip to p0+2*tc | q0+2*tc */
        xmm6 = _mm_adds_epi16(xmm6, xmm7);
        xmm6 = _mm_adds_epi16(xmm6, xmm7);
        xmm0 = _mm_min_epi16(xmm0, xmm6);       /* clip to p0+2*tc | q0+2*tc */

        xmm6 = xmm1;
        xmm6 = _mm_subs_epi16(xmm6, xmm7);      /* p1-2*tc | q1-2*tc */

        /* p1 = (p2 + p1 + p0 + q0 + 2) >> 2
         * q1 = (q2 + q1 + q0 + p0 + 2) >> 2
         */
        xmm1 = xmm2;
        xmm1 = _mm_adds_epi16(xmm1, xmm4);
        xmm1 = _mm_adds_epi16(xmm1, *(__m128i *)add2);
        xmm1 = _mm_srai_epi16(xmm1, 2);
        xmm1 = _mm_max_epi16(xmm1, xmm6);
        xmm6 = _mm_adds_epi16(xmm6, xmm7);
        xmm6 = _mm_adds_epi16(xmm6, xmm7);
        xmm1 = _mm_min_epi16(xmm1, xmm6);

        xmm6 = xmm2;
        xmm6 = _mm_subs_epi16(xmm6, xmm7);      /* p2-2*tc | q2-2*tc */

        /* p2 = (2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3
         * q2 = (2 * q3 + 3 * q2 + q1 + q0 + p0 + 4) >> 3
         */
        xmm4 = _mm_adds_epi16(xmm4, xmm2);      /* tmp+p2 | tmp+q2 */
        xmm2 = _mm_adds_epi16(xmm2, xmm2);      /* 2*p2 | 2*q2 (done with it now) */
        xmm2 = _mm_adds_epi16(xmm2, xmm4);      /* tmp+3*p2 | tmp+3*q2 */
        xmm2 = _mm_adds_epi16(xmm2, xmm3);
        xmm2 = _mm_adds_epi16(xmm2, xmm3);
        xmm2 = _mm_adds_epi16(xmm2, *(__m128i *)add4);
        xmm2 = _mm_srai_epi16(xmm2, 3);
        xmm2 = _mm_max_epi16(xmm2, xmm6);
        xmm6 = _mm_adds_epi16(xmm6, xmm7);
        xmm6 = _mm_adds_epi16(xmm6, xmm7);
        xmm2 = _mm_min_epi16(xmm2, xmm6);

        if (dir == VERT_FILT) {
            /* clip to 8-bit, shuffle back into row order */
            xmm0 = _mm_packus_epi16(xmm0, xmm2);    /* p0 q0 (4 rows) p2 q2 (4 rows) */
            xmm1 = _mm_packus_epi16(xmm1, xmm3);    /* p1 q1 (4 rows) p3 q3 (4 rows) */
            xmm2 = xmm0;

            xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);   /* p0p1 q0q1 */
            xmm2 = _mm_unpackhi_epi8(xmm2, xmm1);   /* p2p3 q2q3 */
            xmm3 = xmm0;

            xmm0 = _mm_unpacklo_epi16(xmm0, xmm2);  /* p0p1p2p3 (4 rows) */
            xmm3 = _mm_unpackhi_epi16(xmm3, xmm2);  /* q0q1q2q3 (4 rows) */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)shufTabP);    /* reverse bytes: p3p2p1p0 */
            xmm2 = xmm0;

            xmm0 = _mm_unpacklo_epi32(xmm0, xmm3);  /* row0-p | row0-q | row1-p | row1-q */
            xmm2 = _mm_unpackhi_epi32(xmm2, xmm3);  /* row2-p | row2-q | row3-p | row3-q */
            xmm1 = _mm_shuffle_epi32(xmm0, 0xee);   /* row1-p | row1-q | ------ | ------ */
            xmm3 = _mm_shuffle_epi32(xmm2, 0xee);   /* row3-p | row3-q | ------ | ------ */

            if (edge->deblockP && edge->deblockQ) {
                /* common case - write 8 pixels at a time */
                _mm_storel_epi64((__m128i *)(srcDst - 4 + 0*srcDstStride), xmm0);
                _mm_storel_epi64((__m128i *)(srcDst - 4 + 1*srcDstStride), xmm1);
                _mm_storel_epi64((__m128i *)(srcDst - 4 + 2*srcDstStride), xmm2);
                _mm_storel_epi64((__m128i *)(srcDst - 4 + 3*srcDstStride), xmm3);
            } else if (edge->deblockP && !edge->deblockQ) {
                /* write only 4 pixels (p side) */
                *(int *)(srcDst - 4 + 0*srcDstStride) = _mm_cvtsi128_si32(xmm0);
                *(int *)(srcDst - 4 + 1*srcDstStride) = _mm_cvtsi128_si32(xmm1);
                *(int *)(srcDst - 4 + 2*srcDstStride) = _mm_cvtsi128_si32(xmm2);
                *(int *)(srcDst - 4 + 3*srcDstStride) = _mm_cvtsi128_si32(xmm3);
            } else if (!edge->deblockP && edge->deblockQ) {
                /* write only 4 pixels (q side) */
                xmm0 = _mm_shuffle_epi32(xmm0, 0x01);
                xmm1 = _mm_shuffle_epi32(xmm1, 0x01);
                xmm2 = _mm_shuffle_epi32(xmm2, 0x01);
                xmm3 = _mm_shuffle_epi32(xmm3, 0x01);

                *(int *)(srcDst + 0*srcDstStride) = _mm_cvtsi128_si32(xmm0);
                *(int *)(srcDst + 1*srcDstStride) = _mm_cvtsi128_si32(xmm1);
                *(int *)(srcDst + 2*srcDstStride) = _mm_cvtsi128_si32(xmm2);
                *(int *)(srcDst + 3*srcDstStride) = _mm_cvtsi128_si32(xmm3);
            }
        } else {
            /* clip to 8-bit, minimal shuffling needed for horizontal edges */
            xmm0 = _mm_packus_epi16(xmm0, xmm0);    /* row -1, row 0 */
            xmm1 = _mm_packus_epi16(xmm1, xmm1);    /* row -2, row 1 */
            xmm2 = _mm_packus_epi16(xmm2, xmm2);    /* row -3, row 2 */

            /* write p section (row -4 unchanged) */
            if (edge->deblockP) {
                *(int *)(srcDst - 3*srcDstStride) = _mm_cvtsi128_si32(xmm2);
                *(int *)(srcDst - 2*srcDstStride) = _mm_cvtsi128_si32(xmm1);
                *(int *)(srcDst - 1*srcDstStride) = _mm_cvtsi128_si32(xmm0);
            }

            /* write q section (row +3 unchanged) */
            if (edge->deblockQ) {
                xmm0 = _mm_shuffle_epi32(xmm0, 0x01);
                xmm1 = _mm_shuffle_epi32(xmm1, 0x01);
                xmm2 = _mm_shuffle_epi32(xmm2, 0x01);

                *(int *)(srcDst + 0*srcDstStride) = _mm_cvtsi128_si32(xmm0);
                *(int *)(srcDst + 1*srcDstStride) = _mm_cvtsi128_si32(xmm1);
                *(int *)(srcDst + 2*srcDstStride) = _mm_cvtsi128_si32(xmm2);
            }
        }
    } else {
        if (dir == VERT_FILT) {
            /* vertical edges need to enable/disable individual columns
             * create mask, used at end with pblendv
             */
            writeMask = 0;
            if (edge->deblockP)
                writeMask |= (0x0000ffff);  /* write -2,-1 */
            if (edge->deblockQ)
                writeMask |= (0xffff0000);  /* write 0,+1 */

            if (dp >= sideThreshhold)
                writeMask &= (0xffffff00);  /* don't write -2 */
            if (dq >= sideThreshhold)
                writeMask &= (0x00ffffff);  /* don't write +1 */
        }

        /* common kernel for normal filter */

        /* do multiplier free: 9*(4-3) - 3*(5-1) */
        xmm4 = xmm0;
        xmm4 = _mm_slli_epi16(xmm4, 3);
        xmm4 = _mm_add_epi16(xmm4, xmm0);       /* 9*[3 3 3 3 4 4 4 4] */
        xmm5 = xmm1;
        xmm5 = _mm_slli_epi16(xmm5, 1);
        xmm5 = _mm_add_epi16(xmm5, xmm1);       /* 3*[2 2 2 2 5 5 5 5] */

        /* delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4 */
        xmm6 = xmm4;
        xmm6 = _mm_unpacklo_epi64(xmm6, xmm5);  /* 9*[3 3 3 3] | 3*[2 2 2 2] */
        xmm4 = _mm_unpackhi_epi64(xmm4, xmm5);  /* 9*[4 4 4 4] | 3*[5 5 5 5] */
        xmm4 = _mm_sub_epi16(xmm4, xmm6);       /* 9*(4-3) | 3*(5-2) */

        xmm5 = _mm_alignr_epi8(xmm5, xmm4, 8);  /* 3*(5-2) | 9*(4-3) */
        xmm4 = _mm_sub_epi16(xmm4, xmm5);
        xmm4 = _mm_add_epi16(xmm4, *(__m128i *)add8);
        xmm4 = _mm_srai_epi16(xmm4, 4);
        xmm4 = _mm_unpacklo_epi64(xmm4, xmm4);  /* replicate into top 64 */

        xmm6 = _mm_set1_epi16((short)(+tc));
        xmm7 = _mm_set1_epi16((short)(-tc));

        /* negate top 4 words of delta, clip to [-tc, +tc] */
        xmm5 = xmm4;
        xmm3 = _mm_setzero_si128();
        xmm3 = _mm_sub_epi16(xmm3, xmm5);
        xmm5 = _mm_unpacklo_epi64(xmm5, xmm3);
        xmm5 = _mm_max_epi16(xmm5, xmm7);
        xmm5 = _mm_min_epi16(xmm5, xmm6);       /* xmm5 = [+d +d +d +d -d -d -d -d] */

        /* p1 = p1 + (((p2 + p0 + 1) >> 1) - p1 + delta) >> 1
         * q1 = q1 + (((q2 + q0 + 1) >> 1) - q1 - delta) >> 1
         */
        xmm2 = _mm_add_epi16(xmm2, xmm0);
        xmm2 = _mm_add_epi16(xmm2, *(__m128i *)add1);
        xmm2 = _mm_srai_epi16(xmm2, 1);
        xmm2 = _mm_sub_epi16(xmm2, xmm1);
        xmm2 = _mm_add_epi16(xmm2, xmm5);
        xmm2 = _mm_srai_epi16(xmm2, 1);

        /* p0 = p0 + delta 
         * q0 = q0 - delta 
         */
        xmm0 = _mm_add_epi16(xmm0, xmm5);

        /* xmm6 = +(tc>>1), xmm7 = -(tc>>1) */
        xmm6 = _mm_srai_epi16(xmm6, 1);
        xmm7 = _mm_setzero_si128();
        xmm7 = _mm_subs_epi16(xmm7, xmm6);
        xmm2 = _mm_max_epi16(xmm2, xmm7);
        xmm2 = _mm_min_epi16(xmm2, xmm6);
        xmm1 = _mm_add_epi16(xmm1, xmm2);       /* p1 += tmp | q1 += tmp */

        /* clip p1,p0,q0,q1 to [0,255] */
        xmm0 = _mm_packus_epi16(xmm0, xmm0);    /* p0 (row 0-3) | q0 (row 0-3) */
        xmm1 = _mm_packus_epi16(xmm1, xmm1);    /* p1 (row 0-3) | q1 (row 0-3) */
        xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);   /* p0p1 p0p1 p0p1 p0p1 q0q1 q0q1 q0q1 q0q1 */

        if (dir == VERT_FILT) {
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)shufTabV);    /* put in order: row0 [-2 -1 0 1], row1 [-2 -1 0 1], ... */

            /* if (delta < 10*tc) */
            xmm3 = _mm_set1_epi16((short)(10*tc));
            xmm4 = _mm_abs_epi16(xmm4);
            xmm4 = _mm_sub_epi16(xmm4, xmm3);
            xmm4 = _mm_srai_epi16(xmm4, 15);            /* 16-bit masks for rows 0 to 3 */
            xmm4 = _mm_unpacklo_epi16(xmm4, xmm4);      /* expand to 32-bit masks (sign-extend to all 0's or all 1's) */

            xmm1 = _mm_set1_epi32(writeMask);           /* per-column mask (same each row) */
            xmm1 = _mm_and_si128(xmm1, xmm4);           /* and with delta < tc*10 mask */

            /* load existing 4x4 block, variable blend, write back */
            xmm2 = _mm_cvtsi32_si128(*(int *)(srcDst - 2 + 0*srcDstStride));
            xmm2 = _mm_insert_epi32(xmm2, *(int *)(srcDst - 2 + 1*srcDstStride), 1);
            xmm2 = _mm_insert_epi32(xmm2, *(int *)(srcDst - 2 + 2*srcDstStride), 2);
            xmm2 = _mm_insert_epi32(xmm2, *(int *)(srcDst - 2 + 3*srcDstStride), 3);
            xmm2 = _mm_blendv_epi8(xmm2, xmm0, xmm1);

            /* avoid pextrd (slow on Atom) */
            xmm3 = _mm_shuffle_epi32(xmm2, 0x01);
            xmm4 = _mm_shuffle_epi32(xmm2, 0x02);
            xmm5 = _mm_shuffle_epi32(xmm2, 0x03);

            *(int *)(srcDst - 2 + 0*srcDstStride) = _mm_cvtsi128_si32(xmm2);
            *(int *)(srcDst - 2 + 1*srcDstStride) = _mm_cvtsi128_si32(xmm3);
            *(int *)(srcDst - 2 + 2*srcDstStride) = _mm_cvtsi128_si32(xmm4);
            *(int *)(srcDst - 2 + 3*srcDstStride) = _mm_cvtsi128_si32(xmm5);
        } else {
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)shufTabH);    /* put in order: row0 [-2 -1 0 1], row1 [-2 -1 0 1], ... */

            /* if (delta < 10*tc) */
            xmm3 = _mm_set1_epi16((short)(10*tc));
            xmm4 = _mm_abs_epi16(xmm4);
            xmm4 = _mm_sub_epi16(xmm4, xmm3);
            xmm4 = _mm_srai_epi16(xmm4, 15);            /* 16-bit masks for columns 0 to 3 */
            xmm4 = _mm_srli_epi16(xmm4, 8);             /* convert to unsigned 0x0000 or 0x00FF */
            xmm4 = _mm_packus_epi16(xmm4, xmm4);        /* pack to 8-bit */
            xmm4 = _mm_shuffle_epi32(xmm4, 0x00);       /* replicate each 4-byte mask for each column */

            /* load existing 4x4 block, variable blend */
            xmm2 = _mm_cvtsi32_si128(*(int *)(srcDst - 2*srcDstStride));
            xmm2 = _mm_insert_epi32(xmm2, *(int *)(srcDst - 1*srcDstStride), 1);
            xmm2 = _mm_insert_epi32(xmm2, *(int *)(srcDst + 0*srcDstStride), 2);
            xmm2 = _mm_insert_epi32(xmm2, *(int *)(srcDst + 1*srcDstStride), 3);
            xmm2 = _mm_blendv_epi8(xmm2, xmm0, xmm4);   /* blend using delta mask */

            /* avoid pextrd (slow on Atom) */
            xmm3 = _mm_shuffle_epi32(xmm2, 0x01);
            xmm4 = _mm_shuffle_epi32(xmm2, 0x02);
            xmm5 = _mm_shuffle_epi32(xmm2, 0x03);

            /* let compiler decide best way to handle conditional writes */
            if (edge->deblockP && dp < sideThreshhold)      *(int *)(srcDst - 2*srcDstStride) = _mm_cvtsi128_si32(xmm2);
            if (edge->deblockP)                             *(int *)(srcDst - 1*srcDstStride) = _mm_cvtsi128_si32(xmm3);
            if (edge->deblockQ)                             *(int *)(srcDst + 0*srcDstStride) = _mm_cvtsi128_si32(xmm4);
            if (edge->deblockQ && dq < sideThreshhold)      *(int *)(srcDst + 1*srcDstStride) = _mm_cvtsi128_si32(xmm5);
        }
    }

    return strongFiltering;
}

/* slow C reference */
Ipp32s h265_FilterEdgeLuma_8u_I_C(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir)
{
    Ipp32s tcIdx, bIdx, tc, beta, sideThreshhold;
    Ipp32s i;
    Ipp32s dp0, dp3, dq0, dq3, d0, d3, dq, dp, d;
    Ipp32s ds0, ds3, dm0, dm3, delta;
    Ipp32s p0, p1, p2, p3, q0, q1, q2, q3, tmp;
    Ipp32s strongFiltering = 2; /* 2 = no filtering, only for counting statistics */
    Ipp8u *r[8];

    tcIdx = Clip3(0, 53, edge->qp + 2 * (edge->strength - 1) + edge->tcOffset);
    bIdx = Clip3(0, 51, edge->qp + edge->betaOffset);
    tc =  tcTable[tcIdx];
    beta = betaTable[bIdx];
    sideThreshhold = (beta + (beta >> 1)) >> 3;

    if (dir == VERT_FILT) {
        r[0] = srcDst + 0*srcDstStride - 4;
        r[1] = r[0] + srcDstStride;
        r[2] = r[1] + srcDstStride;
        r[3] = r[2] + srcDstStride;

        dp0 = abs(r[0][1] - 2*r[0][2] + r[0][3]);
        dq0 = abs(r[0][4] - 2*r[0][5] + r[0][6]);

        dp3 = abs(r[3][1] - 2*r[3][2] + r[3][3]);
        dq3 = abs(r[3][4] - 2*r[3][5] + r[3][6]);

        d0 = dp0 + dq0;
        d3 = dp3 + dq3;
        d = d0 + d3;

        if (d >= beta)
            return strongFiltering;

        dq = dq0 + dq3;
        dp = dp0 + dp3;

        /* since this is abs, can reverse the subtraction */
        ds0  = abs(r[0][0] - r[0][3]);
        ds0 += abs(r[0][4] - r[0][7]);

        ds3  = abs(r[3][0] - r[3][3]);
        ds3 += abs(r[3][4] - r[3][7]);

        dm0  = abs(r[0][4] - r[0][3]);
        dm3  = abs(r[3][4] - r[3][3]);

        strongFiltering = 0;
        if ((ds0 < (beta >> 3)) && (2*d0 < (beta >> 2)) && (dm0 < ((tc * 5 + 1) >> 1)) && (ds3 < (beta >> 3)) && (2*d3 < (beta >> 2)) && (dm3 < ((tc * 5 + 1) >> 1)))
            strongFiltering = 1;

        if (strongFiltering) {
            for (i = 0; i < 4; i++) {
                p0 = srcDst[-1];
                p1 = srcDst[-2];
                p2 = srcDst[-3];
                q0 = srcDst[0];
                q1 = srcDst[1];
                q2 = srcDst[2];

                if (edge->deblockP)
                {
                    p3 = srcDst[-4];
                    tmp = p1 + p0 + q0;
                    srcDst[-1] = (Ipp8u)(Clip3(p0 - 2 * tc, p0 + 2 * tc, (p2 + 2 * tmp + q1 + 4) >> 3));     //p0
                    srcDst[-2] = (Ipp8u)(Clip3(p1 - 2 * tc, p1 + 2 * tc, (p2 + tmp + 2) >> 2));              //p1
                    srcDst[-3] = (Ipp8u)(Clip3(p2 - 2 * tc, p2 + 2 * tc, (2 * p3 + 3 * p2 + tmp + 4) >> 3)); //p2
                }

                if (edge->deblockQ)
                {
                    q3 = srcDst[3];

                    tmp = q1 + q0 + p0;
                    srcDst[0] = (Ipp8u)(Clip3(q0 - 2 * tc, q0 + 2 * tc, (q2 + 2 * tmp + p1 + 4) >> 3));     //q0
                    srcDst[1] = (Ipp8u)(Clip3(q1 - 2 * tc, q1 + 2 * tc, (q2 + tmp + 2) >> 2));              //q1
                    srcDst[2] = (Ipp8u)(Clip3(q2 - 2 * tc, q2 + 2 * tc, (2 * q3 + 3 * q2 + tmp + 4) >> 3)); //q2
                }
                srcDst += srcDstStride;
            }
        } else {
            for (i = 0; i < 4; i++) {
                p0 = srcDst[-1];
                p1 = srcDst[-2];
                p2 = srcDst[-3];
                q0 = srcDst[0];
                q1 = srcDst[1];
                q2 = srcDst[2];

                delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;

                if (abs(delta) < tc * 10)
                {
                    delta = Clip3(-tc, tc, delta);

                    if (edge->deblockP)
                    {
                        srcDst[-1] = (Ipp8u)(Clip3(0, 255, (p0 + delta)));

                        if (dp < sideThreshhold)
                        {
                            tmp = Clip3(-(tc >> 1), (tc >> 1), ((((p2 + p0 + 1) >> 1) - p1 + delta) >> 1));
                            srcDst[-2] = (Ipp8u)(Clip3(0, 255, (p1 + tmp)));
                        }
                    }

                    if (edge->deblockQ)
                    {
                        srcDst[0] = (Ipp8u)(Clip3(0, 255, (q0 - delta)));

                        if (dq < sideThreshhold)
                        {
                            tmp = Clip3(-(tc >> 1), (tc >> 1), ((((q2 + q0 + 1) >> 1) - q1 - delta) >> 1));
                            srcDst[1] = (Ipp8u)(Clip3(0, 255, (q1 + tmp)));
                        }
                    }
                }

                srcDst += srcDstStride;
            }
        }
    } else {
        r[0] = srcDst - 4*srcDstStride;
        r[1] = r[0] + srcDstStride;
        r[2] = r[1] + srcDstStride;
        r[3] = r[2] + srcDstStride;
        r[4] = r[3] + srcDstStride;
        r[5] = r[4] + srcDstStride;
        r[6] = r[5] + srcDstStride;
        r[7] = r[6] + srcDstStride;

        dp0 = abs(r[1][0] - 2*r[2][0] + r[3][0]);
        dq0 = abs(r[4][0] - 2*r[5][0] + r[6][0]);

        dp3 = abs(r[1][3] - 2*r[2][3] + r[3][3]);
        dq3 = abs(r[4][3] - 2*r[5][3] + r[6][3]);

        d0 = dp0 + dq0;
        d3 = dp3 + dq3;

        dq = dq0 + dq3;
        dp = dp0 + dp3;

        d = d0 + d3;

        if (d >= beta)
            return strongFiltering;

        /* since this is abs, can reverse the subtraction */
        ds0  = abs(r[0][0] - r[3][0]);
        ds0 += abs(r[4][0] - r[7][0]);

        ds3  = abs(r[0][3] - r[3][3]);
        ds3 += abs(r[4][3] - r[7][3]);

        dm0  = abs(r[4][0] - r[3][0]);
        dm3  = abs(r[4][3] - r[3][3]);

        strongFiltering = 0;
        if ((ds0 < (beta >> 3)) && (2 * d0 < (beta >> 2)) && (dm0 < ((tc * 5 + 1) >> 1)) && (ds3 < (beta >> 3)) && (2 * d3 < (beta >> 2)) && (dm3 < ((tc * 5 + 1) >> 1)))
            strongFiltering = 1;

        if (strongFiltering) {
            for (i = 0; i < 4; i++)
            {
                p0 = srcDst[-1*srcDstStride];
                p1 = srcDst[-2*srcDstStride];
                p2 = srcDst[-3*srcDstStride];
                q0 = srcDst[0*srcDstStride];
                q1 = srcDst[1*srcDstStride];
                q2 = srcDst[2*srcDstStride];

                if (edge->deblockP)
                {
                    p3 = srcDst[-4*srcDstStride];
                    tmp = p1 + p0 + q0;
                    srcDst[-1*srcDstStride] = (Ipp8u)(Clip3(p0 - 2 * tc, p0 + 2 * tc, (p2 + 2 * tmp + q1 + 4) >> 3));     //p0
                    srcDst[-2*srcDstStride] = (Ipp8u)(Clip3(p1 - 2 * tc, p1 + 2 * tc, (p2 + tmp + 2) >> 2));              //p1
                    srcDst[-3*srcDstStride] = (Ipp8u)(Clip3(p2 - 2 * tc, p2 + 2 * tc, (2 * p3 + 3 * p2 + tmp + 4) >> 3)); //p2
                }

                if (edge->deblockQ)
                {
                    q3 = srcDst[3*srcDstStride];

                    tmp = q1 + q0 + p0;
                    srcDst[0*srcDstStride] = (Ipp8u)(Clip3(q0 - 2 * tc, q0 + 2 * tc, (q2 + 2 * tmp + p1 + 4) >> 3));     //q0
                    srcDst[1*srcDstStride] = (Ipp8u)(Clip3(q1 - 2 * tc, q1 + 2 * tc, (q2 + tmp + 2) >> 2));              //q1
                    srcDst[2*srcDstStride] = (Ipp8u)(Clip3(q2 - 2 * tc, q2 + 2 * tc, (2 * q3 + 3 * q2 + tmp + 4) >> 3)); //q2
                }
                srcDst += 1;
            }
        } else {
            for (i = 0; i < 4; i++)
            {
                p0 = srcDst[-1*srcDstStride];
                p1 = srcDst[-2*srcDstStride];
                p2 = srcDst[-3*srcDstStride];
                q0 = srcDst[0*srcDstStride];
                q1 = srcDst[1*srcDstStride];
                q2 = srcDst[2*srcDstStride];
                delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;

                if (abs(delta) < tc * 10)
                {
                    delta = Clip3(-tc, tc, delta);

                    if (edge->deblockP)
                    {
                        srcDst[-1*srcDstStride] = (Ipp8u)(Clip3(0, 255, (p0 + delta)));

                        if (dp < sideThreshhold)
                        {
                            tmp = Clip3(-(tc >> 1), (tc >> 1), ((((p2 + p0 + 1) >> 1) - p1 + delta) >> 1));
                            srcDst[-2*srcDstStride] = (Ipp8u)(Clip3(0, 255, (p1 + tmp)));
                        }
                    }

                    if (edge->deblockQ)
                    {
                        srcDst[0] = (Ipp8u)(Clip3(0, 255, (q0 - delta)));

                        if (dq < sideThreshhold)
                        {
                            tmp = Clip3(-(tc >> 1), (tc >> 1), ((((q2 + q0 + 1) >> 1) - q1 - delta) >> 1));
                            srcDst[1*srcDstStride] = (Ipp8u)(Clip3(0, 255, (q1 + tmp)));
                        }
                    }
                }
                srcDst += 1;
            }
        }
    }

    return strongFiltering;
}

}; // namespace MFX_HEVC_COMMON

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
