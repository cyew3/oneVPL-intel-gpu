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

#include <stdint.h>
#include <assert.h>

#include <immintrin.h>
#ifdef WIN32
#include <intrin.h>
#endif
#include "mfx_av1_cfl.h"

static constexpr int32_t BSR(uint32_t u32) {
    return u32 == 1?0:BSR(u32>>1)+1;
}

// Returns a vector where all the (32-bits) elements are the sum of all the
// lanes in a.
static inline __m256i fill_sum_epi32(__m256i a) {
    // Given that a == [A, B, C, D, E, F, G, H]
    a = _mm256_hadd_epi32(a, a);
    // Given that A' == A + B, C' == C + D, E' == E + F, G' == G + H
    // a == [A', C', A', C', E', G', E', G']
    a = _mm256_permute4x64_epi64(a, _MM_SHUFFLE(3, 1, 2, 0));
    // a == [A', C', E', G', A', C', E', G']
    a = _mm256_hadd_epi32(a, a);
    // Given that A'' == A' + C' and E'' == E' + G'
    // a == [A'', E'', A'', E'', A'', E'', A'', E'']
    return _mm256_hadd_epi32(a, a);
    // Given that A''' == A'' + E''
    // a == [A''', A''', A''', A''', A''', A''', A''', A''']
}

static inline __m256i _mm256_addl_epi16(__m256i a) {
    return _mm256_add_epi32(_mm256_unpacklo_epi16(a, _mm256_setzero_si256()),
        _mm256_unpackhi_epi16(a, _mm256_setzero_si256()));
}

static void subtract_average_avx2_impl(const uint16_t *src_ptr,
    int16_t *dst_ptr, int width,
    int height, int round_offset,
    int num_pel_log2) {
    // Use SSE2 version for smaller widths
    assert(width == 16 || width == 32);

    const __m256i *src = (__m256i *)src_ptr;
    const __m256i *const end = src + height * CFL_BUF_LINE_I256;
    // To maximize usage of the AVX2 registers, we sum two rows per loop
    // iteration
    const int step = 2 * CFL_BUF_LINE_I256;

    __m256i sum = _mm256_setzero_si256();
    // For width 32, we use a second sum accumulator to reduce accumulator
    // dependencies in the loop.
    __m256i sum2;
    if (width == 32) sum2 = _mm256_setzero_si256();

    do {
        // Add top row to the bottom row
        __m256i l0 = _mm256_add_epi16(_mm256_loadu_si256(src),
            _mm256_loadu_si256(src + CFL_BUF_LINE_I256));
        sum = _mm256_add_epi32(sum, _mm256_addl_epi16(l0));
        if (width == 32) { /* Don't worry, this if it gets optimized out. */
          // Add the second part of the top row to the second part of the bottom row
            __m256i l1 =
                _mm256_add_epi16(_mm256_loadu_si256(src + 1),
                    _mm256_loadu_si256(src + 1 + CFL_BUF_LINE_I256));
            sum2 = _mm256_add_epi32(sum2, _mm256_addl_epi16(l1));
        }
        src += step;
    } while (src < end);
    // Combine both sum accumulators
    if (width == 32) sum = _mm256_add_epi32(sum, sum2);

    __m256i fill = fill_sum_epi32(sum);

    __m256i avg_epi16 = _mm256_srli_epi32(
        _mm256_add_epi32(fill, _mm256_set1_epi32(round_offset)), num_pel_log2);
    avg_epi16 = _mm256_packs_epi32(avg_epi16, avg_epi16);

    // Store and subtract loop
    src = (__m256i *)src_ptr;
    __m256i *dst = (__m256i *)dst_ptr;
    do {
        _mm256_storeu_si256(dst,
            _mm256_sub_epi16(_mm256_loadu_si256(src), avg_epi16));
        if (width == 32) {
            _mm256_storeu_si256(
                dst + 1, _mm256_sub_epi16(_mm256_loadu_si256(src + 1), avg_epi16));
        }
        src += CFL_BUF_LINE_I256;
        dst += CFL_BUF_LINE_I256;
    } while (src < end);
}

namespace AV1PP {
    template<int width, int height> void subtract_average_avx2(const uint16_t *src, int16_t *dst) {
        constexpr int sz = width * height;
        constexpr int num_pel_log2 = BSR(sz);
        constexpr int round_offset = ((sz) >> 1);

        subtract_average_avx2_impl(src, dst, width, height, round_offset, num_pel_log2);
    }
    template void subtract_average_avx2< 4, 4>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2< 4, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2< 4, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2< 8, 4>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2< 8, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2< 8, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2< 8, 32>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2<16, 4>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2<16, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2<16, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2<16, 32>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2<32, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2<32, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_avx2<32, 32>(const uint16_t *src, int16_t *dst);
};

/**
 * Adds 4 pixels (in a 2x2 grid) and multiplies them by 2. Resulting in a more
 * precise version of a box filter 4:2:0 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 *
 * Note: For 4:2:0 luma subsampling, the width will never be greater than 16.
 */
static void cfl_luma_subsampling_420_u8_avx2_impl(const uint8_t *input,
    int input_stride,
    uint16_t *pred_buf_q3, int width,
    int height) {
    (void)width;                               // Forever 32
    const __m256i twos = _mm256_set1_epi8(2);  // Thirty two twos
    const int luma_stride = input_stride << 1;
    __m256i *row = (__m256i *)pred_buf_q3;
    const __m256i *row_end = row + (height >> 1) * CFL_BUF_LINE_I256;
    do {
        __m256i top = _mm256_loadu_si256((__m256i *)input);
        __m256i bot = _mm256_loadu_si256((__m256i *)(input + input_stride));

        __m256i top_16x16 = _mm256_maddubs_epi16(top, twos);
        __m256i bot_16x16 = _mm256_maddubs_epi16(bot, twos);
        __m256i sum_16x16 = _mm256_add_epi16(top_16x16, bot_16x16);

        _mm256_storeu_si256(row, sum_16x16);

        input += luma_stride;
    } while ((row += CFL_BUF_LINE_I256) < row_end);
}

/**
 * Adds 2 pixels (in a 2x1 grid) and multiplies them by 4. Resulting in a more
 * precise version of a box filter 4:2:2 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
    static void cfl_luma_subsampling_422_u8_avx2_impl(const uint8_t *input,
        int input_stride,
        uint16_t *pred_buf_q3, int width,
        int height) {
    (void)width;                                // Forever 32
    const __m256i fours = _mm256_set1_epi8(4);  // Thirty two fours
    __m256i *row = (__m256i *)pred_buf_q3;
    const __m256i *row_end = row + height * CFL_BUF_LINE_I256;
    do {
        __m256i top = _mm256_loadu_si256((__m256i *)input);
        __m256i top_16x16 = _mm256_maddubs_epi16(top, fours);
        _mm256_storeu_si256(row, top_16x16);
        input += input_stride;
    } while ((row += CFL_BUF_LINE_I256) < row_end);
}

    /**
     * Multiplies the pixels by 8 (scaling in Q3). The AVX2 subsampling is only
     * performed on block of width 32.
     *
     * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
     * active area is specified using width and height.
     *
     * Note: We don't need to worry about going over the active area, as long as we
     * stay inside the CfL prediction buffer.
     */
    static void cfl_luma_subsampling_444_u8_avx2_impl(const uint8_t *input,
        int input_stride,
        uint16_t *pred_buf_q3, int width,
        int height) {
        (void)width;  // Forever 32
        __m256i *row = (__m256i *)pred_buf_q3;
        const __m256i *row_end = row + height * CFL_BUF_LINE_I256;
        const __m256i zeros = _mm256_setzero_si256();
        do {
            __m256i top = _mm256_loadu_si256((__m256i *)input);
            top = _mm256_permute4x64_epi64(top, _MM_SHUFFLE(3, 1, 2, 0));

            __m256i row_lo = _mm256_unpacklo_epi8(top, zeros);
            row_lo = _mm256_slli_epi16(row_lo, 3);
            __m256i row_hi = _mm256_unpackhi_epi8(top, zeros);
            row_hi = _mm256_slli_epi16(row_hi, 3);

            _mm256_storeu_si256(row, row_lo);
            _mm256_storeu_si256(row + 1, row_hi);

            input += input_stride;
        } while ((row += CFL_BUF_LINE_I256) < row_end);
    }

/**
 * Adds 4 pixels (in a 2x2 grid) and multiplies them by 2. Resulting in a more
 * precise version of a box filter 4:2:0 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 *
 * Note: For 4:2:0 luma subsampling, the width will never be greater than 16.
 */
static void cfl_luma_subsampling_420_u16_avx2_impl(const uint16_t *input,
    int input_stride,
    uint16_t *pred_buf_q3, int width,
    int height) {
    (void)width;  // Forever 32
    const int luma_stride = input_stride << 1;
    __m256i *row = (__m256i *)pred_buf_q3;
    const __m256i *row_end = row + (height >> 1) * CFL_BUF_LINE_I256;
    do {
        __m256i top = _mm256_loadu_si256((__m256i *)input);
        __m256i bot = _mm256_loadu_si256((__m256i *)(input + input_stride));
        __m256i sum = _mm256_add_epi16(top, bot);

        __m256i top_1 = _mm256_loadu_si256((__m256i *)(input + 16));
        __m256i bot_1 = _mm256_loadu_si256((__m256i *)(input + 16 + input_stride));
        __m256i sum_1 = _mm256_add_epi16(top_1, bot_1);

        __m256i hsum = _mm256_hadd_epi16(sum, sum_1);
        hsum = _mm256_permute4x64_epi64(hsum, _MM_SHUFFLE(3, 1, 2, 0));
        hsum = _mm256_add_epi16(hsum, hsum);

        _mm256_storeu_si256(row, hsum);

        input += luma_stride;
    } while ((row += CFL_BUF_LINE_I256) < row_end);
}

/**
 * Adds 2 pixels (in a 2x1 grid) and multiplies them by 4. Resulting in a more
 * precise version of a box filter 4:2:2 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 *
 */
static void cfl_luma_subsampling_422_u16_avx2_impl(const uint16_t *input,
    int input_stride,
    uint16_t *pred_buf_q3, int width,
    int height) {
    (void)width;  // Forever 32
    __m256i *row = (__m256i *)pred_buf_q3;
    const __m256i *row_end = row + height * CFL_BUF_LINE_I256;
    do {
        __m256i top = _mm256_loadu_si256((__m256i *)input);
        __m256i top_1 = _mm256_loadu_si256((__m256i *)(input + 16));
        __m256i hsum = _mm256_hadd_epi16(top, top_1);
        hsum = _mm256_permute4x64_epi64(hsum, _MM_SHUFFLE(3, 1, 2, 0));
        hsum = _mm256_slli_epi16(hsum, 2);

        _mm256_storeu_si256(row, hsum);

        input += input_stride;
    } while ((row += CFL_BUF_LINE_I256) < row_end);
}

static void cfl_luma_subsampling_444_u16_avx2_impl(const uint16_t *input,
    int input_stride,
    uint16_t *pred_buf_q3, int width,
    int height) {
    (void)width;  // Forever 32
    __m256i *row = (__m256i *)pred_buf_q3;
    const __m256i *row_end = row + height * CFL_BUF_LINE_I256;
    do {
        __m256i top = _mm256_loadu_si256((__m256i *)input);
        __m256i top_1 = _mm256_loadu_si256((__m256i *)(input + 16));
        _mm256_storeu_si256(row, _mm256_slli_epi16(top, 3));
        _mm256_storeu_si256(row + 1, _mm256_slli_epi16(top_1, 3));
        input += input_stride;
    } while ((row += CFL_BUF_LINE_I256) < row_end);
}

namespace AV1PP {
    template<int width, int height> void cfl_luma_subsampling_420_u8_avx2(const uint8_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_420_u8_avx2_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_420_u8_avx2< 4, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2< 4, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2< 4, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2< 8, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2< 8, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2< 8, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2< 8, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2<16, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2<16, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2<16, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2<16, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2<32, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2<32, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_avx2<32, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_420_u16_avx2(const uint16_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_420_u16_avx2_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_420_u16_avx2< 4, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2< 4, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2< 4, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2< 8, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2< 8, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2< 8, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2< 8, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2<16, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2<16, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2<16, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2<16, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2<32, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2<32, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_avx2<32, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_422_u8_avx2(const uint8_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_422_u8_avx2_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_422_u8_avx2< 4, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2< 4, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2< 4, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2< 8, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2< 8, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2< 8, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2< 8, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2<16, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2<16, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2<16, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2<16, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2<32, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2<32, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_avx2<32, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_422_u16_avx2(const uint16_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_422_u16_avx2_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_422_u16_avx2< 4, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2< 4, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2< 4, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2< 8, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2< 8, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2< 8, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2< 8, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2<16, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2<16, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2<16, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2<16, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2<32, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2<32, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_avx2<32, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_444_u8_avx2(const uint8_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_444_u8_avx2_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_444_u8_avx2< 4, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2< 4, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2< 4, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2< 8, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2< 8, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2< 8, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2< 8, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2<16, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2<16, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2<16, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2<16, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2<32, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2<32, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_avx2<32, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_444_u16_avx2(const uint16_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_444_u16_avx2_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_444_u16_avx2< 4, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2< 4, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2< 4, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2< 8, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2< 8, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2< 8, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2< 8, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2<16, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2<16, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2<16, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2<16, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2<32, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2<32, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_avx2<32, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);

};

static inline __m256i predict_unclipped(const __m256i *input, __m256i alpha_q12, __m256i alpha_sign) {
    __m256i ac_q3 = _mm256_loadu_si256(input);
    __m256i ac_sign = _mm256_sign_epi16(alpha_sign, ac_q3);
    __m256i scaled_luma_q0 =
        _mm256_mulhrs_epi16(_mm256_abs_epi16(ac_q3), alpha_q12);
    scaled_luma_q0 = _mm256_sign_epi16(scaled_luma_q0, ac_sign);
    return scaled_luma_q0;
}

static void cfl_predict_nv12_u8_avx2_impl(const int16_t *pred_buf_q3, uint8_t *dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int /*bd*/, int width, int height) {
    (void)width; //=32
    const __m256i alpha_sign = _mm256_set1_epi16(alpha_q3);
    const __m256i alpha_q12 = _mm256_slli_epi16(_mm256_abs_epi16(alpha_sign), 9);
    const __m256i dc_q0 = _mm256_set1_epi16(dcU | (dcV << 16));
    __m256i *row = (__m256i *)pred_buf_q3;
    const __m256i *row_end = row + height * CFL_BUF_LINE_I256;

    do {
        __m256i res = predict_unclipped(row, alpha_q12, alpha_sign);
        res = _mm256_add_epi16(res, dc_q0);
        __m256i next = predict_unclipped(row + 1, alpha_q12, alpha_sign);
        next = _mm256_add_epi16(next, dc_q0);
        res = _mm256_packus_epi16(res, next);
        res = _mm256_permute4x64_epi64(res, _MM_SHUFFLE(3, 1, 2, 0));
        _mm256_storeu_si256((__m256i *)dst, res);
        dst += dst_stride;
    } while ((row += CFL_BUF_LINE_I256) < row_end);
}

static void cfl_predict_nv12_u8_16_avx2_impl(const int16_t *pred_buf_q3, uint8_t *dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int /*bd*/, int width, int height) {
    (void)width; //=16
    const __m256i alpha_sign = _mm256_set1_epi16(alpha_q3);
    const __m256i alpha_q12 = _mm256_slli_epi16(_mm256_abs_epi16(alpha_sign), 9);
    //const __m256i dc_q0 = _mm256_set1_epi16(dcU | (dcV << 16));
    const __m128i dc_q0 = _mm_set1_epi32(dcU | (dcV << 16));
    __m256i *row = (__m256i *)pred_buf_q3;
    const __m256i *row_end = row + height * CFL_BUF_LINE_I256;

    do {
        __m256i v = predict_unclipped(row, alpha_q12, alpha_sign);

        __m128i res = _mm256_extracti128_si256(v, 0);
        __m128i v1 = _mm_unpacklo_epi16(res, res);
        v1 = _mm_add_epi16(v1, dc_q0);
        __m128i v2 = _mm_unpackhi_epi16(res, res);
        v2 = _mm_add_epi16(v2, dc_q0);
        res = _mm_packus_epi16(v1, v2);

        __m128i next = _mm256_extracti128_si256(v, 1);
        __m128i next1 = _mm_unpacklo_epi16(next, next);
        next1 = _mm_add_epi16(next1, dc_q0);
        __m128i next2 = _mm_unpackhi_epi16(next, next);
        next2 = _mm_add_epi16(next2, dc_q0);
        next = _mm_packus_epi16(next1, next2);

        _mm256_storeu_si256((__m256i *)dst, _mm256_set_m128i(next, res));
        dst += dst_stride;
    } while ((row += CFL_BUF_LINE_I256) < row_end);
}

namespace AV1PP {
    template<typename PixType, int width, int height> void cfl_predict_nv12_avx2(const int16_t *pred_buf_q3, PixType *dst, int dst_stride, PixType dcU, PixType dcV, int alpha_q3, int bd) {
        cfl_predict_nv12_u8_avx2_impl(pred_buf_q3, (uint8_t*)dst, dst_stride, (uint8_t)dcU, (uint8_t)dcV, alpha_q3, bd, width, height);
    }

    template void cfl_predict_nv12_avx2<uint8_t, 4, 4>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint8_t, 4, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint8_t, 4, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint8_t, 8, 4>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint8_t, 8, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint8_t, 8, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint8_t, 8, 32>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template<> void cfl_predict_nv12_avx2<uint8_t, 16, 4>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd) {
        cfl_predict_nv12_u8_16_avx2_impl(pred_buf_q3, (uint8_t*)dst, dst_stride, (uint8_t)dcU, (uint8_t)dcV, alpha_q3, bd, 16, 4);
    }
    template<> void cfl_predict_nv12_avx2<uint8_t, 16, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd) {
        cfl_predict_nv12_u8_16_avx2_impl(pred_buf_q3, (uint8_t*)dst, dst_stride, (uint8_t)dcU, (uint8_t)dcV, alpha_q3, bd, 16, 8);
    }
    template<> void cfl_predict_nv12_avx2<uint8_t, 16, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd) {
        cfl_predict_nv12_u8_16_avx2_impl(pred_buf_q3, (uint8_t*)dst, dst_stride, (uint8_t)dcU, (uint8_t)dcV, alpha_q3, bd, 16, 16);
    }
    template<> void cfl_predict_nv12_avx2<uint8_t, 16, 32>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd) {
        cfl_predict_nv12_u8_16_avx2_impl(pred_buf_q3, (uint8_t*)dst, dst_stride, (uint8_t)dcU, (uint8_t)dcV, alpha_q3, bd, 16, 32);
    }
    template void cfl_predict_nv12_avx2<uint8_t, 32, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint8_t, 32, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint8_t, 32, 32>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);

    template void cfl_predict_nv12_avx2<uint16_t, 4, 4>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 4, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 4, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 8, 4>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 8, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 8, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 8, 32>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 16, 4>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 16, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 16, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 16, 32>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 32, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 32, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_avx2<uint16_t, 32, 32>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);

};
