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

#include <algorithm>

#include "mfx_av1_cfl.h"

static constexpr int32_t BSR(uint32_t u32) {
    return u32 == 1 ? 0 : BSR(u32>>1) + 1;
}

static void subtract_average_px_impl(const uint16_t *src, int16_t *dst, int width,
    int height, int round_offset, int num_pel_log2) {
    int sum = round_offset;
    const uint16_t *recon = src;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            sum += recon[i];
        }
        recon += CFL_BUF_LINE;
    }
    const int avg = sum >> num_pel_log2;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            dst[i] = src[i] - avg;
        }
        src += CFL_BUF_LINE;
        dst += CFL_BUF_LINE;
    }
}

namespace AV1PP {
    template<int width, int height> void subtract_average_px(const uint16_t *src, int16_t *dst) {
        constexpr int sz = width * height;
        constexpr int num_pel_log2 = BSR(sz);
        constexpr int round_offset = ((sz) >> 1);

        subtract_average_px_impl(src, dst, width, height, round_offset, num_pel_log2);
    }
    template void subtract_average_px< 4, 4>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px< 4, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px< 4, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px< 8, 4>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px< 8, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px< 8, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px< 8, 32>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px<16, 4>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px<16, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px<16, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px<16, 32>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px<32, 8>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px<32, 16>(const uint16_t *src, int16_t *dst);
    template void subtract_average_px<32, 32>(const uint16_t *src, int16_t *dst);
};

static void cfl_luma_subsampling_420_u8_px_impl(const uint8_t *input,
    int input_stride,
    uint16_t *output_q3, int width,
    int height) {
    for (int j = 0; j < height; j += 2) {
        for (int i = 0; i < width; i += 2) {
            const int bot = i + input_stride;
            output_q3[i >> 1] =
                (input[i] + input[i + 1] + input[bot] + input[bot + 1]) << 1;
        }
        input += input_stride << 1;
        output_q3 += CFL_BUF_LINE;
    }
}

static void cfl_luma_subsampling_422_u8_px_impl(const uint8_t *input,
    int input_stride,
    uint16_t *output_q3, int width,
    int height) {
    assert((height - 1) * CFL_BUF_LINE + width <= CFL_BUF_SQUARE);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i += 2) {
            output_q3[i >> 1] = (input[i] + input[i + 1]) << 2;
        }
        input += input_stride;
        output_q3 += CFL_BUF_LINE;
    }
}

static void cfl_luma_subsampling_444_u8_px_impl(const uint8_t *input,
    int input_stride,
    uint16_t *output_q3, int width,
    int height) {
    assert((height - 1) * CFL_BUF_LINE + width <= CFL_BUF_SQUARE);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            output_q3[i] = input[i] << 3;
        }
        input += input_stride;
        output_q3 += CFL_BUF_LINE;
    }
}

static void cfl_luma_subsampling_420_u16_px_impl(const uint16_t *input,
    int input_stride,
    uint16_t *output_q3, int width,
    int height) {
    for (int j = 0; j < height; j += 2) {
        for (int i = 0; i < width; i += 2) {
            const int bot = i + input_stride;
            output_q3[i >> 1] =
                (input[i] + input[i + 1] + input[bot] + input[bot + 1]) << 1;
        }
        input += input_stride << 1;
        output_q3 += CFL_BUF_LINE;
    }
}

static void cfl_luma_subsampling_422_u16_px_impl(const uint16_t *input,
    int input_stride,
    uint16_t *output_q3, int width,
    int height) {
    assert((height - 1) * CFL_BUF_LINE + width <= CFL_BUF_SQUARE);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i += 2) {
            output_q3[i >> 1] = (input[i] + input[i + 1]) << 2;
        }
        input += input_stride;
        output_q3 += CFL_BUF_LINE;
    }
}

static void cfl_luma_subsampling_444_u16_px_impl(const uint16_t *input,
    int input_stride,
    uint16_t *output_q3, int width,
    int height) {
    assert((height - 1) * CFL_BUF_LINE + width <= CFL_BUF_SQUARE);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            output_q3[i] = input[i] << 3;
        }
        input += input_stride;
        output_q3 += CFL_BUF_LINE;
    }
}

namespace AV1PP {
    template<int width, int height> void cfl_luma_subsampling_420_u8_px(const uint8_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_420_u8_px_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_420_u8_px< 4, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px< 4, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px< 4, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px< 8, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px< 8, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px< 8, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px< 8, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px<16, 4>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px<16, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px<16, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px<16, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px<32, 8>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px<32, 16>(const uint8_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u8_px<32, 32>(const uint8_t *input, int input_stride, uint16_t *output_q3);

    template<int width, int height> void cfl_luma_subsampling_420_u16_px(const uint16_t *input, int input_stride, uint16_t *output_q3) {
        cfl_luma_subsampling_420_u16_px_impl(input, input_stride, output_q3, width, height);
    }
    template void cfl_luma_subsampling_420_u16_px< 4, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px< 4, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px< 4, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px< 8, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px< 8, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px< 8, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px< 8, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px<16, 4>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px<16, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px<16, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px<16, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px<32, 8>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px<32, 16>(const uint16_t *input, int input_stride, uint16_t *output_q3);
    template void cfl_luma_subsampling_420_u16_px<32, 32>(const uint16_t *input, int input_stride, uint16_t *output_q3);

};


template<typename PixType>
void cfl_predict_nv12_px_impl(const int16_t *pred_buf_q3, PixType *dst, int dst_stride, PixType dcU, PixType dcV, int alpha_q3, int bd, int width, int height) {
    const int maxPix = (1 << bd) - 1;

    const int16_t* ac = pred_buf_q3;
    PixType* out = dst;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < (width << 1); i += 2) {
            const int scaled_luma_q6 = alpha_q3 * ac[i >> 1];
            const int val = scaled_luma_q6 < 0 ? -(((-scaled_luma_q6) + (1 << 5)) >> 6) : (scaled_luma_q6 + (1 << 5)) >> 6;
            out[i] = PixType(std::min(std::max(val + dcU, 0), maxPix)); //add U DC pred
            out[i + 1] = PixType(std::min(std::max(val + dcV, 0), maxPix)); //add V DC pred
        }
        out += dst_stride;
        ac += 32; //32 is predifined
    }
}

namespace AV1PP {
    template<typename PixType, int width, int height> void cfl_predict_nv12_px(const int16_t *pred_buf_q3, PixType *dst, int dst_stride, PixType dcU, PixType dcV, int alpha_q3, int bd) {
        cfl_predict_nv12_px_impl(pred_buf_q3, dst, dst_stride, dcU, dcV, alpha_q3, bd, width, height);
    }
    template void cfl_predict_nv12_px<uint8_t, 4, 4>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 4, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 4, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 8, 4>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 8, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 8, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 8, 32>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 16, 4>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 16, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 16, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 16, 32>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 32, 8>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 32, 16>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint8_t, 32, 32>(const int16_t *pred_buf_q3, uint8_t* dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);

    template void cfl_predict_nv12_px<uint16_t, 4, 4>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 4, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 4, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 8, 4>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 8, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 8, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 8, 32>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 16, 4>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 16, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 16, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 16, 32>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 32, 8>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 32, 16>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);
    template void cfl_predict_nv12_px<uint16_t, 32, 32>(const int16_t *pred_buf_q3, uint16_t* dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);

};

