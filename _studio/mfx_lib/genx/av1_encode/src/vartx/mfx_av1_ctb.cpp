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

#include <limits.h> /* for INT_MAX on Linux/Android */
#include <algorithm>
#include <assert.h>
#include "mfx_av1_defs.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_quant.h"
#include "mfx_av1_copy.h"
#include "mfx_av1_bit_count.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_dispatcher_wrappers.h"

namespace H265Enc {
    TxSize get_sqr_tx_size(int tx_dim) {
        switch (tx_dim) {
        case 128: return TX_64X64; break;
        case 64: return TX_64X64; break;
        case 32: return TX_32X32; break;
        case 16: return TX_16X16; break;
        case 8: return TX_8X8; break;
        default: return TX_4X4;
        }
    }

    int32_t GetCtxTxfm(const uint8_t *aboveTxfm, const uint8_t *leftTxfm, BlockSize bsize, TxSize txSize) {
        const uint8_t txw = tx_size_wide[txSize];
        const uint8_t txh = tx_size_high[txSize];
        const int above = *aboveTxfm < txw;
        const int left = *leftTxfm < txh;
        int category = TXFM_PARTITION_CONTEXTS - 1;

        // dummy return, not used by others.
        if (txSize <= TX_4X4)
            return 0;

        TxSize maxTxSize = get_sqr_tx_size(IPP_MAX(block_size_wide[bsize], block_size_high[bsize]));

        if (maxTxSize >= TX_8X8)
            category = (txSize != maxTxSize && maxTxSize > TX_8X8) + (TX_SIZES - 1 - maxTxSize) * 2;
        if (category == TXFM_PARTITION_CONTEXTS - 1)
            return category;
        return category * 3 + above + left;
    }

    void txfm_partition_update(uint8_t *above_ctx, uint8_t *left_ctx, TxSize tx_size, TxSize txb_size) {
        const BlockSize bsize = txsize_to_bsize[txb_size];
        const int32_t bh = block_size_high_4x4[bsize];
        const int32_t bw = block_size_wide_4x4[bsize];
        const uint8_t txw = tx_size_wide[tx_size];
        const uint8_t txh = tx_size_high[tx_size];
        for (int32_t i = 0; i < bh; ++i)
            left_ctx[i] = txh;
        for (int32_t i = 0; i < bw; ++i)
            above_ctx[i] = txw;
    }

    const uint8_t skip_contexts[5][5] = {
        { 1, 2, 2, 2, 3 },
        { 1, 4, 4, 4, 5 },
        { 1, 4, 4, 4, 5 },
        { 1, 4, 4, 4, 5 },
        { 1, 4, 4, 4, 6 }
    };

    int32_t get_entropy_context(TxSize txSize, const uint8_t *a, const uint8_t *l)
    {
        switch (txSize) {
        case TX_4X4:   return !!*(const uint8_t  *)a + !!*(const uint8_t  *)l;
        case TX_8X8:   return !!*(const uint16_t *)a + !!*(const uint16_t *)l;
        case TX_16X16: return !!*(const uint32_t *)a + !!*(const uint32_t *)l;
        case TX_32X32: return !!*(const uint64_t *)a + !!*(const uint64_t *)l;
        default: assert(0 && "Invalid transform size."); return 0;
        }
    }

    int32_t GetTxbSkipCtx(BlockSize planeBsize, TxSize txSize, int32_t plane, const uint8_t *aboveCtx, const uint8_t *leftCtx)
    {
        if (plane == 0) {
            if (planeBsize == txsize_to_bsize[txSize]) {
                return 0;
            } else {
                const int32_t txbSize = 1 << txSize;

                int32_t top = 0;
                int32_t left = 0;
                #pragma novector
                #pragma nounroll
                for (int32_t k = 0; k < txbSize; k++) {
                    top |= aboveCtx[k];
                    left |= leftCtx[k];
                }

                top &= COEFF_CONTEXT_MASK;
                left &= COEFF_CONTEXT_MASK;

                const int32_t max = IPP_MIN(top | left, 4);
                const int32_t min = IPP_MIN(IPP_MIN(top, left), 4);

                return 1;
                //if (min == 0 && max == 0)
                //    return 1;
                //else if (min == 0 && max > 0)
                //    return 2;
                //else {
                //    assert(min > 0 && max > 0);
                //    return 4;
                //}

                //return skip_contexts[min][max];
            }
        } else {
            const int32_t ctx_base = get_entropy_context(txSize, aboveCtx, leftCtx);
            const int32_t ctx_offset =
                (num_pels_log2_lookup[planeBsize] > num_pels_log2_lookup[txsize_to_bsize[txSize]]) ? 10 : 7;
            return ctx_base + ctx_offset;
        }
    }

    void SetVarTxInfo(uint16_t *vartxInfo, uint16_t value, int32_t w) {
        assert(w == 1 || w == 2 || w == 4 || w == 8 || w == 16);
        if (w == 1) {
            *vartxInfo = value;
            return;
        }

        __m128i v = _mm_cvtsi32_si128(value);
        v = _mm_unpacklo_epi16(v, v);

        if (w == 2) {
            storel_si32(vartxInfo + 0 * 16, v);
            storel_si32(vartxInfo + 1 * 16, v);
            return;
        }

        v = _mm_unpacklo_epi32(v, v);

        if (w == 4) {
            storel_epi64(vartxInfo + 0 * 16, v);
            storel_epi64(vartxInfo + 1 * 16, v);
            storel_epi64(vartxInfo + 2 * 16, v);
            storel_epi64(vartxInfo + 3 * 16, v);
            return;
        }

        v = _mm_unpacklo_epi64(v, v);

        if (w == 8) {
            for (int32_t i = 0; i < 8; i++)
                storea_si128(vartxInfo + i * 16, v);
            return;
        }

        __m256i vv = _mm256_broadcastsi128_si256(v);
        for (int32_t i = 0; i < 16; i++)
            storea_si256(vartxInfo + i * 16, vv);
    }

    template <typename PixType>
    RdCost SearchTxType(int32_t bsz, TxSize txSize,
                        uint8_t *aboveNzCtx, uint8_t *leftNzCtx,
                        const PixType* src_, PixType *rec_, const int16_t *diff_,
                        int16_t *coeff_, int16_t *qcoeff_,
                        int32_t qp, const BitCounts &bc, const H265VideoParam &par, TxType& txType,
                        CostType lambda, int16_t *coeffWork_, int32_t y4, int32_t x4,
                        int32_t &bestCulLevel, const QuantParam &qpar)
    {
        const TxbBitCounts &cbc = bc.txb[ get_q_ctx(qp) ][txSize][PLANE_TYPE_Y];
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];

        const int32_t width = 4 << txSize;
        const int32_t step = 1 << txSize;
        const int32_t shiftTab[4] = {6, 6, 6, 4};
        const int32_t SHIFT_SSE = shiftTab[txSize];

        const PixType* src = src_ + (y4 << 2) * SRC_PITCH + (x4 << 2);
        const int32_t recPitch = 64;
        PixType *rec = rec_ + (y4 << 2) * recPitch + (x4 << 2);
        int32_t initSse = VP9PP::sse_p64_p64(src, rec, txSize, txSize);

        const int32_t diffPitch = 4 << log2w;
        const int16_t *diff = diff_ + (y4 << 2) * diffPitch + (x4 << 2);

        CoeffsType* coeffOrigin = coeffWork_;
        CoeffsType* coeffWork   = coeffWork_ + 32*32;
        CoeffsType* qcoeffWork  = coeffWork_ + 3*32*32;
        const int32_t txbSkipCtx = GetTxbSkipCtx(bsz, txSize, 0, aboveNzCtx + x4, leftNzCtx + y4);
        TxType endTxType = (txSize == TX_32X32) ? DCT_DCT : ADST_ADST - 1;
#if !REMOVE_ME_LATER
        endTxType = DCT_DCT;
#endif //REMOVE_ME_LATER

        CostType bestCost = COST_MAX;
        TxType bestTxType = DCT_DCT;
        int32_t bestEob = 0;
        uint32_t bestBitsNz = 0;
        int32_t bestSse = 0;
        const uint32_t bitsNz0 = cbc.txbSkip[txbSkipCtx][1];
        /*int32_t*/ bestCulLevel = 0;

        CoeffsType* qcoeffTest =  qcoeffWork + 32*32;
        CoeffsType* coeffTest  =  coeffWork  + 32*32;
        CoeffsType* qcoeffBest =  qcoeffWork + 2*32*32;
        CoeffsType* coeffBest  =  coeffWork  + 2*32*32;


        for (TxType txType = DCT_DCT; txType <= endTxType; txType++) {
            VP9PP::ftransform_av1(diff, coeffOrigin, diffPitch, txSize, txType);

            int32_t eob = VP9PP::quant(coeffOrigin, qcoeffTest, qpar, txSize);

            uint32_t bitsNz = bitsNz0;
            int32_t sse = initSse;
            int32_t culLevel = 0;
            if (eob) {
                VP9PP::dequant(qcoeffTest, coeffTest, qpar, txSize);

                sse = VP9PP::sse_cont(coeffOrigin, coeffTest, width*width) >> SHIFT_SSE;
                bitsNz = EstimateCoefsAv1(cbc, txbSkipCtx, eob, qcoeffTest, &culLevel);
#if REMOVE_ME_LATER
                bitsNz += bc.interExtTx[txSize][txType]; // + txTypeBits
#endif //REMOVE_ME_LATER
            } else
                sse = VP9PP::ssz_cont(coeffOrigin, width*width) >> SHIFT_SSE;
            CostType cost = sse + lambda * bitsNz;
            if (cost < bestCost) {
                bestTxType = txType;
                bestCost = cost;
                bestEob = eob;
                bestBitsNz = bitsNz;
                bestSse = sse;
                bestCulLevel = culLevel;
                std::swap(qcoeffTest, qcoeffBest);
                std::swap(coeffTest, coeffBest);
            }
        }
        txType = bestTxType;
        int32_t blockIdx = h265_scan_r2z4[y4 * 16 + x4];
        int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
        offset = 0;
        CoeffsType *coeff  = coeff_ + offset;
        CoeffsType *qcoeff = qcoeff_ + offset;
        int32_t bestOffset = bestTxType*32*32;
        if (bestEob) {
            CopyCoeffs(coeffBest,  coeff,  width * width);
            CopyCoeffs(qcoeffBest, qcoeff, width * width);
        } else {
            ZeroCoeffs(qcoeff, width * width);
        }

        if (bestEob == 0)
            txType = DCT_DCT;
        RdCost rd;
        rd.ssz = initSse;
        rd.sse = bestSse;
        rd.coefBits = bestBitsNz;
        rd.modeBits = 0;
        rd.eob = bestEob;
        return rd;
    }

    template <typename PixType>
    RdCost TransformVarTxByRdo(int32_t bsz, TxSize txSize,
                         uint8_t *aboveNzCtx, uint8_t *leftNzCtx,
                         const PixType* src_, PixType *rec_, const int16_t *diff_, int16_t *coeff_, int16_t *qcoeff_,
                         int32_t qp, const BitCounts &bc, const H265VideoParam &par,
                         CostType lambda, int16_t *coeffWork_, int32_t depth,
                         int32_t y4, int32_t x4,
                         uint8_t* aboveTxfmCtx, uint8_t* leftTxfmCtx,
                         uint16_t* vartxInfo, CostType costRef, const QuantParam &qpar, int miRow, int miCol)
    {
        const int32_t width = 4 << txSize;
        const int32_t ctx = GetCtxTxfm(aboveTxfmCtx + x4, leftTxfmCtx + y4, bsz, txSize);
        TxType txType;
        int32_t culLevel;
        int32_t isSplitAllow = txSize > TX_4X4 && depth < MAX_VARTX_DEPTH;
        ALIGNED(32) CoeffsType coeff0[32*32];
        ALIGNED(32) CoeffsType qcoeff0[32*32];
        int32_t blockIdx = h265_scan_r2z4[y4 * 16 + x4];
        int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
        CoeffsType *coeff  = coeff_ + offset;
        CoeffsType *qcoeff = qcoeff_ + offset;


        RdCost rd = SearchTxType(bsz, txSize,
            aboveNzCtx, leftNzCtx,
            src_, rec_, diff_,
            isSplitAllow ? coeff0  : coeff,
            isSplitAllow ? qcoeff0 : qcoeff,
            qp, bc, par,
            txType,
            lambda, coeffWork_,
            y4, x4,
            culLevel, qpar);
#if REMOVE_ME_LATER
        if (txSize > TX_4X4 && depth < MAX_VARTX_DEPTH) {
            rd.modeBits = bc.txfm_partition_cost[ctx][0];
        }
#endif //REMOVE_ME_LATER
        CostType cost = rd.sse + lambda * (rd.coefBits + rd.modeBits);

        //if (costRef < cost) {
        //    return rd;
        //}

        CostType costSplit = COST_MAX;
        RdCost bestRd = {};
        if (isSplitAllow) {
            if (rd.eob) {
                const TxSize sub_txs = sub_tx_size_map[1][txSize];
                const int bsw = tx_size_wide_unit[sub_txs];
                const int bsh = tx_size_high_unit[sub_txs];
#if REMOVE_ME_LATER
                bestRd.modeBits = bc.txfm_partition_cost[ctx][1];
#endif //REMOVE_ME_LATER
                costSplit = 0.f;

                for (int32_t idx = 0; idx < 4; idx++) {
                    int32_t r = (idx >> 1) * bsh;
                    int32_t c = (idx & 0x01) * bsw;
                    RdCost rdPart = TransformVarTxByRdo(bsz, sub_txs,
                        aboveNzCtx, leftNzCtx,
                        src_, rec_, diff_,
                        coeff_, qcoeff_,
                        qp, bc, par,
                        lambda, coeffWork_, depth + 1,
                        y4 + r, x4 + c,
                        aboveTxfmCtx, leftTxfmCtx,
                        vartxInfo, cost - costSplit, qpar, miRow + r/2, miCol + c/2);

                    bestRd.coefBits += rdPart.coefBits;
                    bestRd.eob += rdPart.eob;
                    bestRd.modeBits += rdPart.modeBits;
                    bestRd.sse += rdPart.sse;
                    bestRd.ssz += rdPart.ssz;
                    costSplit = (bestRd.sse + lambda * (bestRd.coefBits + bestRd.modeBits));
                    if (costSplit > cost) break;
                }
            }
            if (costSplit > cost) {
                CopyCoeffs(coeff0,  coeff,  width * width);
                CopyCoeffs(qcoeff0, qcoeff, width * width);
            }
        }
        if (costSplit > cost) {
            SetCulLevel(aboveNzCtx + x4, leftNzCtx + y4, culLevel, txSize);
            txfm_partition_update(aboveTxfmCtx + x4, leftTxfmCtx + y4, txSize, txSize);

#ifndef ADAPTIVE_DEADZONE
            int32_t data = txType + (txSize << 2) + ((!!rd.eob) << 4);
#else
            int32_t data = txType + (txSize << 2) + ((rd.eob&0x7ff) << 4);
#endif
            SetVarTxInfo(vartxInfo + y4 * 16 + x4, data, tx_size_wide_unit[txSize]);
            bestRd = rd;
        }
        return bestRd;
    }

    template <typename PixType>
    RdCost TransformInterYSbVarTxByRdo(int32_t bsz, TxSize txSize, uint8_t *aboveNzCtx, uint8_t *leftNzCtx,
        const PixType* src_, PixType *rec_, const int16_t *diff_, int16_t *coeff_, int16_t *qcoeff_,
        int32_t qp, const BitCounts &bc, const H265VideoParam &par, TxType *chromaTxType,
        CostType lambda, int16_t *coeffWork_, uint8_t* aboveTxfmCtx, uint8_t* leftTxfmCtx,
        uint16_t* vartxInfo, const QuantParam &qpar, float *roundFAdj, int miRow, int miCol)
    {
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t num4x4h = 1 << log2h;
        const int32_t step = 1 << txSize;
        const int32_t idxEnd = num4x4w == step ? 1 : 4;
        int32_t initDepth = max_txsize_lookup[bsz] - txSize;// 1 - 64x64, 0 - others
        RdCost rd = {};

        for (int32_t idx = 0; idx < idxEnd; idx++) {
            int32_t y4 = (idx >> 1) * step;
            int32_t x4 = (idx & 0x01) * step;
            RdCost loc = TransformVarTxByRdo(bsz, txSize,
                aboveNzCtx, leftNzCtx,
                src_, rec_, diff_,
                coeff_, qcoeff_,
                qp, bc, par,
                lambda, coeffWork_,
                initDepth,
                y4, x4,
                aboveTxfmCtx,
                leftTxfmCtx,
                vartxInfo, COST_MAX, qpar, miRow + y4/2, miCol + x4/2);

            rd.coefBits += loc.coefBits;
            rd.eob      += loc.eob;
            rd.modeBits += loc.modeBits;
            rd.sse      += loc.sse;
            rd.ssz      += loc.ssz;

            InvTransformVarTx(bsz, txSize,rec_, diff_, coeff_, coeffWork_, y4, x4, vartxInfo, qpar, roundFAdj);
        }
        return rd;
    }

    template
    RdCost TransformInterYSbVarTxByRdo<uint8_t>(int32_t bsz, TxSize txSize, uint8_t *aboveNzCtx, uint8_t *leftNzCtx,
        const uint8_t* src_, uint8_t *rec_, const int16_t *diff_, int16_t *coeff_, int16_t *qcoeff_,
        int32_t qp, const BitCounts &bc, const H265VideoParam &par, TxType *chromaTxType,
        CostType lambda, int16_t *coeffWork_, uint8_t* aboveTxfmCtx, uint8_t* leftTxfmCtx,
        uint16_t* vartxInfo, const QuantParam &qpar, float *roundFAdj, int miRow, int miCol);

    template <typename PixType>
    void InvTransformVarTx(int32_t bsz, TxSize txSize, PixType *rec_, const int16_t *diff_, int16_t *coeff_, int16_t *coeffWork_,
        int32_t y4, int32_t x4, uint16_t* vartxInfo, const QuantParam &qpar, float *roundFAdj)
    {
        uint16_t data = vartxInfo[y4 * 16 + x4];
        TxSize txSz = (data >> 2) & 0x03;
        TxType txTp = (data) & 0x03;
#ifndef ADAPTIVE_DEADZONE
        int32_t eob  = (data >> 4) & 0x01;
#else
        int32_t eob = (data >> 4);
#endif
        if (txSz == txSize) {
            if (eob) {
                const int32_t log2w = block_size_wide_4x4_log2[bsz];
                const int32_t recPitch = 64;
                PixType *rec = rec_ + (y4 << 2) * recPitch + (x4 << 2);
                int32_t blockIdx = h265_scan_r2z4[y4 * 16 + x4];
                int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
                CoeffsType *coeff  = coeff_ + offset;
#ifdef ADAPTIVE_DEADZONE
                CoeffsType* coeffOrigin = coeffWork_;
                const int32_t diffPitch = 4 << log2w;
                const int16_t *diff = diff_ + (y4 << 2) * diffPitch + (x4 << 2);

                VP9PP::ftransform_av1(diff, coeffOrigin, diffPitch, txSize, txTp);
                adaptDz(coeffOrigin, coeff, reinterpret_cast<const int16_t *>(&qpar), txSize, &roundFAdj[0], eob);
#endif
                VP9PP::itransform_add_av1(coeff, rec, recPitch, txSize, txTp);
            }
        } else {
            const TxSize sub_txs = sub_tx_size_map[1][txSize];
            const int bsw = tx_size_wide_unit[sub_txs];
            const int bsh = tx_size_high_unit[sub_txs];
            InvTransformVarTx(bsz, sub_txs, rec_, diff_, coeff_, coeffWork_, y4, x4, vartxInfo, qpar, roundFAdj);
            InvTransformVarTx(bsz, sub_txs, rec_, diff_, coeff_, coeffWork_, y4, x4 + bsw, vartxInfo, qpar, roundFAdj);
            InvTransformVarTx(bsz, sub_txs, rec_, diff_, coeff_, coeffWork_, y4 + bsh, x4, vartxInfo, qpar, roundFAdj);
            InvTransformVarTx(bsz, sub_txs, rec_, diff_, coeff_, coeffWork_, y4 + bsh, x4 + bsw, vartxInfo, qpar, roundFAdj);
        }
    }
};