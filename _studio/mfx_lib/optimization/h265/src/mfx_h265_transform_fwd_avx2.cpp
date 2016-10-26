//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

// Forward HEVC Transform functions optimised by intrinsics
// Sizes: 4, 8, 16, 32
//
// NOTES:
// - requires AVX2
// - to avoid SSE/AVX transition penalties, ensure that AVX2 code is wrapped with _mm256_zeroupper() 
//     (include explicitly on function entry, ICL should automatically add before function return)

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"
#include "mfx_h265_transform_consts.h"

#if defined (MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO)

//#define AVX2_FTR_32x32_V1_ENABLED

namespace MFX_HEVC_PP
{

#include <immintrin.h> // AVX, AVX2

/* utility macros from original Nablet code */
#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */
#define _mm_storeh_epi64(p, A) _mm_storeh_pd((double *)(p), _mm_castsi128_pd(A))

#define SHIFT_FRW4_1ST_BASE    1
#define SHIFT_FRW4_2ND    8

/* DST 4x4 */
ALIGN_DECL(32) static const short DST4Tab[6][16] = {
    {  29,  55,  74,  74,  84, -29,  55, -84,  29,  55,  74,  74,  84, -29,  55, -84 },     // tf_dst4_A
    {  74,  84,   0, -74, -74,  55,  74, -29,  74,  84,   0, -74, -74,  55,  74, -29 },     // tf_dst4_B

    {  29,  55,  29,  55,  29,  55,  29,  55,  74,  74,  74,  74,  74,  74,  74,  74 },     // tf_4dst_0_01  tf_4dst_1_01
    {  84, -29,  84, -29,  84, -29,  84, -29,  55, -84,  55, -84,  55, -84,  55, -84 },     // tf_4dst_2_01  tf_4dst_3_01
    {  74,  84,  74,  84,  74,  84,  74,  84,   0, -74,   0, -74,   0, -74,   0, -74 },     // tf_4dst_0_23  tf_4dst_1_23
    { -74,  55, -74,  55, -74,  55, -74,  55,  74, -29,  74, -29,  74, -29,  74, -29 },     // tf_4dst_2_23  tf_4dst_3_23
};

/* DCT 4x4 */
ALIGN_DECL(32) static const short DCT4Tab[6][16] = {
    {  64,  64,  83,  36,  64, -64,  36, -83,  64,  64,  83,  36,  64, -64,  36, -83 },     // tf_dct4_A
    {  64,  64, -36, -83, -64,  64,  83, -36,  64,  64, -36, -83, -64,  64,  83, -36 },     // tf_dct4_B

    {  64,  64,  64,  64,  64,  64,  64,  64,  83,  36,  83,  36,  83,  36,  83,  36 },     // tf_4dct_0_01  tf_4dct_1_01
    {  64, -64,  64, -64,  64, -64,  64, -64,  36, -83,  36, -83,  36, -83,  36, -83 },     // tf_4dct_2_01  tf_4dct_3_01
    {  64,  64,  64,  64,  64,  64,  64,  64, -36, -83, -36, -83, -36, -83, -36, -83 },     // tf_4dct_0_23  tf_4dct_1_23
    { -64,  64, -64,  64, -64,  64, -64,  64,  83, -36,  83, -36,  83, -36,  83, -36 },     // tf_4dct_2_23  tf_4dct_3_23
};

/* common kernel for DST/DCT, only difference is coefficient table */
template <int SHIFT_FRW4_1ST>
static __inline void h265_FwdKernel4x4(const short *H265_RESTRICT src, Ipp32s srcStride, short *H265_RESTRICT dst, int dctFlag)
{
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, round1, round2;
    __m256i *coefTab;

    _mm256_zeroupper();

    round1 = _mm256_set1_epi32(1 << (SHIFT_FRW4_1ST - 1));
    round2 = _mm256_set1_epi32(1 << (SHIFT_FRW4_2ND - 1));

    if (dctFlag)
        coefTab = (__m256i *)DCT4Tab;
    else
        coefTab = (__m256i *)DST4Tab;

    Ipp64u row0 = *(const Ipp64u *)(src + 0*srcStride);
    Ipp64u row1 = *(const Ipp64u *)(src + 1*srcStride);
    Ipp64u row2 = *(const Ipp64u *)(src + 2*srcStride);
    Ipp64u row3 = *(const Ipp64u *)(src + 3*srcStride);

    /* first 2 rows */
    ymm2 = _mm256_set_epi64x(row1, row1, row0, row0);   // [0 | 0 | 1 | 1]
    ymm0 = _mm256_shuffle_epi32(ymm2, 0x00);            // [r0-01 | r0-01 | r0-01 | r0-01 | r1-01 | r1-01 | r1-01 | r1-01]
    ymm2 = _mm256_shuffle_epi32(ymm2, 0x55);            // [r0-23 | r0-23 | r0-23 | r0-23 | r1-23 | r1-23 | r1-23 | r1-23]

    ymm0 = _mm256_madd_epi16(ymm0, coefTab[0]);
    ymm2 = _mm256_madd_epi16(ymm2, coefTab[1]);

    ymm0 = _mm256_add_epi32(ymm0, ymm2);
    ymm0 = _mm256_add_epi32(ymm0, round1);
    ymm0 = _mm256_srai_epi32(ymm0, SHIFT_FRW4_1ST);

    /* second 2 rows */
    ymm2 = _mm256_set_epi64x(row3, row3, row2, row2);   // [2 | 2 | 3 | 3]
    ymm1 = _mm256_shuffle_epi32(ymm2, 0x00);            // [r2-01 | r2-01 | r2-01 | r2-01 | r1-01 | r1-01 | r1-01 | r1-01]
    ymm2 = _mm256_shuffle_epi32(ymm2, 0x55);            // [r2-23 | r2-23 | r2-23 | r2-23 | r1-23 | r1-23 | r1-23 | r1-23]

    ymm1 = _mm256_madd_epi16(ymm1, coefTab[0]);
    ymm2 = _mm256_madd_epi16(ymm2, coefTab[1]);

    ymm1 = _mm256_add_epi32(ymm1, ymm2);
    ymm1 = _mm256_add_epi32(ymm1, round1);
    ymm1 = _mm256_srai_epi32(ymm1, SHIFT_FRW4_1ST);

    /* stage 2 */
    ymm0 = _mm256_packs_epi32(ymm0, ymm1);
    ymm1 = _mm256_permute4x64_epi64(ymm0, 0xee);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0x44);

    ymm3 = _mm256_unpackhi_epi16(ymm0, ymm1);
    ymm1 = _mm256_unpacklo_epi16(ymm0, ymm1);

    ymm0 = _mm256_madd_epi16(ymm1, coefTab[2]);   // [xmm0 | xmm1]
    ymm1 = _mm256_madd_epi16(ymm1, coefTab[3]);   // [xmm2 | xmm3]
    ymm2 = _mm256_madd_epi16(ymm3, coefTab[4]);   // [xmm4 | xmm5]
    ymm3 = _mm256_madd_epi16(ymm3, coefTab[5]);   // [xmm6 | xmm7]

    ymm0 = _mm256_add_epi32(ymm0, ymm2);
    ymm2 = _mm256_add_epi32(ymm1, ymm3);

    ymm0 = _mm256_add_epi32(ymm0, round2);
    ymm2 = _mm256_add_epi32(ymm2, round2);

    ymm0 = _mm256_srai_epi32(ymm0, SHIFT_FRW4_2ND);
    ymm2 = _mm256_srai_epi32(ymm2, SHIFT_FRW4_2ND);

    ymm0 = _mm256_packs_epi32(ymm0, ymm2);
    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
    
    _mm256_storeu_si256((__m256i *)dst, ymm0);
}

void H265_FASTCALL MAKE_NAME(h265_DST4x4Fwd_16s)(const short *H265_RESTRICT src, Ipp32s srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth)
{
    switch (bitDepth) {
    case  8: h265_FwdKernel4x4<SHIFT_FRW4_1ST_BASE + 0>(src, srcStride, dst, 0);   break;
    case  9: h265_FwdKernel4x4<SHIFT_FRW4_1ST_BASE + 1>(src, srcStride, dst, 0);   break;
    case 10: h265_FwdKernel4x4<SHIFT_FRW4_1ST_BASE + 2>(src, srcStride, dst, 0);   break;
    }
}

void H265_FASTCALL MAKE_NAME(h265_DCT4x4Fwd_16s)(const short *H265_RESTRICT src, Ipp32s srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth)
{
    switch (bitDepth) {
    case  8: h265_FwdKernel4x4<SHIFT_FRW4_1ST_BASE + 0>(src, srcStride, dst, 1);   break;
    case  9: h265_FwdKernel4x4<SHIFT_FRW4_1ST_BASE + 1>(src, srcStride, dst, 1);   break;
    case 10: h265_FwdKernel4x4<SHIFT_FRW4_1ST_BASE + 2>(src, srcStride, dst, 1);   break;
    }
}

#define SHIFT_FRW8_1ST_BASE    2
#define SHIFT_FRW8_2ND    9

/* DCT 8x8 */
ALIGN_DECL(32) static const short DCT8Tab[4*(1+8)][16] = {
    {  64,  64,  50,  18, -83, -36,  18, -75,  64, -64,  18,  75, -36,  83,  50, -18 },
    {  64,  64, -18, -50,  36,  83,  75, -18, -64,  64, -75, -18, -83,  36,  18, -50 },
    {  64,  64, -75, -89,  83,  36, -89, -50,  64, -64,  89, -50,  36, -83,  75, -89 },
    {  64,  64,  89,  75, -36, -83,  50,  89, -64,  64,  50, -89,  83, -36,  89, -75 },

    {  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64 },  
    {  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64 },  
    {  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64 },  
    {  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64 },

    {  89,  75,  89,  75,  89,  75,  89,  75,  89,  75,  89,  75,  89,  75,  89,  75 }, 
    {  50,  18,  50,  18,  50,  18,  50,  18,  50,  18,  50,  18,  50,  18,  50,  18 },  
    { -18, -50, -18, -50, -18, -50, -18, -50, -18, -50, -18, -50, -18, -50, -18, -50 },  
    { -75, -89, -75, -89, -75, -89, -75, -89, -75, -89, -75, -89, -75, -89, -75, -89 },

    {  83,  36,  83,  36,  83,  36,  83,  36,  83,  36,  83,  36,  83,  36,  83,  36 },  
    { -36, -83, -36, -83, -36, -83, -36, -83, -36, -83, -36, -83, -36, -83, -36, -83 },  
    { -83, -36, -83, -36, -83, -36, -83, -36, -83, -36, -83, -36, -83, -36, -83, -36 },  
    {  36,  83,  36,  83,  36,  83,  36,  83,  36,  83,  36,  83,  36,  83,  36,  83 },

    {  75, -18,  75, -18,  75, -18,  75, -18,  75, -18,  75, -18,  75, -18,  75, -18 },  
    { -89, -50, -89, -50, -89, -50, -89, -50, -89, -50, -89, -50, -89, -50, -89, -50 },  
    {  50,  89,  50,  89,  50,  89,  50,  89,  50,  89,  50,  89,  50,  89,  50,  89 },  
    {  18, -75,  18, -75,  18, -75,  18, -75,  18, -75,  18, -75,  18, -75,  18, -75 },

    {  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64 },  
    { -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64 },  
    {  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64 },  
    { -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64, -64,  64 },

    {  50, -89,  50, -89,  50, -89,  50, -89,  50, -89,  50, -89,  50, -89,  50, -89 },  
    {  18,  75,  18,  75,  18,  75,  18,  75,  18,  75,  18,  75,  18,  75,  18,  75 },  
    { -75, -18, -75, -18, -75, -18, -75, -18, -75, -18, -75, -18, -75, -18, -75, -18 },  
    {  89, -50,  89, -50,  89, -50,  89, -50,  89, -50,  89, -50,  89, -50,  89, -50 },

    {  36, -83,  36, -83,  36, -83,  36, -83,  36, -83,  36, -83,  36, -83,  36, -83 },  
    {  83, -36,  83, -36,  83, -36,  83, -36,  83, -36,  83, -36,  83, -36,  83, -36 },  
    { -36,  83, -36,  83, -36,  83, -36,  83, -36,  83, -36,  83, -36,  83, -36,  83 },  
    { -83,  36, -83,  36, -83,  36, -83,  36, -83,  36, -83,  36, -83,  36, -83,  36 },

    {  18, -50,  18, -50,  18, -50,  18, -50,  18, -50,  18, -50,  18, -50,  18, -50 },  
    {  75, -89,  75, -89,  75, -89,  75, -89,  75, -89,  75, -89,  75, -89,  75, -89 },  
    {  89, -75,  89, -75,  89, -75,  89, -75,  89, -75,  89, -75,  89, -75,  89, -75 },  
    {  50, -18,  50, -18,  50, -18,  50, -18,  50, -18,  50, -18,  50, -18,  50, -18 },
};

template <int SHIFT_FRW8_1ST>
static void h265_DCT8x8Fwd_16s_Kernel(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst)
{
    int i;
    ALIGN_DECL(32) short tmp[8 * 8];

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, round1, round2;

    _mm256_zeroupper();

    round1 = _mm256_set1_epi32(1 << (SHIFT_FRW8_1ST - 1));
    round2 = _mm256_set1_epi32(1 << (SHIFT_FRW8_2ND - 1));

    for (i = 0; i < 8; i++) {
        ymm0 = round1;

        ymm1 = mm256(_mm_loadu_si128((__m128i *)(src + i*srcStride)));  // load 8 16-bit pixels (one row)
        ymm1 = _mm256_permute2x128_si256(ymm1, ymm1, 0x00);
        ymm2 = _mm256_shuffle_epi32(ymm1, 0x39);
        ymm3 = _mm256_shuffle_epi32(ymm1, 0x4e);
        ymm4 = _mm256_shuffle_epi32(ymm1, 0x93);

        ymm1 = _mm256_madd_epi16(ymm1, *(__m256i *)DCT8Tab[0]);
        ymm2 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT8Tab[1]);
        ymm3 = _mm256_madd_epi16(ymm3, *(__m256i *)DCT8Tab[2]);
        ymm4 = _mm256_madd_epi16(ymm4, *(__m256i *)DCT8Tab[3]);

        ymm0 = _mm256_add_epi32(ymm0, ymm1);
        ymm0 = _mm256_add_epi32(ymm0, ymm2);
        ymm0 = _mm256_add_epi32(ymm0, ymm3);
        ymm0 = _mm256_add_epi32(ymm0, ymm4);

        ymm0 = _mm256_srai_epi32(ymm0, SHIFT_FRW8_1ST);
        ymm0 = _mm256_packs_epi32(ymm0, ymm0);
        ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);

        _mm_store_si128((__m128i *)(tmp + i*8), mm128(ymm0));   // (tmp + i*8) should always be 16-byte aligned
    }

    /* load 2 rows at a time from temp buffer, permute qwords into low/high parts for interleaving */
    ymm4 = _mm256_load_si256((__m256i *)(tmp + 0*8));
    ymm0 = _mm256_permute4x64_epi64(ymm4, 0x10);
    ymm4 = _mm256_permute4x64_epi64(ymm4, 0x32);

    ymm5 = _mm256_load_si256((__m256i *)(tmp + 2*8));
    ymm1 = _mm256_permute4x64_epi64(ymm5, 0x10);
    ymm5 = _mm256_permute4x64_epi64(ymm5, 0x32);

    ymm6 = _mm256_load_si256((__m256i *)(tmp + 4*8));
    ymm2 = _mm256_permute4x64_epi64(ymm6, 0x10);
    ymm6 = _mm256_permute4x64_epi64(ymm6, 0x32);

    ymm7 = _mm256_load_si256((__m256i *)(tmp + 6*8));
    ymm3 = _mm256_permute4x64_epi64(ymm7, 0x10);
    ymm7 = _mm256_permute4x64_epi64(ymm7, 0x32);

    ymm0 = _mm256_unpacklo_epi16(ymm0, ymm4);
    ymm1 = _mm256_unpacklo_epi16(ymm1, ymm5);
    ymm2 = _mm256_unpacklo_epi16(ymm2, ymm6);
    ymm3 = _mm256_unpacklo_epi16(ymm3, ymm7);

    /* check compiler output - ICL should be able to avoid stack spills here, even without specifying temp register for result of pmaddwd */
    ymm4 = round2;
    ymm4 = _mm256_add_epi32(ymm4, _mm256_madd_epi16(*(__m256i *)DCT8Tab[ 4], ymm0));
    ymm4 = _mm256_add_epi32(ymm4, _mm256_madd_epi16(*(__m256i *)DCT8Tab[ 5], ymm1));
    ymm4 = _mm256_add_epi32(ymm4, _mm256_madd_epi16(*(__m256i *)DCT8Tab[ 6], ymm2));
    ymm4 = _mm256_add_epi32(ymm4, _mm256_madd_epi16(*(__m256i *)DCT8Tab[ 7], ymm3));
    ymm4 = _mm256_srai_epi32(ymm4, SHIFT_FRW8_2ND);

    ymm5 = round2;
    ymm5 = _mm256_add_epi32(ymm5, _mm256_madd_epi16(*(__m256i *)DCT8Tab[ 8], ymm0));
    ymm5 = _mm256_add_epi32(ymm5, _mm256_madd_epi16(*(__m256i *)DCT8Tab[ 9], ymm1));
    ymm5 = _mm256_add_epi32(ymm5, _mm256_madd_epi16(*(__m256i *)DCT8Tab[10], ymm2));
    ymm5 = _mm256_add_epi32(ymm5, _mm256_madd_epi16(*(__m256i *)DCT8Tab[11], ymm3));
    ymm5 = _mm256_srai_epi32(ymm5, SHIFT_FRW8_2ND);

    ymm4 = _mm256_packs_epi32(ymm4, ymm5);
    ymm4 = _mm256_permute4x64_epi64(ymm4, 0xd8);
    _mm256_storeu_si256((__m256i *)(dst + 0*8), ymm4);

    ymm6 = round2;
    ymm6 = _mm256_add_epi32(ymm6, _mm256_madd_epi16(*(__m256i *)DCT8Tab[12], ymm0));
    ymm6 = _mm256_add_epi32(ymm6, _mm256_madd_epi16(*(__m256i *)DCT8Tab[13], ymm1));
    ymm6 = _mm256_add_epi32(ymm6, _mm256_madd_epi16(*(__m256i *)DCT8Tab[14], ymm2));
    ymm6 = _mm256_add_epi32(ymm6, _mm256_madd_epi16(*(__m256i *)DCT8Tab[15], ymm3));
    ymm6 = _mm256_srai_epi32(ymm6, SHIFT_FRW8_2ND);

    ymm7 = round2;
    ymm7 = _mm256_add_epi32(ymm7, _mm256_madd_epi16(*(__m256i *)DCT8Tab[16], ymm0));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_madd_epi16(*(__m256i *)DCT8Tab[17], ymm1));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_madd_epi16(*(__m256i *)DCT8Tab[18], ymm2));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_madd_epi16(*(__m256i *)DCT8Tab[19], ymm3));
    ymm7 = _mm256_srai_epi32(ymm7, SHIFT_FRW8_2ND);

    ymm6 = _mm256_packs_epi32(ymm6, ymm7);
    ymm6 = _mm256_permute4x64_epi64(ymm6, 0xd8);
    _mm256_storeu_si256((__m256i *)(dst + 2*8), ymm6);

    ymm4 = round2;
    ymm4 = _mm256_add_epi32(ymm4, _mm256_madd_epi16(*(__m256i *)DCT8Tab[20], ymm0));
    ymm4 = _mm256_add_epi32(ymm4, _mm256_madd_epi16(*(__m256i *)DCT8Tab[21], ymm1));
    ymm4 = _mm256_add_epi32(ymm4, _mm256_madd_epi16(*(__m256i *)DCT8Tab[22], ymm2));
    ymm4 = _mm256_add_epi32(ymm4, _mm256_madd_epi16(*(__m256i *)DCT8Tab[23], ymm3));
    ymm4 = _mm256_srai_epi32(ymm4, SHIFT_FRW8_2ND);

    ymm5 = round2;
    ymm5 = _mm256_add_epi32(ymm5, _mm256_madd_epi16(*(__m256i *)DCT8Tab[24], ymm0));
    ymm5 = _mm256_add_epi32(ymm5, _mm256_madd_epi16(*(__m256i *)DCT8Tab[25], ymm1));
    ymm5 = _mm256_add_epi32(ymm5, _mm256_madd_epi16(*(__m256i *)DCT8Tab[26], ymm2));
    ymm5 = _mm256_add_epi32(ymm5, _mm256_madd_epi16(*(__m256i *)DCT8Tab[27], ymm3));
    ymm5 = _mm256_srai_epi32(ymm5, SHIFT_FRW8_2ND);

    ymm4 = _mm256_packs_epi32(ymm4, ymm5);
    ymm4 = _mm256_permute4x64_epi64(ymm4, 0xd8);
    _mm256_storeu_si256((__m256i *)(dst + 4*8), ymm4);

    ymm6 = round2;
    ymm6 = _mm256_add_epi32(ymm6, _mm256_madd_epi16(*(__m256i *)DCT8Tab[28], ymm0));
    ymm6 = _mm256_add_epi32(ymm6, _mm256_madd_epi16(*(__m256i *)DCT8Tab[29], ymm1));
    ymm6 = _mm256_add_epi32(ymm6, _mm256_madd_epi16(*(__m256i *)DCT8Tab[30], ymm2));
    ymm6 = _mm256_add_epi32(ymm6, _mm256_madd_epi16(*(__m256i *)DCT8Tab[31], ymm3));
    ymm6 = _mm256_srai_epi32(ymm6, SHIFT_FRW8_2ND);

    ymm7 = round2;
    ymm7 = _mm256_add_epi32(ymm7, _mm256_madd_epi16(*(__m256i *)DCT8Tab[32], ymm0));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_madd_epi16(*(__m256i *)DCT8Tab[33], ymm1));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_madd_epi16(*(__m256i *)DCT8Tab[34], ymm2));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_madd_epi16(*(__m256i *)DCT8Tab[35], ymm3));
    ymm7 = _mm256_srai_epi32(ymm7, SHIFT_FRW8_2ND);

    ymm6 = _mm256_packs_epi32(ymm6, ymm7);
    ymm6 = _mm256_permute4x64_epi64(ymm6, 0xd8);
    _mm256_storeu_si256((__m256i *)(dst + 6*8), ymm6);
}

void H265_FASTCALL MAKE_NAME(h265_DCT8x8Fwd_16s)(const short *H265_RESTRICT src, int srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth)
{
    switch (bitDepth) {
    case  8: h265_DCT8x8Fwd_16s_Kernel<SHIFT_FRW8_1ST_BASE + 0>(src, srcStride, dst);  break;
    case  9: h265_DCT8x8Fwd_16s_Kernel<SHIFT_FRW8_1ST_BASE + 1>(src, srcStride, dst);  break;
    case 10: h265_DCT8x8Fwd_16s_Kernel<SHIFT_FRW8_1ST_BASE + 2>(src, srcStride, dst);  break;
    }
}

/* DCT 16x16 */
ALIGN_DECL(32) static const short DCT16Tab[8 + 4][16] = {
    {  90,  87,  87,  57,  80,   9,  70, -43,  90,  87,  87,  57,  80,   9,  70, -43 },
    {  57, -80,  43, -90,  25, -70,   9, -25,  57, -80,  43, -90,  25, -70,   9, -25 },
    {  80,  70,   9, -43, -70, -87, -87,   9,  80,  70,   9, -43, -70, -87, -87,   9 },
    { -25,  90,  57,  25,  90, -80,  43, -57, -25,  90,  57,  25,  90, -80,  43, -57 },
    {  57,  43, -80, -90, -25,  57,  90,  25,  57,  43, -80, -90, -25,  57,  90,  25 },
    {  -9, -87, -87,  70,  43,   9,  70, -80,  -9, -87, -87,  70,  43,   9,  70, -80 },
    {  25,   9, -70, -25,  90,  43, -80, -57,  25,   9, -70, -25,  90,  43, -80, -57 },
    {  43,  70,   9, -80, -57,  87,  87, -90,  43,  70,   9, -80, -57,  87,  87, -90 },

    {  64,  64,  89,  75,  83,  36,  75, -18,  64,  64,  89,  75,  83,  36,  75, -18 },
    {  64, -64,  50, -89,  36, -83,  18, -50,  64, -64,  50, -89,  36, -83,  18, -50 },
    {  64,  64,  50,  18, -36, -83, -89, -50,  64,  64,  50,  18, -36, -83, -89, -50 },
    { -64,  64,  18,  75,  83, -36,  75, -89, -64,  64,  18,  75,  83, -36,  75, -89 },
};

#define SHIFT_FRW16_1ST_BASE   3
#define SHIFT_FRW16_2ND  10

ALIGN_DECL(32) static const int c89[8] = {  89,  89,  89,  89,  89,  89,  89,  89 };
ALIGN_DECL(32) static const int c83[8] = { -83, -83, -83, -83, -83, -83, -83, -83 };

ALIGN_DECL(32) static const int c75[8] = {  75,  75,  75,  75,  75,  75,  75,  75 };
ALIGN_DECL(32) static const int c50[8] = {  50,  50,  50,  50,  50,  50,  50,  50 };
ALIGN_DECL(32) static const int c36[8] = {  36,  36,  36,  36,  36,  36,  36,  36 };
ALIGN_DECL(32) static const int c18[8] = {  18,  18,  18,  18,  18,  18,  18,  18 };

ALIGN_DECL(32) static const int c90[8] = {  90,  90,  90,  90,  90,  90,  90,  90 };
ALIGN_DECL(32) static const int c87[8] = {  87,  87,  87,  87,  87,  87,  87,  87 };
ALIGN_DECL(32) static const int c80[8] = {  80,  80,  80,  80,  80,  80,  80,  80 };
ALIGN_DECL(32) static const int c70[8] = {  70,  70,  70,  70,  70,  70,  70,  70 };

ALIGN_DECL(32) static const int c57[8] = {  57,  57,  57,  57,  57,  57,  57,  57 };
ALIGN_DECL(32) static const int c43[8] = {  43,  43,  43,  43,  43,  43,  43,  43 };
ALIGN_DECL(32) static const int c25[8] = {  25,  25,  25,  25,  25,  25,  25,  25 };
ALIGN_DECL(32) static const int c09[8] = {   9,   9,   9,   9,   9,   9,   9,   9 };

template <int SHIFT_FRW16_1ST>
static void h265_DCT16x16Fwd_16s_Kernel(const short *H265_RESTRICT src, Ipp32s srcStride, short *H265_RESTRICT dst, Ipp16s *temp)
{
    int i;
    short *tmp = temp;
    short *H265_RESTRICT p_tmp;

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm6, ymm7, round1, round2, round2m;

    _mm256_zeroupper();

    round1  = _mm256_set1_epi32(1 << (SHIFT_FRW16_1ST - 1));
    round2  = _mm256_set1_epi32(1 << (SHIFT_FRW16_2ND - 1));
    round2m = _mm256_set1_epi32(1 << (SHIFT_FRW16_2ND - 1 - 6));

    p_tmp = tmp;
    for (i = 0; i < 16; i+=2) {
        ymm2 = _mm256_loadu_si256((__m256i *)(src + (i + 0)*srcStride));    // row 0: 0-15
        ymm3 = _mm256_loadu_si256((__m256i *)(src + (i + 1)*srcStride));    // row 1: 0-15

        ymm0 = _mm256_permute2x128_si256(ymm2, ymm3, 0x20);          // [row 0: 0-7  | row 1: 0-7 ]
        ymm1 = _mm256_permute2x128_si256(ymm2, ymm3, 0x31);          // [row 0: 8-15 | row 1: 8-15]

        ymm1 = _mm256_shuffle_epi32(ymm1, _MM_SHUFFLE(1,0,3,2));
        ymm1 = _mm256_shufflelo_epi16(ymm1, _MM_SHUFFLE(0,1,2,3));
        ymm1 = _mm256_shufflehi_epi16(ymm1, _MM_SHUFFLE(0,1,2,3));

        ymm3 = _mm256_sub_epi16(ymm0, ymm1);
        ymm4 = _mm256_add_epi16(ymm0, ymm1);

        ymm2 = _mm256_shuffle_epi32(ymm3, _MM_SHUFFLE(0,0,0,0));
        ymm0 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT16Tab[0]);
        ymm1 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT16Tab[1]);

        ymm2 = _mm256_shuffle_epi32(ymm3, _MM_SHUFFLE(1,1,1,1));
        ymm6 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT16Tab[2]);
        ymm7 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT16Tab[3]);
        ymm0 = _mm256_add_epi32(ymm0, ymm6);
        ymm1 = _mm256_add_epi32(ymm1, ymm7);

        ymm2 = _mm256_shuffle_epi32(ymm3, _MM_SHUFFLE(2,2,2,2));
        ymm6 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT16Tab[4]);
        ymm7 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT16Tab[5]);
        ymm0 = _mm256_add_epi32(ymm0, ymm6);
        ymm1 = _mm256_add_epi32(ymm1, ymm7);

        ymm2 = _mm256_shuffle_epi32(ymm3, _MM_SHUFFLE(3,3,3,3));
        ymm6 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT16Tab[6]);
        ymm7 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT16Tab[7]);
        ymm0 = _mm256_add_epi32(ymm0, ymm6);
        ymm1 = _mm256_add_epi32(ymm1, ymm7);

        ymm0 = _mm256_add_epi32(ymm0, round1);
        ymm1 = _mm256_add_epi32(ymm1, round1);

        ymm0 = _mm256_srai_epi32(ymm0, SHIFT_FRW16_1ST);
        ymm1 = _mm256_srai_epi32(ymm1, SHIFT_FRW16_1ST);
        ymm0 = _mm256_packs_epi32(ymm0, ymm1);

        ymm1 = _mm256_shuffle_epi32(ymm4, _MM_SHUFFLE(1,0,3,2));
        ymm1 = _mm256_shufflelo_epi16(ymm1, _MM_SHUFFLE(0,1,2,3));
        ymm1 = _mm256_shufflehi_epi16(ymm1, _MM_SHUFFLE(0,1,2,3));

        ymm2 = _mm256_add_epi16(ymm4, ymm1);
        ymm3 = _mm256_sub_epi16(ymm4, ymm1);

        ymm2 = _mm256_unpacklo_epi32(ymm2, ymm3);
        ymm3 = _mm256_shuffle_epi32(ymm2, _MM_SHUFFLE(1,0,1,0));
        ymm2 = _mm256_shuffle_epi32(ymm2, _MM_SHUFFLE(3,2,3,2));

        ymm4 = _mm256_madd_epi16(ymm3, *(__m256i *)DCT16Tab[8]);
        ymm3 = _mm256_madd_epi16(ymm3, *(__m256i *)DCT16Tab[9]);

        ymm1 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT16Tab[10]);
        ymm2 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT16Tab[11]);

        ymm4 = _mm256_add_epi32(ymm4, ymm1);
        ymm3 = _mm256_add_epi32(ymm3, ymm2);

        ymm4 = _mm256_add_epi32(ymm4, round1);
        ymm3 = _mm256_add_epi32(ymm3, round1);

        ymm4 = _mm256_srai_epi32(ymm4, SHIFT_FRW16_1ST);
        ymm3 = _mm256_srai_epi32(ymm3, SHIFT_FRW16_1ST);
        ymm4 = _mm256_packs_epi32(ymm4, ymm3);

        /* final pack */
        ymm1 = _mm256_unpacklo_epi16(ymm4, ymm0);
        ymm0 = _mm256_unpackhi_epi16(ymm4, ymm0);

        ymm2 = _mm256_permute2x128_si256(ymm1, ymm0, 0x20);    // [row 0: 0-7  | row 1: 0-7 ]
        ymm3 = _mm256_permute2x128_si256(ymm1, ymm0, 0x31);    // [row 0: 8-15 | row 1: 8-15]

        /* (tmp + i*16) should always be 32-byte aligned */
        _mm256_store_si256((__m256i *)(p_tmp + 0*16), ymm2);
        _mm256_store_si256((__m256i *)(p_tmp + 1*16), ymm3);
        p_tmp += 2*16;
    }

    p_tmp = tmp;
    for (i = 0; i < 16; i+=8) {
        __m256i sm, sp;
        /* E and O*/
        sm = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp +  0*16)));
        sp = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp + 15*16)));
        __m256i O0 = _mm256_sub_epi32(sm, sp);
        __m256i E0 = _mm256_add_epi32(sm, sp);

        /* E and O*/
        sm = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp + 7*16)));
        sp = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp + 8*16)));
        __m256i O7 = _mm256_sub_epi32(sm, sp);
        __m256i E7 = _mm256_add_epi32(sm, sp);

        /* EE and EO */
        __m256i EO0 = _mm256_sub_epi32( E0, E7);
        __m256i EE0 = _mm256_add_epi32( E0, E7);

        /* E and O*/
        sm = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp +  3*16)));
        sp = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp + 12*16)));
        __m256i O3 = _mm256_sub_epi32(sm, sp);
        __m256i E3 = _mm256_add_epi32(sm, sp);

        /* E and O*/
        sm = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp + 4*16)));
        sp = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp + 11*16)));
        __m256i O4 = _mm256_sub_epi32(sm, sp);
        __m256i E4 = _mm256_add_epi32(sm, sp);

        /* EE and EO */
        __m256i EO3 = _mm256_sub_epi32( E3, E4);
        __m256i EE3 = _mm256_add_epi32( E3, E4);

        /* EEE and EEO */
        __m256i EEE0 = _mm256_add_epi32( EE0, EE3);    
        __m256i EEO0 = _mm256_sub_epi32( EE0, EE3);


        /* E and O*/
        sm = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp +  1*16)));
        sp = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp + 14*16)));
        __m256i O1 = _mm256_sub_epi32(sm, sp);
        __m256i E1 = _mm256_add_epi32(sm, sp);

        /* E and O*/
        sm = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp +  6*16)));
        sp = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp +  9*16)));
        __m256i O6 = _mm256_sub_epi32(sm, sp);
        __m256i E6 = _mm256_add_epi32(sm, sp);

        /* EE and EO */
        __m256i EO1 = _mm256_sub_epi32( E1, E6);
        __m256i EE1 = _mm256_add_epi32( E1, E6);

        /* E and O*/
        sm = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp +  2*16)));
        sp = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp + 13*16)));
        __m256i O2 = _mm256_sub_epi32(sm, sp);
        __m256i E2 = _mm256_add_epi32(sm, sp);

        /* E and O*/
        sm = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp +  5*16)));
        sp = _mm256_cvtepi16_epi32(_mm_load_si128((const __m128i *) (p_tmp + 10*16)));
        __m256i O5 = _mm256_sub_epi32(sm, sp);
        __m256i E5 = _mm256_add_epi32(sm, sp);

        /* EE and EO */
        __m256i EO2 = _mm256_sub_epi32( E2, E5);
        __m256i EE2 = _mm256_add_epi32( E2, E5);

        /* EEE and EEO */
        __m256i EEO1 = _mm256_sub_epi32( EE1, EE2);
        __m256i EEE1 = _mm256_add_epi32( EE1, EE2);

        __m256i acc4  = _mm256_mullo_epi32( EEO1, *(__m256i *)c36);
        __m256i acc12 = _mm256_mullo_epi32( EEO1, *(__m256i *)c83);
        __m256i acc0  = _mm256_add_epi32( EEE0, EEE1);
        __m256i acc8  = _mm256_sub_epi32( EEE0, EEE1);
        acc4  = _mm256_sub_epi32( acc4,  _mm256_mullo_epi32(EEO0, *(__m256i *)c83));
        acc12 = _mm256_add_epi32( acc12, _mm256_mullo_epi32(EEO0, *(__m256i *)c36));
        acc0  = _mm256_srai_epi32( _mm256_add_epi32( acc0,  round2m), SHIFT_FRW16_2ND-6);
        acc8  = _mm256_srai_epi32( _mm256_add_epi32( acc8,  round2m), SHIFT_FRW16_2ND-6);
        acc4  = _mm256_srai_epi32( _mm256_add_epi32( acc4,  round2), SHIFT_FRW16_2ND);
        acc12 = _mm256_srai_epi32( _mm256_add_epi32( acc12, round2), SHIFT_FRW16_2ND);
        __m256i acc_0_8 = _mm256_packs_epi32( acc0, acc8);
        __m256i acc_4_12 = _mm256_packs_epi32( acc4, acc12);

        acc_0_8 = _mm256_permute4x64_epi64(acc_0_8, 0xd8);
        acc_4_12 = _mm256_permute4x64_epi64(acc_4_12, 0xd8);
        _mm_storeu_si128((__m128i *)(dst +  0*16), mm128(acc_0_8));
        _mm_storeu_si128((__m128i *)(dst +  4*16), mm128(acc_4_12));
        
        acc_0_8 = _mm256_permute2x128_si256(acc_0_8, acc_0_8, 0x11);
        acc_4_12 = _mm256_permute2x128_si256(acc_4_12, acc_4_12, 0x11);
        _mm_storeu_si128((__m128i *)(dst +  8*16), mm128(acc_0_8));
        _mm_storeu_si128((__m128i *)(dst + 12*16), mm128(acc_4_12));

        __m256i acc2  = _mm256_mullo_epi32( EO0, *(__m256i *)c89);
        __m256i acc6  = _mm256_mullo_epi32( EO0, *(__m256i *)c75);
        __m256i acc10 = _mm256_mullo_epi32( EO0, *(__m256i *)c50);
        __m256i acc14 = _mm256_mullo_epi32( EO0, *(__m256i *)c18);
        acc2  = _mm256_add_epi32( acc2,  _mm256_mullo_epi32( EO1, *(__m256i *)c75));
        acc6  = _mm256_sub_epi32( acc6,  _mm256_mullo_epi32( EO1, *(__m256i *)c18));
        acc10 = _mm256_sub_epi32( acc10, _mm256_mullo_epi32( EO1, *(__m256i *)c89));
        acc14 = _mm256_sub_epi32( acc14, _mm256_mullo_epi32( EO1, *(__m256i *)c50));
        acc2  = _mm256_add_epi32( acc2,  _mm256_mullo_epi32( EO2, *(__m256i *)c50));
        acc6  = _mm256_sub_epi32( acc6,  _mm256_mullo_epi32( EO2, *(__m256i *)c89));
        acc10 = _mm256_add_epi32( acc10, _mm256_mullo_epi32( EO2, *(__m256i *)c18));
        acc14 = _mm256_add_epi32( acc14, _mm256_mullo_epi32( EO2, *(__m256i *)c75));
        acc2  = _mm256_add_epi32( acc2,  _mm256_mullo_epi32( EO3, *(__m256i *)c18));
        acc6  = _mm256_sub_epi32( acc6,  _mm256_mullo_epi32( EO3, *(__m256i *)c50));
        acc10 = _mm256_add_epi32( acc10, _mm256_mullo_epi32( EO3, *(__m256i *)c75));
        acc14 = _mm256_sub_epi32( acc14, _mm256_mullo_epi32( EO3, *(__m256i *)c89));
        acc2  = _mm256_srai_epi32( _mm256_add_epi32( acc2,  round2), SHIFT_FRW16_2ND);
        acc6  = _mm256_srai_epi32( _mm256_add_epi32( acc6,  round2), SHIFT_FRW16_2ND);
        acc10 = _mm256_srai_epi32( _mm256_add_epi32( acc10, round2), SHIFT_FRW16_2ND);
        acc14 = _mm256_srai_epi32( _mm256_add_epi32( acc14, round2), SHIFT_FRW16_2ND);
        __m256i acc_2_6   = _mm256_packs_epi32( acc2,  acc6);
        __m256i acc_10_14 = _mm256_packs_epi32( acc10, acc14);

        acc_2_6 = _mm256_permute4x64_epi64(acc_2_6, 0xd8);
        acc_10_14 = _mm256_permute4x64_epi64(acc_10_14, 0xd8);
        _mm_storeu_si128((__m128i *)(dst +  2*16), mm128(acc_2_6));
        _mm_storeu_si128((__m128i *)(dst + 10*16), mm128(acc_10_14));
        
        acc_2_6 = _mm256_permute2x128_si256(acc_2_6, acc_2_6, 0x11);
        acc_10_14 = _mm256_permute2x128_si256(acc_10_14, acc_10_14, 0x11);
        _mm_storeu_si128((__m128i *)(dst +  6*16), mm128(acc_2_6));
        _mm_storeu_si128((__m128i *)(dst + 14*16), mm128(acc_10_14));

        __m256i acc1 = _mm256_mullo_epi32( O0, *(__m256i *)c90);
        __m256i acc3 = _mm256_mullo_epi32( O0, *(__m256i *)c87);
        __m256i acc5 = _mm256_mullo_epi32( O0, *(__m256i *)c80);
        __m256i acc7 = _mm256_mullo_epi32( O0, *(__m256i *)c70);
        acc1 = _mm256_add_epi32( acc1, _mm256_mullo_epi32( O1, *(__m256i *)c87));
        acc3 = _mm256_add_epi32( acc3, _mm256_mullo_epi32( O1, *(__m256i *)c57));
        acc5 = _mm256_add_epi32( acc5, _mm256_mullo_epi32( O1, *(__m256i *)c09));
        acc7 = _mm256_sub_epi32( acc7, _mm256_mullo_epi32( O1, *(__m256i *)c43));
        acc1 = _mm256_add_epi32( acc1, _mm256_mullo_epi32( O2, *(__m256i *)c80));
        acc3 = _mm256_add_epi32( acc3, _mm256_mullo_epi32( O2, *(__m256i *)c09));
        acc5 = _mm256_sub_epi32( acc5, _mm256_mullo_epi32( O2, *(__m256i *)c70));
        acc7 = _mm256_sub_epi32( acc7, _mm256_mullo_epi32( O2, *(__m256i *)c87));
        acc1 = _mm256_add_epi32( acc1, _mm256_mullo_epi32( O3, *(__m256i *)c70));
        acc3 = _mm256_sub_epi32( acc3, _mm256_mullo_epi32( O3, *(__m256i *)c43));
        acc5 = _mm256_sub_epi32( acc5, _mm256_mullo_epi32( O3, *(__m256i *)c87));
        acc7 = _mm256_add_epi32( acc7, _mm256_mullo_epi32( O3, *(__m256i *)c09));
        acc1 = _mm256_add_epi32( acc1, _mm256_mullo_epi32( O4, *(__m256i *)c57));
        acc3 = _mm256_sub_epi32( acc3, _mm256_mullo_epi32( O4, *(__m256i *)c80));
        acc5 = _mm256_sub_epi32( acc5, _mm256_mullo_epi32( O4, *(__m256i *)c25));
        acc7 = _mm256_add_epi32( acc7, _mm256_mullo_epi32( O4, *(__m256i *)c90));
        acc1 = _mm256_add_epi32( acc1, _mm256_mullo_epi32( O5, *(__m256i *)c43));
        acc3 = _mm256_sub_epi32( acc3, _mm256_mullo_epi32( O5, *(__m256i *)c90));
        acc5 = _mm256_add_epi32( acc5, _mm256_mullo_epi32( O5, *(__m256i *)c57));
        acc7 = _mm256_add_epi32( acc7, _mm256_mullo_epi32( O5, *(__m256i *)c25));
        acc1 = _mm256_add_epi32( acc1, _mm256_mullo_epi32( O6, *(__m256i *)c25));
        acc3 = _mm256_sub_epi32( acc3, _mm256_mullo_epi32( O6, *(__m256i *)c70));
        acc5 = _mm256_add_epi32( acc5, _mm256_mullo_epi32( O6, *(__m256i *)c90));
        acc7 = _mm256_sub_epi32( acc7, _mm256_mullo_epi32( O6, *(__m256i *)c80));
        acc1 = _mm256_add_epi32( acc1, _mm256_mullo_epi32( O7, *(__m256i *)c09));
        acc3 = _mm256_sub_epi32( acc3, _mm256_mullo_epi32( O7, *(__m256i *)c25));
        acc5 = _mm256_add_epi32( acc5, _mm256_mullo_epi32( O7, *(__m256i *)c43));
        acc7 = _mm256_sub_epi32( acc7, _mm256_mullo_epi32( O7, *(__m256i *)c57));
        acc1 = _mm256_srai_epi32( _mm256_add_epi32( acc1, round2), SHIFT_FRW16_2ND);
        acc3 = _mm256_srai_epi32( _mm256_add_epi32( acc3, round2), SHIFT_FRW16_2ND);
        acc5 = _mm256_srai_epi32( _mm256_add_epi32( acc5, round2), SHIFT_FRW16_2ND);
        acc7 = _mm256_srai_epi32( _mm256_add_epi32( acc7, round2), SHIFT_FRW16_2ND);
        __m256i acc_1_3 = _mm256_packs_epi32( acc1, acc3);
        __m256i acc_5_7 = _mm256_packs_epi32( acc5, acc7);

        acc_1_3 = _mm256_permute4x64_epi64(acc_1_3, 0xd8);
        acc_5_7 = _mm256_permute4x64_epi64(acc_5_7, 0xd8);
        _mm_storeu_si128((__m128i *)(dst + 1*16), mm128(acc_1_3));
        _mm_storeu_si128((__m128i *)(dst + 5*16), mm128(acc_5_7));
        
        acc_1_3 = _mm256_permute2x128_si256(acc_1_3, acc_1_3, 0x11);
        acc_5_7 = _mm256_permute2x128_si256(acc_5_7, acc_5_7, 0x11);
        _mm_storeu_si128((__m128i *)(dst + 3*16), mm128(acc_1_3));
        _mm_storeu_si128((__m128i *)(dst + 7*16), mm128(acc_5_7));

        __m256i acc9  = _mm256_mullo_epi32( O0, *(__m256i *)c57);
        __m256i acc11 = _mm256_mullo_epi32( O0, *(__m256i *)c43);
        __m256i acc13 = _mm256_mullo_epi32( O0, *(__m256i *)c25);
        __m256i acc15 = _mm256_mullo_epi32( O0, *(__m256i *)c09);
        acc9  = _mm256_sub_epi32( acc9,  _mm256_mullo_epi32( O1, *(__m256i *)c80));
        acc11 = _mm256_sub_epi32( acc11, _mm256_mullo_epi32( O1, *(__m256i *)c90));
        acc13 = _mm256_sub_epi32( acc13, _mm256_mullo_epi32( O1, *(__m256i *)c70));
        acc15 = _mm256_sub_epi32( acc15, _mm256_mullo_epi32( O1, *(__m256i *)c25));
        acc9  = _mm256_sub_epi32( acc9,  _mm256_mullo_epi32( O2, *(__m256i *)c25));
        acc11 = _mm256_add_epi32( acc11, _mm256_mullo_epi32( O2, *(__m256i *)c57));
        acc13 = _mm256_add_epi32( acc13, _mm256_mullo_epi32( O2, *(__m256i *)c90));
        acc15 = _mm256_add_epi32( acc15, _mm256_mullo_epi32( O2, *(__m256i *)c43));
        acc9  = _mm256_add_epi32( acc9,  _mm256_mullo_epi32( O3, *(__m256i *)c90));
        acc11 = _mm256_add_epi32( acc11, _mm256_mullo_epi32( O3, *(__m256i *)c25));
        acc13 = _mm256_sub_epi32( acc13, _mm256_mullo_epi32( O3, *(__m256i *)c80));
        acc15 = _mm256_sub_epi32( acc15, _mm256_mullo_epi32( O3, *(__m256i *)c57));
        acc9  = _mm256_sub_epi32( acc9,  _mm256_mullo_epi32( O4, *(__m256i *)c09));
        acc11 = _mm256_sub_epi32( acc11, _mm256_mullo_epi32( O4, *(__m256i *)c87));
        acc13 = _mm256_add_epi32( acc13, _mm256_mullo_epi32( O4, *(__m256i *)c43));
        acc15 = _mm256_add_epi32( acc15, _mm256_mullo_epi32( O4, *(__m256i *)c70));
        acc9  = _mm256_sub_epi32( acc9,  _mm256_mullo_epi32( O5, *(__m256i *)c87));
        acc11 = _mm256_add_epi32( acc11, _mm256_mullo_epi32( O5, *(__m256i *)c70));
        acc13 = _mm256_add_epi32( acc13, _mm256_mullo_epi32( O5, *(__m256i *)c09));
        acc15 = _mm256_sub_epi32( acc15, _mm256_mullo_epi32( O5, *(__m256i *)c80));
        acc9  = _mm256_add_epi32( acc9,  _mm256_mullo_epi32( O6, *(__m256i *)c43));
        acc11 = _mm256_add_epi32( acc11, _mm256_mullo_epi32( O6, *(__m256i *)c09));
        acc13 = _mm256_sub_epi32( acc13, _mm256_mullo_epi32( O6, *(__m256i *)c57));
        acc15 = _mm256_add_epi32( acc15, _mm256_mullo_epi32( O6, *(__m256i *)c87));
        acc9  = _mm256_add_epi32( acc9,  _mm256_mullo_epi32( O7, *(__m256i *)c70));
        acc11 = _mm256_sub_epi32( acc11, _mm256_mullo_epi32( O7, *(__m256i *)c80));
        acc13 = _mm256_add_epi32( acc13, _mm256_mullo_epi32( O7, *(__m256i *)c87));
        acc15 = _mm256_sub_epi32( acc15, _mm256_mullo_epi32( O7, *(__m256i *)c90));
        acc9  = _mm256_srai_epi32( _mm256_add_epi32( acc9,  round2), SHIFT_FRW16_2ND);
        acc11 = _mm256_srai_epi32( _mm256_add_epi32( acc11, round2), SHIFT_FRW16_2ND);
        acc13 = _mm256_srai_epi32( _mm256_add_epi32( acc13, round2), SHIFT_FRW16_2ND);
        acc15 = _mm256_srai_epi32( _mm256_add_epi32( acc15, round2), SHIFT_FRW16_2ND);
        __m256i acc_9_11  = _mm256_packs_epi32( acc9,  acc11);
        __m256i acc_13_15 = _mm256_packs_epi32( acc13, acc15);

        acc_9_11 = _mm256_permute4x64_epi64(acc_9_11, 0xd8);
        acc_13_15 = _mm256_permute4x64_epi64(acc_13_15, 0xd8);
        _mm_storeu_si128((__m128i *)(dst +  9*16), mm128(acc_9_11));
        _mm_storeu_si128((__m128i *)(dst + 13*16), mm128(acc_13_15));
        
        acc_9_11 = _mm256_permute2x128_si256(acc_9_11, acc_9_11, 0x11);
        acc_13_15 = _mm256_permute2x128_si256(acc_13_15, acc_13_15, 0x11);
        _mm_storeu_si128((__m128i *)(dst + 11*16), mm128(acc_9_11));
        _mm_storeu_si128((__m128i *)(dst + 15*16), mm128(acc_13_15));

        p_tmp += 8;
        dst   += 8; 
    }
}

void H265_FASTCALL MAKE_NAME(h265_DCT16x16Fwd_16s)(const short *H265_RESTRICT src, Ipp32s srcStride, short *H265_RESTRICT dst, Ipp32u bitDepth, Ipp16s *temp)
{
    switch (bitDepth) {
    case  8: h265_DCT16x16Fwd_16s_Kernel<SHIFT_FRW16_1ST_BASE + 0>(src, srcStride, dst, temp);   break;
    case  9: h265_DCT16x16Fwd_16s_Kernel<SHIFT_FRW16_1ST_BASE + 1>(src, srcStride, dst, temp);   break;
    case 10: h265_DCT16x16Fwd_16s_Kernel<SHIFT_FRW16_1ST_BASE + 2>(src, srcStride, dst, temp);   break;
    }
}

#define SHIFT_FRW32_1ST  4
#define SHIFT_FRW32_2ND  11

ALIGN_DECL(32) static const short DCT32TabH_0[16][16] = {
    /* kfh_0000 - kfh_0015, kfh_0100 - kfh_0115, interleaved and reordered as 0,1,8,9,2,3,10,11... */
    { 90,  90,  90,  82,  88,  67,  85,  46,  82,  22,  78,  -4,  73, -31,  67, -54},
    { 61, -73,  54, -85,  46, -90,  38, -88,  31, -78,  22, -61,  13, -38,   4, -13},

    { 61,  54, -73, -85, -46,  -4,  82,  88,  31, -46, -88, -61, -13,  82,  90,  13},
    { -4, -90, -90,  38,  22,  67,  85, -78, -38, -22, -78,  90,  54, -31,  67, -73},

    { 88,  85,  67,  46,  31, -13, -13, -67, -54, -90, -82, -73, -90, -22, -78,  38},
    {-46,  82,  -4,  88,  38,  54,  73,  -4,  90, -61,  85, -90,  61, -78,  22, -31},

    { 46,  38, -90, -88,  38,  73,  54,  -4, -90, -67,  31,  90,  61, -46, -88, -31},
    { 22,  85,  67, -78, -85,  13,  13,  61,  73, -90, -82,  54,   4,  22,  78, -82},

    { 82,  78,  22,  -4, -54, -82, -90, -73, -61,  13,  13,  85,  78,  67,  85, -22},
    { 31, -88, -46, -61, -90,  31, -67,  90,   4,  54,  73, -38,  88, -90,  38, -46},

    { 31,  22, -78, -61,  90,  85, -61, -90,   4,  73,  54, -38, -88,  -4,  82,  46},
    {-38, -78, -22,  90,  73, -82, -90,  54,  67, -13, -13, -31, -46,  67,  85, -88},

    { 73,  67, -31, -54, -90, -78, -22,  38,  78,  85,  67, -22, -38, -90, -90,   4},
    {-13,  90,  82,  13,  61, -88, -46, -31, -88,  82,  -4,  46,  85, -73,  54, -61},

    { 13,   4, -38, -13,  61,  22, -78, -31,  88,  38, -90, -46,  85,  54, -73, -61},
    { 54,  67, -31, -73,   4,  78,  22, -82, -46,  85,  67, -88, -82,  90,  90, -90},
};

ALIGN_DECL(32) static const short DCT32TabH_2[4][16] = {
    /* kfh_0200 - kfh_0207 */
    { 90,  87,  87,  57,  80,   9,  70, -43,  57, -80,  43, -90,  25, -70,   9, -25},
    { 80,  70,   9, -43, -70, -87, -87,   9, -25,  90,  57,  25,  90, -80,  43, -57},
    { 57,  43, -80, -90, -25,  57,  90,  25,  -9, -87, -87,  70,  43,   9,  70, -80},
    { 25,   9, -70, -25,  90,  43, -80, -57,  43,  70,   9, -80, -57,  87,  87, -90},
};

ALIGN_DECL(32) static const short DCT32TabH_3[2][16] = {
    /* kfh_0300 - kfh_0304 */
    { 89,  75,  75, -18,  50, -89,  18, -50,  89,  75,  75, -18,  50, -89,  18, -50},
    { 50,  18, -89, -50,  18,  75,  75, -89,  50,  18, -89, -50,  18,  75,  75, -89},
};

ALIGN_DECL(32) static const short DCT32TabH_4[1][16] = {
    /* kfh_0400 */
    { 64,  64,  64, -64,  83,  36,  36, -83, 64,  64,  64, -64,  83,  36,  36, -83},
};

ALIGN_DECL(32) static const short DCT32TabV_0[16][16] = {
    /* kfv_0000 - kfv_0015, kfv_0100 - kfv_0115, interleaved for combining stages  */
    { 90,  90,  67,  46, -54, -82, -22,  38,  82,  22, -82, -73,  78,  67, -90,   4},
    { 61,  54, -90, -88,  90,  85, -78, -31,  31, -46,  31,  90, -88,  -4, -73, -61},
    { 61, -73,  -4,  88, -90,  31, -46, -31,  31, -78,  85, -90,  88, -90,  54, -61},
    { -4, -90,  67, -78,  73, -82,  22, -82, -38, -22, -82,  54, -46,  67,  90, -90},

    { 88,  85,  22,  -4, -90, -78,  85,  46, -54, -90,  13,  85, -38, -90,  67, -54},
    { 46,  38, -78, -61,  61,  22,  82,  88, -90, -67,  54, -38,  85,  54,  90,  13},
    {-46,  82, -46, -61,  61, -88,  38, -88,  90, -61,  73, -38,  85, -73,   4, -13},
    { 22,  85, -22,  90,   4,  78,  85, -78,  73, -90, -13, -31, -82,  90,  67, -73},
    
    { 82,  78, -31, -54,  88,  67, -13, -67, -61,  13,  67, -22,  73, -31, -78,  38},
    { 31,  22, -38, -13, -46,  -4,  54,  -4,   4,  73, -90, -46, -13,  82, -88, -31},
    { 31, -88,  82,  13,  46, -90,  73,  -4,   4,  54,  -4,  46,  13, -38,  22, -31},
    {-38, -78, -31, -73,  22,  67,  13,  61,  67, -13,  67, -88,  54, -31,  78, -82},
    
    { 73,  67,  90,  82,  31, -13, -90, -73,  78,  85,  78,  -4, -90, -22,  85, -22},
    { 13,   4, -73, -85,  38,  73, -61, -90,  88,  38, -88, -61,  61, -46,  82,  46},
    {-13,  90,  54, -85,  38,  54, -67,  90, -88,  82,  22, -61,  61, -78,  38, -46},
    { 54,  67, -90,  38, -85,  13, -90,  54, -46,  85, -78,  90,   4,  22,  85, -88},
};

ALIGN_DECL(32) static const int DCT32TabV_2[8][8] = {
    /* kfv_0200 - kfv_0215, reordered as 0,2,1,3,... */
    { 90,  57, -70,   9,  57, -90,  90, -57},
    { 57, -90,  90, -57,   -9, 70, -57, -90},
    { 87,   9, -87,  70, -80,  57, -80,   9},
    { 43, -70,  43,  90, -87,   9,  87,  70},
    { 80, -43,  80, -43, -25,  25,  25, -25},
    { 25, -25, -25,  25,  43, -80,  43, -80},
    { 70,  87,   9, -87,  90,  43, -70,  43},
    {  9, -80,  57, -80,  70, -87,   9,  87},
};

ALIGN_DECL(32) static const int DCT32TabV_3[4][8] = {
    /* kfv_0300 - kfv_0307 */
    { 64,  36, -64, -36,  89, -18,  18, -89},
    { 64, -36,  64,  36,  75, -89,  75,  18},
    { 64, -83,  64, -83,  50, -50,  50, -50},
    { 64,  83, -64,  83,  18,  75, -89,  75},
};

/* output of H pass is written in sequential rows, so this is the order to load for the V pass */
static const int DCT32TabHShuf[32] = {
    28,  0, 16,  1, 24,  2, 17,  3, 30,  4, 18,  5, 25,  6, 19,  7,
    29,  8, 20,  9, 26, 10, 21, 11, 31, 12, 22, 13, 27, 14, 23, 15,
};

/* row order for final output */
static const int DCT32TabVShuf[32] = {
     1,  3,  5,  7,  9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31,
     2,  6, 10, 14, 18, 22, 26, 30,  0,  8, 16, 24,  4, 12, 20, 28,
};

#if defined(AVX2_FTR_32x32_V1_ENABLED)
void H265_FASTCALL MAKE_NAME(h265_DCT32x32Fwd_16s)(const short *H265_RESTRICT src, short *H265_RESTRICT dest)
{
    int i, j, num;
    short *tmp;
    const int *sPos;
    ALIGN_DECL(32) short temp[32*32];
    ALIGN_DECL(32) __m128i buff[16];

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, round1, round2;

    _mm256_zeroupper();

    round1  = _mm256_set1_epi32(1 << (SHIFT_FRW32_1ST - 1));
    round2  = _mm256_set1_epi32(1 << (SHIFT_FRW32_2ND - 1));

    /* horizontal pass */
    num = 0;
    tmp = temp;
    for (i = 0; i < 32; i++) {
        ymm0 = _mm256_loadu_si256((__m256i *)(src + 8*0));      // load  0 - 15
        ymm2 = _mm256_loadu_si256((__m256i *)(src + 8*2));      // load 16 - 31

        ymm2 = _mm256_shufflelo_epi16(ymm2, 0x1b);              // 19 18 17 16 20 21 22 23 27 26 25 24 28 29 30 31
        ymm2 = _mm256_shufflehi_epi16(ymm2, 0x1b);              // 19 18 17 16 23 22 21 20 27 26 25 24 31 30 29 28
        ymm2 = _mm256_shuffle_epi32(ymm2, 0x4e);                // 23 22 21 20 19 18 17 16 31 30 29 28 27 26 25 24
        ymm2 = _mm256_permute2x128_si256(ymm2, ymm2, 0x01);

        ymm0 = _mm256_sub_epi16(ymm0, ymm2);
        ymm2 = _mm256_slli_epi16(ymm2, 1);
        ymm2 = _mm256_add_epi16(ymm0, ymm2);

        ymm1 = _mm256_permute2x128_si256(ymm0, ymm0, 0x11);
        ymm0 = _mm256_permute2x128_si256(ymm0, ymm0, 0x00);

        ymm4 = round1;
        ymm7 = round1;

        /* combine first 2 stages */
        for (j = 0; j < 4; j++) {
            ymm5 = _mm256_shuffle_epi32(ymm0, 0x00);
            ymm3 = ymm5;

            ymm5 = _mm256_madd_epi16(ymm5, *(__m256i *)DCT32TabH_0[4*j + 0]);
            ymm4 = _mm256_add_epi32(ymm4, ymm5);
            ymm3 = _mm256_madd_epi16(ymm3, *(__m256i *)DCT32TabH_0[4*j + 1]);
            ymm7 = _mm256_add_epi32(ymm7, ymm3);

            ymm6 = _mm256_shuffle_epi32(ymm1, 0x00);
            ymm3 = ymm6;

            ymm6 = _mm256_madd_epi16(ymm6, *(__m256i *)DCT32TabH_0[4*j + 2]);
            ymm4 = _mm256_add_epi32(ymm4, ymm6);
            ymm3 = _mm256_madd_epi16(ymm3, *(__m256i *)DCT32TabH_0[4*j + 3]);
            ymm7 = _mm256_add_epi32(ymm7, ymm3);

            /* rotate dwords one position to right */
            ymm0 = _mm256_shuffle_epi32(ymm0, 0x39);
            ymm1 = _mm256_shuffle_epi32(ymm1, 0x39);
        }
        ymm4 = _mm256_srai_epi32(ymm4,  SHIFT_FRW32_1ST);
        ymm7 = _mm256_srai_epi32(ymm7,  SHIFT_FRW32_1ST);
        ymm4 = _mm256_packs_epi32(ymm4, ymm7);
        ymm4 = _mm256_permute4x64_epi64(ymm4, 0xd8);

        buff[4*0 + num] = mm128(ymm4);
        ymm4 = _mm256_permute2x128_si256(ymm4, ymm4, 0x01);
        buff[4*1 + num] = mm128(ymm4);

        /* shuffle for next stage */
        ymm3 = ymm2;
        ymm2 = _mm256_permute2x128_si256(ymm2, ymm2, 0x01);

        ymm2 = _mm256_shufflelo_epi16(ymm2, 0x1b); 
        ymm2 = _mm256_shufflehi_epi16(ymm2, 0x1b); 
        ymm2 = _mm256_shuffle_epi32(ymm2, 0x4e);

        ymm3 = _mm256_sub_epi16(ymm3, ymm2);
        ymm3 = _mm256_permute2x128_si256(ymm3, ymm3, 0x00);

        /* third stage */
        ymm4 = round1;
        ymm0 = _mm256_shuffle_epi32(ymm3, 0x00);
        ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)DCT32TabH_2[0]);
        ymm4 = _mm256_add_epi32(ymm4, ymm0);

        ymm0 = _mm256_shuffle_epi32(ymm3, 0x55);
        ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)DCT32TabH_2[1]);
        ymm4 = _mm256_add_epi32(ymm4, ymm0);

        ymm0 = _mm256_shuffle_epi32(ymm3, 0xAA);
        ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)DCT32TabH_2[2]);
        ymm4 = _mm256_add_epi32(ymm4, ymm0);

        ymm0 = _mm256_shuffle_epi32(ymm3, 0xFF);
        ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)DCT32TabH_2[3]);
        ymm4 = _mm256_add_epi32(ymm4, ymm0);

        ymm4 = _mm256_srai_epi32(ymm4,  SHIFT_FRW32_1ST);
        ymm4 = _mm256_packs_epi32(ymm4, ymm4);
        ymm4 = _mm256_permute4x64_epi64(ymm4, 0xd8);

        buff[4*2 + num] = mm128(ymm4);

        /* shuffle for next stage */
        ymm2 = _mm256_add_epi16(ymm2,  ymm2);
        ymm2 = _mm256_add_epi16(ymm2,  ymm3);
        ymm2 = _mm256_permute2x128_si256(ymm2, ymm2, 0x00);

        ymm3 = _mm256_shufflehi_epi16(ymm2, 0x1b);
        ymm3 = _mm256_unpackhi_epi64(ymm3, ymm3);

        ymm4 = ymm3;
        ymm3 = _mm256_add_epi16(ymm3,  ymm2);
        ymm2 = _mm256_sub_epi16(ymm2,  ymm4);

        /* fourth stage */
        ymm4 = _mm256_shuffle_epi32(ymm2, 0x00);
        ymm5 = _mm256_shuffle_epi32(ymm2, 0x55);
        ymm4 = _mm256_madd_epi16(ymm4, *(__m256i *)DCT32TabH_3[0]);
        ymm5 = _mm256_madd_epi16(ymm5, *(__m256i *)DCT32TabH_3[1]);
        ymm4 = _mm256_add_epi32(ymm4, ymm5);

        ymm5 = _mm256_shufflelo_epi16(ymm3, 0x1b);
        ymm3 = _mm256_sub_epi16(ymm3,  ymm5);
        ymm5 = _mm256_add_epi16(ymm5,  ymm5);
        ymm5 = _mm256_add_epi16(ymm5,  ymm3);

        ymm5 = _mm256_unpacklo_epi32(ymm5, ymm3);
        ymm5 = _mm256_unpacklo_epi32(ymm5, ymm5);
        ymm5 = _mm256_madd_epi16(ymm5, *(__m256i *)DCT32TabH_4[0]);

        ymm4 = _mm256_add_epi32(ymm4, round1);
        ymm5 = _mm256_add_epi32(ymm5, round1);
        ymm4 = _mm256_srai_epi32(ymm4, SHIFT_FRW32_1ST);
        ymm5 = _mm256_srai_epi32(ymm5, SHIFT_FRW32_1ST);
        ymm4 = _mm256_packs_epi32(ymm4, ymm5);
        buff[4*3 + num] = mm128(ymm4);

        num++;
        if (num == 4) {
            /* transpose 4x8 into 8x4 */
            for (j = 0; j < 4; j++) {
                ymm0 = _mm256_load_si256((__m256i *)(buff + 4*j + 0));    // 00 01 02 03 04 05 06 07 | 10 11 12 13 14 15 16 17
                ymm2 = _mm256_load_si256((__m256i *)(buff + 4*j + 2));    // 20 21 22 23 24 25 26 27 | 30 31 32 33 34 35 36 37

                ymm1 = _mm256_unpacklo_epi16(ymm0, ymm2);                 // 00 20 01 21 02 22 03 23 | 10 30 11 31 12 32 13 33
                ymm3 = _mm256_unpackhi_epi16(ymm0, ymm2);                 // 04 24 05 25 06 26 07 27 | 14 34 15 35 16 36 17 37

                ymm0 = _mm256_permute2x128_si256(ymm1, ymm3, 0x20);       // 00 20 01 21 02 22 03 23 | 04 24 05 25 06 26 07 27
                ymm2 = _mm256_permute2x128_si256(ymm1, ymm3, 0x31);       // 10 30 11 31 12 32 13 33 | 14 34 15 35 16 36 17 37
                
                ymm1 = _mm256_unpackhi_epi16(ymm0, ymm2);                 // 02 12 22 32 03 13 23 33 | 06 16 26 36 07 17 27 37
                ymm0 = _mm256_unpacklo_epi16(ymm0, ymm2);                 // 00 10 20 30 01 11 21 31 | 04 14 24 34 05 15 25 35
                ymm2 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);
                ymm3 = _mm256_permute2x128_si256(ymm1, ymm1, 0x01);

                /* write out sequentially, vertical pass reorders as lines are read */
                _mm_storel_epi64((__m128i*)(&tmp[(8*j+0)*32]), mm128(ymm0));
                _mm_storeh_epi64((__m128i*)(&tmp[(8*j+1)*32]), mm128(ymm0));
                _mm_storel_epi64((__m128i*)(&tmp[(8*j+2)*32]), mm128(ymm1));
                _mm_storeh_epi64((__m128i*)(&tmp[(8*j+3)*32]), mm128(ymm1));
                _mm_storel_epi64((__m128i*)(&tmp[(8*j+4)*32]), mm128(ymm2));
                _mm_storeh_epi64((__m128i*)(&tmp[(8*j+5)*32]), mm128(ymm2));
                _mm_storel_epi64((__m128i*)(&tmp[(8*j+6)*32]), mm128(ymm3));
                _mm_storeh_epi64((__m128i*)(&tmp[(8*j+7)*32]), mm128(ymm3));
            }
            tmp += 4;
            num = 0;
        }
        src += 32;
    }

    /* vertical pass */
    num = 0;
    tmp = temp;
    for (i = 0; i < 32; i++) {
        /* tmp should be 32-byte aligned */
        tmp = temp + 32*DCT32TabHShuf[i];
        ymm0 =  _mm256_load_si256((__m256i *)(tmp + 8*0));
        ymm2 =  _mm256_load_si256((__m256i *)(tmp + 8*2));

        ymm2 = _mm256_shuffle_epi32(ymm2, 0x4e);
        ymm2 = _mm256_shufflelo_epi16(ymm2, 0x1b); 
        ymm2 = _mm256_shufflehi_epi16(ymm2, 0x1b);

        ymm1 = _mm256_permute2x128_si256(ymm0, ymm0, 0x11);
        ymm3 = _mm256_permute2x128_si256(ymm2, ymm2, 0x11);
        ymm0 = _mm256_permute2x128_si256(ymm0, ymm0, 0x00);
        ymm2 = _mm256_permute2x128_si256(ymm2, ymm2, 0x00);

        ymm6 = round2;
        ymm7 = round2;

        /* combine first 2 stages */
        for (j = 0; j < 4; j++) {
            ymm4 = _mm256_madd_epi16(ymm0, *(__m256i *)DCT32TabV_0[4*j+0]);
            ymm5 = _mm256_madd_epi16(ymm3, *(__m256i *)DCT32TabV_0[4*j+0]);
            ymm6 = _mm256_add_epi32(ymm6, ymm4);
            ymm6 = _mm256_sub_epi32(ymm6, ymm5);

            ymm4 = _mm256_madd_epi16(ymm1, *(__m256i *)DCT32TabV_0[4*j+1]);
            ymm5 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT32TabV_0[4*j+1]);
            ymm6 = _mm256_add_epi32(ymm6, ymm4);
            ymm6 = _mm256_sub_epi32(ymm6, ymm5);

            ymm4 = _mm256_madd_epi16(ymm0, *(__m256i *)DCT32TabV_0[4*j+2]);
            ymm5 = _mm256_madd_epi16(ymm3, *(__m256i *)DCT32TabV_0[4*j+2]);
            ymm7 = _mm256_add_epi32(ymm7, ymm4);
            ymm7 = _mm256_sub_epi32(ymm7, ymm5);

            ymm4 = _mm256_madd_epi16(ymm1, *(__m256i *)DCT32TabV_0[4*j+3]);
            ymm5 = _mm256_madd_epi16(ymm2, *(__m256i *)DCT32TabV_0[4*j+3]);
            ymm7 = _mm256_add_epi32(ymm7, ymm4);
            ymm7 = _mm256_sub_epi32(ymm7, ymm5);

            ymm0 = _mm256_shuffle_epi32(ymm0, 0x39);
            ymm1 = _mm256_shuffle_epi32(ymm1, 0x39);
            ymm2 = _mm256_shuffle_epi32(ymm2, 0x39);
            ymm3 = _mm256_shuffle_epi32(ymm3, 0x39);
        }

        ymm6 = _mm256_srai_epi32(ymm6, SHIFT_FRW32_2ND);
        ymm7 = _mm256_srai_epi32(ymm7, SHIFT_FRW32_2ND);
        ymm6 = _mm256_packs_epi32(ymm6, ymm7);
        ymm6 = _mm256_permute4x64_epi64(ymm6, 0xd8);

        buff[4*0 + num] = mm128(ymm6);
        ymm6 = _mm256_permute2x128_si256(ymm6, ymm6, 0x01);
        buff[4*1 + num] = mm128(ymm6);

        /* third stage */
        ymm0 = _mm256_cvtepi16_epi32(mm128(ymm0));
        ymm1 = _mm256_cvtepi16_epi32(mm128(ymm1));
        ymm2 = _mm256_cvtepi16_epi32(mm128(ymm2));
        ymm3 = _mm256_cvtepi16_epi32(mm128(ymm3));

        ymm0 = _mm256_add_epi32(ymm0, ymm3);
        ymm1 = _mm256_add_epi32(ymm1, ymm2);
        ymm1 = _mm256_shuffle_epi32(ymm1, 0x1b);

        ymm5 = _mm256_permute2x128_si256(ymm1, ymm1, 0x01);
        ymm4 = ymm0;
        ymm0 = _mm256_sub_epi32(ymm0, ymm5);
        ymm5 = _mm256_add_epi32(ymm5, ymm4);

        ymm4 = _mm256_permute2x128_si256(ymm0, ymm0, 0x11);
        ymm0 = _mm256_permute2x128_si256(ymm0, ymm0, 0x00);
        ymm6 = round2;

        for (j = 0; j < 4; j++) {
            ymm2 = _mm256_mullo_epi32(ymm0, *(__m256i *)DCT32TabV_2[2*j+0]);
            ymm6 = _mm256_add_epi32(ymm6, ymm2);

            ymm2 = _mm256_mullo_epi32(ymm4, *(__m256i *)DCT32TabV_2[2*j+1]);
            ymm6 = _mm256_add_epi32(ymm6, ymm2);

            ymm0 = _mm256_shuffle_epi32(ymm0, 0x39);
            ymm4 = _mm256_shuffle_epi32(ymm4, 0x39);
        }
        ymm6 = _mm256_srai_epi32(ymm6, SHIFT_FRW32_2ND);
        ymm6 = _mm256_packs_epi32(ymm6, ymm6);
        ymm6 = _mm256_permute4x64_epi64(ymm6, 0xd8);
        buff[4*2 + num] = mm128(ymm6);

        /* fourth stage */
        ymm1 = _mm256_permute2x128_si256(ymm5, ymm5, 0x01);
        ymm1 = _mm256_shuffle_epi32(ymm1, 0x1b);
        ymm7 = ymm5;
        ymm5 = _mm256_sub_epi32(ymm5, ymm1);
        ymm1 = _mm256_add_epi32(ymm1, ymm7);
        ymm1 = _mm256_permute2x128_si256(ymm1, ymm5, 0x20);

        ymm6 = round2;
        for (j = 0; j < 4; j++) {
            ymm0 = _mm256_mullo_epi32(ymm1, *(__m256i *)DCT32TabV_3[j]);
            ymm6 = _mm256_add_epi32(ymm6, ymm0);
            ymm1 = _mm256_shuffle_epi32(ymm1, 0x39);
        }
        ymm6 = _mm256_srai_epi32(ymm6, SHIFT_FRW32_2ND);
        ymm6 = _mm256_packs_epi32(ymm6, ymm6);
        ymm6 = _mm256_permute4x64_epi64(ymm6, 0xd8);
        buff[4*3 + num] = mm128(ymm6);

        num++;
        if (num == 4) {
            sPos = DCT32TabVShuf;
            for (j = 0; j < 4; j++) {
                ymm0 = _mm256_load_si256((__m256i *)(buff + 4*j + 0));    // 00 01 02 03 04 05 06 07 | 10 11 12 13 14 15 16 17
                ymm2 = _mm256_load_si256((__m256i *)(buff + 4*j + 2));    // 20 21 22 23 24 25 26 27 | 30 31 32 33 34 35 36 37

                ymm1 = _mm256_unpacklo_epi16(ymm0, ymm2);                 // 00 20 01 21 02 22 03 23 | 10 30 11 31 12 32 13 33
                ymm3 = _mm256_unpackhi_epi16(ymm0, ymm2);                 // 04 24 05 25 06 26 07 27 | 14 34 15 35 16 36 17 37

                ymm0 = _mm256_permute2x128_si256(ymm1, ymm3, 0x20);       // 00 20 01 21 02 22 03 23 | 04 24 05 25 06 26 07 27
                ymm2 = _mm256_permute2x128_si256(ymm1, ymm3, 0x31);       // 10 30 11 31 12 32 13 33 | 14 34 15 35 16 36 17 37
                
                ymm1 = _mm256_unpackhi_epi16(ymm0, ymm2);                 // 02 12 22 32 03 13 23 33 | 06 16 26 36 07 17 27 37
                ymm0 = _mm256_unpacklo_epi16(ymm0, ymm2);                 // 00 10 20 30 01 11 21 31 | 04 14 24 34 05 15 25 35
                ymm2 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);
                ymm3 = _mm256_permute2x128_si256(ymm1, ymm1, 0x01);

                _mm_storel_epi64((__m128i*)(&dest[(*sPos++)*32]), mm128(ymm0));
                _mm_storeh_epi64((__m128i*)(&dest[(*sPos++)*32]), mm128(ymm0));
                _mm_storel_epi64((__m128i*)(&dest[(*sPos++)*32]), mm128(ymm1));
                _mm_storeh_epi64((__m128i*)(&dest[(*sPos++)*32]), mm128(ymm1));
                _mm_storel_epi64((__m128i*)(&dest[(*sPos++)*32]), mm128(ymm2));
                _mm_storeh_epi64((__m128i*)(&dest[(*sPos++)*32]), mm128(ymm2));
                _mm_storel_epi64((__m128i*)(&dest[(*sPos++)*32]), mm128(ymm3));
                _mm_storeh_epi64((__m128i*)(&dest[(*sPos++)*32]), mm128(ymm3));
            }
            num = 0;
            dest += 4;
        }
        tmp += 32;
    }
}
#endif // #if defined(AVX2_FTR_32x32_V1_ENABLED)

} // end namespace MFX_HEVC_PP

#endif // #if defined (MFX_TARGET_OPTIMIZATION_SSE4)
#endif // #if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
/* EOF */
