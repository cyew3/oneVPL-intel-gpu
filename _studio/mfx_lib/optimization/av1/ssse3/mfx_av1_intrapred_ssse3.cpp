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
#include "stddef.h"

#include <cstdint>

typedef uint8_t uint8_t;

extern "C" void vpx_d45_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d45_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d45_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d45_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

extern "C" void vpx_d63_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d63_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d63_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d63_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

extern "C" void vpx_d153_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d153_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d153_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d153_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

extern "C" void vpx_d207_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d207_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d207_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_d207_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

extern "C" void vpx_tm_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_tm_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_tm_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_tm_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

extern "C" void vpx_v_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_v_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_v_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_v_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

extern "C" void vpx_h_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_h_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_h_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_h_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);


extern "C" void vpx_dc_128_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_128_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_128_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_128_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

extern "C" void vpx_dc_left_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_left_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_left_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_left_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

extern "C" void vpx_dc_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

extern "C" void vpx_dc_top_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_top_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_top_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
extern "C" void vpx_dc_top_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

namespace AV1PP
{
    void predictIntra_D45_ssse3(const uint8_t* refPel, uint8_t* dst, int32_t pitch, int32_t width)
    {
        const uint8_t* above = refPel + 1;
        const uint8_t* left  = refPel + 1 + 2*width;

        switch (width) {
        case 4:
            vpx_d45_predictor_4x4_sse2(dst, pitch, above, left); break;
        case 8:
            vpx_d45_predictor_8x8_sse2(dst, pitch, above, left); break;
        case 16:
            vpx_d45_predictor_16x16_ssse3(dst, pitch, above, left); break;
        case 32:
        default:
            vpx_d45_predictor_32x32_ssse3(dst, pitch, above, left); break;
        };
    }

    void predictIntra_D63_ssse3(const uint8_t* refPel, uint8_t* dst, int32_t pitch, int32_t width)
    {
        const uint8_t* above = refPel + 1;
        const uint8_t* left  = refPel + 1 + 2*width;

        switch (width) {
        case 4:
            vpx_d63_predictor_4x4_ssse3(dst, pitch, above, left); break;
        case 8:
            vpx_d63_predictor_8x8_ssse3(dst, pitch, above, left); break;
        case 16:
            vpx_d63_predictor_16x16_ssse3(dst, pitch, above, left); break;
        case 32:
        default:
            vpx_d63_predictor_32x32_ssse3(dst, pitch, above, left); break;
        };
    }

    void predictIntra_D153_ssse3(const uint8_t* refPel, uint8_t* dst, int32_t pitch, int32_t width)
    {
        const uint8_t* above = refPel + 1;
        const uint8_t* left  = refPel + 1 + 2*width;

        switch (width) {
        case 4:
            vpx_d153_predictor_4x4_ssse3(dst, pitch, above, left); break;
        case 8:
            vpx_d153_predictor_8x8_ssse3(dst, pitch, above, left); break;
        case 16:
            vpx_d153_predictor_16x16_ssse3(dst, pitch, above, left); break;
        case 32:
        default:
            vpx_d153_predictor_32x32_ssse3(dst, pitch, above, left); break;
        };
    }

    void predictIntra_D207_ssse3(const uint8_t* refPel, uint8_t* dst, int32_t pitch, int32_t width)
    {
        const uint8_t* above = refPel + 1;
        const uint8_t* left  = refPel + 1 + 2*width;

        switch (width) {
        case 4:
            vpx_d207_predictor_4x4_sse2(dst, pitch, above, left); break;
        case 8:
            vpx_d207_predictor_8x8_ssse3(dst, pitch, above, left); break;
        case 16:
            vpx_d207_predictor_16x16_ssse3(dst, pitch, above, left); break;
        case 32:
        default:
            vpx_d207_predictor_32x32_ssse3(dst, pitch, above, left); break;
        };
    }

    void predictIntra_TM_sse2(const uint8_t* refPel, uint8_t* dst, int32_t pitch, int32_t width)
    {
        const uint8_t* above = refPel + 1;
        const uint8_t* left  = refPel + 1 + 2*width;

        switch (width) {
        case 4:
            vpx_tm_predictor_4x4_sse2(dst, pitch, above, left); break;
        case 8:
            vpx_tm_predictor_8x8_sse2(dst, pitch, above, left); break;
        case 16:
            vpx_tm_predictor_16x16_sse2(dst, pitch, above, left); break;
        case 32:
        default:
            vpx_tm_predictor_32x32_sse2(dst, pitch, above, left); break;
        };
    }

    void predictIntra_Ver_sse2(const uint8_t* refPel, uint8_t* dst, int32_t pitch, int32_t width)
    {
        const uint8_t* above = refPel + 1;
        const uint8_t* left  = refPel + 1 + 2*width;

        switch (width) {
        case 4:
            vpx_v_predictor_4x4_sse2(dst, pitch, above, left); break;
        case 8:
            vpx_v_predictor_8x8_sse2(dst, pitch, above, left); break;
        case 16:
            vpx_v_predictor_16x16_sse2(dst, pitch, above, left); break;
        case 32:
        default:
            vpx_v_predictor_32x32_sse2(dst, pitch, above, left); break;
        };
    }

    void predictIntra_Hor_sse2(const uint8_t* refPel, uint8_t* dst, int32_t pitch, int32_t width)
    {
        const uint8_t* above = refPel + 1;
        const uint8_t* left  = refPel + 1 + 2*width;

        switch (width) {
        case 4:
            vpx_h_predictor_4x4_sse2(dst, pitch, above, left); break;
        case 8:
            vpx_h_predictor_8x8_sse2(dst, pitch, above, left); break;
        case 16:
            vpx_h_predictor_16x16_sse2(dst, pitch, above, left); break;
        case 32:
        default:
            vpx_h_predictor_32x32_sse2(dst, pitch, above, left); break;
        };
    }

    void predictIntra_DC_sse2(const uint8_t* refPel, uint8_t* dst, int32_t pitch, int32_t width, int32_t leftAvailable, int32_t topAvailable)
    {
        const uint8_t* above = refPel + 1;
        const uint8_t* left  = refPel + 1 + 2*width;

        if (leftAvailable && topAvailable) {
            switch (width) {
            case 4:
                vpx_dc_predictor_4x4_sse2(dst, pitch, above, left); break;
            case 8:
                vpx_dc_predictor_8x8_sse2(dst, pitch, above, left); break;
            case 16:
                vpx_dc_predictor_16x16_sse2(dst, pitch, above, left); break;
            case 32:
            default:
                vpx_dc_predictor_32x32_sse2(dst, pitch, above, left); break;
            };

        } else if (leftAvailable) {
            switch (width) {
            case 4:
                vpx_dc_left_predictor_4x4_sse2(dst, pitch, above, left); break;
            case 8:
                vpx_dc_left_predictor_8x8_sse2(dst, pitch, above, left); break;
            case 16:
                vpx_dc_left_predictor_16x16_sse2(dst, pitch, above, left); break;
            case 32:
            default:
                vpx_dc_left_predictor_32x32_sse2(dst, pitch, above, left); break;
            };

        } else if (topAvailable) {
            switch (width) {
            case 4:
                vpx_dc_top_predictor_4x4_sse2(dst, pitch, above, left); break;
            case 8:
                vpx_dc_top_predictor_8x8_sse2(dst, pitch, above, left); break;
            case 16:
                vpx_dc_top_predictor_16x16_sse2(dst, pitch, above, left); break;
            case 32:
            default:
                vpx_dc_top_predictor_32x32_sse2(dst, pitch, above, left); break;
            };
        } else {
            switch (width) {
            case 4:
                vpx_dc_128_predictor_4x4_sse2(dst, pitch, above, left); break;
            case 8:
                vpx_dc_128_predictor_8x8_sse2(dst, pitch, above, left); break;
            case 16:
                vpx_dc_128_predictor_16x16_sse2(dst, pitch, above, left); break;
            case 32:
            default:
                vpx_dc_128_predictor_32x32_sse2(dst, pitch, above, left); break;
            };
        }
    }
}; // namespace AV1PP
