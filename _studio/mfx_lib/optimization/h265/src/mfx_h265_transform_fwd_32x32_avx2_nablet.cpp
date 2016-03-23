/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2016 Intel Corporation. All Rights Reserved.
//
//
*/

// Forward HEVC Transform functions optimised by intrinsics
// Sizes: 32.
// Faster than prev 32x32 version (~1.15 for win32, ~1.25 for win64)
//
// NOTES:
// - requires AVX2
// - to avoid SSE/AVX transition penalties, ensure that AVX2 code is wrapped with _mm256_zeroupper() 
//     (include explicitly on function entry, ICL should automatically add before function return)

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"
//#include "mfx_h265_transform_consts.h"

#if defined (MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO)

namespace MFX_HEVC_PP
{
    // Forward HEVC Transform 32x32 optimised for AVX2

#include <immintrin.h> // avx
//#include "definitions.h"

// ========================================================
#define ALIGNED_AVX  ALIGN_DECL(32)

    // to use intrinsics more easily
#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))
#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (double *)(p)))
#define _mm_storeh_epi64(p, A) _mm_storeh_pd((double *)(p), _mm_castsi128_pd(A))

// cast between ymm <-> sse
#define mm256(s)               _mm256_castsi128_si256(s)  // expand xmm -> ymm
#define mm128(s)               _mm256_castsi256_si128(s)  // xmm = low(ymm)

#define mm256_storel_epi128(p, A)  _mm_store_si128 ( (__m128i *)(p), _mm256_castsi256_si128(A))
#define mm256_storelu_epi128(p, A) _mm_storeu_si128( (__m128i *)(p), _mm256_castsi256_si128(A))
#define mm256_storeh_epi128(p, A)  _mm_store_si128 ( (__m128i *)(p), _mm256_extracti128_si256(A, 1))
#define mm256_storehu_epi128(p, A) _mm_storeu_si128( (__m128i *)(p), _mm256_extracti128_si256(A, 1))

#if defined(_MSC_VER)
#pragma warning (disable: 4310) // warning C4310: cast truncates constant value
#endif

// linux compatibility
#if defined(_WIN32) || defined(_WIN64)
#define M256I_VAR(x, v) \
    static const __m256i x = v
#else
#define M256I_VAR(x, v) \
    static ALIGN_DECL(32) const char array_##x[32] = v; \
    static const __m256i* p_##x = (const __m256i*) array_##x; \
    static const __m256i x = * p_##x
#endif
// ========================================================

#define M256I_D4x2C_INIT(w0,w1,w2,w3) {\
    (char)((w0)&0xFF), (char)(((w0)>>8)&0xFF), (char)(((w0)>>16)&0xFF), (char)(((w0)>>24)&0xFF), \
    (char)((w1)&0xFF), (char)(((w1)>>8)&0xFF), (char)(((w1)>>16)&0xFF), (char)(((w1)>>24)&0xFF), \
    (char)((w2)&0xFF), (char)(((w2)>>8)&0xFF), (char)(((w2)>>16)&0xFF), (char)(((w2)>>24)&0xFF), \
    (char)((w3)&0xFF), (char)(((w3)>>8)&0xFF), (char)(((w3)>>16)&0xFF), (char)(((w3)>>24)&0xFF), \
    (char)((w0)&0xFF), (char)(((w0)>>8)&0xFF), (char)(((w0)>>16)&0xFF), (char)(((w0)>>24)&0xFF), \
    (char)((w1)&0xFF), (char)(((w1)>>8)&0xFF), (char)(((w1)>>16)&0xFF), (char)(((w1)>>24)&0xFF), \
    (char)((w2)&0xFF), (char)(((w2)>>8)&0xFF), (char)(((w2)>>16)&0xFF), (char)(((w2)>>24)&0xFF), \
    (char)((w3)&0xFF), (char)(((w3)>>8)&0xFF), (char)(((w3)>>16)&0xFF), (char)(((w3)>>24)&0xFF)}

// duplicate __m256i -> __m256i
#define M256I_D4x2C(x, w0,w1,w2,w3) M256I_VAR(x, M256I_D4x2C_INIT(w0,w1,w2,w3))

// duplicate __m256i -> __m256i
#define M256I_W8x2C_INIT(w0,w1,w2,w3,w4,w5,w6,w7) {\
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF), \
    (char)((w4)&0xFF),(char)(((w4)>>8)&0xFF), (char)((w5)&0xFF),(char)(((w5)>>8)&0xFF), \
    (char)((w6)&0xFF),(char)(((w6)>>8)&0xFF), (char)((w7)&0xFF),(char)(((w7)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF), \
    (char)((w4)&0xFF),(char)(((w4)>>8)&0xFF), (char)((w5)&0xFF),(char)(((w5)>>8)&0xFF), \
    (char)((w6)&0xFF),(char)(((w6)>>8)&0xFF), (char)((w7)&0xFF),(char)(((w7)>>8)&0xFF)}


#define M256I_W8x2C(x, w0,w1,w2,w3,w4,w5,w6,w7) M256I_VAR(x, M256I_W8x2C_INIT(w0,w1,w2,w3,w4,w5,w6,w7))
// ========================================================

    // Win32: 32x32:   reference = 26589, sse4.1 = 6999, avx2 = 3948
    // Win64: 32x32:   reference = 24227, sse4.1 = 6610, avx2 = 3759

    // #define _SHORT_STYLE_ 1    // in "short style" text looks shorter, but execution is 100 cpu clocks slower

    // Experimenses with loading constants. Actually does not matter
#define _mm256_load_const _mm256_load_si256  // movdqa
    //#define _mm256_load_const _mm256_lddqu_si256  // lddqu
    //#define _mm256_load_const _mm256_loadu_si256  // movdqu



#if (1) // just to roll up this text in MSVC
    // koefs for horizontal 1-D transform
    M256I_D4x2C(rounder_8,   8,  8,  8,  8);
    M256I_D4x2C(rounder_16, 16, 16, 16, 16);
    M256I_D4x2C(rounder_32, 32, 32, 32, 32);

#if defined(_WIN32) || defined(_WIN64)
    ALIGNED_AVX __m256i kefh_shuff = {14,15,12,13,10,11,8,9,6,7,4,5,2,3,0,1,  14,15,12,13,10,11,8,9,6,7,4,5,2,3,0,1};
#else
    static ALIGN_DECL(32) const char array_kefh_shuff[32] = {14,15,12,13,10,11,8,9,6,7,4,5,2,3,0,1,  14,15,12,13,10,11,8,9,6,7,4,5,2,3,0,1}; \
    static const __m256i* p_kefh_shuff = (const __m256i*) array_kefh_shuff; \
    ALIGNED_AVX __m256i kefh_shuff = * p_kefh_shuff;    
#endif

    M256I_W8x2C(kefh_0000,  90, 90,  90, 82,  88, 67,  85, 46);
    M256I_W8x2C(kefh_0001,  82, 22,  78, -4,  73,-31,  67,-54);
    M256I_W8x2C(kefh_0002,  88, 85,  67, 46,  31,-13, -13,-67);
    M256I_W8x2C(kefh_0003, -54,-90, -82,-73, -90,-22, -78, 38);
    M256I_W8x2C(kefh_0004,  82, 78,  22, -4, -54,-82, -90,-73);
    M256I_W8x2C(kefh_0005, -61, 13,  13, 85,  78, 67,  85,-22);
    M256I_W8x2C(kefh_0006,  73, 67, -31,-54, -90,-78, -22, 38);
    M256I_W8x2C(kefh_0007,  78, 85,  67,-22, -38,-90, -90,  4);
    M256I_W8x2C(kefh_0008,  61, 54, -73,-85, -46, -4,  82, 88);
    M256I_W8x2C(kefh_0009,  31,-46, -88,-61, -13, 82,  90, 13);
    M256I_W8x2C(kefh_0010,  46, 38, -90,-88,  38, 73,  54, -4);
    M256I_W8x2C(kefh_0011, -90,-67,  31, 90,  61,-46, -88,-31);
    M256I_W8x2C(kefh_0012,  31, 22, -78,-61,  90, 85, -61,-90);
    M256I_W8x2C(kefh_0013,   4, 73,  54,-38, -88, -4,  82, 46);
    M256I_W8x2C(kefh_0014,  13,  4, -38,-13,  61, 22, -78,-31);
    M256I_W8x2C(kefh_0015,  88, 38, -90,-46,  85, 54, -73,-61);

    M256I_W8x2C(kefh_0100,  61,-73,  54,-85,  46,-90,  38,-88);
    M256I_W8x2C(kefh_0101,  31,-78,  22,-61,  13,-38,   4,-13);
    M256I_W8x2C(kefh_0102, -46, 82,  -4, 88,  38, 54,  73, -4);
    M256I_W8x2C(kefh_0103,  90,-61,  85,-90,  61,-78,  22,-31);
    M256I_W8x2C(kefh_0104,  31,-88, -46,-61, -90, 31, -67, 90);
    M256I_W8x2C(kefh_0105,   4, 54,  73,-38,  88,-90,  38,-46);
    M256I_W8x2C(kefh_0106, -13, 90,  82, 13,  61,-88, -46,-31);
    M256I_W8x2C(kefh_0107, -88, 82,  -4, 46,  85,-73,  54,-61);
    M256I_W8x2C(kefh_0108,  -4,-90, -90, 38,  22, 67,  85,-78);
    M256I_W8x2C(kefh_0109, -38,-22, -78, 90,  54,-31,  67,-73);
    M256I_W8x2C(kefh_0110,  22, 85,  67,-78, -85, 13,  13, 61);
    M256I_W8x2C(kefh_0111,  73,-90, -82, 54,   4, 22,  78,-82);
    M256I_W8x2C(kefh_0112, -38,-78, -22, 90,  73,-82, -90, 54);
    M256I_W8x2C(kefh_0113,  67,-13, -13,-31, -46, 67,  85,-88);
    M256I_W8x2C(kefh_0114,  54, 67, -31,-73,   4, 78,  22,-82);
    M256I_W8x2C(kefh_0115, -46, 85,  67,-88, -82, 90,  90,-90);

    M256I_W8x2C(kefh_0200,  90, 87,  87, 57,  80,  9,  70,-43);
    M256I_W8x2C(kefh_0201,  57,-80,  43,-90,  25,-70,   9,-25);
    M256I_W8x2C(kefh_0202,  80, 70,   9,-43, -70,-87, -87,  9);
    M256I_W8x2C(kefh_0203, -25, 90,  57, 25,  90,-80,  43,-57);
    M256I_W8x2C(kefh_0204,  57, 43, -80,-90, -25, 57,  90, 25);
    M256I_W8x2C(kefh_0205,  -9,-87, -87, 70,  43,  9,  70,-80);
    M256I_W8x2C(kefh_0206,  25,  9, -70,-25,  90, 43, -80,-57);
    M256I_W8x2C(kefh_0207,  43, 70,   9,-80, -57, 87,  87,-90);

    M256I_W8x2C(kefh_0300,  89, 75,  75,-18,  50,-89,  18,-50);
    M256I_W8x2C(kefh_0301,  50, 18, -89,-50,  18, 75,  75,-89);
    M256I_W8x2C(kefh_0400,  64, 64,  64,-64,  83, 36,  36,-83);

    // koefs for vertical 1-D transform
    M256I_D4x2C(rounder_1k, 1024, 1024, 1024, 1024);

    M256I_W8x2C(kefv_0000,  90, 90,  67, 46, -54,-82, -22, 38);
    M256I_W8x2C(kefv_0001,  82, 22, -82,-73,  78, 67, -90,  4);
    M256I_W8x2C(kefv_0002,  88, 85,  22, -4, -90,-78,  85, 46);
    M256I_W8x2C(kefv_0003, -54,-90,  13, 85, -38,-90,  67,-54);
    M256I_W8x2C(kefv_0004,  82, 78, -31,-54,  88, 67, -13,-67);
    M256I_W8x2C(kefv_0005, -61, 13,  67,-22,  73,-31, -78, 38);
    M256I_W8x2C(kefv_0006,  73, 67,  90, 82,  31,-13, -90,-73);
    M256I_W8x2C(kefv_0007,  78, 85,  78, -4, -90,-22,  85,-22);
    M256I_W8x2C(kefv_0008,  61, 54, -90,-88,  90, 85, -78,-31);
    M256I_W8x2C(kefv_0009,  31,-46,  31, 90, -88, -4, -73,-61);
    M256I_W8x2C(kefv_0010,  46, 38, -78,-61,  61, 22,  82, 88);
    M256I_W8x2C(kefv_0011, -90,-67,  54,-38,  85, 54,  90, 13);
    M256I_W8x2C(kefv_0012,  31, 22, -38,-13, -46, -4,  54, -4);
    M256I_W8x2C(kefv_0013,   4, 73, -90,-46, -13, 82, -88,-31);
    M256I_W8x2C(kefv_0014,  13,  4, -73,-85,  38, 73, -61,-90);
    M256I_W8x2C(kefv_0015,  88, 38, -88,-61,  61,-46,  82, 46);

    M256I_W8x2C(kefv_0100,  61,-73,  -4, 88, -90, 31, -46,-31);
    M256I_W8x2C(kefv_0101,  31,-78,  85,-90,  88,-90,  54,-61);
    M256I_W8x2C(kefv_0102, -46, 82, -46,-61,  61,-88,  38,-88);
    M256I_W8x2C(kefv_0103,  90,-61,  73,-38,  85,-73,   4,-13);
    M256I_W8x2C(kefv_0104,  31,-88,  82, 13,  46,-90,  73, -4);
    M256I_W8x2C(kefv_0105,   4, 54,  -4, 46,  13,-38,  22,-31);
    M256I_W8x2C(kefv_0106, -13, 90,  54,-85,  38, 54, -67, 90);
    M256I_W8x2C(kefv_0107, -88, 82,  22,-61,  61,-78,  38,-46);
    M256I_W8x2C(kefv_0108,  -4,-90,  67,-78,  73,-82,  22,-82);
    M256I_W8x2C(kefv_0109, -38,-22, -82, 54, -46, 67,  90,-90);
    M256I_W8x2C(kefv_0110,  22, 85, -22, 90,   4, 78,  85,-78);
    M256I_W8x2C(kefv_0111,  73,-90, -13,-31, -82, 90,  67,-73);
    M256I_W8x2C(kefv_0112, -38,-78, -31,-73,  22, 67,  13, 61);
    M256I_W8x2C(kefv_0113,  67,-13,  67,-88,  54,-31,  78,-82);
    M256I_W8x2C(kefv_0114,  54, 67, -90, 38, -85, 13, -90, 54);
    M256I_W8x2C(kefv_0115, -46, 85, -78, 90,   4, 22,  85,-88);

    M256I_D4x2C(kefv_0200,  90,  57, -70,   9);
    M256I_D4x2C(kefv_0201,  57, -90,  90, -57);
    M256I_D4x2C(kefv_0202,  57, -90,  90, -57);
    M256I_D4x2C(kefv_0203,  -9,  70, -57, -90);
    M256I_D4x2C(kefv_0204,  87,   9, -87,  70);
    M256I_D4x2C(kefv_0205,  43, -70,  43,  90);
    M256I_D4x2C(kefv_0206, -80,  57, -80,   9);
    M256I_D4x2C(kefv_0207, -87,   9,  87,  70);
    M256I_D4x2C(kefv_0208,  80, -43,  80, -43);
    M256I_D4x2C(kefv_0209,  25, -25, -25,  25);
    M256I_D4x2C(kefv_0210, -25,  25,  25, -25);
    M256I_D4x2C(kefv_0211,  43, -80,  43, -80);
    M256I_D4x2C(kefv_0212,  70,  87,   9, -87);
    M256I_D4x2C(kefv_0213,   9, -80,  57, -80);
    M256I_D4x2C(kefv_0214,  90,  43, -70,  43);
    M256I_D4x2C(kefv_0215,  70, -87,   9,  87);

    M256I_D4x2C(kefv_0300,  64,  36, -64, -36);
    M256I_D4x2C(kefv_0301,  89, -18,  18, -89);
    M256I_D4x2C(kefv_0302,  64, -36,  64,  36);
    M256I_D4x2C(kefv_0303,  75, -89,  75,  18);
    M256I_D4x2C(kefv_0304,  64, -83,  64, -83);
    M256I_D4x2C(kefv_0305,  50, -50,  50, -50);
    M256I_D4x2C(kefv_0306,  64,  83, -64,  83);
    M256I_D4x2C(kefv_0307,  18,  75, -89,  75);
#endif

#define tmp_stride  32  // stride in temp[] buffer

    // 250 cpu clocks
    __inline void transpose1(short *__restrict dst, __m128i *__restrict buff)
    {
        __m256i y0, y1, y2, y3, y4, y5, y6, y7;
        //  _mm256_zeroall();

        for (int i=0; i<8; i++)
        {
            // load buffer short[16x4], rotate every 4x4, save [4x16]. Repeat 2 times -> transpose [32x4] -> [4x32]
            y0 =  _mm256_load_si256((const __m256i *)(buff + 0*4)); // 07 06 05 04 03 02 01 00
            y1 =  _mm256_load_si256((const __m256i *)(buff + 1*4)); // 17 16 15 14 13 12 11 10
            y2 =  _mm256_load_si256((const __m256i *)(buff + 2*4)); // 27 26 25 24 23 22 21 20
            y3 =  _mm256_load_si256((const __m256i *)(buff + 3*4)); // 37 36 35 34 33 32 31 30
            y6 = y0;
            y0 = _mm256_unpacklo_epi16(y0, y1);  // 13 03 12 02 11 01 10 00
            y6 = _mm256_unpackhi_epi16(y6, y1);  // 17 07 16 06 15 05 14 04
            y5 = y2;
            y4 = y0;
            y2 = _mm256_unpacklo_epi16(y2, y3);  // 33 23 32 22 31 21 30 20
            y5 = _mm256_unpackhi_epi16(y5, y3);  // 37 27 36 26 35 25 34 24
            y0 = _mm256_unpacklo_epi32(y0, y2);  // 31 21 11 01 30 20 10 00
            _mm_storel_epi64((__m128i*)(&dst[ 1*tmp_stride]), mm128(y0));
            _mm_storeh_epi64((__m128i*)(&dst[ 3*tmp_stride]), mm128(y0));
            y7 = y6;
            y4 = _mm256_unpackhi_epi32(y4, y2);  // 33 23 13 03 32 22 12 02
            _mm_storel_epi64((__m128i*)(&dst[ 5*tmp_stride]), mm128(y4));
            _mm_storeh_epi64((__m128i*)(&dst[ 7*tmp_stride]), mm128(y4));
            y6 = _mm256_unpacklo_epi32(y6, y5);  // 35 25 15 05 34 24 14 04
            _mm_storel_epi64((__m128i*)(&dst[ 9*tmp_stride]), mm128(y6));
            _mm_storeh_epi64((__m128i*)(&dst[11*tmp_stride]), mm128(y6));
            y7 = _mm256_unpackhi_epi32(y7, y5);  // 37 27 17 07 36 26 16 06
            _mm_storel_epi64((__m128i*)(&dst[13*tmp_stride]), mm128(y7));
            _mm_storeh_epi64((__m128i*)(&dst[15*tmp_stride]), mm128(y7));

            // 1th
            __m128i x0 = _mm256_extracti128_si256(y0, 1);
            __m128i x4 = _mm256_extracti128_si256(y4, 1);
            __m128i x6 = _mm256_extracti128_si256(y6, 1);
            __m128i x7 = _mm256_extracti128_si256(y7, 1);
            _mm_storel_epi64((__m128i*)(&dst[17*tmp_stride]), x0);
            _mm_storeh_epi64((__m128i*)(&dst[19*tmp_stride]), x0);
            _mm_storel_epi64((__m128i*)(&dst[21*tmp_stride]), x4);
            _mm_storeh_epi64((__m128i*)(&dst[23*tmp_stride]), x4);
            _mm_storel_epi64((__m128i*)(&dst[25*tmp_stride]), x6);
            _mm_storeh_epi64((__m128i*)(&dst[27*tmp_stride]), x6);
            _mm_storel_epi64((__m128i*)(&dst[29*tmp_stride]), x7);
            _mm_storeh_epi64((__m128i*)(&dst[31*tmp_stride]), x7);

            // 2th
            y0 =  _mm256_load_si256((const __m256i *)(buff + 0*4 + 2));
            y1 =  _mm256_load_si256((const __m256i *)(buff + 1*4 + 2));
            y2 =  _mm256_load_si256((const __m256i *)(buff + 2*4 + 2));
            y3 =  _mm256_load_si256((const __m256i *)(buff + 3*4 + 2));
            y6 = y0;
            y0 = _mm256_unpacklo_epi16(y0, y1);
            y6 = _mm256_unpackhi_epi16(y6, y1);
            y5 = y2;
            y4 = y0;
            y2 = _mm256_unpacklo_epi16(y2, y3);
            y5 = _mm256_unpackhi_epi16(y5, y3);
            y0 = _mm256_unpacklo_epi32(y0, y2);
            _mm_storel_epi64((__m128i*)(&dst[ 2*tmp_stride]), mm128(y0));
            _mm_storeh_epi64((__m128i*)(&dst[ 6*tmp_stride]), mm128(y0));
            y7 = y6;
            y4 = _mm256_unpackhi_epi32(y4, y2);
            _mm_storel_epi64((__m128i*)(&dst[10*tmp_stride]), mm128(y4));
            _mm_storeh_epi64((__m128i*)(&dst[14*tmp_stride]), mm128(y4));
            y6 = _mm256_unpacklo_epi32(y6, y5);
            _mm_storel_epi64((__m128i*)(&dst[18*tmp_stride]), mm128(y6));
            _mm_storeh_epi64((__m128i*)(&dst[22*tmp_stride]), mm128(y6));
            y7 = _mm256_unpackhi_epi32(y7, y5);
            _mm_storel_epi64((__m128i*)(&dst[26*tmp_stride]), mm128(y7));
            _mm_storeh_epi64((__m128i*)(&dst[30*tmp_stride]), mm128(y7));

            // 3th
            x0 = _mm256_extracti128_si256(y0, 1);
            x4 = _mm256_extracti128_si256(y4, 1);
            x6 = _mm256_extracti128_si256(y6, 1);
            x7 = _mm256_extracti128_si256(y7, 1);
            _mm_storel_epi64((__m128i*)(&dst[ 4*tmp_stride]), x0);
            _mm_storeh_epi64((__m128i*)(&dst[12*tmp_stride]), x0);
            _mm_storel_epi64((__m128i*)(&dst[20*tmp_stride]), x4);
            _mm_storeh_epi64((__m128i*)(&dst[28*tmp_stride]), x4);
            _mm_storel_epi64((__m128i*)(&dst[ 0*tmp_stride]), x6);
            _mm_storeh_epi64((__m128i*)(&dst[16*tmp_stride]), x6);
            _mm_storel_epi64((__m128i*)(&dst[ 8*tmp_stride]), x7);
            _mm_storeh_epi64((__m128i*)(&dst[24*tmp_stride]), x7);
            buff += 16;
            dst += 4;
        }
    }

    static __inline void transpose2(short *__restrict dst, __m128i *__restrict buff)
    {
        __m256i y0, y1, y2, y3, y4, y5, y6, y7;
        //  _mm256_zeroall();

        for (int i=0; i<8; i++)
        {
            // load buffer short[16x4], rotate every 4x4, save [4x16]. Repeat 2 times -> transpose [32x4] -> [4x32]
            y0 =  _mm256_load_si256((const __m256i *)(buff + 0*4)); // 07 06 05 04 03 02 01 00
            y1 =  _mm256_load_si256((const __m256i *)(buff + 1*4)); // 17 16 15 14 13 12 11 10
            y2 =  _mm256_load_si256((const __m256i *)(buff + 2*4)); // 27 26 25 24 23 22 21 20
            y3 =  _mm256_load_si256((const __m256i *)(buff + 3*4)); // 37 36 35 34 33 32 31 30
            y6 = y0;
            y0 = _mm256_unpacklo_epi16(y0, y1);  // 13 03 12 02 11 01 10 00
            y6 = _mm256_unpackhi_epi16(y6, y1);  // 17 07 16 06 15 05 14 04
            y5 = y2;
            y4 = y0;
            y2 = _mm256_unpacklo_epi16(y2, y3);  // 33 23 32 22 31 21 30 20
            y5 = _mm256_unpackhi_epi16(y5, y3);  // 37 27 36 26 35 25 34 24
            y0 = _mm256_unpacklo_epi32(y0, y2);  // 31 21 11 01 30 20 10 00
            _mm_storel_epi64((__m128i*)(&dst[ 1*tmp_stride]), mm128(y0));
            _mm_storeh_epi64((__m128i*)(&dst[ 3*tmp_stride]), mm128(y0));
            y7 = y6;
            y4 = _mm256_unpackhi_epi32(y4, y2);  // 33 23 13 03 32 22 12 02
            _mm_storel_epi64((__m128i*)(&dst[ 5*tmp_stride]), mm128(y4));
            _mm_storeh_epi64((__m128i*)(&dst[ 7*tmp_stride]), mm128(y4));
            y6 = _mm256_unpacklo_epi32(y6, y5);  // 35 25 15 05 34 24 14 04
            _mm_storel_epi64((__m128i*)(&dst[ 9*tmp_stride]), mm128(y6));
            _mm_storeh_epi64((__m128i*)(&dst[11*tmp_stride]), mm128(y6));
            y7 = _mm256_unpackhi_epi32(y7, y5);  // 37 27 17 07 36 26 16 06
            _mm_storel_epi64((__m128i*)(&dst[13*tmp_stride]), mm128(y7));
            _mm_storeh_epi64((__m128i*)(&dst[15*tmp_stride]), mm128(y7));

            // 1th
            __m128i x0 = _mm256_extracti128_si256(y0, 1);
            __m128i x4 = _mm256_extracti128_si256(y4, 1);
            __m128i x6 = _mm256_extracti128_si256(y6, 1);
            __m128i x7 = _mm256_extracti128_si256(y7, 1);
            _mm_storel_epi64((__m128i*)(&dst[17*tmp_stride]), x0);
            _mm_storeh_epi64((__m128i*)(&dst[19*tmp_stride]), x0);
            _mm_storel_epi64((__m128i*)(&dst[21*tmp_stride]), x4);
            _mm_storeh_epi64((__m128i*)(&dst[23*tmp_stride]), x4);
            _mm_storel_epi64((__m128i*)(&dst[25*tmp_stride]), x6);
            _mm_storeh_epi64((__m128i*)(&dst[27*tmp_stride]), x6);
            _mm_storel_epi64((__m128i*)(&dst[29*tmp_stride]), x7);
            _mm_storeh_epi64((__m128i*)(&dst[31*tmp_stride]), x7);

            // 2th
            y0 =  _mm256_load_si256((const __m256i *)(buff + 0*4 + 2));
            y1 =  _mm256_load_si256((const __m256i *)(buff + 1*4 + 2));
            y2 =  _mm256_load_si256((const __m256i *)(buff + 2*4 + 2));
            y3 =  _mm256_load_si256((const __m256i *)(buff + 3*4 + 2));
            y6 = y0;
            y0 = _mm256_unpacklo_epi16(y0, y1);
            y6 = _mm256_unpackhi_epi16(y6, y1);
            y5 = y2;
            y4 = y0;
            y2 = _mm256_unpacklo_epi16(y2, y3);
            y5 = _mm256_unpackhi_epi16(y5, y3);
            y0 = _mm256_unpacklo_epi32(y0, y2);
            _mm_storel_epi64((__m128i*)(&dst[ 2*tmp_stride]), mm128(y0));
            _mm_storeh_epi64((__m128i*)(&dst[ 6*tmp_stride]), mm128(y0));
            y7 = y6;
            y4 = _mm256_unpackhi_epi32(y4, y2);
            _mm_storel_epi64((__m128i*)(&dst[10*tmp_stride]), mm128(y4));
            _mm_storeh_epi64((__m128i*)(&dst[14*tmp_stride]), mm128(y4));
            y6 = _mm256_unpacklo_epi32(y6, y5);
            _mm_storel_epi64((__m128i*)(&dst[18*tmp_stride]), mm128(y6));
            _mm_storeh_epi64((__m128i*)(&dst[22*tmp_stride]), mm128(y6));
            y7 = _mm256_unpackhi_epi32(y7, y5);
            _mm_storel_epi64((__m128i*)(&dst[26*tmp_stride]), mm128(y7));
            _mm_storeh_epi64((__m128i*)(&dst[30*tmp_stride]), mm128(y7));

            // 3th
            x0 = _mm256_extracti128_si256(y0, 1);
            x4 = _mm256_extracti128_si256(y4, 1);
            x6 = _mm256_extracti128_si256(y6, 1);
            x7 = _mm256_extracti128_si256(y7, 1);
            _mm_storel_epi64((__m128i*)(&dst[ 0*tmp_stride]), x0);  // <-- here is a diference from transpose1()
            _mm_storeh_epi64((__m128i*)(&dst[ 8*tmp_stride]), x0);
            _mm_storel_epi64((__m128i*)(&dst[16*tmp_stride]), x4);
            _mm_storeh_epi64((__m128i*)(&dst[24*tmp_stride]), x4);
            _mm_storel_epi64((__m128i*)(&dst[ 4*tmp_stride]), x6);
            _mm_storeh_epi64((__m128i*)(&dst[12*tmp_stride]), x6);
            _mm_storel_epi64((__m128i*)(&dst[20*tmp_stride]), x7);
            _mm_storeh_epi64((__m128i*)(&dst[28*tmp_stride]), x7);
            buff += 16;
            dst += 4;
        }
    }

    #define SHIFT_FRW32_1ST_BASE 4

    //void FASTCALL fwd_32x32_dct_avx2(short *__restrict src, short *__restrict dest)
    template <int SHIFT_FRW32_1ST>
    static void h265_DCT32x32Fwd_16s_Kernel(const short *H265_RESTRICT src, Ipp32s srcStride, short *H265_RESTRICT dst)
    {
        short ALIGN_DECL(32) temp[32*32];
        // temporal buffer short[32*4]. Intermediate results will be stored here. Rotate 4x4 and moved to temp[]
        __m128i ALIGN_DECL(32) buff[16*8];
        int num = 0;
        __m256i y0, y1, y2, y3, y4, y5, y6, y7;
        _mm256_zeroall();

        // Horizontal 1-D forward transform

        for (int i=0; i<32; i+=2) 
        {
            /* Read input data (32 signed shorts)            
            */
            y4 = _mm256_load_si256((const __m256i *)(src + srcStride*0)); // a15..a08 | a07..a00   first line
            y5 = _mm256_load_si256((const __m256i *)(src + srcStride*0+16)); // a31..a24 | a23..a16
            y2 = _mm256_load_si256((const __m256i *)(src + srcStride*1)); // b15..b08 | b07..b00   second line
            y3 = _mm256_load_si256((const __m256i *)(src + srcStride*1+16)); // b31..b24 | b23..b16

            y5 = _mm256_shuffle_epi8(y5, kefh_shuff);     // a24 a25 a26 a27 a28 a29 a30 a31 | a16 a17 a18 a19 a20 a21 a22 a23
            y3 = _mm256_shuffle_epi8(y3, kefh_shuff);     // b24 b25 b26 b27 b28 b29 b30 b31 | b16 b17 b18 b19 b20 b21 b22 b23
            y0 = _mm256_permute2x128_si256(y4, y2, 0x20); // low parts : b07..b00 | a07..a00 
            y1 = _mm256_permute2x128_si256(y4, y2, 0x31); // high parts: b15..b08 | a15..a08 
            y2 = _mm256_permute2x128_si256(y5, y3, 0x20); // low parts : b16..b23 | a16..a23 
            y3 = _mm256_permute2x128_si256(y5, y3, 0x31); // high parts: b24..b31 | a24..a31 

#ifdef _SHORT_STYLE_
            y0 = _mm256_sub_epi16(y0, y3);
            y1 = _mm256_sub_epi16(y1, y2);
            y3 = _mm256_add_epi16(y3, y3);
            y2 = _mm256_add_epi16(y2, y2);
            y3 = _mm256_add_epi16(y3, y0);
            y2 = _mm256_add_epi16(y2, y1);

            y5 = _mm256_shuffle_epi32(y0, 0);
            y4 = _mm256_madd_epi16(y5, kefh_0000);
            y5 = _mm256_madd_epi16(y5, kefh_0001);

            y6 = _mm256_shuffle_epi32(y0, 0x55);
            y7 = _mm256_madd_epi16(y6, kefh_0002);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0003);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y0, 0xAA);
            y7 = _mm256_madd_epi16(y6, kefh_0004);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0005);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y0, 0xFF);
            y7 = _mm256_madd_epi16(y6, kefh_0006);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0007);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y1, 0);
            y7 = _mm256_madd_epi16(y6, kefh_0008);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0009);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y1, 0x55);
            y7 = _mm256_madd_epi16(y6, kefh_0010);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0011);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y1, 0xAA);
            y7 = _mm256_madd_epi16(y6, kefh_0012);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0013);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y1, 0xFF);
            y7 = _mm256_madd_epi16(y6, kefh_0014);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0015);
            y5 = _mm256_add_epi32(y5,  y6);

            if (SHIFT_FRW32_1ST == 4) {
                y4 = _mm256_add_epi32(y4, rounder_8);
                y5 = _mm256_add_epi32(y5, rounder_8);
            } else if (SHIFT_FRW32_1ST == 5) {
                y4 = _mm256_add_epi32(y4, rounder_16);
                y5 = _mm256_add_epi32(y5, rounder_16);
            } else if (SHIFT_FRW32_1ST == 6) {
                y4 = _mm256_add_epi32(y4, rounder_32);
                y5 = _mm256_add_epi32(y5, rounder_32);
            }
            y4 = _mm256_srai_epi32(y4,  SHIFT_FRW32_1ST);   // (r03 r02 r01 r00 + add) >>= SHIFT_FRW32_1ST
            y5 = _mm256_srai_epi32(y5,  SHIFT_FRW32_1ST);   // (r07 r06 r05 r04 + add) >>= SHIFT_FRW32_1ST
            y4 = _mm256_packs_epi32(y4, y5);      // r07 r06 r05 r04 r03 r02 r01 r00: clip(-32768, 32767)

            // Store first results
            buff[num]     = mm128(y4);
            buff[num + 4] = _mm256_extracti128_si256(y4, 1);
            num++;

            y5 = _mm256_shuffle_epi32(y0, 0x00);
            y4 = _mm256_madd_epi16(y5, kefh_0100);
            y5 = _mm256_madd_epi16(y5, kefh_0101);

            y6 = _mm256_shuffle_epi32(y0, 0x55);
            y7 = _mm256_madd_epi16(y6, kefh_0102);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0103);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y0, 0xAA);
            y7 = _mm256_madd_epi16(y6, kefh_0104);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0105);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y0, 0xFF);
            y7 = _mm256_madd_epi16(y6, kefh_0106);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0107);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y1, 0x00);
            y7 = _mm256_madd_epi16(y6, kefh_0108);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0109);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y1, 0x55);
            y7 = _mm256_madd_epi16(y6, kefh_0110);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0111);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y1, 0xAA);
            y7 = _mm256_madd_epi16(y6, kefh_0112);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0113);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y1, 0xFF);
            y7 = _mm256_madd_epi16(y6, kefh_0114);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0115);
            y5 = _mm256_add_epi32(y5,  y6);

            if (SHIFT_FRW32_1ST == 4) {
                y4 = _mm256_add_epi32(y4, rounder_8);
                y5 = _mm256_add_epi32(y5, rounder_8);
            } else if (SHIFT_FRW32_1ST == 5) {
                y4 = _mm256_add_epi32(y4, rounder_16);
                y5 = _mm256_add_epi32(y5, rounder_16);
            } else if (SHIFT_FRW32_1ST == 6) {
                y4 = _mm256_add_epi32(y4, rounder_32);
                y5 = _mm256_add_epi32(y5, rounder_32);
            }
            y4 = _mm256_srai_epi32(y4,  SHIFT_FRW32_1ST);   // (r11 r10 r09 r08 + add) >>= SHIFT_FRW32_1ST
            y5 = _mm256_srai_epi32(y5,  SHIFT_FRW32_1ST);   // (r15 r14 r13 r12 + add) >>= SHIFT_FRW32_1ST
            y4 = _mm256_packs_epi32(y4, y5);      // r15 r14 r13 r12 r11 r10 r09 r08: clip(-32768, 32767)

            // Store second results
            buff[num]     = mm128(y4);
            buff[num + 4] = _mm256_extracti128_si256(y4, 1);
            num++;

            // 2nd part.
            y2 = _mm256_shufflelo_epi16(y2, 0x1b); 
            y2 = _mm256_shufflehi_epi16(y2, 0x1b); 
            y2 = _mm256_shuffle_epi32(y2, 0x4e);   // a08 a09 a10 a11 a12 a13 a14 a15

            y3 = _mm256_sub_epi16(y3,  y2);
            y2 = _mm256_add_epi16(y2,  y2);
            y2 = _mm256_add_epi16(y2,  y3);

            y5 = _mm256_shuffle_epi32(y3, 0x00);
            y4 = _mm256_madd_epi16(y5, kefh_0200);
            y5 = _mm256_madd_epi16(y5, kefh_0201);

            y6 = _mm256_shuffle_epi32(y3, 0x55);
            y7 = _mm256_madd_epi16(y6, kefh_0202);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0203);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y3, 0xAA);
            y7 = _mm256_madd_epi16(y6, kefh_0204);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0205);
            y5 = _mm256_add_epi32(y5,  y6);

            y6 = _mm256_shuffle_epi32(y3, 0xFF);
            y7 = _mm256_madd_epi16(y6, kefh_0206);
            y4 = _mm256_add_epi32(y4,  y7);
            y6 = _mm256_madd_epi16(y6, kefh_0207);
            y5 = _mm256_add_epi32(y5,  y6);

            if (SHIFT_FRW32_1ST == 4) {
                y4 = _mm256_add_epi32(y4, rounder_8);
                y5 = _mm256_add_epi32(y5, rounder_8);
            } else if (SHIFT_FRW32_1ST == 5) {
                y4 = _mm256_add_epi32(y4, rounder_16);
                y5 = _mm256_add_epi32(y5, rounder_16);
            } else if (SHIFT_FRW32_1ST == 6) {
                y4 = _mm256_add_epi32(y4, rounder_32);
                y5 = _mm256_add_epi32(y5, rounder_32);
            }
            y4 = _mm256_srai_epi32(y4,  SHIFT_FRW32_1ST);   // (r03 r02 r01 r00 + add) >>= SHIFT_FRW32_1ST
            y5 = _mm256_srai_epi32(y5,  SHIFT_FRW32_1ST);   // (r07 r06 r05 r04 + add) >>= SHIFT_FRW32_1ST
            y4 = _mm256_packs_epi32(y4, y5);      // r07 r06 r05 r04 r03 r02 r01 r00

            // Store 3th results
            buff[num]     = mm128(y4);
            buff[num + 4] = _mm256_extracti128_si256(y4, 1);
            num++;

            // 3rd part.
            y3 = _mm256_shufflehi_epi16(y2, 0x1b); // c04 c05 c06 c07 c03 c02 c01 c00
            y3 = _mm256_unpackhi_epi64(y3, y3);    //   x   x   x   x c04 c05 c06 c07

            y2 = _mm256_sub_epi16(y2,  y3);
            y3 = _mm256_add_epi16(y3,  y3);
            y3 = _mm256_add_epi16(y3,  y2);

            y4 = _mm256_shuffle_epi32(y2, 0x00);
            y5 = _mm256_shuffle_epi32(y2, 0x55);
            y4 = _mm256_madd_epi16(y4, kefh_0300);
            y5 = _mm256_madd_epi16(y5, kefh_0301);
            y4 = _mm256_add_epi32(y4,  y5);

            y6 = _mm256_shufflelo_epi16(y3, 0x1b);

            y3 = _mm256_sub_epi16(y3, y6);
            y6 = _mm256_add_epi16(y6, y6);
            y6 = _mm256_add_epi16(y6, y3);

            y6 = _mm256_unpacklo_epi32(y6, y3);
            y6 = _mm256_unpacklo_epi32(y6, y6);
            y5 = _mm256_madd_epi16(y6, kefh_0400);

            if (SHIFT_FRW32_1ST == 4) {
                y4 = _mm256_add_epi32(y4, rounder_8);
                y5 = _mm256_add_epi32(y5, rounder_8);
            } else if (SHIFT_FRW32_1ST == 5) {
                y4 = _mm256_add_epi32(y4, rounder_16);
                y5 = _mm256_add_epi32(y5, rounder_16);
            } else if (SHIFT_FRW32_1ST == 6) {
                y4 = _mm256_add_epi32(y4, rounder_32);
                y5 = _mm256_add_epi32(y5, rounder_32);
            }
            y4 = _mm256_srai_epi32(y4,  SHIFT_FRW32_1ST);
            y5 = _mm256_srai_epi32(y5,  SHIFT_FRW32_1ST);
            y4 = _mm256_packs_epi32(y4, y5);

            // Store 4th results
            buff[num]     = mm128(y4);
            buff[num + 4] = _mm256_extracti128_si256(y4, 1);
            num += 5;
#else
            y4 = _mm256_load_const(&kefh_shuff);
            y0 = _mm256_sub_epi16(y0, y3);
            y1 = _mm256_sub_epi16(y1, y2);
            y3 = _mm256_add_epi16(y3, y3);
            y2 = _mm256_add_epi16(y2, y2);
            y3 = _mm256_add_epi16(y3, y0);
            y2 = _mm256_add_epi16(y2, y1);

            y4 = _mm256_load_const(&kefh_0000);
            y5 = _mm256_shuffle_epi32(y0, 0);
            y7 = _mm256_load_const(&kefh_0001);
            y4 = _mm256_madd_epi16(y4, y5);
            y5 = _mm256_madd_epi16(y5, y7);

            y7 = _mm256_load_const(&kefh_0002);
            y6 = _mm256_shuffle_epi32(y0, 0x55);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0003);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0004);
            y6 = _mm256_shuffle_epi32(y0, 0xAA);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0005);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0006);
            y6 = _mm256_shuffle_epi32(y0, 0xFF);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0007);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0008);
            y6 = _mm256_shuffle_epi32(y1, 0);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0009);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0010);
            y6 = _mm256_shuffle_epi32(y1, 0x55);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0011);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0012);
            y6 = _mm256_shuffle_epi32(y1, 0xAA);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0013);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0014);
            y6 = _mm256_shuffle_epi32(y1, 0xFF);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0015);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            if (SHIFT_FRW32_1ST == 4) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_8));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_8));
            } else if (SHIFT_FRW32_1ST == 5) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_16));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_16));
            } else if (SHIFT_FRW32_1ST == 6) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_32));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_32));
            }
            y4 = _mm256_srai_epi32(y4,  SHIFT_FRW32_1ST);   // (r03 r02 r01 r00 + add) >>= SHIFT_FRW32_1ST
            y5 = _mm256_srai_epi32(y5,  SHIFT_FRW32_1ST);   // (r07 r06 r05 r04 + add) >>= SHIFT_FRW32_1ST
            y4 = _mm256_packs_epi32(y4, y5);      // r07 r06 r05 r04 r03 r02 r01 r00: clip(-32768, 32767)

            // Store first results
            buff[num]     = mm128(y4);
            buff[num + 4] = _mm256_extracti128_si256(y4, 1);
            num++;

            y4 = _mm256_load_const(&kefh_0100);
            y5 = _mm256_shuffle_epi32(y0, 0x00);
            y7 = _mm256_load_const(&kefh_0101);
            y4 = _mm256_madd_epi16(y4, y5);
            y5 = _mm256_madd_epi16(y5, y7);

            y7 = _mm256_load_const(&kefh_0102);
            y6 = _mm256_shuffle_epi32(y0, 0x55);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0103);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0104);
            y6 = _mm256_shuffle_epi32(y0, 0xAA);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0105);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0106);
            y6 = _mm256_shuffle_epi32(y0, 0xFF);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0107);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0108);
            y6 = _mm256_shuffle_epi32(y1, 0x00);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0109);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0110);
            y6 = _mm256_shuffle_epi32(y1, 0x55);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0111);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0112);
            y6 = _mm256_shuffle_epi32(y1, 0xAA);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0113);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0114);
            y6 = _mm256_shuffle_epi32(y1, 0xFF);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0115);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            if (SHIFT_FRW32_1ST == 4) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_8));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_8));
            } else if (SHIFT_FRW32_1ST == 5) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_16));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_16));
            } else if (SHIFT_FRW32_1ST == 6) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_32));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_32));
            }
            y4 = _mm256_srai_epi32(y4,  SHIFT_FRW32_1ST);   // (r11 r10 r09 r08 + add) >>= SHIFT_FRW32_1ST
            y5 = _mm256_srai_epi32(y5,  SHIFT_FRW32_1ST);   // (r15 r14 r13 r12 + add) >>= SHIFT_FRW32_1ST
            y4 = _mm256_packs_epi32(y4, y5);      // r15 r14 r13 r12 r11 r10 r09 r08: clip(-32768, 32767)

            // Store second results
            buff[num]     = mm128(y4);
            buff[num + 4] = _mm256_extracti128_si256(y4, 1);
            num++;

            // 2nd part.
            y2 = _mm256_shufflelo_epi16(y2, 0x1b); 
            y2 = _mm256_shufflehi_epi16(y2, 0x1b); 
            y2 = _mm256_shuffle_epi32(y2, 0x4e);   // a08 a09 a10 a11 a12 a13 a14 a15

            y3 = _mm256_sub_epi16(y3,  y2);
            y2 = _mm256_add_epi16(y2,  y2);
            y2 = _mm256_add_epi16(y2,  y3);

            y4 = _mm256_load_const(&kefh_0200);
            y5 = _mm256_shuffle_epi32(y3, 0x00);
            y7 = _mm256_load_const(&kefh_0201);
            y4 = _mm256_madd_epi16(y4, y5);
            y5 = _mm256_madd_epi16(y5, y7);

            y7 = _mm256_load_const(&kefh_0202);
            y6 = _mm256_shuffle_epi32(y3, 0x55);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0203);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0204);
            y6 = _mm256_shuffle_epi32(y3, 0xAA);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0205);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            y7 = _mm256_load_const(&kefh_0206);
            y6 = _mm256_shuffle_epi32(y3, 0xFF);
            y7 = _mm256_madd_epi16(y7, y6);
            y4 = _mm256_add_epi32(y4,  y7);
            y7 = _mm256_load_const(&kefh_0207);
            y6 = _mm256_madd_epi16(y6, y7);
            y5 = _mm256_add_epi32(y5,  y6);

            if (SHIFT_FRW32_1ST == 4) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_8));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_8));
            } else if (SHIFT_FRW32_1ST == 5) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_16));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_16));
            } else if (SHIFT_FRW32_1ST == 6) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_32));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_32));
            }
            y4 = _mm256_srai_epi32(y4,  SHIFT_FRW32_1ST);   // (r03 r02 r01 r00 + add) >>= SHIFT_FRW32_1ST
            y5 = _mm256_srai_epi32(y5,  SHIFT_FRW32_1ST);   // (r07 r06 r05 r04 + add) >>= SHIFT_FRW32_1ST
            y4 = _mm256_packs_epi32(y4, y5);      // r07 r06 r05 r04 r03 r02 r01 r00

            // Store 3th results
            buff[num]     = mm128(y4);
            buff[num + 4] = _mm256_extracti128_si256(y4, 1);
            num++;

            // 3rd part.
            y3 = _mm256_shufflehi_epi16(y2, 0x1b); // c04 c05 c06 c07 c03 c02 c01 c00
            y3 = _mm256_unpackhi_epi64(y3, y3);    //   x   x   x   x c04 c05 c06 c07

            y2 = _mm256_sub_epi16(y2,  y3);
            y3 = _mm256_add_epi16(y3,  y3);
            y3 = _mm256_add_epi16(y3,  y2);

            y6 = _mm256_load_const(&kefh_0300);
            y4 = _mm256_shuffle_epi32(y2, 0x00);
            y7 = _mm256_load_const(&kefh_0301);
            y5 = _mm256_shuffle_epi32(y2, 0x55);
            y4 = _mm256_madd_epi16(y4, y6);
            y5 = _mm256_madd_epi16(y5, y7);
            y4 = _mm256_add_epi32(y4,  y5);

            y6 = _mm256_shufflelo_epi16(y3, 0x1b);

            y3 = _mm256_sub_epi16(y3,  y6);
            y6 = _mm256_add_epi16(y6,  y6);
            y6 = _mm256_add_epi16(y6,  y3);

            y6 = _mm256_unpacklo_epi32(y6, y3);
            y5 = _mm256_load_const(&kefh_0400);
            y6 = _mm256_unpacklo_epi32(y6, y6);
            y5 = _mm256_madd_epi16(y5, y6);

            if (SHIFT_FRW32_1ST == 4) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_8));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_8));
            } else if (SHIFT_FRW32_1ST == 5) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_16));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_16));
            } else if (SHIFT_FRW32_1ST == 6) {
                y4 = _mm256_add_epi32(y4,  _mm256_load_const(&rounder_32));
                y5 = _mm256_add_epi32(y5,  _mm256_load_const(&rounder_32));
            }
            y4 = _mm256_srai_epi32(y4,  SHIFT_FRW32_1ST);
            y5 = _mm256_srai_epi32(y5,  SHIFT_FRW32_1ST);
            y4 = _mm256_packs_epi32(y4, y5);

            // Store 4th results
            buff[num]     = mm128(y4);
            buff[num + 4] = _mm256_extracti128_si256(y4, 1);
            num += 5;
#endif
            src += 2*srcStride;
        }

        transpose1(temp, buff);

        // Vertical 1-D forward transform

#define shift       11
#define dst_stride  32  // linear output buffer

        short *tmp = temp;
        num = 0;

        for (int i=0; i<32; i+=2) 
        {
            // load two horizontal lines (first line - into low parts of ymm, second - into high parts)
            y4 = _mm256_load_si256((const __m256i *)(tmp + 16*0));    // a15..a08 | a07..a00   first line
            y5 = _mm256_load_si256((const __m256i *)(tmp + 16*1));    // a31..a24 | a23..a16
            y2 = _mm256_load_si256((const __m256i *)(tmp + 16*2));    // b15..b08 | b07..b00   second line
            y3 = _mm256_load_si256((const __m256i *)(tmp + 16*3));    // b31..b24 | b23..b16
            y0 = _mm256_permute2x128_si256(y4, y2, 0x20); // low parts : b07..b00 | a07..a00 
            y1 = _mm256_permute2x128_si256(y4, y2, 0x31); // high parts: b15..b08 | a15..a08 
            y2 = _mm256_permute2x128_si256(y5, y3, 0x20); // low parts : b23..b16 | a23..a16 
            y3 = _mm256_permute2x128_si256(y5, y3, 0x31); // high parts: b31..b24 | a31..a24

            y2 = _mm256_shuffle_epi32(y2, 0x4e);
            y3 = _mm256_shuffle_epi32(y3, 0x4e);
            y2 = _mm256_shufflelo_epi16(y2, 0x1b); 
            y3 = _mm256_shufflelo_epi16(y3, 0x1b); 
            y2 = _mm256_shufflehi_epi16(y2, 0x1b); // a16 a17 a18 a19 a20 a21 a22 a23
            y3 = _mm256_shufflehi_epi16(y3, 0x1b); // a24 a25 a26 a27 a28 a29 a30 a31

#ifdef _SHORT_STYLE_
            y4 = _mm256_madd_epi16(y0, kefv_0000);
            y5 = _mm256_madd_epi16(y3, kefv_0000);
            y6 = _mm256_add_epi32(y4, rounder_1k);
            y6 = _mm256_sub_epi32(y6, y5);
            y4 = _mm256_madd_epi16(y0, kefv_0001);
            y5 = _mm256_madd_epi16(y3, kefv_0001);
            y7 = _mm256_add_epi32(y4, rounder_1k);
            y7 = _mm256_sub_epi32(y7,  y5);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y4 = _mm256_madd_epi16(y0, kefv_0002);
            y5 = _mm256_madd_epi16(y3, kefv_0002);
            y6 = _mm256_add_epi32(y6,  y4);
            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y0, kefv_0003);
            y5 = _mm256_madd_epi16(y3, kefv_0003);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y4 = _mm256_madd_epi16(y0, kefv_0004);
            y5 = _mm256_madd_epi16(y3, kefv_0004);
            y6 = _mm256_add_epi32(y6,  y4);
            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y0, kefv_0005);
            y5 = _mm256_madd_epi16(y3, kefv_0005);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y4 = _mm256_madd_epi16(y0, kefv_0006);
            y5 = _mm256_madd_epi16(y3, kefv_0006);
            y6 = _mm256_add_epi32(y6,  y4);
            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y0, kefv_0007);
            y5 = _mm256_madd_epi16(y3, kefv_0007);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y4 = _mm256_madd_epi16(y1, kefv_0008);
            y5 = _mm256_madd_epi16(y2, kefv_0008);
            y6 = _mm256_add_epi32(y6,  y4);
            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y1, kefv_0009);
            y5 = _mm256_madd_epi16(y2, kefv_0009);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y4 = _mm256_madd_epi16(y1, kefv_0010);
            y5 = _mm256_madd_epi16(y2, kefv_0010);
            y6 = _mm256_add_epi32(y6,  y4);
            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y1, kefv_0011);
            y5 = _mm256_madd_epi16(y2, kefv_0011);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y4 = _mm256_madd_epi16(y1, kefv_0012);
            y5 = _mm256_madd_epi16(y2, kefv_0012);
            y6 = _mm256_add_epi32(y6,  y4);
            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y1, kefv_0013);
            y5 = _mm256_madd_epi16(y2, kefv_0013);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y4 = _mm256_madd_epi16(y1, kefv_0014);
            y5 = _mm256_madd_epi16(y2, kefv_0014);
            y6 = _mm256_add_epi32(y6,  y4);
            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y1, kefv_0015);
            y5 = _mm256_madd_epi16(y2, kefv_0015);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y6 = _mm256_srai_epi32(y6,  shift);
            y7 = _mm256_srai_epi32(y7,  shift);
            y6 = _mm256_packs_epi32(y6, y7);

            // Store first results
            buff[num]     = mm128(y6);
            buff[num + 4] = _mm256_extracti128_si256(y6, 1);
            num++;

            y4 = _mm256_madd_epi16(y0, kefv_0100);
            y5 = _mm256_madd_epi16(y3, kefv_0100);
            y6 = _mm256_add_epi32(y4, rounder_1k);

            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y0, kefv_0101);
            y5 = _mm256_madd_epi16(y3, kefv_0101);
            y7 = _mm256_add_epi32(y4, rounder_1k);
            y7 = _mm256_sub_epi32(y7,  y5);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y4 = _mm256_madd_epi16(y0, kefv_0102);
            y5 = _mm256_madd_epi16(y3, kefv_0102);
            y6 = _mm256_add_epi32(y6,  y4);

            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y0, kefv_0103);
            y5 = _mm256_madd_epi16(y3, kefv_0103);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y4 = _mm256_madd_epi16(y0, kefv_0104);
            y5 = _mm256_madd_epi16(y3, kefv_0104);
            y6 = _mm256_add_epi32(y6,  y4);

            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y0, kefv_0105);
            y5 = _mm256_madd_epi16(y3, kefv_0105);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y4 = _mm256_madd_epi16(y0, kefv_0106);
            y5 = _mm256_madd_epi16(y3, kefv_0106);
            y6 = _mm256_add_epi32(y6,  y4);

            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y0, kefv_0107);
            y5 = _mm256_madd_epi16(y3, kefv_0107);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y4 = _mm256_madd_epi16(y1, kefv_0108);
            y5 = _mm256_madd_epi16(y2, kefv_0108);
            y6 = _mm256_add_epi32(y6,  y4);

            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y1, kefv_0109);
            y5 = _mm256_madd_epi16(y2, kefv_0109);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y4 = _mm256_madd_epi16(y1, kefv_0110);
            y5 = _mm256_madd_epi16(y2, kefv_0110);
            y6 = _mm256_add_epi32(y6,  y4);

            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y1, kefv_0111);
            y5 = _mm256_madd_epi16(y2, kefv_0111);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y4 = _mm256_madd_epi16(y1, kefv_0112);
            y5 = _mm256_madd_epi16(y2, kefv_0112);
            y6 = _mm256_add_epi32(y6,  y4);

            y6 = _mm256_sub_epi32(y6,  y5);
            y4 = _mm256_madd_epi16(y1, kefv_0113);
            y5 = _mm256_madd_epi16(y2, kefv_0113);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y4 = _mm256_madd_epi16(y1, kefv_0114);
            y5 = _mm256_madd_epi16(y2, kefv_0114);
            y6 = _mm256_add_epi32(y6,  y4);
            y6 = _mm256_sub_epi32(y6,  y5);

            y4 = _mm256_madd_epi16(y1, kefv_0115);
            y5 = _mm256_madd_epi16(y2, kefv_0115);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y6 = _mm256_srai_epi32(y6,  shift);
            y7 = _mm256_srai_epi32(y7,  shift);
            y6 = _mm256_packs_epi32(y6, y7);

            // Store second results
            buff[num]     = mm128(y6);
            buff[num + 4] = _mm256_extracti128_si256(y6, 1);
            num++;

            // 2nd part
            y4 = _mm256_unpackhi_epi16(y0, y0);
            y5 = _mm256_unpackhi_epi16(y1, y1);
            y6 = _mm256_unpackhi_epi16(y2, y2);
            y7 = _mm256_unpackhi_epi16(y3, y3);
            y0 = _mm256_unpacklo_epi16(y0, y0);
            y1 = _mm256_unpacklo_epi16(y1, y1);
            y2 = _mm256_unpacklo_epi16(y2, y2);
            y3 = _mm256_unpacklo_epi16(y3, y3);
            y0 = _mm256_srai_epi32(y0, 16);
            y1 = _mm256_srai_epi32(y1, 16);
            y2 = _mm256_srai_epi32(y2, 16);
            y3 = _mm256_srai_epi32(y3, 16);
            y4 = _mm256_srai_epi32(y4, 16);
            y5 = _mm256_srai_epi32(y5, 16);
            y6 = _mm256_srai_epi32(y6, 16);
            y7 = _mm256_srai_epi32(y7, 16);

            y0 = _mm256_add_epi32(y0,  y3);
            y4 = _mm256_add_epi32(y4,  y7);
            y1 = _mm256_add_epi32(y1,  y2);
            y5 = _mm256_add_epi32(y5,  y6);

            y1 = _mm256_shuffle_epi32(y1, 0x1b);
            y5 = _mm256_shuffle_epi32(y5, 0x1b);

            y0 = _mm256_sub_epi32(y0,  y5);
            y4 = _mm256_sub_epi32(y4,  y1);
            y5 = _mm256_add_epi32(y5,  y5);
            y1 = _mm256_add_epi32(y1,  y1);
            y5 = _mm256_add_epi32(y5,  y0);
            y1 = _mm256_add_epi32(y1,  y4);

            y2 = _mm256_mullo_epi32(y0, kefv_0200);
            y3 = _mm256_mullo_epi32(y4, kefv_0201);
            y6 = _mm256_add_epi32(y2, rounder_1k);
            y6 = _mm256_add_epi32(y6,  y3);
            y2 = _mm256_mullo_epi32(y0, kefv_0202);
            y3 = _mm256_mullo_epi32(y4, kefv_0203);
            y7 = _mm256_add_epi32(y2, rounder_1k);
            y7 = _mm256_add_epi32(y7,  y3);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y4 = _mm256_shuffle_epi32(y4, 0x39);
            y2 = _mm256_mullo_epi32(y0, kefv_0204);
            y3 = _mm256_mullo_epi32(y4, kefv_0205);
            y6 = _mm256_add_epi32(y6,  y2);
            y6 = _mm256_add_epi32(y6,  y3);
            y2 = _mm256_mullo_epi32(y0, kefv_0206);
            y3 = _mm256_mullo_epi32(y4, kefv_0207);
            y7 = _mm256_add_epi32(y7,  y2);
            y7 = _mm256_add_epi32(y7,  y3);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y4 = _mm256_shuffle_epi32(y4, 0x39);
            y2 = _mm256_mullo_epi32(y0, kefv_0208);
            y3 = _mm256_mullo_epi32(y4, kefv_0209);
            y6 = _mm256_add_epi32(y6,  y2);
            y6 = _mm256_add_epi32(y6,  y3);
            y2 = _mm256_mullo_epi32(y0, kefv_0210);
            y3 = _mm256_mullo_epi32(y4, kefv_0211);
            y7 = _mm256_add_epi32(y7,  y2);
            y7 = _mm256_add_epi32(y7,  y3);

            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y4 = _mm256_shuffle_epi32(y4, 0x39);
            y2 = _mm256_mullo_epi32(y0, kefv_0212);
            y3 = _mm256_mullo_epi32(y4, kefv_0213);
            y6 = _mm256_add_epi32(y6,  y2);
            y6 = _mm256_add_epi32(y6,  y3);
            y2 = _mm256_mullo_epi32(y0, kefv_0214);
            y3 = _mm256_mullo_epi32(y4, kefv_0215);
            y7 = _mm256_add_epi32(y7,  y2);
            y7 = _mm256_add_epi32(y7,  y3);

            y6 = _mm256_srai_epi32(y6,  shift);
            y7 = _mm256_srai_epi32(y7,  shift);
            y6 = _mm256_packs_epi32(y6, y7);

            // Store 3th results
            buff[num]     = mm128(y6);
            buff[num + 4] = _mm256_extracti128_si256(y6, 1);
            num++;

            y1 = _mm256_shuffle_epi32(y1, 0x1b);
            y5 = _mm256_sub_epi32(y5,  y1);
            y1 = _mm256_add_epi32(y1,  y1);
            y1 = _mm256_add_epi32(y1,  y5);

            y0 = _mm256_mullo_epi32(y1, kefv_0300);
            y4 = _mm256_mullo_epi32(y5, kefv_0301);
            y6 = _mm256_add_epi32(y0, rounder_1k);
            y7 = _mm256_add_epi32(y4, rounder_1k);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y5 = _mm256_shuffle_epi32(y5, 0x39);
            y0 = _mm256_mullo_epi32(y1, kefv_0302);
            y4 = _mm256_mullo_epi32(y5, kefv_0303);
            y6 = _mm256_add_epi32(y6,  y0);
            y7 = _mm256_add_epi32(y7,  y4);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y5 = _mm256_shuffle_epi32(y5, 0x39);
            y0 = _mm256_mullo_epi32(y1, kefv_0304);
            y4 = _mm256_mullo_epi32(y5, kefv_0305);
            y6 = _mm256_add_epi32(y6,  y0);
            y7 = _mm256_add_epi32(y7,  y4);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y5 = _mm256_shuffle_epi32(y5, 0x39);
            y0 = _mm256_mullo_epi32(y1, kefv_0306);
            y4 = _mm256_mullo_epi32(y5, kefv_0307);
            y6 = _mm256_add_epi32(y6,  y0);
            y7 = _mm256_add_epi32(y7,  y4);

            y6 = _mm256_srai_epi32(y6,  shift);
            y7 = _mm256_srai_epi32(y7,  shift);
            y6 = _mm256_packs_epi32(y6, y7);

            // Store 4th results and rotate
            buff[num]     = mm128(y6);
            buff[num + 4] = _mm256_extracti128_si256(y6, 1);
            num += 5;
#else
            y6 = _mm256_load_const(&rounder_1k);

            y4 = _mm256_load_const(&kefv_0000);
            y7 = y6;
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0001);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0002);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0003);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0004);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0005);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0006);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0007);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0008);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0009);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0010);
            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0011);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0012);
            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0013);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0014);
            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0015);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y6 = _mm256_srai_epi32(y6,  shift);
            y7 = _mm256_srai_epi32(y7,  shift);
            y6 = _mm256_packs_epi32(y6, y7);

            // Store first results
            buff[num]     = mm128(y6);
            buff[num + 4] = _mm256_extracti128_si256(y6, 1);
            num++;

            y7 = _mm256_load_const(&rounder_1k);
            y4 = _mm256_load_const(&kefv_0100);
            y6 = y7;
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0101);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0102);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0103);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0104);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0105);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0106);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0107);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y0);
            y5 = _mm256_madd_epi16(y5, y3);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0108);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_shuffle_epi32(y3, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0109);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0110);
            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0111);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0112);
            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y6 = _mm256_add_epi32(y6,  y4);

            y4 = _mm256_load_const(&kefv_0113);
            y6 = _mm256_sub_epi32(y6,  y5);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y4 = _mm256_load_const(&kefv_0114);
            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y6 = _mm256_add_epi32(y6,  y4);
            y6 = _mm256_sub_epi32(y6,  y5);

            y4 = _mm256_load_const(&kefv_0115);
            y5 = y4;
            y4 = _mm256_madd_epi16(y4, y1);
            y5 = _mm256_madd_epi16(y5, y2);
            y7 = _mm256_add_epi32(y7,  y4);
            y7 = _mm256_sub_epi32(y7,  y5);

            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y2 = _mm256_shuffle_epi32(y2, 0x39);
            y6 = _mm256_srai_epi32(y6,  shift);
            y7 = _mm256_srai_epi32(y7,  shift);
            y6 = _mm256_packs_epi32(y6, y7);

            // Store second results
            buff[num]     = mm128(y6);
            buff[num + 4] = _mm256_extracti128_si256(y6, 1);
            num++;

            // 2nd part
            y4 = _mm256_unpackhi_epi16(y0, y0);
            y5 = _mm256_unpackhi_epi16(y1, y1);
            y6 = _mm256_unpackhi_epi16(y2, y2);
            y7 = _mm256_unpackhi_epi16(y3, y3);
            y0 = _mm256_unpacklo_epi16(y0, y0);
            y1 = _mm256_unpacklo_epi16(y1, y1);
            y2 = _mm256_unpacklo_epi16(y2, y2);
            y3 = _mm256_unpacklo_epi16(y3, y3);
            y0 = _mm256_srai_epi32(y0, 16);
            y1 = _mm256_srai_epi32(y1, 16);
            y2 = _mm256_srai_epi32(y2, 16);
            y3 = _mm256_srai_epi32(y3, 16);
            y4 = _mm256_srai_epi32(y4, 16);
            y5 = _mm256_srai_epi32(y5, 16);
            y6 = _mm256_srai_epi32(y6, 16);
            y7 = _mm256_srai_epi32(y7, 16);

            y0 = _mm256_add_epi32(y0,  y3);
            y4 = _mm256_add_epi32(y4,  y7);
            y1 = _mm256_add_epi32(y1,  y2);
            y5 = _mm256_add_epi32(y5,  y6);

            y1 = _mm256_shuffle_epi32(y1, 0x1b);
            y5 = _mm256_shuffle_epi32(y5, 0x1b);

            y6 = _mm256_load_const(&rounder_1k);
            y0 = _mm256_sub_epi32(y0,  y5);
            y4 = _mm256_sub_epi32(y4,  y1);
            y7 = y6;
            y5 = _mm256_add_epi32(y5,  y5);
            y1 = _mm256_add_epi32(y1,  y1);

            y2 = _mm256_load_const(&kefv_0200);
            y5 = _mm256_add_epi32(y5,  y0);
            y3 = _mm256_load_const(&kefv_0201);
            y1 = _mm256_add_epi32(y1,  y4);

            y2 = _mm256_mullo_epi32(y2, y0);
            y3 = _mm256_mullo_epi32(y3, y4);
            y6 = _mm256_add_epi32(y6,  y2);

            y2 = _mm256_load_const(&kefv_0202);
            y6 = _mm256_add_epi32(y6,  y3);
            y3 = _mm256_load_const(&kefv_0203);

            y2 = _mm256_mullo_epi32(y2, y0);
            y3 = _mm256_mullo_epi32(y3, y4);
            y7 = _mm256_add_epi32(y7,  y2);
            y7 = _mm256_add_epi32(y7,  y3);

            y2 = _mm256_load_const(&kefv_0204);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_load_const(&kefv_0205);
            y4 = _mm256_shuffle_epi32(y4, 0x39);

            y2 = _mm256_mullo_epi32(y2, y0);
            y3 = _mm256_mullo_epi32(y3, y4);
            y6 = _mm256_add_epi32(y6,  y2);
            y2 = _mm256_load_const(&kefv_0206);
            y6 = _mm256_add_epi32(y6,  y3);
            y3 = _mm256_load_const(&kefv_0207);

            y2 = _mm256_mullo_epi32(y2, y0);
            y3 = _mm256_mullo_epi32(y3, y4);
            y7 = _mm256_add_epi32(y7,  y2);
            y7 = _mm256_add_epi32(y7,  y3);

            y2 = _mm256_load_const(&kefv_0208);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_load_const(&kefv_0209);
            y4 = _mm256_shuffle_epi32(y4, 0x39);

            y2 = _mm256_mullo_epi32(y2, y0);
            y3 = _mm256_mullo_epi32(y3, y4);
            y6 = _mm256_add_epi32(y6,  y2);

            y2 = _mm256_load_const(&kefv_0210);
            y6 = _mm256_add_epi32(y6,  y3);
            y3 = _mm256_load_const(&kefv_0211);

            y2 = _mm256_mullo_epi32(y2, y0);
            y3 = _mm256_mullo_epi32(y3, y4);
            y7 = _mm256_add_epi32(y7,  y2);
            y7 = _mm256_add_epi32(y7,  y3);

            y2 = _mm256_load_const(&kefv_0212);
            y0 = _mm256_shuffle_epi32(y0, 0x39);
            y3 = _mm256_load_const(&kefv_0213);
            y4 = _mm256_shuffle_epi32(y4, 0x39);

            y2 = _mm256_mullo_epi32(y2, y0);
            y3 = _mm256_mullo_epi32(y3, y4);
            y6 = _mm256_add_epi32(y6,  y2);

            y2 = _mm256_load_const(&kefv_0214);
            y6 = _mm256_add_epi32(y6,  y3);
            y3 = _mm256_load_const(&kefv_0215);

            y2 = _mm256_mullo_epi32(y2, y0);
            y3 = _mm256_mullo_epi32(y3, y4);
            y7 = _mm256_add_epi32(y7,  y2);
            y7 = _mm256_add_epi32(y7,  y3);

            y6 = _mm256_srai_epi32(y6,  shift);
            y7 = _mm256_srai_epi32(y7,  shift);
            y6 = _mm256_packs_epi32(y6, y7);

            // Store 3th results
            buff[num]     = mm128(y6);
            buff[num + 4] = _mm256_extracti128_si256(y6, 1);
            num++;

            y1 = _mm256_shuffle_epi32(y1, 0x1b);
            y6 = _mm256_load_const(&rounder_1k);
            y5 = _mm256_sub_epi32(y5,  y1);
            y1 = _mm256_add_epi32(y1,  y1);
            y1 = _mm256_add_epi32(y1,  y5);

            y0 = _mm256_load_const(&kefv_0300);
            y7 = y6;
            y4 = _mm256_load_const(&kefv_0301);
            y0 = _mm256_mullo_epi32(y0, y1);
            y4 = _mm256_mullo_epi32(y4, y5);
            y6 = _mm256_add_epi32(y6,  y0);
            y7 = _mm256_add_epi32(y7,  y4);

            y0 = _mm256_load_const(&kefv_0302);
            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y4 = _mm256_load_const(&kefv_0303);
            y5 = _mm256_shuffle_epi32(y5, 0x39);
            y0 = _mm256_mullo_epi32(y0, y1);
            y4 = _mm256_mullo_epi32(y4, y5);
            y6 = _mm256_add_epi32(y6,  y0);
            y7 = _mm256_add_epi32(y7,  y4);

            y0 = _mm256_load_const(&kefv_0304);
            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y4 = _mm256_load_const(&kefv_0305);
            y5 = _mm256_shuffle_epi32(y5, 0x39);
            y0 = _mm256_mullo_epi32(y0, y1);
            y4 = _mm256_mullo_epi32(y4, y5);
            y6 = _mm256_add_epi32(y6,  y0);
            y7 = _mm256_add_epi32(y7,  y4);

            y0 = _mm256_load_const(&kefv_0306);
            y1 = _mm256_shuffle_epi32(y1, 0x39);
            y4 = _mm256_load_const(&kefv_0307);
            y5 = _mm256_shuffle_epi32(y5, 0x39);
            y0 = _mm256_mullo_epi32(y0, y1);
            y4 = _mm256_mullo_epi32(y4, y5);
            y6 = _mm256_add_epi32(y6,  y0);
            y7 = _mm256_add_epi32(y7,  y4);

            y6 = _mm256_srai_epi32(y6,  shift);
            y7 = _mm256_srai_epi32(y7,  shift);
            y6 = _mm256_packs_epi32(y6, y7);

            // Store 4th results and rotate
            buff[num]     = mm128(y6);
            buff[num + 4] = _mm256_extracti128_si256(y6, 1);
            num += 5;
#endif

            tmp += 2*tmp_stride;
        }
        transpose2(dst, buff);
    }

    void H265_FASTCALL MAKE_NAME(h265_DCT32x32Fwd_16s)(const short *H265_RESTRICT src, Ipp32s srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth)
    {
        switch (bitDepth) {
        case  8: h265_DCT32x32Fwd_16s_Kernel<SHIFT_FRW32_1ST_BASE + 0>(src, srcStride, dst);   break;
        case  9: h265_DCT32x32Fwd_16s_Kernel<SHIFT_FRW32_1ST_BASE + 1>(src, srcStride, dst);   break;
        case 10: h265_DCT32x32Fwd_16s_Kernel<SHIFT_FRW32_1ST_BASE + 2>(src, srcStride, dst);   break;
        }
    }

} // namespace

#endif // #if defined (MFX_TARGET_OPTIMIZATION_SSE4)
#endif // #if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
/* EOF */