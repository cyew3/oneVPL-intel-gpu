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
#include "mfx_av1_transform_common.h"
#include "mfx_av1_inv_transform_common.h"
#include "mfx_av1_transform_common_avx2.h"

using namespace AV1PP;

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

static const short sinpi_arr_data[5] = { 0, 1321, 2482, 3344, 3803 };

static void write_buffer_4x4_16s_8u(const int16_t *in, uint8_t *output, int stride)
{
    __m128i r0, r1;
    r0 = _mm_unpacklo_epi32(loadl_si32(output + 0 * stride), loadl_si32(output + 1 * stride));
    r1 = _mm_unpacklo_epi32(loadl_si32(output + 2 * stride), loadl_si32(output + 3 * stride));
    r0 = _mm_unpacklo_epi8(r0, _mm_setzero_si128());
    r1 = _mm_unpacklo_epi8(r1, _mm_setzero_si128());
    r0 = _mm_add_epi16(r0, loada_si128(in + 0));
    r1 = _mm_add_epi16(r1, loada_si128(in + 8));
    r0 = _mm_packus_epi16(r0, r1);
    storel_si32(output + 0 * stride, r0);
    storel_si32(output + 1 * stride, _mm_srli_si128(r0, 4));
    storel_si32(output + 2 * stride, _mm_srli_si128(r0, 8));
    storel_si32(output + 3 * stride, _mm_srli_si128(r0, 12));
}

static void write_buffer_8x8_16s_8u(const int16_t *in, uint8_t *output, int stride)
{
    __m256i r0, r1, r2, r3;
    r1 = loadu4_epi64(output + 0 * stride, output + 2 * stride, output + 1 * stride, output + 3 * stride);
    r3 = loadu4_epi64(output + 4 * stride, output + 6 * stride, output + 5 * stride, output + 7 * stride);
    r0 = _mm256_unpacklo_epi8(r1, _mm256_setzero_si256());
    r1 = _mm256_unpackhi_epi8(r1, _mm256_setzero_si256());
    r2 = _mm256_unpacklo_epi8(r3, _mm256_setzero_si256());
    r3 = _mm256_unpackhi_epi8(r3, _mm256_setzero_si256());
    r0 = _mm256_add_epi16(r0, loada_si256(in +  0));
    r1 = _mm256_add_epi16(r1, loada_si256(in + 16));
    r2 = _mm256_add_epi16(r2, loada_si256(in + 32));
    r3 = _mm256_add_epi16(r3, loada_si256(in + 48));
    r0 = _mm256_packus_epi16(r0, r1);   // row0 row2 row1 row3
    r2 = _mm256_packus_epi16(r2, r3);   // row4 row6 row5 row7
    storel_epi64(output + 0 * stride, si128_lo(r0));
    storel_epi64(output + 1 * stride, si128_hi(r0));
    storeh_epi64(output + 2 * stride, si128_lo(r0));
    storeh_epi64(output + 3 * stride, si128_hi(r0));
    storel_epi64(output + 4 * stride, si128_lo(r2));
    storel_epi64(output + 5 * stride, si128_hi(r2));
    storeh_epi64(output + 6 * stride, si128_lo(r2));
    storeh_epi64(output + 7 * stride, si128_hi(r2));
}

static void write_buffer_16x16_16s_8u(const int16_t *in, uint8_t *output, int stride)
{
    __m256i r0, r1;
    for (int i = 0; i < 16; i += 2) {
        r0 = _mm256_cvtepu8_epi16(loada_si128(output + i * stride));
        r1 = _mm256_cvtepu8_epi16(loada_si128(output + i * stride + stride));
        r0 = _mm256_add_epi16(r0, loada_si256(in + i * 16 +  0));
        r1 = _mm256_add_epi16(r1, loada_si256(in + i * 16 + 16));
        r0 = _mm256_packus_epi16(r0, r1);
        r0 = _mm256_permute4x64_epi64(r0, PERM4x64(0,2,1,3));
        storea2_m128i(output + i * stride, output + i * stride + stride, r0);
    }
}

static void write_buffer_32x32_16s_8u(const int16_t *in, uint8_t *output, int stride)
{
    __m256i r0, r1;
    for (int i = 0; i < 32; i++) {
        r0 = _mm256_cvtepu8_epi16(loada_si128(output + i * stride));
        r1 = _mm256_cvtepu8_epi16(loada_si128(output + i * stride + 16));
        r0 = _mm256_add_epi16(r0, loada_si256(in + i * 32 +  0));
        r1 = _mm256_add_epi16(r1, loada_si256(in + i * 32 + 16));
        r0 = _mm256_packus_epi16(r0, r1);
        r0 = _mm256_permute4x64_epi64(r0, PERM4x64(0,2,1,3));
        storea_si256(output + i * stride, r0);
    }
}

static void idct4x4_sse4_16s(short *inout)
{
    const short *cospi = cospi_arr_data;
    const __m128i rnding   = _mm_set1_epi32(1 << 11);
    __m128i u02, u13, v01, v32, x, y;

    // in: A0 A1 A2 A3
    //     B0 B1 B2 B3
    //     C0 C1 C2 C3
    //     D0 D1 D2 D3

    u02 = _mm_packs_epi32(_mm_srai_epi32(_mm_slli_epi32(((__m128i *)inout)[0], 16), 16),
        _mm_srai_epi32(_mm_slli_epi32(((__m128i *)inout)[1], 16), 16));
    u13 = _mm_packs_epi32(_mm_srai_epi32(((__m128i *)inout)[0], 16),
        _mm_srai_epi32(((__m128i *)inout)[1], 16));

    // u02: A0 A2 B0 B2 C0 C2 D0 D2
    // u13: A1 A3 B1 B3 C1 C3 D1 D3

    x = _mm_madd_epi16(u02, mm_set2_epi16(cospi[32],  cospi[32]));
    y = _mm_madd_epi16(u02, mm_set2_epi16(cospi[32], -cospi[32]));
    x = _mm_add_epi32(x, rnding);
    y = _mm_add_epi32(y, rnding);
    x = _mm_srai_epi32(x, 12);
    y = _mm_srai_epi32(y, 12);
    v01 = _mm_packs_epi32(x, y);

    x = _mm_madd_epi16(u13, mm_set2_epi16(cospi[16],  cospi[48]));
    y = _mm_madd_epi16(u13, mm_set2_epi16(cospi[48], -cospi[16]));
    x = _mm_add_epi32(x, rnding);
    y = _mm_add_epi32(y, rnding);
    x = _mm_srai_epi32(x, 12);
    y = _mm_srai_epi32(y, 12);
    v32 = _mm_packs_epi32(x, y);

    ((__m128i *)inout)[0] = _mm_add_epi16(v01, v32);
    ((__m128i *)inout)[1] = _mm_sub_epi16(v01, v32);
    ((__m128i *)inout)[1] = _mm_shuffle_epi32(((__m128i *)inout)[1], SHUF32(2,3,0,1));
}

static void iadst4x4_sse4_16s(short *inout) {
    const short *sinpi = sinpi_arr_data;
    const __m128i rnding = _mm_set1_epi32(1 << (12 - 1));
    const __m128i sinpi_p1_p4 = mm_set2_epi16(sinpi[1],  sinpi[4]);
    const __m128i sinpi_p2_m1 = mm_set2_epi16(sinpi[2], -sinpi[1]);
    const __m128i sinpi_p3_p3 = mm_set2_epi16(sinpi[3],  sinpi[3]);
    const __m128i sinpi_p4_p4 = mm_set2_epi16(sinpi[4],  sinpi[4]);

    const __m128i zero = _mm_setzero_si128();

    __m128i a01 = loada_si128(inout + 0);
    __m128i a23 = loada_si128(inout + 8);
    __m128i t0 = _mm_unpacklo_epi16(a01, a23);
    __m128i t1 = _mm_unpackhi_epi16(a01, a23);
    a01 = _mm_unpacklo_epi16(t0, t1);
    a23 = _mm_unpackhi_epi16(t0, t1);

    __m128i b02 = _mm_unpacklo_epi16(a01, a23);     // interleaved rows 0 and 2
    __m128i b3z = _mm_unpackhi_epi16(a23, zero);    // interleaved rows 3 with zero
    __m128i b1z = _mm_unpackhi_epi16(a01, zero);    // interleaved rows 1 with zero
    __m128i c0 = _mm_madd_epi16(b02, sinpi_p1_p4);  // sinpi1 * a0 + sinpi4 * a2
    __m128i c1 = _mm_madd_epi16(b02, sinpi_p2_m1);  // sinpi2 * a0 - sinpi1 * a2
    __m128i c3 = _mm_madd_epi16(b1z, sinpi_p3_p3);  // sinpi3 * a1
    __m128i tmp = _mm_madd_epi16(b3z, sinpi_p2_m1); // sinpi2 * a3
    c0 = _mm_add_epi32(c0, tmp);                    // sinpi1 * a0 + sinpi4 * a2 + sinpi2 * a3
    tmp = _mm_madd_epi16(b3z, sinpi_p4_p4);         // sinpi4 * a3
    c1 = _mm_sub_epi32(c1, tmp);                    // sinpi2 * a0 - sinpi1 * a2 - sinpi4 * a3

    __m128i c2 = _mm_unpacklo_epi16(
        _mm_sub_epi16(a01, a23),
        _mm_srli_si128(a23, 8));                    // interleaved a0-a2 and a3
    c2 = _mm_madd_epi16(c2, sinpi_p3_p3);           // sinpi3 * a0 - sinpi3 * a2 + sinpi3 * a3

    __m128i d0 = _mm_add_epi32(c0, c3);
    __m128i d1 = _mm_add_epi32(c1, c3);
    __m128i d3 = _mm_sub_epi32(_mm_add_epi32(c0, c1), c3);

    __m128i e0 = _mm_srai_epi32(_mm_add_epi32(d0, rnding), 12);
    __m128i e1 = _mm_srai_epi32(_mm_add_epi32(d1, rnding), 12);
    __m128i e2 = _mm_srai_epi32(_mm_add_epi32(c2, rnding), 12);
    __m128i e3 = _mm_srai_epi32(_mm_add_epi32(d3, rnding), 12);

    ((__m128i *)inout)[0] = _mm_packs_epi32(e0, e1);
    ((__m128i *)inout)[1] = _mm_packs_epi32(e2, e3);
}

static void idct8x8_avx2_16s(const short *in, short *out, int final_shift)
{
    const short *cospi = cospi_arr_data;
    const __m256i cospi_p08_p56 = mm256_set2_epi16(cospi[ 8],  cospi[56]);
    const __m256i cospi_p56_m08 = mm256_set2_epi16(cospi[56], -cospi[ 8]);
    const __m256i cospi_p40_p24 = mm256_set2_epi16(cospi[40],  cospi[24]);
    const __m256i cospi_p24_m40 = mm256_set2_epi16(cospi[24], -cospi[40]);
    const __m256i cospi_p32_p32 = mm256_set2_epi16(cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16(cospi[32], -cospi[32]);
    const __m256i cospi_p16_p48 = mm256_set2_epi16(cospi[16],  cospi[48]);
    const __m256i cospi_p48_m16 = mm256_set2_epi16(cospi[48], -cospi[16]);

    // transpose input
    __m256i a[4], b[4];
    a[0] = loada2_m128i(in + 0 * 8, in + 4 * 8);
    a[1] = loada2_m128i(in + 1 * 8, in + 5 * 8);
    a[2] = loada2_m128i(in + 2 * 8, in + 6 * 8);
    a[3] = loada2_m128i(in + 3 * 8, in + 7 * 8);
    b[0] = _mm256_unpacklo_epi16(a[0], a[1]);
    b[1] = _mm256_unpacklo_epi16(a[2], a[3]);
    b[2] = _mm256_unpackhi_epi16(a[0], a[1]);
    b[3] = _mm256_unpackhi_epi16(a[2], a[3]);
    a[0] = _mm256_unpacklo_epi32(b[0], b[1]); // A0 A1 A2 A3 B0 B1 B2 B3 A4 A5 A6 A7 B4 B5 B6 B7
    a[1] = _mm256_unpackhi_epi32(b[0], b[1]); // C0 C1 C2 C3 D0 D1 D2 D3 C4 C5 C6 C7 D4 D5 D6 D7
    a[2] = _mm256_unpacklo_epi32(b[2], b[3]); // E0 E1 E2 E3 F0 F1 F2 F3 E4 E5 F6 E7 F4 F5 F6 F7
    a[3] = _mm256_unpackhi_epi32(b[2], b[3]); // G0 G1 G2 G3 H0 H1 H2 H3 G4 G5 G6 G7 H4 H5 H6 H7

    // stage 0
    // stage 1
    // stage 2
    __m256i a17 = _mm256_unpackhi_epi16(a[0], a[3]);
    __m256i a53 = _mm256_unpackhi_epi16(a[2], a[1]);
    __m256i b74 = btf16_avx2(cospi_p08_p56, cospi_p56_m08, a17, 12);
    __m256i b65 = btf16_avx2(cospi_p40_p24, cospi_p24_m40, a53, 12);

    // stage 3
    __m256i a04 = _mm256_unpacklo_epi16(a[0], a[2]);
    __m256i a26 = _mm256_unpacklo_epi16(a[1], a[3]);
    __m256i c01 = btf16_avx2(cospi_p32_p32, cospi_p32_m32, a04, 12);
    __m256i c32 = btf16_avx2(cospi_p16_p48, cospi_p48_m16, a26, 12);
    __m256i c74 = _mm256_add_epi16(b74, b65);
    __m256i c65 = _mm256_sub_epi16(b74, b65);

    // stage 4
    __m256i d01 = _mm256_add_epi16(c01, c32);
    __m256i d32 = _mm256_sub_epi16(c01, c32);
    __m256i d23 = _mm256_permute4x64_epi64(d32, PERM4x64(2,3,0,1));
    __m256i d65 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(c65, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(c65, PERM4x64(2,2,3,3)));
    d65 = btf16_avx2(cospi_p32_p32, cospi_p32_m32, d65, 12);
    __m256i d76 = _mm256_permute2x128_si256(c74, d65, PERM2x128(0,2));
    __m256i d54 = _mm256_permute2x128_si256(c74, d65, PERM2x128(3,1));

    const __m256i final_rounding = _mm256_set1_epi16((1 << final_shift) >> 1);
    d01 = _mm256_add_epi16(d01, final_rounding);
    d23 = _mm256_add_epi16(d23, final_rounding);

    // stage 5
    __m256i *pout = (__m256i *)out;
    __m256i e54 = _mm256_srai_epi16(_mm256_sub_epi16(d23, d54), final_shift);
    __m256i e76 = _mm256_srai_epi16(_mm256_sub_epi16(d01, d76), final_shift);
    pout[0] = _mm256_srai_epi16(_mm256_add_epi16(d01, d76), final_shift);
    pout[1] = _mm256_srai_epi16(_mm256_add_epi16(d23, d54), final_shift);
    pout[2] = _mm256_permute4x64_epi64(e54, PERM4x64(2,3,0,1));
    pout[3] = _mm256_permute4x64_epi64(e76, PERM4x64(2,3,0,1));
}

static void iadst8x8_avx2_16s(const short *in, short *out, int final_shift)
{
    const short *cospi = cospi_arr_data;
    const __m256i cospi_p04_p60 = mm256_set2_epi16( cospi[ 4],  cospi[60]);
    const __m256i cospi_p60_m04 = mm256_set2_epi16( cospi[60], -cospi[ 4]);
    const __m256i cospi_p20_p44 = mm256_set2_epi16( cospi[20],  cospi[44]);
    const __m256i cospi_p44_m20 = mm256_set2_epi16( cospi[44], -cospi[20]);
    const __m256i cospi_p36_p28 = mm256_set2_epi16( cospi[36],  cospi[28]);
    const __m256i cospi_p28_m36 = mm256_set2_epi16( cospi[28], -cospi[36]);
    const __m256i cospi_p52_p12 = mm256_set2_epi16( cospi[52],  cospi[12]);
    const __m256i cospi_p12_m52 = mm256_set2_epi16( cospi[12], -cospi[52]);
    const __m256i cospi_p16_p48 = mm256_set2_epi16( cospi[16],  cospi[48]);
    const __m256i cospi_p48_m16 = mm256_set2_epi16( cospi[48], -cospi[16]);
    const __m256i cospi_m48_p16 = mm256_set2_epi16(-cospi[48],  cospi[16]);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);

    // transpose input
    __m256i a[4], b[4];
    a[0] = loada2_m128i(in + 0 * 8, in + 4 * 8);
    a[1] = loada2_m128i(in + 1 * 8, in + 5 * 8);
    a[2] = loada2_m128i(in + 2 * 8, in + 6 * 8);
    a[3] = loada2_m128i(in + 3 * 8, in + 7 * 8);
    b[0] = _mm256_unpacklo_epi16(a[0], a[1]);
    b[1] = _mm256_unpacklo_epi16(a[2], a[3]);
    b[2] = _mm256_unpackhi_epi16(a[0], a[1]);
    b[3] = _mm256_unpackhi_epi16(a[2], a[3]);
    __m256i a01 = _mm256_unpacklo_epi32(b[0], b[1]);
    __m256i a23 = _mm256_unpackhi_epi32(b[0], b[1]);
    __m256i a45 = _mm256_unpacklo_epi32(b[2], b[3]);
    __m256i a67 = _mm256_unpackhi_epi32(b[2], b[3]);

    // stage 0
    // stage 1
    // stage 2
    __m256i a76 = _mm256_shuffle_epi32(a67, SHUF32(2,3,0,1));
    __m256i a54 = _mm256_shuffle_epi32(a45, SHUF32(2,3,0,1));
    __m256i b70 = _mm256_unpacklo_epi16(a76, a01);
    __m256i b52 = _mm256_unpacklo_epi16(a54, a23);
    __m256i b34 = _mm256_unpackhi_epi16(a23, a54);
    __m256i b16 = _mm256_unpackhi_epi16(a01, a76);
    __m256i c01 = btf16_avx2(cospi_p04_p60, cospi_p60_m04, b70, 12);
    __m256i c23 = btf16_avx2(cospi_p20_p44, cospi_p44_m20, b52, 12);
    __m256i c45 = btf16_avx2(cospi_p36_p28, cospi_p28_m36, b34, 12);
    __m256i c67 = btf16_avx2(cospi_p52_p12, cospi_p12_m52, b16, 12);

    // stage 3
    __m256i d01 = _mm256_add_epi16(c01, c45);
    __m256i d23 = _mm256_add_epi16(c23, c67);
    __m256i d45 = _mm256_sub_epi16(c01, c45);
    __m256i d67 = _mm256_sub_epi16(c23, c67);

    // stage 4
    __m256i e45 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d45, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(d45, PERM4x64(2,2,3,3)));
    __m256i e67 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(d67, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(d67, PERM4x64(2,2,3,3)));
    e45 = btf16_avx2(cospi_p16_p48, cospi_p48_m16, e45, 12);
    e67 = btf16_avx2(cospi_m48_p16, cospi_p16_p48, e67, 12);

    // stage 5
    __m256i f01 = _mm256_add_epi16(d01, d23);
    __m256i f23 = _mm256_sub_epi16(d01, d23);
    __m256i f45 = _mm256_add_epi16(e45, e67);
    __m256i f67 = _mm256_sub_epi16(e45, e67);

    // stage 6
    __m256i g23 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f23, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(f23, PERM4x64(2,2,3,3)));
    __m256i g67 = _mm256_unpacklo_epi16(_mm256_permute4x64_epi64(f67, PERM4x64(0,0,1,1)),
                                        _mm256_permute4x64_epi64(f67, PERM4x64(2,2,3,3)));
    g23 = btf16_avx2(cospi_p32_p32, cospi_p32_m32, g23, 12);
    g67 = btf16_avx2(cospi_p32_p32, cospi_p32_m32, g67, 12);

    // stage 7
    __m256i h02 = _mm256_permute2x128_si256(f01, g67, PERM2x128(0,2));
    __m256i h46 = _mm256_permute2x128_si256(g23, f45, PERM2x128(1,3));
    __m256i h13 = _mm256_permute2x128_si256(g23, f45, PERM2x128(2,0));
    __m256i h57 = _mm256_permute2x128_si256(f01, g67, PERM2x128(3,1));

    const __m256i final_rounding = _mm256_set1_epi16((1 << final_shift) >> 1);
    h02 = _mm256_srai_epi16(_mm256_add_epi16(final_rounding, h02), final_shift);
    h46 = _mm256_srai_epi16(_mm256_add_epi16(final_rounding, h46), final_shift);
    h13 = _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, h13), final_shift);
    h57 = _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, h57), final_shift);

    __m256i *pout = (__m256i *)out;
    pout[0] = _mm256_permute2x128_si256(h02, h13, PERM2x128(0,2));
    pout[1] = _mm256_permute2x128_si256(h02, h13, PERM2x128(1,3));
    pout[2] = _mm256_permute2x128_si256(h46, h57, PERM2x128(0,2));
    pout[3] = _mm256_permute2x128_si256(h46, h57, PERM2x128(1,3));
}

static void idct16x16_avx2_16s(const short *in_, short *out, int final_shift) {
    const short *cospi = cospi_arr_data;
    __m256i u[16], v[16];

    const __m256i cospi_p04_p60 = mm256_set2_epi16( cospi[ 4],  cospi[60]);
    const __m256i cospi_p60_m04 = mm256_set2_epi16( cospi[60], -cospi[ 4]);
    const __m256i cospi_p36_p28 = mm256_set2_epi16( cospi[36],  cospi[28]);
    const __m256i cospi_p28_m36 = mm256_set2_epi16( cospi[28], -cospi[36]);
    const __m256i cospi_p20_p44 = mm256_set2_epi16( cospi[20],  cospi[44]);
    const __m256i cospi_p44_m20 = mm256_set2_epi16( cospi[44], -cospi[20]);
    const __m256i cospi_p52_p12 = mm256_set2_epi16( cospi[52],  cospi[12]);
    const __m256i cospi_p12_m52 = mm256_set2_epi16( cospi[12], -cospi[52]);
    const __m256i cospi_p08_p56 = mm256_set2_epi16( cospi[ 8],  cospi[56]);
    const __m256i cospi_p56_m08 = mm256_set2_epi16( cospi[56], -cospi[ 8]);
    const __m256i cospi_p40_p24 = mm256_set2_epi16( cospi[40],  cospi[24]);
    const __m256i cospi_p24_m40 = mm256_set2_epi16( cospi[24], -cospi[40]);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p16_p48 = mm256_set2_epi16( cospi[16],  cospi[48]);
    const __m256i cospi_p48_m16 = mm256_set2_epi16( cospi[48], -cospi[16]);
    const __m256i cospi_m16_p48 = mm256_set2_epi16(-cospi[16],  cospi[48]);
    const __m256i cospi_p48_p16 = mm256_set2_epi16( cospi[48],  cospi[16]);
    const __m256i cospi_m48_m16 = mm256_set2_epi16(-cospi[48], -cospi[16]);

    __m256i in[16];
    transpose_16x16_16s((const __m256i *)in_, in, 1);

    // stage 2
    btf_avx2(cospi_p04_p60, cospi_p60_m04, in[ 1], in[15], 12, &v[15], &v[ 8]);
    btf_avx2(cospi_p36_p28, cospi_p28_m36, in[ 9], in[ 7], 12, &v[14], &v[ 9]);
    btf_avx2(cospi_p20_p44, cospi_p44_m20, in[ 5], in[11], 12, &v[13], &v[10]);
    btf_avx2(cospi_p52_p12, cospi_p12_m52, in[13], in[ 3], 12, &v[12], &v[11]);

    // stage 3
    btf_avx2(cospi_p08_p56, cospi_p56_m08, in[ 2], in[14], 12, &u[ 7], &u[ 4]);
    btf_avx2(cospi_p40_p24, cospi_p24_m40, in[10], in[ 6], 12, &u[ 6], &u[ 5]);
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
    btf_avx2(cospi_p32_p32, cospi_p32_m32, in[ 0], in[ 8], 12, &v[ 0], &v[ 1]);
    btf_avx2(cospi_p16_p48, cospi_p48_m16, in[ 4], in[12], 12, &v[ 3], &v[ 2]);
    btf_avx2(cospi_m16_p48, cospi_p48_p16,  u[ 9],  u[14], 12, &v[ 9], &v[14]);
    btf_avx2(cospi_m48_m16, cospi_m16_p48,  u[10],  u[13], 12, &v[10], &v[13]);

    // stage 5
    u[0] = _mm256_add_epi16(v[0], v[3]);
    u[1] = _mm256_add_epi16(v[1], v[2]);
    u[2] = _mm256_sub_epi16(v[1], v[2]);
    u[3] = _mm256_sub_epi16(v[0], v[3]);
    u[4] = v[4];
    btf_avx2(cospi_p32_p32, cospi_p32_m32, v[6], v[5], 12, &u[6], &u[5]);
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
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[13], u[10], 12, &v[13], &v[10]);
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[12], u[11], 12, &v[12], &v[11]);
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

    storea_si256(out +  0 * 16, _mm256_srai_epi16(u[ 0], final_shift));
    storea_si256(out +  1 * 16, _mm256_srai_epi16(u[ 1], final_shift));
    storea_si256(out +  2 * 16, _mm256_srai_epi16(u[ 2], final_shift));
    storea_si256(out +  3 * 16, _mm256_srai_epi16(u[ 3], final_shift));
    storea_si256(out +  4 * 16, _mm256_srai_epi16(u[ 4], final_shift));
    storea_si256(out +  5 * 16, _mm256_srai_epi16(u[ 5], final_shift));
    storea_si256(out +  6 * 16, _mm256_srai_epi16(u[ 6], final_shift));
    storea_si256(out +  7 * 16, _mm256_srai_epi16(u[ 7], final_shift));
    storea_si256(out +  8 * 16, _mm256_srai_epi16(u[ 8], final_shift));
    storea_si256(out +  9 * 16, _mm256_srai_epi16(u[ 9], final_shift));
    storea_si256(out + 10 * 16, _mm256_srai_epi16(u[10], final_shift));
    storea_si256(out + 11 * 16, _mm256_srai_epi16(u[11], final_shift));
    storea_si256(out + 12 * 16, _mm256_srai_epi16(u[12], final_shift));
    storea_si256(out + 13 * 16, _mm256_srai_epi16(u[13], final_shift));
    storea_si256(out + 14 * 16, _mm256_srai_epi16(u[14], final_shift));
    storea_si256(out + 15 * 16, _mm256_srai_epi16(u[15], final_shift));
}

static void iadst16x16_avx2_16s(const short *in_, short *out, int final_shift)
{
    const short *cospi = cospi_arr_data;
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
    const __m256i cospi_p08_p56 = mm256_set2_epi16( cospi[ 8],  cospi[56]);
    const __m256i cospi_p56_m08 = mm256_set2_epi16( cospi[56], -cospi[ 8]);
    const __m256i cospi_p40_p24 = mm256_set2_epi16( cospi[40],  cospi[24]);
    const __m256i cospi_p24_m40 = mm256_set2_epi16( cospi[24], -cospi[40]);
    const __m256i cospi_m56_p08 = mm256_set2_epi16(-cospi[56],  cospi[ 8]);
    const __m256i cospi_m24_p40 = mm256_set2_epi16(-cospi[24],  cospi[40]);
    const __m256i cospi_p16_p48 = mm256_set2_epi16( cospi[16],  cospi[48]);
    const __m256i cospi_p48_m16 = mm256_set2_epi16( cospi[48], -cospi[16]);
    const __m256i cospi_m48_p16 = mm256_set2_epi16(-cospi[48],  cospi[16]);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);

    __m256i u[16], v[16], t[16];

    __m256i in[16];
    transpose_16x16_16s((const __m256i *)in_, in, 1);

    // stage 0
    // stage 1
    // stage 2
    btf_avx2(cospi_p02_p62, cospi_p62_m02, in[15], in[ 0], 12, &v[ 0], &v[ 1]);
    btf_avx2(cospi_p10_p54, cospi_p54_m10, in[13], in[ 2], 12, &v[ 2], &v[ 3]);
    btf_avx2(cospi_p18_p46, cospi_p46_m18, in[11], in[ 4], 12, &v[ 4], &v[ 5]);
    btf_avx2(cospi_p26_p38, cospi_p38_m26, in[ 9], in[ 6], 12, &v[ 6], &v[ 7]);
    btf_avx2(cospi_p34_p30, cospi_p30_m34, in[ 7], in[ 8], 12, &v[ 8], &v[ 9]);
    btf_avx2(cospi_p42_p22, cospi_p22_m42, in[ 5], in[10], 12, &v[10], &v[11]);
    btf_avx2(cospi_p50_p14, cospi_p14_m50, in[ 3], in[12], 12, &v[12], &v[13]);
    btf_avx2(cospi_p58_p06, cospi_p06_m58, in[ 1], in[14], 12, &v[14], &v[15]);

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
    // stage 5
    t[ 0] = _mm256_add_epi16(u[ 0], u[ 4]);
    t[ 4] = _mm256_sub_epi16(u[ 0], u[ 4]);
    t[ 1] = _mm256_add_epi16(u[ 1], u[ 5]);
    t[ 5] = _mm256_sub_epi16(u[ 1], u[ 5]);
    t[ 2] = _mm256_add_epi16(u[ 2], u[ 6]);
    t[ 6] = _mm256_sub_epi16(u[ 2], u[ 6]);
    t[ 3] = _mm256_add_epi16(u[ 3], u[ 7]);
    t[ 7] = _mm256_sub_epi16(u[ 3], u[ 7]);
    // stage 6
    btf_avx2(cospi_p16_p48, cospi_p48_m16, t[ 4], t[ 5], 12, &v[ 4], &v[ 5]);
    btf_avx2(cospi_m48_p16, cospi_p16_p48, t[ 6], t[ 7], 12, &v[ 6], &v[ 7]);
    // stage 7
    u[ 0] = _mm256_add_epi16(t[ 0], t[ 2]);
    u[ 2] = _mm256_sub_epi16(t[ 0], t[ 2]);
    u[ 1] = _mm256_add_epi16(t[ 1], t[ 3]);
    u[ 3] = _mm256_sub_epi16(t[ 1], t[ 3]);
    u[ 4] = _mm256_add_epi16(v[ 4], v[ 6]);
    u[ 6] = _mm256_sub_epi16(v[ 4], v[ 6]);
    u[ 5] = _mm256_add_epi16(v[ 5], v[ 7]);
    u[ 7] = _mm256_sub_epi16(v[ 5], v[ 7]);
    // stage 8
    v[ 0] = u[ 0];
    v[ 1] = u[ 1];
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[2], u[3], 12, &v[2], &v[3]);
    v[ 4] = u[ 4];
    v[ 5] = u[ 5];
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[6], u[7], 12, &v[6], &v[7]);

    // stage 4
    btf_avx2(cospi_p08_p56, cospi_p56_m08, u[ 8], u[ 9], 12, &v[ 8], &v[ 9]);
    btf_avx2(cospi_p40_p24, cospi_p24_m40, u[10], u[11], 12, &v[10], &v[11]);
    btf_avx2(cospi_m56_p08, cospi_p08_p56, u[12], u[13], 12, &v[12], &v[13]);
    btf_avx2(cospi_m24_p40, cospi_p40_p24, u[14], u[15], 12, &v[14], &v[15]);
    // stage 5
    u[ 8] = _mm256_add_epi16(v[ 8], v[12]);
    u[12] = _mm256_sub_epi16(v[ 8], v[12]);
    u[ 9] = _mm256_add_epi16(v[ 9], v[13]);
    u[13] = _mm256_sub_epi16(v[ 9], v[13]);
    u[10] = _mm256_add_epi16(v[10], v[14]);
    u[14] = _mm256_sub_epi16(v[10], v[14]);
    u[11] = _mm256_add_epi16(v[11], v[15]);
    u[15] = _mm256_sub_epi16(v[11], v[15]);
    // stage 6
    v[ 8] = u[ 8];
    v[ 9] = u[ 9];
    v[10] = u[10];
    v[11] = u[11];
    btf_avx2(cospi_p16_p48, cospi_p48_m16, u[12], u[13], 12, &v[12], &v[13]);
    btf_avx2(cospi_m48_p16, cospi_p16_p48, u[14], u[15], 12, &v[14], &v[15]);
    // stage 7
    u[ 8] = _mm256_add_epi16(v[ 8], v[10]);
    u[10] = _mm256_sub_epi16(v[ 8], v[10]);
    u[ 9] = _mm256_add_epi16(v[ 9], v[11]);
    u[11] = _mm256_sub_epi16(v[ 9], v[11]);
    u[12] = _mm256_add_epi16(v[12], v[14]);
    u[14] = _mm256_sub_epi16(v[12], v[14]);
    u[13] = _mm256_add_epi16(v[13], v[15]);
    u[15] = _mm256_sub_epi16(v[13], v[15]);
    // stage 8
    v[ 8] = u[ 8];
    v[ 9] = u[ 9];
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[10], u[11], 12, &v[10], &v[11]);
    v[12] = u[12];
    v[13] = u[13];
    btf_avx2(cospi_p32_p32, cospi_p32_m32, u[14], u[15], 12, &v[14], &v[15]);

    const __m256i final_rounding = _mm256_set1_epi16((1 << final_shift) >> 1);

    // stage 9
    storea_si256(out +  0 * 16, _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[ 0]), final_shift));
    storea_si256(out +  1 * 16, _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[ 8]), final_shift)); // change sign
    storea_si256(out +  2 * 16, _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[12]), final_shift));
    storea_si256(out +  3 * 16, _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[ 4]), final_shift));
    storea_si256(out +  4 * 16, _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[ 6]), final_shift));
    storea_si256(out +  5 * 16, _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[14]), final_shift));
    storea_si256(out +  6 * 16, _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[10]), final_shift));
    storea_si256(out +  7 * 16, _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[ 2]), final_shift));
    storea_si256(out +  8 * 16, _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[ 3]), final_shift));
    storea_si256(out +  9 * 16, _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[11]), final_shift));
    storea_si256(out + 10 * 16, _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[15]), final_shift));
    storea_si256(out + 11 * 16, _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[ 7]), final_shift));
    storea_si256(out + 12 * 16, _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[ 5]), final_shift));
    storea_si256(out + 13 * 16, _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[13]), final_shift));
    storea_si256(out + 14 * 16, _mm256_srai_epi16(_mm256_add_epi16(final_rounding, v[ 9]), final_shift));
    storea_si256(out + 15 * 16, _mm256_srai_epi16(_mm256_sub_epi16(final_rounding, v[ 1]), final_shift));
}

static void iidtx4x4_avx2(const int16_t *input, int16_t *output)
{
    const int16_t scale_fractional = (NewSqrt2 - (1 << NewSqrt2Bits));
    const __m256i scale = _mm256_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
    const __m256i scale2 = _mm256_set1_epi16(2048);

    __m256i x = loada_si256(input);
    x = _mm256_adds_epi16(x, _mm256_mulhrs_epi16(x, scale));
    x = _mm256_adds_epi16(x, _mm256_mulhrs_epi16(x, scale));
    x = _mm256_mulhrs_epi16(x, scale2);
    storea_si256(output, x);
}

static inline void iidtx8x8_avx2(const int16_t *input, int16_t *output)
{
    const __m256i four = _mm256_set1_epi16(4);
    storea_si256(output + 0 * 8, _mm256_srai_epi16(_mm256_add_epi16(loada_si256(input + 0 * 8), four), 3));
    storea_si256(output + 2 * 8, _mm256_srai_epi16(_mm256_add_epi16(loada_si256(input + 2 * 8), four), 3));
    storea_si256(output + 4 * 8, _mm256_srai_epi16(_mm256_add_epi16(loada_si256(input + 4 * 8), four), 3));
    storea_si256(output + 6 * 8, _mm256_srai_epi16(_mm256_add_epi16(loada_si256(input + 6 * 8), four), 3));
}

static inline void iidtx16x16_avx2(const int16_t *input, int16_t *output)
{
    const __m256i one = _mm256_set1_epi16(1);
    const __m256i offset = _mm256_set1_epi32(8);
    const __m256i scale_row = _mm256_set1_epi32((2 * 5793) | (1 << 27) | (1 << 29));
    const __m256i scale_col = _mm256_set1_epi32((2 * 5793) | (1 << 27));

    for (int i = 0; i < 16; ++i, input += 16, output += 16) {
        const __m256i src = loada_si256(input);
        __m256i lo = _mm256_unpacklo_epi16(src, one);
        __m256i hi = _mm256_unpackhi_epi16(src, one);
        lo = _mm256_madd_epi16(lo, scale_row);
        hi = _mm256_madd_epi16(hi, scale_row);
        lo = _mm256_srai_epi32(lo, 14);
        hi = _mm256_srai_epi32(hi, 14);
        __m256i x = _mm256_packs_epi32(lo, hi);

        lo = _mm256_unpacklo_epi16(x, one);
        hi = _mm256_unpackhi_epi16(x, one);
        lo = _mm256_madd_epi16(lo, scale_col);
        hi = _mm256_madd_epi16(hi, scale_col);
        lo = _mm256_srai_epi32(lo, 12);
        hi = _mm256_srai_epi32(hi, 12);
        lo = _mm256_add_epi32(lo, offset);
        hi = _mm256_add_epi32(hi, offset);
        lo = _mm256_srai_epi32(lo, 4);
        hi = _mm256_srai_epi32(hi, 4);
        x = _mm256_packs_epi32(lo, hi);
        storea_si256(output, x);
    }
}

static inline void iidtx32x32_avx2(const int16_t *input, int16_t *output)
{
    const __m256i two = _mm256_set1_epi16(2);
    for (int32_t i = 0; i < 32; ++i) {
        __m256i x = loada_si256(input + i * 32 + 0);
        __m256i y = loada_si256(input + i * 32 + 16);
        x = _mm256_srai_epi16(_mm256_add_epi16(x, two), 2);
        y = _mm256_srai_epi16(_mm256_add_epi16(y, two), 2);
        storea_si256(output + i * 32 + 0, x);
        storea_si256(output + i * 32 + 16, y);
    }
}

static void av1_inv_txfm2d_4x4_sse4(const short *coeff, short *output, int tx_type)
{
    if (tx_type == IDTX)
        return iidtx4x4_avx2(coeff, output);

    assert((size_t(coeff) & 15) == 0);
    assert((size_t(output) & 15) == 0);
    assert(inv_txfm_shift_ls[TX_4X4][0] == 0);
    assert(inv_txfm_shift_ls[TX_4X4][1] == -4);

    __m128i *pout = (__m128i *)output;
    pout[0] = loada_si128(coeff + 0);
    pout[1] = loada_si128(coeff + 8);

    if (tx_type == DCT_DCT) {
        idct4x4_sse4_16s(output);
        idct4x4_sse4_16s(output);
    } else if (tx_type == ADST_DCT) {
        idct4x4_sse4_16s(output);
        iadst4x4_sse4_16s(output);
    } else if (tx_type == DCT_ADST) {
        iadst4x4_sse4_16s(output);
        idct4x4_sse4_16s(output);
    } else if (tx_type == ADST_ADST) {
        iadst4x4_sse4_16s(output);
        iadst4x4_sse4_16s(output);
    }

    const __m128i eight = _mm_set1_epi16(8);
    pout[0] = _mm_srai_epi16(_mm_add_epi16(pout[0], eight), 4);
    pout[1] = _mm_srai_epi16(_mm_add_epi16(pout[1], eight), 4);
}

static void av1_inv_txfm2d_8x8_avx2(const short *coeff, short *out, int tx_type)
{
    if (tx_type == IDTX)
        return iidtx8x8_avx2(coeff, out);

    const int shift0 = -inv_txfm_shift_ls[TX_8X8][0];
    const int shift1 = -inv_txfm_shift_ls[TX_8X8][1];

    ALIGN_DECL(32) short intermediate[8 * 8];

    if (tx_type == DCT_DCT) {
        idct8x8_avx2_16s(coeff, intermediate, shift0);
        idct8x8_avx2_16s(intermediate, out, shift1);
    } else if (tx_type == ADST_DCT) {
        idct8x8_avx2_16s(coeff, intermediate, shift0);
        iadst8x8_avx2_16s(intermediate, out, shift1);
    } else if (tx_type == DCT_ADST) {
        iadst8x8_avx2_16s(coeff, intermediate, shift0);
        idct8x8_avx2_16s(intermediate, out, shift1);
    } else { assert(tx_type == ADST_ADST);
        iadst8x8_avx2_16s(coeff, intermediate, shift0);
        iadst8x8_avx2_16s(intermediate, out, shift1);
    }
}

static void av1_inv_txfm2d_16x16_avx2(const int16_t *coeff, short *out, int tx_type)
{
    if (tx_type == IDTX)
        return iidtx16x16_avx2(coeff, out);

    const int shift0 = -inv_txfm_shift_ls[TX_16X16][0];
    const int shift1 = -inv_txfm_shift_ls[TX_16X16][1];

    ALIGN_DECL(32) short intermediate[16 * 16];

    if (tx_type == DCT_DCT) {
        idct16x16_avx2_16s(coeff, intermediate, shift0);
        idct16x16_avx2_16s(intermediate, out, shift1);
    } else if (tx_type == ADST_DCT) {
        idct16x16_avx2_16s(coeff, intermediate, shift0);
        iadst16x16_avx2_16s(intermediate, out, shift1);
    } else if (tx_type == DCT_ADST) {
        iadst16x16_avx2_16s(coeff, intermediate, shift0);
        idct16x16_avx2_16s(intermediate, out, shift1);
    } else if (tx_type == ADST_ADST) {
        iadst16x16_avx2_16s(coeff, intermediate, shift0);
        iadst16x16_avx2_16s(intermediate, out, shift1);
    }
}

static inline void transpose_16x16_(const __m256i *in, __m256i *out)
{
    __m256i u[16], v[16];
    u[ 0] = _mm256_unpacklo_epi16(in[ 0 * 2], in[ 1 * 2]);
    u[ 1] = _mm256_unpacklo_epi16(in[ 2 * 2], in[ 3 * 2]);
    u[ 2] = _mm256_unpacklo_epi16(in[ 4 * 2], in[ 5 * 2]);
    u[ 3] = _mm256_unpacklo_epi16(in[ 6 * 2], in[ 7 * 2]);
    u[ 4] = _mm256_unpacklo_epi16(in[ 8 * 2], in[ 9 * 2]);
    u[ 5] = _mm256_unpacklo_epi16(in[10 * 2], in[11 * 2]);
    u[ 6] = _mm256_unpacklo_epi16(in[12 * 2], in[13 * 2]);
    u[ 7] = _mm256_unpacklo_epi16(in[14 * 2], in[15 * 2]);
    u[ 8] = _mm256_unpackhi_epi16(in[ 0 * 2], in[ 1 * 2]);
    u[ 9] = _mm256_unpackhi_epi16(in[ 2 * 2], in[ 3 * 2]);
    u[10] = _mm256_unpackhi_epi16(in[ 4 * 2], in[ 5 * 2]);
    u[11] = _mm256_unpackhi_epi16(in[ 6 * 2], in[ 7 * 2]);
    u[12] = _mm256_unpackhi_epi16(in[ 8 * 2], in[ 9 * 2]);
    u[13] = _mm256_unpackhi_epi16(in[10 * 2], in[11 * 2]);
    u[14] = _mm256_unpackhi_epi16(in[12 * 2], in[13 * 2]);
    u[15] = _mm256_unpackhi_epi16(in[14 * 2], in[15 * 2]);

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

    out[ 0] = _mm256_permute2x128_si256(u[ 0], u[ 1], PERM2x128(0,2));
    out[ 1] = _mm256_permute2x128_si256(u[ 2], u[ 3], PERM2x128(0,2));
    out[ 2] = _mm256_permute2x128_si256(u[ 4], u[ 5], PERM2x128(0,2));
    out[ 3] = _mm256_permute2x128_si256(u[ 6], u[ 7], PERM2x128(0,2));
    out[ 4] = _mm256_permute2x128_si256(u[ 8], u[ 9], PERM2x128(0,2));
    out[ 5] = _mm256_permute2x128_si256(u[10], u[11], PERM2x128(0,2));
    out[ 6] = _mm256_permute2x128_si256(u[12], u[13], PERM2x128(0,2));
    out[ 7] = _mm256_permute2x128_si256(u[14], u[15], PERM2x128(0,2));
    out[ 8] = _mm256_permute2x128_si256(u[ 0], u[ 1], PERM2x128(1,3));
    out[ 9] = _mm256_permute2x128_si256(u[ 2], u[ 3], PERM2x128(1,3));
    out[10] = _mm256_permute2x128_si256(u[ 4], u[ 5], PERM2x128(1,3));
    out[11] = _mm256_permute2x128_si256(u[ 6], u[ 7], PERM2x128(1,3));
    out[12] = _mm256_permute2x128_si256(u[ 8], u[ 9], PERM2x128(1,3));
    out[13] = _mm256_permute2x128_si256(u[10], u[11], PERM2x128(1,3));
    out[14] = _mm256_permute2x128_si256(u[12], u[13], PERM2x128(1,3));
    out[15] = _mm256_permute2x128_si256(u[14], u[15], PERM2x128(1,3));
}

static void av1_inv_txfm2d_32x32_avx2(const short *coeff, short *out)
{
    const short *cospi = cospi_arr_data;
    const __m256i cospi_p02_p62 = mm256_set2_epi16( cospi[ 2],  cospi[62]);
    const __m256i cospi_p62_m02 = mm256_set2_epi16( cospi[62], -cospi[ 2]);
    const __m256i cospi_p34_p30 = mm256_set2_epi16( cospi[34],  cospi[30]);
    const __m256i cospi_p30_m34 = mm256_set2_epi16( cospi[30], -cospi[34]);
    const __m256i cospi_p18_p46 = mm256_set2_epi16( cospi[18],  cospi[46]);
    const __m256i cospi_p46_m18 = mm256_set2_epi16( cospi[46], -cospi[18]);
    const __m256i cospi_p50_p14 = mm256_set2_epi16( cospi[50],  cospi[14]);
    const __m256i cospi_p14_m50 = mm256_set2_epi16( cospi[14], -cospi[50]);
    const __m256i cospi_p10_p54 = mm256_set2_epi16( cospi[10],  cospi[54]);
    const __m256i cospi_p54_m10 = mm256_set2_epi16( cospi[54], -cospi[10]);
    const __m256i cospi_p42_p22 = mm256_set2_epi16( cospi[42],  cospi[22]);
    const __m256i cospi_p22_m42 = mm256_set2_epi16( cospi[22], -cospi[42]);
    const __m256i cospi_p26_p38 = mm256_set2_epi16( cospi[26],  cospi[38]);
    const __m256i cospi_p38_m26 = mm256_set2_epi16( cospi[38], -cospi[26]);
    const __m256i cospi_p58_p06 = mm256_set2_epi16( cospi[58],  cospi[ 6]);
    const __m256i cospi_p06_m58 = mm256_set2_epi16( cospi[ 6], -cospi[58]);
    const __m256i cospi_p04_p60 = mm256_set2_epi16( cospi[ 4],  cospi[60]);
    const __m256i cospi_p60_m04 = mm256_set2_epi16( cospi[60], -cospi[ 4]);
    const __m256i cospi_p36_p28 = mm256_set2_epi16( cospi[36],  cospi[28]);
    const __m256i cospi_p28_m36 = mm256_set2_epi16( cospi[28], -cospi[36]);
    const __m256i cospi_p20_p44 = mm256_set2_epi16( cospi[20],  cospi[44]);
    const __m256i cospi_p44_m20 = mm256_set2_epi16( cospi[44], -cospi[20]);
    const __m256i cospi_p52_p12 = mm256_set2_epi16( cospi[52],  cospi[12]);
    const __m256i cospi_p12_m52 = mm256_set2_epi16( cospi[12], -cospi[52]);
    const __m256i cospi_p08_p56 = mm256_set2_epi16( cospi[ 8],  cospi[56]);
    const __m256i cospi_p56_m08 = mm256_set2_epi16( cospi[56], -cospi[ 8]);
    const __m256i cospi_p40_p24 = mm256_set2_epi16( cospi[40],  cospi[24]);
    const __m256i cospi_p24_m40 = mm256_set2_epi16( cospi[24], -cospi[40]);
    const __m256i cospi_m08_p56 = mm256_set2_epi16(-cospi[ 8],  cospi[56]);
    const __m256i cospi_p56_p08 = mm256_set2_epi16( cospi[56],  cospi[ 8]);
    const __m256i cospi_m56_m08 = mm256_set2_epi16(-cospi[56], -cospi[ 8]);
    const __m256i cospi_m40_p24 = mm256_set2_epi16(-cospi[40],  cospi[24]);
    const __m256i cospi_p24_p40 = mm256_set2_epi16( cospi[24],  cospi[40]);
    const __m256i cospi_m24_m40 = mm256_set2_epi16(-cospi[24], -cospi[40]);
    const __m256i cospi_p32_p32 = mm256_set2_epi16( cospi[32],  cospi[32]);
    const __m256i cospi_p32_m32 = mm256_set2_epi16( cospi[32], -cospi[32]);
    const __m256i cospi_p16_p48 = mm256_set2_epi16( cospi[16],  cospi[48]);
    const __m256i cospi_p48_m16 = mm256_set2_epi16( cospi[48], -cospi[16]);
    const __m256i cospi_m16_p48 = mm256_set2_epi16(-cospi[16],  cospi[48]);
    const __m256i cospi_p48_p16 = mm256_set2_epi16( cospi[48],  cospi[16]);
    const __m256i cospi_m48_m16 = mm256_set2_epi16(-cospi[48], -cospi[16]);

    __m256i a[32], b[32], c[32], d[32], e[32], f[32], g[32], h[32], i[32], intermediate[64];

    for (int rows = 1; rows >= 0; rows--) {
        const int shift = rows ? 2 : 4;
        const __m256i rounding = _mm256_set1_epi16((1 << shift) >> 1);
        const __m256i *pin  = rows ? (const __m256i *)coeff : intermediate;
        __m256i *pout = rows ? intermediate : (__m256i *)out;

        for (int col = 0; col < 2; ++col) {
            transpose_16x16_(pin + 0 + 32 * col, a +  0);
            transpose_16x16_(pin + 1 + 32 * col, a + 16);

            // first 16 rows
            // stage 0
            // stage 1
            // stage 2
            // stage 3
            btf_avx2(cospi_p04_p60, cospi_p60_m04, a[ 2], a[30], 12, &b[15], &b[ 8]);
            btf_avx2(cospi_p36_p28, cospi_p28_m36, a[18], a[14], 12, &b[14], &b[ 9]);
            btf_avx2(cospi_p20_p44, cospi_p44_m20, a[10], a[22], 12, &b[13], &b[10]);
            btf_avx2(cospi_p52_p12, cospi_p12_m52, a[26], a[ 6], 12, &b[12], &b[11]);
            // stage 4
            btf_avx2(cospi_p08_p56, cospi_p56_m08, a[ 4], a[28], 12, &c[ 7], &c[4]);
            btf_avx2(cospi_p40_p24, cospi_p24_m40, a[20], a[12], 12, &c[ 6], &c[5]);
            c[ 8] = _mm256_add_epi16(b[ 8], b[ 9]);
            c[ 9] = _mm256_sub_epi16(b[ 8], b[ 9]);
            c[10] = _mm256_sub_epi16(b[11], b[10]);
            c[11] = _mm256_add_epi16(b[10], b[11]);
            c[12] = _mm256_add_epi16(b[12], b[13]);
            c[13] = _mm256_sub_epi16(b[12], b[13]);
            c[14] = _mm256_sub_epi16(b[15], b[14]);
            c[15] = _mm256_add_epi16(b[14], b[15]);
            // stage 5
            btf_avx2(cospi_p32_p32, cospi_p32_m32, a[ 0], a[16], 12, &d[ 0], &d[ 1]);
            btf_avx2(cospi_p16_p48, cospi_p48_m16, a[ 8], a[24], 12, &d[ 3], &d[ 2]);
            d[ 4] = _mm256_add_epi16(c[4], c[5]);
            d[ 5] = _mm256_sub_epi16(c[4], c[5]);
            d[ 6] = _mm256_sub_epi16(c[7], c[6]);
            d[ 7] = _mm256_add_epi16(c[6], c[7]);
            btf_avx2(cospi_m16_p48, cospi_p48_p16, c[ 9], c[14], 12, &d[ 9], &d[14]);
            btf_avx2(cospi_m48_m16, cospi_m16_p48, c[10], c[13], 12, &d[10], &d[13]);
            // stage 6
            e[ 0] = _mm256_add_epi16(d[0], d[3]);
            e[ 1] = _mm256_add_epi16(d[1], d[2]);
            e[ 2] = _mm256_sub_epi16(d[1], d[2]);
            e[ 3] = _mm256_sub_epi16(d[0], d[3]);
            btf_avx2(cospi_p32_p32, cospi_p32_m32, d[ 6],  d[ 5], 12, &e[ 6], &e[ 5]);
            e[ 8] = _mm256_add_epi16(c[ 8], c[11]);
            e[ 9] = _mm256_add_epi16(d[ 9], d[10]);
            e[10] = _mm256_sub_epi16(d[ 9], d[10]);
            e[11] = _mm256_sub_epi16(c[ 8], c[11]);
            e[12] = _mm256_sub_epi16(c[15], c[12]);
            e[13] = _mm256_sub_epi16(d[14], d[13]);
            e[14] = _mm256_add_epi16(d[13], d[14]);
            e[15] = _mm256_add_epi16(c[12], c[15]);
            // stage 7
            f[ 0] = _mm256_add_epi16(e[0], d[7]);
            f[ 1] = _mm256_add_epi16(e[1], e[6]);
            f[ 2] = _mm256_add_epi16(e[2], e[5]);
            f[ 3] = _mm256_add_epi16(e[3], d[4]);
            f[ 4] = _mm256_sub_epi16(e[3], d[4]);
            f[ 5] = _mm256_sub_epi16(e[2], e[5]);
            f[ 6] = _mm256_sub_epi16(e[1], e[6]);
            f[ 7] = _mm256_sub_epi16(e[0], d[7]);
            btf_avx2(cospi_p32_p32, cospi_p32_m32, e[13], e[10], 12, &f[13], &f[10]);
            btf_avx2(cospi_p32_p32, cospi_p32_m32, e[12], e[11], 12, &f[12], &f[11]);
            // stage 8
            g[ 0] = _mm256_add_epi16(f[0], e[15]);
            g[ 1] = _mm256_add_epi16(f[1], e[14]);
            g[ 2] = _mm256_add_epi16(f[2], f[13]);
            g[ 3] = _mm256_add_epi16(f[3], f[12]);
            g[ 4] = _mm256_add_epi16(f[4], f[11]);
            g[ 5] = _mm256_add_epi16(f[5], f[10]);
            g[ 6] = _mm256_add_epi16(f[6], e[ 9]);
            g[ 7] = _mm256_add_epi16(f[7], e[ 8]);
            g[ 8] = _mm256_sub_epi16(f[7], e[ 8]);
            g[ 9] = _mm256_sub_epi16(f[6], e[ 9]);
            g[10] = _mm256_sub_epi16(f[5], f[10]);
            g[11] = _mm256_sub_epi16(f[4], f[11]);
            g[12] = _mm256_sub_epi16(f[3], f[12]);
            g[13] = _mm256_sub_epi16(f[2], f[13]);
            g[14] = _mm256_sub_epi16(f[1], e[14]);
            g[15] = _mm256_sub_epi16(f[0], e[15]);

            h[ 0] = _mm256_add_epi16(g[ 0], rounding);
            h[ 1] = _mm256_add_epi16(g[ 1], rounding);
            h[ 2] = _mm256_add_epi16(g[ 2], rounding);
            h[ 3] = _mm256_add_epi16(g[ 3], rounding);
            h[ 4] = _mm256_add_epi16(g[ 4], rounding);
            h[ 5] = _mm256_add_epi16(g[ 5], rounding);
            h[ 6] = _mm256_add_epi16(g[ 6], rounding);
            h[ 7] = _mm256_add_epi16(g[ 7], rounding);
            h[ 8] = _mm256_add_epi16(g[ 8], rounding);
            h[ 9] = _mm256_add_epi16(g[ 9], rounding);
            h[10] = _mm256_add_epi16(g[10], rounding);
            h[11] = _mm256_add_epi16(g[11], rounding);
            h[12] = _mm256_add_epi16(g[12], rounding);
            h[13] = _mm256_add_epi16(g[13], rounding);
            h[14] = _mm256_add_epi16(g[14], rounding);
            h[15] = _mm256_add_epi16(g[15], rounding);


            // second 16 rows
            // stage 0
            // stage 1
            // stage 2
            btf_avx2(cospi_p02_p62, cospi_p62_m02, a[ 1], a[31], 12, &b[31], &b[16]);
            btf_avx2(cospi_p34_p30, cospi_p30_m34, a[17], a[15], 12, &b[30], &b[17]);
            btf_avx2(cospi_p18_p46, cospi_p46_m18, a[ 9], a[23], 12, &b[29], &b[18]);
            btf_avx2(cospi_p50_p14, cospi_p14_m50, a[25], a[ 7], 12, &b[28], &b[19]);
            btf_avx2(cospi_p10_p54, cospi_p54_m10, a[ 5], a[27], 12, &b[27], &b[20]);
            btf_avx2(cospi_p42_p22, cospi_p22_m42, a[21], a[11], 12, &b[26], &b[21]);
            btf_avx2(cospi_p26_p38, cospi_p38_m26, a[13], a[19], 12, &b[25], &b[22]);
            btf_avx2(cospi_p58_p06, cospi_p06_m58, a[29], a[ 3], 12, &b[24], &b[23]);
            // stage 3
            c[16] = _mm256_add_epi16(b[16], b[17]);
            c[17] = _mm256_sub_epi16(b[16], b[17]);
            c[18] = _mm256_sub_epi16(b[19], b[18]);
            c[19] = _mm256_add_epi16(b[18], b[19]);
            c[20] = _mm256_add_epi16(b[20], b[21]);
            c[21] = _mm256_sub_epi16(b[20], b[21]);
            c[22] = _mm256_sub_epi16(b[23], b[22]);
            c[23] = _mm256_add_epi16(b[22], b[23]);
            c[24] = _mm256_add_epi16(b[24], b[25]);
            c[25] = _mm256_sub_epi16(b[24], b[25]);
            c[26] = _mm256_sub_epi16(b[27], b[26]);
            c[27] = _mm256_add_epi16(b[26], b[27]);
            c[28] = _mm256_add_epi16(b[28], b[29]);
            c[29] = _mm256_sub_epi16(b[28], b[29]);
            c[30] = _mm256_sub_epi16(b[31], b[30]);
            c[31] = _mm256_add_epi16(b[30], b[31]);
            // stage 4
            btf_avx2(cospi_m08_p56, cospi_p56_p08, c[17], c[30], 12, &d[17], &d[30]);
            btf_avx2(cospi_m56_m08, cospi_m08_p56, c[18], c[29], 12, &d[18], &d[29]);
            btf_avx2(cospi_m40_p24, cospi_p24_p40, c[21], c[26], 12, &d[21], &d[26]);
            btf_avx2(cospi_m24_m40, cospi_m40_p24, c[22], c[25], 12, &d[22], &d[25]);
            // stage 5
            e[16] = _mm256_add_epi16(c[16], c[19]);
            e[17] = _mm256_add_epi16(d[17], d[18]);
            e[18] = _mm256_sub_epi16(d[17], d[18]);
            e[19] = _mm256_sub_epi16(c[16], c[19]);
            e[20] = _mm256_sub_epi16(c[23], c[20]);
            e[21] = _mm256_sub_epi16(d[22], d[21]);
            e[22] = _mm256_add_epi16(d[21], d[22]);
            e[23] = _mm256_add_epi16(c[20], c[23]);
            e[24] = _mm256_add_epi16(c[24], c[27]);
            e[25] = _mm256_add_epi16(d[25], d[26]);
            e[26] = _mm256_sub_epi16(d[25], d[26]);
            e[27] = _mm256_sub_epi16(c[24], c[27]);
            e[28] = _mm256_sub_epi16(c[31], c[28]);
            e[29] = _mm256_sub_epi16(d[30], d[29]);
            e[30] = _mm256_add_epi16(d[29], d[30]);
            e[31] = _mm256_add_epi16(c[28], c[31]);
            // stage 6
            btf_avx2(cospi_m16_p48, cospi_p48_p16, e[18], e[29], 12, &f[18], &f[29]);
            btf_avx2(cospi_m16_p48, cospi_p48_p16, e[19], e[28], 12, &f[19], &f[28]);
            btf_avx2(cospi_m48_m16, cospi_m16_p48, e[20], e[27], 12, &f[20], &f[27]);
            btf_avx2(cospi_m48_m16, cospi_m16_p48, e[21], e[26], 12, &f[21], &f[26]);
            // stage 7
            g[16] = _mm256_add_epi16(e[16], e[23]);
            g[17] = _mm256_add_epi16(e[17], e[22]);
            g[18] = _mm256_add_epi16(f[18], f[21]);
            g[19] = _mm256_add_epi16(f[19], f[20]);
            g[20] = _mm256_sub_epi16(f[19], f[20]);
            g[21] = _mm256_sub_epi16(f[18], f[21]);
            g[22] = _mm256_sub_epi16(e[17], e[22]);
            g[23] = _mm256_sub_epi16(e[16], e[23]);
            g[24] = _mm256_sub_epi16(e[31], e[24]);
            g[25] = _mm256_sub_epi16(e[30], e[25]);
            g[26] = _mm256_sub_epi16(f[29], f[26]);
            g[27] = _mm256_sub_epi16(f[28], f[27]);
            g[28] = _mm256_add_epi16(f[27], f[28]);
            g[29] = _mm256_add_epi16(f[26], f[29]);
            g[30] = _mm256_add_epi16(e[25], e[30]);
            g[31] = _mm256_add_epi16(e[24], e[31]);
            // stage 8
            btf_avx2(cospi_p32_p32, cospi_p32_m32, g[27], g[20], 12, &h[27], &h[20]);
            btf_avx2(cospi_p32_p32, cospi_p32_m32, g[26], g[21], 12, &h[26], &h[21]);
            btf_avx2(cospi_p32_p32, cospi_p32_m32, g[25], g[22], 12, &h[25], &h[22]);
            btf_avx2(cospi_p32_p32, cospi_p32_m32, g[24], g[23], 12, &h[24], &h[23]);

            // stage 9
            i[ 0] = _mm256_add_epi16(h[ 0], g[31]);
            i[ 1] = _mm256_add_epi16(h[ 1], g[30]);
            i[ 2] = _mm256_add_epi16(h[ 2], g[29]);
            i[ 3] = _mm256_add_epi16(h[ 3], g[28]);
            i[ 4] = _mm256_add_epi16(h[ 4], h[27]);
            i[ 5] = _mm256_add_epi16(h[ 5], h[26]);
            i[ 6] = _mm256_add_epi16(h[ 6], h[25]);
            i[ 7] = _mm256_add_epi16(h[ 7], h[24]);
            i[ 8] = _mm256_add_epi16(h[ 8], h[23]);
            i[ 9] = _mm256_add_epi16(h[ 9], h[22]);
            i[10] = _mm256_add_epi16(h[10], h[21]);
            i[11] = _mm256_add_epi16(h[11], h[20]);
            i[12] = _mm256_add_epi16(h[12], g[19]);
            i[13] = _mm256_add_epi16(h[13], g[18]);
            i[14] = _mm256_add_epi16(h[14], g[17]);
            i[15] = _mm256_add_epi16(h[15], g[16]);
            i[16] = _mm256_sub_epi16(h[15], g[16]);
            i[17] = _mm256_sub_epi16(h[14], g[17]);
            i[18] = _mm256_sub_epi16(h[13], g[18]);
            i[19] = _mm256_sub_epi16(h[12], g[19]);
            i[20] = _mm256_sub_epi16(h[11], h[20]);
            i[21] = _mm256_sub_epi16(h[10], h[21]);
            i[22] = _mm256_sub_epi16(h[ 9], h[22]);
            i[23] = _mm256_sub_epi16(h[ 8], h[23]);
            i[24] = _mm256_sub_epi16(h[ 7], h[24]);
            i[25] = _mm256_sub_epi16(h[ 6], h[25]);
            i[26] = _mm256_sub_epi16(h[ 5], h[26]);
            i[27] = _mm256_sub_epi16(h[ 4], h[27]);
            i[28] = _mm256_sub_epi16(h[ 3], g[28]);
            i[29] = _mm256_sub_epi16(h[ 2], g[29]);
            i[30] = _mm256_sub_epi16(h[ 1], g[30]);
            i[31] = _mm256_sub_epi16(h[ 0], g[31]);

            pout[ 0 * 2 + col] = _mm256_srai_epi16(i[ 0], shift);
            pout[ 1 * 2 + col] = _mm256_srai_epi16(i[ 1], shift);
            pout[ 2 * 2 + col] = _mm256_srai_epi16(i[ 2], shift);
            pout[ 3 * 2 + col] = _mm256_srai_epi16(i[ 3], shift);
            pout[ 4 * 2 + col] = _mm256_srai_epi16(i[ 4], shift);
            pout[ 5 * 2 + col] = _mm256_srai_epi16(i[ 5], shift);
            pout[ 6 * 2 + col] = _mm256_srai_epi16(i[ 6], shift);
            pout[ 7 * 2 + col] = _mm256_srai_epi16(i[ 7], shift);
            pout[ 8 * 2 + col] = _mm256_srai_epi16(i[ 8], shift);
            pout[ 9 * 2 + col] = _mm256_srai_epi16(i[ 9], shift);
            pout[10 * 2 + col] = _mm256_srai_epi16(i[10], shift);
            pout[11 * 2 + col] = _mm256_srai_epi16(i[11], shift);
            pout[12 * 2 + col] = _mm256_srai_epi16(i[12], shift);
            pout[13 * 2 + col] = _mm256_srai_epi16(i[13], shift);
            pout[14 * 2 + col] = _mm256_srai_epi16(i[14], shift);
            pout[15 * 2 + col] = _mm256_srai_epi16(i[15], shift);
            pout[16 * 2 + col] = _mm256_srai_epi16(i[16], shift);
            pout[17 * 2 + col] = _mm256_srai_epi16(i[17], shift);
            pout[18 * 2 + col] = _mm256_srai_epi16(i[18], shift);
            pout[19 * 2 + col] = _mm256_srai_epi16(i[19], shift);
            pout[20 * 2 + col] = _mm256_srai_epi16(i[20], shift);
            pout[21 * 2 + col] = _mm256_srai_epi16(i[21], shift);
            pout[22 * 2 + col] = _mm256_srai_epi16(i[22], shift);
            pout[23 * 2 + col] = _mm256_srai_epi16(i[23], shift);
            pout[24 * 2 + col] = _mm256_srai_epi16(i[24], shift);
            pout[25 * 2 + col] = _mm256_srai_epi16(i[25], shift);
            pout[26 * 2 + col] = _mm256_srai_epi16(i[26], shift);
            pout[27 * 2 + col] = _mm256_srai_epi16(i[27], shift);
            pout[28 * 2 + col] = _mm256_srai_epi16(i[28], shift);
            pout[29 * 2 + col] = _mm256_srai_epi16(i[29], shift);
            pout[30 * 2 + col] = _mm256_srai_epi16(i[30], shift);
            pout[31 * 2 + col] = _mm256_srai_epi16(i[31], shift);
        }
    }
}

namespace AV1PP {

    template <int size, int type> void itransform_av1_avx2(const int16_t *src, int16_t *dst, int pitchDst, int bd)
    {
        bd;
        ALIGN_DECL(32) short buf[32 * 32];
        if (size == 0) {
            av1_inv_txfm2d_4x4_sse4(src, buf, type);

            storel_epi64(dst + 0 * pitchDst, ((__m128i *)buf)[0]);
            storeh_epi64(dst + 1 * pitchDst, ((__m128i *)buf)[0]);
            storel_epi64(dst + 2 * pitchDst, ((__m128i *)buf)[1]);
            storeh_epi64(dst + 3 * pitchDst, ((__m128i *)buf)[1]);

        } else if (size == 1) {
            av1_inv_txfm2d_8x8_avx2(src, buf, type);

            for (int i = 0; i < 8; i++)
                storea_si128(dst + i * pitchDst, ((__m128i *)buf)[i]);

        } else if (size == 2) {
            av1_inv_txfm2d_16x16_avx2(src, buf, type);

            for (int i = 0; i < 16; i++)
                storea_si256(dst + i * pitchDst, ((__m256i *)buf)[i]);

        } else {
            assert(size == 3);
            if (type == IDTX)
                iidtx32x32_avx2(src, buf);
            else
                av1_inv_txfm2d_32x32_avx2(src, buf);

            for (int i = 0; i < 32; i++) {
                storea_si256(dst + i * pitchDst +  0, ((__m256i *)buf)[2 * i + 0]);
                storea_si256(dst + i * pitchDst + 16, ((__m256i *)buf)[2 * i + 1]);
            }
        }
    }

    template void itransform_av1_avx2<0, 0>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<0, 1>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<0, 2>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<0, 3>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<0, 9>(const int16_t*, int16_t*, int, int);
    template void itransform_av1_avx2<1, 0>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<1, 1>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<1, 2>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<1, 3>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<1, 9>(const int16_t*, int16_t*, int, int);
    template void itransform_av1_avx2<2, 0>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<2, 1>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<2, 2>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<2, 3>(const int16_t*,int16_t*,int, int);
    template void itransform_av1_avx2<2, 9>(const int16_t*, int16_t*, int, int);
    template void itransform_av1_avx2<3, 0>(const int16_t*, int16_t*, int, int);
    template void itransform_av1_avx2<3, 9>(const int16_t*, int16_t*, int, int);

    template <int size, int type> void itransform_add_av1_avx2(const int16_t *src, uint8_t *dst, int pitchDst)
    {
        ALIGN_DECL(32) short buf[32 * 32];
        if (size == 0) {
            av1_inv_txfm2d_4x4_sse4(src, buf, type);
            write_buffer_4x4_16s_8u(buf, dst, pitchDst);
        } else if (size == 1) {
            av1_inv_txfm2d_8x8_avx2(src, buf, type);
            write_buffer_8x8_16s_8u(buf, dst, pitchDst);
        } else if (size == 2) {
            av1_inv_txfm2d_16x16_avx2(src, buf, type);
            write_buffer_16x16_16s_8u(buf, dst, pitchDst);
        } else {
            assert(size == 3);
            if (type == IDTX)
                iidtx32x32_avx2(src, buf);
            else
                av1_inv_txfm2d_32x32_avx2(src, buf);
            write_buffer_32x32_16s_8u(buf, dst, pitchDst);
        }
    }

    template void itransform_add_av1_avx2<0, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<0, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<0, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<0, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<0, 9>(const int16_t*,uint8_t*,int);

    template void itransform_add_av1_avx2<1, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<1, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<1, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<1, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<1, 9>(const int16_t*,uint8_t*,int);

    template void itransform_add_av1_avx2<2, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<2, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<2, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<2, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_avx2<2, 9>(const int16_t*, uint8_t*,int);

    template void itransform_add_av1_avx2<3, 0>(const int16_t*, uint8_t*, int);
    template void itransform_add_av1_avx2<3, 9>(const int16_t*, uint8_t*, int);
}; // namespace AV1PP
