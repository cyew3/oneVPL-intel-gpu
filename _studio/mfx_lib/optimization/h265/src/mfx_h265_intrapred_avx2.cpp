/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

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
        y0 = _mm256_cvtepu8_epi16(*(__m128i *)&pSrc[iIdx+1]);
        y1 = _mm256_cvtepu8_epi16(*(__m128i *)&pSrc[iIdx+2]);

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
    __m256i y0, y1, y2, y3;

    _mm256_zeroupper();

    for (int j = 0; j < 32; j++)
    {
        pos += angle;
        iIdx = pos >> 5;
        iFact = pos & 31;

        for (int i = 0; i < 32; i += 16)
        {
            y0 = _mm256_cvtepu8_epi16(*(__m128i *)&pSrc[iIdx+1+i]);
            y1 = _mm256_cvtepu8_epi16(*(__m128i *)&pSrc[iIdx+2+i]);

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

    VM_ASSERT(width == 4 || width == 8 || width == 16 || width == 32);

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
    VM_ASSERT(width == 32 && pSrcDst[0] == topLeft && pSrcDst[2*width] == topRight && pSrcDst[4*width] == bottomLeft);

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

    VM_ASSERT(width == 4 || width == 8 || width == 16 || width == 32);

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
    VM_ASSERT(width == 32 && pSrcDst[0] == topLeft && pSrcDst[2*width] == topRight && pSrcDst[4*width] == bottomLeft);

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
            _mm_storeu_si128((__m128i *)(&pels[row*pitch]), mm128(ymm3));
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
                _mm_storeu_si128((__m128i *)(&pels[row*pitch+col]), mm128(ymm3));
            }
            /* add 16 to each offset for next 16 columns */
            ymm7 = _mm256_add_epi16(ymm7, _mm256_set1_epi16(16));
        }
    }
}

/* planar intra prediction for 4x4, 8x8, 16x16, and 32x32 blocks (with arbitrary pitch) */
void MAKE_NAME(h265_PredictIntra_Planar_8u)(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width)
{
    VM_ASSERT(width == 4 || width == 8 || width == 16 || width == 32);

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
    VM_ASSERT(width == 4 || width == 8 || width == 16 || width == 32);

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

} // end namespace MFX_HEVC_PP

#endif  // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...

#endif  // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
