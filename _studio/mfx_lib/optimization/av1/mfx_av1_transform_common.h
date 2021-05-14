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
#include "limits.h"
#include "mfx_av1_opts_common.h"

#define av1_zero(dest) memset(&(dest), 0, sizeof(dest))
#define range_check(stage, input, buf, size, bit) { stage, input, buf, size, bit; }

namespace AV1PP {

    static const int NewSqrt2Bits = 12;
    static const int NewInvSqrt2 = 2896; // 2^12 / sqrt(2)
    static const int NewSqrt2 = 5793; // 2^12 * sqrt(2)

    static const int MAX_TXWH_IDX = 5;

    typedef unsigned char TxSize;
    enum {
        TX_4X4,             // 4x4 transform
        TX_8X8,             // 8x8 transform
        TX_16X16,           // 16x16 transform
        TX_32X32,           // 32x32 transform
        TX_64X64,           // 64x64 transform
        TX_4X8,             // 4x8 transform
        TX_8X4,             // 8x4 transform
        TX_8X16,            // 8x16 transform
        TX_16X8,            // 16x8 transform
        TX_16X32,           // 16x32 transform
        TX_32X16,           // 32x16 transform
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
        TX_SIZES_LARGEST = TX_64X64,
        TX_INVALID = 255  // Invalid transform size
    };

    typedef unsigned char TxType;
    enum {
        DCT_DCT,
        ADST_DCT,
        DCT_ADST,
        ADST_ADST,
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

    static const int tx_size_wide[TX_SIZES_ALL] = { 4, 8, 16, 32, 64, 4, 8, 8, 16, 16, 32, 32, 64, 4, 16, 8, 32, 16, 64, };
    static const int tx_size_high[TX_SIZES_ALL] = { 4, 8, 16, 32, 64, 8, 4, 16, 8, 32, 16, 64, 32, 16, 4, 32, 8, 64, 16, };
    static const int tx_size_wide_log2[TX_SIZES_ALL] = { 2, 3, 4, 5, 6, 2, 3, 3, 4, 4, 5, 5, 6, 2, 4, 3, 5, 4, 6, };
    static const int tx_size_high_log2[TX_SIZES_ALL] = { 2, 3, 4, 5, 6, 3, 2, 4, 3, 5, 4, 6, 5, 4, 2, 5, 3, 6, 4, };

    enum TXFM_TYPE {
        TXFM_TYPE_DCT4,
        TXFM_TYPE_DCT8,
        TXFM_TYPE_DCT16,
        TXFM_TYPE_DCT32,
        TXFM_TYPE_DCT64,
        TXFM_TYPE_ADST4,
        TXFM_TYPE_ADST8,
        TXFM_TYPE_ADST16,
        TXFM_TYPE_IDENTITY4,
        TXFM_TYPE_IDENTITY8,
        TXFM_TYPE_IDENTITY16,
        TXFM_TYPE_IDENTITY32,
        TXFM_TYPES,
        TXFM_TYPE_INVALID,
    };

    enum TX_TYPE_1D {
        DCT_1D,
        ADST_1D,
        FLIPADST_1D,
        IDTX_1D,
        TX_TYPES_1D,
    };

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

    const TXFM_TYPE av1_txfm_type_ls[5][TX_TYPES_1D] = {
        { TXFM_TYPE_DCT4, TXFM_TYPE_ADST4, TXFM_TYPE_ADST4, TXFM_TYPE_IDENTITY4 },
        { TXFM_TYPE_DCT8, TXFM_TYPE_ADST8, TXFM_TYPE_ADST8, TXFM_TYPE_IDENTITY8 },
        { TXFM_TYPE_DCT16, TXFM_TYPE_ADST16, TXFM_TYPE_ADST16, TXFM_TYPE_IDENTITY16 },
        { TXFM_TYPE_DCT32, TXFM_TYPE_INVALID, TXFM_TYPE_INVALID, TXFM_TYPE_IDENTITY32 },
        { TXFM_TYPE_DCT64, TXFM_TYPE_INVALID, TXFM_TYPE_INVALID, TXFM_TYPE_INVALID }
    };

    const signed char av1_txfm_stage_num_list[TXFM_TYPES] = {
        4,   // TXFM_TYPE_DCT4
        6,   // TXFM_TYPE_DCT8
        8,   // TXFM_TYPE_DCT16
        10,  // TXFM_TYPE_DCT32
        12,  // TXFM_TYPE_DCT64
        7,   // TXFM_TYPE_ADST4
        8,   // TXFM_TYPE_ADST8
        10,  // TXFM_TYPE_ADST16
        1,   // TXFM_TYPE_IDENTITY4
        1,   // TXFM_TYPE_IDENTITY8
        1,   // TXFM_TYPE_IDENTITY16
        1,   // TXFM_TYPE_IDENTITY32
    };

    static const int MAX_TXFM_STAGE_NUM = 12;

    struct TXFM_2D_FLIP_CFG {
        TxSize tx_size;
        int ud_flip;  // flip upside down
        int lr_flip;  // flip left to right
        const char *shift;
        char cos_bit_col;
        char cos_bit_row;
        char stage_range_col[MAX_TXFM_STAGE_NUM];
        char stage_range_row[MAX_TXFM_STAGE_NUM];
        TXFM_TYPE txfm_type_col;
        TXFM_TYPE txfm_type_row;
        int stage_num_col;
        int stage_num_row;
    };

    static inline void get_flip_cfg(TxType tx_type, int *ud_flip, int *lr_flip)
    {
        switch (tx_type) {
        case DCT_DCT:
        case ADST_DCT:
        case DCT_ADST:
        case ADST_ADST:
            *ud_flip = 0;
            *lr_flip = 0;
            break;
        case IDTX:
        case V_DCT:
        case H_DCT:
        case V_ADST:
        case H_ADST:
            *ud_flip = 0;
            *lr_flip = 0;
            break;
        case FLIPADST_DCT:
        case FLIPADST_ADST:
        case V_FLIPADST:
            *ud_flip = 1;
            *lr_flip = 0;
            break;
        case DCT_FLIPADST:
        case ADST_FLIPADST:
        case H_FLIPADST:
            *ud_flip = 0;
            *lr_flip = 1;
            break;
        case FLIPADST_FLIPADST:
            *ud_flip = 1;
            *lr_flip = 1;
            break;
        default:
            *ud_flip = 0;
            *lr_flip = 0;
            assert(0);
        }
    }

    static inline void set_flip_cfg(int tx_type, TXFM_2D_FLIP_CFG *cfg)
    {
        get_flip_cfg(tx_type, &cfg->ud_flip, &cfg->lr_flip);
    }


    inline int get_txw_idx(TxSize tx_size) { return tx_size_wide_log2[tx_size] - tx_size_wide_log2[0]; }
    inline int get_txh_idx(TxSize tx_size) { return tx_size_high_log2[tx_size] - tx_size_high_log2[0]; }

    // Utility function that returns the log of the ratio of the col and row
    // sizes.
    inline int get_rect_tx_log_ratio(int col, int row)
    {
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

    typedef void (*TxfmFunc)(const int *input, int *output, char cos_bit, const char *stage_range);
    typedef void (*FwdTxfm2dFunc)(const short *input, int *output, int stride, TxType tx_type, int bd);

    // av1_cospi_arr[i][j] = (int)round(cos(M_PI*j/128) * (1<<(cos_bit_min+i)));
    const int av1_cospi_arr_data[7][64] = {
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
    const int av1_sinpi_arr_data[7][5] = {
        { 0, 330, 621, 836, 951 },        { 0, 660, 1241, 1672, 1901 },
        { 0, 1321, 2482, 3344, 3803 },    { 0, 2642, 4964, 6689, 7606 },
        { 0, 5283, 9929, 13377, 15212 },  { 0, 10566, 19858, 26755, 30424 },
        { 0, 21133, 39716, 53510, 60849 }
    };

    static const int cos_bit_min = 10;

    inline const int *cospi_arr(int n) { return av1_cospi_arr_data[n - cos_bit_min]; }
    inline const int *sinpi_arr(int n) { return av1_sinpi_arr_data[n - cos_bit_min]; }

    inline int round_shift(int value, int bit) {
        assert(bit >= 1);
        return (value + (1 << (bit - 1))) >> bit;
    }

    inline int half_btf(int w0, int in0, int w1, int in1, int bit) {
        long long result_64 = (long long)(w0 * in0) + (long long)(w1 * in1);
#if CONFIG_COEFFICIENT_RANGE_CHECKING
        assert(result_64 >= INT32_MIN && result_64 <= INT32_MAX);
#endif
        return round_shift((int)result_64, bit);
    }

    inline int range_check_value(int value, char bit)
    {
#if CONFIG_COEFFICIENT_RANGE_CHECKING
        const int64_t max_value = (1LL << (bit - 1)) - 1;
        const int64_t min_value = -(1LL << (bit - 1));
        if (value < min_value || value > max_value) {
            fprintf(stderr, "coeff out of bit range, value: %d bit %d\n", value, bit);
            assert(0);
        }
#endif  // CONFIG_COEFFICIENT_RANGE_CHECKING
#if DO_RANGE_CHECK_CLAMP
        bit = AOMMIN(bit, 31);
        return clamp(value, (1 << (bit - 1)) - 1, -(1 << (bit - 1)));
#endif  // DO_RANGE_CHECK_CLAMP
        (void)bit;
        return value;
    }

    static inline long long clamp64(long long value, long long low, long long high) {
        return value < low ? low : (value > high ? high : value);
    }

    inline void av1_round_shift_array_c(int *arr, int size, int bit)
    {
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
                    arr[i] = (int)clamp64(((long long)1 << (-bit)) * arr[i], INT_MIN, INT_MAX);
                }
            }
        }
    }

};  // namesapce AV1PP
