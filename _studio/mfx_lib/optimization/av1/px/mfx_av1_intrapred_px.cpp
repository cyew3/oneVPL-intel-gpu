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

#include "stdio.h"
#include "assert.h"
#include "string.h"
#include "ipps.h"
#include "mfx_av1_intrapred_common.h"
#include "mfx_av1_get_intra_pred_pels.h"

namespace AV1PP
{
    #define Saturate(min_val, max_val, val) std::max((min_val), std::min((max_val), (val)))
    /* Shift down with rounding */
    #define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n) - 1))) >> (n))

    #define AVG3(a, b, c) (((a) + 2 * (b) + (c) + 2) >> 2)
    #define AVG2(a, b) (((a) + (b) + 1) >> 1)
    #define DST(x, y) dst[(x) + (y)*pitch]
    #define DST0(x, y) dst[2*(x) + (y)*pitch]
    #define DST1(x, y) dst[2*(x) + 1 + (y)*pitch]

    namespace details {
        typedef int16_t tran_low_t;

        static inline uint8_t clip_pixel(int val) {
            return (val > 255) ? 255 : (val < 0) ? 0 : val;
        }
        static inline uint16_t clip_pixel_10bit(int val) {
            return (val > 1023) ? 1023 : (val < 0) ? 0 : val;
        }

        static const int8_t h265_log2m2[257] = {
            -1,  -1,  -1,  -1,   0,  -1,  -1,  -1,   1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            2,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            3,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            4,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            5,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
            6,
        };
    }; // namespace details

    template <int txSize, int leftAvail, int topAvail, typename PixType> void predict_intra_dc_av1_px(const PixType *topPel, const PixType *leftPel, PixType *dst, int pitch, int, int, int)
    {
        const int width = 4 << txSize;
        const int log2Width = txSize + 2;
        const PixType *above = topPel;
        const PixType *left  = leftPel;
        const int offset = sizeof(PixType) == 1 ? 0 : 2;

        int sum = 0;

        if (leftAvail && topAvail) {
            for (int i = 0; i < width; i++)
                sum += above[i] + left[i];
            sum = (sum + width) >> (log2Width + 1);
        } else if (leftAvail) {
            for (int i = 0; i < width; i++)
                sum += left[i];
            sum = (sum + (1 << (log2Width - 1))) >> log2Width;
        } else if (topAvail) {
            for (int i = 0; i < width; i++)
                sum += above[i];
            sum = (sum + (1 << (log2Width - 1))) >> log2Width;
        } else {
            sum = 128 << offset;
        }

        for (int r = 0; r < width; r++) {
            //memset(dst, sum, width);
            for (int c = 0; c < width; c++)
                dst[c] = sum;
            dst += pitch;
        }
    }
    template void predict_intra_dc_av1_px<0, 0, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<0, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<0, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<0, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<1, 0, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<1, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<1, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<1, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<2, 0, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<2, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<2, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<2, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<3, 0, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<3, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<3, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<3, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    template void predict_intra_dc_av1_px<0, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<0, 0, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<0, 1, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<0, 1, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<1, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<1, 0, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<1, 1, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<1, 1, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<2, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<2, 0, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<2, 1, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<2, 1, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<3, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<3, 0, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<3, 1, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_dc_av1_px<3, 1, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    namespace details {
        template <int txSize, typename PixType> void predict_intra_hor_av1_px(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch)
        {
            topPels;
            const int width = 4 << txSize;
            for (int r = 0; r < width; r++, dst += pitch)
                //memset(dst, leftPels[r], width*sizeof(PixType));
                for (int c = 0; c < width; c++)
                    dst[c] = leftPels[r];
        }

        template <int txSize, typename PixType> void predict_intra_ver_av1_px(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch)
        {
            leftPels;
            const int width = 4 << txSize;
            for (int r = 0; r < width; r++, dst += pitch)
                //memcpy(dst, topPels, width);
                for (int c = 0; c < width; c++)
                    dst[c] = topPels[c];
        }

        template <int txSize, typename PixType> void predict_intra_z1_av1_px(const PixType *topPels, PixType *dst, int pitch, int upTop, int dx)
        {
            const int width = 4 << txSize;
            const PixType *above = topPels;
            int r, c, x, base, shift, val;

            //const int dx = 256;
            //const int dy = 1;
            const int bs = width;
            const int stride = pitch;
            const int max_base_x = (2 * bs - 1) << upTop;
            const int frac_bits = 6 - upTop;
            const int base_inc = 1 << upTop;

            //(void)left;
            assert(dx > 0);

            x = dx;
            for (r = 0; r < bs; ++r, dst += stride, x += dx) {
                base = x >> frac_bits;
                shift = ((x << upTop) & 0x3F) >> 1;

                if (base >= max_base_x) {
                    int i;
                    for (i = r; i < bs; ++i) {
                        memset(dst, above[max_base_x], bs * sizeof(dst[0]));
                        dst += stride;
                    }
                    return;
                }

                for (c = 0; c < bs; ++c, base += base_inc) {
                    if (base < max_base_x) {
                        val = above[base] * (32 - shift) + above[base + 1] * shift;
                        val = ROUND_POWER_OF_TWO(val, 5);
                        if (sizeof(PixType) == 1)
                            dst[c] = clip_pixel(val);
                        else
                            dst[c] = clip_pixel_10bit(val);
                    }
                    else {
                        dst[c] = above[max_base_x];
                    }
                }
            }
        }

        template <int txSize, typename PixType> void predict_intra_z2_av1_px(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch, int upTop, int upLeft, int dx, int dy)
        {
            const int width = 4 << txSize;
            const PixType* above = topPels;
            const PixType* left  = leftPels;

            int stride = pitch;
            int bs = width;

            int r, c, x, y, shift1, shift2, val, base1, base2;

            assert(dx > 0);
            assert(dy > 0);

            const int min_base_x = -(1 << upTop);
            const int frac_bits_x = 6 - upTop;
            const int frac_bits_y = 6 - upLeft;
            const int base_inc_x = 1 << upTop;
            x = -dx;
            for (r = 0; r < bs; ++r, x -= dx, dst += stride) {
                base1 = x >> frac_bits_x;
                y = (r << 6) - dy;
                for (c = 0; c < bs; ++c, base1 += base_inc_x, y -= dy) {
                    if (base1 >= min_base_x) {
                        shift1 = ((x << upTop) & 0x3F) >> 1;
                        val = above[base1] * (32 - shift1) + above[base1 + 1] * shift1;
                        val = ROUND_POWER_OF_TWO(val, 5);
                    }
                    else {
                        base2 = y >> frac_bits_y;
                        shift2 = ((y << upLeft) & 0x3F) >> 1;
                        val = left[base2] * (32 - shift2) + left[base2 + 1] * shift2;
                        val = ROUND_POWER_OF_TWO(val, 5);
                    }
                    if (sizeof(PixType) == 1)
                        dst[c] = clip_pixel(val);
                    else
                        dst[c] = clip_pixel_10bit(val);
                }
            }
        }

        template <int txSize, typename PixType> void predict_intra_z3_av1_px(const PixType *leftPels, PixType *dst, int pitch, int upLeft, int dy)
        {
            const int width = 4 << txSize;
            const PixType* left = leftPels;
            const int bs = width;
            const int stride = pitch;
            const int max_base_y = (2 * bs - 1) << upLeft;
            const int frac_bits = 6 - upLeft;
            const int base_inc = 1 << upLeft;

            int r, c, y, base, shift, val;

            assert(dy > 0);

            y = dy;
            for (c = 0; c < bs; ++c, y += dy) {
                base = y >> frac_bits;
                assert(base >= 0);
                shift = ((y << upLeft) & 0x3F) >> 1;

                for (r = 0; r < bs; ++r, base += base_inc) {
                    if (base < max_base_y) {
                        val = left[base] * (32 - shift) + left[base + 1] * shift;
                        val = ROUND_POWER_OF_TWO(val, 5);
                        if (sizeof(PixType) == 1)
                            dst[r * stride + c] = clip_pixel(val);
                        else
                            dst[r * stride + c] = clip_pixel_10bit(val);
                    } else {
                        for (; r < bs; ++r) dst[r * stride + c] = left[max_base_y];
                        break;
                    }
                }
            }
        }

        // Weights are quadratic from '1' to '1 / block_size', scaled by
        // 2^sm_weight_log2_scale.
        const int sm_weight_log2_scale = 8;
        const int MAX_BLOCK_DIM = 32;
        const int NUM_BLOCK_DIMS = 5;
        const uint8_t sm_weight_arrays[NUM_BLOCK_DIMS][MAX_BLOCK_DIM] = {
            // bs = 2
            { 255, 128 },
            // bs = 4
            { 255, 149, 85, 64 },
            // bs = 8
            { 255, 197, 146, 105, 73, 50, 37, 32 },
            // bs = 16
            { 255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16 },
            // bs = 32
            { 255, 240, 225, 210, 196, 182, 169, 157, 145, 133, 122, 111, 101, 92,  83,  74,
               66,  59,  52,  45,  39,  34,  29,  25,  21,  17,  14,  12,  10,  9,   8,   8 },
        };


        template <int txSize, typename PixType> void predict_intra_smooth_av1_px(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch)
        {
            const int width = 4 << txSize;
            const PixType* above = topPels;
            const PixType* left  = leftPels;
            const PixType below_pred = left[width - 1];   // estimated by bottom-left pixel
            const PixType right_pred = above[width - 1];  // estimated by top-right pixel
            const int arr_index = txSize + 1;
            assert(arr_index >= 0);
            assert(arr_index < details::NUM_BLOCK_DIMS);
            const uint8_t *const sm_weights = details::sm_weight_arrays[arr_index];
            // scale = 2 * 2^sm_weight_log2_scale
            const int log2_scale = 1 + details::sm_weight_log2_scale;
            const uint16_t scale = (1 << details::sm_weight_log2_scale);
            //const int offset = sizeof(PixType) == 1 ? 8 : 10;
            //const int maxVal = (1 << offset) - 1;
            for (int r = 0; r < width; ++r) {
                for (int c = 0; c < width; ++c) {
                    const PixType pixels[] = { above[c], below_pred, left[r], right_pred };
                    const uint8_t weights[] = { sm_weights[r], (uint8_t)(scale - sm_weights[r]), sm_weights[c], (uint8_t)(scale - sm_weights[c]) };
                    uint32_t this_pred = 0;
                    assert(scale >= sm_weights[r] && scale >= sm_weights[c]);
                    for (int i = 0; i < 4; ++i)
                        this_pred += weights[i] * pixels[i];
                    if (sizeof(PixType) == 1)
                        dst[c] = details::clip_pixel(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                    else
                        dst[c] = details::clip_pixel_10bit(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                }
                dst += pitch;
            }
        }

        // Some basic checks on weights for smooth predictor.
#define sm_weights_sanity_checks(weights_w, weights_h, weights_scale, \
    pred_scale)                          \
    assert(weights_w[0] < weights_scale);                               \
    assert(weights_h[0] < weights_scale);                               \
    assert(weights_scale - weights_w[bw - 1] < weights_scale);          \
    assert(weights_scale - weights_h[bh - 1] < weights_scale);          \
    assert(pred_scale < 31)  // ensures no overflow when calculating predictor.

        static const uint8_t sm_weight_vector[2 * MAX_BLOCK_DIM] = {
            // Unused, because we always offset by bs, which is at least 2.
            0, 0,
            // bs = 2
            255, 128,
            // bs = 4
            255, 149, 85, 64,
            // bs = 8
            255, 197, 146, 105, 73, 50, 37, 32,
            // bs = 16
            255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16,
            // bs = 32
            255, 240, 225, 210, 196, 182, 169, 157, 145, 133, 122, 111, 101, 92, 83, 74,
            66, 59, 52, 45, 39, 34, 29, 25, 21, 17, 14, 12, 10, 9, 8, 8,
        };

        template <int txSize, typename PixType> void predict_intra_smooth_v_av1_px(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch)
        {
            const int width = 4 << txSize;
            const PixType *above = topPels;
            const PixType *left  = leftPels;
            int bw = width;
            int bh = width;
            const PixType below_pred = left[bh - 1];  // estimated by bottom-left pixel
            const uint8_t *const sm_weights = sm_weight_vector + bh;
            // scale = 2^sm_weight_log2_scale
            const int log2_scale = sm_weight_log2_scale;
            const uint16_t scale = (1 << sm_weight_log2_scale);
            sm_weights_sanity_checks(sm_weights, sm_weights, scale, log2_scale + sizeof(*dst));

            int r;
            for (r = 0; r < bh; r++) {
                int c;
                for (c = 0; c < bw; ++c) {
                    const PixType pixels[] = { above[c], below_pred };
                    const uint8_t weights[] = { sm_weights[r], (uint8_t)(scale - sm_weights[r]) };
                    uint32_t this_pred = 0;
                    assert(scale >= sm_weights[r]);
                    int i;
                    for (i = 0; i < 2; ++i) {
                        this_pred += weights[i] * pixels[i];
                    }
                    //dst[c] = details::clip_pixel(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                    if (sizeof(PixType) == 1)
                        dst[c] = details::clip_pixel(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                    else
                        dst[c] = details::clip_pixel_10bit(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                }
                dst += pitch;
            }
        }

        template <int txSize, typename PixType> void predict_intra_smooth_h_av1_px(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch)
        {
            const int width = 4 << txSize;
            const PixType *above = topPels;
            const PixType *left  = leftPels;
            int bw = width;
            int bh = width;
            const PixType right_pred = above[bw - 1];  // estimated by top-right pixel
            const uint8_t *const sm_weights = sm_weight_vector + bw;
            // scale = 2^sm_weight_log2_scale
            const int log2_scale = sm_weight_log2_scale;
            const uint16_t scale = (1 << sm_weight_log2_scale);
            sm_weights_sanity_checks(sm_weights, sm_weights, scale, log2_scale + sizeof(*dst));

            int r;
            for (r = 0; r < bh; r++) {
                int c;
                for (c = 0; c < bw; ++c) {
                    const PixType pixels[] = { left[r], right_pred };
                    const uint8_t weights[] = { sm_weights[c], (uint8_t)(scale - sm_weights[c]) };
                    uint32_t this_pred = 0;
                    assert(scale >= sm_weights[c]);
                    int i;
                    for (i = 0; i < 2; ++i) {
                        this_pred += weights[i] * pixels[i];
                    }
                    //dst[c] = details::clip_pixel(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                    if (sizeof(PixType) == 1)
                        dst[c] = details::clip_pixel(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                    else
                        dst[c] = details::clip_pixel_10bit(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                }
                dst += pitch;
            }
        }

        inline int abs_diff(int a, int b) { return (a > b) ? a - b : b - a; }

        int predict_intra_paeth_single_pel(int left, int above, int above_left) {
            const int base = above + left - above_left;
            const int diff_left = abs_diff(base, left);
            const int diff_above = abs_diff(base, above);
            const int diff_above_left = abs_diff(base, above_left);
            // Return nearest to base of left, top and top_left.
            return (diff_left <= diff_above && diff_left <= diff_above_left)
                ? left
                : (diff_above <= diff_above_left)
                    ? above
                    : above_left;
        }

        template <int txSize, typename PixType> void predict_intra_paeth_av1_px(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch)
        {
            const int width = 4 << txSize;
            const PixType *above = topPels;
            const PixType *left  = leftPels;
            const int above_left = topPels[-1];

            for (int r = 0; r < width; r++, dst += pitch)
                for (int c = 0; c < width; c++)
                    dst[c] = details::predict_intra_paeth_single_pel(left[r], above[c], above_left);
        }

        template <typename PixType> void predict_intra_nv12_dc_av1_px(const PixType* topPels, const PixType* leftPels, PixType* dst, int pitch, int width,
                                          int leftAvailable, int topAvailable)
        {
            int log2Width = h265_log2m2[width] + 2;
            // uv - interleaved
            const int width2 = width << 1;
            const PixType* above = topPels;
            const PixType* left  = leftPels;

            int sum[2] = {0};

            int i;
            const int offset = sizeof(PixType) == 1 ? 0 : 2;
            PixType expected_dc[2] = {128 << offset, 128 << offset};

            if (leftAvailable && topAvailable) {
                for (i = 0; i < width2; i += 2) {
                    sum[0]   += above[i] + left[i];
                    sum[0+1] += above[i+1] + left[i+1];
                }
                expected_dc[0] = (sum[0] + width) >> (log2Width + 1);
                expected_dc[1] = (sum[1] + width) >> (log2Width + 1);
            } else if (leftAvailable) {
                for (i = 0; i < width2; i += 2) {
                    sum[0]   += left[i];
                    sum[0+1] += left[i+1];
                }
                expected_dc[0] = (sum[0] + (1 << (log2Width - 1))) >> log2Width;
                expected_dc[1] = (sum[1] + (1 << (log2Width - 1))) >> log2Width;
            } else if (topAvailable) {
                for (i = 0; i < width2; i += 2) {
                    sum[0] += above[i];
                    sum[1] += above[i+1];
                }
                expected_dc[0] = (sum[0] + (1 << (log2Width - 1))) >> log2Width;
                expected_dc[1] = (sum[1] + (1 << (log2Width - 1))) >> log2Width;
            }

            for (int r = 0; r < width; r++) {
				//memset((uint16_t*)dst, *((uint16_t*)expected_dc), width);
                //ippsSet_16s(*((uint16_t*)expected_dc), (int16_t*)dst, width);
                for (int c = 0; c < width; c++) {
                    dst[2 * c + 0] = expected_dc[0];
                    dst[2 * c + 1] = expected_dc[1];
                }
                dst += pitch;
            }
        }
    }; // namespace details


    template <int size, int haveLeft, int haveAbove, typename PixType>
    void predict_intra_nv12_dc_av1_px(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch, int, int, int) {
        details::predict_intra_nv12_dc_av1_px(topPels, leftPels, dst, pitch, 4 << size, haveLeft, haveAbove);
    }
    template void predict_intra_nv12_dc_av1_px<0, 0, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<0, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<0, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<0, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<1, 0, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<1, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<1, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<1, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<2, 0, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<2, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<2, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<2, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<3, 0, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<3, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<3, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_px<3, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    template void predict_intra_nv12_dc_av1_px<0, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<0, 0, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<0, 1, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<0, 1, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<1, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<1, 0, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<1, 1, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<1, 1, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<2, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<2, 0, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<2, 1, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<2, 1, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<3, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<3, 0, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<3, 1, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_dc_av1_px<3, 1, 1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);


#if 0
    const int width = 4 << txSize;

        ALIGN_DECL(32) uint8_t predPels_[32 + width * 3 + width];
        uint8_t *predPels = predPels_ + 31;

        GetPredPelsLumaAV1(rec, pitchRec, predPels, width, (uint8_t)127, (uint8_t)129, haveAbove, haveLeft, 0, haveRight, haveBottomLeft, pixTop, pixTopRight, pixLeft, pixBottomLeft);

        predict_intra_dc_avx2<txSize, haveLeft, haveAbove>(predPels, dst, 12*width); dst += width;
        predict_intra_avx2<txSize, V_PRED>(predPels, dst, 12*width); dst += width;
        predict_intra_avx2<txSize, H_PRED>(predPels, dst, 12*width); dst += width;
        predict_intra_av1_avx2<txSize, D45_PRED>(predPels, dst, 12*width);dst += width;
        predict_intra_avx2<txSize, D135_PRED>(predPels, dst, 12*width); dst += width;
        predict_intra_avx2<txSize, D117_PRED>(predPels, dst, 12*width); dst += width;
        predict_intra_avx2<txSize, D153_PRED>(predPels, dst, 12*width); dst += width;
        predict_intra_av1_avx2<txSize, D207_PRED>(predPels, dst, 12*width); dst += width;
        predict_intra_av1_avx2<txSize, D63_PRED>(predPels, dst, 12*width); dst += width;
        predict_intra_av1_avx2<txSize, SMOOTH_PRED>(predPels, dst, 12*width); dst += width;
        predict_intra_av1_avx2<txSize, PAETH_PRED>(predPels, dst, 12*width);
#endif

    // specialization for AV1
    template <int size, int mode, typename PixType> void predict_intra_av1_px(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch, int delta, int upTop, int upLeft) {
        if      (mode ==  9) details::predict_intra_smooth_av1_px  <size>(topPels, leftPels, dst, pitch);
        else if (mode == 10) details::predict_intra_smooth_v_av1_px<size>(topPels, leftPels, dst, pitch);
        else if (mode == 11) details::predict_intra_smooth_h_av1_px<size>(topPels, leftPels, dst, pitch);
        else if (mode == 12) details::predict_intra_paeth_av1_px   <size>(topPels, leftPels, dst, pitch);
        else {
            assert(mode >= 1 && mode <= 8);
            const int angle = mode_to_angle_map[mode] + delta * 3;
            const int dx = get_dx(angle);
            const int dy = get_dy(angle);

            if      (angle > 0 && angle < 90)    details::predict_intra_z1_av1_px<size>(topPels, dst, pitch, upTop, dx);
            else if (angle > 90 && angle < 180)  details::predict_intra_z2_av1_px<size>(topPels, leftPels, dst, pitch, upTop, upLeft, dx, dy);
            else if (angle > 180 && angle < 270) details::predict_intra_z3_av1_px<size>(leftPels, dst, pitch, upLeft, dy);
            else if (angle == 90)                details::predict_intra_ver_av1_px<size>(topPels, leftPels, dst, pitch);
            else if (angle == 180)               details::predict_intra_hor_av1_px<size>(topPels, leftPels, dst, pitch);
        }
    }

    // specialization for AV1
    template void predict_intra_av1_px<0,  1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,  2, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,  3, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,  4, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,  5, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,  6, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,  7, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,  8, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,  9, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 10, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 11, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 12, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,  1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,  2, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,  3, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,  4, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,  5, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,  6, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,  7, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,  8, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,  9, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 10, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 11, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 12, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,  1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,  2, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,  3, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,  4, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,  5, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,  6, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,  7, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,  8, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,  9, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 10, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 11, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 12, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,  1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,  2, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,  3, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,  4, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,  5, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,  6, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,  7, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,  8, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,  9, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 10, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 11, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 12, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    template void predict_intra_av1_px<0,  1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0,  2, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0,  3, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0,  4, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0,  5, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0,  6, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0,  7, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0,  8, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0,  9, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0, 10, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0, 11, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<0, 12, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1,  1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1,  2, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1,  3, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1,  4, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1,  5, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1,  6, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1,  7, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1,  8, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1,  9, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1, 10, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1, 11, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<1, 12, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2,  1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2,  2, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2,  3, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2,  4, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2,  5, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2,  6, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2,  7, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2,  8, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2,  9, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2, 10, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2, 11, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<2, 12, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3,  1, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3,  2, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3,  3, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3,  4, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3,  5, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3,  6, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3,  7, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3,  8, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3,  9, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3, 10, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3, 11, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_px<3, 12, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    template <int size, int mode, typename PixType>
    void predict_intra_nv12_av1_px(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch, int delta, int upTop, int upLeft) {
        const int width = 4 << size;
        assert(size_t(topPels)  % 32 == 0);
        assert(size_t(leftPels) % 32 == 0);
        IntraPredPels<PixType, width> predPelsU;
        IntraPredPels<PixType, width> predPelsV;
        ALIGN_DECL(32) PixType tmp[width * 2];

        for (int i = -1; i < 2 * width; i++) {
            predPelsU.top[i] = topPels[2 * i + 0];
            predPelsV.top[i] = topPels[2 * i + 1];
            predPelsU.left[i] = leftPels[2 * i + 0];
            predPelsV.left[i] = leftPels[2 * i + 1];
        }
        predict_intra_av1_px<size, mode>(predPelsU.top, predPelsU.left, dst,         pitch, delta, upTop, upLeft);
        predict_intra_av1_px<size, mode>(predPelsV.top, predPelsV.left, dst + width, pitch, delta, upTop, upLeft);
        //convert_pred_block<size>(dst, pitch);
        for (int y = 0; y < width; y++) {
            memcpy(tmp, dst + y * pitch, width * 2 * sizeof(PixType));
            for (int x = 0; x < width; x++) {
                dst[y * pitch + 2 * x + 0] = tmp[x];
                dst[y * pitch + 2 * x + 1] = tmp[x + width];
            }
        }
    }

    template void predict_intra_nv12_av1_px<0, 1,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0, 2,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0, 3,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0, 4,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0, 5,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0, 6,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0, 7,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0, 8,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0, 9,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0,10,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0,11,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<0,12,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1, 1,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1, 2,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1, 3,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1, 4,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1, 5,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1, 6,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1, 7,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1, 8,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1, 9,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1,10,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1,11,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<1,12,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2, 1,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2, 2,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2, 3,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2, 4,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2, 5,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2, 6,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2, 7,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2, 8,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2, 9,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2,10,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2,11,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<2,12,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3, 1,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3, 2,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3, 3,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3, 4,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3, 5,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3, 6,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3, 7,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3, 8,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3, 9,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3,10,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3,11,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_px<3,12,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    template void predict_intra_nv12_av1_px<0, 1,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0, 2,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0, 3,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0, 4,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0, 5,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0, 6,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0, 7,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0, 8,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0, 9,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0,10,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0,11,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<0,12,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1, 1,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1, 2,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1, 3,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1, 4,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1, 5,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1, 6,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1, 7,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1, 8,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1, 9,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1,10,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1,11,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<1,12,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2, 1,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2, 2,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2, 3,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2, 4,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2, 5,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2, 6,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2, 7,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2, 8,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2, 9,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2,10,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2,11,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<2,12,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3, 1,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3, 2,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3, 3,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3, 4,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3, 5,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3, 6,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3, 7,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3, 8,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3, 9,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3,10,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3,11,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_nv12_av1_px<3,12,uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    template <int size> void predict_intra_palette_px(uint8_t *dst, int32_t pitch, const uint8_t *palette, const uint8_t *color_map)
    {
        const int32_t width = 4 << size;
        for (int32_t i = 0; i < width; i++) {
            for (int32_t j = 0; j < width; j++) {
                dst[i*pitch + j] = palette[color_map[i * 64 + j]];
            }
        }
    }
    template void predict_intra_palette_px<0>(uint8_t *dst, int32_t pitch, const uint8_t *palette, const uint8_t *color_map);
    template void predict_intra_palette_px<1>(uint8_t *dst, int32_t pitch, const uint8_t *palette, const uint8_t *color_map);
    template void predict_intra_palette_px<2>(uint8_t *dst, int32_t pitch, const uint8_t *palette, const uint8_t *color_map);
    template void predict_intra_palette_px<3>(uint8_t *dst, int32_t pitch, const uint8_t *palette, const uint8_t *color_map);
}; // namespace AV1PP
