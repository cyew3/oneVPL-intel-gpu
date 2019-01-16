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

#define mm256_set2_epi16(lo, hi) _mm256_set1_epi32((lo & 0xffff) | (hi << 16))
#define mm_set2_epi16(lo, hi) _mm_set1_epi32((lo & 0xffff) | (hi << 16))
#define pair_set_epi16(a, b) _mm256_set_epi16(b, a, b, a, b, a, b, a, b, a, b, a, b, a, b, a);
#define pair_set_epi32(a, b) _mm256_set_epi32(b, a, b, a, b, a, b, a)

static inline void btf_avx2(__m256i w0, __m256i w1, __m256i n0, __m256i n1, const int bit, __m256i *out0, __m256i *out1)
{
    const __m256i rounding = _mm256_set1_epi32((1 << bit) >> 1);
    __m256i lo, hi, lo0, hi0, lo1, hi1;

    lo = _mm256_unpacklo_epi16(n0, n1);
    hi = _mm256_unpackhi_epi16(n0, n1);
    lo0 = _mm256_madd_epi16(lo, w0);
    hi0 = _mm256_madd_epi16(hi, w0);
    lo1 = _mm256_madd_epi16(lo, w1);
    hi1 = _mm256_madd_epi16(hi, w1);
    lo0 = _mm256_add_epi32(lo0, rounding);
    hi0 = _mm256_add_epi32(hi0, rounding);
    lo1 = _mm256_add_epi32(lo1, rounding);
    hi1 = _mm256_add_epi32(hi1, rounding);
    lo0 = _mm256_srai_epi32(lo0, bit);
    hi0 = _mm256_srai_epi32(hi0, bit);
    lo1 = _mm256_srai_epi32(lo1, bit);
    hi1 = _mm256_srai_epi32(hi1, bit);

    *out0 = _mm256_packs_epi32(lo0, hi0);
    *out1 = _mm256_packs_epi32(lo1, hi1);
}

static inline __m128i btf16_sse4(__m128i w0, __m128i w1, __m128i n01, const int bit)
{
    const __m128i rounding = _mm_set1_epi32((1 << bit) >> 1);
    __m128i r0 = _mm_madd_epi16(n01, w0);
    __m128i r1 = _mm_madd_epi16(n01, w1);
    r0 = _mm_add_epi32(r0, rounding);
    r1 = _mm_add_epi32(r1, rounding);
    r0 = _mm_srai_epi32(r0, bit);
    r1 = _mm_srai_epi32(r1, bit);
    r0 = _mm_packs_epi32(r0, r1);
    return r0;
}

static inline __m256i btf16_avx2(__m256i w0, __m256i w1, __m256i n01, const int bit)
{
    const __m256i rounding = _mm256_set1_epi32((1 << bit) >> 1);
    __m256i r0 = _mm256_madd_epi16(n01, w0);
    __m256i r1 = _mm256_madd_epi16(n01, w1);
    r0 = _mm256_add_epi32(r0, rounding);
    r1 = _mm256_add_epi32(r1, rounding);
    r0 = _mm256_srai_epi32(r0, bit);
    r1 = _mm256_srai_epi32(r1, bit);
    r0 = _mm256_packs_epi32(r0, r1);
    r0 = _mm256_permute4x64_epi64(r0, PERM4x64(0,2,1,3));
    return r0;
}

inline void transpose_8x8_16s(const __m128i *in, __m128i *out)
{
    __m128i u[8], v[8];
    u[0] = _mm_unpacklo_epi16(in[0], in[1]); // A0 B0 A1 B1 A2 B2 A3 B3
    u[1] = _mm_unpacklo_epi16(in[2], in[3]); // C0 D0 C1 D1 C2 D2 C3 D3
    u[2] = _mm_unpacklo_epi16(in[4], in[5]); // E0 F0 E1 F1 E2 F2 E3 F3
    u[3] = _mm_unpacklo_epi16(in[6], in[7]); // G0 H0 G1 H1 G2 H2 G3 H3
    u[4] = _mm_unpackhi_epi16(in[0], in[1]); // A4 B4 A5 B5 A6 B6 A7 B7
    u[5] = _mm_unpackhi_epi16(in[2], in[3]); // C4 D4 C5 D5 C6 D6 C7 D7
    u[6] = _mm_unpackhi_epi16(in[4], in[5]); // E4 F4 E5 F5 E6 F6 E7 F7
    u[7] = _mm_unpackhi_epi16(in[6], in[7]); // G4 H4 G5 H5 G6 H6 G7 H7
    v[0] = _mm_unpacklo_epi32(u[0], u[1]);   // A0 B0 C0 D0 A1 B1 C1 D1
    v[1] = _mm_unpacklo_epi32(u[2], u[3]);   // E0 F0 G0 H0 E1 F1 G1 H1
    v[2] = _mm_unpackhi_epi32(u[0], u[1]);   // A2 B2 C2 D2 A3 B3 C3 D3
    v[3] = _mm_unpackhi_epi32(u[2], u[3]);   // E2 F2 G2 H2 E3 F3 G3 H3
    v[4] = _mm_unpacklo_epi32(u[4], u[5]);   // A4 B4 C4 D4 A5 B5 C5 D5
    v[5] = _mm_unpacklo_epi32(u[6], u[7]);   // E4 F4 G4 H4 E5 F5 G5 H5
    v[6] = _mm_unpackhi_epi32(u[4], u[5]);   // A6 B6 C6 D6 A7 B7 C7 D7
    v[7] = _mm_unpackhi_epi32(u[6], u[7]);   // E6 F6 G6 H6 E7 F7 G7 H7
    out[0] = _mm_unpacklo_epi64(v[0], v[1]); // A0 B0 C0 D0 E0 F0 G0 H0
    out[1] = _mm_unpackhi_epi64(v[0], v[1]); // A1 B1 C1 D1 E1 F1 G1 H1
    out[2] = _mm_unpacklo_epi64(v[2], v[3]); // A2 B2 C2 D2 E2 F2 G2 H2
    out[3] = _mm_unpackhi_epi64(v[2], v[3]); // A3 B3 C3 D3 E3 F3 G3 H3
    out[4] = _mm_unpacklo_epi64(v[4], v[5]); // A4 B4 C4 D4 E4 F4 G4 H4
    out[5] = _mm_unpackhi_epi64(v[4], v[5]); // A5 B5 C5 D5 E5 F5 G5 H5
    out[6] = _mm_unpacklo_epi64(v[6], v[7]); // A6 B6 C6 D6 E6 F6 G6 H6
    out[7] = _mm_unpackhi_epi64(v[6], v[7]); // A7 B7 C7 D7 E7 F7 G7 H7
}

static inline void transpose_16x16_16s(const __m256i *in, __m256i *out, int pitch)
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
