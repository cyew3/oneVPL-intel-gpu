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

#pragma once

#include "memory.h"
#include "mfx_av1_transform_common.h"

namespace AV1PP {

    static const char fwd_shift_4x4[3] = { 2, 0, 0 };
    static const char fwd_shift_8x8[3] = { 2, -1, 0 };
    static const char fwd_shift_16x16[3] = { 2, -2, 0 };
    static const char fwd_shift_32x32[3] = { 2, -4, 0 };
    static const char fwd_shift_64x64[3] = { 0, -2, -2 };
    static const char fwd_shift_4x8[3] = { 2, -1, 0 };
    static const char fwd_shift_8x4[3] = { 2, -1, 0 };
    static const char fwd_shift_8x16[3] = { 2, -2, 0 };
    static const char fwd_shift_16x8[3] = { 2, -2, 0 };
    static const char fwd_shift_16x32[3] = { 2, -4, 0 };
    static const char fwd_shift_32x16[3] = { 2, -4, 0 };
    static const char fwd_shift_32x64[3] = { 0, -2, -2 };
    static const char fwd_shift_64x32[3] = { 2, -4, -2 };
    static const char fwd_shift_4x16[3] = { 2, -1, 0 };
    static const char fwd_shift_16x4[3] = { 2, -1, 0 };
    static const char fwd_shift_8x32[3] = { 2, -2, 0 };
    static const char fwd_shift_32x8[3] = { 2, -2, 0 };
    static const char fwd_shift_16x64[3] = { 0, -2, 0 };
    static const char fwd_shift_64x16[3] = { 2, -4, 0 };

    static const char *fwd_txfm_shift_ls[TX_SIZES_ALL] = {
        fwd_shift_4x4,   fwd_shift_8x8,   fwd_shift_16x16, fwd_shift_32x32,
        fwd_shift_64x64, fwd_shift_4x8,   fwd_shift_8x4,   fwd_shift_8x16,
        fwd_shift_16x8,  fwd_shift_16x32, fwd_shift_32x16, fwd_shift_32x64,
        fwd_shift_64x32, fwd_shift_4x16,  fwd_shift_16x4,  fwd_shift_8x32,
        fwd_shift_32x8,  fwd_shift_16x64, fwd_shift_64x16,
    };

    static const char fwd_cos_bit_col[MAX_TXWH_IDX][MAX_TXWH_IDX] = {
        { 13, 13, 13, 0, 0 },
        { 13, 13, 13, 12, 0 },
        { 13, 13, 13, 12, 13 },
        { 0, 13, 13, 12, 13 },
        { 0, 0, 13, 12, 13 }
    };

    static const char fwd_cos_bit_row[MAX_TXWH_IDX][MAX_TXWH_IDX] = {
        { 13, 13, 12, 0, 0 },
        { 13, 13, 13, 12, 0 },
        { 13, 13, 12, 13, 12 },
        { 0, 12, 13, 12, 11 },
        { 0, 0, 12, 11, 10 }
    };

    static const char fdct4_range_mult2[4] = { 0, 2, 3, 3 };
    static const char fdct8_range_mult2[6] = { 0, 2, 4, 5, 5, 5 };
    static const char fdct16_range_mult2[8] = { 0, 2, 4, 6, 7, 7, 7, 7 };
    static const char fdct32_range_mult2[10] = { 0, 2, 4, 6, 8, 9, 9, 9, 9, 9 };
    static const char fdct64_range_mult2[12] = { 0, 2, 4, 6, 8, 10, 11, 11, 11, 11, 11, 11 };
    static const char fadst4_range_mult2[7] = { 0, 2, 4, 3, 3, 3, 3 };
    static const char fadst8_range_mult2[8] = { 0, 0, 1, 3, 3, 5, 5, 5 };
    static const char fadst16_range_mult2[10] = { 0, 0, 1, 3, 3, 5, 5, 7, 7, 7 };
    static const char max_fwd_range_mult2_col[5] = { 3, 5, 7, 9, 11 };
    static const char fidtx4_range_mult2[1] = { 1 };
    static const char fidtx8_range_mult2[1] = { 2 };
    static const char fidtx16_range_mult2[1] = { 3 };
    static const char fidtx32_range_mult2[1] = { 4 };

    static const char *fwd_txfm_range_mult2_list[TXFM_TYPES] = {
        fdct4_range_mult2,  fdct8_range_mult2,   fdct16_range_mult2,
        fdct32_range_mult2, fdct64_range_mult2,  fadst4_range_mult2,
        fadst8_range_mult2, fadst16_range_mult2, fidtx4_range_mult2,
        fidtx8_range_mult2, fidtx16_range_mult2, fidtx32_range_mult2
    };

    inline void set_fwd_txfm_non_scale_range(TXFM_2D_FLIP_CFG *cfg)
    {
        const int txh_idx = get_txh_idx(cfg->tx_size);
        av1_zero(cfg->stage_range_col);
        av1_zero(cfg->stage_range_row);

        if (cfg->txfm_type_col != TXFM_TYPE_INVALID) {
            int stage_num_col = cfg->stage_num_col;
            const char *range_mult2_col = fwd_txfm_range_mult2_list[cfg->txfm_type_col];
            for (int i = 0; i < stage_num_col; ++i)
                cfg->stage_range_col[i] = (range_mult2_col[i] + 1) >> 1;
        }

        if (cfg->txfm_type_row != TXFM_TYPE_INVALID) {
            int stage_num_row = cfg->stage_num_row;
            const char *range_mult2_row = fwd_txfm_range_mult2_list[cfg->txfm_type_row];
            for (int i = 0; i < stage_num_row; ++i)
                cfg->stage_range_row[i] = (max_fwd_range_mult2_col[txh_idx] + range_mult2_row[i] + 1) >> 1;
        }
    }

    inline void av1_get_fwd_txfm_cfg(TxType tx_type, TxSize tx_size, TXFM_2D_FLIP_CFG *cfg)
    {
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
};  // namesapce AV1PP
