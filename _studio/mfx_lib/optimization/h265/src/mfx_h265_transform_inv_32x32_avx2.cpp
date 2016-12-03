//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

// Inverse HEVC Transform functions optimised by intrinsics
// Size: 32x32

// Win32: reference = 15878, sse4.1 = 5169, avx = 5021, avx2 = 3404/2557 CPU clocks
// Win64: reference = 16777, sse4.1 = 4867, avx = 4777, avx2 = 3159/2377 CPU clocks
// Note: results after "/" - with _USE_REORDERING_AVX2_

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 
#include <immintrin.h> // AVX, AVX2

#pragma warning (disable : 4310 ) /* disable cast truncates constant value */

//#include <stdio.h>
//#include <windows.h>

//#define _USE_SHORTCUT_ 1
//#define _USE_REORDERING_AVX2_ 1
namespace MFX_HEVC_PP
{
#ifdef  _USE_REORDERING_AVX2_
    int b_use_reordering_avx2 = 1;
#else
    int b_use_reordering_avx2 = 0;
#endif

#define SHIFT_INV_2ND_BASE     12   // Base shift after second inverse transform stage, offset later by (bitDepth - 8) for > 8-bit data

#ifdef NDEBUG
#define MM_LOAD_EPI128(x) (*(__m128i*)x)
#else
#define MM_LOAD_EPI128(x) _mm_load_si128( (__m128i*)x )
#endif

#define _mm_storeh_epi64(p, A) _mm_storeh_pd((double *)(p), _mm_castsi128_pd(A))
#define mm256(s)               _mm256_castsi128_si256(s)  // expand xmm -> ymm
#define mm128(s)               _mm256_castsi256_si128(s)  // xmm = low(ymm)

    // load dst[0-15], expand to 16 bits, add current results, clip/pack to 8 bytes, save back
#define SAVE_RESULT_16_BYTES(tmp, yy, pred, dst) \
    tmp = _mm256_cvtepu8_epi16(MM_LOAD_EPI128(&pred)); \
    yy  = _mm256_add_epi16(yy, tmp); \
    yy  = _mm256_packus_epi16(yy, yy); \
    yy  = _mm256_permute4x64_epi64(yy, 8); \
    _mm_storeu_si128((__m128i *)&dst, _mm256_castsi256_si128(yy));


#if defined(_WIN32) || defined(_WIN64)
#define M128I_VAR(x, v) \
    static const __m128i x = v
#define M256I_VAR(x, v) \
    static const __m256i x = v
#else
#define M128I_VAR(x, v) \
    static ALIGN_DECL(16) const char array_##x[16] = v; \
    static const __m128i* p_##x = (const __m128i*) array_##x; \
    static const __m128i x = * p_##x

#define M256I_VAR(x, v) \
    static ALIGN_DECL(32) const char array_##x[32] = v; \
    static const __m256i* p_##x = (const __m256i*) array_##x; \
    static const __m256i x = * p_##x

#endif


#define M128I_4xDWORD_INIT(w0,w1,w2,w3) {\
    (char)((w0)&0xFF), (char)(((w0)>>8)&0xFF), (char)(((w0)>>16)&0xFF), (char)(((w0)>>24)&0xFF), \
    (char)((w1)&0xFF), (char)(((w1)>>8)&0xFF), (char)(((w1)>>16)&0xFF), (char)(((w1)>>24)&0xFF), \
    (char)((w2)&0xFF), (char)(((w2)>>8)&0xFF), (char)(((w2)>>16)&0xFF), (char)(((w2)>>24)&0xFF), \
    (char)((w3)&0xFF), (char)(((w3)>>8)&0xFF), (char)(((w3)>>16)&0xFF), (char)(((w3)>>24)&0xFF)}

#define M256I_8xDWORD_INIT(w0,w1,w2,w3,w4,w5,w6,w7) {\
    (char)((w0)&0xFF), (char)(((w0)>>8)&0xFF), (char)(((w0)>>16)&0xFF), (char)(((w0)>>24)&0xFF), \
    (char)((w1)&0xFF), (char)(((w1)>>8)&0xFF), (char)(((w1)>>16)&0xFF), (char)(((w1)>>24)&0xFF), \
    (char)((w2)&0xFF), (char)(((w2)>>8)&0xFF), (char)(((w2)>>16)&0xFF), (char)(((w2)>>24)&0xFF), \
    (char)((w3)&0xFF), (char)(((w3)>>8)&0xFF), (char)(((w3)>>16)&0xFF), (char)(((w3)>>24)&0xFF), \
    (char)((w4)&0xFF), (char)(((w4)>>8)&0xFF), (char)(((w4)>>16)&0xFF), (char)(((w4)>>24)&0xFF), \
    (char)((w5)&0xFF), (char)(((w5)>>8)&0xFF), (char)(((w5)>>16)&0xFF), (char)(((w5)>>24)&0xFF), \
    (char)((w6)&0xFF), (char)(((w6)>>8)&0xFF), (char)(((w6)>>16)&0xFF), (char)(((w6)>>24)&0xFF), \
    (char)((w7)&0xFF), (char)(((w7)>>8)&0xFF), (char)(((w7)>>16)&0xFF), (char)(((w7)>>24)&0xFF)}

#define M256I_2x8W_INIT(w0,w1,w2,w3,w4,w5,w6,w7) {\
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF), \
    (char)((w4)&0xFF),(char)(((w4)>>8)&0xFF), (char)((w5)&0xFF),(char)(((w5)>>8)&0xFF), \
    (char)((w6)&0xFF),(char)(((w6)>>8)&0xFF), (char)((w7)&0xFF),(char)(((w7)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF), \
    (char)((w4)&0xFF),(char)(((w4)>>8)&0xFF), (char)((w5)&0xFF),(char)(((w5)>>8)&0xFF), \
    (char)((w6)&0xFF),(char)(((w6)>>8)&0xFF), (char)((w7)&0xFF),(char)(((w7)>>8)&0xFF)}

#define M128I_4xDWORD(x, w0,w1,w2,w3) M128I_VAR(x, M128I_4xDWORD_INIT(w0,w1,w2,w3))

#define M256I_8xDWORD(x, w0,w1,w2,w3,w4,w5,w6,w7) M256I_VAR(x, M256I_8xDWORD_INIT(w0,w1,w2,w3,w4,w5,w6,w7))

#define M256I_2x8W(x, w0,w1,w2,w3,w4,w5,w6,w7) M256I_VAR(x, M256I_2x8W_INIT(w0,w1,w2,w3,w4,w5,w6,w7))


    M256I_8xDWORD(rounder_2048, 2048, 2048, 0, 0, 2048, 2048, 0, 0);
    M256I_8xDWORD(rounder_1024, 1024, 1024, 0, 0, 1024, 1024, 0, 0);
    M256I_8xDWORD(rounder_512,   512,  512, 0, 0,  512,  512, 0, 0);
    M256I_8xDWORD(rounder_256,   256,  256, 0, 0,  256,  256, 0, 0);
    M256I_8xDWORD(rounder_128,   128,  128, 0, 0,  128,  128, 0, 0);
    M128I_4xDWORD(index0,  0,     8*32, 16*32, 24*32);
    M128I_4xDWORD(index1,  4*32, 12*32, 20*32, 28*32);
    M128I_4xDWORD(index2,  2*32,  6*32, 10*32, 14*32);
    M128I_4xDWORD(index3, 18*32, 22*32, 26*32, 30*32);
    M128I_4xDWORD(index4,  1*32,  3*32,  5*32,  7*32);
    M128I_4xDWORD(index5,  9*32, 11*32, 13*32, 15*32);
    M128I_4xDWORD(index6, 17*32, 19*32, 21*32, 23*32);
    M128I_4xDWORD(index7, 25*32, 27*32, 29*32, 31*32);
#if defined(_WIN32) || defined(_WIN64)
    static const  __m128i reord0 = {0,1,8,9,4,5,12,13,2,3,10,11,6,7,14,15};
    static const  __m256i reorder0 = {0,1,8,9,0,1,8,9, 4,5,12,13,4,5,12,13, 2,3,10,11,2,3,10,11, 6,7,14,15,6,7,14,15};
    static const  __m128i reord1 = {0,1,4,5,8,9,12,13,2,3,6,7,10,11,14,15};
    static const  __m128i reord2 = {0,1,4,5, 0,1,4,5, 2,3,6,7, 2,3,6,7};
    static const  __m128i reord3 = {8,9,12,13, 8,9,12,13, 10,11,14,15, 10,11,14,15};
#else
    static ALIGN_DECL(16) const char array_reord0[16] = {0,1,8,9,4,5,12,13,2,3,10,11,6,7,14,15};
    static const __m128i* p_reord0 = (const __m128i*) array_reord0;
    static const __m128i reord0 = * p_reord0;

    static ALIGN_DECL(32) const char array_reorder0[32] = {0,1,8,9,0,1,8,9, 4,5,12,13,4,5,12,13, 2,3,10,11,2,3,10,11, 6,7,14,15,6,7,14,15};
    static const __m256i* p_reorder0 = (const __m256i*) array_reorder0;
    static const __m256i reorder0 = * p_reorder0;

    static ALIGN_DECL(16) const char array_reord1[16] = {0,1,4,5,8,9,12,13,2,3,6,7,10,11,14,15};
    static const __m128i* p_reord1 = (const __m128i*) array_reord1;
    static const __m128i reord1 = * p_reord1;

    static ALIGN_DECL(16) const char array_reord2[16] = {0,1,4,5, 0,1,4,5, 2,3,6,7, 2,3,6,7};
    static const __m128i* p_reord2 = (const __m128i*) array_reord2;
    static const __m128i reord2 = * p_reord2;

    static ALIGN_DECL(16) const char array_reord3[16] = {8,9,12,13, 8,9,12,13, 10,11,14,15, 10,11,14,15};
    static const __m128i* p_reord3 = (const __m128i*) array_reord3;
    static const __m128i reord3 = * p_reord3;
#endif
    M256I_8xDWORD(perm0, 0, 0, 1, 1, 2, 2, 3, 3);
    M256I_8xDWORD(perm1, 0, 0, 0, 0, 1, 1, 1, 1);
    M256I_8xDWORD(perm2, 2, 2, 2, 2, 3, 3, 3, 3);
    M256I_2x8W(koeff0000,   64, 64,  64,-64,  83, 36,  36,-83);
    M256I_2x8W(round_64,    64,  0,  64,  0,   0,  0,   0,  0);
    M256I_2x8W(koeff0001,   89, 75,  75,-18,  50,-89,  18,-50);
    M256I_2x8W(koeff0002,   50, 18, -89,-50,  18, 75,  75,-89);
    M256I_2x8W(koeff0003,   90, 87,  87, 57,  80,  9,  70,-43);
    M256I_2x8W(koeff0004,   57,-80,  43,-90,  25,-70,   9,-25);
    M256I_2x8W(koeff0005,   80, 70,   9,-43, -70,-87, -87,  9);
    M256I_2x8W(koeff0006,  -25, 90,  57, 25,  90,-80,  43,-57);
    M256I_2x8W(koeff0007,   57, 43, -80,-90, -25, 57,  90, 25);
    M256I_2x8W(koeff0008,   -9,-87, -87, 70,  43,  9,  70,-80);
    M256I_2x8W(koeff0009,   25,  9, -70,-25,  90, 43, -80,-57);
    M256I_2x8W(koeff0010,   43, 70,   9,-80, -57, 87,  87,-90);
    M256I_2x8W(koeff00010,  90, 90,  90, 82,  88, 67,  85, 46);
    M256I_2x8W(koeff02030,  88, 85,  67, 46,  31,-13, -13,-67);
    M256I_2x8W(koeff04050,  82, 78,  22, -4, -54,-82, -90,-73);
    M256I_2x8W(koeff06070,  73, 67, -31,-54, -90,-78, -22, 38);
    M256I_2x8W(koeff08090,  61, 54, -73,-85, -46, -4,  82, 88);
    M256I_2x8W(koeff10110,  46, 38, -90,-88,  38, 73,  54, -4);
    M256I_2x8W(koeff12130,  31, 22, -78,-61,  90, 85, -61,-90);
    M256I_2x8W(koeff14150,  13,  4, -38,-13,  61, 22, -78,-31);
    M256I_2x8W(koeff00011,  82, 22,  78, -4,  73,-31,  67,-54);
    M256I_2x8W(koeff02031, -54,-90, -82,-73, -90,-22, -78, 38);
    M256I_2x8W(koeff04051,  -61, 13,  13, 85,  78, 67,  85,-22);
    M256I_2x8W(koeff06071,  78, 85,  67,-22, -38,-90, -90,  4);
    M256I_2x8W(koeff08091,  31,-46, -88,-61, -13, 82,  90, 13);
    M256I_2x8W(koeff10111, -90,-67,  31, 90,  61,-46, -88,-31);
    M256I_2x8W(koeff12131,   4, 73,  54,-38, -88, -4,  82, 46);
    M256I_2x8W(koeff14151,  88, 38, -90,-46,  85, 54, -73,-61);
    M256I_2x8W(koeff00012,  61,-73,  54,-85,  46,-90,  38,-88);
    M256I_2x8W(koeff00013,  31,-78,  22,-61,  13,-38,   4,-13);
    M256I_2x8W(koeff02032, -46, 82,  -4, 88,  38, 54,  73, -4);
    M256I_2x8W(koeff02033,  90,-61,  85,-90,  61,-78,  22,-31);
    M256I_2x8W(koeff04052,  31,-88, -46,-61, -90, 31, -67, 90);
    M256I_2x8W(koeff04053,   4, 54,  73,-38,  88,-90,  38,-46);
    M256I_2x8W(koeff06072, -13, 90,  82, 13,  61,-88, -46,-31);
    M256I_2x8W(koeff06073, -88, 82,  -4, 46,  85,-73,  54,-61);
    M256I_2x8W(koeff08092,  -4,-90, -90, 38,  22, 67,  85,-78);
    M256I_2x8W(koeff08093, -38,-22, -78, 90,  54,-31,  67,-73);
    M256I_2x8W(koeff10112,  22, 85,  67,-78, -85, 13,  13, 61);
    M256I_2x8W(koeff10113,  73,-90, -82, 54,   4, 22,  78,-82);
    M256I_2x8W(koeff12132, -38,-78, -22, 90,  73,-82, -90, 54); 
    M256I_2x8W(koeff12133,  67,-13, -13,-31, -46, 67,  85,-88);
    M256I_2x8W(koeff14152,  54, 67, -31,-73,   4, 78,  22,-82);
    M256I_2x8W(koeff14153, -46, 85,  67,-88, -82, 90,  90,-90);

    static unsigned int repos[32] = 
    {0, 16,  8, 17, 4, 18,  9, 19, 
    2, 20, 10, 21, 5, 22, 11, 23, 
    1, 24, 12, 25, 6, 26, 13, 27,
    3, 28, 14, 29, 7, 30, 15, 31};

    extern void reordering(signed short * __restrict dest, const signed short * __restrict src);

// CREATE NAME FOR <HEVCPP_API> FUNCTIONS
#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    #define MAKE_NAME(func) func ## _avx2
#else
    #define MAKE_NAME(func) func
#endif


template <int bitDepth, typename PredPixType, typename DstCoeffsType, bool inplace>
static void h265_DCT32x32Inv_16sT_Kernel(const PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
{
        const int SHIFT_INV_2ND = (SHIFT_INV_2ND_BASE - (bitDepth - 8));
        ALIGN_DECL(32) __m128i buffr[32*32];
        ALIGN_DECL(32) signed short temp[32*32];
        __m128i sdata, sdata2, s7;
        //s0, s1, s2, s3, s4, s5, s6, s7, s8;
        __m256i ydata1, ydata2, ydata3, ydata4, ydata5, ydata6, ydata7, ydata8, y0, y1, y2, y3, y4, y5, y6, y7, y8, y9, ya;
        _mm256_zeroall();

#ifdef _USE_SHORTCUT_
        // Prepare shortcut for empty columns. Calculate bitmask
        char zero[32]; // mask buffer
        __m256i z0, z1;
        char * coeffs = (char *)coeff;
        z0 = _mm256_lddqu_si256((const __m256i *)(coeffs));
        z1 = _mm256_lddqu_si256((const __m256i *)(coeffs + 32));
        for (int i=0; i<31; i++) {
            coeffs += 32*2;
            z0 = _mm256_or_si256(z0, _mm256_lddqu_si256((const __m256i *)(coeffs)));
            z1 = _mm256_or_si256(z1, _mm256_lddqu_si256((const __m256i *)(coeffs + 32)));
        }
        z0 = _mm256_packs_epi16(z0, z0);
        z1 = _mm256_packs_epi16(z1, z1);

        _mm_storel_epi64((__m128i *)&zero[ 0], mm128(z0));
        _mm_storel_epi64((__m128i *)&zero[ 8], _mm256_extracti128_si256(z0, 1));
        _mm_storel_epi64((__m128i *)&zero[16], mm128(z1));
        _mm_storel_epi64((__m128i *)&zero[24], _mm256_extracti128_si256(z1, 1));
#endif

        // Vertical 1-D inverse transform
        {
#define shift       7

            for (int i=0; i<32; i+=2) 
            {
                int numL = 4 * repos[i];
                int numH = 4 * repos[i+1];

#ifdef _USE_SHORTCUT_
                if (zero[i] == 0) { // check shortcut: is input column empty?
                    __m128i null = _mm_setzero_si128();
                    buffr[numL++] = null; buffr[numL++] = null; buffr[numL++] = null; buffr[numL++] = null; 
                    buffr[numH++] = null; buffr[numH++] = null; buffr[numH++] = null; buffr[numH++] = null; 
                    continue;
                }
#endif
                // load first 2x4 koefficients
#ifndef _USE_REORDERING_AVX2_
                sdata  = _mm_i32gather_epi32((int const *)coeff, index0, 2);  // 2401 2400 1601 1600 0801 0800 0001 0000
#else
                sdata  = _mm_lddqu_si128((const __m128i *)coeff); coeff += 8;
#endif

// GCC 4.7 bug workaround
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
                ydata1 = _mm256_broadcastsi128_si256(sdata);
#elif ( __GNUC__ < 4 || (__GNUC__ == 4 && \
    (__GNUC_MINOR__ < 6 || (__GNUC_MINOR__ == 6 && __GNUC_PATCHLEVEL__ > 0))))
                ydata1 = _mm_broadcastsi128_si256((__m128i const *)&sdata);
#elif(__GNUC__ == 4 && (__GNUC_MINOR__ == 7 && __GNUC_PATCHLEVEL__ > 0))
                ydata1 = _mm_broadcastsi128_si256(sdata);
#endif
                ydata1 = _mm256_shuffle_epi8(ydata1, reorder0);

                y1 = _mm256_madd_epi16(ydata1, koeff0000);
                y1 = _mm256_add_epi32(y1,  round_64);
                y0 = _mm256_shuffle_epi32(y1, 0x4E); 
                y1 = _mm256_sub_epi32(y1, y0);
                y0 = _mm256_add_epi32(y0, y0);
                y0 = _mm256_add_epi32(y0, y1);
                y1 = _mm256_shuffle_epi32(y1, 1); 
                y0 = _mm256_unpacklo_epi64(y0, y1);

                // load second 2x4 koefficients
#ifndef _USE_REORDERING_AVX2_
                sdata  = _mm_i32gather_epi32((int const *)coeff, index1, 2);  // 2801 2800 2001 2000 1201 1200 0401 0400
#else
                sdata  = _mm_lddqu_si128((const __m128i *)coeff); coeff += 8;
#endif
                sdata  = _mm_shuffle_epi8(sdata, reord1);
                ydata1 = _mm256_permute4x64_epi64(mm256(sdata), 0x50);
                ydata2 = _mm256_shuffle_epi32(ydata1, 0x00);               // 1201 0401 1201 0401 1201 0401 1201 0401 | 1200 0400 1200 0400 1200 0400 1200 0400
                ydata1 = _mm256_shuffle_epi32(ydata1, 0x55);               // 2801 2001 2801 2001 2801 2001 2801 2001 | 2800 2000 2800 2000 2800 2000 2800 2000

                y4 = _mm256_madd_epi16(ydata2, koeff0001);
                y5 = _mm256_madd_epi16(ydata1, koeff0002);
                y4 = _mm256_add_epi32(y4,  y5);
                y1 = _mm256_shuffle_epi32(y0, 0x1B); 
                y7 = _mm256_shuffle_epi32(y4, 0x1B); 
                y0 = _mm256_add_epi32(y0,  y4);
                y1 = _mm256_sub_epi32(y1,  y7);

                // load 3d 2x4 koefficients
#ifndef _USE_REORDERING_AVX2_
                sdata  = _mm_i32gather_epi32((int const *)coeff, index2, 2);  // 1401 1400 1001 1000 0601 0600 0201 0200
#else
                sdata  = _mm_lddqu_si128((const __m128i *)coeff); coeff += 8;
#endif
                sdata2 = _mm_shuffle_epi8(sdata, reord2);
                sdata  = _mm_shuffle_epi8(sdata, reord3);
                ydata1 = _mm256_permute4x64_epi64(mm256(sdata2), 0x50); // 0601 0201 0601 0201 0601 0201 0601 0201 | 0600 0200 0600 0200 0600 0200 0600 0200
                ydata2 = _mm256_permute4x64_epi64(mm256(sdata),  0x50); // 1401 1001 1401 1001 1401 1001 1401 1001 | 1400 1000 1400 1000 1400 1000 1400 1000

                y4 = _mm256_madd_epi16(ydata1, koeff0003);
                y5 = _mm256_madd_epi16(ydata1, koeff0004);
                y7 = _mm256_madd_epi16(ydata2, koeff0005);
                y4 = _mm256_add_epi32(y4,  y7);
                y6 = _mm256_madd_epi16(ydata2, koeff0006);
                y5 = _mm256_add_epi32(y5,  y6);

                // load 4th 2x4 koefficients
#ifndef _USE_REORDERING_AVX2_
                sdata  = _mm_i32gather_epi32((int const *)coeff, index3, 2);  // 3001 3000 2601 2600 2201 2200 1801 1800
#else
                sdata  = _mm_lddqu_si128((const __m128i *)coeff); coeff += 8;
#endif
                sdata2 = _mm_shuffle_epi8(sdata, reord2);
                sdata  = _mm_shuffle_epi8(sdata, reord3);
                ydata1 = _mm256_permute4x64_epi64(mm256(sdata2), 0x50);
                ydata2 = _mm256_permute4x64_epi64(mm256(sdata), 0x50);

                y7 = _mm256_madd_epi16(ydata1, koeff0007);
                y4 = _mm256_add_epi32(y4,  y7);
                y6 = _mm256_madd_epi16(ydata1, koeff0008);
                y5 = _mm256_add_epi32(y5,  y6);
                y7 = _mm256_madd_epi16(ydata2, koeff0009);
                y4 = _mm256_add_epi32(y4,  y7);
                y6 = _mm256_madd_epi16(ydata2, koeff0010);
                y5 = _mm256_add_epi32(y5,  y6);

                y2 = _mm256_shuffle_epi32(y0, 0x1B); 
                y3 = _mm256_shuffle_epi32(y1, 0x1B); 
                y6 = _mm256_shuffle_epi32(y4, 0x1B); 
                y7 = _mm256_shuffle_epi32(y5, 0x1B); 

                y0 = _mm256_add_epi32(y0,  y4);
                y1 = _mm256_add_epi32(y1,  y5);
                y2 = _mm256_sub_epi32(y2,  y6);
                y3 = _mm256_sub_epi32(y3,  y7);

                // load 5th 2x4 koefficients
#ifndef _USE_REORDERING_AVX2_
                sdata  = _mm_i32gather_epi32((int const *)coeff, index4, 2);  // 
#else
                sdata  = _mm_lddqu_si128((const __m128i *)coeff); coeff += 8;
#endif
                sdata2 = _mm_shuffle_epi8(sdata, reord2);
                sdata  = _mm_shuffle_epi8(sdata, reord3);
                ydata1 = _mm256_permute4x64_epi64(mm256(sdata2), 0x50);     // 0301 0101 0301 0101 0301 0101 0301 0101 | 0300 0100 0300 0100 0300 0100 0300 0100
                ydata2 = _mm256_permute4x64_epi64(mm256(sdata), 0x50);      // 0701 0501 0701 0501 0701 0501 0701 0501 | 0700 0500 0700 0500 0700 0500 0700 0500

                y6 = _mm256_madd_epi16(ydata1, koeff00010);
                y7 = _mm256_madd_epi16(ydata2, koeff02030);
                y6 = _mm256_add_epi32(y6,  y7);

                // load 6th 2x4 koefficients
#ifndef _USE_REORDERING_AVX2_
                sdata  = _mm_i32gather_epi32((int const *)coeff, index5, 2);  // 1501 1500 1301 1300 1101 1100 0901 0900
#else
                sdata  = _mm_lddqu_si128((const __m128i *)coeff); coeff += 8;
#endif
                sdata2 = _mm_shuffle_epi8(sdata, reord2);
                sdata  = _mm_shuffle_epi8(sdata, reord3);
                ydata3 = _mm256_permute4x64_epi64(mm256(sdata2), 0x50);     // 1101 0901 1101 0901 1101 0901 1101 0901 | 1100 0900 1100 0900 1100 0900 1100 0900
                ydata4 = _mm256_permute4x64_epi64(mm256(sdata), 0x50);      // 1501 1301 1501 1301 1501 1301 1501 1301 | 1500 1300 1500 1300 1500 1300 1500 1300

                y7 = _mm256_madd_epi16(ydata3, koeff04050);
                y6 = _mm256_add_epi32(y6,  y7);
                y7 = _mm256_madd_epi16(ydata4, koeff06070);
                y6 = _mm256_add_epi32(y6,  y7);

                // load 7th 2x4 koefficients
#ifndef _USE_REORDERING_AVX2_
                sdata  = _mm_i32gather_epi32((int const *)coeff, index6, 2);  // 2301 2300 2101 2100 1901 1900 1701 1700
#else
                sdata  = _mm_lddqu_si128((const __m128i *)coeff); coeff += 8;
#endif
                sdata2 = _mm_shuffle_epi8(sdata, reord2);                   // 1901 1701 1901 1701 1900 1700 1900 1700
                sdata  = _mm_shuffle_epi8(sdata, reord3);                   // 2301 2101 2301 2101 2300 2100 2300 2100
                ydata5 = _mm256_permute4x64_epi64(mm256(sdata2), 0x50);     // 1901 1701 1901 1701 1901 1701 1901 1701 | 1900 1700 1900 1700 1900 1700 1900 1700
                ydata6 = _mm256_permute4x64_epi64(mm256(sdata), 0x50);      // 2301 2101 2301 2101 2301 2101 2301 2101 | 2300 2100 2300 2100 2300 2100 2300 2100

                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata5, koeff08090));
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata6, koeff10110));

                // load 8th 2x4 koefficients
#ifndef _USE_REORDERING_AVX2_
                sdata  = _mm_i32gather_epi32((int const *)coeff, index7, 2);  // 
#else
                sdata  = _mm_lddqu_si128((const __m128i *)coeff); coeff += 8;
#endif
                sdata2 = _mm_shuffle_epi8(sdata, reord2);                   // 
                sdata  = _mm_shuffle_epi8(sdata, reord3);                   // 
                ydata7 = _mm256_permute4x64_epi64(mm256(sdata2), 0x50);     // 
                ydata8 = _mm256_permute4x64_epi64(mm256(sdata), 0x50);      // 

                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata7, koeff12130)); // FFF81B0000114500FFC2990000362300
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata8, koeff14150)); // FFE5AE00001F4000FFBA06000038FC00

                y7 = _mm256_shuffle_epi32(y0, 0x1B); 
                y0 = _mm256_add_epi32(y0,  y6);
                y6 = _mm256_shuffle_epi32(y6, 0x1B); 
                y7 = _mm256_sub_epi32(y7,  y6);

                y0 = _mm256_srai_epi32(y0,  shift);            // (r03 r02 r01 r00) >>= shift
                y7 = _mm256_srai_epi32(y7,  shift);            // (r31 r30 r29 r28) >>= shift
                y0 = _mm256_packs_epi32(y0, y7);               // clip(-32768, 32767)

                // Store 1
                _mm_storeu_si128((__m128i *)&buffr[numL++], mm128(y0));
                _mm_storeu_si128((__m128i *)&buffr[numH++], _mm256_extracti128_si256(y0, 1));

                y6 = _mm256_madd_epi16(ydata1, koeff00011);
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata2, koeff02031));
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata3, koeff04051));
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata4, koeff06071));
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata5, koeff08091));
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata6, koeff10111));
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata7, koeff12131));
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(ydata8, koeff14151));

                y7 = _mm256_shuffle_epi32(y1, 0x1B);
                y1 = _mm256_add_epi32(y1,  y6);
                y6 = _mm256_shuffle_epi32(y6, 0x1B);
                y7 = _mm256_sub_epi32(y7,  y6);
                y1 = _mm256_packs_epi32(_mm256_srai_epi32(y1,  shift), _mm256_srai_epi32(y7,  shift));

                // Store 2
                _mm_storeu_si128((__m128i *)&buffr[numL++], mm128(y1));
                _mm_storeu_si128((__m128i *)&buffr[numH++], _mm256_extracti128_si256(y1, 1));

                y0 = _mm256_madd_epi16(ydata1, koeff00012);
                y1 = _mm256_madd_epi16(ydata1, koeff00013);

                y0 = _mm256_add_epi32(y0,  _mm256_madd_epi16(ydata2, koeff02032));
                y1 = _mm256_add_epi32(y1,  _mm256_madd_epi16(ydata2, koeff02033));

                y0 = _mm256_add_epi32(y0,  _mm256_madd_epi16(ydata3, koeff04052));
                y1 = _mm256_add_epi32(y1,  _mm256_madd_epi16(ydata3, koeff04053));

                y0 = _mm256_add_epi32(y0,  _mm256_madd_epi16(ydata4, koeff06072));
                y1 = _mm256_add_epi32(y1,  _mm256_madd_epi16(ydata4, koeff06073));

                y0 = _mm256_add_epi32(y0,  _mm256_madd_epi16(ydata5, koeff08092));
                y1 = _mm256_add_epi32(y1,  _mm256_madd_epi16(ydata5, koeff08093));

                y0 = _mm256_add_epi32(y0,  _mm256_madd_epi16(ydata6, koeff10112));
                y1 = _mm256_add_epi32(y1,  _mm256_madd_epi16(ydata6, koeff10113));

                y0 = _mm256_add_epi32(y0,  _mm256_madd_epi16(ydata7, koeff12132));
                y1 = _mm256_add_epi32(y1,  _mm256_madd_epi16(ydata7, koeff12133));

                y0 = _mm256_add_epi32(y0,  _mm256_madd_epi16(ydata8, koeff14152));
                y1 = _mm256_add_epi32(y1,  _mm256_madd_epi16(ydata8, koeff14153));

                y6 = _mm256_shuffle_epi32(y3, 0x1B);
                y3 = _mm256_add_epi32(y3,  y0);
                y0 = _mm256_shuffle_epi32(y0, 0x1B);
                y6 = _mm256_sub_epi32(y6,  y0);
                y3 = _mm256_packs_epi32(_mm256_srai_epi32(y3,  shift), _mm256_srai_epi32(y6,  shift));

                // Store 3
                _mm_storeu_si128((__m128i *)&buffr[numL++], mm128(y3));
                _mm_storeu_si128((__m128i *)&buffr[numH++], _mm256_extracti128_si256(y3, 1));

                y7 = _mm256_shuffle_epi32(y2, 0x1B);
                y2 = _mm256_add_epi32(y2,  y1);
                y1 = _mm256_shuffle_epi32(y1, 0x1B);
                y7 = _mm256_sub_epi32(y7,  y1);
                y2 = _mm256_packs_epi32(_mm256_srai_epi32(y2,  shift), _mm256_srai_epi32(y7,  shift));

                // Store 4
                _mm_storeu_si128((__m128i *)&buffr[numL++], mm128(y2));
                _mm_storeu_si128((__m128i *)&buffr[numH++], _mm256_extracti128_si256(y2, 1));
#ifndef _USE_REORDERING_AVX2_
                coeff += 2;
#endif
            }
#undef  shift
        }

        reordering(temp, (const signed short *)buffr);

        // Horizontal 1-D inverse transform
        {
            coeff = temp;

            for (int i=0; i<16; i++) {
                _mm256_zeroall();
                // load first 8 words
                s7 = _mm_load_si128((const __m128i *)coeff); coeff += 8;   // load:  D C B A 3 2 1 0
                y1 = _mm256_permutevar8x32_epi32(mm256(s7), perm0);    //        DC DC BA BA | 32 32 10 10 

                y1 = _mm256_madd_epi16(y1, koeff0000);       // k3*-83 + k2*36 ; k3*36 + k2*83 ;  k1*-64 + k0*64 ; k1*64 + k0*64 ;
                if (SHIFT_INV_2ND == 12) {
                    y1 = _mm256_add_epi32(y1, rounder_2048);
                } else if (SHIFT_INV_2ND == 11) {
                    y1 = _mm256_add_epi32(y1, rounder_1024);
                } else if (SHIFT_INV_2ND == 10) {
                    y1 = _mm256_add_epi32(y1, rounder_512);
                } else if (SHIFT_INV_2ND == 9) {
                    y1 = _mm256_add_epi32(y1, rounder_256);
                } else if (SHIFT_INV_2ND == 8) {
                    y1 = _mm256_add_epi32(y1, rounder_128);
                }
                y0 = _mm256_shuffle_epi32(y1, 0x4E); 
                y1 = _mm256_sub_epi32(y1, y0);
                y0 = _mm256_add_epi32(y0, y0);
                y0 = _mm256_add_epi32(y0, y1);
                y1 = _mm256_shuffle_epi32(y1, 1); 
                y0 = _mm256_unpacklo_epi64(y0, y1);

                s7 = _mm_load_si128((const __m128i *)coeff); coeff += 8;
                y4 = _mm256_permutevar8x32_epi32(mm256(s7), perm1);
                y5 = _mm256_permutevar8x32_epi32(mm256(s7), perm2);

                y4 = _mm256_madd_epi16(y4, koeff0001);
                y5 = _mm256_madd_epi16(y5, koeff0002);
                y4 = _mm256_add_epi32(y4,  y5);
                y1 = _mm256_shuffle_epi32(y0, 0x1B); 
                y7 = _mm256_shuffle_epi32(y4, 0x1B); 
                y0 = _mm256_add_epi32(y0,  y4);
                y1 = _mm256_sub_epi32(y1,  y7);

                // load next 8 words
                s7 = _mm_load_si128((const __m128i *)coeff); coeff += 8;
                y5 = _mm256_permutevar8x32_epi32(mm256(s7), perm1);
                y6 = _mm256_permutevar8x32_epi32(mm256(s7), perm2);

                y4 = _mm256_madd_epi16(y5, koeff0003);
                y5 = _mm256_madd_epi16(y5, koeff0004);
                y7 = _mm256_madd_epi16(y6, koeff0005);
                y4 = _mm256_add_epi32(y4,  y7);
                y6 = _mm256_madd_epi16(y6, koeff0006);
                y5 = _mm256_add_epi32(y5,  y6);

                s7 = _mm_load_si128((const __m128i *)coeff); coeff += 8;
                y6 = _mm256_permutevar8x32_epi32(mm256(s7), perm1);

                y7 = _mm256_madd_epi16(y6, koeff0007);
                y4 = _mm256_add_epi32(y4,  y7);
                y6 = _mm256_madd_epi16(y6, koeff0008);
                y5 = _mm256_add_epi32(y5,  y6);

                y6 = _mm256_permutevar8x32_epi32(mm256(s7), perm2);
                y7 = _mm256_madd_epi16(y6, koeff0009);
                y4 = _mm256_add_epi32(y4,  y7);
                y6 = _mm256_madd_epi16(y6, koeff0010);
                y5 = _mm256_add_epi32(y5,  y6);

                y2 = _mm256_shuffle_epi32(y0, 0x1B); 
                y3 = _mm256_shuffle_epi32(y1, 0x1B); 
                y6 = _mm256_shuffle_epi32(y4, 0x1B); 
                y7 = _mm256_shuffle_epi32(y5, 0x1B); 

                y0 = _mm256_add_epi32(y0, y4);
                y1 = _mm256_add_epi32(y1, y5);
                y2 = _mm256_sub_epi32(y2, y6);
                y3 = _mm256_sub_epi32(y3, y7);

                // load 3th 8 words
                s7 = _mm_load_si128((const __m128i *)coeff); coeff += 8;
                y7 = _mm256_permutevar8x32_epi32(mm256(s7), perm1);

                y6 = _mm256_madd_epi16(y7, koeff00010);
                ya = _mm256_madd_epi16(y7, koeff00011);
                y8 = _mm256_madd_epi16(y7, koeff00012);
                y9 = _mm256_madd_epi16(y7, koeff00013);

                y7 = _mm256_permutevar8x32_epi32(mm256(s7), perm2);
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(y7, koeff02030));
                ya = _mm256_add_epi32(ya, _mm256_madd_epi16(y7, koeff02031));
                y8 = _mm256_add_epi32(y8, _mm256_madd_epi16(y7, koeff02032));
                y9 = _mm256_add_epi32(y9, _mm256_madd_epi16(y7, koeff02033));

                s7 = _mm_load_si128((const __m128i *)coeff); coeff += 8;
                y7 = _mm256_permutevar8x32_epi32(mm256(s7), perm1);

                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(y7, koeff04050));
                ya = _mm256_add_epi32(ya, _mm256_madd_epi16(y7, koeff04051));
                y8 = _mm256_add_epi32(y8, _mm256_madd_epi16(y7, koeff04052));
                y9 = _mm256_add_epi32(y9, _mm256_madd_epi16(y7, koeff04053));

                y7 = _mm256_permutevar8x32_epi32(mm256(s7), perm2);
                y6 = _mm256_add_epi32(y6, _mm256_madd_epi16(y7, koeff06070));
                ya = _mm256_add_epi32(ya, _mm256_madd_epi16(y7, koeff06071));
                y8 = _mm256_add_epi32(y8, _mm256_madd_epi16(y7, koeff06072));
                y9 = _mm256_add_epi32(y9, _mm256_madd_epi16(y7, koeff06073));

                // load last 8 words
                s7  = _mm_load_si128((const __m128i *)coeff); coeff += 8;
                y7 = _mm256_permutevar8x32_epi32(mm256(s7), perm1);
                y6 = _mm256_add_epi32(y6, _mm256_madd_epi16(y7, koeff08090));
                ya = _mm256_add_epi32(ya, _mm256_madd_epi16(y7, koeff08091));
                y8 = _mm256_add_epi32(y8, _mm256_madd_epi16(y7, koeff08092));
                y9 = _mm256_add_epi32(y9, _mm256_madd_epi16(y7, koeff08093));

                y7 = _mm256_permutevar8x32_epi32(mm256(s7), perm2);
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(y7, koeff10110));
                ya = _mm256_add_epi32(ya, _mm256_madd_epi16(y7, koeff10111));
                y8 = _mm256_add_epi32(y8, _mm256_madd_epi16(y7, koeff10112));
                y9 = _mm256_add_epi32(y9, _mm256_madd_epi16(y7, koeff10113));

                s7 = _mm_load_si128((const __m128i *)coeff); coeff += 8;
                y7 = _mm256_permutevar8x32_epi32(mm256(s7), perm1);
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(y7, koeff12130));
                ya = _mm256_add_epi32(ya, _mm256_madd_epi16(y7, koeff12131));
                y8 = _mm256_add_epi32(y8, _mm256_madd_epi16(y7, koeff12132));
                y9 = _mm256_add_epi32(y9, _mm256_madd_epi16(y7, koeff12133));

                y7 = _mm256_permutevar8x32_epi32(mm256(s7), perm2);
                y6 = _mm256_add_epi32(y6,  _mm256_madd_epi16(y7, koeff14150));
                ya = _mm256_add_epi32(ya, _mm256_madd_epi16(y7, koeff14151));
                y8 = _mm256_add_epi32(y8, _mm256_madd_epi16(y7, koeff14152));
                y9 = _mm256_add_epi32(y9, _mm256_madd_epi16(y7, koeff14153));

                y7 = _mm256_shuffle_epi32(y0, 0x1B); 
                y0 = _mm256_add_epi32(y0, y6);
                y6 = _mm256_shuffle_epi32(y6, 0x1B); 
                y7 = _mm256_sub_epi32(y7, y6);

                y0 = _mm256_srai_epi32(y0, SHIFT_INV_2ND);           // (r03 r02 r01 r00) >>= SHIFT_INV_2ND
                y7 = _mm256_srai_epi32(y7, SHIFT_INV_2ND);           // (r31 r30 r29 r28) >>= SHIFT_INV_2ND
                y0 = _mm256_packs_epi32(y0, y7);             // clip(-32768, 32767)

                y7 = _mm256_shuffle_epi32(y1, 0x1B);
                y1 = _mm256_add_epi32(y1, ya);
                y6 = _mm256_shuffle_epi32(ya, 0x1B);
                y7 = _mm256_sub_epi32(y7, y6);
                y1 = _mm256_packs_epi32(_mm256_srai_epi32(y1, SHIFT_INV_2ND), _mm256_srai_epi32(y7, SHIFT_INV_2ND)); // clip
                y6 = _mm256_unpacklo_epi64(y0, y1);
                y0 = _mm256_unpackhi_epi64(y1, y0);

                y1 = _mm256_shuffle_epi32(y3, 0x1B);
                y3 = _mm256_add_epi32(y3,  y8);
                y7 = _mm256_shuffle_epi32(y8, 0x1B);
                y1 = _mm256_sub_epi32(y1,  y7);
                y3 = _mm256_packs_epi32(_mm256_srai_epi32(y3, SHIFT_INV_2ND), _mm256_srai_epi32(y1, SHIFT_INV_2ND)); // clip

                y7 = _mm256_shuffle_epi32(y2, 0x1B);
                y2 = _mm256_add_epi32(y2, y9);
                y1 = _mm256_shuffle_epi32(y9, 0x1B);
                y7 = _mm256_sub_epi32(y7, y1);
                y2 = _mm256_packs_epi32(_mm256_srai_epi32(y2, SHIFT_INV_2ND), _mm256_srai_epi32(y7, SHIFT_INV_2ND)); // clip

                y7 = _mm256_unpacklo_epi64(y3, y2);
                y1 = _mm256_unpackhi_epi64(y2, y3);

                // y6: a1 a0                 0...7  8..15 16..23 24..31
                // y7: b1 b0       in mem: +------+------+------+------+
                // y1: c1 c0           +0: |  a0     b0     c0     d0
                // y0: d1 d0      +stride: |  a1     b1     c1     d1
                y2 = _mm256_permute2x128_si256(y6, y7, 0x20); // b0 a0 (low parts)
                y6 = _mm256_permute2x128_si256(y6, y7, 0x31); // b1 a1 (high parts)
                y7 = _mm256_permute2x128_si256(y1, y0, 0x20); // d0 c0 (low parts)
                y1 = _mm256_permute2x128_si256(y1, y0, 0x31); // d1 c1 (high parts)

                if (inplace && sizeof(DstCoeffsType) == 1) {
                    SAVE_RESULT_16_BYTES(y0, y2, pred[0], dst[ 0]);
                    SAVE_RESULT_16_BYTES(y0, y7, pred[16], dst[16]);
                    SAVE_RESULT_16_BYTES(y0, y6, pred[predStride + 0], dst[destStride + 0]);
                    SAVE_RESULT_16_BYTES(y0, y1, pred[predStride + 16], dst[destStride + 16]);
                    dst += destStride * 2;
                    pred += predStride * 2;
                } else if (inplace && sizeof(DstCoeffsType) == 2) {
                    y0 = _mm256_loadu_si256((__m256i *)(&pred[0]));
                    y3 = _mm256_loadu_si256((__m256i *)(&pred[16]));
                    y4 = _mm256_loadu_si256((__m256i *)(&pred[predStride]));
                    y5 = _mm256_loadu_si256((__m256i *)(&pred[predStride + 16]));

                    y2 = _mm256_adds_epi16(y2, y0);
                    y7 = _mm256_adds_epi16(y7, y3);
                    y6 = _mm256_adds_epi16(y6, y4);
                    y1 = _mm256_adds_epi16(y1, y5);

                    y4 = _mm256_setzero_si256();
                    y5 = _mm256_set1_epi16((1 << bitDepth) - 1);

                    y2 = _mm256_max_epi16(y2, y4);  y2 = _mm256_min_epi16(y2, y5);
                    y7 = _mm256_max_epi16(y7, y4);  y7 = _mm256_min_epi16(y7, y5);
                    y6 = _mm256_max_epi16(y6, y4);  y6 = _mm256_min_epi16(y6, y5);
                    y1 = _mm256_max_epi16(y1, y4);  y1 = _mm256_min_epi16(y1, y5);

                    _mm256_store_si256((__m256i *)&dst[ 0], y2);
                    _mm256_store_si256((__m256i *)&dst[16], y7);
                    _mm256_store_si256((__m256i *)&dst[destStride + 0], y6);
                    _mm256_store_si256((__m256i *)&dst[destStride + 16], y1);
                    dst += 2 * destStride;
                    pred += 2 * predStride;
                } else { // destSize == 2
                    _mm256_store_si256((__m256i *)&dst[ 0], y2);
                    _mm256_store_si256((__m256i *)&dst[16], y7);
                    _mm256_store_si256((__m256i *)&dst[destStride + 0], y6);
                    _mm256_store_si256((__m256i *)&dst[destStride + 16], y1);
                    dst += destStride * 2;
                    pred += 2 * predStride;
                }
            }
        }

    } // 

void MAKE_NAME(h265_DCT32x32Inv_16sT)(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth)
{
    if (inplace) {
        switch (bitDepth) {
        case  8:
            if (inplace == 2)
                h265_DCT32x32Inv_16sT_Kernel< 8, Ipp16u, Ipp16u,  true >((Ipp16u *)pred, predStride, (Ipp16u *) destPtr, coeff, destStride);
            else
                h265_DCT32x32Inv_16sT_Kernel< 8, Ipp8u, Ipp8u,  true >((Ipp8u *)pred, predStride, (Ipp8u *) destPtr, coeff, destStride);
            break;
        case  9: h265_DCT32x32Inv_16sT_Kernel< 9, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 10: h265_DCT32x32Inv_16sT_Kernel<10, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 11: h265_DCT32x32Inv_16sT_Kernel<11, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 12: h265_DCT32x32Inv_16sT_Kernel<12, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        }
    } else {
        switch (bitDepth) {
        case  8: h265_DCT32x32Inv_16sT_Kernel< 8, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case  9: h265_DCT32x32Inv_16sT_Kernel< 9, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 10: h265_DCT32x32Inv_16sT_Kernel<10, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 11: h265_DCT32x32Inv_16sT_Kernel<11, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 12: h265_DCT32x32Inv_16sT_Kernel<12, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        }
    }
}

} // end namespace MFX_HEVC_PP

#endif //#if defined (MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
