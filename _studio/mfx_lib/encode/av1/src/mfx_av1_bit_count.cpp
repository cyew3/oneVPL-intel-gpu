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

#include "mfx_av1_defs.h"
#include "mfx_av1_bit_count.h"
#include "mfx_av1_get_context.h"
#include "mfx_av1_binarization.h"
#include "mfx_av1_probabilities.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_tables.h"
#include <immintrin.h>

#include <algorithm>

using namespace AV1Enc;

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
            costs[-ii] = int16_t(cc);
        else
            cost(costs, tree, probs, ii, cc);
    }
}

void av1_cost_tokens(uint16_t *costs, const vpx_prob *probs, int8_t* tree)
{
    cost(costs, tree, probs, 0, 0);
}

#define av1_cost_zero(prob) (prob2bits[prob])

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
  const int shift = CDF_PROB_BITS - 1 - get_msb(p15);
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
            costs[inv_map[i]] = (CostType)av1_cost_symbol(p15);
        else
            costs[i] = (CostType)av1_cost_symbol(p15);

        // Stop once we reach the end of the CDF
        if (cdf[i] == AOM_ICDF(CDF_PROB_TOP)) break;
    }
}

void AV1Enc::EstimateBits(BitCounts &bits)
{
    enum { HAS_COLS = 1, HAS_ROWS = 2 };
    for (int32_t ctx = 0; ctx < AV1_PARTITION_CONTEXTS; ctx++) {
        av1_cost_tokens_from_cdf(bits.av1Partition[HAS_ROWS + HAS_COLS][ctx], default_partition_cdf[ctx], NULL);

        aom_cdf_prob tmpCdf[2];
        uint32_t tmpCost[2] = {};
        partition_gather_vert_alike(tmpCdf, default_partition_cdf[ctx], BLOCK_64X64);
        av1_cost_tokens_from_cdf(tmpCost, tmpCdf, NULL);
        bits.av1Partition[HAS_COLS][ctx][PARTITION_HORZ] = (uint16_t)tmpCost[0];
        bits.av1Partition[HAS_COLS][ctx][PARTITION_SPLIT] = (uint16_t)tmpCost[1];

        partition_gather_horz_alike(tmpCdf, default_partition_cdf[ctx], BLOCK_64X64);
        av1_cost_tokens_from_cdf(tmpCost, tmpCdf, NULL);
        bits.av1Partition[HAS_ROWS][ctx][PARTITION_VERT] = (uint16_t)tmpCost[0];
        bits.av1Partition[HAS_ROWS][ctx][PARTITION_SPLIT] = (uint16_t)tmpCost[1];
    }


    //for (int32_t ctx = 0; ctx < SKIP_CONTEXTS; ctx++) {
    //    vpx_prob prob = default_skip_prob[ctx];
    //    bits.skip[ctx][0] = Bits<0>(prob);
    //    bits.skip[ctx][1] = Bits<1>(prob);
    //}
    for (int32_t ctx = 0; ctx < SKIP_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(bits.skip[ctx], default_skip_cdf[ctx], nullptr);

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

    for (int i = 0; i < KF_MODE_CONTEXTS; ++i)
        for (int j = 0; j < KF_MODE_CONTEXTS; ++j)
            av1_cost_tokens_from_cdf(bits.kfIntraModeAV1[i][j], default_av1_kf_y_mode_cdf[i][j], NULL);
    for (int i = 0; i < BLOCK_SIZE_GROUPS; ++i)
        av1_cost_tokens_from_cdf(bits.intraModeAV1[i], default_if_y_mode_cdf[i], NULL);
    for (int j = 0; j < CFL_ALLOWED_TYPES; j++) {
        for (int i = 0; i < AV1_INTRA_MODES; ++i)
            av1_cost_tokens_from_cdf(bits.intraModeUvAV1[j][i], default_uv_mode_cdf[j][i], NULL);
    }

    for (int i = 0; i < PALETTE_BLOCK_SIZE_CONTEXTS; ++i)
        for (int j = 0; j < 3; ++j)
            av1_cost_tokens_from_cdf(bits.HasPaletteY[i][j], default_av1_y_haspalette_cdf[i][j], NULL);
    for (int i = 0; i < 2; ++i)
        av1_cost_tokens_from_cdf(bits.HasPaletteUV[i], default_av1_uv_haspalette_cdf[i], NULL);
    for (int i = 0; i < PALETTE_BLOCK_SIZE_CONTEXTS; ++i)
        av1_cost_tokens_from_cdf(bits.PaletteSizeY[i], default_av1_y_palettesize_cdf[i], NULL);
    for (int i = 0; i < PALETTE_BLOCK_SIZE_CONTEXTS; ++i)
        av1_cost_tokens_from_cdf(bits.PaletteSizeUV[i], default_av1_uv_palettesize_cdf[i], NULL);

    for (int i = 0; i < PALETTE_SIZES; ++i)
        for (int j = 0; j < 5; ++j)
            av1_cost_tokens_from_cdf(bits.PaletteColorIdxY[i][j], default_av1_y_palettecoloridx_cdf[i][j], NULL);

    for (int i = 0; i < PALETTE_SIZES; ++i)
        for (int j = 0; j < 5; ++j)
            av1_cost_tokens_from_cdf(bits.PaletteColorIdxUV[i][j], default_av1_uv_palette_color_index_cdf[i][j], NULL);


    for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
        for (int32_t plane = 0; plane < BLOCK_TYPES; plane++) {
            for (int32_t inter = 0; inter < REF_TYPES; inter++) {
                bits.coef[plane][inter][txSize].txSize = (uint8_t)txSize;
                bits.coef[plane][inter][txSize].plane = (uint8_t)plane;
                bits.coef[plane][inter][txSize].inter = (uint8_t)inter;
                for (int32_t band = 0; band < COEF_BANDS; band++) {
                    for (int32_t ctx = 0; ctx < PREV_COEF_CONTEXTS; ctx++) {
                        const vpx_prob *probs = default_coef_probs[txSize][plane][inter][band][ctx];
                        bits.coef[plane][inter][txSize].moreCoef[band][ctx][0] = Bits<0>(probs);
                        bits.coef[plane][inter][txSize].moreCoef[band][ctx][1] = Bits<1>(probs);
                        vpx_prob p[10] = {}; // pareto probs
                        for (int32_t i = 2; i < 10; i++)
                            p[i] = (vpx_prob)pareto(i, probs[2]);
                        bits.coef[plane][inter][txSize].token[band][ctx][ZERO_TOKEN       ] = Bits<0>(probs+1);
                        bits.coef[plane][inter][txSize].token[band][ctx][ONE_TOKEN        ] = 512 + Bits<1,0>(probs+1);  // 512 is for sign bit
                        bits.coef[plane][inter][txSize].token[band][ctx][TWO_TOKEN        ] = 512 + Bits<1,1>(probs+1) + Bits<0,0>(p[2],p[3]);
                        bits.coef[plane][inter][txSize].token[band][ctx][THREE_TOKEN      ] = 512 + Bits<1,1>(probs+1) + Bits<0,1,0>(p[2],p[3],p[4]);
                        bits.coef[plane][inter][txSize].token[band][ctx][FOUR_TOKEN       ] = 512 + Bits<1,1>(probs+1) + Bits<0,1,1>(p[2],p[3],p[4]);
                        bits.coef[plane][inter][txSize].token[band][ctx][DCT_VAL_CATEGORY1] = 512 + Bits<1,1>(probs+1) + Bits<1,0,0>(p[2],p[5],p[6]);
                        bits.coef[plane][inter][txSize].token[band][ctx][DCT_VAL_CATEGORY2] = 512 + Bits<1,1>(probs+1) + Bits<1,0,1>(p[2],p[5],p[6]);
                        bits.coef[plane][inter][txSize].token[band][ctx][DCT_VAL_CATEGORY3] = 512 + Bits<1,1>(probs+1) + Bits<1,1,0,0>(p[2],p[5],p[7],p[8]);
                        bits.coef[plane][inter][txSize].token[band][ctx][DCT_VAL_CATEGORY4] = 512 + Bits<1,1>(probs+1) + Bits<1,1,0,1>(p[2],p[5],p[7],p[8]);
                        bits.coef[plane][inter][txSize].token[band][ctx][DCT_VAL_CATEGORY5] = 512 + Bits<1,1>(probs+1) + Bits<1,1,1,0>(p[2],p[5],p[7],p[9]);
                        bits.coef[plane][inter][txSize].token[band][ctx][DCT_VAL_CATEGORY6] = 512 + Bits<1,1>(probs+1) + Bits<1,1,1,1>(p[2],p[5],p[7],p[9]);

                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][ZERO_TOKEN       ] = Bits<1,0>(probs);
                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][ONE_TOKEN        ] = 512 + Bits<1,1,0>(probs);
                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][TWO_TOKEN        ] = 512 + Bits<1,1,1>(probs) + Bits<0,0>(p[2],p[3]);
                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][THREE_TOKEN      ] = 512 + Bits<1,1,1>(probs) + Bits<0,1,0>(p[2],p[3],p[4]);
                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][FOUR_TOKEN       ] = 512 + Bits<1,1,1>(probs) + Bits<0,1,1>(p[2],p[3],p[4]);
                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][DCT_VAL_CATEGORY1] = 512 + Bits<1,1,1>(probs) + Bits<1,0,0>(p[2],p[5],p[6]);
                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][DCT_VAL_CATEGORY2] = 512 + Bits<1,1,1>(probs) + Bits<1,0,1>(p[2],p[5],p[6]);
                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][DCT_VAL_CATEGORY3] = 512 + Bits<1,1,1>(probs) + Bits<1,1,0,0>(p[2],p[5],p[7],p[8]);
                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][DCT_VAL_CATEGORY4] = 512 + Bits<1,1,1>(probs) + Bits<1,1,0,1>(p[2],p[5],p[7],p[8]);
                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][DCT_VAL_CATEGORY5] = 512 + Bits<1,1,1>(probs) + Bits<1,1,1,0>(p[2],p[5],p[7],p[9]);
                        bits.coef[plane][inter][txSize].tokenMc[band][ctx][DCT_VAL_CATEGORY6] = 512 + Bits<1,1,1>(probs) + Bits<1,1,1,1>(p[2],p[5],p[7],p[9]);
                    }
                }
            }
        }
    }


    int sign_cost[CFL_JOINT_SIGNS];
    av1_cost_tokens_from_cdf(sign_cost, default_cfl_sign_cdf, NULL);
    for (int joint_sign = 0; joint_sign < CFL_JOINT_SIGNS; joint_sign++) {
        uint16_t *cost_u = bits.cflCost[joint_sign][CFL_PRED_U];
        uint16_t *cost_v = bits.cflCost[joint_sign][CFL_PRED_V];
        if (CFL_SIGN_U(joint_sign) == CFL_SIGN_ZERO) {
            memset(cost_u, 0, CFL_ALPHABET_SIZE * sizeof(*cost_u));
        }
        else {
            const aom_cdf_prob *cdf_u = default_cfl_alpha_cdf[CFL_CONTEXT_U(joint_sign)];
            av1_cost_tokens_from_cdf(cost_u, cdf_u, NULL);
        }
        if (CFL_SIGN_V(joint_sign) == CFL_SIGN_ZERO) {
            memset(cost_v, 0, CFL_ALPHABET_SIZE * sizeof(*cost_v));
        }
        else {
            const aom_cdf_prob *cdf_v = default_cfl_alpha_cdf[CFL_CONTEXT_V(joint_sign)];
            av1_cost_tokens_from_cdf(cost_v, cdf_v, NULL);
        }
        for (int u = 0; u < CFL_ALPHABET_SIZE; u++)
            cost_u[u] += (uint16_t)sign_cost[joint_sign];
    }

    //static int a = 0;
    //if (!a) {
    //    a = 1;
    //    fprintf(stderr, "morecoef\n");
    //    for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
    //        for (int32_t plane = 0; plane < BLOCK_TYPES; plane++) {
    //            for (int32_t inter = 0; inter < REF_TYPES; inter++) {
    //                for (int32_t band = 0; band < COEF_BANDS; band++) {
    //                    for (int32_t ctx = 0; ctx < (band==0?3:6); ctx++) {
    //                        fprintf(stderr, "%u,", bits.coef[plane][inter][txSize].moreCoef[band][ctx][0]);
    //                        fprintf(stderr, "%u,", bits.coef[plane][inter][txSize].moreCoef[band][ctx][1]);
    //                    }
    //                }
    //            }
    //        }
    //    }
    //    fprintf(stderr, "\n");
    //    fprintf(stderr, "\n");
    //    fprintf(stderr, "token\n");
    //    for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
    //        for (int32_t plane = 0; plane < BLOCK_TYPES; plane++) {
    //            for (int32_t inter = 0; inter < REF_TYPES; inter++) {
    //                for (int32_t band = 0; band < COEF_BANDS; band++) {
    //                    for (int32_t ctx = 0; ctx < (band==0?3:6); ctx++) {
    //                        for (int32_t t = 0; t < 11; t++) {
    //                            fprintf(stderr, "%u,", bits.coef[plane][inter][txSize].token[band][ctx][t]);
    //                        }
    //                    }
    //                }
    //            }
    //        }
    //    }
    //    fprintf(stderr, "\n");
    //    fprintf(stderr, "\n");
    //    fprintf(stderr, "tokenMc\n");
    //    for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
    //        for (int32_t plane = 0; plane < BLOCK_TYPES; plane++) {
    //            for (int32_t inter = 0; inter < REF_TYPES; inter++) {
    //                for (int32_t band = 0; band < COEF_BANDS; band++) {
    //                    for (int32_t ctx = 0; ctx < (band==0?3:6); ctx++) {
    //                        for (int32_t t = 0; t < 11; t++) {
    //                            fprintf(stderr, "%u,", bits.coef[plane][inter][txSize].tokenMc[band][ctx][t]);
    //                        }
    //                    }
    //                }
    //            }
    //        }
    //    }
    //    fprintf(stderr, "\n");
    //    fprintf(stderr, "\n");

    //    fprintf(stderr, "coefs\n");
    //    //fprintf(stderr, ",tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx4,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx8,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx16,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32,tx32\n");
    //    //fprintf(stderr, ",lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,lu,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch,ch\n");
    //    //fprintf(stderr, ",I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P,P\n");
    //    //fprintf(stderr, ",b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b0,b0,b0,b0,b0,b0,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b1,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b2,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b3,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b4,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5,b5\n");
    //    //fprintf(stderr, ",c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5,c0,c0,c1,c1,c2,c2,c3,c3,c4,c4,c5,c5\n");
    //    //fprintf(stderr, ",mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1,mc0,mc1\n");

    //    fprintf(stderr, "tx,plane,inter,band,ctx");
    //    for (int32_t c = 0; c < 1000; c++)
    //        fprintf(stderr, ",%d", c);
    //    fprintf(stderr, "\n");
    //    for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
    //        for (int32_t plane = 0; plane < BLOCK_TYPES; plane++) {
    //            for (int32_t inter = 0; inter < REF_TYPES; inter++) {
    //                for (int32_t band = 0; band < COEF_BANDS; band++) {
    //                    for (int32_t ctx = 0; ctx < ((band == 0) ? 3 : 6); ctx++) {
    //                        fprintf(stderr, "%d,%d,%d,%d,%d", txSize, plane, inter, band, ctx);
    //                        for (int32_t c = 0; c < 1000; c++) {
    //                            int32_t tok = GetToken(c);
    //                            int32_t numbits = bits.coef[plane][inter][txSize].token[band][ctx][tok];
    //                            if (c < CAT6_MIN_VAL) {
    //                                numbits += default_bitcost_extraTotal[c];
    //                            } else {
    //                                int32_t remCoef = c - CAT6_MIN_VAL;
    //                                for (int32_t e = 0; e < 14; e++, remCoef >>= 1)
    //                                    numbits += default_bitcost_extra[DCT_VAL_CATEGORY6-DCT_VAL_CATEGORY1][13-e][remCoef & 1]; // extra bits
    //                            }
    //                            fprintf(stderr, ",%d", numbits);
    //                        }
    //                        fprintf(stderr, "\n");
    //                    }
    //                }
    //            }
    //        }
    //    }
    //}
    for (int32_t ctx = 0; ctx < INTER_MODE_CONTEXTS; ctx++) {
        const vpx_prob *probs = default_inter_mode_probs[ctx];
        bits.interMode[ctx][ZEROMV-NEARESTMV]    = Bits<0>(probs);
        bits.interMode[ctx][NEARESTMV-NEARESTMV] = Bits<1,0>(probs);
        bits.interMode[ctx][NEARMV-NEARESTMV]    = Bits<1,1,0>(probs);
        bits.interMode[ctx][NEWMV-NEARESTMV]     = Bits<1,1,1>(probs);
    }
    for (int32_t ctx0 = 0; ctx0 < DRL_MODE_CONTEXTS; ctx0++) {
        for (int32_t ctx1 = 0; ctx1 < DRL_MODE_CONTEXTS; ctx1++) {
            const vpx_prob probs[2] = { default_drl_prob[ctx0], default_drl_prob[ctx1] };
            bits.drlIdx[ctx0][ctx1][0] = Bits<0>(probs);
            bits.drlIdx[ctx0][ctx1][1] = Bits<1, 0>(probs);
            bits.drlIdx[ctx0][ctx1][2] = Bits<1, 1>(probs);
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
    av1_cost_tokens_from_cdf(bits.intrabc_cost, av1_default_intrabc_cdf, NULL);
    av1_cost_tokens_from_cdf(bits.delta_q_abs_cost, av1_default_delta_q_abs_cdf, NULL);
    for (int32_t ctx = 0; ctx < INTER_MODE_CONTEXTS; ctx++)
        av1_cost_tokens_from_cdf(bits.interCompMode[ctx], default_inter_compound_mode_cdf[ctx], nullptr);

    for (int32_t ctx = 0; ctx < SWITCHABLE_FILTER_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(bits.interpFilterAV1[ctx], default_switchable_interp_cdf[ctx], nullptr);

    //for (int32_t ctx = 0; ctx < IS_INTER_CONTEXTS; ctx++) {
    //    const vpx_prob *probs = &default_is_inter_prob[ctx];
    //    bits.isInter[ctx][0] = Bits<0>(probs);
    //    bits.isInter[ctx][1] = Bits<1>(probs);
    //}
    for (int32_t ctx = 0; ctx < INTRA_INTER_CONTEXTS; ++ctx)
        av1_cost_tokens_from_cdf(bits.isInter[ctx], default_intra_inter_cdf[ctx], nullptr);

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
            uint16_t cost = bits.mvClass0Bit[comp][mv_class0_bit]
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

    for (int32_t txSize = 0; txSize < EXT_TX_SIZES; txSize++) {
        for (int32_t txTypeNom = 0; txTypeNom < TX_TYPES; txTypeNom++) {
            const vpx_prob *probs = default_intra_ext_tx_prob[txSize][txTypeNom];
            bits.intraExtTx[txSize][txTypeNom][DCT_DCT] = Bits<0>(probs);
            bits.intraExtTx[txSize][txTypeNom][ADST_ADST] = Bits<1, 0>(probs);
            bits.intraExtTx[txSize][txTypeNom][ADST_DCT] = Bits<1, 1, 0>(probs);
            bits.intraExtTx[txSize][txTypeNom][DCT_ADST] = Bits<1, 1, 1>(probs);
        }
    }
    Zero(bits.intraExtTx[TX_32X32]);

    for (int i = TX_4X4; i < EXT_TX_SIZES; ++i) {
        for (int s = 1; s < EXT_TX_SETS_INTER; ++s)
            av1_cost_tokens_from_cdf(bits.inter_tx_type_costs[s][i], default_inter_ext_tx_cdf[s][i], av1_ext_tx_inv[av1_ext_tx_set_idx_to_type[1][s]]);
        for (int s = 1; s < EXT_TX_SETS_INTRA; ++s)
            for (int j = 0; j < AV1_INTRA_MODES; ++j)
                av1_cost_tokens_from_cdf(bits.intra_tx_type_costs[s][i][j], default_intra_ext_tx_cdf[s][i][j], av1_ext_tx_inv[av1_ext_tx_set_idx_to_type[0][s]]);
    }

    for (int32_t txSize = 0; txSize < EXT_TX_SIZES; txSize++) {
        const vpx_prob *probs = default_inter_ext_tx_prob[txSize];
        bits.interExtTx[txSize][DCT_DCT] = Bits<0>(probs);
        bits.interExtTx[txSize][ADST_ADST] = Bits<1, 0>(probs);
        bits.interExtTx[txSize][ADST_DCT] = Bits<1, 1, 0>(probs);
        bits.interExtTx[txSize][DCT_ADST] = Bits<1, 1, 1>(probs);
    }
    Zero(bits.interExtTx[TX_32X32]);

    for (int32_t q = 0; q < TOKEN_CDF_Q_CTXS; q++) {
        for (int32_t t = 0; t <= TX_32X32; t++) {
            for (int32_t p = 0; p < PLANE_TYPES; p++) {
                TxbBitCounts& bc = bits.txb[q][t][p];
                bc.qpi = (uint8_t)q;
                bc.txSize = (uint8_t)t;
                bc.plane = (uint8_t)p;

                for (int32_t c = 0; c < 11; c++)
                    av1_cost_tokens_from_cdf(bc.coeffBase[c], default_coeff_base_cdfs[q][t][p][c], nullptr);
                for (int32_t c = 11; c < 16; c++)
                    av1_cost_tokens_from_cdf(bc.coeffBase[c], default_coeff_base_cdfs[q][t][p][c + 10], nullptr);
                for (int32_t c = 0; c < SIG_COEF_CONTEXTS_EOB; c++)
                    av1_cost_tokens_from_cdf(bc.coeffBaseEob[c], default_coeff_base_eob_cdfs[q][t][p][c], nullptr);
                for (int32_t c = 0; c < TXB_SKIP_CONTEXTS; c++)
                    av1_cost_tokens_from_cdf(bc.txbSkip[c], default_txb_skip_cdfs[q][t][c], nullptr);

                //copy transpose
                for (int c = 0; c < 16; c++) {
                    for (int l = 0; l < (NUM_BASE_LEVELS + 2); l++) {
                        bc.coeffBaseT[l][c] = bc.coeffBase[c][l];
                    }
                }

                uint16_t tmp[4];
                for (int32_t c = 0; c < LEVEL_CONTEXTS; c++) {
                    av1_cost_tokens_from_cdf(tmp, default_coeff_lps_cdfs[q][t][p][c], nullptr);

                    bc.coeffBrAcc[c][0] = 0;
                    bc.coeffBrAcc[c][1] = 0;
                    bc.coeffBrAcc[c][2] = 0;
                    for (int32_t i = 3; i < 15; i++)
                        bc.coeffBrAcc[c][i] = uint16_t(tmp[i % 3] + tmp[3] * (i / 3 - 1));
                    bc.coeffBrAcc[c][15] = tmp[3] * 4;

                    //for (int32_t i = 0; i < 4; i++)
                    //    bits.txb[q][t].coeffBr[p][c][i] = tmp[i];
                }
            }
        }

        for (int32_t p = 0; p < PLANE_TYPES; p++) {
            av1_cost_tokens_from_cdf(bits.txb[q][TX_4X4][p].eobMulti, default_eob_multi16_cdfs[q][p][TX_CLASS_2D], nullptr);
            av1_cost_tokens_from_cdf(bits.txb[q][TX_8X8][p].eobMulti, default_eob_multi64_cdfs[q][p][TX_CLASS_2D], nullptr);
            av1_cost_tokens_from_cdf(bits.txb[q][TX_16X16][p].eobMulti, default_eob_multi256_cdfs[q][p][TX_CLASS_2D], nullptr);
            av1_cost_tokens_from_cdf(bits.txb[q][TX_32X32][p].eobMulti, default_eob_multi1024_cdfs[q][p][TX_CLASS_2D], nullptr);

            for (int32_t i = 2; i < 5; i++)  bits.txb[q][TX_4X4][p].eobMulti[i] += uint16_t(512 * i - 512);
            for (int32_t i = 2; i < 7; i++)  bits.txb[q][TX_8X8][p].eobMulti[i] += uint16_t(512 * i - 512);
            for (int32_t i = 2; i < 9; i++)  bits.txb[q][TX_16X16][p].eobMulti[i] += uint16_t(512 * i - 512);
            for (int32_t i = 2; i < 11; i++) bits.txb[q][TX_32X32][p].eobMulti[i] += uint16_t(512 * i - 512);
        }
    }
}

void AV1Enc::GetRefFrameContextsAv1(Frame *frame)
{
    const BitCounts &bc = *frame->bitCount;

    const int8_t posibilities[6][2] = {
        { INTRA_FRAME,  NONE_FRAME   }, // placeholder for case when neighbour is not available
        { INTRA_FRAME,  NONE_FRAME   },
        { LAST_FRAME,   NONE_FRAME   },
        { GOLDEN_FRAME, NONE_FRAME   },
        { ALTREF_FRAME, NONE_FRAME   },
        { LAST_FRAME,   ALTREF_FRAME },
    };

    uint8_t neighborsRefCounts[7];
    ModeInfo above = {}, left = {};
    const ModeInfo *pa = nullptr, *pl = nullptr;

    for (int32_t i = 0; i < 6; i++) {
        for (int32_t j = 0; j < 6; j++) {
            RefFrameContextsAv1 *ctx = &frame->m_refFramesContextsAv1[i][j];
            uint16_t *rfbits = frame->m_refFrameBitsAv1[i][j];

            above.refIdx[0] = posibilities[i][0];
            above.refIdx[1] = posibilities[i][1];
            left.refIdx[0] = posibilities[j][0];
            left.refIdx[1] = posibilities[j][1];
            pa = (i == 0) ? nullptr : &above;
            pl = (j == 0) ? nullptr : &left;

            CollectNeighborsRefCounts(pa, pl, neighborsRefCounts);

            ctx->singleRefP1  = (uint8_t)GetCtxSingleRefP1Av1(neighborsRefCounts);
            ctx->singleRefP2  = (uint8_t)GetCtxSingleRefP2Av1(neighborsRefCounts);
            ctx->singleRefP3  = (uint8_t)GetCtxSingleRefP3(neighborsRefCounts);
            ctx->singleRefP4  = (uint8_t)GetCtxSingleRefP4(neighborsRefCounts);
            ctx->singleRefP5  = (uint8_t)GetCtxSingleRefP5(neighborsRefCounts);
            ctx->refMode      = (uint8_t)GetCtxReferenceMode(pa, pl);
            ctx->compRefType  = (uint8_t)GetCtxCompRefType(pa, pl);
            ctx->compFwdRefP0 = (uint8_t)GetCtxCompRefP0(neighborsRefCounts);
            ctx->compFwdRefP1 = (uint8_t)GetCtxCompRefP1(neighborsRefCounts);
            ctx->compBwdRefP0 = (uint8_t)GetCtxCompBwdRefP0(neighborsRefCounts);

            if (frame->compoundReferenceAllowed) {
                rfbits[0] = bc.compMode[ctx->refMode][0]            // LAST
                          + bc.av1SingleRef[ctx->singleRefP1][0][0]
                          + bc.av1SingleRef[ctx->singleRefP3][2][0]
                          + bc.av1SingleRef[ctx->singleRefP4][3][0];
                rfbits[1] = bc.compMode[ctx->refMode][0]            // ALTREF
                          + bc.av1SingleRef[ctx->singleRefP1][0][1]
                          + bc.av1SingleRef[ctx->singleRefP2][1][1];
                rfbits[2] = bc.compMode[ctx->refMode][1]            // COMP
                          + bc.av1CompRefType[ctx->compRefType][1]
                          + bc.av1CompRef[ctx->compFwdRefP0][0][0]
                          + bc.av1CompRef[ctx->compFwdRefP1][1][1]
                          + bc.av1CompBwdRef[ctx->compBwdRefP0][0][1];
                rfbits[3] = 0xffff;                                 // unused
            } else {
                rfbits[0] = bc.av1SingleRef[ctx->singleRefP1][0][0] // LAST
                          + bc.av1SingleRef[ctx->singleRefP3][2][0]
                          + bc.av1SingleRef[ctx->singleRefP4][3][0];
                rfbits[1] = bc.av1SingleRef[ctx->singleRefP1][0][0] // GOLDEN
                          + bc.av1SingleRef[ctx->singleRefP3][2][1]
                          + bc.av1SingleRef[ctx->singleRefP5][4][1];
                rfbits[2] = 0xffff;                                 // unused
                rfbits[3] = 0xffff;                                 // unused
            }
        }
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


uint32_t AV1Enc::EstimateCoefsFastest(const CoefBitCounts &bits, const CoeffsType *coefs, const int16_t *scan, int32_t numnz, int32_t dcCtx)
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
        coef = std::abs((int)coefs[scan[2]]);
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
        const int16_t c = coefs[*scan];
        uint16_t absCoef = (uint16_t)std::min((int)256, std::abs((int)c));
        numbits += COEF_AVG_BAND2_CTX[acEnergy][absCoef];
        nzc -= (c != 0);
        scan++;
    }
    return numbits;

    //uint16_t remCoefs[32*32];
    //int32_t remNzc = nzc;
    //int32_t eob = 3;
    //while (nzc) {
    //    int16_t coef = coefs[scan[eob]];
    //    uint16_t absCoef = std::min(1000, abs(coef));
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


template <int32_t txType>
uint32_t AV1Enc::EstimateCoefs(const CoefBitCounts &bits, const CoeffsType *coefs, const int16_t *scan, int32_t eob, int32_t dcCtx)
{
    assert(eob != 0);

    const TxSize txSize = bits.txSize;
    const uint8_t *band = (txSize == TX_4X4 ? coefband_4x4 : coefband_8x8plus);
    const int32_t shiftPosY = txSize + 2;
    const int32_t maskPosX = (1 << shiftPosY) - 1;
    int32_t tokenCacheA[32];
    int32_t tokenCacheL[32];

    tokenCacheA[0] = tokenCacheL[0] = dcCtx;

    int32_t segEob = 16 << (txSize << 1);
    uint32_t numbits = 0;

    int32_t c = 0;
    int32_t nzc = 0;
    while (nzc < eob) {
        assert(c < segEob);
        int32_t pos = scan[c];
        int32_t ctx = GetCtxTokenAc<txType>(pos & maskPosX, pos >> shiftPosY, tokenCacheA, tokenCacheL);
        assert(ctx < PREV_COEF_CONTEXTS);

        int32_t absCoef = abs(coefs[pos]);
        int32_t tok = GetToken(absCoef);
        tokenCacheA[pos & maskPosX] = energy_class[tok];
        tokenCacheL[pos >> shiftPosY] = energy_class[tok];
        numbits += bits.tokenMc[band[c]][ctx][tok]; // more_coef=1 and token

        if (absCoef < CAT6_MIN_VAL) {
            numbits += default_bitcost_extraTotal[absCoef];
        } else {
            int32_t remCoef = absCoef - CAT6_MIN_VAL;
            for (int32_t e = 0; e < 14; e++, remCoef >>= 1)
                numbits += default_bitcost_extra[DCT_VAL_CATEGORY6-DCT_VAL_CATEGORY1][13-e][remCoef & 1]; // extra bits
        }

        if (tok == ZERO_TOKEN) {
            c++;
            pos = scan[c];
            while (coefs[pos] == 0) {
                ctx = GetCtxTokenAc<txType>(pos & maskPosX, pos >> shiftPosY, tokenCacheA, tokenCacheL);
                tokenCacheA[pos & maskPosX] = energy_class[ZERO_TOKEN];
                tokenCacheL[pos >> shiftPosY] = energy_class[ZERO_TOKEN];
                numbits += bits.token[band[c]][ctx][ZERO_TOKEN]; // token
                c++;
                pos = scan[c];
            }

            absCoef = abs(coefs[pos]);
            ctx = GetCtxTokenAc<txType>(pos & maskPosX, pos >> shiftPosY, tokenCacheA, tokenCacheL);
            tok = GetToken(absCoef);
            tokenCacheA[pos & maskPosX] = energy_class[tok];
            tokenCacheL[pos >> shiftPosY] = energy_class[tok];
            numbits += bits.token[band[c]][ctx][tok]; // token

            if (absCoef < CAT6_MIN_VAL) {
                numbits += default_bitcost_extraTotal[absCoef];
            } else {
                int32_t remCoef = absCoef - CAT6_MIN_VAL;
                for (int32_t e = 0; e < 14; e++, remCoef >>= 1)
                    numbits += default_bitcost_extra[DCT_VAL_CATEGORY6-DCT_VAL_CATEGORY1][13-e][remCoef & 1]; // extra bits
            }
        }
        nzc++;
        c++;
    }

    if (c < segEob) {
        int32_t pos = scan[c];
        int32_t ctx = GetCtxTokenAc<txType>(pos & maskPosX, pos >> shiftPosY, tokenCacheA, tokenCacheL);
        assert(ctx < PREV_COEF_CONTEXTS);
        numbits += bits.moreCoef[band[c]][ctx][0]; // no more_coef
    }
    return numbits;
}

__m128i get_base_context(__m128i l0, __m128i l1, __m128i l2, __m128i l3, __m128i l4, __m128i offset)
{
    const __m128i k0 = _mm_setzero_si128();
    const __m128i k4 = _mm_set1_epi8(4);
    __m128i sum;
    sum = _mm_add_epi8(l0, l1);
    sum = _mm_add_epi8(sum, l2);
    sum = _mm_add_epi8(sum, l3);
    sum = _mm_add_epi8(sum, l4);
    sum = _mm_avg_epu8(sum, k0); // sum = (sum + 1) >> 1
    sum = _mm_min_epu8(sum, k4);
    sum = _mm_add_epi8(sum, offset);
    return sum;
}

__m256i get_base_context_avx2(__m256i l0, __m256i l1, __m256i l2, __m256i l3, __m256i l4, __m256i offset)
{
    const __m256i k0 = _mm256_setzero_si256();
    const __m256i k4 = _mm256_set1_epi8(4);
    __m256i sum;
    sum = _mm256_add_epi8(_mm256_add_epi8(l0, l1),
                          _mm256_add_epi8(l2, l3));
    sum = _mm256_add_epi8(sum, l4);
    sum = _mm256_avg_epu8(sum, k0); // sum = (sum + 1) >> 1
    sum = _mm256_min_epu8(sum, k4);
    sum = _mm256_add_epi8(sum, offset);
    return sum;
}

__m128i get_br_context(__m128i l0, __m128i l1, __m128i l2, __m128i offset)
{
    const __m128i k0 = _mm_setzero_si128();
    const __m128i k6 = _mm_set1_epi8(6);
    __m128i sum = _mm_add_epi8(l0, l1);
    sum = _mm_add_epi8(sum, l2);
    sum = _mm_avg_epu8(sum, k0); // sum = (sum + 1) >> 1
    sum = _mm_min_epu8(sum, k6);
    sum = _mm_add_epi8(sum, offset);
    return sum;
}

__m256i get_br_context_avx2(__m256i l0, __m256i l1, __m256i l2, __m256i offset)
{
    const __m256i k0 = _mm256_setzero_si256();
    const __m256i k6 = _mm256_set1_epi8(6);
    __m256i sum = _mm256_add_epi8(l0, l1);
    sum = _mm256_add_epi8(sum, l2);
    sum = _mm256_avg_epu8(sum, k0); // sum = (sum + 1) >> 1
    sum = _mm256_min_epu8(sum, k6);
    sum = _mm256_add_epi8(sum, offset);
    return sum;
}

int32_t get_coeff_contexts_4x4(const int16_t *coeff, __m256i (&baseCtx)[2], __m256i (&brCtx)[2])//, uint8_t *baseCtx0, uint8_t *brCtx0)
{
    const __m128i kzero = _mm_setzero_si128();
    const __m128i k3 = _mm_set1_epi8(NUM_BASE_LEVELS + 1);
    const __m128i k15 = _mm_set1_epi8(MAX_BASE_BR_RANGE);

    __m128i l = _mm_abs_epi8(_mm_packs_epi16(loadu2_epi64(coeff + 0, coeff + 4),
                                             loadu2_epi64(coeff + 8, coeff + 12)));
    __m128i acc = _mm_sad_epu8(l, kzero);
    int32_t culLevel = std::min(COEFF_CONTEXT_MASK, _mm_cvtsi128_si32(acc) + _mm_extract_epi32(acc, 2));

    const __m128i br_ctx_offset = _mm_setr_epi8( 0,  7, 14, 14,
                                                 7,  7, 14, 14,
                                                14, 14, 14, 14,
                                                14, 14, 14, 14);

    const __m128i base_ctx_offset = _mm_setr_epi8( 0,  1,  6, 6,
                                                   1,  6,  6, 11,
                                                   6,  6, 11, 11,
                                                   6, 11, 11, 11);

    __m128i a, b, c, d, e, base_ctx, l_sat15;
    l = _mm_min_epu8(k15, l);
    a = _mm_srli_epi32(l, 8);   // + 1
    b = _mm_srli_si128(l, 4);   // + pitch
    c = _mm_srli_si128(a, 4);   // + 1 + pitch
    const __m128i t1 = get_br_context(a, b, c, br_ctx_offset);
//    storea_si128(brCtx0, t1);
    brCtx[0] = _mm256_set_m128i(kzero, t1);

    l_sat15 = l;
    l = _mm_min_epu8(k3, l);
    a = _mm_srli_epi32(l, 8);   // + 1
    b = _mm_srli_si128(l, 4);   // + pitch
    c = _mm_srli_si128(a, 4);   // + 1 + pitch
    d = _mm_srli_epi32(l, 16);  // + 2
    e = _mm_srli_si128(l, 8);   // + 2 * pitch
    base_ctx = get_base_context(a, b, c, d, e, base_ctx_offset);
    base_ctx = _mm_slli_epi16(base_ctx, 4);
    base_ctx = _mm_or_si128(base_ctx, l_sat15);
//    storea_si128(baseCtx0, base_ctx);
    baseCtx[0] = _mm256_set_m128i(kzero, base_ctx);

    return culLevel;
}

alignas(32) static const uint8_t tab_base_ctx_offset[64] = {
     0,  1,  6,  6, 11, 11, 11, 11,
     1,  6,  6, 11, 11, 11, 11, 11,
     6,  6, 11, 11, 11, 11, 11, 11,
     6, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11
};

alignas(32) static const uint8_t tab_br_ctx_offset[64] = {
     0,  7, 14, 14, 14, 14, 14, 14,
     7,  7, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14
};

int32_t get_coeff_contexts_8x8(const int16_t *coeff, __m256i (&baseCtx)[2], __m256i (&brCtx)[2])//, uint8_t *baseCtx0, uint8_t *brCtx0)
{
    const __m256i kzero = _mm256_setzero_si256();
    const __m256i k3 = _mm256_set1_epi8(NUM_BASE_LEVELS + 1);
    const __m256i k15 = _mm256_set1_epi8(MAX_BASE_BR_RANGE);

    const __m256i br_ctx_offset0 = loada_si256(tab_br_ctx_offset + 0);
    const __m256i br_ctx_offset1 = loada_si256(tab_br_ctx_offset + 32);
    const __m256i base_ctx_offset0 = loada_si256(tab_base_ctx_offset + 0);
    const __m256i base_ctx_offset1 = loada_si256(tab_base_ctx_offset + 32);

    __m256i rows0123 = _mm256_abs_epi8(_mm256_packs_epi16(loada_si256(coeff + 0 * 8), loada_si256(coeff + 2 * 8)));
    __m256i rows4567 = _mm256_abs_epi8(_mm256_packs_epi16(loada_si256(coeff + 4 * 8), loada_si256(coeff + 6 * 8)));
    __m256i sum = _mm256_adds_epu8(rows0123, rows4567);
    sum = _mm256_sad_epu8(sum, kzero);

    rows0123 = permute4x64(rows0123, 0, 2, 1, 3);
    rows4567 = permute4x64(rows4567, 0, 2, 1, 3);
    rows0123 = _mm256_min_epu8(k15, rows0123);
    rows4567 = _mm256_min_epu8(k15, rows4567);

    __m256i base_ctx;
    __m256i a, b, c, d, e, tmp;

    tmp = _mm256_blend_epi32(rows0123, rows4567, 3);  // row4 row1 row2 row3
    a = _mm256_srli_epi64(rows0123, 8); // + 1
    b = permute4x64(tmp, 1, 2, 3, 0);   // + pitch
    c = _mm256_srli_epi64(b, 8);        // + 1 + pitch
    const __m256i tmp0 = get_br_context_avx2(a, b, c, br_ctx_offset0);
    //storea_si256(brCtx0, tmp0);
    brCtx[0] = get_br_context_avx2(a, b, c, br_ctx_offset0);

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    const __m256i tmp1 = get_br_context_avx2(a, b, c, br_ctx_offset1);
    //storea_si256(brCtx0 + 32, tmp1);
    brCtx[1] = get_br_context_avx2(a, b, c, br_ctx_offset1);

    __m256i rows0123_sat15 = rows0123;
    __m256i rows4567_sat15 = rows4567;
    rows0123 = _mm256_min_epu8(k3, rows0123);
    rows4567 = _mm256_min_epu8(k3, rows4567);

    tmp = _mm256_blend_epi32(rows0123, rows4567, 3);  // row4 row1 row2 row3
    a = _mm256_srli_epi64(rows0123, 8);         // + 1
    b = permute4x64(tmp, 1, 2, 3, 0);           // + pitch
    c = _mm256_srli_epi64(b, 8);                // + 1 + pitch
    d = _mm256_srli_epi64(rows0123, 16);        // + 2
    e = permute2x128(rows0123, rows4567, 1, 2); // + 2 * pitch
    base_ctx = get_base_context_avx2(a, b, c, d, e, base_ctx_offset0);
    base_ctx = _mm256_slli_epi16(base_ctx, 4);
    base_ctx = _mm256_or_si256(base_ctx, rows0123_sat15);
    baseCtx[0] = base_ctx;
    //storea_si256(baseCtx0, base_ctx);

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    d = _mm256_srli_epi64(rows4567, 16);
    e = permute2x128(rows4567, rows4567, 1, 8);
    base_ctx = get_base_context_avx2(a, b, c, d, e, base_ctx_offset1);
    base_ctx = _mm256_slli_epi16(base_ctx, 4);
    base_ctx = _mm256_or_si256(base_ctx, rows4567_sat15);
    baseCtx[1] = base_ctx;
    //storea_si256(baseCtx0 + 32, base_ctx);

    __m128i sum128 = _mm_add_epi64(si128_lo(sum), si128_hi(sum));
    int32_t culLevel = _mm_cvtsi128_si32(sum128) + _mm_extract_epi32(sum128, 2);
    culLevel = std::min(COEFF_CONTEXT_MASK, culLevel);
    return culLevel;
}

int32_t get_coeff_contexts_16x16(const int16_t *coeff, __m256i (&baseCtx)[2], __m256i (&brCtx)[2], int txSize)//, uint8_t *baseCtx0, uint8_t *brCtx0)
{
    const __m256i kzero = _mm256_setzero_si256();
    const __m256i k3 = _mm256_set1_epi8(NUM_BASE_LEVELS + 1);
    const __m256i k15 = _mm256_set1_epi8(MAX_BASE_BR_RANGE);

    const __m256i br_ctx_offset0 = loada_si256(tab_br_ctx_offset + 0);
    const __m256i br_ctx_offset1 = loada_si256(tab_br_ctx_offset + 32);
    const __m256i base_ctx_offset0 = loada_si256(tab_base_ctx_offset + 0);
    const __m256i base_ctx_offset1 = loada_si256(tab_base_ctx_offset + 32);

    const int32_t pitch = 4 << txSize;
    __m256i rows0123 = _mm256_abs_epi8(_mm256_packs_epi16(loada2_m128i(coeff + 0 * pitch, coeff + 2 * pitch),
                                                          loada2_m128i(coeff + 1 * pitch, coeff + 3 * pitch)));
    __m256i rows4567 = _mm256_abs_epi8(_mm256_packs_epi16(loada2_m128i(coeff + 4 * pitch, coeff + 6 * pitch),
                                                          loada2_m128i(coeff + 5 * pitch, coeff + 7 * pitch)));
    __m256i sum = _mm256_adds_epu8(rows0123, rows4567);
    sum = _mm256_sad_epu8(sum, kzero);

    rows0123 = _mm256_min_epu8(k15, rows0123);
    rows4567 = _mm256_min_epu8(k15, rows4567);

    __m256i base_ctx;
    __m256i a, b, c, d, e, tmp;

    tmp = _mm256_blend_epi32(rows0123, rows4567, 3);  // row4 row1 row2 row3
    a = _mm256_srli_epi64(rows0123, 8); // + 1
    b = permute4x64(tmp, 1, 2, 3, 0);   // + pitch
    c = _mm256_srli_epi64(b, 8);        // + 1 + pitch
    const __m256i tmp0 = get_br_context_avx2(a, b, c, br_ctx_offset0);
    //storea_si256(brCtx0, tmp0);
    brCtx[0] = tmp0;

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    const __m256i tmp1 = get_br_context_avx2(a, b, c, br_ctx_offset1);
    //storea_si256(brCtx0 + 32, tmp1);
    brCtx[1] = tmp1;

    __m256i rows0123_sat15 = rows0123;
    __m256i rows4567_sat15 = rows4567;
    rows0123 = _mm256_min_epu8(k3, rows0123);
    rows4567 = _mm256_min_epu8(k3, rows4567);

    tmp = _mm256_blend_epi32(rows0123, rows4567, 3);  // row4 row1 row2 row3
    a = _mm256_srli_epi64(rows0123, 8);         // + 1
    b = permute4x64(tmp, 1, 2, 3, 0);           // + pitch
    c = _mm256_srli_epi64(b, 8);                // + 1 + pitch
    d = _mm256_srli_epi64(rows0123, 16);        // + 2
    e = permute2x128(rows0123, rows4567, 1, 2); // + 2 * pitch
    base_ctx = get_base_context_avx2(a, b, c, d, e, base_ctx_offset0);
    base_ctx = _mm256_slli_epi16(base_ctx, 4);
    base_ctx = _mm256_or_si256(base_ctx, rows0123_sat15);
    //storea_si256(baseCtx0, base_ctx);
    baseCtx[0] = base_ctx;

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    d = _mm256_srli_epi64(rows4567, 16);
    e = permute2x128(rows4567, rows4567, 1, 8);
    base_ctx = get_base_context_avx2(a, b, c, d, e, base_ctx_offset1);
    base_ctx = _mm256_slli_epi16(base_ctx, 4);
    base_ctx = _mm256_or_si256(base_ctx, rows4567_sat15);
    //storea_si256(baseCtx0 + 32, base_ctx);
    baseCtx[1] = base_ctx;

    __m128i sum128 = _mm_add_epi64(si128_lo(sum), si128_hi(sum));
    int32_t culLevel = _mm_cvtsi128_si32(sum128) + _mm_extract_epi32(sum128, 2);
    culLevel = std::min(COEFF_CONTEXT_MASK, culLevel);
    return culLevel;
}

//static volatile unsigned lock = 0;

// bits = a * BSR(level + 1) + b
static const int32_t HIGH_FREQ_COEFF_FIT_CURVE[TOKEN_CDF_Q_CTXS][TX_32X32 + 1][PLANE_TYPES][2] = {
    { // q <= 20
        //     luma          chroma
        { {  895, 240 }, {  991, 170 } }, // TX_4X4
        { {  895, 240 }, {  991, 170 } }, // TX_8X8
        { { 1031, 177 }, { 1110, 125 } }, // TX_16X16
        { { 1005, 192 }, { 1559,  72 } }, // TX_32X32
    },
    { // q <= 60
        { {  895, 240 }, {  991, 170 } }, // TX_4X4
        { {  895, 240 }, {  991, 170 } }, // TX_8X8
        { { 1031, 177 }, { 1110, 125 } }, // TX_16X16
        { { 1005, 192 }, { 1559,  72 } }, // TX_32X32
    },
    { // q <= 120
        { {  895, 240 }, {  991, 170 } }, // TX_4X4
        { {  895, 240 }, {  991, 170 } }, // TX_8X8
        { { 1031, 177 }, { 1110, 125 } }, // TX_16X16
        { { 1005, 192 }, { 1559,  72 } }, // TX_32X32
    },
    { // q > 120
        { {  882, 203 }, { 1273, 100 } }, // TX_4X4
        { {  882, 203 }, { 1273, 100 } }, // TX_8X8
        { { 1094, 132 }, { 1527,  69 } }, // TX_16X16
        { { 1234, 102 }, { 1837,  61 } }, // TX_32X32
    },
};

alignas(64) static const int8_t av1_default_scan_4x4_b[16] = {
    0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

alignas(64) static const int8_t av1_default_scan_8x8_b[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

alignas(64) const int8_t av1_default_scan_4x4_rb[16] = {
0, 1, 5, 6, 2, 4, 7, 12, 3, 8, 11, 13, 9, 10, 14, 15, };

alignas(64) const int8_t av1_default_scan_8x8_rb[64] = {
0, 1, 5, 6, 14, 15, 27, 28, 2, 4, 7, 13, 16, 26, 29, 42, 3,
8, 12, 17, 25, 30, 41, 43, 9, 11, 18, 24, 31, 40, 44, 53, 10,
19, 23, 32, 39, 45, 52, 54, 20, 22, 33, 38, 46, 51, 55, 60, 21,
34, 37, 47, 50, 56, 59, 61, 35, 36, 48, 49, 57, 58, 62, 63, };

uint32_t AV1Enc::EstimateCoefsAv1(const TxbBitCounts &bc, int32_t txbSkipCtx, int32_t numNonZero,
    const CoeffsType *coeffs, int32_t *culLevel)
{
    assert(numNonZero > 0);
    const uint32_t EffNum = 118; // Average efficiency of estimate.
    uint32_t bits = 0;
    bits += bc.txbSkip[txbSkipCtx][0]; // non skip
    bits += 512 * numNonZero; // signs

    if (numNonZero == 1 && coeffs[0] != 0) {
        const int32_t level = abs(coeffs[0]);
        *culLevel = std::min(COEFF_CONTEXT_MASK, level);
        bits += bc.eobMulti[0]; // eob
        bits += bc.coeffBaseEob[0][std::min(3, level) - 1];
        bits += bc.coeffBrAcc[0][std::min(15, level)];
        return ((bits * EffNum) >> 7);
    }

#if 0
    alignas(64) uint8_t base_contexts[8 * 8];
    alignas(64) uint8_t br_contexts[8 * 8];
#else
    __m256i base_contexts[2];
    __m256i br_contexts[2];
//    alignas(64) uint8_t base_contexts_o[8 * 8];
//    alignas(64) uint8_t br_contexts_o[8 * 8];
#endif

    *culLevel = (bc.txSize == TX_4X4)
        ? get_coeff_contexts_4x4(coeffs, base_contexts, br_contexts)//, base_contexts_o, br_contexts_o)
        : (bc.txSize == TX_8X8)
        ? get_coeff_contexts_8x8(coeffs, base_contexts, br_contexts)//, base_contexts_o, br_contexts_o)
        : get_coeff_contexts_16x16(coeffs, base_contexts, br_contexts, bc.txSize);//, base_contexts_o, br_contexts_o);

    const int32_t level = _mm_cvtsi128_si32(si128_lo(base_contexts[0])) & 15;
    bits += bc.coeffBase[0][(level < 3) ? level : 3];
    bits += bc.coeffBrAcc[_mm_cvtsi128_si32(si128_lo(br_contexts[0]))&0xff][level];
    if (level)
        --numNonZero;
    assert(numNonZero > 0);

    //const int16_t *scan = av1_default_scan[bc.txSize != TX_4X4];
    const int8_t* scan = bc.txSize == TX_4X4 ? av1_default_scan_4x4_b : av1_default_scan_8x8_b;
    const int8_t* scan_e = scan + 36;

    scan++;
    __m128i baseC = _mm_setzero_si128();
    __m128i brC = _mm_setzero_si128();
    if (bc.txSize == TX_4X4) {
#if 0
        const int8_t* scn = av1_default_scan_4x4_rb;
        const unsigned int mu3 = 0x03030303;
        const unsigned int muF = 0x0F0F0F0F;
        const __m128i mask0f = _mm_set_epi32(muF, muF, muF, muF);
        const __m128i zero = _mm_setzero_si128();
        const __m128i yff = _mm_set1_epi16(0xFFFF);
        const __m128i mask3 = _mm_set_epi32(mu3, mu3, mu3, mu3);
        const __m256i zero256 = _mm256_setzero_si256();
        const __m128i one = _mm_set1_epi8(1);
        const __m128i mask2 = _mm_set1_epi8(2);
        const __m128i y50 = _mm_set1_epi8(50);

        const __m256i yff00 = _mm256_set1_epi16(0x00ff);
        const __m128i yff00_128 = _mm_set1_epi16(0x00ff);

        __m256i base = _mm256_load_si256((__m256i*)(bc.coeffBaseT[0])); //16 16bit values
        __m256i base_lohi = _mm256_packus_epi16(_mm256_and_si256(base, yff00), _mm256_srli_epi16(base, 8));
        base_lohi = _mm256_permute4x64_epi64(base_lohi, 0b11011000);

        __m128i bcont = _mm_load_si128((__m128i*)base_contexts);
        __m128i levels = _mm_and_si128(bcont, mask0f);
        bcont = _mm_srli_epi16(bcont, 4);
        bcont = _mm_and_si128(bcont, mask0f);

        __m128i scan_sse = _mm_load_si128((__m128i*)(scn));

        __m128i maskz = _mm_cmpeq_epi8(levels, zero); //zero level mask
        const __m128i mask_nz = _mm_xor_si128(yff, maskz);

        __m128i  mscan = _mm_and_si128(mask_nz, scan_sse);
        mscan = _mm_max_epi8(mscan, _mm_srli_si128(mscan, 1));
        mscan = _mm_andnot_si128(mscan, yff00_128); //make 16 bit
        mscan = _mm_minpos_epu16(mscan);
        mscan = _mm_andnot_si128(mscan, _mm_setr_epi16(0x00ff, 0, 0, 0, 0, 0, 0, 0));
        int maxs = _mm_cvtsi128_si32(mscan);

        const unsigned int c0 = _mm_movemask_epi8(mask_nz) & 0xfffffffe;
        const int num_nz_coeffs = _mm_popcnt_u32(c0); //- coeff at 0
        numNonZero -= num_nz_coeffs;

        __m128i smask = _mm_cmpgt_epi8(_mm_set1_epi8(maxs), scan_sse);
        smask = _mm_insert_epi8(smask, 0, 0);
        scan_sse = _mm_and_si128(scan_sse, smask);

        scan += !numNonZero ? maxs-1 : maxs;
        maskz = _mm_and_si128(maskz, smask);  // ????

        __m256i bcont2 = _mm256_insertf128_si256(_mm256_castsi128_si256(bcont), (bcont), 1);
        __m256i maskz2 = _mm256_insertf128_si256(_mm256_castsi128_si256(maskz), (maskz), 1);

        __m256i bits_lohi = _mm256_shuffle_epi8(base_lohi, bcont2);
        bits_lohi = _mm256_blendv_epi8(zero256, bits_lohi, maskz2);
        __m256i  bits0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi, 0b11110110));  //16-bit bit for zero coeffs level = 0

        const __m128i levels03 = _mm_and_si128(smask, _mm_min_epi8(levels, mask3));

        __m128i lev = one;
        for (int i = 1; i < 4; i++, lev = _mm_add_epi8(lev, one)) { //nz levels
            const __m128i maskL0 = _mm_cmpeq_epi8(levels03, lev); //level mask
            if(_mm_testz_si128(maskL0, yff)) continue;

            base = _mm256_load_si256((__m256i*)(bc.coeffBaseT[i])); //16 16bit values
            base_lohi = _mm256_packus_epi16(_mm256_and_si256(base, yff00), _mm256_srli_epi16(base, 8));
            base_lohi = _mm256_permute4x64_epi64(base_lohi, 0b11011000);

            __m256i mask0 = _mm256_insertf128_si256(_mm256_castsi128_si256(maskL0), (maskL0), 1);
            bits_lohi = _mm256_shuffle_epi8(base_lohi, bcont2);
            bits_lohi = _mm256_blendv_epi8(zero256, bits_lohi, mask0);
            const __m256i tmp0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi, 0b11110110));  //16-bit bit for zero coeffs level = 0
            bits0 = _mm256_add_epi16(bits0, tmp0);
        }

        __m128i brcont0 = _mm_load_si128((__m128i*)br_contexts);

        //get only valid context level != 0 and in scan
        //reset levels 0,1,2 <= zero bitcount
        __m128i m03 = _mm_cmpgt_epi8(levels, mask2);
        m03 = _mm_and_si128(smask, m03);
        brcont0 = _mm_blendv_epi8(y50, brcont0, m03);
        //        brcont0 = _mm256_blendv_epi8(yff, brcont0, mask_nz0);

        //get min
        __m128i minbr128 = _mm_min_epi8(brcont0, _mm_srli_si128(brcont0, 1));
        minbr128 = _mm_and_si128(yff00_128, minbr128); //make 16 bit
        minbr128 = _mm_minpos_epu16(minbr128);
        minbr128 = _mm_and_si128(minbr128, _mm_setr_epi16(0xffff, 0, 0, 0, 0, 0, 0, 0));
        const int min_br = _mm_cvtsi128_si32(minbr128);
        if (min_br < 21) {
            //get max
            __m128i maxbr128 = _mm_andnot_si128(_mm_cmpeq_epi8(brcont0, y50), brcont0);
            maxbr128 = _mm_max_epi8(maxbr128, _mm_srli_si128(maxbr128, 1));
            maxbr128 = _mm_andnot_si128(maxbr128, yff00_128); //make 16 bit
            maxbr128 = _mm_minpos_epu16(maxbr128);
            maxbr128 = _mm_andnot_si128(maxbr128, _mm_setr_epi16(0x00ff, 0, 0, 0, 0, 0, 0, 0));
            const int max_br = _mm_cvtsi128_si32(maxbr128) + 1;

            __m128i cont = _mm_broadcastb_epi8(minbr128);
            for (int i = min_br; i < max_br; i++, cont = _mm_add_epi8(cont, one)) {
                //test for zeros
                const __m128i mask = _mm_cmpeq_epi8(brcont0, cont);
                if(_mm_testz_si128(mask, yff)) continue;

                __m256i brbase = _mm256_load_si256((__m256i*)(bc.coeffBrAcc[i])); //16 16bit values
                base_lohi = _mm256_packus_epi16(_mm256_and_si256(brbase, yff00), _mm256_srli_epi16(brbase, 8));
                base_lohi = _mm256_permute4x64_epi64(base_lohi, 0b11011000);

                __m256i levels0 = _mm256_insertf128_si256(_mm256_castsi128_si256(levels), (levels), 1);
                __m256i mask0 = _mm256_insertf128_si256(_mm256_castsi128_si256(mask), (mask), 1);
                bits_lohi = _mm256_shuffle_epi8(base_lohi, levels0);
                bits_lohi = _mm256_blendv_epi8(zero256, bits_lohi, _mm256_permute4x64_epi64(mask0, 0b01000100));
                const __m256i tmp0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi, 0b11110110));  //16-bit bit for zero coeffs level = 0
                bits0 = _mm256_add_epi16(bits0, tmp0);
            }
        }
        //sum up for final result
        __m128i sum128 = _mm_add_epi16(si128_lo(bits0), si128_hi(bits0));
        sum128 = _mm_add_epi32(_mm_srli_epi32(sum128, 16), _mm_and_si128(sum128, _mm_set1_epi32(0x0000ffff)));
        sum128 = _mm_add_epi32(sum128, _mm_srli_epi64(sum128, 32));
        sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 8));
        bits += _mm_cvtsi128_si32(sum128);
#else
#if 0
    for (; scan < scan_e; scan++) {
        const int pos = *scan;
        const uint8_t bcont = base_contexts[pos];
        const int level = bcont & 15;
        if (!level) {
            bits += bc.coeffBaseT[0][bcont >> 4];
            continue;
        }

        if (--numNonZero == 0) break;

        bits += bc.coeffBase[bcont >> 4][level < 3 ? level : 3];
        bits += bc.coeffBrAcc[br_contexts[pos]][level];
            }

#else
        const __m128i sc = _mm_set_epi8(15,14,11,7,10,13,12,9,6,3,2,5,8,4,1,0);
        baseC = _mm_srli_si128(_mm_shuffle_epi8(si128_lo(base_contexts[0]), sc),1);
        brC = _mm_srli_si128(_mm_shuffle_epi8(si128_lo(br_contexts[0]), sc),1);
        for (; scan < scan_e; scan++) {
            //const int pos = *scan;
            //const uint8_t bcont = base_contexts[pos];
            const uint8_t bcont = _mm_cvtsi128_si32(baseC) & 0xff;
            const int lev = bcont & 15;

            if (lev) {
                if (--numNonZero == 0) break;
                bits += bc.coeffBrAcc[_mm_cvtsi128_si32(brC) & 0xff][lev];
            }
            baseC = _mm_srli_si128(baseC, 1);
            brC = _mm_srli_si128(brC, 1);

            bits += bc.coeffBaseT[lev < 3 ? lev : 3][bcont >> 4];
        }
#endif
#endif
    }
    else
    {
        const int8_t* scn = av1_default_scan_8x8_rb;
        const unsigned int mu3 = 0x03030303;
        const unsigned int muF = 0x0F0F0F0F;
        const __m256i mask = _mm256_set_epi32(muF, muF, muF, muF, muF, muF, muF, muF);
        const __m256i mask3 = _mm256_set_epi32(mu3, mu3, mu3, mu3, mu3, mu3, mu3, mu3);
        const __m256i mask2 = _mm256_set1_epi8(2);
        const __m256i zero = _mm256_setzero_si256();
        const __m256i one = _mm256_set1_epi8(1);
        const __m256i y08 = _mm256_set1_epi8(8);
        const __m256i yff = _mm256_set1_epi16(-1);
        const __m256i yff00 = _mm256_set1_epi16(0x00ff);
        const __m256i y50 = _mm256_set1_epi8(50);
        const __m128i yff00_128 = _mm_set1_epi16(0x00ff);
        const __m128i ff_128 = _mm_setr_epi16(0x00ff, 0, 0, 0, 0, 0, 0, 0);

        __m256i base = _mm256_load_si256((__m256i*)(bc.coeffBaseT[0])); //16 16bit values
        __m256i base_lohi = _mm256_packus_epi16(_mm256_and_si256(base, yff00), _mm256_srli_epi16(base, 8));
        base_lohi = _mm256_permute4x64_epi64(base_lohi, 0b11011000);

        //__m256i bcont0 = _mm256_load_si256((__m256i*)base_contexts);
        __m256i bcont0 = base_contexts[0];
        __m256i levels0 = _mm256_and_si256(bcont0, mask);
        //__m256i bcont1 = _mm256_load_si256((__m256i*)(base_contexts + 32));
        __m256i bcont1 = base_contexts[1];
        __m256i levels1 = _mm256_and_si256(bcont1, mask);
        bcont0 = _mm256_srli_epi16(bcont0, 4);
        bcont0 = _mm256_and_si256(bcont0, mask);
        bcont1 = _mm256_srli_epi16(bcont1, 4);
        bcont1 = _mm256_and_si256(bcont1, mask);

        __m256i scan0 = _mm256_load_si256((__m256i*)(scn));
        __m256i scan1 = _mm256_load_si256((__m256i*)(scn + 32));

        __m256i maskz0 = _mm256_cmpeq_epi8(levels0, zero); //zero level mask
        __m256i maskz1 = _mm256_cmpeq_epi8(levels1, zero); //zero level mask
        const __m256i mask_nz0 = _mm256_xor_si256(yff, maskz0);
        const __m256i mask_nz1 = _mm256_xor_si256(yff, maskz1);

        __m256i ymax = _mm256_set1_epi8(36);
        __m256i smask1 = _mm256_cmpgt_epi8(ymax, scan1);
        __m256i smask0 = _mm256_cmpgt_epi8(ymax, scan0);

        __m256i max_scan = _mm256_max_epi8(_mm256_and_si256(mask_nz0, scan0), _mm256_and_si256(mask_nz1, scan1));

        __m128i mscan = _mm_max_epi8(si128_lo(max_scan), si128_hi(max_scan));
        mscan = _mm_max_epi8(mscan, _mm_srli_si128(mscan, 1));
        mscan = _mm_andnot_si128(mscan, yff00_128); //make 16 bit
        mscan = _mm_minpos_epu16(mscan);
        mscan = _mm_andnot_si128(mscan, ff_128);

        int maxs = _mm_cvtsi128_si32(mscan);

        const unsigned int c0 = _mm256_movemask_epi8(_mm256_and_si256(mask_nz0, smask0)) & 0xfffffffe;
        const unsigned int c1 = _mm256_movemask_epi8(_mm256_and_si256(mask_nz1, smask1));
        const int num_nz_coeffs = _mm_popcnt_u32(c0) + _mm_popcnt_u32(c1); //- coeff at 0
        numNonZero -= num_nz_coeffs;
//        if (nnz - num_nz_coeffs != numNonZero)
//            fprintf(stderr, "wrong nnz\n");

        __m256i brcont0 = br_contexts[0];
        __m256i brcont1 = br_contexts[1];
        maxs = (maxs < 35) ? (numNonZero ? 35 :maxs) : 35;
        if (!numNonZero) {
            maxs--;
            __m256i mscan_mask = _mm256_broadcastb_epi8(mscan);
            __m256i sm_0 = _mm256_cmpeq_epi8(mscan_mask, scan0);
            __m256i sm_1 = _mm256_cmpeq_epi8(mscan_mask, scan1);
            __m256i v_0 = _mm256_and_si256(base_contexts[0], sm_0);
            __m256i v_1 = _mm256_and_si256(base_contexts[1], sm_1);
            __m256i v = _mm256_or_si256(v_0, v_1);
            __m128i vv = _mm_max_epu8(si128_lo(v), si128_hi(v));
            vv = _mm_max_epu8(vv, _mm_srli_si128(vv, 1));
            vv = _mm_andnot_si128(vv, yff00_128);
            vv = _mm_minpos_epu16(vv);
            baseC = _mm_andnot_si128(vv, ff_128);

            v_0 = _mm256_and_si256(brcont0, sm_0);
            v_1 = _mm256_and_si256(brcont1, sm_1);
            v = _mm256_or_si256(v_0, v_1);
            vv = _mm_max_epu8(si128_lo(v), si128_hi(v));
            vv = _mm_max_epu8(vv, _mm_srli_si128(vv, 1));
            vv = _mm_andnot_si128(vv, yff00_128);
            vv = _mm_minpos_epu16(vv);
            brC = _mm_andnot_si128(vv, ff_128);
        }

        ymax = _mm256_set1_epi8(int8_t(maxs + 1));
        smask1 = _mm256_cmpgt_epi8(ymax, scan1);
        scan1 = _mm256_and_si256(scan1, smask1); // ????
        smask0 = _mm256_cmpgt_epi8(ymax, scan0);
        smask0 = _mm256_insert_epi8(smask0, 0, 0);
        scan0 = _mm256_and_si256(scan0, smask0);  // ????

        scan += maxs;
        //        if (sc + maxs != scan)
//            fprintf(stderr, "wrong scan\n");
        maskz0 = _mm256_and_si256(maskz0, smask0);  // ????
        maskz1 = _mm256_and_si256(maskz1, smask1);  // ????

        const __m256i bcont0m0 = _mm256_permute4x64_epi64(bcont0, 0b01000100);
        const __m256i bcont0m1 = _mm256_permute4x64_epi64(bcont0, 0b11101110);
        __m256i bits_lohi_lo = _mm256_shuffle_epi8(base_lohi, bcont0m0);
        __m256i bits_lohi_hi = _mm256_shuffle_epi8(base_lohi, bcont0m1);
        bits_lohi_lo = _mm256_blendv_epi8(zero, bits_lohi_lo, _mm256_permute4x64_epi64(maskz0, 0b01000100));
        bits_lohi_hi = _mm256_blendv_epi8(zero, bits_lohi_hi, _mm256_permute4x64_epi64(maskz0, 0b11101110));
        __m256i  bits0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_lo, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_lo, 0b11110110));  //16-bit bit for zero coeffs level = 0
        __m256i  bits1 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_hi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_hi, 0b11110110));  //16-bit bit for zero coeffs level = 0

        const __m256i bcont1m0 = _mm256_permute4x64_epi64(bcont1, 0b01000100);
        const __m256i bcont1m1 = _mm256_permute4x64_epi64(bcont1, 0b11101110);
        bits_lohi_lo = _mm256_shuffle_epi8(base_lohi, bcont1m0);
        bits_lohi_hi = _mm256_shuffle_epi8(base_lohi, bcont1m1);
        bits_lohi_lo = _mm256_blendv_epi8(zero, bits_lohi_lo, _mm256_permute4x64_epi64(maskz1, 0b01000100));
        bits_lohi_hi = _mm256_blendv_epi8(zero, bits_lohi_hi, _mm256_permute4x64_epi64(maskz1, 0b11101110));
        __m256i tmp0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_lo, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_lo, 0b11110110));  //16-bit bit for zero coeffs level = 0
        __m256i tmp1 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_hi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_hi, 0b11110110));  //16-bit bit for zero coeffs level = 0
        bits0 = _mm256_add_epi16(bits0, tmp0);
        bits1 = _mm256_add_epi16(bits1, tmp1);

        const __m256i levels03 = _mm256_and_si256(smask0, _mm256_min_epi8(levels0, mask3));
        const __m256i levels13 = _mm256_and_si256(smask1, _mm256_min_epi8(levels1, mask3));
        __m256i lev = one;
        for (int i = 1; i < 4; i++, lev = _mm256_add_epi8(lev, one)) { //nz levels
            __m256i maskL0 = _mm256_cmpeq_epi8(levels03, lev); //level mask
            const int z0 = _mm256_testz_si256(maskL0, yff);
            __m256i maskL1 = _mm256_cmpeq_epi8(levels13, lev); //level mask
            const int z1 = _mm256_testz_si256(maskL1, yff);
            if (z0 & z1) continue;

            base = _mm256_load_si256((__m256i*)(bc.coeffBaseT[i])); //16 16bit values
            base_lohi = _mm256_packus_epi16(_mm256_and_si256(base, yff00), _mm256_srli_epi16(base, 8));
            base_lohi = _mm256_permute4x64_epi64(base_lohi, 0b11011000);

            if (!z0) {
                bits_lohi_lo = _mm256_shuffle_epi8(base_lohi, bcont0m0);
                bits_lohi_hi = _mm256_shuffle_epi8(base_lohi, bcont0m1);
                bits_lohi_lo = _mm256_blendv_epi8(zero, bits_lohi_lo, _mm256_permute4x64_epi64(maskL0, 0b01000100));
                bits_lohi_hi = _mm256_blendv_epi8(zero, bits_lohi_hi, _mm256_permute4x64_epi64(maskL0, 0b11101110));
                tmp0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_lo, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_lo, 0b11110110));  //16-bit bit for zero coeffs level = 0
                tmp1 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_hi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_hi, 0b11110110));  //16-bit bit for zero coeffs level = 0
                bits0 = _mm256_add_epi16(bits0, tmp0);
                bits1 = _mm256_add_epi16(bits1, tmp1);
            }

            if (!z1) {
                bits_lohi_lo = _mm256_shuffle_epi8(base_lohi, bcont1m0);
                bits_lohi_hi = _mm256_shuffle_epi8(base_lohi, bcont1m1);
                bits_lohi_lo = _mm256_blendv_epi8(zero, bits_lohi_lo, _mm256_permute4x64_epi64(maskL1, 0b01000100));
                bits_lohi_hi = _mm256_blendv_epi8(zero, bits_lohi_hi, _mm256_permute4x64_epi64(maskL1, 0b11101110));
                const __m256i tmp01 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_lo, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_lo, 0b11110110));  //16-bit bit for zero coeffs level = 0
                const __m256i tmp11 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_hi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_hi, 0b11110110));  //16-bit bit for zero coeffs level = 0
                bits0 = _mm256_add_epi16(bits0, tmp01);
                bits1 = _mm256_add_epi16(bits1, tmp11);
            }
        }

//        __m256i brcont0 = _mm256_load_si256((__m256i*)br_contexts);
//        __m256i brcont1 = _mm256_load_si256((__m256i*)(br_contexts + 32));

        //get only valid context level != 0 and in scan
        //reset levels 0,1,2 <= zero bitcount
        __m256i m03 = _mm256_cmpgt_epi8(levels0, mask2);
        __m256i m13 = _mm256_cmpgt_epi8(levels1, mask2);
        brcont0 = _mm256_blendv_epi8(y50, brcont0, _mm256_and_si256(mask_nz0, m03));
//        brcont0 = _mm256_blendv_epi8(yff, brcont0, mask_nz0);
        brcont0 = _mm256_blendv_epi8(y50, brcont0, smask0);
        brcont1 = _mm256_blendv_epi8(y50, brcont1, _mm256_and_si256(mask_nz1, m13));
//        brcont1 = _mm256_blendv_epi8(yff, brcont1, mask_nz1);
        brcont1 = _mm256_blendv_epi8(y50, brcont1, smask1);

        //get min
        __m256i minbr = _mm256_min_epi8(brcont0, brcont1);
        __m128i minbr128 = _mm_min_epi8(si128_lo(minbr), si128_hi(minbr));
        minbr128 = _mm_min_epi8(minbr128, _mm_srli_si128(minbr128, 1));
        minbr128 = _mm_and_si128(yff00_128, minbr128); //make 16 bit
        minbr128 = _mm_minpos_epu16(minbr128);
        minbr128 = _mm_and_si128(minbr128, _mm_setr_epi16(-1, 0, 0, 0, 0, 0, 0, 0));
        const int min_br = _mm_cvtsi128_si32(minbr128);
        if (min_br < 21) {
            //get max
            const __m256i brcont0m = _mm256_andnot_si256(_mm256_cmpeq_epi8(brcont0, y50), brcont0);
            const __m256i brcont1m = _mm256_andnot_si256(_mm256_cmpeq_epi8(brcont1, y50), brcont1);
            const __m256i maxbr = _mm256_max_epi8(brcont0m, brcont1m);
            __m128i maxbr128 = _mm_max_epi8(si128_lo(maxbr), si128_hi(maxbr));
            maxbr128 = _mm_max_epi8(maxbr128, _mm_srli_si128(maxbr128, 1));
            maxbr128 = _mm_andnot_si128(maxbr128, yff00_128); //make 16 bit
            maxbr128 = _mm_minpos_epu16(maxbr128);
            maxbr128 = _mm_andnot_si128(maxbr128, _mm_setr_epi16(0x00ff, 0, 0, 0, 0, 0, 0, 0));
            const int max_br = _mm_cvtsi128_si32(maxbr128) + 1;

            const __m256i levels00 = _mm256_permute4x64_epi64(levels0, 0b01000100);
            const __m256i levels01 = _mm256_permute4x64_epi64(levels0, 0b11101110);
            const __m256i levels10 = _mm256_permute4x64_epi64(levels1, 0b01000100);
            const __m256i levels11 = _mm256_permute4x64_epi64(levels1, 0b11101110);
            __m256i cont = _mm256_broadcastb_epi8(minbr128);
            for (int i = min_br; i < max_br; i++, cont = _mm256_add_epi8(cont, one)) {
                //test for zeros
                const __m256i mask0 = _mm256_cmpeq_epi8(brcont0, cont);
                const int z0 = _mm256_testz_si256(mask0, yff);
                const __m256i mask1 = _mm256_cmpeq_epi8(brcont1, cont);
                const int z1 = _mm256_testz_si256(mask1, yff);
                if (z0 & z1) continue;

                __m256i brbase = _mm256_load_si256((__m256i*)(bc.coeffBrAcc[i])); //16 16bit values
                base_lohi = _mm256_packus_epi16(_mm256_and_si256(brbase, yff00), _mm256_srli_epi16(brbase, 8));
                base_lohi = _mm256_permute4x64_epi64(base_lohi, 0b11011000);

                if (!z0) {
                    bits_lohi_lo = _mm256_shuffle_epi8(base_lohi, levels00);
                    bits_lohi_hi = _mm256_shuffle_epi8(base_lohi, levels01);
                    bits_lohi_lo = _mm256_blendv_epi8(zero, bits_lohi_lo, _mm256_permute4x64_epi64(mask0, 0b01000100));
                    bits_lohi_hi = _mm256_blendv_epi8(zero, bits_lohi_hi, _mm256_permute4x64_epi64(mask0, 0b11101110));
                    tmp0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_lo, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_lo, 0b11110110));  //16-bit bit for zero coeffs level = 0
                    tmp1 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_hi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_hi, 0b11110110));  //16-bit bit for zero coeffs level = 0
                    bits0 = _mm256_add_epi16(bits0, tmp0);
                    bits1 = _mm256_add_epi16(bits1, tmp1);
                }
                if (!z1) {
                    bits_lohi_lo = _mm256_shuffle_epi8(base_lohi, levels10);
                    bits_lohi_hi = _mm256_shuffle_epi8(base_lohi, levels11);
                    bits_lohi_lo = _mm256_blendv_epi8(zero, bits_lohi_lo, _mm256_permute4x64_epi64(mask1, 0b01000100));
                    bits_lohi_hi = _mm256_blendv_epi8(zero, bits_lohi_hi, _mm256_permute4x64_epi64(mask1, 0b11101110));
                    tmp0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_lo, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_lo, 0b11110110));  //16-bit bit for zero coeffs level = 0
                    tmp1 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_hi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_hi, 0b11110110));  //16-bit bit for zero coeffs level = 0
                    bits0 = _mm256_add_epi16(bits0, tmp0);
                    bits1 = _mm256_add_epi16(bits1, tmp1);
                }
            }
        }
        //sum up for final result
        bits0 = _mm256_add_epi16(bits0, bits1);
        __m128i sum128 = _mm_add_epi16(si128_lo(bits0), si128_hi(bits0));
        sum128 = _mm_add_epi32(_mm_srli_epi32(sum128, 16), _mm_and_si128(sum128, _mm_set1_epi32(0x0000ffff)));
        sum128 = _mm_add_epi32(sum128, _mm_srli_epi64(sum128, 32));
        sum128 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 8));

        bits += _mm_cvtsi128_si32(sum128);
    }
#if 0

        scan++;
        if (bc.txSize == TX_4X4) {
            for (; scan < scan_e; scan++) {
                const int pos = *scan;
                const uint8_t bcont = base_contexts[pos];
                const int level = bcont & 15;
                if (!level) {
                    bits += bc.coeffBaseT[0][bcont >> 4];
                    continue;
                }

                if (--numNonZero == 0) break;

                bits += bc.coeffBase[bcont >> 4][level < 3 ? level : 3];
                bits += bc.coeffBrAcc[br_contexts[pos]][level];
            }
        }
        else {
            for (; scan < scan_e; scan++) {
                const int pos = *scan;
                const uint8_t bcont = base_contexts[pos];
                const int level = bcont & 15;
                const uint16_t* base = bc.coeffBase[bcont >> 4];

                if (level) {
                    if (--numNonZero == 0) break;
                    bits += bc.coeffBrAcc[br_contexts[pos]][level];
                }

                bits += base[level < 3 ? level : 3];
            }
        }

#endif

    if (numNonZero == 0) {
        const int32_t brCont = _mm_cvtsi128_si32(brC) & 0xff;
        const int32_t baseCont = _mm_cvtsi128_si32(baseC) & 0xff;
        const int32_t baseCtx = baseCont>>4;// base_contexts[pos] >> 4;
        const int32_t lev = baseCont&15;// base_contexts[pos] & 15;
        const int32_t baseLevel = (lev < 3) ? lev : 3;

        int c = 36 - int(scan_e - scan);
        bits += bc.coeffBrAcc[brCont/*br_contexts[pos]*/][lev];
        bits += bc.coeffBaseEob[1][baseLevel - 1]; // last coef
        bits += bc.eobMulti[BSR(c) + 1]; // eob
        return ((bits * EffNum) >> 7);
    }


#if 0
    uint32_t sum = 0;
    const int16_t* scan16 = av1_default_scan[bc.txSize] + 36;
    const int16_t* scan16o = scan16;
        do {
            if (const int32_t coeff = coeffs[*scan16]) {
                sum += BSR(abs(coeff) + 1);
                --numNonZero;
            }
            scan16++;
        } while (numNonZero);

        c = scan16 - scan16o + 36 - 1;
#else
        //No need to go in scan order here as we sum only non zero coeffs except first 36 coeffs in scan order
        //                           TX_4X4, TX_8X8, TX_16X16, TX_32X32,    TX_64X64, TX_4X8, TX_8X4, TX_8X16, TX_16X8, TX_16X32, TX_32X16, TX_32X64, TX_64X32,    TX_4X16, TX_16X4, TX_8X32, TX_32X8,    TX_16X64, TX_64X16,
        //static const int numCycles[19] = { 0,      4,      16,       32 * 2,      64 * 4,   0,      0,      8,       8,       16 * 2,   32,       32 * 4,   64 * 2,   4,       4,       8 * 2,   16,      16 * 4,  64 };
        static const int numCycles[19] = { 1, 4, 16, 64, 256, 2, 2, 8, 8, 32, 32, 128, 128, 4, 4, 16, 16, 64, 64 };

        //fraction of 16
        const int num = numCycles[bc.txSize];
        const __m256i y0f = _mm256_set1_epi16(0x0F0F);
        const __m256i yff = _mm256_set1_epi16(-1);
        const __m256i zero = _mm256_setzero_si256();
        const __m256i y35 = _mm256_set1_epi16(35);

        const __m128i table128_lo = _mm_setr_epi8(0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3);
        const __m128i table128_hi = _mm_setr_epi8(0, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7);
        const __m256i table_lo = _mm256_broadcastsi128_si256(table128_lo);
        const __m256i table_hi = _mm256_broadcastsi128_si256(table128_hi);

        const int16_t* scn = av1_default_scan_r[bc.txSize];
        __m256i max_scan = _mm256_setzero_si256();
        __m128i sum128 = _mm_setzero_si128();
        int num_nz_coeffs = 0;
        for (int i = 0; i < num; i++) {
            const __m256i c1 = _mm256_load_si256((__m256i*)(coeffs + (i << 4)));
            //test for zeros
            if (_mm256_testz_si256(c1, yff)) {
                if (num_nz_coeffs == numNonZero)
                    break;
                continue;
            }
            __m256i mask = _mm256_cmpeq_epi16(c1, zero);
            const __m256i cc1 = _mm256_abs_epi16(c1);
            mask = _mm256_xor_si256(yff, mask); //invert

            __m256i sc = _mm256_load_si256((__m256i*)(scn + (i << 4)));
            const __m256i smask = _mm256_cmpgt_epi16(sc, y35);

            const unsigned int ncf = _mm256_movemask_epi8(_mm256_and_si256(mask, smask));
            num_nz_coeffs += (_mm_popcnt_u32(ncf) >> 1);

            sc = _mm256_and_si256(sc, smask);
            sc = _mm256_and_si256(sc, mask);
            max_scan = _mm256_max_epi16(max_scan, sc);

            //read mask for scan first 36 coeffs
            const __m256i ccc1 = _mm256_add_epi16(_mm256_and_si256(cc1, smask), _mm256_set1_epi16(1));

            //bsr on avx2
            const __m256i x = _mm256_max_epi8(_mm256_shuffle_epi8(table_lo, _mm256_and_si256(ccc1, y0f)),
                _mm256_shuffle_epi8(table_hi, _mm256_and_si256(_mm256_srli_epi16(ccc1, 4), y0f)));

            sum128 = _mm_add_epi64(sum128, _mm_sad_epu8(si128_lo(x), _mm_slli_si128(si128_hi(x), 1)));
        }

        __m128i max128 = _mm_max_epi16(si128_lo(max_scan), si128_hi(max_scan));
        max128 = _mm_xor_si128(max128, _mm_set1_epi32(-1));
        max128 = _mm_minpos_epu16(max128);
        max128 = _mm_andnot_si128(max128, _mm_setr_epi16(-1, 0, 0, 0, 0, 0, 0, 0));
        const int c = _mm_cvtsi128_si32(max128);
        sum128 = _mm_add_epi64(sum128, _mm_srli_si128(sum128, 8));
        const uint32_t sum = _mm_cvtsi128_si32(sum128);
#endif

    const int32_t a = HIGH_FREQ_COEFF_FIT_CURVE[bc.qpi][bc.txSize][bc.plane][0];
    const int32_t b = HIGH_FREQ_COEFF_FIT_CURVE[bc.qpi][bc.txSize][bc.plane][1];
    bits += sum * a + (c - 35) * b;
    bits += bc.eobMulti[BSR(c) + 1]; // eob
    return ((bits * EffNum) >> 7);
}

#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
uint32_t AV1Enc::EstimateCoefsAv1_MD(const TxbBitCounts &bc, int32_t txbSkipCtx, int32_t numNonZero,
                                    const CoeffsType *coeffs, int32_t *culLevel)
{
    assert(bc.txSize == TX_8X8);
    assert(numNonZero > 0);

    uint32_t bits = 0;
    bits += bc.txbSkip[txbSkipCtx][0]; // non skip
    bits += 512 * numNonZero; // signs

    if (numNonZero == 1 && coeffs[0] != 0) {
        const int32_t level = abs(coeffs[0]);
        *culLevel = std::min(COEFF_CONTEXT_MASK, level);
        bits += bc.eobMulti[0]; // eob
        bits += bc.coeffBaseEob[0][ std::min(3, level) - 1 ];
        bits += bc.coeffBrAcc[0][ std::min(15, level) ];
        return bits;
    }

    alignas(32) uint8_t base_contexts[8 * 8];
    alignas(32) uint8_t br_contexts[8 * 8];

    *culLevel = get_coeff_contexts_8x8(coeffs, base_contexts, br_contexts);

    const int32_t level = base_contexts[0] & 15;
    bits += bc.coeffBase[0][ std::min(3, level) ];
    bits += bc.coeffBrAcc[ br_contexts[0] ][level];
    if (level)
        --numNonZero;
    assert(numNonZero > 0);

    const int16_t *scan = av1_default_scan[bc.txSize != TX_4X4];
    for (int32_t c = 1; c < 64; c++) {
        const int32_t pos = scan[c];
        const int32_t baseCtx = base_contexts[pos] >> 4;
        const int32_t level = base_contexts[pos] & 15;
        const int32_t baseLevel = std::min(3, level);

        if (level) {
            bits += bc.coeffBrAcc[ br_contexts[pos] ][level];
            if (--numNonZero == 0) {
                bits += bc.coeffBaseEob[1][baseLevel - 1]; // last coef
                bits += bc.eobMulti[ BSR(c) + 1 ]; // eob
                return bits;
            }
        }

        bits += bc.coeffBase[baseCtx][baseLevel];
    }

    assert(0);
    return bits;
}
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

template uint32_t AV1Enc::EstimateCoefs<DCT_DCT>(const CoefBitCounts&,const CoeffsType*,const int16_t*,int32_t,int32_t);
template uint32_t AV1Enc::EstimateCoefs<DCT_ADST>(const CoefBitCounts&,const CoeffsType*,const int16_t*,int32_t,int32_t);
template uint32_t AV1Enc::EstimateCoefs<ADST_DCT>(const CoefBitCounts&,const CoeffsType*,const int16_t*,int32_t,int32_t);
template uint32_t AV1Enc::EstimateCoefs<ADST_ADST>(const CoefBitCounts&,const CoeffsType*,const int16_t*,int32_t,int32_t);

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE

