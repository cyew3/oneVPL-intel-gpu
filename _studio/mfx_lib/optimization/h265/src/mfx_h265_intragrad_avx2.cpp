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

/* AVX2 (CPU) implementation of fast gradient analysis for intra prediction
 * software fallback when Cm version not available
 */

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <string.h>
#include <math.h>
#include <assert.h>
#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_AVX2)

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

ALIGN_DECL(32) static const signed char dyShufTab08[32] = {0, 2, 1, 3, 2, 4, 3, 5, 0, 2, 1, 3, 2, 4, 3, 5, 0, 2, 1, 3, 2, 4, 3, 5, 0, 2, 1, 3, 2, 4, 3, 5}; 
ALIGN_DECL(32) static const signed char dyShufTab16[32] = {0, 1, 4, 5, 2, 3, 6, 7, 4, 5, 8, 9, 6, 7, 10, 11, 0, 1, 4, 5, 2, 3, 6, 7, 4, 5, 8, 9, 6, 7, 10, 11}; 

ALIGN_DECL(32) static const signed char pm1Tab08[32] = {+1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1}; 
ALIGN_DECL(32) static const signed char pm2Tab08[32] = {+2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2}; 

ALIGN_DECL(32) static const signed short pm1Tab16[16] = {+1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1}; 
ALIGN_DECL(32) static const signed short pm2Tab16[16] = {+2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2}; 

/* floating point constants */
ALIGN_DECL(32) static const float fPi05[8] = {CM_CONST_PI * 0.5f,  CM_CONST_PI * 0.5f,  CM_CONST_PI * 0.5f,  CM_CONST_PI * 0.5f,  CM_CONST_PI * 0.5f,  CM_CONST_PI * 0.5f,  CM_CONST_PI * 0.5f,  CM_CONST_PI * 0.5f };
ALIGN_DECL(32) static const float fPi07[8] = {CM_CONST_PI * 0.75f, CM_CONST_PI * 0.75f, CM_CONST_PI * 0.75f, CM_CONST_PI * 0.75f, CM_CONST_PI * 0.75f, CM_CONST_PI * 0.75f, CM_CONST_PI * 0.75f, CM_CONST_PI * 0.75f};
ALIGN_DECL(32) static const float fPi10[8] = {CM_CONST_PI * 1.0f,  CM_CONST_PI * 1.0f,  CM_CONST_PI * 1.0f,  CM_CONST_PI * 1.0f,  CM_CONST_PI * 1.0f,  CM_CONST_PI * 1.0f,  CM_CONST_PI * 1.0f,  CM_CONST_PI * 1.0f };
ALIGN_DECL(32) static const float fPi15[8] = {CM_CONST_PI * 1.5f,  CM_CONST_PI * 1.5f,  CM_CONST_PI * 1.5f,  CM_CONST_PI * 1.5f,  CM_CONST_PI * 1.5f,  CM_CONST_PI * 1.5f,  CM_CONST_PI * 1.5f,  CM_CONST_PI * 1.5f };
ALIGN_DECL(32) static const float fPi20[8] = {CM_CONST_PI * 2.0f,  CM_CONST_PI * 2.0f,  CM_CONST_PI * 2.0f,  CM_CONST_PI * 2.0f,  CM_CONST_PI * 2.0f,  CM_CONST_PI * 2.0f,  CM_CONST_PI * 2.0f,  CM_CONST_PI * 2.0f };

ALIGN_DECL(32) static const float fCep[8]  = {M_DBL_EPSILON, M_DBL_EPSILON, M_DBL_EPSILON, M_DBL_EPSILON, M_DBL_EPSILON, M_DBL_EPSILON, M_DBL_EPSILON, M_DBL_EPSILON};
ALIGN_DECL(32) static const float fC28[8]  = {0.28f, 0.28f, 0.28f, 0.28f, 0.28f, 0.28f, 0.28f, 0.28f};
ALIGN_DECL(32) static const float fC10[8]  = {10.504226f, 10.504226f, 10.504226f, 10.504226f, 10.504226f, 10.504226f, 10.504226f, 10.504226f};
ALIGN_DECL(32) static const float fC02[8]  = {2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f};

#define mmf2i(s)    _mm256_castps_si256(s)
#define mmi2f(s)    _mm256_castsi256_ps(s)

#define mm128(s)    _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)    _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

template <class PixType, class HistType>
static void BuildHistogramInner(const PixType *frame, HistType *hist, mfxI32 framePitch)
{
    ALIGN_DECL(32) mfxI32 x, y, dx[8], dy[8], ang2[4*4];
    ALIGN_DECL(32) mfxU16 amp[4*4];

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
    __m256 ymm3f, ymm5f, ymm6f;

    _mm256_zeroupper();

    /* frame buffer starts at (-1,-1) from upper left pixel in 4x4 block */
    frame -= (framePitch + 1);

    /* process 4x4 blocks, 2 rows per loop */
    for (y = 0; y < 4; y += 2) {
        if (sizeof(PixType) == 1) {
            /* load all 3+1 rows each time to free up registers */
            ymm0 = mm256(_mm_loadl_epi64((__m128i *)(frame + 0*framePitch)));   /* row -1 */
            ymm1 = mm256(_mm_loadl_epi64((__m128i *)(frame + 1*framePitch)));   /* row  0 */
            ymm6 = mm256(_mm_loadl_epi64((__m128i *)(frame + 2*framePitch)));   /* row +1 */
            ymm7 = mm256(_mm_loadl_epi64((__m128i *)(frame + 3*framePitch)));   /* row +2 */
            frame += 2*framePitch;
        
            ymm0 = _mm256_permute2f128_si256(ymm0, ymm1, 0x20);
            ymm1 = _mm256_permute2f128_si256(ymm1, ymm6, 0x20);
            ymm6 = _mm256_permute2f128_si256(ymm6, ymm7, 0x20);

            /* dx = [1,2,1]*row(+1) - [1,2,1]*row(-1), interleave rows +1 and -1 */
            ymm2 = _mm256_unpacklo_epi8(ymm6, ymm0);
            ymm3 = _mm256_srli_si256(ymm2, 2);
            ymm4 = _mm256_srli_si256(ymm2, 4);

            ymm2 = _mm256_maddubs_epi16(ymm2, *(__m256i *)pm1Tab08);    /* col -1 */
            ymm3 = _mm256_maddubs_epi16(ymm3, *(__m256i *)pm2Tab08);    /* col  0 */
            ymm4 = _mm256_maddubs_epi16(ymm4, *(__m256i *)pm1Tab08);    /* col +1 */

            /* add columns, expand to 32 bits, calculate x2 = dx*dx */
            ymm2 = _mm256_add_epi16(ymm2, ymm3);
            ymm2 = _mm256_add_epi16(ymm2, ymm4);
            ymm2 = _mm256_unpacklo_epi16(_mm256_setzero_si256(), ymm2);
            ymm2 = _mm256_srai_epi32(ymm2, 16);
            ymm3 = _mm256_mullo_epi32(ymm2, ymm2);

            /* dy = [1,2,1]*col(-1) - [1,2,1]*col(+1), interleave cols -1 and +1 for each row (-1,0,+1) */
            ymm0 = _mm256_shuffle_epi8(ymm0, *(__m256i *)dyShufTab08);
            ymm4 = _mm256_shuffle_epi8(ymm1, *(__m256i *)dyShufTab08);
            ymm5 = _mm256_shuffle_epi8(ymm6, *(__m256i *)dyShufTab08);

            ymm0 = _mm256_maddubs_epi16(ymm0, *(__m256i *)pm1Tab08);
            ymm4 = _mm256_maddubs_epi16(ymm4, *(__m256i *)pm2Tab08);
            ymm5 = _mm256_maddubs_epi16(ymm5, *(__m256i *)pm1Tab08);

            /* add rows, expand to 32 bits, calculate y2 = dy*dy  */
            ymm4 = _mm256_add_epi16(ymm4, ymm0);
            ymm4 = _mm256_add_epi16(ymm4, ymm5);
            ymm4 = _mm256_unpacklo_epi16(_mm256_setzero_si256(), ymm4);
            ymm4 = _mm256_srai_epi32(ymm4, 16);
            ymm5 = _mm256_mullo_epi32(ymm4, ymm4);
        } else {
            /* load all 3+1 rows each time to free up registers */
            ymm0 = mm256(_mm_loadu_si128((__m128i *)(frame + 0*framePitch)));   /* row -1 */
            ymm1 = mm256(_mm_loadu_si128((__m128i *)(frame + 1*framePitch)));   /* row  0 */
            ymm6 = mm256(_mm_loadu_si128((__m128i *)(frame + 2*framePitch)));   /* row +1 */
            ymm7 = mm256(_mm_loadu_si128((__m128i *)(frame + 3*framePitch)));   /* row +2 */
            frame += 2*framePitch;
        
            ymm0 = _mm256_permute2f128_si256(ymm0, ymm1, 0x20);
            ymm1 = _mm256_permute2f128_si256(ymm1, ymm6, 0x20);
            ymm6 = _mm256_permute2f128_si256(ymm6, ymm7, 0x20);

            /* dx = [1,2,1]*row(+1) - [1,2,1]*row(-1), interleave rows +1 and -1 */
            ymm2 = _mm256_unpacklo_epi16(ymm6, ymm0);
            ymm5 = _mm256_unpackhi_epi16(ymm6, ymm0);
            ymm3 = _mm256_alignr_epi8(ymm5, ymm2, 2*2);
            ymm4 = _mm256_alignr_epi8(ymm5, ymm2, 2*4);

            ymm2 = _mm256_madd_epi16(ymm2, *(__m256i *)pm1Tab16);    /* col -1 */
            ymm3 = _mm256_madd_epi16(ymm3, *(__m256i *)pm2Tab16);    /* col  0 */
            ymm4 = _mm256_madd_epi16(ymm4, *(__m256i *)pm1Tab16);    /* col +1 */

            /* add columns, calculate x2 = dx*dx */
            ymm2 = _mm256_add_epi32(ymm2, ymm3);
            ymm2 = _mm256_add_epi32(ymm2, ymm4);
            ymm3 = _mm256_mullo_epi32(ymm2, ymm2);

            /* dy = [1,2,1]*col(-1) - [1,2,1]*col(+1), interleave cols -1 and +1 for each row (-1,0,+1) */
            ymm0 = _mm256_shuffle_epi8(ymm0, *(__m256i *)dyShufTab16);
            ymm4 = _mm256_shuffle_epi8(ymm1, *(__m256i *)dyShufTab16);
            ymm5 = _mm256_shuffle_epi8(ymm6, *(__m256i *)dyShufTab16);

            ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)pm1Tab16);
            ymm4 = _mm256_madd_epi16(ymm4, *(__m256i *)pm2Tab16);
            ymm5 = _mm256_madd_epi16(ymm5, *(__m256i *)pm1Tab16);

            /* add rows, expand to 32 bits, calculate y2 = dy*dy  */
            ymm4 = _mm256_add_epi32(ymm4, ymm0);
            ymm4 = _mm256_add_epi32(ymm4, ymm5);
            ymm5 = _mm256_mullo_epi32(ymm4, ymm4);
        }

        /* masks: ymm0 = (dx < 0), ymm1 = (dy < 0) */
        ymm0 = _mm256_cmpgt_epi32(_mm256_setzero_si256(), ymm2);
        ymm1 = _mm256_cmpgt_epi32(_mm256_setzero_si256(), ymm4);

        /* save dx, dy for final offset adjustment */
        _mm256_store_si256((__m256i *)dx, ymm2);
        _mm256_store_si256((__m256i *)dy, ymm4);

        /* xy = dx*dy */
        ymm2 = _mm256_mullo_epi32(ymm2, ymm4);

        /* choose starting value (a0, a1) based on quadrant of (x,y) */
        ymm6 = _mm256_load_si256((__m256i *)fPi05);
        ymm7 = _mm256_setzero_si256();
        ymm6 = _mm256_blendv_epi8(ymm6, *(__m256i *)fPi15, ymm1);   /* a0 = 0.5*pi or 1.5*pi */
        ymm7 = _mm256_blendv_epi8(ymm7, *(__m256i *)fPi20, ymm1);   /* a1 = 0 or 2.0*pi */
        ymm7 = _mm256_blendv_epi8(ymm7, *(__m256i *)fPi10, ymm0);   /* a1 = 0 or 2.0*pi or 1.0 *pi */
        ymm0 = _mm256_cmpgt_epi32(ymm5, ymm3);                      /* mask: ymm0 = (y2 > x2) */

        /* angf = a1 or a0, offset = +/- xy */
        ymm7 = _mm256_blendv_epi8(ymm7, ymm6, ymm0);
        ymm6 = _mm256_xor_si256(ymm2, ymm0);
        ymm6 = _mm256_sub_epi32(ymm6, ymm0);

        /* denominator = (y2 + 0.28*x2 + eps) or (x2 + 0.28*y2  + eps) */
        ymm4 = ymm3;
        ymm3 = _mm256_blendv_epi8(ymm3, ymm5, ymm0);
        ymm5 = _mm256_blendv_epi8(ymm5, ymm4, ymm0);
        ymm3f = _mm256_cvtepi32_ps(ymm3);
        ymm5f = _mm256_cvtepi32_ps(ymm5);
        ymm5f = _mm256_mul_ps(ymm5f, *(__m256 *)fC28);
        ymm3f = _mm256_add_ps(ymm3f, ymm5f);
        ymm3f = _mm256_add_ps(ymm3f, *(__m256 *)fCep);
        
        /* a0 - (xy / denom) or a1 + (xy / denom) */
        ymm6f = _mm256_cvtepi32_ps(ymm6);
        ymm6f = _mm256_div_ps(ymm6f, ymm3f);
        ymm6f = _mm256_add_ps(ymm6f, mmi2f(ymm7));

        ymm2 = _mm256_load_si256((__m256i *)dx);
        ymm3 = _mm256_load_si256((__m256i *)dy);

        /* save final mask for (dx == 0 && dy == 0) */
        ymm1 = _mm256_cmpeq_epi32(_mm256_setzero_si256(), ymm2);
        ymm4 = _mm256_cmpeq_epi32(_mm256_setzero_si256(), ymm3);
        ymm1 = _mm256_and_si256(ymm1, ymm4);

        /* save amplitude = abs(dx) + abs(dy) */
        ymm6 = _mm256_abs_epi32(ymm2);
        ymm7 = _mm256_abs_epi32(ymm3);
        ymm6 = _mm256_add_epi32(ymm6, ymm7);
        ymm6 = _mm256_packs_epi32(ymm6, ymm6);
        ymm6 = _mm256_permute4x64_epi64(ymm6, 0xd8);
        _mm_store_si128((__m128i *)(amp + 4*y), mm128(ymm6));

        /* generate masks */
        ymm7 = ymm3;
        ymm6 = _mm256_setzero_si256();
        ymm2 = _mm256_cmpgt_epi32(ymm2, ymm6);   /* mask: dx > 0 */
        ymm3 = _mm256_cmpgt_epi32(ymm3, ymm6);   /* mask: dy > 0 */
        ymm6 = _mm256_cmpgt_epi32(ymm6, ymm7);   /* mask: dy < 0 (inverse: dy >= 0) */
        
        /* add necessary +/- PI offset to atan2 (branchless) */
        ymm4 = _mm256_load_si256((__m256i *)fPi10);
        ymm5 = ymm4;
        ymm4 = _mm256_and_si256(ymm4, ymm0);
        ymm4 = _mm256_and_si256(ymm4, ymm3);     /* (y2 > x2 && dy > 0) */

        ymm0 = _mm256_xor_si256(ymm0, _mm256_set1_epi32(-1));
        ymm0 = _mm256_and_si256(ymm0, ymm2);
        ymm5 = _mm256_and_si256(ymm5, ymm0);     /* (y2 <= x2 && dx > 0) */

        ymm7 = _mm256_and_si256(ymm5, ymm6);     /* (y2 <= x2 && dx > 0 && dy < 0) */
        ymm6 = _mm256_xor_si256(ymm6, _mm256_set1_epi32(-1));
        ymm5 = _mm256_and_si256(ymm5, ymm6);     /* (y2 <= x2 && dx > 0 && dy >= 0) */

        ymm6f = _mm256_add_ps(ymm6f, mmi2f(ymm4));
        ymm6f = _mm256_add_ps(ymm6f, mmi2f(ymm5));
        ymm6f = _mm256_sub_ps(ymm6f, mmi2f(ymm7));

        /* ang2 = (angf - 0.75*CM_CONST_PI)*10.504226 + 2.0f, convert to integer (truncate) */
        ymm6f = _mm256_sub_ps(ymm6f, *(__m256 *)fPi07);
        ymm6f = _mm256_mul_ps(ymm6f, *(__m256 *)fC10);
        ymm6f = _mm256_add_ps(ymm6f, *(__m256 *)fC02);
        ymm4  = _mm256_cvttps_epi32(ymm6f);

        /* if (dx == 0 && dy == 0) ang2 = 0; */
        ymm4 = _mm256_blendv_epi8(ymm4, _mm256_setzero_si256(), ymm1);

        /* store angle, range = [0,34] */
        _mm256_store_si256((__m256i *)(ang2 + 4*y), ymm4);
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

void MAKE_NAME(h265_ComputeRsCs_8u)(const unsigned char *src, Ipp32s pitch, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height)
{
    assert((size_t(src) & 0xf) == 0);
    assert(width == 64);

    __m256i diff;
    __m256i lineL, lineC;

    __m256i lineA0 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src-pitch+0)));
    __m256i lineA1 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src-pitch+16)));
    __m256i lineA2 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src-pitch+32)));
    __m256i lineA3 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src-pitch+48)));
    __m128i lineC128, lineL128;
    for (Ipp32s y = 0; y < height; y += 4, lcuRs += pitchRsCs, lcuCs += pitchRsCs) {
        __m256i cs0 = _mm256_setzero_si256();
        __m256i cs1 = _mm256_setzero_si256();
        __m256i cs2 = _mm256_setzero_si256();
        __m256i cs3 = _mm256_setzero_si256();
        __m256i rs0 = _mm256_setzero_si256();
        __m256i rs1 = _mm256_setzero_si256();
        __m256i rs2 = _mm256_setzero_si256();
        __m256i rs3 = _mm256_setzero_si256();
        for (Ipp32s yy = 0; yy < 4; yy++, src += pitch) {
            _mm_prefetch((const char*)(src-16+pitch), _MM_HINT_T0);
            lineL128 = _mm_load_si128((__m128i *)(src-16));
            lineC128 = _mm_load_si128((__m128i *)(src));
            lineL = _mm256_cvtepu8_epi16(_mm_alignr_epi8(lineC128, lineL128, 15));
            lineC = _mm256_cvtepu8_epi16(lineC128);
            diff = _mm256_sub_epi16(lineL, lineC);
            diff = _mm256_madd_epi16(diff, diff);
            cs0 = _mm256_add_epi32(cs0, diff);
            diff = _mm256_sub_epi16(lineA0, lineC);
            diff = _mm256_madd_epi16(diff, diff);
            rs0 = _mm256_add_epi32(rs0, diff);
            lineL128 = lineC128;
            lineA0 = lineC;

            lineC128 = _mm_load_si128((__m128i *)(src+16));
            lineL = _mm256_cvtepu8_epi16(_mm_alignr_epi8(lineC128, lineL128, 15));
            lineC = _mm256_cvtepu8_epi16(lineC128);
            diff = _mm256_sub_epi16(lineL, lineC);
            diff = _mm256_madd_epi16(diff, diff);
            cs1 = _mm256_add_epi32(cs1, diff);
            diff = _mm256_sub_epi16(lineA1, lineC);
            diff = _mm256_madd_epi16(diff, diff);
            rs1 = _mm256_add_epi32(rs1, diff);
            lineL128 = lineC128;
            lineA1 = lineC;

            lineC128 = _mm_load_si128((__m128i *)(src+32));
            lineL = _mm256_cvtepu8_epi16(_mm_alignr_epi8(lineC128, lineL128, 15));
            lineC = _mm256_cvtepu8_epi16(lineC128);
            diff = _mm256_sub_epi16(lineL, lineC);
            diff = _mm256_madd_epi16(diff, diff);
            cs2 = _mm256_add_epi32(cs2, diff);
            diff = _mm256_sub_epi16(lineA2, lineC);
            diff = _mm256_madd_epi16(diff, diff);
            rs2 = _mm256_add_epi32(rs2, diff);
            lineL128 = lineC128;
            lineA2 = lineC;

            lineC128 = _mm_load_si128((__m128i *)(src+48));
            lineL = _mm256_cvtepu8_epi16(_mm_alignr_epi8(lineC128, lineL128, 15));
            lineC = _mm256_cvtepu8_epi16(lineC128);
            diff = _mm256_sub_epi16(lineL, lineC);
            diff = _mm256_madd_epi16(diff, diff);
            cs3 = _mm256_add_epi32(cs3, diff);
            diff = _mm256_sub_epi16(lineA3, lineC);
            diff = _mm256_madd_epi16(diff, diff);
            rs3 = _mm256_add_epi32(rs3, diff);
            lineL128 = lineC128;
            lineA3 = lineC;
        }

        cs0 = _mm256_hadd_epi32(cs0, cs1);
        cs0 = _mm256_permute4x64_epi64(cs0, 0xd8);
        cs0 = _mm256_srli_epi32(cs0, 4);
        _mm256_storeu_si256((__m256i *)(lcuCs), cs0);

        cs2 = _mm256_hadd_epi32(cs2, cs3);
        cs2 = _mm256_permute4x64_epi64(cs2, 0xd8);
        cs2 = _mm256_srli_epi32(cs2, 4);
        _mm256_storeu_si256((__m256i *)(lcuCs+8), cs2);

        rs0 = _mm256_hadd_epi32(rs0, rs1);
        rs0 = _mm256_permute4x64_epi64(rs0, 0xd8);
        rs0 = _mm256_srli_epi32(rs0, 4);
        _mm256_storeu_si256((__m256i *)(lcuRs), rs0);

        rs2 = _mm256_hadd_epi32(rs2, rs3);
        rs2 = _mm256_permute4x64_epi64(rs2, 0xd8);
        rs2 = _mm256_srli_epi32(rs2, 4);
        _mm256_storeu_si256((__m256i *)(lcuRs+8), rs2);
    }
}


#define _mm_loadh_epi64(a, ptr) _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(a), (__m64 *)(ptr)))
#define _mm_movehl_epi64(a, b) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b)))

// Load 0..31 bytes to YMM register from memory
// NOTE: elements of YMM are permuted [ 16 8 4 2 - 1 ]
template <char init>
static inline __m256i LoadPartialYmm(Ipp8u *pSrc, Ipp32s len)
{
    __m128i xlo = _mm_set1_epi8(init);
    __m128i xhi = _mm_set1_epi8(init);
    if (len & 16) {
        xhi = _mm_loadu_si128((__m128i *)pSrc);
        pSrc += 16;
    }
    if (len & 8) {
        xlo = _mm_loadh_epi64(xlo, (__m64 *)pSrc);
        pSrc += 8;
    }
    if (len & 4) {
        xlo = _mm_insert_epi32(xlo, *((Ipp32s *)pSrc), 1);
        pSrc += 4;
    }
    if (len & 2) {
        xlo = _mm_insert_epi16(xlo, *((short *)pSrc), 1);
        pSrc += 2;
    }
    if (len & 1) {
        xlo = _mm_insert_epi8(xlo, *pSrc, 0);
    }
    return _mm256_inserti128_si256(_mm256_castsi128_si256(xlo), xhi, 1);
}

void MAKE_NAME(h265_ImageDiffHistogram_8u)(Ipp8u* pSrc, Ipp8u* pRef, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s histogram[5], Ipp64s *pSrcDC, Ipp64s *pRefDC)
{
    enum {HIST_THRESH_LO = 4, HIST_THRESH_HI = 12};
    __m256i sDC = _mm256_setzero_si256();
    __m256i rDC = _mm256_setzero_si256();

    __m256i h0 = _mm256_setzero_si256();
    __m256i h1 = _mm256_setzero_si256();
    __m256i h2 = _mm256_setzero_si256();
    __m256i h3 = _mm256_setzero_si256();

    __m256i zero = _mm256_setzero_si256();

    for (Ipp32s i = 0; i < height; i++)
    {
        // process 32 pixels per iteration
        Ipp32s j;
        for (j = 0; j < width - 31; j += 32)
        {
            __m256i s = _mm256_loadu_si256((__m256i *)(&pSrc[j]));
            __m256i r = _mm256_loadu_si256((__m256i *)(&pRef[j]));

            sDC = _mm256_add_epi64(sDC, _mm256_sad_epu8(s, zero));    //accumulate horizontal sums
            rDC = _mm256_add_epi64(rDC, _mm256_sad_epu8(r, zero));

            r = _mm256_sub_epi8(r, _mm256_set1_epi8(-128));   // convert to signed
            s = _mm256_sub_epi8(s, _mm256_set1_epi8(-128));

            __m256i dn = _mm256_subs_epi8(r, s);   // -d saturated to [-128,127]
            __m256i dp = _mm256_subs_epi8(s, r);   // +d saturated to [-128,127]

            __m256i m0 = _mm256_cmpgt_epi8(dn, _mm256_set1_epi8(HIST_THRESH_HI)); // d < -12
            __m256i m1 = _mm256_cmpgt_epi8(dn, _mm256_set1_epi8(HIST_THRESH_LO)); // d < -4
            __m256i m2 = _mm256_cmpgt_epi8(_mm256_set1_epi8(HIST_THRESH_LO), dp); // d < +4
            __m256i m3 = _mm256_cmpgt_epi8(_mm256_set1_epi8(HIST_THRESH_HI), dp); // d < +12

            m0 = _mm256_sub_epi8(zero, m0);    // negate masks from 0xff to 1
            m1 = _mm256_sub_epi8(zero, m1);
            m2 = _mm256_sub_epi8(zero, m2);
            m3 = _mm256_sub_epi8(zero, m3);

            h0 = _mm256_add_epi32(h0, _mm256_sad_epu8(m0, zero)); // accumulate horizontal sums
            h1 = _mm256_add_epi32(h1, _mm256_sad_epu8(m1, zero));
            h2 = _mm256_add_epi32(h2, _mm256_sad_epu8(m2, zero));
            h3 = _mm256_add_epi32(h3, _mm256_sad_epu8(m3, zero));
        }

        // process remaining 1..31 pixels
        if (j < width)
        {
            __m256i s = LoadPartialYmm<0>(&pSrc[j], width & 0x1f);
            __m256i r = LoadPartialYmm<0>(&pRef[j], width & 0x1f);

            sDC = _mm256_add_epi64(sDC, _mm256_sad_epu8(s, zero));    //accumulate horizontal sums
            rDC = _mm256_add_epi64(rDC, _mm256_sad_epu8(r, zero));

            s = LoadPartialYmm<-1>(&pSrc[j], width & 0x1f);   // ensure unused elements not counted

            r = _mm256_sub_epi8(r, _mm256_set1_epi8(-128));   // convert to signed
            s = _mm256_sub_epi8(s, _mm256_set1_epi8(-128));

            __m256i dn = _mm256_subs_epi8(r, s);   // -d saturated to [-128,127]
            __m256i dp = _mm256_subs_epi8(s, r);   // +d saturated to [-128,127]

            __m256i m0 = _mm256_cmpgt_epi8(dn, _mm256_set1_epi8(HIST_THRESH_HI)); // d < -12
            __m256i m1 = _mm256_cmpgt_epi8(dn, _mm256_set1_epi8(HIST_THRESH_LO)); // d < -4
            __m256i m2 = _mm256_cmpgt_epi8(_mm256_set1_epi8(HIST_THRESH_LO), dp); // d < +4
            __m256i m3 = _mm256_cmpgt_epi8(_mm256_set1_epi8(HIST_THRESH_HI), dp); // d < +12

            m0 = _mm256_sub_epi8(zero, m0);    // negate masks from 0xff to 1
            m1 = _mm256_sub_epi8(zero, m1);
            m2 = _mm256_sub_epi8(zero, m2);
            m3 = _mm256_sub_epi8(zero, m3);

            h0 = _mm256_add_epi32(h0, _mm256_sad_epu8(m0, zero)); // accumulate horizontal sums
            h1 = _mm256_add_epi32(h1, _mm256_sad_epu8(m1, zero));
            h2 = _mm256_add_epi32(h2, _mm256_sad_epu8(m2, zero));
            h3 = _mm256_add_epi32(h3, _mm256_sad_epu8(m3, zero));
        }
        pSrc += pitch;
        pRef += pitch;
    }

    // finish horizontal sums
    __m128i tsDC = _mm_add_epi64(_mm256_castsi256_si128(sDC), _mm256_extractf128_si256(sDC, 1));
    __m128i trDC = _mm_add_epi64(_mm256_castsi256_si128(rDC), _mm256_extractf128_si256(rDC, 1));
    tsDC = _mm_add_epi64(tsDC, _mm_movehl_epi64(tsDC, tsDC));
    trDC = _mm_add_epi64(trDC, _mm_movehl_epi64(trDC, trDC));

    __m128i th0 = _mm_add_epi32(_mm256_castsi256_si128(h0), _mm256_extractf128_si256(h0, 1));
    __m128i th1 = _mm_add_epi32(_mm256_castsi256_si128(h1), _mm256_extractf128_si256(h1, 1));
    __m128i th2 = _mm_add_epi32(_mm256_castsi256_si128(h2), _mm256_extractf128_si256(h2, 1));
    __m128i th3 = _mm_add_epi32(_mm256_castsi256_si128(h3), _mm256_extractf128_si256(h3, 1));
    th0 = _mm_add_epi32(th0, _mm_movehl_epi64(th0, th0));
    th1 = _mm_add_epi32(th1, _mm_movehl_epi64(th1, th1));
    th2 = _mm_add_epi32(th2, _mm_movehl_epi64(th2, th2));
    th3 = _mm_add_epi32(th3, _mm_movehl_epi64(th3, th3));

    _mm_storel_epi64((__m128i *)pSrcDC, tsDC);
    _mm_storel_epi64((__m128i *)pRefDC, trDC);

    histogram[0] = _mm_cvtsi128_si32(th0);
    histogram[1] = _mm_cvtsi128_si32(th1);
    histogram[2] = _mm_cvtsi128_si32(th2);
    histogram[3] = _mm_cvtsi128_si32(th3);
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
    __m256i
        s0 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[0*pitch]), (__m128i *)&pSrc[1*pitch])),
        s1 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[2*pitch]), (__m128i *)&pSrc[3*pitch])),
        s2 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[4*pitch]), (__m128i *)&pSrc[5*pitch])),
        s3 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[6*pitch]), (__m128i *)&pSrc[7*pitch]));

    for (Ipp32s y = 0; y < yrange; y += SAD_SEARCH_VSTEP) {
        for (Ipp32s x = 0; x < xrange; x += 8) {
            Ipp8u* pr = pRef + (y * pitch) + x;
            __m256i
                r0 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[0*pitch])),
                r1 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[1*pitch])),
                r2 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[2*pitch])),
                r3 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[3*pitch])),
                r4 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[4*pitch])),
                r5 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[5*pitch])),
                r6 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[6*pitch])),
                r7 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[7*pitch]));
            r0 = _mm256_mpsadbw_epu8(r0, s0, 0x28);
            r1 = _mm256_mpsadbw_epu8(r1, s0, 0x3a);
            r2 = _mm256_mpsadbw_epu8(r2, s1, 0x28);
            r3 = _mm256_mpsadbw_epu8(r3, s1, 0x3a);
            r4 = _mm256_mpsadbw_epu8(r4, s2, 0x28);
            r5 = _mm256_mpsadbw_epu8(r5, s2, 0x3a);
            r6 = _mm256_mpsadbw_epu8(r6, s3, 0x28);
            r7 = _mm256_mpsadbw_epu8(r7, s3, 0x3a);
            r0 = _mm256_add_epi16(r0, r1);
            r2 = _mm256_add_epi16(r2, r3);
            r4 = _mm256_add_epi16(r4, r5);
            r6 = _mm256_add_epi16(r6, r7);
            r0 = _mm256_add_epi16(r0, r2);
            r4 = _mm256_add_epi16(r4, r6);
            r0 = _mm256_add_epi16(r0, r4);
            // horizontal sum
            __m128i t = _mm_add_epi16(_mm256_castsi256_si128(r0), _mm256_extractf128_si256(r0, 1));
            // kill out-of-bound values
            if (xrange - x < 8)
                t = _mm_or_si128(t, _mm_load_si128((__m128i *)tab_killmask[xrange - x]));
            t = _mm_minpos_epu16(t);
            Ipp32u SAD = _mm_extract_epi16(t, 0);
            if (SAD < *bestSAD) {
                *bestSAD = SAD;
                *bestX = x + _mm_extract_epi16(t, 1);
                *bestY = y;
            }
        }
    }
}

//// Load 0..31 bytes to YMM register from memory
//    // NOTE: elements of YMM are permuted [ 16 8 4 2 - 1 ]
//    template <char init>
//    static inline __m256i LoadPartialYmm(Ipp8u *pSrc, Ipp32s len)
//    {
//        __m128i xlo = _mm_set1_epi8(init);
//        __m128i xhi = _mm_set1_epi8(init);
//        if (len & 16) {
//            xhi = _mm_loadu_si128((__m128i *)pSrc);
//            pSrc += 16;
//        }
//        if (len & 8) {
//            xlo = _mm_loadh_epi64(xlo, (__m64 *)pSrc);
//            pSrc += 8;
//        }
//        if (len & 4) {
//            xlo = _mm_insert_epi32(xlo, *((Ipp32s *)pSrc), 1);
//            pSrc += 4;
//        }
//        if (len & 2) {
//            xlo = _mm_insert_epi16(xlo, *((short *)pSrc), 1);
//            pSrc += 2;
//        }
//        if (len & 1) {
//            xlo = _mm_insert_epi8(xlo, *pSrc, 0);
//        }
//        return _mm256_inserti128_si256(_mm256_castsi128_si256(xlo), xhi, 1);
//    }

// Load 0..7 floats to YMM register from memory
// NOTE: elements of YMM are permuted [ 4 2 - 1 ]
static inline __m256 LoadPartialYmm(Ipp32f *pSrc, Ipp32s len)
{
    __m128 xlo = _mm_setzero_ps();
    __m128 xhi = _mm_setzero_ps();
    if (len & 4) {
        xhi = _mm_loadu_ps(pSrc);
        pSrc += 4;
    }
    if (len & 2) {
        xlo = _mm_loadh_pi(xlo, (__m64 *)pSrc);
        pSrc += 2;
    }
    if (len & 1) {
        xlo = _mm_move_ss(xlo, _mm_load_ss(pSrc));
    }
    return _mm256_insertf128_ps(_mm256_castps128_ps256(xlo), xhi, 1);
}

void MAKE_NAME(h265_ComputeRsCsDiff)(Ipp32f* pRs0, Ipp32f* pCs0, Ipp32f* pRs1, Ipp32f* pCs1, Ipp32s len, Ipp32f* pRsDiff, Ipp32f* pCsDiff)
{
    Ipp32s i;//, len = wblocks * hblocks;
    __m256d accRs = _mm256_setzero_pd();
    __m256d accCs = _mm256_setzero_pd();

    for (i = 0; i < len - 7; i += 8)
    {
        __m256 rs = _mm256_sub_ps(_mm256_loadu_ps(&pRs0[i]), _mm256_loadu_ps(&pRs1[i]));
        __m256 cs = _mm256_sub_ps(_mm256_loadu_ps(&pCs0[i]), _mm256_loadu_ps(&pCs1[i]));

        rs = _mm256_mul_ps(rs, rs);
        cs = _mm256_mul_ps(cs, cs);

        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_castps256_ps128(rs)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_castps256_ps128(cs)));
        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_extractf128_ps(rs, 1)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_extractf128_ps(cs, 1)));
    }

    if (i < len)
    {
        __m256 rs = _mm256_sub_ps(LoadPartialYmm(&pRs0[i], len & 0x7), LoadPartialYmm(&pRs1[i], len & 0x7));
        __m256 cs = _mm256_sub_ps(LoadPartialYmm(&pCs0[i], len & 0x7), LoadPartialYmm(&pCs1[i], len & 0x7));

        rs = _mm256_mul_ps(rs, rs);
        cs = _mm256_mul_ps(cs, cs);

        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_castps256_ps128(rs)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_castps256_ps128(cs)));
        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_extractf128_ps(rs, 1)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_extractf128_ps(cs, 1)));
    }

    // horizontal sum
    accRs = _mm256_hadd_pd(accRs, accCs);
    __m128d s = _mm_add_pd(_mm256_castpd256_pd128(accRs), _mm256_extractf128_pd(accRs, 1));

    __m128 t = _mm_cvtpd_ps(s);
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
            __m256i rs = _mm256_setzero_si256();
            __m256i cs = _mm256_setzero_si256();
            __m256i a0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[-srcPitch + 0]));

#ifdef __INTEL_COMPILER
#pragma unroll(4)
#endif
            for (Ipp32s k = 0; k < 4; k++)
            {
                __m256i b0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[-1]));
                __m256i c0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[0]));
                pSrc += srcPitch;

                // accRs += dRs * dRs
                a0 = _mm256_sub_epi16(c0, a0);
                a0 = _mm256_madd_epi16(a0, a0);
                rs = _mm256_add_epi32(rs, a0);

                // accCs += dCs * dCs
                b0 = _mm256_sub_epi16(c0, b0);
                b0 = _mm256_madd_epi16(b0, b0);
                cs = _mm256_add_epi32(cs, b0);

                // reuse next iteration
                a0 = c0;
            }

            // horizontal sum
            rs = _mm256_hadd_epi32(rs, cs);
            rs = _mm256_permute4x64_epi64(rs, _MM_SHUFFLE(3, 1, 2, 0));    // [ cs3 cs2 cs1 cs0 rs3 rs2 rs1 rs0 ]

            // scale by 1.0f/N and store
            __m256 t = _mm256_mul_ps(_mm256_cvtepi32_ps(rs), _mm256_set1_ps(1.0f / 16));
            _mm_storeu_ps(&pRs[i * wblocks + j], _mm256_extractf128_ps(t, 0));
            _mm_storeu_ps(&pCs[i * wblocks + j], _mm256_extractf128_ps(t, 1));

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

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
