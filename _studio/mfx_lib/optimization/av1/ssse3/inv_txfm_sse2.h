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

#include <emmintrin.h>  // SSE2
#include <cstdint>

// perform 8x8 transpose
namespace AV1PP {
    static inline void array_transpose_8x8(__m128i *in, __m128i *res)
    {
        const __m128i tr0_0 = _mm_unpacklo_epi16(in[0], in[1]);
        const __m128i tr0_1 = _mm_unpacklo_epi16(in[2], in[3]);
        const __m128i tr0_2 = _mm_unpackhi_epi16(in[0], in[1]);
        const __m128i tr0_3 = _mm_unpackhi_epi16(in[2], in[3]);
        const __m128i tr0_4 = _mm_unpacklo_epi16(in[4], in[5]);
        const __m128i tr0_5 = _mm_unpacklo_epi16(in[6], in[7]);
        const __m128i tr0_6 = _mm_unpackhi_epi16(in[4], in[5]);
        const __m128i tr0_7 = _mm_unpackhi_epi16(in[6], in[7]);

        const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
        const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_4, tr0_5);
        const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
        const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_4, tr0_5);
        const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_2, tr0_3);
        const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
        const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_2, tr0_3);
        const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);

        res[0] = _mm_unpacklo_epi64(tr1_0, tr1_1);
        res[1] = _mm_unpackhi_epi64(tr1_0, tr1_1);
        res[2] = _mm_unpacklo_epi64(tr1_2, tr1_3);
        res[3] = _mm_unpackhi_epi64(tr1_2, tr1_3);
        res[4] = _mm_unpacklo_epi64(tr1_4, tr1_5);
        res[5] = _mm_unpackhi_epi64(tr1_4, tr1_5);
        res[6] = _mm_unpacklo_epi64(tr1_6, tr1_7);
        res[7] = _mm_unpackhi_epi64(tr1_6, tr1_7);
    }

#define TRANSPOSE_8X4(in0, in1, in2, in3, out0, out1)   \
    {                                                   \
    const __m128i tr0_0 = _mm_unpacklo_epi16(in0, in1); \
    const __m128i tr0_1 = _mm_unpacklo_epi16(in2, in3); \
    in0 = _mm_unpacklo_epi32(tr0_0, tr0_1); /* i1 i0 */ \
    in1 = _mm_unpackhi_epi32(tr0_0, tr0_1); /* i3 i2 */ \
    }

    static inline void array_transpose_4X8(__m128i *in, __m128i *out) {
        const __m128i tr0_0 = _mm_unpacklo_epi16(in[0], in[1]);
        const __m128i tr0_1 = _mm_unpacklo_epi16(in[2], in[3]);
        const __m128i tr0_4 = _mm_unpacklo_epi16(in[4], in[5]);
        const __m128i tr0_5 = _mm_unpacklo_epi16(in[6], in[7]);

        const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
        const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
        const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_4, tr0_5);
        const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_4, tr0_5);

        out[0] = _mm_unpacklo_epi64(tr1_0, tr1_4);
        out[1] = _mm_unpackhi_epi64(tr1_0, tr1_4);
        out[2] = _mm_unpacklo_epi64(tr1_2, tr1_6);
        out[3] = _mm_unpackhi_epi64(tr1_2, tr1_6);
    }

    static inline void array_transpose_16x16(__m128i *res0, __m128i *res1) {
        __m128i tbuf[8];
        array_transpose_8x8(res0, res0);
        array_transpose_8x8(res1, tbuf);
        array_transpose_8x8(res0 + 8, res1);
        array_transpose_8x8(res1 + 8, res1 + 8);

        res0[8] = tbuf[0];
        res0[9] = tbuf[1];
        res0[10] = tbuf[2];
        res0[11] = tbuf[3];
        res0[12] = tbuf[4];
        res0[13] = tbuf[5];
        res0[14] = tbuf[6];
        res0[15] = tbuf[7];
    }

    // Function to allow 8 bit optimisations to be used when profile 0 is used with
    // highbitdepth enabled
    static inline __m128i load_input_data(const int16_t *data) {
#if CONFIG_AV1_HIGHBITDEPTH
        return octa_set_epi16(data[0], data[1], data[2], data[3], data[4], data[5],
            data[6], data[7]);
#else
        return _mm_load_si128((const __m128i *)data);
#endif
    }

    static inline void load_buffer_8x16(const int16_t *input, __m128i *in) {
        in[0] = load_input_data(input + 0 * 16);
        in[1] = load_input_data(input + 1 * 16);
        in[2] = load_input_data(input + 2 * 16);
        in[3] = load_input_data(input + 3 * 16);
        in[4] = load_input_data(input + 4 * 16);
        in[5] = load_input_data(input + 5 * 16);
        in[6] = load_input_data(input + 6 * 16);
        in[7] = load_input_data(input + 7 * 16);

        in[8] = load_input_data(input + 8 * 16);
        in[9] = load_input_data(input + 9 * 16);
        in[10] = load_input_data(input + 10 * 16);
        in[11] = load_input_data(input + 11 * 16);
        in[12] = load_input_data(input + 12 * 16);
        in[13] = load_input_data(input + 13 * 16);
        in[14] = load_input_data(input + 14 * 16);
        in[15] = load_input_data(input + 15 * 16);
    }

#define RECON_AND_STORE(dest, in_x)                  \
    {                                                \
    __m128i d0 = _mm_loadl_epi64((__m128i *)(dest)); \
    d0 = _mm_unpacklo_epi8(d0, zero);                \
    d0 = _mm_add_epi16(in_x, d0);                    \
    d0 = _mm_packus_epi16(d0, d0);                   \
    _mm_storel_epi64((__m128i *)(dest), d0);         \
    }

    template <typename T> inline void recon_and_store(T *dest, __m128i in_x);
    template <> inline void recon_and_store<uint8_t>(uint8_t *dest, __m128i in_x) {
        __m128i d0 = _mm_loadl_epi64((__m128i *)(dest));
        d0 = _mm_unpacklo_epi8(d0, _mm_setzero_si128());
        d0 = _mm_add_epi16(in_x, d0);
        d0 = _mm_packus_epi16(d0, d0);
        _mm_storel_epi64((__m128i *)(dest), d0);
    }
    template <> inline void recon_and_store<int16_t>(int16_t *dest, __m128i in_x) {
        _mm_store_si128((__m128i *)(dest), in_x);
    }

    template <typename T> inline void write_buffer_8x16(T *dest, __m128i *in, int stride) {
        const __m128i final_rounding = _mm_set1_epi16(1 << 5);
        //const __m128i zero = _mm_setzero_si128();
        // Final rounding and shift
        in[0] = _mm_adds_epi16(in[0], final_rounding);
        in[1] = _mm_adds_epi16(in[1], final_rounding);
        in[2] = _mm_adds_epi16(in[2], final_rounding);
        in[3] = _mm_adds_epi16(in[3], final_rounding);
        in[4] = _mm_adds_epi16(in[4], final_rounding);
        in[5] = _mm_adds_epi16(in[5], final_rounding);
        in[6] = _mm_adds_epi16(in[6], final_rounding);
        in[7] = _mm_adds_epi16(in[7], final_rounding);
        in[8] = _mm_adds_epi16(in[8], final_rounding);
        in[9] = _mm_adds_epi16(in[9], final_rounding);
        in[10] = _mm_adds_epi16(in[10], final_rounding);
        in[11] = _mm_adds_epi16(in[11], final_rounding);
        in[12] = _mm_adds_epi16(in[12], final_rounding);
        in[13] = _mm_adds_epi16(in[13], final_rounding);
        in[14] = _mm_adds_epi16(in[14], final_rounding);
        in[15] = _mm_adds_epi16(in[15], final_rounding);

        in[0] = _mm_srai_epi16(in[0], 6);
        in[1] = _mm_srai_epi16(in[1], 6);
        in[2] = _mm_srai_epi16(in[2], 6);
        in[3] = _mm_srai_epi16(in[3], 6);
        in[4] = _mm_srai_epi16(in[4], 6);
        in[5] = _mm_srai_epi16(in[5], 6);
        in[6] = _mm_srai_epi16(in[6], 6);
        in[7] = _mm_srai_epi16(in[7], 6);
        in[8] = _mm_srai_epi16(in[8], 6);
        in[9] = _mm_srai_epi16(in[9], 6);
        in[10] = _mm_srai_epi16(in[10], 6);
        in[11] = _mm_srai_epi16(in[11], 6);
        in[12] = _mm_srai_epi16(in[12], 6);
        in[13] = _mm_srai_epi16(in[13], 6);
        in[14] = _mm_srai_epi16(in[14], 6);
        in[15] = _mm_srai_epi16(in[15], 6);

        recon_and_store(dest + 0 * stride, in[0]);
        recon_and_store(dest + 1 * stride, in[1]);
        recon_and_store(dest + 2 * stride, in[2]);
        recon_and_store(dest + 3 * stride, in[3]);
        recon_and_store(dest + 4 * stride, in[4]);
        recon_and_store(dest + 5 * stride, in[5]);
        recon_and_store(dest + 6 * stride, in[6]);
        recon_and_store(dest + 7 * stride, in[7]);
        recon_and_store(dest + 8 * stride, in[8]);
        recon_and_store(dest + 9 * stride, in[9]);
        recon_and_store(dest + 10 * stride, in[10]);
        recon_and_store(dest + 11 * stride, in[11]);
        recon_and_store(dest + 12 * stride, in[12]);
        recon_and_store(dest + 13 * stride, in[13]);
        recon_and_store(dest + 14 * stride, in[14]);
        recon_and_store(dest + 15 * stride, in[15]);
    }

    //void idct4_sse2(__m128i *in);
    //void idct8_sse2(__m128i *in);
    //void idct16_sse2(__m128i *in0, __m128i *in1);
    //void iadst4_sse2(__m128i *in);
    //void iadst8_sse2(__m128i *in);
    //void iadst16_sse2(__m128i *in0, __m128i *in1);

}  // namespace AV1PP