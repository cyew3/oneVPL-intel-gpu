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

#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>

static const uint2 SIZEOF_MODE_INFO = 32;
typedef matrix    <uint1,1,SIZEOF_MODE_INFO> mode_info;
typedef matrix_ref<uint1,1,SIZEOF_MODE_INFO> mode_info_ref;

static const uint2 ROUND_SHIFT_SINGLE_REF = 10;
static const uint2 ROUND_SHIFT_COMPOUND   = 6;

enum { REF0 = 0, REF1 = 1 };
enum { X = 0, Y = 1 };

_GENX_ matrix<int1,24,8>  g_coefs;
_GENX_ matrix<int1,16,4>  g_coefs_4; // place for 2 4-tap filters because sharp==regular
_GENX_ vector<int2,2>     g_pel;
_GENX_ int2               g_pel_pred_x;
_GENX_ matrix<int2,2,2>   g_pos;
_GENX_ matrix<uint2,2,2>  g_dmv;
_GENX_ matrix<int2,2,2>   g_pos_chroma;
_GENX_ matrix<uint2,2,2>  g_dmv_chroma;
_GENX_ vector<uint4,9>    g_interp_bitcost;
_GENX_ matrix<uint4,2,24> g_interp_bitcost_all; // row0 - nonskip, row1 - skip

_GENX_ uint1 g_use_compound;
_GENX_ uint1 g_skip;
_GENX_ uint1 g_log2_size8x8;

_GENX_ matrix<uint1,23,32> g_hor_input_16;
_GENX_ matrix<uint1,23,16> g_ver_input_16;
_GENX_ matrix<uint1,16,16> g_pred_interp0_16;
_GENX_ matrix<uint1,16,16> g_pred_interp1_16;
_GENX_ matrix<uint1,16,16> g_pred_interp2_16;
_GENX_ matrix<uint1,16,16> g_pred_16;

_GENX_ matrix<uint1,16,16> g_hor_input_8;
_GENX_ matrix<uint1,16,8>  g_ver_input_8;
_GENX_ matrix<uint1,8,8>   g_pred_interp0_8;
_GENX_ matrix<uint1,8,8>   g_pred_interp1_8;
_GENX_ matrix<uint1,8,8>   g_pred_interp2_8;
_GENX_ matrix<uint1,8,8>   g_pred_8;
_GENX_ matrix<int2,8,8>    g_pred_precise_8;
_GENX_ matrix<int2,16,16>  g_pred_precise_16;

_GENX_ matrix<uint1,15,32> g_nv12_input_16;
_GENX_ matrix<int2,8,16>   g_nv12_pred_precise_16;
_GENX_ matrix<uint1,8,16>  g_nv12_pred_16;
_GENX_ matrix<uint1,8,16>  g_nv12_input_8;
_GENX_ matrix<int2,4,8>    g_nv12_pred_precise_8;
_GENX_ matrix<uint1,4,8>   g_nv12_pred_8;

_GENX_ vector<uint1,32>    g_params;

_GENX_ inline float get_lambda    () { return g_params.format<float>()[4/4]; }
_GENX_ inline uint2 get_lambda_int() { return g_params.format<uint2>()[28/2]; }

_GENX_ inline uint1            get_skip    (mode_info_ref mi) { return mi.format<uint1>()[24]; }
_GENX_ inline matrix<int2,2,2> get_mvs     (mode_info_ref mi) { return mi.format<int2>().select<4,1>(); }
_GENX_ inline vector<uint1,2>  get_refs    (mode_info_ref mi) { return mi.format<uint1>().select<2,1>(16); }
_GENX_ inline uint1            get_ref_comb(mode_info_ref mi) { return mi.format<uint1>()[18]; }
_GENX_ inline uint1            get_sb_type (mode_info_ref mi) { return mi.format<uint1>()[19]; }
_GENX_ inline uint1            get_mode    (mode_info_ref mi) { return mi.format<uint1>()[20]; }

_GENX_ inline void set_sb_type    (mode_info_ref mi, uint1 val) { mi.format<uint1>()[19] = val; }
_GENX_ inline void set_skip       (mode_info_ref mi, uint1 val) { mi.format<uint1>()[24] = val; }
_GENX_ inline void set_filter_type(mode_info_ref mi, uint1 val) { mi.format<uint1>()[25] = val; }

enum {
    EIGHTTAP=0,
    EIGHTTAP_SMOOTH=1,
    EIGHTTAP_SHARP=2,
    BILINEAR=3,
    SWITCHABLE_FILTERS=3,
};

enum {
    INTER_FILTER_DIR_OFFSET = (SWITCHABLE_FILTERS + 1) * 2,
    INTER_FILTER_COMP_OFFSET = SWITCHABLE_FILTERS + 1
};

const int1 COEFS[192] = {
    // regular
     0,  0,   0, 64,  0,   0,  0,  0,
     0,  1,  -5, 61,  9,  -2,  0,  0,
     0,  1,  -7, 55, 19,  -5,  1,  0,
     0,  1,  -8, 47, 29,  -6,  1,  0,
     0,  1,  -7, 38, 38,  -7,  1,  0,
     0,  1,  -6, 29, 47,  -8,  1,  0,
     0,  1,  -5, 19, 55,  -7,  1,  0,
     0,  0,  -2,  9, 61,  -5,  1,  0,

    // smooth
     0,  0,   0, 64,  0,   0,  0,  0,
     0,  0,  13, 31, 18,   2,  0,  0,
     0,  0,  10, 30, 21,   3,  0,  0,
     0,  0,   8, 28, 23,   5,  0,  0,
     0, -1,   7, 26, 26,   7, -1,  0,
     0,  0,   5, 23, 28,   8,  0,  0,
     0,  0,   3, 21, 30,  10,  0,  0,
     0,  0,   2, 18, 31,  13,  0,  0,

    // sharp
     0,  0,   0, 64,  0,   0,  0,  0,
    -1,  3,  -6, 62,  8,  -3,  2, -1,
    -2,  5, -11, 58, 19,  -7,  3, -1,
    -2,  5, -12, 50, 30, -10,  4, -1,
    -2,  6, -12, 40, 40, -12,  6, -2,
    -1,  4, -10, 30, 50, -12,  5, -2,
    -1,  3,  -7, 19, 58, -11,  5, -2,
    -1,  2,  -3,  8, 62,  -6,  3, -1,

};

const int1 COEFS_4[64] = {
    // regular
     0, 64,  0,  0,
    -4, 61,  9, -2,
    -6, 55, 19, -4,
    -7, 47, 29, -5,
    -6, 38, 38, -6,
    -5, 29, 47, -7,
    -4, 19, 55, -6,
    -2,  9, 61, -4,

    // smooth
     0, 64,  0,  0,
    13, 31, 18,  2,
    10, 30, 21,  3,
     8, 28, 23,  5,
     6, 26, 26,  6,
     5, 23, 28,  8,
     3, 21, 30, 10,
     2, 18, 31, 13,

    // sharp = regular
};

uint2 INTERP_PROB_BITS_ALL[] = {
    12, 5071, 4182,   // 0 <==
    1813, 3141, 4162, // 1
    3072, 3650, 18,   // 2
    54, 2385, 2585,   // 3
    9, 6033, 3504,    // 4
    1748, 3660, 4169, // 5
    3584, 4292, 9,    // 6
    190, 2468, 1226,  // 7
    18, 5093, 3640,   // 8 <==
    2048, 3335, 4144, // 9
    3284, 3173, 21,   // 10
    92, 1948, 2324,   // 11
    26, 5395, 2592,   // 12 < ==
    1513, 4207, 3386, // 13
    4096, 4566, 6,    // 14
    467, 4243, 602    // 15
};

uint2 INTERP_PROB_BITS[2*(3*3)] =
{
    // compound=0
      30,  5089, 4200,
    5105, 10164, 9275,
    3652,  8711, 7822,
    // compound=1
      35,  6059, 3530,
    5404, 11428, 8899,
    2601,  8625, 6096,
};


static uint1 ZIGZAG_SCAN_64[64] = {
     0,  1,  8,  9,  2,  3, 10, 11,
    16, 17, 24, 25, 18, 19, 26, 27,
     4,  5, 12, 13,  6,  7, 14, 15,
    20, 21, 28, 29, 22, 23, 30, 31,
    32, 33, 40, 41, 34, 35, 42, 43,
    48, 49, 56, 57, 50, 51, 58, 59,
    36, 37, 44, 45, 38, 39, 46, 47,
    52, 53, 60, 61, 54, 55, 62, 63
};

static uint1 ZIGZAG_SCAN_16[16] = {
     0,  1,  4,  5,
     2,  3,  6,  7,
     8,  9, 12, 13,
    10, 11, 14, 15
};


template<uint W, uint H>
void _GENX_ inline transpose8Lines(matrix<short, H, W>& a)
{
    matrix<short, 4, 4> b;
#pragma unroll
    for (int i=0; i<H/8; i++)
    {
#pragma unroll
        for (int j=0; j<W/8; j++)
        {
            b = a.template select<4, 1, 4, 1>(i*8, j*8+4);
            a.template select<4, 1, 4, 1>(i*8, j*8+4) = a.template select<4, 1, 4, 1>(i*8+4, j*8);
            a.template select<4, 1, 4, 1>(i*8+4, j*8) = b;
        }
    }

    matrix<short, 2, W/2> c;
    matrix_ref<int, 2, W/4> c_int = c.template format<int, 2, W/4>();
    matrix_ref<int, H, W/2> a_int = a.template format<int, H, W/2>();

#pragma unroll
    for (int i=0; i<H/4; i++)
    {
        c_int = a_int.template select<2, 1, W/4, 2>(i*4, 1);
        a_int.template select<2, 1, W/4, 2>(i*4, 1) = a_int.template select<2, 1, W/4, 2>(i*4+2, 0);
        a_int.template select<2, 1, W/4, 2>(i*4+2, 0) = c_int;
    }

#pragma unroll
    for (int i=0; i<H/4; i++)
    {
        c = a.template select<2, 2, W/2, 2>(i*4, 1);
        a.template select<2, 2, W/2, 2>(i*4, 1) = a.template select<2, 2, W/2, 2>(i*4+1, 0);
        a.template select<2, 2, W/2, 2>(i*4+1, 0) = c;
    }
}

template<uint BLOCKH, uint BLOCKW>
/*inline*/ _GENX_
uint satdBy8x8(matrix_ref<uint1,BLOCKH,BLOCKW> src, matrix_ref<uint1,BLOCKH,BLOCKW> ref)
{
    uint satd = 0;
    const uint WIDTH = 8;

    matrix<short, BLOCKH, BLOCKW> diff = cm_add<short>(src, -ref);
    matrix<short, BLOCKH, BLOCKW> t, s;
    vector<uint, BLOCKW> abs = 0;

#pragma unroll
    for (int i=0; i<BLOCKH/WIDTH; i++)
    {
        t.template select<4,1,BLOCKW,1>(0) = diff.template select<4,1,BLOCKW,1>(i*WIDTH) + diff.template select<4,1,BLOCKW,1>(4 + i*WIDTH);
        t.template select<4,1,BLOCKW,1>(4) = diff.template select<4,1,BLOCKW,1>(i*WIDTH) - diff.template select<4,1,BLOCKW,1>(4 + i*WIDTH);

        s.template select<2,1,BLOCKW,1>(0) = t.template select<2,1,BLOCKW,1>(0) + t.template select<2,1,BLOCKW,1>(2);
        s.template select<2,1,BLOCKW,1>(2) = t.template select<2,1,BLOCKW,1>(0) - t.template select<2,1,BLOCKW,1>(2);
        s.template select<2,1,BLOCKW,1>(4) = t.template select<2,1,BLOCKW,1>(4) + t.template select<2,1,BLOCKW,1>(6);
        s.template select<2,1,BLOCKW,1>(6) = t.template select<2,1,BLOCKW,1>(4) - t.template select<2,1,BLOCKW,1>(6);

        diff.row(0 + i*WIDTH) = s.row(0) + s.row(1);
        diff.row(1 + i*WIDTH) = s.row(0) - s.row(1);
        diff.row(2 + i*WIDTH) = s.row(2) + s.row(3);
        diff.row(3 + i*WIDTH) = s.row(2) - s.row(3);
        diff.row(4 + i*WIDTH) = s.row(4) + s.row(5);
        diff.row(5 + i*WIDTH) = s.row(4) - s.row(5);
        diff.row(6 + i*WIDTH) = s.row(6) + s.row(7);
        diff.row(7 + i*WIDTH) = s.row(6) - s.row(7);
    }

    transpose8Lines(diff);
#pragma unroll
    for (int i=0; i<BLOCKH/WIDTH; i++)
    {
        t.template select<4,1,BLOCKW,1>(0) = diff.template select<4,1,BLOCKW,1>(i*WIDTH) + diff.template select<4,1,BLOCKW,1>(4 + i*WIDTH);
        t.template select<4,1,BLOCKW,1>(4) = diff.template select<4,1,BLOCKW,1>(i*WIDTH) - diff.template select<4,1,BLOCKW,1>(4 + i*WIDTH);

        s.template select<2,1,BLOCKW,1>(0) = t.template select<2,1,BLOCKW,1>(0) + t.template select<2,1,BLOCKW,1>(2);
        s.template select<2,1,BLOCKW,1>(2) = t.template select<2,1,BLOCKW,1>(0) - t.template select<2,1,BLOCKW,1>(2);
        s.template select<2,1,BLOCKW,1>(4) = t.template select<2,1,BLOCKW,1>(4) + t.template select<2,1,BLOCKW,1>(6);
        s.template select<2,1,BLOCKW,1>(6) = t.template select<2,1,BLOCKW,1>(4) - t.template select<2,1,BLOCKW,1>(6);

        t.template select<4,1,BLOCKW,1>(0) = s.template select<4,2,BLOCKW,1>(0) + s.template select<4,2,BLOCKW,1>(1);
        t.template select<4,1,BLOCKW,1>(4) = s.template select<4,2,BLOCKW,1>(0) - s.template select<4,2,BLOCKW,1>(1);

        t.template select<4,1,BLOCKW,1>(0)  = cm_abs<uint2>(t.template select<4,1,BLOCKW,1>(0)) + cm_abs<uint2>(t.template select<4,1,BLOCKW,1>(4));
        t.template select<2,1,BLOCKW,1>(0) += t.template select<2,1,BLOCKW,1>(2);
        t.template select<1,1,BLOCKW,1>(0) += t.template select<1,1,BLOCKW,1>(1);
        abs += t.row(0);
    }

    satd = cm_sum<uint>(abs);

    return satd;
}

template<uint BLOCKH, uint BLOCKW> /*inline*/ _GENX_
    uint sse(matrix_ref<uint1,BLOCKH,BLOCKW> src, matrix_ref<uint1,BLOCKH,BLOCKW> ref)
{
    vector<uint4, BLOCKW> acc = 0;
    #pragma unroll
    for (int i = 0; i < BLOCKH; i++) {
        vector<int2,BLOCKW> diff = src.row(i) - ref.row(i);
        acc += cm_abs<uint2>(diff) * cm_abs<uint2>(diff);
    }

    uint4 dist = cm_sum<uint4>(acc);
    return dist;
}

_GENX_ /*inline*/ uint sse_8(matrix_ref<uint1,8,8> src, matrix_ref<uint1,8,8> ref)
{
    vector<int2,16> diff;
    vector<int4,16> acc;

    diff = src.format<uint1>().select<16,1>(0) - ref.format<uint1>().select<16,1>(0);
    acc  = diff * diff;
    diff = src.format<uint1>().select<16,1>(16) - ref.format<uint1>().select<16,1>(16);
    acc += diff * diff;
    diff = src.format<uint1>().select<16,1>(32) - ref.format<uint1>().select<16,1>(32);
    acc += diff * diff;
    diff = src.format<uint1>().select<16,1>(48) - ref.format<uint1>().select<16,1>(48);
    acc += diff * diff;

    return cm_sum<uint4>(acc);
}

_GENX_ /*inline*/ void InterpolateRegularH8(uint2 x)
{
    vector<int1,8> fx = g_coefs.row(x);

    if (fx(3) == 64) {
        g_ver_input_8 = g_hor_input_8.select<16,1,8,1>(0,3);
    } else {
        matrix<int2,16,8> acc;
        #pragma unroll
        for (int i = 0; i < 16; i += 2) {
            acc.select<2,1,8,1>(i)  = g_hor_input_8.select<2,1,8,1>(i,1) + g_hor_input_8.select<2,1,8,1>(i,6);
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,2), fx(2));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,3), fx(3));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,4), fx(4));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,5), fx(5));
            acc.select<2,1,8,1>(i) += 32;
        }
        g_ver_input_8 = cm_shr<uint1>(acc, 6, SAT);
    }
}

_GENX_ /*inline*/ void InterpolateSmoothH8(uint2 x)
{
    vector<int1,8> fx = g_coefs.row(x);

    if (fx(3) == 64) {
        g_ver_input_8 = g_hor_input_8.select<16,1,8,1>(0,3);
    } else {
        matrix<int2,16,8> acc;
        #pragma unroll
        for (int i = 0; i < 16; i += 2) {
            acc.select<2,1,8,1>(i)  = cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,1), fx(1));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,2), fx(2));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,3), fx(3));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,4), fx(4));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,5), fx(5));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,6), fx(6));
            acc.select<2,1,8,1>(i) += 32;
        }
        g_ver_input_8 = cm_shr<uint1>(acc, 6, SAT);
    }
}

_GENX_ /*inline*/ void InterpolateCommonH8(uint2 x)
{
    vector<int1,8> fx = g_coefs.row(x);

    if (fx(3) == 64) {
        g_ver_input_8 = g_hor_input_8.select<16,1,8,1>(0,3);
    } else {
        matrix<int2,16,8> acc;
        #pragma unroll
        for (int i = 0; i < 16; i += 2) {
            acc.select<2,1,8,1>(i)  = cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,0), fx(0));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,1), fx(1));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,2), fx(2));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,3), fx(3));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,4), fx(4));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,5), fx(5));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,6), fx(6));
            acc.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,7), fx(7));
            acc.select<2,1,8,1>(i) += 32;
        }
        g_ver_input_8 = cm_shr<uint1>(acc, 6, SAT);
    }
}

_GENX_ /*inline*/ void InterpolateRegularV8(uint2 y)
{
    vector<int1,8> fy = g_coefs.row(y);

    if (fy(3) == 64) {
        g_pred_8 = g_ver_input_8.select<8,1,8,1>(3);
    } else {
        matrix<int2,4,16> acc;
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            acc.row(i)  = g_ver_input_8.select<2,1,8,1>(2*i+1) + g_ver_input_8.select<2,1,8,1>(2*i+6);
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+2), fy(2));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+3), fy(3));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+4), fy(4));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+5), fy(5));
            acc.row(i) += 32;
        }
        g_pred_8 = cm_shr<uint1>(acc, 6, SAT);
    }
}

_GENX_ /*inline*/ void InterpolateSmoothV8(uint2 y)
{
    vector<int1,8> fy = g_coefs.row(y);

    if (fy(3) == 64) {
        g_pred_8 = g_ver_input_8.select<8,1,8,1>(3);
    } else {
        matrix<int2,4,16> acc;
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            acc.row(i)  = cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+1), fy(1));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+2), fy(2));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+3), fy(3));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+4), fy(4));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+5), fy(5));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+6), fy(6));
            acc.row(i) += 32;
        }
        g_pred_8 = cm_shr<uint1>(acc, 6, SAT);
    }
}

_GENX_ /*inline*/ void InterpolateCommonV8(uint2 y)
{
    vector<int1,8> fy = g_coefs.row(y);

    if (fy(3) == 64) {
        g_pred_8 = g_ver_input_8.select<8,1,8,1>(3);
    } else {
        matrix<int2,4,16> acc;
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            acc.row(i)  = cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+0), fy(0));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+1), fy(1));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+2), fy(2));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+3), fy(3));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+4), fy(4));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+5), fy(5));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+6), fy(6));
            acc.row(i) += cm_mul<int2>(g_ver_input_8.select<2,1,8,1>(2*i+7), fy(7));
            acc.row(i) += 32;
        }
        g_pred_8 = cm_shr<uint1>(acc, 6, SAT);
    }
}


_GENX_ /*inline*/ void InterpolateRegularH16(uint2 x)
{
    vector<int1,8> fx = g_coefs.row(x);

    if (fx(3) == 64) {
        g_ver_input_16 = g_hor_input_16.select<23,1,16,1>(0,3);
    } else {
        matrix<int2,2,16> acc;
        #pragma unroll
        for (int i = 0; i < 22; i += 2) {
            #pragma unroll
            for (int j = 0; j < 2; j++) {
                acc.row(j)  = g_hor_input_16.row(i+j).select<16,1>(1) + g_hor_input_16.row(i+j).select<16,1>(6);
                acc.row(j) += g_hor_input_16.row(i+j).select<16,1>(2) * fx(2);
                acc.row(j) += g_hor_input_16.row(i+j).select<16,1>(3) * fx(3);
                acc.row(j) += g_hor_input_16.row(i+j).select<16,1>(4) * fx(4);
                acc.row(j) += g_hor_input_16.row(i+j).select<16,1>(5) * fx(5);
                acc.row(j) += 32;
            }
            g_ver_input_16.select<2,1,16,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
        acc.row(0)  = g_hor_input_16.row(22).select<16,1>(1) + g_hor_input_16.row(22).select<16,1>(6);
        acc.row(0) += g_hor_input_16.row(22).select<16,1>(2) * fx(2);
        acc.row(0) += g_hor_input_16.row(22).select<16,1>(3) * fx(3);
        acc.row(0) += g_hor_input_16.row(22).select<16,1>(4) * fx(4);
        acc.row(0) += g_hor_input_16.row(22).select<16,1>(5) * fx(5);
        acc.row(0) += 32;
        g_ver_input_16.row(22) = cm_shr<uint1>(acc.row(0), 6, SAT);
    }
}

_GENX_ /*inline*/ void InterpolateSmoothH16(uint2 x)
{
    vector<int1,8> fx = g_coefs.row(x);

    if (fx(3) == 64) {
        g_ver_input_16 = g_hor_input_16.select<23,1,16,1>(0,3);
    } else {
        matrix<int2,2,16> acc;
        #pragma unroll
        for (int i = 0; i < 22; i += 2) {
            #pragma unroll
            for (int j = 0; j < 2; j++) {
                acc.row(j)  = cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(1), fx(1));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(2), fx(2));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(3), fx(3));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(4), fx(4));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(5), fx(5));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(6), fx(6));
                acc.row(j) += 32;
            }
            g_ver_input_16.select<2,1,16,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
        acc.row(0)  = cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(1), fx(1));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(2), fx(2));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(3), fx(3));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(4), fx(4));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(5), fx(5));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(6), fx(6));
        acc.row(0) += 32;
        g_ver_input_16.row(22) = cm_shr<uint1>(acc.row(0), 6, SAT);
    }
}

_GENX_ /*inline*/ void InterpolateCommonH16(uint2 x)
{
    vector<int1,8> fx = g_coefs.row(x);

    if (fx(3) == 64) {
        g_ver_input_16 = g_hor_input_16.select<23,1,16,1>(0,3);
    } else {
        matrix<int2,2,16> acc;
        #pragma unroll
        for (int i = 0; i < 22; i += 2) {
            #pragma unroll
            for (int j = 0; j < 2; j++) {
                acc.row(j)  = cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(0), fx(0));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(1), fx(1));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(2), fx(2));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(3), fx(3));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(4), fx(4));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(5), fx(5));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(6), fx(6));
                acc.row(j) += cm_mul<int2>(g_hor_input_16.row(i+j).select<16,1>(7), fx(7));
                acc.row(j) += 32;
            }
            g_ver_input_16.select<2,1,16,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
        acc.row(0)  = cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(0), fx(0));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(1), fx(1));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(2), fx(2));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(3), fx(3));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(4), fx(4));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(5), fx(5));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(6), fx(6));
        acc.row(0) += cm_mul<int2>(g_hor_input_16.row(22).select<16,1>(7), fx(7));
        acc.row(0) += 32;
        g_ver_input_16.row(22) = cm_shr<uint1>(acc.row(0), 6, SAT);
    }
}

_GENX_ /*inline*/ void InterpolateRegularV16(uint2 y)
{
    vector<int1,8> fy = g_coefs.row(y);

    if (fy(3) == 64) {
        g_pred_16 = g_ver_input_16.select<16,1,16,1>(3,0);
    } else {
        matrix<int2,2,16> acc;
        #pragma unroll
        for (int i = 0; i < 16; i += 2) {
            #pragma unroll
            for (int j = 0; j < 2; j++) {
                acc.row(j)  = g_ver_input_16.row(i+j+1) + g_ver_input_16.row(i+j+6);
                acc.row(j) += g_ver_input_16.row(i+j+2) * fy(2);
                acc.row(j) += g_ver_input_16.row(i+j+3) * fy(3);
                acc.row(j) += g_ver_input_16.row(i+j+4) * fy(4);
                acc.row(j) += g_ver_input_16.row(i+j+5) * fy(5);
                acc.row(j) += 32;
            }
            g_pred_16.select<2,1,16,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
    }
}

_GENX_ /*inline*/ void InterpolateSmoothV16(uint2 y)
{
    vector<int1,8> fy = g_coefs.row(y);

    if (fy(3) == 64) {
        g_pred_16 = g_ver_input_16.select<16,1,16,1>(3,0);
    } else {
        matrix<int2,2,16> acc;
        #pragma unroll
        for (int i = 0; i < 16; i += 2) {
            #pragma unroll
            for (int j = 0; j < 2; j++) {
                acc.row(j)  = cm_mul<int2>(g_ver_input_16.row(i+j+1), fy(1));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+2), fy(2));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+3), fy(3));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+4), fy(4));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+5), fy(5));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+6), fy(6));
                acc.row(j) += 32;
            }

            g_pred_16.select<2,1,16,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
    }
}

_GENX_ /*inline*/ void InterpolateCommonV16(uint2 y)
{
    vector<int1,8> fy = g_coefs.row(y);

    if (fy(3) == 64) {
        g_pred_16 = g_ver_input_16.select<16,1,16,1>(3,0);
    } else {
        matrix<int2,2,16> acc;
        #pragma unroll
        for (int i = 0; i < 16; i += 2) {
            #pragma unroll
            for (int j = 0; j < 2; j++) {
                acc.row(j)  = cm_mul<int2>(g_ver_input_16.row(i+j+0), fy(0));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+1), fy(1));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+2), fy(2));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+3), fy(3));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+4), fy(4));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+5), fy(5));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+6), fy(6));
                acc.row(j) += cm_mul<int2>(g_ver_input_16.row(i+j+7), fy(7));
                acc.row(j) += 32;
            }

            g_pred_16.select<2,1,16,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
    }
}

_GENX_ /*inline*/ void Interpolate8x8Precise(uint2 x, uint2 y, uint2 round)
{
    vector<int1,8> fx = g_coefs.row(x);
    vector<int1,8> fy = g_coefs.row(y);

    matrix<int2,16,8> hor_output;

    if (fx(3) == 64) {
        hor_output = g_hor_input_8.select<16,1,8,1>(0,3) * 16;
    } else {
        #pragma unroll
        for (int i = 0; i < 16; i += 2) {
            hor_output.select<2,1,8,1>(i)  = cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,0), fx(0));
            hor_output.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,1), fx(1));
            hor_output.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,2), fx(2));
            hor_output.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,3), fx(3));
            hor_output.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,4), fx(4));
            hor_output.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,5), fx(5));
            hor_output.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,6), fx(6));
            hor_output.select<2,1,8,1>(i) += cm_mul<int2>(g_hor_input_8.select<2,1,8,1>(i,7), fx(7));
            hor_output.select<2,1,8,1>(i) += 2;
            hor_output.select<2,1,8,1>(i) >>= 2;
        }
    }

    int2 offset = (1 << round) >> 1;
    if (fy(3) == 64) {
        round -= 6;
        offset >>= 6;
        g_pred_precise_8 = hor_output.select<8,1,8,1>(3) + offset;
        g_pred_precise_8 >>= round;
    } else {
        vector<int4,16> acc;
        #pragma unroll
        for (int i = 0; i < 4; i++) {
            acc  = cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+0), fy(0));
            acc += cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+1), fy(1));
            acc += cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+2), fy(2));
            acc += cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+3), fy(3));
            acc += cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+4), fy(4));
            acc += cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+5), fy(5));
            acc += cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+6), fy(6));
            acc += cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+7), fy(7));
            acc += offset;
            g_pred_precise_8.select<2,1,8,1>(2*i) = acc >> round;
        }
    }
}

_GENX_ /*inline*/ void Interpolate16x16Precise(uint2 x, uint2 y, uint2 round)
{
    vector<int1,8> fx = g_coefs.row(x);
    vector<int1,8> fy = g_coefs.row(y);

    matrix<int2,23,16> hor_output;

    if (fx(3) == 64) {
        hor_output = g_hor_input_16.select<23,1,16,1>(0,3) * 16;
    } else {
        #pragma unroll
        for (int i = 0; i < 23; i++) {
            hor_output.row(i)  = cm_mul<int2>(g_hor_input_16.row(i).select<16,1>(0), fx(0));
            hor_output.row(i) += cm_mul<int2>(g_hor_input_16.row(i).select<16,1>(1), fx(1));
            hor_output.row(i) += cm_mul<int2>(g_hor_input_16.row(i).select<16,1>(2), fx(2));
            hor_output.row(i) += cm_mul<int2>(g_hor_input_16.row(i).select<16,1>(3), fx(3));
            hor_output.row(i) += cm_mul<int2>(g_hor_input_16.row(i).select<16,1>(4), fx(4));
            hor_output.row(i) += cm_mul<int2>(g_hor_input_16.row(i).select<16,1>(5), fx(5));
            hor_output.row(i) += cm_mul<int2>(g_hor_input_16.row(i).select<16,1>(6), fx(6));
            hor_output.row(i) += cm_mul<int2>(g_hor_input_16.row(i).select<16,1>(7), fx(7));
            hor_output.row(i) += 2;
            hor_output.row(i) >>= 2;
        }
    }

    int2 offset = (1 << round) >> 1;
    if (fy(3) == 64) {
        round -= 6;
        offset >>= 6;
        g_pred_precise_16 = hor_output.select<16,1,16,1>(3,0) + offset;
        if (round)
            g_pred_precise_16 >>= round;
    } else {
        vector<int4,16> acc;
        #pragma unroll
        for (int i = 0; i < 16; i++) {
            acc  = cm_mul<int4>(hor_output.row(i+0), fy(0));
            acc += cm_mul<int4>(hor_output.row(i+1), fy(1));
            acc += cm_mul<int4>(hor_output.row(i+2), fy(2));
            acc += cm_mul<int4>(hor_output.row(i+3), fy(3));
            acc += cm_mul<int4>(hor_output.row(i+4), fy(4));
            acc += cm_mul<int4>(hor_output.row(i+5), fy(5));
            acc += cm_mul<int4>(hor_output.row(i+6), fy(6));
            acc += cm_mul<int4>(hor_output.row(i+7), fy(7));
            acc += offset;
            g_pred_precise_16.row(i) = acc >> round;
        }
    }
}

_GENX_ /*inline*/ void Interpolate4x8PreciseNv12(uint2 x, uint2 y, uint2 round)
{
    vector<int1,4> fx = g_coefs_4.row(x);
    vector<int1,4> fy = g_coefs_4.row(y);

    matrix<int2,8,8> hor_output;

    if (fx(1) == 64) {
        hor_output = g_nv12_input_8.select<8,1,8,1>(0,2) * 16;
    } else {
        #pragma unroll
        for (int i = 0; i < 8; i += 2) {
            hor_output.select<2,1,8,1>(i)  = cm_mul<int2>(g_nv12_input_8.select<2,1,8,1>(i,0), fx(0));
            hor_output.select<2,1,8,1>(i) += cm_mul<int2>(g_nv12_input_8.select<2,1,8,1>(i,2), fx(1));
            hor_output.select<2,1,8,1>(i) += cm_mul<int2>(g_nv12_input_8.select<2,1,8,1>(i,4), fx(2));
            hor_output.select<2,1,8,1>(i) += cm_mul<int2>(g_nv12_input_8.select<2,1,8,1>(i,6), fx(3));
            hor_output.select<2,1,8,1>(i) += 2;
            hor_output.select<2,1,8,1>(i) >>= 2;
        }
    }

    int2 offset = (1 << round) >> 1;
    if (fy(1) == 64) {
        round -= 6;
        offset >>= 6;
        g_nv12_pred_precise_8 = hor_output.select<4,1,8,1>(1) + offset;
        g_nv12_pred_precise_8 >>= round;
    } else {
        vector<int4,16> acc;
        #pragma unroll
        for (int i = 0; i < 2; i++) {
            acc  = cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+0), fy(0));
            acc += cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+1), fy(1));
            acc += cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+2), fy(2));
            acc += cm_mul<int4>(hor_output.select<2,1,8,1>(2*i+3), fy(3));
            acc += offset;
            g_nv12_pred_precise_8.select<2,1,8,1>(2*i) = acc >> round;
        }
    }
}

_GENX_ /*inline*/ void Interpolate8x16PreciseNv12(uint2 x, uint2 y, uint2 round)
{
    vector<int1,8> fx = g_coefs.row(x);
    vector<int1,8> fy = g_coefs.row(y);

    matrix<int2,15,16> hor_output;

    if (fx(3) == 64) {
        hor_output = g_nv12_input_16.select<15,1,16,1>(0,6) * 16;
    } else {
        #pragma unroll
        for (int i = 0; i < 15; i++) {
            hor_output.row(i)  = cm_mul<int2>(g_nv12_input_16.row(i).select<16,1>( 0), fx(0));
            hor_output.row(i) += cm_mul<int2>(g_nv12_input_16.row(i).select<16,1>( 2), fx(1));
            hor_output.row(i) += cm_mul<int2>(g_nv12_input_16.row(i).select<16,1>( 4), fx(2));
            hor_output.row(i) += cm_mul<int2>(g_nv12_input_16.row(i).select<16,1>( 6), fx(3));
            hor_output.row(i) += cm_mul<int2>(g_nv12_input_16.row(i).select<16,1>( 8), fx(4));
            hor_output.row(i) += cm_mul<int2>(g_nv12_input_16.row(i).select<16,1>(10), fx(5));
            hor_output.row(i) += cm_mul<int2>(g_nv12_input_16.row(i).select<16,1>(12), fx(6));
            hor_output.row(i) += cm_mul<int2>(g_nv12_input_16.row(i).select<16,1>(14), fx(7));
            hor_output.row(i) += 2;
            hor_output.row(i) >>= 2;
        }
    }

    int2 offset = (1 << round) >> 1;
    if (fy(3) == 64) {
        round -= 6;
        offset >>= 6;
        g_nv12_pred_precise_16 = hor_output.select<8,1,16,1>(3) + offset;
        g_nv12_pred_precise_16 >>= round;
    } else {
        vector<int4,16> acc;
        #pragma unroll
        for (int i = 0; i < 8; i++) {
            acc  = cm_mul<int4>(hor_output.row(i+0), fy(0));
            acc += cm_mul<int4>(hor_output.row(i+1), fy(1));
            acc += cm_mul<int4>(hor_output.row(i+2), fy(2));
            acc += cm_mul<int4>(hor_output.row(i+3), fy(3));
            acc += cm_mul<int4>(hor_output.row(i+4), fy(4));
            acc += cm_mul<int4>(hor_output.row(i+5), fy(5));
            acc += cm_mul<int4>(hor_output.row(i+6), fy(6));
            acc += cm_mul<int4>(hor_output.row(i+7), fy(7));
            acc += offset;
            g_nv12_pred_precise_16.row(i) = acc >> round;
        }
    }
}

_GENX_ uint2 DecisionBy8x8(SurfaceIndex SRC, SurfaceIndex REF_0, SurfaceIndex REF_1, SurfaceIndex PRED_LUMA, SurfaceIndex PRED_CHROMA)
{
    const uint2 round1 = ROUND_SHIFT_SINGLE_REF - g_use_compound * (ROUND_SHIFT_SINGLE_REF - ROUND_SHIFT_COMPOUND);

    vector<uint2,2> interp = 0;

    if ((g_dmv & 7).any()) {
        matrix<uint1,8,8> src;
        read(SRC, g_pel(X), g_pel(Y), src);

        read(REF_0, g_pos(REF0,X), g_pos(REF0,Y), g_hor_input_8);

        InterpolateRegularH8(g_dmv(REF0,X));

        InterpolateRegularV8(g_dmv(REF0,Y) + 0);
        g_pred_interp0_8 = g_pred_8;

        InterpolateSmoothV8(g_dmv(REF0,Y) + 8);
        g_pred_interp1_8 = g_pred_8;

        InterpolateCommonV8(g_dmv(REF0,Y) + 16);
        g_pred_interp2_8 = g_pred_8;

        if (g_use_compound) {
            read(REF_1, g_pos(REF1,X), g_pos(REF1,Y), g_hor_input_8);

            InterpolateRegularH8(g_dmv(REF1,X));

            InterpolateRegularV8(g_dmv(REF1,Y) + 0);
            g_pred_interp0_8 = cm_avg<uint1>(g_pred_8, g_pred_interp0_8);

            InterpolateSmoothV8(g_dmv(REF1,Y) + 8);
            g_pred_interp1_8 = cm_avg<uint1>(g_pred_8, g_pred_interp1_8);

            InterpolateCommonV8(g_dmv(REF1,Y) + 16);
            g_pred_interp2_8 = cm_avg<uint1>(g_pred_8, g_pred_interp2_8);
        }

        vector<uint4,4> costs;
        if (g_skip) {
            costs(0) = sse_8(src, g_pred_interp0_8);
            costs(1) = sse_8(src, g_pred_interp1_8);
            costs(2) = sse_8(src, g_pred_interp2_8);
        } else {
            costs(0) = satdBy8x8<8,8>(src, g_pred_interp0_8);
            costs(1) = satdBy8x8<8,8>(src, g_pred_interp1_8);
            costs(2) = satdBy8x8<8,8>(src, g_pred_interp2_8);
            costs += 2;
            costs >>= 2;
            costs <<= 11;
        }
        costs += g_interp_bitcost.select<4,1>(0);

        // make decision
        costs.select<4,1>(0) <<= 2;
        costs(1) += 1;
        costs(2) += 2;
        costs(0) = cm_min<uint4>(costs(0), costs(1));
        costs(0) = cm_min<uint4>(costs(0), costs(2));
        interp(Y) = costs(0) & 3;


        read(REF_0, g_pos(REF0,X), g_pos(REF0,Y), g_hor_input_8);

        InterpolateSmoothH8(g_dmv(REF0,X) + 8 * 1);
        InterpolateCommonV8(g_dmv(REF0,Y) + 8 * interp(Y));
        g_pred_interp1_8 = g_pred_8;

        InterpolateCommonH8(g_dmv(REF0,X) + 8 * 2);
        InterpolateCommonV8(g_dmv(REF0,Y) + 8 * interp(Y));
        g_pred_interp2_8 = g_pred_8;

        if (g_use_compound) {
            read(REF_1, g_pos(REF1,X), g_pos(REF1,Y), g_hor_input_8);

            InterpolateSmoothH8(g_dmv(REF1,X) + 8 * 1);
            InterpolateCommonV8(g_dmv(REF1,Y) + 8 * interp(Y));
            g_pred_interp1_8 = cm_avg<uint1>(g_pred_8, g_pred_interp1_8);

            InterpolateCommonH8(g_dmv(REF1,X) + 8 * 2);
            InterpolateCommonV8(g_dmv(REF1,Y) + 8 * interp(Y));
            g_pred_interp2_8 = cm_avg<uint1>(g_pred_8, g_pred_interp2_8);
        }

        vector<uint4,2> bits;
        bits(0) = g_interp_bitcost(interp(Y) + 3);
        bits(1) = g_interp_bitcost(interp(Y) + 6);

        if (g_skip) {
            costs(1) = sse_8(src, g_pred_interp1_8);
            costs(2) = sse_8(src, g_pred_interp2_8);
        } else {
            costs(1) = satdBy8x8<8,8>(src, g_pred_interp1_8);
            costs(2) = satdBy8x8<8,8>(src, g_pred_interp2_8);
            costs.select<2,1>(1) += 2;
            costs.select<2,1>(1) >>= 2;
            costs.select<2,1>(1) <<= 11;
        }
        costs.select<2,1>(1) += bits;

        // make decision
        costs.select<2,1>(1) <<= 2;
        costs(0) &= ~3;
        costs(1) += 1;
        costs(2) += 2;
        costs(0) = cm_min<uint4>(costs(0), costs(1));
        costs(0) = cm_min<uint4>(costs(0), costs(2));
        interp(X) = costs(0) & 3;

        // make final Luma interpolation
        g_dmv += cm_mul<uint2>(interp.replicate<2>(), 8);

        read(REF_0, g_pos(REF0,X), g_pos(REF0,Y), g_hor_input_8);
        Interpolate8x8Precise(g_dmv(REF0,X), g_dmv(REF0,Y), round1);
        if (g_use_compound) {
            matrix<int2,8,8> ref0 = g_pred_precise_8;
            read(REF_1, g_pos(REF1,X), g_pos(REF1,Y), g_hor_input_8);
            Interpolate8x8Precise(g_dmv(REF1,X), g_dmv(REF1,Y), ROUND_SHIFT_COMPOUND);
            g_pred_precise_8 += ref0;
            g_pred_precise_8 += 16;
            g_pred_8 = cm_asr<uint1>(g_pred_precise_8, 5, SAT);
        } else {
            g_pred_8 = cm_add<uint1>(g_pred_precise_8, 0, SAT);
        }
        write(PRED_LUMA, g_pel_pred_x, g_pel(Y), g_pred_8);

    } else {
        g_pos += 3;
        read(REF_0, g_pos(REF0,X), g_pos(REF0,Y), g_pred_8);
        if (g_use_compound) {
            matrix<uint1,8,8> ref1;
            read(REF_1, g_pos(REF1,X), g_pos(REF1,Y), ref1);
            g_pred_8 = cm_avg<uint1>(g_pred_8, ref1);
        }
        write(PRED_LUMA, g_pel_pred_x, g_pel(Y), g_pred_8);
    }

    // make final Chroma interpolation
    uint2 pel_pred_y = g_pel(Y) >> 1;
    if ((g_dmv_chroma & 15).any()) {
        g_dmv_chroma += cm_mul<uint2>(interp.replicate<2>(), 8);
        g_dmv_chroma &= 15; // sharp 4-tap filter == regular 4-tap filter

        read_plane(REF_0, GENX_SURFACE_UV_PLANE, g_pos_chroma(REF0,X), g_pos_chroma(REF0,Y), g_nv12_input_8);
        Interpolate4x8PreciseNv12(g_dmv_chroma(REF0,X), g_dmv_chroma(REF0,Y), round1);
        if (g_use_compound) {
            matrix<int2,4,8> ref0 = g_nv12_pred_precise_8;
            read_plane(REF_1, GENX_SURFACE_UV_PLANE, g_pos_chroma(REF1,X), g_pos_chroma(REF1,Y), g_nv12_input_8);
            Interpolate4x8PreciseNv12(g_dmv_chroma(REF1,X), g_dmv_chroma(REF1,Y), ROUND_SHIFT_COMPOUND);
            g_nv12_pred_precise_8 += ref0;
            g_nv12_pred_precise_8 += 16;
            g_nv12_pred_8 = cm_asr<uint1>(g_nv12_pred_precise_8, 5, SAT);
        } else {
            g_nv12_pred_8 = cm_add<uint1>(g_nv12_pred_precise_8, 0, SAT);
        }
        write(PRED_CHROMA, g_pel_pred_x, pel_pred_y, g_nv12_pred_8);
    } else {
        g_pos_chroma.column(X) += 2;
        g_pos_chroma.column(Y) += 1;
        read_plane(REF_0, GENX_SURFACE_UV_PLANE, g_pos_chroma(REF0,X), g_pos_chroma(REF0,Y), g_nv12_pred_8);
        if (g_use_compound) {
            matrix<uint1,4,8> ref1;
            read_plane(REF_1, GENX_SURFACE_UV_PLANE, g_pos_chroma(REF1,X), g_pos_chroma(REF1,Y), ref1);
            g_nv12_pred_8 = cm_avg<uint1>(g_nv12_pred_8, ref1);
        }
        write(PRED_CHROMA, g_pel_pred_x, pel_pred_y, g_nv12_pred_8);
    }

    return (interp(X) << 4) + interp(Y);
}

_GENX_ uint2 DecisionBy16x16(SurfaceIndex SRC, SurfaceIndex REF_0, SurfaceIndex REF_1, SurfaceIndex PRED_LUMA, SurfaceIndex PRED_CHROMA)
{
    const uint2 round1 = ROUND_SHIFT_SINGLE_REF - g_use_compound * (ROUND_SHIFT_SINGLE_REF - ROUND_SHIFT_COMPOUND);

    vector<uint2,2> interp = 0;

    uint2 size = 8 << g_log2_size8x8;
    vector<int2,2> blk;

    if ((g_dmv & 7).any()) {
        vector<uint4,4> costs(0);
        matrix<uint1,16,16> src;

        blk(Y) = 0;
        do {
            blk(X) = 0;
            do {

                read(SRC, g_pel(X) + blk(X), g_pel(Y) + blk(Y), src);

                read(REF_0, g_pos(REF0,X) + blk(X), g_pos(REF0,Y) + blk(Y) + 0,  g_hor_input_16.select<8,1,32,1>(0,0));
                read(REF_0, g_pos(REF0,X) + blk(X), g_pos(REF0,Y) + blk(Y) + 8,  g_hor_input_16.select<8,1,32,1>(8,0));
                read(REF_0, g_pos(REF0,X) + blk(X), g_pos(REF0,Y) + blk(Y) + 16, g_hor_input_16.select<7,1,32,1>(16,0));

                InterpolateRegularH16(g_dmv(REF0,X));

                InterpolateRegularV16(g_dmv(REF0,Y) + 0);
                g_pred_interp0_16 = g_pred_16;

                InterpolateSmoothV16(g_dmv(REF0,Y) + 8);
                g_pred_interp1_16 = g_pred_16;

                InterpolateCommonV16(g_dmv(REF0,Y) + 16);
                g_pred_interp2_16 = g_pred_16;

                if (g_use_compound) {
                    read(REF_1, g_pos(REF1,X) + blk(X), g_pos(REF1,Y) + blk(Y) + 0,  g_hor_input_16.select<8,1,32,1>(0));
                    read(REF_1, g_pos(REF1,X) + blk(X), g_pos(REF1,Y) + blk(Y) + 8,  g_hor_input_16.select<8,1,32,1>(8));
                    read(REF_1, g_pos(REF1,X) + blk(X), g_pos(REF1,Y) + blk(Y) + 16, g_hor_input_16.select<7,1,32,1>(16));

                    InterpolateRegularH16(g_dmv(REF1,X));

                    InterpolateRegularV16(g_dmv(REF1,Y) + 0);
                    g_pred_interp0_16 = cm_avg<uint1>(g_pred_16, g_pred_interp0_16);

                    InterpolateSmoothV16(g_dmv(REF1,Y) + 8);
                    g_pred_interp1_16 = cm_avg<uint1>(g_pred_16, g_pred_interp1_16);

                    InterpolateCommonV16(g_dmv(REF1,Y) + 16);
                    g_pred_interp2_16 = cm_avg<uint1>(g_pred_16, g_pred_interp2_16);
                }

                if (g_skip) {
                    costs(0) += sse<16,16>(src, g_pred_interp0_16);
                    costs(1) += sse<16,16>(src, g_pred_interp1_16);
                    costs(2) += sse<16,16>(src, g_pred_interp2_16);
                } else {
                    costs(0) += satdBy8x8<16,16>(src, g_pred_interp0_16);
                    costs(1) += satdBy8x8<16,16>(src, g_pred_interp1_16);
                    costs(2) += satdBy8x8<16,16>(src, g_pred_interp2_16);
                }

                blk(X) += 16;
            } while (blk(X) < size);

            blk(Y) += 16;
        } while (blk(Y) < size);

        // making decision
        if (!g_skip) {
            costs += 2;
            costs >>= 2;
            costs <<= 11;
        }
        costs += g_interp_bitcost.select<4,1>(0);

        interp(Y) = 0;
        if (costs(1) < costs(0)) costs(0) = costs(1), interp(Y) = 1;
        if (costs(2) < costs(0)) costs(0) = costs(2), interp(Y) = 2;

        costs.select<2,1>(1) = 0;
        blk(Y) = 0;
        do {
            blk(X) = 0;
            do {

                read(SRC, g_pel(X) + blk(X), g_pel(Y) + blk(Y), src);

                read(REF_0, g_pos(REF0,X) + blk(X), g_pos(REF0,Y) + blk(Y) + 0,  g_hor_input_16.select<8,1,32,1>(0));
                read(REF_0, g_pos(REF0,X) + blk(X), g_pos(REF0,Y) + blk(Y) + 8,  g_hor_input_16.select<8,1,32,1>(8));
                read(REF_0, g_pos(REF0,X) + blk(X), g_pos(REF0,Y) + blk(Y) + 16, g_hor_input_16.select<7,1,32,1>(16));

                InterpolateSmoothH16(g_dmv(REF0,X) + 8 * 1);
                InterpolateCommonV16(g_dmv(REF0,Y) + 8 * interp(Y));
                g_pred_interp1_16 = g_pred_16;

                InterpolateCommonH16(g_dmv(REF0,X) + 8 * 2);
                InterpolateCommonV16(g_dmv(REF0,Y) + 8 * interp(Y));
                g_pred_interp2_16 = g_pred_16;

                if (g_use_compound) {
                    read(REF_1, g_pos(REF1,X) + blk(X), g_pos(REF1,Y) + blk(Y) + 0,  g_hor_input_16.select<8,1,32,1>(0));
                    read(REF_1, g_pos(REF1,X) + blk(X), g_pos(REF1,Y) + blk(Y) + 8,  g_hor_input_16.select<8,1,32,1>(8));
                    read(REF_1, g_pos(REF1,X) + blk(X), g_pos(REF1,Y) + blk(Y) + 16, g_hor_input_16.select<7,1,32,1>(16));

                    InterpolateSmoothH16(g_dmv(REF1,X) + 8 * 1);
                    InterpolateCommonV16(g_dmv(REF1,Y) + 8 * interp(Y));
                    g_pred_interp1_16 = cm_avg<uint1>(g_pred_16, g_pred_interp1_16);

                    InterpolateCommonH16(g_dmv(REF1,X) + 8 * 2);
                    InterpolateCommonV16(g_dmv(REF1,Y) + 8 * interp(Y));
                    g_pred_interp2_16 = cm_avg<uint1>(g_pred_16, g_pred_interp2_16);
                }

                if (g_skip) {
                    costs(1) += sse<16,16>(src, g_pred_interp1_16);
                    costs(2) += sse<16,16>(src, g_pred_interp2_16);
                } else {
                    costs(1) += satdBy8x8<16,16>(src, g_pred_interp1_16);
                    costs(2) += satdBy8x8<16,16>(src, g_pred_interp2_16);
                }

                blk(X) += 16;
            } while (blk(X) < size);

            blk(Y) += 16;
        } while (blk(Y) < size);

        // making decision
        vector<uint4,2> bits;
        bits(0) = g_interp_bitcost(interp(Y) + 3);
        bits(1) = g_interp_bitcost(interp(Y) + 6);

        if (!g_skip) {
            costs.select<2,1>(1) += 2;
            costs.select<2,1>(1) >>= 2;
            costs.select<2,1>(1) <<= 11;
        }
        costs.select<2,1>(1) += bits;
        if (costs(1) < costs(0)) costs(0) = costs(1), interp(X) = 1;
        if (costs(2) < costs(0)) costs(0) = costs(2), interp(X) = 2;


        // make final Luma interpolation
        g_dmv += cm_mul<uint2>(interp.replicate<2>(), 8);

        blk(Y) = 0;
        do {
            blk(X) = 0;
            do {
                read(REF_0, g_pos(REF0,X) + blk(X), g_pos(REF0,Y) + blk(Y) + 0,  g_hor_input_16.select<8,1,32,1>(0));
                read(REF_0, g_pos(REF0,X) + blk(X), g_pos(REF0,Y) + blk(Y) + 8,  g_hor_input_16.select<8,1,32,1>(8));
                read(REF_0, g_pos(REF0,X) + blk(X), g_pos(REF0,Y) + blk(Y) + 16, g_hor_input_16.select<7,1,32,1>(16));

                Interpolate16x16Precise(g_dmv(REF0,X), g_dmv(REF0,Y), round1);

                if (g_use_compound) {
                    matrix<int2,16,16> ref0 = g_pred_precise_16;
                    read(REF_1, g_pos(REF1,X) + blk(X), g_pos(REF1,Y) + blk(Y) + 0,  g_hor_input_16.select<8,1,32,1>(0));
                    read(REF_1, g_pos(REF1,X) + blk(X), g_pos(REF1,Y) + blk(Y) + 8,  g_hor_input_16.select<8,1,32,1>(8));
                    read(REF_1, g_pos(REF1,X) + blk(X), g_pos(REF1,Y) + blk(Y) + 16, g_hor_input_16.select<7,1,32,1>(16));

                    Interpolate16x16Precise(g_dmv(REF1,X), g_dmv(REF1,Y), ROUND_SHIFT_COMPOUND);

                    g_pred_precise_16 += ref0;
                    g_pred_precise_16 += 16;
                    g_pred_16 = cm_asr<uint1>(g_pred_precise_16, 5, SAT);
                } else {
                    g_pred_16 = cm_add<uint1>(g_pred_precise_16, 0, SAT);
                }
                write(PRED_LUMA, g_pel_pred_x + blk(X), g_pel(Y) + blk(Y), g_pred_16);

                blk(X) += 16;
            } while (blk(X) < size);

            blk(Y) += 16;
        } while (blk(Y) < size);

    } else {
        g_pos += 3;
        blk(Y) = 0;
        do {
            blk(X) = 0;
            do {
                read(REF_0, g_pos(REF0,X) + blk(X), g_pos(REF0,Y) + blk(Y), g_pred_16);
                if (g_use_compound) {
                    matrix<uint1,16,16> ref1;
                    read(REF_1, g_pos(REF1,X) + blk(X), g_pos(REF1,Y) + blk(Y), ref1);
                    g_pred_16 = cm_avg<uint1>(g_pred_16, ref1);
                }
                write(PRED_LUMA, g_pel_pred_x + blk(X), g_pel(Y) + blk(Y), g_pred_16);

                blk(X) += 16;
            } while (blk(X) < size);

            blk(Y) += 16;
        } while (blk(Y) < size);
    }

    uint2 size_y = size >> 1;
    uint2 pel_pred_y = g_pel(Y) >> 1;

    // make final Chroma interpolation
    if ((g_dmv_chroma & 15).any()) {
        g_pos_chroma.column(X) -= 4;
        g_pos_chroma.column(Y) -= 2;

        g_dmv_chroma += cm_mul<uint2>(interp.replicate<2>(), 8);

        blk(Y) = 0;
        do {
            blk(X) = 0;
            do {
                read_plane(REF_0, GENX_SURFACE_UV_PLANE, g_pos_chroma(REF0,X) + blk(X), g_pos_chroma(REF0,Y) + blk(Y) + 0, g_nv12_input_16.select<8,1,32,1>(0));
                read_plane(REF_0, GENX_SURFACE_UV_PLANE, g_pos_chroma(REF0,X) + blk(X), g_pos_chroma(REF0,Y) + blk(Y) + 8, g_nv12_input_16.select<7,1,32,1>(8));

                Interpolate8x16PreciseNv12(g_dmv_chroma(REF0,X), g_dmv_chroma(REF0,Y), round1);

                if (g_use_compound) {
                    matrix<int2,8,16> ref0 = g_nv12_pred_precise_16;
                    read_plane(REF_1, GENX_SURFACE_UV_PLANE, g_pos_chroma(REF1,X) + blk(X), g_pos_chroma(REF1,Y) + blk(Y) + 0, g_nv12_input_16.select<8,1,32,1>(0));
                    read_plane(REF_1, GENX_SURFACE_UV_PLANE, g_pos_chroma(REF1,X) + blk(X), g_pos_chroma(REF1,Y) + blk(Y) + 8, g_nv12_input_16.select<7,1,32,1>(8));

                    Interpolate8x16PreciseNv12(g_dmv_chroma(REF1,X), g_dmv_chroma(REF1,Y), ROUND_SHIFT_COMPOUND);

                    g_nv12_pred_precise_16 += ref0;
                    g_nv12_pred_precise_16 += 16;
                    g_nv12_pred_16 = cm_asr<uint1>(g_nv12_pred_precise_16, 5, SAT);
                } else {
                    g_nv12_pred_16 = cm_add<uint1>(g_nv12_pred_precise_16, 0, SAT);
                }

                write(PRED_CHROMA, g_pel_pred_x + blk(X), pel_pred_y + blk(Y), g_nv12_pred_16);

                blk(X) += 16;
            } while (blk(X) < size);

            blk(Y) += 8;
        } while (blk(Y) < size_y);

    } else {
        g_pos_chroma.column(X) += 2;
        g_pos_chroma.column(Y) += 1;
        blk(Y) = 0;
        do {
            blk(X) = 0;
            do {
                read_plane(REF_0, GENX_SURFACE_UV_PLANE, g_pos_chroma(REF0,X) + blk(X), g_pos_chroma(REF0,Y) + blk(Y), g_nv12_pred_16);
                if (g_use_compound) {
                    matrix<uint1,8,16> ref1;
                    read_plane(REF_1, GENX_SURFACE_UV_PLANE, g_pos_chroma(REF1,X) + blk(X), g_pos_chroma(REF1,Y) + blk(Y), ref1);
                    g_nv12_pred_16 = cm_avg<uint1>(g_nv12_pred_16, ref1);
                }
                write(PRED_CHROMA, g_pel_pred_x + blk(X), pel_pred_y + blk(Y), g_nv12_pred_16);

                blk(X) += 16;
            } while (blk(X) < size);

            blk(Y) += 8;
        } while (blk(Y) < size_y);
    }

    return (interp(X) << 4) + interp(Y);
}

#ifdef JOIN_MI
_GENX_ inline uint2 not_joinable(mode_info_ref mi1, mode_info_ref mi2)
{
    uint2 mask = cm_pack_mask(mi1.format<uint2>() != mi2.format<uint2>());
    mask &= 0x20F; // mask everything but MV, refIdxComb and sbType
    return mask;
}

_GENX_ inline void check_is_joinable(SurfaceIndex MODE_INFO, mode_info_ref mi_data, vector<uint2,2> mi_pos)
{
    uint1 skip = get_skip(mi_data);
    mode_info mi_data2;

    read(MODE_INFO, (mi_pos(X) + 1) * SIZEOF_MODE_INFO, mi_pos(Y), mi_data2);
    if (get_mode(mi_data2) == 255 || not_joinable(mi_data, mi_data2)) return;
    skip &= get_skip(mi_data2);

    read(MODE_INFO, mi_pos(X) * SIZEOF_MODE_INFO, mi_pos(Y) + 1, mi_data2);
    if (get_mode(mi_data2) == 255 || not_joinable(mi_data, mi_data2)) return;
    skip &= get_skip(mi_data2);

    read(MODE_INFO, (mi_pos(X) + 1) * SIZEOF_MODE_INFO, mi_pos(Y) + 1, mi_data2);
    if (not_joinable(mi_data, mi_data2)) return;
    skip &= get_skip(mi_data2);

    set_skip(mi_data, skip);
    set_sb_type(mi_data, 6); // BLOCK_16X16
}
#endif

extern "C" _GENX_MAIN_
    void InterpolateDecision(SurfaceIndex PARAM, SurfaceIndex SRC, vector<SurfaceIndex,7> REF, SurfaceIndex MODE_INFO,
                             SurfaceIndex PRED_LUMA, SurfaceIndex PRED_CHROMA, int pred_padding)
{
    read(PARAM, 0, g_params);
    float lambda = get_lambda();
    uint2 lambda_int = get_lambda_int();

    vector<int2,2> origin;
    origin(0) = get_thread_origin_x();
    origin(1) = get_thread_origin_y();

    vector<int2,2> origxy = origin * 32;

    g_coefs   = vector<int1,192>(COEFS);
    g_coefs_4 = vector<int1,64>(COEFS_4);
    vector<uint1,16> scan_z2r(ZIGZAG_SCAN_16);

    vector<uint2,24> interp_bits;
    interp_bits.select<18,1>() = vector<uint2,18>(INTERP_PROB_BITS);

    g_interp_bitcost_all.row(0) = lambda_int * interp_bits;
    g_interp_bitcost_all.row(1) = vector<uint4,24>(lambda * interp_bits + 0.5f);

    uint4 num8x8;
    for (uint4 i = 0; i < 16; i += num8x8) {
        uint1 raster = scan_z2r(i);
        vector<int2,2> locxy;
        locxy(X) = raster & 3;
        locxy(Y) = raster >> 2;
        locxy <<= 3;

        g_pel = origxy + locxy;
        g_pel_pred_x = g_pel(X) + pred_padding;
        vector<uint2,2> mi_pos = g_pel >> 3;

        mode_info mi_data;
        read(MODE_INFO, mi_pos(X) * SIZEOF_MODE_INFO, mi_pos(Y), mi_data);

        g_log2_size8x8 = get_sb_type(mi_data) >> 2;
        num8x8 = 1 << (g_log2_size8x8 << 1);

        if (g_log2_size8x8 == 3 && ((mi_pos(X) | mi_pos(Y)) & 7) != 0)
            return; // in the middle of 64x64

        if (get_mode(mi_data) == 255)
            continue;
#ifdef JOIN_MI
        if (g_log2_size8x8 == 0 && ((mi_pos(X) | mi_pos(Y)) & 1) == 0) {
            check_is_joinable(MODE_INFO, mi_data, mi_pos);
            g_log2_size8x8 = get_sb_type(mi_data) >> 2;
            num8x8 = 1 << (g_log2_size8x8 << 1);
        }
#endif
        vector<uint1,2> refIdx = get_refs(mi_data);

        matrix<int2,2,2> mvs = get_mvs(mi_data);
        g_pos = g_pel.replicate<2>() - 3;
        g_pos += (mvs >> 3);
        g_dmv = (mvs & 7);

        g_pos_chroma = g_pel.replicate<2>();
        g_pos_chroma.column(Y) >>= 1;
        g_pos_chroma.column(X) -= 2;
        g_pos_chroma.column(Y) -= 1;
        g_pos_chroma.column(X) += ((mvs.column(X) >> 4) << 1);
        g_pos_chroma.column(Y) += (mvs.column(Y) >> 4);
        g_dmv_chroma = (mvs & 15);
        g_dmv_chroma >>= 1;

        g_use_compound = 3 - (refIdx[1] & 0x3);
        g_dmv.format<uint4>().select<1,1>(1).merge(0, g_use_compound ^ 1);
        g_dmv_chroma.format<uint4>().select<1,1>(1).merge(0, g_use_compound ^ 1);

        g_skip = get_skip(mi_data);

        uint2 filter_type = 0;
        uint1 ref0 = cm_avg<uint1>(refIdx(0), 0);

        // get lambda*bits for current block: skip/nonskip, singleref/compound
        g_interp_bitcost = g_interp_bitcost_all.format<uint4>().select<9,1>(24 * g_skip + 9 * g_use_compound);

        filter_type = (g_log2_size8x8)
            ? DecisionBy16x16(SRC, REF(ref0), REF(1), PRED_LUMA, PRED_CHROMA)
            : DecisionBy8x8(SRC, REF(ref0), REF(1), PRED_LUMA, PRED_CHROMA);

        // report
        set_filter_type(mi_data, filter_type);

        write(MODE_INFO, mi_pos(X) * SIZEOF_MODE_INFO, mi_pos(Y), mi_data.row(0).select<28, 1>(0));
    }
}
