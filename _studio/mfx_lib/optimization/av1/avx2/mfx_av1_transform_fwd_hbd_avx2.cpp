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
#include "immintrin.h"
#include "mfx_av1_opts_common.h"

enum { TX_4X4=0, TX_8X8=1, TX_16X16=2, TX_32X32=3, TX_SIZES=4 };
enum { DCT_DCT=0, ADST_DCT=1, DCT_ADST=2, ADST_ADST=3, FLIPADST_DCT=4, DCT_FLIPADST=5,
       FLIPADST_FLIPADST=6, ADST_FLIPADST=7, FLIPADST_ADST=8, IDTX=9, V_DCT=10,
       H_DCT=11, V_ADST=12, H_ADST=13, V_FLIPADST=14, H_FLIPADST=15, TX_TYPES
};

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

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

typedef struct TXFM_1D_CFG {
    int txfm_size;
    int stage_num;

    const int8_t *shift;
    const int8_t *stage_range;
    const int8_t *cos_bit;
    TXFM_TYPE txfm_type;
} TXFM_1D_CFG;

#define MAX_TXFM_STAGE_NUM 12
typedef struct TXFM_2D_FLIP_CFG {
    uint8_t tx_size;
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

#define INLINE inline

namespace details {

    static const int cos_bit_min = 10;
    //static const int cos_bit_max = 16;

    // cospi_arr[i][j] = (int)round(cos(M_PI*j/128) * (1<<(cos_bit_min+i)));
    static const int32_t cospi_arr_data[7][64] = {
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

    static INLINE const int32_t *cospi_arr(int n) {
        return cospi_arr_data[n - cos_bit_min];
    }

    //  ---------------- 4x4 1D constants -----------------------
    // shift
    static const int8_t fwd_shift_4[3] = { 2, 0, 0 };

    // stage range
    static const int8_t fwd_stage_range_col_dct_4[4] = { 0, 1, 2, 2 };
    static const int8_t fwd_stage_range_row_dct_4[4] = { 2, 3, 3, 3 };
    static const int8_t fwd_stage_range_col_adst_4[6] = { 0, 0, 1, 2, 2, 2 };
    static const int8_t fwd_stage_range_row_adst_4[6] = { 2, 2, 2, 3, 3, 3 };
    //static const int8_t fwd_stage_range_idx_4[1] = { 0 };

    // cos bit
    static const int8_t fwd_cos_bit_col_dct_4[4] = { 13, 13, 13, 13 };
    static const int8_t fwd_cos_bit_row_dct_4[4] = { 13, 13, 13, 13 };
    static const int8_t fwd_cos_bit_col_adst_4[6] = { 13, 13, 13, 13, 13, 13 };
    static const int8_t fwd_cos_bit_row_adst_4[6] = { 13, 13, 13, 13, 13, 13 };

    //  ---------------- 8x8 1D constants -----------------------
    // shift
    static const int8_t fwd_shift_8[3] = { 2, -1, 0 };

    // stage range
    static const int8_t fwd_stage_range_col_dct_8[6] = { 0, 1, 2, 3, 3, 3 };
    static const int8_t fwd_stage_range_row_dct_8[6] = { 3, 4, 5, 5, 5, 5 };
    static const int8_t fwd_stage_range_col_adst_8[8] = { 0, 0, 1, 2, 2, 3, 3, 3 };
    static const int8_t fwd_stage_range_row_adst_8[8] = { 3, 3, 3, 4, 4, 5, 5, 5 };
    //static const int8_t fwd_stage_range_idx_8[1] = { 0 };

    // cos bit
    static const int8_t fwd_cos_bit_col_dct_8[6] = { 13, 13, 13, 13, 13, 13 };
    static const int8_t fwd_cos_bit_row_dct_8[6] = { 13, 13, 13, 13, 13, 13 };
    static const int8_t fwd_cos_bit_col_adst_8[8] = {
        13, 13, 13, 13, 13, 13, 13, 13
    };
    static const int8_t fwd_cos_bit_row_adst_8[8] = {
        13, 13, 13, 13, 13, 13, 13, 13
    };

    //  ---------------- 16x16 1D constants -----------------------
    // shift
    static const int8_t fwd_shift_16[3] = { 2, -2, 0 };

    // stage range
    static const int8_t fwd_stage_range_col_dct_16[8] = { 0, 1, 2, 3, 4, 4, 4, 4 };
    static const int8_t fwd_stage_range_row_dct_16[8] = { 4, 5, 6, 7, 7, 7, 7, 7 };
    static const int8_t fwd_stage_range_col_adst_16[10] = { 0, 0, 1, 2, 2,
        3, 3, 4, 4, 4 };
    static const int8_t fwd_stage_range_row_adst_16[10] = {
        4, 4, 4, 5, 5, 6, 6, 7, 7, 7,
    };
    //static const int8_t fwd_stage_range_idx_16[1] = { 0 };

    // cos bit
    static const int8_t fwd_cos_bit_col_dct_16[8] = {
        13, 13, 13, 13, 13, 13, 13, 13
    };
    static const int8_t fwd_cos_bit_row_dct_16[8] = {
        12, 12, 12, 12, 12, 12, 12, 12
    };
    static const int8_t fwd_cos_bit_col_adst_16[10] = { 13, 13, 13, 13, 13,
        13, 13, 13, 13, 13 };
    static const int8_t fwd_cos_bit_row_adst_16[10] = { 12, 12, 12, 12, 12,
        12, 12, 12, 12, 12 };

    //  ---------------- 32x32 1D constants -----------------------


    //  ---------------- 64x64 1D constants -----------------------
    // shift
    //static const int8_t fwd_shift_64[3] = { 0, -2, -2 };

    // stage range
    /*static const int8_t fwd_stage_range_col_dct_64[12] = { 0, 1, 2, 3, 4, 5,
        6, 6, 6, 6, 6, 6 };*/
    /*static const int8_t fwd_stage_range_row_dct_64[12] = { 6,  7,  8,  9,  10, 11,
        11, 11, 11, 11, 11, 11 };*/
    //static const int8_t fwd_stage_range_idx_64[1] = { 0 };

    // cos bit
    /*static const int8_t fwd_cos_bit_col_dct_64[12] = { 15, 15, 15, 15, 15, 14,
        13, 13, 13, 13, 13, 13 };*/
    /*static const int8_t fwd_cos_bit_row_dct_64[12] = { 15, 14, 13, 12, 11, 10,
        10, 10, 10, 10, 10, 10 };*/

    //  ---------------- row config fwd_dct_4 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_4 = {
        4,  // .txfm_size
        4,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_4,                // .shift
        fwd_stage_range_row_dct_4,  // .stage_range
        fwd_cos_bit_row_dct_4,      // .cos_bit
        TXFM_TYPE_DCT4              // .txfm_type
    };

    //  ---------------- row config fwd_dct_8 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_8 = {
        8,  // .txfm_size
        6,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_8,                // .shift
        fwd_stage_range_row_dct_8,  // .stage_range
        fwd_cos_bit_row_dct_8,      // .cos_bit_
        TXFM_TYPE_DCT8              // .txfm_type
    };
    //  ---------------- row config fwd_dct_16 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_16 = {
        16,  // .txfm_size
        8,   // .stage_num
        // 0,  // .log_scale
        fwd_shift_16,                // .shift
        fwd_stage_range_row_dct_16,  // .stage_range
        fwd_cos_bit_row_dct_16,      // .cos_bit
        TXFM_TYPE_DCT16              // .txfm_type
    };

    //  ---------------- row config fwd_dct_64 ----------------
    //static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_dct_64 = {
    //    64,                          // .txfm_size
    //    12,                          // .stage_num
    //    fwd_shift_64,                // .shift
    //    fwd_stage_range_row_dct_64,  // .stage_range
    //    fwd_cos_bit_row_dct_64,      // .cos_bit
    //    TXFM_TYPE_DCT64,             // .txfm_type_col
    //};

    //  ---------------- row config fwd_adst_4 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_4 = {
        4,  // .txfm_size
        6,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_4,                 // .shift
        fwd_stage_range_row_adst_4,  // .stage_range
        fwd_cos_bit_row_adst_4,      // .cos_bit
        TXFM_TYPE_ADST4,             // .txfm_type
    };

    //  ---------------- row config fwd_adst_8 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_8 = {
        8,  // .txfm_size
        8,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_8,                 // .shift
        fwd_stage_range_row_adst_8,  // .stage_range
        fwd_cos_bit_row_adst_8,      // .cos_bit
        TXFM_TYPE_ADST8,             // .txfm_type_col
    };

    //  ---------------- row config fwd_adst_16 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_row_cfg_adst_16 = {
        16,  // .txfm_size
        10,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_16,                 // .shift
        fwd_stage_range_row_adst_16,  // .stage_range
        fwd_cos_bit_row_adst_16,      // .cos_bit
        TXFM_TYPE_ADST16,             // .txfm_type
    };


    //  ---------------- col config fwd_dct_4 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_dct_4 = {
        4,  // .txfm_size
        4,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_4,                // .shift
        fwd_stage_range_col_dct_4,  // .stage_range
        fwd_cos_bit_col_dct_4,      // .cos_bit
        TXFM_TYPE_DCT4              // .txfm_type
    };

    //  ---------------- col config fwd_dct_8 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_dct_8 = {
        8,  // .txfm_size
        6,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_8,                // .shift
        fwd_stage_range_col_dct_8,  // .stage_range
        fwd_cos_bit_col_dct_8,      // .cos_bit_
        TXFM_TYPE_DCT8              // .txfm_type
    };
    //  ---------------- col config fwd_dct_16 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_dct_16 = {
        16,  // .txfm_size
        8,   // .stage_num
        // 0,  // .log_scale
        fwd_shift_16,                // .shift
        fwd_stage_range_col_dct_16,  // .stage_range
        fwd_cos_bit_col_dct_16,      // .cos_bit
        TXFM_TYPE_DCT16              // .txfm_type
    };

    //  ---------------- col config fwd_dct_64 ----------------
    //static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_dct_64 = {
    //    64,                          // .txfm_size
    //    12,                          // .stage_num
    //    fwd_shift_64,                // .shift
    //    fwd_stage_range_col_dct_64,  // .stage_range
    //    fwd_cos_bit_col_dct_64,      // .cos_bit
    //    TXFM_TYPE_DCT64,             // .txfm_type_col
    //};

    //  ---------------- col config fwd_adst_4 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_adst_4 = {
        4,  // .txfm_size
        6,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_4,                 // .shift
        fwd_stage_range_col_adst_4,  // .stage_range
        fwd_cos_bit_col_adst_4,      // .cos_bit
        TXFM_TYPE_ADST4,             // .txfm_type
    };

    //  ---------------- col config fwd_adst_8 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_adst_8 = {
        8,  // .txfm_size
        8,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_8,                 // .shift
        fwd_stage_range_col_adst_8,  // .stage_range
        fwd_cos_bit_col_adst_8,      // .cos_bit
        TXFM_TYPE_ADST8,             // .txfm_type_col
    };

    //  ---------------- col config fwd_adst_16 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_col_cfg_adst_16 = {
        16,  // .txfm_size
        10,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_16,                 // .shift
        fwd_stage_range_col_adst_16,  // .stage_range
        fwd_cos_bit_col_adst_16,      // .cos_bit
        TXFM_TYPE_ADST16,             // .txfm_type
    };

#if CONFIG_EXT_TX
    // identity does not need to differentiate between row and col
    //  ---------------- row/col config fwd_identity_4 ----------
    static const TXFM_1D_CFG fwd_txfm_1d_cfg_identity_4 = {
        4,  // .txfm_size
        1,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_4,            // .shift
        fwd_stage_range_idx_4,  // .stage_range
        NULL,                   // .cos_bit
        TXFM_TYPE_IDENTITY4,    // .txfm_type
    };

    //  ---------------- row/col config fwd_identity_8 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_cfg_identity_8 = {
        8,  // .txfm_size
        1,  // .stage_num
        // 0,  // .log_scale
        fwd_shift_8,            // .shift
        fwd_stage_range_idx_8,  // .stage_range
        NULL,                   // .cos_bit
        TXFM_TYPE_IDENTITY8,    // .txfm_type
    };

    //  ---------------- row/col config fwd_identity_16 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_cfg_identity_16 = {
        16,  // .txfm_size
        1,   // .stage_num
        // 0,  // .log_scale
        fwd_shift_16,            // .shift
        fwd_stage_range_idx_16,  // .stage_range
        NULL,                    // .cos_bit
        TXFM_TYPE_IDENTITY16,    // .txfm_type
    };

    //  ---------------- row/col config fwd_identity_32 ----------------
    static const TXFM_1D_CFG fwd_txfm_1d_cfg_identity_32 = {
        32,  // .txfm_size
        1,   // .stage_num
        // 1,  // .log_scale
        fwd_shift_32,            // .shift
        fwd_stage_range_idx_32,  // .stage_range
        NULL,                    // .cos_bit
        TXFM_TYPE_IDENTITY32,    // .txfm_type
    };
#endif

    // frame transform mode
    typedef enum {
        ONLY_4X4,     // only 4x4 transform used
        ALLOW_8X8,    // allow block transform size up to 8x8
        ALLOW_16X16,  // allow block transform size up to 16x16
        ALLOW_32X32,  // allow block transform size up to 32x32
#if CONFIG_TX64X64
        ALLOW_64X64,  // allow block transform size up to 64x64
#endif
        TX_MODE_SELECT,  // transform specified for each block
        TX_MODES,
    } TX_MODE;

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

    typedef enum {
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
    } TX_TYPE;

    // Reverse the 8 16 bit words in __m128i
    static INLINE __m128i mm_reverse_epi16(const __m128i x) {
        const __m128i a = _mm_shufflelo_epi16(x, 0x1b);
        const __m128i b = _mm_shufflehi_epi16(a, 0x1b);
        return _mm_shuffle_epi32(b, 0x4e);
    }

    static INLINE void load_buffer_8x8(const int16_t *input, __m128i *in,
        int stride, int flipud, int fliplr,
        int shift) {
            __m128i u;
            if (!flipud) {
                in[0] = _mm_load_si128((const __m128i *)(input + 0 * stride));
                in[1] = _mm_load_si128((const __m128i *)(input + 1 * stride));
                in[2] = _mm_load_si128((const __m128i *)(input + 2 * stride));
                in[3] = _mm_load_si128((const __m128i *)(input + 3 * stride));
                in[4] = _mm_load_si128((const __m128i *)(input + 4 * stride));
                in[5] = _mm_load_si128((const __m128i *)(input + 5 * stride));
                in[6] = _mm_load_si128((const __m128i *)(input + 6 * stride));
                in[7] = _mm_load_si128((const __m128i *)(input + 7 * stride));
            } else {
                in[0] = _mm_load_si128((const __m128i *)(input + 7 * stride));
                in[1] = _mm_load_si128((const __m128i *)(input + 6 * stride));
                in[2] = _mm_load_si128((const __m128i *)(input + 5 * stride));
                in[3] = _mm_load_si128((const __m128i *)(input + 4 * stride));
                in[4] = _mm_load_si128((const __m128i *)(input + 3 * stride));
                in[5] = _mm_load_si128((const __m128i *)(input + 2 * stride));
                in[6] = _mm_load_si128((const __m128i *)(input + 1 * stride));
                in[7] = _mm_load_si128((const __m128i *)(input + 0 * stride));
            }

            if (fliplr) {
                in[0] = mm_reverse_epi16(in[0]);
                in[1] = mm_reverse_epi16(in[1]);
                in[2] = mm_reverse_epi16(in[2]);
                in[3] = mm_reverse_epi16(in[3]);
                in[4] = mm_reverse_epi16(in[4]);
                in[5] = mm_reverse_epi16(in[5]);
                in[6] = mm_reverse_epi16(in[6]);
                in[7] = mm_reverse_epi16(in[7]);
            }

            u = _mm_unpackhi_epi64(in[4], in[4]);
            in[8] = _mm_cvtepi16_epi32(in[4]);
            in[9] = _mm_cvtepi16_epi32(u);

            u = _mm_unpackhi_epi64(in[5], in[5]);
            in[10] = _mm_cvtepi16_epi32(in[5]);
            in[11] = _mm_cvtepi16_epi32(u);

            u = _mm_unpackhi_epi64(in[6], in[6]);
            in[12] = _mm_cvtepi16_epi32(in[6]);
            in[13] = _mm_cvtepi16_epi32(u);

            u = _mm_unpackhi_epi64(in[7], in[7]);
            in[14] = _mm_cvtepi16_epi32(in[7]);
            in[15] = _mm_cvtepi16_epi32(u);

            u = _mm_unpackhi_epi64(in[3], in[3]);
            in[6] = _mm_cvtepi16_epi32(in[3]);
            in[7] = _mm_cvtepi16_epi32(u);

            u = _mm_unpackhi_epi64(in[2], in[2]);
            in[4] = _mm_cvtepi16_epi32(in[2]);
            in[5] = _mm_cvtepi16_epi32(u);

            u = _mm_unpackhi_epi64(in[1], in[1]);
            in[2] = _mm_cvtepi16_epi32(in[1]);
            in[3] = _mm_cvtepi16_epi32(u);

            u = _mm_unpackhi_epi64(in[0], in[0]);
            in[0] = _mm_cvtepi16_epi32(in[0]);
            in[1] = _mm_cvtepi16_epi32(u);

            in[0] = _mm_slli_epi32(in[0], shift);
            in[1] = _mm_slli_epi32(in[1], shift);
            in[2] = _mm_slli_epi32(in[2], shift);
            in[3] = _mm_slli_epi32(in[3], shift);
            in[4] = _mm_slli_epi32(in[4], shift);
            in[5] = _mm_slli_epi32(in[5], shift);
            in[6] = _mm_slli_epi32(in[6], shift);
            in[7] = _mm_slli_epi32(in[7], shift);

            in[8] = _mm_slli_epi32(in[8], shift);
            in[9] = _mm_slli_epi32(in[9], shift);
            in[10] = _mm_slli_epi32(in[10], shift);
            in[11] = _mm_slli_epi32(in[11], shift);
            in[12] = _mm_slli_epi32(in[12], shift);
            in[13] = _mm_slli_epi32(in[13], shift);
            in[14] = _mm_slli_epi32(in[14], shift);
            in[15] = _mm_slli_epi32(in[15], shift);
    }

    // Hybrid Transform 16x16

    static INLINE void convert_8x8_to_16x16(const __m128i *in, __m128i *out) {
        int row_index = 0;
        int dst_index = 0;
        int src_index = 0;

        // row 0, 1, .., 7
        do {
            out[dst_index] = in[src_index];
            out[dst_index + 1] = in[src_index + 1];
            out[dst_index + 2] = in[src_index + 16];
            out[dst_index + 3] = in[src_index + 17];
            dst_index += 4;
            src_index += 2;
            row_index += 1;
        } while (row_index < 8);

        // row 8, 9, ..., 15
        src_index += 16;
        do {
            out[dst_index] = in[src_index];
            out[dst_index + 1] = in[src_index + 1];
            out[dst_index + 2] = in[src_index + 16];
            out[dst_index + 3] = in[src_index + 17];
            dst_index += 4;
            src_index += 2;
            row_index += 1;
        } while (row_index < 16);
    }

    static INLINE void load_buffer_16x16(const int16_t *input, __m128i *out,
        int stride, int flipud, int fliplr,
        int shift) {
            __m128i in[64];
            // Load 4 8x8 blocks
            const int16_t *topL = input;
            const int16_t *topR = input + 8;
            const int16_t *botL = input + 8 * stride;
            const int16_t *botR = input + 8 * stride + 8;

            const int16_t *tmp;

            if (flipud) {
                // Swap left columns
                tmp = topL;
                topL = botL;
                botL = tmp;
                // Swap right columns
                tmp = topR;
                topR = botR;
                botR = tmp;
            }

            if (fliplr) {
                // Swap top rows
                tmp = topL;
                topL = topR;
                topR = tmp;
                // Swap bottom rows
                tmp = botL;
                botL = botR;
                botR = tmp;
            }

            // load first 8 columns
            load_buffer_8x8(topL, &in[0], stride, flipud, fliplr, shift);
            load_buffer_8x8(botL, &in[32], stride, flipud, fliplr, shift);

            // load second 8 columns
            load_buffer_8x8(topR, &in[16], stride, flipud, fliplr, shift);
            load_buffer_8x8(botR, &in[48], stride, flipud, fliplr, shift);

            convert_8x8_to_16x16(in, out);
    }

    static void fdct16x16_sse4_1(__m128i *in, __m128i *out, int bit) {
        const int32_t *cospi = cospi_arr(bit);
        const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
        const __m128i cospim32 = _mm_set1_epi32(-cospi[32]);
        const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
        const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
        const __m128i cospim48 = _mm_set1_epi32(-cospi[48]);
        const __m128i cospim16 = _mm_set1_epi32(-cospi[16]);
        const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
        const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
        const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
        const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
        const __m128i cospi60 = _mm_set1_epi32(cospi[60]);
        const __m128i cospi4 = _mm_set1_epi32(cospi[4]);
        const __m128i cospi28 = _mm_set1_epi32(cospi[28]);
        const __m128i cospi36 = _mm_set1_epi32(cospi[36]);
        const __m128i cospi44 = _mm_set1_epi32(cospi[44]);
        const __m128i cospi20 = _mm_set1_epi32(cospi[20]);
        const __m128i cospi12 = _mm_set1_epi32(cospi[12]);
        const __m128i cospi52 = _mm_set1_epi32(cospi[52]);
        const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
        __m128i u[16], v[16], x;
        const int col_num = 4;
        int col;

        // Calculate the column 0, 1, 2, 3
        for (col = 0; col < col_num; ++col) {
            // stage 0
            // stage 1
            u[0] = _mm_add_epi32(in[0 * col_num + col], in[15 * col_num + col]);
            u[15] = _mm_sub_epi32(in[0 * col_num + col], in[15 * col_num + col]);
            u[1] = _mm_add_epi32(in[1 * col_num + col], in[14 * col_num + col]);
            u[14] = _mm_sub_epi32(in[1 * col_num + col], in[14 * col_num + col]);
            u[2] = _mm_add_epi32(in[2 * col_num + col], in[13 * col_num + col]);
            u[13] = _mm_sub_epi32(in[2 * col_num + col], in[13 * col_num + col]);
            u[3] = _mm_add_epi32(in[3 * col_num + col], in[12 * col_num + col]);
            u[12] = _mm_sub_epi32(in[3 * col_num + col], in[12 * col_num + col]);
            u[4] = _mm_add_epi32(in[4 * col_num + col], in[11 * col_num + col]);
            u[11] = _mm_sub_epi32(in[4 * col_num + col], in[11 * col_num + col]);
            u[5] = _mm_add_epi32(in[5 * col_num + col], in[10 * col_num + col]);
            u[10] = _mm_sub_epi32(in[5 * col_num + col], in[10 * col_num + col]);
            u[6] = _mm_add_epi32(in[6 * col_num + col], in[9 * col_num + col]);
            u[9] = _mm_sub_epi32(in[6 * col_num + col], in[9 * col_num + col]);
            u[7] = _mm_add_epi32(in[7 * col_num + col], in[8 * col_num + col]);
            u[8] = _mm_sub_epi32(in[7 * col_num + col], in[8 * col_num + col]);

            // stage 2
            v[0] = _mm_add_epi32(u[0], u[7]);
            v[7] = _mm_sub_epi32(u[0], u[7]);
            v[1] = _mm_add_epi32(u[1], u[6]);
            v[6] = _mm_sub_epi32(u[1], u[6]);
            v[2] = _mm_add_epi32(u[2], u[5]);
            v[5] = _mm_sub_epi32(u[2], u[5]);
            v[3] = _mm_add_epi32(u[3], u[4]);
            v[4] = _mm_sub_epi32(u[3], u[4]);
            v[8] = u[8];
            v[9] = u[9];

            v[10] = _mm_mullo_epi32(u[10], cospim32);
            x = _mm_mullo_epi32(u[13], cospi32);
            v[10] = _mm_add_epi32(v[10], x);
            v[10] = _mm_add_epi32(v[10], rnding);
            v[10] = _mm_srai_epi32(v[10], bit);

            v[13] = _mm_mullo_epi32(u[10], cospi32);
            x = _mm_mullo_epi32(u[13], cospim32);
            v[13] = _mm_sub_epi32(v[13], x);
            v[13] = _mm_add_epi32(v[13], rnding);
            v[13] = _mm_srai_epi32(v[13], bit);

            v[11] = _mm_mullo_epi32(u[11], cospim32);
            x = _mm_mullo_epi32(u[12], cospi32);
            v[11] = _mm_add_epi32(v[11], x);
            v[11] = _mm_add_epi32(v[11], rnding);
            v[11] = _mm_srai_epi32(v[11], bit);

            v[12] = _mm_mullo_epi32(u[11], cospi32);
            x = _mm_mullo_epi32(u[12], cospim32);
            v[12] = _mm_sub_epi32(v[12], x);
            v[12] = _mm_add_epi32(v[12], rnding);
            v[12] = _mm_srai_epi32(v[12], bit);
            v[14] = u[14];
            v[15] = u[15];

            // stage 3
            u[0] = _mm_add_epi32(v[0], v[3]);
            u[3] = _mm_sub_epi32(v[0], v[3]);
            u[1] = _mm_add_epi32(v[1], v[2]);
            u[2] = _mm_sub_epi32(v[1], v[2]);
            u[4] = v[4];

            u[5] = _mm_mullo_epi32(v[5], cospim32);
            x = _mm_mullo_epi32(v[6], cospi32);
            u[5] = _mm_add_epi32(u[5], x);
            u[5] = _mm_add_epi32(u[5], rnding);
            u[5] = _mm_srai_epi32(u[5], bit);

            u[6] = _mm_mullo_epi32(v[5], cospi32);
            x = _mm_mullo_epi32(v[6], cospim32);
            u[6] = _mm_sub_epi32(u[6], x);
            u[6] = _mm_add_epi32(u[6], rnding);
            u[6] = _mm_srai_epi32(u[6], bit);

            u[7] = v[7];
            u[8] = _mm_add_epi32(v[8], v[11]);
            u[11] = _mm_sub_epi32(v[8], v[11]);
            u[9] = _mm_add_epi32(v[9], v[10]);
            u[10] = _mm_sub_epi32(v[9], v[10]);
            u[12] = _mm_sub_epi32(v[15], v[12]);
            u[15] = _mm_add_epi32(v[15], v[12]);
            u[13] = _mm_sub_epi32(v[14], v[13]);
            u[14] = _mm_add_epi32(v[14], v[13]);

            // stage 4
            u[0] = _mm_mullo_epi32(u[0], cospi32);
            u[1] = _mm_mullo_epi32(u[1], cospi32);
            v[0] = _mm_add_epi32(u[0], u[1]);
            v[0] = _mm_add_epi32(v[0], rnding);
            v[0] = _mm_srai_epi32(v[0], bit);

            v[1] = _mm_sub_epi32(u[0], u[1]);
            v[1] = _mm_add_epi32(v[1], rnding);
            v[1] = _mm_srai_epi32(v[1], bit);

            v[2] = _mm_mullo_epi32(u[2], cospi48);
            x = _mm_mullo_epi32(u[3], cospi16);
            v[2] = _mm_add_epi32(v[2], x);
            v[2] = _mm_add_epi32(v[2], rnding);
            v[2] = _mm_srai_epi32(v[2], bit);

            v[3] = _mm_mullo_epi32(u[2], cospi16);
            x = _mm_mullo_epi32(u[3], cospi48);
            v[3] = _mm_sub_epi32(x, v[3]);
            v[3] = _mm_add_epi32(v[3], rnding);
            v[3] = _mm_srai_epi32(v[3], bit);

            v[4] = _mm_add_epi32(u[4], u[5]);
            v[5] = _mm_sub_epi32(u[4], u[5]);
            v[6] = _mm_sub_epi32(u[7], u[6]);
            v[7] = _mm_add_epi32(u[7], u[6]);
            v[8] = u[8];

            v[9] = _mm_mullo_epi32(u[9], cospim16);
            x = _mm_mullo_epi32(u[14], cospi48);
            v[9] = _mm_add_epi32(v[9], x);
            v[9] = _mm_add_epi32(v[9], rnding);
            v[9] = _mm_srai_epi32(v[9], bit);

            v[14] = _mm_mullo_epi32(u[9], cospi48);
            x = _mm_mullo_epi32(u[14], cospim16);
            v[14] = _mm_sub_epi32(v[14], x);
            v[14] = _mm_add_epi32(v[14], rnding);
            v[14] = _mm_srai_epi32(v[14], bit);

            v[10] = _mm_mullo_epi32(u[10], cospim48);
            x = _mm_mullo_epi32(u[13], cospim16);
            v[10] = _mm_add_epi32(v[10], x);
            v[10] = _mm_add_epi32(v[10], rnding);
            v[10] = _mm_srai_epi32(v[10], bit);

            v[13] = _mm_mullo_epi32(u[10], cospim16);
            x = _mm_mullo_epi32(u[13], cospim48);
            v[13] = _mm_sub_epi32(v[13], x);
            v[13] = _mm_add_epi32(v[13], rnding);
            v[13] = _mm_srai_epi32(v[13], bit);

            v[11] = u[11];
            v[12] = u[12];
            v[15] = u[15];

            // stage 5
            u[0] = v[0];
            u[1] = v[1];
            u[2] = v[2];
            u[3] = v[3];

            u[4] = _mm_mullo_epi32(v[4], cospi56);
            x = _mm_mullo_epi32(v[7], cospi8);
            u[4] = _mm_add_epi32(u[4], x);
            u[4] = _mm_add_epi32(u[4], rnding);
            u[4] = _mm_srai_epi32(u[4], bit);

            u[7] = _mm_mullo_epi32(v[4], cospi8);
            x = _mm_mullo_epi32(v[7], cospi56);
            u[7] = _mm_sub_epi32(x, u[7]);
            u[7] = _mm_add_epi32(u[7], rnding);
            u[7] = _mm_srai_epi32(u[7], bit);

            u[5] = _mm_mullo_epi32(v[5], cospi24);
            x = _mm_mullo_epi32(v[6], cospi40);
            u[5] = _mm_add_epi32(u[5], x);
            u[5] = _mm_add_epi32(u[5], rnding);
            u[5] = _mm_srai_epi32(u[5], bit);

            u[6] = _mm_mullo_epi32(v[5], cospi40);
            x = _mm_mullo_epi32(v[6], cospi24);
            u[6] = _mm_sub_epi32(x, u[6]);
            u[6] = _mm_add_epi32(u[6], rnding);
            u[6] = _mm_srai_epi32(u[6], bit);

            u[8] = _mm_add_epi32(v[8], v[9]);
            u[9] = _mm_sub_epi32(v[8], v[9]);
            u[10] = _mm_sub_epi32(v[11], v[10]);
            u[11] = _mm_add_epi32(v[11], v[10]);
            u[12] = _mm_add_epi32(v[12], v[13]);
            u[13] = _mm_sub_epi32(v[12], v[13]);
            u[14] = _mm_sub_epi32(v[15], v[14]);
            u[15] = _mm_add_epi32(v[15], v[14]);

            // stage 6
            v[0] = u[0];
            v[1] = u[1];
            v[2] = u[2];
            v[3] = u[3];
            v[4] = u[4];
            v[5] = u[5];
            v[6] = u[6];
            v[7] = u[7];

            v[8] = _mm_mullo_epi32(u[8], cospi60);
            x = _mm_mullo_epi32(u[15], cospi4);
            v[8] = _mm_add_epi32(v[8], x);
            v[8] = _mm_add_epi32(v[8], rnding);
            v[8] = _mm_srai_epi32(v[8], bit);

            v[15] = _mm_mullo_epi32(u[8], cospi4);
            x = _mm_mullo_epi32(u[15], cospi60);
            v[15] = _mm_sub_epi32(x, v[15]);
            v[15] = _mm_add_epi32(v[15], rnding);
            v[15] = _mm_srai_epi32(v[15], bit);

            v[9] = _mm_mullo_epi32(u[9], cospi28);
            x = _mm_mullo_epi32(u[14], cospi36);
            v[9] = _mm_add_epi32(v[9], x);
            v[9] = _mm_add_epi32(v[9], rnding);
            v[9] = _mm_srai_epi32(v[9], bit);

            v[14] = _mm_mullo_epi32(u[9], cospi36);
            x = _mm_mullo_epi32(u[14], cospi28);
            v[14] = _mm_sub_epi32(x, v[14]);
            v[14] = _mm_add_epi32(v[14], rnding);
            v[14] = _mm_srai_epi32(v[14], bit);

            v[10] = _mm_mullo_epi32(u[10], cospi44);
            x = _mm_mullo_epi32(u[13], cospi20);
            v[10] = _mm_add_epi32(v[10], x);
            v[10] = _mm_add_epi32(v[10], rnding);
            v[10] = _mm_srai_epi32(v[10], bit);

            v[13] = _mm_mullo_epi32(u[10], cospi20);
            x = _mm_mullo_epi32(u[13], cospi44);
            v[13] = _mm_sub_epi32(x, v[13]);
            v[13] = _mm_add_epi32(v[13], rnding);
            v[13] = _mm_srai_epi32(v[13], bit);

            v[11] = _mm_mullo_epi32(u[11], cospi12);
            x = _mm_mullo_epi32(u[12], cospi52);
            v[11] = _mm_add_epi32(v[11], x);
            v[11] = _mm_add_epi32(v[11], rnding);
            v[11] = _mm_srai_epi32(v[11], bit);

            v[12] = _mm_mullo_epi32(u[11], cospi52);
            x = _mm_mullo_epi32(u[12], cospi12);
            v[12] = _mm_sub_epi32(x, v[12]);
            v[12] = _mm_add_epi32(v[12], rnding);
            v[12] = _mm_srai_epi32(v[12], bit);

            out[0 * col_num + col] = v[0];
            out[1 * col_num + col] = v[8];
            out[2 * col_num + col] = v[4];
            out[3 * col_num + col] = v[12];
            out[4 * col_num + col] = v[2];
            out[5 * col_num + col] = v[10];
            out[6 * col_num + col] = v[6];
            out[7 * col_num + col] = v[14];
            out[8 * col_num + col] = v[1];
            out[9 * col_num + col] = v[9];
            out[10 * col_num + col] = v[5];
            out[11 * col_num + col] = v[13];
            out[12 * col_num + col] = v[3];
            out[13 * col_num + col] = v[11];
            out[14 * col_num + col] = v[7];
            out[15 * col_num + col] = v[15];
        }
    }

    static INLINE void col_txfm_8x8_rounding(__m128i *in, int shift) {
        const __m128i rounding = _mm_set1_epi32(1 << (shift - 1));

        in[0] = _mm_add_epi32(in[0], rounding);
        in[1] = _mm_add_epi32(in[1], rounding);
        in[2] = _mm_add_epi32(in[2], rounding);
        in[3] = _mm_add_epi32(in[3], rounding);
        in[4] = _mm_add_epi32(in[4], rounding);
        in[5] = _mm_add_epi32(in[5], rounding);
        in[6] = _mm_add_epi32(in[6], rounding);
        in[7] = _mm_add_epi32(in[7], rounding);
        in[8] = _mm_add_epi32(in[8], rounding);
        in[9] = _mm_add_epi32(in[9], rounding);
        in[10] = _mm_add_epi32(in[10], rounding);
        in[11] = _mm_add_epi32(in[11], rounding);
        in[12] = _mm_add_epi32(in[12], rounding);
        in[13] = _mm_add_epi32(in[13], rounding);
        in[14] = _mm_add_epi32(in[14], rounding);
        in[15] = _mm_add_epi32(in[15], rounding);

        in[0] = _mm_srai_epi32(in[0], shift);
        in[1] = _mm_srai_epi32(in[1], shift);
        in[2] = _mm_srai_epi32(in[2], shift);
        in[3] = _mm_srai_epi32(in[3], shift);
        in[4] = _mm_srai_epi32(in[4], shift);
        in[5] = _mm_srai_epi32(in[5], shift);
        in[6] = _mm_srai_epi32(in[6], shift);
        in[7] = _mm_srai_epi32(in[7], shift);
        in[8] = _mm_srai_epi32(in[8], shift);
        in[9] = _mm_srai_epi32(in[9], shift);
        in[10] = _mm_srai_epi32(in[10], shift);
        in[11] = _mm_srai_epi32(in[11], shift);
        in[12] = _mm_srai_epi32(in[12], shift);
        in[13] = _mm_srai_epi32(in[13], shift);
        in[14] = _mm_srai_epi32(in[14], shift);
        in[15] = _mm_srai_epi32(in[15], shift);
    }

    static void col_txfm_16x16_rounding(__m128i *in, int shift) {
        // Note:
        //  We split 16x16 rounding into 4 sections of 8x8 rounding,
        //  instead of 4 columns
        col_txfm_8x8_rounding(&in[0], shift);
        col_txfm_8x8_rounding(&in[16], shift);
        col_txfm_8x8_rounding(&in[32], shift);
        col_txfm_8x8_rounding(&in[48], shift);
    }

#define TRANSPOSE_4X4(x0, x1, x2, x3, y0, y1, y2, y3) \
    do {                                                \
    __m128i u0, u1, u2, u3;                           \
    u0 = _mm_unpacklo_epi32(x0, x1);                  \
    u1 = _mm_unpackhi_epi32(x0, x1);                  \
    u2 = _mm_unpacklo_epi32(x2, x3);                  \
    u3 = _mm_unpackhi_epi32(x2, x3);                  \
    y0 = _mm_unpacklo_epi64(u0, u2);                  \
    y1 = _mm_unpackhi_epi64(u0, u2);                  \
    y2 = _mm_unpacklo_epi64(u1, u3);                  \
    y3 = _mm_unpackhi_epi64(u1, u3);                  \
    } while (0)

    static INLINE void transpose_16x16(const __m128i *in, __m128i *out) {
        // Upper left 8x8
        TRANSPOSE_4X4(in[0], in[4], in[8], in[12], out[0], out[4], out[8], out[12]);
        TRANSPOSE_4X4(in[1], in[5], in[9], in[13], out[16], out[20], out[24],
            out[28]);
        TRANSPOSE_4X4(in[16], in[20], in[24], in[28], out[1], out[5], out[9],
            out[13]);
        TRANSPOSE_4X4(in[17], in[21], in[25], in[29], out[17], out[21], out[25],
            out[29]);

        // Upper right 8x8
        TRANSPOSE_4X4(in[2], in[6], in[10], in[14], out[32], out[36], out[40],
            out[44]);
        TRANSPOSE_4X4(in[3], in[7], in[11], in[15], out[48], out[52], out[56],
            out[60]);
        TRANSPOSE_4X4(in[18], in[22], in[26], in[30], out[33], out[37], out[41],
            out[45]);
        TRANSPOSE_4X4(in[19], in[23], in[27], in[31], out[49], out[53], out[57],
            out[61]);

        // Lower left 8x8
        TRANSPOSE_4X4(in[32], in[36], in[40], in[44], out[2], out[6], out[10],
            out[14]);
        TRANSPOSE_4X4(in[33], in[37], in[41], in[45], out[18], out[22], out[26],
            out[30]);
        TRANSPOSE_4X4(in[48], in[52], in[56], in[60], out[3], out[7], out[11],
            out[15]);
        TRANSPOSE_4X4(in[49], in[53], in[57], in[61], out[19], out[23], out[27],
            out[31]);
        // Lower right 8x8
        TRANSPOSE_4X4(in[34], in[38], in[42], in[46], out[34], out[38], out[42],
            out[46]);
        TRANSPOSE_4X4(in[35], in[39], in[43], in[47], out[50], out[54], out[58],
            out[62]);
        TRANSPOSE_4X4(in[50], in[54], in[58], in[62], out[35], out[39], out[43],
            out[47]);
        TRANSPOSE_4X4(in[51], in[55], in[59], in[63], out[51], out[55], out[59],
            out[63]);
    }

    static INLINE void write_buffer_8x8(const __m128i *res, int32_t *output) {
        _mm_store_si128((__m128i *)(output + 0 * 4), res[0]);
        _mm_store_si128((__m128i *)(output + 1 * 4), res[1]);
        _mm_store_si128((__m128i *)(output + 2 * 4), res[2]);
        _mm_store_si128((__m128i *)(output + 3 * 4), res[3]);

        _mm_store_si128((__m128i *)(output + 4 * 4), res[4]);
        _mm_store_si128((__m128i *)(output + 5 * 4), res[5]);
        _mm_store_si128((__m128i *)(output + 6 * 4), res[6]);
        _mm_store_si128((__m128i *)(output + 7 * 4), res[7]);

        _mm_store_si128((__m128i *)(output + 8 * 4), res[8]);
        _mm_store_si128((__m128i *)(output + 9 * 4), res[9]);
        _mm_store_si128((__m128i *)(output + 10 * 4), res[10]);
        _mm_store_si128((__m128i *)(output + 11 * 4), res[11]);

        _mm_store_si128((__m128i *)(output + 12 * 4), res[12]);
        _mm_store_si128((__m128i *)(output + 13 * 4), res[13]);
        _mm_store_si128((__m128i *)(output + 14 * 4), res[14]);
        _mm_store_si128((__m128i *)(output + 15 * 4), res[15]);
    }

    static void write_buffer_16x16(const __m128i *in, int32_t *output) {
        const int size_8x8 = 16 * 4;
        write_buffer_8x8(&in[0], output);
        output += size_8x8;
        write_buffer_8x8(&in[16], output);
        output += size_8x8;
        write_buffer_8x8(&in[32], output);
        output += size_8x8;
        write_buffer_8x8(&in[48], output);
    }

    static void fadst16x16_sse4_1(__m128i *in, __m128i *out, int bit) {
        const int32_t *cospi = cospi_arr(bit);
        const __m128i cospi2 = _mm_set1_epi32(cospi[2]);
        const __m128i cospi62 = _mm_set1_epi32(cospi[62]);
        const __m128i cospi10 = _mm_set1_epi32(cospi[10]);
        const __m128i cospi54 = _mm_set1_epi32(cospi[54]);
        const __m128i cospi18 = _mm_set1_epi32(cospi[18]);
        const __m128i cospi46 = _mm_set1_epi32(cospi[46]);
        const __m128i cospi26 = _mm_set1_epi32(cospi[26]);
        const __m128i cospi38 = _mm_set1_epi32(cospi[38]);
        const __m128i cospi34 = _mm_set1_epi32(cospi[34]);
        const __m128i cospi30 = _mm_set1_epi32(cospi[30]);
        const __m128i cospi42 = _mm_set1_epi32(cospi[42]);
        const __m128i cospi22 = _mm_set1_epi32(cospi[22]);
        const __m128i cospi50 = _mm_set1_epi32(cospi[50]);
        const __m128i cospi14 = _mm_set1_epi32(cospi[14]);
        const __m128i cospi58 = _mm_set1_epi32(cospi[58]);
        const __m128i cospi6 = _mm_set1_epi32(cospi[6]);
        const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
        const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
        const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
        const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
        const __m128i cospim56 = _mm_set1_epi32(-cospi[56]);
        const __m128i cospim24 = _mm_set1_epi32(-cospi[24]);
        const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
        const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
        const __m128i cospim48 = _mm_set1_epi32(-cospi[48]);
        const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
        const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
        __m128i u[16], v[16], x, y;
        const int col_num = 4;
        int col;

        // Calculate the column 0, 1, 2, 3
        for (col = 0; col < col_num; ++col) {
            // stage 0
            // stage 1
            // stage 2
            v[0] = _mm_mullo_epi32(in[15 * col_num + col], cospi2);
            x = _mm_mullo_epi32(in[0 * col_num + col], cospi62);
            v[0] = _mm_add_epi32(v[0], x);
            v[0] = _mm_add_epi32(v[0], rnding);
            v[0] = _mm_srai_epi32(v[0], bit);

            v[1] = _mm_mullo_epi32(in[15 * col_num + col], cospi62);
            x = _mm_mullo_epi32(in[0 * col_num + col], cospi2);
            v[1] = _mm_sub_epi32(v[1], x);
            v[1] = _mm_add_epi32(v[1], rnding);
            v[1] = _mm_srai_epi32(v[1], bit);

            v[2] = _mm_mullo_epi32(in[13 * col_num + col], cospi10);
            x = _mm_mullo_epi32(in[2 * col_num + col], cospi54);
            v[2] = _mm_add_epi32(v[2], x);
            v[2] = _mm_add_epi32(v[2], rnding);
            v[2] = _mm_srai_epi32(v[2], bit);

            v[3] = _mm_mullo_epi32(in[13 * col_num + col], cospi54);
            x = _mm_mullo_epi32(in[2 * col_num + col], cospi10);
            v[3] = _mm_sub_epi32(v[3], x);
            v[3] = _mm_add_epi32(v[3], rnding);
            v[3] = _mm_srai_epi32(v[3], bit);

            v[4] = _mm_mullo_epi32(in[11 * col_num + col], cospi18);
            x = _mm_mullo_epi32(in[4 * col_num + col], cospi46);
            v[4] = _mm_add_epi32(v[4], x);
            v[4] = _mm_add_epi32(v[4], rnding);
            v[4] = _mm_srai_epi32(v[4], bit);

            v[5] = _mm_mullo_epi32(in[11 * col_num + col], cospi46);
            x = _mm_mullo_epi32(in[4 * col_num + col], cospi18);
            v[5] = _mm_sub_epi32(v[5], x);
            v[5] = _mm_add_epi32(v[5], rnding);
            v[5] = _mm_srai_epi32(v[5], bit);

            v[6] = _mm_mullo_epi32(in[9 * col_num + col], cospi26);
            x = _mm_mullo_epi32(in[6 * col_num + col], cospi38);
            v[6] = _mm_add_epi32(v[6], x);
            v[6] = _mm_add_epi32(v[6], rnding);
            v[6] = _mm_srai_epi32(v[6], bit);

            v[7] = _mm_mullo_epi32(in[9 * col_num + col], cospi38);
            x = _mm_mullo_epi32(in[6 * col_num + col], cospi26);
            v[7] = _mm_sub_epi32(v[7], x);
            v[7] = _mm_add_epi32(v[7], rnding);
            v[7] = _mm_srai_epi32(v[7], bit);

            v[8] = _mm_mullo_epi32(in[7 * col_num + col], cospi34);
            x = _mm_mullo_epi32(in[8 * col_num + col], cospi30);
            v[8] = _mm_add_epi32(v[8], x);
            v[8] = _mm_add_epi32(v[8], rnding);
            v[8] = _mm_srai_epi32(v[8], bit);

            v[9] = _mm_mullo_epi32(in[7 * col_num + col], cospi30);
            x = _mm_mullo_epi32(in[8 * col_num + col], cospi34);
            v[9] = _mm_sub_epi32(v[9], x);
            v[9] = _mm_add_epi32(v[9], rnding);
            v[9] = _mm_srai_epi32(v[9], bit);

            v[10] = _mm_mullo_epi32(in[5 * col_num + col], cospi42);
            x = _mm_mullo_epi32(in[10 * col_num + col], cospi22);
            v[10] = _mm_add_epi32(v[10], x);
            v[10] = _mm_add_epi32(v[10], rnding);
            v[10] = _mm_srai_epi32(v[10], bit);

            v[11] = _mm_mullo_epi32(in[5 * col_num + col], cospi22);
            x = _mm_mullo_epi32(in[10 * col_num + col], cospi42);
            v[11] = _mm_sub_epi32(v[11], x);
            v[11] = _mm_add_epi32(v[11], rnding);
            v[11] = _mm_srai_epi32(v[11], bit);

            v[12] = _mm_mullo_epi32(in[3 * col_num + col], cospi50);
            x = _mm_mullo_epi32(in[12 * col_num + col], cospi14);
            v[12] = _mm_add_epi32(v[12], x);
            v[12] = _mm_add_epi32(v[12], rnding);
            v[12] = _mm_srai_epi32(v[12], bit);

            v[13] = _mm_mullo_epi32(in[3 * col_num + col], cospi14);
            x = _mm_mullo_epi32(in[12 * col_num + col], cospi50);
            v[13] = _mm_sub_epi32(v[13], x);
            v[13] = _mm_add_epi32(v[13], rnding);
            v[13] = _mm_srai_epi32(v[13], bit);

            v[14] = _mm_mullo_epi32(in[1 * col_num + col], cospi58);
            x = _mm_mullo_epi32(in[14 * col_num + col], cospi6);
            v[14] = _mm_add_epi32(v[14], x);
            v[14] = _mm_add_epi32(v[14], rnding);
            v[14] = _mm_srai_epi32(v[14], bit);

            v[15] = _mm_mullo_epi32(in[1 * col_num + col], cospi6);
            x = _mm_mullo_epi32(in[14 * col_num + col], cospi58);
            v[15] = _mm_sub_epi32(v[15], x);
            v[15] = _mm_add_epi32(v[15], rnding);
            v[15] = _mm_srai_epi32(v[15], bit);

            // stage 3
            u[0] = _mm_add_epi32(v[0], v[8]);
            u[8] = _mm_sub_epi32(v[0], v[8]);
            u[1] = _mm_add_epi32(v[1], v[9]);
            u[9] = _mm_sub_epi32(v[1], v[9]);
            u[2] = _mm_add_epi32(v[2], v[10]);
            u[10] = _mm_sub_epi32(v[2], v[10]);
            u[3] = _mm_add_epi32(v[3], v[11]);
            u[11] = _mm_sub_epi32(v[3], v[11]);
            u[4] = _mm_add_epi32(v[4], v[12]);
            u[12] = _mm_sub_epi32(v[4], v[12]);
            u[5] = _mm_add_epi32(v[5], v[13]);
            u[13] = _mm_sub_epi32(v[5], v[13]);
            u[6] = _mm_add_epi32(v[6], v[14]);
            u[14] = _mm_sub_epi32(v[6], v[14]);
            u[7] = _mm_add_epi32(v[7], v[15]);
            u[15] = _mm_sub_epi32(v[7], v[15]);

            // stage 4
            v[0] = u[0];
            v[1] = u[1];
            v[2] = u[2];
            v[3] = u[3];
            v[4] = u[4];
            v[5] = u[5];
            v[6] = u[6];
            v[7] = u[7];

            v[8] = _mm_mullo_epi32(u[8], cospi8);
            x = _mm_mullo_epi32(u[9], cospi56);
            v[8] = _mm_add_epi32(v[8], x);
            v[8] = _mm_add_epi32(v[8], rnding);
            v[8] = _mm_srai_epi32(v[8], bit);

            v[9] = _mm_mullo_epi32(u[8], cospi56);
            x = _mm_mullo_epi32(u[9], cospi8);
            v[9] = _mm_sub_epi32(v[9], x);
            v[9] = _mm_add_epi32(v[9], rnding);
            v[9] = _mm_srai_epi32(v[9], bit);

            v[10] = _mm_mullo_epi32(u[10], cospi40);
            x = _mm_mullo_epi32(u[11], cospi24);
            v[10] = _mm_add_epi32(v[10], x);
            v[10] = _mm_add_epi32(v[10], rnding);
            v[10] = _mm_srai_epi32(v[10], bit);

            v[11] = _mm_mullo_epi32(u[10], cospi24);
            x = _mm_mullo_epi32(u[11], cospi40);
            v[11] = _mm_sub_epi32(v[11], x);
            v[11] = _mm_add_epi32(v[11], rnding);
            v[11] = _mm_srai_epi32(v[11], bit);

            v[12] = _mm_mullo_epi32(u[12], cospim56);
            x = _mm_mullo_epi32(u[13], cospi8);
            v[12] = _mm_add_epi32(v[12], x);
            v[12] = _mm_add_epi32(v[12], rnding);
            v[12] = _mm_srai_epi32(v[12], bit);

            v[13] = _mm_mullo_epi32(u[12], cospi8);
            x = _mm_mullo_epi32(u[13], cospim56);
            v[13] = _mm_sub_epi32(v[13], x);
            v[13] = _mm_add_epi32(v[13], rnding);
            v[13] = _mm_srai_epi32(v[13], bit);

            v[14] = _mm_mullo_epi32(u[14], cospim24);
            x = _mm_mullo_epi32(u[15], cospi40);
            v[14] = _mm_add_epi32(v[14], x);
            v[14] = _mm_add_epi32(v[14], rnding);
            v[14] = _mm_srai_epi32(v[14], bit);

            v[15] = _mm_mullo_epi32(u[14], cospi40);
            x = _mm_mullo_epi32(u[15], cospim24);
            v[15] = _mm_sub_epi32(v[15], x);
            v[15] = _mm_add_epi32(v[15], rnding);
            v[15] = _mm_srai_epi32(v[15], bit);

            // stage 5
            u[0] = _mm_add_epi32(v[0], v[4]);
            u[4] = _mm_sub_epi32(v[0], v[4]);
            u[1] = _mm_add_epi32(v[1], v[5]);
            u[5] = _mm_sub_epi32(v[1], v[5]);
            u[2] = _mm_add_epi32(v[2], v[6]);
            u[6] = _mm_sub_epi32(v[2], v[6]);
            u[3] = _mm_add_epi32(v[3], v[7]);
            u[7] = _mm_sub_epi32(v[3], v[7]);
            u[8] = _mm_add_epi32(v[8], v[12]);
            u[12] = _mm_sub_epi32(v[8], v[12]);
            u[9] = _mm_add_epi32(v[9], v[13]);
            u[13] = _mm_sub_epi32(v[9], v[13]);
            u[10] = _mm_add_epi32(v[10], v[14]);
            u[14] = _mm_sub_epi32(v[10], v[14]);
            u[11] = _mm_add_epi32(v[11], v[15]);
            u[15] = _mm_sub_epi32(v[11], v[15]);

            // stage 6
            v[0] = u[0];
            v[1] = u[1];
            v[2] = u[2];
            v[3] = u[3];

            v[4] = _mm_mullo_epi32(u[4], cospi16);
            x = _mm_mullo_epi32(u[5], cospi48);
            v[4] = _mm_add_epi32(v[4], x);
            v[4] = _mm_add_epi32(v[4], rnding);
            v[4] = _mm_srai_epi32(v[4], bit);

            v[5] = _mm_mullo_epi32(u[4], cospi48);
            x = _mm_mullo_epi32(u[5], cospi16);
            v[5] = _mm_sub_epi32(v[5], x);
            v[5] = _mm_add_epi32(v[5], rnding);
            v[5] = _mm_srai_epi32(v[5], bit);

            v[6] = _mm_mullo_epi32(u[6], cospim48);
            x = _mm_mullo_epi32(u[7], cospi16);
            v[6] = _mm_add_epi32(v[6], x);
            v[6] = _mm_add_epi32(v[6], rnding);
            v[6] = _mm_srai_epi32(v[6], bit);

            v[7] = _mm_mullo_epi32(u[6], cospi16);
            x = _mm_mullo_epi32(u[7], cospim48);
            v[7] = _mm_sub_epi32(v[7], x);
            v[7] = _mm_add_epi32(v[7], rnding);
            v[7] = _mm_srai_epi32(v[7], bit);

            v[8] = u[8];
            v[9] = u[9];
            v[10] = u[10];
            v[11] = u[11];

            v[12] = _mm_mullo_epi32(u[12], cospi16);
            x = _mm_mullo_epi32(u[13], cospi48);
            v[12] = _mm_add_epi32(v[12], x);
            v[12] = _mm_add_epi32(v[12], rnding);
            v[12] = _mm_srai_epi32(v[12], bit);

            v[13] = _mm_mullo_epi32(u[12], cospi48);
            x = _mm_mullo_epi32(u[13], cospi16);
            v[13] = _mm_sub_epi32(v[13], x);
            v[13] = _mm_add_epi32(v[13], rnding);
            v[13] = _mm_srai_epi32(v[13], bit);

            v[14] = _mm_mullo_epi32(u[14], cospim48);
            x = _mm_mullo_epi32(u[15], cospi16);
            v[14] = _mm_add_epi32(v[14], x);
            v[14] = _mm_add_epi32(v[14], rnding);
            v[14] = _mm_srai_epi32(v[14], bit);

            v[15] = _mm_mullo_epi32(u[14], cospi16);
            x = _mm_mullo_epi32(u[15], cospim48);
            v[15] = _mm_sub_epi32(v[15], x);
            v[15] = _mm_add_epi32(v[15], rnding);
            v[15] = _mm_srai_epi32(v[15], bit);

            // stage 7
            u[0] = _mm_add_epi32(v[0], v[2]);
            u[2] = _mm_sub_epi32(v[0], v[2]);
            u[1] = _mm_add_epi32(v[1], v[3]);
            u[3] = _mm_sub_epi32(v[1], v[3]);
            u[4] = _mm_add_epi32(v[4], v[6]);
            u[6] = _mm_sub_epi32(v[4], v[6]);
            u[5] = _mm_add_epi32(v[5], v[7]);
            u[7] = _mm_sub_epi32(v[5], v[7]);
            u[8] = _mm_add_epi32(v[8], v[10]);
            u[10] = _mm_sub_epi32(v[8], v[10]);
            u[9] = _mm_add_epi32(v[9], v[11]);
            u[11] = _mm_sub_epi32(v[9], v[11]);
            u[12] = _mm_add_epi32(v[12], v[14]);
            u[14] = _mm_sub_epi32(v[12], v[14]);
            u[13] = _mm_add_epi32(v[13], v[15]);
            u[15] = _mm_sub_epi32(v[13], v[15]);

            // stage 8
            v[0] = u[0];
            v[1] = u[1];

            y = _mm_mullo_epi32(u[2], cospi32);
            x = _mm_mullo_epi32(u[3], cospi32);
            v[2] = _mm_add_epi32(y, x);
            v[2] = _mm_add_epi32(v[2], rnding);
            v[2] = _mm_srai_epi32(v[2], bit);

            v[3] = _mm_sub_epi32(y, x);
            v[3] = _mm_add_epi32(v[3], rnding);
            v[3] = _mm_srai_epi32(v[3], bit);

            v[4] = u[4];
            v[5] = u[5];

            y = _mm_mullo_epi32(u[6], cospi32);
            x = _mm_mullo_epi32(u[7], cospi32);
            v[6] = _mm_add_epi32(y, x);
            v[6] = _mm_add_epi32(v[6], rnding);
            v[6] = _mm_srai_epi32(v[6], bit);

            v[7] = _mm_sub_epi32(y, x);
            v[7] = _mm_add_epi32(v[7], rnding);
            v[7] = _mm_srai_epi32(v[7], bit);

            v[8] = u[8];
            v[9] = u[9];

            y = _mm_mullo_epi32(u[10], cospi32);
            x = _mm_mullo_epi32(u[11], cospi32);
            v[10] = _mm_add_epi32(y, x);
            v[10] = _mm_add_epi32(v[10], rnding);
            v[10] = _mm_srai_epi32(v[10], bit);

            v[11] = _mm_sub_epi32(y, x);
            v[11] = _mm_add_epi32(v[11], rnding);
            v[11] = _mm_srai_epi32(v[11], bit);

            v[12] = u[12];
            v[13] = u[13];

            y = _mm_mullo_epi32(u[14], cospi32);
            x = _mm_mullo_epi32(u[15], cospi32);
            v[14] = _mm_add_epi32(y, x);
            v[14] = _mm_add_epi32(v[14], rnding);
            v[14] = _mm_srai_epi32(v[14], bit);

            v[15] = _mm_sub_epi32(y, x);
            v[15] = _mm_add_epi32(v[15], rnding);
            v[15] = _mm_srai_epi32(v[15], bit);

            // stage 9
            out[0 * col_num + col] = v[0];
            out[1 * col_num + col] = _mm_sub_epi32(_mm_set1_epi32(0), v[8]);
            out[2 * col_num + col] = v[12];
            out[3 * col_num + col] = _mm_sub_epi32(_mm_set1_epi32(0), v[4]);
            out[4 * col_num + col] = v[6];
            out[5 * col_num + col] = _mm_sub_epi32(_mm_set1_epi32(0), v[14]);
            out[6 * col_num + col] = v[10];
            out[7 * col_num + col] = _mm_sub_epi32(_mm_set1_epi32(0), v[2]);
            out[8 * col_num + col] = v[3];
            out[9 * col_num + col] = _mm_sub_epi32(_mm_set1_epi32(0), v[11]);
            out[10 * col_num + col] = v[15];
            out[11 * col_num + col] = _mm_sub_epi32(_mm_set1_epi32(0), v[7]);
            out[12 * col_num + col] = v[5];
            out[13 * col_num + col] = _mm_sub_epi32(_mm_set1_epi32(0), v[13]);
            out[14 * col_num + col] = v[9];
            out[15 * col_num + col] = _mm_sub_epi32(_mm_set1_epi32(0), v[1]);
        }
    }

    void av1_fwd_txfm2d_16x16_sse4_1(const int16_t *input, int32_t *coeff, int stride, int tx_type, int bd)
    {
        __m128i in[64], out[64];
        const TXFM_1D_CFG *row_cfg = NULL;
        const TXFM_1D_CFG *col_cfg = NULL;

        switch (tx_type) {
        case DCT_DCT:
            row_cfg = &fwd_txfm_1d_row_cfg_dct_16;
            col_cfg = &fwd_txfm_1d_col_cfg_dct_16;
            load_buffer_16x16(input, in, stride, 0, 0, row_cfg->shift[0]);
            fdct16x16_sse4_1(in, out, col_cfg->cos_bit[0]);
            col_txfm_16x16_rounding(out, -row_cfg->shift[1]);
            transpose_16x16(out, in);
            fdct16x16_sse4_1(in, out, row_cfg->cos_bit[0]);
            transpose_16x16(out, in);
            write_buffer_16x16(in, coeff);
            break;
        case ADST_DCT:
            row_cfg = &fwd_txfm_1d_row_cfg_dct_16;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_16;
            load_buffer_16x16(input, in, stride, 0, 0, row_cfg->shift[0]);
            fadst16x16_sse4_1(in, out, col_cfg->cos_bit[0]);
            col_txfm_16x16_rounding(out, -row_cfg->shift[1]);
            transpose_16x16(out, in);
            fdct16x16_sse4_1(in, out, row_cfg->cos_bit[0]);
            transpose_16x16(out, in);
            write_buffer_16x16(in, coeff);
            break;
        case DCT_ADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_16;
            col_cfg = &fwd_txfm_1d_col_cfg_dct_16;
            load_buffer_16x16(input, in, stride, 0, 0, row_cfg->shift[0]);
            fdct16x16_sse4_1(in, out, col_cfg->cos_bit[0]);
            col_txfm_16x16_rounding(out, -row_cfg->shift[1]);
            transpose_16x16(out, in);
            fadst16x16_sse4_1(in, out, row_cfg->cos_bit[0]);
            transpose_16x16(out, in);
            write_buffer_16x16(in, coeff);
            break;
        case ADST_ADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_16;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_16;
            load_buffer_16x16(input, in, stride, 0, 0, row_cfg->shift[0]);
            fadst16x16_sse4_1(in, out, col_cfg->cos_bit[0]);
            col_txfm_16x16_rounding(out, -row_cfg->shift[1]);
            transpose_16x16(out, in);
            fadst16x16_sse4_1(in, out, row_cfg->cos_bit[0]);
            transpose_16x16(out, in);
            write_buffer_16x16(in, coeff);
            break;
#if CONFIG_EXT_TX
        case FLIPADST_DCT:
            row_cfg = &fwd_txfm_1d_row_cfg_dct_16;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_16;
            load_buffer_16x16(input, in, stride, 1, 0, row_cfg->shift[0]);
            fadst16x16_sse4_1(in, out, col_cfg->cos_bit[0]);
            col_txfm_16x16_rounding(out, -row_cfg->shift[1]);
            transpose_16x16(out, in);
            fdct16x16_sse4_1(in, out, row_cfg->cos_bit[0]);
            transpose_16x16(out, in);
            write_buffer_16x16(in, coeff);
            break;
        case DCT_FLIPADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_16;
            col_cfg = &fwd_txfm_1d_col_cfg_dct_16;
            load_buffer_16x16(input, in, stride, 0, 1, row_cfg->shift[0]);
            fdct16x16_sse4_1(in, out, col_cfg->cos_bit[0]);
            col_txfm_16x16_rounding(out, -row_cfg->shift[1]);
            transpose_16x16(out, in);
            fadst16x16_sse4_1(in, out, row_cfg->cos_bit[0]);
            transpose_16x16(out, in);
            write_buffer_16x16(in, coeff);
            break;
        case FLIPADST_FLIPADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_16;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_16;
            load_buffer_16x16(input, in, stride, 1, 1, row_cfg->shift[0]);
            fadst16x16_sse4_1(in, out, col_cfg->cos_bit[0]);
            col_txfm_16x16_rounding(out, -row_cfg->shift[1]);
            transpose_16x16(out, in);
            fadst16x16_sse4_1(in, out, row_cfg->cos_bit[0]);
            transpose_16x16(out, in);
            write_buffer_16x16(in, coeff);
            break;
        case ADST_FLIPADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_16;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_16;
            load_buffer_16x16(input, in, stride, 0, 1, row_cfg->shift[0]);
            fadst16x16_sse4_1(in, out, col_cfg->cos_bit[0]);
            col_txfm_16x16_rounding(out, -row_cfg->shift[1]);
            transpose_16x16(out, in);
            fadst16x16_sse4_1(in, out, row_cfg->cos_bit[0]);
            transpose_16x16(out, in);
            write_buffer_16x16(in, coeff);
            break;
        case FLIPADST_ADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_16;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_16;
            load_buffer_16x16(input, in, stride, 1, 0, row_cfg->shift[0]);
            fadst16x16_sse4_1(in, out, col_cfg->cos_bit[0]);
            col_txfm_16x16_rounding(out, -row_cfg->shift[1]);
            transpose_16x16(out, in);
            fadst16x16_sse4_1(in, out, row_cfg->cos_bit[0]);
            transpose_16x16(out, in);
            write_buffer_16x16(in, coeff);
            break;
#endif  // CONFIG_EXT_TX
        default: assert(0);
        }
        (void)bd;
    }

#if (defined(__GNUC__) && __GNUC__) || defined(__SUNPRO_C)
#define DECLARE_ALIGNED(n, typ, val) typ val __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define DECLARE_ALIGNED(n, typ, val) __declspec(align(n)) typ val
#else
#warning No alignment directives known for this compiler.
#define DECLARE_ALIGNED(n, typ, val) typ val
#endif

    // block transform size
    typedef enum ATTRIBUTE_PACKED {
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

#define TX_SIZE_W_MIN 4
#define TX_SIZE_H_MIN 4

    static INLINE void get_flip_cfg(TX_TYPE tx_type, int *ud_flip, int *lr_flip) {
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

    static void set_flip_cfg(TX_TYPE tx_type, TXFM_2D_FLIP_CFG *cfg) {
        get_flip_cfg(tx_type, &cfg->ud_flip, &cfg->lr_flip);
    }

    // Transform block width in log2
    static const int tx_size_wide_log2[TX_SIZES_ALL] = {
        2, 3, 4, 5, 6, 2, 3, 3, 4, 4, 5, 5, 6, 2, 4, 3, 5, 4, 6,
    };

    // Transform block height in log2
    static const int tx_size_high_log2[TX_SIZES_ALL] = {
        2, 3, 4, 5, 6, 3, 2, 4, 3, 5, 4, 6, 5, 4, 2, 5, 3, 6, 4,
    };

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
    const int8_t fwd_cos_bit_col[MAX_TXWH_IDX /*txw_idx*/][MAX_TXWH_IDX /*txh_idx*/] = {
        { 13, 13, 13, 0, 0 },
        { 13, 13, 13, 12, 0 },
        { 13, 13, 13, 12, 13 },
        { 0, 13, 13, 12, 13 },
        { 0, 0, 13, 12, 13 }
    };

    const int8_t fwd_cos_bit_row[MAX_TXWH_IDX /*txw_idx*/] [MAX_TXWH_IDX /*txh_idx*/] = {
        { 13, 13, 12, 0, 0 },
        { 13, 13, 13, 12, 0 },
        { 13, 13, 12, 13, 12 },
        { 0, 12, 13, 12, 11 },
        { 0, 0, 12, 11, 10 }
    };

    const TXFM_TYPE av1_txfm_type_ls[5][TX_TYPES_1D] = {
        { TXFM_TYPE_DCT4, TXFM_TYPE_ADST4, TXFM_TYPE_ADST4, TXFM_TYPE_IDENTITY4 },
        { TXFM_TYPE_DCT8, TXFM_TYPE_ADST8, TXFM_TYPE_ADST8, TXFM_TYPE_IDENTITY8 },
        { TXFM_TYPE_DCT16, TXFM_TYPE_ADST16, TXFM_TYPE_ADST16, TXFM_TYPE_IDENTITY16 },
        { TXFM_TYPE_DCT32, TXFM_TYPE_ADST32, TXFM_TYPE_ADST32, TXFM_TYPE_IDENTITY32 },
        { TXFM_TYPE_DCT64, TXFM_TYPE_INVALID, TXFM_TYPE_INVALID, TXFM_TYPE_IDENTITY64 }
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

    static INLINE int get_txh_idx(uint8_t tx_size) {
        return tx_size_high_log2[tx_size] - tx_size_high_log2[0];
    }

    #define av1_zero(dest) memset(&(dest), 0, sizeof(dest))

    const int8_t fdct4_range_mult2[4] = { 0, 2, 3, 3 };
    const int8_t fdct8_range_mult2[6] = { 0, 2, 4, 5, 5, 5 };
    const int8_t fdct16_range_mult2[8] = { 0, 2, 4, 6, 7, 7, 7, 7 };
    const int8_t fdct32_range_mult2[10] = { 0, 2, 4, 6, 8, 9, 9, 9, 9, 9 };
    const int8_t fdct64_range_mult2[12] = { 0,  2,  4,  6,  8,  10,
        11, 11, 11, 11, 11, 11 };

    const int8_t fadst4_range_mult2[7] = { 0, 2, 4, 3, 3, 3, 3 };
    const int8_t fadst8_range_mult2[8] = { 0, 0, 1, 3, 3, 5, 5, 5 };
    const int8_t fadst16_range_mult2[10] = { 0, 0, 1, 3, 3, 5, 5, 7, 7, 7 };
    const int8_t fadst32_range_mult2[12] = { 0, 0, 1, 3, 3, 5, 5, 7, 7, 9, 9, 9 };

    const int8_t max_fwd_range_mult2_col[5] = { 3, 5, 7, 9, 11 };

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

    static INLINE void set_fwd_txfm_non_scale_range(TXFM_2D_FLIP_CFG *cfg) {
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

    void av1_get_fwd_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size, TXFM_2D_FLIP_CFG *cfg)
    {
        assert(0);
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

    static INLINE __m128i round_shift_32_sse4_1(__m128i vec, int bit) {
        __m128i tmp, round;
        round = _mm_set1_epi32(1 << (bit - 1));
        tmp = _mm_add_epi32(vec, round);
        return _mm_srai_epi32(tmp, bit);
    }

    // out0 = in0*w0 + in1*w1
    // out1 = -in1*w0 + in0*w1
#define btf_32_sse4_1_type0(w0, w1, in0, in1, out0, out1, bit) \
    do {                                                         \
    __m128i ww0, ww1, in0_w0, in1_w1, in0_w1, in1_w0;          \
    ww0 = _mm_set1_epi32(w0);                                  \
    ww1 = _mm_set1_epi32(w1);                                  \
    in0_w0 = _mm_mullo_epi32(in0, ww0);                        \
    in1_w1 = _mm_mullo_epi32(in1, ww1);                        \
    out0 = _mm_add_epi32(in0_w0, in1_w1);                      \
    out0 = round_shift_32_sse4_1(out0, bit);                   \
    in0_w1 = _mm_mullo_epi32(in0, ww1);                        \
    in1_w0 = _mm_mullo_epi32(in1, ww0);                        \
    out1 = _mm_sub_epi32(in0_w1, in1_w0);                      \
    out1 = round_shift_32_sse4_1(out1, bit);                   \
    } while (0)

    // out0 = in0*w0 + in1*w1
    // out1 = in1*w0 - in0*w1
#define btf_32_sse4_1_type1(w0, w1, in0, in1, out0, out1, bit) \
    do {                                                         \
    __m128i ww0, ww1, in0_w0, in1_w1, in0_w1, in1_w0;          \
    ww0 = _mm_set1_epi32(w0);                                  \
    ww1 = _mm_set1_epi32(w1);                                  \
    in0_w0 = _mm_mullo_epi32(in0, ww0);                        \
    in1_w1 = _mm_mullo_epi32(in1, ww1);                        \
    out0 = _mm_add_epi32(in0_w0, in1_w1);                      \
    out0 = round_shift_32_sse4_1(out0, bit);                   \
    in0_w1 = _mm_mullo_epi32(in0, ww1);                        \
    in1_w0 = _mm_mullo_epi32(in1, ww0);                        \
    out1 = _mm_sub_epi32(in1_w0, in0_w1);                      \
    out1 = round_shift_32_sse4_1(out1, bit);                   \
    } while (0)

    void av1_fdct32_new_sse4_1(const __m128i *input, __m128i *output, const int8_t cos_bit, const int8_t *stage_range)
    {
        const int txfm_size = 32;
        const int num_per_128 = 4;
        const int32_t *cospi;
        __m128i buf0[32];
        __m128i buf1[32];
        int col_num = txfm_size / num_per_128;
        int col;
        (void)stage_range;
        for (col = 0; col < col_num; col++) {
            // stage 0;
            int32_t stage_idx = 0;
            int j;
            for (j = 0; j < 32; ++j) {
                buf0[j] = input[j * col_num + col];
            }

            // stage 1
            stage_idx++;
            buf1[0] = _mm_add_epi32(buf0[0], buf0[31]);
            buf1[31] = _mm_sub_epi32(buf0[0], buf0[31]);
            buf1[1] = _mm_add_epi32(buf0[1], buf0[30]);
            buf1[30] = _mm_sub_epi32(buf0[1], buf0[30]);
            buf1[2] = _mm_add_epi32(buf0[2], buf0[29]);
            buf1[29] = _mm_sub_epi32(buf0[2], buf0[29]);
            buf1[3] = _mm_add_epi32(buf0[3], buf0[28]);
            buf1[28] = _mm_sub_epi32(buf0[3], buf0[28]);
            buf1[4] = _mm_add_epi32(buf0[4], buf0[27]);
            buf1[27] = _mm_sub_epi32(buf0[4], buf0[27]);
            buf1[5] = _mm_add_epi32(buf0[5], buf0[26]);
            buf1[26] = _mm_sub_epi32(buf0[5], buf0[26]);
            buf1[6] = _mm_add_epi32(buf0[6], buf0[25]);
            buf1[25] = _mm_sub_epi32(buf0[6], buf0[25]);
            buf1[7] = _mm_add_epi32(buf0[7], buf0[24]);
            buf1[24] = _mm_sub_epi32(buf0[7], buf0[24]);
            buf1[8] = _mm_add_epi32(buf0[8], buf0[23]);
            buf1[23] = _mm_sub_epi32(buf0[8], buf0[23]);
            buf1[9] = _mm_add_epi32(buf0[9], buf0[22]);
            buf1[22] = _mm_sub_epi32(buf0[9], buf0[22]);
            buf1[10] = _mm_add_epi32(buf0[10], buf0[21]);
            buf1[21] = _mm_sub_epi32(buf0[10], buf0[21]);
            buf1[11] = _mm_add_epi32(buf0[11], buf0[20]);
            buf1[20] = _mm_sub_epi32(buf0[11], buf0[20]);
            buf1[12] = _mm_add_epi32(buf0[12], buf0[19]);
            buf1[19] = _mm_sub_epi32(buf0[12], buf0[19]);
            buf1[13] = _mm_add_epi32(buf0[13], buf0[18]);
            buf1[18] = _mm_sub_epi32(buf0[13], buf0[18]);
            buf1[14] = _mm_add_epi32(buf0[14], buf0[17]);
            buf1[17] = _mm_sub_epi32(buf0[14], buf0[17]);
            buf1[15] = _mm_add_epi32(buf0[15], buf0[16]);
            buf1[16] = _mm_sub_epi32(buf0[15], buf0[16]);

            // stage 2
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            buf0[0] = _mm_add_epi32(buf1[0], buf1[15]);
            buf0[15] = _mm_sub_epi32(buf1[0], buf1[15]);
            buf0[1] = _mm_add_epi32(buf1[1], buf1[14]);
            buf0[14] = _mm_sub_epi32(buf1[1], buf1[14]);
            buf0[2] = _mm_add_epi32(buf1[2], buf1[13]);
            buf0[13] = _mm_sub_epi32(buf1[2], buf1[13]);
            buf0[3] = _mm_add_epi32(buf1[3], buf1[12]);
            buf0[12] = _mm_sub_epi32(buf1[3], buf1[12]);
            buf0[4] = _mm_add_epi32(buf1[4], buf1[11]);
            buf0[11] = _mm_sub_epi32(buf1[4], buf1[11]);
            buf0[5] = _mm_add_epi32(buf1[5], buf1[10]);
            buf0[10] = _mm_sub_epi32(buf1[5], buf1[10]);
            buf0[6] = _mm_add_epi32(buf1[6], buf1[9]);
            buf0[9] = _mm_sub_epi32(buf1[6], buf1[9]);
            buf0[7] = _mm_add_epi32(buf1[7], buf1[8]);
            buf0[8] = _mm_sub_epi32(buf1[7], buf1[8]);
            buf0[16] = buf1[16];
            buf0[17] = buf1[17];
            buf0[18] = buf1[18];
            buf0[19] = buf1[19];
            btf_32_sse4_1_type0(-cospi[32], cospi[32], buf1[20], buf1[27], buf0[20],
                buf0[27], cos_bit);
            btf_32_sse4_1_type0(-cospi[32], cospi[32], buf1[21], buf1[26], buf0[21],
                buf0[26], cos_bit);
            btf_32_sse4_1_type0(-cospi[32], cospi[32], buf1[22], buf1[25], buf0[22],
                buf0[25], cos_bit);
            btf_32_sse4_1_type0(-cospi[32], cospi[32], buf1[23], buf1[24], buf0[23],
                buf0[24], cos_bit);
            buf0[28] = buf1[28];
            buf0[29] = buf1[29];
            buf0[30] = buf1[30];
            buf0[31] = buf1[31];

            // stage 3
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            buf1[0] = _mm_add_epi32(buf0[0], buf0[7]);
            buf1[7] = _mm_sub_epi32(buf0[0], buf0[7]);
            buf1[1] = _mm_add_epi32(buf0[1], buf0[6]);
            buf1[6] = _mm_sub_epi32(buf0[1], buf0[6]);
            buf1[2] = _mm_add_epi32(buf0[2], buf0[5]);
            buf1[5] = _mm_sub_epi32(buf0[2], buf0[5]);
            buf1[3] = _mm_add_epi32(buf0[3], buf0[4]);
            buf1[4] = _mm_sub_epi32(buf0[3], buf0[4]);
            buf1[8] = buf0[8];
            buf1[9] = buf0[9];
            btf_32_sse4_1_type0(-cospi[32], cospi[32], buf0[10], buf0[13], buf1[10],
                buf1[13], cos_bit);
            btf_32_sse4_1_type0(-cospi[32], cospi[32], buf0[11], buf0[12], buf1[11],
                buf1[12], cos_bit);
            buf1[14] = buf0[14];
            buf1[15] = buf0[15];
            buf1[16] = _mm_add_epi32(buf0[16], buf0[23]);
            buf1[23] = _mm_sub_epi32(buf0[16], buf0[23]);
            buf1[17] = _mm_add_epi32(buf0[17], buf0[22]);
            buf1[22] = _mm_sub_epi32(buf0[17], buf0[22]);
            buf1[18] = _mm_add_epi32(buf0[18], buf0[21]);
            buf1[21] = _mm_sub_epi32(buf0[18], buf0[21]);
            buf1[19] = _mm_add_epi32(buf0[19], buf0[20]);
            buf1[20] = _mm_sub_epi32(buf0[19], buf0[20]);
            buf1[24] = _mm_sub_epi32(buf0[31], buf0[24]);
            buf1[31] = _mm_add_epi32(buf0[31], buf0[24]);
            buf1[25] = _mm_sub_epi32(buf0[30], buf0[25]);
            buf1[30] = _mm_add_epi32(buf0[30], buf0[25]);
            buf1[26] = _mm_sub_epi32(buf0[29], buf0[26]);
            buf1[29] = _mm_add_epi32(buf0[29], buf0[26]);
            buf1[27] = _mm_sub_epi32(buf0[28], buf0[27]);
            buf1[28] = _mm_add_epi32(buf0[28], buf0[27]);

            // stage 4
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            buf0[0] = _mm_add_epi32(buf1[0], buf1[3]);
            buf0[3] = _mm_sub_epi32(buf1[0], buf1[3]);
            buf0[1] = _mm_add_epi32(buf1[1], buf1[2]);
            buf0[2] = _mm_sub_epi32(buf1[1], buf1[2]);
            buf0[4] = buf1[4];
            btf_32_sse4_1_type0(-cospi[32], cospi[32], buf1[5], buf1[6], buf0[5],
                buf0[6], cos_bit);
            buf0[7] = buf1[7];
            buf0[8] = _mm_add_epi32(buf1[8], buf1[11]);
            buf0[11] = _mm_sub_epi32(buf1[8], buf1[11]);
            buf0[9] = _mm_add_epi32(buf1[9], buf1[10]);
            buf0[10] = _mm_sub_epi32(buf1[9], buf1[10]);
            buf0[12] = _mm_sub_epi32(buf1[15], buf1[12]);
            buf0[15] = _mm_add_epi32(buf1[15], buf1[12]);
            buf0[13] = _mm_sub_epi32(buf1[14], buf1[13]);
            buf0[14] = _mm_add_epi32(buf1[14], buf1[13]);
            buf0[16] = buf1[16];
            buf0[17] = buf1[17];
            btf_32_sse4_1_type0(-cospi[16], cospi[48], buf1[18], buf1[29], buf0[18],
                buf0[29], cos_bit);
            btf_32_sse4_1_type0(-cospi[16], cospi[48], buf1[19], buf1[28], buf0[19],
                buf0[28], cos_bit);
            btf_32_sse4_1_type0(-cospi[48], -cospi[16], buf1[20], buf1[27], buf0[20],
                buf0[27], cos_bit);
            btf_32_sse4_1_type0(-cospi[48], -cospi[16], buf1[21], buf1[26], buf0[21],
                buf0[26], cos_bit);
            buf0[22] = buf1[22];
            buf0[23] = buf1[23];
            buf0[24] = buf1[24];
            buf0[25] = buf1[25];
            buf0[30] = buf1[30];
            buf0[31] = buf1[31];

            // stage 5
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            btf_32_sse4_1_type0(cospi[32], cospi[32], buf0[0], buf0[1], buf1[0],
                buf1[1], cos_bit);
            btf_32_sse4_1_type1(cospi[48], cospi[16], buf0[2], buf0[3], buf1[2],
                buf1[3], cos_bit);
            buf1[4] = _mm_add_epi32(buf0[4], buf0[5]);
            buf1[5] = _mm_sub_epi32(buf0[4], buf0[5]);
            buf1[6] = _mm_sub_epi32(buf0[7], buf0[6]);
            buf1[7] = _mm_add_epi32(buf0[7], buf0[6]);
            buf1[8] = buf0[8];
            btf_32_sse4_1_type0(-cospi[16], cospi[48], buf0[9], buf0[14], buf1[9],
                buf1[14], cos_bit);
            btf_32_sse4_1_type0(-cospi[48], -cospi[16], buf0[10], buf0[13], buf1[10],
                buf1[13], cos_bit);
            buf1[11] = buf0[11];
            buf1[12] = buf0[12];
            buf1[15] = buf0[15];
            buf1[16] = _mm_add_epi32(buf0[16], buf0[19]);
            buf1[19] = _mm_sub_epi32(buf0[16], buf0[19]);
            buf1[17] = _mm_add_epi32(buf0[17], buf0[18]);
            buf1[18] = _mm_sub_epi32(buf0[17], buf0[18]);
            buf1[20] = _mm_sub_epi32(buf0[23], buf0[20]);
            buf1[23] = _mm_add_epi32(buf0[23], buf0[20]);
            buf1[21] = _mm_sub_epi32(buf0[22], buf0[21]);
            buf1[22] = _mm_add_epi32(buf0[22], buf0[21]);
            buf1[24] = _mm_add_epi32(buf0[24], buf0[27]);
            buf1[27] = _mm_sub_epi32(buf0[24], buf0[27]);
            buf1[25] = _mm_add_epi32(buf0[25], buf0[26]);
            buf1[26] = _mm_sub_epi32(buf0[25], buf0[26]);
            buf1[28] = _mm_sub_epi32(buf0[31], buf0[28]);
            buf1[31] = _mm_add_epi32(buf0[31], buf0[28]);
            buf1[29] = _mm_sub_epi32(buf0[30], buf0[29]);
            buf1[30] = _mm_add_epi32(buf0[30], buf0[29]);

            // stage 6
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            buf0[0] = buf1[0];
            buf0[1] = buf1[1];
            buf0[2] = buf1[2];
            buf0[3] = buf1[3];
            btf_32_sse4_1_type1(cospi[56], cospi[8], buf1[4], buf1[7], buf0[4], buf0[7],
                cos_bit);
            btf_32_sse4_1_type1(cospi[24], cospi[40], buf1[5], buf1[6], buf0[5],
                buf0[6], cos_bit);
            buf0[8] = _mm_add_epi32(buf1[8], buf1[9]);
            buf0[9] = _mm_sub_epi32(buf1[8], buf1[9]);
            buf0[10] = _mm_sub_epi32(buf1[11], buf1[10]);
            buf0[11] = _mm_add_epi32(buf1[11], buf1[10]);
            buf0[12] = _mm_add_epi32(buf1[12], buf1[13]);
            buf0[13] = _mm_sub_epi32(buf1[12], buf1[13]);
            buf0[14] = _mm_sub_epi32(buf1[15], buf1[14]);
            buf0[15] = _mm_add_epi32(buf1[15], buf1[14]);
            buf0[16] = buf1[16];
            btf_32_sse4_1_type0(-cospi[8], cospi[56], buf1[17], buf1[30], buf0[17],
                buf0[30], cos_bit);
            btf_32_sse4_1_type0(-cospi[56], -cospi[8], buf1[18], buf1[29], buf0[18],
                buf0[29], cos_bit);
            buf0[19] = buf1[19];
            buf0[20] = buf1[20];
            btf_32_sse4_1_type0(-cospi[40], cospi[24], buf1[21], buf1[26], buf0[21],
                buf0[26], cos_bit);
            btf_32_sse4_1_type0(-cospi[24], -cospi[40], buf1[22], buf1[25], buf0[22],
                buf0[25], cos_bit);
            buf0[23] = buf1[23];
            buf0[24] = buf1[24];
            buf0[27] = buf1[27];
            buf0[28] = buf1[28];
            buf0[31] = buf1[31];

            // stage 7
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            buf1[0] = buf0[0];
            buf1[1] = buf0[1];
            buf1[2] = buf0[2];
            buf1[3] = buf0[3];
            buf1[4] = buf0[4];
            buf1[5] = buf0[5];
            buf1[6] = buf0[6];
            buf1[7] = buf0[7];
            btf_32_sse4_1_type1(cospi[60], cospi[4], buf0[8], buf0[15], buf1[8],
                buf1[15], cos_bit);
            btf_32_sse4_1_type1(cospi[28], cospi[36], buf0[9], buf0[14], buf1[9],
                buf1[14], cos_bit);
            btf_32_sse4_1_type1(cospi[44], cospi[20], buf0[10], buf0[13], buf1[10],
                buf1[13], cos_bit);
            btf_32_sse4_1_type1(cospi[12], cospi[52], buf0[11], buf0[12], buf1[11],
                buf1[12], cos_bit);
            buf1[16] = _mm_add_epi32(buf0[16], buf0[17]);
            buf1[17] = _mm_sub_epi32(buf0[16], buf0[17]);
            buf1[18] = _mm_sub_epi32(buf0[19], buf0[18]);
            buf1[19] = _mm_add_epi32(buf0[19], buf0[18]);
            buf1[20] = _mm_add_epi32(buf0[20], buf0[21]);
            buf1[21] = _mm_sub_epi32(buf0[20], buf0[21]);
            buf1[22] = _mm_sub_epi32(buf0[23], buf0[22]);
            buf1[23] = _mm_add_epi32(buf0[23], buf0[22]);
            buf1[24] = _mm_add_epi32(buf0[24], buf0[25]);
            buf1[25] = _mm_sub_epi32(buf0[24], buf0[25]);
            buf1[26] = _mm_sub_epi32(buf0[27], buf0[26]);
            buf1[27] = _mm_add_epi32(buf0[27], buf0[26]);
            buf1[28] = _mm_add_epi32(buf0[28], buf0[29]);
            buf1[29] = _mm_sub_epi32(buf0[28], buf0[29]);
            buf1[30] = _mm_sub_epi32(buf0[31], buf0[30]);
            buf1[31] = _mm_add_epi32(buf0[31], buf0[30]);

            // stage 8
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            buf0[0] = buf1[0];
            buf0[1] = buf1[1];
            buf0[2] = buf1[2];
            buf0[3] = buf1[3];
            buf0[4] = buf1[4];
            buf0[5] = buf1[5];
            buf0[6] = buf1[6];
            buf0[7] = buf1[7];
            buf0[8] = buf1[8];
            buf0[9] = buf1[9];
            buf0[10] = buf1[10];
            buf0[11] = buf1[11];
            buf0[12] = buf1[12];
            buf0[13] = buf1[13];
            buf0[14] = buf1[14];
            buf0[15] = buf1[15];
            btf_32_sse4_1_type1(cospi[62], cospi[2], buf1[16], buf1[31], buf0[16],
                buf0[31], cos_bit);
            btf_32_sse4_1_type1(cospi[30], cospi[34], buf1[17], buf1[30], buf0[17],
                buf0[30], cos_bit);
            btf_32_sse4_1_type1(cospi[46], cospi[18], buf1[18], buf1[29], buf0[18],
                buf0[29], cos_bit);
            btf_32_sse4_1_type1(cospi[14], cospi[50], buf1[19], buf1[28], buf0[19],
                buf0[28], cos_bit);
            btf_32_sse4_1_type1(cospi[54], cospi[10], buf1[20], buf1[27], buf0[20],
                buf0[27], cos_bit);
            btf_32_sse4_1_type1(cospi[22], cospi[42], buf1[21], buf1[26], buf0[21],
                buf0[26], cos_bit);
            btf_32_sse4_1_type1(cospi[38], cospi[26], buf1[22], buf1[25], buf0[22],
                buf0[25], cos_bit);
            btf_32_sse4_1_type1(cospi[6], cospi[58], buf1[23], buf1[24], buf0[23],
                buf0[24], cos_bit);

            // stage 9
            stage_idx++;
            buf1[0] = buf0[0];
            buf1[1] = buf0[16];
            buf1[2] = buf0[8];
            buf1[3] = buf0[24];
            buf1[4] = buf0[4];
            buf1[5] = buf0[20];
            buf1[6] = buf0[12];
            buf1[7] = buf0[28];
            buf1[8] = buf0[2];
            buf1[9] = buf0[18];
            buf1[10] = buf0[10];
            buf1[11] = buf0[26];
            buf1[12] = buf0[6];
            buf1[13] = buf0[22];
            buf1[14] = buf0[14];
            buf1[15] = buf0[30];
            buf1[16] = buf0[1];
            buf1[17] = buf0[17];
            buf1[18] = buf0[9];
            buf1[19] = buf0[25];
            buf1[20] = buf0[5];
            buf1[21] = buf0[21];
            buf1[22] = buf0[13];
            buf1[23] = buf0[29];
            buf1[24] = buf0[3];
            buf1[25] = buf0[19];
            buf1[26] = buf0[11];
            buf1[27] = buf0[27];
            buf1[28] = buf0[7];
            buf1[29] = buf0[23];
            buf1[30] = buf0[15];
            buf1[31] = buf0[31];

            for (j = 0; j < 32; ++j) {
                output[j * col_num + col] = buf1[j];
            }
        }
    }

    void av1_fadst32_new_sse4_1(const __m128i *input, __m128i *output, const int8_t cos_bit, const int8_t *stage_range)
    {
        const int txfm_size = 32;
        const int num_per_128 = 4;
        const int32_t *cospi;
        __m128i buf0[32];
        __m128i buf1[32];
        int col_num = txfm_size / num_per_128;
        int col;
        (void)stage_range;
        for (col = 0; col < col_num; col++) {
            // stage 0;
            int32_t stage_idx = 0;
            int j;
            for (j = 0; j < 32; ++j) {
                buf0[j] = input[j * col_num + col];
            }

            // stage 1
            stage_idx++;
            buf1[0] = buf0[31];
            buf1[1] = buf0[0];
            buf1[2] = buf0[29];
            buf1[3] = buf0[2];
            buf1[4] = buf0[27];
            buf1[5] = buf0[4];
            buf1[6] = buf0[25];
            buf1[7] = buf0[6];
            buf1[8] = buf0[23];
            buf1[9] = buf0[8];
            buf1[10] = buf0[21];
            buf1[11] = buf0[10];
            buf1[12] = buf0[19];
            buf1[13] = buf0[12];
            buf1[14] = buf0[17];
            buf1[15] = buf0[14];
            buf1[16] = buf0[15];
            buf1[17] = buf0[16];
            buf1[18] = buf0[13];
            buf1[19] = buf0[18];
            buf1[20] = buf0[11];
            buf1[21] = buf0[20];
            buf1[22] = buf0[9];
            buf1[23] = buf0[22];
            buf1[24] = buf0[7];
            buf1[25] = buf0[24];
            buf1[26] = buf0[5];
            buf1[27] = buf0[26];
            buf1[28] = buf0[3];
            buf1[29] = buf0[28];
            buf1[30] = buf0[1];
            buf1[31] = buf0[30];

            // stage 2
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            btf_32_sse4_1_type0(cospi[1], cospi[63], buf1[0], buf1[1], buf0[0], buf0[1],
                cos_bit);
            btf_32_sse4_1_type0(cospi[5], cospi[59], buf1[2], buf1[3], buf0[2], buf0[3],
                cos_bit);
            btf_32_sse4_1_type0(cospi[9], cospi[55], buf1[4], buf1[5], buf0[4], buf0[5],
                cos_bit);
            btf_32_sse4_1_type0(cospi[13], cospi[51], buf1[6], buf1[7], buf0[6],
                buf0[7], cos_bit);
            btf_32_sse4_1_type0(cospi[17], cospi[47], buf1[8], buf1[9], buf0[8],
                buf0[9], cos_bit);
            btf_32_sse4_1_type0(cospi[21], cospi[43], buf1[10], buf1[11], buf0[10],
                buf0[11], cos_bit);
            btf_32_sse4_1_type0(cospi[25], cospi[39], buf1[12], buf1[13], buf0[12],
                buf0[13], cos_bit);
            btf_32_sse4_1_type0(cospi[29], cospi[35], buf1[14], buf1[15], buf0[14],
                buf0[15], cos_bit);
            btf_32_sse4_1_type0(cospi[33], cospi[31], buf1[16], buf1[17], buf0[16],
                buf0[17], cos_bit);
            btf_32_sse4_1_type0(cospi[37], cospi[27], buf1[18], buf1[19], buf0[18],
                buf0[19], cos_bit);
            btf_32_sse4_1_type0(cospi[41], cospi[23], buf1[20], buf1[21], buf0[20],
                buf0[21], cos_bit);
            btf_32_sse4_1_type0(cospi[45], cospi[19], buf1[22], buf1[23], buf0[22],
                buf0[23], cos_bit);
            btf_32_sse4_1_type0(cospi[49], cospi[15], buf1[24], buf1[25], buf0[24],
                buf0[25], cos_bit);
            btf_32_sse4_1_type0(cospi[53], cospi[11], buf1[26], buf1[27], buf0[26],
                buf0[27], cos_bit);
            btf_32_sse4_1_type0(cospi[57], cospi[7], buf1[28], buf1[29], buf0[28],
                buf0[29], cos_bit);
            btf_32_sse4_1_type0(cospi[61], cospi[3], buf1[30], buf1[31], buf0[30],
                buf0[31], cos_bit);

            // stage 3
            stage_idx++;
            buf1[0] = _mm_add_epi32(buf0[0], buf0[16]);
            buf1[16] = _mm_sub_epi32(buf0[0], buf0[16]);
            buf1[1] = _mm_add_epi32(buf0[1], buf0[17]);
            buf1[17] = _mm_sub_epi32(buf0[1], buf0[17]);
            buf1[2] = _mm_add_epi32(buf0[2], buf0[18]);
            buf1[18] = _mm_sub_epi32(buf0[2], buf0[18]);
            buf1[3] = _mm_add_epi32(buf0[3], buf0[19]);
            buf1[19] = _mm_sub_epi32(buf0[3], buf0[19]);
            buf1[4] = _mm_add_epi32(buf0[4], buf0[20]);
            buf1[20] = _mm_sub_epi32(buf0[4], buf0[20]);
            buf1[5] = _mm_add_epi32(buf0[5], buf0[21]);
            buf1[21] = _mm_sub_epi32(buf0[5], buf0[21]);
            buf1[6] = _mm_add_epi32(buf0[6], buf0[22]);
            buf1[22] = _mm_sub_epi32(buf0[6], buf0[22]);
            buf1[7] = _mm_add_epi32(buf0[7], buf0[23]);
            buf1[23] = _mm_sub_epi32(buf0[7], buf0[23]);
            buf1[8] = _mm_add_epi32(buf0[8], buf0[24]);
            buf1[24] = _mm_sub_epi32(buf0[8], buf0[24]);
            buf1[9] = _mm_add_epi32(buf0[9], buf0[25]);
            buf1[25] = _mm_sub_epi32(buf0[9], buf0[25]);
            buf1[10] = _mm_add_epi32(buf0[10], buf0[26]);
            buf1[26] = _mm_sub_epi32(buf0[10], buf0[26]);
            buf1[11] = _mm_add_epi32(buf0[11], buf0[27]);
            buf1[27] = _mm_sub_epi32(buf0[11], buf0[27]);
            buf1[12] = _mm_add_epi32(buf0[12], buf0[28]);
            buf1[28] = _mm_sub_epi32(buf0[12], buf0[28]);
            buf1[13] = _mm_add_epi32(buf0[13], buf0[29]);
            buf1[29] = _mm_sub_epi32(buf0[13], buf0[29]);
            buf1[14] = _mm_add_epi32(buf0[14], buf0[30]);
            buf1[30] = _mm_sub_epi32(buf0[14], buf0[30]);
            buf1[15] = _mm_add_epi32(buf0[15], buf0[31]);
            buf1[31] = _mm_sub_epi32(buf0[15], buf0[31]);

            // stage 4
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            buf0[0] = buf1[0];
            buf0[1] = buf1[1];
            buf0[2] = buf1[2];
            buf0[3] = buf1[3];
            buf0[4] = buf1[4];
            buf0[5] = buf1[5];
            buf0[6] = buf1[6];
            buf0[7] = buf1[7];
            buf0[8] = buf1[8];
            buf0[9] = buf1[9];
            buf0[10] = buf1[10];
            buf0[11] = buf1[11];
            buf0[12] = buf1[12];
            buf0[13] = buf1[13];
            buf0[14] = buf1[14];
            buf0[15] = buf1[15];
            btf_32_sse4_1_type0(cospi[4], cospi[60], buf1[16], buf1[17], buf0[16],
                buf0[17], cos_bit);
            btf_32_sse4_1_type0(cospi[20], cospi[44], buf1[18], buf1[19], buf0[18],
                buf0[19], cos_bit);
            btf_32_sse4_1_type0(cospi[36], cospi[28], buf1[20], buf1[21], buf0[20],
                buf0[21], cos_bit);
            btf_32_sse4_1_type0(cospi[52], cospi[12], buf1[22], buf1[23], buf0[22],
                buf0[23], cos_bit);
            btf_32_sse4_1_type0(-cospi[60], cospi[4], buf1[24], buf1[25], buf0[24],
                buf0[25], cos_bit);
            btf_32_sse4_1_type0(-cospi[44], cospi[20], buf1[26], buf1[27], buf0[26],
                buf0[27], cos_bit);
            btf_32_sse4_1_type0(-cospi[28], cospi[36], buf1[28], buf1[29], buf0[28],
                buf0[29], cos_bit);
            btf_32_sse4_1_type0(-cospi[12], cospi[52], buf1[30], buf1[31], buf0[30],
                buf0[31], cos_bit);

            // stage 5
            stage_idx++;
            buf1[0] = _mm_add_epi32(buf0[0], buf0[8]);
            buf1[8] = _mm_sub_epi32(buf0[0], buf0[8]);
            buf1[1] = _mm_add_epi32(buf0[1], buf0[9]);
            buf1[9] = _mm_sub_epi32(buf0[1], buf0[9]);
            buf1[2] = _mm_add_epi32(buf0[2], buf0[10]);
            buf1[10] = _mm_sub_epi32(buf0[2], buf0[10]);
            buf1[3] = _mm_add_epi32(buf0[3], buf0[11]);
            buf1[11] = _mm_sub_epi32(buf0[3], buf0[11]);
            buf1[4] = _mm_add_epi32(buf0[4], buf0[12]);
            buf1[12] = _mm_sub_epi32(buf0[4], buf0[12]);
            buf1[5] = _mm_add_epi32(buf0[5], buf0[13]);
            buf1[13] = _mm_sub_epi32(buf0[5], buf0[13]);
            buf1[6] = _mm_add_epi32(buf0[6], buf0[14]);
            buf1[14] = _mm_sub_epi32(buf0[6], buf0[14]);
            buf1[7] = _mm_add_epi32(buf0[7], buf0[15]);
            buf1[15] = _mm_sub_epi32(buf0[7], buf0[15]);
            buf1[16] = _mm_add_epi32(buf0[16], buf0[24]);
            buf1[24] = _mm_sub_epi32(buf0[16], buf0[24]);
            buf1[17] = _mm_add_epi32(buf0[17], buf0[25]);
            buf1[25] = _mm_sub_epi32(buf0[17], buf0[25]);
            buf1[18] = _mm_add_epi32(buf0[18], buf0[26]);
            buf1[26] = _mm_sub_epi32(buf0[18], buf0[26]);
            buf1[19] = _mm_add_epi32(buf0[19], buf0[27]);
            buf1[27] = _mm_sub_epi32(buf0[19], buf0[27]);
            buf1[20] = _mm_add_epi32(buf0[20], buf0[28]);
            buf1[28] = _mm_sub_epi32(buf0[20], buf0[28]);
            buf1[21] = _mm_add_epi32(buf0[21], buf0[29]);
            buf1[29] = _mm_sub_epi32(buf0[21], buf0[29]);
            buf1[22] = _mm_add_epi32(buf0[22], buf0[30]);
            buf1[30] = _mm_sub_epi32(buf0[22], buf0[30]);
            buf1[23] = _mm_add_epi32(buf0[23], buf0[31]);
            buf1[31] = _mm_sub_epi32(buf0[23], buf0[31]);

            // stage 6
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            buf0[0] = buf1[0];
            buf0[1] = buf1[1];
            buf0[2] = buf1[2];
            buf0[3] = buf1[3];
            buf0[4] = buf1[4];
            buf0[5] = buf1[5];
            buf0[6] = buf1[6];
            buf0[7] = buf1[7];
            btf_32_sse4_1_type0(cospi[8], cospi[56], buf1[8], buf1[9], buf0[8], buf0[9],
                cos_bit);
            btf_32_sse4_1_type0(cospi[40], cospi[24], buf1[10], buf1[11], buf0[10],
                buf0[11], cos_bit);
            btf_32_sse4_1_type0(-cospi[56], cospi[8], buf1[12], buf1[13], buf0[12],
                buf0[13], cos_bit);
            btf_32_sse4_1_type0(-cospi[24], cospi[40], buf1[14], buf1[15], buf0[14],
                buf0[15], cos_bit);
            buf0[16] = buf1[16];
            buf0[17] = buf1[17];
            buf0[18] = buf1[18];
            buf0[19] = buf1[19];
            buf0[20] = buf1[20];
            buf0[21] = buf1[21];
            buf0[22] = buf1[22];
            buf0[23] = buf1[23];
            btf_32_sse4_1_type0(cospi[8], cospi[56], buf1[24], buf1[25], buf0[24],
                buf0[25], cos_bit);
            btf_32_sse4_1_type0(cospi[40], cospi[24], buf1[26], buf1[27], buf0[26],
                buf0[27], cos_bit);
            btf_32_sse4_1_type0(-cospi[56], cospi[8], buf1[28], buf1[29], buf0[28],
                buf0[29], cos_bit);
            btf_32_sse4_1_type0(-cospi[24], cospi[40], buf1[30], buf1[31], buf0[30],
                buf0[31], cos_bit);

            // stage 7
            stage_idx++;
            buf1[0] = _mm_add_epi32(buf0[0], buf0[4]);
            buf1[4] = _mm_sub_epi32(buf0[0], buf0[4]);
            buf1[1] = _mm_add_epi32(buf0[1], buf0[5]);
            buf1[5] = _mm_sub_epi32(buf0[1], buf0[5]);
            buf1[2] = _mm_add_epi32(buf0[2], buf0[6]);
            buf1[6] = _mm_sub_epi32(buf0[2], buf0[6]);
            buf1[3] = _mm_add_epi32(buf0[3], buf0[7]);
            buf1[7] = _mm_sub_epi32(buf0[3], buf0[7]);
            buf1[8] = _mm_add_epi32(buf0[8], buf0[12]);
            buf1[12] = _mm_sub_epi32(buf0[8], buf0[12]);
            buf1[9] = _mm_add_epi32(buf0[9], buf0[13]);
            buf1[13] = _mm_sub_epi32(buf0[9], buf0[13]);
            buf1[10] = _mm_add_epi32(buf0[10], buf0[14]);
            buf1[14] = _mm_sub_epi32(buf0[10], buf0[14]);
            buf1[11] = _mm_add_epi32(buf0[11], buf0[15]);
            buf1[15] = _mm_sub_epi32(buf0[11], buf0[15]);
            buf1[16] = _mm_add_epi32(buf0[16], buf0[20]);
            buf1[20] = _mm_sub_epi32(buf0[16], buf0[20]);
            buf1[17] = _mm_add_epi32(buf0[17], buf0[21]);
            buf1[21] = _mm_sub_epi32(buf0[17], buf0[21]);
            buf1[18] = _mm_add_epi32(buf0[18], buf0[22]);
            buf1[22] = _mm_sub_epi32(buf0[18], buf0[22]);
            buf1[19] = _mm_add_epi32(buf0[19], buf0[23]);
            buf1[23] = _mm_sub_epi32(buf0[19], buf0[23]);
            buf1[24] = _mm_add_epi32(buf0[24], buf0[28]);
            buf1[28] = _mm_sub_epi32(buf0[24], buf0[28]);
            buf1[25] = _mm_add_epi32(buf0[25], buf0[29]);
            buf1[29] = _mm_sub_epi32(buf0[25], buf0[29]);
            buf1[26] = _mm_add_epi32(buf0[26], buf0[30]);
            buf1[30] = _mm_sub_epi32(buf0[26], buf0[30]);
            buf1[27] = _mm_add_epi32(buf0[27], buf0[31]);
            buf1[31] = _mm_sub_epi32(buf0[27], buf0[31]);

            // stage 8
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            buf0[0] = buf1[0];
            buf0[1] = buf1[1];
            buf0[2] = buf1[2];
            buf0[3] = buf1[3];
            btf_32_sse4_1_type0(cospi[16], cospi[48], buf1[4], buf1[5], buf0[4],
                buf0[5], cos_bit);
            btf_32_sse4_1_type0(-cospi[48], cospi[16], buf1[6], buf1[7], buf0[6],
                buf0[7], cos_bit);
            buf0[8] = buf1[8];
            buf0[9] = buf1[9];
            buf0[10] = buf1[10];
            buf0[11] = buf1[11];
            btf_32_sse4_1_type0(cospi[16], cospi[48], buf1[12], buf1[13], buf0[12],
                buf0[13], cos_bit);
            btf_32_sse4_1_type0(-cospi[48], cospi[16], buf1[14], buf1[15], buf0[14],
                buf0[15], cos_bit);
            buf0[16] = buf1[16];
            buf0[17] = buf1[17];
            buf0[18] = buf1[18];
            buf0[19] = buf1[19];
            btf_32_sse4_1_type0(cospi[16], cospi[48], buf1[20], buf1[21], buf0[20],
                buf0[21], cos_bit);
            btf_32_sse4_1_type0(-cospi[48], cospi[16], buf1[22], buf1[23], buf0[22],
                buf0[23], cos_bit);
            buf0[24] = buf1[24];
            buf0[25] = buf1[25];
            buf0[26] = buf1[26];
            buf0[27] = buf1[27];
            btf_32_sse4_1_type0(cospi[16], cospi[48], buf1[28], buf1[29], buf0[28],
                buf0[29], cos_bit);
            btf_32_sse4_1_type0(-cospi[48], cospi[16], buf1[30], buf1[31], buf0[30],
                buf0[31], cos_bit);

            // stage 9
            stage_idx++;
            buf1[0] = _mm_add_epi32(buf0[0], buf0[2]);
            buf1[2] = _mm_sub_epi32(buf0[0], buf0[2]);
            buf1[1] = _mm_add_epi32(buf0[1], buf0[3]);
            buf1[3] = _mm_sub_epi32(buf0[1], buf0[3]);
            buf1[4] = _mm_add_epi32(buf0[4], buf0[6]);
            buf1[6] = _mm_sub_epi32(buf0[4], buf0[6]);
            buf1[5] = _mm_add_epi32(buf0[5], buf0[7]);
            buf1[7] = _mm_sub_epi32(buf0[5], buf0[7]);
            buf1[8] = _mm_add_epi32(buf0[8], buf0[10]);
            buf1[10] = _mm_sub_epi32(buf0[8], buf0[10]);
            buf1[9] = _mm_add_epi32(buf0[9], buf0[11]);
            buf1[11] = _mm_sub_epi32(buf0[9], buf0[11]);
            buf1[12] = _mm_add_epi32(buf0[12], buf0[14]);
            buf1[14] = _mm_sub_epi32(buf0[12], buf0[14]);
            buf1[13] = _mm_add_epi32(buf0[13], buf0[15]);
            buf1[15] = _mm_sub_epi32(buf0[13], buf0[15]);
            buf1[16] = _mm_add_epi32(buf0[16], buf0[18]);
            buf1[18] = _mm_sub_epi32(buf0[16], buf0[18]);
            buf1[17] = _mm_add_epi32(buf0[17], buf0[19]);
            buf1[19] = _mm_sub_epi32(buf0[17], buf0[19]);
            buf1[20] = _mm_add_epi32(buf0[20], buf0[22]);
            buf1[22] = _mm_sub_epi32(buf0[20], buf0[22]);
            buf1[21] = _mm_add_epi32(buf0[21], buf0[23]);
            buf1[23] = _mm_sub_epi32(buf0[21], buf0[23]);
            buf1[24] = _mm_add_epi32(buf0[24], buf0[26]);
            buf1[26] = _mm_sub_epi32(buf0[24], buf0[26]);
            buf1[25] = _mm_add_epi32(buf0[25], buf0[27]);
            buf1[27] = _mm_sub_epi32(buf0[25], buf0[27]);
            buf1[28] = _mm_add_epi32(buf0[28], buf0[30]);
            buf1[30] = _mm_sub_epi32(buf0[28], buf0[30]);
            buf1[29] = _mm_add_epi32(buf0[29], buf0[31]);
            buf1[31] = _mm_sub_epi32(buf0[29], buf0[31]);

            // stage 10
            stage_idx++;

            cospi = cospi_arr(cos_bit);
            buf0[0] = buf1[0];
            buf0[1] = buf1[1];
            btf_32_sse4_1_type0(cospi[32], cospi[32], buf1[2], buf1[3], buf0[2],
                buf0[3], cos_bit);
            buf0[4] = buf1[4];
            buf0[5] = buf1[5];
            btf_32_sse4_1_type0(cospi[32], cospi[32], buf1[6], buf1[7], buf0[6],
                buf0[7], cos_bit);
            buf0[8] = buf1[8];
            buf0[9] = buf1[9];
            btf_32_sse4_1_type0(cospi[32], cospi[32], buf1[10], buf1[11], buf0[10],
                buf0[11], cos_bit);
            buf0[12] = buf1[12];
            buf0[13] = buf1[13];
            btf_32_sse4_1_type0(cospi[32], cospi[32], buf1[14], buf1[15], buf0[14],
                buf0[15], cos_bit);
            buf0[16] = buf1[16];
            buf0[17] = buf1[17];
            btf_32_sse4_1_type0(cospi[32], cospi[32], buf1[18], buf1[19], buf0[18],
                buf0[19], cos_bit);
            buf0[20] = buf1[20];
            buf0[21] = buf1[21];
            btf_32_sse4_1_type0(cospi[32], cospi[32], buf1[22], buf1[23], buf0[22],
                buf0[23], cos_bit);
            buf0[24] = buf1[24];
            buf0[25] = buf1[25];
            btf_32_sse4_1_type0(cospi[32], cospi[32], buf1[26], buf1[27], buf0[26],
                buf0[27], cos_bit);
            buf0[28] = buf1[28];
            buf0[29] = buf1[29];
            btf_32_sse4_1_type0(cospi[32], cospi[32], buf1[30], buf1[31], buf0[30],
                buf0[31], cos_bit);

            // stage 11
            stage_idx++;
            buf1[0] = buf0[0];
            buf1[1] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[16]);
            buf1[2] = buf0[24];
            buf1[3] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[8]);
            buf1[4] = buf0[12];
            buf1[5] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[28]);
            buf1[6] = buf0[20];
            buf1[7] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[4]);
            buf1[8] = buf0[6];
            buf1[9] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[22]);
            buf1[10] = buf0[30];
            buf1[11] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[14]);
            buf1[12] = buf0[10];
            buf1[13] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[26]);
            buf1[14] = buf0[18];
            buf1[15] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[2]);
            buf1[16] = buf0[3];
            buf1[17] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[19]);
            buf1[18] = buf0[27];
            buf1[19] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[11]);
            buf1[20] = buf0[15];
            buf1[21] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[31]);
            buf1[22] = buf0[23];
            buf1[23] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[7]);
            buf1[24] = buf0[5];
            buf1[25] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[21]);
            buf1[26] = buf0[29];
            buf1[27] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[13]);
            buf1[28] = buf0[9];
            buf1[29] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[25]);
            buf1[30] = buf0[17];
            buf1[31] = _mm_sub_epi32(_mm_set1_epi32(0), buf0[1]);

            for (j = 0; j < 32; ++j) {
                output[j * col_num + col] = buf1[j];
            }
        }
    }

    typedef void (*TxfmFuncSSE2)(const __m128i *input, __m128i *output,
        const int8_t cos_bit, const int8_t *stage_range);

    static INLINE TxfmFuncSSE2 fwd_txfm_type_to_func(TXFM_TYPE txfm_type) {
        switch (txfm_type) {
        case TXFM_TYPE_DCT32: return av1_fdct32_new_sse4_1; break;
        case TXFM_TYPE_ADST32: return av1_fadst32_new_sse4_1; break;
        default: assert(0);
        }
        return NULL;
    }

    static INLINE void int16_array_with_stride_to_int32_array_without_stride(
        const int16_t *input, int stride, int32_t *output, int txfm1d_size) {
            int r, c;
            for (r = 0; r < txfm1d_size; r++) {
                for (c = 0; c < txfm1d_size; c++) {
                    output[r * txfm1d_size + c] = (int32_t)input[r * stride + c];
                }
            }
    }



    static INLINE void transpose_32_4x4(int stride, const __m128i *input,
        __m128i *output) {
            __m128i temp0 = _mm_unpacklo_epi32(input[0 * stride], input[2 * stride]);
            __m128i temp1 = _mm_unpackhi_epi32(input[0 * stride], input[2 * stride]);
            __m128i temp2 = _mm_unpacklo_epi32(input[1 * stride], input[3 * stride]);
            __m128i temp3 = _mm_unpackhi_epi32(input[1 * stride], input[3 * stride]);

            output[0 * stride] = _mm_unpacklo_epi32(temp0, temp2);
            output[1 * stride] = _mm_unpackhi_epi32(temp0, temp2);
            output[2 * stride] = _mm_unpacklo_epi32(temp1, temp3);
            output[3 * stride] = _mm_unpackhi_epi32(temp1, temp3);
    }

    // the entire input block can be represent by a grid of 4x4 blocks
    // each 4x4 blocks can be represent by 4 vertical __m128i
    // we first transpose each 4x4 block internally
    // then transpose the grid
    static INLINE void transpose_32(int txfm_size, const __m128i *input,
        __m128i *output) {
            const int num_per_128 = 4;
            const int row_size = txfm_size;
            const int col_size = txfm_size / num_per_128;
            int r, c;

            // transpose each 4x4 block internally
            for (r = 0; r < row_size; r += 4) {
                for (c = 0; c < col_size; c++) {
                    transpose_32_4x4(col_size, &input[r * col_size + c],
                        &output[c * 4 * col_size + r / 4]);
                }
            }
    }

    static INLINE __m128i av1_round_shift_32_sse4_1(__m128i vec, int bit) {
        __m128i tmp, round;
        round = _mm_set1_epi32(1 << (bit - 1));
        tmp = _mm_add_epi32(vec, round);
        return _mm_srai_epi32(tmp, bit);
    }

    static INLINE void av1_round_shift_array_32_sse4_1(__m128i *input, __m128i *output, const int size, const int bit)
    {
        if (bit > 0) {
            int i;
            for (i = 0; i < size; i++) {
                output[i] = av1_round_shift_32_sse4_1(input[i], bit);
            }
        } else {
            int i;
            for (i = 0; i < size; i++) {
                output[i] = _mm_slli_epi32(input[i], -bit);
            }
        }
    }

    // Transform block width in pixels
    static const int tx_size_wide[TX_SIZES_ALL] = {
        4, 8, 16, 32, 64, 4, 8, 8, 16, 16, 32, 32, 64, 4, 16, 8, 32, 16, 64,
    };

    static INLINE void fwd_txfm2d_sse4_1(const int16_t *input, int32_t *output,
                                         const int stride, const TXFM_2D_FLIP_CFG *cfg, int32_t *txfm_buf)
    {
        // TODO(sarahparker) This does not currently support rectangular transforms
        // and will break without splitting txfm_size out into row and col size.
        // Rectangular transforms use c code only, so it should be ok for now.
        // It will be corrected when there are sse implementations for rectangular
        // transforms.
        assert(cfg->tx_size < TX_SIZES);
        const int txfm_size = tx_size_wide[cfg->tx_size];
        const int8_t *shift = cfg->shift;
        const int8_t *stage_range_col = cfg->stage_range_col;
        const int8_t *stage_range_row = cfg->stage_range_row;
        const int8_t cos_bit_col = cfg->cos_bit_col;
        const int8_t cos_bit_row = cfg->cos_bit_row;
        const TxfmFuncSSE2 txfm_func_col = fwd_txfm_type_to_func(cfg->txfm_type_col);
        const TxfmFuncSSE2 txfm_func_row = fwd_txfm_type_to_func(cfg->txfm_type_row);

        __m128i *buf_128 = (__m128i *)txfm_buf;
        __m128i *out_128 = (__m128i *)output;
        int num_per_128 = 4;
        int txfm2d_size_128 = txfm_size * txfm_size / num_per_128;

        int16_array_with_stride_to_int32_array_without_stride(input, stride, txfm_buf, txfm_size);
        av1_round_shift_array_32_sse4_1(buf_128, out_128, txfm2d_size_128, -shift[0]);
        txfm_func_col(out_128, buf_128, cos_bit_col, stage_range_col);
        av1_round_shift_array_32_sse4_1(buf_128, out_128, txfm2d_size_128, -shift[1]);
        transpose_32(txfm_size, out_128, buf_128);
        txfm_func_row(buf_128, out_128, cos_bit_row, stage_range_row);
        av1_round_shift_array_32_sse4_1(out_128, buf_128, txfm2d_size_128, -shift[2]);
        transpose_32(txfm_size, buf_128, out_128);
    }

    void av1_fwd_txfm2d_32x32_sse4_1(const int16_t *input, int32_t *output, int stride, int tx_type, int bd)
    {
        DECLARE_ALIGNED(16, int32_t, txfm_buf[1024]);
        TXFM_2D_FLIP_CFG cfg;
        av1_get_fwd_txfm_cfg((TX_TYPE)tx_type, TX_32X32, &cfg);
        (void)bd;
        fwd_txfm2d_sse4_1(input, output, stride, &cfg, txfm_buf);
    }

    static INLINE void load_buffer_4x4(const int16_t *input, __m128i *in,
        int stride, int flipud, int fliplr,
        int shift) {
            if (!flipud) {
                in[0] = _mm_loadl_epi64((const __m128i *)(input + 0 * stride));
                in[1] = _mm_loadl_epi64((const __m128i *)(input + 1 * stride));
                in[2] = _mm_loadl_epi64((const __m128i *)(input + 2 * stride));
                in[3] = _mm_loadl_epi64((const __m128i *)(input + 3 * stride));
            } else {
                in[0] = _mm_loadl_epi64((const __m128i *)(input + 3 * stride));
                in[1] = _mm_loadl_epi64((const __m128i *)(input + 2 * stride));
                in[2] = _mm_loadl_epi64((const __m128i *)(input + 1 * stride));
                in[3] = _mm_loadl_epi64((const __m128i *)(input + 0 * stride));
            }

            if (fliplr) {
                in[0] = _mm_shufflelo_epi16(in[0], 0x1b);
                in[1] = _mm_shufflelo_epi16(in[1], 0x1b);
                in[2] = _mm_shufflelo_epi16(in[2], 0x1b);
                in[3] = _mm_shufflelo_epi16(in[3], 0x1b);
            }

            in[0] = _mm_cvtepi16_epi32(in[0]);
            in[1] = _mm_cvtepi16_epi32(in[1]);
            in[2] = _mm_cvtepi16_epi32(in[2]);
            in[3] = _mm_cvtepi16_epi32(in[3]);

            in[0] = _mm_slli_epi32(in[0], shift);
            in[1] = _mm_slli_epi32(in[1], shift);
            in[2] = _mm_slli_epi32(in[2], shift);
            in[3] = _mm_slli_epi32(in[3], shift);
    }

    // We only use stage-2 bit;
    // shift[0] is used in load_buffer_4x4()
    // shift[1] is used in txfm_func_col()
    // shift[2] is used in txfm_func_row()
    static void fdct4x4_sse4_1(__m128i *in, int bit) {
        const int32_t *cospi = cospi_arr(bit);
        const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
        const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
        const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
        const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
        __m128i s0, s1, s2, s3;
        __m128i u0, u1, u2, u3;
        __m128i v0, v1, v2, v3;

        s0 = _mm_add_epi32(in[0], in[3]);
        s1 = _mm_add_epi32(in[1], in[2]);
        s2 = _mm_sub_epi32(in[1], in[2]);
        s3 = _mm_sub_epi32(in[0], in[3]);

        // btf_32_sse4_1_type0(cospi32, cospi32, s[01], u[02], bit);
        u0 = _mm_mullo_epi32(s0, cospi32);
        u1 = _mm_mullo_epi32(s1, cospi32);
        u2 = _mm_add_epi32(u0, u1);
        v0 = _mm_sub_epi32(u0, u1);

        u3 = _mm_add_epi32(u2, rnding);
        v1 = _mm_add_epi32(v0, rnding);

        u0 = _mm_srai_epi32(u3, bit);
        u2 = _mm_srai_epi32(v1, bit);

        // btf_32_sse4_1_type1(cospi48, cospi16, s[23], u[13], bit);
        v0 = _mm_mullo_epi32(s2, cospi48);
        v1 = _mm_mullo_epi32(s3, cospi16);
        v2 = _mm_add_epi32(v0, v1);

        v3 = _mm_add_epi32(v2, rnding);
        u1 = _mm_srai_epi32(v3, bit);

        v0 = _mm_mullo_epi32(s2, cospi16);
        v1 = _mm_mullo_epi32(s3, cospi48);
        v2 = _mm_sub_epi32(v1, v0);

        v3 = _mm_add_epi32(v2, rnding);
        u3 = _mm_srai_epi32(v3, bit);

        // Note: shift[1] and shift[2] are zeros

        // Transpose 4x4 32-bit
        v0 = _mm_unpacklo_epi32(u0, u1);
        v1 = _mm_unpackhi_epi32(u0, u1);
        v2 = _mm_unpacklo_epi32(u2, u3);
        v3 = _mm_unpackhi_epi32(u2, u3);

        in[0] = _mm_unpacklo_epi64(v0, v2);
        in[1] = _mm_unpackhi_epi64(v0, v2);
        in[2] = _mm_unpacklo_epi64(v1, v3);
        in[3] = _mm_unpackhi_epi64(v1, v3);
    }

    static INLINE void write_buffer_4x4(__m128i *res, int32_t *output) {
        _mm_store_si128((__m128i *)(output + 0 * 4), res[0]);
        _mm_store_si128((__m128i *)(output + 1 * 4), res[1]);
        _mm_store_si128((__m128i *)(output + 2 * 4), res[2]);
        _mm_store_si128((__m128i *)(output + 3 * 4), res[3]);
    }

    static void fadst4x4_sse4_1(__m128i *in, int bit) {
        const int32_t *cospi = cospi_arr(bit);
        const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
        const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
        const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
        const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
        const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
        const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
        const __m128i kZero = _mm_setzero_si128();
        __m128i s0, s1, s2, s3;
        __m128i u0, u1, u2, u3;
        __m128i v0, v1, v2, v3;

        // stage 0
        // stage 1
        // stage 2
        u0 = _mm_mullo_epi32(in[3], cospi8);
        u1 = _mm_mullo_epi32(in[0], cospi56);
        u2 = _mm_add_epi32(u0, u1);
        s0 = _mm_add_epi32(u2, rnding);
        s0 = _mm_srai_epi32(s0, bit);

        v0 = _mm_mullo_epi32(in[3], cospi56);
        v1 = _mm_mullo_epi32(in[0], cospi8);
        v2 = _mm_sub_epi32(v0, v1);
        s1 = _mm_add_epi32(v2, rnding);
        s1 = _mm_srai_epi32(s1, bit);

        u0 = _mm_mullo_epi32(in[1], cospi40);
        u1 = _mm_mullo_epi32(in[2], cospi24);
        u2 = _mm_add_epi32(u0, u1);
        s2 = _mm_add_epi32(u2, rnding);
        s2 = _mm_srai_epi32(s2, bit);

        v0 = _mm_mullo_epi32(in[1], cospi24);
        v1 = _mm_mullo_epi32(in[2], cospi40);
        v2 = _mm_sub_epi32(v0, v1);
        s3 = _mm_add_epi32(v2, rnding);
        s3 = _mm_srai_epi32(s3, bit);

        // stage 3
        u0 = _mm_add_epi32(s0, s2);
        u2 = _mm_sub_epi32(s0, s2);
        u1 = _mm_add_epi32(s1, s3);
        u3 = _mm_sub_epi32(s1, s3);

        // stage 4
        v0 = _mm_mullo_epi32(u2, cospi32);
        v1 = _mm_mullo_epi32(u3, cospi32);
        v2 = _mm_add_epi32(v0, v1);
        s2 = _mm_add_epi32(v2, rnding);
        u2 = _mm_srai_epi32(s2, bit);

        v2 = _mm_sub_epi32(v0, v1);
        s3 = _mm_add_epi32(v2, rnding);
        u3 = _mm_srai_epi32(s3, bit);

        // u0, u1, u2, u3
        u2 = _mm_sub_epi32(kZero, u2);
        u1 = _mm_sub_epi32(kZero, u1);

        // u0, u2, u3, u1
        // Transpose 4x4 32-bit
        v0 = _mm_unpacklo_epi32(u0, u2);
        v1 = _mm_unpackhi_epi32(u0, u2);
        v2 = _mm_unpacklo_epi32(u3, u1);
        v3 = _mm_unpackhi_epi32(u3, u1);

        in[0] = _mm_unpacklo_epi64(v0, v2);
        in[1] = _mm_unpackhi_epi64(v0, v2);
        in[2] = _mm_unpacklo_epi64(v1, v3);
        in[3] = _mm_unpackhi_epi64(v1, v3);
    }

    void av1_fwd_txfm2d_4x4_sse4_1(const int16_t *input, int32_t *coeff, int input_stride, int tx_type, int bd)
    {
        __m128i in[4];
        const TXFM_1D_CFG *row_cfg = NULL;
        const TXFM_1D_CFG *col_cfg = NULL;

        switch (tx_type) {
        case DCT_DCT:
            row_cfg = &fwd_txfm_1d_row_cfg_dct_4;
            col_cfg = &fwd_txfm_1d_col_cfg_dct_4;
            load_buffer_4x4(input, in, input_stride, 0, 0, row_cfg->shift[0]);
            fdct4x4_sse4_1(in, col_cfg->cos_bit[2]);
            fdct4x4_sse4_1(in, row_cfg->cos_bit[2]);
            write_buffer_4x4(in, coeff);
            break;
        case ADST_DCT:
            row_cfg = &fwd_txfm_1d_row_cfg_dct_4;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_4;
            load_buffer_4x4(input, in, input_stride, 0, 0, row_cfg->shift[0]);
            fadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
            fdct4x4_sse4_1(in, row_cfg->cos_bit[2]);
            write_buffer_4x4(in, coeff);
            break;
        case DCT_ADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_4;
            col_cfg = &fwd_txfm_1d_col_cfg_dct_4;
            load_buffer_4x4(input, in, input_stride, 0, 0, row_cfg->shift[0]);
            fdct4x4_sse4_1(in, col_cfg->cos_bit[2]);
            fadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
            write_buffer_4x4(in, coeff);
            break;
        case ADST_ADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_4;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_4;
            load_buffer_4x4(input, in, input_stride, 0, 0, row_cfg->shift[0]);
            fadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
            fadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
            write_buffer_4x4(in, coeff);
            break;
#if CONFIG_EXT_TX
        case FLIPADST_DCT:
            row_cfg = &fwd_txfm_1d_row_cfg_dct_4;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_4;
            load_buffer_4x4(input, in, input_stride, 1, 0, row_cfg->shift[0]);
            fadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
            fdct4x4_sse4_1(in, row_cfg->cos_bit[2]);
            write_buffer_4x4(in, coeff);
            break;
        case DCT_FLIPADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_4;
            col_cfg = &fwd_txfm_1d_col_cfg_dct_4;
            load_buffer_4x4(input, in, input_stride, 0, 1, row_cfg->shift[0]);
            fdct4x4_sse4_1(in, col_cfg->cos_bit[2]);
            fadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
            write_buffer_4x4(in, coeff);
            break;
        case FLIPADST_FLIPADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_4;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_4;
            load_buffer_4x4(input, in, input_stride, 1, 1, row_cfg->shift[0]);
            fadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
            fadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
            write_buffer_4x4(in, coeff);
            break;
        case ADST_FLIPADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_4;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_4;
            load_buffer_4x4(input, in, input_stride, 0, 1, row_cfg->shift[0]);
            fadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
            fadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
            write_buffer_4x4(in, coeff);
            break;
        case FLIPADST_ADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_4;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_4;
            load_buffer_4x4(input, in, input_stride, 1, 0, row_cfg->shift[0]);
            fadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
            fadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
            write_buffer_4x4(in, coeff);
            break;
#endif
        default: assert(0);
        }
        (void)bd;
    }

    static void fdct8x8_sse4_1(__m128i *in, __m128i *out, int bit) {
        const int32_t *cospi = cospi_arr(bit);
        const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
        const __m128i cospim32 = _mm_set1_epi32(-cospi[32]);
        const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
        const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
        const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
        const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
        const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
        const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
        const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
        __m128i u[8], v[8];

        // Even 8 points 0, 2, ..., 14
        // stage 0
        // stage 1
        u[0] = _mm_add_epi32(in[0], in[14]);
        v[7] = _mm_sub_epi32(in[0], in[14]);  // v[7]
        u[1] = _mm_add_epi32(in[2], in[12]);
        u[6] = _mm_sub_epi32(in[2], in[12]);
        u[2] = _mm_add_epi32(in[4], in[10]);
        u[5] = _mm_sub_epi32(in[4], in[10]);
        u[3] = _mm_add_epi32(in[6], in[8]);
        v[4] = _mm_sub_epi32(in[6], in[8]);  // v[4]

        // stage 2
        v[0] = _mm_add_epi32(u[0], u[3]);
        v[3] = _mm_sub_epi32(u[0], u[3]);
        v[1] = _mm_add_epi32(u[1], u[2]);
        v[2] = _mm_sub_epi32(u[1], u[2]);

        v[5] = _mm_mullo_epi32(u[5], cospim32);
        v[6] = _mm_mullo_epi32(u[6], cospi32);
        v[5] = _mm_add_epi32(v[5], v[6]);
        v[5] = _mm_add_epi32(v[5], rnding);
        v[5] = _mm_srai_epi32(v[5], bit);

        u[0] = _mm_mullo_epi32(u[5], cospi32);
        v[6] = _mm_mullo_epi32(u[6], cospim32);
        v[6] = _mm_sub_epi32(u[0], v[6]);
        v[6] = _mm_add_epi32(v[6], rnding);
        v[6] = _mm_srai_epi32(v[6], bit);

        // stage 3
        // type 0
        v[0] = _mm_mullo_epi32(v[0], cospi32);
        v[1] = _mm_mullo_epi32(v[1], cospi32);
        u[0] = _mm_add_epi32(v[0], v[1]);
        u[0] = _mm_add_epi32(u[0], rnding);
        u[0] = _mm_srai_epi32(u[0], bit);

        u[1] = _mm_sub_epi32(v[0], v[1]);
        u[1] = _mm_add_epi32(u[1], rnding);
        u[1] = _mm_srai_epi32(u[1], bit);

        // type 1
        v[0] = _mm_mullo_epi32(v[2], cospi48);
        v[1] = _mm_mullo_epi32(v[3], cospi16);
        u[2] = _mm_add_epi32(v[0], v[1]);
        u[2] = _mm_add_epi32(u[2], rnding);
        u[2] = _mm_srai_epi32(u[2], bit);

        v[0] = _mm_mullo_epi32(v[2], cospi16);
        v[1] = _mm_mullo_epi32(v[3], cospi48);
        u[3] = _mm_sub_epi32(v[1], v[0]);
        u[3] = _mm_add_epi32(u[3], rnding);
        u[3] = _mm_srai_epi32(u[3], bit);

        u[4] = _mm_add_epi32(v[4], v[5]);
        u[5] = _mm_sub_epi32(v[4], v[5]);
        u[6] = _mm_sub_epi32(v[7], v[6]);
        u[7] = _mm_add_epi32(v[7], v[6]);

        // stage 4
        // stage 5
        v[0] = _mm_mullo_epi32(u[4], cospi56);
        v[1] = _mm_mullo_epi32(u[7], cospi8);
        v[0] = _mm_add_epi32(v[0], v[1]);
        v[0] = _mm_add_epi32(v[0], rnding);
        out[2] = _mm_srai_epi32(v[0], bit);  // buf0[4]

        v[0] = _mm_mullo_epi32(u[4], cospi8);
        v[1] = _mm_mullo_epi32(u[7], cospi56);
        v[0] = _mm_sub_epi32(v[1], v[0]);
        v[0] = _mm_add_epi32(v[0], rnding);
        out[14] = _mm_srai_epi32(v[0], bit);  // buf0[7]

        v[0] = _mm_mullo_epi32(u[5], cospi24);
        v[1] = _mm_mullo_epi32(u[6], cospi40);
        v[0] = _mm_add_epi32(v[0], v[1]);
        v[0] = _mm_add_epi32(v[0], rnding);
        out[10] = _mm_srai_epi32(v[0], bit);  // buf0[5]

        v[0] = _mm_mullo_epi32(u[5], cospi40);
        v[1] = _mm_mullo_epi32(u[6], cospi24);
        v[0] = _mm_sub_epi32(v[1], v[0]);
        v[0] = _mm_add_epi32(v[0], rnding);
        out[6] = _mm_srai_epi32(v[0], bit);  // buf0[6]

        out[0] = u[0];   // buf0[0]
        out[8] = u[1];   // buf0[1]
        out[4] = u[2];   // buf0[2]
        out[12] = u[3];  // buf0[3]

        // Odd 8 points: 1, 3, ..., 15
        // stage 0
        // stage 1
        u[0] = _mm_add_epi32(in[1], in[15]);
        v[7] = _mm_sub_epi32(in[1], in[15]);  // v[7]
        u[1] = _mm_add_epi32(in[3], in[13]);
        u[6] = _mm_sub_epi32(in[3], in[13]);
        u[2] = _mm_add_epi32(in[5], in[11]);
        u[5] = _mm_sub_epi32(in[5], in[11]);
        u[3] = _mm_add_epi32(in[7], in[9]);
        v[4] = _mm_sub_epi32(in[7], in[9]);  // v[4]

        // stage 2
        v[0] = _mm_add_epi32(u[0], u[3]);
        v[3] = _mm_sub_epi32(u[0], u[3]);
        v[1] = _mm_add_epi32(u[1], u[2]);
        v[2] = _mm_sub_epi32(u[1], u[2]);

        v[5] = _mm_mullo_epi32(u[5], cospim32);
        v[6] = _mm_mullo_epi32(u[6], cospi32);
        v[5] = _mm_add_epi32(v[5], v[6]);
        v[5] = _mm_add_epi32(v[5], rnding);
        v[5] = _mm_srai_epi32(v[5], bit);

        u[0] = _mm_mullo_epi32(u[5], cospi32);
        v[6] = _mm_mullo_epi32(u[6], cospim32);
        v[6] = _mm_sub_epi32(u[0], v[6]);
        v[6] = _mm_add_epi32(v[6], rnding);
        v[6] = _mm_srai_epi32(v[6], bit);

        // stage 3
        // type 0
        v[0] = _mm_mullo_epi32(v[0], cospi32);
        v[1] = _mm_mullo_epi32(v[1], cospi32);
        u[0] = _mm_add_epi32(v[0], v[1]);
        u[0] = _mm_add_epi32(u[0], rnding);
        u[0] = _mm_srai_epi32(u[0], bit);

        u[1] = _mm_sub_epi32(v[0], v[1]);
        u[1] = _mm_add_epi32(u[1], rnding);
        u[1] = _mm_srai_epi32(u[1], bit);

        // type 1
        v[0] = _mm_mullo_epi32(v[2], cospi48);
        v[1] = _mm_mullo_epi32(v[3], cospi16);
        u[2] = _mm_add_epi32(v[0], v[1]);
        u[2] = _mm_add_epi32(u[2], rnding);
        u[2] = _mm_srai_epi32(u[2], bit);

        v[0] = _mm_mullo_epi32(v[2], cospi16);
        v[1] = _mm_mullo_epi32(v[3], cospi48);
        u[3] = _mm_sub_epi32(v[1], v[0]);
        u[3] = _mm_add_epi32(u[3], rnding);
        u[3] = _mm_srai_epi32(u[3], bit);

        u[4] = _mm_add_epi32(v[4], v[5]);
        u[5] = _mm_sub_epi32(v[4], v[5]);
        u[6] = _mm_sub_epi32(v[7], v[6]);
        u[7] = _mm_add_epi32(v[7], v[6]);

        // stage 4
        // stage 5
        v[0] = _mm_mullo_epi32(u[4], cospi56);
        v[1] = _mm_mullo_epi32(u[7], cospi8);
        v[0] = _mm_add_epi32(v[0], v[1]);
        v[0] = _mm_add_epi32(v[0], rnding);
        out[3] = _mm_srai_epi32(v[0], bit);  // buf0[4]

        v[0] = _mm_mullo_epi32(u[4], cospi8);
        v[1] = _mm_mullo_epi32(u[7], cospi56);
        v[0] = _mm_sub_epi32(v[1], v[0]);
        v[0] = _mm_add_epi32(v[0], rnding);
        out[15] = _mm_srai_epi32(v[0], bit);  // buf0[7]

        v[0] = _mm_mullo_epi32(u[5], cospi24);
        v[1] = _mm_mullo_epi32(u[6], cospi40);
        v[0] = _mm_add_epi32(v[0], v[1]);
        v[0] = _mm_add_epi32(v[0], rnding);
        out[11] = _mm_srai_epi32(v[0], bit);  // buf0[5]

        v[0] = _mm_mullo_epi32(u[5], cospi40);
        v[1] = _mm_mullo_epi32(u[6], cospi24);
        v[0] = _mm_sub_epi32(v[1], v[0]);
        v[0] = _mm_add_epi32(v[0], rnding);
        out[7] = _mm_srai_epi32(v[0], bit);  // buf0[6]

        out[1] = u[0];   // buf0[0]
        out[9] = u[1];   // buf0[1]
        out[5] = u[2];   // buf0[2]
        out[13] = u[3];  // buf0[3]
    }

    static void fadst8x8_sse4_1(__m128i *in, __m128i *out, int bit) {
        const int32_t *cospi = cospi_arr(bit);
        const __m128i cospi4 = _mm_set1_epi32(cospi[4]);
        const __m128i cospi60 = _mm_set1_epi32(cospi[60]);
        const __m128i cospi20 = _mm_set1_epi32(cospi[20]);
        const __m128i cospi44 = _mm_set1_epi32(cospi[44]);
        const __m128i cospi36 = _mm_set1_epi32(cospi[36]);
        const __m128i cospi28 = _mm_set1_epi32(cospi[28]);
        const __m128i cospi52 = _mm_set1_epi32(cospi[52]);
        const __m128i cospi12 = _mm_set1_epi32(cospi[12]);
        const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
        const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
        const __m128i cospim48 = _mm_set1_epi32(-cospi[48]);
        const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
        const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
        const __m128i kZero = _mm_setzero_si128();
        __m128i u[8], v[8], x;

        // Even 8 points: 0, 2, ..., 14
        // stage 0
        // stage 1
        // stage 2
        // (1)
        u[0] = _mm_mullo_epi32(in[14], cospi4);
        x = _mm_mullo_epi32(in[0], cospi60);
        u[0] = _mm_add_epi32(u[0], x);
        u[0] = _mm_add_epi32(u[0], rnding);
        u[0] = _mm_srai_epi32(u[0], bit);

        u[1] = _mm_mullo_epi32(in[14], cospi60);
        x = _mm_mullo_epi32(in[0], cospi4);
        u[1] = _mm_sub_epi32(u[1], x);
        u[1] = _mm_add_epi32(u[1], rnding);
        u[1] = _mm_srai_epi32(u[1], bit);

        // (2)
        u[2] = _mm_mullo_epi32(in[10], cospi20);
        x = _mm_mullo_epi32(in[4], cospi44);
        u[2] = _mm_add_epi32(u[2], x);
        u[2] = _mm_add_epi32(u[2], rnding);
        u[2] = _mm_srai_epi32(u[2], bit);

        u[3] = _mm_mullo_epi32(in[10], cospi44);
        x = _mm_mullo_epi32(in[4], cospi20);
        u[3] = _mm_sub_epi32(u[3], x);
        u[3] = _mm_add_epi32(u[3], rnding);
        u[3] = _mm_srai_epi32(u[3], bit);

        // (3)
        u[4] = _mm_mullo_epi32(in[6], cospi36);
        x = _mm_mullo_epi32(in[8], cospi28);
        u[4] = _mm_add_epi32(u[4], x);
        u[4] = _mm_add_epi32(u[4], rnding);
        u[4] = _mm_srai_epi32(u[4], bit);

        u[5] = _mm_mullo_epi32(in[6], cospi28);
        x = _mm_mullo_epi32(in[8], cospi36);
        u[5] = _mm_sub_epi32(u[5], x);
        u[5] = _mm_add_epi32(u[5], rnding);
        u[5] = _mm_srai_epi32(u[5], bit);

        // (4)
        u[6] = _mm_mullo_epi32(in[2], cospi52);
        x = _mm_mullo_epi32(in[12], cospi12);
        u[6] = _mm_add_epi32(u[6], x);
        u[6] = _mm_add_epi32(u[6], rnding);
        u[6] = _mm_srai_epi32(u[6], bit);

        u[7] = _mm_mullo_epi32(in[2], cospi12);
        x = _mm_mullo_epi32(in[12], cospi52);
        u[7] = _mm_sub_epi32(u[7], x);
        u[7] = _mm_add_epi32(u[7], rnding);
        u[7] = _mm_srai_epi32(u[7], bit);

        // stage 3
        v[0] = _mm_add_epi32(u[0], u[4]);
        v[4] = _mm_sub_epi32(u[0], u[4]);
        v[1] = _mm_add_epi32(u[1], u[5]);
        v[5] = _mm_sub_epi32(u[1], u[5]);
        v[2] = _mm_add_epi32(u[2], u[6]);
        v[6] = _mm_sub_epi32(u[2], u[6]);
        v[3] = _mm_add_epi32(u[3], u[7]);
        v[7] = _mm_sub_epi32(u[3], u[7]);

        // stage 4
        u[0] = v[0];
        u[1] = v[1];
        u[2] = v[2];
        u[3] = v[3];

        u[4] = _mm_mullo_epi32(v[4], cospi16);
        x = _mm_mullo_epi32(v[5], cospi48);
        u[4] = _mm_add_epi32(u[4], x);
        u[4] = _mm_add_epi32(u[4], rnding);
        u[4] = _mm_srai_epi32(u[4], bit);

        u[5] = _mm_mullo_epi32(v[4], cospi48);
        x = _mm_mullo_epi32(v[5], cospi16);
        u[5] = _mm_sub_epi32(u[5], x);
        u[5] = _mm_add_epi32(u[5], rnding);
        u[5] = _mm_srai_epi32(u[5], bit);

        u[6] = _mm_mullo_epi32(v[6], cospim48);
        x = _mm_mullo_epi32(v[7], cospi16);
        u[6] = _mm_add_epi32(u[6], x);
        u[6] = _mm_add_epi32(u[6], rnding);
        u[6] = _mm_srai_epi32(u[6], bit);

        u[7] = _mm_mullo_epi32(v[6], cospi16);
        x = _mm_mullo_epi32(v[7], cospim48);
        u[7] = _mm_sub_epi32(u[7], x);
        u[7] = _mm_add_epi32(u[7], rnding);
        u[7] = _mm_srai_epi32(u[7], bit);

        // stage 5
        v[0] = _mm_add_epi32(u[0], u[2]);
        v[2] = _mm_sub_epi32(u[0], u[2]);
        v[1] = _mm_add_epi32(u[1], u[3]);
        v[3] = _mm_sub_epi32(u[1], u[3]);
        v[4] = _mm_add_epi32(u[4], u[6]);
        v[6] = _mm_sub_epi32(u[4], u[6]);
        v[5] = _mm_add_epi32(u[5], u[7]);
        v[7] = _mm_sub_epi32(u[5], u[7]);

        // stage 6
        u[0] = v[0];
        u[1] = v[1];
        u[4] = v[4];
        u[5] = v[5];

        v[0] = _mm_mullo_epi32(v[2], cospi32);
        x = _mm_mullo_epi32(v[3], cospi32);
        u[2] = _mm_add_epi32(v[0], x);
        u[2] = _mm_add_epi32(u[2], rnding);
        u[2] = _mm_srai_epi32(u[2], bit);

        u[3] = _mm_sub_epi32(v[0], x);
        u[3] = _mm_add_epi32(u[3], rnding);
        u[3] = _mm_srai_epi32(u[3], bit);

        v[0] = _mm_mullo_epi32(v[6], cospi32);
        x = _mm_mullo_epi32(v[7], cospi32);
        u[6] = _mm_add_epi32(v[0], x);
        u[6] = _mm_add_epi32(u[6], rnding);
        u[6] = _mm_srai_epi32(u[6], bit);

        u[7] = _mm_sub_epi32(v[0], x);
        u[7] = _mm_add_epi32(u[7], rnding);
        u[7] = _mm_srai_epi32(u[7], bit);

        // stage 7
        out[0] = u[0];
        out[2] = _mm_sub_epi32(kZero, u[4]);
        out[4] = u[6];
        out[6] = _mm_sub_epi32(kZero, u[2]);
        out[8] = u[3];
        out[10] = _mm_sub_epi32(kZero, u[7]);
        out[12] = u[5];
        out[14] = _mm_sub_epi32(kZero, u[1]);

        // Odd 8 points: 1, 3, ..., 15
        // stage 0
        // stage 1
        // stage 2
        // (1)
        u[0] = _mm_mullo_epi32(in[15], cospi4);
        x = _mm_mullo_epi32(in[1], cospi60);
        u[0] = _mm_add_epi32(u[0], x);
        u[0] = _mm_add_epi32(u[0], rnding);
        u[0] = _mm_srai_epi32(u[0], bit);

        u[1] = _mm_mullo_epi32(in[15], cospi60);
        x = _mm_mullo_epi32(in[1], cospi4);
        u[1] = _mm_sub_epi32(u[1], x);
        u[1] = _mm_add_epi32(u[1], rnding);
        u[1] = _mm_srai_epi32(u[1], bit);

        // (2)
        u[2] = _mm_mullo_epi32(in[11], cospi20);
        x = _mm_mullo_epi32(in[5], cospi44);
        u[2] = _mm_add_epi32(u[2], x);
        u[2] = _mm_add_epi32(u[2], rnding);
        u[2] = _mm_srai_epi32(u[2], bit);

        u[3] = _mm_mullo_epi32(in[11], cospi44);
        x = _mm_mullo_epi32(in[5], cospi20);
        u[3] = _mm_sub_epi32(u[3], x);
        u[3] = _mm_add_epi32(u[3], rnding);
        u[3] = _mm_srai_epi32(u[3], bit);

        // (3)
        u[4] = _mm_mullo_epi32(in[7], cospi36);
        x = _mm_mullo_epi32(in[9], cospi28);
        u[4] = _mm_add_epi32(u[4], x);
        u[4] = _mm_add_epi32(u[4], rnding);
        u[4] = _mm_srai_epi32(u[4], bit);

        u[5] = _mm_mullo_epi32(in[7], cospi28);
        x = _mm_mullo_epi32(in[9], cospi36);
        u[5] = _mm_sub_epi32(u[5], x);
        u[5] = _mm_add_epi32(u[5], rnding);
        u[5] = _mm_srai_epi32(u[5], bit);

        // (4)
        u[6] = _mm_mullo_epi32(in[3], cospi52);
        x = _mm_mullo_epi32(in[13], cospi12);
        u[6] = _mm_add_epi32(u[6], x);
        u[6] = _mm_add_epi32(u[6], rnding);
        u[6] = _mm_srai_epi32(u[6], bit);

        u[7] = _mm_mullo_epi32(in[3], cospi12);
        x = _mm_mullo_epi32(in[13], cospi52);
        u[7] = _mm_sub_epi32(u[7], x);
        u[7] = _mm_add_epi32(u[7], rnding);
        u[7] = _mm_srai_epi32(u[7], bit);

        // stage 3
        v[0] = _mm_add_epi32(u[0], u[4]);
        v[4] = _mm_sub_epi32(u[0], u[4]);
        v[1] = _mm_add_epi32(u[1], u[5]);
        v[5] = _mm_sub_epi32(u[1], u[5]);
        v[2] = _mm_add_epi32(u[2], u[6]);
        v[6] = _mm_sub_epi32(u[2], u[6]);
        v[3] = _mm_add_epi32(u[3], u[7]);
        v[7] = _mm_sub_epi32(u[3], u[7]);

        // stage 4
        u[0] = v[0];
        u[1] = v[1];
        u[2] = v[2];
        u[3] = v[3];

        u[4] = _mm_mullo_epi32(v[4], cospi16);
        x = _mm_mullo_epi32(v[5], cospi48);
        u[4] = _mm_add_epi32(u[4], x);
        u[4] = _mm_add_epi32(u[4], rnding);
        u[4] = _mm_srai_epi32(u[4], bit);

        u[5] = _mm_mullo_epi32(v[4], cospi48);
        x = _mm_mullo_epi32(v[5], cospi16);
        u[5] = _mm_sub_epi32(u[5], x);
        u[5] = _mm_add_epi32(u[5], rnding);
        u[5] = _mm_srai_epi32(u[5], bit);

        u[6] = _mm_mullo_epi32(v[6], cospim48);
        x = _mm_mullo_epi32(v[7], cospi16);
        u[6] = _mm_add_epi32(u[6], x);
        u[6] = _mm_add_epi32(u[6], rnding);
        u[6] = _mm_srai_epi32(u[6], bit);

        u[7] = _mm_mullo_epi32(v[6], cospi16);
        x = _mm_mullo_epi32(v[7], cospim48);
        u[7] = _mm_sub_epi32(u[7], x);
        u[7] = _mm_add_epi32(u[7], rnding);
        u[7] = _mm_srai_epi32(u[7], bit);

        // stage 5
        v[0] = _mm_add_epi32(u[0], u[2]);
        v[2] = _mm_sub_epi32(u[0], u[2]);
        v[1] = _mm_add_epi32(u[1], u[3]);
        v[3] = _mm_sub_epi32(u[1], u[3]);
        v[4] = _mm_add_epi32(u[4], u[6]);
        v[6] = _mm_sub_epi32(u[4], u[6]);
        v[5] = _mm_add_epi32(u[5], u[7]);
        v[7] = _mm_sub_epi32(u[5], u[7]);

        // stage 6
        u[0] = v[0];
        u[1] = v[1];
        u[4] = v[4];
        u[5] = v[5];

        v[0] = _mm_mullo_epi32(v[2], cospi32);
        x = _mm_mullo_epi32(v[3], cospi32);
        u[2] = _mm_add_epi32(v[0], x);
        u[2] = _mm_add_epi32(u[2], rnding);
        u[2] = _mm_srai_epi32(u[2], bit);

        u[3] = _mm_sub_epi32(v[0], x);
        u[3] = _mm_add_epi32(u[3], rnding);
        u[3] = _mm_srai_epi32(u[3], bit);

        v[0] = _mm_mullo_epi32(v[6], cospi32);
        x = _mm_mullo_epi32(v[7], cospi32);
        u[6] = _mm_add_epi32(v[0], x);
        u[6] = _mm_add_epi32(u[6], rnding);
        u[6] = _mm_srai_epi32(u[6], bit);

        u[7] = _mm_sub_epi32(v[0], x);
        u[7] = _mm_add_epi32(u[7], rnding);
        u[7] = _mm_srai_epi32(u[7], bit);

        // stage 7
        out[1] = u[0];
        out[3] = _mm_sub_epi32(kZero, u[4]);
        out[5] = u[6];
        out[7] = _mm_sub_epi32(kZero, u[2]);
        out[9] = u[3];
        out[11] = _mm_sub_epi32(kZero, u[7]);
        out[13] = u[5];
        out[15] = _mm_sub_epi32(kZero, u[1]);
    }

    static INLINE void transpose_8x8(const __m128i *in, __m128i *out) {
        TRANSPOSE_4X4(in[0], in[2], in[4], in[6], out[0], out[2], out[4], out[6]);
        TRANSPOSE_4X4(in[1], in[3], in[5], in[7], out[8], out[10], out[12], out[14]);
        TRANSPOSE_4X4(in[8], in[10], in[12], in[14], out[1], out[3], out[5], out[7]);
        TRANSPOSE_4X4(in[9], in[11], in[13], in[15], out[9], out[11], out[13],
            out[15]);
    }

    void av1_fwd_txfm2d_8x8_sse4_1(const int16_t *input, int32_t *coeff, int stride, int tx_type, int bd)
    {
        __m128i in[16], out[16];
        const TXFM_1D_CFG *row_cfg = NULL;
        const TXFM_1D_CFG *col_cfg = NULL;

        switch (tx_type) {
        case DCT_DCT:
            row_cfg = &fwd_txfm_1d_row_cfg_dct_8;
            col_cfg = &fwd_txfm_1d_col_cfg_dct_8;
            load_buffer_8x8(input, in, stride, 0, 0, row_cfg->shift[0]);
            fdct8x8_sse4_1(in, out, col_cfg->cos_bit[2]);
            col_txfm_8x8_rounding(out, -row_cfg->shift[1]);
            transpose_8x8(out, in);
            fdct8x8_sse4_1(in, out, row_cfg->cos_bit[2]);
            transpose_8x8(out, in);
            write_buffer_8x8(in, coeff);
            break;
        case ADST_DCT:
            row_cfg = &fwd_txfm_1d_row_cfg_dct_8;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_8;
            load_buffer_8x8(input, in, stride, 0, 0, row_cfg->shift[0]);
            fadst8x8_sse4_1(in, out, col_cfg->cos_bit[2]);
            col_txfm_8x8_rounding(out, -row_cfg->shift[1]);
            transpose_8x8(out, in);
            fdct8x8_sse4_1(in, out, row_cfg->cos_bit[2]);
            transpose_8x8(out, in);
            write_buffer_8x8(in, coeff);
            break;
        case DCT_ADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_8;
            col_cfg = &fwd_txfm_1d_col_cfg_dct_8;
            load_buffer_8x8(input, in, stride, 0, 0, row_cfg->shift[0]);
            fdct8x8_sse4_1(in, out, col_cfg->cos_bit[2]);
            col_txfm_8x8_rounding(out, -row_cfg->shift[1]);
            transpose_8x8(out, in);
            fadst8x8_sse4_1(in, out, row_cfg->cos_bit[2]);
            transpose_8x8(out, in);
            write_buffer_8x8(in, coeff);
            break;
        case ADST_ADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_8;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_8;
            load_buffer_8x8(input, in, stride, 0, 0, row_cfg->shift[0]);
            fadst8x8_sse4_1(in, out, col_cfg->cos_bit[2]);
            col_txfm_8x8_rounding(out, -row_cfg->shift[1]);
            transpose_8x8(out, in);
            fadst8x8_sse4_1(in, out, row_cfg->cos_bit[2]);
            transpose_8x8(out, in);
            write_buffer_8x8(in, coeff);
            break;
#if CONFIG_EXT_TX
        case FLIPADST_DCT:
            row_cfg = &fwd_txfm_1d_row_cfg_dct_8;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_8;
            load_buffer_8x8(input, in, stride, 1, 0, row_cfg->shift[0]);
            fadst8x8_sse4_1(in, out, col_cfg->cos_bit[2]);
            col_txfm_8x8_rounding(out, -row_cfg->shift[1]);
            transpose_8x8(out, in);
            fdct8x8_sse4_1(in, out, row_cfg->cos_bit[2]);
            transpose_8x8(out, in);
            write_buffer_8x8(in, coeff);
            break;
        case DCT_FLIPADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_8;
            col_cfg = &fwd_txfm_1d_col_cfg_dct_8;
            load_buffer_8x8(input, in, stride, 0, 1, row_cfg->shift[0]);
            fdct8x8_sse4_1(in, out, col_cfg->cos_bit[2]);
            col_txfm_8x8_rounding(out, -row_cfg->shift[1]);
            transpose_8x8(out, in);
            fadst8x8_sse4_1(in, out, row_cfg->cos_bit[2]);
            transpose_8x8(out, in);
            write_buffer_8x8(in, coeff);
            break;
        case FLIPADST_FLIPADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_8;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_8;
            load_buffer_8x8(input, in, stride, 1, 1, row_cfg->shift[0]);
            fadst8x8_sse4_1(in, out, col_cfg->cos_bit[2]);
            col_txfm_8x8_rounding(out, -row_cfg->shift[1]);
            transpose_8x8(out, in);
            fadst8x8_sse4_1(in, out, row_cfg->cos_bit[2]);
            transpose_8x8(out, in);
            write_buffer_8x8(in, coeff);
            break;
        case ADST_FLIPADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_8;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_8;
            load_buffer_8x8(input, in, stride, 0, 1, row_cfg->shift[0]);
            fadst8x8_sse4_1(in, out, col_cfg->cos_bit[2]);
            col_txfm_8x8_rounding(out, -row_cfg->shift[1]);
            transpose_8x8(out, in);
            fadst8x8_sse4_1(in, out, row_cfg->cos_bit[2]);
            transpose_8x8(out, in);
            write_buffer_8x8(in, coeff);
            break;
        case FLIPADST_ADST:
            row_cfg = &fwd_txfm_1d_row_cfg_adst_8;
            col_cfg = &fwd_txfm_1d_col_cfg_adst_8;
            load_buffer_8x8(input, in, stride, 1, 0, row_cfg->shift[0]);
            fadst8x8_sse4_1(in, out, col_cfg->cos_bit[2]);
            col_txfm_8x8_rounding(out, -row_cfg->shift[1]);
            transpose_8x8(out, in);
            fadst8x8_sse4_1(in, out, row_cfg->cos_bit[2]);
            transpose_8x8(out, in);
            write_buffer_8x8(in, coeff);
            break;
#endif  // CONFIG_EXT_TX
        default: assert(0);
        }
        (void)bd;
    }

};  // namespace details

namespace AV1PP {

    template <int size, int type> void ftransform_hbd_sse4(const int16_t *src, int32_t *dst, int pitchSrc) {
        if (size == 0)      details::av1_fwd_txfm2d_4x4_sse4_1(src, dst, pitchSrc, type, 8);
        else if (size == 1) details::av1_fwd_txfm2d_8x8_sse4_1(src, dst, pitchSrc, type, 8);
        else if (size == 2) details::av1_fwd_txfm2d_16x16_sse4_1(src, dst, pitchSrc, type, 8);
        else if (size == 3) details::av1_fwd_txfm2d_32x32_sse4_1(src, dst, pitchSrc, type, 8);
        else {assert(0);}
    }
    template void ftransform_hbd_sse4<0, 0>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<0, 1>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<0, 2>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<0, 3>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<1, 0>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<1, 1>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<1, 2>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<1, 3>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<2, 0>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<2, 1>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<2, 2>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<2, 3>(const int16_t*,int32_t*,int);
    template void ftransform_hbd_sse4<3, 0>(const int16_t*,int32_t*,int);

}; // namespace AV1PP
