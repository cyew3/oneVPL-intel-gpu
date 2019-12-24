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

#if CMRT_EMU
  #define assert_emu assert
#else
  #define assert_emu(...) (void)0
#endif

#define TRYINTRA_ORIG
#define JOIN_MI
//#define COMPOUND_OPT2

#define ENABLE_DEBUG_PRINTF 0
#if ENABLE_DEBUG_PRINTF
  #define debug_printf printf
#else
  #define debug_printf(...) (void)0
#endif

typedef char  int1;
typedef short int2;
typedef int   int4;
typedef unsigned char  uint1;
typedef unsigned short uint2;
typedef unsigned int   uint4;

#ifdef TRYINTRA_ORIG
enum {
    NEW_MV_DATA_SMALL_SIZE = 8,
    NEW_MV_DATA_LARGE_SIZE = 64
};
#endif

static const uint2 SB_SIZE = 64;
static const uint2 MI_SIZE = 8;
enum { X, Y };
enum { COL=X, ROW=Y };

enum {
    KERNEL_WIDTH_SB = 1,
    KERNEL_HEIGHT_SB = 2
};

enum {
    KERNEL_WIDTH_MI = KERNEL_WIDTH_SB  * SB_SIZE / MI_SIZE,
    KERNEL_HEIGHT_MI = KERNEL_HEIGHT_SB * SB_SIZE / MI_SIZE
};

enum { NEARESTMV = 13, NEARMV = 14, ZEROMV = 15, NEWMV = 16, OUT_OF_PIC = 255 };
enum { BLOCK_8X8 = 3, BLOCK_16X16 = 6, BLOCK_32X32 = 9, BLOCK_64X64 = 12, };
enum { SIZE8  = 0, SIZE16 = 1, SIZE32 = 2, SIZE64 = 3, };
enum { LAST = 0, GOLD = 1, ALTR = 2, COMP = 3, NONE = 255, };
enum { SCAN_ROW = 1, SCAN_COL = 2 };

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

//static const uint2 MI_SIZE = 8;
static const uint2 SIZEOF_MODE_INFO = 32;
static const uint2 INDEX_SHIFT = 8; // 3 bits for initial index when sorting weights
static const uint2 REF_CAT_LEVEL = 640 * INDEX_SHIFT;
static const uint2 MAX_REF_MV_STACK_SIZE = 8;
static const  int2 MV_BORDER_AV1 = 128;

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

typedef matrix<uint1,1,SIZEOF_MODE_INFO>      mode_info;
typedef matrix_ref<uint1,1,SIZEOF_MODE_INFO>  mode_info_ref;


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

static const uint1 SEQUENCE_7_TO_0[8] = { 7, 6, 5, 4, 3, 2, 1, 0 };

static const int1 INTERP_FILTERS[4][4] = {
    { /*0, 0,*/  0, 64,  0,  0, /*0, 0*/ },
    { /*0, 1,*/ -7, 55, 19, -5, /*1, 0*/ },
    { /*0, 1,*/ -7, 38, 38, -7, /*1, 0*/ },
    { /*0, 1,*/ -5, 19, 55, -7, /*1, 0*/ },
};

_GENX_ vector<uint1,128>  g_params;
_GENX_ uint1              g_fastMode;
_GENX_ uint2              g_kernel_mi_row_start;
_GENX_ uint2              g_kernel_mi_col_start;
_GENX_ matrix<int1,4,4>   g_interp_filters;
_GENX_ mode_info          g_mode_info;
_GENX_ vector<uint1,8>    g_has_top_right;
_GENX_ matrix<uint1,1,32> g_cand;
_GENX_ vector<int2,64>    g_stack;
_GENX_ vector<uint2,4>    g_stack_size;
_GENX_ vector<uint2,4>    g_stack_size_mask;
_GENX_ vector<uint2,4>    g_has_new_mv;
_GENX_ vector<uint2,4>    g_ref_match;
_GENX_ matrix<uint2,4,8>  g_weights;
_GENX_ vector<int2,32+4>  g_refmv;
_GENX_ vector<uint1,4>    g_neighbor_refs;
_GENX_ vector<uint2,4>    g_bits_ref_frame;
_GENX_ vector<uint2,2>    g_bits_skip;
_GENX_ matrix<uint2,4,8>  g_bits_mode;      // 3 refs, 4 modes
// extended candidates
_GENX_ uint2              g_ext_mvs_count;
_GENX_ vector<int2,48>    g_ext_mvs;
_GENX_ vector<uint1,48>   g_ext_ref;

_GENX_ matrix<uint1,8,8>   g_pred8;
_GENX_ matrix<uint1,16,16> g_pred16;
_GENX_ matrix<uint1,8,8>   g_src8;
_GENX_ matrix<uint1,16,16> g_src16;



_GENX_ inline uint2           get_width     () { return g_params.format<uint2>()[0/2]; }
_GENX_ inline uint2           get_height    () { return g_params.format<uint2>()[2/2]; }
_GENX_ inline vector<uint2,2> get_wh        () { return g_params.format<uint2>().select<2,1>(0); }
_GENX_ inline uint2           get_lambda_int() { return g_params.format<uint2>()[28/2]; }
_GENX_ inline uint1           get_comp_flag () { return g_params.format<uint1>()[30]; }

_GENX_ inline uint1           get_single_ref_flag() { return !g_params.format<uint1>()[30] && g_params.format<uint1>()[31]; }
_GENX_ inline uint1           get_bidir_comp_flag() { return  g_params.format<uint1>()[30] && g_params.format<uint1>()[31]; }

_GENX_ inline vector<int2,2>  get_mv0       (mode_info_ref mi) { return mi.format<uint2>().select<2,1>(0); }
_GENX_ inline vector<int2,2>  get_mv1       (mode_info_ref mi) { return mi.format<uint2>().select<2,1>(2); }
_GENX_ inline vector<int2,4>  get_mv_pair   (mode_info_ref mi) { return mi.format<uint2>().select<4,1>(0); }
_GENX_ inline vector<int2,2>  get_dmv0      (mode_info_ref mi) { return mi.format<uint2>().select<2,1>(4); }
_GENX_ inline vector<int2,2>  get_dmv1      (mode_info_ref mi) { return mi.format<uint2>().select<2,1>(6); }
_GENX_ inline vector<int2,4>  get_dmv_pair  (mode_info_ref mi) { return mi.format<uint2>().select<4,1>(4); }
_GENX_ inline uint4           get_mv0_as_int(mode_info_ref mi) { return mi.format<uint4>()[0]; }
_GENX_ inline uint4           get_mv1_as_int(mode_info_ref mi) { return mi.format<uint4>()[1]; }
_GENX_ inline uint1           get_ref0      (mode_info_ref mi) { return mi.format<uint1>()[16]; }
_GENX_ inline uint1           get_ref1      (mode_info_ref mi) { return mi.format<uint1>()[17]; }
_GENX_ inline vector<uint1,2> get_refs      (mode_info_ref mi) { return mi.format<uint1>().select<2,1>(16); }
_GENX_ inline uint1           get_ref_comb  (mode_info_ref mi) { return mi.format<uint1>()[18]; }
_GENX_ inline uint1           get_sb_type   (mode_info_ref mi) { return mi.format<uint1>()[19]; }
_GENX_ inline uint1           get_mode      (mode_info_ref mi) { return mi.format<uint1>()[20]; }
_GENX_ inline uint1           get_skip      (mode_info_ref mi) { return mi.format<uint1>()[24]; }
_GENX_ inline uint4           get_sad       (mode_info_ref mi) { return mi.format<uint4>()[28/4]; }

_GENX_ inline void set_mv0     (mode_info_ref mi, vector<int2,2> mv) { mi.format<int2>().select<2,1>(0) = mv; }
_GENX_ inline void set_mv1     (mode_info_ref mi, vector<int2,2> mv) { mi.format<int2>().select<2,1>(2) = mv; }
_GENX_ inline void set_mv_pair (mode_info_ref mi, vector<int2,4> mv) { mi.format<int2>().select<4,1>(0) = mv; }
_GENX_ inline void set_dmv0    (mode_info_ref mi, vector<int2,2> dmv) { mi.format<int2>().select<2,1>(4) = dmv; }
_GENX_ inline void set_dmv1    (mode_info_ref mi, vector<int2,2> dmv) { mi.format<int2>().select<2,1>(6) = dmv; }
_GENX_ inline void set_dmv_pair(mode_info_ref mi, vector<int2,4> dmv) { mi.format<int2>().select<4,1>(4) = dmv; }
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
_GENX_ inline void set_ref_bidir(mode_info_ref mi) { mi.format<uint2>()[16/2] = LAST + (ALTR << 8); mi.format<uint1>()[18] = COMP; }

_GENX_ inline uint4 is_outsize(vector<uint2, 2> mi)
{
    return (mi >= get_wh()).any();
}

static const uint OFFSET_LUT_IEF = 1504; // 5072;

_GENX_ inline void read_param(SurfaceIndex PARAM)
{
    assert_emu(g_params.SZ <= 128);
    read(PARAM, 0, g_params);
    vector<uint1, 16> tmp;
    read(PARAM, OFFSET_LUT_IEF - 16, tmp);
    g_fastMode = tmp(12);
}

_GENX_ inline void get_cost_ref_frames(SurfaceIndex PARAM, uint1 ctx)
{
    assert_emu(ctx >= 0 && ctx < 36);
    read(DWALIGNED(PARAM), OFFSET_REF_FRAMES + ctx * 8, g_bits_ref_frame); // offset in bytes
}

_GENX_ inline void get_cost_skip(SurfaceIndex PARAM, uint1 ctx)
{
    assert_emu(ctx >= 0 && ctx <= 2);
    g_bits_skip.format<uint4>() = g_params.format<uint4>()(OFFSET_SKIP / sizeof(uint4) + ctx);
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

template <uint2 ref>
_GENX_ void add_distinct_mv_to_stack(uint4 mv_cand, uint2 weight)
{
#if 1
    uint2 stack_size = g_stack_size[ref];
    uint2 ref_offset = 8 * ref;

    uint2 mask = cm_pack_mask(g_stack.format<uint4>().select<8,1>(ref_offset) == mv_cand);
    uint4 idx = cm_fbl<uint>(mask);

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
    uint2 mask = cm_pack_mask(predicate);
    uint4 idx = cm_fbl<uint>(mask);

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

_GENX_ void add_candT(uint2 weight, uint2 row_or_col)
{
    //uint2 newmv = (get_mode(g_cand) >= NEARESTMV);

    const uint1 ref0 = get_ref0(g_cand);
    if (ref0 == LAST) {
        //g_has_new_mv(0) |= newmv;
        //g_ref_match(0) |= row_or_col;
        add_distinct_mv_to_stack<0>(g_cand.format<uint4>()(0), weight);
        if (get_ref1(g_cand) != NONE) {
           // g_has_new_mv.select<2, 1>(1) |= newmv;
           // g_ref_match.select<2, 1>(1) |= row_or_col;
            add_distinct_mv_to_stack<1>(g_cand.format<uint4>()(1), weight);
            add_distinct_mvpair_to_stack(weight);
        }
    }
    else if (ref0 != NONE) {
        //g_has_new_mv(1) |= newmv;
        //g_ref_match(1) |= row_or_col;
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

_GENX_ inline uint1 get_cand_len(uint1 cand_log2_size8, uint1 curr_log2_size8, uint1 min_cand_log2_size)
{
    cand_log2_size8 = cm_min<uint1>(cand_log2_size8, curr_log2_size8);
    cand_log2_size8 = cm_max<uint1>(cand_log2_size8, min_cand_log2_size);
    return 1 << cand_log2_size8;
}

_GENX_ inline void fetch_extended_candidates(vector<SurfaceIndex,2> MODE_INFO, uint1 size8,
                                             vector_ref<uint2,2> mi, vector_ref<uint2,2> inner_mi_gt0)
{
    g_ext_mvs_count = 0;

    if (mi(Y) > 0) {
        uint2 pass = inner_mi_gt0(Y);
        uint2 x = 0;
        do {
            read(MODIFIED(MODE_INFO(pass)), (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) - 1, g_cand);
            const uint2 len = 1 << (get_sb_type(g_cand) >> 2);
            x += len;
            g_ext_mvs.select<4,1>(g_ext_mvs_count * 2) = get_mv_pair(g_cand);
            g_ext_ref.select<4,1>(g_ext_mvs_count * 2) = get_refs(g_cand).replicate<2,1,2,0>();
            g_ext_mvs_count += 1;
            g_ext_mvs_count += (get_ref1(g_cand) != NONE);
            if (g_ext_mvs_count >= 16) {
                g_ext_mvs_count = 16;
                return;
            }
        } while (x < size8);
    }

    if (mi(X) > 0) {
        uint2 pass = inner_mi_gt0(X);
        uint2 y = 0;
        do {
            read(MODIFIED(MODE_INFO(pass)), (mi(X) - 1) * SIZEOF_MODE_INFO, mi(Y) + y, g_cand);
            const uint2 len = 1 << (get_sb_type(g_cand) >> 2);
            y += len;
            g_ext_mvs.select<4,1>(g_ext_mvs_count * 2) = get_mv_pair(g_cand);
            g_ext_ref.select<4,1>(g_ext_mvs_count * 2) = get_refs(g_cand).replicate<2,1,2,0>();
            g_ext_mvs_count += 1;
            g_ext_mvs_count += (get_ref1(g_cand) != NONE);
            if (g_ext_mvs_count >= 16) {
                g_ext_mvs_count = 16;
                return;
            }
        } while (y < size8);
    }
}

_GENX_ inline uint find_distinct_mv(vector_ref<int2,32> mvs, vector_ref<int2,2> not_equal_to_this) {
    return cm_fbl<uint>(cm_pack_mask(mvs.format<uint4>().select<16,1>(0) != not_equal_to_this.format<uint4>()(0)));
}

_GENX_ inline void extend_single_ref_stack(uint1 ref, vector_ref<int2,32> ext_mvs)
{
    if (g_stack_size(ref) < 2) {
        uint2 i = 0;
        if (g_stack_size(ref) == 0) {
            g_stack.format<uint4>()(8*ref) = ext_mvs.format<uint4>()(0);
            g_stack_size(ref) = 1;
            i = 1;
        }

        uint idx = find_distinct_mv(ext_mvs, g_stack.select<2,1>(16*ref));
        if (idx < g_ext_mvs_count) {
            g_stack.format<uint4>()(8*ref+1) = ext_mvs.format<uint4>()(idx);
            g_stack_size(ref) = 2;
        }
    }
}

_GENX_ inline void extend_comp_ref_stack_uni(vector_ref<int2,32> ext_mvs_for_ref0)
{
    if (get_comp_flag() && g_stack_size(2) < 2) {
        enum { LAST_FROM_LAST, LAST_FROM_ALTR, ALTR_FROM_ALTR, ALTR_FROM_LAST };
        matrix<int2,4,2> diff_ref_mvs = 0;
        vector<uint2,4> count = 0;
        matrix<int2,4,4> comp_list;

        uint2 i = 0;
        do {
            vector<int2,2> ext_mv;
            ext_mv.format<uint4>() = ext_mvs_for_ref0.format<uint4>()(i);
            uint1 ext_ref = g_ext_ref(2*i);

            if (ext_ref == LAST && count(LAST_FROM_LAST) < 2)
                comp_list.select<1,1,2,1>(count(LAST_FROM_LAST)++, 0) = ext_mv;
            else if (count(LAST_FROM_ALTR) < 2)
                diff_ref_mvs.row(0 + count(LAST_FROM_ALTR)++) = ext_mv;

            if (ext_ref == ALTR && count(ALTR_FROM_ALTR) < 2)
                comp_list.select<1,1,2,1>(count(ALTR_FROM_ALTR)++, 2) = ext_mv;
            else if (count(ALTR_FROM_LAST) < 2)
                diff_ref_mvs.row(2 + count(ALTR_FROM_LAST)++) = ext_mv;

            i++;
        } while (i < g_ext_mvs_count);

        comp_list.select<2,1,2,1>(count(LAST_FROM_LAST),0) = diff_ref_mvs.select<2,1,2,1>(0,0);
        comp_list.select<2,1,2,1>(count(ALTR_FROM_ALTR),2) = diff_ref_mvs.select<2,1,2,1>(2,0);

        if (g_stack_size(2) == 0)
            g_stack.select<8,1>(32) = comp_list.select<2,1,4,1>();
        else if ((g_stack.select<4,1>(32) == comp_list.row(0)).all())
            g_stack.select<4,1>(36) = comp_list.row(1);
        else
            g_stack.select<4,1>(36) = comp_list.row(0);

        g_stack_size(2) = 2;
    }
}

_GENX_ inline void extend_comp_ref_stack(vector_ref<int2, 32> ext_mvs_for_ref0)
{
    if (get_comp_flag() && g_stack_size(2) < 2) {
        enum { LAST_FROM_LAST, LAST_FROM_ALTR, ALTR_FROM_ALTR, ALTR_FROM_LAST };
        matrix<int2, 4, 2> diff_ref_mvs = 0;
        vector<uint2, 4> count = 0;
        matrix<int2, 4, 4> comp_list;

        uint2 i = 0;
        do {
            vector<int2, 2> ext_mv;
            ext_mv.format<uint4>() = ext_mvs_for_ref0.format<uint4>()(i);
            uint1 ext_ref = g_ext_ref(2 * i);

            if (ext_ref == LAST && count(LAST_FROM_LAST) < 2)
                comp_list.select<1, 1, 2, 1>(count(LAST_FROM_LAST)++, 0) = ext_mv;
            else if (count(LAST_FROM_ALTR) < 2)
                diff_ref_mvs.row(0 + count(LAST_FROM_ALTR)++) = ext_mv;

            if (ext_ref == ALTR && count(ALTR_FROM_ALTR) < 2)
                comp_list.select<1, 1, 2, 1>(count(ALTR_FROM_ALTR)++, 2) = -ext_mv;
            else if (count(ALTR_FROM_LAST) < 2)
                diff_ref_mvs.row(2 + count(ALTR_FROM_LAST)++) = -ext_mv;

            i++;
        } while (i < g_ext_mvs_count);

        comp_list.select<2, 1, 2, 1>(count(LAST_FROM_LAST), 0) = diff_ref_mvs.select<2, 1, 2, 1>(0, 0);
        comp_list.select<2, 1, 2, 1>(count(ALTR_FROM_ALTR), 2) = diff_ref_mvs.select<2, 1, 2, 1>(2, 0);

        if (g_stack_size(2) == 0)
            g_stack.select<8, 1>(32) = comp_list.select<2, 1, 4, 1>();
        else if ((g_stack.select<4, 1>(32) == comp_list.row(0)).all())
            g_stack.select<4, 1>(36) = comp_list.row(1);
        else
            g_stack.select<4, 1>(36) = comp_list.row(0);

        g_stack_size(2) = 2;
    }
}

_GENX_ void get_mvrefs(SurfaceIndex PARAM, vector<SurfaceIndex,2> MODE_INFO, uint1 log2_size8, vector<uint2,2> mi, uint pyramidLayer)
{
    const uint1 min_cand_log2_size = cm_add<uint1>(log2_size8, -2, SAT);
    const uint1 size8 = 1 << log2_size8;

    g_stack = 0;
    g_stack_size = 0;
    g_has_new_mv = 0;
    g_ref_match = 0;
    g_weights = 0;

    g_ext_mvs_count = 0;

    uint1 ctx_ref_frame = 0;
    uint1 ctx_skip = 0;
    g_neighbor_refs = NONE;

    vector<uint2,2> inner_mi = mi & 7;
    inner_mi(Y) = mi(Y) & (KERNEL_HEIGHT_MI - 1);
    inner_mi(X) = mi(X) & (KERNEL_WIDTH_MI  - 1);
    vector<uint2,2> inner_mi_gt0 = inner_mi > 0;
    vector<uint2,2> inner_mi_gt1 = inner_mi > 1;
    vector<uint2,2> inner_mi_gt2 = inner_mi > 2;

    uint1 processed_rows = 1;
    uint1 processed_cols = 1;

    // First above row
    if (mi(Y) > 0) {
        uint2 pass = inner_mi_gt0(Y);
        uint2 x = 0;
        do {
            read(MODIFIED(MODE_INFO(pass)), (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) - 1, g_cand);

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

            if (g_ext_mvs_count < 16) {
                g_ext_mvs.select<4,1>(g_ext_mvs_count * 2) = get_mv_pair(g_cand);
                g_ext_ref.select<4,1>(g_ext_mvs_count * 2) = get_refs(g_cand).replicate<2,1,2,0>();
                g_ext_mvs_count += 1;
                g_ext_mvs_count += (get_ref1(g_cand) != NONE);
            }

        } while (x < size8);
    }

    // First left column
    if (mi(X) > 0) {
        uint2 pass = inner_mi_gt0(X);
        uint2 y = 0;
        do {
            read(MODIFIED(MODE_INFO(pass)), (mi(X) - 1) * SIZEOF_MODE_INFO, mi(Y) + y, g_cand);

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

            if (g_ext_mvs_count < 16) {
                g_ext_mvs.select<4,1>(g_ext_mvs_count * 2) = get_mv_pair(g_cand);
                g_ext_ref.select<4,1>(g_ext_mvs_count * 2) = get_refs(g_cand).replicate<2,1,2,0>();
                g_ext_mvs_count += 1;
                g_ext_mvs_count += (get_ref1(g_cand) != NONE);
            }

        } while (y < size8);
    }

    get_cost_ref_frames(PARAM, ctx_ref_frame);
    get_cost_skip(PARAM, ctx_skip);

    // Top-right
    if (has_top_right(mi(X) + size8 - 1, mi(Y)) && mi(Y) > 0 && mi(X) + size8 < get_width()) {
        uint2 pass = inner_mi_gt0(Y);
        pass &= (inner_mi(X) + size8 < 8);
        read(MODIFIED(MODE_INFO(pass)), (mi(X) + size8) * SIZEOF_MODE_INFO, mi(Y) - 1, g_cand);
        add_cand(INDEX_SHIFT * 4, SCAN_ROW);
    }

    vector<uint2,4> has_new_mv = g_has_new_mv;
    g_weights.merge(g_weights + REF_CAT_LEVEL, g_weights > 0);
    const vector<uint4,4> nearest_ref_match = cm_cbit(g_ref_match);

    // Above-left
    if (mi(Y) > 0 && mi(X) > 0) {
        uint2 pass = inner_mi_gt0(Y) & inner_mi_gt0(X);
        read(MODIFIED(MODE_INFO(pass)), (mi(X) - 1) * SIZEOF_MODE_INFO, mi(Y) - 1, g_cand);
        add_cand(INDEX_SHIFT * 4, SCAN_ROW);
    }

    // Second above row
    if (mi(Y) > 1 && processed_rows < 2) {
        uint2 pass = inner_mi_gt1(Y);
        uint2 x = 0;
        do {
            read(MODIFIED(MODE_INFO(pass)), (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) - 2, g_cand);

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
    if (mi(X) > 1 && processed_cols < 2) {
        uint2 pass = inner_mi_gt1(X);
        uint2 y = 0;
        do {
            read(MODIFIED(MODE_INFO(pass)), (mi(X) - 2) * SIZEOF_MODE_INFO, mi(Y) + y, g_cand);

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
    if (mi(Y) > 2 && processed_rows < 3) {
        uint2 pass = inner_mi_gt2(Y);
        uint2 x = 0;
        do {
            read(MODIFIED(MODE_INFO(pass)), (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) - 3, g_cand);

            uint1 cand_log2_size8 = get_sb_type(g_cand) >> 2;
            uint1 len = get_cand_len(cand_log2_size8, log2_size8, min_cand_log2_size);
            x += len;

            add_cand(INDEX_SHIFT * 4 * len, SCAN_ROW);
        } while (x < size8);
    }

    // Third left column
    if (mi(X) > 2 && processed_cols < 3) {
        uint2 pass = inner_mi_gt2(X);
        uint2 y = 0;
        do {
            read(MODIFIED(MODE_INFO(pass)), (mi(X) - 3) * SIZEOF_MODE_INFO, mi(Y) + y, g_cand);

            uint1 cand_log2_size8 = get_sb_type(g_cand) >> 2;
            uint1 len = get_cand_len(cand_log2_size8, log2_size8, min_cand_log2_size);
            y += len;

            add_cand(INDEX_SHIFT * 4 * len, SCAN_COL);
        } while (y < size8);
    }

    // Non Causal Non Normative
    if (pyramidLayer == 0)
    {
        vector<uint2, 2> tmi = mi + size8;
        if (!is_outsize(tmi)) {
            read(MODIFIED(MODE_INFO(0)), tmi(X) * SIZEOF_MODE_INFO, tmi(Y), g_cand);
            add_candT(INDEX_SHIFT * 4, SCAN_ROW);
        }
    }

    // extend stacks
    //fetch_extended_candidates(MODE_INFO, size8, mi, inner_mi_gt0);
    if (g_ext_mvs_count > 0) {
        vector<int2,32> ext_mvs_for_ref0;
        vector<int2,32> ext_mvs_for_ref1;
        vector_ref<int2,32> ext_mvs = g_ext_mvs.select<32,1>();

        if (!get_bidir_comp_flag()) {
            ext_mvs_for_ref0 = ext_mvs;
            ext_mvs_for_ref1 = ext_mvs_for_ref0;
        } else {
            ext_mvs_for_ref0.merge(-ext_mvs, ext_mvs, g_ext_ref.select<32, 1>() == (uint1)ALTR);
            ext_mvs_for_ref1.merge(-ext_mvs_for_ref0, ext_mvs_for_ref0, -(int4)get_comp_flag());
        }

        extend_single_ref_stack(0, ext_mvs_for_ref0);
        extend_single_ref_stack(1, ext_mvs_for_ref1);

        if (!get_bidir_comp_flag()) {
            extend_comp_ref_stack_uni(ext_mvs_for_ref0);
        } else {
            extend_comp_ref_stack(ext_mvs_for_ref0);
        }
    } else {
        g_stack_size(2) = cm_max<uint2>(2, g_stack_size(2));
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
    vector<uint1, 8> sequence_7_to_0(SEQUENCE_7_TO_0);
    g_weights += sequence_7_to_0.replicate<4>();
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

_GENX_ inline uint4 get_min_newmv_cost(uint ref)
{
    const vector<int2,8> ref_mvs = g_refmv.select<8,1>(8 * ref);
    const vector<int2,8> mv = get_mv0(g_mode_info).replicate<4>();

    vector<uint4,4> mode_bits = g_bits_mode.format<uint2>().select<4,1>(8 * ref + MODE_IDX_NEWMV);

    vector<uint4,8> mv_bits = cm_abs<uint4>(mv - ref_mvs);
    mv_bits >>= 1;
    mv_bits += 1;
    mv_bits = (31 << 10) + 800 - (cm_lzd<uint4>(mv_bits) << 10);
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

_GENX_ inline uint4 get_min_newmv_cost_comp()
{
    const vector<int2,16> ref_mvs = g_refmv.select<16,1>(16);
    const vector<int2,16> mv = get_mv_pair(g_mode_info).replicate<4>();

    vector<uint4,4> mode_bits = g_bits_mode.format<uint2>().select<4,1>(16 + MODE_IDX_NEWMV);

    vector<uint4,16> mv_bits = cm_abs<uint4>(mv - ref_mvs);
    mv_bits >>= 1;
    mv_bits += 1;
    mv_bits = (31 << 10) + 800 - (cm_lzd<uint4>(mv_bits) << 10);
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

_GENX_ inline void write_mode_info(SurfaceIndex MODE_INFO, vector<uint2,2> mi, uint2 log2_num8x8)
{
    const uint2 num8x8 = 1 << (log2_num8x8 << 1);
    const uint2 xmask  = (1 << log2_num8x8) - 1;
    const uint2 yshift = log2_num8x8;

    uint2 i = 0;
    do {
        uint2 y = i >> yshift;
        uint2 x = i & xmask;
        write(MODE_INFO, (mi(X) + x) * SIZEOF_MODE_INFO, mi(Y) + y, g_mode_info);
        i++;
    } while (i < num8x8);
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
        acc.select<2,1,8,1>(0)  = in.select<2,1,8,1>(12,0) + in.select<2,1,8,1>(12,5);
        acc.select<2,1,8,1>(0) += in.select<2,1,8,1>(12,1) * filt(X,2-2);
        acc.select<2,1,8,1>(0) += in.select<2,1,8,1>(12,2) * filt(X,3-2);
        acc.select<2,1,8,1>(0) += in.select<2,1,8,1>(12,3) * filt(X,4-2);
        acc.select<2,1,8,1>(0) += in.select<2,1,8,1>(12,4) * filt(X,5-2);
        acc.select<2,1,8,1>(0) += 32;
        tmpHor.select<2,1,8,1>(12) = cm_shr<uint1>(acc.select<2,1,8,1>(0), 6, SAT);
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

_GENX_ inline vector<uint2,16> sad8_acc()
{
    vector<uint2,16> sad;
#if defined(target_gen12) || defined(target_gen12lp)
    sad = cm_abs<uint2>(cm_add<int2>(g_src8.select<2,1,8,1>(0), -g_pred8.select<2,1,8,1>(0)));
    sad = cm_add<uint2>(sad, cm_abs<uint2>(cm_add<int2>(g_src8.select<2,1,8,1>(2), -g_pred8.select<2,1,8,1>(2))));
    sad = cm_add<uint2>(sad, cm_abs<uint2>(cm_add<int2>(g_src8.select<2,1,8,1>(4), -g_pred8.select<2,1,8,1>(4))));
    sad = cm_add<uint2>(sad, cm_abs<uint2>(cm_add<int2>(g_src8.select<2,1,8,1>(6), -g_pred8.select<2,1,8,1>(6))));
    sad.select<8, 2>(0) += sad.select<8, 2>(1);
#elif defined(target_gen11) || defined(target_gen11lp)
    sad = 0;
    sad = cm_sada2<uint2>(g_src8.select<2, 1, 8, 1>(0), g_pred8.select<2, 1, 8, 1>(0), sad);
    sad = cm_sada2<uint2>(g_src8.select<2, 1, 8, 1>(2), g_pred8.select<2, 1, 8, 1>(2), sad);
    sad = cm_sada2<uint2>(g_src8.select<2, 1, 8, 1>(4), g_pred8.select<2, 1, 8, 1>(4), sad);
    sad = cm_sada2<uint2>(g_src8.select<2, 1, 8, 1>(6), g_pred8.select<2, 1, 8, 1>(6), sad);
#else // target_gen12
    sad = cm_sad2<uint2> (g_src8.select<2,1,8,1>(0), g_pred8.select<2,1,8,1>(0));
    sad = cm_sada2<uint2>(g_src8.select<2,1,8,1>(2), g_pred8.select<2,1,8,1>(2), sad);
    sad = cm_sada2<uint2>(g_src8.select<2,1,8,1>(4), g_pred8.select<2,1,8,1>(4), sad);
    sad = cm_sada2<uint2>(g_src8.select<2,1,8,1>(6), g_pred8.select<2,1,8,1>(6), sad);
#endif // target_gen12
    return sad;
}

_GENX_ inline vector<uint2,16> sad16_acc()
{
    vector<uint2,16> sad;
#if defined(target_gen12) || defined(target_gen12lp)
    sad = cm_abs<uint2>(cm_add<int2>(g_src16.row(0), -g_pred16.row(0)));
    #pragma unroll
    for (int i = 1; i < 16; i++)
        sad = cm_add<uint2>(sad, cm_abs<uint2>(cm_add<int2>(g_src16.row(i), -g_pred16.row(i))));
    sad.select<8, 2>(0) += sad.select<8, 2>(1);
#elif defined(target_gen11) || defined(target_gen11lp)
    sad = 0;
    #pragma unroll
    for (int i = 0; i < 16; i++)
        sad = cm_sada2<uint2>(g_src16.select<1,1,16,1>(i), g_pred16.select<1,1,16,1>(i), sad);
#else // target_gen12
    sad = cm_sad2<uint2> (g_src16.select<1,1,16,1>(0), g_pred16.select<1,1,16,1>(0));
    #pragma unroll
    for (int i = 1; i < 16; i++)
        sad = cm_sada2<uint2>(g_src16.select<1,1,16,1>(i), g_pred16.select<1,1,16,1>(i), sad);
#endif // target_gen12
    return sad;
}

_GENX_ inline vector<uint4,2> get_cache_idx(vector_ref<uint4,8> refmvs0, vector_ref<uint4,2> mvs, vector_ref<uint2,2> checked_mv)
{
    vector<uint2,2> mask;
    mask(0) = cm_pack_mask(refmvs0 == mvs.replicate<2,1,4,0>());
    mask(1) = mask(0) >> 4;
    checked_mv |= mask;
    return cm_fbl(vector<uint4,2>(mask | 0x80)); // 0x80 is a guard bit, to ensure that returned index is less than 8
}

#ifdef JOIN_MI
_GENX_ inline uint2 not_joinable(mode_info_ref mi1, mode_info_ref mi2)
{
    uint2 mask = cm_pack_mask(mi1.format<uint2>() != mi2.format<uint2>());
    mask &= 0x20F; // mask everything but MV, refIdxComb and sbType
    return mask;
}

_GENX_ inline void check_is_joinable(SurfaceIndex MODE_INFO, mode_info_ref mi_data, vector<uint2, 2> mi_pos)
{
    uint1 sbtype = get_sb_type(mi_data);
    uint1 skip = get_skip(mi_data);
    mode_info mi_data2;
    uint2 log2_num8x8 = sbtype >> 2;
    uint2 mi_blk_width = 1 << log2_num8x8;
    uint4 sad = get_sad(mi_data);
    if (((mi_pos(X) | mi_pos(Y))&mi_blk_width) != 0) return;

    read(MODIFIED(MODE_INFO), (mi_pos(X) + mi_blk_width) * SIZEOF_MODE_INFO, mi_pos(Y), mi_data2);
    if (get_mode(mi_data2) == 255 || not_joinable(mi_data, mi_data2)) return;
    skip &= get_skip(mi_data2);
    sad += get_sad(mi_data2);

    read(MODIFIED(MODE_INFO), mi_pos(X) * SIZEOF_MODE_INFO, mi_pos(Y) + mi_blk_width, mi_data2);
    if (get_mode(mi_data2) == 255 || not_joinable(mi_data, mi_data2)) return;
    skip &= get_skip(mi_data2);
    sad += get_sad(mi_data2);

    read(MODIFIED(MODE_INFO), (mi_pos(X) + mi_blk_width) * SIZEOF_MODE_INFO, mi_pos(Y) + mi_blk_width, mi_data2);
    if (not_joinable(mi_data, mi_data2)) return;
    skip &= get_skip(mi_data2);
    sad += get_sad(mi_data2);

    set_skip(mi_data, skip);
    set_sb_type(mi_data, sbtype+3);
    set_sad(mi_data, sad);
    write_mode_info(MODE_INFO, mi_pos, log2_num8x8+1);
}
#endif

static const int mtfi[3][4] = {
    { 258784,  687232,  723929,  764633},
    { 727897,  727897,  727897,  996160},
    {1085625, 1085625, 1085625, 1085625}
};
static const int stfi[3][4] = {
    { 20000,  20000,  20000,  20000},
    { 77150,  89805,  89805, 249454},
    {142184, 142184, 249454, 249454}
};

// sq err from 4 mono from stfi
static const int afi[3][4] = {
    { 303627, 315012, 315445, 314350 },
    { 423133, 435791, 443448, 456550 },
    { 397384, 397396, 500000, 500000 }
};
static const int bfi[3][4] = {
    { 547588, 572353, 677961, 794845 },
    {  22428,  41593, 203189, 228990 },
    { 473092, 527727,      0,      0 }
};

static const int mfi[3][4] = {
    {  3458,  6642,  6441,  10107 },
    { 13396, 13410, 13410, 137010 },
    { 13396, 13410, 13410, 137010 }
};

// sq err from 4 mono opt
static const int ati[4][4] = {
    { 224285, 225157, 228965, 244477 },
    { 233921, 281690, 276006, 308719 },
    { 242109, 286192, 302635, 324380 },
    { 265764, 279662, 336254, 378376 }
};
static const int bti[4][4] = {
    { 278397, 437689, 628257, 688823 },
    { 413391, 314939, 525718, 626691 },
    { 424680, 365299, 548104, 605718 },
    { 517953, 622422, 617987, 553362 }
};

static const int mti[4][4] = {
    { 295610, 201050, 177870, 146770 },
    { 218830, 177870, 146770,  63581 },
    {  89055,  28076,  28076,  28076 },
    {  49053,  25316,  23686,  12673 }
};

_GENX_ matrix<int, 3, 4> m_mtfi;
_GENX_ matrix<int, 3, 4> m_stfi;
_GENX_ matrix<int, 3, 4> m_afi;
_GENX_ matrix<int, 3, 4> m_bfi;
_GENX_ matrix<int, 3, 4> m_mfi;
_GENX_ matrix<int, 4, 4> m_bti;
_GENX_ matrix<int, 4, 4> m_ati;
_GENX_ matrix<int, 4, 4> m_mti;

_GENX_
inline bool IsBadPred(float SCpp, float SADpp_best, uint2 mvdAvg, uint1 l, uint1 qidx)
{
    if (l>2) return false;

    bool badPred = false;
    //uint1 Q = sliceQpY - ((l + 1) * 8);
    //uint1 qidx = 0;
    //if (Q<75) qidx = 0;
    //else if (Q<123) qidx = 1;
    //else if (Q<163) qidx = 2;
    //else            qidx = 3;

    //matrix<int, 3, 4> m_mtfi(mtfi);
    float mt = m_mtfi[l][qidx] * 0.00001f;
    //matrix<int, 3, 4> m_stfi(stfi);
    float st = m_stfi[l][qidx] * 0.001f;

    if (SCpp > st && mvdAvg > mt) {
        //matrix<int, 3, 4> m_afi(afi);
        float a = m_afi[l][qidx] * 0.000001f;
        //matrix<int, 3, 4> m_bfi(bfi);
        float b = m_bfi[l][qidx] * 0.000001f;
        //matrix<int, 3, 4> m_mfi(mfi);
        float m = m_mfi[l][qidx] * 0.000001f;
        float SADT = b + cm_log(SCpp)*a*0.693147f;
        float msad = cm_log(SADpp_best + m * cm_min<uint2>(64, mvdAvg))*0.693147f;

        if (msad > SADT)       badPred = true;
    }
    return badPred;
}

_GENX_
inline bool IsGoodPred(float SCpp, float SADpp, uint2 mvdAvg, uint1 l, uint1 qidx)
{
    bool goodPred = false;
    //uint1 Q = sliceQpY - ((l + 1) * 8);
    //uint1 qidx = 0;
    //if (Q<75) qidx = 0;
    //else if (Q<123) qidx = 1;
    //else if (Q<163) qidx = 2;
    //else            qidx = 3;

    //matrix<int, 4, 4> m_bti(bti);
    float b = m_bti[l][qidx] * 0.000001f;
    //matrix<int, 4, 4> m_ati(ati);
    float a = m_ati[l][qidx] * 0.000001f;
    //matrix<int, 4, 4> m_mti(mti);
    float m = m_mti[l][qidx] * 0.000001f;

    float SADT = b + cm_log(SCpp)*a*0.693147f;
    float msad = cm_log(SADpp + m * mvdAvg)*0.693147f;
    if (msad < SADT)       goodPred = true;

    return goodPred;
}

#define INIT_HELPER(V,I) { decltype(V) tmp(I); V = tmp; }

#ifdef TRYINTRA_ORIG
_GENX_ void
ModeDecisionPass2SB(SurfaceIndex PARAM, SurfaceIndex SRC, vector<SurfaceIndex, 8> REF, vector<SurfaceIndex, 2> MODE_INFO, vector<SurfaceIndex, 8> RSCS, vector<uint2, 2> omi, uint sliceQpY, uint pyramidLayer, uint temporalSync, vector<SurfaceIndex, 8> MV)
#else
_GENX_ void
ModeDecisionPass2SB(SurfaceIndex PARAM, SurfaceIndex SRC, vector<SurfaceIndex, 8> REF, vector<SurfaceIndex, 2> MODE_INFO, vector<SurfaceIndex, 8> RSCS, vector<uint2, 2> omi, uint sliceQpY, uint pyramidLayer, uint temporalSync)
#endif
{
    vector<uint1,8*8*2*4> cache;

    //read_param(PARAM);

    vector<uint1,64> scan_z2r(ZIGZAG_SCAN);
    INIT_HELPER(g_has_top_right, HAS_TOP_RIGHT);
    INIT_HELPER(g_interp_filters, INTERP_FILTERS);

    //vector<uint2,2> omi;
    //omi(X) = get_thread_origin_x();
    //omi(Y) = get_thread_origin_y();
    //omi *= 8;

    uint num8x8;
    for (uint2 i = 0; i < 64; i += num8x8) {
        uint1 raster = scan_z2r[i];

        vector<uint2,2> mi = omi;
        mi(X) += uint2(raster & 7);
        mi(Y) += uint2(raster >> 3);

        read(MODE_INFO(0), mi(X) * SIZEOF_MODE_INFO, mi(Y), g_mode_info);

        uint1 sbtype = get_sb_type(g_mode_info);
        uint2 log2_num8x8 = sbtype >> 2;
        uint2 blk_size = 8 << log2_num8x8;
        num8x8 = 1 << (log2_num8x8 << 1);

        uint1 mode = get_mode(g_mode_info);
        if (mode == OUT_OF_PIC) {
            write(MODE_INFO(1), mi(X) * SIZEOF_MODE_INFO, mi(Y), g_mode_info);
            continue;
        }

        get_mvrefs(PARAM, MODE_INFO, log2_num8x8, mi, pyramidLayer);
        vector<int2, 2> pmv0 = g_refmv.select<2, 1>(0);
        vector<int2, 2> pmv1 = g_refmv.select<2, 1>(8);
        vector<int2, 4> pmvc = g_refmv.select<4, 1>(16);
        uint4 bits_1pass;
        uint1 ref_1pass = cm_avg<uint1>(get_ref_comb(g_mode_info), 0); // converts: 0->0 1->1 2->1 3->2

        if (get_mode(g_mode_info) == ZEROMV) {
            bits_1pass = g_bits_mode(ref_1pass, MODE_IDX_ZEROMV);
        } else {
            if (ref_1pass == 2)
                bits_1pass = get_min_newmv_cost_comp();
            else
                bits_1pass = get_min_newmv_cost(ref_1pass);
            set_mode(g_mode_info, NEWMV);
        }

        uint best_cost = get_sad(g_mode_info);
        uint best_sad = best_cost >> 11;
        best_cost += bits_1pass * get_lambda_int();

        if (get_comp_flag()) debug_printf("[MD2] (%d,%2d,%2d) 1PASS ref=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n",
            3 - log2_num8x8, mi(Y), mi(X), ref_1pass, get_mv0(g_mode_info)(0), get_mv0(g_mode_info)(1),
            get_mv1(g_mode_info)(0), get_mv1(g_mode_info)(1), best_cost, bits_1pass);

        vector<int2,2> pel = mi * MI_SIZE;

        vector<uint2,2> checked_mv = 0;

        if (sbtype == BLOCK_8X8)
            read(SRC, pel(X), pel(Y), g_src8);
        else if (sbtype == BLOCK_16X16)
            read(SRC, pel(X), pel(Y), g_src16);

        if (!g_fastMode || ref_1pass == 2)
        {
            for (uint2 m = 0; m < g_stack_size(2); m++) {
                vector_ref<int2, 4> mv = g_refmv.select<4, 1>(16);
                uint4 bits = g_bits_mode(2, 0);

                vector<uint4, 2> cache_idx = get_cache_idx(g_refmv.format<uint4>().select<8, 1>(), mv.format<uint4>(), checked_mv);

                vector<uint2, 16> sad_comp_acc = 0;
                vector<uint2, 16> sad_ref0_acc = 0;
                vector<uint2, 16> sad_ref1_acc = 0;

                matrix<int1, 4, 4> filt;
                vector<uint2, 4> dmv = cm_asr<uint2>(mv, 1) & 3;
                filt.format<uint4>() = g_interp_filters.format<uint4>().iselect(dmv);

                vector<int2, 4> pos = pel.replicate<2>() + cm_asr<int2>(mv, 3);
                pos -= 2;

                if (sbtype == BLOCK_8X8) {
                    interpolate8(REF(0), pos.select<2, 1>(0), filt.select<2, 1, 4, 1>(0));

                    if (cache_idx(0) < g_stack_size(0))
                        sad_ref0_acc = sad8_acc();

                    matrix<uint1, 8, 8> pred0 = g_pred8;
                    interpolate8(REF(1), pos.select<2, 1>(2), filt.select<2, 1, 4, 1>(2));

                    if (cache_idx(1) < g_stack_size(1))
                        sad_ref1_acc = sad8_acc();

                    g_pred8 = cm_avg<uint1>(g_pred8, pred0);
                    sad_comp_acc = sad8_acc();
                }
                else {
                    vector<uint2, 2> blk;
                    blk(Y) = 0;
                    do {

                        blk(X) = 0;
                        do {
                            if (sbtype > BLOCK_16X16)
                                read(SRC, pel(X) + blk(X), pel(Y) + blk(Y), g_src16);

                            interpolate16(REF(0), pos.select<2, 1>(0) + blk, filt.select<2, 1, 4, 1>(0));

                            if (cache_idx(0) < g_stack_size(0))
                                sad_ref0_acc = cm_add<uint2>(sad16_acc(), sad_ref0_acc, SAT);

                            matrix<uint1, 16, 16> pred0 = g_pred16;
                            interpolate16(REF(1), pos.select<2, 1>(2) + blk, filt.select<2, 1, 4, 1>(2));

                            if (cache_idx(1) < g_stack_size(1))
                                sad_ref1_acc = cm_add<uint2>(sad16_acc(), sad_ref1_acc, SAT);

                            g_pred16 = cm_avg<uint1>(pred0, g_pred16);

                            sad_comp_acc = cm_add<uint2>(sad16_acc(), sad_comp_acc, SAT);

                            blk(X) += 16;
                        } while (blk(X) < blk_size);

                        blk(Y) += 16;
                    } while (blk(Y) < blk_size);

                }
                uint sad = cm_sum<uint>(sad_comp_acc.select<8, 2>());
                uint cost = sad << 11;
                cost += bits * get_lambda_int();

                if (get_comp_flag()) debug_printf("[MD2] (%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d),(%d,%d) cost=%u sad=%u bits=%u\n",
                    3 - log2_num8x8, mi(Y), mi(X), 2, m, mv(0), mv(1), mv(2), mv(3), cost, sad, bits);

                if (best_cost > cost) {
                    best_cost = cost;
                    best_sad = sad;
                    set_mv_pair(g_mode_info, mv);
                    set_ref_bidir(g_mode_info);
                    set_mode(g_mode_info, NEARESTMV);
                }

                if (cache_idx(0) < g_stack_size(0)) {
                    sad = cm_sum<uint>(sad_ref0_acc.select<8, 2>());
                    cost = sad << 11;
                    cost += g_bits_mode(0, cache_idx(0)) * get_lambda_int();

                    if (get_comp_flag()) debug_printf("[MD2] (%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u sad=%u bits=%u\n",
                        3 - log2_num8x8, mi(Y), mi(X), 0, cache_idx(0), mv(0), mv(1), cost, sad, g_bits_mode(0, cache_idx(0)));

                    if (best_cost > cost) {
                        best_cost = cost;
                        best_sad = sad;
                        set_mv0(g_mode_info, mv.select<2, 1>(0));
                        set_mv1(g_mode_info, 0);
                        set_ref_single(g_mode_info, LAST);
                        set_mode(g_mode_info, NEARESTMV);
                    }
                }

                if (cache_idx(1) < g_stack_size(1)) {
                    sad = cm_sum<uint>(sad_ref1_acc.select<8, 2>());
                    cost = sad << 11;
                    cost += g_bits_mode(1, cache_idx(1)) * get_lambda_int();

                    if (get_comp_flag()) debug_printf("[MD2] (%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u sad=%u bits=%u\n",
                        3 - log2_num8x8, mi(Y), mi(X), 1, cache_idx(1), mv(2), mv(3), cost, sad, g_bits_mode(1, cache_idx(1)));

                    if (best_cost > cost) {
                        best_cost = cost;
                        best_sad = sad;
                        set_mv0(g_mode_info, mv.select<2, 1>(2));
                        set_mv1(g_mode_info, 0);
                        set_ref_single(g_mode_info, ALTR);
                        set_mode(g_mode_info, NEARESTMV);
                    }
                }

                g_refmv.select<8, 1>(16) = g_refmv.select<8, 1>(20);
                g_refmv.select<8, 1>(24) = g_refmv.select<8, 1>(28);
                g_bits_mode.row(2).select<4, 1>(0) = g_bits_mode.row(2).select<4, 1>(1);
            }
        }

        uint2 cur_refmv_count = g_stack_size(0);
        for (uint2 r = 0; r < 2; r++) {
            for (uint2 m = 0; m < cur_refmv_count; m++) {
                if ((checked_mv(0) & 1) == 0) {
                    vector_ref<int2,2> mv = g_refmv.select<2,1>();
                    uint4 bits = g_bits_mode(0,0);

                    matrix<int1,2,4> filt;
                    vector<uint2,2> dmv = cm_asr<uint2>(mv, 1) & 3;
                    filt.format<uint4>() = g_interp_filters.format<uint4>().iselect(dmv);

                    vector<int2,2> pos = pel + cm_asr<int2>(mv, 3);
                    pos -= 2;

                    vector<uint2,16> sad_acc;
                    if (sbtype == BLOCK_8X8) {
                        interpolate8(REF(r), pos, filt);
                        sad_acc = sad8_acc();
                    } else {
                        vector<uint2,2> blk;

                        sad_acc = 0;
                        blk(Y) = 0;
                        do {
                            blk(X) = 0;
                            do {
                                interpolate16(REF(r), pos + blk, filt);
                                if (sbtype > BLOCK_16X16)
                                    read(SRC, pel(X) + blk(X), pel(Y) + blk(Y), g_src16);
                                sad_acc = cm_add<uint2>(sad16_acc(), sad_acc, SAT);
                                blk(X) += 16;
                            } while (blk(X) < blk_size);

                            blk(Y) += 16;
                        } while (blk(Y) < blk_size);

                    }
                    uint sad = cm_sum<uint>(sad_acc.select<8,2>());
                    uint cost = sad << 11;
                    cost += bits * get_lambda_int();

                    if (get_comp_flag()) debug_printf("[MD2] (%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u sad=%d bits=%u\n",
                        3 - log2_num8x8, mi(Y), mi(X), r, m, mv(0), mv(1), cost, sad, bits);

                    if (best_cost > cost) {
                        best_cost = cost;
                        best_sad = sad;
                        set_mv0(g_mode_info, mv);
                        set_mv1(g_mode_info, 0);
                        set_ref_single(g_mode_info, r << get_comp_flag());
                        set_mode(g_mode_info, NEARESTMV);
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

        // For Split64 & TryIntra
        set_sad(g_mode_info, best_sad);

        uint1 refs_used = cm_avg<uint1>(get_ref_comb(g_mode_info), 0); // converts: 0->0 1->1 2->1 3->2
#ifdef TRYINTRA_ORIG
        if (refs_used == 2) {
            set_dmv_pair(g_mode_info, pmvc);
        } else {
            set_dmv0(g_mode_info, pmv0);
            set_dmv1(g_mode_info, pmv1);
        }
#else
        if (refs_used == 2) {
            const vector<int2, 2> dmv0 = get_mv0(g_mode_info) - pmvc.select<2,1>(0);
            const vector<int2, 2> dmv1 = get_mv1(g_mode_info) - pmvc.select<2,1>(2);
            set_dmv0(g_mode_info, dmv0);
            set_dmv1(g_mode_info, dmv1);
        } else if (refs_used == 1) {
            const vector<int2, 2> dmv0 = get_mv0(g_mode_info) - pmv1;
            set_dmv0(g_mode_info, dmv0);
            set_dmv1(g_mode_info, 0);
        } else {
            const vector<int2, 2> dmv0 = get_mv0(g_mode_info) - pmv0;
            set_dmv0(g_mode_info, dmv0);
            set_dmv1(g_mode_info, 0);
        }
#endif

        write_mode_info(MODE_INFO(1), mi, log2_num8x8);
    }
#ifdef JOIN_MI
    bool joined;
    do {
        joined = false;
        for (uint2 i = 0; i < 64; i += num8x8) {
            uint1 raster = scan_z2r[i];

            vector<uint2, 2> mi = omi;
            mi(X) += uint2(raster & 7);
            mi(Y) += uint2(raster >> 3);

            read(MODIFIED(MODE_INFO(1)), mi(X) * SIZEOF_MODE_INFO, mi(Y), g_mode_info);

            uint1 sbtype = get_sb_type(g_mode_info);
            uint2 log2_num8x8 = sbtype >> 2;
            num8x8 = 1 << (log2_num8x8 << 1);
            uint1 mode = get_mode(g_mode_info);
            if (mode == OUT_OF_PIC || sbtype == BLOCK_64X64) {
                continue;
            }
            check_is_joinable(MODE_INFO(1), g_mode_info, mi);
            // Try Intra
            uint1 sbtype_new = get_sb_type(g_mode_info);
            log2_num8x8 = sbtype_new >> 2;
            num8x8 = 1 << (log2_num8x8 << 1);
            if (sbtype_new != sbtype) joined = true;
        }
    } while (joined);

#if 1
    // Split
    INIT_HELPER(m_mtfi, mtfi);
    INIT_HELPER(m_stfi, stfi);
    INIT_HELPER(m_afi, afi);
    INIT_HELPER(m_bfi, bfi);
    INIT_HELPER(m_mfi, mfi);
    INIT_HELPER(m_bti, bti);
    INIT_HELPER(m_ati, ati);
    INIT_HELPER(m_mti, mti);
    vector<float, 4> divbypix;
    divbypix(0) = 0.015625000000f;
    divbypix(1) = 0.003906250000f;
    divbypix(2) = 0.000976562500f;
    divbypix(3) = 0.000244140625f;
    vector<float, 4> divbyblk4;
    divbyblk4(0) = 0.250000000f;
    divbyblk4(1) = 0.062500000f;
    divbyblk4(2) = 0.015625000f;
    divbyblk4(3) = 0.003906250f;

    uint1 Q = sliceQpY - ((pyramidLayer + 1) * 8);
    uint1 qidx = 0;
    if (Q<75) qidx = 0;
    else if (Q<123) qidx = 1;
    else if (Q<163) qidx = 2;
    else            qidx = 3;

    read(MODIFIED(MODE_INFO(1)), omi(X) * SIZEOF_MODE_INFO, omi(Y), g_mode_info);
    uint1 sbtype = get_sb_type(g_mode_info);
    uint1 split64 = 0;
    if (sbtype == BLOCK_64X64) {
        split64 = 1;
        if (temporalSync) {
            vector<int, 1> Rs = 0;
            vector<int, 1> Cs = 0;
            read(RSCS(6), (omi(X) >> 3) * 4, omi(Y) >> 3, Rs);
            read(RSCS(7), (omi(X) >> 3) * 4, omi(Y) >> 3, Cs);
            float scpp = Rs(0) + Cs(0);
            scpp *= divbyblk4(3);
            if (pyramidLayer > 2) {
                split64 = 0;
            }
            else if(get_skip(g_mode_info) || scpp<20){
                uint2 mvd = 0;

#ifdef TRYINTRA_ORIG
                matrix<uint4, 2, 2>   g_new_mv_data;
                read(MV(3 + 0), omi(X) * NEW_MV_DATA_LARGE_SIZE >> 3, omi(Y) >> 3, g_new_mv_data.select<1, 1, 2, 1>(0));
                read(MV(3 + 4), omi(X) * NEW_MV_DATA_LARGE_SIZE >> 3, omi(Y) >> 3, g_new_mv_data.select<1, 1, 2, 1>(1));

                uint newmv_ref = g_new_mv_data(0, 1) & 3;
                float sadpp = g_new_mv_data(0, 1) >> 2;
                sadpp *= divbypix(3);

                if (newmv_ref == 2) {
                    vector<int2, 4> mv = g_new_mv_data.format<int2, 2, 4>().select<2, 1, 2, 1>() << 1;
                    vector<int2, 4> pmvc = get_dmv_pair(g_mode_info);
                    vector<int2, 4> dmv = mv - pmvc;
                    dmv = cm_abs<int2>(dmv);
                    mvd = dmv(0) + dmv(1);
                    uint2 mvd1 = dmv(2) + dmv(3);
                    mvd = cm_min<uint2>(mvd, mvd1);
                } else {
                    vector<int2, 2> mv = g_new_mv_data.row(newmv_ref & 1).format<int2>().select<2, 1>() << 1;
                    vector<int2, 2> pmv = ((newmv_ref == 1) ?  get_dmv1(g_mode_info) : get_dmv0(g_mode_info));
                    vector<int2, 2> dmv = mv - pmv;
                    dmv = cm_abs<int2>(dmv);
                    mvd = dmv(0) + dmv(1);
                }
#else
                uint1 refs_used = cm_avg<uint1>(get_ref_comb(g_mode_info), 0); // converts: 0->0 1->1 2->1 3->2

                if (refs_used == 2) {
                    vector<int2, 4> dmv = get_dmv_pair(g_mode_info);
                    dmv = cm_abs<int2>(dmv);
                    mvd = dmv(0) + dmv(1);
                    uint2 mvd1 = dmv(2) + dmv(3);
                    mvd = cm_min<uint2>(mvd, mvd1);
                } else {
                    vector<int2, 2> dmv = get_dmv0(g_mode_info);
                    dmv = cm_abs<int2>(dmv);
                    mvd = dmv(0) + dmv(1);
                }

                float sadpp = get_sad(g_mode_info);
                sadpp *= 0.015625f;
                sadpp *= 0.015625f;
                sadpp *= 1.5f;
#endif
                sadpp = cm_max<float>(0.1f, sadpp);
                split64 = (IsBadPred(scpp, sadpp, mvd, pyramidLayer, qidx)) ? 1 : 0;
            }
        }
    }
    if (split64) {
        set_sb_type(g_mode_info, sbtype - 3);
        set_sad(g_mode_info, get_sad(g_mode_info) >> 2);
        write_mode_info(MODE_INFO(1), omi, 3);
    }
#endif
#if 1
    // TryIntra
    for (uint2 i = 0; i < 64; i += num8x8) {
        uint1 raster = scan_z2r[i];

        vector<uint2, 2> mi = omi;
        mi(X) += uint2(raster & 7);
        mi(Y) += uint2(raster >> 3);

        read(MODIFIED(MODE_INFO(1)), mi(X) * SIZEOF_MODE_INFO, mi(Y), g_mode_info);

        sbtype = get_sb_type(g_mode_info);
        uint2 log2_num8x8 = sbtype >> 2;
        uint2 blk_size = 8 << log2_num8x8;
        num8x8 = 1 << (log2_num8x8 << 1);

        uint1 mode = get_mode(g_mode_info);
        if (mode == OUT_OF_PIC) {
            continue;
        }
        if (sbtype == BLOCK_64X64) {
            set_sad(g_mode_info, 13);
            write_mode_info(MODE_INFO(1), mi, 3);
            continue;
        }

        if (split64 || !temporalSync) {
            set_sad(g_mode_info, 0);
            write_mode_info(MODE_INFO(1), mi, log2_num8x8);
            continue;
        }
        uint1 skip = get_skip(g_mode_info);
        uint2 mvd = 0;

        vector<int, 1> Rs = 0;
        vector<int, 1> Cs = 0;
        read(RSCS(log2_num8x8*2+0), (mi(X) >> log2_num8x8) * 4, mi(Y) >> log2_num8x8, Rs);
        read(RSCS(log2_num8x8*2+1), (mi(X) >> log2_num8x8) * 4, mi(Y) >> log2_num8x8, Cs);
        float scpp = Rs(0) + Cs(0);
        //scpp  /= (1 << ((log2_num8x8 + 1) << 1));
        scpp *= divbyblk4(log2_num8x8);

#ifdef TRYINTRA_ORIG
        matrix<uint4, 2, 2>   g_new_mv_data;
        if (log2_num8x8 > 1) {
            read(MV(log2_num8x8 + 0), mi(X) * NEW_MV_DATA_LARGE_SIZE >> log2_num8x8, mi(Y) >> log2_num8x8, g_new_mv_data.select<1, 1, 2, 1>(0));
            read(MV(log2_num8x8 + 4), mi(X) * NEW_MV_DATA_LARGE_SIZE >> log2_num8x8, mi(Y) >> log2_num8x8, g_new_mv_data.select<1, 1, 2, 1>(1));
        } else {
            read(MV(log2_num8x8 + 0), mi(X) * NEW_MV_DATA_SMALL_SIZE >> log2_num8x8, mi(Y) >> log2_num8x8, g_new_mv_data.select<1, 1, 2, 1>(0));
            read(MV(log2_num8x8 + 4), mi(X) * NEW_MV_DATA_SMALL_SIZE >> log2_num8x8, mi(Y) >> log2_num8x8, g_new_mv_data.select<1, 1, 2, 1>(1));
        }
        uint newmv_ref = g_new_mv_data(0, 1) & 3;
        float sadpp = g_new_mv_data(0, 1) >> 2;
        //sadpp /= (1 << ((log2_num8x8 + 3) << 1));
        sadpp *= divbypix(log2_num8x8);

        if (newmv_ref == 2) {
            vector<int2, 4> mv = g_new_mv_data.format<int2, 2, 4>().select<2, 1, 2, 1>() << 1;
            vector<int2, 4> pmvc = get_dmv_pair(g_mode_info);
            vector<int2, 4> dmv = mv - pmvc;
            dmv = cm_abs<int2>(dmv);
            mvd = dmv(0) + dmv(1);
            uint2 mvd1 = dmv(2) + dmv(3);
            mvd = cm_min<uint2>(mvd, mvd1);
        } else {
            vector<int2, 2> mv = g_new_mv_data.row(newmv_ref & 1).format<int2>().select<2, 1>() << 1;
            vector<int2, 2> pmv = ((newmv_ref == 1) ? get_dmv1(g_mode_info) : get_dmv0(g_mode_info));
            vector<int2, 2> dmv = mv - pmv;
            dmv = cm_abs<int2>(dmv);
            mvd = dmv(0) + dmv(1);
        }
#else
        uint1 refs_used = cm_avg<uint1>(get_ref_comb(g_mode_info), 0); // converts: 0->0 1->1 2->1 3->2
        if (refs_used == 2) {
            vector<int2, 4> dmv = get_dmv_pair(g_mode_info);
            dmv = cm_abs<int2>(dmv);
            mvd = dmv(0) + dmv(1);
            uint2 mvd1 = dmv(2) + dmv(3);
            mvd = cm_min<uint2>(mvd, mvd1);
        } else {
            vector<int2, 2> dmv = get_dmv0(g_mode_info);
            dmv = cm_abs<int2>(dmv);
            mvd = dmv(0) + dmv(1);
        }
        float sadpp = get_sad(g_mode_info);
        sadpp /= (1 << ((log2_num8x8 + 3) << 1));
        sadpp *= 1.5f;
#endif

        sadpp = cm_max<float>(0.1f, sadpp);
        uint1 tryIntra = ((IsGoodPred(scpp, sadpp, mvd, pyramidLayer, qidx)) ? 0 : 1);
        if (tryIntra && mode != NEARESTMV && skip) {
            tryIntra = (IsBadPred(scpp, sadpp, mvd, pyramidLayer, qidx)) ? 1 : 0;
        }

        if (tryIntra) set_sad(g_mode_info, 0);
        else          set_sad(g_mode_info, 13);
        write_mode_info(MODE_INFO(1), mi, log2_num8x8);
    }
#endif
    // All partition information now available after this kernel
#endif
}

#ifdef TRYINTRA_ORIG
extern "C" _GENX_MAIN_ void
ModeDecisionPass2(SurfaceIndex PARAM, SurfaceIndex SRC, vector<SurfaceIndex, 8> REF, vector<SurfaceIndex, 2> MODE_INFO, vector<SurfaceIndex, 8> RSCS, uint SliceQpY, uint PyramidLayer, uint TemporalSync, vector<SurfaceIndex, 8> MV, uint yoff)
#else
extern "C" _GENX_MAIN_ void
ModeDecisionPass2(SurfaceIndex PARAM, SurfaceIndex SRC, vector<SurfaceIndex, 8> REF, vector<SurfaceIndex, 2> MODE_INFO, vector<SurfaceIndex, 8> RSCS, uint SliceQpY, uint PyramidLayer, uint TemporalSync, uint yoff)
#endif
{
    read_param(PARAM);
    g_kernel_mi_row_start = (get_thread_origin_y() + (uint2)yoff) * KERNEL_HEIGHT_MI;
    g_kernel_mi_col_start = get_thread_origin_x() * KERNEL_WIDTH_MI;
    vector<uint2, 2> mi;
    mi(Y) = g_kernel_mi_row_start;

    for (uint2 i = 0; i < KERNEL_HEIGHT_SB; i++, mi(Y) += 8) {
        mi(X) = g_kernel_mi_col_start;
        for (uint2 j = 0; j < KERNEL_WIDTH_SB; j++, mi(X) += 8) {
            if(is_outsize(mi)) continue;
#ifdef TRYINTRA_ORIG
            ModeDecisionPass2SB(PARAM, SRC, REF, MODE_INFO, RSCS, mi, SliceQpY, PyramidLayer, TemporalSync, MV);
#else
            ModeDecisionPass2SB(PARAM, SRC, REF, MODE_INFO, RSCS, mi, SliceQpY, PyramidLayer, TemporalSync);
#endif
        }
    }
}
