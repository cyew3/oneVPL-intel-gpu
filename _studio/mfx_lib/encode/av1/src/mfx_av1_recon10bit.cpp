// Copyright (c) 2019 Intel Corporation
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

#include "cassert"
#include "algorithm"
#include "mfx_av1_defs.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_encode.h"
#include "mfx_av1_quant.h"
#include "mfx_av1_copy.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_dispatcher_wrappers.h"
#include "mfx_av1_get_intra_pred_pels.h"
#include "mfx_av1_get_context.h"

using namespace AV1Enc;

template <typename PixType> void AV1CU<PixType>::MakeRecon10bitIAV1()
{
    alignas(64) uint16_t bestRec[64 * 64];
    alignas(16) uint8_t savedPartitionCtx[16];
    storea_si128(savedPartitionCtx, loada_si128(m_contexts.aboveAndLeftPartition));
    m_contexts = m_contextsInitSb;

    for (int32_t i = 0; i < MAX_NUM_PARTITIONS;) {
        const int32_t rasterIdx = av1_scan_z2r4[i];
        const int32_t x4 = (rasterIdx & 15);
        const int32_t y4 = (rasterIdx >> 4);
        const int32_t sbx = x4 << 2;
        const int32_t sby = y4 << 2;
        ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
        const int32_t num4x4 = num_4x4_blocks_lookup[mi->sbType];
        if (mi->mode == OUT_OF_PIC) {
            i += num4x4;
            continue;
        }

        const BlockSize bsz = mi->sbType;
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t num4x4h = 1 << log2h;
        const TxSize maxTxSize = max_txsize_lookup[bsz];

        float roundFAdjYInit[2] = { m_roundFAdjY[0], m_roundFAdjY[1] };

        const BitCounts &bc = *m_currFrame->bitCount;
        const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
        const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);

        const int32_t haveAbove = (miRow > m_tileBorders.rowStart);
        const int32_t haveLeft = (miCol > m_tileBorders.colStart);
        const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
        const ModeInfo *left = GetLeft(mi, miCol, m_tileBorders.colStart);
        const int32_t ctxSkip = GetCtxSkip(above, left);
        const uint8_t aboveTxfm = above ? tx_size_wide[above->txSize] : 0;
        const uint8_t leftTxfm = left ? tx_size_high[left->txSize] : 0;
        const int32_t ctxTxSize = GetCtxTxSizeAV1(above, left, aboveTxfm, leftTxfm, maxTxSize);
        const uint16_t *skipBits = bc.skip[ctxSkip];
        const uint16_t *txSizeBits = bc.txSize[maxTxSize][ctxTxSize];
        const Contexts origCtx = m_contexts; // contexts at the start of current block
        Contexts bestCtx;
        uint8_t *anz = m_contexts.aboveNonzero[0] + x4;
        uint8_t *lnz = m_contexts.leftNonzero[0] + y4;

        const int32_t pitchRecLuma = m_pitchRecLuma10;
        uint16_t *recSb = m_yRec10 + sbx + sby * pitchRecLuma;
        uint16_t *srcSb = m_ySrc10 + sbx + sby * SRC_PITCH;

        int16_t *diff = vp9scratchpad.diffY;
#if ENABLE_HIBIT_COEFS
        int32_t *coef = vp9scratchpad.coefY_hbd;
        int32_t *coefBest = vp9scratchpad.dqcoefY_hbd;
#else
        int16_t *coef = vp9scratchpad.coefY;
        int16_t *coefBest = vp9scratchpad.dqcoefY;
#endif
        int16_t *qcoef = vp9scratchpad.qcoefY[0];
        int16_t *bestQcoef = vp9scratchpad.qcoefY[1];
#if ENABLE_HIBIT_COEFS
        int32_t *coefWork = vp9scratchpad.varTxCoefs[0][0].coef_hbd;
        int32_t *coefWorkBest = vp9scratchpad.varTxCoefs[1][0].coef_hbd;
#else
        int16_t *coefWork = vp9scratchpad.varTxCoefs[0][0].coef;
        int16_t *coefWorkBest = vp9scratchpad.varTxCoefs[1][0].coef;
#endif
        int16_t *eob4x4 = (int16_t *)vp9scratchpad.varTxCoefs[2][0].coef;
        int16_t *eob4x4Best = (int16_t *)vp9scratchpad.varTxCoefs[2][1].coef;
        //float scpp = 0;
        //int32_t scid = GetSpatialComplexity(sbx, sby, log2w, &scpp);
        //int32_t minTxSize = std::max((int32_t)(scid ? TX_4X4 : TX_8X8), mi->txSize - MAX_TX_DEPTH);
        int32_t miColEnd = m_tileBorders.colEnd;

        const int32_t filtType = get_filt_type(above, left, PLANE_TYPE_Y);
        const int32_t max_angle_delta = av1_is_directional_mode(mi->mode) ? MAX_ANGLE_DELTA : 0;
        PaletteInfo *pi = &m_currFrame->m_fenc->m_Palette8x8[miCol + miRow * m_par->miPitch]; // Y Decided in Mode Decision
        pi->palette_size_uv = 0;

        CostType bestCost = COST_MAX;
        RdCost bestRdCost = {};
        TxSize bestTxSize = mi->txSize;
        TxType bestTxType = mi->txType;
        TxSize txSize = mi->txSize;
        TxType txType = mi->txType;
        int32_t bestAngleDelta = mi->angle_delta_y - max_angle_delta;

#if !USE_HWPAK_RESTRICT
        alignas(32) uint16_t poolTxkTypes[4][7][16 * 16];
        for (int32_t txSize = mi->txSize; txSize >= minTxSize; txSize--) {

            for (int delta = -max_angle_delta; delta <= max_angle_delta; delta++) {
                float roundFAdjT[2] = { roundFAdjYInit[0], roundFAdjYInit[1] };

                m_contexts = origCtx;
                RdCost rd = TransformIntraYSbAv1_viaTxkSearch(bsz, mi->mode, haveAbove, haveLeft, txSize,
                    anz, lnz, srcSb, recSb, m_pitchRecLuma, diff, coef, qcoef, m_lumaQp,
                    bc, m_par->FastCoeffCost, m_rdLambda, miRow, miCol, miColEnd,
                    m_par->miRows, m_par->miCols, delta, filtType, poolTxkTypes[txSize][delta + max_angle_delta],
                    *m_par, coefWork, m_aqparamY, &roundFAdjT[0], pi, NULL);

                rd.modeBits = txSizeBits[txSize];
                rd.modeBits += write_uniform_cost(2 * max_angle_delta + 1, delta + max_angle_delta);
                CostType cost = (rd.eob == 0) // if luma has no coeffs, assume skip
                    ? rd.sse + m_rdLambda * (rd.modeBits + skipBits[1])
                    : rd.sse + m_rdLambda * (rd.modeBits + skipBits[0] + rd.coefBits);

                fprintf_trace_cost(stderr, "intra luma refine: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d tx=%d "
                    "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, i, bsz, mi->mode, txSize,
                    rd.sse, rd.modeBits, rd.eob, rd.coefBits, cost);

                if (bestCost > cost) {
                    bestCost = cost;
                    bestRdCost = rd;
                    bestTxSize = txSize;
                    bestAngleDelta = delta;
                    std::swap(qcoef, bestQcoef);

                    bestCtx = m_contexts;
                    CopyNxM(recSb, m_pitchRecLuma, bestRec, num4x4w << 2, num4x4h << 2);

                    m_roundFAdjY[0] = roundFAdjT[0];
                    m_roundFAdjY[1] = roundFAdjT[1];
                }
            }
        }

        const uint16_t *srcTab = poolTxkTypes[bestTxSize][bestAngleDelta + max_angle_delta];
        CopyVarTxInfo(srcTab, m_currFrame->m_fenc->m_txkTypes4x4 + m_ctbAddr * 256 + (miRow & 7) * 32 + (miCol & 7) * 2, num4x4w);
#else
        if (!is_intrabc_block(mi)) {
            if (pi->palette_size_y && pi->palette_size_y == pi->true_colors_y)
            {
                int width = num4x4w << 2;
                bestRdCost.modeBits = txSizeBits[maxTxSize];
                bestCost = m_rdLambda * (bestRdCost.modeBits + skipBits[1]);
                bestTxSize = (TxSize)maxTxSize;
                bestTxType = DCT_DCT;
                bestAngleDelta = 0;
                memset(bestQcoef, 0, sizeof(CoeffsType)*width*width);
                bestCtx = origCtx;
                SetCulLevel(anz, lnz, 0, maxTxSize);
                CopyNxN(srcSb, SRC_PITCH, bestRec, width);
                m_roundFAdjY[0] = roundFAdjYInit[0];
                m_roundFAdjY[1] = roundFAdjYInit[1];
            }
            else {

                //TxType txTypeStart = DCT_DCT;
                //TxType txTypeEnd = ADST_ADST;
                //TxType txTypeList[] = { DCT_DCT,  ADST_DCT, DCT_ADST, ADST_ADST, IDTX };

                //for (int32_t txSize = maxTxSize; txSize >= minTxSize; txSize--)
                {
                    //for (auto txType : txTypeList)
                    {
                        //if (txSize == TX_32X32 && txType != DCT_DCT)
                        //    continue;
                        //if (pi->palette_size_y && txType != DCT_DCT && txType != IDTX)
                        //    continue;
                        //if (!pi->palette_size_y && txType == IDTX)
                        //    continue;
                        //if (txType == ADST_ADST && bestTxType == DCT_DCT)
                        //    continue;

                        //int deltaStart = bestAngleDelta;
                        //int deltaEnd = bestAngleDelta;
                        //int deltaStep = 1;
                        //int numPasses = 1;
                        //if (av1_is_directional_mode(mi->mode)) {
                        //    /*if (txSize == maxTxSize && txType == txTypeStart) {
                        //        // Search again
                        //        deltaStart = -2;
                        //        deltaEnd = 2;
                        //        deltaStep = 2;
                        //        numPasses = 2;
                        //    }
                        //    else*/
                        //    if (txSize == maxTxSize || (maxTxSize == TX_32X32 && txSize == maxTxSize - 1)) {
                        //        deltaStart = std::max(-3, bestAngleDelta - 1);
                        //        deltaEnd = std::min(3, bestAngleDelta + 1);
                        //        deltaStep = 1;
                        //        numPasses = 1;
                        //    }
                        //}

                        //int localBestAngle = 0;
                        //CostType localBestCost = COST_MAX;
                        //for (int pass = 0; pass < numPasses; pass++)
                        {
                            //for (int delta = deltaStart; delta <= deltaEnd; delta += deltaStep)
                            int delta = bestAngleDelta;//0;
                            {
                                float roundFAdjT[2] = { roundFAdjYInit[0], roundFAdjYInit[1] };

                                m_contexts = origCtx;
                                QuantParam aqparamY10 = m_par->qparamY10[m_currFrame->m_sliceQpY];
                                //aqparamY10.dequant[0] =

                                RdCost rd = TransformIntraYSbAv1<uint16_t>(bsz, mi->mode, haveAbove, haveLeft, (TxSize)txSize, txType,
                                    anz, lnz, (uint16_t*)srcSb, (uint16_t*)recSb, pitchRecLuma, diff, coef, qcoef, coefWork,
                                    m_lumaQp, bc, m_rdLambda, miRow, miCol, miColEnd,
                                    delta, filtType,
                                    *m_par,
                                    aqparamY10,
                                    //m_aqparamY,
                                    NULL, //&roundFAdjT[0],
                                    pi, eob4x4);

                                const TxType txTypeNom = GetTxTypeAV1(PLANE_TYPE_Y, (TxSize)txSize, mi->mode, txType);
                                const uint16_t *txTypeBits = (txType == IDTX) ? bc.intra_tx_type_costs[2][txSize][mi->mode] : bc.intraExtTx[txSize][txTypeNom];

                                rd.modeBits = txSizeBits[txSize];
                                if (av1_is_directional_mode(mi->mode)) {
                                    rd.modeBits += write_uniform_cost(2 * max_angle_delta + 1, delta + max_angle_delta);
                                }
                                CostType cost = (rd.eob == 0) // if luma has no coeffs, assume skip
                                    ? rd.sse + m_rdLambda * (rd.modeBits + skipBits[1])
                                    : rd.sse + m_rdLambda * (rd.modeBits + skipBits[0] + rd.coefBits + txTypeBits[txType]);

                                fprintf_trace_cost(stderr, "intra luma refine: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d tx=%d "
                                    "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, i, bsz, mi->mode, txSize,
                                    rd.sse, rd.modeBits, rd.eob, rd.coefBits, cost);

                                const int32_t sbw = num4x4w << 2;

                                /*if (localBestCost > cost) {
                                    localBestCost = cost;
                                    localBestAngle = delta;
                                }*/

                                //if (bestCost > cost)
                                {
                                    bestRdCost = rd;
                                    bestCost = cost;
                                    //bestEob = rd.eob;
                                    bestTxSize = (TxSize)txSize;
                                    bestTxType = txType;
                                    bestAngleDelta = delta;
                                    std::swap(qcoef, bestQcoef);
                                    bestCtx = m_contexts;
                                    CopyNxN(recSb, pitchRecLuma, bestRec, sbw);
                                    std::swap(coefWork, coefWorkBest);
                                    std::swap(coef, coefBest);
                                    std::swap(eob4x4, eob4x4Best);
                                    //m_roundFAdjY[0] = roundFAdjT[0];
                                    //m_roundFAdjY[1] = roundFAdjT[1];
                                }
                            }
                            //delta_start = delta_stop = bestAngleDelta;
                            //deltaStart = localBestAngle - 1;
                            //deltaEnd = localBestAngle + 1;
                        }
                        //if (pi->palette_size_y && pi->palette_size_y == pi->true_colors_y) break;
                        //if (bestRdCost.eob == 0 && bestRdCost.sse == 0) break;
                    }
                    //if (pi->palette_size_y && pi->palette_size_y == pi->true_colors_y) break;
                    //if (bestRdCost.eob == 0 && m_rdLambda * (bestRdCost.modeBits + skipBits[1]) > bestRdCost.sse) break;
                    //if (bestRdCost.eob != 0 && txSize < bestTxSize) break;
                }
#ifdef ADAPTIVE_DEADZONE
                if (bestRdCost.eob) {
                    int32_t step = 1 << bestTxSize;
                    for (int32_t y = 0; y < num4x4h; y += step) {
                        for (int32_t x = 0; x < num4x4w; x += step) {
                            int32_t blockIdx = h265_scan_r2z4[y * 16 + x];
                            int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
#if ENABLE_HIBIT_COEFS
                            int32_t *coeff = coefBest + offset;
                            int32_t *coefOrigin = coefWorkBest + offset;
#else
                            int16_t *coeff = coefBest + offset;
                            int16_t *coefOrigin = coefWorkBest + offset;
#endif
                            if (eob4x4Best[y * 16 + x])
                                adaptDz(coefOrigin, coeff, m_aqparamY, bestTxSize, m_roundFAdjY, eob4x4Best[y * 16 + x]);
                        }
                    }
                }
#endif
            }
        }
#endif
        int use_intrabc = 0;
        alignas(32) uint16_t varTxInfo[16 * 16];

        if (m_currFrame->m_allowIntraBc)
        {
            m_contexts = origCtx;
            float bestRoundFAdjT[2] = { m_roundFAdjY[0], m_roundFAdjY[1] };
            m_roundFAdjY[0] = roundFAdjYInit[0], m_roundFAdjY[1] = roundFAdjYInit[1];

            RdCost rd = {};
            int32_t test = 0;
            if (!m_par->ibcModeDecision)
                test = CheckIntraBlockCopy(i, qcoef, rd, varTxInfo, true, COST_MAX);
            else if (is_intrabc_block(mi)) {
                const int32_t intx = mi->mv[0].mvx >> 3;
                const int32_t inty = mi->mv[0].mvy >> 3;
                const int32_t sbw = num4x4w << 2;
                const uint16_t *bestPredY = recSb + intx + inty * pitchRecLuma;

                uint16_t *rec = (uint16_t*)vp9scratchpad.recY[0];
                diff = vp9scratchpad.diffY;
                CopyFromUnalignedNxN(bestPredY, pitchRecLuma, rec, 64, sbw);
                AV1PP::diff_nxn(srcSb, rec, diff, log2w);

                uint8_t *aboveTxfmCtx = m_contexts.aboveTxfm + x4;
                uint8_t *leftTxfmCtx = m_contexts.leftTxfm + y4;
                QuantParam aqparamY10 = m_par->qparamY10[m_currFrame->m_sliceQpY];

                rd = TransformInterYSbVarTxByRdo(
                    bsz, anz, lnz,
                    srcSb, rec, diff, qcoef,
                    m_lumaQp, bc, m_rdLambda,
                    aboveTxfmCtx, leftTxfmCtx,
                    varTxInfo,
                    aqparamY10, m_roundFAdjY,
                    vp9scratchpad.varTxCoefs, MAX_VARTX_DEPTH, TX_TYPES - 1);

                CopyNxN(rec, 64, recSb, pitchRecLuma, sbw);

                AV1MV dv_ref;

                if (bsz == BLOCK_32X32) {
                    dv_ref = GetMvRefsAV1TU7I(bsz, miRow, miCol);
                    mi->mvd[0].mvx = mi->mv[0].mvx - dv_ref.mvx;
                    mi->mvd[0].mvy = mi->mv[0].mvy - dv_ref.mvy;
                }
                else {
                    dv_ref = { mi->mv[0].mvx - mi->mvd[0].mvx, mi->mv[0].mvy - mi->mvd[0].mvy };
                }
                const int32_t rate_mv = MvCost(mi->mv[0], dv_ref, 0);
                const int32_t rate_mode = bc.intrabc_cost[1];

                uint32_t modeBits = uint32_t(rate_mv + rate_mode);
                rd.modeBits = modeBits;

                //const int txSize = max_txsize_lookup[bsz];
                rd.sse = AV1PP::sse(srcSb, SRC_PITCH, recSb, pitchRecLuma, txSize, txSize);
                test = 1;
            }
            if (test) {
                const int32_t aboveMode = (above ? above->mode : DC_PRED);
                const int32_t leftMode = (left ? left->mode : DC_PRED);
                const uint16_t *intraModeBits = bc.kfIntraModeAV1[intra_mode_context[aboveMode]][intra_mode_context[leftMode]];
                if (bestCost < COST_MAX)
                    bestCost += m_rdLambda * (intraModeBits[mi->mode] + (pi->palette_size_y ? pi->palette_bits : 0));
                CostType cost = (rd.eob == 0)
                    ? rd.sse + m_rdLambda * (rd.modeBits + skipBits[1])
                    : rd.sse + m_rdLambda * (rd.modeBits + skipBits[0] + rd.coefBits);

                const int32_t sbw = num4x4w << 2;

                // update in case of best decision
                if (bestCost > cost)
                {
                    bestRdCost = rd;
                    bestCost = cost;
                    std::swap(qcoef, bestQcoef);
                    bestCtx = m_contexts;
                    CopyNxN(recSb, pitchRecLuma, bestRec, sbw);
                    bestRoundFAdjT[0] = m_roundFAdjY[0], bestRoundFAdjT[1] = m_roundFAdjY[1];

                    use_intrabc = 1;
                    // chrome & skip
                    int32_t uvEob = TransformInterChroma(i, varTxInfo);
                    int32_t bestEobY = rd.eob;
                    mi->skip = (bestEobY == 0 && uvEob == 0);
                }
            }
            m_roundFAdjY[0] = bestRoundFAdjT[0], m_roundFAdjY[1] = bestRoundFAdjT[1];
        }

        m_contexts = bestCtx;
        CopyNxM(bestRec, num4x4w << 2, recSb, pitchRecLuma, num4x4w << 2, num4x4h << 2);
        CopyCoeffs(bestQcoef, m_coeffWorkY + 16 * i, num4x4 << 4);

        if (use_intrabc) {
            //mi->skip is set by IBC
            mi->angle_delta_y = 0;
            mi->mode = DC_PRED;
            pi->palette_size_y = 0;
            mi->interp0 = mi->interp1 = BILINEAR;
            //mi->mv and mvd are set by IBC
            mi->memCtx.skip = ctxSkip;
            mi->memCtx.txSize = ctxTxSize;

            PropagateSubPart(mi, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);
            CopyVarTxInfo(varTxInfo, m_currFrame->m_fenc->m_txkTypes4x4 + m_ctbAddr * 256 + (miRow & 7) * 32 + (miCol & 7) * 2, num4x4w);
            CopyTxSizeInfo(varTxInfo, mi, m_par->miPitch, num4x4w);
            PropagatePaletteInfo(m_currFrame->m_fenc->m_Palette8x8, miRow, miCol, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);
        }
        else {
            mi->interp0 = mi->interp1 = 0;
            mi->txSize = bestTxSize;
            mi->minTxSize = bestTxSize;
            mi->txType = bestTxType;
            mi->skip = (bestRdCost.eob == 0);
            mi->angle_delta_y = bestAngleDelta + max_angle_delta;
            mi->memCtx.skip = ctxSkip;
            mi->memCtx.txSize = ctxTxSize;
#if USE_HWPAK_RESTRICT
            int32_t step = 1 << bestTxSize;
            for (int32_t y = 0; y < num4x4h; y += step) {
                for (int32_t x = 0; x < num4x4w; x += step) {
                    int32_t blkRow = ((miRow << 1) + y);
                    int32_t blkCol = ((miCol << 1) + x);
                    int32_t index = (blkRow & 15) * 16 + (blkCol & 15);
                    m_currFrame->m_fenc->m_txkTypes4x4[m_ctbAddr * 256 + index] = bestTxType;
                }
            }
#endif
            if (pi->palette_size_y) {
                int32_t depth = 4 - log2w;
                const uint8_t *map_src = &m_paletteMapY[depth][sby*MAX_CU_SIZE + sbx];
                uint8_t *map_dst = &m_currFrame->m_fenc->m_ColorMapY[(miRow << 3)*m_currFrame->m_fenc->m_ColorMapYPitch + (miCol << 3)];
                CopyNxM(map_src, MAX_CU_SIZE, map_dst, m_currFrame->m_fenc->m_ColorMapYPitch, num4x4w << 2, num4x4h << 2);
            }
            PropagateSubPart(mi, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);
            (void)CheckIntraChromaNonRdAv1_10bit(i, 4 - log2w, PARTITION_NONE, bestRec, num4x4w << 2);
            PropagatePaletteInfo(m_currFrame->m_fenc->m_Palette8x8, miRow, miCol, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);
        }

        i += num4x4;
    }

    storea_si128(m_contexts.aboveAndLeftPartition, loada_si128(savedPartitionCtx));
}

template void AV1CU<uint8_t>::MakeRecon10bitIAV1();


static int16_t clampMvRowAV1(int16_t mvy, int32_t border, BlockSize sbType, int32_t miRow, int32_t miRows) {
    int32_t bh = block_size_high_8x8[sbType];
    int32_t mbToTopEdge = -((miRow * MI_SIZE) * 8);
    int32_t mbToBottomEdge = ((miRows/* - bh*/ - miRow) * MI_SIZE) * 8;
    return (int16_t)Saturate(mbToTopEdge - border - bh * MI_SIZE * 8, mbToBottomEdge + border, mvy);
}

static int16_t clampMvColAV1(int16_t mvx, int32_t border, BlockSize sbType, int32_t miCol, int32_t miCols) {
    int32_t bw = block_size_wide_8x8[sbType];
    int32_t mbToLeftEdge = -((miCol * MI_SIZE) * 8);
    int32_t mbToRightEdge = ((miCols/* - bw*/ - miCol) * MI_SIZE) * 8;
    return (int16_t)Saturate(mbToLeftEdge - border - bw * MI_SIZE * 8, mbToRightEdge + border, mvx);
}

static inline int32_t HasSubpel(const RefIdx refIdx[2], const AV1MV mv[2])
{
    int32_t hasSubpel0 = (mv[0].mvx | mv[0].mvy) & 7;
    int32_t hasSubpel1 = (mv[1].mvx | mv[1].mvy) & 7;
    if (refIdx[1] != NONE_FRAME)
        hasSubpel0 |= hasSubpel1;
    return hasSubpel0;
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

#define ALIGN_DECL(X) __declspec(align(X))
template <typename PixType> void AV1CU<PixType>::MakeRecon10bitAV1()
{
    assert((m_par->maxTxDepthIntraRefine > 1) == (m_par->maxTxDepthInterRefine > 1)); // both should be refined or not refined
    ModeInfo mi64 = *m_data;

    alignas(16) uint8_t savedPartitionCtx[16];
    storea_si128(savedPartitionCtx, loada_si128(m_contexts.aboveAndLeftPartition));


    uint16_t *predSaved10 = vp9scratchpad.predY10[1];
    uint16_t *predUvSaved10 = vp9scratchpad.predUv10[1];
    if (m_par->CmInterpFlag) {
        if (JoinRefined32x32MI(mi64, true)) {
            CopyNxN(m_yRec10, m_pitchRecLuma10, predSaved10, 64, 64);
            CopyNxM(m_uvRec10, m_pitchRecChroma10, predUvSaved10, PRED_PITCH, 64, 32);
        }
    }

    {
        m_contexts = m_contextsInitSb;

        const uint16_t *refFramesY[3] = {};
        const uint16_t *refFramesUv[3] = {};
        if (!m_par->CmInterpFlag) {
            refFramesY[LAST_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_recon10->y);
            refFramesY[GOLDEN_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_recon10->y);
            refFramesY[ALTREF_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_recon10->y);
            refFramesUv[LAST_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_recon10->uv);
            refFramesUv[GOLDEN_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_recon10->uv);
            refFramesUv[ALTREF_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_recon10->uv);
        }
        const uint16_t *refFramesY_Upscale[3] = {};
        const uint16_t *refFramesUv_Upscale[3] = {};
        if (m_par->superResFlag) {
            refFramesY_Upscale[LAST_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_recon10Upscale->y);
            refFramesY_Upscale[GOLDEN_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_recon10Upscale->y);
            refFramesY_Upscale[ALTREF_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_recon10Upscale->y);
            refFramesUv_Upscale[LAST_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_recon10Upscale->uv);
            refFramesUv_Upscale[GOLDEN_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_recon10Upscale->uv);
            refFramesUv_Upscale[ALTREF_FRAME] = reinterpret_cast<uint16_t*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_recon10Upscale->uv);
        }

        for (int32_t i = 0; i < MAX_NUM_PARTITIONS;) {
            const int32_t rasterIdx = av1_scan_z2r4[i];
            const int32_t x4 = (rasterIdx & 15);
            const int32_t y4 = (rasterIdx >> 4);
            const int32_t sbx = x4 << 2;
            const int32_t sby = y4 << 2;
            const int32_t mix = m_ctbPelX + sbx;
            const int32_t miy = m_ctbPelY + sby;
            ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
            const int32_t num4x4 = num_4x4_blocks_lookup[mi->sbType];
            if (mi->mode == OUT_OF_PIC) {
                i += num4x4;
                continue;
            }

            const BlockSize bsz = mi->sbType;
            const int32_t log2w = block_size_wide_4x4_log2[bsz];
            const int32_t log2h = block_size_high_4x4_log2[bsz];
            const int32_t num4x4w = 1 << log2w;
            const int32_t num4x4h = 1 << log2h;
            const TxSize maxTxSize = std::min((uint8_t)TX_32X32, max_txsize_lookup[bsz]);
            const int32_t sbw = num4x4w << 2;
            const int32_t sbh = num4x4h << 2;
            const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
            const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);
            const int32_t haveTop = (miRow > m_tileBorders.rowStart);
            const int32_t haveLeft = (miCol > m_tileBorders.colStart);
            const int32_t miColEnd = m_tileBorders.colEnd;
            //const QuantParam &qparamY = m_aqparamY;
            QuantParam qparamY10 = m_par->qparamY10[m_currFrame->m_sliceQpY];
            float roundFAdjYInit[2] = { m_roundFAdjY[0], m_roundFAdjY[1] };
            const BitCounts &bc = *m_currFrame->bitCount;
            const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
            const ModeInfo *left = GetLeft(mi, miCol, m_tileBorders.colStart);
            const int32_t ctxSkip = GetCtxSkip(above, left);
            const int32_t ctxIsInter = GetCtxIsInter(above, left);
            const uint8_t aboveTxfm = above ? tx_size_wide[above->txSize] : 0;
            const uint8_t leftTxfm = left ? tx_size_high[left->txSize] : 0;
            const int32_t ctxTxSize = GetCtxTxSizeAV1(above, left, aboveTxfm, leftTxfm, maxTxSize);
            const uint16_t *skipBits = bc.skip[ctxSkip];
            const uint16_t *txSizeBits = bc.txSize[maxTxSize][ctxTxSize];
            //const uint16_t *isInterBits = bc.isInter[ctxIsInter];
            const uint32_t ctxInterp = GetCtxInterpBothAV1(above, left, mi->refIdx);
            const uint16_t *interpFilterBits0 = bc.interpFilterAV1[ctxInterp & 15];
            const uint16_t *interpFilterBits1 = bc.interpFilterAV1[ctxInterp >> 8];
            //const uint16_t *intraModeBits = bc.intraModeAV1[maxTxSize];
            const Contexts origCtx = m_contexts; // contexts at the start of current block
            Contexts bestCtx;
            uint8_t *anz = m_contexts.aboveNonzero[0] + x4;
            uint8_t *lnz = m_contexts.leftNonzero[0] + y4;
            uint8_t *aboveTxfmCtx = m_contexts.aboveTxfm + x4;
            uint8_t *leftTxfmCtx = m_contexts.leftTxfm + y4;
            uint16_t *recSb = m_yRec10 + sbx + sby * m_pitchRecLuma10;
            uint16_t *srcSb = m_ySrc10 + sbx + sby * SRC_PITCH;
            int16_t *diff = vp9scratchpad.diffY;
#if ENABLE_HIBIT_COEFS
            int32_t *coef = vp9scratchpad.coefY_hbd;
#else
            int16_t *coef = vp9scratchpad.coefY;
#endif
            int16_t *qcoef = vp9scratchpad.qcoefY[0];
            int16_t *bestQcoef = vp9scratchpad.qcoefY[1];
#if ENABLE_HIBIT_COEFS
            int32_t *coefBest = vp9scratchpad.dqcoefY_hbd;
            int32_t *coefWork = vp9scratchpad.varTxCoefs[0][0].coef_hbd;
            int32_t *coefWorkBest = vp9scratchpad.varTxCoefs[1][0].coef_hbd;
#else
            int16_t *coefBest = vp9scratchpad.dqcoefY;
            int16_t *coefWork = vp9scratchpad.varTxCoefs[0][0].coef;
            int16_t *coefWorkBest = vp9scratchpad.varTxCoefs[1][0].coef;
#endif
            int16_t *eob4x4 = (int16_t *)vp9scratchpad.varTxCoefs[2][0].coef;
            int16_t *eob4x4Best = (int16_t *)vp9scratchpad.varTxCoefs[2][1].coef;
            //uint64_t bestIntraCost = ULLONG_MAX;
            PredMode bestIntraMode = DC_PRED;
            //int32_t satdPairs[4];
            mi->angle_delta_y = MAX_ANGLE_DELTA; //ZERO_DELTA

            int32_t isNonRef = !m_currFrame->m_isRef;
            // check if mode should be switched to NEWMV (side-effect of swithing to INTRA)
            (m_currFrame->compoundReferenceAllowed)
                ? GetMvRefsAV1TU7B(bsz, miRow, miCol, &m_mvRefs, m_currFrame->refFrameSignBiasVp9[LAST_FRAME] != m_currFrame->refFrameSignBiasVp9[ALTREF_FRAME])
                : GetMvRefsAV1TU7P(bsz, miRow, miCol, &m_mvRefs);

            PaletteInfo *pi = &m_currFrame->m_fenc->m_Palette8x8[miCol + miRow * m_par->miPitch];
            //uint8_t palette[MAX_PALETTE];
            //uint16_t palette_size = 0;
            int32_t depth = 4 - log2w;
            //uint8_t *color_map = &m_paletteMapY[depth][sby*MAX_CU_SIZE + sbx];
            //uint32_t map_bits_est = 0;
            //uint32_t map_bits = 0;
            //uint32_t palette_bits = 0;
            pi->palette_size_y = 0; // Decide here for intra
            pi->color_map = NULL;
            pi->palette_size_uv = 0;
            AV1PP::IntraPredPels<PixType> predPels;
            uint16_t *pred = vp9scratchpad.predY10[0];
            const int32_t refYColocOffset = m_ctbPelX + sbx + (m_ctbPelY + sby) * m_pitchRecLuma10;
            const uint16_t *ref0Y = refFramesY[mi->refIdx[0]] + refYColocOffset;
            AV1MV clampedMV[2] = { mi->mv[0], mi->mv[1] };
            clampedMV[0].mvx = clampMvColAV1(clampedMV[0].mvx, 64 + 4, bsz, miCol, m_par->miCols);
            clampedMV[0].mvy = clampMvRowAV1(clampedMV[0].mvy, 64 + 4, bsz, miRow, m_par->miRows);
            clampedMV[1].mvx = clampMvColAV1(clampedMV[1].mvx, 64 + 4, bsz, miCol, m_par->miCols);
            clampedMV[1].mvy = clampMvRowAV1(clampedMV[1].mvy, 64 + 4, bsz, miRow, m_par->miRows);

            if (mi->mode >= AV1_INTRA_MODES) {
                m_mvRefs.memCtx.skip = ctxSkip;
                m_mvRefs.memCtx.isInter = ctxIsInter;
                m_mvRefs.memCtx.txSize = ctxTxSize;
                int32_t bestEobY = !mi->skip;
                TxSize bestTxSize = maxTxSize;
                TxType bestTxType = DCT_DCT;
                int32_t bestInterp0 = mi->interp0;
                int32_t bestInterp1 = mi->interp1;
                TxType bestChromaTxType = DCT_DCT;
                const int32_t refUvColocOffset = m_ctbPelX + sbx + ((m_ctbPelY + sby) >> 1) * m_pitchRecChroma10;
                const uint16_t *ref0Uv = refFramesUv[mi->refIdx[0]] + refUvColocOffset;
                const uint16_t *ref1Y = NULL;
                const uint16_t *ref1Uv = NULL;
                if (mi->refIdx[1] != NONE_FRAME) {
                    ref1Y = refFramesY[mi->refIdx[1]] + refYColocOffset;
                    ref1Uv = refFramesUv[mi->refIdx[1]] + refUvColocOffset;
                }

                const int hasSubpel = HasSubpel(mi->refIdx, clampedMV);
                alignas(32) uint16_t varTxInfo[16 * 16] = {};// {[0-3] bits - txType, [4-5] - txSize, [6] - eobFlag or [6-15] - eob/num non zero coefs}
                if (m_par->maxTxDepthInterRefine > 1) {
                    if (mi->skip) {
                        small_memset0(anz, num4x4w);
                        small_memset0(lnz, num4x4h);
                        if (m_par->switchInterpFilterRefine) {
                            const int32_t sseShiftY = m_par->bitDepthLumaShift << 1;
                            uint16_t *bestPred = vp9scratchpad.predY10[0];

                            bestInterp1 = bestInterp0 = DEF_FILTER;
                            {
                                const int32_t bestInterSse = AV1PP::sse_p64_p64(srcSb, bestPred, log2w, log2h);
                                CostType bestCost = bestInterSse + m_rdLambda * (interpFilterBits0[EIGHTTAP] + interpFilterBits1[EIGHTTAP]);
                                int32_t freeIdx = 1;

                                // first pass
                                //for (int32_t interp0 = EIGHTTAP + 1; interp0 < SWITCHABLE_FILTERS; interp0++)
                                int32_t interp0 = mi->interp0;
                                {
                                    int32_t interp1 = mi->interp1;

                                    int32_t interp = interp0 + (interp1 << 4);
                                    pred = vp9scratchpad.predY10[freeIdx];
                                    if (mi->refIdx[1] == NONE_FRAME) {
                                        InterpolateAv1SingleRefLuma(ref0Y, m_pitchRecLuma10, pred, clampedMV[0], sbh, log2w, interp);
                                    }
                                    else {
                                        InterpolateAv1FirstRefLuma(ref0Y, m_pitchRecLuma10, vp9scratchpad.compPredIm, clampedMV[0], sbh, log2w, interp);
                                        InterpolateAv1SecondRefLuma(ref1Y, m_pitchRecLuma10, vp9scratchpad.compPredIm, pred, clampedMV[1], sbh, log2w, interp);

                                    }
                                    int32_t sse = AV1PP::sse_p64_p64(srcSb, pred, log2w, log2h);

                                    CostType cost = sse + m_rdLambda * (interpFilterBits0[interp0] + interpFilterBits1[interp1]);
                                    //if (bestCost > cost)
                                    {
                                        bestCost = cost;
                                        bestPred = pred;
                                        bestInterp1 = interp1;
                                        bestInterp0 = interp0;
                                        freeIdx = 1 - freeIdx;
                                    }
                                }
                            }
                            CopyNxN(bestPred, 64, recSb, m_pitchRecLuma10, sbw);
                            mi->interp0 = bestInterp0;
                            mi->interp1 = bestInterp1;
                        }
                    } else {
                        uint16_t *rec = vp9scratchpad.recY10[0];
                        uint16_t *bestPredY = vp9scratchpad.predY10[0];


                        //uint64_t bestSatdCost = RD(bestInterSatd, interpFilterBits0[EIGHTTAP] + interpFilterBits1[EIGHTTAP], m_rdLambdaSqrtInt);
                        int32_t freeIdx = 1;
                        bestInterp1 = bestInterp0 = DEF_FILTER;

                        //if (hasSubpel)
                        {
                            // first pass
                            int32_t interp0 = mi->interp0;
                            //for (int32_t interp0 = EIGHTTAP + 1; interp0 < SWITCHABLE_FILTERS; interp0++)
                            {
                                int32_t interp1 = mi->interp1;
                                {
                                    int32_t interp = interp0 + (interp1 << 4);
                                    pred = vp9scratchpad.predY10[freeIdx];
                                    if (mi->refIdx[1] == NONE_FRAME) {
                                        InterpolateAv1SingleRefLuma(ref0Y, m_pitchRecLuma10, pred, clampedMV[0], sbh, log2w, interp);
                                    }
                                    else {
                                        InterpolateAv1FirstRefLuma(ref0Y, m_pitchRecLuma10, vp9scratchpad.compPredIm, clampedMV[0], sbh, log2w, interp);
                                        InterpolateAv1SecondRefLuma(ref1Y, m_pitchRecLuma10, vp9scratchpad.compPredIm, pred, clampedMV[1], sbh, log2w, interp);
                                    }
                                    //int32_t dist = AV1PP::satd(srcSb, pred, log2w, log2h);
                                    //uint64_t cost = RD(dist, interpFilterBits0[interp0] + interpFilterBits1[interp1], m_rdLambdaSqrtInt);

                                    //if (bestSatdCost > cost)
                                    {
                                        //bestSatdCost = cost;
                                        bestPredY = pred;
                                        bestInterp1 = interp1;
                                        bestInterp0 = interp0;
                                        freeIdx = 1 - freeIdx;
                                    }
                                }
                            }
                        }

                        bestPredY = pred;

                        RdCost rd = {};
                        AV1PP::diff_nxn(srcSb, bestPredY, diff, log2w);
                        CopyNxN(bestPredY, 64, rec, 64, sbw);
                        rd = TransformInterYSbVarTxByRdo(
                            bsz, anz, lnz, srcSb, rec, diff, m_coeffWorkY + i * 16, m_lumaQp, bc, m_rdLambda,
                            aboveTxfmCtx, leftTxfmCtx, varTxInfo, qparamY10, m_roundFAdjY, vp9scratchpad.varTxCoefs, MAX_VARTX_DEPTH, TX_TYPES - 1);

                        const uint32_t modeBits = (rd.eob ? skipBits[0] + rd.modeBits : skipBits[1]);
                        const uint32_t coefBits = (rd.eob ? ((rd.coefBits * 3) >> 2) : 0); // Adjusted for better skip decision
                        const CostType cost = rd.sse + m_rdLambda * (modeBits + coefBits);
                        const CostType costZeroBlock = rd.ssz + m_rdLambda * skipBits[1];
                        if (cost >= costZeroBlock) {
                            rd.eob = 0;
                            ZeroCoeffs(m_coeffWorkY + i * 16, num4x4 * 16);
                        }


                        if (rd.eob == 0) {
                            bestEobY = 0;
                            small_memset0(anz, num4x4w);
                            small_memset0(lnz, num4x4h);
                            CopyNxN(bestPredY, 64, recSb, m_pitchRecLuma10, sbw);
                        } else {
                            bestTxSize = maxTxSize;
                            bestEobY = rd.eob;
                            CopyNxN(rec, 64, recSb, m_pitchRecLuma10, sbw);
                        }
                    }
                }

                // propagate txkTypes
                if (bestEobY == 0)
                    SetVarTxInfo(varTxInfo, DCT_DCT + (maxTxSize << 4), num4x4w);

                // INTER CHROMA
                //const QuantParam(&qparamUv)[2]
                const QuantParam(&qparamUv)[2] = { m_par->qparamUv10[m_chromaQp], m_par->qparamUv10[m_chromaQp] };//m_aqparamUv;
                float *roundFAdjUv[2] = { &m_roundFAdjUv[0][0], &m_roundFAdjUv[1][0] };

                const int32_t subX = m_par->subsamplingX;
                const int32_t subY = m_par->subsamplingY;
                const BlockSize bszUv = ss_size_lookup[bsz][m_par->subsamplingX][m_par->subsamplingY];
                const TxSize txSizeUv = max_txsize_rect_lookup[bszUv];
                const int32_t uvsbx = sbx >> 1;
                const int32_t uvsby = sby >> 1;
                const int32_t uvsbw = sbw >> 1;
                const int32_t uvsbh = sbh >> 1;

                const uint16_t *srcUv = m_uvSrc10 + uvsbx * 2 + uvsby * SRC_PITCH;
                uint16_t *recUv = m_uvRec10 + uvsbx * 2 + uvsby * m_pitchRecChroma10;
                uint16_t *predUv = vp9scratchpad.predUv10[0];
                int16_t *diffU = vp9scratchpad.diffU;
                int16_t *diffV = vp9scratchpad.diffV;
                int16_t *coefU = vp9scratchpad.coefU;
                int16_t *coefV = vp9scratchpad.coefV;
                int16_t *qcoefU = m_coeffWorkU + i * 4;
                int16_t *qcoefV = m_coeffWorkV + i * 4;
                int16_t *coefWorkUv = (int16_t *)vp9scratchpad.varTxCoefs[0][0].coef;
                uint8_t *anzU = m_contexts.aboveNonzero[1] + (uvsbx >> 2);
                uint8_t *anzV = m_contexts.aboveNonzero[2] + (uvsbx >> 2);
                uint8_t *lnzU = m_contexts.leftNonzero[1] + (uvsby >> 2);
                uint8_t *lnzV = m_contexts.leftNonzero[2] + (uvsby >> 2);
                int32_t bestInterp = bestInterp0 + (bestInterp1 << 4);

                if (mi->refIdx[1] == NONE_FRAME) {
                    InterpolateAv1SingleRefChroma(ref0Uv, m_pitchRecChroma10, predUv, clampedMV[0], uvsbh, log2w - 1, bestInterp);
                } else {
                    InterpolateAv1FirstRefChroma(ref0Uv, m_pitchRecChroma10, vp9scratchpad.compPredIm, clampedMV[0], uvsbh, log2w - 1, bestInterp);
                    InterpolateAv1SecondRefChroma(ref1Uv, m_pitchRecChroma10, vp9scratchpad.compPredIm, predUv, clampedMV[1], uvsbh, log2w - 1, bestInterp);
                }

                AV1PP::diff_nv12(srcUv, predUv, diffU, diffV, uvsbh, log2w - 1);
                CopyNxM(predUv, PRED_PITCH, recUv, m_pitchRecChroma10, uvsbw << 1, uvsbh);

                int32_t uvEob = TransformInterUvSbTxType(bszUv, txSizeUv, bestChromaTxType, anzU, anzV, lnzU, lnzV, srcUv,
                    recUv, m_pitchRecChroma10, diffU, diffV, coefU, coefV, qcoefU,
                    qcoefV, coefWorkUv, *m_par, bc, m_chromaQp, varTxInfo, qparamUv, &roundFAdjUv[0],
                    m_rdLambda, (m_currFrame->m_picCodeType == MFX_FRAMETYPE_P && m_currFrame->m_pyramidLayer == 0 && bestEobY != 0) ? 0 : 1);

                if (bestEobY == 0 && uvEob == 0)
                    bestTxSize = maxTxSize;

                if (mi->skip && uvEob != 0)
                    ZeroCoeffs(m_coeffWorkY + i * 16, num4x4 * 16);

                mi->skip = (bestEobY == 0 && uvEob == 0);
                mi->txSize = bestTxSize;
                mi->minTxSize = bestTxSize;
                mi->txType = bestTxType;
                mi->interp0 = bestInterp0;
                mi->interp1 = bestInterp1;
#if 0
                mi->memCtx.singleRefP1 = m_mvRefs.memCtx.singleRefP1;
                mi->memCtx.singleRefP2 = m_mvRefs.memCtx.singleRefP2;
                mi->memCtx.compMode = m_mvRefs.memCtx.compMode;
                mi->memCtx.compRef = m_mvRefs.memCtx.compRef;
#endif
                mi->memCtx.skip = ctxSkip;
                mi->memCtx.txSize = ctxTxSize;
                mi->memCtx.isInter = ctxIsInter;
#if 0
                mi->memCtx.AV1.numDrlBits = numDrlBits;
                mi->memCtxAV1_.drl0 = ctxDrl0;
                mi->memCtxAV1_.drl1 = ctxDrl1;
                mi->memCtxAV1_.nmv0 = GetCtxNmv(m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb], 0, mi->refMvIdx + (mi->mode == NEW_NEARMV ? 1 : 0));
                mi->memCtxAV1_.nmv1 = GetCtxNmv(m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb], 1, mi->refMvIdx + (mi->mode == NEAR_NEWMV ? 1 : 0));
#endif
                mi->memCtx.AV1.interMode = m_mvRefs.mvRefsAV1.interModeCtx[mi->refIdxComb]/* & UP_TO_ALL_ZERO_FLAG_MASK*/;

                PropagateSubPart(mi, m_par->miPitch, sbw >> 3, sbh >> 3);
                CopyVarTxInfo(varTxInfo, m_currFrame->m_fenc->m_txkTypes4x4 + m_ctbAddr * 256 + (miRow & 7) * 32 + (miCol & 7) * 2, num4x4w);
                CopyTxSizeInfo(varTxInfo, mi, m_par->miPitch, num4x4w);
                PropagatePaletteInfo(m_currFrame->m_fenc->m_Palette8x8, miRow, miCol, m_par->miPitch, sbw >> 3, sbh >> 3);

            } else { // switch to INTRA

                const int32_t allow_screen_content_tool_palette = m_currFrame->m_allowPalette && m_currFrame->m_isRef;
                const int32_t max_angle_delta = av1_is_directional_mode(bestIntraMode) ? MAX_ANGLE_DELTA : 0;
                int32_t bestAngleDelta = mi->angle_delta_y - max_angle_delta;
                CostType bestCost = COST_MAX;
                CostType lastCost = COST_MAX;
                TxSize bestTxSize = maxTxSize;
                TxType bestTxType = DCT_DCT;
                int32_t bestEob = 1025;
                int32_t filtType = get_filt_type(above, left, PLANE_TYPE_Y);

                if (m_par->maxTxDepthIntraRefine > 1) {
                    uint16_t *bestRec = vp9scratchpad.recY10[0];
                    bestCtx = m_contexts;
                    CopyNxN(recSb, m_pitchRecLuma10, bestRec, sbw);

                    // best delta decision on first iteration only
                    int delta_start = -max_angle_delta;
                    int delta_stop = max_angle_delta;
                    int delta_step = 1;
                    if (isNonRef && max_angle_delta) {
                        delta_start = -2;
                        delta_stop = 2;
                        delta_step = 2;
                    }

                    const int32_t minTxSize = std::max((int32_t)TX_4X4, max_txsize_lookup[bsz] - MAX_TX_DEPTH);
                    TxType txTypeList[] = { DCT_DCT,  ADST_DCT, DCT_ADST, ADST_ADST, IDTX };
                    TxType txTypeFirst = DCT_DCT;
                    for (int32_t txSize = maxTxSize; txSize >= minTxSize; txSize--)
                    {
                        for (auto txType : txTypeList)
                        {
                            if (txSize == TX_32X32 && txType != DCT_DCT)
                                continue;
                            if (pi->palette_size_y && txType != DCT_DCT && txType != IDTX)
                                continue;
                            if (!pi->palette_size_y && txType == IDTX)
                                continue;

                            if (txSize != maxTxSize && txType != txTypeFirst)
                                continue;

                            //for (int delta = delta_start; delta <= delta_stop; delta += delta_step)
                            int delta = mi->angle_delta_y - max_angle_delta;
                            {

                                float roundFAdjT[2] = { roundFAdjYInit[0], roundFAdjYInit[1] };

                                m_contexts = origCtx;
                                RdCost rd = TransformIntraYSbAv1(bsz, bestIntraMode, haveTop, haveLeft, (TxSize)txSize, txType,
                                    anz, lnz, srcSb, recSb, m_pitchRecLuma10, diff, coef, qcoef, coefWork,
                                    m_lumaQp, bc, m_rdLambda, miRow, miCol, m_tileBorders.colEnd,
                                    delta, filtType, *m_par, qparamY10, NULL, // &roundFAdjT[0],
                                    pi, eob4x4);

                                const TxType txTypeNom = GetTxTypeAV1(PLANE_TYPE_Y, (TxSize)txSize, bestIntraMode, txType);
                                //const uint16_t *txTypeBits = bc.intraExtTx[txSize][txTypeNom];
                                const uint16_t *txTypeBits = (txType == IDTX) ? bc.intra_tx_type_costs[2][txSize][mi->mode] : bc.intraExtTx[txSize][txTypeNom];

                                rd.modeBits = txSizeBits[txSize];
                                if (av1_is_directional_mode(bestIntraMode)) {
                                    rd.modeBits += write_uniform_cost(2 * max_angle_delta + 1, delta + max_angle_delta);
                                }
                                CostType cost = (rd.eob == 0) // if luma has no coeffs, assume skip
                                    ? rd.sse + m_rdLambda * (rd.modeBits + skipBits[1])
                                    : rd.sse + m_rdLambda * (rd.modeBits + skipBits[0] + rd.coefBits + txTypeBits[txType]);

                                fprintf_trace_cost(stderr, "intra luma refine: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d tx=%d "
                                    "sse=%6d modeBits=%4u eob=%d coefBits=%-6u cost=%.0f\n", m_currFrame->m_frameOrder, m_ctbAddr, i, bsz, mi->mode, txSize,
                                    rd.sse, rd.modeBits, rd.eob, rd.coefBits, cost);

                                lastCost = cost;
                                if (bestCost > cost) {
                                    bestCost = cost;
                                    bestEob = rd.eob;
                                    bestTxSize = (TxSize)txSize;
                                    bestTxType = txType;
                                    bestAngleDelta = delta;
                                    std::swap(qcoef, bestQcoef);
                                    //if (!(bestTxSize == minTxSize && bestTxType == ADST_ADST && delta == max_angle_delta))
                                    { // best is not last checked
                                        bestCtx = m_contexts;
                                        CopyNxN(recSb, m_pitchRecLuma10, bestRec, sbw);
                                    }
                                    std::swap(coefWork, coefWorkBest);
                                    std::swap(coef, coefBest);
                                    std::swap(eob4x4, eob4x4Best);
                                    //m_roundFAdjY[0] = roundFAdjT[0];
                                    //m_roundFAdjY[1] = roundFAdjT[1];
                                }
                            }
                            delta_start = delta_stop = bestAngleDelta;
                            if (pi->palette_size_y && pi->palette_size_y == pi->true_colors_y) break;
                        }
                        txTypeFirst = bestTxType;
                        if (bestEob == 0) break;
                    }
#ifdef ADAPTIVE_DEADZONE
                    if (bestEob) {
                        int32_t step = 1 << bestTxSize;
                        for (int32_t y = 0; y < num4x4h; y += step) {
                            for (int32_t x = 0; x < num4x4w; x += step) {
                                int32_t blockIdx = h265_scan_r2z4[y * 16 + x];
                                int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
#if ENABLE_HIBIT_COEFS
                                int32_t *coeff = coefBest + offset;
                                int32_t *coefOrigin = coefWorkBest + offset;
#else
                                int16_t *coeff = coefBest + offset;
                                int16_t *coefOrigin = coefWorkBest + offset;
#endif
                                if (eob4x4Best[y * 16 + x])
                                    adaptDz(coefOrigin, coeff, qparamY10, bestTxSize, m_roundFAdjY, eob4x4Best[y * 16 + x]);
                            }
                        }
                    }
#endif
                    // propagate TxkTypes
                    SetVarTxInfo(m_currFrame->m_fenc->m_txkTypes4x4 + m_ctbAddr * 256 + (miRow & 7) * 32 + (miCol & 7) * 2, bestTxType, num4x4w);

                    m_contexts = bestCtx;
                    CopyNxN(bestRec, sbw, recSb, m_pitchRecLuma10, sbw);
                }

                mi->refIdx[0] = INTRA_FRAME;
                mi->refIdx[1] = NONE_FRAME;
                mi->refIdxComb = INTRA_FRAME;
                mi->skip = (bestEob == 0);
                mi->txSize = bestTxSize;
                mi->minTxSize = bestTxSize;
                mi->txType = bestTxType;
                mi->mode = bestIntraMode;
                mi->interp0 = mi->interp1 = 0;
                //m_currFrame->m_Palette8x8[miRow*m_par->miPitch + miCol].palette_size_y = 0;
                //m_currFrame->m_Palette8x8[miRow*m_par->miPitch + miCol].palette_size_uv = 0;
                mi->angle_delta_y = bestAngleDelta + max_angle_delta;
                mi->memCtx.skip = ctxSkip;
                mi->memCtx.txSize = ctxTxSize;
                mi->memCtx.isInter = ctxIsInter;
                PropagateSubPart(mi, m_par->miPitch, sbw >> 3, sbh >> 3);
                CopyCoeffs(bestQcoef, m_coeffWorkY + 16 * i, num4x4 << 4);
                if (pi->palette_size_y) {
                    const uint8_t *map_src = &m_paletteMapY[depth][sby*MAX_CU_SIZE + sbx];
                    uint8_t *map_dst = &m_currFrame->m_fenc->m_ColorMapY[(miRow << 3)*m_currFrame->m_fenc->m_ColorMapYPitch + (miCol << 3)];
                    //AV1PP::map_intra_palette(srcSb, map_dst,m_currFrame->m_fenc->m_ColorMapYPitch, sbw, &pi->palette_y[0], pi->palette_size_y);
                    CopyNxM(map_src, MAX_CU_SIZE, map_dst, m_currFrame->m_fenc->m_ColorMapYPitch, sbw, sbh);
                }

                (void)CheckIntraChromaNonRdAv1_10bit(i, 4 - log2w, PARTITION_NONE, recSb, m_pitchRecLuma10);
                PropagatePaletteInfo(m_currFrame->m_fenc->m_Palette8x8, miRow, miCol, m_par->miPitch, sbw >> 3, sbh >> 3);
            }

            i += num4x4;
        }
        //joined++;
    } //while (JoinRefined32x32MI(mi64, false));

    storea_si128(m_contexts.aboveAndLeftPartition, loada_si128(savedPartitionCtx));
}

template void AV1CU<uint8_t>::MakeRecon10bitAV1();