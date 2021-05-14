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
#include "string.h"
#include "ipps.h"
#include <stdint.h>

#define AOMMIN(x, y) (((x) < (y)) ? (x) : (y))
#define AOMMAX(x, y) (((x) > (y)) ? (x) : (y))

static inline uint8_t clip_pixel(int val) {
    return (val > 255) ? 255 : (val < 0) ? 0 : val;
}

typedef int32_t tran_high_t;
typedef int16_t tran_low_t;
/* Shift down with rounding */
#define ROUND_POWER_OF_TWO(value, n) \
    (((value) + (1 << ((n) - 1))) >> (n))

#define DCT_CONST_BITS 14

static inline tran_high_t fdct_round_shift(tran_high_t input) {
    tran_high_t rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
    return rv;
}
static inline tran_high_t dct_const_round_shift(tran_high_t input) {
    tran_high_t rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
    return (tran_high_t)rv;
}

static inline tran_high_t check_range(tran_high_t input)
{
    return input;
}
#define WRAPLOW(x) ((int32_t)check_range(x))

#define range_check_buf(...)

static inline int64_t clamp64(int64_t value, int64_t low, int64_t high) {
  return value < low ? low : (value > high ? high : value);
}

static inline int32_t clamp_value(int32_t value, int8_t bit) {
  if (bit <= 0) return value;  // Do nothing for invalid clamp bit.
  const int64_t max_value = (1LL << (bit - 1)) - 1;
  const int64_t min_value = -(1LL << (bit - 1));
  return (int32_t)clamp64(value, min_value, max_value);
}

static inline void clamp_buf(int32_t *buf, int32_t size, int8_t bit) {
    for (int i = 0; i < size; ++i) buf[i] = clamp_value(buf[i], bit);
}

static inline int32_t range_check_value(int32_t value, int8_t bit) {
#if CONFIG_COEFFICIENT_RANGE_CHECKING
  const int64_t max_value = (1LL << (bit - 1)) - 1;
  const int64_t min_value = -(1LL << (bit - 1));
  if (value < min_value || value > max_value) {
    fprintf(stderr, "coeff out of bit range, value: %d bit %d\n", value, bit);
    assert(0);
  }
#else
  (void)bit;
#endif
  return value;
}

static inline uint8_t clip_pixel_add(uint8_t dest, tran_high_t trans)
{
    trans = WRAPLOW(trans);
    return clip_pixel(dest + (int)trans);
}

namespace details
{
    // ----------------------------------------------------
    //                    TRANSFORMS
    // ----------------------------------------------------

    static const int NewSqrt2Bits = 12;
    // 2^12 * sqrt(2)
    static const int32_t NewSqrt2 = 5793;
    // 2^12 / sqrt(2)
    static const int32_t NewInvSqrt2 = 2896;

    // Constants:
    //  for (int i = 1; i< 32; ++i)
    //    printf("static const int cospi_%d_64 = %.0f;\n", i,
    //           round(16384 * cos(i*M_PI/64)));
    // Note: sin(k*Pi/64) = cos((32-k)*Pi/64)
    static const tran_high_t cospi_1_64  = 16364;
    static const tran_high_t cospi_2_64  = 16305;
    static const tran_high_t cospi_3_64  = 16207;
    static const tran_high_t cospi_4_64  = 16069;
    static const tran_high_t cospi_5_64  = 15893;
    static const tran_high_t cospi_6_64  = 15679;
    static const tran_high_t cospi_7_64  = 15426;
    static const tran_high_t cospi_8_64  = 15137;
    static const tran_high_t cospi_9_64  = 14811;
    static const tran_high_t cospi_10_64 = 14449;
    static const tran_high_t cospi_11_64 = 14053;
    static const tran_high_t cospi_12_64 = 13623;
    static const tran_high_t cospi_13_64 = 13160;
    static const tran_high_t cospi_14_64 = 12665;
    static const tran_high_t cospi_15_64 = 12140;
    static const tran_high_t cospi_16_64 = 11585;
    static const tran_high_t cospi_17_64 = 11003;
    static const tran_high_t cospi_18_64 = 10394;
    static const tran_high_t cospi_19_64 = 9760;
    static const tran_high_t cospi_20_64 = 9102;
    static const tran_high_t cospi_21_64 = 8423;
    static const tran_high_t cospi_22_64 = 7723;
    static const tran_high_t cospi_23_64 = 7005;
    static const tran_high_t cospi_24_64 = 6270;
    static const tran_high_t cospi_25_64 = 5520;
    static const tran_high_t cospi_26_64 = 4756;
    static const tran_high_t cospi_27_64 = 3981;
    static const tran_high_t cospi_28_64 = 3196;
    static const tran_high_t cospi_29_64 = 2404;
    static const tran_high_t cospi_30_64 = 1606;
    static const tran_high_t cospi_31_64 = 804;

    //  16384 * sqrt(2) * sin(kPi/9) * 2 / 3
    static const tran_high_t sinpi_1_9 = 5283;
    static const tran_high_t sinpi_2_9 = 9929;
    static const tran_high_t sinpi_3_9 = 13377;
    static const tran_high_t sinpi_4_9 = 15212;

    static void fadst16(const tran_low_t *input, tran_low_t *output)
    {
        tran_high_t s0, s1, s2, s3, s4, s5, s6, s7, s8;
        tran_high_t s9, s10, s11, s12, s13, s14, s15;

        tran_high_t x0 = input[15];
        tran_high_t x1 = input[0];
        tran_high_t x2 = input[13];
        tran_high_t x3 = input[2];
        tran_high_t x4 = input[11];
        tran_high_t x5 = input[4];
        tran_high_t x6 = input[9];
        tran_high_t x7 = input[6];
        tran_high_t x8 = input[7];
        tran_high_t x9 = input[8];
        tran_high_t x10 = input[5];
        tran_high_t x11 = input[10];
        tran_high_t x12 = input[3];
        tran_high_t x13 = input[12];
        tran_high_t x14 = input[1];
        tran_high_t x15 = input[14];

        // stage 1
        s0 = x0 * cospi_1_64  + x1 * cospi_31_64;
        s1 = x0 * cospi_31_64 - x1 * cospi_1_64;
        s2 = x2 * cospi_5_64  + x3 * cospi_27_64;
        s3 = x2 * cospi_27_64 - x3 * cospi_5_64;
        s4 = x4 * cospi_9_64  + x5 * cospi_23_64;
        s5 = x4 * cospi_23_64 - x5 * cospi_9_64;
        s6 = x6 * cospi_13_64 + x7 * cospi_19_64;
        s7 = x6 * cospi_19_64 - x7 * cospi_13_64;
        s8 = x8 * cospi_17_64 + x9 * cospi_15_64;
        s9 = x8 * cospi_15_64 - x9 * cospi_17_64;
        s10 = x10 * cospi_21_64 + x11 * cospi_11_64;
        s11 = x10 * cospi_11_64 - x11 * cospi_21_64;
        s12 = x12 * cospi_25_64 + x13 * cospi_7_64;
        s13 = x12 * cospi_7_64  - x13 * cospi_25_64;
        s14 = x14 * cospi_29_64 + x15 * cospi_3_64;
        s15 = x14 * cospi_3_64  - x15 * cospi_29_64;

        x0 = fdct_round_shift(s0 + s8);
        x1 = fdct_round_shift(s1 + s9);
        x2 = fdct_round_shift(s2 + s10);
        x3 = fdct_round_shift(s3 + s11);
        x4 = fdct_round_shift(s4 + s12);
        x5 = fdct_round_shift(s5 + s13);
        x6 = fdct_round_shift(s6 + s14);
        x7 = fdct_round_shift(s7 + s15);
        x8  = fdct_round_shift(s0 - s8);
        x9  = fdct_round_shift(s1 - s9);
        x10 = fdct_round_shift(s2 - s10);
        x11 = fdct_round_shift(s3 - s11);
        x12 = fdct_round_shift(s4 - s12);
        x13 = fdct_round_shift(s5 - s13);
        x14 = fdct_round_shift(s6 - s14);
        x15 = fdct_round_shift(s7 - s15);

        // stage 2
        s0 = x0;
        s1 = x1;
        s2 = x2;
        s3 = x3;
        s4 = x4;
        s5 = x5;
        s6 = x6;
        s7 = x7;
        s8 =    x8 * cospi_4_64   + x9 * cospi_28_64;
        s9 =    x8 * cospi_28_64  - x9 * cospi_4_64;
        s10 =   x10 * cospi_20_64 + x11 * cospi_12_64;
        s11 =   x10 * cospi_12_64 - x11 * cospi_20_64;
        s12 = - x12 * cospi_28_64 + x13 * cospi_4_64;
        s13 =   x12 * cospi_4_64  + x13 * cospi_28_64;
        s14 = - x14 * cospi_12_64 + x15 * cospi_20_64;
        s15 =   x14 * cospi_20_64 + x15 * cospi_12_64;

        x0 = s0 + s4;
        x1 = s1 + s5;
        x2 = s2 + s6;
        x3 = s3 + s7;
        x4 = s0 - s4;
        x5 = s1 - s5;
        x6 = s2 - s6;
        x7 = s3 - s7;
        x8 = fdct_round_shift(s8 + s12);
        x9 = fdct_round_shift(s9 + s13);
        x10 = fdct_round_shift(s10 + s14);
        x11 = fdct_round_shift(s11 + s15);
        x12 = fdct_round_shift(s8 - s12);
        x13 = fdct_round_shift(s9 - s13);
        x14 = fdct_round_shift(s10 - s14);
        x15 = fdct_round_shift(s11 - s15);

        // stage 3
        s0 = x0;
        s1 = x1;
        s2 = x2;
        s3 = x3;
        s4 = x4 * cospi_8_64  + x5 * cospi_24_64;
        s5 = x4 * cospi_24_64 - x5 * cospi_8_64;
        s6 = - x6 * cospi_24_64 + x7 * cospi_8_64;
        s7 =   x6 * cospi_8_64  + x7 * cospi_24_64;
        s8 = x8;
        s9 = x9;
        s10 = x10;
        s11 = x11;
        s12 = x12 * cospi_8_64  + x13 * cospi_24_64;
        s13 = x12 * cospi_24_64 - x13 * cospi_8_64;
        s14 = - x14 * cospi_24_64 + x15 * cospi_8_64;
        s15 =   x14 * cospi_8_64  + x15 * cospi_24_64;

        x0 = s0 + s2;
        x1 = s1 + s3;
        x2 = s0 - s2;
        x3 = s1 - s3;
        x4 = fdct_round_shift(s4 + s6);
        x5 = fdct_round_shift(s5 + s7);
        x6 = fdct_round_shift(s4 - s6);
        x7 = fdct_round_shift(s5 - s7);
        x8 = s8 + s10;
        x9 = s9 + s11;
        x10 = s8 - s10;
        x11 = s9 - s11;
        x12 = fdct_round_shift(s12 + s14);
        x13 = fdct_round_shift(s13 + s15);
        x14 = fdct_round_shift(s12 - s14);
        x15 = fdct_round_shift(s13 - s15);

        // stage 4
        s2 = (- cospi_16_64) * (x2 + x3);
        s3 = cospi_16_64 * (x2 - x3);
        s6 = cospi_16_64 * (x6 + x7);
        s7 = cospi_16_64 * (- x6 + x7);
        s10 = cospi_16_64 * (x10 + x11);
        s11 = cospi_16_64 * (- x10 + x11);
        s14 = (- cospi_16_64) * (x14 + x15);
        s15 = cospi_16_64 * (x14 - x15);

        x2 = fdct_round_shift(s2);
        x3 = fdct_round_shift(s3);
        x6 = fdct_round_shift(s6);
        x7 = fdct_round_shift(s7);
        x10 = fdct_round_shift(s10);
        x11 = fdct_round_shift(s11);
        x14 = fdct_round_shift(s14);
        x15 = fdct_round_shift(s15);

        output[0] = (tran_low_t)x0;
        output[1] = (tran_low_t)-x8;
        output[2] = (tran_low_t)x12;
        output[3] = (tran_low_t)-x4;
        output[4] = (tran_low_t)x6;
        output[5] = (tran_low_t)x14;
        output[6] = (tran_low_t)x10;
        output[7] = (tran_low_t)x2;
        output[8] = (tran_low_t)x3;
        output[9] = (tran_low_t)x11;
        output[10] = (tran_low_t)x15;
        output[11] = (tran_low_t)x7;
        output[12] = (tran_low_t)x5;
        output[13] = (tran_low_t)-x13;
        output[14] = (tran_low_t)x9;
        output[15] = (tran_low_t)-x1;
    }


    static void fadst8(const tran_low_t *input, tran_low_t *output)
    {
        tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;

        tran_high_t x0 = input[7];
        tran_high_t x1 = input[0];
        tran_high_t x2 = input[5];
        tran_high_t x3 = input[2];
        tran_high_t x4 = input[3];
        tran_high_t x5 = input[4];
        tran_high_t x6 = input[1];
        tran_high_t x7 = input[6];

        // stage 1
        s0 = cospi_2_64  * x0 + cospi_30_64 * x1;
        s1 = cospi_30_64 * x0 - cospi_2_64  * x1;
        s2 = cospi_10_64 * x2 + cospi_22_64 * x3;
        s3 = cospi_22_64 * x2 - cospi_10_64 * x3;
        s4 = cospi_18_64 * x4 + cospi_14_64 * x5;
        s5 = cospi_14_64 * x4 - cospi_18_64 * x5;
        s6 = cospi_26_64 * x6 + cospi_6_64  * x7;
        s7 = cospi_6_64  * x6 - cospi_26_64 * x7;

        x0 = fdct_round_shift(s0 + s4);
        x1 = fdct_round_shift(s1 + s5);
        x2 = fdct_round_shift(s2 + s6);
        x3 = fdct_round_shift(s3 + s7);
        x4 = fdct_round_shift(s0 - s4);
        x5 = fdct_round_shift(s1 - s5);
        x6 = fdct_round_shift(s2 - s6);
        x7 = fdct_round_shift(s3 - s7);

        // stage 2
        s0 = x0;
        s1 = x1;
        s2 = x2;
        s3 = x3;
        s4 = cospi_8_64  * x4 + cospi_24_64 * x5;
        s5 = cospi_24_64 * x4 - cospi_8_64  * x5;
        s6 = - cospi_24_64 * x6 + cospi_8_64  * x7;
        s7 =   cospi_8_64  * x6 + cospi_24_64 * x7;

        x0 = s0 + s2;
        x1 = s1 + s3;
        x2 = s0 - s2;
        x3 = s1 - s3;
        x4 = fdct_round_shift(s4 + s6);
        x5 = fdct_round_shift(s5 + s7);
        x6 = fdct_round_shift(s4 - s6);
        x7 = fdct_round_shift(s5 - s7);

        // stage 3
        s2 = cospi_16_64 * (x2 + x3);
        s3 = cospi_16_64 * (x2 - x3);
        s6 = cospi_16_64 * (x6 + x7);
        s7 = cospi_16_64 * (x6 - x7);

        x2 = fdct_round_shift(s2);
        x3 = fdct_round_shift(s3);
        x6 = fdct_round_shift(s6);
        x7 = fdct_round_shift(s7);

        output[0] = (tran_low_t)x0;
        output[1] = (tran_low_t)-x4;
        output[2] = (tran_low_t)x6;
        output[3] = (tran_low_t)-x2;
        output[4] = (tran_low_t)x3;
        output[5] = (tran_low_t)-x7;
        output[6] = (tran_low_t)x5;
        output[7] = (tran_low_t)-x1;
    }

    static void fadst4(const tran_low_t *input, tran_low_t *output)
    {
        tran_high_t x0, x1, x2, x3;
        tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;

        x0 = input[0];
        x1 = input[1];
        x2 = input[2];
        x3 = input[3];

        if (!(x0 | x1 | x2 | x3)) {
            output[0] = output[1] = output[2] = output[3] = 0;
            return;
        }

        s0 = sinpi_1_9 * x0;
        s1 = sinpi_4_9 * x0;
        s2 = sinpi_2_9 * x1;
        s3 = sinpi_1_9 * x1;
        s4 = sinpi_3_9 * x2;
        s5 = sinpi_4_9 * x3;
        s6 = sinpi_2_9 * x3;
        s7 = x0 + x1 - x3;

        x0 = s0 + s2 + s5;
        x1 = sinpi_3_9 * s7;
        x2 = s1 - s3 + s6;
        x3 = s4;

        s0 = x0 + x3;
        s1 = x1;
        s2 = x2 - x3;
        s3 = x2 - x0 + x3;

        // 1-D transform scaling factor is sqrt(2).
        output[0] = (tran_low_t)fdct_round_shift(s0);
        output[1] = (tran_low_t)fdct_round_shift(s1);
        output[2] = (tran_low_t)fdct_round_shift(s2);
        output[3] = (tran_low_t)fdct_round_shift(s3);
    }


    static void fdct16(const tran_low_t in[16], tran_low_t out[16])
    {
        tran_high_t step1[8];      // canbe16
        tran_high_t step2[8];      // canbe16
        tran_high_t step3[8];      // canbe16
        tran_high_t input[8];      // canbe16
        tran_high_t temp1, temp2;  // needs32

        // step 1
        input[0] = in[0] + in[15];
        input[1] = in[1] + in[14];
        input[2] = in[2] + in[13];
        input[3] = in[3] + in[12];
        input[4] = in[4] + in[11];
        input[5] = in[5] + in[10];
        input[6] = in[6] + in[ 9];
        input[7] = in[7] + in[ 8];

        step1[0] = in[7] - in[ 8];
        step1[1] = in[6] - in[ 9];
        step1[2] = in[5] - in[10];
        step1[3] = in[4] - in[11];
        step1[4] = in[3] - in[12];
        step1[5] = in[2] - in[13];
        step1[6] = in[1] - in[14];
        step1[7] = in[0] - in[15];

        // fdct8(step, step);
        {
            tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
            tran_high_t t0, t1, t2, t3;                  // needs32
            tran_high_t x0, x1, x2, x3;                  // canbe16

            // stage 1
            s0 = input[0] + input[7];
            s1 = input[1] + input[6];
            s2 = input[2] + input[5];
            s3 = input[3] + input[4];
            s4 = input[3] - input[4];
            s5 = input[2] - input[5];
            s6 = input[1] - input[6];
            s7 = input[0] - input[7];

            // fdct4(step, step);
            x0 = s0 + s3;
            x1 = s1 + s2;
            x2 = s1 - s2;
            x3 = s0 - s3;
            t0 = (x0 + x1) * cospi_16_64;
            t1 = (x0 - x1) * cospi_16_64;
            t2 = x3 * cospi_8_64  + x2 * cospi_24_64;
            t3 = x3 * cospi_24_64 - x2 * cospi_8_64;
            out[0] = (tran_low_t)fdct_round_shift(t0);
            out[4] = (tran_low_t)fdct_round_shift(t2);
            out[8] = (tran_low_t)fdct_round_shift(t1);
            out[12] = (tran_low_t)fdct_round_shift(t3);

            // Stage 2
            t0 = (s6 - s5) * cospi_16_64;
            t1 = (s6 + s5) * cospi_16_64;
            t2 = fdct_round_shift(t0);
            t3 = fdct_round_shift(t1);

            // Stage 3
            x0 = s4 + t2;
            x1 = s4 - t2;
            x2 = s7 - t3;
            x3 = s7 + t3;

            // Stage 4
            t0 = x0 * cospi_28_64 + x3 *   cospi_4_64;
            t1 = x1 * cospi_12_64 + x2 *  cospi_20_64;
            t2 = x2 * cospi_12_64 + x1 * -cospi_20_64;
            t3 = x3 * cospi_28_64 + x0 *  -cospi_4_64;
            out[2] = (tran_low_t)fdct_round_shift(t0);
            out[6] = (tran_low_t)fdct_round_shift(t2);
            out[10] = (tran_low_t)fdct_round_shift(t1);
            out[14] = (tran_low_t)fdct_round_shift(t3);
        }

        // step 2
        temp1 = (step1[5] - step1[2]) * cospi_16_64;
        temp2 = (step1[4] - step1[3]) * cospi_16_64;
        step2[2] = fdct_round_shift(temp1);
        step2[3] = fdct_round_shift(temp2);
        temp1 = (step1[4] + step1[3]) * cospi_16_64;
        temp2 = (step1[5] + step1[2]) * cospi_16_64;
        step2[4] = fdct_round_shift(temp1);
        step2[5] = fdct_round_shift(temp2);

        // step 3
        step3[0] = step1[0] + step2[3];
        step3[1] = step1[1] + step2[2];
        step3[2] = step1[1] - step2[2];
        step3[3] = step1[0] - step2[3];
        step3[4] = step1[7] - step2[4];
        step3[5] = step1[6] - step2[5];
        step3[6] = step1[6] + step2[5];
        step3[7] = step1[7] + step2[4];

        // step 4
        temp1 = step3[1] *  -cospi_8_64 + step3[6] * cospi_24_64;
        temp2 = step3[2] * cospi_24_64 + step3[5] *  cospi_8_64;
        step2[1] = fdct_round_shift(temp1);
        step2[2] = fdct_round_shift(temp2);
        temp1 = step3[2] * cospi_8_64 - step3[5] * cospi_24_64;
        temp2 = step3[1] * cospi_24_64 + step3[6] *  cospi_8_64;
        step2[5] = fdct_round_shift(temp1);
        step2[6] = fdct_round_shift(temp2);

        // step 5
        step1[0] = step3[0] + step2[1];
        step1[1] = step3[0] - step2[1];
        step1[2] = step3[3] + step2[2];
        step1[3] = step3[3] - step2[2];
        step1[4] = step3[4] - step2[5];
        step1[5] = step3[4] + step2[5];
        step1[6] = step3[7] - step2[6];
        step1[7] = step3[7] + step2[6];

        // step 6
        temp1 = step1[0] * cospi_30_64 + step1[7] *  cospi_2_64;
        temp2 = step1[1] * cospi_14_64 + step1[6] * cospi_18_64;
        out[1] = (tran_low_t)fdct_round_shift(temp1);
        out[9] = (tran_low_t)fdct_round_shift(temp2);

        temp1 = step1[2] * cospi_22_64 + step1[5] * cospi_10_64;
        temp2 = step1[3] *  cospi_6_64 + step1[4] * cospi_26_64;
        out[5] = (tran_low_t)fdct_round_shift(temp1);
        out[13] = (tran_low_t)fdct_round_shift(temp2);

        temp1 = step1[3] * -cospi_26_64 + step1[4] *  cospi_6_64;
        temp2 = step1[2] * -cospi_10_64 + step1[5] * cospi_22_64;
        out[3] = (tran_low_t)fdct_round_shift(temp1);
        out[11] = (tran_low_t)fdct_round_shift(temp2);

        temp1 = step1[1] * -cospi_18_64 + step1[6] * cospi_14_64;
        temp2 = step1[0] *  -cospi_2_64 + step1[7] * cospi_30_64;
        out[7] = (tran_low_t)fdct_round_shift(temp1);
        out[15] = (tran_low_t)fdct_round_shift(temp2);
    }

    static void fdct8(const tran_low_t *input, tran_low_t *output)
    {
        tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
        tran_high_t t0, t1, t2, t3;                  // needs32
        tran_high_t x0, x1, x2, x3;                  // canbe16

        // stage 1
        s0 = input[0] + input[7];
        s1 = input[1] + input[6];
        s2 = input[2] + input[5];
        s3 = input[3] + input[4];
        s4 = input[3] - input[4];
        s5 = input[2] - input[5];
        s6 = input[1] - input[6];
        s7 = input[0] - input[7];

        // fdct4(step, step);
        x0 = s0 + s3;
        x1 = s1 + s2;
        x2 = s1 - s2;
        x3 = s0 - s3;
        t0 = (x0 + x1) * cospi_16_64;
        t1 = (x0 - x1) * cospi_16_64;
        t2 =  x2 * cospi_24_64 + x3 *  cospi_8_64;
        t3 = -x2 * cospi_8_64  + x3 * cospi_24_64;
        output[0] = (tran_low_t)fdct_round_shift(t0);
        output[2] = (tran_low_t)fdct_round_shift(t2);
        output[4] = (tran_low_t)fdct_round_shift(t1);
        output[6] = (tran_low_t)fdct_round_shift(t3);

        // Stage 2
        t0 = (s6 - s5) * cospi_16_64;
        t1 = (s6 + s5) * cospi_16_64;
        t2 = (tran_low_t)fdct_round_shift(t0);
        t3 = (tran_low_t)fdct_round_shift(t1);

        // Stage 3
        x0 = s4 + t2;
        x1 = s4 - t2;
        x2 = s7 - t3;
        x3 = s7 + t3;

        // Stage 4
        t0 = x0 * cospi_28_64 + x3 *   cospi_4_64;
        t1 = x1 * cospi_12_64 + x2 *  cospi_20_64;
        t2 = x2 * cospi_12_64 + x1 * -cospi_20_64;
        t3 = x3 * cospi_28_64 + x0 *  -cospi_4_64;
        output[1] = (tran_low_t)fdct_round_shift(t0);
        output[3] = (tran_low_t)fdct_round_shift(t2);
        output[5] = (tran_low_t)fdct_round_shift(t1);
        output[7] = (tran_low_t)fdct_round_shift(t3);
    }

    static void fdct4(const tran_low_t *input, tran_low_t *output)
    {
        tran_high_t step[4];
        tran_high_t temp1, temp2;

        step[0] = input[0] + input[3];
        step[1] = input[1] + input[2];
        step[2] = input[1] - input[2];
        step[3] = input[0] - input[3];

        temp1 = (step[0] + step[1]) * cospi_16_64;
        temp2 = (step[0] - step[1]) * cospi_16_64;
        output[0] = (tran_low_t)fdct_round_shift(temp1);
        output[2] = (tran_low_t)fdct_round_shift(temp2);
        temp1 = step[2] * cospi_24_64 + step[3] * cospi_8_64;
        temp2 = -step[2] * cospi_8_64 + step[3] * cospi_24_64;
        output[1] = (tran_low_t)fdct_round_shift(temp1);
        output[3] = (tran_low_t)fdct_round_shift(temp2);
    }

    typedef void (*transform_1d)(const tran_low_t*, tran_low_t*);

    typedef struct {
        transform_1d cols, rows;  // vertical and horizontal
    } transform_2d;

    static const transform_2d FHT_4[] = {
        { fdct4,  fdct4  },  // DCT_DCT  = 0
        { fadst4, fdct4  },  // ADST_DCT = 1
        { fdct4,  fadst4 },  // DCT_ADST = 2
        { fadst4, fadst4 }   // ADST_ADST = 3
    };

    static const transform_2d FHT_8[] = {
        { fdct8,  fdct8  },  // DCT_DCT  = 0
        { fadst8, fdct8  },  // ADST_DCT = 1
        { fdct8,  fadst8 },  // DCT_ADST = 2
        { fadst8, fadst8 }   // ADST_ADST = 3
    };

    static const transform_2d FHT_16[] = {
        { fdct16,  fdct16  },  // DCT_DCT  = 0
        { fadst16, fdct16  },  // ADST_DCT = 1
        { fdct16,  fadst16 },  // DCT_ADST = 2
        { fadst16, fadst16 }   // ADST_ADST = 3
    };

    typedef uint8_t TX_TYPE;
    enum {
        DCT_DCT,    // DCT  in both horizontal and vertical
        ADST_DCT,   // ADST in vertical, DCT in horizontal
        DCT_ADST,   // DCT  in vertical, ADST in horizontal
        ADST_ADST,  // ADST in both directions
        FLIPADST_DCT,
        DCT_FLIPADST,
        FLIPADST_FLIPADST,
        ADST_FLIPADST,
        FLIPADST_ADST,
        IDTX,
        V_DCT,
        H_DCT,
        V_ADST,
        H_ADST,
        V_FLIPADST,
        H_FLIPADST,
        TX_TYPES,
    };

    void fht4x4_px(const int16_t *input, tran_low_t *output, int stride, int tx_type)
    {
        /*if (tx_type == DCT_DCT) {
        vpx_fdct4x4_c(input, output, stride);
        } else */
        {
            tran_low_t out[4 * 4];
            int i, j;
            tran_low_t temp_in[4], temp_out[4];
            const transform_2d ht = FHT_4[tx_type];

            // Columns
            for (i = 0; i < 4; ++i) {
                for (j = 0; j < 4; ++j)
                    temp_in[j] = input[j * stride + i] * 16;
                if (i == 0 && temp_in[0])
                    temp_in[0] += 1;
                ht.cols(temp_in, temp_out);
                for (j = 0; j < 4; ++j)
                    out[j * 4 + i] = temp_out[j];
            }

            // Rows
            for (i = 0; i < 4; ++i) {
                for (j = 0; j < 4; ++j)
                    temp_in[j] = out[j + i * 4];
                ht.rows(temp_in, temp_out);
                for (j = 0; j < 4; ++j)
                    output[j + i * 4] = (temp_out[j] + 1) >> 2;
            }
        }
    }

    void fht8x8_px(const int16_t *input, tran_low_t *output, int stride, int tx_type)
    {
        /*if (tx_type == DCT_DCT) {
        vpx_fdct8x8_c(input, output, stride);
        } else */
        {
            tran_low_t out[64];
            int i, j;
            tran_low_t temp_in[8], temp_out[8];
            const transform_2d ht = FHT_8[tx_type];

            // Columns
            for (i = 0; i < 8; ++i) {
                for (j = 0; j < 8; ++j)
                    temp_in[j] = input[j * stride + i] * 4;
                ht.cols(temp_in, temp_out);
                for (j = 0; j < 8; ++j)
                    out[j * 8 + i] = temp_out[j];
            }

            // Rows
            for (i = 0; i < 8; ++i) {
                for (j = 0; j < 8; ++j)
                    temp_in[j] = out[j + i * 8];
                ht.rows(temp_in, temp_out);
                for (j = 0; j < 8; ++j)
                    output[j + i * 8] = (temp_out[j] + (temp_out[j] < 0)) >> 1;
            }
        }
    }


    // looks like a bug with separated DCT_DCT. use specail function
    void fdct16x16_px(const int16_t *input, int16_t *output, int stride)
    {
        // The 2D transform is done with two passes which are actually pretty
        // similar. In the first one, we transform the columns and transpose
        // the results. In the second one, we transform the rows. To achieve that,
        // as the first pass results are transposed, we transpose the columns (that
        // is the transposed rows) and transpose the results (so that it goes back
        // in normal/row positions).
        int pass;
        // We need an intermediate buffer between passes.
        tran_low_t intermediate[256];
        const int16_t *in_pass0 = input;
        const tran_low_t *in = NULL;
        tran_low_t *out = intermediate;
        // Do the two transform/transpose passes
        for (pass = 0; pass < 2; ++pass) {
            tran_high_t step1[8];      // canbe16
            tran_high_t step2[8];      // canbe16
            tran_high_t step3[8];      // canbe16
            tran_high_t input[8];      // canbe16
            tran_high_t temp1, temp2;  // needs32
            int i;
            for (i = 0; i < 16; i++) {
                if (0 == pass) {
                    // Calculate input for the first 8 results.
                    input[0] = (in_pass0[0 * stride] + in_pass0[15 * stride]) * 4;
                    input[1] = (in_pass0[1 * stride] + in_pass0[14 * stride]) * 4;
                    input[2] = (in_pass0[2 * stride] + in_pass0[13 * stride]) * 4;
                    input[3] = (in_pass0[3 * stride] + in_pass0[12 * stride]) * 4;
                    input[4] = (in_pass0[4 * stride] + in_pass0[11 * stride]) * 4;
                    input[5] = (in_pass0[5 * stride] + in_pass0[10 * stride]) * 4;
                    input[6] = (in_pass0[6 * stride] + in_pass0[9 * stride]) * 4;
                    input[7] = (in_pass0[7 * stride] + in_pass0[8 * stride]) * 4;
                    // Calculate input for the next 8 results.
                    step1[0] = (in_pass0[7 * stride] - in_pass0[8 * stride]) * 4;
                    step1[1] = (in_pass0[6 * stride] - in_pass0[9 * stride]) * 4;
                    step1[2] = (in_pass0[5 * stride] - in_pass0[10 * stride]) * 4;
                    step1[3] = (in_pass0[4 * stride] - in_pass0[11 * stride]) * 4;
                    step1[4] = (in_pass0[3 * stride] - in_pass0[12 * stride]) * 4;
                    step1[5] = (in_pass0[2 * stride] - in_pass0[13 * stride]) * 4;
                    step1[6] = (in_pass0[1 * stride] - in_pass0[14 * stride]) * 4;
                    step1[7] = (in_pass0[0 * stride] - in_pass0[15 * stride]) * 4;
                } else {
                    // Calculate input for the first 8 results.
                    input[0] = ((in[0 * 16] + 1) >> 2) + ((in[15 * 16] + 1) >> 2);
                    input[1] = ((in[1 * 16] + 1) >> 2) + ((in[14 * 16] + 1) >> 2);
                    input[2] = ((in[2 * 16] + 1) >> 2) + ((in[13 * 16] + 1) >> 2);
                    input[3] = ((in[3 * 16] + 1) >> 2) + ((in[12 * 16] + 1) >> 2);
                    input[4] = ((in[4 * 16] + 1) >> 2) + ((in[11 * 16] + 1) >> 2);
                    input[5] = ((in[5 * 16] + 1) >> 2) + ((in[10 * 16] + 1) >> 2);
                    input[6] = ((in[6 * 16] + 1) >> 2) + ((in[9 * 16] + 1) >> 2);
                    input[7] = ((in[7 * 16] + 1) >> 2) + ((in[8 * 16] + 1) >> 2);
                    // Calculate input for the next 8 results.
                    step1[0] = ((in[7 * 16] + 1) >> 2) - ((in[8 * 16] + 1) >> 2);
                    step1[1] = ((in[6 * 16] + 1) >> 2) - ((in[9 * 16] + 1) >> 2);
                    step1[2] = ((in[5 * 16] + 1) >> 2) - ((in[10 * 16] + 1) >> 2);
                    step1[3] = ((in[4 * 16] + 1) >> 2) - ((in[11 * 16] + 1) >> 2);
                    step1[4] = ((in[3 * 16] + 1) >> 2) - ((in[12 * 16] + 1) >> 2);
                    step1[5] = ((in[2 * 16] + 1) >> 2) - ((in[13 * 16] + 1) >> 2);
                    step1[6] = ((in[1 * 16] + 1) >> 2) - ((in[14 * 16] + 1) >> 2);
                    step1[7] = ((in[0 * 16] + 1) >> 2) - ((in[15 * 16] + 1) >> 2);
                }
                // Work on the first eight values; fdct8(input, even_results);
                {
                    tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
                    tran_high_t t0, t1, t2, t3;                  // needs32
                    tran_high_t x0, x1, x2, x3;                  // canbe16

                    // stage 1
                    s0 = input[0] + input[7];
                    s1 = input[1] + input[6];
                    s2 = input[2] + input[5];
                    s3 = input[3] + input[4];
                    s4 = input[3] - input[4];
                    s5 = input[2] - input[5];
                    s6 = input[1] - input[6];
                    s7 = input[0] - input[7];

                    // fdct4(step, step);
                    x0 = s0 + s3;
                    x1 = s1 + s2;
                    x2 = s1 - s2;
                    x3 = s0 - s3;
                    t0 = (x0 + x1) * cospi_16_64;
                    t1 = (x0 - x1) * cospi_16_64;
                    t2 = x3 * cospi_8_64 + x2 * cospi_24_64;
                    t3 = x3 * cospi_24_64 - x2 * cospi_8_64;
                    out[0] = (tran_low_t)fdct_round_shift(t0);
                    out[4] = (tran_low_t)fdct_round_shift(t2);
                    out[8] = (tran_low_t)fdct_round_shift(t1);
                    out[12] = (tran_low_t)fdct_round_shift(t3);

                    // Stage 2
                    t0 = (s6 - s5) * cospi_16_64;
                    t1 = (s6 + s5) * cospi_16_64;
                    t2 = fdct_round_shift(t0);
                    t3 = fdct_round_shift(t1);

                    // Stage 3
                    x0 = s4 + t2;
                    x1 = s4 - t2;
                    x2 = s7 - t3;
                    x3 = s7 + t3;

                    // Stage 4
                    t0 = x0 * cospi_28_64 + x3 * cospi_4_64;
                    t1 = x1 * cospi_12_64 + x2 * cospi_20_64;
                    t2 = x2 * cospi_12_64 + x1 * -cospi_20_64;
                    t3 = x3 * cospi_28_64 + x0 * -cospi_4_64;
                    out[2] = (tran_low_t)fdct_round_shift(t0);
                    out[6] = (tran_low_t)fdct_round_shift(t2);
                    out[10] = (tran_low_t)fdct_round_shift(t1);
                    out[14] = (tran_low_t)fdct_round_shift(t3);
                }
                // Work on the next eight values; step1 -> odd_results
                {
                    // step 2
                    temp1 = (step1[5] - step1[2]) * cospi_16_64;
                    temp2 = (step1[4] - step1[3]) * cospi_16_64;
                    step2[2] = fdct_round_shift(temp1);
                    step2[3] = fdct_round_shift(temp2);
                    temp1 = (step1[4] + step1[3]) * cospi_16_64;
                    temp2 = (step1[5] + step1[2]) * cospi_16_64;
                    step2[4] = fdct_round_shift(temp1);
                    step2[5] = fdct_round_shift(temp2);
                    // step 3
                    step3[0] = step1[0] + step2[3];
                    step3[1] = step1[1] + step2[2];
                    step3[2] = step1[1] - step2[2];
                    step3[3] = step1[0] - step2[3];
                    step3[4] = step1[7] - step2[4];
                    step3[5] = step1[6] - step2[5];
                    step3[6] = step1[6] + step2[5];
                    step3[7] = step1[7] + step2[4];
                    // step 4
                    temp1 = step3[1] * -cospi_8_64 + step3[6] * cospi_24_64;
                    temp2 = step3[2] * cospi_24_64 + step3[5] * cospi_8_64;
                    step2[1] = fdct_round_shift(temp1);
                    step2[2] = fdct_round_shift(temp2);
                    temp1 = step3[2] * cospi_8_64 - step3[5] * cospi_24_64;
                    temp2 = step3[1] * cospi_24_64 + step3[6] * cospi_8_64;
                    step2[5] = fdct_round_shift(temp1);
                    step2[6] = fdct_round_shift(temp2);
                    // step 5
                    step1[0] = step3[0] + step2[1];
                    step1[1] = step3[0] - step2[1];
                    step1[2] = step3[3] + step2[2];
                    step1[3] = step3[3] - step2[2];
                    step1[4] = step3[4] - step2[5];
                    step1[5] = step3[4] + step2[5];
                    step1[6] = step3[7] - step2[6];
                    step1[7] = step3[7] + step2[6];
                    // step 6
                    temp1 = step1[0] * cospi_30_64 + step1[7] * cospi_2_64;
                    temp2 = step1[1] * cospi_14_64 + step1[6] * cospi_18_64;
                    out[1] = (tran_low_t)fdct_round_shift(temp1);
                    out[9] = (tran_low_t)fdct_round_shift(temp2);
                    temp1 = step1[2] * cospi_22_64 + step1[5] * cospi_10_64;
                    temp2 = step1[3] * cospi_6_64 + step1[4] * cospi_26_64;
                    out[5] = (tran_low_t)fdct_round_shift(temp1);
                    out[13] = (tran_low_t)fdct_round_shift(temp2);
                    temp1 = step1[3] * -cospi_26_64 + step1[4] * cospi_6_64;
                    temp2 = step1[2] * -cospi_10_64 + step1[5] * cospi_22_64;
                    out[3] = (tran_low_t)fdct_round_shift(temp1);
                    out[11] = (tran_low_t)fdct_round_shift(temp2);
                    temp1 = step1[1] * -cospi_18_64 + step1[6] * cospi_14_64;
                    temp2 = step1[0] * -cospi_2_64 + step1[7] * cospi_30_64;
                    out[7] = (tran_low_t)fdct_round_shift(temp1);
                    out[15] = (tran_low_t)fdct_round_shift(temp2);
                }
                // Do next column (which is a transposed row in second/horizontal pass)
                in++;
                in_pass0++;
                out += 16;
            }
            // Setup in/out for next pass.
            in = intermediate;
            out = output;
        }
    }


    void fht16x16_px(const int16_t *input, tran_low_t *output, int stride, int tx_type)
    {
        if (tx_type == DCT_DCT) {
            fdct16x16_px(input, output, stride);
        } else {
            tran_low_t out[256];
            int i, j;
            tran_low_t temp_in[16], temp_out[16];
            const transform_2d ht = FHT_16[tx_type];

            // Columns
            for (i = 0; i < 16; ++i) {
                for (j = 0; j < 16; ++j)
                    temp_in[j] = input[j * stride + i] * 4;
                ht.cols(temp_in, temp_out);
                for (j = 0; j < 16; ++j)
                    out[j * 16 + i] = (temp_out[j] + 1 + (temp_out[j] < 0)) >> 2;
            }

            // Rows
            for (i = 0; i < 16; ++i) {
                for (j = 0; j < 16; ++j)
                    temp_in[j] = out[j + i * 16];
                ht.rows(temp_in, temp_out);
                for (j = 0; j < 16; ++j)
                    output[j + i * 16] = temp_out[j];
            }
        }
    }

    static inline tran_high_t dct_32_round(tran_high_t input)
    {
        tran_high_t rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
        // TODO(debargha, peter.derivaz): Find new bounds for this assert,
        // and make the bounds consts.
        // assert(-131072 <= rv && rv <= 131071);
        return rv;
    }

    static inline tran_high_t half_round_shift(tran_high_t input)
    {
        tran_high_t rv = (input + 1 + (input < 0)) >> 2;
        return rv;
    }

    void vpx_fdct32(const tran_high_t *input, tran_high_t *output, int round)
    {
        tran_high_t step[32];
        // Stage 1
        step[0] = input[0] + input[(32 - 1)];
        step[1] = input[1] + input[(32 - 2)];
        step[2] = input[2] + input[(32 - 3)];
        step[3] = input[3] + input[(32 - 4)];
        step[4] = input[4] + input[(32 - 5)];
        step[5] = input[5] + input[(32 - 6)];
        step[6] = input[6] + input[(32 - 7)];
        step[7] = input[7] + input[(32 - 8)];
        step[8] = input[8] + input[(32 - 9)];
        step[9] = input[9] + input[(32 - 10)];
        step[10] = input[10] + input[(32 - 11)];
        step[11] = input[11] + input[(32 - 12)];
        step[12] = input[12] + input[(32 - 13)];
        step[13] = input[13] + input[(32 - 14)];
        step[14] = input[14] + input[(32 - 15)];
        step[15] = input[15] + input[(32 - 16)];
        step[16] = -input[16] + input[(32 - 17)];
        step[17] = -input[17] + input[(32 - 18)];
        step[18] = -input[18] + input[(32 - 19)];
        step[19] = -input[19] + input[(32 - 20)];
        step[20] = -input[20] + input[(32 - 21)];
        step[21] = -input[21] + input[(32 - 22)];
        step[22] = -input[22] + input[(32 - 23)];
        step[23] = -input[23] + input[(32 - 24)];
        step[24] = -input[24] + input[(32 - 25)];
        step[25] = -input[25] + input[(32 - 26)];
        step[26] = -input[26] + input[(32 - 27)];
        step[27] = -input[27] + input[(32 - 28)];
        step[28] = -input[28] + input[(32 - 29)];
        step[29] = -input[29] + input[(32 - 30)];
        step[30] = -input[30] + input[(32 - 31)];
        step[31] = -input[31] + input[(32 - 32)];

        // Stage 2
        output[0] = step[0] + step[16 - 1];
        output[1] = step[1] + step[16 - 2];
        output[2] = step[2] + step[16 - 3];
        output[3] = step[3] + step[16 - 4];
        output[4] = step[4] + step[16 - 5];
        output[5] = step[5] + step[16 - 6];
        output[6] = step[6] + step[16 - 7];
        output[7] = step[7] + step[16 - 8];
        output[8] = -step[8] + step[16 - 9];
        output[9] = -step[9] + step[16 - 10];
        output[10] = -step[10] + step[16 - 11];
        output[11] = -step[11] + step[16 - 12];
        output[12] = -step[12] + step[16 - 13];
        output[13] = -step[13] + step[16 - 14];
        output[14] = -step[14] + step[16 - 15];
        output[15] = -step[15] + step[16 - 16];

        output[16] = step[16];
        output[17] = step[17];
        output[18] = step[18];
        output[19] = step[19];

        output[20] = dct_32_round((-step[20] + step[27]) * cospi_16_64);
        output[21] = dct_32_round((-step[21] + step[26]) * cospi_16_64);
        output[22] = dct_32_round((-step[22] + step[25]) * cospi_16_64);
        output[23] = dct_32_round((-step[23] + step[24]) * cospi_16_64);

        output[24] = dct_32_round((step[24] + step[23]) * cospi_16_64);
        output[25] = dct_32_round((step[25] + step[22]) * cospi_16_64);
        output[26] = dct_32_round((step[26] + step[21]) * cospi_16_64);
        output[27] = dct_32_round((step[27] + step[20]) * cospi_16_64);

        output[28] = step[28];
        output[29] = step[29];
        output[30] = step[30];
        output[31] = step[31];

        // dump the magnitude by 4, hence the intermediate values are within
        // the range of 16 bits.
        if (round) {
            output[0] = half_round_shift(output[0]);
            output[1] = half_round_shift(output[1]);
            output[2] = half_round_shift(output[2]);
            output[3] = half_round_shift(output[3]);
            output[4] = half_round_shift(output[4]);
            output[5] = half_round_shift(output[5]);
            output[6] = half_round_shift(output[6]);
            output[7] = half_round_shift(output[7]);
            output[8] = half_round_shift(output[8]);
            output[9] = half_round_shift(output[9]);
            output[10] = half_round_shift(output[10]);
            output[11] = half_round_shift(output[11]);
            output[12] = half_round_shift(output[12]);
            output[13] = half_round_shift(output[13]);
            output[14] = half_round_shift(output[14]);
            output[15] = half_round_shift(output[15]);

            output[16] = half_round_shift(output[16]);
            output[17] = half_round_shift(output[17]);
            output[18] = half_round_shift(output[18]);
            output[19] = half_round_shift(output[19]);
            output[20] = half_round_shift(output[20]);
            output[21] = half_round_shift(output[21]);
            output[22] = half_round_shift(output[22]);
            output[23] = half_round_shift(output[23]);
            output[24] = half_round_shift(output[24]);
            output[25] = half_round_shift(output[25]);
            output[26] = half_round_shift(output[26]);
            output[27] = half_round_shift(output[27]);
            output[28] = half_round_shift(output[28]);
            output[29] = half_round_shift(output[29]);
            output[30] = half_round_shift(output[30]);
            output[31] = half_round_shift(output[31]);
        }

        // Stage 3
        step[0] = output[0] + output[(8 - 1)];
        step[1] = output[1] + output[(8 - 2)];
        step[2] = output[2] + output[(8 - 3)];
        step[3] = output[3] + output[(8 - 4)];
        step[4] = -output[4] + output[(8 - 5)];
        step[5] = -output[5] + output[(8 - 6)];
        step[6] = -output[6] + output[(8 - 7)];
        step[7] = -output[7] + output[(8 - 8)];
        step[8] = output[8];
        step[9] = output[9];
        step[10] = dct_32_round((-output[10] + output[13]) * cospi_16_64);
        step[11] = dct_32_round((-output[11] + output[12]) * cospi_16_64);
        step[12] = dct_32_round((output[12] + output[11]) * cospi_16_64);
        step[13] = dct_32_round((output[13] + output[10]) * cospi_16_64);
        step[14] = output[14];
        step[15] = output[15];

        step[16] = output[16] + output[23];
        step[17] = output[17] + output[22];
        step[18] = output[18] + output[21];
        step[19] = output[19] + output[20];
        step[20] = -output[20] + output[19];
        step[21] = -output[21] + output[18];
        step[22] = -output[22] + output[17];
        step[23] = -output[23] + output[16];
        step[24] = -output[24] + output[31];
        step[25] = -output[25] + output[30];
        step[26] = -output[26] + output[29];
        step[27] = -output[27] + output[28];
        step[28] = output[28] + output[27];
        step[29] = output[29] + output[26];
        step[30] = output[30] + output[25];
        step[31] = output[31] + output[24];

        // Stage 4
        output[0] = step[0] + step[3];
        output[1] = step[1] + step[2];
        output[2] = -step[2] + step[1];
        output[3] = -step[3] + step[0];
        output[4] = step[4];
        output[5] = dct_32_round((-step[5] + step[6]) * cospi_16_64);
        output[6] = dct_32_round((step[6] + step[5]) * cospi_16_64);
        output[7] = step[7];
        output[8] = step[8] + step[11];
        output[9] = step[9] + step[10];
        output[10] = -step[10] + step[9];
        output[11] = -step[11] + step[8];
        output[12] = -step[12] + step[15];
        output[13] = -step[13] + step[14];
        output[14] = step[14] + step[13];
        output[15] = step[15] + step[12];

        output[16] = step[16];
        output[17] = step[17];
        output[18] = dct_32_round(step[18] * -cospi_8_64 + step[29] * cospi_24_64);
        output[19] = dct_32_round(step[19] * -cospi_8_64 + step[28] * cospi_24_64);
        output[20] = dct_32_round(step[20] * -cospi_24_64 + step[27] * -cospi_8_64);
        output[21] = dct_32_round(step[21] * -cospi_24_64 + step[26] * -cospi_8_64);
        output[22] = step[22];
        output[23] = step[23];
        output[24] = step[24];
        output[25] = step[25];
        output[26] = dct_32_round(step[26] * cospi_24_64 + step[21] * -cospi_8_64);
        output[27] = dct_32_round(step[27] * cospi_24_64 + step[20] * -cospi_8_64);
        output[28] = dct_32_round(step[28] * cospi_8_64 + step[19] * cospi_24_64);
        output[29] = dct_32_round(step[29] * cospi_8_64 + step[18] * cospi_24_64);
        output[30] = step[30];
        output[31] = step[31];

        // Stage 5
        step[0] = dct_32_round((output[0] + output[1]) * cospi_16_64);
        step[1] = dct_32_round((-output[1] + output[0]) * cospi_16_64);
        step[2] = dct_32_round(output[2] * cospi_24_64 + output[3] * cospi_8_64);
        step[3] = dct_32_round(output[3] * cospi_24_64 - output[2] * cospi_8_64);
        step[4] = output[4] + output[5];
        step[5] = -output[5] + output[4];
        step[6] = -output[6] + output[7];
        step[7] = output[7] + output[6];
        step[8] = output[8];
        step[9] = dct_32_round(output[9] * -cospi_8_64 + output[14] * cospi_24_64);
        step[10] = dct_32_round(output[10] * -cospi_24_64 + output[13] * -cospi_8_64);
        step[11] = output[11];
        step[12] = output[12];
        step[13] = dct_32_round(output[13] * cospi_24_64 + output[10] * -cospi_8_64);
        step[14] = dct_32_round(output[14] * cospi_8_64 + output[9] * cospi_24_64);
        step[15] = output[15];

        step[16] = output[16] + output[19];
        step[17] = output[17] + output[18];
        step[18] = -output[18] + output[17];
        step[19] = -output[19] + output[16];
        step[20] = -output[20] + output[23];
        step[21] = -output[21] + output[22];
        step[22] = output[22] + output[21];
        step[23] = output[23] + output[20];
        step[24] = output[24] + output[27];
        step[25] = output[25] + output[26];
        step[26] = -output[26] + output[25];
        step[27] = -output[27] + output[24];
        step[28] = -output[28] + output[31];
        step[29] = -output[29] + output[30];
        step[30] = output[30] + output[29];
        step[31] = output[31] + output[28];

        // Stage 6
        output[0] = step[0];
        output[1] = step[1];
        output[2] = step[2];
        output[3] = step[3];
        output[4] = dct_32_round(step[4] * cospi_28_64 + step[7] * cospi_4_64);
        output[5] = dct_32_round(step[5] * cospi_12_64 + step[6] * cospi_20_64);
        output[6] = dct_32_round(step[6] * cospi_12_64 + step[5] * -cospi_20_64);
        output[7] = dct_32_round(step[7] * cospi_28_64 + step[4] * -cospi_4_64);
        output[8] = step[8] + step[9];
        output[9] = -step[9] + step[8];
        output[10] = -step[10] + step[11];
        output[11] = step[11] + step[10];
        output[12] = step[12] + step[13];
        output[13] = -step[13] + step[12];
        output[14] = -step[14] + step[15];
        output[15] = step[15] + step[14];

        output[16] = step[16];
        output[17] = dct_32_round(step[17] * -cospi_4_64 + step[30] * cospi_28_64);
        output[18] = dct_32_round(step[18] * -cospi_28_64 + step[29] * -cospi_4_64);
        output[19] = step[19];
        output[20] = step[20];
        output[21] = dct_32_round(step[21] * -cospi_20_64 + step[26] * cospi_12_64);
        output[22] = dct_32_round(step[22] * -cospi_12_64 + step[25] * -cospi_20_64);
        output[23] = step[23];
        output[24] = step[24];
        output[25] = dct_32_round(step[25] * cospi_12_64 + step[22] * -cospi_20_64);
        output[26] = dct_32_round(step[26] * cospi_20_64 + step[21] * cospi_12_64);
        output[27] = step[27];
        output[28] = step[28];
        output[29] = dct_32_round(step[29] * cospi_28_64 + step[18] * -cospi_4_64);
        output[30] = dct_32_round(step[30] * cospi_4_64 + step[17] * cospi_28_64);
        output[31] = step[31];

        // Stage 7
        step[0] = output[0];
        step[1] = output[1];
        step[2] = output[2];
        step[3] = output[3];
        step[4] = output[4];
        step[5] = output[5];
        step[6] = output[6];
        step[7] = output[7];
        step[8] = dct_32_round(output[8] * cospi_30_64 + output[15] * cospi_2_64);
        step[9] = dct_32_round(output[9] * cospi_14_64 + output[14] * cospi_18_64);
        step[10] = dct_32_round(output[10] * cospi_22_64 + output[13] * cospi_10_64);
        step[11] = dct_32_round(output[11] * cospi_6_64 + output[12] * cospi_26_64);
        step[12] = dct_32_round(output[12] * cospi_6_64 + output[11] * -cospi_26_64);
        step[13] = dct_32_round(output[13] * cospi_22_64 + output[10] * -cospi_10_64);
        step[14] = dct_32_round(output[14] * cospi_14_64 + output[9] * -cospi_18_64);
        step[15] = dct_32_round(output[15] * cospi_30_64 + output[8] * -cospi_2_64);

        step[16] = output[16] + output[17];
        step[17] = -output[17] + output[16];
        step[18] = -output[18] + output[19];
        step[19] = output[19] + output[18];
        step[20] = output[20] + output[21];
        step[21] = -output[21] + output[20];
        step[22] = -output[22] + output[23];
        step[23] = output[23] + output[22];
        step[24] = output[24] + output[25];
        step[25] = -output[25] + output[24];
        step[26] = -output[26] + output[27];
        step[27] = output[27] + output[26];
        step[28] = output[28] + output[29];
        step[29] = -output[29] + output[28];
        step[30] = -output[30] + output[31];
        step[31] = output[31] + output[30];

        // Final stage --- outputs indices are bit-reversed.
        output[0] = step[0];
        output[16] = step[1];
        output[8] = step[2];
        output[24] = step[3];
        output[4] = step[4];
        output[20] = step[5];
        output[12] = step[6];
        output[28] = step[7];
        output[2] = step[8];
        output[18] = step[9];
        output[10] = step[10];
        output[26] = step[11];
        output[6] = step[12];
        output[22] = step[13];
        output[14] = step[14];
        output[30] = step[15];

        output[1] = dct_32_round(step[16] * cospi_31_64 + step[31] * cospi_1_64);
        output[17] = dct_32_round(step[17] * cospi_15_64 + step[30] * cospi_17_64);
        output[9] = dct_32_round(step[18] * cospi_23_64 + step[29] * cospi_9_64);
        output[25] = dct_32_round(step[19] * cospi_7_64 + step[28] * cospi_25_64);
        output[5] = dct_32_round(step[20] * cospi_27_64 + step[27] * cospi_5_64);
        output[21] = dct_32_round(step[21] * cospi_11_64 + step[26] * cospi_21_64);
        output[13] = dct_32_round(step[22] * cospi_19_64 + step[25] * cospi_13_64);
        output[29] = dct_32_round(step[23] * cospi_3_64 + step[24] * cospi_29_64);
        output[3] = dct_32_round(step[24] * cospi_3_64 + step[23] * -cospi_29_64);
        output[19] = dct_32_round(step[25] * cospi_19_64 + step[22] * -cospi_13_64);
        output[11] = dct_32_round(step[26] * cospi_11_64 + step[21] * -cospi_21_64);
        output[27] = dct_32_round(step[27] * cospi_27_64 + step[20] * -cospi_5_64);
        output[7] = dct_32_round(step[28] * cospi_7_64 + step[19] * -cospi_25_64);
        output[23] = dct_32_round(step[29] * cospi_23_64 + step[18] * -cospi_9_64);
        output[15] = dct_32_round(step[30] * cospi_15_64 + step[17] * -cospi_17_64);
        output[31] = dct_32_round(step[31] * cospi_31_64 + step[16] * -cospi_1_64);
    }

    void fdct32x32_px(const int16_t *input, tran_low_t *out, int stride)
    {
        int i, j;
        tran_high_t output[32 * 32];

        // Columns
        for (i = 0; i < 32; ++i) {
            tran_high_t temp_in[32], temp_out[32];
            for (j = 0; j < 32; ++j) temp_in[j] = input[j * stride + i] * 4;
            vpx_fdct32(temp_in, temp_out, 0);
            for (j = 0; j < 32; ++j)
                output[j * 32 + i] = (temp_out[j] + 1 + (temp_out[j] > 0)) >> 2;
        }

        // Rows
        for (i = 0; i < 32; ++i) {
            tran_high_t temp_in[32], temp_out[32];
            for (j = 0; j < 32; ++j) temp_in[j] = output[j + i * 32];
            vpx_fdct32(temp_in, temp_out, 0);
            for (j = 0; j < 32; ++j)
                out[j + i * 32] =
                (tran_low_t)((temp_out[j] + 1 + (temp_out[j] < 0)) >> 2);
        }
    }

    //-----------------------------------------------------
    //             FWD TRANSFORM HBD
    //-----------------------------------------------------
#define MAX_TXFM_STAGE_NUM 12

    typedef void (*TxfmFunc)(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range);

    typedef enum TXFM_TYPE {
        TXFM_TYPE_DCT4,
        TXFM_TYPE_DCT8,
        TXFM_TYPE_DCT16,
        TXFM_TYPE_DCT32,
        TXFM_TYPE_DCT64,
        TXFM_TYPE_ADST4,
        TXFM_TYPE_ADST8,
        TXFM_TYPE_ADST16,
        TXFM_TYPE_ADST32,
        TXFM_TYPE_IDENTITY4,
        TXFM_TYPE_IDENTITY8,
        TXFM_TYPE_IDENTITY16,
        TXFM_TYPE_IDENTITY32,
        TXFM_TYPE_IDENTITY64,
        TXFM_TYPES,
        TXFM_TYPE_INVALID,
    } TXFM_TYPE;

    // block transform size
    typedef enum {
        TX_4X4,    // 4x4 transform
        TX_8X8,    // 8x8 transform
        TX_16X16,  // 16x16 transform
        TX_32X32,  // 32x32 transform
        TX_64X64,  // 64x64 transform
        TX_4X8,    // 4x8 transform
        TX_8X4,    // 8x4 transform
        TX_8X16,   // 8x16 transform
        TX_16X8,   // 16x8 transform
        TX_16X32,  // 16x32 transform
        TX_32X16,  // 32x16 transform
        TX_32X64,           // 32x64 transform
        TX_64X32,           // 64x32 transform
        TX_4X16,            // 4x16 transform
        TX_16X4,            // 16x4 transform
        TX_8X32,            // 8x32 transform
        TX_32X8,            // 32x8 transform
        TX_16X64,           // 16x64 transform
        TX_64X16,           // 64x16 transform
        TX_SIZES_ALL,       // Includes rectangular transforms
        TX_SIZES = TX_4X8,  // Does NOT include rectangular transforms
        TX_INVALID = 255    // Invalid transform size
    } TX_SIZE;

    typedef struct TXFM_2D_FLIP_CFG {
        TX_SIZE tx_size;
        int ud_flip;  // flip upside down
        int lr_flip;  // flip left to right
        const int8_t *shift;
        int8_t cos_bit_col;
        int8_t cos_bit_row;
        int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
        int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
        TXFM_TYPE txfm_type_col;
        TXFM_TYPE txfm_type_row;
        int stage_num_col;
        int stage_num_row;
    } TXFM_2D_FLIP_CFG;

    // 1D tx types
    typedef enum {
        DCT_1D,
        ADST_1D,
        FLIPADST_1D,
        IDTX_1D,
        // TODO(sarahparker) need to eventually put something here for the
        // mrc experiment to make this work with the ext-tx pruning functions
        TX_TYPES_1D,
    } TX_TYPE_1D;

    static inline void set_flip_cfg(int32_t tx_type, TXFM_2D_FLIP_CFG *cfg)
    {
        switch (tx_type) {
        case DCT_DCT:
        case ADST_DCT:
        case DCT_ADST:
        case ADST_ADST:
            cfg->ud_flip = 0;
            cfg->lr_flip = 0;
            break;
        case IDTX:
        case V_DCT:
        case H_DCT:
        case V_ADST:
        case H_ADST:
            cfg->ud_flip = 0;
            cfg->lr_flip = 0;
            break;
        case FLIPADST_DCT:
        case FLIPADST_ADST:
        case V_FLIPADST:
            cfg->ud_flip = 1;
            cfg->lr_flip = 0;
            break;
        case DCT_FLIPADST:
        case ADST_FLIPADST:
        case H_FLIPADST:
            cfg->ud_flip = 0;
            cfg->lr_flip = 1;
            break;
        case FLIPADST_FLIPADST:
            cfg->ud_flip = 1;
            cfg->lr_flip = 1;
            break;
        default:
            cfg->ud_flip = 0;
            cfg->lr_flip = 0;
            assert(0);
        }
    }

    static const TX_TYPE_1D vtx_tab[TX_TYPES] = {
        DCT_1D,      ADST_1D, DCT_1D,      ADST_1D,
        FLIPADST_1D, DCT_1D,  FLIPADST_1D, ADST_1D, FLIPADST_1D, IDTX_1D,
        DCT_1D,      IDTX_1D, ADST_1D,     IDTX_1D, FLIPADST_1D, IDTX_1D,
    };

    static const TX_TYPE_1D htx_tab[TX_TYPES] = {
        DCT_1D,  DCT_1D,      ADST_1D,     ADST_1D,
        DCT_1D,  FLIPADST_1D, FLIPADST_1D, FLIPADST_1D, ADST_1D, IDTX_1D,
        IDTX_1D, DCT_1D,      IDTX_1D,     ADST_1D,     IDTX_1D, FLIPADST_1D,
    };

    static const int tx_size_wide[TX_SIZES_ALL] = { 4, 8, 16, 32, 64, 4, 8, 8, 16, 16, 32, 32, 64, 4, 16, 8, 32, 16, 64, };
    static const int tx_size_high[TX_SIZES_ALL] = { 4, 8, 16, 32, 64, 8, 4, 16, 8, 32, 16, 64, 32, 16, 4, 32, 8, 64, 16, };
    static const int tx_size_wide_log2[TX_SIZES_ALL] = { 2, 3, 4, 5, 6, 2, 3, 3, 4, 4, 5, 5, 6, 2, 4, 3, 5, 4, 6, };
    static const int tx_size_high_log2[TX_SIZES_ALL] = { 2, 3, 4, 5, 6, 3, 2, 4, 3, 5, 4, 6, 5, 4, 2, 5, 3, 6, 4, };

    static const int8_t fwd_shift_4x4[3] = { 2, 0, 0 };
    static const int8_t fwd_shift_8x8[3] = { 2, -1, 0 };
    static const int8_t fwd_shift_16x16[3] = { 2, -2, 0 };
    static const int8_t fwd_shift_32x32[3] = { 2, -4, 0 };
    static const int8_t fwd_shift_64x64[3] = { 0, -2, -2 };
    static const int8_t fwd_shift_4x8[3] = { 2, -1, 0 };
    static const int8_t fwd_shift_8x4[3] = { 2, -1, 0 };
    static const int8_t fwd_shift_8x16[3] = { 2, -2, 0 };
    static const int8_t fwd_shift_16x8[3] = { 2, -2, 0 };
    static const int8_t fwd_shift_16x32[3] = { 2, -4, 0 };
    static const int8_t fwd_shift_32x16[3] = { 2, -4, 0 };
    static const int8_t fwd_shift_32x64[3] = { 0, -2, -2 };
    static const int8_t fwd_shift_64x32[3] = { 2, -4, -2 };
    static const int8_t fwd_shift_4x16[3] = { 2, -1, 0 };
    static const int8_t fwd_shift_16x4[3] = { 2, -1, 0 };
    static const int8_t fwd_shift_8x32[3] = { 2, -2, 0 };
    static const int8_t fwd_shift_32x8[3] = { 2, -2, 0 };
    static const int8_t fwd_shift_16x64[3] = { 0, -2, 0 };
    static const int8_t fwd_shift_64x16[3] = { 2, -4, 0 };

    static const int8_t *fwd_txfm_shift_ls[TX_SIZES_ALL] = {
        fwd_shift_4x4,   fwd_shift_8x8,   fwd_shift_16x16, fwd_shift_32x32,
        fwd_shift_64x64, fwd_shift_4x8,   fwd_shift_8x4,   fwd_shift_8x16,
        fwd_shift_16x8,  fwd_shift_16x32, fwd_shift_32x16, fwd_shift_32x64,
        fwd_shift_64x32, fwd_shift_4x16,  fwd_shift_16x4,  fwd_shift_8x32,
        fwd_shift_32x8,  fwd_shift_16x64, fwd_shift_64x16,
    };

#define MAX_TXWH_IDX 5
    const int8_t fwd_cos_bit_col[MAX_TXWH_IDX][MAX_TXWH_IDX] = {
        { 13, 13, 13, 0, 0 }, { 13, 13, 13, 12, 0 }, { 13, 13, 13, 12, 13 }, { 0, 13, 13, 12, 13 }, { 0, 0, 13, 12, 13 }
    };

    const int8_t fwd_cos_bit_row[MAX_TXWH_IDX] [MAX_TXWH_IDX] = {
        { 13, 13, 12, 0, 0 }, { 13, 13, 13, 12, 0 }, { 13, 13, 12, 13, 12 }, { 0, 12, 13, 12, 11 }, { 0, 0, 12, 11, 10 }
    };

    const TXFM_TYPE av1_txfm_type_ls[5][TX_TYPES_1D] = {
        { TXFM_TYPE_DCT4, TXFM_TYPE_ADST4, TXFM_TYPE_ADST4, TXFM_TYPE_IDENTITY4 },
        { TXFM_TYPE_DCT8, TXFM_TYPE_ADST8, TXFM_TYPE_ADST8, TXFM_TYPE_IDENTITY8 },
        { TXFM_TYPE_DCT16, TXFM_TYPE_ADST16, TXFM_TYPE_ADST16, TXFM_TYPE_IDENTITY16 },
        { TXFM_TYPE_DCT32, TXFM_TYPE_ADST32, TXFM_TYPE_ADST32, TXFM_TYPE_IDENTITY32 },
        { TXFM_TYPE_DCT64, TXFM_TYPE_INVALID, TXFM_TYPE_INVALID,
        TXFM_TYPE_IDENTITY64 }
    };

    const int8_t av1_txfm_stage_num_list[TXFM_TYPES] = {
        4,   // TXFM_TYPE_DCT4
        6,   // TXFM_TYPE_DCT8
        8,   // TXFM_TYPE_DCT16
        10,  // TXFM_TYPE_DCT32
        12,  // TXFM_TYPE_DCT64
        7,   // TXFM_TYPE_ADST4
        8,   // TXFM_TYPE_ADST8
        10,  // TXFM_TYPE_ADST16
        12,  // TXFM_TYPE_ADST32
        1,   // TXFM_TYPE_IDENTITY4
        1,   // TXFM_TYPE_IDENTITY8
        1,   // TXFM_TYPE_IDENTITY16
        1,   // TXFM_TYPE_IDENTITY32
        1,   // TXFM_TYPE_IDENTITY64
    };

#define av1_zero(dest) memset(&(dest), 0, sizeof(dest))

    static inline int get_txw_idx(TX_SIZE tx_size) {
        return tx_size_wide_log2[tx_size] - tx_size_wide_log2[0];
    }
    static inline int get_txh_idx(TX_SIZE tx_size) {
        return tx_size_high_log2[tx_size] - tx_size_high_log2[0];
    }

    const int8_t fdct4_range_mult2[4] = { 0, 2, 3, 3 };
    const int8_t fdct8_range_mult2[6] = { 0, 2, 4, 5, 5, 5 };
    const int8_t fdct16_range_mult2[8] = { 0, 2, 4, 6, 7, 7, 7, 7 };
    const int8_t fdct32_range_mult2[10] = { 0, 2, 4, 6, 8, 9, 9, 9, 9, 9 };
    const int8_t fdct64_range_mult2[12] = { 0, 2, 4, 6, 8, 10, 11, 11, 11, 11, 11, 11 };
    const int8_t fadst4_range_mult2[7] = { 0, 2, 4, 3, 3, 3, 3 };
    const int8_t fadst8_range_mult2[8] = { 0, 0, 1, 3, 3, 5, 5, 5 };
    const int8_t fadst16_range_mult2[10] = { 0, 0, 1, 3, 3, 5, 5, 7, 7, 7 };
    const int8_t fadst32_range_mult2[12] = { 0, 0, 1, 3, 3, 5, 5, 7, 7, 9, 9, 9 };
    const int8_t fidtx4_range_mult2[1] = { 1 };
    const int8_t fidtx8_range_mult2[1] = { 2 };
    const int8_t fidtx16_range_mult2[1] = { 3 };
    const int8_t fidtx32_range_mult2[1] = { 4 };
    const int8_t fidtx64_range_mult2[1] = { 5 };

    static const int8_t *fwd_txfm_range_mult2_list[TXFM_TYPES] = {
        fdct4_range_mult2,   fdct8_range_mult2,   fdct16_range_mult2,
        fdct32_range_mult2,  fdct64_range_mult2,  fadst4_range_mult2,
        fadst8_range_mult2,  fadst16_range_mult2, fadst32_range_mult2,
        fidtx4_range_mult2,  fidtx8_range_mult2,  fidtx16_range_mult2,
        fidtx32_range_mult2, fidtx64_range_mult2
    };

    const int8_t max_fwd_range_mult2_col[5] = { 3, 5, 7, 9, 11 };

    static inline void set_fwd_txfm_non_scale_range(TXFM_2D_FLIP_CFG *cfg) {
        const int txh_idx = get_txh_idx(cfg->tx_size);
        av1_zero(cfg->stage_range_col);
        av1_zero(cfg->stage_range_row);

        if (cfg->txfm_type_col != TXFM_TYPE_INVALID) {
            int stage_num_col = cfg->stage_num_col;
            const int8_t *range_mult2_col =
                fwd_txfm_range_mult2_list[cfg->txfm_type_col];
            for (int i = 0; i < stage_num_col; ++i)
                cfg->stage_range_col[i] = (range_mult2_col[i] + 1) >> 1;
        }

        if (cfg->txfm_type_row != TXFM_TYPE_INVALID) {
            int stage_num_row = cfg->stage_num_row;
            const int8_t *range_mult2_row =
                fwd_txfm_range_mult2_list[cfg->txfm_type_row];
            for (int i = 0; i < stage_num_row; ++i)
                cfg->stage_range_row[i] =
                (max_fwd_range_mult2_col[txh_idx] + range_mult2_row[i] + 1) >> 1;
        }
    }

    void av1_get_fwd_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size, TXFM_2D_FLIP_CFG *cfg) {
        assert(cfg != NULL);
        cfg->tx_size = tx_size;
        set_flip_cfg(tx_type, cfg);
        const TX_TYPE_1D tx_type_1d_col = vtx_tab[tx_type];
        const TX_TYPE_1D tx_type_1d_row = htx_tab[tx_type];
        const int txw_idx = tx_size_wide_log2[tx_size] - tx_size_wide_log2[0];
        const int txh_idx = tx_size_high_log2[tx_size] - tx_size_high_log2[0];
        cfg->shift = fwd_txfm_shift_ls[tx_size];
        cfg->cos_bit_col = fwd_cos_bit_col[txw_idx][txh_idx];
        cfg->cos_bit_row = fwd_cos_bit_row[txw_idx][txh_idx];
        cfg->txfm_type_col = av1_txfm_type_ls[txh_idx][tx_type_1d_col];
        cfg->txfm_type_row = av1_txfm_type_ls[txw_idx][tx_type_1d_row];
        cfg->stage_num_col = av1_txfm_stage_num_list[cfg->txfm_type_col];
        cfg->stage_num_row = av1_txfm_stage_num_list[cfg->txfm_type_row];
        set_fwd_txfm_non_scale_range(cfg);
    }

    void av1_gen_fwd_stage_range(int8_t *stage_range_col, int8_t *stage_range_row, const TXFM_2D_FLIP_CFG *cfg, int bd) {
        // Take the shift from the larger dimension in the rectangular case.
        const int8_t *shift = cfg->shift;
        // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
        for (int i = 0; i < cfg->stage_num_col && i < MAX_TXFM_STAGE_NUM; ++i) {
            stage_range_col[i] = cfg->stage_range_col[i] + shift[0] + bd + 1;
        }

        // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
        for (int i = 0; i < cfg->stage_num_row && i < MAX_TXFM_STAGE_NUM; ++i) {
            stage_range_row[i] = cfg->stage_range_row[i] + shift[0] + shift[1] + bd + 1;
        }
    }

#define range_check(stage, input, buf, size, bit) \
    {                                               \
    (void)stage;                                  \
    (void)input;                                  \
    (void)buf;                                    \
    (void)size;                                   \
    (void)bit;                                    \
    }

    static const int cos_bit_min = 10;
    //static const int cos_bit_max = 16;

    // av1_cospi_arr[i][j] = (int)round(cos(M_PI*j/128) * (1<<(cos_bit_min+i)));
    const int32_t av1_cospi_arr_data[7][64] = {
        { 1024, 1024, 1023, 1021, 1019, 1016, 1013, 1009, 1004, 999, 993, 987, 980,
        972,  964,  955,  946,  936,  926,  915,  903,  891,  878, 865, 851, 837,
        822,  807,  792,  775,  759,  742,  724,  706,  688,  669, 650, 630, 610,
        590,  569,  548,  526,  505,  483,  460,  438,  415,  392, 369, 345, 321,
        297,  273,  249,  224,  200,  175,  150,  125,  100,  75,  50,  25 },
        { 2048, 2047, 2046, 2042, 2038, 2033, 2026, 2018, 2009, 1998, 1987,
        1974, 1960, 1945, 1928, 1911, 1892, 1872, 1851, 1829, 1806, 1782,
        1757, 1730, 1703, 1674, 1645, 1615, 1583, 1551, 1517, 1483, 1448,
        1412, 1375, 1338, 1299, 1260, 1220, 1179, 1138, 1096, 1053, 1009,
        965,  921,  876,  830,  784,  737,  690,  642,  595,  546,  498,
        449,  400,  350,  301,  251,  201,  151,  100,  50 },
        { 4096, 4095, 4091, 4085, 4076, 4065, 4052, 4036, 4017, 3996, 3973,
        3948, 3920, 3889, 3857, 3822, 3784, 3745, 3703, 3659, 3612, 3564,
        3513, 3461, 3406, 3349, 3290, 3229, 3166, 3102, 3035, 2967, 2896,
        2824, 2751, 2675, 2598, 2520, 2440, 2359, 2276, 2191, 2106, 2019,
        1931, 1842, 1751, 1660, 1567, 1474, 1380, 1285, 1189, 1092, 995,
        897,  799,  700,  601,  501,  401,  301,  201,  101 },
        { 8192, 8190, 8182, 8170, 8153, 8130, 8103, 8071, 8035, 7993, 7946,
        7895, 7839, 7779, 7713, 7643, 7568, 7489, 7405, 7317, 7225, 7128,
        7027, 6921, 6811, 6698, 6580, 6458, 6333, 6203, 6070, 5933, 5793,
        5649, 5501, 5351, 5197, 5040, 4880, 4717, 4551, 4383, 4212, 4038,
        3862, 3683, 3503, 3320, 3135, 2948, 2760, 2570, 2378, 2185, 1990,
        1795, 1598, 1401, 1202, 1003, 803,  603,  402,  201 },
        { 16384, 16379, 16364, 16340, 16305, 16261, 16207, 16143, 16069, 15986, 15893,
        15791, 15679, 15557, 15426, 15286, 15137, 14978, 14811, 14635, 14449, 14256,
        14053, 13842, 13623, 13395, 13160, 12916, 12665, 12406, 12140, 11866, 11585,
        11297, 11003, 10702, 10394, 10080, 9760,  9434,  9102,  8765,  8423,  8076,
        7723,  7366,  7005,  6639,  6270,  5897,  5520,  5139,  4756,  4370,  3981,
        3590,  3196,  2801,  2404,  2006,  1606,  1205,  804,   402 },
        { 32768, 32758, 32729, 32679, 32610, 32522, 32413, 32286, 32138, 31972, 31786,
        31581, 31357, 31114, 30853, 30572, 30274, 29957, 29622, 29269, 28899, 28511,
        28106, 27684, 27246, 26791, 26320, 25833, 25330, 24812, 24279, 23732, 23170,
        22595, 22006, 21403, 20788, 20160, 19520, 18868, 18205, 17531, 16846, 16151,
        15447, 14733, 14010, 13279, 12540, 11793, 11039, 10279, 9512,  8740,  7962,
        7180,  6393,  5602,  4808,  4011,  3212,  2411,  1608,  804 },
        { 65536, 65516, 65457, 65358, 65220, 65043, 64827, 64571, 64277, 63944, 63572,
        63162, 62714, 62228, 61705, 61145, 60547, 59914, 59244, 58538, 57798, 57022,
        56212, 55368, 54491, 53581, 52639, 51665, 50660, 49624, 48559, 47464, 46341,
        45190, 44011, 42806, 41576, 40320, 39040, 37736, 36410, 35062, 33692, 32303,
        30893, 29466, 28020, 26558, 25080, 23586, 22078, 20557, 19024, 17479, 15924,
        14359, 12785, 11204, 9616,  8022,  6424,  4821,  3216,  1608 }
    };

    // av1_sinpi_arr_data[i][j] = (int)round((sqrt(2) * sin(j*Pi/9) * 2 / 3) * (1
    // << (cos_bit_min + i))) modified so that elements j=1,2 sum to element j=4.
    const int32_t av1_sinpi_arr_data[7][5] = {
        { 0, 330, 621, 836, 951 },        { 0, 660, 1241, 1672, 1901 },
        { 0, 1321, 2482, 3344, 3803 },    { 0, 2642, 4964, 6689, 7606 },
        { 0, 5283, 9929, 13377, 15212 },  { 0, 10566, 19858, 26755, 30424 },
        { 0, 21133, 39716, 53510, 60849 }
    };

    static inline const int32_t *cospi_arr(int n) {
        return av1_cospi_arr_data[n - cos_bit_min];
    }
    static inline const int32_t *sinpi_arr(int n) {
        return av1_sinpi_arr_data[n - cos_bit_min];
    }

    static inline int32_t round_shift(int32_t value, int bit) {
        assert(bit >= 1);
        return (value + (1 << (bit - 1))) >> bit;
    }

    static inline int32_t half_btf(int32_t w0, int32_t in0, int32_t w1, int32_t in1,
        int bit) {
            int32_t result_32 = w0 * in0 + w1 * in1;
#if CONFIG_COEFFICIENT_RANGE_CHECKING
            int64_t result_64 = (int64_t)w0 * (int64_t)in0 + (int64_t)w1 * (int64_t)in1;
            if (result_64 < INT32_MIN || result_64 > INT32_MAX) {
                printf("%s %d overflow result_32: %d result_64: %" PRId64
                    " w0: %d in0: %d w1: %d in1: "
                    "%d\n",
                    __FILE__, __LINE__, result_32, result_64, w0, in0, w1, in1);
                assert(0 && "half_btf overflow");
            }
#endif
            return round_shift(result_32, bit);
    }

    void av1_fdct4_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        const int32_t size = 4;
        const int32_t *cospi;

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[4];

        // stage 0;
        range_check(stage, input, input, size, stage_range[stage]);

        // stage 1;
        stage++;
        bf1 = output;
        bf1[0] = input[0] + input[3];
        bf1[1] = input[1] + input[2];
        bf1[2] = -input[2] + input[1];
        bf1[3] = -input[3] + input[0];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
        bf1[1] = half_btf(-cospi[32], bf0[1], cospi[32], bf0[0], cos_bit);
        bf1[2] = half_btf(cospi[48], bf0[2], cospi[16], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[48], bf0[3], -cospi[16], bf0[2], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0];
        bf1[1] = bf0[2];
        bf1[2] = bf0[1];
        bf1[3] = bf0[3];
        range_check(stage, input, bf1, size, stage_range[stage]);
    }

    void av1_fdct8_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        const int32_t size = 8;
        const int32_t *cospi;

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[8];

        // stage 0;
        range_check(stage, input, input, size, stage_range[stage]);

        // stage 1;
        stage++;
        bf1 = output;
        bf1[0] = input[0] + input[7];
        bf1[1] = input[1] + input[6];
        bf1[2] = input[2] + input[5];
        bf1[3] = input[3] + input[4];
        bf1[4] = -input[4] + input[3];
        bf1[5] = -input[5] + input[2];
        bf1[6] = -input[6] + input[1];
        bf1[7] = -input[7] + input[0];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0] + bf0[3];
        bf1[1] = bf0[1] + bf0[2];
        bf1[2] = -bf0[2] + bf0[1];
        bf1[3] = -bf0[3] + bf0[0];
        bf1[4] = bf0[4];
        bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
        bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[5], cos_bit);
        bf1[7] = bf0[7];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = step;
        bf1 = output;
        bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
        bf1[1] = half_btf(-cospi[32], bf0[1], cospi[32], bf0[0], cos_bit);
        bf1[2] = half_btf(cospi[48], bf0[2], cospi[16], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[48], bf0[3], -cospi[16], bf0[2], cos_bit);
        bf1[4] = bf0[4] + bf0[5];
        bf1[5] = -bf0[5] + bf0[4];
        bf1[6] = -bf0[6] + bf0[7];
        bf1[7] = bf0[7] + bf0[6];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[56], bf0[4], cospi[8], bf0[7], cos_bit);
        bf1[5] = half_btf(cospi[24], bf0[5], cospi[40], bf0[6], cos_bit);
        bf1[6] = half_btf(cospi[24], bf0[6], -cospi[40], bf0[5], cos_bit);
        bf1[7] = half_btf(cospi[56], bf0[7], -cospi[8], bf0[4], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0];
        bf1[1] = bf0[4];
        bf1[2] = bf0[2];
        bf1[3] = bf0[6];
        bf1[4] = bf0[1];
        bf1[5] = bf0[5];
        bf1[6] = bf0[3];
        bf1[7] = bf0[7];
        range_check(stage, input, bf1, size, stage_range[stage]);
    }

    void av1_fdct16_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        const int32_t size = 16;
        const int32_t *cospi;

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[16];

        // stage 0;
        range_check(stage, input, input, size, stage_range[stage]);

        // stage 1;
        stage++;
        bf1 = output;
        bf1[0] = input[0] + input[15];
        bf1[1] = input[1] + input[14];
        bf1[2] = input[2] + input[13];
        bf1[3] = input[3] + input[12];
        bf1[4] = input[4] + input[11];
        bf1[5] = input[5] + input[10];
        bf1[6] = input[6] + input[9];
        bf1[7] = input[7] + input[8];
        bf1[8] = -input[8] + input[7];
        bf1[9] = -input[9] + input[6];
        bf1[10] = -input[10] + input[5];
        bf1[11] = -input[11] + input[4];
        bf1[12] = -input[12] + input[3];
        bf1[13] = -input[13] + input[2];
        bf1[14] = -input[14] + input[1];
        bf1[15] = -input[15] + input[0];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0] + bf0[7];
        bf1[1] = bf0[1] + bf0[6];
        bf1[2] = bf0[2] + bf0[5];
        bf1[3] = bf0[3] + bf0[4];
        bf1[4] = -bf0[4] + bf0[3];
        bf1[5] = -bf0[5] + bf0[2];
        bf1[6] = -bf0[6] + bf0[1];
        bf1[7] = -bf0[7] + bf0[0];
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = half_btf(-cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
        bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
        bf1[12] = half_btf(cospi[32], bf0[12], cospi[32], bf0[11], cos_bit);
        bf1[13] = half_btf(cospi[32], bf0[13], cospi[32], bf0[10], cos_bit);
        bf1[14] = bf0[14];
        bf1[15] = bf0[15];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[3];
        bf1[1] = bf0[1] + bf0[2];
        bf1[2] = -bf0[2] + bf0[1];
        bf1[3] = -bf0[3] + bf0[0];
        bf1[4] = bf0[4];
        bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
        bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[5], cos_bit);
        bf1[7] = bf0[7];
        bf1[8] = bf0[8] + bf0[11];
        bf1[9] = bf0[9] + bf0[10];
        bf1[10] = -bf0[10] + bf0[9];
        bf1[11] = -bf0[11] + bf0[8];
        bf1[12] = -bf0[12] + bf0[15];
        bf1[13] = -bf0[13] + bf0[14];
        bf1[14] = bf0[14] + bf0[13];
        bf1[15] = bf0[15] + bf0[12];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
        bf1[1] = half_btf(-cospi[32], bf0[1], cospi[32], bf0[0], cos_bit);
        bf1[2] = half_btf(cospi[48], bf0[2], cospi[16], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[48], bf0[3], -cospi[16], bf0[2], cos_bit);
        bf1[4] = bf0[4] + bf0[5];
        bf1[5] = -bf0[5] + bf0[4];
        bf1[6] = -bf0[6] + bf0[7];
        bf1[7] = bf0[7] + bf0[6];
        bf1[8] = bf0[8];
        bf1[9] = half_btf(-cospi[16], bf0[9], cospi[48], bf0[14], cos_bit);
        bf1[10] = half_btf(-cospi[48], bf0[10], -cospi[16], bf0[13], cos_bit);
        bf1[11] = bf0[11];
        bf1[12] = bf0[12];
        bf1[13] = half_btf(cospi[48], bf0[13], -cospi[16], bf0[10], cos_bit);
        bf1[14] = half_btf(cospi[16], bf0[14], cospi[48], bf0[9], cos_bit);
        bf1[15] = bf0[15];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[56], bf0[4], cospi[8], bf0[7], cos_bit);
        bf1[5] = half_btf(cospi[24], bf0[5], cospi[40], bf0[6], cos_bit);
        bf1[6] = half_btf(cospi[24], bf0[6], -cospi[40], bf0[5], cos_bit);
        bf1[7] = half_btf(cospi[56], bf0[7], -cospi[8], bf0[4], cos_bit);
        bf1[8] = bf0[8] + bf0[9];
        bf1[9] = -bf0[9] + bf0[8];
        bf1[10] = -bf0[10] + bf0[11];
        bf1[11] = bf0[11] + bf0[10];
        bf1[12] = bf0[12] + bf0[13];
        bf1[13] = -bf0[13] + bf0[12];
        bf1[14] = -bf0[14] + bf0[15];
        bf1[15] = bf0[15] + bf0[14];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 6
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = bf0[6];
        bf1[7] = bf0[7];
        bf1[8] = half_btf(cospi[60], bf0[8], cospi[4], bf0[15], cos_bit);
        bf1[9] = half_btf(cospi[28], bf0[9], cospi[36], bf0[14], cos_bit);
        bf1[10] = half_btf(cospi[44], bf0[10], cospi[20], bf0[13], cos_bit);
        bf1[11] = half_btf(cospi[12], bf0[11], cospi[52], bf0[12], cos_bit);
        bf1[12] = half_btf(cospi[12], bf0[12], -cospi[52], bf0[11], cos_bit);
        bf1[13] = half_btf(cospi[44], bf0[13], -cospi[20], bf0[10], cos_bit);
        bf1[14] = half_btf(cospi[28], bf0[14], -cospi[36], bf0[9], cos_bit);
        bf1[15] = half_btf(cospi[60], bf0[15], -cospi[4], bf0[8], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 7
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0];
        bf1[1] = bf0[8];
        bf1[2] = bf0[4];
        bf1[3] = bf0[12];
        bf1[4] = bf0[2];
        bf1[5] = bf0[10];
        bf1[6] = bf0[6];
        bf1[7] = bf0[14];
        bf1[8] = bf0[1];
        bf1[9] = bf0[9];
        bf1[10] = bf0[5];
        bf1[11] = bf0[13];
        bf1[12] = bf0[3];
        bf1[13] = bf0[11];
        bf1[14] = bf0[7];
        bf1[15] = bf0[15];
        range_check(stage, input, bf1, size, stage_range[stage]);
    }

    void av1_fdct32_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
  const int32_t size = 32;
  const int32_t *cospi;

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[32];

  // stage 0;
  range_check(stage, input, input, size, stage_range[stage]);

  // stage 1;
  stage++;
  bf1 = output;
  bf1[0] = input[0] + input[31];
  bf1[1] = input[1] + input[30];
  bf1[2] = input[2] + input[29];
  bf1[3] = input[3] + input[28];
  bf1[4] = input[4] + input[27];
  bf1[5] = input[5] + input[26];
  bf1[6] = input[6] + input[25];
  bf1[7] = input[7] + input[24];
  bf1[8] = input[8] + input[23];
  bf1[9] = input[9] + input[22];
  bf1[10] = input[10] + input[21];
  bf1[11] = input[11] + input[20];
  bf1[12] = input[12] + input[19];
  bf1[13] = input[13] + input[18];
  bf1[14] = input[14] + input[17];
  bf1[15] = input[15] + input[16];
  bf1[16] = -input[16] + input[15];
  bf1[17] = -input[17] + input[14];
  bf1[18] = -input[18] + input[13];
  bf1[19] = -input[19] + input[12];
  bf1[20] = -input[20] + input[11];
  bf1[21] = -input[21] + input[10];
  bf1[22] = -input[22] + input[9];
  bf1[23] = -input[23] + input[8];
  bf1[24] = -input[24] + input[7];
  bf1[25] = -input[25] + input[6];
  bf1[26] = -input[26] + input[5];
  bf1[27] = -input[27] + input[4];
  bf1[28] = -input[28] + input[3];
  bf1[29] = -input[29] + input[2];
  bf1[30] = -input[30] + input[1];
  bf1[31] = -input[31] + input[0];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  cospi = cospi_arr(cos_bit);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0] + bf0[15];
  bf1[1] = bf0[1] + bf0[14];
  bf1[2] = bf0[2] + bf0[13];
  bf1[3] = bf0[3] + bf0[12];
  bf1[4] = bf0[4] + bf0[11];
  bf1[5] = bf0[5] + bf0[10];
  bf1[6] = bf0[6] + bf0[9];
  bf1[7] = bf0[7] + bf0[8];
  bf1[8] = -bf0[8] + bf0[7];
  bf1[9] = -bf0[9] + bf0[6];
  bf1[10] = -bf0[10] + bf0[5];
  bf1[11] = -bf0[11] + bf0[4];
  bf1[12] = -bf0[12] + bf0[3];
  bf1[13] = -bf0[13] + bf0[2];
  bf1[14] = -bf0[14] + bf0[1];
  bf1[15] = -bf0[15] + bf0[0];
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = bf0[18];
  bf1[19] = bf0[19];
  bf1[20] = half_btf(-cospi[32], bf0[20], cospi[32], bf0[27], cos_bit);
  bf1[21] = half_btf(-cospi[32], bf0[21], cospi[32], bf0[26], cos_bit);
  bf1[22] = half_btf(-cospi[32], bf0[22], cospi[32], bf0[25], cos_bit);
  bf1[23] = half_btf(-cospi[32], bf0[23], cospi[32], bf0[24], cos_bit);
  bf1[24] = half_btf(cospi[32], bf0[24], cospi[32], bf0[23], cos_bit);
  bf1[25] = half_btf(cospi[32], bf0[25], cospi[32], bf0[22], cos_bit);
  bf1[26] = half_btf(cospi[32], bf0[26], cospi[32], bf0[21], cos_bit);
  bf1[27] = half_btf(cospi[32], bf0[27], cospi[32], bf0[20], cos_bit);
  bf1[28] = bf0[28];
  bf1[29] = bf0[29];
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  cospi = cospi_arr(cos_bit);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0] + bf0[7];
  bf1[1] = bf0[1] + bf0[6];
  bf1[2] = bf0[2] + bf0[5];
  bf1[3] = bf0[3] + bf0[4];
  bf1[4] = -bf0[4] + bf0[3];
  bf1[5] = -bf0[5] + bf0[2];
  bf1[6] = -bf0[6] + bf0[1];
  bf1[7] = -bf0[7] + bf0[0];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf(-cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
  bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
  bf1[12] = half_btf(cospi[32], bf0[12], cospi[32], bf0[11], cos_bit);
  bf1[13] = half_btf(cospi[32], bf0[13], cospi[32], bf0[10], cos_bit);
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = bf0[16] + bf0[23];
  bf1[17] = bf0[17] + bf0[22];
  bf1[18] = bf0[18] + bf0[21];
  bf1[19] = bf0[19] + bf0[20];
  bf1[20] = -bf0[20] + bf0[19];
  bf1[21] = -bf0[21] + bf0[18];
  bf1[22] = -bf0[22] + bf0[17];
  bf1[23] = -bf0[23] + bf0[16];
  bf1[24] = -bf0[24] + bf0[31];
  bf1[25] = -bf0[25] + bf0[30];
  bf1[26] = -bf0[26] + bf0[29];
  bf1[27] = -bf0[27] + bf0[28];
  bf1[28] = bf0[28] + bf0[27];
  bf1[29] = bf0[29] + bf0[26];
  bf1[30] = bf0[30] + bf0[25];
  bf1[31] = bf0[31] + bf0[24];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  cospi = cospi_arr(cos_bit);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0] + bf0[3];
  bf1[1] = bf0[1] + bf0[2];
  bf1[2] = -bf0[2] + bf0[1];
  bf1[3] = -bf0[3] + bf0[0];
  bf1[4] = bf0[4];
  bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
  bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[5], cos_bit);
  bf1[7] = bf0[7];
  bf1[8] = bf0[8] + bf0[11];
  bf1[9] = bf0[9] + bf0[10];
  bf1[10] = -bf0[10] + bf0[9];
  bf1[11] = -bf0[11] + bf0[8];
  bf1[12] = -bf0[12] + bf0[15];
  bf1[13] = -bf0[13] + bf0[14];
  bf1[14] = bf0[14] + bf0[13];
  bf1[15] = bf0[15] + bf0[12];
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = half_btf(-cospi[16], bf0[18], cospi[48], bf0[29], cos_bit);
  bf1[19] = half_btf(-cospi[16], bf0[19], cospi[48], bf0[28], cos_bit);
  bf1[20] = half_btf(-cospi[48], bf0[20], -cospi[16], bf0[27], cos_bit);
  bf1[21] = half_btf(-cospi[48], bf0[21], -cospi[16], bf0[26], cos_bit);
  bf1[22] = bf0[22];
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = bf0[25];
  bf1[26] = half_btf(cospi[48], bf0[26], -cospi[16], bf0[21], cos_bit);
  bf1[27] = half_btf(cospi[48], bf0[27], -cospi[16], bf0[20], cos_bit);
  bf1[28] = half_btf(cospi[16], bf0[28], cospi[48], bf0[19], cos_bit);
  bf1[29] = half_btf(cospi[16], bf0[29], cospi[48], bf0[18], cos_bit);
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  cospi = cospi_arr(cos_bit);
  bf0 = step;
  bf1 = output;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
  bf1[1] = half_btf(-cospi[32], bf0[1], cospi[32], bf0[0], cos_bit);
  bf1[2] = half_btf(cospi[48], bf0[2], cospi[16], bf0[3], cos_bit);
  bf1[3] = half_btf(cospi[48], bf0[3], -cospi[16], bf0[2], cos_bit);
  bf1[4] = bf0[4] + bf0[5];
  bf1[5] = -bf0[5] + bf0[4];
  bf1[6] = -bf0[6] + bf0[7];
  bf1[7] = bf0[7] + bf0[6];
  bf1[8] = bf0[8];
  bf1[9] = half_btf(-cospi[16], bf0[9], cospi[48], bf0[14], cos_bit);
  bf1[10] = half_btf(-cospi[48], bf0[10], -cospi[16], bf0[13], cos_bit);
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = half_btf(cospi[48], bf0[13], -cospi[16], bf0[10], cos_bit);
  bf1[14] = half_btf(cospi[16], bf0[14], cospi[48], bf0[9], cos_bit);
  bf1[15] = bf0[15];
  bf1[16] = bf0[16] + bf0[19];
  bf1[17] = bf0[17] + bf0[18];
  bf1[18] = -bf0[18] + bf0[17];
  bf1[19] = -bf0[19] + bf0[16];
  bf1[20] = -bf0[20] + bf0[23];
  bf1[21] = -bf0[21] + bf0[22];
  bf1[22] = bf0[22] + bf0[21];
  bf1[23] = bf0[23] + bf0[20];
  bf1[24] = bf0[24] + bf0[27];
  bf1[25] = bf0[25] + bf0[26];
  bf1[26] = -bf0[26] + bf0[25];
  bf1[27] = -bf0[27] + bf0[24];
  bf1[28] = -bf0[28] + bf0[31];
  bf1[29] = -bf0[29] + bf0[30];
  bf1[30] = bf0[30] + bf0[29];
  bf1[31] = bf0[31] + bf0[28];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  cospi = cospi_arr(cos_bit);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[56], bf0[4], cospi[8], bf0[7], cos_bit);
  bf1[5] = half_btf(cospi[24], bf0[5], cospi[40], bf0[6], cos_bit);
  bf1[6] = half_btf(cospi[24], bf0[6], -cospi[40], bf0[5], cos_bit);
  bf1[7] = half_btf(cospi[56], bf0[7], -cospi[8], bf0[4], cos_bit);
  bf1[8] = bf0[8] + bf0[9];
  bf1[9] = -bf0[9] + bf0[8];
  bf1[10] = -bf0[10] + bf0[11];
  bf1[11] = bf0[11] + bf0[10];
  bf1[12] = bf0[12] + bf0[13];
  bf1[13] = -bf0[13] + bf0[12];
  bf1[14] = -bf0[14] + bf0[15];
  bf1[15] = bf0[15] + bf0[14];
  bf1[16] = bf0[16];
  bf1[17] = half_btf(-cospi[8], bf0[17], cospi[56], bf0[30], cos_bit);
  bf1[18] = half_btf(-cospi[56], bf0[18], -cospi[8], bf0[29], cos_bit);
  bf1[19] = bf0[19];
  bf1[20] = bf0[20];
  bf1[21] = half_btf(-cospi[40], bf0[21], cospi[24], bf0[26], cos_bit);
  bf1[22] = half_btf(-cospi[24], bf0[22], -cospi[40], bf0[25], cos_bit);
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = half_btf(cospi[24], bf0[25], -cospi[40], bf0[22], cos_bit);
  bf1[26] = half_btf(cospi[40], bf0[26], cospi[24], bf0[21], cos_bit);
  bf1[27] = bf0[27];
  bf1[28] = bf0[28];
  bf1[29] = half_btf(cospi[56], bf0[29], -cospi[8], bf0[18], cos_bit);
  bf1[30] = half_btf(cospi[8], bf0[30], cospi[56], bf0[17], cos_bit);
  bf1[31] = bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  cospi = cospi_arr(cos_bit);
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = half_btf(cospi[60], bf0[8], cospi[4], bf0[15], cos_bit);
  bf1[9] = half_btf(cospi[28], bf0[9], cospi[36], bf0[14], cos_bit);
  bf1[10] = half_btf(cospi[44], bf0[10], cospi[20], bf0[13], cos_bit);
  bf1[11] = half_btf(cospi[12], bf0[11], cospi[52], bf0[12], cos_bit);
  bf1[12] = half_btf(cospi[12], bf0[12], -cospi[52], bf0[11], cos_bit);
  bf1[13] = half_btf(cospi[44], bf0[13], -cospi[20], bf0[10], cos_bit);
  bf1[14] = half_btf(cospi[28], bf0[14], -cospi[36], bf0[9], cos_bit);
  bf1[15] = half_btf(cospi[60], bf0[15], -cospi[4], bf0[8], cos_bit);
  bf1[16] = bf0[16] + bf0[17];
  bf1[17] = -bf0[17] + bf0[16];
  bf1[18] = -bf0[18] + bf0[19];
  bf1[19] = bf0[19] + bf0[18];
  bf1[20] = bf0[20] + bf0[21];
  bf1[21] = -bf0[21] + bf0[20];
  bf1[22] = -bf0[22] + bf0[23];
  bf1[23] = bf0[23] + bf0[22];
  bf1[24] = bf0[24] + bf0[25];
  bf1[25] = -bf0[25] + bf0[24];
  bf1[26] = -bf0[26] + bf0[27];
  bf1[27] = bf0[27] + bf0[26];
  bf1[28] = bf0[28] + bf0[29];
  bf1[29] = -bf0[29] + bf0[28];
  bf1[30] = -bf0[30] + bf0[31];
  bf1[31] = bf0[31] + bf0[30];
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 8
  stage++;
  cospi = cospi_arr(cos_bit);
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = half_btf(cospi[62], bf0[16], cospi[2], bf0[31], cos_bit);
  bf1[17] = half_btf(cospi[30], bf0[17], cospi[34], bf0[30], cos_bit);
  bf1[18] = half_btf(cospi[46], bf0[18], cospi[18], bf0[29], cos_bit);
  bf1[19] = half_btf(cospi[14], bf0[19], cospi[50], bf0[28], cos_bit);
  bf1[20] = half_btf(cospi[54], bf0[20], cospi[10], bf0[27], cos_bit);
  bf1[21] = half_btf(cospi[22], bf0[21], cospi[42], bf0[26], cos_bit);
  bf1[22] = half_btf(cospi[38], bf0[22], cospi[26], bf0[25], cos_bit);
  bf1[23] = half_btf(cospi[6], bf0[23], cospi[58], bf0[24], cos_bit);
  bf1[24] = half_btf(cospi[6], bf0[24], -cospi[58], bf0[23], cos_bit);
  bf1[25] = half_btf(cospi[38], bf0[25], -cospi[26], bf0[22], cos_bit);
  bf1[26] = half_btf(cospi[22], bf0[26], -cospi[42], bf0[21], cos_bit);
  bf1[27] = half_btf(cospi[54], bf0[27], -cospi[10], bf0[20], cos_bit);
  bf1[28] = half_btf(cospi[14], bf0[28], -cospi[50], bf0[19], cos_bit);
  bf1[29] = half_btf(cospi[46], bf0[29], -cospi[18], bf0[18], cos_bit);
  bf1[30] = half_btf(cospi[30], bf0[30], -cospi[34], bf0[17], cos_bit);
  bf1[31] = half_btf(cospi[62], bf0[31], -cospi[2], bf0[16], cos_bit);
  range_check(stage, input, bf1, size, stage_range[stage]);

  // stage 9
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = bf0[16];
  bf1[2] = bf0[8];
  bf1[3] = bf0[24];
  bf1[4] = bf0[4];
  bf1[5] = bf0[20];
  bf1[6] = bf0[12];
  bf1[7] = bf0[28];
  bf1[8] = bf0[2];
  bf1[9] = bf0[18];
  bf1[10] = bf0[10];
  bf1[11] = bf0[26];
  bf1[12] = bf0[6];
  bf1[13] = bf0[22];
  bf1[14] = bf0[14];
  bf1[15] = bf0[30];
  bf1[16] = bf0[1];
  bf1[17] = bf0[17];
  bf1[18] = bf0[9];
  bf1[19] = bf0[25];
  bf1[20] = bf0[5];
  bf1[21] = bf0[21];
  bf1[22] = bf0[13];
  bf1[23] = bf0[29];
  bf1[24] = bf0[3];
  bf1[25] = bf0[19];
  bf1[26] = bf0[11];
  bf1[27] = bf0[27];
  bf1[28] = bf0[7];
  bf1[29] = bf0[23];
  bf1[30] = bf0[15];
  bf1[31] = bf0[31];
  range_check(stage, input, bf1, size, stage_range[stage]);
}

    void av1_fadst4_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        int bit = cos_bit;
        const int32_t *sinpi = sinpi_arr(bit);
        int32_t x0, x1, x2, x3;
        int32_t s0, s1, s2, s3, s4, s5, s6, s7;

        // stage 0
        range_check(0, input, input, 4, stage_range[0]);
        x0 = input[0];
        x1 = input[1];
        x2 = input[2];
        x3 = input[3];

        if (!(x0 | x1 | x2 | x3)) {
            output[0] = output[1] = output[2] = output[3] = 0;
            return;
        }

        // stage 1
        s0 = range_check_value(sinpi[1] * x0, bit + stage_range[1]);
        s1 = range_check_value(sinpi[4] * x0, bit + stage_range[1]);
        s2 = range_check_value(sinpi[2] * x1, bit + stage_range[1]);
        s3 = range_check_value(sinpi[1] * x1, bit + stage_range[1]);
        s4 = range_check_value(sinpi[3] * x2, bit + stage_range[1]);
        s5 = range_check_value(sinpi[4] * x3, bit + stage_range[1]);
        s6 = range_check_value(sinpi[2] * x3, bit + stage_range[1]);
        s7 = range_check_value(x0 + x1, stage_range[1]);

        // stage 2
        s7 = range_check_value(s7 - x3, stage_range[2]);

        // stage 3
        x0 = range_check_value(s0 + s2, bit + stage_range[3]);
        x1 = range_check_value(sinpi[3] * s7, bit + stage_range[3]);
        x2 = range_check_value(s1 - s3, bit + stage_range[3]);
        x3 = range_check_value(s4, bit + stage_range[3]);

        // stage 4
        x0 = range_check_value(x0 + s5, bit + stage_range[4]);
        x2 = range_check_value(x2 + s6, bit + stage_range[4]);

        // stage 5
        s0 = range_check_value(x0 + x3, bit + stage_range[5]);
        s1 = range_check_value(x1, bit + stage_range[5]);
        s2 = range_check_value(x2 - x3, bit + stage_range[5]);
        s3 = range_check_value(x2 - x0, bit + stage_range[5]);

        // stage 6
        s3 = range_check_value(s3 + x3, bit + stage_range[6]);

        // 1-D transform scaling factor is sqrt(2).
        output[0] = round_shift(s0, bit);
        output[1] = round_shift(s1, bit);
        output[2] = round_shift(s2, bit);
        output[3] = round_shift(s3, bit);
        range_check(6, input, output, 4, stage_range[6]);
    }

    void av1_fadst8_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        const int32_t size = 8;
        const int32_t *cospi;

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[8];

        // stage 0;
        range_check(stage, input, input, size, stage_range[stage]);

        // stage 1;
        stage++;
        assert(output != input);
        bf1 = output;
        bf1[0] = input[0];
        bf1[1] = -input[7];
        bf1[2] = -input[3];
        bf1[3] = input[4];
        bf1[4] = -input[1];
        bf1[5] = input[6];
        bf1[6] = input[2];
        bf1[7] = -input[5];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit);
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[32], bf0[6], -cospi[32], bf0[7], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[2];
        bf1[1] = bf0[1] + bf0[3];
        bf1[2] = bf0[0] - bf0[2];
        bf1[3] = bf0[1] - bf0[3];
        bf1[4] = bf0[4] + bf0[6];
        bf1[5] = bf0[5] + bf0[7];
        bf1[6] = bf0[4] - bf0[6];
        bf1[7] = bf0[5] - bf0[7];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit);
        bf1[5] = half_btf(cospi[48], bf0[4], -cospi[16], bf0[5], cos_bit);
        bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[16], bf0[6], cospi[48], bf0[7], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[4];
        bf1[1] = bf0[1] + bf0[5];
        bf1[2] = bf0[2] + bf0[6];
        bf1[3] = bf0[3] + bf0[7];
        bf1[4] = bf0[0] - bf0[4];
        bf1[5] = bf0[1] - bf0[5];
        bf1[6] = bf0[2] - bf0[6];
        bf1[7] = bf0[3] - bf0[7];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 6
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = half_btf(cospi[4], bf0[0], cospi[60], bf0[1], cos_bit);
        bf1[1] = half_btf(cospi[60], bf0[0], -cospi[4], bf0[1], cos_bit);
        bf1[2] = half_btf(cospi[20], bf0[2], cospi[44], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[44], bf0[2], -cospi[20], bf0[3], cos_bit);
        bf1[4] = half_btf(cospi[36], bf0[4], cospi[28], bf0[5], cos_bit);
        bf1[5] = half_btf(cospi[28], bf0[4], -cospi[36], bf0[5], cos_bit);
        bf1[6] = half_btf(cospi[52], bf0[6], cospi[12], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[12], bf0[6], -cospi[52], bf0[7], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 7
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[1];
        bf1[1] = bf0[6];
        bf1[2] = bf0[3];
        bf1[3] = bf0[4];
        bf1[4] = bf0[5];
        bf1[5] = bf0[2];
        bf1[6] = bf0[7];
        bf1[7] = bf0[0];
        range_check(stage, input, bf1, size, stage_range[stage]);
    }

    void av1_fadst16_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        const int32_t size = 16;
        const int32_t *cospi;

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[16];

        // stage 0;
        range_check(stage, input, input, size, stage_range[stage]);

        // stage 1;
        stage++;
        assert(output != input);
        bf1 = output;
        bf1[0] = input[0];
        bf1[1] = -input[15];
        bf1[2] = -input[7];
        bf1[3] = input[8];
        bf1[4] = -input[3];
        bf1[5] = input[12];
        bf1[6] = input[4];
        bf1[7] = -input[11];
        bf1[8] = -input[1];
        bf1[9] = input[14];
        bf1[10] = input[6];
        bf1[11] = -input[9];
        bf1[12] = input[2];
        bf1[13] = -input[13];
        bf1[14] = -input[5];
        bf1[15] = input[10];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit);
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[32], bf0[6], -cospi[32], bf0[7], cos_bit);
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = half_btf(cospi[32], bf0[10], cospi[32], bf0[11], cos_bit);
        bf1[11] = half_btf(cospi[32], bf0[10], -cospi[32], bf0[11], cos_bit);
        bf1[12] = bf0[12];
        bf1[13] = bf0[13];
        bf1[14] = half_btf(cospi[32], bf0[14], cospi[32], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[32], bf0[14], -cospi[32], bf0[15], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[2];
        bf1[1] = bf0[1] + bf0[3];
        bf1[2] = bf0[0] - bf0[2];
        bf1[3] = bf0[1] - bf0[3];
        bf1[4] = bf0[4] + bf0[6];
        bf1[5] = bf0[5] + bf0[7];
        bf1[6] = bf0[4] - bf0[6];
        bf1[7] = bf0[5] - bf0[7];
        bf1[8] = bf0[8] + bf0[10];
        bf1[9] = bf0[9] + bf0[11];
        bf1[10] = bf0[8] - bf0[10];
        bf1[11] = bf0[9] - bf0[11];
        bf1[12] = bf0[12] + bf0[14];
        bf1[13] = bf0[13] + bf0[15];
        bf1[14] = bf0[12] - bf0[14];
        bf1[15] = bf0[13] - bf0[15];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit);
        bf1[5] = half_btf(cospi[48], bf0[4], -cospi[16], bf0[5], cos_bit);
        bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[16], bf0[6], cospi[48], bf0[7], cos_bit);
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = bf0[10];
        bf1[11] = bf0[11];
        bf1[12] = half_btf(cospi[16], bf0[12], cospi[48], bf0[13], cos_bit);
        bf1[13] = half_btf(cospi[48], bf0[12], -cospi[16], bf0[13], cos_bit);
        bf1[14] = half_btf(-cospi[48], bf0[14], cospi[16], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[16], bf0[14], cospi[48], bf0[15], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[4];
        bf1[1] = bf0[1] + bf0[5];
        bf1[2] = bf0[2] + bf0[6];
        bf1[3] = bf0[3] + bf0[7];
        bf1[4] = bf0[0] - bf0[4];
        bf1[5] = bf0[1] - bf0[5];
        bf1[6] = bf0[2] - bf0[6];
        bf1[7] = bf0[3] - bf0[7];
        bf1[8] = bf0[8] + bf0[12];
        bf1[9] = bf0[9] + bf0[13];
        bf1[10] = bf0[10] + bf0[14];
        bf1[11] = bf0[11] + bf0[15];
        bf1[12] = bf0[8] - bf0[12];
        bf1[13] = bf0[9] - bf0[13];
        bf1[14] = bf0[10] - bf0[14];
        bf1[15] = bf0[11] - bf0[15];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 6
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = bf0[6];
        bf1[7] = bf0[7];
        bf1[8] = half_btf(cospi[8], bf0[8], cospi[56], bf0[9], cos_bit);
        bf1[9] = half_btf(cospi[56], bf0[8], -cospi[8], bf0[9], cos_bit);
        bf1[10] = half_btf(cospi[40], bf0[10], cospi[24], bf0[11], cos_bit);
        bf1[11] = half_btf(cospi[24], bf0[10], -cospi[40], bf0[11], cos_bit);
        bf1[12] = half_btf(-cospi[56], bf0[12], cospi[8], bf0[13], cos_bit);
        bf1[13] = half_btf(cospi[8], bf0[12], cospi[56], bf0[13], cos_bit);
        bf1[14] = half_btf(-cospi[24], bf0[14], cospi[40], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[40], bf0[14], cospi[24], bf0[15], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 7
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[8];
        bf1[1] = bf0[1] + bf0[9];
        bf1[2] = bf0[2] + bf0[10];
        bf1[3] = bf0[3] + bf0[11];
        bf1[4] = bf0[4] + bf0[12];
        bf1[5] = bf0[5] + bf0[13];
        bf1[6] = bf0[6] + bf0[14];
        bf1[7] = bf0[7] + bf0[15];
        bf1[8] = bf0[0] - bf0[8];
        bf1[9] = bf0[1] - bf0[9];
        bf1[10] = bf0[2] - bf0[10];
        bf1[11] = bf0[3] - bf0[11];
        bf1[12] = bf0[4] - bf0[12];
        bf1[13] = bf0[5] - bf0[13];
        bf1[14] = bf0[6] - bf0[14];
        bf1[15] = bf0[7] - bf0[15];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 8
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = half_btf(cospi[2], bf0[0], cospi[62], bf0[1], cos_bit);
        bf1[1] = half_btf(cospi[62], bf0[0], -cospi[2], bf0[1], cos_bit);
        bf1[2] = half_btf(cospi[10], bf0[2], cospi[54], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[54], bf0[2], -cospi[10], bf0[3], cos_bit);
        bf1[4] = half_btf(cospi[18], bf0[4], cospi[46], bf0[5], cos_bit);
        bf1[5] = half_btf(cospi[46], bf0[4], -cospi[18], bf0[5], cos_bit);
        bf1[6] = half_btf(cospi[26], bf0[6], cospi[38], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[38], bf0[6], -cospi[26], bf0[7], cos_bit);
        bf1[8] = half_btf(cospi[34], bf0[8], cospi[30], bf0[9], cos_bit);
        bf1[9] = half_btf(cospi[30], bf0[8], -cospi[34], bf0[9], cos_bit);
        bf1[10] = half_btf(cospi[42], bf0[10], cospi[22], bf0[11], cos_bit);
        bf1[11] = half_btf(cospi[22], bf0[10], -cospi[42], bf0[11], cos_bit);
        bf1[12] = half_btf(cospi[50], bf0[12], cospi[14], bf0[13], cos_bit);
        bf1[13] = half_btf(cospi[14], bf0[12], -cospi[50], bf0[13], cos_bit);
        bf1[14] = half_btf(cospi[58], bf0[14], cospi[6], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[6], bf0[14], -cospi[58], bf0[15], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 9
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[1];
        bf1[1] = bf0[14];
        bf1[2] = bf0[3];
        bf1[3] = bf0[12];
        bf1[4] = bf0[5];
        bf1[5] = bf0[10];
        bf1[6] = bf0[7];
        bf1[7] = bf0[8];
        bf1[8] = bf0[9];
        bf1[9] = bf0[6];
        bf1[10] = bf0[11];
        bf1[11] = bf0[4];
        bf1[12] = bf0[13];
        bf1[13] = bf0[2];
        bf1[14] = bf0[15];
        bf1[15] = bf0[0];
        range_check(stage, input, bf1, size, stage_range[stage]);
    }

    void av1_fadst32_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        const int32_t size = 32;
        const int32_t *cospi;

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[32];

        // stage 0;
        range_check(stage, input, input, size, stage_range[stage]);

        // stage 1;
        stage++;
        bf1 = output;
        bf1[0] = input[31];
        bf1[1] = input[0];
        bf1[2] = input[29];
        bf1[3] = input[2];
        bf1[4] = input[27];
        bf1[5] = input[4];
        bf1[6] = input[25];
        bf1[7] = input[6];
        bf1[8] = input[23];
        bf1[9] = input[8];
        bf1[10] = input[21];
        bf1[11] = input[10];
        bf1[12] = input[19];
        bf1[13] = input[12];
        bf1[14] = input[17];
        bf1[15] = input[14];
        bf1[16] = input[15];
        bf1[17] = input[16];
        bf1[18] = input[13];
        bf1[19] = input[18];
        bf1[20] = input[11];
        bf1[21] = input[20];
        bf1[22] = input[9];
        bf1[23] = input[22];
        bf1[24] = input[7];
        bf1[25] = input[24];
        bf1[26] = input[5];
        bf1[27] = input[26];
        bf1[28] = input[3];
        bf1[29] = input[28];
        bf1[30] = input[1];
        bf1[31] = input[30];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = half_btf(cospi[1], bf0[0], cospi[63], bf0[1], cos_bit);
        bf1[1] = half_btf(-cospi[1], bf0[1], cospi[63], bf0[0], cos_bit);
        bf1[2] = half_btf(cospi[5], bf0[2], cospi[59], bf0[3], cos_bit);
        bf1[3] = half_btf(-cospi[5], bf0[3], cospi[59], bf0[2], cos_bit);
        bf1[4] = half_btf(cospi[9], bf0[4], cospi[55], bf0[5], cos_bit);
        bf1[5] = half_btf(-cospi[9], bf0[5], cospi[55], bf0[4], cos_bit);
        bf1[6] = half_btf(cospi[13], bf0[6], cospi[51], bf0[7], cos_bit);
        bf1[7] = half_btf(-cospi[13], bf0[7], cospi[51], bf0[6], cos_bit);
        bf1[8] = half_btf(cospi[17], bf0[8], cospi[47], bf0[9], cos_bit);
        bf1[9] = half_btf(-cospi[17], bf0[9], cospi[47], bf0[8], cos_bit);
        bf1[10] = half_btf(cospi[21], bf0[10], cospi[43], bf0[11], cos_bit);
        bf1[11] = half_btf(-cospi[21], bf0[11], cospi[43], bf0[10], cos_bit);
        bf1[12] = half_btf(cospi[25], bf0[12], cospi[39], bf0[13], cos_bit);
        bf1[13] = half_btf(-cospi[25], bf0[13], cospi[39], bf0[12], cos_bit);
        bf1[14] = half_btf(cospi[29], bf0[14], cospi[35], bf0[15], cos_bit);
        bf1[15] = half_btf(-cospi[29], bf0[15], cospi[35], bf0[14], cos_bit);
        bf1[16] = half_btf(cospi[33], bf0[16], cospi[31], bf0[17], cos_bit);
        bf1[17] = half_btf(-cospi[33], bf0[17], cospi[31], bf0[16], cos_bit);
        bf1[18] = half_btf(cospi[37], bf0[18], cospi[27], bf0[19], cos_bit);
        bf1[19] = half_btf(-cospi[37], bf0[19], cospi[27], bf0[18], cos_bit);
        bf1[20] = half_btf(cospi[41], bf0[20], cospi[23], bf0[21], cos_bit);
        bf1[21] = half_btf(-cospi[41], bf0[21], cospi[23], bf0[20], cos_bit);
        bf1[22] = half_btf(cospi[45], bf0[22], cospi[19], bf0[23], cos_bit);
        bf1[23] = half_btf(-cospi[45], bf0[23], cospi[19], bf0[22], cos_bit);
        bf1[24] = half_btf(cospi[49], bf0[24], cospi[15], bf0[25], cos_bit);
        bf1[25] = half_btf(-cospi[49], bf0[25], cospi[15], bf0[24], cos_bit);
        bf1[26] = half_btf(cospi[53], bf0[26], cospi[11], bf0[27], cos_bit);
        bf1[27] = half_btf(-cospi[53], bf0[27], cospi[11], bf0[26], cos_bit);
        bf1[28] = half_btf(cospi[57], bf0[28], cospi[7], bf0[29], cos_bit);
        bf1[29] = half_btf(-cospi[57], bf0[29], cospi[7], bf0[28], cos_bit);
        bf1[30] = half_btf(cospi[61], bf0[30], cospi[3], bf0[31], cos_bit);
        bf1[31] = half_btf(-cospi[61], bf0[31], cospi[3], bf0[30], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[16];
        bf1[1] = bf0[1] + bf0[17];
        bf1[2] = bf0[2] + bf0[18];
        bf1[3] = bf0[3] + bf0[19];
        bf1[4] = bf0[4] + bf0[20];
        bf1[5] = bf0[5] + bf0[21];
        bf1[6] = bf0[6] + bf0[22];
        bf1[7] = bf0[7] + bf0[23];
        bf1[8] = bf0[8] + bf0[24];
        bf1[9] = bf0[9] + bf0[25];
        bf1[10] = bf0[10] + bf0[26];
        bf1[11] = bf0[11] + bf0[27];
        bf1[12] = bf0[12] + bf0[28];
        bf1[13] = bf0[13] + bf0[29];
        bf1[14] = bf0[14] + bf0[30];
        bf1[15] = bf0[15] + bf0[31];
        bf1[16] = -bf0[16] + bf0[0];
        bf1[17] = -bf0[17] + bf0[1];
        bf1[18] = -bf0[18] + bf0[2];
        bf1[19] = -bf0[19] + bf0[3];
        bf1[20] = -bf0[20] + bf0[4];
        bf1[21] = -bf0[21] + bf0[5];
        bf1[22] = -bf0[22] + bf0[6];
        bf1[23] = -bf0[23] + bf0[7];
        bf1[24] = -bf0[24] + bf0[8];
        bf1[25] = -bf0[25] + bf0[9];
        bf1[26] = -bf0[26] + bf0[10];
        bf1[27] = -bf0[27] + bf0[11];
        bf1[28] = -bf0[28] + bf0[12];
        bf1[29] = -bf0[29] + bf0[13];
        bf1[30] = -bf0[30] + bf0[14];
        bf1[31] = -bf0[31] + bf0[15];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = bf0[6];
        bf1[7] = bf0[7];
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = bf0[10];
        bf1[11] = bf0[11];
        bf1[12] = bf0[12];
        bf1[13] = bf0[13];
        bf1[14] = bf0[14];
        bf1[15] = bf0[15];
        bf1[16] = half_btf(cospi[4], bf0[16], cospi[60], bf0[17], cos_bit);
        bf1[17] = half_btf(-cospi[4], bf0[17], cospi[60], bf0[16], cos_bit);
        bf1[18] = half_btf(cospi[20], bf0[18], cospi[44], bf0[19], cos_bit);
        bf1[19] = half_btf(-cospi[20], bf0[19], cospi[44], bf0[18], cos_bit);
        bf1[20] = half_btf(cospi[36], bf0[20], cospi[28], bf0[21], cos_bit);
        bf1[21] = half_btf(-cospi[36], bf0[21], cospi[28], bf0[20], cos_bit);
        bf1[22] = half_btf(cospi[52], bf0[22], cospi[12], bf0[23], cos_bit);
        bf1[23] = half_btf(-cospi[52], bf0[23], cospi[12], bf0[22], cos_bit);
        bf1[24] = half_btf(-cospi[60], bf0[24], cospi[4], bf0[25], cos_bit);
        bf1[25] = half_btf(cospi[60], bf0[25], cospi[4], bf0[24], cos_bit);
        bf1[26] = half_btf(-cospi[44], bf0[26], cospi[20], bf0[27], cos_bit);
        bf1[27] = half_btf(cospi[44], bf0[27], cospi[20], bf0[26], cos_bit);
        bf1[28] = half_btf(-cospi[28], bf0[28], cospi[36], bf0[29], cos_bit);
        bf1[29] = half_btf(cospi[28], bf0[29], cospi[36], bf0[28], cos_bit);
        bf1[30] = half_btf(-cospi[12], bf0[30], cospi[52], bf0[31], cos_bit);
        bf1[31] = half_btf(cospi[12], bf0[31], cospi[52], bf0[30], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[8];
        bf1[1] = bf0[1] + bf0[9];
        bf1[2] = bf0[2] + bf0[10];
        bf1[3] = bf0[3] + bf0[11];
        bf1[4] = bf0[4] + bf0[12];
        bf1[5] = bf0[5] + bf0[13];
        bf1[6] = bf0[6] + bf0[14];
        bf1[7] = bf0[7] + bf0[15];
        bf1[8] = -bf0[8] + bf0[0];
        bf1[9] = -bf0[9] + bf0[1];
        bf1[10] = -bf0[10] + bf0[2];
        bf1[11] = -bf0[11] + bf0[3];
        bf1[12] = -bf0[12] + bf0[4];
        bf1[13] = -bf0[13] + bf0[5];
        bf1[14] = -bf0[14] + bf0[6];
        bf1[15] = -bf0[15] + bf0[7];
        bf1[16] = bf0[16] + bf0[24];
        bf1[17] = bf0[17] + bf0[25];
        bf1[18] = bf0[18] + bf0[26];
        bf1[19] = bf0[19] + bf0[27];
        bf1[20] = bf0[20] + bf0[28];
        bf1[21] = bf0[21] + bf0[29];
        bf1[22] = bf0[22] + bf0[30];
        bf1[23] = bf0[23] + bf0[31];
        bf1[24] = -bf0[24] + bf0[16];
        bf1[25] = -bf0[25] + bf0[17];
        bf1[26] = -bf0[26] + bf0[18];
        bf1[27] = -bf0[27] + bf0[19];
        bf1[28] = -bf0[28] + bf0[20];
        bf1[29] = -bf0[29] + bf0[21];
        bf1[30] = -bf0[30] + bf0[22];
        bf1[31] = -bf0[31] + bf0[23];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 6
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = bf0[6];
        bf1[7] = bf0[7];
        bf1[8] = half_btf(cospi[8], bf0[8], cospi[56], bf0[9], cos_bit);
        bf1[9] = half_btf(-cospi[8], bf0[9], cospi[56], bf0[8], cos_bit);
        bf1[10] = half_btf(cospi[40], bf0[10], cospi[24], bf0[11], cos_bit);
        bf1[11] = half_btf(-cospi[40], bf0[11], cospi[24], bf0[10], cos_bit);
        bf1[12] = half_btf(-cospi[56], bf0[12], cospi[8], bf0[13], cos_bit);
        bf1[13] = half_btf(cospi[56], bf0[13], cospi[8], bf0[12], cos_bit);
        bf1[14] = half_btf(-cospi[24], bf0[14], cospi[40], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[24], bf0[15], cospi[40], bf0[14], cos_bit);
        bf1[16] = bf0[16];
        bf1[17] = bf0[17];
        bf1[18] = bf0[18];
        bf1[19] = bf0[19];
        bf1[20] = bf0[20];
        bf1[21] = bf0[21];
        bf1[22] = bf0[22];
        bf1[23] = bf0[23];
        bf1[24] = half_btf(cospi[8], bf0[24], cospi[56], bf0[25], cos_bit);
        bf1[25] = half_btf(-cospi[8], bf0[25], cospi[56], bf0[24], cos_bit);
        bf1[26] = half_btf(cospi[40], bf0[26], cospi[24], bf0[27], cos_bit);
        bf1[27] = half_btf(-cospi[40], bf0[27], cospi[24], bf0[26], cos_bit);
        bf1[28] = half_btf(-cospi[56], bf0[28], cospi[8], bf0[29], cos_bit);
        bf1[29] = half_btf(cospi[56], bf0[29], cospi[8], bf0[28], cos_bit);
        bf1[30] = half_btf(-cospi[24], bf0[30], cospi[40], bf0[31], cos_bit);
        bf1[31] = half_btf(cospi[24], bf0[31], cospi[40], bf0[30], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 7
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[4];
        bf1[1] = bf0[1] + bf0[5];
        bf1[2] = bf0[2] + bf0[6];
        bf1[3] = bf0[3] + bf0[7];
        bf1[4] = -bf0[4] + bf0[0];
        bf1[5] = -bf0[5] + bf0[1];
        bf1[6] = -bf0[6] + bf0[2];
        bf1[7] = -bf0[7] + bf0[3];
        bf1[8] = bf0[8] + bf0[12];
        bf1[9] = bf0[9] + bf0[13];
        bf1[10] = bf0[10] + bf0[14];
        bf1[11] = bf0[11] + bf0[15];
        bf1[12] = -bf0[12] + bf0[8];
        bf1[13] = -bf0[13] + bf0[9];
        bf1[14] = -bf0[14] + bf0[10];
        bf1[15] = -bf0[15] + bf0[11];
        bf1[16] = bf0[16] + bf0[20];
        bf1[17] = bf0[17] + bf0[21];
        bf1[18] = bf0[18] + bf0[22];
        bf1[19] = bf0[19] + bf0[23];
        bf1[20] = -bf0[20] + bf0[16];
        bf1[21] = -bf0[21] + bf0[17];
        bf1[22] = -bf0[22] + bf0[18];
        bf1[23] = -bf0[23] + bf0[19];
        bf1[24] = bf0[24] + bf0[28];
        bf1[25] = bf0[25] + bf0[29];
        bf1[26] = bf0[26] + bf0[30];
        bf1[27] = bf0[27] + bf0[31];
        bf1[28] = -bf0[28] + bf0[24];
        bf1[29] = -bf0[29] + bf0[25];
        bf1[30] = -bf0[30] + bf0[26];
        bf1[31] = -bf0[31] + bf0[27];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 8
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit);
        bf1[5] = half_btf(-cospi[16], bf0[5], cospi[48], bf0[4], cos_bit);
        bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[48], bf0[7], cospi[16], bf0[6], cos_bit);
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = bf0[10];
        bf1[11] = bf0[11];
        bf1[12] = half_btf(cospi[16], bf0[12], cospi[48], bf0[13], cos_bit);
        bf1[13] = half_btf(-cospi[16], bf0[13], cospi[48], bf0[12], cos_bit);
        bf1[14] = half_btf(-cospi[48], bf0[14], cospi[16], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[48], bf0[15], cospi[16], bf0[14], cos_bit);
        bf1[16] = bf0[16];
        bf1[17] = bf0[17];
        bf1[18] = bf0[18];
        bf1[19] = bf0[19];
        bf1[20] = half_btf(cospi[16], bf0[20], cospi[48], bf0[21], cos_bit);
        bf1[21] = half_btf(-cospi[16], bf0[21], cospi[48], bf0[20], cos_bit);
        bf1[22] = half_btf(-cospi[48], bf0[22], cospi[16], bf0[23], cos_bit);
        bf1[23] = half_btf(cospi[48], bf0[23], cospi[16], bf0[22], cos_bit);
        bf1[24] = bf0[24];
        bf1[25] = bf0[25];
        bf1[26] = bf0[26];
        bf1[27] = bf0[27];
        bf1[28] = half_btf(cospi[16], bf0[28], cospi[48], bf0[29], cos_bit);
        bf1[29] = half_btf(-cospi[16], bf0[29], cospi[48], bf0[28], cos_bit);
        bf1[30] = half_btf(-cospi[48], bf0[30], cospi[16], bf0[31], cos_bit);
        bf1[31] = half_btf(cospi[48], bf0[31], cospi[16], bf0[30], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 9
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[2];
        bf1[1] = bf0[1] + bf0[3];
        bf1[2] = -bf0[2] + bf0[0];
        bf1[3] = -bf0[3] + bf0[1];
        bf1[4] = bf0[4] + bf0[6];
        bf1[5] = bf0[5] + bf0[7];
        bf1[6] = -bf0[6] + bf0[4];
        bf1[7] = -bf0[7] + bf0[5];
        bf1[8] = bf0[8] + bf0[10];
        bf1[9] = bf0[9] + bf0[11];
        bf1[10] = -bf0[10] + bf0[8];
        bf1[11] = -bf0[11] + bf0[9];
        bf1[12] = bf0[12] + bf0[14];
        bf1[13] = bf0[13] + bf0[15];
        bf1[14] = -bf0[14] + bf0[12];
        bf1[15] = -bf0[15] + bf0[13];
        bf1[16] = bf0[16] + bf0[18];
        bf1[17] = bf0[17] + bf0[19];
        bf1[18] = -bf0[18] + bf0[16];
        bf1[19] = -bf0[19] + bf0[17];
        bf1[20] = bf0[20] + bf0[22];
        bf1[21] = bf0[21] + bf0[23];
        bf1[22] = -bf0[22] + bf0[20];
        bf1[23] = -bf0[23] + bf0[21];
        bf1[24] = bf0[24] + bf0[26];
        bf1[25] = bf0[25] + bf0[27];
        bf1[26] = -bf0[26] + bf0[24];
        bf1[27] = -bf0[27] + bf0[25];
        bf1[28] = bf0[28] + bf0[30];
        bf1[29] = bf0[29] + bf0[31];
        bf1[30] = -bf0[30] + bf0[28];
        bf1[31] = -bf0[31] + bf0[29];
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 10
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit);
        bf1[3] = half_btf(-cospi[32], bf0[3], cospi[32], bf0[2], cos_bit);
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit);
        bf1[7] = half_btf(-cospi[32], bf0[7], cospi[32], bf0[6], cos_bit);
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = half_btf(cospi[32], bf0[10], cospi[32], bf0[11], cos_bit);
        bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[10], cos_bit);
        bf1[12] = bf0[12];
        bf1[13] = bf0[13];
        bf1[14] = half_btf(cospi[32], bf0[14], cospi[32], bf0[15], cos_bit);
        bf1[15] = half_btf(-cospi[32], bf0[15], cospi[32], bf0[14], cos_bit);
        bf1[16] = bf0[16];
        bf1[17] = bf0[17];
        bf1[18] = half_btf(cospi[32], bf0[18], cospi[32], bf0[19], cos_bit);
        bf1[19] = half_btf(-cospi[32], bf0[19], cospi[32], bf0[18], cos_bit);
        bf1[20] = bf0[20];
        bf1[21] = bf0[21];
        bf1[22] = half_btf(cospi[32], bf0[22], cospi[32], bf0[23], cos_bit);
        bf1[23] = half_btf(-cospi[32], bf0[23], cospi[32], bf0[22], cos_bit);
        bf1[24] = bf0[24];
        bf1[25] = bf0[25];
        bf1[26] = half_btf(cospi[32], bf0[26], cospi[32], bf0[27], cos_bit);
        bf1[27] = half_btf(-cospi[32], bf0[27], cospi[32], bf0[26], cos_bit);
        bf1[28] = bf0[28];
        bf1[29] = bf0[29];
        bf1[30] = half_btf(cospi[32], bf0[30], cospi[32], bf0[31], cos_bit);
        bf1[31] = half_btf(-cospi[32], bf0[31], cospi[32], bf0[30], cos_bit);
        range_check(stage, input, bf1, size, stage_range[stage]);

        // stage 11
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0];
        bf1[1] = -bf0[16];
        bf1[2] = bf0[24];
        bf1[3] = -bf0[8];
        bf1[4] = bf0[12];
        bf1[5] = -bf0[28];
        bf1[6] = bf0[20];
        bf1[7] = -bf0[4];
        bf1[8] = bf0[6];
        bf1[9] = -bf0[22];
        bf1[10] = bf0[30];
        bf1[11] = -bf0[14];
        bf1[12] = bf0[10];
        bf1[13] = -bf0[26];
        bf1[14] = bf0[18];
        bf1[15] = -bf0[2];
        bf1[16] = bf0[3];
        bf1[17] = -bf0[19];
        bf1[18] = bf0[27];
        bf1[19] = -bf0[11];
        bf1[20] = bf0[15];
        bf1[21] = -bf0[31];
        bf1[22] = bf0[23];
        bf1[23] = -bf0[7];
        bf1[24] = bf0[5];
        bf1[25] = -bf0[21];
        bf1[26] = bf0[29];
        bf1[27] = -bf0[13];
        bf1[28] = bf0[9];
        bf1[29] = -bf0[25];
        bf1[30] = bf0[17];
        bf1[31] = -bf0[1];
        range_check(stage, input, bf1, size, stage_range[stage]);
    }

    static inline TxfmFunc fwd_txfm_type_to_func(TXFM_TYPE txfm_type)
    {
        switch (txfm_type) {
        case TXFM_TYPE_DCT4: return av1_fdct4_new;
        case TXFM_TYPE_DCT8: return av1_fdct8_new;
        case TXFM_TYPE_DCT16: return av1_fdct16_new;
        case TXFM_TYPE_DCT32: return av1_fdct32_new;
        //case TXFM_TYPE_DCT64: return av1_fdct64_new;
        case TXFM_TYPE_ADST4: return av1_fadst4_new;
        case TXFM_TYPE_ADST8: return av1_fadst8_new;
        case TXFM_TYPE_ADST16: return av1_fadst16_new;
        case TXFM_TYPE_ADST32: return av1_fadst32_new;
        //case TXFM_TYPE_IDENTITY4: return av1_fidentity4_c;
        //case TXFM_TYPE_IDENTITY8: return av1_fidentity8_c;
        //case TXFM_TYPE_IDENTITY16: return av1_fidentity16_c;
        //case TXFM_TYPE_IDENTITY32: return av1_fidentity32_c;
        //case TXFM_TYPE_IDENTITY64: return av1_fidentity64_c;
        default: assert(0); return NULL;
        }
    }

    // Utility function that returns the log of the ratio of the col and row
    // sizes.
    static inline int get_rect_tx_log_ratio(int col, int row) {
      if (col == row) return 0;
      if (col > row) {
        if (col == row * 2) return 1;
        if (col == row * 4) return 2;
        assert(0 && "Unsupported transform size");
      } else {
        if (row == col * 2) return -1;
        if (row == col * 4) return -2;
        assert(0 && "Unsupported transform size");
      }
      return 0;  // Invalid
    }

    void av1_round_shift_array_c(int32_t *arr, int size, int bit) {
        int i;
        if (bit == 0) {
            return;
        } else {
            if (bit > 0) {
                for (i = 0; i < size; i++) {
                    arr[i] = round_shift(arr[i], bit);
                }
            } else {
                for (i = 0; i < size; i++) {
                    arr[i] = arr[i] * (1 << (-bit));
                }
            }
        }
    }

    static inline void fwd_txfm2d_c(const int16_t *input, int32_t *output, const int stride,
        const TXFM_2D_FLIP_CFG *cfg, int32_t *buf, int bd)
    {
        int c, r;
        // Note when assigning txfm_size_col, we use the txfm_size from the
        // row configuration and vice versa. This is intentionally done to
        // accurately perform rectangular transforms. When the transform is
        // rectangular, the number of columns will be the same as the
        // txfm_size stored in the row cfg struct. It will make no difference
        // for square transforms.
        const int txfm_size_col = tx_size_wide[cfg->tx_size];
        const int txfm_size_row = tx_size_high[cfg->tx_size];
        // Take the shift from the larger dimension in the rectangular case.
        const int8_t *shift = cfg->shift;
        const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
        int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
        int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
        assert(cfg->stage_num_col <= MAX_TXFM_STAGE_NUM);
        assert(cfg->stage_num_row <= MAX_TXFM_STAGE_NUM);
        av1_gen_fwd_stage_range(stage_range_col, stage_range_row, cfg, bd);

        const int8_t cos_bit_col = cfg->cos_bit_col;
        const int8_t cos_bit_row = cfg->cos_bit_row;
        const TxfmFunc txfm_func_col = fwd_txfm_type_to_func(cfg->txfm_type_col);
        const TxfmFunc txfm_func_row = fwd_txfm_type_to_func(cfg->txfm_type_row);

        // use output buffer as temp buffer
        int32_t *temp_in = output;
        int32_t *temp_out = output + txfm_size_row;

        // Columns
        for (c = 0; c < txfm_size_col; ++c) {
            if (cfg->ud_flip == 0) {
                for (r = 0; r < txfm_size_row; ++r) temp_in[r] = input[r * stride + c];
            } else {
                for (r = 0; r < txfm_size_row; ++r)
                    // flip upside down
                        temp_in[r] = input[(txfm_size_row - r - 1) * stride + c];
            }
            av1_round_shift_array_c(temp_in, txfm_size_row, -shift[0]);
            txfm_func_col(temp_in, temp_out, cos_bit_col, stage_range_col);
            av1_round_shift_array_c(temp_out, txfm_size_row, -shift[1]);
            if (cfg->lr_flip == 0) {
                for (r = 0; r < txfm_size_row; ++r)
                    buf[r * txfm_size_col + c] = temp_out[r];
            } else {
                for (r = 0; r < txfm_size_row; ++r)
                    // flip from left to right
                        buf[r * txfm_size_col + (txfm_size_col - c - 1)] = temp_out[r];
            }
        }

        // Rows
        for (r = 0; r < txfm_size_row; ++r) {
            txfm_func_row(buf + r * txfm_size_col, output + r * txfm_size_col,
                cos_bit_row, stage_range_row);
            av1_round_shift_array_c(output + r * txfm_size_col, txfm_size_col, -shift[2]);
            if (abs(rect_type) == 1) {
                // Multiply everything by Sqrt2 if the transform is rectangular and the
                // size difference is a factor of 2.
                for (c = 0; c < txfm_size_col; ++c)
                    output[r * txfm_size_col + c] =
                    round_shift(output[r * txfm_size_col + c] * NewSqrt2, NewSqrt2Bits);
            }
        }
    }

    void av1_fwd_txfm2d_4x4_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd) {
        int32_t txfm_buf[4 * 4];
        TXFM_2D_FLIP_CFG cfg;
        av1_get_fwd_txfm_cfg(tx_type, TX_4X4, &cfg);
        fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
    }

    void av1_fwd_txfm2d_8x8_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd) {
        int32_t txfm_buf[8 * 8];
        TXFM_2D_FLIP_CFG cfg;
        av1_get_fwd_txfm_cfg(tx_type, TX_8X8, &cfg);
        fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
    }

    void av1_fwd_txfm2d_16x16_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd) {
        int32_t txfm_buf[16 * 16];
        TXFM_2D_FLIP_CFG cfg;
        av1_get_fwd_txfm_cfg(tx_type, TX_16X16, &cfg);
        fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
    }

    void av1_fwd_txfm2d_32x32_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd) {
        int32_t txfm_buf[32 * 32];
        TXFM_2D_FLIP_CFG cfg;
        av1_get_fwd_txfm_cfg(tx_type, TX_32X32, &cfg);
        fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
    }

    //-----------------------------------------------------
    //             INV TRANSFORM
    //-----------------------------------------------------

    // [4x4]
    void idct4_c(const tran_low_t *input, tran_low_t *output)
    {
        tran_low_t step[4];
        tran_high_t temp1, temp2;
        // stage 1
        temp1 = (input[0] + input[2]) * cospi_16_64;
        temp2 = (input[0] - input[2]) * cospi_16_64;
        step[0] = WRAPLOW(dct_const_round_shift(temp1));
        step[1] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = input[1] * cospi_24_64 - input[3] * cospi_8_64;
        temp2 = input[1] * cospi_8_64 + input[3] * cospi_24_64;
        step[2] = WRAPLOW(dct_const_round_shift(temp1));
        step[3] = WRAPLOW(dct_const_round_shift(temp2));

        // stage 2
        output[0] = WRAPLOW(step[0] + step[3]);
        output[1] = WRAPLOW(step[1] + step[2]);
        output[2] = WRAPLOW(step[1] - step[2]);
        output[3] = WRAPLOW(step[0] - step[3]);
    }

    void iadst4_c(const tran_low_t *input, tran_low_t *output)
    {
        tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;

        tran_low_t x0 = input[0];
        tran_low_t x1 = input[1];
        tran_low_t x2 = input[2];
        tran_low_t x3 = input[3];

        if (!(x0 | x1 | x2 | x3)) {
            output[0] = output[1] = output[2] = output[3] = 0;
            return;
        }

        s0 = sinpi_1_9 * x0;
        s1 = sinpi_2_9 * x0;
        s2 = sinpi_3_9 * x1;
        s3 = sinpi_4_9 * x2;
        s4 = sinpi_1_9 * x2;
        s5 = sinpi_2_9 * x3;
        s6 = sinpi_4_9 * x3;
        s7 = WRAPLOW(x0 - x2 + x3);

        s0 = s0 + s3 + s5;
        s1 = s1 - s4 - s6;
        s3 = s2;
        s2 = sinpi_3_9 * s7;

        // 1-D transform scaling factor is sqrt(2).
        // The overall dynamic range is 14b (input) + 14b (multiplication scaling)
        // + 1b (addition) = 29b.
        // Hence the output bit depth is 15b.
        output[0] = WRAPLOW(dct_const_round_shift(s0 + s3));
        output[1] = WRAPLOW(dct_const_round_shift(s1 + s3));
        output[2] = WRAPLOW(dct_const_round_shift(s2));
        output[3] = WRAPLOW(dct_const_round_shift(s0 + s1 - s3));
    }

    void iht4x4_16_add_px(const int16_t *input, uint8_t *dest, int stride, int tx_type)
    {
        const transform_2d IHT_4[] = {
            { idct4_c, idct4_c  },  // DCT_DCT  = 0
            { iadst4_c, idct4_c  },   // ADST_DCT = 1
            { idct4_c, iadst4_c },    // DCT_ADST = 2
            { iadst4_c, iadst4_c }      // ADST_ADST = 3
        };

        int i, j;
        tran_low_t out[4 * 4];
        tran_low_t *outptr = out;
        tran_low_t temp_in[4], temp_out[4];

        // inverse transform row vectors
        for (i = 0; i < 4; ++i) {
            IHT_4[tx_type].rows(input, outptr);
            input  += 4;
            outptr += 4;
        }

        // inverse transform column vectors
        for (i = 0; i < 4; ++i) {
            for (j = 0; j < 4; ++j)
                temp_in[j] = out[j * 4 + i];
            IHT_4[tx_type].cols(temp_in, temp_out);
            for (j = 0; j < 4; ++j) {
                dest[j * stride + i] = clip_pixel_add(dest[j * stride + i],
                    ROUND_POWER_OF_TWO(temp_out[j], 4));
            }
        }
    }

    void iht4x4_16_px(const int16_t *input, int16_t *dest, int stride, int tx_type)
    {
        const transform_2d IHT_4[] = {
            { idct4_c, idct4_c  },  // DCT_DCT  = 0
            { iadst4_c, idct4_c  },   // ADST_DCT = 1
            { idct4_c, iadst4_c },    // DCT_ADST = 2
            { iadst4_c, iadst4_c }      // ADST_ADST = 3
        };

        int i, j;
        tran_low_t out[4 * 4];
        tran_low_t *outptr = out;
        tran_low_t temp_in[4], temp_out[4];

        // inverse transform row vectors
        for (i = 0; i < 4; ++i) {
            IHT_4[tx_type].rows(input, outptr);
            input  += 4;
            outptr += 4;
        }

        // inverse transform column vectors
        for (i = 0; i < 4; ++i) {
            for (j = 0; j < 4; ++j)
                temp_in[j] = out[j * 4 + i];
            IHT_4[tx_type].cols(temp_in, temp_out);
            for (j = 0; j < 4; ++j) {
                dest[j * stride + i] = ROUND_POWER_OF_TWO(temp_out[j], 4);
            }
        }
    }

    // [8x8]
    void idct8_c(const tran_low_t *input, tran_low_t *output)
    {
        tran_low_t step1[8], step2[8];
        tran_high_t temp1, temp2;
        // stage 1
        step1[0] = input[0];
        step1[2] = input[4];
        step1[1] = input[2];
        step1[3] = input[6];
        temp1 = input[1] * cospi_28_64 - input[7] * cospi_4_64;
        temp2 = input[1] * cospi_4_64 + input[7] * cospi_28_64;
        step1[4] = WRAPLOW(dct_const_round_shift(temp1));
        step1[7] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = input[5] * cospi_12_64 - input[3] * cospi_20_64;
        temp2 = input[5] * cospi_20_64 + input[3] * cospi_12_64;
        step1[5] = WRAPLOW(dct_const_round_shift(temp1));
        step1[6] = WRAPLOW(dct_const_round_shift(temp2));

        // stage 2
        temp1 = (step1[0] + step1[2]) * cospi_16_64;
        temp2 = (step1[0] - step1[2]) * cospi_16_64;
        step2[0] = WRAPLOW(dct_const_round_shift(temp1));
        step2[1] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = step1[1] * cospi_24_64 - step1[3] * cospi_8_64;
        temp2 = step1[1] * cospi_8_64 + step1[3] * cospi_24_64;
        step2[2] = WRAPLOW(dct_const_round_shift(temp1));
        step2[3] = WRAPLOW(dct_const_round_shift(temp2));
        step2[4] = WRAPLOW(step1[4] + step1[5]);
        step2[5] = WRAPLOW(step1[4] - step1[5]);
        step2[6] = WRAPLOW(-step1[6] + step1[7]);
        step2[7] = WRAPLOW(step1[6] + step1[7]);

        // stage 3
        step1[0] = WRAPLOW(step2[0] + step2[3]);
        step1[1] = WRAPLOW(step2[1] + step2[2]);
        step1[2] = WRAPLOW(step2[1] - step2[2]);
        step1[3] = WRAPLOW(step2[0] - step2[3]);
        step1[4] = step2[4];
        temp1 = (step2[6] - step2[5]) * cospi_16_64;
        temp2 = (step2[5] + step2[6]) * cospi_16_64;
        step1[5] = WRAPLOW(dct_const_round_shift(temp1));
        step1[6] = WRAPLOW(dct_const_round_shift(temp2));
        step1[7] = step2[7];

        // stage 4
        output[0] = WRAPLOW(step1[0] + step1[7]);
        output[1] = WRAPLOW(step1[1] + step1[6]);
        output[2] = WRAPLOW(step1[2] + step1[5]);
        output[3] = WRAPLOW(step1[3] + step1[4]);
        output[4] = WRAPLOW(step1[3] - step1[4]);
        output[5] = WRAPLOW(step1[2] - step1[5]);
        output[6] = WRAPLOW(step1[1] - step1[6]);
        output[7] = WRAPLOW(step1[0] - step1[7]);
    }

    void iadst8_c(const tran_low_t *input, tran_low_t *output)
    {
        int s0, s1, s2, s3, s4, s5, s6, s7;

        tran_high_t x0 = input[7];
        tran_high_t x1 = input[0];
        tran_high_t x2 = input[5];
        tran_high_t x3 = input[2];
        tran_high_t x4 = input[3];
        tran_high_t x5 = input[4];
        tran_high_t x6 = input[1];
        tran_high_t x7 = input[6];

        if (!(x0 | x1 | x2 | x3 | x4 | x5 | x6 | x7)) {
            output[0] = output[1] = output[2] = output[3] = output[4]
            = output[5] = output[6] = output[7] = 0;
            return;
        }

        // stage 1
        s0 = (int)(cospi_2_64  * x0 + cospi_30_64 * x1);
        s1 = (int)(cospi_30_64 * x0 - cospi_2_64  * x1);
        s2 = (int)(cospi_10_64 * x2 + cospi_22_64 * x3);
        s3 = (int)(cospi_22_64 * x2 - cospi_10_64 * x3);
        s4 = (int)(cospi_18_64 * x4 + cospi_14_64 * x5);
        s5 = (int)(cospi_14_64 * x4 - cospi_18_64 * x5);
        s6 = (int)(cospi_26_64 * x6 + cospi_6_64  * x7);
        s7 = (int)(cospi_6_64  * x6 - cospi_26_64 * x7);

        x0 = WRAPLOW(dct_const_round_shift(s0 + s4));
        x1 = WRAPLOW(dct_const_round_shift(s1 + s5));
        x2 = WRAPLOW(dct_const_round_shift(s2 + s6));
        x3 = WRAPLOW(dct_const_round_shift(s3 + s7));
        x4 = WRAPLOW(dct_const_round_shift(s0 - s4));
        x5 = WRAPLOW(dct_const_round_shift(s1 - s5));
        x6 = WRAPLOW(dct_const_round_shift(s2 - s6));
        x7 = WRAPLOW(dct_const_round_shift(s3 - s7));

        // stage 2
        s0 = (int)x0;
        s1 = (int)x1;
        s2 = (int)x2;
        s3 = (int)x3;
        s4 = (int)(cospi_8_64 * x4 + cospi_24_64 * x5);
        s5 = (int)(cospi_24_64 * x4 - cospi_8_64 * x5);
        s6 = (int)(-cospi_24_64 * x6 + cospi_8_64 * x7);
        s7 = (int)(cospi_8_64 * x6 + cospi_24_64 * x7);

        x0 = WRAPLOW(s0 + s2);
        x1 = WRAPLOW(s1 + s3);
        x2 = WRAPLOW(s0 - s2);
        x3 = WRAPLOW(s1 - s3);
        x4 = WRAPLOW(dct_const_round_shift(s4 + s6));
        x5 = WRAPLOW(dct_const_round_shift(s5 + s7));
        x6 = WRAPLOW(dct_const_round_shift(s4 - s6));
        x7 = WRAPLOW(dct_const_round_shift(s5 - s7));

        // stage 3
        s2 = (int)(cospi_16_64 * (x2 + x3));
        s3 = (int)(cospi_16_64 * (x2 - x3));
        s6 = (int)(cospi_16_64 * (x6 + x7));
        s7 = (int)(cospi_16_64 * (x6 - x7));

        x2 = WRAPLOW(dct_const_round_shift(s2));
        x3 = WRAPLOW(dct_const_round_shift(s3));
        x6 = WRAPLOW(dct_const_round_shift(s6));
        x7 = WRAPLOW(dct_const_round_shift(s7));

        output[0] = WRAPLOW(x0);
        output[1] = WRAPLOW(-x4);
        output[2] = WRAPLOW(x6);
        output[3] = WRAPLOW(-x2);
        output[4] = WRAPLOW(x3);
        output[5] = WRAPLOW(-x7);
        output[6] = WRAPLOW(x5);
        output[7] = WRAPLOW(-x1);
    }

    static const transform_2d IHT_8[] = {
        { idct8_c,  idct8_c  },  // DCT_DCT  = 0
        { iadst8_c, idct8_c  },  // ADST_DCT = 1
        { idct8_c,  iadst8_c },  // DCT_ADST = 2
        { iadst8_c, iadst8_c }   // ADST_ADST = 3
    };

    void iht8x8_64_add_px(const tran_low_t *input, uint8_t *dest, int stride, int tx_type, int eob)
    {
        eob;
        int i, j;
        tran_low_t out[8 * 8];
        tran_low_t *outptr = out;
        tran_low_t temp_in[8], temp_out[8];
        const transform_2d ht = IHT_8[tx_type];

        // inverse transform row vectors
        for (i = 0; i < 8; ++i) {
            ht.rows(input, outptr);
            input += 8;
            outptr += 8;
        }

        // inverse transform column vectors
        for (i = 0; i < 8; ++i) {
            for (j = 0; j < 8; ++j)
                temp_in[j] = out[j * 8 + i];
            ht.cols(temp_in, temp_out);
            for (j = 0; j < 8; ++j) {
                dest[j * stride + i] = clip_pixel_add(dest[j * stride + i],
                    ROUND_POWER_OF_TWO(temp_out[j], 5));
            }
        }
    }

    void iht8x8_64_px(const tran_low_t *input, int16_t *dest, int stride, int tx_type, int eob)
    {
        eob;
        int i, j;
        tran_low_t out[8 * 8];
        tran_low_t *outptr = out;
        tran_low_t temp_in[8], temp_out[8];
        const transform_2d ht = IHT_8[tx_type];

        // inverse transform row vectors
        for (i = 0; i < 8; ++i) {
            ht.rows(input, outptr);
            input += 8;
            outptr += 8;
        }

        // inverse transform column vectors
        for (i = 0; i < 8; ++i) {
            for (j = 0; j < 8; ++j)
                temp_in[j] = out[j * 8 + i];
            ht.cols(temp_in, temp_out);
            for (j = 0; j < 8; ++j) {
                dest[j * stride + i] = ROUND_POWER_OF_TWO(temp_out[j], 5);
            }
        }
    }

    // [16x16]
    void iadst16_c(const tran_low_t *input, tran_low_t *output)
    {
        tran_high_t s0, s1, s2, s3, s4, s5, s6, s7, s8;
        tran_high_t s9, s10, s11, s12, s13, s14, s15;

        tran_high_t x0 = input[15];
        tran_high_t x1 = input[0];
        tran_high_t x2 = input[13];
        tran_high_t x3 = input[2];
        tran_high_t x4 = input[11];
        tran_high_t x5 = input[4];
        tran_high_t x6 = input[9];
        tran_high_t x7 = input[6];
        tran_high_t x8 = input[7];
        tran_high_t x9 = input[8];
        tran_high_t x10 = input[5];
        tran_high_t x11 = input[10];
        tran_high_t x12 = input[3];
        tran_high_t x13 = input[12];
        tran_high_t x14 = input[1];
        tran_high_t x15 = input[14];

        if (!(x0 | x1 | x2 | x3 | x4 | x5 | x6 | x7 | x8
            | x9 | x10 | x11 | x12 | x13 | x14 | x15)) {
                output[0] = output[1] = output[2] = output[3] = output[4]
                = output[5] = output[6] = output[7] = output[8]
                = output[9] = output[10] = output[11] = output[12]
                = output[13] = output[14] = output[15] = 0;
                return;
        }

        // stage 1
        s0 = x0 * cospi_1_64  + x1 * cospi_31_64;
        s1 = x0 * cospi_31_64 - x1 * cospi_1_64;
        s2 = x2 * cospi_5_64  + x3 * cospi_27_64;
        s3 = x2 * cospi_27_64 - x3 * cospi_5_64;
        s4 = x4 * cospi_9_64  + x5 * cospi_23_64;
        s5 = x4 * cospi_23_64 - x5 * cospi_9_64;
        s6 = x6 * cospi_13_64 + x7 * cospi_19_64;
        s7 = x6 * cospi_19_64 - x7 * cospi_13_64;
        s8 = x8 * cospi_17_64 + x9 * cospi_15_64;
        s9 = x8 * cospi_15_64 - x9 * cospi_17_64;
        s10 = x10 * cospi_21_64 + x11 * cospi_11_64;
        s11 = x10 * cospi_11_64 - x11 * cospi_21_64;
        s12 = x12 * cospi_25_64 + x13 * cospi_7_64;
        s13 = x12 * cospi_7_64  - x13 * cospi_25_64;
        s14 = x14 * cospi_29_64 + x15 * cospi_3_64;
        s15 = x14 * cospi_3_64  - x15 * cospi_29_64;

        x0 = WRAPLOW(dct_const_round_shift(s0 + s8));
        x1 = WRAPLOW(dct_const_round_shift(s1 + s9));
        x2 = WRAPLOW(dct_const_round_shift(s2 + s10));
        x3 = WRAPLOW(dct_const_round_shift(s3 + s11));
        x4 = WRAPLOW(dct_const_round_shift(s4 + s12));
        x5 = WRAPLOW(dct_const_round_shift(s5 + s13));
        x6 = WRAPLOW(dct_const_round_shift(s6 + s14));
        x7 = WRAPLOW(dct_const_round_shift(s7 + s15));
        x8 = WRAPLOW(dct_const_round_shift(s0 - s8));
        x9 = WRAPLOW(dct_const_round_shift(s1 - s9));
        x10 = WRAPLOW(dct_const_round_shift(s2 - s10));
        x11 = WRAPLOW(dct_const_round_shift(s3 - s11));
        x12 = WRAPLOW(dct_const_round_shift(s4 - s12));
        x13 = WRAPLOW(dct_const_round_shift(s5 - s13));
        x14 = WRAPLOW(dct_const_round_shift(s6 - s14));
        x15 = WRAPLOW(dct_const_round_shift(s7 - s15));

        // stage 2
        s0 = x0;
        s1 = x1;
        s2 = x2;
        s3 = x3;
        s4 = x4;
        s5 = x5;
        s6 = x6;
        s7 = x7;
        s8 =    x8 * cospi_4_64   + x9 * cospi_28_64;
        s9 =    x8 * cospi_28_64  - x9 * cospi_4_64;
        s10 =   x10 * cospi_20_64 + x11 * cospi_12_64;
        s11 =   x10 * cospi_12_64 - x11 * cospi_20_64;
        s12 = - x12 * cospi_28_64 + x13 * cospi_4_64;
        s13 =   x12 * cospi_4_64  + x13 * cospi_28_64;
        s14 = - x14 * cospi_12_64 + x15 * cospi_20_64;
        s15 =   x14 * cospi_20_64 + x15 * cospi_12_64;

        x0 = WRAPLOW(s0 + s4);
        x1 = WRAPLOW(s1 + s5);
        x2 = WRAPLOW(s2 + s6);
        x3 = WRAPLOW(s3 + s7);
        x4 = WRAPLOW(s0 - s4);
        x5 = WRAPLOW(s1 - s5);
        x6 = WRAPLOW(s2 - s6);
        x7 = WRAPLOW(s3 - s7);
        x8 = WRAPLOW(dct_const_round_shift(s8 + s12));
        x9 = WRAPLOW(dct_const_round_shift(s9 + s13));
        x10 = WRAPLOW(dct_const_round_shift(s10 + s14));
        x11 = WRAPLOW(dct_const_round_shift(s11 + s15));
        x12 = WRAPLOW(dct_const_round_shift(s8 - s12));
        x13 = WRAPLOW(dct_const_round_shift(s9 - s13));
        x14 = WRAPLOW(dct_const_round_shift(s10 - s14));
        x15 = WRAPLOW(dct_const_round_shift(s11 - s15));

        // stage 3
        s0 = x0;
        s1 = x1;
        s2 = x2;
        s3 = x3;
        s4 = x4 * cospi_8_64  + x5 * cospi_24_64;
        s5 = x4 * cospi_24_64 - x5 * cospi_8_64;
        s6 = - x6 * cospi_24_64 + x7 * cospi_8_64;
        s7 =   x6 * cospi_8_64  + x7 * cospi_24_64;
        s8 = x8;
        s9 = x9;
        s10 = x10;
        s11 = x11;
        s12 = x12 * cospi_8_64  + x13 * cospi_24_64;
        s13 = x12 * cospi_24_64 - x13 * cospi_8_64;
        s14 = - x14 * cospi_24_64 + x15 * cospi_8_64;
        s15 =   x14 * cospi_8_64  + x15 * cospi_24_64;

        x0 = WRAPLOW(s0 + s2);
        x1 = WRAPLOW(s1 + s3);
        x2 = WRAPLOW(s0 - s2);
        x3 = WRAPLOW(s1 - s3);
        x4 = WRAPLOW(dct_const_round_shift(s4 + s6));
        x5 = WRAPLOW(dct_const_round_shift(s5 + s7));
        x6 = WRAPLOW(dct_const_round_shift(s4 - s6));
        x7 = WRAPLOW(dct_const_round_shift(s5 - s7));
        x8 = WRAPLOW(s8 + s10);
        x9 = WRAPLOW(s9 + s11);
        x10 = WRAPLOW(s8 - s10);
        x11 = WRAPLOW(s9 - s11);
        x12 = WRAPLOW(dct_const_round_shift(s12 + s14));
        x13 = WRAPLOW(dct_const_round_shift(s13 + s15));
        x14 = WRAPLOW(dct_const_round_shift(s12 - s14));
        x15 = WRAPLOW(dct_const_round_shift(s13 - s15));

        // stage 4
        s2 = (- cospi_16_64) * (x2 + x3);
        s3 = cospi_16_64 * (x2 - x3);
        s6 = cospi_16_64 * (x6 + x7);
        s7 = cospi_16_64 * (- x6 + x7);
        s10 = cospi_16_64 * (x10 + x11);
        s11 = cospi_16_64 * (- x10 + x11);
        s14 = (- cospi_16_64) * (x14 + x15);
        s15 = cospi_16_64 * (x14 - x15);

        x2 = WRAPLOW(dct_const_round_shift(s2));
        x3 = WRAPLOW(dct_const_round_shift(s3));
        x6 = WRAPLOW(dct_const_round_shift(s6));
        x7 = WRAPLOW(dct_const_round_shift(s7));
        x10 = WRAPLOW(dct_const_round_shift(s10));
        x11 = WRAPLOW(dct_const_round_shift(s11));
        x14 = WRAPLOW(dct_const_round_shift(s14));
        x15 = WRAPLOW(dct_const_round_shift(s15));

        output[0] = WRAPLOW(x0);
        output[1] = WRAPLOW(-x8);
        output[2] = WRAPLOW(x12);
        output[3] = WRAPLOW(-x4);
        output[4] = WRAPLOW(x6);
        output[5] = WRAPLOW(x14);
        output[6] = WRAPLOW(x10);
        output[7] = WRAPLOW(x2);
        output[8] = WRAPLOW(x3);
        output[9] = WRAPLOW(x11);
        output[10] = WRAPLOW(x15);
        output[11] = WRAPLOW(x7);
        output[12] = WRAPLOW(x5);
        output[13] = WRAPLOW(-x13);
        output[14] = WRAPLOW(x9);
        output[15] = WRAPLOW(-x1);
    }

    void idct16_c(const tran_low_t *input, tran_low_t *output)
    {
        tran_low_t step1[16], step2[16];
        tran_high_t temp1, temp2;

        // stage 1
        step1[0] = input[0/2];
        step1[1] = input[16/2];
        step1[2] = input[8/2];
        step1[3] = input[24/2];
        step1[4] = input[4/2];
        step1[5] = input[20/2];
        step1[6] = input[12/2];
        step1[7] = input[28/2];
        step1[8] = input[2/2];
        step1[9] = input[18/2];
        step1[10] = input[10/2];
        step1[11] = input[26/2];
        step1[12] = input[6/2];
        step1[13] = input[22/2];
        step1[14] = input[14/2];
        step1[15] = input[30/2];

        // stage 2
        step2[0] = step1[0];
        step2[1] = step1[1];
        step2[2] = step1[2];
        step2[3] = step1[3];
        step2[4] = step1[4];
        step2[5] = step1[5];
        step2[6] = step1[6];
        step2[7] = step1[7];

        temp1 = step1[8] * cospi_30_64 - step1[15] * cospi_2_64;
        temp2 = step1[8] * cospi_2_64 + step1[15] * cospi_30_64;
        step2[8] = WRAPLOW(dct_const_round_shift(temp1));
        step2[15] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = step1[9] * cospi_14_64 - step1[14] * cospi_18_64;
        temp2 = step1[9] * cospi_18_64 + step1[14] * cospi_14_64;
        step2[9] = WRAPLOW(dct_const_round_shift(temp1));
        step2[14] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = step1[10] * cospi_22_64 - step1[13] * cospi_10_64;
        temp2 = step1[10] * cospi_10_64 + step1[13] * cospi_22_64;
        step2[10] = WRAPLOW(dct_const_round_shift(temp1));
        step2[13] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = step1[11] * cospi_6_64 - step1[12] * cospi_26_64;
        temp2 = step1[11] * cospi_26_64 + step1[12] * cospi_6_64;
        step2[11] = WRAPLOW(dct_const_round_shift(temp1));
        step2[12] = WRAPLOW(dct_const_round_shift(temp2));

        // stage 3
        step1[0] = step2[0];
        step1[1] = step2[1];
        step1[2] = step2[2];
        step1[3] = step2[3];

        temp1 = step2[4] * cospi_28_64 - step2[7] * cospi_4_64;
        temp2 = step2[4] * cospi_4_64 + step2[7] * cospi_28_64;
        step1[4] = WRAPLOW(dct_const_round_shift(temp1));
        step1[7] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = step2[5] * cospi_12_64 - step2[6] * cospi_20_64;
        temp2 = step2[5] * cospi_20_64 + step2[6] * cospi_12_64;
        step1[5] = WRAPLOW(dct_const_round_shift(temp1));
        step1[6] = WRAPLOW(dct_const_round_shift(temp2));

        step1[8] = WRAPLOW(step2[8] + step2[9]);
        step1[9] = WRAPLOW(step2[8] - step2[9]);
        step1[10] = WRAPLOW(-step2[10] + step2[11]);
        step1[11] = WRAPLOW(step2[10] + step2[11]);
        step1[12] = WRAPLOW(step2[12] + step2[13]);
        step1[13] = WRAPLOW(step2[12] - step2[13]);
        step1[14] = WRAPLOW(-step2[14] + step2[15]);
        step1[15] = WRAPLOW(step2[14] + step2[15]);

        // stage 4
        temp1 = (step1[0] + step1[1]) * cospi_16_64;
        temp2 = (step1[0] - step1[1]) * cospi_16_64;
        step2[0] = WRAPLOW(dct_const_round_shift(temp1));
        step2[1] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = step1[2] * cospi_24_64 - step1[3] * cospi_8_64;
        temp2 = step1[2] * cospi_8_64 + step1[3] * cospi_24_64;
        step2[2] = WRAPLOW(dct_const_round_shift(temp1));
        step2[3] = WRAPLOW(dct_const_round_shift(temp2));
        step2[4] = WRAPLOW(step1[4] + step1[5]);
        step2[5] = WRAPLOW(step1[4] - step1[5]);
        step2[6] = WRAPLOW(-step1[6] + step1[7]);
        step2[7] = WRAPLOW(step1[6] + step1[7]);

        step2[8] = step1[8];
        step2[15] = step1[15];
        temp1 = -step1[9] * cospi_8_64 + step1[14] * cospi_24_64;
        temp2 = step1[9] * cospi_24_64 + step1[14] * cospi_8_64;
        step2[9] = WRAPLOW(dct_const_round_shift(temp1));
        step2[14] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = -step1[10] * cospi_24_64 - step1[13] * cospi_8_64;
        temp2 = -step1[10] * cospi_8_64 + step1[13] * cospi_24_64;
        step2[10] = WRAPLOW(dct_const_round_shift(temp1));
        step2[13] = WRAPLOW(dct_const_round_shift(temp2));
        step2[11] = step1[11];
        step2[12] = step1[12];

        // stage 5
        step1[0] = WRAPLOW(step2[0] + step2[3]);
        step1[1] = WRAPLOW(step2[1] + step2[2]);
        step1[2] = WRAPLOW(step2[1] - step2[2]);
        step1[3] = WRAPLOW(step2[0] - step2[3]);
        step1[4] = step2[4];
        temp1 = (step2[6] - step2[5]) * cospi_16_64;
        temp2 = (step2[5] + step2[6]) * cospi_16_64;
        step1[5] = WRAPLOW(dct_const_round_shift(temp1));
        step1[6] = WRAPLOW(dct_const_round_shift(temp2));
        step1[7] = step2[7];

        step1[8] = WRAPLOW(step2[8] + step2[11]);
        step1[9] = WRAPLOW(step2[9] + step2[10]);
        step1[10] = WRAPLOW(step2[9] - step2[10]);
        step1[11] = WRAPLOW(step2[8] - step2[11]);
        step1[12] = WRAPLOW(-step2[12] + step2[15]);
        step1[13] = WRAPLOW(-step2[13] + step2[14]);
        step1[14] = WRAPLOW(step2[13] + step2[14]);
        step1[15] = WRAPLOW(step2[12] + step2[15]);

        // stage 6
        step2[0] = WRAPLOW(step1[0] + step1[7]);
        step2[1] = WRAPLOW(step1[1] + step1[6]);
        step2[2] = WRAPLOW(step1[2] + step1[5]);
        step2[3] = WRAPLOW(step1[3] + step1[4]);
        step2[4] = WRAPLOW(step1[3] - step1[4]);
        step2[5] = WRAPLOW(step1[2] - step1[5]);
        step2[6] = WRAPLOW(step1[1] - step1[6]);
        step2[7] = WRAPLOW(step1[0] - step1[7]);
        step2[8] = step1[8];
        step2[9] = step1[9];
        temp1 = (-step1[10] + step1[13]) * cospi_16_64;
        temp2 = (step1[10] + step1[13]) * cospi_16_64;
        step2[10] = WRAPLOW(dct_const_round_shift(temp1));
        step2[13] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = (-step1[11] + step1[12]) * cospi_16_64;
        temp2 = (step1[11] + step1[12]) * cospi_16_64;
        step2[11] = WRAPLOW(dct_const_round_shift(temp1));
        step2[12] = WRAPLOW(dct_const_round_shift(temp2));
        step2[14] = step1[14];
        step2[15] = step1[15];

        // stage 7
        output[0] = WRAPLOW(step2[0] + step2[15]);
        output[1] = WRAPLOW(step2[1] + step2[14]);
        output[2] = WRAPLOW(step2[2] + step2[13]);
        output[3] = WRAPLOW(step2[3] + step2[12]);
        output[4] = WRAPLOW(step2[4] + step2[11]);
        output[5] = WRAPLOW(step2[5] + step2[10]);
        output[6] = WRAPLOW(step2[6] + step2[9]);
        output[7] = WRAPLOW(step2[7] + step2[8]);
        output[8] = WRAPLOW(step2[7] - step2[8]);
        output[9] = WRAPLOW(step2[6] - step2[9]);
        output[10] = WRAPLOW(step2[5] - step2[10]);
        output[11] = WRAPLOW(step2[4] - step2[11]);
        output[12] = WRAPLOW(step2[3] - step2[12]);
        output[13] = WRAPLOW(step2[2] - step2[13]);
        output[14] = WRAPLOW(step2[1] - step2[14]);
        output[15] = WRAPLOW(step2[0] - step2[15]);
    }


    static const transform_2d IHT_16[] = {
        { idct16_c,  idct16_c  },  // DCT_DCT  = 0
        { iadst16_c, idct16_c  },  // ADST_DCT = 1
        { idct16_c,  iadst16_c },  // DCT_ADST = 2
        { iadst16_c, iadst16_c }   // ADST_ADST = 3
    };

    void iht16x16_256_add_px(const tran_low_t *input, uint8_t *dest, int stride, int tx_type, int eob)
    {
        eob;
        int i, j;
        tran_low_t out[16 * 16];
        tran_low_t *outptr = out;
        tran_low_t temp_in[16], temp_out[16];
        const transform_2d ht = IHT_16[tx_type];

        // Rows
        for (i = 0; i < 16; ++i) {
            ht.rows(input, outptr);
            input += 16;
            outptr += 16;
        }

        // Columns
        for (i = 0; i < 16; ++i) {
            for (j = 0; j < 16; ++j)
                temp_in[j] = out[j * 16 + i];
            ht.cols(temp_in, temp_out);
            for (j = 0; j < 16; ++j) {
                dest[j * stride + i] = clip_pixel_add(dest[j * stride + i],
                    ROUND_POWER_OF_TWO(temp_out[j], 6));
            }
        }
    }

    void iht16x16_256_px(const tran_low_t *input, int16_t *dest, int stride, int tx_type, int eob)
    {
        eob;
        int i, j;
        tran_low_t out[16 * 16];
        tran_low_t *outptr = out;
        tran_low_t temp_in[16], temp_out[16];
        const transform_2d ht = IHT_16[tx_type];

        // Rows
        for (i = 0; i < 16; ++i) {
            ht.rows(input, outptr);
            input += 16;
            outptr += 16;
        }

        // Columns
        for (i = 0; i < 16; ++i) {
            for (j = 0; j < 16; ++j)
                temp_in[j] = out[j * 16 + i];
            ht.cols(temp_in, temp_out);
            for (j = 0; j < 16; ++j) {
                dest[j * stride + i] = ROUND_POWER_OF_TWO(temp_out[j], 6);
            }
        }
    }

    // [32x32]
    void idct32_c(const tran_low_t *input, tran_low_t *output)
    {
        tran_low_t step1[32], step2[32];
        tran_high_t temp1, temp2;

        // stage 1
        step1[0] = input[0];
        step1[1] = input[16];
        step1[2] = input[8];
        step1[3] = input[24];
        step1[4] = input[4];
        step1[5] = input[20];
        step1[6] = input[12];
        step1[7] = input[28];
        step1[8] = input[2];
        step1[9] = input[18];
        step1[10] = input[10];
        step1[11] = input[26];
        step1[12] = input[6];
        step1[13] = input[22];
        step1[14] = input[14];
        step1[15] = input[30];

        temp1 = input[1] * cospi_31_64 - input[31] * cospi_1_64;
        temp2 = input[1] * cospi_1_64 + input[31] * cospi_31_64;
        step1[16] = WRAPLOW(dct_const_round_shift(temp1));
        step1[31] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = input[17] * cospi_15_64 - input[15] * cospi_17_64;
        temp2 = input[17] * cospi_17_64 + input[15] * cospi_15_64;
        step1[17] = WRAPLOW(dct_const_round_shift(temp1));
        step1[30] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = input[9] * cospi_23_64 - input[23] * cospi_9_64;
        temp2 = input[9] * cospi_9_64 + input[23] * cospi_23_64;
        step1[18] = WRAPLOW(dct_const_round_shift(temp1));
        step1[29] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = input[25] * cospi_7_64 - input[7] * cospi_25_64;
        temp2 = input[25] * cospi_25_64 + input[7] * cospi_7_64;
        step1[19] = WRAPLOW(dct_const_round_shift(temp1));
        step1[28] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = input[5] * cospi_27_64 - input[27] * cospi_5_64;
        temp2 = input[5] * cospi_5_64 + input[27] * cospi_27_64;
        step1[20] = WRAPLOW(dct_const_round_shift(temp1));
        step1[27] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = input[21] * cospi_11_64 - input[11] * cospi_21_64;
        temp2 = input[21] * cospi_21_64 + input[11] * cospi_11_64;
        step1[21] = WRAPLOW(dct_const_round_shift(temp1));
        step1[26] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = input[13] * cospi_19_64 - input[19] * cospi_13_64;
        temp2 = input[13] * cospi_13_64 + input[19] * cospi_19_64;
        step1[22] = WRAPLOW(dct_const_round_shift(temp1));
        step1[25] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = input[29] * cospi_3_64 - input[3] * cospi_29_64;
        temp2 = input[29] * cospi_29_64 + input[3] * cospi_3_64;
        step1[23] = WRAPLOW(dct_const_round_shift(temp1));
        step1[24] = WRAPLOW(dct_const_round_shift(temp2));

        // stage 2
        step2[0] = step1[0];
        step2[1] = step1[1];
        step2[2] = step1[2];
        step2[3] = step1[3];
        step2[4] = step1[4];
        step2[5] = step1[5];
        step2[6] = step1[6];
        step2[7] = step1[7];

        temp1 = step1[8] * cospi_30_64 - step1[15] * cospi_2_64;
        temp2 = step1[8] * cospi_2_64 + step1[15] * cospi_30_64;
        step2[8] = WRAPLOW(dct_const_round_shift(temp1));
        step2[15] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = step1[9] * cospi_14_64 - step1[14] * cospi_18_64;
        temp2 = step1[9] * cospi_18_64 + step1[14] * cospi_14_64;
        step2[9] = WRAPLOW(dct_const_round_shift(temp1));
        step2[14] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = step1[10] * cospi_22_64 - step1[13] * cospi_10_64;
        temp2 = step1[10] * cospi_10_64 + step1[13] * cospi_22_64;
        step2[10] = WRAPLOW(dct_const_round_shift(temp1));
        step2[13] = WRAPLOW(dct_const_round_shift(temp2));

        temp1 = step1[11] * cospi_6_64 - step1[12] * cospi_26_64;
        temp2 = step1[11] * cospi_26_64 + step1[12] * cospi_6_64;
        step2[11] = WRAPLOW(dct_const_round_shift(temp1));
        step2[12] = WRAPLOW(dct_const_round_shift(temp2));

        step2[16] = WRAPLOW(step1[16] + step1[17]);
        step2[17] = WRAPLOW(step1[16] - step1[17]);
        step2[18] = WRAPLOW(-step1[18] + step1[19]);
        step2[19] = WRAPLOW(step1[18] + step1[19]);
        step2[20] = WRAPLOW(step1[20] + step1[21]);
        step2[21] = WRAPLOW(step1[20] - step1[21]);
        step2[22] = WRAPLOW(-step1[22] + step1[23]);
        step2[23] = WRAPLOW(step1[22] + step1[23]);
        step2[24] = WRAPLOW(step1[24] + step1[25]);
        step2[25] = WRAPLOW(step1[24] - step1[25]);
        step2[26] = WRAPLOW(-step1[26] + step1[27]);
        step2[27] = WRAPLOW(step1[26] + step1[27]);
        step2[28] = WRAPLOW(step1[28] + step1[29]);
        step2[29] = WRAPLOW(step1[28] - step1[29]);
        step2[30] = WRAPLOW(-step1[30] + step1[31]);
        step2[31] = WRAPLOW(step1[30] + step1[31]);

        // stage 3
        step1[0] = step2[0];
        step1[1] = step2[1];
        step1[2] = step2[2];
        step1[3] = step2[3];

        temp1 = step2[4] * cospi_28_64 - step2[7] * cospi_4_64;
        temp2 = step2[4] * cospi_4_64 + step2[7] * cospi_28_64;
        step1[4] = WRAPLOW(dct_const_round_shift(temp1));
        step1[7] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = step2[5] * cospi_12_64 - step2[6] * cospi_20_64;
        temp2 = step2[5] * cospi_20_64 + step2[6] * cospi_12_64;
        step1[5] = WRAPLOW(dct_const_round_shift(temp1));
        step1[6] = WRAPLOW(dct_const_round_shift(temp2));

        step1[8] = WRAPLOW(step2[8] + step2[9]);
        step1[9] = WRAPLOW(step2[8] - step2[9]);
        step1[10] = WRAPLOW(-step2[10] + step2[11]);
        step1[11] = WRAPLOW(step2[10] + step2[11]);
        step1[12] = WRAPLOW(step2[12] + step2[13]);
        step1[13] = WRAPLOW(step2[12] - step2[13]);
        step1[14] = WRAPLOW(-step2[14] + step2[15]);
        step1[15] = WRAPLOW(step2[14] + step2[15]);

        step1[16] = step2[16];
        step1[31] = step2[31];
        temp1 = -step2[17] * cospi_4_64 + step2[30] * cospi_28_64;
        temp2 = step2[17] * cospi_28_64 + step2[30] * cospi_4_64;
        step1[17] = WRAPLOW(dct_const_round_shift(temp1));
        step1[30] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = -step2[18] * cospi_28_64 - step2[29] * cospi_4_64;
        temp2 = -step2[18] * cospi_4_64 + step2[29] * cospi_28_64;
        step1[18] = WRAPLOW(dct_const_round_shift(temp1));
        step1[29] = WRAPLOW(dct_const_round_shift(temp2));
        step1[19] = step2[19];
        step1[20] = step2[20];
        temp1 = -step2[21] * cospi_20_64 + step2[26] * cospi_12_64;
        temp2 = step2[21] * cospi_12_64 + step2[26] * cospi_20_64;
        step1[21] = WRAPLOW(dct_const_round_shift(temp1));
        step1[26] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = -step2[22] * cospi_12_64 - step2[25] * cospi_20_64;
        temp2 = -step2[22] * cospi_20_64 + step2[25] * cospi_12_64;
        step1[22] = WRAPLOW(dct_const_round_shift(temp1));
        step1[25] = WRAPLOW(dct_const_round_shift(temp2));
        step1[23] = step2[23];
        step1[24] = step2[24];
        step1[27] = step2[27];
        step1[28] = step2[28];

        // stage 4
        temp1 = (step1[0] + step1[1]) * cospi_16_64;
        temp2 = (step1[0] - step1[1]) * cospi_16_64;
        step2[0] = WRAPLOW(dct_const_round_shift(temp1));
        step2[1] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = step1[2] * cospi_24_64 - step1[3] * cospi_8_64;
        temp2 = step1[2] * cospi_8_64 + step1[3] * cospi_24_64;
        step2[2] = WRAPLOW(dct_const_round_shift(temp1));
        step2[3] = WRAPLOW(dct_const_round_shift(temp2));
        step2[4] = WRAPLOW(step1[4] + step1[5]);
        step2[5] = WRAPLOW(step1[4] - step1[5]);
        step2[6] = WRAPLOW(-step1[6] + step1[7]);
        step2[7] = WRAPLOW(step1[6] + step1[7]);

        step2[8] = step1[8];
        step2[15] = step1[15];
        temp1 = -step1[9] * cospi_8_64 + step1[14] * cospi_24_64;
        temp2 = step1[9] * cospi_24_64 + step1[14] * cospi_8_64;
        step2[9] = WRAPLOW(dct_const_round_shift(temp1));
        step2[14] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = -step1[10] * cospi_24_64 - step1[13] * cospi_8_64;
        temp2 = -step1[10] * cospi_8_64 + step1[13] * cospi_24_64;
        step2[10] = WRAPLOW(dct_const_round_shift(temp1));
        step2[13] = WRAPLOW(dct_const_round_shift(temp2));
        step2[11] = step1[11];
        step2[12] = step1[12];

        step2[16] = WRAPLOW(step1[16] + step1[19]);
        step2[17] = WRAPLOW(step1[17] + step1[18]);
        step2[18] = WRAPLOW(step1[17] - step1[18]);
        step2[19] = WRAPLOW(step1[16] - step1[19]);
        step2[20] = WRAPLOW(-step1[20] + step1[23]);
        step2[21] = WRAPLOW(-step1[21] + step1[22]);
        step2[22] = WRAPLOW(step1[21] + step1[22]);
        step2[23] = WRAPLOW(step1[20] + step1[23]);

        step2[24] = WRAPLOW(step1[24] + step1[27]);
        step2[25] = WRAPLOW(step1[25] + step1[26]);
        step2[26] = WRAPLOW(step1[25] - step1[26]);
        step2[27] = WRAPLOW(step1[24] - step1[27]);
        step2[28] = WRAPLOW(-step1[28] + step1[31]);
        step2[29] = WRAPLOW(-step1[29] + step1[30]);
        step2[30] = WRAPLOW(step1[29] + step1[30]);
        step2[31] = WRAPLOW(step1[28] + step1[31]);

        // stage 5
        step1[0] = WRAPLOW(step2[0] + step2[3]);
        step1[1] = WRAPLOW(step2[1] + step2[2]);
        step1[2] = WRAPLOW(step2[1] - step2[2]);
        step1[3] = WRAPLOW(step2[0] - step2[3]);
        step1[4] = step2[4];
        temp1 = (step2[6] - step2[5]) * cospi_16_64;
        temp2 = (step2[5] + step2[6]) * cospi_16_64;
        step1[5] = WRAPLOW(dct_const_round_shift(temp1));
        step1[6] = WRAPLOW(dct_const_round_shift(temp2));
        step1[7] = step2[7];

        step1[8] = WRAPLOW(step2[8] + step2[11]);
        step1[9] = WRAPLOW(step2[9] + step2[10]);
        step1[10] = WRAPLOW(step2[9] - step2[10]);
        step1[11] = WRAPLOW(step2[8] - step2[11]);
        step1[12] = WRAPLOW(-step2[12] + step2[15]);
        step1[13] = WRAPLOW(-step2[13] + step2[14]);
        step1[14] = WRAPLOW(step2[13] + step2[14]);
        step1[15] = WRAPLOW(step2[12] + step2[15]);

        step1[16] = step2[16];
        step1[17] = step2[17];
        temp1 = -step2[18] * cospi_8_64 + step2[29] * cospi_24_64;
        temp2 = step2[18] * cospi_24_64 + step2[29] * cospi_8_64;
        step1[18] = WRAPLOW(dct_const_round_shift(temp1));
        step1[29] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = -step2[19] * cospi_8_64 + step2[28] * cospi_24_64;
        temp2 = step2[19] * cospi_24_64 + step2[28] * cospi_8_64;
        step1[19] = WRAPLOW(dct_const_round_shift(temp1));
        step1[28] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = -step2[20] * cospi_24_64 - step2[27] * cospi_8_64;
        temp2 = -step2[20] * cospi_8_64 + step2[27] * cospi_24_64;
        step1[20] = WRAPLOW(dct_const_round_shift(temp1));
        step1[27] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = -step2[21] * cospi_24_64 - step2[26] * cospi_8_64;
        temp2 = -step2[21] * cospi_8_64 + step2[26] * cospi_24_64;
        step1[21] = WRAPLOW(dct_const_round_shift(temp1));
        step1[26] = WRAPLOW(dct_const_round_shift(temp2));
        step1[22] = step2[22];
        step1[23] = step2[23];
        step1[24] = step2[24];
        step1[25] = step2[25];
        step1[30] = step2[30];
        step1[31] = step2[31];

        // stage 6
        step2[0] = WRAPLOW(step1[0] + step1[7]);
        step2[1] = WRAPLOW(step1[1] + step1[6]);
        step2[2] = WRAPLOW(step1[2] + step1[5]);
        step2[3] = WRAPLOW(step1[3] + step1[4]);
        step2[4] = WRAPLOW(step1[3] - step1[4]);
        step2[5] = WRAPLOW(step1[2] - step1[5]);
        step2[6] = WRAPLOW(step1[1] - step1[6]);
        step2[7] = WRAPLOW(step1[0] - step1[7]);
        step2[8] = step1[8];
        step2[9] = step1[9];
        temp1 = (-step1[10] + step1[13]) * cospi_16_64;
        temp2 = (step1[10] + step1[13]) * cospi_16_64;
        step2[10] = WRAPLOW(dct_const_round_shift(temp1));
        step2[13] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = (-step1[11] + step1[12]) * cospi_16_64;
        temp2 = (step1[11] + step1[12]) * cospi_16_64;
        step2[11] = WRAPLOW(dct_const_round_shift(temp1));
        step2[12] = WRAPLOW(dct_const_round_shift(temp2));
        step2[14] = step1[14];
        step2[15] = step1[15];

        step2[16] = WRAPLOW(step1[16] + step1[23]);
        step2[17] = WRAPLOW(step1[17] + step1[22]);
        step2[18] = WRAPLOW(step1[18] + step1[21]);
        step2[19] = WRAPLOW(step1[19] + step1[20]);
        step2[20] = WRAPLOW(step1[19] - step1[20]);
        step2[21] = WRAPLOW(step1[18] - step1[21]);
        step2[22] = WRAPLOW(step1[17] - step1[22]);
        step2[23] = WRAPLOW(step1[16] - step1[23]);

        step2[24] = WRAPLOW(-step1[24] + step1[31]);
        step2[25] = WRAPLOW(-step1[25] + step1[30]);
        step2[26] = WRAPLOW(-step1[26] + step1[29]);
        step2[27] = WRAPLOW(-step1[27] + step1[28]);
        step2[28] = WRAPLOW(step1[27] + step1[28]);
        step2[29] = WRAPLOW(step1[26] + step1[29]);
        step2[30] = WRAPLOW(step1[25] + step1[30]);
        step2[31] = WRAPLOW(step1[24] + step1[31]);

        // stage 7
        step1[0] = WRAPLOW(step2[0] + step2[15]);
        step1[1] = WRAPLOW(step2[1] + step2[14]);
        step1[2] = WRAPLOW(step2[2] + step2[13]);
        step1[3] = WRAPLOW(step2[3] + step2[12]);
        step1[4] = WRAPLOW(step2[4] + step2[11]);
        step1[5] = WRAPLOW(step2[5] + step2[10]);
        step1[6] = WRAPLOW(step2[6] + step2[9]);
        step1[7] = WRAPLOW(step2[7] + step2[8]);
        step1[8] = WRAPLOW(step2[7] - step2[8]);
        step1[9] = WRAPLOW(step2[6] - step2[9]);
        step1[10] = WRAPLOW(step2[5] - step2[10]);
        step1[11] = WRAPLOW(step2[4] - step2[11]);
        step1[12] = WRAPLOW(step2[3] - step2[12]);
        step1[13] = WRAPLOW(step2[2] - step2[13]);
        step1[14] = WRAPLOW(step2[1] - step2[14]);
        step1[15] = WRAPLOW(step2[0] - step2[15]);

        step1[16] = step2[16];
        step1[17] = step2[17];
        step1[18] = step2[18];
        step1[19] = step2[19];
        temp1 = (-step2[20] + step2[27]) * cospi_16_64;
        temp2 = (step2[20] + step2[27]) * cospi_16_64;
        step1[20] = WRAPLOW(dct_const_round_shift(temp1));
        step1[27] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = (-step2[21] + step2[26]) * cospi_16_64;
        temp2 = (step2[21] + step2[26]) * cospi_16_64;
        step1[21] = WRAPLOW(dct_const_round_shift(temp1));
        step1[26] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = (-step2[22] + step2[25]) * cospi_16_64;
        temp2 = (step2[22] + step2[25]) * cospi_16_64;
        step1[22] = WRAPLOW(dct_const_round_shift(temp1));
        step1[25] = WRAPLOW(dct_const_round_shift(temp2));
        temp1 = (-step2[23] + step2[24]) * cospi_16_64;
        temp2 = (step2[23] + step2[24]) * cospi_16_64;
        step1[23] = WRAPLOW(dct_const_round_shift(temp1));
        step1[24] = WRAPLOW(dct_const_round_shift(temp2));
        step1[28] = step2[28];
        step1[29] = step2[29];
        step1[30] = step2[30];
        step1[31] = step2[31];

        // final stage
        output[0] = WRAPLOW(step1[0] + step1[31]);
        output[1] = WRAPLOW(step1[1] + step1[30]);
        output[2] = WRAPLOW(step1[2] + step1[29]);
        output[3] = WRAPLOW(step1[3] + step1[28]);
        output[4] = WRAPLOW(step1[4] + step1[27]);
        output[5] = WRAPLOW(step1[5] + step1[26]);
        output[6] = WRAPLOW(step1[6] + step1[25]);
        output[7] = WRAPLOW(step1[7] + step1[24]);
        output[8] = WRAPLOW(step1[8] + step1[23]);
        output[9] = WRAPLOW(step1[9] + step1[22]);
        output[10] = WRAPLOW(step1[10] + step1[21]);
        output[11] = WRAPLOW(step1[11] + step1[20]);
        output[12] = WRAPLOW(step1[12] + step1[19]);
        output[13] = WRAPLOW(step1[13] + step1[18]);
        output[14] = WRAPLOW(step1[14] + step1[17]);
        output[15] = WRAPLOW(step1[15] + step1[16]);
        output[16] = WRAPLOW(step1[15] - step1[16]);
        output[17] = WRAPLOW(step1[14] - step1[17]);
        output[18] = WRAPLOW(step1[13] - step1[18]);
        output[19] = WRAPLOW(step1[12] - step1[19]);
        output[20] = WRAPLOW(step1[11] - step1[20]);
        output[21] = WRAPLOW(step1[10] - step1[21]);
        output[22] = WRAPLOW(step1[9] - step1[22]);
        output[23] = WRAPLOW(step1[8] - step1[23]);
        output[24] = WRAPLOW(step1[7] - step1[24]);
        output[25] = WRAPLOW(step1[6] - step1[25]);
        output[26] = WRAPLOW(step1[5] - step1[26]);
        output[27] = WRAPLOW(step1[4] - step1[27]);
        output[28] = WRAPLOW(step1[3] - step1[28]);
        output[29] = WRAPLOW(step1[2] - step1[29]);
        output[30] = WRAPLOW(step1[1] - step1[30]);
        output[31] = WRAPLOW(step1[0] - step1[31]);
    }
    // 32x32
    void vpx_idct32x32_1024_add_c(const tran_low_t *input, uint8_t *dest, int stride)
    {
        tran_low_t out[32 * 32];
        tran_low_t *outptr = out;
        int i, j;
        tran_low_t temp_in[32], temp_out[32];

        // Rows
        for (i = 0; i < 32; ++i) {
            int16_t zero_coeff[16];
            for (j = 0; j < 16; ++j)
                zero_coeff[j] = input[2 * j] | input[2 * j + 1];
            for (j = 0; j < 8; ++j)
                zero_coeff[j] = zero_coeff[2 * j] | zero_coeff[2 * j + 1];
            for (j = 0; j < 4; ++j)
                zero_coeff[j] = zero_coeff[2 * j] | zero_coeff[2 * j + 1];
            for (j = 0; j < 2; ++j)
                zero_coeff[j] = zero_coeff[2 * j] | zero_coeff[2 * j + 1];

            if (zero_coeff[0] | zero_coeff[1])
                idct32_c(input, outptr);
            else
                memset(outptr, 0, sizeof(tran_low_t) * 32);
            input += 32;
            outptr += 32;
        }

        // Columns
        for (i = 0; i < 32; ++i) {
            for (j = 0; j < 32; ++j)
                temp_in[j] = out[j * 32 + i];
            idct32_c(temp_in, temp_out);
            for (j = 0; j < 32; ++j) {
                dest[j * stride + i] = clip_pixel_add(dest[j * stride + i],
                    ROUND_POWER_OF_TWO(temp_out[j], 6));
            }
        }
    }

    void vpx_idct32x32_1024_c(const tran_low_t *input, int16_t *dest, int stride)
    {
        tran_low_t out[32 * 32];
        tran_low_t *outptr = out;
        int i, j;
        tran_low_t temp_in[32], temp_out[32];

        // Rows
        for (i = 0; i < 32; ++i) {
            int16_t zero_coeff[16];
            for (j = 0; j < 16; ++j)
                zero_coeff[j] = input[2 * j] | input[2 * j + 1];
            for (j = 0; j < 8; ++j)
                zero_coeff[j] = zero_coeff[2 * j] | zero_coeff[2 * j + 1];
            for (j = 0; j < 4; ++j)
                zero_coeff[j] = zero_coeff[2 * j] | zero_coeff[2 * j + 1];
            for (j = 0; j < 2; ++j)
                zero_coeff[j] = zero_coeff[2 * j] | zero_coeff[2 * j + 1];

            if (zero_coeff[0] | zero_coeff[1])
                idct32_c(input, outptr);
            else
                memset(outptr, 0, sizeof(tran_low_t) * 32);
            input += 32;
            outptr += 32;
        }

        // Columns
        for (i = 0; i < 32; ++i) {
            for (j = 0; j < 32; ++j)
                temp_in[j] = out[j * 32 + i];
            idct32_c(temp_in, temp_out);
            for (j = 0; j < 32; ++j) {
                dest[j * stride + i] = ROUND_POWER_OF_TWO(temp_out[j], 6);
            }
        }
    }

    void vpx_idct32x32_135_add_c(const tran_low_t *input, uint8_t *dest, int stride)
    {
        tran_low_t out[32 * 32] = {0};
        tran_low_t *outptr = out;
        int i, j;
        tran_low_t temp_in[32], temp_out[32];

        // Rows
        // only upper-left 16x16 has non-zero coeff
        for (i = 0; i < 16; ++i) {
            idct32_c(input, outptr);
            input += 32;
            outptr += 32;
        }

        // Columns
        for (i = 0; i < 32; ++i) {
            for (j = 0; j < 32; ++j)
                temp_in[j] = out[j * 32 + i];
            idct32_c(temp_in, temp_out);
            for (j = 0; j < 32; ++j) {
                dest[j * stride + i] = clip_pixel_add(dest[j * stride + i],
                    ROUND_POWER_OF_TWO(temp_out[j], 6));
            }
        }
    }

    void vpx_idct32x32_135_c(const tran_low_t *input, int16_t *dest, int stride)
    {
        tran_low_t out[32 * 32] = {0};
        tran_low_t *outptr = out;
        int i, j;
        tran_low_t temp_in[32], temp_out[32];

        // Rows
        // only upper-left 16x16 has non-zero coeff
        for (i = 0; i < 16; ++i) {
            idct32_c(input, outptr);
            input += 32;
            outptr += 32;
        }

        // Columns
        for (i = 0; i < 32; ++i) {
            for (j = 0; j < 32; ++j)
                temp_in[j] = out[j * 32 + i];
            idct32_c(temp_in, temp_out);
            for (j = 0; j < 32; ++j) {
                dest[j * stride + i] = ROUND_POWER_OF_TWO(temp_out[j], 6);
            }
        }
    }

    void vpx_idct32x32_34_add_c(const tran_low_t *input, uint8_t *dest, int stride)
    {
        tran_low_t out[32 * 32] = {0};
        tran_low_t *outptr = out;
        int i, j;
        tran_low_t temp_in[32], temp_out[32];

        // Rows
        // only upper-left 8x8 has non-zero coeff
        for (i = 0; i < 8; ++i) {
            idct32_c(input, outptr);
            input += 32;
            outptr += 32;
        }

        // Columns
        for (i = 0; i < 32; ++i) {
            for (j = 0; j < 32; ++j)
                temp_in[j] = out[j * 32 + i];
            idct32_c(temp_in, temp_out);
            for (j = 0; j < 32; ++j) {
                dest[j * stride + i] = clip_pixel_add(dest[j * stride + i],
                    ROUND_POWER_OF_TWO(temp_out[j], 6));
            }
        }
    }

    void vpx_idct32x32_34_c(const tran_low_t *input, int16_t *dest, int stride)
    {
        tran_low_t out[32 * 32] = {0};
        tran_low_t *outptr = out;
        int i, j;
        tran_low_t temp_in[32], temp_out[32];

        // Rows
        // only upper-left 8x8 has non-zero coeff
        for (i = 0; i < 8; ++i) {
            idct32_c(input, outptr);
            input += 32;
            outptr += 32;
        }

        // Columns
        for (i = 0; i < 32; ++i) {
            for (j = 0; j < 32; ++j)
                temp_in[j] = out[j * 32 + i];
            idct32_c(temp_in, temp_out);
            for (j = 0; j < 32; ++j) {
                dest[j * stride + i] = ROUND_POWER_OF_TWO(temp_out[j], 6);
            }
        }
    }

    static inline tran_high_t dct_const_round_shift(tran_high_t input) {
        tran_high_t rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
        return (tran_high_t)rv;
    }

    void vpx_idct32x32_1_add_c(const tran_low_t *input, uint8_t *dest, int stride)
    {
        int i, j;
        tran_high_t a1;

        tran_low_t out = WRAPLOW(dct_const_round_shift(input[0] * cospi_16_64));
        out = WRAPLOW(dct_const_round_shift(out * cospi_16_64));
        a1 = ROUND_POWER_OF_TWO(out, 6);

        for (j = 0; j < 32; ++j) {
            for (i = 0; i < 32; ++i)
                dest[i] = clip_pixel_add(dest[i], a1);
            dest += stride;
        }
    }

    void vpx_idct32x32_1_c(const tran_low_t *input, int16_t *dest, int stride)
    {
        int i, j;
        tran_high_t a1;

        tran_low_t out = WRAPLOW(dct_const_round_shift(input[0] * cospi_16_64));
        out = WRAPLOW(dct_const_round_shift(out * cospi_16_64));
        a1 = ROUND_POWER_OF_TWO(out, 6);

        for (j = 0; j < 32; ++j) {
            for (i = 0; i < 32; ++i)
                dest[i] = a1;
            dest += stride;
        }
    }

    void idct32x32_add_px(const tran_low_t *input, uint8_t *dest, int stride, int eob)
    {
        eob;
        //if (eob == 1)
        //    vpx_idct32x32_1_add_c(input, dest, stride);
        //else if (eob <= 34)
        //    // non-zero coeff only in upper-left 8x8
        //    vpx_idct32x32_34_add_c(input, dest, stride);
        //else if (eob <= 135)
        //    // non-zero coeff only in upper-left 16x16
        //    vpx_idct32x32_135_add_c(input, dest, stride);
        //else
        vpx_idct32x32_1024_add_c(input, dest, stride);
    }

    void idct32x32_px(const tran_low_t *input, int16_t *dest, int stride, int eob)
    {
        eob;
        //if (eob == 1)
        //    vpx_idct32x32_1_c(input, dest, stride);
        //else if (eob <= 34)
        //    // non-zero coeff only in upper-left 8x8
        //    vpx_idct32x32_34_c(input, dest, stride);
        //else if (eob <= 135)
        //    // non-zero coeff only in upper-left 16x16
        //    vpx_idct32x32_135_c(input, dest, stride);
        //else
        vpx_idct32x32_1024_c(input, dest, stride);
    }


    static const int8_t inv_shift_4x4[2] = { 0, -4 };
    static const int8_t inv_shift_8x8[2] = { -1, -4 };
    static const int8_t inv_shift_16x16[2] = { -2, -4 };
    static const int8_t inv_shift_32x32[2] = { -2, -4 };
    static const int8_t inv_shift_64x64[2] = { -2, -4 };
    static const int8_t inv_shift_4x8[2] = { 0, -4 };
    static const int8_t inv_shift_8x4[2] = { 0, -4 };
    static const int8_t inv_shift_8x16[2] = { -1, -4 };
    static const int8_t inv_shift_16x8[2] = { -1, -4 };
    static const int8_t inv_shift_16x32[2] = { -1, -4 };
    static const int8_t inv_shift_32x16[2] = { -1, -4 };
    static const int8_t inv_shift_32x64[2] = { -1, -4 };
    static const int8_t inv_shift_64x32[2] = { -1, -4 };
    static const int8_t inv_shift_4x16[2] = { -1, -4 };
    static const int8_t inv_shift_16x4[2] = { -1, -4 };
    static const int8_t inv_shift_8x32[2] = { -2, -4 };
    static const int8_t inv_shift_32x8[2] = { -2, -4 };
    static const int8_t inv_shift_16x64[2] = { -2, -4 };
    static const int8_t inv_shift_64x16[2] = { -2, -4 };

    static const int8_t *inv_txfm_shift_ls[TX_SIZES_ALL] = {
        inv_shift_4x4,   inv_shift_8x8,   inv_shift_16x16, inv_shift_32x32,
        inv_shift_64x64, inv_shift_4x8,   inv_shift_8x4,   inv_shift_8x16,
        inv_shift_16x8,  inv_shift_16x32, inv_shift_32x16, inv_shift_32x64,
        inv_shift_64x32, inv_shift_4x16,  inv_shift_16x4,  inv_shift_8x32,
        inv_shift_32x8,  inv_shift_16x64, inv_shift_64x16,
    };

    #define INV_COS_BIT 12
    const int8_t inv_cos_bit_col[MAX_TXWH_IDX][MAX_TXWH_IDX] = {
        { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0,           0 },
        { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0 },
        { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
        {           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
        {           0,           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT }
    };

    const int8_t inv_cos_bit_row[MAX_TXWH_IDX][MAX_TXWH_IDX] = {
        { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0,           0 },
        { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0 },
        { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
        {           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
        {           0,           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT }
    };

    const int8_t iadst4_range[7] = { 0, 1, 0, 0, 0, 0, 0 };

    void av1_get_inv_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size, TXFM_2D_FLIP_CFG *cfg) {
        assert(cfg != NULL);
        cfg->tx_size = tx_size;
        set_flip_cfg(tx_type, cfg);
        av1_zero(cfg->stage_range_col);
        av1_zero(cfg->stage_range_row);
        set_flip_cfg(tx_type, cfg);
        const TX_TYPE_1D tx_type_1d_col = vtx_tab[tx_type];
        const TX_TYPE_1D tx_type_1d_row = htx_tab[tx_type];
        cfg->shift = inv_txfm_shift_ls[tx_size];
        const int txw_idx = get_txw_idx(tx_size);
        const int txh_idx = get_txh_idx(tx_size);
        cfg->cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
        cfg->cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
        cfg->txfm_type_col = av1_txfm_type_ls[txh_idx][tx_type_1d_col];
        if (cfg->txfm_type_col == TXFM_TYPE_ADST4) {
            memcpy(cfg->stage_range_col, iadst4_range, sizeof(iadst4_range));
        }
        cfg->txfm_type_row = av1_txfm_type_ls[txw_idx][tx_type_1d_row];
        if (cfg->txfm_type_row == TXFM_TYPE_ADST4) {
            memcpy(cfg->stage_range_row, iadst4_range, sizeof(iadst4_range));
        }
        cfg->stage_num_col = av1_txfm_stage_num_list[cfg->txfm_type_col];
        cfg->stage_num_row = av1_txfm_stage_num_list[cfg->txfm_type_row];
    }

    static const int8_t inv_start_range[TX_SIZES_ALL] = {
        5,  // 4x4 transform
        6,  // 8x8 transform
        7,  // 16x16 transform
        7,  // 32x32 transform
        7,  // 64x64 transform
        5,  // 4x8 transform
        5,  // 8x4 transform
        6,  // 8x16 transform
        6,  // 16x8 transform
        6,  // 16x32 transform
        6,  // 32x16 transform
        6,  // 32x64 transform
        6,  // 64x32 transform
        6,  // 4x16 transform
        6,  // 16x4 transform
        7,  // 8x32 transform
        7,  // 32x8 transform
        7,  // 16x64 transform
        7,  // 64x16 transform
    };

    void av1_gen_inv_stage_range(int8_t *stage_range_col, int8_t *stage_range_row, const TXFM_2D_FLIP_CFG *cfg,
        TX_SIZE tx_size, int bd)
    {
        const int fwd_shift = inv_start_range[tx_size];
        const int8_t *shift = cfg->shift;
        int8_t opt_range_row, opt_range_col;
        if (bd == 8) {
            opt_range_row = 16;
            opt_range_col = 16;
        } else if (bd == 10) {
            opt_range_row = 18;
            opt_range_col = 16;
        } else {
            assert(bd == 12);
            opt_range_row = 20;
            opt_range_col = 18;
        }
        // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
        for (int i = 0; i < cfg->stage_num_row && i < MAX_TXFM_STAGE_NUM; ++i) {
            int real_range_row = cfg->stage_range_row[i] + fwd_shift + bd + 1;
            (void)real_range_row;
            if (cfg->txfm_type_row == TXFM_TYPE_ADST4 && i == 1) {
                // the adst4 may use 1 extra bit on top of opt_range_row at stage 1
                // so opt_range_col >= real_range_col will not hold
                stage_range_row[i] = opt_range_row;
            } else {
                assert(opt_range_row >= real_range_row);
                stage_range_row[i] = opt_range_row;
            }
        }
        // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
        for (int i = 0; i < cfg->stage_num_col && i < MAX_TXFM_STAGE_NUM; ++i) {
            int real_range_col =
                cfg->stage_range_col[i] + fwd_shift + shift[0] + bd + 1;
            (void)real_range_col;
            if (cfg->txfm_type_col == TXFM_TYPE_ADST4 && i == 1) {
                // the adst4 may use 1 extra bit on top of opt_range_row at stage 1
                // so opt_range_col >= real_range_col will not hold
                stage_range_col[i] = opt_range_col;
            } else {
                assert(opt_range_col >= real_range_col);
                stage_range_col[i] = opt_range_col;
            }
        }
    }

    void av1_idct4_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        assert(output != input);
        //const int32_t size = 4;
        const int32_t *cospi = cospi_arr(cos_bit);

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[4];

        // stage 0;

        // stage 1;
        stage++;
        bf1 = output;
        bf1[0] = input[0];
        bf1[1] = input[2];
        bf1[2] = input[1];
        bf1[3] = input[3];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
        bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit);
        bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[3], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[2], stage_range[stage]);
        bf1[2] = clamp_value(bf0[1] - bf0[2], stage_range[stage]);
        bf1[3] = clamp_value(bf0[0] - bf0[3], stage_range[stage]);
    }

    void av1_idct8_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        assert(output != input);
        //const int32_t size = 8;
        const int32_t *cospi = cospi_arr(cos_bit);

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[8];

        // stage 0;

        // stage 1;
        stage++;
        bf1 = output;
        bf1[0] = input[0];
        bf1[1] = input[4];
        bf1[2] = input[2];
        bf1[3] = input[6];
        bf1[4] = input[1];
        bf1[5] = input[5];
        bf1[6] = input[3];
        bf1[7] = input[7];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit);
        bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit);
        bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit);
        bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
        bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit);
        bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit);
        bf1[4] = clamp_value(bf0[4] + bf0[5], stage_range[stage]);
        bf1[5] = clamp_value(bf0[4] - bf0[5], stage_range[stage]);
        bf1[6] = clamp_value(-bf0[6] + bf0[7], stage_range[stage]);
        bf1[7] = clamp_value(bf0[6] + bf0[7], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = clamp_value(bf0[0] + bf0[3], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[2], stage_range[stage]);
        bf1[2] = clamp_value(bf0[1] - bf0[2], stage_range[stage]);
        bf1[3] = clamp_value(bf0[0] - bf0[3], stage_range[stage]);
        bf1[4] = bf0[4];
        bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
        bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
        bf1[7] = bf0[7];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[7], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[6], stage_range[stage]);
        bf1[2] = clamp_value(bf0[2] + bf0[5], stage_range[stage]);
        bf1[3] = clamp_value(bf0[3] + bf0[4], stage_range[stage]);
        bf1[4] = clamp_value(bf0[3] - bf0[4], stage_range[stage]);
        bf1[5] = clamp_value(bf0[2] - bf0[5], stage_range[stage]);
        bf1[6] = clamp_value(bf0[1] - bf0[6], stage_range[stage]);
        bf1[7] = clamp_value(bf0[0] - bf0[7], stage_range[stage]);
    }

    void av1_idct16_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        assert(output != input);
        //const int32_t size = 16;
        const int32_t *cospi = cospi_arr(cos_bit);

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[16];

        // stage 0;

        // stage 1;
        stage++;
        bf1 = output;
        bf1[0] = input[0];
        bf1[1] = input[8];
        bf1[2] = input[4];
        bf1[3] = input[12];
        bf1[4] = input[2];
        bf1[5] = input[10];
        bf1[6] = input[6];
        bf1[7] = input[14];
        bf1[8] = input[1];
        bf1[9] = input[9];
        bf1[10] = input[5];
        bf1[11] = input[13];
        bf1[12] = input[3];
        bf1[13] = input[11];
        bf1[14] = input[7];
        bf1[15] = input[15];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = bf0[6];
        bf1[7] = bf0[7];
        bf1[8] = half_btf(cospi[60], bf0[8], -cospi[4], bf0[15], cos_bit);
        bf1[9] = half_btf(cospi[28], bf0[9], -cospi[36], bf0[14], cos_bit);
        bf1[10] = half_btf(cospi[44], bf0[10], -cospi[20], bf0[13], cos_bit);
        bf1[11] = half_btf(cospi[12], bf0[11], -cospi[52], bf0[12], cos_bit);
        bf1[12] = half_btf(cospi[52], bf0[11], cospi[12], bf0[12], cos_bit);
        bf1[13] = half_btf(cospi[20], bf0[10], cospi[44], bf0[13], cos_bit);
        bf1[14] = half_btf(cospi[36], bf0[9], cospi[28], bf0[14], cos_bit);
        bf1[15] = half_btf(cospi[4], bf0[8], cospi[60], bf0[15], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit);
        bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit);
        bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit);
        bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit);
        bf1[8] = clamp_value(bf0[8] + bf0[9], stage_range[stage]);
        bf1[9] = clamp_value(bf0[8] - bf0[9], stage_range[stage]);
        bf1[10] = clamp_value(-bf0[10] + bf0[11], stage_range[stage]);
        bf1[11] = clamp_value(bf0[10] + bf0[11], stage_range[stage]);
        bf1[12] = clamp_value(bf0[12] + bf0[13], stage_range[stage]);
        bf1[13] = clamp_value(bf0[12] - bf0[13], stage_range[stage]);
        bf1[14] = clamp_value(-bf0[14] + bf0[15], stage_range[stage]);
        bf1[15] = clamp_value(bf0[14] + bf0[15], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
        bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit);
        bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit);
        bf1[4] = clamp_value(bf0[4] + bf0[5], stage_range[stage]);
        bf1[5] = clamp_value(bf0[4] - bf0[5], stage_range[stage]);
        bf1[6] = clamp_value(-bf0[6] + bf0[7], stage_range[stage]);
        bf1[7] = clamp_value(bf0[6] + bf0[7], stage_range[stage]);
        bf1[8] = bf0[8];
        bf1[9] = half_btf(-cospi[16], bf0[9], cospi[48], bf0[14], cos_bit);
        bf1[10] = half_btf(-cospi[48], bf0[10], -cospi[16], bf0[13], cos_bit);
        bf1[11] = bf0[11];
        bf1[12] = bf0[12];
        bf1[13] = half_btf(-cospi[16], bf0[10], cospi[48], bf0[13], cos_bit);
        bf1[14] = half_btf(cospi[48], bf0[9], cospi[16], bf0[14], cos_bit);
        bf1[15] = bf0[15];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[3], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[2], stage_range[stage]);
        bf1[2] = clamp_value(bf0[1] - bf0[2], stage_range[stage]);
        bf1[3] = clamp_value(bf0[0] - bf0[3], stage_range[stage]);
        bf1[4] = bf0[4];
        bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
        bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
        bf1[7] = bf0[7];
        bf1[8] = clamp_value(bf0[8] + bf0[11], stage_range[stage]);
        bf1[9] = clamp_value(bf0[9] + bf0[10], stage_range[stage]);
        bf1[10] = clamp_value(bf0[9] - bf0[10], stage_range[stage]);
        bf1[11] = clamp_value(bf0[8] - bf0[11], stage_range[stage]);
        bf1[12] = clamp_value(-bf0[12] + bf0[15], stage_range[stage]);
        bf1[13] = clamp_value(-bf0[13] + bf0[14], stage_range[stage]);
        bf1[14] = clamp_value(bf0[13] + bf0[14], stage_range[stage]);
        bf1[15] = clamp_value(bf0[12] + bf0[15], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 6
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = clamp_value(bf0[0] + bf0[7], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[6], stage_range[stage]);
        bf1[2] = clamp_value(bf0[2] + bf0[5], stage_range[stage]);
        bf1[3] = clamp_value(bf0[3] + bf0[4], stage_range[stage]);
        bf1[4] = clamp_value(bf0[3] - bf0[4], stage_range[stage]);
        bf1[5] = clamp_value(bf0[2] - bf0[5], stage_range[stage]);
        bf1[6] = clamp_value(bf0[1] - bf0[6], stage_range[stage]);
        bf1[7] = clamp_value(bf0[0] - bf0[7], stage_range[stage]);
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = half_btf(-cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
        bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
        bf1[12] = half_btf(cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
        bf1[13] = half_btf(cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
        bf1[14] = bf0[14];
        bf1[15] = bf0[15];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 7
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[15], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[14], stage_range[stage]);
        bf1[2] = clamp_value(bf0[2] + bf0[13], stage_range[stage]);
        bf1[3] = clamp_value(bf0[3] + bf0[12], stage_range[stage]);
        bf1[4] = clamp_value(bf0[4] + bf0[11], stage_range[stage]);
        bf1[5] = clamp_value(bf0[5] + bf0[10], stage_range[stage]);
        bf1[6] = clamp_value(bf0[6] + bf0[9], stage_range[stage]);
        bf1[7] = clamp_value(bf0[7] + bf0[8], stage_range[stage]);
        bf1[8] = clamp_value(bf0[7] - bf0[8], stage_range[stage]);
        bf1[9] = clamp_value(bf0[6] - bf0[9], stage_range[stage]);
        bf1[10] = clamp_value(bf0[5] - bf0[10], stage_range[stage]);
        bf1[11] = clamp_value(bf0[4] - bf0[11], stage_range[stage]);
        bf1[12] = clamp_value(bf0[3] - bf0[12], stage_range[stage]);
        bf1[13] = clamp_value(bf0[2] - bf0[13], stage_range[stage]);
        bf1[14] = clamp_value(bf0[1] - bf0[14], stage_range[stage]);
        bf1[15] = clamp_value(bf0[0] - bf0[15], stage_range[stage]);
    }

    void av1_idct32_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        assert(output != input);
        //const int32_t size = 32;
        const int32_t *cospi = cospi_arr(cos_bit);

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[32];

        // stage 0;

        // stage 1;
        stage++;
        bf1 = output;
        bf1[0] = input[0];
        bf1[1] = input[16];
        bf1[2] = input[8];
        bf1[3] = input[24];
        bf1[4] = input[4];
        bf1[5] = input[20];
        bf1[6] = input[12];
        bf1[7] = input[28];
        bf1[8] = input[2];
        bf1[9] = input[18];
        bf1[10] = input[10];
        bf1[11] = input[26];
        bf1[12] = input[6];
        bf1[13] = input[22];
        bf1[14] = input[14];
        bf1[15] = input[30];
        bf1[16] = input[1];
        bf1[17] = input[17];
        bf1[18] = input[9];
        bf1[19] = input[25];
        bf1[20] = input[5];
        bf1[21] = input[21];
        bf1[22] = input[13];
        bf1[23] = input[29];
        bf1[24] = input[3];
        bf1[25] = input[19];
        bf1[26] = input[11];
        bf1[27] = input[27];
        bf1[28] = input[7];
        bf1[29] = input[23];
        bf1[30] = input[15];
        bf1[31] = input[31];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = bf0[6];
        bf1[7] = bf0[7];
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = bf0[10];
        bf1[11] = bf0[11];
        bf1[12] = bf0[12];
        bf1[13] = bf0[13];
        bf1[14] = bf0[14];
        bf1[15] = bf0[15];
        bf1[16] = half_btf(cospi[62], bf0[16], -cospi[2], bf0[31], cos_bit);
        bf1[17] = half_btf(cospi[30], bf0[17], -cospi[34], bf0[30], cos_bit);
        bf1[18] = half_btf(cospi[46], bf0[18], -cospi[18], bf0[29], cos_bit);
        bf1[19] = half_btf(cospi[14], bf0[19], -cospi[50], bf0[28], cos_bit);
        bf1[20] = half_btf(cospi[54], bf0[20], -cospi[10], bf0[27], cos_bit);
        bf1[21] = half_btf(cospi[22], bf0[21], -cospi[42], bf0[26], cos_bit);
        bf1[22] = half_btf(cospi[38], bf0[22], -cospi[26], bf0[25], cos_bit);
        bf1[23] = half_btf(cospi[6], bf0[23], -cospi[58], bf0[24], cos_bit);
        bf1[24] = half_btf(cospi[58], bf0[23], cospi[6], bf0[24], cos_bit);
        bf1[25] = half_btf(cospi[26], bf0[22], cospi[38], bf0[25], cos_bit);
        bf1[26] = half_btf(cospi[42], bf0[21], cospi[22], bf0[26], cos_bit);
        bf1[27] = half_btf(cospi[10], bf0[20], cospi[54], bf0[27], cos_bit);
        bf1[28] = half_btf(cospi[50], bf0[19], cospi[14], bf0[28], cos_bit);
        bf1[29] = half_btf(cospi[18], bf0[18], cospi[46], bf0[29], cos_bit);
        bf1[30] = half_btf(cospi[34], bf0[17], cospi[30], bf0[30], cos_bit);
        bf1[31] = half_btf(cospi[2], bf0[16], cospi[62], bf0[31], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = bf0[6];
        bf1[7] = bf0[7];
        bf1[8] = half_btf(cospi[60], bf0[8], -cospi[4], bf0[15], cos_bit);
        bf1[9] = half_btf(cospi[28], bf0[9], -cospi[36], bf0[14], cos_bit);
        bf1[10] = half_btf(cospi[44], bf0[10], -cospi[20], bf0[13], cos_bit);
        bf1[11] = half_btf(cospi[12], bf0[11], -cospi[52], bf0[12], cos_bit);
        bf1[12] = half_btf(cospi[52], bf0[11], cospi[12], bf0[12], cos_bit);
        bf1[13] = half_btf(cospi[20], bf0[10], cospi[44], bf0[13], cos_bit);
        bf1[14] = half_btf(cospi[36], bf0[9], cospi[28], bf0[14], cos_bit);
        bf1[15] = half_btf(cospi[4], bf0[8], cospi[60], bf0[15], cos_bit);
        bf1[16] = clamp_value(bf0[16] + bf0[17], stage_range[stage]);
        bf1[17] = clamp_value(bf0[16] - bf0[17], stage_range[stage]);
        bf1[18] = clamp_value(-bf0[18] + bf0[19], stage_range[stage]);
        bf1[19] = clamp_value(bf0[18] + bf0[19], stage_range[stage]);
        bf1[20] = clamp_value(bf0[20] + bf0[21], stage_range[stage]);
        bf1[21] = clamp_value(bf0[20] - bf0[21], stage_range[stage]);
        bf1[22] = clamp_value(-bf0[22] + bf0[23], stage_range[stage]);
        bf1[23] = clamp_value(bf0[22] + bf0[23], stage_range[stage]);
        bf1[24] = clamp_value(bf0[24] + bf0[25], stage_range[stage]);
        bf1[25] = clamp_value(bf0[24] - bf0[25], stage_range[stage]);
        bf1[26] = clamp_value(-bf0[26] + bf0[27], stage_range[stage]);
        bf1[27] = clamp_value(bf0[26] + bf0[27], stage_range[stage]);
        bf1[28] = clamp_value(bf0[28] + bf0[29], stage_range[stage]);
        bf1[29] = clamp_value(bf0[28] - bf0[29], stage_range[stage]);
        bf1[30] = clamp_value(-bf0[30] + bf0[31], stage_range[stage]);
        bf1[31] = clamp_value(bf0[30] + bf0[31], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit);
        bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit);
        bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit);
        bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit);
        bf1[8] = clamp_value(bf0[8] + bf0[9], stage_range[stage]);
        bf1[9] = clamp_value(bf0[8] - bf0[9], stage_range[stage]);
        bf1[10] = clamp_value(-bf0[10] + bf0[11], stage_range[stage]);
        bf1[11] = clamp_value(bf0[10] + bf0[11], stage_range[stage]);
        bf1[12] = clamp_value(bf0[12] + bf0[13], stage_range[stage]);
        bf1[13] = clamp_value(bf0[12] - bf0[13], stage_range[stage]);
        bf1[14] = clamp_value(-bf0[14] + bf0[15], stage_range[stage]);
        bf1[15] = clamp_value(bf0[14] + bf0[15], stage_range[stage]);
        bf1[16] = bf0[16];
        bf1[17] = half_btf(-cospi[8], bf0[17], cospi[56], bf0[30], cos_bit);
        bf1[18] = half_btf(-cospi[56], bf0[18], -cospi[8], bf0[29], cos_bit);
        bf1[19] = bf0[19];
        bf1[20] = bf0[20];
        bf1[21] = half_btf(-cospi[40], bf0[21], cospi[24], bf0[26], cos_bit);
        bf1[22] = half_btf(-cospi[24], bf0[22], -cospi[40], bf0[25], cos_bit);
        bf1[23] = bf0[23];
        bf1[24] = bf0[24];
        bf1[25] = half_btf(-cospi[40], bf0[22], cospi[24], bf0[25], cos_bit);
        bf1[26] = half_btf(cospi[24], bf0[21], cospi[40], bf0[26], cos_bit);
        bf1[27] = bf0[27];
        bf1[28] = bf0[28];
        bf1[29] = half_btf(-cospi[8], bf0[18], cospi[56], bf0[29], cos_bit);
        bf1[30] = half_btf(cospi[56], bf0[17], cospi[8], bf0[30], cos_bit);
        bf1[31] = bf0[31];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
        bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit);
        bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit);
        bf1[4] = clamp_value(bf0[4] + bf0[5], stage_range[stage]);
        bf1[5] = clamp_value(bf0[4] - bf0[5], stage_range[stage]);
        bf1[6] = clamp_value(-bf0[6] + bf0[7], stage_range[stage]);
        bf1[7] = clamp_value(bf0[6] + bf0[7], stage_range[stage]);
        bf1[8] = bf0[8];
        bf1[9] = half_btf(-cospi[16], bf0[9], cospi[48], bf0[14], cos_bit);
        bf1[10] = half_btf(-cospi[48], bf0[10], -cospi[16], bf0[13], cos_bit);
        bf1[11] = bf0[11];
        bf1[12] = bf0[12];
        bf1[13] = half_btf(-cospi[16], bf0[10], cospi[48], bf0[13], cos_bit);
        bf1[14] = half_btf(cospi[48], bf0[9], cospi[16], bf0[14], cos_bit);
        bf1[15] = bf0[15];
        bf1[16] = clamp_value(bf0[16] + bf0[19], stage_range[stage]);
        bf1[17] = clamp_value(bf0[17] + bf0[18], stage_range[stage]);
        bf1[18] = clamp_value(bf0[17] - bf0[18], stage_range[stage]);
        bf1[19] = clamp_value(bf0[16] - bf0[19], stage_range[stage]);
        bf1[20] = clamp_value(-bf0[20] + bf0[23], stage_range[stage]);
        bf1[21] = clamp_value(-bf0[21] + bf0[22], stage_range[stage]);
        bf1[22] = clamp_value(bf0[21] + bf0[22], stage_range[stage]);
        bf1[23] = clamp_value(bf0[20] + bf0[23], stage_range[stage]);
        bf1[24] = clamp_value(bf0[24] + bf0[27], stage_range[stage]);
        bf1[25] = clamp_value(bf0[25] + bf0[26], stage_range[stage]);
        bf1[26] = clamp_value(bf0[25] - bf0[26], stage_range[stage]);
        bf1[27] = clamp_value(bf0[24] - bf0[27], stage_range[stage]);
        bf1[28] = clamp_value(-bf0[28] + bf0[31], stage_range[stage]);
        bf1[29] = clamp_value(-bf0[29] + bf0[30], stage_range[stage]);
        bf1[30] = clamp_value(bf0[29] + bf0[30], stage_range[stage]);
        bf1[31] = clamp_value(bf0[28] + bf0[31], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 6
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = clamp_value(bf0[0] + bf0[3], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[2], stage_range[stage]);
        bf1[2] = clamp_value(bf0[1] - bf0[2], stage_range[stage]);
        bf1[3] = clamp_value(bf0[0] - bf0[3], stage_range[stage]);
        bf1[4] = bf0[4];
        bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
        bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
        bf1[7] = bf0[7];
        bf1[8] = clamp_value(bf0[8] + bf0[11], stage_range[stage]);
        bf1[9] = clamp_value(bf0[9] + bf0[10], stage_range[stage]);
        bf1[10] = clamp_value(bf0[9] - bf0[10], stage_range[stage]);
        bf1[11] = clamp_value(bf0[8] - bf0[11], stage_range[stage]);
        bf1[12] = clamp_value(-bf0[12] + bf0[15], stage_range[stage]);
        bf1[13] = clamp_value(-bf0[13] + bf0[14], stage_range[stage]);
        bf1[14] = clamp_value(bf0[13] + bf0[14], stage_range[stage]);
        bf1[15] = clamp_value(bf0[12] + bf0[15], stage_range[stage]);
        bf1[16] = bf0[16];
        bf1[17] = bf0[17];
        bf1[18] = half_btf(-cospi[16], bf0[18], cospi[48], bf0[29], cos_bit);
        bf1[19] = half_btf(-cospi[16], bf0[19], cospi[48], bf0[28], cos_bit);
        bf1[20] = half_btf(-cospi[48], bf0[20], -cospi[16], bf0[27], cos_bit);
        bf1[21] = half_btf(-cospi[48], bf0[21], -cospi[16], bf0[26], cos_bit);
        bf1[22] = bf0[22];
        bf1[23] = bf0[23];
        bf1[24] = bf0[24];
        bf1[25] = bf0[25];
        bf1[26] = half_btf(-cospi[16], bf0[21], cospi[48], bf0[26], cos_bit);
        bf1[27] = half_btf(-cospi[16], bf0[20], cospi[48], bf0[27], cos_bit);
        bf1[28] = half_btf(cospi[48], bf0[19], cospi[16], bf0[28], cos_bit);
        bf1[29] = half_btf(cospi[48], bf0[18], cospi[16], bf0[29], cos_bit);
        bf1[30] = bf0[30];
        bf1[31] = bf0[31];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 7
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[7], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[6], stage_range[stage]);
        bf1[2] = clamp_value(bf0[2] + bf0[5], stage_range[stage]);
        bf1[3] = clamp_value(bf0[3] + bf0[4], stage_range[stage]);
        bf1[4] = clamp_value(bf0[3] - bf0[4], stage_range[stage]);
        bf1[5] = clamp_value(bf0[2] - bf0[5], stage_range[stage]);
        bf1[6] = clamp_value(bf0[1] - bf0[6], stage_range[stage]);
        bf1[7] = clamp_value(bf0[0] - bf0[7], stage_range[stage]);
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = half_btf(-cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
        bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
        bf1[12] = half_btf(cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
        bf1[13] = half_btf(cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
        bf1[14] = bf0[14];
        bf1[15] = bf0[15];
        bf1[16] = clamp_value(bf0[16] + bf0[23], stage_range[stage]);
        bf1[17] = clamp_value(bf0[17] + bf0[22], stage_range[stage]);
        bf1[18] = clamp_value(bf0[18] + bf0[21], stage_range[stage]);
        bf1[19] = clamp_value(bf0[19] + bf0[20], stage_range[stage]);
        bf1[20] = clamp_value(bf0[19] - bf0[20], stage_range[stage]);
        bf1[21] = clamp_value(bf0[18] - bf0[21], stage_range[stage]);
        bf1[22] = clamp_value(bf0[17] - bf0[22], stage_range[stage]);
        bf1[23] = clamp_value(bf0[16] - bf0[23], stage_range[stage]);
        bf1[24] = clamp_value(-bf0[24] + bf0[31], stage_range[stage]);
        bf1[25] = clamp_value(-bf0[25] + bf0[30], stage_range[stage]);
        bf1[26] = clamp_value(-bf0[26] + bf0[29], stage_range[stage]);
        bf1[27] = clamp_value(-bf0[27] + bf0[28], stage_range[stage]);
        bf1[28] = clamp_value(bf0[27] + bf0[28], stage_range[stage]);
        bf1[29] = clamp_value(bf0[26] + bf0[29], stage_range[stage]);
        bf1[30] = clamp_value(bf0[25] + bf0[30], stage_range[stage]);
        bf1[31] = clamp_value(bf0[24] + bf0[31], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 8
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = clamp_value(bf0[0] + bf0[15], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[14], stage_range[stage]);
        bf1[2] = clamp_value(bf0[2] + bf0[13], stage_range[stage]);
        bf1[3] = clamp_value(bf0[3] + bf0[12], stage_range[stage]);
        bf1[4] = clamp_value(bf0[4] + bf0[11], stage_range[stage]);
        bf1[5] = clamp_value(bf0[5] + bf0[10], stage_range[stage]);
        bf1[6] = clamp_value(bf0[6] + bf0[9], stage_range[stage]);
        bf1[7] = clamp_value(bf0[7] + bf0[8], stage_range[stage]);
        bf1[8] = clamp_value(bf0[7] - bf0[8], stage_range[stage]);
        bf1[9] = clamp_value(bf0[6] - bf0[9], stage_range[stage]);
        bf1[10] = clamp_value(bf0[5] - bf0[10], stage_range[stage]);
        bf1[11] = clamp_value(bf0[4] - bf0[11], stage_range[stage]);
        bf1[12] = clamp_value(bf0[3] - bf0[12], stage_range[stage]);
        bf1[13] = clamp_value(bf0[2] - bf0[13], stage_range[stage]);
        bf1[14] = clamp_value(bf0[1] - bf0[14], stage_range[stage]);
        bf1[15] = clamp_value(bf0[0] - bf0[15], stage_range[stage]);
        bf1[16] = bf0[16];
        bf1[17] = bf0[17];
        bf1[18] = bf0[18];
        bf1[19] = bf0[19];
        bf1[20] = half_btf(-cospi[32], bf0[20], cospi[32], bf0[27], cos_bit);
        bf1[21] = half_btf(-cospi[32], bf0[21], cospi[32], bf0[26], cos_bit);
        bf1[22] = half_btf(-cospi[32], bf0[22], cospi[32], bf0[25], cos_bit);
        bf1[23] = half_btf(-cospi[32], bf0[23], cospi[32], bf0[24], cos_bit);
        bf1[24] = half_btf(cospi[32], bf0[23], cospi[32], bf0[24], cos_bit);
        bf1[25] = half_btf(cospi[32], bf0[22], cospi[32], bf0[25], cos_bit);
        bf1[26] = half_btf(cospi[32], bf0[21], cospi[32], bf0[26], cos_bit);
        bf1[27] = half_btf(cospi[32], bf0[20], cospi[32], bf0[27], cos_bit);
        bf1[28] = bf0[28];
        bf1[29] = bf0[29];
        bf1[30] = bf0[30];
        bf1[31] = bf0[31];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 9
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[31], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[30], stage_range[stage]);
        bf1[2] = clamp_value(bf0[2] + bf0[29], stage_range[stage]);
        bf1[3] = clamp_value(bf0[3] + bf0[28], stage_range[stage]);
        bf1[4] = clamp_value(bf0[4] + bf0[27], stage_range[stage]);
        bf1[5] = clamp_value(bf0[5] + bf0[26], stage_range[stage]);
        bf1[6] = clamp_value(bf0[6] + bf0[25], stage_range[stage]);
        bf1[7] = clamp_value(bf0[7] + bf0[24], stage_range[stage]);
        bf1[8] = clamp_value(bf0[8] + bf0[23], stage_range[stage]);
        bf1[9] = clamp_value(bf0[9] + bf0[22], stage_range[stage]);
        bf1[10] = clamp_value(bf0[10] + bf0[21], stage_range[stage]);
        bf1[11] = clamp_value(bf0[11] + bf0[20], stage_range[stage]);
        bf1[12] = clamp_value(bf0[12] + bf0[19], stage_range[stage]);
        bf1[13] = clamp_value(bf0[13] + bf0[18], stage_range[stage]);
        bf1[14] = clamp_value(bf0[14] + bf0[17], stage_range[stage]);
        bf1[15] = clamp_value(bf0[15] + bf0[16], stage_range[stage]);
        bf1[16] = clamp_value(bf0[15] - bf0[16], stage_range[stage]);
        bf1[17] = clamp_value(bf0[14] - bf0[17], stage_range[stage]);
        bf1[18] = clamp_value(bf0[13] - bf0[18], stage_range[stage]);
        bf1[19] = clamp_value(bf0[12] - bf0[19], stage_range[stage]);
        bf1[20] = clamp_value(bf0[11] - bf0[20], stage_range[stage]);
        bf1[21] = clamp_value(bf0[10] - bf0[21], stage_range[stage]);
        bf1[22] = clamp_value(bf0[9] - bf0[22], stage_range[stage]);
        bf1[23] = clamp_value(bf0[8] - bf0[23], stage_range[stage]);
        bf1[24] = clamp_value(bf0[7] - bf0[24], stage_range[stage]);
        bf1[25] = clamp_value(bf0[6] - bf0[25], stage_range[stage]);
        bf1[26] = clamp_value(bf0[5] - bf0[26], stage_range[stage]);
        bf1[27] = clamp_value(bf0[4] - bf0[27], stage_range[stage]);
        bf1[28] = clamp_value(bf0[3] - bf0[28], stage_range[stage]);
        bf1[29] = clamp_value(bf0[2] - bf0[29], stage_range[stage]);
        bf1[30] = clamp_value(bf0[1] - bf0[30], stage_range[stage]);
        bf1[31] = clamp_value(bf0[0] - bf0[31], stage_range[stage]);
    }

    void av1_iadst4_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        int bit = cos_bit;
        const int32_t *sinpi = sinpi_arr(bit);
        int32_t s0, s1, s2, s3, s4, s5, s6, s7;

        int32_t x0 = input[0];
        int32_t x1 = input[1];
        int32_t x2 = input[2];
        int32_t x3 = input[3];

        if (!(x0 | x1 | x2 | x3)) {
            output[0] = output[1] = output[2] = output[3] = 0;
            return;
        }

        assert(sinpi[1] + sinpi[2] == sinpi[4]);

        // stage 1
        s0 = range_check_value(sinpi[1] * x0, stage_range[1] + bit);
        s1 = range_check_value(sinpi[2] * x0, stage_range[1] + bit);
        s2 = range_check_value(sinpi[3] * x1, stage_range[1] + bit);
        s3 = range_check_value(sinpi[4] * x2, stage_range[1] + bit);
        s4 = range_check_value(sinpi[1] * x2, stage_range[1] + bit);
        s5 = range_check_value(sinpi[2] * x3, stage_range[1] + bit);
        s6 = range_check_value(sinpi[4] * x3, stage_range[1] + bit);

        // stage 2
        // NOTICE: (x0 - x2) here may use one extra bit compared to the
        // opt_range_row/col specified in av1_gen_inv_stage_range()
        s7 = range_check_value((x0 - x2) + x3, stage_range[2]);

        // stage 3
        s0 = range_check_value(s0 + s3, stage_range[3] + bit);
        s1 = range_check_value(s1 - s4, stage_range[3] + bit);
        s3 = range_check_value(s2, stage_range[3] + bit);
        s2 = range_check_value(sinpi[3] * s7, stage_range[3] + bit);

        // stage 4
        s0 = range_check_value(s0 + s5, stage_range[4] + bit);
        s1 = range_check_value(s1 - s6, stage_range[4] + bit);

        // stage 5
        x0 = range_check_value(s0 + s3, stage_range[5] + bit);
        x1 = range_check_value(s1 + s3, stage_range[5] + bit);
        x2 = range_check_value(s2, stage_range[5] + bit);
        x3 = range_check_value(s0 + s1, stage_range[5] + bit);

        // stage 6
        x3 = range_check_value(x3 - s3, stage_range[6] + bit);

        output[0] = round_shift(x0, bit);
        output[1] = round_shift(x1, bit);
        output[2] = round_shift(x2, bit);
        output[3] = round_shift(x3, bit);
    }

    void av1_iadst8_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        assert(output != input);
        //const int32_t size = 8;
        const int32_t *cospi = cospi_arr(cos_bit);

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[8];

        // stage 0;

        // stage 1;
        stage++;
        bf1 = output;
        bf1[0] = input[7];
        bf1[1] = input[0];
        bf1[2] = input[5];
        bf1[3] = input[2];
        bf1[4] = input[3];
        bf1[5] = input[4];
        bf1[6] = input[1];
        bf1[7] = input[6];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = half_btf(cospi[4], bf0[0], cospi[60], bf0[1], cos_bit);
        bf1[1] = half_btf(cospi[60], bf0[0], -cospi[4], bf0[1], cos_bit);
        bf1[2] = half_btf(cospi[20], bf0[2], cospi[44], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[44], bf0[2], -cospi[20], bf0[3], cos_bit);
        bf1[4] = half_btf(cospi[36], bf0[4], cospi[28], bf0[5], cos_bit);
        bf1[5] = half_btf(cospi[28], bf0[4], -cospi[36], bf0[5], cos_bit);
        bf1[6] = half_btf(cospi[52], bf0[6], cospi[12], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[12], bf0[6], -cospi[52], bf0[7], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[4], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[5], stage_range[stage]);
        bf1[2] = clamp_value(bf0[2] + bf0[6], stage_range[stage]);
        bf1[3] = clamp_value(bf0[3] + bf0[7], stage_range[stage]);
        bf1[4] = clamp_value(bf0[0] - bf0[4], stage_range[stage]);
        bf1[5] = clamp_value(bf0[1] - bf0[5], stage_range[stage]);
        bf1[6] = clamp_value(bf0[2] - bf0[6], stage_range[stage]);
        bf1[7] = clamp_value(bf0[3] - bf0[7], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit);
        bf1[5] = half_btf(cospi[48], bf0[4], -cospi[16], bf0[5], cos_bit);
        bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[16], bf0[6], cospi[48], bf0[7], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[2], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[3], stage_range[stage]);
        bf1[2] = clamp_value(bf0[0] - bf0[2], stage_range[stage]);
        bf1[3] = clamp_value(bf0[1] - bf0[3], stage_range[stage]);
        bf1[4] = clamp_value(bf0[4] + bf0[6], stage_range[stage]);
        bf1[5] = clamp_value(bf0[5] + bf0[7], stage_range[stage]);
        bf1[6] = clamp_value(bf0[4] - bf0[6], stage_range[stage]);
        bf1[7] = clamp_value(bf0[5] - bf0[7], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 6
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit);
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[32], bf0[6], -cospi[32], bf0[7], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 7
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0];
        bf1[1] = -bf0[4];
        bf1[2] = bf0[6];
        bf1[3] = -bf0[2];
        bf1[4] = bf0[3];
        bf1[5] = -bf0[7];
        bf1[6] = bf0[5];
        bf1[7] = -bf0[1];
    }

    void av1_iadst16_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        assert(output != input);
        //const int32_t size = 16;
        const int32_t *cospi = cospi_arr(cos_bit);

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[16];

        // stage 0;

        // stage 1;
        stage++;
        bf1 = output;
        bf1[0] = input[15];
        bf1[1] = input[0];
        bf1[2] = input[13];
        bf1[3] = input[2];
        bf1[4] = input[11];
        bf1[5] = input[4];
        bf1[6] = input[9];
        bf1[7] = input[6];
        bf1[8] = input[7];
        bf1[9] = input[8];
        bf1[10] = input[5];
        bf1[11] = input[10];
        bf1[12] = input[3];
        bf1[13] = input[12];
        bf1[14] = input[1];
        bf1[15] = input[14];
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = half_btf(cospi[2], bf0[0], cospi[62], bf0[1], cos_bit);
        bf1[1] = half_btf(cospi[62], bf0[0], -cospi[2], bf0[1], cos_bit);
        bf1[2] = half_btf(cospi[10], bf0[2], cospi[54], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[54], bf0[2], -cospi[10], bf0[3], cos_bit);
        bf1[4] = half_btf(cospi[18], bf0[4], cospi[46], bf0[5], cos_bit);
        bf1[5] = half_btf(cospi[46], bf0[4], -cospi[18], bf0[5], cos_bit);
        bf1[6] = half_btf(cospi[26], bf0[6], cospi[38], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[38], bf0[6], -cospi[26], bf0[7], cos_bit);
        bf1[8] = half_btf(cospi[34], bf0[8], cospi[30], bf0[9], cos_bit);
        bf1[9] = half_btf(cospi[30], bf0[8], -cospi[34], bf0[9], cos_bit);
        bf1[10] = half_btf(cospi[42], bf0[10], cospi[22], bf0[11], cos_bit);
        bf1[11] = half_btf(cospi[22], bf0[10], -cospi[42], bf0[11], cos_bit);
        bf1[12] = half_btf(cospi[50], bf0[12], cospi[14], bf0[13], cos_bit);
        bf1[13] = half_btf(cospi[14], bf0[12], -cospi[50], bf0[13], cos_bit);
        bf1[14] = half_btf(cospi[58], bf0[14], cospi[6], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[6], bf0[14], -cospi[58], bf0[15], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[8], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[9], stage_range[stage]);
        bf1[2] = clamp_value(bf0[2] + bf0[10], stage_range[stage]);
        bf1[3] = clamp_value(bf0[3] + bf0[11], stage_range[stage]);
        bf1[4] = clamp_value(bf0[4] + bf0[12], stage_range[stage]);
        bf1[5] = clamp_value(bf0[5] + bf0[13], stage_range[stage]);
        bf1[6] = clamp_value(bf0[6] + bf0[14], stage_range[stage]);
        bf1[7] = clamp_value(bf0[7] + bf0[15], stage_range[stage]);
        bf1[8] = clamp_value(bf0[0] - bf0[8], stage_range[stage]);
        bf1[9] = clamp_value(bf0[1] - bf0[9], stage_range[stage]);
        bf1[10] = clamp_value(bf0[2] - bf0[10], stage_range[stage]);
        bf1[11] = clamp_value(bf0[3] - bf0[11], stage_range[stage]);
        bf1[12] = clamp_value(bf0[4] - bf0[12], stage_range[stage]);
        bf1[13] = clamp_value(bf0[5] - bf0[13], stage_range[stage]);
        bf1[14] = clamp_value(bf0[6] - bf0[14], stage_range[stage]);
        bf1[15] = clamp_value(bf0[7] - bf0[15], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = bf0[6];
        bf1[7] = bf0[7];
        bf1[8] = half_btf(cospi[8], bf0[8], cospi[56], bf0[9], cos_bit);
        bf1[9] = half_btf(cospi[56], bf0[8], -cospi[8], bf0[9], cos_bit);
        bf1[10] = half_btf(cospi[40], bf0[10], cospi[24], bf0[11], cos_bit);
        bf1[11] = half_btf(cospi[24], bf0[10], -cospi[40], bf0[11], cos_bit);
        bf1[12] = half_btf(-cospi[56], bf0[12], cospi[8], bf0[13], cos_bit);
        bf1[13] = half_btf(cospi[8], bf0[12], cospi[56], bf0[13], cos_bit);
        bf1[14] = half_btf(-cospi[24], bf0[14], cospi[40], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[40], bf0[14], cospi[24], bf0[15], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[4], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[5], stage_range[stage]);
        bf1[2] = clamp_value(bf0[2] + bf0[6], stage_range[stage]);
        bf1[3] = clamp_value(bf0[3] + bf0[7], stage_range[stage]);
        bf1[4] = clamp_value(bf0[0] - bf0[4], stage_range[stage]);
        bf1[5] = clamp_value(bf0[1] - bf0[5], stage_range[stage]);
        bf1[6] = clamp_value(bf0[2] - bf0[6], stage_range[stage]);
        bf1[7] = clamp_value(bf0[3] - bf0[7], stage_range[stage]);
        bf1[8] = clamp_value(bf0[8] + bf0[12], stage_range[stage]);
        bf1[9] = clamp_value(bf0[9] + bf0[13], stage_range[stage]);
        bf1[10] = clamp_value(bf0[10] + bf0[14], stage_range[stage]);
        bf1[11] = clamp_value(bf0[11] + bf0[15], stage_range[stage]);
        bf1[12] = clamp_value(bf0[8] - bf0[12], stage_range[stage]);
        bf1[13] = clamp_value(bf0[9] - bf0[13], stage_range[stage]);
        bf1[14] = clamp_value(bf0[10] - bf0[14], stage_range[stage]);
        bf1[15] = clamp_value(bf0[11] - bf0[15], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 6
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit);
        bf1[5] = half_btf(cospi[48], bf0[4], -cospi[16], bf0[5], cos_bit);
        bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[16], bf0[6], cospi[48], bf0[7], cos_bit);
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = bf0[10];
        bf1[11] = bf0[11];
        bf1[12] = half_btf(cospi[16], bf0[12], cospi[48], bf0[13], cos_bit);
        bf1[13] = half_btf(cospi[48], bf0[12], -cospi[16], bf0[13], cos_bit);
        bf1[14] = half_btf(-cospi[48], bf0[14], cospi[16], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[16], bf0[14], cospi[48], bf0[15], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 7
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = clamp_value(bf0[0] + bf0[2], stage_range[stage]);
        bf1[1] = clamp_value(bf0[1] + bf0[3], stage_range[stage]);
        bf1[2] = clamp_value(bf0[0] - bf0[2], stage_range[stage]);
        bf1[3] = clamp_value(bf0[1] - bf0[3], stage_range[stage]);
        bf1[4] = clamp_value(bf0[4] + bf0[6], stage_range[stage]);
        bf1[5] = clamp_value(bf0[5] + bf0[7], stage_range[stage]);
        bf1[6] = clamp_value(bf0[4] - bf0[6], stage_range[stage]);
        bf1[7] = clamp_value(bf0[5] - bf0[7], stage_range[stage]);
        bf1[8] = clamp_value(bf0[8] + bf0[10], stage_range[stage]);
        bf1[9] = clamp_value(bf0[9] + bf0[11], stage_range[stage]);
        bf1[10] = clamp_value(bf0[8] - bf0[10], stage_range[stage]);
        bf1[11] = clamp_value(bf0[9] - bf0[11], stage_range[stage]);
        bf1[12] = clamp_value(bf0[12] + bf0[14], stage_range[stage]);
        bf1[13] = clamp_value(bf0[13] + bf0[15], stage_range[stage]);
        bf1[14] = clamp_value(bf0[12] - bf0[14], stage_range[stage]);
        bf1[15] = clamp_value(bf0[13] - bf0[15], stage_range[stage]);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 8
        stage++;
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit);
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[32], bf0[6], -cospi[32], bf0[7], cos_bit);
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = half_btf(cospi[32], bf0[10], cospi[32], bf0[11], cos_bit);
        bf1[11] = half_btf(cospi[32], bf0[10], -cospi[32], bf0[11], cos_bit);
        bf1[12] = bf0[12];
        bf1[13] = bf0[13];
        bf1[14] = half_btf(cospi[32], bf0[14], cospi[32], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[32], bf0[14], -cospi[32], bf0[15], cos_bit);
        range_check_buf(stage, input, bf1, size, stage_range[stage]);

        // stage 9
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0];
        bf1[1] = -bf0[8];
        bf1[2] = bf0[12];
        bf1[3] = -bf0[4];
        bf1[4] = bf0[6];
        bf1[5] = -bf0[14];
        bf1[6] = bf0[10];
        bf1[7] = -bf0[2];
        bf1[8] = bf0[3];
        bf1[9] = -bf0[11];
        bf1[10] = bf0[15];
        bf1[11] = -bf0[7];
        bf1[12] = bf0[5];
        bf1[13] = -bf0[13];
        bf1[14] = bf0[9];
        bf1[15] = -bf0[1];
    }

    void av1_iadst32_new(const int32_t *input, int32_t *output, int8_t cos_bit, const int8_t *stage_range) {
        const int32_t size = 32;
        const int32_t *cospi;

        int32_t stage = 0;
        int32_t *bf0, *bf1;
        int32_t step[32];

        // stage 0;
        clamp_buf((int32_t *)input, size, stage_range[stage]);

        // stage 1;
        stage++;
        assert(output != input);
        bf1 = output;
        bf1[0] = input[0];
        bf1[1] = -input[31];
        bf1[2] = -input[15];
        bf1[3] = input[16];
        bf1[4] = -input[7];
        bf1[5] = input[24];
        bf1[6] = input[8];
        bf1[7] = -input[23];
        bf1[8] = -input[3];
        bf1[9] = input[28];
        bf1[10] = input[12];
        bf1[11] = -input[19];
        bf1[12] = input[4];
        bf1[13] = -input[27];
        bf1[14] = -input[11];
        bf1[15] = input[20];
        bf1[16] = -input[1];
        bf1[17] = input[30];
        bf1[18] = input[14];
        bf1[19] = -input[17];
        bf1[20] = input[6];
        bf1[21] = -input[25];
        bf1[22] = -input[9];
        bf1[23] = input[22];
        bf1[24] = input[2];
        bf1[25] = -input[29];
        bf1[26] = -input[13];
        bf1[27] = input[18];
        bf1[28] = -input[5];
        bf1[29] = input[26];
        bf1[30] = input[10];
        bf1[31] = -input[21];
        clamp_buf(bf1, size, stage_range[stage]);

        // stage 2
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit);
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[32], bf0[6], -cospi[32], bf0[7], cos_bit);
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = half_btf(cospi[32], bf0[10], cospi[32], bf0[11], cos_bit);
        bf1[11] = half_btf(cospi[32], bf0[10], -cospi[32], bf0[11], cos_bit);
        bf1[12] = bf0[12];
        bf1[13] = bf0[13];
        bf1[14] = half_btf(cospi[32], bf0[14], cospi[32], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[32], bf0[14], -cospi[32], bf0[15], cos_bit);
        bf1[16] = bf0[16];
        bf1[17] = bf0[17];
        bf1[18] = half_btf(cospi[32], bf0[18], cospi[32], bf0[19], cos_bit);
        bf1[19] = half_btf(cospi[32], bf0[18], -cospi[32], bf0[19], cos_bit);
        bf1[20] = bf0[20];
        bf1[21] = bf0[21];
        bf1[22] = half_btf(cospi[32], bf0[22], cospi[32], bf0[23], cos_bit);
        bf1[23] = half_btf(cospi[32], bf0[22], -cospi[32], bf0[23], cos_bit);
        bf1[24] = bf0[24];
        bf1[25] = bf0[25];
        bf1[26] = half_btf(cospi[32], bf0[26], cospi[32], bf0[27], cos_bit);
        bf1[27] = half_btf(cospi[32], bf0[26], -cospi[32], bf0[27], cos_bit);
        bf1[28] = bf0[28];
        bf1[29] = bf0[29];
        bf1[30] = half_btf(cospi[32], bf0[30], cospi[32], bf0[31], cos_bit);
        bf1[31] = half_btf(cospi[32], bf0[30], -cospi[32], bf0[31], cos_bit);
        clamp_buf(bf1, size, stage_range[stage]);

        // stage 3
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[2];
        bf1[1] = bf0[1] + bf0[3];
        bf1[2] = bf0[0] - bf0[2];
        bf1[3] = bf0[1] - bf0[3];
        bf1[4] = bf0[4] + bf0[6];
        bf1[5] = bf0[5] + bf0[7];
        bf1[6] = bf0[4] - bf0[6];
        bf1[7] = bf0[5] - bf0[7];
        bf1[8] = bf0[8] + bf0[10];
        bf1[9] = bf0[9] + bf0[11];
        bf1[10] = bf0[8] - bf0[10];
        bf1[11] = bf0[9] - bf0[11];
        bf1[12] = bf0[12] + bf0[14];
        bf1[13] = bf0[13] + bf0[15];
        bf1[14] = bf0[12] - bf0[14];
        bf1[15] = bf0[13] - bf0[15];
        bf1[16] = bf0[16] + bf0[18];
        bf1[17] = bf0[17] + bf0[19];
        bf1[18] = bf0[16] - bf0[18];
        bf1[19] = bf0[17] - bf0[19];
        bf1[20] = bf0[20] + bf0[22];
        bf1[21] = bf0[21] + bf0[23];
        bf1[22] = bf0[20] - bf0[22];
        bf1[23] = bf0[21] - bf0[23];
        bf1[24] = bf0[24] + bf0[26];
        bf1[25] = bf0[25] + bf0[27];
        bf1[26] = bf0[24] - bf0[26];
        bf1[27] = bf0[25] - bf0[27];
        bf1[28] = bf0[28] + bf0[30];
        bf1[29] = bf0[29] + bf0[31];
        bf1[30] = bf0[28] - bf0[30];
        bf1[31] = bf0[29] - bf0[31];
        clamp_buf(bf1, size, stage_range[stage]);

        // stage 4
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit);
        bf1[5] = half_btf(cospi[48], bf0[4], -cospi[16], bf0[5], cos_bit);
        bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[16], bf0[6], cospi[48], bf0[7], cos_bit);
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = bf0[10];
        bf1[11] = bf0[11];
        bf1[12] = half_btf(cospi[16], bf0[12], cospi[48], bf0[13], cos_bit);
        bf1[13] = half_btf(cospi[48], bf0[12], -cospi[16], bf0[13], cos_bit);
        bf1[14] = half_btf(-cospi[48], bf0[14], cospi[16], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[16], bf0[14], cospi[48], bf0[15], cos_bit);
        bf1[16] = bf0[16];
        bf1[17] = bf0[17];
        bf1[18] = bf0[18];
        bf1[19] = bf0[19];
        bf1[20] = half_btf(cospi[16], bf0[20], cospi[48], bf0[21], cos_bit);
        bf1[21] = half_btf(cospi[48], bf0[20], -cospi[16], bf0[21], cos_bit);
        bf1[22] = half_btf(-cospi[48], bf0[22], cospi[16], bf0[23], cos_bit);
        bf1[23] = half_btf(cospi[16], bf0[22], cospi[48], bf0[23], cos_bit);
        bf1[24] = bf0[24];
        bf1[25] = bf0[25];
        bf1[26] = bf0[26];
        bf1[27] = bf0[27];
        bf1[28] = half_btf(cospi[16], bf0[28], cospi[48], bf0[29], cos_bit);
        bf1[29] = half_btf(cospi[48], bf0[28], -cospi[16], bf0[29], cos_bit);
        bf1[30] = half_btf(-cospi[48], bf0[30], cospi[16], bf0[31], cos_bit);
        bf1[31] = half_btf(cospi[16], bf0[30], cospi[48], bf0[31], cos_bit);
        clamp_buf(bf1, size, stage_range[stage]);

        // stage 5
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[4];
        bf1[1] = bf0[1] + bf0[5];
        bf1[2] = bf0[2] + bf0[6];
        bf1[3] = bf0[3] + bf0[7];
        bf1[4] = bf0[0] - bf0[4];
        bf1[5] = bf0[1] - bf0[5];
        bf1[6] = bf0[2] - bf0[6];
        bf1[7] = bf0[3] - bf0[7];
        bf1[8] = bf0[8] + bf0[12];
        bf1[9] = bf0[9] + bf0[13];
        bf1[10] = bf0[10] + bf0[14];
        bf1[11] = bf0[11] + bf0[15];
        bf1[12] = bf0[8] - bf0[12];
        bf1[13] = bf0[9] - bf0[13];
        bf1[14] = bf0[10] - bf0[14];
        bf1[15] = bf0[11] - bf0[15];
        bf1[16] = bf0[16] + bf0[20];
        bf1[17] = bf0[17] + bf0[21];
        bf1[18] = bf0[18] + bf0[22];
        bf1[19] = bf0[19] + bf0[23];
        bf1[20] = bf0[16] - bf0[20];
        bf1[21] = bf0[17] - bf0[21];
        bf1[22] = bf0[18] - bf0[22];
        bf1[23] = bf0[19] - bf0[23];
        bf1[24] = bf0[24] + bf0[28];
        bf1[25] = bf0[25] + bf0[29];
        bf1[26] = bf0[26] + bf0[30];
        bf1[27] = bf0[27] + bf0[31];
        bf1[28] = bf0[24] - bf0[28];
        bf1[29] = bf0[25] - bf0[29];
        bf1[30] = bf0[26] - bf0[30];
        bf1[31] = bf0[27] - bf0[31];
        clamp_buf(bf1, size, stage_range[stage]);

        // stage 6
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = bf0[6];
        bf1[7] = bf0[7];
        bf1[8] = half_btf(cospi[8], bf0[8], cospi[56], bf0[9], cos_bit);
        bf1[9] = half_btf(cospi[56], bf0[8], -cospi[8], bf0[9], cos_bit);
        bf1[10] = half_btf(cospi[40], bf0[10], cospi[24], bf0[11], cos_bit);
        bf1[11] = half_btf(cospi[24], bf0[10], -cospi[40], bf0[11], cos_bit);
        bf1[12] = half_btf(-cospi[56], bf0[12], cospi[8], bf0[13], cos_bit);
        bf1[13] = half_btf(cospi[8], bf0[12], cospi[56], bf0[13], cos_bit);
        bf1[14] = half_btf(-cospi[24], bf0[14], cospi[40], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[40], bf0[14], cospi[24], bf0[15], cos_bit);
        bf1[16] = bf0[16];
        bf1[17] = bf0[17];
        bf1[18] = bf0[18];
        bf1[19] = bf0[19];
        bf1[20] = bf0[20];
        bf1[21] = bf0[21];
        bf1[22] = bf0[22];
        bf1[23] = bf0[23];
        bf1[24] = half_btf(cospi[8], bf0[24], cospi[56], bf0[25], cos_bit);
        bf1[25] = half_btf(cospi[56], bf0[24], -cospi[8], bf0[25], cos_bit);
        bf1[26] = half_btf(cospi[40], bf0[26], cospi[24], bf0[27], cos_bit);
        bf1[27] = half_btf(cospi[24], bf0[26], -cospi[40], bf0[27], cos_bit);
        bf1[28] = half_btf(-cospi[56], bf0[28], cospi[8], bf0[29], cos_bit);
        bf1[29] = half_btf(cospi[8], bf0[28], cospi[56], bf0[29], cos_bit);
        bf1[30] = half_btf(-cospi[24], bf0[30], cospi[40], bf0[31], cos_bit);
        bf1[31] = half_btf(cospi[40], bf0[30], cospi[24], bf0[31], cos_bit);
        clamp_buf(bf1, size, stage_range[stage]);

        // stage 7
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[8];
        bf1[1] = bf0[1] + bf0[9];
        bf1[2] = bf0[2] + bf0[10];
        bf1[3] = bf0[3] + bf0[11];
        bf1[4] = bf0[4] + bf0[12];
        bf1[5] = bf0[5] + bf0[13];
        bf1[6] = bf0[6] + bf0[14];
        bf1[7] = bf0[7] + bf0[15];
        bf1[8] = bf0[0] - bf0[8];
        bf1[9] = bf0[1] - bf0[9];
        bf1[10] = bf0[2] - bf0[10];
        bf1[11] = bf0[3] - bf0[11];
        bf1[12] = bf0[4] - bf0[12];
        bf1[13] = bf0[5] - bf0[13];
        bf1[14] = bf0[6] - bf0[14];
        bf1[15] = bf0[7] - bf0[15];
        bf1[16] = bf0[16] + bf0[24];
        bf1[17] = bf0[17] + bf0[25];
        bf1[18] = bf0[18] + bf0[26];
        bf1[19] = bf0[19] + bf0[27];
        bf1[20] = bf0[20] + bf0[28];
        bf1[21] = bf0[21] + bf0[29];
        bf1[22] = bf0[22] + bf0[30];
        bf1[23] = bf0[23] + bf0[31];
        bf1[24] = bf0[16] - bf0[24];
        bf1[25] = bf0[17] - bf0[25];
        bf1[26] = bf0[18] - bf0[26];
        bf1[27] = bf0[19] - bf0[27];
        bf1[28] = bf0[20] - bf0[28];
        bf1[29] = bf0[21] - bf0[29];
        bf1[30] = bf0[22] - bf0[30];
        bf1[31] = bf0[23] - bf0[31];
        clamp_buf(bf1, size, stage_range[stage]);

        // stage 8
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = bf0[0];
        bf1[1] = bf0[1];
        bf1[2] = bf0[2];
        bf1[3] = bf0[3];
        bf1[4] = bf0[4];
        bf1[5] = bf0[5];
        bf1[6] = bf0[6];
        bf1[7] = bf0[7];
        bf1[8] = bf0[8];
        bf1[9] = bf0[9];
        bf1[10] = bf0[10];
        bf1[11] = bf0[11];
        bf1[12] = bf0[12];
        bf1[13] = bf0[13];
        bf1[14] = bf0[14];
        bf1[15] = bf0[15];
        bf1[16] = half_btf(cospi[4], bf0[16], cospi[60], bf0[17], cos_bit);
        bf1[17] = half_btf(cospi[60], bf0[16], -cospi[4], bf0[17], cos_bit);
        bf1[18] = half_btf(cospi[20], bf0[18], cospi[44], bf0[19], cos_bit);
        bf1[19] = half_btf(cospi[44], bf0[18], -cospi[20], bf0[19], cos_bit);
        bf1[20] = half_btf(cospi[36], bf0[20], cospi[28], bf0[21], cos_bit);
        bf1[21] = half_btf(cospi[28], bf0[20], -cospi[36], bf0[21], cos_bit);
        bf1[22] = half_btf(cospi[52], bf0[22], cospi[12], bf0[23], cos_bit);
        bf1[23] = half_btf(cospi[12], bf0[22], -cospi[52], bf0[23], cos_bit);
        bf1[24] = half_btf(-cospi[60], bf0[24], cospi[4], bf0[25], cos_bit);
        bf1[25] = half_btf(cospi[4], bf0[24], cospi[60], bf0[25], cos_bit);
        bf1[26] = half_btf(-cospi[44], bf0[26], cospi[20], bf0[27], cos_bit);
        bf1[27] = half_btf(cospi[20], bf0[26], cospi[44], bf0[27], cos_bit);
        bf1[28] = half_btf(-cospi[28], bf0[28], cospi[36], bf0[29], cos_bit);
        bf1[29] = half_btf(cospi[36], bf0[28], cospi[28], bf0[29], cos_bit);
        bf1[30] = half_btf(-cospi[12], bf0[30], cospi[52], bf0[31], cos_bit);
        bf1[31] = half_btf(cospi[52], bf0[30], cospi[12], bf0[31], cos_bit);
        clamp_buf(bf1, size, stage_range[stage]);

        // stage 9
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[0] + bf0[16];
        bf1[1] = bf0[1] + bf0[17];
        bf1[2] = bf0[2] + bf0[18];
        bf1[3] = bf0[3] + bf0[19];
        bf1[4] = bf0[4] + bf0[20];
        bf1[5] = bf0[5] + bf0[21];
        bf1[6] = bf0[6] + bf0[22];
        bf1[7] = bf0[7] + bf0[23];
        bf1[8] = bf0[8] + bf0[24];
        bf1[9] = bf0[9] + bf0[25];
        bf1[10] = bf0[10] + bf0[26];
        bf1[11] = bf0[11] + bf0[27];
        bf1[12] = bf0[12] + bf0[28];
        bf1[13] = bf0[13] + bf0[29];
        bf1[14] = bf0[14] + bf0[30];
        bf1[15] = bf0[15] + bf0[31];
        bf1[16] = bf0[0] - bf0[16];
        bf1[17] = bf0[1] - bf0[17];
        bf1[18] = bf0[2] - bf0[18];
        bf1[19] = bf0[3] - bf0[19];
        bf1[20] = bf0[4] - bf0[20];
        bf1[21] = bf0[5] - bf0[21];
        bf1[22] = bf0[6] - bf0[22];
        bf1[23] = bf0[7] - bf0[23];
        bf1[24] = bf0[8] - bf0[24];
        bf1[25] = bf0[9] - bf0[25];
        bf1[26] = bf0[10] - bf0[26];
        bf1[27] = bf0[11] - bf0[27];
        bf1[28] = bf0[12] - bf0[28];
        bf1[29] = bf0[13] - bf0[29];
        bf1[30] = bf0[14] - bf0[30];
        bf1[31] = bf0[15] - bf0[31];
        clamp_buf(bf1, size, stage_range[stage]);

        // stage 10
        stage++;
        cospi = cospi_arr(cos_bit);
        bf0 = output;
        bf1 = step;
        bf1[0] = half_btf(cospi[1], bf0[0], cospi[63], bf0[1], cos_bit);
        bf1[1] = half_btf(cospi[63], bf0[0], -cospi[1], bf0[1], cos_bit);
        bf1[2] = half_btf(cospi[5], bf0[2], cospi[59], bf0[3], cos_bit);
        bf1[3] = half_btf(cospi[59], bf0[2], -cospi[5], bf0[3], cos_bit);
        bf1[4] = half_btf(cospi[9], bf0[4], cospi[55], bf0[5], cos_bit);
        bf1[5] = half_btf(cospi[55], bf0[4], -cospi[9], bf0[5], cos_bit);
        bf1[6] = half_btf(cospi[13], bf0[6], cospi[51], bf0[7], cos_bit);
        bf1[7] = half_btf(cospi[51], bf0[6], -cospi[13], bf0[7], cos_bit);
        bf1[8] = half_btf(cospi[17], bf0[8], cospi[47], bf0[9], cos_bit);
        bf1[9] = half_btf(cospi[47], bf0[8], -cospi[17], bf0[9], cos_bit);
        bf1[10] = half_btf(cospi[21], bf0[10], cospi[43], bf0[11], cos_bit);
        bf1[11] = half_btf(cospi[43], bf0[10], -cospi[21], bf0[11], cos_bit);
        bf1[12] = half_btf(cospi[25], bf0[12], cospi[39], bf0[13], cos_bit);
        bf1[13] = half_btf(cospi[39], bf0[12], -cospi[25], bf0[13], cos_bit);
        bf1[14] = half_btf(cospi[29], bf0[14], cospi[35], bf0[15], cos_bit);
        bf1[15] = half_btf(cospi[35], bf0[14], -cospi[29], bf0[15], cos_bit);
        bf1[16] = half_btf(cospi[33], bf0[16], cospi[31], bf0[17], cos_bit);
        bf1[17] = half_btf(cospi[31], bf0[16], -cospi[33], bf0[17], cos_bit);
        bf1[18] = half_btf(cospi[37], bf0[18], cospi[27], bf0[19], cos_bit);
        bf1[19] = half_btf(cospi[27], bf0[18], -cospi[37], bf0[19], cos_bit);
        bf1[20] = half_btf(cospi[41], bf0[20], cospi[23], bf0[21], cos_bit);
        bf1[21] = half_btf(cospi[23], bf0[20], -cospi[41], bf0[21], cos_bit);
        bf1[22] = half_btf(cospi[45], bf0[22], cospi[19], bf0[23], cos_bit);
        bf1[23] = half_btf(cospi[19], bf0[22], -cospi[45], bf0[23], cos_bit);
        bf1[24] = half_btf(cospi[49], bf0[24], cospi[15], bf0[25], cos_bit);
        bf1[25] = half_btf(cospi[15], bf0[24], -cospi[49], bf0[25], cos_bit);
        bf1[26] = half_btf(cospi[53], bf0[26], cospi[11], bf0[27], cos_bit);
        bf1[27] = half_btf(cospi[11], bf0[26], -cospi[53], bf0[27], cos_bit);
        bf1[28] = half_btf(cospi[57], bf0[28], cospi[7], bf0[29], cos_bit);
        bf1[29] = half_btf(cospi[7], bf0[28], -cospi[57], bf0[29], cos_bit);
        bf1[30] = half_btf(cospi[61], bf0[30], cospi[3], bf0[31], cos_bit);
        bf1[31] = half_btf(cospi[3], bf0[30], -cospi[61], bf0[31], cos_bit);
        clamp_buf(bf1, size, stage_range[stage]);

        // stage 11
        stage++;
        bf0 = step;
        bf1 = output;
        bf1[0] = bf0[1];
        bf1[1] = bf0[30];
        bf1[2] = bf0[3];
        bf1[3] = bf0[28];
        bf1[4] = bf0[5];
        bf1[5] = bf0[26];
        bf1[6] = bf0[7];
        bf1[7] = bf0[24];
        bf1[8] = bf0[9];
        bf1[9] = bf0[22];
        bf1[10] = bf0[11];
        bf1[11] = bf0[20];
        bf1[12] = bf0[13];
        bf1[13] = bf0[18];
        bf1[14] = bf0[15];
        bf1[15] = bf0[16];
        bf1[16] = bf0[17];
        bf1[17] = bf0[14];
        bf1[18] = bf0[19];
        bf1[19] = bf0[12];
        bf1[20] = bf0[21];
        bf1[21] = bf0[10];
        bf1[22] = bf0[23];
        bf1[23] = bf0[8];
        bf1[24] = bf0[25];
        bf1[25] = bf0[6];
        bf1[26] = bf0[27];
        bf1[27] = bf0[4];
        bf1[28] = bf0[29];
        bf1[29] = bf0[2];
        bf1[30] = bf0[31];
        bf1[31] = bf0[0];
        clamp_buf(bf1, size, stage_range[stage]);
    }

    static inline TxfmFunc inv_txfm_type_to_func(TXFM_TYPE txfm_type)
    {
        switch (txfm_type) {
        case TXFM_TYPE_DCT4: return av1_idct4_new;
        case TXFM_TYPE_DCT8: return av1_idct8_new;
        case TXFM_TYPE_DCT16: return av1_idct16_new;
        case TXFM_TYPE_DCT32: return av1_idct32_new;
#if CONFIG_TX64X64
        case TXFM_TYPE_DCT64: return av1_idct64_new;
#endif  // CONFIG_TX64X64
        case TXFM_TYPE_ADST4: return av1_iadst4_new;
        case TXFM_TYPE_ADST8: return av1_iadst8_new;
        case TXFM_TYPE_ADST16: return av1_iadst16_new;
        case TXFM_TYPE_ADST32: return av1_iadst32_new;
#if CONFIG_EXT_TX
        case TXFM_TYPE_IDENTITY4: return av1_iidentity4_c;
        case TXFM_TYPE_IDENTITY8: return av1_iidentity8_c;
        case TXFM_TYPE_IDENTITY16: return av1_iidentity16_c;
        case TXFM_TYPE_IDENTITY32: return av1_iidentity32_c;
#if CONFIG_TX64X64
        case TXFM_TYPE_IDENTITY64: return av1_iidentity64_c;
#endif  // CONFIG_TX64X64
#endif  // CONFIG_EXT_TX
        default: assert(0); return NULL;
        }
    }

    static inline int clamp(int value, int low, int high) {
        return value < low ? low : (value > high ? high : value);
    }

    static inline uint16_t clip_pixel_highbd(int val, int bd)
    {
        switch (bd) {
        case 8:
        default: return (uint16_t)clamp(val, 0, 255);
        case 10: return (uint16_t)clamp(val, 0, 1023);
        case 12: return (uint16_t)clamp(val, 0, 4095);
        }
    }
    static inline tran_high_t check_range_bd(tran_high_t input, int bd)
    {
#if CONFIG_COEFFICIENT_RANGE_CHECKING
        // For valid AV1 input streams, intermediate stage coefficients should always
        // stay within the range of a signed 16 bit integer. Coefficients can go out
        // of this range for invalid/corrupt AV1 streams. However, strictly checking
        // this range for every intermediate coefficient can burdensome for a decoder,
        // therefore the following assertion is only enabled when configured with
        // --enable-coefficient-range-checking.
        // For valid highbitdepth AV1 streams, intermediate stage coefficients will
        // stay within the ranges:
        // - 8 bit: signed 16 bit integer
        // - 10 bit: signed 18 bit integer
        // - 12 bit: signed 20 bit integer
        const int32_t int_max = (1 << (7 + bd)) - 1;
        const int32_t int_min = -int_max - 1;
        assert(int_min <= input);
        assert(input <= int_max);
        (void)int_min;
#endif  // CONFIG_COEFFICIENT_RANGE_CHECKING
        (void)bd;
        return input;
    }

#define HIGHBD_WRAPLOW(x, bd) ((int32_t)check_range_bd((x), bd))

    static inline uint16_t highbd_clip_pixel_add(uint16_t dest, tran_high_t trans, int bd)
    {
        trans = HIGHBD_WRAPLOW(trans, bd);
        return clip_pixel_highbd(dest + (int)trans, bd);
    }

    static inline void inv_txfm2d_add_c(const int32_t *input, uint16_t *output, int stride, TXFM_2D_FLIP_CFG *cfg,
                                        int32_t *txfm_buf, TX_SIZE tx_size, int bd, int useAdd)
    {
        // Note when assigning txfm_size_col, we use the txfm_size from the
        // row configuration and vice versa. This is intentionally done to
        // accurately perform rectangular transforms. When the transform is
        // rectangular, the number of columns will be the same as the
        // txfm_size stored in the row cfg struct. It will make no difference
        // for square transforms.
        const int txfm_size_col = tx_size_wide[cfg->tx_size];
        const int txfm_size_row = tx_size_high[cfg->tx_size];
        // Take the shift from the larger dimension in the rectangular case.
        const int8_t *shift = cfg->shift;
        const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
        int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
        int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
        assert(cfg->stage_num_row <= MAX_TXFM_STAGE_NUM);
        assert(cfg->stage_num_col <= MAX_TXFM_STAGE_NUM);
        av1_gen_inv_stage_range(stage_range_col, stage_range_row, cfg, tx_size, bd);

        const int8_t cos_bit_col = cfg->cos_bit_col;
        const int8_t cos_bit_row = cfg->cos_bit_row;
        const TxfmFunc txfm_func_col = inv_txfm_type_to_func(cfg->txfm_type_col);
        const TxfmFunc txfm_func_row = inv_txfm_type_to_func(cfg->txfm_type_row);

        // txfm_buf's length is  txfm_size_row * txfm_size_col + 2 *
        // AOMMAX(txfm_size_row, txfm_size_col)
        // it is used for intermediate data buffering
        const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
        int32_t *temp_in = txfm_buf;
        int32_t *temp_out = temp_in + buf_offset;
        int32_t *buf = temp_out + buf_offset;
        int32_t *buf_ptr = buf;
        int c, r;

        // Rows
        for (r = 0; r < txfm_size_row; ++r) {
            if (abs(rect_type) == 1) {
                for (c = 0; c < txfm_size_col; ++c) {
                    temp_in[c] = round_shift(input[c] * NewInvSqrt2, NewSqrt2Bits);
                }
                clamp_buf(temp_in, txfm_size_col, bd + 8);
                txfm_func_row(temp_in, buf_ptr, cos_bit_row, stage_range_row);
            } else {
                for (c = 0; c < txfm_size_col; ++c) {
                    temp_in[c] = input[c];
                }
                clamp_buf(temp_in, txfm_size_col, bd + 8);
                txfm_func_row(temp_in, buf_ptr, cos_bit_row, stage_range_row);
            }
            av1_round_shift_array_c(buf_ptr, txfm_size_col, -shift[0]);
            input += txfm_size_col;
            buf_ptr += txfm_size_col;
        }

        // Columns
        for (c = 0; c < txfm_size_col; ++c) {
            if (cfg->lr_flip == 0) {
                for (r = 0; r < txfm_size_row; ++r)
                    temp_in[r] = buf[r * txfm_size_col + c];
            } else {
                // flip left right
                for (r = 0; r < txfm_size_row; ++r)
                    temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
            }
            clamp_buf(temp_in, txfm_size_row, AOMMAX(bd + 6, 16));
            txfm_func_col(temp_in, temp_out, cos_bit_col, stage_range_col);
            av1_round_shift_array_c(temp_out, txfm_size_row, -shift[1]);
            if (cfg->ud_flip == 0) {
                for (r = 0; r < txfm_size_row; ++r) {
                    if (useAdd)
                        ((uint8_t*)output)[r * stride + c] = highbd_clip_pixel_add(((uint8_t*)output)[r * stride + c], temp_out[r], bd);
                    else
                        output[r * stride + c] = temp_out[r];
                }
            } else {
                // flip upside down
                for (r = 0; r < txfm_size_row; ++r) {
                    if (useAdd)
                        ((uint8_t*)output)[r * stride + c] = highbd_clip_pixel_add(((uint8_t*)output)[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
                    else
                        output[r * stride + c] = temp_out[txfm_size_row - r - 1];
                }
            }
        }
    }

    static inline void inv_txfm2d_add_facade(const int32_t *input, uint16_t *output, int stride, int32_t *txfm_buf,
                                             TX_TYPE tx_type, TX_SIZE tx_size, int bd, int useAdd)
    {
        TXFM_2D_FLIP_CFG cfg;
        av1_get_inv_txfm_cfg(tx_type, tx_size, &cfg);
        // Forward shift sum uses larger square size, to be consistent with what
        // av1_gen_inv_stage_range() does for inverse shifts.
        inv_txfm2d_add_c(input, output, stride, &cfg, txfm_buf, tx_size, bd, useAdd);
    }

    void av1_inv_txfm2d_add_4x4_px(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd, int useAdd)
    {
        int txfm_buf[4 * 4 + 4 + 4];
        inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_4X4, bd, useAdd);
    }

    void av1_inv_txfm2d_add_8x8_px(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd, int useAdd)
    {
        int txfm_buf[8 * 8 + 8 + 8];
        inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_8X8, bd, useAdd);
    }

    void av1_inv_txfm2d_add_16x16_px(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd, int useAdd)
    {
        int txfm_buf[16 * 16 + 16 + 16];
        inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_16X16, bd, useAdd);
    }

    void av1_inv_txfm2d_add_32x32_px(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd, int useAdd)
    {
        int txfm_buf[32 * 32 + 32 + 32];
        inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_32X32, bd, useAdd);
    }

}; // namespace details

namespace AV1PP {
    template <int size, int type> void ftransform_px(const int16_t *src, int16_t *dst, int pitchSrc) {
        if (size == 0)      details::fht4x4_px(src, dst, pitchSrc, type);
        else if (size == 1) details::fht8x8_px(src, dst, pitchSrc, type);
        else if (size == 2) details::fht16x16_px(src, dst, pitchSrc, type);
        else if (size == 3) details::fdct32x32_px(src, dst, pitchSrc);
        else {assert(0);}
    }
    template void ftransform_px<0, 0>(const int16_t*,int16_t*,int);
    template void ftransform_px<0, 1>(const int16_t*,int16_t*,int);
    template void ftransform_px<0, 2>(const int16_t*,int16_t*,int);
    template void ftransform_px<0, 3>(const int16_t*,int16_t*,int);
    template void ftransform_px<1, 0>(const int16_t*,int16_t*,int);
    template void ftransform_px<1, 1>(const int16_t*,int16_t*,int);
    template void ftransform_px<1, 2>(const int16_t*,int16_t*,int);
    template void ftransform_px<1, 3>(const int16_t*,int16_t*,int);
    template void ftransform_px<2, 0>(const int16_t*,int16_t*,int);
    template void ftransform_px<2, 1>(const int16_t*,int16_t*,int);
    template void ftransform_px<2, 2>(const int16_t*,int16_t*,int);
    template void ftransform_px<2, 3>(const int16_t*,int16_t*,int);
    template void ftransform_px<3, 0>(const int16_t*,int16_t*,int);

    template <int size, int type> void ftransform_hbd_px(const int16_t *src, int32_t *dst, int pitchSrc) {
        if (size == 0)      details::av1_fwd_txfm2d_4x4_c(src, dst, pitchSrc, type, 8);
        else if (size == 1) details::av1_fwd_txfm2d_8x8_c(src, dst, pitchSrc, type, 8);
        else if (size == 2) details::av1_fwd_txfm2d_16x16_c(src, dst, pitchSrc, type, 8);
        else if (size == 3) details::av1_fwd_txfm2d_32x32_c(src, dst, pitchSrc, type, 8);
        else {assert(0);}
    }
    template void ftransform_hbd_px<0, 0>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<0, 1>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<0, 2>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<0, 3>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<1, 0>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<1, 1>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<1, 2>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<1, 3>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<2, 0>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<2, 1>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<2, 2>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<2, 3>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_px<3, 0>(const int16_t*,int32_t*,int);

    template <int size, int type> void itransform_px(const int16_t *src, int16_t *dst, int pitchSrc) {
        if (size == 0)      details::iht4x4_16_px(src, dst, pitchSrc, type);
        else if (size == 1) details::iht8x8_64_px(src, dst, pitchSrc, type, 64);
        else if (size == 2) details::iht16x16_256_px(src, dst, pitchSrc, type, 256);
        else if (size == 3) details::idct32x32_px(src, dst, pitchSrc, 1024);
        else {assert(0);}
    }
    template void itransform_px<0, 0>(const int16_t*,int16_t*,int);
    template void itransform_px<0, 1>(const int16_t*,int16_t*,int);
    template void itransform_px<0, 2>(const int16_t*,int16_t*,int);
    template void itransform_px<0, 3>(const int16_t*,int16_t*,int);
    template void itransform_px<1, 0>(const int16_t*,int16_t*,int);
    template void itransform_px<1, 1>(const int16_t*,int16_t*,int);
    template void itransform_px<1, 2>(const int16_t*,int16_t*,int);
    template void itransform_px<1, 3>(const int16_t*,int16_t*,int);
    template void itransform_px<2, 0>(const int16_t*,int16_t*,int);
    template void itransform_px<2, 1>(const int16_t*,int16_t*,int);
    template void itransform_px<2, 2>(const int16_t*,int16_t*,int);
    template void itransform_px<2, 3>(const int16_t*,int16_t*,int);
    template void itransform_px<3, 0>(const int16_t*,int16_t*,int);

    template <int size, int type> void itransform_add_px(const int16_t *src, uint8_t *dst, int pitchSrc) {
        if (size == 0)      details::iht4x4_16_add_px(src, dst, pitchSrc, type);
        else if (size == 1) details::iht8x8_64_add_px(src, dst, pitchSrc, type, 64);
        else if (size == 2) details::iht16x16_256_add_px(src, dst, pitchSrc, type, 256);
        else if (size == 3) details::idct32x32_add_px(src, dst, pitchSrc, 1024);
        else {assert(0);}
    }
    template void itransform_add_px<0, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<0, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<0, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<0, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<1, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<1, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<1, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<1, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<2, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<2, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<2, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<2, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_px<3, 0>(const int16_t*,uint8_t*,int);

    // view src as 16s
    template <int size, int type> void itransform_add_hbd_px(const int16_t *src, uint8_t *dst, int pitchDst) {
        int32_t src32s[32*32];
        int width = 4 << size;
        for (int pos = 0; pos < width*width; pos++) {
            src32s[pos] = (src)[pos];
        }

        if (size == 0)      details::av1_inv_txfm2d_add_4x4_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 1);
        else if (size == 1) details::av1_inv_txfm2d_add_8x8_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 1);
        else if (size == 2) details::av1_inv_txfm2d_add_16x16_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 1);
        else if (size == 3) details::av1_inv_txfm2d_add_32x32_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 1);
        else {assert(0);}
    }

    template void itransform_add_hbd_px<0, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<0, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<0, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<0, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<1, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<1, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<1, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<1, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<2, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<2, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<2, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<2, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_hbd_px<3, 0>(const int16_t*,uint8_t*,int);

    // view src as 16s
    template <int size, int type> void itransform_hbd_px(const int16_t *src, int16_t *dst, int pitchDst) {

        int32_t src32s[32*32];
        int width = 4 << size;
        for (int pos = 0; pos < width*width; pos++) {
            src32s[pos] = (src)[pos];
        }

        if (size == 0)      details::av1_inv_txfm2d_add_4x4_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 0);
        else if (size == 1) details::av1_inv_txfm2d_add_8x8_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 0);
        else if (size == 2) details::av1_inv_txfm2d_add_16x16_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 0);
        else if (size == 3) details::av1_inv_txfm2d_add_32x32_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 0);
        else {assert(0);}
    }

    template void itransform_hbd_px<0, 0>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<0, 1>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<0, 2>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<0, 3>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<1, 0>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<1, 1>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<1, 2>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<1, 3>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<2, 0>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<2, 1>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<2, 2>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<2, 3>(const int16_t*,int16_t*,int);
    template void itransform_hbd_px<3, 0>(const int16_t*,int16_t*,int);
}; // namespace AV1PP
