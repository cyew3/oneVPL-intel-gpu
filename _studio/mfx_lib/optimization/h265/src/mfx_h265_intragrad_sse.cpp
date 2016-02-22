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

/* SSE (CPU) implementation of fast gradient analysis for intra prediction
 * software fallback when Cm version not available
 */

#include <string.h>
#include <math.h>

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_ATOM)

#ifdef MAX
#undef MAX
#endif
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#ifdef MIN
#undef MIN
#endif
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define CLIPVAL(VAL, MINVAL, MAXVAL) MAX(MINVAL, MIN(MAXVAL, VAL))

namespace MFX_HEVC_PP
{

#define HIST_MAX    40

#define CM_CONST_PI   3.14159f
#define M_DBL_EPSILON 0.00001f

ALIGN_DECL(16) static const signed char dyShufTab08[16] = {0, 2, 1, 3, 2, 4, 3, 5, 0, 2, 1, 3, 2, 4, 3, 5}; 
ALIGN_DECL(16) static const signed char dyShufTab16[16] = {0, 1, 4, 5, 2, 3, 6, 7, 4, 5, 8, 9, 6, 7, 10, 11}; 

ALIGN_DECL(16) static const signed char pm1Tab08[16] = {+1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1}; 
ALIGN_DECL(16) static const signed char pm2Tab08[16] = {+2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2}; 

ALIGN_DECL(16) static const signed short pm1Tab16[8] = {+1, -1, +1, -1, +1, -1, +1, -1}; 
ALIGN_DECL(16) static const signed short pm2Tab16[8] = {+2, -2, +2, -2, +2, -2, +2, -2}; 

/* floating point constants */
ALIGN_DECL(16) static const float fPi05[4] = {CM_CONST_PI * 0.5f,  CM_CONST_PI * 0.5f,  CM_CONST_PI * 0.5f,  CM_CONST_PI * 0.5f };
ALIGN_DECL(16) static const float fPi07[4] = {CM_CONST_PI * 0.75f, CM_CONST_PI * 0.75f, CM_CONST_PI * 0.75f, CM_CONST_PI * 0.75f};
ALIGN_DECL(16) static const float fPi10[4] = {CM_CONST_PI * 1.0f,  CM_CONST_PI * 1.0f,  CM_CONST_PI * 1.0f,  CM_CONST_PI * 1.0f };
ALIGN_DECL(16) static const float fPi15[4] = {CM_CONST_PI * 1.5f,  CM_CONST_PI * 1.5f,  CM_CONST_PI * 1.5f,  CM_CONST_PI * 1.5f };
ALIGN_DECL(16) static const float fPi20[4] = {CM_CONST_PI * 2.0f,  CM_CONST_PI * 2.0f,  CM_CONST_PI * 2.0f,  CM_CONST_PI * 2.0f };

ALIGN_DECL(16) static const float fCep[4]  = {M_DBL_EPSILON, M_DBL_EPSILON, M_DBL_EPSILON, M_DBL_EPSILON};
ALIGN_DECL(16) static const float fC28[4]  = {0.28f, 0.28f, 0.28f, 0.28f};
ALIGN_DECL(16) static const float fC10[4]  = {10.504226f, 10.504226f, 10.504226f, 10.504226f};
ALIGN_DECL(16) static const float fC02[4]  = {2.0f, 2.0f, 2.0f, 2.0f};

#define mmf2i(s)    _mm_castps_si128(s)
#define mmi2f(s)    _mm_castsi128_ps(s)

template <class PixType, class HistType>
static void BuildHistogramInner(const PixType *frame, HistType *hist, mfxI32 framePitch)
{
    ALIGN_DECL(16) mfxI32 x, y, dx[4], dy[4], ang2[4*4];
    ALIGN_DECL(16) mfxU16 amp[4*4];

    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    __m128 xmm3f, xmm5f, xmm6f;

    /* frame buffer starts at (-1,-1) from upper left pixel in 4x4 block */
    frame -= (framePitch + 1);

    for (y = 0; y < 4; y++) {
        if (sizeof(PixType) == 1) {
            /* load all 3 rows each time to free up registers */
            xmm0 = _mm_loadl_epi64((__m128i *)(frame + 0*framePitch));  /* row -1 */
            xmm1 = _mm_loadl_epi64((__m128i *)(frame + 1*framePitch));  /* row  0 */
            xmm6 = _mm_loadl_epi64((__m128i *)(frame + 2*framePitch));  /* row +1 */
            frame += framePitch;

            /* dx = [1,2,1]*row(+1) - [1,2,1]*row(-1), interleave rows +1 and -1 */
            xmm2 = _mm_unpacklo_epi8(xmm6, xmm0);
            xmm3 = _mm_srli_si128(xmm2, 2);
            xmm4 = _mm_srli_si128(xmm2, 4);

            xmm2 = _mm_maddubs_epi16(xmm2, *(__m128i *)pm1Tab08);       /* col -1 */
            xmm3 = _mm_maddubs_epi16(xmm3, *(__m128i *)pm2Tab08);       /* col  0 */
            xmm4 = _mm_maddubs_epi16(xmm4, *(__m128i *)pm1Tab08);       /* col +1 */

            /* add columns, expand to 32 bits, calculate x2 = dx*dx */
            xmm2 = _mm_add_epi16(xmm2, xmm3);
            xmm2 = _mm_add_epi16(xmm2, xmm4);
            xmm2 = _mm_cvtepi16_epi32(xmm2);
            xmm3 = _mm_mullo_epi32(xmm2, xmm2);

            /* dy = [1,2,1]*col(-1) - [1,2,1]*col(+1), interleave cols -1 and +1 for each row (-1,0,+1) */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)dyShufTab08);
            xmm4 = _mm_shuffle_epi8(xmm1, *(__m128i *)dyShufTab08);
            xmm5 = _mm_shuffle_epi8(xmm6, *(__m128i *)dyShufTab08);

            xmm0 = _mm_maddubs_epi16(xmm0, *(__m128i *)pm1Tab08);
            xmm4 = _mm_maddubs_epi16(xmm4, *(__m128i *)pm2Tab08);
            xmm5 = _mm_maddubs_epi16(xmm5, *(__m128i *)pm1Tab08);

            /* add rows, expand to 32 bits, calculate y2 = dy*dy  */
            xmm4 = _mm_add_epi16(xmm4, xmm0);
            xmm4 = _mm_add_epi16(xmm4, xmm5);
            xmm4 = _mm_cvtepi16_epi32(xmm4);
            xmm5 = _mm_mullo_epi32(xmm4, xmm4);
        } else {
            /* load all 3 rows each time to free up registers */
            xmm0 = _mm_loadu_si128((__m128i *)(frame + 0*framePitch));  /* row -1 */
            xmm1 = _mm_loadu_si128((__m128i *)(frame + 1*framePitch));  /* row  0 */
            xmm6 = _mm_loadu_si128((__m128i *)(frame + 2*framePitch));  /* row +1 */
            frame += framePitch;

            /* dx = [1,2,1]*row(+1) - [1,2,1]*row(-1), interleave rows +1 and -1 */
            xmm2 = _mm_unpacklo_epi16(xmm6, xmm0);
            xmm5 = _mm_unpackhi_epi16(xmm6, xmm0);
            xmm3 = _mm_alignr_epi8(xmm5, xmm2, 2*2);
            xmm4 = _mm_alignr_epi8(xmm5, xmm2, 2*4);

            xmm2 = _mm_madd_epi16(xmm2, *(__m128i *)pm1Tab16);       /* col -1 */
            xmm3 = _mm_madd_epi16(xmm3, *(__m128i *)pm2Tab16);       /* col  0 */
            xmm4 = _mm_madd_epi16(xmm4, *(__m128i *)pm1Tab16);       /* col +1 */

            /* add columns, calculate x2 = dx*dx */
            xmm2 = _mm_add_epi32(xmm2, xmm3);
            xmm2 = _mm_add_epi32(xmm2, xmm4);
            xmm3 = _mm_mullo_epi32(xmm2, xmm2);

            /* dy = [1,2,1]*col(-1) - [1,2,1]*col(+1), interleave cols -1 and +1 for each row (-1,0,+1) */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)dyShufTab16);
            xmm4 = _mm_shuffle_epi8(xmm1, *(__m128i *)dyShufTab16);
            xmm5 = _mm_shuffle_epi8(xmm6, *(__m128i *)dyShufTab16);

            xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)pm1Tab16);
            xmm4 = _mm_madd_epi16(xmm4, *(__m128i *)pm2Tab16);
            xmm5 = _mm_madd_epi16(xmm5, *(__m128i *)pm1Tab16);

            /* add rows, calculate y2 = dy*dy  */
            xmm4 = _mm_add_epi32(xmm4, xmm0);
            xmm4 = _mm_add_epi32(xmm4, xmm5);
            xmm5 = _mm_mullo_epi32(xmm4, xmm4);
        }

        /* masks: xmm0 = (dx < 0), xmm1 = (dy < 0) */
        xmm0 = _mm_cmpgt_epi32(_mm_setzero_si128(), xmm2);
        xmm1 = _mm_cmpgt_epi32(_mm_setzero_si128(), xmm4);

        /* save dx, dy for final offset adjustment */
        _mm_store_si128((__m128i *)dx, xmm2);
        _mm_store_si128((__m128i *)dy, xmm4);

        /* xy = dx*dy */
        xmm2 = _mm_mullo_epi32(xmm2, xmm4);

        /* choose starting value (a0, a1) based on quadrant of (x,y) */
        xmm6 = _mm_load_si128((__m128i *)fPi05);
        xmm7 = _mm_setzero_si128();
        xmm6 = _mm_blendv_epi8(xmm6, *(__m128i *)fPi15, xmm1);  /* a0 = 0.5*pi or 1.5*pi */
        xmm7 = _mm_blendv_epi8(xmm7, *(__m128i *)fPi20, xmm1);  /* a1 = 0 or 2.0*pi */
        xmm7 = _mm_blendv_epi8(xmm7, *(__m128i *)fPi10, xmm0);  /* a1 = 0 or 2.0*pi or 1.0 *pi */
        xmm0 = _mm_cmpgt_epi32(xmm5, xmm3);                     /* mask: xmm0 = (y2 > x2) */

        /* angf = a1 or a0, offset = +/- xy */
        xmm7 = _mm_blendv_epi8(xmm7, xmm6, xmm0);
        xmm6 = _mm_xor_si128(xmm2, xmm0);
        xmm6 = _mm_sub_epi32(xmm6, xmm0);

        /* denominator = (y2 + 0.28*x2 + eps) or (x2 + 0.28*y2  + eps) */
        xmm4 = xmm3;
        xmm3 = _mm_blendv_epi8(xmm3, xmm5, xmm0);
        xmm5 = _mm_blendv_epi8(xmm5, xmm4, xmm0);
        xmm3f = _mm_cvtepi32_ps(xmm3);
        xmm5f = _mm_cvtepi32_ps(xmm5);
        xmm5f = _mm_mul_ps(xmm5f, *(__m128 *)fC28);
        xmm3f = _mm_add_ps(xmm3f, xmm5f);
        xmm3f = _mm_add_ps(xmm3f, *(__m128 *)fCep);
        
        /* a0 - (xy / denom) or a1 + (xy / denom) */
        xmm6f = _mm_cvtepi32_ps(xmm6);
        xmm6f = _mm_div_ps(xmm6f, xmm3f);
        xmm6f = _mm_add_ps(xmm6f, mmi2f(xmm7));

        xmm2 = _mm_load_si128((__m128i *)dx);
        xmm3 = _mm_load_si128((__m128i *)dy);

        /* save final mask for (dx == 0 && dy == 0) */
        xmm1 = _mm_cmpeq_epi32(_mm_setzero_si128(), xmm2);
        xmm4 = _mm_cmpeq_epi32(_mm_setzero_si128(), xmm3);
        xmm1 = _mm_and_si128(xmm1, xmm4);

        /* save amplitude = abs(dx) + abs(dy) */
        xmm6 = _mm_abs_epi32(xmm2);
        xmm7 = _mm_abs_epi32(xmm3);
        xmm6 = _mm_add_epi32(xmm6, xmm7);
        xmm6 = _mm_packs_epi32(xmm6, xmm6);
        _mm_storel_epi64((__m128i *)(amp + 4*y), xmm6);

        /* generate masks */
        xmm7 = xmm3;
        xmm6 = _mm_setzero_si128();
        xmm2 = _mm_cmpgt_epi32(xmm2, xmm6);   /* mask: dx > 0 */
        xmm3 = _mm_cmpgt_epi32(xmm3, xmm6);   /* mask: dy > 0 */
        xmm6 = _mm_cmpgt_epi32(xmm6, xmm7);   /* mask: dy < 0 (inverse: dy >= 0) */
        
        /* add necessary +/- PI offset to atan2 (branchless) */
        xmm4 = _mm_load_si128((__m128i *)fPi10);
        xmm5 = xmm4;
        xmm4 = _mm_and_si128(xmm4, xmm0);
        xmm4 = _mm_and_si128(xmm4, xmm3);     /* (y2 > x2 && dy > 0) */

        xmm0 = _mm_xor_si128(xmm0, _mm_set1_epi32(-1));
        xmm0 = _mm_and_si128(xmm0, xmm2);
        xmm5 = _mm_and_si128(xmm5, xmm0);     /* (y2 <= x2 && dx > 0) */

        xmm7 = _mm_and_si128(xmm5, xmm6);     /* (y2 <= x2 && dx > 0 && dy < 0) */
        xmm6 = _mm_xor_si128(xmm6, _mm_set1_epi32(-1));
        xmm5 = _mm_and_si128(xmm5, xmm6);     /* (y2 <= x2 && dx > 0 && dy >= 0) */

        xmm6f = _mm_add_ps(xmm6f, mmi2f(xmm4));
        xmm6f = _mm_add_ps(xmm6f, mmi2f(xmm5));
        xmm6f = _mm_sub_ps(xmm6f, mmi2f(xmm7));

        /* ang2 = (angf - 0.75*CM_CONST_PI)*10.504226 + 2.0f, convert to integer (truncate) */
        xmm6f = _mm_sub_ps(xmm6f, *(__m128 *)fPi07);
        xmm6f = _mm_mul_ps(xmm6f, *(__m128 *)fC10);
        xmm6f = _mm_add_ps(xmm6f, *(__m128 *)fC02);
        xmm4  = _mm_cvttps_epi32(xmm6f);

        /* if (dx == 0 && dy == 0) ang2 = 0; */
        xmm4 = _mm_blendv_epi8(xmm4, _mm_setzero_si128(), xmm1);
        
        /* store angle, range = [0,34] */
        _mm_store_si128((__m128i *)(ang2 + 4*y), xmm4);
    }

    /* update histogram (faster to do outside of main loop - compiler should fully unroll) */
    for (y = 0; y < 4; y++) {
        for (x = 0; x < 4; x++) {
            hist[ang2[4*y+x]] += amp[4*y+x];
        }
    }
}

/* sum 4x4 histograms to generate 8x8 map 
 * NOTE: outData8 assumed to fit in 16-bit unsigned (see comment on GPU implementation)
 *   but might be necessary to use 32-bit for 8x8 (consider max value of 64*(abs(dx)+abs(y)))
 * for now saturate to 2^16 - 1
 */
template <class HistType>
static void Generate8x8(HistType *outData4, HistType *outData8, mfxI32 width, mfxI32 height)
{
    mfxI32 x, y, i, t, xBlocks4, yBlocks4, xBlocks8, yBlocks8;
    HistType *pDst, *pDst8;

    __m128i xmm0, xmm1, xmm2, xmm3;

    xBlocks4 = width  / 4;
    yBlocks4 = height / 4;
    xBlocks8 = width  / 8;
    yBlocks8 = height / 8;

    for (y = 0; y < yBlocks8; y++) {
        for (x = 0; x < xBlocks8; x++) {
            pDst = outData4 + 2*(y*xBlocks4 + x) * HIST_MAX; 
            pDst8 = outData8 +   (y*xBlocks8 + x) * HIST_MAX; 

            if (sizeof(HistType) == 4) {
                for (i = 0; i < HIST_MAX; i += 4) {
                    xmm0 = _mm_loadu_si128((__m128i *)(&pDst[i]));
                    xmm1 = _mm_loadu_si128((__m128i *)(&pDst[i + HIST_MAX]));
                    xmm2 = _mm_loadu_si128((__m128i *)(&pDst[i + xBlocks4*HIST_MAX]));
                    xmm3 = _mm_loadu_si128((__m128i *)(&pDst[i + xBlocks4*HIST_MAX + HIST_MAX]));

                    xmm0 = _mm_add_epi32(xmm0, xmm1);
                    xmm0 = _mm_add_epi32(xmm0, xmm2);
                    xmm0 = _mm_add_epi32(xmm0, xmm3);
                    _mm_storeu_si128((__m128i *)(&pDst8[i]), xmm0);
                }

                for (    ; i < HIST_MAX; i++) {
                    t  = pDst[i];
                    t += pDst[i + HIST_MAX];
                    t += pDst[i + xBlocks4*HIST_MAX];
                    t += pDst[i + xBlocks4*HIST_MAX + HIST_MAX];
                    pDst8[i] = t;
                }
            } else {
                for (i = 0; i < HIST_MAX; i += 8) {
                    xmm0 = _mm_loadu_si128((__m128i *)(&pDst[i]));
                    xmm1 = _mm_loadu_si128((__m128i *)(&pDst[i + HIST_MAX]));
                    xmm2 = _mm_loadu_si128((__m128i *)(&pDst[i + xBlocks4*HIST_MAX]));
                    xmm3 = _mm_loadu_si128((__m128i *)(&pDst[i + xBlocks4*HIST_MAX + HIST_MAX]));

                    xmm0 = _mm_adds_epu16(xmm0, xmm1);
                    xmm0 = _mm_adds_epu16(xmm0, xmm2);
                    xmm0 = _mm_adds_epu16(xmm0, xmm3);
                    _mm_storeu_si128((__m128i *)(&pDst8[i]), xmm0);
                }

                for (    ; i < HIST_MAX; i++) {
                    t  = pDst[i];
                    t += pDst[i + HIST_MAX];
                    t += pDst[i + xBlocks4*HIST_MAX];
                    t += pDst[i + xBlocks4*HIST_MAX + HIST_MAX];
                    pDst8[i] = (HistType)CLIPVAL(t, 0, (1 << 16) - 1);
                }
            }
        }
    }
}

/* width and height must be multiples of 8 */
template <class PixType, class HistType>
void MAKE_NAME(h265_AnalyzeGradient)(const PixType *src, Ipp32s pitch, HistType *hist4, HistType *hist8, Ipp32s width, Ipp32s height)
{
    Ipp32s xBlocks4 = width  / 4;
    Ipp32s yBlocks4 = height / 4;

    /* fast path - inner blocks */
    for (Ipp32s y = 0; y < yBlocks4; y++) {
        const PixType *pSrc = src + 4 * y * pitch;
        HistType *pDst = hist4 + y * xBlocks4 * HIST_MAX;
        for (Ipp32s x = 0; x < xBlocks4; x++) {
            memset(pDst, 0, sizeof(HistType) * HIST_MAX);
            BuildHistogramInner(pSrc, pDst, pitch);
            pSrc += 4;
            pDst += HIST_MAX;
        }
    }

    /* sum 4x4 histograms to generate 8x8 map */
    Generate8x8(hist4, hist8, width, height);
}

template void MAKE_NAME(h265_AnalyzeGradient)<Ipp8u, Ipp16u> (const Ipp8u  *src, Ipp32s pitch, Ipp16u *hist4, Ipp16u *hist8, Ipp32s width, Ipp32s height);
template void MAKE_NAME(h265_AnalyzeGradient)<Ipp16u, Ipp32u>(const Ipp16u *src, Ipp32s pitch, Ipp32u *hist4, Ipp32u *hist8, Ipp32s width, Ipp32s height);

#define MAX_CU_SIZE 64

void MAKE_NAME(h265_ComputeRsCs_8u)(const unsigned char *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height)
{
    Ipp32s i, j, k1, k2;
    Ipp32s p2 = pitchSrc << 1;
    Ipp32s p3 = p2 + pitchSrc;
    Ipp32s p4 = pitchSrc << 2;
    const unsigned char *pSrcG = ySrc - pitchSrc - 1;
    const unsigned char *pSrc;

    __m128i xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9;
    __m128i xmm10, xmm11, xmm12, xmm13, xmm14;

    k1 = 0;
    for(i = 0; i < height; i += 4)
    { 
        k2 = k1;
        pSrc = pSrcG;
        for(j = 0; j < width; j += 8)
        {
            xmm5  = _mm_loadu_si128((__m128i *)&pSrc[0]); 
            xmm7  = _mm_loadu_si128((__m128i *)&pSrc[pitchSrc]); 
            xmm9  = _mm_loadu_si128((__m128i *)&pSrc[p2]); 
            xmm11 = _mm_loadu_si128((__m128i *)&pSrc[p3]); 
            xmm13 = _mm_loadu_si128((__m128i *)&pSrc[p4]); 

            xmm6 = _mm_srli_si128 (xmm5, 1);    // 1 pixel shift
            xmm5 = _mm_cvtepu8_epi16(xmm5);
            xmm6 = _mm_cvtepu8_epi16(xmm6);

            xmm8 = _mm_srli_si128 (xmm7, 1);
            xmm7 = _mm_cvtepu8_epi16(xmm7);
            xmm8 = _mm_cvtepu8_epi16(xmm8);

            xmm10 = _mm_srli_si128 (xmm9, 1);
            xmm9  = _mm_cvtepu8_epi16(xmm9);
            xmm10 = _mm_cvtepu8_epi16(xmm10);

            xmm12 = _mm_srli_si128 (xmm11, 1);
            xmm11 = _mm_cvtepu8_epi16(xmm11);
            xmm12 = _mm_cvtepu8_epi16(xmm12);

            xmm14 = _mm_srli_si128 (xmm13, 1);
            xmm13 = _mm_cvtepu8_epi16(xmm13);
            xmm14 = _mm_cvtepu8_epi16(xmm14);

            // Cs
            xmm1 = _mm_sub_epi16(xmm8, xmm7);
            xmm1 = _mm_madd_epi16(xmm1, xmm1);

            xmm2 = _mm_sub_epi16(xmm10, xmm9);
            xmm2 = _mm_madd_epi16(xmm2, xmm2);
            xmm1 = _mm_add_epi32(xmm1, xmm2);

            xmm2 = _mm_sub_epi16(xmm12, xmm11);
            xmm2 = _mm_madd_epi16(xmm2, xmm2);
            xmm1 = _mm_add_epi32(xmm1, xmm2);

            xmm2 = _mm_sub_epi16(xmm14, xmm13);
            xmm2 = _mm_madd_epi16(xmm2, xmm2);
            xmm1 = _mm_add_epi32(xmm1, xmm2);
            
            xmm1 = _mm_hadd_epi32(xmm1, xmm1);
            xmm1 = _mm_srli_epi32 (xmm1, 4);
            _mm_storel_epi64((__m128i *)(lcuCs+k2), xmm1);

            // Rs 
            xmm3 = _mm_sub_epi16(xmm8, xmm6);
            xmm3 = _mm_madd_epi16(xmm3, xmm3);

            xmm4 = _mm_sub_epi16(xmm10, xmm8);
            xmm4 = _mm_madd_epi16(xmm4, xmm4);
            xmm3 = _mm_add_epi32(xmm3, xmm4);

            xmm4 = _mm_sub_epi16(xmm12, xmm10);
            xmm4 = _mm_madd_epi16(xmm4, xmm4);
            xmm3  = _mm_add_epi32(xmm3, xmm4);

            xmm4 = _mm_sub_epi16(xmm14, xmm12);
            xmm4 = _mm_madd_epi16(xmm4, xmm4);
            xmm3  = _mm_add_epi32(xmm3, xmm4);

            xmm3 = _mm_hadd_epi32(xmm3, xmm3);
            xmm3 = _mm_srli_epi32 (xmm3, 4);
            _mm_storel_epi64((__m128i *)(lcuRs+k2), xmm3);

            pSrc += 8;
            k2 += 2;
        }
        k1 += pitchRsCs;
        pSrcG += p4;
    }
}

void MAKE_NAME(h265_ComputeRsCs_16u)(const Ipp16u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height)
{
    Ipp32s i, j, k1, k2;
    Ipp32s p2 = pitchSrc << 1;
    Ipp32s p3 = p2 + pitchSrc;
    Ipp32s p4 = pitchSrc << 2;
    const Ipp16u *pSrcG = ySrc - pitchSrc - 1;
    const Ipp16u *pSrc;

    __m128i xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9;
    __m128i xmm10, xmm11, xmm12, xmm13, xmm14;

    k1 = 0;
    for(i = 0; i < height; i += 4)
    { 
        k2 = k1;
        pSrc = pSrcG;
        for(j = 0; j < width; j += 8)
        {
            xmm7  = _mm_loadu_si128((__m128i *)&pSrc[pitchSrc]); 
            xmm9  = _mm_loadu_si128((__m128i *)&pSrc[p2]); 
            xmm11 = _mm_loadu_si128((__m128i *)&pSrc[p3]); 
            xmm13 = _mm_loadu_si128((__m128i *)&pSrc[p4]); 

            xmm6  = _mm_loadu_si128((__m128i *)&pSrc[1]); 
            xmm8  = _mm_loadu_si128((__m128i *)&pSrc[pitchSrc+1]);
            xmm10 = _mm_loadu_si128((__m128i *)&pSrc[p2+1]); 
            xmm12 = _mm_loadu_si128((__m128i *)&pSrc[p3+1]);
            xmm14 = _mm_loadu_si128((__m128i *)&pSrc[p4+1]);

            // Cs
            xmm1 = _mm_sub_epi16(xmm8, xmm7);
            xmm1 = _mm_madd_epi16(xmm1, xmm1);

            xmm2 = _mm_sub_epi16(xmm10, xmm9);
            xmm2 = _mm_madd_epi16(xmm2, xmm2);
            xmm1 = _mm_add_epi32(xmm1, xmm2);

            xmm2 = _mm_sub_epi16(xmm12, xmm11);
            xmm2 = _mm_madd_epi16(xmm2, xmm2);
            xmm1 = _mm_add_epi32(xmm1, xmm2);

            xmm2 = _mm_sub_epi16(xmm14, xmm13);
            xmm2 = _mm_madd_epi16(xmm2, xmm2);
            xmm1 = _mm_add_epi32(xmm1, xmm2);
            
            xmm1 = _mm_hadd_epi32(xmm1, xmm1);
            xmm1 = _mm_srli_epi32 (xmm1, 4);
            _mm_storel_epi64((__m128i *)(lcuCs+k2), xmm1);

            // Rs 
            xmm3 = _mm_sub_epi16(xmm8, xmm6);
            xmm3 = _mm_madd_epi16(xmm3, xmm3);

            xmm4 = _mm_sub_epi16(xmm10, xmm8);
            xmm4 = _mm_madd_epi16(xmm4, xmm4);
            xmm3 = _mm_add_epi32(xmm3, xmm4);

            xmm4 = _mm_sub_epi16(xmm12, xmm10);
            xmm4 = _mm_madd_epi16(xmm4, xmm4);
            xmm3  = _mm_add_epi32(xmm3, xmm4);

            xmm4 = _mm_sub_epi16(xmm14, xmm12);
            xmm4 = _mm_madd_epi16(xmm4, xmm4);
            xmm3  = _mm_add_epi32(xmm3, xmm4);

            xmm3 = _mm_hadd_epi32(xmm3, xmm3);
            xmm3 = _mm_srli_epi32 (xmm3, 4);
            _mm_storel_epi64((__m128i *)(lcuRs+k2), xmm3);

            pSrc += 8;
            k2 += 2;
        }
        k1 += pitchRsCs;
        pSrcG += p4;
    }
}


}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_ATOM)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
