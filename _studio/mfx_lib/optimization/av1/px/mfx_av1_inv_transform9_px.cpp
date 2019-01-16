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

#include "mfx_av1_transform_common.h"

using namespace AV1PP;

static inline uint8_t clip_pixel(int val) {
    return (val > 255) ? 255 : (val < 0) ? 0 : val;
}

/* Shift down with rounding */
#define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n) - 1))) >> (n))

#define DCT_CONST_BITS 14

static inline int dct_const_round_shift(int input) {
    int rv = ROUND_POWER_OF_TWO(input, DCT_CONST_BITS);
    return (int)rv;
}

static inline int check_range(int input)
{
    return input;
}
#define WRAPLOW(x) ((int)check_range(x))

static inline uint8_t clip_pixel_add(uint8_t dest, int trans)
{
    trans = WRAPLOW(trans);
    return clip_pixel(dest + (int)trans);
}

// Constants:
//  for (int i = 1; i< 32; ++i)
//    printf("static const int cospi_%d_64 = %.0f;\n", i,
//           round(16384 * cos(i*M_PI/64)));
// Note: sin(k*Pi/64) = cos((32-k)*Pi/64)
static const int cospi_1_64  = 16364;
static const int cospi_2_64  = 16305;
static const int cospi_3_64  = 16207;
static const int cospi_4_64  = 16069;
static const int cospi_5_64  = 15893;
static const int cospi_6_64  = 15679;
static const int cospi_7_64  = 15426;
static const int cospi_8_64  = 15137;
static const int cospi_9_64  = 14811;
static const int cospi_10_64 = 14449;
static const int cospi_11_64 = 14053;
static const int cospi_12_64 = 13623;
static const int cospi_13_64 = 13160;
static const int cospi_14_64 = 12665;
static const int cospi_15_64 = 12140;
static const int cospi_16_64 = 11585;
static const int cospi_17_64 = 11003;
static const int cospi_18_64 = 10394;
static const int cospi_19_64 = 9760;
static const int cospi_20_64 = 9102;
static const int cospi_21_64 = 8423;
static const int cospi_22_64 = 7723;
static const int cospi_23_64 = 7005;
static const int cospi_24_64 = 6270;
static const int cospi_25_64 = 5520;
static const int cospi_26_64 = 4756;
static const int cospi_27_64 = 3981;
static const int cospi_28_64 = 3196;
static const int cospi_29_64 = 2404;
static const int cospi_30_64 = 1606;
static const int cospi_31_64 = 804;

//  16384 * sqrt(2) * sin(kPi/9) * 2 / 3
static const int sinpi_1_9 = 5283;
static const int sinpi_2_9 = 9929;
static const int sinpi_3_9 = 13377;
static const int sinpi_4_9 = 15212;

typedef void (*transform_1d)(const int16_t*, int16_t*);

struct transform_2d {
    transform_1d cols, rows;  // vertical and horizontal
};

static void idct4_c(const int16_t *input, int16_t *output)
{
    int16_t step[4];
    int temp1, temp2;
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

static void iadst4_c(const int16_t *input, int16_t *output)
{
    int s0, s1, s2, s3, s4, s5, s6, s7;

    int16_t x0 = input[0];
    int16_t x1 = input[1];
    int16_t x2 = input[2];
    int16_t x3 = input[3];

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

static void iht4x4_16_add_px(const int16_t *input, uint8_t *dest, int stride, int tx_type)
{
    const transform_2d IHT_4[] = {
        { idct4_c, idct4_c  },  // DCT_DCT  = 0
        { iadst4_c, idct4_c  },   // ADST_DCT = 1
        { idct4_c, iadst4_c },    // DCT_ADST = 2
        { iadst4_c, iadst4_c }      // ADST_ADST = 3
    };

    int i, j;
    int16_t out[4 * 4];
    int16_t *outptr = out;
    int16_t temp_in[4], temp_out[4];

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

static void iht4x4_16_px(const int16_t *input, int16_t *dest, int stride, int tx_type)
{
    const transform_2d IHT_4[] = {
        { idct4_c, idct4_c  },  // DCT_DCT  = 0
        { iadst4_c, idct4_c  },   // ADST_DCT = 1
        { idct4_c, iadst4_c },    // DCT_ADST = 2
        { iadst4_c, iadst4_c }      // ADST_ADST = 3
    };

    int i, j;
    int16_t out[4 * 4];
    int16_t *outptr = out;
    int16_t temp_in[4], temp_out[4];

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

static void idct8_c(const int16_t *input, int16_t *output)
{
    int16_t step1[8], step2[8];
    int temp1, temp2;
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

static void iadst8_c(const int16_t *input, int16_t *output)
{
    int s0, s1, s2, s3, s4, s5, s6, s7;

    int x0 = input[7];
    int x1 = input[0];
    int x2 = input[5];
    int x3 = input[2];
    int x4 = input[3];
    int x5 = input[4];
    int x6 = input[1];
    int x7 = input[6];

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

static void iht8x8_64_add_px(const int16_t *input, uint8_t *dest, int stride, int tx_type, int eob)
{
    eob;
    int i, j;
    int16_t out[8 * 8];
    int16_t *outptr = out;
    int16_t temp_in[8], temp_out[8];
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

static void iht8x8_64_px(const int16_t *input, int16_t *dest, int stride, int tx_type, int eob)
{
    eob;
    int i, j;
    int16_t out[8 * 8];
    int16_t *outptr = out;
    int16_t temp_in[8], temp_out[8];
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
static void iadst16_c(const int16_t *input, int16_t *output)
{
    int s0, s1, s2, s3, s4, s5, s6, s7, s8;
    int s9, s10, s11, s12, s13, s14, s15;

    int x0 = input[15];
    int x1 = input[0];
    int x2 = input[13];
    int x3 = input[2];
    int x4 = input[11];
    int x5 = input[4];
    int x6 = input[9];
    int x7 = input[6];
    int x8 = input[7];
    int x9 = input[8];
    int x10 = input[5];
    int x11 = input[10];
    int x12 = input[3];
    int x13 = input[12];
    int x14 = input[1];
    int x15 = input[14];

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

static void idct16_c(const int16_t *input, int16_t *output)
{
    int16_t step1[16], step2[16];
    int temp1, temp2;

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

static void iht16x16_256_add_px(const int16_t *input, uint8_t *dest, int stride, int tx_type, int eob)
{
    eob;
    int i, j;
    int16_t out[16 * 16];
    int16_t *outptr = out;
    int16_t temp_in[16], temp_out[16];
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

static void iht16x16_256_px(const int16_t *input, int16_t *dest, int stride, int tx_type, int eob)
{
    eob;
    int i, j;
    int16_t out[16 * 16];
    int16_t *outptr = out;
    int16_t temp_in[16], temp_out[16];
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
static void idct32_c(const int16_t *input, int16_t *output)
{
    int16_t step1[32], step2[32];
    int temp1, temp2;

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
static void vpx_idct32x32_1024_add_c(const int16_t *input, uint8_t *dest, int stride)
{
    int16_t out[32 * 32];
    int16_t *outptr = out;
    int i, j;
    int16_t temp_in[32], temp_out[32];

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
            memset(outptr, 0, sizeof(int16_t) * 32);
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

static void vpx_idct32x32_1024_c(const int16_t *input, int16_t *dest, int stride)
{
    int16_t out[32 * 32];
    int16_t *outptr = out;
    int i, j;
    int16_t temp_in[32], temp_out[32];

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
            memset(outptr, 0, sizeof(int16_t) * 32);
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

static void idct32x32_add_px(const int16_t *input, uint8_t *dest, int stride, int eob)
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

static void idct32x32_px(const int16_t *input, int16_t *dest, int stride, int eob)
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

namespace AV1PP {
    template <int size, int type> void itransform_vp9_px(const int16_t *src, int16_t *dst, int pitchSrc) {
        if (size == 0)      iht4x4_16_px(src, dst, pitchSrc, type);
        else if (size == 1) iht8x8_64_px(src, dst, pitchSrc, type, 64);
        else if (size == 2) iht16x16_256_px(src, dst, pitchSrc, type, 256);
        else if (size == 3) idct32x32_px(src, dst, pitchSrc, 1024);
        else {assert(0);}
    }
    template void itransform_vp9_px<0, 0>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<0, 1>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<0, 2>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<0, 3>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<1, 0>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<1, 1>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<1, 2>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<1, 3>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<2, 0>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<2, 1>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<2, 2>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<2, 3>(const int16_t*,int16_t*,int);
    template void itransform_vp9_px<3, 0>(const int16_t*,int16_t*,int);

    template <int size, int type> void itransform_add_vp9_px(const int16_t *src, uint8_t *dst, int pitchSrc) {
        if (size == 0)      iht4x4_16_add_px(src, dst, pitchSrc, type);
        else if (size == 1) iht8x8_64_add_px(src, dst, pitchSrc, type, 64);
        else if (size == 2) iht16x16_256_add_px(src, dst, pitchSrc, type, 256);
        else if (size == 3) idct32x32_add_px(src, dst, pitchSrc, 1024);
        else {assert(0);}
    }
    template void itransform_add_vp9_px<0, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<0, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<0, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<0, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<1, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<1, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<1, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<1, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<2, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<2, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<2, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<2, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_vp9_px<3, 0>(const int16_t*,uint8_t*,int);
}; // namespace AV1PP
