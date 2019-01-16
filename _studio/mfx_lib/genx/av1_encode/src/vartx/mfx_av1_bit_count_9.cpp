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

#include "mfx_av1_defs.h"
#include "mfx_av1_bit_count.h"
#include "mfx_av1_probabilities.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_frame.h"

using namespace H265Enc;

namespace {
    template <int32_t B>
    inline uint16_t Bits(vpx_prob prob) { assert(prob<256); return prob2bits[B ? 256-prob : prob]; }
    template <int32_t B0>
    inline uint16_t Bits(const vpx_prob *probs) { return Bits<B0>(*probs); }
    template <int32_t B0, int32_t B1>
    inline uint16_t Bits(const vpx_prob* probs) { return Bits<B0>(*probs) + Bits<B1>(probs+1); }
    template <int32_t B0, int32_t B1, int32_t B2>
    inline uint16_t Bits(const vpx_prob* probs) { return Bits<B0>(*probs) + Bits<B1,B2>(probs+1); }
    template <int32_t B0, int32_t B1, int32_t B2, int32_t B3>
    inline uint16_t Bits(const vpx_prob* probs) { return Bits<B0>(*probs) + Bits<B1,B2,B3>(probs+1); }
    template <int32_t B0, int32_t B1, int32_t B2, int32_t B3, int32_t B4>
    inline uint16_t Bits(const vpx_prob* probs) { return Bits<B0>(*probs) + Bits<B1,B2,B3,B4>(probs+1); }
    template <int32_t B0, int32_t B1, int32_t B2, int32_t B3, int32_t B4, int32_t B5>
    inline uint16_t Bits(const vpx_prob* probs) { return Bits<B0>(*probs) + Bits<B1,B2,B3,B4,B5>(probs+1); }
    template <int32_t B0, int32_t B1, int32_t B2, int32_t B3, int32_t B4, int32_t B5, int32_t B6>
    inline uint16_t Bits(const vpx_prob* probs) { return Bits<B0>(*probs) + Bits<B1,B2,B3,B4,B5,B6>(probs+1); }
    template <int32_t B0, int32_t B1, int32_t B2, int32_t B3, int32_t B4, int32_t B5, int32_t B6, int32_t B7>
    inline uint16_t Bits(const vpx_prob* probs) { return Bits<B0>(*probs) + Bits<B1,B2,B3,B4,B5,B6,B7>(probs+1); }

    inline uint16_t Bits(int32_t b, vpx_prob prob) { return b ? Bits<1>(prob) : Bits<0>(prob); }

    // for syntax where probs are not contiguous
    template <int32_t B0, int32_t B1>
    inline uint16_t Bits(vpx_prob p0, vpx_prob p1) { return Bits<B0>(p0) + Bits<B1>(p1); }
    template <int32_t B0, int32_t B1, int32_t B2>
    inline uint16_t Bits(vpx_prob p0, vpx_prob p1, vpx_prob p2) { return Bits<B0>(p0) + Bits<B1,B2>(p1,p2); }
    template <int32_t B0, int32_t B1, int32_t B2, int32_t B3>
    inline uint16_t Bits(vpx_prob p0, vpx_prob p1, vpx_prob p2, vpx_prob p3) { return Bits<B0>(p0) + Bits<B1,B2,B3>(p1,p2,p3); }
    template <int32_t B0, int32_t B1, int32_t B2, int32_t B3, int32_t B4>
    inline uint16_t Bits(vpx_prob p0, vpx_prob p1, vpx_prob p2, vpx_prob p3, vpx_prob p4) { return Bits<B0>(p0) + Bits<B1,B2,B3,B4>(p1,p2,p3,p4); }

    // Extra bits probabilities are not adaptable, so table is pre-calculated offline:
    // algorithm:
    //      for (int32_t token = DCT_VAL_CATEGORY1; token <= DCT_VAL_CATEGORY6; token++) {
    //          int32_t cat = extra_bits[token].cat;
    //          int32_t numExtra = extra_bits[token].numExtra;
    //          for (int32_t e = 0; e < numExtra; e++) {
    //              default_bitcost_extra[token-DCT_VAL_CATEGORY1][e][0] = Bits<0>(default_cat_probs[cat][e]);
    //              default_bitcost_extra[token-DCT_VAL_CATEGORY1][e][1] = Bits<1>(default_cat_probs[cat][e]);
    //          }
    //      }
    const uint32_t default_bitcost_extra[NUM_TOKENS-DCT_VAL_CATEGORY1][14][2] = {
        { { 352,  717} },
        { { 324,  764}, { 420,  617} },
        { { 289,  832}, { 405,  637}, { 446,  585} },
        { { 277,  859}, { 371,  687}, { 446,  585}, { 473,  554} },
        { { 260,  897}, { 361,  702}, { 441,  591}, { 478,  547}, { 501,  524} },
        { {   6, 3584}, {   6, 3584}, {   6, 3584}, {  12, 3072}, {  20, 2659}, {  38, 2201}, {  79, 1689},
          { 197, 1072}, { 273,  868}, { 380,  673}, { 446,  585}, { 484,  541}, { 501,  524}, { 506,  518}
        },
    };

    // Extra bits probabilities are not adaptable, so table is pre-calculated offline:
    // algorithm:
    //      for (int32_t cat = 1; cat < 6; cat++) {
    //          int16_t numExtra = extra_bits[cat+4].numExtra;
    //          int16_t minCoef = extra_bits[cat+4].coef;
    //          for (int32_t rem = 0; rem < (1 << numExtra); rem++) {
    //              for (int32_t e = 0; e < numExtra; e++) {
    //                  int32_t bit = (rem >> (numExtra-1-e)) & 1;
    //                  default_bitcost_extraTotal[minCoef + rem] += Bits(bit, default_cat_probs[cat][e]);
    //              }
    //          }
    //      }
    // Sum of extra bits cost for first 10 tokens (67 coefs)
    const uint32_t default_bitcost_extraTotal[CAT6_MIN_VAL] = {
            0,     0,     0,     0,     0,   352,   717,   744,
          941,  1184,  1381,  1140,  1279,  1372,  1511,  1683,
         1822,  1915,  2054,  1567,  1648,  1706,  1787,  1883,
         1964,  2022,  2103,  2149,  2230,  2288,  2369,  2465,
         2546,  2604,  2685,  2041,  2064,  2110,  2133,  2191,
         2214,  2260,  2283,  2382,  2405,  2451,  2474,  2532,
         2555,  2601,  2624,  2678,  2701,  2747,  2770,  2828,
         2851,  2897,  2920,  3019,  3042,  3088,  3111,  3169,
         3192,  3238,  3261
    };
};

#define av1_cost_zero(prob) (prob2bits[prob])
#define av1_cost_bit(prob, bit) av1_cost_zero((bit) ? 256 - (prob) : (prob))
static void cost(uint16_t *costs, int8_t* tree, const vpx_prob *probs, int i, int c)
{
    const vpx_prob prob = probs[i / 2];
    int b;
    assert(prob != 0);
    for (b = 0; b <= 1; ++b) {
        const int cc = c + av1_cost_bit(prob, b);
        const int8_t ii = tree[i + b];
        if (ii <= 0)
            costs[-ii] = cc;
        else
            cost(costs, tree, probs, ii, cc);
    }
}

void av1_cost_tokens(uint16_t *costs, const vpx_prob *probs, int8_t* tree)
{
    cost(costs, tree, probs, 0, 0);
}

#define av1_cost_zero(prob) (prob2bits[prob])
typedef unsigned long long uint64_t;

static inline vpx_prob get_prob(unsigned int num, unsigned int den)
{
    assert(den != 0);
    {
        const int p = (int)(((uint64_t)num * 256 + (den >> 1)) / den);
        // (p > 255) ? 255 : (p < 1) ? 1 : p;
        const int clipped_prob = p | ((255 - p) >> 23) | (p == 0);
        return (vpx_prob)clipped_prob;
    }
}

#define CDF_PROB_BITS 15
#define CDF_PROB_TOP (1 << CDF_PROB_BITS)

// The factor to scale from cost in bits to cost in av1_prob_cost units.
#define AV1_PROB_COST_SHIFT 9

// Cost of coding an n bit literal, using 128 (i.e. 50%) probability
// for each bit.
#define av1_cost_literal(n) ((n) * (1 << AV1_PROB_COST_SHIFT))

// Calculate the cost of a symbol with probability p15 / 2^15
static inline int av1_cost_symbol(aom_cdf_prob p15) {
  assert(0 < p15 && p15 < CDF_PROB_TOP);
  const int shift = CDF_PROB_BITS - 1 - BSR(p15);
  return av1_cost_zero(get_prob(p15 << shift, CDF_PROB_TOP)) + av1_cost_literal(shift);
}

template <typename CostType>
void av1_cost_tokens_from_cdf(CostType *costs, const aom_cdf_prob *cdf, const int *inv_map)
{
    aom_cdf_prob prev_cdf = 0;
    for (int i = 0;; ++i) {
        const aom_cdf_prob p15 = AOM_ICDF(cdf[i]) - prev_cdf;
        prev_cdf = AOM_ICDF(cdf[i]);

        if (inv_map)
            costs[inv_map[i]] = av1_cost_symbol(p15);
        else
            costs[i] = av1_cost_symbol(p15);

        // Stop once we reach the end of the CDF
        if (cdf[i] == AOM_ICDF(CDF_PROB_TOP)) break;
    }
}

void H265Enc::EstimateBits(BitCounts &bits, int32_t intraOnly, EnumCodecType codecType)
{
    const vpx_prob (*partition_probs)[PARTITION_TYPES-1] = codecType == CODEC_VP9 ?
        (intraOnly ? kf_partition_probs : default_partition_probs):
        (default_partition_probs);
    Zero(bits.vp9Partition);
    Zero(bits.av1Partition);
    if (codecType == CODEC_VP9) {
        enum {HAS_COLS=1, HAS_ROWS=2};
        for (int32_t ctx = 0; ctx < VP9_PARTITION_CONTEXTS; ctx++) {
            const vpx_prob *probs = partition_probs[ctx];
            bits.vp9Partition[HAS_ROWS+HAS_COLS][ctx][PARTITION_NONE]  = Bits<0>(probs);
            bits.vp9Partition[HAS_ROWS+HAS_COLS][ctx][PARTITION_HORZ]  = Bits<1,0>(probs);
            bits.vp9Partition[HAS_ROWS+HAS_COLS][ctx][PARTITION_VERT]  = Bits<1,1,0>(probs);
            bits.vp9Partition[HAS_ROWS+HAS_COLS][ctx][PARTITION_SPLIT] = Bits<1,1,1>(probs);
            bits.vp9Partition[HAS_COLS][ctx][PARTITION_HORZ]  = Bits<0>(probs[1]);
            bits.vp9Partition[HAS_COLS][ctx][PARTITION_SPLIT] = Bits<1>(probs[1]);
            bits.vp9Partition[HAS_ROWS][ctx][PARTITION_VERT]  = Bits<0>(probs[2]);
            bits.vp9Partition[HAS_ROWS][ctx][PARTITION_SPLIT] = Bits<1>(probs[2]);
        }
    } else {
        enum {HAS_COLS=1, HAS_ROWS=2};
        for (int32_t ctx = 0; ctx < AV1_PARTITION_CONTEXTS; ctx++) {
            av1_cost_tokens_from_cdf(bits.av1Partition[HAS_ROWS + HAS_COLS][ctx], default_partition_cdf[ctx], NULL);

            aom_cdf_prob tmpCdf[2];
            uint32_t tmpCost[2] = {};
            partition_gather_vert_alike(tmpCdf, default_partition_cdf[ctx], BLOCK_64X64);
            av1_cost_tokens_from_cdf(tmpCost, tmpCdf, NULL);
            bits.av1Partition[HAS_COLS][ctx][PARTITION_HORZ]  = tmpCost[0];
            bits.av1Partition[HAS_COLS][ctx][PARTITION_SPLIT] = tmpCost[1];

            partition_gather_horz_alike(tmpCdf, default_partition_cdf[ctx], BLOCK_64X64);
            av1_cost_tokens_from_cdf(tmpCost, tmpCdf, NULL);
            bits.av1Partition[HAS_ROWS][ctx][PARTITION_VERT]  = tmpCost[0];
            bits.av1Partition[HAS_ROWS][ctx][PARTITION_SPLIT] = tmpCost[1];
        }
    }

    for (int32_t ctx = 0; ctx < SKIP_CONTEXTS; ctx++) {
        vpx_prob prob = default_skip_prob[ctx];
        bits.skip[ctx][0] = Bits<0>(prob);
        bits.skip[ctx][1] = Bits<1>(prob);
    }
    Zero(bits.txSize);
    for (int32_t ctx = 0; ctx < TX_SIZE_CONTEXTS; ctx++) {
        const vpx_prob *probs32 = default_tx_probs[TX_32X32][ctx];
        const vpx_prob *probs16 = default_tx_probs[TX_16X16][ctx];
        const vpx_prob *probs8  = default_tx_probs[TX_8X8][ctx];
        bits.txSize[TX_32X32][ctx][TX_4X4]   = Bits<0>(probs32);
        bits.txSize[TX_32X32][ctx][TX_8X8]   = Bits<1,0>(probs32);
        bits.txSize[TX_32X32][ctx][TX_16X16] = Bits<1,1,0>(probs32);
        bits.txSize[TX_32X32][ctx][TX_32X32] = Bits<1,1,1>(probs32);
        bits.txSize[TX_16X16][ctx][TX_4X4]   = Bits<0>(probs16);
        bits.txSize[TX_16X16][ctx][TX_8X8]   = Bits<1,0>(probs16);
        bits.txSize[TX_16X16][ctx][TX_16X16] = Bits<1,1>(probs16);
        bits.txSize[TX_8X8][ctx][TX_4X4]     = Bits<0>(probs8);
        bits.txSize[TX_8X8][ctx][TX_8X8]     = Bits<1>(probs8);
    }
    for (int32_t i = 0; i < TXFM_PARTITION_CONTEXTS; ++i) {
        av1_cost_tokens_from_cdf(bits.txfm_partition_cost[i], default_txfm_partition_cdf[i], NULL);
    }
    if (codecType == CODEC_VP9) {
        for (int32_t modeU = 0; modeU < INTRA_MODES; modeU++) {
            for (int32_t modeL = 0; modeL < INTRA_MODES; modeL++) {
                const vpx_prob *probs = vp9_kf_y_mode_probs[modeU][modeL];
                bits.kfIntraMode[modeU][modeL][DC_PRED]   = Bits<0>(probs);
                bits.kfIntraMode[modeU][modeL][TM_PRED]   = Bits<1,0>(probs);
                bits.kfIntraMode[modeU][modeL][V_PRED]    = Bits<1,1,0>(probs);
                bits.kfIntraMode[modeU][modeL][H_PRED]    = Bits<1,1,1,0,0>(probs);
                bits.kfIntraMode[modeU][modeL][D135_PRED] = Bits<1,1,1,0,1,0>(probs);
                bits.kfIntraMode[modeU][modeL][D117_PRED] = Bits<1,1,1,0,1,1>(probs);
                bits.kfIntraMode[modeU][modeL][D45_PRED]  = Bits<1,1,1,1>(probs) + Bits<0>(probs+6);
                bits.kfIntraMode[modeU][modeL][D63_PRED]  = Bits<1,1,1,1>(probs) + Bits<1,0>(probs+6);
                bits.kfIntraMode[modeU][modeL][D153_PRED] = Bits<1,1,1,1>(probs) + Bits<1,1,0>(probs+6);
                bits.kfIntraMode[modeU][modeL][D207_PRED] = Bits<1,1,1,1>(probs) + Bits<1,1,1>(probs+6);
            }
        }
        for (int32_t ctx = 0; ctx < BLOCK_SIZE_GROUPS; ctx++) {
            const vpx_prob *probs = vp9_default_y_mode_probs[ctx];
            bits.intraMode[ctx][DC_PRED]   = Bits<0>(probs);
            bits.intraMode[ctx][TM_PRED]   = Bits<1,0>(probs);
            bits.intraMode[ctx][V_PRED]    = Bits<1,1,0>(probs);
            bits.intraMode[ctx][H_PRED]    = Bits<1,1,1,0,0>(probs);
            bits.intraMode[ctx][D135_PRED] = Bits<1,1,1,0,1,0>(probs);
            bits.intraMode[ctx][D117_PRED] = Bits<1,1,1,0,1,1>(probs);
            bits.intraMode[ctx][D45_PRED]  = Bits<1,1,1,1>(probs) + Bits<0>(probs+6);
            bits.intraMode[ctx][D63_PRED]  = Bits<1,1,1,1>(probs) + Bits<1,0>(probs+6);
            bits.intraMode[ctx][D153_PRED] = Bits<1,1,1,1>(probs) + Bits<1,1,0>(probs+6);
            bits.intraMode[ctx][D207_PRED] = Bits<1,1,1,1>(probs) + Bits<1,1,1>(probs+6);
        }
        const vpx_prob (*uv_mode_probs)[INTRA_MODES-1] = intraOnly ? kf_uv_mode_probs : vp9_default_uv_mode_probs;
        for (int32_t modeY = 0; modeY < INTRA_MODES; modeY++) {
            const vpx_prob *probs = uv_mode_probs[modeY];
            bits.intraModeUv[modeY][DC_PRED]   = Bits<0>(probs);
            bits.intraModeUv[modeY][TM_PRED]   = Bits<1,0>(probs);
            bits.intraModeUv[modeY][V_PRED]    = Bits<1,1,0>(probs);
            bits.intraModeUv[modeY][H_PRED]    = Bits<1,1,1,0,0>(probs);
            bits.intraModeUv[modeY][D135_PRED] = Bits<1,1,1,0,1,0>(probs);
            bits.intraModeUv[modeY][D117_PRED] = Bits<1,1,1,0,1,1>(probs);
            bits.intraModeUv[modeY][D45_PRED]  = Bits<1,1,1,1>(probs) + Bits<0>(probs+6);
            bits.intraModeUv[modeY][D63_PRED]  = Bits<1,1,1,1>(probs) + Bits<1,0>(probs+6);
            bits.intraModeUv[modeY][D153_PRED] = Bits<1,1,1,1>(probs) + Bits<1,1,0>(probs+6);
            bits.intraModeUv[modeY][D207_PRED] = Bits<1,1,1,1>(probs) + Bits<1,1,1>(probs+6);
        }
    } else {
        for (int i = 0; i < KF_MODE_CONTEXTS; ++i)
            for (int j = 0; j < KF_MODE_CONTEXTS; ++j)
                av1_cost_tokens_from_cdf(bits.kfIntraModeAV1[i][j], default_av1_kf_y_mode_cdf[i][j], NULL);
        for (int i = 0; i < BLOCK_SIZE_GROUPS; ++i)
            av1_cost_tokens_from_cdf(bits.intraModeAV1[i], default_if_y_mode_cdf[i], NULL);
        for (int i = 0; i < AV1_INTRA_MODES; ++i)
            av1_cost_tokens_from_cdf(bits.intraModeUvAV1[i], default_uv_mode_cdf[i], NULL);
    }

    for (int32_t ctx = 0; ctx < INTER_MODE_CONTEXTS; ctx++) {
        const vpx_prob *probs = default_inter_mode_probs[ctx];
        bits.interMode[ctx][ZEROMV-NEARESTMV]    = Bits<0>(probs);
        bits.interMode[ctx][NEARESTMV-NEARESTMV] = Bits<1,0>(probs);
        bits.interMode[ctx][NEARMV-NEARESTMV]    = Bits<1,1,0>(probs);
        bits.interMode[ctx][NEWMV-NEARESTMV]     = Bits<1,1,1>(probs);
    }
    if (codecType == CODEC_AV1) {
        for (int32_t ctx0 = 0; ctx0 < DRL_MODE_CONTEXTS; ctx0++) {
            for (int32_t ctx1 = 0; ctx1 < DRL_MODE_CONTEXTS; ctx1++) {
                const vpx_prob probs[2] = { default_drl_prob[ctx0], default_drl_prob[ctx1] };
                bits.drlIdx[ctx0][ctx1][0] = Bits<0>(probs);
                bits.drlIdx[ctx0][ctx1][1] = Bits<1,0>(probs);
                bits.drlIdx[ctx0][ctx1][2] = Bits<1,1>(probs);
            }
        }
        memcpy(bits.drlIdxNewMv[0], bits.drlIdx[3][3], 3 * sizeof(uint32_t));
        memcpy(bits.drlIdxNewMv[1], bits.drlIdx[2][3], 3 * sizeof(uint32_t));
        memcpy(bits.drlIdxNewMv[2], bits.drlIdx[0][2], 3 * sizeof(uint32_t));
        memcpy(bits.drlIdxNewMv[3], bits.drlIdx[0][0], 3 * sizeof(uint32_t));
        memcpy(bits.drlIdxNewMv[4], bits.drlIdx[0][0], 3 * sizeof(uint32_t));
        memcpy(bits.drlIdxNearMv[0], bits.drlIdx[3][3], 3 * sizeof(uint32_t));
        memcpy(bits.drlIdxNearMv[1], bits.drlIdx[3][3], 3 * sizeof(uint32_t));
        memcpy(bits.drlIdxNearMv[2], bits.drlIdx[2][3], 3 * sizeof(uint32_t));
        memcpy(bits.drlIdxNearMv[3], bits.drlIdx[0][2], 3 * sizeof(uint32_t));
        memcpy(bits.drlIdxNearMv[4], bits.drlIdx[0][0], 3 * sizeof(uint32_t));

        for (int32_t ctx = 0; ctx < NEWMV_MODE_CONTEXTS; ctx++) {
            bits.newMvBit[ctx][0] = Bits<0>(default_newmv_prob[ctx]);
            bits.newMvBit[ctx][1] = Bits<1>(default_newmv_prob[ctx]);
        }
        for (int32_t ctx = 0; ctx < GLOBALMV_MODE_CONTEXTS; ctx++) {
            bits.zeroMvBit[ctx][0] = Bits<0>(default_zeromv_prob[ctx]);
            bits.zeroMvBit[ctx][1] = Bits<1>(default_zeromv_prob[ctx]);
        }
        for (int32_t ctx = 0; ctx < REFMV_MODE_CONTEXTS; ctx++) {
            bits.refMvBit[ctx][0] = Bits<0>(default_refmv_prob[ctx]);
            bits.refMvBit[ctx][1] = Bits<1>(default_refmv_prob[ctx]);
        }
        for (int32_t ctx = 0; ctx < INTER_MODE_CONTEXTS; ctx++)
            av1_cost_tokens_from_cdf(bits.interCompMode[ctx], default_inter_compound_mode_cdf[ctx], nullptr);
    } else {
        Zero(bits.drlIdx);
        Zero(bits.newMvBit);
        Zero(bits.zeroMvBit);
        Zero(bits.refMvBit);
    }
    if (codecType == CODEC_AV1) {
        int8_t av1_switchable_interp_tree[] = {0,2,4,-2, -1, -3};
        for (int32_t ctx = 0; ctx < AV1_INTERP_FILTER_CONTEXTS; ctx++) {
            const vpx_prob *probs = default_interp_filter_probs_av1[ctx];
            av1_cost_tokens(bits.interpFilterAV1[ctx], probs, av1_switchable_interp_tree);
        }
    } else {
        for (int32_t ctx = 0; ctx < VP9_INTERP_FILTER_CONTEXTS; ctx++) {
            const vpx_prob *probs = default_interp_filter_probs_vp9[ctx];
            bits.interpFilterVP9[ctx][EIGHTTAP]        = Bits<0>(probs);
            bits.interpFilterVP9[ctx][EIGHTTAP_SMOOTH] = Bits<1,0>(probs);
            bits.interpFilterVP9[ctx][EIGHTTAP_SHARP]  = Bits<1,1>(probs);
        }
    }
    for (int32_t ctx = 0; ctx < IS_INTER_CONTEXTS; ctx++) {
        const vpx_prob *probs = &default_is_inter_prob[ctx];
        bits.isInter[ctx][0] = Bits<0>(probs);
        bits.isInter[ctx][1] = Bits<1>(probs);
    }
    {
        const vpx_prob *probs = default_mv_joint_probs;
        bits.mvJoint[MV_JOINT_ZERO]   = Bits<0>(probs);
        bits.mvJoint[MV_JOINT_HNZVZ]  = Bits<1,0>(probs);
        bits.mvJoint[MV_JOINT_HZVNZ]  = Bits<1,1,0>(probs);
        bits.mvJoint[MV_JOINT_HNZVNZ] = Bits<1,1,1>(probs);
    }
    for (int32_t comp = 0; comp < 2; comp++) {
        const vpx_prob *probs = &default_mv_sign_prob[comp];
        bits.mvSign[comp][0] = Bits<0>(probs);
        bits.mvSign[comp][1] = Bits<1>(probs);
    }
    for (int32_t comp = 0; comp < 2; comp++) {
        const vpx_prob *probs = default_mv_class_probs[comp];
        bits.mvClass[comp][MV_CLASS_0]  = Bits<0>(probs);
        bits.mvClass[comp][MV_CLASS_1]  = Bits<1,0>(probs);
        bits.mvClass[comp][MV_CLASS_2]  = Bits<1,1,0,0>(probs);
        bits.mvClass[comp][MV_CLASS_3]  = Bits<1,1,0,1>(probs);
        bits.mvClass[comp][MV_CLASS_4]  = Bits<1,1,1>(probs) + Bits<0,0>(probs+4);
        bits.mvClass[comp][MV_CLASS_5]  = Bits<1,1,1>(probs) + Bits<0,1>(probs+4);
        bits.mvClass[comp][MV_CLASS_6]  = Bits<1,1,1>(probs) + Bits<1>(probs+4) + Bits<0>(probs+6);
        bits.mvClass[comp][MV_CLASS_7]  = Bits<1,1,1>(probs) + Bits<1>(probs+4) + Bits<1,0>(probs+6) + Bits<0>(probs+8);
        bits.mvClass[comp][MV_CLASS_8]  = Bits<1,1,1>(probs) + Bits<1>(probs+4) + Bits<1,0>(probs+6) + Bits<1>(probs+8);
        bits.mvClass[comp][MV_CLASS_9]  = Bits<1,1,1>(probs) + Bits<1>(probs+4) + Bits<1,1>(probs+6) + Bits<0>(probs+9);
        bits.mvClass[comp][MV_CLASS_10] = Bits<1,1,1>(probs) + Bits<1>(probs+4) + Bits<1,1>(probs+6) + Bits<1>(probs+9);
    }
    for (int32_t comp = 0; comp < 2; comp++) {
        const vpx_prob *probs = &default_mv_class0_bit_prob[comp];
        bits.mvClass0Bit[comp][0]  = Bits<0>(probs);
        bits.mvClass0Bit[comp][1]  = Bits<1>(probs);
    }
    for (int32_t comp = 0; comp < 2; comp++) {
        for (int32_t i = 0; i < CLASS0_SIZE; i++) {
            const vpx_prob *probs = default_mv_class0_fr_probs[comp][i];
            bits.mvClass0Fr[comp][i][0]  = Bits<0>(probs);
            bits.mvClass0Fr[comp][i][1]  = Bits<1,0>(probs);
            bits.mvClass0Fr[comp][i][2]  = Bits<1,1,0>(probs);
            bits.mvClass0Fr[comp][i][3]  = Bits<1,1,1>(probs);
        }
    }
    for (int32_t comp = 0; comp < 2; comp++) {
        const vpx_prob *probs = &default_mv_class0_hp_prob[comp];
        bits.mvClass0Hp[comp][0]  = Bits<0>(probs);
        bits.mvClass0Hp[comp][1]  = Bits<1>(probs);
    }
    for (int32_t comp = 0; comp < 2; comp++) {
        for (int32_t i = 0; i < MV_OFFSET_BITS; i++) {
            const vpx_prob *probs = &default_mv_bits_prob[comp][i];
            bits.mvBits[comp][i][0]  = Bits<0>(probs);
            bits.mvBits[comp][i][1]  = Bits<1>(probs);
        }
    }
    for (int32_t comp = 0; comp < 2; comp++) {
        const vpx_prob *probs = default_mv_fr_probs[comp];
        bits.mvFr[comp][0]  = Bits<0>(probs);
        bits.mvFr[comp][1]  = Bits<1,0>(probs);
        bits.mvFr[comp][2]  = Bits<1,1,0>(probs);
        bits.mvFr[comp][3]  = Bits<1,1,1>(probs);
    }
    for (int32_t comp = 0; comp < 2; comp++) {
        const vpx_prob *probs = &default_mv_hp_prob[comp];
        bits.mvHp[comp][0]  = Bits<0>(probs);
        bits.mvHp[comp][1]  = Bits<1>(probs);
    }
    for (int32_t comp = 0; comp < 2; comp++) {
        for (int32_t diffmv = 0; diffmv < 16; diffmv++) {
            int32_t mv_class0_bit = diffmv >> 3;
            int32_t mv_class0_fr = (diffmv >> 1) & 3;
            int32_t mv_class0_hp = diffmv & 1;
            uint32_t cost = bits.mvClass0Bit[comp][mv_class0_bit]
                        + bits.mvClass0Fr[comp][mv_class0_bit][mv_class0_fr];
            bits.mvClass0[comp][0][diffmv] = cost;
            bits.mvClass0[comp][1][diffmv] = cost + bits.mvClass0Hp[comp][mv_class0_hp];
        }
    }
    for (int32_t ctx = 0; ctx < COMP_MODE_CONTEXTS; ctx++) {
        const vpx_prob *probs = &default_comp_mode_probs[ctx];
        bits.compMode[ctx][0]  = Bits<0>(probs);
        bits.compMode[ctx][1]  = Bits<1>(probs);
    }

    if (codecType == CODEC_AV1) {
        for (int32_t ctx = 0; ctx < COMP_REF_TYPE_CONTEXTS; ctx++) {
            const vpx_prob *probs = &default_comp_ref_type_probs[ctx];
            bits.av1CompRefType[ctx][0] = Bits<0>(probs);
            bits.av1CompRefType[ctx][1] = Bits<1>(probs);
        }
        for (int32_t ctx = 0; ctx < AV1_REF_CONTEXTS; ctx++) {
            for (int32_t i = 0; i < FWD_REFS - 1; i++) {
                const vpx_prob *probs = &av1_default_comp_ref_probs[ctx][i];
                bits.av1CompRef[ctx][i][0] = Bits<0>(probs);
                bits.av1CompRef[ctx][i][1] = Bits<1>(probs);
            }
            for (int32_t i = 0; i < BWD_REFS - 1; i++) {
                const vpx_prob *probs = &default_comp_bwdref_probs[ctx][i];
                bits.av1CompBwdRef[ctx][i][0] = Bits<0>(probs);
                bits.av1CompBwdRef[ctx][i][1] = Bits<1>(probs);
            }
        }
        for (int32_t ctx = 0; ctx < AV1_REF_CONTEXTS; ctx++) {
            for (int32_t i = 0; i < SINGLE_REFS - 1; i++) {
                const vpx_prob *probs = &av1_default_single_ref_prob[ctx][i];
                bits.av1SingleRef[ctx][i][0] = Bits<0>(probs);
                bits.av1SingleRef[ctx][i][1] = Bits<1>(probs);
            }
        }
        // tmp
        for (int32_t ctx = 0; ctx < VP9_REF_CONTEXTS; ctx++) {
            const vpx_prob *probs = &vp9_default_comp_ref_probs[ctx];
            bits.vp9CompRef[ctx][0]  = Bits<0>(probs);
            bits.vp9CompRef[ctx][1]  = Bits<1>(probs);
        }
        for (int32_t ctx = 0; ctx < VP9_REF_CONTEXTS; ctx++) {
            for (int32_t i = 0; i < 2; i++) {
                const vpx_prob *probs = &vp9_default_single_ref_prob[ctx][i];
                bits.vp9SingleRef[ctx][i][0]  = Bits<0>(probs);
                bits.vp9SingleRef[ctx][i][1]  = Bits<1>(probs);
            }
        }
    } else {
        for (int32_t ctx = 0; ctx < VP9_REF_CONTEXTS; ctx++) {
            const vpx_prob *probs = &vp9_default_comp_ref_probs[ctx];
            bits.vp9CompRef[ctx][0]  = Bits<0>(probs);
            bits.vp9CompRef[ctx][1]  = Bits<1>(probs);
        }
        for (int32_t ctx = 0; ctx < VP9_REF_CONTEXTS; ctx++) {
            for (int32_t i = 0; i < 2; i++) {
                const vpx_prob *probs = &vp9_default_single_ref_prob[ctx][i];
                bits.vp9SingleRef[ctx][i][0]  = Bits<0>(probs);
                bits.vp9SingleRef[ctx][i][1]  = Bits<1>(probs);
            }
        }
    }

    if (codecType == CODEC_AV1) {
        for (int32_t txSize = 0; txSize < EXT_TX_SIZES; txSize++) {
            for (int32_t txTypeNom = 0; txTypeNom < TX_TYPES; txTypeNom++) {
                const vpx_prob *probs = default_intra_ext_tx_prob[txSize][txTypeNom];
                bits.intraExtTx[txSize][txTypeNom][DCT_DCT]   = Bits<0>(probs);
                bits.intraExtTx[txSize][txTypeNom][ADST_ADST] = Bits<1,0>(probs);
                bits.intraExtTx[txSize][txTypeNom][ADST_DCT]  = Bits<1,1,0>(probs);
                bits.intraExtTx[txSize][txTypeNom][DCT_ADST]  = Bits<1,1,1>(probs);
            }
        }
        Zero(bits.intraExtTx[TX_32X32]);

        for (int32_t txSize = 0; txSize < EXT_TX_SIZES; txSize++) {
            const vpx_prob *probs = default_inter_ext_tx_prob[txSize];
            bits.interExtTx[txSize][DCT_DCT]   = Bits<0>(probs);
            bits.interExtTx[txSize][ADST_ADST] = Bits<1,0>(probs);
            bits.interExtTx[txSize][ADST_DCT]  = Bits<1,1,0>(probs);
            bits.interExtTx[txSize][DCT_ADST]  = Bits<1,1,1>(probs);
        }
        Zero(bits.interExtTx[TX_32X32]);

        for (int32_t q = 0; q < TOKEN_CDF_Q_CTXS; q++) {
            for (int32_t t = 0; t <= TX_32X32; t++) {
                for (int32_t p = 0; p < PLANE_TYPES; p++) {
                    Zero(bits.txb[q][t][p].eobMulti);
                    bits.txb[q][t][p].qpi = q;
                    bits.txb[q][t][p].txSize = t;
                    bits.txb[q][t][p].plane = p;

                    for (int32_t c = 0; c < 11; c++)
                        av1_cost_tokens_from_cdf(bits.txb[q][t][p].coeffBase[c], default_coeff_base_cdfs[q][t][p][c], nullptr);
                    for (int32_t c = 11; c < 16; c++)
                        av1_cost_tokens_from_cdf(bits.txb[q][t][p].coeffBase[c], default_coeff_base_cdfs[q][t][p][c + 10], nullptr);
                    for (int32_t c = 0; c < SIG_COEF_CONTEXTS_EOB; c++)
                        av1_cost_tokens_from_cdf(bits.txb[q][t][p].coeffBaseEob[c], default_coeff_base_eob_cdfs[q][t][p][c], nullptr);
                    for (int32_t c = 0; c < TXB_SKIP_CONTEXTS; c++)
                        av1_cost_tokens_from_cdf(bits.txb[q][t][p].txbSkip[c], default_txb_skip_cdfs[q][t][c], nullptr);

                    uint16_t tmp[4];
                    for (int32_t c = 0; c < LEVEL_CONTEXTS; c++) {
                        av1_cost_tokens_from_cdf(tmp, default_coeff_lps_cdfs[q][t][p][c], nullptr);

                        bits.txb[q][t][p].coeffBrAcc[c][0] = 0;
                        bits.txb[q][t][p].coeffBrAcc[c][1] = 0;
                        bits.txb[q][t][p].coeffBrAcc[c][2] = 0;
                        for (int32_t i = 3; i < 15; i++)
                            bits.txb[q][t][p].coeffBrAcc[c][i] = tmp[i % 3] + tmp[3] * (i / 3 - 1);
                        bits.txb[q][t][p].coeffBrAcc[c][15] = tmp[3] * 4;

                        //for (int32_t i = 0; i < 4; i++)
                        //    bits.txb[q][t].coeffBr[p][c][i] = tmp[i];
                    }
                }
            }

            for (int32_t p = 0; p < PLANE_TYPES; p++) {
                av1_cost_tokens_from_cdf(bits.txb[q][TX_4X4  ][p].eobMulti, default_eob_multi16_cdfs[q][p][TX_CLASS_2D], nullptr);
                av1_cost_tokens_from_cdf(bits.txb[q][TX_8X8  ][p].eobMulti, default_eob_multi64_cdfs[q][p][TX_CLASS_2D], nullptr);
                av1_cost_tokens_from_cdf(bits.txb[q][TX_16X16][p].eobMulti, default_eob_multi256_cdfs[q][p][TX_CLASS_2D], nullptr);
                av1_cost_tokens_from_cdf(bits.txb[q][TX_32X32][p].eobMulti, default_eob_multi1024_cdfs[q][p][TX_CLASS_2D], nullptr);

                for (int32_t i = 2; i < 5; i++)  bits.txb[q][TX_4X4  ][p].eobMulti[i] += 512 * i - 512;
                for (int32_t i = 2; i < 7; i++)  bits.txb[q][TX_8X8  ][p].eobMulti[i] += 512 * i - 512;
                for (int32_t i = 2; i < 9; i++)  bits.txb[q][TX_16X16][p].eobMulti[i] += 512 * i - 512;
                for (int32_t i = 2; i < 11; i++) bits.txb[q][TX_32X32][p].eobMulti[i] += 512 * i - 512;
            }
        }
    } else {
        Zero(bits.intraExtTx);
        Zero(bits.interExtTx);
    }
}

namespace {
    template <int32_t txType> int32_t GetCtxTokenAc(int32_t x, int32_t y, const int32_t *tokenCacheA, const int32_t *tokenCacheL) {
        int32_t a = y==0 ? tokenCacheL[0] : tokenCacheA[x];
        int32_t l = x==0 ? tokenCacheA[0] : tokenCacheL[y];
        return (a + l + 1) >> 1;
    }
    template <> int32_t GetCtxTokenAc<DCT_ADST>(int32_t x, int32_t y, const int32_t *tokenCacheA, const int32_t *tokenCacheL) {
        return y==0 ? tokenCacheL[0] : tokenCacheA[x];
    }
    template <> int32_t GetCtxTokenAc<ADST_DCT>(int32_t x, int32_t y, const int32_t *tokenCacheA, const int32_t *tokenCacheL) {
        return x==0 ? tokenCacheA[0] : tokenCacheL[y];
    }

    const int32_t MC_AVG_BAND2_CTX[6][2] = { { 1196,221 }, { 1884,108 }, { 3075,29 }, { 3621,12 }, { 3935,5 }, { 4039,4 } };
    const int16_t COEF_AVG_BAND2_CTX[6][256] = {
        {315, 1530, 3151, 4398, 5676, 6525, 6890, 8240, 8437, 8680, 8877, 10724, 10863, 10956, 11095, 11267, 11406, 11499, 11638, 13877, 13958, 14016, 14097, 14193, 14274, 14332, 14413, 14459, 14540, 14598, 14679, 14775, 14856, 14914, 14995, 15705, 15728, 15774, 15797, 15855, 15878, 15924, 15947, 16046, 16069, 16115, 16138, 16196, 16219, 16265, 16288, 16342, 16365, 16411, 16434, 16492, 16515, 16561, 16584, 16683, 16706, 16752, 16775, 16833, 16856, 16902, 16925, 20177, 20189, 20200, 20212, 20234, 20246, 20257, 20269, 20316, 20328, 20339, 20351, 20373, 20385, 20396, 20408, 20470, 20482, 20493, 20505, 20527, 20539, 20550, 20562, 20609, 20621, 20632, 20644, 20666, 20678, 20689, 20701, 20772, 20784, 20795, 20807, 20829, 20841, 20852, 20864, 20911, 20923, 20934, 20946, 20968, 20980, 20991, 21003, 21065, 21077, 21088, 21100, 21122, 21134, 21145, 21157, 21204, 21216, 21227, 21239, 21261, 21273, 21284, 21296, 21052, 21064, 21075, 21087, 21109, 21121, 21132, 21144, 21191, 21203, 21214, 21226, 21248, 21260, 21271, 21283, 21345, 21357, 21368, 21380, 21402, 21414, 21425, 21437, 21484, 21496, 21507, 21519, 21541, 21553, 21564, 21576, 21647, 21659, 21670, 21682, 21704, 21716, 21727, 21739, 21786, 21798, 21809, 21821, 21843, 21855, 21866, 21878, 21940, 21952, 21963, 21975, 21997, 22009, 22020, 22032, 22079, 22091, 22102, 22114, 22136, 22148, 22159, 22171, 21787, 21799, 21810, 21822, 21844, 21856, 21867, 21879, 21926, 21938, 21949, 21961, 21983, 21995, 22006, 22018, 22080, 22092, 22103, 22115, 22137, 22149, 22160, 22172, 22219, 22231, 22242, 22254, 22276, 22288, 22299, 22311, 22382, 22394, 22405, 22417, 22439, 22451, 22462, 22474, 22521, 22533, 22544, 22556, 22578, 22590, 22601, 22613, 22675, 22687, 22698, 22710, 22732, 22744, 22755, 22767, 22814, 22826, 22837, 22849, 22871},
        {491, 1255, 2509, 3522, 4561, 5339, 5704, 6869, 7066, 7309, 7506, 9132, 9271, 9364, 9503, 9675, 9814, 9907, 10046, 12118, 12199, 12257, 12338, 12434, 12515, 12573, 12654, 12700, 12781, 12839, 12920, 13016, 13097, 13155, 13236, 14109, 14132, 14178, 14201, 14259, 14282, 14328, 14351, 14450, 14473, 14519, 14542, 14600, 14623, 14669, 14692, 14746, 14769, 14815, 14838, 14896, 14919, 14965, 14988, 15087, 15110, 15156, 15179, 15237, 15260, 15306, 15329, 18576, 18588, 18599, 18611, 18633, 18645, 18656, 18668, 18715, 18727, 18738, 18750, 18772, 18784, 18795, 18807, 18869, 18881, 18892, 18904, 18926, 18938, 18949, 18961, 19008, 19020, 19031, 19043, 19065, 19077, 19088, 19100, 19171, 19183, 19194, 19206, 19228, 19240, 19251, 19263, 19310, 19322, 19333, 19345, 19367, 19379, 19390, 19402, 19464, 19476, 19487, 19499, 19521, 19533, 19544, 19556, 19603, 19615, 19626, 19638, 19660, 19672, 19683, 19695, 19451, 19463, 19474, 19486, 19508, 19520, 19531, 19543, 19590, 19602, 19613, 19625, 19647, 19659, 19670, 19682, 19744, 19756, 19767, 19779, 19801, 19813, 19824, 19836, 19883, 19895, 19906, 19918, 19940, 19952, 19963, 19975, 20046, 20058, 20069, 20081, 20103, 20115, 20126, 20138, 20185, 20197, 20208, 20220, 20242, 20254, 20265, 20277, 20339, 20351, 20362, 20374, 20396, 20408, 20419, 20431, 20478, 20490, 20501, 20513, 20535, 20547, 20558, 20570, 20186, 20198, 20209, 20221, 20243, 20255, 20266, 20278, 20325, 20337, 20348, 20360, 20382, 20394, 20405, 20417, 20479, 20491, 20502, 20514, 20536, 20548, 20559, 20571, 20618, 20630, 20641, 20653, 20675, 20687, 20698, 20710, 20781, 20793, 20804, 20816, 20838, 20850, 20861, 20873, 20920, 20932, 20943, 20955, 20977, 20989, 21000, 21012, 21074, 21086, 21097, 21109, 21131, 21143, 21154, 21166, 21213, 21225, 21236, 21248, 21270},
        {774, 1218, 1964, 2611, 3274, 3833, 4198, 4970, 5167, 5410, 5607, 6765, 6904, 6997, 7136, 7308, 7447, 7540, 7679, 9313, 9394, 9452, 9533, 9629, 9710, 9768, 9849, 9895, 9976, 10034, 10115, 10211, 10292, 10350, 10431, 11726, 11749, 11795, 11818, 11876, 11899, 11945, 11968, 12067, 12090, 12136, 12159, 12217, 12240, 12286, 12309, 12363, 12386, 12432, 12455, 12513, 12536, 12582, 12605, 12704, 12727, 12773, 12796, 12854, 12877, 12923, 12946, 15972, 15984, 15995, 16007, 16029, 16041, 16052, 16064, 16111, 16123, 16134, 16146, 16168, 16180, 16191, 16203, 16265, 16277, 16288, 16300, 16322, 16334, 16345, 16357, 16404, 16416, 16427, 16439, 16461, 16473, 16484, 16496, 16567, 16579, 16590, 16602, 16624, 16636, 16647, 16659, 16706, 16718, 16729, 16741, 16763, 16775, 16786, 16798, 16860, 16872, 16883, 16895, 16917, 16929, 16940, 16952, 16999, 17011, 17022, 17034, 17056, 17068, 17079, 17091, 16847, 16859, 16870, 16882, 16904, 16916, 16927, 16939, 16986, 16998, 17009, 17021, 17043, 17055, 17066, 17078, 17140, 17152, 17163, 17175, 17197, 17209, 17220, 17232, 17279, 17291, 17302, 17314, 17336, 17348, 17359, 17371, 17442, 17454, 17465, 17477, 17499, 17511, 17522, 17534, 17581, 17593, 17604, 17616, 17638, 17650, 17661, 17673, 17735, 17747, 17758, 17770, 17792, 17804, 17815, 17827, 17874, 17886, 17897, 17909, 17931, 17943, 17954, 17966, 17582, 17594, 17605, 17617, 17639, 17651, 17662, 17674, 17721, 17733, 17744, 17756, 17778, 17790, 17801, 17813, 17875, 17887, 17898, 17910, 17932, 17944, 17955, 17967, 18014, 18026, 18037, 18049, 18071, 18083, 18094, 18106, 18177, 18189, 18200, 18212, 18234, 18246, 18257, 18269, 18316, 18328, 18339, 18351, 18373, 18385, 18396, 18408, 18470, 18482, 18493, 18505, 18527, 18539, 18550, 18562, 18609, 18621, 18632, 18644, 18666},
        {1052, 1350, 1811, 2228, 2655, 3016, 3381, 3821, 4018, 4261, 4458, 5189, 5328, 5421, 5560, 5732, 5871, 5964, 6103, 7276, 7357, 7415, 7496, 7592, 7673, 7731, 7812, 7858, 7939, 7997, 8078, 8174, 8255, 8313, 8394, 10006, 10029, 10075, 10098, 10156, 10179, 10225, 10248, 10347, 10370, 10416, 10439, 10497, 10520, 10566, 10589, 10643, 10666, 10712, 10735, 10793, 10816, 10862, 10885, 10984, 11007, 11053, 11076, 11134, 11157, 11203, 11226, 13920, 13932, 13943, 13955, 13977, 13989, 14000, 14012, 14059, 14071, 14082, 14094, 14116, 14128, 14139, 14151, 14213, 14225, 14236, 14248, 14270, 14282, 14293, 14305, 14352, 14364, 14375, 14387, 14409, 14421, 14432, 14444, 14515, 14527, 14538, 14550, 14572, 14584, 14595, 14607, 14654, 14666, 14677, 14689, 14711, 14723, 14734, 14746, 14808, 14820, 14831, 14843, 14865, 14877, 14888, 14900, 14947, 14959, 14970, 14982, 15004, 15016, 15027, 15039, 14795, 14807, 14818, 14830, 14852, 14864, 14875, 14887, 14934, 14946, 14957, 14969, 14991, 15003, 15014, 15026, 15088, 15100, 15111, 15123, 15145, 15157, 15168, 15180, 15227, 15239, 15250, 15262, 15284, 15296, 15307, 15319, 15390, 15402, 15413, 15425, 15447, 15459, 15470, 15482, 15529, 15541, 15552, 15564, 15586, 15598, 15609, 15621, 15683, 15695, 15706, 15718, 15740, 15752, 15763, 15775, 15822, 15834, 15845, 15857, 15879, 15891, 15902, 15914, 15530, 15542, 15553, 15565, 15587, 15599, 15610, 15622, 15669, 15681, 15692, 15704, 15726, 15738, 15749, 15761, 15823, 15835, 15846, 15858, 15880, 15892, 15903, 15915, 15962, 15974, 15985, 15997, 16019, 16031, 16042, 16054, 16125, 16137, 16148, 16160, 16182, 16194, 16205, 16217, 16264, 16276, 16287, 16299, 16321, 16333, 16344, 16356, 16418, 16430, 16441, 16453, 16475, 16487, 16498, 16510, 16557, 16569, 16580, 16592, 16614},
        {1417, 1623, 1881, 2120, 2365, 2529, 2894, 3008, 3205, 3448, 3645, 3914, 4053, 4146, 4285, 4457, 4596, 4689, 4828, 5436, 5517, 5575, 5656, 5752, 5833, 5891, 5972, 6018, 6099, 6157, 6238, 6334, 6415, 6473, 6554, 7702, 7725, 7771, 7794, 7852, 7875, 7921, 7944, 8043, 8066, 8112, 8135, 8193, 8216, 8262, 8285, 8339, 8362, 8408, 8431, 8489, 8512, 8558, 8581, 8680, 8703, 8749, 8772, 8830, 8853, 8899, 8922, 11108, 11120, 11131, 11143, 11165, 11177, 11188, 11200, 11247, 11259, 11270, 11282, 11304, 11316, 11327, 11339, 11401, 11413, 11424, 11436, 11458, 11470, 11481, 11493, 11540, 11552, 11563, 11575, 11597, 11609, 11620, 11632, 11703, 11715, 11726, 11738, 11760, 11772, 11783, 11795, 11842, 11854, 11865, 11877, 11899, 11911, 11922, 11934, 11996, 12008, 12019, 12031, 12053, 12065, 12076, 12088, 12135, 12147, 12158, 12170, 12192, 12204, 12215, 12227, 11983, 11995, 12006, 12018, 12040, 12052, 12063, 12075, 12122, 12134, 12145, 12157, 12179, 12191, 12202, 12214, 12276, 12288, 12299, 12311, 12333, 12345, 12356, 12368, 12415, 12427, 12438, 12450, 12472, 12484, 12495, 12507, 12578, 12590, 12601, 12613, 12635, 12647, 12658, 12670, 12717, 12729, 12740, 12752, 12774, 12786, 12797, 12809, 12871, 12883, 12894, 12906, 12928, 12940, 12951, 12963, 13010, 13022, 13033, 13045, 13067, 13079, 13090, 13102, 12718, 12730, 12741, 12753, 12775, 12787, 12798, 12810, 12857, 12869, 12880, 12892, 12914, 12926, 12937, 12949, 13011, 13023, 13034, 13046, 13068, 13080, 13091, 13103, 13150, 13162, 13173, 13185, 13207, 13219, 13230, 13242, 13313, 13325, 13336, 13348, 13370, 13382, 13393, 13405, 13452, 13464, 13475, 13487, 13509, 13521, 13532, 13544, 13606, 13618, 13629, 13641, 13663, 13675, 13686, 13698, 13745, 13757, 13768, 13780, 13802},
        {2035, 2172, 2287, 2395, 2502, 2496, 2861, 2669, 2866, 3109, 3306, 3085, 3224, 3317, 3456, 3628, 3767, 3860, 3999, 3909, 3990, 4048, 4129, 4225, 4306, 4364, 4445, 4491, 4572, 4630, 4711, 4807, 4888, 4946, 5027, 5315, 5338, 5384, 5407, 5465, 5488, 5534, 5557, 5656, 5679, 5725, 5748, 5806, 5829, 5875, 5898, 5952, 5975, 6021, 6044, 6102, 6125, 6171, 6194, 6293, 6316, 6362, 6385, 6443, 6466, 6512, 6535, 7785, 7797, 7808, 7820, 7842, 7854, 7865, 7877, 7924, 7936, 7947, 7959, 7981, 7993, 8004, 8016, 8078, 8090, 8101, 8113, 8135, 8147, 8158, 8170, 8217, 8229, 8240, 8252, 8274, 8286, 8297, 8309, 8380, 8392, 8403, 8415, 8437, 8449, 8460, 8472, 8519, 8531, 8542, 8554, 8576, 8588, 8599, 8611, 8673, 8685, 8696, 8708, 8730, 8742, 8753, 8765, 8812, 8824, 8835, 8847, 8869, 8881, 8892, 8904, 8660, 8672, 8683, 8695, 8717, 8729, 8740, 8752, 8799, 8811, 8822, 8834, 8856, 8868, 8879, 8891, 8953, 8965, 8976, 8988, 9010, 9022, 9033, 9045, 9092, 9104, 9115, 9127, 9149, 9161, 9172, 9184, 9255, 9267, 9278, 9290, 9312, 9324, 9335, 9347, 9394, 9406, 9417, 9429, 9451, 9463, 9474, 9486, 9548, 9560, 9571, 9583, 9605, 9617, 9628, 9640, 9687, 9699, 9710, 9722, 9744, 9756, 9767, 9779, 9395, 9407, 9418, 9430, 9452, 9464, 9475, 9487, 9534, 9546, 9557, 9569, 9591, 9603, 9614, 9626, 9688, 9700, 9711, 9723, 9745, 9757, 9768, 9780, 9827, 9839, 9850, 9862, 9884, 9896, 9907, 9919, 9990, 10002, 10013, 10025, 10047, 10059, 10070, 10082, 10129, 10141, 10152, 10164, 10186, 10198, 10209, 10221, 10283, 10295, 10306, 10318, 10340, 10352, 10363, 10375, 10422, 10434, 10445, 10457, 10479},
    };

    inline uint32_t EstCoefBits(const uint16_t *bcosts, int32_t coef, int32_t *isNonZero, int32_t *tok) {
        if (coef < CAT1_MIN_VAL) { // ZERO_TOKEN .. FOUR_TOKEN
            *isNonZero = (coef != 0);
            *tok = coef;
            return bcosts[coef];
        } else if (coef < CAT6_MIN_VAL) {
            *isNonZero = 1;
            *tok = BSR(coef - 3) + 4;
            return bcosts[*tok] + default_bitcost_extraTotal[coef];
        } else {
            *isNonZero = 1;
            *tok = DCT_VAL_CATEGORY6;
            uint32_t numbits = bcosts[DCT_VAL_CATEGORY6];
            int32_t remCoef = coef - CAT6_MIN_VAL;
            for (int32_t e = 0; e < 14; e++, remCoef >>= 1)
                numbits += default_bitcost_extra[DCT_VAL_CATEGORY6-DCT_VAL_CATEGORY1][13-e][remCoef & 1]; // extra bits
            return numbits;
        }
    }
}


uint32_t H265Enc::EstimateCoefsFastest(const CoefBitCounts &bits, const CoeffsType *coefs, const int16_t *scan, int32_t numnz, int32_t dcCtx)
{
    assert(numnz > 0); // should not be called with numnz=0 (speed)
    //if (numnz == 0)
    //    return bits.moreCoef[0][dcCtx][0]; // no more_coef

    uint32_t numbits = 0;
    int32_t nzc = numnz;
    int32_t coef = abs(coefs[0]);
    int32_t tok;
    int32_t isNz = 0;
    numbits += EstCoefBits(bits.tokenMc[0][dcCtx], coef, &isNz, &tok);
    nzc -= isNz;
    int32_t dcEnergy = energy_class[tok];
    int32_t acEnergy = 0;
    int32_t mc0ctx = dcEnergy;
    int32_t mc0band = 1;
    if (nzc) {
        coef = abs(coefs[scan[1]]);
        numbits += EstCoefBits(bits.tokenMc[1][dcEnergy], coef, &isNz, &tok);
        nzc -= isNz;
        acEnergy = energy_class[tok];
    }
    if (nzc) {
        coef = abs(coefs[scan[2]]);
        numbits += EstCoefBits(bits.tokenMc[1][dcEnergy], coef, &isNz, &tok);
        nzc -= isNz;
        acEnergy += energy_class[tok];
        mc0ctx = (acEnergy + 1) >> 1;
        mc0band = 2;
    }
    if (!nzc)
        return numbits + bits.moreCoef[mc0band][mc0ctx][0]; // no more_coef

    acEnergy >>= 1;
    numbits += MC_AVG_BAND2_CTX[acEnergy][1] * nzc;
    numbits += MC_AVG_BAND2_CTX[acEnergy][0]; // no more_coef
    scan += 3;
    while (nzc) {
        int16_t coef = coefs[*scan];
        uint16_t absCoef = IPP_MIN(256, abs(coef));
        numbits += COEF_AVG_BAND2_CTX[acEnergy][absCoef];
        nzc -= (coef != 0);
        scan++;
    }
    return numbits;

    //uint16_t remCoefs[32*32];
    //int32_t remNzc = nzc;
    //int32_t eob = 3;
    //while (nzc) {
    //    int16_t coef = coefs[scan[eob]];
    //    uint16_t absCoef = IPP_MIN(1000, abs(coef));
    //    remCoefs[eob] = absCoef;
    //    acEnergy += energy_class[GetToken(absCoef)];
    //    nzc -= (coef != 0);
    //    eob++;
    //}
    //acEnergy = int32_t(acEnergy / float(eob/*-1*/) + 0.5f);

    //for (int32_t i = 3; i < eob; i++)
    //    numbits += COEF_AVG_BAND2_CTX[acEnergy][remCoefs[i]];

    //numbits += MC_AVG_BAND2_CTX[acEnergy][1] * remNzc;
    //numbits += MC_AVG_BAND2_CTX[acEnergy][0]; // no more_coef

    //return numbits;
}
