//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_AVX2) && defined(MFX_TARGET_OPTIMIZATION_AVX2)

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (double *)(p)))

namespace MFX_HEVC_PP
{

ALIGN_DECL(32) static const Ipp8s shufTab4[32] = {  
    0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15,
    0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15 
};

ALIGN_DECL(32) static const Ipp16s mulAddOne[16] = {+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1};
ALIGN_DECL(32) static const Ipp16s mulSubOne[16] = {+1,-1,+1,-1,+1,-1,+1,-1,+1,-1,+1,-1,+1,-1,+1,-1};

/* h265_SATD_4x4() - 4x4 SATD adapted from IPP source code
 *
 * multiply-free implementation of sum(abs(T*D*T))
 *   D = difference matrix (cur - ref)
 *   T = transform matrix (values of +/-1, see IPP docs)
 *
 * implemented as sum(abs(T*transpose(T*D)))
 * final matrix is transposed, but doesn't matter since we just sum all the elements
 *
 * supports up to 10-bit video input (with 16u version)
 */
template <typename PixType>
static Ipp32s h265_SATD_4x4_Kernel(const PixType* pSrcCur, int srcCurStep, const PixType* pSrcRef, int srcRefStep)
{
    __m128i  xmm0, xmm1, xmm2, xmm3;
    __m256i  ymm0, ymm1, ymm2, ymm3, ymm7;

    _mm256_zeroupper();

    ymm7 = _mm256_load_si256((__m256i*)shufTab4);

    /* difference matrix (4x4) = cur - ref */
    if (sizeof(PixType) == 2) {
        /* 16-bit */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrcCur + 0*srcCurStep));
        xmm0 = _mm_loadh_epi64(xmm0, (__m128i*)(pSrcCur + 1*srcCurStep));
        xmm2 = _mm_loadl_epi64((__m128i*)(pSrcCur + 2*srcCurStep));
        xmm2 = _mm_loadh_epi64(xmm2, (__m128i*)(pSrcCur + 3*srcCurStep));

        xmm1 = _mm_loadl_epi64((__m128i*)(pSrcRef + 0*srcRefStep));
        xmm1 = _mm_loadh_epi64(xmm1, (__m128i*)(pSrcRef + 1*srcRefStep));
        xmm3 = _mm_loadl_epi64((__m128i*)(pSrcRef + 2*srcRefStep));
        xmm3 = _mm_loadh_epi64(xmm3, (__m128i*)(pSrcRef + 3*srcRefStep));

        ymm0 = _mm256_permute2x128_si256(mm256(xmm0), mm256(xmm2), 0x20);
        ymm1 = _mm256_permute2x128_si256(mm256(xmm1), mm256(xmm3), 0x20);
    } else {
        /* 8-bit */
        xmm0 = _mm_cvtsi32_si128(*(int*)(pSrcCur + 0*srcCurStep));
        xmm0 = _mm_insert_epi32(xmm0, *(int*)(pSrcCur + 1*srcCurStep), 0x01);
        xmm0 = _mm_insert_epi32(xmm0, *(int*)(pSrcCur + 2*srcCurStep), 0x02);
        xmm0 = _mm_insert_epi32(xmm0, *(int*)(pSrcCur + 3*srcCurStep), 0x03);

        xmm1 = _mm_cvtsi32_si128(*(int*)(pSrcRef + 0*srcRefStep));
        xmm1 = _mm_insert_epi32(xmm1, *(int*)(pSrcRef + 1*srcRefStep), 0x01);
        xmm1 = _mm_insert_epi32(xmm1, *(int*)(pSrcRef + 2*srcRefStep), 0x02);
        xmm1 = _mm_insert_epi32(xmm1, *(int*)(pSrcRef + 3*srcRefStep), 0x03);

        ymm0 = _mm256_cvtepu8_epi16(xmm0);
        ymm1 = _mm256_cvtepu8_epi16(xmm1);
    }

    ymm0 = _mm256_sub_epi16(ymm0, ymm1);        /* diff [row0 row1 row2 row3] */

    ymm2 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);

    /* first stage */
    ymm1 = _mm256_sub_epi16(ymm0, ymm2);        /* [row0-row2 row1-row3] */
    ymm0 = _mm256_add_epi16(ymm0, ymm2);        /* [row0+row2 row1+row3] */
    
    ymm2 = _mm256_unpacklo_epi64(ymm0, ymm1);   /* row0+row2 row0-row2 */
    ymm0 = _mm256_unpackhi_epi64(ymm0, ymm1);   /* row1+row3 row1-row3 */

    ymm3 = _mm256_sub_epi16(ymm2, ymm0);        /* row2: 0-1+2-3  row3: 0-1-2+3 */
    ymm2 = _mm256_add_epi16(ymm2, ymm0);        /* row0: 0+1+2+3  row1: 0+1-2-3 */

    /* transpose */
    ymm2 = _mm256_shuffle_epi8(ymm2, ymm7);
    ymm3 = _mm256_shuffle_epi8(ymm3, ymm7);

    ymm0 = _mm256_unpacklo_epi32(ymm2, ymm3);   /* rows 0-3 [0 0 0 0 1 1 1 1] */
    ymm2 = _mm256_unpackhi_epi32(ymm2, ymm3);   /* rows 0-3 [2 2 2 2 3 3 3 3] */

    /* second stage */
    ymm1 = _mm256_sub_epi16(ymm0, ymm2);        /* [row0-row2 row1-row3] */
    ymm0 = _mm256_add_epi16(ymm0, ymm2);        /* [row0+row2 row1+row3] */
    
    ymm2 = _mm256_unpacklo_epi64(ymm0, ymm1);   /* row0+row2 row0-row2 */
    ymm0 = _mm256_unpackhi_epi64(ymm0, ymm1);   /* row1+row3 row1-row3 */

    ymm3 = _mm256_sub_epi16(ymm2, ymm0);        /* row2: 0-1+2-3  row3: 0-1-2+3 */
    ymm2 = _mm256_add_epi16(ymm2, ymm0);        /* row0: 0+1+2+3  row1: 0+1-2-3 */

    /* sum abs values - avoid phadd (faster to shuffle) */
    ymm2 = _mm256_abs_epi16(ymm2);
    ymm3 = _mm256_abs_epi16(ymm3);
    ymm2 = _mm256_add_epi16(ymm2, ymm3);

    ymm3 = _mm256_shuffle_epi32(ymm2, 0x0E);
    ymm2 = _mm256_add_epi16(ymm2, ymm3);
    ymm3 = _mm256_shuffle_epi32(ymm2, 0x01);
    ymm2 = _mm256_add_epi16(ymm2, ymm3);
    ymm3 = _mm256_shufflelo_epi16(ymm2, 0x01);
    ymm2 = _mm256_add_epi16(ymm2, ymm3);
    xmm2 = _mm_cvtepu16_epi32(mm128(ymm2));

    return _mm_cvtsi128_si32(xmm2);
}

Ipp32s H265_FASTCALL MAKE_NAME(h265_SATD_4x4_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep)
{
    return h265_SATD_4x4_Kernel<Ipp8u>(pSrcCur, srcCurStep, pSrcRef, srcRefStep);
}

Ipp32s H265_FASTCALL MAKE_NAME(h265_SATD_4x4_16u)(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep)
{
    return h265_SATD_4x4_Kernel<Ipp16u>(pSrcCur, srcCurStep, pSrcRef, srcRefStep);
}

/* h265_SATD_4x4_Pair() - pairs of 4x4 SATD's
 *
 * load two consecutive 4x4 blocks for cur and ref
 * use low and high halves of ymm registers to process in parallel
 * avoids lane-crossing penalties (except for getting final sum from top half)
 *
 * supports up to 10-bit video input (with 16u version)
 */
template <typename PixType>
static void h265_SATD_4x4_Pair_Kernel(const PixType* pSrcCur, int srcCurStep, const PixType* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    __m128i  xmm0, xmm1, xmm2, xmm3, xmm4;
    __m256i  ymm0, ymm1, ymm2, ymm3, ymm7;

    _mm256_zeroupper();

    ymm7 = _mm256_load_si256((__m256i*)shufTab4);

    if (sizeof(PixType) == 2) {
        /* 16-bit */
        xmm0 = _mm_loadu_si128((__m128i* )(pSrcCur + 0*srcCurStep));
        xmm4 = _mm_loadu_si128((__m128i* )(pSrcCur + 1*srcCurStep));
        ymm0 = _mm256_permute2x128_si256(mm256(xmm0), mm256(xmm4), 0x20);
        ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);

        xmm2 = _mm_loadu_si128((__m128i* )(pSrcCur + 2*srcCurStep));
        xmm4 = _mm_loadu_si128((__m128i* )(pSrcCur + 3*srcCurStep));
        ymm2 = _mm256_permute2x128_si256(mm256(xmm2), mm256(xmm4), 0x20);
        ymm2 = _mm256_permute4x64_epi64(ymm2, 0xd8);

        xmm1 = _mm_loadu_si128((__m128i* )(pSrcRef + 0*srcRefStep));
        xmm4 = _mm_loadu_si128((__m128i* )(pSrcRef + 1*srcRefStep));
        ymm1 = _mm256_permute2x128_si256(mm256(xmm1), mm256(xmm4), 0x20);
        ymm1 = _mm256_permute4x64_epi64(ymm1, 0xd8);

        xmm3 = _mm_loadu_si128((__m128i* )(pSrcRef + 2*srcRefStep));
        xmm4 = _mm_loadu_si128((__m128i* )(pSrcRef + 3*srcRefStep));
        ymm3 = _mm256_permute2x128_si256(mm256(xmm3), mm256(xmm4), 0x20);
        ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
    } else {
        /* 8-bit */
        xmm0 = _mm_loadl_epi64((__m128i* )(pSrcCur + 0*srcCurStep));
        xmm4 = _mm_loadl_epi64((__m128i* )(pSrcCur + 1*srcCurStep));
        xmm0 = _mm_unpacklo_epi32(xmm0, xmm4);

        xmm2 = _mm_loadl_epi64((__m128i* )(pSrcCur + 2*srcCurStep));
        xmm4 = _mm_loadl_epi64((__m128i* )(pSrcCur + 3*srcCurStep));
        xmm2 = _mm_unpacklo_epi32(xmm2, xmm4);

        xmm1 = _mm_loadl_epi64((__m128i* )(pSrcRef + 0*srcRefStep));
        xmm4 = _mm_loadl_epi64((__m128i* )(pSrcRef + 1*srcRefStep));
        xmm1 = _mm_unpacklo_epi32(xmm1, xmm4);

        xmm3 = _mm_loadl_epi64((__m128i* )(pSrcRef + 2*srcRefStep));
        xmm4 = _mm_loadl_epi64((__m128i* )(pSrcRef + 3*srcRefStep));
        xmm3 = _mm_unpacklo_epi32(xmm3, xmm4);

        ymm0 = _mm256_cvtepu8_epi16(xmm0);
        ymm1 = _mm256_cvtepu8_epi16(xmm1);
        ymm2 = _mm256_cvtepu8_epi16(xmm2);
        ymm3 = _mm256_cvtepu8_epi16(xmm3);
    }
    ymm0 = _mm256_sub_epi16(ymm0, ymm1);
    ymm2 = _mm256_sub_epi16(ymm2, ymm3);

    /* first stage */
    ymm1 = _mm256_sub_epi16(ymm0, ymm2);
    ymm0 = _mm256_add_epi16(ymm0, ymm2);
    
    ymm2 = _mm256_unpacklo_epi64(ymm0, ymm1);
    ymm0 = _mm256_unpackhi_epi64(ymm0, ymm1);

    ymm3 = _mm256_sub_epi16(ymm2, ymm0);
    ymm2 = _mm256_add_epi16(ymm2, ymm0);

    /* transpose */
    ymm2 = _mm256_shuffle_epi8(ymm2, ymm7);
    ymm3 = _mm256_shuffle_epi8(ymm3, ymm7);

    ymm0 = _mm256_unpacklo_epi32(ymm2, ymm3);
    ymm2 = _mm256_unpackhi_epi32(ymm2, ymm3);

    /* second stage */
    ymm1 = _mm256_sub_epi16(ymm0, ymm2);
    ymm0 = _mm256_add_epi16(ymm0, ymm2);
    
    ymm2 = _mm256_unpacklo_epi64(ymm0, ymm1);
    ymm0 = _mm256_unpackhi_epi64(ymm0, ymm1);

    ymm3 = _mm256_sub_epi16(ymm2, ymm0);
    ymm2 = _mm256_add_epi16(ymm2, ymm0);

    /* sum abs values - avoid phadd (faster to shuffle) */
    ymm2 = _mm256_abs_epi16(ymm2);
    ymm3 = _mm256_abs_epi16(ymm3);
    ymm2 = _mm256_add_epi16(ymm2, ymm3);

    ymm3 = _mm256_shuffle_epi32(ymm2, 0x0E);
    ymm2 = _mm256_add_epi16(ymm2, ymm3);
    ymm3 = _mm256_shuffle_epi32(ymm2, 0x01);
    ymm2 = _mm256_add_epi16(ymm2, ymm3);
    ymm3 = _mm256_shufflelo_epi16(ymm2, 0x01);
    ymm2 = _mm256_add_epi16(ymm2, ymm3);
    ymm3 = _mm256_permute2x128_si256(ymm2, ymm2, 0x01);

    xmm2 = _mm_cvtepu16_epi32(mm128(ymm2));
    xmm3 = _mm_cvtepu16_epi32(mm128(ymm3));

    satdPair[0] = _mm_cvtsi128_si32(xmm2);
    satdPair[1] = _mm_cvtsi128_si32(xmm3);
}

void H265_FASTCALL MAKE_NAME(h265_SATD_4x4_Pair_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    h265_SATD_4x4_Pair_Kernel<Ipp8u>(pSrcCur, srcCurStep, pSrcRef, srcRefStep, satdPair);
}

void H265_FASTCALL MAKE_NAME(h265_SATD_4x4_Pair_16u)(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    h265_SATD_4x4_Pair_Kernel<Ipp16u>(pSrcCur, srcCurStep, pSrcRef, srcRefStep, satdPair);
}

/* h265_SATD_8x8() - 8x8 SATD adapted from IPP source code
 *
 * multiply-free implementation of sum(abs(T*D*T)) (see comments on SSE version)
 * using full 256 bit registers for single 8x8 transform requires permutes to load data
 *   but avoids spilling to stack as in SSE
 * only partial transpose is required, avoiding cross-lane penalties
 *
 * supports up to 10-bit video input (with 16u version)
 */
template <typename PixType>
static Ipp32s h265_SATD_8x8_Kernel(const PixType* pSrcCur, int srcCurStep, const PixType* pSrcRef, int srcRefStep)
{
    PixType *pCT, *pRT;
    int stepC4, stepR4;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;

    pCT = (PixType*)pSrcCur;
    pRT = (PixType*)pSrcRef;
    stepC4 = 4*srcCurStep;
    stepR4 = 4*srcRefStep;

    _mm256_zeroupper();

    /* ymm registers have [row0 row4], [row1 row5], etc */
    if (sizeof(PixType) == 2) {
        /* 16-bit */
        xmm0 = _mm_loadu_si128((__m128i*)(pCT));
        xmm5 = _mm_loadu_si128((__m128i*)(pRT));
        xmm0 = _mm_sub_epi16(xmm0, xmm5);
        xmm5 = _mm_loadu_si128((__m128i*)(pCT + stepC4));
        xmm4 = _mm_loadu_si128((__m128i*)(pRT + stepR4));
        xmm5 = _mm_sub_epi16(xmm5, xmm4);
        xmm4 = _mm_sub_epi16(xmm0, xmm5);
        xmm0 = _mm_add_epi16(xmm0, xmm5);
        ymm0 = _mm256_inserti128_si256(mm256(xmm0), xmm4, 0x01);
        pCT += srcCurStep;
        pRT += srcRefStep;

        xmm1 = _mm_loadu_si128((__m128i*)(pCT));
        xmm5 = _mm_loadu_si128((__m128i*)(pRT));
        xmm1 = _mm_sub_epi16(xmm1, xmm5);
        xmm5 = _mm_loadu_si128((__m128i*)(pCT + stepC4));
        xmm4 = _mm_loadu_si128((__m128i*)(pRT + stepR4));
        xmm5 = _mm_sub_epi16(xmm5, xmm4);
        xmm4 = _mm_sub_epi16(xmm1, xmm5);
        xmm1 = _mm_add_epi16(xmm1, xmm5);
        ymm1 = _mm256_inserti128_si256(mm256(xmm1), xmm4, 0x01);
        pCT += srcCurStep;
        pRT += srcRefStep;

        xmm2 = _mm_loadu_si128((__m128i*)(pCT));
        xmm5 = _mm_loadu_si128((__m128i*)(pRT));
        xmm2 = _mm_sub_epi16(xmm2, xmm5);
        xmm5 = _mm_loadu_si128((__m128i*)(pCT + stepC4));
        xmm4 = _mm_loadu_si128((__m128i*)(pRT + stepR4));
        xmm5 = _mm_sub_epi16(xmm5, xmm4);
        xmm4 = _mm_sub_epi16(xmm2, xmm5);
        xmm2 = _mm_add_epi16(xmm2, xmm5);
        ymm2 = _mm256_inserti128_si256(mm256(xmm2), xmm4, 0x01);
        pCT += srcCurStep;
        pRT += srcRefStep;

        xmm3 = _mm_loadu_si128((__m128i*)(pCT));
        xmm5 = _mm_loadu_si128((__m128i*)(pRT));
        xmm3 = _mm_sub_epi16(xmm3, xmm5);
        xmm5 = _mm_loadu_si128((__m128i*)(pCT + stepC4));
        xmm4 = _mm_loadu_si128((__m128i*)(pRT + stepR4));
        xmm5 = _mm_sub_epi16(xmm5, xmm4);
        xmm4 = _mm_sub_epi16(xmm3, xmm5);
        xmm3 = _mm_add_epi16(xmm3, xmm5);
        ymm3 = _mm256_inserti128_si256(mm256(xmm3), xmm4, 0x01);
    } else {
        /* 8-bit*/
        xmm0 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
        xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
        xmm0 = _mm_sub_epi16(xmm0, xmm5);
        xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
        xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
        xmm5 = _mm_sub_epi16(xmm5, xmm4);
        xmm4 = _mm_sub_epi16(xmm0, xmm5);
        xmm0 = _mm_add_epi16(xmm0, xmm5);
        ymm0 = _mm256_inserti128_si256(mm256(xmm0), xmm4, 0x01);
        pCT += srcCurStep;
        pRT += srcRefStep;

        xmm1 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
        xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
        xmm1 = _mm_sub_epi16(xmm1, xmm5);
        xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
        xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
        xmm5 = _mm_sub_epi16(xmm5, xmm4);
        xmm4 = _mm_sub_epi16(xmm1, xmm5);
        xmm1 = _mm_add_epi16(xmm1, xmm5);
        ymm1 = _mm256_inserti128_si256(mm256(xmm1), xmm4, 0x01);
        pCT += srcCurStep;
        pRT += srcRefStep;

        xmm2 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
        xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
        xmm2 = _mm_sub_epi16(xmm2, xmm5);
        xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
        xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
        xmm5 = _mm_sub_epi16(xmm5, xmm4);
        xmm4 = _mm_sub_epi16(xmm2, xmm5);
        xmm2 = _mm_add_epi16(xmm2, xmm5);
        ymm2 = _mm256_inserti128_si256(mm256(xmm2), xmm4, 0x01);
        pCT += srcCurStep;
        pRT += srcRefStep;

        xmm3 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
        xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
        xmm3 = _mm_sub_epi16(xmm3, xmm5);
        xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
        xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
        xmm5 = _mm_sub_epi16(xmm5, xmm4);
        xmm4 = _mm_sub_epi16(xmm3, xmm5);
        xmm3 = _mm_add_epi16(xmm3, xmm5);
        ymm3 = _mm256_inserti128_si256(mm256(xmm3), xmm4, 0x01);
    }

    /* first stage */
    ymm4 = _mm256_add_epi16(ymm2, ymm3);
    ymm2 = _mm256_sub_epi16(ymm2, ymm3);

    ymm3 = _mm256_sub_epi16(ymm0, ymm1);
    ymm0 = _mm256_add_epi16(ymm0, ymm1);

    ymm1 = _mm256_add_epi16(ymm3, ymm2);
    ymm3 = _mm256_sub_epi16(ymm3, ymm2);

    ymm2 = _mm256_sub_epi16(ymm0, ymm4);
    ymm0 = _mm256_add_epi16(ymm0, ymm4);

    /* transpose and split */
    ymm4 = _mm256_unpackhi_epi64(ymm0, ymm2);
    ymm0 = _mm256_unpacklo_epi64(ymm0, ymm2);

    ymm2 = _mm256_sub_epi16(ymm0, ymm4);
    ymm0 = _mm256_add_epi16(ymm0, ymm4);

    ymm4 = _mm256_unpackhi_epi64(ymm1, ymm3);
    ymm1 = _mm256_unpacklo_epi64(ymm1, ymm3);

    ymm3 = _mm256_sub_epi16(ymm1, ymm4);
    ymm1 = _mm256_add_epi16(ymm1, ymm4);

    ymm4 = _mm256_unpackhi_epi16(ymm0, ymm1);
    ymm0 = _mm256_unpacklo_epi16(ymm0, ymm1);

    ymm5 = _mm256_unpackhi_epi16(ymm2, ymm3);
    ymm2 = _mm256_unpacklo_epi16(ymm2, ymm3);

    ymm1 = _mm256_unpackhi_epi32(ymm0, ymm2);
    ymm0 = _mm256_unpacklo_epi32(ymm0, ymm2);

    ymm2 = _mm256_unpacklo_epi32(ymm4, ymm5);
    ymm3 = _mm256_unpackhi_epi32(ymm4, ymm5);

    /* second stage - don't worry about final matrix permutation since we just sum the elements in the end */
    ymm4 = ymm0;
    ymm5 = ymm1;
    ymm0 = _mm256_unpacklo_epi64(ymm4, ymm2);
    ymm1 = _mm256_unpackhi_epi64(ymm4, ymm2);
    ymm2 = _mm256_unpacklo_epi64(ymm5, ymm3);
    ymm3 = _mm256_unpackhi_epi64(ymm5, ymm3);

    ymm4 = _mm256_add_epi16(ymm2, ymm3);
    ymm2 = _mm256_sub_epi16(ymm2, ymm3);

    ymm3 = _mm256_sub_epi16(ymm0, ymm1);
    ymm0 = _mm256_add_epi16(ymm0, ymm1);

    if (sizeof(PixType) == 2) {
        __m256i ymm6;

        /* to avoid overflow switch to 32-bit accumulators */
        ymm5 = _mm256_unpacklo_epi16(ymm3, ymm2);
        ymm6 = _mm256_unpackhi_epi16(ymm3, ymm2);
        ymm2 = ymm5;
        ymm3 = ymm6;

        ymm5 = _mm256_abs_epi32( _mm256_madd_epi16(ymm5, *(__m256i *)mulAddOne) );
        ymm6 = _mm256_abs_epi32( _mm256_madd_epi16(ymm6, *(__m256i *)mulAddOne) );
        ymm2 = _mm256_abs_epi32( _mm256_madd_epi16(ymm2, *(__m256i *)mulSubOne) );
        ymm3 = _mm256_abs_epi32( _mm256_madd_epi16(ymm3, *(__m256i *)mulSubOne) );

        ymm2 = _mm256_add_epi32(ymm2, ymm3);
        ymm2 = _mm256_add_epi32(ymm2, ymm5);
        ymm2 = _mm256_add_epi32(ymm2, ymm6);

        ymm5 = _mm256_unpacklo_epi16(ymm0, ymm4);
        ymm6 = _mm256_unpackhi_epi16(ymm0, ymm4);
        ymm4 = ymm5;
        ymm3 = ymm6;

        ymm5 = _mm256_abs_epi32( _mm256_madd_epi16(ymm5, *(__m256i *)mulAddOne) );
        ymm6 = _mm256_abs_epi32( _mm256_madd_epi16(ymm6, *(__m256i *)mulAddOne) );
        ymm4 = _mm256_abs_epi32( _mm256_madd_epi16(ymm4, *(__m256i *)mulSubOne) );
        ymm3 = _mm256_abs_epi32( _mm256_madd_epi16(ymm3, *(__m256i *)mulSubOne) );

        ymm0 = _mm256_add_epi32(ymm2, ymm5);
        ymm0 = _mm256_add_epi32(ymm0, ymm6);
        ymm0 = _mm256_add_epi32(ymm0, ymm4);
        ymm0 = _mm256_add_epi32(ymm0, ymm3);
    } else {
        ymm1 = _mm256_add_epi16(ymm3, ymm2);
        ymm3 = _mm256_sub_epi16(ymm3, ymm2);

        ymm2 = _mm256_sub_epi16(ymm0, ymm4);
        ymm0 = _mm256_add_epi16(ymm0, ymm4);

        /* final abs and sum */
        ymm0 = _mm256_abs_epi16(ymm0);
        ymm1 = _mm256_abs_epi16(ymm1);
        ymm2 = _mm256_abs_epi16(ymm2);
        ymm3 = _mm256_abs_epi16(ymm3);

        ymm0 = _mm256_add_epi16(ymm0, ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm2);
        ymm0 = _mm256_add_epi16(ymm0, ymm3);

        /* use single packed 16*16->32 multiply for first step (slightly faster) */
        ymm0 = _mm256_madd_epi16(ymm0, _mm256_set1_epi16(1));
    }
    ymm1 = _mm256_shuffle_epi32(ymm0, 0x0E);
    ymm0 = _mm256_add_epi32(ymm0, ymm1);
    ymm1 = _mm256_shuffle_epi32(ymm0, 0x01);
    ymm0 = _mm256_add_epi32(ymm0, ymm1);
    xmm1 = _mm256_extracti128_si256(ymm0, 1);
    xmm0 = _mm_add_epi32(mm128(ymm0), xmm1);

    return _mm_cvtsi128_si32(xmm0);
}

Ipp32s H265_FASTCALL MAKE_NAME(h265_SATD_8x8_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep)
{
    return h265_SATD_8x8_Kernel<Ipp8u>(pSrcCur, srcCurStep, pSrcRef, srcRefStep);
}

Ipp32s H265_FASTCALL MAKE_NAME(h265_SATD_8x8_16u)(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep)
{
    return h265_SATD_8x8_Kernel<Ipp16u>(pSrcCur, srcCurStep, pSrcRef, srcRefStep);
}

/* h265_SATD_8x8_Pair_8u() - 8x8 SATD adapted from IPP source code
 * 
 * load two consecutive 8x8 blocks for cur and ref
 * use low and high halves of ymm registers to process in parallel (double-width version of IPP SSE algorithm)
 * avoids lane-crossing penalties (except for loading pixels and storing final sum)
 */
void H265_FASTCALL MAKE_NAME(h265_SATD_8x8_Pair_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    const Ipp8u *pS1, *pS2;
    Ipp32s  s;

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
    __m256i ymm_s0, ymm_s1, ymm_s2, ymm_one;

    static ALIGN_DECL(32) Ipp8s ymm_const11nb[32] = {1,1,1,1,1,1,1,1,1,-1,1,-1,1,-1,1,-1,   1,1,1,1,1,1,1,1,1,-1,1,-1,1,-1,1,-1};

    _mm256_zeroupper();

    pS1 = pSrcCur;
    pS2 = pSrcRef;

    ymm7 = _mm256_load_si256((__m256i*)&(ymm_const11nb[0]));
    ymm_one = _mm256_set1_epi16(1);

    ymm0 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
    ymm5 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
    ymm6 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+srcRefStep))), 0x50);
    ymm1 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+srcCurStep))), 0x50);
    pS2 += 2 * srcRefStep;
    pS1 += 2 * srcCurStep;
    ymm0 = _mm256_maddubs_epi16(ymm0, ymm7);
    ymm5 = _mm256_maddubs_epi16(ymm5, ymm7);
    ymm0 = _mm256_subs_epi16(ymm0, ymm5);
    ymm1 = _mm256_maddubs_epi16(ymm1, ymm7);
    ymm6 = _mm256_maddubs_epi16(ymm6, ymm7);
    ymm1 = _mm256_subs_epi16(ymm1, ymm6);

    ymm2 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
    ymm5 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
    ymm3 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+srcCurStep))), 0x50);
    ymm6 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+srcRefStep))), 0x50);
    pS2 += 2 * srcRefStep;
    pS1 += 2 * srcCurStep;
    ymm2 = _mm256_maddubs_epi16(ymm2, ymm7);
    ymm5 = _mm256_maddubs_epi16(ymm5, ymm7);
    ymm2 = _mm256_subs_epi16(ymm2, ymm5);
    ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
    ymm6 = _mm256_maddubs_epi16(ymm6, ymm7);
    ymm3 = _mm256_subs_epi16(ymm3, ymm6);

    ymm_s0 = ymm2;
    ymm_s1 = ymm3;
    ymm_s2 = ymm1;

    ymm4 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
    ymm2 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
    ymm5 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+srcCurStep))), 0x50);
    ymm3 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+srcRefStep))), 0x50);
    pS2 += 2 * srcRefStep;
    pS1 += 2 * srcCurStep;
    ymm4 = _mm256_maddubs_epi16(ymm4, ymm7);
    ymm2 = _mm256_maddubs_epi16(ymm2, ymm7);
    ymm4 = _mm256_subs_epi16(ymm4, ymm2);
    ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
    ymm5 = _mm256_maddubs_epi16(ymm5, ymm7);
    ymm5 = _mm256_subs_epi16(ymm5, ymm3);

    ymm6 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
    ymm2 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
    ymm1 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+srcCurStep))), 0x50);
    ymm3 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+srcRefStep))), 0x50);
    pS2 += 2 * srcRefStep;
    pS1 += 2 * srcCurStep;
    ymm6 = _mm256_maddubs_epi16(ymm6, ymm7);
    ymm2 = _mm256_maddubs_epi16(ymm2, ymm7);
    ymm6 = _mm256_subs_epi16(ymm6, ymm2);
    ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
    ymm1 = _mm256_maddubs_epi16(ymm1, ymm7);
    ymm2 = ymm4;
    ymm1 = _mm256_subs_epi16(ymm1, ymm3);

    ymm7 = ymm_s2;

    ymm4 = _mm256_adds_epi16(ymm4, ymm5);
    ymm3 = ymm6;
    ymm5 = _mm256_subs_epi16(ymm5, ymm2);
    ymm6 = _mm256_adds_epi16(ymm6, ymm1);
    ymm2 = ymm4;
    ymm1 = _mm256_subs_epi16(ymm1, ymm3);
    ymm4 = _mm256_adds_epi16(ymm4, ymm6);
    ymm3 = ymm5;
    ymm6 = _mm256_subs_epi16(ymm6, ymm2);
    ymm2 = ymm_s0;
    ymm5 = _mm256_adds_epi16(ymm5, ymm1);
    ymm1 = _mm256_subs_epi16(ymm1, ymm3);
    ymm3 = ymm_s1;
    ymm_s1 = ymm1;
    ymm_s0 = ymm6;
    ymm_s2 = ymm5;

    ymm1 = ymm0;
    ymm5 = ymm2;
    ymm0 = _mm256_adds_epi16(ymm0, ymm7);
    ymm7 = _mm256_subs_epi16(ymm7, ymm1);
    ymm2 = _mm256_adds_epi16(ymm2, ymm3);
    ymm3 = _mm256_subs_epi16(ymm3, ymm5);
    ymm1 = ymm0;
    ymm5 = ymm7;
    ymm0 = _mm256_adds_epi16(ymm0, ymm2);
    ymm2 = _mm256_subs_epi16(ymm2, ymm1);
    ymm1 = ymm0;
    ymm7 = _mm256_adds_epi16(ymm7, ymm3);
    ymm3 = _mm256_subs_epi16(ymm3, ymm5);
    ymm0 = _mm256_adds_epi16(ymm0, ymm4);
    ymm4 = _mm256_subs_epi16(ymm4, ymm1);
    ymm5 = ymm_s2;
    ymm1 = ymm7;

    ymm7 = _mm256_adds_epi16(ymm7, ymm5);
    ymm5 = _mm256_subs_epi16(ymm5, ymm1);

    ymm6 = ymm0;
    ymm0 = _mm256_blend_epi16(ymm0, ymm4, 204);
    ymm4 = _mm256_slli_epi64(ymm4, 32);
    ymm6 = _mm256_srli_epi64(ymm6, 32);
    ymm4 = _mm256_or_si256(ymm4, ymm6);
    ymm1 = ymm0;
    ymm0 = _mm256_adds_epi16(ymm0, ymm4);
    ymm4 = _mm256_subs_epi16(ymm4, ymm1);

    ymm1 = ymm7;
    ymm7 = _mm256_blend_epi16(ymm7, ymm5, 204);
    ymm5 = _mm256_slli_epi64(ymm5, 32);
    ymm1 = _mm256_srli_epi64(ymm1, 32);
    ymm5 = _mm256_or_si256(ymm5, ymm1);

    ymm1 = ymm7;
    ymm7 = _mm256_adds_epi16(ymm7, ymm5);
    ymm5 = _mm256_subs_epi16(ymm5, ymm1);

    ymm1 = ymm0;
    ymm0 = _mm256_blend_epi16(ymm0, ymm4, 170);
    ymm4 = _mm256_slli_epi32(ymm4, 16);
    ymm1 = _mm256_srli_epi32(ymm1, 16);
    ymm4 = _mm256_or_si256(ymm4, ymm1);

    ymm4 = _mm256_abs_epi16(ymm4);
    ymm0 = _mm256_abs_epi16(ymm0);
    ymm0 = _mm256_max_epi16(ymm0, ymm4);

    ymm1 = ymm7;
    ymm7 = _mm256_blend_epi16(ymm7, ymm5, 170);
    ymm5 = _mm256_slli_epi32(ymm5, 16);
    ymm1 = _mm256_srli_epi32(ymm1, 16);
    ymm5 = _mm256_or_si256(ymm5, ymm1);

    ymm6 = ymm_s0;
    ymm5 = _mm256_abs_epi16(ymm5);
    ymm7 = _mm256_abs_epi16(ymm7);
    ymm4 = ymm2;
    ymm7 = _mm256_max_epi16(ymm7, ymm5);
    ymm1 = ymm_s1;
    ymm0 = _mm256_adds_epi16(ymm0, ymm7);
    ymm2 = _mm256_adds_epi16(ymm2, ymm6);
    ymm6 = _mm256_subs_epi16(ymm6, ymm4);
    ymm4 = ymm3;
    ymm3 = _mm256_adds_epi16(ymm3, ymm1);
    ymm1 = _mm256_subs_epi16(ymm1, ymm4);

    ymm4 = ymm2;
    ymm2 = _mm256_blend_epi16(ymm2, ymm6, 204);
    ymm6 = _mm256_slli_epi64(ymm6, 32);
    ymm4 = _mm256_srli_epi64(ymm4, 32);
    ymm6 = _mm256_or_si256(ymm6, ymm4);
    ymm4 = ymm2;
    ymm2 = _mm256_adds_epi16(ymm2, ymm6);
    ymm6 = _mm256_subs_epi16(ymm6, ymm4);

    ymm4 = ymm3;
    ymm3 = _mm256_blend_epi16(ymm3, ymm1, 204);
    ymm1 = _mm256_slli_epi64(ymm1, 32);
    ymm4 = _mm256_srli_epi64(ymm4, 32);
    ymm1 = _mm256_or_si256(ymm1, ymm4);
    ymm4 = ymm3;
    ymm3 = _mm256_adds_epi16(ymm3, ymm1);
    ymm1 = _mm256_subs_epi16(ymm1, ymm4);

    ymm4 = ymm2;
    ymm2 = _mm256_blend_epi16(ymm2, ymm6, 170);
    ymm6 = _mm256_slli_epi32(ymm6, 16);
    ymm4 = _mm256_srli_epi32(ymm4, 16);
    ymm6 = _mm256_or_si256(ymm6, ymm4);

    ymm4 = ymm3;
    ymm3 = _mm256_blend_epi16(ymm3, ymm1, 170);
    ymm1 = _mm256_slli_epi32(ymm1, 16);
    ymm4 = _mm256_srli_epi32(ymm4, 16);
    ymm1 = _mm256_or_si256(ymm1, ymm4);

    ymm6 = _mm256_abs_epi16(ymm6);
    ymm2 = _mm256_abs_epi16(ymm2);
    ymm2 = _mm256_max_epi16(ymm2, ymm6);
    ymm0 = _mm256_add_epi32(ymm0, ymm2);
    ymm3 = _mm256_abs_epi16(ymm3);
    ymm1 = _mm256_abs_epi16(ymm1);
    ymm3 = _mm256_max_epi16(ymm3, ymm1);
    ymm0 = _mm256_add_epi32(ymm0, ymm3);

    ymm0 = _mm256_madd_epi16(ymm0, ymm_one);
    ymm1 = _mm256_shuffle_epi32(ymm0, 14);
    ymm0 = _mm256_add_epi32(ymm0, ymm1);
    ymm1 = _mm256_shufflelo_epi16(ymm0, 14);
    ymm0 = _mm256_add_epi32(ymm0, ymm1);
    ymm1 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);

    s = _mm_cvtsi128_si32(mm128(ymm0));
    satdPair[0] = (s << 1);

    s = _mm_cvtsi128_si32(mm128(ymm1));
    satdPair[1] = (s << 1);
}

void H265_FASTCALL MAKE_NAME(h265_SATD_8x8_Pair_16u)(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    satdPair[0] = MAKE_NAME(h265_SATD_8x8_16u)(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = MAKE_NAME(h265_SATD_8x8_16u)(pSrcCur + 8, srcCurStep, pSrcRef + 8, srcRefStep);
}

} // end namespace MFX_HEVC_PP

#endif  // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...

#endif  // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
