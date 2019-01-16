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

#include "mfx_common.h"

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "algorithm"
#include "mfx_av1_defs.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_copy.h"
#include "mfx_av1_get_context.h"
#include "mfx_av1_dispatcher_wrappers.h"
#include "mfx_av1_get_intra_pred_pels.h"
#include "mfx_av1_trace.h"
#include "mfx_av1_quant.h"

#include <immintrin.h>

namespace AV1Enc {

template <class PixType> struct ChromaUVPixType;
template <> struct ChromaUVPixType<uint8_t> { typedef uint16_t type; };
template <> struct ChromaUVPixType<uint16_t> { typedef uint32_t type; };


bool operator <(const IntraLumaMode &l, const IntraLumaMode &r) { return l.cost < r.cost; }

void SortLumaModesByCost(IntraLumaMode *modes, int32_t numCandInput, int32_t numCandSorted)
{
    assert(numCandInput >= numCandSorted);
    // stable partial sort
    for (int32_t i = 0; i < numCandSorted; i++)
        std::swap(modes[i],
                  modes[int32_t(std::min_element(modes + i, modes + numCandInput) - modes)]);
}

template <typename PixType>
void AV1CU<PixType>::GetAngModesFromHistogram(int32_t xPu, int32_t yPu, int32_t puSize, int8_t *modes, int32_t numModes)
{
    assert(numModes <= 33);
    assert(puSize >= 4);
    assert(puSize <= 64);
    assert(h265_log2m2[puSize] >= 0);
    assert(m_ctbPelX + xPu + puSize <= m_par->Width);
    assert(m_ctbPelY + yPu + puSize <= m_par->Height);

    if (m_par->enableCmFlag) {
        assert(numModes <= 1);
        int32_t log2PuSize = h265_log2m2[puSize] + 2;
        int32_t x = (m_ctbPelX + xPu) >> log2PuSize;
        int32_t y = (m_ctbPelY + yPu) >> log2PuSize;
        int32_t pitch = m_currFrame->m_feiIntraAngModes[log2PuSize-2]->m_pitch;
        uint32_t *feiAngModes = (uint32_t *)(m_currFrame->m_feiIntraAngModes[log2PuSize-2]->m_sysmem + y * pitch) + x;
        for (int32_t i = 0; i < numModes; i++) {
            modes[i] = feiAngModes[i];
            assert(modes[i] >= 2 && modes[i] <= 34);
        }
    }
    else {
        int32_t histogram[35] = {};

        // all in units of 4x4 blocks
        if (puSize == 4) {
            int32_t pitch = 40 * m_par->MaxCUSize / 4;
            const HistType *histBlock = m_hist4 + (xPu >> 2) * 40 + (yPu >> 2) * pitch;
            for (int32_t i = 0; i < 35; i++)
                histogram[i] = histBlock[i];
        }
        else {
            puSize >>= 3;
            int32_t pitch = 40 * m_par->MaxCUSize / 8;
            const HistType *histBlock = m_hist8 + (xPu >> 3) * 40 + (yPu >> 3) * pitch;
            for (int32_t y = 0; y < puSize; y++, histBlock += pitch)
                for (int32_t x = 0; x < puSize; x++)
                    for (int32_t i = 0; i < 35; i++)
                        histogram[i] += histBlock[40 * x + i];
        }

        for (int32_t i = 0; i < numModes; i++) {
            int32_t mode = (int32_t)(std::max_element(histogram + 2, histogram + 35) - histogram);
            modes[i] = mode;
            histogram[mode] = -1;
        }
    }
}


template <typename PixType>
void AV1CU<PixType>::CheckIntra(int32_t absPartIdx, int32_t depth, PartitionType partition)
{
    if (!m_par->intraRDO && (depth == 0 || partition != PARTITION_NONE)) {
        m_costCurr = COST_MAX; // skip 64x64 and non-square partitions
        return;
    }

    const int32_t sbx = (av1_scan_z2r4[absPartIdx] & 15) << 2;
    const int32_t sby = (av1_scan_z2r4[absPartIdx] >> 4) << 2;
    const int32_t miRow = (m_ctbPelY + sby) >> 3;
    const int32_t miCol = (m_ctbPelX + sbx) >> 3;
    const ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const uint16_t *skipBits = m_currFrame->bitCount.skip[ GetCtxSkip(above, left) ];

    RdCost costY = (m_par->intraRDO)
        ? CheckIntraLuma(absPartIdx, depth, partition)
            : CheckIntraLumaNonRdAv1(absPartIdx, depth, partition);

    RdCost costUV = {};
    if (m_par->chromaRDO)
        costUV = CheckIntraChroma(absPartIdx, depth, partition);

    int32_t sse = costY.sse + costUV.sse;
    uint32_t bits = costY.modeBits + costUV.modeBits + skipBits[mi->skip];
    if (!mi->skip)
        bits += costY.coefBits + costUV.coefBits;
    m_costCurr += sse + m_rdLambda * bits;
}

/* clang-format off */
static const uint16_t orders_128x128[1] = { 0 };
static const uint16_t orders_128x64[2] = { 0, 1 };
static const uint16_t orders_64x128[2] = { 0, 1 };
static const uint16_t orders_64x64[4] = {0, 1, 2, 3};
static const uint16_t orders_64x32[8] = {
    0, 2, 1, 3, 4, 6, 5, 7,
};
static const uint16_t orders_32x64[8] = {
    0, 1, 2, 3, 4, 5, 6, 7,
};
static const uint16_t orders_32x32[16] = {
    0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15,
};
static const uint16_t orders_32x16[32] = {
    0,  2,  8,  10, 1,  3,  9,  11, 4,  6,  12, 14, 5,  7,  13, 15,
    16, 18, 24, 26, 17, 19, 25, 27, 20, 22, 28, 30, 21, 23, 29, 31,
};
static const uint16_t orders_16x32[32] = {
    0,  1,  2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15,
    16, 17, 18, 19, 24, 25, 26, 27, 20, 21, 22, 23, 28, 29, 30, 31,
};
static const uint16_t orders_16x16[64] = {
    0,  1,  4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23,
    8,  9,  12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31,
    32, 33, 36, 37, 48, 49, 52, 53, 34, 35, 38, 39, 50, 51, 54, 55,
    40, 41, 44, 45, 56, 57, 60, 61, 42, 43, 46, 47, 58, 59, 62, 63,
};
static const uint16_t orders_16x8[128] = {
  0,  2,  8,  10, 32,  34,  40,  42,  1,  3,  9,  11, 33,  35,  41,  43,
  4,  6,  12, 14, 36,  38,  44,  46,  5,  7,  13, 15, 37,  39,  45,  47,
  16, 18, 24, 26, 48,  50,  56,  58,  17, 19, 25, 27, 49,  51,  57,  59,
  20, 22, 28, 30, 52,  54,  60,  62,  21, 23, 29, 31, 53,  55,  61,  63,
  64, 66, 72, 74, 96,  98,  104, 106, 65, 67, 73, 75, 97,  99,  105, 107,
  68, 70, 76, 78, 100, 102, 108, 110, 69, 71, 77, 79, 101, 103, 109, 111,
  80, 82, 88, 90, 112, 114, 120, 122, 81, 83, 89, 91, 113, 115, 121, 123,
  84, 86, 92, 94, 116, 118, 124, 126, 85, 87, 93, 95, 117, 119, 125, 127,
};
static const uint16_t orders_8x16[128] = {
  0,  1,  2,  3,  8,  9,  10, 11, 32,  33,  34,  35,  40,  41,  42,  43,
  4,  5,  6,  7,  12, 13, 14, 15, 36,  37,  38,  39,  44,  45,  46,  47,
  16, 17, 18, 19, 24, 25, 26, 27, 48,  49,  50,  51,  56,  57,  58,  59,
  20, 21, 22, 23, 28, 29, 30, 31, 52,  53,  54,  55,  60,  61,  62,  63,
  64, 65, 66, 67, 72, 73, 74, 75, 96,  97,  98,  99,  104, 105, 106, 107,
  68, 69, 70, 71, 76, 77, 78, 79, 100, 101, 102, 103, 108, 109, 110, 111,
  80, 81, 82, 83, 88, 89, 90, 91, 112, 113, 114, 115, 120, 121, 122, 123,
  84, 85, 86, 87, 92, 93, 94, 95, 116, 117, 118, 119, 124, 125, 126, 127,
};
static const uint16_t orders_8x8[256] = {
  0,   1,   4,   5,   16,  17,  20,  21,  64,  65,  68,  69,  80,  81,  84,
  85,  2,   3,   6,   7,   18,  19,  22,  23,  66,  67,  70,  71,  82,  83,
  86,  87,  8,   9,   12,  13,  24,  25,  28,  29,  72,  73,  76,  77,  88,
  89,  92,  93,  10,  11,  14,  15,  26,  27,  30,  31,  74,  75,  78,  79,
  90,  91,  94,  95,  32,  33,  36,  37,  48,  49,  52,  53,  96,  97,  100,
  101, 112, 113, 116, 117, 34,  35,  38,  39,  50,  51,  54,  55,  98,  99,
  102, 103, 114, 115, 118, 119, 40,  41,  44,  45,  56,  57,  60,  61,  104,
  105, 108, 109, 120, 121, 124, 125, 42,  43,  46,  47,  58,  59,  62,  63,
  106, 107, 110, 111, 122, 123, 126, 127, 128, 129, 132, 133, 144, 145, 148,
  149, 192, 193, 196, 197, 208, 209, 212, 213, 130, 131, 134, 135, 146, 147,
  150, 151, 194, 195, 198, 199, 210, 211, 214, 215, 136, 137, 140, 141, 152,
  153, 156, 157, 200, 201, 204, 205, 216, 217, 220, 221, 138, 139, 142, 143,
  154, 155, 158, 159, 202, 203, 206, 207, 218, 219, 222, 223, 160, 161, 164,
  165, 176, 177, 180, 181, 224, 225, 228, 229, 240, 241, 244, 245, 162, 163,
  166, 167, 178, 179, 182, 183, 226, 227, 230, 231, 242, 243, 246, 247, 168,
  169, 172, 173, 184, 185, 188, 189, 232, 233, 236, 237, 248, 249, 252, 253,
  170, 171, 174, 175, 186, 187, 190, 191, 234, 235, 238, 239, 250, 251, 254,
  255,
};

static const uint16_t *const orders[BLOCK_SIZES_ALL] = {
  //                              4X4
                                  NULL,
  // 4X8,         8X4,            8X8
  NULL,           NULL,           orders_8x8,
  // 8X16,        16X8,           16X16
  orders_8x16,    orders_16x8,    orders_16x16,
  // 16X32,       32X16,          32X32
  orders_16x32,   orders_32x16,   orders_32x32,
  // 32X64,       64X32,          64X64
  orders_32x64,   orders_64x32,   orders_64x64,
  // 64x128,      128x64,         128x128
  orders_64x128,  orders_128x64,  orders_128x128,
  // 4x16,        16x4,           8x32
  NULL,           NULL,           NULL,
  // 32x8,        16x64,          64x16
  NULL,           NULL,           NULL,
  // 32x128,      128x32
  NULL,           NULL
};

#define MAX_ANGLE_DELTA 3
#define ANGLE_STEP 3
int get_msb(unsigned int n) {
#ifdef LINUX
  unsigned int first_set_bit;
#else
  unsigned long first_set_bit;
#endif
  assert(n != 0);
  _BitScanReverse(&first_set_bit, n);
  return first_set_bit;
}

int get_unsigned_bits(unsigned int num_values) {
  return num_values > 0 ? get_msb(num_values) + 1 : 0;
}

#define av1_cost_zero(prob) (prob2bits[prob])
#define av1_cost_bit(prob, bit) av1_cost_zero((bit) ? 256 - (prob) : (prob))

int write_uniform_cost(int n, int v)
{
  int l = get_unsigned_bits(n), m = (1 << l) - n;
  if (l == 0) return 0;
  if (v < m)
    return (l - 1) * av1_cost_bit(128, 0);
  else
    return l * av1_cost_bit(128, 0);
}

int32_t HaveTopRight(BlockSize bsize, int32_t miRow, int32_t miCol, int32_t haveTop, int32_t haveRight, int32_t txsz, int32_t y, int32_t x, int32_t ss_x)
{
    if (!haveTop || !haveRight) return 0;

    const int bw_unit = block_size_wide[bsize] >> 2;
    const int plane_bw_unit = std::max(bw_unit >> ss_x, 1);
    const int top_right_count_unit = tx_size_wide_unit[txsz];

    if (y > 0) {  // Just need to check if enough pixels on the right.
        return x + top_right_count_unit < plane_bw_unit;
    } else {
        // All top-right pixels are in the block above, which is already available.
        if (x + top_right_count_unit < plane_bw_unit) return 1;

        const int bw_in_mi_log2 = block_size_wide_4x4_log2[bsize];
        const int bh_in_mi_log2 = block_size_high_4x4_log2[bsize];
        const int blk_row_in_sb = (miRow & MAX_MIB_MASK) << 1 >> bh_in_mi_log2;
        const int blk_col_in_sb = (miCol & MAX_MIB_MASK) << 1 >> bw_in_mi_log2;

        // Top row of superblock: so top-right pixels are in the top and/or
        // top-right superblocks, both of which are already available.
        if (blk_row_in_sb == 0) return 1;

        // Rightmost column of superblock (and not the top row): so top-right pixels
        // fall in the right superblock, which is not available yet.
        if (((blk_col_in_sb + 1) << bw_in_mi_log2) >= (MAX_MIB_SIZE << 1)) return 0;

        // General case (neither top row nor rightmost column): check if the
        // top-right block is coded before the current block.
        const uint16_t *const order = orders[bsize];
        const int this_blk_index =
            ((blk_row_in_sb + 0) << (MAX_MIB_SIZE_LOG2 + 2 - bw_in_mi_log2)) +
            blk_col_in_sb + 0;
        const uint16_t this_blk_order = order[this_blk_index];
        const int tr_blk_index =
            ((blk_row_in_sb - 1) << (MAX_MIB_SIZE_LOG2 + 2 - bw_in_mi_log2)) +
            blk_col_in_sb + 1;
        const uint16_t tr_blk_order = order[tr_blk_index];
        return tr_blk_order < this_blk_order;
    }
}

int HaveBottomLeft(BlockSize bsize, int32_t miRow, int32_t miCol, int32_t haveLeft, int32_t haveBottom, int32_t txsz, int32_t y, int32_t x, int32_t ss_y)
{
    if (!haveLeft || !haveBottom)
        return 0;

    const int32_t bh_unit = block_size_high[bsize] >> 2;
    const int32_t plane_bh_unit = std::max(bh_unit >> ss_y, 1);
    const int32_t bottom_left_count_unit = tx_size_high_unit[txsz];

    // Bottom-left pixels are in the bottom-left block, which is not available.
    if (x > 0)
        return 0;

    // All bottom-left pixels are in the left block, which is already available.
    if (y + bottom_left_count_unit < plane_bh_unit)
        return 1;

    const int32_t bw_in_mi_log2 = block_size_wide_4x4_log2[bsize];
    const int32_t bh_in_mi_log2 = block_size_high_4x4_log2[bsize];
    const int32_t blk_row_in_sb = (miRow & MAX_MIB_MASK) << 1 >> bh_in_mi_log2;
    const int32_t blk_col_in_sb = (miCol & MAX_MIB_MASK) << 1 >> bw_in_mi_log2;

    // Leftmost column of superblock: so bottom-left pixels maybe in the left
    // and/or bottom-left superblocks. But only the left superblock is
    // available, so check if all required pixels fall in that superblock.
    if (blk_col_in_sb == 0) {
        const int32_t blk_start_row_off = blk_row_in_sb << (bh_in_mi_log2 + MI_SIZE_LOG2-1 - 2) >> ss_y;
        const int32_t row_off_in_sb = blk_start_row_off + y;
        const int32_t sb_height_unit = MAX_MIB_SIZE << 1 << (MI_SIZE_LOG2-1 - 2) >> ss_y;
        return row_off_in_sb + bottom_left_count_unit < sb_height_unit;
    }

    // Bottom row of superblock (and not the leftmost column): so bottom-left
    // pixels fall in the bottom superblock, which is not available yet.
    if (((blk_row_in_sb + 1) << bh_in_mi_log2) >= (MAX_MIB_SIZE << 1)) return 0;

    // General case (neither leftmost column nor bottom row): check if the
    // bottom-left block is coded before the current block.
    const uint16_t *const order = orders[bsize];
    const int32_t this_blk_index =
        ((blk_row_in_sb + 0) << (MAX_MIB_SIZE_LOG2 + 2 - bw_in_mi_log2)) +
        blk_col_in_sb + 0;
    const uint16_t this_blk_order = order[this_blk_index];
    const int32_t bl_blk_index =
        ((blk_row_in_sb + 1) << (MAX_MIB_SIZE_LOG2 + 2 - bw_in_mi_log2)) +
        blk_col_in_sb - 1;
    const uint16_t bl_blk_order = order[bl_blk_index];
    return bl_blk_order < this_blk_order;
}

template <typename PixType>
void av1_filter_intra_edge_corner(PixType *above, PixType *left)
{
    const int32_t kernel[3] = { 5, 6, 5 };
    int32_t s = (left[0] * kernel[0]) + (above[-1] * kernel[1]) + (above[0] * kernel[2]);
    s = (s + 8) >> 4;
    above[-1] = s;
    left[-1] = s;
}

template <typename PixType>
void av1_filter_intra_edge_corner_nv12(PixType *above, PixType *left)
{
    const int32_t kernel[3] = { 5, 6, 5 };
    // u
    int32_t s = (left[0] * kernel[0]) + (above[-2] * kernel[1]) + (above[0] * kernel[2]);
    s = (s + 8) >> 4;
    above[-2] = s;
    left[-2] = s;
    // v
    s = (left[1] * kernel[0]) + (above[-1] * kernel[1]) + (above[1] * kernel[2]);
    s = (s + 8) >> 4;
    above[-1] = s;
    left[-1] = s;
}

int32_t is_smooth(PredMode mode) {
  return (mode == SMOOTH_PRED || mode == SMOOTH_V_PRED || mode == SMOOTH_H_PRED);
}

int32_t get_filt_type(const ModeInfo *above, const ModeInfo *left, int32_t plane) {
  const int32_t ab_sm = above ? is_smooth(plane == PLANE_TYPE_Y ? above->mode : above->modeUV) : 0;
  const int32_t le_sm = left  ? is_smooth(plane == PLANE_TYPE_Y ? left->mode  : left->modeUV)  : 0;

  return (ab_sm || le_sm) ? 1 : 0;
}

int32_t intra_edge_filter_strength(int32_t bs0, int32_t bs1, int32_t delta, int32_t type)
{
    const int32_t d = abs(delta);
    const int32_t blk_wh = bs0 + bs1;

    int32_t strength = 0;
    if (type == 0) {
        if (blk_wh <= 8) {
            if (d >= 56) strength = 1;
        } else if (blk_wh <= 12) {
            if (d >= 40) strength = 1;
        } else if (blk_wh <= 16) {
            if (d >= 40) strength = 1;
        } else if (blk_wh <= 24) {
            if (d >= 8) strength = 1;
            if (d >= 16) strength = 2;
            if (d >= 32) strength = 3;
        } else if (blk_wh <= 32) {
            if (d >= 1) strength = 1;
            if (d >= 4) strength = 2;
            if (d >= 32) strength = 3;
        } else {
            if (d >= 1) strength = 3;
        }
    } else {
        if (blk_wh <= 8) {
            if (d >= 40) strength = 1;
            if (d >= 64) strength = 2;
        } else if (blk_wh <= 16) {
            if (d >= 20) strength = 1;
            if (d >= 48) strength = 2;
        } else if (blk_wh <= 24) {
            if (d >= 4) strength = 3;
        } else {
            if (d >= 1) strength = 3;
        }
    }
    return strength;
}

int32_t use_intra_edge_upsample(int32_t bs0, int32_t bs1, int32_t delta, int32_t type) {
    const int32_t d = abs(delta);
    const int32_t blk_wh = bs0 + bs1;
    if (d <= 0 || d >= 40)
        return 0;
    return type ? (blk_wh <= 8) : (blk_wh <= 16);
}

const uint8_t intra_edge_kernel[INTRA_EDGE_FILT][INTRA_EDGE_TAPS] = {
    { 0, 4, 8, 4, 0 }, { 0, 5, 6, 5, 0 }, { 2, 4, 4, 4, 2 }
};

void av1_filter_intra_edge_c(uint8_t *p, int32_t sz, int32_t strength)
{
    if (!strength) return;

    const int32_t filt = strength - 1;

    uint8_t edge[32 * 2 + 1];
    memcpy(edge, p, sz);

    for (int i = 1; i < sz; i++) {
        int s = 0;
        for (int j = 0; j < INTRA_EDGE_TAPS; j++) {
            int k = i - 2 + j;
            k = (k < 0) ? 0 : k;
            k = (k > sz - 1) ? sz - 1 : k;
            s += edge[k] * intra_edge_kernel[filt][j];
        }
        s = (s + 8) >> 4;
        p[i] = s;
    }
}
void av1_filter_intra_edge_c(uint16_t *p, int32_t sz, int32_t strength) {
    p; sz; strength;
    assert(!"no implemented");
}


void av1_filter_intra_edge_nv12_c(uint8_t *p, int32_t sz, int32_t strength)
{
    assert(sz <= 65);
    if (!strength) return;

    uint8_t u[65], v[65];
    for (int32_t i = 0; i < sz; i++) {
        u[i] = p[2 * i + 0];
        v[i] = p[2 * i + 1];
    }
    av1_filter_intra_edge_c(u, sz, strength);
    av1_filter_intra_edge_c(v, sz, strength);
    for (int32_t i = 0; i < sz; i++) {
        p[2 * i + 0] = u[i];
        p[2 * i + 1] = v[i];
    }
}
void av1_filter_intra_edge_nv12_c(uint16_t *p, int32_t sz, int32_t strength) {
    p; sz; strength;
    assert(!"no implemented");
}


void av1_upsample_intra_edge_c(uint8_t *p, int32_t sz) {
    // interpolate half-sample positions
    assert(sz <= MAX_UPSAMPLE_SZ);

    uint8_t in[MAX_UPSAMPLE_SZ + 3];
    // copy p[-1..(sz-1)] and extend first and last samples
    in[0] = p[-1];
    in[1] = p[-1];
    for (int i = 0; i < sz; i++) {
        in[i + 2] = p[i];
    }
    in[sz + 2] = p[sz - 1];

    // interpolate half-sample edge positions
    p[-2] = in[0];
    for (int i = 0; i < sz; i++) {
        int s = -in[i] + (9 * in[i + 1]) + (9 * in[i + 2]) - in[i + 3];
        s = Saturate(0, 255, (s + 8) >> 4);
        p[2 * i - 1] = s;
        p[2 * i] = in[i + 2];
    }
}
void av1_upsample_intra_edge_c(uint16_t *p, int32_t sz) {
    p; sz;
    assert(!"no implemented");
}


void av1_upsample_intra_edge_nv12_c(uint8_t *p, int32_t sz) {
    assert(sz <= MAX_UPSAMPLE_SZ);

    uint8_t u[2 * MAX_UPSAMPLE_SZ + 3], v[2 * MAX_UPSAMPLE_SZ + 3];
    uint8_t *pu = u + 2;
    uint8_t *pv = v + 2;
    for (int32_t i = -1; i < sz; i++) {
        pu[i] = p[2 * i + 0];
        pv[i] = p[2 * i + 1];
    }
    av1_upsample_intra_edge_c(pu, sz);
    av1_upsample_intra_edge_c(pv, sz);
    for (int32_t i = -2; i < 2 * sz - 1; i++) {
        p[2 * i + 0] = pu[i];
        p[2 * i + 1] = pv[i];
    }
}
void av1_upsample_intra_edge_nv12_c(uint16_t *p, int32_t sz) {
    p; sz;
    assert(!"no implemented");
}


template <typename PixType>
RdCost TransformIntraYSbAv1(int32_t bsz, int32_t mode, int32_t haveTop, int32_t haveLeft_, TxSize txSize,
                            TxType txType, uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src_, PixType *rec_,
                            int32_t pitchRec, int16_t *diff, int16_t *coeff_, int16_t *qcoeff_, int16_t *coefWork, int32_t qp,
                            const BitCounts &bc, CostType lambda, int32_t miRow, int32_t miCol, int32_t miColEnd,
                            int32_t deltaAngle, int32_t filtType, const AV1VideoParam &par, const QuantParam &qpar, float *roundFAdj)
{
    AV1PP::IntraPredPels<PixType> predPels;

    const TxbBitCounts &cbc = bc.txb[ get_q_ctx(qp) ][txSize][PLANE_TYPE_Y];
    const int32_t num4x4w = block_size_wide_4x4[bsz];
    const int32_t num4x4h = block_size_high_4x4[bsz];
    const int16_t *scan = av1_scans[txSize][txType].scan;
    int32_t width = 4 << txSize;
    int32_t step = 1 << txSize;
    RdCost rd = {};
    int32_t txw = tx_size_wide_unit[txSize];
    const int32_t txwpx = 4 << txSize;
    const int32_t txhpx = 4 << txSize;
    const int32_t bh = block_size_high_8x8[bsz];
    const int32_t bw = block_size_wide_8x8[bsz];
    const int32_t distToBottomEdge = (par.miRows - bh - miRow) * 8;
    const int32_t distToRightEdge = (par.miCols - bw - miCol) * 8;
    const int32_t hpx = block_size_high[bsz];
    const int32_t wpx = hpx;//block_size_width[bsz];
    const int32_t isDirectionalMode = av1_is_directional_mode(mode);
    const int32_t angle = mode_to_angle_map[mode] + deltaAngle * ANGLE_STEP;
    int16_t *coefOrigin = coefWork;

    for (int32_t y = 0; y < num4x4h; y += step) {
        const PixType *src = src_ + (y << 2) * SRC_PITCH;
        PixType *rec = rec_ + (y << 2) * pitchRec;
        int32_t haveLeft = haveLeft_;
        for (int32_t x = 0; x < num4x4w; x += step, rec += width, src += width, diff += width) {
            int32_t blockIdx = h265_scan_r2z4[y * 16 + x];
            int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
            CoeffsType *coeff  = coeff_ + offset;
            CoeffsType *qcoeff = qcoeff_ + offset;

            const int32_t haveRight = (miCol + ((x + txw) >> 1)) < miColEnd;
            const int32_t haveTopRight = HaveTopRight(bsz, miRow, miCol, haveTop, haveRight, txSize, y, x, 0);
            // Distance between the right edge of this prediction block to the frame right edge
            const int32_t xr = distToRightEdge + (wpx - (x<<2) - txwpx);
            // Distance between the bottom edge of this prediction block to the frame bottom edge
            const int32_t yd = distToBottomEdge + (hpx - (y<<2) - txhpx);
            const int32_t haveBottom = yd > 0;
            const int32_t haveBottomLeft = HaveBottomLeft(bsz, miRow, miCol, haveLeft, haveBottom, txSize, y, x, 0);
            const int32_t pixTop = haveTop ? std::min(txwpx, xr + txwpx) : 0;
            const int32_t pixTopRight = haveTopRight ? std::min(txwpx, xr) : 0;
            const int32_t pixLeft = haveLeft ? std::min(txhpx, yd + txhpx) : 0;
            const int32_t pixBottomLeft = haveBottomLeft ? std::min(txhpx, yd) : 0;
            AV1PP::GetPredPelsAV1<PLANE_TYPE_Y>(rec, pitchRec, predPels.top, predPels.left, width, haveTop, haveLeft, pixTopRight, pixBottomLeft);

            int32_t upsampleTop = 0;
            int32_t upsampleLeft = 0;
            if (par.enableIntraEdgeFilter && isDirectionalMode) {
                const int32_t need_top = angle < 180;
                const int32_t need_left = angle > 90;
                const int32_t need_right = angle < 90;
                const int32_t need_bottom = angle > 180;
                if (angle != 90 && angle != 180) {
                    if (need_top && need_left && (txwpx + txhpx >= 24))
                        av1_filter_intra_edge_corner(predPels.top, predPels.left);
                    if (haveTop) {
                        const int32_t strength = intra_edge_filter_strength(txwpx, txhpx, angle - 90, filtType);
                        const int32_t sz = 1 + pixTop + (need_right ? txhpx : 0);
                        av1_filter_intra_edge_c(predPels.top - 1, sz, strength);
                    }
                    if (haveLeft) {
                        const int32_t strength = intra_edge_filter_strength(txhpx, txhpx, angle - 180, filtType);
                        const int32_t sz = 1 + pixLeft + (need_bottom ? txwpx : 0);
                        av1_filter_intra_edge_c(predPels.left - 1, sz, strength);
                    }
                }
                upsampleTop = use_intra_edge_upsample(txwpx, txhpx, angle - 90, filtType);
                if (need_top && upsampleTop) {
                    const int32_t sz = txwpx + (need_right ? txhpx : 0);
                    av1_upsample_intra_edge_c(predPels.top, sz);
                }
                upsampleLeft = use_intra_edge_upsample(txwpx, txhpx, angle - 180, filtType);
                if (need_left && upsampleLeft) {
                    const int32_t sz = txhpx + (need_bottom ? txwpx : 0);
                    av1_upsample_intra_edge_c(predPels.left, sz);
                }
            }

            AV1PP::predict_intra_av1(predPels.top, predPels.left, rec, pitchRec, txSize, haveLeft, haveTop,
                                     mode, deltaAngle, upsampleTop, upsampleLeft);

            int32_t txSse = AV1PP::sse(src, SRC_PITCH, rec, pitchRec, txSize, txSize);
            AV1PP::diff_nxn(src, SRC_PITCH, rec, pitchRec, diff, width, txSize);

#ifdef ADAPTIVE_DEADZONE
            AV1PP::ftransform_av1(diff, coefOrigin, width, txSize, txType);
            int32_t eob = AV1PP::quant(coefOrigin, qcoeff, qpar, txSize);
            if(eob) AV1PP::dequant(qcoeff, coeff, qpar, txSize);
            if(eob && roundFAdj) adaptDz(coefOrigin, coeff, reinterpret_cast<const int16_t *>(&qpar), txSize, roundFAdj, eob);
#else
            AV1PP::ftransform_av1(diff, coeff, width, txSize, txType);
            int32_t eob = AV1PP::quant_dequant(coeff, qcoeff, qpar, txSize);
#endif

            const int32_t txbSkipCtx = GetTxbSkipCtx(bsz, txSize, 0, aboveNzCtx+x, leftNzCtx+y);
            uint32_t txBits;

            if (eob) {
                AV1PP::itransform_add_av1(coeff, rec, pitchRec, txSize, txType);
                int32_t sseNz = AV1PP::sse(src, SRC_PITCH, rec, pitchRec, txSize, txSize);
                int32_t culLevel;
                uint32_t bitsNz = EstimateCoefsAv1(cbc, txbSkipCtx, eob, qcoeff, &culLevel);
                txBits = bitsNz;
                txSse = sseNz;
                SetCulLevel(aboveNzCtx+x, leftNzCtx+y, culLevel, txSize);
            } else {
                txBits = cbc.txbSkip[txbSkipCtx][1];
                SetCulLevel(aboveNzCtx+x, leftNzCtx+y, 0, txSize);
            }

            rd.eob += eob;
            rd.sse += txSse;
            rd.coefBits += txBits;
            haveLeft = 1;
        }
        haveTop = 1;
    }
    return rd;
}
template RdCost TransformIntraYSbAv1<uint8_t> (
    int32_t,int32_t,int32_t,int32_t,TxSize,TxType,uint8_t*,uint8_t*,const uint8_t*, uint8_t*, int32_t,int16_t*,int16_t*,int16_t*, int16_t*,
    int32_t,const BitCounts&,CostType,int32_t,int32_t,int32_t,int32_t,int32_t,const AV1VideoParam &par, const QuantParam &qpar, float *roundFAdj);

template RdCost TransformIntraYSbAv1<uint16_t>(
    int32_t,int32_t,int32_t,int32_t,TxSize,TxType,uint8_t*,uint8_t*,const uint16_t*,uint16_t*,int32_t,int16_t*,int16_t*,int16_t*, int16_t*,
    int32_t,const BitCounts&,CostType,int32_t,int32_t,int32_t,int32_t,int32_t,const AV1VideoParam &par, const QuantParam &qpar, float *roundFAdj);


template <typename PixType>
RdCost TransformIntraYSbAv1_viaTxkSearch(int32_t bsz, int32_t mode, int32_t haveTop, int32_t haveLeft_, TxSize txSize,
                            uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src_, PixType *rec_,
                            int32_t pitchRec, int16_t *diff_, int16_t *coeff_, int16_t *qcoeff_, int32_t qp,
                            const BitCounts &bc, int32_t fastCoeffCost, CostType lambda, int32_t miRow, int32_t miCol,
                            int32_t miColEnd, int32_t miRows, int32_t miCols, int32_t deltaAngle, int32_t filtType, uint16_t* txkTypes,
                            const AV1VideoParam &par, int16_t *coeffWork_, const QuantParam &qpar, float *roundFAdj)
{
    AV1PP::IntraPredPels<PixType> predPels;

    const TxbBitCounts &cbc = bc.txb[ get_q_ctx(qp) ][txSize][PLANE_TYPE_Y];
    const int32_t num4x4w = block_size_wide_4x4[bsz];
    const int32_t num4x4h = block_size_high_4x4[bsz];

    int32_t width = 4 << txSize;
    int32_t step = 1 << txSize;
    RdCost rd = {};
    int32_t txw = tx_size_wide_unit[txSize];
    const int32_t txwpx = 4 << txSize;
    const int32_t txhpx = 4 << txSize;
    const int32_t bh = block_size_high_8x8[bsz];
    const int32_t bw = block_size_wide_8x8[bsz];
    const int32_t distToBottomEdge = (miRows - bh - miRow) * 8;
    const int32_t distToRightEdge = (miCols - bw - miCol) * 8;
    const int32_t hpx = block_size_high[bsz];
    const int32_t wpx = hpx;//block_size_width[bsz];
    const int32_t isDirectionalMode = av1_is_directional_mode(mode);
    const int32_t angle = mode_to_angle_map[mode] + deltaAngle * ANGLE_STEP;

    const int32_t shiftTab[4] = {6, 6, 6, 4};
    const int32_t SHIFT_SSE = shiftTab[txSize];

    //alignas(64) int16_t coefY[64*64]
    CoeffsType* coeffOrigin = coeffWork_;
    CoeffsType* coeffWork   = coeffWork_ + 32*32;
    CoeffsType* qcoeffWork  = coeffWork_ + 3*32*32;

    for (int32_t y = 0; y < num4x4h; y += step) {
        const PixType *src = src_ + (y << 2) * SRC_PITCH;
        PixType *rec = rec_ + (y << 2) * pitchRec;
        int32_t haveLeft = haveLeft_;
        int16_t *diff = diff_ + (y << 2) * wpx;
        for (int32_t x = 0; x < num4x4w; x += step, rec += width, src += width, diff += width) {
            int32_t blockIdx = h265_scan_r2z4[y * 16 + x];
            int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
            CoeffsType *coeff  = coeff_ + offset;
            CoeffsType *qcoeff = qcoeff_ + offset;

            const int32_t haveRight = (miCol + ((x + txw) >> 1)) < miColEnd;
            const int32_t haveTopRight = HaveTopRight(bsz, miRow, miCol, haveTop, haveRight, txSize, y, x, 0);
            // Distance between the right edge of this prediction block to the frame right edge
            const int32_t xr = distToRightEdge + (wpx - (x<<2) - txwpx);
            // Distance between the bottom edge of this prediction block to the frame bottom edge
            const int32_t yd = distToBottomEdge + (hpx - (y<<2) - txhpx);
            const int32_t haveBottom = yd > 0;
            const int32_t haveBottomLeft = HaveBottomLeft(bsz, miRow, miCol, haveLeft, haveBottom, txSize, y, x, 0);
            const int32_t pixTop = haveTop ? std::min(txwpx, xr + txwpx) : 0;
            const int32_t pixTopRight = haveTopRight ? std::min(txwpx, xr) : 0;
            const int32_t pixLeft = haveLeft ? std::min(txhpx, yd + txhpx) : 0;
            const int32_t pixBottomLeft = haveBottomLeft ? std::min(txhpx, yd) : 0;
            AV1PP::GetPredPelsAV1<PLANE_TYPE_Y>(rec, pitchRec, predPels.top, predPels.left, width, haveTop, haveLeft, pixTopRight, pixBottomLeft);

            int32_t upsampleTop = 0;
            int32_t upsampleLeft = 0;
            if (par.enableIntraEdgeFilter && isDirectionalMode) {
                const int32_t need_top = angle < 180;
                const int32_t need_left = angle > 90;
                const int32_t need_right = angle < 90;
                const int32_t need_bottom = angle > 180;
                if (angle != 90 && angle != 180) {
                    if (need_top && need_left && (txwpx + txhpx >= 24))
                        av1_filter_intra_edge_corner(predPels.top, predPels.left);
                    if (haveTop) {
                        const int32_t strength = intra_edge_filter_strength(txwpx, txhpx, angle - 90, filtType);
                        const int32_t sz = 1 + pixTop + (need_right ? txhpx : 0);
                        av1_filter_intra_edge_c(predPels.top - 1, sz, strength);
                    }
                    if (haveLeft) {
                        const int32_t strength = intra_edge_filter_strength(txhpx, txhpx, angle - 180, filtType);
                        const int32_t sz = 1 + pixLeft + (need_bottom ? txwpx : 0);
                        av1_filter_intra_edge_c(predPels.left - 1, sz, strength);
                    }
                }
                upsampleTop = use_intra_edge_upsample(txwpx, txhpx, angle - 90, filtType);
                if (need_top && upsampleTop) {
                    const int32_t sz = txwpx + (need_right ? txhpx : 0);
                    av1_upsample_intra_edge_c(predPels.top, sz);
                }
                upsampleLeft = use_intra_edge_upsample(txwpx, txhpx, angle - 180, filtType);
                if (need_left && upsampleLeft) {
                    const int32_t sz = txhpx + (need_bottom ? txwpx : 0);
                    av1_upsample_intra_edge_c(predPels.left, sz);
                }
            }

            AV1PP::predict_intra_av1(predPels.top, predPels.left, rec, pitchRec, txSize, haveLeft, haveTop, mode, deltaAngle, upsampleTop, upsampleLeft);
            const int32_t initTxSse = AV1PP::sse(src, SRC_PITCH, rec, pitchRec, txSize, txSize);
            AV1PP::diff_nxn(src, SRC_PITCH, rec, pitchRec, diff, width, txSize);

            const int32_t txbSkipCtx = GetTxbSkipCtx(bsz, txSize, 0, aboveNzCtx+x, leftNzCtx+y);
            int32_t stopTxType = (txSize == TX_32X32) ? DCT_DCT : ADST_ADST;
            CostType bestCost = COST_MAX;
            uint8_t bestTxType = DCT_DCT;
            int32_t bestEob = 0;
            float roundFAdjInit[2] = { roundFAdj[0], roundFAdj[1] };
            CoeffsType* coeffTest  = coeffWork;
            CoeffsType* qcoeffTest = qcoeffWork;
            CoeffsType* coeffBest  = coeffWork  + 32*32;
            CoeffsType* qcoeffBest = qcoeffWork + 32*32;

            for (int32_t testTxType = DCT_DCT; testTxType <= stopTxType; testTxType++) {

                const int16_t *scan = av1_scans[txSize][testTxType].scan;
                const TxType txTypeNom = GetTxTypeAV1(PLANE_TYPE_Y, txSize, mode, testTxType);
                const uint32_t txTypeBits = bc.intraExtTx[txSize][txTypeNom][testTxType];
                uint32_t txBits;
                int32_t txSse;
                int32_t culLevel;

                float roundFAdjT[2] = { roundFAdjInit[0], roundFAdjInit[1] };

                AV1PP::ftransform_av1(diff, coeffOrigin, width, txSize, testTxType);

                int32_t eob = AV1PP::quant(coeffOrigin, qcoeffTest, qpar, txSize);
                if (eob) {
                    AV1PP::dequant(qcoeffTest, coeffTest, qpar, txSize);
                    int32_t sseNz = AV1PP::sse_cont(coeffOrigin, coeffTest, width*width) >> SHIFT_SSE;
                    uint32_t bitsNz = EstimateCoefsAv1(cbc, txbSkipCtx, eob, qcoeffTest, &culLevel);
                    bitsNz += txTypeBits;
                    txBits = bitsNz;
                    txSse = sseNz;
                } else {
                    culLevel = 0;
                    txBits = cbc.txbSkip[txbSkipCtx][1];
                    txSse = initTxSse;
                }

                CostType cost = txSse + lambda * txBits;
                if (cost < bestCost) {
#ifdef ADAPTIVE_DEADZONE
                    if (eob)
                        adaptDz(coeffOrigin, coeffTest, reinterpret_cast<const int16_t *>(&qpar), txSize, &roundFAdjT[0], eob);
#endif
                    bestCost = cost;
                    bestTxType = testTxType;
                    bestEob = eob;
                    std::swap(coeffTest, coeffBest);
                    std::swap(qcoeffTest, qcoeffBest);
                    roundFAdj[0] = roundFAdjT[0];
                    roundFAdj[1] = roundFAdjT[1];
                }
            }

            txkTypes[y * 16 + x] = bestTxType;

            const int16_t *scan = av1_scans[txSize][bestTxType].scan;
            int32_t eob = bestEob;
            int32_t txSse;
            uint32_t txBits;
            CopyCoeffs(qcoeffBest, qcoeff, width * width);
            if (eob) {
                AV1PP::itransform_add_av1(coeffBest, rec, pitchRec, txSize, bestTxType);
                int32_t sseNz = AV1PP::sse(src, SRC_PITCH, rec, pitchRec, txSize, txSize);
                int32_t culLevel;
                uint32_t bitsNz = EstimateCoefsAv1(cbc, txbSkipCtx, eob, qcoeff, &culLevel);
                const TxType txTypeNom = GetTxTypeAV1(PLANE_TYPE_Y, txSize, mode, bestTxType);
                const uint32_t txTypeBits = bc.intraExtTx[txSize][txTypeNom][bestTxType];
                bitsNz += txTypeBits;
                txBits = bitsNz;
                txSse = sseNz;
                SetCulLevel(aboveNzCtx+x, leftNzCtx+y, culLevel, txSize);
            } else {
                txSse = initTxSse;
                txBits = cbc.txbSkip[txbSkipCtx][1];
                SetCulLevel(aboveNzCtx+x, leftNzCtx+y, 0, txSize);
            }

            rd.eob += eob;
            rd.sse += txSse;
            rd.coefBits += txBits;
            haveLeft = 1;
        }
        haveTop = 1;
    }
    return rd;
}
template RdCost TransformIntraYSbAv1_viaTxkSearch<uint8_t> (
    int32_t,int32_t,int32_t,int32_t,TxSize,uint8_t*,uint8_t*,const uint8_t*, uint8_t*, int32_t,int16_t*,int16_t*,int16_t*,
    int32_t,const BitCounts&,int32_t,CostType,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,uint16_t*,
    const AV1VideoParam &par, int16_t* coeffWork_, const QuantParam &qpar, float *roundFAdj);

template RdCost TransformIntraYSbAv1_viaTxkSearch<uint16_t>(
    int32_t,int32_t,int32_t,int32_t,TxSize,uint8_t*,uint8_t*,const uint16_t*,uint16_t*,int32_t,int16_t*,int16_t*,int16_t*,
    int32_t,const BitCounts&,int32_t,CostType,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,uint16_t*,
    const AV1VideoParam &par, int16_t* coeffWork_, const QuantParam &qpar, float *roundFAdj);
//---------------------------------------------------------
template <typename PixType>
RdCost TransformIntraYSbVp9(int32_t bsz, int32_t mode, int32_t haveTop, int32_t haveLeft_, int32_t notOnRight4x4,
                           TxSize txSize, uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src_, PixType *rec_,
                           int32_t pitchRec, int16_t *diff, int16_t *coeff_, int16_t *qcoeff_, const QuantParam &qpar,
                           const BitCounts &bc, int32_t fastCoeffCost, CostType lambda, int32_t miRow, int32_t miCol,
                           int32_t miColEnd, int32_t miRows, int32_t miCols)
{
    alignas(32) PixType predPels_[32/sizeof(PixType) + (4<<TX_32X32) * 3 + (4<<TX_32X32)];
    PixType *predPels = predPels_ + 32/sizeof(PixType) - 1;

    const int32_t bitDepth = (sizeof(PixType) == 1 ? 8 : 10);
    const PixType halfRange = 1 << (bitDepth - 1);
    const PixType halfRangeM1 = halfRange - 1;
    const PixType halfRangeP1 = halfRange + 1;
    const CoefBitCounts &cbc = bc.coef[PLANE_TYPE_Y][0][txSize];
    const int32_t num4x4w = block_size_wide_4x4[bsz];
    const int32_t num4x4h = block_size_high_4x4[bsz];
    const int32_t txType = (txSize == TX_32X32) ? DCT_DCT : mode2txfm_map[mode];
    const int16_t *scan = vp9_scans[txSize][txType].scan;
    int32_t width = 4 << txSize;
    int32_t step = 1 << txSize;
    RdCost rd = {};
    int32_t txw = tx_size_wide_unit[txSize];
    const int32_t txwpx = 4 << txSize;
    const int32_t txhpx = 4 << txSize;
    const int32_t bh = block_size_high_8x8[bsz];
    const int32_t bw = block_size_wide_8x8[bsz];
    const int32_t distToBottomEdge = (miRows - bh - miRow) * 8 ;
    const int32_t distToRightEdge = (miCols - bw - miCol) * 8;
    const int32_t hpx = block_size_high[bsz];
    const int32_t wpx = hpx;//block_size_width[bsz];

    for (int32_t y = 0; y < num4x4h; y += step) {
        const PixType *src = src_ + (y << 2) * SRC_PITCH;
        PixType *rec = rec_ + (y << 2) * pitchRec;
        int32_t haveLeft = haveLeft_;
        for (int32_t x = 0; x < num4x4w; x += step, rec += width, src += width, diff += width) {
            int32_t blockIdx = h265_scan_r2z4[y * 16 + x];
            int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
            CoeffsType *coeff  = coeff_ + offset;
            CoeffsType *qcoeff = qcoeff_ + offset;
            int32_t notOnRight = (x + step < num4x4w) || notOnRight4x4;

            AV1PP::GetPredPelsLuma(rec, pitchRec, predPels, width, halfRangeM1, halfRangeP1, haveTop, haveLeft, notOnRight);
            AV1PP::predict_intra_vp9(predPels, rec, pitchRec, txSize, haveLeft, haveTop, mode);

            int32_t txSse = AV1PP::sse(src, SRC_PITCH, rec, pitchRec, txSize, txSize);
            AV1PP::diff_nxn(src, SRC_PITCH, rec, pitchRec, diff, width, txSize);
            AV1PP::ftransform_vp9(diff, coeff, width, txSize, txType);
            int32_t eob = AV1PP::quant_dequant(coeff, qcoeff, qpar, txSize);

            int32_t dcCtx = GetDcCtx(aboveNzCtx+x, leftNzCtx+y, step);
            uint32_t txBits = cbc.moreCoef[0][dcCtx][0]; // more_coef=0

            if (eob) {
                AV1PP::itransform_add_vp9(coeff, rec, pitchRec, txSize, txType);
                int32_t sseNz = AV1PP::sse(src, SRC_PITCH, rec, pitchRec, txSize, txSize);
                uint32_t bitsNz = EstimateCoefs(fastCoeffCost, cbc, qcoeff, scan, eob, dcCtx, txType);
                if (txSse + lambda * txBits < sseNz + lambda * bitsNz) {
                    eob = 0;
                    ZeroCoeffs(qcoeff, 16 << (txSize << 1)); // zero coeffs
                    AV1PP::predict_intra_vp9(predPels, rec, pitchRec, txSize, haveLeft, haveTop, mode); // re-predict intra
                    small_memset0(aboveNzCtx+x, step);
                    small_memset0(leftNzCtx+y, step);
                } else {
                    txBits = bitsNz;
                    txSse = sseNz;
                    small_memset1(aboveNzCtx+x, step);
                    small_memset1(leftNzCtx+y, step);
                }
            } else {
                small_memset0(aboveNzCtx+x, step);
                small_memset0(leftNzCtx+y, step);
            }

            rd.eob += eob;
            rd.sse += txSse;
            rd.coefBits += txBits;
            haveLeft = 1;
        }
        haveTop = 1;
    }
    return rd;
}
template RdCost TransformIntraYSbVp9<uint8_t> (
    int32_t,int32_t,int32_t,int32_t,int32_t,TxSize,uint8_t*,uint8_t*,const uint8_t*, uint8_t*, int32_t,int16_t*,int16_t*,int16_t*,
    const QuantParam&,const BitCounts&,int32_t,CostType,int32_t,int32_t,int32_t,int32_t,int32_t);

template RdCost TransformIntraYSbVp9<uint16_t>(
    int32_t,int32_t,int32_t,int32_t,int32_t,TxSize,uint8_t*,uint8_t*,const uint16_t*,uint16_t*,int32_t,int16_t*,int16_t*,int16_t*,
    const QuantParam&,const BitCounts&,int32_t,CostType,int32_t,int32_t,int32_t,int32_t,int32_t);

template <typename PixType>
void AV1CU<PixType>::CheckIntra8x4(int32_t absPartIdx)
{
    const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
    const int32_t x4 = (rasterIdx & 15);
    const int32_t y4 = (rasterIdx >> 4);
    const int32_t miRow = (m_ctbPelY >> 3) + ((av1_scan_z2r4[absPartIdx] >> 4) >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + ((av1_scan_z2r4[absPartIdx] & 15) >> 1);
    const int32_t haveTop = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft  = (miCol > m_tileBorders.colStart);
    const int32_t notOnRight = 0;
    const PixType halfRange = 1<<(m_par->bitDepthLuma-1);
    const PixType halfRangeM1 = halfRange-1;
    const PixType halfRangeP1 = halfRange+1;
    const QuantParam &qparam = m_aqparamY;

    const int32_t miColEnd = m_tileBorders.colEnd;

    PixType *recSb = m_yRec + ((x4 + y4 * m_pitchRecLuma) << LOG2_MIN_TU_SIZE);
    PixType *srcSb = m_ySrc + ((x4 + y4 * SRC_PITCH) << LOG2_MIN_TU_SIZE);
    int16_t *diff = vp9scratchpad.diffY;
    int16_t *coef = vp9scratchpad.coefY;
    int16_t *qcoef = vp9scratchpad.qcoefY[0];
    int16_t *bestQcoef = vp9scratchpad.qcoefY[1];

    alignas(64) PixType bestRec[8*4];
    Contexts bestCtx;
    Contexts origCtx;

    const BitCounts &bc = m_currFrame->bitCount;
    const CoefBitCounts &cbc = bc.coef[PLANE_TYPE_Y][0][TX_4X4];
    ModeInfo *mi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const int32_t ctxSkip = GetCtxSkip(above, left);
    const uint16_t *skipBits = bc.skip[ctxSkip];
    const int32_t leftMode[2] = { left ? left[0].mode : DC_PRED, left ? left[2].mode : DC_PRED };
    const int32_t ctxIsInter = m_currFrame->IsIntra() ? 0 : GetCtxIsInter(above, left);
    const uint32_t isInterBits = m_currFrame->IsIntra() ? 0 : bc.isInter[ctxIsInter][0];

    RdCost totalCostY = {0,0,0};

    int32_t aboveMode = (above ? above->mode : DC_PRED);
    for (int32_t y = 0; y < 2; y++) {
        PixType *rec = recSb + 4*y*m_pitchRecLuma;
        PixType *src = srcSb + 4*y*SRC_PITCH;
        origCtx = m_contexts;

        const uint16_t *intraModeBits = m_currFrame->IsIntra() ? bc.kfIntraMode[aboveMode][ leftMode[y] ] : bc.intraMode[TX_4X4];

        CostType bestCost = COST_MAX;
        RdCost bestRdCost = {};
        int32_t bestMode = INTRA_MODES;
        uint8_t *anz = m_contexts.aboveNonzero[0]+x4;
        uint8_t *lnz = m_contexts.leftNonzero[0]+y4+y;

        for (int32_t i = DC_PRED; i < INTRA_MODES; i++) {
            m_contexts = origCtx;

            RdCost rd = TransformIntraYSbVp9(BLOCK_8X4, i, haveTop|y, haveLeft, 0, TX_4X4, anz, lnz, src, rec,
                                             m_pitchRecLuma, diff, coef, qcoef, qparam, bc, m_par->FastCoeffCost,
                                             m_rdLambda, miRow, miCol, miColEnd, m_par->miRows, m_par->miCols);

            rd.modeBits = intraModeBits[i];
            CostType cost = rd.sse + m_rdLambda * (rd.coefBits + rd.modeBits);

            fprintf_trace_cost(stderr, "intra luma: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d tx=%d "
                "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, absPartIdx, BLOCK_8X4, i, TX_4X4,
                rd.sse, rd.modeBits, rd.eob, rd.coefBits, cost);

            if (bestCost > cost) {
                bestCost = cost;
                bestRdCost = rd;
                bestMode = i;
                std::swap(qcoef, bestQcoef);
                if (bestMode != INTRA_MODES-1) { // best is not last checked
                    bestCtx = m_contexts;
                    CopyNxM(recSb + 4*y*m_pitchRecLuma, m_pitchRecLuma, bestRec, 8, 4);
                }
            }
        }

        if (bestMode != INTRA_MODES-1) { // best is not last checked
            m_contexts = bestCtx;
            CopyNxM(bestRec, 8, recSb + 4*y*m_pitchRecLuma, m_pitchRecLuma, 8, 4);
        }
        CopyCoeffs(bestQcoef, m_coeffWorkY+16*(absPartIdx+2*y), 32);

        Zero(m_data[absPartIdx+2*y]);
        m_data[absPartIdx+2*y].refIdx[0] = INTRA_FRAME;
        m_data[absPartIdx+2*y].refIdx[1] = NONE_FRAME;
        m_data[absPartIdx+2*y].refIdxComb = INTRA_FRAME;
        m_data[absPartIdx+2*y].sbType = BLOCK_8X4;
        m_data[absPartIdx+2*y].skip = (bestRdCost.eob==0);
        m_data[absPartIdx+2*y].txSize = TX_4X4;
        m_data[absPartIdx+2*y].mode = bestMode;
        m_data[absPartIdx+2*y].memCtx.skip = ctxSkip;
        m_data[absPartIdx+2*y].memCtx.isInter = ctxIsInter;
        m_data[absPartIdx+2*y+1] = m_data[absPartIdx+2*y];

        totalCostY.sse += bestRdCost.sse;
        totalCostY.modeBits += bestRdCost.modeBits;
        totalCostY.coefBits += bestRdCost.coefBits;

        aboveMode = bestMode;
    }

    if (!m_data[absPartIdx].skip || !m_data[absPartIdx+2].skip) {
        m_data[absPartIdx].skip = 0;
        m_data[absPartIdx+1].skip = 0;
        m_data[absPartIdx+2].skip = 0;
        m_data[absPartIdx+3].skip = 0;
    }

    RdCost costUV = {};
    if (m_par->chromaRDO)
        costUV = CheckIntraChroma(absPartIdx, 3, PARTITION_NONE);

    int32_t sse = totalCostY.sse + costUV.sse;
    uint32_t bits = totalCostY.modeBits + costUV.modeBits + skipBits[m_data[absPartIdx].skip] + isInterBits;
    if (!m_data[absPartIdx].skip)
        bits += totalCostY.coefBits + costUV.coefBits;
    m_costCurr += sse + m_rdLambda * bits;
}

template <typename PixType>
void AV1CU<PixType>::CheckIntra4x8(int32_t absPartIdx)
{
    const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
    const int32_t x4 = (rasterIdx & 15);
    const int32_t y4 = (rasterIdx >> 4);
    const int32_t miRow = (m_ctbPelY >> 3) + ((av1_scan_z2r4[absPartIdx] >> 4) >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + ((av1_scan_z2r4[absPartIdx] & 15) >> 1);
    const int32_t haveTop = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft  = (miCol > m_tileBorders.colStart);
    const int32_t notOnRight = 0;
    const PixType halfRange = 1<<(m_par->bitDepthLuma-1);
    const PixType halfRangeM1 = halfRange-1;
    const PixType halfRangeP1 = halfRange+1;
    const QuantParam &qparam = m_aqparamY;
    const int32_t miColEnd = m_tileBorders.colEnd;

    PixType *recSb = m_yRec + ((x4 + y4 * m_pitchRecLuma) << LOG2_MIN_TU_SIZE);
    PixType *srcSb = m_ySrc + ((x4 + y4 * SRC_PITCH) << LOG2_MIN_TU_SIZE);
    int16_t *diff = vp9scratchpad.diffY;
    int16_t *coef = vp9scratchpad.coefY;
    int16_t *qcoef = vp9scratchpad.qcoefY[0];
    int16_t *bestQcoef = vp9scratchpad.qcoefY[1];

    alignas(64) PixType bestRec[8*4];
    Contexts bestCtx;
    Contexts origCtx;

    const BitCounts &bc = m_currFrame->bitCount;
    const CoefBitCounts &cbc = bc.coef[PLANE_TYPE_Y][0][TX_4X4];
    ModeInfo *mi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const int32_t ctxSkip = GetCtxSkip(above, left);
    const uint16_t *skipBits = bc.skip[ctxSkip];
    const int32_t aboveMode[2] = { above ? above[0].mode : DC_PRED, above ? above[1].mode : DC_PRED };
    const int32_t ctxIsInter = m_currFrame->IsIntra() ? 0 : GetCtxIsInter(above, left);
    const uint32_t isInterBits = m_currFrame->IsIntra() ? 0 : bc.isInter[ctxIsInter][0];

    RdCost totalCostY = {0,0,0};

    int32_t leftMode = { left ? left[0].mode : DC_PRED };
    for (int32_t x = 0; x < 2; x++) {
        PixType *rec = recSb + 4*x;
        PixType *src = srcSb + 4*x;
        origCtx = m_contexts;

        const uint16_t *intraModeBits = m_currFrame->IsIntra() ? bc.kfIntraMode[ aboveMode[x] ][leftMode] : bc.intraMode[TX_4X4];

        CostType bestCost = COST_MAX;
        RdCost bestRdCost = {};
        int32_t bestMode = INTRA_MODES;
        uint8_t *anz = m_contexts.aboveNonzero[0]+x4+x;
        uint8_t *lnz = m_contexts.leftNonzero[0]+y4;

        for (int32_t i = DC_PRED; i < INTRA_MODES; i++) {
            if (x == 0 && (i == D45_PRED || i == D63_PRED)) {
                // modes D45 and D63 require above-right pixels
                // which are not available when picking mode for first 4x8
                continue;
            }
            m_contexts = origCtx;

            RdCost rd = TransformIntraYSbVp9(BLOCK_4X8, i, haveTop, haveLeft|x, 0, TX_4X4, anz, lnz, src, rec,
                                             m_pitchRecLuma, diff, coef, qcoef, qparam, bc, m_par->FastCoeffCost,
                                             m_rdLambda, miRow, miCol, miColEnd, m_par->miRows, m_par->miCols);
            rd.modeBits = intraModeBits[i];
            CostType cost = rd.sse + m_rdLambda * (rd.coefBits + rd.modeBits);

            fprintf_trace_cost(stderr, "intra luma: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d tx=%d "
                "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, absPartIdx, BLOCK_4X8, i, TX_4X4,
                rd.sse, rd.modeBits, rd.eob, rd.coefBits, cost);

            if (bestCost > cost) {
                bestCost = cost;
                bestRdCost = rd;
                bestMode = i;
                std::swap(qcoef, bestQcoef);
                if (bestMode != INTRA_MODES-1) { // best is not last checked
                    bestCtx = m_contexts;
                    CopyNxM(recSb + 4*x, m_pitchRecLuma, bestRec, 4, 8);
                }
            }
        }

        if (bestMode != INTRA_MODES-1) { // best is not last checked
            m_contexts = bestCtx;
            CopyNxM(bestRec, 4, recSb + 4*x, m_pitchRecLuma, 4, 8);
        }
        CopyCoeffs(bestQcoef,    m_coeffWorkY+16*(absPartIdx+x),   16);
        CopyCoeffs(bestQcoef+32, m_coeffWorkY+16*(absPartIdx+x+2), 16);

        Zero(m_data[absPartIdx+x]);
        m_data[absPartIdx+x].refIdx[0] = INTRA_FRAME;
        m_data[absPartIdx+x].refIdx[1] = NONE_FRAME;
        m_data[absPartIdx+x].refIdxComb = INTRA_FRAME;
        m_data[absPartIdx+x].sbType = BLOCK_4X8;
        m_data[absPartIdx+x].skip = (bestRdCost.eob==0);
        m_data[absPartIdx+x].txSize = TX_4X4;
        m_data[absPartIdx+x].mode = bestMode;
        m_data[absPartIdx+x].memCtx.skip = ctxSkip;
        m_data[absPartIdx+x].memCtx.isInter = ctxIsInter;
        m_data[absPartIdx+x+2] = m_data[absPartIdx+x];

        totalCostY.sse += bestRdCost.sse;
        totalCostY.modeBits += bestRdCost.modeBits;
        totalCostY.coefBits += bestRdCost.coefBits;

        leftMode = bestMode;
    }

    if (!m_data[absPartIdx].skip || !m_data[absPartIdx+1].skip) {
        m_data[absPartIdx].skip = 0;
        m_data[absPartIdx+1].skip = 0;
        m_data[absPartIdx+2].skip = 0;
        m_data[absPartIdx+3].skip = 0;
    }

    RdCost costUV = {};
    if (m_par->chromaRDO)
        costUV = CheckIntraChroma(absPartIdx, 3, PARTITION_NONE);

    int32_t sse = totalCostY.sse + costUV.sse;
    uint32_t bits = totalCostY.modeBits + costUV.modeBits + skipBits[m_data[absPartIdx].skip] + isInterBits;
    if (!m_data[absPartIdx].skip)
        bits += totalCostY.coefBits + costUV.coefBits;
    m_costCurr += sse + m_rdLambda * bits;
}

template <typename PixType>
void AV1CU<PixType>::CheckIntra4x4(int32_t absPartIdx)
{
    const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
    const int32_t x4 = (rasterIdx & 15);
    const int32_t y4 = (rasterIdx >> 4);
    const int32_t miRow = (m_ctbPelY >> 3) + ((av1_scan_z2r4[absPartIdx] >> 4) >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + ((av1_scan_z2r4[absPartIdx] & 15) >> 1);
    const int32_t haveTop = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft  = (miCol > m_tileBorders.colStart);
    const int32_t notOnRight = 0;
    const PixType halfRange = 1<<(m_par->bitDepthLuma-1);
    const PixType halfRangeM1 = halfRange-1;
    const PixType halfRangeP1 = halfRange+1;
    const QuantParam &qparam = m_aqparamY;
    const int32_t miColEnd = m_tileBorders.colEnd;

    PixType *recSb = m_yRec + ((x4 + y4 * m_pitchRecLuma) << LOG2_MIN_TU_SIZE);
    PixType *srcSb = m_ySrc + ((x4 + y4 * SRC_PITCH) << LOG2_MIN_TU_SIZE);
    int16_t *diff = vp9scratchpad.diffY;
    int16_t *coef = vp9scratchpad.coefY;
    int16_t *qcoef = vp9scratchpad.qcoefY[0];
    int16_t *bestQcoef = vp9scratchpad.qcoefY[1];

    alignas(64) PixType bestRec[4*4];
    Contexts bestCtx;
    Contexts origCtx;

    const BitCounts &bc = m_currFrame->bitCount;
    const CoefBitCounts &cbc = bc.coef[PLANE_TYPE_Y][0][TX_4X4];
    ModeInfo *mi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const int32_t ctxSkip = GetCtxSkip(above, left);
    const uint16_t *skipBits = bc.skip[ctxSkip ];
    const int32_t ctxIsInter = m_currFrame->IsIntra() ? 0 : GetCtxIsInter(above, left);
    const uint32_t isInterBits = m_currFrame->IsIntra() ? 0 : bc.isInter[ctxIsInter][0];

    int32_t aboveMode[2] = { DC_PRED, DC_PRED };
    if (above) {
        aboveMode[0] = above[0].mode;
        aboveMode[1] = above[1].mode;
    }

    int32_t leftMode[2] = { DC_PRED, DC_PRED };
    if (left) {
        leftMode[0] = left[0].mode;
        leftMode[1] = left[2].mode;
    }

    RdCost totalCostY = {0,0,0};

    for (int32_t y = 0; y < 2; y++) {
        for (int32_t x = 0; x < 2; x++) {
            CoeffsType *resid = m_residualsY + ((absPartIdx+2*y+x)<<4);
            CoeffsType *coeff = m_coeffWorkY + ((absPartIdx+2*y+x)<<4);
            PixType *rec = recSb + 4*y*m_pitchRecLuma + 4*x;
            PixType *src = srcSb + 4*y*SRC_PITCH + 4*x;

            const uint16_t *intraModeBits = (m_currFrame->IsIntra()) ? bc.kfIntraMode[ aboveMode[x] ][ leftMode[y] ] : bc.intraMode[TX_4X4];

            origCtx = m_contexts;
            CostType bestCost = COST_MAX;
            RdCost bestRdCost = {};
            int32_t bestMode = INTRA_MODES;
            uint8_t *anz = m_contexts.aboveNonzero[0]+x4+x;
            uint8_t *lnz = m_contexts.leftNonzero[0]+y4+y;

            for (int32_t i = DC_PRED; i < INTRA_MODES; i++) {
                m_contexts = origCtx;

                RdCost rd = TransformIntraYSbVp9(BLOCK_4X4, i, haveTop|y, haveLeft|x, !x, TX_4X4, anz, lnz, src, rec,
                                                 m_pitchRecLuma, diff, coef, qcoef, qparam, bc, m_par->FastCoeffCost,
                                                 m_rdLambda, miRow, miCol, miColEnd, m_par->miRows, m_par->miCols);

                rd.modeBits = intraModeBits[i];
                CostType cost = rd.sse + m_rdLambda * (rd.coefBits + rd.modeBits);

                fprintf_trace_cost(stderr, "intra luma: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d tx=%d "
                    "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, absPartIdx, BLOCK_4X4, i, TX_4X4,
                    rd.sse, rd.modeBits, rd.eob, rd.coefBits, cost);

                if (bestCost > cost) {
                    bestCost = cost;
                    bestRdCost = rd;
                    bestMode = i;
                    std::swap(qcoef, bestQcoef);
                    if (bestMode != INTRA_MODES-1) { // best is not last checked
                        bestCtx = m_contexts;
                        CopyNxN(recSb + 4*y*m_pitchRecLuma + 4*x, m_pitchRecLuma, bestRec, 4);
                    }
                }
            }

            if (bestMode != INTRA_MODES-1) { // best is not last checked
                m_contexts = bestCtx;
                CopyNxN(bestRec, 4, recSb + 4*y*m_pitchRecLuma + 4*x, m_pitchRecLuma, 4);
            }
            CopyCoeffs(bestQcoef, m_coeffWorkY+16*(absPartIdx+2*y+x), 16);

            Zero(m_data[absPartIdx+2*y+x]);
            m_data[absPartIdx+2*y+x].refIdx[0] = INTRA_FRAME;
            m_data[absPartIdx+2*y+x].refIdx[1] = NONE_FRAME;
            m_data[absPartIdx+2*y+x].refIdxComb = INTRA_FRAME;
            m_data[absPartIdx+2*y+x].sbType = BLOCK_4X4;
            m_data[absPartIdx+2*y+x].skip = (bestRdCost.eob==0);
            m_data[absPartIdx+2*y+x].txSize = TX_4X4;
            m_data[absPartIdx+2*y+x].mode = bestMode;
            m_data[absPartIdx+2*y+x].memCtx.skip = ctxSkip;
            m_data[absPartIdx+2*y+x].memCtx.isInter = ctxIsInter;

            totalCostY.sse += bestRdCost.sse;
            totalCostY.modeBits += bestRdCost.modeBits;
            totalCostY.coefBits += bestRdCost.coefBits;

            aboveMode[x] = bestMode;
            leftMode[y] = bestMode;
        }
    }

    if (!m_data[absPartIdx].skip || !m_data[absPartIdx+1].skip || !m_data[absPartIdx+2].skip || !m_data[absPartIdx+3].skip) {
        m_data[absPartIdx].skip = 0;
        m_data[absPartIdx+1].skip = 0;
        m_data[absPartIdx+2].skip = 0;
        m_data[absPartIdx+3].skip = 0;
    }

    RdCost costUV = {};
    if (m_par->chromaRDO)
        costUV = CheckIntraChroma(absPartIdx, 3, PARTITION_NONE);

    int32_t sse = totalCostY.sse + costUV.sse;
    uint32_t bits = totalCostY.modeBits + costUV.modeBits + skipBits[m_data[absPartIdx].skip] + isInterBits;
    if (!m_data[absPartIdx].skip)
        bits += totalCostY.coefBits + costUV.coefBits;
    m_costCurr += sse + m_rdLambda * bits;
}

namespace {
    int32_t DiffDc(const uint8_t *src, int32_t pitchSrc, const uint8_t *pred, int32_t pitchPred, int32_t width) {
        assert(width == 8 || width == 16 || width == 32);
        if (width == 8) {
            __m128i zero = _mm_setzero_si128();
            __m128i dc = zero;
            for (int32_t y = 0; y < 8; y++, src+=pitchSrc, pred+=pitchPred) {
                __m128i s = _mm_set1_epi64x(*(uint64_t*)(src));
                __m128i p = _mm_set1_epi64x(*(uint64_t*)(pred));
                s = _mm_unpacklo_epi8(s, zero);
                p = _mm_unpacklo_epi8(p, zero);
                dc = _mm_add_epi16(dc, _mm_sub_epi16(s, p));
            }
            dc = _mm_hadd_epi16(dc, dc);
            dc = _mm_hadd_epi16(dc, dc);
            dc = _mm_hadd_epi16(dc, dc);
            return (int16_t)_mm_extract_epi16(dc, 0);
        }
        else if (width == 16) {
            __m128i zero = _mm_setzero_si128();
            __m128i dc = zero;
            for (int32_t y = 0; y < 16; y++, src+=pitchSrc, pred+=pitchPred) {
                __m128i s = _mm_load_si128((__m128i*)(src));
                __m128i p = _mm_load_si128((__m128i*)(pred));
                __m128i s0 = _mm_unpacklo_epi8(s, zero);
                __m128i s1 = _mm_unpackhi_epi8(s, zero);
                __m128i p0 = _mm_unpacklo_epi8(p, zero);
                __m128i p1 = _mm_unpackhi_epi8(p, zero);
                dc = _mm_add_epi16(dc, _mm_sub_epi16(s0, p0));
                dc = _mm_add_epi16(dc, _mm_sub_epi16(s1, p1));
            }
            dc = _mm_hadd_epi16(dc, dc);
            dc = _mm_hadd_epi16(dc, dc);
            dc = _mm_hadd_epi16(dc, dc);
            return (int16_t)_mm_extract_epi16(dc, 0);
        }
        else {
            __m128i zero = _mm_setzero_si128();
            __m128i dc = zero;
            for (int32_t y = 0; y < 32; y++, src+=pitchSrc, pred+=pitchPred) {
                __m128i s = _mm_load_si128((__m128i*)(src));
                __m128i p = _mm_load_si128((__m128i*)(pred));
                __m128i s0 = _mm_unpacklo_epi8(s, zero);
                __m128i s1 = _mm_unpackhi_epi8(s, zero);
                __m128i p0 = _mm_unpacklo_epi8(p, zero);
                __m128i p1 = _mm_unpackhi_epi8(p, zero);
                dc = _mm_add_epi16(dc, _mm_sub_epi16(s0, p0));
                dc = _mm_add_epi16(dc, _mm_sub_epi16(s1, p1));
                s = _mm_load_si128((__m128i*)(src+16));
                p = _mm_load_si128((__m128i*)(pred+16));
                s0 = _mm_unpacklo_epi8(s, zero);
                s1 = _mm_unpackhi_epi8(s, zero);
                p0 = _mm_unpacklo_epi8(p, zero);
                p1 = _mm_unpackhi_epi8(p, zero);
                dc = _mm_add_epi16(dc, _mm_sub_epi16(s0, p0));
                dc = _mm_add_epi16(dc, _mm_sub_epi16(s1, p1));
            }
            dc = _mm_hadd_epi16(dc, dc);
            dc = _mm_hadd_epi16(dc, dc);
            dc = _mm_hadd_epi16(dc, dc);
            return (int16_t)_mm_extract_epi16(dc, 0);
        }
    }
}


template <typename PixType>
bool AV1CU<PixType>::tryIntraRD(int32_t absPartIdx, int32_t depth, IntraLumaMode *modes)
{
    if ( !m_bIntraCandInBuf ) return true;

    int32_t widthCu = m_par->MaxCUSize >> depth;
    int32_t InterHad = 0;
    int32_t IntraHad = 0;
    int32_t numCand1 = m_par->num_cand_1[m_par->Log2MaxCUSize - depth];

    int32_t offsetLuma = GetLumaOffset(m_par, absPartIdx, SRC_PITCH);
    PixType *pSrc = m_ySrc + offsetLuma;
    int32_t offsetPred = GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
    const PixType *predY = m_interPredY + offsetPred;

    InterHad = AV1PP::satd(pSrc, predY, BSR(widthCu) - 2, BSR(widthCu) - 2) >> m_par->bitDepthLumaShift;
    InterHad+= ((CostType)m_mvdCost*m_rdLambda*512.f + 0.5f);

    bool skipIntraRD = true;
    for(int32_t ic=0; ic<numCand1; ic++) {
        int32_t mode = modes[ic].mode;
        PixType *pred = m_predIntraAll + mode * widthCu * widthCu;
        float dc=0;
        if (mode < 2 || mode >= 18 && mode == 10)
            dc = AV1PP::diff_dc(pSrc, SRC_PITCH, pred, widthCu, widthCu);
        else
            dc = AV1PP::diff_dc(m_srcTr, widthCu, pred, widthCu, widthCu);
        dc *= h265_reci_1to116[(widthCu>>2)-1] * h265_reci_1to116[(widthCu>>2)-1] * (1.0f / 16);
        dc=fabsf(dc);
        IntraHad = modes[ic].satd;
        IntraHad -= dc*16*(widthCu/8)*(widthCu/8);
        IntraHad += (ic*m_rdLambda*512.f + 0.5f);

        if(IntraHad<InterHad || dc<1.0) {
            skipIntraRD = false;
            break;
        }
    }
    return !skipIntraRD;
}


template <typename PixType>
int32_t AV1CU<PixType>::GetNumIntraRDModes(int32_t absPartIdx, int32_t depth, int32_t trDepth, IntraLumaMode *modes, int32_t numModes)
{
    int32_t numCand = numModes;
    int32_t widthPu = m_par->MaxCUSize >> (depth+trDepth);
    int32_t NumModesForFullRD = numModes;
    if (widthPu > 8)  {
        if(m_SCid[depth][absPartIdx]>=5) NumModesForFullRD = 2;
    } else if (widthPu == 8)  {
        if(m_SCid[depth][absPartIdx]>=5) NumModesForFullRD = 4;
    } else {
                      NumModesForFullRD = 6;
    }

    if(numModes>1 && m_currFrame->m_picCodeType != MFX_FRAMETYPE_I && m_STC[depth][absPartIdx]<3 )
    {
        //Restrict number of candidates for 4x4 and 8x8 blocks
        if (widthPu==4 || widthPu==8)
        {
            CostType rdScale=1.0;
            const int32_t T4_1=400, T8_1=960;
            if(m_SCid[depth][absPartIdx]>=5) {
            //Restrict number of candidates based on Rd cost
            if      ((modes[0].cost+modes[0].cost) < (modes[1].cost*rdScale))                        NumModesForFullRD = 1;
            else if (numModes>2 && (modes[0].cost+modes[1].cost) < (modes[2].cost*rdScale))          NumModesForFullRD = 2;
            else if (numModes>3 && (modes[1].cost+modes[2].cost) < (modes[3].cost*rdScale))          NumModesForFullRD = 3;
            } else {
            //Restrict number of candidates based on maximal SATD
            if ((widthPu==4 && m_IntraCandMaxSatd < T4_1) || (widthPu==8 && m_IntraCandMaxSatd < T8_1))    NumModesForFullRD = 1;
            }
        }

        numCand = MIN(numModes, NumModesForFullRD);
    }
    return numCand;
}

const uint8_t mode_to_angle_map[] = {0, 90, 180, 45, 135, 113, 157, 203, 67, 0,
                                  0, 0, 0};

template <typename PixType>
RdCost AV1CU<PixType>::CheckIntraLumaNonRdVp9(int32_t absPartIdx, int32_t depth, PartitionType partition)
{
    assert(depth > 0);
    assert(partition == PARTITION_NONE);

    const BlockSize bsz = GetSbType(depth, partition);
    const int32_t width = MAX_CU_SIZE >> depth;
    const int32_t size = (MAX_NUM_PARTITIONS << 4) >> (depth << 1);
    const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
    const int32_t x4 = (rasterIdx & 15);
    const int32_t y4 = (rasterIdx >> 4);
    const int32_t notOnRight = 0;
    const PixType halfRange = 1<<(m_par->bitDepthLuma-1);
    const PixType halfRangeM1 = halfRange-1;
    const PixType halfRangeP1 = halfRange+1;
    const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);
    const int32_t haveTop = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft  = (miCol > m_tileBorders.colStart);
    const int32_t miColEnd = m_tileBorders.colEnd;
    const int32_t mi_rows = m_tileBorders.rowEnd;
    PixType *recSb = m_yRec + ((x4 + y4 * m_pitchRecLuma) << LOG2_MIN_TU_SIZE);
    PixType *srcSb = m_ySrc + ((x4 + y4 * SRC_PITCH) << LOG2_MIN_TU_SIZE);

    const int32_t sbx = x4 << 2;
    const int32_t sby = y4 << 2;

    const TxSize maxTxSize = max_txsize_lookup[bsz];
    const TxSize txSize = maxTxSize;
    ModeInfo *mi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const int32_t aboveMode = (above ? above->mode : DC_PRED);
    const int32_t leftMode  = (left  ? left->mode  : DC_PRED);
    const BitCounts &bc = m_currFrame->bitCount;
    const CoefBitCounts &cbc = bc.coef[PLANE_TYPE_Y][0][txSize];
    const int32_t ctxSkip = GetCtxSkip(above, left);
    const uint16_t *skipBits = bc.skip[ctxSkip];
    const uint16_t *intraModeBits = (m_currFrame->IsIntra() ? bc.kfIntraMode[aboveMode][leftMode] : bc.intraMode[maxTxSize]);
    const uint16_t *txSizeBits = bc.txSize[maxTxSize][ GetCtxTxSizeVP9(above, left, maxTxSize) ];
    const int32_t ctxIsInter = m_currFrame->IsIntra() ? 0 : GetCtxIsInter(above, left);
    const uint32_t isInterBits = m_currFrame->IsIntra() ? 0 : bc.isInter[ctxIsInter][0];

    alignas(32) PixType predPels_[32/sizeof(PixType) + (4<<TX_32X32) * 3 + (4<<TX_32X32)];
    PixType *predPels = predPels_ + 32/sizeof(PixType) - 1;

    int32_t bestMode = DC_PRED;
    int32_t bestDelta = 0;
    AV1PP::GetPredPelsLuma(recSb, m_pitchRecLuma, predPels, width, halfRangeM1, halfRangeP1, haveTop, haveLeft, notOnRight);
    AV1PP::predict_intra_vp9(predPels, m_predIntraAll + 0 * size, width, maxTxSize, haveLeft, haveTop, DC_PRED);
    AV1PP::predict_intra_vp9(predPels, m_predIntraAll + 1 * size, width, maxTxSize, 0, 0, V_PRED);
    AV1PP::predict_intra_vp9(predPels, m_predIntraAll + 2 * size, width, maxTxSize, 0, 0, H_PRED);
    AV1PP::predict_intra_vp9(predPels, m_predIntraAll + 3 * size, width, maxTxSize, 0, 0, D45_PRED);
    AV1PP::predict_intra_vp9(predPels, m_predIntraAll + 4 * size, width, maxTxSize, 0, 0, D135_PRED);
    AV1PP::predict_intra_vp9(predPels, m_predIntraAll + 5 * size, width, maxTxSize, 0, 0, D117_PRED);
    AV1PP::predict_intra_vp9(predPels, m_predIntraAll + 6 * size, width, maxTxSize, 0, 0, D153_PRED);
    AV1PP::predict_intra_vp9(predPels, m_predIntraAll + 7 * size, width, maxTxSize, 0, 0, D207_PRED);
    AV1PP::predict_intra_vp9(predPels, m_predIntraAll + 8 * size, width, maxTxSize, 0, 0, D63_PRED);
    AV1PP::predict_intra_vp9(predPels, m_predIntraAll + 9 * size, width, maxTxSize, 0, 0, TM_PRED);

    int32_t bestCost = AV1PP::satd(srcSb, m_predIntraAll, width, txSize, txSize);
    bestCost += int32_t(m_rdLambdaSqrt * intraModeBits[DC_PRED] + 0.5f);
    for (int32_t mode = DC_PRED + 1; mode < INTRA_MODES; mode++) {
        int32_t cost = AV1PP::satd(srcSb, m_predIntraAll + mode * size, width, txSize, txSize);
        cost += int32_t(m_rdLambdaSqrt * intraModeBits[mode] + 0.5f);
        if (bestCost > cost) {
            bestCost = cost;
            bestMode = mode;
        }
    }

    const QuantParam &qparam = m_aqparamY;
    uint8_t *anz = m_contexts.aboveNonzero[0]+x4;
    uint8_t *lnz = m_contexts.leftNonzero[0]+y4;
    int16_t *diff = vp9scratchpad.diffY;
    int16_t *coef = vp9scratchpad.coefY;
    int16_t *qcoef = m_coeffWorkY + 16 * absPartIdx;


    RdCost rd = TransformIntraYSbVp9(bsz, bestMode, haveTop, haveLeft, 0, maxTxSize, anz, lnz, srcSb, recSb, m_pitchRecLuma,
                                     diff, coef, qcoef, qparam, bc, m_par->FastCoeffCost, m_rdLambda, miRow, miCol, miColEnd,
                                     m_par->miRows, m_par->miCols);

    Zero(*mi);
    mi->refIdx[0] = INTRA_FRAME;
    mi->refIdx[1] = NONE_FRAME;
    mi->refIdxComb = INTRA_FRAME;
    mi->sbType = bsz;
    mi->skip = (rd.eob == 0);
    mi->txSize = txSize;
    mi->mode = bestMode;
    mi->memCtx.skip = ctxSkip;
    mi->memCtx.isInter = ctxIsInter;
    const int32_t num4x4w = block_size_wide_4x4[bsz];
    const int32_t num4x4h = block_size_high_4x4[bsz];
    PropagateSubPart(mi, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);

    rd.modeBits = isInterBits + txSizeBits[txSize] + intraModeBits[bestMode];
    return rd;
}


template <typename PixType>
RdCost AV1CU<PixType>::CheckIntraLumaNonRdAv1(int32_t absPartIdx, int32_t depth, PartitionType partition)
{
    assert(depth > 0);
    assert(partition == PARTITION_NONE);

    const BlockSize bsz = GetSbType(depth, partition);
    const int32_t width = MAX_CU_SIZE >> depth;
    const int32_t size = (MAX_NUM_PARTITIONS << 4) >> (depth << 1);
    const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
    const int32_t x4 = (rasterIdx & 15);
    const int32_t y4 = (rasterIdx >> 4);
    const int32_t notOnRight = 0;
    const PixType halfRange = 1<<(m_par->bitDepthLuma-1);
    const PixType halfRangeM1 = halfRange-1;
    const PixType halfRangeP1 = halfRange+1;
    const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);
    const int32_t haveTop = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft  = (miCol > m_tileBorders.colStart);
    const int32_t miColEnd = m_tileBorders.colEnd;
    const int32_t mi_rows = m_tileBorders.rowEnd;
    PixType *recSb = m_yRec + ((x4 + y4 * m_pitchRecLuma) << LOG2_MIN_TU_SIZE);
    PixType *srcSb = m_ySrc + ((x4 + y4 * SRC_PITCH) << LOG2_MIN_TU_SIZE);

    const int32_t sbx = x4 << 2;
    const int32_t sby = y4 << 2;

    const TxSize maxTxSize = max_txsize_lookup[bsz];
    const TxSize txSize = maxTxSize;
    ModeInfo *mi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const int32_t aboveMode = (above ? above->mode : DC_PRED);
    const int32_t leftMode  = (left  ? left->mode  : DC_PRED);
    const uint8_t aboveTxfm = above ? tx_size_wide[above->txSize] : 0;
    const uint8_t leftTxfm =  left  ? tx_size_high[left->txSize] : 0;
    const BitCounts &bc = m_currFrame->bitCount;
    const CoefBitCounts &cbc = bc.coef[PLANE_TYPE_Y][0][txSize];
    const int32_t ctxSkip = GetCtxSkip(above, left);
    const uint16_t *skipBits = bc.skip[ctxSkip];
    const uint16_t *intraModeBits = m_currFrame->IsIntra()
        ? bc.kfIntraModeAV1[ intra_mode_context[aboveMode] ][ intra_mode_context[leftMode] ]
        : bc.intraModeAV1[maxTxSize];
    const uint16_t *txSizeBits = bc.txSize[maxTxSize][ GetCtxTxSizeAV1(above, left, aboveTxfm, leftTxfm, maxTxSize) ];
    const int32_t ctxIsInter = m_currFrame->IsIntra() ? 0 : GetCtxIsInter(above, left);
    const uint32_t isInterBits = m_currFrame->IsIntra() ? 0 : bc.isInter[ctxIsInter][0];

    int32_t bestMode = DC_PRED;
    int32_t bestDelta = 0;
    const int32_t x = 0, y = 0;
    const int32_t txw = tx_size_wide_unit[txSize];
    assert(txw == (width >> 2));
    const int32_t haveRight = (miCol + ((x + txw) >> 1)) < miColEnd;
    const int32_t haveTopRight = HaveTopRight(bsz, miRow, miCol, haveTop, haveRight, txSize, y, x, 0);
    const int32_t txwpx = 4 << txSize;
    const int32_t txhpx = 4 << txSize;
    const int32_t bh = block_size_high[bsz] >> 3;
    const int32_t mb_to_bottom_edge = ((mi_rows - bh - miRow) * 8) * 8;
    const int32_t hpx = width;
    const int32_t yd = (mb_to_bottom_edge >> 3) + (hpx - y - txhpx);
    const int32_t haveBottom = yd > 0;
    const int32_t haveBottomLeft = HaveBottomLeft(bsz, miRow, miCol, haveLeft, haveBottom, txSize, y, x, 0);
    const int32_t bw = block_size_wide[bsz] >> 3;
    const int32_t mb_to_right_edge = ((m_par->miCols - bw - miCol) * 8) * 8;
    const int32_t wpx = hpx;
    // Distance between the right edge of this prediction block to the frame right edge
    const int32_t xr = (mb_to_right_edge >> 3) + (wpx - (x<<2) - txwpx);
    const int32_t pixTop = haveTop ? std::min(txwpx, xr + txwpx) : 0;
    const int32_t pixLeft = haveLeft ? std::min(txhpx, yd + txhpx) : 0;
    const int32_t pixTopRight = haveTopRight ? std::min(txwpx, xr) : 0;
    const int32_t pixBottomLeft = haveBottomLeft ? std::min(txhpx, yd) : 0;

    AV1PP::IntraPredPels<PixType> predPels;

    AV1PP::GetPredPelsAV1<PLANE_TYPE_Y>(recSb, m_pitchRecLuma, predPels.top, predPels.left, width, haveTop, haveLeft, pixTopRight, pixBottomLeft);

    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 0 * size, width, maxTxSize, haveLeft, haveTop, DC_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 1 * size, width, maxTxSize, 0, 0, V_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 2 * size, width, maxTxSize, 0, 0, H_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 3 * size, width, maxTxSize, 0, 0, D45_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 4 * size, width, maxTxSize, 0, 0, D135_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 5 * size, width, maxTxSize, 0, 0, D117_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 6 * size, width, maxTxSize, 0, 0, D153_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 7 * size, width, maxTxSize, 0, 0, D207_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 8 * size, width, maxTxSize, 0, 0, D63_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 9 * size, width, maxTxSize, 0, 0, SMOOTH_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll +10 * size, width, maxTxSize, 0, 0, SMOOTH_V_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll +11 * size, width, maxTxSize, 0, 0, SMOOTH_H_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll +12 * size, width, maxTxSize, 0, 0, PAETH_PRED, 0, 0, 0);

    int32_t bestCost = AV1PP::satd(srcSb, m_predIntraAll, width, txSize, txSize);
    bestCost += int32_t(m_rdLambdaSqrt * intraModeBits[DC_PRED] + 0.5f);
    for (int32_t mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
        //if (mode == SMOOTH_H_PRED || mode == SMOOTH_V_PRED)
        //    continue;
        int32_t cost = AV1PP::satd(srcSb, m_predIntraAll + mode * size, width, txSize, txSize);
        int32_t bitCost = intraModeBits[mode];

        if (av1_is_directional_mode(mode)) {
            const int max_angle_delta = MAX_ANGLE_DELTA;
            const int delta = 0;
            bitCost += write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
        }

        cost += int32_t(m_rdLambdaSqrt * bitCost + 0.5f);
        if (bestCost > cost) {
            bestCost = cost;
            bestMode = mode;
        }
    }

    // ext_modes
    //-------------------------------------------------
    if (bsz == BLOCK_8X8 || bsz == BLOCK_16X16 || bsz == BLOCK_32X32) {
        // buildSkipList()
        alignas(64) PixType   predIntra[32 * 32];

        // search
        //for (int32_t mode = V_PRED; mode <= D63_PRED; mode++)
        if (av1_is_directional_mode(bestMode))
        {
            int32_t mode = bestMode;
            //if (SkipList[mode]) continue;
            for (int32_t angleDelta = 1; angleDelta <= 3; angleDelta++) {
                for (int32_t i = 0; i < 2; i++) {
                    int32_t delta = (1 - 2 * i) * angleDelta;
                    AV1PP::predict_intra_av1(predPels.top, predPels.left, predIntra, width, maxTxSize, 0, 0, mode, delta, 0, 0);
                    int32_t cost = AV1PP::satd(srcSb, predIntra, width, txSize, txSize);
                    int32_t bitCost = intraModeBits[mode];

                    int32_t max_angle_delta = MAX_ANGLE_DELTA;
                    bitCost += write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);

                    cost += int32_t(m_rdLambdaSqrt * bitCost + 0.5f);// simple test
                    if (bestCost > cost) {
                        bestCost = cost;
                        bestMode = mode;
                        bestDelta = delta;
                    }
                }
            }
        }

        // recalc
        //if (bestDelta)
        //    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll, width, maxTxSize, 0, 0, bestMode, bestDelta, 0, 0);
    }

    uint8_t *anz = m_contexts.aboveNonzero[0]+x4;
    uint8_t *lnz = m_contexts.leftNonzero[0]+y4;
    int16_t *diff = vp9scratchpad.diffY;
    int16_t *coef = vp9scratchpad.coefY;
    int16_t *qcoef = m_coeffWorkY + 16 * absPartIdx;
    int16_t *coefWork = vp9scratchpad.coefWork;

    const int32_t filtType = get_filt_type(above, left, PLANE_TYPE_Y);

    RdCost rd = TransformIntraYSbAv1(bsz, bestMode, haveTop, haveLeft, maxTxSize, DCT_DCT, anz, lnz, srcSb,
                                     recSb, m_pitchRecLuma, diff, coef, qcoef, coefWork, m_lumaQp, bc, m_rdLambda, miRow,
                                     miCol, miColEnd, bestDelta, filtType, *m_par, m_aqparamY, NULL);

    Zero(*mi);
    mi->refIdx[0] = INTRA_FRAME;
    mi->refIdx[1] = NONE_FRAME;
    mi->refIdxComb = INTRA_FRAME;
    mi->sbType = bsz;
    mi->skip = (rd.eob == 0);
    mi->txSize = txSize;
    mi->mode = bestMode;
    mi->memCtx.skip = ctxSkip;
    mi->memCtx.isInter = ctxIsInter;
    mi->angle_delta_y = MAX_ANGLE_DELTA + bestDelta;
    const int32_t num4x4w = block_size_wide_4x4[bsz];
    const int32_t num4x4h = block_size_high_4x4[bsz];
    PropagateSubPart(mi, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);

    rd.modeBits = isInterBits + txSizeBits[txSize] + intraModeBits[bestMode];
    if (av1_is_directional_mode(bestMode)) {
        int32_t max_angle_delta = MAX_ANGLE_DELTA;
        int32_t delta = bestDelta;
        rd.modeBits += write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
    }
    return rd;
}


template <typename PixType>
RdCost AV1CU<PixType>::CheckIntraLuma(int32_t absPartIdx, int32_t depth, PartitionType partition)
{
    // for now: perform exhaustive search
    // keep satd and txSize=maxTxSize stages for debug and further tuning

    const BlockSize bsz = GetSbType(depth, partition);
    const TxSize maxTxSize = max_txsize_lookup[bsz];
    const TxSize minTxSize = std::max((int32_t)TX_4X4, maxTxSize - m_par->maxTxDepthIntra + 1);
    const int32_t num4x4w = block_size_wide_4x4[bsz];
    const int32_t num4x4h = block_size_high_4x4[bsz];
    const int32_t num4x4  = num_4x4_blocks_lookup[bsz];
    const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
    const int32_t x4 = (rasterIdx & 15);
    const int32_t y4 = (rasterIdx >> 4);
    const int32_t notOnRight = 0;
    const PixType halfRange = 1<<(m_par->bitDepthLuma-1);
    const PixType halfRangeM1 = halfRange-1;
    const PixType halfRangeP1 = halfRange+1;
    const QuantParam &qparam = m_aqparamY;
    const BitCounts &bc = m_currFrame->bitCount;
    int32_t txType = DCT_DCT;

    PixType *recSb = m_yRec + ((x4 + y4 * m_pitchRecLuma) << LOG2_MIN_TU_SIZE);
    PixType *srcSb = m_ySrc + ((x4 + y4 * SRC_PITCH) << LOG2_MIN_TU_SIZE);
    int16_t *diff = vp9scratchpad.diffY;
    int16_t *coef = vp9scratchpad.coefY;
    int16_t *qcoef = vp9scratchpad.qcoefY[0];
    int16_t *bestQcoef = vp9scratchpad.qcoefY[1];

    alignas(64) PixType bestRec[64*64];
    Contexts origCtx = m_contexts;
    Contexts bestCtx;

    const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);
    const int32_t haveTop = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft  = (miCol > m_tileBorders.colStart);
    const int32_t miColEnd = m_tileBorders.colEnd;
    ModeInfo *mi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const int32_t ctxSkip = GetCtxSkip(above, left);
    const int32_t ctxTxSize = GetCtxTxSizeVP9(above, left, maxTxSize);
    const uint16_t *skipBits = bc.skip[ctxSkip];
    const uint16_t *txSizeBits = bc.txSize[maxTxSize][ctxTxSize];
    const int32_t aboveMode = (above ? above->mode : DC_PRED);
    const int32_t leftMode  = (left  ? left->mode  : DC_PRED);
    const uint16_t *intraModeBits = m_currFrame->IsIntra() ? bc.kfIntraMode[aboveMode][leftMode] : bc.intraMode[maxTxSize];
    const int32_t ctxIsInter = m_currFrame->IsIntra() ? 0 : GetCtxIsInter(above, left);
    const uint32_t isInterBits = m_currFrame->IsIntra() ? 0 : bc.isInter[ctxIsInter][0];

    CostType bestCost = COST_MAX;
    RdCost bestRdCost = {INT_MAX, UINT_MAX, UINT_MAX};
    int32_t bestMode = INTRA_MODES;
    int32_t bestTxSize = maxTxSize;
    uint8_t *anz = m_contexts.aboveNonzero[0]+x4;
    uint8_t *lnz = m_contexts.leftNonzero[0]+y4;

    for (int32_t i = DC_PRED; i < INTRA_MODES; i++) {
        for (int32_t txSize = maxTxSize; txSize >= minTxSize; txSize--) {
            m_contexts = origCtx;
            RdCost rd = TransformIntraYSbVp9(bsz, i, haveTop, haveLeft, 0, txSize, anz, lnz, srcSb, recSb, m_pitchRecLuma,
                                             diff, coef, qcoef, qparam, bc, m_par->FastCoeffCost, m_rdLambda, miRow, miCol,
                                             miColEnd, m_par->miRows, m_par->miCols);

            rd.modeBits = isInterBits + txSizeBits[txSize] + intraModeBits[i];
            CostType cost = (rd.eob == 0) // if luma has no coeffs, assume skip
                ? rd.sse + m_rdLambda * (rd.modeBits + skipBits[1])
                : rd.sse + m_rdLambda * (rd.modeBits + skipBits[0] + rd.coefBits);

            fprintf_trace_cost(stderr, "intra luma: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d tx=%d "
                "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, absPartIdx, bsz, i, txSize,
                rd.sse, rd.modeBits, rd.eob, rd.coefBits, cost);

            if (bestCost > cost) {
                bestCost = cost;
                bestRdCost = rd;
                bestMode = i;
                bestTxSize = txSize;
                std::swap(qcoef, bestQcoef);
                if (!(bestMode == INTRA_MODES-1 && bestTxSize==TX_4X4)) { // best is not last checked
                    bestCtx = m_contexts;
                    CopyNxM(recSb, m_pitchRecLuma, bestRec, num4x4w<<2, num4x4h<<2);
                }
            }
        }
    }

    if (!(bestMode == INTRA_MODES-1 && bestTxSize==TX_4X4)) { // best is not last checked
        m_contexts = bestCtx;
        CopyNxM(bestRec, num4x4w<<2, recSb, m_pitchRecLuma, num4x4w<<2, num4x4h<<2);
    }
    if (partition == PARTITION_VERT) {
        int32_t half = num4x4<<3;
        CopyCoeffs(bestQcoef, m_coeffWorkY+16*absPartIdx, half);
        CopyCoeffs(bestQcoef+2*half, m_coeffWorkY+16*absPartIdx+2*half, half);
    } else
        CopyCoeffs(bestQcoef, m_coeffWorkY+16*absPartIdx, num4x4<<4);

    Zero(*mi);
    mi->refIdx[0] = INTRA_FRAME;
    mi->refIdx[1] = NONE_FRAME;
    mi->refIdxComb = INTRA_FRAME;
    mi->sbType = bsz;
    mi->skip = bestRdCost.eob == 0;
    mi->txSize = bestTxSize;
    mi->mode = bestMode;
    mi->memCtx.skip = ctxSkip;
    mi->memCtx.isInter = ctxIsInter;
    mi->memCtx.txSize = ctxTxSize;
    PropagateSubPart(mi, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);

    return bestRdCost;
}


template <typename PixType>
RdCost AV1CU<PixType>::CheckIntraChromaNonRdVp9(int32_t absPartIdx, int32_t depth, PartitionType partition)
{
    typedef typename ChromaUVPixType<PixType>::type ChromaPixType;

    const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
    const int32_t x4Luma = (rasterIdx & 15);
    const int32_t y4Luma = (rasterIdx >> 4);
    ModeInfo *mi = m_data + (x4Luma >> 1) + (y4Luma >> 1) * m_par->miPitch;
    const BlockSize sbType = mi->sbType;
    const BlockSize bsz = ss_size_lookup[sbType][m_par->subsamplingX][m_par->subsamplingX];
    const int32_t widthSb = (MAX_CU_SIZE >> depth) >> 1;
    const int32_t size = ((MAX_NUM_PARTITIONS << 4) >> (depth << 1)) >> 2;
    const int32_t num4x4w = block_size_wide_4x4[bsz];
    const int32_t num4x4h = block_size_high_4x4[bsz];
    const int32_t num4x4 = num_4x4_blocks_lookup[bsz];
    const TxSize txSize = GetUvTxSize(sbType, mi->txSize, *m_par);
    const int32_t x4 = (rasterIdx & 15) >> m_par->subsamplingX;
    const int32_t y4 = (rasterIdx >> 4) >> m_par->subsamplingY;
    const int32_t sseShift = m_par->bitDepthChromaShift<<1;

    PixType *recSb = m_uvRec + ((x4*2 + y4*m_pitchRecChroma) << LOG2_MIN_TU_SIZE);
    PixType *srcSb = m_uvSrc + ((x4*2 + y4*SRC_PITCH) << LOG2_MIN_TU_SIZE);

    alignas(32) ChromaPixType predPels_[32/sizeof(ChromaPixType) + (4<<TX_32X32) * 3 + (4<<TX_32X32)];
    PixType *predPels = (PixType*)(predPels_ + 32/sizeof(ChromaPixType) - 1);
    PixType halfRange = 1<<(m_par->bitDepthChroma-1);
    int32_t shift = 8*sizeof(PixType);
    ChromaPixType halfRangeM1 = (halfRange-1) + ((halfRange-1)<<shift);
    ChromaPixType halfRangeP1 = (halfRange+1) + ((halfRange+1)<<shift);

    const BitCounts &bc = m_currFrame->bitCount;
    const CoefBitCounts &cbc = bc.coef[PLANE_TYPE_UV][0][txSize];
    const int32_t miRow = (m_ctbPelY >> 3) + y4;//(y4 >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + x4;//(x4 >> 1);
    const int32_t haveTop_ = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft_  = (miCol > m_tileBorders.colStart);
    const int32_t miColEnd = m_tileBorders.colEnd;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const uint16_t *skipBits = bc.skip[ GetCtxSkip(above, left) ];
    const uint16_t *intraModeUvBits = bc.intraModeUvAV1[mi->mode];
    const int32_t log2w = 3 - depth;

    const int32_t txw = tx_size_wide_unit[txSize];
    const int32_t txwpx = 4 << txSize;
    const int32_t txhpx = 4 << txSize;

    const BlockSize bszLuma = GetSbType(depth, partition);
    const int32_t bh = block_size_high_8x8[bszLuma];
    const int32_t bw = block_size_wide_8x8[bszLuma];
    const int32_t distToBottomEdge = (m_par->miRows - bh - miRow) * 8;
    const int32_t distToRightEdge = (m_par->miCols - bw - miCol) * 8;

    const int32_t hpx = block_size_high[bsz];
    const int32_t wpx = hpx;

    int32_t bestMode = DC_PRED;
    int32_t bestDelta = 0;
    bestMode = AV1PP::pick_intra_nv12(recSb, m_pitchRecLuma, srcSb, m_rdLambda, intraModeUvBits, log2w, haveLeft_, haveTop_);

    int32_t width = 4<<txSize;
    int32_t step = 1<<txSize;
    int32_t sse = 0;
    int16_t totalEob = 0;
    const int16_t *scan = vp9_default_scan[txSize];
    uint32_t coefBits = 0;
    int32_t haveTop = haveTop_;
    for (int32_t y = 0; y < num4x4h; y += step) {
        PixType *rec = recSb + (y<<2) * m_pitchRecLuma;
        PixType *src = srcSb + (y<<2) * SRC_PITCH;
        int32_t haveLeft = haveLeft_;
        for (int32_t x = 0; x < num4x4w; x += step, rec += width<<1, src += width<<1) {
            assert(rasterIdx + (y<<1) * 16 + (x<<1) < 256);
            int32_t blockIdx = h265_scan_r2z4[rasterIdx + (y<<1) * 16 + (x<<1)];
            int32_t offset = blockIdx<<(LOG2_MIN_TU_SIZE<<1);
            offset >>= 2; // TODO: 4:2:0 only
            CoeffsType *residU = m_residualsU + offset;
            CoeffsType *residV = m_residualsV + offset;
            CoeffsType *coeffU = m_coeffWorkU + offset;
            CoeffsType *coeffV = m_coeffWorkV + offset;
            int32_t notOnRight = x + step < num4x4w;
            AV1PP::GetPredPelsLuma((ChromaPixType*)rec, m_pitchRecChroma>>1, (ChromaPixType*)predPels, width, halfRangeM1, halfRangeP1, haveTop_, haveLeft, notOnRight);
            AV1PP::predict_intra_nv12_vp9(predPels, rec, m_pitchRecChroma, txSize, haveLeft, haveTop, bestMode);
            int32_t sseZero = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);
            AV1PP::diff_nv12(src, SRC_PITCH, rec, m_pitchRecChroma, residU, residV, width, width, txSize);

            int32_t txType = GetTxTypeAV1(PLANE_TYPE_UV, txSize, /*mi->modeUV*/bestMode, 0/*fake*/);

            AV1PP::ftransform_vp9(residU, residU, width, txSize, /*DCT_DCT*/txType);
            AV1PP::ftransform_vp9(residV, residV, width, txSize, /*DCT_DCT*/txType);
            int32_t eobU = AV1PP::quant_dequant(residU, coeffU, m_aqparamUv[0], txSize);
            int32_t eobV = AV1PP::quant_dequant(residV, coeffV, m_aqparamUv[1], txSize);

            int32_t dcCtxU = GetDcCtx(m_contexts.aboveNonzero[1]+x4+x, m_contexts.leftNonzero[1]+y4+y, step);
            int32_t dcCtxV = GetDcCtx(m_contexts.aboveNonzero[2]+x4+x, m_contexts.leftNonzero[2]+y4+y, step);

            uint32_t bitsZeroU = cbc.moreCoef[0][dcCtxU][0];
            uint32_t bitsZeroV = cbc.moreCoef[0][dcCtxV][0];
            uint32_t bitsZero = bitsZeroU + bitsZeroV;

            uint32_t txBits = 0;
            if (eobU) {
                AV1PP::itransform_vp9(residU, residU, width, txSize, /*DCT_DCT*/txType);
                txBits += EstimateCoefs(m_par->FastCoeffCost, cbc, coeffU, scan, eobU, dcCtxU, /*DCT_DCT*/txType);
                small_memset1(m_contexts.aboveNonzero[1]+x4+x, step);
                small_memset1(m_contexts.leftNonzero[1]+y4+y, step);
            } else {
                txBits += bitsZeroU;
                small_memset0(m_contexts.aboveNonzero[1]+x4+x, step);
                small_memset0(m_contexts.leftNonzero[1]+y4+y, step);
            }
            if (eobV) {
                AV1PP::itransform_vp9(residV, residV, width, txSize, /*DCT_DCT*/txType);
                txBits += EstimateCoefs(m_par->FastCoeffCost, cbc, coeffV, scan, eobV, dcCtxV, /*DCT_DCT*/txType);
                small_memset1(m_contexts.aboveNonzero[2]+x4+x, step);
                small_memset1(m_contexts.leftNonzero[2]+y4+y, step);
            } else {
                txBits += bitsZeroV;
                small_memset0(m_contexts.aboveNonzero[2]+x4+x, step);
                small_memset0(m_contexts.leftNonzero[2]+y4+y, step);
            }

            if (eobU | eobV) {
                CoeffsType *residU_ = (eobU ? residU : coeffU); // if cbf==0 coeffWork contains zeroes
                CoeffsType *residV_ = (eobV ? residV : coeffV); // if cbf==0 coeffWork contains zeroes
                AV1PP::adds_nv12(rec, m_pitchRecChroma, rec, m_pitchRecChroma, residU_, residV_, width, width);
            }
            int32_t txSse = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);

            if ((eobU || eobV) && sseZero + m_rdLambda * bitsZero < txSse + m_rdLambda * txBits) {
                eobU = 0;
                eobV = 0;
                ZeroCoeffs(coeffU, 16 << (txSize << 1)); // zero coeffs
                ZeroCoeffs(coeffV, 16 << (txSize << 1)); // zero coeffs

                // re-predict intra
                AV1PP::predict_intra_nv12_vp9(predPels, rec, m_pitchRecChroma, txSize, haveLeft, haveTop, bestMode);
                txSse = sseZero;
                txBits = bitsZero;
                small_memset0(m_contexts.aboveNonzero[1]+x4+x, step);
                small_memset0(m_contexts.aboveNonzero[2]+x4+x, step);
                small_memset0(m_contexts.leftNonzero[1]+y4+y, step);
                small_memset0(m_contexts.leftNonzero[2]+y4+y, step);
            }

            totalEob += eobU;
            totalEob += eobV;
            coefBits += txBits;
            sse += txSse;
            haveLeft = 1;
        }
        haveTop = 1;
    }

    uint32_t modeBits = intraModeUvBits[bestMode];

    CostType cost = (mi->skip && totalEob==0)
        ? sse + m_rdLambda * (modeBits + skipBits[1])
        : sse + m_rdLambda * (modeBits + skipBits[0] + coefBits);

    fprintf_trace_cost(stderr, "intra chroma: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d "
        "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, absPartIdx, bsz, bestMode,
        sse, modeBits, totalEob, coefBits, cost);

    int32_t skip = mi->skip && totalEob==0;

    mi->modeUV = bestMode;
    mi->skip = skip;
    const int32_t num4x4wLuma = block_size_wide_4x4[mi->sbType];
    const int32_t num4x4hLuma = block_size_high_4x4[mi->sbType];
    PropagateSubPart(mi, m_par->miPitch, num4x4wLuma >> 1, num4x4hLuma >> 1);

    RdCost rd;
    rd.sse = sse;
    rd.eob = totalEob;
    rd.modeBits = modeBits;
    rd.coefBits = coefBits;
    return rd;
}


template <typename PixType>
RdCost AV1CU<PixType>::CheckIntraChromaNonRdAv1(int32_t absPartIdx, int32_t depth, PartitionType partition)
{
    const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
    const int32_t x4Luma = (rasterIdx & 15);
    const int32_t y4Luma = (rasterIdx >> 4);
    ModeInfo *mi = m_data + (x4Luma >> 1) + (y4Luma >> 1) * m_par->miPitch;
    const BlockSize sbType = mi->sbType;
    const BlockSize bsz = ss_size_lookup[sbType][m_par->subsamplingX][m_par->subsamplingX];
    const int32_t widthSb = (MAX_CU_SIZE >> depth) >> 1;
    const int32_t size = ((MAX_NUM_PARTITIONS << 4) >> (depth << 1)) >> 2;
    const int32_t num4x4w = block_size_wide_4x4[bsz];
    const int32_t num4x4h = block_size_high_4x4[bsz];
    const int32_t num4x4 = num_4x4_blocks_lookup[bsz];
    const TxSize txSize = std::min((int32_t)TX_32X32, 3 - depth);
    const int32_t x4 = (rasterIdx & 15) >> m_par->subsamplingX;
    const int32_t y4 = (rasterIdx >> 4) >> m_par->subsamplingY;
    const int32_t sseShift = m_par->bitDepthChromaShift<<1;

    PixType *recSb = m_uvRec + ((x4*2 + y4*m_pitchRecChroma) << LOG2_MIN_TU_SIZE);
    PixType *srcSb = m_uvSrc + ((x4*2 + y4*SRC_PITCH) << LOG2_MIN_TU_SIZE);

    typedef typename ChromaUVPixType<PixType>::type ChromaPixType;
    AV1PP::IntraPredPels<ChromaPixType> predPels;
    PixType *pt = (PixType *)predPels.top;
    PixType *pl = (PixType *)predPels.left;

    const BitCounts &bc = m_currFrame->bitCount;
    const TxbBitCounts &cbc = bc.txb[ get_q_ctx(m_lumaQp) ][txSize][PLANE_TYPE_UV];
    const int32_t miRow = (m_ctbPelY >> 3) + y4;//(y4 >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + x4;//(x4 >> 1);
    const int32_t haveTop_ = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft_  = (miCol > m_tileBorders.colStart);
    const int32_t miColEnd = m_tileBorders.colEnd;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const uint16_t *skipBits = bc.skip[ GetCtxSkip(above, left) ];
    const uint16_t *intraModeUvBits = bc.intraModeUvAV1[mi->mode];
    const int32_t log2w = 3 - depth;

    const int32_t txw = tx_size_wide_unit[txSize];
    const int32_t txwpx = 4 << txSize;
    const int32_t txhpx = 4 << txSize;

    const BlockSize bszLuma = GetSbType(depth, partition);
    const int32_t bh = block_size_high_8x8[bszLuma];
    const int32_t bw = block_size_wide_8x8[bszLuma];
    const int32_t distToBottomEdge = (m_par->miRows - bh - miRow) * 8;
    const int32_t distToRightEdge = (m_par->miCols - bw - miCol) * 8;

    const int32_t hpx = block_size_high[bsz];
    const int32_t wpx = hpx;

    int32_t bestMode = DC_PRED;
    int32_t bestDelta = 0;

    const int32_t x = 0;
    const int32_t y = 0;
    const TxSize txSizeMax = max_txsize_lookup[bsz];
    const int32_t txwMax = tx_size_wide_unit[txSizeMax];
    const int32_t txwpxMax = 4 << txSizeMax;
    const int32_t txhpxMax = txwpxMax;
    const int32_t haveRight = (miCol + ((x + txwMax) /*>> 1*/)) < miColEnd;
    const int32_t haveTopRight = HaveTopRight(bszLuma, miRow, miCol, haveTop_, haveRight, txSizeMax, y, x, 1);
    // Distance between the right edge of this prediction block to the frame right edge
    const int32_t xr = (distToRightEdge >> 1) + (wpx - (x<<2) - txwpxMax);
    // Distance between the bottom edge of this prediction block to the frame bottom edge
    const int32_t yd = (distToBottomEdge >> 1) + (hpx - (y<<2) - txhpxMax);
    const int32_t haveBottom = yd > 0;
    const int32_t haveBottomLeft = HaveBottomLeft(bszLuma, miRow, miCol, haveLeft_, haveBottom, txSizeMax, y, x, 1);
    const int32_t pixTop = haveTop_ ? std::min(txwpxMax, xr + txwpxMax) : 0;
    const int32_t pixTopRight = haveTopRight ? std::min(txwpxMax, xr) : 0;
    const int32_t pixLeft = haveLeft_ ? std::min(txhpxMax, yd + txhpxMax) : 0;
    const int32_t pixBottomLeft = haveBottomLeft ? std::min(txhpxMax, yd) : 0;

    AV1PP::GetPredPelsAV1<PLANE_TYPE_UV>((ChromaPixType *)recSb, m_pitchRecChroma>>1, predPels.top, predPels.left, widthSb,
                                         haveTop_, haveLeft_, pixTopRight, pixBottomLeft);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  0 * size * 2, widthSb * 2, log2w, haveLeft_, haveTop_, DC_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  1 * size * 2, widthSb * 2, log2w, 0, 0, V_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  2 * size * 2, widthSb * 2, log2w, 0, 0, H_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  3 * size * 2, widthSb * 2, log2w, 0, 0, D45_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  4 * size * 2, widthSb * 2, log2w, 0, 0, D135_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  5 * size * 2, widthSb * 2, log2w, 0, 0, D117_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  6 * size * 2, widthSb * 2, log2w, 0, 0, D153_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  7 * size * 2, widthSb * 2, log2w, 0, 0, D207_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  8 * size * 2, widthSb * 2, log2w, 0, 0, D63_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  9 * size * 2, widthSb * 2, log2w, 0, 0, SMOOTH_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll + 10 * size * 2, widthSb * 2, log2w, 0, 0, SMOOTH_V_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll + 11 * size * 2, widthSb * 2, log2w, 0, 0, SMOOTH_H_PRED, 0, 0, 0);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll + 12 * size * 2, widthSb * 2, log2w, 0, 0, PAETH_PRED, 0, 0, 0);

    int32_t bestSse = AV1PP::sse_p64_pw(srcSb, m_predIntraAll, log2w + 1, log2w);
    CostType bestCost = bestSse + m_rdLambda * intraModeUvBits[DC_PRED];
    for (int32_t mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
        //if (mode == SMOOTH_H_PRED || mode == SMOOTH_V_PRED)
        //    continue;
        int32_t sse = AV1PP::sse_p64_pw(srcSb, m_predIntraAll + mode * size * 2, log2w + 1, log2w);
        int32_t bitCost = intraModeUvBits[mode];

        if (av1_is_directional_mode(mode)) {
            int32_t max_angle_delta = MAX_ANGLE_DELTA;
            int32_t delta = 0;
            bitCost += write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
        }

        CostType cost = sse + m_rdLambda * bitCost;
        if (bestCost > cost) {
            bestCost = cost;
            bestSse = sse;
            bestMode = mode;
            bestDelta = 0;
        }
    }

    // second pass
    if (av1_is_directional_mode(bestMode)) {
        int32_t mode = bestMode;
        for (int32_t angleDelta = 1; angleDelta <= 3; angleDelta++) {
            for (int32_t i = 0; i < 2; i++) {
                const int32_t delta = (1 - 2 * i) * angleDelta;
                AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll, widthSb * 2, log2w, 0, 0, mode, delta, 0, 0);
                int32_t sse = AV1PP::sse_p64_pw(srcSb, m_predIntraAll, log2w + 1, log2w);
                int32_t bitCost = intraModeUvBits[mode];

                if (av1_is_directional_mode(mode))
                    bitCost += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1, MAX_ANGLE_DELTA + delta);

                CostType cost = sse + m_rdLambda * bitCost;
                if (bestCost > cost) {
                    bestCost = cost;
                    bestMode = mode;
                    bestDelta = delta;
                }
            }
        }
    }

    // recalc
    if (bestDelta)
        AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll, widthSb * 2, log2w, 0, 0, bestMode, bestDelta, 0, 0);

    mi->angle_delta_uv = bestDelta + MAX_ANGLE_DELTA;

    const int32_t isDirectionalMode = av1_is_directional_mode(bestMode);
    const int32_t angle = mode_to_angle_map[bestMode] + (mi->angle_delta_uv - MAX_ANGLE_DELTA) * ANGLE_STEP;

    int32_t width = 4<<txSize;
    int32_t step = 1<<txSize;
    int32_t sse = 0;
    int16_t totalEob = 0;
    const int16_t *scan = av1_default_scan[txSize];
    uint32_t coefBits = 0;
    int32_t haveTop = haveTop_;
    int16_t *coefOriginU = vp9scratchpad.coefWork;
    int16_t *coefOriginV = vp9scratchpad.coefWork + 32 * 32;

    for (int32_t y = 0; y < num4x4h; y += step) {
        PixType *rec = recSb + (y<<2) * m_pitchRecLuma;
        PixType *src = srcSb + (y<<2) * SRC_PITCH;
        int32_t haveLeft = haveLeft_;
        for (int32_t x = 0; x < num4x4w; x += step, rec += width<<1, src += width<<1) {
            assert(rasterIdx + (y<<1) * 16 + (x<<1) < 256);
            int32_t blockIdx = h265_scan_r2z4[rasterIdx + (y<<1) * 16 + (x<<1)];
            int32_t offset = blockIdx<<(LOG2_MIN_TU_SIZE<<1);
            offset >>= 2; // TODO: 4:2:0 only
            CoeffsType *residU = m_residualsU + offset;
            CoeffsType *residV = m_residualsV + offset;
            CoeffsType *coeffU = m_coeffWorkU + offset;
            CoeffsType *coeffV = m_coeffWorkV + offset;

            int32_t notOnRight = x + step < num4x4w;

            const BlockSize bszLuma = GetSbType(depth, partition);
            const int32_t haveRight = (miCol + ((x + txw)/* >> 1*/)) < miColEnd;
            const int32_t haveTopRight = HaveTopRight(bszLuma, miRow, miCol, haveTop, haveRight, txSize, y, x, 1);
            // Distance between the right edge of this prediction block to the frame right edge
            const int32_t xr = (distToRightEdge >> 1) + (wpx - (x<<2) - txwpx);
            // Distance between the bottom edge of this prediction block to the frame bottom edge
            const int32_t yd = (distToBottomEdge >> 1) + (hpx - (y<<2) - txhpx);
            const int32_t haveBottom = yd > 0;
            const int32_t haveBottomLeft = HaveBottomLeft(bszLuma, miRow, miCol, haveLeft, haveBottom, txSize, y, x, 1);
            const int32_t pixTop = haveTop ? std::min(txwpx, xr + txwpx) : 0;
            const int32_t pixLeft = haveLeft ? std::min(txhpx, yd + txhpx) : 0;
            const int32_t pixTopRight = haveTopRight ? std::min(txwpx, xr) : 0;
            const int32_t pixBottomLeft = haveBottomLeft ? std::min(txhpx, yd) : 0;
            AV1PP::GetPredPelsAV1<PLANE_TYPE_UV>((ChromaPixType*)rec, m_pitchRecChroma>>1, predPels.top, predPels.left, width, haveTop, haveLeft, pixTopRight, pixBottomLeft);

            int32_t upTop = 0;
            int32_t upLeft = 0;
            if (m_par->enableIntraEdgeFilter && isDirectionalMode) {
                const int32_t need_top = angle < 180;
                const int32_t need_left = angle > 90;
                const int32_t need_right = angle < 90;
                const int32_t need_bottom = angle > 180;
                const int32_t filtType = get_filt_type(above, left, PLANE_TYPE_UV);
                if (angle != 90 && angle != 180) {
                    if (need_top && need_left && (txwpx + txhpx >= 24))
                        av1_filter_intra_edge_corner_nv12(pt, pl);

                    if (haveTop) {
                        const int32_t strength = intra_edge_filter_strength(txwpx, txhpx, angle - 90, filtType);
                        const int32_t sz = 1 + pixTop + (need_right ? txhpx : 0);
                        av1_filter_intra_edge_nv12_c(pt - 2, sz, strength);
                    }
                    if (haveLeft) {
                        const int32_t strength = intra_edge_filter_strength(txwpx, txhpx, angle - 180, filtType);
                        const int32_t sz = 1 + pixLeft + (need_bottom ? txwpx : 0);
                        av1_filter_intra_edge_nv12_c(pl - 2, sz, strength);
                    }
                }
                upTop = use_intra_edge_upsample(txwpx, txhpx, angle - 90, filtType);
                if (need_top && upTop) {
                    const int32_t sz = txwpx + (need_right ? txhpx : 0);
                    av1_upsample_intra_edge_nv12_c(pt, sz);
                }
                upLeft = use_intra_edge_upsample(txwpx, txhpx, angle - 180, filtType);
                if (need_left && upLeft) {
                    const int32_t sz = txhpx + (need_bottom ? txwpx : 0);
                    av1_upsample_intra_edge_nv12_c(pl, sz);
                }
            }

            const int32_t delta = mi->angle_delta_uv - MAX_ANGLE_DELTA;
            AV1PP::predict_intra_nv12_av1(pt, pl, rec, m_pitchRecChroma, txSize, haveLeft, haveTop, bestMode, delta, upTop, upLeft);

            int32_t sseZero = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);
            AV1PP::diff_nv12(src, SRC_PITCH, rec, m_pitchRecChroma, residU, residV, width, width, txSize);

            int32_t txType = GetTxTypeAV1(PLANE_TYPE_UV, txSize, /*mi->modeUV*/bestMode, 0/*fake*/);

#ifdef ADAPTIVE_DEADZONE
            AV1PP::ftransform_av1(residU, coefOriginU, width, txSize, /*DCT_DCT*/txType);
            AV1PP::ftransform_av1(residV, coefOriginV, width, txSize, /*DCT_DCT*/txType);
            int32_t eobU = AV1PP::quant(coefOriginU, coeffU, m_aqparamUv[0], txSize);
            int32_t eobV = AV1PP::quant(coefOriginV, coeffV, m_aqparamUv[1], txSize);
            if(eobU) AV1PP::dequant(coeffU, residU, m_aqparamUv[0], txSize);
            if(eobV) AV1PP::dequant(coeffV, residV, m_aqparamUv[1], txSize);
            if (eobU) adaptDz(coefOriginU, residU, reinterpret_cast<const int16_t *>(&m_aqparamUv[0]), txSize, &m_roundFAdjUv[0][0], eobU);
            if (eobV) adaptDz(coefOriginV, residV, reinterpret_cast<const int16_t *>(&m_aqparamUv[1]), txSize, &m_roundFAdjUv[1][0], eobV);
#else
            AV1PP::ftransform_av1(residU, residU, width, txSize, /*DCT_DCT*/txType);
            AV1PP::ftransform_av1(residV, residV, width, txSize, /*DCT_DCT*/txType);
            int32_t eobU = AV1PP::quant_dequant(residU, coeffU, m_aqparamUv[0], txSize);
            int32_t eobV = AV1PP::quant_dequant(residV, coeffV, m_aqparamUv[1], txSize);
#endif

            uint8_t *actxU = m_contexts.aboveNonzero[1]+x4+x;
            uint8_t *actxV = m_contexts.aboveNonzero[2]+x4+x;
            uint8_t *lctxU = m_contexts.leftNonzero[1]+y4+y;
            uint8_t *lctxV = m_contexts.leftNonzero[2]+y4+y;
            const int32_t txbSkipCtxU = GetTxbSkipCtx(bsz, txSize, 1, actxU, lctxU);
            const int32_t txbSkipCtxV = GetTxbSkipCtx(bsz, txSize, 1, actxV, lctxV);

            uint32_t txBits = 0;
            if (eobU) {
                AV1PP::itransform_av1(residU, residU, width, txSize, txType);
                int32_t culLevel;
                txBits += EstimateCoefsAv1(cbc, txbSkipCtxU, eobU, coeffU, &culLevel);
                SetCulLevel(actxU, lctxU, culLevel, txSize);
            } else {
                txBits += cbc.txbSkip[txbSkipCtxU][1];
                SetCulLevel(actxU, lctxU, 0, txSize);
            }
            if (eobV) {
                AV1PP::itransform_av1(residV, residV, width, txSize, txType);
                int32_t culLevel;
                txBits += EstimateCoefsAv1(cbc, txbSkipCtxV, eobV, coeffV, &culLevel);
                SetCulLevel(actxV, lctxV, culLevel, txSize);
            } else {
                txBits += cbc.txbSkip[txbSkipCtxV][1];
                SetCulLevel(actxV, lctxV, 0, txSize);
            }

            if (eobU | eobV) {
                CoeffsType *residU_ = (eobU ? residU : coeffU); // if cbf==0 coeffWork contains zeroes
                CoeffsType *residV_ = (eobV ? residV : coeffV); // if cbf==0 coeffWork contains zeroes
                AV1PP::adds_nv12(rec, m_pitchRecChroma, rec, m_pitchRecChroma, residU_, residV_, width, width);
            }
            int32_t txSse = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);

            totalEob += eobU;
            totalEob += eobV;
            coefBits += txBits;
            sse += txSse;
            haveLeft = 1;
        }
        haveTop = 1;
    }

    uint32_t modeBits = intraModeUvBits[bestMode];

    if (av1_is_directional_mode(bestMode)) {
        int32_t delta = mi->angle_delta_uv - MAX_ANGLE_DELTA;
        int32_t max_angle_delta = MAX_ANGLE_DELTA;
        modeBits += write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
    }

    CostType cost = (mi->skip && totalEob==0)
        ? sse + m_rdLambda * (modeBits + skipBits[1])
        : sse + m_rdLambda * (modeBits + skipBits[0] + coefBits);

    fprintf_trace_cost(stderr, "intra chroma: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d "
        "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, absPartIdx, bsz, bestMode,
        sse, modeBits, totalEob, coefBits, cost);

    int32_t skip = mi->skip && totalEob==0;

    mi->modeUV = bestMode;
    mi->skip = skip;
    const int32_t num4x4wLuma = block_size_wide_4x4[mi->sbType];
    const int32_t num4x4hLuma = block_size_high_4x4[mi->sbType];
    PropagateSubPart(mi, m_par->miPitch, num4x4wLuma >> 1, num4x4hLuma >> 1);

    RdCost rd;
    rd.sse = sse;
    rd.eob = totalEob;
    rd.modeBits = modeBits;
    rd.coefBits = coefBits;
    return rd;
}


template <typename PixType>
RdCost AV1CU<PixType>::CheckIntraChroma(int32_t absPartIdx, int32_t depth, PartitionType partition)
{
    typedef typename ChromaUVPixType<PixType>::type ChromaPixType;

    const BlockSize sbType = m_data[absPartIdx].sbType;
    const BlockSize bsz = ss_size_lookup[MAX(BLOCK_8X8,sbType)][m_par->subsamplingX][m_par->subsamplingX];
    const int32_t num4x4w = block_size_wide_4x4[bsz];
    const int32_t num4x4h = block_size_high_4x4[bsz];
    const int32_t num4x4 = num_4x4_blocks_lookup[bsz];
    const TxSize txSize = GetUvTxSize(sbType, m_data[absPartIdx].txSize, *m_par);
    const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
    const int32_t x4 = (rasterIdx & 15) >> m_par->subsamplingX;
    const int32_t y4 = (rasterIdx >> 4) >> m_par->subsamplingY;

    PixType *recSb = m_uvRec + ((x4*2 + y4*m_pitchRecChroma) << LOG2_MIN_TU_SIZE);
    PixType *srcSb = m_uvSrc + ((x4*2 + y4*SRC_PITCH) << LOG2_MIN_TU_SIZE);

    alignas(32) ChromaPixType predPels_[32/sizeof(ChromaPixType) + (4<<TX_32X32) * 3];
    PixType *predPels = (PixType*)(predPels_ + 32/sizeof(ChromaPixType) - 1);
    PixType halfRange = 1<<(m_par->bitDepthChroma-1);
    int32_t shift = 8*sizeof(PixType);
    ChromaPixType halfRangeM1 = (halfRange-1) + ((halfRange-1)<<shift);
    ChromaPixType halfRangeP1 = (halfRange+1) + ((halfRange+1)<<shift);

    CostType bestCost = COST_MAX;
    RdCost bestRdCost = {INT_MAX, UINT_MAX, UINT_MAX};
    int32_t bestMode = INTRA_MODES;
    int32_t bestEob = 4096;

    const BitCounts &bc = m_currFrame->bitCount;
    const CoefBitCounts &cbc = bc.coef[PLANE_TYPE_UV][0][txSize];
    const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);
    const int32_t haveTop_ = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft_  = (miCol > m_tileBorders.colStart);
    ModeInfo *mi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const uint16_t *skipBits = bc.skip[ GetCtxSkip(above, left) ];
    const uint16_t *intraModeUvBits = bc.intraModeUv[m_data[absPartIdx|3].mode];

    alignas(64) PixType bestRec[64*32];
    alignas(64) CoeffsType bestCoefsU[64*32];
    alignas(64) CoeffsType bestCoefsV[64*32];
    Contexts origCtx = m_contexts;
    Contexts bestCtx;

    for (int32_t mode = DC_PRED; mode < INTRA_MODES; mode++) {
        if (mode == D45_PRED || mode == D207_PRED || mode == D63_PRED) continue;
        int32_t width = 4<<txSize;
        int32_t step = 1<<txSize;
        int32_t sse = 0;
        int16_t totalEob = 0;
        const int16_t *scan = vp9_default_scan[txSize];
        uint32_t coefBits = 0;
        m_contexts = origCtx;
        int32_t haveTop = haveTop_;
        for (int32_t y = 0; y < num4x4h; y += step) {
            PixType *rec = recSb + (y<<2) * m_pitchRecLuma;
            PixType *src = srcSb + (y<<2) * SRC_PITCH;
            int32_t haveLeft = haveLeft_;
            for (int32_t x = 0; x < num4x4w; x += step, rec += width<<1, src += width<<1) {
                assert(rasterIdx + (y<<1) * 16 + (x<<1) < 256);
                int32_t blockIdx = h265_scan_r2z4[rasterIdx + (y<<1) * 16 + (x<<1)];
                int32_t offset = blockIdx<<(LOG2_MIN_TU_SIZE<<1);
                offset >>= 2; // TODO: 4:2:0 only
                CoeffsType *residU = m_residualsU + offset;
                CoeffsType *residV = m_residualsV + offset;
                CoeffsType *coeffU = m_coeffWorkU + offset;
                CoeffsType *coeffV = m_coeffWorkV + offset;
                int32_t notOnRight = x + step < num4x4w;
                AV1PP::GetPredPelsLuma((ChromaPixType*)rec, m_pitchRecChroma>>1, (ChromaPixType*)predPels, width, halfRangeM1, halfRangeP1, haveTop, haveLeft, notOnRight);
                AV1PP::predict_intra_nv12_vp9(predPels, rec, m_pitchRecChroma, txSize, haveLeft, haveTop, mode);
                int32_t sseZero = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);
                AV1PP::diff_nv12(src, SRC_PITCH, rec, m_pitchRecChroma, residU, residV, width, width, txSize);
                AV1PP::ftransform_av1(residU, residU, width, txSize, DCT_DCT);
                AV1PP::ftransform_av1(residV, residV, width, txSize, DCT_DCT);
                int32_t eobU = AV1PP::quant_dequant(residU, coeffU, m_aqparamUv[0], txSize);
                int32_t eobV = AV1PP::quant_dequant(residV, coeffV, m_aqparamUv[1], txSize);

                int32_t dcCtxU = GetDcCtx(m_contexts.aboveNonzero[1]+x4+x, m_contexts.leftNonzero[1]+y4+y, step);
                int32_t dcCtxV = GetDcCtx(m_contexts.aboveNonzero[2]+x4+x, m_contexts.leftNonzero[2]+y4+y, step);

                uint32_t bitsZeroU = cbc.moreCoef[0][dcCtxU][0];
                uint32_t bitsZeroV = cbc.moreCoef[0][dcCtxV][0];
                uint32_t bitsZero = bitsZeroU + bitsZeroV;

                uint32_t txBits = 0;
                if (eobU) {
                    AV1PP::itransform_av1(residU, residU, width, txSize, DCT_DCT);
                    txBits += EstimateCoefs(m_par->FastCoeffCost, cbc, coeffU, scan, eobU, dcCtxU, DCT_DCT);
                    small_memset1(m_contexts.aboveNonzero[1]+x4+x, step);
                    small_memset1(m_contexts.leftNonzero[1]+y4+y, step);
                } else {
                    txBits += bitsZeroU;
                    small_memset0(m_contexts.aboveNonzero[1]+x4+x, step);
                    small_memset0(m_contexts.leftNonzero[1]+y4+y, step);
                }
                if (eobV) {
                    AV1PP::itransform_av1(residV, residV, width, txSize, DCT_DCT);
                    txBits += EstimateCoefs(m_par->FastCoeffCost, cbc, coeffV, scan, eobV, dcCtxV, DCT_DCT);
                    small_memset1(m_contexts.aboveNonzero[2]+x4+x, step);
                    small_memset1(m_contexts.leftNonzero[2]+y4+y, step);
                } else {
                    txBits += bitsZeroV;
                    small_memset0(m_contexts.aboveNonzero[2]+x4+x, step);
                    small_memset0(m_contexts.leftNonzero[2]+y4+y, step);
                }

                if (eobU | eobV) {
                    CoeffsType *residU_ = (eobU ? residU : coeffU); // if cbf==0 coeffWork contains zeroes
                    CoeffsType *residV_ = (eobV ? residV : coeffV); // if cbf==0 coeffWork contains zeroes
                    AV1PP::adds_nv12(rec, m_pitchRecChroma, rec, m_pitchRecChroma, residU_, residV_, width, width);
                }
                int32_t txSse = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);

                if ((eobU || eobV) && sseZero + m_rdLambda * bitsZero < txSse + m_rdLambda * txBits) {
                    eobU = 0;
                    eobV = 0;
                    ZeroCoeffs(coeffU, 16 << (txSize << 1)); // zero coeffs
                    ZeroCoeffs(coeffV, 16 << (txSize << 1)); // zero coeffs
                    AV1PP::predict_intra_nv12_vp9(predPels, rec, m_pitchRecChroma, txSize, haveLeft, haveTop, mode); // re-predict intra
                    txSse = sseZero;
                    txBits = bitsZero;
                    small_memset0(m_contexts.aboveNonzero[1]+x4+x, step);
                    small_memset0(m_contexts.aboveNonzero[2]+x4+x, step);
                    small_memset0(m_contexts.leftNonzero[1]+y4+y, step);
                    small_memset0(m_contexts.leftNonzero[2]+y4+y, step);
                }

                totalEob += eobU;
                totalEob += eobV;
                coefBits += txBits;
                sse += txSse;
                haveLeft = 1;
            }
            haveTop = 1;
        }

        uint32_t modeBits = intraModeUvBits[mode];
        CostType cost = (m_data[absPartIdx].skip && totalEob==0)
            ? sse + m_rdLambda * (modeBits + skipBits[1])
            : sse + m_rdLambda * (modeBits + skipBits[0] + coefBits);

        fprintf_trace_cost(stderr, "intra chroma: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d "
            "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, absPartIdx, bsz, mode,
            sse, modeBits, totalEob, coefBits, cost);

        if (bestCost > cost) {
            bestCost = cost;
            bestRdCost.sse = sse;
            bestRdCost.coefBits = coefBits;
            bestRdCost.modeBits = modeBits;
            bestMode = mode;
            bestEob = totalEob;
            if (!(bestMode == INTRA_MODES-1)) { // best is not last checked
                bestCtx = m_contexts;
                CopyNxM(recSb, m_pitchRecChroma, bestRec, num4x4w<<3, num4x4h<<2);
                if (partition == PARTITION_VERT) {
                    int32_t half = num4x4<<3;
                    CopyCoeffs(m_coeffWorkU+4*absPartIdx, bestCoefsU, half);
                    CopyCoeffs(m_coeffWorkV+4*absPartIdx, bestCoefsV, half);
                    CopyCoeffs(m_coeffWorkU+4*absPartIdx+2*half, bestCoefsU+half, half);
                    CopyCoeffs(m_coeffWorkV+4*absPartIdx+2*half, bestCoefsV+half, half);
                } else {
                    CopyCoeffs(m_coeffWorkU+4*absPartIdx, bestCoefsU, num4x4<<4);
                    CopyCoeffs(m_coeffWorkV+4*absPartIdx, bestCoefsV, num4x4<<4);
                }
            }
        }
    }

    if (!(bestMode == INTRA_MODES-1)) { // best is not last checked
        m_contexts = bestCtx;
        CopyNxM(bestRec, num4x4w<<3, recSb, m_pitchRecChroma, num4x4w<<3, num4x4h<<2);
        if (partition == PARTITION_VERT) {
            int32_t half = num4x4<<3;
            CopyCoeffs(bestCoefsU, m_coeffWorkU+4*absPartIdx, half);
            CopyCoeffs(bestCoefsV, m_coeffWorkV+4*absPartIdx, half);
            CopyCoeffs(bestCoefsU+half, m_coeffWorkU+4*absPartIdx+2*half, half);
            CopyCoeffs(bestCoefsV+half, m_coeffWorkV+4*absPartIdx+2*half, half);
        } else {
            CopyCoeffs(bestCoefsU, m_coeffWorkU+4*absPartIdx, num4x4<<4);
            CopyCoeffs(bestCoefsV, m_coeffWorkV+4*absPartIdx, num4x4<<4);
        }
    }

    int32_t skip = m_data[absPartIdx].skip && bestEob==0;

    m_data[absPartIdx].modeUV = bestMode;
    m_data[absPartIdx].skip = skip;

    if (partition == PARTITION_VERT) {
        int32_t half = num_4x4_blocks_lookup[MAX(BLOCK_8X8,sbType)]>>1;
        // cannot use PropagateSubPart because of 4x8, 8x4, 4x4 modes
        //m_data[absPartIdx+2*half] = m_data[absPartIdx];
        //PropagateSubPart(m_data+absPartIdx, half);
        //PropagateSubPart(m_data+absPartIdx+2*half, half);
        for (int32_t i = 0; i < half; i++) {
            m_data[absPartIdx+i].skip = skip;
            m_data[absPartIdx+i].modeUV = bestMode;
            m_data[absPartIdx+i+2*half].skip = skip;
            m_data[absPartIdx+i+2*half].modeUV = bestMode;
        }
    } else {
        int32_t num4x4 = num_4x4_blocks_lookup[MAX(BLOCK_8X8,sbType)];
        // cannot use PropagateSubPart because of 4x8, 8x4, 4x4 modes
        //PropagateSubPart(m_data+absPartIdx, num4x4);
        for (int32_t i = 0; i < num4x4; i++) {
            m_data[absPartIdx+i].skip = skip;
            m_data[absPartIdx+i].modeUV = bestMode;
        }
    }

    return bestRdCost;
}



template class AV1CU<uint8_t>;
template class AV1CU<uint16_t>;

} // namespace

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
