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

/* AVX2 (CPU) implementation of fast gradient analysis for intra prediction
 * software fallback when Cm version not available
 */

#include <string.h>
#include <math.h>

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

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

static const float CM_CONST_PI   = 3.14159f;
static const float M_DBL_EPSILON = 0.00001f;

ALIGN_DECL(32) static const signed char dyShufTab08[32] = {0, 2, 1, 3, 2, 4, 3, 5, 0, 2, 1, 3, 2, 4, 3, 5, 0, 2, 1, 3, 2, 4, 3, 5, 0, 2, 1, 3, 2, 4, 3, 5}; 

ALIGN_DECL(32) static const signed char pm1Tab08[32] = {+1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1, +1, -1}; 
ALIGN_DECL(32) static const signed char pm2Tab08[32] = {+2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2, +2, -2}; 

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

static void BuildHistogramInner(const mfxU8 *frame, mfxU16 *hist, mfxI32 framePitch)
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

/* if requested location is outside of frame, return closest edge pixel */
static inline mfxU8 GetPel(const mfxU8 *frame, mfxI32 x, mfxI32 y, mfxI32 width, mfxI32 height, mfxI32 pitch)
{
    x = CLIPVAL(x, 0, width - 1);
    y = CLIPVAL(y, 0, height - 1);
    return frame[y*pitch + x];
}

/* single-precision float throughout */
static float atan2_fast(float y, float x, float xy, float x2, float y2)
{
    float a0, a1;

    if (y >= 0) {
        a0 = CM_CONST_PI * 0.5f;
        a1 = 0;
    } else {
        a0 = CM_CONST_PI * 1.5f;
        a1 = CM_CONST_PI * 2.0f;
    }

    if (x < 0) {
        a1 = CM_CONST_PI;
    }

    if (y2 <= x2) {
        a1 += (xy / (x2 + 0.28f * y2 + M_DBL_EPSILON));
        return a1;
    } else {
        a0 -= (xy / (y2 + 0.28f * x2 + M_DBL_EPSILON));
        return a0;
    }
}

static mfxI32 CalcAng2(mfxI16 dx, mfxI16 dy)
{
    float fdx, fdy, x2, y2, angf;
    mfxI32 ang2;

    fdx = (float)dx;
    fdy = (float)dy;
    y2 = fdy * fdy;
    x2 = fdx * fdx;

    /* assume (dx == 0 && dy == 0) returns 0 (M_DBL_EPSILON in denominator) */
    angf = atan2_fast(fdy, fdx, fdy*fdx, x2, y2);

    if (y2 > x2 && dy > 0) 
        angf += CM_CONST_PI;
    else if (y2 <= x2 && dx > 0 && dy >= 0) 
        angf += CM_CONST_PI;
    else if (y2 <= x2 && dx > 0 && dy < 0) 
        angf -= CM_CONST_PI;

    angf -= 0.75f * CM_CONST_PI;

    if (dx == 0 && dy == 0) 
        ang2 = 0;
    else
        ang2 = (mfxI32)(angf * 10.504226f + 2.0f);

    return ang2;
}

/* C reference version for handling blocks around edge of frame - could be avoided by padding input frame with 1-pixel border */
static void BuildHistogramEdge(const mfxU8 *frame, mfxU16 *hist, mfxI32 blockX, mfxI32 blockY, mfxI32 width, mfxI32 height, mfxI32 pitch)
{
    mfxI32 x, y, ampXY, ang2;
    mfxI16 dx, dy;

    for (y = blockY; y < blockY + 4; y++) {
        for (x = blockX; x < blockX + 4; x++) {
            dy = GetPel(frame, x-1, y-1, width, height, pitch) + 2*GetPel(frame, x-1, y+0, width, height, pitch) + GetPel(frame, x-1, y+1, width, height, pitch)
                -GetPel(frame, x+1, y-1, width, height, pitch) - 2*GetPel(frame, x+1, y+0, width, height, pitch) - GetPel(frame, x+1, y+1, width, height, pitch);
            dx = GetPel(frame, x-1, y+1, width, height, pitch) + 2*GetPel(frame, x+0, y+1, width, height, pitch) + GetPel(frame, x+1, y+1, width, height, pitch)
                -GetPel(frame, x-1, y-1, width, height, pitch) - 2*GetPel(frame, x+0, y-1, width, height, pitch) - GetPel(frame, x+1, y-1, width, height, pitch);
            ampXY = (abs(dx) + abs(dy));
            
            ang2 = CalcAng2(dx, dy);
            hist[ang2] += (mfxU16)ampXY;
        }
    }
}

/* sum 4x4 histograms to generate 8x8 map 
 * NOTE: outData8 assumed to fit in 16-bit unsigned (see comment on GPU implementation)
 *   but might be necessary to use 32-bit for 8x8 (consider max value of 64*(abs(dx)+abs(y)))
 * for now saturate to 2^16 - 1
 */
static void Generate8x8(const mfxU8 *inData, mfxU16 *outData4, mfxU16 *outData8, mfxI32 width, mfxI32 height)
{
    mfxI32 x, y, i, t, xBlocks4, yBlocks4, xBlocks8, yBlocks8;
    mfxU16 *pDst, *pDst8;

    __m128i xmm0, xmm1, xmm2, xmm3;

    xBlocks4 = width  / 4;
    yBlocks4 = height / 4;
    xBlocks8 = width  / 8;
    yBlocks8 = height / 8;

    for (y = 0; y < yBlocks8; y++) {
        for (x = 0; x < xBlocks8; x++) {
            pDst = outData4 + 2*(y*xBlocks4 + x) * HIST_MAX; 
            pDst8 = outData8 +   (y*xBlocks8 + x) * HIST_MAX; 

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
                pDst8[i] = (mfxU16)CLIPVAL(t, 0, (1 << 16) - 1);
            }
        }
    }
}

/* width and height must be multiples of 8 */
void MAKE_NAME(h265_AnalyzeGradient_8u)(const mfxU8 *inData, mfxU16 *outData4, mfxU16 *outData8, mfxI32 width, mfxI32 height, mfxI32 pitch)
{
    mfxI32 x, y, xBlocks4, yBlocks4;
    const mfxU8 *pSrc;
    mfxU16 *pDst;

    xBlocks4 = width  / 4;
    yBlocks4 = height / 4;

    /* fast path - inner blocks */
    for (y = 1; y < yBlocks4 - 1; y++) {
        pSrc  = inData + 4*(y*pitch + 1);
        pDst = outData4 + (y*xBlocks4 + 1) * HIST_MAX;
        for (x = 1; x < xBlocks4 - 1; x++) {
            memset(pDst, 0, sizeof(mfxU16) * HIST_MAX);
            BuildHistogramInner(pSrc, pDst, pitch);
            pSrc  += 4;
            pDst += HIST_MAX;
        }
    }

    /* top edge */
    y = 0;
    pDst = outData4;
    for (x = 0; x < xBlocks4; x++) {
        memset(pDst, 0, sizeof(mfxU16) * HIST_MAX);
        BuildHistogramEdge(inData, pDst, x*4, y*4, width, height, pitch);
        pDst += HIST_MAX;
    }

    /* bottom edge */
    y = yBlocks4 - 1;
    pDst = outData4 + y*xBlocks4 * HIST_MAX;
    for (x = 0; x < xBlocks4; x++) {
        memset(pDst, 0, sizeof(mfxU16) * HIST_MAX);
        BuildHistogramEdge(inData, pDst, x*4, y*4, width, height, pitch);
        pDst += HIST_MAX;
    }

    /* left edge (corners already done) */
    x = 0;
    for (y = 1; y < yBlocks4 - 1; y++) {
        pDst = outData4 + (y*xBlocks4 + x) * HIST_MAX;
        memset(pDst, 0, sizeof(mfxU16) * HIST_MAX);
        BuildHistogramEdge(inData, pDst, x*4, y*4, width, height, pitch);
    }

    /* right edge (corners already done) */
    x = xBlocks4 - 1;
    for (y = 1; y < yBlocks4 - 1; y++) {
        pDst = outData4 + (y*xBlocks4 + x) * HIST_MAX;
        memset(pDst, 0, sizeof(mfxU16) * HIST_MAX);
        BuildHistogramEdge(inData, pDst, x*4, y*4, width, height, pitch);
    }

    /* sum 4x4 histograms to generate 8x8 map */
    Generate8x8(inData, outData4, outData8, width, height);
}

}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
