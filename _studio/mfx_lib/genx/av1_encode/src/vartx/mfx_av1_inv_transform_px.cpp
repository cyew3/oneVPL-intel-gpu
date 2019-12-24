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
#include "mfx_av1_inv_transform_common.h"

using namespace VP9PP;

#define AOMMIN(x, y) (((x) < (y)) ? (x) : (y))
#define AOMMAX(x, y) (((x) > (y)) ? (x) : (y))

#define range_check_buf(...)

static inline int32_t clamp_value(int value, signed char bit) {
  if (bit <= 0) return value;  // Do nothing for invalid clamp bit.
  const long long max_value = (1LL << (bit - 1)) - 1;
  const long long min_value = -(1LL << (bit - 1));
  return (int)clamp64(value, min_value, max_value);
}

static inline void clamp_buf(int32_t *buf, int32_t size, char bit) {
    for (int i = 0; i < size; ++i) buf[i] = clamp_value(buf[i], bit);
}

typedef void (*transform_1d)(const int16_t*, int16_t*);

typedef struct {
    transform_1d cols, rows;  // vertical and horizontal
} transform_2d;

#define INV_COS_BIT 12
static const char inv_cos_bit_col[MAX_TXWH_IDX][MAX_TXWH_IDX] = {
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0,           0 },
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0 },
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
    {           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
    {           0,           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT }
};

static const char inv_cos_bit_row[MAX_TXWH_IDX][MAX_TXWH_IDX] = {
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0,           0 },
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT,           0 },
    { INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
    {           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT },
    {           0,           0, INV_COS_BIT, INV_COS_BIT, INV_COS_BIT }
};

static const char iadst4_range[7] = { 0, 1, 0, 0, 0, 0, 0 };

static void av1_get_inv_txfm_cfg(TxType tx_type, TxSize tx_size, TXFM_2D_FLIP_CFG *cfg) {
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

static const char inv_start_range[TX_SIZES_ALL] = {
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

static void av1_gen_inv_stage_range(char *stage_range_col, char *stage_range_row, const TXFM_2D_FLIP_CFG *cfg,
    TxSize tx_size, int bd)
{
    const int fwd_shift = inv_start_range[tx_size];
    const char *shift = cfg->shift;
    char opt_range_row, opt_range_col;
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

static void av1_idct4_new(const int32_t *input, int32_t *output, char cos_bit, const char *stage_range) {
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

static void av1_idct8_new(const int32_t *input, int32_t *output, char cos_bit, const char *stage_range) {
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

static void av1_idct16_new(const int32_t *input, int32_t *output, char cos_bit, const char *stage_range) {
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

static void av1_idct32_new(const int32_t *input, int32_t *output, char cos_bit, const char *stage_range) {
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

static void av1_iadst4_new(const int32_t *input, int32_t *output, char cos_bit, const char *stage_range) {
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

static void av1_iadst8_new(const int32_t *input, int32_t *output, char cos_bit, const char *stage_range) {
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

static void av1_iadst16_new(const int32_t *input, int32_t *output, char cos_bit, const char *stage_range) {
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

static inline TxfmFunc inv_txfm_type_to_func(TXFM_TYPE txfm_type)
{
    switch (txfm_type) {
    case TXFM_TYPE_DCT4: return av1_idct4_new;
    case TXFM_TYPE_DCT8: return av1_idct8_new;
    case TXFM_TYPE_DCT16: return av1_idct16_new;
    case TXFM_TYPE_DCT32: return av1_idct32_new;
    //case TXFM_TYPE_DCT64: return av1_idct64_new;
    case TXFM_TYPE_ADST4: return av1_iadst4_new;
    case TXFM_TYPE_ADST8: return av1_iadst8_new;
    case TXFM_TYPE_ADST16: return av1_iadst16_new;
    //case TXFM_TYPE_IDENTITY4: return av1_iidentity4_c;
    //case TXFM_TYPE_IDENTITY8: return av1_iidentity8_c;
    //case TXFM_TYPE_IDENTITY16: return av1_iidentity16_c;
    //case TXFM_TYPE_IDENTITY32: return av1_iidentity32_c;
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

static inline int32_t check_range_bd(int32_t input, int bd)
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

static inline uint16_t highbd_clip_pixel_add(uint16_t dest, int32_t trans, int bd)
{
    trans = HIGHBD_WRAPLOW(trans, bd);
    return clip_pixel_highbd(dest + (int)trans, bd);
}

static inline void inv_txfm2d_add_c(const int32_t *input, uint16_t *output, int stride, TXFM_2D_FLIP_CFG *cfg,
                                    int32_t *txfm_buf, TxSize tx_size, int bd, int useAdd)
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
    const char *shift = cfg->shift;
    const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
    char stage_range_row[MAX_TXFM_STAGE_NUM];
    char stage_range_col[MAX_TXFM_STAGE_NUM];
    assert(cfg->stage_num_row <= MAX_TXFM_STAGE_NUM);
    assert(cfg->stage_num_col <= MAX_TXFM_STAGE_NUM);
    av1_gen_inv_stage_range(stage_range_col, stage_range_row, cfg, tx_size, bd);

    const char cos_bit_col = cfg->cos_bit_col;
    const char cos_bit_row = cfg->cos_bit_row;
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
                                            TxType tx_type, TxSize tx_size, int bd, int useAdd)
{
    TXFM_2D_FLIP_CFG cfg;
    av1_get_inv_txfm_cfg(tx_type, tx_size, &cfg);
    // Forward shift sum uses larger square size, to be consistent with what
    // av1_gen_inv_stage_range() does for inverse shifts.
    inv_txfm2d_add_c(input, output, stride, &cfg, txfm_buf, tx_size, bd, useAdd);
}

static void av1_inv_txfm2d_add_4x4_px(const int32_t *input, uint16_t *output, int stride, TxType tx_type, int bd, int useAdd)
{
    int txfm_buf[4 * 4 + 4 + 4];
    inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_4X4, bd, useAdd);
}

static void av1_inv_txfm2d_add_8x8_px(const int32_t *input, uint16_t *output, int stride, TxType tx_type, int bd, int useAdd)
{
    int txfm_buf[8 * 8 + 8 + 8];
    inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_8X8, bd, useAdd);
}

static void av1_inv_txfm2d_add_16x16_px(const int32_t *input, uint16_t *output, int stride, TxType tx_type, int bd, int useAdd)
{
    int txfm_buf[16 * 16 + 16 + 16];
    inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_16X16, bd, useAdd);
}

static void av1_inv_txfm2d_add_32x32_px(const int32_t *input, uint16_t *output, int stride, TxType tx_type, int bd, int useAdd)
{
    int txfm_buf[32 * 32 + 32 + 32];
    inv_txfm2d_add_facade(input, output, stride, txfm_buf, tx_type, TX_32X32, bd, useAdd);
}

namespace VP9PP {
    template <int size, int type> void itransform_add_av1_px(const int16_t *src, uint8_t *dst, int pitchDst) {
        int32_t src32s[32*32];
        int width = 4 << size;
        for (int pos = 0; pos < width*width; pos++) {
            src32s[pos] = (src)[pos];
        }

        if (size == 0)      av1_inv_txfm2d_add_4x4_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 1);
        else if (size == 1) av1_inv_txfm2d_add_8x8_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 1);
        else if (size == 2) av1_inv_txfm2d_add_16x16_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 1);
        else if (size == 3) av1_inv_txfm2d_add_32x32_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 1);
        else {assert(0);}
    }

    template void itransform_add_av1_px<0, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<0, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<0, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<0, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<1, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<1, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<1, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<1, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<2, 0>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<2, 1>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<2, 2>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<2, 3>(const int16_t*,uint8_t*,int);
    template void itransform_add_av1_px<3, 0>(const int16_t*,uint8_t*,int);

    template <int size, int type> void itransform_av1_px(const int16_t *src, int16_t *dst, int pitchDst) {

        int32_t src32s[32*32];
        int width = 4 << size;
        for (int pos = 0; pos < width*width; pos++) {
            src32s[pos] = (src)[pos];
        }

        if (size == 0)      av1_inv_txfm2d_add_4x4_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 0);
        else if (size == 1) av1_inv_txfm2d_add_8x8_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 0);
        else if (size == 2) av1_inv_txfm2d_add_16x16_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 0);
        else if (size == 3) av1_inv_txfm2d_add_32x32_px(src32s, (uint16_t*)dst, pitchDst, type, 8, 0);
        else {assert(0);}
    }

    template void itransform_av1_px<0, 0>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<0, 1>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<0, 2>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<0, 3>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<1, 0>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<1, 1>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<1, 2>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<1, 3>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<2, 0>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<2, 1>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<2, 2>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<2, 3>(const int16_t*,int16_t*,int);
    template void itransform_av1_px<3, 0>(const int16_t*,int16_t*,int);
}; // namespace VP9PP
