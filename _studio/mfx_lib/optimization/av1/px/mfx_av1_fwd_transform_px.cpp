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
#include "mfx_av1_fwd_transform_common.h"

using namespace AV1PP;

static void av1_fdct4_new(const int *input, int *output, char cos_bit, const char *stage_range)
{
    const int size = 4;
    const int *cospi;

    int stage = 0;
    int *bf0, *bf1;
    int step[4];

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

static void av1_fdct8_new(const int *input, int *output, char cos_bit, const char *stage_range)
{
    const int size = 8;
    const int *cospi;

    int stage = 0;
    int *bf0, *bf1;
    int step[8];

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

static void av1_fdct16_new(const int *input, int *output, char cos_bit, const char *stage_range)
{
    const int size = 16;
    const int *cospi;

    int stage = 0;
    int *bf0, *bf1;
    int step[16];

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

static void av1_fdct32_new(const int *input, int *output, char cos_bit, const char *stage_range)
{
    const int size = 32;
    const int *cospi;

    int stage = 0;
    int *bf0, *bf1;
    int step[32];

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

static void av1_fdct64_new(const int *input, int *output, char cos_bit, const char *stage_range)
{
    const int size = 64;
    const int *cospi;

    int stage = 0;
    int *bf0, *bf1;
    int step[64];

    // stage 0;
    range_check(stage, input, input, size, stage_range[stage]);

    // stage 1;
    stage++;
    bf1 = output;
    bf1[0] = input[0] + input[63];
    bf1[1] = input[1] + input[62];
    bf1[2] = input[2] + input[61];
    bf1[3] = input[3] + input[60];
    bf1[4] = input[4] + input[59];
    bf1[5] = input[5] + input[58];
    bf1[6] = input[6] + input[57];
    bf1[7] = input[7] + input[56];
    bf1[8] = input[8] + input[55];
    bf1[9] = input[9] + input[54];
    bf1[10] = input[10] + input[53];
    bf1[11] = input[11] + input[52];
    bf1[12] = input[12] + input[51];
    bf1[13] = input[13] + input[50];
    bf1[14] = input[14] + input[49];
    bf1[15] = input[15] + input[48];
    bf1[16] = input[16] + input[47];
    bf1[17] = input[17] + input[46];
    bf1[18] = input[18] + input[45];
    bf1[19] = input[19] + input[44];
    bf1[20] = input[20] + input[43];
    bf1[21] = input[21] + input[42];
    bf1[22] = input[22] + input[41];
    bf1[23] = input[23] + input[40];
    bf1[24] = input[24] + input[39];
    bf1[25] = input[25] + input[38];
    bf1[26] = input[26] + input[37];
    bf1[27] = input[27] + input[36];
    bf1[28] = input[28] + input[35];
    bf1[29] = input[29] + input[34];
    bf1[30] = input[30] + input[33];
    bf1[31] = input[31] + input[32];
    bf1[32] = -input[32] + input[31];
    bf1[33] = -input[33] + input[30];
    bf1[34] = -input[34] + input[29];
    bf1[35] = -input[35] + input[28];
    bf1[36] = -input[36] + input[27];
    bf1[37] = -input[37] + input[26];
    bf1[38] = -input[38] + input[25];
    bf1[39] = -input[39] + input[24];
    bf1[40] = -input[40] + input[23];
    bf1[41] = -input[41] + input[22];
    bf1[42] = -input[42] + input[21];
    bf1[43] = -input[43] + input[20];
    bf1[44] = -input[44] + input[19];
    bf1[45] = -input[45] + input[18];
    bf1[46] = -input[46] + input[17];
    bf1[47] = -input[47] + input[16];
    bf1[48] = -input[48] + input[15];
    bf1[49] = -input[49] + input[14];
    bf1[50] = -input[50] + input[13];
    bf1[51] = -input[51] + input[12];
    bf1[52] = -input[52] + input[11];
    bf1[53] = -input[53] + input[10];
    bf1[54] = -input[54] + input[9];
    bf1[55] = -input[55] + input[8];
    bf1[56] = -input[56] + input[7];
    bf1[57] = -input[57] + input[6];
    bf1[58] = -input[58] + input[5];
    bf1[59] = -input[59] + input[4];
    bf1[60] = -input[60] + input[3];
    bf1[61] = -input[61] + input[2];
    bf1[62] = -input[62] + input[1];
    bf1[63] = -input[63] + input[0];
    range_check(stage, input, bf1, size, stage_range[stage]);

    // stage 2
    stage++;
    cospi = cospi_arr(cos_bit);
    bf0 = output;
    bf1 = step;
    bf1[0] = bf0[0] + bf0[31];
    bf1[1] = bf0[1] + bf0[30];
    bf1[2] = bf0[2] + bf0[29];
    bf1[3] = bf0[3] + bf0[28];
    bf1[4] = bf0[4] + bf0[27];
    bf1[5] = bf0[5] + bf0[26];
    bf1[6] = bf0[6] + bf0[25];
    bf1[7] = bf0[7] + bf0[24];
    bf1[8] = bf0[8] + bf0[23];
    bf1[9] = bf0[9] + bf0[22];
    bf1[10] = bf0[10] + bf0[21];
    bf1[11] = bf0[11] + bf0[20];
    bf1[12] = bf0[12] + bf0[19];
    bf1[13] = bf0[13] + bf0[18];
    bf1[14] = bf0[14] + bf0[17];
    bf1[15] = bf0[15] + bf0[16];
    bf1[16] = -bf0[16] + bf0[15];
    bf1[17] = -bf0[17] + bf0[14];
    bf1[18] = -bf0[18] + bf0[13];
    bf1[19] = -bf0[19] + bf0[12];
    bf1[20] = -bf0[20] + bf0[11];
    bf1[21] = -bf0[21] + bf0[10];
    bf1[22] = -bf0[22] + bf0[9];
    bf1[23] = -bf0[23] + bf0[8];
    bf1[24] = -bf0[24] + bf0[7];
    bf1[25] = -bf0[25] + bf0[6];
    bf1[26] = -bf0[26] + bf0[5];
    bf1[27] = -bf0[27] + bf0[4];
    bf1[28] = -bf0[28] + bf0[3];
    bf1[29] = -bf0[29] + bf0[2];
    bf1[30] = -bf0[30] + bf0[1];
    bf1[31] = -bf0[31] + bf0[0];
    bf1[32] = bf0[32];
    bf1[33] = bf0[33];
    bf1[34] = bf0[34];
    bf1[35] = bf0[35];
    bf1[36] = bf0[36];
    bf1[37] = bf0[37];
    bf1[38] = bf0[38];
    bf1[39] = bf0[39];
    bf1[40] = half_btf(-cospi[32], bf0[40], cospi[32], bf0[55], cos_bit);
    bf1[41] = half_btf(-cospi[32], bf0[41], cospi[32], bf0[54], cos_bit);
    bf1[42] = half_btf(-cospi[32], bf0[42], cospi[32], bf0[53], cos_bit);
    bf1[43] = half_btf(-cospi[32], bf0[43], cospi[32], bf0[52], cos_bit);
    bf1[44] = half_btf(-cospi[32], bf0[44], cospi[32], bf0[51], cos_bit);
    bf1[45] = half_btf(-cospi[32], bf0[45], cospi[32], bf0[50], cos_bit);
    bf1[46] = half_btf(-cospi[32], bf0[46], cospi[32], bf0[49], cos_bit);
    bf1[47] = half_btf(-cospi[32], bf0[47], cospi[32], bf0[48], cos_bit);
    bf1[48] = half_btf(cospi[32], bf0[48], cospi[32], bf0[47], cos_bit);
    bf1[49] = half_btf(cospi[32], bf0[49], cospi[32], bf0[46], cos_bit);
    bf1[50] = half_btf(cospi[32], bf0[50], cospi[32], bf0[45], cos_bit);
    bf1[51] = half_btf(cospi[32], bf0[51], cospi[32], bf0[44], cos_bit);
    bf1[52] = half_btf(cospi[32], bf0[52], cospi[32], bf0[43], cos_bit);
    bf1[53] = half_btf(cospi[32], bf0[53], cospi[32], bf0[42], cos_bit);
    bf1[54] = half_btf(cospi[32], bf0[54], cospi[32], bf0[41], cos_bit);
    bf1[55] = half_btf(cospi[32], bf0[55], cospi[32], bf0[40], cos_bit);
    bf1[56] = bf0[56];
    bf1[57] = bf0[57];
    bf1[58] = bf0[58];
    bf1[59] = bf0[59];
    bf1[60] = bf0[60];
    bf1[61] = bf0[61];
    bf1[62] = bf0[62];
    bf1[63] = bf0[63];
    range_check(stage, input, bf1, size, stage_range[stage]);

    // stage 3
    stage++;
    cospi = cospi_arr(cos_bit);
    bf0 = step;
    bf1 = output;
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
    bf1[32] = bf0[32] + bf0[47];
    bf1[33] = bf0[33] + bf0[46];
    bf1[34] = bf0[34] + bf0[45];
    bf1[35] = bf0[35] + bf0[44];
    bf1[36] = bf0[36] + bf0[43];
    bf1[37] = bf0[37] + bf0[42];
    bf1[38] = bf0[38] + bf0[41];
    bf1[39] = bf0[39] + bf0[40];
    bf1[40] = -bf0[40] + bf0[39];
    bf1[41] = -bf0[41] + bf0[38];
    bf1[42] = -bf0[42] + bf0[37];
    bf1[43] = -bf0[43] + bf0[36];
    bf1[44] = -bf0[44] + bf0[35];
    bf1[45] = -bf0[45] + bf0[34];
    bf1[46] = -bf0[46] + bf0[33];
    bf1[47] = -bf0[47] + bf0[32];
    bf1[48] = -bf0[48] + bf0[63];
    bf1[49] = -bf0[49] + bf0[62];
    bf1[50] = -bf0[50] + bf0[61];
    bf1[51] = -bf0[51] + bf0[60];
    bf1[52] = -bf0[52] + bf0[59];
    bf1[53] = -bf0[53] + bf0[58];
    bf1[54] = -bf0[54] + bf0[57];
    bf1[55] = -bf0[55] + bf0[56];
    bf1[56] = bf0[56] + bf0[55];
    bf1[57] = bf0[57] + bf0[54];
    bf1[58] = bf0[58] + bf0[53];
    bf1[59] = bf0[59] + bf0[52];
    bf1[60] = bf0[60] + bf0[51];
    bf1[61] = bf0[61] + bf0[50];
    bf1[62] = bf0[62] + bf0[49];
    bf1[63] = bf0[63] + bf0[48];
    range_check(stage, input, bf1, size, stage_range[stage]);

    // stage 4
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
    bf1[32] = bf0[32];
    bf1[33] = bf0[33];
    bf1[34] = bf0[34];
    bf1[35] = bf0[35];
    bf1[36] = half_btf(-cospi[16], bf0[36], cospi[48], bf0[59], cos_bit);
    bf1[37] = half_btf(-cospi[16], bf0[37], cospi[48], bf0[58], cos_bit);
    bf1[38] = half_btf(-cospi[16], bf0[38], cospi[48], bf0[57], cos_bit);
    bf1[39] = half_btf(-cospi[16], bf0[39], cospi[48], bf0[56], cos_bit);
    bf1[40] = half_btf(-cospi[48], bf0[40], -cospi[16], bf0[55], cos_bit);
    bf1[41] = half_btf(-cospi[48], bf0[41], -cospi[16], bf0[54], cos_bit);
    bf1[42] = half_btf(-cospi[48], bf0[42], -cospi[16], bf0[53], cos_bit);
    bf1[43] = half_btf(-cospi[48], bf0[43], -cospi[16], bf0[52], cos_bit);
    bf1[44] = bf0[44];
    bf1[45] = bf0[45];
    bf1[46] = bf0[46];
    bf1[47] = bf0[47];
    bf1[48] = bf0[48];
    bf1[49] = bf0[49];
    bf1[50] = bf0[50];
    bf1[51] = bf0[51];
    bf1[52] = half_btf(cospi[48], bf0[52], -cospi[16], bf0[43], cos_bit);
    bf1[53] = half_btf(cospi[48], bf0[53], -cospi[16], bf0[42], cos_bit);
    bf1[54] = half_btf(cospi[48], bf0[54], -cospi[16], bf0[41], cos_bit);
    bf1[55] = half_btf(cospi[48], bf0[55], -cospi[16], bf0[40], cos_bit);
    bf1[56] = half_btf(cospi[16], bf0[56], cospi[48], bf0[39], cos_bit);
    bf1[57] = half_btf(cospi[16], bf0[57], cospi[48], bf0[38], cos_bit);
    bf1[58] = half_btf(cospi[16], bf0[58], cospi[48], bf0[37], cos_bit);
    bf1[59] = half_btf(cospi[16], bf0[59], cospi[48], bf0[36], cos_bit);
    bf1[60] = bf0[60];
    bf1[61] = bf0[61];
    bf1[62] = bf0[62];
    bf1[63] = bf0[63];
    range_check(stage, input, bf1, size, stage_range[stage]);

    // stage 5
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
    bf1[32] = bf0[32] + bf0[39];
    bf1[33] = bf0[33] + bf0[38];
    bf1[34] = bf0[34] + bf0[37];
    bf1[35] = bf0[35] + bf0[36];
    bf1[36] = -bf0[36] + bf0[35];
    bf1[37] = -bf0[37] + bf0[34];
    bf1[38] = -bf0[38] + bf0[33];
    bf1[39] = -bf0[39] + bf0[32];
    bf1[40] = -bf0[40] + bf0[47];
    bf1[41] = -bf0[41] + bf0[46];
    bf1[42] = -bf0[42] + bf0[45];
    bf1[43] = -bf0[43] + bf0[44];
    bf1[44] = bf0[44] + bf0[43];
    bf1[45] = bf0[45] + bf0[42];
    bf1[46] = bf0[46] + bf0[41];
    bf1[47] = bf0[47] + bf0[40];
    bf1[48] = bf0[48] + bf0[55];
    bf1[49] = bf0[49] + bf0[54];
    bf1[50] = bf0[50] + bf0[53];
    bf1[51] = bf0[51] + bf0[52];
    bf1[52] = -bf0[52] + bf0[51];
    bf1[53] = -bf0[53] + bf0[50];
    bf1[54] = -bf0[54] + bf0[49];
    bf1[55] = -bf0[55] + bf0[48];
    bf1[56] = -bf0[56] + bf0[63];
    bf1[57] = -bf0[57] + bf0[62];
    bf1[58] = -bf0[58] + bf0[61];
    bf1[59] = -bf0[59] + bf0[60];
    bf1[60] = bf0[60] + bf0[59];
    bf1[61] = bf0[61] + bf0[58];
    bf1[62] = bf0[62] + bf0[57];
    bf1[63] = bf0[63] + bf0[56];
    range_check(stage, input, bf1, size, stage_range[stage]);

    // stage 6
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
    bf1[32] = bf0[32];
    bf1[33] = bf0[33];
    bf1[34] = half_btf(-cospi[8], bf0[34], cospi[56], bf0[61], cos_bit);
    bf1[35] = half_btf(-cospi[8], bf0[35], cospi[56], bf0[60], cos_bit);
    bf1[36] = half_btf(-cospi[56], bf0[36], -cospi[8], bf0[59], cos_bit);
    bf1[37] = half_btf(-cospi[56], bf0[37], -cospi[8], bf0[58], cos_bit);
    bf1[38] = bf0[38];
    bf1[39] = bf0[39];
    bf1[40] = bf0[40];
    bf1[41] = bf0[41];
    bf1[42] = half_btf(-cospi[40], bf0[42], cospi[24], bf0[53], cos_bit);
    bf1[43] = half_btf(-cospi[40], bf0[43], cospi[24], bf0[52], cos_bit);
    bf1[44] = half_btf(-cospi[24], bf0[44], -cospi[40], bf0[51], cos_bit);
    bf1[45] = half_btf(-cospi[24], bf0[45], -cospi[40], bf0[50], cos_bit);
    bf1[46] = bf0[46];
    bf1[47] = bf0[47];
    bf1[48] = bf0[48];
    bf1[49] = bf0[49];
    bf1[50] = half_btf(cospi[24], bf0[50], -cospi[40], bf0[45], cos_bit);
    bf1[51] = half_btf(cospi[24], bf0[51], -cospi[40], bf0[44], cos_bit);
    bf1[52] = half_btf(cospi[40], bf0[52], cospi[24], bf0[43], cos_bit);
    bf1[53] = half_btf(cospi[40], bf0[53], cospi[24], bf0[42], cos_bit);
    bf1[54] = bf0[54];
    bf1[55] = bf0[55];
    bf1[56] = bf0[56];
    bf1[57] = bf0[57];
    bf1[58] = half_btf(cospi[56], bf0[58], -cospi[8], bf0[37], cos_bit);
    bf1[59] = half_btf(cospi[56], bf0[59], -cospi[8], bf0[36], cos_bit);
    bf1[60] = half_btf(cospi[8], bf0[60], cospi[56], bf0[35], cos_bit);
    bf1[61] = half_btf(cospi[8], bf0[61], cospi[56], bf0[34], cos_bit);
    bf1[62] = bf0[62];
    bf1[63] = bf0[63];
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
    bf1[32] = bf0[32] + bf0[35];
    bf1[33] = bf0[33] + bf0[34];
    bf1[34] = -bf0[34] + bf0[33];
    bf1[35] = -bf0[35] + bf0[32];
    bf1[36] = -bf0[36] + bf0[39];
    bf1[37] = -bf0[37] + bf0[38];
    bf1[38] = bf0[38] + bf0[37];
    bf1[39] = bf0[39] + bf0[36];
    bf1[40] = bf0[40] + bf0[43];
    bf1[41] = bf0[41] + bf0[42];
    bf1[42] = -bf0[42] + bf0[41];
    bf1[43] = -bf0[43] + bf0[40];
    bf1[44] = -bf0[44] + bf0[47];
    bf1[45] = -bf0[45] + bf0[46];
    bf1[46] = bf0[46] + bf0[45];
    bf1[47] = bf0[47] + bf0[44];
    bf1[48] = bf0[48] + bf0[51];
    bf1[49] = bf0[49] + bf0[50];
    bf1[50] = -bf0[50] + bf0[49];
    bf1[51] = -bf0[51] + bf0[48];
    bf1[52] = -bf0[52] + bf0[55];
    bf1[53] = -bf0[53] + bf0[54];
    bf1[54] = bf0[54] + bf0[53];
    bf1[55] = bf0[55] + bf0[52];
    bf1[56] = bf0[56] + bf0[59];
    bf1[57] = bf0[57] + bf0[58];
    bf1[58] = -bf0[58] + bf0[57];
    bf1[59] = -bf0[59] + bf0[56];
    bf1[60] = -bf0[60] + bf0[63];
    bf1[61] = -bf0[61] + bf0[62];
    bf1[62] = bf0[62] + bf0[61];
    bf1[63] = bf0[63] + bf0[60];
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
    bf1[32] = bf0[32];
    bf1[33] = half_btf(-cospi[4], bf0[33], cospi[60], bf0[62], cos_bit);
    bf1[34] = half_btf(-cospi[60], bf0[34], -cospi[4], bf0[61], cos_bit);
    bf1[35] = bf0[35];
    bf1[36] = bf0[36];
    bf1[37] = half_btf(-cospi[36], bf0[37], cospi[28], bf0[58], cos_bit);
    bf1[38] = half_btf(-cospi[28], bf0[38], -cospi[36], bf0[57], cos_bit);
    bf1[39] = bf0[39];
    bf1[40] = bf0[40];
    bf1[41] = half_btf(-cospi[20], bf0[41], cospi[44], bf0[54], cos_bit);
    bf1[42] = half_btf(-cospi[44], bf0[42], -cospi[20], bf0[53], cos_bit);
    bf1[43] = bf0[43];
    bf1[44] = bf0[44];
    bf1[45] = half_btf(-cospi[52], bf0[45], cospi[12], bf0[50], cos_bit);
    bf1[46] = half_btf(-cospi[12], bf0[46], -cospi[52], bf0[49], cos_bit);
    bf1[47] = bf0[47];
    bf1[48] = bf0[48];
    bf1[49] = half_btf(cospi[12], bf0[49], -cospi[52], bf0[46], cos_bit);
    bf1[50] = half_btf(cospi[52], bf0[50], cospi[12], bf0[45], cos_bit);
    bf1[51] = bf0[51];
    bf1[52] = bf0[52];
    bf1[53] = half_btf(cospi[44], bf0[53], -cospi[20], bf0[42], cos_bit);
    bf1[54] = half_btf(cospi[20], bf0[54], cospi[44], bf0[41], cos_bit);
    bf1[55] = bf0[55];
    bf1[56] = bf0[56];
    bf1[57] = half_btf(cospi[28], bf0[57], -cospi[36], bf0[38], cos_bit);
    bf1[58] = half_btf(cospi[36], bf0[58], cospi[28], bf0[37], cos_bit);
    bf1[59] = bf0[59];
    bf1[60] = bf0[60];
    bf1[61] = half_btf(cospi[60], bf0[61], -cospi[4], bf0[34], cos_bit);
    bf1[62] = half_btf(cospi[4], bf0[62], cospi[60], bf0[33], cos_bit);
    bf1[63] = bf0[63];
    range_check(stage, input, bf1, size, stage_range[stage]);

    // stage 9
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
    bf1[32] = bf0[32] + bf0[33];
    bf1[33] = -bf0[33] + bf0[32];
    bf1[34] = -bf0[34] + bf0[35];
    bf1[35] = bf0[35] + bf0[34];
    bf1[36] = bf0[36] + bf0[37];
    bf1[37] = -bf0[37] + bf0[36];
    bf1[38] = -bf0[38] + bf0[39];
    bf1[39] = bf0[39] + bf0[38];
    bf1[40] = bf0[40] + bf0[41];
    bf1[41] = -bf0[41] + bf0[40];
    bf1[42] = -bf0[42] + bf0[43];
    bf1[43] = bf0[43] + bf0[42];
    bf1[44] = bf0[44] + bf0[45];
    bf1[45] = -bf0[45] + bf0[44];
    bf1[46] = -bf0[46] + bf0[47];
    bf1[47] = bf0[47] + bf0[46];
    bf1[48] = bf0[48] + bf0[49];
    bf1[49] = -bf0[49] + bf0[48];
    bf1[50] = -bf0[50] + bf0[51];
    bf1[51] = bf0[51] + bf0[50];
    bf1[52] = bf0[52] + bf0[53];
    bf1[53] = -bf0[53] + bf0[52];
    bf1[54] = -bf0[54] + bf0[55];
    bf1[55] = bf0[55] + bf0[54];
    bf1[56] = bf0[56] + bf0[57];
    bf1[57] = -bf0[57] + bf0[56];
    bf1[58] = -bf0[58] + bf0[59];
    bf1[59] = bf0[59] + bf0[58];
    bf1[60] = bf0[60] + bf0[61];
    bf1[61] = -bf0[61] + bf0[60];
    bf1[62] = -bf0[62] + bf0[63];
    bf1[63] = bf0[63] + bf0[62];
    range_check(stage, input, bf1, size, stage_range[stage]);

    // stage 10
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
    bf1[16] = bf0[16];
    bf1[17] = bf0[17];
    bf1[18] = bf0[18];
    bf1[19] = bf0[19];
    bf1[20] = bf0[20];
    bf1[21] = bf0[21];
    bf1[22] = bf0[22];
    bf1[23] = bf0[23];
    bf1[24] = bf0[24];
    bf1[25] = bf0[25];
    bf1[26] = bf0[26];
    bf1[27] = bf0[27];
    bf1[28] = bf0[28];
    bf1[29] = bf0[29];
    bf1[30] = bf0[30];
    bf1[31] = bf0[31];
    bf1[32] = half_btf(cospi[63], bf0[32], cospi[1], bf0[63], cos_bit);
    bf1[33] = half_btf(cospi[31], bf0[33], cospi[33], bf0[62], cos_bit);
    bf1[34] = half_btf(cospi[47], bf0[34], cospi[17], bf0[61], cos_bit);
    bf1[35] = half_btf(cospi[15], bf0[35], cospi[49], bf0[60], cos_bit);
    bf1[36] = half_btf(cospi[55], bf0[36], cospi[9], bf0[59], cos_bit);
    bf1[37] = half_btf(cospi[23], bf0[37], cospi[41], bf0[58], cos_bit);
    bf1[38] = half_btf(cospi[39], bf0[38], cospi[25], bf0[57], cos_bit);
    bf1[39] = half_btf(cospi[7], bf0[39], cospi[57], bf0[56], cos_bit);
    bf1[40] = half_btf(cospi[59], bf0[40], cospi[5], bf0[55], cos_bit);
    bf1[41] = half_btf(cospi[27], bf0[41], cospi[37], bf0[54], cos_bit);
    bf1[42] = half_btf(cospi[43], bf0[42], cospi[21], bf0[53], cos_bit);
    bf1[43] = half_btf(cospi[11], bf0[43], cospi[53], bf0[52], cos_bit);
    bf1[44] = half_btf(cospi[51], bf0[44], cospi[13], bf0[51], cos_bit);
    bf1[45] = half_btf(cospi[19], bf0[45], cospi[45], bf0[50], cos_bit);
    bf1[46] = half_btf(cospi[35], bf0[46], cospi[29], bf0[49], cos_bit);
    bf1[47] = half_btf(cospi[3], bf0[47], cospi[61], bf0[48], cos_bit);
    bf1[48] = half_btf(cospi[3], bf0[48], -cospi[61], bf0[47], cos_bit);
    bf1[49] = half_btf(cospi[35], bf0[49], -cospi[29], bf0[46], cos_bit);
    bf1[50] = half_btf(cospi[19], bf0[50], -cospi[45], bf0[45], cos_bit);
    bf1[51] = half_btf(cospi[51], bf0[51], -cospi[13], bf0[44], cos_bit);
    bf1[52] = half_btf(cospi[11], bf0[52], -cospi[53], bf0[43], cos_bit);
    bf1[53] = half_btf(cospi[43], bf0[53], -cospi[21], bf0[42], cos_bit);
    bf1[54] = half_btf(cospi[27], bf0[54], -cospi[37], bf0[41], cos_bit);
    bf1[55] = half_btf(cospi[59], bf0[55], -cospi[5], bf0[40], cos_bit);
    bf1[56] = half_btf(cospi[7], bf0[56], -cospi[57], bf0[39], cos_bit);
    bf1[57] = half_btf(cospi[39], bf0[57], -cospi[25], bf0[38], cos_bit);
    bf1[58] = half_btf(cospi[23], bf0[58], -cospi[41], bf0[37], cos_bit);
    bf1[59] = half_btf(cospi[55], bf0[59], -cospi[9], bf0[36], cos_bit);
    bf1[60] = half_btf(cospi[15], bf0[60], -cospi[49], bf0[35], cos_bit);
    bf1[61] = half_btf(cospi[47], bf0[61], -cospi[17], bf0[34], cos_bit);
    bf1[62] = half_btf(cospi[31], bf0[62], -cospi[33], bf0[33], cos_bit);
    bf1[63] = half_btf(cospi[63], bf0[63], -cospi[1], bf0[32], cos_bit);
    range_check(stage, input, bf1, size, stage_range[stage]);

    // stage 11
    stage++;
    bf0 = step;
    bf1 = output;
    bf1[0] = bf0[0];
    bf1[1] = bf0[32];
    bf1[2] = bf0[16];
    bf1[3] = bf0[48];
    bf1[4] = bf0[8];
    bf1[5] = bf0[40];
    bf1[6] = bf0[24];
    bf1[7] = bf0[56];
    bf1[8] = bf0[4];
    bf1[9] = bf0[36];
    bf1[10] = bf0[20];
    bf1[11] = bf0[52];
    bf1[12] = bf0[12];
    bf1[13] = bf0[44];
    bf1[14] = bf0[28];
    bf1[15] = bf0[60];
    bf1[16] = bf0[2];
    bf1[17] = bf0[34];
    bf1[18] = bf0[18];
    bf1[19] = bf0[50];
    bf1[20] = bf0[10];
    bf1[21] = bf0[42];
    bf1[22] = bf0[26];
    bf1[23] = bf0[58];
    bf1[24] = bf0[6];
    bf1[25] = bf0[38];
    bf1[26] = bf0[22];
    bf1[27] = bf0[54];
    bf1[28] = bf0[14];
    bf1[29] = bf0[46];
    bf1[30] = bf0[30];
    bf1[31] = bf0[62];
    bf1[32] = bf0[1];
    bf1[33] = bf0[33];
    bf1[34] = bf0[17];
    bf1[35] = bf0[49];
    bf1[36] = bf0[9];
    bf1[37] = bf0[41];
    bf1[38] = bf0[25];
    bf1[39] = bf0[57];
    bf1[40] = bf0[5];
    bf1[41] = bf0[37];
    bf1[42] = bf0[21];
    bf1[43] = bf0[53];
    bf1[44] = bf0[13];
    bf1[45] = bf0[45];
    bf1[46] = bf0[29];
    bf1[47] = bf0[61];
    bf1[48] = bf0[3];
    bf1[49] = bf0[35];
    bf1[50] = bf0[19];
    bf1[51] = bf0[51];
    bf1[52] = bf0[11];
    bf1[53] = bf0[43];
    bf1[54] = bf0[27];
    bf1[55] = bf0[59];
    bf1[56] = bf0[7];
    bf1[57] = bf0[39];
    bf1[58] = bf0[23];
    bf1[59] = bf0[55];
    bf1[60] = bf0[15];
    bf1[61] = bf0[47];
    bf1[62] = bf0[31];
    bf1[63] = bf0[63];
    range_check(stage, input, bf1, size, stage_range[stage]);
}

static void av1_fadst4_new(const int *input, int *output, char cos_bit, const char *stage_range)
{
    int bit = cos_bit;
    const int *sinpi = sinpi_arr(bit);
    int x0, x1, x2, x3;
    int s0, s1, s2, s3, s4, s5, s6, s7;

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

static void av1_fadst8_new(const int *input, int *output, char cos_bit, const char *stage_range)
{
    const int size = 8;
    const int *cospi;

    int stage = 0;
    int *bf0, *bf1;
    int step[8];

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

static void av1_fadst16_new(const int *input, int *output, char cos_bit, const char *stage_range)
{
    const int size = 16;
    const int *cospi;

    int stage = 0;
    int *bf0, *bf1;
    int step[16];

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

static void av1_fidentity4_c(const int *input, int *output, char cos_bit, const char *stage_range)
{
    (void)cos_bit;
    for (int i = 0; i < 4; ++i)
        output[i] = round_shift(input[i] * NewSqrt2, NewSqrt2Bits);
    assert(stage_range[0] + NewSqrt2Bits <= 32);
    range_check(0, input, output, 4, stage_range[0]);
}

static void av1_fidentity8_c(const int *input, int *output, char cos_bit, const char *stage_range)
{
    (void)cos_bit;
    for (int i = 0; i < 8; ++i) output[i] = input[i] * 2;
    range_check(0, input, output, 8, stage_range[0]);
}

static void av1_fidentity16_c(const int *input, int *output, char cos_bit, const char *stage_range)
{
    (void)cos_bit;
    for (int i = 0; i < 16; ++i)
        output[i] = round_shift(input[i] * 2 * NewSqrt2, NewSqrt2Bits);
    assert(stage_range[0] + NewSqrt2Bits <= 32);
    range_check(0, input, output, 16, stage_range[0]);
}

static void av1_fidentity32_c(const int *input, int *output, char cos_bit, const char *stage_range)
{
    (void)cos_bit;
    for (int i = 0; i < 32; ++i) output[i] = input[i] * 4;
    range_check(0, input, output, 32, stage_range[0]);
}

static TxfmFunc fwd_txfm_type_to_func(TXFM_TYPE txfm_type) {
    switch (txfm_type) {
    case TXFM_TYPE_DCT4: return av1_fdct4_new;
    case TXFM_TYPE_DCT8: return av1_fdct8_new;
    case TXFM_TYPE_DCT16: return av1_fdct16_new;

    case TXFM_TYPE_DCT32: return av1_fdct32_new;
    case TXFM_TYPE_DCT64: return av1_fdct64_new;
    case TXFM_TYPE_ADST4: return av1_fadst4_new;
    case TXFM_TYPE_ADST8: return av1_fadst8_new;
    case TXFM_TYPE_ADST16: return av1_fadst16_new;
    case TXFM_TYPE_IDENTITY4: return av1_fidentity4_c;
    case TXFM_TYPE_IDENTITY8: return av1_fidentity8_c;
    case TXFM_TYPE_IDENTITY16: return av1_fidentity16_c;
    case TXFM_TYPE_IDENTITY32: return av1_fidentity32_c;
    default: assert(0); return NULL;
    }
}

static void av1_gen_fwd_stage_range(char *stage_range_col, char *stage_range_row, const TXFM_2D_FLIP_CFG *cfg, int bd)
{
    // Take the shift from the larger dimension in the rectangular case.
    const char *shift = cfg->shift;
    // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
    for (int i = 0; i < cfg->stage_num_col && i < MAX_TXFM_STAGE_NUM; ++i) {
        stage_range_col[i] = cfg->stage_range_col[i] + shift[0] + bd + 1;
    }

    // i < MAX_TXFM_STAGE_NUM will mute above array bounds warning
    for (int i = 0; i < cfg->stage_num_row && i < MAX_TXFM_STAGE_NUM; ++i) {
        stage_range_row[i] = cfg->stage_range_row[i] + shift[0] + shift[1] + bd + 1;
    }
}

static void fwd_txfm2d_c(const short *input, int *output, const int stride, const TXFM_2D_FLIP_CFG *cfg, int *buf, int bd)
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
    const char *shift = cfg->shift;
    const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
    char stage_range_col[MAX_TXFM_STAGE_NUM];
    char stage_range_row[MAX_TXFM_STAGE_NUM];
    assert(cfg->stage_num_col <= MAX_TXFM_STAGE_NUM);
    assert(cfg->stage_num_row <= MAX_TXFM_STAGE_NUM);
    av1_gen_fwd_stage_range(stage_range_col, stage_range_row, cfg, bd);

    const char cos_bit_col = cfg->cos_bit_col;
    const char cos_bit_row = cfg->cos_bit_row;
    const TxfmFunc txfm_func_col = fwd_txfm_type_to_func(cfg->txfm_type_col);
    const TxfmFunc txfm_func_row = fwd_txfm_type_to_func(cfg->txfm_type_row);

    // use output buffer as temp buffer
    int *temp_in = output;
    int *temp_out = output + txfm_size_row;

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
            for (c = 0; c < txfm_size_col; ++c) {
                output[r * txfm_size_col + c] = round_shift(
                    (long long)output[r * txfm_size_col + c] * NewSqrt2, NewSqrt2Bits);
            }
        }
    }
}

static void av1_fwd_txfm2d_4x4_c(const short *input, int *output, int stride, TxType tx_type, int bd) {
    int txfm_buf[4 * 4];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_4X4, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

static void av1_fwd_txfm2d_8x8_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    int txfm_buf[8 * 8];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_8X8, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

static void av1_fwd_txfm2d_16x16_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    int txfm_buf[16 * 16];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_16X16, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

static void av1_fwd_txfm2d_32x32_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    int txfm_buf[32 * 32];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_32X32, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_64x64_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    int txfm_buf[64 * 64];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_64X64, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);

    // Zero out top-right 32x32 area.
    for (int row = 0; row < 32; ++row) {
        memset(output + row * 64 + 32, 0, 32 * sizeof(*output));
    }
    // Zero out the bottom 64x32 area.
    memset(output + 32 * 64, 0, 32 * 64 * sizeof(*output));
    // Re-pack non-zero coeffs in the first 32x32 indices.
    for (int row = 1; row < 32; ++row) {
        memcpy(output + row * 32, output + row * 64, 32 * sizeof(*output));
    }
}

void av1_fwd_txfm2d_4x8_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    ALIGN_DECL(32) int txfm_buf[4 * 8];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_4X8, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_8x4_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    int txfm_buf[8 * 4];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_8X4, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_8x16_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    ALIGN_DECL(32) int txfm_buf[8 * 16];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_8X16, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_16x8_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    int txfm_buf[16 * 8];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_16X8, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_16x32_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    ALIGN_DECL(32) int txfm_buf[16 * 32];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_16X32, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_32x16_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    int txfm_buf[32 * 16];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_32X16, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
}

void av1_fwd_txfm2d_32x64_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    ALIGN_DECL(32) int txfm_buf[32 * 64];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_32X64, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);
    // Zero out the bottom 32x32 area.
    memset(output + 32 * 32, 0, 32 * 32 * sizeof(*output));
    // Note: no repacking needed here.
}

void av1_fwd_txfm2d_64x32_c(const short *input, int *output, int stride, TxType tx_type, int bd)
{
    int txfm_buf[64 * 32];
    TXFM_2D_FLIP_CFG cfg;
    av1_get_fwd_txfm_cfg(tx_type, TX_64X32, &cfg);
    fwd_txfm2d_c(input, output, stride, &cfg, txfm_buf, bd);

    // Zero out right 32x32 area.
    for (int row = 0; row < 32; ++row) {
        memset(output + row * 64 + 32, 0, 32 * sizeof(*output));
    }
    // Re-pack non-zero coeffs in the first 32x32 indices.
    for (int row = 1; row < 32; ++row) {
        memcpy(output + row * 32, output + row * 64, 32 * sizeof(*output));
    }
}

namespace AV1PP {
    template <int size, int type, typename TCoeffType> void ftransform_av1_px(const short *src, TCoeffType *dst, int pitchSrc) {
        int dst32[64*64];
        int bd = 8;
        if (sizeof(TCoeffType) > 2) bd = 10;
        if      (size == TX_4X4)   av1_fwd_txfm2d_4x4_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_8X8)   av1_fwd_txfm2d_8x8_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_16X16) av1_fwd_txfm2d_16x16_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_32X32) av1_fwd_txfm2d_32x32_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_64X64) av1_fwd_txfm2d_64x64_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_4X8)   av1_fwd_txfm2d_4x8_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_8X4)   av1_fwd_txfm2d_8x4_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_8X16)  av1_fwd_txfm2d_8x16_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_16X8)  av1_fwd_txfm2d_16x8_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_16X32) av1_fwd_txfm2d_16x32_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_32X16) av1_fwd_txfm2d_32x16_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_32X64) av1_fwd_txfm2d_32x64_c(src, dst32, pitchSrc, type, bd);
        else if (size == TX_64X32) av1_fwd_txfm2d_64x32_c(src, dst32, pitchSrc, type, bd);
        else {assert(0);}

        const int txh = tx_size_high[size];
        const int txw = tx_size_wide[size];
        for (int i = 0; i < txh; i++)
            for (int j = 0; j < txw; j++)
                dst[i * txw + j] = dst32[i * txw + j];
    }

    template void ftransform_av1_px<TX_4X4,      DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_4X4,     ADST_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_4X4,     DCT_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_4X4,    ADST_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_4X4,         IDTX, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X8,      DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X8,     ADST_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X8,     DCT_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X8,    ADST_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X8,         IDTX, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_16X16,    DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_16X16,   ADST_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_16X16,   DCT_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_16X16,  ADST_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_16X16,       IDTX, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_32X32,    DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_32X32,       IDTX, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_64X64,    DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_4X8,      DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_4X8,     ADST_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_4X8,     DCT_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_4X8,    ADST_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X4,      DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X4,     ADST_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X4,     DCT_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X4,    ADST_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X16,     DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X16,    ADST_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X16,    DCT_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_8X16,   ADST_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_16X8,     DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_16X8,    ADST_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_16X8,    DCT_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_16X8,   ADST_ADST, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_16X32,    DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_32X16,    DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_32X64,    DCT_DCT, short>(const short*,short*,int);
    template void ftransform_av1_px<TX_64X32,    DCT_DCT, short>(const short*,short*,int);

    //hbd
    template void ftransform_av1_px<TX_4X4,     DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_4X4,    ADST_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_4X4,    DCT_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_4X4,   ADST_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_4X4,        IDTX, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X8,     DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X8,    ADST_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X8,    DCT_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X8,   ADST_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X8,        IDTX, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_16X16,   DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_16X16,  ADST_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_16X16,  DCT_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_16X16, ADST_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_16X16,      IDTX, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_32X32,   DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_32X32,      IDTX, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_64X64,   DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_4X8,     DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_4X8,    ADST_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_4X8,    DCT_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_4X8,   ADST_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X4,     DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X4,    ADST_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X4,    DCT_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X4,   ADST_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X16,    DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X16,   ADST_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X16,   DCT_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_8X16,  ADST_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_16X8,    DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_16X8,   ADST_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_16X8,   DCT_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_16X8,  ADST_ADST, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_16X32,   DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_32X16,   DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_32X64,   DCT_DCT, int>(const short*, int*, int);
    template void ftransform_av1_px<TX_64X32,   DCT_DCT, int>(const short*, int*, int);

}; // namespace AV1PP
