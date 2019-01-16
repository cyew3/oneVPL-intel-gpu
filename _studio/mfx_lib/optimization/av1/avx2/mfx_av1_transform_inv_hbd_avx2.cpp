// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "assert.h"
#include "stdio.h"
#include "immintrin.h"
#include "mfx_av1_opts_common.h"
#include "mfx_av1_opts_intrin.h"
#include "mfx_av1_transform_common_avx2.h"

using namespace AV1PP;

enum { TX_4X4=0, TX_8X8=1, TX_16X16=2, TX_32X32=3, TX_SIZES=4, TX_SIZES_ALL=19 };
enum { DCT_DCT=0, ADST_DCT=1, DCT_ADST=2, ADST_ADST=3, FLIPADST_DCT=4, DCT_FLIPADST=5,
       FLIPADST_FLIPADST=6, ADST_FLIPADST=7, FLIPADST_ADST=8, IDTX=9, V_DCT=10,
       H_DCT=11, V_ADST=12, H_ADST=13, V_FLIPADST=14, H_FLIPADST=15, TX_TYPES
};

namespace details {

    const char inv_shift_4x4[2] = { 0, -4 };
    const char inv_shift_8x8[2] = { -1, -4 };
    const char inv_shift_16x16[2] = { -2, -4 };
    const char inv_shift_32x32[2] = { -2, -4 };
    const char inv_shift_64x64[2] = { -2, -4 };
    const char inv_shift_4x8[2] = { 0, -4 };
    const char inv_shift_8x4[2] = { 0, -4 };
    const char inv_shift_8x16[2] = { -1, -4 };
    const char inv_shift_16x8[2] = { -1, -4 };
    const char inv_shift_16x32[2] = { -1, -4 };
    const char inv_shift_32x16[2] = { -1, -4 };
    const char inv_shift_32x64[2] = { -1, -4 };
    const char inv_shift_64x32[2] = { -1, -4 };
    const char inv_shift_4x16[2] = { -1, -4 };
    const char inv_shift_16x4[2] = { -1, -4 };
    const char inv_shift_8x32[2] = { -2, -4 };
    const char inv_shift_32x8[2] = { -2, -4 };
    const char inv_shift_16x64[2] = { -2, -4 };
    const char inv_shift_64x16[2] = { -2, -4 };

    static const char *inv_txfm_shift_ls[TX_SIZES_ALL] = {
        inv_shift_4x4,   inv_shift_8x8,   inv_shift_16x16, inv_shift_32x32,
        inv_shift_64x64, inv_shift_4x8,   inv_shift_8x4,   inv_shift_8x16,
        inv_shift_16x8,  inv_shift_16x32, inv_shift_32x16, inv_shift_32x64,
        inv_shift_64x32, inv_shift_4x16,  inv_shift_16x4,  inv_shift_8x32,
        inv_shift_32x8,  inv_shift_16x64, inv_shift_64x16,
    };

    // static const int cos_bit_min = 10;
    // cospi_arr[i][j] = (int)round(cos(M_PI*j/128) * (1<<(cos_bit_min+i)));
    static const short cospi_arr_data[64] = {
        4096, 4095, 4091, 4085, 4076, 4065, 4052, 4036, 4017, 3996, 3973,
        3948, 3920, 3889, 3857, 3822, 3784, 3745, 3703, 3659, 3612, 3564,
        3513, 3461, 3406, 3349, 3290, 3229, 3166, 3102, 3035, 2967, 2896,
        2824, 2751, 2675, 2598, 2520, 2440, 2359, 2276, 2191, 2106, 2019,
        1931, 1842, 1751, 1660, 1567, 1474, 1380, 1285, 1189, 1092, 995,
        897,  799,  700,  601,  501,  401,  301,  201,  101,
    };

    const short sinpi_arr_data[5] = { 0, 1321, 2482, 3344, 3803 };

    static void write_buffer_4x4_16s_8u(__m128i *in, uint8_t *output, int stride)
    {
        __m128i r0, r1;
        r0 = _mm_unpacklo_epi32(loadl_si32(output + 0 * stride), loadl_si32(output + 1 * stride));
        r1 = _mm_unpacklo_epi32(loadl_si32(output + 2 * stride), loadl_si32(output + 3 * stride));
        r0 = _mm_unpacklo_epi8(r0, _mm_setzero_si128());
        r1 = _mm_unpacklo_epi8(r1, _mm_setzero_si128());
        r0 = _mm_add_epi16(r0, in[0]);
        r1 = _mm_add_epi16(r1, in[1]);
        r0 = _mm_packus_epi16(r0, r1);
        storel_si32(output + 0 * stride, r0);
        storel_si32(output + 1 * stride, _mm_srli_si128(r0, 4));
        storel_si32(output + 2 * stride, _mm_srli_si128(r0, 8));
        storel_si32(output + 3 * stride, _mm_srli_si128(r0, 12));
    }

    static void write_buffer_8x8_16s_8u(__m128i *in, uint8_t *output, int stride)
    {
        __m128i r0, r1;
        for (int i = 0; i < 8; i += 2) {
            r0 = _mm_unpacklo_epi8(loadl_epi64(output + (i + 0) * stride), _mm_setzero_si128());
            r1 = _mm_unpacklo_epi8(loadl_epi64(output + (i + 1) * stride), _mm_setzero_si128());
            r0 = _mm_add_epi16(r0, in[i + 0]);
            r1 = _mm_add_epi16(r1, in[i + 1]);
            r0 = _mm_packus_epi16(r0, r1);
            storel_epi64(output + (i + 0) * stride, r0);
            storeh_epi64(output + (i + 1) * stride, r0);
        }
    }

    static void write_buffer_16x16_16s_8u(__m256i *in, uint8_t *output, int stride)
    {
        __m256i r0, r1;
        for (int i = 0; i < 16; i += 2) {
            r0 = _mm256_cvtepu8_epi16(loada_si128(output + i * stride));
            r1 = _mm256_cvtepu8_epi16(loada_si128(output + i * stride + stride));
            r0 = _mm256_add_epi16(r0, in[i + 0]);
            r1 = _mm256_add_epi16(r1, in[i + 1]);
            r0 = _mm256_packus_epi16(r0, r1);
            r0 = _mm256_permute4x64_epi64(r0, PERM4x64(0,2,1,3));
            storea2_m128i(output + i * stride, output + i * stride + stride, r0);
        }
    }

    static void write_buffer_32x32_16s_8u(__m256i *in, uint8_t *output, int stride)
    {
        __m256i r0, r1;
        for (int i = 0; i < 32; i++) {
            r0 = _mm256_cvtepu8_epi16(loada_si128(output + i * stride));
            r1 = _mm256_cvtepu8_epi16(loada_si128(output + i * stride + 16));
            r0 = _mm256_add_epi16(r0, in[2 * i + 0]);
            r1 = _mm256_add_epi16(r1, in[2 * i + 1]);
            r0 = _mm256_packus_epi16(r0, r1);
            r0 = _mm256_permute4x64_epi64(r0, PERM4x64(0,2,1,3));
            storea_si256(output + i * stride, r0);
        }
    }

    static void idct4x4_sse4_16s(__m128i *in, int final_shift)
    {
        const short *cospi = cospi_arr_data;
        const __m128i rnding   = _mm_set1_epi32(1 << 11);
        __m128i u02, u13, v01, v32, x, y;

        // in: A0 A1 A2 A3
        //     B0 B1 B2 B3
        //     C0 C1 C2 C3
        //     D0 D1 D2 D3

        u02 = _mm_packs_epi32(_mm_srai_epi32(_mm_slli_epi32(in[0], 16), 16),
                              _mm_srai_epi32(_mm_slli_epi32(in[1], 16), 16));
        u13 = _mm_packs_epi32(_mm_srai_epi32(in[0], 16),
                              _mm_srai_epi32(in[1], 16));

        // u02: A0 A2 B0 B2 C0 C2 D0 D2
        // u13: A1 A3 B1 B3 C1 C3 D1 D3

        x = _mm_madd_epi16(u02, _mm_set1_epi32( cospi[32] | ( cospi[32] << 16)));
        y = _mm_madd_epi16(u02, _mm_set1_epi32( cospi[32] | (-cospi[32] << 16)));
        x = _mm_add_epi32(x, rnding);
        y = _mm_add_epi32(y, rnding);
        x = _mm_srai_epi32(x, 12);
        y = _mm_srai_epi32(y, 12);
        v01 = _mm_packs_epi32(x, y);

        x = _mm_madd_epi16(u13, _mm_set1_epi32(cospi[16] | ( cospi[48] << 16)));
        y = _mm_madd_epi16(u13, _mm_set1_epi32(cospi[48] | (-cospi[16] << 16)));
        x = _mm_add_epi32(x, rnding);
        y = _mm_add_epi32(y, rnding);
        x = _mm_srai_epi32(x, 12);
        y = _mm_srai_epi32(y, 12);
        v32 = _mm_packs_epi32(x, y);

        const __m128i final_rounding = _mm_set1_epi16((1 << final_shift) >> 1);
        v01 = _mm_add_epi16(v01, final_rounding);

        in[0] = _mm_add_epi16(v01, v32);
        in[1] = _mm_sub_epi16(v01, v32);
        in[1] = _mm_shuffle_epi32(in[1], SHUF32(2,3,0,1));

        in[0] = _mm_srai_epi16(in[0], final_shift);
        in[1] = _mm_srai_epi16(in[1], final_shift);
    }

    static void iadst4x4_sse4_16s(__m128i *in, int final_shift) {
        const short *sinpi = sinpi_arr_data;
        const __m128i rnding = _mm_set1_epi32(1 << (12 - 1));
        const __m128i sinpi1 = _mm_set1_epi32((int)sinpi[1]);
        const __m128i sinpi2 = _mm_set1_epi32((int)sinpi[2]);
        const __m128i sinpi3 = _mm_set1_epi32((int)sinpi[3]);
        const __m128i sinpi4 = _mm_set1_epi32((int)sinpi[4]);
        __m128i t;
        __m128i s0, s1, s2, s3, s4, s5, s6, s7;
        __m128i x0, x1, x2, x3;
        __m128i u0, u1, u2, u3;
        __m128i v0, v1;

        // transpose
        u0 = _mm_unpacklo_epi16(in[0], in[1]);
        u1 = _mm_unpackhi_epi16(in[0], in[1]);
        v0 = _mm_unpacklo_epi16(u0, u1);
        v1 = _mm_unpackhi_epi16(u0, u1);

        // signs for extension
        u0 = _mm_srai_epi16(v0, 15);
        u1 = _mm_srai_epi16(v1, 15);

        // convert s16 to s32
        x0 = _mm_unpacklo_epi16(v0, u0);
        x1 = _mm_unpackhi_epi16(v0, u0);
        x2 = _mm_unpacklo_epi16(v1, u1);
        x3 = _mm_unpackhi_epi16(v1, u1);

        s0 = _mm_mullo_epi32(x0, sinpi1);
        s1 = _mm_mullo_epi32(x0, sinpi2);
        s2 = _mm_mullo_epi32(x1, sinpi3);
        s3 = _mm_mullo_epi32(x2, sinpi4);
        s4 = _mm_mullo_epi32(x2, sinpi1);
        s5 = _mm_mullo_epi32(x3, sinpi2);
        s6 = _mm_mullo_epi32(x3, sinpi4);
        t = _mm_sub_epi32(x0, x2);
        s7 = _mm_add_epi32(t, x3);

        t = _mm_add_epi32(s0, s3);
        s0 = _mm_add_epi32(t, s5);
        t = _mm_sub_epi32(s1, s4);
        s1 = _mm_sub_epi32(t, s6);
        s3 = s2;
        s2 = _mm_mullo_epi32(s7, sinpi3);

        u0 = _mm_add_epi32(s0, s3);
        u1 = _mm_add_epi32(s1, s3);
        u2 = s2;
        t = _mm_add_epi32(s0, s1);
        u3 = _mm_sub_epi32(t, s3);

        u0 = _mm_add_epi32(u0, rnding);
        u0 = _mm_srai_epi32(u0, 12);

        u1 = _mm_add_epi32(u1, rnding);
        u1 = _mm_srai_epi32(u1, 12);

        u2 = _mm_add_epi32(u2, rnding);
        u2 = _mm_srai_epi32(u2, 12);

        u3 = _mm_add_epi32(u3, rnding);
        u3 = _mm_srai_epi32(u3, 12);

        in[0] = _mm_packs_epi32(u0, u1);
        in[1] = _mm_packs_epi32(u2, u3);

        const __m128i final_rounding = _mm_set1_epi16((1 << final_shift) >> 1);
        in[0] = _mm_srai_epi16(_mm_add_epi16(in[0], final_rounding), final_shift);
        in[1] = _mm_srai_epi16(_mm_add_epi16(in[1], final_rounding), final_shift);
    }

    void av1_inv_txfm2d_add_4x4_sse4_1(const int16_t *coeff, __m128i *output, int tx_type)
    {
        const int shift0 = -inv_txfm_shift_ls[TX_4X4][0];
        const int shift1 = -inv_txfm_shift_ls[TX_4X4][1];

        output[0] = loada_si128(coeff + 0);
        output[1] = loada_si128(coeff + 8);

        if (tx_type == DCT_DCT) {
            idct4x4_sse4_16s(output, shift0);
            idct4x4_sse4_16s(output, shift1);
        } else if (tx_type == ADST_DCT) {
            idct4x4_sse4_16s(output, shift0);
            iadst4x4_sse4_16s(output, shift1);
        } else if (tx_type == DCT_ADST) {
            iadst4x4_sse4_16s(output, shift0);
            idct4x4_sse4_16s(output, shift1);
        } else if (tx_type == ADST_ADST) {
            iadst4x4_sse4_16s(output, shift0);
            iadst4x4_sse4_16s(output, shift1);
        }
    }

    // output = (w0 * n0 + w1 * n1 + (1 << 11)) >> 12
    inline __m256i half_btf_avx2(__m256i w0, __m256i n0, __m256i w1, __m256i n1)
    {
        const int shift = 12;
        const __m256i rounding = _mm256_set1_epi32(1 << (shift - 1));

        __m256i n_lo, n_hi, w_lo, w_hi, res_lo, res_hi;
        n_lo = _mm256_unpacklo_epi16(n0, n1);
        n_hi = _mm256_unpackhi_epi16(n0, n1);
        w_lo = _mm256_unpacklo_epi16(w0, w1);
        w_hi = _mm256_unpackhi_epi16(w0, w1);
        res_lo = _mm256_madd_epi16(n_lo, w_lo);
        res_hi = _mm256_madd_epi16(n_hi, w_hi);
        res_lo = _mm256_add_epi32(res_lo, rounding);
        res_hi = _mm256_add_epi32(res_hi, rounding);
        res_lo = _mm256_srai_epi32(res_lo, shift);
        res_hi = _mm256_srai_epi32(res_hi, shift);
        res_lo = _mm256_packs_epi32(res_lo, res_hi);

        return res_lo;
    }

    // output0 = (w0 * n0 + w1 * n1 + (1 << 11)) >> 12
    // output1 = (w1 * n0 - w0 * n1 + (1 << 11)) >> 12
    static inline void btf_avx2(__m256i w0, __m256i n0, __m256i w1, __m256i n1, __m256i *out0, __m256i *out1)
    {
        const int shift = 12;
        const __m256i rounding = _mm256_set1_epi32(1 << (shift - 1));

        __m256i n_lo, n_hi, w_lo, w_hi, res_lo, res_hi;
        n_lo = _mm256_unpacklo_epi16(n0, n1);
        n_hi = _mm256_unpackhi_epi16(n0, n1);
        w_lo = _mm256_unpacklo_epi16(w0, w1);
        w_hi = _mm256_unpackhi_epi16(w0, w1);
        res_lo = _mm256_madd_epi16(n_lo, w_lo);
        res_hi = _mm256_madd_epi16(n_hi, w_hi);
        res_lo = _mm256_add_epi32(res_lo, rounding);
        res_hi = _mm256_add_epi32(res_hi, rounding);
        res_lo = _mm256_srai_epi32(res_lo, shift);
        res_hi = _mm256_srai_epi32(res_hi, shift);
        *out0 = _mm256_packs_epi32(res_lo, res_hi);

        w0 = _mm256_sub_epi16(_mm256_setzero_si256(), w0);
        w_lo = _mm256_unpacklo_epi16(w1, w0);
        w_hi = _mm256_unpackhi_epi16(w1, w0);
        res_lo = _mm256_madd_epi16(n_lo, w_lo);
        res_hi = _mm256_madd_epi16(n_hi, w_hi);
        res_lo = _mm256_add_epi32(res_lo, rounding);
        res_hi = _mm256_add_epi32(res_hi, rounding);
        res_lo = _mm256_srai_epi32(res_lo, shift);
        res_hi = _mm256_srai_epi32(res_hi, shift);
        *out1 = _mm256_packs_epi32(res_lo, res_hi);
    }

    // output0 = (w0 * n0 + w1 * n1 + (1 << 11)) >> 12
    // output1 = (w1 * n0 - w0 * n1 + (1 << 11)) >> 12
    static inline void btf_sse4(__m128i w0, __m128i n0, __m128i w1, __m128i n1, __m128i *out0, __m128i *out1)
    {
        const int shift = 12;
        const __m128i rounding = _mm_set1_epi32(1 << (shift - 1));

        __m128i n_lo, n_hi, w_lo, w_hi, res_lo, res_hi;
        n_lo = _mm_unpacklo_epi16(n0, n1);
        n_hi = _mm_unpackhi_epi16(n0, n1);
        w_lo = _mm_unpacklo_epi16(w0, w1);
        w_hi = _mm_unpackhi_epi16(w0, w1);
        res_lo = _mm_madd_epi16(n_lo, w_lo);
        res_hi = _mm_madd_epi16(n_hi, w_hi);
        res_lo = _mm_add_epi32(res_lo, rounding);
        res_hi = _mm_add_epi32(res_hi, rounding);
        res_lo = _mm_srai_epi32(res_lo, shift);
        res_hi = _mm_srai_epi32(res_hi, shift);
        *out0 = _mm_packs_epi32(res_lo, res_hi);

        w0 = _mm_sub_epi16(_mm_setzero_si128(), w0);
        w_lo = _mm_unpacklo_epi16(w1, w0);
        w_hi = _mm_unpackhi_epi16(w1, w0);
        res_lo = _mm_madd_epi16(n_lo, w_lo);
        res_hi = _mm_madd_epi16(n_hi, w_hi);
        res_lo = _mm_add_epi32(res_lo, rounding);
        res_hi = _mm_add_epi32(res_hi, rounding);
        res_lo = _mm_srai_epi32(res_lo, shift);
        res_hi = _mm_srai_epi32(res_hi, shift);
        *out1 = _mm_packs_epi32(res_lo, res_hi);
    }

    // output0 = (w * n0 + w * n1 + (1 << 11)) >> 12
    // output1 = (w * n0 - w * n1 + (1 << 11)) >> 12
    static inline void btf_avx2(__m256i w, __m256i n0, __m256i n1, __m256i *out0, __m256i *out1)
    {
        const int shift = 12;
        const __m256i rounding = _mm256_set1_epi32(1 << (shift - 1));
        __m256i n_lo, n_hi, res_lo, res_hi, neg_w;

        n_lo = _mm256_unpacklo_epi16(n0, n1);
        n_hi = _mm256_unpackhi_epi16(n0, n1);
        res_lo = _mm256_madd_epi16(n_lo, w);
        res_hi = _mm256_madd_epi16(n_hi, w);
        res_lo = _mm256_add_epi32(res_lo, rounding);
        res_hi = _mm256_add_epi32(res_hi, rounding);
        res_lo = _mm256_srai_epi32(res_lo, shift);
        res_hi = _mm256_srai_epi32(res_hi, shift);
        *out0 = _mm256_packs_epi32(res_lo, res_hi);

        neg_w = _mm256_sign_epi16(w, _mm256_set1_epi32(0x80000001));
        res_lo = _mm256_madd_epi16(n_lo, neg_w);
        res_hi = _mm256_madd_epi16(n_hi, neg_w);
        res_lo = _mm256_add_epi32(res_lo, rounding);
        res_hi = _mm256_add_epi32(res_hi, rounding);
        res_lo = _mm256_srai_epi32(res_lo, shift);
        res_hi = _mm256_srai_epi32(res_hi, shift);
        *out1 = _mm256_packs_epi32(res_lo, res_hi);
    }

    // output0 = (w * n0 + w * n1 + (1 << 11)) >> 12
    // output1 = (w * n0 - w * n1 + (1 << 11)) >> 12
    static inline void btf_sse4(__m128i w, __m128i n0, __m128i n1, __m128i *out0, __m128i *out1)
    {
        const int shift = 12;
        const __m128i rounding = _mm_set1_epi32(1 << (shift - 1));
        __m128i n_lo, n_hi, res_lo, res_hi, neg_w;

        n_lo = _mm_unpacklo_epi16(n0, n1);
        n_hi = _mm_unpackhi_epi16(n0, n1);
        res_lo = _mm_madd_epi16(n_lo, w);
        res_hi = _mm_madd_epi16(n_hi, w);
        res_lo = _mm_add_epi32(res_lo, rounding);
        res_hi = _mm_add_epi32(res_hi, rounding);
        res_lo = _mm_srai_epi32(res_lo, shift);
        res_hi = _mm_srai_epi32(res_hi, shift);
        *out0 = _mm_packs_epi32(res_lo, res_hi);

        neg_w = _mm_sign_epi16(w, _mm_set1_epi32(0x80000001));
        res_lo = _mm_madd_epi16(n_lo, neg_w);
        res_hi = _mm_madd_epi16(n_hi, neg_w);
        res_lo = _mm_add_epi32(res_lo, rounding);
        res_hi = _mm_add_epi32(res_hi, rounding);
        res_lo = _mm_srai_epi32(res_lo, shift);
        res_hi = _mm_srai_epi32(res_hi, shift);
        *out1 = _mm_packs_epi32(res_lo, res_hi);
    }

    static void idct8x8_sse4_16s(__m128i *in, __m128i *out, int final_shift)
    {
        const short *cospi = cospi_arr_data;
        const __m128i cospi56  = _mm_set1_epi16(cospi[56]);
        const __m128i cospi24  = _mm_set1_epi16(cospi[24]);
        const __m128i cospi40  = _mm_set1_epi16(cospi[40]);
        const __m128i cospi8   = _mm_set1_epi16(cospi[8]);
        const __m128i cospi32  = _mm_set1_epi16(cospi[32]);
        const __m128i cospi48  = _mm_set1_epi16(cospi[48]);
        const __m128i cospi16  = _mm_set1_epi16(cospi[16]);
        __m128i u[8], v[8];

        // stage 0
        // stage 1
        // stage 2
        btf_sse4(cospi8,  in[1], cospi56, in[7], &u[7], &u[4]);
        btf_sse4(cospi40, in[5], cospi24, in[3], &u[6], &u[5]);

        // stage 3
        btf_sse4(cospi32, in[0],          in[4], &v[0], &v[1]);
        btf_sse4(cospi16, in[2], cospi48, in[6], &v[3], &v[2]);
        v[4] = _mm_add_epi16(u[4], u[5]);
        v[5] = _mm_sub_epi16(u[4], u[5]);
        v[6] = _mm_sub_epi16(u[7], u[6]);
        v[7] = _mm_add_epi16(u[6], u[7]);

        // stage 4
        u[0] = _mm_add_epi16(v[0], v[3]);
        u[1] = _mm_add_epi16(v[1], v[2]);
        u[2] = _mm_sub_epi16(v[1], v[2]);
        u[3] = _mm_sub_epi16(v[0], v[3]);
        btf_sse4(cospi32, v[6], v[5], &u[6], &u[5]);

        const __m128i final_rounding = _mm_set1_epi16((1 << final_shift) >> 1);
        u[0] = _mm_add_epi16(u[0], final_rounding);
        u[1] = _mm_add_epi16(u[1], final_rounding);
        u[2] = _mm_add_epi16(u[2], final_rounding);
        u[3] = _mm_add_epi16(u[3], final_rounding);

        // stage 5
        out[0] = _mm_add_epi16(u[0], v[7]);
        out[1] = _mm_add_epi16(u[1], u[6]);
        out[2] = _mm_add_epi16(u[2], u[5]);
        out[3] = _mm_add_epi16(u[3], v[4]);
        out[4] = _mm_sub_epi16(u[3], v[4]);
        out[5] = _mm_sub_epi16(u[2], u[5]);
        out[6] = _mm_sub_epi16(u[1], u[6]);
        out[7] = _mm_sub_epi16(u[0], v[7]);

        out[0] = _mm_srai_epi16(out[0], final_shift);
        out[1] = _mm_srai_epi16(out[1], final_shift);
        out[2] = _mm_srai_epi16(out[2], final_shift);
        out[3] = _mm_srai_epi16(out[3], final_shift);
        out[4] = _mm_srai_epi16(out[4], final_shift);
        out[5] = _mm_srai_epi16(out[5], final_shift);
        out[6] = _mm_srai_epi16(out[6], final_shift);
        out[7] = _mm_srai_epi16(out[7], final_shift);
    }

    static void iadst8x8_sse4_16s(__m128i *in, __m128i *out, int final_shift)
    {
        const short *cospi = cospi_arr_data;
        const __m128i cospi4   = _mm_set1_epi16( cospi[4]);
        const __m128i cospi60  = _mm_set1_epi16( cospi[60]);
        const __m128i cospi20  = _mm_set1_epi16( cospi[20]);
        const __m128i cospi44  = _mm_set1_epi16( cospi[44]);
        const __m128i cospi36  = _mm_set1_epi16( cospi[36]);
        const __m128i cospi28  = _mm_set1_epi16( cospi[28]);
        const __m128i cospi52  = _mm_set1_epi16( cospi[52]);
        const __m128i cospi12  = _mm_set1_epi16( cospi[12]);
        const __m128i cospi16  = _mm_set1_epi16( cospi[16]);
        const __m128i cospi48  = _mm_set1_epi16( cospi[48]);
        const __m128i cospim48 = _mm_set1_epi16(-cospi[48]);
        const __m128i cospi32  = _mm_set1_epi16( cospi[32]);
        __m128i u[8], v[8];

        // Even 8 points: 0, 2, ..., 14
        // stage 0
        // stage 1
        // stage 2
        btf_sse4(cospi4,  in[7], cospi60, in[0], &u[0], &u[1]);
        btf_sse4(cospi20, in[5], cospi44, in[2], &u[2], &u[3]);
        btf_sse4(cospi36, in[3], cospi28, in[4], &u[4], &u[5]);
        btf_sse4(cospi52, in[1], cospi12, in[6], &u[6], &u[7]);

        // stage 3
        v[0] = _mm_add_epi16(u[0], u[4]);
        v[4] = _mm_sub_epi16(u[0], u[4]);
        v[1] = _mm_add_epi16(u[1], u[5]);
        v[5] = _mm_sub_epi16(u[1], u[5]);
        v[2] = _mm_add_epi16(u[2], u[6]);
        v[6] = _mm_sub_epi16(u[2], u[6]);
        v[3] = _mm_add_epi16(u[3], u[7]);
        v[7] = _mm_sub_epi16(u[3], u[7]);

        // stage 4
        u[0] = v[0];
        u[1] = v[1];
        u[2] = v[2];
        u[3] = v[3];
        btf_sse4(cospi16,  v[4], cospi48, v[5], &u[4], &u[5]);
        btf_sse4(cospim48, v[6], cospi16, v[7], &u[6], &u[7]);

        // stage 5
        v[0] = _mm_add_epi16(u[0], u[2]);
        v[1] = _mm_add_epi16(u[1], u[3]);
        v[2] = _mm_sub_epi16(u[0], u[2]);
        v[3] = _mm_sub_epi16(u[1], u[3]);
        v[4] = _mm_add_epi16(u[4], u[6]);
        v[6] = _mm_sub_epi16(u[4], u[6]);
        v[5] = _mm_add_epi16(u[5], u[7]);
        v[7] = _mm_sub_epi16(u[5], u[7]);

        // stage 6
        u[0] = v[0];
        u[1] = v[1];
        btf_sse4(cospi32, v[2], v[3], &u[2], &u[3]);
        u[4] = v[4];
        u[5] = v[5];
        btf_sse4(cospi32, v[6], v[7], &u[6], &u[7]);

        // stage 7
        const __m128i final_rounding = _mm_set1_epi16((1 << final_shift) >> 1);
        out[0] = _mm_srai_epi16(_mm_add_epi16(final_rounding, u[0]), final_shift);
        out[1] = _mm_srai_epi16(_mm_sub_epi16(final_rounding, u[4]), final_shift);
        out[2] = _mm_srai_epi16(_mm_add_epi16(final_rounding, u[6]), final_shift);
        out[3] = _mm_srai_epi16(_mm_sub_epi16(final_rounding, u[2]), final_shift);
        out[4] = _mm_srai_epi16(_mm_add_epi16(final_rounding, u[3]), final_shift);
        out[5] = _mm_srai_epi16(_mm_sub_epi16(final_rounding, u[7]), final_shift);
        out[6] = _mm_srai_epi16(_mm_add_epi16(final_rounding, u[5]), final_shift);
        out[7] = _mm_srai_epi16(_mm_sub_epi16(final_rounding, u[1]), final_shift);
    }

    void av1_inv_txfm2d_add_8x8_sse4_1(const int16_t *coeff, __m128i *out, int tx_type)
    {
        const int shift0 = -inv_txfm_shift_ls[TX_8X8][0];
        const int shift1 = -inv_txfm_shift_ls[TX_8X8][1];
        __m128i in[8];

        for (int i = 0; i < 8; i++)
            out[i] = loada_si128(coeff + i * 8);
        transpose_8x8_16s(out, in);

        ((tx_type == DCT_DCT || tx_type == ADST_DCT)
            ? idct8x8_sse4_16s
            : iadst8x8_sse4_16s)(in, out, shift0);

        transpose_8x8_16s(out, in);

        ((tx_type == DCT_DCT || tx_type == DCT_ADST)
            ? idct8x8_sse4_16s
            : iadst8x8_sse4_16s)(in, out, shift1);
    }

    static inline void transpose_16x16_16s(__m256i *in, __m256i *out, int pitch)
    {
        __m256i u[16], v[16];
        u[ 0] = _mm256_unpacklo_epi16(in[ 0 * pitch], in[ 1 * pitch]);
        u[ 1] = _mm256_unpacklo_epi16(in[ 2 * pitch], in[ 3 * pitch]);
        u[ 2] = _mm256_unpacklo_epi16(in[ 4 * pitch], in[ 5 * pitch]);
        u[ 3] = _mm256_unpacklo_epi16(in[ 6 * pitch], in[ 7 * pitch]);
        u[ 4] = _mm256_unpacklo_epi16(in[ 8 * pitch], in[ 9 * pitch]);
        u[ 5] = _mm256_unpacklo_epi16(in[10 * pitch], in[11 * pitch]);
        u[ 6] = _mm256_unpacklo_epi16(in[12 * pitch], in[13 * pitch]);
        u[ 7] = _mm256_unpacklo_epi16(in[14 * pitch], in[15 * pitch]);
        u[ 8] = _mm256_unpackhi_epi16(in[ 0 * pitch], in[ 1 * pitch]);
        u[ 9] = _mm256_unpackhi_epi16(in[ 2 * pitch], in[ 3 * pitch]);
        u[10] = _mm256_unpackhi_epi16(in[ 4 * pitch], in[ 5 * pitch]);
        u[11] = _mm256_unpackhi_epi16(in[ 6 * pitch], in[ 7 * pitch]);
        u[12] = _mm256_unpackhi_epi16(in[ 8 * pitch], in[ 9 * pitch]);
        u[13] = _mm256_unpackhi_epi16(in[10 * pitch], in[11 * pitch]);
        u[14] = _mm256_unpackhi_epi16(in[12 * pitch], in[13 * pitch]);
        u[15] = _mm256_unpackhi_epi16(in[14 * pitch], in[15 * pitch]);

        v[ 0] = _mm256_unpacklo_epi32(u[ 0], u[ 1]);
        v[ 1] = _mm256_unpacklo_epi32(u[ 2], u[ 3]);
        v[ 2] = _mm256_unpacklo_epi32(u[ 4], u[ 5]);
        v[ 3] = _mm256_unpacklo_epi32(u[ 6], u[ 7]);
        v[ 4] = _mm256_unpackhi_epi32(u[ 0], u[ 1]);
        v[ 5] = _mm256_unpackhi_epi32(u[ 2], u[ 3]);
        v[ 6] = _mm256_unpackhi_epi32(u[ 4], u[ 5]);
        v[ 7] = _mm256_unpackhi_epi32(u[ 6], u[ 7]);
        v[ 8] = _mm256_unpacklo_epi32(u[ 8], u[ 9]);
        v[ 9] = _mm256_unpacklo_epi32(u[10], u[11]);
        v[10] = _mm256_unpacklo_epi32(u[12], u[13]);
        v[11] = _mm256_unpacklo_epi32(u[14], u[15]);
        v[12] = _mm256_unpackhi_epi32(u[ 8], u[ 9]);
        v[13] = _mm256_unpackhi_epi32(u[10], u[11]);
        v[14] = _mm256_unpackhi_epi32(u[12], u[13]);
        v[15] = _mm256_unpackhi_epi32(u[14], u[15]);

        u[ 0] = _mm256_unpacklo_epi64(v[ 0], v[ 1]);
        u[ 1] = _mm256_unpacklo_epi64(v[ 2], v[ 3]);
        u[ 2] = _mm256_unpackhi_epi64(v[ 0], v[ 1]);
        u[ 3] = _mm256_unpackhi_epi64(v[ 2], v[ 3]);
        u[ 4] = _mm256_unpacklo_epi64(v[ 4], v[ 5]);
        u[ 5] = _mm256_unpacklo_epi64(v[ 6], v[ 7]);
        u[ 6] = _mm256_unpackhi_epi64(v[ 4], v[ 5]);
        u[ 7] = _mm256_unpackhi_epi64(v[ 6], v[ 7]);
        u[ 8] = _mm256_unpacklo_epi64(v[ 8], v[ 9]);
        u[ 9] = _mm256_unpacklo_epi64(v[10], v[11]);
        u[10] = _mm256_unpackhi_epi64(v[ 8], v[ 9]);
        u[11] = _mm256_unpackhi_epi64(v[10], v[11]);
        u[12] = _mm256_unpacklo_epi64(v[12], v[13]);
        u[13] = _mm256_unpacklo_epi64(v[14], v[15]);
        u[14] = _mm256_unpackhi_epi64(v[12], v[13]);
        u[15] = _mm256_unpackhi_epi64(v[14], v[15]);

        out[ 0 * pitch] = _mm256_permute2x128_si256(u[ 0], u[ 1], PERM2x128(0,2));
        out[ 1 * pitch] = _mm256_permute2x128_si256(u[ 2], u[ 3], PERM2x128(0,2));
        out[ 2 * pitch] = _mm256_permute2x128_si256(u[ 4], u[ 5], PERM2x128(0,2));
        out[ 3 * pitch] = _mm256_permute2x128_si256(u[ 6], u[ 7], PERM2x128(0,2));
        out[ 4 * pitch] = _mm256_permute2x128_si256(u[ 8], u[ 9], PERM2x128(0,2));
        out[ 5 * pitch] = _mm256_permute2x128_si256(u[10], u[11], PERM2x128(0,2));
        out[ 6 * pitch] = _mm256_permute2x128_si256(u[12], u[13], PERM2x128(0,2));
        out[ 7 * pitch] = _mm256_permute2x128_si256(u[14], u[15], PERM2x128(0,2));
        out[ 8 * pitch] = _mm256_permute2x128_si256(u[ 0], u[ 1], PERM2x128(1,3));
        out[ 9 * pitch] = _mm256_permute2x128_si256(u[ 2], u[ 3], PERM2x128(1,3));
        out[10 * pitch] = _mm256_permute2x128_si256(u[ 4], u[ 5], PERM2x128(1,3));
        out[11 * pitch] = _mm256_permute2x128_si256(u[ 6], u[ 7], PERM2x128(1,3));
        out[12 * pitch] = _mm256_permute2x128_si256(u[ 8], u[ 9], PERM2x128(1,3));
        out[13 * pitch] = _mm256_permute2x128_si256(u[10], u[11], PERM2x128(1,3));
        out[14 * pitch] = _mm256_permute2x128_si256(u[12], u[13], PERM2x128(1,3));
        out[15 * pitch] = _mm256_permute2x128_si256(u[14], u[15], PERM2x128(1,3));
    }

    static inline void transpose_32x32_16s(__m256i *in, __m256i *out)
    {
        transpose_16x16_16s(in +  0, out +  0, 2);
        transpose_16x16_16s(in +  1, out + 32, 2);
        transpose_16x16_16s(in + 32, out +  1, 2);
        transpose_16x16_16s(in + 33, out + 33, 2);
    }

    static void idct16x16_avx2_16s(__m256i *in, __m256i *out, int final_shift) {
        const short *cospi = cospi_arr_data;
        const __m256i cospi60  = _mm256_set1_epi16( cospi[60]);
        const __m256i cospi28  = _mm256_set1_epi16( cospi[28]);
        const __m256i cospi44  = _mm256_set1_epi16( cospi[44]);
        const __m256i cospi20  = _mm256_set1_epi16( cospi[20]);
        const __m256i cospi12  = _mm256_set1_epi16( cospi[12]);
        const __m256i cospi52  = _mm256_set1_epi16( cospi[52]);
        const __m256i cospi36  = _mm256_set1_epi16( cospi[36]);
        const __m256i cospi4   = _mm256_set1_epi16( cospi[ 4]);
        const __m256i cospi56  = _mm256_set1_epi16( cospi[56]);
        const __m256i cospi24  = _mm256_set1_epi16( cospi[24]);
        const __m256i cospi40  = _mm256_set1_epi16( cospi[40]);
        const __m256i cospi8   = _mm256_set1_epi16( cospi[ 8]);
        const __m256i cospi32  = _mm256_set1_epi16( cospi[32]);
        const __m256i cospi48  = _mm256_set1_epi16( cospi[48]);
        const __m256i cospi16  = _mm256_set1_epi16( cospi[16]);
        const __m256i cospim16 = _mm256_set1_epi16(-cospi[16]);
        const __m256i cospim48 = _mm256_set1_epi16(-cospi[48]);
        __m256i u[16], v[16];

        // stage 2
        btf_avx2(cospi4,  in[ 1], cospi60, in[15], &v[15], &v[ 8]);
        btf_avx2(cospi36, in[ 9], cospi28, in[ 7], &v[14], &v[ 9]);
        btf_avx2(cospi20, in[ 5], cospi44, in[11], &v[13], &v[10]);
        btf_avx2(cospi52, in[13], cospi12, in[ 3], &v[12], &v[11]);

        // stage 3
        btf_avx2(cospi8,  in[ 2], cospi56, in[14], &u[ 7], &u[ 4]);
        btf_avx2(cospi40, in[10], cospi24, in[ 6], &u[ 6], &u[ 5]);
        u[ 8] = _mm256_add_epi16(v[ 8], v[ 9]);
        u[ 9] = _mm256_sub_epi16(v[ 8], v[ 9]);
        u[10] = _mm256_sub_epi16(v[11], v[10]);
        u[11] = _mm256_add_epi16(v[10], v[11]);
        u[12] = _mm256_add_epi16(v[12], v[13]);
        u[13] = _mm256_sub_epi16(v[12], v[13]);
        u[14] = _mm256_sub_epi16(v[15], v[14]);
        u[15] = _mm256_add_epi16(v[14], v[15]);

        // stage 4
        v[ 4] = _mm256_add_epi16(u[4], u[5]);
        v[ 5] = _mm256_sub_epi16(u[4], u[5]);
        v[ 6] = _mm256_sub_epi16(u[7], u[6]);
        v[ 7] = _mm256_add_epi16(u[6], u[7]);
        v[ 8] = u[8];
        v[11] = u[11];
        v[12] = u[12];
        v[15] = u[15];
        btf_avx2(cospi32, in[ 0],          in[ 8], &v[ 0], &v[ 1]);
        btf_avx2(cospi16, in[ 4], cospi48, in[12], &v[ 3], &v[ 2]);
        btf_avx2(cospim16, u[ 9], cospi48,  u[14], &v[ 9], &v[14]);
        btf_avx2(cospim48, u[10], cospim16, u[13], &v[10], &v[13]);

        // stage 5
        u[0] = _mm256_add_epi16(v[0], v[3]);
        u[1] = _mm256_add_epi16(v[1], v[2]);
        u[2] = _mm256_sub_epi16(v[1], v[2]);
        u[3] = _mm256_sub_epi16(v[0], v[3]);
        u[4] = v[4];
        btf_avx2(cospi32, v[6], v[5], &u[6], &u[5]);
        u[ 7] = v[7];
        u[ 8] = _mm256_add_epi16(v[ 8], v[11]);
        u[ 9] = _mm256_add_epi16(v[ 9], v[10]);
        u[10] = _mm256_sub_epi16(v[ 9], v[10]);
        u[11] = _mm256_sub_epi16(v[ 8], v[11]);
        u[12] = _mm256_sub_epi16(v[15], v[12]);
        u[13] = _mm256_sub_epi16(v[14], v[13]);
        u[14] = _mm256_add_epi16(v[13], v[14]);
        u[15] = _mm256_add_epi16(v[12], v[15]);

        // stage 6
        v[0] = _mm256_add_epi16(u[0], u[7]);
        v[1] = _mm256_add_epi16(u[1], u[6]);
        v[2] = _mm256_add_epi16(u[2], u[5]);
        v[3] = _mm256_add_epi16(u[3], u[4]);
        v[4] = _mm256_sub_epi16(u[3], u[4]);
        v[5] = _mm256_sub_epi16(u[2], u[5]);
        v[6] = _mm256_sub_epi16(u[1], u[6]);
        v[7] = _mm256_sub_epi16(u[0], u[7]);
        v[ 8] = u[8];
        v[ 9] = u[9];
        btf_avx2(cospi32, u[13], u[10], &v[13], &v[10]);
        btf_avx2(cospi32, u[12], u[11], &v[12], &v[11]);
        v[14] = u[14];
        v[15] = u[15];

        const __m256i final_rounding = _mm256_set1_epi16((1 << final_shift) >> 1);
        v[0] = _mm256_add_epi16(v[0], final_rounding);
        v[1] = _mm256_add_epi16(v[1], final_rounding);
        v[2] = _mm256_add_epi16(v[2], final_rounding);
        v[3] = _mm256_add_epi16(v[3], final_rounding);
        v[4] = _mm256_add_epi16(v[4], final_rounding);
        v[5] = _mm256_add_epi16(v[5], final_rounding);
        v[6] = _mm256_add_epi16(v[6], final_rounding);
        v[7] = _mm256_add_epi16(v[7], final_rounding);

        // stage 7
        u[ 0] = _mm256_add_epi16(v[0], v[15]);
        u[ 1] = _mm256_add_epi16(v[1], v[14]);
        u[ 2] = _mm256_add_epi16(v[2], v[13]);
        u[ 3] = _mm256_add_epi16(v[3], v[12]);
        u[ 4] = _mm256_add_epi16(v[4], v[11]);
        u[ 5] = _mm256_add_epi16(v[5], v[10]);
        u[ 6] = _mm256_add_epi16(v[6], v[ 9]);
        u[ 7] = _mm256_add_epi16(v[7], u[ 8]);
        u[ 8] = _mm256_sub_epi16(v[7], v[ 8]);
        u[ 9] = _mm256_sub_epi16(v[6], v[ 9]);
        u[10] = _mm256_sub_epi16(v[5], v[10]);
        u[11] = _mm256_sub_epi16(v[4], v[11]);
        u[12] = _mm256_sub_epi16(v[3], v[12]);
        u[13] = _mm256_sub_epi16(v[2], v[13]);
        u[14] = _mm256_sub_epi16(v[1], v[14]);
        u[15] = _mm256_sub_epi16(v[0], v[15]);

        out[ 0] = _mm256_srai_epi16(u[ 0], final_shift);
        out[ 1] = _mm256_srai_epi16(u[ 1], final_shift);
        out[ 2] = _mm256_srai_epi16(u[ 2], final_shift);
        out[ 3] = _mm256_srai_epi16(u[ 3], final_shift);
        out[ 4] = _mm256_srai_epi16(u[ 4], final_shift);
        out[ 5] = _mm256_srai_epi16(u[ 5], final_shift);
        out[ 6] = _mm256_srai_epi16(u[ 6], final_shift);
        out[ 7] = _mm256_srai_epi16(u[ 7], final_shift);
        out[ 8] = _mm256_srai_epi16(u[ 8], final_shift);
        out[ 9] = _mm256_srai_epi16(u[ 9], final_shift);
        out[10] = _mm256_srai_epi16(u[10], final_shift);
        out[11] = _mm256_srai_epi16(u[11], final_shift);
        out[12] = _mm256_srai_epi16(u[12], final_shift);
        out[13] = _mm256_srai_epi16(u[13], final_shift);
        out[14] = _mm256_srai_epi16(u[14], final_shift);
        out[15] = _mm256_srai_epi16(u[15], final_shift);
    }

    static void iadst16x16_avx2_16s(__m256i *in, __m256i *out, int final_shift)
    {
        const short *cospi = cospi_arr_data;
        const __m256i cospi2   = _mm256_set1_epi16( cospi[2]);
        const __m256i cospi62  = _mm256_set1_epi16( cospi[62]);
        const __m256i cospi10  = _mm256_set1_epi16( cospi[10]);
        const __m256i cospi54  = _mm256_set1_epi16( cospi[54]);
        const __m256i cospi18  = _mm256_set1_epi16( cospi[18]);
        const __m256i cospi46  = _mm256_set1_epi16( cospi[46]);
        const __m256i cospi26  = _mm256_set1_epi16( cospi[26]);
        const __m256i cospi38  = _mm256_set1_epi16( cospi[38]);
        const __m256i cospi34  = _mm256_set1_epi16( cospi[34]);
        const __m256i cospi30  = _mm256_set1_epi16( cospi[30]);
        const __m256i cospi42  = _mm256_set1_epi16( cospi[42]);
        const __m256i cospi22  = _mm256_set1_epi16( cospi[22]);
        const __m256i cospi50  = _mm256_set1_epi16( cospi[50]);
        const __m256i cospi14  = _mm256_set1_epi16( cospi[14]);
        const __m256i cospi58  = _mm256_set1_epi16( cospi[58]);
        const __m256i cospi6   = _mm256_set1_epi16( cospi[6]);
        const __m256i cospi8   = _mm256_set1_epi16( cospi[8]);
        const __m256i cospi56  = _mm256_set1_epi16( cospi[56]);
        const __m256i cospi40  = _mm256_set1_epi16( cospi[40]);
        const __m256i cospi24  = _mm256_set1_epi16( cospi[24]);
        const __m256i cospim56 = _mm256_set1_epi16(-cospi[56]);
        const __m256i cospim24 = _mm256_set1_epi16(-cospi[24]);
        const __m256i cospi48  = _mm256_set1_epi16( cospi[48]);
        const __m256i cospi16  = _mm256_set1_epi16( cospi[16]);
        const __m256i cospim48 = _mm256_set1_epi16(-cospi[48]);
        const __m256i cospi32  = _mm256_set1_epi16( cospi[32]);

        __m256i u[16], v[16];

        // stage 0
        // stage 1
        // stage 2
        btf_avx2(cospi2,  in[15], cospi62, in[ 0], &v[ 0], &v[ 1]);
        btf_avx2(cospi10, in[13], cospi54, in[ 2], &v[ 2], &v[ 3]);
        btf_avx2(cospi18, in[11], cospi46, in[ 4], &v[ 4], &v[ 5]);
        btf_avx2(cospi26, in[ 9], cospi38, in[ 6], &v[ 6], &v[ 7]);
        btf_avx2(cospi34, in[ 7], cospi30, in[ 8], &v[ 8], &v[ 9]);
        btf_avx2(cospi42, in[ 5], cospi22, in[10], &v[10], &v[11]);
        btf_avx2(cospi50, in[ 3], cospi14, in[12], &v[12], &v[13]);
        btf_avx2(cospi58, in[ 1], cospi6,  in[14], &v[14], &v[15]);

        // stage 3
        u[ 0] = _mm256_add_epi16(v[0], v[ 8]);
        u[ 8] = _mm256_sub_epi16(v[0], v[ 8]);
        u[ 1] = _mm256_add_epi16(v[1], v[ 9]);
        u[ 9] = _mm256_sub_epi16(v[1], v[ 9]);
        u[ 2] = _mm256_add_epi16(v[2], v[10]);
        u[10] = _mm256_sub_epi16(v[2], v[10]);
        u[ 3] = _mm256_add_epi16(v[3], v[11]);
        u[11] = _mm256_sub_epi16(v[3], v[11]);
        u[ 4] = _mm256_add_epi16(v[4], v[12]);
        u[12] = _mm256_sub_epi16(v[4], v[12]);
        u[ 5] = _mm256_add_epi16(v[5], v[13]);
        u[13] = _mm256_sub_epi16(v[5], v[13]);
        u[ 6] = _mm256_add_epi16(v[6], v[14]);
        u[14] = _mm256_sub_epi16(v[6], v[14]);
        u[ 7] = _mm256_add_epi16(v[7], v[15]);
        u[15] = _mm256_sub_epi16(v[7], v[15]);

        // stage 4
        v[ 0] = u[ 0];
        v[ 1] = u[ 1];
        v[ 2] = u[ 2];
        v[ 3] = u[ 3];
        v[ 4] = u[ 4];
        v[ 5] = u[ 5];
        v[ 6] = u[ 6];
        v[ 7] = u[ 7];
        btf_avx2(cospi8,   u[ 8], cospi56, u[ 9], &v[ 8], &v[ 9]);
        btf_avx2(cospi40,  u[10], cospi24, u[11], &v[10], &v[11]);
        btf_avx2(cospim56, u[12], cospi8,  u[13], &v[12], &v[13]);
        btf_avx2(cospim24, u[14], cospi40, u[15], &v[14], &v[15]);

        // stage 5
        u[ 0] = _mm256_add_epi16(v[ 0], v[ 4]);
        u[ 4] = _mm256_sub_epi16(v[ 0], v[ 4]);
        u[ 1] = _mm256_add_epi16(v[ 1], v[ 5]);
        u[ 5] = _mm256_sub_epi16(v[ 1], v[ 5]);
        u[ 2] = _mm256_add_epi16(v[ 2], v[ 6]);
        u[ 6] = _mm256_sub_epi16(v[ 2], v[ 6]);
        u[ 3] = _mm256_add_epi16(v[ 3], v[ 7]);
        u[ 7] = _mm256_sub_epi16(v[ 3], v[ 7]);
        u[ 8] = _mm256_add_epi16(v[ 8], v[12]);
        u[12] = _mm256_sub_epi16(v[ 8], v[12]);
        u[ 9] = _mm256_add_epi16(v[ 9], v[13]);
        u[13] = _mm256_sub_epi16(v[ 9], v[13]);
        u[10] = _mm256_add_epi16(v[10], v[14]);
        u[14] = _mm256_sub_epi16(v[10], v[14]);
        u[11] = _mm256_add_epi16(v[11], v[15]);
        u[15] = _mm256_sub_epi16(v[11], v[15]);

        // stage 6
        v[ 0] = u[ 0];
        v[ 1] = u[ 1];
        v[ 2] = u[ 2];
        v[ 3] = u[ 3];
        btf_avx2(cospi16,  u[ 4], cospi48, u[ 5], &v[ 4], &v[ 5]);
        btf_avx2(cospim48, u[ 6], cospi16, u[ 7], &v[ 6], &v[ 7]);
        v[ 8] = u[ 8];
        v[ 9] = u[ 9];
        v[10] = u[10];
        v[11] = u[11];
        btf_avx2(cospi16,  u[12], cospi48, u[13], &v[12], &v[13]);
        btf_avx2(cospim48, u[14], cospi16, u[15], &v[14], &v[15]);

        // stage 7
        u[ 0] = _mm256_add_epi16(v[ 0], v[ 2]);
        u[ 2] = _mm256_sub_epi16(v[ 0], v[ 2]);
        u[ 1] = _mm256_add_epi16(v[ 1], v[ 3]);
        u[ 3] = _mm256_sub_epi16(v[ 1], v[ 3]);
        u[ 4] = _mm256_add_epi16(v[ 4], v[ 6]);
        u[ 6] = _mm256_sub_epi16(v[ 4], v[ 6]);
        u[ 5] = _mm256_add_epi16(v[ 5], v[ 7]);
        u[ 7] = _mm256_sub_epi16(v[ 5], v[ 7]);
        u[ 8] = _mm256_add_epi16(v[ 8], v[10]);
        u[10] = _mm256_sub_epi16(v[ 8], v[10]);
        u[ 9] = _mm256_add_epi16(v[ 9], v[11]);
        u[11] = _mm256_sub_epi16(v[ 9], v[11]);
        u[12] = _mm256_add_epi16(v[12], v[14]);
        u[14] = _mm256_sub_epi16(v[12], v[14]);
        u[13] = _mm256_add_epi16(v[13], v[15]);
        u[15] = _mm256_sub_epi16(v[13], v[15]);

        // stage 8
        v[ 0] = u[ 0];
        v[ 1] = u[ 1];
        btf_avx2(cospi32, u[2], u[3], &v[2], &v[3]);
        v[ 4] = u[ 4];
        v[ 5] = u[ 5];
        btf_avx2(cospi32, u[6], u[7], &v[6], &v[7]);
        v[ 8] = u[ 8];
        v[ 9] = u[ 9];
        btf_avx2(cospi32, u[10], u[11], &v[10], &v[11]);
        v[12] = u[12];
        v[13] = u[13];
        btf_avx2(cospi32, u[14], u[15], &v[14], &v[15]);

        const __m256i final_rounding = _mm256_set1_epi16((1 << final_shift) >> 1);

        // stage 9
        out[ 0] = _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[ 0]), final_shift);
        out[ 1] = _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[ 8]), final_shift); // change sign
        out[ 2] = _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[12]), final_shift);
        out[ 3] = _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[ 4]), final_shift);
        out[ 4] = _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[ 6]), final_shift);
        out[ 5] = _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[14]), final_shift);
        out[ 6] = _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[10]), final_shift);
        out[ 7] = _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[ 2]), final_shift);
        out[ 8] = _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[ 3]), final_shift);
        out[ 9] = _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[11]), final_shift);
        out[10] = _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[15]), final_shift);
        out[11] = _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[ 7]), final_shift);
        out[12] = _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[ 5]), final_shift);
        out[13] = _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[13]), final_shift);
        out[14] = _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[ 9]), final_shift);
        out[15] = _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[ 1]), final_shift);
    }

    void av1_inv_txfm2d_add_16x16_avx2(const int16_t *coeff, __m256i *out, int tx_type)
    {
        __m256i in[16];
        const int shift0 = -inv_txfm_shift_ls[TX_16X16][0];
        const int shift1 = -inv_txfm_shift_ls[TX_16X16][1];

        for (int i = 0; i < 16; ++i)
            out[i] = loada_si256(coeff + i * 16);

        transpose_16x16_16s(out, in, 1);

        ((tx_type == DCT_DCT || tx_type == ADST_DCT)
            ? idct16x16_avx2_16s
            : iadst16x16_avx2_16s)(in, out, shift0);

        transpose_16x16_16s(out, in, 1);

        ((tx_type == DCT_DCT || tx_type == DCT_ADST)
            ? idct16x16_avx2_16s
            : iadst16x16_avx2_16s)(in, out, shift1);
    }

    static void idct32_avx2_16s(__m256i *in, __m256i *out, int final_shift)
    {
        const short *cospi = cospi_arr_data;
        const __m256i cospi62  = _mm256_set1_epi16( cospi[62]);
        const __m256i cospi30  = _mm256_set1_epi16( cospi[30]);
        const __m256i cospi46  = _mm256_set1_epi16( cospi[46]);
        const __m256i cospi14  = _mm256_set1_epi16( cospi[14]);
        const __m256i cospi54  = _mm256_set1_epi16( cospi[54]);
        const __m256i cospi22  = _mm256_set1_epi16( cospi[22]);
        const __m256i cospi38  = _mm256_set1_epi16( cospi[38]);
        const __m256i cospi6   = _mm256_set1_epi16( cospi[6]);
        const __m256i cospi58  = _mm256_set1_epi16( cospi[58]);
        const __m256i cospi26  = _mm256_set1_epi16( cospi[26]);
        const __m256i cospi42  = _mm256_set1_epi16( cospi[42]);
        const __m256i cospi10  = _mm256_set1_epi16( cospi[10]);
        const __m256i cospi50  = _mm256_set1_epi16( cospi[50]);
        const __m256i cospi18  = _mm256_set1_epi16( cospi[18]);
        const __m256i cospi34  = _mm256_set1_epi16( cospi[34]);
        const __m256i cospi2   = _mm256_set1_epi16( cospi[2]);
        const __m256i cospi60  = _mm256_set1_epi16( cospi[60]);
        const __m256i cospi28  = _mm256_set1_epi16( cospi[28]);
        const __m256i cospi44  = _mm256_set1_epi16( cospi[44]);
        const __m256i cospi12  = _mm256_set1_epi16( cospi[12]);
        const __m256i cospi52  = _mm256_set1_epi16( cospi[52]);
        const __m256i cospi20  = _mm256_set1_epi16( cospi[20]);
        const __m256i cospi36  = _mm256_set1_epi16( cospi[36]);
        const __m256i cospi4   = _mm256_set1_epi16( cospi[4]);
        const __m256i cospi56  = _mm256_set1_epi16( cospi[56]);
        const __m256i cospim56 = _mm256_set1_epi16(-cospi[56]);
        const __m256i cospi24  = _mm256_set1_epi16( cospi[24]);
        const __m256i cospim24 = _mm256_set1_epi16(-cospi[24]);
        const __m256i cospi40  = _mm256_set1_epi16( cospi[40]);
        const __m256i cospi8   = _mm256_set1_epi16( cospi[8]);
        const __m256i cospim40 = _mm256_set1_epi16(-cospi[40]);
        const __m256i cospim8  = _mm256_set1_epi16(-cospi[8]);
        const __m256i cospi32  = _mm256_set1_epi16( cospi[32]);
        const __m256i cospi48  = _mm256_set1_epi16( cospi[48]);
        const __m256i cospim48 = _mm256_set1_epi16(-cospi[48]);
        const __m256i cospi16  = _mm256_set1_epi16( cospi[16]);
        const __m256i cospim16 = _mm256_set1_epi16(-cospi[16]);
        __m256i u[32], v[32];

        for (int col = 0; col < 2; ++col) {
            // stage 0
            // stage 1
            // stage 2
            btf_avx2(cospi2,  in[ 1 * 2 + col], cospi62, in[31 * 2 + col], &v[31], &v[16]);
            btf_avx2(cospi34, in[17 * 2 + col], cospi30, in[15 * 2 + col], &v[30], &v[17]);
            btf_avx2(cospi18, in[ 9 * 2 + col], cospi46, in[23 * 2 + col], &v[29], &v[18]);
            btf_avx2(cospi50, in[25 * 2 + col], cospi14, in[ 7 * 2 + col], &v[28], &v[19]);
            btf_avx2(cospi10, in[ 5 * 2 + col], cospi54, in[27 * 2 + col], &v[27], &v[20]);
            btf_avx2(cospi42, in[21 * 2 + col], cospi22, in[11 * 2 + col], &v[26], &v[21]);
            btf_avx2(cospi26, in[13 * 2 + col], cospi38, in[19 * 2 + col], &v[25], &v[22]);
            btf_avx2(cospi58, in[29 * 2 + col], cospi6,  in[ 3 * 2 + col], &v[24], &v[23]);

            // stage 3
            btf_avx2(cospi4,  in[ 2 * 2 + col], cospi60, in[30 * 2 + col], &u[15], &u[ 8]);
            btf_avx2(cospi36, in[18 * 2 + col], cospi28, in[14 * 2 + col], &u[14], &u[ 9]);
            btf_avx2(cospi20, in[10 * 2 + col], cospi44, in[22 * 2 + col], &u[13], &u[10]);
            btf_avx2(cospi52, in[26 * 2 + col], cospi12, in[ 6 * 2 + col], &u[12], &u[11]);
            u[16] = _mm256_add_epi16(v[16], v[17]);
            u[17] = _mm256_sub_epi16(v[16], v[17]);
            u[18] = _mm256_sub_epi16(v[19], v[18]);
            u[19] = _mm256_add_epi16(v[18], v[19]);
            u[20] = _mm256_add_epi16(v[20], v[21]);
            u[21] = _mm256_sub_epi16(v[20], v[21]);
            u[22] = _mm256_sub_epi16(v[23], v[22]);
            u[23] = _mm256_add_epi16(v[22], v[23]);
            u[24] = _mm256_add_epi16(v[24], v[25]);
            u[25] = _mm256_sub_epi16(v[24], v[25]);
            u[26] = _mm256_sub_epi16(v[27], v[26]);
            u[27] = _mm256_add_epi16(v[26], v[27]);
            u[28] = _mm256_add_epi16(v[28], v[29]);
            u[29] = _mm256_sub_epi16(v[28], v[29]);
            u[30] = _mm256_sub_epi16(v[31], v[30]);
            u[31] = _mm256_add_epi16(v[30], v[31]);

            // stage 4
            btf_avx2(cospi8,  in[ 4 * 2 + col], cospi56, in[28 * 2 + col], &v[ 7], &v[4]);
            btf_avx2(cospi40, in[20 * 2 + col], cospi24, in[12 * 2 + col], &v[ 6], &v[5]);
            v[ 8] = _mm256_add_epi16(u[8], u[9]);
            v[ 9] = _mm256_sub_epi16(u[8], u[9]);
            v[10] = _mm256_sub_epi16(u[11], u[10]);
            v[11] = _mm256_add_epi16(u[10], u[11]);
            v[12] = _mm256_add_epi16(u[12], u[13]);
            v[13] = _mm256_sub_epi16(u[12], u[13]);
            v[14] = _mm256_sub_epi16(u[15], u[14]);
            v[15] = _mm256_add_epi16(u[14], u[15]);
            v[16] = u[16];
            btf_avx2(cospim8,  u[17], cospi56, u[30], &v[17], &v[30]);
            btf_avx2(cospim56, u[18], cospim8, u[29], &v[18], &v[29]);
            v[19] = u[19];
            v[20] = u[20];
            btf_avx2(cospim40, u[21], cospi24, u[26], &v[21], &v[26]);
            btf_avx2(cospim24, u[22], cospim40, u[25], &v[22], &v[25]);
            v[23] = u[23];
            v[24] = u[24];
            v[27] = u[27];
            v[28] = u[28];
            v[31] = u[31];

            // stage 5
            btf_avx2(cospi32, in[ 0 * 2 + col],          in[16 * 2 + col], &u[ 0], &u[ 1]);
            btf_avx2(cospi16, in[ 8 * 2 + col], cospi48, in[24 * 2 + col], &u[ 3], &u[ 2]);
            u[ 4] = _mm256_add_epi16(v[4], v[5]);
            u[ 5] = _mm256_sub_epi16(v[4], v[5]);
            u[ 6] = _mm256_sub_epi16(v[7], v[6]);
            u[ 7] = _mm256_add_epi16(v[6], v[7]);
            u[ 8] = v[8];
            btf_avx2(cospim16, v[ 9], cospi48,  v[14], &u[ 9], &u[14]);
            btf_avx2(cospim48, v[10], cospim16, v[13], &u[10], &u[13]);
            u[11] = v[11];
            u[12] = v[12];
            u[15] = v[15];
            u[16] = _mm256_add_epi16(v[16], v[19]);
            u[17] = _mm256_add_epi16(v[17], v[18]);
            u[18] = _mm256_sub_epi16(v[17], v[18]);
            u[19] = _mm256_sub_epi16(v[16], v[19]);
            u[20] = _mm256_sub_epi16(v[23], v[20]);
            u[21] = _mm256_sub_epi16(v[22], v[21]);
            u[22] = _mm256_add_epi16(v[21], v[22]);
            u[23] = _mm256_add_epi16(v[20], v[23]);
            u[24] = _mm256_add_epi16(v[24], v[27]);
            u[25] = _mm256_add_epi16(v[25], v[26]);
            u[26] = _mm256_sub_epi16(v[25], v[26]);
            u[27] = _mm256_sub_epi16(v[24], v[27]);
            u[28] = _mm256_sub_epi16(v[31], v[28]);
            u[29] = _mm256_sub_epi16(v[30], v[29]);
            u[30] = _mm256_add_epi16(v[29], v[30]);
            u[31] = _mm256_add_epi16(v[28], v[31]);


            // stage 6
            v[ 0] = _mm256_add_epi16(u[0], u[3]);
            v[ 1] = _mm256_add_epi16(u[1], u[2]);
            v[ 2] = _mm256_sub_epi16(u[1], u[2]);
            v[ 3] = _mm256_sub_epi16(u[0], u[3]);
            v[ 4] = u[4];
            btf_avx2(cospi32, u[ 6],  u[ 5], &v[ 6], &v[ 5]);
            v[ 7] = u[7];
            v[ 8] = _mm256_add_epi16(u[8], u[11]);
            v[ 9] = _mm256_add_epi16(u[9], u[10]);
            v[10] = _mm256_sub_epi16(u[9], u[10]);
            v[11] = _mm256_sub_epi16(u[8], u[11]);
            v[12] = _mm256_sub_epi16(u[15], u[12]);
            v[13] = _mm256_sub_epi16(u[14], u[13]);
            v[14] = _mm256_add_epi16(u[13], u[14]);
            v[15] = _mm256_add_epi16(u[12], u[15]);
            v[16] = u[16];
            v[17] = u[17];
            btf_avx2(cospim16, u[18], cospi48,  u[29], &v[18], &v[29]);
            btf_avx2(cospim16, u[19], cospi48,  u[28], &v[19], &v[28]);
            btf_avx2(cospim48, u[20], cospim16, u[27], &v[20], &v[27]);
            btf_avx2(cospim48, u[21], cospim16, u[26], &v[21], &v[26]);
            v[22] = u[22];
            v[23] = u[23];
            v[24] = u[24];
            v[25] = u[25];
            v[30] = u[30];
            v[31] = u[31];

            // stage 7
            u[ 0] = _mm256_add_epi16(v[0], v[7]);
            u[ 1] = _mm256_add_epi16(v[1], v[6]);
            u[ 2] = _mm256_add_epi16(v[2], v[5]);
            u[ 3] = _mm256_add_epi16(v[3], v[4]);
            u[ 4] = _mm256_sub_epi16(v[3], v[4]);
            u[ 5] = _mm256_sub_epi16(v[2], v[5]);
            u[ 6] = _mm256_sub_epi16(v[1], v[6]);
            u[ 7] = _mm256_sub_epi16(v[0], v[7]);
            u[ 8] = v[8];
            u[ 9] = v[9];
            btf_avx2(cospi32, v[13], v[10], &u[13], &u[10]);
            btf_avx2(cospi32, v[12], v[11], &u[12], &u[11]);
            u[14] = v[14];
            u[15] = v[15];
            u[16] = _mm256_add_epi16(v[16], v[23]);
            u[17] = _mm256_add_epi16(v[17], v[22]);
            u[18] = _mm256_add_epi16(v[18], v[21]);
            u[19] = _mm256_add_epi16(v[19], v[20]);
            u[20] = _mm256_sub_epi16(v[19], v[20]);
            u[21] = _mm256_sub_epi16(v[18], v[21]);
            u[22] = _mm256_sub_epi16(v[17], v[22]);
            u[23] = _mm256_sub_epi16(v[16], v[23]);
            u[24] = _mm256_sub_epi16(v[31], v[24]);
            u[25] = _mm256_sub_epi16(v[30], v[25]);
            u[26] = _mm256_sub_epi16(v[29], v[26]);
            u[27] = _mm256_sub_epi16(v[28], v[27]);
            u[28] = _mm256_add_epi16(v[27], v[28]);
            u[29] = _mm256_add_epi16(v[26], v[29]);
            u[30] = _mm256_add_epi16(v[25], v[30]);
            u[31] = _mm256_add_epi16(v[24], v[31]);

            // stage 8
            v[ 0] = _mm256_add_epi16(u[0], u[15]);
            v[ 1] = _mm256_add_epi16(u[1], u[14]);
            v[ 2] = _mm256_add_epi16(u[2], u[13]);
            v[ 3] = _mm256_add_epi16(u[3], u[12]);
            v[ 4] = _mm256_add_epi16(u[4], u[11]);
            v[ 5] = _mm256_add_epi16(u[5], u[10]);
            v[ 6] = _mm256_add_epi16(u[6], u[9]);
            v[ 7] = _mm256_add_epi16(u[7], u[8]);
            v[ 8] = _mm256_sub_epi16(u[7], u[8]);
            v[ 9] = _mm256_sub_epi16(u[6], u[9]);
            v[10] = _mm256_sub_epi16(u[5], u[10]);
            v[11] = _mm256_sub_epi16(u[4], u[11]);
            v[12] = _mm256_sub_epi16(u[3], u[12]);
            v[13] = _mm256_sub_epi16(u[2], u[13]);
            v[14] = _mm256_sub_epi16(u[1], u[14]);
            v[15] = _mm256_sub_epi16(u[0], u[15]);
            v[16] = u[16];
            v[17] = u[17];
            v[18] = u[18];
            v[19] = u[19];
            btf_avx2(cospi32, u[27], u[20], &v[27], &v[20]);
            btf_avx2(cospi32, u[26], u[21], &v[26], &v[21]);
            btf_avx2(cospi32, u[25], u[22], &v[25], &v[22]);
            btf_avx2(cospi32, u[24], u[23], &v[24], &v[23]);
            v[28] = u[28];
            v[29] = u[29];
            v[30] = u[30];
            v[31] = u[31];


            const __m256i final_rounding = _mm256_set1_epi16((1 << final_shift) >> 1);
            v[ 0] = _mm256_add_epi16(v[ 0], final_rounding);
            v[ 1] = _mm256_add_epi16(v[ 1], final_rounding);
            v[ 2] = _mm256_add_epi16(v[ 2], final_rounding);
            v[ 3] = _mm256_add_epi16(v[ 3], final_rounding);
            v[ 4] = _mm256_add_epi16(v[ 4], final_rounding);
            v[ 5] = _mm256_add_epi16(v[ 5], final_rounding);
            v[ 6] = _mm256_add_epi16(v[ 6], final_rounding);
            v[ 7] = _mm256_add_epi16(v[ 7], final_rounding);
            v[ 8] = _mm256_add_epi16(v[ 8], final_rounding);
            v[ 9] = _mm256_add_epi16(v[ 9], final_rounding);
            v[10] = _mm256_add_epi16(v[10], final_rounding);
            v[11] = _mm256_add_epi16(v[11], final_rounding);
            v[12] = _mm256_add_epi16(v[12], final_rounding);
            v[13] = _mm256_add_epi16(v[13], final_rounding);
            v[14] = _mm256_add_epi16(v[14], final_rounding);
            v[15] = _mm256_add_epi16(v[15], final_rounding);

            // stage 9
            u[ 0] = _mm256_add_epi16(v[ 0], v[31]);
            u[ 1] = _mm256_add_epi16(v[ 1], v[30]);
            u[ 2] = _mm256_add_epi16(v[ 2], v[29]);
            u[ 3] = _mm256_add_epi16(v[ 3], v[28]);
            u[ 4] = _mm256_add_epi16(v[ 4], v[27]);
            u[ 5] = _mm256_add_epi16(v[ 5], v[26]);
            u[ 6] = _mm256_add_epi16(v[ 6], v[25]);
            u[ 7] = _mm256_add_epi16(v[ 7], v[24]);
            u[ 8] = _mm256_add_epi16(v[ 8], v[23]);
            u[ 9] = _mm256_add_epi16(v[ 9], v[22]);
            u[10] = _mm256_add_epi16(v[10], v[21]);
            u[11] = _mm256_add_epi16(v[11], v[20]);
            u[12] = _mm256_add_epi16(v[12], v[19]);
            u[13] = _mm256_add_epi16(v[13], v[18]);
            u[14] = _mm256_add_epi16(v[14], v[17]);
            u[15] = _mm256_add_epi16(v[15], v[16]);
            u[16] = _mm256_sub_epi16(v[15], v[16]);
            u[17] = _mm256_sub_epi16(v[14], v[17]);
            u[18] = _mm256_sub_epi16(v[13], v[18]);
            u[19] = _mm256_sub_epi16(v[12], v[19]);
            u[20] = _mm256_sub_epi16(v[11], v[20]);
            u[21] = _mm256_sub_epi16(v[10], v[21]);
            u[22] = _mm256_sub_epi16(v[ 9], v[22]);
            u[23] = _mm256_sub_epi16(v[ 8], v[23]);
            u[24] = _mm256_sub_epi16(v[ 7], v[24]);
            u[25] = _mm256_sub_epi16(v[ 6], v[25]);
            u[26] = _mm256_sub_epi16(v[ 5], v[26]);
            u[27] = _mm256_sub_epi16(v[ 4], v[27]);
            u[28] = _mm256_sub_epi16(v[ 3], v[28]);
            u[29] = _mm256_sub_epi16(v[ 2], v[29]);
            u[30] = _mm256_sub_epi16(v[ 1], v[30]);
            u[31] = _mm256_sub_epi16(v[ 0], v[31]);

            out[ 0 * 2 + col] = _mm256_srai_epi16(u[ 0], final_shift);
            out[ 1 * 2 + col] = _mm256_srai_epi16(u[ 1], final_shift);
            out[ 2 * 2 + col] = _mm256_srai_epi16(u[ 2], final_shift);
            out[ 3 * 2 + col] = _mm256_srai_epi16(u[ 3], final_shift);
            out[ 4 * 2 + col] = _mm256_srai_epi16(u[ 4], final_shift);
            out[ 5 * 2 + col] = _mm256_srai_epi16(u[ 5], final_shift);
            out[ 6 * 2 + col] = _mm256_srai_epi16(u[ 6], final_shift);
            out[ 7 * 2 + col] = _mm256_srai_epi16(u[ 7], final_shift);
            out[ 8 * 2 + col] = _mm256_srai_epi16(u[ 8], final_shift);
            out[ 9 * 2 + col] = _mm256_srai_epi16(u[ 9], final_shift);
            out[10 * 2 + col] = _mm256_srai_epi16(u[10], final_shift);
            out[11 * 2 + col] = _mm256_srai_epi16(u[11], final_shift);
            out[12 * 2 + col] = _mm256_srai_epi16(u[12], final_shift);
            out[13 * 2 + col] = _mm256_srai_epi16(u[13], final_shift);
            out[14 * 2 + col] = _mm256_srai_epi16(u[14], final_shift);
            out[15 * 2 + col] = _mm256_srai_epi16(u[15], final_shift);
            out[16 * 2 + col] = _mm256_srai_epi16(u[16], final_shift);
            out[17 * 2 + col] = _mm256_srai_epi16(u[17], final_shift);
            out[18 * 2 + col] = _mm256_srai_epi16(u[18], final_shift);
            out[19 * 2 + col] = _mm256_srai_epi16(u[19], final_shift);
            out[20 * 2 + col] = _mm256_srai_epi16(u[20], final_shift);
            out[21 * 2 + col] = _mm256_srai_epi16(u[21], final_shift);
            out[22 * 2 + col] = _mm256_srai_epi16(u[22], final_shift);
            out[23 * 2 + col] = _mm256_srai_epi16(u[23], final_shift);
            out[24 * 2 + col] = _mm256_srai_epi16(u[24], final_shift);
            out[25 * 2 + col] = _mm256_srai_epi16(u[25], final_shift);
            out[26 * 2 + col] = _mm256_srai_epi16(u[26], final_shift);
            out[27 * 2 + col] = _mm256_srai_epi16(u[27], final_shift);
            out[28 * 2 + col] = _mm256_srai_epi16(u[28], final_shift);
            out[29 * 2 + col] = _mm256_srai_epi16(u[29], final_shift);
            out[30 * 2 + col] = _mm256_srai_epi16(u[30], final_shift);
            out[31 * 2 + col] = _mm256_srai_epi16(u[31], final_shift);
        }
    }

    void av1_inv_txfm2d_add_32x32_avx2(const int16_t *coeff, __m256i *out)
    {
        const int shift0 = -inv_txfm_shift_ls[TX_32X32][0];
        const int shift1 = -inv_txfm_shift_ls[TX_32X32][1];

        __m256i in[64];

        for (int i = 0; i < 16; ++i)
            out[2*i] = loada_si256(coeff + i * 32);
        transpose_16x16_16s(out, in + 0, 2);

        for (int i = 0; i < 16; ++i)
            out[2*i] = loada_si256(coeff + i * 32 + 16);
        transpose_16x16_16s(out, in + 32, 2);

        for (int i = 0; i < 16; ++i)
            out[2*i] = loada_si256(coeff + i * 32 + 16 * 32);
        transpose_16x16_16s(out, in + 1, 2);

        for (int i = 0; i < 16; ++i)
            out[2*i] = loada_si256(coeff + i * 32 + 16 * 32 + 16);
        transpose_16x16_16s(out, in + 33, 2);

        idct32_avx2_16s(in, out, shift0);
        transpose_32x32_16s(out, in);
        idct32_avx2_16s(in, out, shift1);
    }

}; // namespace details

namespace AV1PP {

    template <int size, int type> void itransform_hbd_avx2(const int16_t *src, int16_t *dst, int pitchDst)
    {
        __m256i buf[64];
        if (size == 0) {
            details::av1_inv_txfm2d_add_4x4_sse4_1(src, (__m128i *)buf, type);

            storel_epi64(dst + 0 * pitchDst, ((__m128i *)buf)[0]);
            storeh_epi64(dst + 1 * pitchDst, ((__m128i *)buf)[0]);
            storel_epi64(dst + 2 * pitchDst, ((__m128i *)buf)[1]);
            storeh_epi64(dst + 3 * pitchDst, ((__m128i *)buf)[1]);

        } else if (size == 1) {
            details::av1_inv_txfm2d_add_8x8_sse4_1(src, (__m128i *)buf, type);

            for (int i = 0; i < 8; i++)
                storea_si128(dst + i * pitchDst, ((__m128i *)buf)[i]);

        } else if (size == 2) {
            details::av1_inv_txfm2d_add_16x16_avx2(src, buf, type);

            for (int i = 0; i < 16; i++)
                storea_si256(dst + i * pitchDst, buf[i]);

        } else { assert(size == 3);
            details::av1_inv_txfm2d_add_32x32_avx2(src, buf);

            for (int i = 0; i < 32; i++) {
                storea_si256(dst + i * pitchDst +  0, buf[2 * i + 0]);
                storea_si256(dst + i * pitchDst + 16, buf[2 * i + 1]);
            }
        }
    }

    template void itransform_hbd_avx2<0, 0>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<0, 1>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<0, 2>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<0, 3>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<1, 0>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<1, 1>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<1, 2>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<1, 3>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<2, 0>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<2, 1>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<2, 2>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<2, 3>(const int16_t*,int16_t*,int);
    template void itransform_hbd_avx2<3, 0>(const int16_t*,int16_t*,int);

    template <int size, int type> void itransform_add_hbd_avx2(const int16_t *src, uint8_t *dst, int pitchDst) {
        __m256i buf[64];
        if (size == 0) {
            details::av1_inv_txfm2d_add_4x4_sse4_1(src, (__m128i *)buf, type);
            details::write_buffer_4x4_16s_8u((__m128i *)buf, dst, pitchDst);
        } else if (size == 1) {
            details::av1_inv_txfm2d_add_8x8_sse4_1(src, (__m128i *)buf, type);
            details::write_buffer_8x8_16s_8u((__m128i *)buf, dst, pitchDst);
        } else if (size == 2) {
            details::av1_inv_txfm2d_add_16x16_avx2(src, buf, type);
            details::write_buffer_16x16_16s_8u(buf, dst, pitchDst);
        } else { assert(size == 3);
            details::av1_inv_txfm2d_add_32x32_avx2(src, buf);
            details::write_buffer_32x32_16s_8u(buf, dst, pitchDst);
        }
    }

    template void itransform_add_hbd_avx2<0, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<0, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<0, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<0, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<1, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<1, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<1, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<1, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<2, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<2, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<2, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<2, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_avx2<3, 0>(const int16_t*,uint8_t*,int);
}; // namespace AV1PP