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
// Sizes: 4, 8, 16, 32
/* NOTE - this implementation requires SSE4.1 (PINSRD, PEXTRD, PMOVZWBW) */

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"
#include "mfx_h265_transform_consts.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_ATOM) && defined(MFX_TARGET_OPTIMIZATION_ATOM) || \
    defined(MFX_MAKENAME_SSE4) && defined(MFX_TARGET_OPTIMIZATION_SSE4) || \
    defined(MFX_MAKENAME_SSSE3) && defined(MFX_TARGET_OPTIMIZATION_SSSE3)

#include <immintrin.h>
#ifdef MFX_EMULATE_SSSE3
#include "mfx_ssse3_emulation.h"
#endif

#pragma warning (disable : 4310 ) /* disable cast truncates constant value */

namespace MFX_HEVC_PP
{

#define SHIFT_INV_1ST          7 // Shift after first inverse transform stage
#define SHIFT_INV_2ND_BASE     12   // Base shift after second inverse transform stage, offset later by (bitDepth - 8) for > 8-bit data
#define REG_DCT               65535

//    NOTE: In debug mode compiler attempts to load data with MOVNTDQA while data is
//    only 8-byte aligned, but PMOVZX does not require 16-byte alignment.

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

#define M128I_W8C(x, w0,w1,w2,w3,w4,w5,w6,w7) M128I_VAR(x, M128I_W8C_INIT(w0,w1,w2,w3,w4,w5,w6,w7))

#define M128I_DC(x, v) M128I_VAR(x, M128I_DC_INIT(v))

#define M128I_D4C(x, w0,w1,w2,w3) M128I_VAR(x, M128I_D4C_INIT(w0,w1,w2,w3))

//---------------------------------------------------------

#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))

#define _mm_storeh_epi64(p, A) _mm_storeh_pd((double *)(p), _mm_castsi128_pd(A))

#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (double *)(p)))


#ifdef _INCLUDED_PMM
#define load_unaligned(x) _mm_lddqu_si128(x)
#else
#define load_unaligned(x) _mm_loadu_si128(x)
#endif

//---------------------------------------------------------

#undef coef_stride
#define coef_stride 4

    template <int bitDepth, typename PredPixType, typename DstCoeffsType, bool inplace>
    static void h265_DCT4x4Inv_16sT_Kernel(const PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
    {
        __m128i s0, s1, s2, s3, s02L, s13L, O0L, O1L, E0L, E1L, H01L, H23L;
        __m128i R0, R1, R2, R3;
        __m128i xmm0, xmm1, xmm2, xmm3;
        __m128i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
        const int SHIFT_INV_2ND = (SHIFT_INV_2ND_BASE - (bitDepth - 8));

        M128I_W2x4C( t_4_O0_13,  83,  36); // g_T4[1][0], g_T4[3][0]
        M128I_W2x4C( t_4_O1_13,  36, -83); // g_T4[1][1], g_T4[3][1]
        M128I_W2x4C( t_4_E0_02,  64,  64); // g_T4[0][0], g_T4[2][0]
        M128I_W2x4C( t_4_E1_02,  64, -64); // g_T4[0][1], g_T4[2][1]

        M128I_DC (round1, 1<<(SHIFT_INV_1ST-1));

        M128I_W8C(t_dct4_A,  64, 83,   64, 36,  64,-36,  64,-83);
        M128I_W8C(t_dct4_B,  64, 36,  -64,-83, -64, 83,  64,-36);

        M128I_DC (round2, 1<<(SHIFT_INV_2ND-1));

        // Utilizing symmetry properties to the maximum to minimize the number of multiplications
        s1 = _mm_loadl_epi64((const __m128i *)(coeff + 1*coef_stride));
        s3 = _mm_loadl_epi64((const __m128i *)(coeff + 3*coef_stride));
        s13L = _mm_unpacklo_epi16(s1, s3);
        O0L = _mm_madd_epi16(s13L, t_4_O0_13);
        O1L = _mm_madd_epi16(s13L, t_4_O1_13);

        s0 = _mm_loadl_epi64((const __m128i *)(coeff + 0*coef_stride));
        s2 = _mm_loadl_epi64((const __m128i *)(coeff + 2*coef_stride));
        s02L = _mm_unpacklo_epi16(s0, s2);
        E0L = _mm_add_epi32( _mm_madd_epi16( s02L, t_4_E0_02), round1);
        E1L = _mm_add_epi32( _mm_madd_epi16( s02L, t_4_E1_02), round1);

        // Combining even and odd terms at each hierarchy levels to calculate the final spatial domain vector
        R0 = _mm_srai_epi32(_mm_add_epi32(E0L, O0L), SHIFT_INV_1ST);
        R3 = _mm_srai_epi32(_mm_sub_epi32(E0L, O0L), SHIFT_INV_1ST);
        R1 = _mm_srai_epi32(_mm_add_epi32(E1L, O1L), SHIFT_INV_1ST);
        R2 = _mm_srai_epi32(_mm_sub_epi32(E1L, O1L), SHIFT_INV_1ST);

        H01L = _mm_packs_epi32(R0, R1);
        H23L = _mm_packs_epi32(R2, R3);

        // first part is done, transpose 4x4 and go to second part
        //H01L : s31 s21 s11 s01 s30 s20 s10 s00
        //H23L : s33 s23 s13 s03 s32 s22 s12 s02
        // second part
        tmp0 = _mm_shuffle_epi32(H01L, _MM_SHUFFLE(0,0,0,0));
        tmp1 = _mm_shuffle_epi32(H01L, _MM_SHUFFLE(1,1,1,1));
        tmp2 = _mm_shuffle_epi32(H01L, _MM_SHUFFLE(2,2,2,2));
        tmp3 = _mm_shuffle_epi32(H01L, _MM_SHUFFLE(3,3,3,3));

        xmm0 = _mm_add_epi32(_mm_madd_epi16(tmp0, t_dct4_A), _mm_madd_epi16(tmp1, t_dct4_B));
        xmm1 = _mm_add_epi32(_mm_madd_epi16(tmp2, t_dct4_A), _mm_madd_epi16(tmp3, t_dct4_B));

        xmm0 = _mm_srai_epi32( _mm_add_epi32(xmm0, round2), SHIFT_INV_2ND);
        xmm1 = _mm_srai_epi32( _mm_add_epi32(xmm1, round2), SHIFT_INV_2ND);

        xmm0 = _mm_packs_epi32(xmm0, xmm1);

        tmp4 = _mm_shuffle_epi32(H23L, _MM_SHUFFLE(0,0,0,0));
        tmp5 = _mm_shuffle_epi32(H23L, _MM_SHUFFLE(1,1,1,1));
        tmp6 = _mm_shuffle_epi32(H23L, _MM_SHUFFLE(2,2,2,2));
        tmp7 = _mm_shuffle_epi32(H23L, _MM_SHUFFLE(3,3,3,3));

        xmm2 = _mm_add_epi32( _mm_madd_epi16(tmp4, t_dct4_A), _mm_madd_epi16(tmp5, t_dct4_B));
        xmm3 = _mm_add_epi32( _mm_madd_epi16(tmp6, t_dct4_A), _mm_madd_epi16(tmp7, t_dct4_B));

        xmm2 = _mm_srai_epi32( _mm_add_epi32(xmm2, round2), SHIFT_INV_2ND);
        xmm3 = _mm_srai_epi32( _mm_add_epi32(xmm3, round2), SHIFT_INV_2ND);

        xmm2 = _mm_packs_epi32(xmm2, xmm3);

        if (inplace && sizeof(DstCoeffsType) == 1) {
            /* load 4 bytes from row 0 and row 1, expand to 16 bits, add temp values, clip/pack to 8 bytes */
            tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 0*predStride), 0);    /* load row 0 (bytes) */
            tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 1*predStride), 1);    /* load row 1 (bytes) */
            tmp0 = _mm_cvtepu8_epi16(tmp0);                                    /* expand to 16 bytes */
            tmp0 = _mm_add_epi16(tmp0, xmm0);                                  /* add to dst */
            tmp0 = _mm_packus_epi16(tmp0, tmp0);                               /* pack to 8 bytes */
            *(int *)(dst + 0*destStride) = _mm_extract_epi32(tmp0, 0);         /* store row 0 (bytes) */
            *(int *)(dst + 1*destStride) = _mm_extract_epi32(tmp0, 1);         /* store row 1 (bytes) */

            /* repeat for rows 2 and 3 */
            tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 2*predStride), 0);
            tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 3*predStride), 1);
            tmp0 = _mm_cvtepu8_epi16(tmp0);
            tmp0 = _mm_add_epi16(tmp0, xmm2);
            tmp0 = _mm_packus_epi16(tmp0, tmp0);
            *(int *)(dst + 2*destStride) = _mm_extract_epi32(tmp0, 0);
            *(int *)(dst + 3*destStride) = _mm_extract_epi32(tmp0, 1);
        } else if (inplace && sizeof(DstCoeffsType) == 2) {
            /* load 4 words from each row, add temp values, clip to [0, 2^bitDepth) */
            tmp0 = _mm_loadl_epi64((__m128i *)(pred + 0*predStride));
            tmp1 = _mm_loadl_epi64((__m128i *)(pred + 1*predStride));
            tmp2 = _mm_loadl_epi64((__m128i *)(pred + 2*predStride));
            tmp3 = _mm_loadl_epi64((__m128i *)(pred + 3*predStride));

            tmp0 = _mm_unpacklo_epi64(tmp0, tmp1);
            tmp2 = _mm_unpacklo_epi64(tmp2, tmp3);

            /* signed saturating add to 16-bit, then clip to bitDepth */
            tmp0 = _mm_adds_epi16(tmp0, xmm0);
            tmp2 = _mm_adds_epi16(tmp2, xmm2);

            tmp4 = _mm_setzero_si128();
            tmp5 = _mm_set1_epi16((1 << bitDepth) - 1);

            tmp0 = _mm_max_epi16(tmp0, tmp4); tmp0 = _mm_min_epi16(tmp0, tmp5);
            tmp2 = _mm_max_epi16(tmp2, tmp4); tmp2 = _mm_min_epi16(tmp2, tmp5);

            _mm_storel_epi64((__m128i*)(dst + 0*destStride), tmp0);        /* store row 0 (words) */
            _mm_storeh_epi64((__m128i*)(dst + 1*destStride), tmp0);        /* store row 1 (words) */
            _mm_storel_epi64((__m128i*)(dst + 2*destStride), tmp2);        /* store row 2 (words) */
            _mm_storeh_epi64((__m128i*)(dst + 3*destStride), tmp2);        /* store row 3 (words) */
        } else {
            _mm_storel_epi64((__m128i*)(dst + 0*destStride), xmm0);        /* store row 0 (words) */
            _mm_storeh_epi64((__m128i*)(dst + 1*destStride), xmm0);        /* store row 1 (words) */
            _mm_storel_epi64((__m128i*)(dst + 2*destStride), xmm2);        /* store row 2 (words) */
            _mm_storeh_epi64((__m128i*)(dst + 3*destStride), xmm2);        /* store row 3 (words) */
        }
    }

    void MAKE_NAME(h265_DCT4x4Inv_16sT)(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth)
    {
        if (inplace) {
            switch (bitDepth) {
            case  8:
                if (inplace == 2)
                    h265_DCT4x4Inv_16sT_Kernel< 8, Ipp8u, Ipp16u,  true >((Ipp8u *)pred, predStride, (Ipp16u *) destPtr, coeff, destStride);
                else
                    h265_DCT4x4Inv_16sT_Kernel< 8, Ipp8u, Ipp8u,  true >((Ipp8u *)pred, predStride, (Ipp8u *) destPtr, coeff, destStride);
                break;
            case  9: h265_DCT4x4Inv_16sT_Kernel< 9, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
            case 10: h265_DCT4x4Inv_16sT_Kernel<10, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
            case 11: h265_DCT4x4Inv_16sT_Kernel<11, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
            case 12: h265_DCT4x4Inv_16sT_Kernel<12, Ipp16u, Ipp16u, true >((Ipp16u *)pred, predStride, (Ipp16u *)destPtr, coeff, destStride); break;
            }
        } else {
            switch (bitDepth) {
            case  8: h265_DCT4x4Inv_16sT_Kernel< 8, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0,(Ipp16s *)destPtr, coeff, destStride); break;
            case  9: h265_DCT4x4Inv_16sT_Kernel< 9, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0,(Ipp16s *)destPtr, coeff, destStride); break;
            case 10: h265_DCT4x4Inv_16sT_Kernel<10, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0,(Ipp16s *)destPtr, coeff, destStride); break;
            case 11: h265_DCT4x4Inv_16sT_Kernel<11, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0,(Ipp16s *)destPtr, coeff, destStride); break;
            case 12: h265_DCT4x4Inv_16sT_Kernel<12, Ipp8u, Ipp16s, false>((Ipp8u *)NULL, 0,(Ipp16s *)destPtr, coeff, destStride); break;
            }
        }
    }

    template <int bitDepth, typename PredPixType, typename DstCoeffsType, bool inplace>
    static void h265_DST4x4Inv_16sT_Kernel(const PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
    {
        __m128i s0, s1, s2, s3, s01L, s23L, H01L, H23L;
        __m128i R0, R1, R2, R3;
        __m128i xmm0, xmm1, xmm2, xmm3;
        __m128i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
        const int SHIFT_INV_2ND = (SHIFT_INV_2ND_BASE - (bitDepth - 8));

        //    dst[i + 0*4] = (short)Clip3( -32768, 32767, ( 29*s0 + 74*s1 + 84*s2 + 55*s3 + rnd_factor ) >> SHIFT_INV_1ST );
        //    dst[i + 1*4] = (short)Clip3( -32768, 32767, ( 55*s0 + 74*s1 - 29*s2 - 84*s3 + rnd_factor ) >> SHIFT_INV_1ST );
        //    dst[i + 2*4] = (short)Clip3( -32768, 32767, ( 74*s0 +  0*s1 - 74*s2 + 74*s3 + rnd_factor ) >> SHIFT_INV_1ST );
        //    dst[i + 3*4] = (short)Clip3( -32768, 32767, ( 84*s0 - 74*s1 + 55*s2 - 29*s3 + rnd_factor ) >> SHIFT_INV_1ST );
        M128I_W2x4C( t_4dst_0_01,  29,  74);
        M128I_W2x4C( t_4dst_1_01,  55,  74);
        M128I_W2x4C( t_4dst_2_01,  74,   0);
        M128I_W2x4C( t_4dst_3_01,  84, -74);
        M128I_W2x4C( t_4dst_0_23,  84,  55);
        M128I_W2x4C( t_4dst_1_23, -29, -84);
        M128I_W2x4C( t_4dst_2_23, -74,  74);
        M128I_W2x4C( t_4dst_3_23,  55, -29);

        M128I_DC (round1, 1<<(SHIFT_INV_1ST-1));

        M128I_W8C(t_dst4_A,  29, 74,   55, 74,  74,  0,  84,-74);
        M128I_W8C(t_dst4_B,  84, 55,  -29,-84, -74, 74,  55,-29);

        M128I_DC (round2, 1<<(SHIFT_INV_2ND-1));

        s0 = _mm_loadl_epi64((const __m128i *)(coeff + 0*coef_stride));
        s1 = _mm_loadl_epi64((const __m128i *)(coeff + 1*coef_stride));
        s01L = _mm_unpacklo_epi16 (s0, s1); // s13 s03 s12 s02 s11 s01 s10 s00

        s2 = _mm_loadl_epi64((const __m128i *)(coeff + 2*coef_stride));
        s3 = _mm_loadl_epi64((const __m128i *)(coeff + 3*coef_stride));
        s23L = _mm_unpacklo_epi16 (s2, s3); // s23 s23 s22 s22 s31 s21 s30 s20

        R0 = _mm_add_epi32( _mm_madd_epi16(s01L, t_4dst_0_01), _mm_madd_epi16(s23L, t_4dst_0_23));
        R1 = _mm_add_epi32( _mm_madd_epi16(s01L, t_4dst_1_01), _mm_madd_epi16(s23L, t_4dst_1_23));
        R2 = _mm_add_epi32( _mm_madd_epi16(s01L, t_4dst_2_01), _mm_madd_epi16(s23L, t_4dst_2_23));
        R3 = _mm_add_epi32( _mm_madd_epi16(s01L, t_4dst_3_01), _mm_madd_epi16(s23L, t_4dst_3_23));

        R0 = _mm_srai_epi32( _mm_add_epi32( R0, round1), SHIFT_INV_1ST);
        R1 = _mm_srai_epi32( _mm_add_epi32( R1, round1), SHIFT_INV_1ST);
        R2 = _mm_srai_epi32( _mm_add_epi32( R2, round1), SHIFT_INV_1ST);
        R3 = _mm_srai_epi32( _mm_add_epi32( R3, round1), SHIFT_INV_1ST);

        H01L = _mm_packs_epi32(R0, R1);
        H23L = _mm_packs_epi32(R2, R3);

        // R23L : s33 s32 s31 s30 s23 s22 s21 s20
        // R01L : s13 s12 s11 s10 s03 s02 s01 s00
        tmp0 = _mm_shuffle_epi32(H01L, _MM_SHUFFLE(0,0,0,0));
        tmp1 = _mm_shuffle_epi32(H01L, _MM_SHUFFLE(1,1,1,1));
        tmp2 = _mm_shuffle_epi32(H01L, _MM_SHUFFLE(2,2,2,2));
        tmp3 = _mm_shuffle_epi32(H01L, _MM_SHUFFLE(3,3,3,3));

        xmm0 = _mm_add_epi32(_mm_madd_epi16(tmp0, t_dst4_A), _mm_madd_epi16(tmp1, t_dst4_B));
        xmm1 = _mm_add_epi32(_mm_madd_epi16(tmp2, t_dst4_A), _mm_madd_epi16(tmp3, t_dst4_B));

        xmm0 = _mm_srai_epi32( _mm_add_epi32(xmm0, round2), SHIFT_INV_2ND);
        xmm1 = _mm_srai_epi32( _mm_add_epi32(xmm1, round2), SHIFT_INV_2ND);

        xmm0 = _mm_packs_epi32(xmm0, xmm1);

        tmp4 = _mm_shuffle_epi32(H23L, _MM_SHUFFLE(0,0,0,0));
        tmp5 = _mm_shuffle_epi32(H23L, _MM_SHUFFLE(1,1,1,1));
        tmp6 = _mm_shuffle_epi32(H23L, _MM_SHUFFLE(2,2,2,2));
        tmp7 = _mm_shuffle_epi32(H23L, _MM_SHUFFLE(3,3,3,3));

        xmm2 = _mm_add_epi32( _mm_madd_epi16(tmp4, t_dst4_A), _mm_madd_epi16(tmp5, t_dst4_B));
        xmm3 = _mm_add_epi32( _mm_madd_epi16(tmp6, t_dst4_A), _mm_madd_epi16(tmp7, t_dst4_B));

        xmm2 = _mm_srai_epi32( _mm_add_epi32(xmm2, round2), SHIFT_INV_2ND);
        xmm3 = _mm_srai_epi32( _mm_add_epi32(xmm3, round2), SHIFT_INV_2ND);

        xmm2 = _mm_packs_epi32(xmm2, xmm3);

        if (inplace && sizeof(DstCoeffsType) == 1) {
            /* load 4 bytes from row 0 and row 1, expand to 16 bits, add temp values, clip/pack to 8 bytes */
            tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 0*predStride), 0);    /* load row 0 (bytes) */
            tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 1*predStride), 1);    /* load row 1 (bytes) */
            tmp0 = _mm_cvtepu8_epi16(tmp0);                                    /* expand to 16 bytes */
            tmp0 = _mm_add_epi16(tmp0, xmm0);                                  /* add to dst */
            tmp0 = _mm_packus_epi16(tmp0, tmp0);                               /* pack to 8 bytes */
            *(int *)(dst + 0*destStride) = _mm_extract_epi32(tmp0, 0);         /* store row 0 (bytes) */
            *(int *)(dst + 1*destStride) = _mm_extract_epi32(tmp0, 1);         /* store row 1 (bytes) */

            /* repeat for rows 2 and 3 */
            tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 2*predStride), 0);
            tmp0 = _mm_insert_epi32(tmp0, *(int *)(pred + 3*predStride), 1);
            tmp0 = _mm_cvtepu8_epi16(tmp0);
            tmp0 = _mm_add_epi16(tmp0, xmm2);
            tmp0 = _mm_packus_epi16(tmp0, tmp0);
            *(int *)(dst + 2*destStride) = _mm_extract_epi32(tmp0, 0);
            *(int *)(dst + 3*destStride) = _mm_extract_epi32(tmp0, 1);
        } else if (inplace && sizeof(DstCoeffsType) == 2) {
            /* load 4 words from each row, add temp values, clip to [0, 2^bitDepth) */
            tmp0 = _mm_loadl_epi64((__m128i *)(pred + 0*predStride));
            tmp1 = _mm_loadl_epi64((__m128i *)(pred + 1*predStride));
            tmp2 = _mm_loadl_epi64((__m128i *)(pred + 2*predStride));
            tmp3 = _mm_loadl_epi64((__m128i *)(pred + 3*predStride));

            tmp0 = _mm_unpacklo_epi64(tmp0, tmp1);
            tmp2 = _mm_unpacklo_epi64(tmp2, tmp3);

            /* signed saturating add to 16-bit, then clip to bitDepth */
            tmp0 = _mm_adds_epi16(tmp0, xmm0);
            tmp2 = _mm_adds_epi16(tmp2, xmm2);

            tmp4 = _mm_setzero_si128();
            tmp5 = _mm_set1_epi16((1 << bitDepth) - 1);

            tmp0 = _mm_max_epi16(tmp0, tmp4); tmp0 = _mm_min_epi16(tmp0, tmp5);
            tmp2 = _mm_max_epi16(tmp2, tmp4); tmp2 = _mm_min_epi16(tmp2, tmp5);

            _mm_storel_epi64((__m128i*)(dst + 0*destStride), tmp0);        /* store row 0 (words) */
            _mm_storeh_epi64((__m128i*)(dst + 1*destStride), tmp0);        /* store row 1 (words) */
            _mm_storel_epi64((__m128i*)(dst + 2*destStride), tmp2);        /* store row 2 (words) */
            _mm_storeh_epi64((__m128i*)(dst + 3*destStride), tmp2);        /* store row 3 (words) */
        } else {
            _mm_storel_epi64((__m128i*)(dst + 0*destStride), xmm0);        /* store row 0 (words) */
            _mm_storeh_epi64((__m128i*)(dst + 1*destStride), xmm0);        /* store row 1 (words) */
            _mm_storel_epi64((__m128i*)(dst + 2*destStride), xmm2);        /* store row 2 (words) */
            _mm_storeh_epi64((__m128i*)(dst + 3*destStride), xmm2);        /* store row 3 (words) */
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
    static void h265_DCT8x8Inv_16sT_Kernel(PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
    {
        ALIGN_DECL(16) short tmp[8 * 8];

        __m128i s, s0, s1, s2, s3, s4, s5, s6, s7;
        __m128i s13L, s57L, s04L, s26L;
        __m128i O0L, O1L, O2L, O3L, E0L, E1L, E2L, E3L;
        __m128i EO0L, EO1L, EE0L, EE1L;
        __m128i R07, R16, R25, R34;
        __m128i d10c10, d32c32, c3210, d3210;
        __m128i r4567, r3210, r7654, r76543210;
        const int SHIFT_INV_2ND = (SHIFT_INV_2ND_BASE - (bitDepth - 8));
        int j;

        signed short *H265_RESTRICT p_tmp = tmp;

        for (j = 0; j < 8; j += 4)
        {
            M128I_W2x4C( t_8_O0_13,  89, 75); // g_T8[1][0], g_T8[3][0]
            M128I_W2x4C( t_8_O0_57,  50, 18); // g_T8[5][0], g_T8[7][0]
            M128I_W2x4C( t_8_O1_13,  75,-18); // g_T8[1][1], g_T8[3][1]
            M128I_W2x4C( t_8_O1_57, -89,-50); // g_T8[5][1], g_T8[7][1]
            M128I_W2x4C( t_8_O2_13,  50,-89); // g_T8[1][2], g_T8[3][2]
            M128I_W2x4C( t_8_O2_57,  18, 75); // g_T8[5][2], g_T8[7][2]
            M128I_W2x4C( t_8_O3_13,  18,-50); // g_T8[1][3], g_T8[3][3]
            M128I_W2x4C( t_8_O3_57,  75,-89); // g_T8[5][3], g_T8[7][3]

            M128I_W2x4C( t_8_EO0_26,  83, 36); // g_T8[2][0], g_T8[6][0]
            M128I_W2x4C( t_8_EO1_26,  36,-83); // g_T8[2][1], g_T8[6][1]
            M128I_W2x4C( t_8_EE0_04,  64, 64); // g_T8[0][0], g_T8[4][0]
            M128I_W2x4C( t_8_EE1_04,  64,-64); // g_T8[0][1], g_T8[4][1]

            M128I_DC ( round1, 1<<(SHIFT_INV_1ST-1));

            s1 = _mm_loadl_epi64((const __m128i *)(coeff + 1*coef_stride));
            s3 = _mm_loadl_epi64((const __m128i *)(coeff + 3*coef_stride));
            s13L = _mm_unpacklo_epi16( s1, s3);

            s5 = _mm_loadl_epi64((const __m128i *)(coeff + 5*coef_stride));
            s7 = _mm_loadl_epi64((const __m128i *)(coeff + 7*coef_stride));
            s57L = _mm_unpacklo_epi16( s5, s7);

            O0L = _mm_add_epi32( _mm_madd_epi16( s13L, t_8_O0_13), _mm_madd_epi16( s57L, t_8_O0_57));
            O1L = _mm_add_epi32( _mm_madd_epi16( s13L, t_8_O1_13), _mm_madd_epi16( s57L, t_8_O1_57));
            O2L = _mm_add_epi32( _mm_madd_epi16( s13L, t_8_O2_13), _mm_madd_epi16( s57L, t_8_O2_57));
            O3L = _mm_add_epi32( _mm_madd_epi16( s13L, t_8_O3_13), _mm_madd_epi16( s57L, t_8_O3_57));

            s0 = _mm_loadl_epi64((const __m128i *)(coeff + 0*coef_stride));
            s4 = _mm_loadl_epi64((const __m128i *)(coeff + 4*coef_stride));
            s04L = _mm_unpacklo_epi16( s0, s4);

            s2 = _mm_loadl_epi64((const __m128i *)(coeff + 2*coef_stride));
            s6 = _mm_loadl_epi64((const __m128i *)(coeff + 6*coef_stride));
            s26L = _mm_unpacklo_epi16( s2, s6);

            EO0L = _mm_madd_epi16( s26L, t_8_EO0_26);
            EO1L = _mm_madd_epi16( s26L, t_8_EO1_26);
            EE0L = _mm_add_epi32( _mm_madd_epi16( s04L, t_8_EE0_04), round1);
            EE1L = _mm_add_epi32( _mm_madd_epi16( s04L, t_8_EE1_04), round1);

            E0L = _mm_add_epi32( EE0L, EO0L);
            E3L = _mm_sub_epi32( EE0L, EO0L);
            E1L = _mm_add_epi32( EE1L, EO1L);
            E2L = _mm_sub_epi32( EE1L, EO1L);

            R07 = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E0L, O0L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E0L, O0L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 0*coef_stride), R07);
            _mm_storeh_epi64((__m128i *)(p_tmp + 7*coef_stride), R07);

            R16 = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E1L, O1L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E1L, O1L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 1*coef_stride), R16);
            _mm_storeh_epi64((__m128i *)(p_tmp + 6*coef_stride), R16);

            R25 = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E2L, O2L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E2L, O2L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 2*coef_stride), R25);
            _mm_storeh_epi64((__m128i *)(p_tmp + 5*coef_stride), R25);

            R34 = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E3L, O3L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E3L, O3L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 3*coef_stride), R34);
            _mm_storeh_epi64((__m128i *)(p_tmp + 4*coef_stride), R34);

            coeff += 4;
            p_tmp += 4;
        }

        p_tmp = tmp;

        for (j = 0; j < 8; ++j)
        {
            M128I_W8C( t_8x1_0,  64, 83, -64,-83,   89, 75, -89,-50);
            M128I_W8C( t_8x1_1,  64,-36,  64,-36,   50,-89,  75,-89);
            M128I_W8C( t_8x1_2,  64, 36,  64, 36,   50, 18,  75,-18);
            M128I_W8C( t_8x1_3, -64, 83,  64,-83,   18, 75,  18,-50);
            M128I_D4C( round2, 1<<(SHIFT_INV_2ND-1), 1<<(SHIFT_INV_2ND-1), 0, 0);

            s = _mm_load_si128((const __m128i *)p_tmp); // s7 s6 s5 s4 s3 s2 s1 s0
            s = _mm_shufflelo_epi16(s, _MM_SHUFFLE(3,1,2,0));
            s = _mm_shufflehi_epi16(s, _MM_SHUFFLE(3,1,2,0));
            s = _mm_shuffle_epi32(s, _MM_SHUFFLE(3,1,2,0)); // s7 s5 s3 s1 s6 s4 s2 s0

            // t_8x1_0 : D1_23 D0_01 C1_23 C0_01 coefs
            // t_8x1_1 : D3_23 D2_01 C3_23 C2_01 coefs
            d10c10 = _mm_add_epi32( _mm_madd_epi16(t_8x1_0, s), round2);     // D1 D0 (C1 + round) (C0 + round)
            d32c32 = _mm_add_epi32( _mm_madd_epi16(t_8x1_1, s), round2);     // D3 D2 (C3 + round) (C2 + round)

            s = _mm_shuffle_epi32(s, _MM_SHUFFLE(2,3,0,1)); // s3 s1 s7 s5 s2 s0 s6 s4

            // t_8x1_2: D1_01 D0_23 C1_01 C0_23 coefs
            // t_8x1_3: D3_01 D2_23 C3_01 C2_23 coefs
            d10c10 = _mm_add_epi32( d10c10, _mm_madd_epi16(t_8x1_2, s));       //  D1 D0 C1 C0
            d32c32 = _mm_add_epi32( d32c32, _mm_madd_epi16(t_8x1_3, s));       //  D3 D2 C3 C2

            c3210 = _mm_unpacklo_epi64(d10c10, d32c32);  //  C3 C2 C1 C0
            d3210 = _mm_unpackhi_epi64(d10c10, d32c32);  //  D3 D2 D1 D0

            r4567 = _mm_sub_epi32(c3210, d3210);      //   r4 r5 r6 r7
            r3210 = _mm_add_epi32(c3210, d3210);      //   r3 r2 r1 r0

            r7654 = _mm_shuffle_epi32(r4567, _MM_SHUFFLE(0,1,2,3)); // r7 r6 r5 r4

            r76543210 = _mm_packs_epi32( _mm_srai_epi32(r3210, SHIFT_INV_2ND), _mm_srai_epi32(r7654, SHIFT_INV_2ND));         //  r7 r6 r5 r4 r3 r2 r1 r0

            if (inplace && sizeof(DstCoeffsType) == 1) {
                /* load dst[0-7], expand to 16 bits, add temp[0-7], clip/pack to 8 bytes */
                s = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&pred[0]));
                s = _mm_add_epi16(s, r76543210);
                s = _mm_packus_epi16(s, s);
                _mm_storel_epi64((__m128i *)&dst[0], s);        /* store dst[0-7] (bytes) */
                dst += destStride;
                pred += predStride;
            } else if (inplace && sizeof(DstCoeffsType) == 2) {
                /* load 8 words from each row, add temp values, clip to [0, 2^bitDepth) */
                s = _mm_loadu_si128((__m128i *)pred);
                s = _mm_adds_epi16(s, r76543210);

                s4 = _mm_setzero_si128();
                s5 = _mm_set1_epi16((1 << bitDepth) - 1);
                s  = _mm_max_epi16(s, s4);
                s  = _mm_min_epi16(s, s5);

                _mm_storeu_si128((__m128i *)dst, s);    /* store temp[0-7] (words) */
                dst += destStride;
                pred += predStride;
            } else {
                _mm_storeu_si128((__m128i *)dst, r76543210);    /* store temp[0-7] (words) */
                dst += destStride;
                pred += predStride;
            }

            p_tmp += 8;
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
    static inline H265_FORCEINLINE void IDCT_16x16_2ND_SSE4(PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
    {
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
        int i;
        const int SHIFT_INV_2ND = (SHIFT_INV_2ND_BASE - (bitDepth - 8));

        M128I_W8C( t_16_a0101_08,  64, 64,  64,-64,  64,-64,  64, 64);
        M128I_W8C( t_16_b0110_4C,  83, 36,  36,-83, -36, 83, -83,-36);
        M128I_W8C( t_16_d0123_2A,  89, 50,  75,-89,  50, 18,  18, 75);
        M128I_W8C( t_16_d0123_6E,  75, 18, -18,-50, -89, 75, -50,-89);
        M128I_W8C( t_16_f0123_01,  90, 87,  87, 57,  80,  9,  70,-43);
        M128I_W8C( t_16_f4567_01,  57,-80,  43,-90,  25,-70,   9,-25);
        M128I_W8C( t_16_f0123_23,  80, 70,   9,-43, -70,-87, -87,  9);
        M128I_W8C( t_16_f4567_23, -25, 90,  57, 25,  90,-80,  43,-57);
        M128I_W8C( t_16_f0123_45,  57, 43, -80,-90, -25, 57,  90, 25);
        M128I_W8C( t_16_f4567_45,  -9,-87, -87, 70,  43,  9,  70,-80);
        M128I_W8C( t_16_f0123_67,  25,  9, -70,-25,  90, 43, -80,-57);
        M128I_W8C( t_16_f4567_67,  43, 70,   9,-80, -57, 87,  87,-90);

        M128I_DC ( round2, 1<<(SHIFT_INV_2ND-1));

        for(i=0; i<16; ++i)
        {
            __m128i s_even, s_odd;
            {
                __m128i tmp0 = _mm_load_si128((const __m128i *)coeff);           // s7  s6  s5  s4  s3  s2  s1 s0
                __m128i tmp1 = _mm_load_si128((const __m128i *)(coeff+8));       // s15 s14 s13 s12 s11 s10 s9 s8

                tmp0 = _mm_shufflelo_epi16( tmp0, _MM_SHUFFLE(3,1,2,0));
                tmp1 = _mm_shufflelo_epi16( tmp1, _MM_SHUFFLE(3,1,2,0));
                tmp0 = _mm_shufflehi_epi16( tmp0, _MM_SHUFFLE(3,1,2,0));
                tmp1 = _mm_shufflehi_epi16( tmp1, _MM_SHUFFLE(3,1,2,0));
                tmp0 = _mm_shuffle_epi32( tmp0, _MM_SHUFFLE(3,1,2,0)); // s7  s5  s3  s1  s6  s4  s2 s0
                tmp1 = _mm_shuffle_epi32( tmp1, _MM_SHUFFLE(3,1,2,0)); // s15 s13 s11 s9 s14 s12 s10 s8

                s_odd  = _mm_unpackhi_epi64(tmp0, tmp1);  // s15 s13 s11 s9 s7  s5 s3 s1
                s_even = _mm_unpacklo_epi16(tmp0, tmp1);  // s14 s6  s12 s4 s10 s2 s8 s0
            }

            // s_even: s14  s6 s12  s4 s10  s2  s8  s0
            // s_odd : s15 s13 s11  s9  s7  s5  s3  s1
            xmm1 = _mm_shuffle_epi32( s_even, _MM_SHUFFLE(0,0,0,0)); // s8  s0
            xmm2 = _mm_shuffle_epi32( s_even, _MM_SHUFFLE(2,2,2,2)); // s12 s4
            xmm3 = _mm_shuffle_epi32( s_even, _MM_SHUFFLE(1,1,1,1)); // s10 s2
            xmm0 = _mm_shuffle_epi32( s_even, _MM_SHUFFLE(3,3,3,3)); // s14 s6
            xmm1 = _mm_madd_epi16( xmm1, t_16_a0101_08);
            xmm2 = _mm_madd_epi16( xmm2, t_16_b0110_4C);
            xmm3 = _mm_madd_epi16( xmm3, t_16_d0123_2A);
            xmm0 = _mm_madd_epi16( xmm0, t_16_d0123_6E);

            xmm1 = _mm_add_epi32( xmm1, round2);
            xmm3 = _mm_add_epi32( xmm3, xmm0);

            xmm1 = _mm_add_epi32( xmm1, xmm2);

            xmm2 = xmm1;
            xmm1 = _mm_sub_epi32( xmm1, xmm3);
            xmm2 = _mm_add_epi32( xmm2, xmm3);

            xmm3 = _mm_shuffle_epi32( xmm1, _MM_SHUFFLE(0,1,2,3));
            // xmm2 - e0
            // xmm3 - e1

            xmm4 = _mm_shuffle_epi32( s_odd, _MM_SHUFFLE(1,1,1,1));
            xmm5 = _mm_shuffle_epi32( s_odd, _MM_SHUFFLE(2,2,2,2));
            xmm1 = _mm_shuffle_epi32( s_odd, _MM_SHUFFLE(0,0,0,0));
            xmm7 = _mm_shuffle_epi32( s_odd, _MM_SHUFFLE(3,3,3,3));

            xmm0 = _mm_madd_epi16( xmm1, t_16_f0123_01);
            xmm1 = _mm_madd_epi16( xmm1, t_16_f4567_01);

            xmm6 = _mm_madd_epi16( xmm4, t_16_f0123_23);
            xmm4 = _mm_madd_epi16( xmm4, t_16_f4567_23);

            xmm0 = _mm_add_epi32( xmm0, xmm6);
            xmm1 = _mm_add_epi32( xmm1, xmm4);

            xmm6 = _mm_madd_epi16( xmm5, t_16_f0123_45);
            xmm5 = _mm_madd_epi16( xmm5, t_16_f4567_45);
            xmm0 = _mm_add_epi32( xmm0, xmm6);

            xmm6 = _mm_madd_epi16( xmm7, t_16_f0123_67);
            xmm7 = _mm_madd_epi16( xmm7, t_16_f4567_67);

            xmm1 = _mm_add_epi32( xmm1, xmm5);
            xmm0 = _mm_add_epi32( xmm0, xmm6);
            xmm1 = _mm_add_epi32( xmm1, xmm7);
            // xmm0 - f0
            // xmm1 - f1

            xmm5 = _mm_add_epi32( xmm2, xmm0);
            xmm2 = _mm_sub_epi32( xmm2, xmm0);

            xmm6 = _mm_add_epi32( xmm3, xmm1);
            xmm3 = _mm_sub_epi32( xmm3, xmm1);

            xmm5 = _mm_srai_epi32( xmm5, SHIFT_INV_2ND);
            xmm6 = _mm_srai_epi32( xmm6, SHIFT_INV_2ND);
            xmm5 = _mm_packs_epi32( xmm5, xmm6);

            xmm3 = _mm_shuffle_epi32( xmm3, _MM_SHUFFLE(0,1,2,3));
            xmm2 = _mm_shuffle_epi32( xmm2, _MM_SHUFFLE(0,1,2,3));

            xmm3 = _mm_srai_epi32( xmm3, SHIFT_INV_2ND);
            xmm2 = _mm_srai_epi32( xmm2, SHIFT_INV_2ND);
            xmm3 = _mm_packs_epi32(xmm3, xmm2);

            if (inplace && sizeof(DstCoeffsType) == 1) {
                /* load dst[0-7], expand to 16 bits, add temp[0-7], clip/pack to 8 bytes */
                xmm1 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&pred[0]));
                xmm1 = _mm_add_epi16(xmm1, xmm5);
                xmm1 = _mm_packus_epi16(xmm1, xmm1);

                /* repeat for [8-15] */
                xmm2 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&pred[8]));
                xmm2 = _mm_add_epi16(xmm2, xmm3);
                xmm2 = _mm_packus_epi16(xmm2, xmm2);

                xmm1 = _mm_unpacklo_epi64(xmm1, xmm2);
                _mm_storeu_si128((__m128i *)&dst[0], xmm1);    /* store dst[0-15] (bytes) */
                dst += destStride;
                pred += predStride;
            } else if (inplace && sizeof(DstCoeffsType) == 2) {
                /* load 8 words at a time, add temp values, clip to [0, 2^bitDepth) */
                xmm1 = _mm_loadu_si128((__m128i *)(&pred[0]));
                xmm1 = _mm_adds_epi16(xmm1, xmm5);

                xmm2 = _mm_loadu_si128((__m128i *)(&pred[8]));
                xmm2 = _mm_adds_epi16(xmm2, xmm3);

                xmm4 = _mm_setzero_si128();
                xmm5 = _mm_set1_epi16((1 << bitDepth) - 1);
                xmm1 = _mm_max_epi16(xmm1, xmm4); xmm1 = _mm_min_epi16(xmm1, xmm5);
                xmm2 = _mm_max_epi16(xmm2, xmm4); xmm2 = _mm_min_epi16(xmm2, xmm5);

                _mm_storeu_si128((__m128i *)(&dst[0]), xmm1);    /* store temp[0-7] (words) */
                _mm_storeu_si128((__m128i *)(&dst[8]), xmm2);    /* store temp[0-7] (words) */
                dst += destStride;
                pred += predStride;

            } else {
                _mm_storeu_si128((__m128i *)(dst + 0), xmm5);    /* store temp[ 0- 7] (words) */
                _mm_storeu_si128((__m128i *)(dst + 8), xmm3);    /* store temp[ 8-15] (words) */
                dst += destStride;
                pred += predStride;
            }

            coeff += 16;
        }
    }

    template <int bitDepth, typename PredPixType, typename DstCoeffsType, bool inplace>
    static void h265_DCT16x16Inv_16sT_Kernel(PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
    {
        __m128i s0, s2, s4, s6, s8, s1, s3, s5, s7, s9, s11, s13, s15;
        __m128i O0L, O1L, O2L, O3L, O4L, O5L, O6L, O7L;
        __m128i E0L, E7L, E1L, E6L, E2L, E5L, E3L, E4L;
        __m128i EE0L, EE1L, EE2L, EE3L;
        __m128i EO0L, EO1L, EO2L, EO3L;
        __m128i EEE0L, EEE1L, EEO0L, EEO1L;
        __m128i s13L, s57L, s9BL, sDFL, s26L, sAEL, s08L, s4CL;
        __m128i R0F, R1E, R2D, R3C, R4B, R5A, R69, R78;
        __m128i sA, sC, sE;
        int j;

        M128I_W2x4C( t_16_O0_13,  90,  87); // g_T16[ 1][0], g_T16[ 3][0]
        M128I_W2x4C( t_16_O0_57,  80,  70); // g_T16[ 5][0], g_T16[ 7][0]
        M128I_W2x4C( t_16_O0_9B,  57,  43); // g_T16[ 9][0], g_T16[11][0]
        M128I_W2x4C( t_16_O0_DF,  25,   9); // g_T16[13][0], g_T16[15][0]
        M128I_W2x4C( t_16_O1_13,  87,  57); // g_T16[ 1][1], g_T16[ 3][1]
        M128I_W2x4C( t_16_O1_57,   9, -43); // g_T16[ 5][1], g_T16[ 7][1]
        M128I_W2x4C( t_16_O1_9B, -80, -90); // g_T16[ 9][1], g_T16[11][1]
        M128I_W2x4C( t_16_O1_DF, -70, -25); // g_T16[13][1], g_T16[15][1]
        M128I_W2x4C( t_16_O2_13,  80,   9); // g_T16[ 1][2], g_T16[ 3][2]
        M128I_W2x4C( t_16_O2_57, -70, -87); // g_T16[ 5][2], g_T16[ 7][2]
        M128I_W2x4C( t_16_O2_9B, -25,  57); // g_T16[ 9][2], g_T16[11][2]
        M128I_W2x4C( t_16_O2_DF,  90,  43); // g_T16[13][2], g_T16[15][2]
        M128I_W2x4C( t_16_O3_13,  70, -43); // g_T16[ 1][3], g_T16[ 3][3]
        M128I_W2x4C( t_16_O3_57, -87,   9); // g_T16[ 5][3], g_T16[ 7][3]
        M128I_W2x4C( t_16_O3_9B,  90,  25); // g_T16[ 9][3], g_T16[11][3]
        M128I_W2x4C( t_16_O3_DF, -80, -57); // g_T16[13][3], g_T16[15][3]
        M128I_W2x4C( t_16_O4_13,  57, -80); // g_T16[ 1][4], g_T16[ 3][4]
        M128I_W2x4C( t_16_O4_57, -25,  90); // g_T16[ 5][4], g_T16[ 7][4]
        M128I_W2x4C( t_16_O4_9B,  -9, -87); // g_T16[ 9][4], g_T16[11][4]
        M128I_W2x4C( t_16_O4_DF,  43,  70); // g_T16[13][4], g_T16[15][4]
        M128I_W2x4C( t_16_O5_13,  43, -90); // g_T16[ 1][5], g_T16[ 3][5]
        M128I_W2x4C( t_16_O5_57,  57,  25); // g_T16[ 5][5], g_T16[ 7][5]
        M128I_W2x4C( t_16_O5_9B, -87,  70); // g_T16[ 9][5], g_T16[11][5]
        M128I_W2x4C( t_16_O5_DF,   9, -80); // g_T16[13][5], g_T16[15][5]
        M128I_W2x4C( t_16_O6_13,  25, -70); // g_T16[ 1][6], g_T16[ 3][6]
        M128I_W2x4C( t_16_O6_57,  90, -80); // g_T16[ 5][6], g_T16[ 7][6]
        M128I_W2x4C( t_16_O6_9B,  43,   9); // g_T16[ 9][6], g_T16[11][6]
        M128I_W2x4C( t_16_O6_DF, -57,  87); // g_T16[13][6], g_T16[15][6]
        M128I_W2x4C( t_16_O7_13,   9, -25); // g_T16[ 1][7], g_T16[ 3][7]
        M128I_W2x4C( t_16_O7_57,  43, -57); // g_T16[ 5][7], g_T16[ 7][7]
        M128I_W2x4C( t_16_O7_9B,  70, -80); // g_T16[ 9][7], g_T16[11][7]
        M128I_W2x4C( t_16_O7_DF,  87, -90); // g_T16[13][7], g_T16[15][7]

        M128I_W2x4C( t_16_E0_26,  89,  75); // g_T16[ 2][0], g_T16[ 6][0]
        M128I_W2x4C( t_16_E0_AE,  50,  18); // g_T16[10][0], g_T16[14][0]
        M128I_W2x4C( t_16_E1_26,  75, -18); // g_T16[ 2][1], g_T16[ 6][1]
        M128I_W2x4C( t_16_E1_AE, -89, -50); // g_T16[10][1], g_T16[14][1]
        M128I_W2x4C( t_16_E2_26,  50, -89); // g_T16[ 2][2], g_T16[ 6][2]
        M128I_W2x4C( t_16_E2_AE,  18,  75); // g_T16[10][2], g_T16[14][2]
        M128I_W2x4C( t_16_E3_26,  18, -50); // g_T16[ 2][3], g_T16[ 6][3]
        M128I_W2x4C( t_16_E3_AE,  75, -89); // g_T16[10][3], g_T16[14][3]
        M128I_W2x4C( t_16_EEE0_08,  64,  64); // g_T16[0][0], g_T16[ 8][0]
        M128I_W2x4C( t_16_EEE1_08,  64, -64); // g_T16[0][1], g_T16[ 8][1]
        M128I_W2x4C( t_16_EEO0_4C,  83,  36); // g_T16[4][0], g_T16[12][0]
        M128I_W2x4C( t_16_EEO1_4C,  36, -83); // g_T16[4][1], g_T16[12][1]

        M128I_DC ( round1, 1<<(SHIFT_INV_1ST-1));

        ALIGN_DECL(16) short tmp[16 * 16];
        short *H265_RESTRICT p_tmp = tmp;

        for (j = 0; j < 16; j += 4)
        {
            // odd
            s1   = _mm_loadl_epi64( (const __m128i *)(coeff + 1*coef_stride));
            s3   = _mm_loadl_epi64( (const __m128i *)(coeff + 3*coef_stride));
            s13L = _mm_unpacklo_epi16( s1, s3);

            s5   = _mm_loadl_epi64( (const __m128i *)(coeff + 5*coef_stride));
            s7   = _mm_loadl_epi64( (const __m128i *)(coeff + 7*coef_stride));
            s57L = _mm_unpacklo_epi16( s5, s7);

            s9   = _mm_loadl_epi64( (const __m128i *)(coeff + 9*coef_stride));
            s11  = _mm_loadl_epi64( (const __m128i *)(coeff + 11*coef_stride));
            s9BL = _mm_unpacklo_epi16( s9, s11);

            s13  = _mm_loadl_epi64( (const __m128i *)(coeff + 13*coef_stride));
            s15  = _mm_loadl_epi64( (const __m128i *)(coeff + 15*coef_stride));
            sDFL = _mm_unpacklo_epi16( s13, s15);

            O0L = _mm_add_epi32( _mm_madd_epi16(s13L, t_16_O0_13), _mm_madd_epi16(s57L, t_16_O0_57));
            O0L = _mm_add_epi32(O0L, _mm_madd_epi16(s9BL, t_16_O0_9B));
            O0L = _mm_add_epi32(O0L, _mm_madd_epi16(sDFL, t_16_O0_DF));

            O1L = _mm_add_epi32( _mm_madd_epi16(s13L, t_16_O1_13), _mm_madd_epi16(s57L, t_16_O1_57));
            O1L = _mm_add_epi32(O1L, _mm_madd_epi16(s9BL, t_16_O1_9B));
            O1L = _mm_add_epi32(O1L, _mm_madd_epi16(sDFL, t_16_O1_DF));

            O2L = _mm_add_epi32( _mm_madd_epi16(s13L, t_16_O2_13), _mm_madd_epi16(s57L, t_16_O2_57));
            O2L = _mm_add_epi32(O2L, _mm_madd_epi16(s9BL, t_16_O2_9B));
            O2L = _mm_add_epi32(O2L, _mm_madd_epi16(sDFL, t_16_O2_DF));

            O3L = _mm_add_epi32( _mm_madd_epi16(s13L, t_16_O3_13), _mm_madd_epi16(s57L, t_16_O3_57));
            O3L = _mm_add_epi32(O3L, _mm_madd_epi16(s9BL, t_16_O3_9B));
            O3L = _mm_add_epi32(O3L, _mm_madd_epi16(sDFL, t_16_O3_DF));

            O4L = _mm_add_epi32( _mm_madd_epi16(s13L, t_16_O4_13), _mm_madd_epi16(s57L, t_16_O4_57));
            O4L = _mm_add_epi32(O4L, _mm_madd_epi16(s9BL, t_16_O4_9B));
            O4L = _mm_add_epi32(O4L, _mm_madd_epi16(sDFL, t_16_O4_DF));

            O5L = _mm_add_epi32( _mm_madd_epi16(s13L, t_16_O5_13), _mm_madd_epi16(s57L, t_16_O5_57));
            O5L = _mm_add_epi32(O5L, _mm_madd_epi16(s9BL, t_16_O5_9B));
            O5L = _mm_add_epi32(O5L, _mm_madd_epi16(sDFL, t_16_O5_DF));

            O6L = _mm_add_epi32( _mm_madd_epi16(s13L, t_16_O6_13), _mm_madd_epi16(s57L, t_16_O6_57));
            O6L = _mm_add_epi32(O6L, _mm_madd_epi16(s9BL, t_16_O6_9B));
            O6L = _mm_add_epi32(O6L, _mm_madd_epi16(sDFL, t_16_O6_DF));

            O7L = _mm_add_epi32( _mm_madd_epi16(s13L, t_16_O7_13), _mm_madd_epi16(s57L, t_16_O7_57));
            O7L = _mm_add_epi32(O7L, _mm_madd_epi16(s9BL, t_16_O7_9B));
            O7L = _mm_add_epi32(O7L, _mm_madd_epi16(sDFL, t_16_O7_DF));

            // even
            s2   = _mm_loadl_epi64( (const __m128i *)(coeff + 2*coef_stride));
            s6   = _mm_loadl_epi64( (const __m128i *)(coeff + 6*coef_stride));
            s26L = _mm_unpacklo_epi16( s2, s6);

            sA   = _mm_loadl_epi64( (const __m128i *)(coeff + 10*coef_stride));
            sE   = _mm_loadl_epi64( (const __m128i *)(coeff + 14*coef_stride));
            sAEL = _mm_unpacklo_epi16( sA, sE);

            EO0L = _mm_add_epi32( _mm_madd_epi16( s26L, t_16_E0_26), _mm_madd_epi16( sAEL, t_16_E0_AE));
            EO1L = _mm_add_epi32( _mm_madd_epi16( s26L, t_16_E1_26), _mm_madd_epi16( sAEL, t_16_E1_AE));
            EO2L = _mm_add_epi32( _mm_madd_epi16( s26L, t_16_E2_26), _mm_madd_epi16( sAEL, t_16_E2_AE));
            EO3L = _mm_add_epi32( _mm_madd_epi16( s26L, t_16_E3_26), _mm_madd_epi16( sAEL, t_16_E3_AE));

            s0   = _mm_loadl_epi64( (const __m128i *)(coeff + 0*coef_stride));
            s8   = _mm_loadl_epi64( (const __m128i *)(coeff + 8*coef_stride));
            s08L = _mm_unpacklo_epi16( s0, s8);

            EEE0L = _mm_add_epi32( _mm_madd_epi16( s08L, t_16_EEE0_08), round1);
            EEE1L = _mm_add_epi32( _mm_madd_epi16( s08L, t_16_EEE1_08), round1);

            s4 = _mm_loadl_epi64( (const __m128i *)(coeff + 4*coef_stride));
            sC = _mm_loadl_epi64( (const __m128i *)(coeff + 12*coef_stride));
            s4CL = _mm_unpacklo_epi16( s4, sC);

            EEO0L = _mm_madd_epi16( s4CL, t_16_EEO0_4C);
            EEO1L = _mm_madd_epi16( s4CL, t_16_EEO1_4C);

            EE0L = _mm_add_epi32( EEE0L, EEO0L);
            EE3L = _mm_sub_epi32( EEE0L, EEO0L);

            EE1L = _mm_add_epi32( EEE1L, EEO1L);
            EE2L = _mm_sub_epi32( EEE1L, EEO1L);

            E0L = _mm_add_epi32( EE0L, EO0L);
            E7L = _mm_sub_epi32( EE0L, EO0L);

            E1L = _mm_add_epi32( EE1L, EO1L);
            E6L = _mm_sub_epi32( EE1L, EO1L);

            E2L = _mm_add_epi32( EE2L, EO2L);
            E5L = _mm_sub_epi32( EE2L, EO2L);

            E3L = _mm_add_epi32( EE3L, EO3L);
            E4L = _mm_sub_epi32( EE3L, EO3L);

            R0F = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E0L, O0L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E0L, O0L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 0*16), R0F);
            _mm_storeh_epi64((__m128i *)(p_tmp +15*16), R0F);

            R1E = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E1L, O1L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E1L, O1L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 1*16), R1E);
            _mm_storeh_epi64((__m128i *)(p_tmp +14*16), R1E);

            R2D = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E2L, O2L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E2L, O2L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 2*16), R2D);
            _mm_storeh_epi64((__m128i *)(p_tmp +13*16), R2D);

            R3C = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E3L, O3L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E3L, O3L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 3*16), R3C);
            _mm_storeh_epi64((__m128i *)(p_tmp +12*16), R3C);

            R4B = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E4L, O4L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E4L, O4L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 4*16), R4B);
            _mm_storeh_epi64((__m128i *)(p_tmp +11*16), R4B);

            R5A = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E5L, O5L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E5L, O5L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 5*16), R5A);
            _mm_storeh_epi64((__m128i *)(p_tmp +10*16), R5A);

            R69 = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E6L, O6L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E6L, O6L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 6*16), R69);
            _mm_storeh_epi64((__m128i *)(p_tmp + 9*16), R69);

            R78 = _mm_packs_epi32( _mm_srai_epi32( _mm_add_epi32( E7L, O7L), SHIFT_INV_1ST), _mm_srai_epi32( _mm_sub_epi32( E7L, O7L), SHIFT_INV_1ST));
            _mm_storel_epi64((__m128i *)(p_tmp + 7*16), R78);
            _mm_storeh_epi64((__m128i *)(p_tmp + 8*16), R78);

            coeff += 4;
            p_tmp += 4;
        }

        IDCT_16x16_2ND_SSE4<bitDepth, PredPixType, DstCoeffsType, inplace>(pred, predStride, dst, tmp, destStride);
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

    // Load constants
    //#define _mm_load_const _mm_load_si128
#define _mm_load_const _mm_lddqu_si128

#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))
#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (double *)(p)))
#define _mm_storeh_epi64(p, A) _mm_storeh_pd((double *)(p), _mm_castsi128_pd(A))

#define src_stride 32  // strade for temp[]

    // 9200 CPU clocks.
    // 43 * _mm_madd_epi16 = 344
    template <int bitDepth, typename PredPixType, typename DstCoeffsType, bool inplace>
    static void h265_DCT32x32Inv_16sT_Kernel(PredPixType *pred, int predStride, DstCoeffsType *dst, const short *H265_RESTRICT coeff, int destStride)
    {
        int i;
        signed short * dest;
        __m128i * in;
        ALIGN_DECL(16) __m128i buff[32*32];
        ALIGN_DECL(16) short temp[32*32];
        __m128i s0, s1, s2, s3, s4, s5, s6, s7;
        const int SHIFT_INV_2ND = (SHIFT_INV_2ND_BASE - (bitDepth - 8));

        s1 = _mm_setzero_si128();
        s2 = _mm_setzero_si128();
        s3 = _mm_setzero_si128();
        s4 = _mm_setzero_si128();
        s5 = _mm_setzero_si128();

        // Vertical 1-D inverse transform
        {
#define shift       7
#define rounder     rounder_64
#define dest_stride 32

            for (i=0; i<32; i++)
            {
                int num = 4 * repos[i];

                s1 = _mm_insert_epi16(s1, (int)coeff[0*src_stride], 0);  // 0 0 0 0 0 0 0 a
                s1 = _mm_insert_epi16(s1, (int)coeff[16*src_stride], 1); // 0 0 0 0 0 0 b a
                s1 = _mm_insert_epi16(s1, (int)coeff[8*src_stride], 2);  // 0 0 0 0 0 c b a
                s1 = _mm_insert_epi16(s1, (int)coeff[24*src_stride], 3); // 0 0 0 0 d c b a
                s1 = _mm_unpacklo_epi32(s1, s1);                       // d c d c b a b a

                s1 = _mm_madd_epi16(s1, _mm_lddqu_si128((const __m128i *)koef0000));
                s1 = _mm_add_epi32(s1,  _mm_lddqu_si128((const __m128i *)rounder));

                s0 = _mm_shuffle_epi32(s1, 0x4E);
                s1 = _mm_sub_epi32(s1, s0);
                s0 = _mm_add_epi32(s0, s0);
                s0 = _mm_add_epi32(s0, s1);
                s1 = _mm_shuffle_epi32(s1, 1);
                s0 = _mm_unpacklo_epi64(s0, s1);

                s2 = _mm_insert_epi16(s2, (int)coeff[ 4*src_stride], 0); // 0 0 0 0 0 0 0 a
                s2 = _mm_insert_epi16(s2, (int)coeff[12*src_stride], 1); // 0 0 0 0 0 0 b a
                s2 = _mm_insert_epi16(s2, (int)coeff[20*src_stride], 2); // 0 0 0 0 0 c b a
                s2 = _mm_insert_epi16(s2, (int)coeff[28*src_stride], 3); // 0 0 0 0 d c b a
                s4 = _mm_shuffle_epi32(s2, 0x00);                      // b a b a b a b a
                s5 = _mm_shuffle_epi32(s2, 0x55);                      // d c d c d c d c

                s4 = _mm_madd_epi16(s4, _mm_lddqu_si128((const __m128i *)koef0001));
                s5 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef0002));
                s4 = _mm_add_epi32(s4,  s5);

                s1 = _mm_shuffle_epi32(s0, 0x1B);
                s7 = _mm_shuffle_epi32(s4, 0x1B);
                s0 = _mm_add_epi32(s0,  s4);
                s1 = _mm_sub_epi32(s1,  s7);

                s3 = _mm_insert_epi16(s3, (int)coeff[2*src_stride], 0);
                s3 = _mm_insert_epi16(s3, (int)coeff[6*src_stride], 1);
                s3 = _mm_insert_epi16(s3, (int)coeff[10*src_stride], 2);
                s3 = _mm_insert_epi16(s3, (int)coeff[14*src_stride], 3);
                s3 = _mm_insert_epi16(s3, (int)coeff[18*src_stride], 4);
                s3 = _mm_insert_epi16(s3, (int)coeff[22*src_stride], 5);
                s3 = _mm_insert_epi16(s3, (int)coeff[26*src_stride], 6);
                s3 = _mm_insert_epi16(s3, (int)coeff[30*src_stride], 7);

                s5 = _mm_shuffle_epi32(s3, 0);
                s4 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef0003));
                s5 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef0004));

                s6 = _mm_shuffle_epi32(s3, 0x55);
                s7 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0005));
                s4 = _mm_add_epi32(s4,  s7);

                s6 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0006));
                s5 = _mm_add_epi32(s5,  s6);

                s6 = _mm_shuffle_epi32(s3, 0xAA);
                s7 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0007));
                s4 = _mm_add_epi32(s4,  s7);
                s6 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0008));
                s5 = _mm_add_epi32(s5,  s6);

                s6 = _mm_shuffle_epi32(s3, 0xFF);
                s7 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0009));
                s4 = _mm_add_epi32(s4,  s7);
                s6 = _mm_madd_epi16(s6, _mm_lddqu_si128((const __m128i *)koef0010));
                s5 = _mm_add_epi32(s5,  s6);

                s2 = _mm_shuffle_epi32(s0, 0x1B);
                s3 = _mm_shuffle_epi32(s1, 0x1B);
                s6 = _mm_shuffle_epi32(s4, 0x1B);
                s7 = _mm_shuffle_epi32(s5, 0x1B);

                s0 = _mm_add_epi32(s0,  s4);
                s1 = _mm_add_epi32(s1,  s5);
                s2 = _mm_sub_epi32(s2,  s6);
                s3 = _mm_sub_epi32(s3,  s7);

                s4 = _mm_insert_epi16(s4, (int)coeff[1*src_stride], 0);
                s4 = _mm_insert_epi16(s4, (int)coeff[3*src_stride], 1);
                s4 = _mm_insert_epi16(s4, (int)coeff[5*src_stride], 2);
                s4 = _mm_insert_epi16(s4, (int)coeff[7*src_stride], 3);
                s4 = _mm_insert_epi16(s4, (int)coeff[9*src_stride], 4);
                s4 = _mm_insert_epi16(s4, (int)coeff[11*src_stride], 5);
                s4 = _mm_insert_epi16(s4, (int)coeff[13*src_stride], 6);
                s4 = _mm_insert_epi16(s4, (int)coeff[15*src_stride], 7);

                s5 = _mm_shuffle_epi32(s4, 0);
                s6 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef00010));
                s5 = _mm_shuffle_epi32(s4, 0x55);
                s7 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef02030));
                s6 = _mm_add_epi32(s6,  s7);

                s5 = _mm_shuffle_epi32(s4, 0xAA);
                s7 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef04050));
                s6 = _mm_add_epi32(s6,  s7);
                s5 = _mm_shuffle_epi32(s4, 0xFF);
                s7 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef06070));
                s6 = _mm_add_epi32(s6,  s7);

                s5 = _mm_insert_epi16(s5, (int)coeff[17*src_stride], 0);
                s5 = _mm_insert_epi16(s5, (int)coeff[19*src_stride], 1);
                s5 = _mm_shuffle_epi32(s5, 0);
                s7 = _mm_madd_epi16(s5, _mm_lddqu_si128((const __m128i *)koef08090));
                s6 = _mm_add_epi32(s6,  s7);

                s5 = _mm_insert_epi16(s5, (int)coeff[21*src_stride], 2);
                s5 = _mm_insert_epi16(s5, (int)coeff[23*src_stride], 3);
                s5 = _mm_insert_epi16(s5, (int)coeff[25*src_stride], 4);
                s5 = _mm_insert_epi16(s5, (int)coeff[27*src_stride], 5);
                s5 = _mm_insert_epi16(s5, (int)coeff[29*src_stride], 6);
                s5 = _mm_insert_epi16(s5, (int)coeff[31*src_stride], 7);

                s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0x55), _mm_lddqu_si128((const __m128i *)koef10110)));
                s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0xAA), _mm_lddqu_si128((const __m128i *)koef12130)));
                s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0xFF), _mm_lddqu_si128((const __m128i *)koef14150)));
                s7 = _mm_shuffle_epi32(s0, 0x1B);
                s0 = _mm_add_epi32(s0,  s6);
                s6 = _mm_shuffle_epi32(s6, 0x1B);
                s7 = _mm_sub_epi32(s7,  s6);

                s0 = _mm_srai_epi32(s0,  shift);            // (r03 r02 r01 r00) >>= shift
                s7 = _mm_srai_epi32(s7,  shift);            // (r31 r30 r29 r28) >>= shift
                s0 = _mm_packs_epi32(s0, s7);               // clip(-32768, 32767)

                // Store 1
                buff[num++] = s0;

                s6 = _mm_madd_epi16(_mm_shuffle_epi32(s4, 0), _mm_lddqu_si128((const __m128i *)koef00011));
                s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s4, 0x55), _mm_lddqu_si128((const __m128i *)koef02031)));
                s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s4, 0xAA), _mm_lddqu_si128((const __m128i *)koef04051)));
                s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s4, 0xFF), _mm_lddqu_si128((const __m128i *)koef06071)));
                s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0x00), _mm_lddqu_si128((const __m128i *)koef08091)));
                s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0x55), _mm_lddqu_si128((const __m128i *)koef10111)));
                s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0xAA), _mm_lddqu_si128((const __m128i *)koef12131)));
                s6 = _mm_add_epi32(s6,  _mm_madd_epi16(_mm_shuffle_epi32(s5, 0xFF), _mm_lddqu_si128((const __m128i *)koef14151)));

                s7 = _mm_shuffle_epi32(s1, 0x1B);
                s1 = _mm_add_epi32(s1,  s6);
                s6 = _mm_shuffle_epi32(s6, 0x1B);
                s7 = _mm_sub_epi32(s7,  s6);

                s1 = _mm_packs_epi32(_mm_srai_epi32(s1,  shift), _mm_srai_epi32(s7,  shift));

                // Store 2
                buff[num++] = s1;

                s1 = _mm_shuffle_epi32(s4, 0x00);
                s0 = _mm_madd_epi16(s1, _mm_lddqu_si128((const __m128i *)koef00012));
                s1 = _mm_madd_epi16(s1, _mm_lddqu_si128((const __m128i *)koef00013));

                s7 = _mm_shuffle_epi32(s4, 0x55);
                s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef02032)));
                s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef02033)));

                s7 = _mm_shuffle_epi32(s4, 0xAA);
                s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef04052)));
                s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef04053)));

                s7 = _mm_shuffle_epi32(s4, 0xFF);
                s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef06072)));
                s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef06073)));

                s7 = _mm_shuffle_epi32(s5, 0x00);
                s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef08092)));
                s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef08093)));

                s7 = _mm_shuffle_epi32(s5, 0x55);
                s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef10112)));
                s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef10113)));

                s7 = _mm_shuffle_epi32(s5, 0xAA);
                s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef12132)));
                s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef12133)));

                s7 = _mm_shuffle_epi32(s5, 0xFF);
                s0 = _mm_add_epi32(s0,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef14152)));
                s1 = _mm_add_epi32(s1,  _mm_madd_epi16(s7, _mm_lddqu_si128((const __m128i *)koef14153)));

                s6 = _mm_shuffle_epi32(s3, 0x1B);
                s3 = _mm_add_epi32(s3,  s0);
                s0 = _mm_shuffle_epi32(s0, 0x1B);
                s6 = _mm_sub_epi32(s6,  s0);
                s3 = _mm_packs_epi32(_mm_srai_epi32(s3,  shift), _mm_srai_epi32(s6,  shift));

                // Store 3
                buff[num++] = s3;

                s7 = _mm_shuffle_epi32(s2, 0x1B);
                s2 = _mm_add_epi32(s2,  s1);
                s1 = _mm_shuffle_epi32(s1, 0x1B);
                s7 = _mm_sub_epi32(s7,  s1);
                s2 = _mm_packs_epi32(_mm_srai_epi32(s2,  shift), _mm_srai_epi32(s7,  shift));

                // Store 4
                buff[num++] = s2;
                coeff++;
            }
#undef  shift
#undef  rounder
#undef  dest_stride
        }

        dest  = temp;
        in = buff;
#define dest_stride 32

        for (i=0; i<8; i++)
        {
            // load buffer short[8x4], rotate 2x4x4, save short[4x8]. Repeat 4 times
            s0 =  _mm_lddqu_si128((const __m128i *)(in + 0*4)); // 07 06 05 04 03 02 01 00
            s1 =  _mm_lddqu_si128((const __m128i *)(in + 1*4)); // 17 16 15 14 13 12 11 10
            s2 =  _mm_lddqu_si128((const __m128i *)(in + 2*4)); // 27 26 25 24 23 22 21 20
            s3 =  _mm_lddqu_si128((const __m128i *)(in + 3*4)); // 37 36 35 34 33 32 31 30
            s6 = s0;
            s0 = _mm_unpacklo_epi16(s0, s1);  // 13 03 12 02 11 01 10 00
            s6 = _mm_unpackhi_epi16(s6, s1);  // 17 07 16 06 15 05 14 04
            s5 = s2;
            s4 = s0;
            s2 = _mm_unpacklo_epi16(s2, s3);  // 33 23 32 22 31 21 30 20
            s5 = _mm_unpackhi_epi16(s5, s3);  // 37 27 36 26 35 25 34 24
            s0 = _mm_unpacklo_epi32(s0, s2);  // 31 21 11 01 30 20 10 00
            _mm_storel_epi64((__m128i*)(&dest[ 0*dest_stride]), s0);
            _mm_storeh_epi64((__m128i*)(&dest[ 1*dest_stride]), s0);
            s7 = s6;
            s4 = _mm_unpackhi_epi32(s4, s2);  // 33 23 13 03 32 22 12 02
            _mm_storel_epi64((__m128i*)(&dest[ 2*dest_stride]), s4);
            _mm_storeh_epi64((__m128i*)(&dest[ 3*dest_stride]), s4);
            s6 = _mm_unpacklo_epi32(s6, s5);  // 35 25 15 05 34 24 14 04
            _mm_storel_epi64((__m128i*)(&dest[28*dest_stride]), s6);
            _mm_storeh_epi64((__m128i*)(&dest[29*dest_stride]), s6);
            s7 = _mm_unpackhi_epi32(s7, s5);  // 37 27 17 07 36 26 16 06
            _mm_storel_epi64((__m128i*)(&dest[30*dest_stride]), s7);
            _mm_storeh_epi64((__m128i*)(&dest[31*dest_stride]), s7);

            // 1th
            s0 =  _mm_lddqu_si128((const __m128i *)(in + 0*4 + 1));
            s1 =  _mm_lddqu_si128((const __m128i *)(in + 1*4 + 1));
            s2 =  _mm_lddqu_si128((const __m128i *)(in + 2*4 + 1));
            s3 =  _mm_lddqu_si128((const __m128i *)(in + 3*4 + 1));
            s6 = s0;
            s0 = _mm_unpacklo_epi16(s0, s1);
            s6 = _mm_unpackhi_epi16(s6, s1);
            s5 = s2;
            s4 = s0;
            s2 = _mm_unpacklo_epi16(s2, s3);
            s5 = _mm_unpackhi_epi16(s5, s3);
            s0 = _mm_unpacklo_epi32(s0, s2);
            _mm_storel_epi64((__m128i*)(&dest[ 4*dest_stride]), s0);
            _mm_storeh_epi64((__m128i*)(&dest[ 5*dest_stride]), s0);
            s7 = s6;
            s4 = _mm_unpackhi_epi32(s4, s2);
            _mm_storel_epi64((__m128i*)(&dest[ 6*dest_stride]), s4);
            _mm_storeh_epi64((__m128i*)(&dest[ 7*dest_stride]), s4);
            s6 = _mm_unpacklo_epi32(s6, s5);
            _mm_storel_epi64((__m128i*)(&dest[24*dest_stride]), s6);
            _mm_storeh_epi64((__m128i*)(&dest[25*dest_stride]), s6);
            s7 = _mm_unpackhi_epi32(s7, s5);
            _mm_storel_epi64((__m128i*)(&dest[26*dest_stride]), s7);
            _mm_storeh_epi64((__m128i*)(&dest[27*dest_stride]), s7);

            // 2th
            s0 =  _mm_lddqu_si128((const __m128i *)(in + 0*4 + 2));
            s1 =  _mm_lddqu_si128((const __m128i *)(in + 1*4 + 2));
            s2 =  _mm_lddqu_si128((const __m128i *)(in + 2*4 + 2));
            s3 =  _mm_lddqu_si128((const __m128i *)(in + 3*4 + 2));
            s6 = s0;
            s0 = _mm_unpacklo_epi16(s0, s1);
            s6 = _mm_unpackhi_epi16(s6, s1);
            s5 = s2;
            s4 = s0;
            s2 = _mm_unpacklo_epi16(s2, s3);
            s5 = _mm_unpackhi_epi16(s5, s3);
            s0 = _mm_unpacklo_epi32(s0, s2);
            _mm_storel_epi64((__m128i*)(&dest[ 8*dest_stride]), s0);
            _mm_storeh_epi64((__m128i*)(&dest[ 9*dest_stride]), s0);
            s7 = s6;
            s4 = _mm_unpackhi_epi32(s4, s2);
            _mm_storel_epi64((__m128i*)(&dest[10*dest_stride]), s4);
            _mm_storeh_epi64((__m128i*)(&dest[11*dest_stride]), s4);
            s6 = _mm_unpacklo_epi32(s6, s5);
            _mm_storel_epi64((__m128i*)(&dest[20*dest_stride]), s6);
            _mm_storeh_epi64((__m128i*)(&dest[21*dest_stride]), s6);
            s7 = _mm_unpackhi_epi32(s7, s5);
            _mm_storel_epi64((__m128i*)(&dest[22*dest_stride]), s7);
            _mm_storeh_epi64((__m128i*)(&dest[23*dest_stride]), s7);

            // 3th
            s0 =  _mm_lddqu_si128((const __m128i *)(in + 0*4 + 3));
            s1 =  _mm_lddqu_si128((const __m128i *)(in + 1*4 + 3));
            s2 =  _mm_lddqu_si128((const __m128i *)(in + 2*4 + 3));
            s3 =  _mm_lddqu_si128((const __m128i *)(in + 3*4 + 3));
            s6 = s0;
            s0 = _mm_unpacklo_epi16(s0, s1);
            s6 = _mm_unpackhi_epi16(s6, s1);
            s5 = s2;
            s4 = s0;
            s2 = _mm_unpacklo_epi16(s2, s3);
            s5 = _mm_unpackhi_epi16(s5, s3);
            s0 = _mm_unpacklo_epi32(s0, s2);
            _mm_storel_epi64((__m128i*)(&dest[12*dest_stride]), s0);
            _mm_storeh_epi64((__m128i*)(&dest[13*dest_stride]), s0);
            s7 = s6;
            s4 = _mm_unpackhi_epi32(s4, s2);
            _mm_storel_epi64((__m128i*)(&dest[14*dest_stride]), s4);
            _mm_storeh_epi64((__m128i*)(&dest[15*dest_stride]), s4);
            s6 = _mm_unpacklo_epi32(s6, s5);
            _mm_storel_epi64((__m128i*)(&dest[16*dest_stride]), s6);
            _mm_storeh_epi64((__m128i*)(&dest[17*dest_stride]), s6);
            s7 = _mm_unpackhi_epi32(s7, s5);
            _mm_storel_epi64((__m128i*)(&dest[18*dest_stride]), s7);
            _mm_storeh_epi64((__m128i*)(&dest[19*dest_stride]), s7);
            dest += 4;
            in += 16;
        }

        // Horizontal 1-D inverse transform
        {
#undef  dest_stride
#define dest_stride destStride

            __m128i s0_, s1_, s6_;

            coeff  = temp;

            for (i=0; i<32; i++) {
                // load first 8 words
                s7 = _mm_load_si128((const __m128i *)(coeff)); coeff += 8;
                s1 = _mm_unpacklo_epi32(s7, s7);
                s1 = _mm_madd_epi16(s1, _mm_load_const((const __m128i *)koef0000));
                if (SHIFT_INV_2ND == 12) {
                    s1 = _mm_add_epi32(s1,  _mm_load_const((const __m128i *)rounder_2048));
                } else if (SHIFT_INV_2ND == 11) {
                    s1 = _mm_add_epi32(s1,  _mm_load_const((const __m128i *)rounder_1024));
                } else if (SHIFT_INV_2ND == 10) {
                    s1 = _mm_add_epi32(s1,  _mm_load_const((const __m128i *)rounder_512));
                } else if (SHIFT_INV_2ND == 9) {
                    s1 = _mm_add_epi32(s1,  _mm_load_const((const __m128i *)rounder_256));
                } else if (SHIFT_INV_2ND == 8) {
                    s1 = _mm_add_epi32(s1,  _mm_load_const((const __m128i *)rounder_128));
                }

                s0 = _mm_shuffle_epi32(s1, 0x4E);
                s1 = _mm_sub_epi32(s1, s0);
                s0 = _mm_add_epi32(s0, s0);
                s0 = _mm_add_epi32(s0, s1);
                s1 = _mm_shuffle_epi32(s1, 1);
                s0 = _mm_unpacklo_epi64(s0, s1);

                s4 = _mm_shuffle_epi32(s7, 0xAA);
                s5 = _mm_shuffle_epi32(s7, 0xFF);

                s4 = _mm_madd_epi16(s4, _mm_load_const((const __m128i *)koef0001));
                s5 = _mm_madd_epi16(s5, _mm_load_const((const __m128i *)koef0002));
                s4 = _mm_add_epi32(s4,  s5);

                s1 = _mm_shuffle_epi32(s0, 0x1B);
                s7 = _mm_shuffle_epi32(s4, 0x1B);
                s0 = _mm_add_epi32(s0,  s4);
                s1 = _mm_sub_epi32(s1,  s7);

                // load next 8 words
                s3 = _mm_load_si128((const __m128i *)(coeff)); coeff += 8;
                s5 = _mm_shuffle_epi32(s3, 0);
                s4 = _mm_madd_epi16(s5, _mm_load_const((const __m128i *)koef0003));
                s5 = _mm_madd_epi16(s5, _mm_load_const((const __m128i *)koef0004));

                s6 = _mm_shuffle_epi32(s3, 0x55);
                s7 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0005));
                s4 = _mm_add_epi32(s4,  s7);
                s6 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0006));
                s5 = _mm_add_epi32(s5,  s6);

                s6 = _mm_shuffle_epi32(s3, 0xAA);
                s7 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0007));
                s4 = _mm_add_epi32(s4,  s7);
                s6 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0008));
                s5 = _mm_add_epi32(s5,  s6);

                s6 = _mm_shuffle_epi32(s3, 0xFF);
                s7 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0009));
                s4 = _mm_add_epi32(s4,  s7);
                s6 = _mm_madd_epi16(s6, _mm_load_const((const __m128i *)koef0010));
                s5 = _mm_add_epi32(s5,  s6);

                s2 = _mm_shuffle_epi32(s0, 0x1B);
                s3 = _mm_shuffle_epi32(s1, 0x1B);
                s6 = _mm_shuffle_epi32(s4, 0x1B);
                s7 = _mm_shuffle_epi32(s5, 0x1B);

                s0 = _mm_add_epi32(s0,  s4);
                s1 = _mm_add_epi32(s1,  s5);
                s2 = _mm_sub_epi32(s2,  s6);
                s3 = _mm_sub_epi32(s3,  s7);

                // load 3th 8 words
                s4  = _mm_load_si128((const __m128i *)(coeff)); coeff += 8;
                s7  = _mm_shuffle_epi32(s4, 0);
                s6  = _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef00010));
                s6_ = _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef00011));
                s0_ = _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef00012));
                s1_ = _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef00013));

                s7  = _mm_shuffle_epi32(s4, 0x55);
                s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef02030)));
                s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef02031)));
                s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef02032)));
                s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef02033)));

                s7  = _mm_shuffle_epi32(s4, 0xAA);
                s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef04050)));
                s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef04051)));
                s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef04052)));
                s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef04053)));

                s7  = _mm_shuffle_epi32(s4, 0xFF);
                s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef06070)));
                s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef06071)));
                s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef06072)));
                s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef06073)));

                // load last 8 words
                s5  = _mm_load_si128((const __m128i *)(coeff)); coeff += 8;
                s7  = _mm_shuffle_epi32(s5, 0);
                s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef08090)));
                s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef08091)));
                s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef08092)));
                s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef08093)));

                s7  = _mm_shuffle_epi32(s5, 0x55);
                s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef10110)));
                s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef10111)));
                s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef10112)));
                s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef10113)));

                s7  = _mm_shuffle_epi32(s5, 0xAA);
                s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef12130)));
                s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef12131)));
                s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef12132)));
                s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef12133)));

                s7  = _mm_shuffle_epi32(s5, 0xFF);
                s6  = _mm_add_epi32(s6,  _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef14150)));
                s6_ = _mm_add_epi32(s6_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef14151)));
                s0_ = _mm_add_epi32(s0_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef14152)));
                s1_ = _mm_add_epi32(s1_, _mm_madd_epi16(s7, _mm_load_const((const __m128i *)koef14153)));

                s7 = _mm_shuffle_epi32(s0, 0x1B);
                s0 = _mm_add_epi32(s0,  s6);
                s6 = _mm_shuffle_epi32(s6, 0x1B);
                s7 = _mm_sub_epi32(s7,  s6);

                s0 = _mm_srai_epi32(s0,  SHIFT_INV_2ND);           // (r03 r02 r01 r00) >>= shift
                s7 = _mm_srai_epi32(s7,  SHIFT_INV_2ND);           // (r31 r30 r29 r28) >>= shift
                s0 = _mm_packs_epi32(s0, s7);              // clip(-32768, 32767)

                s7 = _mm_shuffle_epi32(s1, 0x1B);
                s1 = _mm_add_epi32(s1,  s6_);
                s6 = _mm_shuffle_epi32(s6_, 0x1B);
                s7 = _mm_sub_epi32(s7,  s6);

                s1 = _mm_packs_epi32(_mm_srai_epi32(s1,  SHIFT_INV_2ND), _mm_srai_epi32(s7,  SHIFT_INV_2ND));

                s6 = s0;
                s0 = _mm_unpacklo_epi64(s0, s1);    /* temp[ 0- 7] (words) */
                s1 = _mm_unpackhi_epi64(s1, s6);    /* temp[24-31] (words) */

                if (inplace && sizeof(DstCoeffsType) == 1) {
                    /* load dst[0-7], expand to 16 bits, add temp[0-7], clip/pack to 8 bytes */
                    s6 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&pred[0]));
                    s6 = _mm_add_epi16(s6, s0);
                    s6 = _mm_packus_epi16(s6, s6);

                    /* repeat for [24-31] */
                    s7 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&pred[24]));
                    s7 = _mm_add_epi16(s7, s1);
                    s7 = _mm_packus_epi16(s7, s7);

                    _mm_storel_epi64((__m128i *)&dst[ 0], s6);    /* store dst[ 0- 7] (bytes) */
                    _mm_storel_epi64((__m128i *)&dst[24], s7);    /* store dst[24-31] (bytes) */
                } else if (inplace && sizeof(DstCoeffsType) == 2) {
                    /* load words [0-7] and [24-31], add temp values, clip to [0, 2^bitDepth) */
                    s6 = _mm_loadu_si128((__m128i *)(&pred[0]));
                    s6 = _mm_adds_epi16(s6, s0);

                    s7 = _mm_loadu_si128((__m128i *)(&pred[24]));
                    s7 = _mm_adds_epi16(s7, s1);

                    s4 = _mm_setzero_si128();
                    s5 = _mm_set1_epi16((1 << bitDepth) - 1);
                    s6 = _mm_max_epi16(s6, s4); s6 = _mm_min_epi16(s6, s5);
                    s7 = _mm_max_epi16(s7, s4); s7 = _mm_min_epi16(s7, s5);

                    _mm_storeu_si128((__m128i *)&dst[ 0], s6);    /* store dst[ 0- 7] (bytes) */
                    _mm_storeu_si128((__m128i *)&dst[24], s7);    /* store dst[24-31] (bytes) */
                } else {
                    _mm_storeu_si128((__m128i *)&dst[ 0], s0);    /* store dst[ 0- 7] (words) */
                    _mm_storeu_si128((__m128i *)&dst[24], s1);    /* store dst[24-31] (words) */
                }

                s6 = _mm_shuffle_epi32(s3, 0x1B);
                s3 = _mm_add_epi32(s3,  s0_);
                s0 = _mm_shuffle_epi32(s0_, 0x1B);
                s6 = _mm_sub_epi32(s6,  s0);
                s3 = _mm_packs_epi32(_mm_srai_epi32(s3,  SHIFT_INV_2ND), _mm_srai_epi32(s6,  SHIFT_INV_2ND));

                s7 = _mm_shuffle_epi32(s2, 0x1B);
                s2 = _mm_add_epi32(s2,  s1_);
                s1 = _mm_shuffle_epi32(s1_, 0x1B);
                s7 = _mm_sub_epi32(s7,  s1);
                s2 = _mm_packs_epi32(_mm_srai_epi32(s2,  SHIFT_INV_2ND), _mm_srai_epi32(s7,  SHIFT_INV_2ND));

                s6 = s3;
                s3 = _mm_unpacklo_epi64(s3, s2);   /* temp[ 8-15] (words) */
                s2 = _mm_unpackhi_epi64(s2, s6);   /* temp[16-23] (words) */

                if (inplace && sizeof(DstCoeffsType) == 1) {
                    /* load dst[8-15], expand to 16 bits, add temp[8-15], clip/pack to 8 bytes */
                    s6 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&pred[8]));
                    s6 = _mm_add_epi16(s6, s3);

                    /* repeat for [24-31] */
                    s7 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&pred[16]));
                    s7 = _mm_add_epi16(s7, s2);

                    s6 = _mm_packus_epi16(s6, s7);
                    _mm_storeu_si128((__m128i *)&dst[ 8], s6);    /* store dst[ 8-23] (bytes) */
                    dst += dest_stride;
                    pred += predStride;
                } else if (inplace && sizeof(DstCoeffsType) == 2) {
                    /* load words [8-15] and [16-23], add temp values, clip to [0, 2^bitDepth) */
                    s6 = _mm_loadu_si128((__m128i *)(&pred[8]));
                    s6 = _mm_adds_epi16(s6, s3);

                    s7 = _mm_loadu_si128((__m128i *)(&pred[16]));
                    s7 = _mm_adds_epi16(s7, s2);

                    s4 = _mm_setzero_si128();
                    s5 = _mm_set1_epi16((1 << bitDepth) - 1);
                    s6 = _mm_max_epi16(s6, s4); s6 = _mm_min_epi16(s6, s5);
                    s7 = _mm_max_epi16(s7, s4); s7 = _mm_min_epi16(s7, s5);

                    _mm_storeu_si128((__m128i *)&dst[ 8], s6);    /* store dst[ 0- 7] (bytes) */
                    _mm_storeu_si128((__m128i *)&dst[16], s7);    /* store dst[24-31] (bytes) */
                    dst += dest_stride;
                    pred += predStride;
                } else {
                    _mm_storeu_si128((__m128i *)&dst[ 8], s3);    /* store dst[ 8-15] (words) */
                    _mm_storeu_si128((__m128i *)&dst[16], s2);    /* store dst[16-23] (words) */
                    dst += dest_stride;
                    pred += predStride;
                }
            }
#undef  shift
#undef  rounder
#undef  dest_stride
        }
    }

    void MAKE_NAME(h265_DCT32x32Inv_16sT)(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth)
    {
        if (inplace) {
            switch (bitDepth) {
            case  8:
                if (inplace == 2)
                    h265_DCT32x32Inv_16sT_Kernel< 8, Ipp8u, Ipp16u,  true >((Ipp8u *)pred, predStride, (Ipp16u *) destPtr, coeff, destStride);
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

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
