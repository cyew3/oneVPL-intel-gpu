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
#include "ipps.h"
#include "stdint.h"

    /* Shift down with rounding */
#define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n)-1))) >> (n))

#define SUBPEL_BITS 4
#define SUBPEL_MASK ((1 << SUBPEL_BITS) - 1)
#define SUBPEL_SHIFTS (1 << SUBPEL_BITS)

const int FILTER_BITS = 7;
const int SUBPEL_TAPS = 8;
const int ROUND_STAGE0 = 3;
const int ROUND_STAGE1 = 11;
const int COMPOUND_ROUND1_BITS = 7;
//const int MAX_BLOCK_SIZE = 64;
const int MAX_BLOCK_SIZE = 96;
const int OFFSET_BITS = 2 * FILTER_BITS - ROUND_STAGE0 - COMPOUND_ROUND1_BITS;
const int IMPLIED_PITCH = 64;

enum { no_horz = 0, do_horz = 1 };
enum { no_vert = 0, do_vert = 1 };

int round_power_of_two(int value, int n) { return (value + ((1 << n) >> 1)) >> n; };
uint8_t clip_pixel(int val) { return (val > 255) ? 255 : (val < 0) ? 0 : val; }
uint16_t clip_pixel10(int val) { return (val > 1023) ? 1023 : (val < 0) ? 0 : val; }


namespace details {
    typedef int16_t int16_t;
    typedef uint8_t uint8_t;
    typedef int16_t InterpKernel[SUBPEL_TAPS];

    static inline uint8_t clip_pixel(int val)
    {
        return (val > 255) ? 255 : (val < 0) ? 0 : val;
    }

    static const InterpKernel *get_filter_base(const int16_t *filter)
    {
        // NOTE: This assumes that the filter table is 256-byte aligned.
        // TODO(agrange) Modify to make independent of table alignment.
        return (const InterpKernel *)(((intptr_t)filter) & ~((intptr_t)0xFF));
    }

    static int get_filter_offset(const int16_t *f, const InterpKernel *base)
    {
        return (int)((const InterpKernel *)(intptr_t)f - base);
    }

    void average_px(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int w, int h)
    {
        for (int y = 0; y < h; y++, src1 += pitchSrc1, src2 += pitchSrc2, dst += pitchDst)
            for (int x = 0; x < w; x++)
                dst[x] = (uint8_t)((unsigned(src1[x]) + unsigned(src2[x]) + 1) >> 1);
    }

    template <bool avg> void convolve8_copy_px_impl(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
    {
        int r;

        (void)filter_x;
        (void)filter_y;

        for (r = h; r > 0; --r) {
            if (avg)
                for (int x = 0; x < w; ++x) dst[x] = ROUND_POWER_OF_TWO(dst[x] + src[x], 1);
            else
                memcpy(dst, src, w);
            src += src_stride;
            dst += dst_stride;
        }
    }

    template <bool avg, int w> void convolve8_copy_px_impl(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int h) {
        convolve8_copy_px_impl<avg>(src, src_stride, dst, dst_stride, filter_x, filter_y, w, h);
    }

    template <bool avg> void convolve8_nv12_copy_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
    {
        int r;

        (void)filter_x;
        (void)filter_y;

        if (avg) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    dst[2*x]   = ROUND_POWER_OF_TWO(dst[2*x] + src[2*x], 1);
                    dst[2*x+1] = ROUND_POWER_OF_TWO(dst[2*x+1] + src[2*x+1], 1);
                }

                src += src_stride;
                dst += dst_stride;
            }
        } else {
            for (r = h; r > 0; --r) {
                memcpy(dst, src, 2*w);
                src += src_stride;
                dst += dst_stride;
            }
        }
    }

    static void core_convolve_vert(
        const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
        const InterpKernel *y_filters, int y0_q4, int y_step_q4, int w, int h)
    {
            int x, y;
            src -= src_stride * (SUBPEL_TAPS / 2 - 1);

            for (x = 0; x < w; ++x) {
                int y_q4 = y0_q4;
                for (y = 0; y < h; ++y) {
                    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
                    const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
                    int k, sum = 0;
                    for (k = 0; k < SUBPEL_TAPS; ++k)
                        sum += src_y[k * src_stride] * y_filter[k];
                    dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
                    y_q4 += y_step_q4;
                }
                ++src;
                ++dst;
            }
    }

    static void core_convolve_avg_vert(const uint8_t *src, int src_stride,
        uint8_t *dst, int dst_stride,
        const InterpKernel *y_filters, int y0_q4,
        int y_step_q4, int w, int h)
    {
        int x, y;
        src -= src_stride * (SUBPEL_TAPS / 2 - 1);

        for (x = 0; x < w; ++x) {
            int y_q4 = y0_q4;
            for (y = 0; y < h; ++y) {
                const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
                const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
                int k, sum = 0;
                for (k = 0; k < SUBPEL_TAPS; ++k)
                    sum += src_y[k * src_stride] * y_filter[k];
                dst[y * dst_stride] = ROUND_POWER_OF_TWO(
                    dst[y * dst_stride] +
                    clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)),
                    1);
                y_q4 += y_step_q4;
            }
            ++src;
            ++dst;
        }
    }

    template <bool avg> void convolve8_vert_px_impl(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
    {
        const InterpKernel *const filters_y = get_filter_base(filter_y);
        const int y0_q4 = get_filter_offset(filter_y, filters_y);

        (void)filter_x;

        (avg)
            ? core_convolve_avg_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4, 16, w, h)
            : core_convolve_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4, 16, w, h);
    }
    template <bool avg, int w> void convolve8_vert_px_impl(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int h) {
        convolve8_vert_px_impl<avg>(src, src_stride, dst, dst_stride, filter_x, filter_y, w, h);
    }

    static void core_convolve_vert_chromaNV12(
        const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
        const InterpKernel *y_filters, int y0_q4, int y_step_q4, int w, int h)
    {
            int x, y;
            src -= src_stride * (SUBPEL_TAPS / 2 - 1);

            for (x = 0; x < w; ++x) {
                int y_q4 = y0_q4;

                for (y = 0; y < h; ++y) {
                    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
                    const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
                    int sum[2] = {0};
                    for (int k = 0; k < SUBPEL_TAPS; ++k) {
                        sum[0] += src_y[k * src_stride] * y_filter[k];
                        sum[1] += src_y[k * src_stride + 1] * y_filter[k];
                    }
                    dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum[0], FILTER_BITS));
                    dst[y * dst_stride+1] = clip_pixel(ROUND_POWER_OF_TWO(sum[1], FILTER_BITS));
                    y_q4 += y_step_q4;
                }
                ++src;
                ++src;
                ++dst;
                ++dst;
            }
    }

    static void core_convolve_avg_vert_chromaNV12(const uint8_t *src, int src_stride,
        uint8_t *dst, int dst_stride,
        const InterpKernel *y_filters, int y0_q4,
        int y_step_q4, int w, int h)
    {
        int x, y;
        src -= src_stride * (SUBPEL_TAPS / 2 - 1);

        for (x = 0; x < w; ++x) {
            int y_q4 = y0_q4;
            for (y = 0; y < h; ++y) {
                const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
                const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
                int sum[2] = {0};
                for (int k = 0; k < SUBPEL_TAPS; ++k) {
                    sum[0] += src_y[k * src_stride] * y_filter[k];
                    sum[1] += src_y[k * src_stride+1] * y_filter[k];
                }

                dst[y * dst_stride] = ROUND_POWER_OF_TWO(
                    dst[y * dst_stride] +
                    clip_pixel(ROUND_POWER_OF_TWO(sum[0], FILTER_BITS)),
                    1);

                dst[y * dst_stride+1] = ROUND_POWER_OF_TWO(
                    dst[y * dst_stride+1] +
                    clip_pixel(ROUND_POWER_OF_TWO(sum[1], FILTER_BITS)),
                    1);

                y_q4 += y_step_q4;
            }
            ++src;
            ++src;
            ++dst;
            ++dst;
        }
    }

    template <bool avg> void convolve8_nv12_vert_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
    {
        const InterpKernel *const filters_y = get_filter_base(filter_y);
        const int y0_q4 = get_filter_offset(filter_y, filters_y);

        (void)filter_x;

        (avg)
            ? core_convolve_avg_vert_chromaNV12(src, src_stride, dst, dst_stride, filters_y, y0_q4, 16, w, h)
            : core_convolve_vert_chromaNV12(src, src_stride, dst, dst_stride, filters_y, y0_q4, 16, w, h);
    }

    static void core_convolve_horz(const uint8_t *src, int src_stride,
        uint8_t *dst, int dst_stride,
        const InterpKernel *x_filters, int x0_q4,
        int x_step_q4, int w, int h)
    {
        int x, y;
        src -= SUBPEL_TAPS / 2 - 1;
        for (y = 0; y < h; ++y) {
            int x_q4 = x0_q4;
            for (x = 0; x < w; ++x) {
                const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
                const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
                int k, sum = 0;
                for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
                dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
                x_q4 += x_step_q4;
            }
            src += src_stride;
            dst += dst_stride;
        }
    }

    static void core_convolve_avg_horz(const uint8_t *src, int src_stride,
        uint8_t *dst, int dst_stride,
        const InterpKernel *x_filters, int x0_q4,
        int x_step_q4, int w, int h)
    {
        int x, y;
        src -= SUBPEL_TAPS / 2 - 1;
        for (y = 0; y < h; ++y) {
            int x_q4 = x0_q4;
            for (x = 0; x < w; ++x) {
                const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
                const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
                int k, sum = 0;
                for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
                dst[x] = ROUND_POWER_OF_TWO(
                    dst[x] + clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);
                x_q4 += x_step_q4;
            }
            src += src_stride;
            dst += dst_stride;
        }
    }

    template <bool avg> void convolve8_horz_px_impl(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
    {
        const InterpKernel *const filters_x = get_filter_base(filter_x);
        const int x0_q4 = get_filter_offset(filter_x, filters_x);

        (void)filter_y;

        (avg)
            ? core_convolve_avg_horz(src, src_stride, dst, dst_stride, filters_x, x0_q4, 16, w, h)
            : core_convolve_horz(src, src_stride, dst, dst_stride, filters_x, x0_q4, 16, w, h);
    }
    template <bool avg, int w> void convolve8_horz_px_impl(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int h) {
        convolve8_horz_px_impl<avg>(src, src_stride, dst, dst_stride, filter_x, filter_y, w, h);
    }


    static void core_convolve_horz_chromaNV12(const uint8_t *src, int src_stride,
        uint8_t *dst, int dst_stride,
        const InterpKernel *x_filters, int x0_q4,
        int x_step_q4, int w, int h)
    {
        int x, y;
        src -= (SUBPEL_TAPS / 2 - 1)*2;//uv interleave
        for (y = 0; y < h; ++y) {
            int x_q4 = x0_q4;
            for (x = 0; x < w; ++x) {
                const uint8_t *const src_x = &src[(x_q4 >> SUBPEL_BITS)*2];
                const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
                int sum[2] = {0};
                for (int k = 0; k < SUBPEL_TAPS; ++k) {
                    sum[0] += src_x[2*k] * x_filter[k];
                    sum[1] += src_x[2*k+1] * x_filter[k];
                }
                dst[2*x] = clip_pixel(ROUND_POWER_OF_TWO(sum[0], FILTER_BITS));
                dst[2*x+1] = clip_pixel(ROUND_POWER_OF_TWO(sum[1], FILTER_BITS));
                x_q4 += x_step_q4;
            }
            src += src_stride;
            dst += dst_stride;
        }
    }

    static void core_convolve_avg_horz_chromaNV12(const uint8_t *src, int src_stride,
        uint8_t *dst, int dst_stride,
        const InterpKernel *x_filters, int x0_q4,
        int x_step_q4, int w, int h)
    {
        int x, y;
        src -= (SUBPEL_TAPS / 2 - 1)*2;//uv interleaved
        for (y = 0; y < h; ++y) {
            int x_q4 = x0_q4;
            for (x = 0; x < w; ++x) {
                const uint8_t *const src_x = &src[(x_q4 >> SUBPEL_BITS)*2];//uv interleaved
                const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
                int sum[2] = {0};
                for (int k = 0; k < SUBPEL_TAPS; ++k) {
                    sum[0] += src_x[2*k] * x_filter[k];
                    sum[1] += src_x[2*k+1] * x_filter[k];
                }
                dst[2*x] = ROUND_POWER_OF_TWO(
                    dst[2*x] + clip_pixel(ROUND_POWER_OF_TWO(sum[0], FILTER_BITS)), 1);
                dst[2*x+1] = ROUND_POWER_OF_TWO(
                    dst[2*x+1] + clip_pixel(ROUND_POWER_OF_TWO(sum[1], FILTER_BITS)), 1);

                x_q4 += x_step_q4;
            }
            src += src_stride;
            dst += dst_stride;
        }
    }

    template <bool avg> void convolve8_nv12_horz_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
    {
        const InterpKernel *const filters_x = get_filter_base(filter_x);
        const int x0_q4 = get_filter_offset(filter_x, filters_x);

        (void)filter_y;

        (avg)
            ? core_convolve_avg_horz_chromaNV12(src, src_stride, dst, dst_stride, filters_x, x0_q4, 16, w, h)
            : core_convolve_horz_chromaNV12(src, src_stride, dst, dst_stride, filters_x, x0_q4, 16, w, h);
    }

    template <bool avg> void convolve8_both_px_impl(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
    {
        const InterpKernel *const filters_x = get_filter_base(filter_x);
        const int x0_q4 = get_filter_offset(filter_x, filters_x);

        const InterpKernel *const filters_y = get_filter_base(filter_y);
        const int y0_q4 = get_filter_offset(filter_y, filters_y);

        // Note: Fixed size intermediate buffer, temp, places limits on parameters.
        // 2d filtering proceeds in 2 steps:
        //   (1) Interpolate horizontally into an intermediate buffer, temp.
        //   (2) Interpolate temp vertically to derive the sub-pixel result.
        // Deriving the maximum number of rows in the temp buffer (135):
        // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
        // --Largest block size is 64x64 pixels.
        // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
        //   original frame (in 1/16th pixel units).
        // --Must round-up because block may be located at sub-pixel position.
        // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
        // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
        uint8_t temp[135 * 64];
        int intermediate_height = (((h - 1) * 16 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

        assert(w <= 64);
        assert(h <= 64);

        if (avg) {
            convolve8_both_px_impl<false>(src, src_stride, temp, 64, filter_x, filter_y, w, h);
            convolve8_copy_px_impl<true>(temp, 64, dst, dst_stride, NULL, NULL, w, h);
        } else {
            core_convolve_horz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, 64, filters_x, x0_q4, 16, w, intermediate_height);
            core_convolve_vert(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, filters_y, y0_q4, 16, w, h);
        }
    }
    template <bool avg, int w> void convolve8_both_px_impl(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int h) {
        convolve8_both_px_impl<avg>(src, src_stride, dst, dst_stride, filter_x, filter_y, w, h);
    }


    template <bool avg> void convolve8_nv12_both_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
    {
        const InterpKernel *const filters_x = get_filter_base(filter_x);
        const int x0_q4 = get_filter_offset(filter_x, filters_x);

        const InterpKernel *const filters_y = get_filter_base(filter_y);
        const int y0_q4 = get_filter_offset(filter_y, filters_y);

        uint8_t temp[135 * 64];
        int intermediate_height = (((h - 1) * 16 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

        assert(w <= 64);
        assert(h <= 64);

        if (avg) {
            core_convolve_horz_chromaNV12(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, 64, filters_x, x0_q4, 16, w, intermediate_height);
            core_convolve_avg_vert_chromaNV12(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, filters_y, y0_q4, 16, w, h);
        } else {
            core_convolve_horz_chromaNV12(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, 64, filters_x, x0_q4, 16, w, intermediate_height);
            core_convolve_vert_chromaNV12(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, filters_y, y0_q4, 16, w, h);
        }
    }

#if (defined(__GNUC__) && __GNUC__) || defined(__SUNPRO_C)
#define DECLARE_ALIGNED(n, typ, val) typ val __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define DECLARE_ALIGNED(n, typ, val) __declspec(align(n)) typ val
#else
#warning No alignment directives known for this compiler.
#define DECLARE_ALIGNED(n, typ, val) typ val
#endif
}; // namespace details

namespace AV1PP
{
    template <int w> void average_px(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h) {
        details::average_px(src1, pitchSrc1, src2, pitchSrc2, dst, pitchDst, w, h);
    }
    template void average_px<4>(const uint8_t*,int,const uint8_t*,int,uint8_t*,int,int);
    template void average_px<8>(const uint8_t*,int,const uint8_t*,int,uint8_t*,int,int);
    template void average_px<16>(const uint8_t*,int,const uint8_t*,int,uint8_t*,int,int);
    template void average_px<32>(const uint8_t*,int,const uint8_t*,int,uint8_t*,int,int);
    template void average_px<64>(const uint8_t*,int,const uint8_t*,int,uint8_t*,int,int);

    template <int w> void average_pitch64_px(const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h) {
        details::average_px(src1, 64, src2, 64, dst, 64, w, h);
    }
    template void average_pitch64_px<4>(const uint8_t*,const uint8_t*,uint8_t*,int);
    template void average_pitch64_px<8>(const uint8_t*,const uint8_t*,uint8_t*,int);
    template void average_pitch64_px<16>(const uint8_t*,const uint8_t*,uint8_t*,int);
    template void average_pitch64_px<32>(const uint8_t*,const uint8_t*,uint8_t*,int);
    template void average_pitch64_px<64>(const uint8_t*,const uint8_t*,uint8_t*,int);

    template <int w, int horz, int vert, int avg> void interp_px(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        if (horz && vert) details::convolve8_both_px_impl<avg, w>(src, pitchSrc, dst, pitchDst, fx, fy, h);
        else if (horz)    details::convolve8_horz_px_impl<avg, w>(src, pitchSrc, dst, pitchDst, fx, fy, h);
        else if (vert)    details::convolve8_vert_px_impl<avg, w>(src, pitchSrc, dst, pitchDst, fx, fy, h);
        else              details::convolve8_copy_px_impl<avg, w>(src, pitchSrc, dst, pitchDst, fx, fy, h);
    }
    template void interp_px<4, 0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<4, 0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<4, 0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<4, 0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<4, 1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<4, 1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<4, 1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<4, 1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<8, 0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<8, 0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<8, 0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<8, 0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<8, 1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<8, 1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<8, 1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<8, 1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<16,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<16,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<16,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<16,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<16,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<16,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<16,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<16,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<32,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<32,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<32,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<32,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<32,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<32,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<32,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<32,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<64,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<64,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<64,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<64,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<64,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<64,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<64,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<64,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<96,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<96,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<96,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<96,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<96,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<96,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<96,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_px<96,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);

    template <int w, int horz, int vert, int avg> void interp_pitch64_px(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h) {
        if (horz && vert) details::convolve8_both_px_impl<avg, w>(src, pitchSrc, dst, 64, fx, fy, h);
        else if (horz)    details::convolve8_horz_px_impl<avg, w>(src, pitchSrc, dst, 64, fx, fy, h);
        else if (vert)    details::convolve8_vert_px_impl<avg, w>(src, pitchSrc, dst, 64, fx, fy, h);
        else              details::convolve8_copy_px_impl<avg, w>(src, pitchSrc, dst, 64, fx, fy, h);
    }
    template void interp_pitch64_px<4, 0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<4, 0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<4, 0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<4, 0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<4, 1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<4, 1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<4, 1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<4, 1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<8, 0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<8, 0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<8, 0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<8, 0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<8, 1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<8, 1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<8, 1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<8, 1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<16,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<16,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<16,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<16,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<16,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<16,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<16,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<16,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<32,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<32,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<32,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<32,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<32,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<32,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<32,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<32,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<64,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<64,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<64,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<64,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<64,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<64,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<64,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<64,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<96,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<96,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<96,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<96,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<96,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<96,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<96,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_px<96,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

    template <int w, int horz, int vert, int avg> void interp_nv12_px(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        if (horz && vert) details::convolve8_nv12_both_px<avg>(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
        else if (horz)    details::convolve8_nv12_horz_px<avg>(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
        else if (vert)    details::convolve8_nv12_vert_px<avg>(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
        else              details::convolve8_nv12_copy_px<avg>(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
    }
    template void interp_nv12_px<4, 0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<4, 0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<4, 0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<4, 0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<4, 1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<4, 1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<4, 1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<4, 1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<8, 0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<8, 0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<8, 0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<8, 0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<8, 1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<8, 1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<8, 1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<8, 1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<16,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<16,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<16,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<16,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<16,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<16,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<16,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<16,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<32,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<32,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<32,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<32,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<32,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<32,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<32,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<32,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<64,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<64,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<64,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<64,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<64,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<64,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<64,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_px<64,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);

    template <int w, int horz, int vert, int avg> void interp_nv12_pitch64_px(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h) {
        if (horz && vert) details::convolve8_nv12_both_px<avg>(src, pitchSrc, dst, 64, fx, fy, w, h);
        else if (horz)    details::convolve8_nv12_horz_px<avg>(src, pitchSrc, dst, 64, fx, fy, w, h);
        else if (vert)    details::convolve8_nv12_vert_px<avg>(src, pitchSrc, dst, 64, fx, fy, w, h);
        else              details::convolve8_nv12_copy_px<avg>(src, pitchSrc, dst, 64, fx, fy, w, h);
    }
    template void interp_nv12_pitch64_px<4, 0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<4, 0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<4, 0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<4, 0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<4, 1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<4, 1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<4, 1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<4, 1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<8, 0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<8, 0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<8, 0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<8, 0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<8, 1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<8, 1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<8, 1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<8, 1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<16,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<16,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<16,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<16,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<16,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<16,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<16,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<16,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<32,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<32,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<32,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<32,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<32,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<32,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<32,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<32,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<64,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<64,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<64,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<64,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<64,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<64,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<64,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_px<64,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

    template <int horz, int vert, int avg> void interp_flex_px(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h) {
        if (horz && vert) details::convolve8_both_px_impl<avg>(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
        else if (horz)    details::convolve8_horz_px_impl<avg>(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
        else if (vert)    details::convolve8_vert_px_impl<avg>(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
        else              details::convolve8_copy_px_impl<avg>(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
    }
    template void interp_flex_px<0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h);
    template void interp_flex_px<0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h);
    template void interp_flex_px<0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h);
    template void interp_flex_px<0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h);
    template void interp_flex_px<1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h);
    template void interp_flex_px<1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h);
    template void interp_flex_px<1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h);
    template void interp_flex_px<1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h);

    //========================================================================================================================================================

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

    void interp(const uint16_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h, int round1)
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

    void interp(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h)
    {
        const int pitchTmp = w;
        int16_t tmp[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        interp(src, pitchSrc, tmp, w, fx, fy, w, h, ROUND_STAGE1);

        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                dst[y * pitchDst + x] = clip_pixel10(tmp[y * pitchTmp + x]);
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

    void interp(const uint16_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h)
    {
        const int pitchTmp = w;
        int16_t tmp[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        interp(src, pitchSrc, tmp, w, fx, fy, w, h, COMPOUND_ROUND1_BITS);

        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                dst[y * pitchDst + x] = clip_pixel10(round_power_of_two(ref0[y * pitchRef0 + x] + tmp[y * pitchTmp + x], OFFSET_BITS + 1));
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

namespace AV1PP
{
    // single ref, luma
    template <int w, int horz, int vert>
    void interp_av1_px(const uint8_t *src, int pitchSrc, uint8_t*dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
    }

    template <int w, int horz, int vert>
    void interp_av1_px(const uint16_t *src, int pitchSrc, uint16_t*dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
    }

    // first ref (compound), luma
    template <int w, int horz, int vert>
    void interp_av1_px(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp(src, pitchSrc, dst, pitchDst, fx, fy, w, h, COMPOUND_ROUND1_BITS);
    }

    template <int w, int horz, int vert>
    void interp_av1_px(const uint16_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp(src, pitchSrc, dst, pitchDst, fx, fy, w, h, COMPOUND_ROUND1_BITS);
    }

    // second ref (compound), luma
    template <int w, int horz, int vert>
    void interp_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, w, h);
    }

    template <int w, int horz, int vert>
    void interp_av1_px(const uint16_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, w, h);
    }



    // single ref, luma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_pitch64_av1_px(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_px<w, horz, vert>(src, pitchSrc, dst, IMPLIED_PITCH, fx, fy, h);
    }
    template <int w, int horz, int vert>
    void interp_pitch64_av1_px(const uint16_t *src, int pitchSrc, uint16_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_px<w, horz, vert>(src, pitchSrc, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // first ref (compound), luma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_pitch64_av1_px(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_px<w, horz, vert>(src, pitchSrc, dst, IMPLIED_PITCH, fx, fy, h);
    }

    template <int w, int horz, int vert>
    void interp_pitch64_av1_px(const uint16_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_px<w, horz, vert>(src, pitchSrc, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // second ref (compound), luma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_pitch64_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_px<w, horz, vert>(src, pitchSrc, ref0, IMPLIED_PITCH, dst, IMPLIED_PITCH, fx, fy, h);
    }

    template <int w, int horz, int vert>
    void interp_pitch64_av1_px(const uint16_t *src, int pitchSrc, const int16_t *ref0, uint16_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_px<w, horz, vert>(src, pitchSrc, ref0, IMPLIED_PITCH, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // single ref, chroma
    template <int w, int horz, int vert, typename PixType>
    void interp_nv12_av1_px(const PixType *src, int pitchSrc, PixType *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        PixType bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        PixType bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        PixType dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        PixType dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
        const int32_t extW = w + SUBPEL_TAPS - 1;
        const int32_t extH = h + SUBPEL_TAPS - 1;
        const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
        const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

        PixType *srcU = bufSrcU;
        PixType *srcV = bufSrcV;
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

    template <int w, int horz, int vert>
    void interp_nv12_av1_px(const uint16_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        uint16_t bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        uint16_t bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        int16_t dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        int16_t dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
        const int32_t extW = w + SUBPEL_TAPS - 1;
        const int32_t extH = h + SUBPEL_TAPS - 1;
        const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
        const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

        uint16_t *srcU = bufSrcU;
        uint16_t *srcV = bufSrcV;
        src -= shiftH * pitchSrc + shiftW * 2;
        nv12_to_yv12(src, pitchSrc, srcU, pitchSrcYv12, srcV, pitchSrcYv12, extW, extH);

        srcU += shiftH * pitchSrcYv12 + shiftW;
        srcV += shiftH * pitchSrcYv12 + shiftW;
        interp_av1_px<w, horz, vert>(srcU, pitchSrcYv12, dstU, w, fx, fy, h);
        interp_av1_px<w, horz, vert>(srcV, pitchSrcYv12, dstV, w, fx, fy, h);

        yv12_to_nv12(dstU, w, dstV, w, dst, pitchDst, w, h);
    }

        // first ref (compound), chroma
    /*template <int w, int horz, int vert>
    void interp_nv12_av1_px(const uint16_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        uint16_t bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        uint16_t bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        int16_t dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        int16_t dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
        const int32_t extW = w + SUBPEL_TAPS - 1;
        const int32_t extH = h + SUBPEL_TAPS - 1;
        const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
        const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

        uint16_t *srcU = bufSrcU;
        uint16_t *srcV = bufSrcV;
        src -= shiftH * pitchSrc + shiftW * 2;
        nv12_to_yv12(src, pitchSrc, srcU, pitchSrcYv12, srcV, pitchSrcYv12, extW, extH);

        srcU += shiftH * pitchSrcYv12 + shiftW;
        srcV += shiftH * pitchSrcYv12 + shiftW;
        interp_av1_px<w, horz, vert>(srcU, pitchSrcYv12, dstU, w, fx, fy, h);
        interp_av1_px<w, horz, vert>(srcV, pitchSrcYv12, dstV, w, fx, fy, h);

        yv12_to_nv12(dstU, w, dstV, w, dst, pitchDst, w, h);
    }*/

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

    template <int w, int horz, int vert>
    void interp_nv12_av1_px(const uint16_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        uint16_t bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        uint16_t bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        int16_t ref0U[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        int16_t ref0V[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        uint16_t dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        uint16_t dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
        const int32_t extW = w + SUBPEL_TAPS - 1;
        const int32_t extH = h + SUBPEL_TAPS - 1;
        const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
        const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

        uint16_t *srcU = bufSrcU;
        uint16_t *srcV = bufSrcV;
        src -= shiftH * pitchSrc + shiftW * 2;
        nv12_to_yv12(src, pitchSrc, srcU, pitchSrcYv12, srcV, pitchSrcYv12, extW, extH);
        nv12_to_yv12(ref0, pitchRef0, ref0U, w, ref0V, w, w, h);

        srcU += shiftH * pitchSrcYv12 + shiftW;
        srcV += shiftH * pitchSrcYv12 + shiftW;
        interp_av1_px<w, horz, vert>(srcU, pitchSrcYv12, ref0U, w, dstU, w, fx, fy, h);
        interp_av1_px<w, horz, vert>(srcV, pitchSrcYv12, ref0V, w, dstV, w, fx, fy, h);

        yv12_to_nv12(dstU, w, dstV, w, dst, pitchDst, w, h);
    }

    // second ref (compound), chroma
    /*template <int w, int horz, int vert>
    void interp_nv12_av1_px(const uint16_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        uint16_t bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        uint16_t bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
        int16_t ref0U[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        int16_t ref0V[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        uint8_t dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
        uint8_t dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

        const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
        const int32_t extW = w + SUBPEL_TAPS - 1;
        const int32_t extH = h + SUBPEL_TAPS - 1;
        const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
        const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

        uint16_t *srcU = bufSrcU;
        uint16_t *srcV = bufSrcV;
        src -= shiftH * pitchSrc + shiftW * 2;
        nv12_to_yv12(src, pitchSrc, srcU, pitchSrcYv12, srcV, pitchSrcYv12, extW, extH);
        nv12_to_yv12(ref0, pitchRef0, ref0U, w, ref0V, w, w, h);

        srcU += shiftH * pitchSrcYv12 + shiftW;
        srcV += shiftH * pitchSrcYv12 + shiftW;
        interp_av1_px<w, horz, vert>(srcU, pitchSrcYv12, ref0U, w, dstU, w, fx, fy, h);
        interp_av1_px<w, horz, vert>(srcV, pitchSrcYv12, ref0V, w, dstV, w, fx, fy, h);

        yv12_to_nv12(dstU, w, dstV, w, dst, pitchDst, w, h);
    }*/

    // single ref, chroma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_px(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        const int pitchDstImplied = 64;
        interp_nv12_av1_px<w, horz, vert>(src, pitchSrc, dst, pitchDstImplied, fx, fy, h);
    }

    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_px(const uint16_t *src, int pitchSrc, uint16_t *dst, const int16_t *fx, const int16_t *fy, int h)
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

    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_px(const uint16_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        const int pitchDstImplied = 64;
        interp_nv12_av1_px<w, horz, vert>(src, pitchSrc, dst, pitchDstImplied, fx, fy, h);
    }

    //template <int w, int horz, int vert>
    //void interp_nv12_pitch64_av1_px(const uint16_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h)
    //{
    //    const int pitchDstImplied = 64;
    //    interp_nv12_av1_px<w, horz, vert>(src, pitchSrc, dst, pitchDstImplied, fx, fy, h);
    //}

    // second ref (compound), chroma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        const int pitchRef0Implied = 64;
        const int pitchDstImplied = 64;
        interp_nv12_av1_px<w, horz, vert>(src, pitchSrc, ref0, pitchRef0Implied, dst, pitchDstImplied, fx, fy, h);
    }

    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_px(const uint16_t *src, int pitchSrc, const int16_t *ref0, uint16_t *dst, const int16_t *fx, const int16_t *fy, int h)
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

    typedef void(*interp_av1_single_ref_hbd_fptr_t)(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void(*interp_av1_first_ref_hbd_fptr_t)(const uint16_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void(*interp_av1_second_ref_hbd_fptr_t)(const uint16_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);

    typedef void (* interp_pitch64_av1_single_ref_fptr_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_pitch64_av1_first_ref_fptr_t)(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_pitch64_av1_second_ref_fptr_t)(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

    typedef void(*interp_pitch64_av1_single_ref_hbd_fptr_t)(const uint16_t *src, int pitchSrc, uint16_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void(*interp_pitch64_av1_first_ref_hbd_fptr_t)(const uint16_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void(*interp_pitch64_av1_second_ref_hbd_fptr_t)(const uint16_t *src, int pitchSrc, const int16_t *ref0, uint16_t *dst, const int16_t *fx, const int16_t *fy, int h);

#define INSTATIATE(FUNC_NAME)                                                                         \
    { { FUNC_NAME< 4, 0, 0>, FUNC_NAME< 4, 0, 1> }, { FUNC_NAME< 4, 1, 0>, FUNC_NAME< 4, 1, 1> } },   \
    { { FUNC_NAME< 8, 0, 0>, FUNC_NAME< 8, 0, 1> }, { FUNC_NAME< 8, 1, 0>, FUNC_NAME< 8, 1, 1> } },   \
    { { FUNC_NAME<16, 0, 0>, FUNC_NAME<16, 0, 1> }, { FUNC_NAME<16, 1, 0>, FUNC_NAME<16, 1, 1> } },   \
    { { FUNC_NAME<32, 0, 0>, FUNC_NAME<32, 0, 1> }, { FUNC_NAME<32, 1, 0>, FUNC_NAME<32, 1, 1> } },   \
    { { FUNC_NAME<64, 0, 0>, FUNC_NAME<64, 0, 1> }, { FUNC_NAME<64, 1, 0>, FUNC_NAME<64, 1, 1> } }


 #define INSTATIATE_16(FUNC_NAME)                                                                        \
    { { FUNC_NAME< 4, 0, 0, uint16_t>, FUNC_NAME< 4, 0, 1, uint16_t> }, { FUNC_NAME< 4, 1, 0, uint16_t>, FUNC_NAME< 4, 1, 1, uint16_t> } },   \
    { { FUNC_NAME< 8, 0, 0, uint16_t>, FUNC_NAME< 8, 0, 1, uint16_t> }, { FUNC_NAME< 8, 1, 0, uint16_t>, FUNC_NAME< 8, 1, 1, uint16_t> } },   \
    { { FUNC_NAME<16, 0, 0, uint16_t>, FUNC_NAME<16, 0, 1, uint16_t> }, { FUNC_NAME<16, 1, 0, uint16_t>, FUNC_NAME<16, 1, 1, uint16_t> } },   \
    { { FUNC_NAME<32, 0, 0, uint16_t>, FUNC_NAME<32, 0, 1, uint16_t> }, { FUNC_NAME<32, 1, 0, uint16_t>, FUNC_NAME<32, 1, 1, uint16_t> } },   \
    { { FUNC_NAME<64, 0, 0, uint16_t>, FUNC_NAME<64, 0, 1, uint16_t> }, { FUNC_NAME<64, 1, 0, uint16_t>, FUNC_NAME<64, 1, 1, uint16_t> } }


#define INSTATIATE6(FUNC_NAME)                                                                     \
    INSTATIATE(FUNC_NAME),                                                                         \
    { { FUNC_NAME<96, 0, 0>, FUNC_NAME<96, 0, 1> }, { FUNC_NAME<96, 1, 0>, FUNC_NAME<96, 1, 1> } }

    interp_av1_single_ref_fptr_t interp_av1_single_ref_fptr_arr_px[6][2][2] = { INSTATIATE6(interp_av1_px) };
    interp_av1_first_ref_fptr_t  interp_av1_first_ref_fptr_arr_px[5][2][2]  = { INSTATIATE(interp_av1_px) };
    interp_av1_second_ref_fptr_t interp_av1_second_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_av1_px) };
    // hbd
    interp_av1_single_ref_hbd_fptr_t interp_av1_single_ref_hbd_fptr_arr_px[6][2][2] = { INSTATIATE6(interp_av1_px) };
    interp_av1_first_ref_hbd_fptr_t  interp_av1_first_ref_hbd_fptr_arr_px[5][2][2] = { INSTATIATE(interp_av1_px) };
    interp_av1_second_ref_hbd_fptr_t interp_av1_second_ref_hbd_fptr_arr_px[5][2][2] = { INSTATIATE(interp_av1_px) };

    interp_av1_single_ref_fptr_t interp_nv12_av1_single_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_nv12_av1_px) };
    interp_av1_first_ref_fptr_t  interp_nv12_av1_first_ref_fptr_arr_px[5][2][2]  = { INSTATIATE(interp_nv12_av1_px) };
    interp_av1_second_ref_fptr_t interp_nv12_av1_second_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_nv12_av1_px) };

    interp_pitch64_av1_single_ref_fptr_t interp_pitch64_av1_single_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_pitch64_av1_px) };
    interp_pitch64_av1_first_ref_fptr_t  interp_pitch64_av1_first_ref_fptr_arr_px[5][2][2]  = { INSTATIATE(interp_pitch64_av1_px) };
    interp_pitch64_av1_second_ref_fptr_t interp_pitch64_av1_second_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_pitch64_av1_px) };
    // hbd
    interp_pitch64_av1_single_ref_hbd_fptr_t interp_pitch64_av1_single_ref_hbd_fptr_arr_px[5][2][2] = { INSTATIATE(interp_pitch64_av1_px) };
    interp_pitch64_av1_first_ref_hbd_fptr_t  interp_pitch64_av1_first_ref_hbd_fptr_arr_px[5][2][2] = { INSTATIATE(interp_pitch64_av1_px) };
    interp_pitch64_av1_second_ref_hbd_fptr_t interp_pitch64_av1_second_ref_hbd_fptr_arr_px[5][2][2] = { INSTATIATE(interp_pitch64_av1_px) };

    interp_pitch64_av1_single_ref_fptr_t interp_nv12_pitch64_av1_single_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_nv12_pitch64_av1_px) };
    interp_pitch64_av1_first_ref_fptr_t  interp_nv12_pitch64_av1_first_ref_fptr_arr_px[5][2][2]  = { INSTATIATE(interp_nv12_pitch64_av1_px) };
    interp_pitch64_av1_second_ref_fptr_t interp_nv12_pitch64_av1_second_ref_fptr_arr_px[5][2][2] = { INSTATIATE(interp_nv12_pitch64_av1_px) };
    // hbd
    interp_pitch64_av1_single_ref_hbd_fptr_t interp_nv12_pitch64_av1_single_ref_hbd_fptr_arr_px[5][2][2] = { INSTATIATE(interp_nv12_pitch64_av1_px) };
    interp_pitch64_av1_first_ref_hbd_fptr_t  interp_nv12_pitch64_av1_first_ref_hbd_fptr_arr_px[5][2][2]  = { INSTATIATE(interp_nv12_pitch64_av1_px) };
    interp_pitch64_av1_second_ref_hbd_fptr_t interp_nv12_pitch64_av1_second_ref_hbd_fptr_arr_px[5][2][2] = { INSTATIATE(interp_nv12_pitch64_av1_px) };

}; // namespace AV1PP
