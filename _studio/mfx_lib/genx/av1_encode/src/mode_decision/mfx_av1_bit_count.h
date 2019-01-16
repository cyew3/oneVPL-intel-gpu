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

#pragma once

#include "mfx_av1_defs.h"
#include <stdint.h>

namespace H265Enc {
    // entropy = int(-log2(prob/256.0) * 512 + 0.5)
    // prob2bits[0] and prob2bits[256] are there just for convinience
    const uint16_t prob2bits[257] = {
        4096, 4096, 3584, 3284, 3072, 2907, 2772, 2659, 2560, 2473, 2395, 2325, 2260, 2201, 2147, 2096,
        2048, 2003, 1961, 1921, 1883, 1847, 1813, 1780, 1748, 1718, 1689, 1661, 1635, 1609, 1584, 1559,
        1536, 1513, 1491, 1470, 1449, 1429, 1409, 1390, 1371, 1353, 1335, 1318, 1301, 1284, 1268, 1252,
        1236, 1221, 1206, 1192, 1177, 1163, 1149, 1136, 1123, 1110, 1097, 1084, 1072, 1059, 1047, 1036,
        1024, 1013, 1001,  990,  979,  968,  958,  947,  937,  927,  917,  907,  897,  887,  878,  868,
         859,  850,  841,  832,  823,  814,  806,  797,  789,  780,  772,  764,  756,  748,  740,  732,
         724,  717,  709,  702,  694,  687,  680,  673,  665,  658,  651,  644,  637,  631,  624,  617,
         611,  604,  598,  591,  585,  578,  572,  566,  560,  554,  547,  541,  535,  530,  524,  518,
         512,  506,  501,  495,  489,  484,  478,  473,  467,  462,  456,  451,  446,  441,  435,  430,
         425,  420,  415,  410,  405,  400,  395,  390,  385,  380,  375,  371,  366,  361,  356,  352,
         347,  343,  338,  333,  329,  324,  320,  316,  311,  307,  302,  298,  294,  289,  285,  281,
         277,  273,  268,  264,  260,  256,  252,  248,  244,  240,  236,  232,  228,  224,  220,  216,
         212,  209,  205,  201,  197,  194,  190,  186,  182,  179,  175,  171,  168,  164,  161,  157,
         153,  150,  146,  143,  139,  136,  132,  129,  125,  122,  119,  115,  112,  109,  105,  102,
          99,   95,   92,   89,   86,   82,   79,   76,   73,   70,   66,   63,   60,   57,   54,   51,
          48,   45,   42,   38,   35,   32,   29,   26,   23,   20,   18,   15,   12,    9,    6,    3,
           0};

    struct CoefBitCounts {
        uint8_t txSize, plane, inter, reserved;
        uint32_t moreCoef[COEF_BANDS][PREV_COEF_CONTEXTS][2];
        uint32_t token[COEF_BANDS][PREV_COEF_CONTEXTS][NUM_TOKENS];
        uint32_t tokenMc[COEF_BANDS][PREV_COEF_CONTEXTS][NUM_TOKENS]; // bits for more_coef=1 + token
    };

    struct TxbBitCounts {
        uint8_t qpi, txSize, plane, reserved;
        uint16_t coeffBase[16][NUM_BASE_LEVELS + 2];
        uint16_t coeffBaseEob[SIG_COEF_CONTEXTS_EOB][NUM_BASE_LEVELS + 1];
        //uint16_t coeffBr[LEVEL_CONTEXTS][BR_CDF_SIZE];
        uint16_t coeffBrAcc[LEVEL_CONTEXTS][16];
        uint16_t eobMulti[11];
        uint16_t txbSkip[TXB_SKIP_CONTEXTS][2];
    };

    struct BitCounts {
        uint32_t vp9Partition[4][VP9_PARTITION_CONTEXTS][PARTITION_TYPES];
        uint32_t av1Partition[4][AV1_PARTITION_CONTEXTS][EXT_PARTITION_TYPES];
        uint32_t skip[SKIP_CONTEXTS][2];
        uint32_t txSize[TX_SIZES][TX_SIZE_CONTEXTS][TX_SIZES];
        uint32_t kfIntraMode[INTRA_MODES][INTRA_MODES][INTRA_MODES];
        uint32_t intraMode[BLOCK_SIZE_GROUPS][INTRA_MODES];
        uint32_t intraModeUv[INTRA_MODES][INTRA_MODES];
        CoefBitCounts coef[BLOCK_TYPES][REF_TYPES][TX_SIZES];
        uint32_t interMode[INTER_MODE_CONTEXTS][INTER_MODES];
        uint32_t newMvBit[NEWMV_MODE_CONTEXTS][2];
        uint32_t zeroMvBit[GLOBALMV_MODE_CONTEXTS][2];
        uint32_t refMvBit[REFMV_MODE_CONTEXTS][2];
        uint32_t interCompMode[INTER_MODE_CONTEXTS][AV1_INTER_COMPOUND_MODES];
        uint32_t drlIdx[DRL_MODE_CONTEXTS][DRL_MODE_CONTEXTS][NUM_DRL_INDICES]; // [1-st bit ctx] x [2-nd bit ctx]
        uint32_t drlIdxNewMv[5][NUM_DRL_INDICES]; // [0 to 4 nearest Mvs]
        uint32_t drlIdxNearMv[5][NUM_DRL_INDICES]; // [0 to 4 nearest Mvs]

        uint32_t interpFilterAV1[AV1_INTERP_FILTER_CONTEXTS][SWITCHABLE_FILTERS];
        uint32_t interpFilterVP9[VP9_INTERP_FILTER_CONTEXTS][SWITCHABLE_FILTERS];

        uint32_t isInter[IS_INTER_CONTEXTS][2];
        uint32_t compMode[COMP_MODE_CONTEXTS][2];
        uint32_t vp9CompRef[VP9_REF_CONTEXTS][2];
        uint32_t vp9SingleRef[VP9_REF_CONTEXTS][2][2];
        uint32_t av1CompRef[AV1_REF_CONTEXTS][FWD_REFS - 1][2];
        uint32_t av1CompRefType[COMP_REF_TYPE_CONTEXTS][2];
        uint32_t av1CompBwdRef[AV1_REF_CONTEXTS][BWD_REFS - 1][2];
        uint32_t av1SingleRef[AV1_REF_CONTEXTS][SINGLE_REFS - 1][2];
        uint32_t mvJoint[4];
        uint32_t mvSign[2][2];
        uint32_t mvClass[2][VP9_MV_CLASSES];
        uint32_t mvClass0Bit[2][2];
        uint32_t mvClass0Fr[2][CLASS0_SIZE][4];
        uint32_t mvBits[2][MV_OFFSET_BITS][2];
        uint32_t mvClass0Hp[2][2];
        uint32_t mvFr[2][4];
        uint32_t mvHp[2][2];
        uint32_t mvClass0[2][2][16];

        uint32_t intraExtTx[TX_SIZES][TX_TYPES][TX_TYPES];
        uint32_t interExtTx[TX_SIZES][TX_TYPES];
        uint32_t kfIntraModeAV1[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS][AV1_INTRA_MODES];
        uint32_t intraModeAV1[BLOCK_SIZE_GROUPS][AV1_INTRA_MODES];
        uint32_t intraModeUvAV1[AV1_INTRA_MODES][AV1_UV_INTRA_MODES];

        TxbBitCounts txb[TOKEN_CDF_Q_CTXS][TX_32X32 + 1][PLANE_TYPES];
    };

    struct RefFrameContextsAv1 {
        uint8_t singleRefP1;
        uint8_t singleRefP2;
        uint8_t singleRefP3;
        uint8_t singleRefP4;
        uint8_t singleRefP5;
        uint8_t refMode;
        uint8_t compRefType;
        uint8_t compFwdRefP0;
        uint8_t compFwdRefP1;
        uint8_t compBwdRefP0;
    };

    void EstimateBits(BitCounts &bits, int32_t intraOnly, EnumCodecType codecType);
    void GetRefFrameContextsVp9(Frame *frame);
    void GetRefFrameContextsAv1(Frame *frame);

    uint32_t EstimateCoefsFastest(const CoefBitCounts &bits, const CoeffsType *coefs, const int16_t *scan, int32_t eob, int32_t dcCtx);
    template <int32_t txType> uint32_t EstimateCoefs(const CoefBitCounts &bits, const CoeffsType *coefs, const int16_t *scan, int32_t eob, int32_t dcCtx);

    inline uint32_t EstimateCoefs(int32_t fastCoeffCost, const CoefBitCounts &bits, const CoeffsType *coefs, const int16_t *scan, int32_t eob, int32_t dcCtx, int32_t txType) {
        if (fastCoeffCost) {
            return EstimateCoefsFastest(bits, coefs, scan, eob, dcCtx);
        } else {
            if (txType == DCT_DCT) return EstimateCoefs<DCT_DCT>(bits, coefs, scan, eob, dcCtx);
            if (txType == DCT_ADST) return EstimateCoefs<DCT_ADST>(bits, coefs, scan, eob, dcCtx);
            if (txType == ADST_DCT) return EstimateCoefs<ADST_DCT>(bits, coefs, scan, eob, dcCtx);
            assert(txType == ADST_ADST); return EstimateCoefs<ADST_ADST>(bits, coefs, scan, eob, dcCtx);
        }
    }

    uint32_t EstimateCoefsAv1(const TxbBitCounts &bc, int32_t txbSkipCtx, int32_t numNonZero, const CoeffsType *coeffs, int32_t *culLevel);
};
