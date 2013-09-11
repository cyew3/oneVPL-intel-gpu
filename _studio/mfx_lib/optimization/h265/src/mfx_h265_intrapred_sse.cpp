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

#if (defined(MFX_TARGET_OPTIMIZATION_SSSE3) && defined (MFX_EMULATE_SSSE3)) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 

#include <immintrin.h>
#ifdef MFX_EMULATE_SSSE3
#include "mfx_ssse3_emulation.h"
#endif

typedef Ipp8u PixType;

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

    static inline void PredAngle_4x4_8u_SSE4(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
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

    static inline void PredAngle_8x8_8u_SSE4(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
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

    static inline void PredAngle_16x16_8u_SSE4(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
    {
        Ipp32s iIdx;
        Ipp16s iFact;
        Ipp32s pos = 0;
        __m128i r0, r1, r2, r3;

        for (int j = 0; j < 16; j++)
        {
            pos += angle;
            iIdx = pos >> 5;
            iFact = pos & 31;

            // process i = 0..15

            r0 = _mm_loadu_si128((__m128i *)&pSrc[iIdx+1]);
            r1 = _mm_loadu_si128((__m128i *)&pSrc[iIdx+2]);
            r2 = _mm_unpackhi_epi8(r0, _mm_setzero_si128());
            r3 = _mm_unpackhi_epi8(r1, _mm_setzero_si128());
            r0 = _mm_cvtepu8_epi16(r0);
            r1 = _mm_cvtepu8_epi16(r1);

            r1 = _mm_sub_epi16(r1, r0);
            r3 = _mm_sub_epi16(r3, r2);
            r1 = _mm_mullo_epi16(r1, _mm_set1_epi16(iFact));
            r3 = _mm_mullo_epi16(r3, _mm_set1_epi16(iFact));
            r1 = _mm_add_epi16(r1, _mm_set1_epi16(16));
            r3 = _mm_add_epi16(r3, _mm_set1_epi16(16));
            r1 = _mm_srai_epi16(r1, 5);
            r3 = _mm_srai_epi16(r3, 5);
            r0 = _mm_add_epi16(r0, r1);
            r2 = _mm_add_epi16(r2, r3);

            r0 = _mm_packus_epi16(r0, r2);
            _mm_storeu_si128((__m128i *)&pDst[j*dstPitch], r0);
        }
    }

    static inline void PredAngle_32x32_8u_SSE4(Ipp8u *pSrc, Ipp8u *pDst, Ipp32s dstPitch, Ipp32s angle)
    {
        Ipp32s iIdx;
        Ipp16s iFact;
        Ipp32s pos = 0;
        __m128i r0, r1, r2, r3;

        for (int j = 0; j < 32; j++)
        {
            pos += angle;
            iIdx = pos >> 5;
            iFact = pos & 31;

            for (int i = 0; i < 32; i += 16)
            {
                r0 = _mm_loadu_si128((__m128i *)&pSrc[iIdx+1+i]);
                r1 = _mm_loadu_si128((__m128i *)&pSrc[iIdx+2+i]);
                r2 = _mm_unpackhi_epi8(r0, _mm_setzero_si128());
                r3 = _mm_unpackhi_epi8(r1, _mm_setzero_si128());
                r0 = _mm_cvtepu8_epi16(r0);
                r1 = _mm_cvtepu8_epi16(r1);

                r1 = _mm_sub_epi16(r1, r0);
                r3 = _mm_sub_epi16(r3, r2);
                r1 = _mm_mullo_epi16(r1, _mm_set1_epi16(iFact));
                r3 = _mm_mullo_epi16(r3, _mm_set1_epi16(iFact));
                r1 = _mm_add_epi16(r1, _mm_set1_epi16(16));
                r3 = _mm_add_epi16(r3, _mm_set1_epi16(16));
                r1 = _mm_srai_epi16(r1, 5);
                r3 = _mm_srai_epi16(r3, 5);
                r0 = _mm_add_epi16(r0, r1);
                r2 = _mm_add_epi16(r2, r3);

                r0 = _mm_packus_epi16(r0, r2);
                _mm_storeu_si128((__m128i *)&pDst[j*dstPitch+i], r0);
            }
        }
    }

    // Out-of-place transpose
    static inline void Transpose_4x4_8u_SSE4(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
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

    static inline void Transpose_8x8_8u_SSE4(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
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

    static inline void Transpose_16x16_8u_SSE4(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
    {
        for (int j = 0; j < 16; j += 8)
        {
            for (int i = 0; i < 16; i += 8)
            {
                Transpose_8x8_8u_SSE4(&pSrc[i*srcPitch+j], srcPitch, &pDst[j*dstPitch+i], dstPitch);
            }
        }
    }

    static inline void Transpose_32x32_8u_SSE4(Ipp8u *pSrc, Ipp32s srcPitch, Ipp8u *pDst, Ipp32s dstPitch)
    {
        for (int j = 0; j < 32; j += 8)
        {
            for (int i = 0; i < 32; i += 8)
            {
                Transpose_8x8_8u_SSE4(&pSrc[i*srcPitch+j], srcPitch, &pDst[j*dstPitch+i], dstPitch);
            }
        }
    }

    void MAKE_NAME(h265_PredictIntra_Ang_8u) (
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

        switch (width)
        {
        case 4:
            if (mode >= 18)
                PredAngle_4x4_8u_SSE4(refMain, pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[4*4];
                PredAngle_4x4_8u_SSE4(refMain, buf, 4, intraPredAngle);
                Transpose_4x4_8u_SSE4(buf, 4, pels, pitch);
            }
            break;
        case 8:
            if (mode >= 18)
                PredAngle_8x8_8u_SSE4(refMain, pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[8*8];
                PredAngle_8x8_8u_SSE4(refMain, buf, 8, intraPredAngle);
                Transpose_8x8_8u_SSE4(buf, 8, pels, pitch);
            }
            break;
        case 16:
            if (mode >= 18)
                PredAngle_16x16_8u_SSE4(refMain, pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[16*16];
                PredAngle_16x16_8u_SSE4(refMain, buf, 16, intraPredAngle);
                Transpose_16x16_8u_SSE4(buf, 16, pels, pitch);
            }
            break;
        case 32:
            if (mode >= 18)
                PredAngle_32x32_8u_SSE4(refMain, pels, pitch, intraPredAngle);
            else
            {
                Ipp8u buf[32*32];
                PredAngle_32x32_8u_SSE4(refMain, buf, 32, intraPredAngle);
                Transpose_32x32_8u_SSE4(buf, 32, pels, pitch);
            }
            break;
        }

    } // void h265_PredictIntra_Ang_8u(...)

    }; // namespace MFX_HEVC_COMMON

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */