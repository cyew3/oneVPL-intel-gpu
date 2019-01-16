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
#include "mfx_av1_bit_count.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_dispatcher_wrappers.h"
#include "mfx_av1_copy.h"
#include "mfx_av1_get_context.h"
//#include "mfx_vp9_scan.h"
//#include "mfx_h265_tables.h"
//#include "mfx_h265_enc.h"
//#include "mfx_h265_encode.h"
//#include "mfx_h265_quant.h"
//#include "mfx_av1_scan.h"
//#include "mfx_vp9_get_intra_pred_pels.h"
//#include "mfx_av1_get_intra_pred_pels.h"
//#include "mfx_vp9_prob_opt.h"
//#include "mfx_vp9_trace.h"

#define ENABLE_DEBUG_PRINTF 0
#if ENABLE_DEBUG_PRINTF
  #define debug_printf printf
#else
  #define debug_printf(...)
#endif


namespace H265Enc {
    namespace {
        //int32_t MvCost(int16_t mvd) { // TODO(npshosta): remove all usage of this one
        //    uint8_t ZF;
        //    int32_t index = BSR(mvd*mvd, ZF);
        //    return ((-int32_t(ZF)) & (index + 1)) + 1;  // mvd ? 2*index+3 : 1;
        //}

        int32_t MvCost(uint16_t absMvd, int32_t useMvHp) {
            // model logarithmic function = numbits(mvcomp) = (log2((mvcomp>>1) + 1) << 10) + 800 + (useMvHp ? 512 : 0)
            return (BSR((absMvd >> 1) + 1) << 10) + 800 + (useMvHp << 9);
        }

        int32_t MvCost(int16_t mvdx, int16_t mvdy, int32_t useMvHp) {
            return MvCost((uint16_t)abs(mvdx), useMvHp) + MvCost((uint16_t)abs(mvdy), useMvHp);
        }

        int32_t MvCost(H265MV mv, H265MV mvp, int32_t useMvHp) {
            return MvCost((uint16_t)abs(mv.mvx - mvp.mvx), useMvHp) + MvCost((uint16_t)abs(mv.mvy - mvp.mvy), useMvHp);
        }

        int32_t MvCost(H265MVPair mv, H265MV mvp0, H265MV mvp1, int32_t useMvHp0, int32_t useMvHp1) {
            return MvCost((uint16_t)abs(mv[0].mvx - mvp0.mvx), useMvHp0) + MvCost((uint16_t)abs(mv[0].mvy - mvp0.mvy), useMvHp0)
                 + MvCost((uint16_t)abs(mv[1].mvx - mvp1.mvx), useMvHp1) + MvCost((uint16_t)abs(mv[1].mvy - mvp1.mvy), useMvHp1);
        }

        bool CheckSearchRange(int32_t y, int32_t h, const H265MV &mv, int32_t searchRangeY) {
            return y + (mv.mvy >> 2) + h + 4 + 1 <= searchRangeY;  // +4 from luma interp +1 from chroma interp
        }

        bool SameMotion(const H265MV mv0[2], const H265MV mv1[2], const int8_t refIdx0[2], const int8_t refIdx1[2]) {
            return *reinterpret_cast<const uint16_t*>(refIdx0) == *reinterpret_cast<const uint16_t*>(refIdx1) &&
                *reinterpret_cast<const uint64_t*>(mv0) == *reinterpret_cast<const uint64_t*>(mv1);
        }

        bool HasSameMotion(const H265MV mv[2], const H265MV (*otherMvs)[2], const int8_t refIdx[2],
                           const int8_t (*otherRefIdxs)[2], int32_t numMvRefs)
        {
            for (int32_t i = 0; i < numMvRefs; i++)
                if (SameMotion(mv, otherMvs[i], refIdx, otherRefIdxs[i]))
                    return true;
            return false;
        }

        int16_t ClampMvRow(int16_t mvy, int32_t border, BlockSize sbType, int32_t miRow, int32_t miRows) {
            int32_t bh = block_size_high_8x8[sbType];
            int32_t mbToTopEdge = -((miRow * MI_SIZE) * 8);
            int32_t mbToBottomEdge = ((miRows - bh - miRow) * MI_SIZE) * 8;
            return (int16_t)Saturate(mbToTopEdge - border, mbToBottomEdge + border, mvy);
        }

        int16_t ClampMvCol(int16_t mvx, int32_t border, BlockSize sbType, int32_t miCol, int32_t miCols) {
            int32_t bw = block_size_wide_8x8[sbType];
            int32_t mbToLeftEdge = -((miCol * MI_SIZE) * 8);
            int32_t mbToRightEdge = ((miCols - bw - miCol) * MI_SIZE) * 8;
            return (int16_t)Saturate(mbToLeftEdge - border, mbToRightEdge + border, mvx);
        }

        const int16_t h265_tabTx[128] = {
            0,    16384,   8192,   5461,   4096,   3277,   2731,   2341,   2048,   1820,   1638,   1489,   1365,   1260,   1170,   1092,
            1024,   964,    910,    862,    819,    780,    745,    712,    683,    655,    630,    607,    585,    565,    546,    529,
            512,    496,    482,    468,    455,    443,    431,    420,    410,    400,    390,    381,    372,    364,    356,    349,
            341,    334,    328,    321,    315,    309,    303,    298,    293,    287,    282,    278,    273,    269,    264,    260,
            256,    252,    248,    245,    241,    237,    234,    231,    228,    224,    221,    218,    216,    213,    210,    207,
            205,    202,    200,    197,    195,    193,    191,    188,    186,    184,    182,    180,    178,    176,    174,    172,
            171,    169,    167,    165,    164,    162,    161,    159,    158,    156,    155,    153,    152,    150,    149,    148,
            146,    145,    144,    142,    141,    140,    139,    138,    137,    135,    134,    133,    132,    131,    130,    129,
        };

        int32_t GetDistScaleFactor(int32_t currRefTb, int32_t colRefTb) {
            if (currRefTb == colRefTb) {
                return 4096;
            } else {
                int32_t tb = Saturate(-128, 127, currRefTb);
                int32_t td = Saturate(-128, 127, colRefTb);
                int32_t tx = (td > 0) ? h265_tabTx[td] : -h265_tabTx[-td];
                assert(tx == (16384 + abs(td/2))/td);
                int32_t distScaleFactor = Saturate(-4096, 4095, (tb * tx + 32) >> 6);
                return distScaleFactor;
            }
        }

        uint32_t EstimateMvComponent(const BitCounts &bitCount, int16_t diffmv, int32_t useHp, int32_t comp) {
            assert(diffmv != 0);
            assert(useHp == 0 || useHp == 1);
            uint32_t cost = 0;
            cost += bitCount.mvSign[comp][diffmv < 0];
            diffmv = (uint16_t)abs(diffmv)-1;
            if (diffmv < 16) {
                cost += bitCount.mvClass[comp][0];
                cost += bitCount.mvClass0[comp][useHp][diffmv];
                //int32_t mv_class0_bit = diffmv >> 3;
                //int32_t mv_class0_fr = (diffmv >> 1) & 3;
                //int32_t mv_class0_hp = diffmv & 1;
                //cost += bitCount.mvClass0Bit[comp][mv_class0_bit];
                //cost += bitCount.mvClass0Fr[comp][mv_class0_bit][mv_class0_fr];
                //if (useHp)
                //    cost += bitCount.mvClass0Hp[comp][mv_class0_hp];
            } else {
                int32_t mvClass = MIN(MV_CLASS_10, BSR(diffmv >> 3));
                cost += bitCount.mvClass[comp][mvClass];
                diffmv -= CLASS0_SIZE << (mvClass + 2);
                int32_t intdiff = diffmv >> 3;
                for (int32_t i = 0; i < mvClass; i++, intdiff >>= 1)
                    cost += bitCount.mvBits[comp][i][intdiff & 1];
                int32_t mv_fr = (diffmv >> 1) & 3;
                int32_t mv_hp = diffmv & 1;
                cost += bitCount.mvFr[comp][mv_fr];
                if (useHp)
                    cost += bitCount.mvHp[comp][mv_hp];
            }
            return cost;
        }


        uint32_t EstimateMv(const BitCounts &bitCount, H265MV mv, H265MV bestMv, int32_t useMvHp) {
            uint32_t cost = 0;
            int32_t mvJoint = MV_JOINT_ZERO;
            if (mv.mvx != bestMv.mvx) {
                mvJoint = MV_JOINT_HNZVZ;
                cost += EstimateMvComponent(bitCount, mv.mvx - bestMv.mvx, useMvHp, MV_COMP_HOR);
            }
            if (mv.mvy != bestMv.mvy) {
                mvJoint |= MV_JOINT_HZVNZ;
                cost += EstimateMvComponent(bitCount, mv.mvy - bestMv.mvy, useMvHp, MV_COMP_VER);
            }
            cost += bitCount.mvJoint[mvJoint];
            return cost;
        }


        void EstimateRefFramesAllVp9(const BitCounts &bitCount, const Frame &frame, const ModeInfo *above, const ModeInfo *left,
                                     uint32_t refFrameBitCount[5], MemCtx *memCtx)
        {
            const int32_t idxA = above ? above->refIdxComb + 2 : 0;
            const int32_t idxL = left ? left->refIdxComb + 2 : 0;
            const uint8_t (&ctxs)[4] = frame.m_refFramesContextsVp9[idxA][idxL];
            memCtx->singleRefP1 = ctxs[0];
            memCtx->singleRefP2 = ctxs[1];
            memCtx->compMode = ctxs[2];
            memCtx->compRef = ctxs[3];

            if (frame.referenceMode == SINGLE_REFERENCE) {
                refFrameBitCount[LAST_FRAME]   = bitCount.vp9SingleRef[ ctxs[0] ][0][0];
                refFrameBitCount[GOLDEN_FRAME] = bitCount.vp9SingleRef[ ctxs[0] ][0][1] + bitCount.vp9SingleRef[ ctxs[1] ][1][0];
                refFrameBitCount[ALTREF_FRAME] = bitCount.vp9SingleRef[ ctxs[0] ][0][1] + bitCount.vp9SingleRef[ ctxs[1] ][1][1];
                refFrameBitCount[COMP_VAR0] = 0xffffffff;
                refFrameBitCount[COMP_VAR1] = 0xffffffff;
            } else {
                refFrameBitCount[LAST_FRAME]   = bitCount.vp9SingleRef[ ctxs[0] ][0][0];
                refFrameBitCount[GOLDEN_FRAME] = bitCount.vp9SingleRef[ ctxs[0] ][0][1] + bitCount.vp9SingleRef[ ctxs[1] ][1][0];
                refFrameBitCount[ALTREF_FRAME] = bitCount.vp9SingleRef[ ctxs[0] ][0][1] + bitCount.vp9SingleRef[ ctxs[1] ][1][1];
                refFrameBitCount[LAST_FRAME]   += bitCount.compMode[ ctxs[2] ][0];
                refFrameBitCount[GOLDEN_FRAME] += bitCount.compMode[ ctxs[2] ][0];
                refFrameBitCount[ALTREF_FRAME] += bitCount.compMode[ ctxs[2] ][0];
                refFrameBitCount[COMP_VAR0] = bitCount.compMode[ ctxs[2] ][1] + bitCount.vp9CompRef[ ctxs[3] ][0];
                refFrameBitCount[COMP_VAR1] = bitCount.compMode[ ctxs[2] ][1] + bitCount.vp9CompRef[ ctxs[3] ][1];
            }
        }

        uint32_t EstimateRefFrameAv1(const BitCounts &bits, const Frame &frame, const ModeInfo *above, const ModeInfo *left,
                                   RefIdx refIdxComb, MemCtx *memCtx)
        {
            const int32_t idxA = above ? above->refIdxComb + 2 : 0;
            const int32_t idxL = left ? left->refIdxComb + 2 : 0;
            assert(idxA < 6);
            assert(idxL < 6);
            const RefFrameContextsAv1 &ctxs = frame.m_refFramesContextsAv1[idxA][idxL];
            memCtx->compMode = ctxs.refMode;

            if (frame.referenceMode == SINGLE_REFERENCE) {
                if (refIdxComb == LAST_FRAME) {
                    return bits.av1SingleRef[ctxs.singleRefP1][0][0]
                         + bits.av1SingleRef[ctxs.singleRefP3][2][0]
                         + bits.av1SingleRef[ctxs.singleRefP4][3][0];
                } else if (refIdxComb == GOLDEN_FRAME) {
                    return bits.av1SingleRef[ctxs.singleRefP1][0][0]
                         + bits.av1SingleRef[ctxs.singleRefP3][2][1]
                         + bits.av1SingleRef[ctxs.singleRefP5][4][1];
                } else {
                    assert(0);
                    return 0;
                }
            } else {
                if (refIdxComb == LAST_FRAME) {
                    return bits.compMode[ctxs.refMode][0]
                         + bits.av1SingleRef[ctxs.singleRefP1][0][0]
                         + bits.av1SingleRef[ctxs.singleRefP3][2][0]
                         + bits.av1SingleRef[ctxs.singleRefP4][3][0];
                } else if (refIdxComb == ALTREF_FRAME) {
                    return bits.compMode[ctxs.refMode][0]
                         + bits.av1SingleRef[ctxs.singleRefP1][0][1]
                         + bits.av1SingleRef[ctxs.singleRefP2][1][1];
                } else if (refIdxComb == COMP_VAR0) {
                    return bits.compMode[ctxs.refMode][1]
                         + bits.av1CompRefType[ctxs.compRefType][1]
                         + bits.av1CompRef[ctxs.compFwdRefP0][0][0]
                         + bits.av1CompRef[ctxs.compFwdRefP1][1][1]
                         + bits.av1CompBwdRef[ctxs.compBwdRefP0][0][1];
                } else {
                    assert(0);
                    return 0;
                }
            }
        }
        void EstimateRefFramesAllAv1(const BitCounts &bits, const Frame &frame, const ModeInfo *above, const ModeInfo *left,
                                     uint32_t refFrameBitCount[5], MemCtx *memCtx)
        {
            const int32_t idxA = above ? above->refIdxComb + 2 : 0;
            const int32_t idxL = left ? left->refIdxComb + 2 : 0;
            assert(idxA < 6);
            assert(idxL < 6);
            const RefFrameContextsAv1 &ctxs = frame.m_refFramesContextsAv1[idxA][idxL];
            memCtx->compMode = ctxs.refMode;

            if (!frame.compoundReferenceAllowed) {
                refFrameBitCount[LAST_FRAME] = bits.av1SingleRef[ctxs.singleRefP1][0][0]
                                             + bits.av1SingleRef[ctxs.singleRefP3][2][0]
                                             + bits.av1SingleRef[ctxs.singleRefP4][3][0];
                refFrameBitCount[GOLDEN_FRAME] = bits.av1SingleRef[ctxs.singleRefP1][0][0]
                                               + bits.av1SingleRef[ctxs.singleRefP3][2][1]
                                               + bits.av1SingleRef[ctxs.singleRefP5][4][1];
                refFrameBitCount[ALTREF_FRAME] = 0xffffffff;
                refFrameBitCount[COMP_VAR0] = 0xffffffff;
                refFrameBitCount[COMP_VAR1] = 0xffffffff;
            } else {
                refFrameBitCount[LAST_FRAME] = bits.compMode[ctxs.refMode][0]
                                             + bits.av1SingleRef[ctxs.singleRefP1][0][0]
                                             + bits.av1SingleRef[ctxs.singleRefP3][2][0]
                                             + bits.av1SingleRef[ctxs.singleRefP4][3][0];
                refFrameBitCount[GOLDEN_FRAME] = 0xffffffff;
                refFrameBitCount[ALTREF_FRAME] = bits.compMode[ctxs.refMode][0]
                                               + bits.av1SingleRef[ctxs.singleRefP1][0][1]
                                               + bits.av1SingleRef[ctxs.singleRefP2][1][1];
                refFrameBitCount[COMP_VAR0] = bits.compMode[ctxs.refMode][1]
                                            + bits.av1CompRefType[ctxs.compRefType][1]
                                            + bits.av1CompRef[ctxs.compFwdRefP0][0][0]
                                            + bits.av1CompRef[ctxs.compFwdRefP1][1][1]
                                            + bits.av1CompBwdRef[ctxs.compBwdRefP0][0][1];
                refFrameBitCount[COMP_VAR1] = 0xffffffff;
            }
        }
    }  // anonymous namespace

    const Contexts& Contexts::operator=(const Contexts &other) {
        storea_si128(aboveNonzero[0], loada_si128(other.aboveNonzero[0]));
        storea_si128(aboveNonzero[1], loada_si128(other.aboveNonzero[1]));
        storea_si128(aboveNonzero[2], loada_si128(other.aboveNonzero[2]));
        storea_si128(leftNonzero[0], loada_si128(other.leftNonzero[0]));
        storea_si128(leftNonzero[1], loada_si128(other.leftNonzero[1]));
        storea_si128(leftNonzero[2], loada_si128(other.leftNonzero[2]));
        storea_si128(aboveAndLeftPartition, loada_si128(other.aboveAndLeftPartition));
        return *this;
    }


    // propagate data[0] thru data[1..numParts-1] where numParts=2^n
    void PropagateSubPart(ModeInfo *mi, int32_t miPitch, int32_t miWidth, int32_t miHeight)
    {
        assert(sizeof(*mi) == 32);  // if assert fails uncomment and use nonoptimized-code
        assert((size_t(mi) & 15) == 0);  // expecting 16-byte aligned address
        uint8_t *ptr = reinterpret_cast<uint8_t*>(mi);
        const int32_t pitch = sizeof(ModeInfo) * miPitch;
        const __m128i half1 = loada_si128(ptr + 0);
        const __m128i half2 = loada_si128(ptr + 16);
        for (int32_t x = 1; x < miWidth; x++) {
            storea_si128(ptr + x * 32 + 0,  half1);
            storea_si128(ptr + x * 32 + 16, half2);
        }
        ptr += pitch;
        for (int32_t y = 1; y < miHeight; y++, ptr += pitch) {
            for (int32_t x = 0; x < miWidth; x++) {
                storea_si128(ptr + x * 32 + 0,  half1);
                storea_si128(ptr + x * 32 + 16, half2);
            }
        }
    }


    void InitSuperBlock(SuperBlock *sb, int32_t sbRow, int32_t sbCol, const H265VideoParam &par, Frame *frame, const CoeffsType *coefs, void *tokens) {
        sb->par = &par;
        sb->frame = frame;
        sb->sbRow = sbRow;
        sb->sbCol = sbCol;
        sb->pelX = sbCol << LOG2_MAX_CU_SIZE;
        sb->pelY = sbRow << LOG2_MAX_CU_SIZE;
        sb->sbIndex = sbRow * par.sb64Cols + sbCol;
        sb->tileRow = par.tileParam.mapSb2TileRow[sb->sbRow];
        sb->tileCol = par.tileParam.mapSb2TileCol[sb->sbCol];
        sb->tileIndex = (sb->tileRow * par.tileParam.cols) + sb->tileCol;
        sb->mi = frame->cu_data + (sbRow << 3) * par.miPitch + (sbCol << 3);
        int32_t numCoeffL = 1 << (LOG2_MAX_CU_SIZE << 1);
        int32_t numCoeffC = (numCoeffL >> par.chromaShift);
        sb->coeffs[0] = coefs + sb->sbIndex * (numCoeffL + numCoeffC + numCoeffC);
        sb->coeffs[1] = sb->coeffs[0] + numCoeffL;
        sb->coeffs[2] = sb->coeffs[1] + numCoeffC;
        if (par.codecType == CODEC_VP9)
            sb->tokensVP9 = (TokenVP9 *)tokens + sb->sbIndex * (numCoeffL + numCoeffC + numCoeffC + 64);
        else
            sb->tokensAV1 = (TokenAV1 *)tokens + sb->sbIndex * (numCoeffL + numCoeffC + numCoeffC + 64);
        sb->cdefPreset = -1;
    }


    template <typename PixType>
    void CalculateAndAccumulateSatds(const PixType *src, const PixType *pred, int32_t *satd[4], bool useHadamard) {
#ifdef USE_SAD_ONLY
        VP9PP::sad_store8x8(src, pred, satd[3], 4);
#else
        MetricSave(src, pred, 0, satd[3], useHadamard);
#endif

        __m128i satd_16x16[4];
        for (int32_t i = 0; i < 4; i++) {
            __m128i satd_8x8_00 = loada_si128(satd[3] + 16 * i + 0);
            __m128i satd_8x8_01 = loada_si128(satd[3] + 16 * i + 4);
            __m128i satd_8x8_10 = loada_si128(satd[3] + 16 * i + 8);
            __m128i satd_8x8_11 = loada_si128(satd[3] + 16 * i + 12);
            satd_16x16[i] = _mm_hadd_epi32(_mm_add_epi32(satd_8x8_00, satd_8x8_10), _mm_add_epi32(satd_8x8_01, satd_8x8_11));
            storea_si128(satd[2] + 4 * i, satd_16x16[i]);
        }

        __m128i satd_32x32 = _mm_hadd_epi32(_mm_add_epi32(satd_16x16[0], satd_16x16[1]), _mm_add_epi32(satd_16x16[2], satd_16x16[3]));
        storea_si128(satd[1], satd_32x32);

        __m128i satd_64x64 = _mm_add_epi32(satd_32x32, _mm_srli_si128(satd_32x32, 8));
        *satd[0] = _mm_cvtsi128_si32(satd_64x64) + _mm_extract_epi32(satd_64x64, 1);
    }


    template <typename PixType> void H265CU<PixType>::CalculateZeroMvPredAndSatd()
    {
        for (int32_t refIdx = LAST_FRAME; refIdx <= COMP_VAR1; refIdx++) {
            m_zeroMvpSatd[refIdx][0] = m_zeroMvpSatd64[refIdx];
            m_zeroMvpSatd[refIdx][1] = m_zeroMvpSatd32[refIdx];
            m_zeroMvpSatd[refIdx][2] = m_zeroMvpSatd16[refIdx];
            m_zeroMvpSatd[refIdx][3] = m_zeroMvpSatd8[refIdx];
        }

        const int32_t refColocOffset = m_ctbPelX + m_ctbPelY * m_pitchRecLuma;
        for (int32_t refIdx = LAST_FRAME; refIdx <= ALTREF_FRAME; refIdx++) {
            if (!m_currFrame->isUniq[refIdx])
                continue;

            const PixType *refColoc = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[refIdx]->m_recon->y) + refColocOffset;

            PixType *pred = vp9scratchpad.interpBufs[refIdx][0][ZEROMV_];
            (m_par->codecType == CODEC_VP9)
                ? VP9PP::interp_vp9(refColoc, m_pitchRecLuma, pred, 0, 0, MAX_CU_SIZE, LOG2_MAX_CU_SIZE - 2, 0, DEF_FILTER)
                : VP9PP::interp_av1(refColoc, m_pitchRecLuma, pred, 0, 0, MAX_CU_SIZE, LOG2_MAX_CU_SIZE - 2, DEF_FILTER_DUAL);
            m_interp[refIdx][0][ZEROMV_] = pred;
            m_interp[refIdx][1][ZEROMV_] = pred;
            m_interp[refIdx][2][ZEROMV_] = pred;
            m_interp[refIdx][3][ZEROMV_] = pred;
            CalculateAndAccumulateSatds(m_ySrc, pred, m_zeroMvpSatd[refIdx], m_par->hadamardMe >= 2);
        }

        if (m_currFrame->compoundReferenceAllowed) {
            const int32_t fixRef = m_currFrame->compFixedRef;

            for (int32_t refIdx = COMP_VAR0; refIdx <= COMP_VAR1; refIdx++) {
                const int32_t varRef = m_currFrame->compVarRef[refIdx - COMP_VAR0];
                if (!m_currFrame->isUniq[varRef])
                    continue;

                PixType *pred0 = vp9scratchpad.interpBufs[varRef][0][ZEROMV_];
                PixType *pred1 = vp9scratchpad.interpBufs[fixRef][0][ZEROMV_];
                PixType *pred  = vp9scratchpad.interpBufs[refIdx][0][ZEROMV_];
                VP9PP::average(pred0, pred1, pred, MAX_CU_SIZE, LOG2_MAX_CU_SIZE - 2); // average existing predictions
                m_interp[refIdx][0][ZEROMV_] = pred;
                m_interp[refIdx][1][ZEROMV_] = pred;
                m_interp[refIdx][2][ZEROMV_] = pred;
                m_interp[refIdx][3][ZEROMV_] = pred;

                CalculateAndAccumulateSatds(m_ySrc, pred, m_zeroMvpSatd[refIdx], m_par->hadamardMe >= 2);
            }
        }
    }


    struct GpuMeData {
        H265MV mv;
        uint32_t idx;
    };

    template <typename PixType> void H265CU<PixType>::CalculateNewMvPredAndSatd()
    {
        enum { SIZE8 = 0, SIZE16 = 1, SIZE32 = 2, SIZE64 = 3 };
        enum { DEPTH8 = 3, DEPTH16 = 2, DEPTH32 = 1, DEPTH64 = 0 };

        const int32_t secondRef = m_currFrame->compoundReferenceAllowed
            ? ALTREF_FRAME
            : m_currFrame->isUniq[GOLDEN_FRAME] ? GOLDEN_FRAME : LAST_FRAME;
        __ALIGN2 const RefIdx mapGpuRefIdx[3][2] = { {LAST_FRAME, NONE_FRAME}, {(RefIdx)secondRef, NONE_FRAME}, {LAST_FRAME, ALTREF_FRAME} };

        { // 8x8
            const int32_t pitchGpuMeData8 = m_currFrame->m_feiInterData[LAST_FRAME][SIZE8]->m_pitch;
            const int32_t offsetGpuMeData8 = (m_ctbPelY >> 3) * pitchGpuMeData8 + m_ctbPelX;
            const GpuMeData *gpuMeData[2] = {
                reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[LAST_FRAME][SIZE8]->m_sysmem + offsetGpuMeData8),
                reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[secondRef][SIZE8]->m_sysmem + offsetGpuMeData8)
            };



            const int32_t num8x8w = MIN(8, (m_par->Width  - m_ctbPelX) >> 3);
            const int32_t num8x8h = MIN(8, (m_par->Height - m_ctbPelY) >> 3);

            for (int32_t yblk = 0; yblk < num8x8h; yblk++) {
                for (int32_t xblk = 0; xblk < num8x8w; xblk += 2) {

                    const int32_t numBlk = (yblk << 3) + xblk;

                    int32_t gpuRefIdx = gpuMeData[0][xblk + 0].idx & 0x3;
                    As2B(m_newMvRefIdx8[numBlk + 0]) = As2B(mapGpuRefIdx[ gpuRefIdx ]);
                    m_newMvFromGpu8[numBlk + 0][0].mvx = gpuMeData[gpuRefIdx&1][xblk + 0].mv.mvx << 1;
                    m_newMvFromGpu8[numBlk + 0][0].mvy = gpuMeData[gpuRefIdx&1][xblk + 0].mv.mvy << 1;
                    ClipMV_NR(&m_newMvFromGpu8[numBlk + 0][0]);
                    if (m_newMvRefIdx8[numBlk + 0][1] != NONE_FRAME) {
                        m_newMvFromGpu8[numBlk + 0][1].mvx = gpuMeData[1][xblk + 0].mv.mvx << 1;
                        m_newMvFromGpu8[numBlk + 0][1].mvy = gpuMeData[1][xblk + 0].mv.mvy << 1;
                        ClipMV_NR(&m_newMvFromGpu8[numBlk + 0][1]);
                    } else {
                        m_newMvFromGpu8[numBlk + 0][1] = MV_ZERO;
                    }
                    m_newMvSatd8[numBlk + 0] = gpuMeData[0][xblk + 0].idx >> 2;

                    // store best mv, refIdx for second block
                    gpuRefIdx = gpuMeData[0][xblk + 1].idx & 0x3;
                    As2B(m_newMvRefIdx8[numBlk + 1]) = As2B(mapGpuRefIdx[ gpuRefIdx ]);
                    m_newMvFromGpu8[numBlk + 1][0].mvx = gpuMeData[gpuRefIdx&1][xblk + 1].mv.mvx << 1;
                    m_newMvFromGpu8[numBlk + 1][0].mvy = gpuMeData[gpuRefIdx&1][xblk + 1].mv.mvy << 1;
                    ClipMV_NR(&m_newMvFromGpu8[numBlk + 1][0]);
                    if (m_newMvRefIdx8[numBlk + 1][1] != NONE_FRAME) {
                        m_newMvFromGpu8[numBlk + 1][1].mvx = gpuMeData[1][xblk + 1].mv.mvx << 1;
                        m_newMvFromGpu8[numBlk + 1][1].mvy = gpuMeData[1][xblk + 1].mv.mvy << 1;
                        ClipMV_NR(&m_newMvFromGpu8[numBlk + 1][1]);
                    } else {
                        m_newMvFromGpu8[numBlk + 1][1] = MV_ZERO;
                    }
                    m_newMvSatd8[numBlk + 1] = gpuMeData[0][xblk + 1].idx >> 2;
                }
                gpuMeData[0] = reinterpret_cast<const GpuMeData *>(reinterpret_cast<const uint8_t *>(gpuMeData[0]) + pitchGpuMeData8);
                gpuMeData[1] = reinterpret_cast<const GpuMeData *>(reinterpret_cast<const uint8_t *>(gpuMeData[1]) + pitchGpuMeData8);
            }

            PixType *interpData = reinterpret_cast<PixType *>(m_currFrame->m_feiInterp[SIZE8]->m_sysmem + m_ctbAddr * (64 * 64));
            m_newMvInterp8 = interpData;
        }

        { // 16x16
            const int32_t pitchGpuMeData16 = m_currFrame->m_feiInterData[LAST_FRAME][SIZE16]->m_pitch;
            const int32_t offsetGpuMeData16 = (m_ctbPelY >> 4) * pitchGpuMeData16 + (m_ctbPelX >> 1);
            const GpuMeData *gpuMeData[2] = {
                reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[LAST_FRAME][SIZE16]->m_sysmem + offsetGpuMeData16),
                reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[secondRef][SIZE16]->m_sysmem + offsetGpuMeData16)
            };

            const int32_t num16x16w = MIN(4, (m_par->Width  - m_ctbPelX) >> 4);
            const int32_t num16x16h = MIN(4, (m_par->Height - m_ctbPelY) >> 4);

            for (int32_t yblk = 0; yblk < num16x16h; yblk++) {
                for (int32_t xblk = 0; xblk < num16x16w; xblk++) {
                    const int32_t numBlk = (yblk << 2) + xblk;
                    // store best mv, refIdx for first block
                    int32_t gpuRefIdx = gpuMeData[0][xblk + 0].idx & 0x3;
                    m_newMvSatd16[numBlk] = gpuMeData[0][xblk + 0].idx >> 2;
                    As2B(m_newMvRefIdx16[numBlk + 0]) = As2B(mapGpuRefIdx[ gpuRefIdx ]);
                    m_newMvFromGpu16[numBlk + 0][0].mvx = gpuMeData[gpuRefIdx&1][xblk + 0].mv.mvx << 1;
                    m_newMvFromGpu16[numBlk + 0][0].mvy = gpuMeData[gpuRefIdx&1][xblk + 0].mv.mvy << 1;
                    ClipMV_NR(&m_newMvFromGpu16[numBlk + 0][0]);
                    if (m_newMvRefIdx16[numBlk + 0][1] != NONE_FRAME) {
                        m_newMvFromGpu16[numBlk + 0][1].mvx = gpuMeData[1][xblk + 0].mv.mvx << 1;
                        m_newMvFromGpu16[numBlk + 0][1].mvy = gpuMeData[1][xblk + 0].mv.mvy << 1;
                        ClipMV_NR(&m_newMvFromGpu16[numBlk + 0][1]);
                    } else {
                        m_newMvFromGpu16[numBlk + 0][1] = MV_ZERO;
                    }
                }
                gpuMeData[0] = reinterpret_cast<const GpuMeData *>(reinterpret_cast<const uint8_t *>(gpuMeData[0]) + pitchGpuMeData16);
                gpuMeData[1] = reinterpret_cast<const GpuMeData *>(reinterpret_cast<const uint8_t *>(gpuMeData[1]) + pitchGpuMeData16);
            }
            PixType *interpData = reinterpret_cast<PixType *>(m_currFrame->m_feiInterp[SIZE16]->m_sysmem + m_ctbAddr * (64 * 64));
            m_newMvInterp16 = interpData;
        }

        { // 32x32
            const int32_t pitchGpuMeData32 = m_currFrame->m_feiInterData[LAST_FRAME][SIZE32]->m_pitch;
            const int32_t offsetGpuMeData32 = (m_ctbPelY >> 5) * pitchGpuMeData32 + (m_ctbPelX >> 5) * 64;

            const int32_t num32x32w = MIN(2, (m_par->Width  - m_ctbPelX) >> 5);
            const int32_t num32x32h = MIN(2, (m_par->Height - m_ctbPelY) >> 5);

            for (int32_t yblk = 0; yblk < num32x32h; yblk++) {
                for (int32_t xblk = 0; xblk < num32x32w; xblk++) {
                    const int32_t numBlk = (yblk << 1) + xblk;

                    const GpuMeData *gpuMeData[2] = {
                        reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[LAST_FRAME][SIZE32]->m_sysmem + offsetGpuMeData32 + pitchGpuMeData32*yblk + 64*xblk),
                        reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[secondRef][SIZE32]->m_sysmem + offsetGpuMeData32 + pitchGpuMeData32*yblk + 64*xblk)
                    };
                    // store best mv, refIdx for first block
                    int32_t gpuRefIdx = gpuMeData[0]->idx & 0x3;
                    m_newMvSatd32[numBlk] = gpuMeData[0]->idx >> 2;
                    As2B(m_newMvRefIdx32[numBlk + 0]) = As2B(mapGpuRefIdx[ gpuRefIdx ]);

                    m_newMvFromGpu32[numBlk + 0][0].mvx = gpuMeData[gpuRefIdx&1]->mv.mvx << 1;
                    m_newMvFromGpu32[numBlk + 0][0].mvy = gpuMeData[gpuRefIdx&1]->mv.mvy << 1;

                    ClipMV_NR(&m_newMvFromGpu32[numBlk + 0][0]);

                    if (m_newMvRefIdx32[numBlk + 0][1] != NONE_FRAME) {
                        m_newMvFromGpu32[numBlk + 0][1].mvx = gpuMeData[1]->mv.mvx << 1;
                        m_newMvFromGpu32[numBlk + 0][1].mvy = gpuMeData[1]->mv.mvy << 1;
                        ClipMV_NR(&m_newMvFromGpu32[numBlk + 0][1]);
                    } else {
                        m_newMvFromGpu32[numBlk + 0][1] = MV_ZERO;
                    }
                }
            }
            PixType *interpData = reinterpret_cast<PixType *>(m_currFrame->m_feiInterp[SIZE32]->m_sysmem + m_ctbAddr * (64 * 64));
            m_newMvInterp32 = interpData;
        }

        // SLM doesn't work on HSW and some issue on SKL with GT2
        if (1)
        { // 64x64
            const int32_t pitchGpuMeData64 = m_currFrame->m_feiInterData[LAST_FRAME][SIZE64]->m_pitch;
            const int32_t offsetGpuMeData64 = (m_ctbPelY >> 6) * pitchGpuMeData64 + (m_ctbPelX >> 6) * 64;

            const int32_t num64x64w = MIN(1, (m_par->Width  - m_ctbPelX) >> 6);
            const int32_t num64x64h = MIN(1, (m_par->Height - m_ctbPelY) >> 6);

            for (int32_t yblk = 0; yblk < num64x64h; yblk++) {
                for (int32_t xblk = 0; xblk < num64x64w; xblk++) {
                    const int32_t numBlk = (yblk << 1) + xblk;

                    const GpuMeData *gpuMeData[2] = {
                        reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[LAST_FRAME][SIZE64]->m_sysmem + offsetGpuMeData64 + pitchGpuMeData64*yblk + 64*xblk),
                        reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[secondRef][SIZE64]->m_sysmem + offsetGpuMeData64 + pitchGpuMeData64*yblk + 64*xblk)
                    };
                    // store best mv, refIdx for first block
                    int32_t gpuRefIdx = gpuMeData[0]->idx & 0x3;
                    m_newMvSatd64[numBlk] = gpuMeData[0]->idx >> 2;
                    As2B(m_newMvRefIdx64[numBlk + 0]) = As2B(mapGpuRefIdx[ gpuRefIdx ]);

                    m_newMvFromGpu64[numBlk + 0][0].mvx = gpuMeData[gpuRefIdx&1]->mv.mvx << 1;
                    m_newMvFromGpu64[numBlk + 0][0].mvy = gpuMeData[gpuRefIdx&1]->mv.mvy << 1;

                    ClipMV_NR(&m_newMvFromGpu64[numBlk + 0][0]);

                    if (m_newMvRefIdx64[numBlk + 0][1] != NONE_FRAME) {
                        m_newMvFromGpu64[numBlk + 0][1].mvx = gpuMeData[1]->mv.mvx << 1;
                        m_newMvFromGpu64[numBlk + 0][1].mvy = gpuMeData[1]->mv.mvy << 1;
                        ClipMV_NR(&m_newMvFromGpu64[numBlk + 0][1]);
                    } else {
                        m_newMvFromGpu64[numBlk + 0][1] = MV_ZERO;
                    }
                }
            }
            PixType *interpData = reinterpret_cast<PixType *>(m_currFrame->m_feiInterp[SIZE64]->m_sysmem + m_ctbAddr * (64 * 64));
            m_newMvInterp64 = interpData;
        }
    }


    template <typename PixType>
    void H265CU<PixType>::InitCu(const H265VideoParam &par, ModeInfo *_data, ModeInfo *_dataTemp,
                                 int32_t ctbRow, int32_t ctbCol, const Frame &frame, CoeffsType *m_coeffWork)
    {
        m_par = &par;
        m_sliceQpY = frame.m_sliceQpY;
        //m_lcuQps = &frame.m_lcuQps[0];

        m_data = _data;
        m_dataStored = _dataTemp;
        m_ctbAddr = ctbRow * m_par->PicWidthInCtbs + ctbCol;
        m_ctbPelX = ctbCol * m_par->MaxCUSize;
        m_ctbPelY = ctbRow * m_par->MaxCUSize;

        m_currFrame = &frame;

        FrameData* origin = m_currFrame->m_origin;
        PixType *originY  = reinterpret_cast<PixType*>(origin->y) + m_ctbPelX + m_ctbPelY * origin->pitch_luma_pix;
        PixType *originUv = reinterpret_cast<PixType*>(origin->uv) + m_ctbPelX + (m_ctbPelY >> 1) * origin->pitch_chroma_pix;
        CopyNxN(originY, origin->pitch_luma_pix, m_ySrc, 64);
        CopyNxM(originUv, origin->pitch_chroma_pix, m_uvSrc, 64, 32);
        m_pitchRecLuma = origin->pitch_luma_pix;
        m_pitchRecChroma = origin->pitch_chroma_pix;

        m_coeffWorkY = m_coeffWork;
        m_coeffWorkU = m_coeffWorkY + m_par->MaxCUSize * m_par->MaxCUSize;
        m_coeffWorkV = m_coeffWorkU + (m_par->MaxCUSize * m_par->MaxCUSize >> m_par->chromaShift);

        // limits to clip MV allover the CU
        const int32_t MvShift = 3;
        const int32_t offset = 8;
        HorMax = (int16_t)IPP_MIN((m_par->Width + offset - m_ctbPelX - 1) << MvShift, IPP_MAX_16S);
        HorMin = (int16_t)IPP_MAX((-m_par->MaxCUSize - offset - m_ctbPelX + 1) << MvShift, IPP_MIN_16S);
        VerMax = (int16_t)IPP_MIN((m_par->Height + offset - m_ctbPelY - 1) << MvShift, IPP_MAX_16S);
        VerMin = (int16_t)IPP_MAX((-m_par->MaxCUSize - offset - m_ctbPelY + 1) << MvShift, IPP_MIN_16S);

        std::fill_n(m_costStored, sizeof(m_costStored) / sizeof(m_costStored[0]), COST_MAX);
        m_costCurr = 0.f;

        m_rdLambda = frame.m_lambda;
        m_rdLambdaSqrt = frame.m_lambdaSatd;
        m_rdLambdaSqrtInt = frame.m_lambdaSatdInt;
        m_rdLambdaSqrtInt32 = uint32_t(frame.m_lambdaSatd * 2048 + 0.5);
        // m_ChromaDistWeight = m_cslice->ChromaDistWeight_slice;
        // m_rdLambdaChroma = m_rdLambda*(3.0+1.0/m_ChromaDistWeight)/4.0;

        m_lumaQp = m_sliceQpY;
        m_chromaQp = m_sliceQpY;

        m_tileBorders = GetTileBordersMi(m_par->tileParam, ctbRow << 3, ctbCol << 3);
        const int32_t ctbColWithinTile = ctbCol - (m_tileBorders.colStart >> 3);
        const int32_t ctbRowWithinTile = ctbRow - (m_tileBorders.rowStart >> 3);

        // Setting neighbor CU
        m_availForPred.curr = m_data;
        m_availForPred.left = NULL;
        m_availForPred.above = NULL;
        m_availForPred.aboveLeft = NULL;
        m_availForPred.colocated = NULL;
        if (m_currFrame->m_prevFrame)
            m_availForPred.colocated = m_currFrame->m_prevFrame->cu_data + (ctbCol << 3) + (ctbRow << 3) * m_par->miPitch;
        if (ctbColWithinTile)
            m_availForPred.left = m_data - 256;
        if (ctbRowWithinTile)
            m_availForPred.above = m_data - (m_par->PicWidthInCtbs << 8);
        if (m_availForPred.left && m_availForPred.above)
            m_availForPred.aboveLeft = m_availForPred.above - 256;

        const TileContexts &tileCtx = frame.m_tileContexts[ GetTileIndex(par.tileParam, ctbRow, ctbCol) ];
        As16B(m_contexts.aboveNonzero[0]) = As16B(tileCtx.aboveNonzero[0] + (ctbColWithinTile << 4));
        As8B(m_contexts.aboveNonzero[1])  = As8B(tileCtx.aboveNonzero[1]  + (ctbColWithinTile << 3));
        As8B(m_contexts.aboveNonzero[2])  = As8B(tileCtx.aboveNonzero[2]  + (ctbColWithinTile << 3));
        As16B(m_contexts.leftNonzero[0])  = As16B(tileCtx.leftNonzero[0]  + (ctbRowWithinTile << 4));
        As8B(m_contexts.leftNonzero[1])   = As8B(tileCtx.leftNonzero[1]   + (ctbRowWithinTile << 3));
        As8B(m_contexts.leftNonzero[2])   = As8B(tileCtx.leftNonzero[2]   + (ctbRowWithinTile << 3));
        As8B(m_contexts.abovePartition)   = As8B(tileCtx.abovePartition   + (ctbColWithinTile << 3));
        As8B(m_contexts.leftPartition)    = As8B(tileCtx.leftPartition    + (ctbRowWithinTile << 3));
        if (!m_availForPred.above) {
#ifdef ZERO_NNZ
            Zero(m_contexts.aboveNonzero);
#else
            memset(&m_contexts.aboveNonzero, 1, sizeof(m_contexts.aboveNonzero));
#endif
            Zero(m_contexts.abovePartition);
        }
        if (!m_availForPred.left) {
#ifdef ZERO_NNZ
            Zero(m_contexts.leftNonzero);
#else
            memset(&m_contexts.leftNonzero, 1, sizeof(m_contexts.leftNonzero));
#endif
            Zero(m_contexts.leftPartition);
        }
        m_contextsInitSb = m_contexts;

        m_SCid[0][0]    = 5;
        m_SCpp[0][0]    = 1764.0;
        m_STC[0][0]     = 2;
        m_mvdMax  = 128;
        m_mvdCost = 60;
        m_intraMinDepth = 0;
        m_adaptMaxDepth = MAX_TOTAL_DEPTH;
        m_projMinDepth = 0;
        m_adaptMinDepth = 0;
        //if (m_par->minCUDepthAdapt)
        //    GetAdaptiveMinDepth(0, 0);
        m_isHistBuilt = false;

        if (!frame.IsIntra()) {
            CalculateZeroMvPredAndSatd();
            if (m_par->enableCmFlag)
                CalculateNewMvPredAndSatd();
        }
    }

    BlockSize GetPlaneBlockSize(BlockSize subsize, int32_t plane, const H265VideoParam &par) {
        int32_t subx = plane > 0 ? par.subsamplingX : 0;
        int32_t suby = plane > 0 ? par.subsamplingY : 0;
        return ss_size_lookup[subsize][subx][suby];
    }

    TxSize GetUvTxSize(int32_t miSize, TxSize txSize, const H265VideoParam &par) {
        if (miSize < BLOCK_4X4)
            return TX_4X4;
        return IPP_MIN(txSize, max_txsize_lookup[GetPlaneBlockSize((BlockSize)miSize, 1, par)]);
    }

    void UpdatePartitionCtx(Contexts *ctx, int32_t x4, int32_t y4, int32_t depth, int32_t partitionType) {
        BlockSize bsz = GetSbType(depth, (PartitionType)partitionType);
        int32_t size = 64 >> depth;
        memset(ctx->abovePartition + (x4 >> 1), partition_context_lookup[bsz].above, size >> 3);
        memset(ctx->leftPartition  + (y4 >> 1), partition_context_lookup[bsz].left,  size >> 3);
    }

    template <typename PixType>
    int32_t  H265CU<PixType>::GetCmDistMv(int32_t sbx, int32_t sby, int32_t log2w, int32_t& mvd, H265MVPair predMv[5]) {
        const int32_t w = 1 << (log2w + 2);
        const int32_t h = w;

        int32_t satd = INT_MAX;
        RefIdx *bestRefIdx;
        H265MVPair bestMv;

        int8_t refIdxComb;
        if (log2w == 1) {
            const int32_t numBlk = sby + (sbx >> 3);
            bestRefIdx = m_newMvRefIdx8[numBlk];
            bestMv = m_newMvFromGpu8[numBlk];
            refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            satd = m_newMvSatd8[numBlk];
        } else if (log2w == 2) {
            const int32_t numBlk = (sby >> 2) + (sbx >> 4);
            bestRefIdx = m_newMvRefIdx16[numBlk];
            bestMv = m_newMvFromGpu16[numBlk];
            refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            satd = m_newMvSatd16[numBlk];
        } else if (log2w == 3) {
            const int32_t numBlk = (sby >> 5) * 2 + (sbx >> 5);
            bestRefIdx = m_newMvRefIdx32[numBlk];
            bestMv = m_newMvFromGpu32[numBlk];
            refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            satd = m_newMvSatd32[numBlk];
        } else if (log2w == 4) {
            const int32_t numBlk = (sby >> 6) + (sbx >> 6);
            bestRefIdx = m_newMvRefIdx64[numBlk];
            bestMv = m_newMvFromGpu64[numBlk];
            refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            satd = m_newMvSatd64[numBlk];

        } else {
            assert(0);
        }

        mvd = abs(predMv[refIdxComb][0].mvx - bestMv[0].mvx);
        mvd += abs(predMv[refIdxComb][0].mvy - bestMv[0].mvy);

        if (bestRefIdx[1] != NONE_FRAME) {
            int32_t mvd1 = abs(predMv[refIdxComb][1].mvx - bestMv[1].mvx);
            mvd1 += abs(predMv[refIdxComb][1].mvy - bestMv[1].mvy);
            if (mvd1 < mvd) mvd = mvd1;
        }
        return satd;
    }

    template <typename PixType>
    inline bool H265CU<PixType>::IsBadPred(float SCpp, float SADpp_best, float mvdAvg) const
    {
        if (m_currFrame->m_pyramidLayer>2) return false;

        bool badPred = false;
        int32_t l = m_currFrame->m_pyramidLayer;
        int32_t Q = m_currFrame->m_sliceQpY - ((l + 1) * 8);
        int32_t qidx = 0;
        if (Q<75) qidx = 0;
        else if (Q<123) qidx = 1;
        else if (Q<163) qidx = 2;
        else            qidx = 3;

        static const float mtfi[3][4] = {
            { 0.040435f*64.0f, 0.107380f*64.0f, 0.113114f*64.0f, 0.119474f*64.0f },
            { 0.113734f*64.0f, 0.113734f*64.0f, 0.113734f*64.0f, 0.155650f*64.0f },
            { 0.169629f*64.0f, 0.169629f*64.0f, 0.169629f*64.0f, 0.169629f*64.0f }
        };
        static const float stfi[3][4] = {
            { 0.020000f*1000.0f, 0.020000f*1000.0f, 0.020000f*1000.0f, 0.020000f*1000.0f },
            { 0.077150f*1000.0f, 0.089805f*1000.0f, 0.089805f*1000.0f, 0.249454f*1000.0f },
            { 0.142184f*1000.0f, 0.142184f*1000.0f, 0.249454f*1000.0f, 0.249454f*1000.0f }
        };

        if (SCpp > stfi[l][qidx] && mvdAvg > mtfi[l][qidx]) {

            // sq err from 4 mono from stfi
            static const float afi[3][4] = {
                { 0.303627f, 0.315012f, 0.315445f, 0.314350f },
                { 0.423133f, 0.435791f, 0.443448f, 0.456550f },
                { 0.397384f, 0.397396f, 0.500000f, 0.500000f }
            };
            static const float bfi[3][4] = {
                { 0.547588f, 0.572353f, 0.677961f, 0.794845f },
                { 0.022428f, 0.041593f, 0.203189f, 0.228990f },
                { 0.473092f, 0.527727f, 0.000000f, 0.000000f }
            };

            static const float mfi[3][4] = {
                { 0.003458f, 0.006642f, 0.006441f, 0.010107f },
                { 0.013396f, 0.013410f, 0.013410f, 0.137010f },
                { 0.013396f, 0.013410f, 0.013410f, 0.137010f }
            };

            float SADT = bfi[l][qidx] + logf(SCpp)*afi[l][qidx];
            float msad = logf(SADpp_best + mfi[l][qidx] * IPP_MIN(64, mvdAvg));

            if (msad > SADT)       badPred = true;
        }
        return badPred;
    }

    template <typename PixType>
    inline bool H265CU<PixType>::IsGoodPred(float SCpp, float SADpp, float mvdAvg) const
    {
        bool goodPred = false;
        int32_t l = m_currFrame->m_pyramidLayer;
        int32_t Q = m_currFrame->m_sliceQpY - ((l + 1) * 8);
        int32_t qidx = 0;
        if (Q<75) qidx = 0;
        else if (Q<123) qidx = 1;
        else if (Q<163) qidx = 2;
        else            qidx = 3;

        // sq err from 4 mono opt
        static const float ati[4][4] = {
            { 0.224285f, 0.225157f, 0.228965f, 0.244477f },
            { 0.233921f, 0.281690f, 0.276006f, 0.308719f },
            { 0.242109f, 0.286192f, 0.302635f, 0.324380f },
            { 0.265764f, 0.279662f, 0.336254f, 0.378376f }
        };
        static const float bti[4][4] = {
            { 0.278397f, 0.437689f, 0.628257f, 0.688823f },
            { 0.413391f, 0.314939f, 0.525718f, 0.626691f },
            { 0.424680f, 0.365299f, 0.548104f, 0.605718f },
            { 0.517953f, 0.622422f, 0.617987f, 0.553362f }
        };

        static const float mti[4][4] = {
            { 0.295610f, 0.201050f, 0.177870f, 0.146770f },
            { 0.218830f, 0.177870f, 0.146770f, 0.063581f },
            { 0.089055f, 0.028076f, 0.028076f, 0.028076f },
            { 0.049053f, 0.025316f, 0.023686f, 0.012673f }
        };

        float SADT = bti[l][qidx] + logf(SCpp)*ati[l][qidx];
        float msad = logf(SADpp + mti[l][qidx] * mvdAvg);
        if (msad < SADT)       goodPred = true;

        return goodPred;
    }

    void compute_rscs_px(const unsigned char *ySrc, int pitchSrc, int *lcuRs, int *lcuCs, int pitchRsCs, int width, int height)
    {
        for (int i = 0; i<height; i += 4)
        {
            for (int j = 0; j<width; j += 4)
            {
                int Rs = 0;
                int Cs = 0;
                for (int k = 0; k<4; k++)
                {
                    for (int l = 0; l<4; l++)
                    {
                        int temp = ySrc[(i + k)*pitchSrc + (j + l)] - ySrc[(i + k)*pitchSrc + (j + l - 1)];
                        Cs += temp*temp;

                        temp = ySrc[(i + k)*pitchSrc + (j + l)] - ySrc[(i + k - 1)*pitchSrc + (j + l)];
                        Rs += temp*temp;
                    }
                }
                lcuCs[(i >> 2)*pitchRsCs + (j >> 2)] = Cs >> 4;
                lcuRs[(i >> 2)*pitchRsCs + (j >> 2)] = Rs >> 4;
            }
        }
    }

    void SumUp(int32_t *out, int32_t pitchOut, const int32_t *in, int32_t pitchIn, uint32_t Width, uint32_t Height, int32_t shift) {
        for (uint32_t y = 0; y < ((Height + (1 << shift) - 1) >> shift); y++) {
            for (uint32_t x = 0; x < ((Width + (1 << shift) - 1) >> shift); x++)
                out[y*pitchOut + x] = in[(2 * y)*pitchIn + (2 * x)] + in[(2 * y)*pitchIn + (2 * x + 1)] + in[(2 * y + 1)*pitchIn + (2 * x)] + in[(2 * y + 1)*pitchIn + (2 * x + 1)];
        }
    }

    template <typename PixType> void H265CU<PixType>::JoinMI()
    {
        // Join MI
        int32_t starti = 0;
        do {
            const int32_t rasterIdxO = h265_scan_z2r4[starti];
            const int32_t x4O = (rasterIdxO & 15);
            const int32_t y4O = (rasterIdxO >> 4);
            ModeInfo *miO = m_data + (x4O >> 1) + (y4O >> 1) * m_par->miPitch;

            const int32_t log2wO = block_size_wide_4x4_log2[miO->sbType];
            const int32_t log2hO = block_size_high_4x4_log2[miO->sbType];
            const int32_t x4TL = (x4O >> (log2wO + 1)) << (log2wO + 1);
            const int32_t y4TL = (y4O >> (log2hO + 1)) << (log2hO + 1);
            bool isTL = (log2wO < 4 && log2wO == log2hO && x4O == x4TL && y4O == y4TL) ? true : false;

            if (isTL)
            {
                int32_t numParts = 0;
                int32_t join_num_parts = IPP_MIN(MAX_NUM_PARTITIONS, starti + 4 * num_4x4_blocks_lookup[miO->sbType]);
                for (int32_t i = starti; i < join_num_parts;) {
                    ModeInfo *mi = m_data + ((h265_scan_z2r4[i] & 15) >> 1) + ((h265_scan_z2r4[i] >> 4) >> 1) * m_par->miPitch;
                    i += num_4x4_blocks_lookup[mi->sbType];
                    numParts++;
                }
                if (numParts == 4)
                {
                    int32_t join = 1;
                    int32_t jskip = miO->skip;
                    int32_t sad = miO->sad;
                    for (int32_t i = starti + num_4x4_blocks_lookup[miO->sbType]; i < join_num_parts;) {
                        ModeInfo *mi = m_data + ((h265_scan_z2r4[i] & 15) >> 1) + ((h265_scan_z2r4[i] >> 4) >> 1) * m_par->miPitch;
                        jskip &= mi->skip;
                        sad += mi->sad;
                        if (mi->mode != OUT_OF_PIC && mi->refIdx[0] == miO->refIdx[0] && mi->mv[0].asInt == miO->mv[0].asInt && mi->refIdx[1] == miO->refIdx[1]) {
                            if (miO->refIdx[1] == NONE_FRAME)  join++;
                            else if (mi->mv[1].asInt == miO->mv[1].asInt) join++;
                        }
                        i += num_4x4_blocks_lookup[mi->sbType];
                    }
                    if (join == 4) {
#if PROTOTYPE_GPU_MODE_DECISION
                        miO->skip = jskip;
                        miO->sbType += 3;
                        miO->sad = sad;
                        PropagateSubPart(miO, m_par->miPitch, 1 << log2wO, 1 << log2hO);
                        starti = 0;
#else
                        H265MV clampedMV[2] = { miO->mv[0], miO->mv[1] };
                        ClampMvRefAV1(&clampedMV[0], miO->sbType + 3, (m_ctbPelY >> 3) + (y4O >> 1), (m_ctbPelX >> 3) + (x4O >> 1), m_par);
                        if (miO->refIdx[1] != NONE_FRAME) ClampMvRefAV1(&clampedMV[1], miO->sbType + 3, (m_ctbPelY >> 3) + (y4O >> 1), (m_ctbPelX >> 3) + (x4O >> 1), m_par);
                        if (clampedMV[0].asInt == miO->mv[0].asInt && clampedMV[1].asInt == miO->mv[1].asInt) {
                            miO->skip = jskip;
                            miO->sbType += 3;
                            starti = 0;
                            miO->txSize = IPP_MIN(TX_32X32, max_txsize_lookup[miO->sbType]);
                            PixType *bestInterPred = m_bestInterp[4 - (log2wO + 1)][(y4O >> 1) & 7][(x4O >> 1) & 7];
                            const PixType *bestInterPred1 = m_bestInterp[4 - log2wO][(y4O >> 1) & 7][(x4O >> 1) & 7];
                            const PixType *bestInterPred2 = m_bestInterp[4 - log2wO][(y4O >> 1) & 7][((x4O + (1 << log2wO)) >> 1) & 7];
                            const PixType *bestInterPred3 = m_bestInterp[4 - log2wO][((y4O + (1 << log2hO)) >> 1) & 7][(x4O >> 1) & 7];
                            const PixType *bestInterPred4 = m_bestInterp[4 - log2wO][((y4O + (1 << log2hO)) >> 1) & 7][((x4O + (1 << log2wO)) >> 1) & 7];
                            CopyNxN(bestInterPred1, 64, bestInterPred, 64, 1 << (log2wO + 2));
                            CopyNxN(bestInterPred2, 64, bestInterPred, 64, 1 << (log2wO + 2));
                            CopyNxN(bestInterPred3, 64, bestInterPred, 64, 1 << (log2wO + 2));
                            CopyNxN(bestInterPred4, 64, bestInterPred, 64, 1 << (log2wO + 2));
                        }
                        else {
                            starti = join_num_parts;
                        }
#endif
                    }
                    else {
                        starti = join_num_parts;
                    }
                }
                else {
                    assert(numParts > 4);
                    starti += num_4x4_blocks_lookup[miO->sbType];
                }
            }
            else {
                starti += num_4x4_blocks_lookup[miO->sbType];
            }
        } while (starti < MAX_NUM_PARTITIONS);
        /*int lcuRs[256];
        int lcuCs[256];
        int lcuRs8[64];
        int lcuCs8[64];
        int lcuRs16[16];
        int lcuCs16[16];
        int lcuRs32[4];
        int lcuCs32[4];
        int lcuRs64[1];
        int lcuCs64[1];
        compute_rscs_px(m_currFrame->m_origin->y+m_ctbPelY*m_currFrame->m_origin->pitch_luma_pix+m_ctbPelX, m_currFrame->m_origin->pitch_luma_pix, lcuRs, lcuCs, 16, 64, 64);
        SumUp(lcuRs8, 8, lcuRs, 16, 64, 64, 3);
        SumUp(lcuRs16, 4, lcuRs8, 8, 64, 64, 4);
        SumUp(lcuRs32, 2, lcuRs16, 4, 64, 64, 5);
        SumUp(lcuRs64, 1, lcuRs32, 2, 64, 64, 6);
        SumUp(lcuCs8, 8, lcuCs, 16, 64, 64, 3);
        SumUp(lcuCs16, 4, lcuCs8, 8, 64, 64, 4);
        SumUp(lcuCs32, 2, lcuCs16, 4, 64, 64, 5);
        SumUp(lcuCs64, 1, lcuCs32, 2, 64, 64, 6);*/
#if 1
        // Split MI
        bool split64 = (m_data->sbType == BLOCK_64X64) ? true : false;
        if (split64) {
            float scpp = 20480.0f; // lcuRs64[0] + lcuCs64[0];
            scpp *= 0.0625f;
            scpp *= 0.0625f;
            int32_t mvd = 0;
#ifdef TRYINTRA_ORIG
            H265MVPair predMv[5] = {};
            predMv[0].mv[0].asInt = m_data->mvd[0].asInt;
            predMv[1].mv[0].asInt = m_data->mvd[1].asInt;
            predMv[2].mv[0].asInt = m_data->mvd[1].asInt;
            predMv[3].mv[0].asInt = m_data->mvd[0].asInt;
            predMv[3].mv[1].asInt = m_data->mvd[1].asInt;

            float sadpp = GetCmDistMv(0, 0, 4, mvd, predMv);
            sadpp *= 0.015625f;
            sadpp *= 0.015625f;
#else
            // For Split64 && TryIntra
            float sadpp = m_data->sad;
            if (m_data->refIdxComb == COMP_VAR0) {
                mvd += abs(m_data->mvd[0].mvx);
                mvd += abs(m_data->mvd[0].mvy);
                int32_t mvd1 = abs(m_data->mvd[1].mvx);
                mvd1 += abs(m_data->mvd[1].mvy);
                if (mvd1 < mvd) mvd = mvd1;
                //mvd += abs(m_data->mvd[1].mvx);
                //mvd += abs(m_data->mvd[1].mvy);
            } else {
                mvd += abs(m_data->mvd[0].mvx);
                mvd += abs(m_data->mvd[0].mvy);
            }
            sadpp *= 0.015625f;
            sadpp *= 0.015625f;
            sadpp *= 1.5f;
#endif
            sadpp = IPP_MAX(0.1, sadpp);
            split64 = (IsBadPred(scpp, sadpp, mvd)) ? true : false;
        }
        if (split64) {
            m_data->sbType -= 3;
            m_data->sad /= 4;
            PropagateSubPart(m_data, m_par->miPitch, 8, 8);
        }
#endif
#if 1
        // TryIntra
        int32_t num4x4 = MAX_NUM_PARTITIONS;
        for (int32_t i = 0; i < MAX_NUM_PARTITIONS; i += num4x4) {
            const int32_t rasterIdx = h265_scan_z2r4[i];
            const int32_t x4 = (rasterIdx & 15);
            const int32_t y4 = (rasterIdx >> 4);
            const int32_t sbx = x4 << 2;
            const int32_t sby = y4 << 2;
            //const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
            //const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);
            ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
            num4x4 = num_4x4_blocks_lookup[mi->sbType];
            if (mi->mode == OUT_OF_PIC)
                continue;

            const BlockSize bsz = mi->sbType;
            const int32_t log2w = block_size_wide_4x4_log2[bsz];
            const int32_t log2h = block_size_high_4x4_log2[bsz];
            //const int32_t num4x4w = 1 << log2w;
            //const int32_t num4x4h = 1 << log2h;

            bool tryIntra = (log2w < 4) ? true : false;
            if (tryIntra && !split64) {
                float scpp = (log2w == 3) ? (5120.00f + ((m_ctbPelX+sbx)>>5) + ((m_ctbPelY+sby)>>5)) : ((log2w == 2) ? (2560.0f + ((m_ctbPelX+sbx) >> 4) + ((m_ctbPelY+sby) >> 4)) : (1280.0f + ((m_ctbPelX+sbx) >> 3) + ((m_ctbPelY+sby) >> 3)));
                scpp *= 2;
                scpp /= (1 << ((log2w + 0) << 1));

                int32_t mvd = 0;
#ifdef TRYINTRA_ORIG
                H265MVPair predMv[5] = {};
                predMv[0].mv[0].asInt = mi->mvd[0].asInt;
                predMv[1].mv[0].asInt = mi->mvd[1].asInt;
                predMv[2].mv[0].asInt = mi->mvd[1].asInt;
                predMv[3].mv[0].asInt = mi->mvd[0].asInt;
                predMv[3].mv[1].asInt = mi->mvd[1].asInt;

                float sadpp = GetCmDistMv(sbx, sby, log2w, mvd, predMv);
                sadpp /= (1 << ((log2w + 2) << 1));
#else
                // For Split64 && TryIntra
                float sadpp = mi->sad;
                if (mi->refIdxComb == COMP_VAR0) {
                    mvd += abs(mi->mvd[0].mvx);
                    mvd += abs(mi->mvd[0].mvy);
                    int32_t mvd1 = abs(mi->mvd[1].mvx);
                    mvd1 += abs(mi->mvd[1].mvy);
                    if (mvd1 < mvd) mvd = mvd1;
                }
                else {
                    mvd += abs(mi->mvd[0].mvx);
                    mvd += abs(mi->mvd[0].mvy);
                }
                sadpp /= (1 << ((log2w + 2) << 1));
                sadpp *= 1.5f;
#endif
                sadpp = IPP_MAX(0.1, sadpp);
                tryIntra = ((IsGoodPred(scpp, sadpp, mvd)) ? false : true);
                if (tryIntra && mi->mode != AV1_NEARESTMV && mi->skip) {
                    tryIntra = (IsBadPred(scpp, sadpp, mvd)) ? true : false;
                }
            }
            if (tryIntra) mi->sad = 0;
            else          mi->sad = AV1_INTRA_MODES;
            PropagateSubPart(mi, m_par->miPitch, 1 << (log2w-1), 1 << (log2h-1));
        }
#endif
    }

    template <typename PixType> template <int32_t codecType, int32_t depth>
    void H265CU<PixType>::ModeDecisionInterTu7(int32_t miRow, int32_t miCol)
    {
        const int32_t left = miCol << 3;
        const int32_t top  = miRow << 3;
        const int32_t size = MAX_CU_SIZE >> depth;
        const int32_t halfSize8 = size >> 4;
        int32_t allowNonSplit = (left + size <= m_par->Width && top + size <= m_par->Height);

        const int32_t bsl = 3 - depth;
        const int32_t abovePartition = (m_contexts.abovePartition[miCol & 7] >> bsl) & 1;
        const int32_t leftPartition = (m_contexts.leftPartition[miRow & 7] >> bsl) & 1;
        const int32_t partitionCtx = 4 * bsl + 2 * leftPartition + abovePartition;
        const int32_t halfSize = size >> 1;
        const int32_t hasRows = (top + halfSize < m_par->Height);
        const int32_t hasCols = (left + halfSize < m_par->Width);
        const uint32_t *partitionBits = (m_par->codecType == CODEC_VP9)
            ? m_currFrame->bitCount.vp9Partition[hasRows * 2 + hasCols][partitionCtx]
            : m_currFrame->bitCount.av1Partition[hasRows * 2 + hasCols][partitionCtx];
        ModeInfo *mi = m_currFrame->cu_data + miCol + miRow * m_par->miPitch;

        // cleanup cached MVs
        for (int32_t i = 0; i < 5; i++) {
            m_nonZeroMvp[i][depth][NEARESTMV_].asInt64 = 0;
            m_nonZeroMvp[i][depth][NEARMV_].asInt64 = 0;
            m_nonZeroMvp[i][depth][NEARMV1_].asInt64 = 0;
            m_nonZeroMvp[i][depth][NEARMV2_].asInt64 = 0;
        }

        CostType costInitial = m_costCurr;
        Contexts origCtx = m_contexts;
        if (depth == 0) m_split64 = false;

        if (allowNonSplit && depth == 0 && m_currFrame->m_pyramidLayer<3) {
            // Split MI
            bool split64 = false;
            if (m_currFrame->m_sliceQpY >= 40 && m_currFrame->m_sliceQpY <= 225) {
                float scpp = 40960.0f; // lcuRs64[0] + lcuCs64[0];
                scpp *= 0.0625f;
                scpp *= 0.0625f;
                int32_t mvd = 0;
                const BlockSize bsz = GetSbType(depth, PARTITION_NONE);
                (m_currFrame->compoundReferenceAllowed)
                    ? GetMvRefsAV1TU7B(bsz, miRow, miCol, &m_mvRefs, (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B), 1)
                    : GetMvRefsAV1TU7P(bsz, miRow, miCol, &m_mvRefs, 1);

                int8_t bestRefIdxComp;
                RefIdx *bestRefIdx;
                const int32_t sbx = (miCol & 7) << 3;
                const int32_t sby = (miRow & 7) << 3;
                const int32_t numBlk = (sby >> 6) + (sbx >> 6);
                bestRefIdx = m_newMvRefIdx64[numBlk];
                bestRefIdxComp = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;

                H265MVPair bestRefMv = m_mvRefs.mvs[bestRefIdxComp][NEARESTMV_];
                H265MVPair predMv[5] = {};
                predMv[0].mv[0].asInt = m_mvRefs.mvs[0][NEARESTMV_][0].asInt;
                predMv[1].mv[0].asInt = m_mvRefs.mvs[1][NEARESTMV_][0].asInt;
                predMv[2].mv[0].asInt = m_mvRefs.mvs[2][NEARESTMV_][0].asInt;
                predMv[3].mv[0].asInt = bestRefMv[0].asInt;
                predMv[3].mv[1].asInt = bestRefMv[1].asInt;

                float sadpp = GetCmDistMv(0, 0, 4, mvd, predMv);
                sadpp *= 0.015625f;
                sadpp *= 0.015625f;
                sadpp = IPP_MAX(0.1, sadpp);
                split64 = (IsBadPred(scpp, sadpp, mvd) && scpp > 20.00f && sadpp > 1.0f) ? true : false;
            }
            m_split64 = split64;
            if (split64) {
                allowNonSplit = 0;
            }
        }
        if (allowNonSplit && depth == 1 && m_currFrame->m_pyramidLayer < 2 && m_split64) {
            // Split MI
            bool split32 = false;
            if (m_currFrame->m_sliceQpY >= 40 && m_currFrame->m_sliceQpY <= 225) {
                float scpp = 10240.0f; // lcuRs64[0] + lcuCs64[0];
                scpp *= 0.125f;
                scpp *= 0.125f;
                int32_t mvd = 0;
                const BlockSize bsz = GetSbType(depth, PARTITION_NONE);
                (m_currFrame->compoundReferenceAllowed)
                    ? GetMvRefsAV1TU7B(bsz, miRow, miCol, &m_mvRefs, (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B), 1)
                    : GetMvRefsAV1TU7P(bsz, miRow, miCol, &m_mvRefs, 1);

                int8_t bestRefIdxComp;
                RefIdx *bestRefIdx;
                const int32_t sbx = (miCol & 7) << 3;
                const int32_t sby = (miRow & 7) << 3;
                const int32_t numBlk = (sby >> 5) * 2 + (sbx >> 5);
                bestRefIdx = m_newMvRefIdx32[numBlk];
                bestRefIdxComp = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;

                H265MVPair bestRefMv = m_mvRefs.mvs[bestRefIdxComp][NEARESTMV_];
                H265MVPair predMv[5] = {};
                predMv[0].mv[0].asInt = m_mvRefs.mvs[0][NEARESTMV_][0].asInt;
                predMv[1].mv[0].asInt = m_mvRefs.mvs[1][NEARESTMV_][0].asInt;
                predMv[2].mv[0].asInt = m_mvRefs.mvs[2][NEARESTMV_][0].asInt;
                predMv[3].mv[0].asInt = bestRefMv[0].asInt;
                predMv[3].mv[1].asInt = bestRefMv[1].asInt;

                int32_t bsad = GetCmDistMv(sbx, sby, 3, mvd, predMv);
                float sadpp = bsad;
                sadpp *= 0.03125f;
                sadpp *= 0.03125f;
                sadpp = IPP_MAX(0.1, sadpp);

                int32_t l = m_currFrame->m_pyramidLayer;
                int32_t Q = m_currFrame->m_sliceQpY - ((l + 1) * 8);
                int32_t qidx = 0;
                if (Q<75) qidx = 0;
                else if (Q<123) qidx = 1;
                else if (Q<163) qidx = 2;
                else            qidx = 3;

                float svar = 1;
                const float SVT = 0.5f;
                float sr = 1;
                float SRT = (qidx < 3) ? 0.65f : 0.55f;

                {
                    int32_t numBlk = (sby >> 4) * 4 + (sbx >> 4);
                    int32_t sad = m_newMvSatd16[numBlk];
                    int32_t sad_min = sad;
                    int32_t sad_max = sad;
                    int32_t ssad = sad;
                    numBlk = (sby >> 4) * 4 + ((sbx + 16) >> 4);
                    sad = m_newMvSatd16[numBlk];
                    sad_min = IPP_MIN(sad_min, sad);
                    sad_max = IPP_MAX(sad_max, sad);
                    ssad += sad;
                    numBlk = ((sby + 16) >> 4) * 4 + (sbx >> 4);
                    sad = m_newMvSatd16[numBlk];
                    sad_min = IPP_MIN(sad_min, sad);
                    sad_max = IPP_MAX(sad_max, sad);
                    ssad += sad;
                    numBlk = ((sby + 16) >> 4) * 4 + ((sbx + 16) >> 4);
                    sad = m_newMvSatd16[numBlk];
                    sad_min = IPP_MIN(sad_min, sad);
                    sad_max = IPP_MAX(sad_max, sad);
                    ssad += sad;
                    svar = (float)sad_min / (float)sad_max;
                    sr = (float)ssad / (float)bsad;

                }
                split32 = (IsBadPred(scpp, sadpp, mvd) && scpp > 20.0f && sadpp > 1.0f && svar <= SVT && sr <= SRT) ? true : false;
            }
            if (split32) {
                allowNonSplit = 0;
            }
        }
        if (allowNonSplit) {
            MeCuNonRdAV1<depth>(miRow, miCol);
            debug_printf("%d,%d,%d cost=%f sad=%d\n", depth, miRow, miCol, m_costCurr - costInitial, mi->sad);
#if KERNEL_DEBUG
            m_costCurr += m_rdLambda * partitionBits[PARTITION_NONE];
#endif // KERNEL_DEBUG

            //UpdatePartitionCtx(&m_contexts, x4, y4, depth, PARTITION_NONE);
            uint8_t *pa = m_contexts.abovePartition + (miCol & 7);
            uint8_t *pl = m_contexts.leftPartition + (miRow & 7);
            if (depth == 0) *(uint64_t *)pa = *(uint64_t *)pl = 0;
            if (depth == 1) *(uint32_t *)pa = *(uint32_t *)pl = 0x08080808;
            if (depth == 2) *(uint16_t *)pa = *(uint16_t *)pl = 0x0C0C;

            StoredCuData(depth)[(miCol & 7) + (miRow & 7) * 8] = *mi;
            As16B(m_contextsStored[depth].aboveNonzero[0]) = As16B(m_contexts.aboveNonzero[0]);
            As16B(m_contextsStored[depth].leftNonzero[0]) = As16B(m_contexts.leftNonzero[0]);
            As16B(m_contextsStored[depth].aboveAndLeftPartition) = As16B(m_contexts.aboveAndLeftPartition);
            m_costStored[depth] = m_costCurr;

            if ((depth == 2 && (mi->skip & 0x1) && (m_currFrame->m_pyramidLayer > 0 || !m_split64)) || (depth == 1 && (mi->skip & 0x1) && m_currFrame->m_pyramidLayer > 1)) {
                PropagateSubPart(mi, m_par->miPitch, 1 << (3 - depth), 1 << (3 - depth));
                return;
            }

            if (m_par->cuSplit == 2 && (mi->skip & 0x1) && (mi->mode != AV1_NEWMV) && m_par->BiPyramidLayers > 1 && (m_currFrame->m_pyramidLayer + depth) >= 3)
            {
                PropagateSubPart(mi, m_par->miPitch, 1 << (3 - depth), 1 << (3 - depth));
                return;
            }
            m_contexts = origCtx;
            m_costCurr = costInitial;
        } else {
            m_costStored[depth] = COST_MAX;
        }

        // in case the second, third or fourth parts are out of the picture
        mi[halfSize8].sbType = GetSbType(depth + 1, PARTITION_NONE);
        mi[halfSize8].mode = OUT_OF_PIC;
        mi[halfSize8 * m_par->miPitch] = mi[halfSize8];
        mi[halfSize8 * m_par->miPitch + halfSize8] = mi[halfSize8];

        ModeDecisionInterTu7<codecType, depth + 1>(miRow, miCol);
        if (left + halfSize < m_par->Width)
            ModeDecisionInterTu7<codecType, depth + 1>(miRow, miCol + halfSize8);
        if (top + halfSize < m_par->Height) {
            ModeDecisionInterTu7<codecType, depth + 1>(miRow + halfSize8, miCol);
            if (left + halfSize < m_par->Width)
                ModeDecisionInterTu7<codecType, depth + 1>(miRow + halfSize8, miCol + halfSize8);
        }
#if KERNEL_DEBUG
        m_costCurr += m_rdLambda * partitionBits[PARTITION_SPLIT];
#endif // KERNEL_DEBUG

        // keep best decision
        if (m_costStored[depth] <= m_costCurr) {
            *mi = StoredCuData(depth)[(miCol & 7) + (miRow & 7) * 8];
            PropagateSubPart(mi, m_par->miPitch, 1 << (3 - depth), 1 << (3 - depth));
            As16B(m_contexts.aboveNonzero[0]) = As16B(m_contextsStored[depth].aboveNonzero[0]);
            As16B(m_contexts.leftNonzero[0]) = As16B(m_contextsStored[depth].leftNonzero[0]);
            As16B(m_contexts.aboveAndLeftPartition) = As16B(m_contextsStored[depth].aboveAndLeftPartition);
            m_costCurr = m_costStored[depth];
        }
#ifdef JOIN_MI_INLOOP
        else {
            int32_t sbx = (miCol & 7) << 3;
            int32_t sby = (miRow & 7) << 3;
            ModeInfo *miO = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
            int32_t qsbtype = GetSbType(depth + 1, PARTITION_NONE);
            if (miO->sbType == qsbtype) {
                int join = 1;
                int jskip = miO->skip;
                int ref0 = miO->refIdx[0];
                int ref1 = miO->refIdx[1];
                int sad = miO->sad;
                sbx = ((miCol + halfSize8) & 7) << 3;
                sby = (miRow & 7) << 3;
                ModeInfo *miq = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
                if (miq->sbType == qsbtype) {
                    jskip &= miq->skip;
                    sad += miq->sad;
                    if (miq->mode != OUT_OF_PIC && miq->refIdx[0] == ref0 && miq->mv[0].asInt == miO->mv[0].asInt && miq->refIdx[1] == ref1) {
                        if (ref1 == -1)  join++;
                        else if (miq->mv[1].asInt == miO->mv[1].asInt) join++;
                    }

                    sbx = (miCol & 7) << 3;
                    sby = ((miRow + halfSize8) & 7) << 3;
                    miq = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
                    if (miq->sbType == qsbtype) {
                        jskip &= miq->skip;
                        sad += miq->sad;
                        if (miq->mode != OUT_OF_PIC && miq->refIdx[0] == ref0 && miq->mv[0].asInt == miO->mv[0].asInt && miq->refIdx[1] == ref1) {
                            if (ref1 == -1)  join++;
                            else if (miq->mv[1].asInt == miO->mv[1].asInt) join++;
                        }

                        sbx = ((miCol + halfSize8) & 7) << 3;
                        sby = ((miRow + halfSize8) & 7) << 3;
                        miq = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
                        if (miq->sbType == qsbtype) {
                            jskip &= miq->skip;
                            sad += miq->sad;
                            if (miq->mode != OUT_OF_PIC && miq->refIdx[0] == ref0 && miq->mv[0].asInt == miO->mv[0].asInt && miq->refIdx[1] == ref1) {
                                if (ref1 == -1)  join++;
                                else if (miq->mv[1].asInt == miO->mv[1].asInt) join++;
                            }
                        }
                    }
                }
                if (join == 4) {
                    miO->skip = jskip;
                    miO->sbType += 3;
                    miO->sad = sad;
                    PropagateSubPart(miO, m_par->miPitch, 1 << (3 - depth), 1 << (3 - depth));
                }
            }
        }
#endif
    }
    template void H265CU<uint8_t>::ModeDecisionInterTu7<CODEC_AV1, 0>(int32_t, int32_t);
    template void H265CU<uint8_t>::ModeDecisionInterTu7<CODEC_AV1, 1>(int32_t, int32_t);
    template void H265CU<uint8_t>::ModeDecisionInterTu7<CODEC_AV1, 2>(int32_t, int32_t);
    template void H265CU<uint16_t>::ModeDecisionInterTu7<CODEC_AV1, 0>(int32_t, int32_t);
    template void H265CU<uint16_t>::ModeDecisionInterTu7<CODEC_AV1, 1>(int32_t, int32_t);
    template void H265CU<uint16_t>::ModeDecisionInterTu7<CODEC_AV1, 2>(int32_t, int32_t);

    template <> template <> void H265CU<uint8_t>::ModeDecisionInterTu7<CODEC_AV1, 3>(int32_t miRow, int32_t miCol)
    {
        const int32_t abovePartition = m_contexts.abovePartition[miCol & 7] & 1;
        const int32_t leftPartition = m_contexts.leftPartition[miRow & 7] & 1;
        const int32_t partitionCtx =  2 * leftPartition + abovePartition;
        const uint32_t *partitionBits = m_currFrame->bitCount.av1Partition[3][partitionCtx];
        CostType costInitial = m_costCurr;
        MeCuNonRdAV1<3>(miRow, miCol);
#if KERNEL_DEBUG
        m_costCurr += m_rdLambda * partitionBits[PARTITION_NONE];
#endif // KERNEL_DEBUG
        m_contexts.abovePartition[miCol & 7] = m_contexts.leftPartition[miRow & 7] = 0x0E;
        debug_printf("%d,%d,%d cost=%f\n", 3, miRow, miCol, m_costCurr - costInitial);
    }
    template <> template <> void H265CU<uint16_t>::ModeDecisionInterTu7<CODEC_AV1, 3>(int32_t miRow, int32_t miCol)
    {
        const int32_t abovePartition = m_contexts.abovePartition[miCol & 7] & 1;
        const int32_t leftPartition = m_contexts.leftPartition[miRow & 7] & 1;
        const int32_t partitionCtx =  2 * leftPartition + abovePartition;
        const uint32_t *partitionBits = m_currFrame->bitCount.av1Partition[3][partitionCtx];
        MeCuNonRdAV1<3>(miRow, miCol);
#if KERNEL_DEBUG
        m_costCurr += m_rdLambda * partitionBits[PARTITION_NONE];
#endif // KERNEL_DEBUG
        m_contexts.abovePartition[miCol & 7] = m_contexts.leftPartition[miRow & 7] = 0x0E;
    }

    static inline int32_t HasNoSubpel(H265MV& mv)
    {
        const int32_t dx = (mv.mvx << 1) & 15;
        const int32_t dy = (mv.mvy << 1) & 15;

        if (dx ==0 && dy == 0) return 1;

        return 0;
    }
    static int32_t IsZ2Mode(int32_t mode)
    {
        if (mode == D135_PRED || mode == D117_PRED || mode == D153_PRED) {
            return 1;
        }

        return 0;
    }

    static int32_t IsSmoothVhMode(int32_t mode)
    {
        if (mode == SMOOTH_V_PRED || mode == SMOOTH_H_PRED) {
            return 1;
        }

        return 0;
    }

    int16_t clampMvRowAV1(int16_t mvy, int32_t border, BlockSize sbType, int32_t miRow, int32_t miRows) {
        int32_t bh = block_size_high_8x8[sbType];
        int32_t mbToTopEdge = -((miRow * MI_SIZE) * 8);
        int32_t mbToBottomEdge = ((miRows/* - bh*/ - miRow) * MI_SIZE) * 8;
        return (int16_t)Saturate(mbToTopEdge - border - bh * MI_SIZE * 8, mbToBottomEdge + border, mvy);
    }

    int16_t clampMvColAV1(int16_t mvx, int32_t border, BlockSize sbType, int32_t miCol, int32_t miCols) {
        int32_t bw = block_size_wide_8x8[sbType];
        int32_t mbToLeftEdge = -((miCol * MI_SIZE) * 8);
        int32_t mbToRightEdge = ((miCols/* - bw*/ - miCol) * MI_SIZE) * 8;
        return (int16_t)Saturate(mbToLeftEdge - border - bw * MI_SIZE * 8, mbToRightEdge + border, mvx);
    }

    static inline void ClampMvRefAV1(H265MV *mv, int32_t sbType, int32_t miRow, int32_t miCol, const H265VideoParam *m_par) {
        mv->mvy = clampMvRowAV1(mv->mvy, MV_BORDER_AV1, (BlockSize)sbType, miRow, m_par->miRows);
        mv->mvx = clampMvColAV1(mv->mvx, MV_BORDER_AV1, (BlockSize)sbType, miCol, m_par->miCols);
    }

    int32_t have_nearmv_in_inter_mode(PredMode mode) {
        return mode == AV1_NEARMV || mode == NEAR_NEARMV || mode == NEAR_NEWMV || mode == NEW_NEARMV;
    }

    int32_t have_newmv_in_inter_mode(PredMode mode)
    {
        return mode == AV1_NEWMV || mode == NEW_NEWMV ||
               mode == NEAREST_NEWMV || mode == NEW_NEARESTMV ||
               mode == NEAR_NEWMV || mode == NEW_NEARMV;
    }

    void CheckNewCompoundModes(ModeInfo *mi, MvRefs &m_mvRefs, const uint32_t *interModeBits, const uint32_t *nearMvDrlIdxBits, uint32_t &bestBitCost, int32_t miRow, int32_t miCol, const H265VideoParam *m_par)
    {
        H265MVPair bestRefMv;
        PredMode initMode = mi->mode;

        //  NEW_NEAREST
        if (mi->mv[1] == m_mvRefs.mvs[mi->refIdxComb][NEARESTMV_][1]) {
            uint32_t bitCost = interModeBits[NEW_NEARESTMV_];
            if (bestBitCost > bitCost) {
                bestBitCost = bitCost;
                bestRefMv = m_mvRefs.bestMv[mi->refIdxComb];
                mi->mode = NEW_NEARESTMV;
                mi->refMvIdx = 0;
            }
        }
        //  NEAREST_NEW
        if (mi->mv[0] == m_mvRefs.mvs[mi->refIdxComb][NEARESTMV_][0]) {
            uint32_t bitCost = interModeBits[NEAREST_NEWMV_];
            if (bestBitCost > bitCost) {
                bestBitCost = bitCost;
                bestRefMv = m_mvRefs.bestMv[mi->refIdxComb];
                mi->mode  = NEAREST_NEWMV;
                mi->refMvIdx = 0;
            }
        }
        //  NEAR0_NEW
        if (mi->mv[0] == m_mvRefs.mvs[mi->refIdxComb][NEARMV_][0]) {
            uint32_t bitCost = interModeBits[NEAR_NEWMV_] + nearMvDrlIdxBits[0];
            if (bestBitCost > bitCost) {
                bestBitCost = bitCost;
                bestRefMv = m_mvRefs.bestMv[mi->refIdxComb];
                mi->mode  = NEAR_NEWMV;
                mi->refMvIdx = 0;

                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 1) {
                    H265MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][1].mv;
                    ClampMvRefAV1(refMv + 0, mi->sbType, miRow, miCol, m_par);
                    ClampMvRefAV1(refMv + 1, mi->sbType, miRow, miCol, m_par);
                    bestRefMv = refMv;
                }
            }
        }
        //  NEW_NEAR0
        if (mi->mv[1] == m_mvRefs.mvs[mi->refIdxComb][NEARMV_][1]) {
            uint32_t bitCost = interModeBits[NEW_NEARMV_] + nearMvDrlIdxBits[0];
            if (bestBitCost > bitCost) {
                bestBitCost = bitCost;
                bestRefMv = m_mvRefs.bestMv[mi->refIdxComb];
                mi->mode  = NEW_NEARMV;
                mi->refMvIdx = 0;

                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 1) {
                    H265MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][1].mv;
                    ClampMvRefAV1(refMv + 0, mi->sbType, miRow, miCol, m_par);
                    ClampMvRefAV1(refMv + 1, mi->sbType, miRow, miCol, m_par);
                    bestRefMv = refMv;
                }
            }
        }
        //  NEW_NEAR1
        if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 2 && mi->mv[1] == m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv[1]) {
            uint32_t bitCost = interModeBits[NEW_NEARMV_] + nearMvDrlIdxBits[1];
            if (bestBitCost > bitCost) {
                bestBitCost = bitCost;
                bestRefMv = m_mvRefs.bestMv[mi->refIdxComb];
                mi->mode  = NEW_NEARMV;
                mi->refMvIdx = 1;
                {
                    H265MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv;
                    ClampMvRefAV1(refMv + 0, mi->sbType, miRow, miCol, m_par);
                    ClampMvRefAV1(refMv + 1, mi->sbType, miRow, miCol, m_par);
                    bestRefMv = refMv;
                }
            }
        }
        //  NEAR1_NEW
        if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 2 && mi->mv[0] == m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv[0]) {
            uint32_t bitCost = interModeBits[NEAR_NEWMV_] + nearMvDrlIdxBits[1];
            if (bestBitCost > bitCost) {
                bestBitCost = bitCost;
                bestRefMv = m_mvRefs.bestMv[mi->refIdxComb];
                mi->mode  = NEAR_NEWMV;
                mi->refMvIdx = 1;

                {
                    H265MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv;
                    ClampMvRefAV1(refMv + 0, mi->sbType, miRow, miCol, m_par);
                    ClampMvRefAV1(refMv + 1, mi->sbType, miRow, miCol, m_par);
                    bestRefMv = refMv;
                }
            }
        }

        //  NEW_NEAR2
        if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 3 && mi->mv[1] == m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][3].mv[1]) {
            uint32_t bitCost = interModeBits[NEW_NEARMV_] + nearMvDrlIdxBits[2];
            if (bestBitCost > bitCost) {
                bestBitCost = bitCost;
                bestRefMv = m_mvRefs.bestMv[mi->refIdxComb];
                mi->mode  = NEW_NEARMV;
                mi->refMvIdx = 2;
                {
                    H265MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][3].mv;
                    ClampMvRefAV1(refMv + 0, mi->sbType, miRow, miCol, m_par);
                    ClampMvRefAV1(refMv + 1, mi->sbType, miRow, miCol, m_par);
                    bestRefMv = refMv;
                }
            }
        }
        //  NEAR2_NEW
        if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 3 && mi->mv[0] == m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][3].mv[0]) {
            uint32_t bitCost = interModeBits[NEAR_NEWMV_] + nearMvDrlIdxBits[2];
            if (bestBitCost > bitCost) {
                bestBitCost = bitCost;
                bestRefMv = m_mvRefs.bestMv[mi->refIdxComb];
                mi->mode  = NEAR_NEWMV;
                mi->refMvIdx = 2;

                {
                    H265MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][3].mv;
                    ClampMvRefAV1(refMv + 0, mi->sbType, miRow, miCol, m_par);
                    ClampMvRefAV1(refMv + 1, mi->sbType, miRow, miCol, m_par);
                    bestRefMv = refMv;
                }
            }
        }

        if (mi->mode != initMode) {
            mi->mvd[0].mvx = mi->mv[0].mvx - bestRefMv[0].mvx;
            mi->mvd[0].mvy = mi->mv[0].mvy - bestRefMv[0].mvy;
            mi->mvd[1].mvx = mi->mv[1].mvx - bestRefMv[1].mvx;
            mi->mvd[1].mvy = mi->mv[1].mvy - bestRefMv[1].mvy;
        }
    }

    template <typename PixType>
    RdCost TransformInterYSbAv1_viaCoeffDecision(int32_t bsz, TxSize txSize, uint8_t *aboveNzCtx, uint8_t *leftNzCtx,
                             const PixType* src_, PixType *rec_, const int16_t *diff_, int16_t *coeff_,
                             int16_t *qcoeff_, const QuantParam &qpar, const BitCounts &bc, int32_t fastCoeffCost, int32_t qp)
    {
        const TxbBitCounts &cbc = bc.txb[ get_q_ctx(qp) ][txSize][PLANE_TYPE_Y];
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t num4x4h = 1 << log2h;
        const int32_t recPitch = 4 << log2w;
        const int32_t diffPitch = 4 << log2w;
        const int32_t width = 4 << txSize;
        const int32_t step = 1 << txSize;
        int32_t totalEob = 0;
        uint32_t coefBits = 0;
        int32_t totalSse = 0;
        __ALIGN64 CoeffsType coeffOrigin[32*32];
        const int32_t shiftTab[4] = {6, 6, 6, 4};
        const int32_t SHIFT_SSE = shiftTab[txSize];

        for (int32_t y = 0; y < num4x4h; y += step) {
            const PixType *src = src_ + (y << 2) * SRC_PITCH;
            const CoeffsType *diff = diff_ + (y << 2) * diffPitch;
            PixType *rec = rec_ + (y << 2) * recPitch;
            for (int32_t x = 0; x < num4x4w; x += step, rec += width, src += width, diff += width) {
                int32_t blockIdx = h265_scan_r2z4[y * 16 + x];
                int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
                CoeffsType *coeff  = coeff_ + offset;
                CoeffsType *qcoeff = qcoeff_ + offset;
                VP9PP::ftransform(diff, coeffOrigin, diffPitch, txSize, DCT_DCT);
                int32_t eob = VP9PP::quant(coeffOrigin, qcoeff, qpar, txSize);
                if (eob)
                    VP9PP::dequant(qcoeff, coeff, qpar, txSize);

                const int32_t txbSkipCtx = GetTxbSkipCtx(bsz, txSize, 0, aboveNzCtx+x, leftNzCtx+y);
                uint32_t bitsNz;
                int32_t sse;
                if (eob) {
                    int32_t culLevel;
                    bitsNz = EstimateCoefsAv1(cbc, txbSkipCtx, eob, qcoeff, &culLevel);
                    SetCulLevel(aboveNzCtx+x, leftNzCtx+y, culLevel, txSize);
                    sse = int32_t(VP9PP::sse_cont(coeffOrigin, coeff, width*width) >> SHIFT_SSE);
                } else {
                    bitsNz = (118*cbc.txbSkip[txbSkipCtx][1])>>7;
                    SetCulLevel(aboveNzCtx+x, leftNzCtx+y, 0, txSize);
                    //sse = VP9PP::sse(src, SRC_PITCH, rec, recPitch, txSize, txSize);
                    sse = VP9PP::sse_cont(coeffOrigin, qcoeff, width*width) >> SHIFT_SSE;
                }

                totalSse += sse;
                totalEob += eob;
                coefBits += bitsNz;
            }
        }
        RdCost rd;
        rd.sse = totalSse;//VP9PP::sse_p64_pw(src_, rec_, log2w, log2h);
        rd.coefBits = coefBits;
        rd.modeBits = 0;
        rd.eob = totalEob;
        return rd;
    }


    bool SatdSearch(const H265MVPair (&nonZeroMvp)[4][6+4+4], const H265MVPair mvs, int32_t *depth, int32_t *slot) {
        assert(mvs.asInt64 != 0); // zero mv predictors handled differently

        switch (*depth) {
        case 3:
            *depth = 2;
            if (nonZeroMvp[2][0].asInt64 == mvs.asInt64) return *slot = 0, true;
            if (nonZeroMvp[2][1].asInt64 == mvs.asInt64) return *slot = 1, true;
            if (nonZeroMvp[2][4].asInt64 == mvs.asInt64) return *slot = 4, true;
            if (nonZeroMvp[2][5].asInt64 == mvs.asInt64) return *slot = 5, true;
        case 2:
            *depth = 1;
            if (nonZeroMvp[1][0].asInt64 == mvs.asInt64) return *slot = 0, true;
            if (nonZeroMvp[1][1].asInt64 == mvs.asInt64) return *slot = 1, true;
            if (nonZeroMvp[1][4].asInt64 == mvs.asInt64) return *slot = 4, true;
            if (nonZeroMvp[1][5].asInt64 == mvs.asInt64) return *slot = 5, true;
        case 1:
            *depth = 0;
            if (nonZeroMvp[0][0].asInt64 == mvs.asInt64) return *slot = 0, true;
            if (nonZeroMvp[0][1].asInt64 == mvs.asInt64) return *slot = 1, true;
            if (nonZeroMvp[0][4].asInt64 == mvs.asInt64) return *slot = 4, true;
            if (nonZeroMvp[0][5].asInt64 == mvs.asInt64) return *slot = 5, true;
        default:
            return false;
        }

        //// 00111111 - for depth=3 predictors from depth 2,1 and 0 are available
        //// 00001111 - for depth=2 predictors from depth 1 and 0 are available
        //// 00000011 - for depth=1 predictors from depth 0 are available
        //// 00000000 - for depth=0 no predictors
        //const uint32_t depthmask = 63 >> ((3 - *depth) << 1);

        //__m128i mvp0 = loada_si128(&nonZeroMvp[0][0]);
        //__m128i mvp1 = loada_si128(&nonZeroMvp[1][0]);
        //__m128i mvp2 = loada_si128(&nonZeroMvp[2][0]);
        //__m128i mv = _mm_set1_epi64x(mvs.asInt64);
        //__m128i mask0_ = _mm_cmpeq_epi64(mvp0, mv);
        //__m128i mask1_ = _mm_cmpeq_epi64(mvp1, mv);
        //__m128i mask2_ = _mm_cmpeq_epi64(mvp2, mv);
        //uint32_t mask0 = _mm_movemask_pd(_mm_castsi128_pd(mask0_));
        //uint32_t mask1 = _mm_movemask_pd(_mm_castsi128_pd(mask1_));
        //uint32_t mask2 = _mm_movemask_pd(_mm_castsi128_pd(mask2_));
        //uint32_t mask = (mask0 | (mask1 << 2) | (mask2 << 4)) & depthmask;
        //if (mask) {
        //    int32_t bitpos = BSR(mask);
        //    *depth = bitpos >> 1;
        //    *slot = bitpos & 1;
        //    return true;
        //} else {
        //    return false;
        //}
    }

    template <int32_t depth> int32_t SatdUse(const int32_t *satds) {
        // the following switch in equivalent to this code:
        //    for (int32_t y = 0; y < sbw; y++, satds += pitch)
        //        for (int32_t x = 0; x < sbw; x++)
        //            satd += satds[x];
        const int32_t pitch = 64/8;
        if (depth == 3) {
            return satds[0];
        } else if (depth == 2) {
            __m128i sum = _mm_add_epi32(loadl_epi64(satds), loadl_epi64(satds + pitch));
            return _mm_cvtsi128_si32(sum) + _mm_cvtsi128_si32(_mm_srli_si128(sum,4));
        } else if (depth == 1) {
            __m128i sum = _mm_add_epi32(loada_si128(satds), loada_si128(satds + pitch));
            sum = _mm_add_epi32(sum, loada_si128(satds + 2 * pitch));
            sum = _mm_add_epi32(sum, loada_si128(satds + 3 * pitch));
            sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
            return _mm_cvtsi128_si32(sum) + _mm_cvtsi128_si32(_mm_srli_si128(sum, 4));
        } else {
            assert(0);
            return 0;
        }
    }

    template <int32_t depth, class PixType> int32_t SadSave(const PixType *src1, const PixType *src2, int32_t *satds)
    {
        if (depth == 3) {
            return VP9PP::sad_special(src1, 64, src2, 1, 1);
        }
        else if (depth == 2) {
            return VP9PP::sad_store8x8(src1, src2, satds, 2);
        }
        else if (depth == 1) {
            return VP9PP::sad_store8x8(src1, src2, satds, 3);
        }
        else if (depth == 0) {
            return VP9PP::sad_store8x8(src1, src2, satds, 4);
        }
        else {
            assert(0);
            return 0;
        }
    }

    template <int32_t depth, class PixType> int32_t HadSave(const PixType *src1, const PixType *src2, int32_t *satds)
    {
        // the following switch in equivalent to this code:
        //const int32_t sbw = 64 >> depth;
        //   if (sbw == 8) { // do not save SATD for 8x8 as it is already smallest block
        //       int32_t satd = VP9PP::satd_8x8(src1, src2);
        //       return (satd + 2) >> 2;
        //   } else {
        //       int32_t satdTotal = 0;
        //       for (int32_t y = 0; y < sbw; y += 8, src1 += 8*64, src2 += 8*64, satds += 8) {
        //           for (int32_t x = 0, k = 0; x < sbw; x += 16, k += 2) {
        //               VP9PP::satd_8x8_pair(src1 + x, src2 + x, satds + k);
        //               satds[k + 0] = (satds[k + 0] + 2) >> 2;
        //               satds[k + 1] = (satds[k + 1] + 2) >> 2;
        //               satdTotal += satds[k + 0];
        //               satdTotal += satds[k + 1];
        //           }
        //       }
        //       return satdTotal;
        //   }
#ifndef USE_SAD_ONLY
        int32_t satdTtotal = 0;
#endif
        if (depth == 3) {
#ifndef USE_SAD_ONLY

            return (VP9PP::satd_8x8(src1, src2) + 2) >> 2;
#else
            return VP9PP::sad_special(src1, 64, src2, 1, 1);
#endif
        } else if (depth == 2) {
#ifndef USE_SAD_ONLY
            VP9PP::satd_8x8_pair(src1, src2, satds);
            satds[0] = (satds[0] + 2) >> 2;
            satds[1] = (satds[1] + 2) >> 2;
            satdTtotal = satds[0] + satds[1];
            src1 += 8 * 64; src2 += 8 * 64; satds += 8;
            VP9PP::satd_8x8_pair(src1, src2, satds);
            satds[0] = (satds[0] + 2) >> 2;
            satds[1] = (satds[1] + 2) >> 2;
            return satdTtotal + satds[0] + satds[1];
#else
            return VP9PP::sad_store8x8(src1, src2, satds, 2);
#endif
        } else if (depth == 1) {
#ifndef USE_SAD_ONLY
            __m128i two = _mm_set1_epi32(2), satd4, sum;
            VP9PP::satd_8x8_pair(src1 + 0,  src2 + 0,  satds + 0);
            VP9PP::satd_8x8_pair(src1 + 16, src2 + 16, satds + 2);
            satd4 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds), two), 2);
            storea_si128(satds, satd4);
            sum = satd4;
            src1 += 8 * 64; src2 += 8 * 64; satds += 8;
            VP9PP::satd_8x8_pair(src1 + 0,  src2 + 0,  satds + 0);
            VP9PP::satd_8x8_pair(src1 + 16, src2 + 16, satds + 2);
            satd4 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds), two), 2);
            storea_si128(satds, satd4);
            sum = _mm_add_epi32(sum, satd4);
            src1 += 8 * 64; src2 += 8 * 64; satds += 8;
            VP9PP::satd_8x8_pair(src1 + 0,  src2 + 0,  satds + 0);
            VP9PP::satd_8x8_pair(src1 + 16, src2 + 16, satds + 2);
            satd4 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds), two), 2);
            storea_si128(satds, satd4);
            sum = _mm_add_epi32(sum, satd4);
            src1 += 8 * 64; src2 += 8 * 64; satds += 8;
            VP9PP::satd_8x8_pair(src1 + 0,  src2 + 0,  satds + 0);
            VP9PP::satd_8x8_pair(src1 + 16, src2 + 16, satds + 2);
            satd4 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds), two), 2);
            storea_si128(satds, satd4);
            sum = _mm_add_epi32(sum, satd4);
            sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
            return _mm_cvtsi128_si32(sum) + _mm_cvtsi128_si32(_mm_srli_si128(sum, 4));
#else
            return VP9PP::sad_store8x8(src1, src2, satds, 3);
#endif
        } else if (depth == 0) {
#ifndef USE_SAD_ONLY
            __m128i sum = _mm_setzero_si128(), two = _mm_set1_epi32(2), satd1, satd2;
            for (int32_t y = 0; y < 64; y += 8, src1 += 8 * 64, src2 += 8 * 64, satds += 8) {
                VP9PP::satd_8x8_pair(src1 + 0,  src2 + 0,  satds + 0);
                VP9PP::satd_8x8_pair(src1 + 16, src2 + 16, satds + 2);
                VP9PP::satd_8x8_pair(src1 + 32, src2 + 32, satds + 4);
                VP9PP::satd_8x8_pair(src1 + 48, src2 + 48, satds + 6);
                satd1 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds + 0), two), 2);
                satd2 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds + 4), two), 2);
                storea_si128(satds + 0, satd1);
                storea_si128(satds + 4, satd2);
                sum = _mm_add_epi32(sum, satd1);
                sum = _mm_add_epi32(sum, satd2);
            }
            sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
            return _mm_cvtsi128_si32(sum) + _mm_cvtsi128_si32(_mm_srli_si128(sum, 4));
#else
            return VP9PP::sad_store8x8(src1, src2, satds, 4);
#endif
        } else {
            assert(0);
            return 0;
        }
    }

    template <class PixType> int32_t MetricSave(const PixType *src1, const PixType *src2, int32_t depth, int32_t *satds, bool useHadamard) {
        if (depth == 3) return ((useHadamard) ? HadSave<3>(src1, src2, satds) : SadSave<3>(src1, src2, satds));
        if (depth == 2) return ((useHadamard) ? HadSave<2>(src1, src2, satds) : SadSave<2>(src1, src2, satds));
        if (depth == 1) return ((useHadamard) ? HadSave<1>(src1, src2, satds) : SadSave<1>(src1, src2, satds));
        if (depth == 0) return ((useHadamard) ? HadSave<0>(src1, src2, satds) : SadSave<0>(src1, src2, satds));
        assert(0);
        return 0;
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

    int32_t GetTxbSkipCtx(int32_t planeBsize, TxSize txSize, int32_t plane, const uint8_t *aboveCtx, const uint8_t *leftCtx)
    {
        if (plane == 0) {
            if (planeBsize == txsize_to_bsize[txSize]) {
                return 0;
            } else {
                const int32_t txbSize = 1 << txSize;

                int32_t top = 0;
                int32_t left = 0;
                for (int32_t k = 0; k < txbSize; k++) {
                    top |= aboveCtx[k];
                    left |= leftCtx[k];
                }

                top &= COEFF_CONTEXT_MASK;
                left &= COEFF_CONTEXT_MASK;

                const int32_t max = IPP_MIN(top | left, 4);
                const int32_t min = IPP_MIN(IPP_MIN(top, left), 4);
                return skip_contexts[min][max];
            }
        } else {
            const int32_t ctx_base = get_entropy_context(txSize, aboveCtx, leftCtx);
            const int32_t ctx_offset =
                (num_pels_log2_lookup[planeBsize] > num_pels_log2_lookup[txsize_to_bsize[txSize]]) ? 10 : 7;
            return ctx_base + ctx_offset;
        }
    }


    static PredMode GetModeFromIdx(int32_t modeAsIdx)
    {
        if (modeAsIdx == NEAREST_NEWMV_) return NEAREST_NEWMV;
        if (modeAsIdx == NEW_NEARESTMV_) return NEW_NEARESTMV;

        if (modeAsIdx == NEAR_NEWMV_ || modeAsIdx == NEAR_NEWMV1_ || modeAsIdx == NEAR_NEWMV2_) return NEAR_NEWMV;
        if (modeAsIdx == NEW_NEARMV_ || modeAsIdx == NEW_NEARMV1_ || modeAsIdx == NEW_NEARMV2_) return NEW_NEARMV;

        assert(!"invalid ext comp modes");

        return NEAREST_NEWMV;
    }

    template <typename PixType> template <int32_t depth>
    void H265CU<PixType>::MeCuNonRdAV1(int32_t miRow, int32_t miCol)
    {
        const BlockSize bsz = GetSbType(depth, PARTITION_NONE);
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t num4x4h = 1 << log2h;
        const int32_t sbx = (miCol & 7) << 3;
        const int32_t sby = (miRow & 7) << 3;
        const int32_t sbw = num4x4w << 2;
        const int32_t sbh = num4x4h << 2;
        const TxSize maxTxSize = IPP_MIN(TX_32X32, 4 - depth);
        const PixType *srcPic = m_ySrc + sbx + sby * SRC_PITCH;
        const QuantParam &qparam = m_par->qparamY[m_lumaQp];
        const BitCounts &bc = m_currFrame->bitCount;
        ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;

        int32_t rowStart = m_tileBorders.rowStart;

        const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, rowStart);
        const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
        if (above == m_availForPred.curr && left == m_availForPred.curr) {
            // txSize context is expected to be 1 for decisions made by MeCuNonRd (neighbours always have maxTxSize)
            //assert(GetCtxTxSizeAV1(above, left, maxTxSize) == 1);
        }
        const uint32_t *txSizeBits = bc.txSize[maxTxSize][1];
        const int32_t ctxSkip = GetCtxSkip(above, left);
        const int32_t ctxIsInter = GetCtxIsInter(above, left);
        const uint32_t *skipBits = bc.skip[ctxSkip];
        const uint32_t *isInterBits = bc.isInter[ctxIsInter];
        const uint32_t *txTypeBits = bc.interExtTx[maxTxSize];
        const uint32_t *nearMvDrlIdxBits[COMP_VAR0 + 1] = {};
        const int32_t refColocOffset = m_ctbPelX + sbx + (m_ctbPelY + sby) * m_pitchRecLuma;

        bool useHadamard = (m_par->hadamardMe >= 2);

        (m_currFrame->compoundReferenceAllowed)
            ? GetMvRefsAV1TU7B(bsz, miRow, miCol, &m_mvRefs, (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B), 1)
            : GetMvRefsAV1TU7P(bsz, miRow, miCol, &m_mvRefs, 1);

        EstimateRefFramesAllAv1(bc, *m_currFrame, above, left, m_mvRefs.refFrameBitCount, &m_mvRefs.memCtx);

        m_mvRefs.memCtx.skip = ctxSkip;
        m_mvRefs.memCtx.isInter = ctxIsInter;

        uint32_t interModeBitsAV1[5][/*INTER_MODES*/10+4];
        uint32_t *interModeBits = NULL;

        uint64_t bestInterCost = ULLONG_MAX;
        int32_t bestInterSatd = INT_MAX;
        int32_t bestInterBits = 0;
        H265MVPair bestMvs = {};
        __ALIGN2 RefIdx bestRef[2] = { INTRA_FRAME, NONE_FRAME };
        RefIdx bestRefComb = INTRA_FRAME;
        PredMode bestMode = AV1_NEARESTMV;
        int32_t bestDrlIdx = 0;
        PixType *bestPred = NULL;

        const PixType * const refColoc[3] = {
            reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_recon->y) + refColocOffset,
            reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_recon->y) + refColocOffset,
            reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_recon->y) + refColocOffset,
        };

        int32_t satds[5][6+4+4];
        PixType *preds[5][6+4+4] = {};
        int32_t slot, d;
        int32_t size = BSR(sbw | sbh) - 3; assert(sbw <=  (8 << size) && sbh <= (8 << size));
        // NEWMV
        H265MEInfo meInfo;
        meInfo.depth = depth;
        meInfo.width = sbw;
        meInfo.height = sbh;
        meInfo.posx = sbx;
        meInfo.posy = sby;

        // single-ref MVPs
        for (RefIdx refIdx = LAST_FRAME; refIdx <= ALTREF_FRAME; refIdx++) {
            if (!m_currFrame->isUniq[refIdx])
                continue;

            const H265MVPair (&refMvs)[3] = m_mvRefs.mvs[refIdx];
            const uint32_t refFrameBits = m_mvRefs.refFrameBitCount[refIdx];

            preds[refIdx][ZEROMV_] = m_interp[refIdx][depth][ZEROMV_] + sbx + sby * 64;
            satds[refIdx][ZEROMV_] = m_zeroMvpSatd[refIdx][depth][ (sbx + (sby << depth)) >> (6 - depth) ];

            const int32_t modeCtx = m_mvRefs.mvRefsAV1.interModeCtx[refIdx];
            const int32_t newMvCtx = modeCtx & NEWMV_CTX_MASK;
            const int32_t zeroMvCtx = (modeCtx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
            const int32_t refMvCtx = (modeCtx >> REFMV_OFFSET) & REFMV_CTX_MASK;
            interModeBitsAV1[refIdx][NEWMV_] = bc.newMvBit[newMvCtx][0];
            interModeBitsAV1[refIdx][ZEROMV_] = bc.newMvBit[newMvCtx][1] + bc.zeroMvBit[zeroMvCtx][0];
            interModeBitsAV1[refIdx][NEARESTMV_] = bc.newMvBit[newMvCtx][1] + bc.zeroMvBit[zeroMvCtx][1] + bc.refMvBit[refMvCtx][0];
            interModeBitsAV1[refIdx][NEARMV_] = bc.newMvBit[newMvCtx][1] + bc.zeroMvBit[zeroMvCtx][1] + bc.refMvBit[refMvCtx][1];
            interModeBits = interModeBitsAV1[refIdx];

            if (refMvs[NEARESTMV_][0].asInt == 0) {
                m_interp[refIdx][depth][NEARESTMV_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                preds[refIdx][NEARESTMV_] = preds[refIdx][ZEROMV_];
                satds[refIdx][NEARESTMV_] = satds[refIdx][ZEROMV_];
                satds[refIdx][NEARESTMV_] = INT_MAX;
            } else {
                slot = 6;
                d = depth;
                if (SatdSearch(m_nonZeroMvp[refIdx], refMvs[NEARESTMV_], &d, &slot)) { assert(d >= 0 && d < depth && (slot==0 || slot==1 || slot==4 || slot==5));
                    m_interp[refIdx][depth][NEARESTMV_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                    preds[refIdx][NEARESTMV_] = m_interp[refIdx][depth][NEARESTMV_] + sbx + sby * 64;
                    satds[refIdx][NEARESTMV_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                } else {
                    m_interp[refIdx][depth][NEARESTMV_] = vp9scratchpad.interpBufs[refIdx][depth][NEARESTMV_]; // interpolate to a new buffer
                    preds[refIdx][NEARESTMV_] = m_interp[refIdx][depth][NEARESTMV_] + sbx + sby * 64;
                    InterpolateAv1SingleRefLuma(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARESTMV_], refMvs[NEARESTMV_][0], sbh, log2w, DEF_FILTER_DUAL);
                    m_nonZeroMvp[refIdx][depth][NEARESTMV_] = refMvs[NEARESTMV_];
                    satds[refIdx][NEARESTMV_] = MetricSave(srcPic, preds[refIdx][NEARESTMV_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARESTMV_] + (sbx >> 3) + sby, useHadamard);
                }
            }

            if (refMvs[NEARMV_][0].asInt == 0) { // the only case when NEARMV==NEARESTMV is when NEARMV==ZEROMV
                m_interp[refIdx][depth][NEARMV_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                preds[refIdx][NEARMV_] = preds[refIdx][ZEROMV_];
                satds[refIdx][NEARMV_] = satds[refIdx][ZEROMV_];
                satds[refIdx][NEARMV_] = INT_MAX;
            } else {
                slot = 6;
                d = depth;
                if (SatdSearch(m_nonZeroMvp[refIdx], refMvs[NEARMV_], &d, &slot)) { assert(d >= 0 && d < depth && (slot==0 || slot==1 || slot==4 || slot==5));
                    m_interp[refIdx][depth][NEARMV_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                    preds[refIdx][NEARMV_] = m_interp[refIdx][depth][NEARMV_] + sbx + sby * 64;
                    satds[refIdx][NEARMV_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                } else {
                    m_interp[refIdx][depth][NEARMV_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV_]; // interpolate to a new buffer
                    preds[refIdx][NEARMV_] = m_interp[refIdx][depth][NEARMV_] + sbx + sby * 64;
                    InterpolateAv1SingleRefLuma(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARMV_], refMvs[NEARMV_][0], sbh, log2w, DEF_FILTER_DUAL);
                    m_nonZeroMvp[refIdx][depth][NEARMV_] = refMvs[NEARMV_];
                    satds[refIdx][NEARMV_] = MetricSave(srcPic, preds[refIdx][NEARMV_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV_] + (sbx >> 3) + sby, useHadamard);
                }
            }

            const int32_t nearestMvCount = MIN(4, m_mvRefs.mvRefsAV1.nearestMvCount[refIdx]);
            nearMvDrlIdxBits[refIdx] = bc.drlIdxNearMv[nearestMvCount];

            satds[refIdx][NEARMV1_] = INT_MAX;
            satds[refIdx][NEARMV2_] = INT_MAX;
            if (m_mvRefs.mvRefsAV1.refMvCount[refIdx] > 2) {
                H265MVPair refmv = m_mvRefs.mvRefsAV1.refMvStack[refIdx][2].mv;
                ClampMvRefAV1(&refmv[0], bsz, miRow, miCol, m_par);
                ClampMvRefAV1(&refmv[1], bsz, miRow, miCol, m_par);
                if (refmv[0].asInt == 0) {
                    m_interp[refIdx][depth][NEARMV1_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                    preds[refIdx][NEARMV1_] = preds[refIdx][ZEROMV_];
                    satds[refIdx][NEARMV1_] = satds[refIdx][ZEROMV_];
                    satds[refIdx][NEARMV1_] = INT_MAX;
                } else {
                    slot = 6;
                    d = depth;
                    if (SatdSearch(m_nonZeroMvp[refIdx], refmv, &d, &slot)) { assert(d >= 0 && d < depth && (slot==0 || slot==1 || slot==4 || slot==5));
                        m_interp[refIdx][depth][NEARMV1_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                        preds[refIdx][NEARMV1_] = m_interp[refIdx][depth][NEARMV1_] + sbx + sby * 64;
                        satds[refIdx][NEARMV1_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                    } else {
                        m_interp[refIdx][depth][NEARMV1_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV1_]; // interpolate to a new buffer
                        preds[refIdx][NEARMV1_] = m_interp[refIdx][depth][NEARMV1_] + sbx + sby * 64;
                        InterpolateAv1SingleRefLuma(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARMV1_], refmv[0], sbh, log2w, DEF_FILTER_DUAL);
                        m_nonZeroMvp[refIdx][depth][NEARMV1_] = refmv;
                        satds[refIdx][NEARMV1_] = MetricSave(srcPic, preds[refIdx][NEARMV1_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV1_] + (sbx >> 3) + sby, useHadamard);
                    }
                }

                if (m_mvRefs.mvRefsAV1.refMvCount[refIdx] > 3) {
                    H265MVPair refmv = m_mvRefs.mvRefsAV1.refMvStack[refIdx][3].mv;
                    ClampMvRefAV1(&refmv[0], bsz, miRow, miCol, m_par);
                    ClampMvRefAV1(&refmv[1], bsz, miRow, miCol, m_par);
                    if (refmv[0].asInt == 0) {
                        m_interp[refIdx][depth][NEARMV2_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                        preds[refIdx][NEARMV2_] = preds[refIdx][ZEROMV_];
                        satds[refIdx][NEARMV2_] = satds[refIdx][ZEROMV_];
                        satds[refIdx][NEARMV2_] = INT_MAX;
                    } else {
                        slot = 6;
                        d = depth;
                        if (SatdSearch(m_nonZeroMvp[refIdx], refmv, &d, &slot)) { assert(d >= 0 && d < depth && (slot==0 || slot==1 || slot==4 || slot==5));
                            m_interp[refIdx][depth][NEARMV2_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                            preds[refIdx][NEARMV2_] = m_interp[refIdx][depth][NEARMV2_] + sbx + sby * 64;
                            satds[refIdx][NEARMV2_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                        } else {
                            m_interp[refIdx][depth][NEARMV2_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV2_]; // interpolate to a new buffer
                            preds[refIdx][NEARMV2_] = m_interp[refIdx][depth][NEARMV2_] + sbx + sby * 64;
                            InterpolateAv1SingleRefLuma(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARMV2_], refmv[0], sbh, log2w, DEF_FILTER_DUAL);
                            m_nonZeroMvp[refIdx][depth][NEARMV2_] = refmv;
                            satds[refIdx][NEARMV2_] = MetricSave(srcPic, preds[refIdx][NEARMV2_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV2_] + (sbx >> 3) + sby, useHadamard);
                        }
                    }
                }
            }

            uint64_t cost;
            cost = RD(satds[refIdx][NEARESTMV_], refFrameBits + interModeBits[NEARESTMV_], m_rdLambdaSqrtInt);
#if !KERNEL_DEBUG
            cost = satds[refIdx][NEARESTMV_];
            if (depth < 3) cost = ULLONG_MAX;
#endif //!KERNEL_DEBUG
            if (bestInterCost > cost) {
                bestInterCost = cost;
                bestRef[0] = refIdx;
                bestMode = AV1_NEARESTMV;
                bestDrlIdx = 0;
            }
            cost = RD(satds[refIdx][NEARMV_], refFrameBits + interModeBits[NEARMV_] + nearMvDrlIdxBits[refIdx][0], m_rdLambdaSqrtInt);
#if !KERNEL_DEBUG
            cost = satds[refIdx][NEARMV_];
            if (depth < 3) cost = ULLONG_MAX;
#endif //!KERNEL_DEBUG
            if (bestInterCost > cost) {
                bestInterCost = cost;
                bestRef[0] = refIdx;
                bestMode = AV1_NEARMV;
                bestDrlIdx = 0;
            }
            cost = RD(satds[refIdx][NEARMV1_], refFrameBits + interModeBits[NEARMV_] + nearMvDrlIdxBits[refIdx][1], m_rdLambdaSqrtInt);
#if !KERNEL_DEBUG
            cost = satds[refIdx][NEARMV1_];
            if (depth < 3) cost = ULLONG_MAX;
#endif //!KERNEL_DEBUG
            if (bestInterCost > cost) {
                bestInterCost = cost;
                bestRef[0] = refIdx;
                bestMode = AV1_NEARMV;
                bestDrlIdx = 1;
            }
            cost = RD(satds[refIdx][NEARMV2_], refFrameBits + interModeBits[NEARMV_] + nearMvDrlIdxBits[refIdx][2], m_rdLambdaSqrtInt);
#if !KERNEL_DEBUG
            cost = satds[refIdx][NEARMV2_];
            if (depth < 3) cost = ULLONG_MAX;
#endif //!KERNEL_DEBUG
            if (bestInterCost > cost) {
                bestInterCost = cost;
                bestRef[0] = refIdx;
                bestMode = AV1_NEARMV;
                bestDrlIdx = 2;
            }

            cost = RD(satds[refIdx][ZEROMV_], refFrameBits + interModeBits[ZEROMV_], m_rdLambdaSqrtInt);
#if !KERNEL_DEBUG
            cost = satds[refIdx][ZEROMV_];
#endif // KERNEL_DEBUG
            if (bestInterCost > cost) {
                bestInterCost = cost;
                bestRef[0] = refIdx;
                bestMode = AV1_ZEROMV;
                bestDrlIdx = 0;
            }
        }
        bestRefComb = bestRef[0];

        // compound MVPs

        if (m_currFrame->compoundReferenceAllowed) {
            const RefIdx fixRef = (RefIdx)m_currFrame->compFixedRef;

            for (RefIdx refIdx = COMP_VAR0; refIdx <= COMP_VAR1; refIdx++) {
                const RefIdx varRef = (RefIdx)m_currFrame->compVarRef[refIdx - COMP_VAR0];
                if (!m_currFrame->isUniq[varRef])
                    continue;

                const H265MVPair (&refMvs)[3] = m_mvRefs.mvs[refIdx];
                const uint32_t refFrameBits = m_mvRefs.refFrameBitCount[refIdx];

                preds[refIdx][ZEROMV_] = m_interp[refIdx][depth][ZEROMV_] + sbx + sby * 64;
                satds[refIdx][ZEROMV_] = m_zeroMvpSatd[refIdx][depth][ (sbx + (sby << depth)) >> (6 - depth) ];

                const int32_t modeCtx = m_mvRefs.mvRefsAV1.interModeCtx[refIdx];
                interModeBitsAV1[refIdx][NEWMV_]     = bc.interCompMode[modeCtx][NEW_NEWMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][ZEROMV_]    = bc.interCompMode[modeCtx][ZERO_ZEROMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][NEARESTMV_] = bc.interCompMode[modeCtx][NEAREST_NEARESTMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][NEARMV_]    = bc.interCompMode[modeCtx][NEAR_NEARMV - NEAREST_NEARESTMV];

                interModeBitsAV1[refIdx][NEAREST_NEWMV_]    = bc.interCompMode[modeCtx][NEAREST_NEWMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][NEW_NEARESTMV_]    = bc.interCompMode[modeCtx][NEW_NEARESTMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][NEAR_NEWMV_]    = bc.interCompMode[modeCtx][NEAR_NEWMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][NEW_NEARMV_]    = bc.interCompMode[modeCtx][NEW_NEARMV - NEAREST_NEARESTMV];

                interModeBitsAV1[refIdx][NEAR_NEWMV1_]    = interModeBitsAV1[refIdx][NEAR_NEWMV_];
                interModeBitsAV1[refIdx][NEAR_NEWMV2_]    = interModeBitsAV1[refIdx][NEAR_NEWMV_];
                interModeBitsAV1[refIdx][NEW_NEARMV1_]    = interModeBitsAV1[refIdx][NEW_NEARMV_];
                interModeBitsAV1[refIdx][NEW_NEARMV2_]    = interModeBitsAV1[refIdx][NEW_NEARMV_];

                interModeBits = interModeBitsAV1[refIdx];

                if (refMvs[NEARESTMV_].asInt64 == 0) {
                    m_interp[refIdx][depth][NEARESTMV_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                    preds[refIdx][NEARESTMV_] = preds[refIdx][ZEROMV_];
                    satds[refIdx][NEARESTMV_] = satds[refIdx][ZEROMV_];
                    satds[refIdx][NEARESTMV_] = INT_MAX;
                }
                else {
                    int32_t slot = 6;
                    int32_t d = depth;
                    if (SatdSearch(m_nonZeroMvp[refIdx], refMvs[NEARESTMV_], &d, &slot)) {
                        assert(d >= 0 && d < depth && (slot==0 || slot==1 || slot==4 || slot==5));
                        m_interp[refIdx][depth][NEARESTMV_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                        preds[refIdx][NEARESTMV_] = m_interp[refIdx][depth][NEARESTMV_] + sbx + sby * 64;
                        satds[refIdx][NEARESTMV_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                    } else {
                        m_interp[refIdx][depth][NEARESTMV_] = vp9scratchpad.interpBufs[refIdx][depth][NEARESTMV_]; // interpolate to a new buffer
                        preds[refIdx][NEARESTMV_] = m_interp[refIdx][depth][NEARESTMV_] + sbx + sby * 64;

                        H265MV mv0single = m_mvRefs.mvs[varRef][NEARESTMV_][0];
                        H265MV mv1single = m_mvRefs.mvs[fixRef][NEARESTMV_][0];
                        H265MV mv0comp = m_mvRefs.mvs[refIdx][NEARESTMV_][0];
                        H265MV mv1comp = m_mvRefs.mvs[refIdx][NEARESTMV_][1];
                        if (mv0comp.asInt != mv0single.asInt && mv1comp.asInt != mv1single.asInt) {
                            ALIGNED(32) PixType tmp0[64*64], tmp1[64*64];
                            InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, tmp0, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
                            InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, tmp1, mv1comp, sbh, log2w, DEF_FILTER_DUAL);
                            VP9PP::average(tmp0, tmp1, preds[refIdx][NEARESTMV_], sbh, log2w);
                            //InterpolateAv1FirstRefLuma (refColoc[varRef], m_pitchRecLuma, vp9scratchpad.compPredIm, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
                            //InterpolateAv1SecondRefLuma(refColoc[fixRef], m_pitchRecLuma, vp9scratchpad.compPredIm, preds[refIdx][NEARESTMV_], mv1comp, sbh, log2w, DEF_FILTER_DUAL);
                        } else if (mv0comp.asInt != mv0single.asInt) {
                            PixType *pred0 = preds[refIdx][NEARESTMV_];
                            PixType *pred1 = m_interp[fixRef][depth][NEARESTMV_] + sbx + sby * 64;
                            InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, pred0, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
                            VP9PP::average(pred0, pred1, pred0, sbh, log2w);
                        } else if (mv1comp.asInt != mv1single.asInt) {
                            PixType *pred0 = m_interp[varRef][depth][NEARESTMV_] + sbx + sby * 64;
                            PixType *pred1 = preds[refIdx][NEARESTMV_];
                            InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, pred1, mv1comp, sbh, log2w, DEF_FILTER_DUAL);
                            VP9PP::average(pred0, pred1, pred1, sbh, log2w);
                        } else {
                            PixType *pred0 = m_interp[varRef][depth][NEARESTMV_] + sbx + sby * 64;
                            PixType *pred1 = m_interp[fixRef][depth][NEARESTMV_] + sbx + sby * 64;
                            VP9PP::average(pred0, pred1, preds[refIdx][NEARESTMV_], sbh, log2w); // average existing predictions
                        }
                        m_nonZeroMvp[refIdx][depth][NEARESTMV_] = refMvs[NEARESTMV_];
                        satds[refIdx][NEARESTMV_] = MetricSave(srcPic, preds[refIdx][NEARESTMV_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARESTMV_] + (sbx >> 3) + sby, useHadamard);
                    }
                }

                if (refMvs[NEARMV_].asInt64 == 0) {
                    m_interp[refIdx][depth][NEARMV_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                    preds[refIdx][NEARMV_] = preds[refIdx][ZEROMV_];
                    satds[refIdx][NEARMV_] = satds[refIdx][ZEROMV_];
                    satds[refIdx][NEARMV_] = INT_MAX;
                }
                else {
                    int32_t slot = 6;
                    int32_t d = depth;
                    if (SatdSearch(m_nonZeroMvp[refIdx], refMvs[NEARMV_], &d, &slot)) {
                        assert(d >= 0 && d < depth && (slot==0 || slot==1 || slot==4 || slot==5));
                        m_interp[refIdx][depth][NEARMV_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                        preds[refIdx][NEARMV_] = m_interp[refIdx][depth][NEARMV_] + sbx + sby * 64;
                        satds[refIdx][NEARMV_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                    } else {
                        m_interp[refIdx][depth][NEARMV_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV_]; // interpolate to a new buffer
                        preds[refIdx][NEARMV_] = m_interp[refIdx][depth][NEARMV_] + sbx + sby * 64;

                        H265MV mv0single = m_mvRefs.mvs[varRef][NEARMV_][0];
                        H265MV mv1single = m_mvRefs.mvs[fixRef][NEARMV_][0];
                        H265MV mv0comp = m_mvRefs.mvs[refIdx][NEARMV_][0];
                        H265MV mv1comp = m_mvRefs.mvs[refIdx][NEARMV_][1];
                        if (mv0comp.asInt != mv0single.asInt && mv1comp.asInt != mv1single.asInt) {
                            ALIGNED(32) PixType tmp0[64*64], tmp1[64*64];
                            InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, tmp0, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
                            InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, tmp1, mv1comp, sbh, log2w, DEF_FILTER_DUAL);
                            VP9PP::average(tmp0, tmp1, preds[refIdx][NEARMV_], sbh, log2w);
                            //InterpolateAv1FirstRefLuma (refColoc[varRef], m_pitchRecLuma, vp9scratchpad.compPredIm, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
                            //InterpolateAv1SecondRefLuma(refColoc[fixRef], m_pitchRecLuma, vp9scratchpad.compPredIm, preds[refIdx][NEARMV_], mv1comp, sbh, log2w, DEF_FILTER_DUAL);
                        } else if (mv0comp.asInt != mv0single.asInt) {
                            PixType *pred0 = preds[refIdx][NEARMV_];
                            PixType *pred1 = m_interp[fixRef][depth][NEARMV_] + sbx + sby * 64;
                            InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, pred0, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
                            VP9PP::average(pred0, pred1, pred0, sbh, log2w);
                        } else if (mv1comp.asInt != mv1single.asInt) {
                            PixType *pred0 = m_interp[varRef][depth][NEARMV_] + sbx + sby * 64;
                            PixType *pred1 = preds[refIdx][NEARMV_];
                            InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, pred1, mv1comp, sbh, log2w, DEF_FILTER_DUAL);
                            VP9PP::average(pred0, pred1, pred1, sbh, log2w);
                        } else {
                            PixType *pred0 = m_interp[varRef][depth][NEARMV_] + sbx + sby * 64;
                            PixType *pred1 = m_interp[fixRef][depth][NEARMV_] + sbx + sby * 64;
                            VP9PP::average(pred0, pred1, preds[refIdx][NEARMV_], sbh, log2w); // average existing predictions
                        }
                        m_nonZeroMvp[refIdx][depth][NEARMV_] = refMvs[NEARMV_];
                        satds[refIdx][NEARMV_] = MetricSave(srcPic, preds[refIdx][NEARMV_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV_] + (sbx >> 3) + sby, useHadamard);
                    }
                }

                const int32_t nearestMvCount = MIN(4, m_mvRefs.mvRefsAV1.nearestMvCount[refIdx]);
                nearMvDrlIdxBits[refIdx] = bc.drlIdxNearMv[nearestMvCount];

                satds[refIdx][NEARMV1_] = INT_MAX;
                satds[refIdx][NEARMV2_] = INT_MAX;
                if (m_mvRefs.mvRefsAV1.refMvCount[refIdx] > 2) {
                    const H265MVPair &refmv = m_mvRefs.mvRefsAV1.refMvStack[refIdx][2].mv;
                    if (refmv.asInt64 == 0) {
                        m_interp[refIdx][depth][NEARMV1_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                        preds[refIdx][NEARMV1_] = preds[refIdx][ZEROMV_];
                        satds[refIdx][NEARMV1_] = satds[refIdx][ZEROMV_];
                        satds[refIdx][NEARMV1_] = INT_MAX;
                    } else {
                        slot = 6;
                        d = depth;
                        if (SatdSearch(m_nonZeroMvp[refIdx], refmv, &d, &slot)) {
                            assert(d >= 0 && d < depth && (slot==0 || slot==1 || slot==4 || slot==5));
                            m_interp[refIdx][depth][NEARMV1_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                            preds[refIdx][NEARMV1_] = m_interp[refIdx][depth][NEARMV1_] + sbx + sby * 64;
                            satds[refIdx][NEARMV1_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                        } else {
                            m_interp[refIdx][depth][NEARMV1_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV1_]; // interpolate to a new buffer
                            preds[refIdx][NEARMV1_] = m_interp[refIdx][depth][NEARMV1_] + sbx + sby * 64;

                            const H265MVPair &mvComp = m_mvRefs.mvRefsAV1.refMvStack[refIdx][2].mv;
                            int32_t sameMv0 = 0;
                            int32_t sameMv1 = 0;
                            if (m_mvRefs.mvRefsAV1.refMvCount[varRef] > 2)
                                sameMv0 = (mvComp[0].asInt == m_mvRefs.mvRefsAV1.refMvStack[varRef][2].mv[0].asInt);
                            if (m_mvRefs.mvRefsAV1.refMvCount[fixRef] > 2)
                                sameMv1 = (mvComp[1].asInt == m_mvRefs.mvRefsAV1.refMvStack[fixRef][2].mv[0].asInt);

                            if (!sameMv0 && !sameMv1) {
                                ALIGNED(32) PixType tmp0[64*64], tmp1[64*64];
                                InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, tmp0, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                                InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, tmp1, mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
                                VP9PP::average(tmp0, tmp1, preds[refIdx][NEARMV1_], sbh, log2w);
                                //InterpolateAv1FirstRefLuma (refColoc[varRef], m_pitchRecLuma, vp9scratchpad.compPredIm, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                                //InterpolateAv1SecondRefLuma(refColoc[fixRef], m_pitchRecLuma, vp9scratchpad.compPredIm, preds[refIdx][NEARMV1_], mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
                            } else if (!sameMv0) {
                                PixType *pred0 = preds[refIdx][NEARMV1_];
                                PixType *pred1 = m_interp[fixRef][depth][NEARMV1_] + sbx + sby * 64;
                                InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, pred0, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                                VP9PP::average(pred0, pred1, pred0, sbh, log2w);
                            } else if (!sameMv1) {
                                PixType *pred0 = m_interp[varRef][depth][NEARMV1_] + sbx + sby * 64;
                                PixType *pred1 = preds[refIdx][NEARMV1_];
                                InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, pred1, mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
                                VP9PP::average(pred0, pred1, pred1, sbh, log2w);
                            } else {
                                PixType *pred0 = m_interp[varRef][depth][NEARMV1_] + sbx + sby * 64;
                                PixType *pred1 = m_interp[fixRef][depth][NEARMV1_] + sbx + sby * 64;
                                VP9PP::average(pred0, pred1, preds[refIdx][NEARMV1_], sbh, log2w); // average existing predictions
                            }
                            m_nonZeroMvp[refIdx][depth][NEARMV1_] = refmv;
                            satds[refIdx][NEARMV1_] = MetricSave(srcPic, preds[refIdx][NEARMV1_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV1_] + (sbx >> 3) + sby, useHadamard);
                        }
                    }

                    if (m_mvRefs.mvRefsAV1.refMvCount[refIdx] > 3) {
                        const H265MVPair &refmv = m_mvRefs.mvRefsAV1.refMvStack[refIdx][3].mv;
                        if (refmv.asInt64 == 0) {
                            m_interp[refIdx][depth][NEARMV2_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                            preds[refIdx][NEARMV2_] = preds[refIdx][ZEROMV_];
                            satds[refIdx][NEARMV2_] = satds[refIdx][ZEROMV_];
                            satds[refIdx][NEARMV2_] = INT_MAX;
                        } else {
                            slot = 6;
                            d = depth;
                            if (SatdSearch(m_nonZeroMvp[refIdx], refmv, &d, &slot)) {
                                assert(d >= 0 && d < depth && (slot==0 || slot==1 || slot==4 || slot==5));
                                m_interp[refIdx][depth][NEARMV2_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                                preds[refIdx][NEARMV2_] = m_interp[refIdx][depth][NEARMV2_] + sbx + sby * 64;
                                satds[refIdx][NEARMV2_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                            } else {
                                m_interp[refIdx][depth][NEARMV2_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV2_]; // interpolate to a new buffer
                                preds[refIdx][NEARMV2_] = m_interp[refIdx][depth][NEARMV2_] + sbx + sby * 64;

                                const H265MVPair &mvComp = m_mvRefs.mvRefsAV1.refMvStack[refIdx][3].mv;
                                int32_t sameMv0 = 0;
                                int32_t sameMv1 = 0;
                                if (m_mvRefs.mvRefsAV1.refMvCount[varRef] > 3)
                                    sameMv0 = (mvComp[0].asInt == m_mvRefs.mvRefsAV1.refMvStack[varRef][3].mv[0].asInt);
                                if (m_mvRefs.mvRefsAV1.refMvCount[fixRef] > 3)
                                    sameMv1 = (mvComp[1].asInt == m_mvRefs.mvRefsAV1.refMvStack[fixRef][3].mv[0].asInt);

                                if (!sameMv0 && !sameMv1) {
                                    ALIGNED(32) PixType tmp0[64*64], tmp1[64*64];
                                    InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, tmp0, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                                    InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, tmp1, mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
                                    VP9PP::average(tmp0, tmp1, preds[refIdx][NEARMV2_], sbh, log2w);
                                    //InterpolateAv1FirstRefLuma (refColoc[varRef], m_pitchRecLuma, vp9scratchpad.compPredIm, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                                    //InterpolateAv1SecondRefLuma(refColoc[fixRef], m_pitchRecLuma, vp9scratchpad.compPredIm, preds[refIdx][NEARMV2_], mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
                                } else if (!sameMv0) {
                                    PixType *pred0 = preds[refIdx][NEARMV2_];
                                    PixType *pred1 = m_interp[fixRef][depth][NEARMV2_] + sbx + sby * 64;
                                    InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, pred0, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                                    VP9PP::average(pred0, pred1, pred0, sbh, log2w);
                                } else if (!sameMv1) {
                                    PixType *pred0 = m_interp[varRef][depth][NEARMV2_] + sbx + sby * 64;
                                    PixType *pred1 = preds[refIdx][NEARMV2_];
                                    InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, pred1, mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
                                    VP9PP::average(pred0, pred1, pred1, sbh, log2w);
                                } else {
                                    PixType *pred0 = m_interp[varRef][depth][NEARMV2_] + sbx + sby * 64;
                                    PixType *pred1 = m_interp[fixRef][depth][NEARMV2_] + sbx + sby * 64;
                                    VP9PP::average(pred0, pred1, preds[refIdx][NEARMV2_], sbh, log2w); // average existing predictions
                                }
                                m_nonZeroMvp[refIdx][depth][NEARMV2_] = refmv;
                                satds[refIdx][NEARMV2_] = MetricSave(srcPic, preds[refIdx][NEARMV2_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV2_] + (sbx >> 3) + sby, useHadamard);
                            }
                        }
                    }
                }

                uint64_t cost = RD(satds[refIdx][NEARESTMV_], refFrameBits + interModeBits[NEARESTMV_], m_rdLambdaSqrtInt);
#if KERNEL_DEBUG
                if (bestInterCost > cost) {
                    bestInterCost = cost;
                    bestRefComb = refIdx;
                    bestRef[0] = varRef;
                    bestRef[1] = fixRef;
                    bestMode = AV1_NEARESTMV;
                    bestDrlIdx = 0;
                }
                cost = RD(satds[refIdx][NEARMV_], refFrameBits + interModeBits[NEARMV_] + nearMvDrlIdxBits[refIdx][0], m_rdLambdaSqrtInt);
                if (bestInterCost > cost) {
                    bestInterCost = cost;
                    bestRefComb = refIdx;
                    bestRef[0] = varRef;
                    bestRef[1] = fixRef;
                    bestMode = AV1_NEARMV;
                    bestDrlIdx = 0;
                }
                cost = RD(satds[refIdx][NEARMV1_], refFrameBits + interModeBits[NEARMV_] + nearMvDrlIdxBits[refIdx][1], m_rdLambdaSqrtInt);
                if (bestInterCost > cost) {
                    bestInterCost = cost;
                    bestRefComb = refIdx;
                    bestRef[0] = varRef;
                    bestRef[1] = fixRef;
                    bestMode = AV1_NEARMV;
                    bestDrlIdx = 1;
                }
                cost = RD(satds[refIdx][NEARMV2_], refFrameBits + interModeBits[NEARMV_] + nearMvDrlIdxBits[refIdx][2], m_rdLambdaSqrtInt);
                if (bestInterCost > cost) {
                    bestInterCost = cost;
                    bestRefComb = refIdx;
                    bestRef[0] = varRef;
                    bestRef[1] = fixRef;
                    bestMode = AV1_NEARMV;
                    bestDrlIdx = 2;
                }
#endif // KERNEL_DEBUG
                cost = RD(satds[refIdx][ZEROMV_], refFrameBits + interModeBits[ZEROMV_], m_rdLambdaSqrtInt);
#if !KERNEL_DEBUG
                cost = satds[refIdx][ZEROMV_];
#endif // KERNEL_DEBUG
                if (bestInterCost > cost) {
                    bestInterCost = cost;
                    bestRefComb = refIdx;
                    bestRef[0] = varRef;
                    bestRef[1] = fixRef;
                    bestMode = AV1_ZEROMV;
                    bestDrlIdx = 0;
                }
            }
        }

        bestInterBits = m_mvRefs.refFrameBitCount[bestRefComb] + interModeBitsAV1[bestRefComb][bestMode - AV1_NEARESTMV];
        if (bestMode == AV1_NEARMV)
            bestInterBits += nearMvDrlIdxBits[bestRefComb][bestDrlIdx];
        if (bestDrlIdx > 0) {
            assert(bestMode == AV1_NEARMV);
            bestInterSatd = satds[bestRefComb][NEARMV1_ + bestDrlIdx - 1];
            bestMvs.asInt64 = m_mvRefs.mvRefsAV1.refMvStack[bestRefComb][1 + bestDrlIdx].mv.asInt64;
            bestPred = preds[bestRefComb][NEARMV1_ + bestDrlIdx - 1];
        } else {
            bestInterSatd = satds[bestRefComb][bestMode - AV1_NEARESTMV];
            bestMvs.asInt64 = m_mvRefs.mvs[bestRefComb][bestMode - AV1_NEARESTMV].asInt64;
            bestPred = preds[bestRefComb][bestMode - AV1_NEARESTMV];
        }


        MePuGacc<depth>(&meInfo);

#ifndef USE_SAD_ONLY
        if (m_par->enableCmFlag && useHadamard) {
            meInfo.satd = VP9PP::satd(srcPic, m_interp[meInfo.refIdxComb][depth][NEWMV_] + sbx + sby * 64, log2w, log2h);
        }
#endif
        int32_t newMvDrlIdx = 0;
        H265MVPair bestRefMv = m_mvRefs.mvs[meInfo.refIdxComb][NEARESTMV_];

        const int32_t nearestMvCount = MIN(4, m_mvRefs.mvRefsAV1.nearestMvCount[meInfo.refIdxComb]);
        const uint32_t *newMvDrlIdxBits = bc.drlIdxNewMv[nearestMvCount];

        const int maxNumDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[meInfo.refIdxComb] - 1);
        const int numDrlBits = IPP_MIN(maxNumDrlBits, 1);

        meInfo.mvBits += (numDrlBits << 9);
        for (int32_t idx = 1; idx < 3; idx++) {
            if (m_mvRefs.mvRefsAV1.refMvCount[meInfo.refIdxComb] < idx + 1)
                break;
            H265MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[meInfo.refIdxComb][idx].mv;
            ClampMvRefAV1(refMv+0, bsz, sby >> 3, sbx >> 3, m_par);
            ClampMvRefAV1(refMv+1, bsz, sby >> 3, sbx >> 3, m_par);
            uint32_t mvBits = MvCost(meInfo.mv[0], refMv[0], m_mvRefs.useMvHp[ meInfo.refIdx[0] ]);
            if (meInfo.refIdx[1] != NONE_FRAME)
                mvBits += MvCost(meInfo.mv[1], refMv[1], m_mvRefs.useMvHp[ meInfo.refIdx[1] ]);
            const int numDrlBits = IPP_MIN(maxNumDrlBits, idx + 1);
            mvBits += (numDrlBits << 9);
            if (meInfo.mvBits > mvBits) {
                meInfo.mvBits = mvBits;
                newMvDrlIdx = idx;
                bestRefMv = refMv;
            }
        }

        interModeBits = interModeBitsAV1[meInfo.refIdxComb];

        const uint32_t modeBits = m_mvRefs.refFrameBitCount[meInfo.refIdxComb] + interModeBits[NEWMV_] + meInfo.mvBits;
#if KERNEL_DEBUG
        const uint64_t costNewMv = RD(meInfo.satd, modeBits, m_rdLambdaSqrtInt);
#else //KERNEL_DEBUG
        const uint64_t costNewMv = RD32(meInfo.satd,
                                      m_mvRefs.refFrameBitCount[meInfo.refIdxComb] + interModeBitsAV1[meInfo.refIdxComb][NEWMV_] + meInfo.mvBits,
                                      m_rdLambdaSqrtInt32);
#endif //KERNEL_DEBUG
#if KERNEL_DEBUG
        if (bestInterCost > costNewMv) {
#else //KERNEL_DEBUG
        bestInterCost = ULLONG_MAX;
        if (bestInterCost >= costNewMv) {
#endif //KERNEL_DEBUG
            bestInterCost = costNewMv;
            bestInterSatd = meInfo.satd;
            bestInterBits = m_mvRefs.refFrameBitCount[meInfo.refIdxComb] + interModeBitsAV1[meInfo.refIdxComb][NEWMV_] + meInfo.mvBits;
            bestMvs.asInt64 = meInfo.mv.asInt64;
            As2B(bestRef) = As2B(meInfo.refIdx);
            bestRefComb = meInfo.refIdxComb;
            bestMode = AV1_NEWMV;
            bestPred = m_interp[meInfo.refIdxComb][depth][NEWMV_] + sbx + sby * 64;
            bestDrlIdx = newMvDrlIdx;
        }

#if !KERNEL_DEBUG

        const int secondRef = m_currFrame->compoundReferenceAllowed ? ALTREF_FRAME : GOLDEN_FRAME;
        const int MODES_IDX[4] = { NEARESTMV_, NEARMV_, NEARMV1_, NEARMV2_ };

        uint32_t costRef0 = MAX_UINT, costRef1 = MAX_UINT, costComp;
        uint32_t sadRef0, sadRef1, sadComp;
        uint32_t bitsRef0, bitsRef1, bitsComp;
        int modeRef0 = -1, modeRef1 = -1;
        int checkedMv[3][4] = {};

        if (sbw <= 16) {
            if (m_currFrame->compoundReferenceAllowed) {
                int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[COMP_VAR0] - 2);
                for (int m = 0; m < 4; m++) {
                    if (m >= 2 && m >= m_mvRefs.mvRefsAV1.refMvCount[COMP_VAR0])
                        continue;
                    H265MVPair refmv = (m < 2) ? m_mvRefs.mvs[COMP_VAR0][m] : m_mvRefs.mvRefsAV1.refMvStack[COMP_VAR0][m].mv;
                    if (refmv.asInt64 == 0)
                        continue;
                    int numNearMvDrlBits = IPP_MIN(maxNumNearMvDrlBits, m);

                    if (refmv[0] == m_mvRefs.mvRefsAV1.refMvStack[LAST_FRAME][3].mv[0] && m_mvRefs.mvRefsAV1.refMvCount[LAST_FRAME] > 3) modeRef0 = 3, checkedMv[LAST_FRAME][3] = 1;
                    if (refmv[0] == m_mvRefs.mvRefsAV1.refMvStack[LAST_FRAME][2].mv[0] && m_mvRefs.mvRefsAV1.refMvCount[LAST_FRAME] > 2) modeRef0 = 2, checkedMv[LAST_FRAME][2] = 1;
                    if (refmv[0] == m_mvRefs.mvs[LAST_FRAME][1][0]) modeRef0 = 1, checkedMv[LAST_FRAME][1] = 1;
                    if (refmv[0] == m_mvRefs.mvs[LAST_FRAME][0][0]) modeRef0 = 0, checkedMv[LAST_FRAME][0] = 1;
                    if (refmv[1] == m_mvRefs.mvRefsAV1.refMvStack[ALTREF_FRAME][3].mv[0] && m_mvRefs.mvRefsAV1.refMvCount[ALTREF_FRAME] > 3) modeRef1 = 3, checkedMv[ALTREF_FRAME][3] = 1;
                    if (refmv[1] == m_mvRefs.mvRefsAV1.refMvStack[ALTREF_FRAME][2].mv[0] && m_mvRefs.mvRefsAV1.refMvCount[ALTREF_FRAME] > 2) modeRef1 = 2, checkedMv[ALTREF_FRAME][2] = 1;
                    if (refmv[1] == m_mvRefs.mvs[ALTREF_FRAME][1][0]) modeRef1 = 1, checkedMv[ALTREF_FRAME][1] = 1;
                    if (refmv[1] == m_mvRefs.mvs[ALTREF_FRAME][0][0]) modeRef1 = 0, checkedMv[ALTREF_FRAME][0] = 1;

                    if (modeRef0 >= 0) {
                        const int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[LAST_FRAME] - 2);
                        const int numNearMvDrlBits = IPP_MIN(maxNumNearMvDrlBits, modeRef0);
                        sadRef0   = refmv[0].asInt ? satds[LAST_FRAME][ MODES_IDX[modeRef0] ] : satds[LAST_FRAME][ZEROMV_];
                        bitsRef0  = m_mvRefs.refFrameBitCount[LAST_FRAME];
                        bitsRef0 += interModeBitsAV1[LAST_FRAME][modeRef0 == 0 ? NEARESTMV_ : NEARMV_];
                        bitsRef0 += numNearMvDrlBits << 9;
                        costRef0 = RD32(sadRef0, bitsRef0, m_rdLambdaSqrtInt32);
                        checkedMv[LAST_FRAME][modeRef0] = 1;
                    }

                    if (modeRef1 >= 0) {
                        const int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[ALTREF_FRAME] - 2);
                        const int numNearMvDrlBits = IPP_MIN(maxNumNearMvDrlBits, modeRef1);
                        sadRef1   = refmv[1].asInt ? satds[ALTREF_FRAME][ MODES_IDX[modeRef1] ] : satds[ALTREF_FRAME][ZEROMV_];
                        bitsRef1  = m_mvRefs.refFrameBitCount[ALTREF_FRAME];
                        bitsRef1 += interModeBitsAV1[ALTREF_FRAME][modeRef1 == 0 ? NEARESTMV_ : NEARMV_];
                        bitsRef1 += numNearMvDrlBits << 9;
                        costRef1 = RD32(sadRef1, bitsRef1, m_rdLambdaSqrtInt32);
                        checkedMv[ALTREF_FRAME][modeRef1] = 1;
                    }

                    sadComp   = satds[COMP_VAR0][MODES_IDX[m]];
                    bitsComp  = m_mvRefs.refFrameBitCount[COMP_VAR0];
                    bitsComp += interModeBitsAV1[COMP_VAR0][m == 0 ? NEARESTMV_ : NEARMV_];
                    bitsComp += numNearMvDrlBits << 9;
                    costComp = RD32(sadComp, bitsComp, m_rdLambdaSqrtInt32);

                    if (bestInterCost > costComp) {
                        bestInterCost = costComp;
                        bestInterSatd = sadComp;
                        bestInterBits = bitsComp;
                        bestRefComb = COMP_VAR0;
                        bestRef[0] = LAST_FRAME;
                        bestRef[1] = ALTREF_FRAME;
                        bestMode = AV1_NEARESTMV;
                        bestMvs = refmv;
                        bestPred = preds[bestRefComb][MODES_IDX[m]];
                    }

                    if (bestInterCost > costRef0) {
                        bestInterCost = costRef0;
                        bestInterSatd = sadRef0;
                        bestInterBits = bitsRef0;
                        bestRefComb = LAST_FRAME;
                        bestRef[0] = LAST_FRAME;
                        bestRef[1] = NONE_FRAME;
                        bestMode = AV1_NEARESTMV;
                        bestMvs[0] = refmv[0];
                        bestMvs[1].asInt = 0;
                        bestPred = preds[bestRefComb][MODES_IDX[modeRef0]];
                    }

                    if (bestInterCost > costRef1) {
                        bestInterCost = costRef1;
                        bestInterSatd = sadRef1;
                        bestInterBits = bitsRef1;
                        bestRefComb = ALTREF_FRAME;
                        bestRef[0] = ALTREF_FRAME;
                        bestRef[1] = NONE_FRAME;
                        bestMode = AV1_NEARESTMV;
                        bestMvs[0] = refmv[1];
                        bestMvs[1].asInt = 0;
                        bestPred = preds[bestRefComb][MODES_IDX[modeRef1]];
                    }
                }
            }

            const int REFS[3] = { LAST_FRAME, secondRef };
            for (int r = 0; r < 2; r++) {
                const int ref = REFS[r];
                for (int m = 0; m < 4; m++) {
                    const int mode_idx = MODES_IDX[m];
                    if (satds[ref][mode_idx] == INT_MAX)
                        continue;
                    if (checkedMv[ref][m])
                        continue;

                    const int maxNumDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[ref] - 2);
                    const int numDrlBits = IPP_MIN(maxNumDrlBits, m);
                    uint32_t bits = m_mvRefs.refFrameBitCount[ref]
                                + interModeBitsAV1[ref][m == 0 ? NEARESTMV_ : NEARMV_]
                                + (numDrlBits << 9);

                    uint64_t cost = RD32(satds[ref][mode_idx], bits, m_rdLambdaSqrtInt32);
                    if (bestInterCost > cost) {
                        bestInterCost = cost;
                        bestInterSatd = satds[ref][mode_idx];
                        bestInterBits = bits;
                        bestRefComb = ref;
                        bestRef[0] = (r == 1) ? secondRef : LAST_FRAME;
                        bestRef[1] = (r == 2) ? ALTREF_FRAME : NONE_FRAME;
                        bestMode = AV1_NEARESTMV;
                        bestDrlIdx = (m == 0) ? 0 : m - 1;
                        bestMvs.asInt64 = m < 2 ? m_mvRefs.mvs[bestRefComb][m].asInt64 : m_mvRefs.mvRefsAV1.refMvStack[bestRefComb][m].mv.asInt64;
                        bestPred = preds[bestRefComb][mode_idx];
                    }
                }
            }
        } else {
            const int REFS[3] = { LAST_FRAME, secondRef, COMP_VAR0 };
            const int MODES_IDX[4] = { NEARESTMV_, NEARMV_, NEARMV1_, NEARMV2_ };
            for (int r = 0; r < (2 + m_currFrame->compoundReferenceAllowed); r++) {
                const int ref = REFS[r];
                for (int m = 0; m < 4; m++) {
                    const int mode_idx = MODES_IDX[m];
                    if (satds[ref][mode_idx] == INT_MAX)
                        continue;
                    const int maxNumDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[ref] - 2);
                    const int numDrlBits = IPP_MIN(maxNumDrlBits, m);
                    uint64_t cost = RD32(satds[ref][mode_idx],
                                        m_mvRefs.refFrameBitCount[ref]
                                        + interModeBitsAV1[ref][m == 0 ? NEARESTMV_ : NEARMV_]
                                        + (numDrlBits << 9),
                                        m_rdLambdaSqrtInt32);
                    if (bestInterCost > cost) {
                        bestInterCost = cost;
                        bestInterSatd = satds[ref][mode_idx];
                        bestInterBits = m_mvRefs.refFrameBitCount[ref]
                                      + interModeBitsAV1[ref][m == 0 ? NEARESTMV_ : NEARMV_]
                                      + (numDrlBits << 9);
                        bestRefComb = ref;
                        bestRef[0] = (r == 1) ? secondRef : LAST_FRAME;
                        bestRef[1] = (r == 2) ? ALTREF_FRAME : NONE_FRAME;
                        bestMode = AV1_NEARESTMV;
                        bestDrlIdx = (m == 0) ? 0 : m - 1;
                        bestMvs.asInt64 = m < 2 ? m_mvRefs.mvs[bestRefComb][m].asInt64 : m_mvRefs.mvRefsAV1.refMvStack[bestRefComb][m].mv.asInt64;
                        bestPred = preds[bestRefComb][mode_idx];
                    }
                }
            }
        }

        int32_t bestZeroRef = LAST_FRAME;
        uint64_t bestZeroCost = satds[LAST_FRAME][ZEROMV_];
        if (bestZeroCost > satds[secondRef][ZEROMV_]) {
            bestZeroCost = satds[secondRef][ZEROMV_];
            bestZeroRef = secondRef;
        }
        if (m_currFrame->compoundReferenceAllowed && bestZeroCost > satds[COMP_VAR0][ZEROMV_]) {
            bestZeroCost = satds[COMP_VAR0][ZEROMV_];
            bestZeroRef = COMP_VAR0;
        }
        uint64_t costZ = RD32(bestZeroCost, m_mvRefs.refFrameBitCount[bestZeroRef] + interModeBitsAV1[bestZeroRef][ZEROMV_], m_rdLambdaSqrtInt32);
        if (bestInterCost > costZ) {
            bestInterCost = costZ;
            bestInterSatd = bestZeroCost;
            bestInterBits = m_mvRefs.refFrameBitCount[bestZeroRef] + interModeBitsAV1[bestZeroRef][ZEROMV_];
            bestRefComb = bestZeroRef;
            bestRef[0] = (bestZeroRef == COMP_VAR0) ? LAST_FRAME : bestZeroRef;
            bestRef[1] = (bestZeroRef == COMP_VAR0) ? ALTREF_FRAME : NONE_FRAME;
            bestMode = AV1_ZEROMV;
            bestDrlIdx = 0;
            bestMvs.asInt64 = 0;
            bestPred = preds[bestRefComb][ZEROMV_];
        }
#endif // !KERNEL_DEBUG

        // add experiment with EXT_REF_MODES (non active now)
        // Q+0.2%, S down due to extra interpolation.
        // will be ON after enabling caching approach + GPU kernels update

        const int32_t ctxInterp = GetCtxInterpBothAV1Fast(above, left, bestRef);
        bestInterBits += bc.interpFilterAV1[ctxInterp & 15][DEF_FILTER];
        bestInterBits += bc.interpFilterAV1[ctxInterp >> 8][DEF_FILTER];
        bestInterBits += isInterBits[1];

        m_bestInterp[depth][miRow & 7][miCol & 7] = bestPred;
        //m_bestInterSatd[depth][miRow & 7][miCol & 7] = bestInterSatd;

        // skip cost
        int32_t sseZ = VP9PP::sse_p64_p64(srcPic, bestPred, log2w, log2w);

        CostType bestCost = sseZ + m_rdLambda * (bestInterBits + skipBits[1]);
#if !KERNEL_DEBUG
        if (bestMode == AV1_NEWMV) {
            const PixType *p = (const PixType *)m_currFrame->m_feiInterp[3-depth]->m_sysmem;
            const int32_t pitch = m_currFrame->m_feiInterp[3-depth]->m_pitch;
            p += (m_ctbPelY + sby) * pitch + (m_ctbPelX + sbx);
            sseZ = VP9PP::sse(srcPic, 64, p, pitch, log2w, log2w);
        }
        bestCost = sseZ + uint32_t(m_rdLambda * (bestInterBits + skipBits[1]) + 0.5f);
#endif // KERNEL_DEBUG
        int32_t bestSkip = 1;
        int32_t bestEob = 0;

        // non-skip cost
        RdCost rd;
        rd.modeBits = 0;
        //const int16_t *scan = default_scan[maxTxSize];
        PixType *rec = vp9scratchpad.recY[0];
        int16_t *diff = vp9scratchpad.diffY;
        int16_t *coef = vp9scratchpad.coefY;
        int16_t *qcoef = vp9scratchpad.qcoefY[0];
        uint8_t *anz = m_contexts.aboveNonzero[0] + (sbx >> 2);
        uint8_t *lnz = m_contexts.leftNonzero[0] + (sby >> 2);
        CopyNxN(bestPred, PRED_PITCH, rec, sbw);
        VP9PP::diff_nxn(srcPic, bestPred, diff, log2w);
#if !KERNEL_DEBUG
        if (bestMode == AV1_NEWMV) {
            const PixType *p = (const PixType *)m_currFrame->m_feiInterp[3-depth]->m_sysmem;
            const int32_t pitch = m_currFrame->m_feiInterp[3-depth]->m_pitch;
            p += (m_ctbPelY + sby) * pitch + (m_ctbPelX + sbx);
            CopyNxN(p, pitch, rec, sbw);
            VP9PP::diff_nxn(srcPic, 64, p, pitch, diff, sbw, log2w);
        }
#endif // !KERNEL_DEBUG

        QuantParam qparamAlt = qparam;
        qparamAlt.round[0] = (40 * qparam.dequant[0]) >> 7;
        qparamAlt.round[1] = (40 * qparam.dequant[1]) >> 7;
        rd = TransformInterYSbAv1_viaCoeffDecision(bsz, TX_8X8, anz, lnz, srcPic, rec, diff, coef, qcoef, qparamAlt, bc,
                                                   m_par->cuSplit == 2 ? 0 : 1, m_currFrame->m_sliceQpY);

        // decide between skip/nonskip
        CostType cost = rd.sse + m_rdLambda * (bestInterBits + skipBits[0] + txSizeBits[maxTxSize] + txTypeBits[DCT_DCT] + rd.coefBits);

        //cost = rd.sse + uint32_t(m_rdLambda * (bestInterBits + skipBits[0] + txSizeBits[maxTxSize] + txTypeBits[DCT_DCT] + rd.coefBits) + 0.5f);
        cost = rd.sse + uint32_t(m_rdLambda * (bestInterBits + skipBits[0] + txSizeBits[maxTxSize] + txTypeBits[DCT_DCT] + ((rd.coefBits * 3) >> 2)) + 0.5f);
        if (bestCost > cost) {
            //bestCost = cost;
            bestCost = rd.sse + uint32_t(m_rdLambda * (bestInterBits + skipBits[0] + txSizeBits[maxTxSize] + txTypeBits[DCT_DCT] + rd.coefBits) + 0.5f);
            bestSkip = 0;
            bestEob = rd.eob;
        } else {
            small_memset0(anz, num4x4w);
            small_memset0(lnz, num4x4w);
        }

        Zero(*mi);
        mi->mv.asInt64 = bestMvs.asInt64;
        mi->mvd[0].mvx = bestMvs[0].mvx - bestRefMv[0].mvx;
        mi->mvd[0].mvy = bestMvs[0].mvy - bestRefMv[0].mvy;
        mi->mvd[1].mvx = bestMvs[1].mvx - bestRefMv[1].mvx;
        mi->mvd[1].mvy = bestMvs[1].mvy - bestRefMv[1].mvy;
        As2B(mi->refIdx) = As2B(bestRef);
        mi->refIdxComb = bestRefComb;
        mi->sbType = bsz;
        mi->skip = uint8_t(bestSkip | ((bestEob == 0) ? 2 : 0));
        mi->txSize = maxTxSize;
        mi->mode = bestMode;
        mi->refMvIdx = bestDrlIdx;
        mi->sad = bestInterSatd << 11;

        m_costCurr += bestCost;
    }


    inline int32_t GetRefIdxComb(RefIdx refIdx0, RefIdx refIdx1, int32_t compVarRef1) {
        return refIdx1 == NONE_FRAME ? refIdx0 : COMP_VAR0 + (refIdx1 == compVarRef1);
    }
    inline int32_t GetRefIdxComb(const RefIdx refIdxs[2], int32_t compVarRef1) {
        return GetRefIdxComb(refIdxs[0], refIdxs[1], compVarRef1);
    }

    struct MeData {
        uint64_t cost;
        H265MVPair mv;
        RefIdx refIdx[2];
        int32_t dist;
        int32_t mvBits;
        int32_t otherBits;
    };
    uint64_t CalcMeCost(const MeData &meData, uint64_t lambda) {
        return RD(meData.dist, meData.mvBits + meData.otherBits, lambda);
    }

    static const int16_t DXDY1[9][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0}, {0,  0}, {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    };
    static const int16_t DXDY2[9][2] = {
        {-2, -2}, {0, -2}, {2, -2},
        {-2,  0}, {0,  0}, {2,  0},
        {-2,  2}, {0,  2}, {2,  2}
    };

    template <typename PixType> template <int32_t depth>
    void H265CU<PixType>::MePuGacc(H265MEInfo *meInfo)
    {
        int32_t satds[5];
        satds[0] = INT_MAX;

        if (depth == 3) {
            const int32_t numBlk = meInfo->posy + (meInfo->posx >> 3);
            const RefIdx *bestRefIdx = m_newMvRefIdx8[numBlk];
            const H265MVPair &bestMv = m_newMvFromGpu8[numBlk];
            As2B(meInfo->refIdx) = As2B(bestRefIdx);
            meInfo->mv.asInt64 = bestMv.asInt64;
            meInfo->refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            meInfo->satd = m_newMvSatd8[numBlk];
            meInfo->mvBits = MvCost(bestMv[0], m_mvRefs.bestMv[meInfo->refIdxComb][0], 0);
            if (bestRefIdx[1] != NONE_FRAME)
                meInfo->mvBits += MvCost(bestMv[1], m_mvRefs.bestMv[meInfo->refIdxComb][1], 0);
            m_interp[0][depth][NEWMV_] = NULL;
            m_interp[1][depth][NEWMV_] = NULL;
            m_interp[2][depth][NEWMV_] = NULL;
            m_interp[3][depth][NEWMV_] = NULL;
            m_interp[4][depth][NEWMV_] = NULL;
            m_interp[meInfo->refIdxComb][depth][NEWMV_] = m_newMvInterp8;

        } else if (depth == 2) {
            const int32_t numBlk = (meInfo->posy >> 2) + (meInfo->posx >> 4);
            const RefIdx *bestRefIdx = m_newMvRefIdx16[numBlk];
            const H265MVPair &bestMv = m_newMvFromGpu16[numBlk];
            As2B(meInfo->refIdx) = As2B(bestRefIdx);
            meInfo->mv.asInt64 = bestMv.asInt64;
            meInfo->refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            meInfo->satd = m_newMvSatd16[numBlk];
            meInfo->mvBits = MvCost(bestMv[0], m_mvRefs.bestMv[meInfo->refIdxComb][0], 0);
            if (bestRefIdx[1] != NONE_FRAME)
                meInfo->mvBits += MvCost(bestMv[1], m_mvRefs.bestMv[meInfo->refIdxComb][1], 0);
            m_interp[0][depth][NEWMV_] = NULL;
            m_interp[1][depth][NEWMV_] = NULL;
            m_interp[2][depth][NEWMV_] = NULL;
            m_interp[3][depth][NEWMV_] = NULL;
            m_interp[4][depth][NEWMV_] = NULL;
            m_interp[meInfo->refIdxComb][depth][NEWMV_] = m_newMvInterp16;

        } else if (depth == 1) {
            const int32_t numBlk = (meInfo->posy >> 5)*2 + (meInfo->posx >> 5);
            const RefIdx *bestRefIdx = m_newMvRefIdx32[numBlk];
            const H265MVPair &bestMv = m_newMvFromGpu32[numBlk];
            As2B(meInfo->refIdx) = As2B(bestRefIdx);
            meInfo->mv.asInt64 = bestMv.asInt64;
            meInfo->refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            meInfo->satd = m_newMvSatd32[numBlk];
            meInfo->mvBits = MvCost(bestMv[0], m_mvRefs.bestMv[meInfo->refIdxComb][0], 0);
            if (bestRefIdx[1] != NONE_FRAME)
                meInfo->mvBits += MvCost(bestMv[1], m_mvRefs.bestMv[meInfo->refIdxComb][1], 0);
            m_interp[0][depth][NEWMV_] = NULL;
            m_interp[1][depth][NEWMV_] = NULL;
            m_interp[2][depth][NEWMV_] = NULL;
            m_interp[3][depth][NEWMV_] = NULL;
            m_interp[4][depth][NEWMV_] = NULL;
            m_interp[meInfo->refIdxComb][depth][NEWMV_] = m_newMvInterp32;
        }
#if 1
        else if (depth == 0) {
            const int32_t numBlk = (meInfo->posy >> 6) + (meInfo->posx >> 6);
            const RefIdx *bestRefIdx = m_newMvRefIdx64[numBlk];
            const H265MVPair &bestMv = m_newMvFromGpu64[numBlk];
            As2B(meInfo->refIdx) = As2B(bestRefIdx);
            meInfo->mv.asInt64 = bestMv.asInt64;
            meInfo->refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            meInfo->satd = m_newMvSatd64[numBlk];
            meInfo->mvBits = MvCost(bestMv[0], m_mvRefs.bestMv[meInfo->refIdxComb][0], 0);
            if (bestRefIdx[1] != NONE_FRAME)
                meInfo->mvBits += MvCost(bestMv[1], m_mvRefs.bestMv[meInfo->refIdxComb][1], 0);
            m_interp[0][depth][NEWMV_] = NULL;
            m_interp[1][depth][NEWMV_] = NULL;
            m_interp[2][depth][NEWMV_] = NULL;
            m_interp[3][depth][NEWMV_] = NULL;
            m_interp[4][depth][NEWMV_] = NULL;
            m_interp[meInfo->refIdxComb][depth][NEWMV_] = m_newMvInterp64;

        }
#endif
    }

    template <class PixType>
    void H265CU<PixType>::ModeDecisionInterTu7_SecondPass(int32_t miRow, int32_t miCol)
    {
        int32_t num4x4 = MAX_NUM_PARTITIONS;
        for (int32_t i = 0; i < MAX_NUM_PARTITIONS; i += num4x4) {
            const int32_t rasterIdx = h265_scan_z2r4[i];
            const int32_t x4 = (rasterIdx & 15);
            const int32_t y4 = (rasterIdx >> 4);
            const int32_t sbx = x4 << 2;
            const int32_t sby = y4 << 2;
            const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
            const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);
            ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
            num4x4 = num_4x4_blocks_lookup[mi->sbType];
            if (mi->mode == OUT_OF_PIC)
                continue;

            const BlockSize bsz = mi->sbType;
            const int32_t log2w = block_size_wide_4x4_log2[bsz];
            const int32_t log2h = block_size_high_4x4_log2[bsz];
            const int32_t num4x4w = 1 << log2w;
            const int32_t num4x4h = 1 << log2h;
            const TxSize maxTxSize = IPP_MIN(TX_32X32, max_txsize_lookup[bsz]);
            const int32_t sbw = num4x4w << 2;
            const int32_t sbh = num4x4h << 2;
            const int32_t haveTop = (miRow > m_tileBorders.rowStart);
            const int32_t haveLeft  = (miCol > m_tileBorders.colStart);
            const int32_t miColEnd = m_tileBorders.colEnd;

            const BitCounts &bc = m_currFrame->bitCount;
            const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
            const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
            const int32_t ctxSkip = GetCtxSkip(above, left);
            const int32_t ctxIsInter = GetCtxIsInter(above, left);
            const uint8_t aboveTxfm = above ? tx_size_wide[above->txSize] : 0;
            const uint8_t leftTxfm =  left  ? tx_size_high[left->txSize] : 0;
            const int32_t ctxTxSize = GetCtxTxSizeAV1(above, left, aboveTxfm, leftTxfm, maxTxSize);
            const uint32_t *skipBits = bc.skip[ctxSkip];
            const uint32_t *txSizeBits = bc.txSize[maxTxSize][ctxTxSize];
            const uint32_t *isInterBits = bc.isInter[ctxIsInter];
            const int32_t ctxInterp = GetCtxInterpBothAV1(above, left, mi->refIdx);
            const uint32_t *interpFilterBits0 = bc.interpFilterAV1[ctxInterp & 15];
            const uint32_t *interpFilterBits1 = bc.interpFilterAV1[ctxInterp >> 8];
            const uint32_t *intraModeBits = bc.intraModeAV1[maxTxSize];
            const Contexts origCtx = m_contexts; // contexts at the start of current block
            Contexts bestCtx;
            uint8_t *anz = m_contexts.aboveNonzero[0]+x4;
            uint8_t *lnz = m_contexts.leftNonzero[0]+y4;
            uint8_t *aboveTxfmCtx = m_contexts.aboveTxfm + x4;
            uint8_t *leftTxfmCtx  = m_contexts.leftTxfm  + y4;
            PixType *srcSb = m_ySrc + sbx + sby * SRC_PITCH;
            int16_t *diff = vp9scratchpad.diffY;
            int16_t *coef = vp9scratchpad.coefY;
            int16_t *qcoef = vp9scratchpad.qcoefY[0];
            int16_t *bestQcoef = vp9scratchpad.qcoefY[1];
            int16_t *coefWork = vp9scratchpad.coefWork;
            const int32_t refColocOffset = m_ctbPelX + sbx + (m_ctbPelY + sby) * m_pitchRecLuma;
            const PixType *srcPic = m_ySrc + sbx + sby * SRC_PITCH;

            const PixType * const refColoc[3] = {
                reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_recon->y) + refColocOffset,
                reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_recon->y) + refColocOffset,
                reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_recon->y) + refColocOffset,
            };

            TileBorders saveTileBorders = m_tileBorders;
            m_tileBorders.rowStart = 0;
            m_tileBorders.colStart = 0;
            m_tileBorders.rowEnd = m_par->miRows;
            m_tileBorders.colEnd = m_par->miCols;
            (m_currFrame->compoundReferenceAllowed)
                ? GetMvRefsAV1TU7B(bsz, miRow, miCol, &m_mvRefs, (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B), 2)
                : GetMvRefsAV1TU7P(bsz, miRow, miCol, &m_mvRefs, 2);
            m_tileBorders = saveTileBorders;

            EstimateRefFramesAllAv1(bc, *m_currFrame, above, left, m_mvRefs.refFrameBitCount, &m_mvRefs.memCtx);

            uint32_t interModeBitsAV1[4][/*INTER_MODES*/10+4];
            for (int ref = LAST_FRAME; ref <= ALTREF_FRAME; ref++) {
                const int32_t modeCtx = m_mvRefs.mvRefsAV1.interModeCtx[ref];
                const int32_t newMvCtx = modeCtx & NEWMV_CTX_MASK;
                const int32_t zeroMvCtx = (modeCtx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
                const int32_t refMvCtx = (modeCtx >> REFMV_OFFSET) & REFMV_CTX_MASK;
                interModeBitsAV1[ref][NEWMV_] = bc.newMvBit[newMvCtx][0];
                interModeBitsAV1[ref][ZEROMV_] = bc.newMvBit[newMvCtx][1] + bc.zeroMvBit[zeroMvCtx][0];
                interModeBitsAV1[ref][NEARESTMV_] = bc.newMvBit[newMvCtx][1] + bc.zeroMvBit[zeroMvCtx][1] + bc.refMvBit[refMvCtx][0];
                interModeBitsAV1[ref][NEARMV_] = bc.newMvBit[newMvCtx][1] + bc.zeroMvBit[zeroMvCtx][1] + bc.refMvBit[refMvCtx][1];
            }
            if (m_currFrame->compoundReferenceAllowed) {
                const int32_t modeCtx = m_mvRefs.mvRefsAV1.interModeCtx[COMP_VAR0];
                interModeBitsAV1[COMP_VAR0][NEWMV_]     = bc.interCompMode[modeCtx][NEW_NEWMV - NEAREST_NEARESTMV];
                interModeBitsAV1[COMP_VAR0][ZEROMV_]    = bc.interCompMode[modeCtx][ZERO_ZEROMV - NEAREST_NEARESTMV];
                interModeBitsAV1[COMP_VAR0][NEARESTMV_] = bc.interCompMode[modeCtx][NEAREST_NEARESTMV - NEAREST_NEARESTMV];
                interModeBitsAV1[COMP_VAR0][NEARMV_]    = bc.interCompMode[modeCtx][NEAR_NEARMV - NEAREST_NEARESTMV];
            }

            ALIGNED(32) PixType pred[64*64], pred2[64*64];

            unsigned int sad, sadRef0, sadRef1, best_sad, bits, cost, bestCost = UINT_MAX, bestDrl = 0;

            // first pass bit cost, assume NEWMV
            unsigned int bitsFirstPass = 0;
            unsigned int modeFirstPass = AV1_ZEROMV;
            unsigned int drlFirstPass = 0;

            if (mi->mode == AV1_ZEROMV) {
                bitsFirstPass = m_mvRefs.refFrameBitCount[mi->refIdxComb] + interModeBitsAV1[mi->refIdxComb][ZEROMV_];
                modeFirstPass = AV1_ZEROMV;
            } else {
                const int maxNumNewMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] - 1);
                bitsFirstPass = MvCost(mi->mv[0], m_mvRefs.bestMv[mi->refIdxComb][0], 0);
                if (mi->refIdx[1] != NONE_FRAME)
                    bitsFirstPass += MvCost(mi->mv[1], m_mvRefs.bestMv[mi->refIdxComb][1], 0);
                bitsFirstPass += IPP_MIN(maxNumNewMvDrlBits, 1) << 9;
                drlFirstPass = 0;
                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 1) {
                    bits = MvCost(mi->mv[0], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][1].mv[0], 0);
                    if (mi->refIdx[1] != NONE_FRAME)
                        bits += MvCost(mi->mv[1], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][1].mv[1], 0);
                    bits += IPP_MIN(maxNumNewMvDrlBits, 2) << 9;
                    if (bitsFirstPass > bits) {
                        bitsFirstPass = bits;
                        drlFirstPass = 1;
                    }
                }
                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 2) {
                    bits = MvCost(mi->mv[0], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv[0], 0);
                    if (mi->refIdx[1] != NONE_FRAME)
                        bits += MvCost(mi->mv[1], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv[1], 0);
                    bits += IPP_MIN(maxNumNewMvDrlBits, 2) << 9;
                    if (bitsFirstPass > bits) {
                        bitsFirstPass = bits;
                        drlFirstPass = 2;
                    }
                }
                bitsFirstPass += m_mvRefs.refFrameBitCount[mi->refIdxComb];
                bitsFirstPass += interModeBitsAV1[mi->refIdxComb][NEWMV_];
                modeFirstPass = AV1_NEWMV;
            }

            H265MVPair bestMv = {};
            int bestRefIdxComp = LAST_FRAME;
            int bestMode = 0;

            int checkedMv[3][4] = {};

            if (m_currFrame->compoundReferenceAllowed) {
                int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[COMP_VAR0] - 2);
                for (int m = 0; m < 4; m++) {
                    if (m >= 2 && m >= m_mvRefs.mvRefsAV1.refMvCount[COMP_VAR0])
                        continue;
                    H265MVPair refmv = m < 2 ? m_mvRefs.mvs[COMP_VAR0][m] : m_mvRefs.mvRefsAV1.refMvStack[COMP_VAR0][m].mv;
                    int numNearMvDrlBits = IPP_MIN(maxNumNearMvDrlBits, m);

                    InterpolateAv1SingleRefLuma(refColoc[LAST_FRAME], m_pitchRecLuma, pred, refmv[0], sbh, log2w, DEF_FILTER_DUAL);
                    InterpolateAv1SingleRefLuma(refColoc[ALTREF_FRAME], m_pitchRecLuma, pred2, refmv[1], sbh, log2w, DEF_FILTER_DUAL);

                    uint32_t costRef0 = MAX_UINT;
                    uint32_t costRef1 = MAX_UINT;
                    int modeRef0 = -1;
                    int modeRef1 = -1;
                    if (refmv[0] == m_mvRefs.mvRefsAV1.refMvStack[LAST_FRAME][3].mv[0] && m_mvRefs.mvRefsAV1.refMvCount[LAST_FRAME] > 3) modeRef0 = 3, checkedMv[LAST_FRAME][3] = 1;
                    if (refmv[0] == m_mvRefs.mvRefsAV1.refMvStack[LAST_FRAME][2].mv[0] && m_mvRefs.mvRefsAV1.refMvCount[LAST_FRAME] > 2) modeRef0 = 2, checkedMv[LAST_FRAME][2] = 1;
                    if (refmv[0] == m_mvRefs.mvs[LAST_FRAME][1][0]) modeRef0 = 1, checkedMv[LAST_FRAME][1] = 1;
                    if (refmv[0] == m_mvRefs.mvs[LAST_FRAME][0][0]) modeRef0 = 0, checkedMv[LAST_FRAME][0] = 1;
                    if (refmv[1] == m_mvRefs.mvRefsAV1.refMvStack[ALTREF_FRAME][3].mv[0] && m_mvRefs.mvRefsAV1.refMvCount[ALTREF_FRAME] > 3) modeRef1 = 3, checkedMv[ALTREF_FRAME][3] = 1;
                    if (refmv[1] == m_mvRefs.mvRefsAV1.refMvStack[ALTREF_FRAME][2].mv[0] && m_mvRefs.mvRefsAV1.refMvCount[ALTREF_FRAME] > 2) modeRef1 = 2, checkedMv[ALTREF_FRAME][2] = 1;
                    if (refmv[1] == m_mvRefs.mvs[ALTREF_FRAME][1][0]) modeRef1 = 1, checkedMv[ALTREF_FRAME][1] = 1;
                    if (refmv[1] == m_mvRefs.mvs[ALTREF_FRAME][0][0]) modeRef1 = 0, checkedMv[ALTREF_FRAME][0] = 1;

                    if (modeRef0 >= 0) {
                        const int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[LAST_FRAME] - 2);
                        const int numNearMvDrlBits = IPP_MIN(maxNumNearMvDrlBits, modeRef0);
                        sadRef0 = VP9PP::sad_general(srcPic, 64, pred, 64, log2w, log2h);
                        bits  = m_mvRefs.refFrameBitCount[LAST_FRAME];
                        bits += interModeBitsAV1[LAST_FRAME][modeRef0 == 0 ? NEARESTMV_ : NEARMV_];
                        bits += numNearMvDrlBits << 9;
                        costRef0 = RD32(sadRef0, bits, m_rdLambdaSqrtInt32);
                        checkedMv[LAST_FRAME][modeRef0] = 1;
                    }

                    if (modeRef1 >= 0) {
                        const int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[ALTREF_FRAME] - 2);
                        const int numNearMvDrlBits = IPP_MIN(maxNumNearMvDrlBits, modeRef1);
                        sadRef1 = VP9PP::sad_general(srcPic, 64, pred2, 64, log2w, log2h);
                        bits  = m_mvRefs.refFrameBitCount[ALTREF_FRAME];
                        bits += interModeBitsAV1[ALTREF_FRAME][modeRef1 == 0 ? NEARESTMV_ : NEARMV_];
                        bits += numNearMvDrlBits << 9;
                        costRef1 = RD32(sadRef1, bits, m_rdLambdaSqrtInt32);
                        checkedMv[ALTREF_FRAME][modeRef1] = 1;
                    }

                    VP9PP::average(pred, pred2, pred, sbh, log2w);
                    sad = VP9PP::sad_general(srcPic, 64, pred, 64, log2w, log2h);
                    bits  = m_mvRefs.refFrameBitCount[COMP_VAR0];
                    bits += interModeBitsAV1[COMP_VAR0][m == 0 ? NEARESTMV_ : NEARMV_];
                    bits += numNearMvDrlBits << 9;

                    cost = RD32(sad, bits, m_rdLambdaSqrtInt32);
                    if (bestCost > cost) {
                        bestCost = cost;
                        best_sad = sad;
                        bestRefIdxComp = COMP_VAR0;
                        bestMv = refmv;
                        bestMode = AV1_NEARESTMV;
                        bestDrl = (m == 0) ? 0 : m - 1;
                    }

                    if (bestCost > costRef0) {
                        bestCost = costRef0;
                        best_sad = sadRef0;
                        bestRefIdxComp = LAST_FRAME;
                        bestMv[0] = refmv[0];
                        bestMv[1].asInt = 0;
                        bestMode = AV1_NEARESTMV;
                        bestDrl = (modeRef0 == 0) ? 0 : m - 1;
                    }

                    if (bestCost > costRef1) {
                        bestCost = costRef1;
                        best_sad = sadRef1;
                        bestRefIdxComp = ALTREF_FRAME;
                        bestMv[0] = refmv[1];
                        bestMv[1].asInt = 0;
                        bestMode = AV1_NEARESTMV;
                        bestDrl = (modeRef1 == 0) ? 0 : m - 1;
                    }
                }
            }

            for (int ref = 0; ref < 3; ref++) {
                if (!m_currFrame->isUniq[ref]) continue;
                int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[ref] - 2);
                for (int m = 0; m < 4; m++) {
                    if (m >= 2 && m >= m_mvRefs.mvRefsAV1.refMvCount[ref])
                        continue;
                    if (checkedMv[ref][m])
                        continue;
                    H265MV refmv = m < 2 ? m_mvRefs.mvs[ref][m][0] : m_mvRefs.mvRefsAV1.refMvStack[ref][m].mv[0];
                    int numNearMvDrlBits = IPP_MIN(maxNumNearMvDrlBits, m);

                    InterpolateAv1SingleRefLuma(refColoc[ref], m_pitchRecLuma, pred, refmv, sbh, log2w, DEF_FILTER_DUAL);
                    sad = VP9PP::sad_general(srcPic, 64, pred, 64, log2w, log2h);
                    bits  = m_mvRefs.refFrameBitCount[ref];
                    bits += interModeBitsAV1[ref][m == 0 ? NEARESTMV_ : NEARMV_];
                    bits += numNearMvDrlBits << 9;

                    cost = RD32(sad, bits, m_rdLambdaSqrtInt32);
                    if (bestCost > cost) {
                        bestCost = cost;
                        best_sad = sad;
                        bestRefIdxComp = ref;
                        bestMv[0] = refmv;
                        bestMv[1].asInt = 0;
                        bestMode = AV1_NEARESTMV;
                        bestDrl = (m == 0) ? 0 : m - 1;
                    }
                }
            }

            H265MVPair clippedMv = mi->mv;
            ClipMV_NR(&clippedMv[0]);
            ClipMV_NR(&clippedMv[1]);
            InterpolateAv1SingleRefLuma(refColoc[ mi->refIdx[0] ], m_pitchRecLuma, pred, clippedMv[0], sbh, log2w, DEF_FILTER_DUAL);
            if (mi->refIdxComb == COMP_VAR0) {
                InterpolateAv1SingleRefLuma(refColoc[ mi->refIdx[1] ], m_pitchRecLuma, pred2, clippedMv[1], sbh, log2w, DEF_FILTER_DUAL);
                VP9PP::average(pred, pred2, pred, sbh, log2w);
            }
            assert((VP9PP::sad_general(srcPic, 64, pred, 64, log2w, log2h) << 11) == mi->sad);
            unsigned costFirstPass = mi->sad + bitsFirstPass * m_rdLambdaSqrtInt32;

            if (bestCost >= costFirstPass)
            {
                bestCost = costFirstPass;
                best_sad = mi->sad >> 11;
                bestRefIdxComp = mi->refIdxComb;
                bestMv = mi->mv;
                bestMode = modeFirstPass;
                bestDrl = drlFirstPass;
            }

            mi->mv = bestMv;
            mi->refIdx[0] = bestRefIdxComp == COMP_VAR0 ? LAST_FRAME : bestRefIdxComp;
            mi->refIdx[1] = bestRefIdxComp == COMP_VAR0 ? ALTREF_FRAME : NONE_FRAME;
            mi->refIdxComb = bestRefIdxComp;
            // For Split64 && TryIntra
            mi->sad = best_sad;

            H265MVPair bestRefMv = m_mvRefs.mvs[bestRefIdxComp][NEARESTMV_];
            if (bestRefIdxComp == COMP_VAR0) {
#ifdef TRYINTRA_ORIG
                mi->mvd[0].mvx = bestRefMv[0].mvx;
                mi->mvd[0].mvy = bestRefMv[0].mvy;
                mi->mvd[1].mvx = bestRefMv[1].mvx;
                mi->mvd[1].mvy = bestRefMv[1].mvy;
#else
                mi->mvd[0].mvx = mi->mv[0].mvx - bestRefMv[0].mvx;
                mi->mvd[0].mvy = mi->mv[0].mvy - bestRefMv[0].mvy;
                mi->mvd[1].mvx = mi->mv[1].mvx - bestRefMv[1].mvx;
                mi->mvd[1].mvy = mi->mv[1].mvy - bestRefMv[1].mvy;
#endif
            }else {
#ifndef TRYINTRA_ORIG
                mi->mvd[0].mvx = mi->mv[0].mvx - bestRefMv[0].mvx;
                mi->mvd[0].mvy = mi->mv[0].mvy - bestRefMv[0].mvy;
                mi->mvd[1].asInt = 0;
#else
                mi->mvd[0].mvx = m_mvRefs.mvs[0][NEARESTMV_][0].mvx;
                mi->mvd[0].mvy = m_mvRefs.mvs[0][NEARESTMV_][0].mvy;
                mi->mvd[1].mvx = m_mvRefs.mvs[m_currFrame->compoundReferenceAllowed ? ALTREF_FRAME : GOLDEN_FRAME][NEARESTMV_][0].mvx;
                mi->mvd[1].mvy = m_mvRefs.mvs[m_currFrame->compoundReferenceAllowed ? ALTREF_FRAME : GOLDEN_FRAME][NEARESTMV_][0].mvy;
#endif
            }
            mi->mode = bestMode;
            mi->refMvIdx = bestDrl;
            PropagateSubPart(mi, m_par->miPitch, sbw >> 3, sbh >> 3);
        }
    }

    template <typename PixType>
    ModeInfo *H265CU<PixType>::StoredCuData(int32_t storageIndex) const
    {
        assert(storageIndex < NUM_DATA_STORED_LAYERS);
        return m_dataStored + (storageIndex << 6);
    }

    template<typename PixType>
    void InterpolateAv1SingleRefLuma(const PixType *refColoc, int32_t refPitch, PixType *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp)
    {
        const int32_t intx = mv.mvx >> 3;
        const int32_t inty = mv.mvy >> 3;
        const int32_t dx = (mv.mvx << 1) & 15;
        const int32_t dy = (mv.mvy << 1) & 15;
        refColoc += intx + inty * refPitch;
        VP9PP::interp_av1(refColoc, refPitch, dst, dx, dy, h, log2w, interp);
    }
    template void InterpolateAv1SingleRefLuma<uint8_t> (const uint8_t*, int32_t,uint8_t*, H265MV,int32_t,int32_t,int32_t);
    template void InterpolateAv1SingleRefLuma<uint16_t>(const uint16_t*,int32_t,uint16_t*,H265MV,int32_t,int32_t,int32_t);

    template<typename PixType>
    void InterpolateAv1SingleRefChroma(const PixType *refColoc, int32_t refPitch, PixType *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp)
    {
        const int32_t intx = (mv.mvx >> 4) << 1;
        const int32_t inty = mv.mvy >> 4;
        const int32_t dx = mv.mvx & 15;
        const int32_t dy = mv.mvy & 15;
        refColoc += intx + inty * refPitch;
        VP9PP::interp_nv12_av1(refColoc, refPitch, dst, dx, dy, h, log2w, interp);
    }
    template void InterpolateAv1SingleRefChroma<uint8_t> (const uint8_t*, int32_t,uint8_t*, H265MV,int32_t,int32_t,int32_t);
    template void InterpolateAv1SingleRefChroma<uint16_t>(const uint16_t*,int32_t,uint16_t*,H265MV,int32_t,int32_t,int32_t);

    template<typename PixType>
    void InterpolateAv1FirstRefLuma(const PixType *refColoc, int32_t refPitch, int16_t *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp)
    {
        const int32_t intx = mv.mvx >> 3;
        const int32_t inty = mv.mvy >> 3;
        const int32_t dx = (mv.mvx << 1) & 15;
        const int32_t dy = (mv.mvy << 1) & 15;
        refColoc += intx + inty * refPitch;
        VP9PP::interp_av1(refColoc, refPitch, dst, dx, dy, h, log2w, interp);
    }
    template void InterpolateAv1FirstRefLuma<uint8_t> (const uint8_t*, int32_t,int16_t*,H265MV,int32_t,int32_t,int32_t);
    template void InterpolateAv1FirstRefLuma<uint16_t>(const uint16_t*,int32_t,int16_t*,H265MV,int32_t,int32_t,int32_t);

    template<typename PixType>
    void InterpolateAv1FirstRefChroma(const PixType *refColoc, int32_t refPitch, int16_t *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp)
    {
        const int32_t intx = (mv.mvx >> 4) << 1;
        const int32_t inty = mv.mvy >> 4;
        const int32_t dx = mv.mvx & 15;
        const int32_t dy = mv.mvy & 15;
        refColoc += intx + inty * refPitch;
        VP9PP::interp_nv12_av1(refColoc, refPitch, dst, dx, dy, h, log2w, interp);
    }
    template void InterpolateAv1FirstRefChroma<uint8_t> (const uint8_t*, int32_t,int16_t*,H265MV,int32_t,int32_t,int32_t);
    template void InterpolateAv1FirstRefChroma<uint16_t>(const uint16_t*,int32_t,int16_t*,H265MV,int32_t,int32_t,int32_t);

    template<typename PixType>
    void InterpolateAv1SecondRefLuma(const PixType *refColoc, int32_t refPitch, int16_t *ref0, PixType *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp)
    {
        const int32_t intx = mv.mvx >> 3;
        const int32_t inty = mv.mvy >> 3;
        const int32_t dx = (mv.mvx << 1) & 15;
        const int32_t dy = (mv.mvy << 1) & 15;
        refColoc += intx + inty * refPitch;
        VP9PP::interp_av1(refColoc, refPitch, ref0, dst, dx, dy, h, log2w, interp);
    }
    template void InterpolateAv1SecondRefLuma<uint8_t> (const uint8_t*, int32_t,int16_t*,uint8_t*, H265MV,int32_t,int32_t,int32_t);
    template void InterpolateAv1SecondRefLuma<uint16_t>(const uint16_t*,int32_t,int16_t*,uint16_t*,H265MV,int32_t,int32_t,int32_t);

    template<typename PixType>
    void InterpolateAv1SecondRefChroma(const PixType *refColoc, int32_t refPitch, int16_t *ref0, PixType *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp)
    {
        const int32_t intx = (mv.mvx >> 4) << 1;
        const int32_t inty = mv.mvy >> 4;
        const int32_t dx = mv.mvx & 15;
        const int32_t dy = mv.mvy & 15;
        refColoc += intx + inty * refPitch;
        VP9PP::interp_nv12_av1(refColoc, refPitch, ref0, dst, dx, dy, h, log2w, interp);
    }
    template void InterpolateAv1SecondRefChroma<uint8_t> (const uint8_t*, int32_t,int16_t*,uint8_t*, H265MV,int32_t,int32_t,int32_t);
    template void InterpolateAv1SecondRefChroma<uint16_t>(const uint16_t*,int32_t,int16_t*,uint16_t*,H265MV,int32_t,int32_t,int32_t);

    template class H265CU<uint8_t>;
    template class H265CU<uint16_t>;

}  // namespace H265Enc

