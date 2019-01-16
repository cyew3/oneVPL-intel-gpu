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
#include "string.h"
#include "immintrin.h"
#include "mfx_av1_opts_common.h"
#include "mfx_av1_opts_intrin.h"
#include "mfx_av1_transform_common_avx2.h"
#include "mfx_av1_fwd_transform_common.h"

using namespace AV1PP;

static void fdct4x4_sse4_16s(__m128i *in, int bit)
{
    const int *cospi = cospi_arr(bit);

    const __m128i k_p32_p32 = mm_set2_epi16(cospi[32],  cospi[32]);
    const __m128i k_p32_m32 = mm_set2_epi16(cospi[32], -cospi[32]);
    const __m128i k_p16_p48 = mm_set2_epi16(cospi[16],  cospi[48]);
    const __m128i k_p48_m16 = mm_set2_epi16(cospi[48], -cospi[16]);
    const __m128i rounding = _mm_set1_epi32((1 << bit) >> 1);
    __m128i u0, u1, u2, u3, v0, v1;

    in[1] = _mm_shuffle_epi32(in[1], SHUF32(2,3,0,1));
    v0 = _mm_add_epi16(in[0], in[1]); // row0 row1
    v1 = _mm_sub_epi16(in[0], in[1]); // row3 row2

    u0 = _mm_unpacklo_epi16(v0, _mm_srli_si128(v0, 8)); // row0 row1 row0 row1 row0 row1 row0 row1
    u2 = _mm_madd_epi16(u0, k_p32_m32);
    u0 = _mm_madd_epi16(u0, k_p32_p32);
    u0 = _mm_add_epi32(u0, rounding);
    u2 = _mm_add_epi32(u2, rounding);
    u0 = _mm_srai_epi32(u0, bit);
    u2 = _mm_srai_epi32(u2, bit);
    u0 = _mm_packs_epi32(u0, u2);   // row0 row2

    u1 = _mm_unpacklo_epi16(v1, _mm_srli_si128(v1, 8)); // row3 row2 row3 row2 row3 row2 row3 row2
    u3 = _mm_madd_epi16(u1, k_p48_m16);
    u1 = _mm_madd_epi16(u1, k_p16_p48);
    u1 = _mm_add_epi32(u1, rounding);
    u3 = _mm_add_epi32(u3, rounding);
    u1 = _mm_srai_epi32(u1, bit);
    u3 = _mm_srai_epi32(u3, bit);
    u1 = _mm_packs_epi32(u1, u3);   // row1 row3

    // transpose
    v0 = _mm_unpacklo_epi16(u0, u1);
    v1 = _mm_unpackhi_epi16(u0, u1);
    in[0] = _mm_unpacklo_epi32(v0, v1);
    in[1] = _mm_unpackhi_epi32(v0, v1);
}

static void fadst4x4_sse4_16s(__m128i *in, int bit)
{
    const int *sinpi = sinpi_arr(bit);
    const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
    const __m128i zero = _mm_setzero_si128();
    const __m128i sinpi_p1_p2 = mm_set2_epi16(sinpi[1],  sinpi[2]);
    const __m128i sinpi_p2_p2 = mm_set2_epi16(sinpi[2],  sinpi[2]);
    const __m128i sinpi_p3_p3 = mm_set2_epi16(sinpi[3],  sinpi[3]);
    const __m128i sinpi_p4_m1 = mm_set2_epi16(sinpi[4], -sinpi[1]);
    __m128i s0, s1, s2, s3;
    __m128i x0, x1, x2, x3;
    __m128i u0, u1, u2, u3;
    __m128i v0, v2, v3;

    v0 = _mm_unpacklo_epi16(in[0], _mm_srli_si128(in[0], 8)); // interleaved in0/in1
    v2 = _mm_unpacklo_epi16(in[1], zero);   // interleaved in2/zero
    v3 = _mm_unpackhi_epi16(in[1], zero);   // interleaved in3/zero
    u0 = _mm_madd_epi16(v0, sinpi_p1_p2);   // sinpi1 * in0 + sinpi2 * in1
    u1 = _mm_madd_epi16(v0, sinpi_p3_p3);   // sinpi3 * in0 + sinpi3 * in1
    u2 = _mm_madd_epi16(v0, sinpi_p4_m1);   // sinpi4 * in0 - sinpi1 * in1
    u3 = _mm_madd_epi16(v3, sinpi_p4_m1);   // sinpi4 * in3
    x0 = _mm_add_epi32(u0, u3);             // sinpi1 * in0 + sinpi2 * in1 + sinpi4 * in3
    u3 = _mm_madd_epi16(v3, sinpi_p3_p3);   // sinpi3 * in3
    x1 = _mm_sub_epi32(u1, u3);             // sinpi3 * in0 + sinpi3 * in1 - sinpi3 * in3
    u3 = _mm_madd_epi16(v3, sinpi_p2_p2);   // sinpi2 * in3
    x2 = _mm_add_epi32(u2, u3);             // sinpi4 * in0 - sinpi1 * in1 + sinpi2 * in3
    x3 = _mm_madd_epi16(v2, sinpi_p3_p3);   // sinpi3 * in2

    s0 = _mm_add_epi32(x0, x3);
    s1 = x1;
    s2 = _mm_sub_epi32(x2, x3);
    s3 = _mm_add_epi32(_mm_sub_epi32(x2, x0), x3);

    // pack
    x0 = _mm_add_epi32(s0, rnding);
    x1 = _mm_add_epi32(s1, rnding);
    x2 = _mm_add_epi32(s2, rnding);
    x3 = _mm_add_epi32(s3, rnding);
    x0 = _mm_srai_epi32(x0, bit);
    x1 = _mm_srai_epi32(x1, bit);
    x2 = _mm_srai_epi32(x2, bit);
    x3 = _mm_srai_epi32(x3, bit);
    x0 = _mm_packs_epi32(x0, x1);
    x1 = _mm_packs_epi32(x2, x3);

    // transpose
    u0 = _mm_unpacklo_epi64(x0, x1);
    u1 = _mm_unpackhi_epi64(x0, x1);
    x0 = _mm_unpacklo_epi16(u0, u1);
    x1 = _mm_unpackhi_epi16(u0, u1);
    in[0] = _mm_unpacklo_epi32(x0, x1);
    in[1] = _mm_unpackhi_epi32(x0, x1);
}

template <int rows> static void fadst4_8_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = 13;
    const int *sinpi = sinpi_arr(bit);
    const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
    const __m256i zero = _mm256_setzero_si256();
    const __m256i sinpi_p1_p2 = mm256_set2_epi16(sinpi[1],  sinpi[2]);
    const __m256i sinpi_p2_p2 = mm256_set2_epi16(sinpi[2],  sinpi[2]);
    const __m256i sinpi_p3_p3 = mm256_set2_epi16(sinpi[3],  sinpi[3]);
    const __m256i sinpi_p4_m1 = mm256_set2_epi16(sinpi[4], -sinpi[1]);

    __m256i a02, a13;

    if (rows) {
        const __m256i one = _mm256_set1_epi16(1);
        a02 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 0 * pitch, in + 2 * pitch), one), 1);
        a13 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 1 * pitch, in + 3 * pitch), one), 1);
    } else {
        a02 = _mm256_slli_epi16(loada2_m128i(in + 0 * pitch, in + 2 * pitch), 2);
        a13 = _mm256_slli_epi16(loada2_m128i(in + 1 * pitch, in + 3 * pitch), 2);
    }

    a02 = _mm256_permute4x64_epi64(a02, PERM4x64(0,2,1,3));
    a13 = _mm256_permute4x64_epi64(a13, PERM4x64(0,2,1,3));

    __m256i v01 = _mm256_unpacklo_epi16(a02, a13);  // interleaved rows 0/1
    __m256i v2  = _mm256_unpackhi_epi16(a02, zero); // interleaved row 2 & zeros
    __m256i v3  = _mm256_unpackhi_epi16(a13, zero); // interleaved row 2 & zeros

    __m256i u0 = _mm256_madd_epi16(v01, sinpi_p1_p2);   // sinpi1 * in0 + sinpi2 * in1
    __m256i u1 = _mm256_madd_epi16(v01, sinpi_p3_p3);   // sinpi3 * in0 + sinpi3 * in1
    __m256i u2 = _mm256_madd_epi16(v01, sinpi_p4_m1);   // sinpi4 * in0 - sinpi1 * in1
    __m256i u3 = _mm256_madd_epi16(v3,  sinpi_p4_m1);   // sinpi4 * in3
    __m256i u4 = _mm256_madd_epi16(v3,  sinpi_p3_p3);   // sinpi3 * in3
    __m256i u5 = _mm256_madd_epi16(v3,  sinpi_p2_p2);   // sinpi2 * in3
    __m256i x3 = _mm256_madd_epi16(v2,  sinpi_p3_p3);   // sinpi3 * in2
    __m256i x0 = _mm256_add_epi32(u0, u3);              // sinpi1 * in0 + sinpi2 * in1 + sinpi4 * in3
    __m256i x1 = _mm256_sub_epi32(u1, u4);              // sinpi3 * in0 + sinpi3 * in1 - sinpi3 * in3
    __m256i x2 = _mm256_add_epi32(u2, u5);              // sinpi4 * in0 - sinpi1 * in1 + sinpi2 * in3

    __m256i s0 = _mm256_add_epi32(x0, x3);
    __m256i s1 = x1;
    __m256i s2 = _mm256_sub_epi32(x2, x3);
    __m256i s3 = _mm256_add_epi32(_mm256_sub_epi32(x2, x0), x3);

    // pack
    x0 = _mm256_add_epi32(s0, rnding);
    x1 = _mm256_add_epi32(s1, rnding);
    x2 = _mm256_add_epi32(s2, rnding);
    x3 = _mm256_add_epi32(s3, rnding);
    x0 = _mm256_srai_epi32(x0, bit);
    x1 = _mm256_srai_epi32(x1, bit);
    x2 = _mm256_srai_epi32(x2, bit);
    x3 = _mm256_srai_epi32(x3, bit);
    x0 = _mm256_packs_epi32(x0, x2);    // A0 A1 A2 A3 C0 C1 C2 C3   A4 A5 A6 A7 C4 C5 C6 C7
    x1 = _mm256_packs_epi32(x1, x3);    // B0 B1 B2 B3 D0 D1 D2 D3   B4 B5 B6 B7 D4 D5 D6 D7

    // transpose
    __m256i *pout = (__m256i *)out;
    s0 = _mm256_unpacklo_epi16(x0, x1);
    s1 = _mm256_unpackhi_epi16(x0, x1);
    x0 = _mm256_unpacklo_epi32(s0, s1);
    x1 = _mm256_unpackhi_epi32(s0, s1);
    pout[0] = _mm256_permute2x128_si256(x0, x1, PERM2x128(0,2));
    pout[1] = _mm256_permute2x128_si256(x0, x1, PERM2x128(1,3));

    if (rows) {
        __m256i multiplier = _mm256_set1_epi16((NewSqrt2 - 4096) * 8);
        pout[0] = _mm256_add_epi16(pout[0], _mm256_mulhrs_epi16(pout[0], multiplier));
        pout[1] = _mm256_add_epi16(pout[1], _mm256_mulhrs_epi16(pout[1], multiplier));
    }
}

template <int rows> static void fdct8x8_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = 13;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_m32_p32 = mm256_set2_epi16(-cospi[32],  cospi[32]);
    const __m256i cospi_p48_p16 = mm256_set2_epi16( cospi[48],  cospi[16]);
    const __m256i cospi_m16_p48 = mm256_set2_epi16(-cospi[16],  cospi[48]);
    const __m256i cospi_p56_p08 = mm256_set2_epi16( cospi[56],  cospi[ 8]);
    const __m256i cospi_m08_p56 = mm256_set2_epi16(-cospi[ 8],  cospi[56]);
    const __m256i cospi_p24_p40 = mm256_set2_epi16( cospi[24],  cospi[40]);
    const __m256i cospi_m40_p24 = mm256_set2_epi16(-cospi[40],  cospi[24]);

    __m256i a01, a23, a76, a54;

    if (rows) {
        const __m256i one = _mm256_set1_epi16(1);
        a01 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 0 * pitch, in + 1 * pitch), one), 1);
        a23 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 2 * pitch, in + 3 * pitch), one), 1);
        a76 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 7 * pitch, in + 6 * pitch), one), 1);
        a54 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 5 * pitch, in + 4 * pitch), one), 1);
    } else {
        a01 = _mm256_slli_epi16(loada2_m128i(in + 0 * pitch, in + 1 * pitch), 2);
        a23 = _mm256_slli_epi16(loada2_m128i(in + 2 * pitch, in + 3 * pitch), 2);
        a76 = _mm256_slli_epi16(loada2_m128i(in + 7 * pitch, in + 6 * pitch), 2);
        a54 = _mm256_slli_epi16(loada2_m128i(in + 5 * pitch, in + 4 * pitch), 2);
    }

    // stage 0
    // stage 1
    __m256i b01 = _mm256_add_epi16(a01, a76);
    __m256i b23 = _mm256_add_epi16(a23, a54);
    __m256i b76 = _mm256_sub_epi16(a01, a76);
    __m256i b54 = _mm256_sub_epi16(a23, a54);
    __m256i b32 = _mm256_permute4x64_epi64(b23, PERM4x64(2,3,0,1));

    // stage 2
    __m256i c01 = _mm256_add_epi16(b01, b32);
    __m256i c32 = _mm256_sub_epi16(b01, b32);
    __m256i c47 = _mm256_permute2x128_si256(b76, b54, PERM2x128(3,0));

    __m256i c56 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(b54, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(b76, PERM4x64(2,2,3,3)));
    c56 = btf16_avx2(cospi_m32_p32, cospi_p32_p32, c56, bit);

    // stage 3
    __m256i e04 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(c01, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(c01, PERM4x64(2,2,3,3)));
    __m256i e26 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(c32, PERM4x64(2,2,3,3)),
                                        _mm256_permute4x64_epi64(c32, PERM4x64(0,0,1,1)));

    e04 = btf16_avx2(cospi_p32_p32, cospi_p32_m32, e04, bit);
    e26 = btf16_avx2(cospi_p48_p16, cospi_m16_p48, e26, bit);

    __m256i d47 = _mm256_add_epi16(c47, c56);
    __m256i d56 = _mm256_sub_epi16(c47, c56);

    // stage 4
    // stage 5
    __m256i e17 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d47, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(d47, PERM4x64(2,2,3,3)));
    __m256i e53 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d56, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(d56, PERM4x64(2,2,3,3)));

    e17 = btf16_avx2(cospi_p56_p08, cospi_m08_p56, e17, bit);
    e53 = btf16_avx2(cospi_p24_p40, cospi_m40_p24, e53, bit);

    __m256i e[4];
    e[0] = e04;
    e[1] = _mm256_permute2x128_si256(e53, e17, PERM2x128(2,0));
    e[2] = e26;
    e[3] = _mm256_permute2x128_si256(e53, e17, PERM2x128(1,3));

    __m256i f[4], *pout = (__m256i *)out;
    f[0] = _mm256_unpacklo_epi16(e[0], e[1]);
    f[1] = _mm256_unpacklo_epi16(e[2], e[3]);
    f[2] = _mm256_unpackhi_epi16(e[0], e[1]);
    f[3] = _mm256_unpackhi_epi16(e[2], e[3]);
    e[0] = _mm256_unpacklo_epi32(f[0], f[1]);
    e[1] = _mm256_unpackhi_epi32(f[0], f[1]);
    e[2] = _mm256_unpacklo_epi32(f[2], f[3]);
    e[3] = _mm256_unpackhi_epi32(f[2], f[3]);
    pout[0] = _mm256_permute4x64_epi64(e[0], PERM4x64(0,2,1,3));
    pout[1] = _mm256_permute4x64_epi64(e[1], PERM4x64(0,2,1,3));
    pout[2] = _mm256_permute4x64_epi64(e[2], PERM4x64(0,2,1,3));
    pout[3] = _mm256_permute4x64_epi64(e[3], PERM4x64(0,2,1,3));
}

template <int rows> static void fadst8x8_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = 13;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p16_p48 = mm256_set2_epi16( cospi[16],  cospi[48]);
    const __m256i cospi_p48_m16 = mm256_set2_epi16( cospi[48], -cospi[16]);
    const __m256i cospi_m48_p16 = mm256_set2_epi16(-cospi[48],  cospi[16]);
    const __m256i cospi_p04_p60 = mm256_set2_epi16( cospi[04],  cospi[60]);
    const __m256i cospi_p60_m04 = mm256_set2_epi16( cospi[60], -cospi[04]);
    const __m256i cospi_p20_p44 = mm256_set2_epi16( cospi[20],  cospi[44]);
    const __m256i cospi_p44_m20 = mm256_set2_epi16( cospi[44], -cospi[20]);
    const __m256i cospi_p36_p28 = mm256_set2_epi16( cospi[36],  cospi[28]);
    const __m256i cospi_p28_m36 = mm256_set2_epi16( cospi[28], -cospi[36]);
    const __m256i cospi_p52_p12 = mm256_set2_epi16( cospi[52],  cospi[12]);
    const __m256i cospi_p12_m52 = mm256_set2_epi16( cospi[12], -cospi[52]);
    const __m256i zero = _mm256_setzero_si256();

    __m256i b03, b56, a73, a15;

    if (rows) {
        const __m256i one = _mm256_set1_epi16(1);
        b03 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 0 * pitch, in + 4 * pitch), one), 1);
        a73 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 7 * pitch, in + 3 * pitch), one), 1);
        a15 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 1 * pitch, in + 5 * pitch), one), 1);
        b56 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 6 * pitch, in + 2 * pitch), one), 1);
    } else {
        b03 = _mm256_slli_epi16(loada2_m128i(in + 0 * pitch, in + 4 * pitch), 2);
        a73 = _mm256_slli_epi16(loada2_m128i(in + 7 * pitch, in + 3 * pitch), 2);
        a15 = _mm256_slli_epi16(loada2_m128i(in + 1 * pitch, in + 5 * pitch), 2);
        b56 = _mm256_slli_epi16(loada2_m128i(in + 6 * pitch, in + 2 * pitch), 2);
    }

    // stage 0
    // stage 1
    __m256i b12 = _mm256_sub_epi16(zero, a73);
    __m256i b47 = _mm256_sub_epi16(zero, a15);

    // stage 2
    __m256i c01 = _mm256_permute2x128_si256(b03, b12, PERM2x128(0,2));
    __m256i c45 = _mm256_permute2x128_si256(b47, b56, PERM2x128(0,2));
    __m256i c23 = _mm256_permute2x128_si256(_mm256_unpacklo_epi16(b12, b03),
                                            _mm256_unpackhi_epi16(b12, b03), PERM2x128(1,3));
    __m256i c67 = _mm256_permute2x128_si256(_mm256_unpacklo_epi16(b56, b47),
                                            _mm256_unpackhi_epi16(b56, b47), PERM2x128(1,3));
    c23 = btf16_avx2(cospi_p32_p32, cospi_p32_m32, c23, bit);
    c67 = btf16_avx2(cospi_p32_p32, cospi_p32_m32, c67, bit);

    // stage 3
    __m256i d01 = _mm256_add_epi16(c01, c23);
    __m256i d23 = _mm256_sub_epi16(c01, c23);
    __m256i d45 = _mm256_add_epi16(c45, c67);
    __m256i d67 = _mm256_sub_epi16(c45, c67);

    // stage 4
    __m256i e01 = d01;
    __m256i e23 = d23;
    __m256i e45 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d45, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(d45, PERM4x64(2,2,3,3)));
    __m256i e67 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d67, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(d67, PERM4x64(2,2,3,3)));
    e45 = btf16_avx2(cospi_p16_p48, cospi_p48_m16, e45, bit);
    e67 = btf16_avx2(cospi_m48_p16, cospi_p16_p48, e67, bit);

    // stage 5
    __m256i f01 = _mm256_add_epi16(e01, e45);
    __m256i f23 = _mm256_add_epi16(e23, e67);
    __m256i f45 = _mm256_sub_epi16(e01, e45);
    __m256i f67 = _mm256_sub_epi16(e23, e67);

    // stage 6
    __m256i g01 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f01, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(f01, PERM4x64(2,2,3,3)));
    __m256i g23 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f23, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(f23, PERM4x64(2,2,3,3)));
    __m256i g45 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f45, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(f45, PERM4x64(2,2,3,3)));
    __m256i g67 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f67, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(f67, PERM4x64(2,2,3,3)));
    __m256i h70 = btf16_avx2(cospi_p04_p60, cospi_p60_m04, g01, bit);
    __m256i h52 = btf16_avx2(cospi_p20_p44, cospi_p44_m20, g23, bit);
    __m256i h34 = btf16_avx2(cospi_p36_p28, cospi_p28_m36, g45, bit);
    __m256i h16 = btf16_avx2(cospi_p52_p12, cospi_p12_m52, g67, bit);

    __m256i h[4];
    h[0] = _mm256_permute2x128_si256(h70, h34, PERM2x128(1,3));
    h[1] = _mm256_permute2x128_si256(h16, h52, PERM2x128(0,2));
    h[2] = _mm256_permute2x128_si256(h52, h16, PERM2x128(1,3));
    h[3] = _mm256_permute2x128_si256(h34, h70, PERM2x128(0,2));

    __m256i i[4], *pout = (__m256i *)out;
    i[0] = _mm256_unpacklo_epi16(h[0], h[1]);
    i[1] = _mm256_unpacklo_epi16(h[2], h[3]);
    i[2] = _mm256_unpackhi_epi16(h[0], h[1]);
    i[3] = _mm256_unpackhi_epi16(h[2], h[3]);
    h[0] = _mm256_unpacklo_epi32(i[0], i[1]);
    h[1] = _mm256_unpackhi_epi32(i[0], i[1]);
    h[2] = _mm256_unpacklo_epi32(i[2], i[3]);
    h[3] = _mm256_unpackhi_epi32(i[2], i[3]);
    pout[0] = _mm256_permute4x64_epi64(h[0], PERM4x64(0,2,1,3));
    pout[1] = _mm256_permute4x64_epi64(h[1], PERM4x64(0,2,1,3));
    pout[2] = _mm256_permute4x64_epi64(h[2], PERM4x64(0,2,1,3));
    pout[3] = _mm256_permute4x64_epi64(h[3], PERM4x64(0,2,1,3));
}

template <int bit, int shift> static void fdct16x16_avx2_16s(const short *in, int pitch, short *out)
{
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_m32_p32 = mm256_set2_epi16(-cospi[32], cospi[32]);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32], cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32],-cospi[32]);
    const __m256i cospi_p48_p16 = mm256_set2_epi16( cospi[48], cospi[16]);
    const __m256i cospi_m16_p48 = mm256_set2_epi16(-cospi[16], cospi[48]);
    const __m256i cospi_m48_m16 = mm256_set2_epi16(-cospi[48],-cospi[16]);
    const __m256i cospi_p56_p08 = mm256_set2_epi16( cospi[56], cospi[ 8]);
    const __m256i cospi_m08_p56 = mm256_set2_epi16(-cospi[ 8], cospi[56]);
    const __m256i cospi_p24_p40 = mm256_set2_epi16( cospi[24], cospi[40]);
    const __m256i cospi_m40_p24 = mm256_set2_epi16(-cospi[40], cospi[24]);
    const __m256i cospi_p60_p04 = mm256_set2_epi16( cospi[60], cospi[ 4]);
    const __m256i cospi_m04_p60 = mm256_set2_epi16(-cospi[ 4], cospi[60]);
    const __m256i cospi_p28_p36 = mm256_set2_epi16( cospi[28], cospi[36]);
    const __m256i cospi_m36_p28 = mm256_set2_epi16(-cospi[36], cospi[28]);
    const __m256i cospi_p44_p20 = mm256_set2_epi16( cospi[44], cospi[20]);
    const __m256i cospi_m20_p44 = mm256_set2_epi16(-cospi[20], cospi[44]);
    const __m256i cospi_p12_p52 = mm256_set2_epi16( cospi[12], cospi[52]);
    const __m256i cospi_m52_p12 = mm256_set2_epi16(-cospi[52], cospi[12]);

    __m256i u[16], v[16];
    __m256i *pout = (__m256i *)out;

    if (shift < 0) {
        const int shift_ = shift < 0 ? -shift : shift; // to silent warning
        const __m256i rounding = _mm256_set1_epi16((1 << shift_) >> 1);
        v[ 0] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  0 * pitch), rounding), shift_);
        v[ 1] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  1 * pitch), rounding), shift_);
        v[ 2] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  2 * pitch), rounding), shift_);
        v[ 3] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  3 * pitch), rounding), shift_);
        v[ 4] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  4 * pitch), rounding), shift_);
        v[ 5] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  5 * pitch), rounding), shift_);
        v[ 6] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  6 * pitch), rounding), shift_);
        v[ 7] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  7 * pitch), rounding), shift_);
        v[ 8] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  8 * pitch), rounding), shift_);
        v[ 9] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  9 * pitch), rounding), shift_);
        v[10] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 10 * pitch), rounding), shift_);
        v[11] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 11 * pitch), rounding), shift_);
        v[12] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 12 * pitch), rounding), shift_);
        v[13] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 13 * pitch), rounding), shift_);
        v[14] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 14 * pitch), rounding), shift_);
        v[15] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 15 * pitch), rounding), shift_);
    } else {
        v[ 0] = _mm256_slli_epi16(loada_si256(in +  0 * pitch), shift);
        v[ 1] = _mm256_slli_epi16(loada_si256(in +  1 * pitch), shift);
        v[ 2] = _mm256_slli_epi16(loada_si256(in +  2 * pitch), shift);
        v[ 3] = _mm256_slli_epi16(loada_si256(in +  3 * pitch), shift);
        v[ 4] = _mm256_slli_epi16(loada_si256(in +  4 * pitch), shift);
        v[ 5] = _mm256_slli_epi16(loada_si256(in +  5 * pitch), shift);
        v[ 6] = _mm256_slli_epi16(loada_si256(in +  6 * pitch), shift);
        v[ 7] = _mm256_slli_epi16(loada_si256(in +  7 * pitch), shift);
        v[ 8] = _mm256_slli_epi16(loada_si256(in +  8 * pitch), shift);
        v[ 9] = _mm256_slli_epi16(loada_si256(in +  9 * pitch), shift);
        v[10] = _mm256_slli_epi16(loada_si256(in + 10 * pitch), shift);
        v[11] = _mm256_slli_epi16(loada_si256(in + 11 * pitch), shift);
        v[12] = _mm256_slli_epi16(loada_si256(in + 12 * pitch), shift);
        v[13] = _mm256_slli_epi16(loada_si256(in + 13 * pitch), shift);
        v[14] = _mm256_slli_epi16(loada_si256(in + 14 * pitch), shift);
        v[15] = _mm256_slli_epi16(loada_si256(in + 15 * pitch), shift);
    }

    // stage 0
    // stage 1
    // first 8 rows
    u[ 0] = _mm256_add_epi16(v[0], v[15]);
    u[ 1] = _mm256_add_epi16(v[1], v[14]);
    u[ 2] = _mm256_add_epi16(v[2], v[13]);
    u[ 3] = _mm256_add_epi16(v[3], v[12]);
    u[ 4] = _mm256_add_epi16(v[4], v[11]);
    u[ 5] = _mm256_add_epi16(v[5], v[10]);
    u[ 6] = _mm256_add_epi16(v[6], v[ 9]);
    u[ 7] = _mm256_add_epi16(v[7], v[ 8]);
    // stage 0
    // stage 1
    // other 8 rows
    u[ 8] = _mm256_sub_epi16(v[7], v[ 8]);
    u[ 9] = _mm256_sub_epi16(v[6], v[ 9]);
    u[10] = _mm256_sub_epi16(v[5], v[10]);
    u[11] = _mm256_sub_epi16(v[4], v[11]);
    u[12] = _mm256_sub_epi16(v[3], v[12]);
    u[13] = _mm256_sub_epi16(v[2], v[13]);
    u[14] = _mm256_sub_epi16(v[1], v[14]);
    u[15] = _mm256_sub_epi16(v[0], v[15]);

    // stage 2
    v[ 0] = _mm256_add_epi16(u[0], u[7]);
    v[ 1] = _mm256_add_epi16(u[1], u[6]);
    v[ 2] = _mm256_add_epi16(u[2], u[5]);
    v[ 3] = _mm256_add_epi16(u[3], u[4]);
    v[ 4] = _mm256_sub_epi16(u[3], u[4]);
    v[ 5] = _mm256_sub_epi16(u[2], u[5]);
    v[ 6] = _mm256_sub_epi16(u[1], u[6]);
    v[ 7] = _mm256_sub_epi16(u[0], u[7]);
    // stage 3
    u[0] = _mm256_add_epi16(v[0], v[3]);
    u[1] = _mm256_add_epi16(v[1], v[2]);
    u[2] = _mm256_sub_epi16(v[1], v[2]);
    u[3] = _mm256_sub_epi16(v[0], v[3]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, v[5], v[6], bit, &u[5], &u[6]);
    // stage 4
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[0], u[1], bit, &pout[ 0], &pout[ 8]);
    btf_avx2(cospi_p48_p16, cospi_m16_p48, u[2], u[3], bit, &pout[ 4], &pout[12]);
    v[5] = _mm256_sub_epi16(v[4], u[5]);
    v[4] = _mm256_add_epi16(v[4], u[5]);
    v[6] = _mm256_sub_epi16(v[7], u[6]);
    v[7] = _mm256_add_epi16(v[7], u[6]);
    // stage 5
    btf_avx2(cospi_p56_p08, cospi_m08_p56, v[4], v[7], bit, &pout[ 2], &pout[14]);
    btf_avx2(cospi_p24_p40, cospi_m40_p24, v[5], v[6], bit, &pout[10], &pout[ 6]);

    // stage 2
    btf_avx2(cospi_m32_p32, cospi_p32_p32, u[10], u[13], bit, &u[10], &u[13]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, u[11], u[12], bit, &u[11], &u[12]);
    // stage 3
    v[ 8] = _mm256_add_epi16(u[ 8], u[11]);
    v[ 9] = _mm256_add_epi16(u[ 9], u[10]);
    v[10] = _mm256_sub_epi16(u[ 9], u[10]);
    v[11] = _mm256_sub_epi16(u[ 8], u[11]);
    v[12] = _mm256_sub_epi16(u[15], u[12]);
    v[15] = _mm256_add_epi16(u[15], u[12]);
    v[13] = _mm256_sub_epi16(u[14], u[13]);
    v[14] = _mm256_add_epi16(u[14], u[13]);
    // stage 4
    btf_avx2(cospi_m16_p48, cospi_p48_p16, v[ 9], v[14], bit, &v[ 9], &v[14]);
    btf_avx2(cospi_m48_m16, cospi_m16_p48, v[10], v[13], bit, &v[10], &v[13]);
    // stage 5
    u[ 8] = _mm256_add_epi16(v[ 8], v[ 9]);
    u[ 9] = _mm256_sub_epi16(v[ 8], v[ 9]);
    u[10] = _mm256_sub_epi16(v[11], v[10]);
    u[11] = _mm256_add_epi16(v[11], v[10]);
    u[12] = _mm256_add_epi16(v[12], v[13]);
    u[13] = _mm256_sub_epi16(v[12], v[13]);
    u[14] = _mm256_sub_epi16(v[15], v[14]);
    u[15] = _mm256_add_epi16(v[15], v[14]);
    // stage 6
    btf_avx2(cospi_p60_p04, cospi_m04_p60, u[ 8], u[15], bit, &pout[ 1], &pout[15]);
    btf_avx2(cospi_p28_p36, cospi_m36_p28, u[ 9], u[14], bit, &pout[ 9], &pout[ 7]);
    btf_avx2(cospi_p44_p20, cospi_m20_p44, u[10], u[13], bit, &pout[ 5], &pout[11]);
    btf_avx2(cospi_p12_p52, cospi_m52_p12, u[11], u[12], bit, &pout[13], &pout[ 3]);

    transpose_16x16_16s(pout, pout, 1);
}

template <int rows> static void fadst16x16_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = rows ? 12 : 13;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p16_p48 = mm256_set2_epi16( cospi[16],  cospi[48]);
    const __m256i cospi_p48_m16 = mm256_set2_epi16( cospi[48], -cospi[16]);
    const __m256i cospi_m48_p16 = mm256_set2_epi16(-cospi[48],  cospi[16]);
    const __m256i cospi_p08_p56 = mm256_set2_epi16( cospi[ 8],  cospi[56]);
    const __m256i cospi_p56_m08 = mm256_set2_epi16( cospi[56], -cospi[ 8]);
    const __m256i cospi_p40_p24 = mm256_set2_epi16( cospi[40],  cospi[24]);
    const __m256i cospi_p24_m40 = mm256_set2_epi16( cospi[24], -cospi[40]);
    const __m256i cospi_m56_p08 = mm256_set2_epi16(-cospi[56],  cospi[ 8]);
    const __m256i cospi_m24_p40 = mm256_set2_epi16(-cospi[24],  cospi[40]);
    const __m256i cospi_p02_p62 = mm256_set2_epi16( cospi[ 2],  cospi[62]);
    const __m256i cospi_p62_m02 = mm256_set2_epi16( cospi[62], -cospi[ 2]);
    const __m256i cospi_p10_p54 = mm256_set2_epi16( cospi[10],  cospi[54]);
    const __m256i cospi_p54_m10 = mm256_set2_epi16( cospi[54], -cospi[10]);
    const __m256i cospi_p18_p46 = mm256_set2_epi16( cospi[18],  cospi[46]);
    const __m256i cospi_p46_m18 = mm256_set2_epi16( cospi[46], -cospi[18]);
    const __m256i cospi_p26_p38 = mm256_set2_epi16( cospi[26],  cospi[38]);
    const __m256i cospi_p38_m26 = mm256_set2_epi16( cospi[38], -cospi[26]);
    const __m256i cospi_p34_p30 = mm256_set2_epi16( cospi[34],  cospi[30]);
    const __m256i cospi_p30_m34 = mm256_set2_epi16( cospi[30], -cospi[34]);
    const __m256i cospi_p42_p22 = mm256_set2_epi16( cospi[42],  cospi[22]);
    const __m256i cospi_p22_m42 = mm256_set2_epi16( cospi[22], -cospi[42]);
    const __m256i cospi_p50_p14 = mm256_set2_epi16( cospi[50],  cospi[14]);
    const __m256i cospi_p14_m50 = mm256_set2_epi16( cospi[14], -cospi[50]);
    const __m256i cospi_p58_p06 = mm256_set2_epi16( cospi[58],  cospi[ 6]);
    const __m256i cospi_p06_m58 = mm256_set2_epi16( cospi[ 6], -cospi[58]);
    const __m256i zero = _mm256_setzero_si256();

    __m256i u[16], v[16];
    __m256i *pout = (__m256i *)out;

    if (rows) {
        v[ 0] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  0 * 16), _mm256_set1_epi16(2)), 2);
        v[ 1] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  1 * 16), _mm256_set1_epi16(2)), 2);
        v[ 2] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  2 * 16), _mm256_set1_epi16(2)), 2);
        v[ 3] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  3 * 16), _mm256_set1_epi16(2)), 2);
        v[ 4] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  4 * 16), _mm256_set1_epi16(2)), 2);
        v[ 5] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  5 * 16), _mm256_set1_epi16(2)), 2);
        v[ 6] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  6 * 16), _mm256_set1_epi16(2)), 2);
        v[ 7] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  7 * 16), _mm256_set1_epi16(2)), 2);
        v[ 8] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  8 * 16), _mm256_set1_epi16(2)), 2);
        v[ 9] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in +  9 * 16), _mm256_set1_epi16(2)), 2);
        v[10] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 10 * 16), _mm256_set1_epi16(2)), 2);
        v[11] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 11 * 16), _mm256_set1_epi16(2)), 2);
        v[12] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 12 * 16), _mm256_set1_epi16(2)), 2);
        v[13] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 13 * 16), _mm256_set1_epi16(2)), 2);
        v[14] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 14 * 16), _mm256_set1_epi16(2)), 2);
        v[15] = _mm256_srai_epi16(_mm256_add_epi16(loada_si256(in + 15 * 16), _mm256_set1_epi16(2)), 2);
    } else {
        v[ 0] = _mm256_slli_epi16(loada_si256(in +  0 * pitch), 2);
        v[ 1] = _mm256_slli_epi16(loada_si256(in +  1 * pitch), 2);
        v[ 2] = _mm256_slli_epi16(loada_si256(in +  2 * pitch), 2);
        v[ 3] = _mm256_slli_epi16(loada_si256(in +  3 * pitch), 2);
        v[ 4] = _mm256_slli_epi16(loada_si256(in +  4 * pitch), 2);
        v[ 5] = _mm256_slli_epi16(loada_si256(in +  5 * pitch), 2);
        v[ 6] = _mm256_slli_epi16(loada_si256(in +  6 * pitch), 2);
        v[ 7] = _mm256_slli_epi16(loada_si256(in +  7 * pitch), 2);
        v[ 8] = _mm256_slli_epi16(loada_si256(in +  8 * pitch), 2);
        v[ 9] = _mm256_slli_epi16(loada_si256(in +  9 * pitch), 2);
        v[10] = _mm256_slli_epi16(loada_si256(in + 10 * pitch), 2);
        v[11] = _mm256_slli_epi16(loada_si256(in + 11 * pitch), 2);
        v[12] = _mm256_slli_epi16(loada_si256(in + 12 * pitch), 2);
        v[13] = _mm256_slli_epi16(loada_si256(in + 13 * pitch), 2);
        v[14] = _mm256_slli_epi16(loada_si256(in + 14 * pitch), 2);
        v[15] = _mm256_slli_epi16(loada_si256(in + 15 * pitch), 2);
    }

    // stage 0
    // stage 1
    u[ 0] = v[0];
    u[ 1] = _mm256_sub_epi16(zero, v[15]);
    u[ 2] = _mm256_sub_epi16(zero, v[7]);
    u[ 3] = v[8];
    u[ 4] = _mm256_sub_epi16(zero, v[3]);
    u[ 5] = v[12];
    u[ 6] = v[4];
    u[ 7] = _mm256_sub_epi16(zero, v[11]);
    u[ 8] = _mm256_sub_epi16(zero, v[1]);
    u[ 9] = v[14];
    u[10] = v[6];
    u[11] = _mm256_sub_epi16(zero, v[9]);
    u[12] = v[2];
    u[13] = _mm256_sub_epi16(zero, v[13]);
    u[14] = _mm256_sub_epi16(zero, v[5]);
    u[15] = v[10];

    // stage 2
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[ 2], u[ 3], bit, &v[ 2], &v[ 3]);
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[ 6], u[ 7], bit, &v[ 6], &v[ 7]);
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[10], u[11], bit, &v[10], &v[11]);
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[14], u[15], bit, &v[14], &v[15]);

    // stage 3
    u[ 2] = _mm256_sub_epi16(u[ 0], v[ 2]);
    u[ 3] = _mm256_sub_epi16(u[ 1], v[ 3]);
    u[ 0] = _mm256_add_epi16(u[ 0], v[ 2]);
    u[ 1] = _mm256_add_epi16(u[ 1], v[ 3]);
    u[ 6] = _mm256_sub_epi16(u[ 4], v[ 6]);
    u[ 7] = _mm256_sub_epi16(u[ 5], v[ 7]);
    u[ 4] = _mm256_add_epi16(u[ 4], v[ 6]);
    u[ 5] = _mm256_add_epi16(u[ 5], v[ 7]);
    u[10] = _mm256_sub_epi16(u[ 8], v[10]);
    u[11] = _mm256_sub_epi16(u[ 9], v[11]);
    u[ 8] = _mm256_add_epi16(u[ 8], v[10]);
    u[ 9] = _mm256_add_epi16(u[ 9], v[11]);
    u[14] = _mm256_sub_epi16(u[12], v[14]);
    u[15] = _mm256_sub_epi16(u[13], v[15]);
    u[12] = _mm256_add_epi16(u[12], v[14]);
    u[13] = _mm256_add_epi16(u[13], v[15]);

    // stage 4
    btf_avx2(cospi_p16_p48, cospi_p48_m16, u[ 4], u[ 5], bit, &v[ 4], &v[ 5]);
    btf_avx2(cospi_m48_p16, cospi_p16_p48, u[ 6], u[ 7], bit, &v[ 6], &v[ 7]);
    btf_avx2(cospi_p16_p48, cospi_p48_m16, u[12], u[13], bit, &v[12], &v[13]);
    btf_avx2(cospi_m48_p16, cospi_p16_p48, u[14], u[15], bit, &v[14], &v[15]);

    // stage 5
    u[ 4] = _mm256_sub_epi16(u[ 0], v[ 4]);
    u[ 5] = _mm256_sub_epi16(u[ 1], v[ 5]);
    u[ 6] = _mm256_sub_epi16(u[ 2], v[ 6]);
    u[ 7] = _mm256_sub_epi16(u[ 3], v[ 7]);
    u[ 0] = _mm256_add_epi16(u[ 0], v[ 4]);
    u[ 1] = _mm256_add_epi16(u[ 1], v[ 5]);
    u[ 2] = _mm256_add_epi16(u[ 2], v[ 6]);
    u[ 3] = _mm256_add_epi16(u[ 3], v[ 7]);
    u[12] = _mm256_sub_epi16(u[ 8], v[12]);
    u[13] = _mm256_sub_epi16(u[ 9], v[13]);
    u[14] = _mm256_sub_epi16(u[10], v[14]);
    u[15] = _mm256_sub_epi16(u[11], v[15]);
    u[ 8] = _mm256_add_epi16(u[ 8], v[12]);
    u[ 9] = _mm256_add_epi16(u[ 9], v[13]);
    u[10] = _mm256_add_epi16(u[10], v[14]);
    u[11] = _mm256_add_epi16(u[11], v[15]);

    // stage 6
    btf_avx2(cospi_p08_p56, cospi_p56_m08, u[ 8], u[ 9], bit, &v[ 8], &v[ 9]);
    btf_avx2(cospi_p40_p24, cospi_p24_m40, u[10], u[11], bit, &v[10], &v[11]);
    btf_avx2(cospi_m56_p08, cospi_p08_p56, u[12], u[13], bit, &v[12], &v[13]);
    btf_avx2(cospi_m24_p40, cospi_p40_p24, u[14], u[15], bit, &v[14], &v[15]);

    // stage 7
    u[ 8] = _mm256_sub_epi16(u[0], v[ 8]);
    u[ 9] = _mm256_sub_epi16(u[1], v[ 9]);
    u[10] = _mm256_sub_epi16(u[2], v[10]);
    u[11] = _mm256_sub_epi16(u[3], v[11]);
    u[12] = _mm256_sub_epi16(u[4], v[12]);
    u[13] = _mm256_sub_epi16(u[5], v[13]);
    u[14] = _mm256_sub_epi16(u[6], v[14]);
    u[15] = _mm256_sub_epi16(u[7], v[15]);
    u[ 0] = _mm256_add_epi16(u[0], v[ 8]);
    u[ 1] = _mm256_add_epi16(u[1], v[ 9]);
    u[ 2] = _mm256_add_epi16(u[2], v[10]);
    u[ 3] = _mm256_add_epi16(u[3], v[11]);
    u[ 4] = _mm256_add_epi16(u[4], v[12]);
    u[ 5] = _mm256_add_epi16(u[5], v[13]);
    u[ 6] = _mm256_add_epi16(u[6], v[14]);
    u[ 7] = _mm256_add_epi16(u[7], v[15]);

    // stage 8
    btf_avx2(cospi_p02_p62, cospi_p62_m02, u[ 0], u[ 1], bit, &pout[15], &pout[ 0]);
    btf_avx2(cospi_p10_p54, cospi_p54_m10, u[ 2], u[ 3], bit, &pout[13], &pout[ 2]);
    btf_avx2(cospi_p18_p46, cospi_p46_m18, u[ 4], u[ 5], bit, &pout[11], &pout[ 4]);
    btf_avx2(cospi_p26_p38, cospi_p38_m26, u[ 6], u[ 7], bit, &pout[ 9], &pout[ 6]);
    btf_avx2(cospi_p34_p30, cospi_p30_m34, u[ 8], u[ 9], bit, &pout[ 7], &pout[ 8]);
    btf_avx2(cospi_p42_p22, cospi_p22_m42, u[10], u[11], bit, &pout[ 5], &pout[10]);
    btf_avx2(cospi_p50_p14, cospi_p14_m50, u[12], u[13], bit, &pout[ 3], &pout[12]);
    btf_avx2(cospi_p58_p06, cospi_p06_m58, u[14], u[15], bit, &pout[ 1], &pout[14]);

    transpose_16x16_16s(pout, pout, 1);
}

static void ftransform_4x4_sse4(const short *input, short *coeff, int input_stride, TxType tx_type)
{
    const int shift0 = 2;
    const int bit = 13;

    assert((size_t(input) & 7) == 0);
    assert((size_t(coeff) & 15) == 0);
    assert(fwd_txfm_shift_ls[TX_4X4][0] == shift0);
    assert(fwd_txfm_shift_ls[TX_4X4][1] == 0);
    assert(fwd_txfm_shift_ls[TX_4X4][2] == 0);
    assert(fwd_cos_bit_col[TX_4X4][TX_4X4] == bit);
    assert(fwd_cos_bit_row[TX_4X4][TX_4X4] == bit);

    __m128i *in = (__m128i *)coeff;
    in[0] = loadu2_epi64(input + 0 * input_stride, input + 1 * input_stride);
    in[1] = loadu2_epi64(input + 2 * input_stride, input + 3 * input_stride);
    in[0] = _mm_slli_epi16(in[0], shift0);
    in[1] = _mm_slli_epi16(in[1], shift0);

    if (tx_type == DCT_DCT) {
        fdct4x4_sse4_16s(in, bit);
        fdct4x4_sse4_16s(in, bit);
    } else if (tx_type == ADST_DCT) {
        fadst4x4_sse4_16s(in, bit);
        fdct4x4_sse4_16s(in, bit);
    } else if (tx_type == DCT_ADST) {
        fdct4x4_sse4_16s(in, bit);
        fadst4x4_sse4_16s(in, bit);
    } else { assert(tx_type == ADST_ADST);
        fadst4x4_sse4_16s(in, bit);
        fadst4x4_sse4_16s(in, bit);
    }
}

static void ftransform_8x8_avx2(const short *input, short *coeff, int stride, TxType tx_type)
{
    assert((size_t(input) & 15) == 0);
    assert((size_t(coeff) & 31) == 0);
    assert(fwd_txfm_shift_ls[TX_8X8][0] == 2);
    assert(fwd_txfm_shift_ls[TX_8X8][1] == -1);
    assert(fwd_txfm_shift_ls[TX_8X8][2] == 0);
    assert(fwd_cos_bit_col[TX_8X8][TX_8X8] == 13);
    assert(fwd_cos_bit_row[TX_8X8][TX_8X8] == 13);

    alignas(32) short intermediate[8 * 8];

    if (tx_type == DCT_DCT) {
        fdct8x8_avx2_16s<0>(input, stride, intermediate);
        fdct8x8_avx2_16s<1>(intermediate, 8, coeff);
    } else if (tx_type == ADST_DCT) {
        fadst8x8_avx2_16s<0>(input, stride, intermediate);
        fdct8x8_avx2_16s <1>(intermediate, 8, coeff);
    } else if (tx_type == DCT_ADST) {
        fdct8x8_avx2_16s <0>(input, stride, intermediate);
        fadst8x8_avx2_16s<1>(intermediate, 8, coeff);
    } else { assert(tx_type == ADST_ADST);
        fadst8x8_avx2_16s<0>(input, stride, intermediate);
        fadst8x8_avx2_16s<1>(intermediate, 8, coeff);
    }
}

static void ftransform_16x16_avx2(const short *input, short *coeff, int stride, TxType tx_type)
{
    assert((size_t(input) & 31) == 0);
    assert((size_t(coeff) & 31) == 0);
    assert(fwd_txfm_shift_ls[TX_16X16][0] == 2);
    assert(fwd_txfm_shift_ls[TX_16X16][1] == -2);
    assert(fwd_txfm_shift_ls[TX_16X16][2] == 0);
    assert(fwd_cos_bit_col[TX_16X16][TX_16X16] == 13);
    assert(fwd_cos_bit_row[TX_16X16][TX_16X16] == 12);

    alignas(32) short intermediate[16 * 16];

    if (tx_type == DCT_DCT) {
        fdct16x16_avx2_16s<13, 2>(input, stride, intermediate);
        fdct16x16_avx2_16s<12,-2>(intermediate, 16, coeff);
    } else if (tx_type == ADST_DCT) {
        fadst16x16_avx2_16s<0>(input, stride, intermediate);
        fdct16x16_avx2_16s<12,-2>(intermediate, 16, coeff);
    } else if (tx_type == DCT_ADST) {
        fdct16x16_avx2_16s<13, 2>(input, stride, intermediate);
        fadst16x16_avx2_16s<1>(intermediate, 16, coeff);
    } else { assert(tx_type == ADST_ADST);
        fadst16x16_avx2_16s<0>(input, stride, intermediate);
        fadst16x16_avx2_16s<1>(intermediate, 16, coeff);
    }
}

static inline void transpose_16x16_(const __m256i *in, __m256i *out)
{
    __m256i u[16], v[16];
    u[ 0] = _mm256_unpacklo_epi16(in[ 0], in[ 1]);
    u[ 1] = _mm256_unpacklo_epi16(in[ 2], in[ 3]);
    u[ 2] = _mm256_unpacklo_epi16(in[ 4], in[ 5]);
    u[ 3] = _mm256_unpacklo_epi16(in[ 6], in[ 7]);
    u[ 4] = _mm256_unpacklo_epi16(in[ 8], in[ 9]);
    u[ 5] = _mm256_unpacklo_epi16(in[10], in[11]);
    u[ 6] = _mm256_unpacklo_epi16(in[12], in[13]);
    u[ 7] = _mm256_unpacklo_epi16(in[14], in[15]);
    u[ 8] = _mm256_unpackhi_epi16(in[ 0], in[ 1]);
    u[ 9] = _mm256_unpackhi_epi16(in[ 2], in[ 3]);
    u[10] = _mm256_unpackhi_epi16(in[ 4], in[ 5]);
    u[11] = _mm256_unpackhi_epi16(in[ 6], in[ 7]);
    u[12] = _mm256_unpackhi_epi16(in[ 8], in[ 9]);
    u[13] = _mm256_unpackhi_epi16(in[10], in[11]);
    u[14] = _mm256_unpackhi_epi16(in[12], in[13]);
    u[15] = _mm256_unpackhi_epi16(in[14], in[15]);

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

    out[ 0 * 2] = _mm256_permute2x128_si256(u[ 0], u[ 1], PERM2x128(0,2));
    out[ 1 * 2] = _mm256_permute2x128_si256(u[ 2], u[ 3], PERM2x128(0,2));
    out[ 2 * 2] = _mm256_permute2x128_si256(u[ 4], u[ 5], PERM2x128(0,2));
    out[ 3 * 2] = _mm256_permute2x128_si256(u[ 6], u[ 7], PERM2x128(0,2));
    out[ 4 * 2] = _mm256_permute2x128_si256(u[ 8], u[ 9], PERM2x128(0,2));
    out[ 5 * 2] = _mm256_permute2x128_si256(u[10], u[11], PERM2x128(0,2));
    out[ 6 * 2] = _mm256_permute2x128_si256(u[12], u[13], PERM2x128(0,2));
    out[ 7 * 2] = _mm256_permute2x128_si256(u[14], u[15], PERM2x128(0,2));
    out[ 8 * 2] = _mm256_permute2x128_si256(u[ 0], u[ 1], PERM2x128(1,3));
    out[ 9 * 2] = _mm256_permute2x128_si256(u[ 2], u[ 3], PERM2x128(1,3));
    out[10 * 2] = _mm256_permute2x128_si256(u[ 4], u[ 5], PERM2x128(1,3));
    out[11 * 2] = _mm256_permute2x128_si256(u[ 6], u[ 7], PERM2x128(1,3));
    out[12 * 2] = _mm256_permute2x128_si256(u[ 8], u[ 9], PERM2x128(1,3));
    out[13 * 2] = _mm256_permute2x128_si256(u[10], u[11], PERM2x128(1,3));
    out[14 * 2] = _mm256_permute2x128_si256(u[12], u[13], PERM2x128(1,3));
    out[15 * 2] = _mm256_permute2x128_si256(u[14], u[15], PERM2x128(1,3));
}

static void ftransform_32x32_avx2(const short *input, short *coeff, int pitch)
{
    assert((size_t(input) & 31) == 0);
    assert((size_t(coeff) & 31) == 0);
    assert(fwd_txfm_shift_ls[TX_32X32][0] == 2);
    assert(fwd_txfm_shift_ls[TX_32X32][1] == -4);
    assert(fwd_txfm_shift_ls[TX_32X32][2] == 0);
    assert(fwd_cos_bit_col[TX_32X32][TX_32X32] == 12);
    assert(fwd_cos_bit_row[TX_32X32][TX_32X32] == 12);

    const int bit = 12;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_m32_p32 = mm256_set2_epi16(-cospi[32],  cospi[32]);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_m16_p48 = mm256_set2_epi16(-cospi[16],  cospi[48]);
    const __m256i cospi_p48_p16 = mm256_set2_epi16( cospi[48],  cospi[16]);
    const __m256i cospi_m48_m16 = mm256_set2_epi16(-cospi[48], -cospi[16]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p56_p08 = mm256_set2_epi16( cospi[56],  cospi[ 8]);
    const __m256i cospi_m08_p56 = mm256_set2_epi16(-cospi[ 8],  cospi[56]);
    const __m256i cospi_p24_p40 = mm256_set2_epi16( cospi[24],  cospi[40]);
    const __m256i cospi_m40_p24 = mm256_set2_epi16(-cospi[40],  cospi[24]);
    const __m256i cospi_m56_m08 = mm256_set2_epi16(-cospi[56], -cospi[ 8]);
    const __m256i cospi_m24_m40 = mm256_set2_epi16(-cospi[24], -cospi[40]);
    const __m256i cospi_p60_p04 = mm256_set2_epi16( cospi[60],  cospi[ 4]);
    const __m256i cospi_m04_p60 = mm256_set2_epi16(-cospi[ 4],  cospi[60]);
    const __m256i cospi_p28_p36 = mm256_set2_epi16( cospi[28],  cospi[36]);
    const __m256i cospi_m36_p28 = mm256_set2_epi16(-cospi[36],  cospi[28]);
    const __m256i cospi_p44_p20 = mm256_set2_epi16( cospi[44],  cospi[20]);
    const __m256i cospi_m20_p44 = mm256_set2_epi16(-cospi[20],  cospi[44]);
    const __m256i cospi_p12_p52 = mm256_set2_epi16( cospi[12],  cospi[52]);
    const __m256i cospi_m52_p12 = mm256_set2_epi16(-cospi[52],  cospi[12]);
    const __m256i cospi_p62_p02 = mm256_set2_epi16( cospi[62],  cospi[ 2]);
    const __m256i cospi_m02_p62 = mm256_set2_epi16(-cospi[ 2],  cospi[62]);
    const __m256i cospi_p30_p34 = mm256_set2_epi16( cospi[30],  cospi[34]);
    const __m256i cospi_m34_p30 = mm256_set2_epi16(-cospi[34],  cospi[30]);
    const __m256i cospi_p46_p18 = mm256_set2_epi16( cospi[46],  cospi[18]);
    const __m256i cospi_m18_p46 = mm256_set2_epi16(-cospi[18],  cospi[46]);
    const __m256i cospi_p14_p50 = mm256_set2_epi16( cospi[14],  cospi[50]);
    const __m256i cospi_m50_p14 = mm256_set2_epi16(-cospi[50],  cospi[14]);
    const __m256i cospi_p54_p10 = mm256_set2_epi16( cospi[54],  cospi[10]);
    const __m256i cospi_m10_p54 = mm256_set2_epi16(-cospi[10],  cospi[54]);
    const __m256i cospi_p22_p42 = mm256_set2_epi16( cospi[22],  cospi[42]);
    const __m256i cospi_m42_p22 = mm256_set2_epi16(-cospi[42],  cospi[22]);
    const __m256i cospi_p38_p26 = mm256_set2_epi16( cospi[38],  cospi[26]);
    const __m256i cospi_m26_p38 = mm256_set2_epi16(-cospi[26],  cospi[38]);
    const __m256i cospi_p06_p58 = mm256_set2_epi16( cospi[ 6],  cospi[58]);
    const __m256i cospi_m58_p06 = mm256_set2_epi16(-cospi[58],  cospi[ 6]);

    __m256i a[32], b[32], c[32], d[32], e[32], f[32], g[32], h[32], j[32], intermediate[64];

    pitch >>= 4;
    for (int rows = 0; rows < 2; rows++) {
        __m256i *pout = rows ? (__m256i *)coeff : intermediate;
        __m256i *pin  = rows ? intermediate : (__m256i *)input;

        for (int col = 0; col < 2; col++, pout += 32, pin++) {
            // stage 0
            if (rows) {
                const __m256i eight = _mm256_set1_epi16(8);
                a[ 0] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 0 * 2], eight), 4);
                a[ 1] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 1 * 2], eight), 4);
                a[ 2] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 2 * 2], eight), 4);
                a[ 3] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 3 * 2], eight), 4);
                a[ 4] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 4 * 2], eight), 4);
                a[ 5] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 5 * 2], eight), 4);
                a[ 6] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 6 * 2], eight), 4);
                a[ 7] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 7 * 2], eight), 4);
                a[ 8] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 8 * 2], eight), 4);
                a[ 9] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 9 * 2], eight), 4);
                a[10] = _mm256_srai_epi16(_mm256_add_epi16(pin[10 * 2], eight), 4);
                a[11] = _mm256_srai_epi16(_mm256_add_epi16(pin[11 * 2], eight), 4);
                a[12] = _mm256_srai_epi16(_mm256_add_epi16(pin[12 * 2], eight), 4);
                a[13] = _mm256_srai_epi16(_mm256_add_epi16(pin[13 * 2], eight), 4);
                a[14] = _mm256_srai_epi16(_mm256_add_epi16(pin[14 * 2], eight), 4);
                a[15] = _mm256_srai_epi16(_mm256_add_epi16(pin[15 * 2], eight), 4);
                a[16] = _mm256_srai_epi16(_mm256_add_epi16(pin[16 * 2], eight), 4);
                a[17] = _mm256_srai_epi16(_mm256_add_epi16(pin[17 * 2], eight), 4);
                a[18] = _mm256_srai_epi16(_mm256_add_epi16(pin[18 * 2], eight), 4);
                a[19] = _mm256_srai_epi16(_mm256_add_epi16(pin[19 * 2], eight), 4);
                a[20] = _mm256_srai_epi16(_mm256_add_epi16(pin[20 * 2], eight), 4);
                a[21] = _mm256_srai_epi16(_mm256_add_epi16(pin[21 * 2], eight), 4);
                a[22] = _mm256_srai_epi16(_mm256_add_epi16(pin[22 * 2], eight), 4);
                a[23] = _mm256_srai_epi16(_mm256_add_epi16(pin[23 * 2], eight), 4);
                a[24] = _mm256_srai_epi16(_mm256_add_epi16(pin[24 * 2], eight), 4);
                a[25] = _mm256_srai_epi16(_mm256_add_epi16(pin[25 * 2], eight), 4);
                a[26] = _mm256_srai_epi16(_mm256_add_epi16(pin[26 * 2], eight), 4);
                a[27] = _mm256_srai_epi16(_mm256_add_epi16(pin[27 * 2], eight), 4);
                a[28] = _mm256_srai_epi16(_mm256_add_epi16(pin[28 * 2], eight), 4);
                a[29] = _mm256_srai_epi16(_mm256_add_epi16(pin[29 * 2], eight), 4);
                a[30] = _mm256_srai_epi16(_mm256_add_epi16(pin[30 * 2], eight), 4);
                a[31] = _mm256_srai_epi16(_mm256_add_epi16(pin[31 * 2], eight), 4);
            } else {
                a[ 0] = _mm256_slli_epi16(pin[ 0 * pitch], 2);
                a[ 1] = _mm256_slli_epi16(pin[ 1 * pitch], 2);
                a[ 2] = _mm256_slli_epi16(pin[ 2 * pitch], 2);
                a[ 3] = _mm256_slli_epi16(pin[ 3 * pitch], 2);
                a[ 4] = _mm256_slli_epi16(pin[ 4 * pitch], 2);
                a[ 5] = _mm256_slli_epi16(pin[ 5 * pitch], 2);
                a[ 6] = _mm256_slli_epi16(pin[ 6 * pitch], 2);
                a[ 7] = _mm256_slli_epi16(pin[ 7 * pitch], 2);
                a[ 8] = _mm256_slli_epi16(pin[ 8 * pitch], 2);
                a[ 9] = _mm256_slli_epi16(pin[ 9 * pitch], 2);
                a[10] = _mm256_slli_epi16(pin[10 * pitch], 2);
                a[11] = _mm256_slli_epi16(pin[11 * pitch], 2);
                a[12] = _mm256_slli_epi16(pin[12 * pitch], 2);
                a[13] = _mm256_slli_epi16(pin[13 * pitch], 2);
                a[14] = _mm256_slli_epi16(pin[14 * pitch], 2);
                a[15] = _mm256_slli_epi16(pin[15 * pitch], 2);
                a[16] = _mm256_slli_epi16(pin[16 * pitch], 2);
                a[17] = _mm256_slli_epi16(pin[17 * pitch], 2);
                a[18] = _mm256_slli_epi16(pin[18 * pitch], 2);
                a[19] = _mm256_slli_epi16(pin[19 * pitch], 2);
                a[20] = _mm256_slli_epi16(pin[20 * pitch], 2);
                a[21] = _mm256_slli_epi16(pin[21 * pitch], 2);
                a[22] = _mm256_slli_epi16(pin[22 * pitch], 2);
                a[23] = _mm256_slli_epi16(pin[23 * pitch], 2);
                a[24] = _mm256_slli_epi16(pin[24 * pitch], 2);
                a[25] = _mm256_slli_epi16(pin[25 * pitch], 2);
                a[26] = _mm256_slli_epi16(pin[26 * pitch], 2);
                a[27] = _mm256_slli_epi16(pin[27 * pitch], 2);
                a[28] = _mm256_slli_epi16(pin[28 * pitch], 2);
                a[29] = _mm256_slli_epi16(pin[29 * pitch], 2);
                a[30] = _mm256_slli_epi16(pin[30 * pitch], 2);
                a[31] = _mm256_slli_epi16(pin[31 * pitch], 2);
            }

            // stage 1
            b[ 0] = _mm256_add_epi16(a[ 0], a[31]);
            b[31] = _mm256_sub_epi16(a[ 0], a[31]);
            b[ 1] = _mm256_add_epi16(a[ 1], a[30]);
            b[30] = _mm256_sub_epi16(a[ 1], a[30]);
            b[ 2] = _mm256_add_epi16(a[ 2], a[29]);
            b[29] = _mm256_sub_epi16(a[ 2], a[29]);
            b[ 3] = _mm256_add_epi16(a[ 3], a[28]);
            b[28] = _mm256_sub_epi16(a[ 3], a[28]);
            b[ 4] = _mm256_add_epi16(a[ 4], a[27]);
            b[27] = _mm256_sub_epi16(a[ 4], a[27]);
            b[ 5] = _mm256_add_epi16(a[ 5], a[26]);
            b[26] = _mm256_sub_epi16(a[ 5], a[26]);
            b[ 6] = _mm256_add_epi16(a[ 6], a[25]);
            b[25] = _mm256_sub_epi16(a[ 6], a[25]);
            b[ 7] = _mm256_add_epi16(a[ 7], a[24]);
            b[24] = _mm256_sub_epi16(a[ 7], a[24]);
            b[ 8] = _mm256_add_epi16(a[ 8], a[23]);
            b[23] = _mm256_sub_epi16(a[ 8], a[23]);
            b[ 9] = _mm256_add_epi16(a[ 9], a[22]);
            b[22] = _mm256_sub_epi16(a[ 9], a[22]);
            b[10] = _mm256_add_epi16(a[10], a[21]);
            b[21] = _mm256_sub_epi16(a[10], a[21]);
            b[11] = _mm256_add_epi16(a[11], a[20]);
            b[20] = _mm256_sub_epi16(a[11], a[20]);
            b[12] = _mm256_add_epi16(a[12], a[19]);
            b[19] = _mm256_sub_epi16(a[12], a[19]);
            b[13] = _mm256_add_epi16(a[13], a[18]);
            b[18] = _mm256_sub_epi16(a[13], a[18]);
            b[14] = _mm256_add_epi16(a[14], a[17]);
            b[17] = _mm256_sub_epi16(a[14], a[17]);
            b[15] = _mm256_add_epi16(a[15], a[16]);
            b[16] = _mm256_sub_epi16(a[15], a[16]);

            // process first 16 rows
            // stage 2
            c[ 0] = _mm256_add_epi16(b[ 0], b[15]);
            c[15] = _mm256_sub_epi16(b[ 0], b[15]);
            c[ 1] = _mm256_add_epi16(b[ 1], b[14]);
            c[14] = _mm256_sub_epi16(b[ 1], b[14]);
            c[ 2] = _mm256_add_epi16(b[ 2], b[13]);
            c[13] = _mm256_sub_epi16(b[ 2], b[13]);
            c[ 3] = _mm256_add_epi16(b[ 3], b[12]);
            c[12] = _mm256_sub_epi16(b[ 3], b[12]);
            c[ 4] = _mm256_add_epi16(b[ 4], b[11]);
            c[11] = _mm256_sub_epi16(b[ 4], b[11]);
            c[ 5] = _mm256_add_epi16(b[ 5], b[10]);
            c[10] = _mm256_sub_epi16(b[ 5], b[10]);
            c[ 6] = _mm256_add_epi16(b[ 6], b[ 9]);
            c[ 9] = _mm256_sub_epi16(b[ 6], b[ 9]);
            c[ 7] = _mm256_add_epi16(b[ 7], b[ 8]);
            c[ 8] = _mm256_sub_epi16(b[ 7], b[ 8]);
            // stage 3
            d[ 0] = _mm256_add_epi16(c[0], c[7]);
            d[ 7] = _mm256_sub_epi16(c[0], c[7]);
            d[ 1] = _mm256_add_epi16(c[1], c[6]);
            d[ 6] = _mm256_sub_epi16(c[1], c[6]);
            d[ 2] = _mm256_add_epi16(c[2], c[5]);
            d[ 5] = _mm256_sub_epi16(c[2], c[5]);
            d[ 3] = _mm256_add_epi16(c[3], c[4]);
            d[ 4] = _mm256_sub_epi16(c[3], c[4]);
            btf_avx2(cospi_m32_p32, cospi_p32_p32, c[10], c[13], bit, &d[10], &d[13]);
            btf_avx2(cospi_m32_p32, cospi_p32_p32, c[11], c[12], bit, &d[11], &d[12]);
            // stage 4
            e[ 0] = _mm256_add_epi16(d[ 0], d[ 3]);
            e[ 3] = _mm256_sub_epi16(d[ 0], d[ 3]);
            e[ 1] = _mm256_add_epi16(d[ 1], d[ 2]);
            e[ 2] = _mm256_sub_epi16(d[ 1], d[ 2]);
            btf_avx2(cospi_m32_p32, cospi_p32_p32, d[5], d[6], bit, &e[5], &e[6]);
            e[ 8] = _mm256_add_epi16(c[ 8], d[11]);
            e[11] = _mm256_sub_epi16(c[ 8], d[11]);
            e[ 9] = _mm256_add_epi16(c[ 9], d[10]);
            e[10] = _mm256_sub_epi16(c[ 9], d[10]);
            e[12] = _mm256_sub_epi16(c[15], d[12]);
            e[15] = _mm256_add_epi16(c[15], d[12]);
            e[13] = _mm256_sub_epi16(c[14], d[13]);
            e[14] = _mm256_add_epi16(c[14], d[13]);
            // stage 5
            btf_avx2(cospi_p32_p32, cospi_p32_m32, e[0], e[1], bit, &j[ 0], &j[16]);
            btf_avx2(cospi_p48_p16, cospi_m16_p48, e[2], e[3], bit, &j[ 8], &j[24]);
            f[ 4] = _mm256_add_epi16(d[ 4], e[ 5]);
            f[ 5] = _mm256_sub_epi16(d[ 4], e[ 5]);
            f[ 6] = _mm256_sub_epi16(d[ 7], e[ 6]);
            f[ 7] = _mm256_add_epi16(d[ 7], e[ 6]);
            btf_avx2(cospi_m16_p48, cospi_p48_p16, e[ 9], e[14], bit, &f[ 9], &f[14]);
            btf_avx2(cospi_m48_m16, cospi_m16_p48, e[10], e[13], bit, &f[10], &f[13]);
            // stage 6
            btf_avx2(cospi_p56_p08, cospi_m08_p56, f[4], f[7], bit, &j[ 4], &j[28]);
            btf_avx2(cospi_p24_p40, cospi_m40_p24, f[5], f[6], bit, &j[20], &j[12]);
            g[ 8] = _mm256_add_epi16(e[ 8], f[ 9]);
            g[ 9] = _mm256_sub_epi16(e[ 8], f[ 9]);
            g[10] = _mm256_sub_epi16(e[11], f[10]);
            g[11] = _mm256_add_epi16(e[11], f[10]);
            g[12] = _mm256_add_epi16(e[12], f[13]);
            g[13] = _mm256_sub_epi16(e[12], f[13]);
            g[14] = _mm256_sub_epi16(e[15], f[14]);
            g[15] = _mm256_add_epi16(e[15], f[14]);
            // stage 7
            // stage 8
            // stage 9
            btf_avx2(cospi_p60_p04, cospi_m04_p60, g[ 8], g[15], bit, &j[ 2], &j[30]);
            btf_avx2(cospi_p28_p36, cospi_m36_p28, g[ 9], g[14], bit, &j[18], &j[14]);
            btf_avx2(cospi_p44_p20, cospi_m20_p44, g[10], g[13], bit, &j[10], &j[22]);
            btf_avx2(cospi_p12_p52, cospi_m52_p12, g[11], g[12], bit, &j[26], &j[ 6]);

            // process second 16 rows
            // stage 2
            btf_avx2(cospi_m32_p32, cospi_p32_p32, b[20], b[27], bit, &c[20], &c[27]);
            btf_avx2(cospi_m32_p32, cospi_p32_p32, b[21], b[26], bit, &c[21], &c[26]);
            btf_avx2(cospi_m32_p32, cospi_p32_p32, b[22], b[25], bit, &c[22], &c[25]);
            btf_avx2(cospi_m32_p32, cospi_p32_p32, b[23], b[24], bit, &c[23], &c[24]);
            // stage 3
            d[16] = _mm256_add_epi16(b[16], c[23]);
            d[23] = _mm256_sub_epi16(b[16], c[23]);
            d[17] = _mm256_add_epi16(b[17], c[22]);
            d[22] = _mm256_sub_epi16(b[17], c[22]);
            d[18] = _mm256_add_epi16(b[18], c[21]);
            d[21] = _mm256_sub_epi16(b[18], c[21]);
            d[19] = _mm256_add_epi16(b[19], c[20]);
            d[20] = _mm256_sub_epi16(b[19], c[20]);
            d[24] = _mm256_sub_epi16(b[31], c[24]);
            d[31] = _mm256_add_epi16(b[31], c[24]);
            d[25] = _mm256_sub_epi16(b[30], c[25]);
            d[30] = _mm256_add_epi16(b[30], c[25]);
            d[26] = _mm256_sub_epi16(b[29], c[26]);
            d[29] = _mm256_add_epi16(b[29], c[26]);
            d[27] = _mm256_sub_epi16(b[28], c[27]);
            d[28] = _mm256_add_epi16(b[28], c[27]);
            // stage 4
            btf_avx2(cospi_m16_p48, cospi_p48_p16, d[18], d[29], bit, &e[18], &e[29]);
            btf_avx2(cospi_m16_p48, cospi_p48_p16, d[19], d[28], bit, &e[19], &e[28]);
            btf_avx2(cospi_m48_m16, cospi_m16_p48, d[20], d[27], bit, &e[20], &e[27]);
            btf_avx2(cospi_m48_m16, cospi_m16_p48, d[21], d[26], bit, &e[21], &e[26]);
            // stage 5
            f[16] = _mm256_add_epi16(d[16], e[19]);
            f[19] = _mm256_sub_epi16(d[16], e[19]);
            f[17] = _mm256_add_epi16(d[17], e[18]);
            f[18] = _mm256_sub_epi16(d[17], e[18]);
            f[20] = _mm256_sub_epi16(d[23], e[20]);
            f[23] = _mm256_add_epi16(d[23], e[20]);
            f[21] = _mm256_sub_epi16(d[22], e[21]);
            f[22] = _mm256_add_epi16(d[22], e[21]);
            f[24] = _mm256_add_epi16(d[24], e[27]);
            f[27] = _mm256_sub_epi16(d[24], e[27]);
            f[25] = _mm256_add_epi16(d[25], e[26]);
            f[26] = _mm256_sub_epi16(d[25], e[26]);
            f[28] = _mm256_sub_epi16(d[31], e[28]);
            f[31] = _mm256_add_epi16(d[31], e[28]);
            f[29] = _mm256_sub_epi16(d[30], e[29]);
            f[30] = _mm256_add_epi16(d[30], e[29]);
            // stage 6
            btf_avx2(cospi_m08_p56, cospi_p56_p08, f[17], f[30], bit, &g[17], &g[30]);
            btf_avx2(cospi_m56_m08, cospi_m08_p56, f[18], f[29], bit, &g[18], &g[29]);
            btf_avx2(cospi_m40_p24, cospi_p24_p40, f[21], f[26], bit, &g[21], &g[26]);
            btf_avx2(cospi_m24_m40, cospi_m40_p24, f[22], f[25], bit, &g[22], &g[25]);
            // stage 7
            h[16] = _mm256_add_epi16(f[16], g[17]);
            h[17] = _mm256_sub_epi16(f[16], g[17]);
            h[18] = _mm256_sub_epi16(f[19], g[18]);
            h[19] = _mm256_add_epi16(f[19], g[18]);
            h[20] = _mm256_add_epi16(f[20], g[21]);
            h[21] = _mm256_sub_epi16(f[20], g[21]);
            h[22] = _mm256_sub_epi16(f[23], g[22]);
            h[23] = _mm256_add_epi16(f[23], g[22]);
            h[24] = _mm256_add_epi16(f[24], g[25]);
            h[25] = _mm256_sub_epi16(f[24], g[25]);
            h[26] = _mm256_sub_epi16(f[27], g[26]);
            h[27] = _mm256_add_epi16(f[27], g[26]);
            h[28] = _mm256_add_epi16(f[28], g[29]);
            h[29] = _mm256_sub_epi16(f[28], g[29]);
            h[30] = _mm256_sub_epi16(f[31], g[30]);
            h[31] = _mm256_add_epi16(f[31], g[30]);
            // stage 8
            // stage 9
            btf_avx2(cospi_p62_p02, cospi_m02_p62, h[16], h[31], bit, &j[ 1], &j[31]);
            btf_avx2(cospi_p30_p34, cospi_m34_p30, h[17], h[30], bit, &j[17], &j[15]);
            btf_avx2(cospi_p46_p18, cospi_m18_p46, h[18], h[29], bit, &j[ 9], &j[23]);
            btf_avx2(cospi_p14_p50, cospi_m50_p14, h[19], h[28], bit, &j[25], &j[ 7]);
            btf_avx2(cospi_p54_p10, cospi_m10_p54, h[20], h[27], bit, &j[ 5], &j[27]);
            btf_avx2(cospi_p22_p42, cospi_m42_p22, h[21], h[26], bit, &j[21], &j[11]);
            btf_avx2(cospi_p38_p26, cospi_m26_p38, h[22], h[25], bit, &j[13], &j[19]);
            btf_avx2(cospi_p06_p58, cospi_m58_p06, h[23], h[24], bit, &j[29], &j[ 3]);

            transpose_16x16_(j +  0, pout + 0);
            transpose_16x16_(j + 16, pout + 1);
        }
    }
}

alignas(16) static const char interleave_tab[16] = {0,1,8,9,2,3,10,11,4,5,12,13,6,7,14,15};

template <int rows> static void fdct4_8_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = 13;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p48_p16 = mm256_set2_epi16( cospi[48],  cospi[16]);
    const __m256i cospi_m16_p48 = mm256_set2_epi16(-cospi[16],  cospi[48]);

    __m256i a01, a32;

    if (rows) {
        const __m256i one = _mm256_set1_epi16(1);
        a01 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 0 * pitch, in + 1 * pitch), one), 1);
        a32 = _mm256_srai_epi16(_mm256_add_epi16(loada2_m128i(in + 3 * pitch, in + 2 * pitch), one), 1);
    } else {
        a01 = _mm256_slli_epi16(loada2_m128i(in + 0 * pitch, in + 1 * pitch), 2);
        a32 = _mm256_slli_epi16(loada2_m128i(in + 3 * pitch, in + 2 * pitch), 2);
    }

    // stage 1
    __m256i b01 = _mm256_add_epi16(a01, a32);
    __m256i b32 = _mm256_sub_epi16(a01, a32);

    // stage 2
    __m256i c01 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(b01, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(b01, PERM4x64(2,2,3,3)));
    __m256i c23 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(b32, PERM4x64(2,2,3,3)),
                                        _mm256_permute4x64_epi64(b32, PERM4x64(0,0,1,1)));
    __m256i d02 = btf16_avx2(cospi_p32_p32, cospi_p32_m32, c01, bit);
    __m256i d13 = btf16_avx2(cospi_p48_p16, cospi_m16_p48, c23, bit);

    // stage 3 and transpose
    __m256i e0 = _mm256_unpacklo_epi16(d02, d13);
    __m256i e1 = _mm256_unpackhi_epi16(d02, d13);
    __m256i f0 = _mm256_unpacklo_epi64(e0, e1);
    __m256i f1 = _mm256_unpackhi_epi64(e0, e1);
    __m256i g0 = _mm256_permute2x128_si256(f0, f1, PERM2x128(0,2));
    __m256i g1 = _mm256_permute2x128_si256(f0, f1, PERM2x128(1,3));
    __m256i *pout = (__m256i *)out;
    pout[0] = _mm256_unpacklo_epi32(g0, g1);
    pout[1] = _mm256_unpackhi_epi32(g0, g1);

    if (rows) {
        __m256i multiplier = _mm256_set1_epi16((NewSqrt2 - 4096) * 8);
        pout[0] = _mm256_add_epi16(pout[0], _mm256_mulhrs_epi16(pout[0], multiplier));
        pout[1] = _mm256_add_epi16(pout[1], _mm256_mulhrs_epi16(pout[1], multiplier));
    }
}

template <int rows> static void fdct8_4_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = 13;
    const int *cospi = cospi_arr(bit);
    const __m128i cospi_p32_p32 = mm_set2_epi16( cospi[32],  cospi[32]);
    const __m128i cospi_m32_p32 = mm_set2_epi16(-cospi[32],  cospi[32]);
    const __m128i cospi_p32_m32 = mm_set2_epi16( cospi[32], -cospi[32]);
    const __m128i cospi_p16_p48 = mm_set2_epi16( cospi[16],  cospi[48]);
    const __m128i cospi_p48_m16 = mm_set2_epi16( cospi[48], -cospi[16]);
    const __m128i cospi_p56_p08 = mm_set2_epi16( cospi[56],  cospi[ 8]);
    const __m128i cospi_m08_p56 = mm_set2_epi16(-cospi[ 8],  cospi[56]);
    const __m128i cospi_p24_p40 = mm_set2_epi16( cospi[24],  cospi[40]);
    const __m128i cospi_m40_p24 = mm_set2_epi16(-cospi[40],  cospi[24]);
    const __m128i tab = *(const __m128i *)interleave_tab;

    __m128i a01, a32, a45, a76;

    if (rows) {
        const __m128i one = _mm_set1_epi16(1);
        a01 = _mm_srai_epi16(_mm_add_epi16(loadu2_epi64(in + 0 * pitch, in + 1 * pitch), one), 1);
        a32 = _mm_srai_epi16(_mm_add_epi16(loadu2_epi64(in + 3 * pitch, in + 2 * pitch), one), 1);
        a45 = _mm_srai_epi16(_mm_add_epi16(loadu2_epi64(in + 4 * pitch, in + 5 * pitch), one), 1);
        a76 = _mm_srai_epi16(_mm_add_epi16(loadu2_epi64(in + 7 * pitch, in + 6 * pitch), one), 1);
    } else {
        a01 = _mm_slli_epi16(loadu2_epi64(in + 0 * pitch, in + 1 * pitch), 2);
        a32 = _mm_slli_epi16(loadu2_epi64(in + 3 * pitch, in + 2 * pitch), 2);
        a45 = _mm_slli_epi16(loadu2_epi64(in + 4 * pitch, in + 5 * pitch), 2);
        a76 = _mm_slli_epi16(loadu2_epi64(in + 7 * pitch, in + 6 * pitch), 2);
    }

    // stage 1
    __m128i b01 = _mm_add_epi16(a01, a76);
    __m128i b32 = _mm_add_epi16(a32, a45);
    __m128i b45 = _mm_sub_epi16(a32, a45);
    __m128i b76 = _mm_sub_epi16(a01, a76);

    // stage 2
    __m128i c01 = _mm_add_epi16(b01, b32);
    __m128i c32 = _mm_sub_epi16(b01, b32);
    __m128i c47 = _mm_unpacklo_epi64(b45, b76);
    __m128i c56 = btf16_sse4(cospi_m32_p32, cospi_p32_p32, _mm_unpackhi_epi16(b45, b76), bit);

    // stage 3
    __m128i u[4];
    u[0] = btf16_sse4(cospi_p32_p32, cospi_p32_m32, _mm_shuffle_epi8(c01, tab), bit);
    u[1] = btf16_sse4(cospi_p16_p48, cospi_p48_m16, _mm_shuffle_epi8(c32, tab), bit);
    __m128i d47 = _mm_add_epi16(c47, c56);
    __m128i d56 = _mm_sub_epi16(c47, c56);

    // stage u
    u[2] = btf16_sse4(cospi_p56_p08, cospi_m08_p56, _mm_shuffle_epi8(d47, tab), bit);
    u[3] = btf16_sse4(cospi_p24_p40, cospi_m40_p24, _mm_shuffle_epi8(d56, tab), bit);
    u[3] = _mm_shuffle_epi32(u[3], SHUF32(2,3,0,1));
    // u is
    //  A0 A1 A2 A3 E0 E1 E2 E3
    //  C0 C1 C2 C3 G0 G1 G2 G3
    //  B0 B1 B2 B3 H0 H1 H2 H3
    //  D0 D1 D2 D3 F0 F1 F2 F3


    // stage 5 and transpose
    __m128i v[4], *pout = (__m128i *)out;
    v[0] = _mm_unpacklo_epi16(u[0], u[2]); //AB
    v[1] = _mm_unpacklo_epi16(u[1], u[3]); //CD
    v[2] = _mm_unpackhi_epi16(u[0], u[3]); //EF
    v[3] = _mm_unpackhi_epi16(u[1], u[2]); //GH
    u[0] = _mm_unpacklo_epi32(v[0], v[1]);
    u[1] = _mm_unpacklo_epi32(v[2], v[3]);
    u[2] = _mm_unpackhi_epi32(v[0], v[1]);
    u[3] = _mm_unpackhi_epi32(v[2], v[3]);
    pout[0] = _mm_unpacklo_epi64(u[0], u[1]);
    pout[1] = _mm_unpackhi_epi64(u[0], u[1]);
    pout[2] = _mm_unpacklo_epi64(u[2], u[3]);
    pout[3] = _mm_unpackhi_epi64(u[2], u[3]);

    if (rows) {
        __m128i multiplier = _mm_set1_epi16((NewSqrt2 - 4096) * 8);
        pout[0] = _mm_add_epi16(pout[0], _mm_mulhrs_epi16(pout[0], multiplier));
        pout[1] = _mm_add_epi16(pout[1], _mm_mulhrs_epi16(pout[1], multiplier));
        pout[2] = _mm_add_epi16(pout[2], _mm_mulhrs_epi16(pout[2], multiplier));
        pout[3] = _mm_add_epi16(pout[3], _mm_mulhrs_epi16(pout[3], multiplier));
    }
}

template <int rows> static void fadst8_4_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = 13;
    const int *cospi = cospi_arr(bit);
    const __m128i cospi_p32_p32 = mm_set2_epi16( cospi[32],  cospi[32]);
    const __m128i cospi_p32_m32 = mm_set2_epi16( cospi[32], -cospi[32]);
    const __m128i cospi_p16_p48 = mm_set2_epi16( cospi[16],  cospi[48]);
    const __m128i cospi_p48_m16 = mm_set2_epi16( cospi[48], -cospi[16]);
    const __m128i cospi_m48_p16 = mm_set2_epi16(-cospi[48],  cospi[16]);
    const __m128i cospi_p04_p60 = mm_set2_epi16( cospi[04],  cospi[60]);
    const __m128i cospi_p60_m04 = mm_set2_epi16( cospi[60], -cospi[04]);
    const __m128i cospi_p20_p44 = mm_set2_epi16( cospi[20],  cospi[44]);
    const __m128i cospi_p44_m20 = mm_set2_epi16( cospi[44], -cospi[20]);
    const __m128i cospi_p36_p28 = mm_set2_epi16( cospi[36],  cospi[28]);
    const __m128i cospi_p28_m36 = mm_set2_epi16( cospi[28], -cospi[36]);
    const __m128i cospi_p52_p12 = mm_set2_epi16( cospi[52],  cospi[12]);
    const __m128i cospi_p12_m52 = mm_set2_epi16( cospi[12], -cospi[52]);

    const __m128i tab = *(const __m128i *)interleave_tab;

    __m128i a04, a62, a73, a15;

    if (rows) {
        const __m128i one = _mm_set1_epi16(1);
        a04 = _mm_srai_epi16(_mm_add_epi16(loadu2_epi64(in + 0 * pitch, in + 4 * pitch), one), 1);
        a62 = _mm_srai_epi16(_mm_add_epi16(loadu2_epi64(in + 6 * pitch, in + 2 * pitch), one), 1);
        a73 = _mm_srai_epi16(_mm_add_epi16(loadu2_epi64(in + 7 * pitch, in + 3 * pitch), one), 1);
        a15 = _mm_srai_epi16(_mm_add_epi16(loadu2_epi64(in + 1 * pitch, in + 5 * pitch), one), 1);
    } else {
        a04 = _mm_slli_epi16(loadu2_epi64(in + 0 * pitch, in + 4 * pitch), 2);
        a62 = _mm_slli_epi16(loadu2_epi64(in + 6 * pitch, in + 2 * pitch), 2);
        a73 = _mm_slli_epi16(loadu2_epi64(in + 7 * pitch, in + 3 * pitch), 2);
        a15 = _mm_slli_epi16(loadu2_epi64(in + 1 * pitch, in + 5 * pitch), 2);
    }

    // stage 1
    __m128i b03 = a04;
    __m128i b12 = _mm_sub_epi16(_mm_setzero_si128(), a73);
    __m128i b47 = _mm_sub_epi16(_mm_setzero_si128(), a15);
    __m128i b56 = a62;

    // stage 2
    __m128i c01 = _mm_unpacklo_epi64(b03, b12);
    __m128i c45 = _mm_unpacklo_epi64(b47, b56);
    __m128i c23 = btf16_sse4(cospi_p32_p32, cospi_p32_m32, _mm_unpackhi_epi16(b12, b03), bit);
    __m128i c67 = btf16_sse4(cospi_p32_p32, cospi_p32_m32, _mm_unpackhi_epi16(b56, b47), bit);

    // stage 3
    __m128i d01 = _mm_add_epi16(c01, c23);
    __m128i d23 = _mm_sub_epi16(c01, c23);
    __m128i d45 = _mm_add_epi16(c45, c67);
    __m128i d67 = _mm_sub_epi16(c45, c67);

    // stage 4
    __m128i e01 = d01;
    __m128i e23 = d23;
    __m128i e45 = btf16_sse4(cospi_p16_p48, cospi_p48_m16, _mm_shuffle_epi8(d45, tab), bit);
    __m128i e67 = btf16_sse4(cospi_m48_p16, cospi_p16_p48, _mm_shuffle_epi8(d67, tab), bit);

    // stage 5
    __m128i f01 = _mm_add_epi16(e01, e45);
    __m128i f23 = _mm_add_epi16(e23, e67);
    __m128i f45 = _mm_sub_epi16(e01, e45);
    __m128i f67 = _mm_sub_epi16(e23, e67);

    // stage 6
    __m128i g01 = btf16_sse4(cospi_p04_p60, cospi_p60_m04, _mm_shuffle_epi8(f01, tab), bit);
    __m128i g23 = btf16_sse4(cospi_p20_p44, cospi_p44_m20, _mm_shuffle_epi8(f23, tab), bit);
    __m128i g45 = btf16_sse4(cospi_p36_p28, cospi_p28_m36, _mm_shuffle_epi8(f45, tab), bit);
    __m128i g67 = btf16_sse4(cospi_p52_p12, cospi_p12_m52, _mm_shuffle_epi8(f67, tab), bit);

    // stage 7 and transpose
    __m128i h07 = _mm_shuffle_epi32(g01, SHUF32(2,3,0,1));
    __m128i h16 = g67;
    __m128i h25 = _mm_shuffle_epi32(g23, SHUF32(2,3,0,1));
    __m128i h34 = g45;
    __m128i u0 = _mm_unpacklo_epi16(h07, h16);
    __m128i u1 = _mm_unpacklo_epi16(h25, h34);
    __m128i u2 = _mm_unpackhi_epi16(h34, h25);
    __m128i u3 = _mm_unpackhi_epi16(h16, h07);
    __m128i v0 = _mm_unpacklo_epi32(u0, u1);
    __m128i v1 = _mm_unpacklo_epi32(u2, u3);
    __m128i v2 = _mm_unpackhi_epi32(u0, u1);
    __m128i v3 = _mm_unpackhi_epi32(u2, u3);
    __m128i *pout = (__m128i *)out;
    pout[0] = _mm_unpacklo_epi64(v0, v1);
    pout[1] = _mm_unpackhi_epi64(v0, v1);
    pout[2] = _mm_unpacklo_epi64(v2, v3);
    pout[3] = _mm_unpackhi_epi64(v2, v3);

    if (rows) {
        __m128i multiplier = _mm_set1_epi16((NewSqrt2 - 4096) * 8);
        pout[0] = _mm_add_epi16(pout[0], _mm_mulhrs_epi16(pout[0], multiplier));
        pout[1] = _mm_add_epi16(pout[1], _mm_mulhrs_epi16(pout[1], multiplier));
        pout[2] = _mm_add_epi16(pout[2], _mm_mulhrs_epi16(pout[2], multiplier));
        pout[3] = _mm_add_epi16(pout[3], _mm_mulhrs_epi16(pout[3], multiplier));
    }
}

template <int rows> static void fdct8_16_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = 13;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_m32_p32 = mm256_set2_epi16(-cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p48_p16 = mm256_set2_epi16( cospi[48],  cospi[16]);
    const __m256i cospi_m16_p48 = mm256_set2_epi16(-cospi[16],  cospi[48]);
    const __m256i cospi_p56_p08 = mm256_set2_epi16( cospi[56],  cospi[ 8]);
    const __m256i cospi_m08_p56 = mm256_set2_epi16(-cospi[ 8],  cospi[56]);
    const __m256i cospi_p24_p40 = mm256_set2_epi16( cospi[24],  cospi[40]);
    const __m256i cospi_m40_p24 = mm256_set2_epi16(-cospi[40],  cospi[24]);
    __m256i u[8], v[8];

    if (rows) {
        __m256i two = _mm256_set1_epi16(2);
        u[0] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 0 * pitch)), 2);
        u[1] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 1 * pitch)), 2);
        u[2] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 2 * pitch)), 2);
        u[3] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 3 * pitch)), 2);
        u[4] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 4 * pitch)), 2);
        u[5] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 5 * pitch)), 2);
        u[6] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 6 * pitch)), 2);
        u[7] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 7 * pitch)), 2);
    } else {
        u[0] = _mm256_slli_epi16(loada_si256(in + 0 * pitch), 2);
        u[1] = _mm256_slli_epi16(loada_si256(in + 1 * pitch), 2);
        u[2] = _mm256_slli_epi16(loada_si256(in + 2 * pitch), 2);
        u[3] = _mm256_slli_epi16(loada_si256(in + 3 * pitch), 2);
        u[4] = _mm256_slli_epi16(loada_si256(in + 4 * pitch), 2);
        u[5] = _mm256_slli_epi16(loada_si256(in + 5 * pitch), 2);
        u[6] = _mm256_slli_epi16(loada_si256(in + 6 * pitch), 2);
        u[7] = _mm256_slli_epi16(loada_si256(in + 7 * pitch), 2);
    }

    // stage 1
    v[0] = _mm256_add_epi16(u[0], u[7]);
    v[1] = _mm256_add_epi16(u[1], u[6]);
    v[2] = _mm256_add_epi16(u[2], u[5]);
    v[3] = _mm256_add_epi16(u[3], u[4]);
    v[4] = _mm256_sub_epi16(u[3], u[4]);
    v[5] = _mm256_sub_epi16(u[2], u[5]);
    v[6] = _mm256_sub_epi16(u[1], u[6]);
    v[7] = _mm256_sub_epi16(u[0], u[7]);

    // stage 2
    u[0] = _mm256_add_epi16(v[0], v[3]);
    u[1] = _mm256_add_epi16(v[1], v[2]);
    u[2] = _mm256_sub_epi16(v[1], v[2]);
    u[3] = _mm256_sub_epi16(v[0], v[3]);
    u[4] = v[4];
    btf_avx2(cospi_m32_p32, cospi_p32_p32, v[5], v[6], bit, &u[5], &u[6]);
    u[7] = v[7];

    // stage 3
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[0], u[1], bit, &v[0], &v[1]);
    btf_avx2(cospi_p48_p16, cospi_m16_p48, u[2], u[3], bit, &v[2], &v[3]);
    v[4] = _mm256_add_epi16(u[4], u[5]);
    v[5] = _mm256_sub_epi16(u[4], u[5]);
    v[6] = _mm256_sub_epi16(u[7], u[6]);
    v[7] = _mm256_add_epi16(u[7], u[6]);

    // stage 4
    u[0] = v[0];
    u[1] = v[1];
    u[2] = v[2];
    u[3] = v[3];
    btf_avx2(cospi_p56_p08, cospi_m08_p56, v[4], v[7], bit, &u[4], &u[7]);
    btf_avx2(cospi_p24_p40, cospi_m40_p24, v[5], v[6], bit, &u[5], &u[6]);

    // stage 5
    v[0] = u[0];
    v[1] = u[4];
    v[2] = u[2];
    v[3] = u[6];
    v[4] = u[1];
    v[5] = u[5];
    v[6] = u[3];
    v[7] = u[7];

    //transpose
    u[0] = _mm256_unpacklo_epi16(v[0], v[1]);
    u[1] = _mm256_unpacklo_epi16(v[2], v[3]);
    u[2] = _mm256_unpacklo_epi16(v[4], v[5]);
    u[3] = _mm256_unpacklo_epi16(v[6], v[7]);
    u[4] = _mm256_unpackhi_epi16(v[0], v[1]);
    u[5] = _mm256_unpackhi_epi16(v[2], v[3]);
    u[6] = _mm256_unpackhi_epi16(v[4], v[5]);
    u[7] = _mm256_unpackhi_epi16(v[6], v[7]);
    v[0] = _mm256_unpacklo_epi32(u[0], u[1]);
    v[1] = _mm256_unpacklo_epi32(u[2], u[3]);
    v[2] = _mm256_unpackhi_epi32(u[0], u[1]);
    v[3] = _mm256_unpackhi_epi32(u[2], u[3]);
    v[4] = _mm256_unpacklo_epi32(u[4], u[5]);
    v[5] = _mm256_unpacklo_epi32(u[6], u[7]);
    v[6] = _mm256_unpackhi_epi32(u[4], u[5]);
    v[7] = _mm256_unpackhi_epi32(u[6], u[7]);
    u[0] = _mm256_unpacklo_epi64(v[0], v[1]);
    u[1] = _mm256_unpackhi_epi64(v[0], v[1]);
    u[2] = _mm256_unpacklo_epi64(v[2], v[3]);
    u[3] = _mm256_unpackhi_epi64(v[2], v[3]);
    u[4] = _mm256_unpacklo_epi64(v[4], v[5]);
    u[5] = _mm256_unpackhi_epi64(v[4], v[5]);
    u[6] = _mm256_unpacklo_epi64(v[6], v[7]);
    u[7] = _mm256_unpackhi_epi64(v[6], v[7]);
    __m256i *pout = (__m256i *)out;
    pout[0] = _mm256_permute2x128_si256(u[0], u[1], PERM2x128(0,2));
    pout[1] = _mm256_permute2x128_si256(u[2], u[3], PERM2x128(0,2));
    pout[2] = _mm256_permute2x128_si256(u[4], u[5], PERM2x128(0,2));
    pout[3] = _mm256_permute2x128_si256(u[6], u[7], PERM2x128(0,2));
    pout[4] = _mm256_permute2x128_si256(u[0], u[1], PERM2x128(1,3));
    pout[5] = _mm256_permute2x128_si256(u[2], u[3], PERM2x128(1,3));
    pout[6] = _mm256_permute2x128_si256(u[4], u[5], PERM2x128(1,3));
    pout[7] = _mm256_permute2x128_si256(u[6], u[7], PERM2x128(1,3));

    if (rows) {
        __m256i multiplier = _mm256_set1_epi16((NewSqrt2 - 4096) * 8);
        pout[0] = _mm256_add_epi16(pout[0], _mm256_mulhrs_epi16(pout[0], multiplier));
        pout[1] = _mm256_add_epi16(pout[1], _mm256_mulhrs_epi16(pout[1], multiplier));
        pout[2] = _mm256_add_epi16(pout[2], _mm256_mulhrs_epi16(pout[2], multiplier));
        pout[3] = _mm256_add_epi16(pout[3], _mm256_mulhrs_epi16(pout[3], multiplier));
        pout[4] = _mm256_add_epi16(pout[4], _mm256_mulhrs_epi16(pout[4], multiplier));
        pout[5] = _mm256_add_epi16(pout[5], _mm256_mulhrs_epi16(pout[5], multiplier));
        pout[6] = _mm256_add_epi16(pout[6], _mm256_mulhrs_epi16(pout[6], multiplier));
        pout[7] = _mm256_add_epi16(pout[7], _mm256_mulhrs_epi16(pout[7], multiplier));
    }
}

template <int rows> static void fadst8_16_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = 13;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p16_p48 = mm256_set2_epi16( cospi[16],  cospi[48]);
    const __m256i cospi_p48_m16 = mm256_set2_epi16( cospi[48], -cospi[16]);
    const __m256i cospi_m48_p16 = mm256_set2_epi16(-cospi[48],  cospi[16]);
    const __m256i cospi_p04_p60 = mm256_set2_epi16( cospi[04],  cospi[60]);
    const __m256i cospi_p60_m04 = mm256_set2_epi16( cospi[60], -cospi[04]);
    const __m256i cospi_p20_p44 = mm256_set2_epi16( cospi[20],  cospi[44]);
    const __m256i cospi_p44_m20 = mm256_set2_epi16( cospi[44], -cospi[20]);
    const __m256i cospi_p36_p28 = mm256_set2_epi16( cospi[36],  cospi[28]);
    const __m256i cospi_p28_m36 = mm256_set2_epi16( cospi[28], -cospi[36]);
    const __m256i cospi_p52_p12 = mm256_set2_epi16( cospi[52],  cospi[12]);
    const __m256i cospi_p12_m52 = mm256_set2_epi16( cospi[12], -cospi[52]);
    const __m256i zero = _mm256_setzero_si256();
    __m256i u[8], v[8];

    if (rows) {
        __m256i two = _mm256_set1_epi16(2);
        u[0] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 0 * pitch)), 2);
        u[1] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 1 * pitch)), 2);
        u[2] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 2 * pitch)), 2);
        u[3] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 3 * pitch)), 2);
        u[4] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 4 * pitch)), 2);
        u[5] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 5 * pitch)), 2);
        u[6] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 6 * pitch)), 2);
        u[7] = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 7 * pitch)), 2);
    } else {
        u[0] = _mm256_slli_epi16(loada_si256(in + 0 * pitch), 2);
        u[1] = _mm256_slli_epi16(loada_si256(in + 1 * pitch), 2);
        u[2] = _mm256_slli_epi16(loada_si256(in + 2 * pitch), 2);
        u[3] = _mm256_slli_epi16(loada_si256(in + 3 * pitch), 2);
        u[4] = _mm256_slli_epi16(loada_si256(in + 4 * pitch), 2);
        u[5] = _mm256_slli_epi16(loada_si256(in + 5 * pitch), 2);
        u[6] = _mm256_slli_epi16(loada_si256(in + 6 * pitch), 2);
        u[7] = _mm256_slli_epi16(loada_si256(in + 7 * pitch), 2);
    }

    // stage 1
    v[0] = u[0];
    v[1] = _mm256_sub_epi16(zero, u[7]);
    v[2] = _mm256_sub_epi16(zero, u[3]);
    v[3] = u[4];
    v[4] = _mm256_sub_epi16(zero, u[1]);
    v[5] = u[6];
    v[6] = u[2];
    v[7] = _mm256_sub_epi16(zero, u[5]);

    // stage 2
    u[0] = v[0];
    u[1] = v[1];
    btf_avx2(cospi_p32_p32, cospi_p32_m32, v[2], v[3], bit, &u[2], &u[3]);
    u[4] = v[4];
    u[5] = v[5];
    btf_avx2(cospi_p32_p32, cospi_p32_m32, v[6], v[7], bit, &u[6], &u[7]);

    // stage 3
    v[0] = _mm256_add_epi16(u[0], u[2]);
    v[1] = _mm256_add_epi16(u[1], u[3]);
    v[2] = _mm256_sub_epi16(u[0], u[2]);
    v[3] = _mm256_sub_epi16(u[1], u[3]);
    v[4] = _mm256_add_epi16(u[4], u[6]);
    v[5] = _mm256_add_epi16(u[5], u[7]);
    v[6] = _mm256_sub_epi16(u[4], u[6]);
    v[7] = _mm256_sub_epi16(u[5], u[7]);

    // stage 4
    u[0] = v[0];
    u[1] = v[1];
    u[2] = v[2];
    u[3] = v[3];
    btf_avx2(cospi_p16_p48, cospi_p48_m16, v[4], v[5], bit, &u[4], &u[5]);
    btf_avx2(cospi_m48_p16, cospi_p16_p48, v[6], v[7], bit, &u[6], &u[7]);

    // stage 5
    v[0] = _mm256_add_epi16(u[0], u[4]);
    v[1] = _mm256_add_epi16(u[1], u[5]);
    v[2] = _mm256_add_epi16(u[2], u[6]);
    v[3] = _mm256_add_epi16(u[3], u[7]);
    v[4] = _mm256_sub_epi16(u[0], u[4]);
    v[5] = _mm256_sub_epi16(u[1], u[5]);
    v[6] = _mm256_sub_epi16(u[2], u[6]);
    v[7] = _mm256_sub_epi16(u[3], u[7]);

    // stage 6
    btf_avx2(cospi_p04_p60, cospi_p60_m04, v[0], v[1], bit, &u[0], &u[1]);
    btf_avx2(cospi_p20_p44, cospi_p44_m20, v[2], v[3], bit, &u[2], &u[3]);
    btf_avx2(cospi_p36_p28, cospi_p28_m36, v[4], v[5], bit, &u[4], &u[5]);
    btf_avx2(cospi_p52_p12, cospi_p12_m52, v[6], v[7], bit, &u[6], &u[7]);

    // stage 7
    v[0] = u[1];
    v[1] = u[6];
    v[2] = u[3];
    v[3] = u[4];
    v[4] = u[5];
    v[5] = u[2];
    v[6] = u[7];
    v[7] = u[0];

    //transpose
    u[0] = _mm256_unpacklo_epi16(v[0], v[1]);
    u[1] = _mm256_unpacklo_epi16(v[2], v[3]);
    u[2] = _mm256_unpacklo_epi16(v[4], v[5]);
    u[3] = _mm256_unpacklo_epi16(v[6], v[7]);
    u[4] = _mm256_unpackhi_epi16(v[0], v[1]);
    u[5] = _mm256_unpackhi_epi16(v[2], v[3]);
    u[6] = _mm256_unpackhi_epi16(v[4], v[5]);
    u[7] = _mm256_unpackhi_epi16(v[6], v[7]);
    v[0] = _mm256_unpacklo_epi32(u[0], u[1]);
    v[1] = _mm256_unpacklo_epi32(u[2], u[3]);
    v[2] = _mm256_unpackhi_epi32(u[0], u[1]);
    v[3] = _mm256_unpackhi_epi32(u[2], u[3]);
    v[4] = _mm256_unpacklo_epi32(u[4], u[5]);
    v[5] = _mm256_unpacklo_epi32(u[6], u[7]);
    v[6] = _mm256_unpackhi_epi32(u[4], u[5]);
    v[7] = _mm256_unpackhi_epi32(u[6], u[7]);
    u[0] = _mm256_unpacklo_epi64(v[0], v[1]);
    u[1] = _mm256_unpackhi_epi64(v[0], v[1]);
    u[2] = _mm256_unpacklo_epi64(v[2], v[3]);
    u[3] = _mm256_unpackhi_epi64(v[2], v[3]);
    u[4] = _mm256_unpacklo_epi64(v[4], v[5]);
    u[5] = _mm256_unpackhi_epi64(v[4], v[5]);
    u[6] = _mm256_unpacklo_epi64(v[6], v[7]);
    u[7] = _mm256_unpackhi_epi64(v[6], v[7]);
    __m256i *pout = (__m256i *)out;
    pout[0] = _mm256_permute2x128_si256(u[0], u[1], PERM2x128(0,2));
    pout[1] = _mm256_permute2x128_si256(u[2], u[3], PERM2x128(0,2));
    pout[2] = _mm256_permute2x128_si256(u[4], u[5], PERM2x128(0,2));
    pout[3] = _mm256_permute2x128_si256(u[6], u[7], PERM2x128(0,2));
    pout[4] = _mm256_permute2x128_si256(u[0], u[1], PERM2x128(1,3));
    pout[5] = _mm256_permute2x128_si256(u[2], u[3], PERM2x128(1,3));
    pout[6] = _mm256_permute2x128_si256(u[4], u[5], PERM2x128(1,3));
    pout[7] = _mm256_permute2x128_si256(u[6], u[7], PERM2x128(1,3));

    if (rows) {
        __m256i multiplier = _mm256_set1_epi16((NewSqrt2 - 4096) * 8);
        pout[0] = _mm256_add_epi16(pout[0], _mm256_mulhrs_epi16(pout[0], multiplier));
        pout[1] = _mm256_add_epi16(pout[1], _mm256_mulhrs_epi16(pout[1], multiplier));
        pout[2] = _mm256_add_epi16(pout[2], _mm256_mulhrs_epi16(pout[2], multiplier));
        pout[3] = _mm256_add_epi16(pout[3], _mm256_mulhrs_epi16(pout[3], multiplier));
        pout[4] = _mm256_add_epi16(pout[4], _mm256_mulhrs_epi16(pout[4], multiplier));
        pout[5] = _mm256_add_epi16(pout[5], _mm256_mulhrs_epi16(pout[5], multiplier));
        pout[6] = _mm256_add_epi16(pout[6], _mm256_mulhrs_epi16(pout[6], multiplier));
        pout[7] = _mm256_add_epi16(pout[7], _mm256_mulhrs_epi16(pout[7], multiplier));
    }
}

template <int rows> static void fdct16_8_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = 13;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_m32_p32 = mm256_set2_epi16(-cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p48_p16 = mm256_set2_epi16( cospi[48],  cospi[16]);
    const __m256i cospi_m16_p48 = mm256_set2_epi16(-cospi[16],  cospi[48]);
    const __m256i cospi_m48_m16 = mm256_set2_epi16(-cospi[48], -cospi[16]);
    const __m256i cospi_p56_p08 = mm256_set2_epi16( cospi[56],  cospi[ 8]);
    const __m256i cospi_m08_p56 = mm256_set2_epi16(-cospi[ 8],  cospi[56]);
    const __m256i cospi_p24_p40 = mm256_set2_epi16( cospi[24],  cospi[40]);
    const __m256i cospi_m40_p24 = mm256_set2_epi16(-cospi[40],  cospi[24]);
    const __m256i cospi_p60_p04 = mm256_set2_epi16( cospi[60],  cospi[ 4]);
    const __m256i cospi_m04_p60 = mm256_set2_epi16(-cospi[ 4],  cospi[60]);
    const __m256i cospi_p28_p36 = mm256_set2_epi16( cospi[28],  cospi[36]);
    const __m256i cospi_m36_p28 = mm256_set2_epi16(-cospi[36],  cospi[28]);
    const __m256i cospi_p44_p20 = mm256_set2_epi16( cospi[44],  cospi[20]);
    const __m256i cospi_m20_p44 = mm256_set2_epi16(-cospi[20],  cospi[44]);
    const __m256i cospi_p12_p52 = mm256_set2_epi16( cospi[12],  cospi[52]);
    const __m256i cospi_m52_p12 = mm256_set2_epi16(-cospi[52],  cospi[12]);

    __m256i a_00_01, a_02_03, a_04_05, a_06_07, a_09_08, a_11_10, a_13_12, a_15_14;

    if (rows) {
        __m256i two = _mm256_set1_epi16(2);
        a_00_01 = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 0 * 8)), 2);
        a_02_03 = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 2 * 8)), 2);
        a_04_05 = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 4 * 8)), 2);
        a_06_07 = _mm256_srai_epi16(_mm256_add_epi16(two, loada_si256(in + 6 * 8)), 2);
        a_09_08 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in +  9 * 8, in +  8 * 8)), 2);
        a_11_10 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 11 * 8, in + 10 * 8)), 2);
        a_13_12 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 13 * 8, in + 12 * 8)), 2);
        a_15_14 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 15 * 8, in + 14 * 8)), 2);
    } else {
        a_00_01 = _mm256_slli_epi16(loada2_m128i(in +  0 * pitch, in +  1 * pitch), 2);
        a_02_03 = _mm256_slli_epi16(loada2_m128i(in +  2 * pitch, in +  3 * pitch), 2);
        a_04_05 = _mm256_slli_epi16(loada2_m128i(in +  4 * pitch, in +  5 * pitch), 2);
        a_06_07 = _mm256_slli_epi16(loada2_m128i(in +  6 * pitch, in +  7 * pitch), 2);
        a_09_08 = _mm256_slli_epi16(loada2_m128i(in +  9 * pitch, in +  8 * pitch), 2);
        a_11_10 = _mm256_slli_epi16(loada2_m128i(in + 11 * pitch, in + 10 * pitch), 2);
        a_13_12 = _mm256_slli_epi16(loada2_m128i(in + 13 * pitch, in + 12 * pitch), 2);
        a_15_14 = _mm256_slli_epi16(loada2_m128i(in + 15 * pitch, in + 14 * pitch), 2);
    }

    // stage 1
    __m256i b_00_01 = _mm256_add_epi16(a_00_01, a_15_14);
    __m256i b_02_03 = _mm256_add_epi16(a_02_03, a_13_12);
    __m256i b_04_05 = _mm256_add_epi16(a_04_05, a_11_10);
    __m256i b_06_07 = _mm256_add_epi16(a_06_07, a_09_08);
    __m256i b_09_08 = _mm256_sub_epi16(a_06_07, a_09_08);
    __m256i b_11_10 = _mm256_sub_epi16(a_04_05, a_11_10);
    __m256i b_13_12 = _mm256_sub_epi16(a_02_03, a_13_12);
    __m256i b_15_14 = _mm256_sub_epi16(a_00_01, a_15_14);

    // stage 2
    __m256i b_07_06 = _mm256_permute4x64_epi64(b_06_07, PERM4x64(2,3,0,1));
    __m256i b_05_04 = _mm256_permute4x64_epi64(b_04_05, PERM4x64(2,3,0,1));
    __m256i b_10_11 = _mm256_permute4x64_epi64(b_11_10, PERM4x64(2,3,0,1));
    __m256i c_00_01 = _mm256_add_epi16(b_00_01, b_07_06);
    __m256i c_02_03 = _mm256_add_epi16(b_02_03, b_05_04);
    __m256i c_05_04 = _mm256_sub_epi16(b_02_03, b_05_04);
    __m256i c_07_06 = _mm256_sub_epi16(b_00_01, b_07_06);
    __m256i c_09_08 = b_09_08;
    __m256i c_15_14 = b_15_14;

    __m256i tmp0 = _mm256_unpacklo_epi16(b_10_11, b_13_12);
    __m256i tmp1 = _mm256_unpackhi_epi16(b_10_11, b_13_12);
    __m256i c_10_13 = btf16_avx2(cospi_m32_p32, cospi_p32_p32,
                                 _mm256_permute2x128_si256(tmp0, tmp1, PERM2x128(0,2)), bit);
    __m256i c_11_12 = btf16_avx2(cospi_m32_p32, cospi_p32_p32,
                                 _mm256_permute2x128_si256(tmp0, tmp1, PERM2x128(1,3)), bit);

    // stage 3
    __m256i c_03_02 = _mm256_permute4x64_epi64(c_02_03, PERM4x64(2,3,0,1));
    __m256i d_00_01 = _mm256_add_epi16(c_00_01, c_03_02);
    __m256i d_03_02 = _mm256_sub_epi16(c_00_01, c_03_02);
    __m256i d_04_07 = _mm256_permute2x128_si256(c_05_04, c_07_06, PERM2x128(1,2));
    __m256i d_05_06 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(c_05_04, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(c_07_06, PERM4x64(2,2,3,3)));
    d_05_06 = btf16_avx2(cospi_m32_p32, cospi_p32_p32, d_05_06, bit);
    __m256i c_10_11 = _mm256_permute2x128_si256(c_10_13, c_11_12, PERM2x128(0,2));
    __m256i c_12_13 = _mm256_permute2x128_si256(c_10_13, c_11_12, PERM2x128(3,1));
    __m256i d_09_08 = _mm256_add_epi16(c_09_08, c_10_11);
    __m256i d_10_11 = _mm256_sub_epi16(c_09_08, c_10_11);
    __m256i d_12_13 = _mm256_sub_epi16(c_15_14, c_12_13);
    __m256i d_15_14 = _mm256_add_epi16(c_15_14, c_12_13);

    // stage 4
    __m256i e_04_07 = _mm256_add_epi16(d_04_07, d_05_06);
    __m256i e_05_06 = _mm256_sub_epi16(d_04_07, d_05_06);
    __m256i e_08_11 = _mm256_permute2x128_si256(d_09_08, d_10_11, PERM2x128(1,3));
    __m256i e_12_15 = _mm256_permute2x128_si256(d_12_13, d_15_14, PERM2x128(0,2));
    __m256i e_00_01 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d_00_01, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(d_00_01, PERM4x64(2,2,3,3)));
    __m256i e_02_03 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d_03_02, PERM4x64(2,2,3,3)),
                                            _mm256_permute4x64_epi64(d_03_02, PERM4x64(0,0,1,1)));
    __m256i e_09_14 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d_09_08, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(d_15_14, PERM4x64(2,2,3,3)));
    __m256i e_10_13 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d_10_11, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(d_12_13, PERM4x64(2,2,3,3)));
    e_00_01 = btf16_avx2(cospi_p32_p32, cospi_p32_m32, e_00_01, bit);
    e_02_03 = btf16_avx2(cospi_p48_p16, cospi_m16_p48, e_02_03, bit);
    e_09_14 = btf16_avx2(cospi_m16_p48, cospi_p48_p16, e_09_14, bit);
    e_10_13 = btf16_avx2(cospi_m48_m16, cospi_m16_p48, e_10_13, bit);

    // stage 5
    __m256i e_09_10 = _mm256_permute2x128_si256(e_09_14, e_10_13, PERM2x128(0,2));
    __m256i e_13_14 = _mm256_permute2x128_si256(e_09_14, e_10_13, PERM2x128(3,1));
    __m256i f_00_01 = e_00_01;
    __m256i f_02_03 = e_02_03;
    __m256i f_08_11 = _mm256_add_epi16(e_08_11, e_09_10);
    __m256i f_09_10 = _mm256_sub_epi16(e_08_11, e_09_10);
    __m256i f_12_15 = _mm256_add_epi16(e_12_15, e_13_14);
    __m256i f_13_14 = _mm256_sub_epi16(e_12_15, e_13_14);
    __m256i f_04_07 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(e_04_07, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(e_04_07, PERM4x64(2,2,3,3)));
    __m256i f_05_06 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(e_05_06, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(e_05_06, PERM4x64(2,2,3,3)));
    f_04_07 = btf16_avx2(cospi_p56_p08, cospi_m08_p56, f_04_07, bit);
    f_05_06 = btf16_avx2(cospi_p24_p40, cospi_m40_p24, f_05_06, bit);

    // stage 6
    __m256i g_00_01 = f_00_01;
    __m256i g_02_03 = f_02_03;
    __m256i g_04_07 = f_04_07;
    __m256i g_05_06 = f_05_06;
    __m256i g_08_15 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f_08_11, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(f_12_15, PERM4x64(2,2,3,3)));
    __m256i g_09_14 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f_09_10, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(f_13_14, PERM4x64(2,2,3,3)));
    __m256i g_10_13 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f_09_10, PERM4x64(2,2,3,3)),
                                            _mm256_permute4x64_epi64(f_13_14, PERM4x64(0,0,1,1)));
    __m256i g_11_12 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f_08_11, PERM4x64(2,2,3,3)),
                                            _mm256_permute4x64_epi64(f_12_15, PERM4x64(0,0,1,1)));
    g_08_15 = btf16_avx2(cospi_p60_p04, cospi_m04_p60, g_08_15, bit);
    g_09_14 = btf16_avx2(cospi_p28_p36, cospi_m36_p28, g_09_14, bit);
    g_10_13 = btf16_avx2(cospi_p44_p20, cospi_m20_p44, g_10_13, bit);
    g_11_12 = btf16_avx2(cospi_p12_p52, cospi_m52_p12, g_11_12, bit);

    // stage 7
    __m256i u[8], v[8], *pout = (__m256i *)out;
    u[0] = g_00_01;                                                      // row0 row8
    u[1] = _mm256_permute2x128_si256(g_08_15, g_09_14, PERM2x128(0,2));  // row1 row9
    u[2] = _mm256_permute2x128_si256(g_04_07, g_05_06, PERM2x128(0,2));  // row2 row10
    u[3] = _mm256_permute2x128_si256(g_11_12, g_10_13, PERM2x128(1,3));  // row3 row11
    u[4] = g_02_03;                                                      // row4 row12
    u[5] = _mm256_permute2x128_si256(g_10_13, g_11_12, PERM2x128(0,2));  // row5 row13
    u[6] = _mm256_permute2x128_si256(g_05_06, g_04_07, PERM2x128(1,3));  // row6 row14
    u[7] = _mm256_permute2x128_si256(g_09_14, g_08_15, PERM2x128(1,3));  // row7 row15

    // transpose
    v[0] = _mm256_unpacklo_epi16(u[0], u[1]);
    v[1] = _mm256_unpacklo_epi16(u[2], u[3]);
    v[2] = _mm256_unpacklo_epi16(u[4], u[5]);
    v[3] = _mm256_unpacklo_epi16(u[6], u[7]);
    v[4] = _mm256_unpackhi_epi16(u[0], u[1]);
    v[5] = _mm256_unpackhi_epi16(u[2], u[3]);
    v[6] = _mm256_unpackhi_epi16(u[4], u[5]);
    v[7] = _mm256_unpackhi_epi16(u[6], u[7]);
    u[0] = _mm256_unpacklo_epi32(v[0], v[1]);
    u[1] = _mm256_unpacklo_epi32(v[2], v[3]);
    u[2] = _mm256_unpackhi_epi32(v[0], v[1]);
    u[3] = _mm256_unpackhi_epi32(v[2], v[3]);
    u[4] = _mm256_unpacklo_epi32(v[4], v[5]);
    u[5] = _mm256_unpacklo_epi32(v[6], v[7]);
    u[6] = _mm256_unpackhi_epi32(v[4], v[5]);
    u[7] = _mm256_unpackhi_epi32(v[6], v[7]);
    pout[0] = _mm256_unpacklo_epi64(u[0], u[1]);
    pout[1] = _mm256_unpackhi_epi64(u[0], u[1]);
    pout[2] = _mm256_unpacklo_epi64(u[2], u[3]);
    pout[3] = _mm256_unpackhi_epi64(u[2], u[3]);
    pout[4] = _mm256_unpacklo_epi64(u[4], u[5]);
    pout[5] = _mm256_unpackhi_epi64(u[4], u[5]);
    pout[6] = _mm256_unpacklo_epi64(u[6], u[7]);
    pout[7] = _mm256_unpackhi_epi64(u[6], u[7]);

    if (rows) {
        __m256i multiplier = _mm256_set1_epi16((NewSqrt2 - 4096) * 8);
        pout[0] = _mm256_add_epi16(pout[0], _mm256_mulhrs_epi16(pout[0], multiplier));
        pout[1] = _mm256_add_epi16(pout[1], _mm256_mulhrs_epi16(pout[1], multiplier));
        pout[2] = _mm256_add_epi16(pout[2], _mm256_mulhrs_epi16(pout[2], multiplier));
        pout[3] = _mm256_add_epi16(pout[3], _mm256_mulhrs_epi16(pout[3], multiplier));
        pout[4] = _mm256_add_epi16(pout[4], _mm256_mulhrs_epi16(pout[4], multiplier));
        pout[5] = _mm256_add_epi16(pout[5], _mm256_mulhrs_epi16(pout[5], multiplier));
        pout[6] = _mm256_add_epi16(pout[6], _mm256_mulhrs_epi16(pout[6], multiplier));
        pout[7] = _mm256_add_epi16(pout[7], _mm256_mulhrs_epi16(pout[7], multiplier));
    }
}

template <int rows> static void fadst16_8_avx2_16s(const short *in, int pitch, short *out)
{
    const int bit = 13;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32], cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32],-cospi[32]);
    const __m256i cospi_p16_p48 = mm256_set2_epi16( cospi[16], cospi[48]);
    const __m256i cospi_p48_m16 = mm256_set2_epi16( cospi[48],-cospi[16]);
    const __m256i cospi_m48_p16 = mm256_set2_epi16(-cospi[48], cospi[16]);
    const __m256i cospi_p08_p56 = mm256_set2_epi16( cospi[ 8], cospi[56]);
    const __m256i cospi_p56_m08 = mm256_set2_epi16( cospi[56],-cospi[ 8]);
    const __m256i cospi_p40_p24 = mm256_set2_epi16( cospi[40], cospi[24]);
    const __m256i cospi_p24_m40 = mm256_set2_epi16( cospi[24],-cospi[40]);
    const __m256i cospi_m56_p08 = mm256_set2_epi16(-cospi[56], cospi[ 8]);
    const __m256i cospi_m24_p40 = mm256_set2_epi16(-cospi[24], cospi[40]);
    const __m256i cospi_p02_p62 = mm256_set2_epi16( cospi[ 2], cospi[62]);
    const __m256i cospi_p62_m02 = mm256_set2_epi16( cospi[62],-cospi[ 2]);
    const __m256i cospi_p10_p54 = mm256_set2_epi16( cospi[10], cospi[54]);
    const __m256i cospi_p54_m10 = mm256_set2_epi16( cospi[54],-cospi[10]);
    const __m256i cospi_p18_p46 = mm256_set2_epi16( cospi[18], cospi[46]);
    const __m256i cospi_p46_m18 = mm256_set2_epi16( cospi[46],-cospi[18]);
    const __m256i cospi_p26_p38 = mm256_set2_epi16( cospi[26], cospi[38]);
    const __m256i cospi_p38_m26 = mm256_set2_epi16( cospi[38],-cospi[26]);
    const __m256i cospi_p34_p30 = mm256_set2_epi16( cospi[34], cospi[30]);
    const __m256i cospi_p30_m34 = mm256_set2_epi16( cospi[30],-cospi[34]);
    const __m256i cospi_p42_p22 = mm256_set2_epi16( cospi[42], cospi[22]);
    const __m256i cospi_p22_m42 = mm256_set2_epi16( cospi[22],-cospi[42]);
    const __m256i cospi_p50_p14 = mm256_set2_epi16( cospi[50], cospi[14]);
    const __m256i cospi_p14_m50 = mm256_set2_epi16( cospi[14],-cospi[50]);
    const __m256i cospi_p58_p06 = mm256_set2_epi16( cospi[58], cospi[ 6]);
    const __m256i cospi_p06_m58 = mm256_set2_epi16( cospi[ 6],-cospi[58]);

    __m256i a_00_15, a_07_08, a_03_12, a_04_11, a_01_14, a_06_09, a_02_13, a_05_10;

    if (rows) {
        __m256i two = _mm256_set1_epi16(2);
        a_00_15 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 0 * pitch, in + 15 * pitch)), 2);
        a_07_08 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 7 * pitch, in +  8 * pitch)), 2);
        a_03_12 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 3 * pitch, in + 12 * pitch)), 2);
        a_04_11 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 4 * pitch, in + 11 * pitch)), 2);
        a_01_14 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 1 * pitch, in + 14 * pitch)), 2);
        a_06_09 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 6 * pitch, in +  9 * pitch)), 2);
        a_02_13 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 2 * pitch, in + 13 * pitch)), 2);
        a_05_10 = _mm256_srai_epi16(_mm256_add_epi16(two, loada2_m128i(in + 5 * pitch, in + 10 * pitch)), 2);
    } else {
        a_00_15 = _mm256_slli_epi16(loada2_m128i(in + 0 * pitch, in + 15 * pitch), 2);
        a_07_08 = _mm256_slli_epi16(loada2_m128i(in + 7 * pitch, in +  8 * pitch), 2);
        a_03_12 = _mm256_slli_epi16(loada2_m128i(in + 3 * pitch, in + 12 * pitch), 2);
        a_04_11 = _mm256_slli_epi16(loada2_m128i(in + 4 * pitch, in + 11 * pitch), 2);
        a_01_14 = _mm256_slli_epi16(loada2_m128i(in + 1 * pitch, in + 14 * pitch), 2);
        a_06_09 = _mm256_slli_epi16(loada2_m128i(in + 6 * pitch, in +  9 * pitch), 2);
        a_02_13 = _mm256_slli_epi16(loada2_m128i(in + 2 * pitch, in + 13 * pitch), 2);
        a_05_10 = _mm256_slli_epi16(loada2_m128i(in + 5 * pitch, in + 10 * pitch), 2);
    }

    // stage 1
    const __m256i zero = _mm256_setzero_si256();
    const __m128i ones = _mm_cmpeq_epi16(_mm_setzero_si128(), _mm_setzero_si128());
    const __m256i neg_lo = _mm256_inserti128_si256(zero, ones, 0);
    const __m256i neg_hi = _mm256_inserti128_si256(zero, ones, 1);
    __m256i b_00_01 = _mm256_sub_epi16(_mm256_xor_si256(a_00_15, neg_hi), neg_hi);
    __m256i b_02_03 = _mm256_sub_epi16(_mm256_xor_si256(a_07_08, neg_lo), neg_lo);
    __m256i b_04_05 = _mm256_sub_epi16(_mm256_xor_si256(a_03_12, neg_lo), neg_lo);
    __m256i b_06_07 = _mm256_sub_epi16(_mm256_xor_si256(a_04_11, neg_hi), neg_hi);
    __m256i b_08_09 = _mm256_sub_epi16(_mm256_xor_si256(a_01_14, neg_lo), neg_lo);
    __m256i b_10_11 = _mm256_sub_epi16(_mm256_xor_si256(a_06_09, neg_hi), neg_hi);
    __m256i b_12_13 = _mm256_sub_epi16(_mm256_xor_si256(a_02_13, neg_hi), neg_hi);
    __m256i b_14_15 = _mm256_sub_epi16(_mm256_xor_si256(a_05_10, neg_lo), neg_lo);

    // stage 2
    __m256i c_00_01 = b_00_01;
    __m256i c_04_05 = b_04_05;
    __m256i c_08_09 = b_08_09;
    __m256i c_12_13 = b_12_13;
    __m256i c_02_03 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(b_02_03, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(b_02_03, PERM4x64(2,2,3,3)));
    __m256i c_06_07 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(b_06_07, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(b_06_07, PERM4x64(2,2,3,3)));
    __m256i c_10_11 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(b_10_11, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(b_10_11, PERM4x64(2,2,3,3)));
    __m256i c_14_15 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(b_14_15, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(b_14_15, PERM4x64(2,2,3,3)));
    c_02_03 =  btf16_avx2(cospi_p32_p32, cospi_p32_m32, c_02_03, bit);
    c_06_07 =  btf16_avx2(cospi_p32_p32, cospi_p32_m32, c_06_07, bit);
    c_10_11 =  btf16_avx2(cospi_p32_p32, cospi_p32_m32, c_10_11, bit);
    c_14_15 =  btf16_avx2(cospi_p32_p32, cospi_p32_m32, c_14_15, bit);

    // stage 3
    __m256i d_00_01 = _mm256_add_epi16(c_00_01, c_02_03);
    __m256i d_02_03 = _mm256_sub_epi16(c_00_01, c_02_03);
    __m256i d_04_05 = _mm256_add_epi16(c_04_05, c_06_07);
    __m256i d_06_07 = _mm256_sub_epi16(c_04_05, c_06_07);
    __m256i d_08_09 = _mm256_add_epi16(c_08_09, c_10_11);
    __m256i d_10_11 = _mm256_sub_epi16(c_08_09, c_10_11);
    __m256i d_12_13 = _mm256_add_epi16(c_12_13, c_14_15);
    __m256i d_14_15 = _mm256_sub_epi16(c_12_13, c_14_15);

    // stage 4
    __m256i e_00_01 = d_00_01;
    __m256i e_02_03 = d_02_03;
    __m256i e_08_09 = d_08_09;
    __m256i e_10_11 = d_10_11;
    __m256i e_04_05 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d_04_05, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(d_04_05, PERM4x64(2,2,3,3)));
    __m256i e_06_07 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d_06_07, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(d_06_07, PERM4x64(2,2,3,3)));
    __m256i e_12_13 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d_12_13, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(d_12_13, PERM4x64(2,2,3,3)));
    __m256i e_14_15 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d_14_15, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(d_14_15, PERM4x64(2,2,3,3)));
    e_04_05 = btf16_avx2(cospi_p16_p48, cospi_p48_m16, e_04_05, bit);
    e_06_07 = btf16_avx2(cospi_m48_p16, cospi_p16_p48, e_06_07, bit);
    e_12_13 = btf16_avx2(cospi_p16_p48, cospi_p48_m16, e_12_13, bit);
    e_14_15 = btf16_avx2(cospi_m48_p16, cospi_p16_p48, e_14_15, bit);

    // stage 5
    __m256i f_00_01 = _mm256_add_epi16(e_00_01, e_04_05);
    __m256i f_02_03 = _mm256_add_epi16(e_02_03, e_06_07);
    __m256i f_04_05 = _mm256_sub_epi16(e_00_01, e_04_05);
    __m256i f_06_07 = _mm256_sub_epi16(e_02_03, e_06_07);
    __m256i f_08_09 = _mm256_add_epi16(e_08_09, e_12_13);
    __m256i f_10_11 = _mm256_add_epi16(e_10_11, e_14_15);
    __m256i f_12_13 = _mm256_sub_epi16(e_08_09, e_12_13);
    __m256i f_14_15 = _mm256_sub_epi16(e_10_11, e_14_15);

    // stage 6
    __m256i g_00_01 = f_00_01;
    __m256i g_02_03 = f_02_03;
    __m256i g_04_05 = f_04_05;
    __m256i g_06_07 = f_06_07;
    __m256i g_08_09 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f_08_09, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(f_08_09, PERM4x64(2,2,3,3)));
    __m256i g_10_11 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f_10_11, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(f_10_11, PERM4x64(2,2,3,3)));
    __m256i g_12_13 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f_12_13, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(f_12_13, PERM4x64(2,2,3,3)));
    __m256i g_14_15 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f_14_15, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(f_14_15, PERM4x64(2,2,3,3)));
    g_08_09 = btf16_avx2(cospi_p08_p56, cospi_p56_m08, g_08_09, bit);
    g_10_11 = btf16_avx2(cospi_p40_p24, cospi_p24_m40, g_10_11, bit);
    g_12_13 = btf16_avx2(cospi_m56_p08, cospi_p08_p56, g_12_13, bit);
    g_14_15 = btf16_avx2(cospi_m24_p40, cospi_p40_p24, g_14_15, bit);

    // stage 7
    __m256i h_00_01 = _mm256_add_epi16(g_00_01, g_08_09);
    __m256i h_02_03 = _mm256_add_epi16(g_02_03, g_10_11);
    __m256i h_04_05 = _mm256_add_epi16(g_04_05, g_12_13);
    __m256i h_06_07 = _mm256_add_epi16(g_06_07, g_14_15);
    __m256i h_08_09 = _mm256_sub_epi16(g_00_01, g_08_09);
    __m256i h_10_11 = _mm256_sub_epi16(g_02_03, g_10_11);
    __m256i h_12_13 = _mm256_sub_epi16(g_04_05, g_12_13);
    __m256i h_14_15 = _mm256_sub_epi16(g_06_07, g_14_15);

    // stage 8
    __m256i i_00_01 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(h_00_01, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(h_00_01, PERM4x64(2,2,3,3)));
    __m256i i_02_03 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(h_02_03, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(h_02_03, PERM4x64(2,2,3,3)));
    __m256i i_04_05 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(h_04_05, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(h_04_05, PERM4x64(2,2,3,3)));
    __m256i i_06_07 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(h_06_07, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(h_06_07, PERM4x64(2,2,3,3)));
    __m256i i_08_09 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(h_08_09, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(h_08_09, PERM4x64(2,2,3,3)));
    __m256i i_10_11 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(h_10_11, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(h_10_11, PERM4x64(2,2,3,3)));
    __m256i i_12_13 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(h_12_13, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(h_12_13, PERM4x64(2,2,3,3)));
    __m256i i_14_15 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(h_14_15, PERM4x64(0,0,1,1)),
                                            _mm256_permute4x64_epi64(h_14_15, PERM4x64(2,2,3,3)));
    i_00_01 = btf16_avx2(cospi_p02_p62, cospi_p62_m02, i_00_01, bit);
    i_02_03 = btf16_avx2(cospi_p10_p54, cospi_p54_m10, i_02_03, bit);
    i_04_05 = btf16_avx2(cospi_p18_p46, cospi_p46_m18, i_04_05, bit);
    i_06_07 = btf16_avx2(cospi_p26_p38, cospi_p38_m26, i_06_07, bit);
    i_08_09 = btf16_avx2(cospi_p34_p30, cospi_p30_m34, i_08_09, bit);
    i_10_11 = btf16_avx2(cospi_p42_p22, cospi_p22_m42, i_10_11, bit);
    i_12_13 = btf16_avx2(cospi_p50_p14, cospi_p14_m50, i_12_13, bit);
    i_14_15 = btf16_avx2(cospi_p58_p06, cospi_p06_m58, i_14_15, bit);

    // stage 9
    __m256i u[8], v[8], *pout = (__m256i *)out;
    u[0] = _mm256_permute2x128_si256(i_00_01, i_08_09, PERM2x128(1,3));  // row0 row8
    u[1] = _mm256_permute2x128_si256(i_14_15, i_06_07, PERM2x128(0,2));  // row1 row9
    u[2] = _mm256_permute2x128_si256(i_02_03, i_10_11, PERM2x128(1,3));  // row2 row10
    u[3] = _mm256_permute2x128_si256(i_12_13, i_04_05, PERM2x128(0,2));  // row3 row11
    u[4] = _mm256_permute2x128_si256(i_04_05, i_12_13, PERM2x128(1,3));  // row4 row12
    u[5] = _mm256_permute2x128_si256(i_10_11, i_02_03, PERM2x128(0,2));  // row5 row13
    u[6] = _mm256_permute2x128_si256(i_06_07, i_14_15, PERM2x128(1,3));  // row6 row14
    u[7] = _mm256_permute2x128_si256(i_08_09, i_00_01, PERM2x128(0,2));  // row7 row15

    // transpose
    v[0] = _mm256_unpacklo_epi16(u[0], u[1]);
    v[1] = _mm256_unpacklo_epi16(u[2], u[3]);
    v[2] = _mm256_unpacklo_epi16(u[4], u[5]);
    v[3] = _mm256_unpacklo_epi16(u[6], u[7]);
    v[4] = _mm256_unpackhi_epi16(u[0], u[1]);
    v[5] = _mm256_unpackhi_epi16(u[2], u[3]);
    v[6] = _mm256_unpackhi_epi16(u[4], u[5]);
    v[7] = _mm256_unpackhi_epi16(u[6], u[7]);
    u[0] = _mm256_unpacklo_epi32(v[0], v[1]);
    u[1] = _mm256_unpacklo_epi32(v[2], v[3]);
    u[2] = _mm256_unpackhi_epi32(v[0], v[1]);
    u[3] = _mm256_unpackhi_epi32(v[2], v[3]);
    u[4] = _mm256_unpacklo_epi32(v[4], v[5]);
    u[5] = _mm256_unpacklo_epi32(v[6], v[7]);
    u[6] = _mm256_unpackhi_epi32(v[4], v[5]);
    u[7] = _mm256_unpackhi_epi32(v[6], v[7]);
    pout[0] = _mm256_unpacklo_epi64(u[0], u[1]);
    pout[1] = _mm256_unpackhi_epi64(u[0], u[1]);
    pout[2] = _mm256_unpacklo_epi64(u[2], u[3]);
    pout[3] = _mm256_unpackhi_epi64(u[2], u[3]);
    pout[4] = _mm256_unpacklo_epi64(u[4], u[5]);
    pout[5] = _mm256_unpackhi_epi64(u[4], u[5]);
    pout[6] = _mm256_unpacklo_epi64(u[6], u[7]);
    pout[7] = _mm256_unpackhi_epi64(u[6], u[7]);

    if (rows) {
        __m256i multiplier = _mm256_set1_epi16((NewSqrt2 - 4096) * 8);
        pout[0] = _mm256_add_epi16(pout[0], _mm256_mulhrs_epi16(pout[0], multiplier));
        pout[1] = _mm256_add_epi16(pout[1], _mm256_mulhrs_epi16(pout[1], multiplier));
        pout[2] = _mm256_add_epi16(pout[2], _mm256_mulhrs_epi16(pout[2], multiplier));
        pout[3] = _mm256_add_epi16(pout[3], _mm256_mulhrs_epi16(pout[3], multiplier));
        pout[4] = _mm256_add_epi16(pout[4], _mm256_mulhrs_epi16(pout[4], multiplier));
        pout[5] = _mm256_add_epi16(pout[5], _mm256_mulhrs_epi16(pout[5], multiplier));
        pout[6] = _mm256_add_epi16(pout[6], _mm256_mulhrs_epi16(pout[6], multiplier));
        pout[7] = _mm256_add_epi16(pout[7], _mm256_mulhrs_epi16(pout[7], multiplier));
    }
}

static void ftransform_4x8_avx2(const short *input, short *coeff, int stride, TxType tx_type)
{
    assert((size_t(input) & 7) == 0);
    assert((size_t(coeff) & 31) == 0);
    assert(fwd_txfm_shift_ls[TX_4X8][0] == 2);
    assert(fwd_txfm_shift_ls[TX_4X8][1] == -1);
    assert(fwd_txfm_shift_ls[TX_4X8][2] == 0);
    assert(fwd_cos_bit_col[TX_4X4][TX_8X8] == 13);
    assert(fwd_cos_bit_row[TX_4X4][TX_8X8] == 13);

    alignas(32) short intermediate[4 * 8];

    if (tx_type == DCT_DCT) {
        fdct8_4_avx2_16s<0>(input, stride, intermediate);
        fdct4_8_avx2_16s<1>(intermediate, 8, coeff);
    } else if (tx_type == ADST_DCT) {
        fadst8_4_avx2_16s<0>(input, stride, intermediate);
        fdct4_8_avx2_16s <1>(intermediate, 8, coeff);
    } else if (tx_type == DCT_ADST) {
        fdct8_4_avx2_16s <0>(input, stride, intermediate);
        fadst4_8_avx2_16s<1>(intermediate, 8, coeff);
    } else { assert(tx_type == ADST_ADST);
        fadst8_4_avx2_16s<0>(input, stride, intermediate);
        fadst4_8_avx2_16s<1>(intermediate, 8, coeff);
    }
}

static void ftransform_8x4_avx2(const short *input, short *coeff, int stride, TxType tx_type)
{
    assert((size_t(input) & 7) == 0);
    assert((size_t(coeff) & 31) == 0);
    assert(fwd_txfm_shift_ls[TX_8X4][0] == 2);
    assert(fwd_txfm_shift_ls[TX_8X4][1] == -1);
    assert(fwd_txfm_shift_ls[TX_8X4][2] == 0);
    assert(fwd_cos_bit_col[TX_8X8][TX_4X4] == 13);
    assert(fwd_cos_bit_row[TX_8X8][TX_4X4] == 13);

    alignas(32) short intermediate[8 * 4];

    if (tx_type == DCT_DCT) {
        fdct4_8_avx2_16s<0>(input, stride, intermediate);
        fdct8_4_avx2_16s<1>(intermediate, 4, coeff);
    } else if (tx_type == ADST_DCT) {
        fadst4_8_avx2_16s<0>(input, stride, intermediate);
        fdct8_4_avx2_16s <1>(intermediate, 4, coeff);
    } else if (tx_type == DCT_ADST) {
        fdct4_8_avx2_16s <0>(input, stride, intermediate);
        fadst8_4_avx2_16s<1>(intermediate, 4, coeff);
    } else { assert(tx_type == ADST_ADST);
        fadst4_8_avx2_16s<0>(input, stride, intermediate);
        fadst8_4_avx2_16s<1>(intermediate, 4, coeff);
    }
}

static void ftransform_8x16_avx2(const short *input, short *coeff, int stride, TxType tx_type)
{
    assert((size_t(input) & 31) == 0);
    assert((size_t(coeff) & 31) == 0);
    assert(fwd_txfm_shift_ls[TX_8X16][0] == 2);
    assert(fwd_txfm_shift_ls[TX_8X16][1] == -2);
    assert(fwd_txfm_shift_ls[TX_8X16][2] == 0);
    assert(fwd_cos_bit_col[TX_8X8][TX_16X16] == 13);
    assert(fwd_cos_bit_row[TX_8X8][TX_16X16] == 13);

    alignas(32) short intermediate[8 * 16];

    if (tx_type == DCT_DCT) {
        fdct16_8_avx2_16s<0>(input, stride, intermediate);
        fdct8_16_avx2_16s<1>(intermediate, 16, coeff);
    } else if (tx_type == ADST_DCT) {
        fadst16_8_avx2_16s<0>(input, stride, intermediate);
        fdct8_16_avx2_16s <1>(intermediate, 16, coeff);
    } else if (tx_type == DCT_ADST) {
        fdct16_8_avx2_16s <0>(input, stride, intermediate);
        fadst8_16_avx2_16s<1>(intermediate, 16, coeff);
    } else { assert(tx_type == ADST_ADST);
        fadst16_8_avx2_16s<0>(input, stride, intermediate);
        fadst8_16_avx2_16s<1>(intermediate, 16, coeff);
    }
}

static void ftransform_16x8_avx2(const short *input, short *coeff, int stride, TxType tx_type)
{
    assert((size_t(input) & 31) == 0);
    assert((size_t(coeff) & 31) == 0);
    assert(fwd_txfm_shift_ls[TX_16X8][0] == 2);
    assert(fwd_txfm_shift_ls[TX_16X8][1] == -2);
    assert(fwd_txfm_shift_ls[TX_16X8][2] == 0);
    assert(fwd_cos_bit_col[TX_16X16][TX_8X8] == 13);
    assert(fwd_cos_bit_row[TX_16X16][TX_8X8] == 13);

    alignas(32) short intermediate[8 * 16];

    if (tx_type == DCT_DCT) {
        fdct8_16_avx2_16s<0>(input, stride, intermediate);
        fdct16_8_avx2_16s<1>(intermediate, 8, coeff);
    } else if (tx_type == ADST_DCT) {
        fadst8_16_avx2_16s<0>(input, stride, intermediate);
        fdct16_8_avx2_16s <1>(intermediate, 8, coeff);
    } else if (tx_type == DCT_ADST) {
        fdct8_16_avx2_16s <0>(input, stride, intermediate);
        fadst16_8_avx2_16s<1>(intermediate, 8, coeff);
    } else { assert(tx_type == ADST_ADST);
        fadst8_16_avx2_16s<0>(input, stride, intermediate);
        fadst16_8_avx2_16s<1>(intermediate, 8, coeff);
    }
}

static void ftransform_16x32_avx2(const short *input, short *coeff, int pitch)
{
    assert((size_t(input) & 31) == 0);
    assert((size_t(pitch) & 31) == 0);
    assert((size_t(coeff) & 31) == 0);
    assert(fwd_txfm_shift_ls[TX_16X32][0] == 2);
    assert(fwd_txfm_shift_ls[TX_16X32][1] == -4);
    assert(fwd_txfm_shift_ls[TX_16X32][2] == 0);
    assert(fwd_cos_bit_col[TX_16X16][TX_32X32] == 12);
    assert(fwd_cos_bit_row[TX_16X16][TX_32X32] == 13);

    const int bit = 12;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_m32_p32 = mm256_set2_epi16(-cospi[32],  cospi[32]);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_m16_p48 = mm256_set2_epi16(-cospi[16],  cospi[48]);
    const __m256i cospi_p48_p16 = mm256_set2_epi16( cospi[48],  cospi[16]);
    const __m256i cospi_m48_m16 = mm256_set2_epi16(-cospi[48], -cospi[16]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p56_p08 = mm256_set2_epi16( cospi[56],  cospi[ 8]);
    const __m256i cospi_m08_p56 = mm256_set2_epi16(-cospi[ 8],  cospi[56]);
    const __m256i cospi_p24_p40 = mm256_set2_epi16( cospi[24],  cospi[40]);
    const __m256i cospi_m40_p24 = mm256_set2_epi16(-cospi[40],  cospi[24]);
    const __m256i cospi_m56_m08 = mm256_set2_epi16(-cospi[56], -cospi[ 8]);
    const __m256i cospi_m24_m40 = mm256_set2_epi16(-cospi[24], -cospi[40]);
    const __m256i cospi_p60_p04 = mm256_set2_epi16( cospi[60],  cospi[ 4]);
    const __m256i cospi_m04_p60 = mm256_set2_epi16(-cospi[ 4],  cospi[60]);
    const __m256i cospi_p28_p36 = mm256_set2_epi16( cospi[28],  cospi[36]);
    const __m256i cospi_m36_p28 = mm256_set2_epi16(-cospi[36],  cospi[28]);
    const __m256i cospi_p44_p20 = mm256_set2_epi16( cospi[44],  cospi[20]);
    const __m256i cospi_m20_p44 = mm256_set2_epi16(-cospi[20],  cospi[44]);
    const __m256i cospi_p12_p52 = mm256_set2_epi16( cospi[12],  cospi[52]);
    const __m256i cospi_m52_p12 = mm256_set2_epi16(-cospi[52],  cospi[12]);
    const __m256i cospi_p62_p02 = mm256_set2_epi16( cospi[62],  cospi[ 2]);
    const __m256i cospi_m02_p62 = mm256_set2_epi16(-cospi[ 2],  cospi[62]);
    const __m256i cospi_p30_p34 = mm256_set2_epi16( cospi[30],  cospi[34]);
    const __m256i cospi_m34_p30 = mm256_set2_epi16(-cospi[34],  cospi[30]);
    const __m256i cospi_p46_p18 = mm256_set2_epi16( cospi[46],  cospi[18]);
    const __m256i cospi_m18_p46 = mm256_set2_epi16(-cospi[18],  cospi[46]);
    const __m256i cospi_p14_p50 = mm256_set2_epi16( cospi[14],  cospi[50]);
    const __m256i cospi_m50_p14 = mm256_set2_epi16(-cospi[50],  cospi[14]);
    const __m256i cospi_p54_p10 = mm256_set2_epi16( cospi[54],  cospi[10]);
    const __m256i cospi_m10_p54 = mm256_set2_epi16(-cospi[10],  cospi[54]);
    const __m256i cospi_p22_p42 = mm256_set2_epi16( cospi[22],  cospi[42]);
    const __m256i cospi_m42_p22 = mm256_set2_epi16(-cospi[42],  cospi[22]);
    const __m256i cospi_p38_p26 = mm256_set2_epi16( cospi[38],  cospi[26]);
    const __m256i cospi_m26_p38 = mm256_set2_epi16(-cospi[26],  cospi[38]);
    const __m256i cospi_p06_p58 = mm256_set2_epi16( cospi[ 6],  cospi[58]);
    const __m256i cospi_m58_p06 = mm256_set2_epi16(-cospi[58],  cospi[ 6]);

    __m256i a[32], b[32], c[32], d[32], e[32], f[32], g[32], h[32], j[32];
    alignas(32) short intermediate[16 * 32];

    pitch >>= 4;

    __m256i *pout = (__m256i *)intermediate;
    __m256i *pin  = (__m256i *)input;

    // stage 0
    a[ 0] = _mm256_slli_epi16(pin[ 0 * pitch], 2);
    a[ 1] = _mm256_slli_epi16(pin[ 1 * pitch], 2);
    a[ 2] = _mm256_slli_epi16(pin[ 2 * pitch], 2);
    a[ 3] = _mm256_slli_epi16(pin[ 3 * pitch], 2);
    a[ 4] = _mm256_slli_epi16(pin[ 4 * pitch], 2);
    a[ 5] = _mm256_slli_epi16(pin[ 5 * pitch], 2);
    a[ 6] = _mm256_slli_epi16(pin[ 6 * pitch], 2);
    a[ 7] = _mm256_slli_epi16(pin[ 7 * pitch], 2);
    a[ 8] = _mm256_slli_epi16(pin[ 8 * pitch], 2);
    a[ 9] = _mm256_slli_epi16(pin[ 9 * pitch], 2);
    a[10] = _mm256_slli_epi16(pin[10 * pitch], 2);
    a[11] = _mm256_slli_epi16(pin[11 * pitch], 2);
    a[12] = _mm256_slli_epi16(pin[12 * pitch], 2);
    a[13] = _mm256_slli_epi16(pin[13 * pitch], 2);
    a[14] = _mm256_slli_epi16(pin[14 * pitch], 2);
    a[15] = _mm256_slli_epi16(pin[15 * pitch], 2);
    a[16] = _mm256_slli_epi16(pin[16 * pitch], 2);
    a[17] = _mm256_slli_epi16(pin[17 * pitch], 2);
    a[18] = _mm256_slli_epi16(pin[18 * pitch], 2);
    a[19] = _mm256_slli_epi16(pin[19 * pitch], 2);
    a[20] = _mm256_slli_epi16(pin[20 * pitch], 2);
    a[21] = _mm256_slli_epi16(pin[21 * pitch], 2);
    a[22] = _mm256_slli_epi16(pin[22 * pitch], 2);
    a[23] = _mm256_slli_epi16(pin[23 * pitch], 2);
    a[24] = _mm256_slli_epi16(pin[24 * pitch], 2);
    a[25] = _mm256_slli_epi16(pin[25 * pitch], 2);
    a[26] = _mm256_slli_epi16(pin[26 * pitch], 2);
    a[27] = _mm256_slli_epi16(pin[27 * pitch], 2);
    a[28] = _mm256_slli_epi16(pin[28 * pitch], 2);
    a[29] = _mm256_slli_epi16(pin[29 * pitch], 2);
    a[30] = _mm256_slli_epi16(pin[30 * pitch], 2);
    a[31] = _mm256_slli_epi16(pin[31 * pitch], 2);

    // stage 1
    b[ 0] = _mm256_add_epi16(a[ 0], a[31]);
    b[31] = _mm256_sub_epi16(a[ 0], a[31]);
    b[ 1] = _mm256_add_epi16(a[ 1], a[30]);
    b[30] = _mm256_sub_epi16(a[ 1], a[30]);
    b[ 2] = _mm256_add_epi16(a[ 2], a[29]);
    b[29] = _mm256_sub_epi16(a[ 2], a[29]);
    b[ 3] = _mm256_add_epi16(a[ 3], a[28]);
    b[28] = _mm256_sub_epi16(a[ 3], a[28]);
    b[ 4] = _mm256_add_epi16(a[ 4], a[27]);
    b[27] = _mm256_sub_epi16(a[ 4], a[27]);
    b[ 5] = _mm256_add_epi16(a[ 5], a[26]);
    b[26] = _mm256_sub_epi16(a[ 5], a[26]);
    b[ 6] = _mm256_add_epi16(a[ 6], a[25]);
    b[25] = _mm256_sub_epi16(a[ 6], a[25]);
    b[ 7] = _mm256_add_epi16(a[ 7], a[24]);
    b[24] = _mm256_sub_epi16(a[ 7], a[24]);
    b[ 8] = _mm256_add_epi16(a[ 8], a[23]);
    b[23] = _mm256_sub_epi16(a[ 8], a[23]);
    b[ 9] = _mm256_add_epi16(a[ 9], a[22]);
    b[22] = _mm256_sub_epi16(a[ 9], a[22]);
    b[10] = _mm256_add_epi16(a[10], a[21]);
    b[21] = _mm256_sub_epi16(a[10], a[21]);
    b[11] = _mm256_add_epi16(a[11], a[20]);
    b[20] = _mm256_sub_epi16(a[11], a[20]);
    b[12] = _mm256_add_epi16(a[12], a[19]);
    b[19] = _mm256_sub_epi16(a[12], a[19]);
    b[13] = _mm256_add_epi16(a[13], a[18]);
    b[18] = _mm256_sub_epi16(a[13], a[18]);
    b[14] = _mm256_add_epi16(a[14], a[17]);
    b[17] = _mm256_sub_epi16(a[14], a[17]);
    b[15] = _mm256_add_epi16(a[15], a[16]);
    b[16] = _mm256_sub_epi16(a[15], a[16]);

    // process first 16 rows
    // stage 2
    c[ 0] = _mm256_add_epi16(b[ 0], b[15]);
    c[15] = _mm256_sub_epi16(b[ 0], b[15]);
    c[ 1] = _mm256_add_epi16(b[ 1], b[14]);
    c[14] = _mm256_sub_epi16(b[ 1], b[14]);
    c[ 2] = _mm256_add_epi16(b[ 2], b[13]);
    c[13] = _mm256_sub_epi16(b[ 2], b[13]);
    c[ 3] = _mm256_add_epi16(b[ 3], b[12]);
    c[12] = _mm256_sub_epi16(b[ 3], b[12]);
    c[ 4] = _mm256_add_epi16(b[ 4], b[11]);
    c[11] = _mm256_sub_epi16(b[ 4], b[11]);
    c[ 5] = _mm256_add_epi16(b[ 5], b[10]);
    c[10] = _mm256_sub_epi16(b[ 5], b[10]);
    c[ 6] = _mm256_add_epi16(b[ 6], b[ 9]);
    c[ 9] = _mm256_sub_epi16(b[ 6], b[ 9]);
    c[ 7] = _mm256_add_epi16(b[ 7], b[ 8]);
    c[ 8] = _mm256_sub_epi16(b[ 7], b[ 8]);
    // stage 3
    d[ 0] = _mm256_add_epi16(c[0], c[7]);
    d[ 7] = _mm256_sub_epi16(c[0], c[7]);
    d[ 1] = _mm256_add_epi16(c[1], c[6]);
    d[ 6] = _mm256_sub_epi16(c[1], c[6]);
    d[ 2] = _mm256_add_epi16(c[2], c[5]);
    d[ 5] = _mm256_sub_epi16(c[2], c[5]);
    d[ 3] = _mm256_add_epi16(c[3], c[4]);
    d[ 4] = _mm256_sub_epi16(c[3], c[4]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, c[10], c[13], bit, &d[10], &d[13]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, c[11], c[12], bit, &d[11], &d[12]);
    // stage 4
    e[ 0] = _mm256_add_epi16(d[ 0], d[ 3]);
    e[ 3] = _mm256_sub_epi16(d[ 0], d[ 3]);
    e[ 1] = _mm256_add_epi16(d[ 1], d[ 2]);
    e[ 2] = _mm256_sub_epi16(d[ 1], d[ 2]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, d[5], d[6], bit, &e[5], &e[6]);
    e[ 8] = _mm256_add_epi16(c[ 8], d[11]);
    e[11] = _mm256_sub_epi16(c[ 8], d[11]);
    e[ 9] = _mm256_add_epi16(c[ 9], d[10]);
    e[10] = _mm256_sub_epi16(c[ 9], d[10]);
    e[12] = _mm256_sub_epi16(c[15], d[12]);
    e[15] = _mm256_add_epi16(c[15], d[12]);
    e[13] = _mm256_sub_epi16(c[14], d[13]);
    e[14] = _mm256_add_epi16(c[14], d[13]);
    // stage 5
    btf_avx2(cospi_p32_p32, cospi_p32_m32, e[0], e[1], bit, &j[ 0], &j[16]);
    btf_avx2(cospi_p48_p16, cospi_m16_p48, e[2], e[3], bit, &j[ 8], &j[24]);
    f[ 4] = _mm256_add_epi16(d[ 4], e[ 5]);
    f[ 5] = _mm256_sub_epi16(d[ 4], e[ 5]);
    f[ 6] = _mm256_sub_epi16(d[ 7], e[ 6]);
    f[ 7] = _mm256_add_epi16(d[ 7], e[ 6]);
    btf_avx2(cospi_m16_p48, cospi_p48_p16, e[ 9], e[14], bit, &f[ 9], &f[14]);
    btf_avx2(cospi_m48_m16, cospi_m16_p48, e[10], e[13], bit, &f[10], &f[13]);
    // stage 6
    btf_avx2(cospi_p56_p08, cospi_m08_p56, f[4], f[7], bit, &j[ 4], &j[28]);
    btf_avx2(cospi_p24_p40, cospi_m40_p24, f[5], f[6], bit, &j[20], &j[12]);
    g[ 8] = _mm256_add_epi16(e[ 8], f[ 9]);
    g[ 9] = _mm256_sub_epi16(e[ 8], f[ 9]);
    g[10] = _mm256_sub_epi16(e[11], f[10]);
    g[11] = _mm256_add_epi16(e[11], f[10]);
    g[12] = _mm256_add_epi16(e[12], f[13]);
    g[13] = _mm256_sub_epi16(e[12], f[13]);
    g[14] = _mm256_sub_epi16(e[15], f[14]);
    g[15] = _mm256_add_epi16(e[15], f[14]);
    // stage 7
    // stage 8
    // stage 9
    btf_avx2(cospi_p60_p04, cospi_m04_p60, g[ 8], g[15], bit, &j[ 2], &j[30]);
    btf_avx2(cospi_p28_p36, cospi_m36_p28, g[ 9], g[14], bit, &j[18], &j[14]);
    btf_avx2(cospi_p44_p20, cospi_m20_p44, g[10], g[13], bit, &j[10], &j[22]);
    btf_avx2(cospi_p12_p52, cospi_m52_p12, g[11], g[12], bit, &j[26], &j[ 6]);

    // process second 16 rows
    // stage 2
    btf_avx2(cospi_m32_p32, cospi_p32_p32, b[20], b[27], bit, &c[20], &c[27]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, b[21], b[26], bit, &c[21], &c[26]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, b[22], b[25], bit, &c[22], &c[25]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, b[23], b[24], bit, &c[23], &c[24]);
    // stage 3
    d[16] = _mm256_add_epi16(b[16], c[23]);
    d[23] = _mm256_sub_epi16(b[16], c[23]);
    d[17] = _mm256_add_epi16(b[17], c[22]);
    d[22] = _mm256_sub_epi16(b[17], c[22]);
    d[18] = _mm256_add_epi16(b[18], c[21]);
    d[21] = _mm256_sub_epi16(b[18], c[21]);
    d[19] = _mm256_add_epi16(b[19], c[20]);
    d[20] = _mm256_sub_epi16(b[19], c[20]);
    d[24] = _mm256_sub_epi16(b[31], c[24]);
    d[31] = _mm256_add_epi16(b[31], c[24]);
    d[25] = _mm256_sub_epi16(b[30], c[25]);
    d[30] = _mm256_add_epi16(b[30], c[25]);
    d[26] = _mm256_sub_epi16(b[29], c[26]);
    d[29] = _mm256_add_epi16(b[29], c[26]);
    d[27] = _mm256_sub_epi16(b[28], c[27]);
    d[28] = _mm256_add_epi16(b[28], c[27]);
    // stage 4
    btf_avx2(cospi_m16_p48, cospi_p48_p16, d[18], d[29], bit, &e[18], &e[29]);
    btf_avx2(cospi_m16_p48, cospi_p48_p16, d[19], d[28], bit, &e[19], &e[28]);
    btf_avx2(cospi_m48_m16, cospi_m16_p48, d[20], d[27], bit, &e[20], &e[27]);
    btf_avx2(cospi_m48_m16, cospi_m16_p48, d[21], d[26], bit, &e[21], &e[26]);
    // stage 5
    f[16] = _mm256_add_epi16(d[16], e[19]);
    f[19] = _mm256_sub_epi16(d[16], e[19]);
    f[17] = _mm256_add_epi16(d[17], e[18]);
    f[18] = _mm256_sub_epi16(d[17], e[18]);
    f[20] = _mm256_sub_epi16(d[23], e[20]);
    f[23] = _mm256_add_epi16(d[23], e[20]);
    f[21] = _mm256_sub_epi16(d[22], e[21]);
    f[22] = _mm256_add_epi16(d[22], e[21]);
    f[24] = _mm256_add_epi16(d[24], e[27]);
    f[27] = _mm256_sub_epi16(d[24], e[27]);
    f[25] = _mm256_add_epi16(d[25], e[26]);
    f[26] = _mm256_sub_epi16(d[25], e[26]);
    f[28] = _mm256_sub_epi16(d[31], e[28]);
    f[31] = _mm256_add_epi16(d[31], e[28]);
    f[29] = _mm256_sub_epi16(d[30], e[29]);
    f[30] = _mm256_add_epi16(d[30], e[29]);
    // stage 6
    btf_avx2(cospi_m08_p56, cospi_p56_p08, f[17], f[30], bit, &g[17], &g[30]);
    btf_avx2(cospi_m56_m08, cospi_m08_p56, f[18], f[29], bit, &g[18], &g[29]);
    btf_avx2(cospi_m40_p24, cospi_p24_p40, f[21], f[26], bit, &g[21], &g[26]);
    btf_avx2(cospi_m24_m40, cospi_m40_p24, f[22], f[25], bit, &g[22], &g[25]);
    // stage 7
    h[16] = _mm256_add_epi16(f[16], g[17]);
    h[17] = _mm256_sub_epi16(f[16], g[17]);
    h[18] = _mm256_sub_epi16(f[19], g[18]);
    h[19] = _mm256_add_epi16(f[19], g[18]);
    h[20] = _mm256_add_epi16(f[20], g[21]);
    h[21] = _mm256_sub_epi16(f[20], g[21]);
    h[22] = _mm256_sub_epi16(f[23], g[22]);
    h[23] = _mm256_add_epi16(f[23], g[22]);
    h[24] = _mm256_add_epi16(f[24], g[25]);
    h[25] = _mm256_sub_epi16(f[24], g[25]);
    h[26] = _mm256_sub_epi16(f[27], g[26]);
    h[27] = _mm256_add_epi16(f[27], g[26]);
    h[28] = _mm256_add_epi16(f[28], g[29]);
    h[29] = _mm256_sub_epi16(f[28], g[29]);
    h[30] = _mm256_sub_epi16(f[31], g[30]);
    h[31] = _mm256_add_epi16(f[31], g[30]);
    // stage 8
    // stage 9
    btf_avx2(cospi_p62_p02, cospi_m02_p62, h[16], h[31], bit, &j[ 1], &j[31]);
    btf_avx2(cospi_p30_p34, cospi_m34_p30, h[17], h[30], bit, &j[17], &j[15]);
    btf_avx2(cospi_p46_p18, cospi_m18_p46, h[18], h[29], bit, &j[ 9], &j[23]);
    btf_avx2(cospi_p14_p50, cospi_m50_p14, h[19], h[28], bit, &j[25], &j[ 7]);
    btf_avx2(cospi_p54_p10, cospi_m10_p54, h[20], h[27], bit, &j[ 5], &j[27]);
    btf_avx2(cospi_p22_p42, cospi_m42_p22, h[21], h[26], bit, &j[21], &j[11]);
    btf_avx2(cospi_p38_p26, cospi_m26_p38, h[22], h[25], bit, &j[13], &j[19]);
    btf_avx2(cospi_p06_p58, cospi_m58_p06, h[23], h[24], bit, &j[29], &j[ 3]);

    transpose_16x16_(j +  0, pout + 0);
    transpose_16x16_(j + 16, pout + 1);

    fdct16x16_avx2_16s<13,-4>(intermediate +  0, 32, coeff);
    fdct16x16_avx2_16s<13,-4>(intermediate + 16, 32, coeff + 16 * 16);

    __m256i multiplier = _mm256_set1_epi16((NewSqrt2 - 4096) * 8);
    pout = (__m256i *)coeff;
    pout[ 0] = _mm256_add_epi16(pout[ 0], _mm256_mulhrs_epi16(pout[ 0], multiplier));
    pout[ 1] = _mm256_add_epi16(pout[ 1], _mm256_mulhrs_epi16(pout[ 1], multiplier));
    pout[ 2] = _mm256_add_epi16(pout[ 2], _mm256_mulhrs_epi16(pout[ 2], multiplier));
    pout[ 3] = _mm256_add_epi16(pout[ 3], _mm256_mulhrs_epi16(pout[ 3], multiplier));
    pout[ 4] = _mm256_add_epi16(pout[ 4], _mm256_mulhrs_epi16(pout[ 4], multiplier));
    pout[ 5] = _mm256_add_epi16(pout[ 5], _mm256_mulhrs_epi16(pout[ 5], multiplier));
    pout[ 6] = _mm256_add_epi16(pout[ 6], _mm256_mulhrs_epi16(pout[ 6], multiplier));
    pout[ 7] = _mm256_add_epi16(pout[ 7], _mm256_mulhrs_epi16(pout[ 7], multiplier));
    pout[ 8] = _mm256_add_epi16(pout[ 8], _mm256_mulhrs_epi16(pout[ 8], multiplier));
    pout[ 9] = _mm256_add_epi16(pout[ 9], _mm256_mulhrs_epi16(pout[ 9], multiplier));
    pout[10] = _mm256_add_epi16(pout[10], _mm256_mulhrs_epi16(pout[10], multiplier));
    pout[11] = _mm256_add_epi16(pout[11], _mm256_mulhrs_epi16(pout[11], multiplier));
    pout[12] = _mm256_add_epi16(pout[12], _mm256_mulhrs_epi16(pout[12], multiplier));
    pout[13] = _mm256_add_epi16(pout[13], _mm256_mulhrs_epi16(pout[13], multiplier));
    pout[14] = _mm256_add_epi16(pout[14], _mm256_mulhrs_epi16(pout[14], multiplier));
    pout[15] = _mm256_add_epi16(pout[15], _mm256_mulhrs_epi16(pout[15], multiplier));
    pout[16] = _mm256_add_epi16(pout[16], _mm256_mulhrs_epi16(pout[16], multiplier));
    pout[17] = _mm256_add_epi16(pout[17], _mm256_mulhrs_epi16(pout[17], multiplier));
    pout[18] = _mm256_add_epi16(pout[18], _mm256_mulhrs_epi16(pout[18], multiplier));
    pout[19] = _mm256_add_epi16(pout[19], _mm256_mulhrs_epi16(pout[19], multiplier));
    pout[20] = _mm256_add_epi16(pout[20], _mm256_mulhrs_epi16(pout[20], multiplier));
    pout[21] = _mm256_add_epi16(pout[21], _mm256_mulhrs_epi16(pout[21], multiplier));
    pout[22] = _mm256_add_epi16(pout[22], _mm256_mulhrs_epi16(pout[22], multiplier));
    pout[23] = _mm256_add_epi16(pout[23], _mm256_mulhrs_epi16(pout[23], multiplier));
    pout[24] = _mm256_add_epi16(pout[24], _mm256_mulhrs_epi16(pout[24], multiplier));
    pout[25] = _mm256_add_epi16(pout[25], _mm256_mulhrs_epi16(pout[25], multiplier));
    pout[26] = _mm256_add_epi16(pout[26], _mm256_mulhrs_epi16(pout[26], multiplier));
    pout[27] = _mm256_add_epi16(pout[27], _mm256_mulhrs_epi16(pout[27], multiplier));
    pout[28] = _mm256_add_epi16(pout[28], _mm256_mulhrs_epi16(pout[28], multiplier));
    pout[29] = _mm256_add_epi16(pout[29], _mm256_mulhrs_epi16(pout[29], multiplier));
    pout[30] = _mm256_add_epi16(pout[30], _mm256_mulhrs_epi16(pout[30], multiplier));
    pout[31] = _mm256_add_epi16(pout[31], _mm256_mulhrs_epi16(pout[31], multiplier));
}

static void ftransform_32x16_avx2(const short *input, short *coeff, int pitch)
{
    assert((size_t(input) & 31) == 0);
    assert((size_t(coeff) & 31) == 0);
    assert(fwd_txfm_shift_ls[TX_32X16][0] == 2);
    assert(fwd_txfm_shift_ls[TX_32X16][1] == -4);
    assert(fwd_txfm_shift_ls[TX_32X16][2] == 0);
    assert(fwd_cos_bit_col[TX_32X32][TX_16X16] == 13);
    assert(fwd_cos_bit_row[TX_32X32][TX_16X16] == 13);

    alignas(32) short intermediate[32 * 16];

    fdct16x16_avx2_16s<13,2>(input +  0, pitch, intermediate);
    fdct16x16_avx2_16s<13,2>(input + 16, pitch, intermediate + 16 * 16);

    const int bit = 13;
    const int *cospi = cospi_arr(bit);
    const __m256i cospi_m32_p32 = mm256_set2_epi16(-cospi[32],  cospi[32]);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_m16_p48 = mm256_set2_epi16(-cospi[16],  cospi[48]);
    const __m256i cospi_p48_p16 = mm256_set2_epi16( cospi[48],  cospi[16]);
    const __m256i cospi_m48_m16 = mm256_set2_epi16(-cospi[48], -cospi[16]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p56_p08 = mm256_set2_epi16( cospi[56],  cospi[ 8]);
    const __m256i cospi_m08_p56 = mm256_set2_epi16(-cospi[ 8],  cospi[56]);
    const __m256i cospi_p24_p40 = mm256_set2_epi16( cospi[24],  cospi[40]);
    const __m256i cospi_m40_p24 = mm256_set2_epi16(-cospi[40],  cospi[24]);
    const __m256i cospi_m56_m08 = mm256_set2_epi16(-cospi[56], -cospi[ 8]);
    const __m256i cospi_m24_m40 = mm256_set2_epi16(-cospi[24], -cospi[40]);
    const __m256i cospi_p60_p04 = mm256_set2_epi16( cospi[60],  cospi[ 4]);
    const __m256i cospi_m04_p60 = mm256_set2_epi16(-cospi[ 4],  cospi[60]);
    const __m256i cospi_p28_p36 = mm256_set2_epi16( cospi[28],  cospi[36]);
    const __m256i cospi_m36_p28 = mm256_set2_epi16(-cospi[36],  cospi[28]);
    const __m256i cospi_p44_p20 = mm256_set2_epi16( cospi[44],  cospi[20]);
    const __m256i cospi_m20_p44 = mm256_set2_epi16(-cospi[20],  cospi[44]);
    const __m256i cospi_p12_p52 = mm256_set2_epi16( cospi[12],  cospi[52]);
    const __m256i cospi_m52_p12 = mm256_set2_epi16(-cospi[52],  cospi[12]);
    const __m256i cospi_p62_p02 = mm256_set2_epi16( cospi[62],  cospi[ 2]);
    const __m256i cospi_m02_p62 = mm256_set2_epi16(-cospi[ 2],  cospi[62]);
    const __m256i cospi_p30_p34 = mm256_set2_epi16( cospi[30],  cospi[34]);
    const __m256i cospi_m34_p30 = mm256_set2_epi16(-cospi[34],  cospi[30]);
    const __m256i cospi_p46_p18 = mm256_set2_epi16( cospi[46],  cospi[18]);
    const __m256i cospi_m18_p46 = mm256_set2_epi16(-cospi[18],  cospi[46]);
    const __m256i cospi_p14_p50 = mm256_set2_epi16( cospi[14],  cospi[50]);
    const __m256i cospi_m50_p14 = mm256_set2_epi16(-cospi[50],  cospi[14]);
    const __m256i cospi_p54_p10 = mm256_set2_epi16( cospi[54],  cospi[10]);
    const __m256i cospi_m10_p54 = mm256_set2_epi16(-cospi[10],  cospi[54]);
    const __m256i cospi_p22_p42 = mm256_set2_epi16( cospi[22],  cospi[42]);
    const __m256i cospi_m42_p22 = mm256_set2_epi16(-cospi[42],  cospi[22]);
    const __m256i cospi_p38_p26 = mm256_set2_epi16( cospi[38],  cospi[26]);
    const __m256i cospi_m26_p38 = mm256_set2_epi16(-cospi[26],  cospi[38]);
    const __m256i cospi_p06_p58 = mm256_set2_epi16( cospi[ 6],  cospi[58]);
    const __m256i cospi_m58_p06 = mm256_set2_epi16(-cospi[58],  cospi[ 6]);

    __m256i a[32], b[32], c[32], d[32], e[32], f[32], g[32], h[32], j[32];

    __m256i *pout = (__m256i *)coeff;
    __m256i *pin  = (__m256i *)intermediate;

    const __m256i eight = _mm256_set1_epi16(8);
    a[ 0] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 0], eight), 4);
    a[ 1] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 1], eight), 4);
    a[ 2] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 2], eight), 4);
    a[ 3] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 3], eight), 4);
    a[ 4] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 4], eight), 4);
    a[ 5] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 5], eight), 4);
    a[ 6] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 6], eight), 4);
    a[ 7] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 7], eight), 4);
    a[ 8] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 8], eight), 4);
    a[ 9] = _mm256_srai_epi16(_mm256_add_epi16(pin[ 9], eight), 4);
    a[10] = _mm256_srai_epi16(_mm256_add_epi16(pin[10], eight), 4);
    a[11] = _mm256_srai_epi16(_mm256_add_epi16(pin[11], eight), 4);
    a[12] = _mm256_srai_epi16(_mm256_add_epi16(pin[12], eight), 4);
    a[13] = _mm256_srai_epi16(_mm256_add_epi16(pin[13], eight), 4);
    a[14] = _mm256_srai_epi16(_mm256_add_epi16(pin[14], eight), 4);
    a[15] = _mm256_srai_epi16(_mm256_add_epi16(pin[15], eight), 4);
    a[16] = _mm256_srai_epi16(_mm256_add_epi16(pin[16], eight), 4);
    a[17] = _mm256_srai_epi16(_mm256_add_epi16(pin[17], eight), 4);
    a[18] = _mm256_srai_epi16(_mm256_add_epi16(pin[18], eight), 4);
    a[19] = _mm256_srai_epi16(_mm256_add_epi16(pin[19], eight), 4);
    a[20] = _mm256_srai_epi16(_mm256_add_epi16(pin[20], eight), 4);
    a[21] = _mm256_srai_epi16(_mm256_add_epi16(pin[21], eight), 4);
    a[22] = _mm256_srai_epi16(_mm256_add_epi16(pin[22], eight), 4);
    a[23] = _mm256_srai_epi16(_mm256_add_epi16(pin[23], eight), 4);
    a[24] = _mm256_srai_epi16(_mm256_add_epi16(pin[24], eight), 4);
    a[25] = _mm256_srai_epi16(_mm256_add_epi16(pin[25], eight), 4);
    a[26] = _mm256_srai_epi16(_mm256_add_epi16(pin[26], eight), 4);
    a[27] = _mm256_srai_epi16(_mm256_add_epi16(pin[27], eight), 4);
    a[28] = _mm256_srai_epi16(_mm256_add_epi16(pin[28], eight), 4);
    a[29] = _mm256_srai_epi16(_mm256_add_epi16(pin[29], eight), 4);
    a[30] = _mm256_srai_epi16(_mm256_add_epi16(pin[30], eight), 4);
    a[31] = _mm256_srai_epi16(_mm256_add_epi16(pin[31], eight), 4);

    // stage 1
    b[ 0] = _mm256_add_epi16(a[ 0], a[31]);
    b[31] = _mm256_sub_epi16(a[ 0], a[31]);
    b[ 1] = _mm256_add_epi16(a[ 1], a[30]);
    b[30] = _mm256_sub_epi16(a[ 1], a[30]);
    b[ 2] = _mm256_add_epi16(a[ 2], a[29]);
    b[29] = _mm256_sub_epi16(a[ 2], a[29]);
    b[ 3] = _mm256_add_epi16(a[ 3], a[28]);
    b[28] = _mm256_sub_epi16(a[ 3], a[28]);
    b[ 4] = _mm256_add_epi16(a[ 4], a[27]);
    b[27] = _mm256_sub_epi16(a[ 4], a[27]);
    b[ 5] = _mm256_add_epi16(a[ 5], a[26]);
    b[26] = _mm256_sub_epi16(a[ 5], a[26]);
    b[ 6] = _mm256_add_epi16(a[ 6], a[25]);
    b[25] = _mm256_sub_epi16(a[ 6], a[25]);
    b[ 7] = _mm256_add_epi16(a[ 7], a[24]);
    b[24] = _mm256_sub_epi16(a[ 7], a[24]);
    b[ 8] = _mm256_add_epi16(a[ 8], a[23]);
    b[23] = _mm256_sub_epi16(a[ 8], a[23]);
    b[ 9] = _mm256_add_epi16(a[ 9], a[22]);
    b[22] = _mm256_sub_epi16(a[ 9], a[22]);
    b[10] = _mm256_add_epi16(a[10], a[21]);
    b[21] = _mm256_sub_epi16(a[10], a[21]);
    b[11] = _mm256_add_epi16(a[11], a[20]);
    b[20] = _mm256_sub_epi16(a[11], a[20]);
    b[12] = _mm256_add_epi16(a[12], a[19]);
    b[19] = _mm256_sub_epi16(a[12], a[19]);
    b[13] = _mm256_add_epi16(a[13], a[18]);
    b[18] = _mm256_sub_epi16(a[13], a[18]);
    b[14] = _mm256_add_epi16(a[14], a[17]);
    b[17] = _mm256_sub_epi16(a[14], a[17]);
    b[15] = _mm256_add_epi16(a[15], a[16]);
    b[16] = _mm256_sub_epi16(a[15], a[16]);

    // process first 16 rows
    // stage 2
    c[ 0] = _mm256_add_epi16(b[ 0], b[15]);
    c[15] = _mm256_sub_epi16(b[ 0], b[15]);
    c[ 1] = _mm256_add_epi16(b[ 1], b[14]);
    c[14] = _mm256_sub_epi16(b[ 1], b[14]);
    c[ 2] = _mm256_add_epi16(b[ 2], b[13]);
    c[13] = _mm256_sub_epi16(b[ 2], b[13]);
    c[ 3] = _mm256_add_epi16(b[ 3], b[12]);
    c[12] = _mm256_sub_epi16(b[ 3], b[12]);
    c[ 4] = _mm256_add_epi16(b[ 4], b[11]);
    c[11] = _mm256_sub_epi16(b[ 4], b[11]);
    c[ 5] = _mm256_add_epi16(b[ 5], b[10]);
    c[10] = _mm256_sub_epi16(b[ 5], b[10]);
    c[ 6] = _mm256_add_epi16(b[ 6], b[ 9]);
    c[ 9] = _mm256_sub_epi16(b[ 6], b[ 9]);
    c[ 7] = _mm256_add_epi16(b[ 7], b[ 8]);
    c[ 8] = _mm256_sub_epi16(b[ 7], b[ 8]);
    // stage 3
    d[ 0] = _mm256_add_epi16(c[0], c[7]);
    d[ 7] = _mm256_sub_epi16(c[0], c[7]);
    d[ 1] = _mm256_add_epi16(c[1], c[6]);
    d[ 6] = _mm256_sub_epi16(c[1], c[6]);
    d[ 2] = _mm256_add_epi16(c[2], c[5]);
    d[ 5] = _mm256_sub_epi16(c[2], c[5]);
    d[ 3] = _mm256_add_epi16(c[3], c[4]);
    d[ 4] = _mm256_sub_epi16(c[3], c[4]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, c[10], c[13], bit, &d[10], &d[13]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, c[11], c[12], bit, &d[11], &d[12]);
    // stage 4
    e[ 0] = _mm256_add_epi16(d[ 0], d[ 3]);
    e[ 3] = _mm256_sub_epi16(d[ 0], d[ 3]);
    e[ 1] = _mm256_add_epi16(d[ 1], d[ 2]);
    e[ 2] = _mm256_sub_epi16(d[ 1], d[ 2]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, d[5], d[6], bit, &e[5], &e[6]);
    e[ 8] = _mm256_add_epi16(c[ 8], d[11]);
    e[11] = _mm256_sub_epi16(c[ 8], d[11]);
    e[ 9] = _mm256_add_epi16(c[ 9], d[10]);
    e[10] = _mm256_sub_epi16(c[ 9], d[10]);
    e[12] = _mm256_sub_epi16(c[15], d[12]);
    e[15] = _mm256_add_epi16(c[15], d[12]);
    e[13] = _mm256_sub_epi16(c[14], d[13]);
    e[14] = _mm256_add_epi16(c[14], d[13]);
    // stage 5
    btf_avx2(cospi_p32_p32, cospi_p32_m32, e[0], e[1], bit, &j[ 0], &j[16]);
    btf_avx2(cospi_p48_p16, cospi_m16_p48, e[2], e[3], bit, &j[ 8], &j[24]);
    f[ 4] = _mm256_add_epi16(d[ 4], e[ 5]);
    f[ 5] = _mm256_sub_epi16(d[ 4], e[ 5]);
    f[ 6] = _mm256_sub_epi16(d[ 7], e[ 6]);
    f[ 7] = _mm256_add_epi16(d[ 7], e[ 6]);
    btf_avx2(cospi_m16_p48, cospi_p48_p16, e[ 9], e[14], bit, &f[ 9], &f[14]);
    btf_avx2(cospi_m48_m16, cospi_m16_p48, e[10], e[13], bit, &f[10], &f[13]);
    // stage 6
    btf_avx2(cospi_p56_p08, cospi_m08_p56, f[4], f[7], bit, &j[ 4], &j[28]);
    btf_avx2(cospi_p24_p40, cospi_m40_p24, f[5], f[6], bit, &j[20], &j[12]);
    g[ 8] = _mm256_add_epi16(e[ 8], f[ 9]);
    g[ 9] = _mm256_sub_epi16(e[ 8], f[ 9]);
    g[10] = _mm256_sub_epi16(e[11], f[10]);
    g[11] = _mm256_add_epi16(e[11], f[10]);
    g[12] = _mm256_add_epi16(e[12], f[13]);
    g[13] = _mm256_sub_epi16(e[12], f[13]);
    g[14] = _mm256_sub_epi16(e[15], f[14]);
    g[15] = _mm256_add_epi16(e[15], f[14]);
    // stage 7
    // stage 8
    // stage 9
    btf_avx2(cospi_p60_p04, cospi_m04_p60, g[ 8], g[15], bit, &j[ 2], &j[30]);
    btf_avx2(cospi_p28_p36, cospi_m36_p28, g[ 9], g[14], bit, &j[18], &j[14]);
    btf_avx2(cospi_p44_p20, cospi_m20_p44, g[10], g[13], bit, &j[10], &j[22]);
    btf_avx2(cospi_p12_p52, cospi_m52_p12, g[11], g[12], bit, &j[26], &j[ 6]);

    // process second 16 rows
    // stage 2
    btf_avx2(cospi_m32_p32, cospi_p32_p32, b[20], b[27], bit, &c[20], &c[27]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, b[21], b[26], bit, &c[21], &c[26]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, b[22], b[25], bit, &c[22], &c[25]);
    btf_avx2(cospi_m32_p32, cospi_p32_p32, b[23], b[24], bit, &c[23], &c[24]);
    // stage 3
    d[16] = _mm256_add_epi16(b[16], c[23]);
    d[23] = _mm256_sub_epi16(b[16], c[23]);
    d[17] = _mm256_add_epi16(b[17], c[22]);
    d[22] = _mm256_sub_epi16(b[17], c[22]);
    d[18] = _mm256_add_epi16(b[18], c[21]);
    d[21] = _mm256_sub_epi16(b[18], c[21]);
    d[19] = _mm256_add_epi16(b[19], c[20]);
    d[20] = _mm256_sub_epi16(b[19], c[20]);
    d[24] = _mm256_sub_epi16(b[31], c[24]);
    d[31] = _mm256_add_epi16(b[31], c[24]);
    d[25] = _mm256_sub_epi16(b[30], c[25]);
    d[30] = _mm256_add_epi16(b[30], c[25]);
    d[26] = _mm256_sub_epi16(b[29], c[26]);
    d[29] = _mm256_add_epi16(b[29], c[26]);
    d[27] = _mm256_sub_epi16(b[28], c[27]);
    d[28] = _mm256_add_epi16(b[28], c[27]);
    // stage 4
    btf_avx2(cospi_m16_p48, cospi_p48_p16, d[18], d[29], bit, &e[18], &e[29]);
    btf_avx2(cospi_m16_p48, cospi_p48_p16, d[19], d[28], bit, &e[19], &e[28]);
    btf_avx2(cospi_m48_m16, cospi_m16_p48, d[20], d[27], bit, &e[20], &e[27]);
    btf_avx2(cospi_m48_m16, cospi_m16_p48, d[21], d[26], bit, &e[21], &e[26]);
    // stage 5
    f[16] = _mm256_add_epi16(d[16], e[19]);
    f[19] = _mm256_sub_epi16(d[16], e[19]);
    f[17] = _mm256_add_epi16(d[17], e[18]);
    f[18] = _mm256_sub_epi16(d[17], e[18]);
    f[20] = _mm256_sub_epi16(d[23], e[20]);
    f[23] = _mm256_add_epi16(d[23], e[20]);
    f[21] = _mm256_sub_epi16(d[22], e[21]);
    f[22] = _mm256_add_epi16(d[22], e[21]);
    f[24] = _mm256_add_epi16(d[24], e[27]);
    f[27] = _mm256_sub_epi16(d[24], e[27]);
    f[25] = _mm256_add_epi16(d[25], e[26]);
    f[26] = _mm256_sub_epi16(d[25], e[26]);
    f[28] = _mm256_sub_epi16(d[31], e[28]);
    f[31] = _mm256_add_epi16(d[31], e[28]);
    f[29] = _mm256_sub_epi16(d[30], e[29]);
    f[30] = _mm256_add_epi16(d[30], e[29]);
    // stage 6
    btf_avx2(cospi_m08_p56, cospi_p56_p08, f[17], f[30], bit, &g[17], &g[30]);
    btf_avx2(cospi_m56_m08, cospi_m08_p56, f[18], f[29], bit, &g[18], &g[29]);
    btf_avx2(cospi_m40_p24, cospi_p24_p40, f[21], f[26], bit, &g[21], &g[26]);
    btf_avx2(cospi_m24_m40, cospi_m40_p24, f[22], f[25], bit, &g[22], &g[25]);
    // stage 7
    h[16] = _mm256_add_epi16(f[16], g[17]);
    h[17] = _mm256_sub_epi16(f[16], g[17]);
    h[18] = _mm256_sub_epi16(f[19], g[18]);
    h[19] = _mm256_add_epi16(f[19], g[18]);
    h[20] = _mm256_add_epi16(f[20], g[21]);
    h[21] = _mm256_sub_epi16(f[20], g[21]);
    h[22] = _mm256_sub_epi16(f[23], g[22]);
    h[23] = _mm256_add_epi16(f[23], g[22]);
    h[24] = _mm256_add_epi16(f[24], g[25]);
    h[25] = _mm256_sub_epi16(f[24], g[25]);
    h[26] = _mm256_sub_epi16(f[27], g[26]);
    h[27] = _mm256_add_epi16(f[27], g[26]);
    h[28] = _mm256_add_epi16(f[28], g[29]);
    h[29] = _mm256_sub_epi16(f[28], g[29]);
    h[30] = _mm256_sub_epi16(f[31], g[30]);
    h[31] = _mm256_add_epi16(f[31], g[30]);
    // stage 8
    // stage 9
    btf_avx2(cospi_p62_p02, cospi_m02_p62, h[16], h[31], bit, &j[ 1], &j[31]);
    btf_avx2(cospi_p30_p34, cospi_m34_p30, h[17], h[30], bit, &j[17], &j[15]);
    btf_avx2(cospi_p46_p18, cospi_m18_p46, h[18], h[29], bit, &j[ 9], &j[23]);
    btf_avx2(cospi_p14_p50, cospi_m50_p14, h[19], h[28], bit, &j[25], &j[ 7]);
    btf_avx2(cospi_p54_p10, cospi_m10_p54, h[20], h[27], bit, &j[ 5], &j[27]);
    btf_avx2(cospi_p22_p42, cospi_m42_p22, h[21], h[26], bit, &j[21], &j[11]);
    btf_avx2(cospi_p38_p26, cospi_m26_p38, h[22], h[25], bit, &j[13], &j[19]);
    btf_avx2(cospi_p06_p58, cospi_m58_p06, h[23], h[24], bit, &j[29], &j[ 3]);

    __m256i multiplier = _mm256_set1_epi16((NewSqrt2 - 4096) * 8);
    pout = (__m256i *)coeff;
    j[ 0] = _mm256_add_epi16(j[ 0], _mm256_mulhrs_epi16(j[ 0], multiplier));
    j[ 1] = _mm256_add_epi16(j[ 1], _mm256_mulhrs_epi16(j[ 1], multiplier));
    j[ 2] = _mm256_add_epi16(j[ 2], _mm256_mulhrs_epi16(j[ 2], multiplier));
    j[ 3] = _mm256_add_epi16(j[ 3], _mm256_mulhrs_epi16(j[ 3], multiplier));
    j[ 4] = _mm256_add_epi16(j[ 4], _mm256_mulhrs_epi16(j[ 4], multiplier));
    j[ 5] = _mm256_add_epi16(j[ 5], _mm256_mulhrs_epi16(j[ 5], multiplier));
    j[ 6] = _mm256_add_epi16(j[ 6], _mm256_mulhrs_epi16(j[ 6], multiplier));
    j[ 7] = _mm256_add_epi16(j[ 7], _mm256_mulhrs_epi16(j[ 7], multiplier));
    j[ 8] = _mm256_add_epi16(j[ 8], _mm256_mulhrs_epi16(j[ 8], multiplier));
    j[ 9] = _mm256_add_epi16(j[ 9], _mm256_mulhrs_epi16(j[ 9], multiplier));
    j[10] = _mm256_add_epi16(j[10], _mm256_mulhrs_epi16(j[10], multiplier));
    j[11] = _mm256_add_epi16(j[11], _mm256_mulhrs_epi16(j[11], multiplier));
    j[12] = _mm256_add_epi16(j[12], _mm256_mulhrs_epi16(j[12], multiplier));
    j[13] = _mm256_add_epi16(j[13], _mm256_mulhrs_epi16(j[13], multiplier));
    j[14] = _mm256_add_epi16(j[14], _mm256_mulhrs_epi16(j[14], multiplier));
    j[15] = _mm256_add_epi16(j[15], _mm256_mulhrs_epi16(j[15], multiplier));
    j[16] = _mm256_add_epi16(j[16], _mm256_mulhrs_epi16(j[16], multiplier));
    j[17] = _mm256_add_epi16(j[17], _mm256_mulhrs_epi16(j[17], multiplier));
    j[18] = _mm256_add_epi16(j[18], _mm256_mulhrs_epi16(j[18], multiplier));
    j[19] = _mm256_add_epi16(j[19], _mm256_mulhrs_epi16(j[19], multiplier));
    j[20] = _mm256_add_epi16(j[20], _mm256_mulhrs_epi16(j[20], multiplier));
    j[21] = _mm256_add_epi16(j[21], _mm256_mulhrs_epi16(j[21], multiplier));
    j[22] = _mm256_add_epi16(j[22], _mm256_mulhrs_epi16(j[22], multiplier));
    j[23] = _mm256_add_epi16(j[23], _mm256_mulhrs_epi16(j[23], multiplier));
    j[24] = _mm256_add_epi16(j[24], _mm256_mulhrs_epi16(j[24], multiplier));
    j[25] = _mm256_add_epi16(j[25], _mm256_mulhrs_epi16(j[25], multiplier));
    j[26] = _mm256_add_epi16(j[26], _mm256_mulhrs_epi16(j[26], multiplier));
    j[27] = _mm256_add_epi16(j[27], _mm256_mulhrs_epi16(j[27], multiplier));
    j[28] = _mm256_add_epi16(j[28], _mm256_mulhrs_epi16(j[28], multiplier));
    j[29] = _mm256_add_epi16(j[29], _mm256_mulhrs_epi16(j[29], multiplier));
    j[30] = _mm256_add_epi16(j[30], _mm256_mulhrs_epi16(j[30], multiplier));
    j[31] = _mm256_add_epi16(j[31], _mm256_mulhrs_epi16(j[31], multiplier));

    transpose_16x16_(j +  0, pout + 0);
    transpose_16x16_(j + 16, pout + 1);
}

void av1_fwd_txfm2d_64x64_c(const short *input, int *output, int stride, TxType tx_type, int bd);
void av1_fwd_txfm2d_4x8_c(const short *input, int *output, int stride, TxType tx_type, int bd);
void av1_fwd_txfm2d_8x4_c(const short *input, int *output, int stride, TxType tx_type, int bd);
void av1_fwd_txfm2d_8x16_c(const short *input, int *output, int stride, TxType tx_type, int bd);
void av1_fwd_txfm2d_16x8_c(const short *input, int *output, int stride, TxType tx_type, int bd);
void av1_fwd_txfm2d_16x32_c(const short *input, int *output, int stride, TxType tx_type, int bd);
void av1_fwd_txfm2d_32x16_c(const short *input, int *output, int stride, TxType tx_type, int bd);
void av1_fwd_txfm2d_32x64_c(const short *input, int *output, int stride, TxType tx_type, int bd);
void av1_fwd_txfm2d_64x32_c(const short *input, int *output, int stride, TxType tx_type, int bd);

namespace AV1PP {

    template <int size, int type> void ftransform_av1_avx2(const short *src, short *dst, int pitchSrc) {
        int dst32[64*64];

        if      (size == TX_4X4)   return ftransform_4x4_sse4(src, dst, pitchSrc, type);
        else if (size == TX_8X8)   return ftransform_8x8_avx2(src, dst, pitchSrc, type);
        else if (size == TX_16X16) return ftransform_16x16_avx2(src, dst, pitchSrc, type);
        else if (size == TX_32X32) return ftransform_32x32_avx2(src, dst, pitchSrc);
        else if (size == TX_64X64) av1_fwd_txfm2d_64x64_c(src, dst32, pitchSrc, type, 8);
        else if (size == TX_4X8)   return ftransform_4x8_avx2(src, dst, pitchSrc, type);
        else if (size == TX_8X4)   return ftransform_8x4_avx2(src, dst, pitchSrc, type);
        else if (size == TX_8X16)  return ftransform_8x16_avx2(src, dst, pitchSrc, type);
        else if (size == TX_16X8)  return ftransform_16x8_avx2(src, dst, pitchSrc, type);
        else if (size == TX_16X32) return ftransform_16x32_avx2(src, dst, pitchSrc);
        else if (size == TX_32X16) return ftransform_32x16_avx2(src, dst, pitchSrc);
        else if (size == TX_32X64) av1_fwd_txfm2d_32x64_c(src, dst32, pitchSrc, type, 8);
        else if (size == TX_64X32) av1_fwd_txfm2d_64x32_c(src, dst32, pitchSrc, type, 8);
        else {assert(0);}

        const int txh = tx_size_high[size];
        const int txw = tx_size_wide[size];
        for (int i = 0; i < txh; i++)
            for (int j = 0; j < txw; j++)
                dst[i * txw + j] = dst32[i * txw + j];
    }
    template void ftransform_av1_avx2<TX_4X4,      DCT_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_4X4,     ADST_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_4X4,     DCT_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_4X4,    ADST_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X8,      DCT_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X8,     ADST_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X8,     DCT_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X8,    ADST_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_16X16,    DCT_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_16X16,   ADST_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_16X16,   DCT_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_16X16,  ADST_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_32X32,    DCT_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_64X64,    DCT_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_4X8,      DCT_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_4X8,     ADST_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_4X8,     DCT_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_4X8,    ADST_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X4,      DCT_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X4,     ADST_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X4,     DCT_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X4,    ADST_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X16,     DCT_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X16,    ADST_DCT>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X16,    DCT_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_8X16,   ADST_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_16X8,    DCT_DCT >(const short*,short*,int);
    template void ftransform_av1_avx2<TX_16X8,   ADST_DCT >(const short*,short*,int);
    template void ftransform_av1_avx2<TX_16X8,    DCT_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_16X8,   ADST_ADST>(const short*,short*,int);
    template void ftransform_av1_avx2<TX_16X32,   DCT_DCT >(const short*,short*,int);
    template void ftransform_av1_avx2<TX_32X16,   DCT_DCT >(const short*,short*,int);
    template void ftransform_av1_avx2<TX_32X64,   DCT_DCT >(const short*,short*,int);
    template void ftransform_av1_avx2<TX_64X32,   DCT_DCT >(const short*,short*,int);

}; // namespace AV1PP

