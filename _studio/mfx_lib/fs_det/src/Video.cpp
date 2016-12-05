//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: Video.c
 *
 * Structures and routines for basic video processing
 * 
 ********************************************************************************/
#include <assert.h>
#include "Video.h"
#include <immintrin.h>

#define _mm_loadh_epi64(a, ptr)     _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(a), (__m64 *)(ptr)))
#define _mm_storeh_epi64(ptr, a)    _mm_storeh_pi((__m64 *)(ptr), _mm_castsi128_ps(a))

//Normalization step for Yrg
static inline int NORM(int val, int sum)
{
    int i = (val * 255) / sum;
    return (i > 255) ? 255 : ((i < 0) ? 0 : i);
}

// 12-bit approximation to match SIMD results
static inline int NORM_SSE4(int val, int sum)
{
    int i = _mm_cvtss_si32(_mm_mul_ss(_mm_set_ss((float)val), _mm_mul_ss(_mm_rcp_ss(_mm_set_ss((float)sum)), _mm_set_ss(255.0f))));
    return (i > 255) ? 255 : ((i < 0) ? 0 : i);
}

static void ConvertColorSpaces444_C(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch)
{
    const BYTE* py0 = pYUV;
    const BYTE* pu0 = pYUV + iW*iH;
    const BYTE* pv0 = pYUV + 2*iW*iH;

    BYTE* py = pSkinColorSpace;
    BYTE* pr = pSkinColorSpace + iW*iH;
    BYTE* pg = pSkinColorSpace + 2*iW*iH;

    int kVR = (int)(1.140f * 4096.0f + 0.5f); // Q12
    int kVG = (int)(0.581f * 4096.0f + 0.5f);
    int kUG = (int)(0.395f * 4096.0f + 0.5f);
    int kUB = (int)(2.032f * 4096.0f + 0.5f);

    for (int i = 0; i < iH; i++)
    {
        for (int j = 0; j < iW; j++)
        {
            // load from YUV444
            int y = py0[i*pitch + j];
            int u = pu0[i*pitch + j];
            int v = pv0[i*pitch + j];

            // store Y to Yrg
            py[i*pitch + j] = y;
            
            u -= 128;
            v -= 128;

            //
            // convert to RGB
            //
            y <<= 3;    // Q11
            u <<= 7;    // Q15
            v <<= 7;

            int rr = (v * kVR) >> 16;
            int gg = ((v * kVG) >> 16) + ((u * kUG) >> 16);
            int bb = (u * kUB) >> 16;

            int r = y + rr; // Q11
            int g = y - gg;
            int b = y + bb;

            //
            // convert to Yrg
            //
            b += r + g;
            b = MAX(b, 1);

            pr[i*pitch + j] = NORM(r, b);
            pg[i*pitch + j] = NORM(g, b);
        }
    }
}

static void ConvertColorSpaces444_slice_C(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch, int slice_offset, int slice_nlines)
{
    const BYTE* py0 = pYUV + slice_offset;
    const BYTE* pu0 = pYUV + iW*iH + slice_offset;
    const BYTE* pv0 = pYUV + 2*iW*iH + slice_offset;

    BYTE* py = pSkinColorSpace + slice_offset;
    BYTE* pr = pSkinColorSpace + iW*iH + slice_offset;
    BYTE* pg = pSkinColorSpace + 2*iW*iH + slice_offset;

    int kVR = (int)(1.140f * 4096.0f + 0.5f); // Q12
    int kVG = (int)(0.581f * 4096.0f + 0.5f);
    int kUG = (int)(0.395f * 4096.0f + 0.5f);
    int kUB = (int)(2.032f * 4096.0f + 0.5f);

    for (int i = 0; i < slice_nlines; i++)
    {
        for (int j = 0; j < iW; j++)
        {
            // load from YUV444
            int y = py0[i*pitch + j];
            int u = pu0[i*pitch + j];
            int v = pv0[i*pitch + j];

            // store Y to Yrg
            py[i*pitch + j] = y;
            
            u -= 128;
            v -= 128;

            //
            // convert to RGB
            //
            y <<= 3;    // Q11
            u <<= 7;    // Q15
            v <<= 7;

            int rr = (v * kVR) >> 16;
            int gg = ((v * kVG) >> 16) + ((u * kUG) >> 16);
            int bb = (u * kUB) >> 16;

            int r = y + rr; // Q11
            int g = y - gg;
            int b = y + bb;

            //
            // convert to Yrg
            //
            b += r + g;
            b = MAX(b, 1);

            pr[i*pitch + j] = NORM(r, b);
            pg[i*pitch + j] = NORM(g, b);
        }
    }
}

static void ConvertColorSpaces444_SSE4(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch)
{
    const BYTE* py0 = pYUV;
    const BYTE* pu0 = pYUV + iW*iH;
    const BYTE* pv0 = pYUV + 2*iW*iH;

    BYTE* py = pSkinColorSpace;
    BYTE* pr = pSkinColorSpace + iW*iH;
    BYTE* pg = pSkinColorSpace + 2*iW*iH;

    __m128i kVR = _mm_set1_epi16((short)(1.140f * 4096.0f + 0.5f)); // Q12
    __m128i kVG = _mm_set1_epi16((short)(0.581f * 4096.0f + 0.5f));
    __m128i kUG = _mm_set1_epi16((short)(0.395f * 4096.0f + 0.5f));
    __m128i kUB = _mm_set1_epi16((short)(2.032f * 4096.0f + 0.5f));

    int kVRi = (int)(1.140f * 4096.0f + 0.5f); // Q12
    int kVGi = (int)(0.581f * 4096.0f + 0.5f);
    int kUGi = (int)(0.395f * 4096.0f + 0.5f);
    int kUBi = (int)(2.032f * 4096.0f + 0.5f);

    for (int i = 0; i < iH; i++)
    {
        int j;
        for (j = 0; j < iW - 7; j += 8)
        {
            // load from YUV444
            __m128i y = _mm_loadl_epi64((__m128i *)&py0[i*pitch + j]); // 8
            __m128i u = _mm_loadl_epi64((__m128i *)&pu0[i*pitch + j]); // 8
            __m128i v = _mm_loadl_epi64((__m128i *)&pv0[i*pitch + j]); // 8

            // store Y to Yrg
            _mm_storel_epi64((__m128i *)&py[i*pitch + j], y);

            u = _mm_add_epi8(u, _mm_set1_epi8(-128));   // u -= 128
            v = _mm_add_epi8(v, _mm_set1_epi8(-128));   // v -= 128

            //
            // convert to RGB (1x8 block)
            //
            __m128i mY = _mm_slli_epi16(_mm_cvtepu8_epi16(y), 3);   // Q11
            __m128i mU = _mm_slli_epi16(_mm_cvtepi8_epi16(u), 7);   // Q15
            __m128i mV = _mm_slli_epi16(_mm_cvtepi8_epi16(v), 7);

            __m128i rr = _mm_mulhi_epi16(mV, kVR);  // Q11
            __m128i gg = _mm_adds_epi16(_mm_mulhi_epi16(mV, kVG), _mm_mulhi_epi16(mU, kUG));
            __m128i bb = _mm_mulhi_epi16(mU, kUB);

            __m128i mR = _mm_adds_epi16(mY, rr);    // Q11
            __m128i mG = _mm_subs_epi16(mY, gg);
            __m128i mB = _mm_adds_epi16(mY, bb);

            //
            // convert to Yrg (1x8 block)
            //
            mB = _mm_adds_epi16(mB, mR);
            mB = _mm_adds_epi16(mB, mG);
            mB = _mm_max_epi16(mB, _mm_set1_epi16(1));  // MAX(8r+8g+8b, 1)

            __m128 fRl = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(mR));   // Q11 to float
            __m128 fRh = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(_mm_unpackhi_epi64(mR, mR)));
            __m128 fGl = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(mG));
            __m128 fGh = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(_mm_unpackhi_epi64(mG, mG)));
            __m128 fBl = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(mB));
            __m128 fBh = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(_mm_unpackhi_epi64(mB, mB)));

            fBl = _mm_mul_ps(_mm_rcp_ps(fBl), _mm_set1_ps(255.0f)); // 12-bit approximation of 255/(8r+8g+8b)
            fBh = _mm_mul_ps(_mm_rcp_ps(fBh), _mm_set1_ps(255.0f));

            fRl = _mm_mul_ps(fRl, fBl); // 8r * 255/(8r+8g+8b)
            fRh = _mm_mul_ps(fRh, fBh);
            fGl = _mm_mul_ps(fGl, fBl);
            fGh = _mm_mul_ps(fGh, fBh);

            mR = _mm_packs_epi32(_mm_cvtps_epi32(fRl), _mm_cvtps_epi32(fRh));
            mG = _mm_packs_epi32(_mm_cvtps_epi32(fGl), _mm_cvtps_epi32(fGh));

            mR = _mm_packus_epi16(mR, mG);  // 16 x 8-bit

            _mm_storel_epi64((__m128i *)&pr[i*pitch + j], mR);
            _mm_storeh_epi64((__m128i *)&pg[i*pitch + j], mR);
        }

        for (; j < iW; j++)
        {
            // load from YUV444
            int y = py0[i*pitch + j];
            int u = pu0[i*pitch + j];
            int v = pv0[i*pitch + j];

            // store Y to Yrg
            py[i*pitch + j] = y;
            
            u -= 128;
            v -= 128;

            //
            // convert to RGB
            //
            y <<= 3;    // Q11
            u <<= 7;    // Q15
            v <<= 7;

            int rr = (v * kVRi) >> 16;
            int gg = ((v * kVGi) >> 16) + ((u * kUGi) >> 16);
            int bb = (u * kUBi) >> 16;

            int r = y + rr; // Q11
            int g = y - gg;
            int b = y + bb;

            //
            // convert to Yrg
            //
            b += r + g;
            b = MAX(b, 1);

            pr[i*pitch + j] = NORM_SSE4(r, b);
            pg[i*pitch + j] = NORM_SSE4(g, b);
        }
    }
}

static void ConvertColorSpaces444_slice_SSE4(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch, int slice_offset, int slice_nlines)
{
    const BYTE* py0 = pYUV + slice_offset;
    const BYTE* pu0 = pYUV + iW*iH + slice_offset;
    const BYTE* pv0 = pYUV + 2*iW*iH + slice_offset;

    BYTE* py = pSkinColorSpace + slice_offset;
    BYTE* pr = pSkinColorSpace + iW*iH + slice_offset;
    BYTE* pg = pSkinColorSpace + 2*iW*iH + slice_offset;

    __m128i kVR = _mm_set1_epi16((short)(1.140f * 4096.0f + 0.5f)); // Q12
    __m128i kVG = _mm_set1_epi16((short)(0.581f * 4096.0f + 0.5f));
    __m128i kUG = _mm_set1_epi16((short)(0.395f * 4096.0f + 0.5f));
    __m128i kUB = _mm_set1_epi16((short)(2.032f * 4096.0f + 0.5f));

    int kVRi = (int)(1.140f * 4096.0f + 0.5f); // Q12
    int kVGi = (int)(0.581f * 4096.0f + 0.5f);
    int kUGi = (int)(0.395f * 4096.0f + 0.5f);
    int kUBi = (int)(2.032f * 4096.0f + 0.5f);

    for (int i = 0; i < slice_nlines; i++)
    {
        int j;
        for (j = 0; j < iW - 7; j += 8)
        {
            // load from YUV444
            __m128i y = _mm_loadl_epi64((__m128i *)&py0[i*pitch + j]); // 8
            __m128i u = _mm_loadl_epi64((__m128i *)&pu0[i*pitch + j]); // 8
            __m128i v = _mm_loadl_epi64((__m128i *)&pv0[i*pitch + j]); // 8

            // store Y to Yrg
            _mm_storel_epi64((__m128i *)&py[i*pitch + j], y);

            u = _mm_add_epi8(u, _mm_set1_epi8(-128));   // u -= 128
            v = _mm_add_epi8(v, _mm_set1_epi8(-128));   // v -= 128

            //
            // convert to RGB (1x8 block)
            //
            __m128i mY = _mm_slli_epi16(_mm_cvtepu8_epi16(y), 3);   // Q11
            __m128i mU = _mm_slli_epi16(_mm_cvtepi8_epi16(u), 7);   // Q15
            __m128i mV = _mm_slli_epi16(_mm_cvtepi8_epi16(v), 7);

            __m128i rr = _mm_mulhi_epi16(mV, kVR);  // Q11
            __m128i gg = _mm_adds_epi16(_mm_mulhi_epi16(mV, kVG), _mm_mulhi_epi16(mU, kUG));
            __m128i bb = _mm_mulhi_epi16(mU, kUB);

            __m128i mR = _mm_adds_epi16(mY, rr);    // Q11
            __m128i mG = _mm_subs_epi16(mY, gg);
            __m128i mB = _mm_adds_epi16(mY, bb);

            //
            // convert to Yrg (1x8 block)
            //
            mB = _mm_adds_epi16(mB, mR);
            mB = _mm_adds_epi16(mB, mG);
            mB = _mm_max_epi16(mB, _mm_set1_epi16(1));  // MAX(8r+8g+8b, 1)

            __m128 fRl = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(mR));   // Q11 to float
            __m128 fRh = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(_mm_unpackhi_epi64(mR, mR)));
            __m128 fGl = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(mG));
            __m128 fGh = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(_mm_unpackhi_epi64(mG, mG)));
            __m128 fBl = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(mB));
            __m128 fBh = _mm_cvtepi32_ps(_mm_cvtepu16_epi32(_mm_unpackhi_epi64(mB, mB)));

            fBl = _mm_mul_ps(_mm_rcp_ps(fBl), _mm_set1_ps(255.0f)); // 12-bit approximation of 255/(8r+8g+8b)
            fBh = _mm_mul_ps(_mm_rcp_ps(fBh), _mm_set1_ps(255.0f));

            fRl = _mm_mul_ps(fRl, fBl); // 8r * 255/(8r+8g+8b)
            fRh = _mm_mul_ps(fRh, fBh);
            fGl = _mm_mul_ps(fGl, fBl);
            fGh = _mm_mul_ps(fGh, fBh);

            mR = _mm_packs_epi32(_mm_cvtps_epi32(fRl), _mm_cvtps_epi32(fRh));
            mG = _mm_packs_epi32(_mm_cvtps_epi32(fGl), _mm_cvtps_epi32(fGh));

            mR = _mm_packus_epi16(mR, mG);  // 16 x 8-bit

            _mm_storel_epi64((__m128i *)&pr[i*pitch + j], mR);
            _mm_storeh_epi64((__m128i *)&pg[i*pitch + j], mR);
        }

        for (; j < iW; j++)
        {
            // load from YUV444
            int y = py0[i*pitch + j];
            int u = pu0[i*pitch + j];
            int v = pv0[i*pitch + j];

            // store Y to Yrg
            py[i*pitch + j] = y;
            
            u -= 128;
            v -= 128;

            //
            // convert to RGB
            //
            y <<= 3;    // Q11
            u <<= 7;    // Q15
            v <<= 7;

            int rr = (v * kVRi) >> 16;
            int gg = ((v * kVGi) >> 16) + ((u * kUGi) >> 16);
            int bb = (u * kUBi) >> 16;

            int r = y + rr; // Q11
            int g = y - gg;
            int b = y + bb;

            //
            // convert to Yrg
            //
            b += r + g;
            b = MAX(b, 1);

            pr[i*pitch + j] = NORM_SSE4(r, b);
            pg[i*pitch + j] = NORM_SSE4(g, b);
        }
    }
}

static void ConvertColorSpaces444_AVX2(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch)
{
    const BYTE* py0 = pYUV;
    const BYTE* pu0 = pYUV + iW*iH;
    const BYTE* pv0 = pYUV + 2*iW*iH;

    BYTE* py = pSkinColorSpace;
    BYTE* pr = pSkinColorSpace + iW*iH;
    BYTE* pg = pSkinColorSpace + 2*iW*iH;

    __m256i kVR = _mm256_set1_epi16((short)(1.140f * 4096.0f + 0.5f)); // Q12
    __m256i kVG = _mm256_set1_epi16((short)(0.581f * 4096.0f + 0.5f));
    __m256i kUG = _mm256_set1_epi16((short)(0.395f * 4096.0f + 0.5f));
    __m256i kUB = _mm256_set1_epi16((short)(2.032f * 4096.0f + 0.5f));

    int kVRi = (int)(1.140f * 4096.0f + 0.5f); // Q12
    int kVGi = (int)(0.581f * 4096.0f + 0.5f);
    int kUGi = (int)(0.395f * 4096.0f + 0.5f);
    int kUBi = (int)(2.032f * 4096.0f + 0.5f);

    for (int i = 0; i < iH; i++)
    {
        int j;
        for (j = 0; j < iW - 15; j += 16)
        {
            // load from YUV444
            __m128i y = _mm_loadu_si128((__m128i *)&py0[i*pitch + j]); // 16
            __m128i u = _mm_loadu_si128((__m128i *)&pu0[i*pitch + j]); // 16
            __m128i v = _mm_loadu_si128((__m128i *)&pv0[i*pitch + j]); // 16

            // store Y to Yrg
            _mm_storeu_si128((__m128i *)&py[i*pitch + j], y);

            u = _mm_add_epi8(u, _mm_set1_epi8(-128));   // u -= 128
            v = _mm_add_epi8(v, _mm_set1_epi8(-128));   // v -= 128

            //
            // convert to RGB (1x16 block)
            //
            __m256i mY = _mm256_slli_epi16(_mm256_cvtepu8_epi16(y), 3); // Q11
            __m256i mU = _mm256_slli_epi16(_mm256_cvtepi8_epi16(u), 7); // Q15
            __m256i mV = _mm256_slli_epi16(_mm256_cvtepi8_epi16(v), 7);

            __m256i rr = _mm256_mulhi_epi16(mV, kVR);   // Q11
            __m256i gg = _mm256_adds_epi16(_mm256_mulhi_epi16(mV, kVG), _mm256_mulhi_epi16(mU, kUG));
            __m256i bb = _mm256_mulhi_epi16(mU, kUB);

            __m256i mR = _mm256_adds_epi16(mY, rr);     // Q11
            __m256i mG = _mm256_subs_epi16(mY, gg);
            __m256i mB = _mm256_adds_epi16(mY, bb);

            //
            // convert to Yrg (1x16 block)
            //
            mB = _mm256_adds_epi16(mB, mR);
            mB = _mm256_adds_epi16(mB, mG);
            mB = _mm256_max_epi16(mB, _mm256_set1_epi16(1));    // MAX(8r+8g+8b, 1)

            __m256 fRl = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(mR))); // Q11 to float
            __m256 fRh = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_extractf128_si256(mR, 1)));
            __m256 fGl = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(mG)));
            __m256 fGh = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_extractf128_si256(mG, 1)));
            __m256 fBl = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(mB)));
            __m256 fBh = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_extractf128_si256(mB, 1)));

            fBl = _mm256_mul_ps(_mm256_rcp_ps(fBl), _mm256_set1_ps(255.0f));    // 12-bit approximation of 255/(8r+8g+8b)
            fBh = _mm256_mul_ps(_mm256_rcp_ps(fBh), _mm256_set1_ps(255.0f));

            fRl = _mm256_mul_ps(fRl, fBl);  // 8r * 255/(8r+8g+8b)
            fRh = _mm256_mul_ps(fRh, fBh);
            fGl = _mm256_mul_ps(fGl, fBl);
            fGh = _mm256_mul_ps(fGh, fBh);

            mR = _mm256_packs_epi32(_mm256_cvtps_epi32(fRl), _mm256_cvtps_epi32(fRh));
            mG = _mm256_packs_epi32(_mm256_cvtps_epi32(fGl), _mm256_cvtps_epi32(fGh));

            mR = _mm256_packus_epi16(mR, mG);   // 32 x 8-bit

            mR = _mm256_permutevar8x32_epi32(mR, _mm256_set_epi32(7,3,6,2,5,1,4,0));

            _mm_storeu_si128((__m128i *)&pr[i*pitch + j], _mm256_castsi256_si128(mR));      // lo
            _mm_storeu_si128((__m128i *)&pg[i*pitch + j], _mm256_extractf128_si256(mR, 1)); // hi
        }

        for (; j < iW; j++)
        {
            // load from YUV444
            int y = py0[i*pitch + j];
            int u = pu0[i*pitch + j];
            int v = pv0[i*pitch + j];

            // store Y to Yrg
            py[i*pitch + j] = y;
            
            u -= 128;
            v -= 128;

            //
            // convert to RGB
            //
            y <<= 3;    // Q11
            u <<= 7;    // Q15
            v <<= 7;

            int rr = (v * kVRi) >> 16;
            int gg = ((v * kVGi) >> 16) + ((u * kUGi) >> 16);
            int bb = (u * kUBi) >> 16;

            int r = y + rr; // Q11
            int g = y - gg;
            int b = y + bb;

            //
            // convert to Yrg
            //
            b += r + g;
            b = MAX(b, 1);

            pr[i*pitch + j] = NORM_SSE4(r, b);
            pg[i*pitch + j] = NORM_SSE4(g, b);
        }
    }
}

static void ConvertColorSpaces444_slice_AVX2(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch, int slice_offset, int slice_nlines)
{
    const BYTE* py0 = pYUV + slice_offset;
    const BYTE* pu0 = pYUV + iW*iH + slice_offset;
    const BYTE* pv0 = pYUV + 2*iW*iH + slice_offset;

    BYTE* py = pSkinColorSpace + slice_offset;
    BYTE* pr = pSkinColorSpace + iW*iH + slice_offset;
    BYTE* pg = pSkinColorSpace + 2*iW*iH + slice_offset;

    __m256i kVR = _mm256_set1_epi16((short)(1.140f * 4096.0f + 0.5f)); // Q12
    __m256i kVG = _mm256_set1_epi16((short)(0.581f * 4096.0f + 0.5f));
    __m256i kUG = _mm256_set1_epi16((short)(0.395f * 4096.0f + 0.5f));
    __m256i kUB = _mm256_set1_epi16((short)(2.032f * 4096.0f + 0.5f));

    int kVRi = (int)(1.140f * 4096.0f + 0.5f); // Q12
    int kVGi = (int)(0.581f * 4096.0f + 0.5f);
    int kUGi = (int)(0.395f * 4096.0f + 0.5f);
    int kUBi = (int)(2.032f * 4096.0f + 0.5f);

    for (int i = 0; i < slice_nlines; i++)
    {
        int j;
        for (j = 0; j < iW - 15; j += 16)
        {
            // load from YUV444
            __m128i y = _mm_loadu_si128((__m128i *)&py0[i*pitch + j]); // 16
            __m128i u = _mm_loadu_si128((__m128i *)&pu0[i*pitch + j]); // 16
            __m128i v = _mm_loadu_si128((__m128i *)&pv0[i*pitch + j]); // 16

            // store Y to Yrg
            _mm_storeu_si128((__m128i *)&py[i*pitch + j], y);

            u = _mm_add_epi8(u, _mm_set1_epi8(-128));   // u -= 128
            v = _mm_add_epi8(v, _mm_set1_epi8(-128));   // v -= 128

            //
            // convert to RGB (1x16 block)
            //
            __m256i mY = _mm256_slli_epi16(_mm256_cvtepu8_epi16(y), 3); // Q11
            __m256i mU = _mm256_slli_epi16(_mm256_cvtepi8_epi16(u), 7); // Q15
            __m256i mV = _mm256_slli_epi16(_mm256_cvtepi8_epi16(v), 7);

            __m256i rr = _mm256_mulhi_epi16(mV, kVR);   // Q11
            __m256i gg = _mm256_adds_epi16(_mm256_mulhi_epi16(mV, kVG), _mm256_mulhi_epi16(mU, kUG));
            __m256i bb = _mm256_mulhi_epi16(mU, kUB);

            __m256i mR = _mm256_adds_epi16(mY, rr);     // Q11
            __m256i mG = _mm256_subs_epi16(mY, gg);
            __m256i mB = _mm256_adds_epi16(mY, bb);

            //
            // convert to Yrg (1x16 block)
            //
            mB = _mm256_adds_epi16(mB, mR);
            mB = _mm256_adds_epi16(mB, mG);
            mB = _mm256_max_epi16(mB, _mm256_set1_epi16(1));    // MAX(8r+8g+8b, 1)

            __m256 fRl = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(mR))); // Q11 to float
            __m256 fRh = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_extractf128_si256(mR, 1)));
            __m256 fGl = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(mG)));
            __m256 fGh = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_extractf128_si256(mG, 1)));
            __m256 fBl = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_castsi256_si128(mB)));
            __m256 fBh = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(_mm256_extractf128_si256(mB, 1)));

            fBl = _mm256_mul_ps(_mm256_rcp_ps(fBl), _mm256_set1_ps(255.0f));    // 12-bit approximation of 255/(8r+8g+8b)
            fBh = _mm256_mul_ps(_mm256_rcp_ps(fBh), _mm256_set1_ps(255.0f));

            fRl = _mm256_mul_ps(fRl, fBl);  // 8r * 255/(8r+8g+8b)
            fRh = _mm256_mul_ps(fRh, fBh);
            fGl = _mm256_mul_ps(fGl, fBl);
            fGh = _mm256_mul_ps(fGh, fBh);

            mR = _mm256_packs_epi32(_mm256_cvtps_epi32(fRl), _mm256_cvtps_epi32(fRh));
            mG = _mm256_packs_epi32(_mm256_cvtps_epi32(fGl), _mm256_cvtps_epi32(fGh));

            mR = _mm256_packus_epi16(mR, mG);   // 32 x 8-bit

            mR = _mm256_permutevar8x32_epi32(mR, _mm256_set_epi32(7,3,6,2,5,1,4,0));

            _mm_storeu_si128((__m128i *)&pr[i*pitch + j], _mm256_castsi256_si128(mR));      // lo
            _mm_storeu_si128((__m128i *)&pg[i*pitch + j], _mm256_extractf128_si256(mR, 1)); // hi
        }

        for (; j < iW; j++)
        {
            // load from YUV444
            int y = py0[i*pitch + j];
            int u = pu0[i*pitch + j];
            int v = pv0[i*pitch + j];

            // store Y to Yrg
            py[i*pitch + j] = y;
            
            u -= 128;
            v -= 128;

            //
            // convert to RGB
            //
            y <<= 3;    // Q11
            u <<= 7;    // Q15
            v <<= 7;

            int rr = (v * kVRi) >> 16;
            int gg = ((v * kVGi) >> 16) + ((u * kUGi) >> 16);
            int bb = (u * kUBi) >> 16;

            int r = y + rr; // Q11
            int g = y - gg;
            int b = y + bb;

            //
            // convert to Yrg
            //
            b += r + g;
            b = MAX(b, 1);

            pr[i*pitch + j] = NORM_SSE4(r, b);
            pg[i*pitch + j] = NORM_SSE4(g, b);
        }
    }
}

typedef void(*t_ConvertColorSpaces444)(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch);
typedef void(*t_ConvertColorSpaces444_slice)(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch, int slice_offset, int slice_nlines);

void ConvertColorSpaces444(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch)
{
    t_ConvertColorSpaces444 f = g_FS_OPT_AVX2 ? ConvertColorSpaces444_AVX2: (g_FS_OPT_SSE4 ? ConvertColorSpaces444_SSE4 : ConvertColorSpaces444_C);
    (*f)(pYUV, pSkinColorSpace, iW, iH, pitch);
}

void ConvertColorSpaces444_slice(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch, int slice_offset, int slice_nlines)
{
    t_ConvertColorSpaces444_slice f = g_FS_OPT_AVX2 ? ConvertColorSpaces444_slice_AVX2: (g_FS_OPT_SSE4 ? ConvertColorSpaces444_slice_SSE4 : ConvertColorSpaces444_slice_C);
    (*f)(pYUV, pSkinColorSpace, iW, iH, pitch, slice_offset, slice_nlines);
}


//
// end Dispatcher
//
