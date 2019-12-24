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
#include "stdint.h"

namespace {
    const int FILTER_BITS = 7;
    const int SUBPEL_TAPS = 8;
    const int ROUND_STAGE0 = 3;
    const int ROUND_STAGE1 = 11;
    const int COMPOUND_ROUND1_BITS = 7;
    //const int MAX_BLOCK_SIZE = 64;
    const int MAX_BLOCK_SIZE = 96;
    const int OFFSET_BITS = 2 * FILTER_BITS - ROUND_STAGE0 - COMPOUND_ROUND1_BITS;
    const int IMPLIED_PITCH = 64;

    enum { no_horz=0, do_horz=1 };
    enum { no_vert=0, do_vert=1 };

    int round_power_of_two(int value, int n) { return (value + ((1 << n) >> 1)) >> n; };
    uint8_t clip_pixel(int val) { return (val > 255) ? 255 : (val < 0) ? 0 : val; }

    void interp(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h, int round1)
    {
        assert(w <= MAX_BLOCK_SIZE && h <= MAX_BLOCK_SIZE);
        const int pitchTmp = w;
        int32_t tmpBuf[MAX_BLOCK_SIZE * (MAX_BLOCK_SIZE + SUBPEL_TAPS - 1)];
        int32_t *tmp = tmpBuf;

        const int im_height = h + SUBPEL_TAPS - 1;
        src -= (SUBPEL_TAPS / 2 - 1) * pitchSrc + (SUBPEL_TAPS / 2 - 1);

        for (int y = 0; y < im_height; y++) {
            for (int x = 0; x < w; x++) {
                int sum = 0;
                for (int k = 0; k < SUBPEL_TAPS; ++k)
                    sum += fx[k] * src[y * pitchSrc + x + k];
                tmp[y * pitchTmp + x] = round_power_of_two(sum, ROUND_STAGE0);
            }
        }

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int sum = 0;
                for (int k = 0; k < SUBPEL_TAPS; ++k)
                    sum += fy[k] * tmp[(y + k) * pitchTmp + x];
                int res = round_power_of_two(sum, round1);
                dst[y * pitchDst + x] = res;
            }
        }
    }


    void interp(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h)
    {
        const int pitchTmp = w;
        int16_t tmp[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        interp(src, pitchSrc, tmp, w, fx, fy, w, h, ROUND_STAGE1);

        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                dst[y * pitchDst + x] = clip_pixel(tmp[y * pitchTmp + x]);
    }


    void interp(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h)
    {
        const int pitchTmp = w;
        int16_t tmp[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        interp(src, pitchSrc, tmp, w, fx, fy, w, h, COMPOUND_ROUND1_BITS);

        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                dst[y * pitchDst + x] = clip_pixel(round_power_of_two(ref0[y * pitchRef0 + x] + tmp[y * pitchTmp + x], OFFSET_BITS + 1));
    }


    template <typename T>
    void nv12_to_yv12(const T *src, int pitchSrc, T *dstU, int pitchDstU, T *dstV, int pitchDstV, int w, int h)
    {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                dstU[y * pitchDstU + x] = src[y * pitchSrc + 2 * x + 0];
                dstV[y * pitchDstV + x] = src[y * pitchSrc + 2 * x + 1];
            }
        }
    }


    template <typename T>
    void yv12_to_nv12(const T *srcU, int pitchSrcU, const T *srcV, int pitchSrcV, T *dst, int pitchDst, int w, int h)
    {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                dst[y * pitchDst + 2 * x + 0] = srcU[y * pitchSrcU + x];
                dst[y * pitchDst + 2 * x + 1] = srcV[y * pitchSrcV + x];
            }
        }
    }
};

namespace VP9PP
{
    // single ref, luma
    template <int w, int horz, int vert>
    void interp_av1_px(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
    }

    // first ref (compound), luma
    template <int w, int horz, int vert>
    void interp_av1_px(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp(src, pitchSrc, dst, pitchDst, fx, fy, w, h, COMPOUND_ROUND1_BITS);
    }

    // second ref (compound), luma
    template <int w, int horz, int vert>
    void interp_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, w, h);
    }

    // single ref, luma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_pitch64_av1_px(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_px<w, horz, vert>(src, pitchSrc, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // first ref (compound), luma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_pitch64_av1_px(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_px<w, horz, vert>(src, pitchSrc, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // second ref (compound), luma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_pitch64_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_px<w, horz, vert>(src, pitchSrc, ref0, IMPLIED_PITCH, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // single ref, chroma
    template <int w, int horz, int vert>
    void interp_nv12_av1_px(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        uint8_t bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        uint8_t bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        uint8_t dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        uint8_t dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
        const int32_t extW = w + SUBPEL_TAPS - 1;
        const int32_t extH = h + SUBPEL_TAPS - 1;
        const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
        const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

        uint8_t *srcU = bufSrcU;
        uint8_t *srcV = bufSrcV;
        src -= shiftH * pitchSrc + shiftW * 2;
        nv12_to_yv12(src, pitchSrc, srcU, pitchSrcYv12, srcV, pitchSrcYv12, extW, extH);

        srcU += shiftH * pitchSrcYv12 + shiftW;
        srcV += shiftH * pitchSrcYv12 + shiftW;
        interp_av1_px<w, horz, vert>(srcU, pitchSrcYv12, dstU, w, fx, fy, h);
        interp_av1_px<w, horz, vert>(srcV, pitchSrcYv12, dstV, w, fx, fy, h);

        yv12_to_nv12(dstU, w, dstV, w, dst, pitchDst, w, h);
    }

    // first ref (compound), chroma
    template <int w, int horz, int vert>
    void interp_nv12_av1_px(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        uint8_t bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        uint8_t bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        int16_t dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        int16_t dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
        const int32_t extW = w + SUBPEL_TAPS - 1;
        const int32_t extH = h + SUBPEL_TAPS - 1;
        const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
        const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

        uint8_t *srcU = bufSrcU;
        uint8_t *srcV = bufSrcV;
        src -= shiftH * pitchSrc + shiftW * 2;
        nv12_to_yv12(src, pitchSrc, srcU, pitchSrcYv12, srcV, pitchSrcYv12, extW, extH);

        srcU += shiftH * pitchSrcYv12 + shiftW;
        srcV += shiftH * pitchSrcYv12 + shiftW;
        interp_av1_px<w, horz, vert>(srcU, pitchSrcYv12, dstU, w, fx, fy, h);
        interp_av1_px<w, horz, vert>(srcV, pitchSrcYv12, dstV, w, fx, fy, h);

        yv12_to_nv12(dstU, w, dstV, w, dst, pitchDst, w, h);
    }

    // second ref (compound), chroma
    template <int w, int horz, int vert>
    void interp_nv12_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        uint8_t bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        uint8_t bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        int16_t ref0U[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        int16_t ref0V[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        uint8_t dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        uint8_t dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
        const int32_t extW = w + SUBPEL_TAPS - 1;
        const int32_t extH = h + SUBPEL_TAPS - 1;
        const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
        const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

        uint8_t *srcU = bufSrcU;
        uint8_t *srcV = bufSrcV;
        src -= shiftH * pitchSrc + shiftW * 2;
        nv12_to_yv12(src, pitchSrc, srcU, pitchSrcYv12, srcV, pitchSrcYv12, extW, extH);
        nv12_to_yv12(ref0, pitchRef0, ref0U, w, ref0V, w, w, h);

        srcU += shiftH * pitchSrcYv12 + shiftW;
        srcV += shiftH * pitchSrcYv12 + shiftW;
        interp_av1_px<w, horz, vert>(srcU, pitchSrcYv12, ref0U, w, dstU, w, fx, fy, h);
        interp_av1_px<w, horz, vert>(srcV, pitchSrcYv12, ref0V, w, dstV, w, fx, fy, h);

        yv12_to_nv12(dstU, w, dstV, w, dst, pitchDst, w, h);
    }

    // single ref, chroma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_px(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        const int pitchDstImplied = 64;
        interp_nv12_av1_px<w, horz, vert>(src, pitchSrc, dst, pitchDstImplied, fx, fy, h);
    }

    // first ref (compound), chroma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_px(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        const int pitchDstImplied = 64;
        interp_nv12_av1_px<w, horz, vert>(src, pitchSrc, dst, pitchDstImplied, fx, fy, h);
    }

    // second ref (compound), chroma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        const int pitchRef0Implied = 64;
        const int pitchDstImplied = 64;
        interp_nv12_av1_px<w, horz, vert>(src, pitchSrc, ref0, pitchRef0Implied, dst, pitchDstImplied, fx, fy, h);
    }


    // Instatiate interp functions for:
    //   single ref / first ref of compound / second ref of compound
    //   luma / chroma
    //   dst pitch / dst pitch-less
    //   no_horz / do_horz
    //   no_vert / do_vert
    //   width 4 / 8 / 16 / 32 / 64 / 96

    typedef void (* interp_av1_single_ref_fptr_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_av1_first_ref_fptr_t)(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_av1_second_ref_fptr_t)(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_pitch64_av1_single_ref_fptr_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_pitch64_av1_first_ref_fptr_t)(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_pitch64_av1_second_ref_fptr_t)(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

#define INSTATIATE(FUNC_NAME)                                                                         \
    { { FUNC_NAME< 4, 0, 0>, FUNC_NAME< 4, 0, 1> }, { FUNC_NAME< 4, 1, 0>, FUNC_NAME< 4, 1, 1> } },   \
    { { FUNC_NAME< 8, 0, 0>, FUNC_NAME< 8, 0, 1> }, { FUNC_NAME< 8, 1, 0>, FUNC_NAME< 8, 1, 1> } },   \
    { { FUNC_NAME<16, 0, 0>, FUNC_NAME<16, 0, 1> }, { FUNC_NAME<16, 1, 0>, FUNC_NAME<16, 1, 1> } },   \
    { { FUNC_NAME<32, 0, 0>, FUNC_NAME<32, 0, 1> }, { FUNC_NAME<32, 1, 0>, FUNC_NAME<32, 1, 1> } },   \
    { { FUNC_NAME<64, 0, 0>, FUNC_NAME<64, 0, 1> }, { FUNC_NAME<64, 1, 0>, FUNC_NAME<64, 1, 1> } }

#define INSTATIATE6(FUNC_NAME)                                                                     \
    INSTATIATE(FUNC_NAME),                                                                         \
    { { FUNC_NAME<96, 0, 0>, FUNC_NAME<96, 0, 1> }, { FUNC_NAME<96, 1, 0>, FUNC_NAME<96, 1, 1> } }

    interp_av1_single_ref_fptr_t interp_av1_single_ref_fptr_arr_px[6][2][2] = { INSTATIATE6(interp_av1_px) };
    interp_av1_first_ref_fptr_t  interp_av1_first_ref_fptr_arr_px[5][2][2]  = { INSTATIATE(interp_av1_px) };
    interp_av1_second_ref_fptr_t interp_av1_second_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_av1_px) };

    interp_av1_single_ref_fptr_t interp_nv12_av1_single_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_nv12_av1_px) };
    interp_av1_first_ref_fptr_t  interp_nv12_av1_first_ref_fptr_arr_px[5][2][2]  = { INSTATIATE(interp_nv12_av1_px) };
    interp_av1_second_ref_fptr_t interp_nv12_av1_second_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_nv12_av1_px) };

    interp_pitch64_av1_single_ref_fptr_t interp_pitch64_av1_single_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_pitch64_av1_px) };
    interp_pitch64_av1_first_ref_fptr_t  interp_pitch64_av1_first_ref_fptr_arr_px[5][2][2]  = { INSTATIATE(interp_pitch64_av1_px) };
    interp_pitch64_av1_second_ref_fptr_t interp_pitch64_av1_second_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_pitch64_av1_px) };

    interp_pitch64_av1_single_ref_fptr_t interp_nv12_pitch64_av1_single_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_nv12_pitch64_av1_px) };
    interp_pitch64_av1_first_ref_fptr_t  interp_nv12_pitch64_av1_first_ref_fptr_arr_px[5][2][2]  = { INSTATIATE(interp_nv12_pitch64_av1_px) };
    interp_pitch64_av1_second_ref_fptr_t interp_nv12_pitch64_av1_second_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_nv12_pitch64_av1_px) };

}; // namespace VP9PP
