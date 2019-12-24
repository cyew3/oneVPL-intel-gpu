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
#include "stdlib.h"

#include <cstdint>

namespace AV1PP
{

#define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n)-1))) >> (n))
typedef int8_t int8_t;
typedef uint8_t uint8_t;
typedef int16_t int16_t;
typedef uint16_t uint16_t;

static inline int clamp(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}

static inline int8_t signed_char_clamp(int t) {
    return (int8_t)clamp(t, -128, 127);
}

//#if CONFIG_AV1_HIGHBITDEPTH
#if 1
static inline int16_t signed_char_clamp_high(int t, int bd) {
    switch (bd) {
    case 10: return (int16_t)clamp(t, -128 * 4, 128 * 4 - 1);
    case 12: return (int16_t)clamp(t, -128 * 16, 128 * 16 - 1);
    case 8:
    default: return (int16_t)clamp(t, -128, 128 - 1);
    }
}
#endif

// Should we apply any filter at all: 11111111 yes, 00000000 no
template <typename PixType>  int8_t filter_mask(uint8_t limit, uint8_t blimit, PixType p3,
                                 PixType p2, PixType p1, PixType p0, PixType q0,
                                 PixType q1, PixType q2, PixType q3, int bd)
{
    int8_t mask = 0;
    int16_t limit16 = (uint16_t)limit << (bd - 8);
    int16_t blimit16 = (uint16_t)blimit << (bd - 8);
    mask |= (abs(p3 - p2) > limit16) * -1;
    mask |= (abs(p2 - p1) > limit16) * -1;
    mask |= (abs(p1 - p0) > limit16) * -1;
    mask |= (abs(q1 - q0) > limit16) * -1;
    mask |= (abs(q2 - q1) > limit16) * -1;
    mask |= (abs(q3 - q2) > limit16) * -1;
    mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > blimit16) * -1;
    return ~mask;
}

template <typename PixType> int8_t flat_mask4(uint8_t thresh, PixType p3, PixType p2,
                                PixType p1, PixType p0, PixType q0, PixType q1,
                                PixType q2, PixType q3, int bd)
{
    int8_t mask = 0;
    int16_t thresh16 = (uint16_t)thresh << (bd - 8);
    mask |= (abs(p1 - p0) > thresh16) * -1;
    mask |= (abs(q1 - q0) > thresh16) * -1;
    mask |= (abs(p2 - p0) > thresh16) * -1;
    mask |= (abs(q2 - q0) > thresh16) * -1;
    mask |= (abs(p3 - p0) > thresh16) * -1;
    mask |= (abs(q3 - q0) > thresh16) * -1;
    return ~mask;
}

// Is there high edge variance internal edge: 11111111 yes, 00000000 no
static inline int16_t hev_mask(uint8_t thresh, uint16_t p1, uint16_t p0,
                              uint16_t q0, uint16_t q1, int bd)
{
    int16_t hev = 0;
    int16_t thresh16 = (uint16_t)thresh << (bd - 8);
    hev |= (abs(p1 - p0) > thresh16) * -1;
    hev |= (abs(q1 - q0) > thresh16) * -1;
    return hev;
}

template <typename PixType> void filter4(int8_t mask, uint8_t thresh, PixType *op1,
                           PixType *op0, PixType *oq0, PixType *oq1, int bd)
{
    int8_t filter1, filter2;
    // ^0x80 equivalent to subtracting 0x80 from the values to turn them
    // into -128 to +127 instead of 0 to 255.
    int shift = bd - 8;
    const int16_t ps1 = (int16_t)*op1 - (0x80 << shift);
    const int16_t ps0 = (int16_t)*op0 - (0x80 << shift);
    const int16_t qs0 = (int16_t)*oq0 - (0x80 << shift);
    const int16_t qs1 = (int16_t)*oq1 - (0x80 << shift);
    const uint16_t hev = hev_mask(thresh, *op1, *op0, *oq0, *oq1, bd);

    // add outer taps if we have high edge variance
    int16_t filter = signed_char_clamp_high(ps1 - qs1, bd) & hev;

    // inner taps
    filter = signed_char_clamp_high(filter + 3 * (qs0 - ps0), bd) & mask;

    // save bottom 3 bits so that we round one side +4 and the other +3
    // if it equals 4 we'll set to adjust by -1 to account for the fact
    // we'd round 3 the other way
    filter1 = signed_char_clamp_high(filter + 4, bd) >> 3;
    filter2 = signed_char_clamp_high(filter + 3, bd) >> 3;

    *oq0 = (PixType)(signed_char_clamp_high(qs0 - filter1, bd) + (0x80 << shift));
    *op0 = (PixType)(signed_char_clamp_high(ps0 + filter2, bd) + (0x80 << shift));

    // outer tap adjustments
    filter = ROUND_POWER_OF_TWO(filter1, 1) & ~hev;

    *oq1 = (PixType)(signed_char_clamp_high(qs1 - filter, bd) + (0x80 << shift));
    *op1 = (PixType)(signed_char_clamp_high(ps1 + filter, bd) + (0x80 << shift));
}

    typedef enum EdgeDir { VERT_EDGE = 0, HORZ_EDGE = 1, NUM_EDGE_DIRS } EdgeDir;

    // should we apply any filter at all: 11111111 yes, 00000000 no
    template <typename PixType> int8_t filter_mask2(uint8_t limit, uint8_t blimit, PixType p1, PixType p0, PixType q0, PixType q1, int bd)
    {
        int8_t mask = 0;
        int16_t limit16 = (uint16_t)limit << (bd - 8);
        int16_t blimit16 = (uint16_t)blimit << (bd - 8);
        mask |= (abs(p1 - p0) > limit16) * -1;
        mask |= (abs(q1 - q0) > limit16) * -1;
        mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > blimit16) * -1;
        return ~mask;
    }

    static int8_t filter_mask3_chroma(uint8_t limit, uint8_t blimit, uint16_t p2, uint16_t p1, uint16_t p0, uint16_t q0, uint16_t q1, uint16_t q2, int bd) {
        int8_t mask = 0;
        int16_t limit16 = (uint16_t)limit << (bd - 8);
        int16_t blimit16 = (uint16_t)blimit << (bd - 8);
        mask |= (abs(p2 - p1) > limit16) * -1;
        mask |= (abs(p1 - p0) > limit16) * -1;
        mask |= (abs(q1 - q0) > limit16) * -1;
        mask |= (abs(q2 - q1) > limit16) * -1;
        mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2 > blimit16) * -1;
        return ~mask;
    }

    static int8_t flat_mask3_chroma(uint8_t thresh, uint16_t p2, uint16_t p1, uint16_t p0, uint16_t q0, uint16_t q1, uint16_t q2, int bd) {
        int8_t mask = 0;
        int16_t thresh16 = (uint16_t)thresh << (bd - 8);
        mask |= (abs(p1 - p0) > thresh16) * -1;
        mask |= (abs(q1 - q0) > thresh16) * -1;
        mask |= (abs(p2 - p0) > thresh16) * -1;
        mask |= (abs(q2 - q0) > thresh16) * -1;
        return ~mask;
    }

    template <typename PixType> void filter6(int8_t mask, uint8_t thresh, int8_t flat, PixType *op2, PixType *op1, PixType *op0, PixType *oq0, PixType *oq1, PixType *oq2, int bd)
    {
        if (flat && mask) {
            const PixType p2 = *op2, p1 = *op1, p0 = *op0;
            const PixType q0 = *oq0, q1 = *oq1, q2 = *oq2;

            // 5-tap filter [1, 2, 2, 2, 1]
            *op1 = ROUND_POWER_OF_TWO(p2 * 3 + p1 * 2 + p0 * 2 + q0, 3);
            *op0 = ROUND_POWER_OF_TWO(p2 + p1 * 2 + p0 * 2 + q0 * 2 + q1, 3);
            *oq0 = ROUND_POWER_OF_TWO(p1 + p0 * 2 + q0 * 2 + q1 * 2 + q2, 3);
            *oq1 = ROUND_POWER_OF_TWO(p0 + q0 * 2 + q1 * 2 + q2 * 3, 3);
        } else {
            filter4(mask, thresh, op1, op0, oq0, oq1, bd);
        }
    }
    template void filter6<uint8_t>(int8_t mask, uint8_t thresh, int8_t flat, uint8_t *op2, uint8_t *op1, uint8_t *op0, uint8_t *oq0, uint8_t *oq1, uint8_t *oq2, int bd);
    template void filter6<uint16_t>(int8_t mask, uint8_t thresh, int8_t flat, uint16_t *op2, uint16_t *op1, uint16_t *op0, uint16_t *oq0, uint16_t *oq1, uint16_t *oq2, int bd);

    template <typename PixType> void lpf_vertical_4_av1_px(PixType *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh)
    {
        int i;
        int count = 4;

        int bd = sizeof(PixType) == 1 ? 8 : 10;

        // loop filter designed to work using chars so that we can make maximum use
        // of 8 bit simd instructions.
        for (i = 0; i < count; ++i) {
            const PixType p1 = s[-2], p0 = s[-1];
            const PixType q0 = s[0], q1 = s[1];
            const int8_t mask = filter_mask2(*limit, *blimit, p1, p0, q0, q1, bd);
            filter4(mask, *thresh, s - 2, s - 1, s, s + 1, bd);
            s += pitch;
        }
    }
    template void lpf_vertical_4_av1_px<uint8_t>(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    template void lpf_vertical_4_av1_px<uint16_t>(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

    template <typename PixType> void lpf_vertical_6_av1_px(PixType *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh)
    {
        int i;
        int count = 4;
        int bd = sizeof(PixType) == 1 ? 8 : 10;

        for (i = 0; i < count; ++i) {
            const PixType p2 = s[-3], p1 = s[-2], p0 = s[-1];
            const PixType q0 = s[0], q1 = s[1], q2 = s[2];
            const int8_t mask =
                filter_mask3_chroma(*limit, *blimit, p2, p1, p0, q0, q1, q2, bd);
            const int8_t flat = flat_mask3_chroma(1, p2, p1, p0, q0, q1, q2, bd);
            filter6(mask, *thresh, flat, s - 3, s - 2, s - 1, s, s + 1, s + 2, bd);
            s += pitch;
        }
    }
    template void lpf_vertical_6_av1_px<uint8_t>(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    template void lpf_vertical_6_av1_px<uint16_t>(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

    template <typename PixType> void filter8(int8_t mask, uint8_t thresh, int8_t flat,
        PixType *op3, PixType *op2, PixType *op1,
        PixType *op0, PixType *oq0, PixType *oq1,
        PixType *oq2, PixType *oq3, int bd)
    {
        if (flat && mask) {
            const PixType p3 = *op3, p2 = *op2, p1 = *op1, p0 = *op0;
            const PixType q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3;

            // 7-tap filter [1, 1, 1, 2, 1, 1, 1]
            *op2 = ROUND_POWER_OF_TWO(p3 + p3 + p3 + 2 * p2 + p1 + p0 + q0, 3);
            *op1 = ROUND_POWER_OF_TWO(p3 + p3 + p2 + 2 * p1 + p0 + q0 + q1, 3);
            *op0 = ROUND_POWER_OF_TWO(p3 + p2 + p1 + 2 * p0 + q0 + q1 + q2, 3);
            *oq0 = ROUND_POWER_OF_TWO(p2 + p1 + p0 + 2 * q0 + q1 + q2 + q3, 3);
            *oq1 = ROUND_POWER_OF_TWO(p1 + p0 + q0 + 2 * q1 + q2 + q3 + q3, 3);
            *oq2 = ROUND_POWER_OF_TWO(p0 + q0 + q1 + 2 * q2 + q3 + q3 + q3, 3);
        } else {
            filter4(mask, thresh, op1, op0, oq0, oq1, bd);
        }
    }

    template <typename PixType> void lpf_vertical_8_av1_px(PixType *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh)
    {
        int i;
        int count = 4;
        int bd = sizeof(PixType) == 1 ? 8 : 10;

        for (i = 0; i < count; ++i) {
            const PixType p3 = s[-4], p2 = s[-3], p1 = s[-2], p0 = s[-1];
            const PixType q0 = s[0], q1 = s[1], q2 = s[2], q3 = s[3];
            const int8_t mask = filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3, bd);
            const int8_t flat = flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3, bd);
            filter8(mask, *thresh, flat, s - 4, s - 3, s - 2, s - 1, s, s + 1, s + 2, s + 3, bd);
            s += pitch;
        }
    }
    template void lpf_vertical_8_av1_px<uint8_t>(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    template void lpf_vertical_8_av1_px<uint16_t>(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

    template <typename PixType> void filter14(int8_t mask, uint8_t thresh, int8_t flat, int8_t flat2, PixType *op6, PixType *op5,
                         PixType *op4, PixType *op3, PixType *op2, PixType *op1, PixType *op0, PixType *oq0,
                         PixType *oq1, PixType *oq2, PixType *oq3, PixType *oq4, PixType *oq5, PixType *oq6, int bd)
    {
        if (flat2 && flat && mask) {
            const PixType p6 = *op6, p5 = *op5, p4 = *op4, p3 = *op3, p2 = *op2,
                p1 = *op1, p0 = *op0;
            const PixType q0 = *oq0, q1 = *oq1, q2 = *oq2, q3 = *oq3, q4 = *oq4,
                q5 = *oq5, q6 = *oq6;

            // 13-tap filter [1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1]
            *op5 = ROUND_POWER_OF_TWO(p6 * 7 + p5 * 2 + p4 * 2 + p3 + p2 + p1 + p0 + q0,
                4);
            *op4 = ROUND_POWER_OF_TWO(
                p6 * 5 + p5 * 2 + p4 * 2 + p3 * 2 + p2 + p1 + p0 + q0 + q1, 4);
            *op3 = ROUND_POWER_OF_TWO(
                p6 * 4 + p5 + p4 * 2 + p3 * 2 + p2 * 2 + p1 + p0 + q0 + q1 + q2, 4);
            *op2 = ROUND_POWER_OF_TWO(
                p6 * 3 + p5 + p4 + p3 * 2 + p2 * 2 + p1 * 2 + p0 + q0 + q1 + q2 + q3,
                4);
            *op1 = ROUND_POWER_OF_TWO(p6 * 2 + p5 + p4 + p3 + p2 * 2 + p1 * 2 + p0 * 2 +
                q0 + q1 + q2 + q3 + q4,
                4);
            *op0 = ROUND_POWER_OF_TWO(p6 + p5 + p4 + p3 + p2 + p1 * 2 + p0 * 2 +
                q0 * 2 + q1 + q2 + q3 + q4 + q5,
                4);
            *oq0 = ROUND_POWER_OF_TWO(p5 + p4 + p3 + p2 + p1 + p0 * 2 + q0 * 2 +
                q1 * 2 + q2 + q3 + q4 + q5 + q6,
                4);
            *oq1 = ROUND_POWER_OF_TWO(p4 + p3 + p2 + p1 + p0 + q0 * 2 + q1 * 2 +
                q2 * 2 + q3 + q4 + q5 + q6 * 2,
                4);
            *oq2 = ROUND_POWER_OF_TWO(
                p3 + p2 + p1 + p0 + q0 + q1 * 2 + q2 * 2 + q3 * 2 + q4 + q5 + q6 * 3,
                4);
            *oq3 = ROUND_POWER_OF_TWO(
                p2 + p1 + p0 + q0 + q1 + q2 * 2 + q3 * 2 + q4 * 2 + q5 + q6 * 4, 4);
            *oq4 = ROUND_POWER_OF_TWO(
                p1 + p0 + q0 + q1 + q2 + q3 * 2 + q4 * 2 + q5 * 2 + q6 * 5, 4);
            *oq5 = ROUND_POWER_OF_TWO(p0 + q0 + q1 + q2 + q3 + q4 * 2 + q5 * 2 + q6 * 7,
                4);
        } else {
            filter8(mask, thresh, flat, op3, op2, op1, op0, oq0, oq1, oq2, oq3, bd);
        }
    }

    template <typename PixType> void mb_lpf_vertical_edge_w1(PixType *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd)
    {
        int i;

        for (i = 0; i < count; ++i) {
            const PixType p7 = s[-8], p6 = s[-7], p5 = s[-6], p4 = s[-5], p3 = s[-4],
                        p2 = s[-3], p1 = s[-2], p0 = s[-1];
            const PixType q0 = s[0], q1 = s[1], q2 = s[2], q3 = s[3], q4 = s[4],
                        q5 = s[5], q6 = s[6], q7 = s[7];
            const int8_t mask = filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3, bd);
            const int8_t flat = flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3, bd);

            //(void)p7;
            //(void)q7;
            const int32_t flat2 = flat_mask4(1, p6, p5, p4, p0, q0, q4, q5, q6, bd);
            filter14(mask, *thresh, flat, flat2, s - 7, s - 6, s - 5, s - 4, s - 3, s - 2, s - 1, s, s + 1, s + 2, s + 3, s + 4, s + 5, s + 6, bd);

            s += p;
        }
    }

    template <typename PixType> void lpf_vertical_14_av1_px(PixType *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh)
    {
        int bd = sizeof(PixType) == 1 ? 8 : 10;
        mb_lpf_vertical_edge_w1(s, p, blimit, limit, thresh, 4, bd);
    }
    template void lpf_vertical_14_av1_px<uint8_t>(uint8_t *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    template void lpf_vertical_14_av1_px<uint16_t>(uint16_t *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

    template <typename PixType> void lpf_horizontal_4_av1_px(PixType *s, int p /* pitch */,const uint8_t *blimit, const uint8_t *limit,const uint8_t *thresh)
    {
        int i;
        int count = 4;
        int bd = sizeof(PixType) == 1 ? 8 : 10;

        // loop filter designed to work using chars so that we can make maximum use
        // of 8 bit simd instructions.
        for (i = 0; i < count; ++i) {
            const PixType p1 = s[-2 * p], p0 = s[-p];
            const PixType q0 = s[0 * p], q1 = s[1 * p];
            const int8_t mask = filter_mask2(*limit, *blimit, p1, p0, q0, q1, bd);
            filter4(mask, *thresh, s - 2 * p, s - 1 * p, s, s + 1 * p, bd);
            ++s;
        }
    }
    template void lpf_horizontal_4_av1_px<uint8_t>(uint8_t *s, int p /* pitch */, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    template void lpf_horizontal_4_av1_px<uint16_t>(uint16_t *s, int p /* pitch */, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

    template <typename PixType> void lpf_horizontal_6_av1_px(PixType *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh)
    {
        int i;
        int count = 4;
        int bd = sizeof(PixType) == 1 ? 8 : 10;

        // loop filter designed to work using chars so that we can make maximum use
        // of 8 bit simd instructions.
        for (i = 0; i < count; ++i) {
            const PixType p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
            const PixType q0 = s[0 * p], q1 = s[1 * p], q2 = s[2 * p];

            const int8_t mask = filter_mask3_chroma(*limit, *blimit, p2, p1, p0, q0, q1, q2, bd);
            const int8_t flat = flat_mask3_chroma(1, p2, p1, p0, q0, q1, q2, bd);
            filter6(mask, *thresh, flat, s - 3 * p, s - 2 * p, s - 1 * p, s, s + 1 * p, s + 2 * p, bd);
            ++s;
        }
    }
    template void lpf_horizontal_6_av1_px<uint8_t>(uint8_t *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    template void lpf_horizontal_6_av1_px<uint16_t>(uint16_t *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

    template <typename PixType> void lpf_horizontal_8_av1_px(PixType *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh)
    {
        int i;
        int count = 4;
        int bd = sizeof(PixType) == 1 ? 8 : 10;

        // loop filter designed to work using chars so that we can make maximum use
        // of 8 bit simd instructions.
        for (i = 0; i < count; ++i) {
            const PixType p3 = s[-4 * p], p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
            const PixType q0 = s[0 * p], q1 = s[1 * p], q2 = s[2 * p], q3 = s[3 * p];

            const int8_t mask = filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3, bd);
            const int8_t flat = flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3, bd);
            filter8(mask, *thresh, flat, s - 4 * p, s - 3 * p, s - 2 * p, s - 1 * p, s,
                s + 1 * p, s + 2 * p, s + 3 * p, bd);
            ++s;
        }
    }
    template void lpf_horizontal_8_av1_px<uint8_t>(uint8_t *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    template void lpf_horizontal_8_av1_px<uint16_t>(uint16_t *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

    template <typename PixType> void mb_lpf_horizontal_edge_w1(PixType *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd)
    {
        int i;
        int step = 4;

        // loop filter designed to work using chars so that we can make maximum use
        // of 8 bit simd instructions.
        for (i = 0; i < step * count; ++i) {
            const PixType p7 = s[-8 * p], p6 = s[-7 * p], p5 = s[-6 * p],
                        p4 = s[-5 * p], p3 = s[-4 * p], p2 = s[-3 * p],
                        p1 = s[-2 * p], p0 = s[-p];
            const PixType q0 = s[0 * p], q1 = s[1 * p], q2 = s[2 * p], q3 = s[3 * p],
                        q4 = s[4 * p], q5 = s[5 * p], q6 = s[6 * p], q7 = s[7 * p];
            const int8_t mask = filter_mask(*limit, *blimit, p3, p2, p1, p0, q0, q1, q2, q3, bd);
            const int8_t flat = flat_mask4(1, p3, p2, p1, p0, q0, q1, q2, q3, bd);

            (void)p7;
            (void)q7;
            const int32_t flat2 = flat_mask4(1, p6, p5, p4, p0, q0, q4, q5, q6, bd);

            filter14(mask, *thresh, flat, flat2, s - 7 * p, s - 6 * p, s - 5 * p,
                     s - 4 * p, s - 3 * p, s - 2 * p, s - 1 * p, s, s + 1 * p,
                     s + 2 * p, s + 3 * p, s + 4 * p, s + 5 * p, s + 6 * p, bd);

            ++s;
        }
    }

     template <typename PixType> void lpf_horizontal_14_av1_px(PixType *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh)
    {
        int bd = sizeof(PixType) == 1 ? 8 : 10;
        mb_lpf_horizontal_edge_w1(s, p, blimit, limit, thresh, 1, bd);
    }
     template void lpf_horizontal_14_av1_px<uint8_t>(uint8_t *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
     template void lpf_horizontal_14_av1_px<uint16_t>(uint16_t *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
};
