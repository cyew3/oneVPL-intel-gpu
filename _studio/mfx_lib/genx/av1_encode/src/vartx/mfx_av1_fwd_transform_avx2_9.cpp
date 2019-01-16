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
#include "mfx_av1_transform_common_avx2_9.h"

enum { TX_4X4=0, TX_8X8=1, TX_16X16=2, TX_32X32=3, TX_SIZES=4 };
enum { DCT_DCT=0, ADST_DCT=1, DCT_ADST=2, ADST_ADST=3 };

namespace details {
    using namespace VP9PP;

    static inline __m256i mult_round_shift(const __m256i *pin0, const __m256i *pin1,
                                    const __m256i *pmultiplier, const __m256i *prounding, const int shift)
    {
        const __m256i u0 = _mm256_madd_epi16(*pin0, *pmultiplier);
        const __m256i u1 = _mm256_madd_epi16(*pin1, *pmultiplier);
        const __m256i v0 = _mm256_add_epi32(u0, *prounding);
        const __m256i v1 = _mm256_add_epi32(u1, *prounding);
        const __m256i w0 = _mm256_srai_epi32(v0, shift);
        const __m256i w1 = _mm256_srai_epi32(v1, shift);
        return _mm256_packs_epi32(w0, w1);
    }

    static inline __m256i k_madd_epi32(__m256i a, __m256i b) {
        __m256i buf0, buf1;
        buf0 = _mm256_mul_epu32(a, b);
        a = _mm256_srli_epi64(a, 32);
        b = _mm256_srli_epi64(b, 32);
        buf1 = _mm256_mul_epu32(a, b);
        return _mm256_add_epi64(buf0, buf1);
    }

    static inline __m256i k_packs_epi64(__m256i a, __m256i b) {
        __m256i buf0 = _mm256_shuffle_epi32(a, _MM_SHUFFLE(0, 0, 2, 0));
        __m256i buf1 = _mm256_shuffle_epi32(b, _MM_SHUFFLE(0, 0, 2, 0));
        return _mm256_unpacklo_epi64(buf0, buf1);
    }

    template <bool isFirstPassOf32x32> static inline void transpose_and_output16x16(const __m256i *in, short *out_ptr, int pitch)
    {
        // 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
        // 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F
        // 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F
        // 30 31 32 33 34 35 36 37 38 39 3A 3B 3C 3D 3E 3F
        // 40 41 42 43 44 45 46 47 48 49 4A 4B 4C 4D 4E 4F
        // 50 51 52 53 54 55 56 57 58 59 5A 5B 5C 5D 5E 5F
        // 60 61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F
        // 70 71 72 73 74 75 76 77 78 79 7A 7B 7C 7D 7E 7F
        // 80 81 82 83 84 85 86 87 88 89 8A 8B 8C 8D 8E 8F
        // 90 91 92 93 94 95 96 97 98 99 9A 9B 9C 9D 9E 9F
        // A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA AB AC AD AE AF
        // B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA BB BC BD BE BF
        // C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF
        // D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA DB DC DD DE DF
        // E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA EB EC ED EE EF
        // F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA FB FC FD FE FF
        const __m256i tr0_0  = _mm256_unpacklo_epi16(in[ 0], in[ 1]);
        const __m256i tr0_1  = _mm256_unpacklo_epi16(in[ 2], in[ 3]);
        const __m256i tr0_2  = _mm256_unpacklo_epi16(in[ 4], in[ 5]);
        const __m256i tr0_3  = _mm256_unpacklo_epi16(in[ 6], in[ 7]);
        const __m256i tr0_4  = _mm256_unpacklo_epi16(in[ 8], in[ 9]);
        const __m256i tr0_5  = _mm256_unpacklo_epi16(in[10], in[11]);
        const __m256i tr0_6  = _mm256_unpacklo_epi16(in[12], in[13]);
        const __m256i tr0_7  = _mm256_unpacklo_epi16(in[14], in[15]);
        const __m256i tr0_8  = _mm256_unpackhi_epi16(in[ 0], in[ 1]);
        const __m256i tr0_9  = _mm256_unpackhi_epi16(in[ 2], in[ 3]);
        const __m256i tr0_10 = _mm256_unpackhi_epi16(in[ 4], in[ 5]);
        const __m256i tr0_11 = _mm256_unpackhi_epi16(in[ 6], in[ 7]);
        const __m256i tr0_12 = _mm256_unpackhi_epi16(in[ 8], in[ 9]);
        const __m256i tr0_13 = _mm256_unpackhi_epi16(in[10], in[11]);
        const __m256i tr0_14 = _mm256_unpackhi_epi16(in[12], in[13]);
        const __m256i tr0_15 = _mm256_unpackhi_epi16(in[14], in[15]);
        // 00 10 01 11 02 12 03 13 08 18 09 19 0A 1A 0B 1B
        // 20 30 21 31 22 32 23 33 28 38 29 39 2A 3A 2B 3B
        // 40 50 41 51 42 52 43 53 48 58 49 59 4A 5A 4B 5B
        // 60 70 61 71 62 72 63 73 68 78 69 79 6A 7A 6B 7B
        // 80 90 81 91 82 92 83 93 88 98 89 99 8A 9A 8B 9B
        // A0 B0 A1 B1 A2 B2 A3 B3 A8 B8 A9 B9 AA BA AB BB
        // C0 D0 C1 D1 C2 D2 C3 D3 C8 D8 C9 D9 CA DA CB DB
        // E0 F0 E1 F1 E2 F2 E3 F3 E8 F8 E9 F9 EA FA EB FB
        // 04 14 05 15 06 16 07 17 0C 1C 0D 1D 0E 1E 0F 1F
        // 24 34 25 35 26 36 27 37 2C 3C 2D 3D 2E 3E 2F 3F
        // 44 54 45 55 46 56 47 57 4C 5C 4D 5D 4E 5E 4F 5F
        // 64 74 65 75 66 76 67 77 6C 7C 6D 7D 6E 7E 6F 7F
        // 84 94 85 95 86 96 87 97 8C 9C 8D 9D 8E 9E 8F 9F
        // A4 B4 A5 B5 A6 B6 A7 B7 AC BC AD BD AE BE AF BF
        // C4 D4 C5 D5 C6 D6 C7 D7 CC DC CD DD CE DE CF DF
        // E4 F4 E5 F5 E6 F6 E7 F7 EC FC ED FD EE FE EF FF
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
        // 00 10 20 30 01 11 01 11 08 18 28 38 09 19 29 39
        // 40 50 60 70 41 51 61 71 48 58 68 78 49 59 69 79
        // 80 90 A0 B0 81 91 A1 B1 88 98 A8 B8 89 99 A9 B9
        // C0 D0 E0 F0 C1 D1 E1 F1 C8 D8 E8 F8 C9 D9 E9 F9
        // 04 14 24 34 05 15 25 35 0C 1C 2C 3C 0D 1D 2D 3D
        // 44 54 64 74 45 55 65 75 4C 5C 6C 7C 4D 5D 6D 7D
        // 84 94 A4 B4 85 95 A5 B5 8C 9C AC BC 8D 9D AD BD
        // C4 D4 E4 F4 C5 D5 E5 F5 CC DC EC FC CD DD ED FD
        // 02 12 22 32 03 13 03 13 0A 1A 2A 3A 0B 1B 2B 3B
        // 42 52 62 72 43 53 63 73 4A 5A 6A 7A 4B 5B 6B 7B
        // 82 92 A2 B2 83 93 A3 B3 8A 9A AA BA 8B 9B AB BB
        // C2 D2 E2 F2 C3 D3 E3 F3 CA DA EA FA CB DB EB FB
        // 06 16 26 36 07 17 27 37 0E 1E 2E 3E 0F 1F 2F 3F
        // 46 56 66 76 47 57 67 77 4E 5E 6E 7E 4F 5F 6F 7F
        // 86 96 A6 B6 87 97 A7 B7 8E 9E AE BE 8F 9F AF BF
        // C6 D6 E6 F6 C7 D7 E7 F7 CE DE EE FE CF DF EF FF
        __m256i tr2_0  = _mm256_unpacklo_epi64(tr1_0,  tr1_1);
        __m256i tr2_1  = _mm256_unpacklo_epi64(tr1_2,  tr1_3);
        __m256i tr2_2  = _mm256_unpacklo_epi64(tr1_4,  tr1_5);
        __m256i tr2_3  = _mm256_unpacklo_epi64(tr1_6,  tr1_7);
        __m256i tr2_4  = _mm256_unpacklo_epi64(tr1_8,  tr1_9);
        __m256i tr2_5  = _mm256_unpacklo_epi64(tr1_10, tr1_11);
        __m256i tr2_6  = _mm256_unpacklo_epi64(tr1_12, tr1_13);
        __m256i tr2_7  = _mm256_unpacklo_epi64(tr1_14, tr1_15);
        __m256i tr2_8  = _mm256_unpackhi_epi64(tr1_0,  tr1_1);
        __m256i tr2_9  = _mm256_unpackhi_epi64(tr1_2,  tr1_3);
        __m256i tr2_10 = _mm256_unpackhi_epi64(tr1_4,  tr1_5);
        __m256i tr2_11 = _mm256_unpackhi_epi64(tr1_6,  tr1_7);
        __m256i tr2_12 = _mm256_unpackhi_epi64(tr1_8,  tr1_9);
        __m256i tr2_13 = _mm256_unpackhi_epi64(tr1_10, tr1_11);
        __m256i tr2_14 = _mm256_unpackhi_epi64(tr1_12, tr1_13);
        __m256i tr2_15 = _mm256_unpackhi_epi64(tr1_14, tr1_15);
        // 00 10 20 30 40 50 60 70 08 18 28 38 48 58 68 78
        // 80 90 A0 B0 C0 D0 E0 F0 88 98 A8 B8 C8 D8 E8 F8
        // 04 14 24 34 44 54 64 74 0C 1C 2C 3C 4C 5C 6C 7C
        // 84 94 A4 B4 C4 D4 E4 F4 8C 9C AC BC CC DC EC FC
        // 02 12 22 32 42 52 62 72 0A 1A 2A 3A 4A 5A 6A 7A
        // 82 92 A2 B2 C2 D2 E2 F2 8A 9A AA BA CA DA EA FA
        // 06 16 26 36 46 56 66 76 0E 1E 2E 3E 4E 5E 6E 7E
        // 86 96 A6 B6 C6 D6 E6 F6 8E 9E AE BE CE DE EE FE
        // 01 11 21 31 41 51 61 71 09 19 29 39 49 59 69 79
        // 81 91 A1 B1 C1 D1 E1 F1 89 99 A9 B9 C9 D9 E9 F9
        // 05 15 25 35 45 55 65 75 0D 1D 2D 3D 4D 5D 6D 7D
        // 85 95 A5 B5 C5 D5 E5 F5 8D 9D AD BD CD DD ED FD
        // 03 13 23 33 43 53 63 73 0B 1B 2B 3B 4B 5B 6B 7B
        // 83 93 A3 B3 C3 D3 E3 F3 8B 9B AB BB CB DB EB FB
        // 07 17 27 37 47 57 67 77 0F 1F 2F 3F 4F 5F 6F 7F
        // 87 97 A7 B7 C7 D7 E7 F7 8F 9F AF BF CF DF EF FF
        if (isFirstPassOf32x32) {
            __m256i kZero = _mm256_setzero_si256();
            __m256i kOne  = _mm256_set1_epi16(1);
            __m256i tr2_0_sign  = _mm256_cmpgt_epi16(tr2_0,  kZero);
            __m256i tr2_1_sign  = _mm256_cmpgt_epi16(tr2_1,  kZero);
            __m256i tr2_2_sign  = _mm256_cmpgt_epi16(tr2_2,  kZero);
            __m256i tr2_3_sign  = _mm256_cmpgt_epi16(tr2_3,  kZero);
            __m256i tr2_4_sign  = _mm256_cmpgt_epi16(tr2_4,  kZero);
            __m256i tr2_5_sign  = _mm256_cmpgt_epi16(tr2_5,  kZero);
            __m256i tr2_6_sign  = _mm256_cmpgt_epi16(tr2_6,  kZero);
            __m256i tr2_7_sign  = _mm256_cmpgt_epi16(tr2_7,  kZero);
            __m256i tr2_8_sign  = _mm256_cmpgt_epi16(tr2_8,  kZero);
            __m256i tr2_9_sign  = _mm256_cmpgt_epi16(tr2_9,  kZero);
            __m256i tr2_10_sign = _mm256_cmpgt_epi16(tr2_10, kZero);
            __m256i tr2_11_sign = _mm256_cmpgt_epi16(tr2_11, kZero);
            __m256i tr2_12_sign = _mm256_cmpgt_epi16(tr2_12, kZero);
            __m256i tr2_13_sign = _mm256_cmpgt_epi16(tr2_13, kZero);
            __m256i tr2_14_sign = _mm256_cmpgt_epi16(tr2_14, kZero);
            __m256i tr2_15_sign = _mm256_cmpgt_epi16(tr2_15, kZero);
            tr2_0  = _mm256_sub_epi16(tr2_0,  tr2_0_sign);
            tr2_1  = _mm256_sub_epi16(tr2_1,  tr2_1_sign);
            tr2_2  = _mm256_sub_epi16(tr2_2,  tr2_2_sign);
            tr2_3  = _mm256_sub_epi16(tr2_3,  tr2_3_sign);
            tr2_4  = _mm256_sub_epi16(tr2_4,  tr2_4_sign);
            tr2_5  = _mm256_sub_epi16(tr2_5,  tr2_5_sign);
            tr2_6  = _mm256_sub_epi16(tr2_6,  tr2_6_sign);
            tr2_7  = _mm256_sub_epi16(tr2_7,  tr2_7_sign);
            tr2_8  = _mm256_sub_epi16(tr2_8,  tr2_8_sign);
            tr2_9  = _mm256_sub_epi16(tr2_9,  tr2_9_sign);
            tr2_10 = _mm256_sub_epi16(tr2_10, tr2_10_sign);
            tr2_11 = _mm256_sub_epi16(tr2_11, tr2_11_sign);
            tr2_12 = _mm256_sub_epi16(tr2_12, tr2_12_sign);
            tr2_13 = _mm256_sub_epi16(tr2_13, tr2_13_sign);
            tr2_14 = _mm256_sub_epi16(tr2_14, tr2_14_sign);
            tr2_15 = _mm256_sub_epi16(tr2_15, tr2_15_sign);
            tr2_0  = _mm256_add_epi16(tr2_0,  kOne);
            tr2_1  = _mm256_add_epi16(tr2_1,  kOne);
            tr2_2  = _mm256_add_epi16(tr2_2,  kOne);
            tr2_3  = _mm256_add_epi16(tr2_3,  kOne);
            tr2_4  = _mm256_add_epi16(tr2_4,  kOne);
            tr2_5  = _mm256_add_epi16(tr2_5,  kOne);
            tr2_6  = _mm256_add_epi16(tr2_6,  kOne);
            tr2_7  = _mm256_add_epi16(tr2_7,  kOne);
            tr2_8  = _mm256_add_epi16(tr2_8,  kOne);
            tr2_9  = _mm256_add_epi16(tr2_9,  kOne);
            tr2_10 = _mm256_add_epi16(tr2_10, kOne);
            tr2_11 = _mm256_add_epi16(tr2_11, kOne);
            tr2_12 = _mm256_add_epi16(tr2_12, kOne);
            tr2_13 = _mm256_add_epi16(tr2_13, kOne);
            tr2_14 = _mm256_add_epi16(tr2_14, kOne);
            tr2_15 = _mm256_add_epi16(tr2_15, kOne);
            tr2_0  = _mm256_srai_epi16(tr2_0,  2);
            tr2_1  = _mm256_srai_epi16(tr2_1,  2);
            tr2_2  = _mm256_srai_epi16(tr2_2,  2);
            tr2_3  = _mm256_srai_epi16(tr2_3,  2);
            tr2_4  = _mm256_srai_epi16(tr2_4,  2);
            tr2_5  = _mm256_srai_epi16(tr2_5,  2);
            tr2_6  = _mm256_srai_epi16(tr2_6,  2);
            tr2_7  = _mm256_srai_epi16(tr2_7,  2);
            tr2_8  = _mm256_srai_epi16(tr2_8,  2);
            tr2_9  = _mm256_srai_epi16(tr2_9,  2);
            tr2_10 = _mm256_srai_epi16(tr2_10, 2);
            tr2_11 = _mm256_srai_epi16(tr2_11, 2);
            tr2_12 = _mm256_srai_epi16(tr2_12, 2);
            tr2_13 = _mm256_srai_epi16(tr2_13, 2);
            tr2_14 = _mm256_srai_epi16(tr2_14, 2);
            tr2_15 = _mm256_srai_epi16(tr2_15, 2);
        }
        storea_si256(out_ptr +  0 * pitch, _mm256_permute2x128_si256(tr2_0,  tr2_1,  PERM2x128(0,2)));
        storea_si256(out_ptr +  1 * pitch, _mm256_permute2x128_si256(tr2_8,  tr2_9,  PERM2x128(0,2)));
        storea_si256(out_ptr +  2 * pitch, _mm256_permute2x128_si256(tr2_4,  tr2_5,  PERM2x128(0,2)));
        storea_si256(out_ptr +  3 * pitch, _mm256_permute2x128_si256(tr2_12, tr2_13, PERM2x128(0,2)));
        storea_si256(out_ptr +  4 * pitch, _mm256_permute2x128_si256(tr2_2,  tr2_3,  PERM2x128(0,2)));
        storea_si256(out_ptr +  5 * pitch, _mm256_permute2x128_si256(tr2_10, tr2_11, PERM2x128(0,2)));
        storea_si256(out_ptr +  6 * pitch, _mm256_permute2x128_si256(tr2_6,  tr2_7,  PERM2x128(0,2)));
        storea_si256(out_ptr +  7 * pitch, _mm256_permute2x128_si256(tr2_14, tr2_15, PERM2x128(0,2)));
        storea_si256(out_ptr +  8 * pitch, _mm256_permute2x128_si256(tr2_0,  tr2_1,  PERM2x128(1,3)));
        storea_si256(out_ptr +  9 * pitch, _mm256_permute2x128_si256(tr2_8,  tr2_9,  PERM2x128(1,3)));
        storea_si256(out_ptr + 10 * pitch, _mm256_permute2x128_si256(tr2_4,  tr2_5,  PERM2x128(1,3)));
        storea_si256(out_ptr + 11 * pitch, _mm256_permute2x128_si256(tr2_12, tr2_13, PERM2x128(1,3)));
        storea_si256(out_ptr + 12 * pitch, _mm256_permute2x128_si256(tr2_2,  tr2_3,  PERM2x128(1,3)));
        storea_si256(out_ptr + 13 * pitch, _mm256_permute2x128_si256(tr2_10, tr2_11, PERM2x128(1,3)));
        storea_si256(out_ptr + 14 * pitch, _mm256_permute2x128_si256(tr2_6,  tr2_7,  PERM2x128(1,3)));
        storea_si256(out_ptr + 15 * pitch, _mm256_permute2x128_si256(tr2_14, tr2_15, PERM2x128(1,3)));
    }

    static inline void load_buffer_16x16(const short *input, int pitch, __m256i *in)
    {
        for (int i = 0; i < 16; i++)
            in[i] = _mm256_slli_epi16(loada_si256(input +  i * pitch), 2);
    }

    static inline void right_shift_16x16(__m256i *res)
    {
        const __m256i const_rounding = _mm256_set1_epi16(1);
        for (int i = 0; i < 16; i++) {
            const __m256i sign0 = _mm256_srai_epi16(res[i], 15);
            res[i] = _mm256_add_epi16(res[i], const_rounding);
            res[i] = _mm256_sub_epi16(res[i], sign0);
            res[i] = _mm256_srai_epi16(res[i], 2);
        }
    }

    static void write_buffer_16x16(short *dst, int pitch, __m256i *out) {
        for (int i = 0; i < 16; i++)
            storea_si256(dst + i * pitch, out[i]);
    }

    static void fdct8x8_avx2(const short *input, short *output, int stride)
    {
        // Constants
        //    When we use them, in one case, they are all the same. In all others
        //    it's a pair of them that we need to repeat four times. This is done
        //    by constructing the 32 bit constant corresponding to that pair.
        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m256i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m256i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m256i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m256i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);
        // Load input
        __m256i in01 = loada2_m128i(input + 0 * stride, input + 1 * stride);
        __m256i in32 = loada2_m128i(input + 3 * stride, input + 2 * stride);
        __m256i in45 = loada2_m128i(input + 4 * stride, input + 5 * stride);
        __m256i in76 = loada2_m128i(input + 7 * stride, input + 6 * stride);
        // Pre-condition input (shift by two)
        in01 = _mm256_slli_epi16(in01, 2);
        in32 = _mm256_slli_epi16(in32, 2);
        in45 = _mm256_slli_epi16(in45, 2);
        in76 = _mm256_slli_epi16(in76, 2);

        // We do two passes, first the columns, then the rows. The results of the
        // first pass are transposed so that the same column code can be reused. The
        // results of the second pass are also transposed so that the rows (processed
        // as columns) are put back in row positions.
        for (int pass = 0; pass < 2; pass++) {
            // To store results of each pass before the transpose.
            __m256i res04, res15, res26, res37;
            // Add/subtract
            const __m256i q01 = _mm256_add_epi16(in01, in76); // q0 q1
            const __m256i q32 = _mm256_add_epi16(in32, in45); // q2 q3
            const __m256i q45 = _mm256_sub_epi16(in32, in45); // q5 q4
            const __m256i q76 = _mm256_sub_epi16(in01, in76); // q7 q6
            // Work on first four results
            {
                // Add/subtract
                const __m256i r01 = _mm256_add_epi16(q01, q32);
                const __m256i r23 = _mm256_sub_epi16(q01, q32);
                // Interleave to do the multiply by constants which gets us into 32bits
                {
                    const __m256i tmp0 = _mm256_permute4x64_epi64(r01, PERM4x64(0,2,1,3));
                    const __m256i tmp1 = _mm256_bsrli_epi128(tmp0, 8);
                    const __m256i tmp2 = _mm256_permute4x64_epi64(r23, PERM4x64(2,0,3,1));
                    const __m256i tmp3 = _mm256_bsrli_epi128(tmp2, 8);
                    const __m256i t01 = _mm256_unpacklo_epi16(tmp0, tmp1);
                    const __m256i t23 = _mm256_unpacklo_epi16(tmp2, tmp3);
                    const __m256i u01 = _mm256_madd_epi16(t01, k__cospi_p16_p16);
                    const __m256i u23 = _mm256_madd_epi16(t01, k__cospi_p16_m16);
                    const __m256i u45 = _mm256_madd_epi16(t23, k__cospi_p24_p08);
                    const __m256i u67 = _mm256_madd_epi16(t23, k__cospi_m08_p24);
                    // dct_const_round_shift
                    const __m256i v01 = _mm256_add_epi32(u01, k__DCT_CONST_ROUNDING);
                    const __m256i v23 = _mm256_add_epi32(u23, k__DCT_CONST_ROUNDING);
                    const __m256i v45 = _mm256_add_epi32(u45, k__DCT_CONST_ROUNDING);
                    const __m256i v67 = _mm256_add_epi32(u67, k__DCT_CONST_ROUNDING);
                    const __m256i w01 = _mm256_srai_epi32(v01, DCT_CONST_BITS);
                    const __m256i w23 = _mm256_srai_epi32(v23, DCT_CONST_BITS);
                    const __m256i w45 = _mm256_srai_epi32(v45, DCT_CONST_BITS);
                    const __m256i w67 = _mm256_srai_epi32(v67, DCT_CONST_BITS);
                    // Combine
                    res04 = _mm256_packs_epi32(w01, w23);   // r00 ... r03 r40 ... r43 r04 ... r07 r44 ... r47
                    res26 = _mm256_packs_epi32(w45, w67);
                    res04 = _mm256_permute4x64_epi64(res04, PERM4x64(0,2,1,3));
                    res26 = _mm256_permute4x64_epi64(res26, PERM4x64(0,2,1,3));
                }
            }
            // Work on next four results
            {
                // Interleave to do the multiply by constants which gets us into 32bits
                const __m256i tmp0 = _mm256_permute4x64_epi64(q76, PERM4x64(2,2,3,3));
                const __m256i tmp1 = _mm256_permute4x64_epi64(q45, PERM4x64(2,2,3,3));
                const __m256i d0 = _mm256_unpacklo_epi16(tmp0, tmp1);
                const __m256i e01 = _mm256_madd_epi16(d0, k__cospi_p16_m16);
                const __m256i e23 = _mm256_madd_epi16(d0, k__cospi_p16_p16);
                // dct_const_round_shift
                const __m256i f01 = _mm256_add_epi32(e01, k__DCT_CONST_ROUNDING);
                const __m256i f23 = _mm256_add_epi32(e23, k__DCT_CONST_ROUNDING);
                const __m256i s01 = _mm256_srai_epi32(f01, DCT_CONST_BITS);
                const __m256i s23 = _mm256_srai_epi32(f23, DCT_CONST_BITS);
                // Combine
                __m256i r01 = _mm256_packs_epi32(s01, s23);
                r01 = _mm256_permute4x64_epi64(r01, PERM4x64(0,2,1,3));
                {
                    // Add/subtract
                    const __m256i q47 = _mm256_permute2x128_si256(q45, q76, PERM2x128(0,2));
                    const __m256i x03 = _mm256_add_epi16(q47, r01);
                    const __m256i x12 = _mm256_sub_epi16(q47, r01);
                    // Interleave to do the multiply by constants which gets us into 32bits
                    {
                        const __m256i tmp0 = _mm256_permute4x64_epi64(x03, PERM4x64(0,2,1,3));
                        const __m256i tmp1 = _mm256_bsrli_epi128(tmp0, 8);
                        const __m256i tmp2 = _mm256_permute4x64_epi64(x12, PERM4x64(0,2,1,3));
                        const __m256i tmp3 = _mm256_bsrli_epi128(tmp2, 8);
                        const __m256i t01 = _mm256_unpacklo_epi16(tmp0, tmp1);
                        const __m256i t23 = _mm256_unpacklo_epi16(tmp2, tmp3);

                        const __m256i u01 = _mm256_madd_epi16(t01, k__cospi_p28_p04);
                        const __m256i u23 = _mm256_madd_epi16(t01, k__cospi_m04_p28);
                        const __m256i u45 = _mm256_madd_epi16(t23, k__cospi_p12_p20);
                        const __m256i u67 = _mm256_madd_epi16(t23, k__cospi_m20_p12);
                        // dct_const_round_shift
                        const __m256i v01 = _mm256_add_epi32(u01, k__DCT_CONST_ROUNDING);
                        const __m256i v23 = _mm256_add_epi32(u23, k__DCT_CONST_ROUNDING);
                        const __m256i v45 = _mm256_add_epi32(u45, k__DCT_CONST_ROUNDING);
                        const __m256i v67 = _mm256_add_epi32(u67, k__DCT_CONST_ROUNDING);
                        const __m256i w01 = _mm256_srai_epi32(v01, DCT_CONST_BITS);
                        const __m256i w23 = _mm256_srai_epi32(v23, DCT_CONST_BITS);
                        const __m256i w45 = _mm256_srai_epi32(v45, DCT_CONST_BITS);
                        const __m256i w67 = _mm256_srai_epi32(v67, DCT_CONST_BITS);
                        // Combine
                        res15 = _mm256_packs_epi32(w01, w45);
                        res37 = _mm256_packs_epi32(w67, w23);
                        res15 = _mm256_permute4x64_epi64(res15, PERM4x64(0,2,1,3));
                        res37 = _mm256_permute4x64_epi64(res37, PERM4x64(0,2,1,3));
                    }
                }
            }
            // Transpose the 8x8.
            {
                // 00 01 02 03 04 05 06 07 40 41 42 43 44 45 46 47
                // 10 11 12 13 14 15 16 17 50 51 52 53 54 55 56 57
                // 20 21 22 23 24 25 26 27 60 61 62 63 64 65 66 67
                // 30 31 32 33 34 35 36 37 70 71 72 73 74 75 76 77
                const __m256i tr0_0 = _mm256_unpacklo_epi16(res04, res15);
                const __m256i tr0_1 = _mm256_unpacklo_epi16(res26, res37);
                const __m256i tr0_2 = _mm256_unpackhi_epi16(res04, res15);
                const __m256i tr0_3 = _mm256_unpackhi_epi16(res26, res37);
                // 00 10 01 11 02 12 03 13 40 50 41 51 42 52 43 53
                // 20 30 21 31 22 32 23 33 60 70 61 71 62 72 63 73
                // 04 14 05 15 06 16 07 17 44 54 45 55 46 56 47 57
                // 24 34 25 35 26 36 27 37 64 74 65 75 66 76 67 77
                const __m256i tr1_0 = _mm256_unpacklo_epi32(tr0_0, tr0_1);
                const __m256i tr1_1 = _mm256_unpacklo_epi32(tr0_2, tr0_3);
                const __m256i tr1_2 = _mm256_unpackhi_epi32(tr0_0, tr0_1);
                const __m256i tr1_3 = _mm256_unpackhi_epi32(tr0_2, tr0_3);
                // 00 10 20 30 01 11 21 31 40 50 60 70 41 51 61 71
                // 04 14 24 34 05 15 25 35 44 54 64 74 45 55 65 75
                // 02 12 22 32 03 13 23 33 42 52 62 72 43 53 63 73
                // 06 16 26 36 07 17 27 37 46 56 66 76 47 57 67 77
                in01 = _mm256_permute4x64_epi64(tr1_0, PERM4x64(0,2,1,3));
                in32 = _mm256_permute4x64_epi64(tr1_2, PERM4x64(1,3,0,2));
                in45 = _mm256_permute4x64_epi64(tr1_1, PERM4x64(0,2,1,3));
                in76 = _mm256_permute4x64_epi64(tr1_3, PERM4x64(1,3,0,2));
                // 00 10 20 30 40 50 60 70 01 11 21 31 41 51 61 71
                // 02 12 22 32 42 52 62 72 03 13 23 33 43 53 63 73
                // 04 14 24 34 44 54 64 74 05 15 25 35 45 55 65 75
                // 06 16 26 36 46 56 66 76 07 17 27 37 47 57 67 77
            }
        }
        // Post-condition output and store it
        {
            // Post-condition (division by two)
            //    division of two 16 bits signed numbers using shifts
            //    n / 2 = (n - (n >> 15)) >> 1
            const __m256i sign_in01 = _mm256_srai_epi16(in01, 15);
            const __m256i sign_in32 = _mm256_srai_epi16(in32, 15);
            const __m256i sign_in45 = _mm256_srai_epi16(in45, 15);
            const __m256i sign_in76 = _mm256_srai_epi16(in76, 15);
            in01 = _mm256_sub_epi16(in01, sign_in01);
            in32 = _mm256_sub_epi16(in32, sign_in32);
            in45 = _mm256_sub_epi16(in45, sign_in45);
            in76 = _mm256_sub_epi16(in76, sign_in76);
            in01 = _mm256_srai_epi16(in01, 1);
            in32 = _mm256_srai_epi16(in32, 1);
            in45 = _mm256_srai_epi16(in45, 1);
            in76 = _mm256_srai_epi16(in76, 1);
            // store results
            storea_si256(output + 0 * 8, in01);
            storea_si256(output + 2 * 8, _mm256_permute4x64_epi64(in32, PERM4x64(2,3,0,1)));
            storea_si256(output + 4 * 8, in45);
            storea_si256(output + 6 * 8, _mm256_permute4x64_epi64(in76, PERM4x64(2,3,0,1)));
        }
    }

    template <int tx_type> static void fht8x8_avx2(const short *input, short *output, int pitchInput)
    {
        //__m256i in[4];

        if (tx_type == DCT_DCT) {
            fdct8x8_avx2(input, output, pitchInput);
        //} else if (tx_type == ADST_DCT) {
        //    load_buffer_8x8(input, in, pitchInput);
        //    fadst8_avx2(in);
        //    fdct8_avx2(in);
        //    right_shift_8x8(in, 1);
        //    write_buffer_8x8(output, in, 8);
        //} else if (tx_type == DCT_ADST) {
        //    load_buffer_8x8(input, in, pitchInput);
        //    fdct8_avx2(in);
        //    fadst8_avx2(in);
        //    right_shift_8x8(in, 1);
        //    write_buffer_8x8(output, in, 8);
        //} else if (tx_type == ADST_ADST) {
        //    load_buffer_8x8(input, in, pitchInput);
        //    fadst8_avx2(in);
        //    fadst8_avx2(in);
        //    right_shift_8x8(in, 1);
        //    write_buffer_8x8(output, in, 8);
        } else {
            assert(0);
        }
    }

    static void fdct16x16_avx2(const short *input, short *output, int stride)
    {
        ALIGN_DECL(32) short intermediate[256];

        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m256i k__cospi_p08_m24 = pair_set_epi16(cospi_8_64, -cospi_24_64);
        const __m256i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m256i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m256i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m256i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m256i k__cospi_p30_p02 = pair_set_epi16(cospi_30_64, cospi_2_64);
        const __m256i k__cospi_p14_p18 = pair_set_epi16(cospi_14_64, cospi_18_64);
        const __m256i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
        const __m256i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
        const __m256i k__cospi_p22_p10 = pair_set_epi16(cospi_22_64, cospi_10_64);
        const __m256i k__cospi_p06_p26 = pair_set_epi16(cospi_6_64, cospi_26_64);
        const __m256i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
        const __m256i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);

        short *out = intermediate;
        for (int pass = 0; pass < 2; ++pass) {
            __m256i in00, in01, in02, in03, in04, in05, in06, in07;
            __m256i in08, in09, in10, in11, in12, in13, in14, in15;
            __m256i input0, input1, input2, input3, input4, input5, input6, input7;
            __m256i step1_0, step1_1, step1_2, step1_3;
            __m256i step1_4, step1_5, step1_6, step1_7;
            __m256i step2_1, step2_2, step2_3, step2_4, step2_5, step2_6;
            __m256i step3_0, step3_1, step3_2, step3_3;
            __m256i step3_4, step3_5, step3_6, step3_7;
            __m256i res[16];
            if (0 == pass) {
                in00 = loada_si256(input + 0 * stride);
                in01 = loada_si256(input + 1 * stride);
                in02 = loada_si256(input + 2 * stride);
                in03 = loada_si256(input + 3 * stride);
                in04 = loada_si256(input + 4 * stride);
                in05 = loada_si256(input + 5 * stride);
                in06 = loada_si256(input + 6 * stride);
                in07 = loada_si256(input + 7 * stride);
                in08 = loada_si256(input + 8 * stride);
                in09 = loada_si256(input + 9 * stride);
                in10 = loada_si256(input + 10 * stride);
                in11 = loada_si256(input + 11 * stride);
                in12 = loada_si256(input + 12 * stride);
                in13 = loada_si256(input + 13 * stride);
                in14 = loada_si256(input + 14 * stride);
                in15 = loada_si256(input + 15 * stride);
                // x = x << 2
                in00 = _mm256_slli_epi16(in00, 2);
                in01 = _mm256_slli_epi16(in01, 2);
                in02 = _mm256_slli_epi16(in02, 2);
                in03 = _mm256_slli_epi16(in03, 2);
                in04 = _mm256_slli_epi16(in04, 2);
                in05 = _mm256_slli_epi16(in05, 2);
                in06 = _mm256_slli_epi16(in06, 2);
                in07 = _mm256_slli_epi16(in07, 2);
                in08 = _mm256_slli_epi16(in08, 2);
                in09 = _mm256_slli_epi16(in09, 2);
                in10 = _mm256_slli_epi16(in10, 2);
                in11 = _mm256_slli_epi16(in11, 2);
                in12 = _mm256_slli_epi16(in12, 2);
                in13 = _mm256_slli_epi16(in13, 2);
                in14 = _mm256_slli_epi16(in14, 2);
                in15 = _mm256_slli_epi16(in15, 2);
            } else {
                in00 = loada_si256(intermediate + 0 * 16);
                in01 = loada_si256(intermediate + 1 * 16);
                in02 = loada_si256(intermediate + 2 * 16);
                in03 = loada_si256(intermediate + 3 * 16);
                in04 = loada_si256(intermediate + 4 * 16);
                in05 = loada_si256(intermediate + 5 * 16);
                in06 = loada_si256(intermediate + 6 * 16);
                in07 = loada_si256(intermediate + 7 * 16);
                in08 = loada_si256(intermediate + 8 * 16);
                in09 = loada_si256(intermediate + 9 * 16);
                in10 = loada_si256(intermediate + 10 * 16);
                in11 = loada_si256(intermediate + 11 * 16);
                in12 = loada_si256(intermediate + 12 * 16);
                in13 = loada_si256(intermediate + 13 * 16);
                in14 = loada_si256(intermediate + 14 * 16);
                in15 = loada_si256(intermediate + 15 * 16);
                // x = (x + 1) >> 2
                __m256i one = _mm256_set1_epi16(1);
                in00 = _mm256_add_epi16(in00, one);
                in01 = _mm256_add_epi16(in01, one);
                in02 = _mm256_add_epi16(in02, one);
                in03 = _mm256_add_epi16(in03, one);
                in04 = _mm256_add_epi16(in04, one);
                in05 = _mm256_add_epi16(in05, one);
                in06 = _mm256_add_epi16(in06, one);
                in07 = _mm256_add_epi16(in07, one);
                in08 = _mm256_add_epi16(in08, one);
                in09 = _mm256_add_epi16(in09, one);
                in10 = _mm256_add_epi16(in10, one);
                in11 = _mm256_add_epi16(in11, one);
                in12 = _mm256_add_epi16(in12, one);
                in13 = _mm256_add_epi16(in13, one);
                in14 = _mm256_add_epi16(in14, one);
                in15 = _mm256_add_epi16(in15, one);
                in00 = _mm256_srai_epi16(in00, 2);
                in01 = _mm256_srai_epi16(in01, 2);
                in02 = _mm256_srai_epi16(in02, 2);
                in03 = _mm256_srai_epi16(in03, 2);
                in04 = _mm256_srai_epi16(in04, 2);
                in05 = _mm256_srai_epi16(in05, 2);
                in06 = _mm256_srai_epi16(in06, 2);
                in07 = _mm256_srai_epi16(in07, 2);
                in08 = _mm256_srai_epi16(in08, 2);
                in09 = _mm256_srai_epi16(in09, 2);
                in10 = _mm256_srai_epi16(in10, 2);
                in11 = _mm256_srai_epi16(in11, 2);
                in12 = _mm256_srai_epi16(in12, 2);
                in13 = _mm256_srai_epi16(in13, 2);
                in14 = _mm256_srai_epi16(in14, 2);
                in15 = _mm256_srai_epi16(in15, 2);
            }
            // Calculate input for the first 8 results.
            {
                input0 = _mm256_add_epi16(in00, in15);
                input1 = _mm256_add_epi16(in01, in14);
                input2 = _mm256_add_epi16(in02, in13);
                input3 = _mm256_add_epi16(in03, in12);
                input4 = _mm256_add_epi16(in04, in11);
                input5 = _mm256_add_epi16(in05, in10);
                input6 = _mm256_add_epi16(in06, in09);
                input7 = _mm256_add_epi16(in07, in08);
            }
            // Calculate input for the next 8 results.
            {
                step1_0 = _mm256_sub_epi16(in07, in08);
                step1_1 = _mm256_sub_epi16(in06, in09);
                step1_2 = _mm256_sub_epi16(in05, in10);
                step1_3 = _mm256_sub_epi16(in04, in11);
                step1_4 = _mm256_sub_epi16(in03, in12);
                step1_5 = _mm256_sub_epi16(in02, in13);
                step1_6 = _mm256_sub_epi16(in01, in14);
                step1_7 = _mm256_sub_epi16(in00, in15);
            }
            // Work on the first eight values; fdct8(input, even_results);
            {
                // Add/subtract
                const __m256i q0 = _mm256_add_epi16(input0, input7);
                const __m256i q1 = _mm256_add_epi16(input1, input6);
                const __m256i q2 = _mm256_add_epi16(input2, input5);
                const __m256i q3 = _mm256_add_epi16(input3, input4);
                const __m256i q4 = _mm256_sub_epi16(input3, input4);
                const __m256i q5 = _mm256_sub_epi16(input2, input5);
                const __m256i q6 = _mm256_sub_epi16(input1, input6);
                const __m256i q7 = _mm256_sub_epi16(input0, input7);
                // Work on first four results
                {
                    // Add/subtract
                    const __m256i r0 = _mm256_add_epi16(q0, q3);
                    const __m256i r1 = _mm256_add_epi16(q1, q2);
                    const __m256i r2 = _mm256_sub_epi16(q1, q2);
                    const __m256i r3 = _mm256_sub_epi16(q0, q3);
                    // Interleave to do the multiply by constants which gets us
                    // into 32 bits.
                    {
                        const __m256i t0 = _mm256_unpacklo_epi16(r0, r1);
                        const __m256i t1 = _mm256_unpackhi_epi16(r0, r1);
                        const __m256i t2 = _mm256_unpacklo_epi16(r2, r3);
                        const __m256i t3 = _mm256_unpackhi_epi16(r2, r3);
                        res[ 0] = mult_round_shift(&t0, &t1, &k__cospi_p16_p16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res[ 8] = mult_round_shift(&t0, &t1, &k__cospi_p16_m16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res[ 4] = mult_round_shift(&t2, &t3, &k__cospi_p24_p08, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res[12] = mult_round_shift(&t2, &t3, &k__cospi_m08_p24, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    }
                }
                // Work on next four results
                {
                    // Interleave to do the multiply by constants which gets us
                    // into 32 bits.
                    const __m256i d0 = _mm256_unpacklo_epi16(q6, q5);
                    const __m256i d1 = _mm256_unpackhi_epi16(q6, q5);
                    const __m256i r0 = mult_round_shift(&d0, &d1, &k__cospi_p16_m16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    const __m256i r1 = mult_round_shift(&d0, &d1, &k__cospi_p16_p16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    {
                        // Add/subtract
                        const __m256i x0 = _mm256_add_epi16(q4, r0);
                        const __m256i x1 = _mm256_sub_epi16(q4, r0);
                        const __m256i x2 = _mm256_sub_epi16(q7, r1);
                        const __m256i x3 = _mm256_add_epi16(q7, r1);
                        // Interleave to do the multiply by constants which gets us
                        // into 32 bits.
                        {
                            const __m256i t0 = _mm256_unpacklo_epi16(x0, x3);
                            const __m256i t1 = _mm256_unpackhi_epi16(x0, x3);
                            const __m256i t2 = _mm256_unpacklo_epi16(x1, x2);
                            const __m256i t3 = _mm256_unpackhi_epi16(x1, x2);
                            res[ 2] = mult_round_shift(&t0, &t1, &k__cospi_p28_p04, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                            res[14] = mult_round_shift(&t0, &t1, &k__cospi_m04_p28, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                            res[10] = mult_round_shift(&t2, &t3, &k__cospi_p12_p20, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                            res[ 6] = mult_round_shift(&t2, &t3, &k__cospi_m20_p12, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        }
                    }
                }
            }
            // Work on the next eight values; step1 -> odd_results
            {
                // step 2
                {
                    const __m256i t0 = _mm256_unpacklo_epi16(step1_5, step1_2);
                    const __m256i t1 = _mm256_unpackhi_epi16(step1_5, step1_2);
                    const __m256i t2 = _mm256_unpacklo_epi16(step1_4, step1_3);
                    const __m256i t3 = _mm256_unpackhi_epi16(step1_4, step1_3);
                    step2_2 = mult_round_shift(&t0, &t1, &k__cospi_p16_m16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_3 = mult_round_shift(&t2, &t3, &k__cospi_p16_m16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_5 = mult_round_shift(&t0, &t1, &k__cospi_p16_p16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_4 = mult_round_shift(&t2, &t3, &k__cospi_p16_p16, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                }
                // step 3
                {
                    step3_0 = _mm256_add_epi16(step1_0, step2_3);
                    step3_1 = _mm256_add_epi16(step1_1, step2_2);
                    step3_2 = _mm256_sub_epi16(step1_1, step2_2);
                    step3_3 = _mm256_sub_epi16(step1_0, step2_3);
                    step3_4 = _mm256_sub_epi16(step1_7, step2_4);
                    step3_5 = _mm256_sub_epi16(step1_6, step2_5);
                    step3_6 = _mm256_add_epi16(step1_6, step2_5);
                    step3_7 = _mm256_add_epi16(step1_7, step2_4);
                }
                // step 4
                {
                    const __m256i t0 = _mm256_unpacklo_epi16(step3_1, step3_6);
                    const __m256i t1 = _mm256_unpackhi_epi16(step3_1, step3_6);
                    const __m256i t2 = _mm256_unpacklo_epi16(step3_2, step3_5);
                    const __m256i t3 = _mm256_unpackhi_epi16(step3_2, step3_5);
                    step2_1 = mult_round_shift(&t0, &t1, &k__cospi_m08_p24, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_2 = mult_round_shift(&t2, &t3, &k__cospi_p24_p08, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_6 = mult_round_shift(&t0, &t1, &k__cospi_p24_p08, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    step2_5 = mult_round_shift(&t2, &t3, &k__cospi_p08_m24, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                }
                // step 5
                {
                    step1_0 = _mm256_add_epi16(step3_0, step2_1);
                    step1_1 = _mm256_sub_epi16(step3_0, step2_1);
                    step1_2 = _mm256_add_epi16(step3_3, step2_2);
                    step1_3 = _mm256_sub_epi16(step3_3, step2_2);
                    step1_4 = _mm256_sub_epi16(step3_4, step2_5);
                    step1_5 = _mm256_add_epi16(step3_4, step2_5);
                    step1_6 = _mm256_sub_epi16(step3_7, step2_6);
                    step1_7 = _mm256_add_epi16(step3_7, step2_6);
                }
                // step 6
                {
                    const __m256i t0 = _mm256_unpacklo_epi16(step1_0, step1_7);
                    const __m256i t1 = _mm256_unpackhi_epi16(step1_0, step1_7);
                    const __m256i t2 = _mm256_unpacklo_epi16(step1_1, step1_6);
                    const __m256i t3 = _mm256_unpackhi_epi16(step1_1, step1_6);
                    res[ 1] = mult_round_shift(&t0, &t1, &k__cospi_p30_p02, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[ 9] = mult_round_shift(&t2, &t3, &k__cospi_p14_p18, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[15] = mult_round_shift(&t0, &t1, &k__cospi_m02_p30, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[ 7] = mult_round_shift(&t2, &t3, &k__cospi_m18_p14, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                }
                {
                    const __m256i t0 = _mm256_unpacklo_epi16(step1_2, step1_5);
                    const __m256i t1 = _mm256_unpackhi_epi16(step1_2, step1_5);
                    const __m256i t2 = _mm256_unpacklo_epi16(step1_3, step1_4);
                    const __m256i t3 = _mm256_unpackhi_epi16(step1_3, step1_4);
                    res[ 5] = mult_round_shift(&t0, &t1, &k__cospi_p22_p10, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[13] = mult_round_shift(&t2, &t3, &k__cospi_p06_p26, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[11] = mult_round_shift(&t0, &t1, &k__cospi_m10_p22, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                    res[ 3] = mult_round_shift(&t2, &t3, &k__cospi_m26_p06, &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                }
            }
            // Transpose the results, do it as two 8x8 transposes.
            transpose_and_output16x16<false>(res, out, 16);
            out = output;
        }
    }

    static void fadst16(__m256i *in) {
        // perform 16x16 1-D ADST for 8 columns
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
        const __m256i kZero = _mm256_set1_epi16(0);

        u[0] = _mm256_unpacklo_epi16(in[15], in[0]);
        u[1] = _mm256_unpackhi_epi16(in[15], in[0]);
        u[2] = _mm256_unpacklo_epi16(in[13], in[2]);
        u[3] = _mm256_unpackhi_epi16(in[13], in[2]);
        u[4] = _mm256_unpacklo_epi16(in[11], in[4]);
        u[5] = _mm256_unpackhi_epi16(in[11], in[4]);
        u[6] = _mm256_unpacklo_epi16(in[9], in[6]);
        u[7] = _mm256_unpackhi_epi16(in[9], in[6]);
        u[8] = _mm256_unpacklo_epi16(in[7], in[8]);
        u[9] = _mm256_unpackhi_epi16(in[7], in[8]);
        u[10] = _mm256_unpacklo_epi16(in[5], in[10]);
        u[11] = _mm256_unpackhi_epi16(in[5], in[10]);
        u[12] = _mm256_unpacklo_epi16(in[3], in[12]);
        u[13] = _mm256_unpackhi_epi16(in[3], in[12]);
        u[14] = _mm256_unpacklo_epi16(in[1], in[14]);
        u[15] = _mm256_unpackhi_epi16(in[1], in[14]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_p01_p31);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_p01_p31);
        v[2] = _mm256_madd_epi16(u[0], k__cospi_p31_m01);
        v[3] = _mm256_madd_epi16(u[1], k__cospi_p31_m01);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_p05_p27);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_p05_p27);
        v[6] = _mm256_madd_epi16(u[2], k__cospi_p27_m05);
        v[7] = _mm256_madd_epi16(u[3], k__cospi_p27_m05);
        v[8] = _mm256_madd_epi16(u[4], k__cospi_p09_p23);
        v[9] = _mm256_madd_epi16(u[5], k__cospi_p09_p23);
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

        u[0] = _mm256_add_epi32(v[0], v[16]);
        u[1] = _mm256_add_epi32(v[1], v[17]);
        u[2] = _mm256_add_epi32(v[2], v[18]);
        u[3] = _mm256_add_epi32(v[3], v[19]);
        u[4] = _mm256_add_epi32(v[4], v[20]);
        u[5] = _mm256_add_epi32(v[5], v[21]);
        u[6] = _mm256_add_epi32(v[6], v[22]);
        u[7] = _mm256_add_epi32(v[7], v[23]);
        u[8] = _mm256_add_epi32(v[8], v[24]);
        u[9] = _mm256_add_epi32(v[9], v[25]);
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

        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        v[8] = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        v[9] = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
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

        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
        u[8] = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
        u[9] = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
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

        s[0] = _mm256_packs_epi32(u[0], u[1]);
        s[1] = _mm256_packs_epi32(u[2], u[3]);
        s[2] = _mm256_packs_epi32(u[4], u[5]);
        s[3] = _mm256_packs_epi32(u[6], u[7]);
        s[4] = _mm256_packs_epi32(u[8], u[9]);
        s[5] = _mm256_packs_epi32(u[10], u[11]);
        s[6] = _mm256_packs_epi32(u[12], u[13]);
        s[7] = _mm256_packs_epi32(u[14], u[15]);
        s[8] = _mm256_packs_epi32(u[16], u[17]);
        s[9] = _mm256_packs_epi32(u[18], u[19]);
        s[10] = _mm256_packs_epi32(u[20], u[21]);
        s[11] = _mm256_packs_epi32(u[22], u[23]);
        s[12] = _mm256_packs_epi32(u[24], u[25]);
        s[13] = _mm256_packs_epi32(u[26], u[27]);
        s[14] = _mm256_packs_epi32(u[28], u[29]);
        s[15] = _mm256_packs_epi32(u[30], u[31]);

        // stage 2
        u[0] = _mm256_unpacklo_epi16(s[8], s[9]);
        u[1] = _mm256_unpackhi_epi16(s[8], s[9]);
        u[2] = _mm256_unpacklo_epi16(s[10], s[11]);
        u[3] = _mm256_unpackhi_epi16(s[10], s[11]);
        u[4] = _mm256_unpacklo_epi16(s[12], s[13]);
        u[5] = _mm256_unpackhi_epi16(s[12], s[13]);
        u[6] = _mm256_unpacklo_epi16(s[14], s[15]);
        u[7] = _mm256_unpackhi_epi16(s[14], s[15]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_p04_p28);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_p04_p28);
        v[2] = _mm256_madd_epi16(u[0], k__cospi_p28_m04);
        v[3] = _mm256_madd_epi16(u[1], k__cospi_p28_m04);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_p20_p12);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_p20_p12);
        v[6] = _mm256_madd_epi16(u[2], k__cospi_p12_m20);
        v[7] = _mm256_madd_epi16(u[3], k__cospi_p12_m20);
        v[8] = _mm256_madd_epi16(u[4], k__cospi_m28_p04);
        v[9] = _mm256_madd_epi16(u[5], k__cospi_m28_p04);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_p04_p28);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_p04_p28);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_m12_p20);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_m12_p20);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p20_p12);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p20_p12);

        u[0] = _mm256_add_epi32(v[0], v[8]);
        u[1] = _mm256_add_epi32(v[1], v[9]);
        u[2] = _mm256_add_epi32(v[2], v[10]);
        u[3] = _mm256_add_epi32(v[3], v[11]);
        u[4] = _mm256_add_epi32(v[4], v[12]);
        u[5] = _mm256_add_epi32(v[5], v[13]);
        u[6] = _mm256_add_epi32(v[6], v[14]);
        u[7] = _mm256_add_epi32(v[7], v[15]);
        u[8] = _mm256_sub_epi32(v[0], v[8]);
        u[9] = _mm256_sub_epi32(v[1], v[9]);
        u[10] = _mm256_sub_epi32(v[2], v[10]);
        u[11] = _mm256_sub_epi32(v[3], v[11]);
        u[12] = _mm256_sub_epi32(v[4], v[12]);
        u[13] = _mm256_sub_epi32(v[5], v[13]);
        u[14] = _mm256_sub_epi32(v[6], v[14]);
        u[15] = _mm256_sub_epi32(v[7], v[15]);

        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        v[8] = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        v[9] = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
        u[8] = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
        u[9] = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
        u[10] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
        u[11] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
        u[12] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
        u[13] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
        u[14] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
        u[15] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);

        x[0] = _mm256_add_epi16(s[0], s[4]);
        x[1] = _mm256_add_epi16(s[1], s[5]);
        x[2] = _mm256_add_epi16(s[2], s[6]);
        x[3] = _mm256_add_epi16(s[3], s[7]);
        x[4] = _mm256_sub_epi16(s[0], s[4]);
        x[5] = _mm256_sub_epi16(s[1], s[5]);
        x[6] = _mm256_sub_epi16(s[2], s[6]);
        x[7] = _mm256_sub_epi16(s[3], s[7]);
        x[8] = _mm256_packs_epi32(u[0], u[1]);
        x[9] = _mm256_packs_epi32(u[2], u[3]);
        x[10] = _mm256_packs_epi32(u[4], u[5]);
        x[11] = _mm256_packs_epi32(u[6], u[7]);
        x[12] = _mm256_packs_epi32(u[8], u[9]);
        x[13] = _mm256_packs_epi32(u[10], u[11]);
        x[14] = _mm256_packs_epi32(u[12], u[13]);
        x[15] = _mm256_packs_epi32(u[14], u[15]);

        // stage 3
        u[0] = _mm256_unpacklo_epi16(x[4], x[5]);
        u[1] = _mm256_unpackhi_epi16(x[4], x[5]);
        u[2] = _mm256_unpacklo_epi16(x[6], x[7]);
        u[3] = _mm256_unpackhi_epi16(x[6], x[7]);
        u[4] = _mm256_unpacklo_epi16(x[12], x[13]);
        u[5] = _mm256_unpackhi_epi16(x[12], x[13]);
        u[6] = _mm256_unpacklo_epi16(x[14], x[15]);
        u[7] = _mm256_unpackhi_epi16(x[14], x[15]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_p08_p24);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_p08_p24);
        v[2] = _mm256_madd_epi16(u[0], k__cospi_p24_m08);
        v[3] = _mm256_madd_epi16(u[1], k__cospi_p24_m08);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_m24_p08);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_m24_p08);
        v[6] = _mm256_madd_epi16(u[2], k__cospi_p08_p24);
        v[7] = _mm256_madd_epi16(u[3], k__cospi_p08_p24);
        v[8] = _mm256_madd_epi16(u[4], k__cospi_p08_p24);
        v[9] = _mm256_madd_epi16(u[5], k__cospi_p08_p24);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_p24_m08);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_p24_m08);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_m24_p08);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_m24_p08);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p08_p24);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p08_p24);

        u[0] = _mm256_add_epi32(v[0], v[4]);
        u[1] = _mm256_add_epi32(v[1], v[5]);
        u[2] = _mm256_add_epi32(v[2], v[6]);
        u[3] = _mm256_add_epi32(v[3], v[7]);
        u[4] = _mm256_sub_epi32(v[0], v[4]);
        u[5] = _mm256_sub_epi32(v[1], v[5]);
        u[6] = _mm256_sub_epi32(v[2], v[6]);
        u[7] = _mm256_sub_epi32(v[3], v[7]);
        u[8] = _mm256_add_epi32(v[8], v[12]);
        u[9] = _mm256_add_epi32(v[9], v[13]);
        u[10] = _mm256_add_epi32(v[10], v[14]);
        u[11] = _mm256_add_epi32(v[11], v[15]);
        u[12] = _mm256_sub_epi32(v[8], v[12]);
        u[13] = _mm256_sub_epi32(v[9], v[13]);
        u[14] = _mm256_sub_epi32(v[10], v[14]);
        u[15] = _mm256_sub_epi32(v[11], v[15]);

        u[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        u[8] = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        u[9] = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        v[8] = _mm256_srai_epi32(u[8], DCT_CONST_BITS);
        v[9] = _mm256_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm256_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm256_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm256_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm256_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm256_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm256_srai_epi32(u[15], DCT_CONST_BITS);

        s[0] = _mm256_add_epi16(x[0], x[2]);
        s[1] = _mm256_add_epi16(x[1], x[3]);
        s[2] = _mm256_sub_epi16(x[0], x[2]);
        s[3] = _mm256_sub_epi16(x[1], x[3]);
        s[4] = _mm256_packs_epi32(v[0], v[1]);
        s[5] = _mm256_packs_epi32(v[2], v[3]);
        s[6] = _mm256_packs_epi32(v[4], v[5]);
        s[7] = _mm256_packs_epi32(v[6], v[7]);
        s[8] = _mm256_add_epi16(x[8], x[10]);
        s[9] = _mm256_add_epi16(x[9], x[11]);
        s[10] = _mm256_sub_epi16(x[8], x[10]);
        s[11] = _mm256_sub_epi16(x[9], x[11]);
        s[12] = _mm256_packs_epi32(v[8], v[9]);
        s[13] = _mm256_packs_epi32(v[10], v[11]);
        s[14] = _mm256_packs_epi32(v[12], v[13]);
        s[15] = _mm256_packs_epi32(v[14], v[15]);

        // stage 4
        u[0] = _mm256_unpacklo_epi16(s[2], s[3]);
        u[1] = _mm256_unpackhi_epi16(s[2], s[3]);
        u[2] = _mm256_unpacklo_epi16(s[6], s[7]);
        u[3] = _mm256_unpackhi_epi16(s[6], s[7]);
        u[4] = _mm256_unpacklo_epi16(s[10], s[11]);
        u[5] = _mm256_unpackhi_epi16(s[10], s[11]);
        u[6] = _mm256_unpacklo_epi16(s[14], s[15]);
        u[7] = _mm256_unpackhi_epi16(s[14], s[15]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_m16_m16);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_m16_m16);
        v[2] = _mm256_madd_epi16(u[0], k__cospi_p16_m16);
        v[3] = _mm256_madd_epi16(u[1], k__cospi_p16_m16);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_p16_p16);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_p16_p16);
        v[6] = _mm256_madd_epi16(u[2], k__cospi_m16_p16);
        v[7] = _mm256_madd_epi16(u[3], k__cospi_m16_p16);
        v[8] = _mm256_madd_epi16(u[4], k__cospi_p16_p16);
        v[9] = _mm256_madd_epi16(u[5], k__cospi_p16_p16);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_m16_p16);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_m16_p16);
        v[12] = _mm256_madd_epi16(u[6], k__cospi_m16_m16);
        v[13] = _mm256_madd_epi16(u[7], k__cospi_m16_m16);
        v[14] = _mm256_madd_epi16(u[6], k__cospi_p16_m16);
        v[15] = _mm256_madd_epi16(u[7], k__cospi_p16_m16);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[8] = _mm256_add_epi32(v[8], k__DCT_CONST_ROUNDING);
        u[9] = _mm256_add_epi32(v[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm256_add_epi32(v[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm256_add_epi32(v[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm256_add_epi32(v[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm256_add_epi32(v[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm256_add_epi32(v[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm256_add_epi32(v[15], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        v[8] = _mm256_srai_epi32(u[8], DCT_CONST_BITS);
        v[9] = _mm256_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm256_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm256_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm256_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm256_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm256_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm256_srai_epi32(u[15], DCT_CONST_BITS);

        in[0] = s[0];
        in[1] = _mm256_sub_epi16(kZero, s[8]);
        in[2] = s[12];
        in[3] = _mm256_sub_epi16(kZero, s[4]);
        in[4] = _mm256_packs_epi32(v[4], v[5]);
        in[5] = _mm256_packs_epi32(v[12], v[13]);
        in[6] = _mm256_packs_epi32(v[8], v[9]);
        in[7] = _mm256_packs_epi32(v[0], v[1]);
        in[8] = _mm256_packs_epi32(v[2], v[3]);
        in[9] = _mm256_packs_epi32(v[10], v[11]);
        in[10] = _mm256_packs_epi32(v[14], v[15]);
        in[11] = _mm256_packs_epi32(v[6], v[7]);
        in[12] = s[5];
        in[13] = _mm256_sub_epi16(kZero, s[13]);
        in[14] = s[9];
        in[15] = _mm256_sub_epi16(kZero, s[1]);
    }

    static void fdct16(__m256i *in) {
        // perform 16x16 1-D DCT for 8 columns
        __m256i i[8], s[8], p[8], t[8], u[16], v[16];
        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
        const __m256i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m256i k__cospi_p08_m24 = pair_set_epi16(cospi_8_64, -cospi_24_64);
        const __m256i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m256i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m256i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m256i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m256i k__cospi_p30_p02 = pair_set_epi16(cospi_30_64, cospi_2_64);
        const __m256i k__cospi_p14_p18 = pair_set_epi16(cospi_14_64, cospi_18_64);
        const __m256i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
        const __m256i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
        const __m256i k__cospi_p22_p10 = pair_set_epi16(cospi_22_64, cospi_10_64);
        const __m256i k__cospi_p06_p26 = pair_set_epi16(cospi_6_64, cospi_26_64);
        const __m256i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
        const __m256i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);

        // stage 1
        i[0] = _mm256_add_epi16(in[0], in[15]);
        i[1] = _mm256_add_epi16(in[1], in[14]);
        i[2] = _mm256_add_epi16(in[2], in[13]);
        i[3] = _mm256_add_epi16(in[3], in[12]);
        i[4] = _mm256_add_epi16(in[4], in[11]);
        i[5] = _mm256_add_epi16(in[5], in[10]);
        i[6] = _mm256_add_epi16(in[6], in[9]);
        i[7] = _mm256_add_epi16(in[7], in[8]);

        s[0] = _mm256_sub_epi16(in[7], in[8]);
        s[1] = _mm256_sub_epi16(in[6], in[9]);
        s[2] = _mm256_sub_epi16(in[5], in[10]);
        s[3] = _mm256_sub_epi16(in[4], in[11]);
        s[4] = _mm256_sub_epi16(in[3], in[12]);
        s[5] = _mm256_sub_epi16(in[2], in[13]);
        s[6] = _mm256_sub_epi16(in[1], in[14]);
        s[7] = _mm256_sub_epi16(in[0], in[15]);

        p[0] = _mm256_add_epi16(i[0], i[7]);
        p[1] = _mm256_add_epi16(i[1], i[6]);
        p[2] = _mm256_add_epi16(i[2], i[5]);
        p[3] = _mm256_add_epi16(i[3], i[4]);
        p[4] = _mm256_sub_epi16(i[3], i[4]);
        p[5] = _mm256_sub_epi16(i[2], i[5]);
        p[6] = _mm256_sub_epi16(i[1], i[6]);
        p[7] = _mm256_sub_epi16(i[0], i[7]);

        u[0] = _mm256_add_epi16(p[0], p[3]);
        u[1] = _mm256_add_epi16(p[1], p[2]);
        u[2] = _mm256_sub_epi16(p[1], p[2]);
        u[3] = _mm256_sub_epi16(p[0], p[3]);

        v[0] = _mm256_unpacklo_epi16(u[0], u[1]);
        v[1] = _mm256_unpackhi_epi16(u[0], u[1]);
        v[2] = _mm256_unpacklo_epi16(u[2], u[3]);
        v[3] = _mm256_unpackhi_epi16(u[2], u[3]);

        u[0] = _mm256_madd_epi16(v[0], k__cospi_p16_p16);
        u[1] = _mm256_madd_epi16(v[1], k__cospi_p16_p16);
        u[2] = _mm256_madd_epi16(v[0], k__cospi_p16_m16);
        u[3] = _mm256_madd_epi16(v[1], k__cospi_p16_m16);
        u[4] = _mm256_madd_epi16(v[2], k__cospi_p24_p08);
        u[5] = _mm256_madd_epi16(v[3], k__cospi_p24_p08);
        u[6] = _mm256_madd_epi16(v[2], k__cospi_m08_p24);
        u[7] = _mm256_madd_epi16(v[3], k__cospi_m08_p24);

        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);

        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);

        in[0] = _mm256_packs_epi32(u[0], u[1]);
        in[4] = _mm256_packs_epi32(u[4], u[5]);
        in[8] = _mm256_packs_epi32(u[2], u[3]);
        in[12] = _mm256_packs_epi32(u[6], u[7]);

        u[0] = _mm256_unpacklo_epi16(p[5], p[6]);
        u[1] = _mm256_unpackhi_epi16(p[5], p[6]);
        v[0] = _mm256_madd_epi16(u[0], k__cospi_m16_p16);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_m16_p16);
        v[2] = _mm256_madd_epi16(u[0], k__cospi_p16_p16);
        v[3] = _mm256_madd_epi16(u[1], k__cospi_p16_p16);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);

        u[0] = _mm256_packs_epi32(v[0], v[1]);
        u[1] = _mm256_packs_epi32(v[2], v[3]);

        t[0] = _mm256_add_epi16(p[4], u[0]);
        t[1] = _mm256_sub_epi16(p[4], u[0]);
        t[2] = _mm256_sub_epi16(p[7], u[1]);
        t[3] = _mm256_add_epi16(p[7], u[1]);

        u[0] = _mm256_unpacklo_epi16(t[0], t[3]);
        u[1] = _mm256_unpackhi_epi16(t[0], t[3]);
        u[2] = _mm256_unpacklo_epi16(t[1], t[2]);
        u[3] = _mm256_unpackhi_epi16(t[1], t[2]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_p28_p04);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_p28_p04);
        v[2] = _mm256_madd_epi16(u[2], k__cospi_p12_p20);
        v[3] = _mm256_madd_epi16(u[3], k__cospi_p12_p20);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_m20_p12);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_m20_p12);
        v[6] = _mm256_madd_epi16(u[0], k__cospi_m04_p28);
        v[7] = _mm256_madd_epi16(u[1], k__cospi_m04_p28);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);

        in[2] = _mm256_packs_epi32(v[0], v[1]);
        in[6] = _mm256_packs_epi32(v[4], v[5]);
        in[10] = _mm256_packs_epi32(v[2], v[3]);
        in[14] = _mm256_packs_epi32(v[6], v[7]);

        // stage 2
        u[0] = _mm256_unpacklo_epi16(s[2], s[5]);
        u[1] = _mm256_unpackhi_epi16(s[2], s[5]);
        u[2] = _mm256_unpacklo_epi16(s[3], s[4]);
        u[3] = _mm256_unpackhi_epi16(s[3], s[4]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_m16_p16);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_m16_p16);
        v[2] = _mm256_madd_epi16(u[2], k__cospi_m16_p16);
        v[3] = _mm256_madd_epi16(u[3], k__cospi_m16_p16);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_p16_p16);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_p16_p16);
        v[6] = _mm256_madd_epi16(u[0], k__cospi_p16_p16);
        v[7] = _mm256_madd_epi16(u[1], k__cospi_p16_p16);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);

        t[2] = _mm256_packs_epi32(v[0], v[1]);
        t[3] = _mm256_packs_epi32(v[2], v[3]);
        t[4] = _mm256_packs_epi32(v[4], v[5]);
        t[5] = _mm256_packs_epi32(v[6], v[7]);

        // stage 3
        p[0] = _mm256_add_epi16(s[0], t[3]);
        p[1] = _mm256_add_epi16(s[1], t[2]);
        p[2] = _mm256_sub_epi16(s[1], t[2]);
        p[3] = _mm256_sub_epi16(s[0], t[3]);
        p[4] = _mm256_sub_epi16(s[7], t[4]);
        p[5] = _mm256_sub_epi16(s[6], t[5]);
        p[6] = _mm256_add_epi16(s[6], t[5]);
        p[7] = _mm256_add_epi16(s[7], t[4]);

        // stage 4
        u[0] = _mm256_unpacklo_epi16(p[1], p[6]);
        u[1] = _mm256_unpackhi_epi16(p[1], p[6]);
        u[2] = _mm256_unpacklo_epi16(p[2], p[5]);
        u[3] = _mm256_unpackhi_epi16(p[2], p[5]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_m08_p24);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_m08_p24);
        v[2] = _mm256_madd_epi16(u[2], k__cospi_p24_p08);
        v[3] = _mm256_madd_epi16(u[3], k__cospi_p24_p08);
        v[4] = _mm256_madd_epi16(u[2], k__cospi_p08_m24);
        v[5] = _mm256_madd_epi16(u[3], k__cospi_p08_m24);
        v[6] = _mm256_madd_epi16(u[0], k__cospi_p24_p08);
        v[7] = _mm256_madd_epi16(u[1], k__cospi_p24_p08);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);

        t[1] = _mm256_packs_epi32(v[0], v[1]);
        t[2] = _mm256_packs_epi32(v[2], v[3]);
        t[5] = _mm256_packs_epi32(v[4], v[5]);
        t[6] = _mm256_packs_epi32(v[6], v[7]);

        // stage 5
        s[0] = _mm256_add_epi16(p[0], t[1]);
        s[1] = _mm256_sub_epi16(p[0], t[1]);
        s[2] = _mm256_add_epi16(p[3], t[2]);
        s[3] = _mm256_sub_epi16(p[3], t[2]);
        s[4] = _mm256_sub_epi16(p[4], t[5]);
        s[5] = _mm256_add_epi16(p[4], t[5]);
        s[6] = _mm256_sub_epi16(p[7], t[6]);
        s[7] = _mm256_add_epi16(p[7], t[6]);

        // stage 6
        u[0] = _mm256_unpacklo_epi16(s[0], s[7]);
        u[1] = _mm256_unpackhi_epi16(s[0], s[7]);
        u[2] = _mm256_unpacklo_epi16(s[1], s[6]);
        u[3] = _mm256_unpackhi_epi16(s[1], s[6]);
        u[4] = _mm256_unpacklo_epi16(s[2], s[5]);
        u[5] = _mm256_unpackhi_epi16(s[2], s[5]);
        u[6] = _mm256_unpacklo_epi16(s[3], s[4]);
        u[7] = _mm256_unpackhi_epi16(s[3], s[4]);

        v[0] = _mm256_madd_epi16(u[0], k__cospi_p30_p02);
        v[1] = _mm256_madd_epi16(u[1], k__cospi_p30_p02);
        v[2] = _mm256_madd_epi16(u[2], k__cospi_p14_p18);
        v[3] = _mm256_madd_epi16(u[3], k__cospi_p14_p18);
        v[4] = _mm256_madd_epi16(u[4], k__cospi_p22_p10);
        v[5] = _mm256_madd_epi16(u[5], k__cospi_p22_p10);
        v[6] = _mm256_madd_epi16(u[6], k__cospi_p06_p26);
        v[7] = _mm256_madd_epi16(u[7], k__cospi_p06_p26);
        v[8] = _mm256_madd_epi16(u[6], k__cospi_m26_p06);
        v[9] = _mm256_madd_epi16(u[7], k__cospi_m26_p06);
        v[10] = _mm256_madd_epi16(u[4], k__cospi_m10_p22);
        v[11] = _mm256_madd_epi16(u[5], k__cospi_m10_p22);
        v[12] = _mm256_madd_epi16(u[2], k__cospi_m18_p14);
        v[13] = _mm256_madd_epi16(u[3], k__cospi_m18_p14);
        v[14] = _mm256_madd_epi16(u[0], k__cospi_m02_p30);
        v[15] = _mm256_madd_epi16(u[1], k__cospi_m02_p30);

        u[0] = _mm256_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm256_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm256_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm256_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm256_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm256_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm256_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm256_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[8] = _mm256_add_epi32(v[8], k__DCT_CONST_ROUNDING);
        u[9] = _mm256_add_epi32(v[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm256_add_epi32(v[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm256_add_epi32(v[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm256_add_epi32(v[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm256_add_epi32(v[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm256_add_epi32(v[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm256_add_epi32(v[15], k__DCT_CONST_ROUNDING);

        v[0] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
        v[8] = _mm256_srai_epi32(u[8], DCT_CONST_BITS);
        v[9] = _mm256_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm256_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm256_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm256_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm256_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm256_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm256_srai_epi32(u[15], DCT_CONST_BITS);

        in[1] = _mm256_packs_epi32(v[0], v[1]);
        in[9] = _mm256_packs_epi32(v[2], v[3]);
        in[5] = _mm256_packs_epi32(v[4], v[5]);
        in[13] = _mm256_packs_epi32(v[6], v[7]);
        in[3] = _mm256_packs_epi32(v[8], v[9]);
        in[11] = _mm256_packs_epi32(v[10], v[11]);
        in[7] = _mm256_packs_epi32(v[12], v[13]);
        in[15] = _mm256_packs_epi32(v[14], v[15]);
    }

    template <int tx_type> static void fht16x16_avx2(const short *input, short *output, int stride)
    {
        __m256i in[16];

        if (tx_type == DCT_DCT) {
            fdct16x16_avx2(input, output, stride);
        } else if (tx_type == ADST_DCT) {
            load_buffer_16x16(input, stride, in);
            fadst16(in);
            array_transpose_16x16(in, in);
            right_shift_16x16(in);
            fdct16(in);
            array_transpose_16x16(in, in);
            write_buffer_16x16(output, 16, in);
        } else if (tx_type == DCT_ADST) {
            load_buffer_16x16(input, stride, in);
            fdct16(in);
            array_transpose_16x16(in, in);
            right_shift_16x16(in);
            fadst16(in);
            array_transpose_16x16(in, in);
            write_buffer_16x16(output, 16, in);
        } else if (tx_type == ADST_ADST) {
            load_buffer_16x16(input, stride, in);
            fadst16(in);
            array_transpose_16x16(in, in);
            right_shift_16x16(in);
            fadst16(in);
            array_transpose_16x16(in, in);
            write_buffer_16x16(output, 16, in);
        } else {
            assert(0);
        }
    }

    static void fdct32x32_avx2(const short *input, short *output, int stride)
    {
        const int str1 = stride;
        const int str2 = 2 * stride;
        const int str3 = 2 * stride + str1;

        ALIGN_DECL(32) short intermediate[32 * 32];

        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(+cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m256i k__cospi_m24_m08 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
        const __m256i k__cospi_p24_p08 = pair_set_epi16(+cospi_24_64, cospi_8_64);
        const __m256i k__cospi_p12_p20 = pair_set_epi16(+cospi_12_64, cospi_20_64);
        const __m256i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m256i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p28_p04 = pair_set_epi16(+cospi_28_64, cospi_4_64);
        const __m256i k__cospi_m28_m04 = pair_set_epi16(-cospi_28_64, -cospi_4_64);
        const __m256i k__cospi_m12_m20 = pair_set_epi16(-cospi_12_64, -cospi_20_64);
        const __m256i k__cospi_p30_p02 = pair_set_epi16(+cospi_30_64, cospi_2_64);
        const __m256i k__cospi_p14_p18 = pair_set_epi16(+cospi_14_64, cospi_18_64);
        const __m256i k__cospi_p22_p10 = pair_set_epi16(+cospi_22_64, cospi_10_64);
        const __m256i k__cospi_p06_p26 = pair_set_epi16(+cospi_6_64, cospi_26_64);
        const __m256i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
        const __m256i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
        const __m256i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
        const __m256i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
        const __m256i k__cospi_p31_p01 = pair_set_epi16(+cospi_31_64, cospi_1_64);
        const __m256i k__cospi_p15_p17 = pair_set_epi16(+cospi_15_64, cospi_17_64);
        const __m256i k__cospi_p23_p09 = pair_set_epi16(+cospi_23_64, cospi_9_64);
        const __m256i k__cospi_p07_p25 = pair_set_epi16(+cospi_7_64, cospi_25_64);
        const __m256i k__cospi_m25_p07 = pair_set_epi16(-cospi_25_64, cospi_7_64);
        const __m256i k__cospi_m09_p23 = pair_set_epi16(-cospi_9_64, cospi_23_64);
        const __m256i k__cospi_m17_p15 = pair_set_epi16(-cospi_17_64, cospi_15_64);
        const __m256i k__cospi_m01_p31 = pair_set_epi16(-cospi_1_64, cospi_31_64);
        const __m256i k__cospi_p27_p05 = pair_set_epi16(+cospi_27_64, cospi_5_64);
        const __m256i k__cospi_p11_p21 = pair_set_epi16(+cospi_11_64, cospi_21_64);
        const __m256i k__cospi_p19_p13 = pair_set_epi16(+cospi_19_64, cospi_13_64);
        const __m256i k__cospi_p03_p29 = pair_set_epi16(+cospi_3_64, cospi_29_64);
        const __m256i k__cospi_m29_p03 = pair_set_epi16(-cospi_29_64, cospi_3_64);
        const __m256i k__cospi_m13_p19 = pair_set_epi16(-cospi_13_64, cospi_19_64);
        const __m256i k__cospi_m21_p11 = pair_set_epi16(-cospi_21_64, cospi_11_64);
        const __m256i k__cospi_m05_p27 = pair_set_epi16(-cospi_5_64, cospi_27_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);
        const __m256i kZero = _mm256_set1_epi16(0);
        const __m256i kOne = _mm256_set1_epi16(1);
        // Do the two transform/transpose passes
        int pass;
        short *poutput = intermediate;
        for (pass = 0; pass < 2; ++pass) {
            // We process eight columns (transposed rows in second pass) at a time.
            int column_start;
            for (column_start = 0; column_start < 32; column_start += 16) {
                __m256i step1[32];
                __m256i step2[32];
                __m256i step3[32];
                __m256i out[32];
                // Stage 1
                // Note: even though all the loads below are aligned, using the aligned
                //       intrinsic make the code slightly slower.
                if (0 == pass) {
                    const short *in = &input[column_start];
                    // step1[i] =  (in[ 0 * stride] + in[(32 -  1) * stride]) << 2;
                    // Note: the next four blocks could be in a loop. That would help the
                    //       instruction cache but is actually slower.
                    {
                        const short *ina = in + 0 * str1;
                        const short *inb = in + 31 * str1;
                        __m256i *step1a = &step1[0];
                        __m256i *step1b = &step1[31];
                        const __m256i ina0 = loada_si256(ina);
                        const __m256i ina1 = loada_si256(ina + str1);
                        const __m256i ina2 = loada_si256(ina + str2);
                        const __m256i ina3 = loada_si256(ina + str3);
                        const __m256i inb3 = loada_si256(inb - str3);
                        const __m256i inb2 = loada_si256(inb - str2);
                        const __m256i inb1 = loada_si256(inb - str1);
                        const __m256i inb0 = loada_si256(inb);
                        step1a[0] = _mm256_add_epi16(ina0, inb0);
                        step1a[1] = _mm256_add_epi16(ina1, inb1);
                        step1a[2] = _mm256_add_epi16(ina2, inb2);
                        step1a[3] = _mm256_add_epi16(ina3, inb3);
                        step1b[-3] = _mm256_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm256_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm256_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm256_sub_epi16(ina0, inb0);
                        step1a[0] = _mm256_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm256_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm256_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm256_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm256_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm256_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm256_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm256_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const short *ina = in + 4 * str1;
                        const short *inb = in + 27 * str1;
                        __m256i *step1a = &step1[4];
                        __m256i *step1b = &step1[27];
                        const __m256i ina0 = loada_si256(ina);
                        const __m256i ina1 = loada_si256(ina + str1);
                        const __m256i ina2 = loada_si256(ina + str2);
                        const __m256i ina3 = loada_si256(ina + str3);
                        const __m256i inb3 = loada_si256(inb - str3);
                        const __m256i inb2 = loada_si256(inb - str2);
                        const __m256i inb1 = loada_si256(inb - str1);
                        const __m256i inb0 = loada_si256(inb);
                        step1a[0] = _mm256_add_epi16(ina0, inb0);
                        step1a[1] = _mm256_add_epi16(ina1, inb1);
                        step1a[2] = _mm256_add_epi16(ina2, inb2);
                        step1a[3] = _mm256_add_epi16(ina3, inb3);
                        step1b[-3] = _mm256_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm256_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm256_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm256_sub_epi16(ina0, inb0);
                        step1a[0] = _mm256_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm256_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm256_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm256_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm256_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm256_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm256_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm256_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const short *ina = in + 8 * str1;
                        const short *inb = in + 23 * str1;
                        __m256i *step1a = &step1[8];
                        __m256i *step1b = &step1[23];
                        const __m256i ina0 = loada_si256(ina);
                        const __m256i ina1 = loada_si256(ina + str1);
                        const __m256i ina2 = loada_si256(ina + str2);
                        const __m256i ina3 = loada_si256(ina + str3);
                        const __m256i inb3 = loada_si256(inb - str3);
                        const __m256i inb2 = loada_si256(inb - str2);
                        const __m256i inb1 = loada_si256(inb - str1);
                        const __m256i inb0 = loada_si256(inb);
                        step1a[0] = _mm256_add_epi16(ina0, inb0);
                        step1a[1] = _mm256_add_epi16(ina1, inb1);
                        step1a[2] = _mm256_add_epi16(ina2, inb2);
                        step1a[3] = _mm256_add_epi16(ina3, inb3);
                        step1b[-3] = _mm256_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm256_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm256_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm256_sub_epi16(ina0, inb0);
                        step1a[0] = _mm256_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm256_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm256_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm256_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm256_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm256_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm256_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm256_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const short *ina = in + 12 * str1;
                        const short *inb = in + 19 * str1;
                        __m256i *step1a = &step1[12];
                        __m256i *step1b = &step1[19];
                        const __m256i ina0 = loada_si256(ina);
                        const __m256i ina1 = loada_si256(ina + str1);
                        const __m256i ina2 = loada_si256(ina + str2);
                        const __m256i ina3 = loada_si256(ina + str3);
                        const __m256i inb3 = loada_si256(inb - str3);
                        const __m256i inb2 = loada_si256(inb - str2);
                        const __m256i inb1 = loada_si256(inb - str1);
                        const __m256i inb0 = loada_si256(inb);
                        step1a[0] = _mm256_add_epi16(ina0, inb0);
                        step1a[1] = _mm256_add_epi16(ina1, inb1);
                        step1a[2] = _mm256_add_epi16(ina2, inb2);
                        step1a[3] = _mm256_add_epi16(ina3, inb3);
                        step1b[-3] = _mm256_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm256_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm256_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm256_sub_epi16(ina0, inb0);
                        step1a[0] = _mm256_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm256_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm256_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm256_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm256_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm256_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm256_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm256_slli_epi16(step1b[-0], 2);
                    }
                } else {
                    short *in = &intermediate[column_start];
                    // step1[i] =  in[ 0 * 32] + in[(32 -  1) * 32];
                    // Note: using the same approach as above to have common offset is
                    //       counter-productive as all offsets can be calculated at compile
                    //       time.
                    // Note: the next four blocks could be in a loop. That would help the
                    //       instruction cache but is actually slower.
                    {
                        __m256i in00 =loada_si256(in + 0 * 32);
                        __m256i in01 =loada_si256(in + 1 * 32);
                        __m256i in02 =loada_si256(in + 2 * 32);
                        __m256i in03 =loada_si256(in + 3 * 32);
                        __m256i in28 =loada_si256(in + 28 * 32);
                        __m256i in29 =loada_si256(in + 29 * 32);
                        __m256i in30 =loada_si256(in + 30 * 32);
                        __m256i in31 =loada_si256(in + 31 * 32);
                        step1[0] = _mm256_add_epi16(in00, in31);
                        step1[1] = _mm256_add_epi16(in01, in30);
                        step1[2] = _mm256_add_epi16(in02, in29);
                        step1[3] = _mm256_add_epi16(in03, in28);
                        step1[28] = _mm256_sub_epi16(in03, in28);
                        step1[29] = _mm256_sub_epi16(in02, in29);
                        step1[30] = _mm256_sub_epi16(in01, in30);
                        step1[31] = _mm256_sub_epi16(in00, in31);
                    }
                    {
                        __m256i in04 = loada_si256(in + 4 * 32);
                        __m256i in05 = loada_si256(in + 5 * 32);
                        __m256i in06 = loada_si256(in + 6 * 32);
                        __m256i in07 = loada_si256(in + 7 * 32);
                        __m256i in24 = loada_si256(in + 24 * 32);
                        __m256i in25 = loada_si256(in + 25 * 32);
                        __m256i in26 = loada_si256(in + 26 * 32);
                        __m256i in27 = loada_si256(in + 27 * 32);
                        step1[4] = _mm256_add_epi16(in04, in27);
                        step1[5] = _mm256_add_epi16(in05, in26);
                        step1[6] = _mm256_add_epi16(in06, in25);
                        step1[7] = _mm256_add_epi16(in07, in24);
                        step1[24] = _mm256_sub_epi16(in07, in24);
                        step1[25] = _mm256_sub_epi16(in06, in25);
                        step1[26] = _mm256_sub_epi16(in05, in26);
                        step1[27] = _mm256_sub_epi16(in04, in27);
                    }
                    {
                        __m256i in08 = loada_si256(in + 8 * 32);
                        __m256i in09 = loada_si256(in + 9 * 32);
                        __m256i in10 = loada_si256(in + 10 * 32);
                        __m256i in11 = loada_si256(in + 11 * 32);
                        __m256i in20 = loada_si256(in + 20 * 32);
                        __m256i in21 = loada_si256(in + 21 * 32);
                        __m256i in22 = loada_si256(in + 22 * 32);
                        __m256i in23 = loada_si256(in + 23 * 32);
                        step1[8] = _mm256_add_epi16(in08, in23);
                        step1[9] = _mm256_add_epi16(in09, in22);
                        step1[10] = _mm256_add_epi16(in10, in21);
                        step1[11] = _mm256_add_epi16(in11, in20);
                        step1[20] = _mm256_sub_epi16(in11, in20);
                        step1[21] = _mm256_sub_epi16(in10, in21);
                        step1[22] = _mm256_sub_epi16(in09, in22);
                        step1[23] = _mm256_sub_epi16(in08, in23);
                    }
                    {
                        __m256i in12 = loada_si256(in + 12 * 32);
                        __m256i in13 = loada_si256(in + 13 * 32);
                        __m256i in14 = loada_si256(in + 14 * 32);
                        __m256i in15 = loada_si256(in + 15 * 32);
                        __m256i in16 = loada_si256(in + 16 * 32);
                        __m256i in17 = loada_si256(in + 17 * 32);
                        __m256i in18 = loada_si256(in + 18 * 32);
                        __m256i in19 = loada_si256(in + 19 * 32);
                        step1[12] = _mm256_add_epi16(in12, in19);
                        step1[13] = _mm256_add_epi16(in13, in18);
                        step1[14] = _mm256_add_epi16(in14, in17);
                        step1[15] = _mm256_add_epi16(in15, in16);
                        step1[16] = _mm256_sub_epi16(in15, in16);
                        step1[17] = _mm256_sub_epi16(in14, in17);
                        step1[18] = _mm256_sub_epi16(in13, in18);
                        step1[19] = _mm256_sub_epi16(in12, in19);
                    }
                }
                // Stage 2
                {
                    step2[0] = _mm256_add_epi16(step1[0], step1[15]);
                    step2[1] = _mm256_add_epi16(step1[1], step1[14]);
                    step2[2] = _mm256_add_epi16(step1[2], step1[13]);
                    step2[3] = _mm256_add_epi16(step1[3], step1[12]);
                    step2[4] = _mm256_add_epi16(step1[4], step1[11]);
                    step2[5] = _mm256_add_epi16(step1[5], step1[10]);
                    step2[6] = _mm256_add_epi16(step1[6], step1[9]);
                    step2[7] = _mm256_add_epi16(step1[7], step1[8]);
                    step2[8] = _mm256_sub_epi16(step1[7], step1[8]);
                    step2[9] = _mm256_sub_epi16(step1[6], step1[9]);
                    step2[10] = _mm256_sub_epi16(step1[5], step1[10]);
                    step2[11] = _mm256_sub_epi16(step1[4], step1[11]);
                    step2[12] = _mm256_sub_epi16(step1[3], step1[12]);
                    step2[13] = _mm256_sub_epi16(step1[2], step1[13]);
                    step2[14] = _mm256_sub_epi16(step1[1], step1[14]);
                    step2[15] = _mm256_sub_epi16(step1[0], step1[15]);
                }
                {
                    const __m256i s2_20_0 = _mm256_unpacklo_epi16(step1[27], step1[20]);
                    const __m256i s2_20_1 = _mm256_unpackhi_epi16(step1[27], step1[20]);
                    const __m256i s2_21_0 = _mm256_unpacklo_epi16(step1[26], step1[21]);
                    const __m256i s2_21_1 = _mm256_unpackhi_epi16(step1[26], step1[21]);
                    const __m256i s2_22_0 = _mm256_unpacklo_epi16(step1[25], step1[22]);
                    const __m256i s2_22_1 = _mm256_unpackhi_epi16(step1[25], step1[22]);
                    const __m256i s2_23_0 = _mm256_unpacklo_epi16(step1[24], step1[23]);
                    const __m256i s2_23_1 = _mm256_unpackhi_epi16(step1[24], step1[23]);
                    const __m256i s2_20_2 = _mm256_madd_epi16(s2_20_0, k__cospi_p16_m16);
                    const __m256i s2_20_3 = _mm256_madd_epi16(s2_20_1, k__cospi_p16_m16);
                    const __m256i s2_21_2 = _mm256_madd_epi16(s2_21_0, k__cospi_p16_m16);
                    const __m256i s2_21_3 = _mm256_madd_epi16(s2_21_1, k__cospi_p16_m16);
                    const __m256i s2_22_2 = _mm256_madd_epi16(s2_22_0, k__cospi_p16_m16);
                    const __m256i s2_22_3 = _mm256_madd_epi16(s2_22_1, k__cospi_p16_m16);
                    const __m256i s2_23_2 = _mm256_madd_epi16(s2_23_0, k__cospi_p16_m16);
                    const __m256i s2_23_3 = _mm256_madd_epi16(s2_23_1, k__cospi_p16_m16);
                    const __m256i s2_24_2 = _mm256_madd_epi16(s2_23_0, k__cospi_p16_p16);
                    const __m256i s2_24_3 = _mm256_madd_epi16(s2_23_1, k__cospi_p16_p16);
                    const __m256i s2_25_2 = _mm256_madd_epi16(s2_22_0, k__cospi_p16_p16);
                    const __m256i s2_25_3 = _mm256_madd_epi16(s2_22_1, k__cospi_p16_p16);
                    const __m256i s2_26_2 = _mm256_madd_epi16(s2_21_0, k__cospi_p16_p16);
                    const __m256i s2_26_3 = _mm256_madd_epi16(s2_21_1, k__cospi_p16_p16);
                    const __m256i s2_27_2 = _mm256_madd_epi16(s2_20_0, k__cospi_p16_p16);
                    const __m256i s2_27_3 = _mm256_madd_epi16(s2_20_1, k__cospi_p16_p16);
                    // dct_const_round_shift
                    const __m256i s2_20_4 = _mm256_add_epi32(s2_20_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_20_5 = _mm256_add_epi32(s2_20_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_21_4 = _mm256_add_epi32(s2_21_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_21_5 = _mm256_add_epi32(s2_21_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_22_4 = _mm256_add_epi32(s2_22_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_22_5 = _mm256_add_epi32(s2_22_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_23_4 = _mm256_add_epi32(s2_23_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_23_5 = _mm256_add_epi32(s2_23_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_24_4 = _mm256_add_epi32(s2_24_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_24_5 = _mm256_add_epi32(s2_24_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_25_4 = _mm256_add_epi32(s2_25_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_25_5 = _mm256_add_epi32(s2_25_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_26_4 = _mm256_add_epi32(s2_26_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_26_5 = _mm256_add_epi32(s2_26_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_27_4 = _mm256_add_epi32(s2_27_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_27_5 = _mm256_add_epi32(s2_27_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_20_6 = _mm256_srai_epi32(s2_20_4, DCT_CONST_BITS);
                    const __m256i s2_20_7 = _mm256_srai_epi32(s2_20_5, DCT_CONST_BITS);
                    const __m256i s2_21_6 = _mm256_srai_epi32(s2_21_4, DCT_CONST_BITS);
                    const __m256i s2_21_7 = _mm256_srai_epi32(s2_21_5, DCT_CONST_BITS);
                    const __m256i s2_22_6 = _mm256_srai_epi32(s2_22_4, DCT_CONST_BITS);
                    const __m256i s2_22_7 = _mm256_srai_epi32(s2_22_5, DCT_CONST_BITS);
                    const __m256i s2_23_6 = _mm256_srai_epi32(s2_23_4, DCT_CONST_BITS);
                    const __m256i s2_23_7 = _mm256_srai_epi32(s2_23_5, DCT_CONST_BITS);
                    const __m256i s2_24_6 = _mm256_srai_epi32(s2_24_4, DCT_CONST_BITS);
                    const __m256i s2_24_7 = _mm256_srai_epi32(s2_24_5, DCT_CONST_BITS);
                    const __m256i s2_25_6 = _mm256_srai_epi32(s2_25_4, DCT_CONST_BITS);
                    const __m256i s2_25_7 = _mm256_srai_epi32(s2_25_5, DCT_CONST_BITS);
                    const __m256i s2_26_6 = _mm256_srai_epi32(s2_26_4, DCT_CONST_BITS);
                    const __m256i s2_26_7 = _mm256_srai_epi32(s2_26_5, DCT_CONST_BITS);
                    const __m256i s2_27_6 = _mm256_srai_epi32(s2_27_4, DCT_CONST_BITS);
                    const __m256i s2_27_7 = _mm256_srai_epi32(s2_27_5, DCT_CONST_BITS);
                    // Combine
                    step2[20] = _mm256_packs_epi32(s2_20_6, s2_20_7);
                    step2[21] = _mm256_packs_epi32(s2_21_6, s2_21_7);
                    step2[22] = _mm256_packs_epi32(s2_22_6, s2_22_7);
                    step2[23] = _mm256_packs_epi32(s2_23_6, s2_23_7);
                    step2[24] = _mm256_packs_epi32(s2_24_6, s2_24_7);
                    step2[25] = _mm256_packs_epi32(s2_25_6, s2_25_7);
                    step2[26] = _mm256_packs_epi32(s2_26_6, s2_26_7);
                    step2[27] = _mm256_packs_epi32(s2_27_6, s2_27_7);
                }

                if (pass == 0) {
                    // Stage 3
                    {
                        step3[0] = _mm256_add_epi16(step2[(8 - 1)], step2[0]);
                        step3[1] = _mm256_add_epi16(step2[(8 - 2)], step2[1]);
                        step3[2] = _mm256_add_epi16(step2[(8 - 3)], step2[2]);
                        step3[3] = _mm256_add_epi16(step2[(8 - 4)], step2[3]);
                        step3[4] = _mm256_sub_epi16(step2[(8 - 5)], step2[4]);
                        step3[5] = _mm256_sub_epi16(step2[(8 - 6)], step2[5]);
                        step3[6] = _mm256_sub_epi16(step2[(8 - 7)], step2[6]);
                        step3[7] = _mm256_sub_epi16(step2[(8 - 8)], step2[7]);
                    }
                    {
                        const __m256i s3_10_0 = _mm256_unpacklo_epi16(step2[13], step2[10]);
                        const __m256i s3_10_1 = _mm256_unpackhi_epi16(step2[13], step2[10]);
                        const __m256i s3_11_0 = _mm256_unpacklo_epi16(step2[12], step2[11]);
                        const __m256i s3_11_1 = _mm256_unpackhi_epi16(step2[12], step2[11]);
                        const __m256i s3_10_2 = _mm256_madd_epi16(s3_10_0, k__cospi_p16_m16);
                        const __m256i s3_10_3 = _mm256_madd_epi16(s3_10_1, k__cospi_p16_m16);
                        const __m256i s3_11_2 = _mm256_madd_epi16(s3_11_0, k__cospi_p16_m16);
                        const __m256i s3_11_3 = _mm256_madd_epi16(s3_11_1, k__cospi_p16_m16);
                        const __m256i s3_12_2 = _mm256_madd_epi16(s3_11_0, k__cospi_p16_p16);
                        const __m256i s3_12_3 = _mm256_madd_epi16(s3_11_1, k__cospi_p16_p16);
                        const __m256i s3_13_2 = _mm256_madd_epi16(s3_10_0, k__cospi_p16_p16);
                        const __m256i s3_13_3 = _mm256_madd_epi16(s3_10_1, k__cospi_p16_p16);
                        // dct_const_round_shift
                        const __m256i s3_10_4 = _mm256_add_epi32(s3_10_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_10_5 = _mm256_add_epi32(s3_10_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_11_4 = _mm256_add_epi32(s3_11_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_11_5 = _mm256_add_epi32(s3_11_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_12_4 = _mm256_add_epi32(s3_12_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_12_5 = _mm256_add_epi32(s3_12_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_13_4 = _mm256_add_epi32(s3_13_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_13_5 = _mm256_add_epi32(s3_13_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_10_6 = _mm256_srai_epi32(s3_10_4, DCT_CONST_BITS);
                        const __m256i s3_10_7 = _mm256_srai_epi32(s3_10_5, DCT_CONST_BITS);
                        const __m256i s3_11_6 = _mm256_srai_epi32(s3_11_4, DCT_CONST_BITS);
                        const __m256i s3_11_7 = _mm256_srai_epi32(s3_11_5, DCT_CONST_BITS);
                        const __m256i s3_12_6 = _mm256_srai_epi32(s3_12_4, DCT_CONST_BITS);
                        const __m256i s3_12_7 = _mm256_srai_epi32(s3_12_5, DCT_CONST_BITS);
                        const __m256i s3_13_6 = _mm256_srai_epi32(s3_13_4, DCT_CONST_BITS);
                        const __m256i s3_13_7 = _mm256_srai_epi32(s3_13_5, DCT_CONST_BITS);
                        // Combine
                        step3[10] = _mm256_packs_epi32(s3_10_6, s3_10_7);
                        step3[11] = _mm256_packs_epi32(s3_11_6, s3_11_7);
                        step3[12] = _mm256_packs_epi32(s3_12_6, s3_12_7);
                        step3[13] = _mm256_packs_epi32(s3_13_6, s3_13_7);
                    }
                    {
                        step3[16] = _mm256_add_epi16(step2[23], step1[16]);
                        step3[17] = _mm256_add_epi16(step2[22], step1[17]);
                        step3[18] = _mm256_add_epi16(step2[21], step1[18]);
                        step3[19] = _mm256_add_epi16(step2[20], step1[19]);
                        step3[20] = _mm256_sub_epi16(step1[19], step2[20]);
                        step3[21] = _mm256_sub_epi16(step1[18], step2[21]);
                        step3[22] = _mm256_sub_epi16(step1[17], step2[22]);
                        step3[23] = _mm256_sub_epi16(step1[16], step2[23]);
                        step3[24] = _mm256_sub_epi16(step1[31], step2[24]);
                        step3[25] = _mm256_sub_epi16(step1[30], step2[25]);
                        step3[26] = _mm256_sub_epi16(step1[29], step2[26]);
                        step3[27] = _mm256_sub_epi16(step1[28], step2[27]);
                        step3[28] = _mm256_add_epi16(step2[27], step1[28]);
                        step3[29] = _mm256_add_epi16(step2[26], step1[29]);
                        step3[30] = _mm256_add_epi16(step2[25], step1[30]);
                        step3[31] = _mm256_add_epi16(step2[24], step1[31]);
                    }

                    // Stage 4
                    {
                        step1[0] = _mm256_add_epi16(step3[3], step3[0]);
                        step1[1] = _mm256_add_epi16(step3[2], step3[1]);
                        step1[2] = _mm256_sub_epi16(step3[1], step3[2]);
                        step1[3] = _mm256_sub_epi16(step3[0], step3[3]);
                        step1[8] = _mm256_add_epi16(step3[11], step2[8]);
                        step1[9] = _mm256_add_epi16(step3[10], step2[9]);
                        step1[10] = _mm256_sub_epi16(step2[9], step3[10]);
                        step1[11] = _mm256_sub_epi16(step2[8], step3[11]);
                        step1[12] = _mm256_sub_epi16(step2[15], step3[12]);
                        step1[13] = _mm256_sub_epi16(step2[14], step3[13]);
                        step1[14] = _mm256_add_epi16(step3[13], step2[14]);
                        step1[15] = _mm256_add_epi16(step3[12], step2[15]);
                    }
                    {
                        const __m256i s1_05_0 = _mm256_unpacklo_epi16(step3[6], step3[5]);
                        const __m256i s1_05_1 = _mm256_unpackhi_epi16(step3[6], step3[5]);
                        const __m256i s1_05_2 = _mm256_madd_epi16(s1_05_0, k__cospi_p16_m16);
                        const __m256i s1_05_3 = _mm256_madd_epi16(s1_05_1, k__cospi_p16_m16);
                        const __m256i s1_06_2 = _mm256_madd_epi16(s1_05_0, k__cospi_p16_p16);
                        const __m256i s1_06_3 = _mm256_madd_epi16(s1_05_1, k__cospi_p16_p16);
                        // dct_const_round_shift
                        const __m256i s1_05_4 = _mm256_add_epi32(s1_05_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_05_5 = _mm256_add_epi32(s1_05_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_06_4 = _mm256_add_epi32(s1_06_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_06_5 = _mm256_add_epi32(s1_06_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_05_6 = _mm256_srai_epi32(s1_05_4, DCT_CONST_BITS);
                        const __m256i s1_05_7 = _mm256_srai_epi32(s1_05_5, DCT_CONST_BITS);
                        const __m256i s1_06_6 = _mm256_srai_epi32(s1_06_4, DCT_CONST_BITS);
                        const __m256i s1_06_7 = _mm256_srai_epi32(s1_06_5, DCT_CONST_BITS);
                        // Combine
                        step1[5] = _mm256_packs_epi32(s1_05_6, s1_05_7);
                        step1[6] = _mm256_packs_epi32(s1_06_6, s1_06_7);
                    }
                    {
                        const __m256i s1_18_0 = _mm256_unpacklo_epi16(step3[18], step3[29]);
                        const __m256i s1_18_1 = _mm256_unpackhi_epi16(step3[18], step3[29]);
                        const __m256i s1_19_0 = _mm256_unpacklo_epi16(step3[19], step3[28]);
                        const __m256i s1_19_1 = _mm256_unpackhi_epi16(step3[19], step3[28]);
                        const __m256i s1_20_0 = _mm256_unpacklo_epi16(step3[20], step3[27]);
                        const __m256i s1_20_1 = _mm256_unpackhi_epi16(step3[20], step3[27]);
                        const __m256i s1_21_0 = _mm256_unpacklo_epi16(step3[21], step3[26]);
                        const __m256i s1_21_1 = _mm256_unpackhi_epi16(step3[21], step3[26]);
                        const __m256i s1_18_2 = _mm256_madd_epi16(s1_18_0, k__cospi_m08_p24);
                        const __m256i s1_18_3 = _mm256_madd_epi16(s1_18_1, k__cospi_m08_p24);
                        const __m256i s1_19_2 = _mm256_madd_epi16(s1_19_0, k__cospi_m08_p24);
                        const __m256i s1_19_3 = _mm256_madd_epi16(s1_19_1, k__cospi_m08_p24);
                        const __m256i s1_20_2 = _mm256_madd_epi16(s1_20_0, k__cospi_m24_m08);
                        const __m256i s1_20_3 = _mm256_madd_epi16(s1_20_1, k__cospi_m24_m08);
                        const __m256i s1_21_2 = _mm256_madd_epi16(s1_21_0, k__cospi_m24_m08);
                        const __m256i s1_21_3 = _mm256_madd_epi16(s1_21_1, k__cospi_m24_m08);
                        const __m256i s1_26_2 = _mm256_madd_epi16(s1_21_0, k__cospi_m08_p24);
                        const __m256i s1_26_3 = _mm256_madd_epi16(s1_21_1, k__cospi_m08_p24);
                        const __m256i s1_27_2 = _mm256_madd_epi16(s1_20_0, k__cospi_m08_p24);
                        const __m256i s1_27_3 = _mm256_madd_epi16(s1_20_1, k__cospi_m08_p24);
                        const __m256i s1_28_2 = _mm256_madd_epi16(s1_19_0, k__cospi_p24_p08);
                        const __m256i s1_28_3 = _mm256_madd_epi16(s1_19_1, k__cospi_p24_p08);
                        const __m256i s1_29_2 = _mm256_madd_epi16(s1_18_0, k__cospi_p24_p08);
                        const __m256i s1_29_3 = _mm256_madd_epi16(s1_18_1, k__cospi_p24_p08);
                        // dct_const_round_shift
                        const __m256i s1_18_4 = _mm256_add_epi32(s1_18_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_18_5 = _mm256_add_epi32(s1_18_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_19_4 = _mm256_add_epi32(s1_19_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_19_5 = _mm256_add_epi32(s1_19_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_20_4 = _mm256_add_epi32(s1_20_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_20_5 = _mm256_add_epi32(s1_20_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_21_4 = _mm256_add_epi32(s1_21_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_21_5 = _mm256_add_epi32(s1_21_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_26_4 = _mm256_add_epi32(s1_26_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_26_5 = _mm256_add_epi32(s1_26_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_27_4 = _mm256_add_epi32(s1_27_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_27_5 = _mm256_add_epi32(s1_27_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_28_4 = _mm256_add_epi32(s1_28_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_28_5 = _mm256_add_epi32(s1_28_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_29_4 = _mm256_add_epi32(s1_29_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_29_5 = _mm256_add_epi32(s1_29_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_18_6 = _mm256_srai_epi32(s1_18_4, DCT_CONST_BITS);
                        const __m256i s1_18_7 = _mm256_srai_epi32(s1_18_5, DCT_CONST_BITS);
                        const __m256i s1_19_6 = _mm256_srai_epi32(s1_19_4, DCT_CONST_BITS);
                        const __m256i s1_19_7 = _mm256_srai_epi32(s1_19_5, DCT_CONST_BITS);
                        const __m256i s1_20_6 = _mm256_srai_epi32(s1_20_4, DCT_CONST_BITS);
                        const __m256i s1_20_7 = _mm256_srai_epi32(s1_20_5, DCT_CONST_BITS);
                        const __m256i s1_21_6 = _mm256_srai_epi32(s1_21_4, DCT_CONST_BITS);
                        const __m256i s1_21_7 = _mm256_srai_epi32(s1_21_5, DCT_CONST_BITS);
                        const __m256i s1_26_6 = _mm256_srai_epi32(s1_26_4, DCT_CONST_BITS);
                        const __m256i s1_26_7 = _mm256_srai_epi32(s1_26_5, DCT_CONST_BITS);
                        const __m256i s1_27_6 = _mm256_srai_epi32(s1_27_4, DCT_CONST_BITS);
                        const __m256i s1_27_7 = _mm256_srai_epi32(s1_27_5, DCT_CONST_BITS);
                        const __m256i s1_28_6 = _mm256_srai_epi32(s1_28_4, DCT_CONST_BITS);
                        const __m256i s1_28_7 = _mm256_srai_epi32(s1_28_5, DCT_CONST_BITS);
                        const __m256i s1_29_6 = _mm256_srai_epi32(s1_29_4, DCT_CONST_BITS);
                        const __m256i s1_29_7 = _mm256_srai_epi32(s1_29_5, DCT_CONST_BITS);
                        // Combine
                        step1[18] = _mm256_packs_epi32(s1_18_6, s1_18_7);
                        step1[19] = _mm256_packs_epi32(s1_19_6, s1_19_7);
                        step1[20] = _mm256_packs_epi32(s1_20_6, s1_20_7);
                        step1[21] = _mm256_packs_epi32(s1_21_6, s1_21_7);
                        step1[26] = _mm256_packs_epi32(s1_26_6, s1_26_7);
                        step1[27] = _mm256_packs_epi32(s1_27_6, s1_27_7);
                        step1[28] = _mm256_packs_epi32(s1_28_6, s1_28_7);
                        step1[29] = _mm256_packs_epi32(s1_29_6, s1_29_7);
                    }
                    // Stage 5
                    {
                        step2[4] = _mm256_add_epi16(step1[5], step3[4]);
                        step2[5] = _mm256_sub_epi16(step3[4], step1[5]);
                        step2[6] = _mm256_sub_epi16(step3[7], step1[6]);
                        step2[7] = _mm256_add_epi16(step1[6], step3[7]);
                    }
                    {
                        const __m256i out_00_0 = _mm256_unpacklo_epi16(step1[0], step1[1]);
                        const __m256i out_00_1 = _mm256_unpackhi_epi16(step1[0], step1[1]);
                        const __m256i out_08_0 = _mm256_unpacklo_epi16(step1[2], step1[3]);
                        const __m256i out_08_1 = _mm256_unpackhi_epi16(step1[2], step1[3]);
                        const __m256i out_00_2 = _mm256_madd_epi16(out_00_0, k__cospi_p16_p16);
                        const __m256i out_00_3 = _mm256_madd_epi16(out_00_1, k__cospi_p16_p16);
                        const __m256i out_16_2 = _mm256_madd_epi16(out_00_0, k__cospi_p16_m16);
                        const __m256i out_16_3 = _mm256_madd_epi16(out_00_1, k__cospi_p16_m16);
                        const __m256i out_08_2 = _mm256_madd_epi16(out_08_0, k__cospi_p24_p08);
                        const __m256i out_08_3 = _mm256_madd_epi16(out_08_1, k__cospi_p24_p08);
                        const __m256i out_24_2 = _mm256_madd_epi16(out_08_0, k__cospi_m08_p24);
                        const __m256i out_24_3 = _mm256_madd_epi16(out_08_1, k__cospi_m08_p24);
                        // dct_const_round_shift
                        const __m256i out_00_4 = _mm256_add_epi32(out_00_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_00_5 = _mm256_add_epi32(out_00_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_16_4 = _mm256_add_epi32(out_16_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_16_5 = _mm256_add_epi32(out_16_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_08_4 = _mm256_add_epi32(out_08_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_08_5 = _mm256_add_epi32(out_08_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_24_4 = _mm256_add_epi32(out_24_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_24_5 = _mm256_add_epi32(out_24_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_00_6 = _mm256_srai_epi32(out_00_4, DCT_CONST_BITS);
                        const __m256i out_00_7 = _mm256_srai_epi32(out_00_5, DCT_CONST_BITS);
                        const __m256i out_16_6 = _mm256_srai_epi32(out_16_4, DCT_CONST_BITS);
                        const __m256i out_16_7 = _mm256_srai_epi32(out_16_5, DCT_CONST_BITS);
                        const __m256i out_08_6 = _mm256_srai_epi32(out_08_4, DCT_CONST_BITS);
                        const __m256i out_08_7 = _mm256_srai_epi32(out_08_5, DCT_CONST_BITS);
                        const __m256i out_24_6 = _mm256_srai_epi32(out_24_4, DCT_CONST_BITS);
                        const __m256i out_24_7 = _mm256_srai_epi32(out_24_5, DCT_CONST_BITS);
                        // Combine
                        out[0]  = _mm256_packs_epi32(out_00_6, out_00_7);
                        out[16] = _mm256_packs_epi32(out_16_6, out_16_7);
                        out[8]  = _mm256_packs_epi32(out_08_6, out_08_7);
                        out[24] = _mm256_packs_epi32(out_24_6, out_24_7);
                    }
                    {
                        const __m256i s2_09_0 = _mm256_unpacklo_epi16(step1[9], step1[14]);
                        const __m256i s2_09_1 = _mm256_unpackhi_epi16(step1[9], step1[14]);
                        const __m256i s2_10_0 = _mm256_unpacklo_epi16(step1[10], step1[13]);
                        const __m256i s2_10_1 = _mm256_unpackhi_epi16(step1[10], step1[13]);
                        const __m256i s2_09_2 = _mm256_madd_epi16(s2_09_0, k__cospi_m08_p24);
                        const __m256i s2_09_3 = _mm256_madd_epi16(s2_09_1, k__cospi_m08_p24);
                        const __m256i s2_10_2 = _mm256_madd_epi16(s2_10_0, k__cospi_m24_m08);
                        const __m256i s2_10_3 = _mm256_madd_epi16(s2_10_1, k__cospi_m24_m08);
                        const __m256i s2_13_2 = _mm256_madd_epi16(s2_10_0, k__cospi_m08_p24);
                        const __m256i s2_13_3 = _mm256_madd_epi16(s2_10_1, k__cospi_m08_p24);
                        const __m256i s2_14_2 = _mm256_madd_epi16(s2_09_0, k__cospi_p24_p08);
                        const __m256i s2_14_3 = _mm256_madd_epi16(s2_09_1, k__cospi_p24_p08);
                        // dct_const_round_shift
                        const __m256i s2_09_4 = _mm256_add_epi32(s2_09_2, k__DCT_CONST_ROUNDING);
                        const __m256i s2_09_5 = _mm256_add_epi32(s2_09_3, k__DCT_CONST_ROUNDING);
                        const __m256i s2_10_4 = _mm256_add_epi32(s2_10_2, k__DCT_CONST_ROUNDING);
                        const __m256i s2_10_5 = _mm256_add_epi32(s2_10_3, k__DCT_CONST_ROUNDING);
                        const __m256i s2_13_4 = _mm256_add_epi32(s2_13_2, k__DCT_CONST_ROUNDING);
                        const __m256i s2_13_5 = _mm256_add_epi32(s2_13_3, k__DCT_CONST_ROUNDING);
                        const __m256i s2_14_4 = _mm256_add_epi32(s2_14_2, k__DCT_CONST_ROUNDING);
                        const __m256i s2_14_5 = _mm256_add_epi32(s2_14_3, k__DCT_CONST_ROUNDING);
                        const __m256i s2_09_6 = _mm256_srai_epi32(s2_09_4, DCT_CONST_BITS);
                        const __m256i s2_09_7 = _mm256_srai_epi32(s2_09_5, DCT_CONST_BITS);
                        const __m256i s2_10_6 = _mm256_srai_epi32(s2_10_4, DCT_CONST_BITS);
                        const __m256i s2_10_7 = _mm256_srai_epi32(s2_10_5, DCT_CONST_BITS);
                        const __m256i s2_13_6 = _mm256_srai_epi32(s2_13_4, DCT_CONST_BITS);
                        const __m256i s2_13_7 = _mm256_srai_epi32(s2_13_5, DCT_CONST_BITS);
                        const __m256i s2_14_6 = _mm256_srai_epi32(s2_14_4, DCT_CONST_BITS);
                        const __m256i s2_14_7 = _mm256_srai_epi32(s2_14_5, DCT_CONST_BITS);
                        // Combine
                        step2[9]  = _mm256_packs_epi32(s2_09_6, s2_09_7);
                        step2[10] = _mm256_packs_epi32(s2_10_6, s2_10_7);
                        step2[13] = _mm256_packs_epi32(s2_13_6, s2_13_7);
                        step2[14] = _mm256_packs_epi32(s2_14_6, s2_14_7);
                    }
                    {
                        step2[16] = _mm256_add_epi16(step1[19], step3[16]);
                        step2[17] = _mm256_add_epi16(step1[18], step3[17]);
                        step2[18] = _mm256_sub_epi16(step3[17], step1[18]);
                        step2[19] = _mm256_sub_epi16(step3[16], step1[19]);
                        step2[20] = _mm256_sub_epi16(step3[23], step1[20]);
                        step2[21] = _mm256_sub_epi16(step3[22], step1[21]);
                        step2[22] = _mm256_add_epi16(step1[21], step3[22]);
                        step2[23] = _mm256_add_epi16(step1[20], step3[23]);
                        step2[24] = _mm256_add_epi16(step1[27], step3[24]);
                        step2[25] = _mm256_add_epi16(step1[26], step3[25]);
                        step2[26] = _mm256_sub_epi16(step3[25], step1[26]);
                        step2[27] = _mm256_sub_epi16(step3[24], step1[27]);
                        step2[28] = _mm256_sub_epi16(step3[31], step1[28]);
                        step2[29] = _mm256_sub_epi16(step3[30], step1[29]);
                        step2[30] = _mm256_add_epi16(step1[29], step3[30]);
                        step2[31] = _mm256_add_epi16(step1[28], step3[31]);
                    }
                    // Stage 6
                    {
                        const __m256i out_04_0 = _mm256_unpacklo_epi16(step2[4], step2[7]);
                        const __m256i out_04_1 = _mm256_unpackhi_epi16(step2[4], step2[7]);
                        const __m256i out_20_0 = _mm256_unpacklo_epi16(step2[5], step2[6]);
                        const __m256i out_20_1 = _mm256_unpackhi_epi16(step2[5], step2[6]);
                        const __m256i out_12_0 = _mm256_unpacklo_epi16(step2[5], step2[6]);
                        const __m256i out_12_1 = _mm256_unpackhi_epi16(step2[5], step2[6]);
                        const __m256i out_28_0 = _mm256_unpacklo_epi16(step2[4], step2[7]);
                        const __m256i out_28_1 = _mm256_unpackhi_epi16(step2[4], step2[7]);
                        const __m256i out_04_2 = _mm256_madd_epi16(out_04_0, k__cospi_p28_p04);
                        const __m256i out_04_3 = _mm256_madd_epi16(out_04_1, k__cospi_p28_p04);
                        const __m256i out_20_2 = _mm256_madd_epi16(out_20_0, k__cospi_p12_p20);
                        const __m256i out_20_3 = _mm256_madd_epi16(out_20_1, k__cospi_p12_p20);
                        const __m256i out_12_2 = _mm256_madd_epi16(out_12_0, k__cospi_m20_p12);
                        const __m256i out_12_3 = _mm256_madd_epi16(out_12_1, k__cospi_m20_p12);
                        const __m256i out_28_2 = _mm256_madd_epi16(out_28_0, k__cospi_m04_p28);
                        const __m256i out_28_3 = _mm256_madd_epi16(out_28_1, k__cospi_m04_p28);
                        // dct_const_round_shift
                        const __m256i out_04_4 = _mm256_add_epi32(out_04_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_04_5 = _mm256_add_epi32(out_04_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_20_4 = _mm256_add_epi32(out_20_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_20_5 = _mm256_add_epi32(out_20_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_12_4 = _mm256_add_epi32(out_12_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_12_5 = _mm256_add_epi32(out_12_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_28_4 = _mm256_add_epi32(out_28_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_28_5 = _mm256_add_epi32(out_28_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_04_6 = _mm256_srai_epi32(out_04_4, DCT_CONST_BITS);
                        const __m256i out_04_7 = _mm256_srai_epi32(out_04_5, DCT_CONST_BITS);
                        const __m256i out_20_6 = _mm256_srai_epi32(out_20_4, DCT_CONST_BITS);
                        const __m256i out_20_7 = _mm256_srai_epi32(out_20_5, DCT_CONST_BITS);
                        const __m256i out_12_6 = _mm256_srai_epi32(out_12_4, DCT_CONST_BITS);
                        const __m256i out_12_7 = _mm256_srai_epi32(out_12_5, DCT_CONST_BITS);
                        const __m256i out_28_6 = _mm256_srai_epi32(out_28_4, DCT_CONST_BITS);
                        const __m256i out_28_7 = _mm256_srai_epi32(out_28_5, DCT_CONST_BITS);
                        // Combine
                        out[4]  = _mm256_packs_epi32(out_04_6, out_04_7);
                        out[20] = _mm256_packs_epi32(out_20_6, out_20_7);
                        out[12] = _mm256_packs_epi32(out_12_6, out_12_7);
                        out[28] = _mm256_packs_epi32(out_28_6, out_28_7);
                    }
                    {
                        step3[8]  = _mm256_add_epi16(step2[9], step1[8]);
                        step3[9]  = _mm256_sub_epi16(step1[8], step2[9]);
                        step3[10] = _mm256_sub_epi16(step1[11], step2[10]);
                        step3[11] = _mm256_add_epi16(step2[10], step1[11]);
                        step3[12] = _mm256_add_epi16(step2[13], step1[12]);
                        step3[13] = _mm256_sub_epi16(step1[12], step2[13]);
                        step3[14] = _mm256_sub_epi16(step1[15], step2[14]);
                        step3[15] = _mm256_add_epi16(step2[14], step1[15]);
                    }
                    {
                        const __m256i s3_17_0 = _mm256_unpacklo_epi16(step2[17], step2[30]);
                        const __m256i s3_17_1 = _mm256_unpackhi_epi16(step2[17], step2[30]);
                        const __m256i s3_18_0 = _mm256_unpacklo_epi16(step2[18], step2[29]);
                        const __m256i s3_18_1 = _mm256_unpackhi_epi16(step2[18], step2[29]);
                        const __m256i s3_21_0 = _mm256_unpacklo_epi16(step2[21], step2[26]);
                        const __m256i s3_21_1 = _mm256_unpackhi_epi16(step2[21], step2[26]);
                        const __m256i s3_22_0 = _mm256_unpacklo_epi16(step2[22], step2[25]);
                        const __m256i s3_22_1 = _mm256_unpackhi_epi16(step2[22], step2[25]);
                        const __m256i s3_17_2 = _mm256_madd_epi16(s3_17_0, k__cospi_m04_p28);
                        const __m256i s3_17_3 = _mm256_madd_epi16(s3_17_1, k__cospi_m04_p28);
                        const __m256i s3_18_2 = _mm256_madd_epi16(s3_18_0, k__cospi_m28_m04);
                        const __m256i s3_18_3 = _mm256_madd_epi16(s3_18_1, k__cospi_m28_m04);
                        const __m256i s3_21_2 = _mm256_madd_epi16(s3_21_0, k__cospi_m20_p12);
                        const __m256i s3_21_3 = _mm256_madd_epi16(s3_21_1, k__cospi_m20_p12);
                        const __m256i s3_22_2 = _mm256_madd_epi16(s3_22_0, k__cospi_m12_m20);
                        const __m256i s3_22_3 = _mm256_madd_epi16(s3_22_1, k__cospi_m12_m20);
                        const __m256i s3_25_2 = _mm256_madd_epi16(s3_22_0, k__cospi_m20_p12);
                        const __m256i s3_25_3 = _mm256_madd_epi16(s3_22_1, k__cospi_m20_p12);
                        const __m256i s3_26_2 = _mm256_madd_epi16(s3_21_0, k__cospi_p12_p20);
                        const __m256i s3_26_3 = _mm256_madd_epi16(s3_21_1, k__cospi_p12_p20);
                        const __m256i s3_29_2 = _mm256_madd_epi16(s3_18_0, k__cospi_m04_p28);
                        const __m256i s3_29_3 = _mm256_madd_epi16(s3_18_1, k__cospi_m04_p28);
                        const __m256i s3_30_2 = _mm256_madd_epi16(s3_17_0, k__cospi_p28_p04);
                        const __m256i s3_30_3 = _mm256_madd_epi16(s3_17_1, k__cospi_p28_p04);
                        // dct_const_round_shift
                        const __m256i s3_17_4 = _mm256_add_epi32(s3_17_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_17_5 = _mm256_add_epi32(s3_17_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_18_4 = _mm256_add_epi32(s3_18_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_18_5 = _mm256_add_epi32(s3_18_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_21_4 = _mm256_add_epi32(s3_21_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_21_5 = _mm256_add_epi32(s3_21_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_22_4 = _mm256_add_epi32(s3_22_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_22_5 = _mm256_add_epi32(s3_22_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_17_6 = _mm256_srai_epi32(s3_17_4, DCT_CONST_BITS);
                        const __m256i s3_17_7 = _mm256_srai_epi32(s3_17_5, DCT_CONST_BITS);
                        const __m256i s3_18_6 = _mm256_srai_epi32(s3_18_4, DCT_CONST_BITS);
                        const __m256i s3_18_7 = _mm256_srai_epi32(s3_18_5, DCT_CONST_BITS);
                        const __m256i s3_21_6 = _mm256_srai_epi32(s3_21_4, DCT_CONST_BITS);
                        const __m256i s3_21_7 = _mm256_srai_epi32(s3_21_5, DCT_CONST_BITS);
                        const __m256i s3_22_6 = _mm256_srai_epi32(s3_22_4, DCT_CONST_BITS);
                        const __m256i s3_22_7 = _mm256_srai_epi32(s3_22_5, DCT_CONST_BITS);
                        const __m256i s3_25_4 = _mm256_add_epi32(s3_25_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_25_5 = _mm256_add_epi32(s3_25_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_26_4 = _mm256_add_epi32(s3_26_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_26_5 = _mm256_add_epi32(s3_26_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_29_4 = _mm256_add_epi32(s3_29_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_29_5 = _mm256_add_epi32(s3_29_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_30_4 = _mm256_add_epi32(s3_30_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_30_5 = _mm256_add_epi32(s3_30_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_25_6 = _mm256_srai_epi32(s3_25_4, DCT_CONST_BITS);
                        const __m256i s3_25_7 = _mm256_srai_epi32(s3_25_5, DCT_CONST_BITS);
                        const __m256i s3_26_6 = _mm256_srai_epi32(s3_26_4, DCT_CONST_BITS);
                        const __m256i s3_26_7 = _mm256_srai_epi32(s3_26_5, DCT_CONST_BITS);
                        const __m256i s3_29_6 = _mm256_srai_epi32(s3_29_4, DCT_CONST_BITS);
                        const __m256i s3_29_7 = _mm256_srai_epi32(s3_29_5, DCT_CONST_BITS);
                        const __m256i s3_30_6 = _mm256_srai_epi32(s3_30_4, DCT_CONST_BITS);
                        const __m256i s3_30_7 = _mm256_srai_epi32(s3_30_5, DCT_CONST_BITS);
                        // Combine
                        step3[17] = _mm256_packs_epi32(s3_17_6, s3_17_7);
                        step3[18] = _mm256_packs_epi32(s3_18_6, s3_18_7);
                        step3[21] = _mm256_packs_epi32(s3_21_6, s3_21_7);
                        step3[22] = _mm256_packs_epi32(s3_22_6, s3_22_7);
                        // Combine
                        step3[25] = _mm256_packs_epi32(s3_25_6, s3_25_7);
                        step3[26] = _mm256_packs_epi32(s3_26_6, s3_26_7);
                        step3[29] = _mm256_packs_epi32(s3_29_6, s3_29_7);
                        step3[30] = _mm256_packs_epi32(s3_30_6, s3_30_7);
                    }
                    // Stage 7
                    {
                        const __m256i out_02_0 = _mm256_unpacklo_epi16(step3[8], step3[15]);
                        const __m256i out_02_1 = _mm256_unpackhi_epi16(step3[8], step3[15]);
                        const __m256i out_18_0 = _mm256_unpacklo_epi16(step3[9], step3[14]);
                        const __m256i out_18_1 = _mm256_unpackhi_epi16(step3[9], step3[14]);
                        const __m256i out_10_0 = _mm256_unpacklo_epi16(step3[10], step3[13]);
                        const __m256i out_10_1 = _mm256_unpackhi_epi16(step3[10], step3[13]);
                        const __m256i out_26_0 = _mm256_unpacklo_epi16(step3[11], step3[12]);
                        const __m256i out_26_1 = _mm256_unpackhi_epi16(step3[11], step3[12]);
                        const __m256i out_02_2 = _mm256_madd_epi16(out_02_0, k__cospi_p30_p02);
                        const __m256i out_02_3 = _mm256_madd_epi16(out_02_1, k__cospi_p30_p02);
                        const __m256i out_18_2 = _mm256_madd_epi16(out_18_0, k__cospi_p14_p18);
                        const __m256i out_18_3 = _mm256_madd_epi16(out_18_1, k__cospi_p14_p18);
                        const __m256i out_10_2 = _mm256_madd_epi16(out_10_0, k__cospi_p22_p10);
                        const __m256i out_10_3 = _mm256_madd_epi16(out_10_1, k__cospi_p22_p10);
                        const __m256i out_26_2 = _mm256_madd_epi16(out_26_0, k__cospi_p06_p26);
                        const __m256i out_26_3 = _mm256_madd_epi16(out_26_1, k__cospi_p06_p26);
                        const __m256i out_06_2 = _mm256_madd_epi16(out_26_0, k__cospi_m26_p06);
                        const __m256i out_06_3 = _mm256_madd_epi16(out_26_1, k__cospi_m26_p06);
                        const __m256i out_22_2 = _mm256_madd_epi16(out_10_0, k__cospi_m10_p22);
                        const __m256i out_22_3 = _mm256_madd_epi16(out_10_1, k__cospi_m10_p22);
                        const __m256i out_14_2 = _mm256_madd_epi16(out_18_0, k__cospi_m18_p14);
                        const __m256i out_14_3 = _mm256_madd_epi16(out_18_1, k__cospi_m18_p14);
                        const __m256i out_30_2 = _mm256_madd_epi16(out_02_0, k__cospi_m02_p30);
                        const __m256i out_30_3 = _mm256_madd_epi16(out_02_1, k__cospi_m02_p30);
                        // dct_const_round_shift
                        const __m256i out_02_4 = _mm256_add_epi32(out_02_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_02_5 = _mm256_add_epi32(out_02_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_18_4 = _mm256_add_epi32(out_18_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_18_5 = _mm256_add_epi32(out_18_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_10_4 = _mm256_add_epi32(out_10_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_10_5 = _mm256_add_epi32(out_10_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_26_4 = _mm256_add_epi32(out_26_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_26_5 = _mm256_add_epi32(out_26_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_06_4 = _mm256_add_epi32(out_06_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_06_5 = _mm256_add_epi32(out_06_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_22_4 = _mm256_add_epi32(out_22_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_22_5 = _mm256_add_epi32(out_22_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_14_4 = _mm256_add_epi32(out_14_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_14_5 = _mm256_add_epi32(out_14_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_30_4 = _mm256_add_epi32(out_30_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_30_5 = _mm256_add_epi32(out_30_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_02_6 = _mm256_srai_epi32(out_02_4, DCT_CONST_BITS);
                        const __m256i out_02_7 = _mm256_srai_epi32(out_02_5, DCT_CONST_BITS);
                        const __m256i out_18_6 = _mm256_srai_epi32(out_18_4, DCT_CONST_BITS);
                        const __m256i out_18_7 = _mm256_srai_epi32(out_18_5, DCT_CONST_BITS);
                        const __m256i out_10_6 = _mm256_srai_epi32(out_10_4, DCT_CONST_BITS);
                        const __m256i out_10_7 = _mm256_srai_epi32(out_10_5, DCT_CONST_BITS);
                        const __m256i out_26_6 = _mm256_srai_epi32(out_26_4, DCT_CONST_BITS);
                        const __m256i out_26_7 = _mm256_srai_epi32(out_26_5, DCT_CONST_BITS);
                        const __m256i out_06_6 = _mm256_srai_epi32(out_06_4, DCT_CONST_BITS);
                        const __m256i out_06_7 = _mm256_srai_epi32(out_06_5, DCT_CONST_BITS);
                        const __m256i out_22_6 = _mm256_srai_epi32(out_22_4, DCT_CONST_BITS);
                        const __m256i out_22_7 = _mm256_srai_epi32(out_22_5, DCT_CONST_BITS);
                        const __m256i out_14_6 = _mm256_srai_epi32(out_14_4, DCT_CONST_BITS);
                        const __m256i out_14_7 = _mm256_srai_epi32(out_14_5, DCT_CONST_BITS);
                        const __m256i out_30_6 = _mm256_srai_epi32(out_30_4, DCT_CONST_BITS);
                        const __m256i out_30_7 = _mm256_srai_epi32(out_30_5, DCT_CONST_BITS);
                        // Combine
                        out[2]  = _mm256_packs_epi32(out_02_6, out_02_7);
                        out[18] = _mm256_packs_epi32(out_18_6, out_18_7);
                        out[10] = _mm256_packs_epi32(out_10_6, out_10_7);
                        out[26] = _mm256_packs_epi32(out_26_6, out_26_7);
                        out[6]  = _mm256_packs_epi32(out_06_6, out_06_7);
                        out[22] = _mm256_packs_epi32(out_22_6, out_22_7);
                        out[14] = _mm256_packs_epi32(out_14_6, out_14_7);
                        out[30] = _mm256_packs_epi32(out_30_6, out_30_7);
                    }
                    {
                        step1[16] = _mm256_add_epi16(step3[17], step2[16]);
                        step1[17] = _mm256_sub_epi16(step2[16], step3[17]);
                        step1[18] = _mm256_sub_epi16(step2[19], step3[18]);
                        step1[19] = _mm256_add_epi16(step3[18], step2[19]);
                        step1[20] = _mm256_add_epi16(step3[21], step2[20]);
                        step1[21] = _mm256_sub_epi16(step2[20], step3[21]);
                        step1[22] = _mm256_sub_epi16(step2[23], step3[22]);
                        step1[23] = _mm256_add_epi16(step3[22], step2[23]);
                        step1[24] = _mm256_add_epi16(step3[25], step2[24]);
                        step1[25] = _mm256_sub_epi16(step2[24], step3[25]);
                        step1[26] = _mm256_sub_epi16(step2[27], step3[26]);
                        step1[27] = _mm256_add_epi16(step3[26], step2[27]);
                        step1[28] = _mm256_add_epi16(step3[29], step2[28]);
                        step1[29] = _mm256_sub_epi16(step2[28], step3[29]);
                        step1[30] = _mm256_sub_epi16(step2[31], step3[30]);
                        step1[31] = _mm256_add_epi16(step3[30], step2[31]);
                    }
                    // Final stage --- outputs indices are bit-reversed.
                    {
                        const __m256i out_01_0 = _mm256_unpacklo_epi16(step1[16], step1[31]);
                        const __m256i out_01_1 = _mm256_unpackhi_epi16(step1[16], step1[31]);
                        const __m256i out_17_0 = _mm256_unpacklo_epi16(step1[17], step1[30]);
                        const __m256i out_17_1 = _mm256_unpackhi_epi16(step1[17], step1[30]);
                        const __m256i out_09_0 = _mm256_unpacklo_epi16(step1[18], step1[29]);
                        const __m256i out_09_1 = _mm256_unpackhi_epi16(step1[18], step1[29]);
                        const __m256i out_25_0 = _mm256_unpacklo_epi16(step1[19], step1[28]);
                        const __m256i out_25_1 = _mm256_unpackhi_epi16(step1[19], step1[28]);
                        const __m256i out_01_2 = _mm256_madd_epi16(out_01_0, k__cospi_p31_p01);
                        const __m256i out_01_3 = _mm256_madd_epi16(out_01_1, k__cospi_p31_p01);
                        const __m256i out_17_2 = _mm256_madd_epi16(out_17_0, k__cospi_p15_p17);
                        const __m256i out_17_3 = _mm256_madd_epi16(out_17_1, k__cospi_p15_p17);
                        const __m256i out_09_2 = _mm256_madd_epi16(out_09_0, k__cospi_p23_p09);
                        const __m256i out_09_3 = _mm256_madd_epi16(out_09_1, k__cospi_p23_p09);
                        const __m256i out_25_2 = _mm256_madd_epi16(out_25_0, k__cospi_p07_p25);
                        const __m256i out_25_3 = _mm256_madd_epi16(out_25_1, k__cospi_p07_p25);
                        const __m256i out_07_2 = _mm256_madd_epi16(out_25_0, k__cospi_m25_p07);
                        const __m256i out_07_3 = _mm256_madd_epi16(out_25_1, k__cospi_m25_p07);
                        const __m256i out_23_2 = _mm256_madd_epi16(out_09_0, k__cospi_m09_p23);
                        const __m256i out_23_3 = _mm256_madd_epi16(out_09_1, k__cospi_m09_p23);
                        const __m256i out_15_2 = _mm256_madd_epi16(out_17_0, k__cospi_m17_p15);
                        const __m256i out_15_3 = _mm256_madd_epi16(out_17_1, k__cospi_m17_p15);
                        const __m256i out_31_2 = _mm256_madd_epi16(out_01_0, k__cospi_m01_p31);
                        const __m256i out_31_3 = _mm256_madd_epi16(out_01_1, k__cospi_m01_p31);
                        // dct_const_round_shift
                        const __m256i out_01_4 = _mm256_add_epi32(out_01_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_01_5 = _mm256_add_epi32(out_01_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_17_4 = _mm256_add_epi32(out_17_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_17_5 = _mm256_add_epi32(out_17_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_09_4 = _mm256_add_epi32(out_09_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_09_5 = _mm256_add_epi32(out_09_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_25_4 = _mm256_add_epi32(out_25_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_25_5 = _mm256_add_epi32(out_25_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_07_4 = _mm256_add_epi32(out_07_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_07_5 = _mm256_add_epi32(out_07_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_23_4 = _mm256_add_epi32(out_23_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_23_5 = _mm256_add_epi32(out_23_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_15_4 = _mm256_add_epi32(out_15_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_15_5 = _mm256_add_epi32(out_15_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_31_4 = _mm256_add_epi32(out_31_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_31_5 = _mm256_add_epi32(out_31_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_01_6 = _mm256_srai_epi32(out_01_4, DCT_CONST_BITS);
                        const __m256i out_01_7 = _mm256_srai_epi32(out_01_5, DCT_CONST_BITS);
                        const __m256i out_17_6 = _mm256_srai_epi32(out_17_4, DCT_CONST_BITS);
                        const __m256i out_17_7 = _mm256_srai_epi32(out_17_5, DCT_CONST_BITS);
                        const __m256i out_09_6 = _mm256_srai_epi32(out_09_4, DCT_CONST_BITS);
                        const __m256i out_09_7 = _mm256_srai_epi32(out_09_5, DCT_CONST_BITS);
                        const __m256i out_25_6 = _mm256_srai_epi32(out_25_4, DCT_CONST_BITS);
                        const __m256i out_25_7 = _mm256_srai_epi32(out_25_5, DCT_CONST_BITS);
                        const __m256i out_07_6 = _mm256_srai_epi32(out_07_4, DCT_CONST_BITS);
                        const __m256i out_07_7 = _mm256_srai_epi32(out_07_5, DCT_CONST_BITS);
                        const __m256i out_23_6 = _mm256_srai_epi32(out_23_4, DCT_CONST_BITS);
                        const __m256i out_23_7 = _mm256_srai_epi32(out_23_5, DCT_CONST_BITS);
                        const __m256i out_15_6 = _mm256_srai_epi32(out_15_4, DCT_CONST_BITS);
                        const __m256i out_15_7 = _mm256_srai_epi32(out_15_5, DCT_CONST_BITS);
                        const __m256i out_31_6 = _mm256_srai_epi32(out_31_4, DCT_CONST_BITS);
                        const __m256i out_31_7 = _mm256_srai_epi32(out_31_5, DCT_CONST_BITS);
                        // Combine
                        out[1]  = _mm256_packs_epi32(out_01_6, out_01_7);
                        out[17] = _mm256_packs_epi32(out_17_6, out_17_7);
                        out[9]  = _mm256_packs_epi32(out_09_6, out_09_7);
                        out[25] = _mm256_packs_epi32(out_25_6, out_25_7);
                        out[7]  = _mm256_packs_epi32(out_07_6, out_07_7);
                        out[23] = _mm256_packs_epi32(out_23_6, out_23_7);
                        out[15] = _mm256_packs_epi32(out_15_6, out_15_7);
                        out[31] = _mm256_packs_epi32(out_31_6, out_31_7);
                    }
                    {
                        const __m256i out_05_0 = _mm256_unpacklo_epi16(step1[20], step1[27]);
                        const __m256i out_05_1 = _mm256_unpackhi_epi16(step1[20], step1[27]);
                        const __m256i out_21_0 = _mm256_unpacklo_epi16(step1[21], step1[26]);
                        const __m256i out_21_1 = _mm256_unpackhi_epi16(step1[21], step1[26]);
                        const __m256i out_13_0 = _mm256_unpacklo_epi16(step1[22], step1[25]);
                        const __m256i out_13_1 = _mm256_unpackhi_epi16(step1[22], step1[25]);
                        const __m256i out_29_0 = _mm256_unpacklo_epi16(step1[23], step1[24]);
                        const __m256i out_29_1 = _mm256_unpackhi_epi16(step1[23], step1[24]);
                        const __m256i out_05_2 = _mm256_madd_epi16(out_05_0, k__cospi_p27_p05);
                        const __m256i out_05_3 = _mm256_madd_epi16(out_05_1, k__cospi_p27_p05);
                        const __m256i out_21_2 = _mm256_madd_epi16(out_21_0, k__cospi_p11_p21);
                        const __m256i out_21_3 = _mm256_madd_epi16(out_21_1, k__cospi_p11_p21);
                        const __m256i out_13_2 = _mm256_madd_epi16(out_13_0, k__cospi_p19_p13);
                        const __m256i out_13_3 = _mm256_madd_epi16(out_13_1, k__cospi_p19_p13);
                        const __m256i out_29_2 = _mm256_madd_epi16(out_29_0, k__cospi_p03_p29);
                        const __m256i out_29_3 = _mm256_madd_epi16(out_29_1, k__cospi_p03_p29);
                        const __m256i out_03_2 = _mm256_madd_epi16(out_29_0, k__cospi_m29_p03);
                        const __m256i out_03_3 = _mm256_madd_epi16(out_29_1, k__cospi_m29_p03);
                        const __m256i out_19_2 = _mm256_madd_epi16(out_13_0, k__cospi_m13_p19);
                        const __m256i out_19_3 = _mm256_madd_epi16(out_13_1, k__cospi_m13_p19);
                        const __m256i out_11_2 = _mm256_madd_epi16(out_21_0, k__cospi_m21_p11);
                        const __m256i out_11_3 = _mm256_madd_epi16(out_21_1, k__cospi_m21_p11);
                        const __m256i out_27_2 = _mm256_madd_epi16(out_05_0, k__cospi_m05_p27);
                        const __m256i out_27_3 = _mm256_madd_epi16(out_05_1, k__cospi_m05_p27);
                        // dct_const_round_shift
                        const __m256i out_05_4 = _mm256_add_epi32(out_05_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_05_5 = _mm256_add_epi32(out_05_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_21_4 = _mm256_add_epi32(out_21_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_21_5 = _mm256_add_epi32(out_21_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_13_4 = _mm256_add_epi32(out_13_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_13_5 = _mm256_add_epi32(out_13_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_29_4 = _mm256_add_epi32(out_29_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_29_5 = _mm256_add_epi32(out_29_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_03_4 = _mm256_add_epi32(out_03_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_03_5 = _mm256_add_epi32(out_03_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_19_4 = _mm256_add_epi32(out_19_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_19_5 = _mm256_add_epi32(out_19_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_11_4 = _mm256_add_epi32(out_11_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_11_5 = _mm256_add_epi32(out_11_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_27_4 = _mm256_add_epi32(out_27_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_27_5 = _mm256_add_epi32(out_27_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_05_6 = _mm256_srai_epi32(out_05_4, DCT_CONST_BITS);
                        const __m256i out_05_7 = _mm256_srai_epi32(out_05_5, DCT_CONST_BITS);
                        const __m256i out_21_6 = _mm256_srai_epi32(out_21_4, DCT_CONST_BITS);
                        const __m256i out_21_7 = _mm256_srai_epi32(out_21_5, DCT_CONST_BITS);
                        const __m256i out_13_6 = _mm256_srai_epi32(out_13_4, DCT_CONST_BITS);
                        const __m256i out_13_7 = _mm256_srai_epi32(out_13_5, DCT_CONST_BITS);
                        const __m256i out_29_6 = _mm256_srai_epi32(out_29_4, DCT_CONST_BITS);
                        const __m256i out_29_7 = _mm256_srai_epi32(out_29_5, DCT_CONST_BITS);
                        const __m256i out_03_6 = _mm256_srai_epi32(out_03_4, DCT_CONST_BITS);
                        const __m256i out_03_7 = _mm256_srai_epi32(out_03_5, DCT_CONST_BITS);
                        const __m256i out_19_6 = _mm256_srai_epi32(out_19_4, DCT_CONST_BITS);
                        const __m256i out_19_7 = _mm256_srai_epi32(out_19_5, DCT_CONST_BITS);
                        const __m256i out_11_6 = _mm256_srai_epi32(out_11_4, DCT_CONST_BITS);
                        const __m256i out_11_7 = _mm256_srai_epi32(out_11_5, DCT_CONST_BITS);
                        const __m256i out_27_6 = _mm256_srai_epi32(out_27_4, DCT_CONST_BITS);
                        const __m256i out_27_7 = _mm256_srai_epi32(out_27_5, DCT_CONST_BITS);
                        // Combine
                        out[5]  = _mm256_packs_epi32(out_05_6, out_05_7);
                        out[21] = _mm256_packs_epi32(out_21_6, out_21_7);
                        out[13] = _mm256_packs_epi32(out_13_6, out_13_7);
                        out[29] = _mm256_packs_epi32(out_29_6, out_29_7);
                        out[3]  = _mm256_packs_epi32(out_03_6, out_03_7);
                        out[19] = _mm256_packs_epi32(out_19_6, out_19_7);
                        out[11] = _mm256_packs_epi32(out_11_6, out_11_7);
                        out[27] = _mm256_packs_epi32(out_27_6, out_27_7);
                    }
                } else {
                    __m256i lstep1[64], lstep2[64], lstep3[64];
                    __m256i u[32], v[32], sign[16];
                    const __m256i K32One = _mm256_set1_epi32(1);
                    // start using 32-bit operations
                    // stage 3
                    {
                        // expanding to 32-bit length priori to addition operations
                        lstep2[0]  = _mm256_unpacklo_epi16(step2[0], kZero);
                        lstep2[1]  = _mm256_unpackhi_epi16(step2[0], kZero);
                        lstep2[2]  = _mm256_unpacklo_epi16(step2[1], kZero);
                        lstep2[3]  = _mm256_unpackhi_epi16(step2[1], kZero);
                        lstep2[4]  = _mm256_unpacklo_epi16(step2[2], kZero);
                        lstep2[5]  = _mm256_unpackhi_epi16(step2[2], kZero);
                        lstep2[6]  = _mm256_unpacklo_epi16(step2[3], kZero);
                        lstep2[7]  = _mm256_unpackhi_epi16(step2[3], kZero);
                        lstep2[8]  = _mm256_unpacklo_epi16(step2[4], kZero);
                        lstep2[9]  = _mm256_unpackhi_epi16(step2[4], kZero);
                        lstep2[10] = _mm256_unpacklo_epi16(step2[5], kZero);
                        lstep2[11] = _mm256_unpackhi_epi16(step2[5], kZero);
                        lstep2[12] = _mm256_unpacklo_epi16(step2[6], kZero);
                        lstep2[13] = _mm256_unpackhi_epi16(step2[6], kZero);
                        lstep2[14] = _mm256_unpacklo_epi16(step2[7], kZero);
                        lstep2[15] = _mm256_unpackhi_epi16(step2[7], kZero);
                        lstep2[0]  = _mm256_madd_epi16(lstep2[0], kOne);
                        lstep2[1]  = _mm256_madd_epi16(lstep2[1], kOne);
                        lstep2[2]  = _mm256_madd_epi16(lstep2[2], kOne);
                        lstep2[3]  = _mm256_madd_epi16(lstep2[3], kOne);
                        lstep2[4]  = _mm256_madd_epi16(lstep2[4], kOne);
                        lstep2[5]  = _mm256_madd_epi16(lstep2[5], kOne);
                        lstep2[6]  = _mm256_madd_epi16(lstep2[6], kOne);
                        lstep2[7]  = _mm256_madd_epi16(lstep2[7], kOne);
                        lstep2[8]  = _mm256_madd_epi16(lstep2[8], kOne);
                        lstep2[9]  = _mm256_madd_epi16(lstep2[9], kOne);
                        lstep2[10] = _mm256_madd_epi16(lstep2[10], kOne);
                        lstep2[11] = _mm256_madd_epi16(lstep2[11], kOne);
                        lstep2[12] = _mm256_madd_epi16(lstep2[12], kOne);
                        lstep2[13] = _mm256_madd_epi16(lstep2[13], kOne);
                        lstep2[14] = _mm256_madd_epi16(lstep2[14], kOne);
                        lstep2[15] = _mm256_madd_epi16(lstep2[15], kOne);

                        lstep3[0]  = _mm256_add_epi32(lstep2[14], lstep2[0]);
                        lstep3[1]  = _mm256_add_epi32(lstep2[15], lstep2[1]);
                        lstep3[2]  = _mm256_add_epi32(lstep2[12], lstep2[2]);
                        lstep3[3]  = _mm256_add_epi32(lstep2[13], lstep2[3]);
                        lstep3[4]  = _mm256_add_epi32(lstep2[10], lstep2[4]);
                        lstep3[5]  = _mm256_add_epi32(lstep2[11], lstep2[5]);
                        lstep3[6]  = _mm256_add_epi32(lstep2[8], lstep2[6]);
                        lstep3[7]  = _mm256_add_epi32(lstep2[9], lstep2[7]);
                        lstep3[8]  = _mm256_sub_epi32(lstep2[6], lstep2[8]);
                        lstep3[9]  = _mm256_sub_epi32(lstep2[7], lstep2[9]);
                        lstep3[10] = _mm256_sub_epi32(lstep2[4], lstep2[10]);
                        lstep3[11] = _mm256_sub_epi32(lstep2[5], lstep2[11]);
                        lstep3[12] = _mm256_sub_epi32(lstep2[2], lstep2[12]);
                        lstep3[13] = _mm256_sub_epi32(lstep2[3], lstep2[13]);
                        lstep3[14] = _mm256_sub_epi32(lstep2[0], lstep2[14]);
                        lstep3[15] = _mm256_sub_epi32(lstep2[1], lstep2[15]);
                    }
                    {
                        const __m256i s3_10_0 = _mm256_unpacklo_epi16(step2[13], step2[10]);
                        const __m256i s3_10_1 = _mm256_unpackhi_epi16(step2[13], step2[10]);
                        const __m256i s3_11_0 = _mm256_unpacklo_epi16(step2[12], step2[11]);
                        const __m256i s3_11_1 = _mm256_unpackhi_epi16(step2[12], step2[11]);
                        const __m256i s3_10_2 = _mm256_madd_epi16(s3_10_0, k__cospi_p16_m16);
                        const __m256i s3_10_3 = _mm256_madd_epi16(s3_10_1, k__cospi_p16_m16);
                        const __m256i s3_11_2 = _mm256_madd_epi16(s3_11_0, k__cospi_p16_m16);
                        const __m256i s3_11_3 = _mm256_madd_epi16(s3_11_1, k__cospi_p16_m16);
                        const __m256i s3_12_2 = _mm256_madd_epi16(s3_11_0, k__cospi_p16_p16);
                        const __m256i s3_12_3 = _mm256_madd_epi16(s3_11_1, k__cospi_p16_p16);
                        const __m256i s3_13_2 = _mm256_madd_epi16(s3_10_0, k__cospi_p16_p16);
                        const __m256i s3_13_3 = _mm256_madd_epi16(s3_10_1, k__cospi_p16_p16);
                        // dct_const_round_shift
                        const __m256i s3_10_4 = _mm256_add_epi32(s3_10_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_10_5 = _mm256_add_epi32(s3_10_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_11_4 = _mm256_add_epi32(s3_11_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_11_5 = _mm256_add_epi32(s3_11_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_12_4 = _mm256_add_epi32(s3_12_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_12_5 = _mm256_add_epi32(s3_12_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_13_4 = _mm256_add_epi32(s3_13_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_13_5 = _mm256_add_epi32(s3_13_3, k__DCT_CONST_ROUNDING);
                        lstep3[20] = _mm256_srai_epi32(s3_10_4, DCT_CONST_BITS);
                        lstep3[21] = _mm256_srai_epi32(s3_10_5, DCT_CONST_BITS);
                        lstep3[22] = _mm256_srai_epi32(s3_11_4, DCT_CONST_BITS);
                        lstep3[23] = _mm256_srai_epi32(s3_11_5, DCT_CONST_BITS);
                        lstep3[24] = _mm256_srai_epi32(s3_12_4, DCT_CONST_BITS);
                        lstep3[25] = _mm256_srai_epi32(s3_12_5, DCT_CONST_BITS);
                        lstep3[26] = _mm256_srai_epi32(s3_13_4, DCT_CONST_BITS);
                        lstep3[27] = _mm256_srai_epi32(s3_13_5, DCT_CONST_BITS);
                    }
                    {
                        lstep2[40] = _mm256_unpacklo_epi16(step2[20], kZero);
                        lstep2[41] = _mm256_unpackhi_epi16(step2[20], kZero);
                        lstep2[42] = _mm256_unpacklo_epi16(step2[21], kZero);
                        lstep2[43] = _mm256_unpackhi_epi16(step2[21], kZero);
                        lstep2[44] = _mm256_unpacklo_epi16(step2[22], kZero);
                        lstep2[45] = _mm256_unpackhi_epi16(step2[22], kZero);
                        lstep2[46] = _mm256_unpacklo_epi16(step2[23], kZero);
                        lstep2[47] = _mm256_unpackhi_epi16(step2[23], kZero);
                        lstep2[48] = _mm256_unpacklo_epi16(step2[24], kZero);
                        lstep2[49] = _mm256_unpackhi_epi16(step2[24], kZero);
                        lstep2[50] = _mm256_unpacklo_epi16(step2[25], kZero);
                        lstep2[51] = _mm256_unpackhi_epi16(step2[25], kZero);
                        lstep2[52] = _mm256_unpacklo_epi16(step2[26], kZero);
                        lstep2[53] = _mm256_unpackhi_epi16(step2[26], kZero);
                        lstep2[54] = _mm256_unpacklo_epi16(step2[27], kZero);
                        lstep2[55] = _mm256_unpackhi_epi16(step2[27], kZero);
                        lstep2[40] = _mm256_madd_epi16(lstep2[40], kOne);
                        lstep2[41] = _mm256_madd_epi16(lstep2[41], kOne);
                        lstep2[42] = _mm256_madd_epi16(lstep2[42], kOne);
                        lstep2[43] = _mm256_madd_epi16(lstep2[43], kOne);
                        lstep2[44] = _mm256_madd_epi16(lstep2[44], kOne);
                        lstep2[45] = _mm256_madd_epi16(lstep2[45], kOne);
                        lstep2[46] = _mm256_madd_epi16(lstep2[46], kOne);
                        lstep2[47] = _mm256_madd_epi16(lstep2[47], kOne);
                        lstep2[48] = _mm256_madd_epi16(lstep2[48], kOne);
                        lstep2[49] = _mm256_madd_epi16(lstep2[49], kOne);
                        lstep2[50] = _mm256_madd_epi16(lstep2[50], kOne);
                        lstep2[51] = _mm256_madd_epi16(lstep2[51], kOne);
                        lstep2[52] = _mm256_madd_epi16(lstep2[52], kOne);
                        lstep2[53] = _mm256_madd_epi16(lstep2[53], kOne);
                        lstep2[54] = _mm256_madd_epi16(lstep2[54], kOne);
                        lstep2[55] = _mm256_madd_epi16(lstep2[55], kOne);

                        lstep1[32] = _mm256_unpacklo_epi16(step1[16], kZero);
                        lstep1[33] = _mm256_unpackhi_epi16(step1[16], kZero);
                        lstep1[34] = _mm256_unpacklo_epi16(step1[17], kZero);
                        lstep1[35] = _mm256_unpackhi_epi16(step1[17], kZero);
                        lstep1[36] = _mm256_unpacklo_epi16(step1[18], kZero);
                        lstep1[37] = _mm256_unpackhi_epi16(step1[18], kZero);
                        lstep1[38] = _mm256_unpacklo_epi16(step1[19], kZero);
                        lstep1[39] = _mm256_unpackhi_epi16(step1[19], kZero);
                        lstep1[56] = _mm256_unpacklo_epi16(step1[28], kZero);
                        lstep1[57] = _mm256_unpackhi_epi16(step1[28], kZero);
                        lstep1[58] = _mm256_unpacklo_epi16(step1[29], kZero);
                        lstep1[59] = _mm256_unpackhi_epi16(step1[29], kZero);
                        lstep1[60] = _mm256_unpacklo_epi16(step1[30], kZero);
                        lstep1[61] = _mm256_unpackhi_epi16(step1[30], kZero);
                        lstep1[62] = _mm256_unpacklo_epi16(step1[31], kZero);
                        lstep1[63] = _mm256_unpackhi_epi16(step1[31], kZero);
                        lstep1[32] = _mm256_madd_epi16(lstep1[32], kOne);
                        lstep1[33] = _mm256_madd_epi16(lstep1[33], kOne);
                        lstep1[34] = _mm256_madd_epi16(lstep1[34], kOne);
                        lstep1[35] = _mm256_madd_epi16(lstep1[35], kOne);
                        lstep1[36] = _mm256_madd_epi16(lstep1[36], kOne);
                        lstep1[37] = _mm256_madd_epi16(lstep1[37], kOne);
                        lstep1[38] = _mm256_madd_epi16(lstep1[38], kOne);
                        lstep1[39] = _mm256_madd_epi16(lstep1[39], kOne);
                        lstep1[56] = _mm256_madd_epi16(lstep1[56], kOne);
                        lstep1[57] = _mm256_madd_epi16(lstep1[57], kOne);
                        lstep1[58] = _mm256_madd_epi16(lstep1[58], kOne);
                        lstep1[59] = _mm256_madd_epi16(lstep1[59], kOne);
                        lstep1[60] = _mm256_madd_epi16(lstep1[60], kOne);
                        lstep1[61] = _mm256_madd_epi16(lstep1[61], kOne);
                        lstep1[62] = _mm256_madd_epi16(lstep1[62], kOne);
                        lstep1[63] = _mm256_madd_epi16(lstep1[63], kOne);

                        lstep3[32] = _mm256_add_epi32(lstep2[46], lstep1[32]);
                        lstep3[33] = _mm256_add_epi32(lstep2[47], lstep1[33]);

                        lstep3[34] = _mm256_add_epi32(lstep2[44], lstep1[34]);
                        lstep3[35] = _mm256_add_epi32(lstep2[45], lstep1[35]);
                        lstep3[36] = _mm256_add_epi32(lstep2[42], lstep1[36]);
                        lstep3[37] = _mm256_add_epi32(lstep2[43], lstep1[37]);
                        lstep3[38] = _mm256_add_epi32(lstep2[40], lstep1[38]);
                        lstep3[39] = _mm256_add_epi32(lstep2[41], lstep1[39]);
                        lstep3[40] = _mm256_sub_epi32(lstep1[38], lstep2[40]);
                        lstep3[41] = _mm256_sub_epi32(lstep1[39], lstep2[41]);
                        lstep3[42] = _mm256_sub_epi32(lstep1[36], lstep2[42]);
                        lstep3[43] = _mm256_sub_epi32(lstep1[37], lstep2[43]);
                        lstep3[44] = _mm256_sub_epi32(lstep1[34], lstep2[44]);
                        lstep3[45] = _mm256_sub_epi32(lstep1[35], lstep2[45]);
                        lstep3[46] = _mm256_sub_epi32(lstep1[32], lstep2[46]);
                        lstep3[47] = _mm256_sub_epi32(lstep1[33], lstep2[47]);
                        lstep3[48] = _mm256_sub_epi32(lstep1[62], lstep2[48]);
                        lstep3[49] = _mm256_sub_epi32(lstep1[63], lstep2[49]);
                        lstep3[50] = _mm256_sub_epi32(lstep1[60], lstep2[50]);
                        lstep3[51] = _mm256_sub_epi32(lstep1[61], lstep2[51]);
                        lstep3[52] = _mm256_sub_epi32(lstep1[58], lstep2[52]);
                        lstep3[53] = _mm256_sub_epi32(lstep1[59], lstep2[53]);
                        lstep3[54] = _mm256_sub_epi32(lstep1[56], lstep2[54]);
                        lstep3[55] = _mm256_sub_epi32(lstep1[57], lstep2[55]);
                        lstep3[56] = _mm256_add_epi32(lstep2[54], lstep1[56]);
                        lstep3[57] = _mm256_add_epi32(lstep2[55], lstep1[57]);
                        lstep3[58] = _mm256_add_epi32(lstep2[52], lstep1[58]);
                        lstep3[59] = _mm256_add_epi32(lstep2[53], lstep1[59]);
                        lstep3[60] = _mm256_add_epi32(lstep2[50], lstep1[60]);
                        lstep3[61] = _mm256_add_epi32(lstep2[51], lstep1[61]);
                        lstep3[62] = _mm256_add_epi32(lstep2[48], lstep1[62]);
                        lstep3[63] = _mm256_add_epi32(lstep2[49], lstep1[63]);
                    }

                    // stage 4
                    {
                        // expanding to 32-bit length priori to addition operations
                        lstep2[16] = _mm256_unpacklo_epi16(step2[8], kZero);
                        lstep2[17] = _mm256_unpackhi_epi16(step2[8], kZero);
                        lstep2[18] = _mm256_unpacklo_epi16(step2[9], kZero);
                        lstep2[19] = _mm256_unpackhi_epi16(step2[9], kZero);
                        lstep2[28] = _mm256_unpacklo_epi16(step2[14], kZero);
                        lstep2[29] = _mm256_unpackhi_epi16(step2[14], kZero);
                        lstep2[30] = _mm256_unpacklo_epi16(step2[15], kZero);
                        lstep2[31] = _mm256_unpackhi_epi16(step2[15], kZero);
                        lstep2[16] = _mm256_madd_epi16(lstep2[16], kOne);
                        lstep2[17] = _mm256_madd_epi16(lstep2[17], kOne);
                        lstep2[18] = _mm256_madd_epi16(lstep2[18], kOne);
                        lstep2[19] = _mm256_madd_epi16(lstep2[19], kOne);
                        lstep2[28] = _mm256_madd_epi16(lstep2[28], kOne);
                        lstep2[29] = _mm256_madd_epi16(lstep2[29], kOne);
                        lstep2[30] = _mm256_madd_epi16(lstep2[30], kOne);
                        lstep2[31] = _mm256_madd_epi16(lstep2[31], kOne);

                        lstep1[0]  = _mm256_add_epi32(lstep3[6], lstep3[0]);
                        lstep1[1]  = _mm256_add_epi32(lstep3[7], lstep3[1]);
                        lstep1[2]  = _mm256_add_epi32(lstep3[4], lstep3[2]);
                        lstep1[3]  = _mm256_add_epi32(lstep3[5], lstep3[3]);
                        lstep1[4]  = _mm256_sub_epi32(lstep3[2], lstep3[4]);
                        lstep1[5]  = _mm256_sub_epi32(lstep3[3], lstep3[5]);
                        lstep1[6]  = _mm256_sub_epi32(lstep3[0], lstep3[6]);
                        lstep1[7]  = _mm256_sub_epi32(lstep3[1], lstep3[7]);
                        lstep1[16] = _mm256_add_epi32(lstep3[22], lstep2[16]);
                        lstep1[17] = _mm256_add_epi32(lstep3[23], lstep2[17]);
                        lstep1[18] = _mm256_add_epi32(lstep3[20], lstep2[18]);
                        lstep1[19] = _mm256_add_epi32(lstep3[21], lstep2[19]);
                        lstep1[20] = _mm256_sub_epi32(lstep2[18], lstep3[20]);
                        lstep1[21] = _mm256_sub_epi32(lstep2[19], lstep3[21]);
                        lstep1[22] = _mm256_sub_epi32(lstep2[16], lstep3[22]);
                        lstep1[23] = _mm256_sub_epi32(lstep2[17], lstep3[23]);
                        lstep1[24] = _mm256_sub_epi32(lstep2[30], lstep3[24]);
                        lstep1[25] = _mm256_sub_epi32(lstep2[31], lstep3[25]);
                        lstep1[26] = _mm256_sub_epi32(lstep2[28], lstep3[26]);
                        lstep1[27] = _mm256_sub_epi32(lstep2[29], lstep3[27]);
                        lstep1[28] = _mm256_add_epi32(lstep3[26], lstep2[28]);
                        lstep1[29] = _mm256_add_epi32(lstep3[27], lstep2[29]);
                        lstep1[30] = _mm256_add_epi32(lstep3[24], lstep2[30]);
                        lstep1[31] = _mm256_add_epi32(lstep3[25], lstep2[31]);
                    }
                    {
                        // to be continued...
                        //
                        const __m256i k32_p16_p16 = pair_set_epi32(cospi_16_64, cospi_16_64);
                        const __m256i k32_p16_m16 = pair_set_epi32(cospi_16_64, -cospi_16_64);

                        u[0] = _mm256_unpacklo_epi32(lstep3[12], lstep3[10]);
                        u[1] = _mm256_unpackhi_epi32(lstep3[12], lstep3[10]);
                        u[2] = _mm256_unpacklo_epi32(lstep3[13], lstep3[11]);
                        u[3] = _mm256_unpackhi_epi32(lstep3[13], lstep3[11]);

                        // TODO(jingning): manually inline k_madd_epi32_ to further hide
                        // instruction latency.
                        v[0] = k_madd_epi32(u[0], k32_p16_m16);
                        v[1] = k_madd_epi32(u[1], k32_p16_m16);
                        v[2] = k_madd_epi32(u[2], k32_p16_m16);
                        v[3] = k_madd_epi32(u[3], k32_p16_m16);
                        v[4] = k_madd_epi32(u[0], k32_p16_p16);
                        v[5] = k_madd_epi32(u[1], k32_p16_p16);
                        v[6] = k_madd_epi32(u[2], k32_p16_p16);
                        v[7] = k_madd_epi32(u[3], k32_p16_p16);
                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);

                        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);

                        lstep1[10] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                        lstep1[11] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                        lstep1[12] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                        lstep1[13] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                    }
                    {
                        const __m256i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);
                        const __m256i k32_m24_m08 = pair_set_epi32(-cospi_24_64, -cospi_8_64);
                        const __m256i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);

                        u[0]  = _mm256_unpacklo_epi32(lstep3[36], lstep3[58]);
                        u[1]  = _mm256_unpackhi_epi32(lstep3[36], lstep3[58]);
                        u[2]  = _mm256_unpacklo_epi32(lstep3[37], lstep3[59]);
                        u[3]  = _mm256_unpackhi_epi32(lstep3[37], lstep3[59]);
                        u[4]  = _mm256_unpacklo_epi32(lstep3[38], lstep3[56]);
                        u[5]  = _mm256_unpackhi_epi32(lstep3[38], lstep3[56]);
                        u[6]  = _mm256_unpacklo_epi32(lstep3[39], lstep3[57]);
                        u[7]  = _mm256_unpackhi_epi32(lstep3[39], lstep3[57]);
                        u[8]  = _mm256_unpacklo_epi32(lstep3[40], lstep3[54]);
                        u[9]  = _mm256_unpackhi_epi32(lstep3[40], lstep3[54]);
                        u[10] = _mm256_unpacklo_epi32(lstep3[41], lstep3[55]);
                        u[11] = _mm256_unpackhi_epi32(lstep3[41], lstep3[55]);
                        u[12] = _mm256_unpacklo_epi32(lstep3[42], lstep3[52]);
                        u[13] = _mm256_unpackhi_epi32(lstep3[42], lstep3[52]);
                        u[14] = _mm256_unpacklo_epi32(lstep3[43], lstep3[53]);
                        u[15] = _mm256_unpackhi_epi32(lstep3[43], lstep3[53]);

                        v[0]  = k_madd_epi32(u[0], k32_m08_p24);
                        v[1]  = k_madd_epi32(u[1], k32_m08_p24);
                        v[2]  = k_madd_epi32(u[2], k32_m08_p24);
                        v[3]  = k_madd_epi32(u[3], k32_m08_p24);
                        v[4]  = k_madd_epi32(u[4], k32_m08_p24);
                        v[5]  = k_madd_epi32(u[5], k32_m08_p24);
                        v[6]  = k_madd_epi32(u[6], k32_m08_p24);
                        v[7]  = k_madd_epi32(u[7], k32_m08_p24);
                        v[8]  = k_madd_epi32(u[8], k32_m24_m08);
                        v[9]  = k_madd_epi32(u[9], k32_m24_m08);
                        v[10] = k_madd_epi32(u[10], k32_m24_m08);
                        v[11] = k_madd_epi32(u[11], k32_m24_m08);
                        v[12] = k_madd_epi32(u[12], k32_m24_m08);
                        v[13] = k_madd_epi32(u[13], k32_m24_m08);
                        v[14] = k_madd_epi32(u[14], k32_m24_m08);
                        v[15] = k_madd_epi32(u[15], k32_m24_m08);
                        v[16] = k_madd_epi32(u[12], k32_m08_p24);
                        v[17] = k_madd_epi32(u[13], k32_m08_p24);
                        v[18] = k_madd_epi32(u[14], k32_m08_p24);
                        v[19] = k_madd_epi32(u[15], k32_m08_p24);
                        v[20] = k_madd_epi32(u[8], k32_m08_p24);
                        v[21] = k_madd_epi32(u[9], k32_m08_p24);
                        v[22] = k_madd_epi32(u[10], k32_m08_p24);
                        v[23] = k_madd_epi32(u[11], k32_m08_p24);
                        v[24] = k_madd_epi32(u[4], k32_p24_p08);
                        v[25] = k_madd_epi32(u[5], k32_p24_p08);
                        v[26] = k_madd_epi32(u[6], k32_p24_p08);
                        v[27] = k_madd_epi32(u[7], k32_p24_p08);
                        v[28] = k_madd_epi32(u[0], k32_p24_p08);
                        v[29] = k_madd_epi32(u[1], k32_p24_p08);
                        v[30] = k_madd_epi32(u[2], k32_p24_p08);
                        v[31] = k_madd_epi32(u[3], k32_p24_p08);

                        u[0]  = k_packs_epi64(v[0], v[1]);
                        u[1]  = k_packs_epi64(v[2], v[3]);
                        u[2]  = k_packs_epi64(v[4], v[5]);
                        u[3]  = k_packs_epi64(v[6], v[7]);
                        u[4]  = k_packs_epi64(v[8], v[9]);
                        u[5]  = k_packs_epi64(v[10], v[11]);
                        u[6]  = k_packs_epi64(v[12], v[13]);
                        u[7]  = k_packs_epi64(v[14], v[15]);
                        u[8]  = k_packs_epi64(v[16], v[17]);
                        u[9]  = k_packs_epi64(v[18], v[19]);
                        u[10] = k_packs_epi64(v[20], v[21]);
                        u[11] = k_packs_epi64(v[22], v[23]);
                        u[12] = k_packs_epi64(v[24], v[25]);
                        u[13] = k_packs_epi64(v[26], v[27]);
                        u[14] = k_packs_epi64(v[28], v[29]);
                        u[15] = k_packs_epi64(v[30], v[31]);

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

                        lstep1[36] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                        lstep1[37] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                        lstep1[38] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                        lstep1[39] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                        lstep1[40] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                        lstep1[41] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                        lstep1[42] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                        lstep1[43] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
                        lstep1[52] = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
                        lstep1[53] = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
                        lstep1[54] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
                        lstep1[55] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
                        lstep1[56] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
                        lstep1[57] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
                        lstep1[58] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
                        lstep1[59] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);
                    }
                    // stage 5
                    {
                        lstep2[8]  = _mm256_add_epi32(lstep1[10], lstep3[8]);
                        lstep2[9]  = _mm256_add_epi32(lstep1[11], lstep3[9]);
                        lstep2[10] = _mm256_sub_epi32(lstep3[8], lstep1[10]);
                        lstep2[11] = _mm256_sub_epi32(lstep3[9], lstep1[11]);
                        lstep2[12] = _mm256_sub_epi32(lstep3[14], lstep1[12]);
                        lstep2[13] = _mm256_sub_epi32(lstep3[15], lstep1[13]);
                        lstep2[14] = _mm256_add_epi32(lstep1[12], lstep3[14]);
                        lstep2[15] = _mm256_add_epi32(lstep1[13], lstep3[15]);
                    }
                    {
                        const __m256i k32_p16_p16 = pair_set_epi32(cospi_16_64, cospi_16_64);
                        const __m256i k32_p16_m16 = pair_set_epi32(cospi_16_64, -cospi_16_64);
                        const __m256i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);
                        const __m256i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);

                        u[0] = _mm256_unpacklo_epi32(lstep1[0], lstep1[2]);
                        u[1] = _mm256_unpackhi_epi32(lstep1[0], lstep1[2]);
                        u[2] = _mm256_unpacklo_epi32(lstep1[1], lstep1[3]);
                        u[3] = _mm256_unpackhi_epi32(lstep1[1], lstep1[3]);
                        u[4] = _mm256_unpacklo_epi32(lstep1[4], lstep1[6]);
                        u[5] = _mm256_unpackhi_epi32(lstep1[4], lstep1[6]);
                        u[6] = _mm256_unpacklo_epi32(lstep1[5], lstep1[7]);
                        u[7] = _mm256_unpackhi_epi32(lstep1[5], lstep1[7]);

                        // TODO(jingning): manually inline k_madd_epi32_ to further hide
                        // instruction latency.
                        v[0] = k_madd_epi32(u[0], k32_p16_p16);
                        v[1] = k_madd_epi32(u[1], k32_p16_p16);
                        v[2] = k_madd_epi32(u[2], k32_p16_p16);
                        v[3] = k_madd_epi32(u[3], k32_p16_p16);
                        v[4] = k_madd_epi32(u[0], k32_p16_m16);
                        v[5] = k_madd_epi32(u[1], k32_p16_m16);
                        v[6] = k_madd_epi32(u[2], k32_p16_m16);
                        v[7] = k_madd_epi32(u[3], k32_p16_m16);
                        v[8] = k_madd_epi32(u[4], k32_p24_p08);
                        v[9] = k_madd_epi32(u[5], k32_p24_p08);
                        v[10] = k_madd_epi32(u[6], k32_p24_p08);
                        v[11] = k_madd_epi32(u[7], k32_p24_p08);
                        v[12] = k_madd_epi32(u[4], k32_m08_p24);
                        v[13] = k_madd_epi32(u[5], k32_m08_p24);
                        v[14] = k_madd_epi32(u[6], k32_m08_p24);
                        v[15] = k_madd_epi32(u[7], k32_m08_p24);

                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);

                        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);

                        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);

                        sign[0] = _mm256_cmpgt_epi32(kZero, u[0]);
                        sign[1] = _mm256_cmpgt_epi32(kZero, u[1]);
                        sign[2] = _mm256_cmpgt_epi32(kZero, u[2]);
                        sign[3] = _mm256_cmpgt_epi32(kZero, u[3]);
                        sign[4] = _mm256_cmpgt_epi32(kZero, u[4]);
                        sign[5] = _mm256_cmpgt_epi32(kZero, u[5]);
                        sign[6] = _mm256_cmpgt_epi32(kZero, u[6]);
                        sign[7] = _mm256_cmpgt_epi32(kZero, u[7]);

                        u[0] = _mm256_sub_epi32(u[0], sign[0]);
                        u[1] = _mm256_sub_epi32(u[1], sign[1]);
                        u[2] = _mm256_sub_epi32(u[2], sign[2]);
                        u[3] = _mm256_sub_epi32(u[3], sign[3]);
                        u[4] = _mm256_sub_epi32(u[4], sign[4]);
                        u[5] = _mm256_sub_epi32(u[5], sign[5]);
                        u[6] = _mm256_sub_epi32(u[6], sign[6]);
                        u[7] = _mm256_sub_epi32(u[7], sign[7]);

                        u[0] = _mm256_add_epi32(u[0], K32One);
                        u[1] = _mm256_add_epi32(u[1], K32One);
                        u[2] = _mm256_add_epi32(u[2], K32One);
                        u[3] = _mm256_add_epi32(u[3], K32One);
                        u[4] = _mm256_add_epi32(u[4], K32One);
                        u[5] = _mm256_add_epi32(u[5], K32One);
                        u[6] = _mm256_add_epi32(u[6], K32One);
                        u[7] = _mm256_add_epi32(u[7], K32One);

                        u[0] = _mm256_srai_epi32(u[0], 2);
                        u[1] = _mm256_srai_epi32(u[1], 2);
                        u[2] = _mm256_srai_epi32(u[2], 2);
                        u[3] = _mm256_srai_epi32(u[3], 2);
                        u[4] = _mm256_srai_epi32(u[4], 2);
                        u[5] = _mm256_srai_epi32(u[5], 2);
                        u[6] = _mm256_srai_epi32(u[6], 2);
                        u[7] = _mm256_srai_epi32(u[7], 2);

                        // Combine
                        out[0]  = _mm256_packs_epi32(u[0], u[1]);
                        out[16] = _mm256_packs_epi32(u[2], u[3]);
                        out[8]  = _mm256_packs_epi32(u[4], u[5]);
                        out[24] = _mm256_packs_epi32(u[6], u[7]);
                    }
                    {
                        const __m256i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);
                        const __m256i k32_m24_m08 = pair_set_epi32(-cospi_24_64, -cospi_8_64);
                        const __m256i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);

                        u[0] = _mm256_unpacklo_epi32(lstep1[18], lstep1[28]);
                        u[1] = _mm256_unpackhi_epi32(lstep1[18], lstep1[28]);
                        u[2] = _mm256_unpacklo_epi32(lstep1[19], lstep1[29]);
                        u[3] = _mm256_unpackhi_epi32(lstep1[19], lstep1[29]);
                        u[4] = _mm256_unpacklo_epi32(lstep1[20], lstep1[26]);
                        u[5] = _mm256_unpackhi_epi32(lstep1[20], lstep1[26]);
                        u[6] = _mm256_unpacklo_epi32(lstep1[21], lstep1[27]);
                        u[7] = _mm256_unpackhi_epi32(lstep1[21], lstep1[27]);

                        v[0] = k_madd_epi32(u[0], k32_m08_p24);
                        v[1] = k_madd_epi32(u[1], k32_m08_p24);
                        v[2] = k_madd_epi32(u[2], k32_m08_p24);
                        v[3] = k_madd_epi32(u[3], k32_m08_p24);
                        v[4] = k_madd_epi32(u[4], k32_m24_m08);
                        v[5] = k_madd_epi32(u[5], k32_m24_m08);
                        v[6] = k_madd_epi32(u[6], k32_m24_m08);
                        v[7] = k_madd_epi32(u[7], k32_m24_m08);
                        v[8] = k_madd_epi32(u[4], k32_m08_p24);
                        v[9] = k_madd_epi32(u[5], k32_m08_p24);
                        v[10] = k_madd_epi32(u[6], k32_m08_p24);
                        v[11] = k_madd_epi32(u[7], k32_m08_p24);
                        v[12] = k_madd_epi32(u[0], k32_p24_p08);
                        v[13] = k_madd_epi32(u[1], k32_p24_p08);
                        v[14] = k_madd_epi32(u[2], k32_p24_p08);
                        v[15] = k_madd_epi32(u[3], k32_p24_p08);

                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);

                        u[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        u[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        u[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        u[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        u[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        u[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        u[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        u[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);

                        lstep2[18] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
                        lstep2[19] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
                        lstep2[20] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
                        lstep2[21] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
                        lstep2[26] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
                        lstep2[27] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
                        lstep2[28] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
                        lstep2[29] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
                    }
                    {
                        lstep2[32] = _mm256_add_epi32(lstep1[38], lstep3[32]);
                        lstep2[33] = _mm256_add_epi32(lstep1[39], lstep3[33]);
                        lstep2[34] = _mm256_add_epi32(lstep1[36], lstep3[34]);
                        lstep2[35] = _mm256_add_epi32(lstep1[37], lstep3[35]);
                        lstep2[36] = _mm256_sub_epi32(lstep3[34], lstep1[36]);
                        lstep2[37] = _mm256_sub_epi32(lstep3[35], lstep1[37]);
                        lstep2[38] = _mm256_sub_epi32(lstep3[32], lstep1[38]);
                        lstep2[39] = _mm256_sub_epi32(lstep3[33], lstep1[39]);
                        lstep2[40] = _mm256_sub_epi32(lstep3[46], lstep1[40]);
                        lstep2[41] = _mm256_sub_epi32(lstep3[47], lstep1[41]);
                        lstep2[42] = _mm256_sub_epi32(lstep3[44], lstep1[42]);
                        lstep2[43] = _mm256_sub_epi32(lstep3[45], lstep1[43]);
                        lstep2[44] = _mm256_add_epi32(lstep1[42], lstep3[44]);
                        lstep2[45] = _mm256_add_epi32(lstep1[43], lstep3[45]);
                        lstep2[46] = _mm256_add_epi32(lstep1[40], lstep3[46]);
                        lstep2[47] = _mm256_add_epi32(lstep1[41], lstep3[47]);
                        lstep2[48] = _mm256_add_epi32(lstep1[54], lstep3[48]);
                        lstep2[49] = _mm256_add_epi32(lstep1[55], lstep3[49]);
                        lstep2[50] = _mm256_add_epi32(lstep1[52], lstep3[50]);
                        lstep2[51] = _mm256_add_epi32(lstep1[53], lstep3[51]);
                        lstep2[52] = _mm256_sub_epi32(lstep3[50], lstep1[52]);
                        lstep2[53] = _mm256_sub_epi32(lstep3[51], lstep1[53]);
                        lstep2[54] = _mm256_sub_epi32(lstep3[48], lstep1[54]);
                        lstep2[55] = _mm256_sub_epi32(lstep3[49], lstep1[55]);
                        lstep2[56] = _mm256_sub_epi32(lstep3[62], lstep1[56]);
                        lstep2[57] = _mm256_sub_epi32(lstep3[63], lstep1[57]);
                        lstep2[58] = _mm256_sub_epi32(lstep3[60], lstep1[58]);
                        lstep2[59] = _mm256_sub_epi32(lstep3[61], lstep1[59]);
                        lstep2[60] = _mm256_add_epi32(lstep1[58], lstep3[60]);
                        lstep2[61] = _mm256_add_epi32(lstep1[59], lstep3[61]);
                        lstep2[62] = _mm256_add_epi32(lstep1[56], lstep3[62]);
                        lstep2[63] = _mm256_add_epi32(lstep1[57], lstep3[63]);
                    }
                    // stage 6
                    {
                        const __m256i k32_p28_p04 = pair_set_epi32(cospi_28_64, cospi_4_64);
                        const __m256i k32_p12_p20 = pair_set_epi32(cospi_12_64, cospi_20_64);
                        const __m256i k32_m20_p12 = pair_set_epi32(-cospi_20_64, cospi_12_64);
                        const __m256i k32_m04_p28 = pair_set_epi32(-cospi_4_64, cospi_28_64);

                        u[0]  = _mm256_unpacklo_epi32(lstep2[8], lstep2[14]);
                        u[1]  = _mm256_unpackhi_epi32(lstep2[8], lstep2[14]);
                        u[2]  = _mm256_unpacklo_epi32(lstep2[9], lstep2[15]);
                        u[3]  = _mm256_unpackhi_epi32(lstep2[9], lstep2[15]);
                        u[4]  = _mm256_unpacklo_epi32(lstep2[10], lstep2[12]);
                        u[5]  = _mm256_unpackhi_epi32(lstep2[10], lstep2[12]);
                        u[6]  = _mm256_unpacklo_epi32(lstep2[11], lstep2[13]);
                        u[7]  = _mm256_unpackhi_epi32(lstep2[11], lstep2[13]);
                        u[8]  = _mm256_unpacklo_epi32(lstep2[10], lstep2[12]);
                        u[9]  = _mm256_unpackhi_epi32(lstep2[10], lstep2[12]);
                        u[10] = _mm256_unpacklo_epi32(lstep2[11], lstep2[13]);
                        u[11] = _mm256_unpackhi_epi32(lstep2[11], lstep2[13]);
                        u[12] = _mm256_unpacklo_epi32(lstep2[8], lstep2[14]);
                        u[13] = _mm256_unpackhi_epi32(lstep2[8], lstep2[14]);
                        u[14] = _mm256_unpacklo_epi32(lstep2[9], lstep2[15]);
                        u[15] = _mm256_unpackhi_epi32(lstep2[9], lstep2[15]);

                        v[0] = k_madd_epi32(u[0], k32_p28_p04);
                        v[1] = k_madd_epi32(u[1], k32_p28_p04);
                        v[2] = k_madd_epi32(u[2], k32_p28_p04);
                        v[3] = k_madd_epi32(u[3], k32_p28_p04);
                        v[4] = k_madd_epi32(u[4], k32_p12_p20);
                        v[5] = k_madd_epi32(u[5], k32_p12_p20);
                        v[6] = k_madd_epi32(u[6], k32_p12_p20);
                        v[7] = k_madd_epi32(u[7], k32_p12_p20);
                        v[8] = k_madd_epi32(u[8], k32_m20_p12);
                        v[9] = k_madd_epi32(u[9], k32_m20_p12);
                        v[10] = k_madd_epi32(u[10], k32_m20_p12);
                        v[11] = k_madd_epi32(u[11], k32_m20_p12);
                        v[12] = k_madd_epi32(u[12], k32_m04_p28);
                        v[13] = k_madd_epi32(u[13], k32_m04_p28);
                        v[14] = k_madd_epi32(u[14], k32_m04_p28);
                        v[15] = k_madd_epi32(u[15], k32_m04_p28);

                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);

                        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);

                        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);

                        sign[0] = _mm256_cmpgt_epi32(kZero, u[0]);
                        sign[1] = _mm256_cmpgt_epi32(kZero, u[1]);
                        sign[2] = _mm256_cmpgt_epi32(kZero, u[2]);
                        sign[3] = _mm256_cmpgt_epi32(kZero, u[3]);
                        sign[4] = _mm256_cmpgt_epi32(kZero, u[4]);
                        sign[5] = _mm256_cmpgt_epi32(kZero, u[5]);
                        sign[6] = _mm256_cmpgt_epi32(kZero, u[6]);
                        sign[7] = _mm256_cmpgt_epi32(kZero, u[7]);

                        u[0] = _mm256_sub_epi32(u[0], sign[0]);
                        u[1] = _mm256_sub_epi32(u[1], sign[1]);
                        u[2] = _mm256_sub_epi32(u[2], sign[2]);
                        u[3] = _mm256_sub_epi32(u[3], sign[3]);
                        u[4] = _mm256_sub_epi32(u[4], sign[4]);
                        u[5] = _mm256_sub_epi32(u[5], sign[5]);
                        u[6] = _mm256_sub_epi32(u[6], sign[6]);
                        u[7] = _mm256_sub_epi32(u[7], sign[7]);

                        u[0] = _mm256_add_epi32(u[0], K32One);
                        u[1] = _mm256_add_epi32(u[1], K32One);
                        u[2] = _mm256_add_epi32(u[2], K32One);
                        u[3] = _mm256_add_epi32(u[3], K32One);
                        u[4] = _mm256_add_epi32(u[4], K32One);
                        u[5] = _mm256_add_epi32(u[5], K32One);
                        u[6] = _mm256_add_epi32(u[6], K32One);
                        u[7] = _mm256_add_epi32(u[7], K32One);

                        u[0] = _mm256_srai_epi32(u[0], 2);
                        u[1] = _mm256_srai_epi32(u[1], 2);
                        u[2] = _mm256_srai_epi32(u[2], 2);
                        u[3] = _mm256_srai_epi32(u[3], 2);
                        u[4] = _mm256_srai_epi32(u[4], 2);
                        u[5] = _mm256_srai_epi32(u[5], 2);
                        u[6] = _mm256_srai_epi32(u[6], 2);
                        u[7] = _mm256_srai_epi32(u[7], 2);

                        out[4]  = _mm256_packs_epi32(u[0], u[1]);
                        out[20] = _mm256_packs_epi32(u[2], u[3]);
                        out[12] = _mm256_packs_epi32(u[4], u[5]);
                        out[28] = _mm256_packs_epi32(u[6], u[7]);
                    }
                    {
                        lstep3[16] = _mm256_add_epi32(lstep2[18], lstep1[16]);
                        lstep3[17] = _mm256_add_epi32(lstep2[19], lstep1[17]);
                        lstep3[18] = _mm256_sub_epi32(lstep1[16], lstep2[18]);
                        lstep3[19] = _mm256_sub_epi32(lstep1[17], lstep2[19]);
                        lstep3[20] = _mm256_sub_epi32(lstep1[22], lstep2[20]);
                        lstep3[21] = _mm256_sub_epi32(lstep1[23], lstep2[21]);
                        lstep3[22] = _mm256_add_epi32(lstep2[20], lstep1[22]);
                        lstep3[23] = _mm256_add_epi32(lstep2[21], lstep1[23]);
                        lstep3[24] = _mm256_add_epi32(lstep2[26], lstep1[24]);
                        lstep3[25] = _mm256_add_epi32(lstep2[27], lstep1[25]);
                        lstep3[26] = _mm256_sub_epi32(lstep1[24], lstep2[26]);
                        lstep3[27] = _mm256_sub_epi32(lstep1[25], lstep2[27]);
                        lstep3[28] = _mm256_sub_epi32(lstep1[30], lstep2[28]);
                        lstep3[29] = _mm256_sub_epi32(lstep1[31], lstep2[29]);
                        lstep3[30] = _mm256_add_epi32(lstep2[28], lstep1[30]);
                        lstep3[31] = _mm256_add_epi32(lstep2[29], lstep1[31]);
                    }
                    {
                        const __m256i k32_m04_p28 = pair_set_epi32(-cospi_4_64, cospi_28_64);
                        const __m256i k32_m28_m04 = pair_set_epi32(-cospi_28_64, -cospi_4_64);
                        const __m256i k32_m20_p12 = pair_set_epi32(-cospi_20_64, cospi_12_64);
                        const __m256i k32_m12_m20 =
                            pair_set_epi32(-cospi_12_64, -cospi_20_64);
                        const __m256i k32_p12_p20 = pair_set_epi32(cospi_12_64, cospi_20_64);
                        const __m256i k32_p28_p04 = pair_set_epi32(cospi_28_64, cospi_4_64);

                        u[0] = _mm256_unpacklo_epi32(lstep2[34], lstep2[60]);
                        u[1] = _mm256_unpackhi_epi32(lstep2[34], lstep2[60]);
                        u[2] = _mm256_unpacklo_epi32(lstep2[35], lstep2[61]);
                        u[3] = _mm256_unpackhi_epi32(lstep2[35], lstep2[61]);
                        u[4] = _mm256_unpacklo_epi32(lstep2[36], lstep2[58]);
                        u[5] = _mm256_unpackhi_epi32(lstep2[36], lstep2[58]);
                        u[6] = _mm256_unpacklo_epi32(lstep2[37], lstep2[59]);
                        u[7] = _mm256_unpackhi_epi32(lstep2[37], lstep2[59]);
                        u[8] = _mm256_unpacklo_epi32(lstep2[42], lstep2[52]);
                        u[9] = _mm256_unpackhi_epi32(lstep2[42], lstep2[52]);
                        u[10] = _mm256_unpacklo_epi32(lstep2[43], lstep2[53]);
                        u[11] = _mm256_unpackhi_epi32(lstep2[43], lstep2[53]);
                        u[12] = _mm256_unpacklo_epi32(lstep2[44], lstep2[50]);
                        u[13] = _mm256_unpackhi_epi32(lstep2[44], lstep2[50]);
                        u[14] = _mm256_unpacklo_epi32(lstep2[45], lstep2[51]);
                        u[15] = _mm256_unpackhi_epi32(lstep2[45], lstep2[51]);

                        v[0] = k_madd_epi32(u[0], k32_m04_p28);
                        v[1] = k_madd_epi32(u[1], k32_m04_p28);
                        v[2] = k_madd_epi32(u[2], k32_m04_p28);
                        v[3] = k_madd_epi32(u[3], k32_m04_p28);
                        v[4] = k_madd_epi32(u[4], k32_m28_m04);
                        v[5] = k_madd_epi32(u[5], k32_m28_m04);
                        v[6] = k_madd_epi32(u[6], k32_m28_m04);
                        v[7] = k_madd_epi32(u[7], k32_m28_m04);
                        v[8] = k_madd_epi32(u[8], k32_m20_p12);
                        v[9] = k_madd_epi32(u[9], k32_m20_p12);
                        v[10] = k_madd_epi32(u[10], k32_m20_p12);
                        v[11] = k_madd_epi32(u[11], k32_m20_p12);
                        v[12] = k_madd_epi32(u[12], k32_m12_m20);
                        v[13] = k_madd_epi32(u[13], k32_m12_m20);
                        v[14] = k_madd_epi32(u[14], k32_m12_m20);
                        v[15] = k_madd_epi32(u[15], k32_m12_m20);
                        v[16] = k_madd_epi32(u[12], k32_m20_p12);
                        v[17] = k_madd_epi32(u[13], k32_m20_p12);
                        v[18] = k_madd_epi32(u[14], k32_m20_p12);
                        v[19] = k_madd_epi32(u[15], k32_m20_p12);
                        v[20] = k_madd_epi32(u[8], k32_p12_p20);
                        v[21] = k_madd_epi32(u[9], k32_p12_p20);
                        v[22] = k_madd_epi32(u[10], k32_p12_p20);
                        v[23] = k_madd_epi32(u[11], k32_p12_p20);
                        v[24] = k_madd_epi32(u[4], k32_m04_p28);
                        v[25] = k_madd_epi32(u[5], k32_m04_p28);
                        v[26] = k_madd_epi32(u[6], k32_m04_p28);
                        v[27] = k_madd_epi32(u[7], k32_m04_p28);
                        v[28] = k_madd_epi32(u[0], k32_p28_p04);
                        v[29] = k_madd_epi32(u[1], k32_p28_p04);
                        v[30] = k_madd_epi32(u[2], k32_p28_p04);
                        v[31] = k_madd_epi32(u[3], k32_p28_p04);

                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);
                        u[8] = k_packs_epi64(v[16], v[17]);
                        u[9] = k_packs_epi64(v[18], v[19]);
                        u[10] = k_packs_epi64(v[20], v[21]);
                        u[11] = k_packs_epi64(v[22], v[23]);
                        u[12] = k_packs_epi64(v[24], v[25]);
                        u[13] = k_packs_epi64(v[26], v[27]);
                        u[14] = k_packs_epi64(v[28], v[29]);
                        u[15] = k_packs_epi64(v[30], v[31]);

                        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                        v[8] = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                        v[9] = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                        lstep3[34] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                        lstep3[35] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                        lstep3[36] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                        lstep3[37] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                        lstep3[42] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                        lstep3[43] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                        lstep3[44] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                        lstep3[45] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
                        lstep3[50] = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
                        lstep3[51] = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
                        lstep3[52] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
                        lstep3[53] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
                        lstep3[58] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
                        lstep3[59] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
                        lstep3[60] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
                        lstep3[61] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);
                    }
                    // stage 7
                    {
                        const __m256i k32_p30_p02 = pair_set_epi32(cospi_30_64, cospi_2_64);
                        const __m256i k32_p14_p18 = pair_set_epi32(cospi_14_64, cospi_18_64);
                        const __m256i k32_p22_p10 = pair_set_epi32(cospi_22_64, cospi_10_64);
                        const __m256i k32_p06_p26 = pair_set_epi32(cospi_6_64, cospi_26_64);
                        const __m256i k32_m26_p06 = pair_set_epi32(-cospi_26_64, cospi_6_64);
                        const __m256i k32_m10_p22 = pair_set_epi32(-cospi_10_64, cospi_22_64);
                        const __m256i k32_m18_p14 = pair_set_epi32(-cospi_18_64, cospi_14_64);
                        const __m256i k32_m02_p30 = pair_set_epi32(-cospi_2_64, cospi_30_64);

                        u[0]  = _mm256_unpacklo_epi32(lstep3[16], lstep3[30]);
                        u[1]  = _mm256_unpackhi_epi32(lstep3[16], lstep3[30]);
                        u[2]  = _mm256_unpacklo_epi32(lstep3[17], lstep3[31]);
                        u[3]  = _mm256_unpackhi_epi32(lstep3[17], lstep3[31]);
                        u[4]  = _mm256_unpacklo_epi32(lstep3[18], lstep3[28]);
                        u[5]  = _mm256_unpackhi_epi32(lstep3[18], lstep3[28]);
                        u[6]  = _mm256_unpacklo_epi32(lstep3[19], lstep3[29]);
                        u[7]  = _mm256_unpackhi_epi32(lstep3[19], lstep3[29]);
                        u[8]  = _mm256_unpacklo_epi32(lstep3[20], lstep3[26]);
                        u[9]  = _mm256_unpackhi_epi32(lstep3[20], lstep3[26]);
                        u[10] = _mm256_unpacklo_epi32(lstep3[21], lstep3[27]);
                        u[11] = _mm256_unpackhi_epi32(lstep3[21], lstep3[27]);
                        u[12] = _mm256_unpacklo_epi32(lstep3[22], lstep3[24]);
                        u[13] = _mm256_unpackhi_epi32(lstep3[22], lstep3[24]);
                        u[14] = _mm256_unpacklo_epi32(lstep3[23], lstep3[25]);
                        u[15] = _mm256_unpackhi_epi32(lstep3[23], lstep3[25]);

                        v[0]  = k_madd_epi32(u[0], k32_p30_p02);
                        v[1]  = k_madd_epi32(u[1], k32_p30_p02);
                        v[2]  = k_madd_epi32(u[2], k32_p30_p02);
                        v[3]  = k_madd_epi32(u[3], k32_p30_p02);
                        v[4]  = k_madd_epi32(u[4], k32_p14_p18);
                        v[5]  = k_madd_epi32(u[5], k32_p14_p18);
                        v[6]  = k_madd_epi32(u[6], k32_p14_p18);
                        v[7]  = k_madd_epi32(u[7], k32_p14_p18);
                        v[8]  = k_madd_epi32(u[8], k32_p22_p10);
                        v[9]  = k_madd_epi32(u[9], k32_p22_p10);
                        v[10] = k_madd_epi32(u[10], k32_p22_p10);
                        v[11] = k_madd_epi32(u[11], k32_p22_p10);
                        v[12] = k_madd_epi32(u[12], k32_p06_p26);
                        v[13] = k_madd_epi32(u[13], k32_p06_p26);
                        v[14] = k_madd_epi32(u[14], k32_p06_p26);
                        v[15] = k_madd_epi32(u[15], k32_p06_p26);
                        v[16] = k_madd_epi32(u[12], k32_m26_p06);
                        v[17] = k_madd_epi32(u[13], k32_m26_p06);
                        v[18] = k_madd_epi32(u[14], k32_m26_p06);
                        v[19] = k_madd_epi32(u[15], k32_m26_p06);
                        v[20] = k_madd_epi32(u[8], k32_m10_p22);
                        v[21] = k_madd_epi32(u[9], k32_m10_p22);
                        v[22] = k_madd_epi32(u[10], k32_m10_p22);
                        v[23] = k_madd_epi32(u[11], k32_m10_p22);
                        v[24] = k_madd_epi32(u[4], k32_m18_p14);
                        v[25] = k_madd_epi32(u[5], k32_m18_p14);
                        v[26] = k_madd_epi32(u[6], k32_m18_p14);
                        v[27] = k_madd_epi32(u[7], k32_m18_p14);
                        v[28] = k_madd_epi32(u[0], k32_m02_p30);
                        v[29] = k_madd_epi32(u[1], k32_m02_p30);
                        v[30] = k_madd_epi32(u[2], k32_m02_p30);
                        v[31] = k_madd_epi32(u[3], k32_m02_p30);

                        u[0]  = k_packs_epi64(v[0], v[1]);
                        u[1]  = k_packs_epi64(v[2], v[3]);
                        u[2]  = k_packs_epi64(v[4], v[5]);
                        u[3]  = k_packs_epi64(v[6], v[7]);
                        u[4]  = k_packs_epi64(v[8], v[9]);
                        u[5]  = k_packs_epi64(v[10], v[11]);
                        u[6]  = k_packs_epi64(v[12], v[13]);
                        u[7]  = k_packs_epi64(v[14], v[15]);
                        u[8]  = k_packs_epi64(v[16], v[17]);
                        u[9]  = k_packs_epi64(v[18], v[19]);
                        u[10] = k_packs_epi64(v[20], v[21]);
                        u[11] = k_packs_epi64(v[22], v[23]);
                        u[12] = k_packs_epi64(v[24], v[25]);
                        u[13] = k_packs_epi64(v[26], v[27]);
                        u[14] = k_packs_epi64(v[28], v[29]);
                        u[15] = k_packs_epi64(v[30], v[31]);

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

                        v[0]  = _mm256_cmpgt_epi32(kZero, u[0]);
                        v[1]  = _mm256_cmpgt_epi32(kZero, u[1]);
                        v[2]  = _mm256_cmpgt_epi32(kZero, u[2]);
                        v[3]  = _mm256_cmpgt_epi32(kZero, u[3]);
                        v[4]  = _mm256_cmpgt_epi32(kZero, u[4]);
                        v[5]  = _mm256_cmpgt_epi32(kZero, u[5]);
                        v[6]  = _mm256_cmpgt_epi32(kZero, u[6]);
                        v[7]  = _mm256_cmpgt_epi32(kZero, u[7]);
                        v[8]  = _mm256_cmpgt_epi32(kZero, u[8]);
                        v[9]  = _mm256_cmpgt_epi32(kZero, u[9]);
                        v[10] = _mm256_cmpgt_epi32(kZero, u[10]);
                        v[11] = _mm256_cmpgt_epi32(kZero, u[11]);
                        v[12] = _mm256_cmpgt_epi32(kZero, u[12]);
                        v[13] = _mm256_cmpgt_epi32(kZero, u[13]);
                        v[14] = _mm256_cmpgt_epi32(kZero, u[14]);
                        v[15] = _mm256_cmpgt_epi32(kZero, u[15]);

                        u[0]  = _mm256_sub_epi32(u[0], v[0]);
                        u[1]  = _mm256_sub_epi32(u[1], v[1]);
                        u[2]  = _mm256_sub_epi32(u[2], v[2]);
                        u[3]  = _mm256_sub_epi32(u[3], v[3]);
                        u[4]  = _mm256_sub_epi32(u[4], v[4]);
                        u[5]  = _mm256_sub_epi32(u[5], v[5]);
                        u[6]  = _mm256_sub_epi32(u[6], v[6]);
                        u[7]  = _mm256_sub_epi32(u[7], v[7]);
                        u[8]  = _mm256_sub_epi32(u[8], v[8]);
                        u[9]  = _mm256_sub_epi32(u[9], v[9]);
                        u[10] = _mm256_sub_epi32(u[10], v[10]);
                        u[11] = _mm256_sub_epi32(u[11], v[11]);
                        u[12] = _mm256_sub_epi32(u[12], v[12]);
                        u[13] = _mm256_sub_epi32(u[13], v[13]);
                        u[14] = _mm256_sub_epi32(u[14], v[14]);
                        u[15] = _mm256_sub_epi32(u[15], v[15]);

                        v[0]  = _mm256_add_epi32(u[0], K32One);
                        v[1]  = _mm256_add_epi32(u[1], K32One);
                        v[2]  = _mm256_add_epi32(u[2], K32One);
                        v[3]  = _mm256_add_epi32(u[3], K32One);
                        v[4]  = _mm256_add_epi32(u[4], K32One);
                        v[5]  = _mm256_add_epi32(u[5], K32One);
                        v[6]  = _mm256_add_epi32(u[6], K32One);
                        v[7]  = _mm256_add_epi32(u[7], K32One);
                        v[8]  = _mm256_add_epi32(u[8], K32One);
                        v[9]  = _mm256_add_epi32(u[9], K32One);
                        v[10] = _mm256_add_epi32(u[10], K32One);
                        v[11] = _mm256_add_epi32(u[11], K32One);
                        v[12] = _mm256_add_epi32(u[12], K32One);
                        v[13] = _mm256_add_epi32(u[13], K32One);
                        v[14] = _mm256_add_epi32(u[14], K32One);
                        v[15] = _mm256_add_epi32(u[15], K32One);

                        u[0]  = _mm256_srai_epi32(v[0], 2);
                        u[1]  = _mm256_srai_epi32(v[1], 2);
                        u[2]  = _mm256_srai_epi32(v[2], 2);
                        u[3]  = _mm256_srai_epi32(v[3], 2);
                        u[4]  = _mm256_srai_epi32(v[4], 2);
                        u[5]  = _mm256_srai_epi32(v[5], 2);
                        u[6]  = _mm256_srai_epi32(v[6], 2);
                        u[7]  = _mm256_srai_epi32(v[7], 2);
                        u[8]  = _mm256_srai_epi32(v[8], 2);
                        u[9]  = _mm256_srai_epi32(v[9], 2);
                        u[10] = _mm256_srai_epi32(v[10], 2);
                        u[11] = _mm256_srai_epi32(v[11], 2);
                        u[12] = _mm256_srai_epi32(v[12], 2);
                        u[13] = _mm256_srai_epi32(v[13], 2);
                        u[14] = _mm256_srai_epi32(v[14], 2);
                        u[15] = _mm256_srai_epi32(v[15], 2);

                        out[2]  = _mm256_packs_epi32(u[0], u[1]);
                        out[18] = _mm256_packs_epi32(u[2], u[3]);
                        out[10] = _mm256_packs_epi32(u[4], u[5]);
                        out[26] = _mm256_packs_epi32(u[6], u[7]);
                        out[6]  = _mm256_packs_epi32(u[8], u[9]);
                        out[22] = _mm256_packs_epi32(u[10], u[11]);
                        out[14] = _mm256_packs_epi32(u[12], u[13]);
                        out[30] = _mm256_packs_epi32(u[14], u[15]);
                    }
                    {
                        lstep1[32] = _mm256_add_epi32(lstep3[34], lstep2[32]);
                        lstep1[33] = _mm256_add_epi32(lstep3[35], lstep2[33]);
                        lstep1[34] = _mm256_sub_epi32(lstep2[32], lstep3[34]);
                        lstep1[35] = _mm256_sub_epi32(lstep2[33], lstep3[35]);
                        lstep1[36] = _mm256_sub_epi32(lstep2[38], lstep3[36]);
                        lstep1[37] = _mm256_sub_epi32(lstep2[39], lstep3[37]);
                        lstep1[38] = _mm256_add_epi32(lstep3[36], lstep2[38]);
                        lstep1[39] = _mm256_add_epi32(lstep3[37], lstep2[39]);
                        lstep1[40] = _mm256_add_epi32(lstep3[42], lstep2[40]);
                        lstep1[41] = _mm256_add_epi32(lstep3[43], lstep2[41]);
                        lstep1[42] = _mm256_sub_epi32(lstep2[40], lstep3[42]);
                        lstep1[43] = _mm256_sub_epi32(lstep2[41], lstep3[43]);
                        lstep1[44] = _mm256_sub_epi32(lstep2[46], lstep3[44]);
                        lstep1[45] = _mm256_sub_epi32(lstep2[47], lstep3[45]);
                        lstep1[46] = _mm256_add_epi32(lstep3[44], lstep2[46]);
                        lstep1[47] = _mm256_add_epi32(lstep3[45], lstep2[47]);
                        lstep1[48] = _mm256_add_epi32(lstep3[50], lstep2[48]);
                        lstep1[49] = _mm256_add_epi32(lstep3[51], lstep2[49]);
                        lstep1[50] = _mm256_sub_epi32(lstep2[48], lstep3[50]);
                        lstep1[51] = _mm256_sub_epi32(lstep2[49], lstep3[51]);
                        lstep1[52] = _mm256_sub_epi32(lstep2[54], lstep3[52]);
                        lstep1[53] = _mm256_sub_epi32(lstep2[55], lstep3[53]);
                        lstep1[54] = _mm256_add_epi32(lstep3[52], lstep2[54]);
                        lstep1[55] = _mm256_add_epi32(lstep3[53], lstep2[55]);
                        lstep1[56] = _mm256_add_epi32(lstep3[58], lstep2[56]);
                        lstep1[57] = _mm256_add_epi32(lstep3[59], lstep2[57]);
                        lstep1[58] = _mm256_sub_epi32(lstep2[56], lstep3[58]);
                        lstep1[59] = _mm256_sub_epi32(lstep2[57], lstep3[59]);
                        lstep1[60] = _mm256_sub_epi32(lstep2[62], lstep3[60]);
                        lstep1[61] = _mm256_sub_epi32(lstep2[63], lstep3[61]);
                        lstep1[62] = _mm256_add_epi32(lstep3[60], lstep2[62]);
                        lstep1[63] = _mm256_add_epi32(lstep3[61], lstep2[63]);
                    }
                    // stage 8
                    {
                        const __m256i k32_p31_p01 = pair_set_epi32(cospi_31_64, cospi_1_64);
                        const __m256i k32_p15_p17 = pair_set_epi32(cospi_15_64, cospi_17_64);
                        const __m256i k32_p23_p09 = pair_set_epi32(cospi_23_64, cospi_9_64);
                        const __m256i k32_p07_p25 = pair_set_epi32(cospi_7_64, cospi_25_64);
                        const __m256i k32_m25_p07 = pair_set_epi32(-cospi_25_64, cospi_7_64);
                        const __m256i k32_m09_p23 = pair_set_epi32(-cospi_9_64, cospi_23_64);
                        const __m256i k32_m17_p15 = pair_set_epi32(-cospi_17_64, cospi_15_64);
                        const __m256i k32_m01_p31 = pair_set_epi32(-cospi_1_64, cospi_31_64);

                        u[0]  = _mm256_unpacklo_epi32(lstep1[32], lstep1[62]);
                        u[1]  = _mm256_unpackhi_epi32(lstep1[32], lstep1[62]);
                        u[2]  = _mm256_unpacklo_epi32(lstep1[33], lstep1[63]);
                        u[3]  = _mm256_unpackhi_epi32(lstep1[33], lstep1[63]);
                        u[4]  = _mm256_unpacklo_epi32(lstep1[34], lstep1[60]);
                        u[5]  = _mm256_unpackhi_epi32(lstep1[34], lstep1[60]);
                        u[6]  = _mm256_unpacklo_epi32(lstep1[35], lstep1[61]);
                        u[7]  = _mm256_unpackhi_epi32(lstep1[35], lstep1[61]);
                        u[8]  = _mm256_unpacklo_epi32(lstep1[36], lstep1[58]);
                        u[9]  = _mm256_unpackhi_epi32(lstep1[36], lstep1[58]);
                        u[10] = _mm256_unpacklo_epi32(lstep1[37], lstep1[59]);
                        u[11] = _mm256_unpackhi_epi32(lstep1[37], lstep1[59]);
                        u[12] = _mm256_unpacklo_epi32(lstep1[38], lstep1[56]);
                        u[13] = _mm256_unpackhi_epi32(lstep1[38], lstep1[56]);
                        u[14] = _mm256_unpacklo_epi32(lstep1[39], lstep1[57]);
                        u[15] = _mm256_unpackhi_epi32(lstep1[39], lstep1[57]);

                        v[0] = k_madd_epi32(u[0], k32_p31_p01);
                        v[1] = k_madd_epi32(u[1], k32_p31_p01);
                        v[2] = k_madd_epi32(u[2], k32_p31_p01);
                        v[3] = k_madd_epi32(u[3], k32_p31_p01);
                        v[4] = k_madd_epi32(u[4], k32_p15_p17);
                        v[5] = k_madd_epi32(u[5], k32_p15_p17);
                        v[6] = k_madd_epi32(u[6], k32_p15_p17);
                        v[7] = k_madd_epi32(u[7], k32_p15_p17);
                        v[8] = k_madd_epi32(u[8], k32_p23_p09);
                        v[9] = k_madd_epi32(u[9], k32_p23_p09);
                        v[10] = k_madd_epi32(u[10], k32_p23_p09);
                        v[11] = k_madd_epi32(u[11], k32_p23_p09);
                        v[12] = k_madd_epi32(u[12], k32_p07_p25);
                        v[13] = k_madd_epi32(u[13], k32_p07_p25);
                        v[14] = k_madd_epi32(u[14], k32_p07_p25);
                        v[15] = k_madd_epi32(u[15], k32_p07_p25);
                        v[16] = k_madd_epi32(u[12], k32_m25_p07);
                        v[17] = k_madd_epi32(u[13], k32_m25_p07);
                        v[18] = k_madd_epi32(u[14], k32_m25_p07);
                        v[19] = k_madd_epi32(u[15], k32_m25_p07);
                        v[20] = k_madd_epi32(u[8], k32_m09_p23);
                        v[21] = k_madd_epi32(u[9], k32_m09_p23);
                        v[22] = k_madd_epi32(u[10], k32_m09_p23);
                        v[23] = k_madd_epi32(u[11], k32_m09_p23);
                        v[24] = k_madd_epi32(u[4], k32_m17_p15);
                        v[25] = k_madd_epi32(u[5], k32_m17_p15);
                        v[26] = k_madd_epi32(u[6], k32_m17_p15);
                        v[27] = k_madd_epi32(u[7], k32_m17_p15);
                        v[28] = k_madd_epi32(u[0], k32_m01_p31);
                        v[29] = k_madd_epi32(u[1], k32_m01_p31);
                        v[30] = k_madd_epi32(u[2], k32_m01_p31);
                        v[31] = k_madd_epi32(u[3], k32_m01_p31);

                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);
                        u[8] = k_packs_epi64(v[16], v[17]);
                        u[9] = k_packs_epi64(v[18], v[19]);
                        u[10] = k_packs_epi64(v[20], v[21]);
                        u[11] = k_packs_epi64(v[22], v[23]);
                        u[12] = k_packs_epi64(v[24], v[25]);
                        u[13] = k_packs_epi64(v[26], v[27]);
                        u[14] = k_packs_epi64(v[28], v[29]);
                        u[15] = k_packs_epi64(v[30], v[31]);

                        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                        v[8] = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                        v[9] = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
                        u[8] = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
                        u[9] = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
                        u[10] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
                        u[11] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
                        u[12] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
                        u[13] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
                        u[14] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
                        u[15] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);

                        v[0]  = _mm256_cmpgt_epi32(kZero, u[0]);
                        v[1]  = _mm256_cmpgt_epi32(kZero, u[1]);
                        v[2]  = _mm256_cmpgt_epi32(kZero, u[2]);
                        v[3]  = _mm256_cmpgt_epi32(kZero, u[3]);
                        v[4]  = _mm256_cmpgt_epi32(kZero, u[4]);
                        v[5]  = _mm256_cmpgt_epi32(kZero, u[5]);
                        v[6]  = _mm256_cmpgt_epi32(kZero, u[6]);
                        v[7]  = _mm256_cmpgt_epi32(kZero, u[7]);
                        v[8]  = _mm256_cmpgt_epi32(kZero, u[8]);
                        v[9]  = _mm256_cmpgt_epi32(kZero, u[9]);
                        v[10] = _mm256_cmpgt_epi32(kZero, u[10]);
                        v[11] = _mm256_cmpgt_epi32(kZero, u[11]);
                        v[12] = _mm256_cmpgt_epi32(kZero, u[12]);
                        v[13] = _mm256_cmpgt_epi32(kZero, u[13]);
                        v[14] = _mm256_cmpgt_epi32(kZero, u[14]);
                        v[15] = _mm256_cmpgt_epi32(kZero, u[15]);

                        u[0]  = _mm256_sub_epi32(u[0], v[0]);
                        u[1]  = _mm256_sub_epi32(u[1], v[1]);
                        u[2]  = _mm256_sub_epi32(u[2], v[2]);
                        u[3]  = _mm256_sub_epi32(u[3], v[3]);
                        u[4]  = _mm256_sub_epi32(u[4], v[4]);
                        u[5]  = _mm256_sub_epi32(u[5], v[5]);
                        u[6]  = _mm256_sub_epi32(u[6], v[6]);
                        u[7]  = _mm256_sub_epi32(u[7], v[7]);
                        u[8]  = _mm256_sub_epi32(u[8], v[8]);
                        u[9]  = _mm256_sub_epi32(u[9], v[9]);
                        u[10] = _mm256_sub_epi32(u[10], v[10]);
                        u[11] = _mm256_sub_epi32(u[11], v[11]);
                        u[12] = _mm256_sub_epi32(u[12], v[12]);
                        u[13] = _mm256_sub_epi32(u[13], v[13]);
                        u[14] = _mm256_sub_epi32(u[14], v[14]);
                        u[15] = _mm256_sub_epi32(u[15], v[15]);

                        v[0]  = _mm256_add_epi32(u[0], K32One);
                        v[1]  = _mm256_add_epi32(u[1], K32One);
                        v[2]  = _mm256_add_epi32(u[2], K32One);
                        v[3]  = _mm256_add_epi32(u[3], K32One);
                        v[4]  = _mm256_add_epi32(u[4], K32One);
                        v[5]  = _mm256_add_epi32(u[5], K32One);
                        v[6]  = _mm256_add_epi32(u[6], K32One);
                        v[7]  = _mm256_add_epi32(u[7], K32One);
                        v[8]  = _mm256_add_epi32(u[8], K32One);
                        v[9]  = _mm256_add_epi32(u[9], K32One);
                        v[10] = _mm256_add_epi32(u[10], K32One);
                        v[11] = _mm256_add_epi32(u[11], K32One);
                        v[12] = _mm256_add_epi32(u[12], K32One);
                        v[13] = _mm256_add_epi32(u[13], K32One);
                        v[14] = _mm256_add_epi32(u[14], K32One);
                        v[15] = _mm256_add_epi32(u[15], K32One);

                        u[0]  = _mm256_srai_epi32(v[0], 2);
                        u[1]  = _mm256_srai_epi32(v[1], 2);
                        u[2]  = _mm256_srai_epi32(v[2], 2);
                        u[3]  = _mm256_srai_epi32(v[3], 2);
                        u[4]  = _mm256_srai_epi32(v[4], 2);
                        u[5]  = _mm256_srai_epi32(v[5], 2);
                        u[6]  = _mm256_srai_epi32(v[6], 2);
                        u[7]  = _mm256_srai_epi32(v[7], 2);
                        u[8]  = _mm256_srai_epi32(v[8], 2);
                        u[9]  = _mm256_srai_epi32(v[9], 2);
                        u[10] = _mm256_srai_epi32(v[10], 2);
                        u[11] = _mm256_srai_epi32(v[11], 2);
                        u[12] = _mm256_srai_epi32(v[12], 2);
                        u[13] = _mm256_srai_epi32(v[13], 2);
                        u[14] = _mm256_srai_epi32(v[14], 2);
                        u[15] = _mm256_srai_epi32(v[15], 2);

                        out[1]  = _mm256_packs_epi32(u[0], u[1]);
                        out[17] = _mm256_packs_epi32(u[2], u[3]);
                        out[9]  = _mm256_packs_epi32(u[4], u[5]);
                        out[25] = _mm256_packs_epi32(u[6], u[7]);
                        out[7]  = _mm256_packs_epi32(u[8], u[9]);
                        out[23] = _mm256_packs_epi32(u[10], u[11]);
                        out[15] = _mm256_packs_epi32(u[12], u[13]);
                        out[31] = _mm256_packs_epi32(u[14], u[15]);
                   }
                    {
                        const __m256i k32_p27_p05 = pair_set_epi32(cospi_27_64, cospi_5_64);
                        const __m256i k32_p11_p21 = pair_set_epi32(cospi_11_64, cospi_21_64);
                        const __m256i k32_p19_p13 = pair_set_epi32(cospi_19_64, cospi_13_64);
                        const __m256i k32_p03_p29 = pair_set_epi32(cospi_3_64, cospi_29_64);
                        const __m256i k32_m29_p03 = pair_set_epi32(-cospi_29_64, cospi_3_64);
                        const __m256i k32_m13_p19 = pair_set_epi32(-cospi_13_64, cospi_19_64);
                        const __m256i k32_m21_p11 = pair_set_epi32(-cospi_21_64, cospi_11_64);
                        const __m256i k32_m05_p27 = pair_set_epi32(-cospi_5_64, cospi_27_64);

                        u[0]  = _mm256_unpacklo_epi32(lstep1[40], lstep1[54]);
                        u[1]  = _mm256_unpackhi_epi32(lstep1[40], lstep1[54]);
                        u[2]  = _mm256_unpacklo_epi32(lstep1[41], lstep1[55]);
                        u[3]  = _mm256_unpackhi_epi32(lstep1[41], lstep1[55]);
                        u[4]  = _mm256_unpacklo_epi32(lstep1[42], lstep1[52]);
                        u[5]  = _mm256_unpackhi_epi32(lstep1[42], lstep1[52]);
                        u[6]  = _mm256_unpacklo_epi32(lstep1[43], lstep1[53]);
                        u[7]  = _mm256_unpackhi_epi32(lstep1[43], lstep1[53]);
                        u[8]  = _mm256_unpacklo_epi32(lstep1[44], lstep1[50]);
                        u[9]  = _mm256_unpackhi_epi32(lstep1[44], lstep1[50]);
                        u[10] = _mm256_unpacklo_epi32(lstep1[45], lstep1[51]);
                        u[11] = _mm256_unpackhi_epi32(lstep1[45], lstep1[51]);
                        u[12] = _mm256_unpacklo_epi32(lstep1[46], lstep1[48]);
                        u[13] = _mm256_unpackhi_epi32(lstep1[46], lstep1[48]);
                        u[14] = _mm256_unpacklo_epi32(lstep1[47], lstep1[49]);
                        u[15] = _mm256_unpackhi_epi32(lstep1[47], lstep1[49]);

                        v[0]  = k_madd_epi32(u[0], k32_p27_p05);
                        v[1]  = k_madd_epi32(u[1], k32_p27_p05);
                        v[2]  = k_madd_epi32(u[2], k32_p27_p05);
                        v[3]  = k_madd_epi32(u[3], k32_p27_p05);
                        v[4]  = k_madd_epi32(u[4], k32_p11_p21);
                        v[5]  = k_madd_epi32(u[5], k32_p11_p21);
                        v[6]  = k_madd_epi32(u[6], k32_p11_p21);
                        v[7]  = k_madd_epi32(u[7], k32_p11_p21);
                        v[8]  = k_madd_epi32(u[8], k32_p19_p13);
                        v[9] = k_madd_epi32(u[9], k32_p19_p13);
                        v[10] = k_madd_epi32(u[10], k32_p19_p13);
                        v[11] = k_madd_epi32(u[11], k32_p19_p13);
                        v[12] = k_madd_epi32(u[12], k32_p03_p29);
                        v[13] = k_madd_epi32(u[13], k32_p03_p29);
                        v[14] = k_madd_epi32(u[14], k32_p03_p29);
                        v[15] = k_madd_epi32(u[15], k32_p03_p29);
                        v[16] = k_madd_epi32(u[12], k32_m29_p03);
                        v[17] = k_madd_epi32(u[13], k32_m29_p03);
                        v[18] = k_madd_epi32(u[14], k32_m29_p03);
                        v[19] = k_madd_epi32(u[15], k32_m29_p03);
                        v[20] = k_madd_epi32(u[8], k32_m13_p19);
                        v[21] = k_madd_epi32(u[9], k32_m13_p19);
                        v[22] = k_madd_epi32(u[10], k32_m13_p19);
                        v[23] = k_madd_epi32(u[11], k32_m13_p19);
                        v[24] = k_madd_epi32(u[4], k32_m21_p11);
                        v[25] = k_madd_epi32(u[5], k32_m21_p11);
                        v[26] = k_madd_epi32(u[6], k32_m21_p11);
                        v[27] = k_madd_epi32(u[7], k32_m21_p11);
                        v[28] = k_madd_epi32(u[0], k32_m05_p27);
                        v[29] = k_madd_epi32(u[1], k32_m05_p27);
                        v[30] = k_madd_epi32(u[2], k32_m05_p27);
                        v[31] = k_madd_epi32(u[3], k32_m05_p27);

                        u[0]  = k_packs_epi64(v[0], v[1]);
                        u[1]  = k_packs_epi64(v[2], v[3]);
                        u[2]  = k_packs_epi64(v[4], v[5]);
                        u[3]  = k_packs_epi64(v[6], v[7]);
                        u[4]  = k_packs_epi64(v[8], v[9]);
                        u[5]  = k_packs_epi64(v[10], v[11]);
                        u[6]  = k_packs_epi64(v[12], v[13]);
                        u[7]  = k_packs_epi64(v[14], v[15]);
                        u[8]  = k_packs_epi64(v[16], v[17]);
                        u[9]  = k_packs_epi64(v[18], v[19]);
                        u[10] = k_packs_epi64(v[20], v[21]);
                        u[11] = k_packs_epi64(v[22], v[23]);
                        u[12] = k_packs_epi64(v[24], v[25]);
                        u[13] = k_packs_epi64(v[26], v[27]);
                        u[14] = k_packs_epi64(v[28], v[29]);
                        u[15] = k_packs_epi64(v[30], v[31]);

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

                        v[0]  = _mm256_cmpgt_epi32(kZero, u[0]);
                        v[1]  = _mm256_cmpgt_epi32(kZero, u[1]);
                        v[2]  = _mm256_cmpgt_epi32(kZero, u[2]);
                        v[3]  = _mm256_cmpgt_epi32(kZero, u[3]);
                        v[4]  = _mm256_cmpgt_epi32(kZero, u[4]);
                        v[5]  = _mm256_cmpgt_epi32(kZero, u[5]);
                        v[6]  = _mm256_cmpgt_epi32(kZero, u[6]);
                        v[7]  = _mm256_cmpgt_epi32(kZero, u[7]);
                        v[8]  = _mm256_cmpgt_epi32(kZero, u[8]);
                        v[9]  = _mm256_cmpgt_epi32(kZero, u[9]);
                        v[10] = _mm256_cmpgt_epi32(kZero, u[10]);
                        v[11] = _mm256_cmpgt_epi32(kZero, u[11]);
                        v[12] = _mm256_cmpgt_epi32(kZero, u[12]);
                        v[13] = _mm256_cmpgt_epi32(kZero, u[13]);
                        v[14] = _mm256_cmpgt_epi32(kZero, u[14]);
                        v[15] = _mm256_cmpgt_epi32(kZero, u[15]);

                        u[0] = _mm256_sub_epi32(u[0], v[0]);
                        u[1] = _mm256_sub_epi32(u[1], v[1]);
                        u[2] = _mm256_sub_epi32(u[2], v[2]);
                        u[3] = _mm256_sub_epi32(u[3], v[3]);
                        u[4] = _mm256_sub_epi32(u[4], v[4]);
                        u[5] = _mm256_sub_epi32(u[5], v[5]);
                        u[6] = _mm256_sub_epi32(u[6], v[6]);
                        u[7] = _mm256_sub_epi32(u[7], v[7]);
                        u[8] = _mm256_sub_epi32(u[8], v[8]);
                        u[9] = _mm256_sub_epi32(u[9], v[9]);
                        u[10] = _mm256_sub_epi32(u[10], v[10]);
                        u[11] = _mm256_sub_epi32(u[11], v[11]);
                        u[12] = _mm256_sub_epi32(u[12], v[12]);
                        u[13] = _mm256_sub_epi32(u[13], v[13]);
                        u[14] = _mm256_sub_epi32(u[14], v[14]);
                        u[15] = _mm256_sub_epi32(u[15], v[15]);

                        v[0] = _mm256_add_epi32(u[0], K32One);
                        v[1] = _mm256_add_epi32(u[1], K32One);
                        v[2] = _mm256_add_epi32(u[2], K32One);
                        v[3] = _mm256_add_epi32(u[3], K32One);
                        v[4] = _mm256_add_epi32(u[4], K32One);
                        v[5] = _mm256_add_epi32(u[5], K32One);
                        v[6] = _mm256_add_epi32(u[6], K32One);
                        v[7] = _mm256_add_epi32(u[7], K32One);
                        v[8] = _mm256_add_epi32(u[8], K32One);
                        v[9] = _mm256_add_epi32(u[9], K32One);
                        v[10] = _mm256_add_epi32(u[10], K32One);
                        v[11] = _mm256_add_epi32(u[11], K32One);
                        v[12] = _mm256_add_epi32(u[12], K32One);
                        v[13] = _mm256_add_epi32(u[13], K32One);
                        v[14] = _mm256_add_epi32(u[14], K32One);
                        v[15] = _mm256_add_epi32(u[15], K32One);

                        u[0] = _mm256_srai_epi32(v[0], 2);
                        u[1] = _mm256_srai_epi32(v[1], 2);
                        u[2] = _mm256_srai_epi32(v[2], 2);
                        u[3] = _mm256_srai_epi32(v[3], 2);
                        u[4] = _mm256_srai_epi32(v[4], 2);
                        u[5] = _mm256_srai_epi32(v[5], 2);
                        u[6] = _mm256_srai_epi32(v[6], 2);
                        u[7] = _mm256_srai_epi32(v[7], 2);
                        u[8] = _mm256_srai_epi32(v[8], 2);
                        u[9] = _mm256_srai_epi32(v[9], 2);
                        u[10] = _mm256_srai_epi32(v[10], 2);
                        u[11] = _mm256_srai_epi32(v[11], 2);
                        u[12] = _mm256_srai_epi32(v[12], 2);
                        u[13] = _mm256_srai_epi32(v[13], 2);
                        u[14] = _mm256_srai_epi32(v[14], 2);
                        u[15] = _mm256_srai_epi32(v[15], 2);

                        out[5] = _mm256_packs_epi32(u[0], u[1]);
                        out[21] = _mm256_packs_epi32(u[2], u[3]);
                        out[13] = _mm256_packs_epi32(u[4], u[5]);
                        out[29] = _mm256_packs_epi32(u[6], u[7]);
                        out[3] = _mm256_packs_epi32(u[8], u[9]);
                        out[19] = _mm256_packs_epi32(u[10], u[11]);
                        out[11] = _mm256_packs_epi32(u[12], u[13]);
                        out[27] = _mm256_packs_epi32(u[14], u[15]);
                    }
                }
                // Transpose the results, do it as four 8x8 transposes.
                if (pass == 0) {
                    transpose_and_output16x16<true>(out, poutput, 32);
                    transpose_and_output16x16<true>(out+16, poutput+16, 32);
                } else {
                    transpose_and_output16x16<false>(out, poutput, 32);
                    transpose_and_output16x16<false>(out + 16, poutput+16, 32);
                }
                poutput += 16 * 32;
            }
            poutput = output;
        }
    }

    static void fdct32x32_fast_avx2(const short *input, short *output, int stride)
    {
        const int str1 = stride;
        const int str2 = 2 * stride;
        const int str3 = 2 * stride + str1;

        ALIGN_DECL(32) short intermediate[32 * 32];

        const __m256i k__cospi_p16_p16 = _mm256_set1_epi16(cospi_16_64);
        const __m256i k__cospi_p16_m16 = pair_set_epi16(+cospi_16_64, -cospi_16_64);
        const __m256i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m256i k__cospi_m24_m08 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
        const __m256i k__cospi_p24_p08 = pair_set_epi16(+cospi_24_64, cospi_8_64);
        const __m256i k__cospi_p12_p20 = pair_set_epi16(+cospi_12_64, cospi_20_64);
        const __m256i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m256i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m256i k__cospi_p28_p04 = pair_set_epi16(+cospi_28_64, cospi_4_64);
        const __m256i k__cospi_m28_m04 = pair_set_epi16(-cospi_28_64, -cospi_4_64);
        const __m256i k__cospi_m12_m20 = pair_set_epi16(-cospi_12_64, -cospi_20_64);
        const __m256i k__cospi_p30_p02 = pair_set_epi16(+cospi_30_64, cospi_2_64);
        const __m256i k__cospi_p14_p18 = pair_set_epi16(+cospi_14_64, cospi_18_64);
        const __m256i k__cospi_p22_p10 = pair_set_epi16(+cospi_22_64, cospi_10_64);
        const __m256i k__cospi_p06_p26 = pair_set_epi16(+cospi_6_64, cospi_26_64);
        const __m256i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
        const __m256i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
        const __m256i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
        const __m256i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
        const __m256i k__cospi_p31_p01 = pair_set_epi16(+cospi_31_64, cospi_1_64);
        const __m256i k__cospi_p15_p17 = pair_set_epi16(+cospi_15_64, cospi_17_64);
        const __m256i k__cospi_p23_p09 = pair_set_epi16(+cospi_23_64, cospi_9_64);
        const __m256i k__cospi_p07_p25 = pair_set_epi16(+cospi_7_64, cospi_25_64);
        const __m256i k__cospi_m25_p07 = pair_set_epi16(-cospi_25_64, cospi_7_64);
        const __m256i k__cospi_m09_p23 = pair_set_epi16(-cospi_9_64, cospi_23_64);
        const __m256i k__cospi_m17_p15 = pair_set_epi16(-cospi_17_64, cospi_15_64);
        const __m256i k__cospi_m01_p31 = pair_set_epi16(-cospi_1_64, cospi_31_64);
        const __m256i k__cospi_p27_p05 = pair_set_epi16(+cospi_27_64, cospi_5_64);
        const __m256i k__cospi_p11_p21 = pair_set_epi16(+cospi_11_64, cospi_21_64);
        const __m256i k__cospi_p19_p13 = pair_set_epi16(+cospi_19_64, cospi_13_64);
        const __m256i k__cospi_p03_p29 = pair_set_epi16(+cospi_3_64, cospi_29_64);
        const __m256i k__cospi_m29_p03 = pair_set_epi16(-cospi_29_64, cospi_3_64);
        const __m256i k__cospi_m13_p19 = pair_set_epi16(-cospi_13_64, cospi_19_64);
        const __m256i k__cospi_m21_p11 = pair_set_epi16(-cospi_21_64, cospi_11_64);
        const __m256i k__cospi_m05_p27 = pair_set_epi16(-cospi_5_64, cospi_27_64);
        const __m256i k__DCT_CONST_ROUNDING = _mm256_set1_epi32(DCT_CONST_ROUNDING);
        const __m256i kZero = _mm256_set1_epi16(0);
        const __m256i kOne = _mm256_set1_epi16(1);
        // Do the two transform/transpose passes
        int pass;
        short *poutput = intermediate;
        for (pass = 0; pass < 2; ++pass) {
            // We process eight columns (transposed rows in second pass) at a time.
            int column_start;
            for (column_start = 0; column_start < 32; column_start += 16) {
                __m256i step1[32];
                __m256i step2[32];
                __m256i step3[32];
                __m256i out[32];
                // Stage 1
                // Note: even though all the loads below are aligned, using the aligned
                //       intrinsic make the code slightly slower.
                if (0 == pass) {
                    const short *in = &input[column_start];
                    // step1[i] =  (in[ 0 * stride] + in[(32 -  1) * stride]) << 2;
                    // Note: the next four blocks could be in a loop. That would help the
                    //       instruction cache but is actually slower.
                    {
                        const short *ina = in + 0 * str1;
                        const short *inb = in + 31 * str1;
                        __m256i *step1a = &step1[0];
                        __m256i *step1b = &step1[31];
                        const __m256i ina0 = loada_si256(ina);
                        const __m256i ina1 = loada_si256(ina + str1);
                        const __m256i ina2 = loada_si256(ina + str2);
                        const __m256i ina3 = loada_si256(ina + str3);
                        const __m256i inb3 = loada_si256(inb - str3);
                        const __m256i inb2 = loada_si256(inb - str2);
                        const __m256i inb1 = loada_si256(inb - str1);
                        const __m256i inb0 = loada_si256(inb);
                        step1a[0] = _mm256_add_epi16(ina0, inb0);
                        step1a[1] = _mm256_add_epi16(ina1, inb1);
                        step1a[2] = _mm256_add_epi16(ina2, inb2);
                        step1a[3] = _mm256_add_epi16(ina3, inb3);
                        step1b[-3] = _mm256_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm256_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm256_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm256_sub_epi16(ina0, inb0);
                        step1a[0] = _mm256_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm256_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm256_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm256_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm256_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm256_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm256_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm256_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const short *ina = in + 4 * str1;
                        const short *inb = in + 27 * str1;
                        __m256i *step1a = &step1[4];
                        __m256i *step1b = &step1[27];
                        const __m256i ina0 = loada_si256(ina);
                        const __m256i ina1 = loada_si256(ina + str1);
                        const __m256i ina2 = loada_si256(ina + str2);
                        const __m256i ina3 = loada_si256(ina + str3);
                        const __m256i inb3 = loada_si256(inb - str3);
                        const __m256i inb2 = loada_si256(inb - str2);
                        const __m256i inb1 = loada_si256(inb - str1);
                        const __m256i inb0 = loada_si256(inb);
                        step1a[0] = _mm256_add_epi16(ina0, inb0);
                        step1a[1] = _mm256_add_epi16(ina1, inb1);
                        step1a[2] = _mm256_add_epi16(ina2, inb2);
                        step1a[3] = _mm256_add_epi16(ina3, inb3);
                        step1b[-3] = _mm256_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm256_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm256_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm256_sub_epi16(ina0, inb0);
                        step1a[0] = _mm256_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm256_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm256_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm256_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm256_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm256_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm256_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm256_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const short *ina = in + 8 * str1;
                        const short *inb = in + 23 * str1;
                        __m256i *step1a = &step1[8];
                        __m256i *step1b = &step1[23];
                        const __m256i ina0 = loada_si256(ina);
                        const __m256i ina1 = loada_si256(ina + str1);
                        const __m256i ina2 = loada_si256(ina + str2);
                        const __m256i ina3 = loada_si256(ina + str3);
                        const __m256i inb3 = loada_si256(inb - str3);
                        const __m256i inb2 = loada_si256(inb - str2);
                        const __m256i inb1 = loada_si256(inb - str1);
                        const __m256i inb0 = loada_si256(inb);
                        step1a[0] = _mm256_add_epi16(ina0, inb0);
                        step1a[1] = _mm256_add_epi16(ina1, inb1);
                        step1a[2] = _mm256_add_epi16(ina2, inb2);
                        step1a[3] = _mm256_add_epi16(ina3, inb3);
                        step1b[-3] = _mm256_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm256_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm256_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm256_sub_epi16(ina0, inb0);
                        step1a[0] = _mm256_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm256_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm256_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm256_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm256_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm256_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm256_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm256_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const short *ina = in + 12 * str1;
                        const short *inb = in + 19 * str1;
                        __m256i *step1a = &step1[12];
                        __m256i *step1b = &step1[19];
                        const __m256i ina0 = loada_si256(ina);
                        const __m256i ina1 = loada_si256(ina + str1);
                        const __m256i ina2 = loada_si256(ina + str2);
                        const __m256i ina3 = loada_si256(ina + str3);
                        const __m256i inb3 = loada_si256(inb - str3);
                        const __m256i inb2 = loada_si256(inb - str2);
                        const __m256i inb1 = loada_si256(inb - str1);
                        const __m256i inb0 = loada_si256(inb);
                        step1a[0] = _mm256_add_epi16(ina0, inb0);
                        step1a[1] = _mm256_add_epi16(ina1, inb1);
                        step1a[2] = _mm256_add_epi16(ina2, inb2);
                        step1a[3] = _mm256_add_epi16(ina3, inb3);
                        step1b[-3] = _mm256_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm256_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm256_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm256_sub_epi16(ina0, inb0);
                        step1a[0] = _mm256_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm256_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm256_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm256_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm256_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm256_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm256_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm256_slli_epi16(step1b[-0], 2);
                    }
                } else {
                    short *in = &intermediate[column_start];
                    // step1[i] =  in[ 0 * 32] + in[(32 -  1) * 32];
                    // Note: using the same approach as above to have common offset is
                    //       counter-productive as all offsets can be calculated at compile
                    //       time.
                    // Note: the next four blocks could be in a loop. That would help the
                    //       instruction cache but is actually slower.
                    {
                        __m256i in00 =loada_si256(in + 0 * 32);
                        __m256i in01 =loada_si256(in + 1 * 32);
                        __m256i in02 =loada_si256(in + 2 * 32);
                        __m256i in03 =loada_si256(in + 3 * 32);
                        __m256i in28 =loada_si256(in + 28 * 32);
                        __m256i in29 =loada_si256(in + 29 * 32);
                        __m256i in30 =loada_si256(in + 30 * 32);
                        __m256i in31 =loada_si256(in + 31 * 32);
                        step1[0] = _mm256_add_epi16(in00, in31);
                        step1[1] = _mm256_add_epi16(in01, in30);
                        step1[2] = _mm256_add_epi16(in02, in29);
                        step1[3] = _mm256_add_epi16(in03, in28);
                        step1[28] = _mm256_sub_epi16(in03, in28);
                        step1[29] = _mm256_sub_epi16(in02, in29);
                        step1[30] = _mm256_sub_epi16(in01, in30);
                        step1[31] = _mm256_sub_epi16(in00, in31);
                    }
                    {
                        __m256i in04 = loada_si256(in + 4 * 32);
                        __m256i in05 = loada_si256(in + 5 * 32);
                        __m256i in06 = loada_si256(in + 6 * 32);
                        __m256i in07 = loada_si256(in + 7 * 32);
                        __m256i in24 = loada_si256(in + 24 * 32);
                        __m256i in25 = loada_si256(in + 25 * 32);
                        __m256i in26 = loada_si256(in + 26 * 32);
                        __m256i in27 = loada_si256(in + 27 * 32);
                        step1[4] = _mm256_add_epi16(in04, in27);
                        step1[5] = _mm256_add_epi16(in05, in26);
                        step1[6] = _mm256_add_epi16(in06, in25);
                        step1[7] = _mm256_add_epi16(in07, in24);
                        step1[24] = _mm256_sub_epi16(in07, in24);
                        step1[25] = _mm256_sub_epi16(in06, in25);
                        step1[26] = _mm256_sub_epi16(in05, in26);
                        step1[27] = _mm256_sub_epi16(in04, in27);
                    }
                    {
                        __m256i in08 = loada_si256(in + 8 * 32);
                        __m256i in09 = loada_si256(in + 9 * 32);
                        __m256i in10 = loada_si256(in + 10 * 32);
                        __m256i in11 = loada_si256(in + 11 * 32);
                        __m256i in20 = loada_si256(in + 20 * 32);
                        __m256i in21 = loada_si256(in + 21 * 32);
                        __m256i in22 = loada_si256(in + 22 * 32);
                        __m256i in23 = loada_si256(in + 23 * 32);
                        step1[8] = _mm256_add_epi16(in08, in23);
                        step1[9] = _mm256_add_epi16(in09, in22);
                        step1[10] = _mm256_add_epi16(in10, in21);
                        step1[11] = _mm256_add_epi16(in11, in20);
                        step1[20] = _mm256_sub_epi16(in11, in20);
                        step1[21] = _mm256_sub_epi16(in10, in21);
                        step1[22] = _mm256_sub_epi16(in09, in22);
                        step1[23] = _mm256_sub_epi16(in08, in23);
                    }
                    {
                        __m256i in12 = loada_si256(in + 12 * 32);
                        __m256i in13 = loada_si256(in + 13 * 32);
                        __m256i in14 = loada_si256(in + 14 * 32);
                        __m256i in15 = loada_si256(in + 15 * 32);
                        __m256i in16 = loada_si256(in + 16 * 32);
                        __m256i in17 = loada_si256(in + 17 * 32);
                        __m256i in18 = loada_si256(in + 18 * 32);
                        __m256i in19 = loada_si256(in + 19 * 32);
                        step1[12] = _mm256_add_epi16(in12, in19);
                        step1[13] = _mm256_add_epi16(in13, in18);
                        step1[14] = _mm256_add_epi16(in14, in17);
                        step1[15] = _mm256_add_epi16(in15, in16);
                        step1[16] = _mm256_sub_epi16(in15, in16);
                        step1[17] = _mm256_sub_epi16(in14, in17);
                        step1[18] = _mm256_sub_epi16(in13, in18);
                        step1[19] = _mm256_sub_epi16(in12, in19);
                    }
                }
                // Stage 2
                {
                    step2[0] = _mm256_add_epi16(step1[0], step1[15]);
                    step2[1] = _mm256_add_epi16(step1[1], step1[14]);
                    step2[2] = _mm256_add_epi16(step1[2], step1[13]);
                    step2[3] = _mm256_add_epi16(step1[3], step1[12]);
                    step2[4] = _mm256_add_epi16(step1[4], step1[11]);
                    step2[5] = _mm256_add_epi16(step1[5], step1[10]);
                    step2[6] = _mm256_add_epi16(step1[6], step1[9]);
                    step2[7] = _mm256_add_epi16(step1[7], step1[8]);
                    step2[8] = _mm256_sub_epi16(step1[7], step1[8]);
                    step2[9] = _mm256_sub_epi16(step1[6], step1[9]);
                    step2[10] = _mm256_sub_epi16(step1[5], step1[10]);
                    step2[11] = _mm256_sub_epi16(step1[4], step1[11]);
                    step2[12] = _mm256_sub_epi16(step1[3], step1[12]);
                    step2[13] = _mm256_sub_epi16(step1[2], step1[13]);
                    step2[14] = _mm256_sub_epi16(step1[1], step1[14]);
                    step2[15] = _mm256_sub_epi16(step1[0], step1[15]);
                }
                {
                    const __m256i s2_20_0 = _mm256_unpacklo_epi16(step1[27], step1[20]);
                    const __m256i s2_20_1 = _mm256_unpackhi_epi16(step1[27], step1[20]);
                    const __m256i s2_21_0 = _mm256_unpacklo_epi16(step1[26], step1[21]);
                    const __m256i s2_21_1 = _mm256_unpackhi_epi16(step1[26], step1[21]);
                    const __m256i s2_22_0 = _mm256_unpacklo_epi16(step1[25], step1[22]);
                    const __m256i s2_22_1 = _mm256_unpackhi_epi16(step1[25], step1[22]);
                    const __m256i s2_23_0 = _mm256_unpacklo_epi16(step1[24], step1[23]);
                    const __m256i s2_23_1 = _mm256_unpackhi_epi16(step1[24], step1[23]);
                    const __m256i s2_20_2 = _mm256_madd_epi16(s2_20_0, k__cospi_p16_m16);
                    const __m256i s2_20_3 = _mm256_madd_epi16(s2_20_1, k__cospi_p16_m16);
                    const __m256i s2_21_2 = _mm256_madd_epi16(s2_21_0, k__cospi_p16_m16);
                    const __m256i s2_21_3 = _mm256_madd_epi16(s2_21_1, k__cospi_p16_m16);
                    const __m256i s2_22_2 = _mm256_madd_epi16(s2_22_0, k__cospi_p16_m16);
                    const __m256i s2_22_3 = _mm256_madd_epi16(s2_22_1, k__cospi_p16_m16);
                    const __m256i s2_23_2 = _mm256_madd_epi16(s2_23_0, k__cospi_p16_m16);
                    const __m256i s2_23_3 = _mm256_madd_epi16(s2_23_1, k__cospi_p16_m16);
                    const __m256i s2_24_2 = _mm256_madd_epi16(s2_23_0, k__cospi_p16_p16);
                    const __m256i s2_24_3 = _mm256_madd_epi16(s2_23_1, k__cospi_p16_p16);
                    const __m256i s2_25_2 = _mm256_madd_epi16(s2_22_0, k__cospi_p16_p16);
                    const __m256i s2_25_3 = _mm256_madd_epi16(s2_22_1, k__cospi_p16_p16);
                    const __m256i s2_26_2 = _mm256_madd_epi16(s2_21_0, k__cospi_p16_p16);
                    const __m256i s2_26_3 = _mm256_madd_epi16(s2_21_1, k__cospi_p16_p16);
                    const __m256i s2_27_2 = _mm256_madd_epi16(s2_20_0, k__cospi_p16_p16);
                    const __m256i s2_27_3 = _mm256_madd_epi16(s2_20_1, k__cospi_p16_p16);
                    // dct_const_round_shift
                    const __m256i s2_20_4 = _mm256_add_epi32(s2_20_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_20_5 = _mm256_add_epi32(s2_20_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_21_4 = _mm256_add_epi32(s2_21_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_21_5 = _mm256_add_epi32(s2_21_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_22_4 = _mm256_add_epi32(s2_22_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_22_5 = _mm256_add_epi32(s2_22_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_23_4 = _mm256_add_epi32(s2_23_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_23_5 = _mm256_add_epi32(s2_23_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_24_4 = _mm256_add_epi32(s2_24_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_24_5 = _mm256_add_epi32(s2_24_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_25_4 = _mm256_add_epi32(s2_25_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_25_5 = _mm256_add_epi32(s2_25_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_26_4 = _mm256_add_epi32(s2_26_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_26_5 = _mm256_add_epi32(s2_26_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_27_4 = _mm256_add_epi32(s2_27_2, k__DCT_CONST_ROUNDING);
                    const __m256i s2_27_5 = _mm256_add_epi32(s2_27_3, k__DCT_CONST_ROUNDING);
                    const __m256i s2_20_6 = _mm256_srai_epi32(s2_20_4, DCT_CONST_BITS);
                    const __m256i s2_20_7 = _mm256_srai_epi32(s2_20_5, DCT_CONST_BITS);
                    const __m256i s2_21_6 = _mm256_srai_epi32(s2_21_4, DCT_CONST_BITS);
                    const __m256i s2_21_7 = _mm256_srai_epi32(s2_21_5, DCT_CONST_BITS);
                    const __m256i s2_22_6 = _mm256_srai_epi32(s2_22_4, DCT_CONST_BITS);
                    const __m256i s2_22_7 = _mm256_srai_epi32(s2_22_5, DCT_CONST_BITS);
                    const __m256i s2_23_6 = _mm256_srai_epi32(s2_23_4, DCT_CONST_BITS);
                    const __m256i s2_23_7 = _mm256_srai_epi32(s2_23_5, DCT_CONST_BITS);
                    const __m256i s2_24_6 = _mm256_srai_epi32(s2_24_4, DCT_CONST_BITS);
                    const __m256i s2_24_7 = _mm256_srai_epi32(s2_24_5, DCT_CONST_BITS);
                    const __m256i s2_25_6 = _mm256_srai_epi32(s2_25_4, DCT_CONST_BITS);
                    const __m256i s2_25_7 = _mm256_srai_epi32(s2_25_5, DCT_CONST_BITS);
                    const __m256i s2_26_6 = _mm256_srai_epi32(s2_26_4, DCT_CONST_BITS);
                    const __m256i s2_26_7 = _mm256_srai_epi32(s2_26_5, DCT_CONST_BITS);
                    const __m256i s2_27_6 = _mm256_srai_epi32(s2_27_4, DCT_CONST_BITS);
                    const __m256i s2_27_7 = _mm256_srai_epi32(s2_27_5, DCT_CONST_BITS);
                    // Combine
                    step2[20] = _mm256_packs_epi32(s2_20_6, s2_20_7);
                    step2[21] = _mm256_packs_epi32(s2_21_6, s2_21_7);
                    step2[22] = _mm256_packs_epi32(s2_22_6, s2_22_7);
                    step2[23] = _mm256_packs_epi32(s2_23_6, s2_23_7);
                    step2[24] = _mm256_packs_epi32(s2_24_6, s2_24_7);
                    step2[25] = _mm256_packs_epi32(s2_25_6, s2_25_7);
                    step2[26] = _mm256_packs_epi32(s2_26_6, s2_26_7);
                    step2[27] = _mm256_packs_epi32(s2_27_6, s2_27_7);
                }

//#if !FDCT32x32_HIGH_PRECISION
                // dump the magnitude by half, hence the intermediate values are within
                // the range of 16 bits.
                if (1 == pass) {
                    __m256i s3_00_0 = _mm256_cmpgt_epi16(kZero, step2[0]);
                    __m256i s3_01_0 = _mm256_cmpgt_epi16(kZero, step2[1]);
                    __m256i s3_02_0 = _mm256_cmpgt_epi16(kZero, step2[2]);
                    __m256i s3_03_0 = _mm256_cmpgt_epi16(kZero, step2[3]);
                    __m256i s3_04_0 = _mm256_cmpgt_epi16(kZero, step2[4]);
                    __m256i s3_05_0 = _mm256_cmpgt_epi16(kZero, step2[5]);
                    __m256i s3_06_0 = _mm256_cmpgt_epi16(kZero, step2[6]);
                    __m256i s3_07_0 = _mm256_cmpgt_epi16(kZero, step2[7]);
                    __m256i s2_08_0 = _mm256_cmpgt_epi16(kZero, step2[8]);
                    __m256i s2_09_0 = _mm256_cmpgt_epi16(kZero, step2[9]);
                    __m256i s3_10_0 = _mm256_cmpgt_epi16(kZero, step2[10]);
                    __m256i s3_11_0 = _mm256_cmpgt_epi16(kZero, step2[11]);
                    __m256i s3_12_0 = _mm256_cmpgt_epi16(kZero, step2[12]);
                    __m256i s3_13_0 = _mm256_cmpgt_epi16(kZero, step2[13]);
                    __m256i s2_14_0 = _mm256_cmpgt_epi16(kZero, step2[14]);
                    __m256i s2_15_0 = _mm256_cmpgt_epi16(kZero, step2[15]);
                    __m256i s3_16_0 = _mm256_cmpgt_epi16(kZero, step1[16]);
                    __m256i s3_17_0 = _mm256_cmpgt_epi16(kZero, step1[17]);
                    __m256i s3_18_0 = _mm256_cmpgt_epi16(kZero, step1[18]);
                    __m256i s3_19_0 = _mm256_cmpgt_epi16(kZero, step1[19]);
                    __m256i s3_20_0 = _mm256_cmpgt_epi16(kZero, step2[20]);
                    __m256i s3_21_0 = _mm256_cmpgt_epi16(kZero, step2[21]);
                    __m256i s3_22_0 = _mm256_cmpgt_epi16(kZero, step2[22]);
                    __m256i s3_23_0 = _mm256_cmpgt_epi16(kZero, step2[23]);
                    __m256i s3_24_0 = _mm256_cmpgt_epi16(kZero, step2[24]);
                    __m256i s3_25_0 = _mm256_cmpgt_epi16(kZero, step2[25]);
                    __m256i s3_26_0 = _mm256_cmpgt_epi16(kZero, step2[26]);
                    __m256i s3_27_0 = _mm256_cmpgt_epi16(kZero, step2[27]);
                    __m256i s3_28_0 = _mm256_cmpgt_epi16(kZero, step1[28]);
                    __m256i s3_29_0 = _mm256_cmpgt_epi16(kZero, step1[29]);
                    __m256i s3_30_0 = _mm256_cmpgt_epi16(kZero, step1[30]);
                    __m256i s3_31_0 = _mm256_cmpgt_epi16(kZero, step1[31]);

                    step2[0] = _mm256_sub_epi16(step2[0], s3_00_0);
                    step2[1] = _mm256_sub_epi16(step2[1], s3_01_0);
                    step2[2] = _mm256_sub_epi16(step2[2], s3_02_0);
                    step2[3] = _mm256_sub_epi16(step2[3], s3_03_0);
                    step2[4] = _mm256_sub_epi16(step2[4], s3_04_0);
                    step2[5] = _mm256_sub_epi16(step2[5], s3_05_0);
                    step2[6] = _mm256_sub_epi16(step2[6], s3_06_0);
                    step2[7] = _mm256_sub_epi16(step2[7], s3_07_0);
                    step2[8] = _mm256_sub_epi16(step2[8], s2_08_0);
                    step2[9] = _mm256_sub_epi16(step2[9], s2_09_0);
                    step2[10] = _mm256_sub_epi16(step2[10], s3_10_0);
                    step2[11] = _mm256_sub_epi16(step2[11], s3_11_0);
                    step2[12] = _mm256_sub_epi16(step2[12], s3_12_0);
                    step2[13] = _mm256_sub_epi16(step2[13], s3_13_0);
                    step2[14] = _mm256_sub_epi16(step2[14], s2_14_0);
                    step2[15] = _mm256_sub_epi16(step2[15], s2_15_0);
                    step1[16] = _mm256_sub_epi16(step1[16], s3_16_0);
                    step1[17] = _mm256_sub_epi16(step1[17], s3_17_0);
                    step1[18] = _mm256_sub_epi16(step1[18], s3_18_0);
                    step1[19] = _mm256_sub_epi16(step1[19], s3_19_0);
                    step2[20] = _mm256_sub_epi16(step2[20], s3_20_0);
                    step2[21] = _mm256_sub_epi16(step2[21], s3_21_0);
                    step2[22] = _mm256_sub_epi16(step2[22], s3_22_0);
                    step2[23] = _mm256_sub_epi16(step2[23], s3_23_0);
                    step2[24] = _mm256_sub_epi16(step2[24], s3_24_0);
                    step2[25] = _mm256_sub_epi16(step2[25], s3_25_0);
                    step2[26] = _mm256_sub_epi16(step2[26], s3_26_0);
                    step2[27] = _mm256_sub_epi16(step2[27], s3_27_0);
                    step1[28] = _mm256_sub_epi16(step1[28], s3_28_0);
                    step1[29] = _mm256_sub_epi16(step1[29], s3_29_0);
                    step1[30] = _mm256_sub_epi16(step1[30], s3_30_0);
                    step1[31] = _mm256_sub_epi16(step1[31], s3_31_0);

                    step2[0] = _mm256_add_epi16(step2[0], kOne);
                    step2[1] = _mm256_add_epi16(step2[1], kOne);
                    step2[2] = _mm256_add_epi16(step2[2], kOne);
                    step2[3] = _mm256_add_epi16(step2[3], kOne);
                    step2[4] = _mm256_add_epi16(step2[4], kOne);
                    step2[5] = _mm256_add_epi16(step2[5], kOne);
                    step2[6] = _mm256_add_epi16(step2[6], kOne);
                    step2[7] = _mm256_add_epi16(step2[7], kOne);
                    step2[8] = _mm256_add_epi16(step2[8], kOne);
                    step2[9] = _mm256_add_epi16(step2[9], kOne);
                    step2[10] = _mm256_add_epi16(step2[10], kOne);
                    step2[11] = _mm256_add_epi16(step2[11], kOne);
                    step2[12] = _mm256_add_epi16(step2[12], kOne);
                    step2[13] = _mm256_add_epi16(step2[13], kOne);
                    step2[14] = _mm256_add_epi16(step2[14], kOne);
                    step2[15] = _mm256_add_epi16(step2[15], kOne);
                    step1[16] = _mm256_add_epi16(step1[16], kOne);
                    step1[17] = _mm256_add_epi16(step1[17], kOne);
                    step1[18] = _mm256_add_epi16(step1[18], kOne);
                    step1[19] = _mm256_add_epi16(step1[19], kOne);
                    step2[20] = _mm256_add_epi16(step2[20], kOne);
                    step2[21] = _mm256_add_epi16(step2[21], kOne);
                    step2[22] = _mm256_add_epi16(step2[22], kOne);
                    step2[23] = _mm256_add_epi16(step2[23], kOne);
                    step2[24] = _mm256_add_epi16(step2[24], kOne);
                    step2[25] = _mm256_add_epi16(step2[25], kOne);
                    step2[26] = _mm256_add_epi16(step2[26], kOne);
                    step2[27] = _mm256_add_epi16(step2[27], kOne);
                    step1[28] = _mm256_add_epi16(step1[28], kOne);
                    step1[29] = _mm256_add_epi16(step1[29], kOne);
                    step1[30] = _mm256_add_epi16(step1[30], kOne);
                    step1[31] = _mm256_add_epi16(step1[31], kOne);

                    step2[0] = _mm256_srai_epi16(step2[0], 2);
                    step2[1] = _mm256_srai_epi16(step2[1], 2);
                    step2[2] = _mm256_srai_epi16(step2[2], 2);
                    step2[3] = _mm256_srai_epi16(step2[3], 2);
                    step2[4] = _mm256_srai_epi16(step2[4], 2);
                    step2[5] = _mm256_srai_epi16(step2[5], 2);
                    step2[6] = _mm256_srai_epi16(step2[6], 2);
                    step2[7] = _mm256_srai_epi16(step2[7], 2);
                    step2[8] = _mm256_srai_epi16(step2[8], 2);
                    step2[9] = _mm256_srai_epi16(step2[9], 2);
                    step2[10] = _mm256_srai_epi16(step2[10], 2);
                    step2[11] = _mm256_srai_epi16(step2[11], 2);
                    step2[12] = _mm256_srai_epi16(step2[12], 2);
                    step2[13] = _mm256_srai_epi16(step2[13], 2);
                    step2[14] = _mm256_srai_epi16(step2[14], 2);
                    step2[15] = _mm256_srai_epi16(step2[15], 2);
                    step1[16] = _mm256_srai_epi16(step1[16], 2);
                    step1[17] = _mm256_srai_epi16(step1[17], 2);
                    step1[18] = _mm256_srai_epi16(step1[18], 2);
                    step1[19] = _mm256_srai_epi16(step1[19], 2);
                    step2[20] = _mm256_srai_epi16(step2[20], 2);
                    step2[21] = _mm256_srai_epi16(step2[21], 2);
                    step2[22] = _mm256_srai_epi16(step2[22], 2);
                    step2[23] = _mm256_srai_epi16(step2[23], 2);
                    step2[24] = _mm256_srai_epi16(step2[24], 2);
                    step2[25] = _mm256_srai_epi16(step2[25], 2);
                    step2[26] = _mm256_srai_epi16(step2[26], 2);
                    step2[27] = _mm256_srai_epi16(step2[27], 2);
                    step1[28] = _mm256_srai_epi16(step1[28], 2);
                    step1[29] = _mm256_srai_epi16(step1[29], 2);
                    step1[30] = _mm256_srai_epi16(step1[30], 2);
                    step1[31] = _mm256_srai_epi16(step1[31], 2);
                }
//#endif  // !FDCT32x32_HIGH_PRECISION

//#if FDCT32x32_HIGH_PRECISION
//                if (pass == 0) {
//#endif
                    // Stage 3
                    {
                        step3[0] = _mm256_add_epi16(step2[(8 - 1)], step2[0]);
                        step3[1] = _mm256_add_epi16(step2[(8 - 2)], step2[1]);
                        step3[2] = _mm256_add_epi16(step2[(8 - 3)], step2[2]);
                        step3[3] = _mm256_add_epi16(step2[(8 - 4)], step2[3]);
                        step3[4] = _mm256_sub_epi16(step2[(8 - 5)], step2[4]);
                        step3[5] = _mm256_sub_epi16(step2[(8 - 6)], step2[5]);
                        step3[6] = _mm256_sub_epi16(step2[(8 - 7)], step2[6]);
                        step3[7] = _mm256_sub_epi16(step2[(8 - 8)], step2[7]);
                    }
                    {
                        const __m256i s3_10_0 = _mm256_unpacklo_epi16(step2[13], step2[10]);
                        const __m256i s3_10_1 = _mm256_unpackhi_epi16(step2[13], step2[10]);
                        const __m256i s3_11_0 = _mm256_unpacklo_epi16(step2[12], step2[11]);
                        const __m256i s3_11_1 = _mm256_unpackhi_epi16(step2[12], step2[11]);
                        const __m256i s3_10_2 = _mm256_madd_epi16(s3_10_0, k__cospi_p16_m16);
                        const __m256i s3_10_3 = _mm256_madd_epi16(s3_10_1, k__cospi_p16_m16);
                        const __m256i s3_11_2 = _mm256_madd_epi16(s3_11_0, k__cospi_p16_m16);
                        const __m256i s3_11_3 = _mm256_madd_epi16(s3_11_1, k__cospi_p16_m16);
                        const __m256i s3_12_2 = _mm256_madd_epi16(s3_11_0, k__cospi_p16_p16);
                        const __m256i s3_12_3 = _mm256_madd_epi16(s3_11_1, k__cospi_p16_p16);
                        const __m256i s3_13_2 = _mm256_madd_epi16(s3_10_0, k__cospi_p16_p16);
                        const __m256i s3_13_3 = _mm256_madd_epi16(s3_10_1, k__cospi_p16_p16);
                        // dct_const_round_shift
                        const __m256i s3_10_4 = _mm256_add_epi32(s3_10_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_10_5 = _mm256_add_epi32(s3_10_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_11_4 = _mm256_add_epi32(s3_11_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_11_5 = _mm256_add_epi32(s3_11_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_12_4 = _mm256_add_epi32(s3_12_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_12_5 = _mm256_add_epi32(s3_12_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_13_4 = _mm256_add_epi32(s3_13_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_13_5 = _mm256_add_epi32(s3_13_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_10_6 = _mm256_srai_epi32(s3_10_4, DCT_CONST_BITS);
                        const __m256i s3_10_7 = _mm256_srai_epi32(s3_10_5, DCT_CONST_BITS);
                        const __m256i s3_11_6 = _mm256_srai_epi32(s3_11_4, DCT_CONST_BITS);
                        const __m256i s3_11_7 = _mm256_srai_epi32(s3_11_5, DCT_CONST_BITS);
                        const __m256i s3_12_6 = _mm256_srai_epi32(s3_12_4, DCT_CONST_BITS);
                        const __m256i s3_12_7 = _mm256_srai_epi32(s3_12_5, DCT_CONST_BITS);
                        const __m256i s3_13_6 = _mm256_srai_epi32(s3_13_4, DCT_CONST_BITS);
                        const __m256i s3_13_7 = _mm256_srai_epi32(s3_13_5, DCT_CONST_BITS);
                        // Combine
                        step3[10] = _mm256_packs_epi32(s3_10_6, s3_10_7);
                        step3[11] = _mm256_packs_epi32(s3_11_6, s3_11_7);
                        step3[12] = _mm256_packs_epi32(s3_12_6, s3_12_7);
                        step3[13] = _mm256_packs_epi32(s3_13_6, s3_13_7);
                    }
                    {
                        step3[16] = _mm256_add_epi16(step2[23], step1[16]);
                        step3[17] = _mm256_add_epi16(step2[22], step1[17]);
                        step3[18] = _mm256_add_epi16(step2[21], step1[18]);
                        step3[19] = _mm256_add_epi16(step2[20], step1[19]);
                        step3[20] = _mm256_sub_epi16(step1[19], step2[20]);
                        step3[21] = _mm256_sub_epi16(step1[18], step2[21]);
                        step3[22] = _mm256_sub_epi16(step1[17], step2[22]);
                        step3[23] = _mm256_sub_epi16(step1[16], step2[23]);
                        step3[24] = _mm256_sub_epi16(step1[31], step2[24]);
                        step3[25] = _mm256_sub_epi16(step1[30], step2[25]);
                        step3[26] = _mm256_sub_epi16(step1[29], step2[26]);
                        step3[27] = _mm256_sub_epi16(step1[28], step2[27]);
                        step3[28] = _mm256_add_epi16(step2[27], step1[28]);
                        step3[29] = _mm256_add_epi16(step2[26], step1[29]);
                        step3[30] = _mm256_add_epi16(step2[25], step1[30]);
                        step3[31] = _mm256_add_epi16(step2[24], step1[31]);
                    }

                    // Stage 4
                    {
                        step1[0] = _mm256_add_epi16(step3[3], step3[0]);
                        step1[1] = _mm256_add_epi16(step3[2], step3[1]);
                        step1[2] = _mm256_sub_epi16(step3[1], step3[2]);
                        step1[3] = _mm256_sub_epi16(step3[0], step3[3]);
                        step1[8] = _mm256_add_epi16(step3[11], step2[8]);
                        step1[9] = _mm256_add_epi16(step3[10], step2[9]);
                        step1[10] = _mm256_sub_epi16(step2[9], step3[10]);
                        step1[11] = _mm256_sub_epi16(step2[8], step3[11]);
                        step1[12] = _mm256_sub_epi16(step2[15], step3[12]);
                        step1[13] = _mm256_sub_epi16(step2[14], step3[13]);
                        step1[14] = _mm256_add_epi16(step3[13], step2[14]);
                        step1[15] = _mm256_add_epi16(step3[12], step2[15]);
                    }
                    {
                        const __m256i s1_05_0 = _mm256_unpacklo_epi16(step3[6], step3[5]);
                        const __m256i s1_05_1 = _mm256_unpackhi_epi16(step3[6], step3[5]);
                        const __m256i s1_05_2 = _mm256_madd_epi16(s1_05_0, k__cospi_p16_m16);
                        const __m256i s1_05_3 = _mm256_madd_epi16(s1_05_1, k__cospi_p16_m16);
                        const __m256i s1_06_2 = _mm256_madd_epi16(s1_05_0, k__cospi_p16_p16);
                        const __m256i s1_06_3 = _mm256_madd_epi16(s1_05_1, k__cospi_p16_p16);
                        // dct_const_round_shift
                        const __m256i s1_05_4 = _mm256_add_epi32(s1_05_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_05_5 = _mm256_add_epi32(s1_05_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_06_4 = _mm256_add_epi32(s1_06_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_06_5 = _mm256_add_epi32(s1_06_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_05_6 = _mm256_srai_epi32(s1_05_4, DCT_CONST_BITS);
                        const __m256i s1_05_7 = _mm256_srai_epi32(s1_05_5, DCT_CONST_BITS);
                        const __m256i s1_06_6 = _mm256_srai_epi32(s1_06_4, DCT_CONST_BITS);
                        const __m256i s1_06_7 = _mm256_srai_epi32(s1_06_5, DCT_CONST_BITS);
                        // Combine
                        step1[5] = _mm256_packs_epi32(s1_05_6, s1_05_7);
                        step1[6] = _mm256_packs_epi32(s1_06_6, s1_06_7);
                    }
                    {
                        const __m256i s1_18_0 = _mm256_unpacklo_epi16(step3[18], step3[29]);
                        const __m256i s1_18_1 = _mm256_unpackhi_epi16(step3[18], step3[29]);
                        const __m256i s1_19_0 = _mm256_unpacklo_epi16(step3[19], step3[28]);
                        const __m256i s1_19_1 = _mm256_unpackhi_epi16(step3[19], step3[28]);
                        const __m256i s1_20_0 = _mm256_unpacklo_epi16(step3[20], step3[27]);
                        const __m256i s1_20_1 = _mm256_unpackhi_epi16(step3[20], step3[27]);
                        const __m256i s1_21_0 = _mm256_unpacklo_epi16(step3[21], step3[26]);
                        const __m256i s1_21_1 = _mm256_unpackhi_epi16(step3[21], step3[26]);
                        const __m256i s1_18_2 = _mm256_madd_epi16(s1_18_0, k__cospi_m08_p24);
                        const __m256i s1_18_3 = _mm256_madd_epi16(s1_18_1, k__cospi_m08_p24);
                        const __m256i s1_19_2 = _mm256_madd_epi16(s1_19_0, k__cospi_m08_p24);
                        const __m256i s1_19_3 = _mm256_madd_epi16(s1_19_1, k__cospi_m08_p24);
                        const __m256i s1_20_2 = _mm256_madd_epi16(s1_20_0, k__cospi_m24_m08);
                        const __m256i s1_20_3 = _mm256_madd_epi16(s1_20_1, k__cospi_m24_m08);
                        const __m256i s1_21_2 = _mm256_madd_epi16(s1_21_0, k__cospi_m24_m08);
                        const __m256i s1_21_3 = _mm256_madd_epi16(s1_21_1, k__cospi_m24_m08);
                        const __m256i s1_26_2 = _mm256_madd_epi16(s1_21_0, k__cospi_m08_p24);
                        const __m256i s1_26_3 = _mm256_madd_epi16(s1_21_1, k__cospi_m08_p24);
                        const __m256i s1_27_2 = _mm256_madd_epi16(s1_20_0, k__cospi_m08_p24);
                        const __m256i s1_27_3 = _mm256_madd_epi16(s1_20_1, k__cospi_m08_p24);
                        const __m256i s1_28_2 = _mm256_madd_epi16(s1_19_0, k__cospi_p24_p08);
                        const __m256i s1_28_3 = _mm256_madd_epi16(s1_19_1, k__cospi_p24_p08);
                        const __m256i s1_29_2 = _mm256_madd_epi16(s1_18_0, k__cospi_p24_p08);
                        const __m256i s1_29_3 = _mm256_madd_epi16(s1_18_1, k__cospi_p24_p08);
                        // dct_const_round_shift
                        const __m256i s1_18_4 = _mm256_add_epi32(s1_18_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_18_5 = _mm256_add_epi32(s1_18_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_19_4 = _mm256_add_epi32(s1_19_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_19_5 = _mm256_add_epi32(s1_19_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_20_4 = _mm256_add_epi32(s1_20_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_20_5 = _mm256_add_epi32(s1_20_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_21_4 = _mm256_add_epi32(s1_21_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_21_5 = _mm256_add_epi32(s1_21_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_26_4 = _mm256_add_epi32(s1_26_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_26_5 = _mm256_add_epi32(s1_26_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_27_4 = _mm256_add_epi32(s1_27_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_27_5 = _mm256_add_epi32(s1_27_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_28_4 = _mm256_add_epi32(s1_28_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_28_5 = _mm256_add_epi32(s1_28_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_29_4 = _mm256_add_epi32(s1_29_2, k__DCT_CONST_ROUNDING);
                        const __m256i s1_29_5 = _mm256_add_epi32(s1_29_3, k__DCT_CONST_ROUNDING);
                        const __m256i s1_18_6 = _mm256_srai_epi32(s1_18_4, DCT_CONST_BITS);
                        const __m256i s1_18_7 = _mm256_srai_epi32(s1_18_5, DCT_CONST_BITS);
                        const __m256i s1_19_6 = _mm256_srai_epi32(s1_19_4, DCT_CONST_BITS);
                        const __m256i s1_19_7 = _mm256_srai_epi32(s1_19_5, DCT_CONST_BITS);
                        const __m256i s1_20_6 = _mm256_srai_epi32(s1_20_4, DCT_CONST_BITS);
                        const __m256i s1_20_7 = _mm256_srai_epi32(s1_20_5, DCT_CONST_BITS);
                        const __m256i s1_21_6 = _mm256_srai_epi32(s1_21_4, DCT_CONST_BITS);
                        const __m256i s1_21_7 = _mm256_srai_epi32(s1_21_5, DCT_CONST_BITS);
                        const __m256i s1_26_6 = _mm256_srai_epi32(s1_26_4, DCT_CONST_BITS);
                        const __m256i s1_26_7 = _mm256_srai_epi32(s1_26_5, DCT_CONST_BITS);
                        const __m256i s1_27_6 = _mm256_srai_epi32(s1_27_4, DCT_CONST_BITS);
                        const __m256i s1_27_7 = _mm256_srai_epi32(s1_27_5, DCT_CONST_BITS);
                        const __m256i s1_28_6 = _mm256_srai_epi32(s1_28_4, DCT_CONST_BITS);
                        const __m256i s1_28_7 = _mm256_srai_epi32(s1_28_5, DCT_CONST_BITS);
                        const __m256i s1_29_6 = _mm256_srai_epi32(s1_29_4, DCT_CONST_BITS);
                        const __m256i s1_29_7 = _mm256_srai_epi32(s1_29_5, DCT_CONST_BITS);
                        // Combine
                        step1[18] = _mm256_packs_epi32(s1_18_6, s1_18_7);
                        step1[19] = _mm256_packs_epi32(s1_19_6, s1_19_7);
                        step1[20] = _mm256_packs_epi32(s1_20_6, s1_20_7);
                        step1[21] = _mm256_packs_epi32(s1_21_6, s1_21_7);
                        step1[26] = _mm256_packs_epi32(s1_26_6, s1_26_7);
                        step1[27] = _mm256_packs_epi32(s1_27_6, s1_27_7);
                        step1[28] = _mm256_packs_epi32(s1_28_6, s1_28_7);
                        step1[29] = _mm256_packs_epi32(s1_29_6, s1_29_7);
                    }
                    // Stage 5
                    {
                        step2[4] = _mm256_add_epi16(step1[5], step3[4]);
                        step2[5] = _mm256_sub_epi16(step3[4], step1[5]);
                        step2[6] = _mm256_sub_epi16(step3[7], step1[6]);
                        step2[7] = _mm256_add_epi16(step1[6], step3[7]);
                    }
                    {
                        const __m256i out_00_0 = _mm256_unpacklo_epi16(step1[0], step1[1]);
                        const __m256i out_00_1 = _mm256_unpackhi_epi16(step1[0], step1[1]);
                        const __m256i out_08_0 = _mm256_unpacklo_epi16(step1[2], step1[3]);
                        const __m256i out_08_1 = _mm256_unpackhi_epi16(step1[2], step1[3]);
                        const __m256i out_00_2 = _mm256_madd_epi16(out_00_0, k__cospi_p16_p16);
                        const __m256i out_00_3 = _mm256_madd_epi16(out_00_1, k__cospi_p16_p16);
                        const __m256i out_16_2 = _mm256_madd_epi16(out_00_0, k__cospi_p16_m16);
                        const __m256i out_16_3 = _mm256_madd_epi16(out_00_1, k__cospi_p16_m16);
                        const __m256i out_08_2 = _mm256_madd_epi16(out_08_0, k__cospi_p24_p08);
                        const __m256i out_08_3 = _mm256_madd_epi16(out_08_1, k__cospi_p24_p08);
                        const __m256i out_24_2 = _mm256_madd_epi16(out_08_0, k__cospi_m08_p24);
                        const __m256i out_24_3 = _mm256_madd_epi16(out_08_1, k__cospi_m08_p24);
                        // dct_const_round_shift
                        const __m256i out_00_4 = _mm256_add_epi32(out_00_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_00_5 = _mm256_add_epi32(out_00_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_16_4 = _mm256_add_epi32(out_16_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_16_5 = _mm256_add_epi32(out_16_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_08_4 = _mm256_add_epi32(out_08_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_08_5 = _mm256_add_epi32(out_08_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_24_4 = _mm256_add_epi32(out_24_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_24_5 = _mm256_add_epi32(out_24_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_00_6 = _mm256_srai_epi32(out_00_4, DCT_CONST_BITS);
                        const __m256i out_00_7 = _mm256_srai_epi32(out_00_5, DCT_CONST_BITS);
                        const __m256i out_16_6 = _mm256_srai_epi32(out_16_4, DCT_CONST_BITS);
                        const __m256i out_16_7 = _mm256_srai_epi32(out_16_5, DCT_CONST_BITS);
                        const __m256i out_08_6 = _mm256_srai_epi32(out_08_4, DCT_CONST_BITS);
                        const __m256i out_08_7 = _mm256_srai_epi32(out_08_5, DCT_CONST_BITS);
                        const __m256i out_24_6 = _mm256_srai_epi32(out_24_4, DCT_CONST_BITS);
                        const __m256i out_24_7 = _mm256_srai_epi32(out_24_5, DCT_CONST_BITS);
                        // Combine
                        out[0]  = _mm256_packs_epi32(out_00_6, out_00_7);
                        out[16] = _mm256_packs_epi32(out_16_6, out_16_7);
                        out[8]  = _mm256_packs_epi32(out_08_6, out_08_7);
                        out[24] = _mm256_packs_epi32(out_24_6, out_24_7);
                    }
                    {
                        const __m256i s2_09_0 = _mm256_unpacklo_epi16(step1[9], step1[14]);
                        const __m256i s2_09_1 = _mm256_unpackhi_epi16(step1[9], step1[14]);
                        const __m256i s2_10_0 = _mm256_unpacklo_epi16(step1[10], step1[13]);
                        const __m256i s2_10_1 = _mm256_unpackhi_epi16(step1[10], step1[13]);
                        const __m256i s2_09_2 = _mm256_madd_epi16(s2_09_0, k__cospi_m08_p24);
                        const __m256i s2_09_3 = _mm256_madd_epi16(s2_09_1, k__cospi_m08_p24);
                        const __m256i s2_10_2 = _mm256_madd_epi16(s2_10_0, k__cospi_m24_m08);
                        const __m256i s2_10_3 = _mm256_madd_epi16(s2_10_1, k__cospi_m24_m08);
                        const __m256i s2_13_2 = _mm256_madd_epi16(s2_10_0, k__cospi_m08_p24);
                        const __m256i s2_13_3 = _mm256_madd_epi16(s2_10_1, k__cospi_m08_p24);
                        const __m256i s2_14_2 = _mm256_madd_epi16(s2_09_0, k__cospi_p24_p08);
                        const __m256i s2_14_3 = _mm256_madd_epi16(s2_09_1, k__cospi_p24_p08);
                        // dct_const_round_shift
                        const __m256i s2_09_4 = _mm256_add_epi32(s2_09_2, k__DCT_CONST_ROUNDING);
                        const __m256i s2_09_5 = _mm256_add_epi32(s2_09_3, k__DCT_CONST_ROUNDING);
                        const __m256i s2_10_4 = _mm256_add_epi32(s2_10_2, k__DCT_CONST_ROUNDING);
                        const __m256i s2_10_5 = _mm256_add_epi32(s2_10_3, k__DCT_CONST_ROUNDING);
                        const __m256i s2_13_4 = _mm256_add_epi32(s2_13_2, k__DCT_CONST_ROUNDING);
                        const __m256i s2_13_5 = _mm256_add_epi32(s2_13_3, k__DCT_CONST_ROUNDING);
                        const __m256i s2_14_4 = _mm256_add_epi32(s2_14_2, k__DCT_CONST_ROUNDING);
                        const __m256i s2_14_5 = _mm256_add_epi32(s2_14_3, k__DCT_CONST_ROUNDING);
                        const __m256i s2_09_6 = _mm256_srai_epi32(s2_09_4, DCT_CONST_BITS);
                        const __m256i s2_09_7 = _mm256_srai_epi32(s2_09_5, DCT_CONST_BITS);
                        const __m256i s2_10_6 = _mm256_srai_epi32(s2_10_4, DCT_CONST_BITS);
                        const __m256i s2_10_7 = _mm256_srai_epi32(s2_10_5, DCT_CONST_BITS);
                        const __m256i s2_13_6 = _mm256_srai_epi32(s2_13_4, DCT_CONST_BITS);
                        const __m256i s2_13_7 = _mm256_srai_epi32(s2_13_5, DCT_CONST_BITS);
                        const __m256i s2_14_6 = _mm256_srai_epi32(s2_14_4, DCT_CONST_BITS);
                        const __m256i s2_14_7 = _mm256_srai_epi32(s2_14_5, DCT_CONST_BITS);
                        // Combine
                        step2[9]  = _mm256_packs_epi32(s2_09_6, s2_09_7);
                        step2[10] = _mm256_packs_epi32(s2_10_6, s2_10_7);
                        step2[13] = _mm256_packs_epi32(s2_13_6, s2_13_7);
                        step2[14] = _mm256_packs_epi32(s2_14_6, s2_14_7);
                    }
                    {
                        step2[16] = _mm256_add_epi16(step1[19], step3[16]);
                        step2[17] = _mm256_add_epi16(step1[18], step3[17]);
                        step2[18] = _mm256_sub_epi16(step3[17], step1[18]);
                        step2[19] = _mm256_sub_epi16(step3[16], step1[19]);
                        step2[20] = _mm256_sub_epi16(step3[23], step1[20]);
                        step2[21] = _mm256_sub_epi16(step3[22], step1[21]);
                        step2[22] = _mm256_add_epi16(step1[21], step3[22]);
                        step2[23] = _mm256_add_epi16(step1[20], step3[23]);
                        step2[24] = _mm256_add_epi16(step1[27], step3[24]);
                        step2[25] = _mm256_add_epi16(step1[26], step3[25]);
                        step2[26] = _mm256_sub_epi16(step3[25], step1[26]);
                        step2[27] = _mm256_sub_epi16(step3[24], step1[27]);
                        step2[28] = _mm256_sub_epi16(step3[31], step1[28]);
                        step2[29] = _mm256_sub_epi16(step3[30], step1[29]);
                        step2[30] = _mm256_add_epi16(step1[29], step3[30]);
                        step2[31] = _mm256_add_epi16(step1[28], step3[31]);
                    }
                    // Stage 6
                    {
                        const __m256i out_04_0 = _mm256_unpacklo_epi16(step2[4], step2[7]);
                        const __m256i out_04_1 = _mm256_unpackhi_epi16(step2[4], step2[7]);
                        const __m256i out_20_0 = _mm256_unpacklo_epi16(step2[5], step2[6]);
                        const __m256i out_20_1 = _mm256_unpackhi_epi16(step2[5], step2[6]);
                        const __m256i out_12_0 = _mm256_unpacklo_epi16(step2[5], step2[6]);
                        const __m256i out_12_1 = _mm256_unpackhi_epi16(step2[5], step2[6]);
                        const __m256i out_28_0 = _mm256_unpacklo_epi16(step2[4], step2[7]);
                        const __m256i out_28_1 = _mm256_unpackhi_epi16(step2[4], step2[7]);
                        const __m256i out_04_2 = _mm256_madd_epi16(out_04_0, k__cospi_p28_p04);
                        const __m256i out_04_3 = _mm256_madd_epi16(out_04_1, k__cospi_p28_p04);
                        const __m256i out_20_2 = _mm256_madd_epi16(out_20_0, k__cospi_p12_p20);
                        const __m256i out_20_3 = _mm256_madd_epi16(out_20_1, k__cospi_p12_p20);
                        const __m256i out_12_2 = _mm256_madd_epi16(out_12_0, k__cospi_m20_p12);
                        const __m256i out_12_3 = _mm256_madd_epi16(out_12_1, k__cospi_m20_p12);
                        const __m256i out_28_2 = _mm256_madd_epi16(out_28_0, k__cospi_m04_p28);
                        const __m256i out_28_3 = _mm256_madd_epi16(out_28_1, k__cospi_m04_p28);
                        // dct_const_round_shift
                        const __m256i out_04_4 = _mm256_add_epi32(out_04_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_04_5 = _mm256_add_epi32(out_04_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_20_4 = _mm256_add_epi32(out_20_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_20_5 = _mm256_add_epi32(out_20_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_12_4 = _mm256_add_epi32(out_12_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_12_5 = _mm256_add_epi32(out_12_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_28_4 = _mm256_add_epi32(out_28_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_28_5 = _mm256_add_epi32(out_28_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_04_6 = _mm256_srai_epi32(out_04_4, DCT_CONST_BITS);
                        const __m256i out_04_7 = _mm256_srai_epi32(out_04_5, DCT_CONST_BITS);
                        const __m256i out_20_6 = _mm256_srai_epi32(out_20_4, DCT_CONST_BITS);
                        const __m256i out_20_7 = _mm256_srai_epi32(out_20_5, DCT_CONST_BITS);
                        const __m256i out_12_6 = _mm256_srai_epi32(out_12_4, DCT_CONST_BITS);
                        const __m256i out_12_7 = _mm256_srai_epi32(out_12_5, DCT_CONST_BITS);
                        const __m256i out_28_6 = _mm256_srai_epi32(out_28_4, DCT_CONST_BITS);
                        const __m256i out_28_7 = _mm256_srai_epi32(out_28_5, DCT_CONST_BITS);
                        // Combine
                        out[4]  = _mm256_packs_epi32(out_04_6, out_04_7);
                        out[20] = _mm256_packs_epi32(out_20_6, out_20_7);
                        out[12] = _mm256_packs_epi32(out_12_6, out_12_7);
                        out[28] = _mm256_packs_epi32(out_28_6, out_28_7);
                    }
                    {
                        step3[8]  = _mm256_add_epi16(step2[9], step1[8]);
                        step3[9]  = _mm256_sub_epi16(step1[8], step2[9]);
                        step3[10] = _mm256_sub_epi16(step1[11], step2[10]);
                        step3[11] = _mm256_add_epi16(step2[10], step1[11]);
                        step3[12] = _mm256_add_epi16(step2[13], step1[12]);
                        step3[13] = _mm256_sub_epi16(step1[12], step2[13]);
                        step3[14] = _mm256_sub_epi16(step1[15], step2[14]);
                        step3[15] = _mm256_add_epi16(step2[14], step1[15]);
                    }
                    {
                        const __m256i s3_17_0 = _mm256_unpacklo_epi16(step2[17], step2[30]);
                        const __m256i s3_17_1 = _mm256_unpackhi_epi16(step2[17], step2[30]);
                        const __m256i s3_18_0 = _mm256_unpacklo_epi16(step2[18], step2[29]);
                        const __m256i s3_18_1 = _mm256_unpackhi_epi16(step2[18], step2[29]);
                        const __m256i s3_21_0 = _mm256_unpacklo_epi16(step2[21], step2[26]);
                        const __m256i s3_21_1 = _mm256_unpackhi_epi16(step2[21], step2[26]);
                        const __m256i s3_22_0 = _mm256_unpacklo_epi16(step2[22], step2[25]);
                        const __m256i s3_22_1 = _mm256_unpackhi_epi16(step2[22], step2[25]);
                        const __m256i s3_17_2 = _mm256_madd_epi16(s3_17_0, k__cospi_m04_p28);
                        const __m256i s3_17_3 = _mm256_madd_epi16(s3_17_1, k__cospi_m04_p28);
                        const __m256i s3_18_2 = _mm256_madd_epi16(s3_18_0, k__cospi_m28_m04);
                        const __m256i s3_18_3 = _mm256_madd_epi16(s3_18_1, k__cospi_m28_m04);
                        const __m256i s3_21_2 = _mm256_madd_epi16(s3_21_0, k__cospi_m20_p12);
                        const __m256i s3_21_3 = _mm256_madd_epi16(s3_21_1, k__cospi_m20_p12);
                        const __m256i s3_22_2 = _mm256_madd_epi16(s3_22_0, k__cospi_m12_m20);
                        const __m256i s3_22_3 = _mm256_madd_epi16(s3_22_1, k__cospi_m12_m20);
                        const __m256i s3_25_2 = _mm256_madd_epi16(s3_22_0, k__cospi_m20_p12);
                        const __m256i s3_25_3 = _mm256_madd_epi16(s3_22_1, k__cospi_m20_p12);
                        const __m256i s3_26_2 = _mm256_madd_epi16(s3_21_0, k__cospi_p12_p20);
                        const __m256i s3_26_3 = _mm256_madd_epi16(s3_21_1, k__cospi_p12_p20);
                        const __m256i s3_29_2 = _mm256_madd_epi16(s3_18_0, k__cospi_m04_p28);
                        const __m256i s3_29_3 = _mm256_madd_epi16(s3_18_1, k__cospi_m04_p28);
                        const __m256i s3_30_2 = _mm256_madd_epi16(s3_17_0, k__cospi_p28_p04);
                        const __m256i s3_30_3 = _mm256_madd_epi16(s3_17_1, k__cospi_p28_p04);
                        // dct_const_round_shift
                        const __m256i s3_17_4 = _mm256_add_epi32(s3_17_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_17_5 = _mm256_add_epi32(s3_17_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_18_4 = _mm256_add_epi32(s3_18_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_18_5 = _mm256_add_epi32(s3_18_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_21_4 = _mm256_add_epi32(s3_21_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_21_5 = _mm256_add_epi32(s3_21_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_22_4 = _mm256_add_epi32(s3_22_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_22_5 = _mm256_add_epi32(s3_22_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_17_6 = _mm256_srai_epi32(s3_17_4, DCT_CONST_BITS);
                        const __m256i s3_17_7 = _mm256_srai_epi32(s3_17_5, DCT_CONST_BITS);
                        const __m256i s3_18_6 = _mm256_srai_epi32(s3_18_4, DCT_CONST_BITS);
                        const __m256i s3_18_7 = _mm256_srai_epi32(s3_18_5, DCT_CONST_BITS);
                        const __m256i s3_21_6 = _mm256_srai_epi32(s3_21_4, DCT_CONST_BITS);
                        const __m256i s3_21_7 = _mm256_srai_epi32(s3_21_5, DCT_CONST_BITS);
                        const __m256i s3_22_6 = _mm256_srai_epi32(s3_22_4, DCT_CONST_BITS);
                        const __m256i s3_22_7 = _mm256_srai_epi32(s3_22_5, DCT_CONST_BITS);
                        const __m256i s3_25_4 = _mm256_add_epi32(s3_25_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_25_5 = _mm256_add_epi32(s3_25_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_26_4 = _mm256_add_epi32(s3_26_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_26_5 = _mm256_add_epi32(s3_26_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_29_4 = _mm256_add_epi32(s3_29_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_29_5 = _mm256_add_epi32(s3_29_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_30_4 = _mm256_add_epi32(s3_30_2, k__DCT_CONST_ROUNDING);
                        const __m256i s3_30_5 = _mm256_add_epi32(s3_30_3, k__DCT_CONST_ROUNDING);
                        const __m256i s3_25_6 = _mm256_srai_epi32(s3_25_4, DCT_CONST_BITS);
                        const __m256i s3_25_7 = _mm256_srai_epi32(s3_25_5, DCT_CONST_BITS);
                        const __m256i s3_26_6 = _mm256_srai_epi32(s3_26_4, DCT_CONST_BITS);
                        const __m256i s3_26_7 = _mm256_srai_epi32(s3_26_5, DCT_CONST_BITS);
                        const __m256i s3_29_6 = _mm256_srai_epi32(s3_29_4, DCT_CONST_BITS);
                        const __m256i s3_29_7 = _mm256_srai_epi32(s3_29_5, DCT_CONST_BITS);
                        const __m256i s3_30_6 = _mm256_srai_epi32(s3_30_4, DCT_CONST_BITS);
                        const __m256i s3_30_7 = _mm256_srai_epi32(s3_30_5, DCT_CONST_BITS);
                        // Combine
                        step3[17] = _mm256_packs_epi32(s3_17_6, s3_17_7);
                        step3[18] = _mm256_packs_epi32(s3_18_6, s3_18_7);
                        step3[21] = _mm256_packs_epi32(s3_21_6, s3_21_7);
                        step3[22] = _mm256_packs_epi32(s3_22_6, s3_22_7);
                        // Combine
                        step3[25] = _mm256_packs_epi32(s3_25_6, s3_25_7);
                        step3[26] = _mm256_packs_epi32(s3_26_6, s3_26_7);
                        step3[29] = _mm256_packs_epi32(s3_29_6, s3_29_7);
                        step3[30] = _mm256_packs_epi32(s3_30_6, s3_30_7);
                    }
                    // Stage 7
                    {
                        const __m256i out_02_0 = _mm256_unpacklo_epi16(step3[8], step3[15]);
                        const __m256i out_02_1 = _mm256_unpackhi_epi16(step3[8], step3[15]);
                        const __m256i out_18_0 = _mm256_unpacklo_epi16(step3[9], step3[14]);
                        const __m256i out_18_1 = _mm256_unpackhi_epi16(step3[9], step3[14]);
                        const __m256i out_10_0 = _mm256_unpacklo_epi16(step3[10], step3[13]);
                        const __m256i out_10_1 = _mm256_unpackhi_epi16(step3[10], step3[13]);
                        const __m256i out_26_0 = _mm256_unpacklo_epi16(step3[11], step3[12]);
                        const __m256i out_26_1 = _mm256_unpackhi_epi16(step3[11], step3[12]);
                        const __m256i out_02_2 = _mm256_madd_epi16(out_02_0, k__cospi_p30_p02);
                        const __m256i out_02_3 = _mm256_madd_epi16(out_02_1, k__cospi_p30_p02);
                        const __m256i out_18_2 = _mm256_madd_epi16(out_18_0, k__cospi_p14_p18);
                        const __m256i out_18_3 = _mm256_madd_epi16(out_18_1, k__cospi_p14_p18);
                        const __m256i out_10_2 = _mm256_madd_epi16(out_10_0, k__cospi_p22_p10);
                        const __m256i out_10_3 = _mm256_madd_epi16(out_10_1, k__cospi_p22_p10);
                        const __m256i out_26_2 = _mm256_madd_epi16(out_26_0, k__cospi_p06_p26);
                        const __m256i out_26_3 = _mm256_madd_epi16(out_26_1, k__cospi_p06_p26);
                        const __m256i out_06_2 = _mm256_madd_epi16(out_26_0, k__cospi_m26_p06);
                        const __m256i out_06_3 = _mm256_madd_epi16(out_26_1, k__cospi_m26_p06);
                        const __m256i out_22_2 = _mm256_madd_epi16(out_10_0, k__cospi_m10_p22);
                        const __m256i out_22_3 = _mm256_madd_epi16(out_10_1, k__cospi_m10_p22);
                        const __m256i out_14_2 = _mm256_madd_epi16(out_18_0, k__cospi_m18_p14);
                        const __m256i out_14_3 = _mm256_madd_epi16(out_18_1, k__cospi_m18_p14);
                        const __m256i out_30_2 = _mm256_madd_epi16(out_02_0, k__cospi_m02_p30);
                        const __m256i out_30_3 = _mm256_madd_epi16(out_02_1, k__cospi_m02_p30);
                        // dct_const_round_shift
                        const __m256i out_02_4 = _mm256_add_epi32(out_02_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_02_5 = _mm256_add_epi32(out_02_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_18_4 = _mm256_add_epi32(out_18_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_18_5 = _mm256_add_epi32(out_18_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_10_4 = _mm256_add_epi32(out_10_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_10_5 = _mm256_add_epi32(out_10_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_26_4 = _mm256_add_epi32(out_26_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_26_5 = _mm256_add_epi32(out_26_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_06_4 = _mm256_add_epi32(out_06_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_06_5 = _mm256_add_epi32(out_06_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_22_4 = _mm256_add_epi32(out_22_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_22_5 = _mm256_add_epi32(out_22_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_14_4 = _mm256_add_epi32(out_14_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_14_5 = _mm256_add_epi32(out_14_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_30_4 = _mm256_add_epi32(out_30_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_30_5 = _mm256_add_epi32(out_30_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_02_6 = _mm256_srai_epi32(out_02_4, DCT_CONST_BITS);
                        const __m256i out_02_7 = _mm256_srai_epi32(out_02_5, DCT_CONST_BITS);
                        const __m256i out_18_6 = _mm256_srai_epi32(out_18_4, DCT_CONST_BITS);
                        const __m256i out_18_7 = _mm256_srai_epi32(out_18_5, DCT_CONST_BITS);
                        const __m256i out_10_6 = _mm256_srai_epi32(out_10_4, DCT_CONST_BITS);
                        const __m256i out_10_7 = _mm256_srai_epi32(out_10_5, DCT_CONST_BITS);
                        const __m256i out_26_6 = _mm256_srai_epi32(out_26_4, DCT_CONST_BITS);
                        const __m256i out_26_7 = _mm256_srai_epi32(out_26_5, DCT_CONST_BITS);
                        const __m256i out_06_6 = _mm256_srai_epi32(out_06_4, DCT_CONST_BITS);
                        const __m256i out_06_7 = _mm256_srai_epi32(out_06_5, DCT_CONST_BITS);
                        const __m256i out_22_6 = _mm256_srai_epi32(out_22_4, DCT_CONST_BITS);
                        const __m256i out_22_7 = _mm256_srai_epi32(out_22_5, DCT_CONST_BITS);
                        const __m256i out_14_6 = _mm256_srai_epi32(out_14_4, DCT_CONST_BITS);
                        const __m256i out_14_7 = _mm256_srai_epi32(out_14_5, DCT_CONST_BITS);
                        const __m256i out_30_6 = _mm256_srai_epi32(out_30_4, DCT_CONST_BITS);
                        const __m256i out_30_7 = _mm256_srai_epi32(out_30_5, DCT_CONST_BITS);
                        // Combine
                        out[2]  = _mm256_packs_epi32(out_02_6, out_02_7);
                        out[18] = _mm256_packs_epi32(out_18_6, out_18_7);
                        out[10] = _mm256_packs_epi32(out_10_6, out_10_7);
                        out[26] = _mm256_packs_epi32(out_26_6, out_26_7);
                        out[6]  = _mm256_packs_epi32(out_06_6, out_06_7);
                        out[22] = _mm256_packs_epi32(out_22_6, out_22_7);
                        out[14] = _mm256_packs_epi32(out_14_6, out_14_7);
                        out[30] = _mm256_packs_epi32(out_30_6, out_30_7);
                    }
                    {
                        step1[16] = _mm256_add_epi16(step3[17], step2[16]);
                        step1[17] = _mm256_sub_epi16(step2[16], step3[17]);
                        step1[18] = _mm256_sub_epi16(step2[19], step3[18]);
                        step1[19] = _mm256_add_epi16(step3[18], step2[19]);
                        step1[20] = _mm256_add_epi16(step3[21], step2[20]);
                        step1[21] = _mm256_sub_epi16(step2[20], step3[21]);
                        step1[22] = _mm256_sub_epi16(step2[23], step3[22]);
                        step1[23] = _mm256_add_epi16(step3[22], step2[23]);
                        step1[24] = _mm256_add_epi16(step3[25], step2[24]);
                        step1[25] = _mm256_sub_epi16(step2[24], step3[25]);
                        step1[26] = _mm256_sub_epi16(step2[27], step3[26]);
                        step1[27] = _mm256_add_epi16(step3[26], step2[27]);
                        step1[28] = _mm256_add_epi16(step3[29], step2[28]);
                        step1[29] = _mm256_sub_epi16(step2[28], step3[29]);
                        step1[30] = _mm256_sub_epi16(step2[31], step3[30]);
                        step1[31] = _mm256_add_epi16(step3[30], step2[31]);
                    }
                    // Final stage --- outputs indices are bit-reversed.
                    {
                        const __m256i out_01_0 = _mm256_unpacklo_epi16(step1[16], step1[31]);
                        const __m256i out_01_1 = _mm256_unpackhi_epi16(step1[16], step1[31]);
                        const __m256i out_17_0 = _mm256_unpacklo_epi16(step1[17], step1[30]);
                        const __m256i out_17_1 = _mm256_unpackhi_epi16(step1[17], step1[30]);
                        const __m256i out_09_0 = _mm256_unpacklo_epi16(step1[18], step1[29]);
                        const __m256i out_09_1 = _mm256_unpackhi_epi16(step1[18], step1[29]);
                        const __m256i out_25_0 = _mm256_unpacklo_epi16(step1[19], step1[28]);
                        const __m256i out_25_1 = _mm256_unpackhi_epi16(step1[19], step1[28]);
                        const __m256i out_01_2 = _mm256_madd_epi16(out_01_0, k__cospi_p31_p01);
                        const __m256i out_01_3 = _mm256_madd_epi16(out_01_1, k__cospi_p31_p01);
                        const __m256i out_17_2 = _mm256_madd_epi16(out_17_0, k__cospi_p15_p17);
                        const __m256i out_17_3 = _mm256_madd_epi16(out_17_1, k__cospi_p15_p17);
                        const __m256i out_09_2 = _mm256_madd_epi16(out_09_0, k__cospi_p23_p09);
                        const __m256i out_09_3 = _mm256_madd_epi16(out_09_1, k__cospi_p23_p09);
                        const __m256i out_25_2 = _mm256_madd_epi16(out_25_0, k__cospi_p07_p25);
                        const __m256i out_25_3 = _mm256_madd_epi16(out_25_1, k__cospi_p07_p25);
                        const __m256i out_07_2 = _mm256_madd_epi16(out_25_0, k__cospi_m25_p07);
                        const __m256i out_07_3 = _mm256_madd_epi16(out_25_1, k__cospi_m25_p07);
                        const __m256i out_23_2 = _mm256_madd_epi16(out_09_0, k__cospi_m09_p23);
                        const __m256i out_23_3 = _mm256_madd_epi16(out_09_1, k__cospi_m09_p23);
                        const __m256i out_15_2 = _mm256_madd_epi16(out_17_0, k__cospi_m17_p15);
                        const __m256i out_15_3 = _mm256_madd_epi16(out_17_1, k__cospi_m17_p15);
                        const __m256i out_31_2 = _mm256_madd_epi16(out_01_0, k__cospi_m01_p31);
                        const __m256i out_31_3 = _mm256_madd_epi16(out_01_1, k__cospi_m01_p31);
                        // dct_const_round_shift
                        const __m256i out_01_4 = _mm256_add_epi32(out_01_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_01_5 = _mm256_add_epi32(out_01_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_17_4 = _mm256_add_epi32(out_17_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_17_5 = _mm256_add_epi32(out_17_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_09_4 = _mm256_add_epi32(out_09_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_09_5 = _mm256_add_epi32(out_09_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_25_4 = _mm256_add_epi32(out_25_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_25_5 = _mm256_add_epi32(out_25_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_07_4 = _mm256_add_epi32(out_07_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_07_5 = _mm256_add_epi32(out_07_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_23_4 = _mm256_add_epi32(out_23_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_23_5 = _mm256_add_epi32(out_23_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_15_4 = _mm256_add_epi32(out_15_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_15_5 = _mm256_add_epi32(out_15_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_31_4 = _mm256_add_epi32(out_31_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_31_5 = _mm256_add_epi32(out_31_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_01_6 = _mm256_srai_epi32(out_01_4, DCT_CONST_BITS);
                        const __m256i out_01_7 = _mm256_srai_epi32(out_01_5, DCT_CONST_BITS);
                        const __m256i out_17_6 = _mm256_srai_epi32(out_17_4, DCT_CONST_BITS);
                        const __m256i out_17_7 = _mm256_srai_epi32(out_17_5, DCT_CONST_BITS);
                        const __m256i out_09_6 = _mm256_srai_epi32(out_09_4, DCT_CONST_BITS);
                        const __m256i out_09_7 = _mm256_srai_epi32(out_09_5, DCT_CONST_BITS);
                        const __m256i out_25_6 = _mm256_srai_epi32(out_25_4, DCT_CONST_BITS);
                        const __m256i out_25_7 = _mm256_srai_epi32(out_25_5, DCT_CONST_BITS);
                        const __m256i out_07_6 = _mm256_srai_epi32(out_07_4, DCT_CONST_BITS);
                        const __m256i out_07_7 = _mm256_srai_epi32(out_07_5, DCT_CONST_BITS);
                        const __m256i out_23_6 = _mm256_srai_epi32(out_23_4, DCT_CONST_BITS);
                        const __m256i out_23_7 = _mm256_srai_epi32(out_23_5, DCT_CONST_BITS);
                        const __m256i out_15_6 = _mm256_srai_epi32(out_15_4, DCT_CONST_BITS);
                        const __m256i out_15_7 = _mm256_srai_epi32(out_15_5, DCT_CONST_BITS);
                        const __m256i out_31_6 = _mm256_srai_epi32(out_31_4, DCT_CONST_BITS);
                        const __m256i out_31_7 = _mm256_srai_epi32(out_31_5, DCT_CONST_BITS);
                        // Combine
                        out[1]  = _mm256_packs_epi32(out_01_6, out_01_7);
                        out[17] = _mm256_packs_epi32(out_17_6, out_17_7);
                        out[9]  = _mm256_packs_epi32(out_09_6, out_09_7);
                        out[25] = _mm256_packs_epi32(out_25_6, out_25_7);
                        out[7]  = _mm256_packs_epi32(out_07_6, out_07_7);
                        out[23] = _mm256_packs_epi32(out_23_6, out_23_7);
                        out[15] = _mm256_packs_epi32(out_15_6, out_15_7);
                        out[31] = _mm256_packs_epi32(out_31_6, out_31_7);
                    }
                    {
                        const __m256i out_05_0 = _mm256_unpacklo_epi16(step1[20], step1[27]);
                        const __m256i out_05_1 = _mm256_unpackhi_epi16(step1[20], step1[27]);
                        const __m256i out_21_0 = _mm256_unpacklo_epi16(step1[21], step1[26]);
                        const __m256i out_21_1 = _mm256_unpackhi_epi16(step1[21], step1[26]);
                        const __m256i out_13_0 = _mm256_unpacklo_epi16(step1[22], step1[25]);
                        const __m256i out_13_1 = _mm256_unpackhi_epi16(step1[22], step1[25]);
                        const __m256i out_29_0 = _mm256_unpacklo_epi16(step1[23], step1[24]);
                        const __m256i out_29_1 = _mm256_unpackhi_epi16(step1[23], step1[24]);
                        const __m256i out_05_2 = _mm256_madd_epi16(out_05_0, k__cospi_p27_p05);
                        const __m256i out_05_3 = _mm256_madd_epi16(out_05_1, k__cospi_p27_p05);
                        const __m256i out_21_2 = _mm256_madd_epi16(out_21_0, k__cospi_p11_p21);
                        const __m256i out_21_3 = _mm256_madd_epi16(out_21_1, k__cospi_p11_p21);
                        const __m256i out_13_2 = _mm256_madd_epi16(out_13_0, k__cospi_p19_p13);
                        const __m256i out_13_3 = _mm256_madd_epi16(out_13_1, k__cospi_p19_p13);
                        const __m256i out_29_2 = _mm256_madd_epi16(out_29_0, k__cospi_p03_p29);
                        const __m256i out_29_3 = _mm256_madd_epi16(out_29_1, k__cospi_p03_p29);
                        const __m256i out_03_2 = _mm256_madd_epi16(out_29_0, k__cospi_m29_p03);
                        const __m256i out_03_3 = _mm256_madd_epi16(out_29_1, k__cospi_m29_p03);
                        const __m256i out_19_2 = _mm256_madd_epi16(out_13_0, k__cospi_m13_p19);
                        const __m256i out_19_3 = _mm256_madd_epi16(out_13_1, k__cospi_m13_p19);
                        const __m256i out_11_2 = _mm256_madd_epi16(out_21_0, k__cospi_m21_p11);
                        const __m256i out_11_3 = _mm256_madd_epi16(out_21_1, k__cospi_m21_p11);
                        const __m256i out_27_2 = _mm256_madd_epi16(out_05_0, k__cospi_m05_p27);
                        const __m256i out_27_3 = _mm256_madd_epi16(out_05_1, k__cospi_m05_p27);
                        // dct_const_round_shift
                        const __m256i out_05_4 = _mm256_add_epi32(out_05_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_05_5 = _mm256_add_epi32(out_05_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_21_4 = _mm256_add_epi32(out_21_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_21_5 = _mm256_add_epi32(out_21_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_13_4 = _mm256_add_epi32(out_13_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_13_5 = _mm256_add_epi32(out_13_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_29_4 = _mm256_add_epi32(out_29_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_29_5 = _mm256_add_epi32(out_29_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_03_4 = _mm256_add_epi32(out_03_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_03_5 = _mm256_add_epi32(out_03_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_19_4 = _mm256_add_epi32(out_19_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_19_5 = _mm256_add_epi32(out_19_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_11_4 = _mm256_add_epi32(out_11_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_11_5 = _mm256_add_epi32(out_11_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_27_4 = _mm256_add_epi32(out_27_2, k__DCT_CONST_ROUNDING);
                        const __m256i out_27_5 = _mm256_add_epi32(out_27_3, k__DCT_CONST_ROUNDING);
                        const __m256i out_05_6 = _mm256_srai_epi32(out_05_4, DCT_CONST_BITS);
                        const __m256i out_05_7 = _mm256_srai_epi32(out_05_5, DCT_CONST_BITS);
                        const __m256i out_21_6 = _mm256_srai_epi32(out_21_4, DCT_CONST_BITS);
                        const __m256i out_21_7 = _mm256_srai_epi32(out_21_5, DCT_CONST_BITS);
                        const __m256i out_13_6 = _mm256_srai_epi32(out_13_4, DCT_CONST_BITS);
                        const __m256i out_13_7 = _mm256_srai_epi32(out_13_5, DCT_CONST_BITS);
                        const __m256i out_29_6 = _mm256_srai_epi32(out_29_4, DCT_CONST_BITS);
                        const __m256i out_29_7 = _mm256_srai_epi32(out_29_5, DCT_CONST_BITS);
                        const __m256i out_03_6 = _mm256_srai_epi32(out_03_4, DCT_CONST_BITS);
                        const __m256i out_03_7 = _mm256_srai_epi32(out_03_5, DCT_CONST_BITS);
                        const __m256i out_19_6 = _mm256_srai_epi32(out_19_4, DCT_CONST_BITS);
                        const __m256i out_19_7 = _mm256_srai_epi32(out_19_5, DCT_CONST_BITS);
                        const __m256i out_11_6 = _mm256_srai_epi32(out_11_4, DCT_CONST_BITS);
                        const __m256i out_11_7 = _mm256_srai_epi32(out_11_5, DCT_CONST_BITS);
                        const __m256i out_27_6 = _mm256_srai_epi32(out_27_4, DCT_CONST_BITS);
                        const __m256i out_27_7 = _mm256_srai_epi32(out_27_5, DCT_CONST_BITS);
                        // Combine
                        out[5]  = _mm256_packs_epi32(out_05_6, out_05_7);
                        out[21] = _mm256_packs_epi32(out_21_6, out_21_7);
                        out[13] = _mm256_packs_epi32(out_13_6, out_13_7);
                        out[29] = _mm256_packs_epi32(out_29_6, out_29_7);
                        out[3]  = _mm256_packs_epi32(out_03_6, out_03_7);
                        out[19] = _mm256_packs_epi32(out_19_6, out_19_7);
                        out[11] = _mm256_packs_epi32(out_11_6, out_11_7);
                        out[27] = _mm256_packs_epi32(out_27_6, out_27_7);
                    }
//#if FDCT32x32_HIGH_PRECISION
                //    } else {
                //        __m256i lstep1[64], lstep2[64], lstep3[64];
                //        __m256i u[32], v[32], sign[16];
                //        const __m256i K32One = _mm256_set1_epi32(1);
                //        // start using 32-bit operations
                //        // stage 3
                //        {
                //            // expanding to 32-bit length priori to addition operations
                //            lstep2[0]  = _mm256_unpacklo_epi16(step2[0], kZero);
                //            lstep2[1]  = _mm256_unpackhi_epi16(step2[0], kZero);
                //            lstep2[2]  = _mm256_unpacklo_epi16(step2[1], kZero);
                //            lstep2[3]  = _mm256_unpackhi_epi16(step2[1], kZero);
                //            lstep2[4]  = _mm256_unpacklo_epi16(step2[2], kZero);
                //            lstep2[5]  = _mm256_unpackhi_epi16(step2[2], kZero);
                //            lstep2[6]  = _mm256_unpacklo_epi16(step2[3], kZero);
                //            lstep2[7]  = _mm256_unpackhi_epi16(step2[3], kZero);
                //            lstep2[8]  = _mm256_unpacklo_epi16(step2[4], kZero);
                //            lstep2[9]  = _mm256_unpackhi_epi16(step2[4], kZero);
                //            lstep2[10] = _mm256_unpacklo_epi16(step2[5], kZero);
                //            lstep2[11] = _mm256_unpackhi_epi16(step2[5], kZero);
                //            lstep2[12] = _mm256_unpacklo_epi16(step2[6], kZero);
                //            lstep2[13] = _mm256_unpackhi_epi16(step2[6], kZero);
                //            lstep2[14] = _mm256_unpacklo_epi16(step2[7], kZero);
                //            lstep2[15] = _mm256_unpackhi_epi16(step2[7], kZero);
                //            lstep2[0]  = _mm256_madd_epi16(lstep2[0], kOne);
                //            lstep2[1]  = _mm256_madd_epi16(lstep2[1], kOne);
                //            lstep2[2]  = _mm256_madd_epi16(lstep2[2], kOne);
                //            lstep2[3]  = _mm256_madd_epi16(lstep2[3], kOne);
                //            lstep2[4]  = _mm256_madd_epi16(lstep2[4], kOne);
                //            lstep2[5]  = _mm256_madd_epi16(lstep2[5], kOne);
                //            lstep2[6]  = _mm256_madd_epi16(lstep2[6], kOne);
                //            lstep2[7]  = _mm256_madd_epi16(lstep2[7], kOne);
                //            lstep2[8]  = _mm256_madd_epi16(lstep2[8], kOne);
                //            lstep2[9]  = _mm256_madd_epi16(lstep2[9], kOne);
                //            lstep2[10] = _mm256_madd_epi16(lstep2[10], kOne);
                //            lstep2[11] = _mm256_madd_epi16(lstep2[11], kOne);
                //            lstep2[12] = _mm256_madd_epi16(lstep2[12], kOne);
                //            lstep2[13] = _mm256_madd_epi16(lstep2[13], kOne);
                //            lstep2[14] = _mm256_madd_epi16(lstep2[14], kOne);
                //            lstep2[15] = _mm256_madd_epi16(lstep2[15], kOne);

                //            lstep3[0]  = _mm256_add_epi32(lstep2[14], lstep2[0]);
                //            lstep3[1]  = _mm256_add_epi32(lstep2[15], lstep2[1]);
                //            lstep3[2]  = _mm256_add_epi32(lstep2[12], lstep2[2]);
                //            lstep3[3]  = _mm256_add_epi32(lstep2[13], lstep2[3]);
                //            lstep3[4]  = _mm256_add_epi32(lstep2[10], lstep2[4]);
                //            lstep3[5]  = _mm256_add_epi32(lstep2[11], lstep2[5]);
                //            lstep3[6]  = _mm256_add_epi32(lstep2[8], lstep2[6]);
                //            lstep3[7]  = _mm256_add_epi32(lstep2[9], lstep2[7]);
                //            lstep3[8]  = _mm256_sub_epi32(lstep2[6], lstep2[8]);
                //            lstep3[9]  = _mm256_sub_epi32(lstep2[7], lstep2[9]);
                //            lstep3[10] = _mm256_sub_epi32(lstep2[4], lstep2[10]);
                //            lstep3[11] = _mm256_sub_epi32(lstep2[5], lstep2[11]);
                //            lstep3[12] = _mm256_sub_epi32(lstep2[2], lstep2[12]);
                //            lstep3[13] = _mm256_sub_epi32(lstep2[3], lstep2[13]);
                //            lstep3[14] = _mm256_sub_epi32(lstep2[0], lstep2[14]);
                //            lstep3[15] = _mm256_sub_epi32(lstep2[1], lstep2[15]);
                //        }
                //        {
                //            const __m256i s3_10_0 = _mm256_unpacklo_epi16(step2[13], step2[10]);
                //            const __m256i s3_10_1 = _mm256_unpackhi_epi16(step2[13], step2[10]);
                //            const __m256i s3_11_0 = _mm256_unpacklo_epi16(step2[12], step2[11]);
                //            const __m256i s3_11_1 = _mm256_unpackhi_epi16(step2[12], step2[11]);
                //            const __m256i s3_10_2 = _mm256_madd_epi16(s3_10_0, k__cospi_p16_m16);
                //            const __m256i s3_10_3 = _mm256_madd_epi16(s3_10_1, k__cospi_p16_m16);
                //            const __m256i s3_11_2 = _mm256_madd_epi16(s3_11_0, k__cospi_p16_m16);
                //            const __m256i s3_11_3 = _mm256_madd_epi16(s3_11_1, k__cospi_p16_m16);
                //            const __m256i s3_12_2 = _mm256_madd_epi16(s3_11_0, k__cospi_p16_p16);
                //            const __m256i s3_12_3 = _mm256_madd_epi16(s3_11_1, k__cospi_p16_p16);
                //            const __m256i s3_13_2 = _mm256_madd_epi16(s3_10_0, k__cospi_p16_p16);
                //            const __m256i s3_13_3 = _mm256_madd_epi16(s3_10_1, k__cospi_p16_p16);
                //            // dct_const_round_shift
                //            const __m256i s3_10_4 = _mm256_add_epi32(s3_10_2, k__DCT_CONST_ROUNDING);
                //            const __m256i s3_10_5 = _mm256_add_epi32(s3_10_3, k__DCT_CONST_ROUNDING);
                //            const __m256i s3_11_4 = _mm256_add_epi32(s3_11_2, k__DCT_CONST_ROUNDING);
                //            const __m256i s3_11_5 = _mm256_add_epi32(s3_11_3, k__DCT_CONST_ROUNDING);
                //            const __m256i s3_12_4 = _mm256_add_epi32(s3_12_2, k__DCT_CONST_ROUNDING);
                //            const __m256i s3_12_5 = _mm256_add_epi32(s3_12_3, k__DCT_CONST_ROUNDING);
                //            const __m256i s3_13_4 = _mm256_add_epi32(s3_13_2, k__DCT_CONST_ROUNDING);
                //            const __m256i s3_13_5 = _mm256_add_epi32(s3_13_3, k__DCT_CONST_ROUNDING);
                //            lstep3[20] = _mm256_srai_epi32(s3_10_4, DCT_CONST_BITS);
                //            lstep3[21] = _mm256_srai_epi32(s3_10_5, DCT_CONST_BITS);
                //            lstep3[22] = _mm256_srai_epi32(s3_11_4, DCT_CONST_BITS);
                //            lstep3[23] = _mm256_srai_epi32(s3_11_5, DCT_CONST_BITS);
                //            lstep3[24] = _mm256_srai_epi32(s3_12_4, DCT_CONST_BITS);
                //            lstep3[25] = _mm256_srai_epi32(s3_12_5, DCT_CONST_BITS);
                //            lstep3[26] = _mm256_srai_epi32(s3_13_4, DCT_CONST_BITS);
                //            lstep3[27] = _mm256_srai_epi32(s3_13_5, DCT_CONST_BITS);
                //        }
                //        {
                //            lstep2[40] = _mm256_unpacklo_epi16(step2[20], kZero);
                //            lstep2[41] = _mm256_unpackhi_epi16(step2[20], kZero);
                //            lstep2[42] = _mm256_unpacklo_epi16(step2[21], kZero);
                //            lstep2[43] = _mm256_unpackhi_epi16(step2[21], kZero);
                //            lstep2[44] = _mm256_unpacklo_epi16(step2[22], kZero);
                //            lstep2[45] = _mm256_unpackhi_epi16(step2[22], kZero);
                //            lstep2[46] = _mm256_unpacklo_epi16(step2[23], kZero);
                //            lstep2[47] = _mm256_unpackhi_epi16(step2[23], kZero);
                //            lstep2[48] = _mm256_unpacklo_epi16(step2[24], kZero);
                //            lstep2[49] = _mm256_unpackhi_epi16(step2[24], kZero);
                //            lstep2[50] = _mm256_unpacklo_epi16(step2[25], kZero);
                //            lstep2[51] = _mm256_unpackhi_epi16(step2[25], kZero);
                //            lstep2[52] = _mm256_unpacklo_epi16(step2[26], kZero);
                //            lstep2[53] = _mm256_unpackhi_epi16(step2[26], kZero);
                //            lstep2[54] = _mm256_unpacklo_epi16(step2[27], kZero);
                //            lstep2[55] = _mm256_unpackhi_epi16(step2[27], kZero);
                //            lstep2[40] = _mm256_madd_epi16(lstep2[40], kOne);
                //            lstep2[41] = _mm256_madd_epi16(lstep2[41], kOne);
                //            lstep2[42] = _mm256_madd_epi16(lstep2[42], kOne);
                //            lstep2[43] = _mm256_madd_epi16(lstep2[43], kOne);
                //            lstep2[44] = _mm256_madd_epi16(lstep2[44], kOne);
                //            lstep2[45] = _mm256_madd_epi16(lstep2[45], kOne);
                //            lstep2[46] = _mm256_madd_epi16(lstep2[46], kOne);
                //            lstep2[47] = _mm256_madd_epi16(lstep2[47], kOne);
                //            lstep2[48] = _mm256_madd_epi16(lstep2[48], kOne);
                //            lstep2[49] = _mm256_madd_epi16(lstep2[49], kOne);
                //            lstep2[50] = _mm256_madd_epi16(lstep2[50], kOne);
                //            lstep2[51] = _mm256_madd_epi16(lstep2[51], kOne);
                //            lstep2[52] = _mm256_madd_epi16(lstep2[52], kOne);
                //            lstep2[53] = _mm256_madd_epi16(lstep2[53], kOne);
                //            lstep2[54] = _mm256_madd_epi16(lstep2[54], kOne);
                //            lstep2[55] = _mm256_madd_epi16(lstep2[55], kOne);

                //            lstep1[32] = _mm256_unpacklo_epi16(step1[16], kZero);
                //            lstep1[33] = _mm256_unpackhi_epi16(step1[16], kZero);
                //            lstep1[34] = _mm256_unpacklo_epi16(step1[17], kZero);
                //            lstep1[35] = _mm256_unpackhi_epi16(step1[17], kZero);
                //            lstep1[36] = _mm256_unpacklo_epi16(step1[18], kZero);
                //            lstep1[37] = _mm256_unpackhi_epi16(step1[18], kZero);
                //            lstep1[38] = _mm256_unpacklo_epi16(step1[19], kZero);
                //            lstep1[39] = _mm256_unpackhi_epi16(step1[19], kZero);
                //            lstep1[56] = _mm256_unpacklo_epi16(step1[28], kZero);
                //            lstep1[57] = _mm256_unpackhi_epi16(step1[28], kZero);
                //            lstep1[58] = _mm256_unpacklo_epi16(step1[29], kZero);
                //            lstep1[59] = _mm256_unpackhi_epi16(step1[29], kZero);
                //            lstep1[60] = _mm256_unpacklo_epi16(step1[30], kZero);
                //            lstep1[61] = _mm256_unpackhi_epi16(step1[30], kZero);
                //            lstep1[62] = _mm256_unpacklo_epi16(step1[31], kZero);
                //            lstep1[63] = _mm256_unpackhi_epi16(step1[31], kZero);
                //            lstep1[32] = _mm256_madd_epi16(lstep1[32], kOne);
                //            lstep1[33] = _mm256_madd_epi16(lstep1[33], kOne);
                //            lstep1[34] = _mm256_madd_epi16(lstep1[34], kOne);
                //            lstep1[35] = _mm256_madd_epi16(lstep1[35], kOne);
                //            lstep1[36] = _mm256_madd_epi16(lstep1[36], kOne);
                //            lstep1[37] = _mm256_madd_epi16(lstep1[37], kOne);
                //            lstep1[38] = _mm256_madd_epi16(lstep1[38], kOne);
                //            lstep1[39] = _mm256_madd_epi16(lstep1[39], kOne);
                //            lstep1[56] = _mm256_madd_epi16(lstep1[56], kOne);
                //            lstep1[57] = _mm256_madd_epi16(lstep1[57], kOne);
                //            lstep1[58] = _mm256_madd_epi16(lstep1[58], kOne);
                //            lstep1[59] = _mm256_madd_epi16(lstep1[59], kOne);
                //            lstep1[60] = _mm256_madd_epi16(lstep1[60], kOne);
                //            lstep1[61] = _mm256_madd_epi16(lstep1[61], kOne);
                //            lstep1[62] = _mm256_madd_epi16(lstep1[62], kOne);
                //            lstep1[63] = _mm256_madd_epi16(lstep1[63], kOne);

                //            lstep3[32] = _mm256_add_epi32(lstep2[46], lstep1[32]);
                //            lstep3[33] = _mm256_add_epi32(lstep2[47], lstep1[33]);

                //            lstep3[34] = _mm256_add_epi32(lstep2[44], lstep1[34]);
                //            lstep3[35] = _mm256_add_epi32(lstep2[45], lstep1[35]);
                //            lstep3[36] = _mm256_add_epi32(lstep2[42], lstep1[36]);
                //            lstep3[37] = _mm256_add_epi32(lstep2[43], lstep1[37]);
                //            lstep3[38] = _mm256_add_epi32(lstep2[40], lstep1[38]);
                //            lstep3[39] = _mm256_add_epi32(lstep2[41], lstep1[39]);
                //            lstep3[40] = _mm256_sub_epi32(lstep1[38], lstep2[40]);
                //            lstep3[41] = _mm256_sub_epi32(lstep1[39], lstep2[41]);
                //            lstep3[42] = _mm256_sub_epi32(lstep1[36], lstep2[42]);
                //            lstep3[43] = _mm256_sub_epi32(lstep1[37], lstep2[43]);
                //            lstep3[44] = _mm256_sub_epi32(lstep1[34], lstep2[44]);
                //            lstep3[45] = _mm256_sub_epi32(lstep1[35], lstep2[45]);
                //            lstep3[46] = _mm256_sub_epi32(lstep1[32], lstep2[46]);
                //            lstep3[47] = _mm256_sub_epi32(lstep1[33], lstep2[47]);
                //            lstep3[48] = _mm256_sub_epi32(lstep1[62], lstep2[48]);
                //            lstep3[49] = _mm256_sub_epi32(lstep1[63], lstep2[49]);
                //            lstep3[50] = _mm256_sub_epi32(lstep1[60], lstep2[50]);
                //            lstep3[51] = _mm256_sub_epi32(lstep1[61], lstep2[51]);
                //            lstep3[52] = _mm256_sub_epi32(lstep1[58], lstep2[52]);
                //            lstep3[53] = _mm256_sub_epi32(lstep1[59], lstep2[53]);
                //            lstep3[54] = _mm256_sub_epi32(lstep1[56], lstep2[54]);
                //            lstep3[55] = _mm256_sub_epi32(lstep1[57], lstep2[55]);
                //            lstep3[56] = _mm256_add_epi32(lstep2[54], lstep1[56]);
                //            lstep3[57] = _mm256_add_epi32(lstep2[55], lstep1[57]);
                //            lstep3[58] = _mm256_add_epi32(lstep2[52], lstep1[58]);
                //            lstep3[59] = _mm256_add_epi32(lstep2[53], lstep1[59]);
                //            lstep3[60] = _mm256_add_epi32(lstep2[50], lstep1[60]);
                //            lstep3[61] = _mm256_add_epi32(lstep2[51], lstep1[61]);
                //            lstep3[62] = _mm256_add_epi32(lstep2[48], lstep1[62]);
                //            lstep3[63] = _mm256_add_epi32(lstep2[49], lstep1[63]);
                //        }

                //        // stage 4
                //        {
                //            // expanding to 32-bit length priori to addition operations
                //            lstep2[16] = _mm256_unpacklo_epi16(step2[8], kZero);
                //            lstep2[17] = _mm256_unpackhi_epi16(step2[8], kZero);
                //            lstep2[18] = _mm256_unpacklo_epi16(step2[9], kZero);
                //            lstep2[19] = _mm256_unpackhi_epi16(step2[9], kZero);
                //            lstep2[28] = _mm256_unpacklo_epi16(step2[14], kZero);
                //            lstep2[29] = _mm256_unpackhi_epi16(step2[14], kZero);
                //            lstep2[30] = _mm256_unpacklo_epi16(step2[15], kZero);
                //            lstep2[31] = _mm256_unpackhi_epi16(step2[15], kZero);
                //            lstep2[16] = _mm256_madd_epi16(lstep2[16], kOne);
                //            lstep2[17] = _mm256_madd_epi16(lstep2[17], kOne);
                //            lstep2[18] = _mm256_madd_epi16(lstep2[18], kOne);
                //            lstep2[19] = _mm256_madd_epi16(lstep2[19], kOne);
                //            lstep2[28] = _mm256_madd_epi16(lstep2[28], kOne);
                //            lstep2[29] = _mm256_madd_epi16(lstep2[29], kOne);
                //            lstep2[30] = _mm256_madd_epi16(lstep2[30], kOne);
                //            lstep2[31] = _mm256_madd_epi16(lstep2[31], kOne);

                //            lstep1[0]  = _mm256_add_epi32(lstep3[6], lstep3[0]);
                //            lstep1[1]  = _mm256_add_epi32(lstep3[7], lstep3[1]);
                //            lstep1[2]  = _mm256_add_epi32(lstep3[4], lstep3[2]);
                //            lstep1[3]  = _mm256_add_epi32(lstep3[5], lstep3[3]);
                //            lstep1[4]  = _mm256_sub_epi32(lstep3[2], lstep3[4]);
                //            lstep1[5]  = _mm256_sub_epi32(lstep3[3], lstep3[5]);
                //            lstep1[6]  = _mm256_sub_epi32(lstep3[0], lstep3[6]);
                //            lstep1[7]  = _mm256_sub_epi32(lstep3[1], lstep3[7]);
                //            lstep1[16] = _mm256_add_epi32(lstep3[22], lstep2[16]);
                //            lstep1[17] = _mm256_add_epi32(lstep3[23], lstep2[17]);
                //            lstep1[18] = _mm256_add_epi32(lstep3[20], lstep2[18]);
                //            lstep1[19] = _mm256_add_epi32(lstep3[21], lstep2[19]);
                //            lstep1[20] = _mm256_sub_epi32(lstep2[18], lstep3[20]);
                //            lstep1[21] = _mm256_sub_epi32(lstep2[19], lstep3[21]);
                //            lstep1[22] = _mm256_sub_epi32(lstep2[16], lstep3[22]);
                //            lstep1[23] = _mm256_sub_epi32(lstep2[17], lstep3[23]);
                //            lstep1[24] = _mm256_sub_epi32(lstep2[30], lstep3[24]);
                //            lstep1[25] = _mm256_sub_epi32(lstep2[31], lstep3[25]);
                //            lstep1[26] = _mm256_sub_epi32(lstep2[28], lstep3[26]);
                //            lstep1[27] = _mm256_sub_epi32(lstep2[29], lstep3[27]);
                //            lstep1[28] = _mm256_add_epi32(lstep3[26], lstep2[28]);
                //            lstep1[29] = _mm256_add_epi32(lstep3[27], lstep2[29]);
                //            lstep1[30] = _mm256_add_epi32(lstep3[24], lstep2[30]);
                //            lstep1[31] = _mm256_add_epi32(lstep3[25], lstep2[31]);
                //        }
                //        {
                //            // to be continued...
                //            //
                //            const __m256i k32_p16_p16 = pair_set_epi32(cospi_16_64, cospi_16_64);
                //            const __m256i k32_p16_m16 = pair_set_epi32(cospi_16_64, -cospi_16_64);

                //            u[0] = _mm256_unpacklo_epi32(lstep3[12], lstep3[10]);
                //            u[1] = _mm256_unpackhi_epi32(lstep3[12], lstep3[10]);
                //            u[2] = _mm256_unpacklo_epi32(lstep3[13], lstep3[11]);
                //            u[3] = _mm256_unpackhi_epi32(lstep3[13], lstep3[11]);

                //            // TODO(jingning): manually inline k_madd_epi32_ to further hide
                //            // instruction latency.
                //            v[0] = k_madd_epi32(u[0], k32_p16_m16);
                //            v[1] = k_madd_epi32(u[1], k32_p16_m16);
                //            v[2] = k_madd_epi32(u[2], k32_p16_m16);
                //            v[3] = k_madd_epi32(u[3], k32_p16_m16);
                //            v[4] = k_madd_epi32(u[0], k32_p16_p16);
                //            v[5] = k_madd_epi32(u[1], k32_p16_p16);
                //            v[6] = k_madd_epi32(u[2], k32_p16_p16);
                //            v[7] = k_madd_epi32(u[3], k32_p16_p16);
                //            u[0] = k_packs_epi64(v[0], v[1]);
                //            u[1] = k_packs_epi64(v[2], v[3]);
                //            u[2] = k_packs_epi64(v[4], v[5]);
                //            u[3] = k_packs_epi64(v[6], v[7]);

                //            v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                //            v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                //            v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                //            v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);

                //            lstep1[10] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                //            lstep1[11] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                //            lstep1[12] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                //            lstep1[13] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                //        }
                //        {
                //            const __m256i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);
                //            const __m256i k32_m24_m08 = pair_set_epi32(-cospi_24_64, -cospi_8_64);
                //            const __m256i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);

                //            u[0]  = _mm256_unpacklo_epi32(lstep3[36], lstep3[58]);
                //            u[1]  = _mm256_unpackhi_epi32(lstep3[36], lstep3[58]);
                //            u[2]  = _mm256_unpacklo_epi32(lstep3[37], lstep3[59]);
                //            u[3]  = _mm256_unpackhi_epi32(lstep3[37], lstep3[59]);
                //            u[4]  = _mm256_unpacklo_epi32(lstep3[38], lstep3[56]);
                //            u[5]  = _mm256_unpackhi_epi32(lstep3[38], lstep3[56]);
                //            u[6]  = _mm256_unpacklo_epi32(lstep3[39], lstep3[57]);
                //            u[7]  = _mm256_unpackhi_epi32(lstep3[39], lstep3[57]);
                //            u[8]  = _mm256_unpacklo_epi32(lstep3[40], lstep3[54]);
                //            u[9]  = _mm256_unpackhi_epi32(lstep3[40], lstep3[54]);
                //            u[10] = _mm256_unpacklo_epi32(lstep3[41], lstep3[55]);
                //            u[11] = _mm256_unpackhi_epi32(lstep3[41], lstep3[55]);
                //            u[12] = _mm256_unpacklo_epi32(lstep3[42], lstep3[52]);
                //            u[13] = _mm256_unpackhi_epi32(lstep3[42], lstep3[52]);
                //            u[14] = _mm256_unpacklo_epi32(lstep3[43], lstep3[53]);
                //            u[15] = _mm256_unpackhi_epi32(lstep3[43], lstep3[53]);

                //            v[0]  = k_madd_epi32(u[0], k32_m08_p24);
                //            v[1]  = k_madd_epi32(u[1], k32_m08_p24);
                //            v[2]  = k_madd_epi32(u[2], k32_m08_p24);
                //            v[3]  = k_madd_epi32(u[3], k32_m08_p24);
                //            v[4]  = k_madd_epi32(u[4], k32_m08_p24);
                //            v[5]  = k_madd_epi32(u[5], k32_m08_p24);
                //            v[6]  = k_madd_epi32(u[6], k32_m08_p24);
                //            v[7]  = k_madd_epi32(u[7], k32_m08_p24);
                //            v[8]  = k_madd_epi32(u[8], k32_m24_m08);
                //            v[9]  = k_madd_epi32(u[9], k32_m24_m08);
                //            v[10] = k_madd_epi32(u[10], k32_m24_m08);
                //            v[11] = k_madd_epi32(u[11], k32_m24_m08);
                //            v[12] = k_madd_epi32(u[12], k32_m24_m08);
                //            v[13] = k_madd_epi32(u[13], k32_m24_m08);
                //            v[14] = k_madd_epi32(u[14], k32_m24_m08);
                //            v[15] = k_madd_epi32(u[15], k32_m24_m08);
                //            v[16] = k_madd_epi32(u[12], k32_m08_p24);
                //            v[17] = k_madd_epi32(u[13], k32_m08_p24);
                //            v[18] = k_madd_epi32(u[14], k32_m08_p24);
                //            v[19] = k_madd_epi32(u[15], k32_m08_p24);
                //            v[20] = k_madd_epi32(u[8], k32_m08_p24);
                //            v[21] = k_madd_epi32(u[9], k32_m08_p24);
                //            v[22] = k_madd_epi32(u[10], k32_m08_p24);
                //            v[23] = k_madd_epi32(u[11], k32_m08_p24);
                //            v[24] = k_madd_epi32(u[4], k32_p24_p08);
                //            v[25] = k_madd_epi32(u[5], k32_p24_p08);
                //            v[26] = k_madd_epi32(u[6], k32_p24_p08);
                //            v[27] = k_madd_epi32(u[7], k32_p24_p08);
                //            v[28] = k_madd_epi32(u[0], k32_p24_p08);
                //            v[29] = k_madd_epi32(u[1], k32_p24_p08);
                //            v[30] = k_madd_epi32(u[2], k32_p24_p08);
                //            v[31] = k_madd_epi32(u[3], k32_p24_p08);

                //            u[0]  = k_packs_epi64(v[0], v[1]);
                //            u[1]  = k_packs_epi64(v[2], v[3]);
                //            u[2]  = k_packs_epi64(v[4], v[5]);
                //            u[3]  = k_packs_epi64(v[6], v[7]);
                //            u[4]  = k_packs_epi64(v[8], v[9]);
                //            u[5]  = k_packs_epi64(v[10], v[11]);
                //            u[6]  = k_packs_epi64(v[12], v[13]);
                //            u[7]  = k_packs_epi64(v[14], v[15]);
                //            u[8]  = k_packs_epi64(v[16], v[17]);
                //            u[9]  = k_packs_epi64(v[18], v[19]);
                //            u[10] = k_packs_epi64(v[20], v[21]);
                //            u[11] = k_packs_epi64(v[22], v[23]);
                //            u[12] = k_packs_epi64(v[24], v[25]);
                //            u[13] = k_packs_epi64(v[26], v[27]);
                //            u[14] = k_packs_epi64(v[28], v[29]);
                //            u[15] = k_packs_epi64(v[30], v[31]);

                //            v[0]  = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                //            v[1]  = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                //            v[2]  = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                //            v[3]  = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                //            v[4]  = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                //            v[5]  = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                //            v[6]  = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                //            v[7]  = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                //            v[8]  = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                //            v[9]  = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                //            v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                //            v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                //            v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                //            v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                //            v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                //            v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                //            lstep1[36] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                //            lstep1[37] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                //            lstep1[38] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                //            lstep1[39] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                //            lstep1[40] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                //            lstep1[41] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                //            lstep1[42] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                //            lstep1[43] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
                //            lstep1[52] = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
                //            lstep1[53] = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
                //            lstep1[54] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
                //            lstep1[55] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
                //            lstep1[56] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
                //            lstep1[57] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
                //            lstep1[58] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
                //            lstep1[59] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);
                //        }
                //    // stage 5
                //    {
                //        lstep2[8]  = _mm256_add_epi32(lstep1[10], lstep3[8]);
                //        lstep2[9]  = _mm256_add_epi32(lstep1[11], lstep3[9]);
                //        lstep2[10] = _mm256_sub_epi32(lstep3[8], lstep1[10]);
                //        lstep2[11] = _mm256_sub_epi32(lstep3[9], lstep1[11]);
                //        lstep2[12] = _mm256_sub_epi32(lstep3[14], lstep1[12]);
                //        lstep2[13] = _mm256_sub_epi32(lstep3[15], lstep1[13]);
                //        lstep2[14] = _mm256_add_epi32(lstep1[12], lstep3[14]);
                //        lstep2[15] = _mm256_add_epi32(lstep1[13], lstep3[15]);
                //    }
                //    {
                //        const __m256i k32_p16_p16 = pair_set_epi32(cospi_16_64, cospi_16_64);
                //        const __m256i k32_p16_m16 = pair_set_epi32(cospi_16_64, -cospi_16_64);
                //        const __m256i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);
                //        const __m256i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);

                //        u[0] = _mm256_unpacklo_epi32(lstep1[0], lstep1[2]);
                //        u[1] = _mm256_unpackhi_epi32(lstep1[0], lstep1[2]);
                //        u[2] = _mm256_unpacklo_epi32(lstep1[1], lstep1[3]);
                //        u[3] = _mm256_unpackhi_epi32(lstep1[1], lstep1[3]);
                //        u[4] = _mm256_unpacklo_epi32(lstep1[4], lstep1[6]);
                //        u[5] = _mm256_unpackhi_epi32(lstep1[4], lstep1[6]);
                //        u[6] = _mm256_unpacklo_epi32(lstep1[5], lstep1[7]);
                //        u[7] = _mm256_unpackhi_epi32(lstep1[5], lstep1[7]);

                //        // TODO(jingning): manually inline k_madd_epi32_ to further hide
                //        // instruction latency.
                //        v[0] = k_madd_epi32(u[0], k32_p16_p16);
                //        v[1] = k_madd_epi32(u[1], k32_p16_p16);
                //        v[2] = k_madd_epi32(u[2], k32_p16_p16);
                //        v[3] = k_madd_epi32(u[3], k32_p16_p16);
                //        v[4] = k_madd_epi32(u[0], k32_p16_m16);
                //        v[5] = k_madd_epi32(u[1], k32_p16_m16);
                //        v[6] = k_madd_epi32(u[2], k32_p16_m16);
                //        v[7] = k_madd_epi32(u[3], k32_p16_m16);
                //        v[8] = k_madd_epi32(u[4], k32_p24_p08);
                //        v[9] = k_madd_epi32(u[5], k32_p24_p08);
                //        v[10] = k_madd_epi32(u[6], k32_p24_p08);
                //        v[11] = k_madd_epi32(u[7], k32_p24_p08);
                //        v[12] = k_madd_epi32(u[4], k32_m08_p24);
                //        v[13] = k_madd_epi32(u[5], k32_m08_p24);
                //        v[14] = k_madd_epi32(u[6], k32_m08_p24);
                //        v[15] = k_madd_epi32(u[7], k32_m08_p24);

                //        u[0] = k_packs_epi64(v[0], v[1]);
                //        u[1] = k_packs_epi64(v[2], v[3]);
                //        u[2] = k_packs_epi64(v[4], v[5]);
                //        u[3] = k_packs_epi64(v[6], v[7]);
                //        u[4] = k_packs_epi64(v[8], v[9]);
                //        u[5] = k_packs_epi64(v[10], v[11]);
                //        u[6] = k_packs_epi64(v[12], v[13]);
                //        u[7] = k_packs_epi64(v[14], v[15]);

                //        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                //        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                //        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                //        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                //        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                //        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                //        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                //        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);

                //        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                //        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                //        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                //        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                //        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                //        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                //        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                //        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);

                //        sign[0] = _mm256_cmpgt_epi32(kZero, u[0]);
                //        sign[1] = _mm256_cmpgt_epi32(kZero, u[1]);
                //        sign[2] = _mm256_cmpgt_epi32(kZero, u[2]);
                //        sign[3] = _mm256_cmpgt_epi32(kZero, u[3]);
                //        sign[4] = _mm256_cmpgt_epi32(kZero, u[4]);
                //        sign[5] = _mm256_cmpgt_epi32(kZero, u[5]);
                //        sign[6] = _mm256_cmpgt_epi32(kZero, u[6]);
                //        sign[7] = _mm256_cmpgt_epi32(kZero, u[7]);

                //        u[0] = _mm256_sub_epi32(u[0], sign[0]);
                //        u[1] = _mm256_sub_epi32(u[1], sign[1]);
                //        u[2] = _mm256_sub_epi32(u[2], sign[2]);
                //        u[3] = _mm256_sub_epi32(u[3], sign[3]);
                //        u[4] = _mm256_sub_epi32(u[4], sign[4]);
                //        u[5] = _mm256_sub_epi32(u[5], sign[5]);
                //        u[6] = _mm256_sub_epi32(u[6], sign[6]);
                //        u[7] = _mm256_sub_epi32(u[7], sign[7]);

                //        u[0] = _mm256_add_epi32(u[0], K32One);
                //        u[1] = _mm256_add_epi32(u[1], K32One);
                //        u[2] = _mm256_add_epi32(u[2], K32One);
                //        u[3] = _mm256_add_epi32(u[3], K32One);
                //        u[4] = _mm256_add_epi32(u[4], K32One);
                //        u[5] = _mm256_add_epi32(u[5], K32One);
                //        u[6] = _mm256_add_epi32(u[6], K32One);
                //        u[7] = _mm256_add_epi32(u[7], K32One);

                //        u[0] = _mm256_srai_epi32(u[0], 2);
                //        u[1] = _mm256_srai_epi32(u[1], 2);
                //        u[2] = _mm256_srai_epi32(u[2], 2);
                //        u[3] = _mm256_srai_epi32(u[3], 2);
                //        u[4] = _mm256_srai_epi32(u[4], 2);
                //        u[5] = _mm256_srai_epi32(u[5], 2);
                //        u[6] = _mm256_srai_epi32(u[6], 2);
                //        u[7] = _mm256_srai_epi32(u[7], 2);

                //        // Combine
                //        out[0]  = _mm256_packs_epi32(u[0], u[1]);
                //        out[16] = _mm256_packs_epi32(u[2], u[3]);
                //        out[8]  = _mm256_packs_epi32(u[4], u[5]);
                //        out[24] = _mm256_packs_epi32(u[6], u[7]);
                //    }
                //    {
                //        const __m256i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);
                //        const __m256i k32_m24_m08 = pair_set_epi32(-cospi_24_64, -cospi_8_64);
                //        const __m256i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);

                //        u[0] = _mm256_unpacklo_epi32(lstep1[18], lstep1[28]);
                //        u[1] = _mm256_unpackhi_epi32(lstep1[18], lstep1[28]);
                //        u[2] = _mm256_unpacklo_epi32(lstep1[19], lstep1[29]);
                //        u[3] = _mm256_unpackhi_epi32(lstep1[19], lstep1[29]);
                //        u[4] = _mm256_unpacklo_epi32(lstep1[20], lstep1[26]);
                //        u[5] = _mm256_unpackhi_epi32(lstep1[20], lstep1[26]);
                //        u[6] = _mm256_unpacklo_epi32(lstep1[21], lstep1[27]);
                //        u[7] = _mm256_unpackhi_epi32(lstep1[21], lstep1[27]);

                //        v[0] = k_madd_epi32(u[0], k32_m08_p24);
                //        v[1] = k_madd_epi32(u[1], k32_m08_p24);
                //        v[2] = k_madd_epi32(u[2], k32_m08_p24);
                //        v[3] = k_madd_epi32(u[3], k32_m08_p24);
                //        v[4] = k_madd_epi32(u[4], k32_m24_m08);
                //        v[5] = k_madd_epi32(u[5], k32_m24_m08);
                //        v[6] = k_madd_epi32(u[6], k32_m24_m08);
                //        v[7] = k_madd_epi32(u[7], k32_m24_m08);
                //        v[8] = k_madd_epi32(u[4], k32_m08_p24);
                //        v[9] = k_madd_epi32(u[5], k32_m08_p24);
                //        v[10] = k_madd_epi32(u[6], k32_m08_p24);
                //        v[11] = k_madd_epi32(u[7], k32_m08_p24);
                //        v[12] = k_madd_epi32(u[0], k32_p24_p08);
                //        v[13] = k_madd_epi32(u[1], k32_p24_p08);
                //        v[14] = k_madd_epi32(u[2], k32_p24_p08);
                //        v[15] = k_madd_epi32(u[3], k32_p24_p08);

                //        u[0] = k_packs_epi64(v[0], v[1]);
                //        u[1] = k_packs_epi64(v[2], v[3]);
                //        u[2] = k_packs_epi64(v[4], v[5]);
                //        u[3] = k_packs_epi64(v[6], v[7]);
                //        u[4] = k_packs_epi64(v[8], v[9]);
                //        u[5] = k_packs_epi64(v[10], v[11]);
                //        u[6] = k_packs_epi64(v[12], v[13]);
                //        u[7] = k_packs_epi64(v[14], v[15]);

                //        u[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                //        u[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                //        u[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                //        u[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                //        u[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                //        u[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                //        u[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                //        u[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);

                //        lstep2[18] = _mm256_srai_epi32(u[0], DCT_CONST_BITS);
                //        lstep2[19] = _mm256_srai_epi32(u[1], DCT_CONST_BITS);
                //        lstep2[20] = _mm256_srai_epi32(u[2], DCT_CONST_BITS);
                //        lstep2[21] = _mm256_srai_epi32(u[3], DCT_CONST_BITS);
                //        lstep2[26] = _mm256_srai_epi32(u[4], DCT_CONST_BITS);
                //        lstep2[27] = _mm256_srai_epi32(u[5], DCT_CONST_BITS);
                //        lstep2[28] = _mm256_srai_epi32(u[6], DCT_CONST_BITS);
                //        lstep2[29] = _mm256_srai_epi32(u[7], DCT_CONST_BITS);
                //    }
                //    {
                //        lstep2[32] = _mm256_add_epi32(lstep1[38], lstep3[32]);
                //        lstep2[33] = _mm256_add_epi32(lstep1[39], lstep3[33]);
                //        lstep2[34] = _mm256_add_epi32(lstep1[36], lstep3[34]);
                //        lstep2[35] = _mm256_add_epi32(lstep1[37], lstep3[35]);
                //        lstep2[36] = _mm256_sub_epi32(lstep3[34], lstep1[36]);
                //        lstep2[37] = _mm256_sub_epi32(lstep3[35], lstep1[37]);
                //        lstep2[38] = _mm256_sub_epi32(lstep3[32], lstep1[38]);
                //        lstep2[39] = _mm256_sub_epi32(lstep3[33], lstep1[39]);
                //        lstep2[40] = _mm256_sub_epi32(lstep3[46], lstep1[40]);
                //        lstep2[41] = _mm256_sub_epi32(lstep3[47], lstep1[41]);
                //        lstep2[42] = _mm256_sub_epi32(lstep3[44], lstep1[42]);
                //        lstep2[43] = _mm256_sub_epi32(lstep3[45], lstep1[43]);
                //        lstep2[44] = _mm256_add_epi32(lstep1[42], lstep3[44]);
                //        lstep2[45] = _mm256_add_epi32(lstep1[43], lstep3[45]);
                //        lstep2[46] = _mm256_add_epi32(lstep1[40], lstep3[46]);
                //        lstep2[47] = _mm256_add_epi32(lstep1[41], lstep3[47]);
                //        lstep2[48] = _mm256_add_epi32(lstep1[54], lstep3[48]);
                //        lstep2[49] = _mm256_add_epi32(lstep1[55], lstep3[49]);
                //        lstep2[50] = _mm256_add_epi32(lstep1[52], lstep3[50]);
                //        lstep2[51] = _mm256_add_epi32(lstep1[53], lstep3[51]);
                //        lstep2[52] = _mm256_sub_epi32(lstep3[50], lstep1[52]);
                //        lstep2[53] = _mm256_sub_epi32(lstep3[51], lstep1[53]);
                //        lstep2[54] = _mm256_sub_epi32(lstep3[48], lstep1[54]);
                //        lstep2[55] = _mm256_sub_epi32(lstep3[49], lstep1[55]);
                //        lstep2[56] = _mm256_sub_epi32(lstep3[62], lstep1[56]);
                //        lstep2[57] = _mm256_sub_epi32(lstep3[63], lstep1[57]);
                //        lstep2[58] = _mm256_sub_epi32(lstep3[60], lstep1[58]);
                //        lstep2[59] = _mm256_sub_epi32(lstep3[61], lstep1[59]);
                //        lstep2[60] = _mm256_add_epi32(lstep1[58], lstep3[60]);
                //        lstep2[61] = _mm256_add_epi32(lstep1[59], lstep3[61]);
                //        lstep2[62] = _mm256_add_epi32(lstep1[56], lstep3[62]);
                //        lstep2[63] = _mm256_add_epi32(lstep1[57], lstep3[63]);
                //    }
                //    // stage 6
                //    {
                //        const __m256i k32_p28_p04 = pair_set_epi32(cospi_28_64, cospi_4_64);
                //        const __m256i k32_p12_p20 = pair_set_epi32(cospi_12_64, cospi_20_64);
                //        const __m256i k32_m20_p12 = pair_set_epi32(-cospi_20_64, cospi_12_64);
                //        const __m256i k32_m04_p28 = pair_set_epi32(-cospi_4_64, cospi_28_64);

                //        u[0]  = _mm256_unpacklo_epi32(lstep2[8], lstep2[14]);
                //        u[1]  = _mm256_unpackhi_epi32(lstep2[8], lstep2[14]);
                //        u[2]  = _mm256_unpacklo_epi32(lstep2[9], lstep2[15]);
                //        u[3]  = _mm256_unpackhi_epi32(lstep2[9], lstep2[15]);
                //        u[4]  = _mm256_unpacklo_epi32(lstep2[10], lstep2[12]);
                //        u[5]  = _mm256_unpackhi_epi32(lstep2[10], lstep2[12]);
                //        u[6]  = _mm256_unpacklo_epi32(lstep2[11], lstep2[13]);
                //        u[7]  = _mm256_unpackhi_epi32(lstep2[11], lstep2[13]);
                //        u[8]  = _mm256_unpacklo_epi32(lstep2[10], lstep2[12]);
                //        u[9]  = _mm256_unpackhi_epi32(lstep2[10], lstep2[12]);
                //        u[10] = _mm256_unpacklo_epi32(lstep2[11], lstep2[13]);
                //        u[11] = _mm256_unpackhi_epi32(lstep2[11], lstep2[13]);
                //        u[12] = _mm256_unpacklo_epi32(lstep2[8], lstep2[14]);
                //        u[13] = _mm256_unpackhi_epi32(lstep2[8], lstep2[14]);
                //        u[14] = _mm256_unpacklo_epi32(lstep2[9], lstep2[15]);
                //        u[15] = _mm256_unpackhi_epi32(lstep2[9], lstep2[15]);

                //        v[0] = k_madd_epi32(u[0], k32_p28_p04);
                //        v[1] = k_madd_epi32(u[1], k32_p28_p04);
                //        v[2] = k_madd_epi32(u[2], k32_p28_p04);
                //        v[3] = k_madd_epi32(u[3], k32_p28_p04);
                //        v[4] = k_madd_epi32(u[4], k32_p12_p20);
                //        v[5] = k_madd_epi32(u[5], k32_p12_p20);
                //        v[6] = k_madd_epi32(u[6], k32_p12_p20);
                //        v[7] = k_madd_epi32(u[7], k32_p12_p20);
                //        v[8] = k_madd_epi32(u[8], k32_m20_p12);
                //        v[9] = k_madd_epi32(u[9], k32_m20_p12);
                //        v[10] = k_madd_epi32(u[10], k32_m20_p12);
                //        v[11] = k_madd_epi32(u[11], k32_m20_p12);
                //        v[12] = k_madd_epi32(u[12], k32_m04_p28);
                //        v[13] = k_madd_epi32(u[13], k32_m04_p28);
                //        v[14] = k_madd_epi32(u[14], k32_m04_p28);
                //        v[15] = k_madd_epi32(u[15], k32_m04_p28);

                //        u[0] = k_packs_epi64(v[0], v[1]);
                //        u[1] = k_packs_epi64(v[2], v[3]);
                //        u[2] = k_packs_epi64(v[4], v[5]);
                //        u[3] = k_packs_epi64(v[6], v[7]);
                //        u[4] = k_packs_epi64(v[8], v[9]);
                //        u[5] = k_packs_epi64(v[10], v[11]);
                //        u[6] = k_packs_epi64(v[12], v[13]);
                //        u[7] = k_packs_epi64(v[14], v[15]);

                //        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                //        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                //        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                //        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                //        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                //        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                //        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                //        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);

                //        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                //        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                //        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                //        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                //        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                //        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                //        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                //        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);

                //        sign[0] = _mm256_cmpgt_epi32(kZero, u[0]);
                //        sign[1] = _mm256_cmpgt_epi32(kZero, u[1]);
                //        sign[2] = _mm256_cmpgt_epi32(kZero, u[2]);
                //        sign[3] = _mm256_cmpgt_epi32(kZero, u[3]);
                //        sign[4] = _mm256_cmpgt_epi32(kZero, u[4]);
                //        sign[5] = _mm256_cmpgt_epi32(kZero, u[5]);
                //        sign[6] = _mm256_cmpgt_epi32(kZero, u[6]);
                //        sign[7] = _mm256_cmpgt_epi32(kZero, u[7]);

                //        u[0] = _mm256_sub_epi32(u[0], sign[0]);
                //        u[1] = _mm256_sub_epi32(u[1], sign[1]);
                //        u[2] = _mm256_sub_epi32(u[2], sign[2]);
                //        u[3] = _mm256_sub_epi32(u[3], sign[3]);
                //        u[4] = _mm256_sub_epi32(u[4], sign[4]);
                //        u[5] = _mm256_sub_epi32(u[5], sign[5]);
                //        u[6] = _mm256_sub_epi32(u[6], sign[6]);
                //        u[7] = _mm256_sub_epi32(u[7], sign[7]);

                //        u[0] = _mm256_add_epi32(u[0], K32One);
                //        u[1] = _mm256_add_epi32(u[1], K32One);
                //        u[2] = _mm256_add_epi32(u[2], K32One);
                //        u[3] = _mm256_add_epi32(u[3], K32One);
                //        u[4] = _mm256_add_epi32(u[4], K32One);
                //        u[5] = _mm256_add_epi32(u[5], K32One);
                //        u[6] = _mm256_add_epi32(u[6], K32One);
                //        u[7] = _mm256_add_epi32(u[7], K32One);

                //        u[0] = _mm256_srai_epi32(u[0], 2);
                //        u[1] = _mm256_srai_epi32(u[1], 2);
                //        u[2] = _mm256_srai_epi32(u[2], 2);
                //        u[3] = _mm256_srai_epi32(u[3], 2);
                //        u[4] = _mm256_srai_epi32(u[4], 2);
                //        u[5] = _mm256_srai_epi32(u[5], 2);
                //        u[6] = _mm256_srai_epi32(u[6], 2);
                //        u[7] = _mm256_srai_epi32(u[7], 2);

                //        out[4]  = _mm256_packs_epi32(u[0], u[1]);
                //        out[20] = _mm256_packs_epi32(u[2], u[3]);
                //        out[12] = _mm256_packs_epi32(u[4], u[5]);
                //        out[28] = _mm256_packs_epi32(u[6], u[7]);
                //    }
                //    {
                //        lstep3[16] = _mm256_add_epi32(lstep2[18], lstep1[16]);
                //        lstep3[17] = _mm256_add_epi32(lstep2[19], lstep1[17]);
                //        lstep3[18] = _mm256_sub_epi32(lstep1[16], lstep2[18]);
                //        lstep3[19] = _mm256_sub_epi32(lstep1[17], lstep2[19]);
                //        lstep3[20] = _mm256_sub_epi32(lstep1[22], lstep2[20]);
                //        lstep3[21] = _mm256_sub_epi32(lstep1[23], lstep2[21]);
                //        lstep3[22] = _mm256_add_epi32(lstep2[20], lstep1[22]);
                //        lstep3[23] = _mm256_add_epi32(lstep2[21], lstep1[23]);
                //        lstep3[24] = _mm256_add_epi32(lstep2[26], lstep1[24]);
                //        lstep3[25] = _mm256_add_epi32(lstep2[27], lstep1[25]);
                //        lstep3[26] = _mm256_sub_epi32(lstep1[24], lstep2[26]);
                //        lstep3[27] = _mm256_sub_epi32(lstep1[25], lstep2[27]);
                //        lstep3[28] = _mm256_sub_epi32(lstep1[30], lstep2[28]);
                //        lstep3[29] = _mm256_sub_epi32(lstep1[31], lstep2[29]);
                //        lstep3[30] = _mm256_add_epi32(lstep2[28], lstep1[30]);
                //        lstep3[31] = _mm256_add_epi32(lstep2[29], lstep1[31]);
                //    }
                //    {
                //        const __m256i k32_m04_p28 = pair_set_epi32(-cospi_4_64, cospi_28_64);
                //        const __m256i k32_m28_m04 = pair_set_epi32(-cospi_28_64, -cospi_4_64);
                //        const __m256i k32_m20_p12 = pair_set_epi32(-cospi_20_64, cospi_12_64);
                //        const __m256i k32_m12_m20 =
                //            pair_set_epi32(-cospi_12_64, -cospi_20_64);
                //        const __m256i k32_p12_p20 = pair_set_epi32(cospi_12_64, cospi_20_64);
                //        const __m256i k32_p28_p04 = pair_set_epi32(cospi_28_64, cospi_4_64);

                //        u[0] = _mm256_unpacklo_epi32(lstep2[34], lstep2[60]);
                //        u[1] = _mm256_unpackhi_epi32(lstep2[34], lstep2[60]);
                //        u[2] = _mm256_unpacklo_epi32(lstep2[35], lstep2[61]);
                //        u[3] = _mm256_unpackhi_epi32(lstep2[35], lstep2[61]);
                //        u[4] = _mm256_unpacklo_epi32(lstep2[36], lstep2[58]);
                //        u[5] = _mm256_unpackhi_epi32(lstep2[36], lstep2[58]);
                //        u[6] = _mm256_unpacklo_epi32(lstep2[37], lstep2[59]);
                //        u[7] = _mm256_unpackhi_epi32(lstep2[37], lstep2[59]);
                //        u[8] = _mm256_unpacklo_epi32(lstep2[42], lstep2[52]);
                //        u[9] = _mm256_unpackhi_epi32(lstep2[42], lstep2[52]);
                //        u[10] = _mm256_unpacklo_epi32(lstep2[43], lstep2[53]);
                //        u[11] = _mm256_unpackhi_epi32(lstep2[43], lstep2[53]);
                //        u[12] = _mm256_unpacklo_epi32(lstep2[44], lstep2[50]);
                //        u[13] = _mm256_unpackhi_epi32(lstep2[44], lstep2[50]);
                //        u[14] = _mm256_unpacklo_epi32(lstep2[45], lstep2[51]);
                //        u[15] = _mm256_unpackhi_epi32(lstep2[45], lstep2[51]);

                //        v[0] = k_madd_epi32(u[0], k32_m04_p28);
                //        v[1] = k_madd_epi32(u[1], k32_m04_p28);
                //        v[2] = k_madd_epi32(u[2], k32_m04_p28);
                //        v[3] = k_madd_epi32(u[3], k32_m04_p28);
                //        v[4] = k_madd_epi32(u[4], k32_m28_m04);
                //        v[5] = k_madd_epi32(u[5], k32_m28_m04);
                //        v[6] = k_madd_epi32(u[6], k32_m28_m04);
                //        v[7] = k_madd_epi32(u[7], k32_m28_m04);
                //        v[8] = k_madd_epi32(u[8], k32_m20_p12);
                //        v[9] = k_madd_epi32(u[9], k32_m20_p12);
                //        v[10] = k_madd_epi32(u[10], k32_m20_p12);
                //        v[11] = k_madd_epi32(u[11], k32_m20_p12);
                //        v[12] = k_madd_epi32(u[12], k32_m12_m20);
                //        v[13] = k_madd_epi32(u[13], k32_m12_m20);
                //        v[14] = k_madd_epi32(u[14], k32_m12_m20);
                //        v[15] = k_madd_epi32(u[15], k32_m12_m20);
                //        v[16] = k_madd_epi32(u[12], k32_m20_p12);
                //        v[17] = k_madd_epi32(u[13], k32_m20_p12);
                //        v[18] = k_madd_epi32(u[14], k32_m20_p12);
                //        v[19] = k_madd_epi32(u[15], k32_m20_p12);
                //        v[20] = k_madd_epi32(u[8], k32_p12_p20);
                //        v[21] = k_madd_epi32(u[9], k32_p12_p20);
                //        v[22] = k_madd_epi32(u[10], k32_p12_p20);
                //        v[23] = k_madd_epi32(u[11], k32_p12_p20);
                //        v[24] = k_madd_epi32(u[4], k32_m04_p28);
                //        v[25] = k_madd_epi32(u[5], k32_m04_p28);
                //        v[26] = k_madd_epi32(u[6], k32_m04_p28);
                //        v[27] = k_madd_epi32(u[7], k32_m04_p28);
                //        v[28] = k_madd_epi32(u[0], k32_p28_p04);
                //        v[29] = k_madd_epi32(u[1], k32_p28_p04);
                //        v[30] = k_madd_epi32(u[2], k32_p28_p04);
                //        v[31] = k_madd_epi32(u[3], k32_p28_p04);

                //        u[0] = k_packs_epi64(v[0], v[1]);
                //        u[1] = k_packs_epi64(v[2], v[3]);
                //        u[2] = k_packs_epi64(v[4], v[5]);
                //        u[3] = k_packs_epi64(v[6], v[7]);
                //        u[4] = k_packs_epi64(v[8], v[9]);
                //        u[5] = k_packs_epi64(v[10], v[11]);
                //        u[6] = k_packs_epi64(v[12], v[13]);
                //        u[7] = k_packs_epi64(v[14], v[15]);
                //        u[8] = k_packs_epi64(v[16], v[17]);
                //        u[9] = k_packs_epi64(v[18], v[19]);
                //        u[10] = k_packs_epi64(v[20], v[21]);
                //        u[11] = k_packs_epi64(v[22], v[23]);
                //        u[12] = k_packs_epi64(v[24], v[25]);
                //        u[13] = k_packs_epi64(v[26], v[27]);
                //        u[14] = k_packs_epi64(v[28], v[29]);
                //        u[15] = k_packs_epi64(v[30], v[31]);

                //        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                //        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                //        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                //        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                //        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                //        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                //        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                //        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                //        v[8] = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                //        v[9] = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                //        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                //        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                //        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                //        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                //        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                //        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                //        lstep3[34] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                //        lstep3[35] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                //        lstep3[36] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                //        lstep3[37] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                //        lstep3[42] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                //        lstep3[43] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                //        lstep3[44] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                //        lstep3[45] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
                //        lstep3[50] = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
                //        lstep3[51] = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
                //        lstep3[52] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
                //        lstep3[53] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
                //        lstep3[58] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
                //        lstep3[59] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
                //        lstep3[60] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
                //        lstep3[61] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);
                //    }
                //    // stage 7
                //    {
                //        const __m256i k32_p30_p02 = pair_set_epi32(cospi_30_64, cospi_2_64);
                //        const __m256i k32_p14_p18 = pair_set_epi32(cospi_14_64, cospi_18_64);
                //        const __m256i k32_p22_p10 = pair_set_epi32(cospi_22_64, cospi_10_64);
                //        const __m256i k32_p06_p26 = pair_set_epi32(cospi_6_64, cospi_26_64);
                //        const __m256i k32_m26_p06 = pair_set_epi32(-cospi_26_64, cospi_6_64);
                //        const __m256i k32_m10_p22 = pair_set_epi32(-cospi_10_64, cospi_22_64);
                //        const __m256i k32_m18_p14 = pair_set_epi32(-cospi_18_64, cospi_14_64);
                //        const __m256i k32_m02_p30 = pair_set_epi32(-cospi_2_64, cospi_30_64);

                //        u[0]  = _mm256_unpacklo_epi32(lstep3[16], lstep3[30]);
                //        u[1]  = _mm256_unpackhi_epi32(lstep3[16], lstep3[30]);
                //        u[2]  = _mm256_unpacklo_epi32(lstep3[17], lstep3[31]);
                //        u[3]  = _mm256_unpackhi_epi32(lstep3[17], lstep3[31]);
                //        u[4]  = _mm256_unpacklo_epi32(lstep3[18], lstep3[28]);
                //        u[5]  = _mm256_unpackhi_epi32(lstep3[18], lstep3[28]);
                //        u[6]  = _mm256_unpacklo_epi32(lstep3[19], lstep3[29]);
                //        u[7]  = _mm256_unpackhi_epi32(lstep3[19], lstep3[29]);
                //        u[8]  = _mm256_unpacklo_epi32(lstep3[20], lstep3[26]);
                //        u[9]  = _mm256_unpackhi_epi32(lstep3[20], lstep3[26]);
                //        u[10] = _mm256_unpacklo_epi32(lstep3[21], lstep3[27]);
                //        u[11] = _mm256_unpackhi_epi32(lstep3[21], lstep3[27]);
                //        u[12] = _mm256_unpacklo_epi32(lstep3[22], lstep3[24]);
                //        u[13] = _mm256_unpackhi_epi32(lstep3[22], lstep3[24]);
                //        u[14] = _mm256_unpacklo_epi32(lstep3[23], lstep3[25]);
                //        u[15] = _mm256_unpackhi_epi32(lstep3[23], lstep3[25]);

                //        v[0]  = k_madd_epi32(u[0], k32_p30_p02);
                //        v[1]  = k_madd_epi32(u[1], k32_p30_p02);
                //        v[2]  = k_madd_epi32(u[2], k32_p30_p02);
                //        v[3]  = k_madd_epi32(u[3], k32_p30_p02);
                //        v[4]  = k_madd_epi32(u[4], k32_p14_p18);
                //        v[5]  = k_madd_epi32(u[5], k32_p14_p18);
                //        v[6]  = k_madd_epi32(u[6], k32_p14_p18);
                //        v[7]  = k_madd_epi32(u[7], k32_p14_p18);
                //        v[8]  = k_madd_epi32(u[8], k32_p22_p10);
                //        v[9]  = k_madd_epi32(u[9], k32_p22_p10);
                //        v[10] = k_madd_epi32(u[10], k32_p22_p10);
                //        v[11] = k_madd_epi32(u[11], k32_p22_p10);
                //        v[12] = k_madd_epi32(u[12], k32_p06_p26);
                //        v[13] = k_madd_epi32(u[13], k32_p06_p26);
                //        v[14] = k_madd_epi32(u[14], k32_p06_p26);
                //        v[15] = k_madd_epi32(u[15], k32_p06_p26);
                //        v[16] = k_madd_epi32(u[12], k32_m26_p06);
                //        v[17] = k_madd_epi32(u[13], k32_m26_p06);
                //        v[18] = k_madd_epi32(u[14], k32_m26_p06);
                //        v[19] = k_madd_epi32(u[15], k32_m26_p06);
                //        v[20] = k_madd_epi32(u[8], k32_m10_p22);
                //        v[21] = k_madd_epi32(u[9], k32_m10_p22);
                //        v[22] = k_madd_epi32(u[10], k32_m10_p22);
                //        v[23] = k_madd_epi32(u[11], k32_m10_p22);
                //        v[24] = k_madd_epi32(u[4], k32_m18_p14);
                //        v[25] = k_madd_epi32(u[5], k32_m18_p14);
                //        v[26] = k_madd_epi32(u[6], k32_m18_p14);
                //        v[27] = k_madd_epi32(u[7], k32_m18_p14);
                //        v[28] = k_madd_epi32(u[0], k32_m02_p30);
                //        v[29] = k_madd_epi32(u[1], k32_m02_p30);
                //        v[30] = k_madd_epi32(u[2], k32_m02_p30);
                //        v[31] = k_madd_epi32(u[3], k32_m02_p30);

                //        u[0]  = k_packs_epi64(v[0], v[1]);
                //        u[1]  = k_packs_epi64(v[2], v[3]);
                //        u[2]  = k_packs_epi64(v[4], v[5]);
                //        u[3]  = k_packs_epi64(v[6], v[7]);
                //        u[4]  = k_packs_epi64(v[8], v[9]);
                //        u[5]  = k_packs_epi64(v[10], v[11]);
                //        u[6]  = k_packs_epi64(v[12], v[13]);
                //        u[7]  = k_packs_epi64(v[14], v[15]);
                //        u[8]  = k_packs_epi64(v[16], v[17]);
                //        u[9]  = k_packs_epi64(v[18], v[19]);
                //        u[10] = k_packs_epi64(v[20], v[21]);
                //        u[11] = k_packs_epi64(v[22], v[23]);
                //        u[12] = k_packs_epi64(v[24], v[25]);
                //        u[13] = k_packs_epi64(v[26], v[27]);
                //        u[14] = k_packs_epi64(v[28], v[29]);
                //        u[15] = k_packs_epi64(v[30], v[31]);

                //        v[0]  = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                //        v[1]  = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                //        v[2]  = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                //        v[3]  = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                //        v[4]  = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                //        v[5]  = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                //        v[6]  = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                //        v[7]  = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                //        v[8]  = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                //        v[9]  = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                //        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                //        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                //        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                //        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                //        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                //        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                //        u[0]  = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                //        u[1]  = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                //        u[2]  = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                //        u[3]  = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                //        u[4]  = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                //        u[5]  = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                //        u[6]  = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                //        u[7]  = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
                //        u[8]  = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
                //        u[9]  = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
                //        u[10] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
                //        u[11] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
                //        u[12] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
                //        u[13] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
                //        u[14] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
                //        u[15] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);

                //        v[0]  = _mm256_cmpgt_epi32(kZero, u[0]);
                //        v[1]  = _mm256_cmpgt_epi32(kZero, u[1]);
                //        v[2]  = _mm256_cmpgt_epi32(kZero, u[2]);
                //        v[3]  = _mm256_cmpgt_epi32(kZero, u[3]);
                //        v[4]  = _mm256_cmpgt_epi32(kZero, u[4]);
                //        v[5]  = _mm256_cmpgt_epi32(kZero, u[5]);
                //        v[6]  = _mm256_cmpgt_epi32(kZero, u[6]);
                //        v[7]  = _mm256_cmpgt_epi32(kZero, u[7]);
                //        v[8]  = _mm256_cmpgt_epi32(kZero, u[8]);
                //        v[9]  = _mm256_cmpgt_epi32(kZero, u[9]);
                //        v[10] = _mm256_cmpgt_epi32(kZero, u[10]);
                //        v[11] = _mm256_cmpgt_epi32(kZero, u[11]);
                //        v[12] = _mm256_cmpgt_epi32(kZero, u[12]);
                //        v[13] = _mm256_cmpgt_epi32(kZero, u[13]);
                //        v[14] = _mm256_cmpgt_epi32(kZero, u[14]);
                //        v[15] = _mm256_cmpgt_epi32(kZero, u[15]);

                //        u[0]  = _mm256_sub_epi32(u[0], v[0]);
                //        u[1]  = _mm256_sub_epi32(u[1], v[1]);
                //        u[2]  = _mm256_sub_epi32(u[2], v[2]);
                //        u[3]  = _mm256_sub_epi32(u[3], v[3]);
                //        u[4]  = _mm256_sub_epi32(u[4], v[4]);
                //        u[5]  = _mm256_sub_epi32(u[5], v[5]);
                //        u[6]  = _mm256_sub_epi32(u[6], v[6]);
                //        u[7]  = _mm256_sub_epi32(u[7], v[7]);
                //        u[8]  = _mm256_sub_epi32(u[8], v[8]);
                //        u[9]  = _mm256_sub_epi32(u[9], v[9]);
                //        u[10] = _mm256_sub_epi32(u[10], v[10]);
                //        u[11] = _mm256_sub_epi32(u[11], v[11]);
                //        u[12] = _mm256_sub_epi32(u[12], v[12]);
                //        u[13] = _mm256_sub_epi32(u[13], v[13]);
                //        u[14] = _mm256_sub_epi32(u[14], v[14]);
                //        u[15] = _mm256_sub_epi32(u[15], v[15]);

                //        v[0]  = _mm256_add_epi32(u[0], K32One);
                //        v[1]  = _mm256_add_epi32(u[1], K32One);
                //        v[2]  = _mm256_add_epi32(u[2], K32One);
                //        v[3]  = _mm256_add_epi32(u[3], K32One);
                //        v[4]  = _mm256_add_epi32(u[4], K32One);
                //        v[5]  = _mm256_add_epi32(u[5], K32One);
                //        v[6]  = _mm256_add_epi32(u[6], K32One);
                //        v[7]  = _mm256_add_epi32(u[7], K32One);
                //        v[8]  = _mm256_add_epi32(u[8], K32One);
                //        v[9]  = _mm256_add_epi32(u[9], K32One);
                //        v[10] = _mm256_add_epi32(u[10], K32One);
                //        v[11] = _mm256_add_epi32(u[11], K32One);
                //        v[12] = _mm256_add_epi32(u[12], K32One);
                //        v[13] = _mm256_add_epi32(u[13], K32One);
                //        v[14] = _mm256_add_epi32(u[14], K32One);
                //        v[15] = _mm256_add_epi32(u[15], K32One);

                //        u[0]  = _mm256_srai_epi32(v[0], 2);
                //        u[1]  = _mm256_srai_epi32(v[1], 2);
                //        u[2]  = _mm256_srai_epi32(v[2], 2);
                //        u[3]  = _mm256_srai_epi32(v[3], 2);
                //        u[4]  = _mm256_srai_epi32(v[4], 2);
                //        u[5]  = _mm256_srai_epi32(v[5], 2);
                //        u[6]  = _mm256_srai_epi32(v[6], 2);
                //        u[7]  = _mm256_srai_epi32(v[7], 2);
                //        u[8]  = _mm256_srai_epi32(v[8], 2);
                //        u[9]  = _mm256_srai_epi32(v[9], 2);
                //        u[10] = _mm256_srai_epi32(v[10], 2);
                //        u[11] = _mm256_srai_epi32(v[11], 2);
                //        u[12] = _mm256_srai_epi32(v[12], 2);
                //        u[13] = _mm256_srai_epi32(v[13], 2);
                //        u[14] = _mm256_srai_epi32(v[14], 2);
                //        u[15] = _mm256_srai_epi32(v[15], 2);

                //        out[2]  = _mm256_packs_epi32(u[0], u[1]);
                //        out[18] = _mm256_packs_epi32(u[2], u[3]);
                //        out[10] = _mm256_packs_epi32(u[4], u[5]);
                //        out[26] = _mm256_packs_epi32(u[6], u[7]);
                //        out[6]  = _mm256_packs_epi32(u[8], u[9]);
                //        out[22] = _mm256_packs_epi32(u[10], u[11]);
                //        out[14] = _mm256_packs_epi32(u[12], u[13]);
                //        out[30] = _mm256_packs_epi32(u[14], u[15]);
                //    }
                //    {
                //        lstep1[32] = _mm256_add_epi32(lstep3[34], lstep2[32]);
                //        lstep1[33] = _mm256_add_epi32(lstep3[35], lstep2[33]);
                //        lstep1[34] = _mm256_sub_epi32(lstep2[32], lstep3[34]);
                //        lstep1[35] = _mm256_sub_epi32(lstep2[33], lstep3[35]);
                //        lstep1[36] = _mm256_sub_epi32(lstep2[38], lstep3[36]);
                //        lstep1[37] = _mm256_sub_epi32(lstep2[39], lstep3[37]);
                //        lstep1[38] = _mm256_add_epi32(lstep3[36], lstep2[38]);
                //        lstep1[39] = _mm256_add_epi32(lstep3[37], lstep2[39]);
                //        lstep1[40] = _mm256_add_epi32(lstep3[42], lstep2[40]);
                //        lstep1[41] = _mm256_add_epi32(lstep3[43], lstep2[41]);
                //        lstep1[42] = _mm256_sub_epi32(lstep2[40], lstep3[42]);
                //        lstep1[43] = _mm256_sub_epi32(lstep2[41], lstep3[43]);
                //        lstep1[44] = _mm256_sub_epi32(lstep2[46], lstep3[44]);
                //        lstep1[45] = _mm256_sub_epi32(lstep2[47], lstep3[45]);
                //        lstep1[46] = _mm256_add_epi32(lstep3[44], lstep2[46]);
                //        lstep1[47] = _mm256_add_epi32(lstep3[45], lstep2[47]);
                //        lstep1[48] = _mm256_add_epi32(lstep3[50], lstep2[48]);
                //        lstep1[49] = _mm256_add_epi32(lstep3[51], lstep2[49]);
                //        lstep1[50] = _mm256_sub_epi32(lstep2[48], lstep3[50]);
                //        lstep1[51] = _mm256_sub_epi32(lstep2[49], lstep3[51]);
                //        lstep1[52] = _mm256_sub_epi32(lstep2[54], lstep3[52]);
                //        lstep1[53] = _mm256_sub_epi32(lstep2[55], lstep3[53]);
                //        lstep1[54] = _mm256_add_epi32(lstep3[52], lstep2[54]);
                //        lstep1[55] = _mm256_add_epi32(lstep3[53], lstep2[55]);
                //        lstep1[56] = _mm256_add_epi32(lstep3[58], lstep2[56]);
                //        lstep1[57] = _mm256_add_epi32(lstep3[59], lstep2[57]);
                //        lstep1[58] = _mm256_sub_epi32(lstep2[56], lstep3[58]);
                //        lstep1[59] = _mm256_sub_epi32(lstep2[57], lstep3[59]);
                //        lstep1[60] = _mm256_sub_epi32(lstep2[62], lstep3[60]);
                //        lstep1[61] = _mm256_sub_epi32(lstep2[63], lstep3[61]);
                //        lstep1[62] = _mm256_add_epi32(lstep3[60], lstep2[62]);
                //        lstep1[63] = _mm256_add_epi32(lstep3[61], lstep2[63]);
                //    }
                //    // stage 8
                //    {
                //        const __m256i k32_p31_p01 = pair_set_epi32(cospi_31_64, cospi_1_64);
                //        const __m256i k32_p15_p17 = pair_set_epi32(cospi_15_64, cospi_17_64);
                //        const __m256i k32_p23_p09 = pair_set_epi32(cospi_23_64, cospi_9_64);
                //        const __m256i k32_p07_p25 = pair_set_epi32(cospi_7_64, cospi_25_64);
                //        const __m256i k32_m25_p07 = pair_set_epi32(-cospi_25_64, cospi_7_64);
                //        const __m256i k32_m09_p23 = pair_set_epi32(-cospi_9_64, cospi_23_64);
                //        const __m256i k32_m17_p15 = pair_set_epi32(-cospi_17_64, cospi_15_64);
                //        const __m256i k32_m01_p31 = pair_set_epi32(-cospi_1_64, cospi_31_64);

                //        u[0]  = _mm256_unpacklo_epi32(lstep1[32], lstep1[62]);
                //        u[1]  = _mm256_unpackhi_epi32(lstep1[32], lstep1[62]);
                //        u[2]  = _mm256_unpacklo_epi32(lstep1[33], lstep1[63]);
                //        u[3]  = _mm256_unpackhi_epi32(lstep1[33], lstep1[63]);
                //        u[4]  = _mm256_unpacklo_epi32(lstep1[34], lstep1[60]);
                //        u[5]  = _mm256_unpackhi_epi32(lstep1[34], lstep1[60]);
                //        u[6]  = _mm256_unpacklo_epi32(lstep1[35], lstep1[61]);
                //        u[7]  = _mm256_unpackhi_epi32(lstep1[35], lstep1[61]);
                //        u[8]  = _mm256_unpacklo_epi32(lstep1[36], lstep1[58]);
                //        u[9]  = _mm256_unpackhi_epi32(lstep1[36], lstep1[58]);
                //        u[10] = _mm256_unpacklo_epi32(lstep1[37], lstep1[59]);
                //        u[11] = _mm256_unpackhi_epi32(lstep1[37], lstep1[59]);
                //        u[12] = _mm256_unpacklo_epi32(lstep1[38], lstep1[56]);
                //        u[13] = _mm256_unpackhi_epi32(lstep1[38], lstep1[56]);
                //        u[14] = _mm256_unpacklo_epi32(lstep1[39], lstep1[57]);
                //        u[15] = _mm256_unpackhi_epi32(lstep1[39], lstep1[57]);

                //        v[0] = k_madd_epi32(u[0], k32_p31_p01);
                //        v[1] = k_madd_epi32(u[1], k32_p31_p01);
                //        v[2] = k_madd_epi32(u[2], k32_p31_p01);
                //        v[3] = k_madd_epi32(u[3], k32_p31_p01);
                //        v[4] = k_madd_epi32(u[4], k32_p15_p17);
                //        v[5] = k_madd_epi32(u[5], k32_p15_p17);
                //        v[6] = k_madd_epi32(u[6], k32_p15_p17);
                //        v[7] = k_madd_epi32(u[7], k32_p15_p17);
                //        v[8] = k_madd_epi32(u[8], k32_p23_p09);
                //        v[9] = k_madd_epi32(u[9], k32_p23_p09);
                //        v[10] = k_madd_epi32(u[10], k32_p23_p09);
                //        v[11] = k_madd_epi32(u[11], k32_p23_p09);
                //        v[12] = k_madd_epi32(u[12], k32_p07_p25);
                //        v[13] = k_madd_epi32(u[13], k32_p07_p25);
                //        v[14] = k_madd_epi32(u[14], k32_p07_p25);
                //        v[15] = k_madd_epi32(u[15], k32_p07_p25);
                //        v[16] = k_madd_epi32(u[12], k32_m25_p07);
                //        v[17] = k_madd_epi32(u[13], k32_m25_p07);
                //        v[18] = k_madd_epi32(u[14], k32_m25_p07);
                //        v[19] = k_madd_epi32(u[15], k32_m25_p07);
                //        v[20] = k_madd_epi32(u[8], k32_m09_p23);
                //        v[21] = k_madd_epi32(u[9], k32_m09_p23);
                //        v[22] = k_madd_epi32(u[10], k32_m09_p23);
                //        v[23] = k_madd_epi32(u[11], k32_m09_p23);
                //        v[24] = k_madd_epi32(u[4], k32_m17_p15);
                //        v[25] = k_madd_epi32(u[5], k32_m17_p15);
                //        v[26] = k_madd_epi32(u[6], k32_m17_p15);
                //        v[27] = k_madd_epi32(u[7], k32_m17_p15);
                //        v[28] = k_madd_epi32(u[0], k32_m01_p31);
                //        v[29] = k_madd_epi32(u[1], k32_m01_p31);
                //        v[30] = k_madd_epi32(u[2], k32_m01_p31);
                //        v[31] = k_madd_epi32(u[3], k32_m01_p31);

                //        u[0] = k_packs_epi64(v[0], v[1]);
                //        u[1] = k_packs_epi64(v[2], v[3]);
                //        u[2] = k_packs_epi64(v[4], v[5]);
                //        u[3] = k_packs_epi64(v[6], v[7]);
                //        u[4] = k_packs_epi64(v[8], v[9]);
                //        u[5] = k_packs_epi64(v[10], v[11]);
                //        u[6] = k_packs_epi64(v[12], v[13]);
                //        u[7] = k_packs_epi64(v[14], v[15]);
                //        u[8] = k_packs_epi64(v[16], v[17]);
                //        u[9] = k_packs_epi64(v[18], v[19]);
                //        u[10] = k_packs_epi64(v[20], v[21]);
                //        u[11] = k_packs_epi64(v[22], v[23]);
                //        u[12] = k_packs_epi64(v[24], v[25]);
                //        u[13] = k_packs_epi64(v[26], v[27]);
                //        u[14] = k_packs_epi64(v[28], v[29]);
                //        u[15] = k_packs_epi64(v[30], v[31]);

                //        v[0] = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                //        v[1] = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                //        v[2] = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                //        v[3] = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                //        v[4] = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                //        v[5] = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                //        v[6] = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                //        v[7] = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                //        v[8] = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                //        v[9] = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                //        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                //        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                //        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                //        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                //        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                //        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                //        u[0] = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                //        u[1] = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                //        u[2] = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                //        u[3] = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                //        u[4] = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                //        u[5] = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                //        u[6] = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                //        u[7] = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
                //        u[8] = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
                //        u[9] = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
                //        u[10] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
                //        u[11] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
                //        u[12] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
                //        u[13] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
                //        u[14] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
                //        u[15] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);

                //        v[0]  = _mm256_cmpgt_epi32(kZero, u[0]);
                //        v[1]  = _mm256_cmpgt_epi32(kZero, u[1]);
                //        v[2]  = _mm256_cmpgt_epi32(kZero, u[2]);
                //        v[3]  = _mm256_cmpgt_epi32(kZero, u[3]);
                //        v[4]  = _mm256_cmpgt_epi32(kZero, u[4]);
                //        v[5]  = _mm256_cmpgt_epi32(kZero, u[5]);
                //        v[6]  = _mm256_cmpgt_epi32(kZero, u[6]);
                //        v[7]  = _mm256_cmpgt_epi32(kZero, u[7]);
                //        v[8]  = _mm256_cmpgt_epi32(kZero, u[8]);
                //        v[9]  = _mm256_cmpgt_epi32(kZero, u[9]);
                //        v[10] = _mm256_cmpgt_epi32(kZero, u[10]);
                //        v[11] = _mm256_cmpgt_epi32(kZero, u[11]);
                //        v[12] = _mm256_cmpgt_epi32(kZero, u[12]);
                //        v[13] = _mm256_cmpgt_epi32(kZero, u[13]);
                //        v[14] = _mm256_cmpgt_epi32(kZero, u[14]);
                //        v[15] = _mm256_cmpgt_epi32(kZero, u[15]);

                //        u[0]  = _mm256_sub_epi32(u[0], v[0]);
                //        u[1]  = _mm256_sub_epi32(u[1], v[1]);
                //        u[2]  = _mm256_sub_epi32(u[2], v[2]);
                //        u[3]  = _mm256_sub_epi32(u[3], v[3]);
                //        u[4]  = _mm256_sub_epi32(u[4], v[4]);
                //        u[5]  = _mm256_sub_epi32(u[5], v[5]);
                //        u[6]  = _mm256_sub_epi32(u[6], v[6]);
                //        u[7]  = _mm256_sub_epi32(u[7], v[7]);
                //        u[8]  = _mm256_sub_epi32(u[8], v[8]);
                //        u[9]  = _mm256_sub_epi32(u[9], v[9]);
                //        u[10] = _mm256_sub_epi32(u[10], v[10]);
                //        u[11] = _mm256_sub_epi32(u[11], v[11]);
                //        u[12] = _mm256_sub_epi32(u[12], v[12]);
                //        u[13] = _mm256_sub_epi32(u[13], v[13]);
                //        u[14] = _mm256_sub_epi32(u[14], v[14]);
                //        u[15] = _mm256_sub_epi32(u[15], v[15]);

                //        v[0]  = _mm256_add_epi32(u[0], K32One);
                //        v[1]  = _mm256_add_epi32(u[1], K32One);
                //        v[2]  = _mm256_add_epi32(u[2], K32One);
                //        v[3]  = _mm256_add_epi32(u[3], K32One);
                //        v[4]  = _mm256_add_epi32(u[4], K32One);
                //        v[5]  = _mm256_add_epi32(u[5], K32One);
                //        v[6]  = _mm256_add_epi32(u[6], K32One);
                //        v[7]  = _mm256_add_epi32(u[7], K32One);
                //        v[8]  = _mm256_add_epi32(u[8], K32One);
                //        v[9]  = _mm256_add_epi32(u[9], K32One);
                //        v[10] = _mm256_add_epi32(u[10], K32One);
                //        v[11] = _mm256_add_epi32(u[11], K32One);
                //        v[12] = _mm256_add_epi32(u[12], K32One);
                //        v[13] = _mm256_add_epi32(u[13], K32One);
                //        v[14] = _mm256_add_epi32(u[14], K32One);
                //        v[15] = _mm256_add_epi32(u[15], K32One);

                //        u[0]  = _mm256_srai_epi32(v[0], 2);
                //        u[1]  = _mm256_srai_epi32(v[1], 2);
                //        u[2]  = _mm256_srai_epi32(v[2], 2);
                //        u[3]  = _mm256_srai_epi32(v[3], 2);
                //        u[4]  = _mm256_srai_epi32(v[4], 2);
                //        u[5]  = _mm256_srai_epi32(v[5], 2);
                //        u[6]  = _mm256_srai_epi32(v[6], 2);
                //        u[7]  = _mm256_srai_epi32(v[7], 2);
                //        u[8]  = _mm256_srai_epi32(v[8], 2);
                //        u[9]  = _mm256_srai_epi32(v[9], 2);
                //        u[10] = _mm256_srai_epi32(v[10], 2);
                //        u[11] = _mm256_srai_epi32(v[11], 2);
                //        u[12] = _mm256_srai_epi32(v[12], 2);
                //        u[13] = _mm256_srai_epi32(v[13], 2);
                //        u[14] = _mm256_srai_epi32(v[14], 2);
                //        u[15] = _mm256_srai_epi32(v[15], 2);

                //        out[1]  = _mm256_packs_epi32(u[0], u[1]);
                //        out[17] = _mm256_packs_epi32(u[2], u[3]);
                //        out[9]  = _mm256_packs_epi32(u[4], u[5]);
                //        out[25] = _mm256_packs_epi32(u[6], u[7]);
                //        out[7]  = _mm256_packs_epi32(u[8], u[9]);
                //        out[23] = _mm256_packs_epi32(u[10], u[11]);
                //        out[15] = _mm256_packs_epi32(u[12], u[13]);
                //        out[31] = _mm256_packs_epi32(u[14], u[15]);
                //   }
                //    {
                //        const __m256i k32_p27_p05 = pair_set_epi32(cospi_27_64, cospi_5_64);
                //        const __m256i k32_p11_p21 = pair_set_epi32(cospi_11_64, cospi_21_64);
                //        const __m256i k32_p19_p13 = pair_set_epi32(cospi_19_64, cospi_13_64);
                //        const __m256i k32_p03_p29 = pair_set_epi32(cospi_3_64, cospi_29_64);
                //        const __m256i k32_m29_p03 = pair_set_epi32(-cospi_29_64, cospi_3_64);
                //        const __m256i k32_m13_p19 = pair_set_epi32(-cospi_13_64, cospi_19_64);
                //        const __m256i k32_m21_p11 = pair_set_epi32(-cospi_21_64, cospi_11_64);
                //        const __m256i k32_m05_p27 = pair_set_epi32(-cospi_5_64, cospi_27_64);

                //        u[0]  = _mm256_unpacklo_epi32(lstep1[40], lstep1[54]);
                //        u[1]  = _mm256_unpackhi_epi32(lstep1[40], lstep1[54]);
                //        u[2]  = _mm256_unpacklo_epi32(lstep1[41], lstep1[55]);
                //        u[3]  = _mm256_unpackhi_epi32(lstep1[41], lstep1[55]);
                //        u[4]  = _mm256_unpacklo_epi32(lstep1[42], lstep1[52]);
                //        u[5]  = _mm256_unpackhi_epi32(lstep1[42], lstep1[52]);
                //        u[6]  = _mm256_unpacklo_epi32(lstep1[43], lstep1[53]);
                //        u[7]  = _mm256_unpackhi_epi32(lstep1[43], lstep1[53]);
                //        u[8]  = _mm256_unpacklo_epi32(lstep1[44], lstep1[50]);
                //        u[9]  = _mm256_unpackhi_epi32(lstep1[44], lstep1[50]);
                //        u[10] = _mm256_unpacklo_epi32(lstep1[45], lstep1[51]);
                //        u[11] = _mm256_unpackhi_epi32(lstep1[45], lstep1[51]);
                //        u[12] = _mm256_unpacklo_epi32(lstep1[46], lstep1[48]);
                //        u[13] = _mm256_unpackhi_epi32(lstep1[46], lstep1[48]);
                //        u[14] = _mm256_unpacklo_epi32(lstep1[47], lstep1[49]);
                //        u[15] = _mm256_unpackhi_epi32(lstep1[47], lstep1[49]);

                //        v[0]  = k_madd_epi32(u[0], k32_p27_p05);
                //        v[1]  = k_madd_epi32(u[1], k32_p27_p05);
                //        v[2]  = k_madd_epi32(u[2], k32_p27_p05);
                //        v[3]  = k_madd_epi32(u[3], k32_p27_p05);
                //        v[4]  = k_madd_epi32(u[4], k32_p11_p21);
                //        v[5]  = k_madd_epi32(u[5], k32_p11_p21);
                //        v[6]  = k_madd_epi32(u[6], k32_p11_p21);
                //        v[7]  = k_madd_epi32(u[7], k32_p11_p21);
                //        v[8]  = k_madd_epi32(u[8], k32_p19_p13);
                //        v[9] = k_madd_epi32(u[9], k32_p19_p13);
                //        v[10] = k_madd_epi32(u[10], k32_p19_p13);
                //        v[11] = k_madd_epi32(u[11], k32_p19_p13);
                //        v[12] = k_madd_epi32(u[12], k32_p03_p29);
                //        v[13] = k_madd_epi32(u[13], k32_p03_p29);
                //        v[14] = k_madd_epi32(u[14], k32_p03_p29);
                //        v[15] = k_madd_epi32(u[15], k32_p03_p29);
                //        v[16] = k_madd_epi32(u[12], k32_m29_p03);
                //        v[17] = k_madd_epi32(u[13], k32_m29_p03);
                //        v[18] = k_madd_epi32(u[14], k32_m29_p03);
                //        v[19] = k_madd_epi32(u[15], k32_m29_p03);
                //        v[20] = k_madd_epi32(u[8], k32_m13_p19);
                //        v[21] = k_madd_epi32(u[9], k32_m13_p19);
                //        v[22] = k_madd_epi32(u[10], k32_m13_p19);
                //        v[23] = k_madd_epi32(u[11], k32_m13_p19);
                //        v[24] = k_madd_epi32(u[4], k32_m21_p11);
                //        v[25] = k_madd_epi32(u[5], k32_m21_p11);
                //        v[26] = k_madd_epi32(u[6], k32_m21_p11);
                //        v[27] = k_madd_epi32(u[7], k32_m21_p11);
                //        v[28] = k_madd_epi32(u[0], k32_m05_p27);
                //        v[29] = k_madd_epi32(u[1], k32_m05_p27);
                //        v[30] = k_madd_epi32(u[2], k32_m05_p27);
                //        v[31] = k_madd_epi32(u[3], k32_m05_p27);

                //        u[0]  = k_packs_epi64(v[0], v[1]);
                //        u[1]  = k_packs_epi64(v[2], v[3]);
                //        u[2]  = k_packs_epi64(v[4], v[5]);
                //        u[3]  = k_packs_epi64(v[6], v[7]);
                //        u[4]  = k_packs_epi64(v[8], v[9]);
                //        u[5]  = k_packs_epi64(v[10], v[11]);
                //        u[6]  = k_packs_epi64(v[12], v[13]);
                //        u[7]  = k_packs_epi64(v[14], v[15]);
                //        u[8]  = k_packs_epi64(v[16], v[17]);
                //        u[9]  = k_packs_epi64(v[18], v[19]);
                //        u[10] = k_packs_epi64(v[20], v[21]);
                //        u[11] = k_packs_epi64(v[22], v[23]);
                //        u[12] = k_packs_epi64(v[24], v[25]);
                //        u[13] = k_packs_epi64(v[26], v[27]);
                //        u[14] = k_packs_epi64(v[28], v[29]);
                //        u[15] = k_packs_epi64(v[30], v[31]);

                //        v[0]  = _mm256_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                //        v[1]  = _mm256_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                //        v[2]  = _mm256_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                //        v[3]  = _mm256_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                //        v[4]  = _mm256_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                //        v[5]  = _mm256_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                //        v[6]  = _mm256_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                //        v[7]  = _mm256_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                //        v[8]  = _mm256_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                //        v[9]  = _mm256_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                //        v[10] = _mm256_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                //        v[11] = _mm256_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                //        v[12] = _mm256_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                //        v[13] = _mm256_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                //        v[14] = _mm256_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                //        v[15] = _mm256_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                //        u[0]  = _mm256_srai_epi32(v[0], DCT_CONST_BITS);
                //        u[1]  = _mm256_srai_epi32(v[1], DCT_CONST_BITS);
                //        u[2]  = _mm256_srai_epi32(v[2], DCT_CONST_BITS);
                //        u[3]  = _mm256_srai_epi32(v[3], DCT_CONST_BITS);
                //        u[4]  = _mm256_srai_epi32(v[4], DCT_CONST_BITS);
                //        u[5]  = _mm256_srai_epi32(v[5], DCT_CONST_BITS);
                //        u[6]  = _mm256_srai_epi32(v[6], DCT_CONST_BITS);
                //        u[7]  = _mm256_srai_epi32(v[7], DCT_CONST_BITS);
                //        u[8]  = _mm256_srai_epi32(v[8], DCT_CONST_BITS);
                //        u[9]  = _mm256_srai_epi32(v[9], DCT_CONST_BITS);
                //        u[10] = _mm256_srai_epi32(v[10], DCT_CONST_BITS);
                //        u[11] = _mm256_srai_epi32(v[11], DCT_CONST_BITS);
                //        u[12] = _mm256_srai_epi32(v[12], DCT_CONST_BITS);
                //        u[13] = _mm256_srai_epi32(v[13], DCT_CONST_BITS);
                //        u[14] = _mm256_srai_epi32(v[14], DCT_CONST_BITS);
                //        u[15] = _mm256_srai_epi32(v[15], DCT_CONST_BITS);

                //        v[0]  = _mm256_cmpgt_epi32(kZero, u[0]);
                //        v[1]  = _mm256_cmpgt_epi32(kZero, u[1]);
                //        v[2]  = _mm256_cmpgt_epi32(kZero, u[2]);
                //        v[3]  = _mm256_cmpgt_epi32(kZero, u[3]);
                //        v[4]  = _mm256_cmpgt_epi32(kZero, u[4]);
                //        v[5]  = _mm256_cmpgt_epi32(kZero, u[5]);
                //        v[6]  = _mm256_cmpgt_epi32(kZero, u[6]);
                //        v[7]  = _mm256_cmpgt_epi32(kZero, u[7]);
                //        v[8]  = _mm256_cmpgt_epi32(kZero, u[8]);
                //        v[9]  = _mm256_cmpgt_epi32(kZero, u[9]);
                //        v[10] = _mm256_cmpgt_epi32(kZero, u[10]);
                //        v[11] = _mm256_cmpgt_epi32(kZero, u[11]);
                //        v[12] = _mm256_cmpgt_epi32(kZero, u[12]);
                //        v[13] = _mm256_cmpgt_epi32(kZero, u[13]);
                //        v[14] = _mm256_cmpgt_epi32(kZero, u[14]);
                //        v[15] = _mm256_cmpgt_epi32(kZero, u[15]);

                //        u[0] = _mm256_sub_epi32(u[0], v[0]);
                //        u[1] = _mm256_sub_epi32(u[1], v[1]);
                //        u[2] = _mm256_sub_epi32(u[2], v[2]);
                //        u[3] = _mm256_sub_epi32(u[3], v[3]);
                //        u[4] = _mm256_sub_epi32(u[4], v[4]);
                //        u[5] = _mm256_sub_epi32(u[5], v[5]);
                //        u[6] = _mm256_sub_epi32(u[6], v[6]);
                //        u[7] = _mm256_sub_epi32(u[7], v[7]);
                //        u[8] = _mm256_sub_epi32(u[8], v[8]);
                //        u[9] = _mm256_sub_epi32(u[9], v[9]);
                //        u[10] = _mm256_sub_epi32(u[10], v[10]);
                //        u[11] = _mm256_sub_epi32(u[11], v[11]);
                //        u[12] = _mm256_sub_epi32(u[12], v[12]);
                //        u[13] = _mm256_sub_epi32(u[13], v[13]);
                //        u[14] = _mm256_sub_epi32(u[14], v[14]);
                //        u[15] = _mm256_sub_epi32(u[15], v[15]);

                //        v[0] = _mm256_add_epi32(u[0], K32One);
                //        v[1] = _mm256_add_epi32(u[1], K32One);
                //        v[2] = _mm256_add_epi32(u[2], K32One);
                //        v[3] = _mm256_add_epi32(u[3], K32One);
                //        v[4] = _mm256_add_epi32(u[4], K32One);
                //        v[5] = _mm256_add_epi32(u[5], K32One);
                //        v[6] = _mm256_add_epi32(u[6], K32One);
                //        v[7] = _mm256_add_epi32(u[7], K32One);
                //        v[8] = _mm256_add_epi32(u[8], K32One);
                //        v[9] = _mm256_add_epi32(u[9], K32One);
                //        v[10] = _mm256_add_epi32(u[10], K32One);
                //        v[11] = _mm256_add_epi32(u[11], K32One);
                //        v[12] = _mm256_add_epi32(u[12], K32One);
                //        v[13] = _mm256_add_epi32(u[13], K32One);
                //        v[14] = _mm256_add_epi32(u[14], K32One);
                //        v[15] = _mm256_add_epi32(u[15], K32One);

                //        u[0] = _mm256_srai_epi32(v[0], 2);
                //        u[1] = _mm256_srai_epi32(v[1], 2);
                //        u[2] = _mm256_srai_epi32(v[2], 2);
                //        u[3] = _mm256_srai_epi32(v[3], 2);
                //        u[4] = _mm256_srai_epi32(v[4], 2);
                //        u[5] = _mm256_srai_epi32(v[5], 2);
                //        u[6] = _mm256_srai_epi32(v[6], 2);
                //        u[7] = _mm256_srai_epi32(v[7], 2);
                //        u[8] = _mm256_srai_epi32(v[8], 2);
                //        u[9] = _mm256_srai_epi32(v[9], 2);
                //        u[10] = _mm256_srai_epi32(v[10], 2);
                //        u[11] = _mm256_srai_epi32(v[11], 2);
                //        u[12] = _mm256_srai_epi32(v[12], 2);
                //        u[13] = _mm256_srai_epi32(v[13], 2);
                //        u[14] = _mm256_srai_epi32(v[14], 2);
                //        u[15] = _mm256_srai_epi32(v[15], 2);

                //        out[5] = _mm256_packs_epi32(u[0], u[1]);
                //        out[21] = _mm256_packs_epi32(u[2], u[3]);
                //        out[13] = _mm256_packs_epi32(u[4], u[5]);
                //        out[29] = _mm256_packs_epi32(u[6], u[7]);
                //        out[3] = _mm256_packs_epi32(u[8], u[9]);
                //        out[19] = _mm256_packs_epi32(u[10], u[11]);
                //        out[11] = _mm256_packs_epi32(u[12], u[13]);
                //        out[27] = _mm256_packs_epi32(u[14], u[15]);
                //    }
                //}
//#endif  // FDCT32x32_HIGH_PRECISION
                // Transpose the results, do it as four 8x8 transposes.
                if (pass == 0) {
                    transpose_and_output16x16<true>(out, poutput, 32);
                    transpose_and_output16x16<true>(out+16, poutput+16, 32);
                } else {
                    transpose_and_output16x16<false>(out, poutput, 32);
                    transpose_and_output16x16<false>(out + 16, poutput+16, 32);
                }
                poutput += 16 * 32;
            }
            poutput = output;
        }
    }
};  // namespace details

namespace VP9PP {
    template <int size, int type> void ftransform_vp9_avx2(const short *src, short *dst, int pitchSrc);

    template <> void ftransform_vp9_avx2<TX_8X8, DCT_DCT>(const short *src, short *dst, int pitchSrc) {
        details::fht8x8_avx2<DCT_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_8X8, ADST_DCT>(const short *src, short *dst, int pitchSrc) {
        details::fht8x8_avx2<ADST_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_8X8, DCT_ADST>(const short *src, short *dst, int pitchSrc) {
        details::fht8x8_avx2<DCT_ADST>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_8X8, ADST_ADST>(const short *src, short *dst, int pitchSrc) {
        details::fht8x8_avx2<ADST_ADST>(src, dst, pitchSrc);
    }

    template <> void ftransform_vp9_avx2<TX_16X16, DCT_DCT>(const short *src, short *dst, int pitchSrc) {
        details::fht16x16_avx2<DCT_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_16X16, ADST_DCT>(const short *src, short *dst, int pitchSrc) {
        details::fht16x16_avx2<ADST_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_16X16, DCT_ADST>(const short *src, short *dst, int pitchSrc) {
        details::fht16x16_avx2<DCT_ADST>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_avx2<TX_16X16, ADST_ADST>(const short *src, short *dst, int pitchSrc) {
        details::fht16x16_avx2<ADST_ADST>(src, dst, pitchSrc);
    }

    template <> void ftransform_vp9_avx2<TX_32X32, DCT_DCT>(const short *src, short *dst, int pitchSrc) {
        details::fdct32x32_avx2(src, dst, pitchSrc);
    }

    void fdct32x32_fast_avx2(const short *src, short *dst, int pitchSrc) {
        return details::fdct32x32_fast_avx2(src, dst, pitchSrc);
    }

};  // namespace VP9PP