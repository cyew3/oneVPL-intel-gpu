/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/

// Forward HEVC Transform functions optimised by intrinsics
// Sizes: 4, 8, 16, 32
// NOTE: we don't have AVX2 optimization, so the same SSE2(4) impl is used

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"
#include "mfx_h265_transform_consts.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_ATOM) && defined(MFX_TARGET_OPTIMIZATION_ATOM) || \
    defined(MFX_MAKENAME_SSE4) && defined(MFX_TARGET_OPTIMIZATION_SSE4) || \
    defined(MFX_MAKENAME_SSSE3) && defined(MFX_TARGET_OPTIMIZATION_SSSE3)

#pragma warning (disable : 4310 ) /* disable cast truncates constant value */

namespace MFX_HEVC_PP
{

#define ALIGNED_SSE2 ALIGN_DECL(16)
#define REG_DCT      65535

//#include <pmmintrin.h> //SSE3 (Prescott) Pentium(R) 4 processor (_mm_lddqu_si128() present )
#include <smmintrin.h> // SSE4.1, Intel(R) Core(TM) 2 Duo       (_mm_cvtepi16_epi32(), _mm_mullo_epi32() present)
#include <nmmintrin.h> //SSE4.2, Intel(R) Core(TM) 2 Duo;
#ifdef MFX_EMULATE_SSSE3
#include "mfx_ssse3_emulation.h"
#endif

//---------------------------------------------------------
// aya: should be move in common place (aka: mfx_h265_optimization_defs.h) 
// but common include file mfx_h265_optimization.h should be free from platform specific defs

#if defined(_WIN32) || defined(_WIN64)
#define M128I_VAR(x, v) \
    static const __m128i x = v
#else
#define M128I_VAR(x, v) \
    static ALIGN_DECL(16) const char array_##x[16] = v; \
    static const __m128i* p_##x = (const __m128i*) array_##x; \
    static const __m128i x = * p_##x
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

#define M128I_W4x2C_INIT(w0,w1,w2,w3) {\
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF), \
    (char)((w0)&0xFF),(char)(((w0)>>8)&0xFF), (char)((w1)&0xFF),(char)(((w1)>>8)&0xFF), \
    (char)((w2)&0xFF),(char)(((w2)>>8)&0xFF), (char)((w3)&0xFF),(char)(((w3)>>8)&0xFF)}

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
    
#define M128I_WC(x, v) M128I_VAR(x, M128I_WC_INIT(v))

#define M128I_W2x4C(x, w0,w1) M128I_VAR(x, M128I_W2x4C_INIT(w0,w1))

#define M128I_W4x2C(x, w0,w1,w2,w3) M128I_VAR(x, M128I_W4x2C_INIT(w0,w1,w2,w3))

#define M128I_W8C(x, w0,w1,w2,w3,w4,w5,w6,w7) M128I_VAR(x, M128I_W8C_INIT(w0,w1,w2,w3,w4,w5,w6,w7))

#define M128I_DC(x, v) M128I_VAR(x, M128I_DC_INIT(v))

#define M128I_D4C(x, w0,w1,w2,w3) M128I_VAR(x, M128I_D4C_INIT(w0,w1,w2,w3))

//---------------------------------------------------------

#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))

#define _mm_storeh_epi64(p, A) _mm_storeh_pd((double *)(p), _mm_castsi128_pd(A))

#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (double *)(p)))

#ifndef _INCLUDED_SMM
#define _mm_cvtepi16_epi32(A) _mm_srai_epi32(_mm_unpacklo_epi16(A, A), 16)
#endif

#ifdef _INCLUDED_PMM
#define load_unaligned(x) _mm_lddqu_si128(x)
#else
#define load_unaligned(x) _mm_loadu_si128(x)
#endif

//---------------------------------------------------------

#define SHIFT_FRW4_1ST_BASE 1
#define SHIFT_FRW4_2ND 8

#define SHIFT_FRW8_1ST_BASE 2
#define SHIFT_FRW8_2ND 9

#define SHIFT_FRW16_1ST_BASE 3
#define SHIFT_FRW16_2ND 10

#define SHIFT_FRW32_1ST_BASE 4

#define coef_stride 4
    template <int SHIFT_FRW4_1ST>
    static void h265_DST4x4Fwd_16s_Kernel(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst)
    {
        //const short iDST4[4][4] =
        //{
        //  { 29, 55, 74, 84},
        //  { 74, 74,  0,-74},
        //  { 84,-29,-74, 55},
        //  { 55,-84, 74,-29}
        //};
        M128I_W8C( tf_dst4_A,  29, 55,   74, 74,  84,-29,  55,-84);
        M128I_W8C( tf_dst4_B,  74, 84,    0,-74, -74, 55,  74,-29);

        M128I_DC (round1, 1<<(SHIFT_FRW4_1ST-1));

        M128I_W2x4C( tf_4dst_0_01,  29,  55);
        M128I_W2x4C( tf_4dst_1_01,  74,  74);
        M128I_W2x4C( tf_4dst_2_01,  84, -29);
        M128I_W2x4C( tf_4dst_3_01,  55, -84);
        M128I_W2x4C( tf_4dst_0_23,  74,  84);
        M128I_W2x4C( tf_4dst_1_23,   0, -74);
        M128I_W2x4C( tf_4dst_2_23, -74,  55);
        M128I_W2x4C( tf_4dst_3_23,  74, -29);

        M128I_DC (round2, 1<<(SHIFT_FRW4_2ND-1));


        __m128i h0 = _mm_loadl_epi64((const __m128i *)(src + 0*srcStride));
        __m128i h1 = _mm_loadl_epi64((const __m128i *)(src + 1*srcStride));
        __m128i h2 = _mm_loadl_epi64((const __m128i *)(src + 2*srcStride));
        __m128i h3 = _mm_loadl_epi64((const __m128i *)(src + 3*srcStride));

        __m128i tmp0 = _mm_shuffle_epi32(h0, _MM_SHUFFLE(0,0,0,0));
        __m128i tmp1 = _mm_shuffle_epi32(h0, _MM_SHUFFLE(1,1,1,1));
        __m128i tmp2 = _mm_shuffle_epi32(h1, _MM_SHUFFLE(0,0,0,0));
        __m128i tmp3 = _mm_shuffle_epi32(h1, _MM_SHUFFLE(1,1,1,1));

        __m128i xmm0 = _mm_add_epi32(_mm_madd_epi16(tmp0, tf_dst4_A), _mm_madd_epi16(tmp1, tf_dst4_B));
        __m128i xmm1 = _mm_add_epi32(_mm_madd_epi16(tmp2, tf_dst4_A), _mm_madd_epi16(tmp3, tf_dst4_B));

        xmm0 = _mm_srai_epi32( _mm_add_epi32(xmm0, round1), SHIFT_FRW4_1ST); 
        xmm1 = _mm_srai_epi32( _mm_add_epi32(xmm1, round1), SHIFT_FRW4_1ST); 

        __m128i tmp4 = _mm_shuffle_epi32(h2, _MM_SHUFFLE(0,0,0,0));
        __m128i tmp5 = _mm_shuffle_epi32(h2, _MM_SHUFFLE(1,1,1,1));
        __m128i tmp6 = _mm_shuffle_epi32(h3, _MM_SHUFFLE(0,0,0,0));
        __m128i tmp7 = _mm_shuffle_epi32(h3, _MM_SHUFFLE(1,1,1,1));

        __m128i xmm2 = _mm_add_epi32( _mm_madd_epi16(tmp4, tf_dst4_A), _mm_madd_epi16(tmp5, tf_dst4_B));
        __m128i xmm3 = _mm_add_epi32( _mm_madd_epi16(tmp6, tf_dst4_A), _mm_madd_epi16(tmp7, tf_dst4_B));

        xmm2 = _mm_srai_epi32( _mm_add_epi32(xmm2, round1), SHIFT_FRW4_1ST);
        xmm3 = _mm_srai_epi32( _mm_add_epi32(xmm3, round1), SHIFT_FRW4_1ST);

        __m128i s02H = _mm_packs_epi32(xmm0, xmm2);  // s13 s12 s11 s10 s03 s02 s01 s00
        __m128i s13H = _mm_packs_epi32(xmm1, xmm3);  // s23 s23 s22 s22 s31 s21 s30 s20

        __m128i s01L = _mm_unpacklo_epi16 (s02H, s13H);
        __m128i s23L = _mm_unpackhi_epi16 (s02H, s13H);


        __m128i R0 = _mm_add_epi32( _mm_madd_epi16(s01L, tf_4dst_0_01), _mm_madd_epi16(s23L, tf_4dst_0_23));
        __m128i R1 = _mm_add_epi32( _mm_madd_epi16(s01L, tf_4dst_1_01), _mm_madd_epi16(s23L, tf_4dst_1_23));
        __m128i R2 = _mm_add_epi32( _mm_madd_epi16(s01L, tf_4dst_2_01), _mm_madd_epi16(s23L, tf_4dst_2_23));
        __m128i R3 = _mm_add_epi32( _mm_madd_epi16(s01L, tf_4dst_3_01), _mm_madd_epi16(s23L, tf_4dst_3_23));

        R0 = _mm_srai_epi32( _mm_add_epi32( R0, round2), SHIFT_FRW4_2ND);
        R1 = _mm_srai_epi32( _mm_add_epi32( R1, round2), SHIFT_FRW4_2ND);
        R2 = _mm_srai_epi32( _mm_add_epi32( R2, round2), SHIFT_FRW4_2ND);
        R3 = _mm_srai_epi32( _mm_add_epi32( R3, round2), SHIFT_FRW4_2ND);

        __m128i R01L = _mm_packs_epi32(R0, R1);
        __m128i R23L = _mm_packs_epi32(R2, R3);

        _mm_storel_epi64((__m128i*)(dst + 0*coef_stride), R01L);
        _mm_storeh_epi64((__m128i*)(dst + 1*coef_stride), R01L);
        _mm_storel_epi64((__m128i*)(dst + 2*coef_stride), R23L);
        _mm_storeh_epi64((__m128i*)(dst + 3*coef_stride), R23L);
    }

    void H265_FASTCALL MAKE_NAME(h265_DST4x4Fwd_16s)(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth)
    {
        switch (bitDepth) {
        case  8: h265_DST4x4Fwd_16s_Kernel<SHIFT_FRW4_1ST_BASE + 0>(src, srcStride, dst);  break;
        case  9: h265_DST4x4Fwd_16s_Kernel<SHIFT_FRW4_1ST_BASE + 1>(src, srcStride, dst);  break;
        case 10: h265_DST4x4Fwd_16s_Kernel<SHIFT_FRW4_1ST_BASE + 2>(src, srcStride, dst);  break;
        }
    }

    template <int SHIFT_FRW4_1ST>
    static void h265_DCT4x4Fwd_16s_Kernel(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst)
    {
        //const short iDCT4[4][4] =
        //{
        //  { 64, 64, 64, 64},
        //  { 83, 36,-36,-83},
        //  { 64,-64,-64, 64},
        //  { 36,-83, 83,-36}
        //};
        M128I_W8C( tf_dct4_A,  64, 64,   83, 36,  64,-64,  36,-83);
        M128I_W8C( tf_dct4_B,  64, 64,  -36,-83, -64, 64,  83,-36);

        M128I_DC (round1, 1<<(SHIFT_FRW4_1ST-1));

        M128I_W2x4C( tf_4dct_0_01,  64,  64);
        M128I_W2x4C( tf_4dct_1_01,  83,  36);
        M128I_W2x4C( tf_4dct_2_01,  64, -64);
        M128I_W2x4C( tf_4dct_3_01,  36, -83);
        M128I_W2x4C( tf_4dct_0_23,  64,  64);
        M128I_W2x4C( tf_4dct_1_23, -36, -83);
        M128I_W2x4C( tf_4dct_2_23, -64,  64);
        M128I_W2x4C( tf_4dct_3_23,  83, -36);

        M128I_DC (round2, 1<<(SHIFT_FRW4_2ND-1));


        __m128i h0 = _mm_loadl_epi64((const __m128i *)(src + 0*srcStride));
        __m128i h1 = _mm_loadl_epi64((const __m128i *)(src + 1*srcStride));
        __m128i h2 = _mm_loadl_epi64((const __m128i *)(src + 2*srcStride));
        __m128i h3 = _mm_loadl_epi64((const __m128i *)(src + 3*srcStride));

        __m128i tmp0 = _mm_shuffle_epi32(h0, _MM_SHUFFLE(0,0,0,0));
        __m128i tmp1 = _mm_shuffle_epi32(h0, _MM_SHUFFLE(1,1,1,1));
        __m128i tmp2 = _mm_shuffle_epi32(h1, _MM_SHUFFLE(0,0,0,0));
        __m128i tmp3 = _mm_shuffle_epi32(h1, _MM_SHUFFLE(1,1,1,1));

        __m128i xmm0 = _mm_add_epi32(_mm_madd_epi16(tmp0, tf_dct4_A), _mm_madd_epi16(tmp1, tf_dct4_B));
        __m128i xmm1 = _mm_add_epi32(_mm_madd_epi16(tmp2, tf_dct4_A), _mm_madd_epi16(tmp3, tf_dct4_B));

        xmm0 = _mm_srai_epi32( _mm_add_epi32(xmm0, round1), SHIFT_FRW4_1ST); 
        xmm1 = _mm_srai_epi32( _mm_add_epi32(xmm1, round1), SHIFT_FRW4_1ST); 

        __m128i tmp4 = _mm_shuffle_epi32(h2, _MM_SHUFFLE(0,0,0,0));
        __m128i tmp5 = _mm_shuffle_epi32(h2, _MM_SHUFFLE(1,1,1,1));
        __m128i tmp6 = _mm_shuffle_epi32(h3, _MM_SHUFFLE(0,0,0,0));
        __m128i tmp7 = _mm_shuffle_epi32(h3, _MM_SHUFFLE(1,1,1,1));

        __m128i xmm2 = _mm_add_epi32( _mm_madd_epi16(tmp4, tf_dct4_A), _mm_madd_epi16(tmp5, tf_dct4_B));
        __m128i xmm3 = _mm_add_epi32( _mm_madd_epi16(tmp6, tf_dct4_A), _mm_madd_epi16(tmp7, tf_dct4_B));

        xmm2 = _mm_srai_epi32( _mm_add_epi32(xmm2, round1), SHIFT_FRW4_1ST);
        xmm3 = _mm_srai_epi32( _mm_add_epi32(xmm3, round1), SHIFT_FRW4_1ST);

        __m128i s02H = _mm_packs_epi32(xmm0, xmm2);  // s13 s12 s11 s10 s03 s02 s01 s00
        __m128i s13H = _mm_packs_epi32(xmm1, xmm3);  // s23 s23 s22 s22 s31 s21 s30 s20

        __m128i s01L = _mm_unpacklo_epi16 (s02H, s13H);
        __m128i s23L = _mm_unpackhi_epi16 (s02H, s13H);


        __m128i R0 = _mm_add_epi32( _mm_madd_epi16(s01L, tf_4dct_0_01), _mm_madd_epi16(s23L, tf_4dct_0_23));
        __m128i R1 = _mm_add_epi32( _mm_madd_epi16(s01L, tf_4dct_1_01), _mm_madd_epi16(s23L, tf_4dct_1_23));
        __m128i R2 = _mm_add_epi32( _mm_madd_epi16(s01L, tf_4dct_2_01), _mm_madd_epi16(s23L, tf_4dct_2_23));
        __m128i R3 = _mm_add_epi32( _mm_madd_epi16(s01L, tf_4dct_3_01), _mm_madd_epi16(s23L, tf_4dct_3_23));

        R0 = _mm_srai_epi32( _mm_add_epi32( R0, round2), SHIFT_FRW4_2ND);
        R1 = _mm_srai_epi32( _mm_add_epi32( R1, round2), SHIFT_FRW4_2ND);
        R2 = _mm_srai_epi32( _mm_add_epi32( R2, round2), SHIFT_FRW4_2ND);
        R3 = _mm_srai_epi32( _mm_add_epi32( R3, round2), SHIFT_FRW4_2ND);

        __m128i R01L = _mm_packs_epi32(R0, R1);
        __m128i R23L = _mm_packs_epi32(R2, R3);

        _mm_storel_epi64((__m128i*)(dst + 0*coef_stride), R01L);
        _mm_storeh_epi64((__m128i*)(dst + 1*coef_stride), R01L);
        _mm_storel_epi64((__m128i*)(dst + 2*coef_stride), R23L);
        _mm_storeh_epi64((__m128i*)(dst + 3*coef_stride), R23L);
    }

    void H265_FASTCALL MAKE_NAME(h265_DCT4x4Fwd_16s)(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth)
    {
        switch (bitDepth) {
        case  8: h265_DCT4x4Fwd_16s_Kernel<SHIFT_FRW4_1ST_BASE + 0>(src, srcStride, dst);  break;
        case  9: h265_DCT4x4Fwd_16s_Kernel<SHIFT_FRW4_1ST_BASE + 1>(src, srcStride, dst);  break;
        case 10: h265_DCT4x4Fwd_16s_Kernel<SHIFT_FRW4_1ST_BASE + 2>(src, srcStride, dst);  break;
        }
    }

#undef coef_stride
#define coef_stride 8
    template <int SHIFT_FRW8_1ST>
    static void h265_DCT8x8Fwd_16s_Kernel(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst)
    {
        //static const short g_aiT8[8][8] =
        //{
        //  { 64, 64, 64, 64, 64, 64, 64, 64},
        //  { 89, 75, 50, 18,-18,-50,-75,-89},
        //  { 83, 36,-36,-83,-83,-36, 36, 83},
        //  { 75,-18,-89,-50, 50, 89, 18,-75},
        //  { 64,-64,-64, 64, 64,-64,-64, 64},
        //  { 50,-89, 18, 75,-75,-18, 89,-50},
        //  { 36,-83, 83,-36,-36, 83,-83, 36},
        //  { 18,-50, 75,-89, 89,-75, 50,-18}
        //};
        M128I_W8C( tf_dct8h_0,  64, 64,  50, 18,  -83,-36,  18,-75);
        M128I_W8C( tf_dct8h_1,  64, 64, -18,-50,   36, 83,  75,-18);
        M128I_W8C( tf_dct8h_2,  64, 64, -75,-89,   83, 36, -89,-50);
        M128I_W8C( tf_dct8h_3,  64, 64,  89, 75,  -36,-83,  50, 89);
        M128I_W8C( tf_dct8h_4,  64,-64,  18, 75,  -36, 83,  50,-18);
        M128I_W8C( tf_dct8h_5, -64, 64, -75,-18,  -83, 36,  18,-50);
        M128I_W8C( tf_dct8h_6,  64,-64,  89,-50,   36,-83,  75,-89);
        M128I_W8C( tf_dct8h_7, -64, 64,  50,-89,   83,-36,  89,-75);

        M128I_DC (round1, 1<<(SHIFT_FRW8_1ST-1));

        M128I_W2x4C( tf_dct8_0_01,  64, 64);
        M128I_W2x4C( tf_dct8_1_01,  89, 75);
        M128I_W2x4C( tf_dct8_2_01,  83, 36);
        M128I_W2x4C( tf_dct8_3_01,  75,-18);
        M128I_W2x4C( tf_dct8_4_01,  64,-64);
        M128I_W2x4C( tf_dct8_5_01,  50,-89);
        M128I_W2x4C( tf_dct8_6_01,  36,-83);
        M128I_W2x4C( tf_dct8_7_01,  18,-50);

        M128I_W2x4C( tf_dct8_0_23,  64, 64);
        M128I_W2x4C( tf_dct8_1_23,  50, 18);
        M128I_W2x4C( tf_dct8_2_23, -36,-83);
        M128I_W2x4C( tf_dct8_3_23, -89,-50);
        M128I_W2x4C( tf_dct8_4_23, -64, 64);
        M128I_W2x4C( tf_dct8_5_23,  18, 75);
        M128I_W2x4C( tf_dct8_6_23,  83,-36);
        M128I_W2x4C( tf_dct8_7_23,  75,-89);

        M128I_W2x4C( tf_dct8_0_45,  64, 64);
        M128I_W2x4C( tf_dct8_1_45, -18,-50);
        M128I_W2x4C( tf_dct8_2_45, -83,-36);
        M128I_W2x4C( tf_dct8_3_45,  50, 89);
        M128I_W2x4C( tf_dct8_4_45,  64,-64);
        M128I_W2x4C( tf_dct8_5_45, -75,-18);
        M128I_W2x4C( tf_dct8_6_45, -36, 83);
        M128I_W2x4C( tf_dct8_7_45,  89,-75);

        M128I_W2x4C( tf_dct8_0_67,  64, 64);
        M128I_W2x4C( tf_dct8_1_67, -75,-89);
        M128I_W2x4C( tf_dct8_2_67,  36, 83);
        M128I_W2x4C( tf_dct8_3_67,  18,-75);
        M128I_W2x4C( tf_dct8_4_67, -64, 64);
        M128I_W2x4C( tf_dct8_5_67,  89,-50);
        M128I_W2x4C( tf_dct8_6_67, -83, 36);
        M128I_W2x4C( tf_dct8_7_67,  50,-18);

        M128I_DC (round2, 1<<(SHIFT_FRW8_2ND-1));

        ALIGNED_SSE2 short tmp[8 * 8];

        short *H265_RESTRICT p_tmp = tmp;
        const int tmp_stride = 8;

        for(int i=0; i<8; ++i)
        {
            __m128i s = _mm_load_si128((const __m128i *)(src + i*srcStride)); // s7 s6 s5 s4 s3 s2 s1 s0

            __m128i acc_c = round1;
            __m128i acc_d = round1;

            acc_c = _mm_add_epi32( acc_c, _mm_madd_epi16( s, tf_dct8h_0)); // c3_67 c2_45 c1_23 c0_01
            acc_d = _mm_add_epi32( acc_d, _mm_madd_epi16( s, tf_dct8h_4)); // d3_67 d2_45 d1_23 d0_01

            {
                __m128i t1 = _mm_shuffle_epi32(s, _MM_SHUFFLE(0,3,2,1)); // s1 s0 s7 s6 s5 s4 s3 s2

                acc_c = _mm_add_epi32( acc_c, _mm_madd_epi16( t1, tf_dct8h_1)); // c3_01 c2_67 c1_45 c0_23
                acc_d = _mm_add_epi32( acc_d, _mm_madd_epi16( t1, tf_dct8h_5)); // d3_01 d2_67 d1_45 d0_23
            }

            {
                __m128i t2 = _mm_shuffle_epi32( s, _MM_SHUFFLE(1,0,3,2)); // s3 s2 s1 s0 s7 s6 s5 s4

                acc_c = _mm_add_epi32( acc_c, _mm_madd_epi16( t2, tf_dct8h_2)); // c3_23 c2_01 c1_67 c0_45
                acc_d = _mm_add_epi32( acc_d, _mm_madd_epi16( t2, tf_dct8h_6)); // d3_23 d2_01 d1_67 d0_45
            }


            {
                __m128i t3 = _mm_shuffle_epi32( s, _MM_SHUFFLE(2,1,0,3));// 00111001b);//         ; s5 s4 s3 s2 s1 s0 s7 s6

                acc_c = _mm_add_epi32( acc_c, _mm_madd_epi16( t3, tf_dct8h_3)); // c3_45 c2_23 c1_01 c0_67
                acc_d = _mm_add_epi32( acc_d, _mm_madd_epi16( t3, tf_dct8h_7)); // d3_45 d2_23 d1_01 d0_67
            }

            __m128i r = _mm_packs_epi32(_mm_srai_epi32( acc_c, SHIFT_FRW8_1ST), _mm_srai_epi32( acc_d, SHIFT_FRW8_1ST)); // clip(-32768, 32767)

            _mm_store_si128((__m128i *)(p_tmp), r);
            p_tmp += tmp_stride;
        }

        p_tmp = tmp;

        for (int j = 0; j < 8; j += 4)
        {
            __m128i s0 = _mm_loadl_epi64((const __m128i *)(p_tmp + 0*tmp_stride)); // s03, s02, s01, s00
            __m128i s1 = _mm_loadl_epi64((const __m128i *)(p_tmp + 1*tmp_stride)); // s13, s12, s11, s10
            __m128i s01 = _mm_unpacklo_epi16(s0, s1); // s13, s03, s12, s02, s11, s01, s10, s00

            __m128i s2 = _mm_loadl_epi64((const __m128i *)(p_tmp + 2*tmp_stride));
            __m128i s3 = _mm_loadl_epi64((const __m128i *)(p_tmp + 3*tmp_stride));
            __m128i s23 = _mm_unpacklo_epi16(s2, s3); // s33, s23, s32, s22, s31, s21, s30, s20

            __m128i s4 = _mm_loadl_epi64((const __m128i *)(p_tmp + 4*tmp_stride));
            __m128i s5 = _mm_loadl_epi64((const __m128i *)(p_tmp + 5*tmp_stride));
            __m128i s45 = _mm_unpacklo_epi16(s4, s5); // s53, s43, s52, s42, s51, s41, s50, s40

            __m128i s6 = _mm_loadl_epi64((const __m128i *)(p_tmp + 6*tmp_stride));
            __m128i s7 = _mm_loadl_epi64((const __m128i *)(p_tmp + 7*tmp_stride));
            __m128i s67 = _mm_unpacklo_epi16(s6, s7); // s73, s63, s72, s62, s71, s61, s70, s60

            {
                __m128i acc_0 = round2;
                acc_0 = _mm_add_epi32(acc_0, _mm_madd_epi16(tf_dct8_0_01, s01));
                acc_0 = _mm_add_epi32(acc_0, _mm_madd_epi16(tf_dct8_0_23, s23));
                acc_0 = _mm_add_epi32(acc_0, _mm_madd_epi16(tf_dct8_0_45, s45));
                acc_0 = _mm_add_epi32(acc_0, _mm_madd_epi16(tf_dct8_0_67, s67));
                acc_0 = _mm_srai_epi32(acc_0, SHIFT_FRW8_2ND);

                __m128i acc_1 = round2;
                acc_1 = _mm_add_epi32(acc_1, _mm_madd_epi16(tf_dct8_1_01, s01));
                acc_1 = _mm_add_epi32(acc_1, _mm_madd_epi16(tf_dct8_1_23, s23));
                acc_1 = _mm_add_epi32(acc_1, _mm_madd_epi16(tf_dct8_1_45, s45));
                acc_1 = _mm_add_epi32(acc_1, _mm_madd_epi16(tf_dct8_1_67, s67));
                acc_1 = _mm_srai_epi32(acc_1, SHIFT_FRW8_2ND);

                __m128i r01 = _mm_packs_epi32(acc_0, acc_1);
                _mm_storel_epi64((__m128i *)(dst + 0*coef_stride), r01);
                _mm_storeh_epi64((__m128i *)(dst + 1*coef_stride), r01);
            }
            {
                __m128i acc_2 = round2;
                acc_2 = _mm_add_epi32(acc_2, _mm_madd_epi16(tf_dct8_2_01, s01));
                acc_2 = _mm_add_epi32(acc_2, _mm_madd_epi16(tf_dct8_2_23, s23));
                acc_2 = _mm_add_epi32(acc_2, _mm_madd_epi16(tf_dct8_2_45, s45));
                acc_2 = _mm_add_epi32(acc_2, _mm_madd_epi16(tf_dct8_2_67, s67));
                acc_2 = _mm_srai_epi32(acc_2, SHIFT_FRW8_2ND);

                __m128i acc_3 = round2;
                acc_3 = _mm_add_epi32(acc_3, _mm_madd_epi16(tf_dct8_3_01, s01));
                acc_3 = _mm_add_epi32(acc_3, _mm_madd_epi16(tf_dct8_3_23, s23));
                acc_3 = _mm_add_epi32(acc_3, _mm_madd_epi16(tf_dct8_3_45, s45));
                acc_3 = _mm_add_epi32(acc_3, _mm_madd_epi16(tf_dct8_3_67, s67));
                acc_3 = _mm_srai_epi32(acc_3, SHIFT_FRW8_2ND);

                __m128i r23 = _mm_packs_epi32(acc_2, acc_3);
                _mm_storel_epi64((__m128i *)(dst + 2*coef_stride), r23);
                _mm_storeh_epi64((__m128i *)(dst + 3*coef_stride), r23);
            }
            {
                __m128i acc_4 = round2;
                acc_4 = _mm_add_epi32(acc_4, _mm_madd_epi16(tf_dct8_4_01, s01));
                acc_4 = _mm_add_epi32(acc_4, _mm_madd_epi16(tf_dct8_4_23, s23));
                acc_4 = _mm_add_epi32(acc_4, _mm_madd_epi16(tf_dct8_4_45, s45));
                acc_4 = _mm_add_epi32(acc_4, _mm_madd_epi16(tf_dct8_4_67, s67));
                acc_4 = _mm_srai_epi32(acc_4, SHIFT_FRW8_2ND);

                __m128i acc_5 = round2;
                acc_5 = _mm_add_epi32(acc_5, _mm_madd_epi16(tf_dct8_5_01, s01));
                acc_5 = _mm_add_epi32(acc_5, _mm_madd_epi16(tf_dct8_5_23, s23));
                acc_5 = _mm_add_epi32(acc_5, _mm_madd_epi16(tf_dct8_5_45, s45));
                acc_5 = _mm_add_epi32(acc_5, _mm_madd_epi16(tf_dct8_5_67, s67));
                acc_5 = _mm_srai_epi32(acc_5, SHIFT_FRW8_2ND);

                __m128i r45 = _mm_packs_epi32(acc_4, acc_5);
                _mm_storel_epi64((__m128i *)(dst + 4*coef_stride), r45);
                _mm_storeh_epi64((__m128i *)(dst + 5*coef_stride), r45);
            }
            {
                __m128i acc_6 = round2;
                acc_6 = _mm_add_epi32(acc_6, _mm_madd_epi16(tf_dct8_6_01, s01));
                acc_6 = _mm_add_epi32(acc_6, _mm_madd_epi16(tf_dct8_6_23, s23));
                acc_6 = _mm_add_epi32(acc_6, _mm_madd_epi16(tf_dct8_6_45, s45));
                acc_6 = _mm_add_epi32(acc_6, _mm_madd_epi16(tf_dct8_6_67, s67));
                acc_6 = _mm_srai_epi32(acc_6, SHIFT_FRW8_2ND);

                __m128i acc_7 = round2;
                acc_7 = _mm_add_epi32(acc_7, _mm_madd_epi16(tf_dct8_7_01, s01));
                acc_7 = _mm_add_epi32(acc_7, _mm_madd_epi16(tf_dct8_7_23, s23));
                acc_7 = _mm_add_epi32(acc_7, _mm_madd_epi16(tf_dct8_7_45, s45));
                acc_7 = _mm_add_epi32(acc_7, _mm_madd_epi16(tf_dct8_7_67, s67));
                acc_7 = _mm_srai_epi32(acc_7, SHIFT_FRW8_2ND);

                __m128i r67 = _mm_packs_epi32(acc_6, acc_7);
                _mm_storel_epi64((__m128i *)(dst + 6*coef_stride), r67);
                _mm_storeh_epi64((__m128i *)(dst + 7*coef_stride), r67);
            }
            p_tmp += 4;
            dst   += 4;
        }
    }

    void H265_FASTCALL MAKE_NAME(h265_DCT8x8Fwd_16s)(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth)
    {
        switch (bitDepth) {
        case  8: h265_DCT8x8Fwd_16s_Kernel<SHIFT_FRW8_1ST_BASE + 0>(src, srcStride, dst);  break;
        case  9: h265_DCT8x8Fwd_16s_Kernel<SHIFT_FRW8_1ST_BASE + 1>(src, srcStride, dst);  break;
        case 10: h265_DCT8x8Fwd_16s_Kernel<SHIFT_FRW8_1ST_BASE + 2>(src, srcStride, dst);  break;
        }
    }

#undef coef_stride

#define coef_stride 16
    template <int SHIFT_FRW16_1ST>
    static void h265_DCT16x16Fwd_16s_Kernel(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst)
    {
        M128I_W8C( tf_dct16_f0123_01,  90, 87,  87, 57,  80,  9,  70,-43);
        M128I_W8C( tf_dct16_f4567_01,  57,-80,  43,-90,  25,-70,   9,-25);
        M128I_W8C( tf_dct16_f0123_23,  80, 70,   9,-43, -70,-87, -87,  9);
        M128I_W8C( tf_dct16_f4567_23, -25, 90,  57, 25,  90,-80,  43,-57);
        M128I_W8C( tf_dct16_f0123_45,  57, 43, -80,-90, -25, 57,  90, 25);
        M128I_W8C( tf_dct16_f4567_45,  -9,-87, -87, 70,  43,  9,  70,-80);
        M128I_W8C( tf_dct16_f0123_67,  25,  9, -70,-25,  90, 43, -80,-57);
        M128I_W8C( tf_dct16_f4567_67,  43, 70,   9,-80, -57, 87,  87,-90);

        M128I_W8C( tf_dct16_e0123_01,  64, 64,  89, 75,  83, 36,  75,-18);
        M128I_W8C( tf_dct16_e4567_01,  64,-64,  50,-89,  36,-83,  18,-50);
        M128I_W8C( tf_dct16_e0123_23,  64, 64,  50, 18, -36,-83, -89,-50);
        M128I_W8C( tf_dct16_e4567_23, -64, 64,  18, 75,  83,-36,  75,-89);

        M128I_DC (round1, 1<<(SHIFT_FRW16_1ST-1));

        M128I_DC (c89, 89);
        M128I_DC (c_83, -83);
        M128I_DC (c75, 75);
        M128I_DC (c50, 50);
        M128I_DC (c36, 36);
        M128I_DC (c18, 18);

        M128I_DC (c90, 90);
        M128I_DC (c87, 87);
        M128I_DC (c80, 80);
        M128I_DC (c70, 70);
        M128I_DC (c57, 57);
        M128I_DC (c43, 43);
        M128I_DC (c25, 25);
        M128I_DC (c9, 9);

        M128I_DC (round2,  1<<(SHIFT_FRW16_2ND-1));
        M128I_DC (round2m, 1<<(SHIFT_FRW16_2ND-1-6));

        static const int tmp_stride = 16;
        ALIGNED_SSE2 short tmp[16 * 16];

        //partialButterfly16(src, tmp, 3, 16);

        short *H265_RESTRICT p_tmp = tmp;

        for(int i=0; i<16; ++i)
        {
            __m128i s07 = _mm_load_si128((const __m128i *)(src + i*srcStride));     // s7 s6 s5 s4 s3 s2 s1 s0
            __m128i s8F = _mm_load_si128((const __m128i *)(src + i*srcStride + 8)); // s15 s14 s13 s12 s11 s10 s9 s8

            s8F = _mm_shuffle_epi32(   s8F, _MM_SHUFFLE(1,0,3,2));
            s8F = _mm_shufflelo_epi16( s8F, _MM_SHUFFLE(0,1,2,3));
            s8F = _mm_shufflehi_epi16( s8F, _MM_SHUFFLE(0,1,2,3));
            // s07 = s(0,1,2,3,4,5,6,7), s8F = s(F,E,D,C,B,A,9,8)

            __m128i O0 = _mm_sub_epi16(s07, s8F);
            __m128i E0 = _mm_add_epi16(s07, s8F);

            // f0, f1 - register paar to accumulate the odd part of transform
            __m128i t0 = _mm_shuffle_epi32( O0, _MM_SHUFFLE(0,0,0,0));

            __m128i f0 = _mm_madd_epi16(t0, tf_dct16_f0123_01);
            __m128i f1 = _mm_madd_epi16(t0, tf_dct16_f4567_01);

            __m128i t1 = _mm_shuffle_epi32( O0, _MM_SHUFFLE(1,1,1,1));
            f0 = _mm_add_epi32(    f0, _mm_madd_epi16(    t1, tf_dct16_f0123_23));
            f1 = _mm_add_epi32(    f1, _mm_madd_epi16(    t1, tf_dct16_f4567_23));

            __m128i t2 = _mm_shuffle_epi32( O0, _MM_SHUFFLE(2,2,2,2));
            f0 = _mm_add_epi32(    f0, _mm_madd_epi16(    t2, tf_dct16_f0123_45));
            f1 = _mm_add_epi32(    f1, _mm_madd_epi16(    t2, tf_dct16_f4567_45));

            __m128i t3  = _mm_shuffle_epi32( O0, _MM_SHUFFLE(3,3,3,3));
            f0 = _mm_add_epi32(    f0, _mm_madd_epi16(    t3, tf_dct16_f0123_67));
            f1 = _mm_add_epi32(    f1, _mm_madd_epi16(    t3, tf_dct16_f4567_67));

            f0 = _mm_srai_epi32( _mm_add_epi32( f0, round1), SHIFT_FRW16_1ST);
            f1 = _mm_srai_epi32( _mm_add_epi32( f1, round1), SHIFT_FRW16_1ST);
            __m128i r_odd = _mm_packs_epi32( f0, f1);

            __m128i E0_B = _mm_shufflelo_epi16( _mm_shuffle_epi32( E0, _MM_SHUFFLE(1,0,3,2)), _MM_SHUFFLE(0,1,2,3));

            __m128i EE0 = _mm_add_epi16( E0, E0_B);
            __m128i EO0 = _mm_sub_epi16( E0, E0_B);

            __m128i cd = _mm_unpacklo_epi32 (EE0, EO0);// c0,c1, d0,d1, c2,c3, d2,d3

            __m128i cd1 = _mm_shuffle_epi32( cd, _MM_SHUFFLE(1,0,1,0));
            __m128i cd2 = _mm_shuffle_epi32( cd, _MM_SHUFFLE(3,2,3,2));

            __m128i g0 = _mm_madd_epi16( cd1, tf_dct16_e0123_01);
            __m128i g1 = _mm_madd_epi16( cd1, tf_dct16_e4567_01);

            g0 = _mm_add_epi32( g0, _mm_madd_epi16(    cd2, tf_dct16_e0123_23));
            g1 = _mm_add_epi32( g1, _mm_madd_epi16(    cd2, tf_dct16_e4567_23));

            g0 = _mm_srai_epi32( _mm_add_epi32( g0, round1), SHIFT_FRW16_1ST);
            g1 = _mm_srai_epi32( _mm_add_epi32( g1, round1), SHIFT_FRW16_1ST);
            __m128i r_even = _mm_packs_epi32( g0, g1);

            _mm_store_si128((__m128i *)(p_tmp),     _mm_unpacklo_epi16(r_even, r_odd));
            _mm_store_si128((__m128i *)(p_tmp + 8), _mm_unpackhi_epi16(r_even, r_odd));

            p_tmp += tmp_stride;
        }

        p_tmp = tmp;

        for (int i=0; i<16; i+=4) 
        {    
            __m128i sm, sp;
            /* E and O*/
            sm = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp +  0*tmp_stride)));
            sp = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp + 15*tmp_stride)));
            __m128i O0 = _mm_sub_epi32(sm, sp);
            __m128i E0 = _mm_add_epi32(sm, sp);

            /* E and O*/
            sm = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp + 7*tmp_stride)));
            sp = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp + 8*tmp_stride)));
            __m128i O7 = _mm_sub_epi32(sm, sp);
            __m128i E7 = _mm_add_epi32(sm, sp);

            /* EE and EO */
            __m128i EO0 = _mm_sub_epi32( E0, E7);
            __m128i EE0 = _mm_add_epi32( E0, E7);

            /* E and O*/
            sm = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp +  3*tmp_stride)));
            sp = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp + 12*tmp_stride)));
            __m128i O3 = _mm_sub_epi32(sm, sp);
            __m128i E3 = _mm_add_epi32(sm, sp);

            /* E and O*/
            sm = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp + 4*tmp_stride)));
            sp = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp + 11*tmp_stride)));
            __m128i O4 = _mm_sub_epi32(sm, sp);
            __m128i E4 = _mm_add_epi32(sm, sp);

            /* EE and EO */
            __m128i EO3 = _mm_sub_epi32( E3, E4);
            __m128i EE3 = _mm_add_epi32( E3, E4);

            /* EEE and EEO */
            __m128i EEE0 = _mm_add_epi32( EE0, EE3);    
            __m128i EEO0 = _mm_sub_epi32( EE0, EE3);


            /* E and O*/
            sm = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp +  1*tmp_stride)));
            sp = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp + 14*tmp_stride)));
            __m128i O1 = _mm_sub_epi32(sm, sp);
            __m128i E1 = _mm_add_epi32(sm, sp);

            /* E and O*/
            sm = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp +  6*tmp_stride)));
            sp = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp +  9*tmp_stride)));
            __m128i O6 = _mm_sub_epi32(sm, sp);
            __m128i E6 = _mm_add_epi32(sm, sp);

            /* EE and EO */
            __m128i EO1 = _mm_sub_epi32( E1, E6);
            __m128i EE1 = _mm_add_epi32( E1, E6);

            /* E and O*/
            sm = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp +  2*tmp_stride)));
            sp = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp + 13*tmp_stride)));
            __m128i O2 = _mm_sub_epi32(sm, sp);
            __m128i E2 = _mm_add_epi32(sm, sp);

            /* E and O*/
            sm = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp +  5*tmp_stride)));
            sp = _mm_cvtepi16_epi32(_mm_loadl_epi64((const __m128i *) (p_tmp + 10*tmp_stride)));
            __m128i O5 = _mm_sub_epi32(sm, sp);
            __m128i E5 = _mm_add_epi32(sm, sp);

            /* EE and EO */
            __m128i EO2 = _mm_sub_epi32( E2, E5);
            __m128i EE2 = _mm_add_epi32( E2, E5);

            /* EEE and EEO */
            __m128i EEO1 = _mm_sub_epi32( EE1, EE2);
            __m128i EEE1 = _mm_add_epi32( EE1, EE2);

            __m128i acc4  = _mm_mullo_epi32( EEO1, c36);
            __m128i acc12 = _mm_mullo_epi32( EEO1, c_83);
            __m128i acc0  = _mm_add_epi32( EEE0, EEE1);
            __m128i acc8  = _mm_sub_epi32( EEE0, EEE1);
            acc4  = _mm_sub_epi32( acc4,  _mm_mullo_epi32(EEO0, c_83));
            acc12 = _mm_add_epi32( acc12, _mm_mullo_epi32(EEO0, c36));
            acc0  = _mm_srai_epi32( _mm_add_epi32( acc0,  round2m), SHIFT_FRW16_2ND-6);
            acc8  = _mm_srai_epi32( _mm_add_epi32( acc8,  round2m), SHIFT_FRW16_2ND-6);
            acc4  = _mm_srai_epi32( _mm_add_epi32( acc4,  round2), SHIFT_FRW16_2ND);
            acc12 = _mm_srai_epi32( _mm_add_epi32( acc12, round2), SHIFT_FRW16_2ND);
            __m128i acc_0_8 = _mm_packs_epi32( acc0, acc8);
            __m128i acc_4_12 = _mm_packs_epi32( acc4, acc12);
            _mm_storel_epi64((__m128i *)(dst +  0*coef_stride), acc_0_8);
            _mm_storeh_epi64((__m128i *)(dst +  8*coef_stride), acc_0_8);
            _mm_storel_epi64((__m128i *)(dst +  4*coef_stride), acc_4_12);
            _mm_storeh_epi64((__m128i *)(dst + 12*coef_stride), acc_4_12);
            //      dst[ 0      ] = (64*EEE[0] + 64*EEE[1] + add)>>shift;
            //      dst[ 8*line ] = (64*EEE[0] - 64*EEE[1] + add)>>shift;
            //      dst[ 4*line ] = (83*EEO[0] + 36*EEO[1] + add)>>shift;        
            //      dst[ 12*line] = (36*EEO[0] - 83*EEO[1] + add)>>shift;


            __m128i acc2  = _mm_mullo_epi32( EO0, c89);
            __m128i acc6  = _mm_mullo_epi32( EO0, c75);
            __m128i acc10 = _mm_mullo_epi32( EO0, c50);
            __m128i acc14 = _mm_mullo_epi32( EO0, c18);
            acc2  = _mm_add_epi32( acc2,  _mm_mullo_epi32( EO1, c75));
            acc6  = _mm_sub_epi32( acc6,  _mm_mullo_epi32( EO1, c18));
            acc10 = _mm_sub_epi32( acc10, _mm_mullo_epi32( EO1, c89));
            acc14 = _mm_sub_epi32( acc14, _mm_mullo_epi32( EO1, c50));
            acc2  = _mm_add_epi32( acc2,  _mm_mullo_epi32( EO2, c50));
            acc6  = _mm_sub_epi32( acc6,  _mm_mullo_epi32( EO2, c89));
            acc10 = _mm_add_epi32( acc10, _mm_mullo_epi32( EO2, c18));
            acc14 = _mm_add_epi32( acc14, _mm_mullo_epi32( EO2, c75));
            acc2  = _mm_add_epi32( acc2,  _mm_mullo_epi32( EO3, c18));
            acc6  = _mm_sub_epi32( acc6,  _mm_mullo_epi32( EO3, c50));
            acc10 = _mm_add_epi32( acc10, _mm_mullo_epi32( EO3, c75));
            acc14 = _mm_sub_epi32( acc14, _mm_mullo_epi32( EO3, c89));
            acc2  = _mm_srai_epi32( _mm_add_epi32( acc2,  round2), SHIFT_FRW16_2ND);
            acc6  = _mm_srai_epi32( _mm_add_epi32( acc6,  round2), SHIFT_FRW16_2ND);
            acc10 = _mm_srai_epi32( _mm_add_epi32( acc10, round2), SHIFT_FRW16_2ND);
            acc14 = _mm_srai_epi32( _mm_add_epi32( acc14, round2), SHIFT_FRW16_2ND);
            __m128i acc_2_6   = _mm_packs_epi32( acc2,  acc6);
            __m128i acc_10_14 = _mm_packs_epi32( acc10, acc14);
            _mm_storel_epi64((__m128i *)(dst + 2*coef_stride),  acc_2_6);
            _mm_storeh_epi64((__m128i *)(dst + 6*coef_stride),  acc_2_6);
            _mm_storel_epi64((__m128i *)(dst + 10*coef_stride), acc_10_14);
            _mm_storeh_epi64((__m128i *)(dst + 14*coef_stride), acc_10_14);
            //      dst[ 2*line ] = (89*EO[0] +  75*EO[1] +  50*EO[2] +  18*EO[3] + add)>>shift;      
            //      dst[ 6*line ] = (75*EO[0] + -18*EO[1] + -89*EO[2] + -50*EO[3] + add)>>shift;      
            //      dst[10*line ] = (50*EO[0] + -89*EO[1] +  18*EO[2] +  75*EO[3] + add)>>shift;      
            //      dst[14*line ] = (18*EO[0] + -50*EO[1] +  75*EO[2] + -89*EO[3] + add)>>shift;      


            __m128i acc1 = _mm_mullo_epi32( O0, c90);
            __m128i acc3 = _mm_mullo_epi32( O0, c87);
            __m128i acc5 = _mm_mullo_epi32( O0, c80);
            __m128i acc7 = _mm_mullo_epi32( O0, c70);
            acc1 = _mm_add_epi32( acc1, _mm_mullo_epi32( O1, c87));
            acc3 = _mm_add_epi32( acc3, _mm_mullo_epi32( O1, c57));
            acc5 = _mm_add_epi32( acc5, _mm_mullo_epi32( O1, c9));
            acc7 = _mm_sub_epi32( acc7, _mm_mullo_epi32( O1, c43));
            acc1 = _mm_add_epi32( acc1, _mm_mullo_epi32( O2, c80));
            acc3 = _mm_add_epi32( acc3, _mm_mullo_epi32( O2, c9));
            acc5 = _mm_sub_epi32( acc5, _mm_mullo_epi32( O2, c70));
            acc7 = _mm_sub_epi32( acc7, _mm_mullo_epi32( O2, c87));
            acc1 = _mm_add_epi32( acc1, _mm_mullo_epi32( O3, c70));
            acc3 = _mm_sub_epi32( acc3, _mm_mullo_epi32( O3, c43));
            acc5 = _mm_sub_epi32( acc5, _mm_mullo_epi32( O3, c87));
            acc7 = _mm_add_epi32( acc7, _mm_mullo_epi32( O3, c9));
            acc1 = _mm_add_epi32( acc1, _mm_mullo_epi32( O4, c57));
            acc3 = _mm_sub_epi32( acc3, _mm_mullo_epi32( O4, c80));
            acc5 = _mm_sub_epi32( acc5, _mm_mullo_epi32( O4, c25));
            acc7 = _mm_add_epi32( acc7, _mm_mullo_epi32( O4, c90));
            acc1 = _mm_add_epi32( acc1, _mm_mullo_epi32( O5, c43));
            acc3 = _mm_sub_epi32( acc3, _mm_mullo_epi32( O5, c90));
            acc5 = _mm_add_epi32( acc5, _mm_mullo_epi32( O5, c57));
            acc7 = _mm_add_epi32( acc7, _mm_mullo_epi32( O5, c25));
            acc1 = _mm_add_epi32( acc1, _mm_mullo_epi32( O6, c25));
            acc3 = _mm_sub_epi32( acc3, _mm_mullo_epi32( O6, c70));
            acc5 = _mm_add_epi32( acc5, _mm_mullo_epi32( O6, c90));
            acc7 = _mm_sub_epi32( acc7, _mm_mullo_epi32( O6, c80));
            acc1 = _mm_add_epi32( acc1, _mm_mullo_epi32( O7, c9));
            acc3 = _mm_sub_epi32( acc3, _mm_mullo_epi32( O7, c25));
            acc5 = _mm_add_epi32( acc5, _mm_mullo_epi32( O7, c43));
            acc7 = _mm_sub_epi32( acc7, _mm_mullo_epi32( O7, c57));
            acc1 = _mm_srai_epi32( _mm_add_epi32( acc1, round2), SHIFT_FRW16_2ND);
            acc3 = _mm_srai_epi32( _mm_add_epi32( acc3, round2), SHIFT_FRW16_2ND);
            acc5 = _mm_srai_epi32( _mm_add_epi32( acc5, round2), SHIFT_FRW16_2ND);
            acc7 = _mm_srai_epi32( _mm_add_epi32( acc7, round2), SHIFT_FRW16_2ND);
            __m128i acc_1_3 = _mm_packs_epi32( acc1, acc3);
            __m128i acc_5_7 = _mm_packs_epi32( acc5, acc7);
            _mm_storel_epi64((__m128i *)(dst + 1*coef_stride), acc_1_3);
            _mm_storeh_epi64((__m128i *)(dst + 3*coef_stride), acc_1_3);
            _mm_storel_epi64((__m128i *)(dst + 5*coef_stride), acc_5_7);
            _mm_storeh_epi64((__m128i *)(dst + 7*coef_stride), acc_5_7);
            //        dst[ 1*line ] = (90*O[0] +  87*O[1] +  80*O[2] +  70*O[3] +  57*O[4] +  43*O[5] +  25*O[6] +   9*O[7] + add)>>shift;
            //        dst[ 3*line ] = (87*O[0] +  57*O[1] +   9*O[2] + -43*O[3] + -80*O[4] + -90*O[5] + -70*O[6] + -25*O[7] + add)>>shift;
            //        dst[ 5*line ] = (80*O[0] +   9*O[1] + -70*O[2] + -87*O[3] + -25*O[4] +  57*O[5] +  90*O[6] +  43*O[7] + add)>>shift;
            //        dst[ 7*line ] = (70*O[0] + -43*O[1] + -87*O[2] +   9*O[3] +  90*O[4] +  25*O[5] + -80*O[6] + -57*O[7] + add)>>shift;


            __m128i acc9  = _mm_mullo_epi32( O0, c57);
            __m128i acc11 = _mm_mullo_epi32( O0, c43);
            __m128i acc13 = _mm_mullo_epi32( O0, c25);
            __m128i acc15 = _mm_mullo_epi32( O0, c9);
            acc9  = _mm_sub_epi32( acc9,  _mm_mullo_epi32( O1, c80));
            acc11 = _mm_sub_epi32( acc11, _mm_mullo_epi32( O1, c90));
            acc13 = _mm_sub_epi32( acc13, _mm_mullo_epi32( O1, c70));
            acc15 = _mm_sub_epi32( acc15, _mm_mullo_epi32( O1, c25));
            acc9  = _mm_sub_epi32( acc9,  _mm_mullo_epi32( O2, c25));
            acc11 = _mm_add_epi32( acc11, _mm_mullo_epi32( O2, c57));
            acc13 = _mm_add_epi32( acc13, _mm_mullo_epi32( O2, c90));
            acc15 = _mm_add_epi32( acc15, _mm_mullo_epi32( O2, c43));
            acc9  = _mm_add_epi32( acc9,  _mm_mullo_epi32( O3, c90));
            acc11 = _mm_add_epi32( acc11, _mm_mullo_epi32( O3, c25));
            acc13 = _mm_sub_epi32( acc13, _mm_mullo_epi32( O3, c80));
            acc15 = _mm_sub_epi32( acc15, _mm_mullo_epi32( O3, c57));
            acc9  = _mm_sub_epi32( acc9,  _mm_mullo_epi32( O4, c9));
            acc11 = _mm_sub_epi32( acc11, _mm_mullo_epi32( O4, c87));
            acc13 = _mm_add_epi32( acc13, _mm_mullo_epi32( O4, c43));
            acc15 = _mm_add_epi32( acc15, _mm_mullo_epi32( O4, c70));
            acc9  = _mm_sub_epi32( acc9,  _mm_mullo_epi32( O5, c87));
            acc11 = _mm_add_epi32( acc11, _mm_mullo_epi32( O5, c70));
            acc13 = _mm_add_epi32( acc13, _mm_mullo_epi32( O5, c9));
            acc15 = _mm_sub_epi32( acc15, _mm_mullo_epi32( O5, c80));
            acc9  = _mm_add_epi32( acc9,  _mm_mullo_epi32( O6, c43));
            acc11 = _mm_add_epi32( acc11, _mm_mullo_epi32( O6, c9));
            acc13 = _mm_sub_epi32( acc13, _mm_mullo_epi32( O6, c57));
            acc15 = _mm_add_epi32( acc15, _mm_mullo_epi32( O6, c87));
            acc9  = _mm_add_epi32( acc9,  _mm_mullo_epi32( O7, c70));
            acc11 = _mm_sub_epi32( acc11, _mm_mullo_epi32( O7, c80));
            acc13 = _mm_add_epi32( acc13, _mm_mullo_epi32( O7, c87));
            acc15 = _mm_sub_epi32( acc15, _mm_mullo_epi32( O7, c90));
            acc9  = _mm_srai_epi32( _mm_add_epi32( acc9,  round2), SHIFT_FRW16_2ND);
            acc11 = _mm_srai_epi32( _mm_add_epi32( acc11, round2), SHIFT_FRW16_2ND);
            acc13 = _mm_srai_epi32( _mm_add_epi32( acc13, round2), SHIFT_FRW16_2ND);
            acc15 = _mm_srai_epi32( _mm_add_epi32( acc15, round2), SHIFT_FRW16_2ND);
            __m128i acc_9_11  = _mm_packs_epi32( acc9,  acc11);
            __m128i acc_13_15 = _mm_packs_epi32( acc13, acc15);
            _mm_storel_epi64((__m128i *)(dst +  9*coef_stride), acc_9_11);
            _mm_storeh_epi64((__m128i *)(dst + 11*coef_stride), acc_9_11);
            _mm_storel_epi64((__m128i *)(dst + 13*coef_stride), acc_13_15);
            _mm_storeh_epi64((__m128i *)(dst + 15*coef_stride), acc_13_15);
            //        dst[ 9*line ] = (57*O[0] + -80*O[1] + -25*O[2] +  90*O[3] +  -9*O[4] + -87*O[5] +  43*O[6] +  70*O[7] + add)>>shift;
            //        dst[11*line ] = (43*O[0] + -90*O[1] +  57*O[2] +  25*O[3] + -87*O[4] +  70*O[5] +   9*O[6] + -80*O[7] + add)>>shift;
            //        dst[13*line ] = (25*O[0] + -70*O[1] +  90*O[2] + -80*O[3] +  43*O[4] +   9*O[5] + -57*O[6] +  87*O[7] + add)>>shift;
            //        dst[15*line ] = ( 9*O[0] + -25*O[1] +  43*O[2] + -57*O[3] +  70*O[4] + -80*O[5] +  87*O[6] + -90*O[7] + add)>>shift;

            p_tmp += 4;
            dst   += 4; 
        }
    }

    void H265_FASTCALL MAKE_NAME(h265_DCT16x16Fwd_16s)(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth)
    {
        switch (bitDepth) {
        case  8: h265_DCT16x16Fwd_16s_Kernel<SHIFT_FRW16_1ST_BASE + 0>(src, srcStride, dst);   break;
        case  9: h265_DCT16x16Fwd_16s_Kernel<SHIFT_FRW16_1ST_BASE + 1>(src, srcStride, dst);   break;
        case 10: h265_DCT16x16Fwd_16s_Kernel<SHIFT_FRW16_1ST_BASE + 2>(src, srcStride, dst);   break;
        }
    }

#undef coef_stride
    // Load input data
    //#define _mm_load_128 _mm_load_si128  // movdqa - load aligned data
#define _mm_load_128 _mm_lddqu_si128   // lddqu  - load unaligned data

    // Load constants
    //#define _mm_load_const _mm_load_si128  
#define _mm_load_const(A) *(const __m128i *)(A) // 5621

#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))
#define _mm_storeh_epi64(p, A) _mm_storeh_pd((double *)(p), _mm_castsi128_pd(A))

    template <int SHIFT_FRW32_1ST>
    static void h265_DCT32x32Fwd_16s_Kernel(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dest)
    {
        short ALIGNED_SSE2 temp[32*32];
        // temporal buffer short[32*4]. Intermediate results will be stored here. Rotate 4x4 and moved to temp[]
        // Using this buffer will accelerate the functions: from 10000 CPU clocks to 8000
        __m128i ALIGNED_SSE2 buff[16];
        int num = 0;
        __m128i s0, s1, s2, s3, s4, s5, s6, s7;
        s4 = _mm_setzero_si128();
        s5 = _mm_setzero_si128();

        // Horizontal 1-D forward transform

#define tmp_stride  32  // stride in temp[] buffer

        short *dst = temp;

        for (int i=0; i<32; i++) 
        {
            /* Read input data (32 signed shorts)

            */
            s0 =  _mm_load_128((const __m128i *)(src + 8*0));
            s1 =  _mm_load_128((const __m128i *)(src + 8*1));
            s2 =  _mm_load_128((const __m128i *)(src + 8*2));
            s3 =  _mm_load_128((const __m128i *)(src + 8*3));
            /*
            ; Input:
            ; s0 = s07 s06 s05 s04 s03 s02 s01 s00
            ; s1 = s15 s14 s13 s12 s11 s10 s09 s08
            ; s2 = s23 s22 s21 s20 s19 s18 s17 s16
            ; s3 = s31 s30 s29 s28 s27 s26 s25 s24
            */
            s4 = _mm_load_const((const __m128i *)kfh_shuff);
            s2 = _mm_shuffle_epi8(s2, s4);      // s2 = s16 s17 s18 s19 s20 s21 s22 s23
            s3 = _mm_shuffle_epi8(s3, s4);      // s3 = s24 s25 s26 s27 s28 s29 s30 s31

            s0 = _mm_sub_epi16(s0, s3);
            s1 = _mm_sub_epi16(s1, s2);
            s3 = _mm_add_epi16(s3, s3);
            s2 = _mm_add_epi16(s2, s2);
            s3 = _mm_add_epi16(s3, s0);
            s2 = _mm_add_epi16(s2, s1);

            s4 = _mm_load_const((const __m128i *)kfh_0000);
            s5 = _mm_shuffle_epi32(s0, 0);
            s7 = _mm_load_const((const __m128i *)kfh_0001);
            s4 = _mm_madd_epi16(s4, s5);
            s5 = _mm_madd_epi16(s5, s7);

            s7 = _mm_load_const((const __m128i *)kfh_0002);
            s6 = _mm_shuffle_epi32(s0, 0x55);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0003);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0004);
            s6 = _mm_shuffle_epi32(s0, 0xAA);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0005);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0006);
            s6 = _mm_shuffle_epi32(s0, 0xFF);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0007);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0008);
            s6 = _mm_shuffle_epi32(s1, 0);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0009);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0010);
            s6 = _mm_shuffle_epi32(s1, 0x55);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0011);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0012);
            s6 = _mm_shuffle_epi32(s1, 0xAA);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0013);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0014);
            s6 = _mm_shuffle_epi32(s1, 0xFF);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0015);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            if (SHIFT_FRW32_1ST == 4) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_8));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_8));
            } else if (SHIFT_FRW32_1ST == 5) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_16));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_16));
            } else if (SHIFT_FRW32_1ST == 6) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_32));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_32));
            }
            s4 = _mm_srai_epi32(s4,  SHIFT_FRW32_1ST);   // (r03 r02 r01 r00 + add) >>= shift
            s5 = _mm_srai_epi32(s5,  SHIFT_FRW32_1ST);   // (r07 r06 r05 r04 + add) >>= shift
            s4 = _mm_packs_epi32(s4, s5);      // r07 r06 r05 r04 r03 r02 r01 r00: clip(-32768, 32767)

            // Store first results
            buff[num++] = s4;

            s4 = _mm_load_const((const __m128i *)kfh_0100);
            s5 = _mm_shuffle_epi32(s0, 0x00);
            s7 = _mm_load_const((const __m128i *)kfh_0101);
            s4 = _mm_madd_epi16(s4, s5);
            s5 = _mm_madd_epi16(s5, s7);

            s7 = _mm_load_const((const __m128i *)kfh_0102);
            s6 = _mm_shuffle_epi32(s0, 0x55);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0103);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0104);
            s6 = _mm_shuffle_epi32(s0, 0xAA);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0105);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0106);
            s6 = _mm_shuffle_epi32(s0, 0xFF);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0107);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0108);
            s6 = _mm_shuffle_epi32(s1, 0x00);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0109);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0110);
            s6 = _mm_shuffle_epi32(s1, 0x55);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0111);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0112);
            s6 = _mm_shuffle_epi32(s1, 0xAA);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0113);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0114);
            s6 = _mm_shuffle_epi32(s1, 0xFF);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0115);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            if (SHIFT_FRW32_1ST == 4) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_8));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_8));
            } else if (SHIFT_FRW32_1ST == 5) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_16));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_16));
            } else if (SHIFT_FRW32_1ST == 6) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_32));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_32));
            }
            s4 = _mm_srai_epi32(s4,  SHIFT_FRW32_1ST);   // (r11 r10 r09 r08 + add) >>= shift
            s5 = _mm_srai_epi32(s5,  SHIFT_FRW32_1ST);   // (r15 r14 r13 r12 + add) >>= shift
            s4 = _mm_packs_epi32(s4, s5);      // r15 r14 r13 r12 r11 r10 r09 r08: clip(-32768, 32767)

            // Store second results
            buff[num++] = s4;

            // 2nd part.
            s2 = _mm_shufflelo_epi16(s2, 0x1b); 
            s2 = _mm_shufflehi_epi16(s2, 0x1b); 
            s2 = _mm_shuffle_epi32(s2, 0x4e);   // a08 a09 a10 a11 a12 a13 a14 a15

            s3 = _mm_sub_epi16(s3,  s2);
            s2 = _mm_add_epi16(s2,  s2);
            s2 = _mm_add_epi16(s2,  s3);

            s4 = _mm_load_const((const __m128i *)kfh_0200);
            s5 = _mm_shuffle_epi32(s3, 0x00);
            s7 = _mm_load_const((const __m128i *)kfh_0201);
            s4 = _mm_madd_epi16(s4, s5);
            s5 = _mm_madd_epi16(s5, s7);

            s7 = _mm_load_const((const __m128i *)kfh_0202);
            s6 = _mm_shuffle_epi32(s3, 0x55);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0203);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0204);
            s6 = _mm_shuffle_epi32(s3, 0xAA);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0205);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            s7 = _mm_load_const((const __m128i *)kfh_0206);
            s6 = _mm_shuffle_epi32(s3, 0xFF);
            s7 = _mm_madd_epi16(s7, s6);
            s4 = _mm_add_epi32(s4,  s7);
            s7 = _mm_load_const((const __m128i *)kfh_0207);
            s6 = _mm_madd_epi16(s6, s7);
            s5 = _mm_add_epi32(s5,  s6);

            if (SHIFT_FRW32_1ST == 4) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_8));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_8));
            } else if (SHIFT_FRW32_1ST == 5) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_16));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_16));
            } else if (SHIFT_FRW32_1ST == 6) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_32));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_32));
            }
            s4 = _mm_srai_epi32(s4,  SHIFT_FRW32_1ST);   // (r03 r02 r01 r00 + add) >>= shift
            s5 = _mm_srai_epi32(s5,  SHIFT_FRW32_1ST);   // (r07 r06 r05 r04 + add) >>= shift
            s4 = _mm_packs_epi32(s4, s5);      // r07 r06 r05 r04 r03 r02 r01 r00

            // Store 3th results
            buff[num++] = s4;

            // 3rd part.
            s3 = _mm_shufflehi_epi16(s2, 0x1b); // c04 c05 c06 c07 c03 c02 c01 c00
            s3 = _mm_unpackhi_epi64(s3, s3);    //   x   x   x   x c04 c05 c06 c07

            s2 = _mm_sub_epi16(s2,  s3);
            s3 = _mm_add_epi16(s3,  s3);
            s3 = _mm_add_epi16(s3,  s2);

            s6 = _mm_load_const((const __m128i *)kfh_0300);
            s4 = _mm_shuffle_epi32(s2, 0x00);
            s7 = _mm_load_const((const __m128i *)kfh_0301);
            s5 = _mm_shuffle_epi32(s2, 0x55);
            s4 = _mm_madd_epi16(s4, s6);
            s5 = _mm_madd_epi16(s5, s7);
            s4 = _mm_add_epi32(s4,  s5);

            s6 = _mm_shufflelo_epi16(s3, 0x1b);

            s3 = _mm_sub_epi16(s3,  s6);
            s6 = _mm_add_epi16(s6,  s6);
            s6 = _mm_add_epi16(s6,  s3);

            s6 = _mm_unpacklo_epi32(s6, s3);
            s5 = _mm_load_const((const __m128i *)kfh_0400);
            s6 = _mm_unpacklo_epi32(s6, s6);
            s5 = _mm_madd_epi16(s5, s6);

            if (SHIFT_FRW32_1ST == 4) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_8));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_8));
            } else if (SHIFT_FRW32_1ST == 5) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_16));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_16));
            } else if (SHIFT_FRW32_1ST == 6) {
                s4 = _mm_add_epi32(s4,  _mm_load_const((const __m128i *)rounder_32));
                s5 = _mm_add_epi32(s5,  _mm_load_const((const __m128i *)rounder_32));
            }
            s4 = _mm_srai_epi32(s4,  SHIFT_FRW32_1ST);
            s5 = _mm_srai_epi32(s5,  SHIFT_FRW32_1ST);
            s4 = _mm_packs_epi32(s4, s5);

            // Store 4th results
            buff[num++] = s4;
            if (num == 16) {
                // load buffer short[8x4], rotate 2x4x4, save short[4x8]. Repeat 4 times
                num = 0;
                s0 =  _mm_load_128((const __m128i *)(buff + 0*4)); // 07 06 05 04 03 02 01 00
                s1 =  _mm_load_128((const __m128i *)(buff + 1*4)); // 17 16 15 14 13 12 11 10
                s2 =  _mm_load_128((const __m128i *)(buff + 2*4)); // 27 26 25 24 23 22 21 20
                s3 =  _mm_load_128((const __m128i *)(buff + 3*4)); // 37 36 35 34 33 32 31 30
                s6 = s0;
                s0 = _mm_unpacklo_epi16(s0, s1);  // 13 03 12 02 11 01 10 00
                s6 = _mm_unpackhi_epi16(s6, s1);  // 17 07 16 06 15 05 14 04
                s5 = s2;
                s4 = s0;
                s2 = _mm_unpacklo_epi16(s2, s3);  // 33 23 32 22 31 21 30 20
                s5 = _mm_unpackhi_epi16(s5, s3);  // 37 27 36 26 35 25 34 24
                s0 = _mm_unpacklo_epi32(s0, s2);  // 31 21 11 01 30 20 10 00
                _mm_storel_epi64((__m128i*)(&dst[ 1*tmp_stride]), s0);
                _mm_storeh_epi64((__m128i*)(&dst[ 3*tmp_stride]), s0);
                s7 = s6;
                s4 = _mm_unpackhi_epi32(s4, s2);  // 33 23 13 03 32 22 12 02
                _mm_storel_epi64((__m128i*)(&dst[ 5*tmp_stride]), s4);
                _mm_storeh_epi64((__m128i*)(&dst[ 7*tmp_stride]), s4);
                s6 = _mm_unpacklo_epi32(s6, s5);  // 35 25 15 05 34 24 14 04
                _mm_storel_epi64((__m128i*)(&dst[ 9*tmp_stride]), s6);
                _mm_storeh_epi64((__m128i*)(&dst[11*tmp_stride]), s6);
                s7 = _mm_unpackhi_epi32(s7, s5);  // 37 27 17 07 36 26 16 06
                _mm_storel_epi64((__m128i*)(&dst[13*tmp_stride]), s7);
                _mm_storeh_epi64((__m128i*)(&dst[15*tmp_stride]), s7);

                // 1th
                s0 =  _mm_load_128((const __m128i *)(buff + 0*4 + 1));
                s1 =  _mm_load_128((const __m128i *)(buff + 1*4 + 1));
                s2 =  _mm_load_128((const __m128i *)(buff + 2*4 + 1));
                s3 =  _mm_load_128((const __m128i *)(buff + 3*4 + 1));
                s6 = s0;
                s0 = _mm_unpacklo_epi16(s0, s1);
                s6 = _mm_unpackhi_epi16(s6, s1);
                s5 = s2;
                s4 = s0;
                s2 = _mm_unpacklo_epi16(s2, s3);
                s5 = _mm_unpackhi_epi16(s5, s3);
                s0 = _mm_unpacklo_epi32(s0, s2);
                _mm_storel_epi64((__m128i*)(&dst[17*tmp_stride]), s0);
                _mm_storeh_epi64((__m128i*)(&dst[19*tmp_stride]), s0);
                s7 = s6;
                s4 = _mm_unpackhi_epi32(s4, s2);
                _mm_storel_epi64((__m128i*)(&dst[21*tmp_stride]), s4);
                _mm_storeh_epi64((__m128i*)(&dst[23*tmp_stride]), s4);
                s6 = _mm_unpacklo_epi32(s6, s5);
                _mm_storel_epi64((__m128i*)(&dst[25*tmp_stride]), s6);
                _mm_storeh_epi64((__m128i*)(&dst[27*tmp_stride]), s6);
                s7 = _mm_unpackhi_epi32(s7, s5);
                _mm_storel_epi64((__m128i*)(&dst[29*tmp_stride]), s7);
                _mm_storeh_epi64((__m128i*)(&dst[31*tmp_stride]), s7);

                // 2th
                s0 =  _mm_load_128((const __m128i *)(buff + 0*4 + 2));
                s1 =  _mm_load_128((const __m128i *)(buff + 1*4 + 2));
                s2 =  _mm_load_128((const __m128i *)(buff + 2*4 + 2));
                s3 =  _mm_load_128((const __m128i *)(buff + 3*4 + 2));
                s6 = s0;
                s0 = _mm_unpacklo_epi16(s0, s1);
                s6 = _mm_unpackhi_epi16(s6, s1);
                s5 = s2;
                s4 = s0;
                s2 = _mm_unpacklo_epi16(s2, s3);
                s5 = _mm_unpackhi_epi16(s5, s3);
                s0 = _mm_unpacklo_epi32(s0, s2);
                _mm_storel_epi64((__m128i*)(&dst[ 2*tmp_stride]), s0);
                _mm_storeh_epi64((__m128i*)(&dst[ 6*tmp_stride]), s0);
                s7 = s6;
                s4 = _mm_unpackhi_epi32(s4, s2);
                _mm_storel_epi64((__m128i*)(&dst[10*tmp_stride]), s4);
                _mm_storeh_epi64((__m128i*)(&dst[14*tmp_stride]), s4);
                s6 = _mm_unpacklo_epi32(s6, s5);
                _mm_storel_epi64((__m128i*)(&dst[18*tmp_stride]), s6);
                _mm_storeh_epi64((__m128i*)(&dst[22*tmp_stride]), s6);
                s7 = _mm_unpackhi_epi32(s7, s5);
                _mm_storel_epi64((__m128i*)(&dst[26*tmp_stride]), s7);
                _mm_storeh_epi64((__m128i*)(&dst[30*tmp_stride]), s7);

                // 3th
                s0 =  _mm_load_128((const __m128i *)(buff + 0*4 + 3));
                s1 =  _mm_load_128((const __m128i *)(buff + 1*4 + 3));
                s2 =  _mm_load_128((const __m128i *)(buff + 2*4 + 3));
                s3 =  _mm_load_128((const __m128i *)(buff + 3*4 + 3));
                s6 = s0;
                s0 = _mm_unpacklo_epi16(s0, s1);
                s6 = _mm_unpackhi_epi16(s6, s1);
                s5 = s2;
                s4 = s0;
                s2 = _mm_unpacklo_epi16(s2, s3);
                s5 = _mm_unpackhi_epi16(s5, s3);
                s0 = _mm_unpacklo_epi32(s0, s2);
                _mm_storel_epi64((__m128i*)(&dst[ 4*tmp_stride]), s0);
                _mm_storeh_epi64((__m128i*)(&dst[12*tmp_stride]), s0);
                s7 = s6;
                s4 = _mm_unpackhi_epi32(s4, s2);
                _mm_storel_epi64((__m128i*)(&dst[20*tmp_stride]), s4);
                _mm_storeh_epi64((__m128i*)(&dst[28*tmp_stride]), s4);
                s6 = _mm_unpacklo_epi32(s6, s5);
                _mm_storel_epi64((__m128i*)(&dst[ 0*tmp_stride]), s6);
                _mm_storeh_epi64((__m128i*)(&dst[16*tmp_stride]), s6);
                s7 = _mm_unpackhi_epi32(s7, s5);
                _mm_storel_epi64((__m128i*)(&dst[ 8*tmp_stride]), s7);
                _mm_storeh_epi64((__m128i*)(&dst[24*tmp_stride]), s7);
                dst += 4;
            }

            src += srcStride;
        }

        // Vertical 1-D forward transform

#define shift       11
#define rounder     rounder_1k
#define dst_stride  32  // linear output buffer

        short *tmp = temp;

        for (int i=0; i<32; i++) 
        {
            s0 =  _mm_load_128((const __m128i *)(tmp + 8*0)); // s07 s06 s05 s04 s03 s02 s01 s00
            s1 =  _mm_load_128((const __m128i *)(tmp + 8*1)); // s15 s14 s13 s12 s11 s10 s09 s08
            s2 =  _mm_load_128((const __m128i *)(tmp + 8*2)); // s23 s22 s21 s20 s19 s18 s17 s16
            s3 =  _mm_load_128((const __m128i *)(tmp + 8*3)); // s31 s30 s29 s28 s27 s26 s25 s24

            s6 = _mm_load_const((const __m128i *)rounder);

            s2 = _mm_shuffle_epi32(s2, 0x4e);
            s3 = _mm_shuffle_epi32(s3, 0x4e);
            s2 = _mm_shufflelo_epi16(s2, 0x1b); 
            s3 = _mm_shufflelo_epi16(s3, 0x1b); 
            s2 = _mm_shufflehi_epi16(s2, 0x1b); // s16 s17 s18 s19 s20 s21 s22 s23
            s3 = _mm_shufflehi_epi16(s3, 0x1b); // s24 s25 s26 s27 s28 s29 s30 s31

            s4 = _mm_load_const((const __m128i *)kfv_0000);
            s7 = s6;
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0001);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0002);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_shuffle_epi32(s3, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0003);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0004);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_shuffle_epi32(s3, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0005);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0006);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_shuffle_epi32(s3, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0007);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0008);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_shuffle_epi32(s3, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0009);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0010);
            s1 = _mm_shuffle_epi32(s1, 0x39);
            s2 = _mm_shuffle_epi32(s2, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0011);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0012);
            s1 = _mm_shuffle_epi32(s1, 0x39);
            s2 = _mm_shuffle_epi32(s2, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0013);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0014);
            s1 = _mm_shuffle_epi32(s1, 0x39);
            s2 = _mm_shuffle_epi32(s2, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0015);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s1 = _mm_shuffle_epi32(s1, 0x39);
            s2 = _mm_shuffle_epi32(s2, 0x39);
            s6 = _mm_srai_epi32(s6,  shift);
            s7 = _mm_srai_epi32(s7,  shift);
            s6 = _mm_packs_epi32(s6, s7);

            // Store first results
            buff[num++] = s6;

            s7 = _mm_load_const((const __m128i *)rounder);
            s4 = _mm_load_const((const __m128i *)kfv_0100);
            s6 = s7;
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0101);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0102);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_shuffle_epi32(s3, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0103);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0104);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_shuffle_epi32(s3, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0105);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0106);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_shuffle_epi32(s3, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0107);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s0);
            s5 = _mm_madd_epi16(s5, s3);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0108);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_shuffle_epi32(s3, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0109);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0110);
            s1 = _mm_shuffle_epi32(s1, 0x39);
            s2 = _mm_shuffle_epi32(s2, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0111);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0112);
            s1 = _mm_shuffle_epi32(s1, 0x39);
            s2 = _mm_shuffle_epi32(s2, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s6 = _mm_add_epi32(s6,  s4);

            s4 = _mm_load_const((const __m128i *)kfv_0113);
            s6 = _mm_sub_epi32(s6,  s5);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0114);
            s1 = _mm_shuffle_epi32(s1, 0x39);
            s2 = _mm_shuffle_epi32(s2, 0x39);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s6 = _mm_add_epi32(s6,  s4);
            s6 = _mm_sub_epi32(s6,  s5);

            s4 = _mm_load_const((const __m128i *)kfv_0115);
            s5 = s4;
            s4 = _mm_madd_epi16(s4, s1);
            s5 = _mm_madd_epi16(s5, s2);
            s7 = _mm_add_epi32(s7,  s4);
            s7 = _mm_sub_epi32(s7,  s5);

            s1 = _mm_shuffle_epi32(s1, 0x39);
            s2 = _mm_shuffle_epi32(s2, 0x39);
            s6 = _mm_srai_epi32(s6,  shift);
            s7 = _mm_srai_epi32(s7,  shift);
            s6 = _mm_packs_epi32(s6, s7);

            // Store second results
            buff[num++] = s6;

            // 2nd part
            s4 = _mm_movehl_epi64(s4, s0);
            s5 = _mm_movehl_epi64(s5, s1);
            s6 = _mm_movehl_epi64(s6, s2);
            s7 = _mm_movehl_epi64(s7, s3);
            s0 = _mm_cvtepi16_epi32(s0); // 4x short->dword
            s4 = _mm_cvtepi16_epi32(s4);
            s1 = _mm_cvtepi16_epi32(s1);
            s5 = _mm_cvtepi16_epi32(s5);
            s3 = _mm_cvtepi16_epi32(s3);
            s7 = _mm_cvtepi16_epi32(s7);
            s2 = _mm_cvtepi16_epi32(s2);
            s6 = _mm_cvtepi16_epi32(s6);

            s0 = _mm_add_epi32(s0,  s3);
            s4 = _mm_add_epi32(s4,  s7);
            s1 = _mm_add_epi32(s1,  s2);
            s5 = _mm_add_epi32(s5,  s6);

            s1 = _mm_shuffle_epi32(s1, 0x1b);
            s5 = _mm_shuffle_epi32(s5, 0x1b);

            s6 = _mm_load_const((const __m128i *)rounder);
            s0 = _mm_sub_epi32(s0,  s5);
            s4 = _mm_sub_epi32(s4,  s1);
            s7 = s6;
            s5 = _mm_add_epi32(s5,  s5);
            s1 = _mm_add_epi32(s1,  s1);

            s2 = _mm_load_const((const __m128i *)kfv_0200);
            s5 = _mm_add_epi32(s5,  s0);
            s3 = _mm_load_const((const __m128i *)kfv_0201);
            s1 = _mm_add_epi32(s1,  s4);

            s2 = _mm_mullo_epi32(s2, s0);
            s3 = _mm_mullo_epi32(s3, s4);
            s6 = _mm_add_epi32(s6,  s2);

            s2 = _mm_load_const((const __m128i *)kfv_0202);
            s6 = _mm_add_epi32(s6,  s3);
            s3 = _mm_load_const((const __m128i *)kfv_0203);

            s2 = _mm_mullo_epi32(s2, s0);
            s3 = _mm_mullo_epi32(s3, s4);
            s7 = _mm_add_epi32(s7,  s2);
            s7 = _mm_add_epi32(s7,  s3);

            s2 = _mm_load_const((const __m128i *)kfv_0204);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_load_const((const __m128i *)kfv_0205);
            s4 = _mm_shuffle_epi32(s4, 0x39);

            s2 = _mm_mullo_epi32(s2, s0);
            s3 = _mm_mullo_epi32(s3, s4);
            s6 = _mm_add_epi32(s6,  s2);
            s2 = _mm_load_const((const __m128i *)kfv_0206);
            s6 = _mm_add_epi32(s6,  s3);
            s3 = _mm_load_const((const __m128i *)kfv_0207);

            s2 = _mm_mullo_epi32(s2, s0);
            s3 = _mm_mullo_epi32(s3, s4);
            s7 = _mm_add_epi32(s7,  s2);
            s7 = _mm_add_epi32(s7,  s3);

            s2 = _mm_load_const((const __m128i *)kfv_0208);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_load_const((const __m128i *)kfv_0209);
            s4 = _mm_shuffle_epi32(s4, 0x39);

            s2 = _mm_mullo_epi32(s2, s0);
            s3 = _mm_mullo_epi32(s3, s4);
            s6 = _mm_add_epi32(s6,  s2);

            s2 = _mm_load_const((const __m128i *)kfv_0210);
            s6 = _mm_add_epi32(s6,  s3);
            s3 = _mm_load_const((const __m128i *)kfv_0211);

            s2 = _mm_mullo_epi32(s2, s0);
            s3 = _mm_mullo_epi32(s3, s4);
            s7 = _mm_add_epi32(s7,  s2);
            s7 = _mm_add_epi32(s7,  s3);

            s2 = _mm_load_const((const __m128i *)kfv_0212);
            s0 = _mm_shuffle_epi32(s0, 0x39);
            s3 = _mm_load_const((const __m128i *)kfv_0213);
            s4 = _mm_shuffle_epi32(s4, 0x39);

            s2 = _mm_mullo_epi32(s2, s0);
            s3 = _mm_mullo_epi32(s3, s4);
            s6 = _mm_add_epi32(s6,  s2);

            s2 = _mm_load_const((const __m128i *)kfv_0214);
            s6 = _mm_add_epi32(s6,  s3);
            s3 = _mm_load_const((const __m128i *)kfv_0215);

            s2 = _mm_mullo_epi32(s2, s0);
            s3 = _mm_mullo_epi32(s3, s4);
            s7 = _mm_add_epi32(s7,  s2);
            s7 = _mm_add_epi32(s7,  s3);

            s6 = _mm_srai_epi32(s6,  shift);
            s7 = _mm_srai_epi32(s7,  shift);
            s6 = _mm_packs_epi32(s6, s7);

            // Store 3th results
            buff[num++] = s6;

            s1 = _mm_shuffle_epi32(s1, 0x1b);
            s6 = _mm_load_const((const __m128i *)rounder);
            s5 = _mm_sub_epi32(s5,  s1);
            s1 = _mm_add_epi32(s1,  s1);
            s1 = _mm_add_epi32(s1,  s5);

            s0 = _mm_load_const((const __m128i *)kfv_0300);
            s7 = s6;
            s4 = _mm_load_const((const __m128i *)kfv_0301);
            s0 = _mm_mullo_epi32(s0, s1);
            s4 = _mm_mullo_epi32(s4, s5);
            s6 = _mm_add_epi32(s6,  s0);
            s7 = _mm_add_epi32(s7,  s4);

            s0 = _mm_load_const((const __m128i *)kfv_0302);
            s1 = _mm_shuffle_epi32(s1, 0x39);
            s4 = _mm_load_const((const __m128i *)kfv_0303);
            s5 = _mm_shuffle_epi32(s5, 0x39);
            s0 = _mm_mullo_epi32(s0, s1);
            s4 = _mm_mullo_epi32(s4, s5);
            s6 = _mm_add_epi32(s6,  s0);
            s7 = _mm_add_epi32(s7,  s4);

            s0 = _mm_load_const((const __m128i *)kfv_0304);
            s1 = _mm_shuffle_epi32(s1, 0x39);
            s4 = _mm_load_const((const __m128i *)kfv_0305);
            s5 = _mm_shuffle_epi32(s5, 0x39);
            s0 = _mm_mullo_epi32(s0, s1);
            s4 = _mm_mullo_epi32(s4, s5);
            s6 = _mm_add_epi32(s6,  s0);
            s7 = _mm_add_epi32(s7,  s4);

            s0 = _mm_load_const((const __m128i *)kfv_0306);
            s1 = _mm_shuffle_epi32(s1, 0x39);
            s4 = _mm_load_const((const __m128i *)kfv_0307);
            s5 = _mm_shuffle_epi32(s5, 0x39);
            s0 = _mm_mullo_epi32(s0, s1);
            s4 = _mm_mullo_epi32(s4, s5);
            s6 = _mm_add_epi32(s6,  s0);
            s7 = _mm_add_epi32(s7,  s4);

            s6 = _mm_srai_epi32(s6,  shift);
            s7 = _mm_srai_epi32(s7,  shift);
            s6 = _mm_packs_epi32(s6, s7);

            // Store 4th results and rotate
            buff[num++] = s6;
            if (num == 16) {
                num = 0;
                s0 =  _mm_load_128((const __m128i *)(buff + 0*4));
                s1 =  _mm_load_128((const __m128i *)(buff + 1*4));
                s2 =  _mm_load_128((const __m128i *)(buff + 2*4));
                s3 =  _mm_load_128((const __m128i *)(buff + 3*4));
                s6 = s0;
                s0 = _mm_unpacklo_epi16(s0, s1);
                s6 = _mm_unpackhi_epi16(s6, s1);
                s5 = s2;
                s4 = s0;
                s2 = _mm_unpacklo_epi16(s2, s3);
                s5 = _mm_unpackhi_epi16(s5, s3);
                s0 = _mm_unpacklo_epi32(s0, s2);
                _mm_storel_epi64((__m128i*)(&dest[ 1*dst_stride]), s0);
                _mm_storeh_epi64((__m128i*)(&dest[ 3*dst_stride]), s0);
                s7 = s6;
                s4 = _mm_unpackhi_epi32(s4, s2);
                _mm_storel_epi64((__m128i*)(&dest[ 5*dst_stride]), s4);
                _mm_storeh_epi64((__m128i*)(&dest[ 7*dst_stride]), s4);
                s6 = _mm_unpacklo_epi32(s6, s5);
                _mm_storel_epi64((__m128i*)(&dest[ 9*dst_stride]), s6);
                _mm_storeh_epi64((__m128i*)(&dest[11*dst_stride]), s6);
                s7 = _mm_unpackhi_epi32(s7, s5);
                _mm_storel_epi64((__m128i*)(&dest[13*dst_stride]), s7);
                _mm_storeh_epi64((__m128i*)(&dest[15*dst_stride]), s7);

                // 1th
                s0 =  _mm_load_128((const __m128i *)(buff + 0*4 + 1));
                s1 =  _mm_load_128((const __m128i *)(buff + 1*4 + 1));
                s2 =  _mm_load_128((const __m128i *)(buff + 2*4 + 1));
                s3 =  _mm_load_128((const __m128i *)(buff + 3*4 + 1));
                s6 = s0;
                s0 = _mm_unpacklo_epi16(s0, s1);
                s6 = _mm_unpackhi_epi16(s6, s1);
                s5 = s2;
                s4 = s0;
                s2 = _mm_unpacklo_epi16(s2, s3);
                s5 = _mm_unpackhi_epi16(s5, s3);
                s0 = _mm_unpacklo_epi32(s0, s2);
                _mm_storel_epi64((__m128i*)(&dest[17*dst_stride]), s0);
                _mm_storeh_epi64((__m128i*)(&dest[19*dst_stride]), s0);
                s7 = s6;
                s4 = _mm_unpackhi_epi32(s4, s2);
                _mm_storel_epi64((__m128i*)(&dest[21*dst_stride]), s4);
                _mm_storeh_epi64((__m128i*)(&dest[23*dst_stride]), s4);
                s6 = _mm_unpacklo_epi32(s6, s5);
                _mm_storel_epi64((__m128i*)(&dest[25*dst_stride]), s6);
                _mm_storeh_epi64((__m128i*)(&dest[27*dst_stride]), s6);
                s7 = _mm_unpackhi_epi32(s7, s5);
                _mm_storel_epi64((__m128i*)(&dest[29*dst_stride]), s7);
                _mm_storeh_epi64((__m128i*)(&dest[31*dst_stride]), s7);

                // 2th
                s0 =  _mm_load_128((const __m128i *)(buff + 0*4 + 2));
                s1 =  _mm_load_128((const __m128i *)(buff + 1*4 + 2));
                s2 =  _mm_load_128((const __m128i *)(buff + 2*4 + 2));
                s3 =  _mm_load_128((const __m128i *)(buff + 3*4 + 2));
                s6 = s0;
                s0 = _mm_unpacklo_epi16(s0, s1);
                s6 = _mm_unpackhi_epi16(s6, s1);
                s5 = s2;
                s4 = s0;
                s2 = _mm_unpacklo_epi16(s2, s3);
                s5 = _mm_unpackhi_epi16(s5, s3);
                s0 = _mm_unpacklo_epi32(s0, s2);
                _mm_storel_epi64((__m128i*)(&dest[ 2*dst_stride]), s0);
                _mm_storeh_epi64((__m128i*)(&dest[ 6*dst_stride]), s0);
                s7 = s6;
                s4 = _mm_unpackhi_epi32(s4, s2);
                _mm_storel_epi64((__m128i*)(&dest[10*dst_stride]), s4);
                _mm_storeh_epi64((__m128i*)(&dest[14*dst_stride]), s4);
                s6 = _mm_unpacklo_epi32(s6, s5);
                _mm_storel_epi64((__m128i*)(&dest[18*dst_stride]), s6);
                _mm_storeh_epi64((__m128i*)(&dest[22*dst_stride]), s6);
                s7 = _mm_unpackhi_epi32(s7, s5);
                _mm_storel_epi64((__m128i*)(&dest[26*dst_stride]), s7);
                _mm_storeh_epi64((__m128i*)(&dest[30*dst_stride]), s7);

                // 3th
                s0 =  _mm_load_128((const __m128i *)(buff + 0*4 + 3));
                s1 =  _mm_load_128((const __m128i *)(buff + 1*4 + 3));
                s2 =  _mm_load_128((const __m128i *)(buff + 2*4 + 3));
                s3 =  _mm_load_128((const __m128i *)(buff + 3*4 + 3));
                s6 = s0;
                s0 = _mm_unpacklo_epi16(s0, s1);
                s6 = _mm_unpackhi_epi16(s6, s1);
                s5 = s2;
                s4 = s0;
                s2 = _mm_unpacklo_epi16(s2, s3);
                s5 = _mm_unpackhi_epi16(s5, s3);
                s0 = _mm_unpacklo_epi32(s0, s2);
                _mm_storel_epi64((__m128i*)(&dest[ 0*dst_stride]), s0);
                _mm_storeh_epi64((__m128i*)(&dest[ 8*dst_stride]), s0);
                s7 = s6;
                s4 = _mm_unpackhi_epi32(s4, s2);
                _mm_storel_epi64((__m128i*)(&dest[16*dst_stride]), s4);
                _mm_storeh_epi64((__m128i*)(&dest[24*dst_stride]), s4);
                s6 = _mm_unpacklo_epi32(s6, s5);
                _mm_storel_epi64((__m128i*)(&dest[ 4*dst_stride]), s6);
                _mm_storeh_epi64((__m128i*)(&dest[12*dst_stride]), s6);
                s7 = _mm_unpackhi_epi32(s7, s5);
                _mm_storel_epi64((__m128i*)(&dest[20*dst_stride]), s7);
                _mm_storeh_epi64((__m128i*)(&dest[28*dst_stride]), s7);
                dest += 4;
            }
            tmp += tmp_stride;
        }
    }

    void H265_FASTCALL MAKE_NAME(h265_DCT32x32Fwd_16s)(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth)
    {
        switch (bitDepth) {
        case  8: h265_DCT32x32Fwd_16s_Kernel<SHIFT_FRW32_1ST_BASE + 0>(src, srcStride, dst);   break;
        case  9: h265_DCT32x32Fwd_16s_Kernel<SHIFT_FRW32_1ST_BASE + 1>(src, srcStride, dst);   break;
        case 10: h265_DCT32x32Fwd_16s_Kernel<SHIFT_FRW32_1ST_BASE + 2>(src, srcStride, dst);   break;
        }
    }

} // end namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...
#endif // #if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */
