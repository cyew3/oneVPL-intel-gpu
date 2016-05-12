/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "assert.h"
#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_AVX2) && defined(MFX_TARGET_OPTIMIZATION_AVX2)

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (double *)(p)))

namespace MFX_HEVC_PP
{

static const Ipp32s intraPredAngleTable[] = {
    0,   0,  32,  26,  21,  17, 13,  9,  5,  2,  0, -2, -5, -9, -13, -17, -21,
    -26, -32, -26, -21, -17, -13, -9, -5, -2,  0,  2,  5,  9, 13,  17,  21,  26, 32
};

static const Ipp32s invAngleTable[] = {
    0,    0,  256,  315,  390,  482,  630,  910,
    1638, 4096,    0, 4096, 1638,  910,  630,  482,
    390,  315,  256,  315,  390,  482,  630,  910,
    1638, 4096,    0, 4096, 1638,  910,  630,  482,
    390,  315,  256
};

static inline void PredAngle_4x4_8u_AVX2(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
{
    Ipp32s iIdx;
    Ipp16s iFact;
    Ipp32s pos = 0;
    __m128i r0, r1;

    for (int j = 0; j < 4; j++)
    {
        pos += angle;
        iIdx = pos >> 5;
        iFact = pos & 31;

        // process i = 0..3

        // r0 = (Ipp16s)pSrc[iIdx+1+i];
        // r1 = (Ipp16s)pSrc[iIdx+2+i];
        r0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[iIdx+1]));
        r1 = _mm_srli_si128(r0, 2);

        // r0 += ((r1 - r0) * iFact + 16) >> 5;
        r1 = _mm_sub_epi16(r1, r0);
        r1 = _mm_mullo_epi16(r1, _mm_set1_epi16(iFact));
        r1 = _mm_add_epi16(r1, _mm_set1_epi16(16));
        r1 = _mm_srai_epi16(r1, 5);
        r0 = _mm_add_epi16(r0, r1);

        // pDst[j*pitch+i] = (Ipp8u)r0;
        r0 = _mm_packus_epi16(r0, r0);
        *(int *)&pDst[j*dstPitch] = _mm_cvtsi128_si32(r0);
    }
}

static inline void PredAngle_8x8_8u_AVX2(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
{
    Ipp32s iIdx;
    Ipp16s iFact;
    Ipp32s pos = 0;
    __m128i r0, r1;

    for (int j = 0; j < 8; j++)
    {
        pos += angle;
        iIdx = pos >> 5;
        iFact = pos & 31;

        // process i = 0..7

        r0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[iIdx+1]));
        r1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[iIdx+2]));

        r1 = _mm_sub_epi16(r1, r0);
        r1 = _mm_mullo_epi16(r1, _mm_set1_epi16(iFact));
        r1 = _mm_add_epi16(r1, _mm_set1_epi16(16));
        r1 = _mm_srai_epi16(r1, 5);
        r0 = _mm_add_epi16(r0, r1);

        r0 = _mm_packus_epi16(r0, r0);
        _mm_storel_epi64((__m128i *)&pDst[j*dstPitch], r0);
    }
}

static inline void PredAngle_16x16_8u_AVX2(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
{
    Ipp32s iIdx;
    Ipp16s iFact;
    Ipp32s pos = 0;
    __m256i y0, y1;

    _mm256_zeroupper();

    for (int j = 0; j < 16; j++)
    {
        pos += angle;
        iIdx = pos >> 5;
        iFact = pos & 31;

        // process i = 0..15
        y0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[iIdx+1]));
        y1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[iIdx+2]));

        y1 = _mm256_sub_epi16(y1, y0);
        y1 = _mm256_mullo_epi16(y1, _mm256_set1_epi16(iFact));
        y1 = _mm256_add_epi16(y1, _mm256_set1_epi16(16));
        y1 = _mm256_srai_epi16(y1, 5);
        y0 = _mm256_add_epi16(y0, y1);
        y0 = _mm256_packus_epi16(y0, y0);
        y0 = _mm256_permute4x64_epi64(y0, 0xd8);
        _mm_storeu_si128((__m128i *)&pDst[j*dstPitch], mm128(y0));
    }
}

static inline void PredAngle_32x32_8u_AVX2(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
{
    Ipp32s iIdx;
    Ipp16s iFact;
    Ipp32s pos = 0;
    __m256i y0, y1;

    _mm256_zeroupper();

    for (int j = 0; j < 32; j++)
    {
        pos += angle;
        iIdx = pos >> 5;
        iFact = pos & 31;

        for (int i = 0; i < 32; i += 16)
        {
            y0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[iIdx+1+i]));
            y1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[iIdx+2+i]));

            y1 = _mm256_sub_epi16(y1, y0);
            y1 = _mm256_mullo_epi16(y1, _mm256_set1_epi16(iFact));
            y1 = _mm256_add_epi16(y1, _mm256_set1_epi16(16));
            y1 = _mm256_srai_epi16(y1, 5);
            y0 = _mm256_add_epi16(y0, y1);
            y0 = _mm256_packus_epi16(y0, y0);
            y0 = _mm256_permute4x64_epi64(y0, 0xd8);
            _mm_storeu_si128((__m128i *)&pDst[j*dstPitch + i], mm128(y0));
        }
    }
}


// Out-of-place transpose
static inline void Transpose_4x4_8u_AVX2(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
{
    __m128i r0, r1, r2, r3;
    r0 = _mm_cvtsi32_si128(*(int *)&pSrc[0*srcPitch]);  // 03,02,01,00
    r1 = _mm_cvtsi32_si128(*(int *)&pSrc[1*srcPitch]);  // 13,12,11,10
    r2 = _mm_cvtsi32_si128(*(int *)&pSrc[2*srcPitch]);  // 23,22,21,20
    r3 = _mm_cvtsi32_si128(*(int *)&pSrc[3*srcPitch]);  // 33,32,31,30

    r0 = _mm_unpacklo_epi8(r0, r1);
    r2 = _mm_unpacklo_epi8(r2, r3);
    r0 = _mm_unpacklo_epi16(r0, r2);    // 33,23,13,03, 32,22,12,02, 31,21,11,01, 30,20,10,00,

    *(int *)&pDst[0*dstPitch] = _mm_cvtsi128_si32(r0);
    *(int *)&pDst[1*dstPitch] = _mm_extract_epi32(r0, 1);
    *(int *)&pDst[2*dstPitch] = _mm_extract_epi32(r0, 2);
    *(int *)&pDst[3*dstPitch] = _mm_extract_epi32(r0, 3);
}

static inline void Transpose_8x8_8u_AVX2(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
{
    __m128i r0, r1, r2, r3, r4, r5, r6, r7;
    r0 = _mm_loadl_epi64((__m128i*)&pSrc[0*srcPitch]);  // 07,06,05,04,03,02,01,00
    r1 = _mm_loadl_epi64((__m128i*)&pSrc[1*srcPitch]);  // 17,16,15,14,13,12,11,10
    r2 = _mm_loadl_epi64((__m128i*)&pSrc[2*srcPitch]);  // 27,26,25,24,23,22,21,20
    r3 = _mm_loadl_epi64((__m128i*)&pSrc[3*srcPitch]);  // 37,36,35,34,33,32,31,30
    r4 = _mm_loadl_epi64((__m128i*)&pSrc[4*srcPitch]);  // 47,46,45,44,43,42,41,40
    r5 = _mm_loadl_epi64((__m128i*)&pSrc[5*srcPitch]);  // 57,56,55,54,53,52,51,50
    r6 = _mm_loadl_epi64((__m128i*)&pSrc[6*srcPitch]);  // 67,66,65,64,63,62,61,60
    r7 = _mm_loadl_epi64((__m128i*)&pSrc[7*srcPitch]);  // 77,76,75,74,73,72,71,70

    r0 = _mm_unpacklo_epi8(r0, r1);
    r2 = _mm_unpacklo_epi8(r2, r3);
    r4 = _mm_unpacklo_epi8(r4, r5);
    r6 = _mm_unpacklo_epi8(r6, r7);

    r1 = r0;
    r0 = _mm_unpacklo_epi16(r0, r2);
    r1 = _mm_unpackhi_epi16(r1, r2);
    r5 = r4;
    r4 = _mm_unpacklo_epi16(r4, r6);
    r5 = _mm_unpackhi_epi16(r5, r6);
    r2 = r0;
    r0 = _mm_unpacklo_epi32(r0, r4);
    r2 = _mm_unpackhi_epi32(r2, r4);
    r3 = r1;
    r1 = _mm_unpacklo_epi32(r1, r5);
    r3 = _mm_unpackhi_epi32(r3, r5);

    _mm_storel_pi((__m64 *)&pDst[0*dstPitch], _mm_castsi128_ps(r0));    // MOVLPS
    _mm_storeh_pi((__m64 *)&pDst[1*dstPitch], _mm_castsi128_ps(r0));    // MOVHPS
    _mm_storel_pi((__m64 *)&pDst[2*dstPitch], _mm_castsi128_ps(r2));
    _mm_storeh_pi((__m64 *)&pDst[3*dstPitch], _mm_castsi128_ps(r2));
    _mm_storel_pi((__m64 *)&pDst[4*dstPitch], _mm_castsi128_ps(r1));
    _mm_storeh_pi((__m64 *)&pDst[5*dstPitch], _mm_castsi128_ps(r1));
    _mm_storel_pi((__m64 *)&pDst[6*dstPitch], _mm_castsi128_ps(r3));
    _mm_storeh_pi((__m64 *)&pDst[7*dstPitch], _mm_castsi128_ps(r3));
}

static inline void Transpose_16x16_8u_AVX2(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
{
    for (int j = 0; j < 16; j += 8)
    {
        for (int i = 0; i < 16; i += 8)
        {
            Transpose_8x8_8u_AVX2(&pSrc[i*srcPitch+j], srcPitch, &pDst[j*dstPitch+i], dstPitch);
        }
    }
}

static inline void Transpose_32x32_8u_AVX2(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
{
    for (int j = 0; j < 32; j += 8)
    {
        for (int i = 0; i < 32; i += 8)
        {
            Transpose_8x8_8u_AVX2(&pSrc[i*srcPitch+j], srcPitch, &pDst[j*dstPitch+i], dstPitch);
        }
    }
}

static inline void PredAngle_4x4_16u_AVX2(Ipp16u *pSrc, Ipp16u *pDst, Ipp32s dstPitch, Ipp32s angle)
{
    Ipp32s iIdx;
    Ipp16s iFact;
    Ipp32s pos = 0;
    __m128i r0, r1;

    for (int j = 0; j < 4; j++)
    {
        pos += angle;
        iIdx = pos >> 5;
        iFact = pos & 31;

        // process i = 0..3

        r0 = _mm_loadl_epi64((__m128i *)&pSrc[iIdx+1]);
        r1 = _mm_loadl_epi64((__m128i *)&pSrc[iIdx+2]);

        r1 = _mm_sub_epi16(r1, r0);
        r1 = _mm_mullo_epi16(r1, _mm_set1_epi16(iFact));
        r1 = _mm_add_epi16(r1, _mm_set1_epi16(16));
        r1 = _mm_srai_epi16(r1, 5);
        r0 = _mm_add_epi16(r0, r1);

        _mm_storel_epi64((__m128i *)&pDst[j*dstPitch], r0);
    }
}

static inline void PredAngle_8x8_16u_AVX2(Ipp16u *pSrc, Ipp16u *pDst, Ipp32s dstPitch, Ipp32s angle)
{
    Ipp32s iIdx;
    Ipp16s iFact;
    Ipp32s pos = 0;
    __m128i r0, r1;

    for (int j = 0; j < 8; j++)
    {
        pos += angle;
        iIdx = pos >> 5;
        iFact = pos & 31;

        // process i = 0..7

        r0 = _mm_loadu_si128((__m128i *)&pSrc[iIdx+1]);
        r1 = _mm_loadu_si128((__m128i *)&pSrc[iIdx+2]);

        r1 = _mm_sub_epi16(r1, r0);
        r1 = _mm_mullo_epi16(r1, _mm_set1_epi16(iFact));
        r1 = _mm_add_epi16(r1, _mm_set1_epi16(16));
        r1 = _mm_srai_epi16(r1, 5);
        r0 = _mm_add_epi16(r0, r1);

        _mm_storeu_si128((__m128i *)&pDst[j*dstPitch], r0);
    }
}

static inline void PredAngle_16x16_16u_AVX2(Ipp16u *pSrc, Ipp16u *pDst, Ipp32s dstPitch, Ipp32s angle)
{
    Ipp32s iIdx;
    Ipp16s iFact;
    Ipp32s pos = 0;
    __m256i y0, y1;

    _mm256_zeroupper();

    for (int j = 0; j < 16; j++)
    {
        pos += angle;
        iIdx = pos >> 5;
        iFact = pos & 31;

        // process i = 0..15
        y0 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+1]);
        y1 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+2]);

        y1 = _mm256_sub_epi16(y1, y0);
        y1 = _mm256_mullo_epi16(y1, _mm256_set1_epi16(iFact));
        y1 = _mm256_add_epi16(y1, _mm256_set1_epi16(16));
        y1 = _mm256_srai_epi16(y1, 5);
        y0 = _mm256_add_epi16(y0, y1);

        _mm256_storeu_si256((__m256i *)&pDst[j*dstPitch], y0);
    }
}

static inline void PredAngle_32x32_16u_AVX2(Ipp16u *pSrc, Ipp16u *pDst, Ipp32s dstPitch, Ipp32s angle)
{
    Ipp32s iIdx;
    Ipp16s iFact;
    Ipp32s pos = 0;
    __m256i y0, y1;

    _mm256_zeroupper();

    for (int j = 0; j < 32; j++)
    {
        pos += angle;
        iIdx = pos >> 5;
        iFact = pos & 31;

        for (int i = 0; i < 32; i += 16)
        {
            y0 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+1+i]);
            y1 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+2+i]);

            y1 = _mm256_sub_epi16(y1, y0);
            y1 = _mm256_mullo_epi16(y1, _mm256_set1_epi16(iFact));
            y1 = _mm256_add_epi16(y1, _mm256_set1_epi16(16));
            y1 = _mm256_srai_epi16(y1, 5);
            y0 = _mm256_add_epi16(y0, y1);

            _mm256_storeu_si256((__m256i *)&pDst[j*dstPitch + i], y0);
        }
    }
}

static inline void Transpose_4x4_16u_AVX2(Ipp16u *pSrc, Ipp32s srcPitch, Ipp16u *pDst, Ipp32s dstPitch)
{
    __m128i r0, r1, r2, r3;

    r0 = _mm_loadl_epi64((__m128i*)&pSrc[0*srcPitch]);  // 03,02,01,00
    r1 = _mm_loadl_epi64((__m128i*)&pSrc[1*srcPitch]);  // 13,12,11,10
    r2 = _mm_loadl_epi64((__m128i*)&pSrc[2*srcPitch]);  // 23,22,21,20
    r3 = _mm_loadl_epi64((__m128i*)&pSrc[3*srcPitch]);  // 33,32,31,30

    r0 = _mm_unpacklo_epi16(r0, r1);
    r2 = _mm_unpacklo_epi16(r2, r3);
    r1 = r0;
    r0 = _mm_unpacklo_epi32(r0, r2);
    r1 = _mm_unpackhi_epi32(r1, r2);

    _mm_storel_pi((__m64 *)&pDst[0*dstPitch], _mm_castsi128_ps(r0));    // MOVLPS
    _mm_storeh_pi((__m64 *)&pDst[1*dstPitch], _mm_castsi128_ps(r0));    // MOVHPS
    _mm_storel_pi((__m64 *)&pDst[2*dstPitch], _mm_castsi128_ps(r1));
    _mm_storeh_pi((__m64 *)&pDst[3*dstPitch], _mm_castsi128_ps(r1));
}

static inline void Transpose_4x8_16u_AVX2(Ipp16u *pSrc, Ipp32s srcPitch, Ipp16u *pDst, Ipp32s dstPitch)
{
    __m128i r0, r1, r2, r3, r4, r5, r6, r7;
    r0 = _mm_loadl_epi64((__m128i*)&pSrc[0*srcPitch]);  // 03,02,01,00
    r1 = _mm_loadl_epi64((__m128i*)&pSrc[1*srcPitch]);  // 13,12,11,10
    r2 = _mm_loadl_epi64((__m128i*)&pSrc[2*srcPitch]);  // 23,22,21,20
    r3 = _mm_loadl_epi64((__m128i*)&pSrc[3*srcPitch]);  // 33,32,31,30
    r4 = _mm_loadl_epi64((__m128i*)&pSrc[4*srcPitch]);  // 43,42,41,40
    r5 = _mm_loadl_epi64((__m128i*)&pSrc[5*srcPitch]);  // 53,52,51,50
    r6 = _mm_loadl_epi64((__m128i*)&pSrc[6*srcPitch]);  // 63,62,61,60
    r7 = _mm_loadl_epi64((__m128i*)&pSrc[7*srcPitch]);  // 73,72,71,70

    r0 = _mm_unpacklo_epi16(r0, r1);
    r2 = _mm_unpacklo_epi16(r2, r3);
    r4 = _mm_unpacklo_epi16(r4, r5);
    r6 = _mm_unpacklo_epi16(r6, r7);

    r1 = r0;
    r0 = _mm_unpacklo_epi32(r0, r2);
    r1 = _mm_unpackhi_epi32(r1, r2);
    r5 = r4;
    r4 = _mm_unpacklo_epi32(r4, r6);
    r5 = _mm_unpackhi_epi32(r5, r6);
    r2 = r0;
    r0 = _mm_unpacklo_epi64(r0, r4);
    r2 = _mm_unpackhi_epi64(r2, r4);
    r3 = r1;
    r1 = _mm_unpacklo_epi64(r1, r5);
    r3 = _mm_unpackhi_epi64(r3, r5);

    _mm_storeu_si128((__m128i *)&pDst[0*dstPitch], r0);
    _mm_storeu_si128((__m128i *)&pDst[1*dstPitch], r2);
    _mm_storeu_si128((__m128i *)&pDst[2*dstPitch], r1);
    _mm_storeu_si128((__m128i *)&pDst[3*dstPitch], r3);
}

static inline void Transpose_8x8_16u_AVX2(Ipp16u *pSrc, Ipp32s srcPitch, Ipp16u *pDst, Ipp32s dstPitch)
{
    for (int j = 0; j < 8; j += 4)
    {
        Transpose_4x8_16u_AVX2(&pSrc[j], srcPitch, &pDst[j*dstPitch], dstPitch);
    }
}

static inline void Transpose_16x16_16u_AVX2(Ipp16u *pSrc, Ipp32s srcPitch, Ipp16u *pDst, Ipp32s dstPitch)
{
    for (int j = 0; j < 16; j += 4)
    {
        for (int i = 0; i < 16; i += 8)
        {
            Transpose_4x8_16u_AVX2(&pSrc[i*srcPitch+j], srcPitch, &pDst[j*dstPitch+i], dstPitch);
        }
    }
}

static inline void Transpose_32x32_16u_AVX2(Ipp16u *pSrc, Ipp32s srcPitch, Ipp16u *pDst, Ipp32s dstPitch)
{
    for (int j = 0; j < 32; j += 4)
    {
        for (int i = 0; i < 32; i += 8)
        {
            Transpose_4x8_16u_AVX2(&pSrc[i*srcPitch+j], srcPitch, &pDst[j*dstPitch+i], dstPitch);
        }
    }
}

template <typename PixType>
static inline void h265_PredictIntra_Ang_Kernel (
    Ipp32s mode,
    PixType* PredPel,
    PixType* pels,
    Ipp32s pitch,
    Ipp32s width)
{
    Ipp32s intraPredAngle = intraPredAngleTable[mode];
    ALIGN_DECL(64) PixType refMainBuf[4*64+1];
    PixType *refMain = refMainBuf + 128;
    PixType *PredPel1, *PredPel2;
    Ipp32s invAngle = invAngleTable[mode];
    Ipp32s invAngleSum;
    Ipp32s i;

    if (mode >= 18)
    {
        PredPel1 = PredPel;
        PredPel2 = PredPel + 2 * width + 1;
    }
    else
    {
        PredPel1 = PredPel + 2 * width;
        PredPel2 = PredPel + 1;
    }

    refMain[0] = PredPel[0];

    if (intraPredAngle < 0)
    {
        for (i = 1; i <= width; i++)
        {
            refMain[i] = PredPel1[i];
        }

        invAngleSum = 128;
        for (i = -1; i > ((width * intraPredAngle) >> 5); i--)
        {
            invAngleSum += invAngle;
            refMain[i] = PredPel2[(invAngleSum >> 8) - 1];
        }
    }
    else
    {
        for (i = 1; i <= 2*width; i++)
        {
            refMain[i] = PredPel1[i];
        }
    }

    if (sizeof(PixType) == 1) {
        switch (width)
        {
        case 4:
            if (mode >= 18)
                PredAngle_4x4_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[4*4];
                PredAngle_4x4_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)buf, 4, intraPredAngle);
                Transpose_4x4_8u_AVX2((Ipp8u *)buf, 4, (Ipp8u *)pels, pitch);
            }
            break;
        case 8:
            if (mode >= 18)
                PredAngle_8x8_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[8*8];
                PredAngle_8x8_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)buf, 8, intraPredAngle);
                Transpose_8x8_8u_AVX2((Ipp8u *)buf, 8, (Ipp8u *)pels, pitch);
            }
            break;
        case 16:
            if (mode >= 18)
                PredAngle_16x16_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[16*16];
                PredAngle_16x16_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)buf, 16, intraPredAngle);
                Transpose_16x16_8u_AVX2((Ipp8u *)buf, 16, (Ipp8u *)pels, pitch);
            }
            break;
        case 32:
            if (mode >= 18)
                PredAngle_32x32_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[32*32];
                PredAngle_32x32_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)buf, 32, intraPredAngle);
                Transpose_32x32_8u_AVX2((Ipp8u *)buf, 32, (Ipp8u *)pels, pitch);
            }
            break;
        }
    } else {
        switch (width)
        {
        case 4:
            if (mode >= 18)
                PredAngle_4x4_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)pels, pitch, intraPredAngle);
            else
            {
                Ipp16u buf[4*4];
                PredAngle_4x4_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)buf, 4, intraPredAngle);
                Transpose_4x4_16u_AVX2((Ipp16u *)buf, 4, (Ipp16u *)pels, pitch);
            }
            break;
        case 8:
            if (mode >= 18)
                PredAngle_8x8_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)pels, pitch, intraPredAngle);
            else
            {
                Ipp16u buf[8*8];
                PredAngle_8x8_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)buf, 8, intraPredAngle);
                Transpose_8x8_16u_AVX2((Ipp16u *)buf, 8, (Ipp16u *)pels, pitch);
            }
            break;
        case 16:
            if (mode >= 18)
                PredAngle_16x16_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)pels, pitch, intraPredAngle);
            else
            {
                Ipp16u buf[16*16];
                PredAngle_16x16_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)buf, 16, intraPredAngle);
                Transpose_16x16_16u_AVX2((Ipp16u *)buf, 16, (Ipp16u *)pels, pitch);
            }
            break;
        case 32:
            if (mode >= 18)
                PredAngle_32x32_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)pels, pitch, intraPredAngle);
            else
            {
                Ipp16u buf[32*32];
                PredAngle_32x32_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)buf, 32, intraPredAngle);
                Transpose_32x32_16u_AVX2((Ipp16u *)buf, 32, (Ipp16u *)pels, pitch);
            }
            break;
        }
    }
}

void MAKE_NAME(h265_PredictIntra_Ang_8u) (Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width)
{
    h265_PredictIntra_Ang_Kernel<Ipp8u>(mode, PredPel, pels, pitch, width);
}

void MAKE_NAME(h265_PredictIntra_Ang_16u) (Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width)
{
    h265_PredictIntra_Ang_Kernel<Ipp16u>(mode, PredPel, pels, pitch, width);
}

template <typename PixType>
static inline void h265_PredictIntra_Ang_NoTranspose_Kernel(
    Ipp32s mode,
    PixType* PredPel,
    PixType* pels,
    Ipp32s pitch,
    Ipp32s width)
{
    Ipp32s intraPredAngle = intraPredAngleTable[mode];
    PixType refMainBuf[4*64+1];
    PixType *refMain = refMainBuf + 128;
    PixType *PredPel1, *PredPel2;
    Ipp32s invAngle = invAngleTable[mode];
    Ipp32s invAngleSum;
    Ipp32s i;

    if (mode >= 18)
    {
        PredPel1 = PredPel;
        PredPel2 = PredPel + 2 * width + 1;
    }
    else
    {
        PredPel1 = PredPel + 2 * width;
        PredPel2 = PredPel + 1;
    }

    refMain[0] = PredPel[0];

    if (intraPredAngle < 0)
    {
        for (i = 1; i <= width; i++)
        {
            refMain[i] = PredPel1[i];
        }

        invAngleSum = 128;
        for (i = -1; i > ((width * intraPredAngle) >> 5); i--)
        {
            invAngleSum += invAngle;
            refMain[i] = PredPel2[(invAngleSum >> 8) - 1];
        }
    }
    else
    {
        for (i = 1; i <= 2*width; i++)
        {
            refMain[i] = PredPel1[i];
        }
    }

    if (sizeof(PixType) == 1) {
        switch (width)
        {
        case 4:
            PredAngle_4x4_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)pels, pitch, intraPredAngle);
            break;
        case 8:
            PredAngle_8x8_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)pels, pitch, intraPredAngle);
            break;
        case 16:
            PredAngle_16x16_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)pels, pitch, intraPredAngle);
            break;
        case 32:
            PredAngle_32x32_8u_AVX2((Ipp8u *)refMain, (Ipp8u *)pels, pitch, intraPredAngle);
            break;
        }
    } else {
        switch (width)
        {
        case 4:
            PredAngle_4x4_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)pels, pitch, intraPredAngle);
            break;
        case 8:
            PredAngle_8x8_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)pels, pitch, intraPredAngle);
            break;
        case 16:
            PredAngle_16x16_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)pels, pitch, intraPredAngle);
            break;
        case 32:
            PredAngle_32x32_16u_AVX2((Ipp16u *)refMain, (Ipp16u *)pels, pitch, intraPredAngle);
            break;
        }
    }
}

void MAKE_NAME(h265_PredictIntra_Ang_NoTranspose_8u) (Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width)
{
    h265_PredictIntra_Ang_NoTranspose_Kernel<Ipp8u>(mode, PredPel, pels, pitch, width);
}

void MAKE_NAME(h265_PredictIntra_Ang_NoTranspose_16u) (Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width)
{
    h265_PredictIntra_Ang_NoTranspose_Kernel<Ipp16u>(mode, PredPel, pels, pitch, width);
}

//
// Encoder version:
//  computes all 33 modes in one call, returned in pels[33][]
//  for mode < 18, output is returned in transposed form
//

ALIGN_DECL(32) static const Ipp8s tab_frac[32][32] = {
    { 32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0,32, 0 },
    { 31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1 },
    { 30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2 },
    { 29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3 },
    { 28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4 },
    { 27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5 },
    { 26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6 },
    { 25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7 },
    { 24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8 },
    { 23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9 },
    { 22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10 },
    { 21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11 },
    { 20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12 },
    { 19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13 },
    { 18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14 },
    { 17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15 },
    { 16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16 },
    { 15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17,15,17 },
    { 14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18,14,18 },
    { 13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19,13,19 },
    { 12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20,12,20 },
    { 11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21,11,21 },
    { 10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22,10,22 },
    {  9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23, 9,23 },
    {  8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24, 8,24 },
    {  7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25, 7,25 },
    {  6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26, 6,26 },
    {  5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27, 5,27 },
    {  4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28, 4,28 },
    {  3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29, 3,29 },
    {  2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30, 2,30 },
    {  1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31, 1,31 },
};

ALIGN_DECL(32) static const Ipp8s tab_shuf4x4[32] =
    {  0, 1, 1, 2, 2, 3, 3, 4, 8, 9, 9,10,10,11,11,12, 0, 1, 1, 2, 2, 3, 3, 4, 8, 9, 9,10,10,11,11,12 };

ALIGN_DECL(32) static const Ipp8s tab_shuf8x8[32] =
    {  0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8 };

ALIGN_DECL(32) static const Ipp8s tab_left4x4[32] =
    {  1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7 };

ALIGN_DECL(32) static const Ipp8s tab_left8x8[32] =
    {  1, 2, 3, 4, 5, 6, 7, 8, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7, 8, 2, 3, 4, 5, 6, 7, 8, 9 };

ALIGN_DECL(32) static const Ipp8s tab_right4x4[32] =
    {  0, 1, 2, 3, 9, 0, 1, 2,10, 9, 0, 1,11,10, 9, 0 };    // proj_4x4 and shift right

ALIGN_DECL(32) static const Ipp8s tab_right8x8[32] =
    { 15,14,13,12,11,10, 9, 0, 1, 2, 3, 4, 5, 6, 7,-1,15,14,13,12,11,10, 9, 0, 1, 2, 3, 4, 5, 6, 7,-1 };

ALIGN_DECL(32) static const Ipp8s tab_right16x16[32] =
    { -1,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1,-1,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };

ALIGN_DECL(32) static const Ipp8s tab_right32x32[32] =
    { 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

ALIGN_DECL(32) static const Ipp8s proj_4x4[][32] = {
    { -1,-1,-1, 0, 9,10,11,12,-1,-1,-1, 0, 1, 2, 3, 4,-1,-1,-1, 0, 9,10,11,12,-1,-1,-1, 0, 1, 2, 3, 4 },    // mode 11
    { -1,-1,-1, 0, 9,10,11,12,-1,-1,-1, 0, 1, 2, 3, 4,-1,-1,-1, 0, 9,10,11,12,-1,-1,-1, 0, 1, 2, 3, 4 },    // mode 12
    { -1,-1, 4, 0, 9,10,11,12,-1,-1,12, 0, 1, 2, 3, 4,-1,-1, 4, 0, 9,10,11,12,-1,-1,12, 0, 1, 2, 3, 4 },    // mode 13
    { -1,-1, 2, 0, 9,10,11,12,-1,-1,10, 0, 1, 2, 3, 4,-1,-1, 2, 0, 9,10,11,12,-1,-1,10, 0, 1, 2, 3, 4 },    // mode 14
    { -1, 4, 2, 0, 9,10,11,12,-1,12,10, 0, 1, 2, 3, 4,-1, 4, 2, 0, 9,10,11,12,-1,12,10, 0, 1, 2, 3, 4 },    // mode 15
    { -1, 3, 2, 0, 9,10,11,12,-1,11,10, 0, 1, 2, 3, 4,-1, 3, 2, 0, 9,10,11,12,-1,11,10, 0, 1, 2, 3, 4 },    // mode 16
    {  4, 2, 1, 0, 9,10,11,12,12,10, 9, 0, 1, 2, 3, 4, 4, 2, 1, 0, 9,10,11,12,12,10, 9, 0, 1, 2, 3, 4 },    // mode 17
};

ALIGN_DECL(32) static const Ipp8s proj_8x8[][32] = {
    { -1,-1,-1,-1,-1,-1,-1, 0, 1, 2, 3, 4, 5, 6, 7, 8,-1,-1,-1,-1,-1,-1,-1, 0, 1, 2, 3, 4, 5, 6, 7, 8 },
    { -1,-1,-1,-1,-1,-1, 6, 0, 1, 2, 3, 4, 5, 6, 7, 8,-1,-1,-1,-1,-1,-1, 6, 0, 1, 2, 3, 4, 5, 6, 7, 8 },
    { -1,-1,-1,-1,-1, 7, 4, 0, 1, 2, 3, 4, 5, 6, 7, 8,-1,-1,-1,-1,-1, 7, 4, 0, 1, 2, 3, 4, 5, 6, 7, 8 },
    { -1,-1,-1,-1, 7, 5, 2, 0, 1, 2, 3, 4, 5, 6, 7, 8,-1,-1,-1,-1, 7, 5, 2, 0, 1, 2, 3, 4, 5, 6, 7, 8 },
    { -1,-1,-1, 8, 6, 4, 2, 0, 1, 2, 3, 4, 5, 6, 7, 8,-1,-1,-1, 8, 6, 4, 2, 0, 1, 2, 3, 4, 5, 6, 7, 8 },
    { -1,-1, 8, 6, 5, 3, 2, 0, 1, 2, 3, 4, 5, 6, 7, 8,-1,-1, 8, 6, 5, 3, 2, 0, 1, 2, 3, 4, 5, 6, 7, 8 },
    { -1, 7, 6, 5, 4, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8,-1, 7, 6, 5, 4, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8 },
};

ALIGN_DECL(32) static const Ipp8s proj_16x16[][32] = {
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,13, 6, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,13, 6, 0 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,14,11, 7, 4, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,14,11, 7, 4, 0 },
    { -1,-1,-1,-1,-1,-1,-1,-1,-1,15,12,10, 7, 5, 2, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,15,12,10, 7, 5, 2, 0 },
    { -1,-1,-1,-1,-1,-1,-1,15,13,11, 9, 8, 6, 4, 2, 0,-1,-1,-1,-1,-1,-1,-1,15,13,11, 9, 8, 6, 4, 2, 0 },
    { -1,-1,-1,-1,-1,15,14,12,11, 9, 8, 6, 5, 3, 2, 0,-1,-1,-1,-1,-1,15,14,12,11, 9, 8, 6, 5, 3, 2, 0 },
    { -1,-1,-1,15,14,12,11,10, 9, 7, 6, 5, 4, 2, 1, 0,-1,-1,-1,15,14,12,11,10, 9, 7, 6, 5, 4, 2, 1, 0 },
};

ALIGN_DECL(32) static const Ipp8s proj_32x32[][3][32] = {
    {{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },    // shuffles from src[16] into dst[0]
     { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0 },    // shuffles from src[ 0] into dst[16]
     { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0,-1 }},   // shuffles from src[16] into dst[16]

    {{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
     { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,13, 6, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,13, 6, 0 },
     { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,10, 3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,10, 3,-1,-1,-1 }},

    {{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
     { -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,14,11, 7, 4, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,14,11, 7, 4, 0 },
     { -1,-1,-1,-1,-1,-1,-1,12, 9, 5, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12, 9, 5, 2,-1,-1,-1,-1,-1 }},

    {{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
     { -1,-1,-1,-1,-1,-1,-1,-1,-1,15,12,10, 7, 5, 2, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,15,12,10, 7, 5, 2, 0 },
     { -1,-1,-1,14,11, 9, 6, 4, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,14,11, 9, 6, 4, 1,-1,-1,-1,-1,-1,-1,-1 }},

    {{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,14,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,14 },
     { -1,-1,-1,-1,-1,-1,-1,15,13,11, 9, 8, 6, 4, 2, 0,-1,-1,-1,-1,-1,-1,-1,15,13,11, 9, 8, 6, 4, 2, 0 },
     { 12,10, 8, 7, 5, 3, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,10, 8, 7, 5, 3, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }},

    {{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,14,13,11,10, 8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,14,13,11,10, 8 },
     { -1,-1,-1,-1,-1,15,14,12,11, 9, 8, 6, 5, 3, 2, 0,-1,-1,-1,-1,-1,15,14,12,11, 9, 8, 6, 5, 3, 2, 0 },
     {  7, 5, 4, 2, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 7, 5, 4, 2, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }},

    {{ -1,-1,-1,-1,-1,-1,15,14,12,11,10, 9, 7, 6, 5, 4,-1,-1,-1,-1,-1,-1,15,14,12,11,10, 9, 7, 6, 5, 4 },
     { -1,-1,-1,15,14,12,11,10, 9, 7, 6, 5, 4, 2, 1, 0,-1,-1,-1,15,14,12,11,10, 9, 7, 6, 5, 4, 2, 1, 0 },
     {  2, 1, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 2, 1, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }},
};

#define STEP(angle) {   \
    pos += angle;       \
    iIdx = pos >> 5;    \
    iFact = pos & 31;   \
}

#define MUX4(dst, src, idx) \
switch ((idx+4) % 4) { \
    case 0: dst = src##0; break; \
    case 1: dst = src##1; break; \
    case 2: dst = src##2; break; \
    case 3: dst = src##3; break; \
}

template <int mode>
static inline void PredAngle_4x4(Ipp8u *pSrc1, Ipp8u* /*pSrc2*/, Ipp8u *pDst1, Ipp8u *pDst2)
{
    Ipp32s iIdx;
    Ipp32s iFact;
    Ipp32s pos = 0;
    Ipp32s angle = intraPredAngleTable[mode];

    __m128i r0, r1, r2, r3;
    __m128i s0, s1, s2, s3;
    __m128i t0, t1, t2, t3;
    __m128i f0, f1, f2, f3;

    r0 = _mm_load_si128((__m128i *)&pSrc1[0]);  // load as [src2 src1]

    t0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)tab_shuf4x4));
    t1 = _mm_shuffle_epi8(_mm_srli_si128(r0, 1), _mm_load_si128((__m128i *)tab_shuf4x4));
    t2 = _mm_shuffle_epi8(_mm_srli_si128(r0, 2), _mm_load_si128((__m128i *)tab_shuf4x4));
    t3 = _mm_shuffle_epi8(_mm_srli_si128(r0, 3), _mm_load_si128((__m128i *)tab_shuf4x4));

    STEP(angle);
    MUX4(r0, t, iIdx);
    f0 = _mm_load_si128((__m128i *)tab_frac[iFact]);

    STEP(angle);
    MUX4(r1, t, iIdx);
    f1 = _mm_load_si128((__m128i *)tab_frac[iFact]);

    STEP(angle);
    MUX4(r2, t, iIdx);
    f2 = _mm_load_si128((__m128i *)tab_frac[iFact]);

    STEP(angle);
    MUX4(r3, t, iIdx);
    f3 = _mm_load_si128((__m128i *)tab_frac[iFact]);

    r0 = _mm_maddubs_epi16(r0, f0);
    r1 = _mm_maddubs_epi16(r1, f1);
    r2 = _mm_maddubs_epi16(r2, f2);
    r3 = _mm_maddubs_epi16(r3, f3);

    s0 = _mm_unpacklo_epi64(r0, r1);
    s1 = _mm_unpacklo_epi64(r2, r3);
    s2 = _mm_unpackhi_epi64(r0, r1);
    s3 = _mm_unpackhi_epi64(r2, r3);

    s0 = _mm_add_epi16(s0, _mm_set1_epi16(16));
    s1 = _mm_add_epi16(s1, _mm_set1_epi16(16));
    s2 = _mm_add_epi16(s2, _mm_set1_epi16(16));
    s3 = _mm_add_epi16(s3, _mm_set1_epi16(16));

    s0 = _mm_srai_epi16(s0, 5);
    s1 = _mm_srai_epi16(s1, 5);
    s2 = _mm_srai_epi16(s2, 5);
    s3 = _mm_srai_epi16(s3, 5);

    s0 = _mm_packus_epi16(s0, s1);
    s2 = _mm_packus_epi16(s2, s3);

    _mm_store_si128((__m128i *)pDst1, s0);
    _mm_store_si128((__m128i *)pDst2, s2);
}

#define MUX8(dst, src, idx) \
switch ((idx+8) % 8) { \
    case 0: dst = src##0; break; \
    case 1: dst = src##1; break; \
    case 2: dst = src##2; break; \
    case 3: dst = src##3; break; \
    case 4: dst = src##4; break; \
    case 5: dst = src##5; break; \
    case 6: dst = src##6; break; \
    case 7: dst = src##7; break; \
}

#define PRED8(j) { \
    STEP(angle); \
    MUX8(r0, t, iIdx); \
    f0 = _mm256_load_si256((__m256i *)tab_frac[iFact]); \
        \
    STEP(angle); \
    MUX8(r1, t, iIdx); \
    f1 = _mm256_load_si256((__m256i *)tab_frac[iFact]); \
        \
    r0 = _mm256_maddubs_epi16(r0, f0); \
    r1 = _mm256_maddubs_epi16(r1, f1); \
    r0 = _mm256_add_epi16(r0, _mm256_set1_epi16(16)); \
    r1 = _mm256_add_epi16(r1, _mm256_set1_epi16(16)); \
    r0 = _mm256_srai_epi16(r0, 5); \
    r1 = _mm256_srai_epi16(r1, 5); \
    r0 = _mm256_packus_epi16(r0, r1); \
        \
    _mm_store_si128((__m128i *)&pDst1[j*16], mm128(r0)); \
    _mm_store_si128((__m128i *)&pDst2[j*16], mm128(_mm256_permute2x128_si256(r0, r0, 0x01))); \
}

template <int mode>
static inline void PredAngle_8x8(Ipp8u *pSrc1, Ipp8u *pSrc2, Ipp8u *pDst1, Ipp8u *pDst2)
{
    Ipp32s iIdx;
    Ipp32s iFact;
    Ipp32s pos = 0;
    Ipp32s angle = intraPredAngleTable[mode];

    __m256i r0, r1;
    __m256i t0, t1, t2, t3, t4, t5, t6, t7;
    __m256i f0, f1;

    r0 = _mm256_permute2x128_si256(mm256(_mm_load_si128((__m128i *)&pSrc1[0])), mm256(_mm_load_si128((__m128i *)&pSrc2[0])), 0x20);
    t0 = _mm256_shuffle_epi8(r0, *(__m256i *)tab_shuf8x8);
    t1 = _mm256_shuffle_epi8(_mm256_srli_si256(r0, 1), *(__m256i *)tab_shuf8x8);
    t2 = _mm256_shuffle_epi8(_mm256_srli_si256(r0, 2), *(__m256i *)tab_shuf8x8);
    t3 = _mm256_shuffle_epi8(_mm256_srli_si256(r0, 3), *(__m256i *)tab_shuf8x8);
    t4 = _mm256_shuffle_epi8(_mm256_srli_si256(r0, 4), *(__m256i *)tab_shuf8x8);
    t5 = _mm256_shuffle_epi8(_mm256_srli_si256(r0, 5), *(__m256i *)tab_shuf8x8);
    t6 = _mm256_shuffle_epi8(_mm256_srli_si256(r0, 6), *(__m256i *)tab_shuf8x8);
    t7 = _mm256_shuffle_epi8(_mm256_srli_si256(r0, 7), *(__m256i *)tab_shuf8x8);

    PRED8(0);
    PRED8(1);
    PRED8(2);
    PRED8(3);
}

#define PRED16(j) { \
    pos += angle; \
    iIdx = pos >> 5; \
    iFact = pos & 31; \
    iIdx = ((iIdx+16) % 16); \
    f0 = _mm256_load_si256((__m256i *)tab_frac[iFact]); \
        \
    t0 = _mm256_permute2x128_si256(mm256(_mm_loadu_si128((__m128i *)&pSrc1[iIdx+0])), mm256(_mm_loadu_si128((__m128i *)&pSrc2[iIdx+0])), 0x20); \
    r1 = _mm256_permute2x128_si256(mm256(_mm_loadu_si128((__m128i *)&pSrc1[iIdx+1])), mm256(_mm_loadu_si128((__m128i *)&pSrc2[iIdx+1])), 0x20); \
    r0 = _mm256_unpacklo_epi8(t0, r1); \
    r1 = _mm256_unpackhi_epi8(t0, r1); \
        \
    r0 = _mm256_maddubs_epi16(r0, f0); \
    r1 = _mm256_maddubs_epi16(r1, f0); \
    r0 = _mm256_add_epi16(r0, _mm256_set1_epi16(16)); \
    r1 = _mm256_add_epi16(r1, _mm256_set1_epi16(16)); \
    r0 = _mm256_srai_epi16(r0, 5); \
    r1 = _mm256_srai_epi16(r1, 5); \
    r0 = _mm256_packus_epi16(r0, r1); \
    _mm_store_si128((__m128i *)&pDst1[j*16], mm128(r0)); \
    _mm_store_si128((__m128i *)&pDst2[j*16], mm128(_mm256_permute2x128_si256(r0, r0, 0x01))); \
}

template <int mode>
static inline void PredAngle_16x16(Ipp8u *pSrc1, Ipp8u *pSrc2, Ipp8u *pDst1, Ipp8u *pDst2)
{
    Ipp32s iIdx;
    Ipp32s iFact;
    Ipp32s pos = 0;
    Ipp32s angle = intraPredAngleTable[mode];

    __m256i r0, r1, t0, f0;

    PRED16(0);
    PRED16(1);
    PRED16(2);
    PRED16(3);
    PRED16(4);
    PRED16(5);
    PRED16(6);
    PRED16(7);
    PRED16(8);
    PRED16(9);
    PRED16(10);
    PRED16(11);
    PRED16(12);
    PRED16(13);
    PRED16(14);
    PRED16(15);
}

#define PRED32(j) { \
    pos += angle; \
    iIdx = pos >> 5; \
    iFact = pos & 31; \
    iIdx = ((iIdx+32) % 32); \
    f0 = _mm256_load_si256((__m256i *)tab_frac[iFact]); \
        \
    t0 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+0]); \
    r1 = _mm256_loadu_si256((__m256i *)&pSrc[iIdx+1]); \
    r0 = _mm256_unpacklo_epi8(t0, r1); \
    r1 = _mm256_unpackhi_epi8(t0, r1); \
        \
    r0 = _mm256_maddubs_epi16(r0, f0); \
    r1 = _mm256_maddubs_epi16(r1, f0); \
    r0 = _mm256_add_epi16(r0, _mm256_set1_epi16(16)); \
    r1 = _mm256_add_epi16(r1, _mm256_set1_epi16(16)); \
    r0 = _mm256_srai_epi16(r0, 5); \
    r1 = _mm256_srai_epi16(r1, 5); \
    r0 = _mm256_packus_epi16(r0, r1); \
    _mm256_store_si256((__m256i *)&pDst[j*32+0], r0); \
}

template <int mode>
static inline void PredAngle_32x32(Ipp8u *pSrc1, Ipp8u *pSrc2, Ipp8u *pDst1, Ipp8u *pDst2)
{
    Ipp32s i, iIdx;
    Ipp32s iFact, pos, angle;
    Ipp8u *pSrc, *pDst;

    __m256i r0, r1;
    __m256i t0, f0;

    for (i = 0; i < 2; i++) {
        pos = 0;
        angle = intraPredAngleTable[mode];
        pSrc = (i == 0 ? pSrc1 : pSrc2);
        pDst = (i == 0 ? pDst1 : pDst2);

        PRED32(0);
        PRED32(1);
        PRED32(2);
        PRED32(3);
        PRED32(4);
        PRED32(5);
        PRED32(6);
        PRED32(7);
        PRED32(8);
        PRED32(9);
        PRED32(10);
        PRED32(11);
        PRED32(12);
        PRED32(13);
        PRED32(14);
        PRED32(15);
        PRED32(16);
        PRED32(17);
        PRED32(18);
        PRED32(19);
        PRED32(20);
        PRED32(21);
        PRED32(22);
        PRED32(23);
        PRED32(24);
        PRED32(25);
        PRED32(26);
        PRED32(27);
        PRED32(28);
        PRED32(29);
        PRED32(30);
        PRED32(31);
    }
}

template <int width, int mode>
static inline void PredAngle(Ipp8u *pSrc1, Ipp8u *pSrc2, Ipp8u *pDst1, Ipp8u *pDst2)
{
    switch (width)
    {
    case 4:
        PredAngle_4x4<mode>(pSrc1, pSrc2, pDst1, pDst2);
        break;
    case 8:
        PredAngle_8x8<mode>(pSrc1, pSrc2, pDst1, pDst2);
        break;
    case 16:
        PredAngle_16x16<mode>(pSrc1, pSrc2, pDst1, pDst2);
        break;
    case 32:
        PredAngle_32x32<mode>(pSrc1, pSrc2, pDst1, pDst2);
        break;
    }
}

//
// mode = 2,34 (shift left)
//
template <int width>
static inline void PredAngle2(Ipp8u *pSrc1, Ipp8u *pSrc2, Ipp8u *pDst1, Ipp8u *pDst2)
{
    switch (width)
    {
    case 4:
        {
            __m128i r0 = _mm_load_si128((__m128i *)&pSrc1[0]);  // load as [src2 src1]
            __m128i s0 = _mm_unpackhi_epi64(r0, r0);

            r0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)tab_left4x4));
            s0 = _mm_shuffle_epi8(s0, _mm_load_si128((__m128i *)tab_left4x4));

            _mm_store_si128((__m128i*)&pDst1[0], r0);
            _mm_store_si128((__m128i*)&pDst2[0], s0);
        }
        break;
    case 8:
        {
            __m128i r0 = _mm_load_si128((__m128i *)&pSrc1[0]);
            __m128i s0 = _mm_load_si128((__m128i *)&pSrc2[0]);

            __m128i t0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)tab_left8x8));
            __m128i t2 = _mm_shuffle_epi8(_mm_srli_si128(r0, 2), _mm_load_si128((__m128i *)tab_left8x8));
            __m128i t4 = _mm_shuffle_epi8(_mm_srli_si128(r0, 4), _mm_load_si128((__m128i *)tab_left8x8));
            __m128i t6 = _mm_shuffle_epi8(_mm_srli_si128(r0, 6), _mm_load_si128((__m128i *)tab_left8x8));

            _mm_store_si128((__m128i*)&pDst1[ 0], t0);
            _mm_store_si128((__m128i*)&pDst1[16], t2);
            _mm_store_si128((__m128i*)&pDst1[32], t4);
            _mm_store_si128((__m128i*)&pDst1[48], t6);

            t0 = _mm_shuffle_epi8(s0, _mm_load_si128((__m128i *)tab_left8x8));
            t2 = _mm_shuffle_epi8(_mm_srli_si128(s0, 2), _mm_load_si128((__m128i *)tab_left8x8));
            t4 = _mm_shuffle_epi8(_mm_srli_si128(s0, 4), _mm_load_si128((__m128i *)tab_left8x8));
            t6 = _mm_shuffle_epi8(_mm_srli_si128(s0, 6), _mm_load_si128((__m128i *)tab_left8x8));

            _mm_store_si128((__m128i*)&pDst2[ 0], t0);
            _mm_store_si128((__m128i*)&pDst2[16], t2);
            _mm_store_si128((__m128i*)&pDst2[32], t4);
            _mm_store_si128((__m128i*)&pDst2[48], t6);
        }
        break;
    case 16:
        {
            __m128i r0 = _mm_load_si128((__m128i *)&pSrc1[ 0]);
            __m128i r1 = _mm_load_si128((__m128i *)&pSrc1[16]);
            __m128i s0 = _mm_load_si128((__m128i *)&pSrc2[ 0]);
            __m128i s1 = _mm_load_si128((__m128i *)&pSrc2[16]);

            _mm_store_si128((__m128i*)&pDst1[ 0*16], _mm_alignr_epi8(r1, r0, 1));
            _mm_store_si128((__m128i*)&pDst1[ 1*16], _mm_alignr_epi8(r1, r0, 2));
            _mm_store_si128((__m128i*)&pDst1[ 2*16], _mm_alignr_epi8(r1, r0, 3));
            _mm_store_si128((__m128i*)&pDst1[ 3*16], _mm_alignr_epi8(r1, r0, 4));
            _mm_store_si128((__m128i*)&pDst1[ 4*16], _mm_alignr_epi8(r1, r0, 5));
            _mm_store_si128((__m128i*)&pDst1[ 5*16], _mm_alignr_epi8(r1, r0, 6));
            _mm_store_si128((__m128i*)&pDst1[ 6*16], _mm_alignr_epi8(r1, r0, 7));
            _mm_store_si128((__m128i*)&pDst1[ 7*16], _mm_alignr_epi8(r1, r0, 8));
            _mm_store_si128((__m128i*)&pDst1[ 8*16], _mm_alignr_epi8(r1, r0, 9));
            _mm_store_si128((__m128i*)&pDst1[ 9*16], _mm_alignr_epi8(r1, r0, 10));
            _mm_store_si128((__m128i*)&pDst1[10*16], _mm_alignr_epi8(r1, r0, 11));
            _mm_store_si128((__m128i*)&pDst1[11*16], _mm_alignr_epi8(r1, r0, 12));
            _mm_store_si128((__m128i*)&pDst1[12*16], _mm_alignr_epi8(r1, r0, 13));
            _mm_store_si128((__m128i*)&pDst1[13*16], _mm_alignr_epi8(r1, r0, 14));
            _mm_store_si128((__m128i*)&pDst1[14*16], _mm_alignr_epi8(r1, r0, 15));
            _mm_store_si128((__m128i*)&pDst1[15*16], r1);

            _mm_store_si128((__m128i*)&pDst2[ 0*16], _mm_alignr_epi8(s1, s0, 1));
            _mm_store_si128((__m128i*)&pDst2[ 1*16], _mm_alignr_epi8(s1, s0, 2));
            _mm_store_si128((__m128i*)&pDst2[ 2*16], _mm_alignr_epi8(s1, s0, 3));
            _mm_store_si128((__m128i*)&pDst2[ 3*16], _mm_alignr_epi8(s1, s0, 4));
            _mm_store_si128((__m128i*)&pDst2[ 4*16], _mm_alignr_epi8(s1, s0, 5));
            _mm_store_si128((__m128i*)&pDst2[ 5*16], _mm_alignr_epi8(s1, s0, 6));
            _mm_store_si128((__m128i*)&pDst2[ 6*16], _mm_alignr_epi8(s1, s0, 7));
            _mm_store_si128((__m128i*)&pDst2[ 7*16], _mm_alignr_epi8(s1, s0, 8));
            _mm_store_si128((__m128i*)&pDst2[ 8*16], _mm_alignr_epi8(s1, s0, 9));
            _mm_store_si128((__m128i*)&pDst2[ 9*16], _mm_alignr_epi8(s1, s0, 10));
            _mm_store_si128((__m128i*)&pDst2[10*16], _mm_alignr_epi8(s1, s0, 11));
            _mm_store_si128((__m128i*)&pDst2[11*16], _mm_alignr_epi8(s1, s0, 12));
            _mm_store_si128((__m128i*)&pDst2[12*16], _mm_alignr_epi8(s1, s0, 13));
            _mm_store_si128((__m128i*)&pDst2[13*16], _mm_alignr_epi8(s1, s0, 14));
            _mm_store_si128((__m128i*)&pDst2[14*16], _mm_alignr_epi8(s1, s0, 15));
            _mm_store_si128((__m128i*)&pDst2[15*16], s1);
        }
        break;
    case 32:
        {
            __m128i r0 = _mm_load_si128((__m128i *)&pSrc1[ 0]);
            __m128i r1 = _mm_load_si128((__m128i *)&pSrc1[16]);
            __m128i r2 = _mm_load_si128((__m128i *)&pSrc1[32]);
            __m128i r3 = _mm_load_si128((__m128i *)&pSrc1[48]);
            __m128i s0 = _mm_load_si128((__m128i *)&pSrc2[ 0]);
            __m128i s1 = _mm_load_si128((__m128i *)&pSrc2[16]);
            __m128i s2 = _mm_load_si128((__m128i *)&pSrc2[32]);
            __m128i s3 = _mm_load_si128((__m128i *)&pSrc2[48]);

            _mm_store_si128((__m128i*)&pDst1[ 0*32+ 0], _mm_alignr_epi8(r1, r0, 1));
            _mm_store_si128((__m128i*)&pDst1[ 0*32+16], _mm_alignr_epi8(r2, r1, 1));
            _mm_store_si128((__m128i*)&pDst1[ 1*32+ 0], _mm_alignr_epi8(r1, r0, 2));
            _mm_store_si128((__m128i*)&pDst1[ 1*32+16], _mm_alignr_epi8(r2, r1, 2));
            _mm_store_si128((__m128i*)&pDst1[ 2*32+ 0], _mm_alignr_epi8(r1, r0, 3));
            _mm_store_si128((__m128i*)&pDst1[ 2*32+16], _mm_alignr_epi8(r2, r1, 3));
            _mm_store_si128((__m128i*)&pDst1[ 3*32+ 0], _mm_alignr_epi8(r1, r0, 4));
            _mm_store_si128((__m128i*)&pDst1[ 3*32+16], _mm_alignr_epi8(r2, r1, 4));
            _mm_store_si128((__m128i*)&pDst1[ 4*32+ 0], _mm_alignr_epi8(r1, r0, 5));
            _mm_store_si128((__m128i*)&pDst1[ 4*32+16], _mm_alignr_epi8(r2, r1, 5));
            _mm_store_si128((__m128i*)&pDst1[ 5*32+ 0], _mm_alignr_epi8(r1, r0, 6));
            _mm_store_si128((__m128i*)&pDst1[ 5*32+16], _mm_alignr_epi8(r2, r1, 6));
            _mm_store_si128((__m128i*)&pDst1[ 6*32+ 0], _mm_alignr_epi8(r1, r0, 7));
            _mm_store_si128((__m128i*)&pDst1[ 6*32+16], _mm_alignr_epi8(r2, r1, 7));
            _mm_store_si128((__m128i*)&pDst1[ 7*32+ 0], _mm_alignr_epi8(r1, r0, 8));
            _mm_store_si128((__m128i*)&pDst1[ 7*32+16], _mm_alignr_epi8(r2, r1, 8));
            _mm_store_si128((__m128i*)&pDst1[ 8*32+ 0], _mm_alignr_epi8(r1, r0, 9));
            _mm_store_si128((__m128i*)&pDst1[ 8*32+16], _mm_alignr_epi8(r2, r1, 9));
            _mm_store_si128((__m128i*)&pDst1[ 9*32+ 0], _mm_alignr_epi8(r1, r0, 10));
            _mm_store_si128((__m128i*)&pDst1[ 9*32+16], _mm_alignr_epi8(r2, r1, 10));
            _mm_store_si128((__m128i*)&pDst1[10*32+ 0], _mm_alignr_epi8(r1, r0, 11));
            _mm_store_si128((__m128i*)&pDst1[10*32+16], _mm_alignr_epi8(r2, r1, 11));
            _mm_store_si128((__m128i*)&pDst1[11*32+ 0], _mm_alignr_epi8(r1, r0, 12));
            _mm_store_si128((__m128i*)&pDst1[11*32+16], _mm_alignr_epi8(r2, r1, 12));
            _mm_store_si128((__m128i*)&pDst1[12*32+ 0], _mm_alignr_epi8(r1, r0, 13));
            _mm_store_si128((__m128i*)&pDst1[12*32+16], _mm_alignr_epi8(r2, r1, 13));
            _mm_store_si128((__m128i*)&pDst1[13*32+ 0], _mm_alignr_epi8(r1, r0, 14));
            _mm_store_si128((__m128i*)&pDst1[13*32+16], _mm_alignr_epi8(r2, r1, 14));
            _mm_store_si128((__m128i*)&pDst1[14*32+ 0], _mm_alignr_epi8(r1, r0, 15));
            _mm_store_si128((__m128i*)&pDst1[14*32+16], _mm_alignr_epi8(r2, r1, 15));
            _mm_store_si128((__m128i*)&pDst1[15*32+ 0], r1);
            _mm_store_si128((__m128i*)&pDst1[15*32+16], r2);
            _mm_store_si128((__m128i*)&pDst1[16*32+ 0], _mm_alignr_epi8(r2, r1, 1));
            _mm_store_si128((__m128i*)&pDst1[16*32+16], _mm_alignr_epi8(r3, r2, 1));
            _mm_store_si128((__m128i*)&pDst1[17*32+ 0], _mm_alignr_epi8(r2, r1, 2));
            _mm_store_si128((__m128i*)&pDst1[17*32+16], _mm_alignr_epi8(r3, r2, 2));
            _mm_store_si128((__m128i*)&pDst1[18*32+ 0], _mm_alignr_epi8(r2, r1, 3));
            _mm_store_si128((__m128i*)&pDst1[18*32+16], _mm_alignr_epi8(r3, r2, 3));
            _mm_store_si128((__m128i*)&pDst1[19*32+ 0], _mm_alignr_epi8(r2, r1, 4));
            _mm_store_si128((__m128i*)&pDst1[19*32+16], _mm_alignr_epi8(r3, r2, 4));
            _mm_store_si128((__m128i*)&pDst1[20*32+ 0], _mm_alignr_epi8(r2, r1, 5));
            _mm_store_si128((__m128i*)&pDst1[20*32+16], _mm_alignr_epi8(r3, r2, 5));
            _mm_store_si128((__m128i*)&pDst1[21*32+ 0], _mm_alignr_epi8(r2, r1, 6));
            _mm_store_si128((__m128i*)&pDst1[21*32+16], _mm_alignr_epi8(r3, r2, 6));
            _mm_store_si128((__m128i*)&pDst1[22*32+ 0], _mm_alignr_epi8(r2, r1, 7));
            _mm_store_si128((__m128i*)&pDst1[22*32+16], _mm_alignr_epi8(r3, r2, 7));
            _mm_store_si128((__m128i*)&pDst1[23*32+ 0], _mm_alignr_epi8(r2, r1, 8));
            _mm_store_si128((__m128i*)&pDst1[23*32+16], _mm_alignr_epi8(r3, r2, 8));
            _mm_store_si128((__m128i*)&pDst1[24*32+ 0], _mm_alignr_epi8(r2, r1, 9));
            _mm_store_si128((__m128i*)&pDst1[24*32+16], _mm_alignr_epi8(r3, r2, 9));
            _mm_store_si128((__m128i*)&pDst1[25*32+ 0], _mm_alignr_epi8(r2, r1, 10));
            _mm_store_si128((__m128i*)&pDst1[25*32+16], _mm_alignr_epi8(r3, r2, 10));
            _mm_store_si128((__m128i*)&pDst1[26*32+ 0], _mm_alignr_epi8(r2, r1, 11));
            _mm_store_si128((__m128i*)&pDst1[26*32+16], _mm_alignr_epi8(r3, r2, 11));
            _mm_store_si128((__m128i*)&pDst1[27*32+ 0], _mm_alignr_epi8(r2, r1, 12));
            _mm_store_si128((__m128i*)&pDst1[27*32+16], _mm_alignr_epi8(r3, r2, 12));
            _mm_store_si128((__m128i*)&pDst1[28*32+ 0], _mm_alignr_epi8(r2, r1, 13));
            _mm_store_si128((__m128i*)&pDst1[28*32+16], _mm_alignr_epi8(r3, r2, 13));
            _mm_store_si128((__m128i*)&pDst1[29*32+ 0], _mm_alignr_epi8(r2, r1, 14));
            _mm_store_si128((__m128i*)&pDst1[29*32+16], _mm_alignr_epi8(r3, r2, 14));
            _mm_store_si128((__m128i*)&pDst1[30*32+ 0], _mm_alignr_epi8(r2, r1, 15));
            _mm_store_si128((__m128i*)&pDst1[30*32+16], _mm_alignr_epi8(r3, r2, 15));
            _mm_store_si128((__m128i*)&pDst1[31*32+ 0], r2);
            _mm_store_si128((__m128i*)&pDst1[31*32+16], r3);

            _mm_store_si128((__m128i*)&pDst2[ 0*32+ 0], _mm_alignr_epi8(s1, s0, 1));
            _mm_store_si128((__m128i*)&pDst2[ 0*32+16], _mm_alignr_epi8(s2, s1, 1));
            _mm_store_si128((__m128i*)&pDst2[ 1*32+ 0], _mm_alignr_epi8(s1, s0, 2));
            _mm_store_si128((__m128i*)&pDst2[ 1*32+16], _mm_alignr_epi8(s2, s1, 2));
            _mm_store_si128((__m128i*)&pDst2[ 2*32+ 0], _mm_alignr_epi8(s1, s0, 3));
            _mm_store_si128((__m128i*)&pDst2[ 2*32+16], _mm_alignr_epi8(s2, s1, 3));
            _mm_store_si128((__m128i*)&pDst2[ 3*32+ 0], _mm_alignr_epi8(s1, s0, 4));
            _mm_store_si128((__m128i*)&pDst2[ 3*32+16], _mm_alignr_epi8(s2, s1, 4));
            _mm_store_si128((__m128i*)&pDst2[ 4*32+ 0], _mm_alignr_epi8(s1, s0, 5));
            _mm_store_si128((__m128i*)&pDst2[ 4*32+16], _mm_alignr_epi8(s2, s1, 5));
            _mm_store_si128((__m128i*)&pDst2[ 5*32+ 0], _mm_alignr_epi8(s1, s0, 6));
            _mm_store_si128((__m128i*)&pDst2[ 5*32+16], _mm_alignr_epi8(s2, s1, 6));
            _mm_store_si128((__m128i*)&pDst2[ 6*32+ 0], _mm_alignr_epi8(s1, s0, 7));
            _mm_store_si128((__m128i*)&pDst2[ 6*32+16], _mm_alignr_epi8(s2, s1, 7));
            _mm_store_si128((__m128i*)&pDst2[ 7*32+ 0], _mm_alignr_epi8(s1, s0, 8));
            _mm_store_si128((__m128i*)&pDst2[ 7*32+16], _mm_alignr_epi8(s2, s1, 8));
            _mm_store_si128((__m128i*)&pDst2[ 8*32+ 0], _mm_alignr_epi8(s1, s0, 9));
            _mm_store_si128((__m128i*)&pDst2[ 8*32+16], _mm_alignr_epi8(s2, s1, 9));
            _mm_store_si128((__m128i*)&pDst2[ 9*32+ 0], _mm_alignr_epi8(s1, s0, 10));
            _mm_store_si128((__m128i*)&pDst2[ 9*32+16], _mm_alignr_epi8(s2, s1, 10));
            _mm_store_si128((__m128i*)&pDst2[10*32+ 0], _mm_alignr_epi8(s1, s0, 11));
            _mm_store_si128((__m128i*)&pDst2[10*32+16], _mm_alignr_epi8(s2, s1, 11));
            _mm_store_si128((__m128i*)&pDst2[11*32+ 0], _mm_alignr_epi8(s1, s0, 12));
            _mm_store_si128((__m128i*)&pDst2[11*32+16], _mm_alignr_epi8(s2, s1, 12));
            _mm_store_si128((__m128i*)&pDst2[12*32+ 0], _mm_alignr_epi8(s1, s0, 13));
            _mm_store_si128((__m128i*)&pDst2[12*32+16], _mm_alignr_epi8(s2, s1, 13));
            _mm_store_si128((__m128i*)&pDst2[13*32+ 0], _mm_alignr_epi8(s1, s0, 14));
            _mm_store_si128((__m128i*)&pDst2[13*32+16], _mm_alignr_epi8(s2, s1, 14));
            _mm_store_si128((__m128i*)&pDst2[14*32+ 0], _mm_alignr_epi8(s1, s0, 15));
            _mm_store_si128((__m128i*)&pDst2[14*32+16], _mm_alignr_epi8(s2, s1, 15));
            _mm_store_si128((__m128i*)&pDst2[15*32+ 0], s1);
            _mm_store_si128((__m128i*)&pDst2[15*32+16], s2);
            _mm_store_si128((__m128i*)&pDst2[16*32+ 0], _mm_alignr_epi8(s2, s1, 1));
            _mm_store_si128((__m128i*)&pDst2[16*32+16], _mm_alignr_epi8(s3, s2, 1));
            _mm_store_si128((__m128i*)&pDst2[17*32+ 0], _mm_alignr_epi8(s2, s1, 2));
            _mm_store_si128((__m128i*)&pDst2[17*32+16], _mm_alignr_epi8(s3, s2, 2));
            _mm_store_si128((__m128i*)&pDst2[18*32+ 0], _mm_alignr_epi8(s2, s1, 3));
            _mm_store_si128((__m128i*)&pDst2[18*32+16], _mm_alignr_epi8(s3, s2, 3));
            _mm_store_si128((__m128i*)&pDst2[19*32+ 0], _mm_alignr_epi8(s2, s1, 4));
            _mm_store_si128((__m128i*)&pDst2[19*32+16], _mm_alignr_epi8(s3, s2, 4));
            _mm_store_si128((__m128i*)&pDst2[20*32+ 0], _mm_alignr_epi8(s2, s1, 5));
            _mm_store_si128((__m128i*)&pDst2[20*32+16], _mm_alignr_epi8(s3, s2, 5));
            _mm_store_si128((__m128i*)&pDst2[21*32+ 0], _mm_alignr_epi8(s2, s1, 6));
            _mm_store_si128((__m128i*)&pDst2[21*32+16], _mm_alignr_epi8(s3, s2, 6));
            _mm_store_si128((__m128i*)&pDst2[22*32+ 0], _mm_alignr_epi8(s2, s1, 7));
            _mm_store_si128((__m128i*)&pDst2[22*32+16], _mm_alignr_epi8(s3, s2, 7));
            _mm_store_si128((__m128i*)&pDst2[23*32+ 0], _mm_alignr_epi8(s2, s1, 8));
            _mm_store_si128((__m128i*)&pDst2[23*32+16], _mm_alignr_epi8(s3, s2, 8));
            _mm_store_si128((__m128i*)&pDst2[24*32+ 0], _mm_alignr_epi8(s2, s1, 9));
            _mm_store_si128((__m128i*)&pDst2[24*32+16], _mm_alignr_epi8(s3, s2, 9));
            _mm_store_si128((__m128i*)&pDst2[25*32+ 0], _mm_alignr_epi8(s2, s1, 10));
            _mm_store_si128((__m128i*)&pDst2[25*32+16], _mm_alignr_epi8(s3, s2, 10));
            _mm_store_si128((__m128i*)&pDst2[26*32+ 0], _mm_alignr_epi8(s2, s1, 11));
            _mm_store_si128((__m128i*)&pDst2[26*32+16], _mm_alignr_epi8(s3, s2, 11));
            _mm_store_si128((__m128i*)&pDst2[27*32+ 0], _mm_alignr_epi8(s2, s1, 12));
            _mm_store_si128((__m128i*)&pDst2[27*32+16], _mm_alignr_epi8(s3, s2, 12));
            _mm_store_si128((__m128i*)&pDst2[28*32+ 0], _mm_alignr_epi8(s2, s1, 13));
            _mm_store_si128((__m128i*)&pDst2[28*32+16], _mm_alignr_epi8(s3, s2, 13));
            _mm_store_si128((__m128i*)&pDst2[29*32+ 0], _mm_alignr_epi8(s2, s1, 14));
            _mm_store_si128((__m128i*)&pDst2[29*32+16], _mm_alignr_epi8(s3, s2, 14));
            _mm_store_si128((__m128i*)&pDst2[30*32+ 0], _mm_alignr_epi8(s2, s1, 15));
            _mm_store_si128((__m128i*)&pDst2[30*32+16], _mm_alignr_epi8(s3, s2, 15));
            _mm_store_si128((__m128i*)&pDst2[31*32+ 0], s2);
            _mm_store_si128((__m128i*)&pDst2[31*32+16], s3);
        }
        break;
    }
}

ALIGN_DECL(32) static const Ipp8s tab_bfilt8x8[16] =
    {  8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0 };

// compute USAT8(a + ((b - c) >> 1))
static inline __m128i BoundaryFilter(__m128i a, __m128i b, __m128i c)
{
    c = _mm_xor_si128(c, _mm_set1_epi8(0xff));
    b = _mm_avg_epu8(b, c);
    b = _mm_sub_epi8(b, _mm_set1_epi8(0x80));   // pavgb(b,~c)-0x80 = (b-c)>>1

    a = _mm_sub_epi8(a, _mm_set1_epi8(0x80));   // convert to signed
    a = _mm_adds_epi8(a, b);                    // signed saturate
    a = _mm_add_epi8(a, _mm_set1_epi8(0x80));   // convert to unsigned
    return a;
}

//
// mode 10,26 (copy)
// Src is unaligned, Dst is aligned
template <int width>
static inline void PredAngle10(Ipp8u *pSrc1, Ipp8u *pSrc2, Ipp8u *pDst1, Ipp8u *pDst2)
{
    switch (width)
    {
    case 4:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[1]); // load as [src2 src1]
            __m128i s0;

            __m128i t0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)tab_bfilt8x8));
            __m128i u0 = _mm_set1_epi8(pSrc1[0]);

            t0 = BoundaryFilter(t0, r0, u0);

            s0 = _mm_shuffle_epi32(r0, _MM_SHUFFLE(2,2,2,2));
            r0 = _mm_shuffle_epi32(r0, _MM_SHUFFLE(0,0,0,0));

            _mm_store_si128((__m128i *)&pDst1[0], s0);
            _mm_store_si128((__m128i *)&pDst2[0], r0);

            pDst1[0*4] = (Ipp8u)_mm_extract_epi8(t0, 0);
            pDst1[1*4] = (Ipp8u)_mm_extract_epi8(t0, 1);
            pDst1[2*4] = (Ipp8u)_mm_extract_epi8(t0, 2);
            pDst1[3*4] = (Ipp8u)_mm_extract_epi8(t0, 3);

            pDst2[0*4] = (Ipp8u)_mm_extract_epi8(t0, 8);
            pDst2[1*4] = (Ipp8u)_mm_extract_epi8(t0, 9);
            pDst2[2*4] = (Ipp8u)_mm_extract_epi8(t0, 10);
            pDst2[3*4] = (Ipp8u)_mm_extract_epi8(t0, 11);
        }
        break;
    case 8:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[1]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[1]);

            __m128i rs = _mm_unpacklo_epi64(r0, s0);
            __m128i t0 = _mm_shuffle_epi8(rs, _mm_load_si128((__m128i *)tab_bfilt8x8));
            __m128i u0 = _mm_set1_epi8(pSrc1[0]);

            t0 = BoundaryFilter(t0, rs, u0);

            r0 = _mm_unpacklo_epi64(r0, r0);
            s0 = _mm_unpacklo_epi64(s0, s0);

            for (int i = 0; i < 4; i++)
            {
                _mm_store_si128((__m128i *)&pDst1[i*16], s0);
                _mm_store_si128((__m128i *)&pDst2[i*16], r0);
            }

            pDst1[0*8] = (Ipp8u)_mm_extract_epi8(t0, 0);
            pDst1[1*8] = (Ipp8u)_mm_extract_epi8(t0, 1);
            pDst1[2*8] = (Ipp8u)_mm_extract_epi8(t0, 2);
            pDst1[3*8] = (Ipp8u)_mm_extract_epi8(t0, 3);
            pDst1[4*8] = (Ipp8u)_mm_extract_epi8(t0, 4);
            pDst1[5*8] = (Ipp8u)_mm_extract_epi8(t0, 5);
            pDst1[6*8] = (Ipp8u)_mm_extract_epi8(t0, 6);
            pDst1[7*8] = (Ipp8u)_mm_extract_epi8(t0, 7);

            pDst2[0*8] = (Ipp8u)_mm_extract_epi8(t0, 8);
            pDst2[1*8] = (Ipp8u)_mm_extract_epi8(t0, 9);
            pDst2[2*8] = (Ipp8u)_mm_extract_epi8(t0, 10);
            pDst2[3*8] = (Ipp8u)_mm_extract_epi8(t0, 11);
            pDst2[4*8] = (Ipp8u)_mm_extract_epi8(t0, 12);
            pDst2[5*8] = (Ipp8u)_mm_extract_epi8(t0, 13);
            pDst2[6*8] = (Ipp8u)_mm_extract_epi8(t0, 14);
            pDst2[7*8] = (Ipp8u)_mm_extract_epi8(t0, 15);
        }
        break;
    case 16:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[1]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[1]);

            __m128i t0 = _mm_shuffle_epi8(s0, _mm_setzero_si128());
            __m128i t1 = _mm_shuffle_epi8(r0, _mm_setzero_si128());
            __m128i u0 = _mm_set1_epi8(pSrc1[0]);

            t0 = BoundaryFilter(t0, r0, u0);
            t1 = BoundaryFilter(t1, s0, u0);

            for (int i = 0; i < 16; i++)
            {
                _mm_store_si128((__m128i *)&pDst1[i*16], s0);
                _mm_store_si128((__m128i *)&pDst2[i*16], r0);
            }

            pDst1[0*16] = (Ipp8u)_mm_extract_epi8(t0, 0);
            pDst1[1*16] = (Ipp8u)_mm_extract_epi8(t0, 1);
            pDst1[2*16] = (Ipp8u)_mm_extract_epi8(t0, 2);
            pDst1[3*16] = (Ipp8u)_mm_extract_epi8(t0, 3);
            pDst1[4*16] = (Ipp8u)_mm_extract_epi8(t0, 4);
            pDst1[5*16] = (Ipp8u)_mm_extract_epi8(t0, 5);
            pDst1[6*16] = (Ipp8u)_mm_extract_epi8(t0, 6);
            pDst1[7*16] = (Ipp8u)_mm_extract_epi8(t0, 7);
            pDst1[8*16] = (Ipp8u)_mm_extract_epi8(t0, 8);
            pDst1[9*16] = (Ipp8u)_mm_extract_epi8(t0, 9);
            pDst1[10*16] = (Ipp8u)_mm_extract_epi8(t0, 10);
            pDst1[11*16] = (Ipp8u)_mm_extract_epi8(t0, 11);
            pDst1[12*16] = (Ipp8u)_mm_extract_epi8(t0, 12);
            pDst1[13*16] = (Ipp8u)_mm_extract_epi8(t0, 13);
            pDst1[14*16] = (Ipp8u)_mm_extract_epi8(t0, 14);
            pDst1[15*16] = (Ipp8u)_mm_extract_epi8(t0, 15);

            pDst2[0*16] = (Ipp8u)_mm_extract_epi8(t1, 0);
            pDst2[1*16] = (Ipp8u)_mm_extract_epi8(t1, 1);
            pDst2[2*16] = (Ipp8u)_mm_extract_epi8(t1, 2);
            pDst2[3*16] = (Ipp8u)_mm_extract_epi8(t1, 3);
            pDst2[4*16] = (Ipp8u)_mm_extract_epi8(t1, 4);
            pDst2[5*16] = (Ipp8u)_mm_extract_epi8(t1, 5);
            pDst2[6*16] = (Ipp8u)_mm_extract_epi8(t1, 6);
            pDst2[7*16] = (Ipp8u)_mm_extract_epi8(t1, 7);
            pDst2[8*16] = (Ipp8u)_mm_extract_epi8(t1, 8);
            pDst2[9*16] = (Ipp8u)_mm_extract_epi8(t1, 9);
            pDst2[10*16] = (Ipp8u)_mm_extract_epi8(t1, 10);
            pDst2[11*16] = (Ipp8u)_mm_extract_epi8(t1, 11);
            pDst2[12*16] = (Ipp8u)_mm_extract_epi8(t1, 12);
            pDst2[13*16] = (Ipp8u)_mm_extract_epi8(t1, 13);
            pDst2[14*16] = (Ipp8u)_mm_extract_epi8(t1, 14);
            pDst2[15*16] = (Ipp8u)_mm_extract_epi8(t1, 15);
        }
        break;
    case 32:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[ 1]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[17]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[ 1]);
            __m128i s1 = _mm_loadu_si128((__m128i *)&pSrc2[17]);

            for (int i = 0; i < 32; i++)
            {
                _mm_store_si128((__m128i *)&pDst1[i*32+ 0], s0);
                _mm_store_si128((__m128i *)&pDst1[i*32+16], s1);
                _mm_store_si128((__m128i *)&pDst2[i*32+ 0], r0);
                _mm_store_si128((__m128i *)&pDst2[i*32+16], r1);
            }
        }
        break;
    }
}

// Src is unaligned, Dst is aligned
template <int width>
static void CopyLine(Ipp8u *pSrc1, Ipp8u *pSrc2, Ipp8u *pDst1, Ipp8u *pDst2)
{
    switch (width) {
    case 4:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[1]); // load as [src2 src1]
            __m128i s0 = _mm_shuffle_epi32(r0, _MM_SHUFFLE(1,0,3,2));

            _mm_store_si128((__m128i *)&pDst1[0], s0);          // store as [src2 src1]
        }
        break;
    case 8:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[1]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[1]);

            _mm_store_si128((__m128i *)&pDst1[0], s0);
            _mm_store_si128((__m128i *)&pDst2[0], r0);
        }
        break;
    case 16:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[ 1]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[17]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[ 1]);
            __m128i s1 = _mm_loadu_si128((__m128i *)&pSrc2[17]);

            _mm_store_si128((__m128i *)&pDst1[ 0], s0);
            _mm_store_si128((__m128i *)&pDst1[16], s1);
            _mm_store_si128((__m128i *)&pDst2[ 0], r0);
            _mm_store_si128((__m128i *)&pDst2[16], r1);
        }
        break;
    case 32:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[ 1]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[17]);
            __m128i r2 = _mm_loadu_si128((__m128i *)&pSrc1[33]);
            __m128i r3 = _mm_loadu_si128((__m128i *)&pSrc1[49]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[ 1]);
            __m128i s1 = _mm_loadu_si128((__m128i *)&pSrc2[17]);
            __m128i s2 = _mm_loadu_si128((__m128i *)&pSrc2[33]);
            __m128i s3 = _mm_loadu_si128((__m128i *)&pSrc2[49]);

            _mm_store_si128((__m128i *)&pDst1[ 0], s0);
            _mm_store_si128((__m128i *)&pDst1[16], s1);
            _mm_store_si128((__m128i *)&pDst1[32], s2);
            _mm_store_si128((__m128i *)&pDst1[48], s3);
            _mm_store_si128((__m128i *)&pDst2[ 0], r0);
            _mm_store_si128((__m128i *)&pDst2[16], r1);
            _mm_store_si128((__m128i *)&pDst2[32], r2);
            _mm_store_si128((__m128i *)&pDst2[48], r3);
        }
        break;
    }
}

// Src is unaligned, Dst is aligned
template <int width, int mode>
static void ProjLine(Ipp8u *pSrc1, Ipp8u *pSrc2, Ipp8u *pDst1, Ipp8u *pDst2)
{
    switch (width) {
    case 4:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[0]); // load as [src2 src1]

            __m128i t0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)proj_4x4[mode-11]));

            _mm_store_si128((__m128i *)&pDst1[0], t0);          // store as [src2 src1]
        }
        break;
    case 8:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[0]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[0]);

            __m128i t0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)proj_8x8[mode-11]));
            __m128i t1 = _mm_shuffle_epi8(s0, _mm_load_si128((__m128i *)proj_8x8[mode-11]));

            t1 = _mm_insert_epi8(t1, pSrc1[0], 7);  // pDst2[7] = pSrc1[0];

            t1 = _mm_shuffle_epi32(t1, _MM_SHUFFLE(1,0,3,2));
            _mm_store_si128((__m128i *)&pDst1[0], _mm_unpacklo_epi64(t0, t1));
            _mm_store_si128((__m128i *)&pDst2[0], _mm_unpackhi_epi64(t1, t0));
        }
        break;
    case 16:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[0]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[0]);

            __m128i t0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)proj_16x16[mode-11]));
            __m128i t1 = _mm_shuffle_epi8(s0, _mm_load_si128((__m128i *)proj_16x16[mode-11]));

            t1 = _mm_insert_epi8(t1, pSrc1[0], 15); // pDst2[15] = pSrc1[0];

            _mm_store_si128((__m128i *)&pDst1[0], t0);
            _mm_store_si128((__m128i *)&pDst2[0], t1);

            // shift the "above" pixels
            r0 = _mm_insert_epi8(_mm_srli_si128(r0, 1), pSrc1[16], 15);
            s0 = _mm_insert_epi8(_mm_srli_si128(s0, 1), pSrc2[16], 15);
            _mm_store_si128((__m128i *)&pDst1[16], s0);
            _mm_store_si128((__m128i *)&pDst2[16], r0);
        }
        break;
    case 32:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[0]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[16]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[0]);
            __m128i s1 = _mm_loadu_si128((__m128i *)&pSrc2[16]);

            __m128i t0 = _mm_shuffle_epi8(r1, _mm_load_si128((__m128i *)proj_32x32[mode-11][0]));
            __m128i t1 = _mm_shuffle_epi8(s1, _mm_load_si128((__m128i *)proj_32x32[mode-11][0]));

            _mm_store_si128((__m128i *)&pDst1[0], t0);
            _mm_store_si128((__m128i *)&pDst2[0], t1);

            t0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)proj_32x32[mode-11][1]));
            t1 = _mm_shuffle_epi8(s0, _mm_load_si128((__m128i *)proj_32x32[mode-11][1]));
            t0 = _mm_or_si128(t0, _mm_shuffle_epi8(r1, _mm_load_si128((__m128i *)proj_32x32[mode-11][2])));
            t1 = _mm_or_si128(t1, _mm_shuffle_epi8(s1, _mm_load_si128((__m128i *)proj_32x32[mode-11][2])));

            t1 = _mm_insert_epi8(t1, pSrc1[0], 15); // pDst2[31] = pSrc1[0];

            _mm_store_si128((__m128i *)&pDst1[16], t0);
            _mm_store_si128((__m128i *)&pDst2[16], t1);

            // shift the "above" pixels
            r0 = _mm_alignr_epi8(r1, r0, 1);
            s0 = _mm_alignr_epi8(s1, s0, 1);
            _mm_store_si128((__m128i *)&pDst1[32], s0);
            _mm_store_si128((__m128i *)&pDst2[32], r0);

            r1 = _mm_insert_epi8(_mm_srli_si128(r1, 1), pSrc1[32], 15);
            s1 = _mm_insert_epi8(_mm_srli_si128(s1, 1), pSrc2[32], 15);
            _mm_store_si128((__m128i *)&pDst1[48], s1);
            _mm_store_si128((__m128i *)&pDst2[48], r1);
        }
        break;
    }
}

//
// mode 18 (shift right)
// Src is unaligned, Dst is aligned
template <int width>
static inline void PredAngle18(Ipp8u *pSrc1, Ipp8u *pSrc2, Ipp8u *pDst2)
{
    switch (width)
    {
    case 4:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[0]); // load as [src2 src1]

            __m128i t0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)tab_right4x4));

            _mm_store_si128((__m128i *)&pDst2[0], t0);
        }
        break;
    case 8:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[0]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[0]);
            r0 = _mm_unpacklo_epi64(r0, s0);

            r0 = _mm_shuffle_epi8(r0, _mm_load_si128((__m128i *)tab_right8x8));

            __m128i t0 = _mm_srli_si128(r0, 7);
            __m128i t1 = _mm_srli_si128(r0, 6);
            __m128i t2 = _mm_srli_si128(r0, 5);
            __m128i t3 = _mm_srli_si128(r0, 4);
            __m128i t4 = _mm_srli_si128(r0, 3);
            __m128i t5 = _mm_srli_si128(r0, 2);
            __m128i t6 = _mm_srli_si128(r0, 1);
            __m128i t7 = r0;

            t0 = _mm_unpacklo_epi64(t0, t1);
            t2 = _mm_unpacklo_epi64(t2, t3);
            t4 = _mm_unpacklo_epi64(t4, t5);
            t6 = _mm_unpacklo_epi64(t6, t7);

            _mm_store_si128((__m128i *)&pDst2[ 0], t0);
            _mm_store_si128((__m128i *)&pDst2[16], t2);
            _mm_store_si128((__m128i *)&pDst2[32], t4);
            _mm_store_si128((__m128i *)&pDst2[48], t6);
        }
        break;
    case 16:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[0]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[0]);

            s0 = _mm_shuffle_epi8(s0, _mm_load_si128((__m128i *)tab_right16x16));

            _mm_store_si128((__m128i*)&pDst2[ 0*16], r0);
            _mm_store_si128((__m128i*)&pDst2[ 1*16], _mm_alignr_epi8(r0, s0, 15));
            _mm_store_si128((__m128i*)&pDst2[ 2*16], _mm_alignr_epi8(r0, s0, 14));
            _mm_store_si128((__m128i*)&pDst2[ 3*16], _mm_alignr_epi8(r0, s0, 13));
            _mm_store_si128((__m128i*)&pDst2[ 4*16], _mm_alignr_epi8(r0, s0, 12));
            _mm_store_si128((__m128i*)&pDst2[ 5*16], _mm_alignr_epi8(r0, s0, 11));
            _mm_store_si128((__m128i*)&pDst2[ 6*16], _mm_alignr_epi8(r0, s0, 10));
            _mm_store_si128((__m128i*)&pDst2[ 7*16], _mm_alignr_epi8(r0, s0, 9));
            _mm_store_si128((__m128i*)&pDst2[ 8*16], _mm_alignr_epi8(r0, s0, 8));
            _mm_store_si128((__m128i*)&pDst2[ 9*16], _mm_alignr_epi8(r0, s0, 7));
            _mm_store_si128((__m128i*)&pDst2[10*16], _mm_alignr_epi8(r0, s0, 6));
            _mm_store_si128((__m128i*)&pDst2[11*16], _mm_alignr_epi8(r0, s0, 5));
            _mm_store_si128((__m128i*)&pDst2[12*16], _mm_alignr_epi8(r0, s0, 4));
            _mm_store_si128((__m128i*)&pDst2[13*16], _mm_alignr_epi8(r0, s0, 3));
            _mm_store_si128((__m128i*)&pDst2[14*16], _mm_alignr_epi8(r0, s0, 2));
            _mm_store_si128((__m128i*)&pDst2[15*16], _mm_alignr_epi8(r0, s0, 1));
        }
        break;
    case 32:
        {
            __m128i r0 = _mm_loadu_si128((__m128i *)&pSrc1[0]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&pSrc1[16]);
            __m128i s0 = _mm_loadu_si128((__m128i *)&pSrc2[0]);
            __m128i s1 = _mm_loadu_si128((__m128i *)&pSrc2[16]);

            s0 = _mm_shuffle_epi8(s0, _mm_load_si128((__m128i *)tab_right32x32));
            s1 = _mm_shuffle_epi8(s1, _mm_load_si128((__m128i *)tab_right32x32));
            s0 = _mm_alignr_epi8(s0, s1, 15);
            s1 = _mm_slli_si128(s1, 1);

            _mm_store_si128((__m128i*)&pDst2[ 0*32+ 0], r0);
            _mm_store_si128((__m128i*)&pDst2[ 0*32+16], r1);
            _mm_store_si128((__m128i*)&pDst2[ 1*32+ 0], _mm_alignr_epi8(r0, s0, 15));
            _mm_store_si128((__m128i*)&pDst2[ 1*32+16], _mm_alignr_epi8(r1, r0, 15));
            _mm_store_si128((__m128i*)&pDst2[ 2*32+ 0], _mm_alignr_epi8(r0, s0, 14));
            _mm_store_si128((__m128i*)&pDst2[ 2*32+16], _mm_alignr_epi8(r1, r0, 14));
            _mm_store_si128((__m128i*)&pDst2[ 3*32+ 0], _mm_alignr_epi8(r0, s0, 13));
            _mm_store_si128((__m128i*)&pDst2[ 3*32+16], _mm_alignr_epi8(r1, r0, 13));
            _mm_store_si128((__m128i*)&pDst2[ 4*32+ 0], _mm_alignr_epi8(r0, s0, 12));
            _mm_store_si128((__m128i*)&pDst2[ 4*32+16], _mm_alignr_epi8(r1, r0, 12));
            _mm_store_si128((__m128i*)&pDst2[ 5*32+ 0], _mm_alignr_epi8(r0, s0, 11));
            _mm_store_si128((__m128i*)&pDst2[ 5*32+16], _mm_alignr_epi8(r1, r0, 11));
            _mm_store_si128((__m128i*)&pDst2[ 6*32+ 0], _mm_alignr_epi8(r0, s0, 10));
            _mm_store_si128((__m128i*)&pDst2[ 6*32+16], _mm_alignr_epi8(r1, r0, 10));
            _mm_store_si128((__m128i*)&pDst2[ 7*32+ 0], _mm_alignr_epi8(r0, s0, 9));
            _mm_store_si128((__m128i*)&pDst2[ 7*32+16], _mm_alignr_epi8(r1, r0, 9));
            _mm_store_si128((__m128i*)&pDst2[ 8*32+ 0], _mm_alignr_epi8(r0, s0, 8));
            _mm_store_si128((__m128i*)&pDst2[ 8*32+16], _mm_alignr_epi8(r1, r0, 8));
            _mm_store_si128((__m128i*)&pDst2[ 9*32+ 0], _mm_alignr_epi8(r0, s0, 7));
            _mm_store_si128((__m128i*)&pDst2[ 9*32+16], _mm_alignr_epi8(r1, r0, 7));
            _mm_store_si128((__m128i*)&pDst2[10*32+ 0], _mm_alignr_epi8(r0, s0, 6));
            _mm_store_si128((__m128i*)&pDst2[10*32+16], _mm_alignr_epi8(r1, r0, 6));
            _mm_store_si128((__m128i*)&pDst2[11*32+ 0], _mm_alignr_epi8(r0, s0, 5));
            _mm_store_si128((__m128i*)&pDst2[11*32+16], _mm_alignr_epi8(r1, r0, 5));
            _mm_store_si128((__m128i*)&pDst2[12*32+ 0], _mm_alignr_epi8(r0, s0, 4));
            _mm_store_si128((__m128i*)&pDst2[12*32+16], _mm_alignr_epi8(r1, r0, 4));
            _mm_store_si128((__m128i*)&pDst2[13*32+ 0], _mm_alignr_epi8(r0, s0, 3));
            _mm_store_si128((__m128i*)&pDst2[13*32+16], _mm_alignr_epi8(r1, r0, 3));
            _mm_store_si128((__m128i*)&pDst2[14*32+ 0], _mm_alignr_epi8(r0, s0, 2));
            _mm_store_si128((__m128i*)&pDst2[14*32+16], _mm_alignr_epi8(r1, r0, 2));
            _mm_store_si128((__m128i*)&pDst2[15*32+ 0], _mm_alignr_epi8(r0, s0, 1));
            _mm_store_si128((__m128i*)&pDst2[15*32+16], _mm_alignr_epi8(r1, r0, 1));

            _mm_store_si128((__m128i*)&pDst2[16*32+ 0], s0);
            _mm_store_si128((__m128i*)&pDst2[16*32+16], r0);
            _mm_store_si128((__m128i*)&pDst2[17*32+ 0], _mm_alignr_epi8(s0, s1, 15));
            _mm_store_si128((__m128i*)&pDst2[17*32+16], _mm_alignr_epi8(r0, s0, 15));
            _mm_store_si128((__m128i*)&pDst2[18*32+ 0], _mm_alignr_epi8(s0, s1, 14));
            _mm_store_si128((__m128i*)&pDst2[18*32+16], _mm_alignr_epi8(r0, s0, 14));
            _mm_store_si128((__m128i*)&pDst2[19*32+ 0], _mm_alignr_epi8(s0, s1, 13));
            _mm_store_si128((__m128i*)&pDst2[19*32+16], _mm_alignr_epi8(r0, s0, 13));
            _mm_store_si128((__m128i*)&pDst2[20*32+ 0], _mm_alignr_epi8(s0, s1, 12));
            _mm_store_si128((__m128i*)&pDst2[20*32+16], _mm_alignr_epi8(r0, s0, 12));
            _mm_store_si128((__m128i*)&pDst2[21*32+ 0], _mm_alignr_epi8(s0, s1, 11));
            _mm_store_si128((__m128i*)&pDst2[21*32+16], _mm_alignr_epi8(r0, s0, 11));
            _mm_store_si128((__m128i*)&pDst2[22*32+ 0], _mm_alignr_epi8(s0, s1, 10));
            _mm_store_si128((__m128i*)&pDst2[22*32+16], _mm_alignr_epi8(r0, s0, 10));
            _mm_store_si128((__m128i*)&pDst2[23*32+ 0], _mm_alignr_epi8(s0, s1, 9));
            _mm_store_si128((__m128i*)&pDst2[23*32+16], _mm_alignr_epi8(r0, s0, 9));
            _mm_store_si128((__m128i*)&pDst2[24*32+ 0], _mm_alignr_epi8(s0, s1, 8));
            _mm_store_si128((__m128i*)&pDst2[24*32+16], _mm_alignr_epi8(r0, s0, 8));
            _mm_store_si128((__m128i*)&pDst2[25*32+ 0], _mm_alignr_epi8(s0, s1, 7));
            _mm_store_si128((__m128i*)&pDst2[25*32+16], _mm_alignr_epi8(r0, s0, 7));
            _mm_store_si128((__m128i*)&pDst2[26*32+ 0], _mm_alignr_epi8(s0, s1, 6));
            _mm_store_si128((__m128i*)&pDst2[26*32+16], _mm_alignr_epi8(r0, s0, 6));
            _mm_store_si128((__m128i*)&pDst2[27*32+ 0], _mm_alignr_epi8(s0, s1, 5));
            _mm_store_si128((__m128i*)&pDst2[27*32+16], _mm_alignr_epi8(r0, s0, 5));
            _mm_store_si128((__m128i*)&pDst2[28*32+ 0], _mm_alignr_epi8(s0, s1, 4));
            _mm_store_si128((__m128i*)&pDst2[28*32+16], _mm_alignr_epi8(r0, s0, 4));
            _mm_store_si128((__m128i*)&pDst2[29*32+ 0], _mm_alignr_epi8(s0, s1, 3));
            _mm_store_si128((__m128i*)&pDst2[29*32+16], _mm_alignr_epi8(r0, s0, 3));
            _mm_store_si128((__m128i*)&pDst2[30*32+ 0], _mm_alignr_epi8(s0, s1, 2));
            _mm_store_si128((__m128i*)&pDst2[30*32+16], _mm_alignr_epi8(r0, s0, 2));
            _mm_store_si128((__m128i*)&pDst2[31*32+ 0], _mm_alignr_epi8(s0, s1, 1));
            _mm_store_si128((__m128i*)&pDst2[31*32+16], _mm_alignr_epi8(r0, s0, 1));
        }
        break;
    }
}

static void PredictIntra_Ang_All_4x4_Even(Ipp8u* PredPel, Ipp8u* /*FiltPel*/, Ipp8u* pels)
{
    ALIGN_DECL(64) Ipp8u ref1[16];  // input for mode < 18
    ALIGN_DECL(64) Ipp8u ref2[16];  // input for mode > 18

    Ipp8u *PredPel2 = PredPel + 2 * 4;
    //Ipp8u *FiltPel2 = FiltPel + 2 * 4;
    Ipp8u (*buf)[4*4] = (Ipp8u(*)[4*4])pels;

    // unfiltered
    CopyLine<4>(PredPel, PredPel2, ref1, ref2);
    PredAngle2<4>(ref1, ref2, buf[2-2], buf[34-2]);
    PredAngle<4,4>(ref1, ref2, buf[4-2], buf[32-2]);
    PredAngle<4,6>(ref1, ref2, buf[6-2], buf[30-2]);
    PredAngle<4,8>(ref1, ref2, buf[8-2], buf[28-2]);

    PredAngle10<4>(PredPel, PredPel2, buf[10-2], buf[26-2]);

    ProjLine<4,12>(PredPel, PredPel2, ref1, ref2);
    PredAngle<4,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
    ProjLine<4,14>(PredPel, PredPel2, ref1, ref2);
    PredAngle<4,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
    ProjLine<4,16>(PredPel, PredPel2, ref1, ref2);
    PredAngle<4,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
    PredAngle18<4>(PredPel, PredPel2, buf[18-2]);
}

static void PredictIntra_Ang_All_8x8_Even(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels)
{
    ALIGN_DECL(64) Ipp8u ref1[16];  // input for mode < 18
    ALIGN_DECL(64) Ipp8u ref2[16];  // input for mode > 18

    Ipp8u *PredPel2 = PredPel + 2 * 8;
    Ipp8u *FiltPel2 = FiltPel + 2 * 8;
    Ipp8u (*buf)[8*8] = (Ipp8u(*)[8*8])pels;

    // filtered
    CopyLine<8>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle2<8>(ref1, ref2, buf[2-2], buf[34-2]);

    // unfiltered
    CopyLine<8>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,4>(ref1, ref2, buf[4-2], buf[32-2]);
    PredAngle<8,6>(ref1, ref2, buf[6-2], buf[30-2]);
    PredAngle<8,8>(ref1, ref2, buf[8-2], buf[28-2]);

    PredAngle10<8>(PredPel, PredPel2, buf[10-2], buf[26-2]);

    ProjLine<8,12>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
    ProjLine<8,14>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
    ProjLine<8,16>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,16>(ref1, ref2, buf[16-2], buf[36-16-2]);

    // filtered
    PredAngle18<8>(FiltPel, FiltPel2, buf[18-2]);
}

static void PredictIntra_Ang_All_16x16_Even(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels)
{
    ALIGN_DECL(64) Ipp8u ref1[32];  // input for mode < 18
    ALIGN_DECL(64) Ipp8u ref2[32];  // input for mode > 18

    Ipp8u *PredPel2 = PredPel + 2 * 16;
    Ipp8u *FiltPel2 = FiltPel + 2 * 16;
    Ipp8u (*buf)[16*16] = (Ipp8u(*)[16*16])pels;

    // filtered
    CopyLine<16>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle2<16>(ref1, ref2, buf[2-2], buf[34-2]);
    PredAngle<16,4>(ref1, ref2, buf[4-2], buf[32-2]);
    PredAngle<16,6>(ref1, ref2, buf[6-2], buf[30-2]);
    PredAngle<16,8>(ref1, ref2, buf[8-2], buf[28-2]);

    // unfiltered
    CopyLine<16>(PredPel, PredPel2, ref1, ref2);

    PredAngle10<16>(PredPel, PredPel2, buf[10-2], buf[26-2]);

    // filtered
    ProjLine<16,12>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<16,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
    ProjLine<16,14>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<16,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
    ProjLine<16,16>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<16,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
    PredAngle18<16>(FiltPel, FiltPel2, buf[18-2]);
}

static void PredictIntra_Ang_All_32x32_Even(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels)
{
    ALIGN_DECL(64) Ipp8u ref1[64];  // input for mode < 18
    ALIGN_DECL(64) Ipp8u ref2[64];  // input for mode > 18

    Ipp8u *PredPel2 = PredPel + 2 * 32;
    Ipp8u *FiltPel2 = FiltPel + 2 * 32;
    Ipp8u (*buf)[32*32] = (Ipp8u(*)[32*32])pels;

    // filtered
    CopyLine<32>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle2<32>(ref1, ref2, buf[2-2], buf[34-2]);
    PredAngle<32,4>(ref1, ref2, buf[4-2], buf[32-2]);
    PredAngle<32,6>(ref1, ref2, buf[6-2], buf[30-2]);
    PredAngle<32,8>(ref1, ref2, buf[8-2], buf[28-2]);

    // unfiltered
    PredAngle10<32>(PredPel, PredPel2, buf[10-2], buf[26-2]);

    // filtered
    ProjLine<32,12>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<32,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
    ProjLine<32,14>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<32,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
    ProjLine<32,16>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<32,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
    PredAngle18<32>(FiltPel, FiltPel2, buf[18-2]);
}

static void PredictIntra_Ang_All_4x4(Ipp8u* PredPel, Ipp8u* /*FiltPel*/, Ipp8u* pels)
{
    ALIGN_DECL(64) Ipp8u ref1[64];  // input for mode < 18
    ALIGN_DECL(64) Ipp8u ref2[64];  // input for mode > 18

    Ipp8u *PredPel2 = PredPel + 2 * 4;
    //Ipp8u *FiltPel2 = FiltPel + 2 * 4;
    Ipp8u (*buf)[4*4] = (Ipp8u(*)[4*4])pels;

    // unfiltered
    CopyLine<4>(PredPel, PredPel2, ref1, ref2);
    PredAngle2<4>(ref1, ref2, buf[2-2], buf[34-2]);
    PredAngle<4,3>(ref1, ref2, buf[3-2], buf[33-2]);
    PredAngle<4,4>(ref1, ref2, buf[4-2], buf[32-2]);
    PredAngle<4,5>(ref1, ref2, buf[5-2], buf[31-2]);
    PredAngle<4,6>(ref1, ref2, buf[6-2], buf[30-2]);
    PredAngle<4,7>(ref1, ref2, buf[7-2], buf[29-2]);
    PredAngle<4,8>(ref1, ref2, buf[8-2], buf[28-2]);
    PredAngle<4,9>(ref1, ref2, buf[9-2], buf[27-2]);

    PredAngle10<4>(PredPel, PredPel2, buf[10-2], buf[26-2]);

    ProjLine<4,11>(PredPel, PredPel2, ref1, ref2);
    PredAngle<4,11>(ref1, ref2, buf[11-2], buf[36-11-2]);
    ProjLine<4,12>(PredPel, PredPel2, ref1, ref2);
    PredAngle<4,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
    ProjLine<4,13>(PredPel, PredPel2, ref1, ref2);
    PredAngle<4,13>(ref1, ref2, buf[13-2], buf[36-13-2]);
    ProjLine<4,14>(PredPel, PredPel2, ref1, ref2);
    PredAngle<4,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
    ProjLine<4,15>(PredPel, PredPel2, ref1, ref2);
    PredAngle<4,15>(ref1, ref2, buf[15-2], buf[36-15-2]);
    ProjLine<4,16>(PredPel, PredPel2, ref1, ref2);
    PredAngle<4,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
    ProjLine<4,17>(PredPel, PredPel2, ref1, ref2);
    PredAngle<4,17>(ref1, ref2, buf[17-2], buf[36-17-2]);
    PredAngle18<4>(PredPel, PredPel2, buf[18-2]);
}

static void PredictIntra_Ang_All_8x8(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels)
{
    ALIGN_DECL(64) Ipp8u ref1[16];  // input for mode < 18
    ALIGN_DECL(64) Ipp8u ref2[16];  // input for mode > 18

    Ipp8u *PredPel2 = PredPel + 2 * 8;
    Ipp8u *FiltPel2 = FiltPel + 2 * 8;
    Ipp8u (*buf)[8*8] = (Ipp8u(*)[8*8])pels;

    // filtered
    CopyLine<8>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle2<8>(ref1, ref2, buf[2-2], buf[34-2]);

    // unfiltered
    CopyLine<8>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,3>(ref1, ref2, buf[3-2], buf[33-2]);
    PredAngle<8,4>(ref1, ref2, buf[4-2], buf[32-2]);
    PredAngle<8,5>(ref1, ref2, buf[5-2], buf[31-2]);
    PredAngle<8,6>(ref1, ref2, buf[6-2], buf[30-2]);
    PredAngle<8,7>(ref1, ref2, buf[7-2], buf[29-2]);
    PredAngle<8,8>(ref1, ref2, buf[8-2], buf[28-2]);
    PredAngle<8,9>(ref1, ref2, buf[9-2], buf[27-2]);

    PredAngle10<8>(PredPel, PredPel2, buf[10-2], buf[26-2]);

    ProjLine<8,11>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,11>(ref1, ref2, buf[11-2], buf[36-11-2]);
    ProjLine<8,12>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
    ProjLine<8,13>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,13>(ref1, ref2, buf[13-2], buf[36-13-2]);
    ProjLine<8,14>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
    ProjLine<8,15>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,15>(ref1, ref2, buf[15-2], buf[36-15-2]);
    ProjLine<8,16>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
    ProjLine<8,17>(PredPel, PredPel2, ref1, ref2);
    PredAngle<8,17>(ref1, ref2, buf[17-2], buf[36-17-2]);

    // filtered
    PredAngle18<8>(FiltPel, FiltPel2, buf[18-2]);
}

static void PredictIntra_Ang_All_16x16(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels)
{
    ALIGN_DECL(64) Ipp8u ref1[32];  // input for mode < 18
    ALIGN_DECL(64) Ipp8u ref2[32];  // input for mode > 18

    Ipp8u *PredPel2 = PredPel + 2 * 16;
    Ipp8u *FiltPel2 = FiltPel + 2 * 16;
    Ipp8u (*buf)[16*16] = (Ipp8u(*)[16*16])pels;

    // filtered
    CopyLine<16>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle2<16>(ref1, ref2, buf[2-2], buf[34-2]);
    PredAngle<16,3>(ref1, ref2, buf[3-2], buf[33-2]);
    PredAngle<16,4>(ref1, ref2, buf[4-2], buf[32-2]);
    PredAngle<16,5>(ref1, ref2, buf[5-2], buf[31-2]);
    PredAngle<16,6>(ref1, ref2, buf[6-2], buf[30-2]);
    PredAngle<16,7>(ref1, ref2, buf[7-2], buf[29-2]);
    PredAngle<16,8>(ref1, ref2, buf[8-2], buf[28-2]);

    // unfiltered
    CopyLine<16>(PredPel, PredPel2, ref1, ref2);
    PredAngle<16,9>(ref1, ref2, buf[9-2], buf[27-2]);

    PredAngle10<16>(PredPel, PredPel2, buf[10-2], buf[26-2]);

    ProjLine<16,11>(PredPel, PredPel2, ref1, ref2);
    PredAngle<16,11>(ref1, ref2, buf[11-2], buf[36-11-2]);

    // filtered
    ProjLine<16,12>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<16,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
    ProjLine<16,13>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<16,13>(ref1, ref2, buf[13-2], buf[36-13-2]);
    ProjLine<16,14>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<16,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
    ProjLine<16,15>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<16,15>(ref1, ref2, buf[15-2], buf[36-15-2]);
    ProjLine<16,16>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<16,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
    ProjLine<16,17>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<16,17>(ref1, ref2, buf[17-2], buf[36-17-2]);
    PredAngle18<16>(FiltPel, FiltPel2, buf[18-2]);
}

static void PredictIntra_Ang_All_32x32(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels)
{
    ALIGN_DECL(64) Ipp8u ref1[64];  // input for mode < 18
    ALIGN_DECL(64) Ipp8u ref2[64];  // input for mode > 18

    Ipp8u *PredPel2 = PredPel + 2 * 32;
    Ipp8u *FiltPel2 = FiltPel + 2 * 32;
    Ipp8u (*buf)[32*32] = (Ipp8u(*)[32*32])pels;

    // filtered
    CopyLine<32>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle2<32>(ref1, ref2, buf[2-2], buf[34-2]);
    PredAngle<32,3>(ref1, ref2, buf[3-2], buf[33-2]);
    PredAngle<32,4>(ref1, ref2, buf[4-2], buf[32-2]);
    PredAngle<32,5>(ref1, ref2, buf[5-2], buf[31-2]);
    PredAngle<32,6>(ref1, ref2, buf[6-2], buf[30-2]);
    PredAngle<32,7>(ref1, ref2, buf[7-2], buf[29-2]);
    PredAngle<32,8>(ref1, ref2, buf[8-2], buf[28-2]);
    PredAngle<32,9>(ref1, ref2, buf[9-2], buf[27-2]);

    // unfiltered
    PredAngle10<32>(PredPel, PredPel2, buf[10-2], buf[26-2]);

    // filtered
    ProjLine<32,11>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<32,11>(ref1, ref2, buf[11-2], buf[36-11-2]);
    ProjLine<32,12>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<32,12>(ref1, ref2, buf[12-2], buf[36-12-2]);
    ProjLine<32,13>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<32,13>(ref1, ref2, buf[13-2], buf[36-13-2]);
    ProjLine<32,14>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<32,14>(ref1, ref2, buf[14-2], buf[36-14-2]);
    ProjLine<32,15>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<32,15>(ref1, ref2, buf[15-2], buf[36-15-2]);
    ProjLine<32,16>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<32,16>(ref1, ref2, buf[16-2], buf[36-16-2]);
    ProjLine<32,17>(FiltPel, FiltPel2, ref1, ref2);
    PredAngle<32,17>(ref1, ref2, buf[17-2], buf[36-17-2]);
    PredAngle18<32>(FiltPel, FiltPel2, buf[18-2]);
}

//
// pels should be aligned, and large enough to hold pels[33][32*32]
//
void MAKE_NAME(h265_PredictIntra_Ang_All_8u) (
    Ipp8u* PredPel,
    Ipp8u* FiltPel,
    Ipp8u* pels,
    Ipp32s width)
{
    _mm256_zeroupper();

    switch (width) {
    case 4:
        PredictIntra_Ang_All_4x4(PredPel, FiltPel, pels);
        break;
    case 8:
        PredictIntra_Ang_All_8x8(PredPel, FiltPel, pels);
        break;
    case 16:
        PredictIntra_Ang_All_16x16(PredPel, FiltPel, pels);
        break;
    case 32:
        PredictIntra_Ang_All_32x32(PredPel, FiltPel, pels);
        break;
    }
}

void MAKE_NAME(h265_PredictIntra_Ang_All_Even_8u) (
    Ipp8u* PredPel,
    Ipp8u* FiltPel,
    Ipp8u* pels,
    Ipp32s width)
{
    _mm256_zeroupper();

    switch (width) {
    case 4:
        PredictIntra_Ang_All_4x4_Even(PredPel, FiltPel, pels);
        break;
    case 8:
        PredictIntra_Ang_All_8x8_Even(PredPel, FiltPel, pels);
        break;
    case 16:
        PredictIntra_Ang_All_16x16_Even(PredPel, FiltPel, pels);
        break;
    case 32:
        PredictIntra_Ang_All_32x32_Even(PredPel, FiltPel, pels);
        break;
    }
}

ALIGN_DECL(32) static const char fppShufTab[3][32] = {
    { 0, -1, 1, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1, 0, -1, 1, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1 },
    { 1, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1, 8, -1, 1, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1, 8, -1 },
    { 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1, 8, -1, 9, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1, 8, -1, 9, -1 },
};

/* assume PredPel buffer size is at least (4*width + 32) bytes to allow 32-byte loads (see calling code)
 * only the data in range [0, 4*width] is actually used
 * supported widths = 4, 8, 16, 32 (but should not require 4 - see spec)
 */
void MAKE_NAME(h265_FilterPredictPels_8u)(Ipp8u* PredPel, Ipp32s width)
{
    Ipp8u t0, t1, t2, t3;
    Ipp8u *p0;
    Ipp32s i;
    __m256i ymm0, ymm1, ymm2, ymm7;

    assert(width == 4 || width == 8 || width == 16 || width == 32);

    _mm256_zeroupper();

    /* precalcuate boundary cases (0, 2*w, 2*w+1, 4*w) to allow compact kernel, write back when finished */
    t0 = (PredPel[1] + 2 * PredPel[0] + PredPel[2*width+1] + 2) >> 2;
    t1 = PredPel[2*width];
    t2 = (PredPel[0] + 2 * PredPel[2*width+1] + PredPel[2*width+2] + 2) >> 2;
    t3 = PredPel[4*width];

    i = 4*width;
    p0 = PredPel;
    ymm2 = _mm256_loadu_si256((__m256i *)p0);
    ymm7 = _mm256_set1_epi16(2);
    do {
        /* shuffle and zero-extend to 16 bits */
        ymm2 = _mm256_permute4x64_epi64(ymm2, 0x94);
        ymm0 = _mm256_shuffle_epi8(ymm2, *(__m256i *)fppShufTab[0]);
        ymm1 = _mm256_shuffle_epi8(ymm2, *(__m256i *)fppShufTab[1]);
        ymm2 = _mm256_shuffle_epi8(ymm2, *(__m256i *)fppShufTab[2]);

        /* y[i] = (x[i-1] + 2*x[i] + x[i+1] + 2) >> 2 */
        ymm0 = _mm256_add_epi16(ymm0, ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm2);
        ymm0 = _mm256_add_epi16(ymm0, ymm7);
        ymm0 = _mm256_srli_epi16(ymm0, 2);
        ymm0 = _mm256_packus_epi16(ymm0, ymm0);

        /* read 32 bytes (16 new) before writing */
        ymm2 = _mm256_loadu_si256((__m256i *)(p0 + 16));
        ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
        _mm_storeu_si128((__m128i *)(p0 + 1), mm128(ymm0));

        p0 += 16;
        i -= 16;
    } while (i > 0);

    PredPel[0*width+0] = t0;
    PredPel[2*width+0] = t1;
    PredPel[2*width+1] = t2;
    PredPel[4*width+0] = t3;
}

/* width always = 32 (see spec) */
void MAKE_NAME(h265_FilterPredictPels_Bilinear_8u) (
    Ipp8u* pSrcDst,
    int width,
    int topLeft,
    int bottomLeft,
    int topRight)
{
    int x;
    Ipp8u *p0;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    _mm256_zeroupper();

    /* see calling code - if any of this changes, code would need to be rewritten */
    assert(width == 32 && pSrcDst[0] == topLeft && pSrcDst[2*width] == topRight && pSrcDst[4*width] == bottomLeft);

    p0 = pSrcDst;
    if (topLeft == 128 && topRight == 128 && bottomLeft == 128) {
        /* fast path: set 128 consecutive pixels to 128 */
        ymm0 = _mm256_set1_epi8(128);
        _mm256_storeu_si256((__m256i *)(p0 +  0), ymm0);
        _mm256_storeu_si256((__m256i *)(p0 + 32), ymm0);
        _mm256_storeu_si256((__m256i *)(p0 + 64), ymm0);
        _mm256_storeu_si256((__m256i *)(p0 + 96), ymm0);
    } else {
        /* calculate 16 at a time with successive addition 
         * p[x] = ( ((64-x)*TL + x*TR + 32) >> 6 ) = ( ((64*TL + 32) + x*(TR - TL)) >> 6 )
         * similar for p[x+64]
         */
        ymm0 = _mm256_set1_epi16(64*topLeft + 32);
        ymm1 = ymm0;

        ymm2 = _mm256_set1_epi16(topRight - topLeft);
        ymm3 = _mm256_set1_epi16(bottomLeft - topLeft);
        ymm4 = _mm256_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
            
        ymm6 = _mm256_slli_epi16(ymm2, 4);          /* 16*(TR-TL) */
        ymm7 = _mm256_slli_epi16(ymm3, 4);          /* 16*(BL-TL) */

        ymm2 = _mm256_mullo_epi16(ymm2, ymm4);      /* [0*(TR-TL) 1*(TR-TL) ... ] */
        ymm3 = _mm256_mullo_epi16(ymm3, ymm4);      /* [0*(BL-TL) 1*(BL-TL) ... ] */

        ymm0 = _mm256_add_epi16(ymm0, ymm2);
        ymm1 = _mm256_add_epi16(ymm1, ymm3);

        for(x = 0; x < 64; x += 32) {
            /* calculate 2 sets of 16 pixels for each direction */
            ymm2 = _mm256_srli_epi16(ymm0, 6);
            ymm3 = _mm256_srli_epi16(ymm1, 6);
            ymm0 = _mm256_add_epi16(ymm0, ymm6);
            ymm1 = _mm256_add_epi16(ymm1, ymm7);

            ymm4 = _mm256_srli_epi16(ymm0, 6);
            ymm5 = _mm256_srli_epi16(ymm1, 6);
            ymm0 = _mm256_add_epi16(ymm0, ymm6);
            ymm1 = _mm256_add_epi16(ymm1, ymm7);

            /* clip to 8 bits, write 32 pixels for each direction */
            ymm2 = _mm256_packus_epi16(ymm2, ymm4);
            ymm3 = _mm256_packus_epi16(ymm3, ymm5);
            _mm256_storeu_si256((__m256i *)(p0 +  0), _mm256_permute4x64_epi64(ymm2, 0xd8));
            _mm256_storeu_si256((__m256i *)(p0 + 64), _mm256_permute4x64_epi64(ymm3, 0xd8));

            p0 += 32;
        }
        pSrcDst[64] = topRight;
    }
}

/* assume PredPel buffer size is at least (4*width + 32) bytes to allow 32-byte loads (see calling code)
 * only the data in range [0, 4*width] is actually used
 * supported widths = 4, 8, 16, 32 (but should not require 4 - see spec)
 */
void MAKE_NAME(h265_FilterPredictPels_16s)(Ipp16s* PredPel, Ipp32s width)
{
    Ipp16s t0, t1, t2, t3;
    Ipp16s *p0;
    Ipp32s i;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm7;

    assert(width == 4 || width == 8 || width == 16 || width == 32);

    _mm256_zeroupper();

    /* precalcuate boundary cases (0, 2*w, 2*w+1, 4*w) to allow compact kernel, write back when finished */
    t0 = (PredPel[1] + 2 * PredPel[0] + PredPel[2*width+1] + 2) >> 2;
    t1 = PredPel[2*width];
    t2 = (PredPel[0] + 2 * PredPel[2*width+1] + PredPel[2*width+2] + 2) >> 2;
    t3 = PredPel[4*width];

    i = 4*width;
    p0 = PredPel;
    ymm0 = _mm256_loadu_si256((__m256i *)(p0 + 0));
    ymm7 = _mm256_set1_epi16(2);
    do {
        /* load pixels and shift into 3 registers */
        ymm3 = _mm256_loadu_si256((__m256i *)(p0 + 8));
        ymm1 = _mm256_alignr_epi8(ymm3, ymm0, 2*1);
        ymm2 = _mm256_alignr_epi8(ymm3, ymm0, 2*2);

        /* y[i] = (x[i-1] + 2*x[i] + x[i+1] + 2) >> 2 */
        ymm1 = _mm256_add_epi16(ymm1, ymm1);
        ymm1 = _mm256_add_epi16(ymm1, ymm0);
        ymm1 = _mm256_add_epi16(ymm1, ymm2);
        ymm1 = _mm256_add_epi16(ymm1, ymm7);
        ymm1 = _mm256_srli_epi16(ymm1, 2);

        /* write 16 output pixels (1-16), load new data first (overwrites last pixel) */
        ymm0 = _mm256_loadu_si256((__m256i *)(p0 + 16));
        _mm256_storeu_si256((__m256i *)(p0 + 1), ymm1);

        p0 += 16;
        i -= 16;
    } while (i > 0);

    PredPel[0*width+0] = t0;
    PredPel[2*width+0] = t1;
    PredPel[2*width+1] = t2;
    PredPel[4*width+0] = t3;
}

/* width always = 32 (see spec) */
void MAKE_NAME(h265_FilterPredictPels_Bilinear_16s) (
    Ipp16s* pSrcDst,
    int width,
    int topLeft,
    int bottomLeft,
    int topRight)
{
    int i, x;
    Ipp16s *p0;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    _mm256_zeroupper();

    /* see calling code - if any of this changes, code would need to be rewritten */
    assert(width == 32 && pSrcDst[0] == topLeft && pSrcDst[2*width] == topRight && pSrcDst[4*width] == bottomLeft);

    p0 = pSrcDst;
    if (topLeft == 128 && topRight == 128 && bottomLeft == 128) {
        /* fast path: set 128 consecutive pixels to 128 (verify that compiler unrolls fully) */
        ymm0 = _mm256_set1_epi16(128);
        for (i = 0; i < 128; i += 16)
            _mm256_storeu_si256((__m256i *)(p0 + i), ymm0);
    } else {
        /* calculate 16 at a time with successive addition 
         * p[x] = ( ((64-x)*TL + x*TR + 32) >> 6 ) = ( ((64*TL + 32) + x*(TR - TL)) >> 6 )
         * similar for p[x+64]
         * for 10-bit input, max intermediate value (e.g. 64*topLeft+32) just fits in 16 bits (treat as unsigned)
         * successive addition/subtraction is correct with 2's complement math
         */
        ymm0 = _mm256_set1_epi16(64*topLeft + 32);
        ymm1 = ymm0;

        ymm2 = _mm256_set1_epi16(topRight - topLeft);
        ymm3 = _mm256_set1_epi16(bottomLeft - topLeft);
        ymm4 = _mm256_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
            
        ymm6 = _mm256_slli_epi16(ymm2, 4);          /* 16*(TR-TL) */
        ymm7 = _mm256_slli_epi16(ymm3, 4);          /* 16*(BL-TL) */

        ymm2 = _mm256_mullo_epi16(ymm2, ymm4);      /* [0*(TR-TL) 1*(TR-TL) ... ] */
        ymm3 = _mm256_mullo_epi16(ymm3, ymm4);      /* [0*(BL-TL) 1*(BL-TL) ... ] */

        ymm0 = _mm256_add_epi16(ymm0, ymm2);
        ymm1 = _mm256_add_epi16(ymm1, ymm3);

        for(x = 0; x < 64; x += 32) {
            /* calculate 2 sets of 16 pixels for each direction */
            ymm2 = _mm256_srli_epi16(ymm0, 6);
            ymm3 = _mm256_srli_epi16(ymm1, 6);
            ymm0 = _mm256_add_epi16(ymm0, ymm6);
            ymm1 = _mm256_add_epi16(ymm1, ymm7);

            ymm4 = _mm256_srli_epi16(ymm0, 6);
            ymm5 = _mm256_srli_epi16(ymm1, 6);
            ymm0 = _mm256_add_epi16(ymm0, ymm6);
            ymm1 = _mm256_add_epi16(ymm1, ymm7);

            /* write 32 pixels for each direction */
            _mm256_storeu_si256((__m256i *)(p0 +  0), ymm2);
            _mm256_storeu_si256((__m256i *)(p0 + 16), ymm4);
            _mm256_storeu_si256((__m256i *)(p0 + 64), ymm3);
            _mm256_storeu_si256((__m256i *)(p0 + 80), ymm5);

            p0 += 32;
        }
        pSrcDst[64] = topRight;
    }
}

/* optimized kernel with const width and shift */
template <Ipp32s width, Ipp32s shift>
static void h265_PredictIntra_Planar_8u_Kernel(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch)
{
    ALIGN_DECL(32) Ipp16s leftColumn[32], horInc[32];
    Ipp32s row, col;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    _mm256_zeroupper();

    if (width == 4) {
        /* broadcast scalar values */
        xmm7 = _mm_set1_epi16(width);
        xmm6 = _mm_set1_epi16(PredPel[1+width]);
        xmm1 = _mm_set1_epi16(PredPel[3*width+1]);

        /* xmm6 = horInc = topRight - leftColumn, xmm5 = leftColumn = (PredPel[2*width+1+i] << shift) + width */
        xmm5 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*(int *)(&PredPel[2*width+1])));
        xmm6 = _mm_sub_epi16(xmm6, xmm5);
        xmm5 = _mm_slli_epi16(xmm5, shift);
        xmm5 = _mm_add_epi16(xmm5, xmm7);

        /* xmm2 = verInc = bottomLeft - topRow, xmm0 = topRow = (PredPel[1+i] << shift) */
        xmm0 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*(int *)(&PredPel[1])));
        xmm2 = _mm_sub_epi16(xmm1, xmm0);
        xmm0 = _mm_slli_epi16(xmm0, shift);
        xmm7 = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);

        /* generate prediction signal one row at a time */
        for (row = 0; row < width; row++) {
            /* calculate horizontal component */
            xmm3 = _mm_shufflelo_epi16(xmm5, 0);
            xmm4 = _mm_shufflelo_epi16(xmm6, 0);
            xmm4 = _mm_mullo_epi16(xmm4, xmm7);
            xmm3 = _mm_add_epi16(xmm3, xmm4);

            /* add vertical component, scale and pack to 8 bits */
            xmm0 = _mm_add_epi16(xmm0, xmm2);
            xmm3 = _mm_add_epi16(xmm3, xmm0);
            xmm3 = _mm_srli_epi16(xmm3, shift+1);
            xmm3 = _mm_packus_epi16(xmm3, xmm3);

            /* shift in (leftColumn[j] + width) for next row */
            xmm5 = _mm_srli_si128(xmm5, 2);
            xmm6 = _mm_srli_si128(xmm6, 2);

            /* store 4 8-bit pixels */
            *(int *)(&pels[row*pitch]) = _mm_cvtsi128_si32(xmm3);
        }
    } else if (width == 8) {
        /* broadcast scalar values */
        xmm7 = _mm_set1_epi16(width);
        xmm6 = _mm_set1_epi16(PredPel[1+width]);
        xmm1 = _mm_set1_epi16(PredPel[3*width+1]);

        /* 8x8 block (see comments for 4x4 case) */
        xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&PredPel[2*width+1]));
        xmm6 = _mm_sub_epi16(xmm6, xmm5);
        xmm5 = _mm_slli_epi16(xmm5, shift);
        xmm5 = _mm_add_epi16(xmm5, xmm7);

        xmm0 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&PredPel[1]));
        xmm2 = _mm_sub_epi16(xmm1, xmm0);
        xmm0 = _mm_slli_epi16(xmm0, shift);
        xmm7 = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);

        for (row = 0; row < width; row++) {
            /* generate 8 pixels per row */
            xmm3 = _mm_shufflelo_epi16(xmm5, 0);
            xmm4 = _mm_shufflelo_epi16(xmm6, 0);
            xmm3 = _mm_unpacklo_epi64(xmm3, xmm3);
            xmm4 = _mm_unpacklo_epi64(xmm4, xmm4);
            xmm4 = _mm_mullo_epi16(xmm4, xmm7);
            xmm3 = _mm_add_epi16(xmm3, xmm4);

            xmm0 = _mm_add_epi16(xmm0, xmm2);
            xmm3 = _mm_add_epi16(xmm3, xmm0);
            xmm3 = _mm_srli_epi16(xmm3, shift+1);
            xmm3 = _mm_packus_epi16(xmm3, xmm3);

            xmm5 = _mm_srli_si128(xmm5, 2);
            xmm6 = _mm_srli_si128(xmm6, 2);

            /* store 8 8-bit pixels */
            _mm_storel_epi64((__m128i *)(&pels[row*pitch]), xmm3);
        }
    } else if (width == 16) {
        /* broadcast scalar values */
        ymm7 = _mm256_set1_epi16(width);
        ymm6 = _mm256_set1_epi16(PredPel[1+width]);
        ymm1 = _mm256_set1_epi16(PredPel[3*width+1]);

        /* load 16 8-bit pixels, zero extend to 16 bits */
        ymm5 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&PredPel[2*width+1])));
        ymm6 = _mm256_sub_epi16(ymm6, ymm5);
        ymm5 = _mm256_slli_epi16(ymm5, shift);
        ymm5 = _mm256_add_epi16(ymm5, ymm7);

        /* store in aligned temp buffer */
        _mm256_store_si256((__m256i*)(leftColumn), ymm5);
        _mm256_store_si256((__m256i*)(horInc), ymm6);

        ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&PredPel[1])));
        ymm2 = _mm256_sub_epi16(ymm1, ymm0);
        ymm0 = _mm256_slli_epi16(ymm0, shift);
        ymm7 = _mm256_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

        for (row = 0; row < width; row++) {
            /* generate 16 pixels per row */
            ymm3 = _mm256_set1_epi16(leftColumn[row]);
            ymm4 = _mm256_set1_epi16(horInc[row]);
            ymm4 = _mm256_mullo_epi16(ymm4, ymm7);
            ymm3 = _mm256_add_epi16(ymm3, ymm4);

            ymm0 = _mm256_add_epi16(ymm0, ymm2);
            ymm3 = _mm256_add_epi16(ymm3, ymm0);
            ymm3 = _mm256_srli_epi16(ymm3, shift+1);
            ymm3 = _mm256_packus_epi16(ymm3, ymm3);

            /* store 16 8-bit pixels */
            ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
            _mm_store_si128((__m128i *)(&pels[row*pitch]), mm128(ymm3));
        }
    } else if (width == 32) {
        /* broadcast scalar values */
        ymm7 = _mm256_set1_epi16(width);
        ymm6 = _mm256_set1_epi16(PredPel[1+width]);
        ymm1 = _mm256_set1_epi16(PredPel[3*width+1]);

        for (col = 0; col < width; col += 16) {
            /* load 16 8-bit pixels, zero extend to 16 bits */
            ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&PredPel[2*width+1+col])));
            ymm2 = _mm256_sub_epi16(ymm6, ymm0);
            ymm0 = _mm256_slli_epi16(ymm0, shift);
            ymm0 = _mm256_add_epi16(ymm0, ymm7);

            /* store in aligned temp buffer */
            _mm256_store_si256((__m256i*)(horInc + col), ymm2);
            _mm256_store_si256((__m256i*)(leftColumn + col), ymm0);
        }
        ymm7 = _mm256_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

        /* generate 8 pixels per row, repeat 2 times (width = 32) */
        for (col = 0; col < width; col += 16) {
            ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&PredPel[1+col])));
            ymm2 = _mm256_sub_epi16(ymm1, ymm0);
            ymm0 = _mm256_slli_epi16(ymm0, shift);

            for (row = 0; row < width; row++) {
                /* generate 16 pixels per row */
                ymm3 = _mm256_set1_epi16(leftColumn[row]);
                ymm4 = _mm256_set1_epi16(horInc[row]);
                ymm4 = _mm256_mullo_epi16(ymm4, ymm7);
                ymm3 = _mm256_add_epi16(ymm3, ymm4);

                ymm0 = _mm256_add_epi16(ymm0, ymm2);
                ymm3 = _mm256_add_epi16(ymm3, ymm0);
                ymm3 = _mm256_srli_epi16(ymm3, shift+1);
                ymm3 = _mm256_packus_epi16(ymm3, ymm3);

                /* store 16 8-bit pixels */
                ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
                _mm_store_si128((__m128i *)(&pels[row*pitch+col]), mm128(ymm3));
            }
            /* add 16 to each offset for next 16 columns */
            ymm7 = _mm256_add_epi16(ymm7, _mm256_set1_epi16(16));
        }
    }
}

/* planar intra prediction for 4x4, 8x8, 16x16, and 32x32 blocks (with arbitrary pitch) */
void MAKE_NAME(h265_PredictIntra_Planar_8u)(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width)
{
    assert(width == 4 || width == 8 || width == 16 || width == 32);

    switch (width) {
    case 4:
        h265_PredictIntra_Planar_8u_Kernel< 4, 2>(PredPel, pels, pitch);
        break;
    case 8:
        h265_PredictIntra_Planar_8u_Kernel< 8, 3>(PredPel, pels, pitch);
        break;
    case 16:
        h265_PredictIntra_Planar_8u_Kernel<16, 4>(PredPel, pels, pitch);
        break;
    case 32:
        h265_PredictIntra_Planar_8u_Kernel<32, 5>(PredPel, pels, pitch);
        break;
    }
}

/* optimized kernel with const width and shift (16-bit version) */
template <Ipp32s width, Ipp32s shift>
static void h265_PredictIntra_Planar_16s_Kernel(Ipp16s* PredPel, Ipp16s* pels, Ipp32s pitch)
{
    ALIGN_DECL(32) Ipp16s leftColumn[32], horInc[32];
    Ipp32s row, col;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    _mm256_zeroupper();

    if (width == 4) {
        /* broadcast scalar values */
        xmm7 = _mm_set1_epi16(width);
        xmm6 = _mm_set1_epi16(PredPel[1+width]);
        xmm1 = _mm_set1_epi16(PredPel[3*width+1]);

        /* xmm6 = horInc = topRight - leftColumn, xmm5 = leftColumn = (PredPel[2*width+1+i] << shift) + width */
        xmm5 = _mm_loadl_epi64((__m128i *)(&PredPel[2*width+1]));
        xmm6 = _mm_sub_epi16(xmm6, xmm5);
        xmm5 = _mm_slli_epi16(xmm5, shift);
        xmm5 = _mm_add_epi16(xmm5, xmm7);

        /* xmm2 = verInc = bottomLeft - topRow, xmm0 = topRow = (PredPel[1+i] << shift) */
        xmm0 = _mm_loadl_epi64((__m128i *)(&PredPel[1]));
        xmm2 = _mm_sub_epi16(xmm1, xmm0);
        xmm0 = _mm_slli_epi16(xmm0, shift);
        xmm7 = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);

        /* generate prediction signal one row at a time */
        for (row = 0; row < width; row++) {
            /* calculate horizontal component */
            xmm3 = _mm_shufflelo_epi16(xmm5, 0);
            xmm4 = _mm_shufflelo_epi16(xmm6, 0);
            xmm4 = _mm_mullo_epi16(xmm4, xmm7);
            xmm3 = _mm_add_epi16(xmm3, xmm4);

            /* add vertical component, scale and pack to 8 bits */
            xmm0 = _mm_add_epi16(xmm0, xmm2);
            xmm3 = _mm_add_epi16(xmm3, xmm0);
            xmm3 = _mm_srli_epi16(xmm3, shift+1);

            /* shift in (leftColumn[j] + width) for next row */
            xmm5 = _mm_srli_si128(xmm5, 2);
            xmm6 = _mm_srli_si128(xmm6, 2);

            /* store 4 16-bit pixels */
            _mm_storel_epi64((__m128i *)(&pels[row*pitch]), xmm3);
        }
    } else if (width == 8) {
        /* broadcast scalar values */
        xmm7 = _mm_set1_epi16(width);
        xmm6 = _mm_set1_epi16(PredPel[1+width]);
        xmm1 = _mm_set1_epi16(PredPel[3*width+1]);

        /* 8x8 block (see comments for 4x4 case) */
        xmm5 = _mm_loadu_si128((__m128i *)(&PredPel[2*width+1]));
        xmm6 = _mm_sub_epi16(xmm6, xmm5);
        xmm5 = _mm_slli_epi16(xmm5, shift);
        xmm5 = _mm_add_epi16(xmm5, xmm7);

        xmm0 = _mm_loadu_si128((__m128i *)(&PredPel[1]));
        xmm2 = _mm_sub_epi16(xmm1, xmm0);
        xmm0 = _mm_slli_epi16(xmm0, shift);
        xmm7 = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);

        for (row = 0; row < width; row++) {
            /* generate 8 pixels per row */
            xmm3 = _mm_shufflelo_epi16(xmm5, 0);
            xmm4 = _mm_shufflelo_epi16(xmm6, 0);
            xmm3 = _mm_unpacklo_epi64(xmm3, xmm3);
            xmm4 = _mm_unpacklo_epi64(xmm4, xmm4);
            xmm4 = _mm_mullo_epi16(xmm4, xmm7);
            xmm3 = _mm_add_epi16(xmm3, xmm4);

            xmm0 = _mm_add_epi16(xmm0, xmm2);
            xmm3 = _mm_add_epi16(xmm3, xmm0);
            xmm3 = _mm_srli_epi16(xmm3, shift+1);

            xmm5 = _mm_srli_si128(xmm5, 2);
            xmm6 = _mm_srli_si128(xmm6, 2);

            /* store 8 16-bit pixels */
            _mm_storeu_si128((__m128i *)(&pels[row*pitch]), xmm3);
        }
    } else if (width == 16) {
        /* broadcast scalar values */
        ymm7 = _mm256_set1_epi16(width);
        ymm6 = _mm256_set1_epi16(PredPel[1+width]);
        ymm1 = _mm256_set1_epi16(PredPel[3*width+1]);

        /* load 16 16-bit pixels */
        ymm5 = _mm256_loadu_si256((__m256i *)(&PredPel[2*width+1]));
        ymm6 = _mm256_sub_epi16(ymm6, ymm5);
        ymm5 = _mm256_slli_epi16(ymm5, shift);
        ymm5 = _mm256_add_epi16(ymm5, ymm7);

        /* store in aligned temp buffer */
        _mm256_store_si256((__m256i*)(leftColumn), ymm5);
        _mm256_store_si256((__m256i*)(horInc), ymm6);

        ymm0 = _mm256_loadu_si256((__m256i *)(&PredPel[1]));
        ymm2 = _mm256_sub_epi16(ymm1, ymm0);
        ymm0 = _mm256_slli_epi16(ymm0, shift);
        ymm7 = _mm256_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

        for (row = 0; row < width; row++) {
            /* generate 16 pixels per row */
            ymm3 = _mm256_set1_epi16(leftColumn[row]);
            ymm4 = _mm256_set1_epi16(horInc[row]);
            ymm4 = _mm256_mullo_epi16(ymm4, ymm7);
            ymm3 = _mm256_add_epi16(ymm3, ymm4);

            ymm0 = _mm256_add_epi16(ymm0, ymm2);
            ymm3 = _mm256_add_epi16(ymm3, ymm0);
            ymm3 = _mm256_srli_epi16(ymm3, shift+1);

            /* store 16 16-bit pixels */
            _mm256_storeu_si256((__m256i *)(&pels[row*pitch]), ymm3);
        }
    } else if (width == 32) {
        /* broadcast scalar values */
        ymm7 = _mm256_set1_epi16(width);
        ymm6 = _mm256_set1_epi16(PredPel[1+width]);
        ymm1 = _mm256_set1_epi16(PredPel[3*width+1]);

        for (col = 0; col < width; col += 16) {
            /* load 16 16-bit pixels */
            ymm0 = _mm256_loadu_si256((__m256i *)(&PredPel[2*width+1+col]));
            ymm2 = _mm256_sub_epi16(ymm6, ymm0);
            ymm0 = _mm256_slli_epi16(ymm0, shift);
            ymm0 = _mm256_add_epi16(ymm0, ymm7);

            /* store in aligned temp buffer */
            _mm256_store_si256((__m256i*)(horInc + col), ymm2);
            _mm256_store_si256((__m256i*)(leftColumn + col), ymm0);
        }
        ymm7 = _mm256_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

        /* generate 16 pixels per row, repeat 2 times (width = 32) */
        for (col = 0; col < width; col += 16) {
            ymm0 = _mm256_loadu_si256((__m256i *)(&PredPel[1+col]));
            ymm2 = _mm256_sub_epi16(ymm1, ymm0);
            ymm0 = _mm256_slli_epi16(ymm0, shift);

            for (row = 0; row < width; row++) {
                /* generate 16 pixels per row */
                ymm3 = _mm256_set1_epi16(leftColumn[row]);
                ymm4 = _mm256_set1_epi16(horInc[row]);
                ymm4 = _mm256_mullo_epi16(ymm4, ymm7);
                ymm3 = _mm256_add_epi16(ymm3, ymm4);

                ymm0 = _mm256_add_epi16(ymm0, ymm2);
                ymm3 = _mm256_add_epi16(ymm3, ymm0);
                ymm3 = _mm256_srli_epi16(ymm3, shift+1);

                /* store 16 8-bit pixels */
                _mm256_storeu_si256((__m256i *)(&pels[row*pitch+col]), ymm3);
            }
            /* add 16 to each offset for next 16 columns */
            ymm7 = _mm256_add_epi16(ymm7, _mm256_set1_epi16(16));
        }
    }
}

/* planar intra prediction for 4x4, 8x8, 16x16, and 32x32 blocks (with arbitrary pitch) */
void MAKE_NAME(h265_PredictIntra_Planar_16s)(Ipp16s* PredPel, Ipp16s* pels, Ipp32s pitch, Ipp32s width)
{
    assert(width == 4 || width == 8 || width == 16 || width == 32);

    switch (width) {
    case 4:
        h265_PredictIntra_Planar_16s_Kernel< 4, 2>(PredPel, pels, pitch);
        break;
    case 8:
        h265_PredictIntra_Planar_16s_Kernel< 8, 3>(PredPel, pels, pitch);
        break;
    case 16:
        h265_PredictIntra_Planar_16s_Kernel<16, 4>(PredPel, pels, pitch);
        break;
    case 32:
        h265_PredictIntra_Planar_16s_Kernel<32, 5>(PredPel, pels, pitch);
        break;
    }
}

namespace PredictIntra_Planar_ChromaNV12_8u_avx2_details {
    template <Ipp32s W, Ipp32s log2w> void Impl(Ipp8u* predPel, Ipp8u* dst, Ipp32s dstStride)
    {
        ALIGN_DECL(32) Ipp32u storedLeftByWplusW[32], storedTopRightMinusLeft[32];

        __m256i width = _mm256_set1_epi16(W);
        __m256i topRight = _mm256_set1_epi32(predPel[2*W+2] + (predPel[2*W+3] << 16));
        __m256i botLeft  = _mm256_set1_epi32(predPel[6*W+2] + (predPel[6*W+3] << 16));

        for (Ipp32s y = 0; y < W; y += 8) {
            __m256i left = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(predPel+4*W+2+2*y)));
            __m256i topRightMinusLeft = _mm256_sub_epi16(topRight, left); // TopRight - Left[y]
            __m256i leftByWplusW = _mm256_add_epi16(_mm256_slli_epi16(left, log2w), width); // Width * Left[y] + Width

            _mm256_store_si256((__m256i*)(storedTopRightMinusLeft + y), topRightMinusLeft);
            _mm256_store_si256((__m256i*)(storedLeftByWplusW + y), leftByWplusW);
        }

        __m256i seq8 = _mm256_setr_epi16(1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8); // x + 1
        for (Ipp32s x = 0; x < W; x += 8) {
            __m256i top  = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(predPel+2+2*x)));
            __m256i botLeftMinusTop = _mm256_sub_epi16(botLeft, top); // BottomLeft - Top[x]
            __m256i acc = _mm256_slli_epi16(top, log2w); // Width * Top[x]
            for (Ipp32s y = 0; y < W; y++) {
                __m256i res;
                __m256i leftByWplusW = _mm256_set1_epi32(storedLeftByWplusW[y]);
                __m256i topRightMinusLeft = _mm256_set1_epi32(storedTopRightMinusLeft[y]);
                topRightMinusLeft = _mm256_mullo_epi16(topRightMinusLeft, seq8); // (x + 1) * (TopRight - Left[y])
                acc = _mm256_add_epi16(acc, botLeftMinusTop); // Width * Top[x] + (y + 1) * (BottomLeft - Top[x])
                res = _mm256_add_epi16(acc, leftByWplusW); // + Width * Left[x] + Width
                res = _mm256_add_epi16(res, topRightMinusLeft); // + (x + 1) * (TopRight - Left[y])
                res = _mm256_srli_epi16(res, log2w+1);
                res = _mm256_packus_epi16(res, res);
                if (W == 4) {
                    *(Ipp64u *)(dst+y*dstStride+2*x) = _mm_extract_epi64(_mm256_castsi256_si128(res), 0);
                } else {
                    res = _mm256_permute4x64_epi64(res, 0xd8);
                    _mm_storeu_si128((__m128i *)(dst+y*dstStride+2*x), mm128(res));
                }
            }

            if (W > 8)
                seq8 = _mm256_add_epi16(seq8, _mm256_set1_epi16(8));
        }
    }
};

/* planar intra prediction for 4x4, 8x8, 16x16, and 32x32 blocks (with arbitrary pitch) */
void h265_PredictIntra_Planar_ChromaNV12_8u_avx2(Ipp8u* predPel, Ipp8u* dst, Ipp32s dstStride, Ipp32s blkSize)
{
    assert(blkSize == 4 || blkSize == 8 || blkSize == 16 || blkSize == 32);

    switch (blkSize) {
    case 4:  return PredictIntra_Planar_ChromaNV12_8u_avx2_details::Impl<4, 2>(predPel, dst, dstStride);
    case 8:  return PredictIntra_Planar_ChromaNV12_8u_avx2_details::Impl<8, 3>(predPel, dst, dstStride);
    case 16: return PredictIntra_Planar_ChromaNV12_8u_avx2_details::Impl<16,4>(predPel, dst, dstStride);
    case 32: return PredictIntra_Planar_ChromaNV12_8u_avx2_details::Impl<32,5>(predPel, dst, dstStride);
    }
}

} // end namespace MFX_HEVC_PP

#endif  // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...

#endif  // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
