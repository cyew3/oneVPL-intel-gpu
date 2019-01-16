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

#pragma once
#include "assert.h"
#include "immintrin.h"
#include "mfx_av1_opts_intrin.h"

#define pair_set_epi16(a, b) _mm256_set_epi16(b, a, b, a, b, a, b, a, b, a, b, a, b, a, b, a);
#define pair_set_epi32(a, b) _mm256_set_epi32(b, a, b, a, b, a, b, a)

namespace details {
    static const int DCT_CONST_BITS = 14;
    static const int DCT_CONST_ROUNDING = 1 << (DCT_CONST_BITS - 1);

    static const short cospi_1_64  = 16364;
    static const short cospi_2_64  = 16305;
    static const short cospi_3_64  = 16207;
    static const short cospi_4_64  = 16069;
    static const short cospi_5_64  = 15893;
    static const short cospi_6_64  = 15679;
    static const short cospi_7_64  = 15426;
    static const short cospi_8_64  = 15137;
    static const short cospi_9_64  = 14811;
    static const short cospi_10_64 = 14449;
    static const short cospi_11_64 = 14053;
    static const short cospi_12_64 = 13623;
    static const short cospi_13_64 = 13160;
    static const short cospi_14_64 = 12665;
    static const short cospi_15_64 = 12140;
    static const short cospi_16_64 = 11585;
    static const short cospi_17_64 = 11003;
    static const short cospi_18_64 = 10394;
    static const short cospi_19_64 = 9760;
    static const short cospi_20_64 = 9102;
    static const short cospi_21_64 = 8423;
    static const short cospi_22_64 = 7723;
    static const short cospi_23_64 = 7005;
    static const short cospi_24_64 = 6270;
    static const short cospi_25_64 = 5520;
    static const short cospi_26_64 = 4756;
    static const short cospi_27_64 = 3981;
    static const short cospi_28_64 = 3196;
    static const short cospi_29_64 = 2404;
    static const short cospi_30_64 = 1606;
    static const short cospi_31_64 = 804;

    inline void array_transpose_16x16(__m256i *in, __m256i *out) {
        const __m256i tr0_0  = _mm256_unpacklo_epi16(in[ 0], in[1]);
        const __m256i tr0_1  = _mm256_unpacklo_epi16(in[ 2], in[3]);
        const __m256i tr0_2  = _mm256_unpacklo_epi16(in[ 4], in[5]);
        const __m256i tr0_3  = _mm256_unpacklo_epi16(in[ 6], in[7]);
        const __m256i tr0_4  = _mm256_unpacklo_epi16(in[ 8], in[9]);
        const __m256i tr0_5  = _mm256_unpacklo_epi16(in[10], in[11]);
        const __m256i tr0_6  = _mm256_unpacklo_epi16(in[12], in[13]);
        const __m256i tr0_7  = _mm256_unpacklo_epi16(in[14], in[15]);
        const __m256i tr0_8  = _mm256_unpackhi_epi16(in[ 0], in[1]);
        const __m256i tr0_9  = _mm256_unpackhi_epi16(in[ 2], in[3]);
        const __m256i tr0_10 = _mm256_unpackhi_epi16(in[ 4], in[5]);
        const __m256i tr0_11 = _mm256_unpackhi_epi16(in[ 6], in[7]);
        const __m256i tr0_12 = _mm256_unpackhi_epi16(in[ 8], in[9]);
        const __m256i tr0_13 = _mm256_unpackhi_epi16(in[10], in[11]);
        const __m256i tr0_14 = _mm256_unpackhi_epi16(in[12], in[13]);
        const __m256i tr0_15 = _mm256_unpackhi_epi16(in[14], in[15]);

        const __m256i tr1_0  = _mm256_unpacklo_epi32(tr0_0,  tr0_1);
        const __m256i tr1_1  = _mm256_unpacklo_epi32(tr0_2,  tr0_3);
        const __m256i tr1_2  = _mm256_unpacklo_epi32(tr0_4,  tr0_5);
        const __m256i tr1_3  = _mm256_unpacklo_epi32(tr0_6,  tr0_7);
        const __m256i tr1_4  = _mm256_unpacklo_epi32(tr0_8,  tr0_9);
        const __m256i tr1_5  = _mm256_unpacklo_epi32(tr0_10, tr0_11);
        const __m256i tr1_6  = _mm256_unpacklo_epi32(tr0_12, tr0_13);
        const __m256i tr1_7  = _mm256_unpacklo_epi32(tr0_14, tr0_15);
        const __m256i tr1_8  = _mm256_unpackhi_epi32(tr0_0,  tr0_1);
        const __m256i tr1_9  = _mm256_unpackhi_epi32(tr0_2,  tr0_3);
        const __m256i tr1_10 = _mm256_unpackhi_epi32(tr0_4,  tr0_5);
        const __m256i tr1_11 = _mm256_unpackhi_epi32(tr0_6,  tr0_7);
        const __m256i tr1_12 = _mm256_unpackhi_epi32(tr0_8,  tr0_9);
        const __m256i tr1_13 = _mm256_unpackhi_epi32(tr0_10, tr0_11);
        const __m256i tr1_14 = _mm256_unpackhi_epi32(tr0_12, tr0_13);
        const __m256i tr1_15 = _mm256_unpackhi_epi32(tr0_14, tr0_15);

        const __m256i tr2_0  = _mm256_unpacklo_epi64(tr1_0,  tr1_1);
        const __m256i tr2_1  = _mm256_unpacklo_epi64(tr1_2,  tr1_3);
        const __m256i tr2_2  = _mm256_unpacklo_epi64(tr1_4,  tr1_5);
        const __m256i tr2_3  = _mm256_unpacklo_epi64(tr1_6,  tr1_7);
        const __m256i tr2_4  = _mm256_unpacklo_epi64(tr1_8,  tr1_9);
        const __m256i tr2_5  = _mm256_unpacklo_epi64(tr1_10, tr1_11);
        const __m256i tr2_6  = _mm256_unpacklo_epi64(tr1_12, tr1_13);
        const __m256i tr2_7  = _mm256_unpacklo_epi64(tr1_14, tr1_15);
        const __m256i tr2_8  = _mm256_unpackhi_epi64(tr1_0,  tr1_1);
        const __m256i tr2_9  = _mm256_unpackhi_epi64(tr1_2,  tr1_3);
        const __m256i tr2_10 = _mm256_unpackhi_epi64(tr1_4,  tr1_5);
        const __m256i tr2_11 = _mm256_unpackhi_epi64(tr1_6,  tr1_7);
        const __m256i tr2_12 = _mm256_unpackhi_epi64(tr1_8,  tr1_9);
        const __m256i tr2_13 = _mm256_unpackhi_epi64(tr1_10, tr1_11);
        const __m256i tr2_14 = _mm256_unpackhi_epi64(tr1_12, tr1_13);
        const __m256i tr2_15 = _mm256_unpackhi_epi64(tr1_14, tr1_15);

        out[ 0] = _mm256_permute2x128_si256(tr2_0,  tr2_1,  PERM2x128(0, 2));
        out[ 8] = _mm256_permute2x128_si256(tr2_0,  tr2_1,  PERM2x128(1, 3));
        out[ 4] = _mm256_permute2x128_si256(tr2_2,  tr2_3,  PERM2x128(0, 2));
        out[12] = _mm256_permute2x128_si256(tr2_2,  tr2_3,  PERM2x128(1, 3));
        out[ 2] = _mm256_permute2x128_si256(tr2_4,  tr2_5,  PERM2x128(0, 2));
        out[10] = _mm256_permute2x128_si256(tr2_4,  tr2_5,  PERM2x128(1, 3));
        out[ 6] = _mm256_permute2x128_si256(tr2_6,  tr2_7,  PERM2x128(0, 2));
        out[14] = _mm256_permute2x128_si256(tr2_6,  tr2_7,  PERM2x128(1, 3));
        out[ 1] = _mm256_permute2x128_si256(tr2_8,  tr2_9,  PERM2x128(0, 2));
        out[ 9] = _mm256_permute2x128_si256(tr2_8,  tr2_9,  PERM2x128(1, 3));
        out[ 5] = _mm256_permute2x128_si256(tr2_10, tr2_11, PERM2x128(0, 2));
        out[13] = _mm256_permute2x128_si256(tr2_10, tr2_11, PERM2x128(1, 3));
        out[ 3] = _mm256_permute2x128_si256(tr2_12, tr2_13, PERM2x128(0, 2));
        out[11] = _mm256_permute2x128_si256(tr2_12, tr2_13, PERM2x128(1, 3));
        out[ 7] = _mm256_permute2x128_si256(tr2_14, tr2_15, PERM2x128(0, 2));
        out[15] = _mm256_permute2x128_si256(tr2_14, tr2_15, PERM2x128(1, 3));
    }
};  // namespace details