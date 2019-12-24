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
#include <type_traits>

#include "mfx_av1_cfl.h"

static constexpr int32_t BSR(uint32_t u32) {
    return u32 == 1?0:BSR(u32>>1)+1;
}

static inline __m128i fill_sum_epi32(__m128i l0) {
    l0 = _mm_add_epi32(l0, _mm_shuffle_epi32(l0, _MM_SHUFFLE(1, 0, 3, 2)));
    return _mm_add_epi32(l0, _mm_shuffle_epi32(l0, _MM_SHUFFLE(2, 3, 0, 1)));
}

static void subtract_average_sse2_impl(const uint16_t *src_ptr,
    int16_t *dst_ptr, int width,
    int height, int round_offset,
    int num_pel_log2) {
    const __m128i zeros = _mm_setzero_si128();
    const __m128i round_offset_epi32 = _mm_set1_epi32(round_offset);
    const __m128i *src = (__m128i *)src_ptr;
    const __m128i *const end = src + height * CFL_BUF_LINE_I128;
    const int step = CFL_BUF_LINE_I128 * (1 + (width == 8) + 3 * (width == 4));

    __m128i sum = zeros;
    do {
        __m128i l0;
        if (width == 4) {
            l0 = _mm_add_epi16(_mm_loadl_epi64(src),
                _mm_loadl_epi64(src + CFL_BUF_LINE_I128));
            __m128i l1 = _mm_add_epi16(_mm_loadl_epi64(src + 2 * CFL_BUF_LINE_I128),
                _mm_loadl_epi64(src + 3 * CFL_BUF_LINE_I128));
            sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(l0, zeros),
                _mm_unpacklo_epi16(l1, zeros)));
        }
        else {
            if (width == 8) {
                l0 = _mm_add_epi16(_mm_loadu_si128(src),
                    _mm_loadu_si128(src + CFL_BUF_LINE_I128));
            }
            else {
                l0 = _mm_add_epi16(_mm_loadu_si128(src), _mm_loadu_si128(src + 1));
            }
            sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(l0, zeros),
                _mm_unpackhi_epi16(l0, zeros)));
            if (width == 32) {
                l0 = _mm_add_epi16(_mm_loadu_si128(src + 2), _mm_loadu_si128(src + 3));
                sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(l0, zeros),
                    _mm_unpackhi_epi16(l0, zeros)));
            }
        }
        src += step;
    } while (src < end);

    sum = fill_sum_epi32(sum);

    __m128i avg_epi16 =
        _mm_srli_epi32(_mm_add_epi32(sum, round_offset_epi32), num_pel_log2);
    avg_epi16 = _mm_packs_epi32(avg_epi16, avg_epi16);

    src = (__m128i *)src_ptr;
    __m128i *dst = (__m128i *)dst_ptr;
    do {
        if (width == 4) {
            _mm_storel_epi64(dst, _mm_sub_epi16(_mm_loadl_epi64(src), avg_epi16));
        }
        else {
            _mm_storeu_si128(dst, _mm_sub_epi16(_mm_loadu_si128(src), avg_epi16));
            if (width > 8) {
                _mm_storeu_si128(dst + 1,
                    _mm_sub_epi16(_mm_loadu_si128(src + 1), avg_epi16));
                if (width == 32) {
                    _mm_storeu_si128(dst + 2,
                        _mm_sub_epi16(_mm_loadu_si128(src + 2), avg_epi16));
                    _mm_storeu_si128(dst + 3,
                        _mm_sub_epi16(_mm_loadu_si128(src + 3), avg_epi16));
                }
            }
        }
        src += CFL_BUF_LINE_I128;
        dst += CFL_BUF_LINE_I128;
    } while (src < end);
}

namespace AV1PP {
    template<int width, int height> void subtract_average_sse2(const uint16_t *src, int16_t *dst) {
        constexpr int sz = width * height;
        constexpr int num_pel_log2 = BSR(sz);
        constexpr int round_offset = ((sz) >> 1);

        subtract_average_sse2_impl(src, dst, width, height, round_offset, num_pel_log2);
    }
    template void subtract_average_sse2< 4, 4>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2< 4, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2< 4, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2< 8, 4>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2< 8, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2< 8, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2< 8, 32>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2<16, 4>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2<16, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2<16, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2<16, 32>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2<32, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2<32, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_sse2<32, 32>(const uint16_t *src, int16_t *dst);
};


// Load 32-bit integer from memory into the first element of dst.
static inline __m128i _mm_loadh_epi32(__m128i const *mem_addr) {
    return _mm_cvtsi32_si128(*((int *)mem_addr));
}

// Store 32-bit integer from the first element of a into memory.
static inline void _mm_storeh_epi32(__m128i const *mem_addr, __m128i a) {
    *((int *)mem_addr) = _mm_cvtsi128_si32(a);
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
 */
static void cfl_luma_subsampling_420_u8_ssse3_impl(const uint8_t *input,
    int input_stride,
    uint16_t *pred_buf_q3,
    int width, int height) {
    const __m128i twos = _mm_set1_epi8(2);
    __m128i *pred_buf_m128i = (__m128i *)pred_buf_q3;
    const __m128i *end = pred_buf_m128i + (height >> 1) * CFL_BUF_LINE_I128;
    const int luma_stride = input_stride << 1;
    do {
        if (width == 4) {
            __m128i top = _mm_loadh_epi32((__m128i *)input);
            top = _mm_maddubs_epi16(top, twos);
            __m128i bot = _mm_loadh_epi32((__m128i *)(input + input_stride));
            bot = _mm_maddubs_epi16(bot, twos);
            const __m128i sum = _mm_add_epi16(top, bot);
            _mm_storeh_epi32(pred_buf_m128i, sum);
        }
        else if (width == 8) {
            __m128i top = _mm_loadl_epi64((__m128i *)input);
            top = _mm_maddubs_epi16(top, twos);
            __m128i bot = _mm_loadl_epi64((__m128i *)(input + input_stride));
            bot = _mm_maddubs_epi16(bot, twos);
            const __m128i sum = _mm_add_epi16(top, bot);
            _mm_storel_epi64(pred_buf_m128i, sum);
        }
        else {
            __m128i top = _mm_loadu_si128((__m128i *)input);
            top = _mm_maddubs_epi16(top, twos);
            __m128i bot = _mm_loadu_si128((__m128i *)(input + input_stride));
            bot = _mm_maddubs_epi16(bot, twos);
            const __m128i sum = _mm_add_epi16(top, bot);
            _mm_storeu_si128(pred_buf_m128i, sum);
            if (width == 32) {
                __m128i top_1 = _mm_loadu_si128(((__m128i *)input) + 1);
                __m128i bot_1 =
                    _mm_loadu_si128(((__m128i *)(input + input_stride)) + 1);
                top_1 = _mm_maddubs_epi16(top_1, twos);
                bot_1 = _mm_maddubs_epi16(bot_1, twos);
                __m128i sum_1 = _mm_add_epi16(top_1, bot_1);
                _mm_storeu_si128(pred_buf_m128i + 1, sum_1);
            }
        }
        input += luma_stride;
        pred_buf_m128i += CFL_BUF_LINE_I128;
    } while (pred_buf_m128i < end);
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
static void cfl_luma_subsampling_422_u8_ssse3_impl(const uint8_t *input,
    int input_stride,
    uint16_t *pred_buf_q3,
    int width, int height) {
    const __m128i fours = _mm_set1_epi8(4);
    __m128i *pred_buf_m128i = (__m128i *)pred_buf_q3;
    const __m128i *end = pred_buf_m128i + height * CFL_BUF_LINE_I128;
    do {
        if (width == 4) {
            __m128i top = _mm_loadh_epi32((__m128i *)input);
            top = _mm_maddubs_epi16(top, fours);
            _mm_storeh_epi32(pred_buf_m128i, top);
        }
        else if (width == 8) {
            __m128i top = _mm_loadl_epi64((__m128i *)input);
            top = _mm_maddubs_epi16(top, fours);
            _mm_storel_epi64(pred_buf_m128i, top);
        }
        else {
            __m128i top = _mm_loadu_si128((__m128i *)input);
            top = _mm_maddubs_epi16(top, fours);
            _mm_storeu_si128(pred_buf_m128i, top);
            if (width == 32) {
                __m128i top_1 = _mm_loadu_si128(((__m128i *)input) + 1);
                top_1 = _mm_maddubs_epi16(top_1, fours);
                _mm_storeu_si128(pred_buf_m128i + 1, top_1);
            }
        }
        input += input_stride;
        pred_buf_m128i += CFL_BUF_LINE_I128;
    } while (pred_buf_m128i < end);
}

/**
 * Multiplies the pixels by 8 (scaling in Q3).
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
static void cfl_luma_subsampling_444_u8_ssse3_impl(const uint8_t *input,
    int input_stride,
    uint16_t *pred_buf_q3,
    int width, int height) {
    const __m128i zeros = _mm_setzero_si128();
    const int luma_stride = input_stride;
    __m128i *pred_buf_m128i = (__m128i *)pred_buf_q3;
    const __m128i *end = pred_buf_m128i + height * CFL_BUF_LINE_I128;
    do {
        if (width == 4) {
            __m128i row = _mm_loadh_epi32((__m128i *)input);
            row = _mm_unpacklo_epi8(row, zeros);
            _mm_storel_epi64(pred_buf_m128i, _mm_slli_epi16(row, 3));
        }
        else if (width == 8) {
            __m128i row = _mm_loadl_epi64((__m128i *)input);
            row = _mm_unpacklo_epi8(row, zeros);
            _mm_storeu_si128(pred_buf_m128i, _mm_slli_epi16(row, 3));
        }
        else {
            __m128i row = _mm_loadu_si128((__m128i *)input);
            const __m128i row_lo = _mm_unpacklo_epi8(row, zeros);
            const __m128i row_hi = _mm_unpackhi_epi8(row, zeros);
            _mm_storeu_si128(pred_buf_m128i, _mm_slli_epi16(row_lo, 3));
            _mm_storeu_si128(pred_buf_m128i + 1, _mm_slli_epi16(row_hi, 3));
            if (width == 32) {
                __m128i row_1 = _mm_loadu_si128(((__m128i *)input) + 1);
                const __m128i row_1_lo = _mm_unpacklo_epi8(row_1, zeros);
                const __m128i row_1_hi = _mm_unpackhi_epi8(row_1, zeros);
                _mm_storeu_si128(pred_buf_m128i + 2, _mm_slli_epi16(row_1_lo, 3));
                _mm_storeu_si128(pred_buf_m128i + 3, _mm_slli_epi16(row_1_hi, 3));
            }
        }
        input += luma_stride;
        pred_buf_m128i += CFL_BUF_LINE_I128;
    } while (pred_buf_m128i < end);
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
 */
static void cfl_luma_subsampling_420_u16_ssse3_impl(const uint16_t *input,
    int input_stride,
    uint16_t *pred_buf_q3,
    int width, int height) {
    const uint16_t *end = pred_buf_q3 + (height >> 1) * CFL_BUF_LINE;
    const int luma_stride = input_stride << 1;
    do {
        if (width == 4) {
            const __m128i top = _mm_loadl_epi64((__m128i *)input);
            const __m128i bot = _mm_loadl_epi64((__m128i *)(input + input_stride));
            __m128i sum = _mm_add_epi16(top, bot);
            sum = _mm_hadd_epi16(sum, sum);
            *((int *)pred_buf_q3) = _mm_cvtsi128_si32(_mm_add_epi16(sum, sum));
        }
        else {
            const __m128i top = _mm_loadu_si128((__m128i *)input);
            const __m128i bot = _mm_loadu_si128((__m128i *)(input + input_stride));
            __m128i sum = _mm_add_epi16(top, bot);
            if (width == 8) {
                sum = _mm_hadd_epi16(sum, sum);
                _mm_storel_epi64((__m128i *)pred_buf_q3, _mm_add_epi16(sum, sum));
            }
            else {
                const __m128i top_1 = _mm_loadu_si128(((__m128i *)input) + 1);
                const __m128i bot_1 =
                    _mm_loadu_si128(((__m128i *)(input + input_stride)) + 1);
                sum = _mm_hadd_epi16(sum, _mm_add_epi16(top_1, bot_1));
                _mm_storeu_si128((__m128i *)pred_buf_q3, _mm_add_epi16(sum, sum));
                if (width == 32) {
                    const __m128i top_2 = _mm_loadu_si128(((__m128i *)input) + 2);
                    const __m128i bot_2 =
                        _mm_loadu_si128(((__m128i *)(input + input_stride)) + 2);
                    const __m128i top_3 = _mm_loadu_si128(((__m128i *)input) + 3);
                    const __m128i bot_3 =
                        _mm_loadu_si128(((__m128i *)(input + input_stride)) + 3);
                    const __m128i sum_2 = _mm_add_epi16(top_2, bot_2);
                    const __m128i sum_3 = _mm_add_epi16(top_3, bot_3);
                    __m128i next_sum = _mm_hadd_epi16(sum_2, sum_3);
                    _mm_storeu_si128(((__m128i *)pred_buf_q3) + 1,
                        _mm_add_epi16(next_sum, next_sum));
                }
            }
        }
        input += luma_stride;
    } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
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
static void cfl_luma_subsampling_422_u16_ssse3_impl(const uint16_t *input,
    int input_stride,
    uint16_t *pred_buf_q3,
    int width, int height) {
    __m128i *pred_buf_m128i = (__m128i *)pred_buf_q3;
    const __m128i *end = pred_buf_m128i + height * CFL_BUF_LINE_I128;
    do {
        if (width == 4) {
            const __m128i top = _mm_loadl_epi64((__m128i *)input);
            const __m128i sum = _mm_slli_epi16(_mm_hadd_epi16(top, top), 2);
            _mm_storeh_epi32(pred_buf_m128i, sum);
        }
        else {
            const __m128i top = _mm_loadu_si128((__m128i *)input);
            if (width == 8) {
                const __m128i sum = _mm_slli_epi16(_mm_hadd_epi16(top, top), 2);
                _mm_storel_epi64(pred_buf_m128i, sum);
            }
            else {
                const __m128i top_1 = _mm_loadu_si128(((__m128i *)input) + 1);
                const __m128i sum = _mm_slli_epi16(_mm_hadd_epi16(top, top_1), 2);
                _mm_storeu_si128(pred_buf_m128i, sum);
                if (width == 32) {
                    const __m128i top_2 = _mm_loadu_si128(((__m128i *)input) + 2);
                    const __m128i top_3 = _mm_loadu_si128(((__m128i *)input) + 3);
                    const __m128i sum_1 = _mm_slli_epi16(_mm_hadd_epi16(top_2, top_3), 2);
                    _mm_storeu_si128(pred_buf_m128i + 1, sum_1);
                }
            }
        }
        pred_buf_m128i += CFL_BUF_LINE_I128;
        input += input_stride;
    } while (pred_buf_m128i < end);
}

static void cfl_luma_subsampling_444_u16_ssse3_impl(const uint16_t *input,
    int input_stride,
    uint16_t *pred_buf_q3,
    int width, int height) {
    const uint16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
    do {
        if (width == 4) {
            const __m128i row = _mm_slli_epi16(_mm_loadl_epi64((__m128i *)input), 3);
            _mm_storel_epi64((__m128i *)pred_buf_q3, row);
        }
        else {
            const __m128i row = _mm_slli_epi16(_mm_loadu_si128((__m128i *)input), 3);
            _mm_storeu_si128((__m128i *)pred_buf_q3, row);
            if (width >= 16) {
                __m128i row_1 = _mm_loadu_si128(((__m128i *)input) + 1);
                row_1 = _mm_slli_epi16(row_1, 3);
                _mm_storeu_si128(((__m128i *)pred_buf_q3) + 1, row_1);
                if (width == 32) {
                    __m128i row_2 = _mm_loadu_si128(((__m128i *)input) + 2);
                    row_2 = _mm_slli_epi16(row_2, 3);
                    _mm_storeu_si128(((__m128i *)pred_buf_q3) + 2, row_2);
                    __m128i row_3 = _mm_loadu_si128(((__m128i *)input) + 3);
                    row_3 = _mm_slli_epi16(row_3, 3);
                    _mm_storeu_si128(((__m128i *)pred_buf_q3) + 3, row_3);
                }
            }
        }
        input += input_stride;
        pred_buf_q3 += CFL_BUF_LINE;
    } while (pred_buf_q3 < end);
}

namespace AV1PP {
    template<int width, int height> void cfl_luma_subsampling_420_u8_ssse3(const uint8_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_420_u8_ssse3_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_420_u8_ssse3< 4, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3< 4, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3< 4, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3< 8, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3< 8, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3< 8, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3< 8, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3<16, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3<16, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3<16, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3<16, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3<32, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3<32, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_ssse3<32, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_420_u16_ssse3(const uint16_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_420_u16_ssse3_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_420_u16_ssse3< 4, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3< 4, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3< 4, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3< 8, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3< 8, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3< 8, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3< 8, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3<16, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3<16, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3<16, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3<16, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3<32, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3<32, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_ssse3<32, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_422_u8_ssse3(const uint8_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_422_u8_ssse3_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_422_u8_ssse3< 4, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3< 4, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3< 4, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3< 8, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3< 8, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3< 8, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3< 8, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3<16, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3<16, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3<16, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3<16, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3<32, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3<32, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u8_ssse3<32, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_422_u16_ssse3(const uint16_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_422_u16_ssse3_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_422_u16_ssse3< 4, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3< 4, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3< 4, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3< 8, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3< 8, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3< 8, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3< 8, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3<16, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3<16, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3<16, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3<16, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3<32, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3<32, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_422_u16_ssse3<32, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_444_u8_ssse3(const uint8_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_444_u8_ssse3_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_444_u8_ssse3< 4, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3< 4, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3< 4, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3< 8, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3< 8, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3< 8, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3< 8, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3<16, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3<16, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3<16, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3<16, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3<32, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3<32, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u8_ssse3<32, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_444_u16_ssse3(const uint16_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_444_u16_ssse3_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_444_u16_ssse3< 4, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3< 4, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3< 4, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3< 8, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3< 8, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3< 8, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3< 8, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3<16, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3<16, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3<16, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3<16, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3<32, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3<32, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_444_u16_ssse3<32, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);

};

static inline __m128i predict_unclipped(const __m128i *input, __m128i alpha_q12,  __m128i alpha_sign, __m128i dc_q0) {
    __m128i ac_q3 = _mm_loadu_si128(input);
    __m128i ac_sign = _mm_sign_epi16(alpha_sign, ac_q3);
    __m128i scaled_luma_q0 = _mm_mulhrs_epi16(_mm_abs_epi16(ac_q3), alpha_q12);
    scaled_luma_q0 = _mm_sign_epi16(scaled_luma_q0, ac_sign);
    return scaled_luma_q0;
    //return _mm_add_epi16(scaled_luma_q0, dc_q0);
}

static  void cfl_predict_nv12_u8_ssse3_impl(const int16_t *pred_buf_q3, uint8_t *dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd, int width, int height)
{
    bd; //ignore bd
    const __m128i alpha_sign = _mm_set1_epi16(alpha_q3);
    const __m128i alpha_q12 = _mm_slli_epi16(_mm_abs_epi16(alpha_sign), 9);
    const __m128i dc_q0 = _mm_set1_epi32(dcU | (dcV<<16));
    __m128i *row = (__m128i *)pred_buf_q3;
    const __m128i *row_end = row + height * CFL_BUF_LINE_I128;
    do {
        __m128i res = predict_unclipped(row, alpha_q12, alpha_sign, dc_q0);
        __m128i v1 = _mm_unpacklo_epi16(res, res);
        v1 = _mm_add_epi16(v1, dc_q0);
        __m128i v2 = _mm_unpackhi_epi16(res, res);
        v2 = _mm_add_epi16(v2, dc_q0);
        res = _mm_packus_epi16(v1, v2);
        if (width < 16) {
            if (width == 4)
                _mm_storel_epi64((__m128i *)dst, res);
            else
                _mm_storeu_si128((__m128i *)dst, res);
        }
        else {
            __m128i next = predict_unclipped(row + 1, alpha_q12, alpha_sign, dc_q0);
            __m128i next1 = _mm_unpacklo_epi16(next, next);
            next1 = _mm_add_epi16(next1, dc_q0);
            __m128i next2 = _mm_unpackhi_epi16(next, next);
            next2 = _mm_add_epi16(next2, dc_q0);

            next = _mm_packus_epi16(next1, next2);
            _mm_storeu_si128((__m128i *)dst, res);
            _mm_storeu_si128((__m128i *)(dst+16), next);
            if (width == 32) {
                res = predict_unclipped(row + 2, alpha_q12, alpha_sign, dc_q0);
                __m128i v1 = _mm_unpacklo_epi16(res, res);
                v1 = _mm_add_epi16(v1, dc_q0);
                __m128i v2 = _mm_unpackhi_epi16(res, res);
                v2 = _mm_add_epi16(v2, dc_q0);
                res = _mm_packus_epi16(v1, v2);

                next = predict_unclipped(row + 3, alpha_q12, alpha_sign, dc_q0);
                __m128i next1 = _mm_unpacklo_epi16(next, next);
                next1 = _mm_add_epi16(next1, dc_q0);
                __m128i next2 = _mm_unpackhi_epi16(next, next);
                next2 = _mm_add_epi16(next2, dc_q0);

                res = _mm_packus_epi16(next1, next2);

                _mm_storeu_si128((__m128i *)(dst + 32), res);
                _mm_storeu_si128((__m128i *)(dst + 48), next);
            }
        }
        dst += dst_stride;
    } while ((row += CFL_BUF_LINE_I128) < row_end);
}

static  void cfl_predict_nv12_u8_4_ssse3_impl(const int16_t *pred_buf_q3, uint8_t *dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd, int width, int height)
{
    bd; //ignore bd
    const __m128i alpha_sign = _mm_set1_epi16(alpha_q3);
    const __m128i alpha_q12 = _mm_slli_epi16(_mm_abs_epi16(alpha_sign), 9);
    const __m128i dc_q0 = _mm_set1_epi32(dcU | (dcV << 16));
    __m128i *row = (__m128i *)pred_buf_q3;
    const __m128i *row_end = row + height * CFL_BUF_LINE_I128;
    do {
        __m128i res = predict_unclipped(row, alpha_q12, alpha_sign, dc_q0);
        __m128i v1 = _mm_unpacklo_epi16(res, res);
        v1 = _mm_add_epi16(v1, dc_q0);
        __m128i v2 = _mm_unpackhi_epi16(res, res);
        v2 = _mm_add_epi16(v2, dc_q0);
        res = _mm_packus_epi16(v1, v2);
        _mm_storel_epi64((__m128i *)dst, res);
        dst += dst_stride;
    } while ((row += CFL_BUF_LINE_I128) < row_end);
}

static  void cfl_predict_nv12_u8_8_ssse3_impl(const int16_t *pred_buf_q3, uint8_t *dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd, int width, int height)
{
    bd; //ignore bd
    const __m128i alpha_sign = _mm_set1_epi16(alpha_q3);
    const __m128i alpha_q12 = _mm_slli_epi16(_mm_abs_epi16(alpha_sign), 9);
    const __m128i dc_q0 = _mm_set1_epi32(dcU | (dcV << 16));
    __m128i *row = (__m128i *)pred_buf_q3;
    const __m128i *row_end = row + height * CFL_BUF_LINE_I128;
    do {
        __m128i res = predict_unclipped(row, alpha_q12, alpha_sign, dc_q0);
        __m128i v1 = _mm_unpacklo_epi16(res, res);
        v1 = _mm_add_epi16(v1, dc_q0);
        __m128i v2 = _mm_unpackhi_epi16(res, res);
        v2 = _mm_add_epi16(v2, dc_q0);
        res = _mm_packus_epi16(v1, v2);
        _mm_storeu_si128((__m128i *)dst, res);
        dst += dst_stride;
    } while ((row += CFL_BUF_LINE_I128) < row_end);
}

static  void cfl_predict_nv12_u8_16_ssse3_impl(const int16_t *pred_buf_q3, uint8_t *dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd, int width, int height)
{
    bd; //ignore bd
    const __m128i alpha_sign = _mm_set1_epi16(alpha_q3);
    const __m128i alpha_q12 = _mm_slli_epi16(_mm_abs_epi16(alpha_sign), 9);
    const __m128i dc_q0 = _mm_set1_epi32(dcU | (dcV << 16));
    __m128i *row = (__m128i *)pred_buf_q3;
    const __m128i *row_end = row + height * CFL_BUF_LINE_I128;
    do {
        __m128i res = predict_unclipped(row, alpha_q12, alpha_sign, dc_q0);
        __m128i v1 = _mm_unpacklo_epi16(res, res);
        v1 = _mm_add_epi16(v1, dc_q0);
        __m128i v2 = _mm_unpackhi_epi16(res, res);
        v2 = _mm_add_epi16(v2, dc_q0);
        res = _mm_packus_epi16(v1, v2);

        __m128i next = predict_unclipped(row + 1, alpha_q12, alpha_sign, dc_q0);
        __m128i next1 = _mm_unpacklo_epi16(next, next);
        next1 = _mm_add_epi16(next1, dc_q0);
        __m128i next2 = _mm_unpackhi_epi16(next, next);
        next2 = _mm_add_epi16(next2, dc_q0);

        next = _mm_packus_epi16(next1, next2);
        _mm_storeu_si128((__m128i *)dst, res);
        _mm_storeu_si128((__m128i *)(dst + 16), next);
        dst += dst_stride;
    } while ((row += CFL_BUF_LINE_I128) < row_end);
}

static  void cfl_predict_nv12_u16_ssse3_impl(const int16_t *pred_buf_q3, uint16_t *dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd, int width, int height) {
}

namespace AV1PP {
    template<typename PixType, int width, int height> void cfl_predict_nv12_ssse3(const int16_t *pred_buf_q3, PixType *dst, int dst_stride, PixType dcU, PixType dcV, int alpha_q3, int bd) {
        cfl_predict_nv12_u8_ssse3_impl(pred_buf_q3, (uint8_t*)dst, dst_stride, (uint8_t)dcU, (uint8_t)dcV, alpha_q3, bd, width, height);
    }

    template<> void cfl_predict_nv12_ssse3<uint8_t, 4, 4>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd) {
        cfl_predict_nv12_u8_4_ssse3_impl(pred_buf_q3, (uint8_t*)dst, dst_stride, (uint8_t)dcU, (uint8_t)dcV, alpha_q3, bd, 4, 4);
    }
    template void cfl_predict_nv12_ssse3<uint8_t, 4, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint8_t, 4, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint8_t, 8, 4>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template<> void cfl_predict_nv12_ssse3<uint8_t, 8, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd) {
        cfl_predict_nv12_u8_8_ssse3_impl(pred_buf_q3, (uint8_t*)dst, dst_stride, (uint8_t)dcU, (uint8_t)dcV, alpha_q3, bd, 8, 8);
    }
    template void cfl_predict_nv12_ssse3<uint8_t, 8, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint8_t, 8, 32>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint8_t, 16, 4>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint8_t, 16, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template<> void cfl_predict_nv12_ssse3<uint8_t, 16, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd) {
        cfl_predict_nv12_u8_16_ssse3_impl(pred_buf_q3, (uint8_t*)dst, dst_stride, (uint8_t)dcU, (uint8_t)dcV, alpha_q3, bd, 16, 16);
    }
    template void cfl_predict_nv12_ssse3<uint8_t, 16, 32>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint8_t, 32, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint8_t, 32, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint8_t, 32, 32>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);

    template void cfl_predict_nv12_ssse3<uint16_t, 4, 4>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 4, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 4, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 8, 4>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 8, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 8, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 8, 32>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 16, 4>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 16, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 16, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 16, 32>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 32, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 32, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_ssse3<uint16_t, 32, 32>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);

};
