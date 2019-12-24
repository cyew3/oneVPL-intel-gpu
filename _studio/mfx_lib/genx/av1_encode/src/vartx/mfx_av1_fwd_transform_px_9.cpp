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

#include "mfx_av1_transform_common.h"

using namespace VP9PP;

/* Shift down with rounding */
#define ROUND_POWER_OF_TWO(value, n) \
    (((value) + (1 << ((n) - 1))) >> (n))

#define DCT_CONST_BITS 14

static inline int32_t fdct_round_shift(int32_t input) {
    int32_t rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
    return rv;
}

// Constants:
//  for (int i = 1; i< 32; ++i)
//    printf("static const int cospi_%d_64 = %.0f;\n", i,
//           round(16384 * cos(i*M_PI/64)));
// Note: sin(k*Pi/64) = cos((32-k)*Pi/64)
static const int32_t cospi_1_64  = 16364;
static const int32_t cospi_2_64  = 16305;
static const int32_t cospi_3_64  = 16207;
static const int32_t cospi_4_64  = 16069;
static const int32_t cospi_5_64  = 15893;
static const int32_t cospi_6_64  = 15679;
static const int32_t cospi_7_64  = 15426;
static const int32_t cospi_8_64  = 15137;
static const int32_t cospi_9_64  = 14811;
static const int32_t cospi_10_64 = 14449;
static const int32_t cospi_11_64 = 14053;
static const int32_t cospi_12_64 = 13623;
static const int32_t cospi_13_64 = 13160;
static const int32_t cospi_14_64 = 12665;
static const int32_t cospi_15_64 = 12140;
static const int32_t cospi_16_64 = 11585;
static const int32_t cospi_17_64 = 11003;
static const int32_t cospi_18_64 = 10394;
static const int32_t cospi_19_64 = 9760;
static const int32_t cospi_20_64 = 9102;
static const int32_t cospi_21_64 = 8423;
static const int32_t cospi_22_64 = 7723;
static const int32_t cospi_23_64 = 7005;
static const int32_t cospi_24_64 = 6270;
static const int32_t cospi_25_64 = 5520;
static const int32_t cospi_26_64 = 4756;
static const int32_t cospi_27_64 = 3981;
static const int32_t cospi_28_64 = 3196;
static const int32_t cospi_29_64 = 2404;
static const int32_t cospi_30_64 = 1606;
static const int32_t cospi_31_64 = 804;

//  16384 * sqrt(2) * sin(kPi/9) * 2 / 3
static const int32_t sinpi_1_9 = 5283;
static const int32_t sinpi_2_9 = 9929;
static const int32_t sinpi_3_9 = 13377;
static const int32_t sinpi_4_9 = 15212;

static void fadst16(const int16_t *input, int16_t *output)
{
    int32_t s0, s1, s2, s3, s4, s5, s6, s7, s8;
    int32_t s9, s10, s11, s12, s13, s14, s15;

    int32_t x0 = input[15];
    int32_t x1 = input[0];
    int32_t x2 = input[13];
    int32_t x3 = input[2];
    int32_t x4 = input[11];
    int32_t x5 = input[4];
    int32_t x6 = input[9];
    int32_t x7 = input[6];
    int32_t x8 = input[7];
    int32_t x9 = input[8];
    int32_t x10 = input[5];
    int32_t x11 = input[10];
    int32_t x12 = input[3];
    int32_t x13 = input[12];
    int32_t x14 = input[1];
    int32_t x15 = input[14];

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

    output[0] = (int16_t)x0;
    output[1] = (int16_t)-x8;
    output[2] = (int16_t)x12;
    output[3] = (int16_t)-x4;
    output[4] = (int16_t)x6;
    output[5] = (int16_t)x14;
    output[6] = (int16_t)x10;
    output[7] = (int16_t)x2;
    output[8] = (int16_t)x3;
    output[9] = (int16_t)x11;
    output[10] = (int16_t)x15;
    output[11] = (int16_t)x7;
    output[12] = (int16_t)x5;
    output[13] = (int16_t)-x13;
    output[14] = (int16_t)x9;
    output[15] = (int16_t)-x1;
}

static void fadst8(const int16_t *input, int16_t *output)
{
    int32_t s0, s1, s2, s3, s4, s5, s6, s7;

    int32_t x0 = input[7];
    int32_t x1 = input[0];
    int32_t x2 = input[5];
    int32_t x3 = input[2];
    int32_t x4 = input[3];
    int32_t x5 = input[4];
    int32_t x6 = input[1];
    int32_t x7 = input[6];

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

    output[0] = (int16_t)x0;
    output[1] = (int16_t)-x4;
    output[2] = (int16_t)x6;
    output[3] = (int16_t)-x2;
    output[4] = (int16_t)x3;
    output[5] = (int16_t)-x7;
    output[6] = (int16_t)x5;
    output[7] = (int16_t)-x1;
}

static void fadst4(const int16_t *input, int16_t *output)
{
    int32_t x0, x1, x2, x3;
    int32_t s0, s1, s2, s3, s4, s5, s6, s7;

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
    output[0] = (int16_t)fdct_round_shift(s0);
    output[1] = (int16_t)fdct_round_shift(s1);
    output[2] = (int16_t)fdct_round_shift(s2);
    output[3] = (int16_t)fdct_round_shift(s3);
}


static void fdct16(const int16_t in[16], int16_t out[16])
{
    int32_t step1[8];      // canbe16
    int32_t step2[8];      // canbe16
    int32_t step3[8];      // canbe16
    int32_t input[8];      // canbe16
    int32_t temp1, temp2;  // needs32

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
        int32_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
        int32_t t0, t1, t2, t3;                  // needs32
        int32_t x0, x1, x2, x3;                  // canbe16

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
        out[0] = (int16_t)fdct_round_shift(t0);
        out[4] = (int16_t)fdct_round_shift(t2);
        out[8] = (int16_t)fdct_round_shift(t1);
        out[12] = (int16_t)fdct_round_shift(t3);

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
        out[2] = (int16_t)fdct_round_shift(t0);
        out[6] = (int16_t)fdct_round_shift(t2);
        out[10] = (int16_t)fdct_round_shift(t1);
        out[14] = (int16_t)fdct_round_shift(t3);
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
    out[1] = (int16_t)fdct_round_shift(temp1);
    out[9] = (int16_t)fdct_round_shift(temp2);

    temp1 = step1[2] * cospi_22_64 + step1[5] * cospi_10_64;
    temp2 = step1[3] *  cospi_6_64 + step1[4] * cospi_26_64;
    out[5] = (int16_t)fdct_round_shift(temp1);
    out[13] = (int16_t)fdct_round_shift(temp2);

    temp1 = step1[3] * -cospi_26_64 + step1[4] *  cospi_6_64;
    temp2 = step1[2] * -cospi_10_64 + step1[5] * cospi_22_64;
    out[3] = (int16_t)fdct_round_shift(temp1);
    out[11] = (int16_t)fdct_round_shift(temp2);

    temp1 = step1[1] * -cospi_18_64 + step1[6] * cospi_14_64;
    temp2 = step1[0] *  -cospi_2_64 + step1[7] * cospi_30_64;
    out[7] = (int16_t)fdct_round_shift(temp1);
    out[15] = (int16_t)fdct_round_shift(temp2);
}

static void fdct8(const int16_t *input, int16_t *output)
{
    int32_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
    int32_t t0, t1, t2, t3;                  // needs32
    int32_t x0, x1, x2, x3;                  // canbe16

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
    output[0] = (int16_t)fdct_round_shift(t0);
    output[2] = (int16_t)fdct_round_shift(t2);
    output[4] = (int16_t)fdct_round_shift(t1);
    output[6] = (int16_t)fdct_round_shift(t3);

    // Stage 2
    t0 = (s6 - s5) * cospi_16_64;
    t1 = (s6 + s5) * cospi_16_64;
    t2 = (int16_t)fdct_round_shift(t0);
    t3 = (int16_t)fdct_round_shift(t1);

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
    output[1] = (int16_t)fdct_round_shift(t0);
    output[3] = (int16_t)fdct_round_shift(t2);
    output[5] = (int16_t)fdct_round_shift(t1);
    output[7] = (int16_t)fdct_round_shift(t3);
}

static void fdct4(const int16_t *input, int16_t *output)
{
    int32_t step[4];
    int32_t temp1, temp2;

    step[0] = input[0] + input[3];
    step[1] = input[1] + input[2];
    step[2] = input[1] - input[2];
    step[3] = input[0] - input[3];

    temp1 = (step[0] + step[1]) * cospi_16_64;
    temp2 = (step[0] - step[1]) * cospi_16_64;
    output[0] = (int16_t)fdct_round_shift(temp1);
    output[2] = (int16_t)fdct_round_shift(temp2);
    temp1 = step[2] * cospi_24_64 + step[3] * cospi_8_64;
    temp2 = -step[2] * cospi_8_64 + step[3] * cospi_24_64;
    output[1] = (int16_t)fdct_round_shift(temp1);
    output[3] = (int16_t)fdct_round_shift(temp2);
}

typedef void (*transform_1d)(const int16_t*, int16_t*);

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

static void fht4x4_px(const int16_t *input, int16_t *output, int stride, int tx_type)
{
    /*if (tx_type == DCT_DCT) {
    vpx_fdct4x4_c(input, output, stride);
    } else */
    {
        int16_t out[4 * 4];
        int i, j;
        int16_t temp_in[4], temp_out[4];
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

static void fht8x8_px(const int16_t *input, int16_t *output, int stride, int tx_type)
{
    /*if (tx_type == DCT_DCT) {
    vpx_fdct8x8_c(input, output, stride);
    } else */
    {
        int16_t out[64];
        int i, j;
        int16_t temp_in[8], temp_out[8];
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
static void fdct16x16_px(const int16_t *input, int16_t *output, int stride)
{
    // The 2D transform is done with two passes which are actually pretty
    // similar. In the first one, we transform the columns and transpose
    // the results. In the second one, we transform the rows. To achieve that,
    // as the first pass results are transposed, we transpose the columns (that
    // is the transposed rows) and transpose the results (so that it goes back
    // in normal/row positions).
    int pass;
    // We need an intermediate buffer between passes.
    int16_t intermediate[256];
    const int16_t *in_pass0 = input;
    const int16_t *in = NULL;
    int16_t *out = intermediate;
    // Do the two transform/transpose passes
    for (pass = 0; pass < 2; ++pass) {
        int32_t step1[8];      // canbe16
        int32_t step2[8];      // canbe16
        int32_t step3[8];      // canbe16
        int32_t input[8];      // canbe16
        int32_t temp1, temp2;  // needs32
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
                int32_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
                int32_t t0, t1, t2, t3;                  // needs32
                int32_t x0, x1, x2, x3;                  // canbe16

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
                out[0] = (int16_t)fdct_round_shift(t0);
                out[4] = (int16_t)fdct_round_shift(t2);
                out[8] = (int16_t)fdct_round_shift(t1);
                out[12] = (int16_t)fdct_round_shift(t3);

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
                out[2] = (int16_t)fdct_round_shift(t0);
                out[6] = (int16_t)fdct_round_shift(t2);
                out[10] = (int16_t)fdct_round_shift(t1);
                out[14] = (int16_t)fdct_round_shift(t3);
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
                out[1] = (int16_t)fdct_round_shift(temp1);
                out[9] = (int16_t)fdct_round_shift(temp2);
                temp1 = step1[2] * cospi_22_64 + step1[5] * cospi_10_64;
                temp2 = step1[3] * cospi_6_64 + step1[4] * cospi_26_64;
                out[5] = (int16_t)fdct_round_shift(temp1);
                out[13] = (int16_t)fdct_round_shift(temp2);
                temp1 = step1[3] * -cospi_26_64 + step1[4] * cospi_6_64;
                temp2 = step1[2] * -cospi_10_64 + step1[5] * cospi_22_64;
                out[3] = (int16_t)fdct_round_shift(temp1);
                out[11] = (int16_t)fdct_round_shift(temp2);
                temp1 = step1[1] * -cospi_18_64 + step1[6] * cospi_14_64;
                temp2 = step1[0] * -cospi_2_64 + step1[7] * cospi_30_64;
                out[7] = (int16_t)fdct_round_shift(temp1);
                out[15] = (int16_t)fdct_round_shift(temp2);
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

static void fht16x16_px(const int16_t *input, int16_t *output, int stride, int tx_type)
{
    if (tx_type == DCT_DCT) {
        fdct16x16_px(input, output, stride);
    } else {
        int16_t out[256];
        int i, j;
        int16_t temp_in[16], temp_out[16];
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

static inline int32_t dct_32_round(int32_t input)
{
    int32_t rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
    // TODO(debargha, peter.derivaz): Find new bounds for this assert,
    // and make the bounds consts.
    // assert(-131072 <= rv && rv <= 131071);
    return rv;
}

static inline int32_t half_round_shift(int32_t input)
{
    int32_t rv = (input + 1 + (input < 0)) >> 2;
    return rv;
}

static void vpx_fdct32(const int32_t *input, int32_t *output, int round)
{
    int32_t step[32];
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

static void fdct32x32_px(const int16_t *input, int16_t *out, int stride)
{
    int i, j;
    int32_t output[32 * 32];

    // Columns
    for (i = 0; i < 32; ++i) {
        int32_t temp_in[32], temp_out[32];
        for (j = 0; j < 32; ++j) temp_in[j] = input[j * stride + i] * 4;
        vpx_fdct32(temp_in, temp_out, 0);
        for (j = 0; j < 32; ++j)
            output[j * 32 + i] = (temp_out[j] + 1 + (temp_out[j] > 0)) >> 2;
    }

    // Rows
    for (i = 0; i < 32; ++i) {
        int32_t temp_in[32], temp_out[32];
        for (j = 0; j < 32; ++j) temp_in[j] = output[j + i * 32];
        vpx_fdct32(temp_in, temp_out, 0);
        for (j = 0; j < 32; ++j)
            out[j + i * 32] =
            (int16_t)((temp_out[j] + 1 + (temp_out[j] < 0)) >> 2);
    }
}

namespace VP9PP {
    template <int size, int type> void ftransform_vp9_px(const int16_t *src, int16_t *dst, int pitchSrc) {
        if (size == 0)      fht4x4_px(src, dst, pitchSrc, type);
        else if (size == 1) fht8x8_px(src, dst, pitchSrc, type);
        else if (size == 2) fht16x16_px(src, dst, pitchSrc, type);
        else if (size == 3) fdct32x32_px(src, dst, pitchSrc);
        else {assert(0);}
    }
    template void ftransform_vp9_px<0, 0>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<0, 1>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<0, 2>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<0, 3>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<1, 0>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<1, 1>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<1, 2>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<1, 3>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<2, 0>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<2, 1>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<2, 2>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<2, 3>(const int16_t*,int16_t*,int);
    template void ftransform_vp9_px<3, 0>(const int16_t*,int16_t*,int);
}; // namespace VP9PP
