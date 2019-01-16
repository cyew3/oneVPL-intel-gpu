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
#include "immintrin.h"
#include "mfx_av1_opts_intrin.h"
#include "mfx_av1_transform9_common_avx2.h"

enum { TX_4X4=0, TX_8X8=1, TX_16X16=2, TX_32X32=3, TX_SIZES=4 };
enum { DCT_DCT=0, ADST_DCT=1, DCT_ADST=2, ADST_ADST=3 };

#define pair_set_epi16(a, b) _mm256_set_epi16(b, a, b, a, b, a, b, a, b, a, b, a, b, a, b, a);

namespace details {
    using namespace AV1PP;

    template <typename T> inline void recon_and_store(T *dest0, T *dest1, const __m256i *in_x);

    template <> inline void recon_and_store<unsigned char>(unsigned char *dest0, unsigned char *dest1, const __m256i *in_x) {
        __m256i d0 = _mm256_cvtepu8_epi16(loada_si128(dest0));
        __m256i d1 = _mm256_cvtepu8_epi16(loada_si128(dest1));
        d0 = _mm256_add_epi16(in_x[0], d0);
        d1 = _mm256_add_epi16(in_x[1], d1);
        d0 = _mm256_packus_epi16(d0, d1);
        d0 = _mm256_permute4x64_epi64(d0, PERM4x64(0,2,1,3));
        storea_si128(dest0, si128(d0));
        storea_si128(dest1, _mm256_extracti128_si256(d0, 1));
    }

    template <> inline void recon_and_store<short>(short *dest0, short *dest1, const __m256i *in_x) {
        storea_si256(dest0, in_x[0]);
        storea_si256(dest1, in_x[1]);
    }

    inline void load_buffer_16x16(const short *src, __m256i *in) {
        in[0]  = loada_si256(src + 0 * 16);
        in[1]  = loada_si256(src + 1 * 16);
        in[2]  = loada_si256(src + 2 * 16);
        in[3]  = loada_si256(src + 3 * 16);
        in[4]  = loada_si256(src + 4 * 16);
        in[5]  = loada_si256(src + 5 * 16);
        in[6]  = loada_si256(src + 6 * 16);
        in[7]  = loada_si256(src + 7 * 16);
        in[8]  = loada_si256(src + 8 * 16);
        in[9]  = loada_si256(src + 9 * 16);
        in[10] = loada_si256(src + 10 * 16);
        in[11] = loada_si256(src + 11 * 16);
        in[12] = loada_si256(src + 12 * 16);
        in[13] = loada_si256(src + 13 * 16);
        in[14] = loada_si256(src + 14 * 16);
        in[15] = loada_si256(src + 15 * 16);
    }

    template <typename T> inline void write_buffer_16x16(__m256i *in, T *dst, int pitch) {
        const __m256i final_rounding = _mm256_set1_epi16(1 << 5);
        // Final rounding and shift
        in[0]  = _mm256_adds_epi16(in[0], final_rounding);
        in[1]  = _mm256_adds_epi16(in[1], final_rounding);
        in[2]  = _mm256_adds_epi16(in[2], final_rounding);
        in[3]  = _mm256_adds_epi16(in[3], final_rounding);
        in[4]  = _mm256_adds_epi16(in[4], final_rounding);
        in[5]  = _mm256_adds_epi16(in[5], final_rounding);
        in[6]  = _mm256_adds_epi16(in[6], final_rounding);
        in[7]  = _mm256_adds_epi16(in[7], final_rounding);
        in[8]  = _mm256_adds_epi16(in[8], final_rounding);
        in[9]  = _mm256_adds_epi16(in[9], final_rounding);
        in[10] = _mm256_adds_epi16(in[10], final_rounding);
        in[11] = _mm256_adds_epi16(in[11], final_rounding);
        in[12] = _mm256_adds_epi16(in[12], final_rounding);
        in[13] = _mm256_adds_epi16(in[13], final_rounding);
        in[14] = _mm256_adds_epi16(in[14], final_rounding);
        in[15] = _mm256_adds_epi16(in[15], final_rounding);

        in[0]  = _mm256_srai_epi16(in[0], 6);
        in[1]  = _mm256_srai_epi16(in[1], 6);
        in[2]  = _mm256_srai_epi16(in[2], 6);
        in[3]  = _mm256_srai_epi16(in[3], 6);
        in[4]  = _mm256_srai_epi16(in[4], 6);
        in[5]  = _mm256_srai_epi16(in[5], 6);
        in[6]  = _mm256_srai_epi16(in[6], 6);
        in[7]  = _mm256_srai_epi16(in[7], 6);
        in[8]  = _mm256_srai_epi16(in[8], 6);
        in[9]  = _mm256_srai_epi16(in[9], 6);
        in[10] = _mm256_srai_epi16(in[10], 6);
        in[11] = _mm256_srai_epi16(in[11], 6);
        in[12] = _mm256_srai_epi16(in[12], 6);
        in[13] = _mm256_srai_epi16(in[13], 6);
        in[14] = _mm256_srai_epi16(in[14], 6);
        in[15] = _mm256_srai_epi16(in[15], 6);

        recon_and_store(dst +  0 * pitch, dst +  1 * pitch, in +  0);
        recon_and_store(dst +  2 * pitch, dst +  3 * pitch, in +  2);
        recon_and_store(dst +  4 * pitch, dst +  5 * pitch, in +  4);
        recon_and_store(dst +  6 * pitch, dst +  7 * pitch, in +  6);
        recon_and_store(dst +  8 * pitch, dst +  9 * pitch, in +  8);
        recon_and_store(dst + 10 * pitch, dst + 11 * pitch, in + 10);
        recon_and_store(dst + 12 * pitch, dst + 13 * pitch, in + 12);
        recon_and_store(dst + 14 * pitch, dst + 15 * pitch, in + 14);
    }

    void idct16(__m256i *in) {
        const __m256i k__cospi_p30_m02 = pair_set_epi16(cospi_30_64, -cospi_2_64);
        const __m256i k__cospi_p02_p30 = pair_set_epi16(cospi_2_64, cospi_30_64);
        const __m256i k__cospi_p14_m18 = pair_set_epi16(cospi_14_64, -cospi_18_64);
        const __m256i k__cospi_p18_p14 = pair_set_epi16(cospi_18_64, cospi_14_64);
        const __m256i k__cospi_p22_m10 = pair_set_epi16(cospi_22_64, -cospi_10_64);
        const __m256i k__cospi_p10_p22 = pair_set_epi16(cospi_10_64, cospi_22_64);
        const __m256i k__cospi_p06_m26 = pair_set_epi16(cospi_6_64, -cospi_26_64);
        const __m256i k__cospi_p26_p06 = pair_set_epi16(cospi_26_64, cospi_6_64);
        const __m256i k__cospi_p28_m04 = pair_set_epi16(cospi_28_64, -cospi_4_64);
        const __m256i k__cospi_p04_p28 = pair_set_epi16(cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p12_m20 = pair_set_epi16(cospi_12_64, -cospi_20_64);
        const __m256i k__cospi_p20_p12 = pair_set_epi16(cospi_20_64, cospi_12_64);
        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
        const __m256i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
        const __m256i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m256i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m256i k__cospi_m24_m08 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
        const __m256i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);
        __m256i v[16], u[16], s[16], t[16];
        s[0]  = in[0];
        s[1]  = in[8];
        s[2]  = in[4];
        s[3]  = in[12];
        s[4]  = in[2];
        s[5]  = in[10];
        s[6]  = in[6];
        s[7]  = in[14];
        s[8]  = in[1];
        s[9]  = in[9];
        s[10] = in[5];
        s[11] = in[13];
        s[12] = in[3];
        s[13] = in[11];
        s[14] = in[7];
        s[15] = in[15];
        u[0]  = _mm256_unpacklo_epi16(s[8], s[15]);
        u[1]  = _mm256_unpackhi_epi16(s[8], s[15]);
        u[2]  = _mm256_unpacklo_epi16(s[9], s[14]);
        u[3]  = _mm256_unpackhi_epi16(s[9], s[14]);
        u[4]  = _mm256_unpacklo_epi16(s[10], s[13]);
        u[5]  = _mm256_unpackhi_epi16(s[10], s[13]);
        u[6]  = _mm256_unpacklo_epi16(s[11], s[12]);
        u[7]  = _mm256_unpackhi_epi16(s[11], s[12]);
        v[0]  = _mm256_madd_epi16(u[0], k__cospi_p30_m02);
        v[1]  = _mm256_madd_epi16(u[1], k__cospi_p30_m02);
        v[2]  = _mm256_madd_epi16(u[0], k__cospi_p02_p30);
        v[3]  = _mm256_madd_epi16(u[1], k__cospi_p02_p30);
        v[4]  = _mm256_madd_epi16(u[2], k__cospi_p14_m18);
        v[5]  = _mm256_madd_epi16(u[3], k__cospi_p14_m18);
        v[6]  = _mm256_madd_epi16(u[2], k__cospi_p18_p14);
        v[7]  = _mm256_madd_epi16(u[3], k__cospi_p18_p14);
        v[8]  = _mm256_madd_epi16(u[4], k__cospi_p22_m10);
        v[9]  = _mm256_madd_epi16(u[5], k__cospi_p22_m10);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_p10_p22);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_p10_p22);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_p06_m26);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_p06_m26);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p26_p06);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p26_p06);
        u[0]  = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1]  = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2]  = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3]  = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4]  = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5]  = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6]  = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7]  = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[8]  = _mm256_add_epi32(v[8], k__DCT_CONST_ROUNDING);
        u[9]  = _mm256_add_epi32(v[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm256_add_epi32(v[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm256_add_epi32(v[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm256_add_epi32(v[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm256_add_epi32(v[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm256_add_epi32(v[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm256_add_epi32(v[15], k__DCT_CONST_ROUNDING);
        u[0]  = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        u[1]  = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        u[2]  = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        u[3]  = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        u[4]  = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        u[5]  = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        u[6]  = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        u[7]  = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        u[8]  = _mm256_srai_epi32(u[8], DCT_CONST_BITS);
        u[9]  = _mm256_srai_epi32(u[9], DCT_CONST_BITS);
        u[10] = _mm256_srai_epi32(u[10], DCT_CONST_BITS);
        u[11] = _mm256_srai_epi32(u[11], DCT_CONST_BITS);
        u[12] = _mm256_srai_epi32(u[12], DCT_CONST_BITS);
        u[13] = _mm256_srai_epi32(u[13], DCT_CONST_BITS);
        u[14] = _mm256_srai_epi32(u[14], DCT_CONST_BITS);
        u[15] = _mm256_srai_epi32(u[15], DCT_CONST_BITS);
        s[8]  = _mm256_packs_epi32(u[0], u[1]);
        s[15] = _mm256_packs_epi32(u[2], u[3]);
        s[9]  = _mm256_packs_epi32(u[4], u[5]);
        s[14] = _mm256_packs_epi32(u[6], u[7]);
        s[10] = _mm256_packs_epi32(u[8], u[9]);
        s[13] = _mm256_packs_epi32(u[10], u[11]);
        s[11] = _mm256_packs_epi32(u[12], u[13]);
        s[12] = _mm256_packs_epi32(u[14], u[15]);
        t[0]  = s[0];
        t[1]  = s[1];
        t[2]  = s[2];
        t[3]  = s[3];
        u[0]  = _mm256_unpacklo_epi16(s[4], s[7]);
        u[1]  = _mm256_unpackhi_epi16(s[4], s[7]);
        u[2]  = _mm256_unpacklo_epi16(s[5], s[6]);
        u[3]  = _mm256_unpackhi_epi16(s[5], s[6]);
        v[0]  = _mm256_madd_epi16(u[0], k__cospi_p28_m04);
        v[1]  = _mm256_madd_epi16(u[1], k__cospi_p28_m04);
        v[2]  = _mm256_madd_epi16(u[0], k__cospi_p04_p28);
        v[3]  = _mm256_madd_epi16(u[1], k__cospi_p04_p28);
        v[4]  = _mm256_madd_epi16(u[2], k__cospi_p12_m20);
        v[5]  = _mm256_madd_epi16(u[3], k__cospi_p12_m20);
        v[6]  = _mm256_madd_epi16(u[2], k__cospi_p20_p12);
        v[7]  = _mm256_madd_epi16(u[3], k__cospi_p20_p12);
        u[0]  = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1]  = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2]  = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3]  = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4]  = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5]  = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6]  = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7]  = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[0]  = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        u[1]  = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        u[2]  = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        u[3]  = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        u[4]  = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        u[5]  = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        u[6]  = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        u[7]  = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        t[4]  = _mm256_packs_epi32(u[0], u[1]);
        t[7]  = _mm256_packs_epi32(u[2], u[3]);
        t[5]  = _mm256_packs_epi32(u[4], u[5]);
        t[6]  = _mm256_packs_epi32(u[6], u[7]);
        t[8]  = _mm256_add_epi16(s[8], s[9]);
        t[9]  = _mm256_sub_epi16(s[8], s[9]);
        t[10] = _mm256_sub_epi16(s[11], s[10]);
        t[11] = _mm256_add_epi16(s[10], s[11]);
        t[12] = _mm256_add_epi16(s[12], s[13]);
        t[13] = _mm256_sub_epi16(s[12], s[13]);
        t[14] = _mm256_sub_epi16(s[15], s[14]);
        t[15] = _mm256_add_epi16(s[14], s[15]);
        u[0]  = _mm256_unpacklo_epi16(t[0], t[1]);
        u[1]  = _mm256_unpackhi_epi16(t[0], t[1]);
        u[2]  = _mm256_unpacklo_epi16(t[2], t[3]);
        u[3]  = _mm256_unpackhi_epi16(t[2], t[3]);
        u[4]  = _mm256_unpacklo_epi16(t[9], t[14]);
        u[5]  = _mm256_unpackhi_epi16(t[9], t[14]);
        u[6]  = _mm256_unpacklo_epi16(t[10], t[13]);
        u[7]  = _mm256_unpackhi_epi16(t[10], t[13]);
        v[0]  = _mm256_madd_epi16(u[0], k__cospi_p16_p16);
        v[1]  = _mm256_madd_epi16(u[1], k__cospi_p16_p16);
        v[2]  = _mm256_madd_epi16(u[0], k__cospi_p16_m16);
        v[3]  = _mm256_madd_epi16(u[1], k__cospi_p16_m16);
        v[4]  = _mm256_madd_epi16(u[2], k__cospi_p24_m08);
        v[5]  = _mm256_madd_epi16(u[3], k__cospi_p24_m08);
        v[6]  = _mm256_madd_epi16(u[2], k__cospi_p08_p24);
        v[7]  = _mm256_madd_epi16(u[3], k__cospi_p08_p24);
        v[8]  = _mm256_madd_epi16(u[4], k__cospi_m08_p24);
        v[9]  = _mm256_madd_epi16(u[5], k__cospi_m08_p24);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_p24_p08);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_p24_p08);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_m24_m08);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_m24_m08);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_m08_p24);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_m08_p24);
        u[0]  = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1]  = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2]  = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3]  = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4]  = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5]  = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6]  = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7]  = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[8]  = _mm256_add_epi32(v[8], k__DCT_CONST_ROUNDING);
        u[9]  = _mm256_add_epi32(v[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm256_add_epi32(v[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm256_add_epi32(v[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm256_add_epi32(v[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm256_add_epi32(v[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm256_add_epi32(v[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm256_add_epi32(v[15], k__DCT_CONST_ROUNDING);
        u[0]  = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        u[1]  = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        u[2]  = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        u[3]  = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        u[4]  = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        u[5]  = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        u[6]  = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        u[7]  = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        u[8]  = _mm256_srai_epi32(u[8], DCT_CONST_BITS);
        u[9]  = _mm256_srai_epi32(u[9], DCT_CONST_BITS);
        u[10] = _mm256_srai_epi32(u[10], DCT_CONST_BITS);
        u[11] = _mm256_srai_epi32(u[11], DCT_CONST_BITS);
        u[12] = _mm256_srai_epi32(u[12], DCT_CONST_BITS);
        u[13] = _mm256_srai_epi32(u[13], DCT_CONST_BITS);
        u[14] = _mm256_srai_epi32(u[14], DCT_CONST_BITS);
        u[15] = _mm256_srai_epi32(u[15], DCT_CONST_BITS);
        s[0]  = _mm256_packs_epi32(u[0], u[1]);
        s[1]  = _mm256_packs_epi32(u[2], u[3]);
        s[2]  = _mm256_packs_epi32(u[4], u[5]);
        s[3]  = _mm256_packs_epi32(u[6], u[7]);
        s[4]  = _mm256_add_epi16(t[4], t[5]);
        s[5]  = _mm256_sub_epi16(t[4], t[5]);
        s[6]  = _mm256_sub_epi16(t[7], t[6]);
        s[7]  = _mm256_add_epi16(t[6], t[7]);
        s[8]  = t[8];
        s[15] = t[15];
        s[9]  = _mm256_packs_epi32(u[8], u[9]);
        s[14] = _mm256_packs_epi32(u[10], u[11]);
        s[10] = _mm256_packs_epi32(u[12], u[13]);
        s[13] = _mm256_packs_epi32(u[14], u[15]);
        s[11] = t[11];
        s[12] = t[12];
        t[0]  = _mm256_add_epi16(s[0], s[3]);
        t[1]  = _mm256_add_epi16(s[1], s[2]);
        t[2]  = _mm256_sub_epi16(s[1], s[2]);
        t[3]  = _mm256_sub_epi16(s[0], s[3]);
        t[4]  = s[4];
        t[7]  = s[7];
        u[0]  = _mm256_unpacklo_epi16(s[5], s[6]);
        u[1]  = _mm256_unpackhi_epi16(s[5], s[6]);
        v[0]  = _mm256_madd_epi16(u[0], k__cospi_m16_p16);
        v[1]  = _mm256_madd_epi16(u[1], k__cospi_m16_p16);
        v[2]  = _mm256_madd_epi16(u[0], k__cospi_p16_p16);
        v[3]  = _mm256_madd_epi16(u[1], k__cospi_p16_p16);
        u[0]  = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1]  = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2]  = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3]  = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[0]  = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        u[1]  = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        u[2]  = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        u[3]  = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        t[5]  = _mm256_packs_epi32(u[0], u[1]);
        t[6]  = _mm256_packs_epi32(u[2], u[3]);
        t[8]  = _mm256_add_epi16(s[8], s[11]);
        t[9]  = _mm256_add_epi16(s[9], s[10]);
        t[10] = _mm256_sub_epi16(s[9], s[10]);
        t[11] = _mm256_sub_epi16(s[8], s[11]);
        t[12] = _mm256_sub_epi16(s[15], s[12]);
        t[13] = _mm256_sub_epi16(s[14], s[13]);
        t[14] = _mm256_add_epi16(s[13], s[14]);
        t[15] = _mm256_add_epi16(s[12], s[15]);
        s[0]  = _mm256_add_epi16(t[0], t[7]);
        s[1]  = _mm256_add_epi16(t[1], t[6]);
        s[2]  = _mm256_add_epi16(t[2], t[5]);
        s[3]  = _mm256_add_epi16(t[3], t[4]);
        s[4]  = _mm256_sub_epi16(t[3], t[4]);
        s[5]  = _mm256_sub_epi16(t[2], t[5]);
        s[6]  = _mm256_sub_epi16(t[1], t[6]);
        s[7]  = _mm256_sub_epi16(t[0], t[7]);
        s[8]  = t[8];
        s[9]  = t[9];
        u[0]  = _mm256_unpacklo_epi16(t[10], t[13]);
        u[1]  = _mm256_unpackhi_epi16(t[10], t[13]);
        u[2]  = _mm256_unpacklo_epi16(t[11], t[12]);
        u[3]  = _mm256_unpackhi_epi16(t[11], t[12]);
        v[0]  = _mm256_madd_epi16(u[0], k__cospi_m16_p16);
        v[1]  = _mm256_madd_epi16(u[1], k__cospi_m16_p16);
        v[2]  = _mm256_madd_epi16(u[0], k__cospi_p16_p16);
        v[3]  = _mm256_madd_epi16(u[1], k__cospi_p16_p16);
        v[4]  = _mm256_madd_epi16(u[2], k__cospi_m16_p16);
        v[5]  = _mm256_madd_epi16(u[3], k__cospi_m16_p16);
        v[6]  = _mm256_madd_epi16(u[2], k__cospi_p16_p16);
        v[7]  = _mm256_madd_epi16(u[3], k__cospi_p16_p16);
        u[0]  = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1]  = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2]  = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3]  = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4]  = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5]  = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6]  = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7]  = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[0]  = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        u[1]  = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        u[2]  = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        u[3]  = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        u[4]  = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        u[5]  = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        u[6]  = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        u[7]  = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        s[10] = _mm256_packs_epi32(u[0], u[1]);
        s[13] = _mm256_packs_epi32(u[2], u[3]);
        s[11] = _mm256_packs_epi32(u[4], u[5]);
        s[12] = _mm256_packs_epi32(u[6], u[7]);
        s[14] = t[14];
        s[15] = t[15];

        in[0]  = _mm256_add_epi16(s[0], s[15]);
        in[1]  = _mm256_add_epi16(s[1], s[14]);
        in[2]  = _mm256_add_epi16(s[2], s[13]);
        in[3]  = _mm256_add_epi16(s[3], s[12]);
        in[4]  = _mm256_add_epi16(s[4], s[11]);
        in[5]  = _mm256_add_epi16(s[5], s[10]);
        in[6]  = _mm256_add_epi16(s[6], s[9]);
        in[7]  = _mm256_add_epi16(s[7], s[8]);
        in[8]  = _mm256_sub_epi16(s[7], s[8]);
        in[9]  = _mm256_sub_epi16(s[6], s[9]);
        in[10] = _mm256_sub_epi16(s[5], s[10]);
        in[11] = _mm256_sub_epi16(s[4], s[11]);
        in[12] = _mm256_sub_epi16(s[3], s[12]);
        in[13] = _mm256_sub_epi16(s[2], s[13]);
        in[14] = _mm256_sub_epi16(s[1], s[14]);
        in[15] = _mm256_sub_epi16(s[0], s[15]);
    }

    void iadst16(__m256i *in) {
        __m256i s[16], x[16], u[32], v[32];
        const __m256i k__cospi_p01_p31 = pair_set_epi16(cospi_1_64, cospi_31_64);
        const __m256i k__cospi_p31_m01 = pair_set_epi16(cospi_31_64, -cospi_1_64);
        const __m256i k__cospi_p05_p27 = pair_set_epi16(cospi_5_64, cospi_27_64);
        const __m256i k__cospi_p27_m05 = pair_set_epi16(cospi_27_64, -cospi_5_64);
        const __m256i k__cospi_p09_p23 = pair_set_epi16(cospi_9_64, cospi_23_64);
        const __m256i k__cospi_p23_m09 = pair_set_epi16(cospi_23_64, -cospi_9_64);
        const __m256i k__cospi_p13_p19 = pair_set_epi16(cospi_13_64, cospi_19_64);
        const __m256i k__cospi_p19_m13 = pair_set_epi16(cospi_19_64, -cospi_13_64);
        const __m256i k__cospi_p17_p15 = pair_set_epi16(cospi_17_64, cospi_15_64);
        const __m256i k__cospi_p15_m17 = pair_set_epi16(cospi_15_64, -cospi_17_64);
        const __m256i k__cospi_p21_p11 = pair_set_epi16(cospi_21_64, cospi_11_64);
        const __m256i k__cospi_p11_m21 = pair_set_epi16(cospi_11_64, -cospi_21_64);
        const __m256i k__cospi_p25_p07 = pair_set_epi16(cospi_25_64, cospi_7_64);
        const __m256i k__cospi_p07_m25 = pair_set_epi16(cospi_7_64, -cospi_25_64);
        const __m256i k__cospi_p29_p03 = pair_set_epi16(cospi_29_64, cospi_3_64);
        const __m256i k__cospi_p03_m29 = pair_set_epi16(cospi_3_64, -cospi_29_64);
        const __m256i k__cospi_p04_p28 = pair_set_epi16(cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p28_m04 = pair_set_epi16(cospi_28_64, -cospi_4_64);
        const __m256i k__cospi_p20_p12 = pair_set_epi16(cospi_20_64, cospi_12_64);
        const __m256i k__cospi_p12_m20 = pair_set_epi16(cospi_12_64, -cospi_20_64);
        const __m256i k__cospi_m28_p04 = pair_set_epi16(-cospi_28_64, cospi_4_64);
        const __m256i k__cospi_m12_p20 = pair_set_epi16(-cospi_12_64, cospi_20_64);
        const __m256i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
        const __m256i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
        const __m256i k__cospi_m24_p08 = pair_set_epi16(-cospi_24_64, cospi_8_64);
        const __m256i k__cospi_m16_m16 = _mm256_set1_epi16(-cospi_16_64);
        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);
        const __m256i kZero = _mm256_setzero_si256();
        u[0]  = _mm256_unpacklo_epi16(in[15], in[0]);
        u[1]  = _mm256_unpackhi_epi16(in[15], in[0]);
        u[2]  = _mm256_unpacklo_epi16(in[13], in[2]);
        u[3]  = _mm256_unpackhi_epi16(in[13], in[2]);
        u[4]  = _mm256_unpacklo_epi16(in[11], in[4]);
        u[5]  = _mm256_unpackhi_epi16(in[11], in[4]);
        u[6]  = _mm256_unpacklo_epi16(in[9], in[6]);
        u[7]  = _mm256_unpackhi_epi16(in[9], in[6]);
        u[8]  = _mm256_unpacklo_epi16(in[7], in[8]);
        u[9]  = _mm256_unpackhi_epi16(in[7], in[8]);
        u[10] = _mm256_unpacklo_epi16(in[5], in[10]);
        u[11] = _mm256_unpackhi_epi16(in[5], in[10]);
        u[12] = _mm256_unpacklo_epi16(in[3], in[12]);
        u[13] = _mm256_unpackhi_epi16(in[3], in[12]);
        u[14] = _mm256_unpacklo_epi16(in[1], in[14]);
        u[15] = _mm256_unpackhi_epi16(in[1], in[14]);
        v[0]  = _mm256_madd_epi16(u[0], k__cospi_p01_p31);
        v[1]  = _mm256_madd_epi16(u[1], k__cospi_p01_p31);
        v[2]  = _mm256_madd_epi16(u[0], k__cospi_p31_m01);
        v[3]  = _mm256_madd_epi16(u[1], k__cospi_p31_m01);
        v[4]  = _mm256_madd_epi16(u[2], k__cospi_p05_p27);
        v[5]  = _mm256_madd_epi16(u[3], k__cospi_p05_p27);
        v[6]  = _mm256_madd_epi16(u[2], k__cospi_p27_m05);
        v[7]  = _mm256_madd_epi16(u[3], k__cospi_p27_m05);
        v[8]  = _mm256_madd_epi16(u[4], k__cospi_p09_p23);
        v[9]  = _mm256_madd_epi16(u[5], k__cospi_p09_p23);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_p23_m09);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_p23_m09);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_p13_p19);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_p13_p19);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p19_m13);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p19_m13);
        v[16] = _mm256_madd_epi16(u[8], k__cospi_p17_p15);
        v[17] = _mm256_madd_epi16(u[9], k__cospi_p17_p15);
        v[18] = _mm256_madd_epi16(u[8], k__cospi_p15_m17);
        v[19] = _mm256_madd_epi16(u[9], k__cospi_p15_m17);
        v[20] = _mm256_madd_epi16(u[10], k__cospi_p21_p11);
        v[21] = _mm256_madd_epi16(u[11], k__cospi_p21_p11);
        v[22] = _mm256_madd_epi16(u[10], k__cospi_p11_m21);
        v[23] = _mm256_madd_epi16(u[11], k__cospi_p11_m21);
        v[24] = _mm256_madd_epi16(u[12], k__cospi_p25_p07);
        v[25] = _mm256_madd_epi16(u[13], k__cospi_p25_p07);
        v[26] = _mm256_madd_epi16(u[12], k__cospi_p07_m25);
        v[27] = _mm256_madd_epi16(u[13], k__cospi_p07_m25);
        v[28] = _mm256_madd_epi16(u[14], k__cospi_p29_p03);
        v[29] = _mm256_madd_epi16(u[15], k__cospi_p29_p03);
        v[30] = _mm256_madd_epi16(u[14], k__cospi_p03_m29);
        v[31] = _mm256_madd_epi16(u[15], k__cospi_p03_m29);
        u[0]  = _mm256_add_epi32(v[0], v[16]);
        u[1]  = _mm256_add_epi32(v[1], v[17]);
        u[2]  = _mm256_add_epi32(v[2], v[18]);
        u[3]  = _mm256_add_epi32(v[3], v[19]);
        u[4]  = _mm256_add_epi32(v[4], v[20]);
        u[5]  = _mm256_add_epi32(v[5], v[21]);
        u[6]  = _mm256_add_epi32(v[6], v[22]);
        u[7]  = _mm256_add_epi32(v[7], v[23]);
        u[8]  = _mm256_add_epi32(v[8], v[24]);
        u[9]  = _mm256_add_epi32(v[9], v[25]);
        u[10] = _mm256_add_epi32(v[10], v[26]);
        u[11] = _mm256_add_epi32(v[11], v[27]);
        u[12] = _mm256_add_epi32(v[12], v[28]);
        u[13] = _mm256_add_epi32(v[13], v[29]);
        u[14] = _mm256_add_epi32(v[14], v[30]);
        u[15] = _mm256_add_epi32(v[15], v[31]);
        u[16] = _mm256_sub_epi32(v[0], v[16]);
        u[17] = _mm256_sub_epi32(v[1], v[17]);
        u[18] = _mm256_sub_epi32(v[2], v[18]);
        u[19] = _mm256_sub_epi32(v[3], v[19]);
        u[20] = _mm256_sub_epi32(v[4], v[20]);
        u[21] = _mm256_sub_epi32(v[5], v[21]);
        u[22] = _mm256_sub_epi32(v[6], v[22]);
        u[23] = _mm256_sub_epi32(v[7], v[23]);
        u[24] = _mm256_sub_epi32(v[8], v[24]);
        u[25] = _mm256_sub_epi32(v[9], v[25]);
        u[26] = _mm256_sub_epi32(v[10], v[26]);
        u[27] = _mm256_sub_epi32(v[11], v[27]);
        u[28] = _mm256_sub_epi32(v[12], v[28]);
        u[29] = _mm256_sub_epi32(v[13], v[29]);
        u[30] = _mm256_sub_epi32(v[14], v[30]);
        u[31] = _mm256_sub_epi32(v[15], v[31]);
        v[0]  = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1]  = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2]  = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3]  = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4]  = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5]  = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6]  = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7]  = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        v[8]  = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        v[9]  = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);
        v[16] = _mm256_add_epi32(u[16], k__DCT_CONST_ROUNDING);
        v[17] = _mm256_add_epi32(u[17], k__DCT_CONST_ROUNDING);
        v[18] = _mm256_add_epi32(u[18], k__DCT_CONST_ROUNDING);
        v[19] = _mm256_add_epi32(u[19], k__DCT_CONST_ROUNDING);
        v[20] = _mm256_add_epi32(u[20], k__DCT_CONST_ROUNDING);
        v[21] = _mm256_add_epi32(u[21], k__DCT_CONST_ROUNDING);
        v[22] = _mm256_add_epi32(u[22], k__DCT_CONST_ROUNDING);
        v[23] = _mm256_add_epi32(u[23], k__DCT_CONST_ROUNDING);
        v[24] = _mm256_add_epi32(u[24], k__DCT_CONST_ROUNDING);
        v[25] = _mm256_add_epi32(u[25], k__DCT_CONST_ROUNDING);
        v[26] = _mm256_add_epi32(u[26], k__DCT_CONST_ROUNDING);
        v[27] = _mm256_add_epi32(u[27], k__DCT_CONST_ROUNDING);
        v[28] = _mm256_add_epi32(u[28], k__DCT_CONST_ROUNDING);
        v[29] = _mm256_add_epi32(u[29], k__DCT_CONST_ROUNDING);
        v[30] = _mm256_add_epi32(u[30], k__DCT_CONST_ROUNDING);
        v[31] = _mm256_add_epi32(u[31], k__DCT_CONST_ROUNDING);
        u[0]  = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
        u[1]  = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
        u[2]  = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
        u[3]  = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
        u[4]  = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
        u[5]  = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
        u[6]  = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
        u[7]  = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
        u[8]  = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
        u[9]  = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
        u[10] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
        u[11] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
        u[12] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
        u[13] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
        u[14] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
        u[15] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);
        u[16] = _mm256_srai_epi32(v[16], DCT_CONST_BITS);
        u[17] = _mm256_srai_epi32(v[17], DCT_CONST_BITS);
        u[18] = _mm256_srai_epi32(v[18], DCT_CONST_BITS);
        u[19] = _mm256_srai_epi32(v[19], DCT_CONST_BITS);
        u[20] = _mm256_srai_epi32(v[20], DCT_CONST_BITS);
        u[21] = _mm256_srai_epi32(v[21], DCT_CONST_BITS);
        u[22] = _mm256_srai_epi32(v[22], DCT_CONST_BITS);
        u[23] = _mm256_srai_epi32(v[23], DCT_CONST_BITS);
        u[24] = _mm256_srai_epi32(v[24], DCT_CONST_BITS);
        u[25] = _mm256_srai_epi32(v[25], DCT_CONST_BITS);
        u[26] = _mm256_srai_epi32(v[26], DCT_CONST_BITS);
        u[27] = _mm256_srai_epi32(v[27], DCT_CONST_BITS);
        u[28] = _mm256_srai_epi32(v[28], DCT_CONST_BITS);
        u[29] = _mm256_srai_epi32(v[29], DCT_CONST_BITS);
        u[30] = _mm256_srai_epi32(v[30], DCT_CONST_BITS);
        u[31] = _mm256_srai_epi32(v[31], DCT_CONST_BITS);
        s[0]  = _mm256_packs_epi32(u[0], u[1]);
        s[1]  = _mm256_packs_epi32(u[2], u[3]);
        s[2]  = _mm256_packs_epi32(u[4], u[5]);
        s[3]  = _mm256_packs_epi32(u[6], u[7]);
        s[4]  = _mm256_packs_epi32(u[8], u[9]);
        s[5]  = _mm256_packs_epi32(u[10], u[11]);
        s[6]  = _mm256_packs_epi32(u[12], u[13]);
        s[7]  = _mm256_packs_epi32(u[14], u[15]);
        s[8]  = _mm256_packs_epi32(u[16], u[17]);
        s[9]  = _mm256_packs_epi32(u[18], u[19]);
        s[10] = _mm256_packs_epi32(u[20], u[21]);
        s[11] = _mm256_packs_epi32(u[22], u[23]);
        s[12] = _mm256_packs_epi32(u[24], u[25]);
        s[13] = _mm256_packs_epi32(u[26], u[27]);
        s[14] = _mm256_packs_epi32(u[28], u[29]);
        s[15] = _mm256_packs_epi32(u[30], u[31]);
        u[0]  = _mm256_unpacklo_epi16(s[8], s[9]);
        u[1]  = _mm256_unpackhi_epi16(s[8], s[9]);
        u[2]  = _mm256_unpacklo_epi16(s[10], s[11]);
        u[3]  = _mm256_unpackhi_epi16(s[10], s[11]);
        u[4]  = _mm256_unpacklo_epi16(s[12], s[13]);
        u[5]  = _mm256_unpackhi_epi16(s[12], s[13]);
        u[6]  = _mm256_unpacklo_epi16(s[14], s[15]);
        u[7]  = _mm256_unpackhi_epi16(s[14], s[15]);
        v[0]  = _mm256_madd_epi16(u[0], k__cospi_p04_p28);
        v[1]  = _mm256_madd_epi16(u[1], k__cospi_p04_p28);
        v[2]  = _mm256_madd_epi16(u[0], k__cospi_p28_m04);
        v[3]  = _mm256_madd_epi16(u[1], k__cospi_p28_m04);
        v[4]  = _mm256_madd_epi16(u[2], k__cospi_p20_p12);
        v[5]  = _mm256_madd_epi16(u[3], k__cospi_p20_p12);
        v[6]  = _mm256_madd_epi16(u[2], k__cospi_p12_m20);
        v[7]  = _mm256_madd_epi16(u[3], k__cospi_p12_m20);
        v[8]  = _mm256_madd_epi16(u[4], k__cospi_m28_p04);
        v[9]  = _mm256_madd_epi16(u[5], k__cospi_m28_p04);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_p04_p28);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_p04_p28);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_m12_p20);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_m12_p20);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p20_p12);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p20_p12);
        u[0]  = _mm256_add_epi32(v[0], v[8]);
        u[1]  = _mm256_add_epi32(v[1], v[9]);
        u[2]  = _mm256_add_epi32(v[2], v[10]);
        u[3]  = _mm256_add_epi32(v[3], v[11]);
        u[4]  = _mm256_add_epi32(v[4], v[12]);
        u[5]  = _mm256_add_epi32(v[5], v[13]);
        u[6]  = _mm256_add_epi32(v[6], v[14]);
        u[7]  = _mm256_add_epi32(v[7], v[15]);
        u[8]  = _mm256_sub_epi32(v[0], v[8]);
        u[9]  = _mm256_sub_epi32(v[1], v[9]);
        u[10] = _mm256_sub_epi32(v[2], v[10]);
        u[11] = _mm256_sub_epi32(v[3], v[11]);
        u[12] = _mm256_sub_epi32(v[4], v[12]);
        u[13] = _mm256_sub_epi32(v[5], v[13]);
        u[14] = _mm256_sub_epi32(v[6], v[14]);
        u[15] = _mm256_sub_epi32(v[7], v[15]);
        v[0]  = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1]  = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2]  = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3]  = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4]  = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5]  = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6]  = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7]  = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        v[8]  = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        v[9]  = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);
        u[0]  = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
        u[1]  = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
        u[2]  = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
        u[3]  = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
        u[4]  = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
        u[5]  = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
        u[6]  = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
        u[7]  = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
        u[8]  = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
        u[9]  = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
        u[10] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
        u[11] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
        u[12] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
        u[13] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
        u[14] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
        u[15] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);
        x[0]  = _mm256_add_epi16(s[0], s[4]);
        x[1]  = _mm256_add_epi16(s[1], s[5]);
        x[2]  = _mm256_add_epi16(s[2], s[6]);
        x[3]  = _mm256_add_epi16(s[3], s[7]);
        x[4]  = _mm256_sub_epi16(s[0], s[4]);
        x[5]  = _mm256_sub_epi16(s[1], s[5]);
        x[6]  = _mm256_sub_epi16(s[2], s[6]);
        x[7]  = _mm256_sub_epi16(s[3], s[7]);
        x[8]  = _mm256_packs_epi32(u[0], u[1]);
        x[9]  = _mm256_packs_epi32(u[2], u[3]);
        x[10] = _mm256_packs_epi32(u[4], u[5]);
        x[11] = _mm256_packs_epi32(u[6], u[7]);
        x[12] = _mm256_packs_epi32(u[8], u[9]);
        x[13] = _mm256_packs_epi32(u[10], u[11]);
        x[14] = _mm256_packs_epi32(u[12], u[13]);
        x[15] = _mm256_packs_epi32(u[14], u[15]);
        u[0]  = _mm256_unpacklo_epi16(x[4], x[5]);
        u[1]  = _mm256_unpackhi_epi16(x[4], x[5]);
        u[2]  = _mm256_unpacklo_epi16(x[6], x[7]);
        u[3]  = _mm256_unpackhi_epi16(x[6], x[7]);
        u[4]  = _mm256_unpacklo_epi16(x[12], x[13]);
        u[5]  = _mm256_unpackhi_epi16(x[12], x[13]);
        u[6]  = _mm256_unpacklo_epi16(x[14], x[15]);
        u[7]  = _mm256_unpackhi_epi16(x[14], x[15]);
        v[0]  = _mm256_madd_epi16(u[0], k__cospi_p08_p24);
        v[1]  = _mm256_madd_epi16(u[1], k__cospi_p08_p24);
        v[2]  = _mm256_madd_epi16(u[0], k__cospi_p24_m08);
        v[3]  = _mm256_madd_epi16(u[1], k__cospi_p24_m08);
        v[4]  = _mm256_madd_epi16(u[2], k__cospi_m24_p08);
        v[5]  = _mm256_madd_epi16(u[3], k__cospi_m24_p08);
        v[6]  = _mm256_madd_epi16(u[2], k__cospi_p08_p24);
        v[7]  = _mm256_madd_epi16(u[3], k__cospi_p08_p24);
        v[8]  = _mm256_madd_epi16(u[4], k__cospi_p08_p24);
        v[9]  = _mm256_madd_epi16(u[5], k__cospi_p08_p24);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_p24_m08);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_p24_m08);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_m24_p08);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_m24_p08);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p08_p24);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p08_p24);
        u[0]  = _mm256_add_epi32(v[0], v[4]);
        u[1]  = _mm256_add_epi32(v[1], v[5]);
        u[2]  = _mm256_add_epi32(v[2], v[6]);
        u[3]  = _mm256_add_epi32(v[3], v[7]);
        u[4]  = _mm256_sub_epi32(v[0], v[4]);
        u[5]  = _mm256_sub_epi32(v[1], v[5]);
        u[6]  = _mm256_sub_epi32(v[2], v[6]);
        u[7]  = _mm256_sub_epi32(v[3], v[7]);
        u[8]  = _mm256_add_epi32(v[8], v[12]);
        u[9]  = _mm256_add_epi32(v[9], v[13]);
        u[10] = _mm256_add_epi32(v[10], v[14]);
        u[11] = _mm256_add_epi32(v[11], v[15]);
        u[12] = _mm256_sub_epi32(v[8], v[12]);
        u[13] = _mm256_sub_epi32(v[9], v[13]);
        u[14] = _mm256_sub_epi32(v[10], v[14]);
        u[15] = _mm256_sub_epi32(v[11], v[15]);
        u[0]  = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        u[1]  = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        u[2]  = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        u[3]  = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        u[4]  = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        u[5]  = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        u[6]  = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        u[7]  = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        u[8]  = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        u[9]  = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);
        v[0]  = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1]  = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2]  = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3]  = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4]  = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5]  = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6]  = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7]  = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        v[8]  = _mm256_srai_epi32(u[8], DCT_CONST_BITS);
        v[9]  = _mm256_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm256_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm256_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm256_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm256_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm256_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm256_srai_epi32(u[15], DCT_CONST_BITS);
        s[0]  = _mm256_add_epi16(x[0], x[2]);
        s[1]  = _mm256_add_epi16(x[1], x[3]);
        s[2]  = _mm256_sub_epi16(x[0], x[2]);
        s[3]  = _mm256_sub_epi16(x[1], x[3]);
        s[4]  = _mm256_packs_epi32(v[0], v[1]);
        s[5]  = _mm256_packs_epi32(v[2], v[3]);
        s[6]  = _mm256_packs_epi32(v[4], v[5]);
        s[7]  = _mm256_packs_epi32(v[6], v[7]);
        s[8]  = _mm256_add_epi16(x[8], x[10]);
        s[9]  = _mm256_add_epi16(x[9], x[11]);
        s[10] = _mm256_sub_epi16(x[8], x[10]);
        s[11] = _mm256_sub_epi16(x[9], x[11]);
        s[12] = _mm256_packs_epi32(v[8], v[9]);
        s[13] = _mm256_packs_epi32(v[10], v[11]);
        s[14] = _mm256_packs_epi32(v[12], v[13]);
        s[15] = _mm256_packs_epi32(v[14], v[15]);
        u[0]  = _mm256_unpacklo_epi16(s[2], s[3]);
        u[1]  = _mm256_unpackhi_epi16(s[2], s[3]);
        u[2]  = _mm256_unpacklo_epi16(s[6], s[7]);
        u[3]  = _mm256_unpackhi_epi16(s[6], s[7]);
        u[4]  = _mm256_unpacklo_epi16(s[10], s[11]);
        u[5]  = _mm256_unpackhi_epi16(s[10], s[11]);
        u[6]  = _mm256_unpacklo_epi16(s[14], s[15]);
        u[7]  = _mm256_unpackhi_epi16(s[14], s[15]);
        v[0]  = _mm256_madd_epi16(u[0], k__cospi_m16_m16);
        v[1]  = _mm256_madd_epi16(u[1], k__cospi_m16_m16);
        v[2]  = _mm256_madd_epi16(u[0], k__cospi_p16_m16);
        v[3]  = _mm256_madd_epi16(u[1], k__cospi_p16_m16);
        v[4]  = _mm256_madd_epi16(u[2], k__cospi_p16_p16);
        v[5]  = _mm256_madd_epi16(u[3], k__cospi_p16_p16);
        v[6]  = _mm256_madd_epi16(u[2], k__cospi_m16_p16);
        v[7]  = _mm256_madd_epi16(u[3], k__cospi_m16_p16);
        v[8]  = _mm256_madd_epi16(u[4], k__cospi_p16_p16);
        v[9]  = _mm256_madd_epi16(u[5], k__cospi_p16_p16);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_m16_p16);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_m16_p16);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_m16_m16);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_m16_m16);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p16_m16);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p16_m16);
        u[0]  = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1]  = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2]  = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3]  = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4]  = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5]  = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6]  = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7]  = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[8]  = _mm256_add_epi32(v[8], k__DCT_CONST_ROUNDING);
        u[9]  = _mm256_add_epi32(v[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm256_add_epi32(v[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm256_add_epi32(v[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm256_add_epi32(v[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm256_add_epi32(v[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm256_add_epi32(v[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm256_add_epi32(v[15], k__DCT_CONST_ROUNDING);
        v[0]  = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1]  = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2]  = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3]  = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4]  = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5]  = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6]  = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7]  = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        v[8]  = _mm256_srai_epi32(u[8], DCT_CONST_BITS);
        v[9]  = _mm256_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm256_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm256_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm256_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm256_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm256_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm256_srai_epi32(u[15], DCT_CONST_BITS);

        in[0]  = s[0];
        in[1]  = _mm256_sub_epi16(kZero, s[8]);
        in[2]  = s[12];
        in[3]  = _mm256_sub_epi16(kZero, s[4]);
        in[4]  = _mm256_packs_epi32(v[4], v[5]);
        in[5]  = _mm256_packs_epi32(v[12], v[13]);
        in[6]  = _mm256_packs_epi32(v[8], v[9]);
        in[7]  = _mm256_packs_epi32(v[0], v[1]);
        in[8]  = _mm256_packs_epi32(v[2], v[3]);
        in[9]  = _mm256_packs_epi32(v[10], v[11]);
        in[10] = _mm256_packs_epi32(v[14], v[15]);
        in[11] = _mm256_packs_epi32(v[6], v[7]);
        in[12] = s[5];
        in[13] = _mm256_sub_epi16(kZero, s[13]);
        in[14] = s[9];
        in[15] = _mm256_sub_epi16(kZero, s[1]);
    }

    template <int tx_type, typename T> void iht16x16_256_avx2(const short *src, T *dst, int pitchDst)
    {
        __m256i in[16];
        load_buffer_16x16(src, in);
        if (tx_type == DCT_DCT) {
            array_transpose_16x16(in, in);
            idct16(in);
            array_transpose_16x16(in, in);
            idct16(in);
        } else if (tx_type == ADST_DCT) {
            array_transpose_16x16(in, in);
            idct16(in);
            array_transpose_16x16(in, in);
            iadst16(in);
        } else if (tx_type == DCT_ADST) {
            array_transpose_16x16(in, in);
            iadst16(in);
            array_transpose_16x16(in, in);
            idct16(in);
        } else if (tx_type == ADST_ADST) {
            array_transpose_16x16(in, in);
            iadst16(in);
            array_transpose_16x16(in, in);
            iadst16(in);
        } else {
            assert(0);
        }
        write_buffer_16x16(in, dst, pitchDst);
    }

// Define Macro for multiplying elements by constants and adding them together.
#define MULTIPLICATION_AND_ADD(lo_0, hi_0, lo_1, hi_1, cst0, cst1, cst2, cst3, \
                               res0, res1, res2, res3)                         \
 {                                                                             \
    tmp0 = _mm256_madd_epi16(lo_0, cst0);                                      \
    tmp1 = _mm256_madd_epi16(hi_0, cst0);                                      \
    tmp2 = _mm256_madd_epi16(lo_0, cst1);                                      \
    tmp3 = _mm256_madd_epi16(hi_0, cst1);                                      \
    tmp4 = _mm256_madd_epi16(lo_1, cst2);                                      \
    tmp5 = _mm256_madd_epi16(hi_1, cst2);                                      \
    tmp6 = _mm256_madd_epi16(lo_1, cst3);                                      \
    tmp7 = _mm256_madd_epi16(hi_1, cst3);                                      \
                                                                               \
    tmp0 = _mm256_add_epi32(tmp0, rounding);                                   \
    tmp1 = _mm256_add_epi32(tmp1, rounding);                                   \
    tmp2 = _mm256_add_epi32(tmp2, rounding);                                   \
    tmp3 = _mm256_add_epi32(tmp3, rounding);                                   \
    tmp4 = _mm256_add_epi32(tmp4, rounding);                                   \
    tmp5 = _mm256_add_epi32(tmp5, rounding);                                   \
    tmp6 = _mm256_add_epi32(tmp6, rounding);                                   \
    tmp7 = _mm256_add_epi32(tmp7, rounding);                                   \
                                                                               \
    tmp0 = _mm256_srai_epi32(tmp0, DCT_CONST_BITS);                            \
    tmp1 = _mm256_srai_epi32(tmp1, DCT_CONST_BITS);                            \
    tmp2 = _mm256_srai_epi32(tmp2, DCT_CONST_BITS);                            \
    tmp3 = _mm256_srai_epi32(tmp3, DCT_CONST_BITS);                            \
    tmp4 = _mm256_srai_epi32(tmp4, DCT_CONST_BITS);                            \
    tmp5 = _mm256_srai_epi32(tmp5, DCT_CONST_BITS);                            \
    tmp6 = _mm256_srai_epi32(tmp6, DCT_CONST_BITS);                            \
    tmp7 = _mm256_srai_epi32(tmp7, DCT_CONST_BITS);                            \
                                                                               \
    res0 = _mm256_packs_epi32(tmp0, tmp1);                                     \
    res1 = _mm256_packs_epi32(tmp2, tmp3);                                     \
    res2 = _mm256_packs_epi32(tmp4, tmp5);                                     \
    res3 = _mm256_packs_epi32(tmp6, tmp7);                                     \
  }

#define IDCT32                                                                   \
    /* Stage1 */                                                                 \
    {                                                                            \
      const __m256i lo_1_31 = _mm256_unpacklo_epi16(in[1], in[31]);              \
      const __m256i hi_1_31 = _mm256_unpackhi_epi16(in[1], in[31]);              \
      const __m256i lo_17_15 = _mm256_unpacklo_epi16(in[17], in[15]);            \
      const __m256i hi_17_15 = _mm256_unpackhi_epi16(in[17], in[15]);            \
                                                                                 \
      const __m256i lo_9_23 = _mm256_unpacklo_epi16(in[9], in[23]);              \
      const __m256i hi_9_23 = _mm256_unpackhi_epi16(in[9], in[23]);              \
      const __m256i lo_25_7 = _mm256_unpacklo_epi16(in[25], in[7]);              \
      const __m256i hi_25_7 = _mm256_unpackhi_epi16(in[25], in[7]);              \
                                                                                 \
      const __m256i lo_5_27 = _mm256_unpacklo_epi16(in[5], in[27]);              \
      const __m256i hi_5_27 = _mm256_unpackhi_epi16(in[5], in[27]);              \
      const __m256i lo_21_11 = _mm256_unpacklo_epi16(in[21], in[11]);            \
      const __m256i hi_21_11 = _mm256_unpackhi_epi16(in[21], in[11]);            \
                                                                                 \
      const __m256i lo_13_19 = _mm256_unpacklo_epi16(in[13], in[19]);            \
      const __m256i hi_13_19 = _mm256_unpackhi_epi16(in[13], in[19]);            \
      const __m256i lo_29_3 = _mm256_unpacklo_epi16(in[29], in[3]);              \
      const __m256i hi_29_3 = _mm256_unpackhi_epi16(in[29], in[3]);              \
                                                                                 \
      MULTIPLICATION_AND_ADD(lo_1_31, hi_1_31, lo_17_15, hi_17_15, stg1_0,       \
                             stg1_1, stg1_2, stg1_3, stp1_16, stp1_31, stp1_17,  \
                             stp1_30)                                            \
      MULTIPLICATION_AND_ADD(lo_9_23, hi_9_23, lo_25_7, hi_25_7, stg1_4, stg1_5, \
                             stg1_6, stg1_7, stp1_18, stp1_29, stp1_19, stp1_28) \
      MULTIPLICATION_AND_ADD(lo_5_27, hi_5_27, lo_21_11, hi_21_11, stg1_8,       \
                             stg1_9, stg1_10, stg1_11, stp1_20, stp1_27,         \
                             stp1_21, stp1_26)                                   \
      MULTIPLICATION_AND_ADD(lo_13_19, hi_13_19, lo_29_3, hi_29_3, stg1_12,      \
                             stg1_13, stg1_14, stg1_15, stp1_22, stp1_25,        \
                             stp1_23, stp1_24)                                   \
    }                                                                            \
                                                                                 \
    /* Stage2 */                                                                 \
    {                                                                            \
      const __m256i lo_2_30 = _mm256_unpacklo_epi16(in[2], in[30]);              \
      const __m256i hi_2_30 = _mm256_unpackhi_epi16(in[2], in[30]);              \
      const __m256i lo_18_14 = _mm256_unpacklo_epi16(in[18], in[14]);            \
      const __m256i hi_18_14 = _mm256_unpackhi_epi16(in[18], in[14]);            \
                                                                                 \
      const __m256i lo_10_22 = _mm256_unpacklo_epi16(in[10], in[22]);            \
      const __m256i hi_10_22 = _mm256_unpackhi_epi16(in[10], in[22]);            \
      const __m256i lo_26_6 = _mm256_unpacklo_epi16(in[26], in[6]);              \
      const __m256i hi_26_6 = _mm256_unpackhi_epi16(in[26], in[6]);              \
                                                                                 \
      MULTIPLICATION_AND_ADD(lo_2_30, hi_2_30, lo_18_14, hi_18_14, stg2_0,       \
                             stg2_1, stg2_2, stg2_3, stp2_8, stp2_15, stp2_9,    \
                             stp2_14)                                            \
      MULTIPLICATION_AND_ADD(lo_10_22, hi_10_22, lo_26_6, hi_26_6, stg2_4,       \
                             stg2_5, stg2_6, stg2_7, stp2_10, stp2_13, stp2_11,  \
                             stp2_12)                                            \
                                                                                 \
      stp2_16 = _mm256_add_epi16(stp1_16, stp1_17);                              \
      stp2_17 = _mm256_sub_epi16(stp1_16, stp1_17);                              \
      stp2_18 = _mm256_sub_epi16(stp1_19, stp1_18);                              \
      stp2_19 = _mm256_add_epi16(stp1_19, stp1_18);                              \
                                                                                 \
      stp2_20 = _mm256_add_epi16(stp1_20, stp1_21);                              \
      stp2_21 = _mm256_sub_epi16(stp1_20, stp1_21);                              \
      stp2_22 = _mm256_sub_epi16(stp1_23, stp1_22);                              \
      stp2_23 = _mm256_add_epi16(stp1_23, stp1_22);                              \
                                                                                 \
      stp2_24 = _mm256_add_epi16(stp1_24, stp1_25);                              \
      stp2_25 = _mm256_sub_epi16(stp1_24, stp1_25);                              \
      stp2_26 = _mm256_sub_epi16(stp1_27, stp1_26);                              \
      stp2_27 = _mm256_add_epi16(stp1_27, stp1_26);                              \
                                                                                 \
      stp2_28 = _mm256_add_epi16(stp1_28, stp1_29);                              \
      stp2_29 = _mm256_sub_epi16(stp1_28, stp1_29);                              \
      stp2_30 = _mm256_sub_epi16(stp1_31, stp1_30);                              \
      stp2_31 = _mm256_add_epi16(stp1_31, stp1_30);                              \
    }                                                                            \
                                                                                 \
    /* Stage3 */                                                                 \
    {                                                                            \
      const __m256i lo_4_28 = _mm256_unpacklo_epi16(in[4], in[28]);              \
      const __m256i hi_4_28 = _mm256_unpackhi_epi16(in[4], in[28]);              \
      const __m256i lo_20_12 = _mm256_unpacklo_epi16(in[20], in[12]);            \
      const __m256i hi_20_12 = _mm256_unpackhi_epi16(in[20], in[12]);            \
                                                                                 \
      const __m256i lo_17_30 = _mm256_unpacklo_epi16(stp2_17, stp2_30);          \
      const __m256i hi_17_30 = _mm256_unpackhi_epi16(stp2_17, stp2_30);          \
      const __m256i lo_18_29 = _mm256_unpacklo_epi16(stp2_18, stp2_29);          \
      const __m256i hi_18_29 = _mm256_unpackhi_epi16(stp2_18, stp2_29);          \
                                                                                 \
      const __m256i lo_21_26 = _mm256_unpacklo_epi16(stp2_21, stp2_26);          \
      const __m256i hi_21_26 = _mm256_unpackhi_epi16(stp2_21, stp2_26);          \
      const __m256i lo_22_25 = _mm256_unpacklo_epi16(stp2_22, stp2_25);          \
      const __m256i hi_22_25 = _mm256_unpackhi_epi16(stp2_22, stp2_25);          \
                                                                                 \
      MULTIPLICATION_AND_ADD(lo_4_28, hi_4_28, lo_20_12, hi_20_12, stg3_0,       \
                             stg3_1, stg3_2, stg3_3, stp1_4, stp1_7, stp1_5,     \
                             stp1_6)                                             \
                                                                                 \
      stp1_8 = _mm256_add_epi16(stp2_8, stp2_9);                                 \
      stp1_9 = _mm256_sub_epi16(stp2_8, stp2_9);                                 \
      stp1_10 = _mm256_sub_epi16(stp2_11, stp2_10);                              \
      stp1_11 = _mm256_add_epi16(stp2_11, stp2_10);                              \
      stp1_12 = _mm256_add_epi16(stp2_12, stp2_13);                              \
      stp1_13 = _mm256_sub_epi16(stp2_12, stp2_13);                              \
      stp1_14 = _mm256_sub_epi16(stp2_15, stp2_14);                              \
      stp1_15 = _mm256_add_epi16(stp2_15, stp2_14);                              \
                                                                                 \
      MULTIPLICATION_AND_ADD(lo_17_30, hi_17_30, lo_18_29, hi_18_29, stg3_4,     \
                             stg3_5, stg3_6, stg3_4, stp1_17, stp1_30, stp1_18,  \
                             stp1_29)                                            \
      MULTIPLICATION_AND_ADD(lo_21_26, hi_21_26, lo_22_25, hi_22_25, stg3_8,     \
                             stg3_9, stg3_10, stg3_8, stp1_21, stp1_26, stp1_22, \
                             stp1_25)                                            \
                                                                                 \
      stp1_16 = stp2_16;                                                         \
      stp1_31 = stp2_31;                                                         \
      stp1_19 = stp2_19;                                                         \
      stp1_20 = stp2_20;                                                         \
      stp1_23 = stp2_23;                                                         \
      stp1_24 = stp2_24;                                                         \
      stp1_27 = stp2_27;                                                         \
      stp1_28 = stp2_28;                                                         \
    }                                                                            \
                                                                                 \
    /* Stage4 */                                                                 \
    {                                                                            \
      const __m256i lo_0_16 = _mm256_unpacklo_epi16(in[0], in[16]);              \
      const __m256i hi_0_16 = _mm256_unpackhi_epi16(in[0], in[16]);              \
      const __m256i lo_8_24 = _mm256_unpacklo_epi16(in[8], in[24]);              \
      const __m256i hi_8_24 = _mm256_unpackhi_epi16(in[8], in[24]);              \
                                                                                 \
      const __m256i lo_9_14 = _mm256_unpacklo_epi16(stp1_9, stp1_14);            \
      const __m256i hi_9_14 = _mm256_unpackhi_epi16(stp1_9, stp1_14);            \
      const __m256i lo_10_13 = _mm256_unpacklo_epi16(stp1_10, stp1_13);          \
      const __m256i hi_10_13 = _mm256_unpackhi_epi16(stp1_10, stp1_13);          \
                                                                                 \
      MULTIPLICATION_AND_ADD(lo_0_16, hi_0_16, lo_8_24, hi_8_24, stg4_0, stg4_1, \
                             stg4_2, stg4_3, stp2_0, stp2_1, stp2_2, stp2_3)     \
                                                                                 \
      stp2_4 = _mm256_add_epi16(stp1_4, stp1_5);                                 \
      stp2_5 = _mm256_sub_epi16(stp1_4, stp1_5);                                 \
      stp2_6 = _mm256_sub_epi16(stp1_7, stp1_6);                                 \
      stp2_7 = _mm256_add_epi16(stp1_7, stp1_6);                                 \
                                                                                 \
      MULTIPLICATION_AND_ADD(lo_9_14, hi_9_14, lo_10_13, hi_10_13, stg4_4,       \
                             stg4_5, stg4_6, stg4_4, stp2_9, stp2_14, stp2_10,   \
                             stp2_13)                                            \
                                                                                 \
      stp2_8 = stp1_8;                                                           \
      stp2_15 = stp1_15;                                                         \
      stp2_11 = stp1_11;                                                         \
      stp2_12 = stp1_12;                                                         \
                                                                                 \
      stp2_16 = _mm256_add_epi16(stp1_16, stp1_19);                              \
      stp2_17 = _mm256_add_epi16(stp1_17, stp1_18);                              \
      stp2_18 = _mm256_sub_epi16(stp1_17, stp1_18);                              \
      stp2_19 = _mm256_sub_epi16(stp1_16, stp1_19);                              \
      stp2_20 = _mm256_sub_epi16(stp1_23, stp1_20);                              \
      stp2_21 = _mm256_sub_epi16(stp1_22, stp1_21);                              \
      stp2_22 = _mm256_add_epi16(stp1_22, stp1_21);                              \
      stp2_23 = _mm256_add_epi16(stp1_23, stp1_20);                              \
                                                                                 \
      stp2_24 = _mm256_add_epi16(stp1_24, stp1_27);                              \
      stp2_25 = _mm256_add_epi16(stp1_25, stp1_26);                              \
      stp2_26 = _mm256_sub_epi16(stp1_25, stp1_26);                              \
      stp2_27 = _mm256_sub_epi16(stp1_24, stp1_27);                              \
      stp2_28 = _mm256_sub_epi16(stp1_31, stp1_28);                              \
      stp2_29 = _mm256_sub_epi16(stp1_30, stp1_29);                              \
      stp2_30 = _mm256_add_epi16(stp1_29, stp1_30);                              \
      stp2_31 = _mm256_add_epi16(stp1_28, stp1_31);                              \
    }                                                                            \
                                                                                 \
    /* Stage5 */                                                                 \
    {                                                                            \
      const __m256i lo_6_5 = _mm256_unpacklo_epi16(stp2_6, stp2_5);              \
      const __m256i hi_6_5 = _mm256_unpackhi_epi16(stp2_6, stp2_5);              \
      const __m256i lo_18_29 = _mm256_unpacklo_epi16(stp2_18, stp2_29);          \
      const __m256i hi_18_29 = _mm256_unpackhi_epi16(stp2_18, stp2_29);          \
                                                                                 \
      const __m256i lo_19_28 = _mm256_unpacklo_epi16(stp2_19, stp2_28);          \
      const __m256i hi_19_28 = _mm256_unpackhi_epi16(stp2_19, stp2_28);          \
      const __m256i lo_20_27 = _mm256_unpacklo_epi16(stp2_20, stp2_27);          \
      const __m256i hi_20_27 = _mm256_unpackhi_epi16(stp2_20, stp2_27);          \
                                                                                 \
      const __m256i lo_21_26 = _mm256_unpacklo_epi16(stp2_21, stp2_26);          \
      const __m256i hi_21_26 = _mm256_unpackhi_epi16(stp2_21, stp2_26);          \
                                                                                 \
      stp1_0 = _mm256_add_epi16(stp2_0, stp2_3);                                 \
      stp1_1 = _mm256_add_epi16(stp2_1, stp2_2);                                 \
      stp1_2 = _mm256_sub_epi16(stp2_1, stp2_2);                                 \
      stp1_3 = _mm256_sub_epi16(stp2_0, stp2_3);                                 \
                                                                                 \
      tmp0 = _mm256_madd_epi16(lo_6_5, stg4_1);                                  \
      tmp1 = _mm256_madd_epi16(hi_6_5, stg4_1);                                  \
      tmp2 = _mm256_madd_epi16(lo_6_5, stg4_0);                                  \
      tmp3 = _mm256_madd_epi16(hi_6_5, stg4_0);                                  \
                                                                                 \
      tmp0 = _mm256_add_epi32(tmp0, rounding);                                   \
      tmp1 = _mm256_add_epi32(tmp1, rounding);                                   \
      tmp2 = _mm256_add_epi32(tmp2, rounding);                                   \
      tmp3 = _mm256_add_epi32(tmp3, rounding);                                   \
                                                                                 \
      tmp0 = _mm256_srai_epi32(tmp0, DCT_CONST_BITS);                            \
      tmp1 = _mm256_srai_epi32(tmp1, DCT_CONST_BITS);                            \
      tmp2 = _mm256_srai_epi32(tmp2, DCT_CONST_BITS);                            \
      tmp3 = _mm256_srai_epi32(tmp3, DCT_CONST_BITS);                            \
                                                                                 \
      stp1_5 = _mm256_packs_epi32(tmp0, tmp1);                                   \
      stp1_6 = _mm256_packs_epi32(tmp2, tmp3);                                   \
                                                                                 \
      stp1_4 = stp2_4;                                                           \
      stp1_7 = stp2_7;                                                           \
                                                                                 \
      stp1_8 = _mm256_add_epi16(stp2_8, stp2_11);                                \
      stp1_9 = _mm256_add_epi16(stp2_9, stp2_10);                                \
      stp1_10 = _mm256_sub_epi16(stp2_9, stp2_10);                               \
      stp1_11 = _mm256_sub_epi16(stp2_8, stp2_11);                               \
      stp1_12 = _mm256_sub_epi16(stp2_15, stp2_12);                              \
      stp1_13 = _mm256_sub_epi16(stp2_14, stp2_13);                              \
      stp1_14 = _mm256_add_epi16(stp2_14, stp2_13);                              \
      stp1_15 = _mm256_add_epi16(stp2_15, stp2_12);                              \
                                                                                 \
      stp1_16 = stp2_16;                                                         \
      stp1_17 = stp2_17;                                                         \
                                                                                 \
      MULTIPLICATION_AND_ADD(lo_18_29, hi_18_29, lo_19_28, hi_19_28, stg4_4,     \
                             stg4_5, stg4_4, stg4_5, stp1_18, stp1_29, stp1_19,  \
                             stp1_28)                                            \
      MULTIPLICATION_AND_ADD(lo_20_27, hi_20_27, lo_21_26, hi_21_26, stg4_6,     \
                             stg4_4, stg4_6, stg4_4, stp1_20, stp1_27, stp1_21,  \
                             stp1_26)                                            \
                                                                                 \
      stp1_22 = stp2_22;                                                         \
      stp1_23 = stp2_23;                                                         \
      stp1_24 = stp2_24;                                                         \
      stp1_25 = stp2_25;                                                         \
      stp1_30 = stp2_30;                                                         \
      stp1_31 = stp2_31;                                                         \
    }                                                                            \
                                                                                 \
    /* Stage6 */                                                                 \
    {                                                                            \
      const __m256i lo_10_13 = _mm256_unpacklo_epi16(stp1_10, stp1_13);          \
      const __m256i hi_10_13 = _mm256_unpackhi_epi16(stp1_10, stp1_13);          \
      const __m256i lo_11_12 = _mm256_unpacklo_epi16(stp1_11, stp1_12);          \
      const __m256i hi_11_12 = _mm256_unpackhi_epi16(stp1_11, stp1_12);          \
                                                                                 \
      stp2_0 = _mm256_add_epi16(stp1_0, stp1_7);                                 \
      stp2_1 = _mm256_add_epi16(stp1_1, stp1_6);                                 \
      stp2_2 = _mm256_add_epi16(stp1_2, stp1_5);                                 \
      stp2_3 = _mm256_add_epi16(stp1_3, stp1_4);                                 \
      stp2_4 = _mm256_sub_epi16(stp1_3, stp1_4);                                 \
      stp2_5 = _mm256_sub_epi16(stp1_2, stp1_5);                                 \
      stp2_6 = _mm256_sub_epi16(stp1_1, stp1_6);                                 \
      stp2_7 = _mm256_sub_epi16(stp1_0, stp1_7);                                 \
                                                                                 \
      stp2_8 = stp1_8;                                                           \
      stp2_9 = stp1_9;                                                           \
      stp2_14 = stp1_14;                                                         \
      stp2_15 = stp1_15;                                                         \
                                                                                 \
      MULTIPLICATION_AND_ADD(lo_10_13, hi_10_13, lo_11_12, hi_11_12, stg6_0,     \
                             stg4_0, stg6_0, stg4_0, stp2_10, stp2_13, stp2_11,  \
                             stp2_12)                                            \
                                                                                 \
      stp2_16 = _mm256_add_epi16(stp1_16, stp1_23);                              \
      stp2_17 = _mm256_add_epi16(stp1_17, stp1_22);                              \
      stp2_18 = _mm256_add_epi16(stp1_18, stp1_21);                              \
      stp2_19 = _mm256_add_epi16(stp1_19, stp1_20);                              \
      stp2_20 = _mm256_sub_epi16(stp1_19, stp1_20);                              \
      stp2_21 = _mm256_sub_epi16(stp1_18, stp1_21);                              \
      stp2_22 = _mm256_sub_epi16(stp1_17, stp1_22);                              \
      stp2_23 = _mm256_sub_epi16(stp1_16, stp1_23);                              \
                                                                                 \
      stp2_24 = _mm256_sub_epi16(stp1_31, stp1_24);                              \
      stp2_25 = _mm256_sub_epi16(stp1_30, stp1_25);                              \
      stp2_26 = _mm256_sub_epi16(stp1_29, stp1_26);                              \
      stp2_27 = _mm256_sub_epi16(stp1_28, stp1_27);                              \
      stp2_28 = _mm256_add_epi16(stp1_27, stp1_28);                              \
      stp2_29 = _mm256_add_epi16(stp1_26, stp1_29);                              \
      stp2_30 = _mm256_add_epi16(stp1_25, stp1_30);                              \
      stp2_31 = _mm256_add_epi16(stp1_24, stp1_31);                              \
    }                                                                            \
                                                                                 \
    /* Stage7 */                                                                 \
    {                                                                            \
      const __m256i lo_20_27 = _mm256_unpacklo_epi16(stp2_20, stp2_27);          \
      const __m256i hi_20_27 = _mm256_unpackhi_epi16(stp2_20, stp2_27);          \
      const __m256i lo_21_26 = _mm256_unpacklo_epi16(stp2_21, stp2_26);          \
      const __m256i hi_21_26 = _mm256_unpackhi_epi16(stp2_21, stp2_26);          \
                                                                                 \
      const __m256i lo_22_25 = _mm256_unpacklo_epi16(stp2_22, stp2_25);          \
      const __m256i hi_22_25 = _mm256_unpackhi_epi16(stp2_22, stp2_25);          \
      const __m256i lo_23_24 = _mm256_unpacklo_epi16(stp2_23, stp2_24);          \
      const __m256i hi_23_24 = _mm256_unpackhi_epi16(stp2_23, stp2_24);          \
                                                                                 \
      stp1_0 = _mm256_add_epi16(stp2_0, stp2_15);                                \
      stp1_1 = _mm256_add_epi16(stp2_1, stp2_14);                                \
      stp1_2 = _mm256_add_epi16(stp2_2, stp2_13);                                \
      stp1_3 = _mm256_add_epi16(stp2_3, stp2_12);                                \
      stp1_4 = _mm256_add_epi16(stp2_4, stp2_11);                                \
      stp1_5 = _mm256_add_epi16(stp2_5, stp2_10);                                \
      stp1_6 = _mm256_add_epi16(stp2_6, stp2_9);                                 \
      stp1_7 = _mm256_add_epi16(stp2_7, stp2_8);                                 \
      stp1_8 = _mm256_sub_epi16(stp2_7, stp2_8);                                 \
      stp1_9 = _mm256_sub_epi16(stp2_6, stp2_9);                                 \
      stp1_10 = _mm256_sub_epi16(stp2_5, stp2_10);                               \
      stp1_11 = _mm256_sub_epi16(stp2_4, stp2_11);                               \
      stp1_12 = _mm256_sub_epi16(stp2_3, stp2_12);                               \
      stp1_13 = _mm256_sub_epi16(stp2_2, stp2_13);                               \
      stp1_14 = _mm256_sub_epi16(stp2_1, stp2_14);                               \
      stp1_15 = _mm256_sub_epi16(stp2_0, stp2_15);                               \
                                                                                 \
      stp1_16 = stp2_16;                                                         \
      stp1_17 = stp2_17;                                                         \
      stp1_18 = stp2_18;                                                         \
      stp1_19 = stp2_19;                                                         \
                                                                                 \
      MULTIPLICATION_AND_ADD(lo_20_27, hi_20_27, lo_21_26, hi_21_26, stg6_0,     \
                             stg4_0, stg6_0, stg4_0, stp1_20, stp1_27, stp1_21,  \
                             stp1_26)                                            \
      MULTIPLICATION_AND_ADD(lo_22_25, hi_22_25, lo_23_24, hi_23_24, stg6_0,     \
                             stg4_0, stg6_0, stg4_0, stp1_22, stp1_25, stp1_23,  \
                             stp1_24)                                            \
                                                                                 \
      stp1_28 = stp2_28;                                                         \
      stp1_29 = stp2_29;                                                         \
      stp1_30 = stp2_30;                                                         \
      stp1_31 = stp2_31;                                                         \
    }

    template <typename T> void idct32x32_avx2(const short *input, T *dest, int stride)
    {
        const __m256i rounding = _mm256_set1_epi32(DCT_CONST_ROUNDING);
        const __m256i final_rounding = _mm256_set1_epi16(1 << 5);
        //const __m256i zero = _mm256_setzero_si256();
        const __m256i stg1_0 = pair_set_epi16(cospi_31_64, -cospi_1_64);
        const __m256i stg1_1 = pair_set_epi16(cospi_1_64, cospi_31_64);
        const __m256i stg1_2 = pair_set_epi16(cospi_15_64, -cospi_17_64);
        const __m256i stg1_3 = pair_set_epi16(cospi_17_64, cospi_15_64);
        const __m256i stg1_4 = pair_set_epi16(cospi_23_64, -cospi_9_64);
        const __m256i stg1_5 = pair_set_epi16(cospi_9_64, cospi_23_64);
        const __m256i stg1_6 = pair_set_epi16(cospi_7_64, -cospi_25_64);
        const __m256i stg1_7 = pair_set_epi16(cospi_25_64, cospi_7_64);
        const __m256i stg1_8 = pair_set_epi16(cospi_27_64, -cospi_5_64);
        const __m256i stg1_9 = pair_set_epi16(cospi_5_64, cospi_27_64);
        const __m256i stg1_10 = pair_set_epi16(cospi_11_64, -cospi_21_64);
        const __m256i stg1_11 = pair_set_epi16(cospi_21_64, cospi_11_64);
        const __m256i stg1_12 = pair_set_epi16(cospi_19_64, -cospi_13_64);
        const __m256i stg1_13 = pair_set_epi16(cospi_13_64, cospi_19_64);
        const __m256i stg1_14 = pair_set_epi16(cospi_3_64, -cospi_29_64);
        const __m256i stg1_15 = pair_set_epi16(cospi_29_64, cospi_3_64);
        const __m256i stg2_0 = pair_set_epi16(cospi_30_64, -cospi_2_64);
        const __m256i stg2_1 = pair_set_epi16(cospi_2_64, cospi_30_64);
        const __m256i stg2_2 = pair_set_epi16(cospi_14_64, -cospi_18_64);
        const __m256i stg2_3 = pair_set_epi16(cospi_18_64, cospi_14_64);
        const __m256i stg2_4 = pair_set_epi16(cospi_22_64, -cospi_10_64);
        const __m256i stg2_5 = pair_set_epi16(cospi_10_64, cospi_22_64);
        const __m256i stg2_6 = pair_set_epi16(cospi_6_64, -cospi_26_64);
        const __m256i stg2_7 = pair_set_epi16(cospi_26_64, cospi_6_64);
        const __m256i stg3_0 = pair_set_epi16(cospi_28_64, -cospi_4_64);
        const __m256i stg3_1 = pair_set_epi16(cospi_4_64, cospi_28_64);
        const __m256i stg3_2 = pair_set_epi16(cospi_12_64, -cospi_20_64);
        const __m256i stg3_3 = pair_set_epi16(cospi_20_64, cospi_12_64);
        const __m256i stg3_4 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m256i stg3_5 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m256i stg3_6 = pair_set_epi16(-cospi_28_64, -cospi_4_64);
        const __m256i stg3_8 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m256i stg3_9 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m256i stg3_10 = pair_set_epi16(-cospi_12_64, -cospi_20_64);
        const __m256i stg4_0 = pair_set_epi16(cospi_16_64, cospi_16_64);
        const __m256i stg4_1 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m256i stg4_2 = pair_set_epi16(cospi_24_64, -cospi_8_64);
        const __m256i stg4_3 = pair_set_epi16(cospi_8_64, cospi_24_64);
        const __m256i stg4_4 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m256i stg4_5 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m256i stg4_6 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
        const __m256i stg6_0 = pair_set_epi16(-cospi_16_64, cospi_16_64);
        __m256i in[32], col[64];
        __m256i stp1_0, stp1_1, stp1_2, stp1_3, stp1_4, stp1_5, stp1_6, stp1_7,
            stp1_8, stp1_9, stp1_10, stp1_11, stp1_12, stp1_13, stp1_14, stp1_15,
            stp1_16, stp1_17, stp1_18, stp1_19, stp1_20, stp1_21, stp1_22, stp1_23,
            stp1_24, stp1_25, stp1_26, stp1_27, stp1_28, stp1_29, stp1_30, stp1_31;
        __m256i stp2_0, stp2_1, stp2_2, stp2_3, stp2_4, stp2_5, stp2_6, stp2_7,
            stp2_8, stp2_9, stp2_10, stp2_11, stp2_12, stp2_13, stp2_14, stp2_15,
            stp2_16, stp2_17, stp2_18, stp2_19, stp2_20, stp2_21, stp2_22, stp2_23,
            stp2_24, stp2_25, stp2_26, stp2_27, stp2_28, stp2_29, stp2_30, stp2_31;
        __m256i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
        int i, j, i32;
        for (i = 0; i < 2; i++) {
            i32 = (i << 5);
            in[0]  = loada_si256(input); input += 16;
            in[16] = loada_si256(input); input += 16;
            in[1]  = loada_si256(input); input += 16;
            in[17] = loada_si256(input); input += 16;
            in[2]  = loada_si256(input); input += 16;
            in[18] = loada_si256(input); input += 16;
            in[3]  = loada_si256(input); input += 16;
            in[19] = loada_si256(input); input += 16;
            in[4]  = loada_si256(input); input += 16;
            in[20] = loada_si256(input); input += 16;
            in[5]  = loada_si256(input); input += 16;
            in[21] = loada_si256(input); input += 16;
            in[6]  = loada_si256(input); input += 16;
            in[22] = loada_si256(input); input += 16;
            in[7]  = loada_si256(input); input += 16;
            in[23] = loada_si256(input); input += 16;
            in[8]  = loada_si256(input); input += 16;
            in[24] = loada_si256(input); input += 16;
            in[9]  = loada_si256(input); input += 16;
            in[25] = loada_si256(input); input += 16;
            in[10] = loada_si256(input); input += 16;
            in[26] = loada_si256(input); input += 16;
            in[11] = loada_si256(input); input += 16;
            in[27] = loada_si256(input); input += 16;
            in[12] = loada_si256(input); input += 16;
            in[28] = loada_si256(input); input += 16;
            in[13] = loada_si256(input); input += 16;
            in[29] = loada_si256(input); input += 16;
            in[14] = loada_si256(input); input += 16;
            in[30] = loada_si256(input); input += 16;
            in[15] = loada_si256(input); input += 16;
            in[31] = loada_si256(input); input += 16;
            array_transpose_16x16(in, in);
            array_transpose_16x16(in + 16, in + 16);
            IDCT32
            col[i32 + 0]  = _mm256_add_epi16(stp1_0, stp1_31);
            col[i32 + 1]  = _mm256_add_epi16(stp1_1, stp1_30);
            col[i32 + 2]  = _mm256_add_epi16(stp1_2, stp1_29);
            col[i32 + 3]  = _mm256_add_epi16(stp1_3, stp1_28);
            col[i32 + 4]  = _mm256_add_epi16(stp1_4, stp1_27);
            col[i32 + 5]  = _mm256_add_epi16(stp1_5, stp1_26);
            col[i32 + 6]  = _mm256_add_epi16(stp1_6, stp1_25);
            col[i32 + 7]  = _mm256_add_epi16(stp1_7, stp1_24);
            col[i32 + 8]  = _mm256_add_epi16(stp1_8, stp1_23);
            col[i32 + 9]  = _mm256_add_epi16(stp1_9, stp1_22);
            col[i32 + 10] = _mm256_add_epi16(stp1_10, stp1_21);
            col[i32 + 11] = _mm256_add_epi16(stp1_11, stp1_20);
            col[i32 + 12] = _mm256_add_epi16(stp1_12, stp1_19);
            col[i32 + 13] = _mm256_add_epi16(stp1_13, stp1_18);
            col[i32 + 14] = _mm256_add_epi16(stp1_14, stp1_17);
            col[i32 + 15] = _mm256_add_epi16(stp1_15, stp1_16);
            col[i32 + 16] = _mm256_sub_epi16(stp1_15, stp1_16);
            col[i32 + 17] = _mm256_sub_epi16(stp1_14, stp1_17);
            col[i32 + 18] = _mm256_sub_epi16(stp1_13, stp1_18);
            col[i32 + 19] = _mm256_sub_epi16(stp1_12, stp1_19);
            col[i32 + 20] = _mm256_sub_epi16(stp1_11, stp1_20);
            col[i32 + 21] = _mm256_sub_epi16(stp1_10, stp1_21);
            col[i32 + 22] = _mm256_sub_epi16(stp1_9, stp1_22);
            col[i32 + 23] = _mm256_sub_epi16(stp1_8, stp1_23);
            col[i32 + 24] = _mm256_sub_epi16(stp1_7, stp1_24);
            col[i32 + 25] = _mm256_sub_epi16(stp1_6, stp1_25);
            col[i32 + 26] = _mm256_sub_epi16(stp1_5, stp1_26);
            col[i32 + 27] = _mm256_sub_epi16(stp1_4, stp1_27);
            col[i32 + 28] = _mm256_sub_epi16(stp1_3, stp1_28);
            col[i32 + 29] = _mm256_sub_epi16(stp1_2, stp1_29);
            col[i32 + 30] = _mm256_sub_epi16(stp1_1, stp1_30);
            col[i32 + 31] = _mm256_sub_epi16(stp1_0, stp1_31);
        }
        for (i = 0; i < 2; i++) {
            j = i << 4;
            array_transpose_16x16(col + j + 0,  in + 0);
            array_transpose_16x16(col + j + 32, in + 16);
            IDCT32
            in[0] = _mm256_add_epi16(stp1_0, stp1_31);
            in[1] = _mm256_add_epi16(stp1_1, stp1_30);
            in[2] = _mm256_add_epi16(stp1_2, stp1_29);
            in[3] = _mm256_add_epi16(stp1_3, stp1_28);
            in[4] = _mm256_add_epi16(stp1_4, stp1_27);
            in[5] = _mm256_add_epi16(stp1_5, stp1_26);
            in[6] = _mm256_add_epi16(stp1_6, stp1_25);
            in[7] = _mm256_add_epi16(stp1_7, stp1_24);
            in[8] = _mm256_add_epi16(stp1_8, stp1_23);
            in[9] = _mm256_add_epi16(stp1_9, stp1_22);
            in[10] = _mm256_add_epi16(stp1_10, stp1_21);
            in[11] = _mm256_add_epi16(stp1_11, stp1_20);
            in[12] = _mm256_add_epi16(stp1_12, stp1_19);
            in[13] = _mm256_add_epi16(stp1_13, stp1_18);
            in[14] = _mm256_add_epi16(stp1_14, stp1_17);
            in[15] = _mm256_add_epi16(stp1_15, stp1_16);
            in[16] = _mm256_sub_epi16(stp1_15, stp1_16);
            in[17] = _mm256_sub_epi16(stp1_14, stp1_17);
            in[18] = _mm256_sub_epi16(stp1_13, stp1_18);
            in[19] = _mm256_sub_epi16(stp1_12, stp1_19);
            in[20] = _mm256_sub_epi16(stp1_11, stp1_20);
            in[21] = _mm256_sub_epi16(stp1_10, stp1_21);
            in[22] = _mm256_sub_epi16(stp1_9, stp1_22);
            in[23] = _mm256_sub_epi16(stp1_8, stp1_23);
            in[24] = _mm256_sub_epi16(stp1_7, stp1_24);
            in[25] = _mm256_sub_epi16(stp1_6, stp1_25);
            in[26] = _mm256_sub_epi16(stp1_5, stp1_26);
            in[27] = _mm256_sub_epi16(stp1_4, stp1_27);
            in[28] = _mm256_sub_epi16(stp1_3, stp1_28);
            in[29] = _mm256_sub_epi16(stp1_2, stp1_29);
            in[30] = _mm256_sub_epi16(stp1_1, stp1_30);
            in[31] = _mm256_sub_epi16(stp1_0, stp1_31);

            in[0]  = _mm256_adds_epi16(in[0],  final_rounding);
            in[1]  = _mm256_adds_epi16(in[1],  final_rounding);
            in[2]  = _mm256_adds_epi16(in[2],  final_rounding);
            in[3]  = _mm256_adds_epi16(in[3],  final_rounding);
            in[4]  = _mm256_adds_epi16(in[4],  final_rounding);
            in[5]  = _mm256_adds_epi16(in[5],  final_rounding);
            in[6]  = _mm256_adds_epi16(in[6],  final_rounding);
            in[7]  = _mm256_adds_epi16(in[7],  final_rounding);
            in[8]  = _mm256_adds_epi16(in[8],  final_rounding);
            in[9]  = _mm256_adds_epi16(in[9],  final_rounding);
            in[10] = _mm256_adds_epi16(in[10], final_rounding);
            in[11] = _mm256_adds_epi16(in[11], final_rounding);
            in[12] = _mm256_adds_epi16(in[12], final_rounding);
            in[13] = _mm256_adds_epi16(in[13], final_rounding);
            in[14] = _mm256_adds_epi16(in[14], final_rounding);
            in[15] = _mm256_adds_epi16(in[15], final_rounding);
            in[16] = _mm256_adds_epi16(in[16], final_rounding);
            in[17] = _mm256_adds_epi16(in[17], final_rounding);
            in[18] = _mm256_adds_epi16(in[18], final_rounding);
            in[19] = _mm256_adds_epi16(in[19], final_rounding);
            in[20] = _mm256_adds_epi16(in[20], final_rounding);
            in[21] = _mm256_adds_epi16(in[21], final_rounding);
            in[22] = _mm256_adds_epi16(in[22], final_rounding);
            in[23] = _mm256_adds_epi16(in[23], final_rounding);
            in[24] = _mm256_adds_epi16(in[24], final_rounding);
            in[25] = _mm256_adds_epi16(in[25], final_rounding);
            in[26] = _mm256_adds_epi16(in[26], final_rounding);
            in[27] = _mm256_adds_epi16(in[27], final_rounding);
            in[28] = _mm256_adds_epi16(in[28], final_rounding);
            in[29] = _mm256_adds_epi16(in[29], final_rounding);
            in[30] = _mm256_adds_epi16(in[30], final_rounding);
            in[31] = _mm256_adds_epi16(in[31], final_rounding);
            in[0]  = _mm256_srai_epi16(in[0],  6);
            in[1]  = _mm256_srai_epi16(in[1],  6);
            in[2]  = _mm256_srai_epi16(in[2],  6);
            in[3]  = _mm256_srai_epi16(in[3],  6);
            in[4]  = _mm256_srai_epi16(in[4],  6);
            in[5]  = _mm256_srai_epi16(in[5],  6);
            in[6]  = _mm256_srai_epi16(in[6],  6);
            in[7]  = _mm256_srai_epi16(in[7],  6);
            in[8]  = _mm256_srai_epi16(in[8],  6);
            in[9]  = _mm256_srai_epi16(in[9],  6);
            in[10] = _mm256_srai_epi16(in[10], 6);
            in[11] = _mm256_srai_epi16(in[11], 6);
            in[12] = _mm256_srai_epi16(in[12], 6);
            in[13] = _mm256_srai_epi16(in[13], 6);
            in[14] = _mm256_srai_epi16(in[14], 6);
            in[15] = _mm256_srai_epi16(in[15], 6);
            in[16] = _mm256_srai_epi16(in[16], 6);
            in[17] = _mm256_srai_epi16(in[17], 6);
            in[18] = _mm256_srai_epi16(in[18], 6);
            in[19] = _mm256_srai_epi16(in[19], 6);
            in[20] = _mm256_srai_epi16(in[20], 6);
            in[21] = _mm256_srai_epi16(in[21], 6);
            in[22] = _mm256_srai_epi16(in[22], 6);
            in[23] = _mm256_srai_epi16(in[23], 6);
            in[24] = _mm256_srai_epi16(in[24], 6);
            in[25] = _mm256_srai_epi16(in[25], 6);
            in[26] = _mm256_srai_epi16(in[26], 6);
            in[27] = _mm256_srai_epi16(in[27], 6);
            in[28] = _mm256_srai_epi16(in[28], 6);
            in[29] = _mm256_srai_epi16(in[29], 6);
            in[30] = _mm256_srai_epi16(in[30], 6);
            in[31] = _mm256_srai_epi16(in[31], 6);
            recon_and_store(dest +  0 * stride, dest +  1 * stride, in +  0);
            recon_and_store(dest +  2 * stride, dest +  3 * stride, in +  2);
            recon_and_store(dest +  4 * stride, dest +  5 * stride, in +  4);
            recon_and_store(dest +  6 * stride, dest +  7 * stride, in +  6);
            recon_and_store(dest +  8 * stride, dest +  9 * stride, in +  8);
            recon_and_store(dest + 10 * stride, dest + 11 * stride, in + 10);
            recon_and_store(dest + 12 * stride, dest + 13 * stride, in + 12);
            recon_and_store(dest + 14 * stride, dest + 15 * stride, in + 14);
            recon_and_store(dest + 16 * stride, dest + 17 * stride, in + 16);
            recon_and_store(dest + 18 * stride, dest + 19 * stride, in + 18);
            recon_and_store(dest + 20 * stride, dest + 21 * stride, in + 20);
            recon_and_store(dest + 22 * stride, dest + 23 * stride, in + 22);
            recon_and_store(dest + 24 * stride, dest + 25 * stride, in + 24);
            recon_and_store(dest + 26 * stride, dest + 27 * stride, in + 26);
            recon_and_store(dest + 28 * stride, dest + 29 * stride, in + 28);
            recon_and_store(dest + 30 * stride, dest + 31 * stride, in + 30);
            dest += 16;
        }
    }
};  // namespace details

namespace AV1PP {
    template <int size, int type> void itransform_add_vp9_avx2(const short *src, unsigned char *dst, int pitchDst);

    template <> void itransform_add_vp9_avx2<TX_16X16, DCT_DCT>(const short *src, unsigned char *dst, int pitchDst) {
        details::iht16x16_256_avx2<DCT_DCT>(src, dst, pitchDst);
    }
    template <> void itransform_add_vp9_avx2<TX_16X16, ADST_DCT>(const short *src, unsigned char *dst, int pitchDst) {
        details::iht16x16_256_avx2<ADST_DCT>(src, dst, pitchDst);
    }
    template <> void itransform_add_vp9_avx2<TX_16X16, DCT_ADST>(const short *src, unsigned char *dst, int pitchDst) {
        details::iht16x16_256_avx2<DCT_ADST>(src, dst, pitchDst);
    }
    template <> void itransform_add_vp9_avx2<TX_16X16, ADST_ADST>(const short *src, unsigned char *dst, int pitchDst) {
        details::iht16x16_256_avx2<ADST_ADST>(src, dst, pitchDst);
    }

    template <> void itransform_add_vp9_avx2<TX_32X32, DCT_DCT>(const short *src, unsigned char *dst, int pitchSrc) {
        details::idct32x32_avx2(src, dst, pitchSrc);
    }

    template <int size, int type> void itransform_vp9_avx2(const short *src, short *dst, int pitchDst);

    template <> void itransform_vp9_avx2<TX_16X16, DCT_DCT>(const short *src, short *dst, int pitchDst) {
        details::iht16x16_256_avx2<DCT_DCT>(src, dst, pitchDst);
    }
    template <> void itransform_vp9_avx2<TX_16X16, ADST_DCT>(const short *src, short *dst, int pitchDst) {
        details::iht16x16_256_avx2<ADST_DCT>(src, dst, pitchDst);
    }
    template <> void itransform_vp9_avx2<TX_16X16, DCT_ADST>(const short *src, short *dst, int pitchDst) {
        details::iht16x16_256_avx2<DCT_ADST>(src, dst, pitchDst);
    }
    template <> void itransform_vp9_avx2<TX_16X16, ADST_ADST>(const short *src, short *dst, int pitchDst) {
        details::iht16x16_256_avx2<ADST_ADST>(src, dst, pitchDst);
    }

    template <> void itransform_vp9_avx2<TX_32X32, DCT_DCT>(const short *src, short *dst, int pitchSrc) {
        details::idct32x32_avx2(src, dst, pitchSrc);
    }

};  // namespace AV1PP
