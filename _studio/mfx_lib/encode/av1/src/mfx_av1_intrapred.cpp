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

#define CHROMA_PALETTE 1

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
int32_t AV1CU<PixType>::CheckIntra(int32_t absPartIdx, int32_t depth, PartitionType partition, uint32_t &ret_bits)
{
    if (!m_par->intraRDO && (depth == 0 || partition != PARTITION_NONE)) {
        m_costCurr = COST_MAX; // skip 64x64 and non-square partitions
        return INT_MAX;
    }

    const int32_t sbx = (av1_scan_z2r4[absPartIdx] & 15) << 2;
    const int32_t sby = (av1_scan_z2r4[absPartIdx] >> 4) << 2;
    const int32_t miRow = (m_ctbPelY + sby) >> 3;
    const int32_t miCol = (m_ctbPelX + sbx) >> 3;
    const ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const uint16_t *skipBits = m_currFrame->bitCount->skip[ GetCtxSkip(above, left) ];

    RdCost costY = CheckIntraLumaNonRdAv1(absPartIdx, depth, partition);

    RdCost costUV = {};
    /*if (m_par->chromaRDO)
        costUV = CheckIntraChroma(absPartIdx, depth, partition);*/

    int32_t sse = costY.sse + costUV.sse;
    uint32_t bits = costY.modeBits + costUV.modeBits + skipBits[mi->skip];
    if (!mi->skip)
        bits += costY.coefBits + costUV.coefBits;
    m_costCurr += sse + m_rdLambda * bits;
    ret_bits = bits;
    return sse;
}

#define MAX_ANGLE_DELTA 3
#define ANGLE_STEP 3

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

static const uint16_t HAVE_TOP_RIGHT_BLOCK_8X8_TX_4X4[16] = { 65535, 21845, 30583, 21845, 32639, 21845, 30583, 21845, 32767, 21845, 30583, 21845, 32639, 21845, 30583, 21845, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_8X8_TX_8X8[8] = { 255, 85, 119, 85, 127, 85, 119, 85, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_16X16_TX_4X4[16] = { 65535, 30583, 30583, 30583, 32639, 30583, 30583, 30583, 32767, 30583, 30583, 30583, 32639, 30583, 30583, 30583, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_16X16_TX_8X8[8] = { 255, 85, 119, 85, 127, 85, 119, 85, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_16X16_TX_16X16[4] = { 15, 5, 7, 5, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_32X32_TX_4X4[16] = { 65535, 32639, 32639, 32639, 32639, 32639, 32639, 32639, 32767, 32639, 32639, 32639, 32639, 32639, 32639, 32639, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_32X32_TX_8X8[8] = { 255, 119, 119, 119, 127, 119, 119, 119, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_32X32_TX_16X16[4] = { 15, 5, 7, 5, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_32X32_TX_32X32[2] = { 3, 1, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_64X64_TX_4X4[16] = { 65535, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_64X64_TX_8X8[8] = { 255, 127, 127, 127, 127, 127, 127, 127, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_64X64_TX_16X16[4] = { 15, 7, 7, 7, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_64X64_TX_32X32[2] = { 3, 1, };
static const uint16_t HAVE_TOP_RIGHT_BLOCK_64X64_TX_64X64[1] = { 1, };
static const uint16_t *HAVE_TOP_RIGHT[4][5] = {
    { HAVE_TOP_RIGHT_BLOCK_8X8_TX_4X4,   HAVE_TOP_RIGHT_BLOCK_8X8_TX_8X8,   nullptr,                             nullptr,                             nullptr },
    { HAVE_TOP_RIGHT_BLOCK_16X16_TX_4X4, HAVE_TOP_RIGHT_BLOCK_16X16_TX_8X8, HAVE_TOP_RIGHT_BLOCK_16X16_TX_16X16, nullptr,                             nullptr },
    { HAVE_TOP_RIGHT_BLOCK_32X32_TX_4X4, HAVE_TOP_RIGHT_BLOCK_32X32_TX_8X8, HAVE_TOP_RIGHT_BLOCK_32X32_TX_16X16, HAVE_TOP_RIGHT_BLOCK_32X32_TX_32X32, nullptr },
    { HAVE_TOP_RIGHT_BLOCK_64X64_TX_4X4, HAVE_TOP_RIGHT_BLOCK_64X64_TX_8X8, HAVE_TOP_RIGHT_BLOCK_64X64_TX_16X16, HAVE_TOP_RIGHT_BLOCK_64X64_TX_32X32, HAVE_TOP_RIGHT_BLOCK_64X64_TX_64X64 },
};

static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_8X8_TX_4X4[16] = { 21845, 4369, 21845, 257, 21845, 4369, 21845, 1, 21845, 4369, 21845, 257, 21845, 4369, 21845, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_8X8_TX_8X8[8] = { 85, 17, 85, 1, 85, 17, 85, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_16X16_TX_4X4[16] = { 4369, 4369, 4369, 257, 4369, 4369, 4369, 1, 4369, 4369, 4369, 257, 4369, 4369, 4369, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_16X16_TX_8X8[8] = { 85, 17, 85, 1, 85, 17, 85, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_16X16_TX_16X16[4] = { 5, 1, 5, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_32X32_TX_4X4[16] = { 257, 257, 257, 257, 257, 257, 257, 1, 257, 257, 257, 257, 257, 257, 257, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_32X32_TX_8X8[8] = { 17, 17, 17, 1, 17, 17, 17, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_32X32_TX_16X16[4] = { 5, 1, 5, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_32X32_TX_32X32[2] = { 1, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_64X64_TX_4X4[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_64X64_TX_8X8[8] = { 1, 1, 1, 1, 1, 1, 1, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_64X64_TX_16X16[4] = { 1, 1, 1, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_64X64_TX_32X32[2] = { 1, 0, };
static const uint16_t HAVE_BOTTOM_LEFT_BLOCK_64X64_TX_64X64[1] = { 0, };
static const uint16_t *HAVE_BOTTOM_LEFT[4][5] = {
    { HAVE_BOTTOM_LEFT_BLOCK_8X8_TX_4X4,   HAVE_BOTTOM_LEFT_BLOCK_8X8_TX_8X8,   nullptr,                               nullptr,                               nullptr                               },
    { HAVE_BOTTOM_LEFT_BLOCK_16X16_TX_4X4, HAVE_BOTTOM_LEFT_BLOCK_16X16_TX_8X8, HAVE_BOTTOM_LEFT_BLOCK_16X16_TX_16X16, nullptr,                               nullptr                               },
    { HAVE_BOTTOM_LEFT_BLOCK_32X32_TX_4X4, HAVE_BOTTOM_LEFT_BLOCK_32X32_TX_8X8, HAVE_BOTTOM_LEFT_BLOCK_32X32_TX_16X16, HAVE_BOTTOM_LEFT_BLOCK_32X32_TX_32X32, nullptr                               },
    { HAVE_BOTTOM_LEFT_BLOCK_64X64_TX_4X4, HAVE_BOTTOM_LEFT_BLOCK_64X64_TX_8X8, HAVE_BOTTOM_LEFT_BLOCK_64X64_TX_16X16, HAVE_BOTTOM_LEFT_BLOCK_64X64_TX_32X32, HAVE_BOTTOM_LEFT_BLOCK_64X64_TX_64X64 },
};

int32_t HaveTopRight(int32_t log2BlockWidth, int32_t miRow, int32_t miCol, int32_t txSize, int32_t y, int32_t x)
{
    assert(log2BlockWidth >= 1 && log2BlockWidth <= 4); // from 8x8 to 64x64
    assert(txSize >= 0 && txSize <= 4); // from 4x4 to 64x64
    int32_t idxY = (((miRow & 7) << 1) + y) >> txSize;
    int32_t idxX = (((miCol & 7) << 1) + x) >> txSize;
    return (HAVE_TOP_RIGHT[log2BlockWidth - 1][txSize][idxY] >> idxX) & 1;
}

int32_t HaveBottomLeft(int32_t log2BlockWidth, int32_t miRow, int32_t miCol, int32_t txSize, int32_t y, int32_t x)
{
    assert(log2BlockWidth >= 1 && log2BlockWidth <= 4); // from 8x8 to 64x64
    assert(txSize >= 0 && txSize <= 4); // from 4x4 to 64x64
    int32_t idxY = (((miRow & 7) << 1) + y) >> txSize;
    int32_t idxX = (((miCol & 7) << 1) + x) >> txSize;
    return (HAVE_BOTTOM_LEFT[log2BlockWidth - 1][txSize][idxY] >> idxX) & 1;
}

template <typename PixType>
void av1_filter_intra_edge_corner(PixType *above, PixType *left)
{
    const int32_t kernel[3] = { 5, 6, 5 };
    int32_t s = (left[0] * kernel[0]) + (above[-1] * kernel[1]) + (above[0] * kernel[2]);
    s = (s + 8) >> 4;
    above[-1] = (PixType)s;
    left[-1] = (PixType)s;
}

template <typename PixType>
void av1_filter_intra_edge_corner_nv12(PixType *above, PixType *left)
{
    const int32_t kernel[3] = { 5, 6, 5 };
    // u
    int32_t s = (left[0] * kernel[0]) + (above[-2] * kernel[1]) + (above[0] * kernel[2]);
    s = (s + 8) >> 4;
    above[-2] = (PixType)s;
    left[-2] = (PixType)s;
    // v
    s = (left[1] * kernel[0]) + (above[-1] * kernel[1]) + (above[1] * kernel[2]);
    s = (s + 8) >> 4;
    above[-1] = (PixType)s;
    left[-1] = (PixType)s;
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
        p[i] = (uint8_t)s;
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
        p[2 * i - 1] = (uint8_t)s;
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


template <typename PixType, typename TCoeffType>
RdCost TransformIntraYSbAv1(BlockSize bsz, int32_t mode, int32_t haveTop, int32_t haveLeft_, TxSize txSize,
                            TxType txType, uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src_, PixType *rec_,
                            int32_t pitchRec, int16_t *diff, TCoeffType *coeff_, int16_t *qcoeff_, TCoeffType *coefWork, int32_t qp,
                            const BitCounts &bc, CostType lambda, int32_t miRow, int32_t miCol, int32_t miColEnd,
                            int32_t deltaAngle, int32_t filtType, const AV1VideoParam &par, const QuantParam &qpar, float *roundFAdj, PaletteInfo *pi, int16_t* retEob)
{
    AV1PP::IntraPredPels<PixType> predPels;
    const int bd = sizeof(PixType) == 1 ? 8 : 10;

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
    const int32_t log2w = block_size_wide_4x4_log2[bsz];
    const int32_t distToBottomEdge = (par.miRows - bh - miRow) * 8;
    const int32_t distToRightEdge = (par.miCols - bw - miCol) * 8;
    const int32_t hpx = block_size_high[bsz];
    const int32_t wpx = hpx;//block_size_width[bsz];
    const int32_t isDirectionalMode = av1_is_directional_mode((PredMode)mode);
    const int32_t angle = mode_to_angle_map[mode] + deltaAngle * ANGLE_STEP;

    for (int32_t y = 0; y < num4x4h; y += step) {
        const PixType *src = src_ + (y << 2) * SRC_PITCH;
        PixType *rec = rec_ + (y << 2) * pitchRec;
        uint8_t *color_map = pi->color_map + (y << 2) * 64;
        int32_t haveLeft = haveLeft_;
        for (int32_t x = 0; x < num4x4w; x += step, rec += width, src += width, diff += width, color_map += width) {
            int32_t blockIdx = h265_scan_r2z4[y * 16 + x];
            int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
            TCoeffType *coeff  = coeff_ + offset;
            CoeffsType *qcoeff = qcoeff_ + offset;
            TCoeffType *coefOrigin = coefWork + offset;
            int32_t txSse = 0;
            // Check haspalette_y
            if (pi && pi->palette_size_y) {
                if (pi->palette_size_y == pi->true_colors_y) {
                    CopyNxM(src, SRC_PITCH, rec, pitchRec, width, width);
                }
                else {
                    // Palette Prediction
                    AV1PP::predict_intra_palette(rec, pitchRec, txSize, &pi->palette_y[0], color_map); //use map
                    txSse = AV1PP::sse(src, SRC_PITCH, rec, pitchRec, txSize, txSize);
                    AV1PP::diff_nxn(src, SRC_PITCH, rec, pitchRec, diff, width, txSize);
                }
            }
            else {
                const int32_t haveRight = (miCol + ((x + txw) >> 1)) < miColEnd;
                const int32_t haveTopRight = haveTop && haveRight && HaveTopRight(log2w, miRow, miCol, txSize, y, x);
                // Distance between the right edge of this prediction block to the frame right edge
                const int32_t xr = distToRightEdge + (wpx - (x << 2) - txwpx);
                // Distance between the bottom edge of this prediction block to the frame bottom edge
                const int32_t yd = distToBottomEdge + (hpx - (y << 2) - txhpx);
                const int32_t haveBottom = yd > 0;
                const int32_t haveBottomLeft = haveLeft && haveBottom && HaveBottomLeft(log2w, miRow, miCol, txSize, y, x);
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

                txSse = AV1PP::sse(src, SRC_PITCH, rec, pitchRec, txSize, txSize);
                AV1PP::diff_nxn(src, SRC_PITCH, rec, pitchRec, diff, width, txSize);
            }

#ifdef ADAPTIVE_DEADZONE
            int32_t eob = 0;
            if (!txSse) {
                memset(qcoeff, 0, sizeof(CoeffsType)*width*width);  // happens quite abit with screen content coding
            } else {
                AV1PP::ftransform_av1(diff, coefOrigin, width, txSize, txType);
                eob = AV1PP::quant(coefOrigin, qcoeff, qpar, txSize);
                if (eob)
                    AV1PP::dequant(qcoeff, coeff, qpar, txSize, sizeof(PixType) == 1 ? 8 : 10);

                if (eob && roundFAdj)
                    adaptDz(coefOrigin, coeff, qpar, txSize, roundFAdj, eob);
            }
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
                SetCulLevel(aboveNzCtx+x, leftNzCtx+y, (uint8_t)culLevel, txSize);
            } else {
                txBits = cbc.txbSkip[txbSkipCtx][1];
                SetCulLevel(aboveNzCtx+x, leftNzCtx+y, 0, txSize);
            }

            if(retEob) retEob[y*16+x] = eob;
            rd.eob += eob;
            rd.sse += txSse;
            rd.coefBits += txBits;
            haveLeft = 1;
        }
        haveTop = 1;
    }
    return rd;
}
template RdCost TransformIntraYSbAv1<uint8_t, short> (
    BlockSize,int32_t,int32_t,int32_t,TxSize,TxType,uint8_t*,uint8_t*,const uint8_t*, uint8_t*, int32_t,int16_t*,int16_t*,int16_t*, int16_t*,
    int32_t,const BitCounts&,CostType,int32_t,int32_t,int32_t,int32_t,int32_t,const AV1VideoParam &par, const QuantParam &qpar, float *roundFAdj, PaletteInfo *pi, int16_t *retEob);

template RdCost TransformIntraYSbAv1<uint16_t, short>(
    BlockSize, int32_t, int32_t, int32_t, TxSize, TxType, uint8_t*, uint8_t*, const uint16_t*, uint16_t*, int32_t, int16_t*, int16_t*, int16_t*, int16_t*,
    int32_t, const BitCounts&, CostType, int32_t, int32_t, int32_t, int32_t, int32_t, const AV1VideoParam &par, const QuantParam &qpar, float *roundFAdj, PaletteInfo *pi, int16_t *retEob);

//#if ENABLE_10BIT
template RdCost TransformIntraYSbAv1<uint16_t, int>(
    BlockSize,int32_t,int32_t,int32_t,TxSize,TxType,uint8_t*,uint8_t*,const uint16_t*,uint16_t*,int32_t,int16_t*,int*,int16_t*, int*,
    int32_t,const BitCounts&,CostType,int32_t,int32_t,int32_t,int32_t,int32_t,const AV1VideoParam &par, const QuantParam &qpar, float *roundFAdj, PaletteInfo *pi, int16_t *retEob);
//#endif

template <typename PixType>
RdCost TransformIntraYSbAv1_viaTxkSearch(BlockSize bsz, int32_t mode, int32_t haveTop, int32_t haveLeft_, TxSize txSize,
                            uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src_, PixType *rec_,
                            int32_t pitchRec, int16_t *diff_, int16_t *coeff_, int16_t *qcoeff_, int32_t qp,
                            const BitCounts &bc, int32_t fastCoeffCost, CostType lambda, int32_t miRow, int32_t miCol,
                            int32_t miColEnd, int32_t miRows, int32_t miCols, int32_t deltaAngle, int32_t filtType, uint16_t* txkTypes,
                            const AV1VideoParam &par, int16_t *coeffWork_, const QuantParam &qpar, float *roundFAdj, PaletteInfo *pi)
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
    const int32_t log2w = block_size_wide_4x4_log2[bsz];
    const int32_t distToBottomEdge = (miRows - bh - miRow) * 8;
    const int32_t distToRightEdge = (miCols - bw - miCol) * 8;
    const int32_t hpx = block_size_high[bsz];
    const int32_t wpx = hpx;//block_size_width[bsz];
    const int32_t isDirectionalMode = av1_is_directional_mode((PredMode)mode);
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
        uint8_t *color_map = pi->color_map + (y << 2) * 64;
        for (int32_t x = 0; x < num4x4w; x += step, rec += width, src += width, diff += width, color_map += width) {
            int32_t blockIdx = h265_scan_r2z4[y * 16 + x];
            int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
            CoeffsType *qcoeff = qcoeff_ + offset;
            int32_t initTxSse = 0;
            // Check haspalette_y
            if (pi && pi->palette_size_y) {
                if (pi->palette_size_y == pi->true_colors_y) {
                    CopyNxM(src, SRC_PITCH, rec, pitchRec, width, width);
                }
                else {
                    // Palette Prediction
                    AV1PP::predict_intra_palette(rec, pitchRec, txSize, &pi->palette_y[0], color_map); //use map
                    initTxSse = AV1PP::sse(src, SRC_PITCH, rec, pitchRec, txSize, txSize);
                    AV1PP::diff_nxn(src, SRC_PITCH, rec, pitchRec, diff, width, txSize);
                }
            }
            else {
                const int32_t haveRight = (miCol + ((x + txw) >> 1)) < miColEnd;
                const int32_t haveTopRight = haveTop && haveRight && HaveTopRight(log2w, miRow, miCol, txSize, y, x);
                // Distance between the right edge of this prediction block to the frame right edge
                const int32_t xr = distToRightEdge + (wpx - (x << 2) - txwpx);
                // Distance between the bottom edge of this prediction block to the frame bottom edge
                const int32_t yd = distToBottomEdge + (hpx - (y << 2) - txhpx);
                const int32_t haveBottom = yd > 0;
                const int32_t haveBottomLeft = haveLeft && haveBottom && HaveBottomLeft(log2w, miRow, miCol, txSize, y, x);
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
                initTxSse = AV1PP::sse(src, SRC_PITCH, rec, pitchRec, txSize, txSize);
                AV1PP::diff_nxn(src, SRC_PITCH, rec, pitchRec, diff, width, txSize);
            }
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

            for (TxType testTxType = DCT_DCT; testTxType <= stopTxType; testTxType++) {

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
                    int32_t sseNz = int32_t(AV1PP::sse_cont(coeffOrigin, coeffTest, width*width) >> SHIFT_SSE);
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
                        adaptDz(coeffOrigin, coeffTest, qpar, txSize, &roundFAdjT[0], eob);
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
                SetCulLevel(aboveNzCtx+x, leftNzCtx+y, (uint8_t)culLevel, txSize);
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

#if ENABLE_10BIT
template RdCost TransformIntraYSbAv1_viaTxkSearch<uint16_t>(
    BlockSize,int32_t,int32_t,int32_t,TxSize,uint8_t*,uint8_t*,const uint16_t*,uint16_t*,int32_t,int16_t*,int16_t*,int16_t*,
    int32_t,const BitCounts&,int32_t,CostType,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,uint16_t*,
    const AV1VideoParam &par, int16_t* coeffWork_, const QuantParam &qpar, float *roundFAdj, PaletteInfo *pi);
#endif

uint8_t Reconstruct16x16DcOnly(const uint8_t *src, int32_t pitch, uint8_t predPel, const QuantParam &qparam, int16_t *dcQCoef)
{
    // ftransform: dc = (sum(src) - sum(pred) + 1) >> 1, all ac = 0
    __m128i s = _mm_setzero_si128();
    for (int32_t i = 0; i < 16; i++)
        s = _mm_add_epi64(s, _mm_sad_epu8(loada_si128(src + i * pitch), _mm_setzero_si128()));
    int32_t coeff = (_mm_cvtsi128_si32(s) + _mm_extract_epi32(s, 2) - predPel * 16 * 16 + 1) >> 1;

    // quant
    const int32_t sign = (coeff >> 31);
    coeff = (coeff ^ sign) - sign;
    if (coeff >= qparam.zbin[0]) {
        coeff = std::min((int32_t)INT16_MAX, coeff + qparam.round[0]);
        coeff = ((((coeff * qparam.quant[0]) >> 16) + coeff) * qparam.quantShift[0]) >> 16;
        *dcQCoef = (int16_t)coeff;
        // dequant
        coeff = ((coeff * qparam.dequant[0]) ^ sign) - sign;
        coeff = (int16_t)std::min(std::max(coeff, (int32_t)INT16_MIN), (int32_t)INT16_MAX);
        // itransform_add
        int32_t resid = (coeff * 2 + 128) >> 8;
        predPel = (uint8_t)Saturate(0, 255, predPel + resid);
    }
    return predPel;
}

template <typename PixType>
uint64_t AV1CU<PixType>::CheckIntra64x64(PredMode *bestIntraMode)
{
    alignas(16) uint32_t sumTop[4];
    __m128i s0 = _mm_shuffle_epi32(_mm_sad_epu8(loada_si128(m_yRec - m_pitchRecLuma + 0), _mm_setzero_si128()),  (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6));
    __m128i s1 = _mm_shuffle_epi32(_mm_sad_epu8(loada_si128(m_yRec - m_pitchRecLuma + 16), _mm_setzero_si128()), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6));
    __m128i s2 = _mm_shuffle_epi32(_mm_sad_epu8(loada_si128(m_yRec - m_pitchRecLuma + 32), _mm_setzero_si128()), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6));
    __m128i s3 = _mm_shuffle_epi32(_mm_sad_epu8(loada_si128(m_yRec - m_pitchRecLuma + 48), _mm_setzero_si128()), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6));
    s0 = _mm_unpacklo_epi64(s0, s1);
    s2 = _mm_unpacklo_epi64(s2, s3);
    s0 = _mm_hadd_epi32(s0, s2);
    storea_si128(sumTop, s0);

    const int32_t miRow = (m_ctbPelY >> 3);
    const int32_t miCol = (m_ctbPelX >> 3);
    const int32_t haveTop = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft = (miCol > m_tileBorders.colStart);

    const uint8_t *src = (const uint8_t *)m_ySrc;

    int16_t dcQCoef = 0;
    int32_t satd = 0;
    for (int32_t y = 0; y < 4; y++) {

        uint32_t sumLeft = 0;
        if (haveLeft)
            for (int i = 0; i < 16; i++)
                sumLeft += m_yRec[(y * 16 + i) * m_pitchRecLuma - 1];

        for (int32_t x = 0; x < 4; x++) {
            int32_t sum = 0;
            int32_t shift = 0;
            if (haveTop | y) {
                sum = sumTop[x];
                shift = 4;
            }
            if (haveLeft | x) {
                sum += sumLeft;
                shift = (shift == 0) ? 4 : 5;
            }
            uint8_t recPel = shift ? ((sum + ((1 << shift) >> 1)) >> shift) : 128;

            // for first 16x16 block: preserve DC coef and zero out AC coefs
            // for other 16x16 blocks: zero out all coefs
            if (x == 0 && y == 0)
                recPel = Reconstruct16x16DcOnly(src, SRC_PITCH, recPel, m_aqparamY, &dcQCoef);

            sumTop[x] = recPel * 16;
            sumLeft = recPel * 16;

            satd += AV1PP::satd_with_const(src + y * 16 * SRC_PITCH + x * 16, recPel, 2, 2);
        }
    }

    *bestIntraMode = DC_PRED;

    const BitCounts &bc = *m_currFrame->bitCount;
    const TxbBitCounts &cbc = bc.txb[get_q_ctx(m_lumaQp)][TX_16X16][PLANE_TYPE_Y];
    const uint16_t *intraModeBits = bc.intraModeAV1[TX_16X16];
    const ModeInfo *above = GetAbove(m_data, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left = GetLeft(m_data, miCol, m_tileBorders.colStart);
    const uint16_t *isInterBits = bc.isInter[ GetCtxIsInter(above, left) ];
    const int32_t bits = intraModeBits[DC_PRED] + isInterBits[0] + cbc.txbSkip[0][1] * 15 + (dcQCoef ? (cbc.txbSkip[0][0] + BSR(dcQCoef) * 512) : cbc.txbSkip[0][1]);
    const uint64_t bestIntraCost = RD(satd, bits, m_rdLambdaSqrtInt);
    return bestIntraCost;
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

const uint8_t mode_to_angle_map[] = {0, 90, 180, 45, 135, 113, 157, 203, 67, 0, 0, 0, 0};

typedef struct Struct_PaletteNode
{
    uint8_t color;
    uint16_t count;
    uint32_t bits;
    uint32_t sum1;
    uint32_t sum2;
    Struct_PaletteNode *L;
    Struct_PaletteNode *R;
    Struct_PaletteNode *C;
    uint16_t GetTotalCount()
    {
        return count;
        //uint32_t total_count = 0;
        //const Struct_PaletteNode *node = this;
        //do {
        //    total_count += node->count;
        //} while (node = node->C);
        //return total_count;
    }
    uint8_t GetMeanColor()
    {
        return color;
        /*float num = (int)count*color;
        float den = count;
        Struct_PaletteNode *t = C;
        while (t) {
            num += (int)t->color*(int)t->count;
            den += t->count;
            t = t->C;
        }
        uint8_t mean = (uint8_t)((num / den) + 0.5f);
        return mean;*/
    }
    uint32_t GetTotalSse(uint8_t ref_color)
    {
        return sum2 - sum1 * ref_color + count * ref_color * ref_color;
        //uint32_t total_sse = 0;
        //const Struct_PaletteNode *node = this;
        //do {
        //    total_sse += node->count * ((int)ref_color - (int)node->color) * ((int)ref_color - (int)node->color);
        //} while (node = node->C);
        //return total_sse;
    }
    void Add(Struct_PaletteNode *p) {
        Struct_PaletteNode *node = this;
        while (node->C)
            node = node->C;
        node->C = p;
        count += p->count;
        sum1 += p->sum1;
        sum2 += p->sum2;
    }
    Struct_PaletteNode * MergeL(Struct_PaletteNode **tail) {
        if (!this->L) {
            assert(0);
            return this;
        } else {
            this->L->R = this->R;
            if (!this->R) {
                if (tail) *tail = this->L;
            } else {
                this->R->L = this->L;
            }
            this->L->Add(this);
            return this->L;
        }
    }
    Struct_PaletteNode * MergeR(Struct_PaletteNode **head) {
        if (!this->R) {
            assert(0);
            return this;
        } else {
            this->R->L = this->L;
            if (!this->L) {
                if(head) *head = this->R;
            } else {
                this->L->R = this->R;
            }
            this->R->Add(this);
            return this->R;
        }
    }
} PaletteNode;

static const int16_t LOG2_MUL_256[32] = {
       0,  256,  405,  512,  594,  661,  718,  768,  811,  850,  885,  917,  947,  974, 1000, 1024,
    1046, 1067, 1087, 1106, 1124, 1141, 1158, 1173, 1188, 1203, 1217, 1230, 1243, 1256, 1268, 1280
};

uint32_t FastLog2Mul256(uint16_t x) {
    if (x <= 32)
        return LOG2_MUL_256[x - 1];
    // linear approximation
    const int32_t bsr = BSR(x);
    return (bsr << 8) + (((x - (1 << bsr)) << 8) >> bsr);
}

uint32_t ColorBits(uint16_t icount, int32_t log2Size)
{
    //float p = icount * isz;
    //float si = (-log2(p));
    //uint32_t colbits = si * 512;
    //if (icount > 1) colbits += (uint32_t)(si * (float)(icount - 1) * 256);

    uint32_t entropy = (log2Size << 8) - FastLog2Mul256(icount);
    uint32_t colbits = entropy * (icount + 1);
    return colbits;
}

template <typename PixType>
uint8_t GetColorCount(PixType* srcSb, uint32_t width, uint32_t height, uint8_t *ret_palette, uint32_t bitdepth, CostType rdLambda, PaletteInfo *pi, uint8_t *color_map, uint32_t &bits, bool optimize)
{
    const int MAX_EXT_PALETTE = 24;
    int EXT_PALETTE = MAX_EXT_PALETTE;// +((width > 16) ? 16 : ((width > 8) ? 12 : 8));
    int colors = 0;
    bits = 0;

    if (bitdepth <= 8) {
        int16_t color_count[256] = { 0 };
        PixType plt[MAX_EXT_PALETTE + 1] = { 0 };
        const PixType* src = srcSb;
        for (uint32_t i = 0; i < height && colors <= EXT_PALETTE; i++) {
            for (uint32_t j = 0; j < width; j++) {
                const PixType v = *src;
                if (!color_count[v]++) {
                    plt[colors] = v;
                    colors++;
                    if (colors > EXT_PALETTE) break;
                }
                src++;
            }
            src = srcSb + ((i + 1) << 6);
        }

        pi->true_colors_y = (uint8_t)colors;
        pi->sse = 0;
        pi->color_map = color_map;

        if (colors > 1 && colors <= EXT_PALETTE) {
            const int32_t log2Size = BSR(width * height);
            uint8_t map_enc[256];
            PaletteNode palette[MAX_EXT_PALETTE] = { 0 };

            int k;
            std::sort(std::begin(plt), std::begin(plt) + colors);
            for (k = 0; k < colors; k++) {
                const int idx = plt[k];
                map_enc[idx] = k;
                palette[k].color = idx;
                palette[k].count = color_count[idx];
                palette[k].sum1 = color_count[idx] * idx * 2;
                palette[k].sum2 = color_count[idx] * idx * idx;
                if (k) {
                    palette[k].L = &palette[k - 1];
                    palette[k - 1].R = &palette[k];
                }
            }
            if (!optimize) {
                if (colors > 1 && colors <= MAX_PALETTE) {
                    for (uint16_t i = 0; i < colors; i++) {
                        uint16_t icount = palette[i].count;
                        ret_palette[i] = palette[i].color;
                        uint32_t colbits = ColorBits(icount, log2Size);
                        bits += colbits;
                    }
                    for (uint32_t i = 0;i < height;i++) {
                        for (uint32_t j = 0; j < width; j++) {
                            const int idx = (i << 6) + j;
                            color_map[idx] = map_enc[srcSb[idx]];
                        }
                    }
                }
            }
            else {
                PaletteNode *curr = NULL;
                PaletteNode *head = &palette[0];
                PaletteNode *tail = &palette[k - 1];
                int prevk = 0;
                do {
                    // RD Opt Palette
                    curr = head;
                    prevk = k;
                    while (curr && k > 1) {
                        uint16_t icount = curr->GetTotalCount();
                        uint32_t colbits = ColorBits(icount, log2Size);
                        curr->bits = colbits;
                        uint32_t sseL = (curr->L ? curr->GetTotalSse(curr->L->GetMeanColor()) : INT_MAX);
                        uint32_t sseR = (curr->R ? curr->GetTotalSse(curr->R->GetMeanColor()) : INT_MAX);

                        if (sseL <= sseR) {
                            uint16_t icountL = curr->L->GetTotalCount();
                            CostType colCost = 0;
                            if (k > 2 && icountL > icount) {
                                uint32_t ncolbits = ColorBits(icountL + icount, log2Size);
                                colCost = (curr->L->bits + colbits - ncolbits)* rdLambda;
                            }
                            if (icountL > icount && colCost > sseL) {
                                curr = curr->MergeL(&tail);
                                k--;
                            } else {
                                curr = curr->R;
                            }
                        }
                        else {
                            uint16_t icountR = curr->R->GetTotalCount();
                            CostType colCost = 0;
                            if (k > 2 && icountR >= icount) {
                                uint32_t rcolbits = ColorBits(icountR, log2Size);
                                uint32_t ncolbits = ColorBits(icountR + icount, log2Size);
                                colCost = (rcolbits + colbits - ncolbits)* rdLambda;
                            }
                            if (icountR >= icount && colCost > sseR) {
                                curr = curr->MergeR(&head);
                                k--;
                            } else {
                                curr = curr->R;
                            }
                        }
                    }
                    // Force reduction
                    while (k > MAX_PALETTE) {
                        PaletteNode *curr = head;
                        uint32_t bestSse = INT_MAX;
                        PaletteNode *minCand = NULL;
                        PaletteNode *minMergeL = NULL;
                        PaletteNode *minMergeR = NULL;

                        while (curr)
                        {
                            uint32_t sseL = (curr->L ? curr->GetTotalSse(curr->L->GetMeanColor()) : INT_MAX);
                            uint32_t sseR = (curr->R ? curr->GetTotalSse(curr->R->GetMeanColor()) : INT_MAX);
                            if (sseL <= sseR) {
                                if (sseL < bestSse) {
                                    bestSse = sseL;
                                    minCand = curr;
                                    minMergeL = curr->L;
                                    minMergeR = NULL;
                                }
                            }
                            else {
                                if (sseR < bestSse) {
                                    bestSse = sseR;
                                    minCand = curr;
                                    minMergeL = NULL;
                                    minMergeR = curr->R;
                                }
                            }
                            curr = curr->R;
                        }
                        if (minCand) {
                            if (minMergeL) {
                                minCand = minCand->MergeL(&tail);
                                k--;
                            }
                            else if (minMergeR) {
                                minCand = minCand->MergeR(&head);
                                k--;
                            }
                        }
                    };
                } while (k < prevk && k>1);

                colors = k;
                if (colors > 1 && colors <= MAX_PALETTE) {
                    PaletteNode *t = head;
                    uint8_t i = 0;
                    bits = 0;
                    while (t) {
                        ret_palette[i] = t->GetMeanColor();
                        bits += t->bits;
                        pi->sse += t->GetTotalSse(t->GetMeanColor());

                        for (PaletteNode *c = t; c; c = c->C)
                            map_enc[c->color] = i;

                        t = t->R;
                        i++;
                    }
                    assert(i == k);
                    const __m256i shuftab = _mm256_set1_epi32(0x0C080400);
                    for (uint32_t i = 0;i < height;i++) {
                        //for (uint32_t j = 0; j < width; j++) {
                        //    const int idx = (i << 6) + j;
                        //    color_map[idx] = map_enc[srcSb[idx]];
                        //}
                        for (uint32_t j = 0; j < width; j += 8) {
                            __m256i c = _mm256_cvtepu8_epi32(loadl_epi64(srcSb + i * SRC_PITCH + j));
                            __m256i m = _mm256_i32gather_epi32((int32_t *)map_enc, c, 1);
                            m = _mm256_shuffle_epi8(m, shuftab);
                            storel_si32(color_map + i * SRC_PITCH + j + 0, si128_lo(m));
                            storel_si32(color_map + i * SRC_PITCH + j + 4, si128_hi(m));
                        }
                    }
                }
            }
        }
    } else {
        // not implemented
        assert(0);
    }

    return (uint8_t) colors;
}

template <typename PixType>
uint8_t GetColorCountUV(PixType* srcSb, uint32_t width, uint32_t height, uint8_t *ret_palette, uint32_t bitdepth, CostType rdLambda, PaletteInfo *pi, uint8_t *color_map, uint32_t &bits, bool& is_dist)
{
    //Need to create joined palette with one index and separate palettes

    const int EXT_PALETTE = MAX_PALETTE + 8;
    int colorsU = 0;
    int colorsV = 0;
    bits = 0;

    if (bitdepth <= 8) {
        int color_countU[256] = { 0 };
        int color_countV[256] = { 0 };
        int palette_u[MAX_PALETTE] = { -1 };
        int palette_v[MAX_PALETTE] = { -1, -1, -1, -1, -1, -1, -1, -1 };
        //joint histogram

        PixType* src = srcSb;
        for (uint32_t i = 0; i < height; i++) {
            for (uint32_t j = 0; j < (width << 1); j += 2) {
                const int u = src[j];
                const int v = src[j + 1];
                const int cu = !color_countU[u]++;
                const int cv = !color_countV[v]++;
                if (cu) {
                    palette_u[colorsU&(MAX_PALETTE - 1)] = u;
                    colorsU += cu;
                }
                if (cv) {
                    colorsV += cv;
                }
            }
            src += 64;
        }

        int max_colors = std::max(colorsU, colorsV);
        pi->true_colors_uv = max_colors;
        pi->sseUV = 0;
        pi->color_map = color_map;

        //don't optimize colors for now - need to optimize u and v jointly as they share the same index
        if (colorsU > 1 && colorsV > 1 && colorsV <= colorsU && max_colors <= MAX_PALETTE) {
            //U palette should be in increasing order
            std::sort(std::begin(palette_u), std::begin(palette_u) + colorsU);
            int map_idx[256];
            for (int i = 0; i < colorsU; i++) {
                map_idx[palette_u[i]] = i;
            }
#if 0
            int map_idx1[256];
            for (int i = 0; i < colorsV; i++) {
                map_idx1[palette_v[i]] = i;
            }
#endif
            for (uint32_t i = 0; i < height; i++) {
                const int idx = (i << 6);
                for (uint32_t j = 0; j < width; j++) {
                    const int off = idx + (j << 1);
                    const int im = map_idx[srcSb[off]];
                    color_map[idx + j] = im;
                    PixType v = srcSb[off + 1];
                    const int vi = palette_v[im];
                    if (vi >= 0 && vi != v) {
                        //choose color with more count
                        if (color_countV[v] < color_countV[vi]) {
                            v = vi;
                        }
                        is_dist = true;
                    }
                    palette_v[im] = v;
                }
            }

            float log2_isz = log2(1.0 / (width * height));
            for (int i = 0; i < max_colors; i++) {
                const int color = palette_u[i];
                uint16_t icount = color_countU[color];
                ret_palette[i] = color;
                const int color_v = palette_v[i];
                ret_palette[MAX_PALETTE + i] = color_v == -1 ? ret_palette[MAX_PALETTE + i - 1] : color_v;
                float si = -(log2(icount) + log2_isz);
                uint32_t colbits = si * 512;
                if (icount > 1) colbits += (uint32_t)(si * ((icount - 1) * 256));
                bits += colbits;
            }

            //U palette reassign to minimize sse?

            return max_colors;
        }

        return 0;
    }
    else {
        // not implemented
        assert(0);
    }

    return 0;
}

static const int32_t HashToPaletteColorContext[9] = { -1, -1, 0, -1, -1, 4,  3,  2, 1 };

uint32_t GetPaletteColorContext(const uint8_t *colorMap, int32_t pitch, int32_t row, int32_t col, int32_t &new_color_idx)
{
    assert(row || col);

    const uint8_t *pCurColor = colorMap + row * pitch + col;
    const uint8_t curColor = pCurColor[0];
    const uint8_t curColorComplement = MAX_PALETTE - curColor; // complemented colors are convenient for sorting

    int32_t numNonZeroScores = 1;
    uint8_t nonZeroScores[3] = {}; // 4 MSB - score, 4 LSB - color complement

    // set scores for neighboring colors
    if (row == 0) {
        // first row then there is only one neighbor (left)
        nonZeroScores[0] = (2 << 4) | (MAX_PALETTE - pCurColor[-1]);
    } else if (col == 0) {
        // first column then there is only one neighbor (top)
        nonZeroScores[0] = (2 << 4) | (MAX_PALETTE - pCurColor[-pitch]);
    } else {
        // there may be from 1 to 3 distinct colors
        const uint8_t left = MAX_PALETTE - pCurColor[-1];
        const uint8_t top = MAX_PALETTE - pCurColor[-pitch];
        const uint8_t topLeft = MAX_PALETTE - pCurColor[-pitch - 1];

        // there are 5 different cases
        if (left == top) {
            if (left == topLeft) { // #1 all the same
                nonZeroScores[0] = (5 << 4) | left;
                numNonZeroScores = 1;
            } else { // #2 top-left is distinct
                nonZeroScores[0] = (4 << 4) | left;
                nonZeroScores[1] = (1 << 4) | topLeft;
                numNonZeroScores = 2;
            }
        } else {
            if (left == topLeft) { // #3 top is distinct
                nonZeroScores[0] = (3 << 4) | left;
                nonZeroScores[1] = (2 << 4) | top;
                numNonZeroScores = 2;
            } else if (top == topLeft) { // #4 left is distinct
                nonZeroScores[0] = (3 << 4) | top;
                nonZeroScores[1] = (2 << 4) | left;
                numNonZeroScores = 2;
            } else { // #4 all are distinct
                nonZeroScores[0] = (2 << 4) | std::max(left, top);
                nonZeroScores[1] = (2 << 4) | std::min(left, top);
                nonZeroScores[2] = (1 << 4) | topLeft;
                numNonZeroScores = 3;
            }
        }
    }
    assert(numNonZeroScores > 0);
    assert(numNonZeroScores < 2 || nonZeroScores[0] > nonZeroScores[1]);
    assert(numNonZeroScores < 3 || nonZeroScores[1] > nonZeroScores[2]);


    int32_t newColorIdx = curColor;

    // Find new index for current color
    // It can be one of scored colors
    //   then its index is index of that color (after sorting)
    // Or it can be one of unscored colors
    //   then its index is incremented by number of scored colors
    //   which changed order relative to current color
    if (curColorComplement == (nonZeroScores[0] & 0xf)) {
        newColorIdx = 0;
    } else if (curColorComplement == (nonZeroScores[1] & 0xf)) {
        newColorIdx = 1;
    } else if (curColorComplement == (nonZeroScores[2] & 0xf)) {
        newColorIdx = 2;
    } else {
        if (curColorComplement > (nonZeroScores[0] & 0xf))
            newColorIdx++;
        if (numNonZeroScores > 1) {
            if (curColorComplement > (nonZeroScores[1] & 0xf))
                newColorIdx++;
            if (numNonZeroScores > 2 && curColorComplement > (nonZeroScores[2] & 0xf))
                newColorIdx++;
        }
    }

    const int32_t colorContextHash = (nonZeroScores[0] >> 4) + ((nonZeroScores[1] >> 4) << 1) + ((nonZeroScores[2] >> 4) << 1); // Palette_Color_Hash_Multipliers = {1,2,2}
    const int32_t colorContext = HashToPaletteColorContext[colorContextHash];
    assert(colorContext >= 0);
    assert(colorContext < 5);

    new_color_idx = newColorIdx;
    return colorContext;
}

template <typename PixType>
uint32_t GetPaletteTokensCost(const BitCounts &bc, PixType *src, int32_t width, int32_t height, uint8_t *palette, uint32_t palette_size, uint8_t *color_map)
{
    uint32_t bitCost = write_uniform_cost(palette_size, color_map[0]);

    const uint16_t(&colorBits)[5][MAX_PALETTE] = bc.PaletteColorIdxY[palette_size - 2];

    // when estimating bits no need to follow wavefront order
    int32_t j = 1;
    for (int32_t i = 0; i < height; ++i) {
        for (; j < width; ++j) {
            int32_t new_color_idx = color_map[i * MAX_CU_SIZE + j];
            int32_t ctx = GetPaletteColorContext(color_map, MAX_CU_SIZE, i, j, new_color_idx);
            bitCost += colorBits[ctx][new_color_idx];
        }
        j = 0;
    }

    return bitCost;
}

template <typename PixType>
uint32_t GetPaletteTokensCostUV(const BitCounts &bc, PixType *src, int32_t width, int32_t height, uint8_t *palette, uint32_t palette_size, uint8_t *color_map)
{
    uint32_t bitCost = write_uniform_cost(palette_size, color_map[0]);

    const uint16_t(&colorBits)[5][MAX_PALETTE] = bc.PaletteColorIdxUV[palette_size - 2];

    // when estimating bits no need to follow wavefront order
    int32_t j = 1;
    for (int32_t i = 0; i < height; ++i) {
        for (; j < width; ++j) {
            int32_t new_color_idx = color_map[i * MAX_CU_SIZE + j];
            int32_t ctx = GetPaletteColorContext(color_map, MAX_CU_SIZE, i, j, new_color_idx);
            bitCost += colorBits[ctx][new_color_idx];
        }
        j = 0;
    }

    return bitCost;
}

uint32_t GetPaletteDeltaCost(uint8_t *colors_xmit, uint32_t colors_xmit_size, uint32_t bitdepth, uint32_t min_delta)
{
    int32_t index = 0;
    int32_t bits = 0;
    int32_t delta_bits = bitdepth - 3;
    uint32_t max_delta_bits = delta_bits;
    if (colors_xmit_size) {
        bits += bitdepth;
        index++;
    }
    if (index < colors_xmit_size) {
        int32_t max_delta = 0;
        for (uint32_t i = 1; i < colors_xmit_size; i++) {
            int32_t delta = abs((int)colors_xmit[i] - (int)colors_xmit[i - 1]);
            if (delta > max_delta) max_delta = delta;
        }
        assert(max_delta >= min_delta);
        int32_t max_val = (max_delta - min_delta);
        max_delta_bits = (max_val?BSR(max_val)+1:0);
        max_delta_bits = MAX(delta_bits, max_delta_bits);
        bits += 2;  // extra bits
        for (int i = 1; i < colors_xmit_size; ++i) {
            bits += max_delta_bits;
            int32_t range = (1 << bitdepth) -1 - colors_xmit[i] - min_delta;
            uint32_t range_bits = (range?BSR(range)+1:0);
            max_delta_bits = MIN(range_bits, max_delta_bits);
        }
    }
    return bits;
}

static int CeilLog2(int n) {
    if (n < 2) return 0;
    int i = 1, p = 2;
    while (p < n) {
        i++;
        p = p << 1;
    }
    return i;
}

int GetPaletteDeltaBitsV(const uint8_t* palette_v, int palette_size_uv, int bitDepth, int *zeroCount, int *minBits) {
    const int n = palette_size_uv;
    const int maxVal = 1 << bitDepth;
    int max_d = 0;
    *minBits = bitDepth - 4;
    *zeroCount = 0;
    for (int i = 1; i < n; ++i) {
        const int delta = palette_v[i] - palette_v[i - 1];
        const int v = abs(delta);
        const int d = std::min(v, maxVal - v);
        if (d > max_d) max_d = d;
        if (d == 0) ++(*zeroCount);
    }
    return std::max(CeilLog2(max_d + 1), *minBits);
}

uint32_t GetPaletteCacheIndicesCost(uint8_t *palette, uint32_t palette_size, uint8_t *cache, uint32_t cache_size, uint8_t *colors_xmit, uint32_t &index_bits)
{
    index_bits = 0;
    if (cache_size == 0) {
        for (int i = 0; i < palette_size; ++i) colors_xmit[i] = palette[i];
        return palette_size;
    }
    uint32_t colors_found = 0;
    uint8_t palette_found[MAX_PALETTE] = { 0 };
    for (int32_t index = 0; index < cache_size && colors_found < palette_size; index++) {
        for (uint32_t k = 0; k < palette_size; k++) {
            if (cache[index] == palette[k]) {
                colors_found++;
                palette_found[k] = 1;
                break;
            }
            index_bits++;
        }
    }
    uint32_t i = 0;
    for (int32_t k = 0; k < palette_size; k++) {
        if (!palette_found[k]) colors_xmit[i++] = palette[k];
    }
    assert(i == palette_size - colors_found);
    return i;
}

template <typename PixType>
uint32_t AV1CU<PixType>::GetPaletteCache(uint32_t miRow, bool isLuma, uint32_t miCol, uint8_t *cache)
{
    PaletteInfo *abovePalette = NULL;
    PaletteInfo *leftPalette = NULL;
    const int32_t haveTop = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft = (miCol > m_tileBorders.colStart);
    int32_t aiz = 0;
    int32_t liz = 0;
    uint8_t* a_palette{ nullptr };
    uint8_t* l_palette{ nullptr };
    if (haveTop && (miRow % 8)) {
        abovePalette = &m_currFrame->m_fenc->m_Palette8x8[(miRow - 1)*m_par->miPitch + miCol];
        if (isLuma) {
            aiz = abovePalette->palette_size_y;
            a_palette = abovePalette->palette_y;
        }
        else {
            aiz = abovePalette->palette_size_uv;
            a_palette = abovePalette->palette_u; //different for v
        }
    }
    if (haveLeft) {
        leftPalette = &m_currFrame->m_fenc->m_Palette8x8[miRow*m_par->miPitch + miCol - 1];
        if (isLuma) {
            liz = leftPalette->palette_size_y;
            l_palette = leftPalette->palette_y;
        }
        else {
            liz = leftPalette->palette_size_uv;
            l_palette = leftPalette->palette_u; //different for v
        }
    }

    if (aiz == 0 && liz == 0) return 0;

    uint32_t ai = 0;
    uint32_t li = 0;
    uint32_t colors_found = 0;
    while (ai < aiz  && li < liz) {
        uint16_t aboveColor = a_palette[ai];
        uint16_t leftColor = l_palette[li];
        if (leftColor < aboveColor) {
            if (colors_found == 0 || leftColor != cache[colors_found - 1]) {
                cache[colors_found++] = leftColor;
            }
            li++;
        } else {
            if (colors_found == 0 || aboveColor != cache[colors_found - 1]) {
                cache[colors_found++] = aboveColor;
            }
            ai++;
            if (leftColor == aboveColor) li++;
        }
    }
    while (ai < aiz) {
        uint16_t aboveColor = a_palette[ai++];
        if (colors_found == 0 || aboveColor != cache[colors_found - 1]) {
            cache[colors_found++] = aboveColor;
        }
    }
    while (li < liz) {
        uint16_t leftColor = l_palette[li++];
        if (colors_found == 0 || leftColor != cache[colors_found - 1]) {
            cache[colors_found++] = leftColor;
        }
    }
    assert(colors_found <= 2*MAX_PALETTE);
    return colors_found;
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
    const BitCounts &bc = *m_currFrame->bitCount;
    const int32_t ctxSkip = GetCtxSkip(above, left);
    const uint16_t *intraModeBits = m_currFrame->IsIntra()
        ? bc.kfIntraModeAV1[ intra_mode_context[aboveMode] ][ intra_mode_context[leftMode] ]
        : bc.intraModeAV1[maxTxSize];
    const uint16_t *txSizeBits = bc.txSize[maxTxSize][ GetCtxTxSizeAV1(above, left, aboveTxfm, leftTxfm, maxTxSize) ];
    const int32_t ctxIsInter = m_currFrame->IsIntra() ? 0 : GetCtxIsInter(above, left);
    const uint32_t isInterBits = m_currFrame->IsIntra() ? 0 : bc.isInter[ctxIsInter][0];

    PredMode bestMode = DC_PRED;
    int32_t bestDelta = 0;
    const int32_t x = 0, y = 0;
    const int32_t txw = tx_size_wide_unit[txSize];
    assert(txw == (width >> 2));
    const int32_t haveRight = (miCol + ((x + txw) >> 1)) < miColEnd;
    const int32_t log2w = block_size_wide_4x4_log2[bsz];
    const int32_t log2h = block_size_high_4x4_log2[bsz];
    const int32_t haveTopRight = haveTop && haveRight && HaveTopRight(log2w, miRow, miCol, txSize, y, x);
    const int32_t txwpx = 4 << txSize;
    const int32_t txhpx = 4 << txSize;
    const int32_t bh = block_size_high[bsz] >> 3;
    const int32_t mb_to_bottom_edge = ((mi_rows - bh - miRow) * 8) * 8;
    const int32_t hpx = width;
    const int32_t yd = (mb_to_bottom_edge >> 3) + (hpx - y - txhpx);
    const int32_t haveBottom = yd > 0;
    const int32_t haveBottomLeft = haveLeft && haveBottom && HaveBottomLeft(log2w, miRow, miCol, txSize, y, x);
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
    if (depth && !m_isSCC[depth-1]) {
        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 3 * size, width, maxTxSize, 0, 0, D45_PRED, 0, 0, 0);
        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 4 * size, width, maxTxSize, 0, 0, D135_PRED, 0, 0, 0);
        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 5 * size, width, maxTxSize, 0, 0, D117_PRED, 0, 0, 0); // 5
        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 6 * size, width, maxTxSize, 0, 0, D153_PRED, 0, 0, 0);
        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 7 * size, width, maxTxSize, 0, 0, D207_PRED, 0, 0, 0); // 7
        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 8 * size, width, maxTxSize, 0, 0, D63_PRED, 0, 0, 0);
        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + 9 * size, width, maxTxSize, 0, 0, SMOOTH_PRED, 0, 0, 0); // 9
    }
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll +10 * size, width, maxTxSize, 0, 0, SMOOTH_V_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll +11 * size, width, maxTxSize, 0, 0, SMOOTH_H_PRED, 0, 0, 0);
    AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll +12 * size, width, maxTxSize, 0, 0, PAETH_PRED, 0, 0, 0);

    int32_t bestCost = AV1PP::satd(srcSb, m_predIntraAll, width, txSize, txSize);
    bestCost += int32_t(m_rdLambdaSqrt * intraModeBits[DC_PRED] + 0.5f);
    for (PredMode mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
        if (depth && m_isSCC[depth-1] && mode > H_PRED && mode < SMOOTH_V_PRED) continue;
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
            PredMode mode = bestMode;
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

    PaletteInfo *pi = &m_currFrame->m_fenc->m_Palette8x8[miCol + miRow * m_par->miPitch];
    pi->palette_size_y = 0;
    pi->color_map = NULL;
    pi->palette_size_uv = 0;
    uint32_t palette_bits = 0;

    const Contexts origCtx = m_contexts;
    uint8_t *anz = m_contexts.aboveNonzero[0]+x4;
    uint8_t *lnz = m_contexts.leftNonzero[0]+y4;
    int16_t *diff = vp9scratchpad.diffY;
    int16_t *coef = vp9scratchpad.coefY;
    int16_t *qcoef = m_coeffWorkY + 16 * absPartIdx;
    int16_t *coefWork = (int16_t *)vp9scratchpad.varTxCoefs;

    const int32_t filtType = get_filt_type(above, left, PLANE_TYPE_Y);

    RdCost rd = TransformIntraYSbAv1(bsz, bestMode, haveTop, haveLeft, maxTxSize, DCT_DCT, anz, lnz, srcSb,
                                     recSb, m_pitchRecLuma, diff, coef, qcoef, coefWork, m_lumaQp, bc, m_rdLambda, miRow,
                                     miCol, miColEnd, bestDelta, filtType, *m_par, m_aqparamY, NULL, pi, NULL);

    if (bsz >= BLOCK_8X8
        && block_size_wide[bsz] <= 64
        && block_size_high[bsz] <= 64
        && m_currFrame->m_allowPalette && rd.eob) {

        uint8_t palette[MAX_PALETTE];
        uint16_t palette_size = 0;
        uint8_t *color_map = &m_paletteMapY[depth][sby*MAX_CU_SIZE + sbx];
        uint32_t map_bits_est;
        // CHECK Palette Mode
        palette_size = GetColorCount(srcSb, width, width, palette, m_par->bitDepthLuma, m_rdLambda, pi, color_map, map_bits_est);
        if (palette_size > 1 && palette_size <= MAX_PALETTE) {
            const uint32_t ctxB = block_size_wide_4x4_log2[bsz] + block_size_high_4x4_log2[bsz] - 2;
            const int32_t sizeU = (above ? m_currFrame->m_fenc->m_Palette8x8[miCol + (miRow - 1) * m_par->miPitch].palette_size_y : 0);
            const int32_t sizeL = (left ? m_currFrame->m_fenc->m_Palette8x8[(miCol - 1) + miRow * m_par->miPitch].palette_size_y : 0);
            const int32_t ctxC = 0 + (sizeU ? 1 : 0) + (sizeL ? 1 : 0);
            uint8_t cache[2 * MAX_PALETTE];
            uint8_t colors_xmit[MAX_PALETTE];
            palette_bits += bc.HasPaletteY[ctxB][ctxC][1];
            palette_bits += bc.PaletteSizeY[ctxB][palette_size - 2];
            uint32_t cache_size = GetPaletteCache(miRow, true, miCol, cache);
            uint32_t index_bits = 0;
            uint32_t colors_xmit_size = GetPaletteCacheIndicesCost(palette, palette_size, cache, cache_size, colors_xmit, index_bits);
            palette_bits += index_bits * 512; // cache index used unused bits
            palette_bits += 512 * GetPaletteDeltaCost(colors_xmit, colors_xmit_size, m_par->bitDepthLuma, 1);

            //uint32_t map_bits = GetPaletteTokensCost(bc, srcSb, width, width, palette, palette_size, color_map);
            palette_bits += map_bits_est;

            uint32_t bits = intraModeBits[bestMode];
            if (av1_is_directional_mode(bestMode)) {
                int32_t max_angle_delta = MAX_ANGLE_DELTA;
                int32_t delta = bestDelta;
                bits += write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
            }
            if (rd.eob)
                bits += rd.coefBits;
            CostType cost = rd.sse + m_rdLambda * bits;

            CostType paletteCost = m_rdLambda * (intraModeBits[DC_PRED] + palette_bits) + pi->sse;
            if (cost > 0.5*paletteCost && cost < 2.0*paletteCost) {
                // get better estimate
                palette_bits -= map_bits_est;
                uint32_t map_bits = GetPaletteTokensCost(bc, srcSb, width, width, palette, palette_size, color_map);
                palette_bits += map_bits;
                paletteCost = m_rdLambda * (intraModeBits[DC_PRED] + palette_bits) + pi->sse;
            }
            if (cost > paletteCost) {
                bestCost = cost;
                bestMode = DC_PRED;
                pi->palette_size_y = palette_size;
                pi->palette_bits = palette_bits;
                for (int k = 0; k < palette_size; k++)
                    pi->palette_y[k] = palette[k];

                rd = TransformIntraYSbAv1(bsz, bestMode, haveTop, haveLeft, maxTxSize, bsz <= BLOCK_16X16 ? IDTX : DCT_DCT, anz, lnz, srcSb,
                    recSb, m_pitchRecLuma, diff, coef, qcoef, coefWork, m_lumaQp, bc, m_rdLambda, miRow,
                    miCol, miColEnd, bestDelta, filtType, *m_par, m_aqparamY, NULL, pi, NULL);
            }
        }
    }

    const int32_t num4x4w = 1 << log2w;
    const int32_t num4x4h = 1 << log2h;
    const int32_t num4x4 = num_4x4_blocks_lookup[bsz];

    rd.modeBits = isInterBits + txSizeBits[txSize] + intraModeBits[bestMode] + (pi->palette_size_y ? palette_bits : 0);
    if (av1_is_directional_mode(bestMode)) {
        int32_t max_angle_delta = MAX_ANGLE_DELTA;
        int32_t delta = bestDelta;
        rd.modeBits += write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
    }
    const uint16_t *skipBits = bc.skip[ctxSkip];
    CostType cost = (rd.eob == 0)
        ? rd.sse + m_rdLambda * (rd.modeBits + skipBits[1])
        : rd.sse + m_rdLambda * (rd.modeBits + skipBits[0] + rd.coefBits/* + txTypeBits[DCT_DCT]*/);
    CostType bestCost_ = cost;

    int use_intrabc = 0;
    alignas(32) uint16_t varTxInfo[16 * 16];
    if (m_currFrame->m_allowIntraBc && m_par->ibcModeDecision && bsz <= BLOCK_16X16)
    {
        alignas(64) PixType bestRec[64 * 64];
        alignas(64) int16_t bestQcoef[64 * 64];

        int i = absPartIdx;
        RdCost bestRd = rd;
        Contexts bestCtx = m_contexts;
        CopyNxN(recSb, m_pitchRecLuma, bestRec, num4x4w << 2);
        CopyCoeffs(qcoef, bestQcoef, num4x4 << 4);

        m_contexts = origCtx;
        mi->sbType = bsz;

        int32_t test = CheckIntraBlockCopy(i, qcoef, rd, varTxInfo, (rd.eob || pi->palette_size_y), bestCost_);
        if (test) {
            cost = (rd.eob == 0)
                ? rd.sse + m_rdLambda * (rd.modeBits + skipBits[1])
                : rd.sse + m_rdLambda * (rd.modeBits + skipBits[0] + rd.coefBits);

            if (bestCost_ > cost) {
                use_intrabc = 1;
            } else {
                m_contexts = bestCtx;
                rd = bestRd;
                CopyNxN(bestRec, num4x4w << 2, recSb, m_pitchRecLuma, num4x4w << 2);
                CopyCoeffs(bestQcoef, qcoef, num4x4 << 4);
            }
        }
    }

    if (use_intrabc) {
        mi->refIdx[0] = INTRA_FRAME;
        mi->refIdx[1] = NONE_FRAME;
        mi->refIdxComb = INTRA_FRAME;
        mi->sbType = bsz;
        mi->skip = (rd.eob == 0);
        //mi->txSize = txSize;
        mi->angle_delta_y = 0;
        mi->mode = DC_PRED;
        pi->palette_size_y = 0;
        mi->interp0 = mi->interp1 = BILINEAR;
        //mi->mv and mvd are set by IBC
        mi->memCtx.skip = ctxSkip;
        mi->memCtx.isInter = 0;
        mi->angle_delta_y = MAX_ANGLE_DELTA + 0;

        PropagateSubPart(mi, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);
        PropagatePaletteInfo(m_currFrame->m_fenc->m_Palette8x8, miRow, miCol, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);

        CopyVarTxInfo(varTxInfo, m_currFrame->m_fenc->m_txkTypes4x4 + m_ctbAddr * 256 + (miRow & 7) * 32 + (miCol & 7) * 2, num4x4w);
        CopyTxSizeInfo(varTxInfo, mi, m_par->miPitch, num4x4w);

    } else {
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

        PropagateSubPart(mi, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);
        PropagatePaletteInfo(m_currFrame->m_fenc->m_Palette8x8, miRow, miCol, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);
    }
    return rd;
}

template <typename PixType>
RdCost AV1CU<PixType>::CheckIntraChromaNonRdAv1(int32_t absPartIdx, int32_t depth, PartitionType partition, const PixType* bestLuma, int lumaStride)
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
    const TxSize txSize = std::min(TxSize(TX_32X32), TxSize(3 - depth));
    const int32_t x4 = x4Luma >> m_par->subsamplingX;
    const int32_t y4 = y4Luma >> m_par->subsamplingY;
    const int32_t sseShift = m_par->bitDepthChromaShift<<1;

    PixType *recSb = m_uvRec + ((x4*2 + y4*m_pitchRecChroma) << LOG2_MIN_TU_SIZE);
    PixType *srcSb = m_uvSrc + ((x4*2 + y4*SRC_PITCH) << LOG2_MIN_TU_SIZE);

    typedef typename ChromaUVPixType<PixType>::type ChromaPixType;
    AV1PP::IntraPredPels<ChromaPixType> predPels;
    PixType *pt = (PixType *)predPels.top;
    PixType *pl = (PixType *)predPels.left;

    const BitCounts &bc = *m_currFrame->bitCount;
    const TxbBitCounts &cbc = bc.txb[ get_q_ctx(m_lumaQp) ][txSize][PLANE_TYPE_UV];
    const int32_t miRow = (m_ctbPelY >> 3) + y4;//(y4 >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + x4;//(x4 >> 1);
    const int32_t haveTop_ = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft_  = (miCol > m_tileBorders.colStart);
    const int32_t miColEnd = m_tileBorders.colEnd;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
    const uint16_t *skipBits = bc.skip[ GetCtxSkip(above, left) ];
    const int32_t log2w = 3 - depth;

    const int32_t txw = tx_size_wide_unit[txSize];
    const int32_t txwpx = 4 << txSize;
    const int32_t txhpx = 4 << txSize;

    const BlockSize bszLuma = GetSbType(depth, partition);
    const int32_t bh = block_size_high_8x8[bszLuma];
    const int32_t bw = block_size_wide_8x8[bszLuma];
    const int32_t distToBottomEdge = (m_par->miRows - bh - miRow) * 8;
    const int32_t distToRightEdge = (m_par->miCols - bw - miCol) * 8;

    bool  is_cfl_allowed = bh <= 4 && bw <= 4;
    const uint16_t *intraModeUvBits = bc.intraModeUvAV1[is_cfl_allowed][mi->mode];

    const int32_t hpx = block_size_high[bsz];
    const int32_t wpx = hpx;

    PredMode bestMode = DC_PRED;
    int32_t bestDelta = 0;

    int32_t x = 0;
    int32_t y = 0;
    const TxSize txSizeMax = max_txsize_lookup[bsz];
    const int32_t txwMax = tx_size_wide_unit[txSizeMax];
    const int32_t txwpxMax = 4 << txSizeMax;
    const int32_t txhpxMax = txwpxMax;
    int32_t haveRight = (miCol + ((x + txwMax) /*>> 1*/)) < miColEnd;
    int32_t haveTopRight = haveTop_ && haveRight && HaveTopRight(log2w + 1, miRow, miCol, txSizeMax + 1, y, x);
    // Distance between the right edge of this prediction block to the frame right edge
    int32_t xr = (distToRightEdge >> 1) + (wpx - (x<<2) - txwpxMax);
    // Distance between the bottom edge of this prediction block to the frame bottom edge
    int32_t yd = (distToBottomEdge >> 1) + (hpx - (y<<2) - txhpxMax);
    int32_t haveBottom = yd > 0;
    int32_t haveBottomLeft = haveLeft_ && haveBottom && HaveBottomLeft(log2w + 1, miRow, miCol, txSizeMax + 1, y, x);
    int32_t pixTop = haveTop_ ? std::min(txwpxMax, xr + txwpxMax) : 0;
    int32_t pixTopRight = haveTopRight ? std::min(txwpxMax, xr) : 0;
    int32_t pixLeft = haveLeft_ ? std::min(txhpxMax, yd + txhpxMax) : 0;
    int32_t pixBottomLeft = haveBottomLeft ? std::min(txhpxMax, yd) : 0;

    AV1PP::GetPredPelsAV1<PLANE_TYPE_UV>((ChromaPixType *)recSb, m_pitchRecChroma>>1, predPels.top, predPels.left, widthSb,
                                         haveTop_, haveLeft_, pixTopRight, pixBottomLeft);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll +  0 * size * 2, widthSb * 2, log2w, haveLeft_, haveTop_, DC_PRED, 0, 0, 0);
    int32_t bestSse = AV1PP::sse_p64_pw(srcSb, m_predIntraAll, log2w + 1, log2w);
    CostType bestCost = bestSse + m_rdLambda * intraModeUvBits[DC_PRED];

    for (PredMode mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++) {
        //if (mode == SMOOTH_H_PRED || mode == SMOOTH_V_PRED)
        //    continue;
        AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll + mode * size * 2, widthSb * 2, log2w, 0, 0, mode, 0, 0, 0);
        int32_t sse = AV1PP::sse_p64_pw(srcSb, m_predIntraAll + mode * size * 2, log2w + 1, log2w);
        if (sse > bestCost) continue;
        int32_t bitCost = intraModeUvBits[mode];

        if (av1_is_directional_mode(mode)) {
            const int32_t max_angle_delta = MAX_ANGLE_DELTA;
            const int32_t delta = 0;
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

    PredMode oldBest = bestMode;
    //predict CFL from best luma prediction
    alignas(32) PixType predCFL[32 * 32 * 2];
    if (m_par->cflFlag && bestLuma && is_cfl_allowed) { //CFL allowed
        alignas(32) uint16_t chromaBuf[32 * 32];
        alignas(32) int16_t chromaAc[32 * 32];
        const int width = bw << 2;
        const int height = bh << 2;
        const int sz = width * height;

        //subsample luma to chroma
#if 0
        cfl_luma_subsampling_420_lbd_c((uint8_t*)bestLuma, lumaStride, chromaBuf, width<<1, height<<1); //luma size
        //cfl_luma_subsampling_420_lbd_avx2((uint8_t*)bestLuma, lumaStride, chromaBuf, width << 1, height << 1);
#else
        if (sizeof(PixType) == 1)
            AV1PP::cfl_subsample_420_u8_fptr_arr[max_txsize_lookup[bszLuma]]((const uint8_t*)bestLuma, lumaStride, chromaBuf);
        else
            AV1PP::cfl_subsample_420_u16_fptr_arr[max_txsize_lookup[bszLuma]]((const uint16_t*)bestLuma, lumaStride, chromaBuf);
#endif
#if 0
        int num_pel_log2 = get_msb(sz);
        int round_offset = ((sz) >> 1);
        subtract_average_c(chromaBuf, chromaAc, width, height, round_offset, num_pel_log2);
        //subtract_average_avx2(chromaBuf, chromaAc, width, height, round_offset, num_pel_log2);
#else
        AV1PP::cfl_subtract_average_fptr_arr[txSize](chromaBuf, chromaAc);
#endif

        int best_alpha_u = 0;
        int best_alpha_v = 0;
        int best_sign = 0;

        //find max abs ac value to reduce number of test cases
        //max width, height = 16 for chroma
        const int maxPix = (256 << m_par->bitDepthChromaShift) - 1;
        const CostType modeCost =  m_rdLambda * intraModeUvBits[UV_CFL_PRED];

        CostType bestCostUV[2] = { FLT_MAX, FLT_MAX };
        int bestAlphaUV[2] = {0,0};
        int bestSignUV[2] = { CFL_SIGN_ZERO, CFL_SIGN_ZERO };

        //First find best value for alpha = 0
        int costU, costV;
        AV1PP::sse_p64_pw_uv(srcSb, m_predIntraAll + bestMode * size * 2, log2w + 1, log2w, costU, costV);
        bestCostUV[0] = costU + m_rdLambda * bc.cflCost[0][0][0];
        bestCostUV[1] = costV + m_rdLambda * bc.cflCost[0][1][0];

        for (int alpha = 0; alpha < CFL_ALPHABET_SIZE; alpha++) {
            for (int sign = CFL_SIGN_NEG; sign < CFL_SIGNS; sign++) {

                int16_t alpha_q3 = (sign == CFL_SIGN_POS) ? alpha + 1 : -alpha - 1;

                AV1PP::cfl_predict_nv12_u8_fptr_arr[txSize](chromaAc, (uint8_t*)predCFL, widthSb<<1, m_predIntraAll[0], m_predIntraAll[1], alpha_q3, m_par->bitDepthChromaShift+8);
#if 0
                alignas(32) PixType predCFLT[32 * 32 * 2];
                int16_t* acPtr = chromaAc;
                PixType*  cfl = predCFLT;

                //prediction
                const PixType dcU = m_predIntraAll[0]; //dc the same for all pixels
                const PixType dcV = m_predIntraAll[1];

                for (int j = 0; j < height; j++) {
                    for (int i = 0; i < (width << 1); i += 2) {
                        const int scaled_luma_q6 = alpha_q3 * acPtr[i >> 1];
                        const int val = scaled_luma_q6 < 0 ? -(((-scaled_luma_q6) + (1 << 5)) >> 6) : (scaled_luma_q6 + (1 << 5)) >> 6;
                        cfl[i] = PixType(std::min(std::max(val + dcU, 0), maxPix)); //add U DC pred
                        cfl[i + 1] = PixType(std::min(std::max(val + dcV, 0), maxPix)); //add V DC pred
                    }
                    cfl += widthSb << 1;
                    acPtr += 32;
                }
#endif

                int costU, costV;
                AV1PP::sse_p64_pw_uv(srcSb, predCFL, log2w + 1, log2w, costU, costV);
                CostType cost[2];
                int joint_sign_u = sign * CFL_SIGNS + 0 - 1;
                int joint_sign_v = 0 * CFL_SIGNS + sign - 1;

                cost[0] = costU + m_rdLambda * bc.cflCost[joint_sign_u][0][alpha];
                cost[1] = costV + m_rdLambda * bc.cflCost[joint_sign_v][1][alpha];
                for (int i = 0; i < 2; i++) {
                    if (bestCostUV[i] > cost[i]) {
                        bestCostUV[i] = cost[i];
                        bestAlphaUV[i] = 0;
                        bestSignUV[i] = sign;
                    }
                }
            }
        }
        //exclude both sings = 0
        if (bestSignUV[0] != 0 && bestSignUV[1] != 0) {
            int joint_sign = bestSignUV[0] * CFL_SIGNS + bestSignUV[1] - 1;
            CostType cost = bestCostUV[0] + bestCostUV[1] + modeCost +
                m_rdLambda * (bc.cflCost[joint_sign][0][bestAlphaUV[0]] + bc.cflCost[joint_sign][1][bestAlphaUV[1]]);
            if (cost < bestCost) {
                CFL_params* cflp = m_currFrame->m_fenc->m_cfl + (miCol + miRow * (m_par->PicWidthInCtbs << 3));
                cflp->joint_sign = joint_sign;
                cflp->alpha_u = bestAlphaUV[0];
                cflp->alpha_v = bestAlphaUV[1];

                int16_t* acPtr = chromaAc;
                PixType*  cfl = predCFL;

                //prediction
                int16_t alpha_q3_u = bestSignUV[0] == CFL_SIGN_ZERO ? 0 : (bestSignUV[0] == CFL_SIGN_POS) ? bestAlphaUV[0] + 1 : -bestAlphaUV[0] - 1;
                int16_t alpha_q3_v = bestSignUV[1] == CFL_SIGN_ZERO ? 0 : (bestSignUV[1] == CFL_SIGN_POS) ? bestAlphaUV[1] + 1 : -bestAlphaUV[1] - 1;

                const PixType dcU = m_predIntraAll[0]; //dc the same for all pixels
                const PixType dcV = m_predIntraAll[1];

                for (int j = 0; j < height; j++) {
                    for (int i = 0; i < (width << 1); i += 2) {
                        const auto lumaAc = acPtr[i >> 1];
                        int scaled_luma_q6_u = alpha_q3_u * lumaAc;
                        const int val_u = scaled_luma_q6_u < 0 ? -(((-scaled_luma_q6_u) + (1 << 5)) >> 6) : (scaled_luma_q6_u + (1 << 5)) >> 6;
                        cfl[i] = PixType(std::min(std::max(val_u + dcU, 0), maxPix)); //add DC pred
                        const auto scaled_luma_q6_v = alpha_q3_v * lumaAc;
                        const int val_v = scaled_luma_q6_v < 0 ? -(((-scaled_luma_q6_v) + (1 << 5)) >> 6) : (scaled_luma_q6_v + (1 << 5)) >> 6;
                        cfl[i + 1] = PixType(std::min(std::max(val_v + dcV, 0), maxPix)); //add DC pred
                    }
                    cfl += widthSb << 1;
                    acPtr += 32;
                }
                int costU, costV;
                AV1PP::sse_p64_pw_uv(srcSb, predCFL, log2w + 1, log2w, costU, costV);

                bestCost = cost;
                bestDelta = 0;
                bestMode = UV_CFL_PRED;
                bestSse = costU + costV;
            }
        }
    }

    // second pass
    if (av1_is_directional_mode(oldBest)) {
        PredMode mode = oldBest;
        for (int32_t angleDelta = 1; angleDelta <= 3; angleDelta++) {
            for (int32_t i = 0; i < 2; i++) {
                const int32_t delta = (1 - 2 * i) * angleDelta;
                AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll, widthSb * 2, log2w, 0, 0, mode, delta, 0, 0);
                const int32_t sse = AV1PP::sse_p64_pw(srcSb, m_predIntraAll, log2w + 1, log2w);
                if (sse > bestCost) continue;
                int32_t bitCost = intraModeUvBits[mode];
                bitCost += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1, MAX_ANGLE_DELTA + delta);

                const CostType cost = sse + m_rdLambda * bitCost;
                if (bestCost > cost) {
                    bestCost = cost;
                    bestDelta = delta;
                    bestMode = oldBest;
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
    int32_t totalEob = 0;
    uint32_t coefBits = 0;
    int32_t haveTop = haveTop_;
    int16_t *coefOriginU = (int16_t *)vp9scratchpad.varTxCoefs;
    int16_t *coefOriginV = coefOriginU + 32 * 32;

    if (bestMode == UV_CFL_PRED) {
        PixType *rec = recSb;
        PixType *src = srcSb;
        const int height = bh << 2;

        int32_t blockIdx = h265_scan_r2z4[rasterIdx];
        int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
        offset >>= 2; // TODO: 4:2:0 only
        CoeffsType *residU = vp9scratchpad.diffU + offset;
        CoeffsType *residV = vp9scratchpad.diffV + offset;
        CoeffsType *coeffU = m_coeffWorkU + offset;
        CoeffsType *coeffV = m_coeffWorkV + offset;

        //copy prediction to reconstruct
        CopyNxM(predCFL, widthSb<<1, rec, m_pitchRecChroma, widthSb<<1, height);

        //int32_t sseZero = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);
        AV1PP::diff_nv12(src, SRC_PITCH, predCFL, widthSb<<1, residU, residV, width, width, txSize);

        int32_t txType = GetTxTypeAV1(PLANE_TYPE_UV, txSize, /*mi->modeUV*/bestMode, 0/*fake*/);

#ifdef ADAPTIVE_DEADZONE
        AV1PP::ftransform_av1(residU, coefOriginU, width, txSize, /*DCT_DCT*/txType);
        AV1PP::ftransform_av1(residV, coefOriginV, width, txSize, /*DCT_DCT*/txType);
        int32_t eobU = AV1PP::quant(coefOriginU, coeffU, m_aqparamUv[0], txSize);
        int32_t eobV = AV1PP::quant(coefOriginV, coeffV, m_aqparamUv[1], txSize);
        if (eobU) {
            AV1PP::dequant(coeffU, residU, m_aqparamUv[0], txSize, sizeof(PixType) == 1 ? 8 : 10);
            adaptDz(coefOriginU, residU, m_aqparamUv[0], txSize, &m_roundFAdjUv[0][0], eobU);
        }
        if (eobV) {
            AV1PP::dequant(coeffV, residV, m_aqparamUv[1], txSize, sizeof(PixType) == 1 ? 8 : 10);
            adaptDz(coefOriginV, residV, m_aqparamUv[1], txSize, &m_roundFAdjUv[1][0], eobV);
        }
#else
        AV1PP::ftransform_av1(residU, residU, width, txSize, /*DCT_DCT*/txType);
        AV1PP::ftransform_av1(residV, residV, width, txSize, /*DCT_DCT*/txType);
        int32_t eobU = AV1PP::quant_dequant(residU, coeffU, m_aqparamUv[0], txSize);
        int32_t eobV = AV1PP::quant_dequant(residV, coeffV, m_aqparamUv[1], txSize);
#endif

        uint8_t *actxU = m_contexts.aboveNonzero[1] + x4 + x;
        uint8_t *actxV = m_contexts.aboveNonzero[2] + x4 + x;
        uint8_t *lctxU = m_contexts.leftNonzero[1] + y4 + y;
        uint8_t *lctxV = m_contexts.leftNonzero[2] + y4 + y;
        const int32_t txbSkipCtxU = GetTxbSkipCtx(bsz, txSize, 1, actxU, lctxU);
        const int32_t txbSkipCtxV = GetTxbSkipCtx(bsz, txSize, 1, actxV, lctxV);

        uint32_t txBits = 0;
        if (eobU) {
            AV1PP::itransform_av1(residU, residU, width, txSize, txType, sizeof(PixType) == 1 ? 8 : 10);
            int32_t culLevel;
            txBits += EstimateCoefsAv1(cbc, txbSkipCtxU, eobU, coeffU, &culLevel);
            SetCulLevel(actxU, lctxU, (uint8_t)culLevel, txSize);
        }
        else {
            txBits += cbc.txbSkip[txbSkipCtxU][1];
            SetCulLevel(actxU, lctxU, 0, txSize);
        }
        if (eobV) {
            AV1PP::itransform_av1(residV, residV, width, txSize, txType, sizeof(PixType) == 1 ? 8 : 10);
            int32_t culLevel;
            txBits += EstimateCoefsAv1(cbc, txbSkipCtxV, eobV, coeffV, &culLevel);
            SetCulLevel(actxV, lctxV, (uint8_t)culLevel, txSize);
        }
        else {
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

    }
    else
    {
        for (y = 0; y < num4x4h; y += step) {
            PixType *rec = recSb + (y << 2) * m_pitchRecLuma;
            PixType *src = srcSb + (y << 2) * SRC_PITCH;
            int32_t haveLeft = haveLeft_;
            for (x = 0; x < num4x4w; x += step, rec += width << 1, src += width << 1) {
                assert(rasterIdx + (y << 1) * 16 + (x << 1) < 256);
                int32_t blockIdx = h265_scan_r2z4[rasterIdx + (y << 1) * 16 + (x << 1)];
                int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
                offset >>= 2; // TODO: 4:2:0 only
                CoeffsType *residU = vp9scratchpad.diffU + offset;
                CoeffsType *residV = vp9scratchpad.diffV + offset;
                CoeffsType *coeffU = m_coeffWorkU + offset;
                CoeffsType *coeffV = m_coeffWorkV + offset;

                haveRight = (miCol + ((x + txw)/* >> 1*/)) < miColEnd;
                haveTopRight = haveTop && haveRight && HaveTopRight(log2w + 1, miRow, miCol, txSize + 1, y, x);
                // Distance between the right edge of this prediction block to the frame right edge
                xr = (distToRightEdge >> 1) + (wpx - (x << 2) - txwpx);
                // Distance between the bottom edge of this prediction block to the frame bottom edge
                yd = (distToBottomEdge >> 1) + (hpx - (y << 2) - txhpx);
                haveBottom = yd > 0;
                haveBottomLeft = haveLeft && haveBottom && HaveBottomLeft(log2w + 1, miRow, miCol, txSize + 1, y, x);
                pixTop = haveTop ? std::min(txwpx, xr + txwpx) : 0;
                pixLeft = haveLeft ? std::min(txhpx, yd + txhpx) : 0;
                pixTopRight = haveTopRight ? std::min(txwpx, xr) : 0;
                pixBottomLeft = haveBottomLeft ? std::min(txhpx, yd) : 0;
                AV1PP::GetPredPelsAV1<PLANE_TYPE_UV>((ChromaPixType*)rec, m_pitchRecChroma >> 1, predPels.top, predPels.left, width, haveTop, haveLeft, pixTopRight, pixBottomLeft);

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

                //int32_t sseZero = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);
                AV1PP::diff_nv12(src, SRC_PITCH, rec, m_pitchRecChroma, residU, residV, width, width, txSize);

                int32_t txType = GetTxTypeAV1(PLANE_TYPE_UV, txSize, /*mi->modeUV*/bestMode, 0/*fake*/);

#ifdef ADAPTIVE_DEADZONE
                AV1PP::ftransform_av1(residU, coefOriginU, width, txSize, /*DCT_DCT*/txType);
                AV1PP::ftransform_av1(residV, coefOriginV, width, txSize, /*DCT_DCT*/txType);
                int32_t eobU = AV1PP::quant(coefOriginU, coeffU, m_aqparamUv[0], txSize);
                int32_t eobV = AV1PP::quant(coefOriginV, coeffV, m_aqparamUv[1], txSize);
                if (eobU) {
                    AV1PP::dequant(coeffU, residU, m_aqparamUv[0], txSize, sizeof(PixType) == 1 ? 8 : 10);
                    adaptDz(coefOriginU, residU, m_aqparamUv[0], txSize, &m_roundFAdjUv[0][0], eobU);
                }
                if (eobV) {
                    AV1PP::dequant(coeffV, residV, m_aqparamUv[1], txSize, sizeof(PixType) == 1 ? 8 : 10);
                    adaptDz(coefOriginV, residV, m_aqparamUv[1], txSize, &m_roundFAdjUv[1][0], eobV);
                }
#else
                AV1PP::ftransform_av1(residU, residU, width, txSize, /*DCT_DCT*/txType);
                AV1PP::ftransform_av1(residV, residV, width, txSize, /*DCT_DCT*/txType);
                int32_t eobU = AV1PP::quant_dequant(residU, coeffU, m_aqparamUv[0], txSize);
                int32_t eobV = AV1PP::quant_dequant(residV, coeffV, m_aqparamUv[1], txSize);
#endif

                uint8_t *actxU = m_contexts.aboveNonzero[1] + x4 + x;
                uint8_t *actxV = m_contexts.aboveNonzero[2] + x4 + x;
                uint8_t *lctxU = m_contexts.leftNonzero[1] + y4 + y;
                uint8_t *lctxV = m_contexts.leftNonzero[2] + y4 + y;
                const int32_t txbSkipCtxU = GetTxbSkipCtx(bsz, txSize, 1, actxU, lctxU);
                const int32_t txbSkipCtxV = GetTxbSkipCtx(bsz, txSize, 1, actxV, lctxV);

                uint32_t txBits = 0;
                if (eobU) {
                    AV1PP::itransform_av1(residU, residU, width, txSize, txType, sizeof(PixType) == 1 ? 8 : 10);
                    int32_t culLevel;
                    txBits += EstimateCoefsAv1(cbc, txbSkipCtxU, eobU, coeffU, &culLevel);
                    SetCulLevel(actxU, lctxU, (uint8_t)culLevel, txSize);
                }
                else {
                    txBits += cbc.txbSkip[txbSkipCtxU][1];
                    SetCulLevel(actxU, lctxU, 0, txSize);
                }
                if (eobV) {
                    AV1PP::itransform_av1(residV, residV, width, txSize, txType, sizeof(PixType) == 1 ? 8 : 10);
                    int32_t culLevel;
                    txBits += EstimateCoefsAv1(cbc, txbSkipCtxV, eobV, coeffV, &culLevel);
                    SetCulLevel(actxV, lctxV, (uint8_t)culLevel, txSize);
                }
                else {
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

#if CHROMA_PALETTE
    const int32_t allow_screen_content_tool_palette = m_currFrame->m_allowPalette && m_currFrame->m_isRef;
    PaletteInfo *pi = &m_currFrame->m_fenc->m_Palette8x8[miCol + miRow * m_par->miPitch];
    pi->palette_size_uv = 0;
    uint8_t *map_src{ nullptr };

    uint8_t palette[2 * MAX_PALETTE];
    uint16_t palette_size = 0;
    uint32_t palette_bits = 0;
    uint32_t map_bits_est = 0;
    CostType palette_cost = FLT_MAX;
    bool     palette_has_dist = false;

    if (mi->sbType >= BLOCK_8X8
        && block_size_wide[mi->sbType] <= 32
        && block_size_high[mi->sbType] <= 32
        && allow_screen_content_tool_palette// && m_currFrame->IsIntra()
        //&& pi->palette_size_y > 1 && mi->mode == DC_PRED  //use palette only when Luma has palette
        ) {

        // CHECK Palette Mode
        int32_t sbx = (x4 << 2) >> m_par->subsamplingX;
        int32_t sby = (y4 << 2) >> m_par->subsamplingY;
        const int sbh = bh << 2;
        const int sbw = bw << 2;

        uint8_t *color_map = &m_paletteMapUV[depth][sby*MAX_CU_SIZE + sbx];
        map_src = color_map;

        // CHECK Palette Mode
        palette_size = GetColorCountUV(srcSb, sbw, sbw, palette, m_par->bitDepthChroma, m_rdLambda, pi, color_map, map_bits_est, palette_has_dist);

        if (palette_size > 1 && palette_size <= MAX_PALETTE && !palette_has_dist) {
            const int32_t width = sbw;

            uint32_t ctxBS = num_pels_log2_lookup[mi->sbType] - num_pels_log2_lookup[BLOCK_8X8];

            uint8_t cache[2 * MAX_PALETTE];
            uint8_t colors_xmit[MAX_PALETTE];
            palette_bits += bc.HasPaletteUV[pi->palette_size_y > 0][palette_size > 0];
            palette_bits += bc.PaletteSizeUV[ctxBS][palette_size - 2];

            //U channel
            uint32_t cache_size = GetPaletteCache(miRow, false, miCol, cache);
            uint32_t index_bits = 0;
            uint32_t colors_xmit_size = GetPaletteCacheIndicesCost(palette, palette_size, cache, cache_size, colors_xmit, index_bits);
            palette_bits += index_bits * 512; // cache index used unused bits
            palette_bits += 512 * GetPaletteDeltaCost(colors_xmit, colors_xmit_size, m_par->bitDepthChroma, 1);
            // V channel palette color cost.
            int zeroCount = 0, minBits = 0;

            int bits = GetPaletteDeltaBitsV(&palette[MAX_PALETTE], palette_size, m_par->bitDepthChroma, &zeroCount, &minBits);
            const int bitsDelta = 2 + m_par->bitDepthChroma + (bits + 1) * (palette_size - 1) - zeroCount;
            const int bitsRaw = m_par->bitDepthChroma * palette_size;
            palette_bits += (1 + std::min(bitsDelta, bitsRaw)) * 512;

            //uint32_t map_bits = GetPaletteTokensCost(bc, srcSb, width, width, palette, palette_size, color_map);
            palette_bits += map_bits_est;

            palette_cost = m_rdLambda * (intraModeUvBits[DC_PRED] + palette_bits) + pi->sseUV; // +sse when distortion

            if (cost > 0.75*palette_cost && cost < 1.25*palette_cost) {
                // get better estimate
                palette_bits -= map_bits_est;
                uint32_t map_bits = GetPaletteTokensCostUV(bc, srcSb, width, width, palette, palette_size, map_src);
                palette_bits += map_bits;
                palette_cost = m_rdLambda * (intraModeUvBits[DC_PRED] + palette_bits) + pi->sseUV;
            }
        }
    }

    if (cost >= palette_cost && palette_size > 0 && !palette_has_dist) {
        const int sbh = bh << 2;
        const int sbw = bw << 2;
        PixType *rec = recSb;
        PixType *src = srcSb;

        pi->palette_size_uv = (uint8_t)palette_size;
        pi->palette_bits += palette_bits;
        for (int k = 0; k < palette_size; k++) {
            pi->palette_u[k] = palette[k];
            pi->palette_v[k] = palette[MAX_PALETTE + k];
        }

        int32_t blockIdx = h265_scan_r2z4[rasterIdx];
        int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
        offset >>= 2; // TODO: 4:2:0 only
        CoeffsType *residU = vp9scratchpad.diffU + offset;
        CoeffsType *residV = vp9scratchpad.diffV + offset;
        CoeffsType *coeffU = m_coeffWorkU + offset;
        CoeffsType *coeffV = m_coeffWorkV + offset;


        const int32_t yc = (miRow << 3) >> m_currFrame->m_par->subsamplingY;
        const int32_t xc = (miCol << 3) >> m_currFrame->m_par->subsamplingX;

        uint8_t *map_dst = &m_currFrame->m_fenc->m_ColorMapUV[yc*m_currFrame->m_fenc->m_ColorMapUVPitch + xc];
        CopyNxM_unaligned(map_src, MAX_CU_SIZE, map_dst, m_currFrame->m_fenc->m_ColorMapUVPitch, sbw, sbh);

        uint8_t *actxU = m_contexts.aboveNonzero[1] + x4 + x;
        uint8_t *actxV = m_contexts.aboveNonzero[2] + x4 + x;
        uint8_t *lctxU = m_contexts.leftNonzero[1] + y4 + y;
        uint8_t *lctxV = m_contexts.leftNonzero[2] + y4 + y;
        const int32_t txbSkipCtxU = GetTxbSkipCtx(bsz, txSize, 1, actxU, lctxU);
        const int32_t txbSkipCtxV = GetTxbSkipCtx(bsz, txSize, 1, actxV, lctxV);

        int32_t eobU = 0, eobV = 0;
        uint32_t txBits = 0;
        int32_t txSse = 0;

        if (pi->palette_size_uv == pi->true_colors_uv && !palette_has_dist) {

            CopyNxM_unaligned(src, SRC_PITCH, rec, m_pitchRecChroma, width << 1, width);

            txBits += cbc.txbSkip[txbSkipCtxU][1];
            SetCulLevel(actxU, lctxU, 0, txSize);
            txBits += cbc.txbSkip[txbSkipCtxV][1];
            SetCulLevel(actxV, lctxV, 0, txSize);
            txSse = 0;
            ZeroCoeffs(coeffU, 16 << (txSize << 1)); // zero coeffs
            ZeroCoeffs(coeffV, 16 << (txSize << 1)); // zero coeffs
        }
        else {
            // Palette Prediction
            for (int32_t i = 0; i < width; i++) {
                for (int32_t j = 0; j < width; j++) {
                    int off = i * m_pitchRecChroma + (j << 1);
                    int idx = map_src[i * 64 + j];
                    rec[off] = (uint8_t)pi->palette_u[idx];
                    rec[off + 1] = (uint8_t)pi->palette_v[idx];
                }
            }

            AV1PP::diff_nv12(src, SRC_PITCH, rec, m_pitchRecChroma, residU, residV, width, width, txSize);

            int32_t txType = GetTxTypeAV1(PLANE_TYPE_UV, txSize, /*mi->modeUV*/bestMode, 0/*fake*/);

#ifdef ADAPTIVE_DEADZONE
            AV1PP::ftransform_av1(residU, coefOriginU, width, txSize, /*DCT_DCT*/txType);
            AV1PP::ftransform_av1(residV, coefOriginV, width, txSize, /*DCT_DCT*/txType);
            int32_t eobU = AV1PP::quant(coefOriginU, coeffU, m_aqparamUv[0], txSize);
            int32_t eobV = AV1PP::quant(coefOriginV, coeffV, m_aqparamUv[1], txSize);
            if (eobU) {
                AV1PP::dequant(coeffU, residU, m_aqparamUv[0], txSize, sizeof(PixType) == 1 ? 8 : 10);
                adaptDz(coefOriginU, residU, m_aqparamUv[0], txSize, &m_roundFAdjUv[0][0], eobU);
            }
            if (eobV) {
                AV1PP::dequant(coeffV, residV, m_aqparamUv[1], txSize, sizeof(PixType) == 1 ? 8 : 10);
                adaptDz(coefOriginV, residV, m_aqparamUv[1], txSize, &m_roundFAdjUv[1][0], eobV);
            }
#else
            AV1PP::ftransform_av1(residU, residU, width, txSize, /*DCT_DCT*/txType);
            AV1PP::ftransform_av1(residV, residV, width, txSize, /*DCT_DCT*/txType);
            int32_t eobU = AV1PP::quant_dequant(residU, coeffU, m_aqparamUv[0], txSize);
            int32_t eobV = AV1PP::quant_dequant(residV, coeffV, m_aqparamUv[1], txSize);
#endif

            if (eobU) {
                AV1PP::itransform_av1(residU, residU, width, txSize, txType, sizeof(PixType) == 1 ? 8 : 10);
                int32_t culLevel;
                txBits += EstimateCoefsAv1(cbc, txbSkipCtxU, eobU, coeffU, &culLevel);
                SetCulLevel(actxU, lctxU, (uint8_t)culLevel, txSize);
            }
            else {
                txBits += cbc.txbSkip[txbSkipCtxU][1];
                SetCulLevel(actxU, lctxU, 0, txSize);
            }
            if (eobV) {
                AV1PP::itransform_av1(residV, residV, width, txSize, txType, sizeof(PixType) == 1 ? 8 : 10);
                int32_t culLevel;
                txBits += EstimateCoefsAv1(cbc, txbSkipCtxV, eobV, coeffV, &culLevel);
                SetCulLevel(actxV, lctxV, (uint8_t)culLevel, txSize);
            }
            else {
                txBits += cbc.txbSkip[txbSkipCtxV][1];
                SetCulLevel(actxV, lctxV, 0, txSize);
            }

            if (eobU | eobV) {
                CoeffsType *residU_ = (eobU ? residU : coeffU); // if cbf==0 coeffWork contains zeroes
                CoeffsType *residV_ = (eobV ? residV : coeffV); // if cbf==0 coeffWork contains zeroes
                AV1PP::adds_nv12(rec, m_pitchRecChroma, rec, m_pitchRecChroma, residU_, residV_, width, width);
            }
            txSse = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);
        }

        totalEob = eobU;
        totalEob += eobV;
        coefBits = txBits;
        sse = txSse;
        modeBits = palette_bits;

        bestMode = DC_PRED;
    }
#endif


    fprintf_trace_cost(stderr, "intra chroma: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d "
        "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, absPartIdx, bsz, bestMode,
        sse, modeBits, totalEob, coefBits, cost);

    int32_t skip = mi->skip && totalEob==0;

    mi->modeUV = bestMode;
    mi->skip = (uint8_t)skip;
#if 0
    const int32_t num4x4wLuma = block_size_wide_4x4[mi->sbType];
    const int32_t num4x4hLuma = block_size_high_4x4[mi->sbType];
    PropagateSubPart(mi, m_par->miPitch, num4x4wLuma >> 1, num4x4hLuma >> 1);
#endif
    PropagateSubPart(mi, m_par->miPitch, num4x4w, num4x4h);

    RdCost rd;
    rd.sse = sse;
    rd.eob = totalEob;
    rd.modeBits = modeBits;
    rd.coefBits = coefBits;
    return rd;
}

template int32_t AV1CU<uint8_t>::CheckIntra(int32_t absPartIdx, int32_t depth, PartitionType partition, uint32_t &bits);
template RdCost AV1CU<uint8_t>::CheckIntraChromaNonRdAv1(int32_t, int32_t, uint8_t, const uint8_t*, int);
template uint64_t AV1CU<uint8_t>::CheckIntra64x64(PredMode *bestIntraMode);
template uint32_t AV1CU<uint8_t>::GetPaletteCache(uint32_t miRow, bool isLuma, uint32_t miCol, uint8_t *cache);

#if ENABLE_10BIT
template void AV1CU<uint16_t>::CheckIntra(int32_t absPartIdx, int32_t depth, PartitionType partition);
template RdCost AV1CU<uint16_t>::CheckIntraChromaNonRdAv1(int32_t, int32_t, uint8_t, const uint16_t*, int);
template uint64_t AV1CU<uint16_t>::CheckIntra64x64(PredMode *bestIntraMode);
template uint32_t AV1CU<uint16_t>::GetPaletteCache(uint32_t miRow, bool isLuma, uint32_t miCol, uint16_t *cache);
#endif


template <typename PixType>
RdCost AV1CU<PixType>::CheckIntraChromaNonRdAv1_10bit(int32_t absPartIdx, int32_t depth, PartitionType partition, const uint16_t* bestLuma, int lumaStride)
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
    const TxSize txSize = std::min(TxSize(TX_32X32), TxSize(3 - depth));
    const int32_t x4 = x4Luma >> m_par->subsamplingX;
    const int32_t y4 = y4Luma >> m_par->subsamplingY;
    const int32_t sseShift = m_par->bitDepthChromaShift << 1;

    uint16_t *recSb = m_uvRec10 + ((x4 * 2 + y4 * m_pitchRecChroma10) << LOG2_MIN_TU_SIZE);
    uint16_t *srcSb = m_uvSrc10 + ((x4 * 2 + y4 * SRC_PITCH) << LOG2_MIN_TU_SIZE);

    typedef typename ChromaUVPixType<uint16_t>::type ChromaPixType;
    AV1PP::IntraPredPels<ChromaPixType> predPels;
    uint16_t *pt = (uint16_t *)predPels.top;
    uint16_t *pl = (uint16_t *)predPels.left;

    const BitCounts &bc = *m_currFrame->bitCount;
    const TxbBitCounts &cbc = bc.txb[get_q_ctx(m_lumaQp)][txSize][PLANE_TYPE_UV];
    const int32_t miRow = (m_ctbPelY >> 3) + y4;//(y4 >> 1);
    const int32_t miCol = (m_ctbPelX >> 3) + x4;//(x4 >> 1);



    const int32_t haveTop_ = (miRow > m_tileBorders.rowStart);
    const int32_t haveLeft_ = (miCol > m_tileBorders.colStart);
    const int32_t miColEnd = m_tileBorders.colEnd;
    const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
    const ModeInfo *left = GetLeft(mi, miCol, m_tileBorders.colStart);
    const uint16_t *skipBits = bc.skip[GetCtxSkip(above, left)];
    const int32_t log2w = 3 - depth;

    const int32_t txw = tx_size_wide_unit[txSize];
    const int32_t txwpx = 4 << txSize;
    const int32_t txhpx = 4 << txSize;

    const BlockSize bszLuma = GetSbType(depth, partition);
    const int32_t bh = block_size_high_8x8[bszLuma];
    const int32_t bw = block_size_wide_8x8[bszLuma];
    const int32_t distToBottomEdge = (m_par->miRows - bh - miRow) * 8;
    const int32_t distToRightEdge = (m_par->miCols - bw - miCol) * 8;

    bool  is_cfl_allowed = bh <= 4 && bw <= 4;
    const uint16_t *intraModeUvBits = bc.intraModeUvAV1[is_cfl_allowed][mi->mode];

    const int32_t hpx = block_size_high[bsz];
    const int32_t wpx = hpx;

    PredMode bestMode = mi->modeUV;//D207_PRED;
    int32_t bestDelta = mi->angle_delta_uv - MAX_ANGLE_DELTA;//0;

    int32_t x = 0;
    int32_t y = 0;
    const TxSize txSizeMax = max_txsize_lookup[bsz];
    const int32_t txwMax = tx_size_wide_unit[txSizeMax];
    const int32_t txwpxMax = 4 << txSizeMax;
    const int32_t txhpxMax = txwpxMax;
    int32_t haveRight = (miCol + ((x + txwMax) /*>> 1*/)) < miColEnd;
    int32_t haveTopRight = haveTop_ && haveRight && HaveTopRight(log2w + 1, miRow, miCol, txSizeMax + 1, y, x);
    // Distance between the right edge of this prediction block to the frame right edge
    int32_t xr = (distToRightEdge >> 1) + (wpx - (x << 2) - txwpxMax);
    // Distance between the bottom edge of this prediction block to the frame bottom edge
    int32_t yd = (distToBottomEdge >> 1) + (hpx - (y << 2) - txhpxMax);
    int32_t haveBottom = yd > 0;
    int32_t haveBottomLeft = haveLeft_ && haveBottom && HaveBottomLeft(log2w + 1, miRow, miCol, txSizeMax + 1, y, x);
    int32_t pixTop = haveTop_ ? std::min(txwpxMax, xr + txwpxMax) : 0;
    int32_t pixTopRight = haveTopRight ? std::min(txwpxMax, xr) : 0;
    int32_t pixLeft = haveLeft_ ? std::min(txhpxMax, yd + txhpxMax) : 0;
    int32_t pixBottomLeft = haveBottomLeft ? std::min(txhpxMax, yd) : 0;

    AV1PP::GetPredPelsAV1<PLANE_TYPE_UV>((ChromaPixType *)recSb, m_pitchRecChroma >> 1, predPels.top, predPels.left, widthSb, haveTop_, haveLeft_, pixTopRight, pixBottomLeft);
    AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll10bit + 0 * size * 2, widthSb * 2, log2w, haveLeft_, haveTop_, DC_PRED, 0, 0, 0);

    int32_t bestSse = 0;    // AV1PP::sse_p64_pw(srcSb, m_predIntraAll10bit, log2w + 1, log2w);
    CostType bestCost = 0;  // bestSse + m_rdLambda * intraModeUvBits[DC_PRED];

#if 0
    for (PredMode mode = DC_PRED + 1; mode < AV1_INTRA_MODES; mode++)
    {
        //if (mode == SMOOTH_H_PRED || mode == SMOOTH_V_PRED)
        //    continue;
        AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll10bit + mode * size * 2, widthSb * 2, log2w, 0, 0, mode, 0, 0, 0);
        int32_t sse = AV1PP::sse_p64_pw(srcSb, m_predIntraAll10bit + mode * size * 2, log2w + 1, log2w);
        if (sse > bestCost) continue;
        int32_t bitCost = intraModeUvBits[mode];

        if (av1_is_directional_mode(mode)) {
            const int32_t max_angle_delta = MAX_ANGLE_DELTA;
            const int32_t delta = 0;
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
#endif
    PredMode oldBest = bestMode;
#if 0
    //predict CFL from best luma prediction
    alignas(32) PixType predCFL[32 * 32 * 2];
    if (m_par->cflFlag && bestLuma && is_cfl_allowed) { //CFL allowed
        alignas(32) uint16_t chromaBuf[32 * 32];
        alignas(32) int16_t chromaAc[32 * 32];
        const int width = bw << 2;
        const int height = bh << 2;
        const int sz = width * height;

        //subsample luma to chroma
#if 0
        cfl_luma_subsampling_420_lbd_c((uint8_t*)bestLuma, lumaStride, chromaBuf, width << 1, height << 1); //luma size
        //cfl_luma_subsampling_420_lbd_avx2((uint8_t*)bestLuma, lumaStride, chromaBuf, width << 1, height << 1);
#else
        if (sizeof(PixType) == 1)
            AV1PP::cfl_subsample_420_u8_fptr_arr[max_txsize_lookup[bszLuma]]((const uint8_t*)bestLuma, lumaStride, chromaBuf);
        else
            AV1PP::cfl_subsample_420_u16_fptr_arr[max_txsize_lookup[bszLuma]]((const uint16_t*)bestLuma, lumaStride, chromaBuf);
#endif
#if 0
        int num_pel_log2 = get_msb(sz);
        int round_offset = ((sz) >> 1);
        subtract_average_c(chromaBuf, chromaAc, width, height, round_offset, num_pel_log2);
        //subtract_average_avx2(chromaBuf, chromaAc, width, height, round_offset, num_pel_log2);
#else
        AV1PP::cfl_subtract_average_fptr_arr[txSize](chromaBuf, chromaAc);
#endif

        int best_alpha_u = 0;
        int best_alpha_v = 0;
        int best_sign = 0;

        //find max abs ac value to reduce number of test cases
        //max width, height = 16 for chroma
        const int maxPix = (256 << m_par->bitDepthChromaShift) - 1;
        const CostType modeCost = m_rdLambda * intraModeUvBits[UV_CFL_PRED];

        CostType bestCostUV[2] = { FLT_MAX, FLT_MAX };
        int bestAlphaUV[2] = { 0,0 };
        int bestSignUV[2] = { CFL_SIGN_ZERO, CFL_SIGN_ZERO };

        //First find best value for alpha = 0
        int costU, costV;
        AV1PP::sse_p64_pw_uv(srcSb, m_predIntraAll + bestMode * size * 2, log2w + 1, log2w, costU, costV);
        bestCostUV[0] = costU + m_rdLambda * bc.cflCost[0][0][0];
        bestCostUV[1] = costV + m_rdLambda * bc.cflCost[0][1][0];

        for (int alpha = 0; alpha < CFL_ALPHABET_SIZE; alpha++) {
            for (int sign = CFL_SIGN_NEG; sign < CFL_SIGNS; sign++) {

                int16_t alpha_q3 = (sign == CFL_SIGN_POS) ? alpha + 1 : -alpha - 1;

                AV1PP::cfl_predict_nv12_u8_fptr_arr[txSize](chromaAc, (uint8_t*)predCFL, widthSb << 1, m_predIntraAll[0], m_predIntraAll[1], alpha_q3, m_par->bitDepthChromaShift + 8);
#if 0
                alignas(32) PixType predCFLT[32 * 32 * 2];
                int16_t* acPtr = chromaAc;
                PixType*  cfl = predCFLT;

                //prediction
                const PixType dcU = m_predIntraAll[0]; //dc the same for all pixels
                const PixType dcV = m_predIntraAll[1];

                for (int j = 0; j < height; j++) {
                    for (int i = 0; i < (width << 1); i += 2) {
                        const int scaled_luma_q6 = alpha_q3 * acPtr[i >> 1];
                        const int val = scaled_luma_q6 < 0 ? -(((-scaled_luma_q6) + (1 << 5)) >> 6) : (scaled_luma_q6 + (1 << 5)) >> 6;
                        cfl[i] = PixType(std::min(std::max(val + dcU, 0), maxPix)); //add U DC pred
                        cfl[i + 1] = PixType(std::min(std::max(val + dcV, 0), maxPix)); //add V DC pred
                    }
                    cfl += widthSb << 1;
                    acPtr += 32;
                }
#endif

                int costU, costV;
                AV1PP::sse_p64_pw_uv(srcSb, predCFL, log2w + 1, log2w, costU, costV);
                CostType cost[2];
                int joint_sign_u = sign * CFL_SIGNS + 0 - 1;
                int joint_sign_v = 0 * CFL_SIGNS + sign - 1;

                cost[0] = costU + m_rdLambda * bc.cflCost[joint_sign_u][0][alpha];
                cost[1] = costV + m_rdLambda * bc.cflCost[joint_sign_v][1][alpha];
                for (int i = 0; i < 2; i++) {
                    if (bestCostUV[i] > cost[i]) {
                        bestCostUV[i] = cost[i];
                        bestAlphaUV[i] = 0;
                        bestSignUV[i] = sign;
                    }
                }
            }
        }
        //exclude both sings = 0
        if (bestSignUV[0] != 0 && bestSignUV[1] != 0) {
            int joint_sign = bestSignUV[0] * CFL_SIGNS + bestSignUV[1] - 1;
            CostType cost = bestCostUV[0] + bestCostUV[1] + modeCost +
                m_rdLambda * (bc.cflCost[joint_sign][0][bestAlphaUV[0]] + bc.cflCost[joint_sign][1][bestAlphaUV[1]]);
            if (cost < bestCost) {
                CFL_params* cflp = m_currFrame->m_fenc->m_cfl + (miCol + miRow * (m_par->PicWidthInCtbs << 3));
                cflp->joint_sign = joint_sign;
                cflp->alpha_u = bestAlphaUV[0];
                cflp->alpha_v = bestAlphaUV[1];

                int16_t* acPtr = chromaAc;
                PixType*  cfl = predCFL;

                //prediction
                int16_t alpha_q3_u = bestSignUV[0] == CFL_SIGN_ZERO ? 0 : (bestSignUV[0] == CFL_SIGN_POS) ? bestAlphaUV[0] + 1 : -bestAlphaUV[0] - 1;
                int16_t alpha_q3_v = bestSignUV[1] == CFL_SIGN_ZERO ? 0 : (bestSignUV[1] == CFL_SIGN_POS) ? bestAlphaUV[1] + 1 : -bestAlphaUV[1] - 1;

                const PixType dcU = m_predIntraAll[0]; //dc the same for all pixels
                const PixType dcV = m_predIntraAll[1];

                for (int j = 0; j < height; j++) {
                    for (int i = 0; i < (width << 1); i += 2) {
                        const auto lumaAc = acPtr[i >> 1];
                        int scaled_luma_q6_u = alpha_q3_u * lumaAc;
                        const int val_u = scaled_luma_q6_u < 0 ? -(((-scaled_luma_q6_u) + (1 << 5)) >> 6) : (scaled_luma_q6_u + (1 << 5)) >> 6;
                        cfl[i] = PixType(std::min(std::max(val_u + dcU, 0), maxPix)); //add DC pred
                        const auto scaled_luma_q6_v = alpha_q3_v * lumaAc;
                        const int val_v = scaled_luma_q6_v < 0 ? -(((-scaled_luma_q6_v) + (1 << 5)) >> 6) : (scaled_luma_q6_v + (1 << 5)) >> 6;
                        cfl[i + 1] = PixType(std::min(std::max(val_v + dcV, 0), maxPix)); //add DC pred
                    }
                    cfl += widthSb << 1;
                    acPtr += 32;
                }
                int costU, costV;
                AV1PP::sse_p64_pw_uv(srcSb, predCFL, log2w + 1, log2w, costU, costV);

                bestCost = cost;
                bestDelta = 0;
                bestMode = UV_CFL_PRED;
                bestSse = costU + costV;
            }
        }
    }
#endif


#if 0
    // second pass
    if (av1_is_directional_mode(oldBest)) {
        PredMode mode = oldBest;
        for (int32_t angleDelta = 1; angleDelta <= 3; angleDelta++) {
            for (int32_t i = 0; i < 2; i++) {
                const int32_t delta = (1 - 2 * i) * angleDelta;
                AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll10bit, widthSb * 2, log2w, 0, 0, mode, delta, 0, 0);
                const int32_t sse = AV1PP::sse_p64_pw(srcSb, m_predIntraAll10bit, log2w + 1, log2w);
                if (sse > bestCost) continue;
                int32_t bitCost = intraModeUvBits[mode];
                bitCost += write_uniform_cost(2 * MAX_ANGLE_DELTA + 1, MAX_ANGLE_DELTA + delta);

                const CostType cost = sse + m_rdLambda * bitCost;
                if (bestCost > cost) {
                    bestCost = cost;
                    bestDelta = delta;
                    bestMode = oldBest;
                }
            }
        }
    }
#endif

    // recalc
    if (bestDelta)
        AV1PP::predict_intra_nv12_av1(pt, pl, m_predIntraAll10bit, widthSb * 2, log2w, 0, 0, bestMode, bestDelta, 0, 0);

    mi->angle_delta_uv = bestDelta + MAX_ANGLE_DELTA;

    const int32_t isDirectionalMode = av1_is_directional_mode(bestMode);
    const int32_t angle = mode_to_angle_map[bestMode] + (mi->angle_delta_uv - MAX_ANGLE_DELTA) * ANGLE_STEP;

    int32_t width = 4 << txSize;
    int32_t step = 1 << txSize;
    int32_t sse = 0;
    int32_t totalEob = 0;
    uint32_t coefBits = 0;
    int32_t haveTop = haveTop_;
    int16_t *coefOriginU = (int16_t *)vp9scratchpad.varTxCoefs;
    int16_t *coefOriginV = coefOriginU + 32 * 32;

#if 0
    if (bestMode == UV_CFL_PRED) {
        PixType *rec = recSb;
        PixType *src = srcSb;
        const int height = bh << 2;

        int32_t blockIdx = h265_scan_r2z4[rasterIdx];
        int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
        offset >>= 2; // TODO: 4:2:0 only
        CoeffsType *residU = vp9scratchpad.diffU + offset;
        CoeffsType *residV = vp9scratchpad.diffV + offset;
        CoeffsType *coeffU = m_coeffWorkU + offset;
        CoeffsType *coeffV = m_coeffWorkV + offset;

        //copy prediction to reconstruct
        CopyNxM(predCFL, widthSb << 1, rec, m_pitchRecChroma, widthSb << 1, height);

        //int32_t sseZero = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);
        AV1PP::diff_nv12(src, SRC_PITCH, predCFL, widthSb << 1, residU, residV, width, width, txSize);

        int32_t txType = GetTxTypeAV1(PLANE_TYPE_UV, txSize, /*mi->modeUV*/bestMode, 0/*fake*/);

#ifdef ADAPTIVE_DEADZONE
        AV1PP::ftransform_av1(residU, coefOriginU, width, txSize, /*DCT_DCT*/txType);
        AV1PP::ftransform_av1(residV, coefOriginV, width, txSize, /*DCT_DCT*/txType);
        int32_t eobU = AV1PP::quant(coefOriginU, coeffU, m_aqparamUv[0], txSize);
        int32_t eobV = AV1PP::quant(coefOriginV, coeffV, m_aqparamUv[1], txSize);
        if (eobU) {
            AV1PP::dequant(coeffU, residU, m_aqparamUv[0], txSize);
            adaptDz(coefOriginU, residU, m_aqparamUv[0], txSize, &m_roundFAdjUv[0][0], eobU);
        }
        if (eobV) {
            AV1PP::dequant(coeffV, residV, m_aqparamUv[1], txSize);
            adaptDz(coefOriginV, residV, m_aqparamUv[1], txSize, &m_roundFAdjUv[1][0], eobV);
        }
#else
        AV1PP::ftransform_av1(residU, residU, width, txSize, /*DCT_DCT*/txType);
        AV1PP::ftransform_av1(residV, residV, width, txSize, /*DCT_DCT*/txType);
        int32_t eobU = AV1PP::quant_dequant(residU, coeffU, m_aqparamUv[0], txSize);
        int32_t eobV = AV1PP::quant_dequant(residV, coeffV, m_aqparamUv[1], txSize);
#endif

        uint8_t *actxU = m_contexts.aboveNonzero[1] + x4 + x;
        uint8_t *actxV = m_contexts.aboveNonzero[2] + x4 + x;
        uint8_t *lctxU = m_contexts.leftNonzero[1] + y4 + y;
        uint8_t *lctxV = m_contexts.leftNonzero[2] + y4 + y;
        const int32_t txbSkipCtxU = GetTxbSkipCtx(bsz, txSize, 1, actxU, lctxU);
        const int32_t txbSkipCtxV = GetTxbSkipCtx(bsz, txSize, 1, actxV, lctxV);

        uint32_t txBits = 0;
        if (eobU) {
            AV1PP::itransform_av1(residU, residU, width, txSize, txType);
            int32_t culLevel;
            txBits += EstimateCoefsAv1(cbc, txbSkipCtxU, eobU, coeffU, &culLevel);
            SetCulLevel(actxU, lctxU, (uint8_t)culLevel, txSize);
        }
        else {
            txBits += cbc.txbSkip[txbSkipCtxU][1];
            SetCulLevel(actxU, lctxU, 0, txSize);
        }
        if (eobV) {
            AV1PP::itransform_av1(residV, residV, width, txSize, txType);
            int32_t culLevel;
            txBits += EstimateCoefsAv1(cbc, txbSkipCtxV, eobV, coeffV, &culLevel);
            SetCulLevel(actxV, lctxV, (uint8_t)culLevel, txSize);
        }
        else {
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

    }
    else
#endif
    {
        for (y = 0; y < num4x4h; y += step) {
            uint16_t *rec = recSb + (y << 2) * m_pitchRecLuma10;
            uint16_t *src = srcSb + (y << 2) * SRC_PITCH;
            int32_t haveLeft = haveLeft_;
            for (x = 0; x < num4x4w; x += step, rec += width << 1, src += width << 1) {
                assert(rasterIdx + (y << 1) * 16 + (x << 1) < 256);
                int32_t blockIdx = h265_scan_r2z4[rasterIdx + (y << 1) * 16 + (x << 1)];
                int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
                offset >>= 2; // TODO: 4:2:0 only
                CoeffsType *residU = vp9scratchpad.diffU + offset;
                CoeffsType *residV = vp9scratchpad.diffV + offset;
                CoeffsType *coeffU = m_coeffWorkU + offset;
                CoeffsType *coeffV = m_coeffWorkV + offset;

                haveRight = (miCol + ((x + txw)/* >> 1*/)) < miColEnd;
                haveTopRight = haveTop && haveRight && HaveTopRight(log2w + 1, miRow, miCol, txSize + 1, y, x);
                // Distance between the right edge of this prediction block to the frame right edge
                xr = (distToRightEdge >> 1) + (wpx - (x << 2) - txwpx);
                // Distance between the bottom edge of this prediction block to the frame bottom edge
                yd = (distToBottomEdge >> 1) + (hpx - (y << 2) - txhpx);
                haveBottom = yd > 0;
                haveBottomLeft = haveLeft && haveBottom && HaveBottomLeft(log2w + 1, miRow, miCol, txSize + 1, y, x);
                pixTop = haveTop ? std::min(txwpx, xr + txwpx) : 0;
                pixLeft = haveLeft ? std::min(txhpx, yd + txhpx) : 0;
                pixTopRight = haveTopRight ? std::min(txwpx, xr) : 0;
                pixBottomLeft = haveBottomLeft ? std::min(txhpx, yd) : 0;
                AV1PP::GetPredPelsAV1<PLANE_TYPE_UV>((ChromaPixType*)rec, m_pitchRecChroma10 >> 1, predPels.top, predPels.left, width, haveTop, haveLeft, pixTopRight, pixBottomLeft);

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
                AV1PP::predict_intra_nv12_av1(pt, pl, rec, m_pitchRecChroma10, txSize, haveLeft, haveTop, bestMode, delta, upTop, upLeft);

                //int32_t sseZero = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);
                AV1PP::diff_nv12(src, SRC_PITCH, rec, m_pitchRecChroma10, residU, residV, width, width, txSize);

                int32_t txType = GetTxTypeAV1(PLANE_TYPE_UV, txSize, /*mi->modeUV*/bestMode, 0/*fake*/);

#ifdef ADAPTIVE_DEADZONE
                AV1PP::ftransform_av1(residU, coefOriginU, width, txSize, /*DCT_DCT*/txType);
                AV1PP::ftransform_av1(residV, coefOriginV, width, txSize, /*DCT_DCT*/txType);


                //m_aqparamUv[0] = m_par->qparamUv[m_chromaQp];
                //m_aqparamUv[1] = m_par->qparamUv[m_chromaQp];

                int32_t eobU = AV1PP::quant(coefOriginU, coeffU, /*m_aqparamUv[0]*/m_par->qparamUv10[m_chromaQp], txSize);
                int32_t eobV = AV1PP::quant(coefOriginV, coeffV, /*m_aqparamUv[1]*/m_par->qparamUv10[m_chromaQp], txSize);
                if (eobU) {
                    AV1PP::dequant(coeffU, residU, /*m_aqparamUv[0]*/m_par->qparamUv10[m_chromaQp], txSize, 10);
                    adaptDz(coefOriginU, residU, m_aqparamUv[0], txSize, &m_roundFAdjUv[0][0], eobU);
                }
                if (eobV) {
                    AV1PP::dequant(coeffV, residV, /*m_aqparamUv[1]*/m_par->qparamUv10[m_chromaQp], txSize, 10);
                    adaptDz(coefOriginV, residV, m_aqparamUv[1], txSize, &m_roundFAdjUv[1][0], eobV);
                }
#else
                AV1PP::ftransform_av1(residU, residU, width, txSize, /*DCT_DCT*/txType);
                AV1PP::ftransform_av1(residV, residV, width, txSize, /*DCT_DCT*/txType);
                int32_t eobU = AV1PP::quant_dequant(residU, coeffU, m_aqparamUv[0], txSize);
                int32_t eobV = AV1PP::quant_dequant(residV, coeffV, m_aqparamUv[1], txSize);
#endif

                uint8_t *actxU = m_contexts.aboveNonzero[1] + x4 + x;
                uint8_t *actxV = m_contexts.aboveNonzero[2] + x4 + x;
                uint8_t *lctxU = m_contexts.leftNonzero[1] + y4 + y;
                uint8_t *lctxV = m_contexts.leftNonzero[2] + y4 + y;
                const int32_t txbSkipCtxU = GetTxbSkipCtx(bsz, txSize, 1, actxU, lctxU);
                const int32_t txbSkipCtxV = GetTxbSkipCtx(bsz, txSize, 1, actxV, lctxV);

                uint32_t txBits = 0;
                if (eobU) {
                    AV1PP::itransform_av1(residU, residU, width, txSize, txType, 10);
                    int32_t culLevel;
                    txBits += EstimateCoefsAv1(cbc, txbSkipCtxU, eobU, coeffU, &culLevel);
                    SetCulLevel(actxU, lctxU, (uint8_t)culLevel, txSize);
                }
                else {
                    txBits += cbc.txbSkip[txbSkipCtxU][1];
                    SetCulLevel(actxU, lctxU, 0, txSize);
                }
                if (eobV) {
                    AV1PP::itransform_av1(residV, residV, width, txSize, txType, 10);
                    int32_t culLevel;
                    txBits += EstimateCoefsAv1(cbc, txbSkipCtxV, eobV, coeffV, &culLevel);
                    SetCulLevel(actxV, lctxV, (uint8_t)culLevel, txSize);
                }
                else {
                    txBits += cbc.txbSkip[txbSkipCtxV][1];
                    SetCulLevel(actxV, lctxV, 0, txSize);
                }

                if (eobU | eobV) {
                    CoeffsType *residU_ = (eobU ? residU : coeffU); // if cbf==0 coeffWork contains zeroes
                    CoeffsType *residV_ = (eobV ? residV : coeffV); // if cbf==0 coeffWork contains zeroes
                    AV1PP::adds_nv12(rec, m_pitchRecChroma10, rec, m_pitchRecChroma10, residU_, residV_, width, width);
                }
                int32_t txSse = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma10, txSize + 1, txSize);

                totalEob += eobU;
                totalEob += eobV;
                coefBits += txBits;
                sse += txSse;
                haveLeft = 1;
            }
            haveTop = 1;
        }
    }

    uint32_t modeBits = intraModeUvBits[bestMode];

    if (av1_is_directional_mode(bestMode)) {
        int32_t delta = mi->angle_delta_uv - MAX_ANGLE_DELTA;
        int32_t max_angle_delta = MAX_ANGLE_DELTA;
        modeBits += write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
    }

    CostType cost = (mi->skip && totalEob == 0)
        ? sse + m_rdLambda * (modeBits + skipBits[1])
        : sse + m_rdLambda * (modeBits + skipBits[0] + coefBits);

#if 0 //CHROMA_PALETTE
    const int32_t allow_screen_content_tool_palette = m_currFrame->m_allowPalette && m_currFrame->m_isRef;
    PaletteInfo *pi = &m_currFrame->m_fenc->m_Palette8x8[miCol + miRow * m_par->miPitch];
    pi->palette_size_uv = 0;
    uint8_t *map_src{ nullptr };

    uint8_t palette[2 * MAX_PALETTE];
    uint16_t palette_size = 0;
    uint32_t palette_bits = 0;
    uint32_t map_bits_est = 0;
    CostType palette_cost = FLT_MAX;
    bool     palette_has_dist = false;

    if (mi->sbType >= BLOCK_8X8
        && block_size_wide[mi->sbType] <= 32
        && block_size_high[mi->sbType] <= 32
        && allow_screen_content_tool_palette// && m_currFrame->IsIntra()
        //&& pi->palette_size_y > 1 && mi->mode == DC_PRED  //use palette only when Luma has palette
        ) {

        // CHECK Palette Mode
        int32_t sbx = (x4 << 2) >> m_par->subsamplingX;
        int32_t sby = (y4 << 2) >> m_par->subsamplingY;
        const int sbh = bh << 2;
        const int sbw = bw << 2;

        uint8_t *color_map = &m_paletteMapUV[depth][sby*MAX_CU_SIZE + sbx];
        map_src = color_map;

        // CHECK Palette Mode
        palette_size = GetColorCountUV(srcSb, sbw, sbw, palette, m_par->bitDepthChroma, m_rdLambda, pi, color_map, map_bits_est, palette_has_dist);

        if (palette_size > 1 && palette_size <= MAX_PALETTE && !palette_has_dist) {
            const int32_t width = sbw;

            uint32_t ctxBS = num_pels_log2_lookup[mi->sbType] - num_pels_log2_lookup[BLOCK_8X8];

            uint8_t cache[2 * MAX_PALETTE];
            uint8_t colors_xmit[MAX_PALETTE];
            palette_bits += bc.HasPaletteUV[pi->palette_size_y > 0][palette_size > 0];
            palette_bits += bc.PaletteSizeUV[ctxBS][palette_size - 2];

            //U channel
            uint32_t cache_size = GetPaletteCache(miRow, false, miCol, cache);
            uint32_t index_bits = 0;
            uint32_t colors_xmit_size = GetPaletteCacheIndicesCost(palette, palette_size, cache, cache_size, colors_xmit, index_bits);
            palette_bits += index_bits * 512; // cache index used unused bits
            palette_bits += 512 * GetPaletteDeltaCost(colors_xmit, colors_xmit_size, m_par->bitDepthChroma, 1);
            // V channel palette color cost.
            int zeroCount = 0, minBits = 0;

            int bits = GetPaletteDeltaBitsV(&palette[MAX_PALETTE], palette_size, m_par->bitDepthChroma, &zeroCount, &minBits);
            const int bitsDelta = 2 + m_par->bitDepthChroma + (bits + 1) * (palette_size - 1) - zeroCount;
            const int bitsRaw = m_par->bitDepthChroma * palette_size;
            palette_bits += (1 + std::min(bitsDelta, bitsRaw)) * 512;

            //uint32_t map_bits = GetPaletteTokensCost(bc, srcSb, width, width, palette, palette_size, color_map);
            palette_bits += map_bits_est;

            palette_cost = m_rdLambda * (intraModeUvBits[DC_PRED] + palette_bits) + pi->sseUV; // +sse when distortion

            if (cost > 0.75*palette_cost && cost < 1.25*palette_cost) {
                // get better estimate
                palette_bits -= map_bits_est;
                uint32_t map_bits = GetPaletteTokensCostUV(bc, srcSb, width, width, palette, palette_size, map_src);
                palette_bits += map_bits;
                palette_cost = m_rdLambda * (intraModeUvBits[DC_PRED] + palette_bits) + pi->sseUV;
            }
        }
    }

    if (cost >= palette_cost && palette_size > 0 && !palette_has_dist) {
        const int sbh = bh << 2;
        const int sbw = bw << 2;
        PixType *rec = recSb;
        PixType *src = srcSb;

        pi->palette_size_uv = (uint8_t)palette_size;
        pi->palette_bits += palette_bits;
        for (int k = 0; k < palette_size; k++) {
            pi->palette_u[k] = palette[k];
            pi->palette_v[k] = palette[MAX_PALETTE + k];
        }

        int32_t blockIdx = h265_scan_r2z4[rasterIdx];
        int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
        offset >>= 2; // TODO: 4:2:0 only
        CoeffsType *residU = vp9scratchpad.diffU + offset;
        CoeffsType *residV = vp9scratchpad.diffV + offset;
        CoeffsType *coeffU = m_coeffWorkU + offset;
        CoeffsType *coeffV = m_coeffWorkV + offset;


        const int32_t yc = (miRow << 3) >> m_currFrame->m_par->subsamplingY;
        const int32_t xc = (miCol << 3) >> m_currFrame->m_par->subsamplingX;

        uint8_t *map_dst = &m_currFrame->m_fenc->m_ColorMapUV[yc*m_currFrame->m_fenc->m_ColorMapUVPitch + xc];
        CopyNxM_unaligned(map_src, MAX_CU_SIZE, map_dst, m_currFrame->m_fenc->m_ColorMapUVPitch, sbw, sbh);

        uint8_t *actxU = m_contexts.aboveNonzero[1] + x4 + x;
        uint8_t *actxV = m_contexts.aboveNonzero[2] + x4 + x;
        uint8_t *lctxU = m_contexts.leftNonzero[1] + y4 + y;
        uint8_t *lctxV = m_contexts.leftNonzero[2] + y4 + y;
        const int32_t txbSkipCtxU = GetTxbSkipCtx(bsz, txSize, 1, actxU, lctxU);
        const int32_t txbSkipCtxV = GetTxbSkipCtx(bsz, txSize, 1, actxV, lctxV);

        int32_t eobU = 0, eobV = 0;
        uint32_t txBits = 0;
        int32_t txSse = 0;

        if (pi->palette_size_uv == pi->true_colors_uv && !palette_has_dist) {

            CopyNxM_unaligned(src, SRC_PITCH, rec, m_pitchRecChroma, width << 1, width);

            txBits += cbc.txbSkip[txbSkipCtxU][1];
            SetCulLevel(actxU, lctxU, 0, txSize);
            txBits += cbc.txbSkip[txbSkipCtxV][1];
            SetCulLevel(actxV, lctxV, 0, txSize);
            txSse = 0;
            ZeroCoeffs(coeffU, 16 << (txSize << 1)); // zero coeffs
            ZeroCoeffs(coeffV, 16 << (txSize << 1)); // zero coeffs
        }
        else {
            // Palette Prediction
            for (int32_t i = 0; i < width; i++) {
                for (int32_t j = 0; j < width; j++) {
                    int off = i * m_pitchRecChroma + (j << 1);
                    int idx = map_src[i * 64 + j];
                    rec[off] = (uint8_t)pi->palette_u[idx];
                    rec[off + 1] = (uint8_t)pi->palette_v[idx];
                }
            }

            AV1PP::diff_nv12(src, SRC_PITCH, rec, m_pitchRecChroma, residU, residV, width, width, txSize);

            int32_t txType = GetTxTypeAV1(PLANE_TYPE_UV, txSize, /*mi->modeUV*/bestMode, 0/*fake*/);

#ifdef ADAPTIVE_DEADZONE
            AV1PP::ftransform_av1(residU, coefOriginU, width, txSize, /*DCT_DCT*/txType);
            AV1PP::ftransform_av1(residV, coefOriginV, width, txSize, /*DCT_DCT*/txType);
            int32_t eobU = AV1PP::quant(coefOriginU, coeffU, m_aqparamUv[0], txSize);
            int32_t eobV = AV1PP::quant(coefOriginV, coeffV, m_aqparamUv[1], txSize);
            if (eobU) {
                AV1PP::dequant(coeffU, residU, m_aqparamUv[0], txSize);
                adaptDz(coefOriginU, residU, m_aqparamUv[0], txSize, &m_roundFAdjUv[0][0], eobU);
            }
            if (eobV) {
                AV1PP::dequant(coeffV, residV, m_aqparamUv[1], txSize);
                adaptDz(coefOriginV, residV, m_aqparamUv[1], txSize, &m_roundFAdjUv[1][0], eobV);
            }
#else
            AV1PP::ftransform_av1(residU, residU, width, txSize, /*DCT_DCT*/txType);
            AV1PP::ftransform_av1(residV, residV, width, txSize, /*DCT_DCT*/txType);
            int32_t eobU = AV1PP::quant_dequant(residU, coeffU, m_aqparamUv[0], txSize);
            int32_t eobV = AV1PP::quant_dequant(residV, coeffV, m_aqparamUv[1], txSize);
#endif

            if (eobU) {
                AV1PP::itransform_av1(residU, residU, width, txSize, txType);
                int32_t culLevel;
                txBits += EstimateCoefsAv1(cbc, txbSkipCtxU, eobU, coeffU, &culLevel);
                SetCulLevel(actxU, lctxU, (uint8_t)culLevel, txSize);
            }
            else {
                txBits += cbc.txbSkip[txbSkipCtxU][1];
                SetCulLevel(actxU, lctxU, 0, txSize);
            }
            if (eobV) {
                AV1PP::itransform_av1(residV, residV, width, txSize, txType);
                int32_t culLevel;
                txBits += EstimateCoefsAv1(cbc, txbSkipCtxV, eobV, coeffV, &culLevel);
                SetCulLevel(actxV, lctxV, (uint8_t)culLevel, txSize);
            }
            else {
                txBits += cbc.txbSkip[txbSkipCtxV][1];
                SetCulLevel(actxV, lctxV, 0, txSize);
            }

            if (eobU | eobV) {
                CoeffsType *residU_ = (eobU ? residU : coeffU); // if cbf==0 coeffWork contains zeroes
                CoeffsType *residV_ = (eobV ? residV : coeffV); // if cbf==0 coeffWork contains zeroes
                AV1PP::adds_nv12(rec, m_pitchRecChroma, rec, m_pitchRecChroma, residU_, residV_, width, width);
            }
            txSse = AV1PP::sse(src, SRC_PITCH, rec, m_pitchRecChroma, txSize + 1, txSize);
        }

        totalEob = eobU;
        totalEob += eobV;
        coefBits = txBits;
        sse = txSse;
        modeBits = palette_bits;

        bestMode = DC_PRED;
    }
#endif


    fprintf_trace_cost(stderr, "intra chroma: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d "
        "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, absPartIdx, bsz, bestMode,
        sse, modeBits, totalEob, coefBits, cost);

    int32_t skip = mi->skip && totalEob == 0;

    mi->modeUV = bestMode;
    mi->skip = (uint8_t)skip;
#if 0
    const int32_t num4x4wLuma = block_size_wide_4x4[mi->sbType];
    const int32_t num4x4hLuma = block_size_high_4x4[mi->sbType];
    PropagateSubPart(mi, m_par->miPitch, num4x4wLuma >> 1, num4x4hLuma >> 1);
#endif
    PropagateSubPart(mi, m_par->miPitch, num4x4w, num4x4h);

    RdCost rd;
    rd.sse = sse;
    rd.eob = totalEob;
    rd.modeBits = modeBits;
    rd.coefBits = coefBits;
    return rd;
}

template RdCost AV1CU<uint8_t>::CheckIntraChromaNonRdAv1_10bit(int32_t, int32_t, uint8_t, const uint16_t*, int);

} // namespace

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
