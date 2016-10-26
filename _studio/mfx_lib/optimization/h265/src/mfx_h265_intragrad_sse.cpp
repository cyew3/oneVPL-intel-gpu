//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

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



#define _mm_loadh_epi64(a, ptr) _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(a), (__m64 *)(ptr)))
#define _mm_movehl_epi64(a, b) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b)))
    // Load 0..3 floats to XMM register from memory
    // NOTE: elements of XMM are permuted [ 2 - 1 ]
    static inline __m128 LoadPartialXmm(Ipp32f *pSrc, Ipp32s len)
    {
        __m128 xmm = _mm_setzero_ps();
        if (len & 2)
        {
            xmm = _mm_loadh_pi(xmm, (__m64 *)pSrc);
            pSrc += 2;
        }
        if (len & 1)
        {
            xmm = _mm_move_ss(xmm, _mm_load_ss(pSrc));
        }
        return xmm;
    }

        // Load 0..15 bytes to XMM register from memory
    // NOTE: elements of XMM are permuted [ 8 4 2 - 1 ]
    template <char init>
    static inline __m128i LoadPartialXmm(Ipp8u *pSrc, Ipp32s len)
    {
        __m128i xmm = _mm_set1_epi8(init);
        if (len & 8) {
            xmm = _mm_loadh_epi64(xmm, (__m64 *)pSrc);
            pSrc += 8;
        }
        if (len & 4) {
            xmm = _mm_insert_epi32(xmm, *((Ipp32s *)pSrc), 1);
            pSrc += 4;
        }
        if (len & 2) {
            xmm = _mm_insert_epi16(xmm, *((short *)pSrc), 1);
            pSrc += 2;
        }
        if (len & 1) {
            xmm = _mm_insert_epi8(xmm, *pSrc, 0);
        }
        return xmm;
    }



void MAKE_NAME(h265_ImageDiffHistogram_8u)(Ipp8u* pSrc, Ipp8u* pRef, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s histogram[5], Ipp64s *pSrcDC, Ipp64s *pRefDC)
{
    enum {HIST_THRESH_LO = 4, HIST_THRESH_HI = 12};
    __m128i sDC = _mm_setzero_si128();
    __m128i rDC = _mm_setzero_si128();

    __m128i h0 = _mm_setzero_si128();
    __m128i h1 = _mm_setzero_si128();
    __m128i h2 = _mm_setzero_si128();
    __m128i h3 = _mm_setzero_si128();

    __m128i zero = _mm_setzero_si128();

    for (Ipp32s i = 0; i < height; i++)
    {
        // process 16 pixels per iteration
        Ipp32s j;
        for (j = 0; j < width - 15; j += 16)
        {
            __m128i s = _mm_loadu_si128((__m128i *)(&pSrc[j]));
            __m128i r = _mm_loadu_si128((__m128i *)(&pRef[j]));

            sDC = _mm_add_epi64(sDC, _mm_sad_epu8(s, zero));    //accumulate horizontal sums
            rDC = _mm_add_epi64(rDC, _mm_sad_epu8(r, zero));

            r = _mm_sub_epi8(r, _mm_set1_epi8(-128));   // convert to signed
            s = _mm_sub_epi8(s, _mm_set1_epi8(-128));

            __m128i dn = _mm_subs_epi8(r, s);   // -d saturated to [-128,127]
            __m128i dp = _mm_subs_epi8(s, r);   // +d saturated to [-128,127]

            __m128i m0 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(HIST_THRESH_HI)); // d < -12
            __m128i m1 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(HIST_THRESH_LO)); // d < -4
            __m128i m2 = _mm_cmpgt_epi8(_mm_set1_epi8(HIST_THRESH_LO), dp); // d < +4
            __m128i m3 = _mm_cmpgt_epi8(_mm_set1_epi8(HIST_THRESH_HI), dp); // d < +12

            m0 = _mm_sub_epi8(zero, m0);    // negate masks from 0xff to 1
            m1 = _mm_sub_epi8(zero, m1);
            m2 = _mm_sub_epi8(zero, m2);
            m3 = _mm_sub_epi8(zero, m3);

            h0 = _mm_add_epi32(h0, _mm_sad_epu8(m0, zero)); // accumulate horizontal sums
            h1 = _mm_add_epi32(h1, _mm_sad_epu8(m1, zero));
            h2 = _mm_add_epi32(h2, _mm_sad_epu8(m2, zero));
            h3 = _mm_add_epi32(h3, _mm_sad_epu8(m3, zero));
        }

        // process remaining 1..15 pixels
        if (j < width)
        {
            __m128i s = LoadPartialXmm<0>(&pSrc[j], width & 0xf);
            __m128i r = LoadPartialXmm<0>(&pRef[j], width & 0xf);

            sDC = _mm_add_epi64(sDC, _mm_sad_epu8(s, zero));    //accumulate horizontal sums
            rDC = _mm_add_epi64(rDC, _mm_sad_epu8(r, zero));

            s = LoadPartialXmm<-1>(&pSrc[j], width & 0xf);      // ensure unused elements not counted

            r = _mm_sub_epi8(r, _mm_set1_epi8(-128));   // convert to signed
            s = _mm_sub_epi8(s, _mm_set1_epi8(-128));

            __m128i dn = _mm_subs_epi8(r, s);   // -d saturated to [-128,127]
            __m128i dp = _mm_subs_epi8(s, r);   // +d saturated to [-128,127]

            __m128i m0 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(HIST_THRESH_HI)); // d < -12
            __m128i m1 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(HIST_THRESH_LO)); // d < -4
            __m128i m2 = _mm_cmpgt_epi8(_mm_set1_epi8(HIST_THRESH_LO), dp); // d < +4
            __m128i m3 = _mm_cmpgt_epi8(_mm_set1_epi8(HIST_THRESH_HI), dp); // d < +12

            m0 = _mm_sub_epi8(zero, m0);    // negate masks from 0xff to 1
            m1 = _mm_sub_epi8(zero, m1);
            m2 = _mm_sub_epi8(zero, m2);
            m3 = _mm_sub_epi8(zero, m3);

            h0 = _mm_add_epi32(h0, _mm_sad_epu8(m0, zero)); // accumulate horizontal sums
            h1 = _mm_add_epi32(h1, _mm_sad_epu8(m1, zero));
            h2 = _mm_add_epi32(h2, _mm_sad_epu8(m2, zero));
            h3 = _mm_add_epi32(h3, _mm_sad_epu8(m3, zero));
        }
        pSrc += pitch;
        pRef += pitch;
    }

    // finish horizontal sums
    sDC = _mm_add_epi64(sDC, _mm_movehl_epi64(sDC, sDC));
    rDC = _mm_add_epi64(rDC, _mm_movehl_epi64(rDC, rDC));

    h0 = _mm_add_epi32(h0, _mm_movehl_epi64(h0, h0));
    h1 = _mm_add_epi32(h1, _mm_movehl_epi64(h1, h1));
    h2 = _mm_add_epi32(h2, _mm_movehl_epi64(h2, h2));
    h3 = _mm_add_epi32(h3, _mm_movehl_epi64(h3, h3));

    _mm_storel_epi64((__m128i *)pSrcDC, sDC);
    _mm_storel_epi64((__m128i *)pRefDC, rDC);

    histogram[0] = _mm_cvtsi128_si32(h0);
    histogram[1] = _mm_cvtsi128_si32(h1);
    histogram[2] = _mm_cvtsi128_si32(h2);
    histogram[3] = _mm_cvtsi128_si32(h3);
    histogram[4] = width * height;

    // undo cumulative counts, by differencing
    histogram[4] -= histogram[3];
    histogram[3] -= histogram[2];
    histogram[2] -= histogram[1];
    histogram[1] -= histogram[0];
}


ALIGN_DECL(16) static const mfxU16 tab_killmask[8][8] = {
        0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
        0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
        0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
        0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
        0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff,
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff,
    };

void MAKE_NAME(h265_SearchBestBlock8x8_8u)(Ipp8u *pSrc, Ipp8u *pRef, Ipp32s pitch, Ipp32s xrange, Ipp32s yrange, Ipp32u *bestSAD, Ipp32s *bestX, Ipp32s *bestY)
{
    enum {SAD_SEARCH_VSTEP  = 2};  // 1=FS 2=FHS
    __m128i
        s0 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[0*pitch]), (__m128i *)&pSrc[1*pitch]),
        s1 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[2*pitch]), (__m128i *)&pSrc[3*pitch]),
        s2 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[4*pitch]), (__m128i *)&pSrc[5*pitch]),
        s3 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[6*pitch]), (__m128i *)&pSrc[7*pitch]);
    for (Ipp32s y = 0; y < yrange; y += SAD_SEARCH_VSTEP) {
        for (Ipp32s x = 0; x < xrange; x += 8) {
            Ipp8u* pr = pRef + (y * pitch) + x;
            __m128i
                r0 = _mm_loadu_si128((__m128i *)&pr[0*pitch]),
                r1 = _mm_loadu_si128((__m128i *)&pr[1*pitch]),
                r2 = _mm_loadu_si128((__m128i *)&pr[2*pitch]),
                r3 = _mm_loadu_si128((__m128i *)&pr[3*pitch]),
                r4 = _mm_loadu_si128((__m128i *)&pr[4*pitch]),
                r5 = _mm_loadu_si128((__m128i *)&pr[5*pitch]),
                r6 = _mm_loadu_si128((__m128i *)&pr[6*pitch]),
                r7 = _mm_loadu_si128((__m128i *)&pr[7*pitch]);
            r0 = _mm_add_epi16(_mm_mpsadbw_epu8(r0, s0, 0), _mm_mpsadbw_epu8(r0, s0, 5));
            r1 = _mm_add_epi16(_mm_mpsadbw_epu8(r1, s0, 2), _mm_mpsadbw_epu8(r1, s0, 7));
            r2 = _mm_add_epi16(_mm_mpsadbw_epu8(r2, s1, 0), _mm_mpsadbw_epu8(r2, s1, 5));
            r3 = _mm_add_epi16(_mm_mpsadbw_epu8(r3, s1, 2), _mm_mpsadbw_epu8(r3, s1, 7));
            r4 = _mm_add_epi16(_mm_mpsadbw_epu8(r4, s2, 0), _mm_mpsadbw_epu8(r4, s2, 5));
            r5 = _mm_add_epi16(_mm_mpsadbw_epu8(r5, s2, 2), _mm_mpsadbw_epu8(r5, s2, 7));
            r6 = _mm_add_epi16(_mm_mpsadbw_epu8(r6, s3, 0), _mm_mpsadbw_epu8(r6, s3, 5));
            r7 = _mm_add_epi16(_mm_mpsadbw_epu8(r7, s3, 2), _mm_mpsadbw_epu8(r7, s3, 7));
            r0 = _mm_add_epi16(r0, r1);
            r2 = _mm_add_epi16(r2, r3);
            r4 = _mm_add_epi16(r4, r5);
            r6 = _mm_add_epi16(r6, r7);
            r0 = _mm_add_epi16(r0, r2);
            r4 = _mm_add_epi16(r4, r6);
            r0 = _mm_add_epi16(r0, r4);
            // kill out-of-bound values
            if (xrange - x < 8)
                r0 = _mm_or_si128(r0, _mm_load_si128((__m128i *)tab_killmask[xrange - x]));
            r0 = _mm_minpos_epu16(r0);
            Ipp32u SAD = _mm_extract_epi16(r0, 0);
            if (SAD < *bestSAD) {
                *bestSAD = SAD;
                *bestX = x + _mm_extract_epi16(r0, 1);
                *bestY = y;
            }
        }
    }
}

void MAKE_NAME(h265_ComputeRsCsDiff)(Ipp32f* pRs0, Ipp32f* pCs0, Ipp32f* pRs1, Ipp32f* pCs1, Ipp32s len, Ipp32f* pRsDiff, Ipp32f* pCsDiff)
    {
        Ipp32s i;//, len = wblocks * hblocks;
        __m128d accRs = _mm_setzero_pd();
        __m128d accCs = _mm_setzero_pd();

        for (i = 0; i < len - 3; i += 4)
        {
            __m128 rs = _mm_sub_ps(_mm_loadu_ps(&pRs0[i]), _mm_loadu_ps(&pRs1[i]));
            __m128 cs = _mm_sub_ps(_mm_loadu_ps(&pCs0[i]), _mm_loadu_ps(&pCs1[i]));

            rs = _mm_mul_ps(rs, rs);
            cs = _mm_mul_ps(cs, cs);

            accRs = _mm_add_pd(accRs, _mm_cvtps_pd(rs));
            accCs = _mm_add_pd(accCs, _mm_cvtps_pd(cs));
            accRs = _mm_add_pd(accRs, _mm_cvtps_pd(_mm_movehl_ps(rs, rs)));
            accCs = _mm_add_pd(accCs, _mm_cvtps_pd(_mm_movehl_ps(cs, cs)));
        }

        if (i < len)
        {
            __m128 rs = _mm_sub_ps(LoadPartialXmm(&pRs0[i], len & 0x3), LoadPartialXmm(&pRs1[i], len & 0x3));
            __m128 cs = _mm_sub_ps(LoadPartialXmm(&pCs0[i], len & 0x3), LoadPartialXmm(&pCs1[i], len & 0x3));

            rs = _mm_mul_ps(rs, rs);
            cs = _mm_mul_ps(cs, cs);

            accRs = _mm_add_pd(accRs, _mm_cvtps_pd(rs));
            accCs = _mm_add_pd(accCs, _mm_cvtps_pd(cs));
            accRs = _mm_add_pd(accRs, _mm_cvtps_pd(_mm_movehl_ps(rs, rs)));
            accCs = _mm_add_pd(accCs, _mm_cvtps_pd(_mm_movehl_ps(cs, cs)));
        }

        // horizontal sum
        accRs = _mm_hadd_pd(accRs, accCs);

        __m128 t = _mm_cvtpd_ps(accRs);
        _mm_store_ss(pRsDiff, t);
        _mm_store_ss(pCsDiff, _mm_shuffle_ps(t, t, _MM_SHUFFLE(0,0,0,1)));
    }

void MAKE_NAME(h265_ComputeRsCs4x4_8u)(const Ipp8u* pSrc, Ipp32s srcPitch, Ipp32s wblocks, Ipp32s hblocks, Ipp32f* pRs, Ipp32f* pCs)
{
    for (Ipp32s i = 0; i < hblocks; i++)
    {
        // 4 horizontal blocks at a time
        Ipp32s j;
        for (j = 0; j < wblocks - 3; j += 4)
        {
            __m128i rs = _mm_setzero_si128();
            __m128i cs = _mm_setzero_si128();
            __m128i a0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-srcPitch + 0]));
            __m128i a1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-srcPitch + 8]));

#ifdef __INTEL_COMPILER
#pragma unroll(4)
#endif
            for (Ipp32s k = 0; k < 4; k++)
            {
                __m128i b0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-1]));
                __m128i b1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[7]));
                __m128i c0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[0]));
                __m128i c1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[8]));
                pSrc += srcPitch;

                // accRs += dRs * dRs
                a0 = _mm_sub_epi16(c0, a0);
                a1 = _mm_sub_epi16(c1, a1);
                a0 = _mm_madd_epi16(a0, a0);
                a1 = _mm_madd_epi16(a1, a1);
                a0 = _mm_hadd_epi32(a0, a1);
                rs = _mm_add_epi32(rs, a0);

                // accCs += dCs * dCs
                b0 = _mm_sub_epi16(c0, b0);
                b1 = _mm_sub_epi16(c1, b1);
                b0 = _mm_madd_epi16(b0, b0);
                b1 = _mm_madd_epi16(b1, b1);
                b0 = _mm_hadd_epi32(b0, b1);
                cs = _mm_add_epi32(cs, b0);

                // reuse next iteration
                a0 = c0;
                a1 = c1;
            }

            // scale by 1.0f/N and store
            _mm_storeu_ps(&pRs[i * wblocks + j], _mm_mul_ps(_mm_cvtepi32_ps(rs), _mm_set1_ps(1.0f / 16)));
            _mm_storeu_ps(&pCs[i * wblocks + j], _mm_mul_ps(_mm_cvtepi32_ps(cs), _mm_set1_ps(1.0f / 16)));

            pSrc -= 4 * srcPitch;
            pSrc += 16;
        }

        // remaining blocks
        for (; j < wblocks; j++)
        {
            Ipp32s accRs = 0;
            Ipp32s accCs = 0;

            for (Ipp32s k = 0; k < 4; k++)
            {
                for (Ipp32s l = 0; l < 4; l++)
                {
                    Ipp32s dRs = pSrc[l] - pSrc[l - srcPitch];
                    Ipp32s dCs = pSrc[l] - pSrc[l - 1];
                    accRs += dRs * dRs;
                    accCs += dCs * dCs;
                }
                pSrc += srcPitch;
            }
            pRs[i * wblocks + j] = accRs * (1.0f / 16);
            pCs[i * wblocks + j] = accCs * (1.0f / 16);

            pSrc -= 4 * srcPitch;
            pSrc += 4;
        }
        pSrc -= 4 * wblocks;
        pSrc += 4 * srcPitch;
    }
}
}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_ATOM)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
