/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/
// Inverse HEVC Transform functions optimised by intrinsics
// Sizes: 4, 8, 16
#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 
#include <immintrin.h> // AVX, AVX2

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

// HEVC_PP is build by ICL only with special options /QaxSSE4.2,CORE_AVX2
// so, MS options should be ignored
#if defined (_MSC_VER)
#pragma warning (disable: 4752) // warning C4752: found Intel(R) Advanced Vector Extensions; consider using /arch:AVX
#endif

// windows workarround. MS compiler issue due to MACROS WRAPPER
#if defined (_MSC_VER) || defined (__INTEL_COMPILER)
#pragma warning (disable : 4310 ) /* disable cast truncates constant value */
#pragma warning (disable : 4701 ) //warning C4701: potentially uninitialized local variable 'destByte' used
#pragma warning (disable : 4703)
#endif

namespace MFX_HEVC_PP
{
#define SHIFT_INV_1ST          7 // Shift after first inverse transform stage
#define SHIFT_INV_2ND_BASE     12   // Base shift after second inverse transform stage, offset later by (bitDepth - 8) for > 8-bit data
#define REG_DCT                  65535

#ifdef NDEBUG
  #define MM_LOAD_EPI128(x) (*(__m128i*)x)
#else
  #define MM_LOAD_EPI128(x) _mm_load_si128( (__m128i*)x )
#endif

// load dst[0-15], expand to 16 bits, add current results, clip/pack to 8 bytes, save back
#define SAVE_RESULT_16_BYTES(y0, yy, pred, dst) \
      y0 = _mm256_cvtepu8_epi16(MM_LOAD_EPI128(&pred)); \
      yy = _mm256_add_epi16(yy, y0); \
      yy = _mm256_packus_epi16(yy, yy); \
      yy = _mm256_permute4x64_epi64(yy, 8); \
      _mm_storeu_si128((__m128i *)&dst, _mm256_castsi256_si128(yy));


typedef unsigned char       uint8_t;
typedef signed short        int16_t;
typedef signed long         int32_t;
typedef unsigned long       uint32_t;

#define ALIGNED_SSE2 ALIGN_DECL(16)
//#define FASTCALL     __fastcall


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


#define M128I_WC_INIT(v) {\
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF)}

#define M128I_W2x4C_INIT(w0,w1) {\
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF)}

#define M128I_W8C_INIT(w0,w1,w2,w3,w4,w5,w6,w7) {\
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF), \
    (char)((w4)&0xFF),(char)(((w4)>>8)&0xFF), (char)((w5)&0xFF),(char)(((w5)>>8)&0xFF), \
    (char)((w6)&0xFF),(char)(((w6)>>8)&0xFF), (char)((w7)&0xFF),(char)(((w7)>>8)&0xFF)}

#define M128I_DC_INIT(v) {\
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF),(char)(((v)>>24)&0xFF)}

#define M128I_D4C_INIT(w0,w1,w2,w3) {\
    (char)((w0)&0xFF), (char)(((w0)>>8)&0xFF), (char)(((w0)>>16)&0xFF), (char)(((w0)>>24)&0xFF), \
    (char)((w1)&0xFF), (char)(((w1)>>8)&0xFF), (char)(((w1)>>16)&0xFF), (char)(((w1)>>24)&0xFF), \
    (char)((w2)&0xFF), (char)(((w2)>>8)&0xFF), (char)(((w2)>>16)&0xFF), (char)(((w2)>>24)&0xFF), \
    (char)((w3)&0xFF), (char)(((w3)>>8)&0xFF), (char)(((w3)>>16)&0xFF), (char)(((w3)>>24)&0xFF)}


#define M256I_WC_INIT(v) {\
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)((v)&0xFF), (char)(((v)>>8)&0xFF)}


#define M256I_W2x8C_INIT(w0,w1) {\
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF)}


#define M256I_2xW2x4C_INIT(wl0, wl1,  wh0, wh1) {\
    (char)((wl0)&0xFF),(char)(((wl0)>>8)&0xFF), (char)((wl1)&0xFF),(char)(((wl1)>>8)&0xFF), \
    (char)((wl0)&0xFF),(char)(((wl0)>>8)&0xFF), (char)((wl1)&0xFF),(char)(((wl1)>>8)&0xFF), \
    (char)((wl0)&0xFF),(char)(((wl0)>>8)&0xFF), (char)((wl1)&0xFF),(char)(((wl1)>>8)&0xFF), \
    (char)((wl0)&0xFF),(char)(((wl0)>>8)&0xFF), (char)((wl1)&0xFF),(char)(((wl1)>>8)&0xFF), \
    (char)((wh0)&0xFF),(char)(((wh0)>>8)&0xFF), (char)((wh1)&0xFF),(char)(((wh1)>>8)&0xFF), \
    (char)((wh0)&0xFF),(char)(((wh0)>>8)&0xFF), (char)((wh1)&0xFF),(char)(((wh1)>>8)&0xFF), \
    (char)((wh0)&0xFF),(char)(((wh0)>>8)&0xFF), (char)((wh1)&0xFF),(char)(((wh1)>>8)&0xFF), \
    (char)((wh0)&0xFF),(char)(((wh0)>>8)&0xFF), (char)((wh1)&0xFF),(char)(((wh1)>>8)&0xFF)}



#define M256I_W8x2C_INIT(w0,w1,w2,w3,w4,w5,w6,w7) {\
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF), \
    (char)((w4)&0xFF),(char)(((w4)>>8)&0xFF), (char)((w5)&0xFF),(char)(((w5)>>8)&0xFF), \
    (char)((w6)&0xFF),(char)(((w6)>>8)&0xFF), (char)((w7)&0xFF),(char)(((w7)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF), \
    (char)((w4)&0xFF),(char)(((w4)>>8)&0xFF), (char)((w5)&0xFF),(char)(((w5)>>8)&0xFF), \
    (char)((w6)&0xFF),(char)(((w6)>>8)&0xFF), (char)((w7)&0xFF),(char)(((w7)>>8)&0xFF)}

#define M256I_DC_INIT(v) {\
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF), \
    (char)((v)&0xFF), (char)(((v)>>8)&0xFF), (char)(((v)>>16)&0xFF), (char)(((v)>>24)&0xFF)}

#define M256I_D4x2C_INIT(w0,w1,w2,w3) {\
    (char)((w0)&0xFF), (char)(((w0)>>8)&0xFF), (char)(((w0)>>16)&0xFF), (char)(((w0)>>24)&0xFF), \
    (char)((w1)&0xFF), (char)(((w1)>>8)&0xFF), (char)(((w1)>>16)&0xFF), (char)(((w1)>>24)&0xFF), \
    (char)((w2)&0xFF), (char)(((w2)>>8)&0xFF), (char)(((w2)>>16)&0xFF), (char)(((w2)>>24)&0xFF), \
    (char)((w3)&0xFF), (char)(((w3)>>8)&0xFF), (char)(((w3)>>16)&0xFF), (char)(((w3)>>24)&0xFF), \
    (char)((w0)&0xFF), (char)(((w0)>>8)&0xFF), (char)(((w0)>>16)&0xFF), (char)(((w0)>>24)&0xFF), \
    (char)((w1)&0xFF), (char)(((w1)>>8)&0xFF), (char)(((w1)>>16)&0xFF), (char)(((w1)>>24)&0xFF), \
    (char)((w2)&0xFF), (char)(((w2)>>8)&0xFF), (char)(((w2)>>16)&0xFF), (char)(((w2)>>24)&0xFF), \
    (char)((w3)&0xFF), (char)(((w3)>>8)&0xFF), (char)(((w3)>>16)&0xFF), (char)(((w3)>>24)&0xFF)}


#define M128I_WC(x, v)                           M128I_VAR(x, M128I_WC_INIT(v))
#define M128I_W2x4C(x, w0,w1)                    M128I_VAR(x, M128I_W2x4C_INIT(w0,w1))
#define M128I_W8C(x, w0,w1,w2,w3,w4,w5,w6,w7)    M128I_VAR(x, M128I_W8C_INIT(w0,w1,w2,w3))
#define M128I_DC(x, v)                           M128I_VAR(x, M128I_DC_INIT(w0,w1,w2,w3))
#define M128I_D4C(x, w0,w1,w2,w3)                M128I_VAR(x, M128I_D4C_INIT(w0,w1,w2,w3))

#define M256I_WC(x, v)                           M256I_VAR(x, M256I_WC_INIT(v))
#define M256I_W2x8C(x, w0,w1)                    M256I_VAR(x, M256I_W2x8C_INIT(w0,w1))
#define M256I_2xW2x4C(x, wl0, wl1,  wh0, wh1)    M256I_VAR(x, M256I_2xW2x4C_INIT(wl0, wl1,  wh0, wh1))
#define M256I_W8x2C(x, w0,w1,w2,w3,w4,w5,w6,w7)  M256I_VAR(x, M256I_W8x2C_INIT(w0,w1,w2,w3,w4,w5,w6,w7))
#define M256I_DC(x, v)                           M256I_VAR(x, M256I_DC_INIT(v))
#define M256I_D4x2C(x, w0,w1,w2,w3)              M256I_VAR(x, M256I_D4x2C_INIT(w0,w1,w2,w3))


#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))
#define _mm_storeh_epi64(p, A) _mm_storeh_pd((double *)(p), _mm_castsi128_pd(A))
#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (double *)(p)))


#define mm256_storel_epi128(p, A)  _mm_store_si128 ( (__m128i *)(p), _mm256_castsi256_si128(A))
#define mm256_storelu_epi128(p, A) _mm_storeu_si128( (__m128i *)(p), _mm256_castsi256_si128(A))

#define mm256_storeh_epi128(p, A)  _mm_store_si128 ( (__m128i *)(p), _mm256_extracti128_si256(A, 1))
#define mm256_storehu_epi128(p, A) _mm_storeu_si128( (__m128i *)(p), _mm256_extracti128_si256(A, 1))



#ifdef _INCLUDED_PMM
#define load_unaligned(x) _mm_lddqu_si128(x)
#else
#define load_unaligned(x) _mm_loadu_si128(x)
#endif

#define M256I_W8x2SHORT_INIT(w0, w1) {\
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF)}

#define M256I_2x8SHORT_INIT(w0,w1,w2,w3,w4,w5,w6,w7) {\
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF), \
    (char)((w4)&0xFF),(char)(((w4)>>8)&0xFF), (char)((w5)&0xFF),(char)(((w5)>>8)&0xFF), \
    (char)((w6)&0xFF),(char)(((w6)>>8)&0xFF), (char)((w7)&0xFF),(char)(((w7)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF), \
    (char)((w4)&0xFF),(char)(((w4)>>8)&0xFF), (char)((w5)&0xFF),(char)(((w5)>>8)&0xFF), \
    (char)((w6)&0xFF),(char)(((w6)>>8)&0xFF), (char)((w7)&0xFF),(char)(((w7)>>8)&0xFF)}


#define M256I_W8x2SHORT(x, w0, w1)                 M256I_VAR(x, M256I_W8x2SHORT_INIT(w0, w1))
#define M256I_2x8SHORT(x, w0,w1,w2,w3,w4,w5,w6,w7) M256I_VAR(x, M256I_2x8SHORT_INIT(w0,w1,w2,w3,w4,w5,w6,w7))

// Constants for 16x16
M256I_W8x2SHORT( t_16_O0_13,  90,  87); // g_T16[ 1][0], g_T16[ 3][0]
M256I_W8x2SHORT( t_16_O0_57,  80,  70); // g_T16[ 5][0], g_T16[ 7][0]
M256I_W8x2SHORT( t_16_O0_9B,  57,  43); // g_T16[ 9][0], g_T16[11][0]
M256I_W8x2SHORT( t_16_O0_DF,  25,   9); // g_T16[13][0], g_T16[15][0]
M256I_W8x2SHORT( t_16_O1_13,  87,  57); // g_T16[ 1][1], g_T16[ 3][1]
M256I_W8x2SHORT( t_16_O1_57,   9, -43); // g_T16[ 5][1], g_T16[ 7][1]
M256I_W8x2SHORT( t_16_O1_9B, -80, -90); // g_T16[ 9][1], g_T16[11][1]
M256I_W8x2SHORT( t_16_O1_DF, -70, -25); // g_T16[13][1], g_T16[15][1]
M256I_W8x2SHORT( t_16_O2_13,  80,   9); // g_T16[ 1][2], g_T16[ 3][2]
M256I_W8x2SHORT( t_16_O2_57, -70, -87); // g_T16[ 5][2], g_T16[ 7][2]
M256I_W8x2SHORT( t_16_O2_9B, -25,  57); // g_T16[ 9][2], g_T16[11][2]
M256I_W8x2SHORT( t_16_O2_DF,  90,  43); // g_T16[13][2], g_T16[15][2]
M256I_W8x2SHORT( t_16_O3_13,  70, -43); // g_T16[ 1][3], g_T16[ 3][3]
M256I_W8x2SHORT( t_16_O3_57, -87,   9); // g_T16[ 5][3], g_T16[ 7][3]
M256I_W8x2SHORT( t_16_O3_9B,  90,  25); // g_T16[ 9][3], g_T16[11][3]
M256I_W8x2SHORT( t_16_O3_DF, -80, -57); // g_T16[13][3], g_T16[15][3]
M256I_W8x2SHORT( t_16_O4_13,  57, -80); // g_T16[ 1][4], g_T16[ 3][4]
M256I_W8x2SHORT( t_16_O4_57, -25,  90); // g_T16[ 5][4], g_T16[ 7][4]
M256I_W8x2SHORT( t_16_O4_9B,  -9, -87); // g_T16[ 9][4], g_T16[11][4]
M256I_W8x2SHORT( t_16_O4_DF,  43,  70); // g_T16[13][4], g_T16[15][4]
M256I_W8x2SHORT( t_16_O5_13,  43, -90); // g_T16[ 1][5], g_T16[ 3][5]
M256I_W8x2SHORT( t_16_O5_57,  57,  25); // g_T16[ 5][5], g_T16[ 7][5]
M256I_W8x2SHORT( t_16_O5_9B, -87,  70); // g_T16[ 9][5], g_T16[11][5]
M256I_W8x2SHORT( t_16_O5_DF,   9, -80); // g_T16[13][5], g_T16[15][5]
M256I_W8x2SHORT( t_16_O6_13,  25, -70); // g_T16[ 1][6], g_T16[ 3][6]
M256I_W8x2SHORT( t_16_O6_57,  90, -80); // g_T16[ 5][6], g_T16[ 7][6]
M256I_W8x2SHORT( t_16_O6_9B,  43,   9); // g_T16[ 9][6], g_T16[11][6]
M256I_W8x2SHORT( t_16_O6_DF, -57,  87); // g_T16[13][6], g_T16[15][6]
M256I_W8x2SHORT( t_16_O7_13,   9, -25); // g_T16[ 1][7], g_T16[ 3][7]
M256I_W8x2SHORT( t_16_O7_57,  43, -57); // g_T16[ 5][7], g_T16[ 7][7]
M256I_W8x2SHORT( t_16_O7_9B,  70, -80); // g_T16[ 9][7], g_T16[11][7]
M256I_W8x2SHORT( t_16_O7_DF,  87, -90); // g_T16[13][7], g_T16[15][7]

M256I_W8x2SHORT( t_16_E0_26,  89,  75); // g_T16[ 2][0], g_T16[ 6][0]
M256I_W8x2SHORT( t_16_E0_AE,  50,  18); // g_T16[10][0], g_T16[14][0]
M256I_W8x2SHORT( t_16_E1_26,  75, -18); // g_T16[ 2][1], g_T16[ 6][1]
M256I_W8x2SHORT( t_16_E1_AE, -89, -50); // g_T16[10][1], g_T16[14][1]
M256I_W8x2SHORT( t_16_E2_26,  50, -89); // g_T16[ 2][2], g_T16[ 6][2]
M256I_W8x2SHORT( t_16_E2_AE,  18,  75); // g_T16[10][2], g_T16[14][2]
M256I_W8x2SHORT( t_16_E3_26,  18, -50); // g_T16[ 2][3], g_T16[ 6][3]
M256I_W8x2SHORT( t_16_E3_AE,  75, -89); // g_T16[10][3], g_T16[14][3]
M256I_W8x2SHORT( t_16_EEE0_08,  64,  64); // g_T16[0][0], g_T16[ 8][0]
M256I_W8x2SHORT( t_16_EEE1_08,  64, -64); // g_T16[0][1], g_T16[ 8][1]
M256I_W8x2SHORT( t_16_EEO0_4C,  83,  36); // g_T16[4][0], g_T16[12][0]
M256I_W8x2SHORT( t_16_EEO1_4C,  36, -83); // g_T16[4][1], g_T16[12][1]
M256I_DC( round1y, 1<<(SHIFT_INV_1ST-1));

#if defined(_WIN32) || defined(_WIN64)
static const  __m256i oddeven = {0,1, 4,5, 8,9, 12,13, 2,3, 6,7, 10,11, 14,15,  0,1, 4,5, 8,9, 12,13, 2,3, 6,7, 10,11, 14,15};
static const  __m256i shuffev = {0,1, 8,9, 2,3, 10,11, 4,5, 12,13, 6,7, 14,15,  0,1, 8,9, 2,3, 10,11, 4,5, 12,13, 6,7, 14,15};

#else
static ALIGN_DECL(32) const char array_oddeven[32] = {0,1, 4,5, 8,9, 12,13, 2,3, 6,7, 10,11, 14,15,  0,1, 4,5, 8,9, 12,13, 2,3, 6,7, 10,11, 14,15};
static const __m256i* p_oddeven = (const __m256i*) array_oddeven;
static const __m256i oddeven = * p_oddeven;

static ALIGN_DECL(32) const char array_shuffev[32] = {0,1, 8,9, 2,3, 10,11, 4,5, 12,13, 6,7, 14,15,  0,1, 8,9, 2,3, 10,11, 4,5, 12,13, 6,7, 14,15};
static const __m256i* p_shuffev = (const __m256i*) array_shuffev;
static const __m256i shuffev = * p_shuffev;
#endif


M256I_2x8SHORT( t_16_a0101_08,  64, 64,  64,-64,  64,-64,  64, 64);
M256I_2x8SHORT( t_16_b0110_4C,  83, 36,  36,-83, -36, 83, -83,-36);
M256I_2x8SHORT( t_16_d0123_2A,  89, 50,  75,-89,  50, 18,  18, 75);
M256I_2x8SHORT( t_16_d0123_6E,  75, 18, -18,-50, -89, 75, -50,-89);
M256I_2x8SHORT( t_16_f0123_01,  90, 87,  87, 57,  80,  9,  70,-43);
M256I_2x8SHORT( t_16_f4567_01,  57,-80,  43,-90,  25,-70,   9,-25);
M256I_2x8SHORT( t_16_f0123_23,  80, 70,   9,-43, -70,-87, -87,  9);
M256I_2x8SHORT( t_16_f4567_23, -25, 90,  57, 25,  90,-80,  43,-57);
M256I_2x8SHORT( t_16_f0123_45,  57, 43, -80,-90, -25, 57,  90, 25);
M256I_2x8SHORT( t_16_f4567_45,  -9,-87, -87, 70,  43,  9,  70,-80);
M256I_2x8SHORT( t_16_f0123_67,  25,  9, -70,-25,  90, 43, -80,-57);
M256I_2x8SHORT( t_16_f4567_67,  43, 70,   9,-80, -57, 87,  87,-90);

#undef coef_stride
#define coef_stride 4

// CREATE NAME FOR <HEVCPP_API> FUNCTIONS
#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    #define MAKE_NAME(func) func ## _avx2
#else
    #define MAKE_NAME(func) func
#endif

#if ((__GNUC__== 4) && (__GNUC_MINOR__== 7))
#pragma GCC push_options
#pragma GCC optimize ("O0")
#endif

template <int bitDepth, typename PredPixType, typename DstCoeffsType, bool inplace>
static void h265_DCT4x4Inv_16sT_Kernel(const PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
{
   const int SHIFT_INV_2ND = (SHIFT_INV_2ND_BASE - (bitDepth - 8));

   M256I_2xW2x4C(t_4dct_02_01,  64, 83,   64,-36);
   M256I_2xW2x4C(t_4dct_02_23,  64, 36,  -64, 83);
   M256I_2xW2x4C(t_4dct_13_01,  64, 36,   64,-83);
   M256I_2xW2x4C(t_4dct_13_23, -64,-83,   64,-36);

   M256I_DC (round1_avx, 1<<(SHIFT_INV_1ST-1));

   M256I_W8x2C(t_dct4_A_avx,  64, 83,   64, 36,  64,-36,  64,-83);
   M256I_W8x2C(t_dct4_B_avx,  64, 36,  -64,-83, -64, 83,  64,-36);

   M256I_DC (round2_avx, 1<<(SHIFT_INV_2ND-1));

   __m128i s0 = _mm_loadl_epi64((const __m128i *)(coeff + 0*coef_stride));
   __m128i s1 = _mm_loadl_epi64((const __m128i *)(coeff + 1*coef_stride));
   __m128i s2 = _mm_loadl_epi64((const __m128i *)(coeff + 2*coef_stride));
   __m128i s3 = _mm_loadl_epi64((const __m128i *)(coeff + 3*coef_stride));

   _mm256_zeroupper();

// GCC 4.7 bug workaround
#if defined(__GNUC__) && ( __GNUC__ < 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ < 6 || (__GNUC_MINOR__ == 6 && __GNUC_PATCHLEVEL__ > 0))))
   __m128i tmp_s0s1 = _mm_unpacklo_epi16 (s0, s1);   
   __m256i s01 = _mm_broadcastsi128_si256( (__m128i const *)&tmp_s0s1 ); // s13 s03 s12 s02 s11 s01 s10 s00
    __m128i tmp_s2s3 = _mm_unpacklo_epi16 (s2, s3);
   __m256i s23 = _mm_broadcastsi128_si256( (__m128i const *)&tmp_s2s3 ); // s23 s23 s22 s22 s31 s21 s30 s20
#elif (__GNUC__ == 4 && (__GNUC_MINOR__ == 7 && __GNUC_PATCHLEVEL__ > 0))
   __m256i s01 = _mm_broadcastsi128_si256( _mm_unpacklo_epi16 (s0, s1) ); // s13 s03 s12 s02 s11 s01 s10 s00
   __m256i s23 = _mm_broadcastsi128_si256( _mm_unpacklo_epi16 (s2, s3) ); // s23 s23 s22 s22 s31 s21 s30 s20
#else
   __m256i s01 = _mm256_broadcastsi128_si256(_mm_unpacklo_epi16 (s0, s1) ); // s13 s03 s12 s02 s11 s01 s10 s00
   __m256i s23 = _mm256_broadcastsi128_si256(_mm_unpacklo_epi16 (s2, s3) ); // s23 s23 s22 s22 s31 s21 s30 s20
#endif


// #if ((__GNUC__== 4) && (__GNUC_MINOR__== 7)) || defined(OSX)
//    __m256i s01 = _mm_broadcastsi128_si256( _mm_unpacklo_epi16 (s0, s1) ); // s13 s03 s12 s02 s11 s01 s10 s00
//    __m256i s23 = _mm_broadcastsi128_si256( _mm_unpacklo_epi16 (s2, s3)); // s23 s23 s22 s22 s31 s21 s30 s20
// #else
//    __m256i s01 = _mm256_broadcastsi128_si256( _mm_unpacklo_epi16 (s0, s1)); // s13 s03 s12 s02 s11 s01 s10 s00
//    __m256i s23 = _mm256_broadcastsi128_si256( _mm_unpacklo_epi16 (s2, s3)); // s23 s23 s22 s22 s31 s21 s30 s20
// #endif

   __m256i R02 = _mm256_add_epi32( _mm256_madd_epi16(s01, t_4dct_02_01), _mm256_madd_epi16(s23, t_4dct_02_23));
   __m256i R13 = _mm256_add_epi32( _mm256_madd_epi16(s01, t_4dct_13_01), _mm256_madd_epi16(s23, t_4dct_13_23));

   R02 = _mm256_srai_epi32( _mm256_add_epi32( R02, round1_avx), SHIFT_INV_1ST);
   R13 = _mm256_srai_epi32( _mm256_add_epi32( R13, round1_avx), SHIFT_INV_1ST);

   __m256i H0123 = _mm256_packs_epi32(R02, R13);

   // H0123 : s33 s32 s31 s30  s23 s22 s21 s20  s13 s12 s11 s10  s03 s02 s01 s00

   __m256i tmp04 = _mm256_shuffle_epi32(H0123, _MM_SHUFFLE(0,0,0,0));
   __m256i tmp15 = _mm256_shuffle_epi32(H0123, _MM_SHUFFLE(1,1,1,1));
   __m256i tmp26 = _mm256_shuffle_epi32(H0123, _MM_SHUFFLE(2,2,2,2));
   __m256i tmp37 = _mm256_shuffle_epi32(H0123, _MM_SHUFFLE(3,3,3,3));

   __m256i xmm02 = _mm256_add_epi32(_mm256_madd_epi16(tmp04, t_dct4_A_avx), _mm256_madd_epi16(tmp15, t_dct4_B_avx));
   __m256i xmm13 = _mm256_add_epi32(_mm256_madd_epi16(tmp26, t_dct4_A_avx), _mm256_madd_epi16(tmp37, t_dct4_B_avx));

   xmm02 = _mm256_srai_epi32(_mm256_add_epi32(xmm02, round2_avx), SHIFT_INV_2ND); 
   xmm13 = _mm256_srai_epi32(_mm256_add_epi32(xmm13, round2_avx), SHIFT_INV_2ND); 

   __m256i xmm0123 = _mm256_packs_epi32(xmm02, xmm13);

    if (inplace && sizeof(DstCoeffsType) == 1) {
      __m128i tmp0;
      /* load 4 bytes from row 0 and row 1, expand to 16 bits, add temp values, clip/pack to 8 bytes */
      //tmp0 = _mm_insert_epi32(tmp0, *(int *)(dst + 0*destStride), 0);    /* load row 0 (bytes) */      
// Workaround for GCC 4.7 bug
#if __GNUC__
      tmp0 = _mm_insert_epi32( _mm_setzero_si128(), *(int *)(pred + 0*predStride), 0);      /* load row 0 (bytes) */ 
#else
      tmp0 = _mm_insert_epi32( _mm_undefined_si128(), *(int *)(pred + 0*predStride), 0);      /* load row 0 (bytes) */ 
#endif

      tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 1*predStride), 1);    /* load row 1 (bytes) */
      tmp0 = _mm_cvtepu8_epi16(tmp0);                                        /* expand to 16 bytes */
      tmp0 = _mm_add_epi16(tmp0, _mm256_castsi256_si128(xmm0123));           /* add to dst */
      tmp0 = _mm_packus_epi16(tmp0, tmp0);                                   /* pack to 8 bytes */
      *(int *)(dst + 0*destStride) = _mm_extract_epi32(tmp0, 0);         /* store row 0 (bytes) */
      *(int *)(dst + 1*destStride) = _mm_extract_epi32(tmp0, 1);         /* store row 1 (bytes) */

      /* repeat for rows 2 and 3 */
      tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 2*predStride), 0);    /* load row 0 (bytes) */      

      tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 3*predStride), 1);    /* load row 1 (bytes) */
      tmp0 = _mm_cvtepu8_epi16(tmp0);                                        /* expand to 16 bytes */
      tmp0 = _mm_add_epi16(tmp0, _mm256_extracti128_si256(xmm0123, 1));      /* add to dst */
      tmp0 = _mm_packus_epi16(tmp0, tmp0);                                   /* pack to 8 bytes */
      *(int *)(dst + 2*destStride) = _mm_extract_epi32(tmp0, 0);         /* store row 0 (bytes) */
      *(int *)(dst + 3*destStride) = _mm_extract_epi32(tmp0, 1);         /* store row 1 (bytes) */
    } else if (inplace && sizeof(DstCoeffsType) == 2) {
       __m256i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;

       /* load 4 words from each row, add temp values, clip to [0, 2^bitDepth) */
       tmp0 = mm256(_mm_loadl_epi64((__m128i *)(pred + 0*predStride)));
       tmp1 = mm256(_mm_loadl_epi64((__m128i *)(pred + 1*predStride)));
       tmp2 = mm256(_mm_loadl_epi64((__m128i *)(pred + 2*predStride)));
       tmp3 = mm256(_mm_loadl_epi64((__m128i *)(pred + 3*predStride)));

       tmp0 = _mm256_unpacklo_epi64(tmp0, tmp1);
       tmp2 = _mm256_unpacklo_epi64(tmp2, tmp3);
       tmp0 = _mm256_permute2x128_si256(tmp0, tmp2, 0x20);

       /* signed saturating add to 16-bit, then clip to bitDepth */
       tmp0 = _mm256_adds_epi16(tmp0, xmm0123);
       tmp4 = _mm256_setzero_si256();
       tmp5 = _mm256_set1_epi16((1 << bitDepth) - 1);
       tmp0 = _mm256_max_epi16(tmp0, tmp4);
       tmp0 = _mm256_min_epi16(tmp0, tmp5);

      _mm_storel_epi64((__m128i*)(dst + 0*destStride), _mm256_castsi256_si128(tmp0));
      _mm_storeh_epi64((__m128i*)(dst + 1*destStride), _mm256_castsi256_si128(tmp0));
      _mm_storel_epi64((__m128i*)(dst + 2*destStride), _mm256_extracti128_si256(tmp0, 1));
      _mm_storeh_epi64((__m128i*)(dst + 3*destStride), _mm256_extracti128_si256(tmp0, 1));
    } else {
      _mm_storel_epi64((__m128i*)(dst + 0*destStride), _mm256_castsi256_si128(xmm0123));
      _mm_storeh_epi64((__m128i*)(dst + 1*destStride), _mm256_castsi256_si128(xmm0123));
      _mm_storel_epi64((__m128i*)(dst + 2*destStride), _mm256_extracti128_si256(xmm0123, 1));
      _mm_storeh_epi64((__m128i*)(dst + 3*destStride), _mm256_extracti128_si256(xmm0123, 1));
    }
}

#if ((__GNUC__== 4) && (__GNUC_MINOR__== 7))
#pragma GCC pop_options
#endif

void MAKE_NAME(h265_DCT4x4Inv_16sT)(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth)
{
    if (inplace) {
        switch (bitDepth) {
        case  8:
            if (inplace == 2)
                h265_DCT4x4Inv_16sT_Kernel< 8, Ipp8u, Ipp16u, true >((Ipp8u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride);
            else
                h265_DCT4x4Inv_16sT_Kernel< 8, Ipp8u, Ipp8u,  true >((Ipp8u *)pred, predStride, (Ipp8u *)destPtr, coeff, destStride);
            break;
        case  9: h265_DCT4x4Inv_16sT_Kernel< 9, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 10: h265_DCT4x4Inv_16sT_Kernel<10, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 11: h265_DCT4x4Inv_16sT_Kernel<11, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 12: h265_DCT4x4Inv_16sT_Kernel<12, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        }
    } else {
        switch (bitDepth) {
        case  8: h265_DCT4x4Inv_16sT_Kernel< 8, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case  9: h265_DCT4x4Inv_16sT_Kernel< 9, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 10: h265_DCT4x4Inv_16sT_Kernel<10, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 11: h265_DCT4x4Inv_16sT_Kernel<11, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 12: h265_DCT4x4Inv_16sT_Kernel<12, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        }
    }
}

template <int bitDepth, typename PredPixType, typename DstCoeffsType, bool inplace>
static void h265_DST4x4Inv_16sT_Kernel(const PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
{
//    dst[i + 0*4] = (short)Clip3( -32768, 32767, ( 29*s0 + 74*s1 + 84*s2 + 55*s3 + rnd_factor ) >> SHIFT_INV_1ST );
//    dst[i + 1*4] = (short)Clip3( -32768, 32767, ( 55*s0 + 74*s1 - 29*s2 - 84*s3 + rnd_factor ) >> SHIFT_INV_1ST );
//    dst[i + 2*4] = (short)Clip3( -32768, 32767, ( 74*s0 +  0*s1 - 74*s2 + 74*s3 + rnd_factor ) >> SHIFT_INV_1ST );
//    dst[i + 3*4] = (short)Clip3( -32768, 32767, ( 84*s0 - 74*s1 + 55*s2 - 29*s3 + rnd_factor ) >> SHIFT_INV_1ST );

   const int SHIFT_INV_2ND = (SHIFT_INV_2ND_BASE - (bitDepth - 8));

   M256I_2xW2x4C(t_4dst_02_01,  29, 74,   74,  0);
   M256I_2xW2x4C(t_4dst_02_23,  84, 55,  -74, 74);
   M256I_2xW2x4C(t_4dst_13_01,  55, 74,   84,-74);
   M256I_2xW2x4C(t_4dst_13_23, -29,-84,   55,-29);

   M256I_DC (round1_avx, 1<<(SHIFT_INV_1ST-1));

   M256I_W8x2C(t_dst4_A_avx,  29, 74,   55, 74,  74,  0,  84,-74);
   M256I_W8x2C(t_dst4_B_avx,  84, 55,  -29,-84, -74, 74,  55,-29);
   M256I_DC (round2_avx, 1<<(SHIFT_INV_2ND-1));

   __m128i s0 = _mm_loadl_epi64((const __m128i *)(coeff + 0*coef_stride));
   __m128i s1 = _mm_loadl_epi64((const __m128i *)(coeff + 1*coef_stride));
   __m128i s2 = _mm_loadl_epi64((const __m128i *)(coeff + 2*coef_stride));
   __m128i s3 = _mm_loadl_epi64((const __m128i *)(coeff + 3*coef_stride));

   _mm256_zeroupper();

// GCC 4.7 bug workaround
#if  defined(__GNUC__) && ( __GNUC__ < 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ < 6 || (__GNUC_MINOR__ == 6 && __GNUC_PATCHLEVEL__ > 0))))
   __m128i tmp_s0s1 = _mm_unpacklo_epi16 (s0, s1);   
   __m256i s01 = _mm_broadcastsi128_si256( (__m128i const *)&tmp_s0s1 ); // s13 s03 s12 s02 s11 s01 s10 s00
   __m128i tmp_s2s3 = _mm_unpacklo_epi16 (s2, s3);
   __m256i s23 = _mm_broadcastsi128_si256( (__m128i const *)&tmp_s2s3 ); // s23 s23 s22 s22 s31 s21 s30 s20
#elif(__GNUC__ == 4 && (__GNUC_MINOR__ == 7 && __GNUC_PATCHLEVEL__ > 0))
   __m256i s01 = _mm_broadcastsi128_si256( _mm_unpacklo_epi16 (s0, s1) ); // s13 s03 s12 s02 s11 s01 s10 s00
   __m256i s23 = _mm_broadcastsi128_si256( _mm_unpacklo_epi16 (s2, s3) ); // s23 s23 s22 s22 s31 s21 s30 s20
#else
   __m256i s01 = _mm256_broadcastsi128_si256( _mm_unpacklo_epi16 (s0, s1) ); // s13 s03 s12 s02 s11 s01 s10 s00
   __m256i s23 = _mm256_broadcastsi128_si256( _mm_unpacklo_epi16 (s2, s3) ); // s23 s23 s22 s22 s31 s21 s30 s20

#endif

// GCC 4.7 bug workaround
// #if ((__GNUC__== 4) && (__GNUC_MINOR__== 7))
//    __m256i s01 = _mm_broadcastsi128_si256( _mm_unpacklo_epi16 (s0, s1)); // s13 s03 s12 s02 s11 s01 s10 s00
//    __m256i s23 = _mm_broadcastsi128_si256( _mm_unpacklo_epi16 (s2, s3)); // s23 s23 s22 s22 s31 s21 s30 s20
// 
// #else
//    __m256i s01 = _mm256_broadcastsi128_si256( _mm_unpacklo_epi16 (s0, s1)); // s13 s03 s12 s02 s11 s01 s10 s00
//    __m256i s23 = _mm256_broadcastsi128_si256( _mm_unpacklo_epi16 (s2, s3)); // s23 s23 s22 s22 s31 s21 s30 s20
// #endif

   __m256i R02 = _mm256_add_epi32( _mm256_madd_epi16(s01, t_4dst_02_01), _mm256_madd_epi16(s23, t_4dst_02_23));
   __m256i R13 = _mm256_add_epi32( _mm256_madd_epi16(s01, t_4dst_13_01), _mm256_madd_epi16(s23, t_4dst_13_23));

   R02 = _mm256_srai_epi32( _mm256_add_epi32( R02, round1_avx), SHIFT_INV_1ST);
   R13 = _mm256_srai_epi32( _mm256_add_epi32( R13, round1_avx), SHIFT_INV_1ST);

   __m256i H0123 = _mm256_packs_epi32(R02, R13);

   // H0123 : s33 s32 s31 s30  s23 s22 s21 s20  s13 s12 s11 s10  s03 s02 s01 s00

   __m256i tmp04 = _mm256_shuffle_epi32(H0123, _MM_SHUFFLE(0,0,0,0));
   __m256i tmp15 = _mm256_shuffle_epi32(H0123, _MM_SHUFFLE(1,1,1,1));
   __m256i tmp26 = _mm256_shuffle_epi32(H0123, _MM_SHUFFLE(2,2,2,2));
   __m256i tmp37 = _mm256_shuffle_epi32(H0123, _MM_SHUFFLE(3,3,3,3));

   __m256i xmm02 = _mm256_add_epi32( _mm256_madd_epi16(tmp04, t_dst4_A_avx), _mm256_madd_epi16(tmp15, t_dst4_B_avx));
   __m256i xmm13 = _mm256_add_epi32( _mm256_madd_epi16(tmp26, t_dst4_A_avx), _mm256_madd_epi16(tmp37, t_dst4_B_avx));

   xmm02 = _mm256_srai_epi32( _mm256_add_epi32(xmm02, round2_avx), SHIFT_INV_2ND); 
   xmm13 = _mm256_srai_epi32( _mm256_add_epi32(xmm13, round2_avx), SHIFT_INV_2ND); 

   __m256i xmm0123 = _mm256_packs_epi32(xmm02, xmm13);

    if (inplace && sizeof(DstCoeffsType) == 1) {
      __m128i tmp0;
      /* load 4 bytes from row 0 and row 1, expand to 16 bits, add temp values, clip/pack to 8 bytes */
      //tmp0 = _mm_insert_epi32(tmp0, *(int *)(dst + 0*destStride), 0);    /* load row 0 (bytes) */

// Workaround for GCC 4.7 bug
#if __GNUC__
      tmp0 = _mm_insert_epi32( _mm_setzero_si128(), *(int *)(pred + 0*predStride), 0);      /* load row 0 (bytes) */ 
#else
      tmp0 = _mm_insert_epi32( _mm_undefined_si128(), *(int *)(pred + 0*predStride), 0);      /* load row 0 (bytes) */ 
#endif

      tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 1*predStride), 1);    /* load row 1 (bytes) */
      tmp0 = _mm_cvtepu8_epi16(tmp0);                                        /* expand to 16 bytes */
      tmp0 = _mm_add_epi16(tmp0, _mm256_castsi256_si128(xmm0123));           /* add to dst */
      tmp0 = _mm_packus_epi16(tmp0, tmp0);                                   /* pack to 8 bytes */
      *(int *)(dst + 0*destStride) = _mm_extract_epi32(tmp0, 0);         /* store row 0 (bytes) */
      *(int *)(dst + 1*destStride) = _mm_extract_epi32(tmp0, 1);         /* store row 1 (bytes) */

      /* repeat for rows 2 and 3 */
      tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 2*predStride), 0);    /* load row 0 (bytes) */    

      tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 3*predStride), 1);    /* load row 1 (bytes) */
      tmp0 = _mm_cvtepu8_epi16(tmp0);                                        /* expand to 16 bytes */
      tmp0 = _mm_add_epi16(tmp0, _mm256_extracti128_si256(xmm0123, 1));      /* add to dst */
      tmp0 = _mm_packus_epi16(tmp0, tmp0);                                   /* pack to 8 bytes */
      *(int *)(dst + 2*destStride) = _mm_extract_epi32(tmp0, 0);         /* store row 0 (bytes) */
      *(int *)(dst + 3*destStride) = _mm_extract_epi32(tmp0, 1);         /* store row 1 (bytes) */
    } else if (inplace && sizeof(DstCoeffsType) == 2) {
       __m256i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;

       /* load 4 words from each row, add temp values, clip to [0, 2^bitDepth) */
       tmp0 = mm256(_mm_loadl_epi64((__m128i *)(pred + 0*predStride)));
       tmp1 = mm256(_mm_loadl_epi64((__m128i *)(pred + 1*predStride)));
       tmp2 = mm256(_mm_loadl_epi64((__m128i *)(pred + 2*predStride)));
       tmp3 = mm256(_mm_loadl_epi64((__m128i *)(pred + 3*predStride)));

       tmp0 = _mm256_unpacklo_epi64(tmp0, tmp1);
       tmp2 = _mm256_unpacklo_epi64(tmp2, tmp3);
       tmp0 = _mm256_permute2x128_si256(tmp0, tmp2, 0x20);

       /* signed saturating add to 16-bit, then clip to bitDepth */
       tmp0 = _mm256_adds_epi16(tmp0, xmm0123);
       tmp4 = _mm256_setzero_si256();
       tmp5 = _mm256_set1_epi16((1 << bitDepth) - 1);
       tmp0 = _mm256_max_epi16(tmp0, tmp4);
       tmp0 = _mm256_min_epi16(tmp0, tmp5);

      _mm_storel_epi64((__m128i*)(dst + 0*destStride), _mm256_castsi256_si128(tmp0));
      _mm_storeh_epi64((__m128i*)(dst + 1*destStride), _mm256_castsi256_si128(tmp0));
      _mm_storel_epi64((__m128i*)(dst + 2*destStride), _mm256_extracti128_si256(tmp0, 1));
      _mm_storeh_epi64((__m128i*)(dst + 3*destStride), _mm256_extracti128_si256(tmp0, 1));
    } else {
      _mm_storel_epi64((__m128i*)(dst + 0*destStride), _mm256_castsi256_si128(xmm0123));
      _mm_storeh_epi64((__m128i*)(dst + 1*destStride), _mm256_castsi256_si128(xmm0123));
      _mm_storel_epi64((__m128i*)(dst + 2*destStride), _mm256_extracti128_si256(xmm0123, 1));
      _mm_storeh_epi64((__m128i*)(dst + 3*destStride), _mm256_extracti128_si256(xmm0123, 1));
    }
}

void MAKE_NAME(h265_DST4x4Inv_16sT)(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth)
{
    if (inplace) {
        switch (bitDepth) {
        case  8:
            if (inplace == 2)
                h265_DST4x4Inv_16sT_Kernel< 8, Ipp8u, Ipp16u,  true >((Ipp8u *)pred, predStride, (Ipp16u *) destPtr, coeff, destStride);
            else
                h265_DST4x4Inv_16sT_Kernel< 8, Ipp8u, Ipp8u,  true >((Ipp8u *)pred, predStride, (Ipp8u *) destPtr, coeff, destStride);
            break;
        case  9: h265_DST4x4Inv_16sT_Kernel< 9, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 10: h265_DST4x4Inv_16sT_Kernel<10, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 11: h265_DST4x4Inv_16sT_Kernel<11, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 12: h265_DST4x4Inv_16sT_Kernel<12, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        }
    } else {
        switch (bitDepth) {
        case  8: h265_DST4x4Inv_16sT_Kernel< 8, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case  9: h265_DST4x4Inv_16sT_Kernel< 9, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 10: h265_DST4x4Inv_16sT_Kernel<10, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 11: h265_DST4x4Inv_16sT_Kernel<11, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 12: h265_DST4x4Inv_16sT_Kernel<12, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        }
    }
}

#undef coef_stride
#define coef_stride 8


template <int bitDepth, typename PredPixType, typename DstCoeffsType, bool inplace>
static void h265_DCT8x8Inv_16sT_Kernel(const PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
{
   const int SHIFT_INV_2ND = (SHIFT_INV_2ND_BASE - (bitDepth - 8));
   ALIGN_DECL(32) short tmp[8 * 8];

   int16_t *__restrict p_tmp = tmp;

   _mm256_zeroupper();

   {
      M256I_W2x8C( t_8_O0_13,  89, 75); // g_T8[1][0], g_T8[3][0]
      M256I_W2x8C( t_8_O0_57,  50, 18); // g_T8[5][0], g_T8[7][0]
      M256I_W2x8C( t_8_O1_13,  75,-18); // g_T8[1][1], g_T8[3][1]
      M256I_W2x8C( t_8_O1_57, -89,-50); // g_T8[5][1], g_T8[7][1]
      M256I_W2x8C( t_8_O2_13,  50,-89); // g_T8[1][2], g_T8[3][2]
      M256I_W2x8C( t_8_O2_57,  18, 75); // g_T8[5][2], g_T8[7][2]
      M256I_W2x8C( t_8_O3_13,  18,-50); // g_T8[1][3], g_T8[3][3]
      M256I_W2x8C( t_8_O3_57,  75,-89); // g_T8[5][3], g_T8[7][3]

      M256I_W2x8C( t_8_EO0_26,  83, 36); // g_T8[2][0], g_T8[6][0]
      M256I_W2x8C( t_8_EO1_26,  36,-83); // g_T8[2][1], g_T8[6][1]
      M256I_W2x8C( t_8_EE0_04,  64, 64); // g_T8[0][0], g_T8[4][0]
      M256I_W2x8C( t_8_EE1_04,  64,-64); // g_T8[0][1], g_T8[4][1]

      M256I_DC ( round1, 1<<(SHIFT_INV_1ST-1));

      __m256i s1 = _mm256_cvtepu16_epi32( *(const __m128i *)(coeff + 1*coef_stride));
      __m256i s3 = _mm256_cvtepu16_epi32( *(const __m128i *)(coeff + 3*coef_stride));
      __m256i s13L =  _mm256_or_si256( s1, _mm256_slli_si256( s3, 2));

      __m256i s5 = _mm256_cvtepu16_epi32( *(const __m128i *)(coeff + 5*coef_stride));
      __m256i s7 = _mm256_cvtepu16_epi32( *(const __m128i *)(coeff + 7*coef_stride));
      __m256i s57L = _mm256_or_si256( s5, _mm256_slli_si256( s7, 2));

      __m256i O0L = _mm256_add_epi32( _mm256_madd_epi16( s13L, t_8_O0_13), _mm256_madd_epi16( s57L, t_8_O0_57));
      __m256i O1L = _mm256_add_epi32( _mm256_madd_epi16( s13L, t_8_O1_13), _mm256_madd_epi16( s57L, t_8_O1_57));
      __m256i O2L = _mm256_add_epi32( _mm256_madd_epi16( s13L, t_8_O2_13), _mm256_madd_epi16( s57L, t_8_O2_57));
      __m256i O3L = _mm256_add_epi32( _mm256_madd_epi16( s13L, t_8_O3_13), _mm256_madd_epi16( s57L, t_8_O3_57));


      __m256i s0 = _mm256_cvtepu16_epi32( *(const __m128i *)(coeff + 0*coef_stride));
      __m256i s4 = _mm256_cvtepu16_epi32( *(const __m128i *)(coeff + 4*coef_stride));
      __m256i s04L = _mm256_or_si256( s0, _mm256_slli_si256( s4, 2));

      __m256i s2 = _mm256_cvtepu16_epi32( *(const __m128i *)(coeff + 2*coef_stride));
      __m256i s6 = _mm256_cvtepu16_epi32( *(const __m128i *)(coeff + 6*coef_stride));
      __m256i s26L = _mm256_or_si256( s2, _mm256_slli_si256( s6, 2));

      __m256i EO0L = _mm256_madd_epi16( s26L, t_8_EO0_26);
      __m256i EO1L = _mm256_madd_epi16( s26L, t_8_EO1_26);
      __m256i EE0L = _mm256_add_epi32( _mm256_madd_epi16( s04L, t_8_EE0_04), round1);
      __m256i EE1L = _mm256_add_epi32( _mm256_madd_epi16( s04L, t_8_EE1_04), round1);

      __m256i E0L = _mm256_add_epi32( EE0L, EO0L);
      __m256i E3L = _mm256_sub_epi32( EE0L, EO0L);
      __m256i E1L = _mm256_add_epi32( EE1L, EO1L);
      __m256i E2L = _mm256_sub_epi32( EE1L, EO1L);


      __m256i R07L = _mm256_srai_epi32( _mm256_add_epi32( E0L, O0L), SHIFT_INV_1ST);
      __m256i R07H = _mm256_srai_epi32( _mm256_sub_epi32( E0L, O0L), SHIFT_INV_1ST);
      __m256i R07  = _mm256_permute4x64_epi64( _mm256_packs_epi32( R07L, R07H), _MM_SHUFFLE(3,1,2,0));
      mm256_storel_epi128( p_tmp + 0*coef_stride, R07);
      mm256_storeh_epi128( p_tmp + 7*coef_stride, R07);

      __m256i R16L = _mm256_srai_epi32( _mm256_add_epi32( E1L, O1L), SHIFT_INV_1ST);
      __m256i R16H = _mm256_srai_epi32( _mm256_sub_epi32( E1L, O1L), SHIFT_INV_1ST);
      __m256i R16  = _mm256_permute4x64_epi64(_mm256_packs_epi32( R16L, R16H), _MM_SHUFFLE(3,1,2,0));
      mm256_storel_epi128( p_tmp + 1*coef_stride, R16);
      mm256_storeh_epi128( p_tmp + 6*coef_stride, R16);

      __m256i R25L = _mm256_srai_epi32( _mm256_add_epi32( E2L, O2L), SHIFT_INV_1ST);
      __m256i R25H = _mm256_srai_epi32( _mm256_sub_epi32( E2L, O2L), SHIFT_INV_1ST);
      __m256i R25  = _mm256_permute4x64_epi64(_mm256_packs_epi32( R25L, R25H), _MM_SHUFFLE(3,1,2,0));
      mm256_storel_epi128( p_tmp + 2*coef_stride, R25);
      mm256_storeh_epi128( p_tmp + 5*coef_stride, R25);

      __m256i R34L = _mm256_srai_epi32( _mm256_add_epi32( E3L, O3L), SHIFT_INV_1ST);
      __m256i R34H = _mm256_srai_epi32( _mm256_sub_epi32( E3L, O3L), SHIFT_INV_1ST);
      __m256i R34  = _mm256_permute4x64_epi64(_mm256_packs_epi32( R34L, R34H), _MM_SHUFFLE(3,1,2,0));
      mm256_storel_epi128( p_tmp + 3*coef_stride, R34);
      mm256_storeh_epi128( p_tmp + 4*coef_stride, R34);
    }

   p_tmp = tmp;

   for (int j = 0; j < 8; j += 2)
   {
      M256I_W8x2C( t_8x1_0,  64, 83, -64,-83,   89, 75, -89,-50);
      M256I_W8x2C( t_8x1_1,  64,-36,  64,-36,   50,-89,  75,-89);
      M256I_W8x2C( t_8x1_2,  64, 36,  64, 36,   50, 18,  75,-18);
      M256I_W8x2C( t_8x1_3, -64, 83,  64,-83,   18, 75,  18,-50);
      M256I_D4x2C( round2, 1<<(SHIFT_INV_2ND-1), 1<<(SHIFT_INV_2ND-1), 0, 0);

      // use Hi and Lo parts of YMM register for two lines
      __m256i s = _mm256_load_si256((const __m256i *)p_tmp); // s7 s6 s5 s4 s3 s2 s1 s0
      s = _mm256_shufflelo_epi16(s, _MM_SHUFFLE(3,1,2,0));
      s = _mm256_shufflehi_epi16(s, _MM_SHUFFLE(3,1,2,0));
      s = _mm256_shuffle_epi32(s, _MM_SHUFFLE(3,1,2,0)); // s7 s5 s3 s1 s6 s4 s2 s0

      // t_8x1_0 : D1_23 D0_01 C1_23 C0_01 coefs
      // t_8x1_1 : D3_23 D2_01 C3_23 C2_01 coefs
      __m256i d10c10 = _mm256_add_epi32( _mm256_madd_epi16(t_8x1_0, s), round2);     // D1 D0 (C1 + round) (C0 + round)
      __m256i d32c32 = _mm256_add_epi32( _mm256_madd_epi16(t_8x1_1, s), round2);     // D3 D2 (C3 + round) (C2 + round)

      s = _mm256_shuffle_epi32(s, _MM_SHUFFLE(2,3,0,1)); // s3 s1 s7 s5 s2 s0 s6 s4

      // t_8x1_2: D1_01 D0_23 C1_01 C0_23 coefs
      // t_8x1_3: D3_01 D2_23 C3_01 C2_23 coefs
      d10c10 = _mm256_add_epi32( d10c10, _mm256_madd_epi16(t_8x1_2, s));       //  D1 D0 C1 C0
      d32c32 = _mm256_add_epi32( d32c32, _mm256_madd_epi16(t_8x1_3, s));       //  D3 D2 C3 C2

      __m256i c3210 = _mm256_unpacklo_epi64(d10c10, d32c32);  //  C3 C2 C1 C0
      __m256i d3210 = _mm256_unpackhi_epi64(d10c10, d32c32);  //  D3 D2 D1 D0

      __m256i r4567 = _mm256_sub_epi32(c3210, d3210);      //   r4 r5 r6 r7
      __m256i r3210 = _mm256_add_epi32(c3210, d3210);      //   r3 r2 r1 r0

      __m256i r7654 = _mm256_shuffle_epi32(r4567, _MM_SHUFFLE(0,1,2,3)); // r7 r6 r5 r4

      __m256i r76543210 = _mm256_packs_epi32(_mm256_srai_epi32(r3210, SHIFT_INV_2ND), _mm256_srai_epi32(r7654, SHIFT_INV_2ND));         //  r7 r6 r5 r4 r3 r2 r1 r0

      if (inplace && sizeof(DstCoeffsType) == 1) {
         /* load dst[0-7], expand to 16 bits, add temp[0-7], clip/pack to 8 bytes */
         __m128i s = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&pred[0]));
         s = _mm_add_epi16(s, _mm256_castsi256_si128(r76543210));
         s = _mm_packus_epi16(s, s);
         _mm_storel_epi64((__m128i *)&dst[0], s);        /* store dst[0-7] (bytes) */

         s = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&pred[predStride]));
         s = _mm_add_epi16(s, _mm256_extracti128_si256(r76543210, 1));
         s = _mm_packus_epi16(s, s);
         _mm_storel_epi64((__m128i *)&dst[destStride], s);        /* store dst[0-7] (bytes) */

         dst += 2*destStride;
         pred += 2*predStride;
      } else if (inplace && sizeof(DstCoeffsType) == 2) {
         __m256i tmp0, tmp1, tmp4, tmp5;

         /* load 8 words from 2 rows, add temp values, clip to [0, 2^bitDepth) */
         tmp0 = mm256(_mm_loadu_si128((__m128i *)(&pred[0])));
         tmp1 = mm256(_mm_loadu_si128((__m128i *)(&pred[predStride])));
         tmp0 = _mm256_permute2x128_si256(tmp0, tmp1, 0x20);

         tmp0 = _mm256_adds_epi16(tmp0, r76543210);
         tmp4 = _mm256_setzero_si256();
         tmp5 = _mm256_set1_epi16((1 << bitDepth) - 1);
         tmp0 = _mm256_max_epi16(tmp0, tmp4);
         tmp0 = _mm256_min_epi16(tmp0, tmp5);

         mm256_storel_epi128(dst,             tmp0);     //  r7 r6 r5 r4 r3 r2 r1 r0
         mm256_storeh_epi128(dst + destStride, tmp0);     //  r7 r6 r5 r4 r3 r2 r1 r0
         dst += 2*destStride;
         pred += 2*predStride;
      } else {
         mm256_storel_epi128(dst,             r76543210);     //  r7 r6 r5 r4 r3 r2 r1 r0
         mm256_storeh_epi128(dst + destStride, r76543210);     //  r7 r6 r5 r4 r3 r2 r1 r0
         dst += 2*destStride;
      }

      p_tmp += 2*8;
   }
}

void MAKE_NAME(h265_DCT8x8Inv_16sT)(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth)
{
    if (inplace) {
        switch (bitDepth) {
        case  8:
            if (inplace == 2)
                h265_DCT8x8Inv_16sT_Kernel< 8, Ipp8u, Ipp16u,  true >((Ipp8u *)pred, predStride, (Ipp16u *) destPtr, coeff, destStride);
            else
                h265_DCT8x8Inv_16sT_Kernel< 8, Ipp8u, Ipp8u,  true >((Ipp8u *)pred, predStride, (Ipp8u *) destPtr, coeff, destStride);
            break;
        case  9: h265_DCT8x8Inv_16sT_Kernel< 9, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 10: h265_DCT8x8Inv_16sT_Kernel<10, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 11: h265_DCT8x8Inv_16sT_Kernel<11, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 12: h265_DCT8x8Inv_16sT_Kernel<12, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        }
    } else {
        switch (bitDepth) {
        case  8: h265_DCT8x8Inv_16sT_Kernel< 8, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case  9: h265_DCT8x8Inv_16sT_Kernel< 9, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 10: h265_DCT8x8Inv_16sT_Kernel<10, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 11: h265_DCT8x8Inv_16sT_Kernel<11, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 12: h265_DCT8x8Inv_16sT_Kernel<12, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        }
    }
}

#undef coef_stride
#define coef_stride 16

template <int bitDepth, typename PredPixType, typename DstCoeffsType, bool inplace>
static inline H265_FORCEINLINE void DCTInverse16x16_h_2nd_avx2(const PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
{
   const int SHIFT_INV_2ND = (SHIFT_INV_2ND_BASE - (bitDepth - 8));

   M256I_DC (round2y, 1<<(SHIFT_INV_2ND-1));

   for(int i=0; i<16; i+=2)
   {
      __m256i data0 = _mm256_load_si256((const __m256i *)coeff);      // s15 s14 s13 s12 s11 s10 s9 s8 | s7  s6  s5  s4  s3  s2  s1 s0
      data0 = _mm256_shuffle_epi8(data0, oddeven);                    // s_odH s_evH | s_odL s_evL
      __m256i data1 = _mm256_load_si256((const __m256i *)(coeff+16)); // next line
      data1 = _mm256_shuffle_epi8(data1, oddeven);                    // v_odH v_evH | v_odL v_evL
      data0 = _mm256_permute4x64_epi64(data0, 0xD8);                  //    s_odd    |   s_even
      data1 = _mm256_permute4x64_epi64(data1, 0xD8);                  //    v_odd    |   v_even

      __m256i y_even =_mm256_permute2x128_si256(data0, data1, 0x24);  //    v_even   |   s_even
      __m256i y_odd = _mm256_permute2x128_si256(data0, data1, 0x31);  //    v_odd    |   s_odd
      y_even = _mm256_shuffle_epi8(y_even, shuffev);                  //    v_even+  |   s_even+ (reorder: s14 s6  s12 s4 s10 s2 s8 s0)

      // y_even: s14  s6 s12  s4 s10  s2  s8  s0
      // y_odd : s15 s13 s11  s9  s7  s5  s3  s1
      __m256i xmm1 = _mm256_shuffle_epi32(y_even, _MM_SHUFFLE(0,0,0,0)); // s8  s0
      __m256i xmm2 = _mm256_shuffle_epi32(y_even, _MM_SHUFFLE(2,2,2,2)); // s12 s4
      __m256i xmm3 = _mm256_shuffle_epi32(y_even, _MM_SHUFFLE(1,1,1,1)); // s10 s2
      __m256i xmm0 = _mm256_shuffle_epi32(y_even, _MM_SHUFFLE(3,3,3,3)); // s14 s6
      xmm1 = _mm256_madd_epi16(xmm1, t_16_a0101_08);
      xmm2 = _mm256_madd_epi16(xmm2, t_16_b0110_4C);
      xmm3 = _mm256_madd_epi16(xmm3, t_16_d0123_2A);
      xmm0 = _mm256_madd_epi16(xmm0, t_16_d0123_6E);

      xmm1 = _mm256_add_epi32(xmm1, round2y);
      xmm3 = _mm256_add_epi32(xmm3, xmm0);

      xmm1 = _mm256_add_epi32(xmm1, xmm2);

      xmm2 = xmm1;
      xmm1 = _mm256_sub_epi32(xmm1, xmm3);
      xmm2 = _mm256_add_epi32(xmm2, xmm3);

      xmm3 = _mm256_shuffle_epi32(xmm1, _MM_SHUFFLE(0,1,2,3));
      // xmm2 - e0
      // xmm3 - e1

      __m256i xmm4 = _mm256_shuffle_epi32(y_odd, _MM_SHUFFLE(1,1,1,1));
      __m256i xmm5 = _mm256_shuffle_epi32(y_odd, _MM_SHUFFLE(2,2,2,2));
              xmm1 = _mm256_shuffle_epi32(y_odd, _MM_SHUFFLE(0,0,0,0));
      __m256i xmm7 = _mm256_shuffle_epi32(y_odd, _MM_SHUFFLE(3,3,3,3));

      xmm0 = _mm256_madd_epi16(xmm1, t_16_f0123_01);
      xmm1 = _mm256_madd_epi16(xmm1, t_16_f4567_01);

      __m256i xmm6 = _mm256_madd_epi16(xmm4, t_16_f0123_23);
      xmm4 = _mm256_madd_epi16(xmm4, t_16_f4567_23);

      xmm0 = _mm256_add_epi32(xmm0, xmm6);
      xmm1 = _mm256_add_epi32(xmm1, xmm4);

      xmm6 = _mm256_madd_epi16(xmm5, t_16_f0123_45);
      xmm5 = _mm256_madd_epi16(xmm5, t_16_f4567_45);
      xmm0 = _mm256_add_epi32(xmm0, xmm6);

      xmm6 = _mm256_madd_epi16(xmm7, t_16_f0123_67);
      xmm7 = _mm256_madd_epi16(xmm7, t_16_f4567_67);

      xmm1 = _mm256_add_epi32(xmm1, xmm5);
      xmm0 = _mm256_add_epi32(xmm0, xmm6);
      xmm1 = _mm256_add_epi32(xmm1, xmm7);
      // xmm0 - f0
      // xmm1 - f1

      xmm5 = _mm256_add_epi32(xmm2, xmm0);
      xmm2 = _mm256_sub_epi32(xmm2, xmm0);

      xmm6 = _mm256_add_epi32(xmm3, xmm1);
      xmm3 = _mm256_sub_epi32(xmm3, xmm1);

      xmm5 = _mm256_srai_epi32(xmm5, SHIFT_INV_2ND);
      xmm6 = _mm256_srai_epi32(xmm6, SHIFT_INV_2ND);
      xmm5 = _mm256_packs_epi32(xmm5, xmm6);

      xmm3 = _mm256_shuffle_epi32(xmm3, _MM_SHUFFLE(0,1,2,3));
      xmm2 = _mm256_shuffle_epi32(xmm2, _MM_SHUFFLE(0,1,2,3));

      xmm3 = _mm256_srai_epi32(xmm3, SHIFT_INV_2ND);
      xmm2 = _mm256_srai_epi32(xmm2, SHIFT_INV_2ND);
      xmm3 = _mm256_packs_epi32(xmm3, xmm2);

      xmm2 = _mm256_permute2x128_si256(xmm5, xmm3, 0x20); // low parts
      xmm5 = _mm256_permute2x128_si256(xmm5, xmm3, 0x31); // high parts

      if (inplace && sizeof(DstCoeffsType) == 1) {
         SAVE_RESULT_16_BYTES(xmm0, xmm2, pred[0], dst[0]);
         SAVE_RESULT_16_BYTES(xmm0, xmm5, pred[predStride], dst[destStride]);
         dst += 2 * destStride;
         pred += 2 * predStride;
      } else if (inplace && sizeof(DstCoeffsType) == 2) {
         xmm0 = _mm256_loadu_si256((__m256i *)(&pred[0]));
         xmm1 = _mm256_loadu_si256((__m256i *)(&pred[predStride]));

         xmm0 = _mm256_adds_epi16(xmm0, xmm2);
         xmm1 = _mm256_adds_epi16(xmm1, xmm5);
         xmm4 = _mm256_setzero_si256();
         xmm5 = _mm256_set1_epi16((1 << bitDepth) - 1);
         xmm0 = _mm256_max_epi16(xmm0, xmm4); xmm0 = _mm256_min_epi16(xmm0, xmm5);
         xmm1 = _mm256_max_epi16(xmm1, xmm4); xmm1 = _mm256_min_epi16(xmm1, xmm5);

         _mm256_storeu_si256((__m256i *)&dst[0], xmm0);
         _mm256_storeu_si256((__m256i *)&dst[destStride], xmm1);
         dst += 2 * destStride;
         pred += 2 * predStride;
      } else {
         _mm256_storeu_si256((__m256i *)&dst[0], xmm2);
         _mm256_storeu_si256((__m256i *)&dst[destStride], xmm5);

         dst += 2 * destStride;
      }
      coeff += 32;
   }
}

#define LOAD_UNPACK_2x8_SHORT(y, a, b) \
   y  = _mm256_cvtepu16_epi32((__m128i)*((__m128i *)(coeff + a*coef_stride))); \
   y0 = _mm256_cvtepu16_epi32((__m128i)*((__m128i *)(coeff + b*coef_stride))); \
   y0 = _mm256_slli_si256(y0, 2); \
   y  = _mm256_or_si256(y, y0);

#define SAVE_LOHI_256(y, a, b) \
   y = _mm256_permute4x64_epi64(y, 0xD8); \
   *(__m128i *)(p_tmp + a*16) = _mm256_castsi256_si128(y); \
   *(__m128i *)(p_tmp + b*16) = _mm256_extracti128_si256(y, 1);


template <int bitDepth, typename PredPixType, typename DstCoeffsType, bool inplace>
static void h265_DCT16x16Inv_16sT_Kernel(const PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
{
   ALIGN_DECL(32) short tmp[16 * 16];
   short *__restrict p_tmp = tmp;
   __m256i y0, y13L, y57L, y9BL, yDFL, y26L, yAEL, y4CL, y08L;

   for (int j = 0; j < 16; j += 8)
   {
      // odd
      LOAD_UNPACK_2x8_SHORT(y13L,  1, 3)
      LOAD_UNPACK_2x8_SHORT(y57L,  5, 7)
      LOAD_UNPACK_2x8_SHORT(y9BL,  9, 11)
      LOAD_UNPACK_2x8_SHORT(yDFL, 13, 15)

      __m256i O0L = _mm256_add_epi32(_mm256_madd_epi16(y13L, t_16_O0_13), _mm256_madd_epi16(y57L, t_16_O0_57));
      O0L = _mm256_add_epi32(O0L, _mm256_madd_epi16(y9BL, t_16_O0_9B));
      O0L = _mm256_add_epi32(O0L, _mm256_madd_epi16(yDFL, t_16_O0_DF));

      __m256i O1L = _mm256_add_epi32(_mm256_madd_epi16(y13L, t_16_O1_13), _mm256_madd_epi16(y57L, t_16_O1_57));
      O1L = _mm256_add_epi32(O1L, _mm256_madd_epi16(y9BL, t_16_O1_9B));
      O1L = _mm256_add_epi32(O1L, _mm256_madd_epi16(yDFL, t_16_O1_DF));

      __m256i O2L = _mm256_add_epi32(_mm256_madd_epi16(y13L, t_16_O2_13), _mm256_madd_epi16(y57L, t_16_O2_57));
      O2L = _mm256_add_epi32(O2L, _mm256_madd_epi16(y9BL, t_16_O2_9B));
      O2L = _mm256_add_epi32(O2L, _mm256_madd_epi16(yDFL, t_16_O2_DF));

      __m256i O3L = _mm256_add_epi32(_mm256_madd_epi16(y13L, t_16_O3_13), _mm256_madd_epi16(y57L, t_16_O3_57));
      O3L = _mm256_add_epi32(O3L, _mm256_madd_epi16(y9BL, t_16_O3_9B));
      O3L = _mm256_add_epi32(O3L, _mm256_madd_epi16(yDFL, t_16_O3_DF));

      __m256i O4L = _mm256_add_epi32(_mm256_madd_epi16(y13L, t_16_O4_13), _mm256_madd_epi16(y57L, t_16_O4_57));
      O4L = _mm256_add_epi32(O4L, _mm256_madd_epi16(y9BL, t_16_O4_9B));
      O4L = _mm256_add_epi32(O4L, _mm256_madd_epi16(yDFL, t_16_O4_DF));

      __m256i O5L = _mm256_add_epi32(_mm256_madd_epi16(y13L, t_16_O5_13), _mm256_madd_epi16(y57L, t_16_O5_57));
      O5L = _mm256_add_epi32(O5L, _mm256_madd_epi16(y9BL, t_16_O5_9B));
      O5L = _mm256_add_epi32(O5L, _mm256_madd_epi16(yDFL, t_16_O5_DF));

      __m256i O6L = _mm256_add_epi32(_mm256_madd_epi16(y13L, t_16_O6_13), _mm256_madd_epi16(y57L, t_16_O6_57));
      O6L = _mm256_add_epi32(O6L, _mm256_madd_epi16(y9BL, t_16_O6_9B));
      O6L = _mm256_add_epi32(O6L, _mm256_madd_epi16(yDFL, t_16_O6_DF));

      __m256i O7L = _mm256_add_epi32(_mm256_madd_epi16(y13L, t_16_O7_13), _mm256_madd_epi16(y57L, t_16_O7_57));
      O7L = _mm256_add_epi32(O7L, _mm256_madd_epi16(y9BL, t_16_O7_9B));
      O7L = _mm256_add_epi32(O7L, _mm256_madd_epi16(yDFL, t_16_O7_DF));

      // even
      LOAD_UNPACK_2x8_SHORT(y26L,  2, 6)
      LOAD_UNPACK_2x8_SHORT(yAEL, 10, 14)

      __m256i EO0L = _mm256_add_epi32(_mm256_madd_epi16(y26L, t_16_E0_26), _mm256_madd_epi16(yAEL, t_16_E0_AE));
      __m256i EO1L = _mm256_add_epi32(_mm256_madd_epi16(y26L, t_16_E1_26), _mm256_madd_epi16(yAEL, t_16_E1_AE));
      __m256i EO2L = _mm256_add_epi32(_mm256_madd_epi16(y26L, t_16_E2_26), _mm256_madd_epi16(yAEL, t_16_E2_AE));
      __m256i EO3L = _mm256_add_epi32(_mm256_madd_epi16(y26L, t_16_E3_26), _mm256_madd_epi16(yAEL, t_16_E3_AE));

      LOAD_UNPACK_2x8_SHORT(y08L, 0,  8)

      __m256i EEE0L = _mm256_add_epi32(_mm256_madd_epi16(y08L, t_16_EEE0_08), round1y);
      __m256i EEE1L = _mm256_add_epi32(_mm256_madd_epi16(y08L, t_16_EEE1_08), round1y);

      LOAD_UNPACK_2x8_SHORT(y4CL, 4, 12)

      __m256i EEO0L = _mm256_madd_epi16(y4CL, t_16_EEO0_4C);
      __m256i EEO1L = _mm256_madd_epi16(y4CL, t_16_EEO1_4C);

      __m256i EE0L = _mm256_add_epi32(EEE0L, EEO0L);
      __m256i EE3L = _mm256_sub_epi32(EEE0L, EEO0L);

      __m256i EE1L = _mm256_add_epi32(EEE1L, EEO1L);
      __m256i EE2L = _mm256_sub_epi32(EEE1L, EEO1L);

      __m256i E0L = _mm256_add_epi32(EE0L, EO0L);
      __m256i E7L = _mm256_sub_epi32(EE0L, EO0L);

      __m256i E1L = _mm256_add_epi32(EE1L, EO1L);
      __m256i E6L = _mm256_sub_epi32(EE1L, EO1L);

      __m256i E2L = _mm256_add_epi32(EE2L, EO2L);
      __m256i E5L = _mm256_sub_epi32(EE2L, EO2L);

      __m256i E3L = _mm256_add_epi32(EE3L, EO3L);
      __m256i E4L = _mm256_sub_epi32(EE3L, EO3L);

      __m256i R0F = _mm256_packs_epi32(_mm256_srai_epi32(_mm256_add_epi32(E0L, O0L), SHIFT_INV_1ST), _mm256_srai_epi32(_mm256_sub_epi32(E0L, O0L), SHIFT_INV_1ST));
      SAVE_LOHI_256(R0F, 0, 15)

      __m256i R1E = _mm256_packs_epi32(_mm256_srai_epi32(_mm256_add_epi32(E1L, O1L), SHIFT_INV_1ST), _mm256_srai_epi32(_mm256_sub_epi32(E1L, O1L), SHIFT_INV_1ST));
      SAVE_LOHI_256(R1E, 1, 14)

      __m256i R2D = _mm256_packs_epi32(_mm256_srai_epi32(_mm256_add_epi32(E2L, O2L), SHIFT_INV_1ST), _mm256_srai_epi32(_mm256_sub_epi32(E2L, O2L), SHIFT_INV_1ST));
      SAVE_LOHI_256(R2D, 2, 13)

      __m256i R3C = _mm256_packs_epi32(_mm256_srai_epi32(_mm256_add_epi32(E3L, O3L), SHIFT_INV_1ST), _mm256_srai_epi32(_mm256_sub_epi32(E3L, O3L), SHIFT_INV_1ST));
      SAVE_LOHI_256(R3C, 3, 12)

      __m256i R4B = _mm256_packs_epi32(_mm256_srai_epi32(_mm256_add_epi32(E4L, O4L), SHIFT_INV_1ST), _mm256_srai_epi32(_mm256_sub_epi32(E4L, O4L), SHIFT_INV_1ST));
      SAVE_LOHI_256(R4B, 4, 11)

      __m256i R5A = _mm256_packs_epi32(_mm256_srai_epi32(_mm256_add_epi32(E5L, O5L), SHIFT_INV_1ST), _mm256_srai_epi32(_mm256_sub_epi32(E5L, O5L), SHIFT_INV_1ST));
      SAVE_LOHI_256(R5A, 5, 10)

      __m256i R69 = _mm256_packs_epi32(_mm256_srai_epi32(_mm256_add_epi32(E6L, O6L), SHIFT_INV_1ST), _mm256_srai_epi32(_mm256_sub_epi32(E6L, O6L), SHIFT_INV_1ST));
      SAVE_LOHI_256(R69, 6, 9)

      __m256i R78 = _mm256_packs_epi32(_mm256_srai_epi32(_mm256_add_epi32(E7L, O7L), SHIFT_INV_1ST), _mm256_srai_epi32(_mm256_sub_epi32(E7L, O7L), SHIFT_INV_1ST));
      SAVE_LOHI_256(R78, 7, 8)

      coeff += 8;
      p_tmp += 8;
   }

   DCTInverse16x16_h_2nd_avx2<bitDepth, PredPixType, DstCoeffsType, inplace>(pred, predStride, dst, tmp, destStride);
}

void MAKE_NAME(h265_DCT16x16Inv_16sT)(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth)
{
    if (inplace) {
        switch (bitDepth) {
        case  8:
            if (inplace == 2)
                h265_DCT16x16Inv_16sT_Kernel< 8, Ipp8u, Ipp16u,  true >((Ipp8u *)pred, predStride, (Ipp16u *) destPtr, coeff, destStride);
            else
                h265_DCT16x16Inv_16sT_Kernel< 8, Ipp8u, Ipp8u,  true >((Ipp8u *)pred, predStride, (Ipp8u *) destPtr, coeff, destStride);
            break;
        case  9: h265_DCT16x16Inv_16sT_Kernel< 9, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 10: h265_DCT16x16Inv_16sT_Kernel<10, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 11: h265_DCT16x16Inv_16sT_Kernel<11, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        case 12: h265_DCT16x16Inv_16sT_Kernel<12, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
        }
    } else {
        switch (bitDepth) {
        case  8: h265_DCT16x16Inv_16sT_Kernel< 8, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case  9: h265_DCT16x16Inv_16sT_Kernel< 9, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 10: h265_DCT16x16Inv_16sT_Kernel<10, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 11: h265_DCT16x16Inv_16sT_Kernel<11, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        case 12: h265_DCT16x16Inv_16sT_Kernel<12, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0, (Ipp16s *)destPtr, coeff, destStride); break;
        }
    }
}

} // end namespace MFX_HEVC_PP

#endif //#if defined (MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
