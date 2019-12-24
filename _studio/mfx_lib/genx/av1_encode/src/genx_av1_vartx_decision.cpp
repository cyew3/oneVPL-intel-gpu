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


#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#pragma warning(disable: 4458)
#pragma warning(disable: 4100)
#pragma warning(disable: 4389)
#pragma warning(disable: 4456)

#include <cm/cm.h>
#include <cm/cmtl.h>
//#include <cm/genx_vme.h>
typedef unsigned char       uint1;  // unsigned byte
typedef unsigned short      uint2;  // unsigned word
typedef unsigned int        uint4;  // unsigned dword
typedef unsigned long long  uint8;  // unsigned qword
typedef char                int1;   // byte
typedef short               int2;   // word
typedef int                 int4;   // dword
typedef long long           int8;   // qword

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38F
#endif

#if CMRT_EMU
#define assert_emu assert
#else
#define assert_emu(...) (void)0
#endif

static const uint2 SIZEOF_MODE_INFO = 32;
static const uint2 SIZEOF_ADZ_CTX = 32;

typedef matrix    <uint1,1,SIZEOF_MODE_INFO> mode_info;
typedef matrix_ref<uint1,1,SIZEOF_MODE_INFO> mode_info_ref;

typedef vector<int2, 10>     qparam_t;
typedef vector_ref<int2, 10> qparam_ref;

enum { REF0 = 0, REF1 = 1 };
enum { X = 0, Y = 1 };
enum { OUT_OF_PIC = 255 };
enum { TX_4X4, TX_8X8, TX_16X16, TX_32X32 };
enum { DCT_DCT, ADST_DCT, DCT_ADST, ADST_ADST };

enum { COEF_SSE_SHIFT = 6 };
enum { COEF_SSE_SHIFT_32 = 4 };

static const uint OFFSET_SKIP         = 112;
static const uint OFFSET_INIT_DZ      = 5052;
static const uint OFFSET_FIT_CURVE    = 5056;
static const uint OFFSET_TX_SIZE_STEP = 888;

static const uint OFFSET_EOB_MULTI_4         = 1500;
static const uint OFFSET_COEFF_EOB_DC_4      = OFFSET_EOB_MULTI_4         + 2 * 11;
static const uint OFFSET_COEFF_BASE_EOB_AC_4 = OFFSET_COEFF_EOB_DC_4      + 2 * 16;
static const uint OFFSET_TXB_SKIP_4          = OFFSET_COEFF_BASE_EOB_AC_4 + 2 * 3;
static const uint OFFSET_COEFF_BASE_4        = OFFSET_TXB_SKIP_4          + 2 * 14;
static const uint OFFSET_COEFF_BR_ACC_4      = OFFSET_COEFF_BASE_4        + 2 * 64;

static const uint OFFSET_EOB_MULTI_8         = OFFSET_EOB_MULTI_4         + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_EOB_DC_8      = OFFSET_COEFF_EOB_DC_4      + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_BASE_EOB_AC_8 = OFFSET_COEFF_BASE_EOB_AC_4 + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_TXB_SKIP_8          = OFFSET_TXB_SKIP_4          + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_BASE_8        = OFFSET_COEFF_BASE_4        + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_BR_ACC_8      = OFFSET_COEFF_BR_ACC_4      + OFFSET_TX_SIZE_STEP;

static const uint OFFSET_EOB_MULTI_16         = OFFSET_EOB_MULTI_8         + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_EOB_DC_16      = OFFSET_COEFF_EOB_DC_8      + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_BASE_EOB_AC_16 = OFFSET_COEFF_BASE_EOB_AC_8 + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_TXB_SKIP_16          = OFFSET_TXB_SKIP_8          + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_BASE_16        = OFFSET_COEFF_BASE_8        + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_BR_ACC_16      = OFFSET_COEFF_BR_ACC_8      + OFFSET_TX_SIZE_STEP;

static const uint OFFSET_EOB_MULTI_32         = OFFSET_EOB_MULTI_16         + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_EOB_DC_32      = OFFSET_COEFF_EOB_DC_16      + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_BASE_EOB_AC_32 = OFFSET_COEFF_BASE_EOB_AC_16 + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_TXB_SKIP_32          = OFFSET_TXB_SKIP_16          + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_BASE_32        = OFFSET_COEFF_BASE_16        + OFFSET_TX_SIZE_STEP;
static const uint OFFSET_COEFF_BR_ACC_32      = OFFSET_COEFF_BR_ACC_16      + OFFSET_TX_SIZE_STEP;

static uint1 ZIGZAG_SCAN[64] = {
    0,  1,  8,  9,  2,  3, 10, 11,
    16, 17, 24, 25, 18, 19, 26, 27,
    4,  5, 12, 13,  6,  7, 14, 15,
    20, 21, 28, 29, 22, 23, 30, 31,
    32, 33, 40, 41, 34, 35, 42, 43,
    48, 49, 56, 57, 50, 51, 58, 59,
    36, 37, 44, 45, 38, 39, 46, 47,
    52, 53, 60, 61, 54, 55, 62, 63
};

static uint1 SCAN_4X4[16] = {
    0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

static uint1 SCAN_4X4_TRANSPOSED[16] = {
    0, 4, 1, 2, 5, 8, 12, 9, 6, 3, 7, 10, 13, 14, 11, 15
};

static const uint1 SCAN_8X8[64] = {
    0,  1,  8,  16, 9,  2,  3,  10,
    17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

static const uint1 SCAN_8X8_TRANSPOSED[64] = {
    0,  8,  1,  2,  9, 16, 24, 17,
    10,  3,  4, 11, 18, 25, 32, 40,
    33, 26, 19, 12,  5,  6, 13, 20,
    27, 34, 41, 48, 56, 49, 42, 35,
    28, 21, 14,  7, 15, 22, 29, 36,
    43, 50, 57, 58, 51, 44, 37, 30,
    23, 31, 38, 45, 52, 59, 60, 53,
    46, 39, 47, 54, 61, 62, 55, 63
};

static const uint1 BASE_CTX_OFFSET_4[16] = {
    0,  1,  6,  6,
    1,  6,  6, 11,
    6,  6, 11, 11,
    6, 11, 11, 11,
};

static const uint1 BASE_CTX_OFFSET_00_63[64] = {
     0,  1,  6,  6, 11, 11, 11, 11,
     1,  6,  6, 11, 11, 11, 11, 11,
     6,  6, 11, 11, 11, 11, 11, 11,
     6, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
};

static const uint1 BR_CTX_OFFSET_4[16] = {
     0,  7, 14, 14,
     7,  7, 14, 14,
    14, 14, 14, 14,
    14, 14, 14, 14,
};

static const uint1 BR_CTX_OFFSET_00_63[64] = {
      0,  7, 14, 14, 14, 14, 14, 14,
      7,  7, 14, 14, 14, 14, 14, 14,
     14, 14, 14, 14, 14, 14, 14, 14,
     14, 14, 14, 14, 14, 14, 14, 14,
     14, 14, 14, 14, 14, 14, 14, 14,
     14, 14, 14, 14, 14, 14, 14, 14,
     14, 14, 14, 14, 14, 14, 14, 14,
     14, 14, 14, 14, 14, 14, 14, 14,
};

static const int2 COSPI_12[32] = {
    4096, 4091, 4076, 4052, 4017, 3973, 3920, 3857,
    3784, 3703, 3612, 3513, 3406, 3290, 3166, 3035,
    2896, 2751, 2598, 2440, 2276, 2106, 1931, 1751,
    1567, 1380, 1189,  995,  799,  601,  401,  201
};

static const int2 COSPI_VP9[16] = {
    16384, 16305, 16069, 15679, 15137, 14449, 13623, 12665,
    11585, 10394,  9102,  7723,  6270,  4756,  3196,  1606
};

static const int2 COSPI_13[8] = {
    8192, 8035, 7568, 6811, 5793, 4551, 3135, 1598
};

_GENX_ vector<uint4, 8>      g_zeroes;
_GENX_ vector<uint2, 32>     g_sequence_0_to_31;
_GENX_ vector<uint1, 32>     g_params;
_GENX_ vector<uint1, 32>     g_scan_8x8_t; // transposed
_GENX_ vector<uint1, 16>     g_scan_4x4_t; // transposed
_GENX_ vector<uint1, 32>     g_br_ctx_offset_00_31;
_GENX_ vector<uint1, 32>     g_base_ctx_offset_00_31;
_GENX_ vector<uint1, 16>     g_br_ctx_offset_4;
_GENX_ vector<uint1, 16>     g_base_ctx_offset_4;
_GENX_ vector<uint4, 16>     g_high_freq_sum;
_GENX_ matrix<uint2, 8, 8>   g_vartx_info;
_GENX_ matrix<int2, 16, 16>  g_coef_16_lo;
_GENX_ matrix<int2, 16, 16>  g_coef_16_orig;
_GENX_ matrix<int2, 16, 16>  g_coef_16_hi;
_GENX_ matrix<int2, 8, 8>    g_coef_8;
_GENX_ matrix<int2, 8, 8>    g_coef_8_orig;
_GENX_ matrix<int2, 4, 16>   g_coef_4;
_GENX_ matrix<int2, 4, 16>   g_coef_4_orig;
_GENX_ matrix<int2, 8, 8>    g_diff_8;
_GENX_ matrix<int2, 8, 8>    g_err_8;
_GENX_ matrix<int2, 4, 16>   g_err_4;
_GENX_ matrix<uint2, 4, 2>   g_high_freq_coef_fit_curve;
_GENX_ uint2                 g_txb_skip_ctx;
_GENX_ qparam_t              g_qparam;
_GENX_ uint2                 g_num_nz;
_GENX_ int2                  g_last_diag;
_GENX_ int                   g_pred_padding;

_GENX_ vector<float, 2>      g_curr_adz;
_GENX_ vector<float, 2>      g_round_adj;

typedef vector<uint4, 6> rd_cost;
enum { SSE=0, BIT=1, NZC=2, DZ0=4, DZ1=5 };
_GENX_ rd_cost               g_rd_cost4;
_GENX_ rd_cost               g_rd_cost8;
_GENX_ rd_cost               g_rd_cost16;
_GENX_ rd_cost               g_rd_cost32;

_GENX_ inline uint2      get_width()      { return g_params.format<uint2>()[0 / 2]; }
_GENX_ inline uint2      get_height()     { return g_params.format<uint2>()[2 / 2]; }
_GENX_ inline float      get_lambda()     { return g_params.format<float>()[4 / 4]; }
_GENX_ inline qparam_t   get_qparam()     { return g_params.format<int2>().select<10, 1>(8 / 2); }
_GENX_ inline qparam_ref get_qparam_ref() { return g_params.format<int2>().select<10, 1>(8 / 2); }
_GENX_ inline uint1      get_comp_flag()  { return g_params.format<uint1>()[30]; }
_GENX_ inline uint1      get_bidir_flag() { return !g_params.format<uint1>()[31]; }

_GENX_ inline uint1 get_sb_type(mode_info_ref mi) { return mi.format<uint1>()[19]; }
_GENX_ inline uint1 get_mode   (mode_info_ref mi) { return mi.format<uint1>()[20]; }
_GENX_ inline uint1 get_skip   (mode_info_ref mi) { return mi.format<uint1>()[24]; }
_GENX_ inline void  set_skip1  (mode_info_ref mi) { mi.format<uint1>()[24] = 1; }
_GENX_ inline void  set_skip0  (mode_info_ref mi) { mi.format<uint1>()[24] = 0; }

_GENX_ vector<uint4, 16> g_sse;

_GENX_ inline void sse_8(matrix_ref<uint1, 8, 8> src, matrix_ref<uint1, 8, 8> pred)
{
    vector<int2, 16> diff;
    diff = src.select<2, 1, 8, 1>(0) - pred.select<2, 1, 8, 1>(0);
    g_sse  = cm_abs<uint2>(diff) * cm_abs<uint2>(diff);
    diff = src.select<2, 1, 8, 1>(2) - pred.select<2, 1, 8, 1>(2);
    g_sse += cm_abs<uint2>(diff) * cm_abs<uint2>(diff);
    diff = src.select<2, 1, 8, 1>(4) - pred.select<2, 1, 8, 1>(4);
    g_sse += cm_abs<uint2>(diff) * cm_abs<uint2>(diff);
    diff = src.select<2, 1, 8, 1>(6) - pred.select<2, 1, 8, 1>(6);
    g_sse += cm_abs<uint2>(diff) * cm_abs<uint2>(diff);
}

_GENX_ inline void sse_16_acc(matrix_ref<uint1, 16, 16> src, matrix_ref<uint1, 16, 16> pred)
{
    vector<int2, 16> diff;
    #pragma unroll
    for (int i = 0; i < 16; i++) {
        diff = src.row(i) - pred.row(i);
        g_sse += cm_abs<uint2>(diff) * cm_abs<uint2>(diff);
    }
}

_GENX_ inline vector<int2, 16> half_btf_16(int2 w0, vector_ref<int2, 16> x0, int2 w1, vector_ref<int2, 16> x1, uint round, uint shift)
{
    vector<int4, 16> res;
    res = w0 * x0;
    res += w1 * x1;
    res += round;
    return res >> shift;
}

_GENX_ /*inline*/ void dct_4x4x4()
{
    matrix<int2, 4, 16> u, v;
    matrix_ref<int2, 4, 16> coef4x4x4 = g_coef_4;
    uint2 shift = 13;
    uint2 rounding = (1 << shift) >> 1;

    // stage 1
    u.row(0) = coef4x4x4.row(0) + coef4x4x4.row(3);
    u.row(1) = coef4x4x4.row(1) + coef4x4x4.row(2);
    u.row(2) = coef4x4x4.row(1) - coef4x4x4.row(2);
    u.row(3) = coef4x4x4.row(0) - coef4x4x4.row(3);

    // stage 2
    // stage 3
    coef4x4x4.row(0) = half_btf_16( COSPI_13[32 / 8], u.row(0),  COSPI_13[32 / 8], u.row(1), rounding, shift);
    coef4x4x4.row(1) = half_btf_16( COSPI_13[48 / 8], u.row(2),  COSPI_13[16 / 8], u.row(3), rounding, shift);
    coef4x4x4.row(2) = half_btf_16( COSPI_13[32 / 8], u.row(0), -COSPI_13[32 / 8], u.row(1), rounding, shift);
    coef4x4x4.row(3) = half_btf_16(-COSPI_13[16 / 8], u.row(2),  COSPI_13[48 / 8], u.row(3), rounding, shift);
}

_GENX_ void dct_8x8()
{
    matrix<int2, 8, 8> u, v;
    uint2 shift = 13;
    uint2 rounding = (1 << shift) >> 1;

    // stage 1
    u.row(0) = g_coef_8.row(0) + g_coef_8.row(7);
    u.row(1) = g_coef_8.row(1) + g_coef_8.row(6);
    u.row(3) = g_coef_8.row(2) + g_coef_8.row(5);
    u.row(2) = g_coef_8.row(3) + g_coef_8.row(4);
    u.row(4) = g_coef_8.row(3) - g_coef_8.row(4);
    u.row(5) = g_coef_8.row(0) - g_coef_8.row(7);
    u.row(6) = g_coef_8.row(2) - g_coef_8.row(5);
    u.row(7) = g_coef_8.row(1) - g_coef_8.row(6);

    // stage 2
    v.select<2, 1, 8, 1>(0) = u.select<2, 1, 8, 1>(0) + u.select<2, 1, 8, 1>(2);
    v.select<2, 1, 8, 1>(2) = u.select<2, 1, 8, 1>(0) - u.select<2, 1, 8, 1>(2);
    matrix<int4, 8, 8> tmp;
    tmp.row(0) = -COSPI_13[32 / 8] * u.row(6) + COSPI_13[32 / 8] * u.row(7);
    tmp.row(1) =  COSPI_13[32 / 8] * u.row(6) + COSPI_13[32 / 8] * u.row(7);
    tmp.select<2, 1, 8, 1>() += rounding;
    tmp.select<2, 1, 8, 1>() >>= shift;
    v.select<2, 1, 8, 1>(4) = tmp.select<2, 1, 8, 1>();

    // stage 3
    matrix<int2, 4, 8> t;
    t.select<2, 1, 8, 1>(0) = u.select<2, 1, 8, 1>(4) + v.select<2, 1, 8, 1>(4);
    t.select<2, 1, 8, 1>(2) = u.select<2, 1, 8, 1>(4) - v.select<2, 1, 8, 1>(4);

    // stage 3
    // stage 4
    // stage 5
    tmp.row(0) =  COSPI_13[32 / 8] * v.row(0) + COSPI_13[32 / 8] * v.row(1);
    tmp.row(1) =  COSPI_13[56 / 8] * t.row(0) + COSPI_13[ 8 / 8] * t.row(1);
    tmp.row(2) =  COSPI_13[16 / 8] * v.row(2) + COSPI_13[48 / 8] * v.row(3);
    tmp.row(3) = -COSPI_13[40 / 8] * t.row(2) + COSPI_13[24 / 8] * t.row(3);
    tmp.row(4) =  COSPI_13[32 / 8] * v.row(0) - COSPI_13[32 / 8] * v.row(1);
    tmp.row(5) =  COSPI_13[24 / 8] * t.row(2) + COSPI_13[40 / 8] * t.row(3);
    tmp.row(6) =  COSPI_13[48 / 8] * v.row(2) - COSPI_13[16 / 8] * v.row(3);
    tmp.row(7) = -COSPI_13[ 8 / 8] * t.row(0) + COSPI_13[56 / 8] * t.row(1);
    tmp += rounding;
    g_coef_8 = tmp >> shift;
}

_GENX_ void dct_16x16()
{
    matrix<int2, 8, 16> input, step1, step2, step3;
    matrix<int2, 8, 16> s;
    matrix<int2, 4, 16> x, t;
    uint2 shift = 14;
    uint2 rounding = (1 << shift) >> 1;

    // stage 1
    input.row(0) = g_coef_16_lo.row(0) + g_coef_16_lo.row(15);
    input.row(1) = g_coef_16_lo.row(1) + g_coef_16_lo.row(14);
    input.row(2) = g_coef_16_lo.row(2) + g_coef_16_lo.row(13);
    input.row(3) = g_coef_16_lo.row(3) + g_coef_16_lo.row(12);
    input.row(4) = g_coef_16_lo.row(4) + g_coef_16_lo.row(11);
    input.row(5) = g_coef_16_lo.row(5) + g_coef_16_lo.row(10);
    input.row(6) = g_coef_16_lo.row(6) + g_coef_16_lo.row(9);
    input.row(7) = g_coef_16_lo.row(7) + g_coef_16_lo.row(8);
    step1.row(0) = g_coef_16_lo.row(7) - g_coef_16_lo.row(8);
    step1.row(1) = g_coef_16_lo.row(6) - g_coef_16_lo.row(9);
    step1.row(2) = g_coef_16_lo.row(5) - g_coef_16_lo.row(10);
    step1.row(3) = g_coef_16_lo.row(4) - g_coef_16_lo.row(11);
    step1.row(4) = g_coef_16_lo.row(3) - g_coef_16_lo.row(12);
    step1.row(5) = g_coef_16_lo.row(2) - g_coef_16_lo.row(13);
    step1.row(6) = g_coef_16_lo.row(1) - g_coef_16_lo.row(14);
    step1.row(7) = g_coef_16_lo.row(0) - g_coef_16_lo.row(15);

    // fdct8
    s.row(0) = input.row(0) + input.row(7);
    s.row(1) = input.row(1) + input.row(6);
    s.row(2) = input.row(2) + input.row(5);
    s.row(3) = input.row(3) + input.row(4);
    s.row(4) = input.row(3) - input.row(4);
    s.row(5) = input.row(2) - input.row(5);
    s.row(6) = input.row(1) - input.row(6);
    s.row(7) = input.row(0) - input.row(7);

    // fdct4
    x.row(0) = s.row(0) + s.row(3);
    x.row(1) = s.row(1) + s.row(2);
    x.row(2) = s.row(1) - s.row(2);
    x.row(3) = s.row(0) - s.row(3);
    g_coef_16_lo.row( 0) = half_btf_16( COSPI_VP9[16/2], x.row(0),  COSPI_VP9[16/2], x.row(1), rounding, shift);
    g_coef_16_lo.row( 8) = half_btf_16( COSPI_VP9[16/2], x.row(0), -COSPI_VP9[16/2], x.row(1), rounding, shift);
    g_coef_16_lo.row( 4) = half_btf_16( COSPI_VP9[24/2], x.row(2),  COSPI_VP9[ 8/2], x.row(3), rounding, shift);
    g_coef_16_lo.row(12) = half_btf_16(-COSPI_VP9[ 8/2], x.row(2),  COSPI_VP9[24/2], x.row(3), rounding, shift);

    // stage 2
    t.row(2) = half_btf_16(COSPI_VP9[16/2], s.row(6), -COSPI_VP9[16/2], s.row(5), rounding, shift);
    t.row(3) = half_btf_16(COSPI_VP9[16/2], s.row(6),  COSPI_VP9[16/2], s.row(5), rounding, shift);

    // stage 3
    x.row(0) = s.row(4) + t.row(2);
    x.row(1) = s.row(4) - t.row(2);
    x.row(2) = s.row(7) - t.row(3);
    x.row(3) = s.row(7) + t.row(3);

    // stage 4
    g_coef_16_lo.row( 2) = half_btf_16( COSPI_VP9[28/2], x.row(0), COSPI_VP9[ 4/2], x.row(3), rounding, shift);
    g_coef_16_lo.row( 6) = half_btf_16(-COSPI_VP9[20/2], x.row(1), COSPI_VP9[12/2], x.row(2), rounding, shift);
    g_coef_16_lo.row(10) = half_btf_16( COSPI_VP9[12/2], x.row(1), COSPI_VP9[20/2], x.row(2), rounding, shift);
    g_coef_16_lo.row(14) = half_btf_16(-COSPI_VP9[ 4/2], x.row(0), COSPI_VP9[28/2], x.row(3), rounding, shift);


    // step 2
    step2.row(2) = half_btf_16(COSPI_VP9[16/2], step1.row(5), -COSPI_VP9[16/2], step1.row(2), rounding, shift);
    step2.row(3) = half_btf_16(COSPI_VP9[16/2], step1.row(4), -COSPI_VP9[16/2], step1.row(3), rounding, shift);
    step2.row(4) = half_btf_16(COSPI_VP9[16/2], step1.row(3),  COSPI_VP9[16/2], step1.row(4), rounding, shift);
    step2.row(5) = half_btf_16(COSPI_VP9[16/2], step1.row(2),  COSPI_VP9[16/2], step1.row(5), rounding, shift);

    // step 3
    step3.row(0) = step1.row(0) + step2.row(3);
    step3.row(1) = step1.row(1) + step2.row(2);
    step3.row(2) = step1.row(1) - step2.row(2);
    step3.row(3) = step1.row(0) - step2.row(3);
    step3.row(4) = step1.row(7) - step2.row(4);
    step3.row(5) = step1.row(6) - step2.row(5);
    step3.row(6) = step1.row(6) + step2.row(5);
    step3.row(7) = step1.row(7) + step2.row(4);

    // step 4
    step2.row(1) = half_btf_16(-COSPI_VP9[ 8/2], step3.row(1),  COSPI_VP9[24/2], step3.row(6), rounding, shift);
    step2.row(2) = half_btf_16( COSPI_VP9[24/2], step3.row(2),  COSPI_VP9[ 8/2], step3.row(5), rounding, shift);
    step2.row(5) = half_btf_16( COSPI_VP9[ 8/2], step3.row(2), -COSPI_VP9[24/2], step3.row(5), rounding, shift);
    step2.row(6) = half_btf_16( COSPI_VP9[24/2], step3.row(1),  COSPI_VP9[ 8/2], step3.row(6), rounding, shift);

    // step 5
    step1.row(0) = step3.row(0) + step2.row(1);
    step1.row(1) = step3.row(0) - step2.row(1);
    step1.row(2) = step3.row(3) + step2.row(2);
    step1.row(3) = step3.row(3) - step2.row(2);
    step1.row(4) = step3.row(4) - step2.row(5);
    step1.row(5) = step3.row(4) + step2.row(5);
    step1.row(6) = step3.row(7) - step2.row(6);
    step1.row(7) = step3.row(7) + step2.row(6);

    // step 6
    g_coef_16_lo.row( 1) = half_btf_16( COSPI_VP9[30/2], step1.row(0), COSPI_VP9[ 2/2], step1.row(7), rounding, shift);
    g_coef_16_lo.row( 9) = half_btf_16( COSPI_VP9[14/2], step1.row(1), COSPI_VP9[18/2], step1.row(6), rounding, shift);
    g_coef_16_lo.row( 5) = half_btf_16( COSPI_VP9[22/2], step1.row(2), COSPI_VP9[10/2], step1.row(5), rounding, shift);
    g_coef_16_lo.row(13) = half_btf_16( COSPI_VP9[ 6/2], step1.row(3), COSPI_VP9[26/2], step1.row(4), rounding, shift);
    g_coef_16_lo.row( 3) = half_btf_16(-COSPI_VP9[26/2], step1.row(3), COSPI_VP9[ 6/2], step1.row(4), rounding, shift);
    g_coef_16_lo.row(11) = half_btf_16(-COSPI_VP9[10/2], step1.row(2), COSPI_VP9[22/2], step1.row(5), rounding, shift);
    g_coef_16_lo.row( 7) = half_btf_16(-COSPI_VP9[18/2], step1.row(1), COSPI_VP9[14/2], step1.row(6), rounding, shift);
    g_coef_16_lo.row(15) = half_btf_16(-COSPI_VP9[ 2/2], step1.row(0), COSPI_VP9[30/2], step1.row(7), rounding, shift);
}

_GENX_ void dct_32x16()
{
    matrix<int2, 32, 16> u;
    matrix_ref<int2, 16, 16> lo = g_coef_16_lo;
    matrix_ref<int2, 16, 16> hi = g_coef_16_hi;
    uint2 shift = 12;
    uint2 rounding = (1 << shift) >> 1;

    // stage 1
    u.row( 0) = lo.row( 0) + hi.row(15);
    u.row( 1) = lo.row( 1) + hi.row(14);
    u.row( 2) = lo.row( 2) + hi.row(13);
    u.row( 3) = lo.row( 3) + hi.row(12);
    u.row( 4) = lo.row( 4) + hi.row(11);
    u.row( 5) = lo.row( 5) + hi.row(10);
    u.row( 6) = lo.row( 6) + hi.row( 9);
    u.row( 7) = lo.row( 7) + hi.row( 8);
    u.row( 8) = lo.row( 8) + hi.row( 7);
    u.row( 9) = lo.row( 9) + hi.row( 6);
    u.row(10) = lo.row(10) + hi.row( 5);
    u.row(11) = lo.row(11) + hi.row( 4);
    u.row(12) = lo.row(12) + hi.row( 3);
    u.row(13) = lo.row(13) + hi.row( 2);
    u.row(14) = lo.row(14) + hi.row( 1);
    u.row(15) = lo.row(15) + hi.row( 0);
    u.row(16) = lo.row(15) - hi.row( 0);
    u.row(17) = lo.row(14) - hi.row( 1);
    u.row(18) = lo.row(13) - hi.row( 2);
    u.row(19) = lo.row(12) - hi.row( 3);
    u.row(20) = lo.row(11) - hi.row( 4);
    u.row(21) = lo.row(10) - hi.row( 5);
    u.row(22) = lo.row( 9) - hi.row( 6);
    u.row(23) = lo.row( 8) - hi.row( 7);
    u.row(24) = lo.row( 7) - hi.row( 8);
    u.row(25) = lo.row( 6) - hi.row( 9);
    u.row(26) = lo.row( 5) - hi.row(10);
    u.row(27) = lo.row( 4) - hi.row(11);
    u.row(28) = lo.row( 3) - hi.row(12);
    u.row(29) = lo.row( 2) - hi.row(13);
    u.row(30) = lo.row( 1) - hi.row(14);
    u.row(31) = lo.row( 0) - hi.row(15);

    // stage 2
    matrix<int2, 16, 16> v;
    v.row(0) = u.row(0) + u.row(15);
    v.row(1) = u.row(1) + u.row(14);
    v.row(2) = u.row(2) + u.row(13);
    v.row(3) = u.row(3) + u.row(12);
    v.row(4) = u.row(4) + u.row(11);
    v.row(5) = u.row(5) + u.row(10);
    v.row(6) = u.row(6) + u.row(9);
    v.row(7) = u.row(7) + u.row(8);
    v.row(8) = u.row(7) - u.row(8);
    v.row(9) = u.row(6) - u.row(9);
    v.row(10) = u.row(5) - u.row(10);
    v.row(11) = u.row(4) - u.row(11);
    v.row(12) = u.row(3) - u.row(12);
    v.row(13) = u.row(2) - u.row(13);
    v.row(14) = u.row(1) - u.row(14);
    v.row(15) = u.row(0) - u.row(15);

    // stage 3
    u.row(0) = v.row(0) + v.row(7);
    u.row(1) = v.row(1) + v.row(6);
    u.row(2) = v.row(2) + v.row(5);
    u.row(3) = v.row(3) + v.row(4);
    u.row(4) = v.row(3) - v.row(4);
    u.row(5) = v.row(2) - v.row(5);
    u.row(6) = v.row(1) - v.row(6);
    u.row(7) = v.row(0) - v.row(7);
    // stage 4
    matrix<int2, 4, 16> t;
    t.row(0) = u.row(0) + u.row(3);
    t.row(1) = u.row(1) + u.row(2);
    t.row(2) = u.row(1) - u.row(2);
    t.row(3) = u.row(0) - u.row(3);
    // stage 5
    lo.row(0) = half_btf_16( COSPI_12[32 / 2], t.row(0),  COSPI_12[32 / 2], t.row(1), rounding, shift);
    hi.row(0) = half_btf_16(-COSPI_12[32 / 2], t.row(1),  COSPI_12[32 / 2], t.row(0), rounding, shift);
    lo.row(8) = half_btf_16( COSPI_12[48 / 2], t.row(2),  COSPI_12[16 / 2], t.row(3), rounding, shift);
    hi.row(8) = half_btf_16( COSPI_12[48 / 2], t.row(3), -COSPI_12[16 / 2], t.row(2), rounding, shift);

    // stage 4
    t.row(1) = half_btf_16(-COSPI_12[32 / 2], u.row(5), COSPI_12[32 / 2], u.row(6), rounding, shift);
    t.row(2) = half_btf_16( COSPI_12[32 / 2], u.row(6), COSPI_12[32 / 2], u.row(5), rounding, shift);
    // stage 5
    u.row(5) = u.row(4) - t.row(1);
    u.row(4) = u.row(4) + t.row(1);
    u.row(6) = u.row(7) - t.row(2);
    u.row(7) = u.row(7) + t.row(2);
    // stage 6
    lo.row( 4) = half_btf_16(COSPI_12[56 / 2], u.row(4),  COSPI_12[ 8 / 2], u.row(7), rounding, shift);
    hi.row( 4) = half_btf_16(COSPI_12[24 / 2], u.row(5),  COSPI_12[40 / 2], u.row(6), rounding, shift);
    lo.row(12) = half_btf_16(COSPI_12[24 / 2], u.row(6), -COSPI_12[40 / 2], u.row(5), rounding, shift);
    hi.row(12) = half_btf_16(COSPI_12[56 / 2], u.row(7), -COSPI_12[ 8 / 2], u.row(4), rounding, shift);
    // stage 7
    // stage 8

    // stage 3
    u.row(10) = half_btf_16(-COSPI_12[32 / 2], v.row(10), COSPI_12[32 / 2], v.row(13), rounding, shift);
    u.row(11) = half_btf_16(-COSPI_12[32 / 2], v.row(11), COSPI_12[32 / 2], v.row(12), rounding, shift);
    u.row(12) = half_btf_16( COSPI_12[32 / 2], v.row(12), COSPI_12[32 / 2], v.row(11), rounding, shift);
    u.row(13) = half_btf_16( COSPI_12[32 / 2], v.row(13), COSPI_12[32 / 2], v.row(10), rounding, shift);
    // stage 4
    v.row(11) = v.row( 8) - u.row(11);
    v.row( 8) = v.row( 8) + u.row(11);
    v.row(10) = v.row( 9) - u.row(10);
    v.row( 9) = v.row( 9) + u.row(10);
    v.row(12) = v.row(15) - u.row(12);
    v.row(13) = v.row(14) - u.row(13);
    v.row(14) = v.row(14) + u.row(13);
    v.row(15) = v.row(15) + u.row(12);
    // stage 5
    u.row( 9) = half_btf_16(-COSPI_12[16 / 2], v.row( 9), COSPI_12[48 / 2], v.row(14), rounding, shift);
    u.row(14) = half_btf_16( COSPI_12[16 / 2], v.row(14), COSPI_12[48 / 2], v.row( 9), rounding, shift);
    // stage 6
    v.row( 9) = v.row( 8) - u.row( 9);
    v.row( 8) = v.row( 8) + u.row( 9);
    v.row(14) = v.row(15) - u.row(14);
    v.row(15) = v.row(15) + u.row(14);
    // stage 7
    lo.row( 2) = half_btf_16(COSPI_12[60 / 2], v.row( 8),  COSPI_12[ 4 / 2], v.row(15), rounding, shift);
    hi.row( 2) = half_btf_16(COSPI_12[28 / 2], v.row( 9),  COSPI_12[36 / 2], v.row(14), rounding, shift);
    lo.row(14) = half_btf_16(COSPI_12[28 / 2], v.row(14), -COSPI_12[36 / 2], v.row( 9), rounding, shift);
    hi.row(14) = half_btf_16(COSPI_12[60 / 2], v.row(15), -COSPI_12[ 4 / 2], v.row( 8), rounding, shift);
    // stage 8

    // stage 5
    u.row(10) = half_btf_16(-COSPI_12[48 / 2], v.row(10), -COSPI_12[16 / 2], v.row(13), rounding, shift);
    u.row(13) = half_btf_16( COSPI_12[48 / 2], v.row(13), -COSPI_12[16 / 2], v.row(10), rounding, shift);
    // stage 6
    v.row(10) = v.row(11) - u.row(10);
    v.row(11) = v.row(11) + u.row(10);
    v.row(13) = v.row(12) - u.row(13);
    v.row(12) = v.row(12) + u.row(13);
    // stage 7
    lo.row(10) = half_btf_16(COSPI_12[44 / 2], v.row(10), COSPI_12[20 / 2], v.row(13), rounding, shift);
    hi.row(10) = half_btf_16(COSPI_12[12 / 2], v.row(11), COSPI_12[52 / 2], v.row(12), rounding, shift);
    lo.row( 6) = half_btf_16(COSPI_12[12 / 2], v.row(12), -COSPI_12[52 / 2], v.row(11), rounding, shift);
    hi.row( 6) = half_btf_16(COSPI_12[44 / 2], v.row(13), -COSPI_12[20 / 2], v.row(10), rounding, shift);
    // stage 8

    // stage 2
    v.row(20-16) = half_btf_16(-COSPI_12[32 / 2], u.row(20), COSPI_12[32 / 2], u.row(27), rounding, shift);
    v.row(21-16) = half_btf_16(-COSPI_12[32 / 2], u.row(21), COSPI_12[32 / 2], u.row(26), rounding, shift);
    v.row(22-16) = half_btf_16(-COSPI_12[32 / 2], u.row(22), COSPI_12[32 / 2], u.row(25), rounding, shift);
    v.row(23-16) = half_btf_16(-COSPI_12[32 / 2], u.row(23), COSPI_12[32 / 2], u.row(24), rounding, shift);
    v.row(24-16) = half_btf_16( COSPI_12[32 / 2], u.row(24), COSPI_12[32 / 2], u.row(23), rounding, shift);
    v.row(25-16) = half_btf_16( COSPI_12[32 / 2], u.row(25), COSPI_12[32 / 2], u.row(22), rounding, shift);
    v.row(26-16) = half_btf_16( COSPI_12[32 / 2], u.row(26), COSPI_12[32 / 2], u.row(21), rounding, shift);
    v.row(27-16) = half_btf_16( COSPI_12[32 / 2], u.row(27), COSPI_12[32 / 2], u.row(20), rounding, shift);
    // stage 3
    u.row(23) = u.row(16) - v.row(23 - 16);
    u.row(16) = u.row(16) + v.row(23 - 16);
    u.row(22) = u.row(17) - v.row(22 - 16);
    u.row(17) = u.row(17) + v.row(22 - 16);
    u.row(21) = u.row(18) - v.row(21 - 16);
    u.row(18) = u.row(18) + v.row(21 - 16);
    u.row(20) = u.row(19) - v.row(20 - 16);
    u.row(19) = u.row(19) + v.row(20 - 16);
    u.row(24) = u.row(31) - v.row(24 - 16);
    u.row(25) = u.row(30) - v.row(25 - 16);
    u.row(26) = u.row(29) - v.row(26 - 16);
    u.row(27) = u.row(28) - v.row(27 - 16);
    u.row(28) = u.row(28) + v.row(27 - 16);
    u.row(29) = u.row(29) + v.row(26 - 16);
    u.row(30) = u.row(30) + v.row(25 - 16);
    u.row(31) = u.row(31) + v.row(24 - 16);
    // stage 4
    v.row(18-16) = half_btf_16(-COSPI_12[16 / 2], u.row(18),  COSPI_12[48 / 2], u.row(29), rounding, shift);
    v.row(19-16) = half_btf_16(-COSPI_12[16 / 2], u.row(19),  COSPI_12[48 / 2], u.row(28), rounding, shift);
    v.row(20-16) = half_btf_16(-COSPI_12[48 / 2], u.row(20), -COSPI_12[16 / 2], u.row(27), rounding, shift);
    v.row(21-16) = half_btf_16(-COSPI_12[48 / 2], u.row(21), -COSPI_12[16 / 2], u.row(26), rounding, shift);
    v.row(26-16) = half_btf_16( COSPI_12[48 / 2], u.row(26), -COSPI_12[16 / 2], u.row(21), rounding, shift);
    v.row(27-16) = half_btf_16( COSPI_12[48 / 2], u.row(27), -COSPI_12[16 / 2], u.row(20), rounding, shift);
    v.row(28-16) = half_btf_16( COSPI_12[16 / 2], u.row(28),  COSPI_12[48 / 2], u.row(19), rounding, shift);
    v.row(29-16) = half_btf_16( COSPI_12[16 / 2], u.row(29),  COSPI_12[48 / 2], u.row(18), rounding, shift);
    // stage 5
    u.row(19) = u.row(16) - v.row(19-16);
    u.row(16) = u.row(16) + v.row(19-16);
    u.row(18) = u.row(17) - v.row(18-16);
    u.row(17) = u.row(17) + v.row(18-16);
    u.row(21) = u.row(22) - v.row(21-16);
    u.row(22) = u.row(22) + v.row(21-16);
    u.row(20) = u.row(23) - v.row(20-16);
    u.row(23) = u.row(23) + v.row(20-16);
    u.row(27) = u.row(24) - v.row(27-16);
    u.row(24) = u.row(24) + v.row(27-16);
    u.row(26) = u.row(25) - v.row(26-16);
    u.row(25) = u.row(25) + v.row(26-16);
    u.row(29) = u.row(30) - v.row(29-16);
    u.row(30) = u.row(30) + v.row(29-16);
    u.row(28) = u.row(31) - v.row(28-16);
    u.row(31) = u.row(31) + v.row(28-16);
    // stage 6
    v.row(17-16) = half_btf_16(-COSPI_12[ 8 / 2], u.row(17),  COSPI_12[56 / 2], u.row(30), rounding, shift);
    v.row(18-16) = half_btf_16(-COSPI_12[56 / 2], u.row(18), -COSPI_12[ 8 / 2], u.row(29), rounding, shift);
    v.row(21-16) = half_btf_16(-COSPI_12[40 / 2], u.row(21),  COSPI_12[24 / 2], u.row(26), rounding, shift);
    v.row(22-16) = half_btf_16(-COSPI_12[24 / 2], u.row(22), -COSPI_12[40 / 2], u.row(25), rounding, shift);
    v.row(25-16) = half_btf_16( COSPI_12[24 / 2], u.row(25), -COSPI_12[40 / 2], u.row(22), rounding, shift);
    v.row(26-16) = half_btf_16( COSPI_12[40 / 2], u.row(26),  COSPI_12[24 / 2], u.row(21), rounding, shift);
    v.row(29-16) = half_btf_16( COSPI_12[56 / 2], u.row(29), -COSPI_12[ 8 / 2], u.row(18), rounding, shift);
    v.row(30-16) = half_btf_16( COSPI_12[ 8 / 2], u.row(30),  COSPI_12[56 / 2], u.row(17), rounding, shift);
    // stage 7
    u.row(17) = u.row(16) - v.row(17-16);
    u.row(16) = u.row(16) + v.row(17-16);
    u.row(18) = u.row(19) - v.row(18-16);
    u.row(19) = u.row(19) + v.row(18-16);
    u.row(21) = u.row(20) - v.row(21-16);
    u.row(20) = u.row(20) + v.row(21-16);
    u.row(22) = u.row(23) - v.row(22-16);
    u.row(23) = u.row(23) + v.row(22-16);
    u.row(25) = u.row(24) - v.row(25-16);
    u.row(24) = u.row(24) + v.row(25-16);
    u.row(26) = u.row(27) - v.row(26-16);
    u.row(27) = u.row(27) + v.row(26-16);
    u.row(29) = u.row(28) - v.row(29-16);
    u.row(28) = u.row(28) + v.row(29-16);
    u.row(30) = u.row(31) - v.row(30-16);
    u.row(31) = u.row(31) + v.row(30-16);
    // stage 8
    lo.row( 1) = half_btf_16(COSPI_12[62 / 2], u.row(16),  COSPI_12[ 2 / 2], u.row(31), rounding, shift);
    hi.row( 1) = half_btf_16(COSPI_12[30 / 2], u.row(17),  COSPI_12[34 / 2], u.row(30), rounding, shift);
    lo.row( 9) = half_btf_16(COSPI_12[46 / 2], u.row(18),  COSPI_12[18 / 2], u.row(29), rounding, shift);
    hi.row( 9) = half_btf_16(COSPI_12[14 / 2], u.row(19),  COSPI_12[50 / 2], u.row(28), rounding, shift);
    lo.row( 5) = half_btf_16(COSPI_12[54 / 2], u.row(20),  COSPI_12[10 / 2], u.row(27), rounding, shift);
    hi.row( 5) = half_btf_16(COSPI_12[22 / 2], u.row(21),  COSPI_12[42 / 2], u.row(26), rounding, shift);
    lo.row(13) = half_btf_16(COSPI_12[38 / 2], u.row(22),  COSPI_12[26 / 2], u.row(25), rounding, shift);
    hi.row(13) = half_btf_16(COSPI_12[ 6 / 2], u.row(23),  COSPI_12[58 / 2], u.row(24), rounding, shift);
    lo.row( 3) = half_btf_16(COSPI_12[ 6 / 2], u.row(24), -COSPI_12[58 / 2], u.row(23), rounding, shift);
    hi.row( 3) = half_btf_16(COSPI_12[38 / 2], u.row(25), -COSPI_12[26 / 2], u.row(22), rounding, shift);
    lo.row(11) = half_btf_16(COSPI_12[22 / 2], u.row(26), -COSPI_12[42 / 2], u.row(21), rounding, shift);
    hi.row(11) = half_btf_16(COSPI_12[54 / 2], u.row(27), -COSPI_12[10 / 2], u.row(20), rounding, shift);
    lo.row( 7) = half_btf_16(COSPI_12[14 / 2], u.row(28), -COSPI_12[50 / 2], u.row(19), rounding, shift);
    hi.row( 7) = half_btf_16(COSPI_12[46 / 2], u.row(29), -COSPI_12[18 / 2], u.row(18), rounding, shift);
    lo.row(15) = half_btf_16(COSPI_12[30 / 2], u.row(30), -COSPI_12[34 / 2], u.row(17), rounding, shift);
    hi.row(15) = half_btf_16(COSPI_12[62 / 2], u.row(31), -COSPI_12[ 2 / 2], u.row(16), rounding, shift);
}

_GENX_ inline void transpose_4x4x4()
{
    matrix_ref<int2, 4, 16> t = g_coef_4;
    matrix<int2, 4, 16> u, v;
    u.select<2, 1, 16, 1>(0) = t.replicate<16, 2, 2, 16>(0, 0);
    u.select<2, 1, 16, 1>(2) = t.replicate<16, 2, 2, 16>(0, 1);

    u.select<2, 1, 16, 1>(0) = t.select<4, 1, 8, 2>(0, 0);
    u.select<2, 1, 16, 1>(2) = t.select<4, 1, 8, 2>(0, 1);
    v.select<2, 1, 16, 1>(0) = u.select<4, 1, 8, 2>(0, 0);
    v.select<2, 1, 16, 1>(2) = u.select<4, 1, 8, 2>(0, 1);
    u.select<2, 1, 16, 1>(0) = v.select<4, 1, 8, 2>(0, 0);
    u.select<2, 1, 16, 1>(2) = v.select<4, 1, 8, 2>(0, 1);
    v.select<2, 1, 16, 1>(0) = u.select<4, 1, 8, 2>(0, 0);
    v.select<2, 1, 16, 1>(2) = u.select<4, 1, 8, 2>(0, 1);
    t.select<1, 1, 16, 1>(0) = v.select<4, 1, 4, 1>(0, 0);
    t.select<1, 1, 16, 1>(1) = v.select<4, 1, 4, 1>(0, 4);
    t.select<1, 1, 16, 1>(2) = v.select<4, 1, 4, 1>(0, 8);
    t.select<1, 1, 16, 1>(3) = v.select<4, 1, 4, 1>(0, 12);
}

_GENX_ inline void transpose_8x8()
{
    matrix_ref<int2, 8, 8> t = g_coef_8;
    matrix<int2, 8, 8> u, v;

    u.select<4, 1, 8, 1>(0, 0) = t.select<8, 1, 4, 2>(0, 0);
    u.select<4, 1, 8, 1>(4, 0) = t.select<8, 1, 4, 2>(0, 1);
    v.select<4, 1, 8, 1>(0, 0) = u.select<8, 1, 4, 2>(0, 0);
    v.select<4, 1, 8, 1>(4, 0) = u.select<8, 1, 4, 2>(0, 1);
    t.select<4, 1, 8, 1>(0, 0) = cm_avg<int2>(v.select<8, 1, 4, 2>(0, 0), 0);
    t.select<4, 1, 8, 1>(4, 0) = cm_avg<int2>(v.select<8, 1, 4, 2>(0, 1), 0);
}

_GENX_ void transpose_16x16()
{
    matrix<int2, 16, 16> t;
    matrix_ref<int2, 16, 16> u = g_coef_16_lo;

    t.select<8, 1, 16, 1>(0, 0) = u.select<16, 1, 8, 2>(0, 0);
    t.select<8, 1, 16, 1>(8, 0) = u.select<16, 1, 8, 2>(0, 1);
    u.select<8, 1, 16, 1>(0, 0) = t.select<16, 1, 8, 2>(0, 0);
    u.select<8, 1, 16, 1>(8, 0) = t.select<16, 1, 8, 2>(0, 1);
    t.select<8, 1, 16, 1>(0, 0) = u.select<16, 1, 8, 2>(0, 0);
    t.select<8, 1, 16, 1>(8, 0) = u.select<16, 1, 8, 2>(0, 1);
    u.select<8, 1, 16, 1>(0, 0) = cm_avg<int2>(t.select<16, 1, 8, 2>(0, 0), 0);
    u.select<8, 1, 16, 1>(8, 0) = cm_avg<int2>(t.select<16, 1, 8, 2>(0, 1), 0);
}

_GENX_ void transpose_32x16()
{
    transpose_16x16();

    matrix<int2, 16, 16> v;
    matrix_ref<int2, 16, 16> u = g_coef_16_hi;
    v.select<8, 1, 16, 1>(0, 0) = u.select<16, 1, 8, 2>(0, 0);
    v.select<8, 1, 16, 1>(8, 0) = u.select<16, 1, 8, 2>(0, 1);
    u.select<8, 1, 16, 1>(0, 0) = v.select<16, 1, 8, 2>(0, 0);
    u.select<8, 1, 16, 1>(8, 0) = v.select<16, 1, 8, 2>(0, 1);
    v.select<8, 1, 16, 1>(0, 0) = u.select<16, 1, 8, 2>(0, 0);
    v.select<8, 1, 16, 1>(8, 0) = u.select<16, 1, 8, 2>(0, 1);
    u.select<8, 1, 16, 1>(0, 0) = cm_avg<int2>(v.select<16, 1, 8, 2>(0, 0), 0);
    u.select<8, 1, 16, 1>(8, 0) = cm_avg<int2>(v.select<16, 1, 8, 2>(0, 1), 0);
}

_GENX_ inline uint4 quant_dequant_4x4x4()
{
    vector<int2, 16> zbin = g_qparam[1];
    vector<int2, 16> round = g_qparam[3];
    vector<int2, 16> quant = g_qparam[5];
    vector<int2, 16> shift = g_qparam[7];
    vector<int2, 16> scale = g_qparam[9];
    vector<int2, 16> coef, dqcoef;
    vector<int2, 16> diff;
    vector<uint4, 16> acc = 0;

    #pragma unroll
    for (int i = 1; i < 4; i++) {
        coef = g_coef_4.row(i);
        g_coef_4.row(i).format<uint2>() = cm_add<uint2>(cm_abs<uint2>(g_coef_4.row(i)), round, SAT);
        g_coef_4.row(i) += (g_coef_4.row(i) * quant).format<uint2>().select<16, 2>(1);
        g_coef_4.row(i) = (g_coef_4.row(i) * shift).format<uint2>().select<16, 2>(1);
        g_coef_4.row(i).merge(0, cm_abs<uint2>(coef) < zbin); // deadzoned
        dqcoef = cm_mul<int2>(g_coef_4.row(i), scale, SAT);
        g_coef_4.row(i).merge(-g_coef_4.row(i), coef < 0);
        g_err_4.row(i) = cm_abs<uint2>(coef) - dqcoef;
        acc += cm_abs<uint2>(g_err_4.row(i)) * cm_abs<uint2>(g_err_4.row(i));
    }

    zbin.select<4, 4>() = g_qparam[0];
    round.select<4, 4>() = g_qparam[2];
    quant.select<4, 4>() = g_qparam[4];
    shift.select<4, 4>() = g_qparam[6];
    scale.select<4, 4>() = g_qparam[8];

    coef = g_coef_4.row(0);
    g_coef_4.row(0).format<uint2>() = cm_add<uint2>(cm_abs<uint2>(g_coef_4.row(0)), round, SAT);
    g_coef_4.row(0) += (g_coef_4.row(0) * quant).format<uint2>().select<16, 2>(1);
    g_coef_4.row(0) = (g_coef_4.row(0) * shift).format<uint2>().select<16, 2>(1);
    g_coef_4.row(0).merge(0, cm_abs<uint2>(coef) < zbin); // deadzoned
    dqcoef = cm_mul<int2>(g_coef_4.row(0), scale, SAT);
    g_coef_4.row(0).merge(-g_coef_4.row(0), coef < 0);
    g_err_4.row(0) = cm_abs<uint2>(coef) - dqcoef;
    acc += cm_abs<uint2>(g_err_4.row(0)) * cm_abs<uint2>(g_err_4.row(0));

    acc.select<8, 1>() = acc.select<8, 2>(0) + acc.select<8, 2>(1);
    acc.select<4, 1>() = acc.select<4, 2>(0) + acc.select<4, 2>(1);
    uint4 sse = cm_sum<uint4>(acc.select<4, 1>() >> (uint)COEF_SSE_SHIFT);
    return sse;
}

_GENX_ uint4 quant_dequant_8()
{
    vector<int2, 16> zbin = g_qparam[1];
    vector<int2, 16> round = g_qparam[3];
    vector<int2, 16> quant = g_qparam[5];
    vector<int2, 16> shift = g_qparam[7];
    vector<int2, 16> scale = g_qparam[9];
    vector<int2, 16> coef, dqcoef;
    vector<int2, 16> diff;
    vector<uint4, 16> acc = 0;

    vector_ref<int2, 64> coef8 = g_coef_8.format<int2>();
    vector_ref<int2, 64> err8 = g_err_8.format<int2>();

    #pragma unroll
    for (int i = 16; i < 64; i += 16) {
        coef = coef8.select<16, 1>(i);
        coef8.select<16, 1>(i).format<uint2>() = cm_add<uint2>(cm_abs<uint2>(coef8.select<16, 1>(i)), round, SAT);
        coef8.select<16, 1>(i) += (coef8.select<16, 1>(i) * quant).format<uint2>().select<16, 2>(1);
        coef8.select<16, 1>(i) = (coef8.select<16, 1>(i) * shift).format<uint2>().select<16, 2>(1);
        coef8.select<16, 1>(i).merge(0, cm_abs<uint2>(coef) < zbin); // deadzoned
        dqcoef = cm_mul<int2>(coef8.select<16, 1>(i), scale, SAT);
        coef8.select<16, 1>(i).merge(-coef8.select<16, 1>(i), coef < 0);
        err8.select<16, 1>(i) = cm_abs<uint2>(coef) - dqcoef;
        acc += cm_abs<uint2>(err8.select<16, 1>(i)) * cm_abs<uint2>(err8.select<16, 1>(i));
    }

    zbin[0] = g_qparam[0];
    round[0] = g_qparam[2];
    quant[0] = g_qparam[4];
    shift[0] = g_qparam[6];
    scale[0] = g_qparam[8];

    coef = coef8.select<16, 1>(0);
    coef8.select<16, 1>(0).format<uint2>() = cm_add<uint2>(cm_abs<uint2>(coef8.select<16, 1>(0)), round, SAT);
    coef8.select<16, 1>(0) += (coef8.select<16, 1>(0) * quant).format<uint2>().select<16, 2>(1);
    coef8.select<16, 1>(0) = (coef8.select<16, 1>(0) * shift).format<uint2>().select<16, 2>(1);
    coef8.select<16, 1>(0).merge(0, cm_abs<uint2>(coef) < zbin); // deadzoned
    dqcoef = cm_mul<int2>(coef8.select<16, 1>(0), scale, SAT);
    coef8.select<16, 1>(0).merge(-coef8.select<16, 1>(0), coef < 0);
    err8.select<16, 1>(0)= cm_abs<uint2>(coef) - dqcoef;
    acc += cm_abs<uint2>(err8.select<16, 1>(0)) * cm_abs<uint2>(err8.select<16, 1>(0));

    uint4 sse = cm_sum<uint4>(acc);
    return sse;
}

_GENX_ uint4 quant_dequant_16(uint2 is32x32)
{
    vector<int2, 16> zbin = g_qparam[1];
    vector<int2, 16> round = g_qparam[3];
    vector<int2, 16> quant = g_qparam[5];
    vector<int2, 16> shift = g_qparam[7];
    vector<int2, 16> scale = g_qparam[9];
    vector<int2, 16> coef, dqcoef;
    vector<int2, 16> diff;
    vector<uint4, 16> acc;

    acc = 0;
    #pragma unroll
    for (int i = 1; i < 16; i++) {
        coef = g_coef_16_lo.row(i);
        g_coef_16_lo.row(i).format<uint2>() = cm_add<uint2>(cm_abs<uint2>(g_coef_16_lo.row(i)), round, SAT);
        g_coef_16_lo.row(i) += (g_coef_16_lo.row(i) * quant).format<uint2>().select<16, 2>(1);
        g_coef_16_lo.row(i) = (g_coef_16_lo.row(i) * shift).format<uint2>().select<16, 2>(1);
        g_coef_16_lo.row(i).merge(0, cm_abs<uint2>(coef) < zbin); // deadzoned
        dqcoef = cm_mul<int2>(g_coef_16_lo.row(i), scale, SAT);
        g_coef_16_lo.row(i).merge(-g_coef_16_lo.row(i), coef < 0);
        dqcoef >>= is32x32;
        diff = cm_abs<uint2>(coef) - dqcoef;
        acc += cm_abs<uint2>(diff) * cm_abs<uint2>(diff);
        if (i < 8)
            g_err_8.row(i) = diff.select<8, 1>();
    }

    zbin[0] = g_qparam[0];
    round[0] = g_qparam[2];
    quant[0] = g_qparam[4];
    shift[0] = g_qparam[6];
    scale[0] = g_qparam[8];

    coef = g_coef_16_lo.row(0);
    g_coef_16_lo.row(0).format<uint2>() = cm_add<uint2>(cm_abs<uint2>(g_coef_16_lo.row(0)), round, SAT);
    g_coef_16_lo.row(0) += (g_coef_16_lo.row(0) * quant).format<uint2>().select<16, 2>(1);
    g_coef_16_lo.row(0) = (g_coef_16_lo.row(0) * shift).format<uint2>().select<16, 2>(1);
    g_coef_16_lo.row(0).merge(0, cm_abs<uint2>(coef) < zbin); // deadzoned
    dqcoef = cm_mul<int2>(g_coef_16_lo.row(0), scale, SAT);
    g_coef_16_lo.row(0).merge(-g_coef_16_lo.row(0), coef < 0);
    dqcoef >>= is32x32;
    diff = cm_abs<uint2>(coef) - dqcoef;
    acc += cm_abs<uint2>(diff) * cm_abs<uint2>(diff);
    g_err_8.row(0) = diff.select<8, 1>();

    uint4 sse = cm_sum<uint4>(acc);
    return sse;
}

_GENX_ uint4 quant_dequant_16_2()
{
    int2 zbin = g_qparam[1];
    int2 round = g_qparam[3];
    int2 quant = g_qparam[5];
    int2 shift = g_qparam[7];
    int2 scale = g_qparam[9];
    vector<int2, 16> coef, dqcoef;
    vector<int2, 16> diff;
    vector<uint4, 16> acc;

    acc = 0;
    #pragma unroll
    for (int i = 0; i < 16; i++) {
        coef = g_coef_16_hi.row(i);
        g_coef_16_hi.row(i).format<uint2>() = cm_add<uint2>(cm_abs<uint2>(coef), round, SAT);
        g_coef_16_hi.row(i) += (g_coef_16_hi.row(i) * quant).format<uint2>().select<16, 2>(1);
        g_coef_16_hi.row(i) = (g_coef_16_hi.row(i) * shift).format<uint2>().select<16, 2>(1);
        g_coef_16_hi.row(i).merge(0, cm_abs<uint2>(coef) < zbin);
        dqcoef = cm_mul<int2>(g_coef_16_hi.row(i), scale, SAT);
        g_coef_16_hi.row(i).merge(-g_coef_16_hi.row(i), coef < 0);
        dqcoef >>= 1; // 32x32
        diff = cm_abs<uint2>(coef) - dqcoef;
        acc += cm_abs<uint2>(diff) * cm_abs<uint2>(diff);
    }

    uint4 sse = cm_sum<uint4>(acc);
    return sse;
}

_GENX_ inline uint2 get_cost_txb_skip(SurfaceIndex PARAM, uint2 tx_size, uint2 value)
{
    assert_emu(tx_size <= 3);
    assert_emu(g_txb_skip_ctx <= 6);
    assert_emu(value <= 1);
    uint4 global_offset = OFFSET_TXB_SKIP_4 / sizeof(uint2) + tx_size * (OFFSET_TX_SIZE_STEP / sizeof(uint2)) + g_txb_skip_ctx * 2 + value;
    vector<uint2, 8> cost;
    read(PARAM, global_offset, g_zeroes, cost);
    return cost[0];
}

_GENX_ inline uint2 get_cost_eob_multi(SurfaceIndex PARAM, uint2 tx_size, uint2 log2_eob)
{
    assert_emu(tx_size <= 3);
    assert_emu(log2_eob <= 10);
    uint4 global_offset = OFFSET_EOB_MULTI_4 / sizeof(uint2) + tx_size * (OFFSET_TX_SIZE_STEP / sizeof(uint2)) + log2_eob;
    vector<uint2, 8> cost;
    read(PARAM, global_offset, g_zeroes, cost);
    return cost[0];
}

_GENX_ inline uint2 get_cost_coeff_eob_dc(SurfaceIndex PARAM, uint2 tx_size, uint2 level)
{
    assert_emu(tx_size <= 3);
    assert_emu(level <= 15);
    uint4 global_offset = OFFSET_COEFF_EOB_DC_4 / sizeof(uint2) + tx_size * (OFFSET_TX_SIZE_STEP / sizeof(uint2)) + level;
    vector<uint2, 8> cost;
    read(PARAM, global_offset, g_zeroes, cost);
    return cost[0];
}

_GENX_ inline uint2 get_cost_coeff_eob_ac(SurfaceIndex PARAM, uint2 tx_size, uint2 level)
{
    assert_emu(tx_size <= 3);
    assert_emu(level <= 2);
    uint4 global_offset = OFFSET_COEFF_BASE_EOB_AC_4 / sizeof(uint2) + tx_size * (OFFSET_TX_SIZE_STEP / sizeof(uint2)) + level;
    vector<uint2, 8> cost;
    read(PARAM, global_offset, g_zeroes, cost);
    return cost[0];
}

_GENX_ inline vector<uint2, 16> get_cost_coeff_br(SurfaceIndex PARAM, uint2 tx_size, vector_ref<uint2, 16> offsets)
{
    assert_emu(tx_size <= 3);
    vector<uint2, 16> cost;
    vector<uint4, 16> offsets_ = offsets;
    uint4 offset = OFFSET_COEFF_BR_ACC_4 / sizeof(uint2) + tx_size * OFFSET_TX_SIZE_STEP / sizeof(uint2);
    read(CONSTANT(PARAM), offset, offsets_, cost);
    return cost;
}

_GENX_ inline vector<uint2, 16> get_cost_coeff_base(SurfaceIndex PARAM, uint2 tx_size, vector_ref<uint1, 16> offsets)
{
    assert_emu(tx_size <= 3);
    vector<uint2, 16> cost;
    vector<uint4, 16> offsets_ = offsets;
    uint4 offset = OFFSET_COEFF_BASE_4 / sizeof(uint2) + tx_size * (OFFSET_TX_SIZE_STEP / sizeof(uint2));
    read(CONSTANT(PARAM), offset, offsets_, cost);
    return cost;
}

_GENX_ inline vector<uint2, 16> get_cost_coeff_base(SurfaceIndex PARAM, uint2 tx_size, vector_ref<uint2, 16> offsets)
{
    assert_emu(tx_size <= 3);
    vector<uint2, 16> cost;
    vector<uint4, 16> offsets_ = offsets;
    uint4 offset = OFFSET_COEFF_BASE_4 / sizeof(uint2) + tx_size * (OFFSET_TX_SIZE_STEP / sizeof(uint2));
    read(CONSTANT(PARAM), offset, offsets_, cost);
    return cost;
}

template <uint N> _GENX_ inline vector<uint1, N> min4(vector_ref<uint1, N> x)
{
    vector<uint1, N> x_ = cm_add<uint1>(x, 255 - 4, SAT);
    //x_.template format<uint4>() = x_.template format<uint4>() + 0x04040404; // could be this but compiler goes crazy
    x_.template format<uint4>() &= 0x07070707;
    x_.template format<uint4>() -= 0x03030303;
    return x_;
}

template <uint N> _GENX_ inline vector<uint1, N> min6(vector_ref<uint1, N> x)
{
    vector<uint1, N> x_ = cm_add<uint1>(x, 255 - 6, SAT);
    //x_.template format<uint4>() = x_.template format<uint4>() + 0x06060606; // could be this but compiler goes crazy
    x_.template format<uint4>() &= 0x07070707;
    x_.template format<uint4>() -= 0x01010101;
    return x_;
}

template <uint N> _GENX_ inline vector<uint1, N> my_min(vector_ref<uint1, N> x, uint1 v)
{
    vector<uint1, N> x_ = cm_add<uint1>(x, 0xff - v, SAT);
    const uint4 v4 = v | (v << 8) | (v << 16) | (v << 24);
    return (x_.template format<uint4>() & v4).template format<uint1>();
}

template <uint N> _GENX_ inline vector<uint1, N> my_min(vector<uint1, N> x, uint1 v)
{
    return my_min(x.template select<N, 1>(), v);
}

_GENX_ inline void compute_contexts_4(matrix_ref<int2, 4, 4> coefs, vector_ref<uint2, 16> base_contexts, vector_ref<uint2, 16> br_contexts)
{
    vector<uint1, 24> levels_x0;
    vector<uint1, 20> levels_x1;
    vector<uint1, 16> levels_x2;

    // pack to unsigned byte [0..15]: (uint1)min(abs(coef), 15)
    vector<uint2, 16> coefs_uint2 = cm_min<uint2>(cm_abs<uint2>(coefs), 15);
    levels_x0.select<16, 1>() = coefs_uint2;
    levels_x0.select<8, 1>(16) = 0;

    // shift by 1 byte
    levels_x1.format<uint4>().select<4, 1>(0) = levels_x0.format<uint4>().select<4, 1>(0) >> 8;
    levels_x1.format<uint4>().select<1, 1>(4) = 0;

    vector_ref<uint1, 16> y1 = levels_x0.select<16, 1>(4);
    vector_ref<uint1, 16> y2 = levels_x0.select<16, 1>(8);
    vector_ref<uint1, 16> x1 = levels_x1.select<16, 1>();
    vector_ref<uint1, 16> xy = levels_x1.select<16, 1>(4);
    vector_ref<uint1, 16> x2 = levels_x2.select<16, 1>();

    // compute contexts for levels' increments
    br_contexts = x1 + y1;
    br_contexts += xy;
    br_contexts = cm_avg<uint2>(br_contexts, 0);
    br_contexts = cm_min<uint2>(br_contexts, 6);
    br_contexts += g_br_ctx_offset_4;
    br_contexts <<= 4;
    br_contexts += coefs_uint2;

    // pack to [0..3]
    levels_x0.select<16, 1>() = cm_min<uint1>(levels_x0.select<16, 1>(), 3);
    levels_x1.format<uint4>().select<4, 1>(0) = levels_x0.format<uint4>().select<4, 1>(0) >> 8;
    levels_x2.format<uint4>().select<4, 1>(0) = levels_x0.format<uint4>().select<4, 1>(0) >> 16;

    // compute contexts for base levels
    base_contexts = x1 + x2;
    base_contexts += y1;
    base_contexts += xy;
    base_contexts += y2;
    base_contexts = cm_avg<uint2>(base_contexts, 0);
    base_contexts = cm_min<uint1>(base_contexts, 4);
    base_contexts += g_base_ctx_offset_4;
    base_contexts[0] = 0;
    base_contexts <<= 2;
    base_contexts += levels_x0.select<16, 1>();
}

_GENX_ inline void compute_contexts(matrix_ref<int2, 8, 8> coefs, vector_ref<uint1, 64> base_contexts, vector_ref<uint2, 64> br_contexts)
{
    vector<uint4, 80 / 4> levels_x0;
    vector<uint4, 72 / 4> levels_x1;
    vector<uint4, 64 / 4> levels_x2;

    // pack to unsigned byte [0..15]: (uint1)min(abs(coef), 15)
    vector<uint2, 64> coefs_uint2 = cm_add<uint2>(cm_abs<uint2>(coefs), 0xffff - 15, SAT);
    coefs_uint2.format<uint4>() = coefs_uint2.format<uint4>() & 0x000f000f;
    levels_x0.select<16, 1>().format<uint1>() = coefs_uint2;
    levels_x0.select<4, 1>(16) = 0;

    // shift by 1 byte
    levels_x1.select<16, 1>(0) = levels_x0.select<16, 1>(0) >> 8;
    levels_x1.select<8, 2>(0) += levels_x0.select<8, 2>(1) << 24;
    levels_x1.select<2, 1>(16) = 0;

    vector_ref<uint4, 16> y1 = levels_x0.select<16, 1>(2);
    vector_ref<uint4, 16> y2 = levels_x0.select<16, 1>(4);
    vector_ref<uint4, 16> x1 = levels_x1.select<16, 1>();
    vector_ref<uint4, 16> xy = levels_x1.select<16, 1>(2);
    vector_ref<uint4, 16> x2 = levels_x2.select<16, 1>();

    // compute contexts for levels' increments
    vector<uint4, 16> y1_plus_xy = y1 + xy;
    vector<uint4, 16> tmp = x1 + y1_plus_xy;
    tmp = (tmp + (tmp & 0x01010101)) >> 1;
    tmp.format<uint1>() = min6(tmp.format<uint1>());
    tmp.format<uint4>().select<8, 1>(0) += g_br_ctx_offset_00_31.format<uint4>();
    tmp.format<uint4>().select<8, 1>(8) += 0x0E0E0E0E;
    br_contexts = tmp.format<uint1>() << 4; // leaving byte range here
    br_contexts.format<uint4>() += coefs_uint2.format<uint4>();

    // pack to [0..3]
    levels_x0.select<16, 1>().format<uint1>() = cm_add<uint1>(levels_x0.select<16, 1>().format<uint1>(), 0xff - 3, SAT);
    levels_x0.select<16, 1>().format<uint4>() &= 0x03030303;
    levels_x1.select<16, 1>(0) = levels_x0.select<16, 1>(0) >> 8;
    levels_x1.select<8, 2>(0) += levels_x0.select<8, 2>(1) << 24;

    // shift by 2 bytes
    levels_x2.select<16, 1>(0) = levels_x0.select<16, 1>(0) >> 16;
    levels_x2.select<8, 2>(0) += levels_x0.select<8, 2>(1) << 16;

    // compute contexts for base levels
    y1_plus_xy = y1 + xy;
    tmp = x1 + x2 + y1_plus_xy + y2;
    tmp = (tmp + (tmp & 0x01010101)) >> 1;
    tmp.format<uint1>() = min4(tmp.format<uint1>());
    tmp.format<uint4>().select<8, 1>(0) += g_base_ctx_offset_00_31.format<uint4>();
    tmp.format<uint4>().select<8, 1>(8) += 0x0B0B0B0B;
    tmp.format<uint1>()(0) = 0;
    base_contexts.format<uint4>() = tmp.format<uint4>() << 2; // still within byte range
    base_contexts.format<uint4>() += levels_x0.select<16, 1>();
}

_GENX_ uint get_bits_4(SurfaceIndex PARAM)
{
    matrix_ref<int2, 4, 4> coef4 = g_coef_4.format<int2, 16, 4>().select<4, 1, 4, 1>();

    vector<int2, 16> coef4_s = coef4.format<int2>().iselect(g_scan_4x4_t);
    uint2 nz_masks = cm_pack_mask(coef4_s != 0);

    g_num_nz = cm_cbit(nz_masks);
    if (g_num_nz == 0) {
        g_last_diag = 0;
        return get_cost_txb_skip(PARAM, TX_4X4, 1); // skip
    }

    uint4 msb = 31 - cm_lzd<uint4>(nz_masks);

    uint bits = 0;
    bits += get_cost_txb_skip(PARAM, TX_4X4, 0); // non skip
    bits += 512 * g_num_nz; // signs

    float rcp_scale0 = 0.016f / g_qparam[8];
    float rcp_scale1 = 0.016f / g_qparam[9];
    //g_err_4.row(0).merge(0, coef4 == 0);
    //g_rd_cost4.format<float>()[DZ0] += g_err_4(0, 0) * rcp_scale0;

    if (msb == 0) { // dc is the only non-zero coef
        const uint2 dc = cm_abs<uint2>(coef4(0, 0));
        bits += get_cost_eob_multi(PARAM, TX_4X4, 0);
        bits += get_cost_coeff_eob_dc(PARAM, TX_4X4, cm_min<uint2>(15, dc));
        bits = (bits * 118) >> 7;
        return bits;
    }

    //g_err_4(0, 0) = 0;
    //g_rd_cost4.format<float>()[DZ1] += cm_sum<int4>(g_err_4.row(0)) * rcp_scale1;

    vector<uint2, 16> base_contexts, br_contexts;
    compute_contexts_4(coef4, base_contexts, br_contexts);

    vector<uint2, 16> base_ctx = base_contexts.iselect(g_scan_4x4_t);
    vector<uint2, 16> br_ctx = br_contexts.iselect(g_scan_4x4_t);
    vector<uint2, 16> blevel = base_ctx & 3;

    uint2 nz_mask = cm_pack_mask(blevel > 0);
    uint4 lz = cm_lzd<uint4>(uint4(nz_mask));
    vector<uint2, 1> mask_br = uint2(0xffffffff >> lz);
    vector<uint2, 1> mask_bz = mask_br >> 1;
    vector<uint2, 16> cost_br = get_cost_coeff_br(PARAM, TX_4X4, br_ctx);
    vector<uint2, 16> cost_bz = get_cost_coeff_base(PARAM, TX_4X4, base_ctx);
    cost_br.merge(cost_br, 0, mask_br[0]);
    cost_bz.merge(cost_bz, 0, mask_bz[0]);
    vector<uint2, 16> bits_acc = cost_br + cost_bz;

    bits += cm_sum<uint>(bits_acc);

    bits += get_cost_coeff_eob_ac(PARAM, TX_4X4, blevel[31 - lz] - 1); // last coef
    bits += get_cost_eob_multi(PARAM, TX_4X4, 32 - cm_lzd<uint4>(msb)); // eob
    bits = (bits * 118) >> 7;

    return bits;
}

_GENX_ inline uint get_bits_8(SurfaceIndex PARAM)
{
    vector<uint2, 4> nz_masks;
    nz_masks(0) = cm_pack_mask(g_coef_8.select<2, 1, 8, 1>(0) != 0);
    nz_masks(1) = cm_pack_mask(g_coef_8.select<2, 1, 8, 1>(2) != 0);
    nz_masks(2) = cm_pack_mask(g_coef_8.select<2, 1, 8, 1>(4) != 0);
    nz_masks(3) = cm_pack_mask(g_coef_8.select<2, 1, 8, 1>(6) != 0);

    g_num_nz = cm_sum<uint2>(cm_cbit(nz_masks.format<uint4>()));

    if (g_num_nz == 0) {
        g_last_diag = 0;
        //g_rd_cost8.format<float>().select<2, 1>(DZ0) = 0.0f;
        return get_cost_txb_skip(PARAM, TX_8X8, 1); // skip
    }

    vector_ref<uint2, 8> sequence_0_to_7 = g_sequence_0_to_31.select<8, 1>();

    vector<int4, 8> msb = 31 - cm_lzd<uint4>(cm_shl<uint>(nz_masks.format<uint1>(), sequence_0_to_7));
    g_last_diag = cm_reduced_max<int2>(msb);

    uint bits = 0;
    bits += get_cost_txb_skip(PARAM, TX_8X8, 0); // non skip

    //float rcp_scale0 = 0.016f / g_qparam[8];
    //float rcp_scale1 = 0.016f / g_qparam[9];
    //g_err_8.select<1, 1, 1, 1>().merge(0, g_coef_8(0, 0) == 0);
    //g_rd_cost8.format<float>()[DZ0] = g_err_8(0, 0) * rcp_scale0;

    if (g_last_diag == 0) { // dc is the only non-zero coef
        const uint2 dc = cm_abs<uint2>(g_coef_8(0, 0));
        //bits += get_cost_eob_multi(PARAM, TX_8X8, 0);
        bits += get_cost_coeff_eob_dc(PARAM, TX_8X8, cm_min<uint2>(15, dc));
        //g_rd_cost8.format<float>()[DZ1] = 0.0f;
        return bits;
    }

    vector<uint1, 64> base_contexts;
    vector<uint2, 64> br_contexts;
    compute_contexts(g_coef_8, base_contexts, br_contexts);

    vector<uint2, 16> bits_acc = 0;
    uint4 num_nz = g_num_nz;

    //vector<int2, 16> err_ac = 0;
    //g_err_8(0, 0) = 0;

    #pragma unroll
    for (int i = 0; i < 32; i += 16) {
        vector<uint2, 16> s = g_scan_8x8_t.select<16, 1>(i);
        vector<uint2, 16> base_ctx = base_contexts.iselect(s);
        vector<uint2, 16> br_ctx = br_contexts.iselect(s);
        vector<uint2, 16> blevel = base_ctx & 3;
        //vector<int2, 16>  err = g_err_8.format<int2>().iselect(s);

        uint2 nz_mask = cm_pack_mask(blevel > 0);
        num_nz -= cm_cbit(nz_mask);
        uint4 lz = cm_lzd<uint4>(uint4(nz_mask));
        vector<uint2, 1> mask_br = uint2(0xffffffff >> lz);
        vector<uint2, 1> mask_bz = mask_br >> 1;
        mask_br.merge(0xffff, num_nz > 0);
        mask_bz.merge(0xffff, num_nz > 0);
        vector<uint2, 16> cost_br = get_cost_coeff_br(PARAM, TX_8X8, br_ctx);
        vector<uint2, 16> cost_bz = get_cost_coeff_base(PARAM, TX_8X8, base_ctx);
        cost_br.merge(cost_br, 0, mask_br[0]);
        cost_bz.merge(cost_bz, 0, mask_bz[0]);
        bits_acc += cost_br;
        bits_acc += cost_bz;

        //err.merge(0, ~nz_mask);
        //err_ac += err;

        if (num_nz == 0) {
            bits += get_cost_coeff_eob_ac(PARAM, TX_8X8, blevel[31 - lz] - 1); // last coef
            break;
        }
    }

    bits += cm_sum<uint>(bits_acc);

    //g_rd_cost8.format<float>()[DZ1] = cm_sum<int4>(err_ac) * rcp_scale1;

    vector<uint4, 16> lzd;
    g_high_freq_sum = 31 * 4;
    // zero already processed coefs
    lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_8.select<2, 1, 8, 1>(0)), 1)); lzd.merge(31, 0x3f7f); g_high_freq_sum -= lzd;
    lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_8.select<2, 1, 8, 1>(2)), 1)); lzd.merge(31, 0x0f1f); g_high_freq_sum -= lzd;
    lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_8.select<2, 1, 8, 1>(4)), 1)); lzd.merge(31, 0x070f); g_high_freq_sum -= lzd;
    lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_8.select<2, 1, 8, 1>(6)), 1)); lzd.merge(31, 0x0103); g_high_freq_sum -= lzd;
    g_high_freq_sum.select<8, 1>() += g_high_freq_sum.select<8, 1>(8);

    return bits;
}

_GENX_ uint get_bits_16(SurfaceIndex PARAM, uint2 tx_size)
{
    assert_emu(tx_size == TX_16X16 || tx_size == TX_32X32);

    if (g_num_nz == 0) {
        //g_rd_cost16.format<float>().select<2, 1>(DZ0) = 0.0f;
        return get_cost_txb_skip(PARAM, tx_size, 1); // skip
    }

    uint bits = 0;
    bits += get_cost_txb_skip(PARAM, tx_size, 0); // non skip

    float rcp_scale0 = 0.016f / g_qparam[8];
    float rcp_scale1 = 0.016f / g_qparam[9];
    //g_err_8.select<1, 1, 1, 1>().merge(0, g_coef_16_lo(0, 0) == 0);
    //g_rd_cost16.format<float>()[DZ0] = g_err_8(0, 0) * rcp_scale0;

    if (g_last_diag == 0) { // dc is the only non-zero coef
        const uint2 dc = cm_abs<uint2>(g_coef_16_lo(0, 0));
        //bits += get_cost_eob_multi(PARAM, tx_size, 0);
        bits += get_cost_coeff_eob_dc(PARAM, tx_size, cm_min<uint2>(15, dc));
        //g_rd_cost16.format<float>()[DZ1] = 0.0f;
        return bits;
    }

    vector<uint1, 64> base_contexts;
    vector<uint2, 64> br_contexts;
    compute_contexts(g_coef_16_lo.select<8, 1, 8, 1>(), base_contexts, br_contexts);

    vector<uint1, 16> base_ctx, blevel;
    vector<uint2, 16> br_ctx;
    vector<uint2, 16> bits_acc = 0;
    vector<int2, 16> err;

    uint4 num_nz = g_num_nz;

    //vector<int2, 16> err_ac = 0;
    //g_err_8(0, 0) = 0;

    #pragma unroll
    for (int i = 0; i < 32; i += 16) {
        vector<uint2, 16> s = g_scan_8x8_t.select<16, 1>(i);
        base_ctx = base_contexts.iselect(s);
        br_ctx = br_contexts.iselect(s);
        blevel = base_ctx & 3;
        //err = g_err_8.format<int2>().iselect(s);

        uint2 nz_mask = cm_pack_mask(blevel > 0);
        num_nz -= cm_cbit(nz_mask);
        uint4 lz = cm_lzd<uint4>(uint4(nz_mask));
        vector<uint2, 1> mask_br = uint2(0xffffffff >> lz);
        vector<uint2, 1> mask_bz = mask_br >> 1;
        mask_br.merge(0xffff, num_nz > 0);
        mask_bz.merge(0xffff, num_nz > 0);
        vector<uint2, 16> cost_br = get_cost_coeff_br(PARAM, tx_size, br_ctx);
        vector<uint2, 16> cost_bz = get_cost_coeff_base(PARAM, tx_size, base_ctx);
        cost_br.merge(cost_br, 0, mask_br[0]);
        cost_bz.merge(cost_bz, 0, mask_bz[0]);
        bits_acc += cost_br;
        bits_acc += cost_bz;

        //err.merge(0, ~nz_mask);
        //err_ac += err;

        if (num_nz == 0) {
            bits += get_cost_coeff_eob_ac(PARAM, tx_size, blevel[31 - lz] - 1); // last coef
            break;
        }
    }

    bits += cm_sum<uint>(bits_acc);

    //g_rd_cost16.format<float>()[DZ1] = cm_sum<int4>(err_ac) * rcp_scale1;

    return bits;
}

_GENX_ inline void write_coefs_4x4x4(SurfaceIndex QCOEFS, vector<uint2, 2> mi, uint2 zz)
{
    vector<uint2, 2> sb = mi >> 3;
    uint2 sb_cols = (get_width() + 7) >> 3;
    uint2 sb_addr = sb[X] + sb[Y] * sb_cols;
    uint4 coef_offset = sb_addr * 4096 * sizeof(int2) + zz * 64 * sizeof(int2);
    write(QCOEFS, coef_offset + 0, g_coef_4_orig.format<int2>().select<32, 1>(0));
    write(QCOEFS, coef_offset + 64, g_coef_4_orig.format<int2>().select<32, 1>(32));
}

_GENX_ inline void write_coefs_8(SurfaceIndex QCOEFS, vector<uint2, 2> mi, uint2 zz)
{
    vector<uint2, 2> sb = mi >> 3;
    uint2 sb_cols = (get_width() + 7) >> 3;
    uint2 sb_addr = sb[X] + sb[Y] * sb_cols;
    uint4 coef_offset = sb_addr * 4096 * sizeof(int2) + zz * 64 * sizeof(int2);
    write(QCOEFS, coef_offset + 0, g_coef_8_orig.format<int2>().select<32, 1>(0));
    write(QCOEFS, coef_offset + 64, g_coef_8_orig.format<int2>().select<32, 1>(32));
}

_GENX_ inline void write_coefs_16(SurfaceIndex QCOEFS, vector<uint2, 2> mi, uint2 zz)
{
    vector<uint2, 2> sb = mi >> 3;
    uint2 sb_cols = (get_width() + 7) >> 3;
    uint2 sb_addr = sb[X] + sb[Y] * sb_cols;
    uint4 coef_offset = sb_addr * 4096 * sizeof(int2) + zz * 64 * sizeof(int2);
    for (uint2 i = 0; i < 16; i += 2, coef_offset += 64)
        write(QCOEFS, coef_offset, g_coef_16_orig.select<2, 1, 16, 1>(i).format<int2>());
}

#define BLOCK0(mi, off) (mi)
#define BLOCK1(mi, off) (mi.format<uint4>() + (off <<  0)).format<uint2>()
#define BLOCK2(mi, off) (mi.format<uint4>() + (off << 16)).format<uint2>()
#define BLOCK3(mi, off) (mi.format<uint4>() + (off << 16) + off).format<uint2>()

_GENX_ inline void decide4x4x4(SurfaceIndex PARAM, SurfaceIndex SRC, SurfaceIndex PRED, vector<uint2, 2> mi)
{
    uint4 sse;
    uint4 bits;
    uint2 vartx_info;

    g_diff_8 <<= 2;
    matrix<int2, 8, 8> diff = g_diff_8;

    g_coef_4.row(0) = g_diff_8.select<2, 4, 8, 1>(0);
    g_coef_4.row(1) = g_diff_8.select<2, 4, 8, 1>(1);
    g_coef_4.row(2) = g_diff_8.select<2, 4, 8, 1>(2);
    g_coef_4.row(3) = g_diff_8.select<2, 4, 8, 1>(3);
    dct_4x4x4();
    transpose_4x4x4();
    dct_4x4x4();
    g_coef_4_orig.row(0) = g_coef_4.select<4, 1, 4, 1>(0, 0);
    g_coef_4_orig.row(1) = g_coef_4.select<4, 1, 4, 1>(0, 4);
    g_coef_4_orig.row(2) = g_coef_4.select<4, 1, 4, 1>(0, 8);
    g_coef_4_orig.row(3) = g_coef_4.select<4, 1, 4, 1>(0, 12);

    g_qparam = get_qparam();
    sse = quant_dequant_4x4x4();
    matrix<int2, 4, 16> coef4x4x4_ = g_coef_4;
    //matrix<int2, 4, 16> err4x4x4_ = g_err_4;

    g_txb_skip_ctx = 1;
    //g_rd_cost4.format<float>().select<2, 1>(DZ0) = 0.0f;

    g_coef_4.row(0) = coef4x4x4_.select<4, 1, 4, 1>(0, 12);
    //g_err_4.row(0) = err4x4x4_.select<4, 1, 4, 1>(0, 12);
    bits = get_bits_4(PARAM);
    vartx_info = g_num_nz;
    g_coef_4.row(3) = g_coef_4.row(0);

    g_coef_4.row(0) = coef4x4x4_.select<4, 1, 4, 1>(0, 8);
    //g_err_4.row(0) = err4x4x4_.select<4, 1, 4, 1>(0, 8);
    bits += get_bits_4(PARAM);
    vartx_info += g_num_nz;
    g_coef_4.row(2) = g_coef_4.row(0);

    g_coef_4.row(0) = coef4x4x4_.select<4, 1, 4, 1>(0, 4);
    //g_err_4.row(0) = err4x4x4_.select<4, 1, 4, 1>(0, 4);
    bits += get_bits_4(PARAM);
    vartx_info += g_num_nz;
    g_coef_4.row(1) = g_coef_4.row(0);

    g_coef_4.row(0) = coef4x4x4_.select<4, 1, 4, 1>(0, 0);
    //g_err_4.row(0) = err4x4x4_.select<4, 1, 4, 1>(0, 0);
    bits += get_bits_4(PARAM);
    vartx_info += g_num_nz;

    g_rd_cost4[SSE] = sse;
    g_rd_cost4[BIT] = bits;
    g_rd_cost4[NZC] = vartx_info;

    vartx_info <<= 4;
    vartx_info += DCT_DCT + (TX_4X4 << 2);

    vector<uint2, 2> blk = mi & 7;
    g_vartx_info(blk[Y], blk[X]) = vartx_info;
}

_GENX_ void decide8(SurfaceIndex PARAM, SurfaceIndex SRC, SurfaceIndex PRED, SurfaceIndex QCOEFS,
                    vector<uint2, 2> mi, uint2 zz, uint2 depth)
{
    matrix<uint1, 8, 8> src, pred;

    vector<int4, 2> pos = mi * 8;

    read(SRC, pos(X), pos(Y), src);
    read(PRED, pos(X) + g_pred_padding, pos(Y), pred);
    g_diff_8 = src - pred;

    g_coef_8 = g_diff_8 << 2;
    dct_8x8();

    // transpose also does y = (x + 1) >> 1
    transpose_8x8();
    dct_8x8();
    g_coef_8_orig = g_coef_8;

    uint4 sse;
    uint4 bits;
    g_qparam = get_qparam();
    sse = quant_dequant_8();
    sse >>= COEF_SSE_SHIFT;

    //if (mi[Y] == 120+1 && mi[X] == 8+1) {
    //    printf("coef:\n");
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8_orig(0, 0), g_coef_8_orig(0, 1), g_coef_8_orig(0, 2), g_coef_8_orig(0, 3), g_coef_8_orig(0, 4), g_coef_8_orig(0, 5), g_coef_8_orig(0, 6), g_coef_8_orig(0, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8_orig(1, 0), g_coef_8_orig(1, 1), g_coef_8_orig(1, 2), g_coef_8_orig(1, 3), g_coef_8_orig(1, 4), g_coef_8_orig(1, 5), g_coef_8_orig(1, 6), g_coef_8_orig(1, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8_orig(2, 0), g_coef_8_orig(2, 1), g_coef_8_orig(2, 2), g_coef_8_orig(2, 3), g_coef_8_orig(2, 4), g_coef_8_orig(2, 5), g_coef_8_orig(2, 6), g_coef_8_orig(2, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8_orig(3, 0), g_coef_8_orig(3, 1), g_coef_8_orig(3, 2), g_coef_8_orig(3, 3), g_coef_8_orig(3, 4), g_coef_8_orig(3, 5), g_coef_8_orig(3, 6), g_coef_8_orig(3, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8_orig(4, 0), g_coef_8_orig(4, 1), g_coef_8_orig(4, 2), g_coef_8_orig(4, 3), g_coef_8_orig(4, 4), g_coef_8_orig(4, 5), g_coef_8_orig(4, 6), g_coef_8_orig(4, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8_orig(5, 0), g_coef_8_orig(5, 1), g_coef_8_orig(5, 2), g_coef_8_orig(5, 3), g_coef_8_orig(5, 4), g_coef_8_orig(5, 5), g_coef_8_orig(5, 6), g_coef_8_orig(5, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8_orig(6, 0), g_coef_8_orig(6, 1), g_coef_8_orig(6, 2), g_coef_8_orig(6, 3), g_coef_8_orig(6, 4), g_coef_8_orig(6, 5), g_coef_8_orig(6, 6), g_coef_8_orig(6, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8_orig(7, 0), g_coef_8_orig(7, 1), g_coef_8_orig(7, 2), g_coef_8_orig(7, 3), g_coef_8_orig(7, 4), g_coef_8_orig(7, 5), g_coef_8_orig(7, 6), g_coef_8_orig(7, 7));

    //    printf("qcoef:\n");
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8(0, 0), g_coef_8(0, 1), g_coef_8(0, 2), g_coef_8(0, 3), g_coef_8(0, 4), g_coef_8(0, 5), g_coef_8(0, 6), g_coef_8(0, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8(1, 0), g_coef_8(1, 1), g_coef_8(1, 2), g_coef_8(1, 3), g_coef_8(1, 4), g_coef_8(1, 5), g_coef_8(1, 6), g_coef_8(1, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8(2, 0), g_coef_8(2, 1), g_coef_8(2, 2), g_coef_8(2, 3), g_coef_8(2, 4), g_coef_8(2, 5), g_coef_8(2, 6), g_coef_8(2, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8(3, 0), g_coef_8(3, 1), g_coef_8(3, 2), g_coef_8(3, 3), g_coef_8(3, 4), g_coef_8(3, 5), g_coef_8(3, 6), g_coef_8(3, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8(4, 0), g_coef_8(4, 1), g_coef_8(4, 2), g_coef_8(4, 3), g_coef_8(4, 4), g_coef_8(4, 5), g_coef_8(4, 6), g_coef_8(4, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8(5, 0), g_coef_8(5, 1), g_coef_8(5, 2), g_coef_8(5, 3), g_coef_8(5, 4), g_coef_8(5, 5), g_coef_8(5, 6), g_coef_8(5, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8(6, 0), g_coef_8(6, 1), g_coef_8(6, 2), g_coef_8(6, 3), g_coef_8(6, 4), g_coef_8(6, 5), g_coef_8(6, 6), g_coef_8(6, 7));
    //    printf("%d %d %d %d %d %d %d %d\n", g_coef_8(7, 0), g_coef_8(7, 1), g_coef_8(7, 2), g_coef_8(7, 3), g_coef_8(7, 4), g_coef_8(7, 5), g_coef_8(7, 6), g_coef_8(7, 7));
    //}

    g_txb_skip_ctx = depth > 0;
    g_high_freq_sum = 0;
    bits = get_bits_8(PARAM);

    vector<int2, 1> eob, tmp;
    eob = g_last_diag * g_last_diag;
    eob += 2 * g_last_diag;
    eob >>= 1;
    tmp = 15 - g_last_diag;
    tmp *= tmp;
    tmp >>= 1;
    tmp = 63 - tmp;
    eob.merge(tmp, g_last_diag >= 8);
    //int4 eob = (g_last_diag < 8)
    //    ? ((g_last_diag * g_last_diag + 2 * g_last_diag) >> 1 )
    //    : (63 - ((15 - g_last_diag) * (15 - g_last_diag) >> 1));

    const uint2 A = g_high_freq_coef_fit_curve[TX_8X8][0];
    const uint2 B = g_high_freq_coef_fit_curve[TX_8X8][1];

    uint2 vartx_data = DCT_DCT + (TX_8X8 << 2) + (g_num_nz << 4);
    bits += 512 * g_num_nz; // signs
    bits += A * cm_sum<uint4>(g_high_freq_sum.select<8, 1>());
    bits += B * cm_add<uint2>(eob(0), -32, SAT);
    if (g_num_nz > 0) {
        bits += get_cost_eob_multi(PARAM, TX_8X8, 32 - cm_lzd<uint4>(eob(0)));
        bits = (bits * 118) >> 7;
    }

    g_rd_cost8[SSE] = sse;
    g_rd_cost8[BIT] = bits;
    g_rd_cost8[NZC] = g_num_nz;
    float cost8 = g_rd_cost8[SSE] + get_lambda() * g_rd_cost8[BIT];
    float cost4 = FLT_MAX;

    if (g_num_nz && depth < 2) {
        decide4x4x4(PARAM, SRC, PRED, mi);
        cost4 = g_rd_cost4[SSE] + get_lambda() * g_rd_cost4[BIT];
    }

    int2 split = cost4 <= cost8;
    if (split) {
        g_rd_cost8 = g_rd_cost4;
        if (g_rd_cost8[NZC] > 0)
            write_coefs_4x4x4(QCOEFS, mi, zz);
    } else {
        vector<uint2, 2> blk = mi & 7;
        g_vartx_info(blk[Y], blk[X]) = vartx_data;
        if (g_rd_cost8[NZC] > 0)
            write_coefs_8(QCOEFS, mi, zz);
    }
}

_GENX_ void decide16(SurfaceIndex PARAM, SurfaceIndex SRC, SurfaceIndex PRED, SurfaceIndex QCOEFS, vector<uint2, 2> mi, uint2 zz, uint2 depth)
{
    matrix<uint1, 16, 16> src, pred;

    vector<int4, 2> pos = mi * 8;

    read(SRC, pos(X), pos(Y), src);
    read(PRED, pos(X) + g_pred_padding, pos(Y), pred);
    g_coef_16_lo = src - pred;

    g_coef_16_lo <<= 2;
    dct_16x16();

    // y = (x - 2 + 1) >> 1
    // then transpose does z = (y + 1) >> 1
    // which results in (x + 1) >> 2
    g_coef_16_lo = cm_avg<int2>(g_coef_16_lo, -2);
    transpose_16x16();

    dct_16x16();
    g_coef_16_orig = g_coef_16_lo;

    uint4 sse;
    uint4 bits;
    g_qparam = get_qparam();
    sse = quant_dequant_16(0);
    sse >>= COEF_SSE_SHIFT;

    vector<uint2, 16> nz_masks;
    #pragma unroll
    for (int i = 0; i < 16; i++)
        nz_masks[i] = cm_pack_mask(g_coef_16_lo.row(i) != 0);
    g_num_nz = cm_sum<uint2>(cm_cbit(nz_masks.format<uint4>()));

    vector<int4, 16> msb = 31 - cm_lzd<uint4>(vector<uint4, 16>(nz_masks));
    msb.merge(msb + g_sequence_0_to_31.select<16, 1>(), msb >= 0);
    g_last_diag = cm_reduced_max<int2>(msb);

    g_txb_skip_ctx = depth > 0;
    bits = get_bits_16(PARAM, TX_16X16);

    g_high_freq_sum = 0;
    if (g_num_nz) {
        // zero already processed coefs
        vector<uint4, 16> lzd;
        lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(0)), 1)); lzd.merge(31, 0x007f); g_high_freq_sum -= lzd;
        lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(1)), 1)); lzd.merge(31, 0x003f); g_high_freq_sum -= lzd;
        lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(2)), 1)); lzd.merge(31, 0x001f); g_high_freq_sum -= lzd;
        lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(3)), 1)); lzd.merge(31, 0x000f); g_high_freq_sum -= lzd;
        lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(4)), 1)); lzd.merge(31, 0x000f); g_high_freq_sum -= lzd;
        lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(5)), 1)); lzd.merge(31, 0x0007); g_high_freq_sum -= lzd;
        lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(6)), 1)); lzd.merge(31, 0x0003); g_high_freq_sum -= lzd;
        lzd = cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(7)), 1)); lzd.merge(31, 0x0001); g_high_freq_sum -= lzd;
        #pragma unroll
        for (int i = 8; i < 16; i++)
            g_high_freq_sum -= cm_lzd<uint4>(cm_abs<uint4>(g_coef_16_lo.row(i)) + 1);
        g_high_freq_sum += 31 * 16;
    }

    vector<int2, 1> eob, tmp;
    eob = g_last_diag * g_last_diag;
    eob += 2 * g_last_diag;
    eob >>= 1;
    tmp = 31 - g_last_diag;
    tmp *= tmp;
    tmp >>= 1;
    tmp = 255 - tmp;
    eob.merge(tmp, g_last_diag >= 16);
    //int4 eob = (g_last_diag < 16)
    //    ? ((g_last_diag * g_last_diag + 2 * g_last_diag) >> 1)
    //    : (255 - ((31 - g_last_diag) * (31 - g_last_diag) >> 1));

    const uint2 A = g_high_freq_coef_fit_curve[TX_16X16][0];
    const uint2 B = g_high_freq_coef_fit_curve[TX_16X16][1];

    uint2 vartx_data = DCT_DCT + (TX_16X16 << 2) + (g_num_nz << 4);
    bits += 512 * g_num_nz; // signs
    bits += A * cm_sum<uint4>(g_high_freq_sum);
    bits += B * cm_add<uint2>(eob(0), -32, SAT);
    if (g_num_nz > 0) {
        bits += get_cost_eob_multi(PARAM, TX_16X16, 32 - cm_lzd<uint4>(eob(0)));
        bits = (bits * 118) >> 7;
    }

    g_rd_cost16[SSE] = sse;
    g_rd_cost16[BIT] = bits;
    g_rd_cost16[NZC] = g_num_nz;
    float cost16 = g_rd_cost16[SSE] + get_lambda() * g_rd_cost16[BIT];
    float cost8 = FLT_MAX;

    if (g_num_nz && depth < 2) {
        rd_cost rd_cost8;
        decide8(PARAM, SRC, PRED, QCOEFS, BLOCK0(mi, 1), zz + 0, depth + 1);
        rd_cost8  = g_rd_cost8;

        decide8(PARAM, SRC, PRED, QCOEFS, BLOCK1(mi, 1), zz + 1, depth + 1);
        rd_cost8.select<4, 1>(SSE) += g_rd_cost8.select<4, 1>(SSE);
        //rd_cost8.select<2, 1>(DZ0).format<float>() += g_rd_cost8.select<2, 1>(DZ0).format<float>();

        decide8(PARAM, SRC, PRED, QCOEFS, BLOCK2(mi, 1), zz + 2, depth + 1);
        rd_cost8.select<4, 1>(SSE) += g_rd_cost8.select<4, 1>(SSE);
        //rd_cost8.select<2, 1>(DZ0).format<float>() += g_rd_cost8.select<2, 1>(DZ0).format<float>();

        decide8(PARAM, SRC, PRED, QCOEFS, BLOCK3(mi, 1), zz + 3, depth + 1);
        rd_cost8.select<4, 1>(SSE) += g_rd_cost8.select<4, 1>(SSE);
        //rd_cost8.select<2, 1>(DZ0).format<float>() += g_rd_cost8.select<2, 1>(DZ0).format<float>();

        g_rd_cost8 = rd_cost8;
        cost8 = g_rd_cost8[SSE] + get_lambda() * g_rd_cost8[BIT];
    }

    int2 split = cost8 <= cost16;
    if (split)
        g_rd_cost16 = g_rd_cost8;
    else {
        vector<uint2, 2> blk = mi & 7;
        g_vartx_info.select<2, 1, 2, 1>(blk[Y], blk[X]) = vartx_data;
        if (g_rd_cost16[NZC] > 0)
            write_coefs_16(QCOEFS, mi, zz);
    }
}

_GENX_ void decide32(SurfaceIndex PARAM, SurfaceIndex SRC, SurfaceIndex PRED, SurfaceIndex QCOEFS,
                     SurfaceIndex SCRATCHBUF, vector<uint2, 2> mi, uint2 zz, uint2 depth)
{
    matrix<uint1, 16, 16> src, pred;

    vector<int4, 2> pos = mi * 8;
    vector<int4, 2> scratchbuf_pos = pos & ~63;

    read(SRC,  pos(X), pos(Y), src);
    read(PRED, pos(X) + g_pred_padding, pos(Y), pred);
    g_coef_16_lo = src - pred;

    read(SRC,  pos(X), pos(Y) + 16, src);
    read(PRED, pos(X) + g_pred_padding, pos(Y) + 16, pred);
    g_coef_16_hi = src - pred;

    g_coef_16_lo <<= 2;
    g_coef_16_hi <<= 2;
    dct_32x16();

    // y = x >> 3
    // then transpose does z = (y + 1) >> 1
    // which results in (x + 8) >> 4
    g_coef_16_lo >>= 3;
    g_coef_16_hi >>= 3;
    transpose_32x16();

    write(SCRATCHBUF, scratchbuf_pos(X) +  0, scratchbuf_pos(Y) + 0, g_coef_16_lo.select<8, 1, 16, 1>(0, 0));
    write(SCRATCHBUF, scratchbuf_pos(X) +  0, scratchbuf_pos(Y) + 8, g_coef_16_lo.select<8, 1, 16, 1>(8, 0));
    write(SCRATCHBUF, scratchbuf_pos(X) + 32, scratchbuf_pos(Y) + 0, g_coef_16_hi.select<8, 1, 16, 1>(0, 0));
    write(SCRATCHBUF, scratchbuf_pos(X) + 32, scratchbuf_pos(Y) + 8, g_coef_16_hi.select<8, 1, 16, 1>(8, 0));

    read(SRC, pos(X) + 16, pos(Y), src);
    read(PRED, pos(X) + 16 + g_pred_padding, pos(Y), pred);
    g_coef_16_lo = src - pred;

    read(SRC, pos(X) + 16, pos(Y) + 16, src);
    read(PRED, pos(X) + 16 + g_pred_padding, pos(Y) + 16, pred);
    g_coef_16_hi = src - pred;

    g_coef_16_lo <<= 2;
    g_coef_16_hi <<= 2;
    dct_32x16();

    // y = x >> 3
    // then transpose does z = (y + 1) >> 1
    // which results in (x + 8) >> 4
    g_coef_16_lo >>= 3;
    g_coef_16_hi >>= 3;
    transpose_32x16();

    write(SCRATCHBUF, scratchbuf_pos(X) +  0, scratchbuf_pos(Y) + 16, g_coef_16_lo.select<8, 1, 16, 1>(0, 0));
    write(SCRATCHBUF, scratchbuf_pos(X) +  0, scratchbuf_pos(Y) + 24, g_coef_16_lo.select<8, 1, 16, 1>(8, 0));

    read(MODIFIED(SCRATCHBUF), scratchbuf_pos(X) + 32, scratchbuf_pos(Y) + 0, g_coef_16_lo.select<8, 1, 16, 1>(0, 0));
    read(MODIFIED(SCRATCHBUF), scratchbuf_pos(X) + 32, scratchbuf_pos(Y) + 8, g_coef_16_lo.select<8, 1, 16, 1>(8, 0));

    dct_32x16();
    //transpose_32x16();

    // quadrants 2 and 3
    write(SCRATCHBUF, scratchbuf_pos(X) + 32, scratchbuf_pos(Y) + 0, g_coef_16_lo.select<8, 1, 16, 1>(0, 0));
    write(SCRATCHBUF, scratchbuf_pos(X) + 32, scratchbuf_pos(Y) + 8, g_coef_16_lo.select<8, 1, 16, 1>(8, 0));
    write(SCRATCHBUF, scratchbuf_pos(X) + 32, scratchbuf_pos(Y) + 16, g_coef_16_hi.select<8, 1, 16, 1>(0, 0));
    write(SCRATCHBUF, scratchbuf_pos(X) + 32, scratchbuf_pos(Y) + 24, g_coef_16_hi.select<8, 1, 16, 1>(8, 0));

    // quant
    qparam_t qparam_0 = get_qparam();
    // scale quant params for 32x32
    qparam_0.select<4, 1>(0) = cm_avg<int2>(qparam_0.select<4, 1>(0), 0);
    qparam_0.select<2, 1>(6) <<= 1;
    g_qparam = qparam_0;
    g_qparam.select<5, 2>(0) = g_qparam.select<5, 2>(1);
    uint4 sse;
    sse  = quant_dequant_16(1);
    sse += quant_dequant_16_2();

    vector<uint2, 32> nz_masks;
    #pragma unroll
    for (int i = 0; i < 16; i++) {
        nz_masks[i +  0] = cm_pack_mask(g_coef_16_lo.row(i) != 0);
        nz_masks[i + 16] = cm_pack_mask(g_coef_16_hi.row(i) != 0);
    }
    vector<uint4, 16> num_nz = cm_cbit(nz_masks.format<uint4>());
    uint2 num_nz0 = cm_sum<uint2>(num_nz.select<8, 1>(0));
    uint2 num_nz1 = cm_sum<uint2>(num_nz.select<8, 1>(8));
    g_num_nz = num_nz0 + num_nz1;

    vector<int4, 32> msb;
    msb = 31 - cm_lzd<uint4>(vector<uint4, 32>(nz_masks));
    msb.merge(msb + g_sequence_0_to_31 + 16, msb >= 0);
    g_last_diag = cm_reduced_max<int2>(msb);

    g_high_freq_sum = 0;
    if (num_nz0) {
        #pragma unroll
        for (int i = 0; i < 16; i++)
            g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(i)), 1));
        g_high_freq_sum += 31 * 16;
    }
    if (num_nz1) {
        #pragma unroll
        for (int i = 0; i < 16; i++)
            g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_hi.row(i)), 1));
        g_high_freq_sum += 31 * 16;
    }


    read(MODIFIED(SCRATCHBUF), scratchbuf_pos(X), scratchbuf_pos(Y) +  0, g_coef_16_lo.select<8, 1, 16, 1>(0, 0));
    read(MODIFIED(SCRATCHBUF), scratchbuf_pos(X), scratchbuf_pos(Y) +  8, g_coef_16_lo.select<8, 1, 16, 1>(8, 0));
    read(MODIFIED(SCRATCHBUF), scratchbuf_pos(X), scratchbuf_pos(Y) + 16, g_coef_16_hi.select<8, 1, 16, 1>(0, 0));
    read(MODIFIED(SCRATCHBUF), scratchbuf_pos(X), scratchbuf_pos(Y) + 24, g_coef_16_hi.select<8, 1, 16, 1>(8, 0));

    dct_32x16();
    //transpose_32x16();

    // quadrants 0 and 1
    write(SCRATCHBUF, scratchbuf_pos(X) + 0, scratchbuf_pos(Y) + 0, g_coef_16_lo.select<8, 1, 16, 1>(0, 0));
    write(SCRATCHBUF, scratchbuf_pos(X) + 0, scratchbuf_pos(Y) + 8, g_coef_16_lo.select<8, 1, 16, 1>(8, 0));
    write(SCRATCHBUF, scratchbuf_pos(X) + 0, scratchbuf_pos(Y) + 16, g_coef_16_hi.select<8, 1, 16, 1>(0, 0));
    write(SCRATCHBUF, scratchbuf_pos(X) + 0, scratchbuf_pos(Y) + 24, g_coef_16_hi.select<8, 1, 16, 1>(8, 0));

    // quant
    sse += quant_dequant_16_2();
    g_qparam = qparam_0;
    sse += quant_dequant_16(1);
    sse >>= COEF_SSE_SHIFT_32;

    #pragma unroll
    for (int i = 0; i < 16; i++) {
        nz_masks[i +  0] = cm_pack_mask(g_coef_16_lo.row(i) != 0);
        nz_masks[i + 16] = cm_pack_mask(g_coef_16_hi.row(i) != 0);
    }
    num_nz = cm_cbit(nz_masks.format<uint4>());
    num_nz0 = cm_sum<uint2>(num_nz.select<8, 1>(0));
    num_nz1 = cm_sum<uint2>(num_nz.select<8, 1>(8));
    g_num_nz += num_nz0;
    g_num_nz += num_nz1;

    msb = 31 - cm_lzd<uint4>(vector<uint4, 32>(nz_masks));
    msb.merge(msb + g_sequence_0_to_31, msb >= 0);
    g_last_diag = cm_max<int2>(g_last_diag, cm_reduced_max<int2>(msb));

    g_txb_skip_ctx = depth > 0;
    uint4 bits = get_bits_16(PARAM, TX_32X32);
    //g_rd_cost32.format<float>().select<2, 1>(DZ0) = g_rd_cost16.format<float>().select<2, 1>(DZ0);

    if (num_nz0) {
        g_coef_16_lo.row(0).merge(0, 0x007f);
        g_coef_16_lo.row(1).merge(0, 0x003f);
        g_coef_16_lo.row(2).merge(0, 0x001f);
        g_coef_16_lo.row(3).select<4, 1>() = 0;
        g_coef_16_lo.row(4).select<4, 1>() = 0;
        g_coef_16_lo.row(5).merge(0, 0x0007);
        g_coef_16_lo.row(6).select<2, 1>() = 0;
        g_coef_16_lo.row(7).select<1, 1>() = 0;
        g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(0)), 1));
        g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(1)), 1));
        g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(2)), 1));
        g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(3)), 1));
        g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(4)), 1));
        g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(5)), 1));
        g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(6)), 1));
        g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(7)), 1));

        #pragma unroll
        for (int i = 8; i < 16; i++)
            g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_lo.row(i)), 1));

        g_high_freq_sum += 31 * 16;
    }

    if (num_nz1) {
        #pragma unroll
        for (int i = 0; i < 16; i++)
            g_high_freq_sum -= cm_lzd<uint4>(cm_add<uint4>(cm_abs<uint2>(g_coef_16_hi.row(i)), 1));

        g_high_freq_sum += 31 * 16;
    }

    vector<int2, 1> eob, tmp;
    eob = g_last_diag * g_last_diag;
    eob += 2 * g_last_diag;
    eob >>= 1;
    tmp = 63 - g_last_diag;
    tmp *= tmp;
    tmp >>= 1;
    tmp = 1023 - tmp;
    eob.merge(tmp, g_last_diag >= 32);
    //int2 eob = (g_last_diag < 32)
    //    ? ((g_last_diag * g_last_diag + 2 * g_last_diag) >> 1)
    //    : (1023 - ((63 - g_last_diag) * (63 - g_last_diag) >> 1));

    const uint2 A = g_high_freq_coef_fit_curve[TX_32X32][0];
    const uint2 B = g_high_freq_coef_fit_curve[TX_32X32][1];

    uint2 vartx_data = DCT_DCT + (TX_32X32 << 2) + (g_num_nz << 4);
    bits += 512 * g_num_nz; // signs
    bits += A * cm_sum<uint4>(g_high_freq_sum);
    bits += B * cm_add<uint2>(eob(0), -32, SAT);
    if (g_num_nz > 0) {
        bits += get_cost_eob_multi(PARAM, TX_32X32, 32 - cm_lzd<uint4>(eob(0)));
        bits = (bits * 118) >> 7;
    }

    g_rd_cost32[SSE] = sse;
    g_rd_cost32[BIT] = bits;
    g_rd_cost32[NZC] = g_num_nz;
    float cost32 = g_rd_cost32[SSE] + get_lambda() * g_rd_cost32[BIT];
    float cost16 = FLT_MAX;

    if (g_num_nz) {
        rd_cost rd_cost16;
        decide16(PARAM, SRC, PRED, QCOEFS, BLOCK0(mi, 2), zz + 0, depth + 1);
        rd_cost16  = g_rd_cost16;
        decide16(PARAM, SRC, PRED, QCOEFS, BLOCK1(mi, 2), zz + 4, depth + 1);
        rd_cost16.select<4, 1>(SSE) += g_rd_cost16.select<4, 1>(SSE);
        //rd_cost16.select<2, 1>(DZ0).format<float>() += g_rd_cost16.select<2, 1>(DZ0).format<float>();
        decide16(PARAM, SRC, PRED, QCOEFS, BLOCK2(mi, 2), zz + 8, depth + 1);
        rd_cost16.select<4, 1>(SSE) += g_rd_cost16.select<4, 1>(SSE);
        //rd_cost16.select<2, 1>(DZ0).format<float>() += g_rd_cost16.select<2, 1>(DZ0).format<float>();
        decide16(PARAM, SRC, PRED, QCOEFS, BLOCK3(mi, 2), zz + 12, depth + 1);
        rd_cost16.select<4, 1>(SSE) += g_rd_cost16.select<4, 1>(SSE);
        //rd_cost16.select<2, 1>(DZ0).format<float>() += g_rd_cost16.select<2, 1>(DZ0).format<float>();

        g_rd_cost16 = rd_cost16;
        cost16 = g_rd_cost16[SSE] + get_lambda() * g_rd_cost16[BIT];
    }

    if (cost16 <= cost32)
        g_rd_cost32 = g_rd_cost16;
    else {
        vector<uint2, 2> blk = mi & 7;
        g_vartx_info.select<4, 1, 4, 1>(blk[Y], blk[X]) = vartx_data;

        if (g_rd_cost32[NZC] > 0) {
            vector<uint2, 2> sb = mi >> 3;
            uint2 sb_cols = (get_width() + 7) >> 3;
            uint2 sb_addr = sb[X] + sb[Y] * sb_cols;
            uint4 coef_offset = sb_addr * 4096 * sizeof(int2) + zz * 64 * sizeof(int2);

            matrix<int2, 8, 16> m0, m1;
            for (uint2 i = 0; i < 32; i += 8) {
                read(MODIFIED(SCRATCHBUF), scratchbuf_pos(X) + 0, scratchbuf_pos(Y) + i, m0);
                read(MODIFIED(SCRATCHBUF), scratchbuf_pos(X) + 32, scratchbuf_pos(Y) + i, m1);
#pragma unroll
                for (uint2 j = 0; j < 8; j++, coef_offset += 64) {
                    write(QCOEFS, coef_offset + 0, m0.row(j));
                    write(QCOEFS, coef_offset + 32, m1.row(j));
                }
            }
        }
    }
}

_GENX_ inline vector<float, 2> add_weighted(vector<float, 2> curr_adz, vector<float, 2> prev_adz, vector<float, 2> prev_adz_delta)
{
    vector<float, 8> weights;
    weights[0] = 1.000000000f;
    weights[1] = 0.984496437f;
    weights[2] = 0.939413063f;
    weights[3] = 0.868815056f;
    weights[4] = 0.778800783f;
    weights[5] = 0.676633846f;
    weights[6] = 0.569782825f;
    weights[7] = 0.465043188f;

    vector<float, 2> diff = curr_adz - prev_adz;
    vector<uint2, 2> idx = diff * diff * 8 + 0.5;
    idx = cm_min<uint2>(idx, 7);
    curr_adz += prev_adz_delta * weights.iselect(idx);
    return curr_adz;
}

_GENX_ inline void setup_adaptive_dead_zone(SurfaceIndex PARAM, SurfaceIndex PREV_ADZ, SurfaceIndex PREV_ADZ_DELTA, vector<uint2, 2> sb)
{
    const uint2 scene_cut = 0;

    const uint2 sb_cols = (get_width() + 7) >> 3;
    const uint2 sb_rows = (get_height() + 7) >> 3;
    const uint2 sb_addr = sb[X] + sb[Y] * sb_cols;

    vector<uint2, 2> initDz_;
    read(DWALIGNED(PARAM), OFFSET_INIT_DZ, initDz_);
    vector<float, 2> initDz = initDz_;

    float varDz;

    read(DWALIGNED(PREV_ADZ), sb_addr * SIZEOF_ADZ_CTX, g_curr_adz);

    //if (get_comp_flag() && !get_bidir_flag()) {
    //    vector<float, 2> prev_adz, prev_adz_delta;

    //    varDz = scene_cut ? 10.0f : 8.0f;

    //    if (sb[X] > 0) {
    //        read(DWALIGNED(PREV_ADZ), sb_addr * SIZEOF_ADZ_CTX - SIZEOF_ADZ_CTX, prev_adz);
    //        read(DWALIGNED(PREV_ADZ_DELTA), sb_addr * SIZEOF_ADZ_CTX - SIZEOF_ADZ_CTX, prev_adz_delta);
    //        g_curr_adz = add_weighted(g_curr_adz, prev_adz, prev_adz_delta);
    //    }
    //    if (sb[Y] > 0) {
    //        read(DWALIGNED(PREV_ADZ), (sb_addr - sb_cols) * SIZEOF_ADZ_CTX, prev_adz);
    //        read(DWALIGNED(PREV_ADZ_DELTA), (sb_addr - sb_cols) * SIZEOF_ADZ_CTX, prev_adz_delta);
    //        g_curr_adz = add_weighted(g_curr_adz, prev_adz, prev_adz_delta);
    //    }
    //    if (sb[X] + 1 < sb_cols && sb[Y] > 0) {
    //        read(DWALIGNED(PREV_ADZ), (sb_addr - sb_cols + 1) * SIZEOF_ADZ_CTX, prev_adz);
    //        read(DWALIGNED(PREV_ADZ_DELTA), (sb_addr - sb_cols + 1) * SIZEOF_ADZ_CTX, prev_adz_delta);
    //        g_curr_adz = add_weighted(g_curr_adz, prev_adz, prev_adz_delta);
    //    }
    //    if (sb[X] + 1 < sb_cols && sb[Y] + 1 < sb_rows) {
    //        read(DWALIGNED(PREV_ADZ), (sb_addr + sb_cols + 1) * SIZEOF_ADZ_CTX, prev_adz);
    //        read(DWALIGNED(PREV_ADZ_DELTA), (sb_addr + sb_cols + 1) * SIZEOF_ADZ_CTX, prev_adz_delta);
    //        g_curr_adz = add_weighted(g_curr_adz, prev_adz, prev_adz_delta);
    //    }
    //} else {
    //    varDz = 4.0f;
    //    // Use min
    //    g_curr_adz = cm_min<float>(g_curr_adz, initDz);
    //}

    //// Saturate
    //vector<float, 2> minDz = initDz - varDz;
    //vector<float, 2> maxDz = initDz + varDz;
    //g_curr_adz = cm_min<float>(g_curr_adz, maxDz);
    //g_curr_adz = cm_max<float>(g_curr_adz, minDz);

    // set rounding params for quantization
    vector<uint4, 2> round;
    round = g_curr_adz + 0.5f;
    round *= get_qparam().select<2, 1>(8);
    get_qparam_ref().select<2, 1>(2) = round >> 7;
    //if (sb[Y] == 120/8 && sb[X] == 8/8)
    //    printf("sb = %d %d, g_curr_adz = %.3f %.3f, new round = %d %d\n", sb[Y], sb[X], g_curr_adz[0], g_curr_adz[1], get_qparam()[2], get_qparam()[3]);

    // set initial rounding params for accumulation
    //g_round_adj = g_curr_adz * get_qparam().select<2, 1>(8) * (1.0f / 128.0f);
}

_GENX_ inline void update_adaptive_dead_zone(SurfaceIndex CURR_ADZ, SurfaceIndex CURR_ADZ_DELTA, vector<uint2, 2> sb)
{
    vector<float, 2> init_adz = g_curr_adz;
    vector<float, 2> curr_adz_delta;

    g_curr_adz = g_round_adj * 128.0f / get_qparam().select<2, 1>(8);
    curr_adz_delta = g_curr_adz - init_adz;

    const uint2 sb_cols = (get_width() + 7) >> 3;
    const uint2 sb_rows = (get_height() + 7) >> 3;
    const uint2 sb_addr = sb[X] + sb[Y] * sb_cols;

    vector<float, 8> tmp = 0;
    tmp.select<2, 1>() = g_curr_adz; // tmp work-around for dataport minimum block size, until adz and adz_delta are combined
    write(CURR_ADZ, sb_addr * SIZEOF_ADZ_CTX, tmp);

    tmp.select<2, 1>() = curr_adz_delta;
    write(CURR_ADZ_DELTA, sb_addr * SIZEOF_ADZ_CTX, tmp);
}

extern "C" _GENX_MAIN_
    void VarTxDecision(SurfaceIndex PARAM, SurfaceIndex SRC, SurfaceIndex PRED, SurfaceIndex MODE_INFO,
                       SurfaceIndex SCRATCHBUF, SurfaceIndex QCOEFS, SurfaceIndex VAR_TX_INFO,
                       SurfaceIndex PREV_ADZ, SurfaceIndex PREV_ADZ_DELTA, SurfaceIndex CURR_ADZ,
                       SurfaceIndex CURR_ADZ_DELTA, int pred_padding)
{
    g_pred_padding = pred_padding;

    read(PARAM, 0, g_params);
    read(PARAM, OFFSET_FIT_CURVE, g_high_freq_coef_fit_curve.format<uint2>());

    vector<uint2, 2> sb;
    sb(X) = get_thread_origin_x();
    sb(Y) = get_thread_origin_y();

    uint2 sb_cols = (get_width() + 7) >> 3;
    uint2 sb_addr = sb[X] + sb[Y] * sb_cols;

    //setup_adaptive_dead_zone(PARAM, PREV_ADZ, PREV_ADZ_DELTA, sb);
    setup_adaptive_dead_zone(PARAM, CURR_ADZ, CURR_ADZ_DELTA, sb);

    vector<uint2, 2> omi = sb * 8;

    g_scan_8x8_t = vector<uint1, 32>(SCAN_8X8_TRANSPOSED);
    g_scan_4x4_t = vector<uint1, 16>(SCAN_4X4_TRANSPOSED);

    g_br_ctx_offset_00_31.format<uint4>() = 0x0E0E0E0E;
    g_br_ctx_offset_00_31.format<uint2>()[0] = 0x0700;
    g_br_ctx_offset_00_31.format<uint2>()[4] = 0x0707;

    g_base_ctx_offset_00_31.format<uint4>() = 0x0B0B0B0B;
    g_base_ctx_offset_00_31.format<uint4>()[0] = 0x06060100;
    g_base_ctx_offset_00_31.format<uint4>()[2] = 0x0B060601;
    g_base_ctx_offset_00_31.format<uint4>()[4] = 0x0B0B0606;
    g_base_ctx_offset_00_31.format<uint4>()[6] = 0x0B0B0B06;

    g_br_ctx_offset_4.format<uint4>() = g_br_ctx_offset_00_31.format<uint4>().select<4, 2>(0);
    g_base_ctx_offset_4.format<uint4>() = g_base_ctx_offset_00_31.format<uint4>().select<4, 2>(0);

    cmtl::cm_vector_assign<uint2, 8>(g_sequence_0_to_31.select<8, 1>(), 0, 1);
    g_sequence_0_to_31.select<8, 1>(8) = g_sequence_0_to_31.select<8, 1>(0) + 8;
    g_sequence_0_to_31.select<16, 1>(16) = g_sequence_0_to_31.select<16, 1>(0) + 16;

    g_zeroes = 0;
    g_vartx_info.format<uint4>() = 0;

    vector<uint1, 64> scan_z2r/*(ZIGZAG_SCAN)*/;
    scan_z2r.format<uint4>().select<1, 1>(0) = 0x09080100;
    scan_z2r.format<uint4>().select<1, 1>(1) = scan_z2r.format<uint4>().select<1, 1>(0) + 0x02020202;
    scan_z2r.format<uint4>().select<2, 1>(2) = scan_z2r.format<uint4>().select<2, 1>(0) + 0x10101010;
    scan_z2r.format<uint4>().select<4, 1>(4) = scan_z2r.format<uint4>().select<4, 1>(0) + 0x04040404;
    scan_z2r.format<uint4>().select<8, 1>(8) = scan_z2r.format<uint4>().select<8, 1>(0) + 0x20202020;

    uint num8x8;
    for (uint2 i = 0; i < 64; i += num8x8) {
        uint1 raster = scan_z2r[i];

        vector<uint2, 2> blk;
        blk(X) = uint2(raster & 7);
        blk(Y) = uint2(raster >> 3);
        vector<uint2, 2> mi = cm_add<uint2>(omi, blk);

        mode_info mode_info;
        read(MODE_INFO, mi(X) * SIZEOF_MODE_INFO, mi(Y), mode_info);

        const uint2 log2_num8x8 = get_sb_type(mode_info) >> 2;
        num8x8 = 1 << (log2_num8x8 << 1);

        if (get_mode(mode_info) == OUT_OF_PIC)
            continue;

        if (get_skip(mode_info) == 0) {
            rd_cost rd;
            if (log2_num8x8 == 0) {
                decide8(PARAM, SRC, PRED, QCOEFS, mi, i, 0); rd = g_rd_cost8;
            } else if (log2_num8x8 == 1) {
                decide16(PARAM, SRC, PRED, QCOEFS, mi, i, 0); rd = g_rd_cost16;
            } else if (log2_num8x8 == 2) {
                decide32(PARAM, SRC, PRED, QCOEFS, SCRATCHBUF, mi, i, 0); rd = g_rd_cost32;
            } else if (log2_num8x8 == 3) {
                decide32(PARAM, SRC, PRED, QCOEFS, SCRATCHBUF, BLOCK0(mi, 4), i + 0, 1);
                rd = g_rd_cost32;

                decide32(PARAM, SRC, PRED, QCOEFS, SCRATCHBUF, BLOCK1(mi, 4), i + 16, 1);
                rd.select<4, 1>(SSE) += g_rd_cost32.select<4, 1>(SSE);
                //rd.select<2, 1>(DZ0).format<float>() += g_rd_cost32.select<2, 1>(DZ0).format<float>();

                decide32(PARAM, SRC, PRED, QCOEFS, SCRATCHBUF, BLOCK2(mi, 4), i + 32, 1);
                rd.select<4, 1>(SSE) += g_rd_cost32.select<4, 1>(SSE);
                //rd.select<2, 1>(DZ0).format<float>() += g_rd_cost32.select<2, 1>(DZ0).format<float>();

                decide32(PARAM, SRC, PRED, QCOEFS, SCRATCHBUF, BLOCK3(mi, 4), i + 48, 1);
                rd.select<4, 1>(SSE) += g_rd_cost32.select<4, 1>(SSE);
                //rd.select<2, 1>(DZ0).format<float>() += g_rd_cost32.select<2, 1>(DZ0).format<float>();
            }

            set_skip1(mode_info);
            if (rd[NZC] > 0) {
                vector<int4, 2> pos = mi * 8;
                vector<int2, 16> diff;
                if (log2_num8x8 == 0) {
                    matrix<uint1, 8, 8> src, pred;
                    read(SRC, pos(X), pos(Y), src);
                    read(PRED, pos(X) + g_pred_padding, pos(Y), pred);
                    sse_8(src, pred);
                } else {
                    g_sse = 0;
                    const uint2 size = 8 << log2_num8x8;
                    for (uint2 y = 0; y < size; y += 16) {
                        for (uint2 x = 0; x < size; x += 16) {
                            matrix<uint1, 16, 16> src, pred;
                            read(SRC, pos(X) + x, pos(Y) + y, src);
                            read(PRED, pos(X) + x + g_pred_padding, pos(Y) + y, pred);
                            sse_16_acc(src, pred);
                        }
                    }
                }

                float best_cost_adjusted = rd[SSE] + get_lambda() * ((rd[BIT] * 3) >> 2);
                float cost_skip = cm_sum<uint4>(g_sse);
                if (cost_skip > best_cost_adjusted) {
                    set_skip0(mode_info);
                    //g_round_adj += rd.format<float>().select<2, 1>(DZ0);
                }
            }
        }

        if (get_skip(mode_info)) {
            if (log2_num8x8 == 0)
                g_vartx_info(blk[Y], blk[X]) = DCT_DCT + (TX_8X8 << 2);
            else if (log2_num8x8 == 1)
                g_vartx_info.select<2, 1, 2, 1>(blk[Y], blk[X]) = DCT_DCT + (TX_16X16 << 2);
            else if (log2_num8x8 == 2)
                g_vartx_info.select<4, 1, 4, 1>(blk[Y], blk[X]) = DCT_DCT + (TX_32X32 << 2);
            else
                g_vartx_info = DCT_DCT + (TX_32X32 << 2);
        }

        write(MODE_INFO, mi(X) * SIZEOF_MODE_INFO, mi(Y), mode_info);
    }

    //update_adaptive_dead_zone(CURR_ADZ, CURR_ADZ_DELTA, sb);

    uint4 vartx_info_global_offset = sb_addr * 16 * 16 * sizeof(uint2);
    for (int i = 0; i < 8; i++, vartx_info_global_offset += 64) {
        vector<uint2, 16> row = g_vartx_info.row(i).replicate<8, 1, 2, 0>();
        write(VAR_TX_INFO, vartx_info_global_offset, row.replicate<2>());
    }
}
