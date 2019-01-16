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
            costs[inv_map[i]] = av1_cost_symbol(p15);
        else
            costs[i] = av1_cost_symbol(p15);

        // Stop once we reach the end of the CDF
        if (cdf[i] == AOM_ICDF(CDF_PROB_TOP)) break;
    }
}

void AV1Enc::EstimateBits(BitCounts &bits, int32_t intraOnly, EnumCodecType codecType)
{
    const vpx_prob (*partition_probs)[PARTITION_TYPES-1] = default_partition_probs;
    Zero(bits.av1Partition);

    enum { HAS_COLS = 1, HAS_ROWS = 2 };
    for (int32_t ctx = 0; ctx < AV1_PARTITION_CONTEXTS; ctx++) {
        av1_cost_tokens_from_cdf(bits.av1Partition[HAS_ROWS + HAS_COLS][ctx], default_partition_cdf[ctx], NULL);

        aom_cdf_prob tmpCdf[2];
        uint32_t tmpCost[2] = {};
        partition_gather_vert_alike(tmpCdf, default_partition_cdf[ctx], BLOCK_64X64);
        av1_cost_tokens_from_cdf(tmpCost, tmpCdf, NULL);
        bits.av1Partition[HAS_COLS][ctx][PARTITION_HORZ] = tmpCost[0];
        bits.av1Partition[HAS_COLS][ctx][PARTITION_SPLIT] = tmpCost[1];

        partition_gather_horz_alike(tmpCdf, default_partition_cdf[ctx], BLOCK_64X64);
        av1_cost_tokens_from_cdf(tmpCost, tmpCdf, NULL);
        bits.av1Partition[HAS_ROWS][ctx][PARTITION_VERT] = tmpCost[0];
        bits.av1Partition[HAS_ROWS][ctx][PARTITION_SPLIT] = tmpCost[1];
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

    for (int i = 0; i < KF_MODE_CONTEXTS; ++i)
        for (int j = 0; j < KF_MODE_CONTEXTS; ++j)
            av1_cost_tokens_from_cdf(bits.kfIntraModeAV1[i][j], default_av1_kf_y_mode_cdf[i][j], NULL);
    for (int i = 0; i < BLOCK_SIZE_GROUPS; ++i)
        av1_cost_tokens_from_cdf(bits.intraModeAV1[i], default_if_y_mode_cdf[i], NULL);
    for (int i = 0; i < AV1_INTRA_MODES; ++i)
        av1_cost_tokens_from_cdf(bits.intraModeUvAV1[i], default_uv_mode_cdf[i], NULL);

    for (int32_t txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
        for (int32_t plane = 0; plane < BLOCK_TYPES; plane++) {
            for (int32_t inter = 0; inter < REF_TYPES; inter++) {
                bits.coef[plane][inter][txSize].txSize = txSize;
                bits.coef[plane][inter][txSize].plane = plane;
                bits.coef[plane][inter][txSize].inter = inter;
                for (int32_t band = 0; band < COEF_BANDS; band++) {
                    for (int32_t ctx = 0; ctx < PREV_COEF_CONTEXTS; ctx++) {
                        const vpx_prob *probs = default_coef_probs[txSize][plane][inter][band][ctx];
                        bits.coef[plane][inter][txSize].moreCoef[band][ctx][0] = Bits<0>(probs);
                        bits.coef[plane][inter][txSize].moreCoef[band][ctx][1] = Bits<1>(probs);
                        vpx_prob p[10] = {}; // pareto probs
                        for (int32_t i = 2; i < 10; i++)
                            p[i] = pareto(i, probs[2]);
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
    for (int32_t ctx = 0; ctx < INTER_MODE_CONTEXTS; ctx++)
        av1_cost_tokens_from_cdf(bits.interCompMode[ctx], default_inter_compound_mode_cdf[ctx], nullptr);

    int8_t av1_switchable_interp_tree[] = { 0,2,4,-2, -1, -3 };
    for (int32_t ctx = 0; ctx < AV1_INTERP_FILTER_CONTEXTS; ctx++) {
        const vpx_prob *probs = default_interp_filter_probs_av1[ctx];
        av1_cost_tokens(bits.interpFilterAV1[ctx], probs, av1_switchable_interp_tree);
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

                //copy transpose
                for (int c = 0; c < 16; c++) {
                    for (int l = 0; l < (NUM_BASE_LEVELS + 2); l++) {
                        bits.txb[q][t][p].coeffBaseT[l][c] = bits.txb[q][t][p].coeffBase[c][l];
                    }
                }

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
            av1_cost_tokens_from_cdf(bits.txb[q][TX_4X4][p].eobMulti, default_eob_multi16_cdfs[q][p][TX_CLASS_2D], nullptr);
            av1_cost_tokens_from_cdf(bits.txb[q][TX_8X8][p].eobMulti, default_eob_multi64_cdfs[q][p][TX_CLASS_2D], nullptr);
            av1_cost_tokens_from_cdf(bits.txb[q][TX_16X16][p].eobMulti, default_eob_multi256_cdfs[q][p][TX_CLASS_2D], nullptr);
            av1_cost_tokens_from_cdf(bits.txb[q][TX_32X32][p].eobMulti, default_eob_multi1024_cdfs[q][p][TX_CLASS_2D], nullptr);

            for (int32_t i = 2; i < 5; i++)  bits.txb[q][TX_4X4][p].eobMulti[i] += 512 * i - 512;
            for (int32_t i = 2; i < 7; i++)  bits.txb[q][TX_8X8][p].eobMulti[i] += 512 * i - 512;
            for (int32_t i = 2; i < 9; i++)  bits.txb[q][TX_16X16][p].eobMulti[i] += 512 * i - 512;
            for (int32_t i = 2; i < 11; i++) bits.txb[q][TX_32X32][p].eobMulti[i] += 512 * i - 512;
        }
    }
}

void AV1Enc::GetRefFrameContextsAv1(Frame *frame)
{
    const BitCounts &bc = frame->bitCount;

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

            ctx->singleRefP1  = GetCtxSingleRefP1Av1(neighborsRefCounts);
            ctx->singleRefP2  = GetCtxSingleRefP2Av1(neighborsRefCounts);
            ctx->singleRefP3  = GetCtxSingleRefP3(neighborsRefCounts);
            ctx->singleRefP4  = GetCtxSingleRefP4(neighborsRefCounts);
            ctx->singleRefP5  = GetCtxSingleRefP5(neighborsRefCounts);
            ctx->refMode      = GetCtxReferenceMode(pa, pl);
            ctx->compRefType  = GetCtxCompRefType(pa, pl);
            ctx->compFwdRefP0 = GetCtxCompRefP0(neighborsRefCounts);
            ctx->compFwdRefP1 = GetCtxCompRefP1(neighborsRefCounts);
            ctx->compBwdRefP0 = GetCtxCompBwdRefP0(neighborsRefCounts);

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
        const int16_t& coef = coefs[*scan];
        uint16_t absCoef = std::min((int)256, std::abs((int)coef));
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
            int32_t pos = scan[c];
            while (coefs[pos] == 0) {
                int32_t ctx = GetCtxTokenAc<txType>(pos & maskPosX, pos >> shiftPosY, tokenCacheA, tokenCacheL);
                tokenCacheA[pos & maskPosX] = energy_class[ZERO_TOKEN];
                tokenCacheL[pos >> shiftPosY] = energy_class[ZERO_TOKEN];
                numbits += bits.token[band[c]][ctx][ZERO_TOKEN]; // token
                c++;
                pos = scan[c];
            }

            int32_t absCoef = abs(coefs[pos]);
            int32_t ctx = GetCtxTokenAc<txType>(pos & maskPosX, pos >> shiftPosY, tokenCacheA, tokenCacheL);
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

int32_t get_coeff_contexts_4x4(const int16_t *coeff, uint8_t *baseCtx, uint8_t *brCtx)
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
    storea_si128(brCtx, get_br_context(a, b, c, br_ctx_offset));

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
    storea_si128(baseCtx, base_ctx);

    return culLevel;
}

inline AV1_FORCEINLINE __m128i si128(__m256i r) { return _mm256_castsi256_si128(r); }
inline AV1_FORCEINLINE __m128i si128_lo(__m256i r) { return si128(r); }
inline AV1_FORCEINLINE __m128i si128_hi(__m256i r) { return _mm256_extracti128_si256(r, 1); }

inline AV1_FORCEINLINE __m256i loada2_m128i(const void *lo, const void *hi) {
    return _mm256_insertf128_si256(_mm256_castsi128_si256(loada_si128(lo)), loada_si128(hi), 1);
}

#define permute4x64(r, m0, m1, m2, m3) _mm256_permute4x64_epi64(r, (m0) + ((m1) << 2) + ((m2) << 4) + ((m3) << 6))
#define permute2x128(r0, r1, m0, m1) _mm256_permute2x128_si256(r0, r1, (m0) + ((m1) << 4))

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

int32_t get_coeff_contexts_8x8(const int16_t *coeff, uint8_t *baseCtx, uint8_t *brCtx)
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

    __m256i br_ctx, base_ctx;
    __m256i a, b, c, d, e, tmp;

    tmp = _mm256_blend_epi32(rows0123, rows4567, 3);  // row4 row1 row2 row3
    a = _mm256_srli_epi64(rows0123, 8); // + 1
    b = permute4x64(tmp, 1, 2, 3, 0);   // + pitch
    c = _mm256_srli_epi64(b, 8);        // + 1 + pitch
    storea_si256(brCtx, get_br_context_avx2(a, b, c, br_ctx_offset0));

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    storea_si256(brCtx + 32, get_br_context_avx2(a, b, c, br_ctx_offset1));

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
    storea_si256(baseCtx, base_ctx);

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    d = _mm256_srli_epi64(rows4567, 16);
    e = permute2x128(rows4567, rows4567, 1, 8);
    base_ctx = get_base_context_avx2(a, b, c, d, e, base_ctx_offset1);
    base_ctx = _mm256_slli_epi16(base_ctx, 4);
    base_ctx = _mm256_or_si256(base_ctx, rows4567_sat15);
    storea_si256(baseCtx + 32, base_ctx);

    __m128i sum128 = _mm_add_epi64(si128_lo(sum), si128_hi(sum));
    int32_t culLevel = _mm_cvtsi128_si32(sum128) + _mm_extract_epi32(sum128, 2);
    culLevel = std::min(COEFF_CONTEXT_MASK, culLevel);
    return culLevel;
}

int32_t get_coeff_contexts_16x16(const int16_t *coeff, uint8_t *baseCtx, uint8_t *brCtx, int32_t txSize)
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

    __m256i br_ctx, base_ctx;
    __m256i a, b, c, d, e, tmp;

    tmp = _mm256_blend_epi32(rows0123, rows4567, 3);  // row4 row1 row2 row3
    a = _mm256_srli_epi64(rows0123, 8); // + 1
    b = permute4x64(tmp, 1, 2, 3, 0);   // + pitch
    c = _mm256_srli_epi64(b, 8);        // + 1 + pitch
    storea_si256(brCtx, get_br_context_avx2(a, b, c, br_ctx_offset0));

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    storea_si256(brCtx + 32, get_br_context_avx2(a, b, c, br_ctx_offset1));

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
    storea_si256(baseCtx, base_ctx);

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    d = _mm256_srli_epi64(rows4567, 16);
    e = permute2x128(rows4567, rows4567, 1, 8);
    base_ctx = get_base_context_avx2(a, b, c, d, e, base_ctx_offset1);
    base_ctx = _mm256_slli_epi16(base_ctx, 4);
    base_ctx = _mm256_or_si256(base_ctx, rows4567_sat15);
    storea_si256(baseCtx + 32, base_ctx);

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

alignas(64) const int16_t av1_default_scan_4x4_r[16] = {
0, 1, 5, 6, 2, 4, 7, 12, 3, 8, 11, 13, 9, 10, 14, 15, };


alignas(64) const int16_t av1_default_scan_8x8_r[64] = {
0, 1, 5, 6, 14, 15, 27, 28, 2, 4, 7, 13, 16, 26, 29, 42, 3,
8, 12, 17, 25, 30, 41, 43, 9, 11, 18, 24, 31, 40, 44, 53, 10,
19, 23, 32, 39, 45, 52, 54, 20, 22, 33, 38, 46, 51, 55, 60, 21,
34, 37, 47, 50, 56, 59, 61, 35, 36, 48, 49, 57, 58, 62, 63, };


alignas(64) const int16_t av1_default_scan_16x16_r[256] = {
0, 1, 5, 6, 14, 15, 27, 28, 44, 45, 65, 66, 90, 91, 119, 120, 2,
4, 7, 13, 16, 26, 29, 43, 46, 64, 67, 89, 92, 118, 121, 150, 3,
8, 12, 17, 25, 30, 42, 47, 63, 68, 88, 93, 117, 122, 149, 151, 9,
11, 18, 24, 31, 41, 48, 62, 69, 87, 94, 116, 123, 148, 152, 177, 10,
19, 23, 32, 40, 49, 61, 70, 86, 95, 115, 124, 147, 153, 176, 178, 20,
22, 33, 39, 50, 60, 71, 85, 96, 114, 125, 146, 154, 175, 179, 200, 21,
34, 38, 51, 59, 72, 84, 97, 113, 126, 145, 155, 174, 180, 199, 201, 35,
37, 52, 58, 73, 83, 98, 112, 127, 144, 156, 173, 181, 198, 202, 219, 36,
53, 57, 74, 82, 99, 111, 128, 143, 157, 172, 182, 197, 203, 218, 220, 54,
56, 75, 81, 100, 110, 129, 142, 158, 171, 183, 196, 204, 217, 221, 234, 55,
76, 80, 101, 109, 130, 141, 159, 170, 184, 195, 205, 216, 222, 233, 235, 77,
79, 102, 108, 131, 140, 160, 169, 185, 194, 206, 215, 223, 232, 236, 245, 78,
103, 107, 132, 139, 161, 168, 186, 193, 207, 214, 224, 231, 237, 244, 246, 104,
106, 133, 138, 162, 167, 187, 192, 208, 213, 225, 230, 238, 243, 247, 252, 105,
134, 137, 163, 166, 188, 191, 209, 212, 226, 229, 239, 242, 248, 251, 253, 135,
136, 164, 165, 189, 190, 210, 211, 227, 228, 240, 241, 249, 250, 254, 255, };


alignas(64) const int16_t av1_default_scan_32x32_r[1024] = {
0, 1, 5, 6, 14, 15, 27, 28, 44, 45, 65, 66, 90, 91, 119, 120, 152,
153, 189, 190, 230, 231, 275, 276, 324, 325, 377, 378, 434, 435, 495, 496, 2,
4, 7, 13, 16, 26, 29, 43, 46, 64, 67, 89, 92, 118, 121, 151, 154,
188, 191, 229, 232, 274, 277, 323, 326, 376, 379, 433, 436, 494, 497, 558, 3,
8, 12, 17, 25, 30, 42, 47, 63, 68, 88, 93, 117, 122, 150, 155, 187,
192, 228, 233, 273, 278, 322, 327, 375, 380, 432, 437, 493, 498, 557, 559, 9,
11, 18, 24, 31, 41, 48, 62, 69, 87, 94, 116, 123, 149, 156, 186, 193,
227, 234, 272, 279, 321, 328, 374, 381, 431, 438, 492, 499, 556, 560, 617, 10,
19, 23, 32, 40, 49, 61, 70, 86, 95, 115, 124, 148, 157, 185, 194, 226,
235, 271, 280, 320, 329, 373, 382, 430, 439, 491, 500, 555, 561, 616, 618, 20,
22, 33, 39, 50, 60, 71, 85, 96, 114, 125, 147, 158, 184, 195, 225, 236,
270, 281, 319, 330, 372, 383, 429, 440, 490, 501, 554, 562, 615, 619, 672, 21,
34, 38, 51, 59, 72, 84, 97, 113, 126, 146, 159, 183, 196, 224, 237, 269,
282, 318, 331, 371, 384, 428, 441, 489, 502, 553, 563, 614, 620, 671, 673, 35,
37, 52, 58, 73, 83, 98, 112, 127, 145, 160, 182, 197, 223, 238, 268, 283,
317, 332, 370, 385, 427, 442, 488, 503, 552, 564, 613, 621, 670, 674, 723, 36,
53, 57, 74, 82, 99, 111, 128, 144, 161, 181, 198, 222, 239, 267, 284, 316,
333, 369, 386, 426, 443, 487, 504, 551, 565, 612, 622, 669, 675, 722, 724, 54,
56, 75, 81, 100, 110, 129, 143, 162, 180, 199, 221, 240, 266, 285, 315, 334,
368, 387, 425, 444, 486, 505, 550, 566, 611, 623, 668, 676, 721, 725, 770, 55,
76, 80, 101, 109, 130, 142, 163, 179, 200, 220, 241, 265, 286, 314, 335, 367,
388, 424, 445, 485, 506, 549, 567, 610, 624, 667, 677, 720, 726, 769, 771, 77,
79, 102, 108, 131, 141, 164, 178, 201, 219, 242, 264, 287, 313, 336, 366, 389,
423, 446, 484, 507, 548, 568, 609, 625, 666, 678, 719, 727, 768, 772, 813, 78,
103, 107, 132, 140, 165, 177, 202, 218, 243, 263, 288, 312, 337, 365, 390, 422,
447, 483, 508, 547, 569, 608, 626, 665, 679, 718, 728, 767, 773, 812, 814, 104,
106, 133, 139, 166, 176, 203, 217, 244, 262, 289, 311, 338, 364, 391, 421, 448,
482, 509, 546, 570, 607, 627, 664, 680, 717, 729, 766, 774, 811, 815, 852, 105,
134, 138, 167, 175, 204, 216, 245, 261, 290, 310, 339, 363, 392, 420, 449, 481,
510, 545, 571, 606, 628, 663, 681, 716, 730, 765, 775, 810, 816, 851, 853, 135,
137, 168, 174, 205, 215, 246, 260, 291, 309, 340, 362, 393, 419, 450, 480, 511,
544, 572, 605, 629, 662, 682, 715, 731, 764, 776, 809, 817, 850, 854, 887, 136,
169, 173, 206, 214, 247, 259, 292, 308, 341, 361, 394, 418, 451, 479, 512, 543,
573, 604, 630, 661, 683, 714, 732, 763, 777, 808, 818, 849, 855, 886, 888, 170,
172, 207, 213, 248, 258, 293, 307, 342, 360, 395, 417, 452, 478, 513, 542, 574,
603, 631, 660, 684, 713, 733, 762, 778, 807, 819, 848, 856, 885, 889, 918, 171,
208, 212, 249, 257, 294, 306, 343, 359, 396, 416, 453, 477, 514, 541, 575, 602,
632, 659, 685, 712, 734, 761, 779, 806, 820, 847, 857, 884, 890, 917, 919, 209,
211, 250, 256, 295, 305, 344, 358, 397, 415, 454, 476, 515, 540, 576, 601, 633,
658, 686, 711, 735, 760, 780, 805, 821, 846, 858, 883, 891, 916, 920, 945, 210,
251, 255, 296, 304, 345, 357, 398, 414, 455, 475, 516, 539, 577, 600, 634, 657,
687, 710, 736, 759, 781, 804, 822, 845, 859, 882, 892, 915, 921, 944, 946, 252,
254, 297, 303, 346, 356, 399, 413, 456, 474, 517, 538, 578, 599, 635, 656, 688,
709, 737, 758, 782, 803, 823, 844, 860, 881, 893, 914, 922, 943, 947, 968, 253,
298, 302, 347, 355, 400, 412, 457, 473, 518, 537, 579, 598, 636, 655, 689, 708,
738, 757, 783, 802, 824, 843, 861, 880, 894, 913, 923, 942, 948, 967, 969, 299,
301, 348, 354, 401, 411, 458, 472, 519, 536, 580, 597, 637, 654, 690, 707, 739,
756, 784, 801, 825, 842, 862, 879, 895, 912, 924, 941, 949, 966, 970, 987, 300,
349, 353, 402, 410, 459, 471, 520, 535, 581, 596, 638, 653, 691, 706, 740, 755,
785, 800, 826, 841, 863, 878, 896, 911, 925, 940, 950, 965, 971, 986, 988, 350,
352, 403, 409, 460, 470, 521, 534, 582, 595, 639, 652, 692, 705, 741, 754, 786,
799, 827, 840, 864, 877, 897, 910, 926, 939, 951, 964, 972, 985, 989, 1002, 351,
404, 408, 461, 469, 522, 533, 583, 594, 640, 651, 693, 704, 742, 753, 787, 798,
828, 839, 865, 876, 898, 909, 927, 938, 952, 963, 973, 984, 990, 1001, 1003, 405,
407, 462, 468, 523, 532, 584, 593, 641, 650, 694, 703, 743, 752, 788, 797, 829,
838, 866, 875, 899, 908, 928, 937, 953, 962, 974, 983, 991, 1000, 1004, 1013, 406,
463, 467, 524, 531, 585, 592, 642, 649, 695, 702, 744, 751, 789, 796, 830, 837,
867, 874, 900, 907, 929, 936, 954, 961, 975, 982, 992, 999, 1005, 1012, 1014, 464,
466, 525, 530, 586, 591, 643, 648, 696, 701, 745, 750, 790, 795, 831, 836, 868,
873, 901, 906, 930, 935, 955, 960, 976, 981, 993, 998, 1006, 1011, 1015, 1020, 465,
526, 529, 587, 590, 644, 647, 697, 700, 746, 749, 791, 794, 832, 835, 869, 872,
902, 905, 931, 934, 956, 959, 977, 980, 994, 997, 1007, 1010, 1016, 1019, 1021, 527,
528, 588, 589, 645, 646, 698, 699, 747, 748, 792, 793, 833, 834, 870, 871, 903,
904, 932, 933, 957, 958, 978, 979, 995, 996, 1008, 1009, 1017, 1018, 1022, 1023, };


alignas(64) const int16_t av1_default_scan_4x8_r[32] = {
0, 1, 3, 6, 2, 4, 7, 10, 5, 8, 11, 14, 9, 12, 15, 18, 13,
16, 19, 22, 17, 20, 23, 26, 21, 24, 27, 29, 25, 28, 30, 31, };


alignas(64) const int16_t av1_default_scan_8x4_r[32] = {
0, 2, 5, 9, 13, 17, 21, 25, 1, 4, 8, 12, 16, 20, 24, 28, 3,
7, 11, 15, 19, 23, 27, 30, 6, 10, 14, 18, 22, 26, 29, 31, };


alignas(64) const int16_t av1_default_scan_8x16_r[128] = {
0, 1, 3, 6, 10, 15, 21, 28, 2, 4, 7, 11, 16, 22, 29, 36, 5,
8, 12, 17, 23, 30, 37, 44, 9, 13, 18, 24, 31, 38, 45, 52, 14,
19, 25, 32, 39, 46, 53, 60, 20, 26, 33, 40, 47, 54, 61, 68, 27,
34, 41, 48, 55, 62, 69, 76, 35, 42, 49, 56, 63, 70, 77, 84, 43,
50, 57, 64, 71, 78, 85, 92, 51, 58, 65, 72, 79, 86, 93, 100, 59,
66, 73, 80, 87, 94, 101, 107, 67, 74, 81, 88, 95, 102, 108, 113, 75,
82, 89, 96, 103, 109, 114, 118, 83, 90, 97, 104, 110, 115, 119, 122, 91,
98, 105, 111, 116, 120, 123, 125, 99, 106, 112, 117, 121, 124, 126, 127, };


alignas(64) const int16_t av1_default_scan_16x8_r[128] = {
0, 2, 5, 9, 14, 20, 27, 35, 43, 51, 59, 67, 75, 83, 91, 99, 1,
4, 8, 13, 19, 26, 34, 42, 50, 58, 66, 74, 82, 90, 98, 106, 3,
7, 12, 18, 25, 33, 41, 49, 57, 65, 73, 81, 89, 97, 105, 112, 6,
11, 17, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 111, 117, 10,
16, 23, 31, 39, 47, 55, 63, 71, 79, 87, 95, 103, 110, 116, 121, 15,
22, 30, 38, 46, 54, 62, 70, 78, 86, 94, 102, 109, 115, 120, 124, 21,
29, 37, 45, 53, 61, 69, 77, 85, 93, 101, 108, 114, 119, 123, 126, 28,
36, 44, 52, 60, 68, 76, 84, 92, 100, 107, 113, 118, 122, 125, 127, };


alignas(64) const int16_t av1_default_scan_16x32_r[512] = {
0, 1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 66, 78, 91, 105, 120, 2,
4, 7, 11, 16, 22, 29, 37, 46, 56, 67, 79, 92, 106, 121, 136, 5,
8, 12, 17, 23, 30, 38, 47, 57, 68, 80, 93, 107, 122, 137, 152, 9,
13, 18, 24, 31, 39, 48, 58, 69, 81, 94, 108, 123, 138, 153, 168, 14,
19, 25, 32, 40, 49, 59, 70, 82, 95, 109, 124, 139, 154, 169, 184, 20,
26, 33, 41, 50, 60, 71, 83, 96, 110, 125, 140, 155, 170, 185, 200, 27,
34, 42, 51, 61, 72, 84, 97, 111, 126, 141, 156, 171, 186, 201, 216, 35,
43, 52, 62, 73, 85, 98, 112, 127, 142, 157, 172, 187, 202, 217, 232, 44,
53, 63, 74, 86, 99, 113, 128, 143, 158, 173, 188, 203, 218, 233, 248, 54,
64, 75, 87, 100, 114, 129, 144, 159, 174, 189, 204, 219, 234, 249, 264, 65,
76, 88, 101, 115, 130, 145, 160, 175, 190, 205, 220, 235, 250, 265, 280, 77,
89, 102, 116, 131, 146, 161, 176, 191, 206, 221, 236, 251, 266, 281, 296, 90,
103, 117, 132, 147, 162, 177, 192, 207, 222, 237, 252, 267, 282, 297, 312, 104,
118, 133, 148, 163, 178, 193, 208, 223, 238, 253, 268, 283, 298, 313, 328, 119,
134, 149, 164, 179, 194, 209, 224, 239, 254, 269, 284, 299, 314, 329, 344, 135,
150, 165, 180, 195, 210, 225, 240, 255, 270, 285, 300, 315, 330, 345, 360, 151,
166, 181, 196, 211, 226, 241, 256, 271, 286, 301, 316, 331, 346, 361, 376, 167,
182, 197, 212, 227, 242, 257, 272, 287, 302, 317, 332, 347, 362, 377, 392, 183,
198, 213, 228, 243, 258, 273, 288, 303, 318, 333, 348, 363, 378, 393, 407, 199,
214, 229, 244, 259, 274, 289, 304, 319, 334, 349, 364, 379, 394, 408, 421, 215,
230, 245, 260, 275, 290, 305, 320, 335, 350, 365, 380, 395, 409, 422, 434, 231,
246, 261, 276, 291, 306, 321, 336, 351, 366, 381, 396, 410, 423, 435, 446, 247,
262, 277, 292, 307, 322, 337, 352, 367, 382, 397, 411, 424, 436, 447, 457, 263,
278, 293, 308, 323, 338, 353, 368, 383, 398, 412, 425, 437, 448, 458, 467, 279,
294, 309, 324, 339, 354, 369, 384, 399, 413, 426, 438, 449, 459, 468, 476, 295,
310, 325, 340, 355, 370, 385, 400, 414, 427, 439, 450, 460, 469, 477, 484, 311,
326, 341, 356, 371, 386, 401, 415, 428, 440, 451, 461, 470, 478, 485, 491, 327,
342, 357, 372, 387, 402, 416, 429, 441, 452, 462, 471, 479, 486, 492, 497, 343,
358, 373, 388, 403, 417, 430, 442, 453, 463, 472, 480, 487, 493, 498, 502, 359,
374, 389, 404, 418, 431, 443, 454, 464, 473, 481, 488, 494, 499, 503, 506, 375,
390, 405, 419, 432, 444, 455, 465, 474, 482, 489, 495, 500, 504, 507, 509, 391,
406, 420, 433, 445, 456, 466, 475, 483, 490, 496, 501, 505, 508, 510, 511, };


alignas(64) const int16_t av1_default_scan_32x16_r[512] = {
0, 2, 5, 9, 14, 20, 27, 35, 44, 54, 65, 77, 90, 104, 119, 135, 151,
167, 183, 199, 215, 231, 247, 263, 279, 295, 311, 327, 343, 359, 375, 391, 1,
4, 8, 13, 19, 26, 34, 43, 53, 64, 76, 89, 103, 118, 134, 150, 166,
182, 198, 214, 230, 246, 262, 278, 294, 310, 326, 342, 358, 374, 390, 406, 3,
7, 12, 18, 25, 33, 42, 52, 63, 75, 88, 102, 117, 133, 149, 165, 181,
197, 213, 229, 245, 261, 277, 293, 309, 325, 341, 357, 373, 389, 405, 420, 6,
11, 17, 24, 32, 41, 51, 62, 74, 87, 101, 116, 132, 148, 164, 180, 196,
212, 228, 244, 260, 276, 292, 308, 324, 340, 356, 372, 388, 404, 419, 433, 10,
16, 23, 31, 40, 50, 61, 73, 86, 100, 115, 131, 147, 163, 179, 195, 211,
227, 243, 259, 275, 291, 307, 323, 339, 355, 371, 387, 403, 418, 432, 445, 15,
22, 30, 39, 49, 60, 72, 85, 99, 114, 130, 146, 162, 178, 194, 210, 226,
242, 258, 274, 290, 306, 322, 338, 354, 370, 386, 402, 417, 431, 444, 456, 21,
29, 38, 48, 59, 71, 84, 98, 113, 129, 145, 161, 177, 193, 209, 225, 241,
257, 273, 289, 305, 321, 337, 353, 369, 385, 401, 416, 430, 443, 455, 466, 28,
37, 47, 58, 70, 83, 97, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256,
272, 288, 304, 320, 336, 352, 368, 384, 400, 415, 429, 442, 454, 465, 475, 36,
46, 57, 69, 82, 96, 111, 127, 143, 159, 175, 191, 207, 223, 239, 255, 271,
287, 303, 319, 335, 351, 367, 383, 399, 414, 428, 441, 453, 464, 474, 483, 45,
56, 68, 81, 95, 110, 126, 142, 158, 174, 190, 206, 222, 238, 254, 270, 286,
302, 318, 334, 350, 366, 382, 398, 413, 427, 440, 452, 463, 473, 482, 490, 55,
67, 80, 94, 109, 125, 141, 157, 173, 189, 205, 221, 237, 253, 269, 285, 301,
317, 333, 349, 365, 381, 397, 412, 426, 439, 451, 462, 472, 481, 489, 496, 66,
79, 93, 108, 124, 140, 156, 172, 188, 204, 220, 236, 252, 268, 284, 300, 316,
332, 348, 364, 380, 396, 411, 425, 438, 450, 461, 471, 480, 488, 495, 501, 78,
92, 107, 123, 139, 155, 171, 187, 203, 219, 235, 251, 267, 283, 299, 315, 331,
347, 363, 379, 395, 410, 424, 437, 449, 460, 470, 479, 487, 494, 500, 505, 91,
106, 122, 138, 154, 170, 186, 202, 218, 234, 250, 266, 282, 298, 314, 330, 346,
362, 378, 394, 409, 423, 436, 448, 459, 469, 478, 486, 493, 499, 504, 508, 105,
121, 137, 153, 169, 185, 201, 217, 233, 249, 265, 281, 297, 313, 329, 345, 361,
377, 393, 408, 422, 435, 447, 458, 468, 477, 485, 492, 498, 503, 507, 510, 120,
136, 152, 168, 184, 200, 216, 232, 248, 264, 280, 296, 312, 328, 344, 360, 376,
392, 407, 421, 434, 446, 457, 467, 476, 484, 491, 497, 502, 506, 509, 511, };

alignas(64) const int16_t av1_default_scan_4x16_r[64] = {
0, 1, 3, 6, 2, 4, 7, 10, 5, 8, 11, 14, 9, 12, 15, 18, 13,
16, 19, 22, 17, 20, 23, 26, 21, 24, 27, 30, 25, 28, 31, 34, 29,
32, 35, 38, 33, 36, 39, 42, 37, 40, 43, 46, 41, 44, 47, 50, 45,
48, 51, 54, 49, 52, 55, 58, 53, 56, 59, 61, 57, 60, 62, 63, };


alignas(64) const int16_t av1_default_scan_16x4_r[64] = {
0, 2, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 1,
4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 3,
7, 11, 15, 19, 23, 27, 31, 35, 39, 43, 47, 51, 55, 59, 62, 6,
10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50, 54, 58, 61, 63, };


alignas(64) const int16_t av1_default_scan_8x32_r[256] = {
0, 1, 3, 6, 10, 15, 21, 28, 2, 4, 7, 11, 16, 22, 29, 36, 5,
8, 12, 17, 23, 30, 37, 44, 9, 13, 18, 24, 31, 38, 45, 52, 14,
19, 25, 32, 39, 46, 53, 60, 20, 26, 33, 40, 47, 54, 61, 68, 27,
34, 41, 48, 55, 62, 69, 76, 35, 42, 49, 56, 63, 70, 77, 84, 43,
50, 57, 64, 71, 78, 85, 92, 51, 58, 65, 72, 79, 86, 93, 100, 59,
66, 73, 80, 87, 94, 101, 108, 67, 74, 81, 88, 95, 102, 109, 116, 75,
82, 89, 96, 103, 110, 117, 124, 83, 90, 97, 104, 111, 118, 125, 132, 91,
98, 105, 112, 119, 126, 133, 140, 99, 106, 113, 120, 127, 134, 141, 148, 107,
114, 121, 128, 135, 142, 149, 156, 115, 122, 129, 136, 143, 150, 157, 164, 123,
130, 137, 144, 151, 158, 165, 172, 131, 138, 145, 152, 159, 166, 173, 180, 139,
146, 153, 160, 167, 174, 181, 188, 147, 154, 161, 168, 175, 182, 189, 196, 155,
162, 169, 176, 183, 190, 197, 204, 163, 170, 177, 184, 191, 198, 205, 212, 171,
178, 185, 192, 199, 206, 213, 220, 179, 186, 193, 200, 207, 214, 221, 228, 187,
194, 201, 208, 215, 222, 229, 235, 195, 202, 209, 216, 223, 230, 236, 241, 203,
210, 217, 224, 231, 237, 242, 246, 211, 218, 225, 232, 238, 243, 247, 250, 219,
226, 233, 239, 244, 248, 251, 253, 227, 234, 240, 245, 249, 252, 254, 255, };


alignas(64) const int16_t av1_default_scan_32x8_r[256] = {
0, 2, 5, 9, 14, 20, 27, 35, 43, 51, 59, 67, 75, 83, 91, 99, 107,
115, 123, 131, 139, 147, 155, 163, 171, 179, 187, 195, 203, 211, 219, 227, 1,
4, 8, 13, 19, 26, 34, 42, 50, 58, 66, 74, 82, 90, 98, 106, 114,
122, 130, 138, 146, 154, 162, 170, 178, 186, 194, 202, 210, 218, 226, 234, 3,
7, 12, 18, 25, 33, 41, 49, 57, 65, 73, 81, 89, 97, 105, 113, 121,
129, 137, 145, 153, 161, 169, 177, 185, 193, 201, 209, 217, 225, 233, 240, 6,
11, 17, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128,
136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 239, 245, 10,
16, 23, 31, 39, 47, 55, 63, 71, 79, 87, 95, 103, 111, 119, 127, 135,
143, 151, 159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 238, 244, 249, 15,
22, 30, 38, 46, 54, 62, 70, 78, 86, 94, 102, 110, 118, 126, 134, 142,
150, 158, 166, 174, 182, 190, 198, 206, 214, 222, 230, 237, 243, 248, 252, 21,
29, 37, 45, 53, 61, 69, 77, 85, 93, 101, 109, 117, 125, 133, 141, 149,
157, 165, 173, 181, 189, 197, 205, 213, 221, 229, 236, 242, 247, 251, 254, 28,
36, 44, 52, 60, 68, 76, 84, 92, 100, 108, 116, 124, 132, 140, 148, 156,
164, 172, 180, 188, 196, 204, 212, 220, 228, 235, 241, 246, 250, 253, 255, };


const int16_t *av1_default_scan_r[TX_SIZES_ALL] = { //reverse scans
    av1_default_scan_4x4_r,
    av1_default_scan_8x8_r,
    av1_default_scan_16x16_r,
    av1_default_scan_32x32_r,
    av1_default_scan_32x32_r,
    av1_default_scan_4x8_r,
    av1_default_scan_8x4_r,
    av1_default_scan_8x16_r,
    av1_default_scan_16x8_r,
    av1_default_scan_16x32_r,
    av1_default_scan_32x16_r,
    av1_default_scan_32x32_r,
    av1_default_scan_32x32_r,
    av1_default_scan_4x16_r,
    av1_default_scan_16x4_r,
    av1_default_scan_8x32_r,
    av1_default_scan_32x8_r,
    av1_default_scan_16x32_r,
    av1_default_scan_32x16_r
};


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

    alignas(64) uint8_t base_contexts[8 * 8];
    alignas(64) uint8_t br_contexts[8 * 8];

    *culLevel = (bc.txSize == TX_4X4)
        ? get_coeff_contexts_4x4(coeffs, base_contexts, br_contexts)
        : (bc.txSize == TX_8X8)
        ? get_coeff_contexts_8x8(coeffs, base_contexts, br_contexts)
        : get_coeff_contexts_16x16(coeffs, base_contexts, br_contexts, bc.txSize);

    const int32_t level = base_contexts[0] & 15;
    bits += bc.coeffBase[0][(level < 3) ? level : 3];
    bits += bc.coeffBrAcc[br_contexts[0]][level];
    if (level)
        --numNonZero;
    assert(numNonZero > 0);

    //const int16_t *scan = av1_default_scan[bc.txSize != TX_4X4];
    const int8_t* scan = bc.txSize == TX_4X4 ? av1_default_scan_4x4_b : av1_default_scan_8x8_b;
    const int8_t* scan_e = scan + 36;;

    scan++;
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
        const __m256i yff = _mm256_set1_epi16(0xFFFF);
        const __m256i yff00 = _mm256_set1_epi16(0x00ff);
        const __m256i y50 = _mm256_set1_epi8(50);
        const __m128i yff00_128 = _mm_set1_epi16(0x00ff);

        __m256i base = _mm256_load_si256((__m256i*)(bc.coeffBaseT[0])); //16 16bit values
        __m256i base_lohi = _mm256_packus_epi16(_mm256_and_si256(base, yff00), _mm256_srli_epi16(base, 8));
        base_lohi = _mm256_permute4x64_epi64(base_lohi, 0b11011000);

        __m256i bcont0 = _mm256_load_si256((__m256i*)base_contexts);
        __m256i levels0 = _mm256_and_si256(bcont0, mask);
        __m256i bcont1 = _mm256_load_si256((__m256i*)(base_contexts + 32));
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
        mscan = _mm_andnot_si128(mscan, _mm_setr_epi16(0x00ff, 0, 0, 0, 0, 0, 0, 0));
        int maxs = _mm_cvtsi128_si32(mscan);

        const unsigned int c0 = _mm256_movemask_epi8(_mm256_and_si256(mask_nz0, smask0)) & 0xfffffffe;
        const unsigned int c1 = _mm256_movemask_epi8(_mm256_and_si256(mask_nz1, smask1));
        const int num_nz_coeffs = _mm_popcnt_u32(c0) + _mm_popcnt_u32(c1); //- coeff at 0
        numNonZero -= num_nz_coeffs;
//        if (nnz - num_nz_coeffs != numNonZero)
//            fprintf(stderr, "wrong nnz\n");

        maxs = (maxs < 35) ? (numNonZero ? 35 :maxs) : 35;
        if (!numNonZero) {
            maxs--;
        }

        ymax = _mm256_set1_epi8(maxs+1);
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
        const __m256i tmp0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_lo, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_lo, 0b11110110));  //16-bit bit for zero coeffs level = 0
        const __m256i tmp1 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_hi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_hi, 0b11110110));  //16-bit bit for zero coeffs level = 0
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
                const __m256i tmp0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_lo, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_lo, 0b11110110));  //16-bit bit for zero coeffs level = 0
                const __m256i tmp1 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_hi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_hi, 0b11110110));  //16-bit bit for zero coeffs level = 0
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

        __m256i brcont0 = _mm256_load_si256((__m256i*)br_contexts);
        __m256i brcont1 = _mm256_load_si256((__m256i*)(br_contexts + 32));

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
        minbr128 = _mm_and_si128(minbr128, _mm_setr_epi16(0xffff, 0, 0, 0, 0, 0, 0, 0));
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
                    const __m256i tmp0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_lo, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_lo, 0b11110110));  //16-bit bit for zero coeffs level = 0
                    const __m256i tmp1 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_hi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_hi, 0b11110110));  //16-bit bit for zero coeffs level = 0
                    bits0 = _mm256_add_epi16(bits0, tmp0);
                    bits1 = _mm256_add_epi16(bits1, tmp1);
                }
                if (!z1) {
                    bits_lohi_lo = _mm256_shuffle_epi8(base_lohi, levels10);
                    bits_lohi_hi = _mm256_shuffle_epi8(base_lohi, levels11);
                    bits_lohi_lo = _mm256_blendv_epi8(zero, bits_lohi_lo, _mm256_permute4x64_epi64(mask1, 0b01000100));
                    bits_lohi_hi = _mm256_blendv_epi8(zero, bits_lohi_hi, _mm256_permute4x64_epi64(mask1, 0b11101110));
                    const __m256i tmp0 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_lo, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_lo, 0b11110110));  //16-bit bit for zero coeffs level = 0
                    const __m256i tmp1 = _mm256_unpacklo_epi8(_mm256_permute4x64_epi64(bits_lohi_hi, 0b11010100), _mm256_permute4x64_epi64(bits_lohi_hi, 0b11110110));  //16-bit bit for zero coeffs level = 0
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
        const int pos = *scan;
        const int32_t baseCtx = base_contexts[pos] >> 4;
        const int32_t level = base_contexts[pos] & 15;
        const int32_t baseLevel = (level < 3) ? level : 3;

        int c = 36 - (scan_e - scan);
        bits += bc.coeffBrAcc[br_contexts[pos]][level];
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
        static const int numCycles[19] = { 0,      4,      16,       32 * 2,      64 * 4,   0,      0,      8,       8,       16 * 2,   32,       32 * 4,   64 * 2,   4,       4,       8 * 2,   16,      16 * 4,  64 };

        //fraction of 16
        const int num = numCycles[bc.txSize];
        const __m256i y0f = _mm256_set1_epi16(0x0F0F);
        const __m256i yff = _mm256_set1_epi16(0xFFFF);
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

            __m256i scan = _mm256_load_si256((__m256i*)(scn + (i << 4)));
            const __m256i smask = _mm256_cmpgt_epi16(scan, y35);

            const unsigned int ncf = _mm256_movemask_epi8(_mm256_and_si256(mask, smask));
            num_nz_coeffs += (_mm_popcnt_u32(ncf) >> 1);

            scan = _mm256_and_si256(scan, smask);
            scan = _mm256_and_si256(scan, mask);
            max_scan = _mm256_max_epi16(max_scan, scan);

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
        max128 = _mm_andnot_si128(max128, _mm_setr_epi16(0xffff, 0, 0, 0, 0, 0, 0, 0));
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

