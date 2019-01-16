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
//#include <cm/genx_vme.h>

#define JOIN_MI_INLOOP
#define SINGLE_SIDED_COMPOUND

#define MVPRED_FAR (0)
#define READ_COST_COEFF_BR_ACC 1
#define READ_COST_COEFF_BASE   1

#define ENABLE_DEBUG_PRINTF 0
#if ENABLE_DEBUG_PRINTF
  #define debug_printf printf
#else
  #define debug_printf(...) (void)0
#endif

#if CMRT_EMU
  #define assert_emu assert
#else
  #define assert_emu(...) (void)0
#endif


static const uint2 SB_SIZE = 64;
static const uint2 MI_SIZE = 8;
static const uint2 MAX_REF_MV_STACK_SIZE = 8;
static const uint2 INDEX_SHIFT = 8; // 3 bits for initial index when sorting weights
static const uint2 REF_CAT_LEVEL = 640 * INDEX_SHIFT;
static const int2 MV_BORDER_AV1 = 128;
static const uint SCRATCH = 6;
static const uint2 SIZEOF_MODE_INFO = 32;
static const uint1 REFMV_OFFSET    = 4;
static const uint1 COMP_NEWMV_CTXS = 5;
static const uint1 MAX_COMP_NEWMV_CTX = COMP_NEWMV_CTXS - 1;

enum {
    X = 0,
    Y = 1
};

enum {
    NEW_MV_DATA_SMALL_SIZE = 8,
    NEW_MV_DATA_LARGE_SIZE = 64
};

enum {
    TILE_WIDTH_SB  = 2,
    TILE_HEIGHT_SB = 1
};

enum {
    TILE_WIDTH_MI  = TILE_WIDTH_SB  * SB_SIZE / MI_SIZE,
    TILE_HEIGHT_MI = TILE_HEIGHT_SB * SB_SIZE / MI_SIZE
};

enum {
    ABOVE_CTX_OFFSET = 0,
    ABOVE_CTX_SIZE   = TILE_WIDTH_MI,
    LEFT_CTX_OFFSET  = ABOVE_CTX_SIZE,
    LEFT_CTX_SIZE    = SB_SIZE / MI_SIZE,
    TXB_CTX_SIZE     = ABOVE_CTX_SIZE + LEFT_CTX_SIZE,
};

enum SplitIcraDecisionMode
{
    SPLIT_NONE,
    SPLIT_TRY_EE,
    SPLIT_TRY_NE,
    SPLIT_MUST
};


enum {
    BLOCK_4X4   = 0,
    BLOCK_8X8   = 3,
    BLOCK_16X16 = 6,
    BLOCK_32X32 = 9,
    BLOCK_64X64 = 12
};

enum {
    SIZE8  = 0,
    SIZE16 = 1,
    SIZE32 = 2,
    SIZE64 = 3,
};

enum {
    LAST = 0,
    GOLD = 1,
    ALTR = 2,
    COMP = 3,
    NONE = 255,
};

enum {
    NEARESTMV  = 13,
    NEARMV     = 14,
    ZEROMV     = 15,
    NEWMV      = 16,
    OUT_OF_PIC = 255,
};

enum {
    MODE_IDX_NEARESTMV = 0,
    MODE_IDX_NEARMV    = 1,
    MODE_IDX_NEARMV1   = 2,
    MODE_IDX_NEARMV2   = 3,
    MODE_IDX_ZEROMV    = 4,
    MODE_IDX_NEWMV     = 5,
    MODE_IDX_NEWMV1    = 6,
    MODE_IDX_NEWMV2    = 7,
};

enum {
    SCAN_ROW = 1,
    SCAN_COL = 2
};

static const uint2 BITS_IS_INTER = 18;

static const uint2 COEFF_CONTEXT_MASK = 63;

static const int4 COEF_SSE_SHIFT = 6;

static const int2 cospi_4_64  = 16069;
static const int2 cospi_8_64  = 15137;
static const int2 cospi_12_64 = 13623;
static const int2 cospi_16_64 = 11585;
static const int2 cospi_20_64 = 9102;
static const int2 cospi_24_64 = 6270;
static const int2 cospi_28_64 = 3196;

static const uint1 SEQUENCE_7_TO_0[8] = { 7, 6, 5, 4, 3, 2, 1, 0 };

static const uint1 SCAN[64] = {
    0,  1,  8,  16, 9,  2,  3,  10,
    17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

static const uint1 BASE_CTX_OFFSET_00_31[32] = {
     0,  1,  6,  6, 11, 11, 11, 11,
     1,  6,  6, 11, 11, 11, 11, 11,
     6,  6, 11, 11, 11, 11, 11, 11,
     6, 11, 11, 11, 11, 11, 11, 11,
};

static const uint1 BR_CTX_OFFSET_00_15[16] = {
      0,  7, 14, 14, 14, 14, 14, 14,
      7,  7, 14, 14, 14, 14, 14, 14
};

static const int1 INTERP_FILTERS_0Q[4] = { /*0, 0,*/  0, 64,  0,  0, /*0, 0*/ };
static const int1 INTERP_FILTERS_1Q[4] = { /*0, 1,*/ -7, 55, 19, -5, /*1, 0*/ };
static const int1 INTERP_FILTERS_2Q[4] = { /*0, 1,*/ -7, 38, 38, -7, /*1, 0*/ };
static const int1 INTERP_FILTERS_3Q[4] = { /*0, 1,*/ -5, 19, 55, -7, /*1, 0*/ };

static const uint1 LUT_TXB_SKIP_CTX[32] = {
    1, 2, 2, 2, 3,
    1, 4, 4, 4, 5,
    1, 4, 4, 4, 5,
    1, 4, 4, 4, 5,
    1, 4, 4, 4, 6,
};

static const uint1 HAS_TOP_RIGHT[8] = {
    0xff, // 11111111
    0x55, // 01010101
    0x77, // 01110111
    0x55, // 01010101
    0x7f, // 01111111
    0x55, // 01010101
    0x77, // 01110111
    0x55, // 01010101
};

typedef matrix    <uint1,1,SIZEOF_MODE_INFO> mode_info;
typedef matrix_ref<uint1,1,SIZEOF_MODE_INFO> mode_info_ref;

_GENX_ matrix<int1,4,4>    g_interp_filters;
_GENX_ mode_info           g_mi;
_GENX_ mode_info           g_mi_saved16;
_GENX_ mode_info           g_mi_saved32;
_GENX_ mode_info           g_mi_saved64;
_GENX_ vector<uint1,128>   g_params;
_GENX_ matrix<uint4,2,2>   g_new_mv_data;
_GENX_ uint2               g_tile_mi_row_start;
_GENX_ uint2               g_tile_mi_col_start;
_GENX_ uint2               g_tile_mi_row_end;
_GENX_ uint2               g_tile_mi_col_end;
_GENX_ vector<int2,64>     g_qcoef;
_GENX_ vector<uint1,16>    g_br_ctx_offset_00_15;
_GENX_ vector<uint1,32>    g_base_ctx_offset_00_31;
_GENX_ vector<uint1,64>    g_scan;
_GENX_ matrix<uint2,8,8>   g_zeromv_sad_8;
_GENX_ matrix<uint4,4,4>   g_zeromv_sad_16;
_GENX_ matrix<uint4,2,2>   g_zeromv_sad_32;
_GENX_ uint4               g_zeromv_sad_64;
_GENX_ vector<uint,2>      g_rd_cost;
_GENX_ uint                g_skip_cost;
_GENX_ vector<uint2,4>     g_stack_size_mask;
_GENX_ vector<uint2,4>     g_stack_size;
_GENX_ vector<uint2,4>     g_has_new_mv;
_GENX_ vector<uint2,4>     g_ref_match;
_GENX_ vector<int2,64>     g_stack;
_GENX_ vector<int2,32+4>   g_refmv;
_GENX_ matrix<uint2,4,8>   g_weights;
_GENX_ matrix<uint1,1,32>  g_cand;
_GENX_ vector<uint2,4>     g_bits_ref_frame;
_GENX_ matrix<uint2,4,8>   g_bits_mode;      // 3 refs, 4 modes
_GENX_ vector<uint1,TXB_CTX_SIZE> g_coef_ctx;
_GENX_ vector<uint2,2>     g_bits_skip;
_GENX_ vector<uint2,4>     g_bits_interp_filter;
_GENX_ vector<uint2,4>     g_bits_tx_size_and_type;
_GENX_ vector<uint1,4>     g_neighbor_refs;
_GENX_ vector<uint1,32>    g_lut_txb_skip_ctx;
_GENX_ vector<uint1,8>     g_has_top_right;
_GENX_ matrix<uint1,8,8>   g_pred8;
_GENX_ matrix<uint1,16,16> g_pred16;

#define swap_idx(A, B) { uint t = A; A = B; B = t; }

_GENX_ inline void read_param(SurfaceIndex PARAM)
{
    assert_emu(g_params.SZ <= 128);
    read(PARAM, 0, g_params);
}

_GENX_ inline void set_mv0     (mode_info_ref mi, vector<int2,2> mv) { mi.format<int2>().select<2,1>(0) = mv; }
_GENX_ inline void set_mv1     (mode_info_ref mi, vector<int2,2> mv) { mi.format<int2>().select<2,1>(2) = mv; }
_GENX_ inline void set_mv_pair (mode_info_ref mi, vector<int2,4> mv) { mi.format<int2>().select<4,1>(0) = mv; }
_GENX_ inline void zero_mv_pair(mode_info_ref mi)                    { mi.format<int2>().select<4,1>(0) = 0;  }
_GENX_ inline void set_mv0     (mode_info_ref mi, uint4 mv)          { mi.format<uint4>()[0]            = mv; }
_GENX_ inline void set_mv1     (mode_info_ref mi, uint4 mv)          { mi.format<uint4>()[1]            = mv; }
_GENX_ inline void set_ref0    (mode_info_ref mi, uint1 value)       { mi.format<uint1>()[16]           = value; }
_GENX_ inline void set_ref1    (mode_info_ref mi, uint1 value)       { mi.format<uint1>()[17]           = value; }
_GENX_ inline void set_ref_comb(mode_info_ref mi, uint1 value)       { mi.format<uint1>()[18]           = value; }
_GENX_ inline void set_sb_type (mode_info_ref mi, uint1 value)       { mi.format<uint1>()[19]           = value; }
_GENX_ inline void set_mode    (mode_info_ref mi, uint1 value)       { mi.format<uint1>()[20]           = value; }
_GENX_ inline void set_skip    (mode_info_ref mi, uint1 value)       { mi.format<uint1>()[24]           = value; }
_GENX_ inline void set_sad     (mode_info_ref mi, uint4 value)       { mi.format<uint4>()[28/4]         = value; }

_GENX_ inline void set_ref_single(mode_info_ref mi, uint1 ref) { mi.format<uint1>().select<2,2>(16) = ref; mi.format<uint1>()[17] = NONE; }
_GENX_ inline void set_ref_bidir (mode_info_ref mi) { mi.format<uint2>()[16/2] = LAST + (ALTR << 8); mi.format<uint1>()[18] = COMP; }

_GENX_ inline vector<int2,2>  get_mv0       (mode_info_ref mi) { return mi.format<uint2>().select<2,1>(0); }
_GENX_ inline vector<int2,2>  get_mv1       (mode_info_ref mi) { return mi.format<uint2>().select<2,1>(2); }
_GENX_ inline vector<int2,4>  get_mv_pair   (mode_info_ref mi) { return mi.format<uint2>().select<4,1>(0); }
_GENX_ inline uint4           get_mv0_as_int(mode_info_ref mi) { return mi.format<uint4>()[0]; }
_GENX_ inline uint4           get_mv1_as_int(mode_info_ref mi) { return mi.format<uint4>()[1]; }
_GENX_ inline uint1           get_ref0      (mode_info_ref mi) { return mi.format<uint1>()[16]; }
_GENX_ inline uint1           get_ref1      (mode_info_ref mi) { return mi.format<uint1>()[17]; }
_GENX_ inline vector<uint1,2> get_refs      (mode_info_ref mi) { return mi.format<uint1>().select<2,1>(16); }
_GENX_ inline uint1           get_ref_comb  (mode_info_ref mi) { return mi.format<uint1>()[18]; }
_GENX_ inline uint1           get_sb_type   (mode_info_ref mi) { return mi.format<uint1>()[19]; }
_GENX_ inline uint1           get_mode      (mode_info_ref mi) { return mi.format<uint1>()[20]; }
_GENX_ inline uint1           get_skip      (mode_info_ref mi) { return mi.format<uint1>()[24]; }
_GENX_ inline uint4           get_sad       (mode_info_ref mi) { return mi.format<uint4>()[28 / 4]; }

_GENX_ inline uint2           get_width     () { return g_params.format<uint2>()[0/2]; }
_GENX_ inline uint2           get_height    () { return g_params.format<uint2>()[2/2]; }
_GENX_ inline vector<uint2,2> get_wh        () { return g_params.format<uint2>().select<2,1>(0); }
_GENX_ inline float           get_lambda    () { return g_params.format<float>()[4/4]; }
_GENX_ inline vector<int2,10> get_qparam    () { return g_params.format<int2>().select<10,1>(8/2); }
_GENX_ inline uint2           get_lambda_int() { return g_params.format<uint2>()[28/2]; }
_GENX_ inline uint1           get_comp_flag () { return g_params.format<uint1>()[30]; }
#ifdef SINGLE_SIDED_COMPOUND
_GENX_ inline uint1           get_single_ref_flag() { return !g_params.format<uint1>()[30]; }
#else
_GENX_ inline uint1           get_single_ref_flag() { return g_params.format<uint1>()[31]; }
#endif


#define BLOCK0(mi, off) (mi)
#define BLOCK1(mi, off) (mi.format<uint4>() + (off <<  0)).format<uint2>()
#define BLOCK2(mi, off) (mi.format<uint4>() + (off << 16)).format<uint2>()
#define BLOCK3(mi, off) (mi.format<uint4>() + (off << 16) + off).format<uint2>()

#define TX_BLOCK(mi, xblk, yblk) ((mi).format<uint4>() + ((yblk) << 16) + (xblk)).format<uint2>()

_GENX_ inline uint4 is_outsize(vector<uint2,2> mi)
{
    return (mi >= get_wh()).any();
}

static const uint OFFSET_EOB_MULTI         = 32;
static const uint OFFSET_COEFF_EOB_DC      = OFFSET_EOB_MULTI         + 2 * 7;
static const uint OFFSET_COEFF_BASE_EOB_AC = OFFSET_COEFF_EOB_DC      + 2 * 16;
static const uint OFFSET_TXB_SKIP          = OFFSET_COEFF_BASE_EOB_AC + 2 * 3;
static const uint OFFSET_SKIP              = OFFSET_TXB_SKIP          + 2 * 14;
static const uint OFFSET_COEFF_BASE        = OFFSET_SKIP              + 2 * 6;
static const uint OFFSET_COEFF_BR_ACC      = OFFSET_COEFF_BASE        + 2 * 64;
static const uint OFFSET_REF_FRAMES        = OFFSET_COEFF_BR_ACC      + 2 * 336;
static const uint OFFSET_SINGLE_MODES      = OFFSET_REF_FRAMES        + 2 * 144;
static const uint OFFSET_COMP_MODES        = OFFSET_SINGLE_MODES      + 2 * 72;

_GENX_ inline uint2 get_cost_eob_multi(uint2 log2_eob)
{
    return g_params.format<uint2>()[OFFSET_EOB_MULTI / sizeof(uint2) + log2_eob];
}

_GENX_ inline uint2 get_cost_coeff_eob_dc(uint2 level)
{
    return g_params.format<uint2>()[OFFSET_COEFF_EOB_DC / sizeof(uint2) + level];
}

_GENX_ inline uint2 get_cost_coeff_eob_ac(uint2 level)
{
    return g_params.format<uint2>()[OFFSET_COEFF_BASE_EOB_AC / sizeof(uint2) + level];
}

_GENX_ inline uint2 get_cost_txb_skip(SurfaceIndex PARAM, uint2 ctx, uint2 value)
{
    assert_emu(ctx >= 0 && ctx <= 6);
    assert_emu(value >= 0 && value <= 1);
    return g_params.format<uint2>()[OFFSET_TXB_SKIP / sizeof(uint2) + ctx * 2 + value];
}

_GENX_ inline void get_cost_skip(SurfaceIndex PARAM, uint1 ctx)
{
    assert_emu(ctx >= 0 && ctx <= 2);
    g_bits_skip.format<uint4>() = g_params.format<uint4>()(OFFSET_SKIP / sizeof(uint4) + ctx);
}

_GENX_ inline vector<uint2,16> get_cost_coeff_br(SurfaceIndex PARAM, vector_ref<uint2,16> offsets)
{
#if READ_COST_COEFF_BR_ACC
    vector<uint2,16> cost;
    vector<uint4,16> offsets_ = offsets;
    read(CONSTANT(PARAM), OFFSET_COEFF_BR_ACC / sizeof(uint2), offsets_, cost);
    return cost;
#else // READ_COST_COEFF_BR
    return g_params.format<uint2>().iselect(OFFSET_COEFF_BR_ACC / sizeof(uint2) + offsets);
#endif // READ_COST_COEFF_BR
}

_GENX_ inline vector<uint2,16> get_cost_coeff_base(SurfaceIndex PARAM, vector_ref<uint2,16> offsets)
{
#if READ_COST_COEFF_BASE
    vector<uint2,16> cost;
    vector<uint4,16> offsets_ = offsets;
    read(CONSTANT(PARAM), OFFSET_COEFF_BASE / sizeof(uint2), offsets_, cost);
    return cost;
#else // READ_COST_COEFF_BASE
    return g_params.format<uint2>().iselect(OFFSET_COEFF_BASE / sizeof(uint2) + offsets);
#endif // READ_COST_COEFF_BASE
}

_GENX_ inline void get_cost_ref_frames(SurfaceIndex PARAM, uint1 ctx)
{
    assert_emu(ctx >= 0 && ctx < 36);
    read(DWALIGNED(PARAM), OFFSET_REF_FRAMES + ctx * 8, g_bits_ref_frame); // offset in bytes
}

_GENX_ inline matrix<uint2,4,4> get_cost_modes(SurfaceIndex PARAM, vector<uint1,4> ctx)
{
    assert_emu((ctx.select<3,1>() >= 0).all());
    assert_emu((ctx.select<3,1>() < 18).all());

    ctx.format<uint4>() <<= 2;
    ctx(2) += (OFFSET_COMP_MODES - OFFSET_SINGLE_MODES) / sizeof(uint2);  // compond modes
    ctx(3) = ctx(2);  // unused
    vector<uint1,16> offsets = ctx.replicate<4,1,4,0>();  // [0123] -> [0000 1111 2222 3333]
    offsets.format<uint4>() += 0x03020100;

    vector<uint,16> offsets_ = offsets;
    vector<uint2,16> bits;
    read(CONSTANT(PARAM), OFFSET_SINGLE_MODES / sizeof(uint2), offsets_, bits);
    return bits;
}

_GENX_ inline void write_mode_info_64(SurfaceIndex MODE_INFO, vector<uint2,2> mi)
{
    for (uint2 i = 0; i < 64; i++) {
        uint2 y = i >> 3;
        uint2 x = i & 7;
        write(MODE_INFO, (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) + y, g_mi);
    }
}

_GENX_ inline void write_mode_info_32(SurfaceIndex MODE_INFO, vector<uint2,2> mi)
{
    for (uint2 i = 0; i < 16; i++) {
        uint2 y = i >> 2;
        uint2 x = i & 3;
        write(MODE_INFO, (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) + y, g_mi);
    }
}

_GENX_ inline void write_mode_info_16(SurfaceIndex MODE_INFO, vector<uint2,2> mi)
{
    for (uint2 i = 0; i < 4; i++) {
        uint2 y = i >> 1;
        uint2 x = i & 1;
        write(MODE_INFO, (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) + y, g_mi);
    }
}

_GENX_ inline void write_mode_info_8(SurfaceIndex MODE_INFO, vector<uint2,2> mi)
{
    write(MODE_INFO, mi(X) * SIZEOF_MODE_INFO, mi(Y), g_mi);
}

template<typename RT, uint N> _GENX_ inline vector<RT,N> fdct_round_shift(vector_ref<int4,N> v)
{
    const uint4 DCT_SHIFT = 14;
    const int4 DCT_OFFSET = 1 << (DCT_SHIFT - 1);
    v += DCT_OFFSET;
    return cm_asr<RT>(v, DCT_SHIFT);
}

template<typename RT, uint R, uint C> _GENX_ inline matrix<RT,R,C> fdct_round_shift(matrix_ref<int4,R,C> v)
{
    return fdct_round_shift<RT>(v.template format<int4>());
}

template <uint R2, typename T, uint R, uint C>
_GENX_ inline matrix_ref<T,R2,C> rows(matrix_ref<T,R,C> m, ushort ioff) {
    return m.template select<R2,1,C,1>(ioff,0);
}

template <uint R2, typename T, uint R, uint C>
_GENX_ inline matrix_ref<T,R2,C> rows(matrix<T,R,C> &m, ushort ioff) {
    return m.template select<R2,1,C,1>(ioff,0);
}

_GENX_ inline void fdct8()
{
    matrix_ref<int2,8,8> diff = g_qcoef.format<int2,8,8>();
    matrix<int2,8,8> s;
    matrix<int2,4,8> x;
    matrix<int4,4,8> t;

    // stage 1
    s.row(0) = diff.row(0) + diff.row(7);
    s.row(1) = diff.row(1) + diff.row(6);
    s.row(2) = diff.row(3) + diff.row(4);
    s.row(3) = diff.row(2) + diff.row(5);
    s.row(4) = diff.row(3) - diff.row(4);
    s.row(5) = diff.row(2) - diff.row(5);
    s.row(6) = diff.row(1) - diff.row(6);
    s.row(7) = diff.row(0) - diff.row(7);

    // fdct4(step, step);
    rows<2>(x,0) = rows<2>(s,0) + rows<2>(s,2);
    rows<2>(x,2) = rows<2>(s,0) - rows<2>(s,2);
    t.row(0) = x.row(0) + x.row(1);
    t.row(1) = x.row(0) - x.row(1);
    rows<2>(t,0) *= cospi_16_64;
    t.row(2) = x.row(3) * cospi_24_64 + x.row(2) * cospi_8_64;
    t.row(3) = x.row(2) * cospi_24_64 - x.row(3) * cospi_8_64;
    t = fdct_round_shift<int4>(t.select_all());
    g_qcoef.format<int2,8,8>().row(0) = t.row(0);
    g_qcoef.format<int2,8,8>().row(4) = t.row(1);
    g_qcoef.format<int2,8,8>().row(2) = t.row(2);
    g_qcoef.format<int2,8,8>().row(6) = t.row(3);

    // Stage 2
    t.row(0) = s.row(6) - s.row(5);
    t.row(1) = s.row(6) + s.row(5);
    rows<2>(t,0) *= cospi_16_64;
    rows<2>(t,2) = fdct_round_shift<int4>(rows<2>(t,0));

    // Stage 3
    t.row(0) = s.row(4) + t.row(2);
    t.row(1) = s.row(4) - t.row(2);
    t.row(2) = s.row(7) - t.row(3);
    t.row(3) = s.row(7) + t.row(3);
    x = t;

    // Stage 4
    t.row(0) = x.row(0) * cospi_28_64 + x.row(3) *  cospi_4_64;
    t.row(1) = x.row(1) * cospi_12_64 + x.row(2) *  cospi_20_64;
    t.row(2) = x.row(2) * cospi_12_64 + x.row(1) * -cospi_20_64;
    t.row(3) = x.row(3) * cospi_28_64 + x.row(0) * -cospi_4_64;
    t = fdct_round_shift<int4>(t.format<int4>());
    g_qcoef.format<int2,8,8>().row(1) = t.row(0);
    g_qcoef.format<int2,8,8>().row(3) = t.row(2);
    g_qcoef.format<int2,8,8>().row(5) = t.row(1);
    g_qcoef.format<int2,8,8>().row(7) = t.row(3);
}

_GENX_ inline void transpose_qcoef_inplace()
{
    matrix_ref<int2,8,8> inout = g_qcoef.format<int2,8,8>();
    matrix<int2,8,8> tmp;
    tmp.select<2,1,8,1>(0,0) = inout.select<8,1,2,4>(0,0);
    tmp.select<2,1,8,1>(2,0) = inout.select<8,1,2,4>(0,1);
    tmp.select<2,1,8,1>(4,0) = inout.select<8,1,2,4>(0,2);
    tmp.select<2,1,8,1>(6,0) = inout.select<8,1,2,4>(0,3);
    inout.select<4,1,8,1>(0,0) = tmp.select<8,1,4,2>(0,0);
    inout.select<4,1,8,1>(4,0) = tmp.select<8,1,4,2>(0,1);
}

_GENX_ inline uint4 quant_dequant()
{
    const vector<int2,10> qparam = get_qparam();

    vector<int2,16> zbin  = qparam[1];
    vector<int2,16> round = qparam[3];
    vector<int2,16> quant = qparam[5];
    vector<int2,16> shift = qparam[7];
    vector<int2,16> scale = qparam[9];
    vector<uint2,16> deadzoned, coef;
    vector<int2,16> diff;
    vector<uint4,16> acc;

    acc = 0;
    #pragma unroll
    for (int i = 16; i < 64; i += 16) {
        coef = g_qcoef.select<16,1>(i);
        deadzoned = coef < zbin;
        g_qcoef.select<16,1>(i).format<uint2>() = cm_add<uint2>(g_qcoef.select<16,1>(i), round, SAT);
        g_qcoef.select<16,1>(i) += (g_qcoef.select<16,1>(i) * quant).format<uint2>().select<16,2>(1);
        g_qcoef.select<16,1>(i) = (g_qcoef.select<16,1>(i) * shift).format<uint2>().select<16,2>(1);
        g_qcoef.select<16,1>(i).merge(0, deadzoned);
        diff = coef - cm_mul<int2>(g_qcoef.select<16,1>(i) * scale, SAT);
        acc += cm_abs<uint2>(diff) * cm_abs<uint2>(diff);
    }

    zbin[0]  = qparam[0];
    round[0] = qparam[2];
    quant[0] = qparam[4];
    shift[0] = qparam[6];
    scale[0] = qparam[8];

    coef = g_qcoef.select<16,1>(0);
    deadzoned = coef < zbin;
    g_qcoef.select<16,1>(0).format<uint2>() = cm_add<uint2>(g_qcoef.select<16,1>(0), round, SAT);
    g_qcoef.select<16,1>(0) += (g_qcoef.select<16,1>(0) * quant).format<uint2>().select<16,2>(1);
    g_qcoef.select<16,1>(0) = (g_qcoef.select<16,1>(0) * shift).format<uint2>().select<16,2>(1);
    g_qcoef.select<16,1>(0).merge(0, deadzoned);
    diff = coef - cm_mul<int2>(g_qcoef.select<16,1>(0) * scale, SAT);
    acc += cm_abs<uint2>(diff) * cm_abs<uint2>(diff);

    uint4 sse = cm_sum<uint4>(acc);
    return sse >> COEF_SSE_SHIFT;
}

template <size_t N> _GENX_ inline vector<uint2,N>
    compute_br_ctx(vector<uint1,N> a, vector<uint1,N> b, vector<uint1,N> c)
{
    vector<uint4,N/4> acc = a.template format<uint4>() + b.template format<uint4>();
    acc += c.template format<uint4>();
    vector<uint2,N> br_ctx = cm_avg<uint2>(acc.template format<uint1>(), 0);
    br_ctx.merge(6, br_ctx > 6);
    return br_ctx;
}

template <size_t N> _GENX_ inline vector<uint2,N>
    compute_base_ctx(vector<uint1,N> a, vector<uint1,N> b, vector<uint1,N> c, vector<uint1,N> d, vector<uint1,N> e)
{
    vector<uint4,N/4> acc = a.template format<uint4>() + b.template format<uint4>();
    acc += c.template format<uint4>();
    acc += d.template format<uint4>();
    acc += e.template format<uint4>();
    vector<uint2,N> base_ctx = cm_avg<uint2>(acc.template format<uint1>(), 0);
    base_ctx.merge(4, base_ctx > 4);
    return base_ctx;
}

template <typename T, size_t N> _GENX_ inline vector<uint1,N> my_min(vector<T,N> x, uint1 v)
{
    vector_ref<uint1,N> x_ = cm_add<uint1>(x, 0xff - v, SAT);
    uint4 v4 = v | (v << 8) | (v << 16) | (v << 24);
    vector_ref<uint4,N/4> x4 = x_.template format<uint4>();
    return (x4 & v4).template format<uint1>();
}

template <typename T, size_t N> _GENX_ inline vector<uint1,N> my_min(vector_ref<T,N> x, uint1 v)
{
    vector_ref<uint1,N> x_ = cm_add<uint1>(x, 0xff - v, SAT);
    uint4 v4 = v | (v << 8) | (v << 16) | (v << 24);
    vector_ref<uint4,N/4> x4 = x_.template format<uint4>();
    return (x4 & v4).template format<uint1>();
}

_GENX_ inline uint2 count_non_zero_coefs()
{
    vector<uint,2> m;
    m[0] = cm_pack_mask(g_qcoef.select<32,1>(0)  != 0);
    m[1] = cm_pack_mask(g_qcoef.select<32,1>(32) != 0);
    uint2 nzc = cm_sum<uint2>(cm_cbit(m));
    return nzc;
}

_GENX_ inline void compute_contexts(vector<uint1,80> &levels, vector_ref<uint4,64> contexts)
{
    vector<uint1,64> x1, x2, xy, y1, y2, l;
    vector<uint2,64> base_ctx, br_ctx;

    x1 = levels.select<64,1>(1);
    x2 = levels.select<64,1>(2);
    y1 = levels.select<64,1>(8);
    xy = levels.select<64,1>(9);
    y2 = levels.select<64,1>(16);

    xy.format<uint2>().select<8,4>(3) &= 0xff;  // zero 7th pixel in each of 4 rows
    x1.format<uint2>().select<8,4>(3) &= 0xff;  // zero 7th pixel in each of 4 rows
    x2.format<uint2>().select<8,4>(3) = 0;      // zero 6th and 7th pixels in each of 4 rows

    br_ctx = compute_br_ctx(x1, y1, xy);
    br_ctx.select<16,1>(0)  += g_br_ctx_offset_00_15;
    br_ctx.select<48,1>(16) += 14;
    br_ctx *= 16;
    br_ctx += levels.select<64,1>(0);

    x1 = my_min(x1, 3);
    x2 = my_min(x2, 3);
    y1 = my_min(y1, 3);
    y2 = my_min(y2, 3);
    xy = my_min(xy, 3);
    l.merge(3, levels.select<64,1>(0), levels.select<64,1>(0) > 3);

    base_ctx = compute_base_ctx(x1, y1, x2, y2, xy);
    base_ctx.select<32,1>(0)  += g_base_ctx_offset_00_31;
    base_ctx.select<32,1>(32) += 11;
    base_ctx[0] = 0;
    base_ctx *= 4;
    base_ctx += l;

    contexts.format<uint2>().select<64,2>(0) = base_ctx;
    contexts.format<uint2>().select<64,2>(1) = br_ctx;
}

_GENX_ inline uint get_bits(SurfaceIndex PARAM, uint2 txb_skip_ctx, uint2 num_nz, vector_ref<uint1,1> cul_level)
{
    if (num_nz == 0) {
        cul_level = 0;
        return get_cost_txb_skip(PARAM, txb_skip_ctx, 1); // skip
    }

    uint bits = 0;
    bits += get_cost_txb_skip(PARAM, txb_skip_ctx, 0); // non skip
    bits += 512 * num_nz; // signs

    if (num_nz == 1 && g_qcoef(0) != 0) {
        const uint2 dc = g_qcoef(0);
        cul_level = cm_min<uint1>(COEFF_CONTEXT_MASK, dc);
        bits += get_cost_eob_multi(0);
        bits += get_cost_coeff_eob_dc(cm_min<uint2>(15, dc));
        return bits;
    }

    vector<uint1,80> levels;
    levels.select<64,1>() = my_min(g_qcoef.format<uint2>(), 15);
    levels.select<16,1>(64) = 0;

    cul_level = cm_min<uint1>(COEFF_CONTEXT_MASK, cm_sum<uint2>(g_qcoef, SAT));

    vector<uint4,64> contexts;
    compute_contexts(levels, contexts);

    vector<uint4,16> ctx;
    vector<uint2,16> br_ctx, base_ctx, blevel;
    uint4 lz, i;

    vector<uint2,16> bits_acc = 0;
    //#pragma unroll(4)
    for (i = 0; i < 64; i += 16) {
        vector<uint2, 16> s = g_scan.select<16,1>(i);
        ctx = contexts.iselect(s);
        br_ctx = ctx.format<uint2>().select<16,2>(1);
        base_ctx = ctx.format<uint2>().select<16,2>(0);
        blevel = base_ctx & 3;

        uint2 nz_mask = cm_pack_mask(blevel > 0);
        num_nz -= cm_cbit(nz_mask);
        lz = cm_lzd(uint4(nz_mask));
        vector<uint2,1> mask_br = uint2(0xffffffff >> lz);
        vector<uint2,1> mask_bz = mask_br >> 1;
        mask_br.merge(0xffff, num_nz > 0);
        mask_bz.merge(0xffff, num_nz > 0);
        vector<uint2,16> cost_br = get_cost_coeff_br(PARAM, br_ctx);
        vector<uint2,16> cost_bz = get_cost_coeff_base(PARAM, base_ctx);
        cost_br.merge(cost_br, 0, mask_br[0]);
        cost_bz.merge(cost_bz, 0, mask_bz[0]);
        bits_acc += cost_br;
        bits_acc += cost_bz;

        if (num_nz == 0)
            break;
    }

    const uint4 msb = 31 - lz;
    const uint4 eob = i + msb;
    bits += cm_sum<uint>(bits_acc);
    bits += get_cost_coeff_eob_ac(blevel[msb] - 1); // last coef
    bits += get_cost_eob_multi(31 - cm_lzd(eob) + 1); // eob
    return bits;
}

_GENX_ inline uint2 get_txb_skip_ctx(uint2 is8x8, vector<uint2,2> mi)
{
    if (is8x8)
        return 0;
    const uint2 top  = g_coef_ctx(ABOVE_CTX_OFFSET + mi(X) - g_tile_mi_col_start);
    const uint2 left = g_coef_ctx(LEFT_CTX_OFFSET  + (mi(Y) & 7));
    const uint2 max = cm_min<uint2>(4, top | left);
    const uint2 min = cm_min<uint2>(cm_min<uint2>(4, top), left);
    const uint2 txb_skip_ctx = g_lut_txb_skip_ctx[5 * min + max];
    return txb_skip_ctx;
}

template <int SIZE>
_GENX_ inline void set_coef_ctx(vector<uint2,2> mi, uint1 culLevel)
{
    g_coef_ctx.select<SIZE,1>(ABOVE_CTX_OFFSET + mi(X) - g_tile_mi_col_start) = culLevel;
    g_coef_ctx.select<SIZE,1>(LEFT_CTX_OFFSET  + (mi(Y) & 7)) = culLevel;
}

_GENX_ void get_rd_cost_8(SurfaceIndex PARAM, uint2 is8x8, vector<uint2,2> mi)
{
    g_skip_cost += cm_sum<uint>(cm_mul<uint2>(g_qcoef, g_qcoef));
    g_qcoef *= 4;
    fdct8();
    transpose_qcoef_inplace();
    fdct8();
    transpose_qcoef_inplace();
    g_qcoef = cm_abs<uint2>(g_qcoef) >> 1;
    g_rd_cost[0] += quant_dequant();
    const uint2 num_nz = count_non_zero_coefs();
    const uint2 txb_skip_ctx = get_txb_skip_ctx(is8x8, mi);
    vector<uint1,1> cul_level;
    g_rd_cost[1] += ((118*get_bits(PARAM, txb_skip_ctx, num_nz, cul_level))>>7);
    set_coef_ctx<1>(mi, cul_level(0));
}

template <uint2 ref>
_GENX_ void add_distinct_mv_to_stack(uint4 mv_cand, uint2 weight)
{
#if 1
    uint2 stack_size = g_stack_size[ref];
    uint2 ref_offset = 8 * ref;

    uint2 mask = cm_pack_mask<uint2>(g_stack.format<uint4>().select<8,1>(ref_offset) == mv_cand);
    uint2 idx = cm_fbl<uint>(mask);

    if (idx < stack_size) {
        g_weights.format<uint2>()(ref_offset + idx) += weight;
    } else if (stack_size < MAX_REF_MV_STACK_SIZE) {
        idx = ref_offset + stack_size;
        g_stack.format<uint4>()(idx) = mv_cand;
        g_weights.format<uint2>()(idx) = weight;
        g_stack_size[ref] = stack_size + 1;
    }
#else
    const uint2 stack_size = g_stack_size[ref];
    uint2 i = 0;
    uint2 idx = 8 * ref;
    for (; i < stack_size; ++i, ++idx) {
        if (g_stack.format<uint4>()(idx) == mv_cand) {
            g_weights.format<uint2>()(idx) += weight;
            return;
        }
    }

    if (i < MAX_REF_MV_STACK_SIZE) {
        g_stack.format<uint4>()(idx) = mv_cand;
        g_weights.format<uint2>()(idx) = weight;
        g_stack_size[ref] = stack_size + 1;
    }
#endif
}

_GENX_ void add_distinct_mvpair_to_stack(uint2 weight)
{
#if 1
    vector<uint2,8> predicate =
        (g_stack.format<uint4>().select<8,2>(16) == get_mv0_as_int(g_cand)) &
        (g_stack.format<uint4>().select<8,2>(17) == get_mv1_as_int(g_cand));
    uint2 mask = cm_pack_mask<uint2>(predicate);
    uint2 idx = cm_fbl<uint>(mask);

    if (idx < g_stack_size[2]) {
        g_weights(2, idx) += weight;
    } else if (g_stack_size[2] < MAX_REF_MV_STACK_SIZE) {
        g_stack.select<2,1>(32 + 4 * g_stack_size[2] + 0).format<uint4>() = get_mv0_as_int(g_cand);
        g_stack.select<2,1>(32 + 4 * g_stack_size[2] + 2).format<uint4>() = get_mv1_as_int(g_cand);
        g_weights(2, g_stack_size[2]) = weight;
        g_stack_size[2]++;
    }

#else
    uint2 i = 0;
    for (; i < g_stack_size(2); ++i) {
        if ((g_stack.select<4,1>(32+4*i) == get_mv_pair(g_cand)).all()) {
            g_weights(2,i) += weight;
            return;
        }
    }

    if (i < MAX_REF_MV_STACK_SIZE) {
        g_stack.select<2,1>(32+4*i+0).format<uint4>() = get_mv0_as_int(g_cand);
        g_stack.select<2,1>(32+4*i+2).format<uint4>() = get_mv1_as_int(g_cand);
        g_weights(2,i) = weight;
        g_stack_size[2]++;
    }
#endif
}

_GENX_ void add_cand(uint2 weight, uint2 row_or_col)
{
    uint2 newmv = (get_mode(g_cand) == NEWMV);

    const uint1 ref0 = get_ref0(g_cand);
    if (ref0 == LAST) {
        g_has_new_mv(0) |= newmv;
        g_ref_match(0) |= row_or_col;
        add_distinct_mv_to_stack<0>(g_cand.format<uint4>()(0), weight);
        if (get_ref1(g_cand) != NONE) {
            g_has_new_mv.select<2,1>(1) |= newmv;
            g_ref_match.select<2,1>(1) |= row_or_col;
            add_distinct_mv_to_stack<1>(g_cand.format<uint4>()(1), weight);
            add_distinct_mvpair_to_stack(weight);
        }
    } else if (ref0 != NONE) {
        g_has_new_mv(1) |= newmv;
        g_ref_match(1) |= row_or_col;
        add_distinct_mv_to_stack<1>(g_cand.format<uint4>()(0), weight);
    }
}

_GENX_ inline uint2 has_top_right(uint2 mi_x, uint2 mi_y) {
    mi_x &= 7;
    mi_y &= 7;
    uint1 byte = g_has_top_right(mi_y);
    uint1 bit = byte >> mi_x;
    return bit & 1;
}

_GENX_ inline vector<uint2,4> stable_partial_sort(vector<uint2,8> weights)
{
    vector<uint2,4> indices;

    #pragma unroll
    for (int i = 0; i < 4; i++) {
        indices[i] = cm_reduced_max<uint2>(weights);
        weights.merge(0, weights == indices[i]);
    }

    return indices;
}

_GENX_ inline matrix<uint2,4,4> stable_partial_sort3(matrix<uint2,4,8> weights)
{
    matrix<uint2,4,4> indices;

    #pragma unroll
    for (int i = 0; i < 4; i++) {
        matrix<uint2,4,4> tmp4 = cm_max<uint2>(weights.select<4,1,4,2>(0,0), weights.select<4,1,4,2>(0,1));
        matrix<uint2,4,2> tmp2 = cm_max<uint2>(tmp4.select<4,1,2,2>(0,0), tmp4.select<4,1,2,2>(0,1));
        indices.select<4,1,1,1>(0,i) = cm_max<uint2>(tmp2.select<4,1,1,1>(0,0), tmp2.select<4,1,1,1>(0,1));
        weights.row(0).merge(0, weights.row(0) == indices(0,i));
        weights.row(1).merge(0, weights.row(1) == indices(1,i));
        weights.row(2).merge(0, weights.row(2) == indices(2,i));
    }

    return indices;
}

_GENX_ inline uint1 get_cand_len(uint1 cand_log2_size8, uint1 curr_log2_size8, uint1 min_cand_log2_size)
{
    cand_log2_size8 = cm_min<uint1>(cand_log2_size8, curr_log2_size8);
    cand_log2_size8 = cm_max<uint1>(cand_log2_size8, min_cand_log2_size);
    return 1 << cand_log2_size8;
}

_GENX_ void get_mvrefs(SurfaceIndex PARAM, SurfaceIndex MODE_INFO, uint1 log2_size8, vector<uint2,2> mi)
{
    const uint1 min_cand_log2_size = cm_add<uint1>(log2_size8, -2, SAT);
    const uint1 size8 = 1 << log2_size8;

    g_stack = 0;
    g_stack_size = 0;
    g_has_new_mv = 0;
    g_ref_match = 0;
    g_weights = 0;

    uint1 ctx_ref_frame = 0;
    uint1 ctx_skip = 0;
    g_neighbor_refs = NONE;

    uint1 processed_rows = 1;
    uint1 processed_cols = 1;

    // First above row
    if (mi(Y) > g_tile_mi_row_start) {
        uint2 x = 0;
        do {
            read(MODIFIED(MODE_INFO), (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) - 1, g_cand);

            if (x == 0) {
                ctx_ref_frame = 6 * cm_add<uint1>(get_ref_comb(g_cand), 2);
                ctx_skip = get_skip(g_cand);
                g_neighbor_refs.select<2,1>(0) = get_refs(g_cand);
            }

            uint1 cand_log2_size8 = get_sb_type(g_cand) >> 2;
            uint1 len = get_cand_len(cand_log2_size8, log2_size8, min_cand_log2_size);
            x += len;

            if (cand_log2_size8 >= log2_size8) {
                processed_rows = cm_min<uint1>(cand_log2_size8 + 1, 3);
                len *= processed_rows;
            }

            add_cand(INDEX_SHIFT * 4 * len, SCAN_ROW);
        } while (x < size8);
    }

    // First left column
    if (mi(X) > g_tile_mi_col_start) {
        uint2 y = 0;
        do {
            read(MODIFIED(MODE_INFO), (mi(X) - 1) * SIZEOF_MODE_INFO, mi(Y) + y, g_cand);

            if (y == 0) {
                ctx_ref_frame += cm_add<uint1>(get_ref_comb(g_cand), 2);
                ctx_skip += get_skip(g_cand);
                g_neighbor_refs.select<2,1>(2) = get_refs(g_cand);
            }

            uint1 cand_log2_size8 = get_sb_type(g_cand) >> 2;
            uint1 len = get_cand_len(cand_log2_size8, log2_size8, min_cand_log2_size);
            y += len;

            if (cand_log2_size8 >= log2_size8) {
                processed_cols = cm_min<uint1>(cand_log2_size8 + 1, 3);
                len *= processed_cols;
            }

            add_cand(INDEX_SHIFT * 4 * len, SCAN_COL);
        } while (y < size8);
    }

    get_cost_ref_frames(PARAM, ctx_ref_frame);
    get_cost_skip(PARAM, ctx_skip);

    // Top-right
    if (has_top_right(mi(X) + size8 - 1, mi(Y)) && mi(Y) > g_tile_mi_row_start && mi(X) + size8 < g_tile_mi_col_end) {
        read(MODIFIED(MODE_INFO), (mi(X) + size8) * SIZEOF_MODE_INFO, mi(Y) - 1, g_cand);
        add_cand(INDEX_SHIFT * 4, SCAN_ROW);
    }

    vector<uint2,4> has_new_mv = g_has_new_mv;
    g_weights.merge(g_weights + REF_CAT_LEVEL, g_weights > 0);
    const vector<uint4,4> nearest_ref_match = cm_cbit(g_ref_match);

    // Above-left
    if (mi(Y) > g_tile_mi_row_start && mi(X) > g_tile_mi_col_start && MVPRED_FAR) {
        read(MODIFIED(MODE_INFO), (mi(X) - 1) * SIZEOF_MODE_INFO, mi(Y) - 1, g_cand);
        add_cand(INDEX_SHIFT * 4, SCAN_ROW);
    }

    // Second above row
    if (mi(Y) - 1 > g_tile_mi_row_start && MVPRED_FAR && processed_rows < 2) {
        uint2 x = 0;
        do {
            read(MODIFIED(MODE_INFO), (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) - 2, g_cand);

            uint1 cand_log2_size8 = get_sb_type(g_cand) >> 2;
            uint1 len = get_cand_len(cand_log2_size8, log2_size8, min_cand_log2_size);
            x += len;

            if (cand_log2_size8 >= log2_size8) {
                processed_rows = cm_min<uint1>(cand_log2_size8 + 2, 3);
                len *= (processed_rows - 1);
            }

            add_cand(INDEX_SHIFT * 4 * len, SCAN_ROW);
        } while (x < size8);
    }

    // Second left column
    if (mi(X) - 1 > g_tile_mi_col_start && MVPRED_FAR && processed_cols < 2) {
        uint2 y = 0;
        do {
            read(MODIFIED(MODE_INFO), (mi(X) - 2) * SIZEOF_MODE_INFO, mi(Y) + y, g_cand);

            uint1 cand_log2_size8 = get_sb_type(g_cand) >> 2;
            uint1 len = get_cand_len(cand_log2_size8, log2_size8, min_cand_log2_size);
            y += len;

            if (cand_log2_size8 >= log2_size8) {
                processed_cols = cm_min<uint1>(cand_log2_size8 + 2, 3);
                len *= (processed_cols - 1);
            }

            add_cand(INDEX_SHIFT * 4 * len, SCAN_COL);
        } while (y < size8);
    }

    // Third above row
    if (mi(Y) - 2 > g_tile_mi_row_start && MVPRED_FAR && processed_rows < 3) {
        uint2 x = 0;
        do {
            read(MODIFIED(MODE_INFO), (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) - 3, g_cand);

            uint1 cand_log2_size8 = get_sb_type(g_cand) >> 2;
            uint1 len = get_cand_len(cand_log2_size8, log2_size8, min_cand_log2_size);
            x += len;

            add_cand(INDEX_SHIFT * 4 * len, SCAN_ROW);
        } while (x < size8);
    }

    // Third left column
    if (mi(X) - 2 > g_tile_mi_col_start && MVPRED_FAR && processed_cols < 3) {
        uint2 y = 0;
        do {
            read(MODIFIED(MODE_INFO), (mi(X) - 3) * SIZEOF_MODE_INFO, mi(Y) + y, g_cand);

            uint1 cand_log2_size8 = get_sb_type(g_cand) >> 2;
            uint1 len = get_cand_len(cand_log2_size8,log2_size8, min_cand_log2_size);
            y += len;

            add_cand(INDEX_SHIFT * 4 * len, SCAN_COL);
        } while (y < size8);
    }

    const vector<uint4,4> ref_match = cm_cbit(g_ref_match);

    vector<uint1,4> ctx = cm_add<uint1>(ref_match * 2 + nearest_ref_match * 6, has_new_mv);

    matrix<uint2,4,4> bits = get_cost_modes(PARAM, ctx);
    bits += g_bits_ref_frame.replicate<4,1,4,0>();  // [0123] -> [0000 1111 2222 3333]

    g_bits_mode.format<uint4,4,4>().select<4,1,2,2>() = bits.format<uint4,4,2>();
    g_bits_mode.select<4,1,2,4>(0,MODE_IDX_NEARMV1) = g_bits_mode.select<4,1,2,4>(0,MODE_IDX_NEARMV);
    g_bits_mode.select<4,1,2,4>(0,MODE_IDX_NEARMV2) = g_bits_mode.select<4,1,2,4>(0,MODE_IDX_NEARMV);


    // add bit cost of drl idx
    vector<uint1,8> num_mvref_minus1;
    num_mvref_minus1.select<4,2>(0) = cm_max<uint1>(0, cm_add<int2>(g_stack_size, -2)); // nearmv for last/second/compound/unused
    num_mvref_minus1.select<4,2>(1) = cm_max<uint1>(0, cm_add<int2>(g_stack_size, -1)); // newmv  for last/second/compound/unused

    vector<uint1,4> max_num_drl_bits;
    max_num_drl_bits.format<uint>() = 0x02020100; // 0,1,2,2 bits for nearest/zero, near/new, near1/new1 and near2/new2 modes

    // matrix 4x8 where
    //   4 refs: last, second, compound, unused;
    //   8 modes: nearest, near, near1, near2, zero, new, new1, new2
    matrix<uint2,4,8> num_drl_bits = cm_min<uint2>(num_mvref_minus1.replicate<8,1,4,0>(), max_num_drl_bits.replicate<8>());
    num_drl_bits.format<uint4>() <<= 9;
    g_bits_mode += num_drl_bits;

    // sort weights
    matrix<uint2,4,4> indices;
    g_weights += vector<uint1,8>(SEQUENCE_7_TO_0).replicate<4>();
    #pragma unroll
    for (int i = 0; i < 4; i++) {
        matrix<uint2,4,4> tmp4 = cm_max<uint2>(g_weights.select<4,1,4,2>(0,0), g_weights.select<4,1,4,2>(0,1));
        matrix<uint2,4,2> tmp2 = cm_max<uint2>(tmp4.select<4,1,2,2>(0,0), tmp4.select<4,1,2,2>(0,1));
        indices.select<4,1,1,1>(0,i) = cm_max<uint2>(tmp2.select<4,1,1,1>(0,0), tmp2.select<4,1,1,1>(0,1));
        if (i < 3) {
            g_weights.row(0).merge(0, g_weights.row(0) == indices(0,i));
            g_weights.row(1).merge(0, g_weights.row(1) == indices(1,i));
            g_weights.row(2).merge(0, g_weights.row(2) == indices(2,i));
        }
    }

    indices &= 7;
    indices = 7 - indices;

    // 1d select to get 4 mvs for first ref and 4 mvs for second ref
    indices.row(1) += 8;
    g_refmv.format<uint4>().select<8,1>(0) = g_stack.format<uint4>().iselect(indices.format<uint2>().select<8,1>());

    if (get_comp_flag()) {
        // 1d select to get 16 mv components, or 8 mv, or 4 mv pairs.
        //   indices.row(2) contains 4 indices of 4 mv pairs (each mv pair is 4 words)
        //   select them as 8 uints
        vector<uint2,8> idx;
        idx.select<4,2>(0) = indices.row(2) * 2;
        idx.select<4,2>(1) = idx.select<4,2>(0) + 1;
        g_refmv.select<16,1>(16).format<uint4>() = g_stack.select<32,1>(32).format<uint4>().iselect(idx);

        // if compound stack is too small borrow mvs from single ref stacks
        if (g_stack_size(2) < 2) {
            g_refmv.format<uint4>().select<2,1>(8+2) = g_refmv.format<uint4>().select<2,4>(1);
            if (g_stack_size(2) < 1)
                g_refmv.format<uint4>().select<2,1>(8+0) = g_refmv.format<uint4>().select<2,4>(0);
        }
    }

    // when stack size is 0/1, 1 stack mv can be PredMv
    // when stack size is 2,   2 stack mv can be PredMv
    // when stack size is 3+,  3 stack mv can be PredMv
    g_stack_size_mask = cm_min<uint2>(cm_max<uint2>(g_stack_size, 1), 3);
    g_stack_size_mask = vector<uint2,4>(0xffff) << g_stack_size_mask;

    // clamp stack sizes
    // max=4, since no more than 4 mvs in stack used
    // min=2, since when there are less than 2 mvs, first 2 are set to zero
    g_stack_size = cm_min<uint2>(4, g_stack_size);
    g_stack_size = cm_max<uint2>(2, g_stack_size);

    g_stack_size.select<1,1>(1).merge(0, get_single_ref_flag());
    g_stack_size.select<1,1>(2).merge(0, !get_comp_flag());

    // clamp motion vectors
    vector<int4,4> minmaxmv_;
    minmaxmv_.select<2,1>(0) = -mi * MI_SIZE * 8 - (MI_SIZE * size8) * 8 - MV_BORDER_AV1;
    minmaxmv_.select<2,1>(2) = (get_wh() - mi) * MI_SIZE * 8 + MV_BORDER_AV1;
    vector<int2,4> minmaxmv = cm_add<int2>(minmaxmv_, 0, SAT);

    g_refmv.select<16,1>(0)  = cm_max<int2>(g_refmv.select<16,1>(0),  minmaxmv.select<2,1>(0).replicate<8>());
    g_refmv.select<16,1>(16) = cm_max<int2>(g_refmv.select<16,1>(16), minmaxmv.select<2,1>(0).replicate<8>());
    g_refmv.select<16,1>(0)  = cm_min<int2>(g_refmv.select<16,1>(0),  minmaxmv.select<2,1>(2).replicate<8>());
    g_refmv.select<16,1>(16) = cm_min<int2>(g_refmv.select<16,1>(16), minmaxmv.select<2,1>(2).replicate<8>());
}

_GENX_ void interpolate8(SurfaceIndex REF, vector<int2,2> pos, matrix<int1,2,4> filt)
{
    matrix<uint1,14,16> in;
    matrix<int2,4,8> acc;
    matrix<uint1,14,8> tmpHor;

    if ((filt(X,1) & filt(Y,1)) == 64) {
        read(REF, pos(X) + 2, pos(Y) + 2, g_pred8);
        return;
    }

    // hor interpolation
    if (filt(X,1) == 64) {
        read(REF, pos(X) + 2, pos(Y), tmpHor);
    } else {
        read(REF, pos(X), pos(Y), in);
        #pragma unroll
        for (int i = 0; i < 12; i += 4) {
            #pragma unroll
            for (int j = 0; j < 4; j += 2) {
                acc.select<2,1,8,1>(j)  = in.select<2,1,8,1>(i+j,0) + in.select<2,1,8,1>(i+j,5);
                acc.select<2,1,8,1>(j) += in.select<2,1,8,1>(i+j,1) * filt(X,2-2);
                acc.select<2,1,8,1>(j) += in.select<2,1,8,1>(i+j,2) * filt(X,3-2);
                acc.select<2,1,8,1>(j) += in.select<2,1,8,1>(i+j,3) * filt(X,4-2);
                acc.select<2,1,8,1>(j) += in.select<2,1,8,1>(i+j,4) * filt(X,5-2);
                acc.select<2,1,8,1>(j) += 32;
            }
            tmpHor.select<4,1,8,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
        acc.select<2,1,8,1>()  = in.select<2,1,8,1>(12,0) + in.select<2,1,8,1>(12,5);
        acc.select<2,1,8,1>() += in.select<2,1,8,1>(12,1) * filt(X,2-2);
        acc.select<2,1,8,1>() += in.select<2,1,8,1>(12,2) * filt(X,3-2);
        acc.select<2,1,8,1>() += in.select<2,1,8,1>(12,3) * filt(X,4-2);
        acc.select<2,1,8,1>() += in.select<2,1,8,1>(12,4) * filt(X,5-2);
        acc.select<2,1,8,1>() += 32;
        tmpHor.select<2,1,8,1>(12) = cm_shr<uint1>(acc.select<2,1,8,1>(), 6, SAT);
    }

    // vert interpolation
    if (filt(Y,1) == 64) {
        g_pred8 = tmpHor.select<8,1,8,1>(2,0);
    } else {
        #pragma unroll
        for (int i = 0; i < 8; i += 4) {
            #pragma unroll
            for (int j = 0; j < 4; j += 2) {
                acc.select<2,1,8,1>(j)  = tmpHor.select<2,1,8,1>(i+j+0) + tmpHor.select<2,1,8,1>(i+j+5);
                acc.select<2,1,8,1>(j) += tmpHor.select<2,1,8,1>(i+j+1) * filt(Y,2-2);
                acc.select<2,1,8,1>(j) += tmpHor.select<2,1,8,1>(i+j+2) * filt(Y,3-2);
                acc.select<2,1,8,1>(j) += tmpHor.select<2,1,8,1>(i+j+3) * filt(Y,4-2);
                acc.select<2,1,8,1>(j) += tmpHor.select<2,1,8,1>(i+j+4) * filt(Y,5-2);
                acc.select<2,1,8,1>(j) += 32;
            }
            g_pred8.select<4,1,8,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
    }
}

_GENX_ void interpolate16(SurfaceIndex REF, vector<int2,2> pos, matrix<int1,2,4> filt)
{
    matrix<uint1,22,32> in;
    matrix<uint1,22,16> tmpHor;
    matrix<int2,2,16> acc;

    if ((filt(X,1) & filt(Y,1)) == 64) {
        read(REF, pos(X) + 2, pos(Y) + 2, g_pred16);
        return;
    }

    // hor interpolation
    if (filt(X,1) == 64) {
        read(REF, pos(X) + 2, pos(Y) + 0,  tmpHor.select<16,1,16,1>(0));
        read(REF, pos(X) + 2, pos(Y) + 16, tmpHor.select<5,1,16,1>(16));
    } else {
        read(REF, pos(X), pos(Y) + 0,  in.select<8,1,32,1>(0));
        read(REF, pos(X), pos(Y) + 8,  in.select<8,1,32,1>(8));
        read(REF, pos(X), pos(Y) + 16, in.select<5,1,32,1>(16));
        #pragma unroll
        for (int i = 0; i < 22; i += 2) {
            #pragma unroll
            for (int j = 0; j < 2; j++) {
                acc.row(j)  = in.select<1,1,16,1>(i+j,1-1) + in.select<1,1,16,1>(i+j,6-1);
                acc.row(j) += in.select<1,1,16,1>(i+j,2-1) * filt(X,2-2);
                acc.row(j) += in.select<1,1,16,1>(i+j,3-1) * filt(X,3-2);
                acc.row(j) += in.select<1,1,16,1>(i+j,4-1) * filt(X,4-2);
                acc.row(j) += in.select<1,1,16,1>(i+j,5-1) * filt(X,5-2);
                acc.row(j) += 32;
            }
            tmpHor.select<2,1,16,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
    }

    // vert interpolation
    if (filt(Y,1) == 64) {
        g_pred16 = tmpHor.select<16,1,16,1>(2,0);
    } else {
        #pragma unroll
        for (int i = 0; i < 16; i+=2) {
            #pragma unroll
            for (int j = 0; j < 2; j++) {
                acc.row(j)  = tmpHor.row(i+j+1-1) + tmpHor.row(i+j+6-1);
                acc.row(j) += tmpHor.row(i+j+2-1) * filt(Y,2-2);
                acc.row(j) += tmpHor.row(i+j+3-1) * filt(Y,3-2);
                acc.row(j) += tmpHor.row(i+j+4-1) * filt(Y,4-2);
                acc.row(j) += tmpHor.row(i+j+5-1) * filt(Y,5-2);
                acc.row(j) += 32;
            }
            g_pred16.select<2,1,16,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
    }
}

inline _GENX_ uint2 sad8(matrix_ref<uint1,8,8> m0, matrix_ref<uint1,8,8> m1)
{
    vector<uint2,16> sad;
    sad = cm_sad2<uint2> (m0.select<2,1,8,1>(0), m1.select<2,1,8,1>(0));
    sad = cm_sada2<uint2>(m0.select<2,1,8,1>(2), m1.select<2,1,8,1>(2), sad);
    sad = cm_sada2<uint2>(m0.select<2,1,8,1>(4), m1.select<2,1,8,1>(4), sad);
    sad = cm_sada2<uint2>(m0.select<2,1,8,1>(6), m1.select<2,1,8,1>(6), sad);
    return cm_sum<uint2>(sad.select<8,2>());
}

inline _GENX_ uint2 sad16(matrix_ref<uint1,16,16> m0, matrix_ref<uint1,16,16> m1)
{
    vector<uint2,16> sad;
    sad = cm_sad2<uint2> (m0.select<1,1,16,1>(0), m1.select<1,1,16,1>(0));
    #pragma unroll
    for (int i = 1; i < 16; i++)
        sad = cm_sada2<uint2>(m0.select<1,1,16,1>(i), m1.select<1,1,16,1>(i), sad);
    return cm_sum<uint2>(sad.select<8,2>());
}

_GENX_ inline vector<uint2,16> sad16_acc(matrix_ref<uint1,16,16> m0, matrix_ref<uint1,16,16> m1)
{
    vector<uint2,16> sad;
    sad = cm_sad2<uint2> (m0.select<1,1,16,1>(0), m1.select<1,1,16,1>(0));
    #pragma unroll
    for (int i = 1; i < 16; i++)
        sad = cm_sada2<uint2>(m0.select<1,1,16,1>(i), m1.select<1,1,16,1>(i), sad);
    return sad;
}

template <int N>
_GENX_ inline uint4 get_mv_cost(vector_ref<int2,N> mv, vector_ref<int2,N> ref_mv)
{
    vector<uint4,N> bits = cm_abs<uint4>(mv - ref_mv);
    bits >>= 1;
    bits += 1;
    bits = (31 << 10) + 800 - (cm_lzd(bits) << 10);
    return cm_sum<uint4>(bits);
}

_GENX_ /*inline */uint4 get_min_newmv_cost(uint ref)
{
    const vector<int2,8> ref_mvs = g_refmv.select<8,1>(8 * ref);
    const vector<int2,8> mv = get_mv0(g_mi).replicate<4>();

    vector<uint4,4> mode_bits = g_bits_mode.format<uint2>().select<4,1>(8 * ref + MODE_IDX_NEWMV);

    vector<uint4,8> mv_bits = cm_abs<uint4>(mv - ref_mvs);
    mv_bits >>= 1;
    mv_bits += 1;
    mv_bits = (31 << 10) + 800 - (cm_lzd(mv_bits) << 10);
    vector<uint4,4> bits = mv_bits.select<4,2>(0) + mv_bits.select<4,2>(1);
    bits += mode_bits;

    const uint4 mask = g_stack_size_mask(ref);
    assert_emu((mask & 1) == 0);
    assert_emu((mask & 8) != 0);
    bits.merge(0xffffffff, mask);

    bits(0) = cm_min<uint4>(bits(0), bits(1));
    bits(0) = cm_min<uint4>(bits(0), bits(2));

    return bits(0);
}

_GENX_ /*inline*/ uint4 get_min_newmv_cost_comp()
{
    const vector<int2,16> ref_mvs = g_refmv.select<16,1>(16);
    const vector<int2,16> mv = get_mv_pair(g_mi).replicate<4>();

    vector<uint4,4> mode_bits = g_bits_mode.format<uint2>().select<4,1>(16 + MODE_IDX_NEWMV);

    vector<uint4,16> mv_bits = cm_abs<uint4>(mv - ref_mvs);
    mv_bits >>= 1;
    mv_bits += 1;
    mv_bits = (31 << 10) + 800 - (cm_lzd(mv_bits) << 10);
    mv_bits.select<8,1>() = mv_bits.select<8,2>(0) + mv_bits.select<8,2>(1);
    vector<uint4,4> bits = mv_bits.select<4,2>(0) + mv_bits.select<4,2>(1);
    bits += mode_bits;

    const uint4 mask = g_stack_size_mask(2);
    assert_emu((mask & 1) == 0);
    assert_emu((mask & 8) != 0);
    bits.merge(0xffffffff, mask);

    bits(0) = cm_min<uint4>(bits(0), bits(1));
    bits(0) = cm_min<uint4>(bits(0), bits(2));

    return bits(0);
}


_GENX_ inline uint2 get_interp_filter_bits()
{
    const uint2 is_compound = (get_ref1(g_mi) != NONE);
    const uint2 no_same_ref = (g_neighbor_refs != get_ref0(g_mi)).all();
    uint2 ctx = 2 * is_compound;
    ctx += no_same_ref;
    return g_bits_interp_filter[ctx];
}

_GENX_ inline void get_newmv_cost_and_bits(uint &cost, uint &bits)
{
    uint newmv_ref = g_new_mv_data(0,1) & 3;
    uint sad = g_new_mv_data(0,1) >> 2;

    if (newmv_ref == 2) {
        set_mv_pair(g_mi, g_new_mv_data.format<int2,2,4>().select<2,1,2,1>() << 1);
        set_ref_bidir(g_mi);
        bits = get_min_newmv_cost_comp();
    } else {
        set_mv0(g_mi, g_new_mv_data.row(newmv_ref & 1).format<int2>().select<2,1>() << 1);
        set_ref_single(g_mi, (newmv_ref & 1) << get_comp_flag());
        bits = get_min_newmv_cost(newmv_ref);
    }

    sad <<= 11;
    set_sad(g_mi, sad);
    cost = bits * get_lambda_int();
    cost += sad;
}

_GENX_ inline vector<uint4,2> get_cache_idx(vector_ref<uint4,8> refmvs0, vector_ref<uint4,2> mvs, vector_ref<uint2,2> checked_mv)
{
    vector<uint2,2> mask;
    mask(0) = cm_pack_mask(refmvs0 == mvs.replicate<2,1,4,0>());
    mask(1) = mask(0) >> 4;
    checked_mv |= mask;
    return cm_fbl<uint4>(vector<uint4,2>(mask | 0x80)); // 0x80 is a guard bit, to ensure that returned index is less than 8
}


_GENX_ uint
    decide8(SurfaceIndex PARAM, SurfaceIndex SRC, vector<SurfaceIndex,7> REF, vector<SurfaceIndex,8> MV,
            SurfaceIndex MODE_INFO, vector<uint2,2> mi)
{
    g_mi = 0;
    set_sb_type(g_mi, BLOCK_8X8);

    if (is_outsize(mi)) {
        set_mode(g_mi, OUT_OF_PIC);
        write_mode_info_8(MODE_INFO, mi);
        return 0;
    }

    set_mode(g_mi, NEWMV);

    matrix<uint1,8,8> pred;
    uint cost_non_split;
    uint bits_non_split;

    get_mvrefs(PARAM, MODE_INFO, SIZE8, mi);

    read(MV(SIZE8 + 0), mi(X) * NEW_MV_DATA_SMALL_SIZE, mi(Y), g_new_mv_data.select<1,1,2,1>(0));
    read(MV(SIZE8 + 4), mi(X) * NEW_MV_DATA_SMALL_SIZE, mi(Y), g_new_mv_data.select<1,1,2,1>(1));

    get_newmv_cost_and_bits(cost_non_split, bits_non_split);
    //if (get_comp_flag()) debug_printf("(3,%2d,%2d) NEWMV ref=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), get_ref_comb(g_mi),
    //    get_mv_pair(g_mi)(0), get_mv_pair(g_mi)(1), get_mv_pair(g_mi)(2), get_mv_pair(g_mi)(3), cost_non_split, bits_non_split);

    vector<int4,2> pel = mi * MI_SIZE;
    vector<int2,2> pel_minus2 = pel - 2;

    vector<uint2,2> checked_mv = 0;

    matrix<uint1,8,8> src;
    read(SRC,          pel(X), pel(Y), src);
    read(REF(2+SIZE8), pel(X), pel(Y), pred);

    for (uint2 i = 0; i < g_stack_size(2); i++) {
        vector_ref<int2,4> mv = g_refmv.select<4,1>(16);
        if ((mv != 0).any()) {

            vector<uint4,2> cache_idx = get_cache_idx(g_refmv.format<uint4>().select<8,1>(), mv.format<uint4>(), checked_mv);

            vector<int2,4> pos = pel_minus2.replicate<2>() + cm_asr<int2>(mv, 3);
            matrix<int1,4,4> filt;
            vector<uint2,4> dmv = cm_asr<uint2>(mv, 1) & 3;
            filt.format<uint4>() = g_interp_filters.format<uint4>().iselect(dmv);

            uint4 cost_ref0 = uint4(-1);
            uint4 cost_ref1 = uint4(-1);
            uint4 sad_ref0;
            uint4 sad_ref1;
            uint4 bits_ref0;
            uint4 bits_ref1;

            interpolate8(REF(0), pos.select<2,1>(0), filt.select<2,1,4,1>(0));
            matrix<uint1,8,8> pred0 = g_pred8;
            if (cache_idx(0) < g_stack_size(0)) {
                sad_ref0  = sad8(src, g_pred8) << 11;
                bits_ref0 = g_bits_mode(0, cache_idx(0));
                cost_ref0 = bits_ref0 * get_lambda_int();
                cost_ref0 += sad_ref0;
                //if (get_comp_flag()) debug_printf("(3,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), 0, cache_idx(0), mv(0), mv(1), cost_ref0, bits_ref0);
            }

            interpolate8(REF(1), pos.select<2,1>(2), filt.select<2,1,4,1>(2));
            matrix<uint1,8,8> pred1 = g_pred8;
            if (cache_idx(1) < g_stack_size(1)) {
                sad_ref1  = sad8(src, g_pred8) << 11;
                bits_ref1 = g_bits_mode(1, cache_idx(1));
                cost_ref1 = bits_ref1 * get_lambda_int();
                cost_ref1 += sad_ref1;
                //if (get_comp_flag()) debug_printf("(3,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), 1, cache_idx(1), mv(2), mv(3), cost_ref1, bits_ref1);
            }

            g_pred8 = cm_avg<uint1>(pred0, g_pred8);

            uint4 sad = sad8(src, g_pred8) << 11;
            uint2 bits = g_bits_mode(2,0);
            uint4 cost = sad + bits * get_lambda_int();
            //if (get_comp_flag()) debug_printf("(3,%2d,%2d) REFMV ref=2 mode=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), i, mv(0), mv(1), mv(2), mv(3), cost, bits);

            if (cost_non_split > cost) {
                cost_non_split = cost;
                bits_non_split = bits;
                set_mv_pair(g_mi, mv);
                set_ref_bidir(g_mi);
                set_mode(g_mi, NEARESTMV);
                set_sad(g_mi, sad);
                pred = g_pred8;
            }

            if (cost_non_split > cost_ref0) {
                cost_non_split = cost_ref0;
                bits_non_split = bits_ref0;
                set_mv0(g_mi, mv.select<2,1>(0));
                set_mv1(g_mi, 0);
                set_ref_single(g_mi, LAST);
                set_mode(g_mi, NEARESTMV);
                set_sad(g_mi, sad_ref0);
                pred = pred0;
            }

            if (cost_non_split > cost_ref1) {
                cost_non_split = cost_ref1;
                bits_non_split = bits_ref1;
                set_mv0(g_mi, mv.select<2,1>(2));
                set_mv1(g_mi, 0);
                set_ref_single(g_mi, ALTR);
                set_mode(g_mi, NEARESTMV);
                set_sad(g_mi, sad_ref1);
                pred = pred1;
            }
        }
        g_refmv.select<8,1>(16) = g_refmv.select<8,1>(20);
        g_refmv.select<8,1>(24) = g_refmv.select<8,1>(28);
        g_bits_mode.row(2).select<4,1>(0) = g_bits_mode.row(2).select<4,1>(1);
    }

    uint2 cur_refmv_count = g_stack_size(0);
    for (uint2 ref = 0; ref < 2; ref++) {
        for (uint2 i = 0; i < cur_refmv_count; i++) {
            vector_ref<int2,2> mv = g_refmv.select<2,1>();
            if (mv.format<uint4>()(0) != 0 && (checked_mv(0) & 1) == 0) {

                vector<int2,2> pos = pel_minus2 + cm_asr<int2>(mv, 3);
                matrix<int1,2,4> filt;
                vector<uint2,2> dmv = cm_asr<uint2>(mv, 1) & 3;
                filt.format<uint4>() = g_interp_filters.format<uint4>().iselect(dmv);

                interpolate8(REF(ref), pos, filt);
                uint4 sad = sad8(src, g_pred8) << 11;
                uint2 bits = g_bits_mode(0,0);
                uint4 cost = sad + bits * get_lambda_int();
                //if (get_comp_flag()) debug_printf("(3,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), ref, i, mv(0), mv(1), cost, bits);

                if (cost_non_split > cost) {
                    cost_non_split = cost;
                    bits_non_split = bits;
                    set_mv0(g_mi, mv);
                    set_mv1(g_mi, 0);
                    set_ref_single(g_mi, ref << get_comp_flag());
                    set_mode(g_mi, NEARESTMV);
                    set_sad(g_mi, sad);
                    pred = g_pred8;
                }
            }

            checked_mv(0) >>= 1;
            g_refmv.select<8,1>(0) = g_refmv.select<8,1>(2);
            g_bits_mode.row(0).select<4,1>(0) = g_bits_mode.row(0).select<4,1>(1);
        }

        checked_mv(0) = checked_mv(1);
        cur_refmv_count = g_stack_size(1);
        g_refmv.select<8,1>(0) = g_refmv.select<8,1>(8);
        g_bits_mode.row(0).select<4,1>(0) = g_bits_mode.row(1).select<4,1>(0);
    }

    vector<uint2,2> loc = mi & 7;
    uint4 zeromv_data = g_zeromv_sad_8(loc(Y), loc(X));
    uint4 zeromv_sad = (zeromv_data >> 2) << 11;
    uint1 zeromv_ref = zeromv_data & 3;
    uint2 zeromv_bits = g_bits_mode(zeromv_ref, MODE_IDX_ZEROMV);
    uint4 zeromv_cost = zeromv_sad + zeromv_bits * get_lambda_int();
    //if (get_comp_flag()) debug_printf("(3,%2d,%2d) ZEROMV ref=%d cost=%u bits=%u\n", mi(Y), mi(X), zeromv_ref, zeromv_cost, zeromv_bits);

    if (cost_non_split > zeromv_cost) {
        matrix<uint1,8,8> ref0, ref1;
        read(REF(0), pel(X), pel(Y), ref0);
        read(REF(1), pel(X), pel(Y), ref1);
        cost_non_split = zeromv_cost;
        bits_non_split = zeromv_bits;
        zero_mv_pair(g_mi);
        set_mode(g_mi, ZEROMV);
        set_ref_single(g_mi, (zeromv_ref & 1) << get_comp_flag());
        set_sad(g_mi, zeromv_sad);

        if (zeromv_ref == 0)
            pred = ref0;
        else if (zeromv_ref == 1)
            pred = ref1;
        else {
            set_ref1(g_mi, ALTR);
            set_ref_comb(g_mi, COMP);
            pred = cm_avg<uint1>(ref0, ref1);
        }
    }

    bits_non_split += BITS_IS_INTER;
    bits_non_split += get_interp_filter_bits();

    g_rd_cost[0] = 0;
    g_rd_cost[1] = 0;
    //g_rd_cost[1] = bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE8];
    g_skip_cost = uint(get_lambda() * (bits_non_split + g_bits_skip[1]) + 0.5f);
    g_qcoef = src - pred;
    get_rd_cost_8(PARAM, 1, mi);
    //cost_non_split = g_rd_cost[0] + uint(get_lambda() * g_rd_cost[1] + 0.5f);
    cost_non_split = g_rd_cost[0] + uint(get_lambda() * (bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE8] + ((g_rd_cost[1]*3)>>2)) + 0.5f);

    //if (get_comp_flag()) debug_printf("(3,%2d,%2d) skip_cost=%u skip_bits=%u\n", mi(Y), mi(X), g_skip_cost, bits_non_split + g_bits_skip[1]);
    //if (get_comp_flag()) debug_printf("(3,%2d,%2d) non_skip_cost=%u non_skip_bits=%u\n", mi(Y), mi(X), cost_non_split, g_rd_cost[1]);

    if (cost_non_split >= g_skip_cost) {
        cost_non_split = g_skip_cost;
        set_skip(g_mi, 1);
        set_coef_ctx<1>(mi, 0);
    } else {
        cost_non_split = g_rd_cost[0] + uint(get_lambda() * (bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE8] + g_rd_cost[1]) + 0.5f);
    }

    //debug_printf("3,%d,%d cost=%u\n", mi(Y), mi(X), cost_non_split);

    write_mode_info_8(MODE_INFO, mi);
    return cost_non_split;
}

_GENX_ inline uint2 not_joinable(mode_info_ref mi1, mode_info_ref mi2)
{
    uint2 mask = cm_pack_mask(mi1.format<uint2>() != mi2.format<uint2>());
    mask &= 0x20F; // mask everything but MV, refIdxComb and sbType
    return mask;
}

_GENX_ uint
    decide16(SurfaceIndex PARAM, SurfaceIndex SRC, vector<SurfaceIndex,7> REF, vector<SurfaceIndex,8> MV,
             SurfaceIndex MODE_INFO, vector<uint2,2> mi, uint skipee)
{
    if (is_outsize(mi)) {
        g_mi = 0;
        set_sb_type(g_mi, BLOCK_16X16);
        set_mode(g_mi, OUT_OF_PIC);
        write_mode_info_8(MODE_INFO, mi);
        return 0;
    }

    vector<uint1, TXB_CTX_SIZE> coef_ctx_orig = g_coef_ctx;
    uint cost_split = 0;

    uint cost_non_split = INT_MAX;
    if (!is_outsize(mi + 1)) {
        g_mi = 0;
        set_sb_type(g_mi, BLOCK_16X16);
        set_mode(g_mi, NEWMV);

        uint pred_idx = 2 + SIZE16;

        uint bits_non_split;

        get_mvrefs(PARAM, MODE_INFO, SIZE16, mi);

        read(MV(SIZE16 + 0), mi(X) * NEW_MV_DATA_SMALL_SIZE >> 1, mi(Y) >> 1, g_new_mv_data.select<1,1,2,1>(0));
        read(MV(SIZE16 + 4), mi(X) * NEW_MV_DATA_SMALL_SIZE >> 1, mi(Y) >> 1, g_new_mv_data.select<1,1,2,1>(1));

        get_newmv_cost_and_bits(cost_non_split, bits_non_split);
        //if (get_comp_flag()) debug_printf("(2,%2d,%2d) NEWMV ref=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), get_ref_comb(g_mi),
        //    get_mv_pair(g_mi)(0), get_mv_pair(g_mi)(1), get_mv_pair(g_mi)(2), get_mv_pair(g_mi)(3), cost_non_split, bits_non_split);

        vector<int4,2> pel = mi * MI_SIZE;
        vector<int2,2> pel_minus2 = pel - 2;

        uint2 cur_refmv_count = g_stack_size(0);
        matrix<uint1,16,16> src;
        read(SRC, pel(X), pel(Y), src);

        vector<uint2,2> checked_mv = 0;

        for (uint2 i = 0; i < g_stack_size(2); i++) {
            vector_ref<int2,4> mv = g_refmv.select<4,1>(16);
            if ((mv != 0).any()) {

                vector<uint4,2> cache_idx = get_cache_idx(g_refmv.format<uint4>().select<8,1>(), mv.format<uint4>(), checked_mv);

                vector<int2,4> pos = pel_minus2.replicate<2>() + cm_asr<int2>(mv, 3);
                matrix<int1,4,4> filt;
                vector<uint2,4> dmv = cm_asr<uint2>(mv, 1) & 3;
                filt.format<uint4>() = g_interp_filters.format<uint4>().iselect(dmv);

                uint4 cost_ref0 = uint4(-1);
                uint4 cost_ref1 = uint4(-1);
                uint4 sad_ref0;
                uint4 sad_ref1;
                uint4 bits_ref0;
                uint4 bits_ref1;

                interpolate16(REF(0), pos.select<2,1>(0), filt.select<2,1,4,1>(0));
                matrix<uint1,16,16> pred0 = g_pred16;
                if (cache_idx(0) < g_stack_size(0)) {
                    sad_ref0  = sad16(src, g_pred16) << 11;
                    bits_ref0 = g_bits_mode(0, cache_idx(0));
                    cost_ref0 = bits_ref0 * get_lambda_int();
                    cost_ref0 += sad_ref0;
                    //if (get_comp_flag()) debug_printf("(2,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), 0, cache_idx(0), mv(0), mv(1), cost_ref0, bits_ref0);
                }

                interpolate16(REF(1), pos.select<2,1>(2), filt.select<2,1,4,1>(2));
                matrix<uint1,16,16> pred1 = g_pred16;
                if (cache_idx(1) < g_stack_size(1)) {
                    sad_ref1  = sad16(src, g_pred16) << 11;
                    bits_ref1 = g_bits_mode(1, cache_idx(1));
                    cost_ref1 = bits_ref1 * get_lambda_int();
                    cost_ref1 += sad_ref1;
                    //if (get_comp_flag()) debug_printf("(2,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), 1, cache_idx(1), mv(2), mv(3), cost_ref1, bits_ref1);
                }

                g_pred16 = cm_avg<uint1>(pred0, g_pred16);
                uint4 sad = sad16(src, g_pred16) << 11;
                uint2 bits = g_bits_mode(2,0);
                uint4 cost = sad + bits * get_lambda_int();
                //if (get_comp_flag()) debug_printf("(2,%2d,%2d) REFMV ref=2 mode=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), i, mv(0), mv(1), mv(2), mv(3), cost, bits);

                if (cost_non_split > cost) {
                    cost_non_split = cost;
                    bits_non_split = bits;
                    set_mv_pair(g_mi, mv);
                    set_ref_bidir(g_mi);
                    set_mode(g_mi, NEARESTMV);
                    set_sad(g_mi, sad);
                    write(REF(pred_idx), pel(X), pel(Y), g_pred16);
                }

                if (cost_non_split > cost_ref0) {
                    cost_non_split = cost_ref0;
                    bits_non_split = bits_ref0;
                    set_mv0(g_mi, mv.select<2,1>(0));
                    set_mv1(g_mi, 0);
                    set_ref_single(g_mi, LAST);
                    set_mode(g_mi, NEARESTMV);
                    set_sad(g_mi, sad_ref0);
                    write(REF(pred_idx), pel(X), pel(Y), pred0);
                }

                if (cost_non_split > cost_ref1) {
                    cost_non_split = cost_ref1;
                    bits_non_split = bits_ref1;
                    set_mv0(g_mi, mv.select<2,1>(2));
                    set_mv1(g_mi, 0);
                    set_ref_single(g_mi, ALTR);
                    set_mode(g_mi, NEARESTMV);
                    set_sad(g_mi, sad_ref1);
                    write(REF(pred_idx), pel(X), pel(Y), pred1);
                }
            }
            g_refmv.select<8,1>(16) = g_refmv.select<8,1>(20);
            g_refmv.select<8,1>(24) = g_refmv.select<8,1>(28);
            g_bits_mode.row(2).select<4,1>(0) = g_bits_mode.row(2).select<4,1>(1);
        }

        for (uint2 ref = 0; ref < 2; ref++) {
            for (uint2 i = 0; i < cur_refmv_count; i++) {
                vector_ref<int2,2> mv = g_refmv.select<2,1>();
                if (mv.format<uint4>()(0) != 0 && (checked_mv(0) & 1) == 0) {

                    vector<int2,2> pos = pel_minus2 + cm_asr<int2>(mv, 3);
                    matrix<int1,2,4> filt;
                    vector<uint2,2> dmv = cm_asr<uint2>(mv, 1) & 3;
                    filt.format<uint4>() = g_interp_filters.format<uint4>().iselect(dmv);

                    interpolate16(REF(ref), pos, filt);

                    uint4 sad = sad16(src, g_pred16) << 11;
                    uint2 bits = g_bits_mode(0,0);
                    uint4 cost = sad + bits * get_lambda_int();
                    //if (get_comp_flag()) debug_printf("(2,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), ref, i, mv(0), mv(1), cost, bits);

                    if (cost_non_split > cost) {
                        cost_non_split = cost;
                        bits_non_split = bits;
                        set_mv0(g_mi, mv);
                        set_mv1(g_mi, 0);
                        set_ref_single(g_mi, ref << get_comp_flag());
                        set_mode(g_mi, NEARESTMV);
                        set_sad(g_mi, sad);
                        write(REF(pred_idx), pel(X), pel(Y), g_pred16);
                    }
                }

                checked_mv(0) >>= 1;
                g_refmv.select<8,1>(0) = g_refmv.select<8,1>(2);
                g_bits_mode.row(0).select<4,1>(0) = g_bits_mode.row(0).select<4,1>(1);
            }

            checked_mv(0) = checked_mv(1);
            cur_refmv_count = g_stack_size(1);
            g_refmv.select<8,1>(0) = g_refmv.select<8,1>(8);
            g_bits_mode.row(0).select<4,1>(0) = g_bits_mode.row(1).select<4,1>(0);
        }

        matrix<uint1,16,16> ref0, ref1;

        vector<uint2,2> loc = mi >> 1; loc &= 3;
        uint4 zeromv_data = g_zeromv_sad_16(loc(Y), loc(X));
        uint4 zeromv_sad = (zeromv_data >> 2) << 11;
        uint1 zeromv_ref = zeromv_data & 3;
        uint2 zeromv_bits = g_bits_mode(zeromv_ref, MODE_IDX_ZEROMV);
        uint4 zeromv_cost = zeromv_sad + zeromv_bits * get_lambda_int();
        //if (get_comp_flag()) debug_printf("(2,%2d,%2d) ZEROMV ref=%d cost=%u bits=%u\n", mi(Y), mi(X), zeromv_ref, zeromv_cost, zeromv_bits);

        if (cost_non_split > zeromv_cost) {
            cost_non_split = zeromv_cost;
            bits_non_split = zeromv_bits;
            zero_mv_pair(g_mi);
            set_mode(g_mi, ZEROMV);
            set_ref_single(g_mi, (zeromv_ref & 1) << get_comp_flag());
            set_sad(g_mi, zeromv_sad);

            if (zeromv_ref == 2) {
                set_ref1(g_mi, ALTR);
                set_ref_comb(g_mi, COMP);
                read(REF(0), pel(X), pel(Y), ref0);
                read(REF(1), pel(X), pel(Y), ref1);
                ref0 = cm_avg<uint1>(ref0, ref1);
                write(REF(pred_idx), pel(X), pel(Y), ref0);
            } else {
                pred_idx = zeromv_ref;
            }
        }

        bits_non_split += BITS_IS_INTER;
        bits_non_split += get_interp_filter_bits();

        g_coef_ctx = coef_ctx_orig;

        matrix<uint1,16,16> pred;
        read(MODIFIED(REF(pred_idx)), pel(X), pel(Y), pred);

        g_rd_cost[0] = 0;
        g_rd_cost[1] = 0;
        //g_rd_cost[1] = bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE16];
        g_skip_cost = uint(get_lambda() * (bits_non_split + g_bits_skip[1]) + 0.5f);
        g_qcoef = src.select<8,1,8,1>(0,0) - pred.select<8,1,8,1>(0,0);
        get_rd_cost_8(PARAM, 0, BLOCK0(mi, 1));
        g_qcoef = src.select<8,1,8,1>(0,8) - pred.select<8,1,8,1>(0,8);
        get_rd_cost_8(PARAM, 0, BLOCK1(mi, 1));
        g_qcoef = src.select<8,1,8,1>(8,0) - pred.select<8,1,8,1>(8,0);
        get_rd_cost_8(PARAM, 0, BLOCK2(mi, 1));
        g_qcoef = src.select<8,1,8,1>(8,8) - pred.select<8,1,8,1>(8,8);
        get_rd_cost_8(PARAM, 0, BLOCK3(mi, 1));
        //cost_non_split = g_rd_cost[0] + uint(get_lambda() * g_rd_cost[1] + 0.5f);
        cost_non_split = g_rd_cost[0] + uint(get_lambda() * (bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE16] + ((g_rd_cost[1]*3)>>2)) + 0.5f);

        //if (get_comp_flag()) debug_printf("(2,%2d,%2d) skip_cost=%u skip_bits=%u\n", mi(Y), mi(X), g_skip_cost, bits_non_split + g_bits_skip[1]);
        //if (get_comp_flag()) debug_printf("(2,%2d,%2d) non_skip_cost=%u non_skip_bits=%u\n", mi(Y), mi(X), cost_non_split, g_rd_cost[1]);

        if (cost_non_split >= g_skip_cost) {
            cost_non_split = g_skip_cost;
            set_skip(g_mi, 1);
            set_coef_ctx<2>(mi, 0);

            if (skipee) {
                write_mode_info_16(MODE_INFO, mi);
                //debug_printf("2,%d,%d cost=%u\n", mi(Y), mi(X), cost_non_split);
                return cost_non_split;
            }

        } else {
            cost_non_split = g_rd_cost[0] + uint(get_lambda() * (bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE16] + g_rd_cost[1]) + 0.5f);
        }
        //debug_printf("2,%d,%d cost=%u\n", mi(Y), mi(X), cost_non_split);
    }

    vector<uint1, TXB_CTX_SIZE> coef_ctx_saved = g_coef_ctx;
    g_mi_saved16 = g_mi;

    mode_info miq0, miq1, miq2, miq3;

    {
        g_coef_ctx = coef_ctx_orig;

        cost_split += decide8(PARAM, SRC, REF, MV, MODE_INFO, BLOCK0(mi, 1));
        miq0 = g_mi;
        cost_split += decide8(PARAM, SRC, REF, MV, MODE_INFO, BLOCK1(mi, 1));
        miq1 = g_mi;
        cost_split += decide8(PARAM, SRC, REF, MV, MODE_INFO, BLOCK2(mi, 1));
        miq2 = g_mi;
        cost_split += decide8(PARAM, SRC, REF, MV, MODE_INFO, BLOCK3(mi, 1));
        miq3 = g_mi;
    }


    if (cost_split < cost_non_split) {
#ifdef JOIN_MI_INLOOP
        g_mi = miq0;
        // Join MI
        const int qsbtype = BLOCK_8X8;
        while (get_sb_type(miq0) == qsbtype) {
            uint1 skip = get_skip(miq0);
            if (get_mode(miq1) == 255 || not_joinable(miq0, miq1)) break;
            skip &= get_skip(miq1);

            if (get_mode(miq2) == 255 || not_joinable(miq0, miq2)) break;
            skip &= get_skip(miq2);

            if (not_joinable(miq0, miq3)) break;
            skip &= get_skip(miq3);

            set_skip(g_mi, skip);
            set_sb_type(g_mi, BLOCK_16X16);
            uint4 sad = get_sad(miq0);
            sad += get_sad(miq1);
            sad += get_sad(miq2);
            sad += get_sad(miq3);
            set_sad(g_mi, sad);
            write_mode_info_16(MODE_INFO, mi);
            break;
        }
#endif

        return cost_split;
    }

    g_coef_ctx = coef_ctx_saved;
    g_mi = g_mi_saved16;

    write_mode_info_16(MODE_INFO, mi);
    return cost_non_split;
}


_GENX_
inline bool IsBadPred0(float SCpp, float SADpp_best, uint2 mvdAvg, uint1 l)
{
    bool badPred = false;
    const float mt = (l == 0) ? 2.58784f : ((l == 1) ? 7.278976f : 10.856256f);
    const float st = (l == 0) ? 20.f : ((l == 1) ? 77.15f : 142.184f);

    if (SCpp > st && mvdAvg > mt) {
        const float a = (l == 0) ? 0.303627f : ((l == 1) ? 0.423133f : 0.397384f);
        const float b = (l == 0) ? 0.547588f : ((l == 1) ? 0.022428f : 0.473092f);
        const float m = (l == 0) ? 0.003458f : 0.013396f;
        float SADT = b + cm_log(SCpp)*a*0.693147f;
        float msad = cm_log(SADpp_best + m * cm_min<uint2>(64, mvdAvg))*0.693147f;

        if (msad > SADT)       badPred = true;
    }
    return badPred;
}

_GENX_
inline bool IsBadPred1(float SCpp, float SADpp_best, uint2 mvdAvg, uint1 l)
{
    bool badPred = false;
    const float mt = (l == 0) ? 6.87232f : ((l == 1) ? 7.278976f : 10.856256f);
    const float st = (l == 0) ? 20.f : ((l == 1) ? 89.805f : 142.184f);

    if (SCpp > st && mvdAvg > mt) {
        const float a = (l == 0) ? 0.315012f : ((l == 1) ? 0.435791f : 0.397396f);
        const float b = (l == 0) ? 0.572353f : ((l == 1) ? 0.041593f : 0.527727f);
        const float m = (l == 0) ? 0.006642f : 0.013410f;
        float SADT = b + cm_log(SCpp)*a*0.693147f;
        float msad = cm_log(SADpp_best + m * cm_min<uint2>(64, mvdAvg))*0.693147f;

        if (msad > SADT)       badPred = true;
    }
    return badPred;
}

_GENX_
inline bool IsBadPred2(float SCpp, float SADpp_best, uint2 mvdAvg, uint1 l)
{
    bool badPred = false;
    const float mt = (l == 0) ? 7.239296f : ((l == 1) ? 7.278976f : 10.856256f);
    const float st = (l == 0) ? 20.f : ((l == 1) ? 89.805f : 249.454f);

    if (SCpp > st && mvdAvg > mt) {
        const float a = (l == 0) ? 0.315445f : ((l == 1) ? 0.443448f : 0.500000f);
        const float b = (l == 0) ? 0.677961f : ((l == 1) ? 0.203189f : 0.000000f);
        const float m = (l == 0) ? 0.006441f : 0.013410f;
        float SADT = b + cm_log(SCpp)*a*0.693147f;
        float msad = cm_log(SADpp_best + m * cm_min<uint2>(64, mvdAvg))*0.693147f;

        if (msad > SADT)       badPred = true;
    }
    return badPred;
}

_GENX_
inline bool IsBadPred3(float SCpp, float SADpp_best, uint2 mvdAvg, uint1 l)
{
    bool badPred = false;
    const float mt = (l == 0) ? 7.646336f : ((l == 1) ? 9.9616f : 10.856256f);
    const float st = (l == 0) ? 20.f : 249.454f;

    if (SCpp > st && mvdAvg > mt) {
        const float a = (l == 0) ? 0.314350f : ((l == 1) ? 0.456550f : 0.500000f);
        const float b = (l == 0) ? 0.794845f : ((l == 1) ? 0.228990f : 0.000000f);
        const float m = (l == 0) ? 0.010107f : 0.137010f;
        float SADT = b + cm_log(SCpp)*a*0.693147f;
        float msad = cm_log(SADpp_best + m * cm_min<uint2>(64, mvdAvg))*0.693147f;

        if (msad > SADT)       badPred = true;
    }
    return badPred;
}

_GENX_ uint1 SplitIEF(vector<SurfaceIndex, 8> MV, vector<SurfaceIndex, 4> RSCS, vector<uint2, 2> mi, int qpLayer, int is64)
{
    int2 l = (qpLayer & 0x3);
    int2 qp = (qpLayer >> 2);
    if (qp < 40)      return SPLIT_TRY_NE;
    if (qp > 225)     return SPLIT_TRY_EE;
    if (l > 1 + is64) return SPLIT_TRY_EE;

    vector<int, 1> Rs = 0;
    vector<int, 1> Cs = 0;
    read(RSCS(is64 * 2 + 0), (mi(X) >> (2 + is64)) * 4, mi(Y) >> (2 + is64), Rs);
    read(RSCS(is64 * 2 + 1), (mi(X) >> (2 + is64)) * 4, mi(Y) >> (2 + is64), Cs);
    float scpp = Rs(0) + Cs(0);
    scpp *= (is64 ? 0.003906250f : 0.015625000f);
    if (scpp <=  20.00f) return SPLIT_TRY_EE;

    uint newmv_ref = g_new_mv_data(0, 1) & 3;
    uint mvd = 0;
    if (newmv_ref == 2) {
        vector<int2, 4> mv = g_new_mv_data.format<int2, 2, 4>().select<2, 1, 2, 1>() << 1;
        vector<int2, 4> dmv = mv - g_refmv.select<4, 1>(16);
        dmv = cm_abs<int2>(dmv);
        mvd = dmv(0) + dmv(1);
        uint2 mvd1 = dmv(2) + dmv(3);
        mvd = cm_min<uint2>(mvd, mvd1);
    } else {
        vector<int2, 2> mv = g_new_mv_data.row(newmv_ref & 1).format<int2>().select<2, 1>() << 1;
        vector<int2, 2> pmv = ((newmv_ref == 1) ? g_refmv.select<2, 1>(8) : g_refmv.select<2, 1>(0));
        vector<int2, 2> dmv = mv - pmv;
        dmv = cm_abs<int2>(dmv);
        mvd = dmv(0) + dmv(1);
    }
    if (mvd <= 2) return SPLIT_TRY_NE;

    // Split
    int2 Q = qp - ((l + 1) * 8);
    uint1 qidx = 0;
    if (Q < 75)       qidx = 0;
    else if (Q < 123) qidx = 1;
    else if (Q < 163) qidx = 2;
    else              qidx = 3;

    uint bsad = g_new_mv_data(0, 1) >> 2;
    uint ssad = bsad;
    float sadpp = bsad;
    sadpp *= (is64 ? 0.000244140625f : 0.000976562500f);
    if (sadpp <= 1.00f) return SPLIT_TRY_NE;

    uint sad_min = 1;
    uint sad_max = 1;

    if (is64) {
        /*matrix<uint4, 1, 2>   new_mv_data;
        read(MV(SIZE32 + 0), mi(X) * NEW_MV_DATA_LARGE_SIZE >> 2, mi(Y) >> 2, new_mv_data);
        uint sad = new_mv_data(0, 1) >> 2;
        sad_min = sad;
        sad_max = sad;
        ssad = sad;
        read(MV(SIZE32 + 0), (mi(X) + 4) * NEW_MV_DATA_LARGE_SIZE >> 2, mi(Y) >> 2, new_mv_data);
        sad = new_mv_data(0, 1) >> 2;
        sad_min = cm_min<uint>(sad_min, sad);
        sad_max = cm_max<uint>(sad_max, sad);
        ssad += sad;
        read(MV(SIZE32 + 0), mi(X) * NEW_MV_DATA_LARGE_SIZE >> 2, (mi(Y) + 4) >> 2, new_mv_data);
        sad = new_mv_data(0, 1) >> 2;
        sad_min = cm_min<uint>(sad_min, sad);
        sad_max = cm_max<uint>(sad_max, sad);
        ssad += sad;
        read(MV(SIZE32 + 0), (mi(X) + 4) * NEW_MV_DATA_LARGE_SIZE >> 2, (mi(Y) + 4) >> 2, new_mv_data);
        sad = new_mv_data(0, 1) >> 2;
        sad_min = cm_min<uint>(sad_min, sad);
        sad_max = cm_max<uint>(sad_max, sad);
        ssad += sad;

        float svar = (float)sad_min / (float)sad_max;
        const float SVT = 0.67f;
        if (svar > SVT) return SPLIT_TRY_EE;
        float sr = (float)ssad / (float)bsad;
        float SRT = (qidx < 3) ? 0.65f : 0.55f;
        if (sr > SRT) return SPLIT_TRY_EE;*/
    } else {
        matrix<uint4, 1, 2>   new_mv_data;
        read(MV(SIZE16 + 0), mi(X) * NEW_MV_DATA_SMALL_SIZE >> 1, mi(Y) >> 1, new_mv_data);
        uint sad = new_mv_data(0, 1) >> 2;
        sad_min = sad;
        sad_max = sad;
        ssad = sad;
        read(MV(SIZE16 + 0), (mi(X) + 2) * NEW_MV_DATA_SMALL_SIZE >> 1, mi(Y) >> 1, new_mv_data);
        sad = new_mv_data(0, 1) >> 2;
        sad_min = cm_min<uint>(sad_min, sad);
        sad_max = cm_max<uint>(sad_max, sad);
        ssad += sad;
        read(MV(SIZE16 + 0), mi(X) * NEW_MV_DATA_SMALL_SIZE >> 1, (mi(Y) + 2) >> 1, new_mv_data);
        sad = new_mv_data(0, 1) >> 2;
        sad_min = cm_min<uint>(sad_min, sad);
        sad_max = cm_max<uint>(sad_max, sad);
        ssad += sad;
        read(MV(SIZE16 + 0), (mi(X) + 2) * NEW_MV_DATA_SMALL_SIZE >> 1, (mi(Y) + 2) >> 1, new_mv_data);
        sad = new_mv_data(0, 1) >> 2;
        sad_min = cm_min<uint>(sad_min, sad);
        sad_max = cm_max<uint>(sad_max, sad);
        ssad += sad;

        float svar = (float)sad_min / (float)sad_max;
        const float SVT = 0.5f;
        if (svar > SVT) return SPLIT_TRY_EE;
        float sr = (float)ssad / (float)bsad;
        float SRT = (qidx < 3) ? 0.65f : 0.55f;
        if (sr > SRT) return SPLIT_TRY_EE;
    }

    switch (qidx) {
    case 0:
        return (IsBadPred0(scpp, sadpp, mvd, l)) ? SPLIT_MUST : SPLIT_TRY_NE;
        break;
    case 1:
        return (IsBadPred1(scpp, sadpp, mvd, l)) ? SPLIT_MUST : SPLIT_TRY_NE;
        break;
    case 2:
        return (IsBadPred2(scpp, sadpp, mvd, l)) ? SPLIT_MUST : SPLIT_TRY_NE;
        break;
    case 3:
        return (IsBadPred3(scpp, sadpp, mvd, l)) ? SPLIT_MUST : SPLIT_TRY_NE;
        break;
    default:
        return SPLIT_TRY_NE;
        break;
    }
    return SPLIT_TRY_NE;
}

_GENX_ uint
    decide32(SurfaceIndex PARAM, SurfaceIndex SRC, vector<SurfaceIndex,7> REF, vector<SurfaceIndex,8> MV,
             SurfaceIndex MODE_INFO, vector<uint2,2> mi, vector<SurfaceIndex, 4> RSCS, uint qpLayer, uint1 split64)
{
    if (is_outsize(mi)) {
        g_mi = 0;
        set_sb_type(g_mi, BLOCK_32X32);
        set_mode(g_mi, OUT_OF_PIC);
        write_mode_info_8(MODE_INFO, mi);
        return 0;
    }

    vector<uint1,TXB_CTX_SIZE> coef_ctx_orig = g_coef_ctx;
    uint cost_split = 0;
    uint1 split32 = SPLIT_TRY_NE;
    uint cost_non_split = INT_MAX;
    uint1 skipee = (qpLayer & 0x3) > 1 ? 1 : 0;

    if (!is_outsize(mi + 3)) {
        g_mi = 0;
        set_sb_type(g_mi, BLOCK_32X32);
        set_mode(g_mi, NEWMV);

        get_mvrefs(PARAM, MODE_INFO, SIZE32, mi);

        read(MV(SIZE32 + 0), mi(X) * NEW_MV_DATA_LARGE_SIZE >> 2, mi(Y) >> 2, g_new_mv_data.select<1, 1, 2, 1>(0));
        read(MV(SIZE32 + 4), mi(X) * NEW_MV_DATA_LARGE_SIZE >> 2, mi(Y) >> 2, g_new_mv_data.select<1, 1, 2, 1>(1));

        if(split64 == SPLIT_MUST)
            split32 = SplitIEF(MV, RSCS, mi, qpLayer, 0);

        if (split32 != SPLIT_MUST) {

            uint pred_idx = 2 + SIZE32;
            uint scratch_idx = SCRATCH;

            uint bits_non_split;

            get_newmv_cost_and_bits(cost_non_split, bits_non_split);
            //if (get_comp_flag()) debug_printf("(1,%2d,%2d) NEWMV ref=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), get_ref_comb(g_mi),
            //    get_mv_pair(g_mi)(0), get_mv_pair(g_mi)(1), get_mv_pair(g_mi)(2), get_mv_pair(g_mi)(3), cost_non_split, bits_non_split);

            vector<int4,2> pel = mi * MI_SIZE;
            vector<int2,2> pel_minus2 = pel - 2;

            uint2 cur_refmv_count = g_stack_size(0);
            for (uint2 ref = 0; ref < 2; ref++) {
                for (uint2 i = 0; i < cur_refmv_count; i++) {
                    vector_ref<int2,2> mv = g_refmv.select<2,1>();
                    if (mv.format<uint4>()(0) != 0) {
                        matrix<uint1,16,16> src;

                        vector<uint2,16> sad_acc = 0;

                        vector<int2,2> pos = pel_minus2 + cm_asr<int2>(mv, 3);
                        matrix<int1,2,4> filt;
                        vector<uint2,2> dmv = cm_asr<uint2>(mv, 1) & 3;
                        filt.format<uint4>() = g_interp_filters.format<uint4>().iselect(dmv);

                        vector<uint2,2> blk;
                        for (blk(Y) = 0; blk(Y) < 32; blk(Y) += 16) {
                            for (blk(X) = 0; blk(X) < 32; blk(X) += 16) {
                                interpolate16(REF(ref), pos + blk, filt);
                                write(REF(scratch_idx), pel(X) + blk(X), pel(Y) + blk(Y), g_pred16);
                                read (SRC,              pel(X) + blk(X), pel(Y) + blk(Y), src);
                                sad_acc = cm_add<uint2>(sad16_acc(src, g_pred16), sad_acc, SAT);
                            }
                        }
                        uint4 sad = cm_sum<uint>(sad_acc.select<8,2>()) << 11;
                        uint2 bits = g_bits_mode(0,0);
                        uint4 cost = sad + bits * get_lambda_int();
                        //if (get_comp_flag()) debug_printf("(1,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), ref, i, mv(0), mv(1), cost, bits);

                        if (cost_non_split > cost) {
                            cost_non_split = cost;
                            bits_non_split = bits;
                            set_mv0(g_mi, mv);
                            set_mv1(g_mi, 0);
                            set_ref_single(g_mi, ref << get_comp_flag());
                            set_mode(g_mi, NEARESTMV);
                            set_sad(g_mi, sad);
                            swap_idx(pred_idx, scratch_idx);
                        }
                    }
                    g_refmv.select<8,1>(0) = g_refmv.select<8,1>(2);
                    g_bits_mode.row(0).select<4,1>(0) = g_bits_mode.row(0).select<4,1>(1);
                }
                cur_refmv_count = g_stack_size(1);
                g_refmv.select<8,1>(0) = g_refmv.select<8,1>(8);
                g_bits_mode.row(0).select<4,1>(0) = g_bits_mode.row(1).select<4,1>(0);
            }

            for (uint2 i = 0; i < g_stack_size(2); i++) {
                vector_ref<int2,4> mv = g_refmv.select<4,1>(16);
                if ((mv != 0).any()) {

                    vector<uint2,16> sad_acc = 0;

                    vector<int2,4> pos = pel_minus2.replicate<2>() + cm_asr<int2>(mv, 3);
                    matrix<int1,4,4> filt;
                    vector<uint2,4> dmv = cm_asr<uint2>(mv, 1) & 3;
                    filt.format<uint4>() = g_interp_filters.format<uint4>().iselect(dmv);

                    vector<uint2,2> blk;
                    for (blk(Y) = 0; blk(Y) < 32; blk(Y) += 16) {
                        for (blk(X) = 0; blk(X) < 32; blk(X) += 16) {
                            matrix<uint1,16,16> src, pred0;
                            interpolate16(REF(0), pos.select<2,1>(0) + blk, filt.select<2,1,4,1>(0));
                            pred0 = g_pred16;
                            interpolate16(REF(1), pos.select<2,1>(2) + blk, filt.select<2,1,4,1>(2));
                            g_pred16 = cm_avg<uint1>(pred0, g_pred16);
                            write(REF(scratch_idx), pel(X) + blk(X), pel(Y) + blk(Y), g_pred16);
                            read (SRC,              pel(X) + blk(X), pel(Y) + blk(Y), src);
                            sad_acc = cm_add<uint2>(sad16_acc(src, g_pred16), sad_acc, SAT);
                        }
                    }
                    uint4 sad = cm_sum<uint>(sad_acc.select<8,2>()) << 11;
                    uint2 bits = g_bits_mode(2,0);
                    uint4 cost = sad + bits * get_lambda_int();
                    //if (get_comp_flag()) debug_printf("(1,%2d,%2d) REFMV ref=2 mode=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), i, mv(0), mv(1), mv(2), mv(3), cost, bits);

                    if (cost_non_split > cost) {
                        cost_non_split = cost;
                        bits_non_split = bits;
                        set_mv_pair(g_mi, mv);
                        set_ref_bidir(g_mi);
                        set_mode(g_mi, NEARESTMV);
                        set_sad(g_mi, sad);
                        swap_idx(pred_idx, scratch_idx);
                    }
                }
                g_refmv.select<8,1>(16) = g_refmv.select<8,1>(20);
                g_refmv.select<8,1>(24) = g_refmv.select<8,1>(28);
                g_bits_mode.row(2).select<4,1>(0) = g_bits_mode.row(2).select<4,1>(1);
            }


            vector<uint2,2> loc = mi >> 2; loc &= 1;
            uint4 zeromv_data = g_zeromv_sad_32(loc(Y), loc(X));
            uint4 zeromv_sad = (zeromv_data >> 2) << 11;
            uint1 zeromv_ref = zeromv_data & 3;
            uint2 zeromv_bits = g_bits_mode(zeromv_ref, MODE_IDX_ZEROMV);
            uint4 zeromv_cost = zeromv_sad + zeromv_bits * get_lambda_int();
            //if (get_comp_flag()) debug_printf("(1,%2d,%2d) ZEROMV ref=%d cost=%u bits=%u\n", mi(Y), mi(X), zeromv_ref, zeromv_cost, zeromv_bits);

            matrix<uint1,8,32> src, ref0, ref1;
            if (cost_non_split > zeromv_cost) {
                cost_non_split = zeromv_cost;
                bits_non_split = zeromv_bits;
                zero_mv_pair(g_mi);
                set_mode(g_mi, ZEROMV);
                set_ref_single(g_mi, (zeromv_ref & 1) << get_comp_flag());
                set_sad(g_mi, zeromv_sad);

                if (zeromv_ref == 2) {
                    set_ref1(g_mi, ALTR);
                    set_ref_comb(g_mi, COMP);
                    for (int i = 0; i < 32; i += 8) {
                        read (REF(0),        pel(X), pel(Y) + i, ref0);
                        read (REF(1),        pel(X), pel(Y) + i, ref1);
                        write(REF(pred_idx), pel(X), pel(Y) + i, matrix<uint1,8,32>(cm_avg<uint1>(ref0, ref1)));
                    }
                } else
                    pred_idx = zeromv_ref;
            }

            bits_non_split += BITS_IS_INTER;
            bits_non_split += get_interp_filter_bits();

            g_coef_ctx = coef_ctx_orig;

            g_rd_cost[0] = 0;
            g_rd_cost[1] = 0;
            //g_rd_cost[1] = bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE32];
            g_skip_cost = uint(get_lambda() * (bits_non_split + g_bits_skip[1]) + 0.5f);
            vector<uint2,2> blk;
            for (blk(Y) = 0; blk(Y) < 4; blk(Y)++) {
                for (blk(X) = 0; blk(X) < 4; blk(X)++) {
                    matrix<uint1,8,8> src, pred;
                    read(SRC,                     pel(X) + blk(X) * 8, pel(Y) + blk(Y) * 8, src);
                    read(MODIFIED(REF(pred_idx)), pel(X) + blk(X) * 8, pel(Y) + blk(Y) * 8, pred);
                    g_qcoef = src - pred;
                    get_rd_cost_8(PARAM, 0, mi + blk);
                }
            }
            //cost_non_split = g_rd_cost[0] + uint(get_lambda() * g_rd_cost[1] + 0.5f);
            cost_non_split = g_rd_cost[0] + uint(get_lambda() * (bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE32] + ((g_rd_cost[1]*3)>>2)) + 0.5f);

            //if (get_comp_flag()) debug_printf("(1,%2d,%2d) skip_cost=%u skip_bits=%u\n", mi(Y), mi(X), g_skip_cost, bits_non_split + g_bits_skip[1]);
            //if (get_comp_flag()) debug_printf("(1,%2d,%2d) non_skip_cost=%u non_skip_bits=%u\n", mi(Y), mi(X), cost_non_split, g_rd_cost[1]);

            if (cost_non_split >= g_skip_cost) {
                cost_non_split = g_skip_cost;
                set_skip(g_mi, 1);
                set_coef_ctx<4>(mi, 0);

                if (skipee) {
                    write_mode_info_32(MODE_INFO, mi);
                    debug_printf("1,%d,%d cost=%u\n", mi(Y), mi(X), cost_non_split);
                    return cost_non_split;
                }

            } else {
                cost_non_split = g_rd_cost[0] + uint(get_lambda() * (bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE32] + g_rd_cost[1]) + 0.5f);
            }

            debug_printf("1,%d,%d cost=%u\n", mi(Y), mi(X), cost_non_split);
            // test icra paritioning
            //write_mode_info_32(MODE_INFO, mi);
            //return cost_non_split;
        }
    }
    vector<uint1, TXB_CTX_SIZE> coef_ctx_saved = g_coef_ctx;
    g_mi_saved32 = g_mi;

    mode_info miq0, miq1, miq2, miq3;

    {
        g_coef_ctx = coef_ctx_orig;

        cost_split += decide16(PARAM, SRC, REF, MV, MODE_INFO, BLOCK0(mi, 2), (qpLayer & 0x3) || (split64 != SPLIT_MUST));
        miq0 = g_mi;
        cost_split += decide16(PARAM, SRC, REF, MV, MODE_INFO, BLOCK1(mi, 2), (qpLayer & 0x3) || (split64 != SPLIT_MUST));
        miq1 = g_mi;
        cost_split += decide16(PARAM, SRC, REF, MV, MODE_INFO, BLOCK2(mi, 2), (qpLayer & 0x3) || (split64 != SPLIT_MUST));
        miq2 = g_mi;
        cost_split += decide16(PARAM, SRC, REF, MV, MODE_INFO, BLOCK3(mi, 2), (qpLayer & 0x3) || (split64 != SPLIT_MUST));
        miq3 = g_mi;
    }

    if (cost_split < cost_non_split) {
#ifdef JOIN_MI_INLOOP
        g_mi = miq0;
        // Join MI
        const int qsbtype = BLOCK_16X16;
        while (get_sb_type(miq0) == qsbtype) {
            uint1 skip = get_skip(miq0);
            if (get_mode(miq1) == 255 || not_joinable(miq0, miq1)) break;
            skip &= get_skip(miq1);

            if (get_mode(miq2) == 255 || not_joinable(miq0, miq2)) break;
            skip &= get_skip(miq2);

            if (not_joinable(miq0, miq3)) break;
            skip &= get_skip(miq3);

            set_skip(g_mi, skip);
            set_sb_type(g_mi, BLOCK_32X32);
            uint4 sad = get_sad(miq0);
            sad += get_sad(miq1);
            sad += get_sad(miq2);
            sad += get_sad(miq3);
            set_sad(g_mi, sad);
            write_mode_info_32(MODE_INFO, mi);
            break;
        }
#endif

        return cost_split;
    }

    g_coef_ctx = coef_ctx_saved;
    g_mi = g_mi_saved32;

    write_mode_info_32(MODE_INFO, mi);
    return cost_non_split;
}

_GENX_ inline void
    decide64(SurfaceIndex PARAM, SurfaceIndex SRC, vector<SurfaceIndex,7> REF, vector<SurfaceIndex,8> MV,
             SurfaceIndex MODE_INFO, vector<uint2,2> mi, vector<SurfaceIndex, 4> RSCS, uint qpLayer)
{
    if (is_outsize(mi)) {
        g_mi = 0;
        set_sb_type(g_mi, BLOCK_16X16);
        set_mode(g_mi, OUT_OF_PIC);
        write_mode_info_8(MODE_INFO, mi);
        return;
    }
    vector<uint1,TXB_CTX_SIZE> coef_ctx_orig = g_coef_ctx;
    uint cost_split = 0;
    uint1 split64 = SPLIT_TRY_NE;
    uint cost_non_split = INT_MAX;
    if (!is_outsize(mi + 7)) {
        g_mi = 0;
        set_sb_type(g_mi, BLOCK_64X64);
        set_mode(g_mi, NEWMV);

        get_mvrefs(PARAM, MODE_INFO, SIZE64, mi);

        read(MV(SIZE64 + 0), mi(X) * NEW_MV_DATA_LARGE_SIZE >> 3, mi(Y) >> 3, g_new_mv_data.select<1, 1, 2, 1>(0));
        read(MV(SIZE64 + 4), mi(X) * NEW_MV_DATA_LARGE_SIZE >> 3, mi(Y) >> 3, g_new_mv_data.select<1, 1, 2, 1>(1));

        split64 = SplitIEF(MV, RSCS, mi, qpLayer, 1);

        if (split64 != SPLIT_MUST) {
            uint pred_idx = 2 + SIZE64;
            uint scratch_idx = SCRATCH;

            uint bits_non_split;

            get_newmv_cost_and_bits(cost_non_split, bits_non_split);
            //if (get_comp_flag()) debug_printf("(0,%2d,%2d) NEWMV ref=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), get_ref_comb(g_mi),
            //    get_mv_pair(g_mi)(0), get_mv_pair(g_mi)(1), get_mv_pair(g_mi)(2), get_mv_pair(g_mi)(3), cost_non_split, bits_non_split);

            vector<int4,2> pel = mi * MI_SIZE;
            vector<int2,2> pel_minus2 = pel - 2;

            uint2 cur_refmv_count = g_stack_size(0);
            for (uint2 ref = 0; ref < 2; ref++) {
                for (uint2 i = 0; i < cur_refmv_count; i++) {
                    vector_ref<int2,2> mv = g_refmv.select<2,1>();
                    if (mv.format<uint4>()(0) != 0) {
                        matrix<uint1,16,16> src;

                        vector<uint2,16> sad_acc = 0;

                        vector<int2,2> pos = pel_minus2 + cm_asr<int2>(mv, 3);
                        matrix<int1,2,4> filt;
                        vector<uint2,2> dmv = cm_asr<uint2>(mv, 1) & 3;
                        filt.format<uint4>() = g_interp_filters.format<uint4>().iselect(dmv);

                        vector<uint2,2> blk;
                        for (blk(Y) = 0; blk(Y) < 64; blk(Y) += 16) {
                            for (blk(X) = 0; blk(X) < 64; blk(X) += 16) {
                                interpolate16(REF(ref), pos + blk, filt);
                                write(REF(scratch_idx), pel(X) + blk(X), pel(Y) + blk(Y), g_pred16);
                                read (SRC,              pel(X) + blk(X), pel(Y) + blk(Y), src);
                                sad_acc = cm_add<uint2>(sad16_acc(src, g_pred16), sad_acc, SAT);
                            }
                        }
                        uint4 sad = cm_sum<uint>(sad_acc.select<8,2>()) << 11;
                        uint2 bits = g_bits_mode(0,0);
                        uint4 cost = sad + bits * get_lambda_int();
                        //if (get_comp_flag()) debug_printf("(0,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), ref, i, mv(0), mv(1), cost, bits);

                        if (cost_non_split > cost) {
                            cost_non_split = cost;
                            bits_non_split = bits;
                            set_mv0(g_mi, mv);
                            set_mv1(g_mi, 0);
                            set_ref_single(g_mi, ref << get_comp_flag());
                            set_mode(g_mi, NEARESTMV);
                            set_sad(g_mi, sad);
                            swap_idx(pred_idx, scratch_idx);
                        }
                    }
                    g_refmv.select<8,1>(0) = g_refmv.select<8,1>(2);
                    g_bits_mode.row(0).select<4,1>(0) = g_bits_mode.row(0).select<4,1>(1);
                }
                cur_refmv_count = g_stack_size(1);
                g_refmv.select<8,1>(0) = g_refmv.select<8,1>(8);
                g_bits_mode.row(0).select<4,1>(0) = g_bits_mode.row(1).select<4,1>(0);
            }

            for (uint2 i = 0; i < g_stack_size(2); i++) {
                vector_ref<int2,4> mv = g_refmv.select<4,1>(16);
                if ((mv != 0).any()) {

                    vector<uint2,16> sad_acc = 0;

                    vector<int2,4> pos = pel_minus2.replicate<2>() + cm_asr<int2>(mv, 3);
                    matrix<int1,4,4> filt;
                    vector<uint2,4> dmv = cm_asr<uint2>(mv, 1) & 3;
                    filt.format<uint4>() = g_interp_filters.format<uint4>().iselect(dmv);

                    vector<uint2,2> blk;
                    for (blk(Y) = 0; blk(Y) < 64; blk(Y) += 16) {
                        for (blk(X) = 0; blk(X) < 64; blk(X) += 16) {
                            matrix<uint1,16,16> src, pred0;
                            interpolate16(REF(0), pos.select<2,1>(0) + blk, filt.select<2,1,4,1>(0));
                            pred0 = g_pred16;
                            interpolate16(REF(1), pos.select<2,1>(2) + blk, filt.select<2,1,4,1>(2));
                            g_pred16 = cm_avg<uint1>(pred0, g_pred16);
                            write(REF(scratch_idx), pel(X) + blk(X), pel(Y) + blk(Y), g_pred16);
                            read (SRC,              pel(X) + blk(X), pel(Y) + blk(Y), src);
                            sad_acc = cm_add<uint2>(sad16_acc(src, g_pred16), sad_acc, SAT);
                        }
                    }
                    uint4 sad = cm_sum<uint>(sad_acc.select<8,2>()) << 11;
                    uint2 bits = g_bits_mode(2,0);
                    uint4 cost = sad + bits * get_lambda_int();
                    //if (get_comp_flag()) debug_printf("(0,%2d,%2d) REFMV ref=2 mode=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n", mi(Y), mi(X), i, mv(0), mv(1), mv(2), mv(3), cost, bits);

                    if (cost_non_split > cost) {
                        cost_non_split = cost;
                        bits_non_split = bits;
                        set_mv_pair(g_mi, mv);
                        set_ref_bidir(g_mi);
                        set_mode(g_mi, NEARESTMV);
                        set_sad(g_mi, sad);
                        swap_idx(pred_idx, scratch_idx);
                    }
                }
                g_refmv.select<8,1>(16) = g_refmv.select<8,1>(20);
                g_refmv.select<8,1>(24) = g_refmv.select<8,1>(28);
                g_bits_mode.row(2).select<4,1>(0) = g_bits_mode.row(2).select<4,1>(1);
            }


            uint4 zeromv_sad  = (g_zeromv_sad_64 >> 2) << 11;
            uint1 zeromv_ref  = g_zeromv_sad_64 & 3;
            uint2 zeromv_bits = g_bits_mode(zeromv_ref, MODE_IDX_ZEROMV);
            uint4 zeromv_cost = zeromv_sad + zeromv_bits * get_lambda_int();
            //if (get_comp_flag()) debug_printf("(0,%2d,%2d) ZEROMV ref=%d cost=%u bits=%u\n", mi(Y), mi(X), zeromv_ref, zeromv_cost, zeromv_bits);

            if (cost_non_split > zeromv_cost) {
                matrix<uint1,8,32> src, ref0, ref1;
                cost_non_split = zeromv_cost;
                bits_non_split = zeromv_bits;
                zero_mv_pair(g_mi);
                set_mode(g_mi, ZEROMV);
                set_ref_single(g_mi, (zeromv_ref & 1) << get_comp_flag());
                set_sad(g_mi, zeromv_sad);

                if (zeromv_ref == 2) {
                    set_ref1(g_mi, ALTR);
                    set_ref_comb(g_mi, COMP);
                    for (int i = 0; i < 64; i += 8) {
                        for (int j = 0; j < 64; j += 32) {
                            read (REF(0),        pel(X) + j, pel(Y) + i, ref0);
                            read (REF(1),        pel(X) + j, pel(Y) + i, ref1);
                            write(REF(pred_idx), pel(X) + j, pel(Y) + i, matrix<uint1,8,32>(cm_avg<uint1>(ref0, ref1)));
                        }
                    }
                } else
                    pred_idx = zeromv_ref;
            }

            bits_non_split += BITS_IS_INTER;
            bits_non_split += get_interp_filter_bits();

            g_coef_ctx = coef_ctx_orig;

            g_rd_cost[0] = 0;
            g_rd_cost[1] = 0;
            //g_rd_cost[1] = bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE64];
            g_skip_cost = uint(get_lambda() * (bits_non_split + g_bits_skip[1]) + 0.5f);
            vector<uint2,2> blk;
            for (blk(Y) = 0; blk(Y) < 8; blk(Y)++) {
                for (blk(X) = 0; blk(X) < 8; blk(X)++) {
                    matrix<uint1,8,8> src, ref0;
                    read(SRC,                     pel(X) + blk(X) * 8, pel(Y) + blk(Y) * 8, src);
                    read(MODIFIED(REF(pred_idx)), pel(X) + blk(X) * 8, pel(Y) + blk(Y) * 8, ref0);
                    g_qcoef = src - ref0;
                    get_rd_cost_8(PARAM, 0, mi + blk);
                }
            }
            //cost_non_split = g_rd_cost[0] + uint(get_lambda() * g_rd_cost[1] + 0.5f);
            cost_non_split = g_rd_cost[0] + uint(get_lambda() * (bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE64] + ((g_rd_cost[1]*3)>>2)) + 0.5f);

            //if (get_comp_flag()) debug_printf("(0,%2d,%2d) skip_cost=%u skip_bits=%u\n", mi(Y), mi(X), g_skip_cost, bits_non_split + g_bits_skip[1]);
            //if (get_comp_flag()) debug_printf("(0,%2d,%2d) non_skip_cost=%u non_skip_bits=%u\n", mi(Y), mi(X), cost_non_split, g_rd_cost[1]);

            if (cost_non_split >= g_skip_cost) {
                cost_non_split = g_skip_cost;
                set_skip(g_mi, 1);
                set_coef_ctx<8>(mi, 0);
            } else {
                cost_non_split = g_rd_cost[0] + uint(get_lambda() * (bits_non_split + g_bits_skip[0] + g_bits_tx_size_and_type[SIZE64] + g_rd_cost[1]) + 0.5f);
            }

            debug_printf("0,%d,%d cost=%u sad=%d\n", mi(Y), mi(X), cost_non_split, get_sad(g_mi));
            // test icra paritioning
            //write_mode_info_64(MODE_INFO, mi);
            //return;
        }
    }

    vector<uint1, TXB_CTX_SIZE> coef_ctx_saved = g_coef_ctx;
    g_mi_saved64 = g_mi;

    mode_info miq0, miq1, miq2, miq3;
    {
        g_coef_ctx = coef_ctx_orig;

        cost_split += decide32(PARAM, SRC, REF, MV, MODE_INFO, BLOCK0(mi, 4), RSCS, qpLayer, split64);
        miq0 = g_mi;
        cost_split += decide32(PARAM, SRC, REF, MV, MODE_INFO, BLOCK1(mi, 4), RSCS, qpLayer, split64);
        miq1 = g_mi;
        cost_split += decide32(PARAM, SRC, REF, MV, MODE_INFO, BLOCK2(mi, 4), RSCS, qpLayer, split64);
        miq2 = g_mi;
        cost_split += decide32(PARAM, SRC, REF, MV, MODE_INFO, BLOCK3(mi, 4), RSCS, qpLayer, split64);
        miq3 = g_mi;
    }

    if (cost_split < cost_non_split) {
#ifdef JOIN_MI_INLOOP
        g_mi = miq0;
        // Join MI
        const int qsbtype = BLOCK_32X32;
        while (get_sb_type(miq0) == qsbtype) {
            uint1 skip = get_skip(miq0);
            if (get_mode(miq1) == 255 || not_joinable(miq0, miq1)) break;
            skip &= get_skip(miq1);

            if (get_mode(miq2) == 255 || not_joinable(miq0, miq2)) break;
            skip &= get_skip(miq2);

            if (not_joinable(miq0, miq3)) break;
            skip &= get_skip(miq3);

            set_skip(g_mi, skip);
            set_sb_type(g_mi, BLOCK_64X64);
            uint4 sad = get_sad(miq0);
            sad += get_sad(miq1);
            sad += get_sad(miq2);
            sad += get_sad(miq3);
            set_sad(g_mi, sad);
            write_mode_info_64(MODE_INFO, mi);
            break;
        }
#endif
        return;
    }

    g_coef_ctx = coef_ctx_saved;
    g_mi = g_mi_saved64;

    write_mode_info_64(MODE_INFO, mi);
    return;
}

_GENX_ inline vector<uint2,16> zeromv_sad(matrix_ref<uint1,8,16> src, matrix_ref<uint1,8,16> ref)
{
    vector<uint2,16> sad = cm_sad2<uint2>(src.row(0), ref.row(0));
    #pragma unroll
    for (int k = 1; k < 8; k++)
        sad = cm_sada2<uint2>(src.row(k), ref.row(k), sad);
    return sad;
}

_GENX_ inline vector<uint2,16> zeromv_sad(matrix_ref<uint1,8,16> src, matrix_ref<uint1,8,16> ref0, matrix_ref<uint1,8,16> ref1)
{
    vector<uint2,16> sad = cm_sad2<uint2>(src.row(0), cm_avg<uint1>(ref0.row(0), ref1.row(0)));
    #pragma unroll
    for (int k = 1; k < 8; k++)
        sad = cm_sada2<uint2>(src.row(k), cm_avg<uint1>(ref0.row(k), ref1.row(k)), sad);
    return sad;
}

template <typename RT, typename T, uint R, uint C> _GENX_ inline matrix<RT,R/2,C/2> add_2x2(matrix_ref<T,R,C> m)
{
    const uint CS = C==2 ? 1 : 2;
    matrix<RT,R,C/2> tmp = m.template select<R,1,C/2,CS>(0,0) + m.template select<R,1,C/2,CS>(0,1);
    return tmp.template select<R/2,2,C/2,1>(0,0) + tmp.template select<R/2,2,C/2,1>(1,0);
}

_GENX_ inline void calculate_zeromv_sad(SurfaceIndex SRC, vector<SurfaceIndex,7> REF, vector<uint2,2> mi)
{
    vector<int4,2> pel = mi * MI_SIZE;
    matrix<uint1,8,32> src, ref0, ref1;
    matrix<uint2,24,8> sad8;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 2; j++) {
            read(SRC,    pel(X) + j * 32, pel(Y) + i * 8, src);
            read(REF(0), pel(X) + j * 32, pel(Y) + i * 8, ref0);
            read(REF(1), pel(X) + j * 32, pel(Y) + i * 8, ref1);

            matrix<uint2,3,32> sad_acc;
            sad_acc.select<1,1,16,1>(0,0)  = zeromv_sad(src.select<8,1,16,1>(0,0),  ref0.select<8,1,16,1>(0,0));
            sad_acc.select<1,1,16,1>(0,16) = zeromv_sad(src.select<8,1,16,1>(0,16), ref0.select<8,1,16,1>(0,16));
            sad_acc.select<1,1,16,1>(1,0)  = zeromv_sad(src.select<8,1,16,1>(0,0),  ref1.select<8,1,16,1>(0,0));
            sad_acc.select<1,1,16,1>(1,16) = zeromv_sad(src.select<8,1,16,1>(0,16), ref1.select<8,1,16,1>(0,16));
            sad_acc.select<1,1,16,1>(2,0)  = zeromv_sad(src.select<8,1,16,1>(0,0),  ref0.select<8,1,16,1>(0,0),  ref1.select<8,1,16,1>(0,0));
            sad_acc.select<1,1,16,1>(2,16) = zeromv_sad(src.select<8,1,16,1>(0,16), ref0.select<8,1,16,1>(0,16), ref1.select<8,1,16,1>(0,16));

            vector<uint2,3*8> sad_tmp;
            sad_tmp = sad_acc.select<3,1,8,4>(0,0) + sad_acc.select<3,1,8,4>(0,2);
            sad_tmp.select<12,1>() = sad_tmp.select<12,2>(0) + sad_tmp.select<12,2>(1);

            sad8.select<3,8,4,1>(i,j*4) = sad_tmp.select<12,1>();
        }
    }

    const uint2 MAX_SAD_8X8 = 255 * 8 * 8;
    //const uint2 mask = ~(-get_comp_flag());
    const uint mask = get_comp_flag() ? 0: 0xffff;
    sad8.select<2,1,8,1>(16,0).merge(MAX_SAD_8X8 + 1, mask);
    sad8.select<2,1,8,1>(18,0).merge(MAX_SAD_8X8 + 1, mask);
    sad8.select<2,1,8,1>(20,0).merge(MAX_SAD_8X8 + 1, mask);
    sad8.select<2,1,8,1>(22,0).merge(MAX_SAD_8X8 + 1, mask);

    matrix<uint2,12,4> sad16 = add_2x2<uint2>(sad8.select_all());
    matrix<uint4,6,2>  sad32 = add_2x2<uint4>(sad16.select_all());
    matrix<uint4,3,1>  sad64 = add_2x2<uint4>(sad32.select_all());

    sad8 <<= 2;
    sad8.select<8,1,8,1>(8,0)  += 1;
    sad8.select<8,1,8,1>(16,0) += 2;
    sad8.select<8,1,8,1>(0,0) = cm_min<uint2>(sad8.select<8,1,8,1>(0,0), sad8.select<8,1,8,1>(8,0));
    g_zeromv_sad_8            = cm_min<uint2>(sad8.select<8,1,8,1>(0,0), sad8.select<8,1,8,1>(16,0));

    matrix<uint4,12,4> sad16_int = sad16 << 2;
    sad16_int.select<4,1,4,1>(4,0) += 1;
    sad16_int.select<4,1,4,1>(8,0) += 2;
    sad16_int.select<4,1,4,1>(0,0) = cm_min<uint4>(sad16_int.select<4,1,4,1>(0,0), sad16_int.select<4,1,4,1>(4,0));
    g_zeromv_sad_16                = cm_min<uint4>(sad16_int.select<4,1,4,1>(0,0), sad16_int.select<4,1,4,1>(8,0));

    sad32 <<= 2;
    sad32.select<2,1,2,1>(2,0) += 1;
    sad32.select<2,1,2,1>(4,0) += 2;
    sad32.select<2,1,2,1>(0,0) = cm_min<uint4>(sad32.select<2,1,2,1>(0,0), sad32.select<2,1,2,1>(2,0));
    g_zeromv_sad_32            = cm_min<uint4>(sad32.select<2,1,2,1>(0,0), sad32.select<2,1,2,1>(4,0));

    sad64 <<= 2;
    sad64.select<1,1,1,1>(1,0) += 1;
    sad64.select<1,1,1,1>(2,0) += 2;
    sad64.select<1,1,1,1>(0,0) = cm_min<uint4>(sad64.select<1,1,1,1>(0,0), sad64.select<1,1,1,1>(1,0))(0);
    g_zeromv_sad_64            = cm_min<uint4>(sad64.select<1,1,1,1>(0,0), sad64.select<1,1,1,1>(2,0))(0);
}

extern "C" _GENX_MAIN_ void
    ModeDecision(SurfaceIndex PARAM, SurfaceIndex SRC, vector<SurfaceIndex,7> REF, vector<SurfaceIndex,8> MV,
                 SurfaceIndex MODE_INFO, vector<SurfaceIndex, 4> RSCS, uint qpLayer)
{
    read_param(PARAM);

    g_br_ctx_offset_00_15   = vector<uint1,16>(BR_CTX_OFFSET_00_15);
    g_base_ctx_offset_00_31 = vector<uint1,32>(BASE_CTX_OFFSET_00_31);

    g_scan = vector<uint1,64>(SCAN);

    g_lut_txb_skip_ctx = vector<uint1,32>(LUT_TXB_SKIP_CTX);

    g_interp_filters.row(0) = vector<int1,4>(INTERP_FILTERS_0Q);
    g_interp_filters.row(1) = vector<int1,4>(INTERP_FILTERS_1Q);
    g_interp_filters.row(2) = vector<int1,4>(INTERP_FILTERS_2Q);
    g_interp_filters.row(3) = vector<int1,4>(INTERP_FILTERS_3Q);

    g_bits_interp_filter[0] = 12 + 18;    // single ref, one of neighbors have same ref
    g_bits_interp_filter[1] = 54 + 92;    // single ref, neighbors have different refs
    g_bits_interp_filter[2] = 9 + 26;     // compound, one of neighbors have same ref
    g_bits_interp_filter[3] = 190 + 467;  // compound, neighbors have different refs

    g_bits_tx_size_and_type[SIZE8]  = 220 + 277;
    g_bits_tx_size_and_type[SIZE16] = 416 + 212;
    g_bits_tx_size_and_type[SIZE32] = 221 + 0;
    g_bits_tx_size_and_type[SIZE64] = 221 + 0;

    g_has_top_right = vector<uint1,8>(HAS_TOP_RIGHT);

    g_tile_mi_row_start = get_thread_origin_y() * TILE_HEIGHT_MI;
    g_tile_mi_col_start = get_thread_origin_x() * TILE_WIDTH_MI;
    g_tile_mi_row_end   = cm_min<uint2>(g_tile_mi_row_start + TILE_HEIGHT_MI, get_height());
    g_tile_mi_col_end   = cm_min<uint2>(g_tile_mi_col_start + TILE_WIDTH_MI,  get_width());

    vector<uint2,2> mi;
    mi(Y) = g_tile_mi_row_start;
#ifdef ZERO_NNZ
    g_coef_ctx = 0; // zero above and left ctx
#else
    g_coef_ctx = 1; // predict above and left ctx
#endif
    for (uint2 i = 0; i < TILE_HEIGHT_SB; i++, mi(Y) += 8) {
        mi(X) = g_tile_mi_col_start;
#ifdef ZERO_NNZ
        g_coef_ctx.select<LEFT_CTX_SIZE,1>(LEFT_CTX_OFFSET) = 0; // zero left ctx
#else
        g_coef_ctx.select<LEFT_CTX_SIZE,1>(LEFT_CTX_OFFSET) = 1; // predict left ctx
#endif

        for (uint2 j = 0; j < TILE_WIDTH_SB; j++, mi(X) += 8) {
            calculate_zeromv_sad(SRC, REF, mi);
            decide64(PARAM, SRC, REF, MV, MODE_INFO, mi, RSCS, qpLayer);
        }
    }
}
