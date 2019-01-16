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
#include "stdio.h"
#include "immintrin.h"
#include "mfx_av1_opts_common.h"
#include "mfx_av1_transform_common_avx2.h"


typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;

enum { TX_4X4=0, TX_8X8=1, TX_16X16=2, TX_32X32=3, TX_SIZES=4, TX_SIZES_ALL=19 };
enum { DCT_DCT=0, ADST_DCT=1, DCT_ADST=2, ADST_ADST=3, FLIPADST_DCT=4, DCT_FLIPADST=5,
       FLIPADST_FLIPADST=6, ADST_FLIPADST=7, FLIPADST_ADST=8, IDTX=9, V_DCT=10,
       H_DCT=11, V_ADST=12, H_ADST=13, V_FLIPADST=14, H_FLIPADST=15, TX_TYPES
};

#ifndef NULL
#ifdef __cplusplus#include "assert.h"
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
} TXFM_TYPE;

typedef struct TXFM_1D_CFG {
    int txfm_size;
    int stage_num;

    const int8_t *shift;
    const int8_t *stage_range;
    const int8_t *cos_bit;
    TXFM_TYPE txfm_type;
} TXFM_1D_CFG;

typedef struct TXFM_2D_FLIP_CFG {
    int ud_flip;  // flip upside down
    int lr_flip;  // flip left to right
    const TXFM_1D_CFG *col_cfg;
    const TXFM_1D_CFG *row_cfg;
} TXFM_2D_FLIP_CFG;

#define INLINE inline

namespace details {

    const int8_t inv_shift_4x4[2] = { 0, -4 };
    const int8_t inv_shift_8x8[2] = { -1, -4 };
    const int8_t inv_shift_16x16[2] = { -2, -4 };
    const int8_t inv_shift_32x32[2] = { -2, -4 };
    const int8_t inv_shift_64x64[2] = { -2, -4 };
    const int8_t inv_shift_4x8[2] = { 0, -4 };
    const int8_t inv_shift_8x4[2] = { 0, -4 };
    const int8_t inv_shift_8x16[2] = { -1, -4 };
    const int8_t inv_shift_16x8[2] = { -1, -4 };
    const int8_t inv_shift_16x32[2] = { -1, -4 };
    const int8_t inv_shift_32x16[2] = { -1, -4 };
    const int8_t inv_shift_32x64[2] = { -1, -4 };
    const int8_t inv_shift_64x32[2] = { -1, -4 };
    const int8_t inv_shift_4x16[2] = { -1, -4 };
    const int8_t inv_shift_16x4[2] = { -1, -4 };
    const int8_t inv_shift_8x32[2] = { -2, -4 };
    const int8_t inv_shift_32x8[2] = { -2, -4 };
    const int8_t inv_shift_16x64[2] = { -2, -4 };
    const int8_t inv_shift_64x16[2] = { -2, -4 };

    const int8_t *inv_txfm_shift_ls[TX_SIZES_ALL] = {
        inv_shift_4x4,   inv_shift_8x8,   inv_shift_16x16, inv_shift_32x32,
        inv_shift_64x64, inv_shift_4x8,   inv_shift_8x4,   inv_shift_8x16,
        inv_shift_16x8,  inv_shift_16x32, inv_shift_32x16, inv_shift_32x64,
        inv_shift_64x32, inv_shift_4x16,  inv_shift_16x4,  inv_shift_8x32,
        inv_shift_32x8,  inv_shift_16x64, inv_shift_64x16,
    };

    const int8_t MAX_TXWH_IDX = 5;
    const int8_t INV_COS_BIT = 12;
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


    static INLINE void load_buffer_4x4(const int32_t *coeff, __m128i *in) {
        in[0] = _mm_load_si128((const __m128i *)(coeff + 0));
        in[1] = _mm_load_si128((const __m128i *)(coeff + 4));
        in[2] = _mm_load_si128((const __m128i *)(coeff + 8));
        in[3] = _mm_load_si128((const __m128i *)(coeff + 12));
    }

    static INLINE void load_buffer_4x4_view_src_as_16s(const int32_t *coeff, __m128i *in) {

        int16_t* coeff16s = (int16_t*)coeff;
        in[0] = _mm_loadl_epi64((const __m128i *)(coeff16s + 0));
        in[0] = _mm_cvtepi16_epi32 (in[0]);
        in[1] = _mm_loadl_epi64((const __m128i *)(coeff16s + 4));
        in[1] = _mm_cvtepi16_epi32 (in[1]);
        in[2] = _mm_loadl_epi64((const __m128i *)(coeff16s + 8));
        in[2] = _mm_cvtepi16_epi32 (in[2]);
        in[3] = _mm_loadl_epi64((const __m128i *)(coeff16s + 12));
        in[3] = _mm_cvtepi16_epi32 (in[3]);

    }

    static const int cos_bit_min = 10;

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

    static void idct4x4_sse4_1(__m128i *in, int bit) {
        const int32_t *cospi = cospi_arr(bit);
        const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
        const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
        const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
        const __m128i cospim16 = _mm_set1_epi32(-cospi[16]);
        const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
        __m128i u0, u1, u2, u3;
        __m128i v0, v1, v2, v3, x, y;

        v0 = _mm_unpacklo_epi32(in[0], in[1]);
        v1 = _mm_unpackhi_epi32(in[0], in[1]);
        v2 = _mm_unpacklo_epi32(in[2], in[3]);
        v3 = _mm_unpackhi_epi32(in[2], in[3]);

        u0 = _mm_unpacklo_epi64(v0, v2);
        u1 = _mm_unpackhi_epi64(v0, v2);
        u2 = _mm_unpacklo_epi64(v1, v3);
        u3 = _mm_unpackhi_epi64(v1, v3);

        x = _mm_mullo_epi32(u0, cospi32);
        y = _mm_mullo_epi32(u2, cospi32);
        v0 = _mm_add_epi32(x, y);
        v0 = _mm_add_epi32(v0, rnding);
        v0 = _mm_srai_epi32(v0, bit);

        v1 = _mm_sub_epi32(x, y);
        v1 = _mm_add_epi32(v1, rnding);
        v1 = _mm_srai_epi32(v1, bit);

        x = _mm_mullo_epi32(u1, cospi48);
        y = _mm_mullo_epi32(u3, cospim16);
        v2 = _mm_add_epi32(x, y);
        v2 = _mm_add_epi32(v2, rnding);
        v2 = _mm_srai_epi32(v2, bit);

        x = _mm_mullo_epi32(u1, cospi16);
        y = _mm_mullo_epi32(u3, cospi48);
        v3 = _mm_add_epi32(x, y);
        v3 = _mm_add_epi32(v3, rnding);
        v3 = _mm_srai_epi32(v3, bit);

        in[0] = _mm_add_epi32(v0, v3);
        in[1] = _mm_add_epi32(v1, v2);
        in[2] = _mm_sub_epi32(v1, v2);
        in[3] = _mm_sub_epi32(v0, v3);
    }

//  16384 * sqrt(2) * sin(kPi/9) * 2 / 3
typedef long long          int64_t;
typedef int64_t tran_high_t;

    const int32_t av1_sinpi_arr_data[7][5] = {
        { 0, 330, 621, 836, 951 },        { 0, 660, 1241, 1672, 1901 },
        { 0, 1321, 2482, 3344, 3803 },    { 0, 2642, 4964, 6689, 7606 },
        { 0, 5283, 9929, 13377, 15212 },  { 0, 10566, 19858, 26755, 30424 },
        { 0, 21133, 39716, 53510, 60849 }
    };

    static INLINE const int32_t *sinpi_arr(int n) {
        return av1_sinpi_arr_data[n - cos_bit_min];
    }

    static void iadst4x4_sse4_1(__m128i *in, int bit) {
        const int32_t *sinpi = sinpi_arr(bit);
        const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
        const __m128i sinpi1 = _mm_set1_epi32((int)sinpi[1]);
        const __m128i sinpi2 = _mm_set1_epi32((int)sinpi[2]);
        const __m128i sinpi3 = _mm_set1_epi32((int)sinpi[3]);
        const __m128i sinpi4 = _mm_set1_epi32((int)sinpi[4]);
        __m128i t;
        __m128i s0, s1, s2, s3, s4, s5, s6, s7;
        __m128i x0, x1, x2, x3;
        __m128i u0, u1, u2, u3;
        __m128i v0, v1, v2, v3;

        v0 = _mm_unpacklo_epi32(in[0], in[1]);
        v1 = _mm_unpackhi_epi32(in[0], in[1]);
        v2 = _mm_unpacklo_epi32(in[2], in[3]);
        v3 = _mm_unpackhi_epi32(in[2], in[3]);

        x0 = _mm_unpacklo_epi64(v0, v2);
        x1 = _mm_unpackhi_epi64(v0, v2);
        x2 = _mm_unpacklo_epi64(v1, v3);
        x3 = _mm_unpackhi_epi64(v1, v3);

        s0 = _mm_mullo_epi32(x0, sinpi1);
        s1 = _mm_mullo_epi32(x0, sinpi2);
        s2 = _mm_mullo_epi32(x1, sinpi3);
        s3 = _mm_mullo_epi32(x2, sinpi4);
        s4 = _mm_mullo_epi32(x2, sinpi1);
        s5 = _mm_mullo_epi32(x3, sinpi2);
        s6 = _mm_mullo_epi32(x3, sinpi4);
        t = _mm_sub_epi32(x0, x2);
        s7 = _mm_add_epi32(t, x3);

        t = _mm_add_epi32(s0, s3);
        s0 = _mm_add_epi32(t, s5);
        t = _mm_sub_epi32(s1, s4);
        s1 = _mm_sub_epi32(t, s6);
        s3 = s2;
        s2 = _mm_mullo_epi32(s7, sinpi3);

        u0 = _mm_add_epi32(s0, s3);
        u1 = _mm_add_epi32(s1, s3);
        u2 = s2;
        t = _mm_add_epi32(s0, s1);
        u3 = _mm_sub_epi32(t, s3);

        u0 = _mm_add_epi32(u0, rnding);
        u0 = _mm_srai_epi32(u0, bit);

        u1 = _mm_add_epi32(u1, rnding);
        u1 = _mm_srai_epi32(u1, bit);

        u2 = _mm_add_epi32(u2, rnding);
        u2 = _mm_srai_epi32(u2, bit);

        u3 = _mm_add_epi32(u3, rnding);
        u3 = _mm_srai_epi32(u3, bit);

        in[0] = u0;
        in[1] = u1;
        in[2] = u2;
        in[3] = u3;
    }

    static INLINE void round_shift_4x4(__m128i *in, int shift) {
        __m128i rnding = _mm_set1_epi32(1 << (shift - 1));

        in[0] = _mm_add_epi32(in[0], rnding);
        in[1] = _mm_add_epi32(in[1], rnding);
        in[2] = _mm_add_epi32(in[2], rnding);
        in[3] = _mm_add_epi32(in[3], rnding);

        in[0] = _mm_srai_epi32(in[0], shift);
        in[1] = _mm_srai_epi32(in[1], shift);
        in[2] = _mm_srai_epi32(in[2], shift);
        in[3] = _mm_srai_epi32(in[3], shift);
    }

    static INLINE __m128i highbd_clamp_epi16(__m128i u, int bd) {
        const __m128i zero = _mm_setzero_si128();
        const __m128i one = _mm_set1_epi16(1);
        const __m128i max = _mm_sub_epi16(_mm_slli_epi16(one, bd), one);
        __m128i clamped, mask;

        mask = _mm_cmpgt_epi16(u, max);
        clamped = _mm_andnot_si128(mask, u);
        mask = _mm_and_si128(mask, max);
        clamped = _mm_or_si128(mask, clamped);
        mask = _mm_cmpgt_epi16(clamped, zero);
        clamped = _mm_and_si128(clamped, mask);

        return clamped;
    }

    static void write_buffer_4x4_noclamp(__m128i *in, uint16_t *output, int stride, int fliplr, int flipud, int shift, int bd) {
        bd;
        const __m128i zero = _mm_setzero_si128();
        __m128i u0, u1, u2, u3;
        __m128i v0, v1, v2, v3;

        round_shift_4x4(in, shift);

        v0 = zero;//_mm_loadl_epi64((__m128i const *)(output + 0 * stride));
        v1 = zero;//_mm_loadl_epi64((__m128i const *)(output + 1 * stride));
        v2 = zero;//_mm_loadl_epi64((__m128i const *)(output + 2 * stride));
        v3 = zero;//_mm_loadl_epi64((__m128i const *)(output + 3 * stride));

        v0 = _mm_unpacklo_epi16(v0, zero);
        v1 = _mm_unpacklo_epi16(v1, zero);
        v2 = _mm_unpacklo_epi16(v2, zero);
        v3 = _mm_unpacklo_epi16(v3, zero);

        if (fliplr) {
            in[0] = _mm_shuffle_epi32(in[0], 0x1B);
            in[1] = _mm_shuffle_epi32(in[1], 0x1B);
            in[2] = _mm_shuffle_epi32(in[2], 0x1B);
            in[3] = _mm_shuffle_epi32(in[3], 0x1B);
        }

        if (flipud) {
            u0 = _mm_add_epi32(in[3], v0);
            u1 = _mm_add_epi32(in[2], v1);
            u2 = _mm_add_epi32(in[1], v2);
            u3 = _mm_add_epi32(in[0], v3);
        } else {
            u0 = _mm_add_epi32(in[0], v0);
            u1 = _mm_add_epi32(in[1], v1);
            u2 = _mm_add_epi32(in[2], v2);
            u3 = _mm_add_epi32(in[3], v3);
        }

        v0 = _mm_packs_epi32(u0, u1);
        v2 = _mm_packs_epi32(u2, u3);

        u0 = v0;//highbd_clamp_epi16(v0, bd);
        u2 = v2;//highbd_clamp_epi16(v2, bd);

        v0 = _mm_unpacklo_epi64(u0, u0);
        v1 = _mm_unpackhi_epi64(u0, u0);
        v2 = _mm_unpacklo_epi64(u2, u2);
        v3 = _mm_unpackhi_epi64(u2, u2);

        _mm_storel_epi64((__m128i *)(output + 0 * stride), v0);
        _mm_storel_epi64((__m128i *)(output + 1 * stride), v1);
        _mm_storel_epi64((__m128i *)(output + 2 * stride), v2);
        _mm_storel_epi64((__m128i *)(output + 3 * stride), v3);
    }

    static void write_buffer_4x4_view_dst_as_8u(__m128i *in, uint16_t *output, int stride, int fliplr, int flipud, int shift, int bd)
    {
        const __m128i zero = _mm_setzero_si128();
        __m128i u0, u1, u2, u3;
        __m128i v0, v1, v2, v3;

        round_shift_4x4(in, shift);

        uint8_t* out_8 = (uint8_t*)output;
        int out8Pitch = stride;

        __m128i d0 = _mm_cvtsi32_si128(*(const int *)(out_8 + out8Pitch * 0));
        __m128i d1 = _mm_cvtsi32_si128(*(const int *)(out_8 + out8Pitch * 1));
        __m128i d2 = _mm_cvtsi32_si128(*(const int *)(out_8 + out8Pitch * 2));
        __m128i d3 = _mm_cvtsi32_si128(*(const int *)(out_8 + out8Pitch * 3));
        d0 = _mm_unpacklo_epi8(d0, zero);
        v0 = _mm_unpacklo_epi16(d0, zero);
        d1 = _mm_unpacklo_epi8(d1, zero);
        v1 = _mm_unpacklo_epi16(d1, zero);
        d2 = _mm_unpacklo_epi8(d2, zero);
        v2 = _mm_unpacklo_epi16(d2, zero);
        d3 = _mm_unpacklo_epi8(d3, zero);
        v3 = _mm_unpacklo_epi16(d3, zero);

        if (fliplr) {
            in[0] = _mm_shuffle_epi32(in[0], 0x1B);
            in[1] = _mm_shuffle_epi32(in[1], 0x1B);
            in[2] = _mm_shuffle_epi32(in[2], 0x1B);
            in[3] = _mm_shuffle_epi32(in[3], 0x1B);
        }

        if (flipud) {
            u0 = _mm_add_epi32(in[3], v0);
            u1 = _mm_add_epi32(in[2], v1);
            u2 = _mm_add_epi32(in[1], v2);
            u3 = _mm_add_epi32(in[0], v3);
        } else {
            u0 = _mm_add_epi32(in[0], v0);
            u1 = _mm_add_epi32(in[1], v1);
            u2 = _mm_add_epi32(in[2], v2);
            u3 = _mm_add_epi32(in[3], v3);
        }

        v0 = _mm_packus_epi32(u0, u1);
        v2 = _mm_packus_epi32(u2, u3);

        u0 = highbd_clamp_epi16(v0, bd);
        u2 = highbd_clamp_epi16(v2, bd);

        v0 = _mm_unpacklo_epi64(u0, u0);
        v1 = _mm_unpackhi_epi64(u0, u0);
        v2 = _mm_unpacklo_epi64(u2, u2);
        v3 = _mm_unpackhi_epi64(u2, u2);

        *(int *)(out_8+0*out8Pitch) = _mm_cvtsi128_si32(_mm_packus_epi16(v0, v0));
        *(int *)(out_8+1*out8Pitch) = _mm_cvtsi128_si32(_mm_packus_epi16(v1, v1));
        *(int *)(out_8+2*out8Pitch) = _mm_cvtsi128_si32(_mm_packus_epi16(v2, v2));
        *(int *)(out_8+3*out8Pitch) = _mm_cvtsi128_si32(_mm_packus_epi16(v3, v3));
    }

    void av1_inv_txfm2d_add_4x4_sse4_1(const int32_t *coeff, uint16_t *output, int stride, int tx_type, int bd, int useAdd)
    {
        __m128i in[4];
        const int8_t *shift = inv_txfm_shift_ls[TX_4X4];
        const int txw_idx = TX_4X4;
        const int txh_idx = TX_4X4;

        switch (tx_type) {
        case DCT_DCT:
            load_buffer_4x4_view_src_as_16s(coeff, in);
            idct4x4_sse4_1(in, inv_cos_bit_row[txw_idx][txh_idx]);
            idct4x4_sse4_1(in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_4x4_view_dst_as_8u(in, output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_4x4_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
        case ADST_DCT:
            load_buffer_4x4_view_src_as_16s(coeff, in);
            idct4x4_sse4_1(in, inv_cos_bit_row[txw_idx][txh_idx]);
            iadst4x4_sse4_1(in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_4x4_view_dst_as_8u(in, output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_4x4_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
        case DCT_ADST:
            load_buffer_4x4_view_src_as_16s(coeff, in);
            iadst4x4_sse4_1(in, inv_cos_bit_row[txw_idx][txh_idx]);
            idct4x4_sse4_1(in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_4x4_view_dst_as_8u(in, output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_4x4_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
        case ADST_ADST:
            load_buffer_4x4_view_src_as_16s(coeff, in);
            iadst4x4_sse4_1(in, inv_cos_bit_row[txw_idx][txh_idx]);
            iadst4x4_sse4_1(in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_4x4_view_dst_as_8u(in, output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_4x4_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
#if 0
        case FLIPADST_DCT:
            row_cfg = &inv_txfm_1d_row_cfg_dct_4;
            col_cfg = &inv_txfm_1d_col_cfg_adst_4;
            load_buffer_4x4(coeff, in);
            idct4x4_sse4_1(in, row_cfg->cos_bit[2]);
            iadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
            write_buffer_4x4(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
            break;
        case DCT_FLIPADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_4;
            col_cfg = &inv_txfm_1d_col_cfg_dct_4;
            load_buffer_4x4(coeff, in);
            iadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
            idct4x4_sse4_1(in, col_cfg->cos_bit[2]);
            write_buffer_4x4(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
            break;
        case FLIPADST_FLIPADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_4;
            col_cfg = &inv_txfm_1d_col_cfg_adst_4;
            load_buffer_4x4(coeff, in);
            iadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
            iadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
            write_buffer_4x4(in, output, stride, 1, 1, -row_cfg->shift[1], bd);
            break;
        case ADST_FLIPADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_4;
            col_cfg = &inv_txfm_1d_col_cfg_adst_4;
            load_buffer_4x4(coeff, in);
            iadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
            iadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
            write_buffer_4x4(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
            break;
        case FLIPADST_ADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_4;
            col_cfg = &inv_txfm_1d_col_cfg_adst_4;
            load_buffer_4x4(coeff, in);
            iadst4x4_sse4_1(in, row_cfg->cos_bit[2]);
            iadst4x4_sse4_1(in, col_cfg->cos_bit[2]);
            write_buffer_4x4(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
            break;
#endif  // 0
        default: assert(0);
        }
    }

    // 8x8
    static void load_buffer_8x8(const int32_t *coeff, __m128i *in) {
        in[0] = _mm_load_si128((const __m128i *)(coeff + 0));
        in[1] = _mm_load_si128((const __m128i *)(coeff + 4));
        in[2] = _mm_load_si128((const __m128i *)(coeff + 8));
        in[3] = _mm_load_si128((const __m128i *)(coeff + 12));
        in[4] = _mm_load_si128((const __m128i *)(coeff + 16));
        in[5] = _mm_load_si128((const __m128i *)(coeff + 20));
        in[6] = _mm_load_si128((const __m128i *)(coeff + 24));
        in[7] = _mm_load_si128((const __m128i *)(coeff + 28));
        in[8] = _mm_load_si128((const __m128i *)(coeff + 32));
        in[9] = _mm_load_si128((const __m128i *)(coeff + 36));
        in[10] = _mm_load_si128((const __m128i *)(coeff + 40));
        in[11] = _mm_load_si128((const __m128i *)(coeff + 44));
        in[12] = _mm_load_si128((const __m128i *)(coeff + 48));
        in[13] = _mm_load_si128((const __m128i *)(coeff + 52));
        in[14] = _mm_load_si128((const __m128i *)(coeff + 56));
        in[15] = _mm_load_si128((const __m128i *)(coeff + 60));
    }

    static void load_buffer_8x8_view_src_as_16s(const int32_t *coeff32, __m128i *in) {
        const int16_t* coeff = (const int16_t*)coeff32;

        in[0] = _mm_loadl_epi64((const __m128i *)(coeff + 0));
        in[0] = _mm_cvtepi16_epi32 (in[0]);
        in[1] = _mm_loadl_epi64((const __m128i *)(coeff + 4));
        in[1] = _mm_cvtepi16_epi32 (in[1]);
        in[2] = _mm_loadl_epi64((const __m128i *)(coeff + 8));
        in[2] = _mm_cvtepi16_epi32 (in[2]);
        in[3] = _mm_loadl_epi64((const __m128i *)(coeff + 12));
        in[3] = _mm_cvtepi16_epi32 (in[3]);
        in[4] = _mm_loadl_epi64((const __m128i *)(coeff + 16));
        in[4] = _mm_cvtepi16_epi32 (in[4]);
        in[5] = _mm_loadl_epi64((const __m128i *)(coeff + 20));
        in[5] = _mm_cvtepi16_epi32 (in[5]);
        in[6] = _mm_loadl_epi64((const __m128i *)(coeff + 24));
        in[6] = _mm_cvtepi16_epi32 (in[6]);
        in[7] = _mm_loadl_epi64((const __m128i *)(coeff + 28));
        in[7] = _mm_cvtepi16_epi32 (in[7]);
        in[8] = _mm_loadl_epi64((const __m128i *)(coeff + 32));
        in[8] = _mm_cvtepi16_epi32 (in[8]);
        in[9] = _mm_loadl_epi64((const __m128i *)(coeff + 36));
        in[9] = _mm_cvtepi16_epi32 (in[9]);
        in[10] = _mm_loadl_epi64((const __m128i *)(coeff + 40));
        in[10] = _mm_cvtepi16_epi32 (in[10]);
        in[11] = _mm_loadl_epi64((const __m128i *)(coeff + 44));
        in[11] = _mm_cvtepi16_epi32 (in[11]);
        in[12] = _mm_loadl_epi64((const __m128i *)(coeff + 48));
        in[12] = _mm_cvtepi16_epi32 (in[12]);
        in[13] = _mm_loadl_epi64((const __m128i *)(coeff + 52));
        in[13] = _mm_cvtepi16_epi32 (in[13]);
        in[14] = _mm_loadl_epi64((const __m128i *)(coeff + 56));
        in[14] = _mm_cvtepi16_epi32 (in[14]);
        in[15] = _mm_loadl_epi64((const __m128i *)(coeff + 60));
        in[15] = _mm_cvtepi16_epi32 (in[15]);
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

    static INLINE void transpose_8x8(const __m128i *in, __m128i *out)
    {
        TRANSPOSE_4X4(in[0], in[2], in[4], in[6], out[0], out[2], out[4], out[6]);
        TRANSPOSE_4X4(in[1], in[3], in[5], in[7], out[8], out[10], out[12], out[14]);
        TRANSPOSE_4X4(in[8], in[10], in[12], in[14], out[1], out[3], out[5], out[7]);
        TRANSPOSE_4X4(in[9], in[11], in[13], in[15], out[9], out[11], out[13], out[15]);
    }

    static void idct8x8_sse4_1(__m128i *in, __m128i *out, int bit)
    {
        const int32_t *cospi = cospi_arr(bit);
        const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
        const __m128i cospim8 = _mm_set1_epi32(-cospi[8]);
        const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
        const __m128i cospim40 = _mm_set1_epi32(-cospi[40]);
        const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
        const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
        const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
        const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
        const __m128i cospim16 = _mm_set1_epi32(-cospi[16]);
        const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
        const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
        __m128i u0, u1, u2, u3, u4, u5, u6, u7;
        __m128i v0, v1, v2, v3, v4, v5, v6, v7;
        __m128i x, y;
        int col;

        // Note:
        //  Even column: 0, 2, ..., 14
        //  Odd column: 1, 3, ..., 15
        //  one even column plus one odd column constructs one row (8 coeffs)
        //  total we have 8 rows (8x8).
        for (col = 0; col < 2; ++col) {
            // stage 0
            // stage 1
            // stage 2
            u0 = in[0 * 2 + col];
            u1 = in[4 * 2 + col];
            u2 = in[2 * 2 + col];
            u3 = in[6 * 2 + col];

            x = _mm_mullo_epi32(in[1 * 2 + col], cospi56);
            y = _mm_mullo_epi32(in[7 * 2 + col], cospim8);
            u4 = _mm_add_epi32(x, y);
            u4 = _mm_add_epi32(u4, rnding);
            u4 = _mm_srai_epi32(u4, bit);

            x = _mm_mullo_epi32(in[1 * 2 + col], cospi8);
            y = _mm_mullo_epi32(in[7 * 2 + col], cospi56);
            u7 = _mm_add_epi32(x, y);
            u7 = _mm_add_epi32(u7, rnding);
            u7 = _mm_srai_epi32(u7, bit);

            x = _mm_mullo_epi32(in[5 * 2 + col], cospi24);
            y = _mm_mullo_epi32(in[3 * 2 + col], cospim40);
            u5 = _mm_add_epi32(x, y);
            u5 = _mm_add_epi32(u5, rnding);
            u5 = _mm_srai_epi32(u5, bit);

            x = _mm_mullo_epi32(in[5 * 2 + col], cospi40);
            y = _mm_mullo_epi32(in[3 * 2 + col], cospi24);
            u6 = _mm_add_epi32(x, y);
            u6 = _mm_add_epi32(u6, rnding);
            u6 = _mm_srai_epi32(u6, bit);

            // stage 3
            x = _mm_mullo_epi32(u0, cospi32);
            y = _mm_mullo_epi32(u1, cospi32);
            v0 = _mm_add_epi32(x, y);
            v0 = _mm_add_epi32(v0, rnding);
            v0 = _mm_srai_epi32(v0, bit);

            v1 = _mm_sub_epi32(x, y);
            v1 = _mm_add_epi32(v1, rnding);
            v1 = _mm_srai_epi32(v1, bit);

            x = _mm_mullo_epi32(u2, cospi48);
            y = _mm_mullo_epi32(u3, cospim16);
            v2 = _mm_add_epi32(x, y);
            v2 = _mm_add_epi32(v2, rnding);
            v2 = _mm_srai_epi32(v2, bit);

            x = _mm_mullo_epi32(u2, cospi16);
            y = _mm_mullo_epi32(u3, cospi48);
            v3 = _mm_add_epi32(x, y);
            v3 = _mm_add_epi32(v3, rnding);
            v3 = _mm_srai_epi32(v3, bit);

            v4 = _mm_add_epi32(u4, u5);
            v5 = _mm_sub_epi32(u4, u5);
            v6 = _mm_sub_epi32(u7, u6);
            v7 = _mm_add_epi32(u6, u7);

            // stage 4
            u0 = _mm_add_epi32(v0, v3);
            u1 = _mm_add_epi32(v1, v2);
            u2 = _mm_sub_epi32(v1, v2);
            u3 = _mm_sub_epi32(v0, v3);
            u4 = v4;
            u7 = v7;

            x = _mm_mullo_epi32(v5, cospi32);
            y = _mm_mullo_epi32(v6, cospi32);
            u6 = _mm_add_epi32(y, x);
            u6 = _mm_add_epi32(u6, rnding);
            u6 = _mm_srai_epi32(u6, bit);

            u5 = _mm_sub_epi32(y, x);
            u5 = _mm_add_epi32(u5, rnding);
            u5 = _mm_srai_epi32(u5, bit);

            // stage 5
            out[0 * 2 + col] = _mm_add_epi32(u0, u7);
            out[1 * 2 + col] = _mm_add_epi32(u1, u6);
            out[2 * 2 + col] = _mm_add_epi32(u2, u5);
            out[3 * 2 + col] = _mm_add_epi32(u3, u4);
            out[4 * 2 + col] = _mm_sub_epi32(u3, u4);
            out[5 * 2 + col] = _mm_sub_epi32(u2, u5);
            out[6 * 2 + col] = _mm_sub_epi32(u1, u6);
            out[7 * 2 + col] = _mm_sub_epi32(u0, u7);
        }
    }

    static void round_shift_8x8(__m128i *in, int shift) {
        round_shift_4x4(&in[0], shift);
        round_shift_4x4(&in[4], shift);
        round_shift_4x4(&in[8], shift);
        round_shift_4x4(&in[12], shift);
    }

    static __m128i get_recon_8x8(const __m128i pred, __m128i res_lo, __m128i res_hi, int fliplr, int bd)
    {
        __m128i x0, x1;
        const __m128i zero = _mm_setzero_si128();

        x0 = _mm_unpacklo_epi16(pred, zero);
        x1 = _mm_unpackhi_epi16(pred, zero);

        if (fliplr) {
            res_lo = _mm_shuffle_epi32(res_lo, 0x1B);
            res_hi = _mm_shuffle_epi32(res_hi, 0x1B);
            x0 = _mm_add_epi32(res_hi, x0);
            x1 = _mm_add_epi32(res_lo, x1);

        } else {
            x0 = _mm_add_epi32(res_lo, x0);
            x1 = _mm_add_epi32(res_hi, x1);
        }

        x0 = _mm_packus_epi32(x0, x1);
        return highbd_clamp_epi16(x0, bd);
    }

    static __m128i get_recon_8x8_noclamp(const __m128i pred, __m128i res_lo, __m128i res_hi, int fliplr, int bd)
    {
        bd;
        __m128i x0, x1;
        const __m128i zero = _mm_setzero_si128();

        x0 = _mm_unpacklo_epi16(pred, zero);
        x1 = _mm_unpackhi_epi16(pred, zero);

        if (fliplr) {
            res_lo = _mm_shuffle_epi32(res_lo, 0x1B);
            res_hi = _mm_shuffle_epi32(res_hi, 0x1B);
            x0 = _mm_add_epi32(res_hi, x0);
            x1 = _mm_add_epi32(res_lo, x1);

        } else {
            x0 = _mm_add_epi32(res_lo, x0);
            x1 = _mm_add_epi32(res_hi, x1);
        }

        x0 = _mm_packs_epi32(x0, x1);
        return x0/*highbd_clamp_epi16(x0, bd)*/;
    }

    static void write_buffer_8x8(__m128i *in, uint16_t *output, int stride,
        int fliplr, int flipud, int shift, int bd) {
            __m128i u0, u1, u2, u3, u4, u5, u6, u7;
            __m128i v0, v1, v2, v3, v4, v5, v6, v7;

            round_shift_8x8(in, shift);

            v0 = _mm_load_si128((__m128i const *)(output + 0 * stride));
            v1 = _mm_load_si128((__m128i const *)(output + 1 * stride));
            v2 = _mm_load_si128((__m128i const *)(output + 2 * stride));
            v3 = _mm_load_si128((__m128i const *)(output + 3 * stride));
            v4 = _mm_load_si128((__m128i const *)(output + 4 * stride));
            v5 = _mm_load_si128((__m128i const *)(output + 5 * stride));
            v6 = _mm_load_si128((__m128i const *)(output + 6 * stride));
            v7 = _mm_load_si128((__m128i const *)(output + 7 * stride));

            if (flipud) {
                u0 = get_recon_8x8(v0, in[14], in[15], fliplr, bd);
                u1 = get_recon_8x8(v1, in[12], in[13], fliplr, bd);
                u2 = get_recon_8x8(v2, in[10], in[11], fliplr, bd);
                u3 = get_recon_8x8(v3, in[8], in[9], fliplr, bd);
                u4 = get_recon_8x8(v4, in[6], in[7], fliplr, bd);
                u5 = get_recon_8x8(v5, in[4], in[5], fliplr, bd);
                u6 = get_recon_8x8(v6, in[2], in[3], fliplr, bd);
                u7 = get_recon_8x8(v7, in[0], in[1], fliplr, bd);
            } else {
                u0 = get_recon_8x8(v0, in[0], in[1], fliplr, bd);
                u1 = get_recon_8x8(v1, in[2], in[3], fliplr, bd);
                u2 = get_recon_8x8(v2, in[4], in[5], fliplr, bd);
                u3 = get_recon_8x8(v3, in[6], in[7], fliplr, bd);
                u4 = get_recon_8x8(v4, in[8], in[9], fliplr, bd);
                u5 = get_recon_8x8(v5, in[10], in[11], fliplr, bd);
                u6 = get_recon_8x8(v6, in[12], in[13], fliplr, bd);
                u7 = get_recon_8x8(v7, in[14], in[15], fliplr, bd);
            }

            _mm_store_si128((__m128i *)(output + 0 * stride), u0);
            _mm_store_si128((__m128i *)(output + 1 * stride), u1);
            _mm_store_si128((__m128i *)(output + 2 * stride), u2);
            _mm_store_si128((__m128i *)(output + 3 * stride), u3);
            _mm_store_si128((__m128i *)(output + 4 * stride), u4);
            _mm_store_si128((__m128i *)(output + 5 * stride), u5);
            _mm_store_si128((__m128i *)(output + 6 * stride), u6);
            _mm_store_si128((__m128i *)(output + 7 * stride), u7);
    }

    static void write_buffer_8x8_noclamp(__m128i *in, uint16_t *output, int stride,
        int fliplr, int flipud, int shift, int bd) {
            __m128i u0, u1, u2, u3, u4, u5, u6, u7;
            __m128i v0, v1, v2, v3, v4, v5, v6, v7;

            round_shift_8x8(in, shift);

            const __m128i zero = _mm_setzero_si128();

            v0 = zero;//_mm_load_si128((__m128i const *)(output + 0 * stride));
            v1 = zero;//_mm_load_si128((__m128i const *)(output + 1 * stride));
            v2 = zero;//_mm_load_si128((__m128i const *)(output + 2 * stride));
            v3 = zero;//_mm_load_si128((__m128i const *)(output + 3 * stride));
            v4 = zero;//_mm_load_si128((__m128i const *)(output + 4 * stride));
            v5 = zero;//_mm_load_si128((__m128i const *)(output + 5 * stride));
            v6 = zero;//_mm_load_si128((__m128i const *)(output + 6 * stride));
            v7 = zero;//_mm_load_si128((__m128i const *)(output + 7 * stride));

            if (flipud) {
                u0 = get_recon_8x8(v0, in[14], in[15], fliplr, bd);
                u1 = get_recon_8x8(v1, in[12], in[13], fliplr, bd);
                u2 = get_recon_8x8(v2, in[10], in[11], fliplr, bd);
                u3 = get_recon_8x8(v3, in[8], in[9], fliplr, bd);
                u4 = get_recon_8x8(v4, in[6], in[7], fliplr, bd);
                u5 = get_recon_8x8(v5, in[4], in[5], fliplr, bd);
                u6 = get_recon_8x8(v6, in[2], in[3], fliplr, bd);
                u7 = get_recon_8x8(v7, in[0], in[1], fliplr, bd);
            } else {
                u0 = get_recon_8x8_noclamp(v0, in[0], in[1], fliplr, bd);
                u1 = get_recon_8x8_noclamp(v1, in[2], in[3], fliplr, bd);
                u2 = get_recon_8x8_noclamp(v2, in[4], in[5], fliplr, bd);
                u3 = get_recon_8x8_noclamp(v3, in[6], in[7], fliplr, bd);
                u4 = get_recon_8x8_noclamp(v4, in[8], in[9], fliplr, bd);
                u5 = get_recon_8x8_noclamp(v5, in[10], in[11], fliplr, bd);
                u6 = get_recon_8x8_noclamp(v6, in[12], in[13], fliplr, bd);
                u7 = get_recon_8x8_noclamp(v7, in[14], in[15], fliplr, bd);
            }

            _mm_store_si128((__m128i *)(output + 0 * stride), u0);
            _mm_store_si128((__m128i *)(output + 1 * stride), u1);
            _mm_store_si128((__m128i *)(output + 2 * stride), u2);
            _mm_store_si128((__m128i *)(output + 3 * stride), u3);
            _mm_store_si128((__m128i *)(output + 4 * stride), u4);
            _mm_store_si128((__m128i *)(output + 5 * stride), u5);
            _mm_store_si128((__m128i *)(output + 6 * stride), u6);
            _mm_store_si128((__m128i *)(output + 7 * stride), u7);
    }

    static void write_buffer_8x8_view_dst_as_8u(__m128i *in, uint16_t *output, int stride,
        int fliplr, int flipud, int shift, int bd) {
            __m128i u0, u1, u2, u3, u4, u5, u6, u7;
            __m128i v0, v1, v2, v3, v4, v5, v6, v7;

            round_shift_8x8(in, shift);
            const __m128i zero = _mm_setzero_si128();
            uint8_t* output8 = (uint8_t*)output;
            int stride8 = stride;

            v0 = _mm_unpacklo_epi8 (_mm_loadl_epi64((__m128i const *)((uint8_t*)output8 + 0 * stride8)), zero);
            v1 = _mm_unpacklo_epi8 (_mm_loadl_epi64((__m128i const *)((uint8_t*)output8 + 1 * stride8)), zero);
            v2 = _mm_unpacklo_epi8 (_mm_loadl_epi64((__m128i const *)((uint8_t*)output8 + 2 * stride8)), zero);
            v3 = _mm_unpacklo_epi8 (_mm_loadl_epi64((__m128i const *)((uint8_t*)output8 + 3 * stride8)), zero);
            v4 = _mm_unpacklo_epi8 (_mm_loadl_epi64((__m128i const *)((uint8_t*)output8 + 4 * stride8)), zero);
            v5 = _mm_unpacklo_epi8 (_mm_loadl_epi64((__m128i const *)((uint8_t*)output8 + 5 * stride8)), zero);
            v6 = _mm_unpacklo_epi8 (_mm_loadl_epi64((__m128i const *)((uint8_t*)output8 + 6 * stride8)), zero);
            v7 = _mm_unpacklo_epi8 (_mm_loadl_epi64((__m128i const *)((uint8_t*)output8 + 7 * stride8)), zero);

            if (flipud) {
                u0 = get_recon_8x8(v0, in[14], in[15], fliplr, bd);
                u1 = get_recon_8x8(v1, in[12], in[13], fliplr, bd);
                u2 = get_recon_8x8(v2, in[10], in[11], fliplr, bd);
                u3 = get_recon_8x8(v3, in[8], in[9], fliplr, bd);
                u4 = get_recon_8x8(v4, in[6], in[7], fliplr, bd);
                u5 = get_recon_8x8(v5, in[4], in[5], fliplr, bd);
                u6 = get_recon_8x8(v6, in[2], in[3], fliplr, bd);
                u7 = get_recon_8x8(v7, in[0], in[1], fliplr, bd);
            } else {
                u0 = get_recon_8x8(v0, in[0], in[1], fliplr, bd);
                u1 = get_recon_8x8(v1, in[2], in[3], fliplr, bd);
                u2 = get_recon_8x8(v2, in[4], in[5], fliplr, bd);
                u3 = get_recon_8x8(v3, in[6], in[7], fliplr, bd);
                u4 = get_recon_8x8(v4, in[8], in[9], fliplr, bd);
                u5 = get_recon_8x8(v5, in[10], in[11], fliplr, bd);
                u6 = get_recon_8x8(v6, in[12], in[13], fliplr, bd);
                u7 = get_recon_8x8(v7, in[14], in[15], fliplr, bd);
            }

            u0 = _mm_packus_epi16(u0, u0);
            _mm_storel_epi64((__m128i *)((uint8_t*)output8 + 0 * stride8), u0);
            u1 = _mm_packus_epi16(u1, u1);
            _mm_storel_epi64((__m128i *)((uint8_t*)output8 + 1 * stride8), u1);
            u2 = _mm_packus_epi16(u2, u2);
            _mm_storel_epi64((__m128i *)((uint8_t*)output8 + 2 * stride8), u2);
            u3 = _mm_packus_epi16(u3, u3);
            _mm_storel_epi64((__m128i *)((uint8_t*)output8 + 3 * stride8), u3);
            u4 = _mm_packus_epi16(u4, u4);
            _mm_storel_epi64((__m128i *)((uint8_t*)output8 + 4 * stride8), u4);
            u5 = _mm_packus_epi16(u5, u5);
            _mm_storel_epi64((__m128i *)((uint8_t*)output8 + 5 * stride8), u5);
            u6 = _mm_packus_epi16(u6, u6);
            _mm_storel_epi64((__m128i *)((uint8_t*)output8 + 6 * stride8), u6);
            u7 = _mm_packus_epi16(u7, u7);
            _mm_storel_epi64((__m128i *)((uint8_t*)output8 + 7 * stride8), u7);
    }

    static void iadst8x8_sse4_1(__m128i *in, __m128i *out, int bit)
    {
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


    void av1_inv_txfm2d_add_8x8_sse4_1(const int32_t *coeff, uint16_t *output, int stride, int tx_type, int bd, int useAdd)
    {
        __m128i in[16], out[16];
        const int8_t *shift = inv_txfm_shift_ls[TX_8X8];
        const int txw_idx = TX_8X8;
        const int txh_idx = TX_8X8;

        switch (tx_type) {
        case DCT_DCT:
            load_buffer_8x8_view_src_as_16s(coeff, in);
            transpose_8x8(in, out);
            idct8x8_sse4_1(out, in, inv_cos_bit_row[txw_idx][txh_idx]);
            transpose_8x8(in, out);
            round_shift_8x8(out, -shift[0]);
            idct8x8_sse4_1(out, in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_8x8_view_dst_as_8u(in, output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_8x8_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
        case DCT_ADST:
            load_buffer_8x8_view_src_as_16s(coeff, in);
            transpose_8x8(in, out);
            iadst8x8_sse4_1(out, in, inv_cos_bit_row[txw_idx][txh_idx]);
            transpose_8x8(in, out);
            round_shift_8x8(out, -shift[0]);
            idct8x8_sse4_1(out, in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_8x8_view_dst_as_8u(in, output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_8x8_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
        case ADST_DCT:
            load_buffer_8x8_view_src_as_16s(coeff, in);
            transpose_8x8(in, out);
            idct8x8_sse4_1(out, in, inv_cos_bit_row[txw_idx][txh_idx]);
            transpose_8x8(in, out);
            round_shift_8x8(out, -shift[0]);
            iadst8x8_sse4_1(out, in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_8x8_view_dst_as_8u(in, output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_8x8_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
        case ADST_ADST:
            load_buffer_8x8_view_src_as_16s(coeff, in);
            transpose_8x8(in, out);
            iadst8x8_sse4_1(out, in, inv_cos_bit_row[txw_idx][txh_idx]);
            transpose_8x8(in, out);
            round_shift_8x8(out, -shift[0]);
            iadst8x8_sse4_1(out, in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_8x8_view_dst_as_8u(in, output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_8x8_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
#if 0
        case FLIPADST_DCT:
            row_cfg = &inv_txfm_1d_row_cfg_dct_8;
            col_cfg = &inv_txfm_1d_col_cfg_adst_8;
            load_buffer_8x8(coeff, in);
            transpose_8x8(in, out);
            idct8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
            transpose_8x8(in, out);
            iadst8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
            write_buffer_8x8(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
            break;
        case DCT_FLIPADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_8;
            col_cfg = &inv_txfm_1d_col_cfg_dct_8;
            load_buffer_8x8(coeff, in);
            transpose_8x8(in, out);
            iadst8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
            transpose_8x8(in, out);
            idct8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
            write_buffer_8x8(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
            break;
        case ADST_FLIPADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_8;
            col_cfg = &inv_txfm_1d_col_cfg_adst_8;
            load_buffer_8x8(coeff, in);
            transpose_8x8(in, out);
            iadst8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
            transpose_8x8(in, out);
            iadst8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
            write_buffer_8x8(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
            break;
        case FLIPADST_FLIPADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_8;
            col_cfg = &inv_txfm_1d_col_cfg_adst_8;
            load_buffer_8x8(coeff, in);
            transpose_8x8(in, out);
            iadst8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
            transpose_8x8(in, out);
            iadst8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
            write_buffer_8x8(in, output, stride, 1, 1, -row_cfg->shift[1], bd);
            break;
        case FLIPADST_ADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_8;
            col_cfg = &inv_txfm_1d_col_cfg_adst_8;
            load_buffer_8x8(coeff, in);
            transpose_8x8(in, out);
            iadst8x8_sse4_1(out, in, row_cfg->cos_bit[2]);
            transpose_8x8(in, out);
            iadst8x8_sse4_1(out, in, col_cfg->cos_bit[2]);
            write_buffer_8x8(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
            break;
#endif  // 0
        default: assert(0);
        }
    }

    // 16x16
    static void load_buffer_16x16(const int32_t *coeff, __m128i *in) {
        int i;
        for (i = 0; i < 64; ++i) {
            in[i] = _mm_load_si128((const __m128i *)(coeff + (i << 2)));
        }
    }

    static void load_buffer_16x16_view_src_as_16s(const int32_t *coeff32, __m128i *in) {
        int i;
        const int16_t* coeff = (const int16_t*)coeff32;

        for (i = 0; i < 64; ++i) {
            in[i] = _mm_loadl_epi64((const __m128i *)(coeff + (i << 2)));
            in[i] = _mm_cvtepi16_epi32 (in[i]);
        }
    }

    static INLINE void transpose_16x16(const __m128i *in, __m128i *out)
    {
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

    // Note:
    //  rounding = 1 << (bit - 1)
    static INLINE __m128i half_btf_sse4_1(const __m128i *w0, const __m128i *n0,
        const __m128i *w1, const __m128i *n1,
        const __m128i *rounding, int bit) {
            __m128i x, y;

            x = _mm_mullo_epi32(*w0, *n0);
            y = _mm_mullo_epi32(*w1, *n1);
            x = _mm_add_epi32(x, y);
            x = _mm_add_epi32(x, *rounding);
            x = _mm_srai_epi32(x, bit);
            return x;
    }

    static void idct16x16_sse4_1(__m128i *in, __m128i *out, int bit) {
        const int32_t *cospi = cospi_arr(bit);
        const __m128i cospi60 = _mm_set1_epi32(cospi[60]);
        const __m128i cospim4 = _mm_set1_epi32(-cospi[4]);
        const __m128i cospi28 = _mm_set1_epi32(cospi[28]);
        const __m128i cospim36 = _mm_set1_epi32(-cospi[36]);
        const __m128i cospi44 = _mm_set1_epi32(cospi[44]);
        const __m128i cospi20 = _mm_set1_epi32(cospi[20]);
        const __m128i cospim20 = _mm_set1_epi32(-cospi[20]);
        const __m128i cospi12 = _mm_set1_epi32(cospi[12]);
        const __m128i cospim52 = _mm_set1_epi32(-cospi[52]);
        const __m128i cospi52 = _mm_set1_epi32(cospi[52]);
        const __m128i cospi36 = _mm_set1_epi32(cospi[36]);
        const __m128i cospi4 = _mm_set1_epi32(cospi[4]);
        const __m128i cospi56 = _mm_set1_epi32(cospi[56]);
        const __m128i cospim8 = _mm_set1_epi32(-cospi[8]);
        const __m128i cospi24 = _mm_set1_epi32(cospi[24]);
        const __m128i cospim40 = _mm_set1_epi32(-cospi[40]);
        const __m128i cospi40 = _mm_set1_epi32(cospi[40]);
        const __m128i cospi8 = _mm_set1_epi32(cospi[8]);
        const __m128i cospi32 = _mm_set1_epi32(cospi[32]);
        const __m128i cospi48 = _mm_set1_epi32(cospi[48]);
        const __m128i cospi16 = _mm_set1_epi32(cospi[16]);
        const __m128i cospim16 = _mm_set1_epi32(-cospi[16]);
        const __m128i cospim48 = _mm_set1_epi32(-cospi[48]);
        const __m128i rnding = _mm_set1_epi32(1 << (bit - 1));
        __m128i u[16], v[16], x, y;
        int col;

        for (col = 0; col < 4; ++col) {
            // stage 0
            // stage 1
            u[0] = in[0 * 4 + col];
            u[1] = in[8 * 4 + col];
            u[2] = in[4 * 4 + col];
            u[3] = in[12 * 4 + col];
            u[4] = in[2 * 4 + col];
            u[5] = in[10 * 4 + col];
            u[6] = in[6 * 4 + col];
            u[7] = in[14 * 4 + col];
            u[8] = in[1 * 4 + col];
            u[9] = in[9 * 4 + col];
            u[10] = in[5 * 4 + col];
            u[11] = in[13 * 4 + col];
            u[12] = in[3 * 4 + col];
            u[13] = in[11 * 4 + col];
            u[14] = in[7 * 4 + col];
            u[15] = in[15 * 4 + col];

            // stage 2
            v[0] = u[0];
            v[1] = u[1];
            v[2] = u[2];
            v[3] = u[3];
            v[4] = u[4];
            v[5] = u[5];
            v[6] = u[6];
            v[7] = u[7];

            v[8] = half_btf_sse4_1(&cospi60, &u[8], &cospim4, &u[15], &rnding, bit);
            v[9] = half_btf_sse4_1(&cospi28, &u[9], &cospim36, &u[14], &rnding, bit);
            v[10] = half_btf_sse4_1(&cospi44, &u[10], &cospim20, &u[13], &rnding, bit);
            v[11] = half_btf_sse4_1(&cospi12, &u[11], &cospim52, &u[12], &rnding, bit);
            v[12] = half_btf_sse4_1(&cospi52, &u[11], &cospi12, &u[12], &rnding, bit);
            v[13] = half_btf_sse4_1(&cospi20, &u[10], &cospi44, &u[13], &rnding, bit);
            v[14] = half_btf_sse4_1(&cospi36, &u[9], &cospi28, &u[14], &rnding, bit);
            v[15] = half_btf_sse4_1(&cospi4, &u[8], &cospi60, &u[15], &rnding, bit);

            // stage 3
            u[0] = v[0];
            u[1] = v[1];
            u[2] = v[2];
            u[3] = v[3];
            u[4] = half_btf_sse4_1(&cospi56, &v[4], &cospim8, &v[7], &rnding, bit);
            u[5] = half_btf_sse4_1(&cospi24, &v[5], &cospim40, &v[6], &rnding, bit);
            u[6] = half_btf_sse4_1(&cospi40, &v[5], &cospi24, &v[6], &rnding, bit);
            u[7] = half_btf_sse4_1(&cospi8, &v[4], &cospi56, &v[7], &rnding, bit);
            u[8] = _mm_add_epi32(v[8], v[9]);
            u[9] = _mm_sub_epi32(v[8], v[9]);
            u[10] = _mm_sub_epi32(v[11], v[10]);
            u[11] = _mm_add_epi32(v[10], v[11]);
            u[12] = _mm_add_epi32(v[12], v[13]);
            u[13] = _mm_sub_epi32(v[12], v[13]);
            u[14] = _mm_sub_epi32(v[15], v[14]);
            u[15] = _mm_add_epi32(v[14], v[15]);

            // stage 4
            x = _mm_mullo_epi32(u[0], cospi32);
            y = _mm_mullo_epi32(u[1], cospi32);
            v[0] = _mm_add_epi32(x, y);
            v[0] = _mm_add_epi32(v[0], rnding);
            v[0] = _mm_srai_epi32(v[0], bit);

            v[1] = _mm_sub_epi32(x, y);
            v[1] = _mm_add_epi32(v[1], rnding);
            v[1] = _mm_srai_epi32(v[1], bit);

            v[2] = half_btf_sse4_1(&cospi48, &u[2], &cospim16, &u[3], &rnding, bit);
            v[3] = half_btf_sse4_1(&cospi16, &u[2], &cospi48, &u[3], &rnding, bit);
            v[4] = _mm_add_epi32(u[4], u[5]);
            v[5] = _mm_sub_epi32(u[4], u[5]);
            v[6] = _mm_sub_epi32(u[7], u[6]);
            v[7] = _mm_add_epi32(u[6], u[7]);
            v[8] = u[8];
            v[9] = half_btf_sse4_1(&cospim16, &u[9], &cospi48, &u[14], &rnding, bit);
            v[10] = half_btf_sse4_1(&cospim48, &u[10], &cospim16, &u[13], &rnding, bit);
            v[11] = u[11];
            v[12] = u[12];
            v[13] = half_btf_sse4_1(&cospim16, &u[10], &cospi48, &u[13], &rnding, bit);
            v[14] = half_btf_sse4_1(&cospi48, &u[9], &cospi16, &u[14], &rnding, bit);
            v[15] = u[15];

            // stage 5
            u[0] = _mm_add_epi32(v[0], v[3]);
            u[1] = _mm_add_epi32(v[1], v[2]);
            u[2] = _mm_sub_epi32(v[1], v[2]);
            u[3] = _mm_sub_epi32(v[0], v[3]);
            u[4] = v[4];

            x = _mm_mullo_epi32(v[5], cospi32);
            y = _mm_mullo_epi32(v[6], cospi32);
            u[5] = _mm_sub_epi32(y, x);
            u[5] = _mm_add_epi32(u[5], rnding);
            u[5] = _mm_srai_epi32(u[5], bit);

            u[6] = _mm_add_epi32(y, x);
            u[6] = _mm_add_epi32(u[6], rnding);
            u[6] = _mm_srai_epi32(u[6], bit);

            u[7] = v[7];
            u[8] = _mm_add_epi32(v[8], v[11]);
            u[9] = _mm_add_epi32(v[9], v[10]);
            u[10] = _mm_sub_epi32(v[9], v[10]);
            u[11] = _mm_sub_epi32(v[8], v[11]);
            u[12] = _mm_sub_epi32(v[15], v[12]);
            u[13] = _mm_sub_epi32(v[14], v[13]);
            u[14] = _mm_add_epi32(v[13], v[14]);
            u[15] = _mm_add_epi32(v[12], v[15]);

            // stage 6
            v[0] = _mm_add_epi32(u[0], u[7]);
            v[1] = _mm_add_epi32(u[1], u[6]);
            v[2] = _mm_add_epi32(u[2], u[5]);
            v[3] = _mm_add_epi32(u[3], u[4]);
            v[4] = _mm_sub_epi32(u[3], u[4]);
            v[5] = _mm_sub_epi32(u[2], u[5]);
            v[6] = _mm_sub_epi32(u[1], u[6]);
            v[7] = _mm_sub_epi32(u[0], u[7]);
            v[8] = u[8];
            v[9] = u[9];

            x = _mm_mullo_epi32(u[10], cospi32);
            y = _mm_mullo_epi32(u[13], cospi32);
            v[10] = _mm_sub_epi32(y, x);
            v[10] = _mm_add_epi32(v[10], rnding);
            v[10] = _mm_srai_epi32(v[10], bit);

            v[13] = _mm_add_epi32(x, y);
            v[13] = _mm_add_epi32(v[13], rnding);
            v[13] = _mm_srai_epi32(v[13], bit);

            x = _mm_mullo_epi32(u[11], cospi32);
            y = _mm_mullo_epi32(u[12], cospi32);
            v[11] = _mm_sub_epi32(y, x);
            v[11] = _mm_add_epi32(v[11], rnding);
            v[11] = _mm_srai_epi32(v[11], bit);

            v[12] = _mm_add_epi32(x, y);
            v[12] = _mm_add_epi32(v[12], rnding);
            v[12] = _mm_srai_epi32(v[12], bit);

            v[14] = u[14];
            v[15] = u[15];

            // stage 7
            out[0 * 4 + col] = _mm_add_epi32(v[0], v[15]);
            out[1 * 4 + col] = _mm_add_epi32(v[1], v[14]);
            out[2 * 4 + col] = _mm_add_epi32(v[2], v[13]);
            out[3 * 4 + col] = _mm_add_epi32(v[3], v[12]);
            out[4 * 4 + col] = _mm_add_epi32(v[4], v[11]);
            out[5 * 4 + col] = _mm_add_epi32(v[5], v[10]);
            out[6 * 4 + col] = _mm_add_epi32(v[6], v[9]);
            out[7 * 4 + col] = _mm_add_epi32(v[7], v[8]);
            out[8 * 4 + col] = _mm_sub_epi32(v[7], v[8]);
            out[9 * 4 + col] = _mm_sub_epi32(v[6], v[9]);
            out[10 * 4 + col] = _mm_sub_epi32(v[5], v[10]);
            out[11 * 4 + col] = _mm_sub_epi32(v[4], v[11]);
            out[12 * 4 + col] = _mm_sub_epi32(v[3], v[12]);
            out[13 * 4 + col] = _mm_sub_epi32(v[2], v[13]);
            out[14 * 4 + col] = _mm_sub_epi32(v[1], v[14]);
            out[15 * 4 + col] = _mm_sub_epi32(v[0], v[15]);
        }
    }

    static void round_shift_16x16(__m128i *in, int shift)
    {
        round_shift_8x8(&in[0], shift);
        round_shift_8x8(&in[16], shift);
        round_shift_8x8(&in[32], shift);
        round_shift_8x8(&in[48], shift);
    }

    static void assign_8x8_input_from_16x16(const __m128i *in, __m128i *in8x8, int col)
    {
        int i;
        for (i = 0; i < 16; i += 2) {
            in8x8[i] = in[col];
            in8x8[i + 1] = in[col + 1];
            col += 4;
        }
    }

    static void swap_addr(uint16_t **output1, uint16_t **output2)
    {
        uint16_t *tmp;
        tmp = *output1;
        *output1 = *output2;
        *output2 = tmp;
    }

    static void swap_addr_8u(uint8_t **output1, uint8_t **output2)
    {
        uint8_t *tmp;
        tmp = *output1;
        *output1 = *output2;
        *output2 = tmp;
    }

    static void write_buffer_16x16(__m128i *in, uint16_t *output, int stride,
        int fliplr, int flipud, int shift, int bd)
    {
        __m128i in8x8[16];
        uint16_t *leftUp = &output[0];
        uint16_t *rightUp = &output[8];
        uint16_t *leftDown = &output[8 * stride];
        uint16_t *rightDown = &output[8 * stride + 8];

        if (fliplr) {
            swap_addr(&leftUp, &rightUp);
            swap_addr(&leftDown, &rightDown);
        }

        if (flipud) {
            swap_addr(&leftUp, &leftDown);
            swap_addr(&rightUp, &rightDown);
        }

        // Left-up quarter
        assign_8x8_input_from_16x16(in, in8x8, 0);
        write_buffer_8x8(in8x8, leftUp, stride, fliplr, flipud, shift, bd);

        // Right-up quarter
        assign_8x8_input_from_16x16(in, in8x8, 2);
        write_buffer_8x8(in8x8, rightUp, stride, fliplr, flipud, shift, bd);

        // Left-down quarter
        assign_8x8_input_from_16x16(in, in8x8, 32);
        write_buffer_8x8(in8x8, leftDown, stride, fliplr, flipud, shift, bd);

        // Right-down quarter
        assign_8x8_input_from_16x16(in, in8x8, 34);
        write_buffer_8x8(in8x8, rightDown, stride, fliplr, flipud, shift, bd);
    }

    static void write_buffer_16x16_noclamp(__m128i *in, uint16_t *output, int stride,
        int fliplr, int flipud, int shift, int bd)
    {
        __m128i in8x8[16];
        uint16_t *leftUp = &output[0];
        uint16_t *rightUp = &output[8];
        uint16_t *leftDown = &output[8 * stride];
        uint16_t *rightDown = &output[8 * stride + 8];

        if (fliplr) {
            swap_addr(&leftUp, &rightUp);
            swap_addr(&leftDown, &rightDown);
        }

        if (flipud) {
            swap_addr(&leftUp, &leftDown);
            swap_addr(&rightUp, &rightDown);
        }

        // Left-up quarter
        assign_8x8_input_from_16x16(in, in8x8, 0);
        write_buffer_8x8_noclamp(in8x8, leftUp, stride, fliplr, flipud, shift, bd);

        // Right-up quarter
        assign_8x8_input_from_16x16(in, in8x8, 2);
        write_buffer_8x8_noclamp(in8x8, rightUp, stride, fliplr, flipud, shift, bd);

        // Left-down quarter
        assign_8x8_input_from_16x16(in, in8x8, 32);
        write_buffer_8x8_noclamp(in8x8, leftDown, stride, fliplr, flipud, shift, bd);

        // Right-down quarter
        assign_8x8_input_from_16x16(in, in8x8, 34);
        write_buffer_8x8_noclamp(in8x8, rightDown, stride, fliplr, flipud, shift, bd);
    }

    static void write_buffer_16x16_8u(__m128i *in, uint8_t *output, int stride,
        int fliplr, int flipud, int shift, int bd)
    {
        __m128i in8x8[16];
        uint8_t *leftUp = &output[0];
        uint8_t *rightUp = &output[8];
        uint8_t *leftDown = &output[8 * stride];
        uint8_t *rightDown = &output[8 * stride + 8];

        if (fliplr) {
            swap_addr_8u(&leftUp, &rightUp);
            swap_addr_8u(&leftDown, &rightDown);
        }

        if (flipud) {
            swap_addr_8u(&leftUp, &leftDown);
            swap_addr_8u(&rightUp, &rightDown);
        }

        // Left-up quarter
        assign_8x8_input_from_16x16(in, in8x8, 0);
        write_buffer_8x8_view_dst_as_8u(in8x8, (uint16_t *)leftUp, stride, fliplr, flipud, shift, bd);

        // Right-up quarter
        assign_8x8_input_from_16x16(in, in8x8, 2);
        write_buffer_8x8_view_dst_as_8u(in8x8, (uint16_t *)rightUp, stride, fliplr, flipud, shift, bd);

        // Left-down quarter
        assign_8x8_input_from_16x16(in, in8x8, 32);
        write_buffer_8x8_view_dst_as_8u(in8x8, (uint16_t *)leftDown, stride, fliplr, flipud, shift, bd);

        // Right-down quarter
        assign_8x8_input_from_16x16(in, in8x8, 34);
        write_buffer_8x8_view_dst_as_8u(in8x8, (uint16_t *)rightDown, stride, fliplr, flipud, shift, bd);
    }

    static void iadst16x16_sse4_1(__m128i *in, __m128i *out, int bit)
    {
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

    void av1_inv_txfm2d_add_16x16_sse4_1(const int32_t *coeff, uint16_t *output, int stride, int tx_type, int bd, int useAdd)
    {
        __m128i in[64], out[64];
        const int8_t *shift = inv_txfm_shift_ls[TX_16X16];
        const int txw_idx = TX_16X16;
        const int txh_idx = TX_16X16;

        switch (tx_type) {
        case DCT_DCT:
            load_buffer_16x16_view_src_as_16s(coeff, in);
            transpose_16x16(in, out);
            idct16x16_sse4_1(out, in, inv_cos_bit_row[txw_idx][txh_idx]);
            round_shift_16x16(in, -shift[0]);
            transpose_16x16(in, out);
            idct16x16_sse4_1(out, in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_16x16_8u(in, (uint8_t*)output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_16x16_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
        case DCT_ADST:
            load_buffer_16x16_view_src_as_16s(coeff, in);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, inv_cos_bit_row[txw_idx][txh_idx]);
            round_shift_16x16(in, -shift[0]);
            transpose_16x16(in, out);
            idct16x16_sse4_1(out, in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_16x16_8u(in, (uint8_t*)output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_16x16_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
        case ADST_DCT:
            load_buffer_16x16_view_src_as_16s(coeff, in);
            transpose_16x16(in, out);
            idct16x16_sse4_1(out, in, inv_cos_bit_row[txw_idx][txh_idx]);
            round_shift_16x16(in, -shift[0]);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_16x16_8u(in, (uint8_t*)output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_16x16_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
        case ADST_ADST:
            load_buffer_16x16_view_src_as_16s(coeff, in);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, inv_cos_bit_row[txw_idx][txh_idx]);
            round_shift_16x16(in, -shift[0]);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_16x16_8u(in, (uint8_t*)output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_16x16_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
#if 0
        case FLIPADST_DCT:
            row_cfg = &inv_txfm_1d_row_cfg_dct_16;
            col_cfg = &inv_txfm_1d_col_cfg_adst_16;
            load_buffer_16x16(coeff, in);
            transpose_16x16(in, out);
            idct16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
            round_shift_16x16(in, -row_cfg->shift[0]);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
            write_buffer_16x16(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
            break;
        case DCT_FLIPADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_16;
            col_cfg = &inv_txfm_1d_col_cfg_dct_16;
            load_buffer_16x16(coeff, in);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
            round_shift_16x16(in, -row_cfg->shift[0]);
            transpose_16x16(in, out);
            idct16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
            write_buffer_16x16(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
            break;
        case ADST_FLIPADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_16;
            col_cfg = &inv_txfm_1d_col_cfg_adst_16;
            load_buffer_16x16(coeff, in);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
            round_shift_16x16(in, -row_cfg->shift[0]);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
            write_buffer_16x16(in, output, stride, 1, 0, -row_cfg->shift[1], bd);
            break;
        case FLIPADST_FLIPADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_16;
            col_cfg = &inv_txfm_1d_col_cfg_adst_16;
            load_buffer_16x16(coeff, in);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
            round_shift_16x16(in, -row_cfg->shift[0]);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
            write_buffer_16x16(in, output, stride, 1, 1, -row_cfg->shift[1], bd);
            break;
        case FLIPADST_ADST:
            row_cfg = &inv_txfm_1d_row_cfg_adst_16;
            col_cfg = &inv_txfm_1d_col_cfg_adst_16;
            load_buffer_16x16(coeff, in);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, row_cfg->cos_bit[2]);
            round_shift_16x16(in, -row_cfg->shift[0]);
            transpose_16x16(in, out);
            iadst16x16_sse4_1(out, in, col_cfg->cos_bit[2]);
            write_buffer_16x16(in, output, stride, 0, 1, -row_cfg->shift[1], bd);
            break;
#endif
        default: assert(0);
        }
    }

    static void load_buffer_32x32(const int32_t *coeff, __m256i *in)
    {
        int i;
        for (i = 0; i < 128; ++i) {
            in[i] = _mm256_loadu_si256((const __m256i *)coeff);
            coeff += 8;
        }
    }


    // fixme: mix mm265 / m128 => performance penalty
    static void load_buffer_32x32_view_src_as_16s(const int32_t *coeff32, __m256i *in)
    {
        const int16_t* coeff = (const int16_t*)coeff32;

        for (int i = 0; i < 128; ++i) {
            __m128i src = _mm_loadu_si128((const __m128i *)(coeff));
            in[i] = _mm256_cvtepi16_epi32 (src);
            coeff += 8;
        }
    }

    // Note:
    //  Total 32x4 registers to represent 32x32 block coefficients.
    //  For high bit depth, each coefficient is 4-byte.
    //  Each __m256i register holds 8 coefficients.
    //  So each "row" we needs 4 register. Totally 32 rows
    //  Register layout:
    //   v0,   v1,   v2,   v3,
    //   v4,   v5,   v6,   v7,
    //   ... ...
    //   v124, v125, v126, v127

    static void transpose_32x32_8x8(const __m256i *in, __m256i *out) {
        __m256i u0, u1, u2, u3, u4, u5, u6, u7;
        __m256i x0, x1;

        u0 = _mm256_unpacklo_epi32(in[0], in[4]);
        u1 = _mm256_unpackhi_epi32(in[0], in[4]);

        u2 = _mm256_unpacklo_epi32(in[8], in[12]);
        u3 = _mm256_unpackhi_epi32(in[8], in[12]);

        u4 = _mm256_unpacklo_epi32(in[16], in[20]);
        u5 = _mm256_unpackhi_epi32(in[16], in[20]);

        u6 = _mm256_unpacklo_epi32(in[24], in[28]);
        u7 = _mm256_unpackhi_epi32(in[24], in[28]);

        x0 = _mm256_unpacklo_epi64(u0, u2);
        x1 = _mm256_unpacklo_epi64(u4, u6);
        out[0] = _mm256_permute2f128_si256(x0, x1, 0x20);
        out[16] = _mm256_permute2f128_si256(x0, x1, 0x31);

        x0 = _mm256_unpackhi_epi64(u0, u2);
        x1 = _mm256_unpackhi_epi64(u4, u6);
        out[4] = _mm256_permute2f128_si256(x0, x1, 0x20);
        out[20] = _mm256_permute2f128_si256(x0, x1, 0x31);

        x0 = _mm256_unpacklo_epi64(u1, u3);
        x1 = _mm256_unpacklo_epi64(u5, u7);
        out[8] = _mm256_permute2f128_si256(x0, x1, 0x20);
        out[24] = _mm256_permute2f128_si256(x0, x1, 0x31);

        x0 = _mm256_unpackhi_epi64(u1, u3);
        x1 = _mm256_unpackhi_epi64(u5, u7);
        out[12] = _mm256_permute2f128_si256(x0, x1, 0x20);
        out[28] = _mm256_permute2f128_si256(x0, x1, 0x31);
    }

    static void transpose_32x32_16x16(const __m256i *in, __m256i *out)
    {
        transpose_32x32_8x8(&in[0], &out[0]);
        transpose_32x32_8x8(&in[1], &out[32]);
        transpose_32x32_8x8(&in[32], &out[1]);
        transpose_32x32_8x8(&in[33], &out[33]);
    }

    static void transpose_32x32(const __m256i *in, __m256i *out)
    {
        transpose_32x32_16x16(&in[0], &out[0]);
        transpose_32x32_16x16(&in[2], &out[64]);
        transpose_32x32_16x16(&in[64], &out[2]);
        transpose_32x32_16x16(&in[66], &out[66]);
    }

    static INLINE __m256i half_btf_avx2(const __m256i *w0, const __m256i *n0,
        const __m256i *w1, const __m256i *n1,
        const __m256i *rounding, int bit)
    {
        __m256i x, y;

        x = _mm256_mullo_epi32(*w0, *n0);
        y = _mm256_mullo_epi32(*w1, *n1);
        x = _mm256_add_epi32(x, y);
        x = _mm256_add_epi32(x, *rounding);
        x = _mm256_srai_epi32(x, bit);
        return x;
    }

    static void idct32_avx2(__m256i *in, __m256i *out, int bit) {
        const int32_t *cospi = cospi_arr(bit);
        const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
        const __m256i cospi30 = _mm256_set1_epi32(cospi[30]);
        const __m256i cospi46 = _mm256_set1_epi32(cospi[46]);
        const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
        const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
        const __m256i cospi22 = _mm256_set1_epi32(cospi[22]);
        const __m256i cospi38 = _mm256_set1_epi32(cospi[38]);
        const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
        const __m256i cospi58 = _mm256_set1_epi32(cospi[58]);
        const __m256i cospi26 = _mm256_set1_epi32(cospi[26]);
        const __m256i cospi42 = _mm256_set1_epi32(cospi[42]);
        const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
        const __m256i cospi50 = _mm256_set1_epi32(cospi[50]);
        const __m256i cospi18 = _mm256_set1_epi32(cospi[18]);
        const __m256i cospi34 = _mm256_set1_epi32(cospi[34]);
        const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
        const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
        const __m256i cospim26 = _mm256_set1_epi32(-cospi[26]);
        const __m256i cospim42 = _mm256_set1_epi32(-cospi[42]);
        const __m256i cospim10 = _mm256_set1_epi32(-cospi[10]);
        const __m256i cospim50 = _mm256_set1_epi32(-cospi[50]);
        const __m256i cospim18 = _mm256_set1_epi32(-cospi[18]);
        const __m256i cospim34 = _mm256_set1_epi32(-cospi[34]);
        const __m256i cospim2 = _mm256_set1_epi32(-cospi[2]);
        const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
        const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
        const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
        const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
        const __m256i cospi52 = _mm256_set1_epi32(cospi[52]);
        const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
        const __m256i cospi36 = _mm256_set1_epi32(cospi[36]);
        const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
        const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
        const __m256i cospim20 = _mm256_set1_epi32(-cospi[20]);
        const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
        const __m256i cospim4 = _mm256_set1_epi32(-cospi[4]);
        const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
        const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
        const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
        const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
        const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
        const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
        const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
        const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
        const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
        const __m256i cospim32 = _mm256_set1_epi32(-cospi[32]);
        const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
        const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
        const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
        const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
        const __m256i rounding = _mm256_set1_epi32(1 << (bit - 1));
        __m256i bf1[32], bf0[32];
        int col;

        for (col = 0; col < 4; ++col) {
            // stage 0
            // stage 1
            bf1[0] = in[0 * 4 + col];
            bf1[1] = in[16 * 4 + col];
            bf1[2] = in[8 * 4 + col];
            bf1[3] = in[24 * 4 + col];
            bf1[4] = in[4 * 4 + col];
            bf1[5] = in[20 * 4 + col];
            bf1[6] = in[12 * 4 + col];
            bf1[7] = in[28 * 4 + col];
            bf1[8] = in[2 * 4 + col];
            bf1[9] = in[18 * 4 + col];
            bf1[10] = in[10 * 4 + col];
            bf1[11] = in[26 * 4 + col];
            bf1[12] = in[6 * 4 + col];
            bf1[13] = in[22 * 4 + col];
            bf1[14] = in[14 * 4 + col];
            bf1[15] = in[30 * 4 + col];
            bf1[16] = in[1 * 4 + col];
            bf1[17] = in[17 * 4 + col];
            bf1[18] = in[9 * 4 + col];
            bf1[19] = in[25 * 4 + col];
            bf1[20] = in[5 * 4 + col];
            bf1[21] = in[21 * 4 + col];
            bf1[22] = in[13 * 4 + col];
            bf1[23] = in[29 * 4 + col];
            bf1[24] = in[3 * 4 + col];
            bf1[25] = in[19 * 4 + col];
            bf1[26] = in[11 * 4 + col];
            bf1[27] = in[27 * 4 + col];
            bf1[28] = in[7 * 4 + col];
            bf1[29] = in[23 * 4 + col];
            bf1[30] = in[15 * 4 + col];
            bf1[31] = in[31 * 4 + col];

            // stage 2
            bf0[0] = bf1[0];
            bf0[1] = bf1[1];
            bf0[2] = bf1[2];
            bf0[3] = bf1[3];
            bf0[4] = bf1[4];
            bf0[5] = bf1[5];
            bf0[6] = bf1[6];
            bf0[7] = bf1[7];
            bf0[8] = bf1[8];
            bf0[9] = bf1[9];
            bf0[10] = bf1[10];
            bf0[11] = bf1[11];
            bf0[12] = bf1[12];
            bf0[13] = bf1[13];
            bf0[14] = bf1[14];
            bf0[15] = bf1[15];
            bf0[16] =
                half_btf_avx2(&cospi62, &bf1[16], &cospim2, &bf1[31], &rounding, bit);
            bf0[17] =
                half_btf_avx2(&cospi30, &bf1[17], &cospim34, &bf1[30], &rounding, bit);
            bf0[18] =
                half_btf_avx2(&cospi46, &bf1[18], &cospim18, &bf1[29], &rounding, bit);
            bf0[19] =
                half_btf_avx2(&cospi14, &bf1[19], &cospim50, &bf1[28], &rounding, bit);
            bf0[20] =
                half_btf_avx2(&cospi54, &bf1[20], &cospim10, &bf1[27], &rounding, bit);
            bf0[21] =
                half_btf_avx2(&cospi22, &bf1[21], &cospim42, &bf1[26], &rounding, bit);
            bf0[22] =
                half_btf_avx2(&cospi38, &bf1[22], &cospim26, &bf1[25], &rounding, bit);
            bf0[23] =
                half_btf_avx2(&cospi6, &bf1[23], &cospim58, &bf1[24], &rounding, bit);
            bf0[24] =
                half_btf_avx2(&cospi58, &bf1[23], &cospi6, &bf1[24], &rounding, bit);
            bf0[25] =
                half_btf_avx2(&cospi26, &bf1[22], &cospi38, &bf1[25], &rounding, bit);
            bf0[26] =
                half_btf_avx2(&cospi42, &bf1[21], &cospi22, &bf1[26], &rounding, bit);
            bf0[27] =
                half_btf_avx2(&cospi10, &bf1[20], &cospi54, &bf1[27], &rounding, bit);
            bf0[28] =
                half_btf_avx2(&cospi50, &bf1[19], &cospi14, &bf1[28], &rounding, bit);
            bf0[29] =
                half_btf_avx2(&cospi18, &bf1[18], &cospi46, &bf1[29], &rounding, bit);
            bf0[30] =
                half_btf_avx2(&cospi34, &bf1[17], &cospi30, &bf1[30], &rounding, bit);
            bf0[31] =
                half_btf_avx2(&cospi2, &bf1[16], &cospi62, &bf1[31], &rounding, bit);

            // stage 3
            bf1[0] = bf0[0];
            bf1[1] = bf0[1];
            bf1[2] = bf0[2];
            bf1[3] = bf0[3];
            bf1[4] = bf0[4];
            bf1[5] = bf0[5];
            bf1[6] = bf0[6];
            bf1[7] = bf0[7];
            bf1[8] =
                half_btf_avx2(&cospi60, &bf0[8], &cospim4, &bf0[15], &rounding, bit);
            bf1[9] =
                half_btf_avx2(&cospi28, &bf0[9], &cospim36, &bf0[14], &rounding, bit);
            bf1[10] =
                half_btf_avx2(&cospi44, &bf0[10], &cospim20, &bf0[13], &rounding, bit);
            bf1[11] =
                half_btf_avx2(&cospi12, &bf0[11], &cospim52, &bf0[12], &rounding, bit);
            bf1[12] =
                half_btf_avx2(&cospi52, &bf0[11], &cospi12, &bf0[12], &rounding, bit);
            bf1[13] =
                half_btf_avx2(&cospi20, &bf0[10], &cospi44, &bf0[13], &rounding, bit);
            bf1[14] =
                half_btf_avx2(&cospi36, &bf0[9], &cospi28, &bf0[14], &rounding, bit);
            bf1[15] =
                half_btf_avx2(&cospi4, &bf0[8], &cospi60, &bf0[15], &rounding, bit);
            bf1[16] = _mm256_add_epi32(bf0[16], bf0[17]);
            bf1[17] = _mm256_sub_epi32(bf0[16], bf0[17]);
            bf1[18] = _mm256_sub_epi32(bf0[19], bf0[18]);
            bf1[19] = _mm256_add_epi32(bf0[18], bf0[19]);
            bf1[20] = _mm256_add_epi32(bf0[20], bf0[21]);
            bf1[21] = _mm256_sub_epi32(bf0[20], bf0[21]);
            bf1[22] = _mm256_sub_epi32(bf0[23], bf0[22]);
            bf1[23] = _mm256_add_epi32(bf0[22], bf0[23]);
            bf1[24] = _mm256_add_epi32(bf0[24], bf0[25]);
            bf1[25] = _mm256_sub_epi32(bf0[24], bf0[25]);
            bf1[26] = _mm256_sub_epi32(bf0[27], bf0[26]);
            bf1[27] = _mm256_add_epi32(bf0[26], bf0[27]);
            bf1[28] = _mm256_add_epi32(bf0[28], bf0[29]);
            bf1[29] = _mm256_sub_epi32(bf0[28], bf0[29]);
            bf1[30] = _mm256_sub_epi32(bf0[31], bf0[30]);
            bf1[31] = _mm256_add_epi32(bf0[30], bf0[31]);

            // stage 4
            bf0[0] = bf1[0];
            bf0[1] = bf1[1];
            bf0[2] = bf1[2];
            bf0[3] = bf1[3];
            bf0[4] =
                half_btf_avx2(&cospi56, &bf1[4], &cospim8, &bf1[7], &rounding, bit);
            bf0[5] =
                half_btf_avx2(&cospi24, &bf1[5], &cospim40, &bf1[6], &rounding, bit);
            bf0[6] =
                half_btf_avx2(&cospi40, &bf1[5], &cospi24, &bf1[6], &rounding, bit);
            bf0[7] = half_btf_avx2(&cospi8, &bf1[4], &cospi56, &bf1[7], &rounding, bit);
            bf0[8] = _mm256_add_epi32(bf1[8], bf1[9]);
            bf0[9] = _mm256_sub_epi32(bf1[8], bf1[9]);
            bf0[10] = _mm256_sub_epi32(bf1[11], bf1[10]);
            bf0[11] = _mm256_add_epi32(bf1[10], bf1[11]);
            bf0[12] = _mm256_add_epi32(bf1[12], bf1[13]);
            bf0[13] = _mm256_sub_epi32(bf1[12], bf1[13]);
            bf0[14] = _mm256_sub_epi32(bf1[15], bf1[14]);
            bf0[15] = _mm256_add_epi32(bf1[14], bf1[15]);
            bf0[16] = bf1[16];
            bf0[17] =
                half_btf_avx2(&cospim8, &bf1[17], &cospi56, &bf1[30], &rounding, bit);
            bf0[18] =
                half_btf_avx2(&cospim56, &bf1[18], &cospim8, &bf1[29], &rounding, bit);
            bf0[19] = bf1[19];
            bf0[20] = bf1[20];
            bf0[21] =
                half_btf_avx2(&cospim40, &bf1[21], &cospi24, &bf1[26], &rounding, bit);
            bf0[22] =
                half_btf_avx2(&cospim24, &bf1[22], &cospim40, &bf1[25], &rounding, bit);
            bf0[23] = bf1[23];
            bf0[24] = bf1[24];
            bf0[25] =
                half_btf_avx2(&cospim40, &bf1[22], &cospi24, &bf1[25], &rounding, bit);
            bf0[26] =
                half_btf_avx2(&cospi24, &bf1[21], &cospi40, &bf1[26], &rounding, bit);
            bf0[27] = bf1[27];
            bf0[28] = bf1[28];
            bf0[29] =
                half_btf_avx2(&cospim8, &bf1[18], &cospi56, &bf1[29], &rounding, bit);
            bf0[30] =
                half_btf_avx2(&cospi56, &bf1[17], &cospi8, &bf1[30], &rounding, bit);
            bf0[31] = bf1[31];

            // stage 5
            bf1[0] =
                half_btf_avx2(&cospi32, &bf0[0], &cospi32, &bf0[1], &rounding, bit);
            bf1[1] =
                half_btf_avx2(&cospi32, &bf0[0], &cospim32, &bf0[1], &rounding, bit);
            bf1[2] =
                half_btf_avx2(&cospi48, &bf0[2], &cospim16, &bf0[3], &rounding, bit);
            bf1[3] =
                half_btf_avx2(&cospi16, &bf0[2], &cospi48, &bf0[3], &rounding, bit);
            bf1[4] = _mm256_add_epi32(bf0[4], bf0[5]);
            bf1[5] = _mm256_sub_epi32(bf0[4], bf0[5]);
            bf1[6] = _mm256_sub_epi32(bf0[7], bf0[6]);
            bf1[7] = _mm256_add_epi32(bf0[6], bf0[7]);
            bf1[8] = bf0[8];
            bf1[9] =
                half_btf_avx2(&cospim16, &bf0[9], &cospi48, &bf0[14], &rounding, bit);
            bf1[10] =
                half_btf_avx2(&cospim48, &bf0[10], &cospim16, &bf0[13], &rounding, bit);
            bf1[11] = bf0[11];
            bf1[12] = bf0[12];
            bf1[13] =
                half_btf_avx2(&cospim16, &bf0[10], &cospi48, &bf0[13], &rounding, bit);
            bf1[14] =
                half_btf_avx2(&cospi48, &bf0[9], &cospi16, &bf0[14], &rounding, bit);
            bf1[15] = bf0[15];
            bf1[16] = _mm256_add_epi32(bf0[16], bf0[19]);
            bf1[17] = _mm256_add_epi32(bf0[17], bf0[18]);
            bf1[18] = _mm256_sub_epi32(bf0[17], bf0[18]);
            bf1[19] = _mm256_sub_epi32(bf0[16], bf0[19]);
            bf1[20] = _mm256_sub_epi32(bf0[23], bf0[20]);
            bf1[21] = _mm256_sub_epi32(bf0[22], bf0[21]);
            bf1[22] = _mm256_add_epi32(bf0[21], bf0[22]);
            bf1[23] = _mm256_add_epi32(bf0[20], bf0[23]);
            bf1[24] = _mm256_add_epi32(bf0[24], bf0[27]);
            bf1[25] = _mm256_add_epi32(bf0[25], bf0[26]);
            bf1[26] = _mm256_sub_epi32(bf0[25], bf0[26]);
            bf1[27] = _mm256_sub_epi32(bf0[24], bf0[27]);
            bf1[28] = _mm256_sub_epi32(bf0[31], bf0[28]);
            bf1[29] = _mm256_sub_epi32(bf0[30], bf0[29]);
            bf1[30] = _mm256_add_epi32(bf0[29], bf0[30]);
            bf1[31] = _mm256_add_epi32(bf0[28], bf0[31]);

            // stage 6
            bf0[0] = _mm256_add_epi32(bf1[0], bf1[3]);
            bf0[1] = _mm256_add_epi32(bf1[1], bf1[2]);
            bf0[2] = _mm256_sub_epi32(bf1[1], bf1[2]);
            bf0[3] = _mm256_sub_epi32(bf1[0], bf1[3]);
            bf0[4] = bf1[4];
            bf0[5] =
                half_btf_avx2(&cospim32, &bf1[5], &cospi32, &bf1[6], &rounding, bit);
            bf0[6] =
                half_btf_avx2(&cospi32, &bf1[5], &cospi32, &bf1[6], &rounding, bit);
            bf0[7] = bf1[7];
            bf0[8] = _mm256_add_epi32(bf1[8], bf1[11]);
            bf0[9] = _mm256_add_epi32(bf1[9], bf1[10]);
            bf0[10] = _mm256_sub_epi32(bf1[9], bf1[10]);
            bf0[11] = _mm256_sub_epi32(bf1[8], bf1[11]);
            bf0[12] = _mm256_sub_epi32(bf1[15], bf1[12]);
            bf0[13] = _mm256_sub_epi32(bf1[14], bf1[13]);
            bf0[14] = _mm256_add_epi32(bf1[13], bf1[14]);
            bf0[15] = _mm256_add_epi32(bf1[12], bf1[15]);
            bf0[16] = bf1[16];
            bf0[17] = bf1[17];
            bf0[18] =
                half_btf_avx2(&cospim16, &bf1[18], &cospi48, &bf1[29], &rounding, bit);
            bf0[19] =
                half_btf_avx2(&cospim16, &bf1[19], &cospi48, &bf1[28], &rounding, bit);
            bf0[20] =
                half_btf_avx2(&cospim48, &bf1[20], &cospim16, &bf1[27], &rounding, bit);
            bf0[21] =
                half_btf_avx2(&cospim48, &bf1[21], &cospim16, &bf1[26], &rounding, bit);
            bf0[22] = bf1[22];
            bf0[23] = bf1[23];
            bf0[24] = bf1[24];
            bf0[25] = bf1[25];
            bf0[26] =
                half_btf_avx2(&cospim16, &bf1[21], &cospi48, &bf1[26], &rounding, bit);
            bf0[27] =
                half_btf_avx2(&cospim16, &bf1[20], &cospi48, &bf1[27], &rounding, bit);
            bf0[28] =
                half_btf_avx2(&cospi48, &bf1[19], &cospi16, &bf1[28], &rounding, bit);
            bf0[29] =
                half_btf_avx2(&cospi48, &bf1[18], &cospi16, &bf1[29], &rounding, bit);
            bf0[30] = bf1[30];
            bf0[31] = bf1[31];

            // stage 7
            bf1[0] = _mm256_add_epi32(bf0[0], bf0[7]);
            bf1[1] = _mm256_add_epi32(bf0[1], bf0[6]);
            bf1[2] = _mm256_add_epi32(bf0[2], bf0[5]);
            bf1[3] = _mm256_add_epi32(bf0[3], bf0[4]);
            bf1[4] = _mm256_sub_epi32(bf0[3], bf0[4]);
            bf1[5] = _mm256_sub_epi32(bf0[2], bf0[5]);
            bf1[6] = _mm256_sub_epi32(bf0[1], bf0[6]);
            bf1[7] = _mm256_sub_epi32(bf0[0], bf0[7]);
            bf1[8] = bf0[8];
            bf1[9] = bf0[9];
            bf1[10] =
                half_btf_avx2(&cospim32, &bf0[10], &cospi32, &bf0[13], &rounding, bit);
            bf1[11] =
                half_btf_avx2(&cospim32, &bf0[11], &cospi32, &bf0[12], &rounding, bit);
            bf1[12] =
                half_btf_avx2(&cospi32, &bf0[11], &cospi32, &bf0[12], &rounding, bit);
            bf1[13] =
                half_btf_avx2(&cospi32, &bf0[10], &cospi32, &bf0[13], &rounding, bit);
            bf1[14] = bf0[14];
            bf1[15] = bf0[15];
            bf1[16] = _mm256_add_epi32(bf0[16], bf0[23]);
            bf1[17] = _mm256_add_epi32(bf0[17], bf0[22]);
            bf1[18] = _mm256_add_epi32(bf0[18], bf0[21]);
            bf1[19] = _mm256_add_epi32(bf0[19], bf0[20]);
            bf1[20] = _mm256_sub_epi32(bf0[19], bf0[20]);
            bf1[21] = _mm256_sub_epi32(bf0[18], bf0[21]);
            bf1[22] = _mm256_sub_epi32(bf0[17], bf0[22]);
            bf1[23] = _mm256_sub_epi32(bf0[16], bf0[23]);
            bf1[24] = _mm256_sub_epi32(bf0[31], bf0[24]);
            bf1[25] = _mm256_sub_epi32(bf0[30], bf0[25]);
            bf1[26] = _mm256_sub_epi32(bf0[29], bf0[26]);
            bf1[27] = _mm256_sub_epi32(bf0[28], bf0[27]);
            bf1[28] = _mm256_add_epi32(bf0[27], bf0[28]);
            bf1[29] = _mm256_add_epi32(bf0[26], bf0[29]);
            bf1[30] = _mm256_add_epi32(bf0[25], bf0[30]);
            bf1[31] = _mm256_add_epi32(bf0[24], bf0[31]);

            // stage 8
            bf0[0] = _mm256_add_epi32(bf1[0], bf1[15]);
            bf0[1] = _mm256_add_epi32(bf1[1], bf1[14]);
            bf0[2] = _mm256_add_epi32(bf1[2], bf1[13]);
            bf0[3] = _mm256_add_epi32(bf1[3], bf1[12]);
            bf0[4] = _mm256_add_epi32(bf1[4], bf1[11]);
            bf0[5] = _mm256_add_epi32(bf1[5], bf1[10]);
            bf0[6] = _mm256_add_epi32(bf1[6], bf1[9]);
            bf0[7] = _mm256_add_epi32(bf1[7], bf1[8]);
            bf0[8] = _mm256_sub_epi32(bf1[7], bf1[8]);
            bf0[9] = _mm256_sub_epi32(bf1[6], bf1[9]);
            bf0[10] = _mm256_sub_epi32(bf1[5], bf1[10]);
            bf0[11] = _mm256_sub_epi32(bf1[4], bf1[11]);
            bf0[12] = _mm256_sub_epi32(bf1[3], bf1[12]);
            bf0[13] = _mm256_sub_epi32(bf1[2], bf1[13]);
            bf0[14] = _mm256_sub_epi32(bf1[1], bf1[14]);
            bf0[15] = _mm256_sub_epi32(bf1[0], bf1[15]);
            bf0[16] = bf1[16];
            bf0[17] = bf1[17];
            bf0[18] = bf1[18];
            bf0[19] = bf1[19];
            bf0[20] =
                half_btf_avx2(&cospim32, &bf1[20], &cospi32, &bf1[27], &rounding, bit);
            bf0[21] =
                half_btf_avx2(&cospim32, &bf1[21], &cospi32, &bf1[26], &rounding, bit);
            bf0[22] =
                half_btf_avx2(&cospim32, &bf1[22], &cospi32, &bf1[25], &rounding, bit);
            bf0[23] =
                half_btf_avx2(&cospim32, &bf1[23], &cospi32, &bf1[24], &rounding, bit);
            bf0[24] =
                half_btf_avx2(&cospi32, &bf1[23], &cospi32, &bf1[24], &rounding, bit);
            bf0[25] =
                half_btf_avx2(&cospi32, &bf1[22], &cospi32, &bf1[25], &rounding, bit);
            bf0[26] =
                half_btf_avx2(&cospi32, &bf1[21], &cospi32, &bf1[26], &rounding, bit);
            bf0[27] =
                half_btf_avx2(&cospi32, &bf1[20], &cospi32, &bf1[27], &rounding, bit);
            bf0[28] = bf1[28];
            bf0[29] = bf1[29];
            bf0[30] = bf1[30];
            bf0[31] = bf1[31];

            // stage 9
            out[0 * 4 + col] = _mm256_add_epi32(bf0[0], bf0[31]);
            out[1 * 4 + col] = _mm256_add_epi32(bf0[1], bf0[30]);
            out[2 * 4 + col] = _mm256_add_epi32(bf0[2], bf0[29]);
            out[3 * 4 + col] = _mm256_add_epi32(bf0[3], bf0[28]);
            out[4 * 4 + col] = _mm256_add_epi32(bf0[4], bf0[27]);
            out[5 * 4 + col] = _mm256_add_epi32(bf0[5], bf0[26]);
            out[6 * 4 + col] = _mm256_add_epi32(bf0[6], bf0[25]);
            out[7 * 4 + col] = _mm256_add_epi32(bf0[7], bf0[24]);
            out[8 * 4 + col] = _mm256_add_epi32(bf0[8], bf0[23]);
            out[9 * 4 + col] = _mm256_add_epi32(bf0[9], bf0[22]);
            out[10 * 4 + col] = _mm256_add_epi32(bf0[10], bf0[21]);
            out[11 * 4 + col] = _mm256_add_epi32(bf0[11], bf0[20]);
            out[12 * 4 + col] = _mm256_add_epi32(bf0[12], bf0[19]);
            out[13 * 4 + col] = _mm256_add_epi32(bf0[13], bf0[18]);
            out[14 * 4 + col] = _mm256_add_epi32(bf0[14], bf0[17]);
            out[15 * 4 + col] = _mm256_add_epi32(bf0[15], bf0[16]);
            out[16 * 4 + col] = _mm256_sub_epi32(bf0[15], bf0[16]);
            out[17 * 4 + col] = _mm256_sub_epi32(bf0[14], bf0[17]);
            out[18 * 4 + col] = _mm256_sub_epi32(bf0[13], bf0[18]);
            out[19 * 4 + col] = _mm256_sub_epi32(bf0[12], bf0[19]);
            out[20 * 4 + col] = _mm256_sub_epi32(bf0[11], bf0[20]);
            out[21 * 4 + col] = _mm256_sub_epi32(bf0[10], bf0[21]);
            out[22 * 4 + col] = _mm256_sub_epi32(bf0[9], bf0[22]);
            out[23 * 4 + col] = _mm256_sub_epi32(bf0[8], bf0[23]);
            out[24 * 4 + col] = _mm256_sub_epi32(bf0[7], bf0[24]);
            out[25 * 4 + col] = _mm256_sub_epi32(bf0[6], bf0[25]);
            out[26 * 4 + col] = _mm256_sub_epi32(bf0[5], bf0[26]);
            out[27 * 4 + col] = _mm256_sub_epi32(bf0[4], bf0[27]);
            out[28 * 4 + col] = _mm256_sub_epi32(bf0[3], bf0[28]);
            out[29 * 4 + col] = _mm256_sub_epi32(bf0[2], bf0[29]);
            out[30 * 4 + col] = _mm256_sub_epi32(bf0[1], bf0[30]);
            out[31 * 4 + col] = _mm256_sub_epi32(bf0[0], bf0[31]);
        }
    }

    static void round_shift_32x32(__m256i *in, int shift) {
        __m256i rnding = _mm256_set1_epi32(1 << (shift - 1));
        int i = 0;

        while (i < 128) {
            in[i] = _mm256_add_epi32(in[i], rnding);
            in[i] = _mm256_srai_epi32(in[i], shift);
            i++;
        }
    }

    static __m256i highbd_clamp_epi32(__m256i x, int bd) {
        const __m256i zero = _mm256_setzero_si256();
        const __m256i one = _mm256_set1_epi16(1);
        const __m256i max = _mm256_sub_epi16(_mm256_slli_epi16(one, bd), one);
        __m256i clamped, mask;

        mask = _mm256_cmpgt_epi16(x, max);
        clamped = _mm256_andnot_si256(mask, x);
        mask = _mm256_and_si256(mask, max);
        clamped = _mm256_or_si256(mask, clamped);
        mask = _mm256_cmpgt_epi16(clamped, zero);
        clamped = _mm256_and_si256(clamped, mask);

        return clamped;
    }

    static void write_buffer_32x32_noclamp(__m256i *in, uint16_t *output, int stride,
                                           int fliplr, int flipud, int shift, int /*bd*/)
    {
            __m256i u0, u1, x0, x1, x2, x3, v0, v1, v2, v3;
            const __m256i zero = _mm256_setzero_si256();
            int i = 0;
            (void)fliplr;
            (void)flipud;

            round_shift_32x32(in, shift);

            while (i < 128) {
                u0 = zero;//_mm256_loadu_si256((const __m256i *)output);
                u1 = zero;//_mm256_loadu_si256((const __m256i *)(output + 16));

                x0 = _mm256_unpacklo_epi16(u0, zero);
                x1 = _mm256_unpackhi_epi16(u0, zero);
                x2 = _mm256_unpacklo_epi16(u1, zero);
                x3 = _mm256_unpackhi_epi16(u1, zero);

                v0 = _mm256_permute2f128_si256(in[i], in[i + 1], 0x20);
                v1 = _mm256_permute2f128_si256(in[i], in[i + 1], 0x31);
                v2 = _mm256_permute2f128_si256(in[i + 2], in[i + 3], 0x20);
                v3 = _mm256_permute2f128_si256(in[i + 2], in[i + 3], 0x31);

                v0 = _mm256_add_epi32(v0, x0);
                v1 = _mm256_add_epi32(v1, x1);
                v2 = _mm256_add_epi32(v2, x2);
                v3 = _mm256_add_epi32(v3, x3);

                v0 = _mm256_packs_epi32(v0, v1);
                v2 = _mm256_packs_epi32(v2, v3);

                //v0 = highbd_clamp_epi32(v0, bd);
                //v2 = highbd_clamp_epi32(v2, bd);

                _mm256_storeu_si256((__m256i *)output, v0);
                _mm256_storeu_si256((__m256i *)(output + 16), v2);
                output += stride;
                i += 4;
            }
    }

    static void write_buffer_32x32_view_dst_as_8u(__m256i *in, uint16_t *output, int stride,
                                                  int fliplr, int flipud, int shift, int bd)
    {
            __m256i u0, u1, x0, x1, x2, x3, v0, v1, v2, v3;
            __m256i tu0, tx1, tx2;
            const __m256i zero = _mm256_setzero_si256();
            int i = 0;
            (void)fliplr;
            (void)flipud;

            round_shift_32x32(in, shift);
            uint8_t* output8 = (uint8_t*)output;
            int stride8 = stride;
            //for (int y = 0; y < 32; y++) {
            //    for (int x = 0; x < 32; x++) {
            //        output8[y*stride8+x] = output[y*stride+x];// = 1 + y*32+x;
            //    }
            //}

            while (i < 128) {
                //u0 = _mm256_loadu_si256((const __m256i *)output);
                //u1 = _mm256_loadu_si256((const __m256i *)(output + 16));

                tu0 = _mm256_loadu_si256((const __m256i *)output8);
                tx1 = _mm256_unpacklo_epi8(tu0, zero);
                tx2 = _mm256_unpackhi_epi8(tu0, zero);
                u0 = _mm256_permute2f128_si256(tx1, tx2, 0x20);
                u1 = _mm256_permute2f128_si256(tx1, tx2, 0x31);

                x0 = _mm256_unpacklo_epi16(u0, zero);
                x1 = _mm256_unpackhi_epi16(u0, zero);
                x2 = _mm256_unpacklo_epi16(u1, zero);
                x3 = _mm256_unpackhi_epi16(u1, zero);

                v0 = _mm256_permute2f128_si256(in[i], in[i + 1], 0x20);
                v1 = _mm256_permute2f128_si256(in[i], in[i + 1], 0x31);
                v2 = _mm256_permute2f128_si256(in[i + 2], in[i + 3], 0x20);
                v3 = _mm256_permute2f128_si256(in[i + 2], in[i + 3], 0x31);

                v0 = _mm256_add_epi32(v0, x0);
                v1 = _mm256_add_epi32(v1, x1);
                v2 = _mm256_add_epi32(v2, x2);
                v3 = _mm256_add_epi32(v3, x3);

                v0 = _mm256_packus_epi32(v0, v1);
                v2 = _mm256_packus_epi32(v2, v3);

                v0 = highbd_clamp_epi32(v0, bd);
                v2 = highbd_clamp_epi32(v2, bd);

                //_mm256_storeu_si256((__m256i *)output, v0);
                //_mm256_storeu_si256((__m256i *)(output + 16), v2);

                tu0 = _mm256_packus_epi16(v0, v2);
                tx1 = _mm256_permute4x64_epi64(tu0, 0xd8);
                _mm256_storeu_si256((__m256i *)output8, tx1);

                //output += stride;
                output8 += stride8;
                i += 4;
            }
    }

    void av1_inv_txfm2d_add_32x32_avx2(const int32_t *coeff, uint16_t *output, int stride, int tx_type, int bd, int useAdd)
    {
        __m256i in[128], out[128];
        const int8_t *shift = inv_txfm_shift_ls[TX_32X32];
        const int txw_idx = TX_32X32;
        const int txh_idx = TX_32X32;

        switch (tx_type) {
        case DCT_DCT:
            load_buffer_32x32_view_src_as_16s(coeff, in);
            transpose_32x32(in, out);
            idct32_avx2(out, in, inv_cos_bit_row[txw_idx][txh_idx]);
            round_shift_32x32(in, -shift[0]);
            transpose_32x32(in, out);
            idct32_avx2(out, in, inv_cos_bit_col[txw_idx][txh_idx]);
            if (useAdd)
                write_buffer_32x32_view_dst_as_8u(in, output, stride, 0, 0, -shift[1], bd);
            else
                write_buffer_32x32_noclamp(in, output, stride, 0, 0, -shift[1], bd);
            break;
        default: assert(0);
        }
    }

}; // namespace details

namespace VP9PP {

    template <int size, int type> void itransform_hbd_sse4(const int32_t *src, int16_t *dst, int pitchDst) {
        if (size == 0)      details::av1_inv_txfm2d_add_4x4_sse4_1(src, (uint16_t*)dst, pitchDst, type, 8, 0);
        else if (size == 1) details::av1_inv_txfm2d_add_8x8_sse4_1(src, (uint16_t*)dst, pitchDst, type, 8, 0);
        else if (size == 2) details::av1_inv_txfm2d_add_16x16_sse4_1(src, (uint16_t*)dst, pitchDst, type, 8, 0);
        else if (size == 3) details::av1_inv_txfm2d_add_32x32_avx2(src, (uint16_t*)dst, pitchDst, type, 8, 0);
        else {assert(0);}
    }

    template void itransform_hbd_sse4<0, 0>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<0, 1>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<0, 2>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<0, 3>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<1, 0>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<1, 1>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<1, 2>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<1, 3>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<2, 0>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<2, 1>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<2, 2>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<2, 3>(const int32_t*,int16_t*,int);
    template void itransform_hbd_sse4<3, 0>(const int32_t*,int16_t*,int);

    template <int size, int type> void itransform_add_hbd_sse4(const int32_t *src, uint16_t *dst, int pitchDst) {
        if (size == 0)      details::av1_inv_txfm2d_add_4x4_sse4_1(src, dst, pitchDst, type, 8, 1);
        else if (size == 1) details::av1_inv_txfm2d_add_8x8_sse4_1(src, dst, pitchDst, type, 8, 1);
        else if (size == 2) details::av1_inv_txfm2d_add_16x16_sse4_1(src, dst, pitchDst, type, 8, 1);
        else if (size == 3) details::av1_inv_txfm2d_add_32x32_avx2(src, dst, pitchDst, type, 8, 1);
        else {assert(0);}
    }

    template void itransform_add_hbd_sse4<0, 0>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<0, 1>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<0, 2>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<0, 3>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<1, 0>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<1, 1>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<1, 2>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<1, 3>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<2, 0>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<2, 1>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<2, 2>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<2, 3>(const int32_t*,uint16_t*,int);
    template void itransform_add_hbd_sse4<3, 0>(const int32_t*,uint16_t*,int);
}; // namespace VP9PP