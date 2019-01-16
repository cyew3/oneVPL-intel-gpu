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

#if CMRT_EMU
  #define assert_emu assert
#else
  #define assert_emu(...) (void)0
#endif

static const uint2 BLOCK_SIZE = 8;
static const uint2 SIZEOF_MODE_INFO = 32;

enum {
    BASE_TOP_LEFT = 0x80,
    BASE_TOP      = 0x7f,
    BASE_LEFT     = 0x81,
};

enum {
    BASE_TOP_LEFT_X4 = BASE_TOP_LEFT | (BASE_TOP_LEFT << 8) | (BASE_TOP_LEFT << 16) | (BASE_TOP_LEFT << 24),
    BASE_TOP_X4      = BASE_TOP      | (BASE_TOP      << 8) | (BASE_TOP      << 16) | (BASE_TOP      << 24),
    BASE_LEFT_X4     = BASE_LEFT     | (BASE_LEFT     << 8) | (BASE_LEFT     << 16) | (BASE_LEFT     << 24),
};


enum { LEFT, TOP, RIGHT, BOTTOM };

enum { TOP_RIGHT, BOTTOM_LEFT };

enum { X, Y };

enum { COL=X, ROW=Y };

enum { DC_PRED=0, V_PRED=1, H_PRED=2, D45_PRED=3, D135_PRED=4, D117_PRED=5, D153_PRED=6, D207_PRED=7, D63_PRED=8,
       SMOOTH_PRED=9, SMOOTH_V_PRED=10, SMOOTH_H_PRED=11, PAETH_PRED=12, AV1_INTRA_MODES=PAETH_PRED+1 };

enum {
    ANGLE_36,  ANGLE_39,  ANGLE_42,  ANGLE_45,  ANGLE_48,  ANGLE_51,  ANGLE_54,
    ANGLE_58,  ANGLE_61,  ANGLE_64,  ANGLE_67,  ANGLE_70,  ANGLE_73,  ANGLE_76,
    ANGLE_81,  ANGLE_84,  ANGLE_87,  ANGLE_90,  ANGLE_93,  ANGLE_96,  ANGLE_99,
    ANGLE_104, ANGLE_107, ANGLE_110, ANGLE_113, ANGLE_116, ANGLE_119, ANGLE_122,
    ANGLE_126, ANGLE_129, ANGLE_132, ANGLE_135, ANGLE_138, ANGLE_141, ANGLE_144,
    ANGLE_148, ANGLE_151, ANGLE_154, ANGLE_157, ANGLE_160, ANGLE_163, ANGLE_166,
    ANGLE_171, ANGLE_174, ANGLE_177, ANGLE_180, ANGLE_183, ANGLE_186, ANGLE_189,
    ANGLE_194, ANGLE_197, ANGLE_200, ANGLE_203, ANGLE_206, ANGLE_209, ANGLE_212,
};

enum { OUT_OF_PIC = 255 };

enum {
    BLOCK_8X8   = 3,
    BLOCK_16X16 = 6,
    BLOCK_32X32 = 9,
    BLOCK_64X64 = 12,
};

static const uint2 ANGLE_TO_DX[56] = {
   90,   80,   71,   64,   57,   51,   45,
   40,   35,   31,   27,   23,   19,   15,
   11,    7,    3,    1,    3,    7,   11,
   15,   19,   23,   27,   31,   35,   40,
   45,   51,   57,   64,   71,   80,   90,
  102,  116,  132,  151,  178,  215,  273,
  372,  547, 1023,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,
};

static const uint2 ANGLE_TO_DY[56] = {
      1,   1,   1,   1,    1,   1,   1,
      1,   1,   1,   1,    1,   1,   1,
      1,   1,   1,   1, 1023, 547, 372,
    273, 215, 178, 151,  132, 116, 102,
     90,  80,  71,  64,   57,  51,  45,
     40,  35,  31,  27,   23,  19,  15,
     11,   7,   3,   1,    3,   7,  11,
     15,  19,  23,  27,   31,  35,  40,
};

static const uint2 INTRA_MODE_COST[3][13] = {
    { 756, 2051+3*512, 2284+3*512, 3222+3*512, 2646+3*512, 2772+3*512, 2970+3*512, 2724+3*512, 2724+3*512, 2111, 2858, 2808,  887 }, // 8x8
    { 591, 2170+3*512, 2216+3*512, 3737+3*512, 3154+3*512, 3316+3*512, 3472+3*512, 3087+3*512, 2980+3*512, 1926, 2478, 2272, 1119 }, // 16x16
    { 405, 2592+3*512, 2589+3*512, 4356+3*512, 5505+3*512, 4720+3*512, 5844+3*512, 5013+3*512, 4993+3*512, 2652, 2784, 2292, 1001 }, // 32x32
};

static const uint1 SCAN_Z2R[16] = { 0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15 };

static const uint1 SMOOTH_WEIGHTS_8[]  = { 255, 197, 146, 105, 73, 50, 37, 32 };
static const uint1 SMOOTH_WEIGHTS_16[] = { 255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16 };
static const uint1 SMOOTH_WEIGHTS_32[] = { 255, 240, 225, 210, 196, 182, 169, 157, 145, 133, 122, 111, 101, 92,  83,  74,
                                            66,  59,  52,  45,  39,  34,  29,  25,  21,  17,  14,  12,  10,  9,   8,   8 };
static const uint1 HAS_TOP_RIGHT[8] = {
          //   7<-0
          //
    0xff, // 11111111
    0x55, // 01010101
    0x77, // 01110111  0
    0x55, // 01010101  |
    0x7f, // 01111111  v
    0x55, // 01010101  7
    0x77, // 01110111
    0x55, // 01010101
};

static const uint1 HAS_BOTTOM_LEFT[8] = {
          //   7<-0
          //
    0x55, // 01010101
    0x11, // 00010001
    0x55, // 01010101  0
    0x01, // 00000001  |
    0x55, // 01010101  v
    0x11, // 00010001  7
    0x55, // 01010101
    0x00  // 00000000
};

_GENX_ matrix<uint1,8,8>   g_src8;
_GENX_ matrix<uint1,16,16> g_src16;
_GENX_ matrix<uint1,32,32> g_src32;
_GENX_ matrix<uint1,8,8>   g_pred8;
_GENX_ matrix<uint1,16,16> g_pred16;
_GENX_ matrix<uint1,32,32> g_pred32;
_GENX_ vector<uint1,8>     g_weights_8;
_GENX_ vector<uint1,8>     g_inv_weights_8;
_GENX_ vector<uint1,16>    g_weights_16;
_GENX_ vector<uint1,16>    g_inv_weights_16;
_GENX_ vector<uint1,32>    g_weights_32;
_GENX_ vector<uint1,32>    g_inv_weights_32;
_GENX_ vector<uint1,32>    g_sequence_1_to_32;
_GENX_ vector<uint1,8>     g_top_right;
_GENX_ vector<uint1,8>     g_bottom_left;
_GENX_ vector<uint2,4>     g_availability;
_GENX_ vector<int4,2>      g_read_loc;
_GENX_ uint4               g_best_decision;
_GENX_ uint2               g_lambda;
_GENX_ uint2               g_early_exit;
//_GENX_ vector<uint2,56>    g_angle_to_dx;
//_GENX_ vector<uint2,56>    g_angle_to_dy;

typedef matrix<uint1,1,SIZEOF_MODE_INFO>      mode_info;
typedef matrix_ref<uint1,1,SIZEOF_MODE_INFO>  mode_info_ref;

_GENX_ inline uint1 get_sb_type(mode_info_ref mi) { return mi.format<uint1>()[19]; }
_GENX_ inline uint1 get_mode   (mode_info_ref mi) { return mi.format<uint1>()[20]; }
_GENX_ inline uint4 get_intra_info(mode_info_ref mi) { return mi.format<uint4>()[28/4];}

_GENX_ inline uint2 has(const vector_ref<uint1,8> lookup_table, vector<uint2,2> mi) {
    mi &= 7;
    uint1 byte = lookup_table(mi(ROW));
    uint1 bit = byte >> mi(COL);
    return bit & 1;
}

_GENX_ inline matrix<uint1,2,32> get_pred_pels_8(uint1 aboveleft_pel, vector_ref<uint1,16> above_row, vector_ref<uint1,16> left_col)
{
    matrix<uint1,2,32> pred_pels;

    if (g_availability(LEFT)) {
        pred_pels.row(LEFT).select<16,1>(1) = left_col;
        uint1 last_avail = pred_pels(LEFT, 8 + g_availability(BOTTOM));
        pred_pels.row(LEFT).select<8,1>(1+8).merge(last_avail, g_sequence_1_to_32.select<8,1>() > g_availability(BOTTOM));
        pred_pels.row(LEFT).select<16,1>(16) = pred_pels(LEFT,16);
        pred_pels.row(TOP) = left_col(0);
    } else {
        vector<uint1,1> value = BASE_LEFT;
        value.merge(above_row(0), g_availability(TOP));
        pred_pels.row(LEFT) = value(0);
        pred_pels.row(TOP) = BASE_TOP;
        pred_pels(TOP,0) = BASE_TOP_LEFT;
    }

    if (g_availability(TOP)) {
        pred_pels.row(TOP).select<16,1>(1) = above_row;
        uint1 last_avail = pred_pels(TOP, 8 + g_availability(RIGHT));
        pred_pels.row(TOP).select<8,1>(1+8).merge(last_avail, g_sequence_1_to_32.select<8,1>() > g_availability(RIGHT));
        pred_pels.row(TOP).select<16,1>(16) = pred_pels(TOP,16);
        pred_pels(TOP,0) = above_row(0);
    }

    pred_pels.select<1,1,1,1>(TOP,0).merge(aboveleft_pel, g_availability(TOP) & g_availability(LEFT));

    pred_pels(LEFT,0) = pred_pels(TOP,0);

    return pred_pels;
}

_GENX_ inline matrix<uint1,2,96> get_pred_pels_16(uint1 aboveleft_pel, vector_ref<uint1,32> above_row, vector_ref<uint1,32> left_col)
{
    matrix<uint1,2,96> pred_pels;

    if (g_availability(LEFT)) {
        pred_pels.row(LEFT).select<32,1>(32) = left_col;
        vector<uint1,4> last_avail = pred_pels(LEFT, 47 + g_availability(BOTTOM));
        pred_pels.row(LEFT).select<16,1>(48).merge(last_avail(0), g_sequence_1_to_32.select<16,1>() > g_availability(BOTTOM));
        pred_pels.row(LEFT).select<32,1>(64).format<uint4>() = last_avail.format<uint4>()(0);
        pred_pels.row(TOP).select<4,1>(0) = left_col(0);
        pred_pels.row(TOP).format<uint4>() = pred_pels.row(TOP).format<uint4>()(0);
    } else {
        vector<uint1,1> value = BASE_LEFT;
        value.merge(above_row(0), g_availability(TOP));
        pred_pels.row(LEFT).select<4,1>(0) = value(0);
        pred_pels.row(LEFT).format<uint4>() = pred_pels.row(LEFT).format<uint4>()(0);

        pred_pels.row(TOP).select<32,1>( 0).format<uint4>() = BASE_TOP_LEFT_X4;
        pred_pels.row(TOP).select<64,1>(32).format<uint4>() = BASE_TOP_X4;
    }

    if (g_availability(TOP)) {
        pred_pels.row(TOP).select<32,1>(32) = above_row;
        vector<uint1,4> last_avail = pred_pels(TOP, 47 + g_availability(RIGHT));
        pred_pels.row(TOP).select<16,1>(48).merge(last_avail(0), g_sequence_1_to_32.select<16,1>() > g_availability(RIGHT));
        pred_pels.row(TOP).select<32,1>(64).format<uint4>() = last_avail.format<uint4>()(0);
        pred_pels.row(TOP).select< 4,1>(0) = above_row(0);
        pred_pels.row(TOP).select<32,1>(0).format<uint4>() = pred_pels.row(TOP).select<32,1>(0).format<uint4>()(0);
    }

    pred_pels.select<1,1,4,1>(TOP,0).merge(aboveleft_pel, -g_availability(TOP) & -g_availability(LEFT));
    pred_pels.select<1,1,32,1>(TOP ,0).format<uint4>() = pred_pels.select<1,1,32,1>(TOP,0).format<uint4>()(0);
    pred_pels.select<1,1,32,1>(LEFT,0).format<uint4>() = pred_pels.select<1,1,32,1>(TOP,0).format<uint4>()(0);

    return pred_pels;
}

_GENX_ inline matrix<uint1,2,128> get_pred_pels_32(uint1 aboveleft_pel, vector_ref<uint1,64> above_row, vector_ref<uint1,64> left_col)
{
    matrix<uint1,2,128> pred_pels;

    if (g_availability(LEFT)) {
        pred_pels.row(LEFT).select<64,1>(32) = left_col;
        vector<uint1,4> last_avail = pred_pels(LEFT, 63 + g_availability(BOTTOM));
        pred_pels.row(LEFT).select<32,1>(64).merge(last_avail(0), g_sequence_1_to_32 > g_availability(BOTTOM));
        pred_pels.row(LEFT).select<32,1>(96).format<uint4>() = last_avail.format<uint4>()(0);
        pred_pels.row(TOP).select<4,1>(0) = left_col(0);
        pred_pels.row(TOP).format<uint4>() = pred_pels.row(TOP).format<uint4>()(0);
    } else {
        vector<uint1,1> value = BASE_LEFT;
        value.merge(above_row(0), g_availability(TOP));
        pred_pels.row(LEFT).select<4,1>(0) = value(0);
        pred_pels.row(LEFT).format<uint4>() = pred_pels.row(LEFT).format<uint4>()(0);

        pred_pels.row(TOP).select<32,1>( 0).format<uint4>() = BASE_TOP_LEFT_X4;
        pred_pels.row(TOP).select<96,1>(32).format<uint4>() = BASE_TOP_X4;
    }

    if (g_availability(TOP)) {
        pred_pels.row(TOP).select<64,1>(32) = above_row;
        vector<uint1,4> last_avail = pred_pels(TOP, 63 + g_availability(RIGHT));
        pred_pels.row(TOP).select<32,1>(64).merge(last_avail(0), g_sequence_1_to_32 > g_availability(RIGHT));
        pred_pels.row(TOP).select<32,1>(96).format<uint4>() = last_avail.format<uint4>()(0);
        pred_pels.row(TOP).select< 4,1>(0) = above_row(0);
        pred_pels.row(TOP).select<32,1>(0).format<uint4>() = pred_pels.row(TOP).select<32,1>(0).format<uint4>()(0);
    }

    pred_pels.select<1,1,4,1>(TOP,0).merge(aboveleft_pel, -g_availability(TOP) & -g_availability(LEFT));
    pred_pels.select<1,1,32,1>(TOP ,0).format<uint4>() = pred_pels.select<1,1,32,1>(TOP,0).format<uint4>()(0);
    pred_pels.select<1,1,32,1>(LEFT,0).format<uint4>() = pred_pels.select<1,1,32,1>(TOP,0).format<uint4>()(0);

    return pred_pels;
}

_GENX_ inline uint2 sad8()
{
    vector<uint2,16> sad;
    sad = cm_sad2<uint2> (g_src8.select<2,1,8,1>(0), g_pred8.select<2,1,8,1>(0));
    sad = cm_sada2<uint2>(g_src8.select<2,1,8,1>(2), g_pred8.select<2,1,8,1>(2), sad);
    sad = cm_sada2<uint2>(g_src8.select<2,1,8,1>(4), g_pred8.select<2,1,8,1>(4), sad);
    sad = cm_sada2<uint2>(g_src8.select<2,1,8,1>(6), g_pred8.select<2,1,8,1>(6), sad);
    return cm_sum<uint2>(sad.select<8,2>());
}

_GENX_ /*inline*/ uint2 sad16()
{
    vector<uint2,16> sad;
    sad = cm_sad2<uint2> (g_src16.row(0), g_pred16.row(0));
    #pragma unroll
    for (int i = 1; i < 16; i++)
        sad = cm_sada2<uint2>(g_src16.row(i), g_pred16.row(i), sad);
    return cm_sum<uint2>(sad.select<8,2>());
}

_GENX_ /*inline*/ uint4 sad32()
{
    vector<uint2,16> sad;
    sad = cm_sad2<uint2> (g_src32.format<uint1,64,16>().row(0), g_pred32.format<uint1,64,16>().row(0));
    #pragma unroll
    for (int i = 1; i < 64; i++)
        sad = cm_sada2<uint2>(g_src32.format<uint1,64,16>().row(i), g_pred32.format<uint1,64,16>().row(i), sad);
    return cm_sum<uint4>(sad.select<8,2>());
}

_GENX_ inline void predict_intra_dc_8(matrix_ref<uint1,2,32> pred_pels)
{
    vector_ref<uint1,8> t = pred_pels.row(TOP).select<8,1>(1);
    vector_ref<uint1,8> l = pred_pels.row(LEFT).select<8,1>(1);
    vector<uint1,16> p;
    p.select<8,1>(0).merge(t, l, -g_availability(TOP));
    p.select<8,1>(8).merge(l, t, -g_availability(LEFT));
    uint2 sum = cm_sum<uint2>(p);
    sum += 8;
    vector<uint1,4> val = sum >> 4;
    g_pred8.format<uint4>() = val.format<uint4>()(0);
}

_GENX_ inline void predict_intra_dc_16(matrix_ref<uint1,2,96> pred_pels)
{
    vector_ref<uint1,16> t = pred_pels.row(TOP).select<16,1>(32);
    vector_ref<uint1,16> l = pred_pels.row(LEFT).select<16,1>(32);
    vector<uint1,32> p;
    p.select<16,1>( 0).merge(t, l, -g_availability(TOP));
    p.select<16,1>(16).merge(l, t, -g_availability(LEFT));
    uint2 sum = cm_sum<uint2>(p);
    sum += 16;
    vector<uint1,4> val = sum >> 5;
    g_pred16.format<uint4>() = val.format<uint4>()(0);
}

_GENX_ inline void predict_intra_dc_32(matrix_ref<uint1,2,128> pred_pels)
{
    vector_ref<uint1,32> t = pred_pels.row(TOP).select<32,1>(32);
    vector_ref<uint1,32> l = pred_pels.row(LEFT).select<32,1>(32);
    vector<uint1,64> p;
    p.select<32,1>( 0).merge(t, l, -g_availability(TOP));
    p.select<32,1>(32).merge(l, t, -g_availability(LEFT));
    uint2 sum = cm_sum<uint2>(p);
    sum += 32;
    vector<uint1,4> val = sum >> 6;
    g_pred32.format<uint4>() = val.format<uint4>()(0);
}

_GENX_ inline void predict_intra_v_8(matrix_ref<uint1,2,32> pred_pels)
{
    g_pred8.select<2,1,8,1>(0) = pred_pels.select<1,1,8,1>(TOP,1).replicate<2>();
    g_pred8.select<2,1,8,1>(2).format<uint4>() = g_pred8.select<2,1,8,1>(0).format<uint4>();
    g_pred8.select<4,1,8,1>(4).format<uint4>() = g_pred8.select<4,1,8,1>(0).format<uint4>();
}

_GENX_ inline void predict_intra_v_16(matrix_ref<uint1,2,96> pred_pels)
{
    g_pred16.select< 1,1,16,1>(0) = pred_pels.select<1,1,16,1>(TOP,32);
    g_pred16.select< 1,1,16,1>(1).format<uint4>() = g_pred16.select<1,1,16,1>(0).format<uint4>();
    g_pred16.select< 2,1,16,1>(2).format<uint4>() = g_pred16.select<2,1,16,1>(0).format<uint4>();
    g_pred16.select<12,1,16,1>(4).format<uint4>() = g_pred16.select<4,1,16,1>(0).format<uint4>().replicate<3>();
}

_GENX_ inline void predict_intra_v_32(matrix_ref<uint1,2,128> pred_pels)
{
    g_pred32.select< 1,1,32,1>(0) = pred_pels.select<1,1,32,1>(TOP,32);
    g_pred32.select< 1,1,32,1>(1).format<uint4>() = g_pred32.select<1,1,32,1>(0).format<uint4>();
    g_pred32.select<30,1,32,1>(2).format<uint4>() = g_pred32.select<2,1,32,1>(0).format<uint4>().replicate<15>();
}

_GENX_ inline void predict_intra_h_8(matrix_ref<uint1,2,32> pred_pels)
{
    vector<uint2,8> pred = pred_pels.select<1,1,8,1>(LEFT,1) * 0x101;
    g_pred8.format<uint2>() = pred.replicate<8,1,4,0>();
}

_GENX_ inline void predict_intra_h_16(matrix_ref<uint1,2,96> pred_pels)
{
    vector<uint2,16> pred = pred_pels.select<1,1,16,1>(LEFT,32) * 0x101;
    g_pred16.format<uint2>() = pred.replicate<16,1,8,0>();
}

_GENX_ inline void predict_intra_h_32(matrix_ref<uint1,2,128> pred_pels)
{
    vector<uint2,32> pred = pred_pels.select<1,1,32,1>(LEFT,32) * 0x101;
    g_pred32.format<uint2>() = pred.replicate<32,1,16,0>();
}

_GENX_ inline void predict_intra_smooth_8(matrix_ref<uint1,2,32> pred_pels)
{
    vector<uint2,8> weighted_bottom_left = cm_mul<uint2>(g_inv_weights_8, pred_pels(LEFT,1+7));
    vector<uint2,8> weighted_top_right   = cm_mul<uint2>(g_inv_weights_8, pred_pels(TOP, 1+7)) + 256;

    vector<uint1,8> top  = pred_pels.row(TOP).select<8,1>(1);
    vector<uint1,8> left = pred_pels.row(LEFT).select<8,1>(1);

    vector<uint4,16> acc;

    #pragma unroll
    for (int i = 0; i < 8; i += 2) {
        acc = weighted_top_right.replicate<2>() + weighted_bottom_left.replicate<2,1,8,0>(i);
        acc += top.replicate<2>() * g_weights_8.replicate<2,1,8,0>(i);
        acc += g_weights_8.replicate<2>() * left.replicate<2,1,8,0>(i);
        g_pred8.select<2,1,8,1>(i) = cm_asr<uint1>(acc, 9, SAT);
    }
}

_GENX_ inline void predict_intra_smooth_16(matrix_ref<uint1,2,96> pred_pels)
{
    vector<uint2,16> weighted_bottom_left = cm_mul<uint2>(g_inv_weights_16, pred_pels(LEFT,32+15));
    vector<uint2,16> weighted_top_right   = cm_mul<uint2>(g_inv_weights_16, pred_pels(TOP, 32+15)) + 256;

    vector<uint1,16> top  = pred_pels.row(TOP).select<16,1>(32);
    vector<uint1,16> left = pred_pels.row(LEFT).select<16,1>(32);

    vector<uint4,16> acc;

    #pragma unroll
    for (int i = 0; i < 16; i++) {
        acc = weighted_top_right + weighted_bottom_left(i);
        acc += g_weights_16(i) * top;
        acc += g_weights_16 * left(i);
        g_pred16.row(i) = cm_asr<uint1>(acc, 9, SAT);
    }
}

_GENX_ inline void predict_intra_smooth_32(matrix_ref<uint1,2,128> pred_pels)
{
    vector<uint2,32> weighted_bottom_left = cm_mul<uint2>(g_inv_weights_32, pred_pels(LEFT,32+31));
    vector<uint2,32> weighted_top_right   = cm_mul<uint2>(g_inv_weights_32, pred_pels(TOP, 32+31)) + 256;

    vector<uint1,32> top  = pred_pels.row(TOP).select<32,1>(32);
    vector<uint1,32> left = pred_pels.row(LEFT).select<32,1>(32);

    vector<uint4,16> acc;

    #pragma unroll
    for (int i = 0; i < 32; i++) {
        acc = weighted_top_right.select<16,1>(0) + weighted_bottom_left(i);
        acc += g_weights_32(i) * top.select<16,1>(0);
        acc += g_weights_32.select<16,1>(0) * left(i);
        g_pred32.row(i).select<16,1>(0) = cm_asr<uint1>(acc, 9, SAT);
        acc = weighted_top_right.select<16,1>(16) + weighted_bottom_left(i);
        acc += g_weights_32(i) * top.select<16,1>(16);
        acc += g_weights_32.select<16,1>(16) * left(i);
        g_pred32.row(i).select<16,1>(16) = cm_asr<uint1>(acc, 9, SAT);
    }
}

_GENX_ inline void predict_intra_smooth_v_8(matrix_ref<uint1,2,32> pred_pels)
{
    vector_ref<uint1,8> top = pred_pels.row(TOP).select<8,1>(1);

    vector<uint2,8> weighted_bottom_left = g_inv_weights_8 * pred_pels(LEFT,1+7);

    matrix<uint2,8,8> acc;
    acc.select<2,1,8,1>(0)  = weighted_bottom_left.replicate<2,1,8,0>(0) + 128;
    acc.select<2,1,8,1>(0) += cm_mul<uint2>(top.replicate<2>(), g_weights_8.replicate<2,1,8,0>(0));
    acc.select<2,1,8,1>(2)  = weighted_bottom_left.replicate<2,1,8,0>(2) + 128;
    acc.select<2,1,8,1>(2) += cm_mul<uint2>(top.replicate<2>(), g_weights_8.replicate<2,1,8,0>(2));
    acc.select<2,1,8,1>(4)  = weighted_bottom_left.replicate<2,1,8,0>(4) + 128;
    acc.select<2,1,8,1>(4) += cm_mul<uint2>(top.replicate<2>(), g_weights_8.replicate<2,1,8,0>(4));
    acc.select<2,1,8,1>(6)  = weighted_bottom_left.replicate<2,1,8,0>(6) + 128;
    acc.select<2,1,8,1>(6) += cm_mul<uint2>(top.replicate<2>(), g_weights_8.replicate<2,1,8,0>(6));

    g_pred8 = cm_asr<uint1>(acc, 8, SAT);
}

_GENX_ inline void predict_intra_smooth_v_16(matrix_ref<uint1,2,96> pred_pels)
{
    vector_ref<uint1,16> top = pred_pels.row(TOP).select<16,1>(32);

    vector<uint2,16> weighted_bottom_left = g_inv_weights_16 * pred_pels(LEFT,32+15);
    weighted_bottom_left += 128;

    matrix<uint2,16,16> acc;
    acc  = cm_mul<uint2>(top.replicate<16>(), g_weights_16.replicate<16,1,16,0>());
    acc += weighted_bottom_left.replicate<16,1,16,0>();

    g_pred16 = cm_asr<uint1>(acc, 8, SAT);
}

_GENX_ inline void predict_intra_smooth_v_32(matrix_ref<uint1,2,128> pred_pels)
{
    vector_ref<uint1,32> top = pred_pels.row(TOP).select<32,1>(32);

    vector<uint2,32> weighted_bottom_left = g_inv_weights_32 * pred_pels(LEFT,32+31);
    weighted_bottom_left += 128;

    #pragma unroll
    for (int r = 0; r < 32; r++) {
        vector<uint2,32> acc;
        acc  = top * g_weights_32(r);
        acc += weighted_bottom_left(r);
        g_pred32.row(r) = cm_asr<uint1>(acc, 8, SAT);
    }
}

_GENX_ inline void predict_intra_smooth_h_8(matrix_ref<uint1,2,32> pred_pels)
{
    vector_ref<uint1,8> left = pred_pels.row(LEFT).select<8,1>(1);

    vector<uint2,8> weighted_top_right = cm_mul<uint2>(g_inv_weights_8, pred_pels(TOP,1+7)) + 128;

    matrix<uint2,8,8> acc;
    acc = cm_mul<uint2>(g_weights_8.replicate<8>(), left.replicate<8,1,8,0>());
    acc += weighted_top_right.replicate<8>();

    g_pred8 = cm_asr<uint1>(acc, 8, SAT);
}

_GENX_ inline void predict_intra_smooth_h_16(matrix_ref<uint1,2,96> pred_pels)
{
    vector_ref<uint1,16> left = pred_pels.row(LEFT).select<16,1>(32);

    vector<uint2,16> weighted_top_right = cm_mul<uint2>(g_inv_weights_16, pred_pels(TOP,32+15)) + 128;

    matrix<uint2,16,16> acc;
    acc = cm_mul<uint2>(g_weights_16.replicate<16>(), left.replicate<16,1,16,0>());
    acc += weighted_top_right.replicate<16>();

    g_pred16 = cm_asr<uint1>(acc, 8, SAT);
}

_GENX_ inline void predict_intra_smooth_h_32(matrix_ref<uint1,2,128> pred_pels)
{
    vector_ref<uint1,32> left = pred_pels.row(LEFT).select<32,1>(32);

    vector<uint2,32> weighted_top_right = cm_mul<uint2>(g_inv_weights_32, pred_pels(TOP,32+31)) + 128;

    #pragma unroll
    for (int r = 0; r < 32; r++) {
        vector<uint2,32> acc;
        acc  = g_weights_32 * left(r);
        acc += weighted_top_right;
        g_pred32.row(r) = cm_asr<uint1>(acc, 8, SAT);
    }
}

_GENX_ inline void predict_intra_paeth_8(matrix_ref<uint1,2,32> pred_pels)
{
    vector<uint1,4> al = pred_pels(TOP,0);
    vector<uint1,8> a = pred_pels.row(TOP).select<8,1>(1);
    vector<uint1,8> l = pred_pels.row(LEFT).select<8,1>(1);
    vector<int2,8> a_minus_al = a - al(0);
    vector<int2,8> l_minus_al = l - al(0);
    vector<uint2,8> diff_l = cm_abs<uint2>(a_minus_al);
    vector<uint2,8> diff_a = cm_abs<uint2>(l_minus_al);

    g_pred8.format<uint4>() = al.format<uint4>()(0);

    #pragma unroll
    for (int i = 0; i < 8; i += 2) {
        vector<uint2,16> diff_al = cm_abs<uint2>(cm_add<int2>(a_minus_al.replicate<2>(), l_minus_al.replicate<2,1,8,0>(i)));
        vector<uint1,16> min_diff = cm_min<uint2>(diff_al, diff_a.replicate<2,1,8,0>(i));
        g_pred8.select<2,1,8,1>(i).merge(a.replicate<2>(), diff_a.replicate<2,1,8,0>(i) <= diff_al);
        g_pred8.select<2,1,8,1>(i).merge(l.replicate<2,1,8,0>(i), diff_l.replicate<2>() <= min_diff);
    }
}

_GENX_ inline void predict_intra_paeth_16(matrix_ref<uint1,2,96> pred_pels)
{
    vector<uint1,4> al = pred_pels(TOP,31);
    vector_ref<uint1,16> a = pred_pels.row(TOP).select<16,1>(32);
    vector_ref<uint1,16> l = pred_pels.row(LEFT).select<16,1>(32);
    vector<int2,16> a_minus_al = a - al(0);
    vector<int2,16> l_minus_al = l - al(0);
    vector<uint2,16> diff_l = cm_abs<uint2>(a_minus_al);
    vector<uint2,16> diff_a = cm_abs<uint2>(l_minus_al);

    g_pred16.format<uint4>() = al.format<uint4>()(0);

    //#pragma unroll
    for (int i = 0; i < 16; i++) {
        vector<uint2,16> diff_al = cm_abs<uint2>(cm_add<int2>(a_minus_al, l_minus_al(i)));
        vector<uint1,16> min_diff = cm_min<uint2>(diff_al, diff_a(i));
        g_pred16.row(i).merge(a, diff_a(i) <= diff_al);
        g_pred16.row(i).merge(l(i), diff_l <= min_diff);
    }
}

_GENX_ inline void predict_intra_paeth_32(matrix_ref<uint1,2,128> pred_pels)
{
    uint1 al = pred_pels(TOP,31);
    vector_ref<uint1,32> a = pred_pels.row(TOP).select<32,1>(32);
    vector_ref<uint1,32> l = pred_pels.row(LEFT).select<32,1>(32);
    vector<int2,32> a_minus_al = a - al;
    vector<int2,32> l_minus_al = l - al;
    vector<uint2,32> diff_l = cm_abs<uint2>(a_minus_al);
    vector<uint2,32> diff_a = cm_abs<uint2>(l_minus_al);

    //#pragma unroll
    for (int i = 0; i < 32; i++) {
        vector<uint2,32> diff_al = cm_abs<uint2>(cm_add<int2>(a_minus_al, l_minus_al(i)));
        vector<uint1,32> min_diff = cm_min<uint2>(diff_al, diff_a(i));
        g_pred32.row(i).merge(a, al, diff_a(i) <= diff_al);
        g_pred32.row(i).merge(l(i), diff_l <= min_diff);
    }
}

_GENX_ inline void predict_intra_z1_8(vector_ref<uint1,32> top_pred_pels, uint2 dx)
{
    vector<uint2,8> x = dx * g_sequence_1_to_32.select<8,1>();
    vector<uint2,8> base = x >> 6; base += 1;
    vector<uint2,8> shift = x & 0x3f; shift >>= 1;
    vector<uint2,8> inv_shift = 32 - shift;

    vector<uint1,16> p0, p1;
    matrix<uint2,8,8> p;

    #pragma unroll
    for (uint2 r = 0; r < 8; r += 2) {
        matrix<uint1,2,16> tmp;
        tmp.row(0) = top_pred_pels.select<16,1>(base(r));
        tmp.row(1) = top_pred_pels.select<16,1>(base(r+1));

        p0 = tmp.select<2,1,8,1>(0,0);
        p1 = tmp.select<2,1,8,1>(0,1);

        p.select<2,1,8,1>(r) = p0 * inv_shift.replicate<2,1,8,0>(r);
        p.select<2,1,8,1>(r) += cm_mul<uint2>(p1, shift.replicate<2,1,8,0>(r));
    }

    p += 16;
    g_pred8 = cm_asr<uint1>(p, 5, SAT);
}

_GENX_ inline void predict_intra_z1_16(vector_ref<uint1,96> top_pred_pels, uint2 dx)
{
    vector<uint2,16> x = dx * g_sequence_1_to_32.select<16,1>();
    vector<uint2,16> base = x >> 6; base += 32;
    vector<uint2,16> shift = x & 0x3f; shift >>= 1;
    vector<uint2,16> inv_shift = 32 - shift;
    matrix<uint2,16,16> p;

    #pragma unroll
    for (uint2 r = 0; r < 16; r++) {
        vector<uint1,24> tmp;
        tmp = top_pred_pels.select<24,1>(base(r));

        p.row(r)  = cm_mul<uint2>(tmp.select<16,1>(0), inv_shift(r));
        p.row(r) += cm_mul<uint2>(tmp.select<16,1>(1), shift(r));
    }

    p += 16;
    g_pred16 = cm_asr<uint1>(p, 5, SAT);
}

_GENX_ inline void predict_intra_z2_8(matrix_ref<uint1,2,32> pred_pels, uint2 dx, uint2 dy)
{
    matrix<int2,2,8> xy;
    xy.row(X) = g_sequence_1_to_32.select<8,1>() * (-dx);
    xy.row(Y) = g_sequence_1_to_32.select<8,1>() * (-dy);

    matrix<int2,2,8> base = xy >> 6;
    matrix<uint2,2,8> shift = xy & 0x3f; shift >>= 1;
    matrix<uint2,2,8> inv_shift = 32 - shift;

    vector<int2,16> base2 = base.row(Y).replicate<2>() + 1;
    base2.select<8,1>(8) += 1;

    matrix<uint2,8,8> p;

    #pragma unroll
    for (uint2 r = 0; r < 8; r += 2) {
        vector<int2,16> base1 = base.select<1,1,2,1>(X,r).replicate<2,1,8,0>() + g_sequence_1_to_32.select<8,1>().replicate<2>();
        vector<uint1,16> top_p0 = pred_pels.row(TOP).iselect(cm_add<uint2>(base1, 0, SAT));
        vector<uint1,16> top_p1 = pred_pels.row(TOP).iselect(cm_add<uint2>(base1, 1, SAT));
        vector<uint2,16> top_p = top_p0 * inv_shift.row(X).replicate<2,1,8,0>(r);
        top_p += cm_mul<uint2>(top_p1, shift.row(X).replicate<2,1,8,0>(r));

        vector<uint1,16> left_p0 = pred_pels.row(LEFT).iselect(cm_add<uint2>(base2, 0, SAT));
        vector<uint1,16> left_p1 = pred_pels.row(LEFT).iselect(cm_add<uint2>(base2, 1, SAT));
        base2 += 2;
        p.select<2,1,8,1>(r) = left_p0 * inv_shift.row(Y).replicate<2>();
        p.select<2,1,8,1>(r) += cm_mul<uint2>(left_p1, shift.row(Y).replicate<2>());

        p.select<2,1,8,1>(r).merge(top_p, base1 >= 0);
    }

    p += 16;
    g_pred8 = cm_asr<uint1>(p, 5, SAT);
}

_GENX_ inline void predict_intra_z2_16(matrix_ref<uint1,2,96> pred_pels, uint2 dx, uint2 dy)
{
    matrix<int2,2,16> xy;
    xy.row(X) = g_sequence_1_to_32.select<16,1>() * (-dx);
    xy.row(Y) = g_sequence_1_to_32.select<16,1>() * (-dy);

    matrix<int2,2,16> base = xy >> 6; base += 31;
    matrix<uint2,2,16> shift = xy & 0x3f; shift >>= 1;
    matrix<uint2,2,16> inv_shift = 32 - shift;

    vector<int2,16> base2 = base.row(Y) + 1;

    matrix<uint2,16,16> p;
    vector<uint2,16> top_p;

    #pragma unroll
    for (uint2 r = 0; r < 16; r++) {
        vector<int2,16> base1 = base(X,r) + g_sequence_1_to_32.select<16,1>();
        vector<uint1,16> top_p0 = pred_pels.row(TOP).iselect(cm_add<uint2>(base1, 0, SAT));
        vector<uint1,16> top_p1 = pred_pels.row(TOP).iselect(cm_add<uint2>(base1, 1, SAT));
        top_p  = cm_mul<uint2>(top_p0, inv_shift(X,r));
        top_p += cm_mul<uint2>(top_p1, shift(X,r));

        vector<uint1,16> left_p0 = pred_pels.row(LEFT).iselect(cm_add<uint2>(base2, 0, SAT));
        vector<uint1,16> left_p1 = pred_pels.row(LEFT).iselect(cm_add<uint2>(base2, 1, SAT));
        base2 += 1;
        p.row(r)  = cm_mul<uint2>(left_p0, inv_shift.row(Y));
        p.row(r) += cm_mul<uint2>(left_p1, shift.row(Y));

        p.row(r).merge(top_p, base1 >= 0);
    }

    p += 16;
    g_pred16 = cm_asr<uint1>(p, 5, SAT);
}

_GENX_ inline void predict_intra_z3_8(vector_ref<uint1,32> left_pred_pels, uint2 dy)
{
    predict_intra_z1_8(left_pred_pels, dy);

    matrix<uint1,8,8> tmp;
    tmp.select<4,1,8,1>(0) = g_pred8.select<8,1,4,2>(0,0);
    tmp.select<4,1,8,1>(4) = g_pred8.select<8,1,4,2>(0,1);
    g_pred8.select<2,1,8,1>(0) = tmp.select<8,1,2,4>(0,0);
    g_pred8.select<2,1,8,1>(2) = tmp.select<8,1,2,4>(0,1);
    g_pred8.select<2,1,8,1>(4) = tmp.select<8,1,2,4>(0,2);
    g_pred8.select<2,1,8,1>(6) = tmp.select<8,1,2,4>(0,3);
}

_GENX_ inline void transpose_16x16(matrix_ref<uint1,16,16> in, matrix_ref<uint1,16,16> out)
{
    matrix<uint1,16,16> t;
    t.select<4,1,16,1>(0)    = in.select<16,1,4,4>(0,0);
    t.select<4,1,16,1>(4)    = in.select<16,1,4,4>(0,1);
    t.select<4,1,16,1>(8)    = in.select<16,1,4,4>(0,2);
    t.select<4,1,16,1>(12)   = in.select<16,1,4,4>(0,3);
    out.select<4,1,16,1>(0)  = t.select<16,1,4,4>(0,0);
    out.select<4,1,16,1>(4)  = t.select<16,1,4,4>(0,1);
    out.select<4,1,16,1>(8)  = t.select<16,1,4,4>(0,2);
    out.select<4,1,16,1>(12) = t.select<16,1,4,4>(0,3);
}

_GENX_ inline void transpose_16x16(matrix_ref<uint1,16,16> inout)
{
    transpose_16x16(inout, inout);
}

_GENX_ inline void predict_intra_z3_16(vector_ref<uint1,96> left_pred_pels, uint2 dy)
{
    predict_intra_z1_16(left_pred_pels, dy);
    transpose_16x16(g_pred16);
}

_GENX_ inline vector<uint2,8> filter(vector_ref<uint1,8> p0, vector_ref<uint1,8> p1, uint1 c0, uint1 c1)
{
    return cm_add<uint2>(cm_mul<uint2>(p0, c0), cm_mul<uint2>(p1, c1));
}

_GENX_ inline vector<uint2,16> filter(vector_ref<uint1,16> p0, vector_ref<uint1,16> p1, uint1 c0, uint1 c1)
{
    return cm_add<uint2>(cm_mul<uint2>(p0, c0), cm_mul<uint2>(p1, c1));
}

_GENX_ inline vector<uint1,32> filter_and_pack_2x16(vector_ref<uint1,16> p0, vector_ref<uint1,16> p1,
                                                    vector_ref<uint1,16> q0, vector_ref<uint1,16> q1,
                                                    uint1 c0, uint1 c1, uint1 d0, uint1 d1)
{
    vector<uint1,32> res;
    if (c0 == c1 && d0 == d1) {
        res.select<16,1>( 0) = cm_avg<uint1>(p0, p1);
        res.select<16,1>(16) = cm_avg<uint1>(q0, q1);
    } else {
        vector<uint2,32> acc;
        acc.select<16,1>( 0) = filter(p0, p1, c0, c1);
        acc.select<16,1>(16) = filter(q0, q1, d0, d1);
        acc += 16;
        res = cm_asr<uint1>(acc, 5, SAT);
    }
    return res;
}

_GENX_ inline vector<uint2,16> filter(vector_ref<uint1,16> p0, vector_ref<uint1,16> p1, vector_ref<uint2,16> c0, vector_ref<uint2,16> c1)
{
    return cm_add<uint2>(cm_mul<uint2>(p0, c0), cm_mul<uint2>(p1, c1));
}

_GENX_ inline void predict_intra_d45_8(vector_ref<uint1,32> top_pred_pels)
{
    g_pred8 = top_pred_pels.replicate<8,1,8,1>(2);
}

_GENX_ inline void predict_intra_d45_16(vector_ref<uint1,96> top_pred_pels)
{
    #pragma unroll
    for (uint2 r = 0; r < 15; r++)
        g_pred16.row(r) = top_pred_pels.select<16,1>(33 + r);
    g_pred16.row(15) = g_pred16.format<uint1>().select<16,1>(14*16+1);
    g_pred16(15,15) = top_pred_pels(31+32);
}

_GENX_ inline void predict_intra_d45_32(vector_ref<uint1,128> top_pred_pels)
{
    vector_ref<uint1,32> part1 = top_pred_pels.select<32,1>(32);
    vector<uint1,32>     part2 = top_pred_pels.select<32,1>(48);
    vector_ref<uint1,32> part3 = top_pred_pels.select<32,1>(64);

    #pragma unroll
    for (uint2 r = 0; r < 15; r++) {
        g_pred32.row(r).select<16,1>( 0) = part1.select<16,1>(r+1);
        g_pred32.row(r).select<16,1>(16) = part2.select<16,1>(r+1);
    }
    g_pred32.row(15) = part2;
    #pragma unroll
    for (uint2 r = 0; r < 15; r++) {
        g_pred32.row(16+r).select<16,1>( 0) = part2.select<16,1>(r+1);
        g_pred32.row(16+r).select<16,1>(16) = part3.select<16,1>(r+1);
    }
    g_pred32.row(31) = part3;
}

_GENX_ inline void predict_intra_d67_8(vector_ref<uint1,32> top_pred_pels)
{
    vector<uint2,8> x = 27 * g_sequence_1_to_32.select<8,1>();
    vector<uint2,8> shift = x & 0x3f; shift >>= 1;
    vector<uint2,8> inv_shift = 32 - shift;

    matrix<uint2,8,8> p;

    p.select<2,1,8,1>(0) = filter(top_pred_pels.replicate<2,0,8,1>(1), top_pred_pels.replicate<2,0,8,1>(2), inv_shift.replicate<2,1,8,0>(0), shift.replicate<2,1,8,0>(0));
    p.select<2,1,8,1>(2) = filter(top_pred_pels.replicate<2,0,8,1>(2), top_pred_pels.replicate<2,0,8,1>(3), inv_shift.replicate<2,1,8,0>(2), shift.replicate<2,1,8,0>(2));
    p.select<2,1,8,1>(4) = filter(top_pred_pels.replicate<2,0,8,1>(3), top_pred_pels.replicate<2,0,8,1>(4), inv_shift.replicate<2,1,8,0>(4), shift.replicate<2,1,8,0>(4));
    p.select<2,1,8,1>(6) = filter(top_pred_pels.replicate<2,1,8,1>(3), top_pred_pels.replicate<2,1,8,1>(4), inv_shift.replicate<2,1,8,0>(6), shift.replicate<2,1,8,0>(6));

    p += 16;
    g_pred8 = cm_asr<uint1>(p, 5, SAT);
}

_GENX_ /*inline*/ void predict_intra_d67_16(vector_ref<uint1,96> top_pred_pels)
{
    vector<uint1,32> p = top_pred_pels.select<32,1>(32);
    g_pred16.select<2,1,16,1>( 0) = filter_and_pack_2x16(p.select<16,1>(0), p.select<16,1>(1), p.select<16,1>(0), p.select<16,1>(1), 19, 13,  5, 27);
    g_pred16.select<2,1,16,1>( 2) = filter_and_pack_2x16(p.select<16,1>(1), p.select<16,1>(2), p.select<16,1>(1), p.select<16,1>(2), 24,  8, 10, 22);
    g_pred16.select<2,1,16,1>( 4) = filter_and_pack_2x16(p.select<16,1>(2), p.select<16,1>(3), p.select<16,1>(2), p.select<16,1>(3), 29,  3, 15, 17);
    g_pred16.select<2,1,16,1>( 6) = filter_and_pack_2x16(p.select<16,1>(2), p.select<16,1>(3), p.select<16,1>(3), p.select<16,1>(4),  2, 30, 20, 12);
    g_pred16.select<2,1,16,1>( 8) = filter_and_pack_2x16(p.select<16,1>(3), p.select<16,1>(4), p.select<16,1>(4), p.select<16,1>(5),  7, 25, 25,  7);
    g_pred16.select<2,1,16,1>(10) = filter_and_pack_2x16(p.select<16,1>(4), p.select<16,1>(5), p.select<16,1>(5), p.select<16,1>(6), 12, 20, 30,  2);
    g_pred16.select<2,1,16,1>(12) = filter_and_pack_2x16(p.select<16,1>(5), p.select<16,1>(6), p.select<16,1>(5), p.select<16,1>(6), 17, 15,  3, 29);
    g_pred16.select<2,1,16,1>(14) = filter_and_pack_2x16(p.select<16,1>(6), p.select<16,1>(7), p.select<16,1>(6), p.select<16,1>(7), 22, 10,  8, 24);
}

_GENX_ /*inline*/ void predict_intra_d67_32(vector_ref<uint1,128> top_pred_pels)
{
    vector<uint1,32> lo = top_pred_pels.select<32,1>(32);
    vector<uint1,32> hi = top_pred_pels.select<32,1>(48);

    g_pred32.row( 0) = filter_and_pack_2x16(lo.select<16,1>( 0), lo.select<16,1>( 1), hi.select<16,1>( 0), hi.select<16,1>( 1), 19, 13, 19, 13);
    g_pred32.row( 1) = filter_and_pack_2x16(lo.select<16,1>( 0), lo.select<16,1>( 1), hi.select<16,1>( 0), hi.select<16,1>( 1),  5, 27,  5, 27);
    g_pred32.row( 2) = filter_and_pack_2x16(lo.select<16,1>( 1), lo.select<16,1>( 2), hi.select<16,1>( 1), hi.select<16,1>( 2), 24,  8, 24,  8);
    g_pred32.row( 3) = filter_and_pack_2x16(lo.select<16,1>( 1), lo.select<16,1>( 2), hi.select<16,1>( 1), hi.select<16,1>( 2), 10, 22, 10, 22);
    g_pred32.row( 4) = filter_and_pack_2x16(lo.select<16,1>( 2), lo.select<16,1>( 3), hi.select<16,1>( 2), hi.select<16,1>( 3), 29,  3, 29,  3);
    g_pred32.row( 5) = filter_and_pack_2x16(lo.select<16,1>( 2), lo.select<16,1>( 3), hi.select<16,1>( 2), hi.select<16,1>( 3), 15, 17, 15, 17);
    g_pred32.row( 6) = filter_and_pack_2x16(lo.select<16,1>( 2), lo.select<16,1>( 3), hi.select<16,1>( 2), hi.select<16,1>( 3),  2, 30,  2, 30);
    g_pred32.row( 7) = filter_and_pack_2x16(lo.select<16,1>( 3), lo.select<16,1>( 4), hi.select<16,1>( 3), hi.select<16,1>( 4), 20, 12, 20, 12);
    g_pred32.row( 8) = filter_and_pack_2x16(lo.select<16,1>( 3), lo.select<16,1>( 4), hi.select<16,1>( 3), hi.select<16,1>( 4),  7, 25,  7, 25);
    g_pred32.row( 9) = filter_and_pack_2x16(lo.select<16,1>( 4), lo.select<16,1>( 5), hi.select<16,1>( 4), hi.select<16,1>( 5), 25,  7, 25,  7);
    g_pred32.row(10) = filter_and_pack_2x16(lo.select<16,1>( 4), lo.select<16,1>( 5), hi.select<16,1>( 4), hi.select<16,1>( 5), 12, 20, 12, 20);
    g_pred32.row(11) = filter_and_pack_2x16(lo.select<16,1>( 5), lo.select<16,1>( 6), hi.select<16,1>( 5), hi.select<16,1>( 6), 30,  2, 30,  2);
    g_pred32.row(12) = filter_and_pack_2x16(lo.select<16,1>( 5), lo.select<16,1>( 6), hi.select<16,1>( 5), hi.select<16,1>( 6), 17, 15, 17, 15);
    g_pred32.row(13) = filter_and_pack_2x16(lo.select<16,1>( 5), lo.select<16,1>( 6), hi.select<16,1>( 5), hi.select<16,1>( 6),  3, 29,  3, 29);
    g_pred32.row(14) = filter_and_pack_2x16(lo.select<16,1>( 6), lo.select<16,1>( 7), hi.select<16,1>( 6), hi.select<16,1>( 7), 22, 10, 22, 10);
    g_pred32.row(15) = filter_and_pack_2x16(lo.select<16,1>( 6), lo.select<16,1>( 7), hi.select<16,1>( 6), hi.select<16,1>( 7),  8, 24,  8, 24);
    g_pred32.row(16) = filter_and_pack_2x16(lo.select<16,1>( 7), lo.select<16,1>( 8), hi.select<16,1>( 7), hi.select<16,1>( 8), 27,  5, 27,  5);
    g_pred32.row(17) = filter_and_pack_2x16(lo.select<16,1>( 7), lo.select<16,1>( 8), hi.select<16,1>( 7), hi.select<16,1>( 8), 13, 19, 13, 19);
    g_pred32.row(18) = filter_and_pack_2x16(lo.select<16,1>( 8), lo.select<16,1>( 9), hi.select<16,1>( 8), hi.select<16,1>( 9), 32,  0, 32,  0);
    g_pred32.row(19) = filter_and_pack_2x16(lo.select<16,1>( 8), lo.select<16,1>( 9), hi.select<16,1>( 8), hi.select<16,1>( 9), 18, 14, 18, 14);
    g_pred32.row(20) = filter_and_pack_2x16(lo.select<16,1>( 8), lo.select<16,1>( 9), hi.select<16,1>( 8), hi.select<16,1>( 9),  5, 27,  5, 27);
    g_pred32.row(21) = filter_and_pack_2x16(lo.select<16,1>( 9), lo.select<16,1>(10), hi.select<16,1>( 9), hi.select<16,1>(10), 23,  9, 23,  9);
    g_pred32.row(22) = filter_and_pack_2x16(lo.select<16,1>( 9), lo.select<16,1>(10), hi.select<16,1>( 9), hi.select<16,1>(10), 10, 22, 10, 22);
    g_pred32.row(23) = filter_and_pack_2x16(lo.select<16,1>(10), lo.select<16,1>(11), hi.select<16,1>(10), hi.select<16,1>(11), 28,  4, 28,  4);
    g_pred32.row(24) = filter_and_pack_2x16(lo.select<16,1>(10), lo.select<16,1>(11), hi.select<16,1>(10), hi.select<16,1>(11), 15, 17, 15, 17);
    g_pred32.row(25) = filter_and_pack_2x16(lo.select<16,1>(10), lo.select<16,1>(11), hi.select<16,1>(10), hi.select<16,1>(11),  1, 31,  1, 31);
    g_pred32.row(26) = filter_and_pack_2x16(lo.select<16,1>(11), lo.select<16,1>(12), hi.select<16,1>(11), hi.select<16,1>(12), 20, 12, 20, 12);
    g_pred32.row(27) = filter_and_pack_2x16(lo.select<16,1>(11), lo.select<16,1>(12), hi.select<16,1>(11), hi.select<16,1>(12),  6, 26,  6, 26);
    g_pred32.row(28) = filter_and_pack_2x16(lo.select<16,1>(12), lo.select<16,1>(13), hi.select<16,1>(12), hi.select<16,1>(13), 25,  7, 25,  7);
    g_pred32.row(29) = filter_and_pack_2x16(lo.select<16,1>(12), lo.select<16,1>(13), hi.select<16,1>(12), hi.select<16,1>(13), 11, 21, 11, 21);
    g_pred32.row(30) = filter_and_pack_2x16(lo.select<16,1>(13), lo.select<16,1>(14), hi.select<16,1>(13), hi.select<16,1>(14), 30,  2, 30,  2);
    g_pred32.row(31) = filter_and_pack_2x16(lo.select<16,1>(13), lo.select<16,1>(14), hi.select<16,1>(13), hi.select<16,1>(14), 16, 16, 16, 16);
}

_GENX_ inline void predict_intra_d113_8(matrix_ref<uint1,2,32> pred_pels)
{
    vector<uint1,16> top;
    top.select<8,1>(0) = pred_pels(TOP,0);
    top.select<8,1>(8) = pred_pels.row(TOP).select<8,1>(1);
    vector<uint1,16> left;
    left.select<8,1>(0) = pred_pels(LEFT,0);
    left.select<8,1>(8) = pred_pels.row(LEFT).select<8,1>(1);

    matrix<uint2,8,8> p;
    p.row(0) = filter(top.select<8,1>(7), top.select<8,1>(8), 14, 18);
    p.row(1) = filter(top.select<8,1>(7), top.select<8,1>(8), 27,  5);
    p.row(2) = filter(top.select<8,1>(6), top.select<8,1>(7),  9, 23);
    p.row(3) = filter(top.select<8,1>(6), top.select<8,1>(7), 22, 10);
    p.row(4) = filter(top.select<8,1>(5), top.select<8,1>(6),  4, 28);
    p.row(5) = filter(top.select<8,1>(5), top.select<8,1>(6), 17, 15);
    p.row(6) = filter(top.select<8,1>(5), top.select<8,1>(6), 31,  1);
    p.row(7) = filter(top.select<8,1>(4), top.select<8,1>(5), 12, 20);

    matrix<uint2,4,8> left_p_t;
    left_p_t.row(0) = filter(left.select<8,1>(5), left.select<8,1>(6), 12, 20);
    left_p_t.row(1) = filter(left.select<8,1>(3), left.select<8,1>(4), 23,  9);
    left_p_t.row(2) = filter(left.select<8,1>(0), left.select<8,1>(1),  3, 29);
    matrix<uint2,4,8> tmp;
    tmp.select<1,1,8,1>(0) = left_p_t.select<4,1,2,4>(0,0);
    tmp.select<1,1,8,1>(1) = left_p_t.select<4,1,2,4>(0,1);
    tmp.select<1,1,8,1>(2) = left_p_t.select<4,1,2,4>(0,2);
    tmp.select<1,1,8,1>(3) = left_p_t.select<4,1,2,4>(0,3);
    matrix<uint2,8,4> left_p;
    left_p.select<4,1,4,1>(0) = tmp.select<4,1,4,2>(0,0);
    left_p.select<4,1,4,1>(4) = tmp.select<4,1,4,2>(0,1);

    p.select<1,1,4,1>(2).merge(left_p.row(2), 0x01);
    p.select<1,1,4,1>(3).merge(left_p.row(3), 0x01);
    p.select<1,1,4,1>(4).merge(left_p.row(4), 0x03);
    p.select<1,1,4,1>(5).merge(left_p.row(5), 0x03);
    p.select<1,1,4,1>(6).merge(left_p.row(6), 0x03);
    p.select<1,1,4,1>(7).merge(left_p.row(7), 0x07);

    p += 16;
    g_pred8 = cm_asr<uint1>(p, 5, SAT);
}

_GENX_ inline void predict_intra_d113_16(matrix_ref<uint1,2,96> pred_pels)
{
    vector<uint1,32> top  = pred_pels.row(TOP ).select<32,1>(16);
    vector<uint1,32> left = pred_pels.row(LEFT).select<32,1>(16);

    g_pred16.select<2,1,16,1>(0)  = filter_and_pack_2x16(top.select<16,1>(15), top.select<16,1>(16), top.select<16,1>(15), top.select<16,1>(16), 14, 18, 27,  5);
    g_pred16.select<2,1,16,1>(2)  = filter_and_pack_2x16(top.select<16,1>(14), top.select<16,1>(15), top.select<16,1>(14), top.select<16,1>(15),  9, 23, 22, 10);
    g_pred16.select<2,1,16,1>(4)  = filter_and_pack_2x16(top.select<16,1>(13), top.select<16,1>(14), top.select<16,1>(13), top.select<16,1>(14),  4, 28, 17, 15);
    g_pred16.select<2,1,16,1>(6)  = filter_and_pack_2x16(top.select<16,1>(13), top.select<16,1>(14), top.select<16,1>(12), top.select<16,1>(13), 31,  1, 12, 20);
    g_pred16.select<2,1,16,1>(8)  = filter_and_pack_2x16(top.select<16,1>(12), top.select<16,1>(13), top.select<16,1>(11), top.select<16,1>(12), 26,  6,  7, 25);
    g_pred16.select<2,1,16,1>(10) = filter_and_pack_2x16(top.select<16,1>(11), top.select<16,1>(12), top.select<16,1>(10), top.select<16,1>(11), 21, 11,  2, 30);
    g_pred16.select<2,1,16,1>(12) = filter_and_pack_2x16(top.select<16,1>(10), top.select<16,1>(11), top.select<16,1>(10), top.select<16,1>(11), 16, 16, 29,  3);
    g_pred16.select<2,1,16,1>(14) = filter_and_pack_2x16(top.select<16,1>( 9), top.select<16,1>(10), top.select<16,1>( 9), top.select<16,1>(10), 11, 21, 24,  8);

    matrix<uint1,8,16> left_p_t;
    left_p_t.select<2,1,16,1>(0) = filter_and_pack_2x16(left.select<16,1>(13), left.select<16,1>(14), left.select<16,1>(11), left.select<16,1>(12), 12, 20, 23,  9);
    left_p_t.select<2,1,16,1>(2) = filter_and_pack_2x16(left.select<16,1>( 8), left.select<16,1>( 9), left.select<16,1>( 6), left.select<16,1>( 7),  3, 29, 14, 18);
    left_p_t.select<2,1,16,1>(4) = filter_and_pack_2x16(left.select<16,1>( 4), left.select<16,1>( 5), left.select<16,1>( 1), left.select<16,1>( 2), 26,  6,  5, 27);
    matrix<uint1,8,16> tmp;
    tmp.select<2,1,16,1>(0) = left_p_t.select<8,1,4,4>(0,0);
    tmp.select<2,1,16,1>(2) = left_p_t.select<8,1,4,4>(0,1);
    tmp.select<2,1,16,1>(4) = left_p_t.select<8,1,4,4>(0,2);
    tmp.select<2,1,16,1>(6) = left_p_t.select<8,1,4,4>(0,3);
    matrix<uint1,16,8> left_p;
    left_p.select<4,1,8,1>( 0) = tmp.select<8,1,4,4>(0,0);
    left_p.select<4,1,8,1>( 4) = tmp.select<8,1,4,4>(0,1);
    left_p.select<4,1,8,1>( 8) = tmp.select<8,1,4,4>(0,2);
    left_p.select<4,1,8,1>(12) = tmp.select<8,1,4,4>(0,3);

    g_pred16.select<1,1,8,1>( 2).merge(left_p.row( 2), 0x01);
    g_pred16.select<1,1,8,1>( 3).merge(left_p.row( 3), 0x01);
    g_pred16.select<1,1,8,1>( 4).merge(left_p.row( 4), 0x03);
    g_pred16.select<1,1,8,1>( 5).merge(left_p.row( 5), 0x03);
    g_pred16.select<1,1,8,1>( 6).merge(left_p.row( 6), 0x03);
    g_pred16.select<1,1,8,1>( 7).merge(left_p.row( 7), 0x07);
    g_pred16.select<1,1,8,1>( 8).merge(left_p.row( 8), 0x07);
    g_pred16.select<1,1,8,1>( 9).merge(left_p.row( 9), 0x0f);
    g_pred16.select<1,1,8,1>(10).merge(left_p.row(10), 0x0f);
    g_pred16.select<1,1,8,1>(11).merge(left_p.row(11), 0x1f);
    g_pred16.select<1,1,8,1>(12).merge(left_p.row(12), 0x1f);
    g_pred16.select<1,1,8,1>(13).merge(left_p.row(13), 0x1f);
    g_pred16.select<1,1,8,1>(14).merge(left_p.row(14), 0x3f);
    g_pred16.select<1,1,8,1>(15).merge(left_p.row(15), 0x3f);
}

_GENX_ /*inline*/ void predict_intra_d113_32(matrix_ref<uint1,2,128> pred_pels)
{
    vector<uint1,32> top_lo = pred_pels.row(TOP).select<32,1>(16);
    vector<uint1,32> top_hi = pred_pels.row(TOP).select<32,1>(32);

    g_pred32.row( 0) = filter_and_pack_2x16(top_lo.select<16,1>(15), top_lo.select<16,1>(16), top_hi.select<16,1>(15), top_hi.select<16,1>(16), 14, 18, 14, 18);
    g_pred32.row( 1) = filter_and_pack_2x16(top_lo.select<16,1>(15), top_lo.select<16,1>(16), top_hi.select<16,1>(15), top_hi.select<16,1>(16), 27,  5, 27,  5);
    g_pred32.row( 2) = filter_and_pack_2x16(top_lo.select<16,1>(14), top_lo.select<16,1>(15), top_hi.select<16,1>(14), top_hi.select<16,1>(15),  9, 23,  9, 23);
    g_pred32.row( 3) = filter_and_pack_2x16(top_lo.select<16,1>(14), top_lo.select<16,1>(15), top_hi.select<16,1>(14), top_hi.select<16,1>(15), 22, 10, 22, 10);
    g_pred32.row( 4) = filter_and_pack_2x16(top_lo.select<16,1>(13), top_lo.select<16,1>(14), top_hi.select<16,1>(13), top_hi.select<16,1>(14),  4, 28,  4, 28);
    g_pred32.row( 5) = filter_and_pack_2x16(top_lo.select<16,1>(13), top_lo.select<16,1>(14), top_hi.select<16,1>(13), top_hi.select<16,1>(14), 17, 15, 17, 15);
    g_pred32.row( 6) = filter_and_pack_2x16(top_lo.select<16,1>(13), top_lo.select<16,1>(14), top_hi.select<16,1>(13), top_hi.select<16,1>(14), 31,  1, 31,  1);
    g_pred32.row( 7) = filter_and_pack_2x16(top_lo.select<16,1>(12), top_lo.select<16,1>(13), top_hi.select<16,1>(12), top_hi.select<16,1>(13), 12, 20, 12, 20);
    g_pred32.row( 8) = filter_and_pack_2x16(top_lo.select<16,1>(12), top_lo.select<16,1>(13), top_hi.select<16,1>(12), top_hi.select<16,1>(13), 26,  6, 26,  6);
    g_pred32.row( 9) = filter_and_pack_2x16(top_lo.select<16,1>(11), top_lo.select<16,1>(12), top_hi.select<16,1>(11), top_hi.select<16,1>(12),  7, 25,  7, 25);
    g_pred32.row(10) = filter_and_pack_2x16(top_lo.select<16,1>(11), top_lo.select<16,1>(12), top_hi.select<16,1>(11), top_hi.select<16,1>(12), 21, 11, 21, 11);
    g_pred32.row(11) = filter_and_pack_2x16(top_lo.select<16,1>(10), top_lo.select<16,1>(11), top_hi.select<16,1>(10), top_hi.select<16,1>(11),  2, 30,  2, 30);
    g_pred32.row(12) = filter_and_pack_2x16(top_lo.select<16,1>(10), top_lo.select<16,1>(11), top_hi.select<16,1>(10), top_hi.select<16,1>(11), 16, 16, 16, 16);
    g_pred32.row(13) = filter_and_pack_2x16(top_lo.select<16,1>(10), top_lo.select<16,1>(11), top_hi.select<16,1>(10), top_hi.select<16,1>(11), 29,  3, 29,  3);
    g_pred32.row(14) = filter_and_pack_2x16(top_lo.select<16,1>( 9), top_lo.select<16,1>(10), top_hi.select<16,1>( 9), top_hi.select<16,1>(10), 11, 21, 11, 21);
    g_pred32.row(15) = filter_and_pack_2x16(top_lo.select<16,1>( 9), top_lo.select<16,1>(10), top_hi.select<16,1>( 9), top_hi.select<16,1>(10), 24,  8, 24,  8);
    g_pred32.row(16) = filter_and_pack_2x16(top_lo.select<16,1>( 8), top_lo.select<16,1>( 9), top_hi.select<16,1>( 8), top_hi.select<16,1>( 9),  6, 26,  6, 26);
    g_pred32.row(17) = filter_and_pack_2x16(top_lo.select<16,1>( 8), top_lo.select<16,1>( 9), top_hi.select<16,1>( 8), top_hi.select<16,1>( 9), 19, 13, 19, 13);
    g_pred32.row(18) = filter_and_pack_2x16(top_lo.select<16,1>( 7), top_lo.select<16,1>( 8), top_hi.select<16,1>( 7), top_hi.select<16,1>( 8),  1, 31,  1, 31);
    g_pred32.row(19) = filter_and_pack_2x16(top_lo.select<16,1>( 7), top_lo.select<16,1>( 8), top_hi.select<16,1>( 7), top_hi.select<16,1>( 8), 14, 18, 14, 18);
    g_pred32.row(20) = filter_and_pack_2x16(top_lo.select<16,1>( 7), top_lo.select<16,1>( 8), top_hi.select<16,1>( 7), top_hi.select<16,1>( 8), 28,  4, 28,  4);
    g_pred32.row(21) = filter_and_pack_2x16(top_lo.select<16,1>( 6), top_lo.select<16,1>( 7), top_hi.select<16,1>( 6), top_hi.select<16,1>( 7),  9, 23,  9, 23);
    g_pred32.row(22) = filter_and_pack_2x16(top_lo.select<16,1>( 6), top_lo.select<16,1>( 7), top_hi.select<16,1>( 6), top_hi.select<16,1>( 7), 23,  9, 23,  9);
    g_pred32.row(23) = filter_and_pack_2x16(top_lo.select<16,1>( 5), top_lo.select<16,1>( 6), top_hi.select<16,1>( 5), top_hi.select<16,1>( 6),  4, 28,  4, 28);
    g_pred32.row(24) = filter_and_pack_2x16(top_lo.select<16,1>( 5), top_lo.select<16,1>( 6), top_hi.select<16,1>( 5), top_hi.select<16,1>( 6), 18, 14, 18, 14);
    g_pred32.row(25) = filter_and_pack_2x16(top_lo.select<16,1>( 5), top_lo.select<16,1>( 6), top_hi.select<16,1>( 5), top_hi.select<16,1>( 6), 31,  1, 31,  1);
    g_pred32.row(26) = filter_and_pack_2x16(top_lo.select<16,1>( 4), top_lo.select<16,1>( 5), top_hi.select<16,1>( 4), top_hi.select<16,1>( 5), 13, 19, 13, 19);
    g_pred32.row(27) = filter_and_pack_2x16(top_lo.select<16,1>( 4), top_lo.select<16,1>( 5), top_hi.select<16,1>( 4), top_hi.select<16,1>( 5), 26,  6, 26,  6);
    g_pred32.row(28) = filter_and_pack_2x16(top_lo.select<16,1>( 3), top_lo.select<16,1>( 4), top_hi.select<16,1>( 3), top_hi.select<16,1>( 4),  8, 24,  8, 24);
    g_pred32.row(29) = filter_and_pack_2x16(top_lo.select<16,1>( 3), top_lo.select<16,1>( 4), top_hi.select<16,1>( 3), top_hi.select<16,1>( 4), 21, 11, 21, 11);
    g_pred32.row(30) = filter_and_pack_2x16(top_lo.select<16,1>( 2), top_lo.select<16,1>( 3), top_hi.select<16,1>( 2), top_hi.select<16,1>( 3),  3, 29,  3, 29);
    g_pred32.row(31) = filter_and_pack_2x16(top_lo.select<16,1>( 2), top_lo.select<16,1>( 3), top_hi.select<16,1>( 2), top_hi.select<16,1>( 3), 16, 16, 16, 16);

    vector<uint1,32> left0 = pred_pels.row(LEFT).select<32,1>(0);
    vector<uint1,32> left1 = pred_pels.row(LEFT).select<32,1>(16);
    vector<uint1,32> left2 = pred_pels.row(LEFT).select<32,1>(32);

    // top-left 16x16 of left prediction
    matrix<uint1,16,16> left_p;
    left_p.select<2,1,16,1>(0) = filter_and_pack_2x16(left1.select<16,1>(13), left1.select<16,1>(14), left1.select<16,1>(11), left1.select<16,1>(12), 12, 20, 23,  9);
    left_p.select<2,1,16,1>(2) = filter_and_pack_2x16(left1.select<16,1>( 8), left1.select<16,1>( 9), left1.select<16,1>( 6), left1.select<16,1>( 7),  3, 29, 14, 18);
    left_p.select<2,1,16,1>(4) = filter_and_pack_2x16(left1.select<16,1>( 4), left1.select<16,1>( 5), left1.select<16,1>( 1), left1.select<16,1>( 2), 26,  6,  5, 27);

    transpose_16x16(left_p);
    g_pred32.select<1,1,16,1>( 2).merge(left_p.row( 2), 0xffff >> 15);
    g_pred32.select<1,1,16,1>( 3).merge(left_p.row( 3), 0xffff >> 15);
    g_pred32.select<1,1,16,1>( 4).merge(left_p.row( 4), 0xffff >> 14);
    g_pred32.select<1,1,16,1>( 5).merge(left_p.row( 5), 0xffff >> 14);
    g_pred32.select<1,1,16,1>( 6).merge(left_p.row( 6), 0xffff >> 14);
    g_pred32.select<1,1,16,1>( 7).merge(left_p.row( 7), 0xffff >> 13);
    g_pred32.select<1,1,16,1>( 8).merge(left_p.row( 8), 0xffff >> 13);
    g_pred32.select<1,1,16,1>( 9).merge(left_p.row( 9), 0xffff >> 12);
    g_pred32.select<1,1,16,1>(10).merge(left_p.row(10), 0xffff >> 12);
    g_pred32.select<1,1,16,1>(11).merge(left_p.row(11), 0xffff >> 11);
    g_pred32.select<1,1,16,1>(12).merge(left_p.row(12), 0xffff >> 11);
    g_pred32.select<1,1,16,1>(13).merge(left_p.row(13), 0xffff >> 11);
    g_pred32.select<1,1,16,1>(14).merge(left_p.row(14), 0xffff >> 10);
    g_pred32.select<1,1,16,1>(15).merge(left_p.row(15), 0xffff >> 10);

    // bottom-left 16x16 of left prediction
    left_p.select<2,1,16,1>( 0) = filter_and_pack_2x16(left2.select<16,1>(13), left2.select<16,1>(14), left2.select<16,1>(11), left2.select<16,1>(12), 12, 20, 23,  9);
    left_p.select<2,1,16,1>( 2) = filter_and_pack_2x16(left2.select<16,1>( 8), left2.select<16,1>( 9), left2.select<16,1>( 6), left2.select<16,1>( 7),  3, 29, 14, 18);
    left_p.select<2,1,16,1>( 4) = filter_and_pack_2x16(left2.select<16,1>( 4), left2.select<16,1>( 5), left2.select<16,1>( 1), left2.select<16,1>( 2), 26,  6,  5, 27);
    left_p.select<2,1,16,1>( 6) = filter_and_pack_2x16(left1.select<16,1>(15), left1.select<16,1>(16), left1.select<16,1>(13), left1.select<16,1>(14), 17, 15, 28,  4);
    left_p.select<2,1,16,1>( 8) = filter_and_pack_2x16(left1.select<16,1>(10), left1.select<16,1>(11), left1.select<16,1>( 8), left1.select<16,1>( 9),  8, 24, 19, 13);
    left_p.select<2,1,16,1>(10) = filter_and_pack_2x16(left1.select<16,1>( 6), left1.select<16,1>( 7), left1.select<16,1>( 3), left1.select<16,1>( 4), 31,  1, 10, 22);
    left_p.select<2,1,16,1>(12) = filter_and_pack_2x16(left1.select<16,1>( 1), left1.select<16,1>( 2), left1.select<16,1>( 1), left1.select<16,1>( 2), 22, 10, 22, 10);

    transpose_16x16(left_p);
    g_pred32.select<1,1,16,1>(16).merge(left_p.row( 0), 0xffff >> 9);
    g_pred32.select<1,1,16,1>(17).merge(left_p.row( 1), 0xffff >> 9);
    g_pred32.select<1,1,16,1>(18).merge(left_p.row( 2), 0xffff >> 8);
    g_pred32.select<1,1,16,1>(19).merge(left_p.row( 3), 0xffff >> 8);
    g_pred32.select<1,1,16,1>(20).merge(left_p.row( 4), 0xffff >> 8);
    g_pred32.select<1,1,16,1>(21).merge(left_p.row( 5), 0xffff >> 7);
    g_pred32.select<1,1,16,1>(22).merge(left_p.row( 6), 0xffff >> 7);
    g_pred32.select<1,1,16,1>(23).merge(left_p.row( 7), 0xffff >> 6);
    g_pred32.select<1,1,16,1>(24).merge(left_p.row( 8), 0xffff >> 6);
    g_pred32.select<1,1,16,1>(25).merge(left_p.row( 9), 0xffff >> 6);
    g_pred32.select<1,1,16,1>(26).merge(left_p.row(10), 0xffff >> 5);
    g_pred32.select<1,1,16,1>(27).merge(left_p.row(11), 0xffff >> 5);
    g_pred32.select<1,1,16,1>(28).merge(left_p.row(12), 0xffff >> 4);
    g_pred32.select<1,1,16,1>(29).merge(left_p.row(13), 0xffff >> 4);
    g_pred32.select<1,1,16,1>(30).merge(left_p.row(14), 0xffff >> 3);
    g_pred32.select<1,1,16,1>(31).merge(left_p.row(15), 0xffff >> 3);
}

_GENX_ inline void predict_intra_d135_8(matrix_ref<uint1,2,32> pred_pels)
{
    vector<uint1,8> sequence_8_to_1 = 9 - g_sequence_1_to_32.select<8,1>();
    vector<uint1,16> left;
    vector<uint1,16> top;
    left.select<8,1>(0) = pred_pels.row(LEFT).iselect(sequence_8_to_1);  // reversed left pels
    left.select<8,1>(8) = pred_pels(LEFT,1);
    top.select<8,1>(0) = pred_pels(TOP,0);
    top.select<8,1>(8) = pred_pels.row(TOP).select<8,1>();

    matrix<uint1,8,8> tmp;
    tmp.select<2,1,8,1>(0).merge(top.replicate<2,1,8,1>(7), left.replicate<2,1,8,1>(7), 0xfffe); // pred[00123456] pred[01234567]
    tmp.select<2,1,8,1>(2).merge(top.replicate<2,1,8,1>(5), left.replicate<2,1,8,1>(5), 0xfcf8); // pred[00001234] pred[00012345]
    tmp.select<2,1,8,1>(4).merge(top.replicate<2,1,8,1>(3), left.replicate<2,1,8,1>(3), 0xf0e0); // pred[00000012] pred[00000123]
    tmp.select<2,1,8,1>(6).merge(top.replicate<2,1,8,1>(1), left.replicate<2,1,8,1>(1), 0xc080); // pred[00000000] pred[00000001]

    g_pred8.select<4,2,8,1>(0) = tmp.select<4,2,8,1>(1);
    g_pred8.select<4,2,8,1>(1) = tmp.select<4,2,8,1>(0);
}

_GENX_ inline void predict_intra_d135_16(matrix_ref<uint1,2,96> pred_pels)
{
    vector<uint1,16> sequence_46_to_31 = 47 - g_sequence_1_to_32.select<16,1>();
    vector<uint1,32> pred;
    pred.select<16,1>( 0) = pred_pels.row(LEFT).iselect(sequence_46_to_31);
    pred.select<16,1>(16) = pred_pels.row(TOP).select<16,1>(32);
    #pragma unroll
    for (uint2 r = 0; r < 16; r++)
        g_pred16.row(r) = pred.select<16,1>(15 - r);
}

_GENX_ inline void predict_intra_d135_32(matrix_ref<uint1,2,128> pred_pels)
{
    vector_ref<uint1,32> top_pels = pred_pels.row(TOP).select<32,1>(32);

    #pragma unroll
    for (uint2 r = 0; r < 16; r++)
        g_pred32.row(r).select<16,1>(16) = top_pels.select<16,1>(15 - r);

    vector<uint1,32> sequence_62_to_31 = 63 - g_sequence_1_to_32;
    vector<uint1,32> reversed_left_pels = pred_pels.row(LEFT).iselect(sequence_62_to_31);

    #pragma unroll
    for (uint2 r = 0; r < 16; r++)
        g_pred32.row(16+r).select<16,1>(0) = reversed_left_pels.select<16,1>(15 - r);

    vector<uint1,32> mixed_pels;
    mixed_pels.select<16,1>( 0) = reversed_left_pels.select<16,1>(16);
    mixed_pels.select<16,1>(16) = top_pels.select<16,1>(0);

    #pragma unroll
    for (uint2 r = 0; r < 16; r++) {
        g_pred32.row(   r).select<16,1>( 0) = mixed_pels.select<16,1>(15 - r);
        g_pred32.row(16+r).select<16,1>(16) = mixed_pels.select<16,1>(15 - r);
    }
}

_GENX_ inline void predict_intra_d157_8(matrix_ref<uint1,2,32> pred_pels)
{
    vector<uint1,16> top;
    top.select<8,1>(0) = pred_pels(TOP,0);
    top.select<8,1>(8) = pred_pels.row(TOP).select<8,1>(1);
    vector<uint1,16> left;
    left.select<8,1>(0) = pred_pels(LEFT,0);
    left.select<8,1>(8) = pred_pels.row(LEFT).select<8,1>(1);

    matrix<uint2,8,8> left_p_t;
    left_p_t.row(0) = filter(left.select<8,1>(7), left.select<8,1>(8), 14, 18);
    left_p_t.row(1) = filter(left.select<8,1>(7), left.select<8,1>(8), 27,  5);
    left_p_t.row(2) = filter(left.select<8,1>(6), left.select<8,1>(7),  9, 23);
    left_p_t.row(3) = filter(left.select<8,1>(6), left.select<8,1>(7), 22, 10);
    left_p_t.row(4) = filter(left.select<8,1>(5), left.select<8,1>(6),  4, 28);
    left_p_t.row(5) = filter(left.select<8,1>(5), left.select<8,1>(6), 17, 15);
    left_p_t.row(6) = filter(left.select<8,1>(5), left.select<8,1>(6), 31,  1);
    left_p_t.row(7) = filter(left.select<8,1>(4), left.select<8,1>(5), 12, 20);
    matrix<uint2,8,8> tmp;
    tmp.select<2,1,8,1>(0) = left_p_t.select<8,1,2,4>(0,0);
    tmp.select<2,1,8,1>(2) = left_p_t.select<8,1,2,4>(0,1);
    tmp.select<2,1,8,1>(4) = left_p_t.select<8,1,2,4>(0,2);
    tmp.select<2,1,8,1>(6) = left_p_t.select<8,1,2,4>(0,3);
    matrix<uint2,8,8> p;
    p.select<4,1,8,1>(0) = tmp.select<8,1,4,2>(0,0);
    p.select<4,1,8,1>(4) = tmp.select<8,1,4,2>(0,1);

    matrix<uint2,3,8> top_p;
    top_p.row(0) = filter(top.select<8,1>(5), top.select<8,1>(6), 12, 20);
    top_p.row(1) = filter(top.select<8,1>(3), top.select<8,1>(4), 23,  9);
    top_p.row(2) = filter(top.select<8,1>(0), top.select<8,1>(1),  3, 29);

    p.row(0).merge(top_p.row(0), 0xffff << 2);
    p.row(1).merge(top_p.row(1), 0xffff << 4);
    p.row(2).merge(top_p.row(2), 0xffff << 7);

    p += 16;
    g_pred8 = cm_asr<uint1>(p, 5, SAT);
}

_GENX_ inline void predict_intra_d157_16(matrix_ref<uint1,2,96> pred_pels)
{
    vector<uint1,32> top  = pred_pels.row(TOP ).select<32,1>(16);
    vector<uint1,32> left = pred_pels.row(LEFT).select<32,1>(16);

    matrix<uint1,16,16> left_p_t;
    left_p_t.select<2,1,16,1>(0)  = filter_and_pack_2x16(left.select<16,1>(15), left.select<16,1>(16), left.select<16,1>(15), left.select<16,1>(16), 14, 18, 27,  5);
    left_p_t.select<2,1,16,1>(2)  = filter_and_pack_2x16(left.select<16,1>(14), left.select<16,1>(15), left.select<16,1>(14), left.select<16,1>(15),  9, 23, 22, 10);
    left_p_t.select<2,1,16,1>(4)  = filter_and_pack_2x16(left.select<16,1>(13), left.select<16,1>(14), left.select<16,1>(13), left.select<16,1>(14),  4, 28, 17, 15);
    left_p_t.select<2,1,16,1>(6)  = filter_and_pack_2x16(left.select<16,1>(13), left.select<16,1>(14), left.select<16,1>(12), left.select<16,1>(13), 31,  1, 12, 20);
    left_p_t.select<2,1,16,1>(8)  = filter_and_pack_2x16(left.select<16,1>(12), left.select<16,1>(13), left.select<16,1>(11), left.select<16,1>(12), 26,  6,  7, 25);
    left_p_t.select<2,1,16,1>(10) = filter_and_pack_2x16(left.select<16,1>(11), left.select<16,1>(12), left.select<16,1>(10), left.select<16,1>(11), 21, 11,  2, 30);
    left_p_t.select<2,1,16,1>(12) = filter_and_pack_2x16(left.select<16,1>(10), left.select<16,1>(11), left.select<16,1>(10), left.select<16,1>(11), 16, 16, 29,  3);
    left_p_t.select<2,1,16,1>(14) = filter_and_pack_2x16(left.select<16,1>( 9), left.select<16,1>(10), left.select<16,1>( 9), left.select<16,1>(10), 11, 21, 24,  8);
    matrix<uint1,16,16> tmp;
    tmp.select<4,1,16,1>( 0) = left_p_t.select<16,1,4,4>(0,0);
    tmp.select<4,1,16,1>( 4) = left_p_t.select<16,1,4,4>(0,1);
    tmp.select<4,1,16,1>( 8) = left_p_t.select<16,1,4,4>(0,2);
    tmp.select<4,1,16,1>(12) = left_p_t.select<16,1,4,4>(0,3);

    g_pred16.select<4,1,16,1>( 0) = tmp.select<16,1,4,4>(0,0);
    g_pred16.select<4,1,16,1>( 4) = tmp.select<16,1,4,4>(0,1);
    g_pred16.select<4,1,16,1>( 8) = tmp.select<16,1,4,4>(0,2);
    g_pred16.select<4,1,16,1>(12) = tmp.select<16,1,4,4>(0,3);

    matrix<uint1,2,16> top_p;
    top_p = filter_and_pack_2x16(top.select<16,1>(13), top.select<16,1>(14), top.select<16,1>(11), top.select<16,1>(12), 12, 20, 23,  9);
    g_pred16.row(0).merge(top_p.row(0), 0xffff << 2);
    g_pred16.row(1).merge(top_p.row(1), 0xffff << 4);

    top_p = filter_and_pack_2x16(top.select<16,1>( 8), top.select<16,1>( 9), top.select<16,1>( 6), top.select<16,1>( 7),  3, 29, 14, 18);
    g_pred16.row(2).merge(top_p.row(0), 0xffff << 7);
    g_pred16.row(3).merge(top_p.row(1), 0xffff << 9);

    top_p = filter_and_pack_2x16(top.select<16,1>( 4), top.select<16,1>( 5), top.select<16,1>( 1), top.select<16,1>( 2), 26,  6,  5, 27);
    g_pred16.row(4).merge(top_p.row(0), 0xffff << 11);
    g_pred16.row(5).merge(top_p.row(1), 0xffff << 14);
}

_GENX_ inline void predict_intra_d157_32(matrix_ref<uint1,2,128> pred_pels)
{
    vector<uint1,32>     left_lo = pred_pels.row(LEFT).select<32,1>(16);
    vector_ref<uint1,32> left_hi = pred_pels.row(LEFT).select<32,1>(32);

    g_pred32.row( 0) = filter_and_pack_2x16(left_lo.select<16,1>(15), left_lo.select<16,1>(16), left_hi.select<16,1>(15), left_hi.select<16,1>(16), 14, 18, 14, 18);
    g_pred32.row( 1) = filter_and_pack_2x16(left_lo.select<16,1>(15), left_lo.select<16,1>(16), left_hi.select<16,1>(15), left_hi.select<16,1>(16), 27,  5, 27,  5);
    g_pred32.row( 2) = filter_and_pack_2x16(left_lo.select<16,1>(14), left_lo.select<16,1>(15), left_hi.select<16,1>(14), left_hi.select<16,1>(15),  9, 23,  9, 23);
    g_pred32.row( 3) = filter_and_pack_2x16(left_lo.select<16,1>(14), left_lo.select<16,1>(15), left_hi.select<16,1>(14), left_hi.select<16,1>(15), 22, 10, 22, 10);
    g_pred32.row( 4) = filter_and_pack_2x16(left_lo.select<16,1>(13), left_lo.select<16,1>(14), left_hi.select<16,1>(13), left_hi.select<16,1>(14),  4, 28,  4, 28);
    g_pred32.row( 5) = filter_and_pack_2x16(left_lo.select<16,1>(13), left_lo.select<16,1>(14), left_hi.select<16,1>(13), left_hi.select<16,1>(14), 17, 15, 17, 15);
    g_pred32.row( 6) = filter_and_pack_2x16(left_lo.select<16,1>(13), left_lo.select<16,1>(14), left_hi.select<16,1>(13), left_hi.select<16,1>(14), 31,  1, 31,  1);
    g_pred32.row( 7) = filter_and_pack_2x16(left_lo.select<16,1>(12), left_lo.select<16,1>(13), left_hi.select<16,1>(12), left_hi.select<16,1>(13), 12, 20, 12, 20);
    g_pred32.row( 8) = filter_and_pack_2x16(left_lo.select<16,1>(12), left_lo.select<16,1>(13), left_hi.select<16,1>(12), left_hi.select<16,1>(13), 26,  6, 26,  6);
    g_pred32.row( 9) = filter_and_pack_2x16(left_lo.select<16,1>(11), left_lo.select<16,1>(12), left_hi.select<16,1>(11), left_hi.select<16,1>(12),  7, 25,  7, 25);
    g_pred32.row(10) = filter_and_pack_2x16(left_lo.select<16,1>(11), left_lo.select<16,1>(12), left_hi.select<16,1>(11), left_hi.select<16,1>(12), 21, 11, 21, 11);
    g_pred32.row(11) = filter_and_pack_2x16(left_lo.select<16,1>(10), left_lo.select<16,1>(11), left_hi.select<16,1>(10), left_hi.select<16,1>(11),  2, 30,  2, 30);
    g_pred32.row(12) = filter_and_pack_2x16(left_lo.select<16,1>(10), left_lo.select<16,1>(11), left_hi.select<16,1>(10), left_hi.select<16,1>(11), 16, 16, 16, 16);
    g_pred32.row(13) = filter_and_pack_2x16(left_lo.select<16,1>(10), left_lo.select<16,1>(11), left_hi.select<16,1>(10), left_hi.select<16,1>(11), 29,  3, 29,  3);
    g_pred32.row(14) = filter_and_pack_2x16(left_lo.select<16,1>( 9), left_lo.select<16,1>(10), left_hi.select<16,1>( 9), left_hi.select<16,1>(10), 11, 21, 11, 21);
    g_pred32.row(15) = filter_and_pack_2x16(left_lo.select<16,1>( 9), left_lo.select<16,1>(10), left_hi.select<16,1>( 9), left_hi.select<16,1>(10), 24,  8, 24,  8);
    g_pred32.row(16) = filter_and_pack_2x16(left_lo.select<16,1>( 8), left_lo.select<16,1>( 9), left_hi.select<16,1>( 8), left_hi.select<16,1>( 9),  6, 26,  6, 26);
    g_pred32.row(17) = filter_and_pack_2x16(left_lo.select<16,1>( 8), left_lo.select<16,1>( 9), left_hi.select<16,1>( 8), left_hi.select<16,1>( 9), 19, 13, 19, 13);
    g_pred32.row(18) = filter_and_pack_2x16(left_lo.select<16,1>( 7), left_lo.select<16,1>( 8), left_hi.select<16,1>( 7), left_hi.select<16,1>( 8),  1, 31,  1, 31);
    g_pred32.row(19) = filter_and_pack_2x16(left_lo.select<16,1>( 7), left_lo.select<16,1>( 8), left_hi.select<16,1>( 7), left_hi.select<16,1>( 8), 14, 18, 14, 18);
    g_pred32.row(20) = filter_and_pack_2x16(left_lo.select<16,1>( 7), left_lo.select<16,1>( 8), left_hi.select<16,1>( 7), left_hi.select<16,1>( 8), 28,  4, 28,  4);
    g_pred32.row(21) = filter_and_pack_2x16(left_lo.select<16,1>( 6), left_lo.select<16,1>( 7), left_hi.select<16,1>( 6), left_hi.select<16,1>( 7),  9, 23,  9, 23);
    g_pred32.row(22) = filter_and_pack_2x16(left_lo.select<16,1>( 6), left_lo.select<16,1>( 7), left_hi.select<16,1>( 6), left_hi.select<16,1>( 7), 23,  9, 23,  9);
    g_pred32.row(23) = filter_and_pack_2x16(left_lo.select<16,1>( 5), left_lo.select<16,1>( 6), left_hi.select<16,1>( 5), left_hi.select<16,1>( 6),  4, 28,  4, 28);
    g_pred32.row(24) = filter_and_pack_2x16(left_lo.select<16,1>( 5), left_lo.select<16,1>( 6), left_hi.select<16,1>( 5), left_hi.select<16,1>( 6), 18, 14, 18, 14);
    g_pred32.row(25) = filter_and_pack_2x16(left_lo.select<16,1>( 5), left_lo.select<16,1>( 6), left_hi.select<16,1>( 5), left_hi.select<16,1>( 6), 31,  1, 31,  1);
    g_pred32.row(26) = filter_and_pack_2x16(left_lo.select<16,1>( 4), left_lo.select<16,1>( 5), left_hi.select<16,1>( 4), left_hi.select<16,1>( 5), 13, 19, 13, 19);
    g_pred32.row(27) = filter_and_pack_2x16(left_lo.select<16,1>( 4), left_lo.select<16,1>( 5), left_hi.select<16,1>( 4), left_hi.select<16,1>( 5), 26,  6, 26,  6);
    g_pred32.row(28) = filter_and_pack_2x16(left_lo.select<16,1>( 3), left_lo.select<16,1>( 4), left_hi.select<16,1>( 3), left_hi.select<16,1>( 4),  8, 24,  8, 24);
    g_pred32.row(29) = filter_and_pack_2x16(left_lo.select<16,1>( 3), left_lo.select<16,1>( 4), left_hi.select<16,1>( 3), left_hi.select<16,1>( 4), 21, 11, 21, 11);
    g_pred32.row(30) = filter_and_pack_2x16(left_lo.select<16,1>( 2), left_lo.select<16,1>( 3), left_hi.select<16,1>( 2), left_hi.select<16,1>( 3),  3, 29,  3, 29);
    g_pred32.row(31) = filter_and_pack_2x16(left_lo.select<16,1>( 2), left_lo.select<16,1>( 3), left_hi.select<16,1>( 2), left_hi.select<16,1>( 3), 16, 16, 16, 16);

    matrix<uint1,16,16> tmp;
    transpose_16x16(g_pred32.select<16,1,16,1>(0,0));
    transpose_16x16(g_pred32.select<16,1,16,1>(16,16));
    transpose_16x16(g_pred32.select<16,1,16,1>(0,16), tmp);
    transpose_16x16(g_pred32.select<16,1,16,1>(16,0), g_pred32.select<16,1,16,1>(0,16));
    g_pred32.select<16,1,16,1>(16,0) = tmp;

    vector_ref<uint1,32> top0 = pred_pels.row(TOP).select<32,1>(30);
    vector<uint1,32>     top1 = pred_pels.row(TOP).select<32,1>(16);
    vector_ref<uint1,32> top2 = pred_pels.row(TOP).select<32,1>(32);

    vector<uint1,32> top_p;
    top_p = filter_and_pack_2x16(top1.select<16,1>(13), top1.select<16,1>(14), top2.select<16,1>(13), top2.select<16,1>(14), 12, 20, 12, 20);
    g_pred32.select<1,1,16,1>(0, 0).merge(top_p.select<16,1>(0), 0xffff << 2);
    g_pred32.select<1,1,16,1>(0,16) = top_p.select<16,1>(16);

    top_p = filter_and_pack_2x16(top1.select<16,1>(11), top1.select<16,1>(12), top2.select<16,1>(11), top2.select<16,1>(12), 23,  9, 23,  9);
    g_pred32.select<1,1,16,1>(1, 0).merge(top_p.select<16,1>(0), 0xffff << 4);
    g_pred32.select<1,1,16,1>(1,16) = top_p.select<16,1>(16);

    top_p = filter_and_pack_2x16(top1.select<16,1>( 8), top1.select<16,1>( 9), top2.select<16,1>( 8), top2.select<16,1>( 9),  3, 29,  3, 29);
    g_pred32.select<1,1,16,1>(2, 0).merge(top_p.select<16,1>(0), 0xffff << 7);
    g_pred32.select<1,1,16,1>(2,16) = top_p.select<16,1>(16);

    top_p = filter_and_pack_2x16(top1.select<16,1>( 6), top1.select<16,1>( 7), top2.select<16,1>( 6), top2.select<16,1>( 7), 14, 18, 14, 18);
    g_pred32.select<1,1,16,1>(3, 0).merge(top_p.select<16,1>(0), 0xffff <<  9);
    g_pred32.select<1,1,16,1>(3,16) = top_p.select<16,1>(16);

    top_p = filter_and_pack_2x16(top1.select<16,1>( 4), top1.select<16,1>( 5), top2.select<16,1>( 4), top2.select<16,1>( 5), 26,  6, 26,  6);
    g_pred32.select<1,1,16,1>(4, 0).merge(top_p.select<16,1>(0), 0xffff << 11);
    g_pred32.select<1,1,16,1>(4,16) = top_p.select<16,1>(16);

    top_p = filter_and_pack_2x16(top1.select<16,1>( 1), top1.select<16,1>( 2), top2.select<16,1>( 1), top2.select<16,1>( 2),  5, 27,  5, 27);
    g_pred32.select<1,1,16,1>(5, 0).merge(top_p.select<16,1>(0), 0xffff << 14);
    g_pred32.select<1,1,16,1>(5,16) = top_p.select<16,1>(16);

    // need only second half of these rows
    top_p = filter_and_pack_2x16(top1.select<16,1>(15), top1.select<16,1>(16), top1.select<16,1>(13), top1.select<16,1>(14), 17, 15, 28,  4);
    g_pred32.select<1,1,16,1>(6,16) = top_p.select<16,1>(0);
    g_pred32.select<1,1,16,1>(7,16).merge(top_p.select<16,1>(16), 0xffff << (18-16));

    top_p = filter_and_pack_2x16(top1.select<16,1>(10), top1.select<16,1>(11), top1.select<16,1>( 8), top1.select<16,1>( 9),  8, 24, 19, 13);
    g_pred32.select<1,1,16,1>(8,16).merge(top_p.select<16,1>( 0), 0xffff << (21-16));
    g_pred32.select<1,1,16,1>(9,16).merge(top_p.select<16,1>(16), 0xffff << (23-16));

    top_p = filter_and_pack_2x16(top1.select<16,1>( 6), top1.select<16,1>( 7), top1.select<16,1>( 3), top1.select<16,1>( 4), 31,  1, 10, 22);
    g_pred32.select<1,1,16,1>(10,16).merge(top_p.select<16,1>( 0), 0xffff << (25-16));
    g_pred32.select<1,1,16,1>(11,16).merge(top_p.select<16,1>(16), 0xffff << (28-16));

    top_p = filter_and_pack_2x16(top0.select<16,1>( 1), top0.select<16,1>( 2), top1.select<16,1>( 1), top1.select<16,1>( 2), 22, 10, 22, 10);
    g_pred32.select<1,1,16,1>(12,16).merge(top_p.select<16,1>(16), 0xffff << (30-16));


    //matrix<uint1,2,128> pred_pels_;
    //pred_pels_.row(TOP)  = pred_pels.row(LEFT);
    //pred_pels_.row(LEFT) = pred_pels.row(TOP);
    //predict_intra_d113_32(pred_pels_);

    //matrix<uint1,16,16> tmp;
    //transpose_16x16(g_pred32.select<16,1,16,1>(0,0));
    //transpose_16x16(g_pred32.select<16,1,16,1>(16,16));
    //transpose_16x16(g_pred32.select<16,1,16,1>(0,16), tmp);
    //transpose_16x16(g_pred32.select<16,1,16,1>(16,0), g_pred32.select<16,1,16,1>(0,16));
    //g_pred32.select<16,1,16,1>(16,0) = tmp;
}

_GENX_ inline void predict_intra_d203_8(vector_ref<uint1,32> left_pred_pels)
{
    predict_intra_d67_8(left_pred_pels);

    matrix<uint1,8,8> tmp;
    tmp.select<4,1,8,1>(0) = g_pred8.select<8,1,4,2>(0,0);
    tmp.select<4,1,8,1>(4) = g_pred8.select<8,1,4,2>(0,1);
    g_pred8.select<2,1,8,1>(0) = tmp.select<8,1,2,4>(0,0);
    g_pred8.select<2,1,8,1>(2) = tmp.select<8,1,2,4>(0,1);
    g_pred8.select<2,1,8,1>(4) = tmp.select<8,1,2,4>(0,2);
    g_pred8.select<2,1,8,1>(6) = tmp.select<8,1,2,4>(0,3);
}

_GENX_ inline void predict_intra_d203_16(vector_ref<uint1,96> left_pred_pels)
{
    predict_intra_d67_16(left_pred_pels);
    transpose_16x16(g_pred16);
}

_GENX_ inline void predict_intra_d203_32(vector_ref<uint1,128> left_pred_pels)
{
    predict_intra_d67_32(left_pred_pels);

    matrix<uint1,16,16> tmp;
    transpose_16x16(g_pred32.select<16,1,16,1>(0,0));
    transpose_16x16(g_pred32.select<16,1,16,1>(16,16));
    transpose_16x16(g_pred32.select<16,1,16,1>(0,16), tmp);
    transpose_16x16(g_pred32.select<16,1,16,1>(16,0), g_pred32.select<16,1,16,1>(0,16));
    g_pred32.select<16,1,16,1>(16,0) = tmp;
}

#define TOP_RIGHT_CORNER(mi, bsize) (mi.format<uint4>() + ((bsize >> 3) - 1)).format<uint2>()
#define BOTTOM_LEFT_CORNER(mi, bsize) (mi.format<uint4>() + (((bsize >> 3) - 1) << 16)).format<uint2>()

static const uint1 TEST_MODE_CTX[13][16] =
{
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }, //mode,0
    { 1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,1
    { 1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,2
    { 1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,3
    { 1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,4
    { 1,1,1,0,1,0,0,0,0,0,0,0,0,1,1,1 }, //mode,5
    { 1,0,1,0,1,0,0,0,0,0,0,0,0,1,1,1 }, //mode,6
    { 1,0,1,1,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,7
    { 1,1,0,1,0,0,0,1,0,0,0,0,0,1,1,1 }, //mode,8
    { 1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1 }, //mode,9
    { 1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1 }, //mode,10
    { 1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1 }, //mode,11
    { 1,1,1,0,0,0,0,0,0,1,1,1,0,1,1,1 }, //mode,12
};

_GENX_ inline void check_intra_8(SurfaceIndex SRC)
{
    vector<uint1,4+16> tmp;
    vector<uint1,16> above_row;
    vector<uint1,16> left_col;
    uint1 aboveleft_pel;
    read(SRC, g_read_loc(X) - 4, g_read_loc(Y) - 1, tmp);
    read(SRC, g_read_loc(X) - 1, g_read_loc(Y) + 0, left_col.format<uint1,16,1>());
    read(SRC, g_read_loc(X) + 0, g_read_loc(Y) + 0, g_src8);
    aboveleft_pel = tmp(3);
    above_row = tmp.select<16,1>(4);

    matrix<uint1,2,32> pred_pels = get_pred_pels_8(aboveleft_pel, above_row, left_col);

    uint4 decision;

    predict_intra_dc_8(pred_pels);
    g_best_decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][DC_PRED] + DC_PRED;

    predict_intra_v_8(pred_pels);
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][V_PRED] + V_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_h_8(pred_pels);
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][H_PRED] + H_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_d45_8(pred_pels.row(TOP));
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][D45_PRED] + D45_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_d135_8(pred_pels);
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][D135_PRED] + D135_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_d113_8(pred_pels);
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][D117_PRED] + D117_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_d157_8(pred_pels);
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][D153_PRED] + D153_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_d203_8(pred_pels.row(LEFT));
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][D207_PRED] + D207_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_d67_8(pred_pels.row(TOP));
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][D63_PRED] + D63_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_smooth_8(pred_pels);
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][SMOOTH_PRED] + SMOOTH_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    /*
    predict_intra_smooth_v_8(pred_pels);
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][SMOOTH_V_PRED] + SMOOTH_V_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_smooth_h_8(pred_pels);
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][SMOOTH_H_PRED] + SMOOTH_H_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    */
    /*
    predict_intra_paeth_8(pred_pels);
    decision = sad8() * 8192 + g_lambda * INTRA_MODE_COST[0][PAETH_PRED] + PAETH_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    */
exit:
    return;
}

_GENX_ /*inline*/ void check_intra_16(SurfaceIndex SRC)
{
    matrix<uint1, 13, 16>   test_mode(TEST_MODE_CTX);
    vector<uint1,4+32> tmp;
    vector<uint1,32> above_row;
    vector<uint1,32> left_col;
    uint1 aboveleft_pel;
    read(SRC, g_read_loc(X) - 4, g_read_loc(Y) - 1, tmp.select<4,1>(0));
    read(SRC, g_read_loc(X) + 0, g_read_loc(Y) - 1, tmp.select<32,1>(4));
    read(SRC, g_read_loc(X) - 1, g_read_loc(Y) + 0, left_col.format<uint1,32,1>());
    read(SRC, g_read_loc(X) + 0, g_read_loc(Y) + 0, g_src16);
    aboveleft_pel = tmp(3);
    above_row = tmp.select<32,1>(4);

    matrix<uint1,2,96> pred_pels = get_pred_pels_16(aboveleft_pel, above_row, left_col);

    uint4 decision;

    predict_intra_dc_16(pred_pels);
    g_best_decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][DC_PRED] + DC_PRED;

    predict_intra_v_16(pred_pels);
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][V_PRED] + V_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_h_16(pred_pels);
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][H_PRED] + H_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    if (test_mode[D45_PRED][g_best_decision & 15]) {
    predict_intra_d45_16(pred_pels.row(TOP));
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][D45_PRED] + D45_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[D135_PRED][g_best_decision & 15]) {
    predict_intra_d135_16(pred_pels);
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][D135_PRED] + D135_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[D117_PRED][g_best_decision & 15]) {
    predict_intra_d113_16(pred_pels);
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][D117_PRED] + D117_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[D153_PRED][g_best_decision & 15]) {
    predict_intra_d157_16(pred_pels);
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][D153_PRED] + D153_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[D207_PRED][g_best_decision & 15]) {
    predict_intra_d203_16(pred_pels.row(LEFT));
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][D207_PRED] + D207_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[D63_PRED][g_best_decision & 15]) {
    predict_intra_d67_16(pred_pels.row(TOP));
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][D63_PRED] + D63_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[SMOOTH_PRED][g_best_decision & 15]) {
    predict_intra_smooth_16(pred_pels);
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][SMOOTH_PRED] + SMOOTH_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[SMOOTH_V_PRED][g_best_decision & 15]) {
    predict_intra_smooth_v_16(pred_pels);
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][SMOOTH_V_PRED] + SMOOTH_V_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[SMOOTH_H_PRED][g_best_decision & 15]) {
    predict_intra_smooth_h_16(pred_pels);
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][SMOOTH_H_PRED] + SMOOTH_H_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }
    /*
    if (test_mode[PAETH_PRED][g_best_decision & 15]) {
    predict_intra_paeth_16(pred_pels);
    decision = sad16() * 8192 + g_lambda * INTRA_MODE_COST[1][PAETH_PRED] + PAETH_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }
    */
exit:
    return;
}

_GENX_ /*inline*/ void check_intra_32(SurfaceIndex SRC)
{
    matrix<uint1, 13, 16>   test_mode(TEST_MODE_CTX);
    vector<uint1,4+64> tmp;
    vector<uint1,64> above_row;
    vector<uint1,64> left_col;
    uint1 aboveleft_pel;
    read(SRC, g_read_loc(X) - 4, g_read_loc(Y) - 1, tmp.select<4,1>(0));
    read(SRC, g_read_loc(X) + 0, g_read_loc(Y) - 1, tmp.select<32,1>(4));
    read(SRC, g_read_loc(X) +32, g_read_loc(Y) - 1, tmp.select<32,1>(36));
    read(SRC, g_read_loc(X) - 1, g_read_loc(Y) + 0, left_col.format<uint1,64,1>());
    read(SRC, g_read_loc(X) + 0, g_read_loc(Y) + 0, g_src32.select<8,1,32,1>(0));
    read(SRC, g_read_loc(X) + 0, g_read_loc(Y) + 8, g_src32.select<8,1,32,1>(8));
    read(SRC, g_read_loc(X) + 0, g_read_loc(Y) +16, g_src32.select<8,1,32,1>(16));
    read(SRC, g_read_loc(X) + 0, g_read_loc(Y) +24, g_src32.select<8,1,32,1>(24));
    aboveleft_pel = tmp(3);
    above_row = tmp.select<64,1>(4);

    matrix<uint1,2,128> pred_pels = get_pred_pels_32(aboveleft_pel, above_row, left_col);

    uint4 decision;

    predict_intra_dc_32(pred_pels);
    g_best_decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][DC_PRED] + DC_PRED;

    predict_intra_v_32(pred_pels);
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][V_PRED] + V_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    predict_intra_h_32(pred_pels);
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][H_PRED] + H_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);

    if (test_mode[D45_PRED][g_best_decision & 15]) {
    predict_intra_d45_32(pred_pels.row(TOP));
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][D45_PRED] + D45_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[D135_PRED][g_best_decision & 15]) {
    predict_intra_d135_32(pred_pels);
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][D135_PRED] + D135_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[D117_PRED][g_best_decision & 15]) {
    predict_intra_d113_32(pred_pels);
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][D117_PRED] + D117_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[D153_PRED][g_best_decision & 15]) {
    predict_intra_d157_32(pred_pels);
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][D153_PRED] + D153_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[D207_PRED][g_best_decision & 15]) {
    predict_intra_d203_32(pred_pels.row(LEFT));
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][D207_PRED] + D207_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[D63_PRED][g_best_decision & 15]) {
    predict_intra_d67_32(pred_pels.row(TOP));
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][D63_PRED] + D63_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[SMOOTH_PRED][g_best_decision & 15]) {
    predict_intra_smooth_32(pred_pels);
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][SMOOTH_PRED] + SMOOTH_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[SMOOTH_V_PRED][g_best_decision & 15]) {
    predict_intra_smooth_v_32(pred_pels);
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][SMOOTH_V_PRED] + SMOOTH_V_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }

    if (test_mode[SMOOTH_H_PRED][g_best_decision & 15]) {
    predict_intra_smooth_h_32(pred_pels);
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][SMOOTH_H_PRED] + SMOOTH_H_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }
    /*
    if (test_mode[PAETH_PRED][g_best_decision & 15]) {
    predict_intra_paeth_32(pred_pels);
    decision = sad32() * 8192 + g_lambda * INTRA_MODE_COST[2][PAETH_PRED] + PAETH_PRED;
    if (g_early_exit & (decision > (g_best_decision >> 1) * 3)) goto exit;
    g_best_decision = cm_min<uint4>(g_best_decision, decision);
    }
    */
exit:
    return;
}

extern "C" _GENX_MAIN_ void CheckIntra(SurfaceIndex SRC, SurfaceIndex MODE_INFO, uint mi_cols, uint mi_rows, uint lambda, uint early_exit)
{
    g_early_exit = early_exit;
    g_lambda = lambda << 4;

    cmtl::cm_vector_assign<uint1,32>(g_sequence_1_to_32, 1, 1);
    g_weights_8  = vector<uint1,8>(SMOOTH_WEIGHTS_8);
    g_weights_16 = vector<uint1,16>(SMOOTH_WEIGHTS_16);
    g_weights_32 = vector<uint1,32>(SMOOTH_WEIGHTS_32);
    g_inv_weights_8  = 256 - g_weights_8;
    g_inv_weights_16 = 256 - g_weights_16;
    g_inv_weights_32 = 256 - g_weights_32;

    g_top_right = vector<uint1,8>(HAS_TOP_RIGHT);
    g_bottom_left = vector<uint1,8>(HAS_BOTTOM_LEFT);

    //g_angle_to_dx = vector<uint2,56>(ANGLE_TO_DX);
    //g_angle_to_dy = vector<uint2,56>(ANGLE_TO_DY);

    vector<uint1,16> scan_z2r(SCAN_Z2R);

    vector<uint2,2> mi_wh;
    mi_wh(COL) = mi_cols;
    mi_wh(ROW) = mi_rows;

    vector<uint2,2> omi;
    omi(COL) = get_thread_origin_x();
    omi(ROW) = get_thread_origin_y();
    omi *= 4;

    uint num8x8;
    for (uint2 i = 0; i < 16; i += num8x8) {
        uint1 raster = scan_z2r[i];

        vector<uint2,2> mi;
        mi(X) = omi(X) + (raster & 3);
        mi(Y) = omi(Y) + (raster >> 2);

        mode_info mode_info;
        read(MODE_INFO, mi(X) * SIZEOF_MODE_INFO, mi(Y), mode_info);

        uint1 sbtype = get_sb_type(mode_info);
        if (sbtype == BLOCK_64X64)
            return;

        num8x8 = 1 << ((sbtype >> 2) << 1);

        uint1 mode = get_mode(mode_info);
        if (mode == OUT_OF_PIC)
            continue;

        uint4 intra_info = get_intra_info(mode_info) & 0xf;
        if (intra_info >= AV1_INTRA_MODES)
            continue;

        const uint2 block_size = 8 << (sbtype >> 2);
        vector<uint2,2> num_pix_to_the_border;
        num_pix_to_the_border = mi_wh - mi;
        num_pix_to_the_border *= 8;
        num_pix_to_the_border -= block_size;
        num_pix_to_the_border = cm_min<uint2>(block_size, num_pix_to_the_border);

        g_availability.select<2,1>(LEFT)   = mi > 0;
        g_availability.select<1,1>(RIGHT)  = has(g_top_right, TOP_RIGHT_CORNER(mi, block_size));
        g_availability.select<1,1>(BOTTOM) = has(g_bottom_left, BOTTOM_LEFT_CORNER(mi, block_size));
        g_availability.select<2,1>(RIGHT) *= num_pix_to_the_border;

        g_read_loc = mi * 8;

        if (sbtype == BLOCK_8X8) {
            check_intra_8(SRC);
        } else if (sbtype == BLOCK_16X16) {
            check_intra_16(SRC);
        } else {
            assert_emu(sbtype == BLOCK_32X32);
            check_intra_32(SRC);
        }

        write(MODE_INFO, mi(COL) * SIZEOF_MODE_INFO + 28, mi(ROW), matrix<uint4,1,1>(g_best_decision));
    }
}
