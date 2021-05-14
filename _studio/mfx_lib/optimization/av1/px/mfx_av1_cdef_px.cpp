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
#include "stdlib.h"
#ifdef _WIN32
#include "intrin.h"
#endif
#include <immintrin.h>

#include <cstdint>
#include <algorithm>

#define ALIGN_POWER_OF_TWO(value, n) (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

/* We only need to buffer three horizontal pixels too, but let's align to
   16 bytes (8 x 16 bits) to make vectorization easier. */
#define CDEF_HBORDER (8)
#define CDEF_BLOCKSIZE 64
#define CDEF_BSTRIDE ALIGN_POWER_OF_TWO(CDEF_BLOCKSIZE + 2 * CDEF_HBORDER, 3)
#define CDEF_VERY_LARGE (30000)

namespace AV1PP
{
    static const int cdef_directions[8][2] = {
        { -1 * CDEF_BSTRIDE + 1, -2 * CDEF_BSTRIDE + 2 },
        {  0 * CDEF_BSTRIDE + 1, -1 * CDEF_BSTRIDE + 2 },
        {  0 * CDEF_BSTRIDE + 1,  0 * CDEF_BSTRIDE + 2 },
        {  0 * CDEF_BSTRIDE + 1,  1 * CDEF_BSTRIDE + 2 },
        {  1 * CDEF_BSTRIDE + 1,  2 * CDEF_BSTRIDE + 2 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE + 1 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE + 0 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE - 1 }
    };
    static const int cdef_directions_nv12[8][2] = {
        { -1 * CDEF_BSTRIDE + 2, -2 * CDEF_BSTRIDE + 4 },
        {  0 * CDEF_BSTRIDE + 2, -1 * CDEF_BSTRIDE + 4 },
        {  0 * CDEF_BSTRIDE + 2,  0 * CDEF_BSTRIDE + 4 },
        {  0 * CDEF_BSTRIDE + 2,  1 * CDEF_BSTRIDE + 4 },
        {  1 * CDEF_BSTRIDE + 2,  2 * CDEF_BSTRIDE + 4 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE + 2 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE + 0 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE - 2 }
    };
    static const int cdef_pri_taps[2][2] = { { 4, 2 }, { 3, 3 } };
    static const int cdef_sec_taps[2][2] = { { 2, 1 }, { 2, 1 } };


    /* Detect direction. 0 means 45-degree up-right, 2 is horizontal, and so on.
       The search minimizes the weighted variance along all the lines in a
       particular direction, i.e. the squared error between the input and a
       "predicted" block where each pixel is replaced by the average along a line
       in a particular direction. Since each direction have the same sum(x^2) term,
       that term is never computed. See Section 2, step 2, of:
       http://jmvalin.ca/notes/intra_paint.pdf */
    int cdef_find_dir_px(const uint16_t *img, int stride, int *var, int coeff_shift)
    {
        int i;
        int cost[8] = { 0 };
        int partial[8][15] = { { 0 } };
        int best_cost = 0;
        int best_dir = 0;
        /* Instead of dividing by n between 2 and 8, we multiply by 3*5*7*8/n.
        The output is then 840 times larger, but we don't care for finding
        the max. */
        static const int div_table[] = { 0, 840, 420, 280, 210, 168, 140, 120, 105 };
        for (i = 0; i < 8; i++) {
            int j;
            for (j = 0; j < 8; j++) {
                int x;
                /* We subtract 128 here to reduce the maximum range of the squared
                partial sums. */
                x = (img[i * stride + j] >> coeff_shift) - 128;
                partial[0][i + j] += x;
                partial[1][i + j / 2] += x;
                partial[2][i] += x;
                partial[3][3 + i - j / 2] += x;
                partial[4][7 + i - j] += x;
                partial[5][3 - i / 2 + j] += x;
                partial[6][j] += x;
                partial[7][i / 2 + j] += x;
            }
        }
        for (i = 0; i < 8; i++) {
            cost[2] += partial[2][i] * partial[2][i];
            cost[6] += partial[6][i] * partial[6][i];
        }
        cost[2] *= div_table[8];
        cost[6] *= div_table[8];
        for (i = 0; i < 7; i++) {
            cost[0] += (partial[0][i] * partial[0][i] +
                partial[0][14 - i] * partial[0][14 - i]) *
                div_table[i + 1];
            cost[4] += (partial[4][i] * partial[4][i] +
                partial[4][14 - i] * partial[4][14 - i]) *
                div_table[i + 1];
        }
        cost[0] += partial[0][7] * partial[0][7] * div_table[8];
        cost[4] += partial[4][7] * partial[4][7] * div_table[8];
        for (i = 1; i < 8; i += 2) {
            int j;
            for (j = 0; j < 4 + 1; j++) {
                cost[i] += partial[i][3 + j] * partial[i][3 + j];
            }
            cost[i] *= div_table[8];
            for (j = 0; j < 4 - 1; j++) {
                cost[i] += (partial[i][j] * partial[i][j] +
                    partial[i][10 - j] * partial[i][10 - j]) *
                    div_table[2 * j + 2];
            }
        }
        for (i = 0; i < 8; i++) {
            if (cost[i] > best_cost) {
                best_cost = cost[i];
                best_dir = i;
            }
        }
        /* Difference between the optimal variance and the variance along the
        orthogonal direction. Again, the sum(x^2) terms cancel out. */
        *var = best_cost - cost[(best_dir + 4) & 7];
        /* We'd normally divide by 840, but dividing by 1024 is close enough
        for what we're going to do with this. */
        *var >>= 10;
        return best_dir;
    }


    static inline int get_msb(unsigned int n) {
#ifdef _WIN32
        unsigned long first_set_bit = 0;
#else
        unsigned int first_set_bit = 0;
#endif
        assert(n != 0);
        _BitScanReverse(&first_set_bit, n);
        return first_set_bit;
    }

    static int sign(int i) { return i < 0 ? -1 : 1; }

    static int constrain(int diff, int threshold, int damping) {
        if (!threshold) return 0;

        const int shift = std::max(0, damping - get_msb(threshold));
        return sign(diff) *
            std::min(abs(diff), std::max(0, threshold - (abs(diff) >> shift)));
    }

    static int clamp(int value, int low, int high) {
        return value < low ? low : (value > high ? high : value);
    }

    template <typename TDst>
    void cdef_filter_block_4x4_px(TDst *dst, int dstride, const uint16_t *in, int pri_strength,
                                  int sec_strength, int dir, int pri_damping, int sec_damping)
    {
        int i, j, k;
        const int s = CDEF_BSTRIDE;
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *sec_taps = cdef_sec_taps[pri_strength & 1];
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                short sum = 0;
                short y;
                short x = in[i * s + j];
                int max = x;
                int min = x;
                for (k = 0; k < 2; k++)
                {
                    short p0 = in[i * s + j + cdef_directions[dir][k]];
                    short p1 = in[i * s + j - cdef_directions[dir][k]];
                    sum += pri_taps[k] * constrain(p0 - x, pri_strength, pri_damping);
                    sum += pri_taps[k] * constrain(p1 - x, pri_strength, pri_damping);
                    if (p0 != CDEF_VERY_LARGE) max = std::max((int)p0, max);
                    if (p1 != CDEF_VERY_LARGE) max = std::max((int)p1, max);
                    min = std::min((int)p0, min);
                    min = std::min((int)p1, min);
                    short s0 = in[i * s + j + cdef_directions[(dir + 2) & 7][k]];
                    short s1 = in[i * s + j - cdef_directions[(dir + 2) & 7][k]];
                    short s2 = in[i * s + j + cdef_directions[(dir + 6) & 7][k]];
                    short s3 = in[i * s + j - cdef_directions[(dir + 6) & 7][k]];
                    if (s0 != CDEF_VERY_LARGE) max = std::max((int)s0, max);
                    if (s1 != CDEF_VERY_LARGE) max = std::max((int)s1, max);
                    if (s2 != CDEF_VERY_LARGE) max = std::max((int)s2, max);
                    if (s3 != CDEF_VERY_LARGE) max = std::max((int)s3, max);
                    min = std::min((int)s0, min);
                    min = std::min((int)s1, min);
                    min = std::min((int)s2, min);
                    min = std::min((int)s3, min);
                    sum += sec_taps[k] * constrain(s0 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s1 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s2 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s3 - x, sec_strength, sec_damping);
                }
                y = clamp((short)x + ((8 + sum - (sum < 0)) >> 4), min, max);
                dst[i * dstride + j] = (uint8_t)y;
            }
        }
    }
    template void cdef_filter_block_4x4_px<uint8_t> (uint8_t*, int,const uint16_t*,int,int,int,int,int);
    template void cdef_filter_block_4x4_px<uint16_t>(uint16_t*,int,const uint16_t*,int,int,int,int,int);

    template <typename TDst>
    void cdef_filter_block_4x4_nv12_px(TDst *dst, int dstride, const uint16_t *in, int pri_strength,
                                       int sec_strength, int dir, int pri_damping, int sec_damping)
    {
        int i, j, k, p;
        const int s = CDEF_BSTRIDE;
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *sec_taps = cdef_sec_taps[pri_strength & 1];
        for (i = 0; i < 4; i++) {
            for (p = 0; p < 2; p++) {
                for (j = p; j < 8; j += 2) {
                    short sum = 0;
                    short y;
                    short x = in[i * s + j];
                    int max = x;
                    int min = x;
                    for (k = 0; k < 2; k++)
                    {
                        short p0 = in[i * s + j + cdef_directions_nv12[dir][k]];
                        short p1 = in[i * s + j - cdef_directions_nv12[dir][k]];
                        sum += pri_taps[k] * constrain(p0 - x, pri_strength, pri_damping);
                        sum += pri_taps[k] * constrain(p1 - x, pri_strength, pri_damping);
                        if (p0 != CDEF_VERY_LARGE) max = std::max((int)p0, max);
                        if (p1 != CDEF_VERY_LARGE) max = std::max((int)p1, max);
                        min = std::min((int)p0, min);
                        min = std::min((int)p1, min);
                        short s0 = in[i * s + j + cdef_directions_nv12[(dir + 2) & 7][k]];
                        short s1 = in[i * s + j - cdef_directions_nv12[(dir + 2) & 7][k]];
                        short s2 = in[i * s + j + cdef_directions_nv12[(dir + 6) & 7][k]];
                        short s3 = in[i * s + j - cdef_directions_nv12[(dir + 6) & 7][k]];
                        if (s0 != CDEF_VERY_LARGE) max = std::max((int)s0, max);
                        if (s1 != CDEF_VERY_LARGE) max = std::max((int)s1, max);
                        if (s2 != CDEF_VERY_LARGE) max = std::max((int)s2, max);
                        if (s3 != CDEF_VERY_LARGE) max = std::max((int)s3, max);
                        min = std::min((int)s0, min);
                        min = std::min((int)s1, min);
                        min = std::min((int)s2, min);
                        min = std::min((int)s3, min);
                        sum += sec_taps[k] * constrain(s0 - x, sec_strength, sec_damping);
                        sum += sec_taps[k] * constrain(s1 - x, sec_strength, sec_damping);
                        sum += sec_taps[k] * constrain(s2 - x, sec_strength, sec_damping);
                        sum += sec_taps[k] * constrain(s3 - x, sec_strength, sec_damping);
                    }
                    y = clamp((short)x + ((8 + sum - (sum < 0)) >> 4), min, max);
                    dst[i * dstride + j] = (uint8_t)y;
                }
            }
        }
    }
    template void cdef_filter_block_4x4_nv12_px<uint8_t> (uint8_t*, int,const uint16_t*,int,int,int,int,int);
    template void cdef_filter_block_4x4_nv12_px<uint16_t>(uint16_t*,int,const uint16_t*,int,int,int,int,int);

    template <typename TDst>
    void cdef_filter_block_8x8_px(TDst *dst, int dstride, const uint16_t *in, int pri_strength,
                                  int sec_strength, int dir, int pri_damping, int sec_damping)
    {
        int i, j, k;
        const int s = CDEF_BSTRIDE;
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *sec_taps = cdef_sec_taps[pri_strength & 1];
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                short sum = 0;
                short y;
                short x = in[i * s + j];
                int max = x;
                int min = x;
                for (k = 0; k < 2; k++)
                {
                    short p0 = in[i * s + j + cdef_directions[dir][k]];
                    short p1 = in[i * s + j - cdef_directions[dir][k]];
                    sum += pri_taps[k] * constrain(p0 - x, pri_strength, pri_damping);
                    sum += pri_taps[k] * constrain(p1 - x, pri_strength, pri_damping);
                    if (p0 != CDEF_VERY_LARGE) max = std::max((int)p0, max);
                    if (p1 != CDEF_VERY_LARGE) max = std::max((int)p1, max);
                    min = std::min((int)p0, min);
                    min = std::min((int)p1, min);
                    short s0 = in[i * s + j + cdef_directions[(dir + 2) & 7][k]];
                    short s1 = in[i * s + j - cdef_directions[(dir + 2) & 7][k]];
                    short s2 = in[i * s + j + cdef_directions[(dir + 6) & 7][k]];
                    short s3 = in[i * s + j - cdef_directions[(dir + 6) & 7][k]];
                    if (s0 != CDEF_VERY_LARGE) max = std::max((int)s0, max);
                    if (s1 != CDEF_VERY_LARGE) max = std::max((int)s1, max);
                    if (s2 != CDEF_VERY_LARGE) max = std::max((int)s2, max);
                    if (s3 != CDEF_VERY_LARGE) max = std::max((int)s3, max);
                    min = std::min((int)s0, min);
                    min = std::min((int)s1, min);
                    min = std::min((int)s2, min);
                    min = std::min((int)s3, min);
                    sum += sec_taps[k] * constrain(s0 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s1 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s2 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s3 - x, sec_strength, sec_damping);
                }
                y = clamp((short)x + ((8 + sum - (sum < 0)) >> 4), min, max);
                dst[i * dstride + j] = (uint8_t)y;
            }
        }
    }
    template void cdef_filter_block_8x8_px<uint8_t> (uint8_t*, int,const uint16_t*,int,int,int,int,int);
    template void cdef_filter_block_8x8_px<uint16_t>(uint16_t*,int,const uint16_t*,int,int,int,int,int);

    void cdef_estimate_block_4x4_px(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
                                    int sec_strength, int dir, int pri_damping, int sec_damping, int *sse)
    {
        int i, j, k;
        int err = 0;
        const int s = CDEF_BSTRIDE;
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *sec_taps = cdef_sec_taps[pri_strength & 1];
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                short sum = 0;
                short y;
                short x = in[i * s + j];
                int max = x;
                int min = x;
                for (k = 0; k < 2; k++)
                {
                    short p0 = in[i * s + j + cdef_directions[dir][k]];
                    short p1 = in[i * s + j - cdef_directions[dir][k]];
                    sum += pri_taps[k] * constrain(p0 - x, pri_strength, pri_damping);
                    sum += pri_taps[k] * constrain(p1 - x, pri_strength, pri_damping);
                    if (p0 != CDEF_VERY_LARGE) max = std::max((int)p0, max);
                    if (p1 != CDEF_VERY_LARGE) max = std::max((int)p1, max);
                    min = std::min((int)p0, min);
                    min = std::min((int)p1, min);
                    short s0 = in[i * s + j + cdef_directions[(dir + 2) & 7][k]];
                    short s1 = in[i * s + j - cdef_directions[(dir + 2) & 7][k]];
                    short s2 = in[i * s + j + cdef_directions[(dir + 6) & 7][k]];
                    short s3 = in[i * s + j - cdef_directions[(dir + 6) & 7][k]];
                    if (s0 != CDEF_VERY_LARGE) max = std::max((int)s0, max);
                    if (s1 != CDEF_VERY_LARGE) max = std::max((int)s1, max);
                    if (s2 != CDEF_VERY_LARGE) max = std::max((int)s2, max);
                    if (s3 != CDEF_VERY_LARGE) max = std::max((int)s3, max);
                    min = std::min((int)s0, min);
                    min = std::min((int)s1, min);
                    min = std::min((int)s2, min);
                    min = std::min((int)s3, min);
                    sum += sec_taps[k] * constrain(s0 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s1 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s2 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s3 - x, sec_strength, sec_damping);
                }
                y = clamp((short)x + ((8 + sum - (sum < 0)) >> 4), min, max);

                    int16_t diff = org[i * ostride + j] - y;
                    err += diff * diff;
            }
        }

        *sse = err;
    }

    template <typename PixType> void cdef_estimate_block_4x4_nv12_px(const PixType *org, int ostride, const uint16_t *in, int pri_strength,
                                         int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, PixType *dst, int dstride)
    {
        dst; dstride;
        const int coeff_shift = sizeof(PixType) == 1 ? 0 : 2;
        int i, j, k, p;
        int err = 0;
        const int s = CDEF_BSTRIDE;
        const int *pri_taps = cdef_pri_taps[(pri_strength >> coeff_shift) & 1];
        const int *sec_taps = cdef_sec_taps[(pri_strength >> coeff_shift) & 1];
        for (i = 0; i < 4; i++) {
            for (p = 0; p < 2; p++) {
                for (j = p; j < 8; j += 2) {
                    short sum = 0;
                    short y;
                    short x = in[i * s + j];
                    int max = x;
                    int min = x;
                    for (k = 0; k < 2; k++)
                    {
                        short p0 = in[i * s + j + cdef_directions_nv12[dir][k]];
                        short p1 = in[i * s + j - cdef_directions_nv12[dir][k]];
                        sum += pri_taps[k] * constrain(p0 - x, pri_strength, pri_damping);
                        sum += pri_taps[k] * constrain(p1 - x, pri_strength, pri_damping);
                        if (p0 != CDEF_VERY_LARGE) max = std::max((int)p0, max);
                        if (p1 != CDEF_VERY_LARGE) max = std::max((int)p1, max);
                        min = std::min((int)p0, min);
                        min = std::min((int)p1, min);
                        short s0 = in[i * s + j + cdef_directions_nv12[(dir + 2) & 7][k]];
                        short s1 = in[i * s + j - cdef_directions_nv12[(dir + 2) & 7][k]];
                        short s2 = in[i * s + j + cdef_directions_nv12[(dir + 6) & 7][k]];
                        short s3 = in[i * s + j - cdef_directions_nv12[(dir + 6) & 7][k]];
                        if (s0 != CDEF_VERY_LARGE) max = std::max((int)s0, max);
                        if (s1 != CDEF_VERY_LARGE) max = std::max((int)s1, max);
                        if (s2 != CDEF_VERY_LARGE) max = std::max((int)s2, max);
                        if (s3 != CDEF_VERY_LARGE) max = std::max((int)s3, max);
                        min = std::min((int)s0, min);
                        min = std::min((int)s1, min);
                        min = std::min((int)s2, min);
                        min = std::min((int)s3, min);
                        sum += sec_taps[k] * constrain(s0 - x, sec_strength, sec_damping);
                        sum += sec_taps[k] * constrain(s1 - x, sec_strength, sec_damping);
                        sum += sec_taps[k] * constrain(s2 - x, sec_strength, sec_damping);
                        sum += sec_taps[k] * constrain(s3 - x, sec_strength, sec_damping);
                    }
                    y = clamp((short)x + ((8 + sum - (sum < 0)) >> 4), min, max);

                    int16_t diff = org[i * ostride + j] - y;
                    err += diff * diff;
                    if (dst)
                        dst[i * dstride + j] = (PixType)y;
                }
            }
        }

        *sse = err;
    }
    template void cdef_estimate_block_4x4_nv12_px<uint8_t>(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
        int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, uint8_t *dst, int dstride);
    template void cdef_estimate_block_4x4_nv12_px<uint16_t>(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength,
        int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, uint16_t *dst, int dstride);


    template <typename PixType> void cdef_estimate_block_4x4_nv12_sec0_px(const PixType *org, int ostride, const uint16_t *in, int pri_strength,
                                              int dir, int pri_damping, int *sse, PixType *dst, int dstride)
    {
        dst;
        dstride;
        const int coeff_shift = sizeof(PixType) == 1 ? 0 : 2;
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength, 0, dir, pri_damping, 3 + coeff_shift, sse, dst, dstride);
    }
    template void cdef_estimate_block_4x4_nv12_sec0_px<uint8_t>(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
        int dir, int pri_damping, int *sse, uint8_t *dst, int dstride);
    template void cdef_estimate_block_4x4_nv12_sec0_px<uint16_t>(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength,
        int dir, int pri_damping, int *sse, uint16_t *dst, int dstride);

    template <typename PixType>
    void cdef_estimate_block_8x8_px(const PixType *org, int ostride, const uint16_t *in, int pri_strength,
                                    int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, PixType* dst, int32_t dstride)
    {
        //dst; dstride;
        int i, j, k;
        int err = 0;
        const int coeff_shift = sizeof(PixType) == 1 ? 0 : 2;
        const int s = CDEF_BSTRIDE;
        const int *pri_taps = cdef_pri_taps[(pri_strength >> coeff_shift) & 1];
        const int *sec_taps = cdef_sec_taps[(pri_strength >> coeff_shift) & 1];
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                short sum = 0;
                short y;
                short x = in[i * s + j];
                int max = x;
                int min = x;
                for (k = 0; k < 2; k++)
                {
                    short p0 = in[i * s + j + cdef_directions[dir][k]];
                    short p1 = in[i * s + j - cdef_directions[dir][k]];
                    sum += pri_taps[k] * constrain(p0 - x, pri_strength, pri_damping);
                    sum += pri_taps[k] * constrain(p1 - x, pri_strength, pri_damping);
                    if (p0 != CDEF_VERY_LARGE) max = std::max((int)p0, max);
                    if (p1 != CDEF_VERY_LARGE) max = std::max((int)p1, max);
                    min = std::min((int)p0, min);
                    min = std::min((int)p1, min);
                    short s0 = in[i * s + j + cdef_directions[(dir + 2) & 7][k]];
                    short s1 = in[i * s + j - cdef_directions[(dir + 2) & 7][k]];
                    short s2 = in[i * s + j + cdef_directions[(dir + 6) & 7][k]];
                    short s3 = in[i * s + j - cdef_directions[(dir + 6) & 7][k]];
                    if (s0 != CDEF_VERY_LARGE) max = std::max((int)s0, max);
                    if (s1 != CDEF_VERY_LARGE) max = std::max((int)s1, max);
                    if (s2 != CDEF_VERY_LARGE) max = std::max((int)s2, max);
                    if (s3 != CDEF_VERY_LARGE) max = std::max((int)s3, max);
                    min = std::min((int)s0, min);
                    min = std::min((int)s1, min);
                    min = std::min((int)s2, min);
                    min = std::min((int)s3, min);
                    sum += sec_taps[k] * constrain(s0 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s1 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s2 - x, sec_strength, sec_damping);
                    sum += sec_taps[k] * constrain(s3 - x, sec_strength, sec_damping);
                }
                y = clamp((short)x + ((8 + sum - (sum < 0)) >> 4), min, max);

                int16_t diff = org[i * ostride + j] - y;
                err += diff * diff;

                if (dst)
                    dst[i * dstride + j] = (PixType)y;
            }
        }
        *sse = err;
    }
    template void cdef_estimate_block_8x8_px<uint8_t>(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
        int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, uint8_t* dst, int32_t dstride);
    template void cdef_estimate_block_8x8_px<uint16_t>(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength,
        int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, uint16_t* dst, int32_t dstride);

    template <typename PixType> void cdef_estimate_block_8x8_sec0_px(const PixType *org, int ostride, const uint16_t *in, int pri_strength,
                                         int dir, int pri_damping, int *sse, PixType *dst, int dstride)
    {
        dst; dstride;
        int coeff_shift = sizeof(PixType) == 1 ? 0 : 2;
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength, 0, dir, pri_damping, 3 + coeff_shift, sse, dst, dstride);
    }
    template void cdef_estimate_block_8x8_sec0_px<uint8_t>(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
        int dir, int pri_damping, int *sse, uint8_t *dst, int dstride);
    template void cdef_estimate_block_8x8_sec0_px<uint16_t>(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength,
        int dir, int pri_damping, int *sse, uint16_t *dst, int dstride);

    void cdef_estimate_block_4x4_pri0_px(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse)
    {
        const int pri_strength = 0;
        const int pri_damping = 3; // not used
        const int dir = 0; // not used
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength, 1, dir, pri_damping, sec_damping, sse + 0);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength, 2, dir, pri_damping, sec_damping, sse + 1);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength, 4, dir, pri_damping, sec_damping, sse + 2);
    }

    void cdef_estimate_block_4x4_nv12_pri0_px(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse)
    {
        const int pri_strength = 0;
        const int pri_damping = 3; // not used
        const int dir = 0; // not used
        uint8_t *dst = nullptr;// not used
        int dstride = 0;// not used
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength, 1, dir, pri_damping, sec_damping, sse + 0, dst, dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength, 2, dir, pri_damping, sec_damping, sse + 1, dst, dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength, 4, dir, pri_damping, sec_damping, sse + 2, dst, dstride);
    }

    void cdef_estimate_block_8x8_pri0_px(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse)
    {
        const int pri_strength = 0;
        const int pri_damping = 3; // not used
        const int dir = 0; // not used
        uint8_t *dst = nullptr;// not used
        int dstride = 0;// not used
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength, 2, dir, pri_damping, sec_damping, sse + 1, dst, dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength, 1, dir, pri_damping, sec_damping, sse + 0, dst, dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength, 4, dir, pri_damping, sec_damping, sse + 2, dst, dstride);
    }

    void cdef_estimate_block_4x4_all_sec_px(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
                                            int dir, int pri_damping, int sec_damping, int *sse)
    {
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength, 0, dir, pri_damping, sec_damping, sse + 0);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength, 1, dir, pri_damping, sec_damping, sse + 1);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength, 2, dir, pri_damping, sec_damping, sse + 2);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength, 4, dir, pri_damping, sec_damping, sse + 3);
    }

    template <typename PixType> void cdef_estimate_block_4x4_nv12_all_sec_px(const PixType *org, int ostride, const uint16_t *in, int pri_strength,
                                                 int dir, int pri_damping, int sec_damping, int *sse, PixType **dst, int dstride)
    {
        const int shift = sizeof(PixType) == 1 ? 0 : 2;
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength, 0 << shift, dir, pri_damping, sec_damping, sse + 0, dst[0], dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength, 1 << shift, dir, pri_damping, sec_damping, sse + 1, dst[1], dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength, 2 << shift, dir, pri_damping, sec_damping, sse + 2, dst[2], dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength, 4 << shift, dir, pri_damping, sec_damping, sse + 3, dst[3], dstride);
    }
    template void cdef_estimate_block_4x4_nv12_all_sec_px<uint8_t>(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
        int dir, int pri_damping, int sec_damping, int *sse, uint8_t **dst, int dstride);
    template void cdef_estimate_block_4x4_nv12_all_sec_px<uint16_t>(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength,
        int dir, int pri_damping, int sec_damping, int *sse, uint16_t **dst, int dstride);

    template <typename PixType> void cdef_estimate_block_8x8_all_sec_px(const PixType *org, int ostride, const uint16_t *in, int pri_strength,
                                            int dir, int pri_damping, int sec_damping, int *sse, PixType **dst, int dstride)
    {
        const int shift = sizeof(PixType) == 1 ? 0 : 2;
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength, 0 << shift, dir, pri_damping, sec_damping, sse + 0, dst[0], dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength, 1 << shift, dir, pri_damping, sec_damping, sse + 1, dst[1], dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength, 2 << shift, dir, pri_damping, sec_damping, sse + 2, dst[2], dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength, 4 << shift, dir, pri_damping, sec_damping, sse + 3, dst[3], dstride);
    }
    template void cdef_estimate_block_8x8_all_sec_px<uint8_t>(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
        int dir, int pri_damping, int sec_damping, int *sse, uint8_t **dst, int dstride);
    template void cdef_estimate_block_8x8_all_sec_px<uint16_t>(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength,
        int dir, int pri_damping, int sec_damping, int *sse, uint16_t **dst, int dstride);

    void cdef_estimate_block_4x4_2pri_px(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1,
                                         int dir, int pri_damping, int sec_damping, int *sse)
    {
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength0, 0, dir, pri_damping, sec_damping, sse + 0);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength0, 1, dir, pri_damping, sec_damping, sse + 1);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength0, 2, dir, pri_damping, sec_damping, sse + 2);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength0, 4, dir, pri_damping, sec_damping, sse + 3);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength1, 0, dir, pri_damping, sec_damping, sse + 4);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength1, 1, dir, pri_damping, sec_damping, sse + 5);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength1, 2, dir, pri_damping, sec_damping, sse + 6);
        cdef_estimate_block_4x4_px(org, ostride, in, pri_strength1, 4, dir, pri_damping, sec_damping, sse + 7);
    }

    void cdef_estimate_block_4x4_nv12_2pri_px(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1,
                                              int dir, int pri_damping, int sec_damping, int *sse)
    {
        uint8_t *dst = nullptr;// not used
        int dstride = 0;// not used
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength0, 0, dir, pri_damping, sec_damping, sse + 0, dst, dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength0, 1, dir, pri_damping, sec_damping, sse + 1, dst, dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength0, 2, dir, pri_damping, sec_damping, sse + 2, dst, dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength0, 4, dir, pri_damping, sec_damping, sse + 3, dst, dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength1, 0, dir, pri_damping, sec_damping, sse + 4, dst, dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength1, 1, dir, pri_damping, sec_damping, sse + 5, dst, dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength1, 2, dir, pri_damping, sec_damping, sse + 6, dst, dstride);
        cdef_estimate_block_4x4_nv12_px(org, ostride, in, pri_strength1, 4, dir, pri_damping, sec_damping, sse + 7, dst, dstride);
    }

    void cdef_estimate_block_8x8_2pri_px(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1,
                                         int dir, int pri_damping, int sec_damping, int *sse)
    {
        uint8_t *dst = nullptr;// not used
        int dstride = 0;// not used
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength0, 0, dir, pri_damping, sec_damping, sse + 0, dst, dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength0, 1, dir, pri_damping, sec_damping, sse + 1, dst, dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength0, 2, dir, pri_damping, sec_damping, sse + 2, dst, dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength0, 4, dir, pri_damping, sec_damping, sse + 3, dst, dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength1, 0, dir, pri_damping, sec_damping, sse + 4, dst, dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength1, 1, dir, pri_damping, sec_damping, sse + 5, dst, dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength1, 2, dir, pri_damping, sec_damping, sse + 6, dst, dstride);
        cdef_estimate_block_8x8_px(org, ostride, in, pri_strength1, 4, dir, pri_damping, sec_damping, sse + 7, dst, dstride);
    }
};
