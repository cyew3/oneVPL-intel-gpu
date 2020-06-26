// Copyright (c) 2014-2020 Intel Corporation
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

#include <limits.h> /* for INT_MAX on Linux/Android */
#include <algorithm>
#include <assert.h>
#include "mfx_av1_defs.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_encode.h"
#include "mfx_av1_quant.h"
#include "mfx_av1_copy.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_dispatcher_wrappers.h"
#include "mfx_av1_get_intra_pred_pels.h"
#include "mfx_av1_get_context.h"
#include "mfx_av1_prob_opt.h"
#include "mfx_av1_trace.h"
#include "mfx_av1_superres.h"

#define ENABLE_DEBUG_PRINTF 0
#if ENABLE_DEBUG_PRINTF
  #define debug_printf(...) fprintf(stderr, __VA_ARGS__)
#else
  #define debug_printf(...) (void)0
#endif

namespace AV1Enc {
    namespace {
        int32_t MvCost(int16_t mvd) { // TODO(npshosta): remove all usage of this one
            uint8_t ZF = 0;
            int32_t index = BSR(mvd*mvd, ZF);
            return ((-int32_t(ZF)) & (index + 1)) + 1;  // mvd ? 2*index+3 : 1;
        }

        int32_t MvCost(uint32_t absMvd, int32_t useMvHp) {
            // model logarithmic function = numbits(mvcomp) = (log2((mvcomp>>1) + 1) << 10) + 800 + (useMvHp ? 512 : 0)
            return (BSR((absMvd >> 1) + 1) << 10) + 800 + (useMvHp << 9);
        }

        int32_t MvCost(int16_t mvdx, int16_t mvdy, int32_t useMvHp) {
            return MvCost(abs(mvdx), useMvHp) + MvCost(abs(mvdy), useMvHp);
        }

        int32_t MvCost(AV1MVPair mv, AV1MV mvp0, AV1MV mvp1, int32_t useMvHp0, int32_t useMvHp1) {
            return MvCost(abs(mv[0].mvx - mvp0.mvx), useMvHp0) + MvCost(abs(mv[0].mvy - mvp0.mvy), useMvHp0)
                 + MvCost(abs(mv[1].mvx - mvp1.mvx), useMvHp1) + MvCost(abs(mv[1].mvy - mvp1.mvy), useMvHp1);
        }

        bool CheckSearchRange(int32_t y, int32_t h, const AV1MV &mv, int32_t searchRangeY) {
            return y + (mv.mvy >> 2) + h + 4 + 1 <= searchRangeY;  // +4 from luma interp +1 from chroma interp
        }

        bool SameMotion(const AV1MV mv0[2], const AV1MV mv1[2], const int8_t refIdx0[2], const int8_t refIdx1[2]) {
            return *reinterpret_cast<const uint16_t*>(refIdx0) == *reinterpret_cast<const uint16_t*>(refIdx1) &&
                *reinterpret_cast<const uint64_t*>(mv0) == *reinterpret_cast<const uint64_t*>(mv1);
        }

        bool HasSameMotion(const AV1MV mv[2], const AV1MV (*otherMvs)[2], const int8_t refIdx[2],
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

        const int16_t av1_tabTx[128] = {
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
                int32_t tx = (td > 0) ? av1_tabTx[td] : -av1_tabTx[-td];
                assert(tx == (16384 + abs(td/2))/td);
                int32_t distScaleFactor = Saturate(-4096, 4095, (tb * tx + 32) >> 6);
                return distScaleFactor;
            }
        }

        uint32_t EstimateMvComponent(const BitCounts &bitCount, int16_t dmv, int32_t useHp, int32_t comp) {
            assert(useHp == 0 || useHp == 1);
            uint32_t cost = 0;
            cost += bitCount.mvSign[comp][dmv < 0];
            int diffmv = abs(dmv)-1;
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


        uint32_t EstimateMv(const BitCounts &bitCount, AV1MV mv, AV1MV bestMv, int32_t useMvHp) {
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

            if (frame.referenceMode == SINGLE_REFERENCE) {
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

    int32_t MvCost(AV1MV mv, AV1MV mvp, int32_t useMvHp) {
        return MvCost(abs(mv.mvx - mvp.mvx), useMvHp) + MvCost(abs(mv.mvy - mvp.mvy), useMvHp);
    }

    const Contexts& Contexts::operator=(const Contexts &other) {
        storea_si128(aboveNonzero[0], loada_si128(other.aboveNonzero[0]));
        storea_si128(aboveNonzero[1], loada_si128(other.aboveNonzero[1]));
        storea_si128(aboveNonzero[2], loada_si128(other.aboveNonzero[2]));
        storea_si128(leftNonzero[0], loada_si128(other.leftNonzero[0]));
        storea_si128(leftNonzero[1], loada_si128(other.leftNonzero[1]));
        storea_si128(leftNonzero[2], loada_si128(other.leftNonzero[2]));
        storea_si128(aboveAndLeftPartition, loada_si128(other.aboveAndLeftPartition));
        storea_si128(aboveTxfm, loada_si128(other.aboveTxfm));
        storea_si128(leftTxfm,  loada_si128(other.leftTxfm));
        return *this;
    }


    int32_t GetLumaOffset(const AV1VideoParam *par, int32_t absPartIdx, int32_t pitch)
    {
        int32_t puRasterIdx = av1_scan_z2r4[absPartIdx];
        int32_t puStartRow = puRasterIdx >> 4;
        int32_t puStartColumn = puRasterIdx & 15;

        return (puStartRow * pitch + puStartColumn) << LOG2_MIN_TU_SIZE;
    }

    // chroma offset for NV12
    int32_t GetChromaOffset(const AV1VideoParam *par, int32_t absPartIdx, int32_t pitch)
    {
        int32_t puRasterIdx = av1_scan_z2r4[absPartIdx];
        int32_t puStartRow = puRasterIdx >> 4;
        int32_t puStartColumn = puRasterIdx & 15;

        return ((puStartRow * pitch >> par->chromaShiftH) + (puStartColumn << par->chromaShiftWInv)) << LOG2_MIN_TU_SIZE;
    }


    int32_t GetChromaOffset1(const AV1VideoParam *par, int32_t absPartIdx, int32_t pitch)
    {
        int32_t puRasterIdx = av1_scan_z2r4[absPartIdx];
        int32_t puStartRow = puRasterIdx >> 4;
        int32_t puStartColumn = puRasterIdx & 15;

        return ((puStartRow * pitch >> par->chromaShiftH) + (puStartColumn >> par->chromaShiftW)) << LOG2_MIN_TU_SIZE;
    }


    // propagate data[0] thru data[1..numParts-1] where numParts=2^n
    void PropagateSubPart(ModeInfo *mi, int32_t miPitch, int32_t miWidth, int32_t miHeight)
    {
        assert(sizeof(*mi) == 32);  // if assert fails uncomment and use nonoptimized-code
        assert((size_t(mi) & 31) == 0);  // expecting 16-byte aligned address
        uint8_t *ptr = reinterpret_cast<uint8_t*>(mi);
        const int32_t pitch = sizeof(ModeInfo) * miPitch;
        const __m256i half = loada_si256(ptr + 0);
        for (int32_t x = 1; x < miWidth; x++) {
            storea_si256(ptr + x * 32 + 0,  half);
        }
        ptr += pitch;
        for (int32_t y = 1; y < miHeight; y++, ptr += pitch) {
            for (int32_t x = 0; x < miWidth; x++) {
                storea_si256(ptr + x * 32 + 0,  half);
            }
        }
    }

    void PropagatePaletteInfo(PaletteInfo *p, int32_t miRow, int32_t miCol, int32_t miPitch, int32_t miWidth, int32_t miHeight)
    {
        //propogate only border blocks
        PaletteInfo* p_ptr = &p[miRow*miPitch + miCol];
        const uint8_t ps_y = p_ptr->palette_size_y;
        const uint8_t ps_uv = p_ptr->palette_size_uv;
        for (int32_t j = 1;j < miWidth;j++) {
            PaletteInfo* pc = (p_ptr + j);
            pc->palette_size_y = ps_y;
            pc->palette_size_uv = ps_uv;
            for (int32_t k = 0; k < ps_y; k++) {
                pc->palette_y[k] = p_ptr->palette_y[k];
            }
            for (int32_t k = 0; k < ps_uv; k++) {
                pc->palette_u[k] = p_ptr->palette_u[k];
                pc->palette_v[k] = p_ptr->palette_v[k];
            }
        }
        for (int32_t i = 1;i < miHeight;i++) {
            for (int32_t j = 0;j < miWidth;j++) {
                PaletteInfo* pc = (p_ptr + j +  i*miPitch);

                pc->palette_size_y = ps_y;
                pc->palette_size_uv = ps_uv;
                for (int32_t k = 0; k < ps_y; k++) {
                    pc->palette_y[k] = p_ptr->palette_y[k];
                }
                for (int32_t k = 0; k < ps_uv; k++) {
                    pc->palette_u[k] = p_ptr->palette_u[k];
                    pc->palette_v[k] = p_ptr->palette_v[k];
                }
            }
        }
    }

    void InitSuperBlock(SuperBlock *sb, int32_t sbRow, int32_t sbCol, const AV1VideoParam &par, Frame *frame, const CoeffsType *coefs) {
        sb->par = &par;
        sb->frame = frame;
        sb->sbRow = sbRow;
        sb->sbCol = sbCol;
        sb->pelX = sbCol << LOG2_MAX_CU_SIZE;
        sb->pelY = sbRow << LOG2_MAX_CU_SIZE;
        sb->sbIndex = sbRow * par.sb64Cols + sbCol;
        sb->tileRow = frame->m_tileParam.mapSb2TileRow[sb->sbRow];
        sb->tileCol = frame->m_tileParam.mapSb2TileCol[sb->sbCol];
        sb->tileIndex = (sb->tileRow * frame->m_tileParam.cols) + sb->tileCol;
        sb->mi = frame->m_modeInfo + (sbRow << 3) * par.miPitch + (sbCol << 3);
        int32_t numCoeffL = 1 << (LOG2_MAX_CU_SIZE << 1);
        int32_t numCoeffC = (numCoeffL >> par.chromaShift);
        sb->coeffs[0] = coefs + sb->sbIndex * (numCoeffL + numCoeffC + numCoeffC);
        sb->coeffs[1] = sb->coeffs[0] + numCoeffL;
        sb->coeffs[2] = sb->coeffs[1] + numCoeffC;
        sb->cdefPreset = -1;
        sb->code_delta = par.UseDQP ? 1 : 0;
    }


    template <typename PixType>
    void CalculateAndAccumulateSatds(const PixType *src, const PixType *pred, int32_t *satd[4], bool useHadamard) {
#ifdef USE_SAD_ONLY
        AV1PP::sad_store8x8(src, pred, satd[3], 4);
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

        __m128i satd_64x64 = _mm_add_epi32(satd_32x32, _mm_bsrli_si128(satd_32x32, 8));
        *satd[0] = _mm_cvtsi128_si32(satd_64x64) + _mm_extract_epi32(satd_64x64, 1);
    }


    struct GpuMeData {
        AV1MV mv;
        uint32_t idx;
    };

    template <typename PixType> void AV1CU<PixType>::CalculateNewMvPredAndSatd()
    {
        enum { SIZE8 = 0, SIZE16 = 1, SIZE32 = 2, SIZE64 = 3 };
        enum { DEPTH8 = 3, DEPTH16 = 2, DEPTH32 = 1, DEPTH64 = 0 };

        const int8_t secondRef = m_currFrame->compoundReferenceAllowed
            ? ALTREF_FRAME
            : m_currFrame->isUniq[GOLDEN_FRAME] ? GOLDEN_FRAME : LAST_FRAME;
        const uint16_t mapGpuRefIdx[3] = {
            uint16_t(LAST_FRAME | (NONE_FRAME<<8)),
            uint16_t(secondRef | (NONE_FRAME<<8)),
            uint16_t(LAST_FRAME | (ALTREF_FRAME<<8)) };

        { // 8x8
            const int32_t pitchGpuMeData8 = m_currFrame->m_feiInterData[LAST_FRAME][SIZE8]->m_pitch;
            const int32_t offsetGpuMeData8 = (m_ctbPelY >> 3) * pitchGpuMeData8 + m_ctbPelX;
            const GpuMeData *gpuMeData[2] = {
                reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[LAST_FRAME][SIZE8]->m_sysmem + offsetGpuMeData8),
                reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[secondRef][SIZE8]->m_sysmem + offsetGpuMeData8)
            };

            const int32_t num8x8w = std::min(8, (m_par->Width  - m_ctbPelX) >> 3);
            const int32_t num8x8h = std::min(8, (m_par->Height - m_ctbPelY) >> 3);

            for (int32_t yblk = 0; yblk < num8x8h; yblk++) {
                int32_t numBlk = (yblk << 3);
                for (int32_t xblk = 0; xblk < num8x8w; xblk++, numBlk++) {
                    const uint32_t gpuRefIdx0 = gpuMeData[0][xblk].idx;
                    const uint16_t refs0 = mapGpuRefIdx[gpuRefIdx0 & 0x3];
                    *((uint16_t*)(&m_newMvRefIdx8[numBlk + 0][0])) = refs0;
                    m_newMvFromGpu8[numBlk + 0][0].asInt = ClipMV_NR_shift(gpuMeData[gpuRefIdx0 & 1][xblk].mv);
                    if ((refs0>>8) != NONE_FRAME) {
                        m_newMvFromGpu8[numBlk + 0][0].asInt = ClipMV_NR_shift(gpuMeData[1][xblk].mv);
                    } else {
                        m_newMvFromGpu8[numBlk + 0][1].asInt = 0;
                    }
                    m_newMvSatd8[numBlk + 0] = gpuRefIdx0 >> 2;
                }
                gpuMeData[0] = reinterpret_cast<const GpuMeData *>(reinterpret_cast<const uint8_t *>(gpuMeData[0]) + pitchGpuMeData8);
                gpuMeData[1] = reinterpret_cast<const GpuMeData *>(reinterpret_cast<const uint8_t *>(gpuMeData[1]) + pitchGpuMeData8);
            }

            //PixType *interpData = reinterpret_cast<PixType *>(m_currFrame->m_feiInterp[SIZE8]->m_sysmem + m_ctbAddr * (64 * 64));
            //m_newMvInterp8 = interpData;
        }

        { // 16x16
            const int32_t pitchGpuMeData16 = m_currFrame->m_feiInterData[LAST_FRAME][SIZE16]->m_pitch;
            const int32_t offsetGpuMeData16 = (m_ctbPelY >> 4) * pitchGpuMeData16 + (m_ctbPelX >> 1);
            const GpuMeData *gpuMeData[2] = {
                reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[LAST_FRAME][SIZE16]->m_sysmem + offsetGpuMeData16),
                reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[secondRef][SIZE16]->m_sysmem + offsetGpuMeData16)
            };

            const int32_t num16x16w = std::min(4, (m_par->Width  - m_ctbPelX) >> 4);
            const int32_t num16x16h = std::min(4, (m_par->Height - m_ctbPelY) >> 4);

            for (int32_t yblk = 0; yblk < num16x16h; yblk++) {
                int32_t numBlk = (yblk << 2);
                for (int32_t xblk = 0; xblk < num16x16w; xblk++, numBlk++) {
                    // store best mv, refIdx for first block
                    int32_t gpuRefIdx = gpuMeData[0][xblk + 0].idx;
                    m_newMvSatd16[numBlk] = gpuRefIdx >> 2;
                    const uint16_t refs = mapGpuRefIdx[gpuRefIdx&3];
                    *((uint16_t*)(&m_newMvRefIdx16[numBlk][0])) = refs;

                    m_newMvFromGpu16[numBlk][0].asInt = ClipMV_NR_shift(gpuMeData[gpuRefIdx & 1][xblk + 0].mv);
                    if ((refs>>8) != NONE_FRAME) {
                        m_newMvFromGpu16[numBlk][1].asInt = ClipMV_NR_shift(gpuMeData[1][xblk + 0].mv);
                    } else {
                        m_newMvFromGpu16[numBlk][1].asInt = 0;
                    }
                }
                gpuMeData[0] = reinterpret_cast<const GpuMeData *>(reinterpret_cast<const uint8_t *>(gpuMeData[0]) + pitchGpuMeData16);
                gpuMeData[1] = reinterpret_cast<const GpuMeData *>(reinterpret_cast<const uint8_t *>(gpuMeData[1]) + pitchGpuMeData16);
            }
            //PixType *interpData = reinterpret_cast<PixType *>(m_currFrame->m_feiInterp[SIZE16]->m_sysmem + m_ctbAddr * (64 * 64));
            //m_newMvInterp16 = interpData;
        }

        { // 32x32
            const int32_t pitchGpuMeData32 = m_currFrame->m_feiInterData[LAST_FRAME][SIZE32]->m_pitch;
            const int32_t offsetGpuMeData32 = (m_ctbPelY >> 5) * pitchGpuMeData32 + (m_ctbPelX >> 5) * 64;

            const int32_t num32x32w = std::min(2, (m_par->Width  - m_ctbPelX) >> 5);
            const int32_t num32x32h = std::min(2, (m_par->Height - m_ctbPelY) >> 5);

            for (int32_t yblk = 0; yblk < num32x32h; yblk++) {
                int32_t numBlk = (yblk << 1);
                const GpuMeData *gpuMeData[2] = {
                    reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[LAST_FRAME][SIZE32]->m_sysmem + offsetGpuMeData32 + pitchGpuMeData32 * yblk),
                    reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[secondRef][SIZE32]->m_sysmem + offsetGpuMeData32 + pitchGpuMeData32 * yblk)
                };
                for (int32_t xblk = 0; xblk < num32x32w; xblk++, numBlk++) {

                    const int off = xblk << 6;
                    // store best mv, refIdx for first block
                    int32_t gpuRefIdx = gpuMeData[0]->idx;
                    m_newMvSatd32[numBlk] = gpuRefIdx >> 2;
                    const uint16_t refs0 = mapGpuRefIdx[gpuRefIdx&3];
                    *((uint16_t*)(&m_newMvRefIdx32[numBlk + 0][0])) = refs0;

                    m_newMvFromGpu32[numBlk + 0][0].asInt = ClipMV_NR_shift(gpuMeData[gpuRefIdx & 1][off].mv);
                    if ((refs0>>8) != NONE_FRAME) {
                        m_newMvFromGpu32[numBlk + 0][1].asInt = ClipMV_NR_shift(gpuMeData[1][off].mv);
                    } else {
                        m_newMvFromGpu32[numBlk + 0][1].asInt = 0;
                    }
                }
            }
            //PixType *interpData = reinterpret_cast<PixType *>(m_currFrame->m_feiInterp[SIZE32]->m_sysmem + m_ctbAddr * (64 * 64));
            //m_newMvInterp32 = interpData;
        }

        // SLM doesn't work on HSW and some issue on SKL with GT2
        if (1)
        { // 64x64
            const int32_t pitchGpuMeData64 = m_currFrame->m_feiInterData[LAST_FRAME][SIZE64]->m_pitch;
            const int32_t offsetGpuMeData64 = (m_ctbPelY >> 6) * pitchGpuMeData64 + (m_ctbPelX >> 6) * 64;

            const int32_t num64x64w = std::min(1, (m_par->Width  - m_ctbPelX) >> 6);
            const int32_t num64x64h = std::min(1, (m_par->Height - m_ctbPelY) >> 6);

            for (int32_t yblk = 0; yblk < num64x64h; yblk++) {
                int32_t numBlk = (yblk << 1);
                const GpuMeData *gpuMeData[2] = {
                    reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[LAST_FRAME][SIZE64]->m_sysmem + offsetGpuMeData64 + pitchGpuMeData64 * yblk),
                    reinterpret_cast<const GpuMeData *>(m_currFrame->m_feiInterData[secondRef][SIZE64]->m_sysmem + offsetGpuMeData64 + pitchGpuMeData64 * yblk)
                };
                for (int32_t xblk = 0; xblk < num64x64w; xblk++, numBlk++) {
                    const int off = xblk << 6;
                    // store best mv, refIdx for first block
                    const int32_t gpuRefIdx = gpuMeData[0]->idx;
                    m_newMvSatd64[numBlk] = gpuRefIdx >> 2;
                    const uint16_t refs0 = mapGpuRefIdx[gpuRefIdx&3];
                    *((uint16_t*)(&m_newMvRefIdx64[numBlk + 0][0])) = refs0;

                    m_newMvFromGpu64[numBlk + 0][0].asInt = ClipMV_NR_shift(gpuMeData[gpuRefIdx & 1][off].mv);
                    if ((refs0>>8) != NONE_FRAME) {
                        m_newMvFromGpu64[numBlk + 0][1].asInt = ClipMV_NR_shift(gpuMeData[1][off].mv);
                    } else {
                        m_newMvFromGpu64[numBlk + 0][1].asInt = 0;
                    }
                }
            }
            //PixType *interpData = reinterpret_cast<PixType *>(m_currFrame->m_feiInterp[SIZE64]->m_sysmem + m_ctbAddr * (64 * 64));
            //m_newMvInterp64 = interpData;
        }
    }

    template <typename PixType>
    void AV1CU<PixType>::InitCu(const AV1VideoParam &par, ModeInfo *_data, ModeInfo *_dataTemp,
                                 int32_t ctbRow, int32_t ctbCol, const Frame &frame, CoeffsType *m_coeffWork, uint8_t* lcuRound)
    {
        m_par = &par;
        //m_sliceQpY = frame.m_sliceQpY;
        m_lcuQps = &frame.m_lcuQps[0];

        m_data = _data;
        m_dataStored = _dataTemp;
        m_ctbAddr = ctbRow * m_par->PicWidthInCtbs + ctbCol;
        m_ctbPelX = ctbCol * m_par->MaxCUSize;
        m_ctbPelY = ctbRow * m_par->MaxCUSize;

        m_currFrame = &frame;

        FrameData* recon = m_currFrame->m_recon;
        m_pitchRecLuma = recon->pitch_luma_pix;
        m_pitchRecChroma = recon->pitch_chroma_pix;
        m_yRec  = reinterpret_cast<PixType*>(recon->y)  + m_ctbPelX + m_ctbPelY * m_pitchRecLuma;
        m_uvRec = reinterpret_cast<PixType*>(recon->uv) + m_ctbPelX + (m_ctbPelY >> 1) * m_pitchRecChroma;

        if (m_par->src10Enable) {
            FrameData* recon10 = m_currFrame->m_recon10;
            m_pitchRecLuma10 = recon10->pitch_luma_pix;
            m_pitchRecChroma10 = recon10->pitch_chroma_pix;
            m_yRec10 = reinterpret_cast<uint16_t*>(recon10->y) + m_ctbPelX + m_ctbPelY * m_pitchRecLuma10;
            m_uvRec10 = reinterpret_cast<uint16_t*>(recon10->uv) + m_ctbPelX + (m_ctbPelY >> 1) * m_pitchRecChroma10;
        }

        FrameData* origin = m_currFrame->m_origin;
        PixType *originY  = reinterpret_cast<PixType*>(origin->y) + m_ctbPelX + m_ctbPelY * origin->pitch_luma_pix;
        PixType *originUv = reinterpret_cast<PixType*>(origin->uv) + m_ctbPelX + (m_ctbPelY >> 1) * origin->pitch_chroma_pix;
        //PF FIXME
        CopyNxN(originY, origin->pitch_luma_pix, m_ySrc, 64);
        CopyNxM(originUv, origin->pitch_chroma_pix, m_uvSrc, 64, 32);
        if (m_par->src10Enable) {
            FrameData* origin10 = m_currFrame->m_origin10;
            uint16_t *originY10 = reinterpret_cast<uint16_t*>(origin10->y) + m_ctbPelX + m_ctbPelY * origin10->pitch_luma_pix;
            uint16_t *originUv10 = reinterpret_cast<uint16_t*>(origin10->uv) + m_ctbPelX + (m_ctbPelY >> 1) * origin10->pitch_chroma_pix;
            //PF FIXME
            CopyNxN(originY10, origin10->pitch_luma_pix, m_ySrc10, 64);
            CopyNxM(originUv10, origin10->pitch_chroma_pix, m_uvSrc10, 64, 32);
        }

        m_coeffWorkY = m_coeffWork;
        m_coeffWorkU = m_coeffWorkY + m_par->MaxCUSize * m_par->MaxCUSize;
        m_coeffWorkV = m_coeffWorkU + (m_par->MaxCUSize * m_par->MaxCUSize >> m_par->chromaShift);

        // limits to clip MV allover the CU
        const int32_t MvShift = 3;
        const int32_t offset = 8;
        HorMax = (int16_t)std::min((m_par->Width + offset - m_ctbPelX - 1) << MvShift, int32_t(std::numeric_limits<int16_t>::max()));
        HorMin = (int16_t)std::max((-m_par->MaxCUSize - offset - m_ctbPelX + 1) << MvShift, int32_t(std::numeric_limits<int16_t>::min()));
        VerMax = (int16_t)std::min((m_par->Height + offset - m_ctbPelY - 1) << MvShift, int32_t(std::numeric_limits<int16_t>::max()));
        VerMin = (int16_t)std::max((-m_par->MaxCUSize - offset - m_ctbPelY + 1) << MvShift, int32_t(std::numeric_limits<int16_t>::min()));

        std::fill_n(m_costStored, sizeof(m_costStored) / sizeof(m_costStored[0]), COST_MAX);
        m_costCurr = 0.f;

        if (par.UseDQP) {
            m_lumaQp = m_lcuQps[m_ctbAddr];
            m_chromaQp = m_lcuQps[m_ctbAddr];
            if ((par.DeltaQpMode & AMT_DQP_CAQ)) {
                // save offs
                m_rdLambda = frame.m_lambda;
                m_rdLambdaSqrt = frame.m_lambdaSatd;
                m_rdLambdaSqrtInt = frame.m_lambdaSatdInt;
                m_rdLambdaSqrtInt32 = uint32_t(frame.m_lambdaSatd * 2048 + 0.5);
            } else {
                int16_t q = vp9_dc_quant(m_lumaQp, 0, par.bitDepthLuma);
                m_rdLambda = 88 * q * q / 24.f / 512 / 16 / 128;
                m_rdLambdaSqrt = sqrt(m_rdLambda * 512) / 512;
                m_rdLambdaSqrtInt = int(m_rdLambdaSqrt * (1 << 24));
                m_rdLambdaSqrtInt32 = uint32_t(m_rdLambdaSqrt * 2048 + 0.5);
            }
        } else {
            m_lumaQp = frame.m_sliceQpY;
            m_chromaQp = frame.m_sliceQpY;
            m_rdLambda = frame.m_lambda;
            m_rdLambdaSqrt = frame.m_lambdaSatd;
            m_rdLambdaSqrtInt = frame.m_lambdaSatdInt;
            m_rdLambdaSqrtInt32 = uint32_t(frame.m_lambdaSatd * 2048 + 0.5);
            // m_ChromaDistWeight = m_cslice->ChromaDistWeight_slice;
            // m_rdLambdaChroma = m_rdLambda*(3.0+1.0/m_ChromaDistWeight)/4.0;
        }

        m_tileBorders = GetTileBordersMi(frame.m_tileParam, ctbRow << 3, ctbCol << 3);
        const int32_t ctbColWithinTile = ctbCol - (m_tileBorders.colStart >> 3);
        const int32_t ctbRowWithinTile = ctbRow - (m_tileBorders.rowStart >> 3);
        const int32_t lastctbColWithinTile = ((m_tileBorders.colEnd + 7) >> 3) - 1 - (m_tileBorders.colStart >> 3);
        const int32_t lastctbRowWithinTile = ((m_tileBorders.rowEnd + 7) >> 3) - 1 - (m_tileBorders.rowStart >> 3);

        // Setting neighbor CU
        m_availForPred.curr = m_data;
        m_availForPred.left = NULL;
        m_availForPred.above = NULL;
        m_availForPred.aboveLeft = NULL;
        m_availForPred.colocated = NULL;
        if (m_currFrame->m_prevFrame)
            m_availForPred.colocated = m_currFrame->m_prevFrame->m_modeInfo + (ctbCol << 3) + (ctbRow << 3) * m_par->miPitch;
        if (ctbColWithinTile)
            m_availForPred.left = m_data - 256;
        if (ctbRowWithinTile)
            m_availForPred.above = m_data - (m_par->PicWidthInCtbs << 8);
        if (m_availForPred.left && m_availForPred.above)
            m_availForPred.aboveLeft = m_availForPred.above - 256;

        int32_t tile_index  = GetTileIndex(frame.m_tileParam, ctbRow, ctbCol);
        const TileContexts &tileCtx = frame.m_tileContexts[ tile_index ];
        As16B(m_contexts.aboveTxfm) = As16B(tileCtx.aboveTxfm + (ctbColWithinTile << 4));
        As16B(m_contexts.leftTxfm) = As16B(tileCtx.leftTxfm + (ctbRowWithinTile << 4));
        if (!m_availForPred.above) {
            Zero(m_contexts.aboveNonzero);
            Zero(m_contexts.abovePartition);
        }
        else {
            As16B(m_contexts.aboveNonzero[0]) = As16B(tileCtx.aboveNonzero[0] + (ctbColWithinTile << 4));
            As8B(m_contexts.aboveNonzero[1]) = As8B(tileCtx.aboveNonzero[1] + (ctbColWithinTile << 3));
            As8B(m_contexts.aboveNonzero[2]) = As8B(tileCtx.aboveNonzero[2] + (ctbColWithinTile << 3));
            As8B(m_contexts.abovePartition) = As8B(tileCtx.abovePartition + (ctbColWithinTile << 3));
        }

        if (!m_availForPred.left) {
            Zero(m_contexts.leftNonzero);
            Zero(m_contexts.leftPartition);
        }
        else {
            As16B(m_contexts.leftNonzero[0]) = As16B(tileCtx.leftNonzero[0] + (ctbRowWithinTile << 4));
            As8B(m_contexts.leftNonzero[1]) = As8B(tileCtx.leftNonzero[1] + (ctbRowWithinTile << 3));
            As8B(m_contexts.leftNonzero[2]) = As8B(tileCtx.leftNonzero[2] + (ctbRowWithinTile << 3));
            As8B(m_contexts.leftPartition) = As8B(tileCtx.leftPartition + (ctbRowWithinTile << 3));
        }

        m_contextsInitSb = m_contexts;
        // Hardcoded without dqp, can support dqp with depth 0 (SB level)
        m_aqparamY = m_par->qparamY[m_lumaQp];
        m_aqparamUv[0] = m_par->qparamUv[m_chromaQp];
        m_aqparamUv[1] = m_par->qparamUv[m_chromaQp];
#ifdef ADAPTIVE_DEADZONE
        int32_t frmclass = (frame.IsIntra() ? 0 : (((m_par->BiPyramidLayers > 1 && frame.m_pyramidLayer >= 3) || frame.IsNotRef()) ? 2 : 1));
        float Dz0 = ((frmclass == 0) ? 48.0f : ((frmclass == 1) ? 44.0f : 40.0f));
        float Dz1 = ((frmclass == 0) ? 48.0f : ((frmclass == 1) ? 40.0f : 40.0f));

#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
        const int32_t DZTW = tileCtx.tileSb64Cols;
#else
        const int32_t DZTW = 3;
#endif
        ADzCtx *currDz = &tileCtx.adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile];

#if 1 //defined(ADAPTIVE_DEADZONE_TEMPORAL_ONLY)
        Frame *temporalFrame = frame.IsIntra()
            ? NULL
            : ((m_currFrame->m_picCodeType == MFX_FRAMETYPE_P)
                ?
#ifndef LOWDELAY_SEQ
                ((m_currFrame->refFramesVp9[LAST_FRAME]->m_pyramidLayer > m_currFrame ->m_pyramidLayer)
                    ? m_currFrame->refFramesVp9[ALTREF_FRAME]
                    : m_currFrame->refFramesVp9[LAST_FRAME])
#else
                m_currFrame->refFramesVp9[LAST_FRAME]
#endif
                : ((m_currFrame->refFramesVp9[LAST_FRAME]->m_pyramidLayer > m_currFrame->refFramesVp9[ALTREF_FRAME]->m_pyramidLayer)
                    ? m_currFrame->refFramesVp9[LAST_FRAME]
                    : m_currFrame->refFramesVp9[ALTREF_FRAME]));
        bool useSpatial = (!temporalFrame || !m_currFrame->m_temporalSync || temporalFrame->IsIntra());
#else
        Frame *temporalFrame = frame.IsIntra() ? NULL : ((m_currFrame->m_picCodeType == MFX_FRAMETYPE_P) ? m_currFrame->refFramesVp9[LAST_FRAME] : NULL);
        bool useSpatial = (!temporalFrame || !m_currFrame->m_temporalSync || temporalFrame->m_picCodeType != MFX_FRAMETYPE_P);
#endif

        if (useSpatial) {
#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
            if (m_currFrame->m_picCodeType != MFX_FRAMETYPE_I) {
                currDz->SetDcAc(Dz0, Dz1);
            } else
#endif
            {
                if (!ctbColWithinTile && !ctbRowWithinTile) {
                    currDz->SetDcAc(Dz0, Dz1);
                } else if (!ctbColWithinTile && ctbRowWithinTile) {
                    *currDz = tileCtx.adzctx[1][ctbRowWithinTile - 1];  // above right sync
                } else {
                    *currDz = tileCtx.adzctx[std::min(ctbColWithinTile - 1, DZTW - 1)][ctbRowWithinTile];  // previous
                }
            }
        } else {
#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
            if (m_currFrame->m_picCodeType == MFX_FRAMETYPE_P && temporalFrame->m_picCodeType == MFX_FRAMETYPE_P
#ifndef LOWDELAY_SEQ
                && m_currFrame->m_pyramidLayer == temporalFrame->m_pyramidLayer
#endif
                ) {
                float varDz = (m_par->SceneCut) ? 10.0f : 8.0f; // only temporal
                ADzCtx *prevDz = &temporalFrame->m_tileContexts[tile_index].adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile];
                *currDz = *prevDz; // Copy
                // Add
                if (ctbColWithinTile) {
                    ADzCtx *PDz = &temporalFrame->m_tileContexts[tile_index].adzctx[std::min(ctbColWithinTile - 1, DZTW - 1)][ctbRowWithinTile];
                    ADzCtx *DDz = &temporalFrame->m_tileContexts[tile_index].adzctxDelta[std::min(ctbColWithinTile - 1, DZTW - 1)][ctbRowWithinTile];
                    //currDz->Add(DDz->aqroundFactorY, DDz->aqroundFactorUv);
                    currDz->AddWieghted(DDz->aqroundFactorY, DDz->aqroundFactorUv, PDz->aqroundFactorY, PDz->aqroundFactorUv);
                }
                if (ctbRowWithinTile) {
                    ADzCtx *PDz = &temporalFrame->m_tileContexts[tile_index].adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile - 1];
                    ADzCtx *DDz = &temporalFrame->m_tileContexts[tile_index].adzctxDelta[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile - 1];
                    //currDz->Add(DDz->aqroundFactorY, DDz->aqroundFactorUv);
                    currDz->AddWieghted(DDz->aqroundFactorY, DDz->aqroundFactorUv, PDz->aqroundFactorY, PDz->aqroundFactorUv);
                }
                if (ctbColWithinTile < lastctbColWithinTile && ctbRowWithinTile) {
                    ADzCtx *PDz = &temporalFrame->m_tileContexts[tile_index].adzctx[std::min(ctbColWithinTile + 1, DZTW - 1)][ctbRowWithinTile -1];
                    ADzCtx *DDz = &temporalFrame->m_tileContexts[tile_index].adzctxDelta[std::min(ctbColWithinTile + 1, DZTW - 1)][ctbRowWithinTile -1];
                    //currDz->Add(DDz->aqroundFactorY, DDz->aqroundFactorUv);
                    currDz->AddWieghted(DDz->aqroundFactorY, DDz->aqroundFactorUv, PDz->aqroundFactorY, PDz->aqroundFactorUv);
                }
                if (ctbColWithinTile < lastctbColWithinTile && ctbRowWithinTile < lastctbRowWithinTile) {
                    ADzCtx *PDz = &temporalFrame->m_tileContexts[tile_index].adzctx[std::min(ctbColWithinTile + 1, DZTW - 1)][ctbRowWithinTile + 1];
                    ADzCtx *DDz = &temporalFrame->m_tileContexts[tile_index].adzctxDelta[std::min(ctbColWithinTile + 1, DZTW - 1)][ctbRowWithinTile + 1];
                    //currDz->Add(DDz->aqroundFactorY, DDz->aqroundFactorUv);
                    currDz->AddWieghted(DDz->aqroundFactorY, DDz->aqroundFactorUv, PDz->aqroundFactorY, PDz->aqroundFactorUv);
                }
                currDz->SaturateAcDc(Dz0, Dz1, varDz); // Saturate
            } else {
                float varDz = 4.0f;
                ADzCtx *prevDz = &temporalFrame->m_tileContexts[tile_index].adzctx[std::min(ctbColWithinTile, DZTW - 1)][ctbRowWithinTile];
                currDz->SetDcAc(Dz0, Dz1); // Set
                currDz->SetMin(prevDz->aqroundFactorY, prevDz->aqroundFactorUv); // Use min
                currDz->SaturateAcDc(Dz0, Dz1, varDz); // Saturate
            }
#else
            if (!ctbColWithinTile) // coloc row sync
            {
                // For Sanity w/o Scene Change Detector and now w/ temporalSync
                float varDz = (m_par->SceneCut) ? 10.0f : 8.0f; // temporal
                ADzCtx *prevDz = &temporalFrame->m_tileContexts[tile_index].adzctx[DZTW - 1][ctbRowWithinTile];
                if (m_currFrame->m_pyramidLayer == temporalFrame->m_pyramidLayer) {
                    *currDz = *prevDz; // copy
                } else {
                    currDz->SetDcAc(Dz0, Dz1);
                    currDz->SetMin(prevDz->aqroundFactorY, prevDz->aqroundFactorUv); // Use min
                }
                currDz->SaturateAcDc(Dz0, Dz1, varDz);
            } else {
                *currDz = tileCtx.adzctx[std::min(ctbColWithinTile - 1, DZTW-1)][ctbRowWithinTile];  // previous
            }
#endif
        }

#if GPU_VARTX
        ADzCtx *gpuAdz = (ADzCtx *)m_currFrame->m_feiAdz->m_sysmem;
        ADzCtx *gpuAdzDelta = (ADzCtx *)m_currFrame->m_feiAdzDelta->m_sysmem;
        //if (!m_currFrame->IsIntra()) {
        //    //printf("initcu: %2d %2d %2d:  %.3f %.3f  %.3f %.3f\n", m_currFrame->m_poc, ctbRow, ctbCol,
        //    //    currDz->aqroundFactorY[0], gpuAdz[m_ctbAddr].aqroundFactorY[0] - gpuAdzDelta[m_ctbAddr].aqroundFactorY[0],
        //    //    currDz->aqroundFactorY[1], gpuAdz[m_ctbAddr].aqroundFactorY[1] - gpuAdzDelta[m_ctbAddr].aqroundFactorY[1]);
        //    currDz->aqroundFactorY[0] = gpuAdz[m_ctbAddr].aqroundFactorY[0] - gpuAdzDelta[m_ctbAddr].aqroundFactorY[0];
        //    currDz->aqroundFactorY[1] = gpuAdz[m_ctbAddr].aqroundFactorY[1] - gpuAdzDelta[m_ctbAddr].aqroundFactorY[1];
        //}
        gpuAdz[m_ctbAddr].aqroundFactorY[0] = currDz->aqroundFactorY[0];
        gpuAdz[m_ctbAddr].aqroundFactorY[1] = currDz->aqroundFactorY[1];
#endif // GPU_VARTX

#if USE_CMODEL_PAK
        *lcuRound = ((int32_t)(currDz->aqroundFactorY[1] + 1.0) >> 2) - 1;
        m_aqparamY.round[0] = ((*lcuRound + 1) * m_aqparamY.dequant[0]) >> 5;
        m_aqparamY.round[1] = ((*lcuRound + 1) * m_aqparamY.dequant[1]) >> 5;
        m_aqparamUv[0].round[0] = ((*lcuRound + 1) * m_aqparamUv[0].dequant[0]) >> 5;
        m_aqparamUv[0].round[1] = ((*lcuRound + 1) * m_aqparamUv[0].dequant[1]) >> 5;
        m_aqparamUv[1].round[0] = ((*lcuRound + 1) * m_aqparamUv[1].dequant[0]) >> 5;
        m_aqparamUv[1].round[1] = ((*lcuRound + 1) * m_aqparamUv[1].dequant[1]) >> 5;
#else
        m_aqparamY.round[0]     = (int16_t)(((int32_t)(currDz->aqroundFactorY[0] + 0.5) * m_aqparamY.dequant[0]) >> 7);
        m_aqparamY.round[1]     = (int16_t)(((int32_t)(currDz->aqroundFactorY[1] + 0.5) * m_aqparamY.dequant[1]) >> 7);
        m_aqparamUv[0].round[0] = (int16_t)(((int32_t)(currDz->aqroundFactorUv[0][0] + 0.5) * m_aqparamUv[0].dequant[0]) >> 7);
        m_aqparamUv[0].round[1] = (int16_t)(((int32_t)(currDz->aqroundFactorUv[0][1] + 0.5) * m_aqparamUv[0].dequant[1]) >> 7);
        m_aqparamUv[1].round[0] = (int16_t)(((int32_t)(currDz->aqroundFactorUv[1][0] + 0.5) * m_aqparamUv[1].dequant[0]) >> 7);
        m_aqparamUv[1].round[1] = (int16_t)(((int32_t)(currDz->aqroundFactorUv[1][1] + 0.5) * m_aqparamUv[1].dequant[1]) >> 7);
#endif
        m_roundFAdjY[0]     = (currDz->aqroundFactorY[0] * (float)m_aqparamY.dequant[0]) / 128.0f;
        m_roundFAdjY[1]     = (currDz->aqroundFactorY[1] * (float)m_aqparamY.dequant[1]) / 128.0f;
        m_roundFAdjUv[0][0] = (currDz->aqroundFactorUv[0][0] * (float)m_aqparamUv[0].dequant[0]) / 128.0f;
        m_roundFAdjUv[0][1] = (currDz->aqroundFactorUv[0][1] * (float)m_aqparamUv[0].dequant[1]) / 128.0f;
        m_roundFAdjUv[1][0] = (currDz->aqroundFactorUv[1][0] * (float)m_aqparamUv[1].dequant[0]) / 128.0f;
        m_roundFAdjUv[1][1] = (currDz->aqroundFactorUv[1][1] * (float)m_aqparamUv[1].dequant[1]) / 128.0f;
#else
        if (frame.IsIntra()) {                                                                     // I
            m_roundFAdjY[0] = m_roundFAdjY[1] =  48.0f;
            m_roundFAdjUv[0][0] = m_roundFAdjUv[0][1] = m_roundFAdjUv[1][0] = m_roundFAdjUv[1][1] = 48.0f;
            m_aqparamY.round[0] = (48 * m_aqparamY.dequant[0]) >> 7;
            m_aqparamY.round[1] = (48 * m_aqparamY.dequant[1]) >> 7;
            m_aqparamUv[0].round[0] = m_aqparamUv[1].round[0] = (48 * m_aqparamUv[0].dequant[0]) >> 7;
            m_aqparamUv[0].round[1] = m_aqparamUv[1].round[1] = (48 * m_aqparamUv[0].dequant[1]) >> 7;
#if USE_CMODEL_PAK
            *lcuRound = 11;
#endif
        } else if ((m_par->BiPyramidLayers > 1 && frame.m_pyramidLayer >= 3) || !frame.m_isRef) {  // NRefB
            m_roundFAdjY[0] = m_roundFAdjY[1] = 40.0f;
            m_roundFAdjUv[0][0] = m_roundFAdjUv[0][1] = m_roundFAdjUv[1][0] = m_roundFAdjUv[1][1] = 40.0f;
            m_aqparamY.round[0] = (40 * m_aqparamY.dequant[0]) >> 7;
            m_aqparamY.round[1] = (40 * m_aqparamY.dequant[1]) >> 7;
            m_aqparamUv[0].round[0] = m_aqparamUv[1].round[0] = (40 * m_aqparamUv[0].dequant[0]) >> 7;
            m_aqparamUv[0].round[1] = m_aqparamUv[1].round[1] = (40 * m_aqparamUv[0].dequant[1]) >> 7;
#if USE_CMODEL_PAK
            *lcuRound = 9;
#endif
        } else {                                                                                  // P/BRef
            m_roundFAdjY[0] = m_roundFAdjUv[0][0] = m_roundFAdjUv[1][0] = 44.0f;
            m_roundFAdjY[1] = m_roundFAdjUv[0][1] = m_roundFAdjUv[1][1] =  40.0f;
            m_aqparamY.round[0] = (44 * m_aqparamY.dequant[0]) >> 7;
            m_aqparamY.round[1] = (40 * m_aqparamY.dequant[1]) >> 7;
            m_aqparamUv[0].round[0] = m_aqparamUv[1].round[0] = (44 * m_aqparamUv[0].dequant[0]) >> 7;
            m_aqparamUv[0].round[1] = m_aqparamUv[1].round[1] = (40 * m_aqparamUv[0].dequant[1]) >> 7;
#if USE_CMODEL_PAK
            *lcuRound = 9;
#endif
        }
#endif
        m_SCid[0][0]    = 5;
        m_SCpp[0][0]    = 1764.0;
        m_STC[0][0]     = 2;
        m_mvdMax  = 128;
        m_mvdCost = 60;
        //GetSpatialComplexity(0, 0, 0, 0);
        m_intraMinDepth = 0;
        m_adaptMaxDepth = MAX_TOTAL_DEPTH;
        m_projMinDepth = 0;
        m_adaptMinDepth = 0;
        //if (m_par->minCUDepthAdapt)
        //    GetAdaptiveMinDepth(0, 0);

        if (!frame.IsIntra()) {
            //CalculateZeroMvPredAndSatd();
            if (m_par->enableCmFlag)
                CalculateNewMvPredAndSatd();
        }

#if ENABLE_SW_ME
        MemoizeInit();
#endif
    }



    BlockSize GetPlaneBlockSize(BlockSize subsize, int32_t plane, const AV1VideoParam &par) {
        int32_t subx = plane > 0 ? par.subsamplingX : 0;
        int32_t suby = plane > 0 ? par.subsamplingY : 0;
        return ss_size_lookup[subsize][subx][suby];
    }

    TxSize GetUvTxSize(int32_t miSize, TxSize txSize, const AV1VideoParam &par) {
        if (miSize < BLOCK_4X4)
            return TX_4X4;
        return TxSize(std::min(txSize, max_txsize_lookup[GetPlaneBlockSize(AV1Enc::BlockSize(miSize), 1, par)]));
    }

    template <typename PixType>
    uint8_t AV1CU<PixType>::GetAdaptiveIntraMinDepth(int32_t absPartIdx, int32_t depth, int32_t *maxSC)
    {
        const float delta_sc[10] = {12.0, 39.0, 81.0, 168.0, 268.0, 395.0, 553.0, 744.0, 962.0, 962.0};
        uint8_t intraMinDepth = 0;
        int32_t subSC[4];
        float subSCpp[4];
        intraMinDepth = 0;
        *maxSC = 0;
        uint32_t numParts = (m_par->NumPartInCU >> (depth << 1));
        float bitDepthMult = static_cast<float>(1 << (m_par->bitDepthLumaShift * 2));
        for (int32_t i = 0; i < 4; i++) {
            int32_t partAddr = (numParts >> 2) * i;
            subSC[i] = GetSpatialComplexity(absPartIdx, depth, partAddr, depth+1, subSCpp+i);
            if (subSC[i] > *maxSC)
                *maxSC = subSC[i];
            for (int32_t j = i-1; j >= 0; j--)
                if (fabsf(subSCpp[i]-subSCpp[j]) > delta_sc[std::min(subSC[i], subSC[j])] * bitDepthMult)
                    intraMinDepth = 1;
        }
        // absolutes & overrides
        if (IsTuSplitIntra() && ((m_par->Log2MaxCUSize-depth) <= 5 || m_par->QuadtreeTUMaxDepthIntra > 2)) {
            if (*maxSC < 5)
                intraMinDepth = 0;
        } else {
            if (*maxSC >= m_par->IntraMinDepthSC)  intraMinDepth = 1;
            if (*maxSC < 2)                        intraMinDepth = 0;
        }
        return intraMinDepth += depth;
    }


    template <typename PixType>
    void AV1CU<PixType>::GetAdaptiveMinDepth(int32_t absPartIdx, int32_t depth)
    {
        int32_t width  = m_par->MaxCUSize >> depth;
        int32_t height = m_par->MaxCUSize >> depth;
        int32_t posx  = ((av1_scan_z2r4[absPartIdx] & 15) << LOG2_MIN_TU_SIZE);
        int32_t posy  = ((av1_scan_z2r4[absPartIdx] >> 4) << LOG2_MIN_TU_SIZE);
        m_intraMinDepth = 0;

        int32_t maxSC = m_SCid[depth][absPartIdx];
        if ((m_ctbPelX + posx + width) > m_par->Width || (m_ctbPelY + posy + height) > m_par->Height) {
            m_intraMinDepth = 1;
        } else {
            m_intraMinDepth = GetAdaptiveIntraMinDepth(absPartIdx, depth, &maxSC);
        }

        if (m_currFrame->m_refPicList[0].m_refFramesCount == 0 && m_currFrame->m_refPicList[1].m_refFramesCount) {
            m_adaptMinDepth = m_intraMinDepth;

            if (m_adaptMinDepth == 0 && (m_par->Log2MaxCUSize-depth) > 5) {
                m_adaptMaxDepth = m_SCid[depth][absPartIdx]+((maxSC != m_SCid[depth][absPartIdx])?1:0)+(m_currFrame->m_isRef);
            }

            return;
        }
        uint8_t interMinDepthColSTC = 0;
        int32_t STC = 2;

        if ((m_par->Log2MaxCUSize-depth) > 5) {
            FrameData *ref0 = m_currFrame->m_refPicList[0].m_refFrames[0]->m_recon;
            STC = GetSpatioTemporalComplexityColocated(absPartIdx, depth, 0, depth, ref0);
            if (STC >= m_par->InterMinDepthSTC && m_intraMinDepth > depth) interMinDepthColSTC = 1;
        }

        uint8_t interMinDepthColCu = 0;
        const RefPicList *refLists = m_currFrame->m_refPicList;
        int32_t cuDataColOffset = (m_ctbAddr << m_par->Log2NumPartInCU);

        int8_t qpCur = m_lcuQps[m_ctbAddr];
        int8_t qpPrev = qpCur;
        int32_t xLimit = (m_par->Width  - m_ctbPelX) >> LOG2_MIN_TU_SIZE;
        int32_t yLimit = (m_par->Height - m_ctbPelY) >> LOG2_MIN_TU_SIZE;

        uint8_t depthMin = 4;
        int32_t depthSum = 0;
        int32_t numParts = 0;

        for (int32_t list = 1; list >= 0; list--) {
            if (m_currFrame->m_refPicList[list].m_refFramesCount > 0) {
                Frame *ref0 = refLists[list].m_refFrames[0];
                const ModeInfo *ctbCol = ref0->m_modeInfo + cuDataColOffset;
                if (ref0->m_picCodeType == MFX_FRAMETYPE_I) continue;   // Spatial complexity does not equal temporal complexity -NG
                uint32_t i = 0;
                while (i < m_par->NumPartInCU) {
                    uint8_t colDepth = depth_lookup[ctbCol[i].sbType];
                    assert(colDepth <= 3);
                    if (depthMin > colDepth)
                        depthMin = colDepth;
                    int32_t num8x8parts = 64 >> (colDepth << 1);
                    depthSum += colDepth << (6 - 2 * colDepth);
                    numParts += num8x8parts;
                    i += num8x8parts << 2;
                    while (i < m_par->NumPartInCU && ((av1_scan_z2r4[i] & 15) >= xLimit || (av1_scan_z2r4[i] >> 4) >= yLimit))
                        i += num8x8parts << 2;
                }
                assert(i == m_par->NumPartInCU);
            }
        }
        if (!numParts) {
            interMinDepthColCu = 0;
        } else {
            float depthAvg = (float)depthSum / numParts;

            uint8_t depthDelta = 1;
            if (qpCur - qpPrev < 0 || (qpCur - qpPrev >= 0 && depthAvg - depthMin > 0.5f))
                depthDelta = 0;

            interMinDepthColCu = (depthMin > 0) ? uint8_t(depthMin - depthDelta) : depthMin;
        }


        if (STC < 4) {
            m_adaptMinDepth = std::max(interMinDepthColCu, interMinDepthColSTC);   // Copy
        } else {
            m_adaptMinDepth = interMinDepthColSTC;                                // Reset
        }
        if (m_adaptMinDepth == 0 && (m_par->Log2MaxCUSize-depth) > 5) {
            m_adaptMaxDepth = m_SCid[depth][absPartIdx]+((maxSC != m_SCid[depth][absPartIdx])?1:0)+(m_currFrame->m_isRef);
        }
    }


    template <typename PixType>
    ModeInfo *AV1CU<PixType>::GetCuDataXY(int32_t x, int32_t y, Frame *ref)
    {
        ModeInfo *cu = NULL;
        if (x < 0 || y < 0 || x >= (int32_t) m_par->Width  || y >= (int32_t) m_par->Height) return cu;
        if (!ref)                                                                         return cu;
        int32_t lcux     = x >> m_par->Log2MaxCUSize;
        int32_t lcuy     = y >> m_par->Log2MaxCUSize;
        uint32_t ctbAddr  = lcuy*m_par->PicWidthInCtbs+lcux;
        int32_t posx     = x - (lcux << m_par->Log2MaxCUSize);
        int32_t posy     = y - (lcuy << m_par->Log2MaxCUSize);
        int32_t absPartIdx = h265_scan_r2z4[(posy >> LOG2_MIN_TU_SIZE)*16+(posx >> LOG2_MIN_TU_SIZE)];
        cu = ref->m_modeInfo + (ctbAddr << m_par->Log2NumPartInCU) + absPartIdx;
        return cu;
    }

    template <typename PixType>
    void AV1CU<PixType>::GetProjectedDepth(int32_t absPartIdx, int32_t depth, uint8_t allowSplit)
    {
        const ModeInfo *dataBestInter = GetBestCuDecision(absPartIdx, depth);
        uint8_t layer = (m_par->BiPyramidLayers > 1) ? m_currFrame->m_pyramidLayer : 0;
        bool highestLayerBiFrame = (m_par->BiPyramidLayers > 1 && layer == m_par->BiPyramidLayers - 1);
        int32_t numParts = (m_par->NumPartInCU >> (depth << 1) );

        if (depth == 0 && !highestLayerBiFrame && dataBestInter->skip != 1 && allowSplit)
        {
            int32_t minDepth0 = MAX_TOTAL_DEPTH;
            int32_t minDepth1 = MAX_TOTAL_DEPTH;
            int32_t foundDepth0 = 0;
            int32_t foundDepth1 = 0;
            int8_t qpCur = m_lcuQps[m_ctbAddr];
            int8_t qp0 = 52;
            int8_t qp1 = 52;
            const RefPicList *refPicList = m_currFrame->m_refPicList;

            for (int32_t i = 0; i < numParts; i += 4) {
                int32_t posx = (uint8_t)((av1_scan_z2r4[absPartIdx+i] & 15) << LOG2_MIN_TU_SIZE);
                int32_t posy = (uint8_t)((av1_scan_z2r4[absPartIdx+i] >> 4) << LOG2_MIN_TU_SIZE);
                if (dataBestInter[i].refIdx[0] > NONE_FRAME) {
                    Frame *ref0 = refPicList[0].m_refFrames[dataBestInter[i].refIdx[0]];
                    if (ref0->m_picCodeType != MFX_FRAMETYPE_I) {
                        posx += dataBestInter[i].mv[0].mvx/4;
                        posy += dataBestInter[i].mv[0].mvy/4;
                        ModeInfo *cu0 = GetCuDataXY(m_ctbPelX+posx, m_ctbPelY+posy, ref0);
                        if (cu0) {
                            if (depth_lookup[cu0->sbType] < minDepth0) minDepth0 = depth_lookup[cu0->sbType];
                            foundDepth0++;
                        }
                    }
                }
            }
            if (minDepth0 == MAX_TOTAL_DEPTH || foundDepth0 < numParts/5) minDepth0 = 0;
            if (layer == 0 && qp0 < qpCur) minDepth0 = 0;

            if (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B) {
                for (int32_t i = 0; i < numParts; i += 4) {
                    int32_t posx = (uint8_t)((av1_scan_z2r4[absPartIdx+i] & 15) << LOG2_MIN_TU_SIZE);
                    int32_t posy = (uint8_t)((av1_scan_z2r4[absPartIdx+i] >> 4) << LOG2_MIN_TU_SIZE);
                    if (dataBestInter[i].refIdx[1] > NONE_FRAME) {
                        Frame *ref1 = refPicList[1].m_refFrames[dataBestInter[i].refIdx[1]];
                        if (ref1->m_picCodeType != MFX_FRAMETYPE_I) {
                            posx += dataBestInter[i].mv[1].mvx/4;
                            posy += dataBestInter[i].mv[1].mvy/4;
                            ModeInfo *cu1 = GetCuDataXY(m_ctbPelX+posx, m_ctbPelY+posy, ref1);
                            if (cu1) {
                                if (depth_lookup[cu1->sbType] < minDepth1) minDepth1 = depth_lookup[cu1->sbType];
                                foundDepth1++;
                            }
                        }
                    }
                }
            }
            if (minDepth1 == MAX_TOTAL_DEPTH || foundDepth1 < numParts/5) minDepth1 = 0;
            if (layer == 0 && qp1 < qpCur) minDepth0 = 0;

            if (foundDepth0 && foundDepth1) {
                m_projMinDepth = (uint8_t)(std::min(minDepth0, minDepth1));
            } else if (foundDepth0) {
                m_projMinDepth = (uint8_t)minDepth0;
            } else {
                m_projMinDepth = (uint8_t)minDepth1;
            }
            if (m_projMinDepth && (layer || m_STC[depth][absPartIdx] < 1 || m_STC[depth][absPartIdx] > 3))
                m_projMinDepth--;  // Projected from diff layer or change in complexity or very high complexity
        }

        if (depth == 0 && dataBestInter->skip != 1 && allowSplit && m_STC[depth][absPartIdx] < 2)
        {
            int32_t maxDepth0 = 0;
            int32_t maxDepth1 = 0;
            int32_t foundDepth0 = 0;
            int32_t foundDepth1 = 0;
            int8_t qpCur = m_lcuQps[m_ctbAddr];
            int8_t qp0 = 0;
            int8_t qp1 = 0;
            const RefPicList *refPicList = m_currFrame->m_refPicList;

            for (int32_t i = 0; i < numParts; i += 4) {
                int32_t posx = (uint8_t)((av1_scan_z2r4[absPartIdx+i] & 15) << LOG2_MIN_TU_SIZE);
                int32_t posy = (uint8_t)((av1_scan_z2r4[absPartIdx+i] >> 4) << LOG2_MIN_TU_SIZE);
                if (dataBestInter[i].refIdx[0] == 0) {
                    Frame *ref0 = refPicList[0].m_refFrames[dataBestInter[i].refIdx[0]];
                    posx += dataBestInter[i].mv[0].mvx/4;
                    posy += dataBestInter[i].mv[0].mvy/4;
                    ModeInfo *cu0 = GetCuDataXY(m_ctbPelX+posx, m_ctbPelY+posy, ref0);
                    if (cu0) {
                        if (depth_lookup[cu0->sbType] > maxDepth0) maxDepth0 = depth_lookup[cu0->sbType];
                        foundDepth0++;
                    }
                }
            }
            if (maxDepth0 == 0 || foundDepth0 < numParts/5 || qp0 > qpCur) maxDepth0 = MAX_TOTAL_DEPTH;
            if (qp0 == qpCur) maxDepth0++;

            if (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B) {
                for (int32_t i = 0; i < numParts; i += 4) {
                    int32_t posx = (uint8_t)((av1_scan_z2r4[absPartIdx+i] & 15) << LOG2_MIN_TU_SIZE);
                    int32_t posy = (uint8_t)((av1_scan_z2r4[absPartIdx+i] >> 4) << LOG2_MIN_TU_SIZE);
                    if (dataBestInter[i].refIdx[1] == 0) {
                        Frame *ref1 = refPicList[1].m_refFrames[dataBestInter[i].refIdx[1]];
                        posx += dataBestInter[i].mv[1].mvx/4;
                        posy += dataBestInter[i].mv[1].mvy/4;
                        ModeInfo *cu1 = GetCuDataXY(m_ctbPelX+posx, m_ctbPelY+posy, ref1);
                        if (cu1) {
                            if (depth_lookup[cu1->sbType] > maxDepth1) maxDepth1 = depth_lookup[cu1->sbType];
                            foundDepth1++;
                        }
                    }
                }
            }
            if (maxDepth1 == 0 || foundDepth1 < numParts/5 || qp1 > qpCur) maxDepth1 = MAX_TOTAL_DEPTH;
            if (qp1 == qpCur) maxDepth1++;

            m_adaptMaxDepth = std::min((int32_t)m_adaptMaxDepth, std::min(maxDepth0, maxDepth1));
        }
    }

    template <typename PixType>
    void AV1CU<PixType>::GetSpatialComplexity(int32_t absPartIdx, int32_t depth, int32_t partAddr, int32_t partDepth)
    {
        float SCpp = 0;
        m_SCid[depth][absPartIdx] = GetSpatialComplexity(absPartIdx, depth, partAddr, partDepth, &SCpp);
        m_SCpp[depth][absPartIdx] = SCpp;
    }

    // lower limit of SFM(Rs,Cs) range for spatial classification
    static const float lmt_sc2[10] = {
        16.0, 81.0, 225.0, 529.0, 1024.0, 1764.0, 2809.0, 4225.0, 6084.0, static_cast<float>(INT_MAX)
    };

    template <typename PixType>
    int32_t AV1CU<PixType>::GetSpatialComplexity(int32_t absPartIdx, int32_t depth, int32_t partAddr, int32_t partDepth, float *SCpp) const
    {
        int32_t width  = m_par->MaxCUSize >> partDepth;
        int32_t height = m_par->MaxCUSize >> partDepth;
        int32_t posx  = ((av1_scan_z2r4[absPartIdx+partAddr] & 15) << LOG2_MIN_TU_SIZE);
        int32_t posy  = ((av1_scan_z2r4[absPartIdx+partAddr] >> 4) << LOG2_MIN_TU_SIZE);

        if (m_ctbPelX + posx + width > m_par->Width)
            width = m_par->Width - posx - m_ctbPelX;
        if (m_ctbPelY + posy + height > m_par->Height)
            height = m_par->Height - posy - m_ctbPelY;

        int32_t log2BlkSize = m_par->Log2MaxCUSize - partDepth;
        int32_t pitchRsCs = m_currFrame->m_stats[0]->m_pitchRsCs[log2BlkSize - 2];
        int32_t idx = ((m_ctbPelY+posy) >> log2BlkSize)*pitchRsCs + ((m_ctbPelX+posx) >> log2BlkSize);
        int32_t Rs2 = m_currFrame->m_stats[0]->m_rs[log2BlkSize-2][idx];
        int32_t Cs2 = m_currFrame->m_stats[0]->m_cs[log2BlkSize-2][idx];
        int32_t RsCs2 = Rs2 + Cs2;
        *SCpp = RsCs2 * h265_reci_1to116[(width >> 2) - 1] * h265_reci_1to116[(height >> 2) - 1];

        const float bitDepthMult = static_cast<float>(1 << (m_par->bitDepthLumaShift * 2));
        int32_t scVal = 5;
        for (int32_t i = 0; i < 10; i++) {
            if (*SCpp < lmt_sc2[i]*bitDepthMult)  {
                scVal = i;
                break;
            }
        }
        return scVal;
    }

    template <typename PixType>
    int32_t AV1CU<PixType>::GetSpatialComplexity(int32_t posx, int32_t posy, int32_t log2w, float *SCpp) const
    {
        int32_t width =  1 << (log2w+2);
        int32_t height = width;

        if (m_ctbPelX + posx + width > m_par->Width)
            width = m_par->Width - posx - m_ctbPelX;
        if (m_ctbPelY + posy + height > m_par->Height)
            height = m_par->Height - posy - m_ctbPelY;

        int32_t pitchRsCs = m_currFrame->m_stats[0]->m_pitchRsCs[log2w];
        int32_t idx = ((m_ctbPelY + posy) >> (log2w+2))*pitchRsCs + ((m_ctbPelX + posx) >> (log2w + 2));
        int32_t Rs2 = m_currFrame->m_stats[0]->m_rs[log2w][idx];
        int32_t Cs2 = m_currFrame->m_stats[0]->m_cs[log2w][idx];
        int32_t RsCs2 = Rs2 + Cs2;
        *SCpp = RsCs2 * h265_reci_1to116[(width >> 2) - 1] * h265_reci_1to116[(height >> 2) - 1];

        const float bitDepthMult = static_cast<float>(1 << (m_par->bitDepthLumaShift * 2));
        int32_t scVal = 5;
        for (int32_t i = 0; i < 10; i++) {
            if (*SCpp < lmt_sc2[i] * bitDepthMult) {
                scVal = i;
                break;
            }
        }
        return scVal;
    }


    template <typename PixType>
    int32_t AV1CU<PixType>::GetSpatioTemporalComplexity(int32_t absPartIdx, int32_t depth, int32_t partAddr, int32_t partDepth)
    {
        int32_t width  = m_par->MaxCUSize >> partDepth;
        int32_t height = m_par->MaxCUSize >> partDepth;
        int32_t posx  = ((av1_scan_z2r4[absPartIdx+partAddr] & 15) << LOG2_MIN_TU_SIZE);
        int32_t posy  = ((av1_scan_z2r4[absPartIdx+partAddr] >> 4) << LOG2_MIN_TU_SIZE);

        if (m_ctbPelX + posx + width > m_par->Width)
            width = m_par->Width - posx - m_ctbPelX;
        if (m_ctbPelY + posy + height > m_par->Height)
            height = m_par->Height - posy - m_ctbPelY;

        float SCpp = m_SCpp[depth][absPartIdx];

        int32_t offsetLuma = posx + posy * SRC_PITCH;
        PixType *pSrc = m_ySrc + offsetLuma;
        int32_t offsetPred = posx + posy * MAX_CU_SIZE;
        const PixType *predY = m_interPredY + offsetPred;
        int32_t sad = AV1PP::sad_special(pSrc, SRC_PITCH, predY, width, height);
        float sadpp = sad * h265_reci_1to116[(width >> 2)-1] * h265_reci_1to116[(height >> 2)-1] * (1.0f / 16);
        sadpp = (sadpp*sadpp);

        if     (sadpp < 0.09f*SCpp )  return 0;  // Very Low
        else if (sadpp < 0.36f*SCpp )  return 1;  // Low
        else if (sadpp < 1.44f*SCpp )  return 2;  // Medium
        else if (sadpp < 3.24f*SCpp )  return 3;  // High
        else                         return 4;  // VeryHigh
    }
    template <typename PixType>
    int32_t AV1CU<PixType>::GetSpatioTemporalComplexityColocated(int32_t absPartIdx, int32_t depth, int32_t partAddr,
        int32_t partDepth, FrameData *ref)
    {
        int32_t width  = m_par->MaxCUSize >> partDepth;
        int32_t height = m_par->MaxCUSize >> partDepth;
        int32_t posx  = ((av1_scan_z2r4[absPartIdx+partAddr] & 15) << LOG2_MIN_TU_SIZE);
        int32_t posy  = ((av1_scan_z2r4[absPartIdx+partAddr] >> 4) << LOG2_MIN_TU_SIZE);


        if (m_ctbPelX + posx + width > m_par->Width)
            width = m_par->Width - posx - m_ctbPelX;
        if (m_ctbPelY + posy + height > m_par->Height)
            height = m_par->Height - posy - m_ctbPelY;

        int32_t log2BlkSize = m_par->Log2MaxCUSize - partDepth;
        int32_t pitchRsCs = m_currFrame->m_stats[0]->m_pitchRsCs[log2BlkSize - 2];
        int32_t idx = ((m_ctbPelY+posy) >> log2BlkSize)*pitchRsCs + ((m_ctbPelX+posx) >> log2BlkSize);
        int32_t Rs2 = m_currFrame->m_stats[0]->m_rs[log2BlkSize-2][idx];
        int32_t Cs2 = m_currFrame->m_stats[0]->m_cs[log2BlkSize-2][idx];
        int32_t RsCs2 = Rs2 + Cs2;
        float SCpp = RsCs2 * h265_reci_1to116[(width >> 2) - 1] * h265_reci_1to116[(height >> 2) - 1];

        int32_t offsetLuma = posx + posy * SRC_PITCH;
        PixType *pSrc = m_ySrc + offsetLuma;

        int32_t refOffset = m_ctbPelX + posx + (m_ctbPelY + posy) * ref->pitch_luma_pix;
        PixType *predY = reinterpret_cast<PixType*>(ref->y) + refOffset;
        int32_t sad = AV1PP::sad_general(pSrc, SRC_PITCH, predY, ref->pitch_luma_pix, width, height);
        float sadpp = sad * h265_reci_1to116[(width >> 2)-1] * h265_reci_1to116[(height >> 2)-1] * (1.0f / 16);
        sadpp = (sadpp*sadpp);

        if     (sadpp < 0.09f*SCpp )  return 0;  // Very Low
        else if (sadpp < 0.36f*SCpp )  return 1;  // Low
        else if (sadpp < 1.44f*SCpp )  return 2;  // Medium
        else if (sadpp < 3.24f*SCpp )  return 3;  // High
        else                         return 4;  // VeryHigh
    }

    template <typename PixType>
    int32_t AV1CU<PixType>::GetSpatioTemporalComplexity(int32_t absPartIdx, int32_t depth, int32_t partAddr, int32_t partDepth, int32_t *scVal)
    {
        int32_t width  = m_par->MaxCUSize >> partDepth;
        int32_t height = m_par->MaxCUSize >> partDepth;
        int32_t posx  = ((av1_scan_z2r4[absPartIdx+partAddr] & 15) << LOG2_MIN_TU_SIZE);
        int32_t posy  = ((av1_scan_z2r4[absPartIdx+partAddr] >> 4) << LOG2_MIN_TU_SIZE);

        if (m_ctbPelX + posx + width > m_par->Width)
            width = m_par->Width - posx - m_ctbPelX;
        if (m_ctbPelY + posy + height > m_par->Height)
            height = m_par->Height - posy - m_ctbPelY;

        int32_t log2BlkSize = m_par->Log2MaxCUSize - partDepth;
        int32_t pitchRsCs = m_currFrame->m_stats[0]->m_pitchRsCs[log2BlkSize - 2];
        int32_t idx = ((m_ctbPelY+posy) >> log2BlkSize)*pitchRsCs + ((m_ctbPelX+posx) >> log2BlkSize);
        int32_t Rs2 = m_currFrame->m_stats[0]->m_rs[log2BlkSize-2][idx];
        int32_t Cs2 = m_currFrame->m_stats[0]->m_cs[log2BlkSize-2][idx];
        int32_t RsCs2 = Rs2 + Cs2;
        float SCpp = RsCs2 * h265_reci_1to116[(width >> 2) - 1] * h265_reci_1to116[(height >> 2) - 1];

        const float bitDepthMult = static_cast<float>(1 << (m_par->bitDepthLumaShift * 2));
        for (int32_t i = 0; i < 10; i++) {
            if (SCpp < lmt_sc2[i] * bitDepthMult) {
                *scVal = i;
                break;
            }
        }

        int32_t offsetLuma = posx + posy*SRC_PITCH;
        PixType *pSrc = m_ySrc + offsetLuma;
        int32_t offsetPred = posx + posy * MAX_CU_SIZE;
        const PixType *predY = m_interPredY + offsetPred;
        int32_t sad = AV1PP::sad_special(pSrc, SRC_PITCH, predY, width, height);
        float sadpp = sad * h265_reci_1to116[(width >> 2)-1] * h265_reci_1to116[(height >> 2)-1] * (1.0f / 16);
        sadpp = sadpp*sadpp;

        if      (sadpp < 0.09f*SCpp)  return 0;  // Very Low
        else if (sadpp < 0.36f*SCpp)  return 1;  // Low
        else if (sadpp < 1.44f*SCpp)  return 2;  // Medium
        else if (sadpp < 3.24f*SCpp)  return 3;  // High
        else                          return 4;  // VeryHigh
    }

    template <typename PixType>
    bool AV1CU<PixType>::TuMaxSplitInterHasNonZeroCoeff(uint32_t absPartIdx, uint8_t trIdxMax)
    {
        uint8_t depth = depth_lookup[m_data[absPartIdx].sbType];
        uint32_t numParts = (m_par->NumPartInCU >> ((depth + 0) << 1) );  // in TU
        uint32_t num_tr_parts = (m_par->NumPartInCU >> ((depth + trIdxMax) << 1) );
        int32_t width = MAX_CU_SIZE >> (depth + trIdxMax);

        uint8_t nz[3] = {0};
        uint8_t nzt[3] = {};

        // for (uint32_t i = 0; i < numParts; i++) m_data[absPartIdx + i].trIdx = trIdxMax;
        for (uint32_t pos = 0; pos < numParts; ) {
            // nzt[0] = EncInterLumaTuGetBaseCBF(absPartIdx + pos, (absPartIdx + pos)*16, width);
            nz[0] |= nzt[0];
            pos += num_tr_parts;
            if (nz[0]) return true;
        }

        if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) && !m_par->chroma422) {
            // doesnot handle anything other than 4:2:0 for now -NG
            if (width == 4) {
                num_tr_parts <<= 2;
                width <<= 1;
            }
            for (uint32_t pos = 0; pos < numParts; ) {
                // EncInterChromaTuGetBaseCBF(absPartIdx + pos, ((absPartIdx + pos)*16) >> 2, width >> 1, &nzt[1]);
                nz[1] |= nzt[1];
                nz[2] |= nzt[2];
                pos += num_tr_parts;
                if (nz[1] || nz[2]) return true;
            }
        } else {
            nz[1] = nz[2] = 0;
        }
        return false;
    }

    template <typename PixType>
    bool AV1CU<PixType>::tryIntraICRA(int32_t absPartIdx, int32_t depth)
    {
        if (m_currFrame->m_picCodeType == MFX_FRAMETYPE_I) return true;
        // Inter
        int32_t widthCu = (m_par->MaxCUSize >> depth);
        uint32_t numParts = (m_par->NumPartInCU >> (depth << 1) );
        const ModeInfo *dataBestInter = GetBestCuDecision(absPartIdx, depth);
        // Motion Analysis
        int32_t mvdMax = 0, mvdCost = 0;

        for (int32_t i = 0; i < 1; i++) {
            int32_t partAddr = 0;
            if (!dataBestInter[partAddr].skip)
            {
                if (dataBestInter[partAddr].refIdx[0] > NONE_FRAME) {
                    mvdCost += MvCost(dataBestInter[partAddr].mvd[0].mvx);
                    mvdCost += MvCost(dataBestInter[partAddr].mvd[0].mvy);
                }
                if (dataBestInter[partAddr].refIdx[1] > NONE_FRAME) {
                    mvdCost += MvCost(dataBestInter[partAddr].mvd[1].mvx);
                    mvdCost += MvCost(dataBestInter[partAddr].mvd[1].mvy);
                }

                if (abs(dataBestInter[partAddr].mvd[0].mvx) > mvdMax)
                    mvdMax = abs(dataBestInter[partAddr].mvd[0].mvx);
                if (abs(dataBestInter[partAddr].mvd[0].mvy) > mvdMax)
                    mvdMax = abs(dataBestInter[partAddr].mvd[0].mvy);
                if (abs(dataBestInter[partAddr].mvd[1].mvx) > mvdMax)
                    mvdMax = abs(dataBestInter[partAddr].mvd[1].mvx);
                if (abs(dataBestInter[partAddr].mvd[1].mvy) > mvdMax)
                    mvdMax = abs(dataBestInter[partAddr].mvd[1].mvy);
            }
        }
        m_mvdMax = mvdMax;
        m_mvdCost = mvdCost;
        // CBF Condition
        bool tryIntra = true;
        if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
            int32_t idx422 = m_par->chroma422 ? (m_par->NumPartInCU >> (2 * depth + 1)) : 0;
            tryIntra = !dataBestInter->skip;
        } else {
            tryIntra = !dataBestInter->skip;
        }
        // CBF Check
        if (!tryIntra && m_SCid[depth][absPartIdx] < 2)
        {
            // check if all cbf are really zero for low scene complexity
            if (dataBestInter->skip == 1) {
                CoeffsType *residY = m_interResidY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
                const PixType *srcY = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchRecLuma);
                const PixType *predY = m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
                AV1PP::diff_nxn(srcY, 64, predY, 64, residY, 64, widthCu);
                if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) && !m_par->chroma422) {
                    const PixType *srcC = m_uvSrc + GetChromaOffset(m_par, absPartIdx, m_pitchRecChroma);
                    const PixType *predC = m_interPredC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv);
                    CoeffsType *residU = m_interResidU + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
                    CoeffsType *residV = m_interResidV + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
                    AV1PP::diff_nv12(srcC, 64, predC, 64, residU, residV, 64, widthCu >> m_par->chromaShiftH, BSR(widthCu) - 2);
                }
            }
            const uint8_t tr_depth_max = 4 - depth;
            tryIntra = TuMaxSplitInterHasNonZeroCoeff(absPartIdx, tr_depth_max);
        }
        // ICRA and Motion analysis
        if (tryIntra)
        {
            int32_t SCid = m_SCid[depth][absPartIdx];
            int32_t STC  = m_STC[depth][absPartIdx];
            int32_t subSC[4] = {SCid, SCid, SCid, SCid};
            int32_t subSTC[4] = {STC, STC, STC, STC};
            int32_t minSC    =  SCid;
            bool goodPred = false;
            bool tryProj = false;
            // 8x8 is also 4x4 intra
            if (widthCu == 8) {
                for (int32_t i = 0; i < 4; i++) {
                    int32_t partAddr = (numParts >> 2) * i;
                    subSTC[i] = GetSpatioTemporalComplexity(absPartIdx, depth, partAddr, depth+1, subSC+i);
                    if (subSC[i] < minSC) minSC = subSC[i];
                }
                goodPred = subSTC[0] < 1 && subSTC[1] < 1 && subSTC[2] < 1 && subSTC[3] < 1;
                tryProj  = subSTC[0] < 2 && subSTC[1] < 2 && subSTC[2] < 2 && subSTC[3] < 2;
            } else {
                goodPred = STC < 1;
                tryProj  = STC < 2;
            }

            if (minSC < 5 && mvdMax > 15)  {
                tryIntra = true;
            } else if (minSC < 6 && mvdMax > 31)  {
                tryIntra = true;
            } else if (minSC < 7 && mvdMax > 63)  {
                tryIntra = true;
            } else if (mvdMax > 127) {
                tryIntra = true;
            } else if (goodPred)  {
                tryIntra = false;
            } else if (tryProj)   {
                // Projection Analysis
                bool foundIntra = false;

                const RefPicList *refPicList = m_currFrame->m_refPicList;
                Frame *ref0 = refPicList[0].m_refFrames[0];
                int32_t cuDataColOffset = (m_ctbAddr << m_par->Log2NumPartInCU);
                const ModeInfo *col0 = refPicList[0].m_refFrames[0]->m_modeInfo + cuDataColOffset;
                Frame *ref1 = (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B)? refPicList[1].m_refFrames[0] : NULL;
                const ModeInfo *col1 = (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B)
                    ? refPicList[1].m_refFrames[0]->m_modeInfo + cuDataColOffset
                    : NULL;
                if (ref0 == ref1) {
                    if (ref0->m_picCodeType == MFX_FRAMETYPE_I) {
                        foundIntra = true;
                    } else {
                        for (int32_t i = absPartIdx; i < absPartIdx+(int32_t)numParts && !foundIntra; i ++) {
                            if (col0 && col0[i].mode < INTRA_MODES) foundIntra = true;
                        }
                    }
                } else if ((ref0&&ref0->m_picCodeType != MFX_FRAMETYPE_I) || (ref1&&ref1->m_picCodeType != MFX_FRAMETYPE_I)) {
                    for (int32_t i = absPartIdx; i < absPartIdx+(int32_t)numParts && !foundIntra; i ++) {
                        if (ref0->m_picCodeType != MFX_FRAMETYPE_I && col0 && col0[i].mode < INTRA_MODES) foundIntra = true;
                        if (ref1 && ref1->m_picCodeType != MFX_FRAMETYPE_I && col1 && col1[i].mode < INTRA_MODES) foundIntra = true;
                    }
                } else {
                    foundIntra = true;
                }
                if (foundIntra == false) tryIntra = false;
            }
        } else {
            if (mvdMax > 15) tryIntra = true;
        }
        return tryIntra;
    }





    template <typename PixType>
    bool AV1CU<PixType>::JoinCU(int32_t absPartIdx, int32_t depth)
    {
        const BlockSize bsz = GetSbType(depth, PARTITION_NONE);
        const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
        const int32_t x4 = (rasterIdx & 15);
        const int32_t y4 = (rasterIdx >> 4);
        const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
        const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t num4x4h = 1 << log2h;
        ModeInfo *mi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;
        uint8_t skipped = (depth_lookup[mi->sbType] == depth+1 &&
            mi->mode >= INTRA_MODES &&
            mi->skip) ? 1 : 0;
        PredMode predMode = NEWMV;
        int8_t refIdx[2] = {mi->refIdx[0], mi->refIdx[1]};
        AV1MV mv[2]    = {mi->mv[0]    , mi->mv[1]    };
        int32_t candBest = -1;
        uint32_t numParts = (m_par->NumPartInCU >> (2 * depth)) >> 2;
        int32_t absPartIdxSplit = absPartIdx + numParts;
        if (refIdx[0] >= 0 && refIdx[1] >= 0) {
            for (int32_t i = 1; i < 4 && skipped; i++, absPartIdxSplit += numParts) {
                skipped &= (depth_lookup[m_data[absPartIdxSplit].sbType] == depth+1 &&
                    m_data[absPartIdxSplit].mode >= INTRA_MODES &&
                    m_data[absPartIdxSplit].skip) ? 1 : 0;
                if (!skipped                                        ||
                    refIdx[0] != m_data[absPartIdxSplit].refIdx[0] ||
                    refIdx[1] != m_data[absPartIdxSplit].refIdx[1] ||
                    mv[0]     != m_data[absPartIdxSplit].mv[0]     ||
                    mv[1]     != m_data[absPartIdxSplit].mv[1]) {
                        skipped = 0;
                }
            }
        } else {
            int32_t listIdx = (refIdx[1] >= 0);
            for (int32_t i = 1; i < 4 && skipped; i++, absPartIdxSplit += numParts) {
                skipped &= (depth_lookup[m_data[absPartIdxSplit].sbType] == depth+1 &&
                    m_data[absPartIdxSplit].mode >= INTRA_MODES &&
                    m_data[absPartIdxSplit].skip) ? 1 : 0;
                if (!skipped                                        ||
                    refIdx[0] != m_data[absPartIdxSplit].refIdx[0] ||
                    refIdx[1] != m_data[absPartIdxSplit].refIdx[1] ||
                    mv[listIdx]     != m_data[absPartIdxSplit].mv[listIdx]     ) {
                        skipped = 0;
                }
            }
        }
        if (skipped) {
            int32_t cuWidth = (m_par->MaxCUSize >> depth);
            int32_t cuWidthInMinTU = (cuWidth >> LOG2_MIN_TU_SIZE);
            GetMvRefsOld(mi->sbType, miRow, miCol, &m_mvRefs);

            // TODO(npshosta): re-visit this
            for (PredMode i = NEARESTMV; i <= ZEROMV; i++) {
                if (mv[0] == m_mvRefs.mv[i-NEARESTMV][refIdx[0]] &&
                    mv[1] == m_mvRefs.mv[i-NEARESTMV][refIdx[1]]) {
                        predMode = i;
                        break;
                }
            }
        }
        if (predMode != NEWMV) {
            BlockSize blockSize = mi->sbType;
            memset(m_data + absPartIdx, 0, sizeof(ModeInfo));
            mi->refIdx[0] = refIdx[0];
            mi->refIdx[1] = refIdx[1];
            mi->mv[0]     = mv[0];
            mi->mv[1]     = mv[1];
            // FIXME: re-visit this
            mi->sbType = blockSize + 3;
            mi->mode = predMode;
            mi->modeUV = predMode;
            mi->txSize = 0;
            mi->skip = 1;
            mi->interp = 0;
            PropagateSubPart(mi, m_par->miPitch, num4x4w, num4x4h);
            return true;
        }
        return false;
    }

    void UpdatePartitionCtx(Contexts *ctx, int32_t x4, int32_t y4, int32_t depth, int32_t partitionType) {
        BlockSize bsz = GetSbType(depth, PartitionType(partitionType));
        int32_t size = 64 >> depth;
        memset(ctx->abovePartition + (x4 >> 1), partition_context_lookup[bsz].above, size >> 3);
        memset(ctx->leftPartition  + (y4 >> 1), partition_context_lookup[bsz].left,  size >> 3);
    }

    template <typename PixType>
    void AV1CU<PixType>::ModeDecision(int32_t absPartIdx, int32_t depth)
    {
        int32_t x4 = av1_scan_z2r4[absPartIdx] & 15;
        int32_t y4 = av1_scan_z2r4[absPartIdx] >> 4;
        int32_t left = (int32_t)m_ctbPelX + (x4 << LOG2_MIN_TU_SIZE);
        int32_t top  = (int32_t)m_ctbPelY + (y4 << LOG2_MIN_TU_SIZE);
        ModeInfo *mi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;
        PaletteInfo *pi = &m_currFrame->m_fenc->m_Palette8x8[(left >> 3) + (top >> 3) * m_par->miPitch];

        int32_t size = m_par->MaxCUSize >> depth;
        int32_t halfSize = size >> 1;
        int32_t halfSize8 = halfSize >> 3;
        int32_t hasRows = (top + halfSize < m_par->Height);
        int32_t hasCols = (left + halfSize < m_par->Width);

        const int32_t maxDepth = (m_par->partModes >= PARTS_SQUARE_4x4) ? 4 : 3;
        const int32_t rectAllowed = (m_par->partModes == PARTS_ALL);
        int32_t allowNonSplit = (left + size <= m_par->Width && top + size <= m_par->Height);
        int32_t allowSplit = (depth < maxDepth);
        int32_t allowHorz = rectAllowed && (depth < 4) && (left + size <= m_par->Width) && (top + size <= m_par->Height || top + halfSize == m_par->Height);
        int32_t allowVert = rectAllowed && (depth < 4) && (top + size <= m_par->Height) && (left + size <= m_par->Width || left + halfSize == m_par->Width);
        assert(allowNonSplit || allowSplit || allowHorz || allowVert);

        // and initialize storage
        m_costStored[depth] = COST_MAX;

        int32_t bsl = std::max(0, 3 - depth);
        int32_t abovePartition = (m_contexts.abovePartition[x4 >> 1] >> bsl) & 1;
        int32_t leftPartition = (m_contexts.leftPartition[y4 >> 1] >> bsl) & 1;
        int32_t partitionCtx = 4*bsl + 2*leftPartition + abovePartition;
        const uint16_t *partitionBits = m_currFrame->bitCount->av1Partition[hasRows * 2 + hasCols][partitionCtx];

        // cleanup cached MVs
        for (int32_t i = 0; i < 5; i++) {
            m_nonZeroMvp[i][depth][NEARESTMV_].asInt64 = 0;
            m_nonZeroMvp[i][depth][NEARMV_].asInt64 = 0;
            m_nonZeroMvp[i][depth][NEARMV1_].asInt64 = 0;
            m_nonZeroMvp[i][depth][NEARMV2_].asInt64 = 0;
        }
#if ENABLE_SW_ME
        MemoizeClear(depth);
#endif
        CostType costInitial = m_costCurr;
        Contexts origCtx = m_contexts;

        const int32_t num4x4 = 256 >> (depth << 1);

        if (depth == 2) {
            m_cachedBestSad8x8Raw[0][0] = m_cachedBestSad8x8Raw[0][1] = m_cachedBestSad8x8Raw[1][0] = m_cachedBestSad8x8Raw[1][1] = INT_MAX;
            m_cachedBestSad8x8Rec[0][0] = m_cachedBestSad8x8Rec[0][1] = m_cachedBestSad8x8Rec[1][0] = m_cachedBestSad8x8Rec[1][1] = INT_MAX;
        }
        m_isSCC[depth] = 0;

        if (m_currFrame->m_picCodeType == MFX_FRAMETYPE_I && depth == 1 && allowNonSplit) {
            int32_t log2BlkSize = m_par->Log2MaxCUSize - depth;
            const int32_t pitchRsCs = m_currFrame->m_stats[0]->m_pitchRsCs[log2BlkSize - 2];
            int32_t idx = (top >> log2BlkSize)*pitchRsCs + (left >> log2BlkSize);
            int32_t Rs2 = m_currFrame->m_stats[0]->m_rs[log2BlkSize - 2][idx];
            int32_t Cs2 = m_currFrame->m_stats[0]->m_cs[log2BlkSize - 2][idx];
            if ((m_par->screenMode & 0x2)) {
                const Statistics* stat = m_currFrame->m_stats[0];
                const int32_t pitchColorCount = stat->m_pitchColorCount16x16;
                int i = top >> 4;
                int j = left >> 4;
                int32_t colorCount = stat->m_colorCount16x16[i * pitchColorCount + j];
                colorCount += stat->m_colorCount16x16[i * pitchColorCount + j + 1];
                colorCount += stat->m_colorCount16x16[(i + 1) * pitchColorCount + j];
                colorCount += stat->m_colorCount16x16[(i + 1) * pitchColorCount + j + 1];
                if (Rs2 > 16384 && Cs2 > 16384 && colorCount > 32)
                    allowNonSplit = 0;
            } else {
                if (Rs2 > 16384 && Cs2 > 16384)
                    allowNonSplit = 0;
            }
        }

        if (allowNonSplit) {
#if ENABLE_SW_ME
            if (m_currFrame->m_picCodeType != MFX_FRAMETYPE_I) {
                MeCu(absPartIdx, depth, PARTITION_NONE);
                m_costCurr += m_rdLambda * partitionBits[PARTITION_NONE];
                UpdatePartitionCtx(&m_contexts, x4, y4, depth, PARTITION_NONE);
                SaveFinalCuDecision(absPartIdx, depth, PARTITION_NONE, depth, m_par->chromaRDO);
                m_contexts = origCtx;
                m_costCurr = costInitial;
                CheckIntra(absPartIdx, depth, PARTITION_NONE);
                m_costCurr += m_rdLambda * partitionBits[PARTITION_NONE];
                UpdatePartitionCtx(&m_contexts, x4, y4, depth, PARTITION_NONE);
            } else
#endif
            {
                uint32_t bits;
                int32_t sse = CheckIntra(absPartIdx, depth, PARTITION_NONE, bits);
                m_costCurr += m_rdLambda * partitionBits[PARTITION_NONE];
                bits += partitionBits[PARTITION_NONE];
                UpdatePartitionCtx(&m_contexts, x4, y4, depth, PARTITION_NONE);

                if (depth == 2 && mi->interp0 == BILINEAR && mi->skip)
                    allowSplit = 0;

                if (depth && pi->palette_size_y == 0 && mi->skip && m_rdLambda *bits > sse)
                    allowSplit = 0;

                if (depth && (mi->interp0 == BILINEAR || pi->palette_size_y)) m_isSCC[depth] = 1;
            }
        }

        const int32_t numParts = (256 >> 2) >> (depth << 1);

        if (allowHorz) {
            if (m_costCurr > costInitial) {
                SaveFinalCuDecision(absPartIdx, depth, PARTITION_NONE, depth, m_par->chromaRDO);
                m_contexts = origCtx;
                m_costCurr = costInitial;
            }

            if (depth == 3) {
                //CheckIntra8x4(absPartIdx);
                //m_costCurr += m_rdLambda * partitionBits[PARTITION_HORZ];
                assert(!"CheckIntra8x4");
            } else {
                // in case the second part is out of the picture
                mi[halfSize8 * m_par->miPitch].sbType = GetSbType(depth, PARTITION_HORZ);
                mi[halfSize8 * m_par->miPitch].mode = OUT_OF_PIC;

#if ENABLE_SW_ME
                if (m_currFrame->m_picCodeType != MFX_FRAMETYPE_I) {
                    m_costStored[depth+1] = COST_MAX;
                    m_costCurr += m_rdLambda * partitionBits[PARTITION_HORZ];
                    MeCu(absPartIdx, depth, PARTITION_HORZ);
                    SaveFinalCuDecision(absPartIdx, depth, PARTITION_HORZ, depth+1, m_par->chromaRDO);
                    m_contexts = origCtx;
                    m_costCurr = costInitial;
                    CheckIntra(absPartIdx, depth, PARTITION_HORZ);
                    LoadFinalCuDecision(absPartIdx, depth, PARTITION_HORZ, depth+1, m_par->chromaRDO);
                    if (top + size <= m_par->Height) {
                        CostType costInitial2 = m_costCurr;
                        Contexts origCtx2 = m_contexts;
                        MeCu(absPartIdx + 2*numParts, depth, PARTITION_HORZ);
                        SaveFinalCuDecision(absPartIdx, depth, PARTITION_NONE, depth, m_par->chromaRDO);
                        m_contexts = origCtx2;
                        m_costCurr = costInitial2;
                        CheckIntra(absPartIdx + 2*numParts, depth, PARTITION_HORZ);
                        LoadFinalCuDecision(absPartIdx, depth, PARTITION_NONE, depth, m_par->chromaRDO);
                    }
                } else
#endif
                {
                    uint32_t bits;
                    int32_t sse = CheckIntra(absPartIdx, depth, PARTITION_HORZ, bits);
                    if (top + size <= m_par->Height)
                        sse = CheckIntra(absPartIdx + 2*numParts, depth, PARTITION_HORZ, bits);
                    m_costCurr += m_rdLambda * partitionBits[PARTITION_HORZ];
                }
            }
            UpdatePartitionCtx(&m_contexts, x4, y4, depth, PARTITION_HORZ);
        }

        if (allowVert) {
            if (m_costCurr > costInitial) {
                SaveFinalCuDecision(absPartIdx, depth, PARTITION_NONE, depth, m_par->chromaRDO);
                m_contexts = origCtx;
                m_costCurr = costInitial;
            }

            if (depth == 3) {
                //CheckIntra4x8(absPartIdx);
                //m_costCurr += m_rdLambda * partitionBits[PARTITION_VERT];
                assert(!"CheckIntra4x8");
            } else {
                // in case the second part is out of the picture
                mi[halfSize8].sbType = GetSbType(depth, PARTITION_HORZ);
                mi[halfSize8].mode = OUT_OF_PIC;
#if ENABLE_SW_ME
                if (m_currFrame->m_picCodeType != MFX_FRAMETYPE_I) {
                    m_costStored[depth+1] = COST_MAX;
                    m_costCurr += m_rdLambda * partitionBits[PARTITION_VERT];
                    MeCu(absPartIdx, depth, PARTITION_VERT);
                    SaveFinalCuDecision(absPartIdx, depth, PARTITION_VERT, depth+1, m_par->chromaRDO);
                    m_contexts = origCtx;
                    m_costCurr = costInitial;
                    CheckIntra(absPartIdx, depth, PARTITION_VERT);
                    LoadFinalCuDecision(absPartIdx, depth, PARTITION_VERT, depth+1, m_par->chromaRDO);

                    if (left + size <= m_par->Width) {
                        CostType costInitial2 = m_costCurr;
                        Contexts origCtx2 = m_contexts;
                        MeCu(absPartIdx + numParts, depth, PARTITION_VERT);
                        SaveFinalCuDecision(absPartIdx, depth, PARTITION_NONE, depth, m_par->chromaRDO);
                        m_contexts = origCtx2;
                        m_costCurr = costInitial2;
                        CheckIntra(absPartIdx + numParts, depth, PARTITION_VERT);
                        LoadFinalCuDecision(absPartIdx, depth, PARTITION_NONE, depth, m_par->chromaRDO);
                    }
                } else
#endif
                {
                    uint32_t bits;
                    int32_t sse = CheckIntra(absPartIdx, depth, PARTITION_VERT, bits);
                    if (left + size <= m_par->Width)
                        sse = CheckIntra(absPartIdx + numParts, depth, PARTITION_VERT, bits);
                    m_costCurr += m_rdLambda * partitionBits[PARTITION_VERT];
                }
            }
            UpdatePartitionCtx(&m_contexts, x4, y4, depth, PARTITION_VERT);
        }

        if (allowSplit) {
            if (m_costCurr > costInitial) {
                SaveFinalCuDecision(absPartIdx, depth, PARTITION_NONE, depth, m_par->chromaRDO);
                m_contexts = origCtx;
                m_costCurr = costInitial;
            }

            if (depth == 3) {
                //CheckIntra4x4(absPartIdx);
                assert(!"CheckIntra4x4");
            } else {
                // in case the second, third or fourth parts are out of the picture
                mi[halfSize8].sbType = GetSbType(depth + 1, PARTITION_NONE);
                mi[halfSize8].mode = OUT_OF_PIC;
                mi[halfSize8 * m_par->miPitch] = mi[halfSize8];
                mi[halfSize8 * m_par->miPitch + halfSize8] = mi[halfSize8];

                ModeDecision(absPartIdx + 0*numParts, depth + 1);
                if (left + halfSize < m_par->Width)
                    ModeDecision(absPartIdx + 1*numParts, depth + 1);
                if (top + halfSize < m_par->Height) {
                    ModeDecision(absPartIdx + 2*numParts, depth + 1);
                    if (left + halfSize < m_par->Width)
                        ModeDecision(absPartIdx + 3*numParts, depth + 1);
                }
            }
            m_costCurr += m_rdLambda * partitionBits[PARTITION_SPLIT];
        }

        // keep best decision
        LoadFinalCuDecision(absPartIdx, depth, PARTITION_NONE, depth, m_par->chromaRDO);
    }

#if ENABLE_SW_ME
    template <typename PixType>
    void AV1CU<PixType>::MemoizeInit()
    {
        if (!m_par->enableCmFlag || m_par->numBiRefineIter > 1) {
            memset(&m_memBestMV[1][0], 0, sizeof(m_memBestMV[0][0]) * 3 * 4);
#if 0
            int32_t j, k, l;
            for (j = 0; j < 4; j++) {
                m_memBestMV[3][j].Init();
                m_memBestMV[2][j].Init();
                m_memBestMV[1][j].Init();
#if 0
                for (k = 0; k < 4; k++) {
                    for (l = 0; l < 4; l++) {
                        m_memSubpel[3][j][k][l].Init(&(m_predBufHi3[j][k][l][0]), &(m_predBuf3[j][k][l][0]));
                        m_memSubpel[2][j][k][l].Init(&(m_predBufHi2[j][k][l][0]), &(m_predBuf2[j][k][l][0]));
                        m_memSubpel[1][j][k][l].Init(&(m_predBufHi1[j][k][l][0]), &(m_predBuf1[j][k][l][0]));
                        m_memSubpel[0][j][k][l].Init(&(m_predBufHi0[j][k][l][0]), &(m_predBuf0[j][k][l][0]));
                        if (m_par->hadamardMe >= 2) {
                            m_memSubHad[3][j][k][l].Init(&(m_satd8Buf3[j][k][l][0][0]), &(m_satd8Buf3[j][k][l][1][0]),
                                &(m_satd8Buf3[j][k][l][2][0]), &(m_satd8Buf3[j][k][l][3][0]));
                            m_memSubHad[2][j][k][l].Init(&(m_satd8Buf2[j][k][l][0][0]), &(m_satd8Buf2[j][k][l][1][0]),
                                &(m_satd8Buf2[j][k][l][2][0]), &(m_satd8Buf2[j][k][l][3][0]));
                            m_memSubHad[1][j][k][l].Init(&(m_satd8Buf1[j][k][l][0][0]), &(m_satd8Buf1[j][k][l][1][0]),
                                &(m_satd8Buf1[j][k][l][2][0]), &(m_satd8Buf1[j][k][l][3][0]));
                        }
                    }
                }
#endif
            }
#endif
        }
#ifdef MEMOIZE_NUMCAND
        int32_t k;

        m_memCandSubHad[0].Init();
        m_memCandSubHad[1].Init();
        m_memCandSubHad[2].Init();
        m_memCandSubHad[3].Init();
        for (k = 0; k < MEMOIZE_NUMCAND; k++) {
            m_memCandSubHad[0].Init(k, &(m_satd8CandBuf0[k][0]), &(m_predBufCand0[k][0]));
            m_memCandSubHad[1].Init(k, &(m_satd8CandBuf1[k][0]), &(m_predBufCand1[k][0]));
            m_memCandSubHad[2].Init(k, &(m_satd8CandBuf2[k][0]), &(m_predBufCand2[k][0]));
            m_memCandSubHad[3].Init(k, &(m_satd8CandBuf3[k][0]), &(m_predBufCand3[k][0]));
        }
#endif
    }

    template <typename PixType>
    void AV1CU<PixType>::MemoizeClear(int32_t depth)
    {
        if (!m_par->enableCmFlag) {
            int32_t memSize = (m_par->Log2MaxCUSize - 3 - depth);
            if (m_par->hadamardMe >= 2 && memSize > 0) {
                for (int32_t j = 0; j < 4; j++) {
                    m_memBestMV[memSize][j].Init();
                    for (int32_t k = 0; k < 4; k++) {
                        for (int32_t l = 0; l < 4; l++) {
                            m_memSubpel[memSize][j][k][l].Init();
                            m_memSubHad[memSize][j][k][l].Init();
                        }
                    }
                }
            }
            else {
                for (int32_t j = 0; j < 4; j++) {
                    m_memBestMV[memSize][j].Init();
                    for (int32_t k = 0; k < 4; k++)
                        for (int32_t l = 0; l < 4; l++)
                            m_memSubpel[memSize][j][k][l].Init();
                }
            }
#ifdef MEMOIZE_NUMCAND
            m_memCandSubHad[memSize].Init();
#endif
        }
    }

    template <typename PixType> template <int32_t codecType, int32_t depth>
    void AV1CU<PixType>::ModeDecisionInterTu7(int32_t miRow, int32_t miCol)
    {
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        if (depth == 0)
            CalculateZeroMvPredAndSatd();
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

        const int32_t left = miCol << 3;
        const int32_t top  = miRow << 3;
        const int32_t size = MAX_CU_SIZE >> depth;
        const int32_t halfSize8 = size >> 4;
        const int32_t allowNonSplit = (left + size <= m_par->Width && top + size <= m_par->Height);

        const int32_t bsl = 3 - depth;
        const int32_t abovePartition = (m_contexts.abovePartition[miCol & 7] >> bsl) & 1;
        const int32_t leftPartition = (m_contexts.leftPartition[miRow & 7] >> bsl) & 1;
        const int32_t partitionCtx = 4 * bsl + 2 * leftPartition + abovePartition;
        const int32_t halfSize = size >> 1;
        const int32_t hasRows = (top + halfSize < m_par->Height);
        const int32_t hasCols = (left + halfSize < m_par->Width);
        const uint16_t *partitionBits = m_currFrame->bitCount->av1Partition[hasRows * 2 + hasCols][partitionCtx];
        ModeInfo *mi = m_currFrame->m_modeInfo + miCol + miRow * m_par->miPitch;

        // cleanup cached MVs
        for (int32_t i = 0; i < 5; i++) {
            m_nonZeroMvp[i][depth][NEARESTMV_].asInt64 = 0;
            m_nonZeroMvp[i][depth][NEARMV_].asInt64 = 0;
            m_nonZeroMvp[i][depth][NEARMV1_].asInt64 = 0;
            m_nonZeroMvp[i][depth][NEARMV2_].asInt64 = 0;
        }

        MemoizeClear(depth);
        CostType costInitial = m_costCurr;
        Contexts origCtx = m_contexts;

        if (allowNonSplit) {
            MeCuNonRdAV1<depth>(miRow, miCol);
#if !PROTOTYPE_GPU_MODE_DECISION_SW_PATH
            m_costCurr += m_rdLambda * partitionBits[PARTITION_NONE];
#endif // !PROTOTYPE_GPU_MODE_DECISION_SW_PATH

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
#if !PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        m_costCurr += m_rdLambda * partitionBits[PARTITION_SPLIT];
#endif // !PROTOTYPE_GPU_MODE_DECISION_SW_PATH

        // keep best decision
        if (m_costStored[depth] <= m_costCurr) {
            *mi = StoredCuData(depth)[(miCol & 7) + (miRow & 7) * 8];
            PropagateSubPart(mi, m_par->miPitch, 1 << (3 - depth), 1 << (3 - depth));
            As16B(m_contexts.aboveNonzero[0]) = As16B(m_contextsStored[depth].aboveNonzero[0]);
            As16B(m_contexts.leftNonzero[0]) = As16B(m_contextsStored[depth].leftNonzero[0]);
            As16B(m_contexts.aboveAndLeftPartition) = As16B(m_contextsStored[depth].aboveAndLeftPartition);
            m_costCurr = m_costStored[depth];
        }
    }
    template void AV1CU<uint8_t>::ModeDecisionInterTu7<CODEC_AV1, 0>(int32_t, int32_t);
    template void AV1CU<uint8_t>::ModeDecisionInterTu7<CODEC_AV1, 1>(int32_t, int32_t);
    template void AV1CU<uint8_t>::ModeDecisionInterTu7<CODEC_AV1, 2>(int32_t, int32_t);
#if ENABLE_10BIT
    template void AV1CU<uint16_t>::ModeDecisionInterTu7<CODEC_AV1, 0>(int32_t, int32_t);
    template void AV1CU<uint16_t>::ModeDecisionInterTu7<CODEC_AV1, 1>(int32_t, int32_t);
    template void AV1CU<uint16_t>::ModeDecisionInterTu7<CODEC_AV1, 2>(int32_t, int32_t);
#endif


    template <> template <> void AV1CU<uint8_t>::ModeDecisionInterTu7<CODEC_AV1, 3>(int32_t miRow, int32_t miCol)
    {
        const int32_t abovePartition = m_contexts.abovePartition[miCol & 7] & 1;
        const int32_t leftPartition = m_contexts.leftPartition[miRow & 7] & 1;
        const int32_t partitionCtx =  2 * leftPartition + abovePartition;
        const uint16_t *partitionBits = m_currFrame->bitCount->av1Partition[3][partitionCtx];
        MemoizeClear(3);
        MeCuNonRdAV1<3>(miRow, miCol);
#if !PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        m_costCurr += m_rdLambda * partitionBits[PARTITION_NONE];
#endif // !PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        m_contexts.abovePartition[miCol & 7] = m_contexts.leftPartition[miRow & 7] = 0x0E;
    }


#if ENABLE_10BIT
    template <> template <> void AV1CU<uint16_t>::ModeDecisionInterTu7<CODEC_AV1, 3>(int32_t miRow, int32_t miCol)
    {
        const int32_t abovePartition = m_contexts.abovePartition[miCol & 7] & 1;
        const int32_t leftPartition = m_contexts.leftPartition[miRow & 7] & 1;
        const int32_t partitionCtx =  2 * leftPartition + abovePartition;
        const uint16_t *partitionBits = m_currFrame->bitCount->av1Partition[3][partitionCtx];
        MemoizeClear(3);
        MeCuNonRdAV1<3>(miRow, miCol);
#if !PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        m_costCurr += m_rdLambda * partitionBits[PARTITION_NONE];
#endif // !PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        m_contexts.abovePartition[miCol & 7] = m_contexts.leftPartition[miRow & 7] = 0x0E;
    }
#endif

    template <class PixType>
    void AV1CU<PixType>::ModeDecisionInterTu7_SecondPass(int32_t miRow, int32_t miCol)
    {
        int32_t num4x4 = MAX_NUM_PARTITIONS;
        for (int32_t i = 0; i < MAX_NUM_PARTITIONS; i += num4x4) {
            const int32_t rasterIdx = av1_scan_z2r4[i];
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
            const TxSize maxTxSize = std::min((uint8_t)TX_32X32, max_txsize_lookup[bsz]);
            const int32_t sbw = num4x4w << 2;
            const int32_t sbh = num4x4h << 2;

            TileBorders saveTileBorders = m_tileBorders;
            m_tileBorders.rowStart = 0;
            m_tileBorders.colStart = 0;
            m_tileBorders.rowEnd = m_par->miRows;
            m_tileBorders.colEnd = m_par->miCols;
            //m_tileBorders = GetTileBordersMi(m_currFrame->m_par->tileParam, miRow, miCol);

            const BitCounts &bc = *m_currFrame->bitCount;
            const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
            const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
            const int32_t ctxSkip = GetCtxSkip(above, left);
            const int32_t ctxIsInter = GetCtxIsInter(above, left);
            const uint8_t aboveTxfm = above ? tx_size_wide[above->txSize] : 0;
            const uint8_t leftTxfm =  left  ? tx_size_high[left->txSize] : 0;
            const int32_t ctxTxSize = GetCtxTxSizeAV1(above, left, aboveTxfm, leftTxfm, maxTxSize);
            const uint16_t *skipBits = bc.skip[ctxSkip];
            const uint16_t *txSizeBits = bc.txSize[maxTxSize][ctxTxSize];
            const uint16_t *isInterBits = bc.isInter[ctxIsInter];
            const int32_t ctxInterp = GetCtxInterpBothAV1(above, left, mi->refIdx);
            const uint16_t *interpFilterBits0 = bc.interpFilterAV1[ctxInterp & 15];
            const uint16_t *interpFilterBits1 = bc.interpFilterAV1[ctxInterp >> 8];
            const uint16_t *intraModeBits = bc.intraModeAV1[maxTxSize];
            const Contexts origCtx = m_contexts; // contexts at the start of current block
            Contexts bestCtx;
            uint8_t *anz = m_contexts.aboveNonzero[0]+x4;
            uint8_t *lnz = m_contexts.leftNonzero[0]+y4;
            uint8_t *aboveTxfmCtx = m_contexts.aboveTxfm + x4;
            uint8_t *leftTxfmCtx  = m_contexts.leftTxfm  + y4;
            PixType *recSb = m_yRec + sbx + sby * m_pitchRecLuma;
            PixType *srcSb = m_ySrc + sbx + sby * SRC_PITCH;
            int16_t *diff = vp9scratchpad.diffY;
            int16_t *coef = vp9scratchpad.coefY;
            int16_t *qcoef = vp9scratchpad.qcoefY[0];
            int16_t *bestQcoef = vp9scratchpad.qcoefY[1];
            int16_t *coefWork = (int16_t *)vp9scratchpad.varTxCoefs;
            const int32_t refColocOffset = m_ctbPelX + sbx + (m_ctbPelY + sby) * m_pitchRecLuma;
            const PixType *srcPic = m_ySrc + sbx + sby * SRC_PITCH;

            const PixType * const refColoc[3] = {
                reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_recon->y) + refColocOffset,
                reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_recon->y) + refColocOffset,
                reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_recon->y) + refColocOffset,
            };

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

            alignas(32) PixType pred[64*64], pred2[64*64];

            unsigned int sad, bits, cost, bestCost = UINT_MAX, bestBits = 0, bestDrl = 0;

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
                bitsFirstPass += std::min(maxNumNewMvDrlBits, 1) << 9;
                drlFirstPass = 0;
                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 1) {
                    bits = MvCost(mi->mv[0], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][1].mv[0], 0);
                    if (mi->refIdx[1] != NONE_FRAME)
                        bits += MvCost(mi->mv[1], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][1].mv[1], 0);
                    bits += std::min(maxNumNewMvDrlBits, 2) << 9;
                    if (bitsFirstPass > bits) {
                        bitsFirstPass = bits;
                        drlFirstPass = 1;
                    }
                }
                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 2) {
                    bits = MvCost(mi->mv[0], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv[0], 0);
                    if (mi->refIdx[1] != NONE_FRAME)
                        bits += MvCost(mi->mv[1], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv[1], 0);
                    bits += std::min(maxNumNewMvDrlBits, 2) << 9;
                    if (bitsFirstPass > bits) {
                        bitsFirstPass = bits;
                        drlFirstPass = 2;
                    }
                }
                bitsFirstPass += m_mvRefs.refFrameBitCount[mi->refIdxComb];
                bitsFirstPass += interModeBitsAV1[mi->refIdxComb][NEWMV_];
                modeFirstPass = AV1_NEWMV;
            }

            AV1MVPair clippedMv = mi->mv;
            ClipMV_NR(&clippedMv[0]);
            ClipMV_NR(&clippedMv[1]);
            InterpolateAv1SingleRefLuma_SwPath(refColoc[ mi->refIdx[0] ], m_pitchRecLuma, pred, clippedMv[0], sbh, log2w, DEF_FILTER_DUAL);
            if (mi->refIdxComb == COMP_VAR0) {
                InterpolateAv1SingleRefLuma_SwPath(refColoc[ mi->refIdx[1] ], m_pitchRecLuma, pred2, clippedMv[1], sbh, log2w, DEF_FILTER_DUAL);
                AV1PP::average(pred, pred2, pred, sbh, log2w);
            }
            unsigned sadFirstPass = AV1PP::sad_general(srcPic, 64, pred, 64, log2w, log2h);
            unsigned costFirstPass = RD32(sadFirstPass, bitsFirstPass, m_rdLambdaSqrtInt32);

            const int depth = 4-log2w;
            if (m_currFrame->m_poc==1) debug_printf("[MD2] (%d,%2d,%2d) 1PASS ref=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n",
                depth, miRow, miCol, (mi->refIdxComb+1)>>1, mi->mv[0].mvx, mi->mv[0].mvy, mi->mv[1].mvx, mi->mv[1].mvy,
                costFirstPass, bitsFirstPass);

            AV1MVPair bestMv = {};
            int bestRefIdxComp = LAST_FRAME;
            int bestMode = 0;

            int checkedMv[3][4] = {};

            if (m_currFrame->compoundReferenceAllowed) {
                int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[COMP_VAR0] - 2);
                for (int m = 0; m < 4; m++) {
                    if (m >= 2 && m >= m_mvRefs.mvRefsAV1.refMvCount[COMP_VAR0])
                        continue;
                    AV1MVPair refmv = m < 2 ? m_mvRefs.mvs[COMP_VAR0][m] : m_mvRefs.mvRefsAV1.refMvStack[COMP_VAR0][m].mv;
                    int numNearMvDrlBits = std::min(maxNumNearMvDrlBits, m);

                    InterpolateAv1SingleRefLuma_SwPath(refColoc[LAST_FRAME], m_pitchRecLuma, pred, refmv[0], sbh, log2w, DEF_FILTER_DUAL);
                    InterpolateAv1SingleRefLuma_SwPath(refColoc[ALTREF_FRAME], m_pitchRecLuma, pred2, refmv[1], sbh, log2w, DEF_FILTER_DUAL);

                    uint32_t costRef0 = MAX_UINT;
                    uint32_t costRef1 = MAX_UINT;
                    uint32_t bitsRef0 = 0;
                    uint32_t bitsRef1 = 0;
                    uint32_t sadRef0 = 0;
                    uint32_t sadRef1 = 0;
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
                        const int numNearMvDrlBits = std::min(maxNumNearMvDrlBits, modeRef0);
                        sadRef0 = AV1PP::sad_general(srcPic, 64, pred, 64, log2w, log2h);
                        bitsRef0  = m_mvRefs.refFrameBitCount[LAST_FRAME];
                        bitsRef0 += interModeBitsAV1[LAST_FRAME][modeRef0 == 0 ? NEARESTMV_ : NEARMV_];
                        bitsRef0 += numNearMvDrlBits << 9;
                        costRef0 = RD32(sadRef0, bitsRef0, m_rdLambdaSqrtInt32);
                        checkedMv[LAST_FRAME][modeRef0] = 1;
                    }

                    if (modeRef1 >= 0) {
                        const int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[ALTREF_FRAME] - 2);
                        const int numNearMvDrlBits = std::min(maxNumNearMvDrlBits, modeRef1);
                        sadRef1 = AV1PP::sad_general(srcPic, 64, pred2, 64, log2w, log2h);
                        bitsRef1  = m_mvRefs.refFrameBitCount[ALTREF_FRAME];
                        bitsRef1 += interModeBitsAV1[ALTREF_FRAME][modeRef1 == 0 ? NEARESTMV_ : NEARMV_];
                        bitsRef1 += numNearMvDrlBits << 9;
                        costRef1 = RD32(sadRef1, bitsRef1, m_rdLambdaSqrtInt32);
                        checkedMv[ALTREF_FRAME][modeRef1] = 1;
                    }

                    AV1PP::average(pred, pred2, pred, sbh, log2w);
                    sad = AV1PP::sad_general(srcPic, 64, pred, 64, log2w, log2h);
                    bits  = m_mvRefs.refFrameBitCount[COMP_VAR0];
                    bits += interModeBitsAV1[COMP_VAR0][m == 0 ? NEARESTMV_ : NEARMV_];
                    bits += numNearMvDrlBits << 9;
                    cost = RD32(sad, bits, m_rdLambdaSqrtInt32);

                    if (m_currFrame->m_poc==1) debug_printf("[MD2] (%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d),(%d,%d) cost=%u sad=%u bits=%u\n",
                        depth, miRow, miCol, 2, m, refmv[0].mvx, refmv[0].mvy, refmv[1].mvx, refmv[1].mvy, cost, sad, bits);

                    if (modeRef0 >= 0) {
                        if (m_currFrame->m_poc==1) debug_printf("[MD2] (%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u sad=%u bits=%u\n",
                            depth, miRow, miCol, 0, modeRef0, refmv[0].mvx, refmv[0].mvy, costRef0, sadRef0, bitsRef0);
                    }

                    if (modeRef1 >= 0) {
                        if (m_currFrame->m_poc==1) debug_printf("[MD2] (%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u sad=%u bits=%u\n",
                            depth, miRow, miCol, 1, modeRef1, refmv[1].mvx, refmv[1].mvy, costRef1, sadRef1, bitsRef1);
                    }

                    if (bestCost > cost) {
                        bestCost = cost;
                        bestRefIdxComp = COMP_VAR0;
                        bestMv = refmv;
                        bestMode = AV1_NEARESTMV;
                        bestDrl = (m == 0) ? 0 : m - 1;
                    }

                    if (bestCost > costRef0) {
                        bestCost = costRef0;
                        bestRefIdxComp = LAST_FRAME;
                        bestMv[0] = refmv[0];
                        bestMv[1].asInt = 0;
                        bestMode = AV1_NEARESTMV;
                        bestDrl = (modeRef0 == 0) ? 0 : m - 1;
                    }

                    if (bestCost > costRef1) {
                        bestCost = costRef1;
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
                    AV1MV refmv = m < 2 ? m_mvRefs.mvs[ref][m][0] : m_mvRefs.mvRefsAV1.refMvStack[ref][m].mv[0];
                    int numNearMvDrlBits = std::min(maxNumNearMvDrlBits, m);

                    InterpolateAv1SingleRefLuma_SwPath(refColoc[ref], m_pitchRecLuma, pred, refmv, sbh, log2w, DEF_FILTER_DUAL);
                    sad = AV1PP::sad_general(srcPic, 64, pred, 64, log2w, log2h);
                    bits  = m_mvRefs.refFrameBitCount[ref];
                    bits += interModeBitsAV1[ref][m == 0 ? NEARESTMV_ : NEARMV_];
                    bits += numNearMvDrlBits << 9;
                    cost = RD32(sad, bits, m_rdLambdaSqrtInt32);

                    if (m_currFrame->m_poc==1) debug_printf("[MD2] (%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u sad=%u bits=%u\n",
                        depth, miRow, miCol, (ref+1)>>1, m, refmv.mvx, refmv.mvy, cost, sad, bits);

                    if (bestCost > cost) {
                        bestCost = cost;
                        bestRefIdxComp = ref;
                        bestMv[0] = refmv;
                        bestMv[1].asInt = 0;
                        bestMode = AV1_NEARESTMV;
                        bestDrl = (m == 0) ? 0 : m - 1;
                    }
                }
            }

            if (bestCost >= costFirstPass) {
                bestCost = costFirstPass;
                bestBits = bitsFirstPass;
                bestRefIdxComp = mi->refIdxComb;
                bestMv = mi->mv;
                bestMode = modeFirstPass;
                bestDrl = drlFirstPass;
            }

            mi->mv = bestMv;
            mi->refIdx[0] = bestRefIdxComp == COMP_VAR0 ? LAST_FRAME : bestRefIdxComp;
            mi->refIdx[1] = bestRefIdxComp == COMP_VAR0 ? ALTREF_FRAME : NONE_FRAME;
            mi->refIdxComb = bestRefIdxComp;
            mi->mode = bestMode;
            mi->refMvIdx = bestDrl;
            PropagateSubPart(mi, m_par->miPitch, sbw >> 3, sbh >> 3);
        }
    }
#endif
    void CopyVarTxInfo(const uint16_t *src, uint16_t *dst, int32_t w) {
        assert(w == 1 || w == 2 || w == 4 || w == 8 || w == 16);
        switch (w) {
        case 1: *dst = *src; return;
        case 2:
            storel_si32(dst + 0 * 16, loadu_si32(src + 0 * 16));
            storel_si32(dst + 1 * 16, loadu_si32(src + 1 * 16));
            return;
        case 4:
            storel_epi64(dst + 0 * 16, loadl_epi64(src + 0 * 16));
            storel_epi64(dst + 1 * 16, loadl_epi64(src + 1 * 16));
            storel_epi64(dst + 2 * 16, loadl_epi64(src + 2 * 16));
            storel_epi64(dst + 3 * 16, loadl_epi64(src + 3 * 16));
            return;
        case 8:
            for (int32_t i = 0; i < 8; i++)
                storea_si128(dst + i * 16, loada_si128(src + i * 16));
            return;
        case 16:
            for (int32_t i = 0; i < 16; i++)
                storea_si256(dst + i * 16, loada_si256(src + i * 16));
            return;
        }
    }

    template <typename PixType> void AV1CU<PixType>::RefineDecisionIAV1()
    {
        alignas(64) PixType bestRec[64*64];
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
            const int32_t haveLeft  = (miCol > m_tileBorders.colStart);
            const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
            const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
            const int32_t ctxSkip = GetCtxSkip(above, left);
            const uint8_t aboveTxfm = above ? tx_size_wide[above->txSize] : 0;
            const uint8_t leftTxfm =  left  ? tx_size_high[left->txSize] : 0;
            const int32_t ctxTxSize = GetCtxTxSizeAV1(above, left, aboveTxfm, leftTxfm, maxTxSize);
            const uint16_t *skipBits = bc.skip[ctxSkip];
            const uint16_t *txSizeBits = bc.txSize[maxTxSize][ctxTxSize];
            const Contexts origCtx = m_contexts; // contexts at the start of current block
            Contexts bestCtx;
            uint8_t *anz = m_contexts.aboveNonzero[0]+x4;
            uint8_t *lnz = m_contexts.leftNonzero[0]+y4;
            PixType *recSb = m_yRec + sbx + sby * m_pitchRecLuma;
            PixType *srcSb = m_ySrc + sbx + sby * SRC_PITCH;
            int16_t *diff = vp9scratchpad.diffY;
            int16_t *coef = vp9scratchpad.coefY;
            int16_t *coefBest = vp9scratchpad.dqcoefY;
            int16_t *qcoef = vp9scratchpad.qcoefY[0];
            int16_t *bestQcoef = vp9scratchpad.qcoefY[1];
            int16_t *coefWork  = (int16_t *)vp9scratchpad.varTxCoefs[0][0].coef;
            int16_t *coefWorkBest = (int16_t *)vp9scratchpad.varTxCoefs[1][0].coef;
            int16_t *eob4x4 = (int16_t *)vp9scratchpad.varTxCoefs[2][0].coef;
            int16_t *eob4x4Best = (int16_t *)vp9scratchpad.varTxCoefs[2][1].coef;
            float scpp = 0;
            int32_t scid = GetSpatialComplexity(sbx, sby, log2w, &scpp);
            int32_t minTxSize = std::max((int32_t)(scid?TX_4X4:TX_8X8), mi->txSize - MAX_TX_DEPTH);
            int32_t miColEnd = m_tileBorders.colEnd;

            const int32_t filtType = get_filt_type(above, left, PLANE_TYPE_Y);
            const int32_t max_angle_delta = av1_is_directional_mode(mi->mode) ? MAX_ANGLE_DELTA : 0;
            PaletteInfo *pi = &m_currFrame->m_fenc->m_Palette8x8[miCol + miRow * m_par->miPitch]; // Y Decided in Mode Decision
            pi->palette_size_uv = 0;

            CostType bestCost = COST_MAX;
            RdCost bestRdCost = {};
            TxSize bestTxSize = mi->txSize;
            TxType bestTxType = DCT_DCT;
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
                        CopyNxM(recSb, m_pitchRecLuma, bestRec, num4x4w<<2, num4x4h<<2);

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

                    TxType txTypeStart = DCT_DCT;
                    //TxType txTypeEnd = ADST_ADST;
                    TxType txTypeList[] = { DCT_DCT,  ADST_DCT, DCT_ADST, ADST_ADST, IDTX };

                    for (int32_t txSize = maxTxSize; txSize >= minTxSize; txSize--) {
                        for (auto txType : txTypeList) {
                            if (txSize == TX_32X32 && txType != DCT_DCT)
                                continue;
                            if (pi->palette_size_y && txType != DCT_DCT && txType != IDTX)
                                continue;
                            if (!pi->palette_size_y && txType == IDTX)
                                continue;
                            if (txType == ADST_ADST && bestTxType == DCT_DCT)
                                continue;

                            int deltaStart = bestAngleDelta;
                            int deltaEnd = bestAngleDelta;
                            int deltaStep = 1;
                            int numPasses = 1;
                            if (av1_is_directional_mode(mi->mode)) {
                                /*if (txSize == maxTxSize && txType == txTypeStart) {
                                    // Search again
                                    deltaStart = -2;
                                    deltaEnd = 2;
                                    deltaStep = 2;
                                    numPasses = 2;
                                }
                                else*/
                                if (txSize == maxTxSize || (maxTxSize == TX_32X32 && txSize == maxTxSize -1)) {
                                    deltaStart = std::max(-3, bestAngleDelta - 1);
                                    deltaEnd = std::min(3, bestAngleDelta + 1);
                                    deltaStep = 1;
                                    numPasses = 1;
                                }
                            }

                            int localBestAngle = 0;
                            CostType localBestCost = COST_MAX;
                            for (int pass = 0; pass < numPasses; pass++) {
                                for (int delta = deltaStart; delta <= deltaEnd; delta += deltaStep) {

                                    float roundFAdjT[2] = { roundFAdjYInit[0], roundFAdjYInit[1] };

                                    m_contexts = origCtx;
                                    RdCost rd = TransformIntraYSbAv1(bsz, mi->mode, haveAbove, haveLeft, (TxSize)txSize, txType,
                                        anz, lnz, srcSb, recSb, m_pitchRecLuma, diff, coef, qcoef, coefWork,
                                        m_lumaQp, bc, m_rdLambda, miRow, miCol, miColEnd,
                                        delta, filtType, *m_par, m_aqparamY, NULL, //&roundFAdjT[0],
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

                                    if (localBestCost > cost) {
                                        localBestCost = cost;
                                        localBestAngle = delta;
                                    }

                                    if (bestCost > cost) {
                                        bestRdCost = rd;
                                        bestCost = cost;
                                        //bestEob = rd.eob;
                                        bestTxSize = (TxSize)txSize;
                                        bestTxType = txType;
                                        bestAngleDelta = delta;
                                        std::swap(qcoef, bestQcoef);
                                        bestCtx = m_contexts;
                                        CopyNxN(recSb, m_pitchRecLuma, bestRec, sbw);
                                        std::swap(coefWork, coefWorkBest);
                                        std::swap(coef, coefBest);
                                        std::swap(eob4x4, eob4x4Best);
                                        //m_roundFAdjY[0] = roundFAdjT[0];
                                        //m_roundFAdjY[1] = roundFAdjT[1];
                                    }
                                }
                                //delta_start = delta_stop = bestAngleDelta;
                                deltaStart = localBestAngle - 1;
                                deltaEnd = localBestAngle + 1;
                            }
                            //if (pi->palette_size_y && pi->palette_size_y == pi->true_colors_y) break;
                            if (bestRdCost.eob == 0 && bestRdCost.sse == 0) break;
                        }
                        //if (pi->palette_size_y && pi->palette_size_y == pi->true_colors_y) break;
                        if (bestRdCost.eob == 0 && m_rdLambda * (bestRdCost.modeBits + skipBits[1]) > bestRdCost.sse) break;
                        if (bestRdCost.eob != 0 && txSize < bestTxSize) break;
                    }
#ifdef ADAPTIVE_DEADZONE
                    if (bestRdCost.eob) {
                        int32_t step = 1 << bestTxSize;
                        for (int32_t y = 0; y < num4x4h; y += step) {
                            for (int32_t x = 0; x < num4x4w; x += step) {
                                int32_t blockIdx = h265_scan_r2z4[y * 16 + x];
                                int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
                                CoeffsType *coeff = coefBest + offset;
                                int16_t *coefOrigin = coefWorkBest + offset;
                                if(eob4x4Best[y * 16 + x])
                                    adaptDz(coefOrigin, coeff, m_aqparamY, bestTxSize, m_roundFAdjY, eob4x4Best[y*16+x]);
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

                RdCost rd;
                int32_t test = 0;
                if (!m_par->ibcModeDecision)
                    test = CheckIntraBlockCopy(i, qcoef, rd, varTxInfo, true, COST_MAX);
                else if (is_intrabc_block(mi)) {
                    const int32_t intx = mi->mv[0].mvx >> 3;
                    const int32_t inty = mi->mv[0].mvy >> 3;
                    const int32_t sbw = num4x4w << 2;
                    const PixType *bestPredY = recSb + intx + inty * m_pitchRecLuma;

                    PixType *rec = vp9scratchpad.recY[0];
                    diff = vp9scratchpad.diffY;
                    CopyFromUnalignedNxN(bestPredY, m_pitchRecLuma, rec, 64, sbw);
                    AV1PP::diff_nxn(srcSb, rec, diff, log2w);

                    uint8_t *aboveTxfmCtx = m_contexts.aboveTxfm + x4;
                    uint8_t *leftTxfmCtx = m_contexts.leftTxfm + y4;

                    rd = TransformInterYSbVarTxByRdo(
                        bsz, anz, lnz,
                        srcSb, rec, diff, qcoef,
                        m_lumaQp, bc, m_rdLambda,
                        aboveTxfmCtx, leftTxfmCtx,
                        varTxInfo,
                        m_aqparamY, m_roundFAdjY,
                        vp9scratchpad.varTxCoefs, MAX_VARTX_DEPTH, TX_TYPES-1);

                    CopyNxN(rec, 64, recSb, m_pitchRecLuma, sbw);

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

                    const int txSize = max_txsize_lookup[bsz];
                    rd.sse = AV1PP::sse(srcSb, SRC_PITCH, recSb, m_pitchRecLuma, txSize, txSize);
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
                        CopyNxN(recSb, m_pitchRecLuma, bestRec, sbw);
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
            CopyNxM(bestRec, num4x4w<<2, recSb, m_pitchRecLuma, num4x4w<<2, num4x4h<<2);
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
            } else {
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
                (void)CheckIntraChromaNonRdAv1(i, 4 - log2w, PARTITION_NONE, bestRec, num4x4w << 2);
                PropagatePaletteInfo(m_currFrame->m_fenc->m_Palette8x8, miRow, miCol, m_par->miPitch, num4x4w >> 1, num4x4h >> 1);
            }

            i += num4x4;
        }

        storea_si128(m_contexts.aboveAndLeftPartition, loada_si128(savedPartitionCtx));
    }

    static inline int32_t HasSubpel(const RefIdx refIdx[2], const AV1MV mv[2])
    {
        int32_t hasSubpel0 = (mv[0].mvx | mv[0].mvy) & 7;
        int32_t hasSubpel1 = (mv[1].mvx | mv[1].mvy) & 7;
        if (refIdx[1] != NONE_FRAME)
            hasSubpel0 |= hasSubpel1;
        return hasSubpel0;
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

    static inline void ClampMvRefAV1(AV1MV *mv, BlockSize sbType, int32_t miRow, int32_t miCol, const AV1VideoParam *m_par) {
        mv->mvy = clampMvRowAV1(mv->mvy, MV_BORDER_AV1, sbType, miRow, m_par->miRows);
        mv->mvx = clampMvColAV1(mv->mvx, MV_BORDER_AV1, sbType, miCol, m_par->miCols);
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

    void CheckNewCompoundModes(ModeInfo *mi, MvRefs &m_mvRefs, const uint16_t *interModeBits, const uint16_t *nearMvDrlIdxBits, uint32_t &bestBitCost, int32_t miRow, int32_t miCol, const AV1VideoParam *m_par)
    {
        AV1MVPair bestRefMv;
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
                    AV1MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][1].mv;
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
                    AV1MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][1].mv;
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
                    AV1MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv;
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
                    AV1MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv;
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
                    AV1MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][3].mv;
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
                    AV1MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][3].mv;
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
    int32_t  AV1CU<PixType>::GetCmDistMv(int32_t sbx, int32_t sby, int32_t log2w, int32_t& mvd) {
        const int32_t w = 1<< (log2w+2);
        const int32_t h = w;

        int32_t satd = INT_MAX;
        RefIdx *bestRefIdx;
        AV1MVPair bestMv;

        int8_t refIdxComb;
        if (log2w == 1) {
            const int32_t numBlk = sby + (sbx >> 3);
            bestRefIdx = m_newMvRefIdx8[numBlk];
            bestMv = m_newMvFromGpu8[numBlk];
            refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            satd = m_newMvSatd8[numBlk];
        }
        else if (log2w == 2) {
            const int32_t numBlk = (sby >> 2) + (sbx >> 4);
            bestRefIdx = m_newMvRefIdx16[numBlk];
            bestMv = m_newMvFromGpu16[numBlk];
            refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            satd = m_newMvSatd16[numBlk];
        }
        else if (log2w == 3) {
            const int32_t numBlk = (sby >> 5) * 2 + (sbx >> 5);
            bestRefIdx = m_newMvRefIdx32[numBlk];
            bestMv = m_newMvFromGpu32[numBlk];
            refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            satd = m_newMvSatd32[numBlk];
        }
        else if (log2w == 4) {
            const int32_t numBlk = (sby >> 6) + (sbx >> 6);
            bestRefIdx = m_newMvRefIdx64[numBlk];
            bestMv = m_newMvFromGpu64[numBlk];
            refIdxComb = (bestRefIdx[1] == NONE_FRAME) ? bestRefIdx[0] : COMP_VAR0;
            satd = m_newMvSatd64[numBlk];

        } else {
            assert(0);
        }

        mvd = std::abs(m_mvRefs.bestMv[refIdxComb][0].mvx - bestMv[0].mvx);
        mvd += std::abs(m_mvRefs.bestMv[refIdxComb][0].mvy - bestMv[0].mvy);

        if (bestRefIdx[1] != NONE_FRAME) {
            int32_t mvd1 = std::abs(m_mvRefs.bestMv[refIdxComb][1].mvx - bestMv[1].mvx);
            mvd1 += std::abs(m_mvRefs.bestMv[refIdxComb][1].mvy - bestMv[1].mvy);
            if (mvd1 < mvd) mvd = mvd1;
        }
        return satd;
    }

    static inline int32_t IsZeroResd(float SCpp, float SADpp, int8_t q, int32_t ybd)
    {
        int32_t sgoodPred = false;
        float Q = (float) vp9_dc_quant(q, 0, ybd) / 8.0f;
        float SADT = Q*0.186278f;
        if (SADpp < SADT) sgoodPred = true;
        return sgoodPred;
    }

    template <typename PixType>
    inline bool AV1CU<PixType>::IsGoodPred(float SCpp, float SADpp, float mvdAvg) const
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

    template <typename PixType>
    inline bool AV1CU<PixType>::IsBadPred(float SCpp, float SADpp_best, float mvdAvg) const
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
            float msad = logf(SADpp_best + mfi[l][qidx] * std::min(64.0f, mvdAvg));

            if (msad > SADT)       badPred = true;
        }
        return badPred;
    }

    bool IsJoinable(const ModeInfo &curr, const ModeInfo &neighbour) {
        return curr.mv == neighbour.mv && curr.refIdxComb == neighbour.refIdxComb && curr.refIdxComb!=INTRA_FRAME && curr.sbType == neighbour.sbType
            && neighbour.mode != OUT_OF_PIC && curr.interp == neighbour.interp;
    }

    int GetMinTxSizeInLCU(ModeInfo *mi, int32_t miPitch) {
        int minTxSize = TX_32X32;
        for (int i=0; i < 8; i++) {
            for (int j=0; j < 8; j++) {
                minTxSize = std::min(minTxSize, (int)mi[i*miPitch+j].txSize);
            }
        }
        return minTxSize;
    }

    template <typename PixType> bool AV1CU<PixType>::JoinRefined32x32MI(ModeInfo mi64, bool check)
    {
#if !GPU_VARTX
        ModeInfo *mi = m_data;
        if (mi->sbType == BLOCK_32X32) {
            ModeInfo *mi2 = m_data + 4 + 0 * m_par->miPitch;
            ModeInfo *mi3 = m_data + 0 + 4 * m_par->miPitch;
            ModeInfo *mi4 = m_data + 4 + 4 * m_par->miPitch;

            if (IsJoinable(*mi, *mi2) && IsJoinable(*mi, *mi3) && IsJoinable(*mi, *mi4) && (check || GetMinTxSizeInLCU(mi, m_par->miPitch)> TX_8X8)) {
                if (!check) {
                    mi->sbType = BLOCK_64X64;
                    mi->skip &= (mi2->skip & mi3->skip & mi4->skip);
                    mi64.sbType = mi->sbType;
                    mi64.skip = mi->skip;
                    *mi = mi64;
                    *mi2 = *mi3 = *mi4 = *mi;
                }
                return true;
            }
        }
#endif
        return false;
    }

    template <typename PixType> void AV1CU<PixType>::JoinMI()
    {
        for (int i = 0; i < 4; i++) {
            const int32_t x8 = (i & 1) << 2;
            const int32_t y8 = (i >> 1) << 2;
            ModeInfo *mi = m_data + x8 + y8 * m_par->miPitch;
            if (mi->sbType != BLOCK_16X16)
                continue;

            const ModeInfo *mi2 = m_data + (x8 + 2) + (y8 + 0) * m_par->miPitch;
            const ModeInfo *mi3 = m_data + (x8 + 0) + (y8 + 2) * m_par->miPitch;
            const ModeInfo *mi4 = m_data + (x8 + 2) + (y8 + 2) * m_par->miPitch;
            if (IsJoinable(*mi, *mi2) && IsJoinable(*mi, *mi3) && IsJoinable(*mi, *mi4)) {
                mi->sbType = BLOCK_32X32;
                mi->skip &= (mi2->skip & mi3->skip & mi4->skip);
            }
        }
        ModeInfo *mi = m_data;
        if (mi->sbType == BLOCK_32X32) {
            const ModeInfo *mi2 = m_data + 4 + 0 * m_par->miPitch;
            const ModeInfo *mi3 = m_data + 0 + 4 * m_par->miPitch;
            const ModeInfo *mi4 = m_data + 4 + 4 * m_par->miPitch;

            if (IsJoinable(*mi, *mi2) && IsJoinable(*mi, *mi3) && IsJoinable(*mi, *mi4)) {
                mi->sbType = BLOCK_64X64;
                mi->skip &= (mi2->skip & mi3->skip & mi4->skip);
            }
        }
    }

    template <typename PixType> bool AV1CU<PixType>::Split64()
    {
        // Split MI
        bool split64 = (m_data->sbType == BLOCK_64X64) ? true : false;
        if (split64 && m_currFrame->m_temporalSync) {
            if (m_par->enableCmFlag) {
                (m_currFrame->compoundReferenceAllowed)
                    ? GetMvRefsAV1TU7B(m_data->sbType, (m_ctbPelY >> 3), (m_ctbPelX >> 3), &m_mvRefs, (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B))
                    : GetMvRefsAV1TU7P(m_data->sbType, (m_ctbPelY >> 3), (m_ctbPelX >> 3), &m_mvRefs);
                float scpp = 0;
                int32_t scid = GetSpatialComplexity(0, 0, 4, &scpp);
                int32_t mvd;
                float sadpp = GetCmDistMv(0, 0, 4, mvd);
                sadpp = std::max(0.1f, sadpp / (64 * 64));
                split64 = (IsBadPred(scpp, sadpp, mvd)) ? true : false;
            } else {
                (m_currFrame->compoundReferenceAllowed)
                    ? GetMvRefsAV1TU7B(m_data->sbType, (m_ctbPelY >> 3), (m_ctbPelX >> 3), &m_mvRefs, (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B))
                    : GetMvRefsAV1TU7P(m_data->sbType, (m_ctbPelY >> 3), (m_ctbPelX >> 3), &m_mvRefs);
                float scpp = 0;
                int32_t scid = GetSpatialComplexity(0, 0, 4, &scpp);
                const PixType *bestInterPred = m_bestInterp[0][0][0];
                float sadpp = AV1PP::sad_special(m_ySrc, SRC_PITCH, bestInterPred, 4, 4);
                sadpp = std::max(0.1f, sadpp / (64 * 64));
                int32_t mvd = std::abs(m_mvRefs.bestMv[m_data->refIdxComb][0].mvx - m_data->mv[0].mvx);
                mvd += std::abs(m_mvRefs.bestMv[m_data->refIdxComb][0].mvy - m_data->mv[0].mvy);
                if (m_data->refIdx[1] != NONE_FRAME) {
                    int32_t mvd1 = std::abs(m_mvRefs.bestMv[m_data->refIdxComb][1].mvx - m_data->mv[1].mvx);
                    mvd1 += std::abs(m_mvRefs.bestMv[m_data->refIdxComb][1].mvy - m_data->mv[1].mvy);
                    if (mvd1 < mvd) mvd = mvd1;
                }
                split64 = (IsBadPred(scpp, sadpp, mvd)) ? true : false;
            }
        }
        return split64;
    }


    void CopyTxSizeInfo(const uint16_t *varTxInfo, ModeInfo *mi, int32_t miPitch, int32_t w)
    {
        assert(w == 1 || w == 2 || w == 4 || w == 8 || w == 16);
        //__m128i v;
        switch (w) {
        case 1: case 2: mi->txSize = (*varTxInfo >> 4) & 3; return;
        case 4:
            for (int32_t i = 0; i < 2; i++) {
                mi[i * miPitch + 0].txSize = (varTxInfo[i * 32 + 0] >> 4) & 3;
                mi[i * miPitch + 1].txSize = (varTxInfo[i * 32 + 2] >> 4) & 3;
            }
            return;
        case 8:
            for (int32_t i = 0; i < 4; i++) {
                mi[i * miPitch + 0].txSize = (varTxInfo[i * 32 + 0] >> 4) & 3;
                mi[i * miPitch + 1].txSize = (varTxInfo[i * 32 + 2] >> 4) & 3;
                mi[i * miPitch + 2].txSize = (varTxInfo[i * 32 + 4] >> 4) & 3;
                mi[i * miPitch + 3].txSize = (varTxInfo[i * 32 + 6] >> 4) & 3;
            }
            return;
        case 16:
            for (int32_t i = 0; i < 8; i++) {
                mi[i * miPitch + 0].txSize = (varTxInfo[i * 32 +  0] >> 4) & 3;
                mi[i * miPitch + 1].txSize = (varTxInfo[i * 32 +  2] >> 4) & 3;
                mi[i * miPitch + 2].txSize = (varTxInfo[i * 32 +  4] >> 4) & 3;
                mi[i * miPitch + 3].txSize = (varTxInfo[i * 32 +  6] >> 4) & 3;
                mi[i * miPitch + 4].txSize = (varTxInfo[i * 32 +  8] >> 4) & 3;
                mi[i * miPitch + 5].txSize = (varTxInfo[i * 32 + 10] >> 4) & 3;
                mi[i * miPitch + 6].txSize = (varTxInfo[i * 32 + 12] >> 4) & 3;
                mi[i * miPitch + 7].txSize = (varTxInfo[i * 32 + 14] >> 4) & 3;
            }
            return;
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


    #define ALIGN_DECL(X) __declspec(align(X))
    template <typename PixType> void AV1CU<PixType>::RefineDecisionAV1()
    {
        assert((m_par->maxTxDepthIntraRefine > 1) == (m_par->maxTxDepthInterRefine > 1)); // both should be refined or not refined
        ModeInfo mi64 = *m_data;

        alignas(16) uint8_t savedPartitionCtx[16];
        storea_si128(savedPartitionCtx, loada_si128(m_contexts.aboveAndLeftPartition));


        PixType *predSaved = vp9scratchpad.predY[1];
        PixType *predUvSaved = vp9scratchpad.predUv[1];
        if(m_par->CmInterpFlag) {
            if (JoinRefined32x32MI(mi64, true)) {
                CopyNxN(m_yRec, m_pitchRecLuma, predSaved, 64, 64);
                CopyNxM(m_uvRec, m_pitchRecChroma, predUvSaved, PRED_PITCH, 64, 32);
            }
        }

        uint32_t joined = 0;
        do {
        m_contexts = m_contextsInitSb;


        const PixType *refFramesY[3] = {};
        const PixType *refFramesUv[3] = {};
        if (!m_par->CmInterpFlag) {
            refFramesY[LAST_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_recon->y);
            refFramesY[GOLDEN_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_recon->y);
            refFramesY[ALTREF_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_recon->y);
            refFramesUv[LAST_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_recon->uv);
            refFramesUv[GOLDEN_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_recon->uv);
            refFramesUv[ALTREF_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_recon->uv);
        }


        const PixType *refFramesY_Upscale[3] = {};
        const PixType *refFramesUv_Upscale[3] = {};
        if (m_par->superResFlag) {
            refFramesY_Upscale[LAST_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_reconUpscale->y);
            refFramesY_Upscale[GOLDEN_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_reconUpscale->y);
            refFramesY_Upscale[ALTREF_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_reconUpscale->y);
            refFramesUv_Upscale[LAST_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_reconUpscale->uv);
            refFramesUv_Upscale[GOLDEN_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_reconUpscale->uv);
            refFramesUv_Upscale[ALTREF_FRAME] = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_reconUpscale->uv);
        }
        // Merge MI with same MV.
#if !PROTOTYPE_GPU_MODE_DECISION
        JoinMI();
#endif

        // Since there is no 64x64 intra check, prevent bad 64x64 inter blocks by splitting.
#if !PROTOTYPE_GPU_MODE_DECISION
        bool split64 = Split64();
        if (split64) {
            m_data->sbType -= 3;
            ModeInfo *mi2 = m_data + 4;
            ModeInfo *mi3 = m_data + 4 * m_par->miPitch;
            ModeInfo *mi4 = m_data + 4 * m_par->miPitch + 4;
            *mi2 = *mi3 = *mi4 = *m_data;
#if !PROTOTYPE_GPU_MODE_DECISION
            const PixType *bestInterPred = m_bestInterp[0][0][0];
            CopyNxN(bestInterPred, 64, m_bestInterp[1][0][0], 64, 32);
            CopyNxN(bestInterPred + 32, 64, m_bestInterp[1][0][4], 64, 32);
            CopyNxN(bestInterPred + 32 * 64, 64, m_bestInterp[1][4][0], 64, 32);
            CopyNxN(bestInterPred + 32 * 64 + 32, 64, m_bestInterp[1][4][4], 64, 32);
#endif
        }
#endif

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
            const int32_t haveLeft  = (miCol > m_tileBorders.colStart);
            const int32_t miColEnd = m_tileBorders.colEnd;
            const QuantParam &qparamY = m_aqparamY;
            float roundFAdjYInit[2] = { m_roundFAdjY[0], m_roundFAdjY[1] };

            const BitCounts &bc = *m_currFrame->bitCount;
            const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
            const ModeInfo *left  = GetLeft(mi, miCol, m_tileBorders.colStart);
            const int32_t ctxSkip = GetCtxSkip(above, left);
            const int32_t ctxIsInter = GetCtxIsInter(above, left);
            const uint8_t aboveTxfm = above ? tx_size_wide[above->txSize] : 0;
            const uint8_t leftTxfm =  left  ? tx_size_high[left->txSize] : 0;
            const int32_t ctxTxSize = GetCtxTxSizeAV1(above, left, aboveTxfm, leftTxfm, maxTxSize);
            const uint16_t *skipBits = bc.skip[ctxSkip];
            const uint16_t *txSizeBits = bc.txSize[maxTxSize][ctxTxSize];
            const uint16_t *isInterBits = bc.isInter[ctxIsInter];
            const uint32_t ctxInterp = GetCtxInterpBothAV1(above, left, mi->refIdx);
            const uint16_t *interpFilterBits0 = bc.interpFilterAV1[ctxInterp & 15];
            const uint16_t *interpFilterBits1 = bc.interpFilterAV1[ctxInterp >> 8];
            const uint16_t *intraModeBits = bc.intraModeAV1[maxTxSize];
            const Contexts origCtx = m_contexts; // contexts at the start of current block
            Contexts bestCtx;
            uint8_t *anz = m_contexts.aboveNonzero[0]+x4;
            uint8_t *lnz = m_contexts.leftNonzero[0]+y4;
            uint8_t *aboveTxfmCtx = m_contexts.aboveTxfm + x4;
            uint8_t *leftTxfmCtx  = m_contexts.leftTxfm  + y4;
            PixType *recSb = m_yRec + sbx + sby * m_pitchRecLuma;
            PixType *srcSb = m_ySrc + sbx + sby * SRC_PITCH;
            int16_t *diff = vp9scratchpad.diffY;
            int16_t *coef = vp9scratchpad.coefY;
            int16_t *qcoef = vp9scratchpad.qcoefY[0];
            int16_t *bestQcoef = vp9scratchpad.qcoefY[1];
            int16_t *coefBest = vp9scratchpad.dqcoefY;
            int16_t *coefWork = (int16_t *)vp9scratchpad.varTxCoefs[0][0].coef;
            int16_t *coefWorkBest = (int16_t *)vp9scratchpad.varTxCoefs[1][0].coef;
            int16_t *eob4x4 = (int16_t *)vp9scratchpad.varTxCoefs[2][0].coef;
            int16_t *eob4x4Best = (int16_t *)vp9scratchpad.varTxCoefs[2][1].coef;
            uint64_t bestIntraCost = ULLONG_MAX;
            PredMode bestIntraMode = DC_PRED;
            int32_t satdPairs[4];

            mi->angle_delta_y = MAX_ANGLE_DELTA; //ZERO_DELTA

            int32_t isNonRef = !m_currFrame->m_isRef;
            // check if mode should be switched to NEWMV (side-effect of swithing to INTRA)
            (m_currFrame->compoundReferenceAllowed)
                ? GetMvRefsAV1TU7B(bsz, miRow, miCol, &m_mvRefs, m_currFrame->refFrameSignBiasVp9[LAST_FRAME] != m_currFrame->refFrameSignBiasVp9[ALTREF_FRAME])
                : GetMvRefsAV1TU7P(bsz, miRow, miCol, &m_mvRefs);

            PaletteInfo *pi = &m_currFrame->m_fenc->m_Palette8x8[miCol + miRow * m_par->miPitch];
            uint8_t palette[MAX_PALETTE];
            uint16_t palette_size = 0;
            int32_t depth = 4 - log2w;
            uint8_t *color_map = &m_paletteMapY[depth][sby*MAX_CU_SIZE + sbx];
            uint32_t map_bits_est = 0;
            uint32_t map_bits = 0;
            uint32_t palette_bits = 0;
            pi->palette_size_y = 0; // Decide here for intra
            pi->color_map = NULL;
            pi->palette_size_uv = 0;

            bool tryIntra = (log2w<4) ? true : false;
#if PROTOTYPE_GPU_MODE_DECISION
            uint32_t gpuIntraData = *(uint32_t *)&mi->memCtx;
            if (m_par->TryIntra != 1 && tryIntra) {
                tryIntra = ((gpuIntraData & 0xf) < AV1_INTRA_MODES) ? true : false;
            }
#else
            if (m_par->TryIntra != 1 && tryIntra && m_currFrame->m_temporalSync) {
                if (m_par->enableCmFlag) {
                    float scpp = 0;
                    int32_t scid = GetSpatialComplexity(sbx, sby, log2w, &scpp);
                    int32_t mvd;
                    float sadpp = GetCmDistMv(sbx, sby, log2w, mvd);
                    sadpp = std::max(0.1, sadpp / (sbw*sbh));
                    tryIntra = ((IsGoodPred(scpp, sadpp, mvd)) ? false : true);
                    if (tryIntra && mi->mode != AV1_NEARESTMV && mi->skip) {
                        tryIntra = (IsBadPred(scpp, sadpp, mvd)) ? true : false;
                    }
                }
                else {
                    float scpp = 0;
                    int32_t scid = GetSpatialComplexity(sbx, sby, log2w, &scpp);
                    const PixType *bestInterPred = m_bestInterp[4 - log2w][miRow & 7][miCol & 7];
                    float sadpp = AV1PP::sad_special(srcSb, SRC_PITCH, bestInterPred, log2w, log2h);
                    sadpp = std::max(0.1, sadpp / (sbw*sbh));
                    int32_t mvd = std::abs(m_mvRefs.bestMv[mi->refIdxComb][0].mvx - mi->mv[0].mvx);
                    mvd += std::abs(m_mvRefs.bestMv[mi->refIdxComb][0].mvy - mi->mv[0].mvy);
                    if (mi->refIdx[1] != NONE_FRAME) {
                        int32_t mvd1 = std::abs(m_mvRefs.bestMv[mi->refIdxComb][1].mvx - mi->mv[1].mvx);
                        mvd1 += std::abs(m_mvRefs.bestMv[mi->refIdxComb][1].mvy - mi->mv[1].mvy);
                        if (mvd1 < mvd) mvd = mvd1;
                    }
                    tryIntra = ((IsGoodPred(scpp, sadpp, mvd)) ? false : true);
                    if (tryIntra && mi->mode != AV1_NEARESTMV && mi->skip) {
                        tryIntra = (IsBadPred(scpp, sadpp, mvd)) ? true : false;
                    }
                }
            }
#endif
#if PROTOTYPE_GPU_MODE_DECISION && GPU_INTRA_DECISION
            if (tryIntra && (gpuIntraData & 0xf) >= AV1_INTRA_MODES) {
                // just in case of mismatch of algorithms
                *(uint32_t *)&mi->memCtx = 0; gpuIntraData = 0;
            }
#endif
            AV1PP::IntraPredPels<PixType> predPels;

            if (tryIntra) {
                int32_t x = 0, y = 0;
                int32_t txSize = log2w;
                int32_t txw = tx_size_wide_unit[txSize];
                const int32_t haveRight = (miCol + ((x + txw) >> 1)) < miColEnd;
                const int32_t haveTopRight = haveTop && haveRight && HaveTopRight(log2w, miRow, miCol, txSize, y, x);
                const int32_t txwpx = 4 << txSize;
                const int32_t txhpx = txwpx;
                const int32_t bh = sbh >> 3;
                const int32_t bw = sbw >> 3;
                const int32_t distToBottomEdge = (m_par->miRows - bh - miRow) * 8;
                int hpx = block_size_high[bsz];
                int wpx = hpx;
                const int32_t yd = distToBottomEdge + (hpx - (y << 2) - txhpx);
                const int32_t haveBottom = yd > 0;
                const int32_t haveBottomLeft = haveLeft && haveBottom && HaveBottomLeft(log2w, miRow, miCol, txSize, y, x);
                const int32_t distToRightEdge = (m_par->miCols - bw - miCol) * 8;
                const int32_t xr = distToRightEdge + (wpx - (x << 2) - txwpx);
                int32_t pixTopRight = haveTopRight ? std::min(txwpx, xr) : 0;
                int32_t pixBottomLeft = haveBottomLeft ? std::min(txhpx, yd) : 0;
                const PixType* rec = (const PixType*)recSb;
                int pitchRec = m_pitchRecLuma;

                AV1PP::GetPredPelsAV1<PLANE_TYPE_Y>(rec, pitchRec, predPels.top, predPels.left, sbw, haveTop, haveLeft, pixTopRight, pixBottomLeft);
            }

#if !GPU_INTRA_DECISION
            if(tryIntra) {
            int32_t txSize = log2w;
            const int32_t width = 4 << txSize;
            // DC_PRED = 0, V_PRED = 1, H_PRED = 2, D45_PRED = 3, D135_PRED = 4, D117_PRED = 5, D153_PRED = 6, D207_PRED = 7, D63_PRED = 8, SMOOTH_PRED=9, SMOOTH_V_PRED=10, SMOOTH_H_PRED=11, PAETH_PRED=12
            static const uint8_t test_mode[13][16] =
            {
                { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }, //mode,0
                { 1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,1
                { 1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,2
                { 1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,3
                { 1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,4
                { 1,1,1,0,1,0,0,0,0,0,0,0,0,1,1,1 }, //mode,5
                { 1,0,1,0,1,0,0,0,0,0,0,0,0,1,1,1 }, //mode,6
                { 1,0,1,1,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,7
                { 1,1,0,1,0,0,0,1,0,0,0,0,0,1,1,1 }, //mode,8
                { 1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1 }, //mode,9
                { 1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1 }, //mode,10
                { 1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1 }, //mode,11
                { 1,1,1,0,0,0,0,0,0,1,1,1,0,1,1,1 }, //mode,12
            };
            switch (log2w) {
            case 1:
                {
                    for (int32_t mode = 0; mode < AV1_INTRA_MODES; mode += 2) {
                        if (IsSmoothVhMode(mode))
                            continue;

                        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + (mode + 0) * width,
                                                 14 * width, txSize, haveLeft, haveTop, mode + 0, 0, 0, 0);
                        if (mode + 1 < AV1_INTRA_MODES)
                            AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + (mode + 1) * width,
                                                     14 * width, txSize, haveLeft, haveTop, mode + 1, 0, 0, 0);

                        const PixType *pred = m_predIntraAll + mode * 8;
                        AV1PP::satd_8x8x2(srcSb, pred, 8 * 14, satdPairs);
                        int32_t satd0 = (satdPairs[0] + 2) >> 2;
                        int32_t satd1 = (satdPairs[1] + 2) >> 2;

                        int32_t deltaBits0 = 0;
                        if (av1_is_directional_mode(mode)) {
                            int32_t max_angle_delta = MAX_ANGLE_DELTA;
                            int32_t delta = 0;
                            deltaBits0 = write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
                        }

                        int32_t deltaBits1 = 0;
                        if (av1_is_directional_mode(mode + 1)) {
                            deltaBits1 = deltaBits0;
                        }

                        uint64_t cost0 = (RD(satd0, intraModeBits[mode + 0] + deltaBits0, m_rdLambdaSqrtInt) << 4) + mode + 0;
                        if (isNonRef && bestIntraCost != ULLONG_MAX && cost0 > (bestIntraCost + (bestIntraCost >> 1))) {
                            break;
                        }
                        bestIntraCost = std::min(bestIntraCost, cost0);

                        uint64_t cost1 = (RD(satd1, intraModeBits[mode + 1] + deltaBits1, m_rdLambdaSqrtInt) << 4) + mode + 1;
                        if (mode + 1 == AV1_INTRA_MODES)
                            cost1 = ULLONG_MAX;
                        else if (isNonRef && cost1 > (bestIntraCost + (bestIntraCost >> 1))) {
                            break;
                        }
                        bestIntraCost = std::min(bestIntraCost, cost1);
                    }
                }
                break;
            case 2:
                {
                    for (int32_t mode = 0; mode < AV1_INTRA_MODES; mode++) {
                        if (!test_mode[mode][bestIntraCost & 15]) continue;
                        //if (IsSmoothVhMode(mode))
                        //    continue;
                        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + mode * width, 14 * width,
                                                 txSize, haveLeft, haveTop, mode, 0, 0, 0);
                        const PixType *psrc = srcSb;
                        const PixType *pred = m_predIntraAll + mode * 16;
                        AV1PP::satd_8x8_pair(psrc, pred, 16 * 14, satdPairs);
                        pred += 8 * 16 * 14; psrc += 8 * SRC_PITCH;
                        AV1PP::satd_8x8_pair(psrc, pred, 16 * 14, satdPairs + 2);
                        int32_t satd = ((satdPairs[0] + 2) >> 2) + ((satdPairs[1] + 2) >> 2)
                                    + ((satdPairs[2] + 2) >> 2) + ((satdPairs[3] + 2) >> 2);

                        int32_t deltaBits = 0;
                        if (av1_is_directional_mode(mode)) {
                            int32_t max_angle_delta = MAX_ANGLE_DELTA;
                            int32_t delta = 0;
                            deltaBits = write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
                        }

                        uint64_t cost = (RD(satd, intraModeBits[mode] + deltaBits, m_rdLambdaSqrtInt) << 4) + mode;

                        if (isNonRef && bestIntraCost != ULLONG_MAX && cost > (bestIntraCost + (bestIntraCost >> 1))) {
                            break;
                        }

                        bestIntraCost = std::min(bestIntraCost, cost);
                    }
                }
                break;
            case 3:
                {
                    for (int32_t mode = 0; mode < AV1_INTRA_MODES; mode++) {
                        if (!test_mode[mode][bestIntraCost & 15]) continue;
                        //if (IsSmoothVhMode(mode)) continue;
                        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + mode * width, 14 * width,
                                                 txSize, haveLeft, haveTop, mode, 0, 0, 0);
                        const PixType *psrc = srcSb;
                        const PixType *pred = m_predIntraAll + mode * 32;
                        int32_t satd = 0;
                        for (int32_t j = 0; j < 32; j += 8, psrc += SRC_PITCH * 8, pred += 8 * 32 * 14) {
                            AV1PP::satd_8x8_pair(psrc + 0, pred + 0, 32 * 14, satdPairs);
                            AV1PP::satd_8x8_pair(psrc + 16, pred + 16, 32 * 14, satdPairs + 2);
                            satd += ((satdPairs[0] + 2) >> 2) + ((satdPairs[1] + 2) >> 2);
                            satd += ((satdPairs[2] + 2) >> 2) + ((satdPairs[3] + 2) >> 2);
                        }

                        int32_t deltaBits = 0;
                        if (av1_is_directional_mode(mode)) {
                            int32_t max_angle_delta = MAX_ANGLE_DELTA;
                            int32_t delta = 0;
                            deltaBits = write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
                        }
                        uint64_t cost = (RD(satd, intraModeBits[mode] + deltaBits, m_rdLambdaSqrtInt) << 4) + mode;

                        if (isNonRef && bestIntraCost != ULLONG_MAX && cost > (bestIntraCost + (bestIntraCost >> 1))) {
                            break;
                        }

                        bestIntraCost = std::min(bestIntraCost, cost);
                    }
                }
                break;
            default: assert(0);
            case 4: // no 64x64 intra
                break;
            }
            bestIntraMode = bestIntraCost & 15;
            bestIntraCost >>= 4;
            bestIntraCost += isInterBits[0] * m_rdLambdaSqrtInt;
            }
#else
            if (tryIntra) {
                gpuIntraData = *(uint32_t *)&mi->memCtx;
                bestIntraMode = gpuIntraData & 0xf;
                assert(bestIntraMode < AV1_INTRA_MODES);

                int32_t deltaBits = av1_is_directional_mode(bestIntraMode) ? write_uniform_cost(2 * MAX_ANGLE_DELTA + 1, MAX_ANGLE_DELTA) : 0;
                AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll, sbw, maxTxSize, haveLeft, haveTop, bestIntraMode, 0, 0, 0);
                int32_t satd = AV1PP::satd(srcSb, m_predIntraAll, sbw, log2w, log2h);
                bestIntraCost = RD(satd, intraModeBits[bestIntraMode] + deltaBits + isInterBits[0], m_rdLambdaSqrtInt);
                const int32_t allow_screen_content_tool_palette = m_currFrame->m_allowPalette && m_currFrame->m_isRef;
                if (mi->sbType >= BLOCK_8X8
                    && block_size_wide[mi->sbType] <= 32
                    && block_size_high[mi->sbType] <= 32
                    && allow_screen_content_tool_palette) {
                    // CHECK Palette Mode
                    palette_size = GetColorCount(srcSb, sbw, sbw, palette, m_par->bitDepthLuma, m_rdLambda, pi, color_map, map_bits_est, true);
                    if (palette_size > 1 && palette_size <= MAX_PALETTE) {
                        //map_bits_est += palette_size * 512;
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

                        AV1PP::predict_intra_palette(m_predIntraAll, sbw, log2w, palette, color_map); //use map
                        int32_t satd = AV1PP::satd(srcSb, m_predIntraAll, sbw, log2w, log2h);
                        uint64_t PaletteCost = RD(satd, intraModeBits[DC_PRED] + isInterBits[0] + palette_bits + map_bits_est, m_rdLambdaSqrtInt);


                        if (bestIntraCost > 0.5*PaletteCost && bestIntraCost < 2.0*PaletteCost)
                        {
                            // get better estimate
                            uint32_t map_bits = GetPaletteTokensCost(bc, srcSb, sbw, sbw, palette, palette_size, color_map);
                            palette_bits += map_bits;
                            PaletteCost = RD(satd, intraModeBits[DC_PRED] + isInterBits[0] + palette_bits + map_bits, m_rdLambdaSqrtInt);
                        }

                        if (PaletteCost < bestIntraCost)
                            bestIntraCost = PaletteCost;
                    }
                }
            }
            else if (log2w == 4) {
                bestIntraCost = CheckIntra64x64(&bestIntraMode);
            }
#endif

            uint32_t interBits = 0;

            m_mvRefs.refFrameBitCount[mi->refIdxComb] = EstimateRefFrameAv1(
                bc, *m_currFrame, above, left, mi->refIdxComb, &m_mvRefs.memCtx);

            const int32_t nearestMvCount = MIN(4, m_mvRefs.mvRefsAV1.nearestMvCount[mi->refIdxComb]);
            const uint16_t *newMvDrlIdxBits  = bc.drlIdxNewMv[nearestMvCount];
            const uint16_t *nearMvDrlIdxBits = bc.drlIdxNearMv[nearestMvCount];

            uint16_t interModeBits[AV1_INTER_MODES];
            const int32_t modeCtx = m_mvRefs.mvRefsAV1.interModeCtx[mi->refIdxComb];
            const int32_t newMvCtx = modeCtx & NEWMV_CTX_MASK;
            const int32_t zeroMvCtx = (modeCtx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
            const int32_t refMvCtx = (modeCtx >> REFMV_OFFSET) & REFMV_CTX_MASK;
            if (mi->refIdx[1] == NONE_FRAME) {
                interModeBits[NEWMV_]     = bc.newMvBit[newMvCtx][0];
                interModeBits[ZEROMV_]    = bc.newMvBit[newMvCtx][1] + bc.zeroMvBit[zeroMvCtx][0];
                interModeBits[NEARESTMV_] = bc.newMvBit[newMvCtx][1] + bc.zeroMvBit[zeroMvCtx][1] + bc.refMvBit[refMvCtx][0];
                interModeBits[NEARMV_]    = bc.newMvBit[newMvCtx][1] + bc.zeroMvBit[zeroMvCtx][1] + bc.refMvBit[refMvCtx][1];
            } else {
                assert(modeCtx < INTER_MODE_CONTEXTS);
                interModeBits[NEWMV_]     = bc.interCompMode[modeCtx][NEW_NEWMV - NEAREST_NEARESTMV];
                interModeBits[ZEROMV_]    = bc.interCompMode[modeCtx][ZERO_ZEROMV - NEAREST_NEARESTMV];
                interModeBits[NEARESTMV_] = bc.interCompMode[modeCtx][NEAREST_NEARESTMV - NEAREST_NEARESTMV];
                interModeBits[NEARMV_]    = bc.interCompMode[modeCtx][NEAR_NEARMV - NEAREST_NEARESTMV];
                // new compound modes
                interModeBits[NEAREST_NEWMV_] = bc.interCompMode[modeCtx][NEAREST_NEWMV - NEAREST_NEARESTMV];
                interModeBits[NEW_NEARESTMV_] = bc.interCompMode[modeCtx][NEW_NEARESTMV - NEAREST_NEARESTMV];
                interModeBits[NEAR_NEWMV_]    = bc.interCompMode[modeCtx][NEAR_NEWMV - NEAREST_NEARESTMV];
                interModeBits[NEW_NEARMV_]    = bc.interCompMode[modeCtx][NEW_NEARMV - NEAREST_NEARESTMV];
            }

            uint32_t bestBitCost = 0;
            AV1MVPair bestRefMv = m_mvRefs.bestMv[mi->refIdxComb];
            bestBitCost = interModeBits[NEWMV_] + newMvDrlIdxBits[0];
            bestBitCost += MvCost(mi->mv[0], bestRefMv[0], m_mvRefs.useMvHp[ mi->refIdx[0] ]);
            if (mi->refIdx[1] != NONE_FRAME)
                bestBitCost += MvCost(mi->mv[1], bestRefMv[1], m_mvRefs.useMvHp[ mi->refIdx[1] ]);
            mi->mode = AV1_NEWMV;
            mi->refMvIdx = 0;

            for (int32_t idx = 1; idx < 3; idx++) {
                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] < idx + 1)
                    break;
                AV1MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][idx].mv;
                ClampMvRefAV1(refMv + 0, mi->sbType, miRow, miCol, m_par);
                ClampMvRefAV1(refMv + 1, mi->sbType, miRow, miCol, m_par);
                uint32_t bitCost = interModeBits[NEWMV_] + newMvDrlIdxBits[idx];
                bitCost += MvCost(mi->mv[0], refMv[0], m_mvRefs.useMvHp[ mi->refIdx[0] ]);
                if (mi->refIdx[1] != NONE_FRAME)
                    bitCost += MvCost(mi->mv[1], refMv[1], m_mvRefs.useMvHp[ mi->refIdx[1] ]);
                if (bestBitCost > bitCost) {
                    bestBitCost = bitCost;
                    bestRefMv = refMv;
                    mi->mode = AV1_NEWMV;
                    mi->refMvIdx = idx;
                }
            }

            mi->mvd[0].mvx = mi->mv[0].mvx - bestRefMv[0].mvx;
            mi->mvd[0].mvy = mi->mv[0].mvy - bestRefMv[0].mvy;
            mi->mvd[1].mvx = mi->mv[1].mvx - bestRefMv[1].mvx;
            mi->mvd[1].mvy = mi->mv[1].mvy - bestRefMv[1].mvy;


            if (mi->mv.asInt64 == m_mvRefs.mvs[mi->refIdxComb][NEARESTMV_].asInt64) {
                uint32_t bitCost = interModeBits[NEARESTMV_];
                if (bestBitCost > bitCost) {
                    bestBitCost = bitCost;
                    mi->mode = AV1_NEARESTMV;
                    mi->refMvIdx = 0;
                }
            }

            if (mi->mv.asInt64 == m_mvRefs.mvs[mi->refIdxComb][NEARMV_].asInt64) {
                uint32_t bitCost = interModeBits[NEARMV_] + nearMvDrlIdxBits[0];
                if (bestBitCost > bitCost) {
                    bestBitCost = bitCost;
                    mi->mode = AV1_NEARMV;
                    mi->refMvIdx = 0;
                }
            }

            if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 2 && mi->mv.asInt64 == m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][2].mv.asInt64) {
                uint32_t bitCost = interModeBits[NEARMV_] + nearMvDrlIdxBits[1];
                if (bestBitCost > bitCost) {
                    bestBitCost = bitCost;
                    mi->mode = AV1_NEARMV;
                    mi->refMvIdx = 1;
                }
            }

            if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] > 3 && mi->mv.asInt64 == m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb][3].mv.asInt64) {
                uint32_t bitCost = interModeBits[NEARMV_] + nearMvDrlIdxBits[2];
                if (bestBitCost > bitCost) {
                    bestBitCost = bitCost;
                    mi->mode = AV1_NEARMV;
                    mi->refMvIdx = 2;
                }
            }

            if (mi->mv.asInt64 == 0) {
                uint32_t bitCost = interModeBits[ZEROMV_];
                if (bestBitCost > bitCost) {
                    bestBitCost = bitCost;
                    mi->mode = AV1_ZEROMV;
                    mi->refMvIdx = 0;
                }
            }

            if (mi->refIdx[1] != NONE_FRAME && mi->mode == AV1_NEWMV) {
                CheckNewCompoundModes(mi, m_mvRefs, interModeBits, nearMvDrlIdxBits, bestBitCost, miRow, miCol, m_par);
            }

            int32_t numDrlBits = 0;
            int32_t ctxDrl0 = 0;
            int32_t ctxDrl1 = 0;
            if (mi->mode == AV1_NEWMV) {
                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] == 2) numDrlBits = 1;
                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] >= 3) numDrlBits = 2;
                ctxDrl0 = GetCtxDrlIdx(m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb], 0);
                ctxDrl1 = GetCtxDrlIdx(m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb], 1);
            } else if (have_nearmv_in_inter_mode(mi->mode)) {
                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] == 3) numDrlBits = 1;
                if (m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb] >= 4) numDrlBits = 2;
                ctxDrl0 = GetCtxDrlIdx(m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb], 1);
                ctxDrl1 = GetCtxDrlIdx(m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb], 2);
            }

            interBits = bestBitCost + (interpFilterBits0[mi->interp & 0x3] + interpFilterBits1[(mi->interp >> 4) & 0x3]) + isInterBits[1] + m_mvRefs.refFrameBitCount[mi->refIdxComb];

#if PROTOTYPE_GPU_MODE_DECISION
            PixType *pred = vp9scratchpad.predY[0];

            const int32_t refYColocOffset = m_ctbPelX + sbx + (m_ctbPelY + sby) * m_pitchRecLuma;
            const PixType *ref0Y = refFramesY[mi->refIdx[0]] + refYColocOffset;

            AV1MV clampedMV[2] = { mi->mv[0], mi->mv[1] };
            //ClampMvRefAV1(&clampedMV[0], bsz, miRow, miCol, m_par);
            //ClampMvRefAV1(&clampedMV[1], bsz, miRow, miCol, m_par);
            clampedMV[0].mvx = clampMvColAV1(clampedMV[0].mvx, 64 + 4, bsz, miCol, m_par->miCols);
            clampedMV[0].mvy = clampMvRowAV1(clampedMV[0].mvy, 64 + 4, bsz, miRow, m_par->miRows);
            clampedMV[1].mvx = clampMvColAV1(clampedMV[1].mvx, 64 + 4, bsz, miCol, m_par->miCols);
            clampedMV[1].mvy = clampMvRowAV1(clampedMV[1].mvy, 64 + 4, bsz, miRow, m_par->miRows);


            if (m_par->CmInterpFlag) {
                if (!joined)
                    CopyNxN(recSb, m_pitchRecLuma, pred, 64, sbw);
                else
                    pred = predSaved;
            }
            else
            {
                if (mi->refIdx[1] == NONE_FRAME) {
                    if (m_par->superResFlag) {
                        FrameData* ref = m_currFrame->refFramesVp9[mi->refIdx[0]]->m_reconUpscale;
                        InterpolateAv1SingleRefLuma_ScaleMode_new(ref, (uint8_t*)pred, clampedMV[0], mix, miy, sbw, sbh, EIGHTTAP);
                    }
                    else
                    {
                        InterpolateAv1SingleRefLuma(ref0Y, m_pitchRecLuma, pred, clampedMV[0], sbh, log2w, EIGHTTAP);
                    }
                }
                else {
                    if (m_par->superResFlag)
                    {
                        FrameData* ref0 = m_currFrame->refFramesVp9[mi->refIdx[0]]->m_reconUpscale;
                        InterpolateAv1FirstRefLuma_ScaleMode_new(ref0, (int16_t*)vp9scratchpad.compPredIm, clampedMV[0], mix, miy, sbw, sbh, EIGHTTAP);
                        FrameData* ref1 = m_currFrame->refFramesVp9[mi->refIdx[1]]->m_reconUpscale;
                        InterpolateAv1SecondRefLuma_ScaleMode_new(ref1, (const int16_t*)vp9scratchpad.compPredIm, (uint8_t*)pred, clampedMV[1], mix, miy, sbw, sbh, EIGHTTAP);
                    }
                    else {
                        InterpolateAv1FirstRefLuma(ref0Y, m_pitchRecLuma, vp9scratchpad.compPredIm, clampedMV[0], sbh, log2w, EIGHTTAP);
                        const PixType *ref1Y = refFramesY[mi->refIdx[1]] + refYColocOffset;
                        InterpolateAv1SecondRefLuma(ref1Y, m_pitchRecLuma, vp9scratchpad.compPredIm, pred, clampedMV[1], sbh, log2w, EIGHTTAP);
                    }
                }
            }


            const int32_t bestInterSatd = AV1PP::satd(srcSb, pred, log2w, log2h);
#else //PROTOTYPE_GPU_MODE_DECISION
            const PixType *bestInterPred = m_bestInterp[4 - log2w][miRow & 7][miCol & 7];
            const int32_t bestInterSatd = AV1PP::satd(bestInterPred, srcSb, log2w, log2h);
#endif // PROTOTYPE_GPU_MODE_DECISION

            uint64_t bestInterCost = RD(bestInterSatd, interBits, m_rdLambdaSqrtInt);

            if (bestInterCost < bestIntraCost) { // keep INTER
                m_mvRefs.memCtx.skip = ctxSkip;
                m_mvRefs.memCtx.isInter = ctxIsInter;
                //m_mvRefs.memCtx.interp = ctxInterp;
                m_mvRefs.memCtx.txSize = ctxTxSize;

                int32_t bestEobY = !mi->skip;
                TxSize bestTxSize = maxTxSize;
                TxType bestTxType = DCT_DCT;
                int32_t bestInterp0 = mi->interp0;
                int32_t bestInterp1 = mi->interp1;
                TxType bestChromaTxType = DCT_DCT;

                const int32_t refUvColocOffset = m_ctbPelX + sbx + ((m_ctbPelY + sby) >> 1) * m_pitchRecChroma;
                const PixType *ref0Uv = refFramesUv[ mi->refIdx[0] ] + refUvColocOffset;
                const PixType *ref1Y  = NULL;
                const PixType *ref1Uv = NULL;
                if (!m_par->CmInterpFlag) {
                    if (mi->refIdx[1] != NONE_FRAME) {
                        ref1Y = refFramesY[mi->refIdx[1]] + refYColocOffset;
                        ref1Uv = refFramesUv[mi->refIdx[1]] + refUvColocOffset;
                    }
                }



                const int hasSubpel = HasSubpel(mi->refIdx, clampedMV);


#if GPU_VARTX
                uint16_t *varTxInfo = (uint16_t *)m_currFrame->m_feiVarTxInfo->m_sysmem;
                varTxInfo += m_ctbAddr * 256 + (miRow & 7) * 2 * 16 + (miCol & 7) * 2;
                int16_t *coef = (int16_t *)m_currFrame->m_feiCoefs->m_sysmem;
                coef += m_ctbAddr * 4096 + i * 16;
#else // GPU_VARTX
                alignas(32) uint16_t varTxInfo[16 * 16] = {};// {[0-3] bits - txType, [4-5] - txSize, [6] - eobFlag or [6-15] - eob/num non zero coefs}
#endif // GPU_VARTX

                if (m_par->maxTxDepthInterRefine > 1) {
                    if (mi->skip) {

                        small_memset0(anz, num4x4w);
                        small_memset0(lnz, num4x4h);
                        if (m_par->switchInterpFilterRefine) {
                            const int32_t sseShiftY = m_par->bitDepthLumaShift << 1;
                            PixType *bestPred = vp9scratchpad.predY[0];

                            if (!m_par->CmInterpFlag) {
                                bestInterp1 = bestInterp0 = DEF_FILTER;
                                if (hasSubpel) {
                                    const int32_t bestInterSse = AV1PP::sse_p64_p64(srcSb, bestPred, log2w, log2h);
                                    CostType bestCost = bestInterSse + m_rdLambda * (interpFilterBits0[EIGHTTAP] + interpFilterBits1[EIGHTTAP]);
                                    int32_t freeIdx = 1;

                                    // first pass
                                    for (int32_t interp0 = EIGHTTAP + 1; interp0 < SWITCHABLE_FILTERS; interp0++) {
#if USE_HWPAK_RESTRICT
                                        int32_t interp1 = interp0;
#else
                                        int32_t interp1 = DEF_FILTER;
#endif
                                        int32_t interp = interp0 + (interp1 << 4);
                                        pred = vp9scratchpad.predY[freeIdx];
                                        if (mi->refIdx[1] == NONE_FRAME) {
                                            if (m_par->superResFlag) {
                                                FrameData* ref = m_currFrame->refFramesVp9[mi->refIdx[0]]->m_reconUpscale;
                                                InterpolateAv1SingleRefLuma_ScaleMode_new(ref, (uint8_t*)pred, clampedMV[0], mix, miy, sbw, sbh, interp);
                                            }
                                            else {
                                                InterpolateAv1SingleRefLuma(ref0Y, m_pitchRecLuma, pred, clampedMV[0], sbh, log2w, interp);
                                            }
                                        }
                                        else {
                                            if (m_par->superResFlag)
                                            {
                                                FrameData* ref0 = m_currFrame->refFramesVp9[mi->refIdx[0]]->m_reconUpscale;
                                                InterpolateAv1FirstRefLuma_ScaleMode_new(ref0, (int16_t*)vp9scratchpad.compPredIm, clampedMV[0], mix, miy, sbw, sbh, interp);
                                                FrameData* ref1 = m_currFrame->refFramesVp9[mi->refIdx[1]]->m_reconUpscale;
                                                InterpolateAv1SecondRefLuma_ScaleMode_new(ref1, (int16_t*)vp9scratchpad.compPredIm, (uint8_t*)pred, clampedMV[1], mix, miy, sbw, sbh, interp);
                                            }
                                            else
                                            {
                                                InterpolateAv1FirstRefLuma(ref0Y, m_pitchRecLuma, vp9scratchpad.compPredIm, clampedMV[0], sbh, log2w, interp);
                                                InterpolateAv1SecondRefLuma(ref1Y, m_pitchRecLuma, vp9scratchpad.compPredIm, pred, clampedMV[1], sbh, log2w, interp);
                                            }
                                        }
                                        int32_t sse = AV1PP::sse_p64_p64(srcSb, pred, log2w, log2h);

                                        CostType cost = sse + m_rdLambda * (interpFilterBits0[interp0] + interpFilterBits1[interp1]);
                                        if (bestCost > cost) {
                                            bestCost = cost;
                                            bestPred = pred;
                                            bestInterp1 = interp1;
                                            bestInterp0 = interp0;
                                            freeIdx = 1 - freeIdx;
                                        }
                                    }
#if !USE_HWPAK_RESTRICT
                                    // second pass
                                    for (int32_t interp1 = 1; interp1 < SWITCHABLE_FILTERS; interp1++) {
                                        int32_t interp0 = bestInterp0;
                                        int32_t interp = interp0 + (interp1 << 4);
                                        PixType *pred = vp9scratchpad.predY[freeIdx];
                                        if (mi->refIdx[1] == NONE_FRAME) {
                                            if (m_par->superResFlag) {
                                                FrameData* ref = m_currFrame->refFramesVp9[mi->refIdx[0]]->m_reconUpscale;
                                                InterpolateAv1SingleRefLuma_ScaleMode_new(ref, (uint8_t*)pred, clampedMV[0], mix, miy, sbw, sbh, interp);
                                            }
                                            else {
                                                InterpolateAv1SingleRefLuma(ref0Y, m_pitchRecLuma, pred, clampedMV[0], sbh, log2w, interp);
                                            }
                                        }
                                        else {
                                            if (m_par->superResFlag)
                                            {
                                                FrameData* ref0 = m_currFrame->refFramesVp9[mi->refIdx[0]]->m_reconUpscale;
                                                InterpolateAv1FirstRefLuma_ScaleMode_new(ref0, (int16_t*)vp9scratchpad.compPredIm, clampedMV[0], mix, miy, sbw, sbh, interp);
                                                FrameData* ref1 = m_currFrame->refFramesVp9[mi->refIdx[1]]->m_reconUpscale;
                                                InterpolateAv1SecondRefLuma_ScaleMode_new(ref1, (int16_t*)vp9scratchpad.compPredIm, (uint8_t*)pred, clampedMV[1], mix, miy, sbw, sbh, interp);

                                            }
                                            else
                                            {
                                                InterpolateAv1FirstRefLuma(ref0Y, m_pitchRecLuma, vp9scratchpad.compPredIm, clampedMV[0], sbh, log2w, interp);
                                                InterpolateAv1SecondRefLuma(ref1Y, m_pitchRecLuma, vp9scratchpad.compPredIm, pred, clampedMV[1], sbh, log2w, interp);

                                            }
                                        }
                                        int32_t sse = AV1PP::sse_p64_p64(srcSb, pred, log2w, log2h);

                                        CostType cost = sse + m_rdLambda * (interpFilterBits0[interp0] + interpFilterBits1[interp1]);
                                        if (bestCost > cost) {
                                            bestCost = cost;
                                            bestPred = pred;
                                            bestInterp1 = interp1;
                                            bestInterp0 = interp0;
                                            freeIdx = 1 - freeIdx;
                                        }
                                    }
#endif
                                }
                                CopyNxN(bestPred, 64, recSb, m_pitchRecLuma, sbw);
                                mi->interp0 = bestInterp0;
                                mi->interp1 = bestInterp1;
                            }

                        }

                    } else {
                        PixType *rec = vp9scratchpad.recY[0];
                        PixType *bestPredY = vp9scratchpad.predY[0];


                        if (!m_par->CmInterpFlag) {
                            uint64_t bestSatdCost = RD(bestInterSatd, interpFilterBits0[EIGHTTAP] + interpFilterBits1[EIGHTTAP], m_rdLambdaSqrtInt);
                            int32_t freeIdx = 1;
                            bestInterp1 = bestInterp0 = DEF_FILTER;

                            if (hasSubpel) {
                                // first pass
                                for (int32_t interp0 = EIGHTTAP + 1; interp0 < SWITCHABLE_FILTERS; interp0++) {
#if USE_HWPAK_RESTRICT
                                    int32_t interp1 = interp0;
#else
                                    int32_t interp1 = DEF_FILTER;
#endif
                                    {
                                        int32_t interp = interp0 + (interp1 << 4);
                                        pred = vp9scratchpad.predY[freeIdx];
                                        if (mi->refIdx[1] == NONE_FRAME) {
                                            if (m_par->superResFlag) {
                                                FrameData* ref = m_currFrame->refFramesVp9[mi->refIdx[0]]->m_reconUpscale;
                                                InterpolateAv1SingleRefLuma_ScaleMode_new(ref, (uint8_t*)pred, clampedMV[0], mix, miy, sbw, sbh, interp);
                                            }
                                            else {
                                                InterpolateAv1SingleRefLuma(ref0Y, m_pitchRecLuma, pred, clampedMV[0], sbh, log2w, interp);
                                            }
                                        }
                                        else {
                                            if (m_par->superResFlag)
                                            {
                                                FrameData* ref0 = m_currFrame->refFramesVp9[mi->refIdx[0]]->m_reconUpscale;
                                                InterpolateAv1FirstRefLuma_ScaleMode_new(ref0, (int16_t*)vp9scratchpad.compPredIm, clampedMV[0], mix, miy, sbw, sbh, interp);
                                                FrameData* ref1 = m_currFrame->refFramesVp9[mi->refIdx[1]]->m_reconUpscale;
                                                InterpolateAv1SecondRefLuma_ScaleMode_new(ref1, (int16_t*)vp9scratchpad.compPredIm, (uint8_t*)pred, clampedMV[1], mix, miy, sbw, sbh, interp);

                                            }
                                            else
                                            {
                                                InterpolateAv1FirstRefLuma(ref0Y, m_pitchRecLuma, vp9scratchpad.compPredIm, clampedMV[0], sbh, log2w, interp);
                                                InterpolateAv1SecondRefLuma(ref1Y, m_pitchRecLuma, vp9scratchpad.compPredIm, pred, clampedMV[1], sbh, log2w, interp);
                                            }
                                        }
                                        int32_t dist = AV1PP::satd(srcSb, pred, log2w, log2h);
                                        uint64_t cost = RD(dist, interpFilterBits0[interp0] + interpFilterBits1[interp1], m_rdLambdaSqrtInt);

                                        if (bestSatdCost > cost) {
                                            bestSatdCost = cost;
                                            bestPredY = pred;
                                            bestInterp1 = interp1;
                                            bestInterp0 = interp0;
                                            freeIdx = 1 - freeIdx;
                                        }
                                    }
                                }
#if !USE_HWPAK_RESTRICT
                                // second pass
                                for (int32_t interp1 = 1; interp1 < SWITCHABLE_FILTERS; interp1++) {
                                    int32_t interp0 = bestInterp0;
                                    int32_t interp = interp0 + (interp1 << 4);
                                    PixType *pred = vp9scratchpad.predY[freeIdx];
                                    if (mi->refIdx[1] == NONE_FRAME) {
                                        if (m_par->superResFlag) {
                                            FrameData* ref = m_currFrame->refFramesVp9[mi->refIdx[0]]->m_reconUpscale;
                                            InterpolateAv1SingleRefLuma_ScaleMode_new(ref, (uint8_t*)pred, clampedMV[0], mix, miy, sbw, sbh, interp);
                                        }
                                        else {
                                            InterpolateAv1SingleRefLuma(ref0Y, m_pitchRecLuma, pred, clampedMV[0], sbh, log2w, interp);
                                        }
                                    }
                                    else {
                                        if (m_par->superResFlag)
                                        {
                                            FrameData* ref0 = m_currFrame->refFramesVp9[mi->refIdx[0]]->m_reconUpscale;
                                            InterpolateAv1FirstRefLuma_ScaleMode_new(ref0, (int16_t*)vp9scratchpad.compPredIm, clampedMV[0], mix, miy, sbw, sbh, interp);
                                            FrameData* ref1 = m_currFrame->refFramesVp9[mi->refIdx[1]]->m_reconUpscale;
                                            InterpolateAv1SecondRefLuma_ScaleMode_new(ref1, (int16_t*)vp9scratchpad.compPredIm, (uint8_t*)pred, clampedMV[1], mix, miy, sbw, sbh, interp);

                                        }
                                        else {
                                            InterpolateAv1FirstRefLuma(ref0Y, m_pitchRecLuma, vp9scratchpad.compPredIm, clampedMV[0], sbh, log2w, interp);
                                            InterpolateAv1SecondRefLuma(ref1Y, m_pitchRecLuma, vp9scratchpad.compPredIm, pred, clampedMV[1], sbh, log2w, interp);
                                        }
                                    }
                                    int32_t dist = AV1PP::satd(srcSb, pred, log2w, log2h);
                                    uint64_t cost = RD(dist, interpFilterBits0[interp0] + interpFilterBits1[interp1], m_rdLambdaSqrtInt);

                                    if (bestSatdCost > cost) {
                                        bestSatdCost = cost;
                                        bestPredY = pred;
                                        bestInterp1 = interp1;
                                        bestInterp0 = interp0;
                                        freeIdx = 1 - freeIdx;
                                    }
                                }
#endif
                            }
                        } else
                        {
                            bestPredY = pred;
                        }

                        RdCost rd = {};
#if GPU_VARTX
                        rd.eob = BuildVarTxRecon(bsz, maxTxSize, bestPredY, diff, coef, m_coeffWorkY + i * 16, coefBest, varTxInfo, qparamY, m_roundFAdjY);
#else // GPU_VARTX

                        AV1PP::diff_nxn(srcSb, bestPredY, diff, log2w);
                        CopyNxN(bestPredY, 64, rec, 64, sbw);
                        rd = TransformInterYSbVarTxByRdo(
                            bsz, anz, lnz, srcSb, rec, diff, m_coeffWorkY + i * 16, m_lumaQp, bc, m_rdLambda,
                            aboveTxfmCtx, leftTxfmCtx, varTxInfo, qparamY, m_roundFAdjY, vp9scratchpad.varTxCoefs, MAX_VARTX_DEPTH, TX_TYPES-1);

                        const uint32_t modeBits = (rd.eob ? skipBits[0] + rd.modeBits : skipBits[1]);
                        const uint32_t coefBits = (rd.eob ? ((rd.coefBits * 3) >> 2) : 0); // Adjusted for better skip decision
                        const CostType cost = rd.sse + m_rdLambda * (modeBits + coefBits);
                        const CostType costZeroBlock = rd.ssz + m_rdLambda * skipBits[1];
                        if (cost >= costZeroBlock) {
                            rd.eob = 0;
                            ZeroCoeffs(m_coeffWorkY + i * 16, num4x4 * 16);
                        }
#endif // GPU_VARTX

                        if (rd.eob == 0) {
                            bestEobY = 0;
                            small_memset0(anz, num4x4w);
                            small_memset0(lnz, num4x4h);

                            if (!m_par->CmInterpFlag) {
                                // when interpolation filter is decided by GPU
                                // predicted pixel are already in reconstracted frame
                                CopyNxN(bestPredY, 64, recSb, m_pitchRecLuma, sbw);
                            }
                            else
                            {
                                if (joined)
                                    CopyNxN(bestPredY, 64, recSb, m_pitchRecLuma, sbw);
                            }

                        } else {
                            bestTxSize = maxTxSize;
                            bestEobY = rd.eob;
#if GPU_VARTX
                            CopyNxN(bestPredY, 64, recSb, m_pitchRecLuma, sbw);
#else //GPU_VARTX
                            CopyNxN(rec, 64, recSb, m_pitchRecLuma, sbw);
#endif // GPU_VARTX
                        }
                    }
                }

                // propagate txkTypes
                if (bestEobY == 0)
                    SetVarTxInfo(varTxInfo, DCT_DCT + (maxTxSize << 4), num4x4w);

                // INTER CHROMA
                const QuantParam (&qparamUv)[2] = m_aqparamUv;
                float *roundFAdjUv[2] = { &m_roundFAdjUv[0][0], &m_roundFAdjUv[1][0] };

                const int32_t subX = m_par->subsamplingX;
                const int32_t subY = m_par->subsamplingY;
                const BlockSize bszUv = ss_size_lookup[bsz][m_par->subsamplingX][m_par->subsamplingY];
                const TxSize txSizeUv = max_txsize_rect_lookup[bszUv];
                const int32_t uvsbx = sbx >> 1;
                const int32_t uvsby = sby >> 1;
                const int32_t uvsbw = sbw >> 1;
                const int32_t uvsbh = sbh >> 1;

                const PixType *srcUv = m_uvSrc + uvsbx * 2 + uvsby * SRC_PITCH;
                PixType *recUv = m_uvRec + uvsbx * 2 + uvsby * m_pitchRecChroma;
                PixType *predUv = vp9scratchpad.predUv[0];
                int16_t *diffU = vp9scratchpad.diffU;
                int16_t *diffV = vp9scratchpad.diffV;
                int16_t *coefU = vp9scratchpad.coefU;
                int16_t *coefV = vp9scratchpad.coefV;
                int16_t *qcoefU = m_coeffWorkU + i * 4;
                int16_t *qcoefV = m_coeffWorkV + i * 4;
                uint8_t *anzU = m_contexts.aboveNonzero[1] + (uvsbx >> 2);
                uint8_t *anzV = m_contexts.aboveNonzero[2] + (uvsbx >> 2);
                uint8_t *lnzU = m_contexts.leftNonzero[1] + (uvsby >> 2);
                uint8_t *lnzV = m_contexts.leftNonzero[2] + (uvsby >> 2);


                if (!m_par->CmInterpFlag) {
                    int32_t bestInterp = bestInterp0 + (bestInterp1 << 4);
                    if (mi->refIdx[1] == NONE_FRAME) {
                        if (m_par->superResFlag)
                        {
                            int pitchRecUpscaleChroma = m_currFrame->refFramesVp9[0]->m_reconUpscale->pitch_chroma_pix;
                            SubpelParams subpel_params;
                            PadBlock block;
                            int mi_x = m_ctbPelX + sbx;
                            int mi_y = m_ctbPelY + sby;
                            calc_subpel_params(clampedMV[0], 1,
                                &subpel_params,
                                sbw >> 1, sbh >> 1, &block,
                                mi_x >> 1, mi_y >> 1,
                                m_par->sourceWidth >> 1, m_par->Height >> 1);
                            const uint8_t* ref0Uv_upscaled = (uint8_t*)refFramesUv_Upscale[mi->refIdx[0]] + block.y0 * pitchRecUpscaleChroma + block.x0 * 2;
                            InterpolateAv1SingleRefChroma_ScaleMode(ref0Uv_upscaled, pitchRecUpscaleChroma, (uint8_t*)predUv, clampedMV[0], sbw >> 1, sbh >> 1, bestInterp, &subpel_params);
                        }
                        else
                        {
                            InterpolateAv1SingleRefChroma(ref0Uv, m_pitchRecChroma, predUv, clampedMV[0], uvsbh,
                                log2w - 1, bestInterp);
                        }
                    }
                    else {
                        if (m_par->superResFlag)
                        {
                            int pitchRecUpscaleChroma = m_currFrame->refFramesVp9[0]->m_reconUpscale->pitch_chroma_pix;
                            SubpelParams subpel_params;
                            PadBlock block;
                            int mi_x = m_ctbPelX + sbx;
                            int mi_y = m_ctbPelY + sby;

                            // [1]
                            calc_subpel_params(clampedMV[0], 1,
                                &subpel_params,
                                sbw >> 1, sbh >> 1, &block,
                                mi_x >> 1, mi_y >> 1,
                                m_par->sourceWidth >> 1, m_par->Height >> 1);
                            uint8_t* ref0Uv_ = (uint8_t*)refFramesUv_Upscale[mi->refIdx[0]] + block.y0 * pitchRecUpscaleChroma + block.x0 * 2;
                            InterpolateAv1FirstRefChroma_ScaleMode(ref0Uv_, pitchRecUpscaleChroma, (uint16_t*)vp9scratchpad.compPredIm, clampedMV[0], sbw >> 1, sbh >> 1, bestInterp, &subpel_params);

                            // [2]
                            calc_subpel_params(clampedMV[1], 1,
                                &subpel_params,
                                sbw >> 1, sbh >> 1, &block,
                                mi_x >> 1, mi_y >> 1,
                                m_par->sourceWidth >> 1, m_par->Height >> 1);
                            uint8_t* ref1Uv_ = (uint8_t*)refFramesUv_Upscale[mi->refIdx[1]] + block.y0 * pitchRecUpscaleChroma + block.x0 * 2;
                            InterpolateAv1SecondRefChroma_ScaleMode(ref1Uv_, pitchRecUpscaleChroma, (uint16_t*)vp9scratchpad.compPredIm, (uint8_t*)predUv, clampedMV[0], sbw >> 1, sbh >> 1, bestInterp, &subpel_params);
                        }
                        else
                        {
                            InterpolateAv1FirstRefChroma(ref0Uv, m_pitchRecChroma, vp9scratchpad.compPredIm, clampedMV[0],
                                uvsbh, log2w - 1, bestInterp);
                            InterpolateAv1SecondRefChroma(ref1Uv, m_pitchRecChroma, vp9scratchpad.compPredIm, predUv,
                                clampedMV[1], uvsbh, log2w - 1, bestInterp);
                        }
                    }

                    AV1PP::diff_nv12(srcUv, predUv, diffU, diffV, uvsbh, log2w - 1);
                    CopyNxM(predUv, PRED_PITCH, recUv, m_pitchRecChroma, uvsbw << 1, uvsbh);
                }
                else
                {
                    if (!joined) {
                        AV1PP::diff_nv12(srcUv, SRC_PITCH, recUv, m_pitchRecChroma, diffU, diffV, uvsbw, uvsbh, log2w - 1);
                    }
                    else {
                        AV1PP::diff_nv12(srcUv, predUvSaved, diffU, diffV, uvsbh, log2w - 1);
                        CopyNxM(predUvSaved, PRED_PITCH, recUv, m_pitchRecChroma, uvsbw << 1, uvsbh);
                    }
                }


                int32_t uvEob = TransformInterUvSbTxType(bszUv, txSizeUv, bestChromaTxType, anzU, anzV, lnzU, lnzV, srcUv,
                                                         recUv, m_pitchRecChroma, diffU, diffV, coefU, coefV, qcoefU,
                                                         qcoefV, coefWork, *m_par, bc, m_chromaQp, varTxInfo, qparamUv, &roundFAdjUv[0],
                                                         m_rdLambda, (m_currFrame->m_picCodeType == MFX_FRAMETYPE_P && m_currFrame->m_pyramidLayer == 0 && bestEobY!=0) ? 0 : 1);

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
                mi->memCtx.singleRefP1 = m_mvRefs.memCtx.singleRefP1;
                mi->memCtx.singleRefP2 = m_mvRefs.memCtx.singleRefP2;
                mi->memCtx.compMode    = m_mvRefs.memCtx.compMode;
                mi->memCtx.compRef     = m_mvRefs.memCtx.compRef;
                mi->memCtx.skip        = ctxSkip;
                mi->memCtx.txSize      = ctxTxSize;
                mi->memCtx.isInter     = ctxIsInter;
                mi->memCtx.AV1.numDrlBits = numDrlBits;
                mi->memCtxAV1_.drl0    = ctxDrl0;
                mi->memCtxAV1_.drl1    = ctxDrl1;
                mi->memCtxAV1_.nmv0    = GetCtxNmv(m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb], 0, mi->refMvIdx + (mi->mode == NEW_NEARMV ? 1 : 0));
                mi->memCtxAV1_.nmv1    = GetCtxNmv(m_mvRefs.mvRefsAV1.refMvCount[mi->refIdxComb], m_mvRefs.mvRefsAV1.refMvStack[mi->refIdxComb], 1, mi->refMvIdx + (mi->mode == NEAR_NEWMV ? 1 : 0));

                mi->memCtx.AV1.interMode = m_mvRefs.mvRefsAV1.interModeCtx[mi->refIdxComb]/* & UP_TO_ALL_ZERO_FLAG_MASK*/;

                PropagateSubPart(mi, m_par->miPitch, sbw >> 3, sbh >> 3);
                CopyVarTxInfo(varTxInfo, m_currFrame->m_fenc->m_txkTypes4x4 + m_ctbAddr * 256 + (miRow & 7) * 32 + (miCol & 7) * 2, num4x4w);
                CopyTxSizeInfo(varTxInfo, mi, m_par->miPitch, num4x4w);
                PropagatePaletteInfo(m_currFrame->m_fenc->m_Palette8x8, miRow, miCol, m_par->miPitch, sbw >> 3, sbh >> 3);
            } else { // switch to INTRA

#if GPU_INTRA_DECISION
                if(1) {
#if 0
                uint64_t gpuIntraCost = bestIntraCost;
                gpuIntraCost -= isInterBits[0] * m_rdLambdaSqrtInt;
                gpuIntraCost <<= 4;
                gpuIntraCost += bestIntraMode;
                int32_t gpuIntraMode = bestIntraMode;
#endif

                bestIntraCost = ULLONG_MAX;
                bestIntraMode = DC_PRED;

                int32_t txSize = log2w;
                const int32_t width = 4 << txSize;
                // DC_PRED = 0, V_PRED = 1, H_PRED = 2, D45_PRED = 3, D135_PRED = 4, D117_PRED = 5, D153_PRED = 6, D207_PRED = 7, D63_PRED = 8, SMOOTH_PRED=9, SMOOTH_V_PRED=10, SMOOTH_H_PRED=11, PAETH_PRED=12
                static const uint8_t test_mode[13][16] =
                {
                    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }, //mode,0
                    { 1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,1
                    { 1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,2
                    { 1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,3
                    { 1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,4
                    { 1,1,1,0,1,0,0,0,0,0,0,0,0,1,1,1 }, //mode,5
                    { 1,0,1,0,1,0,0,0,0,0,0,0,0,1,1,1 }, //mode,6
                    { 1,0,1,1,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,7
                    { 1,1,0,1,0,0,0,1,0,0,0,0,0,1,1,1 }, //mode,8
                    { 1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1 }, //mode,9
                    { 1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1 }, //mode,10
                    { 1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1 }, //mode,11
                    { 1,1,1,0,0,0,0,0,0,1,1,1,0,1,1,1 }, //mode,12
                };
                switch (log2w) {
                case 1:
                {
                    for (PredMode mode = 0; mode < AV1_INTRA_MODES; mode += 2) {
                        if (IsSmoothVhMode(mode))
                            continue;

                        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + (mode + 0) * width,
                            14 * width, txSize, haveLeft, haveTop, mode + 0, 0, 0, 0);
                        if (mode + 1 < AV1_INTRA_MODES)
                            AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + (mode + 1) * width,
                                14 * width, txSize, haveLeft, haveTop, mode + 1, 0, 0, 0);

                        const PixType *predIntra = m_predIntraAll + mode * 8;
                        AV1PP::satd_8x8x2(srcSb, predIntra, 8 * 14, satdPairs);
                        int32_t satd0 = (satdPairs[0] + 2) >> 2;
                        int32_t satd1 = (satdPairs[1] + 2) >> 2;

                        int32_t deltaBits0 = 0;
                        if (av1_is_directional_mode(mode)) {
                            int32_t max_angle_delta = MAX_ANGLE_DELTA;
                            int32_t delta = 0;
                            deltaBits0 = write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
                        }

                        int32_t deltaBits1 = 0;
                        if (av1_is_directional_mode(mode + 1)) {
                            deltaBits1 = deltaBits0;
                        }

                        uint64_t cost0 = (RD(satd0, intraModeBits[mode + 0] + deltaBits0, m_rdLambdaSqrtInt) << 4) + mode + 0;
                        if (isNonRef && bestIntraCost != ULLONG_MAX && cost0 > (bestIntraCost + (bestIntraCost >> 1))) {
                            break;
                        }
                        bestIntraCost = std::min(bestIntraCost, cost0);

                        uint64_t cost1 = (RD(satd1, intraModeBits[mode + 1] + deltaBits1, m_rdLambdaSqrtInt) << 4) + mode + 1;
                        if (mode + 1 == AV1_INTRA_MODES)
                            cost1 = ULLONG_MAX;
                        else if (isNonRef && cost1 > (bestIntraCost + (bestIntraCost >> 1))) {
                            break;
                        }
                        bestIntraCost = std::min(bestIntraCost, cost1);
                    }
                }
                break;
                case 2:
                {
                    for (PredMode mode = 0; mode < AV1_INTRA_MODES; mode++) {
#if 0
                        if (mode == gpuIntraMode) {
                            bestIntraCost = std::min(bestIntraCost, gpuIntraCost);
                            continue;
                        }
#endif
                        if (!test_mode[mode][bestIntraCost & 15]) continue;
                        //if (IsSmoothVhMode(mode))
                        //    continue;
                        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + mode * width, 14 * width,
                            txSize, haveLeft, haveTop, mode, 0, 0, 0);
                        const PixType *psrc = srcSb;
                        const PixType *predIntra = m_predIntraAll + mode * 16;
                        AV1PP::satd_8x8_pair(psrc, predIntra, 16 * 14, satdPairs);
                        predIntra += 8 * 16 * 14; psrc += 8 * SRC_PITCH;
                        AV1PP::satd_8x8_pair(psrc, predIntra, 16 * 14, satdPairs + 2);
                        int32_t satd = ((satdPairs[0] + 2) >> 2) + ((satdPairs[1] + 2) >> 2)
                            + ((satdPairs[2] + 2) >> 2) + ((satdPairs[3] + 2) >> 2);

                        int32_t deltaBits = 0;
                        if (av1_is_directional_mode(mode)) {
                            int32_t max_angle_delta = MAX_ANGLE_DELTA;
                            int32_t delta = 0;
                            deltaBits = write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
                        }

                        uint64_t cost = (RD(satd, intraModeBits[mode] + deltaBits, m_rdLambdaSqrtInt) << 4) + mode;

                        if (isNonRef && bestIntraCost != ULLONG_MAX && cost > (bestIntraCost + (bestIntraCost >> 1))) {
                            break;
                        }

                        bestIntraCost = std::min(bestIntraCost, cost);
                    }
                }
                break;
                case 3:
                {
                    for (PredMode mode = 0; mode < AV1_INTRA_MODES; mode++) {
#if 0
                        if (mode == gpuIntraMode) {
                            bestIntraCost = std::min(bestIntraCost, gpuIntraCost);
                            continue;
                        }
#endif
                        if (!test_mode[mode][bestIntraCost & 15]) continue;
                        //if (IsSmoothVhMode(mode)) continue;
                        AV1PP::predict_intra_av1(predPels.top, predPels.left, m_predIntraAll + mode * width, 14 * width,
                            txSize, haveLeft, haveTop, mode, 0, 0, 0);
                        const PixType *psrc = srcSb;
                        const PixType *predIntra = m_predIntraAll + mode * 32;
                        int32_t satd = 0;
                        for (int32_t j = 0; j < 32; j += 8, psrc += SRC_PITCH * 8, predIntra += 8 * 32 * 14) {
                            AV1PP::satd_8x8_pair(psrc + 0, predIntra + 0, 32 * 14, satdPairs);
                            AV1PP::satd_8x8_pair(psrc + 16, predIntra + 16, 32 * 14, satdPairs + 2);
                            satd += ((satdPairs[0] + 2) >> 2) + ((satdPairs[1] + 2) >> 2);
                            satd += ((satdPairs[2] + 2) >> 2) + ((satdPairs[3] + 2) >> 2);
                        }

                        int32_t deltaBits = 0;
                        if (av1_is_directional_mode(mode)) {
                            int32_t max_angle_delta = MAX_ANGLE_DELTA;
                            int32_t delta = 0;
                            deltaBits = write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
                        }
                        uint64_t cost = (RD(satd, intraModeBits[mode] + deltaBits, m_rdLambdaSqrtInt) << 4) + mode;

                        if (isNonRef && bestIntraCost != ULLONG_MAX && cost > (bestIntraCost + (bestIntraCost >> 1))) {
                            break;
                        }

                        bestIntraCost = std::min(bestIntraCost, cost);
                    }
                }
                break;
                default: assert(0);
                case 4: // no 64x64 intra
                    bestIntraCost = 0;
                    break;
                }
#if 0
                bestIntraCost = std::min(bestIntraCost, gpuIntraCost); // for mismatch in algorithms (tested modes in cpu/gpu can be different, early exist are different etc)
#endif
                bestIntraMode = bestIntraCost & 15;
                bestIntraCost >>= 4;
                bestIntraCost += isInterBits[0] * m_rdLambdaSqrtInt;
                }
#endif

                const int32_t allow_screen_content_tool_palette = m_currFrame->m_allowPalette && m_currFrame->m_isRef;
                //int32_t depth = 4 - log2w;
                if (mi->sbType >= BLOCK_8X8
                    && block_size_wide[mi->sbType] <= 32
                    && block_size_high[mi->sbType] <= 32
                    && allow_screen_content_tool_palette) {
                    // CHECK Palette Mode
                    //uint16_t palette[MAX_PALETTE];
                    //uint16_t palette_size = 0;
                    //uint32_t palette_bits = 0;
                    //uint8_t *color_map = &m_paletteMapY[depth][sby*MAX_CU_SIZE + sbx];
                    //uint32_t map_bits_est;
                    // CHECK Palette Mode
                    if (palette_size == 0) {
                        palette_size = GetColorCount(srcSb, sbw, sbw, palette, m_par->bitDepthLuma, m_rdLambda, pi, color_map, map_bits_est);
                    }

                    if (palette_size > 1 && palette_size <= MAX_PALETTE) {
                        int32_t width = sbw;
                        int32_t bestDelta = 0;
                        if (palette_bits == 0) {
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
                        }
                        //palette_bits += map_bits_est;
                        // best intra mode cost
                        int32_t filtType = get_filt_type(above, left, PLANE_TYPE_Y);
                        RdCost rd = TransformIntraYSbAv1(bsz, bestIntraMode, haveTop, haveLeft, maxTxSize, bsz <= BLOCK_16X16 ? IDTX : DCT_DCT, anz, lnz, srcSb,
                            recSb, m_pitchRecLuma, diff, coef, qcoef, coefWork, m_lumaQp, bc, m_rdLambda, miRow,
                            miCol, miColEnd, bestDelta, filtType, *m_par, m_aqparamY, NULL, pi, NULL);

                        uint32_t bits = intraModeBits[bestIntraMode];
                        if (av1_is_directional_mode(bestIntraMode)) {
                            int32_t max_angle_delta = MAX_ANGLE_DELTA;
                            int32_t delta = bestDelta;
                            bits += write_uniform_cost(2 * max_angle_delta + 1, max_angle_delta + delta);
                        }
                        if (rd.eob)
                            bits += rd.coefBits;
                        CostType cost = rd.sse + m_rdLambda * bits;

                        CostType paletteCost = m_rdLambda * (intraModeBits[DC_PRED] + palette_bits + (map_bits ? map_bits : map_bits_est)) + pi->sse; // +sse when distortion

                        if (map_bits == 0 && cost > 0.5*paletteCost && cost < 2.0*paletteCost)
                        {
                            // get better estimate
                            //palette_bits -= map_bits_est;
                            map_bits = GetPaletteTokensCost(bc, srcSb, width, width, palette, palette_size, color_map);
                            //palette_bits += map_bits;
                            paletteCost = m_rdLambda * (intraModeBits[DC_PRED] + palette_bits) + pi->sse;
                        }
                        if (cost > paletteCost) {
                            bestIntraMode = DC_PRED;
                            pi->palette_size_y = (uint8_t)palette_size;
                            pi->palette_bits = palette_bits;
                            for (int k = 0; k < palette_size; k++)
                                pi->palette_y[k] = palette[k];
                        }
                    }
                }

                const int32_t max_angle_delta = av1_is_directional_mode(bestIntraMode) ? MAX_ANGLE_DELTA : 0;
                int32_t bestAngleDelta = mi->angle_delta_y - max_angle_delta;
                CostType bestCost = COST_MAX;
                CostType lastCost = COST_MAX;
                TxSize bestTxSize = maxTxSize;
                TxType bestTxType = DCT_DCT;
                int32_t bestEob = 1025;
                int32_t filtType = get_filt_type(above, left, PLANE_TYPE_Y);

                if (m_par->maxTxDepthIntraRefine > 1) {
                    PixType *bestRec = vp9scratchpad.recY[0];
                    bestCtx = m_contexts;
                    CopyNxN(recSb, m_pitchRecLuma, bestRec, sbw);

                    // best delta decision on first iteration only
                    int delta_start = -max_angle_delta;
                    int delta_stop  = max_angle_delta;
                    int delta_step = 1;
                    if (isNonRef && max_angle_delta) {
                        delta_start = -2;
                        delta_stop  = 2;
                        delta_step  = 2;
                    }

                    const int32_t minTxSize = std::max((int32_t)TX_4X4, max_txsize_lookup[bsz] - MAX_TX_DEPTH);
                    TxType txTypeList[] = { DCT_DCT,  ADST_DCT, DCT_ADST, ADST_ADST, IDTX };
                    TxType txTypeFirst = DCT_DCT;
                    for (int32_t txSize = maxTxSize; txSize >= minTxSize; txSize--) {
                        for (auto txType : txTypeList) {
                            if (txSize == TX_32X32 && txType != DCT_DCT)
                                continue;
                            if (pi->palette_size_y && txType != DCT_DCT && txType != IDTX)
                                continue;
                            if (!pi->palette_size_y && txType == IDTX)
                                continue;

                            if (txSize != maxTxSize && txType != txTypeFirst)
                                continue;

                            for (int delta = delta_start; delta <= delta_stop; delta += delta_step) {

                                float roundFAdjT[2] = { roundFAdjYInit[0], roundFAdjYInit[1] };

                                m_contexts = origCtx;
                                RdCost rd = TransformIntraYSbAv1(bsz, bestIntraMode, haveTop, haveLeft, (TxSize)txSize, txType,
                                                                 anz, lnz, srcSb, recSb, m_pitchRecLuma, diff, coef, qcoef, coefWork,
                                                                 m_lumaQp, bc, m_rdLambda, miRow, miCol, m_tileBorders.colEnd,
                                                                 delta, filtType, *m_par, m_aqparamY, NULL, // &roundFAdjT[0],
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
                                        CopyNxN(recSb, m_pitchRecLuma, bestRec, sbw);
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
                                CoeffsType *coeff = coefBest + offset;
                                int16_t *coefOrigin = coefWorkBest + offset;
                                if (eob4x4Best[y * 16 + x])
                                    adaptDz(coefOrigin, coeff, m_aqparamY, bestTxSize, m_roundFAdjY, eob4x4Best[y * 16 + x]);
                            }
                        }
                    }
#endif

                    // propagate TxkTypes
                    SetVarTxInfo(m_currFrame->m_fenc->m_txkTypes4x4 + m_ctbAddr * 256 + (miRow & 7) * 32 + (miCol & 7) * 2, bestTxType, num4x4w);

                    m_contexts = bestCtx;
                    CopyNxN(bestRec, sbw, recSb, m_pitchRecLuma, sbw);
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
                CopyCoeffs(bestQcoef, m_coeffWorkY+16*i, num4x4<<4);
                if (pi->palette_size_y) {
                    const uint8_t *map_src = &m_paletteMapY[depth][sby*MAX_CU_SIZE + sbx];
                    uint8_t *map_dst = &m_currFrame->m_fenc->m_ColorMapY[(miRow << 3)*m_currFrame->m_fenc->m_ColorMapYPitch + (miCol << 3)];
                    //AV1PP::map_intra_palette(srcSb, map_dst,m_currFrame->m_fenc->m_ColorMapYPitch, sbw, &pi->palette_y[0], pi->palette_size_y);
                    CopyNxM(map_src, MAX_CU_SIZE, map_dst, m_currFrame->m_fenc->m_ColorMapYPitch, sbw, sbh);
                }

                (void)CheckIntraChromaNonRdAv1(i, 4 - log2w, PARTITION_NONE, recSb, m_pitchRecLuma);
                PropagatePaletteInfo(m_currFrame->m_fenc->m_Palette8x8, miRow, miCol, m_par->miPitch, sbw >> 3, sbh >> 3);
            }

            i += num4x4;
        }
        joined++;
        } while (JoinRefined32x32MI(mi64, false));

        storea_si128(m_contexts.aboveAndLeftPartition, loada_si128(savedPartitionCtx));
    }

#if ENABLE_SW_ME

    int32_t MvCost1RefLog(int16_t mvx, int16_t mvy, const AV1MV &mvp, CostType lambda)
    {
        int32_t bits = MvCost(int16_t(mvx - mvp.mvx)) + MvCost(int16_t(mvy - mvp.mvy));
        return int32_t(bits * lambda * 512 + 0.5f);
    }

    int32_t MvCost1RefLog(AV1MV mv, const AV1MV &mvp, CostType lambda)
    {
        return MvCost1RefLog(mv.mvx, mv.mvy, mvp, lambda);
    }

    static const int32_t MemSubpelExtW[4][4][4] = {
        {  // 8x8/
            { 0, 0, 0, 0 },  // 00 01 02 03
            { 0, 0, 0, 0 },  // 10 11 12 13
            { 8, 0, 8, 0 },  // 20 21 22 23
            { 0, 0, 0, 0 }   // 30 31 32 33
        },
        {  // 16x16
            { 0, 0, 0, 0 },  // 00 01 02 03
            { 0, 0, 0, 0 },  // 10 11 12 13
            { 8, 0, 8, 0 },  // 20 21 22 23
            { 0, 0, 0, 0 }   // 30 31 32 33
        },
        {  // 32x32
            { 0, 0, 0, 0 },  // 00 01 02 03
            { 0, 0, 0, 0 },  // 10 11 12 13
            { 8, 0, 8, 0 },  // 20 21 22 23
            { 0, 0, 0, 0 }   // 30 31 32 33
        },
        {  // 64x64
            { 0, 0, 0, 0 },  // 00 01 02 03
            { 0, 0, 0, 0 },  // 10 11 12 13
            { 8, 0, 8, 0 },  // 20 21 22 23
            { 0, 0, 0, 0 }   // 30 31 32 33
        }
    };

    // In Pixels (see opts for limitations)
    static const int32_t MemSubpelExtH[4][4][4] = {
        {  // 8x8
            { 0, 0, 4, 0 },  // 00 01 02 03
            { 0, 0, 0, 0 },  // 10 11 12 13
            { 0, 0, 2, 0 },  // 20 21 22 23
            { 0, 0, 0, 0 }   // 30 31 32 33
        },
        {  // 16x16
            { 0, 0, 2, 0 },  // 00 01 02 03
            { 0, 0, 0, 0 },  // 10 11 12 13
            { 2, 0, 2, 0 },  // 20 21 22 23
            { 0, 0, 0, 0 }   // 30 31 32 33
        },
        {  // 32x32
            { 0, 0, 2, 0 },  // 00 01 02 03
            { 0, 0, 0, 0 },  // 10 11 12 13
            { 2, 0, 2, 0 },  // 20 21 22 23
            { 0, 0, 0, 0 }   // 30 31 32 33
        },
        {  // 64x64
            { 0, 0, 2, 0 },  // 00 01 02 03
            { 0, 0, 0, 0 },  // 10 11 12 13
            { 2, 0, 2, 0 },  // 20 21 22 23
            { 0, 0, 0, 0 }   // 30 31 32 33
        }
    };

    template <typename PixType>
    int32_t AV1CU<PixType>::tuHadSave(const PixType* src, int32_t pitchSrc, const PixType* rec, int32_t pitchRec,
        int32_t width, int32_t height, int32_t *satd, int32_t hmemPitch)
    {
        uint32_t satdTotal = 0;
        int32_t sp = hmemPitch >> 3;
        /* assume height and width are multiple of 8 */
        assert(!(width & 0x07) && !(height & 0x07));
        /* test with optimized SATD source code */
        if (width == 8 && height == 8) {
            if (satd) {
                satd[0] = AV1PP::satd_8x8((const uint8_t  *)src, pitchSrc, (const uint8_t  *)rec, pitchRec);
                satd[0] = (satd[0]+2) >> 2;
                satdTotal += satd[0];
            } else {
                int32_t had = AV1PP::satd_8x8((const uint8_t  *)src, pitchSrc, (const uint8_t  *)rec, pitchRec);
                had = (had+2) >> 2;
                satdTotal += had;
            }
        } else {
            /* multiple 8x8 blocks - do as many pairs as possible */
            int32_t widthPair = width & ~0x0f;
            int32_t widthRem = width - widthPair;
            for (int32_t j = 0; j < height; j += 8, src += pitchSrc * 8, rec += pitchRec * 8, satd += sp) {
                int32_t i = 0, k = 0;
                for (; i < widthPair; i += 8*2, k += 2) {
                    AV1PP::satd_8x8_pair((const uint8_t  *)src + i, pitchSrc, (const uint8_t  *)rec + i, pitchRec, &satd[k]);
                    satd[k] = (satd[k]+2) >> 2;
                    satd[k+1] = (satd[k+1]+2) >> 2;
                    satdTotal += (satd[k] + satd[k+1]);
                }
                if (widthRem) {
                    satd[k] = AV1PP::satd_8x8((const uint16_t *)src+i, pitchSrc, (const uint16_t *)rec+i, pitchRec);
                    satd[k] = (satd[k]+2) >> 2;
                    satdTotal += satd[k];
                }
            }
        }

        return satdTotal;
    }

    template <typename PixType>
    int32_t AV1CU<PixType>::tuHad(const PixType* src, int32_t pitchSrc, const PixType* rec, int32_t pitchRec,
        int32_t width, int32_t height)
    {
        uint32_t satdTotal = 0;
        /* assume height and width are multiple of 8 */
        assert(!(width & 0x07) && !(height & 0x07));
        /* test with optimized SATD source code */
        if (width == 8 && height == 8) {
            int32_t had = AV1PP::satd_8x8((const uint8_t  *)src, pitchSrc, (const uint8_t  *)rec, pitchRec);
            had = (had + 2) >> 2;
            satdTotal += had;
        } else {
            /* multiple 8x8 blocks - do as many pairs as possible */
            int32_t satd[2];
            int32_t widthPair = width & ~0x0f;
            int32_t widthRem = width - widthPair;
            for (int32_t j = 0; j < height; j += 8, src += pitchSrc * 8, rec += pitchRec * 8) {
                int32_t i = 0, k = 0;
                for (; i < widthPair; i += 8 * 2, k += 2) {
                    AV1PP::satd_8x8_pair((const uint8_t  *)src + i, pitchSrc, (const uint8_t  *)rec + i, pitchRec, satd);
                    satd[0] = (satd[0] + 2) >> 2;
                    satd[1] = (satd[1] + 2) >> 2;
                    satdTotal += (satd[0] + satd[1]);
                }
                if (widthRem) {
                    satd[0] = AV1PP::satd_8x8((const uint16_t *)src + i, pitchSrc, (const uint16_t *)rec + i, pitchRec);
                    satd[0] = (satd[0] + 2) >> 2;
                    satdTotal += satd[0];
                }
            }
        }

        return satdTotal;
    }

    template <typename PixType>
    int32_t AV1CU<PixType>::tuHadUse(int32_t width, int32_t height, int32_t *satd8, int32_t hmemPitch) const
    {
        uint32_t satdTotal = 0;
        /* assume height and width are multiple of 4 */
        assert(!(width & 0x03) && !(height & 0x03));

        if ((height | width) & 0x07) {
            // assert(0);
        } else {
            int32_t s8 = hmemPitch >> 3;
            /* multiple 8x8 blocks */
            for (int32_t j = 0; j < height >> 3; j ++) {
                for (int32_t i = 0; i < width >> 3; i ++) {
                    satdTotal += satd8[j*s8+i];
                }
            }
        }
        return satdTotal;
    }

    template <typename PixType>
    inline void  AV1CU<PixType>::MemSubpelGetBufSetMv(int32_t size, AV1MV *mv, int32_t refIdxMem, PixType **predBuf, int16_t **predBufHi)
    {
        // ignore LSB in VPX to use HEVC indexes
        int32_t hPh = (mv->mvx >> 1)&0x3;
        int32_t vPh = (mv->mvy >> 1)&0x3;
        *predBuf = m_memSubpel[size][refIdxMem][hPh][vPh].predBuf;
        *predBufHi = m_memSubpel[size][refIdxMem][hPh][vPh].predBufHi;
        m_memSubpel[size][refIdxMem][hPh][vPh].done = true;
        m_memSubpel[size][refIdxMem][hPh][vPh].mv = *mv;
    }

    template <typename PixType>
    inline void  AV1CU<PixType>::MemSubpelGetBufSetMv(int32_t size, AV1MV *mv, int32_t refIdxMem, PixType **predBuf)
    {
        // ignore LSB in VPX to use HEVC indexes
        int32_t hPh = (mv->mvx >> 1) & 0x3;
        int32_t vPh = (mv->mvy >> 1) & 0x3;
        *predBuf = m_memSubpel[size][refIdxMem][hPh][vPh].predBuf;
        m_memSubpel[size][refIdxMem][hPh][vPh].done = true;
        m_memSubpel[size][refIdxMem][hPh][vPh].mv = *mv;
    }

    template <typename PixType>
    inline bool  AV1CU<PixType>::MemSubpelInRange(int32_t size, const AV1MEInfo *meInfo, int32_t refIdxMem, const AV1MV *cmv)
    {
        const int32_t hPh = 2;
        const int32_t vPh = 0;
        const int32_t MaxSubpelExtW = 8;
        const int32_t MaxSubpelExtH = 2;
        AV1MV tmv = *cmv;
        tmv.mvx -= (2 << 1);
        tmv.mvy -= 0;

        switch (size)
        {
        case 0:
            if (m_memSubpel[0][refIdxMem][hPh][vPh].done) {
                int32_t rely = meInfo->posy & 0x0f;
                int32_t relx = meInfo->posx & 0x0f;
                int32_t dMVx = (tmv.mvx - m_memSubpel[0][refIdxMem][hPh][vPh].mv.mvx) >> 3;   // same phase, always integer pixel num
                int32_t dMVy = (tmv.mvy - m_memSubpel[0][refIdxMem][hPh][vPh].mv.mvy) >> 3;

                // FIXME
                int32_t rangeLx = relx+1+(MaxSubpelExtW/2);
                int32_t rangeRx = (8+(MaxSubpelExtW/2) -(relx+meInfo->width));
                int32_t rangeLy = rely+1+(MaxSubpelExtH/2);
                int32_t rangeRy = (8+(MaxSubpelExtH/2) -(rely+meInfo->height));
                if (dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                    return true;
                }
            }
        case 1:
            if (m_memSubpel[1][refIdxMem][hPh][vPh].done) {
                int32_t rely = meInfo->posy & 0x0f;
                int32_t relx = meInfo->posx & 0x0f;
                int32_t dMVx = (tmv.mvx - m_memSubpel[1][refIdxMem][hPh][vPh].mv.mvx) >> 3;   // same phase, always integer pixel num
                int32_t dMVy = (tmv.mvy - m_memSubpel[1][refIdxMem][hPh][vPh].mv.mvy) >> 3;
                int32_t rangeLx = relx+1+(MaxSubpelExtW/2);
                int32_t rangeRx = (16+(MaxSubpelExtW/2) -(relx+meInfo->width));
                int32_t rangeLy = rely+1+(MaxSubpelExtH/2);
                int32_t rangeRy = (16+(MaxSubpelExtH/2) -(rely+meInfo->height));
                if (dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                    return true;
                }
            }
        case 2:
            if (m_memSubpel[2][refIdxMem][hPh][vPh].done) {
                int32_t rely = meInfo->posy & 0x1f;
                int32_t relx = meInfo->posx & 0x1f;
                int32_t dMVx = (tmv.mvx - m_memSubpel[2][refIdxMem][hPh][vPh].mv.mvx) >> 3;   // same phase, always integer pixel num
                int32_t dMVy = (tmv.mvy - m_memSubpel[2][refIdxMem][hPh][vPh].mv.mvy) >> 3;
                int32_t rangeLx = relx+1+(MaxSubpelExtW/2);
                int32_t rangeRx = (32+(MaxSubpelExtW/2) -(relx+meInfo->width));
                int32_t rangeLy = rely+1+(MaxSubpelExtH/2);
                int32_t rangeRy = (32+(MaxSubpelExtH/2) -(rely+meInfo->height));
                if (dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                    return true;
                }
            }
        case 3:
            if (m_memSubpel[3][refIdxMem][hPh][vPh].done) {
                int32_t rely = meInfo->posy & 0x3f;
                int32_t relx = meInfo->posx & 0x3f;
                int32_t dMVx = (tmv.mvx - m_memSubpel[3][refIdxMem][hPh][vPh].mv.mvx) >> 3;   // same phase, always integer pixel num
                int32_t dMVy = (tmv.mvy - m_memSubpel[3][refIdxMem][hPh][vPh].mv.mvy) >> 3;
                int32_t rangeLx = relx+1+(MaxSubpelExtW/2);
                int32_t rangeRx = (64+(MaxSubpelExtW/2) -(relx+meInfo->width));
                int32_t rangeLy = rely+1+(MaxSubpelExtH/2);
                int32_t rangeRy = (64+(MaxSubpelExtH/2) -(rely+meInfo->height));
                if (dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                    return true;
                }
            }
        }

        return false;
    }


    template <typename PixType>
    inline bool  AV1CU<PixType>::MemBestMV(int32_t size, int32_t refIdxMem, AV1MV *mv)
    {
        switch (size)
        {
        case 0:
        case 1:
            if (m_memBestMV[1][refIdxMem].done) {
                *mv = m_memBestMV[1][refIdxMem].mv;
                return true;
            }
        case 2:
            if (m_memBestMV[2][refIdxMem].done) {
                *mv = m_memBestMV[2][refIdxMem].mv;
                return true;
            }
        case 3:
            if (m_memBestMV[3][refIdxMem].done) {
                *mv = m_memBestMV[3][refIdxMem].mv;
                return true;
            }
        }
        return false;
    }

    template <typename PixType>
    inline void  AV1CU<PixType>::SetMemBestMV(int32_t size, const AV1MEInfo* meInfo, int32_t refIdxMem, AV1MV mv)
    {
        if (meInfo->width != meInfo->height) return;
        if (!m_memBestMV[size][refIdxMem].done) {
            m_memBestMV[size][refIdxMem].mv = mv;
            m_memBestMV[size][refIdxMem].done = true;
        }
    }

    template <typename PixType>
    inline bool  AV1CU<PixType>::MemSubpelUse(int32_t size, int32_t hPh, int32_t vPh, const AV1MEInfo* meInfo, int32_t refIdxMem, const AV1MV *emv, const PixType*& predBuf, int32_t& memPitch)
    {
        const int32_t pitch[4] = {
            (MAX_CU_SIZE >> 3) + 8,
            (MAX_CU_SIZE >> 2) + 16,
            (MAX_CU_SIZE >> 1) + 32,
            (MAX_CU_SIZE >> 0) + 32
        };

        switch (size)
        {
        case 0:
            if (m_memSubpel[0][refIdxMem][hPh][vPh].done) {
                int32_t rely = meInfo->posy & 0x07;
                int32_t relx = meInfo->posx & 0x07;
                int32_t dMVx = (emv->mvx - m_memSubpel[0][refIdxMem][hPh][vPh].mv.mvx) >> 3;   // same phase, always integer pixel num
                int32_t dMVy = (emv->mvy - m_memSubpel[0][refIdxMem][hPh][vPh].mv.mvy) >> 3;
                int32_t rangeLx = relx + 1 + (MemSubpelExtW[0][hPh][vPh] / 2);
                int32_t rangeRx = (8 + (MemSubpelExtW[0][hPh][vPh] / 2) - (relx + meInfo->width));
                int32_t rangeLy = rely + 1 + (MemSubpelExtH[0][hPh][vPh] / 2);
                int32_t rangeRy = (8 + (MemSubpelExtH[0][hPh][vPh] / 2) - (rely + meInfo->height));
                if (dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                    //memPitch = (MAX_CU_SIZE >> 3) + 8;
                    memPitch = pitch[0];
                    predBuf = &(m_memSubpel[0][refIdxMem][hPh][vPh].predBuf[(rely + dMVy + (MemSubpelExtH[0][hPh][vPh] / 2))*memPitch + (relx + dMVx + (MemSubpelExtW[0][hPh][vPh] / 2))]);
                    return true;
                }
            }
        case 1:
            if (m_memSubpel[1][refIdxMem][hPh][vPh].done) {
                int32_t rely = meInfo->posy & 0x0f;
                int32_t relx = meInfo->posx & 0x0f;
                int32_t dMVx = (emv->mvx - m_memSubpel[1][refIdxMem][hPh][vPh].mv.mvx) >> 3;   // same phase, always integer pixel num
                int32_t dMVy = (emv->mvy - m_memSubpel[1][refIdxMem][hPh][vPh].mv.mvy) >> 3;
                int32_t rangeLx = relx + 1 + (MemSubpelExtW[1][hPh][vPh] / 2);
                int32_t rangeRx = (16 + (MemSubpelExtW[1][hPh][vPh] / 2) - (relx + meInfo->width));
                int32_t rangeLy = rely + 1 + (MemSubpelExtH[1][hPh][vPh] / 2);
                int32_t rangeRy = (16 + (MemSubpelExtH[1][hPh][vPh] / 2) - (rely + meInfo->height));
                if (dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                    //memPitch = (MAX_CU_SIZE >> 2) + 8;
                    memPitch = pitch[1];
                    predBuf = &(m_memSubpel[1][refIdxMem][hPh][vPh].predBuf[(rely + dMVy + (MemSubpelExtH[1][hPh][vPh] / 2))*memPitch + (relx + dMVx + (MemSubpelExtW[1][hPh][vPh] / 2))]);
                    return true;
                }
            }
        case 2:
            if (m_memSubpel[2][refIdxMem][hPh][vPh].done) {
                int32_t rely = meInfo->posy & 0x1f;
                int32_t relx = meInfo->posx & 0x1f;
                int32_t dMVx = (emv->mvx - m_memSubpel[2][refIdxMem][hPh][vPh].mv.mvx) >> 3;   // same phase, always integer pixel num
                int32_t dMVy = (emv->mvy - m_memSubpel[2][refIdxMem][hPh][vPh].mv.mvy) >> 3;
                int32_t rangeLx = relx + 1 + (MemSubpelExtW[2][hPh][vPh] / 2);
                int32_t rangeRx = (32 + (MemSubpelExtW[2][hPh][vPh] / 2) - (relx + meInfo->width));
                int32_t rangeLy = rely + 1 + (MemSubpelExtH[2][hPh][vPh] / 2);
                int32_t rangeRy = (32 + (MemSubpelExtH[2][hPh][vPh] / 2) - (rely + meInfo->height));
                if (dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                    //memPitch = (MAX_CU_SIZE >> 1) + 8;
                    memPitch = pitch[2];
                    predBuf = &(m_memSubpel[2][refIdxMem][hPh][vPh].predBuf[(rely + dMVy + (MemSubpelExtH[2][hPh][vPh] / 2))*memPitch + (relx + dMVx + (MemSubpelExtW[2][hPh][vPh] / 2))]);
                    return true;
                }
            }
        case 3:
            if (m_memSubpel[3][refIdxMem][hPh][vPh].done) {
                int32_t rely = meInfo->posy & 0x3f;
                int32_t relx = meInfo->posx & 0x3f;
                int32_t dMVx = (emv->mvx - m_memSubpel[3][refIdxMem][hPh][vPh].mv.mvx) >> 3;   // same phase, always integer pixel num
                int32_t dMVy = (emv->mvy - m_memSubpel[3][refIdxMem][hPh][vPh].mv.mvy) >> 3;
                int32_t rangeLx = relx + 1 + (MemSubpelExtW[3][hPh][vPh] / 2);
                int32_t rangeRx = (64 + (MemSubpelExtW[3][hPh][vPh] / 2) - (relx + meInfo->width));
                int32_t rangeLy = rely + 1 + (MemSubpelExtH[3][hPh][vPh] / 2);
                int32_t rangeRy = (64 + (MemSubpelExtH[3][hPh][vPh] / 2) - (rely + meInfo->height));
                if (dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                    //memPitch = MAX_CU_SIZE + 8;
                    memPitch = pitch[3];
                    predBuf = &(m_memSubpel[3][refIdxMem][hPh][vPh].predBuf[(rely + dMVy + (MemSubpelExtH[3][hPh][vPh] / 2))*memPitch + (relx + dMVx + (MemSubpelExtW[3][hPh][vPh] / 2))]);
                    return true;
                }
            }
        }
        return false;
    }

    template <typename PixType>
    inline bool  AV1CU<PixType>::MemSubpelSave(int32_t size, int32_t hPh, int32_t vPh,
        const AV1MEInfo* meInfo, int32_t refIdxMem,
        const AV1MV *mv, PixType*& predBuf, int32_t& memPitch)
    {
        if (meInfo->width != meInfo->height) return false;

        // no hbd in this path
        if (meInfo->width == 8 && ((vPh&1) || (hPh&1))) return false;

        const int32_t pitch[4] = {
            (MAX_CU_SIZE >> 3) + 8,
            (MAX_CU_SIZE >> 2) + 16,
            (MAX_CU_SIZE >> 1) + 32,
            (MAX_CU_SIZE >> 0) + 32
        };
        if (!m_memSubpel[size][refIdxMem][hPh][vPh].done) {
            //memPitch = (MAX_CU_SIZE >> (3 - size)) + 8;
            memPitch = pitch[size];
            predBuf = m_memSubpel[size][refIdxMem][hPh][vPh].predBuf;
            m_memSubpel[size][refIdxMem][hPh][vPh].done = true;
            m_memSubpel[size][refIdxMem][hPh][vPh].mv = *mv;
            return true;
        }

        return false;
    }

    template <typename PixType>
    inline bool  AV1CU<PixType>::MemHadFirst(int32_t size, const AV1MEInfo *meInfo, int32_t refIdxMem)
    {
        //if ((meInfo->height | meInfo->width) & /*0x07*/0xf) return false;
        if ((meInfo->height | meInfo->width) & 0x7) return false;
        switch (size)
        {
        case 0:
        case 1:
            if (m_memSubHad[1][refIdxMem][0][0].count) return false;
        case 2:
            if (m_memSubHad[2][refIdxMem][0][0].count) return false;
        case 3:
            if (m_memSubHad[3][refIdxMem][0][0].count) return false;
        }

        return true;
    }


    template <typename PixType>
    inline bool  AV1CU<PixType>::MemHadUse(int32_t size, int32_t hPh, int32_t vPh, const AV1MEInfo* meInfo, int32_t refIdxMem, const AV1MV *mv, int32_t& satd, int32_t& foundSize)
    {
        if ((meInfo->height | meInfo->width) & 0x07) return false;
        if (foundSize < size) return false;
        if (foundSize > size) size = foundSize;
        switch (size)
        {
        case 0:
        case 1:
            for (int32_t testPos = 0; testPos < m_memSubHad[1][refIdxMem][hPh][vPh].count; testPos++) {
                if (m_memSubHad[1][refIdxMem][hPh][vPh].mv[testPos] == *mv) {
                    int32_t offset = ((meInfo->posy & 0x0f) >> 3)*((MAX_CU_SIZE >> 2) >> 3) + ((meInfo->posx & 0x0f) >> 3);
                    int32_t *satd8 = &(m_memSubHad[1][refIdxMem][hPh][vPh].satd8[testPos][offset]);
                    int32_t hmemPitch = MAX_CU_SIZE >> 2;
                    satd = (tuHadUse(meInfo->width, meInfo->height, satd8, hmemPitch)) >> m_par->bitDepthLumaShift;
                    foundSize = 1;
                    return true;
                }
            }
        case 2:
            for (int32_t testPos = 0; testPos < m_memSubHad[2][refIdxMem][hPh][vPh].count; testPos++) {
                if (m_memSubHad[2][refIdxMem][hPh][vPh].mv[testPos] == *mv) {
                    int32_t offset = ((meInfo->posy & 0x1f) >> 3)*((MAX_CU_SIZE >> 1) >> 3) + ((meInfo->posx & 0x1f) >> 3);
                    int32_t *satd8 = &(m_memSubHad[2][refIdxMem][hPh][vPh].satd8[testPos][offset]);
                    int32_t hmemPitch = MAX_CU_SIZE >> 1;
                    satd = (tuHadUse(meInfo->width, meInfo->height, satd8, hmemPitch)) >> m_par->bitDepthLumaShift;
                    foundSize = 2;
                    return true;
                }
            }
        case 3:
            for (int32_t testPos = 0; testPos < m_memSubHad[3][refIdxMem][hPh][vPh].count; testPos++) {
                if (m_memSubHad[3][refIdxMem][hPh][vPh].mv[testPos] == *mv) {
                    int32_t offset = ((meInfo->posy & 0x3f) >> 3)*(MAX_CU_SIZE >> 3) + ((meInfo->posx & 0x3f) >> 3);
                    int32_t *satd8 = &(m_memSubHad[3][refIdxMem][hPh][vPh].satd8[testPos][offset]);
                    int32_t hmemPitch = MAX_CU_SIZE;
                    satd = (tuHadUse(meInfo->width, meInfo->height, satd8, hmemPitch)) >> m_par->bitDepthLumaShift;
                    foundSize = 3;
                    return true;
                }
            }
        default:
            break;
        }

        return false;
    }

    template <typename PixType>
    inline bool  AV1CU<PixType>::MemHadSave(int32_t size, int32_t hPh, int32_t vPh, const AV1MEInfo* meInfo, int32_t refIdxMem, const AV1MV *mv, int32_t*& satd8, int32_t& hmemPitch)
    {
        if (meInfo->width != meInfo->height) return false;
        if (size == 0) { satd8 = NULL; return false; }
        if (m_memSubHad[size][refIdxMem][hPh][vPh].count < 4) {
            satd8 = &(m_memSubHad[size][refIdxMem][hPh][vPh].satd8[m_memSubHad[size][refIdxMem][hPh][vPh].count][0]);
            m_memSubHad[size][refIdxMem][hPh][vPh].mv[m_memSubHad[size][refIdxMem][hPh][vPh].count] = *mv;
            m_memSubHad[size][refIdxMem][hPh][vPh].count++;
            hmemPitch = MAX_CU_SIZE >> (3 - size);
            return true;
        }
        return false;
    }

    template <typename PixType>
    void AV1CU<PixType>::MemHadGetBuf(int32_t size, int32_t hPh, int32_t vPh, int32_t refIdxMem, const AV1MV *mv, int32_t **satd8)
    {
        if (size == 0) { satd8= NULL; return; }
        assert(m_memSubHad[size][refIdxMem][hPh][vPh].count < 4);
        *satd8 = &(m_memSubHad[size][refIdxMem][hPh][vPh].satd8[m_memSubHad[size][refIdxMem][hPh][vPh].count][0]);
        m_memSubHad[size][refIdxMem][hPh][vPh].mv[m_memSubHad[size][refIdxMem][hPh][vPh].count]   = *mv;
        m_memSubHad[size][refIdxMem][hPh][vPh].count++;
    }


    // absPartIdx - for minimal TU
    template <typename PixType>
    int32_t AV1CU<PixType>::MatchingMetricPu(const PixType *src, const AV1MEInfo *meInfo, const AV1MV *mv,
        const FrameData *refPic, int32_t useHadamard)
    {
        int32_t refOffset = m_ctbPelX + meInfo->posx + (mv->mvx >> 3) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 3)) * refPic->pitch_luma_pix;
        const PixType *rec = reinterpret_cast<PixType*>(refPic->y) + refOffset;
        int32_t pitchRec = refPic->pitch_luma_pix;
        PixType *predBuf = reinterpret_cast<PixType*>(m_scratchPad.interp.matchingMetric.predBuf);

        if ((mv->mvx | mv->mvy) & 7) {
            MeInterpolate(meInfo, mv, reinterpret_cast<PixType*>(refPic->y), refPic->pitch_luma_pix, predBuf, MAX_CU_SIZE);
            rec = predBuf;
            pitchRec = MAX_CU_SIZE;

            return ((useHadamard)
                ? AV1PP::satd(src, SRC_PITCH, rec, MAX_CU_SIZE, BSR(meInfo->width>>2), BSR(meInfo->height>>2))
                : AV1PP::sad_special(src, SRC_PITCH, rec, BSR(meInfo->width>>2), BSR(meInfo->height>>2))) >> m_par->bitDepthLumaShift;
        } else {
            return ((useHadamard)
                ? AV1PP::satd(src, SRC_PITCH, rec, pitchRec, BSR(meInfo->width>>2), BSR(meInfo->height>>2))
                : AV1PP::sad_general(src, SRC_PITCH, rec, pitchRec, BSR(meInfo->width>>2), BSR(meInfo->height>>2))) >> m_par->bitDepthLumaShift;
        }
    }

    // absPartIdx - for minimal TU
    template <typename PixType>
    int32_t AV1CU<PixType>::MatchingMetricPuMem(const PixType *src, const AV1MEInfo *meInfo, const AV1MV *mv,
        const FrameData *refPic, int32_t useHadamard, int32_t refIdxMem, int32_t size, int32_t& hadFoundSize)
    {
        int32_t refOffset = m_ctbPelX + meInfo->posx + (mv->mvx >> 3) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 3)) * refPic->pitch_luma_pix;
        const PixType *rec = reinterpret_cast<PixType*>(refPic->y) + refOffset;
        int32_t pitchRec = refPic->pitch_luma_pix;
        PixType *predBuf = reinterpret_cast<PixType*>(m_scratchPad.interp.matchingMetric.predBuf);

        PixType *predPtr;

        int32_t hPh = (mv->mvx >> 1) & 0x3;
        int32_t vPh = (mv->mvy >> 1) & 0x3;

        if ((mv->mvx | mv->mvy) & 7)
        {
            if (useHadamard)
            {
                int32_t satd = 0;
                if (!MemHadUse(size, hPh, vPh, meInfo, refIdxMem, mv, satd, hadFoundSize))
                {
                    if (!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, rec, pitchRec)) {
                        if (MemSubpelSave(size, hPh, vPh, meInfo, refIdxMem, mv, predPtr, pitchRec)) {
                            AV1MEInfo ext_meInfo = *meInfo;
                            AV1MV ext_mv = *mv;
                            ext_mv.mvx -= int16_t((MemSubpelExtW[size][hPh][vPh] / 2) << 3);
                            ext_mv.mvy -= int16_t((MemSubpelExtH[size][hPh][vPh] / 2) << 3);
                            ext_meInfo.width += MemSubpelExtW[size][hPh][vPh];
                            ext_meInfo.height += MemSubpelExtH[size][hPh][vPh];
                            MeInterpolateSave(&ext_meInfo, &ext_mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predPtr, pitchRec);
                            predPtr += (MemSubpelExtH[size][hPh][vPh] / 2)*pitchRec + (MemSubpelExtW[size][hPh][vPh] / 2);
                            rec = predPtr;
                        } else {
                            MeInterpolate(meInfo, mv, reinterpret_cast<PixType*>(refPic->y), refPic->pitch_luma_pix, predBuf, MAX_CU_SIZE);
                            rec = predBuf;
                            pitchRec = MAX_CU_SIZE;
                        }
                    }
                    int32_t *satd8 = NULL;
                    int32_t hmemPitch = 0;
                    if (MemHadSave(size, hPh, vPh, meInfo, refIdxMem, mv, satd8, hmemPitch)) {
                        satd = (tuHadSave(src, SRC_PITCH, rec, pitchRec, meInfo->width, meInfo->height, satd8, hmemPitch)) >> m_par->bitDepthLumaShift;
                    } else {
                        satd = (tuHad(src, SRC_PITCH, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
                    }
                }
                return satd;
            } else {
                if (!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, rec, pitchRec)) {
                    if (MemSubpelSave(size, hPh, vPh, meInfo, refIdxMem, mv, predPtr, pitchRec)) {
                        AV1MEInfo ext_meInfo = *meInfo;
                        AV1MV ext_mv = *mv;
                        ext_mv.mvx -= int16_t((MemSubpelExtW[size][hPh][vPh] / 2) << 3);
                        ext_mv.mvy -= int16_t((MemSubpelExtH[size][hPh][vPh] / 2) << 3);
                        ext_meInfo.width += MemSubpelExtW[size][hPh][vPh];
                        ext_meInfo.height += MemSubpelExtH[size][hPh][vPh];
                        MeInterpolateSave(&ext_meInfo, &ext_mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predPtr, pitchRec);
                        predPtr += (MemSubpelExtH[size][hPh][vPh] / 2)*pitchRec + (MemSubpelExtW[size][hPh][vPh] / 2);
                        rec = predPtr;
                    } else {
                        MeInterpolate(meInfo, mv, reinterpret_cast<PixType*>(refPic->y), refPic->pitch_luma_pix, predBuf, MAX_CU_SIZE);
                        rec = predBuf;
                        pitchRec = MAX_CU_SIZE;
                    }
                }

                return ((useHadamard)
                    ? AV1PP::satd(src, SRC_PITCH, rec, pitchRec, BSR(meInfo->width >> 2), BSR(meInfo->height >> 2))
                    : AV1PP::sad_general(src, SRC_PITCH, rec, pitchRec, BSR(meInfo->width >> 2), BSR(meInfo->height >> 2))) >> m_par->bitDepthLumaShift;
            }
        }
        else
        {
            if (useHadamard)
            {
                int32_t satd = 0;
                if (!MemHadUse(size, hPh, vPh, meInfo, refIdxMem, mv, satd, hadFoundSize))
                {
                    int32_t *satd8 = NULL;
                    int32_t hmemPitch = 0;
                    if (MemHadSave(size, hPh, vPh, meInfo, refIdxMem, mv, satd8, hmemPitch)) {
                        satd = (tuHadSave(src, SRC_PITCH, rec, pitchRec, meInfo->width, meInfo->height, satd8, hmemPitch)) >> m_par->bitDepthLumaShift;
                    } else {
                        satd = (tuHad(src, SRC_PITCH, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
                    }
                }
                return satd;
            } else {
                return ((useHadamard)
                    ? AV1PP::satd(src, SRC_PITCH, rec, pitchRec, BSR(meInfo->width >> 2), BSR(meInfo->height >> 2))
                    : AV1PP::sad_general(src, SRC_PITCH, rec, pitchRec, BSR(meInfo->width >> 2), BSR(meInfo->height >> 2))) >> m_par->bitDepthLumaShift;
            }
        }
    }
#endif

    template <typename PixType>
    bool AV1CU<PixType>::CheckFrameThreadingSearchRange(const AV1MEInfo *meInfo, const AV1MV *mv) const
    {
        int searchRangeY = m_par->m_meSearchRangeY;  // pixel
        int magic_wa = 1;  // tmp wa for TU7 mismatch

        if (meInfo->posy + mv->mvy / 8 + meInfo->height + 4 + magic_wa > searchRangeY) {  // +4 for interpolation
            return false;  // skip
        }

        return true;
    }


    template <typename PixType>
    RdCost SearchTxType(
        BlockSize bsz, TxSize txSize, const uint8_t *aboveNzCtx, const uint8_t *leftNzCtx,
        const PixType* src, const PixType *rec, const int16_t *diff, VarTxCoefs varTxCoefs[ADST_ADST + 2],
        int32_t coefOffset, int32_t qp, const BitCounts &bc, CostType lambda, int32_t y4,
        int32_t x4, const QuantParam &qpar, TxType *outTxType, int32_t *outCulLevel, uint32_t max_tx_ids)
    {
        const TxbBitCounts &cbc = bc.txb[ get_q_ctx(qp) ][txSize][PLANE_TYPE_Y];
        const int32_t shiftSse = (txSize == TX_32X32) ? 4 : 6;

        const int32_t diffPitch = block_size_wide[bsz];
        diff += (y4 << 2) * diffPitch + (x4 << 2);
        src += (y4 << 2) * 64 + (x4 << 2);
        rec += (y4 << 2) * 64 + (x4 << 2);

        const int32_t txbSkipCtx = GetTxbSkipCtx(bsz, txSize, 0, aboveNzCtx + x4, leftNzCtx + y4);
        const uint32_t bits0 = cbc.txbSkip[txbSkipCtx][1]; // all_zero=1
        const int32_t sse0 = AV1PP::sse_p64_p64(src, rec, txSize, txSize); // all_zero=1
        const int32_t bd = 8 + ((sizeof(PixType)-1) << 1);

#if USE_HWPAK_RESTRICT
        TxType txTypeSet[] = { IDTX, DCT_DCT };
#else
        TxType txTypeSet[] = { DCT_DCT, ADST_DCT, DCT_ADST };
#endif

        CostType bestCost = COST_MAX;
        TxType bestTxType = DCT_DCT;
        uint32_t bestBitsNz = 0;
        int32_t bestCulLevel = 0;
        int32_t bestEob = 0;
        int32_t bestSse = 0;

        coefOffset &= 1023;
        uint32_t txi = 0;
        for (auto txType : txTypeSet) {
            if (bsz == BLOCK_64X64 && txType != DCT_DCT) continue; // only DCT

            if (txi > max_tx_ids)
                break;
            int pos = (txType == IDTX) ? ADST_ADST + 1 : txType;
            int16_t *coef = varTxCoefs[pos].coef + coefOffset;
            int16_t *qcoef = varTxCoefs[pos].qcoef + coefOffset;
            int16_t *dqcoef = varTxCoefs[pos].dqcoef + coefOffset;

            AV1PP::ftransform_av1(diff, coef, diffPitch, txSize, txType);

            int32_t eob = AV1PP::quant(coef, qcoef, qpar, txSize);

            uint32_t bitsNz;
            int32_t sse, culLevel;
            if (eob) {
                AV1PP::dequant(qcoef, dqcoef, qpar, txSize, sizeof(PixType) == 1 ? 8 : 10);
                sse = int32_t(AV1PP::sse_cont(coef, dqcoef, 16 << (txSize << 1)) >> shiftSse);
                bitsNz = EstimateCoefsAv1(cbc, txbSkipCtx, eob, qcoef, &culLevel);

                bitsNz += bc.inter_tx_type_costs[3][txSize][txType];
            } else {
                bitsNz = bits0;
                sse = sse0;
                culLevel = 0;
            }

            const CostType cost = sse + lambda * bitsNz;
            if (bestCost > cost) {
                bestCost = cost;
                bestTxType = txType;
                bestEob = eob;
                bestBitsNz = bitsNz;
                bestSse = sse;
                bestCulLevel = culLevel;
            }

            txi++;
        }

        if (bestEob == 0)
            bestTxType = DCT_DCT;

        RdCost rd;
        rd.ssz = sse0;
        rd.sse = bestSse;
        rd.coefBits = bestBitsNz;
        rd.modeBits = 0;
        rd.eob = bestEob;

        *outTxType = bestTxType;
        *outCulLevel = bestCulLevel;

        return rd;
    }

    static void txfm_partition_update(uint8_t *above_ctx, uint8_t *left_ctx, TxSize tx_size, TxSize txb_size)
    {
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

    template <typename PixType>
    RdCost TransformVarTxByRdo(BlockSize bsz, TxSize txSize, uint8_t *aboveNzCtx, uint8_t *leftNzCtx,
                               const PixType* src, PixType *rec, const int16_t *diff,
                               VarTxCoefs varTxCoefs[MAX_VARTX_DEPTH + 1][ADST_ADST + 2],
                               int32_t qp, const BitCounts &bc, CostType lambda, int32_t depth,
                               int32_t y4, int32_t x4, uint8_t* aboveTxfmCtx, uint8_t* leftTxfmCtx,
                               uint16_t* vartxInfo, const QuantParam &qpar, int32_t max_vartx_depth, uint32_t max_tx_ids)
    {
        const int32_t ctx = GetCtxTxfm(aboveTxfmCtx + x4, leftTxfmCtx + y4, bsz, txSize);
        const int32_t allowSplit = txSize > TX_4X4 && depth < max_vartx_depth;
        const int32_t blockIdx = h265_scan_r2z4[y4 * 16 + x4];
        const int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);

        TxType txType;
        int32_t culLevel;
        RdCost nonSplitRd = SearchTxType(
            bsz, txSize, aboveNzCtx, leftNzCtx, src, rec, diff, varTxCoefs[depth],
            offset, qp, bc, lambda, y4, x4, qpar, &txType, &culLevel, max_tx_ids);

        if (txSize > TX_4X4 && depth < MAX_VARTX_DEPTH)
            nonSplitRd.modeBits = bc.txfm_partition_cost[ctx][0];

        const CostType nonSplitCost = nonSplitRd.sse + lambda * (nonSplitRd.coefBits + nonSplitRd.modeBits);

        CostType splitCost = COST_MAX;
        RdCost splitRd = {};
        if (allowSplit && nonSplitRd.eob > 0) {
            const TxSize subTxSize = txSize - 1;
            const int32_t bsw = 1 << subTxSize;
            const int32_t bsh = 1 << subTxSize;
            splitRd.modeBits = bc.txfm_partition_cost[ctx][1];
            splitCost = 0.0f;

            for (int32_t idx = 0; idx < 4; idx++) {
                const int32_t r = (idx >> 1) * bsh;
                const int32_t c = (idx & 0x01) * bsw;
                splitRd += TransformVarTxByRdo(
                    bsz, subTxSize, aboveNzCtx, leftNzCtx, src, rec, diff, varTxCoefs,
                    qp, bc, lambda, depth + 1, y4 + r, x4 + c, aboveTxfmCtx, leftTxfmCtx,
                    vartxInfo, qpar, max_vartx_depth, max_tx_ids);

                splitCost = (splitRd.sse + lambda * (splitRd.coefBits + splitRd.modeBits));
                if (splitCost > nonSplitCost)
                    break;
            }
        }

        if (splitCost > nonSplitCost) {
            SetCulLevel(aboveNzCtx + x4, leftNzCtx + y4, (uint8_t)culLevel, txSize);
            txfm_partition_update(aboveTxfmCtx + x4, leftTxfmCtx + y4, txSize, txSize);

            const int16_t data = txType + (txSize << 4) + ((nonSplitRd.eob & 0x3ff) << 6);
            SetVarTxInfo(vartxInfo + y4 * 16 + x4, data, tx_size_wide_unit[txSize]);

            return nonSplitRd;
        } else
            return splitRd;
    }
    template <typename PixType>
    RdCost TransformInterYSbVarTxByRdo(
        BlockSize bsz, uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src, PixType *rec,
        const int16_t *diff, int16_t *qcoef, int32_t qp, const BitCounts &bc, CostType lambda,
        uint8_t *aboveTxfmCtx, uint8_t *leftTxfmCtx, uint16_t *vartxInfo, const QuantParam &qpar,
        float *roundFAdj, VarTxCoefs varTxCoefs[MAX_VARTX_DEPTH + 1][ADST_ADST + 2], int32_t max_vartx_depth, uint32_t max_tx_ids)
    {
        const TxSize maxTxSize = std::min((uint8_t)TX_32X32, max_txsize_lookup[bsz]);
        const int32_t numBlocks = (bsz == BLOCK_64X64) ? 4 : 1;
        const int32_t initDepth = (bsz == BLOCK_64X64) ? 1 : 0;
        RdCost rd = {};

        for (int32_t idx = 0; idx < numBlocks; idx++) {
            const int32_t y4 = (idx >> 1) * 8;
            const int32_t x4 = (idx & 0x01) * 8;

            rd += TransformVarTxByRdo(
                bsz, maxTxSize, aboveNzCtx, leftNzCtx, src, rec, diff, varTxCoefs, qp, bc,
                lambda, initDepth, y4, x4, aboveTxfmCtx, leftTxfmCtx, vartxInfo, qpar, max_vartx_depth, max_tx_ids);

            InvTransformVarTx(bsz, maxTxSize, rec, varTxCoefs + initDepth, qcoef, y4, x4, vartxInfo, qpar, roundFAdj);
        }
        return rd;
    }
    template RdCost TransformInterYSbVarTxByRdo<uint8_t>(BlockSize bsz, uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const uint8_t* src, uint8_t *rec,
        const int16_t *diff, int16_t *qcoef, int32_t qp, const BitCounts &bc, CostType lambda,
        uint8_t *aboveTxfmCtx, uint8_t *leftTxfmCtx, uint16_t *vartxInfo, const QuantParam &qpar,
        float *roundFAdj, VarTxCoefs varTxCoefs[MAX_VARTX_DEPTH + 1][ADST_ADST + 2], int32_t max_vartx_depth, uint32_t max_tx_ids);

//#if ENABLE_10BIT
    template RdCost TransformInterYSbVarTxByRdo<uint16_t>(BlockSize bsz, uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const uint16_t* src, uint16_t *rec,
        const int16_t *diff, int16_t *qcoef, int32_t qp, const BitCounts &bc, CostType lambda,
        uint8_t *aboveTxfmCtx, uint8_t *leftTxfmCtx, uint16_t *vartxInfo, const QuantParam &qpar,
        float *roundFAdj, VarTxCoefs varTxCoefs[MAX_VARTX_DEPTH + 1][ADST_ADST + 2], int32_t max_vartx_depth, uint32_t max_tx_ids);
//#endif

#if GPU_VARTX
    void transpose16x16(int16_t *in, int16_t *out, int pitch)
    {
        __m256i a[16], b[16];
        for (int i = 0; i < 16; i++) a[i] = loada_si256(in + i * pitch);

        for (int i = 0; i < 8; i++) b[i + 0] = _mm256_unpacklo_epi16(a[2 * i + 0], a[2 * i + 1]);
        for (int i = 0; i < 8; i++) b[i + 8] = _mm256_unpackhi_epi16(a[2 * i + 0], a[2 * i + 1]);

        for (int i = 0; i < 4; i++) a[i + 0] = _mm256_unpacklo_epi32(b[2 * i + 0], b[2 * i + 1]);
        for (int i = 0; i < 4; i++) a[i + 4] = _mm256_unpackhi_epi32(b[2 * i + 0], b[2 * i + 1]);
        for (int i = 0; i < 4; i++) a[i + 8] = _mm256_unpacklo_epi32(b[2 * i + 8], b[2 * i + 9]);
        for (int i = 0; i < 4; i++) a[i + 12] = _mm256_unpackhi_epi32(b[2 * i + 8], b[2 * i + 9]);

        for (int i = 0; i < 2; i++) b[i + 0] = _mm256_unpacklo_epi64(a[2 * i + 0], a[2 * i + 1]);
        for (int i = 0; i < 2; i++) b[i + 2] = _mm256_unpackhi_epi64(a[2 * i + 0], a[2 * i + 1]);
        for (int i = 0; i < 2; i++) b[i + 4] = _mm256_unpacklo_epi64(a[2 * i + 4], a[2 * i + 5]);
        for (int i = 0; i < 2; i++) b[i + 6] = _mm256_unpackhi_epi64(a[2 * i + 4], a[2 * i + 5]);
        for (int i = 0; i < 2; i++) b[i + 8] = _mm256_unpacklo_epi64(a[2 * i + 8], a[2 * i + 9]);
        for (int i = 0; i < 2; i++) b[i + 10] = _mm256_unpackhi_epi64(a[2 * i + 8], a[2 * i + 9]);
        for (int i = 0; i < 2; i++) b[i + 12] = _mm256_unpacklo_epi64(a[2 * i + 12], a[2 * i + 13]);
        for (int i = 0; i < 2; i++) b[i + 14] = _mm256_unpackhi_epi64(a[2 * i + 12], a[2 * i + 13]);

        for (int i = 0; i < 8; i++) a[i + 0] = permute2x128(b[2 * i + 0], b[2 * i + 1], 0, 2);
        for (int i = 0; i < 8; i++) a[i + 8] = permute2x128(b[2 * i + 0], b[2 * i + 1], 1, 3);

        for (int i = 0; i < 16; i++) storea_si256(out + i * pitch, a[i]);
    }

    void transpose(int16_t *coefs, TxSize txSize)
    {
        if (txSize == TX_4X4) {
            __m256i r0, r1;
            r0 = loada_si256(coefs);            // 01234567 89abcdef
            r0 = shuffle32(r0, 0, 2, 1, 3);     // 01452367 89cdabef
            r0 = permute4x64(r0, 0, 2, 1, 3);   // 014589cd 2367abef
            r1 = _mm256_srli_epi32(r0, 16);     // 1*5*9*d* 3*7*b*f*
            r0 = _mm256_slli_epi32(r0, 16);
            r0 = _mm256_srli_epi32(r0, 16);     // 0*4*8*c* 2*6*a*e*
            r0 = _mm256_packus_epi32(r0, r1);   // 048c159d 26ae37bf
            storea_si256(coefs, r0);
        } else if (txSize == TX_8X8) {
            __m256i a[4], b[4];
            a[0] = loada_si256(coefs + 0 * 8);
            a[1] = loada_si256(coefs + 2 * 8);
            a[2] = loada_si256(coefs + 4 * 8);
            a[3] = loada_si256(coefs + 6 * 8);
            b[0] = _mm256_unpacklo_epi16(a[0], a[2]);
            b[1] = _mm256_unpacklo_epi16(a[1], a[3]);
            b[2] = _mm256_unpackhi_epi16(a[0], a[2]);
            b[3] = _mm256_unpackhi_epi16(a[1], a[3]);
            a[0] = _mm256_unpacklo_epi16(b[0], b[1]);
            a[1] = _mm256_unpackhi_epi16(b[0], b[1]);
            a[2] = _mm256_unpacklo_epi16(b[2], b[3]);
            a[3] = _mm256_unpackhi_epi16(b[2], b[3]);
            b[0] = permute2x128(a[0], a[1], 0, 2);
            b[1] = permute2x128(a[0], a[1], 1, 3);
            b[2] = permute2x128(a[2], a[3], 0, 2);
            b[3] = permute2x128(a[2], a[3], 1, 3);
            a[0] = _mm256_unpacklo_epi16(b[0], b[1]);
            a[1] = _mm256_unpacklo_epi16(b[2], b[3]);
            a[2] = _mm256_unpackhi_epi16(b[0], b[1]);
            a[3] = _mm256_unpackhi_epi16(b[2], b[3]);
            storea2_m128i(coefs + 0 * 8, coefs + 2 * 8, a[0]);
            storea2_m128i(coefs + 4 * 8, coefs + 6 * 8, a[1]);
            storea2_m128i(coefs + 1 * 8, coefs + 3 * 8, a[2]);
            storea2_m128i(coefs + 5 * 8, coefs + 7 * 8, a[3]);
        } else if (txSize == TX_16X16) {
            transpose16x16(coefs, coefs, 16);
        } else {
            assert(txSize == TX_32X32);
            alignas(32) int16_t tmp[32 * 16];
            transpose16x16(coefs +  0 * 32 +  0, coefs +  0 * 32 +  0, 32);
            transpose16x16(coefs + 16 * 32 + 16, coefs + 16 * 32 + 16, 32);
            transpose16x16(coefs +  0 * 32 + 16, tmp, 32);
            transpose16x16(coefs + 16 * 32 +  0, coefs +  0 * 32 + 16, 32);
            for (int i = 0; i < 16; i++) storea_si256(coefs + (16 + i) * 32, loada_si256(tmp + i * 32));
        }
    }

    template <typename PixType>
    int RecursiveVarTxRecon(int32_t bsz, TxSize txSize, PixType *rec_, const int16_t *diff_, int16_t *coeff_, int16_t *qcoeff_,
                            int16_t *dqcoeff_, int32_t y4, int32_t x4, uint16_t* vartxInfo, const QuantParam &qpar, float *roundFAdj)
    {
        uint16_t data = vartxInfo[y4 * 16 + x4];
        TxSize txSz = (data >> 2) & 0x03;
        TxType txTp = (data) & 0x03;
        if (txSz == txSize) {
            const int32_t eob = (data >> 4);
            const int32_t blockIdx = h265_scan_r2z4[y4 * 16 + x4];
            const int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
            CoeffsType *coeff = coeff_ + offset;
            CoeffsType *qcoeff = qcoeff_ + offset;
            if (eob) {
                const int32_t log2w = block_size_wide_4x4_log2[bsz];
                const int32_t recPitch = 64;
                PixType *rec = rec_ + (y4 << 2) * recPitch + (x4 << 2);
                CoeffsType* dqcoeff = dqcoeff_ + offset;
                const int32_t diffPitch = 4 << log2w;
                const int16_t *diff = diff_ + (y4 << 2) * diffPitch + (x4 << 2);

                //ALIGNED(32) int16_t tmp_coeff[32 * 32];
                //ALIGNED(32) int16_t tmp_qcoeff[32 * 32];
                //AV1PP::ftransform_av1(diff, tmp_coeff, diffPitch, txSize, txTp);

                transpose(coeff, txSize);

                int tmp_eob = AV1PP::quant(coeff, qcoeff, qpar, txSize);
                if (txSize > TX_4X4) {
                    assert(tmp_eob == eob);
                }
                //int size = 4 << txSize;
                //for (int i = 0; i < size; i++)
                //    for (int j = i + 1; j < size; j++)
                //        std::swap(qcoeff[i * size + j], qcoeff[j * size + i]);

                //for (int i = 0; i < size; i++)
                //    for (int j = 0; j < size; j++)
                //        assert(qcoeff[i * size + j] == tmp_qcoeff[i * size + j]);

                AV1PP::dequant(qcoeff, dqcoeff, qpar, txSize, sizeof(PixType) == 1 ? 8 : 10);
#ifdef ADAPTIVE_DEADZONE
                if (tmp_eob)
                    adaptDz(coeff, dqcoeff, qpar, txSize, &roundFAdj[0], tmp_eob);
#endif
                AV1PP::itransform_add_av1(dqcoeff, rec, recPitch, txSize, txTp);
                vartxInfo[y4 * 16 + x4] = txTp + (txSize << 4) + ((tmp_eob & 0x3ff) << 6);
                return tmp_eob;
            } else {
                const int32_t numCoeff = 1 << (tx_size_wide_log2[txSize] << 1);
                ZeroCoeffs(qcoeff, numCoeff);
                vartxInfo[y4 * 16 + x4] = txTp + (txSize << 4);
            }
            return eob;
        }
        else {
            const TxSize sub_txs = sub_tx_size_map[1][txSize];
            const int32_t bsw = tx_size_wide_unit[sub_txs];
            const int32_t bsh = tx_size_high_unit[sub_txs];
            int32_t eob = 0;
            eob += RecursiveVarTxRecon(bsz, sub_txs, rec_, diff_, coeff_, qcoeff_, dqcoeff_, y4, x4, vartxInfo, qpar, roundFAdj);
            eob += RecursiveVarTxRecon(bsz, sub_txs, rec_, diff_, coeff_, qcoeff_, dqcoeff_, y4, x4 + bsw, vartxInfo, qpar, roundFAdj);
            eob += RecursiveVarTxRecon(bsz, sub_txs, rec_, diff_, coeff_, qcoeff_, dqcoeff_, y4 + bsh, x4, vartxInfo, qpar, roundFAdj);
            eob += RecursiveVarTxRecon(bsz, sub_txs, rec_, diff_, coeff_, qcoeff_, dqcoeff_, y4 + bsh, x4 + bsw, vartxInfo, qpar, roundFAdj);
            //if (sub_txs == TX_4X4) {
            //    const int32_t vartx_eob = (data >> 4);
            //    assert(vartx_eob == eob);
            //}
            return eob;
        }
    }

    template <typename PixType>
    int BuildVarTxRecon(int32_t bsz, TxSize txSize, PixType *rec_, const int16_t *diff_, int16_t *coeff_,
                        int16_t *qcoeff_, int16_t *dqcoeff_, uint16_t *vartxInfo, const QuantParam &qpar,
                        float *roundFAdj)
    {
        int eob = 0;

        const int32_t idxEnd = (bsz == BLOCK_64X64) ? 4 : 1;
        for (int32_t idx = 0; idx < idxEnd; idx++) {
            int32_t y4 = (idx >> 1) * 8;
            int32_t x4 = (idx & 0x01) * 8;

            eob += RecursiveVarTxRecon(bsz, txSize, rec_, diff_, coeff_, qcoeff_, dqcoeff_, y4, x4, vartxInfo, qpar, roundFAdj);
        }

        return eob;
    }
#endif // GPU_VARTX

    template <typename PixType>
    void InvTransformVarTx(
        BlockSize bsz, TxSize txSize, PixType *rec, const VarTxCoefs (*varTxCoefs)[ADST_ADST + 2],
        int16_t *qcoefOut, int32_t y4, int32_t x4, const uint16_t *vartxInfo, const QuantParam &qpar,
        float *roundFAdj)
    {
        const int32_t rasterIdx = y4 * 16 + x4;
        const uint16_t data = vartxInfo[rasterIdx];
        const TxSize txSz = (data >> 4) & 0x03;

        if (txSz == txSize) {
            const int32_t numCoefs = (16 << (txSize << 1));
            const int32_t zigzagIdx = h265_scan_r2z4[rasterIdx];
            const int32_t offset = zigzagIdx << (LOG2_MIN_TU_SIZE << 1);
            qcoefOut += offset;

            const int32_t eob = (data >> 6);
            if (eob) {
                const TxType txType = (data & 0xf);
                const int pos = (txType == IDTX) ? ADST_ADST + 1 : txType;
                const CoeffsType *dqcoef = (*varTxCoefs)[pos].dqcoef + (offset & 1023);
                const CoeffsType *qcoef = (*varTxCoefs)[pos].qcoef + (offset & 1023);
                CopyCoeffs(qcoef, qcoefOut, numCoefs);

#ifdef ADAPTIVE_DEADZONE
                const CoeffsType *coef = (*varTxCoefs)[pos].coef + (offset & 1023);
                adaptDz(coef, dqcoef, qpar, txSize, &roundFAdj[0], eob);
#endif

                rec += (y4 << 2) * 64 + (x4 << 2);
                AV1PP::itransform_add_av1(dqcoef, rec, 64, txSize, txType);
            } else {
                ZeroCoeffs(qcoefOut, numCoefs);
            }
        } else {
            txSize -= 1;
            const int32_t ssw = 1 << txSize;
            const int32_t ssh = 1 << txSize;
            InvTransformVarTx(bsz, txSize, rec, varTxCoefs + 1, qcoefOut, y4, x4, vartxInfo, qpar, roundFAdj);
            InvTransformVarTx(bsz, txSize, rec, varTxCoefs + 1, qcoefOut, y4, x4 + ssw, vartxInfo, qpar, roundFAdj);
            InvTransformVarTx(bsz, txSize, rec, varTxCoefs + 1, qcoefOut, y4 + ssh, x4, vartxInfo, qpar, roundFAdj);
            InvTransformVarTx(bsz, txSize, rec, varTxCoefs + 1, qcoefOut, y4 + ssh, x4 + ssw, vartxInfo, qpar, roundFAdj);
        }
    }

    template <typename PixType>
    RdCost TransformInterYSb(int32_t bsz, TxSize txSize, uint8_t *aboveNzCtx, uint8_t *leftNzCtx,
                             const PixType* src_, PixType *rec_, const int16_t *diff_, int16_t *coeff_,
                             int16_t *qcoeff_, const QuantParam &qpar, const BitCounts &bc, int32_t fastCoeffCost)
    {
        const CoefBitCounts &cbc = bc.coef[PLANE_TYPE_Y][1][txSize];
        const int32_t sseShift = (sizeof(PixType)-1) << 1;
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t num4x4h = 1 << log2h;
        const int16_t *scan = vp9_default_scan[txSize];
        const int32_t recPitch = 4 << log2w;
        const int32_t diffPitch = 4 << log2w;
        const int32_t width = 4 << txSize;
        const int32_t step = 1 << txSize;
        int32_t totalEob = 0;
        uint32_t coefBits = 0;
        for (int32_t y = 0; y < num4x4h; y += step) {
            const PixType *src = src_ + (y << 2) * SRC_PITCH;
            const CoeffsType *diff = diff_ + (y << 2) * diffPitch;
            PixType *rec = rec_ + (y << 2) * recPitch;
            for (int32_t x = 0; x < num4x4w; x += step, rec += width, src += width, diff += width) {
                int32_t blockIdx = h265_scan_r2z4[y * 16 + x];
                int32_t offset = blockIdx << (LOG2_MIN_TU_SIZE << 1);
                CoeffsType *coeff  = coeff_ + offset;
                CoeffsType *qcoeff = qcoeff_ + offset;
                AV1PP::ftransform_av1(diff, coeff, diffPitch, txSize, DCT_DCT);
                int32_t eob = AV1PP::quant_dequant(coeff, qcoeff, qpar, txSize);

                int32_t dcCtx = GetDcCtx(aboveNzCtx+x, leftNzCtx+y, step);
                uint32_t bitsNz;
                if (eob) {
                    AV1PP::itransform_add_av1(coeff, rec, recPitch, txSize, DCT_DCT);
                    bitsNz = EstimateCoefs(fastCoeffCost, cbc, qcoeff, scan, eob, dcCtx, DCT_DCT);
                    small_memset1(aboveNzCtx+x, step);
                    small_memset1(leftNzCtx+y, step);
                } else {
                    bitsNz = cbc.moreCoef[0][dcCtx][0];
                    small_memset0(aboveNzCtx+x, step);
                    small_memset0(leftNzCtx+y, step);
                }

                totalEob += eob;
                coefBits += bitsNz;
            }
        }
        RdCost rd;
        rd.sse = AV1PP::sse_p64_pw(src_, rec_, log2w, log2h);
        rd.coefBits = coefBits;
        rd.modeBits = 0;
        rd.eob = totalEob;
        return rd;
    }

    template <typename PixType>
    RdCost TransformInterYSbAv1_viaCoeffDecision(BlockSize bsz, TxSize txSize, uint8_t *aboveNzCtx, uint8_t *leftNzCtx,
                             const PixType* src_, PixType *rec_, const int16_t *diff_, int16_t *coeff_,
                             int16_t *qcoeff_, const QuantParam &qpar, const BitCounts &bc, int32_t fastCoeffCost, int32_t qp)
    {
        const TxbBitCounts &cbc = bc.txb[ get_q_ctx(qp) ][txSize][PLANE_TYPE_Y];
        const int32_t sseShift = (sizeof(PixType)-1) << 1;
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
        alignas(64) CoeffsType coeffOrigin[32*32];
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
                AV1PP::ftransform_av1(diff, coeffOrigin, diffPitch, txSize, DCT_DCT);
                int32_t eob = AV1PP::quant(coeffOrigin, qcoeff, qpar, txSize);
                if (eob)
                    AV1PP::dequant(qcoeff, coeff, qpar, txSize);

                const int32_t txbSkipCtx = GetTxbSkipCtx(bsz, txSize, 0, aboveNzCtx+x, leftNzCtx+y);
                uint32_t bitsNz;
                int32_t sse;
                if (eob) {
                    int32_t culLevel;
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    bitsNz = EstimateCoefsAv1_MD(cbc, txbSkipCtx, eob, qcoeff, &culLevel);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    bitsNz = EstimateCoefsAv1(cbc, txbSkipCtx, eob, qcoeff, &culLevel);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    SetCulLevel(aboveNzCtx+x, leftNzCtx+y, (uint8_t)culLevel, txSize);
                    sse = int32_t(AV1PP::sse_cont(coeffOrigin, coeff, width*width) >> SHIFT_SSE);
                } else {
                    bitsNz = (118 * cbc.txbSkip[txbSkipCtx][1]) >> 7;
                    SetCulLevel(aboveNzCtx+x, leftNzCtx+y, 0, txSize);
                    //sse = AV1PP::sse(src, SRC_PITCH, rec, recPitch, txSize, txSize);
                    sse = int32_t(AV1PP::ssz_cont(coeffOrigin, width*width) >> SHIFT_SSE);
                }

                totalSse += sse;
                totalEob += eob;
                coefBits += bitsNz;
            }
        }
        RdCost rd;
        rd.sse = totalSse;//AV1PP::sse_p64_pw(src_, rec_, log2w, log2h);
        rd.coefBits = coefBits;
        rd.modeBits = 0;
        rd.eob = totalEob;
        return rd;
    }


    template <typename PixType>
    RdCost TransformInterUvSb(int32_t bsz, TxSize txSize, uint8_t *aboveU, uint8_t *aboveV, uint8_t *leftU, uint8_t *leftV,
                              const PixType* src_, PixType *rec_, int32_t pitchRec, const int16_t *diffU_,
                              const int16_t *diffV_, int16_t *coeffU_, int16_t *coeffV_, int16_t *qcoeffU_,
                              int16_t *qcoeffV_, const QuantParam (&qpar)[2], const BitCounts &bc, int32_t fastCoeffCost)
    {
        const CoefBitCounts &cbc = bc.coef[PLANE_TYPE_UV][1][txSize];
        const int32_t sseShift = (sizeof(PixType)-1) << 1;
        const int32_t bd = 8 + sseShift;
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t num4x4h = 1 << log2h;
        const int16_t *scan = vp9_default_scan[txSize];
        const int32_t diffPitch = 4 << log2w;
        const int32_t width = 4 << txSize;
        const int32_t step = 1 << txSize;
        int32_t totalEob = 0;
        uint32_t coefBits = 0;

        for (int32_t y = 0; y < num4x4h; y += step) {
            const PixType *src = src_ + (y << 2) * SRC_PITCH;
            const CoeffsType *diffU = diffU_ + (y << 2) * diffPitch;
            const CoeffsType *diffV = diffV_ + (y << 2) * diffPitch;
            PixType *rec = rec_ + (y << 2) * pitchRec;
            for (int32_t x = 0; x < num4x4w; x += step, rec += width << 1, src += width << 1, diffU += width, diffV += width) {
                int32_t blockIdx = h265_scan_r2z4[(y << 1) * 16 + (x << 1)];
                int32_t offset = (blockIdx << (LOG2_MIN_TU_SIZE << 1)) >> 2;
                CoeffsType *coeffU  = coeffU_ + offset;
                CoeffsType *coeffV  = coeffV_ + offset;
                CoeffsType *qcoeffU = qcoeffU_ + offset;
                CoeffsType *qcoeffV = qcoeffV_ + offset;
                AV1PP::ftransform_av1(diffU, coeffU, diffPitch, txSize, DCT_DCT);
                AV1PP::ftransform_av1(diffV, coeffV, diffPitch, txSize, DCT_DCT);
                int32_t eobU = AV1PP::quant_dequant(coeffU, qcoeffU, qpar[0], txSize);
                int32_t eobV = AV1PP::quant_dequant(coeffV, qcoeffV, qpar[1], txSize);

                int32_t dcCtxU = GetDcCtx(aboveU+x, leftU+y, step);
                int32_t dcCtxV = GetDcCtx(aboveV+x, leftV+y, step);

                uint32_t bitsNz = 0;
                if (eobU) {
                    AV1PP::itransform_av1(coeffU, coeffU, width, txSize, DCT_DCT);
                    bitsNz += EstimateCoefs(fastCoeffCost, cbc, qcoeffU, scan,eobU, dcCtxU, DCT_DCT);
                    small_memset1(aboveU+x, step);
                    small_memset1(leftU+y, step);
                } else {
                    bitsNz += cbc.moreCoef[0][dcCtxU][0]; // more_coef=0
                    small_memset0(aboveU+x, step);
                    small_memset0(leftU+y, step);
                }

                if (eobV) {
                    AV1PP::itransform_av1(coeffV, coeffV, width, txSize, DCT_DCT);
                    bitsNz += EstimateCoefs(fastCoeffCost, cbc, qcoeffV, scan,eobV, dcCtxV, DCT_DCT);
                    small_memset1(aboveV+x, step);
                    small_memset1(leftV+y, step);
                } else {
                    bitsNz += cbc.moreCoef[0][dcCtxV][0]; // more_coef=0
                    small_memset0(aboveV+x, step);
                    small_memset0(leftV+y, step);
                }

                if (eobU | eobV) {
                    CoeffsType *residU_ = (eobU ? coeffU : qcoeffU);  // if cbf == 0 qcoeffU contains zeroes
                    CoeffsType *residV_ = (eobV ? coeffV : qcoeffV);  // if cbf == 0 qcoeffV contains zeroes
                    AV1PP::adds_nv12(rec, pitchRec, rec, pitchRec, residU_, residV_, width, width);
                }

                totalEob += eobU;
                totalEob += eobV;
                coefBits += bitsNz;
            }
        }

        RdCost rd;
        rd.sse = AV1PP::sse(src_, SRC_PITCH, rec_, pitchRec, log2w + 1, log2h);
        rd.coefBits = coefBits;
        rd.modeBits = 0;
        rd.eob = totalEob;
        return rd;
    }


    template <typename PixType>
    int32_t TransformInterUvSbTxType(BlockSize bsz, TxSize txSize, TxType txType_, uint8_t *aboveU, uint8_t *aboveV,
                                    uint8_t *leftU, uint8_t *leftV, const PixType* src_, PixType *rec_,
                                    int32_t pitchRec, const int16_t *diffU_, const int16_t *diffV_, int16_t *coeffU_,
                                    int16_t *coeffV_, int16_t *qcoeffU_, int16_t *qcoeffV_, int16_t *coefWork, const AV1VideoParam &par,
                                    const BitCounts &bc, int32_t qp, const uint16_t* txk_types, const QuantParam (&qpar)[2], float **roundFAdj, CostType lambda, int32_t trySkip)
    {
        const int32_t fastCoeffCost = par.FastCoeffCost;
        const TxbBitCounts &cbc = bc.txb[ get_q_ctx(qp) ][txSize][PLANE_TYPE_UV];
        const int32_t sseShift = (sizeof(PixType)-1) << 1;
        const int32_t bd = 8 + sseShift;
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t num4x4h = 1 << log2h;
        const int32_t shiftTab[4] = { 6, 6, 6, 4 };
        const int32_t SHIFT_SSE = shiftTab[txSize];

        const int32_t diffPitch = 4 << log2w;
        const int32_t width = 4 << txSize;
        const int32_t stepX = 1 << log2w;
        const int32_t stepY = 1 << log2h;
        int32_t totalEob = 0;
        int16_t* coeffOriginU = coefWork;
        int16_t* coeffOriginV = coefWork + 32*32;

        for (int32_t y = 0; y < num4x4h; y += stepY) {
            const PixType *src = src_ + (y << 2) * SRC_PITCH;
            const CoeffsType *diffU = diffU_ + (y << 2) * diffPitch;
            const CoeffsType *diffV = diffV_ + (y << 2) * diffPitch;
            PixType *rec = rec_ + (y << 2) * pitchRec;
            for (int32_t x = 0; x < num4x4w; x += stepX, rec += width << 1, src += width << 1, diffU += width, diffV += width) {
                int32_t blockIdx = h265_scan_r2z4[(y << 1) * 16 + (x << 1)];
                int32_t offset = (blockIdx << (LOG2_MIN_TU_SIZE << 1)) >> 2;
                CoeffsType *coeffU  = coeffU_ + offset;
                CoeffsType *coeffV  = coeffV_ + offset;
                CoeffsType *qcoeffU = qcoeffU_ + offset;
                CoeffsType *qcoeffV = qcoeffV_ + offset;
                TxType txType = txk_types ? (txk_types[(y << 1) * 16 + (x << 1)] & 0xf) : txType_;
                txType = GetTxTypeAV1(PLANE_TYPE_UV, txSize, AV1_INTRA_MODES/* inter mode */, txType);

//#ifdef ADAPTIVE_DEADZONE
                AV1PP::ftransform_av1(diffU, coeffOriginU, diffPitch, txSize, txType);
                AV1PP::ftransform_av1(diffV, coeffOriginV, diffPitch, txSize, txType);

                int32_t eobU = AV1PP::quant(coeffOriginU, qcoeffU, qpar[0], txSize);
                int32_t eobV = AV1PP::quant(coeffOriginV, qcoeffV, qpar[1], txSize);
                if(eobU) AV1PP::dequant(qcoeffU, coeffU, qpar[0], txSize, sizeof(PixType) == 1 ? 8 : 10);
                if(eobV) AV1PP::dequant(qcoeffV, coeffV, qpar[1], txSize, sizeof(PixType) == 1 ? 8 : 10);
                //if(eobU) adaptDz(coeffOriginU, coeffU, qpar[0], txSize, roundFAdj[0], eobU);
                //if(eobV) adaptDz(coeffOriginV, coeffV, qpar[1], txSize, roundFAdj[1], eobV);
//#else
//                AV1PP::ftransform_fast(diffU, coeffU, diffPitch, txSize, txType);
//                AV1PP::ftransform_fast(diffV, coeffV, diffPitch, txSize, txType);
//                int32_t eobU = AV1PP::quant_dequant(coeffU, qcoeffU, qpar[0], txSize);
//                int32_t eobV = AV1PP::quant_dequant(coeffV, qcoeffV, qpar[1], txSize);
//#endif

                const int32_t txbSkipCtxU = GetTxbSkipCtx(bsz, txSize, 1, aboveU+x, leftU+y);
                const int32_t txbSkipCtxV = GetTxbSkipCtx(bsz, txSize, 1, aboveV+x, leftV+y);

                uint32_t bitsNz = 0;
                if (eobU) {
                    if (trySkip) {
                        uint32_t bitsz = cbc.txbSkip[txbSkipCtxU][1];
                        int32_t ssz = int32_t(AV1PP::ssz_cont(coeffOriginU, width*width) >> SHIFT_SSE);
                        CostType costz = (ssz + lambda * bitsz);
                        int32_t sse = int32_t(AV1PP::sse_cont(coeffOriginU, coeffU, width*width) >> SHIFT_SSE);

                        int32_t culLevel;
                        int32_t coefBits = EstimateCoefsAv1(cbc, txbSkipCtxU, eobU, qcoeffU, &culLevel);
                        CostType cost = (sse + lambda * ((coefBits * 3) >> 2));

                        if (cost < costz) {
#ifdef ADAPTIVE_DEADZONE
                            adaptDz(coeffOriginU, coeffU, qpar[0], txSize, roundFAdj[0], eobU);
#endif
                            bitsNz += coefBits;
                            AV1PP::itransform_av1(coeffU, coeffU, width, txSize, txType, sizeof(PixType) == 1 ? 8 : 10);
                            SetCulLevel(aboveU + x, leftU + y, (uint8_t)culLevel, txSize);
                        } else {
                            memset(qcoeffU, 0, sizeof(CoeffsType)*width*width);
                            bitsNz += cbc.txbSkip[txbSkipCtxU][1];
                            SetCulLevel(aboveU + x, leftU + y, 0, txSize);
                            eobU = 0;
                        }
                    } else {
                        int32_t culLevel;
                        int32_t coefBits = EstimateCoefsAv1(cbc, txbSkipCtxU, eobU, qcoeffU, &culLevel);
#ifdef ADAPTIVE_DEADZONE
                        adaptDz(coeffOriginU, coeffU, qpar[0], txSize, roundFAdj[0], eobU);
#endif
                        bitsNz += coefBits;
                        AV1PP::itransform_av1(coeffU, coeffU, width, txSize, txType, sizeof(PixType) == 1 ? 8 : 10);
                        SetCulLevel(aboveU + x, leftU + y, (uint8_t)culLevel, txSize);
                    }
                } else {
                    bitsNz += cbc.txbSkip[txbSkipCtxU][1];
                    SetCulLevel(aboveU+x, leftU+y, 0, txSize);
                }

                if (eobV) {
                    if (trySkip) {
                        uint32_t bitsz = cbc.txbSkip[txbSkipCtxV][1];
                        int32_t ssz = int32_t(AV1PP::ssz_cont(coeffOriginV, width*width) >> SHIFT_SSE);
                        CostType costz = (ssz + lambda * bitsz);
                        int32_t sse = int32_t(AV1PP::sse_cont(coeffOriginV, coeffV, width*width) >> SHIFT_SSE);

                        int32_t culLevel;
                        int32_t coefBits = EstimateCoefsAv1(cbc, txbSkipCtxV, eobV, qcoeffV, &culLevel);
                        CostType cost = (sse + lambda * ((coefBits * 3) >> 2));

                        if (cost < costz) {
#ifdef ADAPTIVE_DEADZONE
                            adaptDz(coeffOriginV, coeffV, qpar[1], txSize, roundFAdj[1], eobV);
#endif
                            bitsNz += coefBits;
                            AV1PP::itransform_av1(coeffV, coeffV, width, txSize, txType, sizeof(PixType) == 1 ? 8 : 10);
                            SetCulLevel(aboveV + x, leftV + y, (uint8_t)culLevel, txSize);
                        } else {
                            memset(qcoeffV, 0, sizeof(CoeffsType)*width*width);
                            bitsNz += cbc.txbSkip[txbSkipCtxV][1];
                            SetCulLevel(aboveV + x, leftV + y, 0, txSize);
                            eobV = 0;
                        }
                    } else {
                        int32_t culLevel;
                        int32_t coefBits = EstimateCoefsAv1(cbc, txbSkipCtxV, eobV, qcoeffV, &culLevel);
#ifdef ADAPTIVE_DEADZONE
                        adaptDz(coeffOriginV, coeffV, qpar[1], txSize, roundFAdj[1], eobV);
#endif
                        bitsNz += coefBits;
                        AV1PP::itransform_av1(coeffV, coeffV, width, txSize, txType, sizeof(PixType) == 1 ? 8 : 10);
                        SetCulLevel(aboveV + x, leftV + y, (uint8_t)culLevel, txSize);
                    }
                } else {
                    bitsNz += cbc.txbSkip[txbSkipCtxV][1];
                    SetCulLevel(aboveV+x, leftV+y, 0, txSize);
                }

                if (eobU | eobV) {
                    CoeffsType *residU_ = (eobU ? coeffU : qcoeffU);  // if cbf == 0 qcoeffU contains zeroes
                    CoeffsType *residV_ = (eobV ? coeffV : qcoeffV);  // if cbf == 0 qcoeffV contains zeroes
                    AV1PP::adds_nv12(rec, pitchRec, rec, pitchRec, residU_, residV_, width, width);
                }

                totalEob += eobU;
                totalEob += eobV;
            }
        }

        return totalEob;
    }
    template int32_t TransformInterUvSbTxType<uint8_t>(BlockSize bsz, TxSize txSize, TxType txType_, uint8_t *aboveU, uint8_t *aboveV,
        uint8_t *leftU, uint8_t *leftV, const uint8_t* src_, uint8_t *rec_,
        int32_t pitchRec, const int16_t *diffU_, const int16_t *diffV_, int16_t *coeffU_,
        int16_t *coeffV_, int16_t *qcoeffU_, int16_t *qcoeffV_, int16_t *coefWork, const AV1VideoParam &par,
        const BitCounts &bc, int32_t qp, const uint16_t* txk_types, const QuantParam(&qpar)[2], float **roundFAdj, CostType lambda, int32_t trySkip);
//#if ENABLE_10BIT
    template int32_t TransformInterUvSbTxType<uint16_t>(
        BlockSize bsz, TxSize txSize, TxType txType_, uint8_t *aboveU, uint8_t *aboveV,
        uint8_t *leftU, uint8_t *leftV, const uint16_t* src_, uint16_t *rec_,
        int32_t pitchRec, const int16_t *diffU_, const int16_t *diffV_, int16_t *coeffU_,
        int16_t *coeffV_, int16_t *qcoeffU_, int16_t *qcoeffV_, int16_t *coefWork, const AV1VideoParam &par,
        const BitCounts &bc, int32_t qp, const uint16_t* txk_types, const QuantParam(&qpar)[2], float **roundFAdj, CostType lambda, int32_t trySkip);
//#endif

    bool SatdSearch(const AV1MVPair (&nonZeroMvp)[4][6+4+4], const AV1MVPair mvs, int32_t *depth, int32_t *slot) {
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
            return _mm_cvtsi128_si32(sum) + _mm_cvtsi128_si32(_mm_bsrli_si128(sum,4));
        } else if (depth == 1) {
            __m128i sum = _mm_add_epi32(loada_si128(satds), loada_si128(satds + pitch));
            sum = _mm_add_epi32(sum, loada_si128(satds + 2 * pitch));
            sum = _mm_add_epi32(sum, loada_si128(satds + 3 * pitch));
            sum = _mm_add_epi32(sum, _mm_bsrli_si128(sum, 8));
            return _mm_cvtsi128_si32(sum) + _mm_cvtsi128_si32(_mm_bsrli_si128(sum, 4));
        } else {
            assert(0);
            return 0;
        }
    }

    template <int32_t depth, class PixType> int32_t SadSave(const PixType *src1, const PixType *src2, int32_t *satds)
    {
        if (depth == 3) {
            return AV1PP::sad_special(src1, 64, src2, 1, 1);
        }
        else if (depth == 2) {
            return AV1PP::sad_store8x8(src1, src2, satds, 2);
        }
        else if (depth == 1) {
            return AV1PP::sad_store8x8(src1, src2, satds, 3);
        }
        else if (depth == 0) {
            return AV1PP::sad_store8x8(src1, src2, satds, 4);
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
        //       int32_t satd = AV1PP::satd_8x8(src1, src2);
        //       return (satd + 2) >> 2;
        //   } else {
        //       int32_t satdTotal = 0;
        //       for (int32_t y = 0; y < sbw; y += 8, src1 += 8*64, src2 += 8*64, satds += 8) {
        //           for (int32_t x = 0, k = 0; x < sbw; x += 16, k += 2) {
        //               AV1PP::satd_8x8_pair(src1 + x, src2 + x, satds + k);
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

            return (AV1PP::satd_8x8(src1, src2) + 2) >> 2;
#else
            return AV1PP::sad_special(src1, 64, src2, 1, 1);
#endif
        } else if (depth == 2) {
#ifndef USE_SAD_ONLY
            AV1PP::satd_8x8_pair(src1, src2, satds);
            satds[0] = (satds[0] + 2) >> 2;
            satds[1] = (satds[1] + 2) >> 2;
            satdTtotal = satds[0] + satds[1];
            src1 += 8 * 64; src2 += 8 * 64; satds += 8;
            AV1PP::satd_8x8_pair(src1, src2, satds);
            satds[0] = (satds[0] + 2) >> 2;
            satds[1] = (satds[1] + 2) >> 2;
            return satdTtotal + satds[0] + satds[1];
#else
            return AV1PP::sad_store8x8(src1, src2, satds, 2);
#endif
        } else if (depth == 1) {
#ifndef USE_SAD_ONLY
            __m128i two = _mm_set1_epi32(2), satd4, sum;
            AV1PP::satd_8x8_pair(src1 + 0,  src2 + 0,  satds + 0);
            AV1PP::satd_8x8_pair(src1 + 16, src2 + 16, satds + 2);
            satd4 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds), two), 2);
            storea_si128(satds, satd4);
            sum = satd4;
            src1 += 8 * 64; src2 += 8 * 64; satds += 8;
            AV1PP::satd_8x8_pair(src1 + 0,  src2 + 0,  satds + 0);
            AV1PP::satd_8x8_pair(src1 + 16, src2 + 16, satds + 2);
            satd4 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds), two), 2);
            storea_si128(satds, satd4);
            sum = _mm_add_epi32(sum, satd4);
            src1 += 8 * 64; src2 += 8 * 64; satds += 8;
            AV1PP::satd_8x8_pair(src1 + 0,  src2 + 0,  satds + 0);
            AV1PP::satd_8x8_pair(src1 + 16, src2 + 16, satds + 2);
            satd4 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds), two), 2);
            storea_si128(satds, satd4);
            sum = _mm_add_epi32(sum, satd4);
            src1 += 8 * 64; src2 += 8 * 64; satds += 8;
            AV1PP::satd_8x8_pair(src1 + 0,  src2 + 0,  satds + 0);
            AV1PP::satd_8x8_pair(src1 + 16, src2 + 16, satds + 2);
            satd4 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds), two), 2);
            storea_si128(satds, satd4);
            sum = _mm_add_epi32(sum, satd4);
            sum = _mm_add_epi32(sum, _mm_bsrli_si128(sum, 8));
            return _mm_cvtsi128_si32(sum) + _mm_cvtsi128_si32(_mm_bsrli_si128(sum, 4));
#else
            return AV1PP::sad_store8x8(src1, src2, satds, 3);
#endif
        } else if (depth == 0) {
#ifndef USE_SAD_ONLY
            __m128i sum = _mm_setzero_si128(), two = _mm_set1_epi32(2), satd1, satd2;
            for (int32_t y = 0; y < 64; y += 8, src1 += 8 * 64, src2 += 8 * 64, satds += 8) {
                AV1PP::satd_8x8_pair(src1 + 0,  src2 + 0,  satds + 0);
                AV1PP::satd_8x8_pair(src1 + 16, src2 + 16, satds + 2);
                AV1PP::satd_8x8_pair(src1 + 32, src2 + 32, satds + 4);
                AV1PP::satd_8x8_pair(src1 + 48, src2 + 48, satds + 6);
                satd1 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds + 0), two), 2);
                satd2 = _mm_srli_epi32(_mm_add_epi32(loada_si128(satds + 4), two), 2);
                storea_si128(satds + 0, satd1);
                storea_si128(satds + 4, satd2);
                sum = _mm_add_epi32(sum, satd1);
                sum = _mm_add_epi32(sum, satd2);
            }
            sum = _mm_add_epi32(sum, _mm_bsrli_si128(sum, 8));
            return _mm_cvtsi128_si32(sum) + _mm_cvtsi128_si32(_mm_bsrli_si128(sum, 4));
#else
            return AV1PP::sad_store8x8(src1, src2, satds, 4);
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


    int32_t GetDcCtx(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx, int32_t step) {
        int32_t above = 0;
        int32_t left = 0;
        for (int32_t j = 0; j < step; j++) {
            above |= aboveNonzeroCtx[j];
            left  |= leftNonzeroCtx[j];
        }
        return above + left;
    }

    int32_t GetDcCtx(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx, int32_t stepX, int32_t stepY) {
        int32_t above = 0;
        int32_t left = 0;
        for (int32_t j = 0; j < stepX; j++)
            above |= aboveNonzeroCtx[j];
        for (int32_t j = 0; j < stepY; j++)
            left |= leftNonzeroCtx[j];
        return above + left;
    }

    template <> int32_t GetDcCtx<1>(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx) {
        return *aboveNonzeroCtx + *leftNonzeroCtx;
    }
    template <> int32_t GetDcCtx<2>(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx) {
        return (aboveNonzeroCtx[0] | aboveNonzeroCtx[1]) + (leftNonzeroCtx[0] | leftNonzeroCtx[1]);
    }
    template <> int32_t GetDcCtx<4>(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx) {
        int32_t a = _mm_cvtsi128_si32(_mm_cmpeq_epi32(loadu_si32(aboveNonzeroCtx), _mm_setzero_si128()));
        int32_t l = _mm_cvtsi128_si32(_mm_cmpeq_epi32(loadu_si32(leftNonzeroCtx), _mm_setzero_si128()));
        return 2 + a + l;
    }
    template <> int32_t GetDcCtx<8>(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx) {
        int32_t a = _mm_cvtsi128_si32(_mm_cmpeq_epi64(loadl_epi64(aboveNonzeroCtx), _mm_setzero_si128()));
        int32_t l = _mm_cvtsi128_si32(_mm_cmpeq_epi64(loadl_epi64(leftNonzeroCtx), _mm_setzero_si128()));
        return 2 + a + l;
    }
    template <> int32_t GetDcCtx<16>(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx) {
        // empty, needed for template compilation only
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

    int32_t GetTxbSkipCtx(BlockSize planeBsize, TxSize txSize, int32_t plane, const uint8_t *aboveCtx, const uint8_t *leftCtx)
    {
        if (plane == 0) {
            if (planeBsize == txsize_to_bsize[txSize]) {
                return 0;
            } else {
                const int32_t txbSize = 1 << txSize;

                int32_t top = 0;
                int32_t left = 0;
#ifdef __INTEL_COMPILER
                #pragma novector
                #pragma nounroll
#endif // __INTEL_COMPILER
                for (int32_t k = 0; k < txbSize; k++) {
                    top |= aboveCtx[k];
                    left |= leftCtx[k];
                }

                top &= COEFF_CONTEXT_MASK;
                left &= COEFF_CONTEXT_MASK;

                const int32_t max = std::min(top | left, 4);
                const int32_t min = std::min(std::min(top, left), 4);
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

    inline RefIdx GetRefIdxComb(RefIdx refIdx0, RefIdx refIdx1, int32_t compVarRef1) {
        return refIdx1 == NONE_FRAME ? refIdx0 : COMP_VAR0 + (refIdx1 == compVarRef1);
    }
    inline RefIdx GetRefIdxComb(const RefIdx refIdxs[2], int32_t compVarRef1) {
        return GetRefIdxComb(refIdxs[0], refIdxs[1], compVarRef1);
    }

    template <typename PixType>
    bool AV1CU<PixType>::IsTuSplitIntra() const
    {
        int32_t tuSplitIntra = (m_par->tuSplitIntra == 1 ||
            m_par->tuSplitIntra == 3 && (m_currFrame->m_picCodeType == MFX_FRAMETYPE_I ||
            (m_par->BiPyramidLayers > 1 && m_currFrame->m_pyramidLayer == 0)));

        return tuSplitIntra;
    }

    template <typename PixType>
    ModeInfo *AV1CU<PixType>::StoredCuData(int32_t storageIndex) const
    {
        assert(storageIndex < 4);
        return m_dataStored + (storageIndex << 6);
    }

    template <typename PixType>
    void AV1CU<PixType>::SaveFinalCuDecision(int32_t absPartIdx, int32_t depth, PartitionType partition, int32_t storageIndex, int32_t chroma)
    {
        assert((uint32_t)storageIndex < sizeof(m_costStored) / sizeof(m_costStored[0]));
        assert((uint32_t)storageIndex < sizeof(m_recStoredY) / sizeof(m_recStoredY[0]));
        assert((uint32_t)storageIndex < sizeof(m_coeffStoredY) / sizeof(m_coeffStoredY[0]));
        assert((uint32_t)storageIndex < sizeof(m_recStoredC) / sizeof(m_recStoredC[0]));
        assert((uint32_t)storageIndex < sizeof(m_coeffStoredU) / sizeof(m_coeffStoredU[0]));
        assert((uint32_t)storageIndex < sizeof(m_coeffStoredV) / sizeof(m_coeffStoredV[0]));

        if (m_costStored[storageIndex] > m_costCurr) {
            int32_t bsz = GetSbType(depth, partition);
            int32_t x4 = (av1_scan_z2r4[absPartIdx] & 15);
            int32_t y4 = (av1_scan_z2r4[absPartIdx] >> 4);
            int32_t widthY = block_size_wide[bsz];
            int32_t heightY = block_size_high[bsz];
            int32_t num4x4 = num_4x4_blocks_lookup[bsz];
            int32_t widthC = widthY >> m_par->chromaShiftW;
            int32_t heightC = heightY >> m_par->chromaShiftH;
            int32_t coeffOffsetY = absPartIdx << 4;
            int32_t coeffOffsetC = absPartIdx << (4 - m_par->chromaShift);
            int32_t numCoeffY = num4x4 << 4;
            int32_t numCoeffC = num4x4 << (4 - m_par->chromaShift);

            int32_t pitchRecY = m_pitchRecLuma;
            int32_t pitchRecC = m_pitchRecChroma;
            PixType *recY = m_yRec  + GetLumaOffset(m_par, absPartIdx, pitchRecY);
            PixType *recC = m_uvRec + GetChromaOffset(m_par, absPartIdx, pitchRecC);
            const ModeInfo *srcMi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;
            ModeInfo *dstMi = StoredCuData(storageIndex) + (x4 >> 1) + (y4 >> 1) * (MAX_CU_SIZE >> 3);

            const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
            const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);
            const PaletteInfo *srcPi = m_currFrame->m_fenc->m_Palette8x8 + miCol + miRow * m_par->miPitch;
            PaletteInfo *dstPi = &m_paletteStoredY[storageIndex][(x4 >> 1) + (y4 >> 1) * (MAX_CU_SIZE >> 3)];

            m_contextsStored[storageIndex] = m_contexts;
            m_costStored[storageIndex] = m_costCurr;                                            // full RD cost
            CopyNxM(recY, pitchRecY, m_recStoredY[storageIndex], MAX_CU_SIZE, widthY, heightY);
            CopyCuData(srcMi, m_par->miPitch, dstMi, (MAX_CU_SIZE >> 3), widthY >> 3, heightY >> 3);
            CopyPaletteInfo(srcPi, m_par->miPitch, dstPi, (MAX_CU_SIZE >> 3), widthY >> 3, heightY >> 3);
            if (partition == PARTITION_VERT) {
                CopyCoeffs(m_coeffWorkY + coeffOffsetY, m_coeffStoredY[storageIndex], num4x4 * 8);
                CopyCoeffs(m_coeffWorkY + coeffOffsetY + numCoeffY, m_coeffStoredY[storageIndex] + numCoeffY, numCoeffY >> 1);
            } else {
                CopyCoeffs(m_coeffWorkY + coeffOffsetY, m_coeffStoredY[storageIndex], numCoeffY);
            }

            if (chroma) {
                CopyNxM(recC, pitchRecC, m_recStoredC[storageIndex], MAX_CU_SIZE << m_par->chromaShiftWInv, widthC * 2, heightC);
                if (partition == PARTITION_VERT) {
                    CopyCoeffs(m_coeffWorkU + coeffOffsetC, m_coeffStoredU[storageIndex], numCoeffC >> 1);
                    CopyCoeffs(m_coeffWorkV + coeffOffsetC, m_coeffStoredV[storageIndex], numCoeffC >> 1);
                    CopyCoeffs(m_coeffWorkU + coeffOffsetC + numCoeffC, m_coeffStoredU[storageIndex] + numCoeffC, numCoeffC >> 1);
                    CopyCoeffs(m_coeffWorkV + coeffOffsetC + numCoeffC, m_coeffStoredV[storageIndex] + numCoeffC, numCoeffC >> 1);
                } else {
                    CopyCoeffs(m_coeffWorkU + coeffOffsetC, m_coeffStoredU[storageIndex], numCoeffC);
                    CopyCoeffs(m_coeffWorkV + coeffOffsetC, m_coeffStoredV[storageIndex], numCoeffC);
                }
            }
        }
    }

    template <typename PixType>
    void AV1CU<PixType>::LoadFinalCuDecision(int32_t absPartIdx, int32_t depth, PartitionType partition, int32_t storageIndex, int32_t chroma)
    {
        assert((uint32_t)storageIndex < sizeof(m_costStored) / sizeof(m_costStored[0]));
        assert((uint32_t)storageIndex < sizeof(m_recStoredY) / sizeof(m_recStoredY[0]));
        assert((uint32_t)storageIndex < sizeof(m_coeffStoredY) / sizeof(m_coeffStoredY[0]));
        assert((uint32_t)storageIndex < sizeof(m_recStoredC) / sizeof(m_recStoredC[0]));
        assert((uint32_t)storageIndex < sizeof(m_coeffStoredU) / sizeof(m_coeffStoredU[0]));
        assert((uint32_t)storageIndex < sizeof(m_coeffStoredV) / sizeof(m_coeffStoredV[0]));

        if (m_costStored[storageIndex] <= m_costCurr) {
            int32_t bsz = 3 * (4 - depth) - partition;
            int32_t x4 = (av1_scan_z2r4[absPartIdx] & 15);
            int32_t y4 = (av1_scan_z2r4[absPartIdx] >> 4);
            int32_t widthY = block_size_wide[bsz];
            int32_t heightY = block_size_high[bsz];
            int32_t num4x4 = num_4x4_blocks_lookup[bsz];
            int32_t widthC = widthY >> m_par->chromaShiftW;
            int32_t heightC = heightY >> m_par->chromaShiftH;
            int32_t coeffOffsetY = absPartIdx << 4;
            int32_t coeffOffsetC = absPartIdx << (4 - m_par->chromaShift);
            int32_t numCoeffY = num4x4 << 4;
            int32_t numCoeffC = num4x4 << (4 - m_par->chromaShift);

            int32_t pitchRecY = m_pitchRecLuma;
            int32_t pitchRecC = m_pitchRecChroma;
            PixType *recY = m_yRec  + GetLumaOffset(m_par, absPartIdx, pitchRecY);
            PixType *recC = m_uvRec + GetChromaOffset(m_par, absPartIdx, pitchRecC);
            const ModeInfo *srcMi = StoredCuData(storageIndex) + (x4 >> 1) + (y4 >> 1) * (MAX_CU_SIZE >> 3);
            ModeInfo *dstMi = m_data + (x4 >> 1) + (y4 >> 1) * m_par->miPitch;

            const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
            const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);
            const PaletteInfo *srcPi = &m_paletteStoredY[storageIndex][(x4 >> 1) + (y4 >> 1) * (MAX_CU_SIZE >> 3)];
            PaletteInfo *dstPi = m_currFrame->m_fenc->m_Palette8x8 + miCol + miRow * m_par->miPitch;

            m_contexts = m_contextsStored[storageIndex];
            m_costCurr = m_costStored[storageIndex];                                       // full RD cost
            CopyNxM(m_recStoredY[storageIndex], MAX_CU_SIZE, recY, pitchRecY, widthY, heightY);
            CopyCuData(srcMi, (MAX_CU_SIZE >> 3), dstMi, m_par->miPitch, widthY >> 3, heightY >> 3);
            CopyPaletteInfo(srcPi, (MAX_CU_SIZE >> 3), dstPi, m_par->miPitch, widthY >> 3, heightY >> 3);
            if (partition == PARTITION_VERT) {
                CopyCoeffs(m_coeffStoredY[storageIndex], m_coeffWorkY + coeffOffsetY, numCoeffY >> 1);  // block of Y coefficients
                CopyCoeffs(m_coeffStoredY[storageIndex] + numCoeffY, m_coeffWorkY + coeffOffsetY + numCoeffY, numCoeffY >> 1);  // block of Y coefficients
            } else {
                CopyCoeffs(m_coeffStoredY[storageIndex], m_coeffWorkY + coeffOffsetY, numCoeffY);  // block of Y coefficients
            }

            if (chroma) {
                CopyNxM(m_recStoredC[storageIndex], MAX_CU_SIZE << m_par->chromaShiftWInv, recC, pitchRecC, widthC * 2, heightC);
                if (partition == PARTITION_VERT) {
                    CopyCoeffs(m_coeffStoredU[storageIndex], m_coeffWorkU + coeffOffsetC, numCoeffC >> 1);  // block of U coefficients
                    CopyCoeffs(m_coeffStoredV[storageIndex], m_coeffWorkV + coeffOffsetC, numCoeffC >> 1);  // block of V coefficients
                    CopyCoeffs(m_coeffStoredU[storageIndex] + numCoeffC, m_coeffWorkU + coeffOffsetC + numCoeffC, numCoeffC >> 1);  // block of U coefficients
                    CopyCoeffs(m_coeffStoredV[storageIndex] + numCoeffC, m_coeffWorkV + coeffOffsetC + numCoeffC, numCoeffC >> 1);  // block of V coefficients
                } else {
                    CopyCoeffs(m_coeffStoredU[storageIndex], m_coeffWorkU + coeffOffsetC, numCoeffC);  // block of U coefficients
                    CopyCoeffs(m_coeffStoredV[storageIndex], m_coeffWorkV + coeffOffsetC, numCoeffC);  // block of V coefficients
                }
            }
        }
    }


    template <typename PixType>
    const ModeInfo *AV1CU<PixType>::GetBestCuDecision(int32_t absPartIdx, int32_t depth) const
    {
        assert((uint32_t)depth < sizeof(m_costStored) / sizeof(m_costStored[0]));
        return (m_costStored[depth] <= m_costCurr ? StoredCuData(depth) : m_data) + absPartIdx;
    }

    //template class AV1CU<uint8_t>;
    //template class AV1CU<uint16_t>;
    template void AV1CU<uint8_t>::RefineDecisionAV1();
    template void AV1CU<uint8_t>::RefineDecisionIAV1();
    template void AV1CU<uint8_t>::InitCu(const AV1VideoParam &_par, ModeInfo *_data, ModeInfo *_dataTemp, int32_t ctbRow, int32_t ctbCol, const Frame &frame, CoeffsType *m_coeffWork, uint8_t* lcuRound);
    template void AV1CU<uint8_t>::ModeDecision(int, int);

#if ENABLE_SW_ME
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
    template <typename PixType> void AV1CU<PixType>::CalculateZeroMvPredAndSatd()
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
            AV1PP::interp_av1(refColoc, m_pitchRecLuma, pred, 0, 0, MAX_CU_SIZE, LOG2_MAX_CU_SIZE - 2, DEF_FILTER_DUAL);
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
                PixType *pred = vp9scratchpad.interpBufs[refIdx][0][ZEROMV_];
                AV1PP::average(pred0, pred1, pred, MAX_CU_SIZE, LOG2_MAX_CU_SIZE - 2); // average existing predictions
                m_interp[refIdx][0][ZEROMV_] = pred;
                m_interp[refIdx][1][ZEROMV_] = pred;
                m_interp[refIdx][2][ZEROMV_] = pred;
                m_interp[refIdx][3][ZEROMV_] = pred;

                CalculateAndAccumulateSatds(m_ySrc, pred, m_zeroMvpSatd[refIdx], m_par->hadamardMe >= 2);
            }
        }
    }

    template <typename PixType>
    void AV1CU<PixType>::InitCu_SwPath(const AV1VideoParam &par, ModeInfo *_data, ModeInfo *_dataTemp,
        int32_t ctbRow, int32_t ctbCol, const Frame &frame, CoeffsType *m_coeffWork)
    {
        m_par = &par;
        m_sliceQpY = frame.m_sliceQpY;
        m_lcuQps = &frame.m_lcuQps[0];

        m_data = _data;
        m_dataStored = _dataTemp;
        m_ctbAddr = ctbRow * m_par->PicWidthInCtbs + ctbCol;
        m_ctbPelX = ctbCol * m_par->MaxCUSize;
        m_ctbPelY = ctbRow * m_par->MaxCUSize;

        m_currFrame = &frame;

        FrameData* recon = m_currFrame->m_recon;
        m_pitchRecLuma = recon->pitch_luma_pix;
        m_pitchRecChroma = recon->pitch_chroma_pix;
        m_yRec = reinterpret_cast<PixType*>(recon->y) + m_ctbPelX + m_ctbPelY * m_pitchRecLuma;
        m_uvRec = reinterpret_cast<PixType*>(recon->uv) + m_ctbPelX + (m_ctbPelY >> 1) * m_pitchRecChroma;

        FrameData* origin = m_currFrame->m_origin;
        PixType *originY = reinterpret_cast<PixType*>(origin->y) + m_ctbPelX + m_ctbPelY * origin->pitch_luma_pix;
        PixType *originUv = reinterpret_cast<PixType*>(origin->uv) + m_ctbPelX + (m_ctbPelY >> 1) * origin->pitch_chroma_pix;
        CopyNxN(originY, origin->pitch_luma_pix, m_ySrc, 64);
        CopyNxM(originUv, origin->pitch_chroma_pix, m_uvSrc, 64, 32);

        m_coeffWorkY = m_coeffWork;
        m_coeffWorkU = m_coeffWorkY + m_par->MaxCUSize * m_par->MaxCUSize;
        m_coeffWorkV = m_coeffWorkU + (m_par->MaxCUSize * m_par->MaxCUSize >> m_par->chromaShift);

        // limits to clip MV allover the CU
        const int32_t MvShift = 3;
        const int32_t offset = 8;
        HorMax = std::min((m_par->Width + offset - m_ctbPelX - 1) << MvShift, int32_t(std::numeric_limits<int16_t>::max()));
        HorMin = std::max((-m_par->MaxCUSize - offset - m_ctbPelX + 1) << MvShift, int32_t(std::numeric_limits<int16_t>::min()));
        VerMax = std::min((m_par->Height + offset - m_ctbPelY - 1) << MvShift, int32_t(std::numeric_limits<int16_t>::max()));
        VerMin = std::max((-m_par->MaxCUSize - offset - m_ctbPelY + 1) << MvShift, int32_t(std::numeric_limits<int16_t>::min()));

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
            m_availForPred.colocated = m_currFrame->m_prevFrame->m_modeInfo + (ctbCol << 3) + (ctbRow << 3) * m_par->miPitch;
        if (ctbColWithinTile)
            m_availForPred.left = m_data - 256;
        if (ctbRowWithinTile)
            m_availForPred.above = m_data - (m_par->PicWidthInCtbs << 8);
        if (m_availForPred.left && m_availForPred.above)
            m_availForPred.aboveLeft = m_availForPred.above - 256;

        int32_t tile_index = GetTileIndex(par.tileParam, ctbRow, ctbCol);
        const TileContexts &tileCtx = frame.m_tileContexts[tile_index];
        As16B(m_contexts.aboveNonzero[0]) = As16B(tileCtx.aboveNonzero[0] + (ctbColWithinTile << 4));
        As8B(m_contexts.aboveNonzero[1]) = As8B(tileCtx.aboveNonzero[1] + (ctbColWithinTile << 3));
        As8B(m_contexts.aboveNonzero[2]) = As8B(tileCtx.aboveNonzero[2] + (ctbColWithinTile << 3));
        As16B(m_contexts.leftNonzero[0]) = As16B(tileCtx.leftNonzero[0] + (ctbRowWithinTile << 4));
        As8B(m_contexts.leftNonzero[1]) = As8B(tileCtx.leftNonzero[1] + (ctbRowWithinTile << 3));
        As8B(m_contexts.leftNonzero[2]) = As8B(tileCtx.leftNonzero[2] + (ctbRowWithinTile << 3));
        As8B(m_contexts.abovePartition) = As8B(tileCtx.abovePartition + (ctbColWithinTile << 3));
        As8B(m_contexts.leftPartition) = As8B(tileCtx.leftPartition + (ctbRowWithinTile << 3));
        As16B(m_contexts.aboveTxfm) = As16B(tileCtx.aboveTxfm + (ctbColWithinTile << 4));
        As16B(m_contexts.leftTxfm) = As16B(tileCtx.leftTxfm + (ctbRowWithinTile << 4));
        if (!m_availForPred.above) {
            Zero(m_contexts.aboveNonzero);
            Zero(m_contexts.abovePartition);
        }
        if (!m_availForPred.left) {
            Zero(m_contexts.leftNonzero);
            Zero(m_contexts.leftPartition);
        }
        m_contextsInitSb = m_contexts;
        // Hardcoded without dqp, can support dqp with depth 0 (SB level)
        m_aqparamY = m_par->qparamY[m_lumaQp];
        m_aqparamUv[0] = m_par->qparamUv[m_chromaQp];
        m_aqparamUv[1] = m_par->qparamUv[m_chromaQp];

        m_SCid[0][0] = 5;
        m_SCpp[0][0] = 1764.0;
        m_STC[0][0] = 2;
        m_mvdMax = 128;
        m_mvdCost = 60;
        //GetSpatialComplexity(0, 0, 0, 0);
        m_intraMinDepth = 0;
        m_adaptMaxDepth = MAX_TOTAL_DEPTH;
        m_projMinDepth = 0;
        m_adaptMinDepth = 0;
        //if (m_par->minCUDepthAdapt)
        //    GetAdaptiveMinDepth(0, 0);

        if (!frame.IsIntra()) {
            //CalculateZeroMvPredAndSatd();
            if (m_par->enableCmFlag)
                CalculateNewMvPredAndSatd();
        }

        MemoizeInit();
    }
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

    template <typename PixType> template <int32_t depth>
    void AV1CU<PixType>::MeCuNonRdAV1(int32_t miRow, int32_t miCol)
    {
        const BlockSize bsz = GetSbType(depth, PARTITION_NONE);
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t num4x4h = 1 << log2h;
        const int32_t num4x4 = 1 << (log2w + log2h);
        const int32_t sbx = (miCol & 7) << 3;
        const int32_t sby = (miRow & 7) << 3;
        const int32_t sbw = num4x4w << 2;
        const int32_t sbh = num4x4h << 2;
        const TxSize maxTxSize = std::min((TxSize)TX_32X32, TxSize(4 - depth));
        const PixType *srcPic = m_ySrc + sbx + sby * SRC_PITCH;
        const QuantParam &qparam = m_aqparamY;
        const BitCounts &bc = *m_currFrame->bitCount;
        ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
        const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
        const ModeInfo *left = GetLeft(mi, miCol, m_tileBorders.colStart);
        if (above == m_availForPred.curr && left == m_availForPred.curr) {
            // txSize context is expected to be 1 for decisions made by MeCuNonRd (neighbours always have maxTxSize)
            //assert(GetCtxTxSizeAV1(above, left, maxTxSize) == 1);
        }
        const uint16_t *txSizeBits = bc.txSize[maxTxSize][1];
        const int32_t ctxSkip = GetCtxSkip(above, left);
        const int32_t ctxIsInter = GetCtxIsInter(above, left);
        const uint16_t *skipBits = bc.skip[ctxSkip];
        const uint16_t *isInterBits = bc.isInter[ctxIsInter];
        const uint16_t *txTypeBits = bc.interExtTx[maxTxSize];
        const uint16_t *nearMvDrlIdxBits[COMP_VAR0 + 1] = {};
        const int32_t refColocOffset = m_ctbPelX + sbx + (m_ctbPelY + sby) * m_pitchRecLuma;

        bool useHadamard = (m_par->hadamardMe >= 2);

        (m_currFrame->compoundReferenceAllowed)
            ? GetMvRefsAV1TU7B(bsz, miRow, miCol, &m_mvRefs, (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B), 1)
            : GetMvRefsAV1TU7P(bsz, miRow, miCol, &m_mvRefs, 1);

        EstimateRefFramesAllAv1(bc, *m_currFrame, above, left, m_mvRefs.refFrameBitCount, &m_mvRefs.memCtx);

        m_mvRefs.memCtx.skip = ctxSkip;
        m_mvRefs.memCtx.isInter = ctxIsInter;

        uint32_t interModeBitsAV1[5][/*INTER_MODES*/10 + 4];
        uint32_t *interModeBits = NULL;

        uint64_t bestInterCost = ULLONG_MAX;
        int32_t bestInterSatd = INT_MAX;
        int32_t bestInterBits = 0;
        AV1MVPair bestMvs = {};
        alignas(2) RefIdx bestRef[2] = { INTRA_FRAME, NONE_FRAME };
        RefIdx bestRefComb = INTRA_FRAME;
        PredMode bestMode = AV1_NEARESTMV;
        int32_t bestDrlIdx = 0;
        PixType *bestPred = NULL;

        const PixType * const refColoc[3] = {
            reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[LAST_FRAME]->m_recon->y) + refColocOffset,
            reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[GOLDEN_FRAME]->m_recon->y) + refColocOffset,
            reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[ALTREF_FRAME]->m_recon->y) + refColocOffset,
        };

        int32_t satds[5][6 + 4 + 4];
        for (int i = 0; i < 5; i++) for (int j = 0; j < 6 + 4 + 4; j++) satds[i][j] = INT_MAX;
        PixType *preds[5][6 + 4 + 4] = {};
        int32_t slot, d;
        //int32_t size = BSR(sbw | sbh) - 3; assert(sbw <=  (8 << size) && sbh <= (8 << size));
        // NEWMV
        AV1MEInfo meInfo;
        meInfo.depth = depth;
        meInfo.width = sbw;
        meInfo.height = sbh;
        meInfo.posx = sbx;
        meInfo.posy = sby;

        // single-ref MVPs
        for (RefIdx refIdx = LAST_FRAME; refIdx <= ALTREF_FRAME; refIdx++) {
            if (!m_currFrame->isUniq[refIdx])
                continue;

            const AV1MVPair(&refMvs)[3] = m_mvRefs.mvs[refIdx];
            const uint32_t refFrameBits = m_mvRefs.refFrameBitCount[refIdx];

            preds[refIdx][ZEROMV_] = m_interp[refIdx][depth][ZEROMV_] + sbx + sby * 64;
            satds[refIdx][ZEROMV_] = m_zeroMvpSatd[refIdx][depth][(sbx + (sby << depth)) >> (6 - depth)];

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
            }
            else {
                slot = 6;
                d = depth;
                if (SatdSearch(m_nonZeroMvp[refIdx], refMvs[NEARESTMV_], &d, &slot)) {
                    assert(d >= 0 && d < depth && (slot == 0 || slot == 1 || slot == 4 || slot == 5));
                    m_interp[refIdx][depth][NEARESTMV_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                    preds[refIdx][NEARESTMV_] = m_interp[refIdx][depth][NEARESTMV_] + sbx + sby * 64;
                    satds[refIdx][NEARESTMV_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                }
                else {
                    m_interp[refIdx][depth][NEARESTMV_] = vp9scratchpad.interpBufs[refIdx][depth][NEARESTMV_]; // interpolate to a new buffer
                    preds[refIdx][NEARESTMV_] = m_interp[refIdx][depth][NEARESTMV_] + sbx + sby * 64;
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    InterpolateAv1SingleRefLuma_SwPath(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARESTMV_], refMvs[NEARESTMV_][0], sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    InterpolateAv1SingleRefLuma(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARESTMV_], refMvs[NEARESTMV_][0], sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    m_nonZeroMvp[refIdx][depth][NEARESTMV_] = refMvs[NEARESTMV_];
                    satds[refIdx][NEARESTMV_] = MetricSave(srcPic, preds[refIdx][NEARESTMV_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARESTMV_] + (sbx >> 3) + sby, useHadamard);
                }
            }

            if (refMvs[NEARMV_][0].asInt == 0) { // the only case when NEARMV==NEARESTMV is when NEARMV==ZEROMV
                m_interp[refIdx][depth][NEARMV_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                preds[refIdx][NEARMV_] = preds[refIdx][ZEROMV_];
                satds[refIdx][NEARMV_] = satds[refIdx][ZEROMV_];
            }
            else {
                slot = 6;
                d = depth;
                if (SatdSearch(m_nonZeroMvp[refIdx], refMvs[NEARMV_], &d, &slot)) {
                    assert(d >= 0 && d < depth && (slot == 0 || slot == 1 || slot == 4 || slot == 5));
                    m_interp[refIdx][depth][NEARMV_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                    preds[refIdx][NEARMV_] = m_interp[refIdx][depth][NEARMV_] + sbx + sby * 64;
                    satds[refIdx][NEARMV_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                }
                else {
                    m_interp[refIdx][depth][NEARMV_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV_]; // interpolate to a new buffer
                    preds[refIdx][NEARMV_] = m_interp[refIdx][depth][NEARMV_] + sbx + sby * 64;
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    InterpolateAv1SingleRefLuma_SwPath(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARMV_], refMvs[NEARMV_][0], sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    InterpolateAv1SingleRefLuma(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARMV_], refMvs[NEARMV_][0], sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    m_nonZeroMvp[refIdx][depth][NEARMV_] = refMvs[NEARMV_];
                    satds[refIdx][NEARMV_] = MetricSave(srcPic, preds[refIdx][NEARMV_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV_] + (sbx >> 3) + sby, useHadamard);
                }
            }

            const int32_t nearestMvCount = MIN(4, m_mvRefs.mvRefsAV1.nearestMvCount[refIdx]);
            nearMvDrlIdxBits[refIdx] = bc.drlIdxNearMv[nearestMvCount];

            satds[refIdx][NEARMV1_] = INT_MAX;
            satds[refIdx][NEARMV2_] = INT_MAX;
            if (m_mvRefs.mvRefsAV1.refMvCount[refIdx] > 2) {
                AV1MVPair refmv = m_mvRefs.mvRefsAV1.refMvStack[refIdx][2].mv;
                ClampMvRefAV1(&refmv[0], bsz, miRow, miCol, m_par);
                ClampMvRefAV1(&refmv[1], bsz, miRow, miCol, m_par);
                if (refmv[0].asInt == 0) {
                    m_interp[refIdx][depth][NEARMV1_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                    preds[refIdx][NEARMV1_] = preds[refIdx][ZEROMV_];
                    satds[refIdx][NEARMV1_] = satds[refIdx][ZEROMV_];
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    satds[refIdx][NEARMV1_] = INT_MAX;
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                }
                else {
                    slot = 6;
                    d = depth;
                    if (SatdSearch(m_nonZeroMvp[refIdx], refmv, &d, &slot)) {
                        assert(d >= 0 && d < depth && (slot == 0 || slot == 1 || slot == 4 || slot == 5));
                        m_interp[refIdx][depth][NEARMV1_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                        preds[refIdx][NEARMV1_] = m_interp[refIdx][depth][NEARMV1_] + sbx + sby * 64;
                        satds[refIdx][NEARMV1_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                    }
                    else {
                        m_interp[refIdx][depth][NEARMV1_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV1_]; // interpolate to a new buffer
                        preds[refIdx][NEARMV1_] = m_interp[refIdx][depth][NEARMV1_] + sbx + sby * 64;
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                        InterpolateAv1SingleRefLuma_SwPath(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARMV1_], refmv[0], sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                        InterpolateAv1SingleRefLuma(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARMV1_], refmv[0], sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                        m_nonZeroMvp[refIdx][depth][NEARMV1_] = refmv;
                        satds[refIdx][NEARMV1_] = MetricSave(srcPic, preds[refIdx][NEARMV1_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV1_] + (sbx >> 3) + sby, useHadamard);
                    }
                }

                if (m_mvRefs.mvRefsAV1.refMvCount[refIdx] > 3) {
                    refmv = m_mvRefs.mvRefsAV1.refMvStack[refIdx][3].mv;
                    ClampMvRefAV1(&refmv[0], bsz, miRow, miCol, m_par);
                    ClampMvRefAV1(&refmv[1], bsz, miRow, miCol, m_par);
                    if (refmv[0].asInt == 0) {
                        m_interp[refIdx][depth][NEARMV2_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                        preds[refIdx][NEARMV2_] = preds[refIdx][ZEROMV_];
                        satds[refIdx][NEARMV2_] = satds[refIdx][ZEROMV_];
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                        satds[refIdx][NEARMV2_] = INT_MAX;
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    }
                    else {
                        slot = 6;
                        d = depth;
                        if (SatdSearch(m_nonZeroMvp[refIdx], refmv, &d, &slot)) {
                            assert(d >= 0 && d < depth && (slot == 0 || slot == 1 || slot == 4 || slot == 5));
                            m_interp[refIdx][depth][NEARMV2_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                            preds[refIdx][NEARMV2_] = m_interp[refIdx][depth][NEARMV2_] + sbx + sby * 64;
                            satds[refIdx][NEARMV2_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                        }
                        else {
                            m_interp[refIdx][depth][NEARMV2_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV2_]; // interpolate to a new buffer
                            preds[refIdx][NEARMV2_] = m_interp[refIdx][depth][NEARMV2_] + sbx + sby * 64;
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1SingleRefLuma_SwPath(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARMV2_], refmv[0], sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1SingleRefLuma(refColoc[refIdx], m_pitchRecLuma, preds[refIdx][NEARMV2_], refmv[0], sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            m_nonZeroMvp[refIdx][depth][NEARMV2_] = refmv;
                            satds[refIdx][NEARMV2_] = MetricSave(srcPic, preds[refIdx][NEARMV2_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV2_] + (sbx >> 3) + sby, useHadamard);
                        }
                    }
                }
            }

            uint64_t cost = RD(satds[refIdx][NEARESTMV_], refFrameBits + interModeBits[NEARESTMV_], m_rdLambdaSqrtInt);
            if (bestInterCost > cost) {
                bestInterCost = cost;
                bestRef[0] = refIdx;
                bestMode = AV1_NEARESTMV;
                bestDrlIdx = 0;
            }
            cost = RD(satds[refIdx][NEARMV_], refFrameBits + interModeBits[NEARMV_] + nearMvDrlIdxBits[refIdx][0], m_rdLambdaSqrtInt);
            if (bestInterCost > cost) {
                bestInterCost = cost;
                bestRef[0] = refIdx;
                bestMode = AV1_NEARMV;
                bestDrlIdx = 0;
            }
            cost = RD(satds[refIdx][NEARMV1_], refFrameBits + interModeBits[NEARMV_] + nearMvDrlIdxBits[refIdx][1], m_rdLambdaSqrtInt);
            if (bestInterCost > cost) {
                bestInterCost = cost;
                bestRef[0] = refIdx;
                bestMode = AV1_NEARMV;
                bestDrlIdx = 1;
            }
            cost = RD(satds[refIdx][NEARMV2_], refFrameBits + interModeBits[NEARMV_] + nearMvDrlIdxBits[refIdx][2], m_rdLambdaSqrtInt);
            if (bestInterCost > cost) {
                bestInterCost = cost;
                bestRef[0] = refIdx;
                bestMode = AV1_NEARMV;
                bestDrlIdx = 2;
            }

            cost = RD(satds[refIdx][ZEROMV_], refFrameBits + interModeBits[ZEROMV_], m_rdLambdaSqrtInt);
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

                const AV1MVPair(&refMvs)[3] = m_mvRefs.mvs[refIdx];
                const uint32_t refFrameBits = m_mvRefs.refFrameBitCount[refIdx];

                preds[refIdx][ZEROMV_] = m_interp[refIdx][depth][ZEROMV_] + sbx + sby * 64;
                satds[refIdx][ZEROMV_] = m_zeroMvpSatd[refIdx][depth][(sbx + (sby << depth)) >> (6 - depth)];

                const int32_t modeCtx = m_mvRefs.mvRefsAV1.interModeCtx[refIdx];
                interModeBitsAV1[refIdx][NEWMV_] = bc.interCompMode[modeCtx][NEW_NEWMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][ZEROMV_] = bc.interCompMode[modeCtx][ZERO_ZEROMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][NEARESTMV_] = bc.interCompMode[modeCtx][NEAREST_NEARESTMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][NEARMV_] = bc.interCompMode[modeCtx][NEAR_NEARMV - NEAREST_NEARESTMV];

                interModeBitsAV1[refIdx][NEAREST_NEWMV_] = bc.interCompMode[modeCtx][NEAREST_NEWMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][NEW_NEARESTMV_] = bc.interCompMode[modeCtx][NEW_NEARESTMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][NEAR_NEWMV_] = bc.interCompMode[modeCtx][NEAR_NEWMV - NEAREST_NEARESTMV];
                interModeBitsAV1[refIdx][NEW_NEARMV_] = bc.interCompMode[modeCtx][NEW_NEARMV - NEAREST_NEARESTMV];

                interModeBitsAV1[refIdx][NEAR_NEWMV1_] = interModeBitsAV1[refIdx][NEAR_NEWMV_];
                interModeBitsAV1[refIdx][NEAR_NEWMV2_] = interModeBitsAV1[refIdx][NEAR_NEWMV_];
                interModeBitsAV1[refIdx][NEW_NEARMV1_] = interModeBitsAV1[refIdx][NEW_NEARMV_];
                interModeBitsAV1[refIdx][NEW_NEARMV2_] = interModeBitsAV1[refIdx][NEW_NEARMV_];

                interModeBits = interModeBitsAV1[refIdx];

                if (refMvs[NEARESTMV_].asInt64 == 0) {
                    m_interp[refIdx][depth][NEARESTMV_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                    preds[refIdx][NEARESTMV_] = preds[refIdx][ZEROMV_];
                    satds[refIdx][NEARESTMV_] = satds[refIdx][ZEROMV_];
                }
                else {
                    slot = 6;
                    d = depth;
                    if (SatdSearch(m_nonZeroMvp[refIdx], refMvs[NEARESTMV_], &d, &slot)) {
                        assert(d >= 0 && d < depth && (slot == 0 || slot == 1 || slot == 4 || slot == 5));
                        m_interp[refIdx][depth][NEARESTMV_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                        preds[refIdx][NEARESTMV_] = m_interp[refIdx][depth][NEARESTMV_] + sbx + sby * 64;
                        satds[refIdx][NEARESTMV_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                    }
                    else {
                        m_interp[refIdx][depth][NEARESTMV_] = vp9scratchpad.interpBufs[refIdx][depth][NEARESTMV_]; // interpolate to a new buffer
                        preds[refIdx][NEARESTMV_] = m_interp[refIdx][depth][NEARESTMV_] + sbx + sby * 64;

                        AV1MV mv0single = m_mvRefs.mvs[varRef][NEARESTMV_][0];
                        AV1MV mv1single = m_mvRefs.mvs[fixRef][NEARESTMV_][0];
                        AV1MV mv0comp = m_mvRefs.mvs[refIdx][NEARESTMV_][0];
                        AV1MV mv1comp = m_mvRefs.mvs[refIdx][NEARESTMV_][1];
                        if (mv0comp.asInt != mv0single.asInt && mv1comp.asInt != mv1single.asInt) {
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            alignas(64) PixType tmp1[64 * 64];
                            alignas(64) PixType tmp2[64 * 64];
                            InterpolateAv1SingleRefLuma_SwPath(refColoc[varRef], m_pitchRecLuma, tmp1, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
                            InterpolateAv1SingleRefLuma_SwPath(refColoc[fixRef], m_pitchRecLuma, tmp2, mv1comp, sbh, log2w, DEF_FILTER_DUAL);
                            AV1PP::average(tmp1, tmp2, preds[refIdx][NEARESTMV_], sbh, log2w);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1FirstRefLuma(refColoc[varRef], m_pitchRecLuma, vp9scratchpad.compPredIm, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
                            InterpolateAv1SecondRefLuma(refColoc[fixRef], m_pitchRecLuma, vp9scratchpad.compPredIm, preds[refIdx][NEARESTMV_], mv1comp, sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                        }
                        else if (mv0comp.asInt != mv0single.asInt) {
                            PixType *pred0 = preds[refIdx][NEARESTMV_];
                            PixType *pred1 = m_interp[fixRef][depth][NEARESTMV_] + sbx + sby * 64;
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1SingleRefLuma_SwPath(refColoc[varRef], m_pitchRecLuma, pred0, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, pred0, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            AV1PP::average(pred0, pred1, pred0, sbh, log2w);
                        }
                        else if (mv1comp.asInt != mv1single.asInt) {
                            PixType *pred0 = m_interp[varRef][depth][NEARESTMV_] + sbx + sby * 64;
                            PixType *pred1 = preds[refIdx][NEARESTMV_];
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1SingleRefLuma_SwPath(refColoc[fixRef], m_pitchRecLuma, pred1, mv1comp, sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, pred1, mv1comp, sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            AV1PP::average(pred0, pred1, pred1, sbh, log2w);
                        }
                        else {
                            PixType *pred0 = m_interp[varRef][depth][NEARESTMV_] + sbx + sby * 64;
                            PixType *pred1 = m_interp[fixRef][depth][NEARESTMV_] + sbx + sby * 64;
                            AV1PP::average(pred0, pred1, preds[refIdx][NEARESTMV_], sbh, log2w); // average existing predictions
                        }
                        m_nonZeroMvp[refIdx][depth][NEARESTMV_] = refMvs[NEARESTMV_];
                        satds[refIdx][NEARESTMV_] = MetricSave(srcPic, preds[refIdx][NEARESTMV_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARESTMV_] + (sbx >> 3) + sby, useHadamard);
                    }
                }

                if (refMvs[NEARMV_].asInt64 == 0) {
                    m_interp[refIdx][depth][NEARMV_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                    preds[refIdx][NEARMV_] = preds[refIdx][ZEROMV_];
                    satds[refIdx][NEARMV_] = satds[refIdx][ZEROMV_];
                }
                else {
                    slot = 6;
                    d = depth;
                    if (SatdSearch(m_nonZeroMvp[refIdx], refMvs[NEARMV_], &d, &slot)) {
                        assert(d >= 0 && d < depth && (slot == 0 || slot == 1 || slot == 4 || slot == 5));
                        m_interp[refIdx][depth][NEARMV_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                        preds[refIdx][NEARMV_] = m_interp[refIdx][depth][NEARMV_] + sbx + sby * 64;
                        satds[refIdx][NEARMV_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                    }
                    else {
                        m_interp[refIdx][depth][NEARMV_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV_]; // interpolate to a new buffer
                        preds[refIdx][NEARMV_] = m_interp[refIdx][depth][NEARMV_] + sbx + sby * 64;

                        AV1MV mv0single = m_mvRefs.mvs[varRef][NEARMV_][0];
                        AV1MV mv1single = m_mvRefs.mvs[fixRef][NEARMV_][0];
                        AV1MV mv0comp = m_mvRefs.mvs[refIdx][NEARMV_][0];
                        AV1MV mv1comp = m_mvRefs.mvs[refIdx][NEARMV_][1];
                        if (mv0comp.asInt != mv0single.asInt && mv1comp.asInt != mv1single.asInt) {
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            alignas(64) PixType tmp1[64 * 64];
                            alignas(64) PixType tmp2[64 * 64];
                            InterpolateAv1SingleRefLuma_SwPath(refColoc[varRef], m_pitchRecLuma, tmp1, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
                            InterpolateAv1SingleRefLuma_SwPath(refColoc[fixRef], m_pitchRecLuma, tmp2, mv1comp, sbh, log2w, DEF_FILTER_DUAL);
                            AV1PP::average(tmp1, tmp2, preds[refIdx][NEARMV_], sbh, log2w);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1FirstRefLuma(refColoc[varRef], m_pitchRecLuma, vp9scratchpad.compPredIm, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
                            InterpolateAv1SecondRefLuma(refColoc[fixRef], m_pitchRecLuma, vp9scratchpad.compPredIm, preds[refIdx][NEARMV_], mv1comp, sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                        }
                        else if (mv0comp.asInt != mv0single.asInt) {
                            PixType *pred0 = preds[refIdx][NEARMV_];
                            PixType *pred1 = m_interp[fixRef][depth][NEARMV_] + sbx + sby * 64;
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1SingleRefLuma_SwPath(refColoc[varRef], m_pitchRecLuma, pred0, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, pred0, mv0comp, sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            AV1PP::average(pred0, pred1, pred0, sbh, log2w);
                        }
                        else if (mv1comp.asInt != mv1single.asInt) {
                            PixType *pred0 = m_interp[varRef][depth][NEARMV_] + sbx + sby * 64;
                            PixType *pred1 = preds[refIdx][NEARMV_];
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1SingleRefLuma_SwPath(refColoc[fixRef], m_pitchRecLuma, pred1, mv1comp, sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, pred1, mv1comp, sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            AV1PP::average(pred0, pred1, pred1, sbh, log2w);
                        }
                        else {
                            PixType *pred0 = m_interp[varRef][depth][NEARMV_] + sbx + sby * 64;
                            PixType *pred1 = m_interp[fixRef][depth][NEARMV_] + sbx + sby * 64;
                            AV1PP::average(pred0, pred1, preds[refIdx][NEARMV_], sbh, log2w); // average existing predictions
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
                    AV1MVPair refmv = m_mvRefs.mvRefsAV1.refMvStack[refIdx][2].mv;
                    if (refmv.asInt64 == 0) {
                        m_interp[refIdx][depth][NEARMV1_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                        preds[refIdx][NEARMV1_] = preds[refIdx][ZEROMV_];
                        satds[refIdx][NEARMV1_] = satds[refIdx][ZEROMV_];
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                        satds[refIdx][NEARMV1_] = INT_MAX;
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                    }
                    else {
                        slot = 6;
                        d = depth;
                        if (SatdSearch(m_nonZeroMvp[refIdx], refmv, &d, &slot)) {
                            assert(d >= 0 && d < depth && (slot == 0 || slot == 1 || slot == 4 || slot == 5));
                            m_interp[refIdx][depth][NEARMV1_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                            preds[refIdx][NEARMV1_] = m_interp[refIdx][depth][NEARMV1_] + sbx + sby * 64;
                            satds[refIdx][NEARMV1_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                        }
                        else {
                            m_interp[refIdx][depth][NEARMV1_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV1_]; // interpolate to a new buffer
                            preds[refIdx][NEARMV1_] = m_interp[refIdx][depth][NEARMV1_] + sbx + sby * 64;

                            const AV1MVPair &mvComp = m_mvRefs.mvRefsAV1.refMvStack[refIdx][2].mv;
                            int32_t sameMv0 = 0;
                            int32_t sameMv1 = 0;
                            if (m_mvRefs.mvRefsAV1.refMvCount[varRef] > 2)
                                sameMv0 = (mvComp[0].asInt == m_mvRefs.mvRefsAV1.refMvStack[varRef][2].mv[0].asInt);
                            if (m_mvRefs.mvRefsAV1.refMvCount[fixRef] > 2)
                                sameMv1 = (mvComp[1].asInt == m_mvRefs.mvRefsAV1.refMvStack[fixRef][2].mv[0].asInt);

                            if (!sameMv0 && !sameMv1) {
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                alignas(64) PixType tmp1[64 * 64];
                                alignas(64) PixType tmp2[64 * 64];
                                InterpolateAv1SingleRefLuma_SwPath(refColoc[varRef], m_pitchRecLuma, tmp1, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                                InterpolateAv1SingleRefLuma_SwPath(refColoc[fixRef], m_pitchRecLuma, tmp2, mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
                                AV1PP::average(tmp1, tmp2, preds[refIdx][NEARMV1_], sbh, log2w);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                InterpolateAv1FirstRefLuma(refColoc[varRef], m_pitchRecLuma, vp9scratchpad.compPredIm, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                                InterpolateAv1SecondRefLuma(refColoc[fixRef], m_pitchRecLuma, vp9scratchpad.compPredIm, preds[refIdx][NEARMV1_], mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            }
                            else if (!sameMv0) {
                                PixType *pred0 = preds[refIdx][NEARMV1_];
                                PixType *pred1 = m_interp[fixRef][depth][NEARMV1_] + sbx + sby * 64;
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                InterpolateAv1SingleRefLuma_SwPath(refColoc[varRef], m_pitchRecLuma, pred0, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, pred0, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                AV1PP::average(pred0, pred1, pred0, sbh, log2w);
                            }
                            else if (!sameMv1) {
                                PixType *pred0 = m_interp[varRef][depth][NEARMV1_] + sbx + sby * 64;
                                PixType *pred1 = preds[refIdx][NEARMV1_];
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                InterpolateAv1SingleRefLuma_SwPath(refColoc[fixRef], m_pitchRecLuma, pred1, mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, pred1, mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                AV1PP::average(pred0, pred1, pred1, sbh, log2w);
                            }
                            else {
                                PixType *pred0 = m_interp[varRef][depth][NEARMV1_] + sbx + sby * 64;
                                PixType *pred1 = m_interp[fixRef][depth][NEARMV1_] + sbx + sby * 64;
                                AV1PP::average(pred0, pred1, preds[refIdx][NEARMV1_], sbh, log2w); // average existing predictions
                            }
                            m_nonZeroMvp[refIdx][depth][NEARMV1_] = refmv;
                            satds[refIdx][NEARMV1_] = MetricSave(srcPic, preds[refIdx][NEARMV1_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV1_] + (sbx >> 3) + sby, useHadamard);
                        }
                    }

                    if (m_mvRefs.mvRefsAV1.refMvCount[refIdx] > 3) {
                        refmv = m_mvRefs.mvRefsAV1.refMvStack[refIdx][3].mv;
                        if (refmv.asInt64 == 0) {
                            m_interp[refIdx][depth][NEARMV2_] = m_interp[refIdx][depth][ZEROMV_]; // reuse interpolation from another inter mode
                            preds[refIdx][NEARMV2_] = preds[refIdx][ZEROMV_];
                            satds[refIdx][NEARMV2_] = satds[refIdx][ZEROMV_];
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                            satds[refIdx][NEARMV2_] = INT_MAX;
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                        }
                        else {
                            slot = 6;
                            d = depth;
                            if (SatdSearch(m_nonZeroMvp[refIdx], refmv, &d, &slot)) {
                                assert(d >= 0 && d < depth && (slot == 0 || slot == 1 || slot == 4 || slot == 5));
                                m_interp[refIdx][depth][NEARMV2_] = m_interp[refIdx][d][slot]; // reuse interpolation from another depth
                                preds[refIdx][NEARMV2_] = m_interp[refIdx][depth][NEARMV2_] + sbx + sby * 64;
                                satds[refIdx][NEARMV2_] = SatdUse<depth>(m_nonZeroMvpSatd[refIdx][d][slot] + (sbx >> 3) + sby);
                            }
                            else {
                                m_interp[refIdx][depth][NEARMV2_] = vp9scratchpad.interpBufs[refIdx][depth][NEARMV2_]; // interpolate to a new buffer
                                preds[refIdx][NEARMV2_] = m_interp[refIdx][depth][NEARMV2_] + sbx + sby * 64;

                                const AV1MVPair &mvComp = m_mvRefs.mvRefsAV1.refMvStack[refIdx][3].mv;
                                int32_t sameMv0 = 0;
                                int32_t sameMv1 = 0;
                                if (m_mvRefs.mvRefsAV1.refMvCount[varRef] > 3)
                                    sameMv0 = (mvComp[0].asInt == m_mvRefs.mvRefsAV1.refMvStack[varRef][3].mv[0].asInt);
                                if (m_mvRefs.mvRefsAV1.refMvCount[fixRef] > 3)
                                    sameMv1 = (mvComp[1].asInt == m_mvRefs.mvRefsAV1.refMvStack[fixRef][3].mv[0].asInt);

                                if (!sameMv0 && !sameMv1) {
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                    alignas(64) PixType tmp1[64 * 64];
                                    alignas(64) PixType tmp2[64 * 64];
                                    InterpolateAv1SingleRefLuma_SwPath(refColoc[varRef], m_pitchRecLuma, tmp1, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                                    InterpolateAv1SingleRefLuma_SwPath(refColoc[fixRef], m_pitchRecLuma, tmp2, mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
                                    AV1PP::average(tmp1, tmp2, preds[refIdx][NEARMV2_], sbh, log2w);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                    InterpolateAv1FirstRefLuma(refColoc[varRef], m_pitchRecLuma, vp9scratchpad.compPredIm, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                                    InterpolateAv1SecondRefLuma(refColoc[fixRef], m_pitchRecLuma, vp9scratchpad.compPredIm, preds[refIdx][NEARMV2_], mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                }
                                else if (!sameMv0) {
                                    PixType *pred0 = preds[refIdx][NEARMV2_];
                                    PixType *pred1 = m_interp[fixRef][depth][NEARMV2_] + sbx + sby * 64;
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                    InterpolateAv1SingleRefLuma_SwPath(refColoc[varRef], m_pitchRecLuma, pred0, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                    InterpolateAv1SingleRefLuma(refColoc[varRef], m_pitchRecLuma, pred0, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                    AV1PP::average(pred0, pred1, pred0, sbh, log2w);
                                }
                                else if (!sameMv1) {
                                    PixType *pred0 = m_interp[varRef][depth][NEARMV2_] + sbx + sby * 64;
                                    PixType *pred1 = preds[refIdx][NEARMV2_];
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                    InterpolateAv1SingleRefLuma_SwPath(refColoc[fixRef], m_pitchRecLuma, pred1, mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                    InterpolateAv1SingleRefLuma(refColoc[fixRef], m_pitchRecLuma, pred1, mvComp[1], sbh, log2w, DEF_FILTER_DUAL);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                                    AV1PP::average(pred0, pred1, pred1, sbh, log2w);
                                }
                                else {
                                    PixType *pred0 = m_interp[varRef][depth][NEARMV2_] + sbx + sby * 64;
                                    PixType *pred1 = m_interp[fixRef][depth][NEARMV2_] + sbx + sby * 64;
                                    AV1PP::average(pred0, pred1, preds[refIdx][NEARMV2_], sbh, log2w); // average existing predictions
                                }
                                m_nonZeroMvp[refIdx][depth][NEARMV2_] = refmv;
                                satds[refIdx][NEARMV2_] = MetricSave(srcPic, preds[refIdx][NEARMV2_], depth, m_nonZeroMvpSatd[refIdx][depth][NEARMV2_] + (sbx >> 3) + sby, useHadamard);
                            }
                        }
                    }
                }

                uint64_t cost = RD(satds[refIdx][NEARESTMV_], refFrameBits + interModeBits[NEARESTMV_], m_rdLambdaSqrtInt);
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
                cost = RD(satds[refIdx][ZEROMV_], refFrameBits + interModeBits[ZEROMV_], m_rdLambdaSqrtInt);
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
        }
        else {
            bestInterSatd = satds[bestRefComb][bestMode - AV1_NEARESTMV];
            bestMvs.asInt64 = m_mvRefs.mvs[bestRefComb][bestMode - AV1_NEARESTMV].asInt64;
            bestPred = preds[bestRefComb][bestMode - AV1_NEARESTMV];
        }


#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        (m_par->enableCmFlag) ? MePuGacc_SwPath<depth>(&meInfo) : MePu(&meInfo);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        (m_par->enableCmFlag) ? MePuGacc<depth>(&meInfo) : MePu(&meInfo);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

#ifndef USE_SAD_ONLY
        if (m_par->enableCmFlag) {
#ifdef REINTERP_MEPU
            if (meInfo.refIdx[1] == NONE_FRAME) {
                if ((meInfo.mv[0].mvx & 0x7) && (meInfo.mv[0].mvy & 0x7)) {
                    InterpolateAv1SingleRefLuma(refColoc[meInfo.refIdx[0]], m_pitchRecLuma, m_interp[meInfo.refIdxComb][depth][NEWMV_] + sbx + sby * 64, meInfo.mv[0], sbh, log2w, DEF_FILTER_DUAL);
                    if (!useHadamard) meInfo.satd = AV1PP::sad_special(srcPic, SRC_PITCH, m_interp[meInfo.refIdxComb][depth][NEWMV_] + sbx + sby * 64, log2w, log2h);
                }
            }
            else {
                if (((meInfo.mv[0].mvx & 0x7) || (meInfo.mv[0].mvy & 0x7)) || ((meInfo.mv[1].mvx & 0x7) || (meInfo.mv[1].mvy & 0x7))) {
                    InterpolateAv1FirstRefLuma(refColoc[meInfo.refIdx[0]], m_pitchRecLuma, vp9scratchpad.compPredIm, meInfo.mv[0], sbh, log2w, DEF_FILTER_DUAL);
                    InterpolateAv1SecondRefLuma(refColoc[meInfo.refIdx[1]], m_pitchRecLuma, vp9scratchpad.compPredIm, m_interp[meInfo.refIdxComb][depth][NEWMV_] + sbx + sby * 64, meInfo.mv[1], sbh, log2w, DEF_FILTER_DUAL);
                    if (!useHadamard) meInfo.satd = AV1PP::sad_special(srcPic, SRC_PITCH, m_interp[meInfo.refIdxComb][depth][NEWMV_] + sbx + sby * 64, log2w, log2h);
                }
            }
#endif
            if (useHadamard) {
                meInfo.satd = AV1PP::satd(srcPic, m_interp[meInfo.refIdxComb][depth][NEWMV_] + sbx + sby * 64, log2w, log2h);
            }
        }
#endif
        int32_t newMvDrlIdx = 0;
        AV1MVPair bestRefMv = m_mvRefs.mvs[meInfo.refIdxComb][NEARESTMV_];

        const int32_t nearestMvCount = MIN(4, m_mvRefs.mvRefsAV1.nearestMvCount[meInfo.refIdxComb]);
        const uint16_t *newMvDrlIdxBits = bc.drlIdxNewMv[nearestMvCount];

        meInfo.mvBits += newMvDrlIdxBits[0];
        for (int32_t idx = 1; idx < 3; idx++) {
            if (m_mvRefs.mvRefsAV1.refMvCount[meInfo.refIdxComb] < idx + 1)
                break;
            AV1MVPair refMv = m_mvRefs.mvRefsAV1.refMvStack[meInfo.refIdxComb][idx].mv;
            ClampMvRefAV1(refMv + 0, bsz, sby >> 3, sbx >> 3, m_par);
            ClampMvRefAV1(refMv + 1, bsz, sby >> 3, sbx >> 3, m_par);
            uint32_t mvBits = MvCost(meInfo.mv[0], refMv[0], m_mvRefs.useMvHp[meInfo.refIdx[0]]);
            if (meInfo.refIdx[1] != NONE_FRAME)
                mvBits += MvCost(meInfo.mv[1], refMv[1], m_mvRefs.useMvHp[meInfo.refIdx[1]]);
            mvBits += newMvDrlIdxBits[idx];
            if (meInfo.mvBits > mvBits) {
                meInfo.mvBits = mvBits;
                newMvDrlIdx = idx;
                bestRefMv = refMv;
            }
        }

        int32_t useExtCompModes = 0;
        interModeBits = interModeBitsAV1[meInfo.refIdxComb];

        uint32_t modeBits = m_mvRefs.refFrameBitCount[meInfo.refIdxComb] + interModeBits[NEWMV_] + meInfo.mvBits;
        uint64_t costNewMv = RD(meInfo.satd, modeBits, m_rdLambdaSqrtInt);
        if (bestInterCost > costNewMv) {
            bestInterCost = costNewMv;
            bestInterSatd = meInfo.satd;
            bestInterBits = modeBits;
            bestMvs.asInt64 = meInfo.mv.asInt64;
            As2B(bestRef) = As2B(meInfo.refIdx);
            bestRefComb = meInfo.refIdxComb;
            bestMode = AV1_NEWMV;
            bestPred = m_interp[meInfo.refIdxComb][depth][NEWMV_] + sbx + sby * 64;
            bestDrlIdx = newMvDrlIdx;
        }

        // add experiment with EXT_REF_MODES (non active now)
        // Q+0.2%, S down due to extra interpolation.
        // will be ON after enabling caching approach + GPU kernels update
        if (useExtCompModes && m_currFrame->compoundReferenceAllowed) {
            interModeBits = interModeBitsAV1[COMP_VAR0];
            const RefIdx refIdx = COMP_VAR0;
            const RefIdx fixRef = (RefIdx)m_currFrame->compFixedRef;
            const RefIdx varRef = (RefIdx)m_currFrame->compVarRef[refIdx - COMP_VAR0];

            const int32_t listOfModes[] = { NEAREST_NEWMV_, NEW_NEARESTMV_, NEAR_NEWMV_, NEW_NEARMV_, NEAR_NEWMV1_, NEW_NEARMV1_, NEAR_NEWMV2_, NEW_NEARMV2_ };
            const int32_t extModesCount = 8;
            for (int32_t idx = 0; idx < extModesCount; idx++) {

                int32_t drlIdx = 0;
                int32_t drlBits = 0;
                int modeAsIdx = listOfModes[idx];

                AV1MVPair mvCompRef = m_mvRefs.mvs[refIdx][NEARESTMV_];
                if (modeAsIdx == NEAR_NEWMV_ || modeAsIdx == NEW_NEARMV_) {
                    mvCompRef = m_mvRefs.mvs[refIdx][NEARMV_];
                    drlIdx = 0;
                    drlBits = nearMvDrlIdxBits[refIdx][0];
                }
                else if (modeAsIdx == NEAR_NEWMV1_ || modeAsIdx == NEW_NEARMV1_) {

                    if (m_mvRefs.mvRefsAV1.refMvCount[refIdx] < 3) continue;

                    mvCompRef = m_mvRefs.mvRefsAV1.refMvStack[refIdx][2].mv;
                    drlIdx = 1;
                    drlBits = nearMvDrlIdxBits[refIdx][1];
                }
                else if (modeAsIdx == NEAR_NEWMV2_ || modeAsIdx == NEW_NEARMV2_) {

                    if (m_mvRefs.mvRefsAV1.refMvCount[refIdx] < 4) continue;

                    mvCompRef = m_mvRefs.mvRefsAV1.refMvStack[refIdx][3].mv;
                    drlIdx = 2;
                    drlBits = nearMvDrlIdxBits[refIdx][2];
                }

                int32_t parityBit = (idx & 0x1);
                //AV1MVPair mvComp = parityBit ? ({meInfo.mv[0], mvCompRef[1]}) : ({mvCompRef[0], meInfo.mv[1]});
                AV1MVPair mvComp = { mvCompRef[0], meInfo.mv[1] };
                if (parityBit) {
                    mvComp[0] = meInfo.mv[0];
                    mvComp[1] = mvCompRef[1];
                }

                m_interp[refIdx][depth][modeAsIdx] = vp9scratchpad.interpBufs[refIdx][depth][modeAsIdx]; // interpolate to a new buffer
                preds[refIdx][modeAsIdx] = m_interp[refIdx][depth][modeAsIdx] + sbx + sby * 64;

                InterpolateAv1FirstRefLuma(refColoc[varRef], m_pitchRecLuma, vp9scratchpad.compPredIm, mvComp[0], sbh, log2w, DEF_FILTER_DUAL);
                InterpolateAv1SecondRefLuma(refColoc[fixRef], m_pitchRecLuma, vp9scratchpad.compPredIm, preds[refIdx][modeAsIdx], mvComp[1], sbh, log2w, DEF_FILTER_DUAL);

                //satds[refIdx][mode] = MetricSave(srcPic, preds[refIdx][mode], depth, m_nonZeroMvpSatd[refIdx][depth][mode] + (sbx >> 3) + sby, useHadamard);
                if (useHadamard) {
                    satds[refIdx][modeAsIdx] = AV1PP::satd(srcPic, preds[refIdx][modeAsIdx], 4 - depth, 4 - depth);
                }
                else {
                    satds[refIdx][modeAsIdx] = AV1PP::sad_special(srcPic, 64, preds[refIdx][modeAsIdx], 4 - depth, 4 - depth);
                }

                m_nonZeroMvp[refIdx][depth][modeAsIdx] = mvComp;

                modeBits = m_mvRefs.refFrameBitCount[refIdx] + interModeBits[modeAsIdx] + MvCost(mvComp[1 - parityBit], mvCompRef[1 - parityBit], 0) + drlBits;

                uint64_t cost = RD(satds[refIdx][modeAsIdx], modeBits, m_rdLambdaSqrtInt);

                if (bestInterCost > cost) {
                    bestInterCost = cost;
                    bestInterSatd = satds[refIdx][modeAsIdx];
                    bestInterBits = modeBits;
                    bestMvs = mvComp;

                    bestRef[0] = varRef;
                    bestRef[1] = fixRef;

                    bestRefComb = refIdx;
                    bestPred = m_interp[bestRefComb][depth][modeAsIdx] + sbx + sby * 64;
                    bestRefMv = mvCompRef;

                    bestMode = GetModeFromIdx(modeAsIdx);
                    bestDrlIdx = drlIdx;
                }
            }
        }

#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        // re-evaluate all inter modes
        // with simplifications and in order as GPU kernel does

        // NEWMV first
        const int maxNumNewMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[meInfo.refIdxComb] - 1);

        bestInterBits = MvCost(meInfo.mv[0], m_mvRefs.bestMv[meInfo.refIdxComb][0], 0);
        if (meInfo.refIdx[1] != NONE_FRAME)
            bestInterBits += MvCost(meInfo.mv[1], m_mvRefs.bestMv[meInfo.refIdxComb][1], 0);
        bestInterBits += std::min(maxNumNewMvDrlBits, 1) << 9;
        bestDrlIdx = 0;

        if (m_mvRefs.mvRefsAV1.refMvCount[meInfo.refIdxComb] > 1) {
            int32_t bits = MvCost(meInfo.mv[0], m_mvRefs.mvRefsAV1.refMvStack[meInfo.refIdxComb][1].mv[0], 0);
            if (meInfo.refIdx[1] != NONE_FRAME)
                bits += MvCost(meInfo.mv[1], m_mvRefs.mvRefsAV1.refMvStack[meInfo.refIdxComb][1].mv[1], 0);
            bits += std::min(maxNumNewMvDrlBits, 2) << 9;
            if (bestInterBits > bits) {
                bestInterBits = bits;
                bestDrlIdx = 1;
            }
        }
        if (m_mvRefs.mvRefsAV1.refMvCount[meInfo.refIdxComb] > 2) {
            int32_t bits = MvCost(meInfo.mv[0], m_mvRefs.mvRefsAV1.refMvStack[meInfo.refIdxComb][2].mv[0], 0);
            if (meInfo.refIdx[1] != NONE_FRAME)
                bits += MvCost(meInfo.mv[1], m_mvRefs.mvRefsAV1.refMvStack[meInfo.refIdxComb][2].mv[1], 0);
            bits += std::min(maxNumNewMvDrlBits, 2) << 9;
            if (bestInterBits > bits) {
                bestInterBits = bits;
                bestDrlIdx = 2;
            }
        }

        bestInterBits += m_mvRefs.refFrameBitCount[meInfo.refIdxComb];
        bestInterBits += interModeBitsAV1[meInfo.refIdxComb][NEWMV_];
        bestInterCost = RD32(meInfo.satd, bestInterBits, m_rdLambdaSqrtInt32);
        bestMvs.asInt64 = meInfo.mv.asInt64;
        As2B(bestRef) = As2B(meInfo.refIdx);
        bestRefComb = meInfo.refIdxComb;
        bestMode = AV1_NEWMV;
        bestPred = m_interp[meInfo.refIdxComb][depth][NEWMV_] + sbx + sby * 64;
        if (m_currFrame->m_poc == 1) debug_printf("(%d,%2d,%2d) NEWMV ref=%d mv=(%d,%d),(%d,%d) cost=%u bits=%d\n", depth,
            miRow, miCol, bestRefComb, bestMvs[0].mvx, bestMvs[0].mvy, bestMvs[1].mvx, bestMvs[1].mvy, (uint32_t)bestInterCost, bestInterBits);

        const int secondRef = m_currFrame->compoundReferenceAllowed ? ALTREF_FRAME : GOLDEN_FRAME;
        const int MODES_IDX[4] = { NEARESTMV_, NEARMV_, NEARMV1_, NEARMV2_ };

        uint32_t costRef0 = MAX_UINT, costRef1 = MAX_UINT, costComp;
        uint32_t sadRef0, sadRef1, sadComp;
        uint32_t bitsRef0, bitsRef1, bitsComp;
        int modeRef0, modeRef1;
        int checkedMv[3][4] = {};

        int m, mode_idx, maxNumNearMvDrlBits, numNearMvDrlBits, ref;
        uint32_t bits, cost_;
        AV1MVPair refmv;

        if (sbw <= 16) {
            if (m_currFrame->compoundReferenceAllowed) {
                int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[COMP_VAR0] - 2);
                for (m = 0; m < 4; m++) {
                    if (m >= 2 && m >= m_mvRefs.mvRefsAV1.refMvCount[COMP_VAR0])
                        continue;
                    refmv = (m < 2) ? m_mvRefs.mvs[COMP_VAR0][m] : m_mvRefs.mvRefsAV1.refMvStack[COMP_VAR0][m].mv;
                    if (refmv.asInt64 == 0)
                        continue;
                    int numNearMvDrlBits = std::min(maxNumNearMvDrlBits, m);

                    modeRef0 = -1;
                    modeRef1 = -1;
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
                        const int numNearMvDrlBits = std::min(maxNumNearMvDrlBits, modeRef0);
                        sadRef0 = refmv[0].asInt ? satds[LAST_FRAME][MODES_IDX[modeRef0]] : satds[LAST_FRAME][ZEROMV_];
                        bitsRef0 = m_mvRefs.refFrameBitCount[LAST_FRAME];
                        bitsRef0 += interModeBitsAV1[LAST_FRAME][modeRef0 == 0 ? NEARESTMV_ : NEARMV_];
                        bitsRef0 += numNearMvDrlBits << 9;
                        costRef0 = RD32(sadRef0, bitsRef0, m_rdLambdaSqrtInt32);
                        checkedMv[LAST_FRAME][modeRef0] = 1;
                        if (m_currFrame->m_poc == 1) debug_printf("(%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n", depth,
                            miRow, miCol, 0, modeRef0, refmv[0].mvx, refmv[0].mvy, costRef0, bitsRef0);
                    }

                    if (modeRef1 >= 0) {
                        const int maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[ALTREF_FRAME] - 2);
                        const int numNearMvDrlBits = std::min(maxNumNearMvDrlBits, modeRef1);
                        sadRef1 = refmv[1].asInt ? satds[ALTREF_FRAME][MODES_IDX[modeRef1]] : satds[ALTREF_FRAME][ZEROMV_];
                        bitsRef1 = m_mvRefs.refFrameBitCount[ALTREF_FRAME];
                        bitsRef1 += interModeBitsAV1[ALTREF_FRAME][modeRef1 == 0 ? NEARESTMV_ : NEARMV_];
                        bitsRef1 += numNearMvDrlBits << 9;
                        costRef1 = RD32(sadRef1, bitsRef1, m_rdLambdaSqrtInt32);
                        checkedMv[ALTREF_FRAME][modeRef1] = 1;
                        if (m_currFrame->m_poc == 1) debug_printf("(%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n",
                            depth, miRow, miCol, 1, modeRef1, refmv[1].mvx, refmv[1].mvy, costRef1, bitsRef1);
                    }

                    sadComp = satds[COMP_VAR0][MODES_IDX[m]];
                    bitsComp = m_mvRefs.refFrameBitCount[COMP_VAR0];
                    bitsComp += interModeBitsAV1[COMP_VAR0][m == 0 ? NEARESTMV_ : NEARMV_];
                    bitsComp += numNearMvDrlBits << 9;
                    costComp = RD32(sadComp, bitsComp, m_rdLambdaSqrtInt32);
                    if (m_currFrame->m_poc == 1) debug_printf("(%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n",
                        depth, miRow, miCol, 2, m, refmv[0].mvx, refmv[0].mvy, refmv[1].mvx, refmv[1].mvy, costComp, bitsComp);

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
                ref = REFS[r];
                if (!m_currFrame->isUniq[ref])
                    continue;
                for (int m = 0; m < 4; m++) {
                    mode_idx = MODES_IDX[m];
                    if (satds[ref][mode_idx] == INT_MAX)
                        continue;
                    if (checkedMv[ref][m])
                        continue;
                    AV1MVPair mv = (m < 2) ? m_mvRefs.mvs[ref][m] : m_mvRefs.mvRefsAV1.refMvStack[ref][m].mv;
                    if (mv.asInt64 == 0)
                        continue;

                    const int maxNumDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[ref] - 2);
                    const int numDrlBits = std::min(maxNumDrlBits, m);
                    bits = m_mvRefs.refFrameBitCount[ref]
                        + interModeBitsAV1[ref][m == 0 ? NEARESTMV_ : NEARMV_]
                        + (numDrlBits << 9);

                    cost_ = RD32(satds[ref][mode_idx], bits, m_rdLambdaSqrtInt32);
                    if (m_currFrame->m_poc == 1) debug_printf("(%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n",
                        depth, miRow, miCol, r, m, mv[0].mvx, mv[0].mvy, cost_, bits);
                    if (bestInterCost > cost_) {
                        bestInterCost = cost_;
                        bestInterSatd = satds[ref][mode_idx];
                        bestInterBits = bits;
                        bestRefComb = ref;
                        bestRef[0] = (r == 1) ? secondRef : LAST_FRAME;
                        bestRef[1] = (r == 2) ? ALTREF_FRAME : NONE_FRAME;
                        bestMode = AV1_NEARESTMV;
                        bestDrlIdx = (m == 0) ? 0 : m - 1;
                        bestMvs = mv;
                        bestPred = preds[bestRefComb][mode_idx];
                    }
                }
            }
        }
        else {
            const int REFS[3] = { LAST_FRAME, secondRef, COMP_VAR0 };
            const int MODES_IDX[4] = { NEARESTMV_, NEARMV_, NEARMV1_, NEARMV2_ };

            for (int r = 0; r < (2 + m_currFrame->compoundReferenceAllowed); r++) {
                ref = REFS[r];
                if (ref != COMP_VAR0 && !m_currFrame->isUniq[ref])
                    continue;
                maxNumNearMvDrlBits = Saturate(0, 2, m_mvRefs.mvRefsAV1.refMvCount[ref] - 2);
                for (int m = 0; m < 4; m++) {
                    mode_idx = MODES_IDX[m];
                    if (satds[ref][mode_idx] == INT_MAX)
                        continue;
                    AV1MVPair mv = (m < 2) ? m_mvRefs.mvs[ref][m] : m_mvRefs.mvRefsAV1.refMvStack[ref][m].mv;
                    if (mv.asInt64 == 0)
                        continue;
                    numNearMvDrlBits = std::min(maxNumNearMvDrlBits, m);
                    bits = m_mvRefs.refFrameBitCount[ref];
                    bits += interModeBitsAV1[ref][m == 0 ? NEARESTMV_ : NEARMV_];
                    bits += numNearMvDrlBits << 9;
                    cost_ = RD32(satds[ref][mode_idx], bits, m_rdLambdaSqrtInt32);
                    if (m_currFrame->m_poc == 1)
                        if (r == 2)
                            debug_printf("(%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d),(%d,%d) cost=%u bits=%u\n", depth,
                                miRow, miCol, r, m, mv[0].mvx, mv[0].mvy, mv[1].mvx, mv[1].mvy, cost_, bits);
                        else
                            debug_printf("(%d,%2d,%2d) REFMV ref=%d mode=%d mv=(%d,%d) cost=%u bits=%u\n", depth,
                                miRow, miCol, r, m, mv[0].mvx, mv[0].mvy, cost_, bits);

                    if (bestInterCost > cost_) {
                        bestInterCost = cost_;
                        bestInterBits = bits;
                        bestRefComb = ref;
                        bestRef[0] = (r == 1) ? secondRef : LAST_FRAME;
                        bestRef[1] = (r == 2) ? ALTREF_FRAME : NONE_FRAME;
                        bestMode = AV1_NEARESTMV;
                        bestDrlIdx = (m == 0) ? 0 : m - 1;
                        bestMvs = mv;
                        bestPred = preds[bestRefComb][mode_idx];
                    }
                }
            }
        }

        int32_t bestZeroRef = LAST_FRAME;
        int32_t bestZeroCost = satds[LAST_FRAME][ZEROMV_];
        if (bestZeroCost > satds[secondRef][ZEROMV_]) {
            bestZeroCost = satds[secondRef][ZEROMV_];
            bestZeroRef = secondRef;
        }
        if (m_currFrame->compoundReferenceAllowed && bestZeroCost > satds[COMP_VAR0][ZEROMV_]) {
            bestZeroCost = satds[COMP_VAR0][ZEROMV_];
            bestZeroRef = COMP_VAR0;
        }
        uint32_t zeroBits = m_mvRefs.refFrameBitCount[bestZeroRef] + interModeBitsAV1[bestZeroRef][ZEROMV_];
        uint32_t costZ = RD32(bestZeroCost, zeroBits, m_rdLambdaSqrtInt32);
        if (m_currFrame->m_poc == 1) debug_printf("(%d,%2d,%2d) ZEROMV ref=%d cost=%u bits=%u\n", depth, miRow, miCol, (bestZeroRef + 1) >> 1, costZ, zeroBits);
        if (bestInterCost > costZ) {
            bestInterCost = costZ;
            bestInterSatd = bestZeroCost;
            bestInterBits = zeroBits;
            bestRefComb = bestZeroRef;
            bestRef[0] = (bestZeroRef == COMP_VAR0) ? LAST_FRAME : bestZeroRef;
            bestRef[1] = (bestZeroRef == COMP_VAR0) ? ALTREF_FRAME : NONE_FRAME;
            bestMode = AV1_ZEROMV;
            bestDrlIdx = 0;
            bestMvs.asInt64 = 0;
            bestPred = preds[bestRefComb][ZEROMV_];
        }

#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

        const int32_t ctxInterp = GetCtxInterpBothAV1Fast(above, left, bestRef);
        bestInterBits += bc.interpFilterAV1[ctxInterp & 15][DEF_FILTER];
        bestInterBits += bc.interpFilterAV1[ctxInterp >> 8][DEF_FILTER];
        bestInterBits += isInterBits[1];

        m_bestInterp[depth][miRow & 7][miCol & 7] = bestPred;
        //m_bestInterSatd[depth][miRow & 7][miCol & 7] = bestInterSatd;

        // skip cost
        int32_t sseZ = AV1PP::sse_p64_p64(srcPic, bestPred, log2w, log2w);
        CostType bestCost = CostType(sseZ + uint32_t(m_rdLambda * (bestInterBits + skipBits[1]) + 0.5f));
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
        AV1PP::diff_nxn(srcPic, bestPred, diff, log2w);
        QuantParam qparamAlt = qparam;
        qparamAlt.round[0] = (MIN(40, qparam.round[0]) * qparam.dequant[0]) >> 7;
        qparamAlt.round[1] = (MIN(40, qparam.round[1]) * qparam.dequant[1]) >> 7;
        rd = TransformInterYSbAv1_viaCoeffDecision(bsz, TX_8X8, anz, lnz, srcPic, rec, diff, coef, qcoef, qparamAlt, bc,
            m_par->cuSplit == 2 ? 0 : 1, m_currFrame->m_sliceQpY);

        // decide between skip/nonskip
        //CostType cost = rd.sse + uint32_t(m_rdLambda * (bestInterBits + skipBits[0] + txSizeBits[maxTxSize] + txTypeBits[DCT_DCT] + rd.coefBits) + 0.5f);
        // Adjust cost for better skip decision
        CostType cost = CostType(rd.sse + uint32_t(m_rdLambda * (bestInterBits + skipBits[0] + txSizeBits[maxTxSize] + txTypeBits[DCT_DCT] + ((rd.coefBits * 3) >> 2)) + 0.5f));

        if (m_currFrame->m_poc == 1) debug_printf("(%d,%2d,%2d) skip_cost=%u skip_bits=%u\n", depth, miRow, miCol, (uint32_t)bestCost, bestInterBits + skipBits[1]);
        if (m_currFrame->m_poc == 1) debug_printf("(%d,%2d,%2d) non_skip_cost=%u non_skip_bits=%u\n", depth, miRow, miCol, (uint32_t)cost, bestInterBits + skipBits[0] + txSizeBits[maxTxSize] + txTypeBits[DCT_DCT] + rd.coefBits);

        if (cost < bestCost) {
            //bestCost = cost;
            bestCost = CostType(rd.sse + uint32_t(m_rdLambda * (bestInterBits + skipBits[0] + txSizeBits[maxTxSize] + txTypeBits[DCT_DCT] + rd.coefBits) + 0.5f));
            bestSkip = 0;
            bestEob = rd.eob;
        }
        else {
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
        mi->skip = (uint8_t)bestSkip;
        mi->txSize = maxTxSize;
        mi->mode = bestMode;
        mi->refMvIdx = bestDrlIdx;

        m_costCurr += bestCost;
    }

    template <typename PixType>
    void AV1CU<PixType>::MeCu(int32_t absPartIdx, int32_t depth, PartitionType partition)
    {
        RdCost bestRd = { INT_MAX, INT_MAX, UINT_MAX, UINT_MAX };
        CostType bestCost = COST_MAX;
        AV1MV bestMv0 = MV_ZERO;
        AV1MV bestMv1 = MV_ZERO;
        RefIdx bestRef0 = NONE_FRAME;
        RefIdx bestRef1 = NONE_FRAME;
        int32_t bestInterp = DEF_FILTER;
        PredMode bestMode = NEARESTMV;
        TxSize bestTxSize = TX_4X4;
        int32_t bestSkip = 0;
        int32_t bestEob = 0;

        const BlockSize bsz = GetSbType(depth, partition);
        const BlockSize bszUv = ss_size_lookup[MAX(BLOCK_8X8, bsz)][m_par->subsamplingX][m_par->subsamplingY];
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t sbx = (av1_scan_z2r4[absPartIdx] & 15) << 2;
        const int32_t sby = (av1_scan_z2r4[absPartIdx] >> 4) << 2;
        const int32_t sbw = block_size_wide[bsz];
        const int32_t sbh = block_size_high[bsz];
        const int32_t uvsbx = sbx >> 1;
        const int32_t uvsby = sby >> 1;
        const int32_t uvsbw = sbw >> 1;
        const int32_t uvsbh = sbh >> 1;
        const int32_t refYColocOffset = m_ctbPelX + sbx + (m_ctbPelY + sby) * m_pitchRecLuma;
        const int32_t refUvColocOffset = m_ctbPelX + sbx + ((m_ctbPelY + sby) >> 1) * m_pitchRecChroma;
        const int32_t numFilters = m_par->switchInterpFilter ? SWITCHABLE_FILTERS : 1;

        const TxSize maxTxSize = max_txsize_lookup[bsz];
        const TxSize minTxSize = std::max((TxSize)TX_4X4, TxSize(maxTxSize - m_par->maxTxDepthInter + 1));
        const int32_t sseShiftY = m_par->bitDepthLumaShift << 1;
        const int32_t sseShiftUv = m_par->bitDepthChromaShift << 1;

        const PixType *srcY = m_ySrc + sbx + sby * SRC_PITCH;
        const PixType *srcUv = m_uvSrc + (uvsbx << 1) + uvsby * SRC_PITCH;

        const int32_t miRow = (m_ctbPelY + sby) >> 3;
        const int32_t miCol = (m_ctbPelX + sbx) >> 3;
        GetMvRefsOld(bsz, miRow, miCol, &m_mvRefs);

        PixType *bestRecY = vp9scratchpad.recY[1];
        PixType *bestRecUv = vp9scratchpad.recUv[1];
        int16_t *bestQcoefY = vp9scratchpad.qcoefY[1];
        int16_t *bestQcoefU = vp9scratchpad.qcoefU[1];
        int16_t *bestQcoefV = vp9scratchpad.qcoefV[1];

        PixType *predY = vp9scratchpad.predY[0];
        PixType *predUv = vp9scratchpad.predUv[0];
        PixType *recY = vp9scratchpad.recY[0];
        PixType *recUv = vp9scratchpad.recUv[0];
        int16_t *diffY = vp9scratchpad.diffY;
        int16_t *diffU = vp9scratchpad.diffU;
        int16_t *diffV = vp9scratchpad.diffV;
        int16_t *coefY = vp9scratchpad.coefY;
        int16_t *coefU = vp9scratchpad.coefU;
        int16_t *coefV = vp9scratchpad.coefV;
        int16_t *qcoefY = vp9scratchpad.qcoefY[0];
        int16_t *qcoefU = vp9scratchpad.qcoefU[0];
        int16_t *qcoefV = vp9scratchpad.qcoefV[0];

        Contexts ctx;
        Contexts bestCtx;
        Contexts ctxAfterSkip = m_contexts;
        memset(ctxAfterSkip.aboveNonzero[0] + (sbx >> 2), 0, sbw >> 2);
        memset(ctxAfterSkip.aboveNonzero[1] + (uvsbx >> 2), 0, uvsbw >> 2);
        memset(ctxAfterSkip.aboveNonzero[2] + (uvsbx >> 2), 0, uvsbw >> 2);
        memset(ctxAfterSkip.leftNonzero[0] + (sby >> 2), 0, sbh >> 2);
        memset(ctxAfterSkip.leftNonzero[1] + (uvsby >> 2), 0, uvsbh >> 2);
        memset(ctxAfterSkip.leftNonzero[2] + (uvsby >> 2), 0, uvsbh >> 2);

        uint8_t *anz[3] = { ctx.aboveNonzero[0] + (sbx >> 2), ctx.aboveNonzero[1] + (uvsbx >> 2), ctx.aboveNonzero[2] + (uvsbx >> 2) };
        uint8_t *lnz[3] = { ctx.leftNonzero[0] + (sby >> 2), ctx.leftNonzero[1] + (uvsby >> 2), ctx.leftNonzero[2] + (uvsby >> 2) };

        ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
        const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
        const ModeInfo *left = GetLeft(mi, miCol, m_tileBorders.colStart);
        const QuantParam &qparamY = m_aqparamY;
        const QuantParam(&qparamUv)[2] = m_aqparamUv;
        const BitCounts &bc = *m_currFrame->bitCount;
        const int32_t ctxSkip = GetCtxSkip(above, left);
        const int32_t ctxInterp = GetCtxInterpVP9(above, left);
        const int32_t ctxIsInter = GetCtxIsInter(above, left);
        const int32_t ctxTxSize = GetCtxTxSizeVP9(above, left, maxTxSize);
        const uint16_t *txSizeBits = bc.txSize[maxTxSize][ctxTxSize];
        const uint16_t *skipBits = bc.skip[ctxSkip];
        const uint16_t *interpFilterBits = nullptr;// bc.interpFilterVP9[ctxInterp];
        const uint16_t *isInterBits = bc.isInter[ctxIsInter];
        const uint16_t *interModeBits = bc.interMode[m_mvRefs.memCtx.interMode];

        //EstimateRefFramesAllVp9(bc, *m_currFrame, above, left, m_mvRefs.refFrameBitCount, &m_mvRefs.memCtx);
        m_mvRefs.memCtx.skip = ctxSkip;
        m_mvRefs.memCtx.isInter = ctxIsInter;
        m_mvRefs.memCtx.interp = ctxInterp;
        m_mvRefs.memCtx.txSize = ctxTxSize;

        for (int32_t i = 0; i <= m_mvRefs.numMvRefs; i++) {
            int8_t refIdx[2];
            AV1MVPair mv;
            PredMode predMode;
            if (i < m_mvRefs.numMvRefs) {
                refIdx[0] = m_mvRefs.refIdx[i][0];
                refIdx[1] = m_mvRefs.refIdx[i][1];
                mv[0] = m_mvRefs.mv[i][0];
                mv[1] = m_mvRefs.mv[i][1];
                predMode = m_mvRefs.mode[i];
                if (m_par->m_framesInParallel > 1 && (
                    !CheckSearchRange(sby, sbh, mv[0], m_par->m_meSearchRangeY) ||
                    !CheckSearchRange(sby, sbh, mv[1], m_par->m_meSearchRangeY)))
                    continue;
            }
            else {
                AV1MEInfo meInfo = {};
                meInfo.depth = (uint8_t)depth;
                meInfo.width = sbw;
                meInfo.height = sbh;
                meInfo.posx = sbx;
                meInfo.posy = sby;
                MePu(&meInfo);
                if (meInfo.satd == INT_MAX)
                    continue;
                ClipMV_NR(&meInfo.mv[0]);
                ClipMV_NR(&meInfo.mv[1]);
                refIdx[0] = meInfo.refIdx[0];
                refIdx[1] = meInfo.refIdx[1];
                mv.asInt64 = meInfo.mv.asInt64;
                predMode = NEWMV;
                if (HasSameMotion(mv, m_mvRefs.mv, refIdx, m_mvRefs.refIdx, m_mvRefs.numMvRefs))
                    continue;
            }

            const PixType *ref0Y = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[refIdx[0]]->m_recon->y) + refYColocOffset;
            const PixType *ref0Uv = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[refIdx[0]]->m_recon->uv) + refUvColocOffset;
            const PixType *ref1Y = NULL;
            const PixType *ref1Uv = NULL;
            if (refIdx[1] != NONE_FRAME) {
                ref1Y = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[refIdx[1]]->m_recon->y) + refYColocOffset;
                ref1Uv = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[refIdx[1]]->m_recon->uv) + refUvColocOffset;
            }

            for (int32_t interp = EIGHTTAP; interp < numFilters; interp++) {
                const int32_t interpCacheIdx = (refIdx[1] == NONE_FRAME) ? refIdx[0] : (COMP_VAR0 + (refIdx[1] == m_currFrame->compVarRef[1]));
                const uint32_t refFrameBits = m_mvRefs.refFrameBitCount[interpCacheIdx];

                AV1MVPair clippedMv = { mv[0], mv[1] };
                ClipMV_NR(clippedMv + 0);
                ClipMV_NR(clippedMv + 1);

                if (refIdx[1] == NONE_FRAME) {
                    InterpolateAv1SingleRefLuma(ref0Y, m_pitchRecLuma, predY, clippedMv[0], sbh, log2w, interp);
                    if (m_par->chromaRDO)
                        InterpolateAv1SingleRefChroma(ref0Uv, m_pitchRecLuma, predUv, clippedMv[0], uvsbh, log2w - 1, interp);
                }
                else {
                    InterpolateAv1FirstRefLuma(ref0Y, m_pitchRecLuma, vp9scratchpad.compPredIm, clippedMv[0], sbh, log2w, interp);
                    InterpolateAv1SecondRefLuma(ref1Y, m_pitchRecLuma, vp9scratchpad.compPredIm, predY, clippedMv[1], sbh, log2w, interp);
                    if (m_par->chromaRDO) {
                        InterpolateAv1FirstRefChroma(ref0Uv, m_pitchRecLuma, vp9scratchpad.compPredIm, clippedMv[0], uvsbh, log2w - 1, interp);
                        InterpolateAv1SecondRefChroma(ref1Uv, m_pitchRecLuma, vp9scratchpad.compPredIm, predUv, clippedMv[1], uvsbh, log2w - 1, interp);
                    }
                }

                CopyNxM(predY, BUF_PITCH, recY, sbw, sbh);
                CopyNxM(predUv, BUF_PITCH, recUv, uvsbw << 1, uvsbh);
                int32_t sseY = AV1PP::sse_p64_p64(srcY, predY, log2w, log2h);
                int32_t sseUv = 0;
                if (m_par->chromaRDO)
                    sseUv = AV1PP::sse_p64_p64(srcUv, predUv, log2w, log2h - 1);
                uint32_t modeBits = refFrameBits + interpFilterBits[interp] + interModeBits[predMode - NEARESTMV];
                const int32_t refIdxComb = GetRefIdxComb(refIdx, m_currFrame->compVarRef[1]);
                if (predMode == NEWMV) {
                    modeBits += EstimateMv(bc, mv[0], m_mvRefs.bestMv[refIdxComb][0], m_mvRefs.useMvHp[refIdx[0]]);
                    if (refIdx[1] > NONE_FRAME)
                        modeBits += EstimateMv(bc, mv[1], m_mvRefs.bestMv[refIdxComb][1], m_mvRefs.useMvHp[refIdx[1]]);
                }

                CostType cost = sseY + sseUv + m_rdLambda * (modeBits + skipBits[1]);
                fprintf_trace_cost(stderr, "inter: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d ref=%d,%d mv=%d,%d mv1=%d,%d skip=1      "
                    "sseY=%6d sseUv=%5d modeBits=%4u cost=%.1f\n", m_currFrame->m_frameOrder, m_ctbAddr, absPartIdx, bsz, predMode,
                    refIdx[0], refIdx[1], mv[0].mvx, mv[0].mvy, mv[1].mvx, mv[1].mvy, sseY, sseUv, modeBits + skipBits[1], cost);
                if (bestCost > cost) {
                    bestCost = cost;
                    bestMv0 = mv[0];
                    bestMv1 = mv[1];
                    bestRef0 = refIdx[0];
                    bestRef1 = refIdx[1];
                    bestInterp = interp;
                    bestMode = predMode;
                    bestTxSize = maxTxSize;
                    bestSkip = 1;
                    bestEob = 0;
                    bestCtx = ctxAfterSkip;
                    std::swap(bestRecY, recY);
                    std::swap(bestRecUv, recUv);
                }

                AV1PP::diff_nxm(srcY, predY, diffY, sbh, log2w);
                if (m_par->chromaRDO)
                    AV1PP::diff_nv12(srcUv, predUv, diffU, diffV, uvsbh, log2w - 1);

                for (int32_t txSize = maxTxSize; txSize >= minTxSize; txSize--) {
                    TxSize txSizeUv = MIN(max_txsize_lookup[bszUv], (TxSize)txSize);
                    ctx = m_contexts;
                    CopyNxM(predY, BUF_PITCH, recY, sbw, sbh);
                    CopyNxM(predUv, BUF_PITCH, recUv, uvsbw << 1, uvsbh);

                    RdCost rdcostY = TransformInterYSb(bsz, (TxSize)txSize, anz[0], lnz[0], srcY, recY, diffY,
                        coefY, qcoefY, qparamY, bc, m_par->FastCoeffCost);
                    RdCost rdcostUv = {};
                    if (m_par->chromaRDO)
                        rdcostUv = TransformInterUvSb(bszUv, txSizeUv, anz[1], anz[2], lnz[1], lnz[2], srcUv,
                            recUv, uvsbw << 1, diffU, diffV, coefU, coefV, qcoefU,
                            qcoefV, qparamUv, bc, m_par->FastCoeffCost);

                    uint32_t bits = modeBits + txSizeBits[txSize] + skipBits[0] + rdcostY.coefBits + rdcostUv.coefBits;
                    cost = rdcostY.sse + rdcostUv.sse + m_rdLambda * bits;
                    fprintf_trace_cost(stderr, "inter: poc=%d ctb=%2d idx=%3d bsz=%d mode=%d ref=%d,%d mv=%d,%d mv1=%d,%d skip=0 tx=%d "
                        "sseY=%6d sseUv=%5d modeBits=%4u cost=%6.0f coefY=%u coefUv=%u eobY=%d eobUv=%d\n", m_currFrame->m_frameOrder,
                        m_ctbAddr, absPartIdx, bsz, predMode, refIdx[0], refIdx[1], mv[0].mvx, mv[0].mvy, mv[1].mvx, mv[1].mvy, txSize,
                        rdcostY.sse, rdcostUv.sse, modeBits + txSizeBits[txSize] + skipBits[0], cost, rdcostY.coefBits, rdcostUv.coefBits,
                        rdcostY.eob, rdcostUv.eob);
                    if (bestCost > cost) {
                        bestCost = cost;
                        bestMv0 = mv[0];
                        bestMv1 = mv[1];
                        bestRef0 = refIdx[0];
                        bestRef1 = refIdx[1];
                        bestInterp = interp;
                        bestMode = predMode;
                        bestTxSize = (TxSize)txSize;
                        bestSkip = 0;
                        bestEob = rdcostY.eob + rdcostUv.eob;
                        bestCtx = ctx;
                        std::swap(bestRecY, recY);
                        std::swap(bestRecUv, recUv);
                        std::swap(bestQcoefY, qcoefY);
                        std::swap(bestQcoefU, qcoefU);
                        std::swap(bestQcoefV, qcoefV);
                    }
                }
            }
        }

        fprintf_trace_cost(stderr, "BEST: mode=%d ref=%d,%d mv=%d,%d mv1=%d,%d skip=%d tx=%d eob=%d cost=%6.0f\n\n", bestMode,
            bestRef0, bestRef1, bestMv0.mvx, bestMv0.mvy, bestMv1.mvx, bestMv1.mvy, bestSkip, bestTxSize, bestEob, bestCost);

        const RefIdx bestRefIdxComb = GetRefIdxComb(bestRef0, bestRef1, m_currFrame->compVarRef[1]);
        int32_t num4x4 = num_4x4_blocks_lookup[bsz];
        Zero(*mi);
        mi->mv[0] = bestMv0;
        mi->mv[1] = bestMv1;
        mi->mvd[0].mvx = bestMv0.mvx - m_mvRefs.bestMv[bestRefIdxComb][0].mvx;
        mi->mvd[0].mvy = bestMv0.mvy - m_mvRefs.bestMv[bestRefIdxComb][0].mvy;
        mi->mvd[1].mvx = bestMv1.mvx - m_mvRefs.bestMv[bestRefIdxComb][1].mvx;
        mi->mvd[1].mvy = bestMv1.mvy - m_mvRefs.bestMv[bestRefIdxComb][1].mvy;
        mi->memCtx = m_mvRefs.memCtx;
        mi->refIdx[0] = bestRef0;
        mi->refIdx[1] = bestRef1;
        mi->refIdxComb = bestRefIdxComb;
        mi->sbType = bsz;
        mi->skip = uint8_t(bestSkip | ((bestEob == 0) ? 2 : 0));
        mi->txSize = bestTxSize;
        mi->mode = bestMode;
        mi->interp = (InterpFilter)bestInterp;
        PropagateSubPart(mi, m_par->miPitch, sbw >> 3, sbh >> 3);

        recY = m_yRec + sbx + sby * m_pitchRecLuma;
        recUv = m_uvRec + uvsbx * 2 + uvsby * m_pitchRecChroma;
        qcoefY = m_coeffWorkY + absPartIdx * 16;
        qcoefU = m_coeffWorkU + absPartIdx * 4;
        qcoefV = m_coeffWorkV + absPartIdx * 4;
        CopyNxM(bestRecY, sbw, recY, m_pitchRecLuma, sbw, sbh);
        if (partition == PARTITION_VERT) {
            int32_t halfY = num4x4 * 8;
            CopyCoeffs(bestQcoefY, qcoefY, halfY);
            CopyCoeffs(bestQcoefY + 2 * halfY, qcoefY + 2 * halfY, halfY);
        }
        else {
            CopyCoeffs(bestQcoefY, qcoefY, num4x4 * 16);
        }

        if (m_par->chromaRDO) {
            CopyNxM(bestRecUv, uvsbw << 1, recUv, m_pitchRecChroma, uvsbw << 1, uvsbh);
            if (partition == PARTITION_VERT) {
                int32_t halfUv = num4x4 * 2;
                CopyCoeffs(bestQcoefU, qcoefU, halfUv);
                CopyCoeffs(bestQcoefU + 2 * halfUv, qcoefU + 2 * halfUv, halfUv);
                CopyCoeffs(bestQcoefV, qcoefV, halfUv);
                CopyCoeffs(bestQcoefV + 2 * halfUv, qcoefV + 2 * halfUv, halfUv);
            }
            else {
                CopyCoeffs(bestQcoefU, qcoefU, num4x4 * 4);
                CopyCoeffs(bestQcoefV, qcoefV, num4x4 * 4);
            }
        }

        m_contexts = bestCtx;

        bestCost += m_rdLambda * isInterBits[1];
        m_costCurr += bestCost;
    }

    template <typename PixType>
    int16_t AV1CU<PixType>::MeIntSeed(const AV1MEInfo *meInfo, const AV1MV &mvp, int32_t list,
        int32_t refIdx, AV1MV mvRefBest0, AV1MV mvRefBest00,
        AV1MV *mvBest, int32_t *costBest, int32_t *mvCostBest, AV1MV *mvBestSub)
    {
        int32_t costOrig = *costBest;
        int16_t meStepMax = int16_t(MAX(MIN(meInfo->width, meInfo->height), 16) * 4);
        int32_t size = BSR(meInfo->width | meInfo->height) - 3; assert(meInfo->width <= (8 << size) && meInfo->height <= (8 << size));
        PixType *src = m_ySrc + meInfo->posx + meInfo->posy * SRC_PITCH;
        // FrameData *ref = m_currFrame->m_refPicList[list].m_refFrames[refIdx]->m_recon;
        FrameData *ref = m_currFrame->m_dpbVP9[m_currFrame->refFrameIdx[refIdx]]->m_recon;
        int32_t useHadamard = (m_par->hadamardMe == 3);
        // Helps ME quality, memoization & reduced dyn range helps speed.
        AV1MV mvSeed;

        int32_t mvCostMax = (int32_t)(MvCost(meStepMax) * m_rdLambdaSqrt * 512 + 0.5f);
        mvCostMax *= 2;

        int32_t uniqRefIdx = refIdx;
        assert(uniqRefIdx >= 0);
        if (MemBestMV(size, uniqRefIdx, &mvSeed)) {
            AV1MV mvSeedInt = mvSeed;
            if ((mvSeed.mvx | mvSeed.mvy) & 7) {
                mvSeedInt.mvx = (mvSeed.mvx + (1 << 1)) & ~7;
                mvSeedInt.mvy = (mvSeed.mvy + (1 << 1)) & ~7;
            }

            bool checkSeed = true;
            if (m_par->m_framesInParallel > 1 && !CheckFrameThreadingSearchRange(meInfo, &mvSeedInt))
                checkSeed = false;

            if (checkSeed && mvSeedInt != *mvBest) {
                int32_t cost = MatchingMetricPu(src, meInfo, &mvSeedInt, ref, useHadamard);
                if (cost < *costBest) {
                    int32_t mvCostSeed = MvCost1RefLog(mvSeedInt, mvp, m_rdLambdaSqrt);
                    if (*costBest > cost + mvCostSeed) {
                        *costBest = cost + mvCostSeed;
                        *mvBest = mvSeedInt;
                        *mvBestSub = mvSeed;
                        *mvCostBest = mvCostSeed;
                    }
                }
            }
        }

#if 0
        if (refIdx != NONE_FRAME) {
            int32_t zeroPocDiff = m_currFrame->m_refPicList[list].m_deltaPoc[0];
            int32_t currPocDiff = m_currFrame->m_refPicList[list].m_deltaPoc[refIdx];
            int32_t scale = GetDistScaleFactor(currPocDiff, zeroPocDiff);
            AV1MV mvProj = mvRefBest0;
            mvProj.mvx = (int16_t)Saturate(-32768, 32767, (scale * mvProj.mvx + 127 + (scale * mvProj.mvx < 0)) >> 8);
            mvProj.mvy = (int16_t)Saturate(-32768, 32767, (scale * mvProj.mvy + 127 + (scale * mvProj.mvy < 0)) >> 8);

            ClipMV_NR(&mvProj);
            bool checkProj = true;
            AV1MV mvSeedInt = mvProj;
            if ((mvProj.mvx | mvProj.mvy) & 7) {
                mvSeedInt.mvx = (mvProj.mvx + 1) & ~7;
                mvSeedInt.mvy = (mvProj.mvy + 1) & ~7;
            }

            if (m_par->m_framesInParallel > 1 && !CheckFrameThreadingSearchRange(meInfo, &mvSeedInt))
                checkProj = false;

            if (checkProj && mvSeedInt != *mvBest) {
                int32_t hadFoundSize = size;
                int32_t cost = MatchingMetricPu(src, meInfo, &mvSeedInt, ref, useHadamard);
                if (cost < *costBest) {
                    int32_t mvCostSeed = MvCost1RefLog(mvSeedInt, amvp, m_rdLambdaSqrt);
                    if (*costBest > cost + mvCostSeed) {
                        *costBest = cost + mvCostSeed;
                        *mvBest = mvSeedInt;
                        *mvBestSub = mvProj;
                        *mvCostBest = mvCostSeed;
                    }
                }
            }
        }
#endif

#if 0
        if (list) {
            int32_t zeroPocDiff = m_currFrame->m_refPicList[0].m_deltaPoc[0];
            int32_t currPocDiff = m_currFrame->m_refPicList[list].m_deltaPoc[refIdx];
            int32_t scale = GetDistScaleFactor(currPocDiff, zeroPocDiff);
            AV1MV mvProj = mvRefBest00;

            mvProj.mvx = (int16_t)Saturate(-32768, 32767, (scale * mvProj.mvx + 127 + (scale * mvProj.mvx < 0)) >> 8);
            mvProj.mvy = (int16_t)Saturate(-32768, 32767, (scale * mvProj.mvy + 127 + (scale * mvProj.mvy < 0)) >> 8);
            ClipMV_NR(&mvProj);
            bool checkProj = true;

            AV1MV mvSeedInt = mvProj;
            if ((mvProj.mvx | mvProj.mvy) & 7) {
                mvSeedInt.mvx = (mvProj.mvx + 1) & ~7;
                mvSeedInt.mvy = (mvProj.mvy + 1) & ~7;
            }

            if (m_par->m_framesInParallel > 1 && !CheckFrameThreadingSearchRange(meInfo, &mvSeedInt))
                checkProj = false;

            if (checkProj && mvSeedInt != *mvBest) {
                int32_t hadFoundSize = size;
                int32_t cost = MatchingMetricPu(src, meInfo, &mvSeedInt, ref, useHadamard);
                if (cost < *costBest) {
                    int32_t mvCostSeed = MvCost1RefLog(mvSeedInt, amvp, m_rdLambdaSqrt);
                    if (*costBest > cost + mvCostSeed) {
                        *costBest = cost + mvCostSeed;
                        *mvBest = mvSeedInt;
                        *mvBestSub = mvProj;
                        *mvCostBest = mvCostSeed;
                    }
                }
            }
        }
#endif

        // Range by seed
        if (mvCostMax > *mvCostBest && *costBest - *mvCostBest + mvCostMax < costOrig) {
            meStepMax /= 2;
            mvCostMax = (int32_t)(MvCost(meStepMax) * m_rdLambdaSqrt * 512 + 0.5f);
            mvCostMax *= 2;
        }
        // Range by cost
        if ((*costBest - *mvCostBest) < mvCostMax) {
            int32_t stepCost = mvCostMax;
            int16_t stepMax = meStepMax;
            while (stepCost > (*costBest - *mvCostBest) && stepCost > *mvCostBest && stepMax > 4) {
                stepMax /= 2;
                stepCost = (int32_t)(MvCost(stepMax) * m_rdLambdaSqrt * 512 + 0.5f);
                stepCost *= 2;
            }
            meStepMax = stepMax;
        }

        return meStepMax;
    }


    const int16_t tab_mePattern[1 + 8 + 6][2] = {
        {0, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1},
        {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}
    };

    const struct TransitionTable {
        uint8_t start;
        uint8_t len;
    } tab_meTransition[1 + 8 + 6] = {
        {1, 8}, {7, 5}, {1, 3}, {1, 5}, {3, 3}, {3, 5}, {5, 3}, {5, 5},
        {7, 3}, {7, 5}, {1, 3}, {1, 5}, {3, 3}, {3, 5}, {5, 3}
    };

    const struct TransitionTableStart {
        uint8_t start;
        uint8_t len;
    } tab_meTransitionStart[1 + 8] = {
        {1, 8}, {6, 7}, {8, 5}, {8, 7}, {2, 5}, {2, 7}, {4, 5}, {4, 7}, {6, 5}
    };

    const struct TransitionTableStart2 {
        uint8_t start;
        uint8_t len;
    } tab_meTransitionStart2[1 + 8] = {
        {1, 8}, {8, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3}, {7, 3}
    };

    const int16_t mePosToLoc[1 + 8 + 6] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6
    };

    const int16_t refineTransition[9][8] = {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 1, 1, 1, 0, 0},
        {0, 0, 0, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 0, 1, 1, 1},
        {1, 1, 0, 0, 0, 1, 1, 1},
        {1, 1, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 0, 0, 0, 1},
        {0, 1, 1, 1, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 1, 0, 0}
    };


    template <typename PixType>
    void AV1CU<PixType>::MeIntPel(const AV1MEInfo *meInfo, const AV1MV &mvp,
        const FrameData *ref, AV1MV *mv, int32_t *cost, int32_t *mvCost, int32_t meStepRange)
    {
        if (m_par->patternIntPel == 1) {
            return MeIntPelLog(meInfo, mvp, ref, mv, cost, mvCost, meStepRange);
        }
        else if (m_par->patternIntPel == 100) {  // not modified for VPX yet
            assert(0);
            //return MeIntPelFullSearch(meInfo, mvp, ref, mv, cost, mvCost);
        }
        else {
            assert(0);
        }
    }

    int32_t ClipMV_special(AV1MV *rcMv, MvLimits& limits)
    {
        int HorMin = limits.col_min;
        int HorMax = limits.col_max;
        int VerMin = limits.row_min;
        int VerMax = limits.row_max;

        int32_t change = 0;
        if (rcMv->mvx / 8 < HorMin) {
            change = 1;
        }
        else if (rcMv->mvx / 8 > HorMax) {
            change = 1;
        }
        if (rcMv->mvy / 8 < VerMin) {
            change = 1;
        }
        else if (rcMv->mvy / 8 > VerMax) {
            change = 1;
        }
        return change;
    }
    template <typename PixType>
    void AV1CU<PixType>::MeIntPelFullSearch(const AV1MEInfo *meInfo, const AV1MV &mvp, const FrameData *ref, AV1MV *mv_, int32_t *cost_, int32_t *mvCost_, MvLimits& limits)
    {
        const int32_t RANGE = 8 * 64;

        AV1MV mvBest = *mv_;
        int32_t costBest = *cost_;
        int32_t mvCostBest = *mvCost_;
        PixType *src = m_ySrc + meInfo->posx + meInfo->posy * SRC_PITCH;
        int32_t useHadamard = (m_par->hadamardMe == 3);

        AV1MV mvCenter = mvBest;
        //for (int32_t dy = -RANGE; dy <= RANGE; dy += 8)
        for (int32_t dy = limits.row_min; dy <= limits.row_max; dy++)
        {
            //for (int32_t dx = -RANGE; dx <= RANGE; dx += 8)
            for (int32_t dx = limits.col_min; dx <= limits.col_max; dx++)
            {
                AV1MV mv = {
                    static_cast<int16_t>(dx * 8),
                    static_cast<int16_t>(dy * 8)
                };

                if (ClipMV_special(&mv, limits)) {
                    continue;
                }

                int32_t cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                if (costBest > cost) {
                    int32_t mvCost = MvCost1RefLog(mv, mvp, m_rdLambdaSqrt);
                    cost += mvCost;
                    if (costBest > cost) {
                        costBest = cost;
                        mvCostBest = mvCost;
                        mvBest = mv;
                    }
                }
            }
        }

        *mv_ = mvBest;
        *cost_ = costBest;
        *mvCost_ = mvCostBest;
    }
    template void AV1CU<uint8_t>::MeIntPelFullSearch(const AV1MEInfo *meInfo, const AV1MV &mvp, const FrameData *ref, AV1MV *mv, int32_t *cost, int32_t *mvCost, MvLimits& limits);
#if ENABLE_10BIT
    template void AV1CU<uint16_t>::MeIntPelFullSearch(const AV1MEInfo *meInfo, const AV1MV &mvp, const FrameData *ref, AV1MV *mv, int32_t *cost, int32_t *mvCost, MvLimits& limits);
#endif
    const int16_t tab_mePatternSelector[5][12][2] = {
        {{0, 0}, { 0, -1}, {1,  0}, {0,  1}, {-1, 0}, {0, 0}, {0, 0}, { 0, 0}, { 0, 0}, { 0,  0}, {0,  0}, {0,  0}},  // diamond
        {{0, 0}, {-1, -1}, {0, -1}, {1, -1}, { 1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}},  // box
        {{0, 0}, {-1, -1}, {1, -1}, {1,  1}, {-1, 1}, {0, 0}, {0, 0}, { 0, 0}, { 0, 0}, { 0,  0}, {0,  0}, {0,  0}},  // Sq
        {{0, 0}, {-1,  0}, {1,  0}, {0,  0}, { 0, 0}, {0, 0}, {0, 0}, { 0, 0}, { 0, 0}, { 0,  0}, {0,  0}, {0,  0}},  // H
        {{0, 0}, { 0, -1}, {0,  1}, {0,  0}, { 0, 0}, {0, 0}, {0, 0}, { 0, 0}, { 0, 0}, { 0,  0}, {0,  0}, {0,  0}},  // V
    };


    template <typename PixType>
    void AV1CU<PixType>::MeIntPelLog(const AV1MEInfo *meInfo, const AV1MV &mvp,
        const FrameData *ref, AV1MV *outMv, int32_t *outCost, int32_t *outMvCost, int32_t meStepRange)

    {
        // Log2(step ) is used, except meStepMax
        int16_t meStepBest = 2;
        int16_t meStepOrig = int16_t(MAX(MIN(meInfo->width, meInfo->height), 16) * 4);
        int16_t meStepMax = int16_t(MIN(meStepRange, meStepOrig));
        AV1MV mvBest = *outMv;
        int32_t costBest = *outCost;
        int32_t mvCostBest = *outMvCost;

        PixType *src = m_ySrc + meInfo->posx + meInfo->posy * SRC_PITCH;
        int32_t useHadamard = (m_par->hadamardMe == 3);
        int16_t mePosBest = 0;
        // expanding search
        AV1MV mvCenter = mvBest;
        for (int16_t meStep = meStepBest; (1 << meStep) <= meStepMax; meStep++) {
            for (int16_t mePos = 1; mePos < 9; mePos++) {
                AV1MV mv = {
                    int16_t(mvCenter.mvx + ((tab_mePattern[mePos][0] << meStep) << 1)),
                    int16_t(mvCenter.mvy + ((tab_mePattern[mePos][1] << meStep) << 1))
                };
                if (ClipMV(&mv))
                    continue;

                if (m_par->m_framesInParallel > 1 && !CheckFrameThreadingSearchRange(meInfo, &mv))
                    continue;

                int32_t cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);

                if (costBest > cost) {
                    int32_t mvCost = MvCost1RefLog(mv, mvp, m_rdLambdaSqrt);
                    cost += mvCost;
                    if (costBest > cost) {
                        costBest = cost;
                        mvCostBest = mvCost;
                        mvBest = mv;
                        meStepBest = meStep;
                        mePosBest = mePos;
                    }
                }
            }
        }

        // then logarithm from best
        // for Cpattern
        int32_t transPos = mePosBest;
        uint8_t start = 0, len = 1;
        int16_t meStep = meStepBest;
        if (meStepBest > 2) {
            start = tab_meTransitionStart[mePosBest].start;
            len = tab_meTransitionStart[mePosBest].len;
            meStep--;
        }
        else {
#ifdef NO_ME_2
            meStep = 0;
#else
            if (mvBest != *outMv) {
                start = tab_meTransitionStart2[mePosBest].start;
                len = tab_meTransitionStart2[mePosBest].len;
            }
            else {
                meStep = 0;
            }
#endif
        }

        mvCenter = mvBest;
        int32_t iterbest = costBest;
        while (meStep >= 2) {
            int32_t refine = 1;
            int32_t bestPos = 0;
            for (int16_t mePos = start; mePos < start + len; mePos++) {
                AV1MV mv = {
                    int16_t(mvCenter.mvx + ((tab_mePattern[mePos][0] << meStep) << 1)),
                    int16_t(mvCenter.mvy + ((tab_mePattern[mePos][1] << meStep) << 1))
                };
                if (ClipMV(&mv))
                    continue;

                if (m_par->m_framesInParallel > 1 && !CheckFrameThreadingSearchRange(meInfo, &mv))
                    continue;

                int32_t cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);

                if (costBest > cost) {
                    int32_t mvCost = MvCost1RefLog(mv, mvp, m_rdLambdaSqrt);
                    cost += mvCost;
                    if (costBest > cost) {
                        costBest = cost;
                        mvCostBest = mvCost;
                        mvBest = mv;
                        refine = 0;
                        bestPos = mePos;
                    }
                }
            }

            if (refine) {
                meStep--;
            }
            else {
                if (refineTransition[mePosToLoc[transPos]][mePosToLoc[bestPos] - 1]) {
                    meStep--;
                    bestPos = 0;
                }
                else if ((costBest + m_rdLambdaSqrt * 512) > iterbest) {
                    meStep--;
                    bestPos = 0;
                }
#ifndef NO_ME_2
                else if (meStep <= 4) {
                    meStep++;
                }
#endif
                mvCenter = mvBest;
            }
            start = tab_meTransition[bestPos].start;
            len = tab_meTransition[bestPos].len;
            iterbest = costBest;
            transPos = bestPos;
        }

        *outMv = mvBest;
        *outCost = costBest;
        *outMvCost = mvCostBest;
    }


    template <typename PixType>
    void AV1CU<PixType>::AddMvCost(const AV1MV &mvp, int32_t log2Step, const int32_t *dists,
        AV1MV *mv, int32_t *costBest, int32_t *mvCostBest, int32_t patternSubPel) const
    {
        const int16_t(*pattern)[2] = tab_mePatternSelector[int32_t(patternSubPel == SUBPEL_BOX)] + 1;
        int32_t numMvs = (patternSubPel == SUBPEL_BOX) ? 8 : 4;  // BOX or DIA

        AV1MV centerMv = *mv;
        for (int32_t i = 0; i < numMvs; i++) {
            int32_t cost = dists[i];
            if (*costBest > cost) {
                int16_t mvx = centerMv.mvx + (pattern[i][0] << log2Step);
                int16_t mvy = centerMv.mvy + (pattern[i][1] << log2Step);
                int32_t mvCost = MvCost1RefLog(mvx, mvy, mvp, m_rdLambdaSqrt);
                cost += mvCost;
                if (*costBest > cost) {
                    *costBest = cost;
                    *mvCostBest = mvCost;
                    mv->mvx = mvx;
                    mv->mvy = mvy;
                }
            }
        }
    }

#ifdef USE_BATCHED_FASTORTH_SEARCH
#define CompareFastCosts(costFast, bestFastCost, testPos, shift, bestPos, bestmvCost, bestmv) {   \
    if (costFast < bestFastCost) {                                                                \
        AV1MV tmpMv = {                                                                          \
            (int16_t)(mvCenter.mvx+(((int16_t)tab_mePatternSelector[1][testPos+1][0] << shift)<<1)),\
            (int16_t)(mvCenter.mvy+(((int16_t)tab_mePatternSelector[1][testPos+1][1] << shift)<<1)) \
        };                                                                                        \
        int32_t tmpMvCost = MvCost1RefLog(tmpMv.mvx, tmpMv.mvy, mvp, m_rdLambdaSqrt);              \
        costFast += tmpMvCost;                                                                    \
        if (costFast < bestFastCost) {                                                            \
            bestFastCost = costFast;                                                              \
            bestPos = testPos;                                                                    \
            bestmvCost = tmpMvCost;                                                               \
            bestmv = tmpMv;                                                                       \
        }                                                                                         \
    }                                                                                             \
}

    template <typename PixType>
    void AV1CU<PixType>::MemSubPelBatchedFastBoxDiaOrth(
        const AV1MEInfo *meInfo,
        const AV1MV &mvp,
        const FrameData *ref,
        AV1MV *mv,
        int32_t *cost,
        int32_t *mvCost,
        int32_t refIdxMem,
        int32_t size)
    {
        assert(m_par->patternSubPel == SUBPEL_FASTBOX_DIA_ORTH);

        int32_t w = meInfo->width;
        int32_t h = meInfo->height;
        int32_t log2w = BSR(w >> 2);
        int32_t log2h = BSR(h >> 2);
        int32_t bitDepth = m_par->bitDepthLuma;
        int32_t costShift = m_par->bitDepthLumaShift;
        int32_t shift = 20 - bitDepth;
        int32_t offset = 1 << (19 - bitDepth);
        int32_t useHadamard = (m_par->hadamardMe >= 2);
        int16_t *preAvgTmpBuf = m_scratchPad.interp.interpWithAvg.tmpBuf;

        int32_t(*costFunc)(const PixType*, int32_t, const PixType*, int32_t, int32_t, int32_t);
        costFunc = AV1PP::sad_general;

        int32_t pitchSrc = SRC_PITCH;
        PixType *src = m_ySrc + meInfo->posx + meInfo->posy * pitchSrc;
        int32_t pitchRef = ref->pitch_luma_pix;
        const PixType *refY = (const PixType *)ref->y;
        refY += (int32_t)(m_ctbPelX + meInfo->posx + (mv->mvx >> 3) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 3)) * pitchRef);

        const int32_t pitch[4] = {
            (MAX_CU_SIZE >> 3) + 8,
            (MAX_CU_SIZE >> 2) + 16,
            (MAX_CU_SIZE >> 1) + 32,
            (MAX_CU_SIZE >> 0) + 32
        };
        const int32_t memPitch = pitch[size];

        PixType *tmpSubMem[8];

        uint16_t *predBufHi20 = nullptr;
        PixType *predBuf20;
        AV1MV tmv;
        tmv = *mv;
        tmv.mvx += (2 << 1);
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBuf20, (int16_t**)&predBufHi20);

#if 0
        // intermediate halfpels Hi[2,0]: change (-1, -4) (w+8, h+8) to (-4,-4) (w+8, h+8)
        // (intermediate, save to predBufHi[2][0])
        InterpHor(refY - 4 - 4 * pitchRef, pitchRef, predBufHi20, memPitch, 2, (w + 8) & ~7, h + 10, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);
        int16_t *tmpHor2 = predBufHi20 + 3 * memPitch + 3;  // Save For Quarter Pel
        // interpolate horizontal halfpels: Lo (2,0) (-4, -4) (w+8, h+8)
        h265_InterpLumaPack(predBufHi20, memPitch, predBuf20, memPitch, w + 8, h + 8, bitDepth);  // save to predBuf[2][0]
#endif
        int _w = (w + 8) & ~7;
        int _h = h + 10;
        AV1PP::interp_flex(refY - 4 - 4 * pitchRef, pitchRef, predBuf20, memPitch, ((2 << 1) << 1) & 15, 0, _w, _h, 0, DEF_FILTER, m_par->codecType == CODEC_AV1, predBufHi20);

        uint16_t *tmpHor2 = predBufHi20 + 3 * memPitch + 3;  // Save For Quarter Pel
        // 0 1 2
        // 7   3
        // 6 5 4
        tmpSubMem[7] = predBuf20 + 4 * memPitch + 3;
        tmpSubMem[3] = tmpSubMem[7] + 1;

        int32_t *satd8 = NULL;
        int32_t satdPitch = MAX_CU_SIZE >> (3 - size);
        AV1MV mvCenter = *mv;
        int32_t costFast = 0;
        int32_t bestFastCost = *cost;
        int32_t bestHalf = -1;
        int32_t bestmvCostHalf = *mvCost;
        AV1MV bestmvHalf = *mv;

        // interpolate vertical halfpels
        PixType *predBuf02;
        tmv = *mv;
        tmv.mvy += (2 << 1);
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBuf02);

        // case[13]
#if 0
        // Hi [0,2] (0,-1) (w, h+2)
        InterpVer(refY - pitchRef, pitchRef, predBufHi02, memPitch, 2, w, h + 2, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);
        // Lo [0,2] (0,-1) (w, h+2)
        h265_InterpLumaPack(predBufHi02, memPitch, predBuf02, memPitch, w, h + 2, bitDepth);
#endif
        AV1PP::interp_flex(refY - pitchRef, pitchRef, predBuf02, memPitch, 0, ((2 << 1) << 1) & 15, w, h + 2, 0/* no BiPred */, DEF_FILTER, m_par->codecType == CODEC_AV1);

        tmpSubMem[1] = predBuf02;
        tmpSubMem[5] = predBuf02 + memPitch;

        costFast = costFunc(src, pitchSrc, tmpSubMem[1], memPitch, log2w, log2h) >> costShift;
        CompareFastCosts(costFast, bestFastCost, 1, 1, bestHalf, bestmvCostHalf, bestmvHalf);

        costFast = costFunc(src, pitchSrc, tmpSubMem[3], memPitch, log2w, log2h) >> costShift;
        CompareFastCosts(costFast, bestFastCost, 3, 1, bestHalf, bestmvCostHalf, bestmvHalf);

        costFast = costFunc(src, pitchSrc, tmpSubMem[5], memPitch, log2w, log2h) >> costShift;
        CompareFastCosts(costFast, bestFastCost, 5, 1, bestHalf, bestmvCostHalf, bestmvHalf);

        costFast = costFunc(src, pitchSrc, tmpSubMem[7], memPitch, log2w, log2h) >> costShift;
        CompareFastCosts(costFast, bestFastCost, 7, 1, bestHalf, bestmvCostHalf, bestmvHalf);

        AV1MV mvBest = *mv;
        int32_t costBest = *cost;
        int32_t mvCostBest = *mvCost;
        if (m_par->hadamardMe >= 2) {  // always
            // when satd is for subpel only need to recalculate cost for intpel motion vector
            // ignore LSB to meet HEVC behaviour
            MemHadGetBuf(size, (mv->mvx >> 1) & 3, (mv->mvy >> 1) & 3, refIdxMem, mv, &satd8);
            costBest = tuHadSave(src, pitchSrc, refY, pitchRef, w, h, satd8, satdPitch) >> costShift;
            costBest += mvCostBest;
        }

        if (bestHalf > 0) {
            // interpolate diagonal halfpels Hi (2,2) (-4,-1) (w+8, h+2)
            uint16_t *tmpHor22 = predBufHi20 + 3 * memPitch;
            //int16_t *predBufHi22;
            PixType *predBuf22;

            tmv.mvx = mv->mvx + (2 << 1);
            tmv.mvy = mv->mvy + (2 << 1);
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBuf22);
#if 0
            InterpVer(tmpHor22, memPitch, predBufHi22, memPitch, 2, (w + 8) & ~7, h + 2, 6, 0, bitDepth, preAvgTmpBuf);  // save to predBufHi[2][2]
            // Lo (2,2) (-4,-1) (w+8, h+2)
            h265_InterpLumaPack(predBufHi22, memPitch, predBuf22, memPitch, w + 8, h + 2, bitDepth);  // save to predBuf[2][2]
#endif
            AV1PP::interp_flex(tmpHor22, memPitch, predBuf22, memPitch, 0, ((2 << 1) << 1) & 15, (w + 8) & ~7, h + 2, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);
#ifdef MEMOIZE_SUBPEL_TEST
            costFast = costFunc(src, pitchSrc, predBuf22 + 3, memPitch, log2w, log2h) >> costShift;
            AV1PP::interp_flex(refY - 4 - pitchRef, pitchRef, m_scratchPad.interp.matchingMetric.predBuf, memPitch, ((2 << 1) << 1) & 15, ((2 << 1) << 1) & 15, (w + 8) & ~7, h + 2, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);
            int32_t costTest = costFunc(src, pitchSrc, m_scratchPad.interp.matchingMetric.predBuf + 3, memPitch, log2w, log2h) >> costShift;
            assert(costTest == costFast);
#endif
            tmpSubMem[0] = predBuf22 + 3;
            tmpSubMem[2] = tmpSubMem[0] + 1;
            tmpSubMem[4] = tmpSubMem[0] + 1 + memPitch;
            tmpSubMem[6] = tmpSubMem[0] + memPitch;

            costFast = costFunc(src, pitchSrc, tmpSubMem[0], memPitch, log2w, log2h) >> costShift;
            CompareFastCosts(costFast, bestFastCost, 0, 1, bestHalf, bestmvCostHalf, bestmvHalf);

            costFast = costFunc(src, pitchSrc, tmpSubMem[2], memPitch, log2w, log2h) >> costShift;
            CompareFastCosts(costFast, bestFastCost, 2, 1, bestHalf, bestmvCostHalf, bestmvHalf);

            costFast = costFunc(src, pitchSrc, tmpSubMem[4], memPitch, log2w, log2h) >> costShift;
            CompareFastCosts(costFast, bestFastCost, 4, 1, bestHalf, bestmvCostHalf, bestmvHalf);

            costFast = costFunc(src, pitchSrc, tmpSubMem[6], memPitch, log2w, log2h) >> costShift;
            CompareFastCosts(costFast, bestFastCost, 6, 1, bestHalf, bestmvCostHalf, bestmvHalf);

            MemHadGetBuf(size, (bestmvHalf.mvx >> 1) & 3, (bestmvHalf.mvy >> 1) & 3, refIdxMem, &bestmvHalf, &satd8);
            bestFastCost = tuHadSave(src, pitchSrc, tmpSubMem[bestHalf], memPitch, w, h, satd8, satdPitch) >> costShift;

            bestFastCost += bestmvCostHalf;
            if (costBest > bestFastCost) {
                costBest = bestFastCost;
                mvCostBest = bestmvCostHalf;
                mvBest = bestmvHalf;
            }
        }

        int32_t hpelx = mvBest.mvx - mv->mvx + (4 << 1);  // comment form hevc: can be 2, 4 or 6
        int32_t hpely = mvBest.mvy - mv->mvy + (4 << 1);  // can be 2, 4 or 6
        int32_t dx = hpelx & 7;  // comment form hevc: can be 0 or 2
        int32_t dy = hpely & 7;  // can be 0 or 2

        mvCenter = mvBest;
        costFast = 0;
        bestFastCost = INT_MAX;
        int32_t bestQuarterDia = 1;
        int32_t bestQuarterDiaMvCost = 0;
        AV1MV bestmvQuarter;
        // 0 1 2
        // 7   3
        // 6 5 4
        // interpolate vertical quater-pels
        if (dx == 0) {  // best halfpel is intpel or ver-halfpel [0,1/3] (0,0)(w,h)
            PixType *predBuf0q1;
            tmv = mvBest;
            tmv.mvy -= (1 << 1);
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBuf0q1);
#if 0
            // hpx+0 hpy-1/4 predBufHi[0][1/3]
            InterpVer(refY + (hpely - 5 >> 2) * pitchRef, pitchRef, predBufHi0q1, memPitch, 3 - dy, w, h, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);
            h265_InterpLumaPack(predBufHi0q1, memPitch, predBuf0q1, memPitch, w, h, bitDepth);  // predBuf[0][1/3]
#endif
            AV1PP::interp_flex(refY + (hpely - (5 << 1) >> 3) * pitchRef, pitchRef, predBuf0q1, memPitch, 0, ((((3 << 1) - dy)) << 1) & 15, w, h, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);

            tmpSubMem[1] = predBuf0q1;
        }
        else {  // best halfpel is hor-halfpel or diag-halfpel
            PixType *predBuf2q1;
            tmv.mvx = mvBest.mvx;
            tmv.mvy = mv->mvy + (3 << 1) - dy;
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBuf2q1);
#if 0
            // [2,1/3] (0,0) (w,h+2)
            // hpx+0 hpy-1/4 predBufHi[2][1/3]
            InterpVer(tmpHor2 + (hpelx >> 2), memPitch, predBufHi2q1, memPitch, 3 - dy, w, h + 2, 6, 0, bitDepth, preAvgTmpBuf);
            h265_InterpLumaPack(predBufHi2q1, memPitch, predBuf2q1, memPitch, w, h + 2, bitDepth);  // predBuf[2][1/3]
#endif
            AV1PP::interp_flex(tmpHor2 + (hpelx >> 3), memPitch, predBuf2q1, memPitch, 0, (((3 << 1) - dy) << 1) & 15, w, h + 2, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);

            tmpSubMem[1] = predBuf2q1 + (hpely - (1 << 1) >> 3) * memPitch;
        }

        // interpolate vertical qpels
        if (dx == 0) {  // best halfpel is intpel or ver-halfpel [0,1/3] (0,0) (w,h)
            PixType *predBuf0q5;
            tmv = mvBest;
            tmv.mvy += (1 << 1);
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBuf0q5);
#if 0
            // hpx+0 hpy+1/4 predBufHi[2][1/3]
            InterpVer(refY + (hpely - 3 >> 2) * pitchRef, pitchRef, predBufHi0q5, memPitch, dy + 1, w, h, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);
            h265_InterpLumaPack(predBufHi0q5, memPitch, predBuf0q5, memPitch, w, h, bitDepth);  // predBuf[0][1/3]
#endif
            AV1PP::interp_flex(refY + (hpely - (3 << 1) >> 3) * pitchRef, pitchRef, predBuf0q5, memPitch, 0, ((dy + (1 << 1)) << 1) & 15, w, h, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);

            tmpSubMem[5] = predBuf0q5;
        }
        else {  // best halfpel is hor-halfpel or diag-halfpel
            PixType *predBuf2q5;
            tmv.mvx = mvBest.mvx;
            tmv.mvy = mv->mvy + dy + (1 << 1);
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBuf2q5);

#if 0
            // [2,1/3] (0,0) (w,h+2)
            // hpx+0 hpy+1/4 predBufHi[2][1/3]
            InterpVer(tmpHor2 + (hpelx >> 2), memPitch, predBufHi2q5, memPitch, dy + 1, w, h + 2, 6, 0, bitDepth, preAvgTmpBuf);
            h265_InterpLumaPack(predBufHi2q5, memPitch, predBuf2q5, memPitch, w, h + 2, bitDepth);  // predBuf[2][1/3]
#endif
            AV1PP::interp_flex(tmpHor2 + (hpelx >> 3), memPitch, predBuf2q5, memPitch, 0, ((dy + (1 << 1)) << 1) & 15, w, h + 2, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);

            tmpSubMem[5] = predBuf2q5 + (hpely + (1 << 1) >> 3) * memPitch;
        }

        // intermediate horizontal quater-pels (left of best half-pel) [1/3,0] (0,-4) (w,h+8)
        uint16_t *predBufHiq07;
        PixType *predBufq07;
        tmv.mvx = mvBest.mvx - (1 << 1);
        tmv.mvy = mv->mvy;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBufq07, (int16_t**)&predBufHiq07);

        // case[7]
#if 0
        int16_t *tmpHor1 = predBufHiq07 + 3 * memPitch;
        // hpx-1/4 hpy+0 (intermediate) predBufHi[1/3][0]
        InterpHor(refY - 4 * pitchRef + (hpelx - 5 >> 2), pitchRef, predBufHiq07, memPitch, 3 - dx, w, h + 10, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);
        // Lo [1/3,0] (0,-4) (w, h+8)
        h265_InterpLumaPack(predBufHiq07, memPitch, predBufq07, memPitch, w, h + 8, bitDepth);  // save to predBuf[1/3][0]
#endif
        uint16_t *tmpHor1 = predBufHiq07 + 3 * memPitch;
        AV1PP::interp_flex(refY - 4 * pitchRef + (hpelx - (5 << 1) >> 3), pitchRef, predBufq07, memPitch, (((3 << 1) - dx) << 1) & 15, 0, w, h + 10, 0, DEF_FILTER, m_par->codecType == CODEC_AV1, predBufHiq07);

        // interpolate horizontal quater-pels (left of best half-pel)
        if (dy == 0) {  // best halfpel is intpel or hor-halfpel [1/3, 0] (0,0) (w,h)
            tmpSubMem[7] = predBufq07 + 4 * memPitch;
        }
        else {  // best halfpel is vert-halfpel or diag-halfpel [1/3,2] (0,0) (w,h+2)
            PixType *predBufq27;
            tmv.mvx = mvBest.mvx - (1 << 1);
            tmv.mvy = mv->mvy + dy;
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBufq27);

#if 0
            // hpx-1/4 hpy+0 save to predBufHi[1/3][2]
            InterpVer(tmpHor1, memPitch, predBufHiq27, memPitch, dy, w, h + 2, 6, 0, bitDepth, preAvgTmpBuf);
            h265_InterpLumaPack(predBufHiq27, memPitch, predBufq27, memPitch, w, h + 2, bitDepth);  // predBuf[1/3][2]
#endif
            AV1PP::interp_flex(tmpHor1, memPitch, predBufq27, memPitch, 0, ((dy) << 1) & 15, w, h + 2, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);

            tmpSubMem[7] = predBufq27 + (hpely >> 3) * memPitch;
        }

        uint16_t *predBufHiq03;
        PixType *predBufq03;
        // intermediate horizontal quater-pels (right of best half-pel) [1/3,0] (0,-4) (w,h+8)
        tmv.mvx = mvBest.mvx + (1 << 1);
        tmv.mvy = mv->mvy;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBufq03, (int16_t**)&predBufHiq03);

#if 0
        int16_t *tmpHor3 = predBufHiq03 + 3 * memPitch;
        // hpx+1/4 hpy+0 (intermediate) predBufHi[1/3][0]
        InterpHor(refY - 4 * pitchRef + (hpelx - 3 >> 2), pitchRef, predBufHiq03, memPitch, dx + 1, w, h + 10, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);
        // Lo (1/3,0) (0,-4) (w, h+8)
        h265_InterpLumaPack(predBufHiq03, memPitch, predBufq03, memPitch, w, h + 8, bitDepth);  // save to predBuf[1/3][0]
#endif
        uint16_t *tmpHor3 = predBufHiq03 + 3 * memPitch;
        AV1PP::interp_flex(refY - 4 * pitchRef + (hpelx - (3 << 1) >> 3), pitchRef, predBufq03, memPitch, ((dx + (1 << 1)) << 1) & 15, 0, w, h + 10, 0, DEF_FILTER, m_par->codecType == CODEC_AV1, predBufHiq03);

        // case[4]
        // interpolate horizontal quater-pels (right of best half-pel)
        if (dy == 0) {  // best halfpel is intpel or hor-halfpel [1/3, 0] (0,0) (w,h)
            tmpSubMem[3] = predBufq03 + 4 * memPitch;
        }
        else {  // best halfpel is vert-halfpel or diag-halfpel [1/3,2] (0,0) (w,h+2)
            PixType *predBufq23;
            tmv.mvx = mvBest.mvx + (1 << 1);
            tmv.mvy = mv->mvy + dy;
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBufq23);
#if 0
            // hpx+1/4 hpy+0 predBufHi[1/3][2]
            InterpVer(tmpHor3, memPitch, predBufHiq23, memPitch, dy, w, h + 2, 6, 0, bitDepth, preAvgTmpBuf);
            h265_InterpLumaPack(predBufHiq23, memPitch, predBufq23, memPitch, w, h + 2, bitDepth);  // predBuf[1/3][2]
#endif
            AV1PP::interp_flex(tmpHor3, memPitch, predBufq23, memPitch, 0, ((dy) << 1) & 15, w, h + 2, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);

            tmpSubMem[3] = predBufq23 + (hpely >> 3) * memPitch;
        }

        costFast = costFunc(src, pitchSrc, tmpSubMem[1], memPitch, log2w, log2h) >> costShift;  // sad[0][1/3]
        CompareFastCosts(costFast, bestFastCost, 1, 0, bestQuarterDia, bestQuarterDiaMvCost, bestmvQuarter);

        costFast = costFunc(src, pitchSrc, tmpSubMem[3], memPitch, log2w, log2h) >> costShift;
        CompareFastCosts(costFast, bestFastCost, 3, 0, bestQuarterDia, bestQuarterDiaMvCost, bestmvQuarter);

        costFast = costFunc(src, pitchSrc, tmpSubMem[5], memPitch, log2w, log2h) >> costShift;
        CompareFastCosts(costFast, bestFastCost, 5, 0, bestQuarterDia, bestQuarterDiaMvCost, bestmvQuarter);

        costFast = costFunc(src, pitchSrc, tmpSubMem[7], memPitch, log2w, log2h) >> costShift;
        CompareFastCosts(costFast, bestFastCost, 7, 0, bestQuarterDia, bestQuarterDiaMvCost, bestmvQuarter);

        MemHadGetBuf(size, (bestmvQuarter.mvx >> 1) & 3, (bestmvQuarter.mvy >> 1) & 3, refIdxMem, &bestmvQuarter, &satd8);
        bestFastCost = tuHadSave(src, pitchSrc, tmpSubMem[bestQuarterDia], memPitch, w, h, satd8, satdPitch) >> costShift;
        bestFastCost += bestQuarterDiaMvCost;

        if (costBest > bestFastCost) {

            // 0 1 2
            // 7   3
            // 6 5 4
            mvCenter = mvBest;
            costBest = bestFastCost;
            mvCostBest = bestQuarterDiaMvCost;
            mvBest = bestmvQuarter;

            bestFastCost = INT_MAX;
            int32_t bestQuarter = 0;

            // case[0]
            if (bestQuarterDia == 1 || bestQuarterDia == 7)
            {
                PixType *predBufqq0;
                tmv.mvx = mvCenter.mvx - (1 << 1);
                tmv.mvy = mv->mvy + (3 << 1) - dy;
                MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBufqq0);

#if 0
                // interpolate 2 diagonal quater-pels (left-top and left-bottom of best half-pel) [1/3,1/3] (0,0) (w,h+2)
                // hpx-1/4 hpy-1/4  predBufHi[1/3][1/3]
                InterpVer(tmpHor1, memPitch, predBufHiqq0, memPitch, 3 - dy, w, h + 2, 6, 0, bitDepth, preAvgTmpBuf);
                h265_InterpLumaPack(predBufHiqq0, memPitch, predBufqq0, memPitch, w, h + 2, bitDepth);  // predBuf[1/3][1/3]
#endif
                AV1PP::interp_flex(tmpHor1, memPitch, predBufqq0, memPitch, 0, (((3 << 1) - dy) << 1) & 15, w, h + 2, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);

                tmpSubMem[0] = predBufqq0 + (hpely - (1 << 1) >> 3) * memPitch;
                costFast = costFunc(src, pitchSrc, tmpSubMem[0], memPitch, log2w, log2h) >> costShift;  // sad[1/3][1/3]
                CompareFastCosts(costFast, bestFastCost, 0, 0, bestQuarter, bestQuarterDiaMvCost, bestmvQuarter);
            }

            // case[2]
            if (bestQuarterDia == 1 || bestQuarterDia == 3)
            {
                PixType *predBufqq2;
                // interpolate 2 diagonal quater-pels (right-top and right-bottom of best half-pel) [1/3,1/3] (0,0) (w,h+2)
                tmv.mvx = mvCenter.mvx + (1 << 1);
                tmv.mvy = mv->mvy + (3 << 1) - dy;
                MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBufqq2);

#if 0
                // hpx+1/4 hpy-1/4 predBufHi[1/3][1/3]
                InterpVer(tmpHor3, memPitch, predBufHiqq2, memPitch, 3 - dy, w, h + 2, 6, 0, bitDepth, preAvgTmpBuf);
                h265_InterpLumaPack(predBufHiqq2, memPitch, predBufqq2, memPitch, w, h + 2, bitDepth);    // predBuf[1/3][1/3]
#endif
                AV1PP::interp_flex(tmpHor3, memPitch, predBufqq2, memPitch, 0, (((3 << 1) - dy) << 1) & 15, w, h + 2, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);

                tmpSubMem[2] = predBufqq2 + (hpely - (1 << 1) >> 3) * memPitch;
                costFast = costFunc(src, pitchSrc, tmpSubMem[2], memPitch, log2w, log2h) >> costShift;  // sad[1/3][1/3]
                CompareFastCosts(costFast, bestFastCost, 2, 0, bestQuarter, bestQuarterDiaMvCost, bestmvQuarter);
            }

            // case[6]
            if (bestQuarterDia == 7 || bestQuarterDia == 5)
            {
                PixType *predBufqq6;
                tmv.mvx = mvCenter.mvx - (1 << 1);
                tmv.mvy = mv->mvy + dy + (1 << 1);

                MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBufqq6);

#if 0
                // hpx-1/4 hpy+1/4  predBufHi[1/3][1/3]
                InterpVer(tmpHor1, memPitch, predBufHiqq6, memPitch, dy + 1, w, h + 2, 6, 0, bitDepth, preAvgTmpBuf);
                h265_InterpLumaPack(predBufHiqq6, memPitch, predBufqq6, memPitch, w, h + 2, bitDepth);  // predBuf[1/3][1/3]
#endif
                AV1PP::interp_flex(tmpHor1, memPitch, predBufqq6, memPitch, 0, ((dy + (1 << 1)) << 1) & 15, w, h + 2, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);

                tmpSubMem[6] = predBufqq6 + (hpely + (1 << 1) >> 3) * memPitch;
                costFast = costFunc(src, pitchSrc, tmpSubMem[6], memPitch, log2w, log2h) >> costShift;  // sad[1/3][1/3]
                CompareFastCosts(costFast, bestFastCost, 6, 0, bestQuarter, bestQuarterDiaMvCost, bestmvQuarter);
            }

            // case[4]
            if (bestQuarterDia == 3 || bestQuarterDia == 5)
            {
                PixType *predBufqq4;
                tmv.mvx = mvCenter.mvx + (1 << 1);
                tmv.mvy = mv->mvy + dy + (1 << 1);
                MemSubpelGetBufSetMv(size, &tmv, refIdxMem, &predBufqq4);

#if 0
                // hpx+1/4 hpy+1/4 predBufHi[1/3][1/3]
                InterpVer(tmpHor3, memPitch, predBufHiqq4, memPitch, dy + 1, w, h + 2, 6, 0, bitDepth, preAvgTmpBuf);
                h265_InterpLumaPack(predBufHiqq4, memPitch, predBufqq4, memPitch, w, h + 2, bitDepth);                // predBuf[1/3][1/3]
#endif
                AV1PP::interp_flex(tmpHor3, memPitch, predBufqq4, memPitch, 0, ((dy + (1 << 1)) << 1) & 15, w, h + 2, 0, DEF_FILTER, m_par->codecType == CODEC_AV1);

                tmpSubMem[4] = predBufqq4 + (hpely + (1 << 1) >> 3) * memPitch;
                costFast = costFunc(src, pitchSrc, tmpSubMem[4], memPitch, log2w, log2h) >> costShift;  // sad[1/3][1/3]
                CompareFastCosts(costFast, bestFastCost, 4, 0, bestQuarter, bestQuarterDiaMvCost, bestmvQuarter);
            }

            MemHadGetBuf(size, (bestmvQuarter.mvx >> 1) & 3, (bestmvQuarter.mvy >> 1) & 3, refIdxMem, &bestmvQuarter, &satd8);
            bestFastCost = tuHadSave(src, pitchSrc, tmpSubMem[bestQuarter], memPitch, w, h, satd8, satdPitch) >> costShift;                  // had
            bestFastCost += bestQuarterDiaMvCost;

            if (costBest > bestFastCost) {
                costBest = bestFastCost;
                mvCostBest = bestQuarterDiaMvCost;
                mvBest = bestmvQuarter;
            }
        }
        *cost = costBest;
        *mvCost = mvCostBest;
        *mv = mvBest;
    }
#endif

    template <typename PixType>
    bool AV1CU<PixType>::MeSubPelSeed(const AV1MEInfo *meInfo, const AV1MV &mvp, const FrameData *ref,
        AV1MV *mvBest, int32_t *costBest, int32_t *mvCostBest, int32_t refIdxMem,
        const AV1MV &mvInt, const AV1MV &mvBestSub)
    {
        bool subpelSeeded = false;
        if (mvBestSub.mvx & 7 || mvBestSub.mvy & 7)
        {
            AV1MV mvInt1, mvInt2, mvInt3, mvInt4;
            mvInt1 = mvInt2 = mvInt3 = mvInt4 = mvInt;
            // bounding ints
            mvInt1.mvx = mvBestSub.mvx & ~7;
            mvInt1.mvy = mvBestSub.mvy & ~7;
            if (mvBestSub.mvx & 7) {
                mvInt2 = mvInt1;
                mvInt2.mvx += (4 << 1);
            }
            if (mvBestSub.mvy & 7) {
                mvInt3 = mvInt1;
                mvInt3.mvy += (4 << 1);
                if (m_par->m_framesInParallel > 1 && !CheckFrameThreadingSearchRange(meInfo, &mvInt3))
                    return subpelSeeded;
            }
            if (mvBestSub.mvx & 7 && mvBestSub.mvy & 7) {
                mvInt4.mvx = mvInt2.mvx;
                mvInt4.mvy = mvInt3.mvy;
            }

            if (*mvBest == mvInt1 || *mvBest == mvInt2 || *mvBest == mvInt3 || *mvBest == mvInt4) {
                const int32_t size = BSR(meInfo->width | meInfo->height) - 3; assert(meInfo->width <= (8 << size) && meInfo->height <= (8 << size));
                int32_t hadFoundSize = size;
                int32_t useHadamard = (m_par->hadamardMe == 3);
                PixType *src = m_ySrc + meInfo->posx + meInfo->posy * SRC_PITCH;

                int32_t cost = MatchingMetricPuMem(src, meInfo, &mvBestSub, ref, useHadamard, refIdxMem, size, hadFoundSize);
#ifdef MEMOIZE_SUBPEL_TEST
                int32_t testcost = MatchingMetricPu(src, meInfo, &mvBestSub, ref, useHadamard);
                assert(testcost == cost);
#endif

                //int32_t cost = MatchingMetricPu(src, meInfo, &mvBestSub, ref, useHadamard);
                cost += MvCost1RefLog(mvBestSub, mvp, m_rdLambdaSqrt);
                if (cost < *costBest) {
                    subpelSeeded = true;
                    useHadamard = (m_par->hadamardMe >= 2);
                    *mvBest = mvBestSub;
                    *costBest = MatchingMetricPuMem(src, meInfo, mvBest, ref, useHadamard, refIdxMem, size, hadFoundSize);
#ifdef MEMOIZE_SUBPEL_TEST
                    int32_t testcost = MatchingMetricPu(src, meInfo, mvBest, ref, useHadamard);
                    assert(testcost == *costBest);
#endif
                    *mvCostBest = MvCost1RefLog(*mvBest, mvp, m_rdLambdaSqrt);
                    *costBest += *mvCostBest;
                    AV1MV mvCenter = *mvBest;
                    int32_t bestPos = 0;
                    for (int32_t i = 0; i < 4; i++) {
                        AV1MV mv = mvCenter;
                        if (i & 2) mv.mvx += (i & 1) ? (1 << 1) : (-1 << 1);  // ok for VPX_Q3 ???
                        else       mv.mvy += (i & 1) ? (1 << 1) : (-1 << 1);  // ????
                        hadFoundSize = size;
                        cost = MatchingMetricPuMem(src, meInfo, &mv, ref, useHadamard, refIdxMem, size, hadFoundSize);
#ifdef MEMOIZE_SUBPEL_TEST
                        int32_t testcost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                        assert(testcost == cost);
#endif
                        if (cost < *costBest) {
                            int32_t mvCost = MvCost1RefLog(mv, mvp, m_rdLambdaSqrt);
                            if (*costBest > cost + mvCost) {
                                *costBest = cost + mvCost;
                                *mvBest = mv;
                                *mvCostBest = mvCost;
                                bestPos = i;
                            }
                        }
                    }
                    if (*mvBest != mvCenter) {
                        mvCenter = *mvBest;
                        for (int32_t i = 0; i < 2; i++) {
                            AV1MV mv = mvCenter;
                            if (bestPos & 2) mv.mvy += (i & 1) ? (1 << 1) : (-1 << 1);
                            else             mv.mvx += (i & 1) ? (1 << 1) : (-1 << 1);
                            hadFoundSize = size;
                            cost = MatchingMetricPuMem(src, meInfo, &mv, ref, useHadamard, refIdxMem, size, hadFoundSize);
#ifdef MEMOIZE_SUBPEL_TEST
                            int32_t testcost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                            assert(testcost == cost);
#endif
                            if (cost < *costBest) {
                                int32_t mvCost = MvCost1RefLog(mv, mvp, m_rdLambdaSqrt);
                                if (*costBest > cost + mvCost) {
                                    *costBest = cost + mvCost;
                                    *mvBest = mv;
                                    *mvCostBest = mvCost;
                                }
                            }
                        }
                    }
                }
            }
        }
        return subpelSeeded;
    }


    template <typename PixType>
    void AV1CU<PixType>::MeSubPel(const AV1MEInfo *meInfo, const AV1MV &mvp, const FrameData *ref,
        AV1MV *outMv, int32_t *outCost, int32_t *outMvCost, int32_t refIdxMem)
    {
        AV1MV mvCenter = *outMv;
        AV1MV mvBest = *outMv;
        int32_t startPos = 1;
        int32_t meStep = 1;
        int32_t costBest = *outCost;
        int32_t costBestSAD = *outCost;
        int32_t mvCostBest = *outMvCost;

        const int32_t endPos[5] = { 5, 9, 5, 3, 3 };  // based on index
        int16_t pattern_index[2][2];    // meStep, iter
        int32_t iterNum[2] = { 1, 1 };


        int32_t size = BSR(meInfo->width | meInfo->height) - 3; assert(meInfo->width <= (8 << size) && meInfo->height <= (8 << size));
        int32_t hadFoundSize = size;

#ifdef MEMOIZE_SUBPEL_TEST
        AV1MV mvtemp = *mv;
        int32_t costtemp = *cost;
        int32_t mvCosttemp = *mvCost;
        bool TestMemSubPelBatchedFastBoxDiaOrth = false;
#endif

        // select subMe pattern
        switch (m_par->patternSubPel)
        {
        case SUBPEL_NO:             // int pel only
            return;
        case SUBPEL_BOX_HPEL_ONLY:  // more points with square patterns, no quarter-pel
            pattern_index[0][1] = pattern_index[0][0] = pattern_index[0][1] = pattern_index[1][0] = 1;
            break;
        case SUBPEL_DIA_2STEP:      // quarter pel with simplified diamond pattern - double
            pattern_index[0][1] = pattern_index[0][0] = pattern_index[0][1] = pattern_index[1][0] = 0;
            iterNum[1] = iterNum[0] = 2;
            break;

        case SUBPEL_FASTBOX_DIA_ORTH:
            // +, (x), +, (--,|)
            iterNum[1] = iterNum[0] = 2;
            pattern_index[1][0] = pattern_index[0][0] = 0;
            pattern_index[1][1] = 2;
            if (m_par->hadamardMe == 2) {
#if defined(USE_BATCHED_FASTORTH_SEARCH)
#if 1
                //if (meInfo->width == meInfo->height && (meInfo->width == m_par->MaxCUSize || MemHadFirst(size, meInfo, refIdxMem))) { // no forced splits in av1 yet
                if (meInfo->width == meInfo->height && meInfo->width == m_par->MaxCUSize) {
#ifdef MEMOIZE_SUBPEL_TEST
                    MemSubPelBatchedFastBoxDiaOrth(meInfo, mvp/*predInfo*/, ref, &mvtemp, &costtemp, &mvCosttemp, refIdxMem, size);
                    TestMemSubPelBatchedFastBoxDiaOrth = true;
#else
                    return MemSubPelBatchedFastBoxDiaOrth(meInfo, mvp/*predInfo*/, ref, mv, cost, mvCost, refIdxMem, size);
#endif
                }
                else if (!MemSubpelInRange(size, meInfo, refIdxMem, mv) && meInfo->width == meInfo->height) {
#ifdef MEMOIZE_SUBPEL_TEST
                    MemSubPelBatchedFastBoxDiaOrth(meInfo, mvp/*predInfo*/, ref, &mvtemp, &costtemp, &mvCosttemp, refIdxMem, size);
                    TestMemSubPelBatchedFastBoxDiaOrth = true;
#else
                    return MemSubPelBatchedFastBoxDiaOrth(meInfo, mvp/*predInfo*/, ref, mv, cost, mvCost, refIdxMem, size);
#endif
                }
#else
                return MemSubPelBatchedFastBoxDiaOrth(meInfo, mvp, ref, mv, cost, mvCost, refIdxMem, size);
#endif
#endif
            }
            break;
        }

        PixType *src = m_ySrc + meInfo->posx + meInfo->posy * SRC_PITCH;

        int32_t useHadamard = (m_par->hadamardMe >= 2);
        if (m_par->hadamardMe == 2) {
            // when satd is for subpel only need to recalculate cost for intpel motion vector

            costBest = mvCostBest + MatchingMetricPuMem(src, meInfo, &mvBest, ref, useHadamard, refIdxMem, size, hadFoundSize);
#ifdef MEMOIZE_SUBPEL_TEST
            int32_t testcostBest = mvCostBest + MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard);
            assert(testcostBest == costBest);
#endif
            if (m_par->partModes == 1 && hadFoundSize <= size) hadFoundSize = -1;
        }


        if (m_par->patternSubPel == SUBPEL_FASTBOX_DIA_ORTH) {
            meStep = 1;
            useHadamard = (m_par->hadamardMe == 3);
            int32_t costBestHalf = costBestSAD;    // or HAD if hadamardMe == 3
            int32_t mvCostBestHalf = *outMvCost;
            AV1MV mvBestHalf = mvCenter;

            for (int32_t iter = 0; iter < iterNum[meStep]; iter++) {
                for (int32_t mePos = startPos; mePos < endPos[pattern_index[meStep][iter]]; mePos++) {
                    AV1MV mv = {
                        int16_t(mvCenter.mvx + ((tab_mePatternSelector[pattern_index[meStep][iter]][mePos][0] << meStep) << 1)),
                        int16_t(mvCenter.mvy + ((tab_mePatternSelector[pattern_index[meStep][iter]][mePos][1] << meStep) << 1))
                    };
                    int32_t cost = MatchingMetricPuMem(src, meInfo, &mv, ref, useHadamard, refIdxMem, size, hadFoundSize);
#ifdef MEMOIZE_SUBPEL_TEST
                    int32_t testcost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                    assert(testcost == cost);
#endif
                    if (costBestHalf > cost) {
                        int32_t mvCost = MvCost1RefLog(mv, mvp, m_rdLambdaSqrt);
                        cost += mvCost;
                        if (costBestHalf > cost) {
                            costBestHalf = cost;
                            mvCostBestHalf = mvCost;
                            mvBestHalf = mv;
                        }
                    }
                }
                if (mvBestHalf == mvCenter)
                    break;
            }

            if (mvBestHalf != mvCenter) {
                if (m_par->hadamardMe == 2) {
                    costBestHalf = MatchingMetricPuMem(src, meInfo, &mvBestHalf, ref, true, refIdxMem, size, hadFoundSize);
#ifdef MEMOIZE_SUBPEL_TEST
                    int32_t testcost = MatchingMetricPu(src, meInfo, &mvBestHalf, ref, true);
                    assert(testcost == costBestHalf);
#endif
                    costBestHalf += mvCostBestHalf;
                }
                if (costBest > costBestHalf) {
                    costBest = costBestHalf;
                    mvCostBest = mvCostBestHalf;
                    mvBest = mvBestHalf;
                }
            }

            hadFoundSize = size;
            mvCenter = mvBest;
            meStep--;
            int32_t costBestQuarter = INT_MAX;
            int32_t mvCostBestQuarter = 0;
            AV1MV mvBestQuarter = mvCenter;
            int32_t bestQPos = 1;
            for (int32_t iter = 0; iter < iterNum[meStep]; iter++) {
                for (int32_t mePos = startPos; mePos < endPos[pattern_index[meStep][iter]]; mePos++) {
                    AV1MV mv = {
                        int16_t(mvCenter.mvx + ((tab_mePatternSelector[pattern_index[meStep][iter]][mePos][0] << meStep) << 1)),
                        int16_t(mvCenter.mvy + ((tab_mePatternSelector[pattern_index[meStep][iter]][mePos][1] << meStep) << 1))
                    };
                    int32_t cost = MatchingMetricPuMem(src, meInfo, &mv, ref, useHadamard, refIdxMem, size, hadFoundSize);
                    //int32_t cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
#ifdef MEMOIZE_SUBPEL_TEST
                    int32_t testcost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                    assert(testcost == cost);
#endif
                    if (costBestQuarter > cost) {
                        int32_t mvCost = MvCost1RefLog(mv, mvp, m_rdLambdaSqrt);
                        cost += mvCost;
                        if (costBestQuarter > cost) {
                            costBestQuarter = cost;
                            mvCostBestQuarter = mvCost;
                            mvBestQuarter = mv;
                            bestQPos = mePos;
                        }
                    }
                }

                if (m_par->hadamardMe == 2) {
                    costBestQuarter = MatchingMetricPuMem(src, meInfo, &mvBestQuarter, ref, true, refIdxMem, size, hadFoundSize);
#ifdef MEMOIZE_SUBPEL_TEST
                    int32_t testcost = MatchingMetricPu(src, meInfo, &mvBestQuarter, ref, true);
                    assert(testcost == costBestQuarter);
#endif
                    costBestQuarter += mvCostBestQuarter;
                }
                if (costBest > costBestQuarter) {
                    costBest = costBestQuarter;
                    mvCostBest = mvCostBestQuarter;
                    mvBest = mvBestQuarter;
                }
                //if (mvBestQuarter == mvCenter)
                else
                    break;
                mvCenter = mvBest;
                costBestQuarter = INT_MAX;
                mvCostBestQuarter = 0;
                mvBestQuarter = mvCenter;
                if (bestQPos == 1 || bestQPos == 3) pattern_index[0][1] = 3;
                else                                pattern_index[0][1] = 4;
            }
#ifdef MEMOIZE_SUBPEL_TEST
            if (TestMemSubPelBatchedFastBoxDiaOrth) {
                assert(costBest == costtemp);
                assert(mvCostBest == mvCosttemp);
                assert(mvBest == mvtemp);
            }
#endif
        }
        else {
            while (meStep >= 0) {
                AV1MV bestMv = mvCenter;
                for (int32_t iter = 0; iter < iterNum[meStep]; iter++) {
                    for (int32_t mePos = startPos; mePos < endPos[pattern_index[meStep][iter]]; mePos++) {
                        AV1MV mv = {
                            int16_t(bestMv.mvx + ((tab_mePatternSelector[pattern_index[meStep][iter]][mePos][0] << meStep) << 1)),
                            int16_t(bestMv.mvy + ((tab_mePatternSelector[pattern_index[meStep][iter]][mePos][1] << meStep) << 1))
                        };

                        int32_t cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                        if (m_par->partModes == 1 && hadFoundSize <= size)
                            hadFoundSize = -1;
#ifdef MEMOIZE_SUBPEL_TEST
                        int32_t testcost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                        assert(testcost == cost);
#endif

                        if (costBest > cost) {
                            int32_t mvCost = MvCost1RefLog(mv, mvp, m_rdLambdaSqrt);
                            cost += mvCost;
                            if (costBest > cost) {
                                costBest = cost;
                                mvCostBest = mvCost;
                                mvBest = mv;
                            }
                        }
                    }

                    bestMv = mvBest;
                    if (bestMv == mvCenter)
                        break;
                }

                if (m_par->patternSubPel == SUBPEL_BOX_HPEL_ONLY)
                    break;  // no quarter pel

                hadFoundSize = size;

                mvCenter = mvBest;
                meStep--;
                // startPos = 1;
            }
        }

        *outCost = costBest;
        *outMvCost = mvCostBest;
        *outMv = mvBest;
    }


    struct MeData {
        uint64_t cost;
        AV1MVPair mv;
        RefIdx refIdx[2];
        int32_t dist;
        int32_t mvBits;
        int32_t otherBits;
    };
    uint64_t CalcMeCost(const MeData &meData, uint64_t lambda) {
        return RD(meData.dist, meData.mvBits + meData.otherBits, lambda);
    }

#define NO_MVP_CANDS
    template <typename PixType>
    void AV1CU<PixType>::MePu(AV1MEInfo *meInfo)
    {
        if (m_par->enableCmFlag)
            return MePuGacc(meInfo);

        const int32_t w = meInfo->width;
        const int32_t h = meInfo->height;
        const int32_t log2w = BSR(w) - 2;
        const int32_t log2h = BSR(h) - 2;
        int32_t size = BSR(meInfo->width | meInfo->height) - 3; assert(meInfo->width <= (8 << size) && meInfo->height <= (8 << size));
        const int32_t predBufOffset = meInfo->posx + meInfo->posy * 64;

        const int32_t refYColocOffset = m_ctbPelX + meInfo->posx + (m_ctbPelY + meInfo->posy) * m_pitchRecLuma;
        const PixType *src = m_ySrc + meInfo->posx + meInfo->posy * SRC_PITCH;

        PixType *predictions[5] = {
            vp9scratchpad.interpBufs[LAST_FRAME][meInfo->depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[GOLDEN_FRAME][meInfo->depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[ALTREF_FRAME][meInfo->depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[COMP_VAR0][meInfo->depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[COMP_VAR1][meInfo->depth][NEWMV_] + predBufOffset
        };

        MeData meData[5] = {};
        meData[LAST_FRAME].dist = INT_MAX;
        meData[GOLDEN_FRAME].dist = INT_MAX;
        meData[ALTREF_FRAME].dist = INT_MAX;
        meData[COMP_VAR0].dist = INT_MAX;
        meData[COMP_VAR1].dist = INT_MAX;
        meData[LAST_FRAME].cost = ULLONG_MAX;
        meData[GOLDEN_FRAME].cost = ULLONG_MAX;
        meData[ALTREF_FRAME].cost = ULLONG_MAX;
        meData[COMP_VAR0].cost = ULLONG_MAX;
        meData[COMP_VAR1].cost = ULLONG_MAX;

        for (int8_t refIdx = LAST_FRAME; refIdx <= ALTREF_FRAME; refIdx++) {
            if (!m_currFrame->isUniq[refIdx])
                continue;

            AV1MV mvp = m_mvRefs.bestMv[refIdx][0];
            int32_t useMvHp = m_mvRefs.useMvHp[refIdx];
            AV1MV mvBest = { 0, 0 };
            int32_t costBest = INT_MAX;
            int32_t mvCostBest = 0;
            Frame *refFrm = m_currFrame->m_dpbVP9[m_currFrame->refFrameIdx[refIdx]];

            // use satd for all candidates even if satd is for subpel only
            int32_t useHadamard = (m_par->hadamardMe == 3);

            FrameData *ref = refFrm->m_recon;
#ifndef NO_MVP_CANDS
            {
                AV1MV mv = { (int16_t)(mvp.mvx & ~1), (int16_t)(mvp.mvy & ~1) };
                ClipMV_NR(&mv);

                // PATCH
                if (m_par->m_framesInParallel > 1 && CheckFrameThreadingSearchRange(meInfo, &mv) == false)
                    continue;

                // notFound = false;
                int32_t cost;
                const PixType *predBuf;
                //int32_t memPitch;
                int8_t refsArr[2] = { refIdx, refIdx };
                cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);

                if (costBest > cost) {
                    costBest = cost;
                    mvBest = mv;
                }
            }
#else
            AV1MV mv = { (int16_t)(mvp.mvx & ~1), (int16_t)(mvp.mvy & ~1) };
            ClipMV_NR(&mv);
            mvBest = mv;
#endif
            AV1MV mvBestSub = mvBest;
#ifndef NO_MVP_CANDS
            if (costBest == INT_MAX) {
                costBest = MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard);
                mvCostBest = MvCost1RefLog(mvBest, mvp, m_rdLambdaSqrt);
                costBest += mvCostBest;
            }
            else
#endif
            {
                if ((mvBest.mvx | mvBest.mvy) & 7) {
                    // recalculate cost for nearest int-pel
                    mvBest.mvx = (mvBest.mvx + (1 << 1)) & ~7;
                    mvBest.mvy = (mvBest.mvy + (1 << 1)) & ~7;
                    if (m_par->m_framesInParallel > 1 && !CheckFrameThreadingSearchRange(meInfo, &mvBest)) {
                        // best subpel amvp is inside range, but rounded intpel isn't
                        mvBest.mvy -= (4 << 1);
                    }
                    costBest = MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard);
                    mvCostBest = MvCost1RefLog(mvBest, mvp, m_rdLambdaSqrt);
                    costBest += mvCostBest;
                }
                else {
#ifdef NO_MVP_CANDS
                    costBest = MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard);
#endif
                    // add cost of zero mvd
                    mvCostBest = 2 * (int32_t)(512 * m_rdLambdaSqrt + 0.5f);
                    costBest += mvCostBest;
                }
            }

            int16_t meStepMax = MeIntSeed(meInfo, mvp, 0, refIdx, meData[0].mv[0], meData[0].mv[0], &mvBest, &costBest, &mvCostBest, &mvBestSub);
            AV1MV mvInt = mvBest;
            MeIntPel(meInfo, mvp, ref, &mvBest, &costBest, &mvCostBest, meStepMax);

            if (m_par->patternSubPel == SUBPEL_BOX
                || !MeSubPelSeed(meInfo, mvp, ref, &mvBest, &costBest, &mvCostBest, refIdx, mvInt, mvBestSub)) {
                MeSubPel(meInfo, mvp, ref, &mvBest, &costBest, &mvCostBest, refIdx);
            }

            // loop update
            if (costBest < INT_MAX) {
                const PixType *refColoc = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[refIdx]->m_recon->y) + refYColocOffset;
                if (useMvHp) {
                    costBest = Refine8pel(src, refColoc, m_pitchRecLuma, w, h, &mvBest, predictions + refIdx);
                    mvCostBest = MvCost(mvBest, mvp, 1);
                }
                else {
                    const PixType *rec = nullptr;
                    int32_t pitchRec = 0;
                    if (!MemSubpelUse(size, (mvBest.mvx >> 1) & 3, (mvBest.mvy >> 1) & 3, meInfo, refIdx, &mvBest, rec, pitchRec)) {
                        InterpolateAv1SingleRefLuma(refColoc, m_pitchRecLuma, predictions[refIdx], mvBest, h, log2w, DEF_FILTER_DUAL);
                    }
                    else {
                        CopyNxM_unaligned_src(rec, pitchRec, predictions[refIdx], 64, meInfo->width, meInfo->height);
                    }

                    costBest -= mvCostBest;
#ifdef MEMOIZE_SUBPEL_TEST
#ifdef USE_SAD_ONLY
                    costBest = AV1PP::sad_special(src, SRC_PITCH, predictions[refIdx], log2w, log2h);
#else
                    int32_t costTemp;
                    if (m_par->hadamardMe >= 2)
                        costTemp = AV1PP::satd(src, SRC_PITCH, predictions[refIdx], PRED_PITCH, log2w, log2h);
                    else
                        costTemp = AV1PP::sad_special(src, SRC_PITCH, predictions[refIdx], log2w, log2h);
#endif
                    assert(costTemp == costBest);
#endif
                    mvCostBest = MvCost(mvBest, mvp, 0);
                }

                // store current cost
                meData[refIdx].mv[0] = mvBest;
                SetMemBestMV(size, meInfo, refIdx, mvBest);
                meData[refIdx].mv[1] = MV_ZERO;
                meData[refIdx].refIdx[0] = refIdx;
                meData[refIdx].refIdx[1] = NONE_FRAME;
                meData[refIdx].dist = costBest;
                meData[refIdx].mvBits = mvCostBest;
                meData[refIdx].otherBits = m_mvRefs.refFrameBitCount[refIdx];
                meData[refIdx].cost = CalcMeCost(meData[refIdx], m_rdLambdaSqrtInt);
            }
        }

        if (m_currFrame->compoundReferenceAllowed) {
            const RefIdx compFixed = (RefIdx)m_currFrame->compFixedRef;
            const RefIdx compVar0 = (RefIdx)m_currFrame->compVarRef[0];
            const RefIdx compVar1 = (RefIdx)m_currFrame->compVarRef[1];

            int32_t compFixedIdx = 1;
            if (m_currFrame->m_picCodeType == MFX_FRAMETYPE_B)
                compFixedIdx = m_currFrame->refFrameSignBiasVp9[compFixed];

            if (meData[compFixed].dist < INT_MAX) {
                if (meData[compVar0].dist < INT_MAX) {
                    bool badPred1 = false;
                    bool badPred2 = false;

                    if (m_currFrame->m_picCodeType == MFX_FRAMETYPE_P && log2w > 1) {
                        float scpp = 0;
                        int32_t sad1 = meData[compFixed].dist;
                        int32_t sad2 = meData[compVar0].dist;
                        int32_t mvd1 = std::abs(m_mvRefs.bestMv[compFixed][0].mvx - meData[compFixed].mv[0].mvx);
                        mvd1 += std::abs(m_mvRefs.bestMv[compFixed][0].mvy - meData[compFixed].mv[0].mvy);
                        int32_t mvd2 = std::abs(m_mvRefs.bestMv[compVar0][0].mvx - meData[compVar0].mv[0].mvx);
                        mvd2 += std::abs(m_mvRefs.bestMv[compVar0][0].mvy - meData[compVar0].mv[0].mvy);
                        if (m_par->hadamardMe >= 2)
                            sad1 = AV1PP::sad_special(src, SRC_PITCH, predictions[compFixed], log2w, log2h);
                        if (m_par->hadamardMe >= 2)
                            sad2 = AV1PP::sad_special(src, SRC_PITCH, predictions[compVar0], log2w, log2h);
                        badPred1 = IsBadPred(scpp, (float)sad1, (float)mvd1);
                        badPred2 = IsBadPred(scpp, (float)sad2, (float)mvd2);
                    }

                    if (!badPred1 || !badPred2) {
                        AV1PP::average(predictions[compFixed], predictions[compVar0], predictions[COMP_VAR0], h, log2w);
                        meData[COMP_VAR0].mv[compFixedIdx] = meData[compFixed].mv[0];
                        meData[COMP_VAR0].mv[!compFixedIdx] = meData[compVar0].mv[0];
                        meData[COMP_VAR0].refIdx[compFixedIdx] = compFixed;
                        meData[COMP_VAR0].refIdx[!compFixedIdx] = compVar0;
#ifdef USE_SAD_ONLY
                        meData[COMP_VAR0].dist = AV1PP::sad_special(src, SRC_PITCH, predictions[COMP_VAR0], log2w, log2h);
#else

                        if (m_par->hadamardMe >= 2)
                            meData[COMP_VAR0].dist = AV1PP::satd(src, SRC_PITCH, predictions[COMP_VAR0], PRED_PITCH, log2w, log2h);
                        else
                            meData[COMP_VAR0].dist = AV1PP::sad_special(src, SRC_PITCH, predictions[COMP_VAR0], log2w, log2h);
#endif
                        meData[COMP_VAR0].mvBits = meData[compFixed].mvBits + meData[compVar0].mvBits;
                        meData[COMP_VAR0].otherBits = m_mvRefs.refFrameBitCount[COMP_VAR0];
                        meData[COMP_VAR0].cost = CalcMeCost(meData[COMP_VAR0], m_rdLambdaSqrtInt);
                    }
                }
                if (meData[compVar1].dist < INT_MAX) {
                    AV1PP::average(predictions[compFixed], predictions[compVar1], predictions[COMP_VAR1], h, log2w);
                    meData[COMP_VAR1].mv[compFixedIdx] = meData[compFixed].mv[0];
                    meData[COMP_VAR1].mv[!compFixedIdx] = meData[compVar1].mv[0];
                    meData[COMP_VAR1].refIdx[compFixedIdx] = compFixed;
                    meData[COMP_VAR1].refIdx[!compFixedIdx] = compVar1;
#ifdef USE_SAD_ONLY
                    meData[COMP_VAR1].dist = AV1PP::sad_special(src, SRC_PITCH, predictions[COMP_VAR1], log2w, log2h);
#else
                    if (m_par->hadamardMe >= 2)
                        meData[COMP_VAR1].dist = AV1PP::satd(src, SRC_PITCH, predictions[COMP_VAR1], PRED_PITCH, log2w, log2h);
                    else
                        meData[COMP_VAR1].dist = AV1PP::sad_special(src, SRC_PITCH, predictions[COMP_VAR1], log2w, log2h);
#endif
                    meData[COMP_VAR1].mvBits = meData[compFixed].mvBits + meData[compVar1].mvBits;
                    meData[COMP_VAR1].otherBits = m_mvRefs.refFrameBitCount[COMP_VAR1];
                    meData[COMP_VAR1].cost = CalcMeCost(meData[COMP_VAR1], m_rdLambdaSqrtInt);
                }
                const int32_t compRefIdx = COMP_VAR0 + (meData[COMP_VAR1].dist < meData[COMP_VAR0].dist);
                if (meData[compRefIdx].dist < INT_MAX && m_par->numBiRefineIter > 1) {
                    const RefIdx(&refIdx)[2] = meData[compRefIdx].refIdx;
                    AV1MVPair mvpComp;
                    mvpComp[0] = m_mvRefs.bestMv[compRefIdx][0];
                    mvpComp[1] = m_mvRefs.bestMv[compRefIdx][1];
                    for (int32_t i = 0; i < m_par->numBiRefineIter - 1; i++)
                        meData[compRefIdx].dist = RefineBiPred(*this, *meInfo, refIdx, &meData[compRefIdx].mv, mvpComp, m_par->hadamardMe >= 2);
                    meData[compRefIdx].mvBits = 0;
                    meData[compRefIdx].mvBits += MvCost(meData[compRefIdx].mv[0], mvpComp[0], m_mvRefs.useMvHp[refIdx[0]]);
                    meData[compRefIdx].mvBits += MvCost(meData[compRefIdx].mv[1], mvpComp[1], m_mvRefs.useMvHp[refIdx[1]]);
                    meData[compRefIdx].cost = CalcMeCost(meData[compRefIdx], m_rdLambdaSqrtInt);
                }
            }
        }

        uint64_t bestCost = ULLONG_MAX;
        RefIdx bestRefIdx = 0;
        for (RefIdx i = LAST_FRAME; i <= COMP_VAR1; i++) {
            if (bestCost > meData[i].cost) {
                bestCost = meData[i].cost;
                bestRefIdx = i;
            }
        }

        m_interp[LAST_FRAME][meInfo->depth][NEWMV_] = predictions[LAST_FRAME] - predBufOffset;
        m_interp[GOLDEN_FRAME][meInfo->depth][NEWMV_] = predictions[GOLDEN_FRAME] - predBufOffset;
        m_interp[ALTREF_FRAME][meInfo->depth][NEWMV_] = predictions[ALTREF_FRAME] - predBufOffset;
        m_interp[COMP_VAR0][meInfo->depth][NEWMV_] = predictions[COMP_VAR0] - predBufOffset;
        m_interp[COMP_VAR1][meInfo->depth][NEWMV_] = predictions[COMP_VAR1] - predBufOffset;

        meInfo->refIdx[0] = meData[bestRefIdx].refIdx[0];
        meInfo->refIdx[1] = meData[bestRefIdx].refIdx[1];
        meInfo->mv.asInt64 = meData[bestRefIdx].mv.asInt64;
        meInfo->refIdxComb = bestRefIdx;
        meInfo->satd = meData[bestRefIdx].dist;
        meInfo->mvBits = meData[bestRefIdx].mvBits;
    }


    namespace {
        template <typename PixType> int32_t RefineRect(AV1CU<PixType> &cu, const AV1MEInfo &meInfo, int32_t refIdx, AV1MV *initmv, const AV1MV &) {
            alignas(64) PixType pred[64 * 64];
            const PixType *src = cu.m_ySrc + meInfo.posx + meInfo.posy * cu.SRC_PITCH;
            const FrameData *refFrame = cu.m_currFrame->refFrames[refIdx]->m_recon;
            const int32_t pitchRef = refFrame->pitch_luma_pix;

            initmv->mvx <<= 1;
            initmv->mvy <<= 1;

            int32_t bestCost = INT_MAX;
            AV1MV bestMv = *initmv;
            AV1MV cmv = *initmv;

            for (int32_t dx = -4 * 8; dx <= 4 * 8; dx += 8) {
                for (int32_t dy = -4 * 8; dy <= 4 * 8; dy += 8) {
                    AV1MV mv = { cmv.mvx + dx, cmv.mvy + dy };
                    cu.ClipMV_NR(&mv);
                    int32_t cost = cu.MatchingMetricPu(src, &meInfo, &mv, refFrame, 0);
                    if (bestCost > cost) {
                        bestCost = cost;
                        bestMv = mv;
                    }
                }
            }

            cmv = bestMv;
            for (int32_t dx = -4; dx <= 4; dx += 4) {
                for (int32_t dy = -4; dy <= 4; dy += 4) {
                    AV1MV mv = { cmv.mvx + dx, cmv.mvy + dy };
                    cu.ClipMV_NR(&mv);
                    int32_t cost = cu.MatchingMetricPu(src, &meInfo, &mv, refFrame, 0);
                    if (bestCost > cost) {
                        bestCost = cost;
                        bestMv = mv;
                    }
                }
            }

            cmv = bestMv;
            for (int32_t dx = -2; dx <= 2; dx += 2) {
                for (int32_t dy = -2; dy <= 2; dy += 2) {
                    AV1MV mv = { cmv.mvx + dx, cmv.mvy + dy };
                    cu.ClipMV_NR(&mv);
                    int32_t cost = cu.MatchingMetricPu(src, &meInfo, &mv, refFrame, 0);
                    if (bestCost > cost) {
                        bestCost = cost;
                        bestMv = mv;
                    }
                }
            }


            initmv->mvx = bestMv.mvx >> 1;
            initmv->mvy = bestMv.mvy >> 1;
            return bestCost;
        }

        template <typename PixType>
        inline void AV1_FORCEINLINE Refine8pelKeepBest(int32_t *bestCost, int32_t cost, AV1MV *bestMv, int16_t mvx, int16_t mvy, PixType **bestPred, PixType **pred) {
            if (*bestCost > cost) {
                *bestCost = cost;
                bestMv->mvx = mvx;
                bestMv->mvy = mvy;
                std::swap(*bestPred, *pred);
            }
        }

        template <typename PixType>
        int32_t Refine8pel(const PixType *src, const PixType *refColoc, int32_t pitchRef, int32_t w, int32_t h, AV1MV *mv, PixType *predictions[2])
        {
            alignas(64) PixType tmpBuf[64 * (64 + 8)]; // 8 additional rows
            PixType *tmp = tmpBuf + 4 * MAX_CU_SIZE;
            const int32_t pitch = 64;
            const int32_t log2w = BSR(w) - 2;
            const int32_t log2h = BSR(h) - 2;

            PixType *&pred = predictions[1];
            PixType *&bestPred = predictions[0];
            int32_t bestCost = INT_MAX;
            AV1MV bestMv;
            const bool isAV1 = true;

            // center for input mv with qpel accuracy
            const PixType *refC = refColoc + (mv->mvx >> 3) + (mv->mvy >> 3) * pitchRef;
            const int32_t dx = (mv->mvx & 7) << 1;
            const int32_t dy = (mv->mvy & 7) << 1;

            AV1PP::interp_av1(refC + ((dx - 2) >> 4) - 4 * pitchRef, pitchRef, tmp - 4 * pitch, (dx - 2) & 15, 0, h + 8, log2w, 0, DEF_FILTER_DUAL, isAV1);
            AV1PP::interp_av1(tmp + ((dy - 2) >> 4) * pitch, pitch, pred, 0, (dy - 2) & 15, h, log2w, 0, DEF_FILTER_DUAL, isAV1);
            int32_t cost = AV1PP::satd(src, pred, log2w, log2h);
            Refine8pelKeepBest(&bestCost, cost, &bestMv, mv->mvx - 1, mv->mvy - 1, &bestPred, &pred);

            AV1PP::interp_av1(tmp, pitch, pred, 0, dy - 0, h, log2w, 0, DEF_FILTER_DUAL, isAV1);
            cost = AV1PP::satd(src, pred, log2w, log2h);
            Refine8pelKeepBest(&bestCost, cost, &bestMv, mv->mvx - 1, mv->mvy, &bestPred, &pred);

            AV1PP::interp_av1(tmp, pitch, pred, 0, dy + 2, h, log2w, 0, DEF_FILTER_DUAL, isAV1);
            cost = AV1PP::satd(src, pred, log2w, log2h);
            Refine8pelKeepBest(&bestCost, cost, &bestMv, mv->mvx - 1, mv->mvy + 1, &bestPred, &pred);

            AV1PP::interp_av1(refC - 4 * pitchRef, pitchRef, tmp - 4 * pitch, dx, 0, h + 8, log2w, 0, DEF_FILTER_DUAL, isAV1);
            AV1PP::interp_av1(tmp + ((dy - 2) >> 4) * pitch, pitch, pred, 0, (dy - 2) & 15, h, log2w, 0, DEF_FILTER_DUAL, isAV1);
            cost = AV1PP::satd(src, pred, log2w, log2h);
            Refine8pelKeepBest(&bestCost, cost, &bestMv, mv->mvx, mv->mvy - 1, &bestPred, &pred);

            AV1PP::interp_av1(tmp, pitch, pred, 0, dy - 0, h, log2w, 0, DEF_FILTER_DUAL, isAV1);
            cost = AV1PP::satd(src, pred, log2w, log2h);
            Refine8pelKeepBest(&bestCost, cost, &bestMv, mv->mvx, mv->mvy, &bestPred, &pred);

            AV1PP::interp_av1(tmp, pitch, pred, 0, dy + 2, h, log2w, 0, DEF_FILTER_DUAL, isAV1);
            cost = AV1PP::satd(src, pred, log2w, log2h);
            Refine8pelKeepBest(&bestCost, cost, &bestMv, mv->mvx, mv->mvy + 1, &bestPred, &pred);

            AV1PP::interp_av1(refC - 4 * pitchRef, pitchRef, tmp - 4 * pitch, dx + 2, 0, h + 8, log2w, 0, DEF_FILTER_DUAL, isAV1);
            AV1PP::interp_av1(tmp + ((dy - 2) >> 4) * pitch, pitch, pred, 0, (dy - 2) & 15, h, log2w, 0, DEF_FILTER_DUAL, isAV1);
            cost = AV1PP::satd(src, pred, log2w, log2h);
            Refine8pelKeepBest(&bestCost, cost, &bestMv, mv->mvx + 1, mv->mvy - 1, &bestPred, &pred);

            AV1PP::interp_av1(tmp, pitch, pred, 0, dy - 0, h, log2w, 0, DEF_FILTER_DUAL, isAV1);
            cost = AV1PP::satd(src, pred, log2w, log2h);
            Refine8pelKeepBest(&bestCost, cost, &bestMv, mv->mvx + 1, mv->mvy, &bestPred, &pred);

            AV1PP::interp_av1(tmp, pitch, pred, 0, dy + 2, h, log2w, 0, DEF_FILTER_DUAL, isAV1);
            cost = AV1PP::satd(src, pred, log2w, log2h);
            Refine8pelKeepBest(&bestCost, cost, &bestMv, mv->mvx + 1, mv->mvy + 1, &bestPred, &pred);

            *mv = bestMv;
            return bestCost;
        }

        template <typename PixType> int32_t RefineBiPred(AV1CU<PixType> &cu, const AV1MEInfo &meInfo, const int8_t refIdx[2],
            AV1MVPair *initmv, const AV1MVPair mvp, bool useHadamard)
        {
            const int32_t pitchRef = cu.m_pitchRecLuma;
            const int32_t pitchSrc = SRC_PITCH;
            const PixType *src = cu.m_ySrc + meInfo.posx + meInfo.posy * pitchSrc;
            const FrameData *refFrame0 = cu.m_currFrame->refFramesVp9[refIdx[0]]->m_recon;
            const FrameData *refFrame1 = cu.m_currFrame->refFramesVp9[refIdx[1]]->m_recon;

            int32_t bestCost = INT_MAX;

            AV1MV mv[2] = { (*initmv)[0], (*initmv)[1] };
            AV1MV bestMv[2] = { (*initmv)[0], (*initmv)[1] };

            const int32_t w = meInfo.width;
            const int32_t h = meInfo.height;
            const int32_t log2w = BSR(w) - 2;
            const int32_t log2h = BSR(h) - 2;
            PixType *predY = cu.vp9scratchpad.predY[0];

            for (int32_t r = 0; r <= 1; r++) {
                int32_t step = (cu.m_par->allowHighPrecisionMv && UseMvHp(mvp[r])) ? 0 : 1;
                for (int32_t dy = -1; dy <= 1; dy++) {
                    for (int32_t dx = -1; dx <= 1; dx++) {
                        mv[r].mvx = int16_t((*initmv)[r].mvx + (dx << step));
                        mv[r].mvy = int16_t((*initmv)[r].mvy + (dy << step));

                        const int32_t intx0 = cu.m_ctbPelX + meInfo.posx + (mv[0].mvx >> 3);
                        const int32_t inty0 = cu.m_ctbPelY + meInfo.posy + (mv[0].mvy >> 3);
                        const int32_t dx0 = (mv[0].mvx << 1) & 15;
                        const int32_t dy0 = (mv[0].mvy << 1) & 15;
                        const PixType *ref0 = reinterpret_cast<PixType*>(refFrame0->y) + intx0 + inty0 * pitchRef;
                        const int32_t intx1 = cu.m_ctbPelX + meInfo.posx + (mv[1].mvx >> 3);
                        const int32_t inty1 = cu.m_ctbPelY + meInfo.posy + (mv[1].mvy >> 3);
                        const int32_t dx1 = (mv[1].mvx << 1) & 15;
                        const int32_t dy1 = (mv[1].mvy << 1) & 15;
                        const PixType *ref1 = reinterpret_cast<PixType*>(refFrame1->y) + intx1 + inty1 * pitchRef;

                        AV1PP::interp_av1(ref0, pitchRef, cu.vp9scratchpad.compPredIm, dx0, dy0, h, log2w, DEF_FILTER_DUAL);
                        AV1PP::interp_av1(ref1, pitchRef, cu.vp9scratchpad.compPredIm, predY, dx1, dy1, h, log2w, DEF_FILTER_DUAL);

                        int32_t cost = 0;
                        if (useHadamard) {
                            cost = AV1PP::satd(src, predY, log2w, log2h);
                        }
                        else {
                            cost = AV1PP::sad_special(src, SRC_PITCH, predY, log2w, log2h);
                        }
                        if (bestCost > cost) {
                            bestCost = cost;
                            bestMv[r] = mv[r];
                        }
                    }
                }
                mv[r] = bestMv[r];
            }

            (*initmv)[0] = bestMv[0];
            (*initmv)[1] = bestMv[1];
            return bestCost;
        }

    };  // namespace


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

    template <typename PixType>
    void AV1CU<PixType>::MePuGacc(AV1MEInfo *meInfo)
    {
        const int32_t w = meInfo->width;
        const int32_t h = meInfo->height;
        const int32_t log2w = BSR(w) - 2;
        const int32_t log2h = BSR(h) - 2;

        const int32_t refYColocOffset = m_ctbPelX + meInfo->posx + (m_ctbPelY + meInfo->posy) * m_pitchRecLuma;
        const PixType *src = m_ySrc + meInfo->posx + meInfo->posy * SRC_PITCH;

        const int32_t predBufOffset = meInfo->posx + meInfo->posy * 64;
        PixType *predictions[5] = {
            vp9scratchpad.interpBufs[0][meInfo->depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[1][meInfo->depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[2][meInfo->depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[3][meInfo->depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[4][meInfo->depth][NEWMV_] + predBufOffset
        };

        uint64_t costs[5] = { ULLONG_MAX, ULLONG_MAX, ULLONG_MAX, ULLONG_MAX, ULLONG_MAX };
        int32_t satds[5];
        uint32_t mvbits[5];
        AV1MVPair mvs[5];
        alignas(2) RefIdx refIdxs[5][2];
        RefIdx bestRefIdx = 0;
        satds[0] = INT_MAX;

        for (RefIdx refIdx = 0; refIdx < 3; refIdx++) {
            if (!m_currFrame->isUniq[refIdx])
                continue;

            const AV1MV &mvp = m_mvRefs.bestMv[refIdx][0];
            const int32_t useMvHp = m_mvRefs.useMvHp[refIdx];

            const FeiOutData * const * gpuMeData = m_currFrame->m_feiInterData[refIdx];

            AV1MV bestMv;
            if (w != h) {
                if (w == 32 || h == 32) {
                    const int32_t x = (m_ctbPelX + meInfo->posx) >> 5;
                    const int32_t y = (m_ctbPelY + meInfo->posy) >> 5;
                    const int32_t pitchData = gpuMeData[2]->m_pitch;
                    const uint32_t *cmData0;
                    const uint32_t *cmData1;
                    if (h == 32) {
                        cmData0 = reinterpret_cast<const uint32_t *>(gpuMeData[2]->m_sysmem + y * pitchData + 64 * (x + 0));
                        cmData1 = reinterpret_cast<const uint32_t *>(gpuMeData[2]->m_sysmem + y * pitchData + 64 * (x + 1));
                    }
                    else {
                        cmData0 = reinterpret_cast<const uint32_t*>(gpuMeData[2]->m_sysmem + (y + 0) * pitchData + 64 * x);
                        cmData1 = reinterpret_cast<const uint32_t*>(gpuMeData[2]->m_sysmem + (y + 1) * pitchData + 64 * x);
                    }
                    const AV1MV *mv0 = reinterpret_cast<const AV1MV *>(cmData0);
                    const AV1MV *mv1 = reinterpret_cast<const AV1MV *>(cmData1);
                    const uint32_t *dist0 = cmData0 + 1 + 4;
                    const uint32_t *dist1 = cmData1 + 1 + 4;

                    const int16_t leftMostX = std::min(mv0->mvx, mv1->mvx);
                    const int16_t rightMostX = std::max(mv0->mvx, mv1->mvx);
                    const int16_t topMostY = std::min(mv0->mvy, mv1->mvy);
                    const int16_t bottomMostY = std::max(mv0->mvy, mv1->mvy);

                    uint64_t bestCost = ULLONG_MAX;
                    for (int16_t mvy = bottomMostY - 1; mvy <= topMostY + 1; mvy++) {
                        int16_t dmvy0 = mvy - mv0->mvy;
                        for (int16_t mvx = rightMostX - 1; mvx <= leftMostX + 1; mvx++) {
                            int16_t dmvx0 = mvx - mv0->mvx;
                            int32_t dist = dist0[dmvx0 + 3 * dmvy0] + dist1[dmvx0 + 3 * dmvy0];
                            int32_t bits = MvCost((mvx << 1) - mvp.mvx, (mvy << 1) - mvp.mvy, useMvHp);
                            uint64_t cost = RD(dist, bits, m_rdLambdaSqrtInt);
                            if (bestCost > cost) {
                                bestCost = cost;
                                bestMv.mvx = mvx;
                                bestMv.mvy = mvy;
                            }
                        }
                    }
                    if (bestCost == ULLONG_MAX)
                        continue;
                }
                else if (w == 16 || h == 16) {
                    const int32_t x = (m_ctbPelX + meInfo->posx) >> 4;
                    const int32_t y = (m_ctbPelY + meInfo->posy) >> 4;
                    const int32_t pitchData = gpuMeData[1]->m_pitch;
                    const uint32_t *cmData0;
                    const uint32_t *cmData1;
                    if (h == 16) {
                        cmData0 = reinterpret_cast<const uint32_t*>(gpuMeData[1]->m_sysmem + y * pitchData + 8 * (x + 0));
                        cmData1 = reinterpret_cast<const uint32_t*>(gpuMeData[1]->m_sysmem + y * pitchData + 8 * (x + 1));
                    }
                    else {
                        cmData0 = reinterpret_cast<const uint32_t*>(gpuMeData[1]->m_sysmem + (y + 0) * pitchData + 8 * x);
                        cmData1 = reinterpret_cast<const uint32_t*>(gpuMeData[1]->m_sysmem + (y + 1) * pitchData + 8 * x);
                    }
                    const AV1MV &mv0 = *(reinterpret_cast<const AV1MV *>(cmData0));
                    const AV1MV &mv1 = *(reinterpret_cast<const AV1MV *>(cmData1));
                    if (mv0 != mv1)
                        continue;
                    bestMv.asInt = mv0.asInt;
                }
                else if (w == 8 || h == 8) {
                    const int32_t x = (m_ctbPelX + meInfo->posx) >> 3;
                    const int32_t y = (m_ctbPelY + meInfo->posy) >> 3;
                    const int32_t pitchData = gpuMeData[0]->m_pitch;
                    const uint32_t *cmData0;
                    const uint32_t *cmData1;
                    if (h == 8) {
                        cmData0 = reinterpret_cast<const uint32_t*>(gpuMeData[0]->m_sysmem + y * pitchData + 8 * (x + 0));
                        cmData1 = reinterpret_cast<const uint32_t*>(gpuMeData[0]->m_sysmem + y * pitchData + 8 * (x + 1));
                    }
                    else {
                        cmData0 = reinterpret_cast<const uint32_t*>(gpuMeData[0]->m_sysmem + (y + 0) * pitchData + 8 * x);
                        cmData1 = reinterpret_cast<const uint32_t*>(gpuMeData[0]->m_sysmem + (y + 1) * pitchData + 8 * x);
                    }
                    const AV1MV &mv0 = *(reinterpret_cast<const AV1MV *>(cmData0));
                    const AV1MV &mv1 = *(reinterpret_cast<const AV1MV *>(cmData1));
                    if (mv0 != mv1)
                        continue;
                    bestMv.asInt = mv0.asInt;
                }
            }
            else if (w == 64) {
                const int32_t x = (m_ctbPelX + meInfo->posx) >> 6;
                const int32_t y = (m_ctbPelY + meInfo->posy) >> 6;
                const int32_t pitchData32 = gpuMeData[2]->m_pitch;
                const uint8_t *cmData32 = gpuMeData[2]->m_sysmem;
                const uint32_t *cmData32_0 = reinterpret_cast<const uint32_t *>(cmData32 + (2 * y + 0) * pitchData32 + 64 * (2 * x + 0));
                const uint32_t *cmData32_1 = reinterpret_cast<const uint32_t *>(cmData32 + (2 * y + 0) * pitchData32 + 64 * (2 * x + 1));
                const uint32_t *cmData32_2 = reinterpret_cast<const uint32_t *>(cmData32 + (2 * y + 1) * pitchData32 + 64 * (2 * x + 0));
                const uint32_t *cmData32_3 = reinterpret_cast<const uint32_t *>(cmData32 + (2 * y + 1) * pitchData32 + 64 * (2 * x + 1));
                const AV1MV *mv0 = reinterpret_cast<const AV1MV *>(cmData32_0);
                const AV1MV *mv1 = reinterpret_cast<const AV1MV *>(cmData32_1);
                const AV1MV *mv2 = reinterpret_cast<const AV1MV *>(cmData32_2);
                const AV1MV *mv3 = reinterpret_cast<const AV1MV *>(cmData32_3);
                const uint32_t *dist0 = cmData32_0 + 1 + 4;
                const uint32_t *dist1 = cmData32_1 + 1 + 4;
                const uint32_t *dist2 = cmData32_2 + 1 + 4;
                const uint32_t *dist3 = cmData32_3 + 1 + 4;

                const int16_t leftMostX = (int16_t)std::min(mv0->mvx, std::min(mv1->mvx, std::min(mv2->mvx, mv3->mvx)));
                const int16_t rightMostX = (int16_t)std::max(mv0->mvx, std::max(mv1->mvx, std::max(mv2->mvx, mv3->mvx)));
                const int16_t topMostY = (int16_t)std::min(mv0->mvy, std::min(mv1->mvy, std::min(mv2->mvy, mv3->mvy)));
                const int16_t bottomMostY = (int16_t)std::max(mv0->mvy, std::max(mv1->mvy, std::max(mv2->mvy, mv3->mvy)));

                uint64_t bestCost = ULLONG_MAX;
                for (int16_t mvy = bottomMostY - 1; mvy <= topMostY + 1; mvy++) {
                    int16_t dmvy0 = mvy - mv0->mvy;
                    int16_t dmvy1 = mvy - mv1->mvy;
                    int16_t dmvy2 = mvy - mv2->mvy;
                    int16_t dmvy3 = mvy - mv3->mvy;
                    for (int16_t mvx = rightMostX - 1; mvx <= leftMostX + 1; mvx++) {
                        int16_t dmvx0 = mvx - mv0->mvx;
                        int16_t dmvx1 = mvx - mv1->mvx;
                        int16_t dmvx2 = mvx - mv2->mvx;
                        int16_t dmvx3 = mvx - mv3->mvx;
                        int32_t dist = dist0[dmvx0 + 3 * dmvy0] + dist1[dmvx1 + 3 * dmvy1]
                            + dist2[dmvx2 + 3 * dmvy2] + dist3[dmvx3 + 3 * dmvy3];
                        int32_t bits = MvCost((mvx << 1) - mvp.mvx, (mvy << 1) - mvp.mvy, useMvHp);
                        uint64_t cost = RD(dist, bits, m_rdLambdaSqrtInt);
                        if (bestCost > cost) {
                            bestCost = cost;
                            bestMv.mvx = mvx;
                            bestMv.mvy = mvy;
                        }
                    }
                }

                const uint32_t *cmData = reinterpret_cast<const uint32_t *>(gpuMeData[3]->m_sysmem + y * gpuMeData[3]->m_pitch + 64 * x);
                const AV1MV *mv = reinterpret_cast<const AV1MV *>(cmData);
                const uint32_t *dists = cmData + 1;

                for (int32_t i = 0; i < 9; i++) {
                    AV1MV mvcand = { (int16_t)(mv->mvx + DXDY2[i][0]), (int16_t)(mv->mvy + DXDY2[i][1]) };
                    int32_t bits = MvCost((mvcand.mvx << 1) - mvp.mvx, (mvcand.mvy << 1) - mvp.mvy, useMvHp);
                    uint64_t cost = RD(dists[i], bits, m_rdLambdaSqrtInt);
                    if (bestCost > cost) {
                        bestCost = cost;
                        bestMv.asInt = mvcand.asInt;
                    }
                }
            }
            else if (w == 32) {
                const int32_t x = (m_ctbPelX + meInfo->posx) >> 5;
                const int32_t y = (m_ctbPelY + meInfo->posy) >> 5;
                const uint32_t *cmData = reinterpret_cast<const uint32_t *>(gpuMeData[2]->m_sysmem + y * gpuMeData[2]->m_pitch + 64 * x);
                const AV1MV *mv = reinterpret_cast<const AV1MV *>(cmData);
                const uint32_t *dists = cmData + 1;

                uint64_t bestCost = ULLONG_MAX;
                for (int32_t i = 0; i < 9; i++) {
                    AV1MV mvcand = { (int16_t)(mv->mvx + DXDY1[i][0]), (int16_t)(mv->mvy + DXDY1[i][1]) };
                    int32_t bits = MvCost((mvcand.mvx << 1) - mvp.mvx, (mvcand.mvy << 1) - mvp.mvy, useMvHp);
                    uint64_t cost = RD(dists[i], bits, m_rdLambdaSqrtInt);
                    if (bestCost > cost) {
                        bestCost = cost;
                        bestMv.asInt = mvcand.asInt;
                    }
                }
            }
            else if (w <= 16) {
                const int32_t x = (m_ctbPelX + meInfo->posx) >> (m_par->Log2MaxCUSize - meInfo->depth);
                const int32_t y = (m_ctbPelY + meInfo->posy) >> (m_par->Log2MaxCUSize - meInfo->depth);
                const int32_t size = 3 - meInfo->depth;
                const int32_t pitchData = gpuMeData[size]->m_pitch;
                const uint32_t *cmData = reinterpret_cast<const uint32_t*>(gpuMeData[size]->m_sysmem + y * pitchData + 8 * x);
                const AV1MV *mv = reinterpret_cast<const AV1MV*>(cmData);
                bestMv.asInt = mv->asInt;
            }

            // now do real interpolation and calculate SATD
            const PixType *refColoc = reinterpret_cast<PixType*>(m_currFrame->refFramesVp9[refIdx]->m_recon->y) + refYColocOffset;
            bestMv.mvx <<= 1;
            bestMv.mvy <<= 1;
            ClipMV_NR(&bestMv);
            if (useMvHp) {
                satds[refIdx] = Refine8pel(src, refColoc, m_pitchRecLuma, w, h, &bestMv, predictions + refIdx);
                mvbits[refIdx] = MvCost(bestMv, mvp, 1);
            }
            else {
                InterpolateAv1SingleRefLuma(refColoc, m_pitchRecLuma, predictions[refIdx], bestMv, h, log2w, DEF_FILTER_DUAL);
                satds[refIdx] = AV1PP::satd(src, predictions[refIdx], log2w, log2h);
                mvbits[refIdx] = MvCost(bestMv, mvp, 0);
            }

            // store current cost
            mvs[refIdx][0] = bestMv;

            mvs[refIdx][1] = MV_ZERO;
            refIdxs[refIdx][0] = refIdx;
            refIdxs[refIdx][1] = NONE_FRAME;
            costs[refIdx] = RD(satds[refIdx], mvbits[refIdx] + m_mvRefs.refFrameBitCount[refIdx], m_rdLambdaSqrtInt);
            if (costs[bestRefIdx] > costs[refIdx])
                bestRefIdx = refIdx;
        }

        if (m_currFrame->compoundReferenceAllowed) {
            const RefIdx compFixed = (RefIdx)m_currFrame->compFixedRef;
            const RefIdx compVar0 = (RefIdx)m_currFrame->compVarRef[0];
            const RefIdx compVar1 = (RefIdx)m_currFrame->compVarRef[1];
            const int32_t compFixedIdx = m_currFrame->refFrameSignBiasVp9[compFixed];
            if (costs[compFixed] < ULLONG_MAX) {
                if (costs[compVar0] < ULLONG_MAX) {
                    AV1PP::average(predictions[compFixed], predictions[compVar0], predictions[COMP_VAR0], h, log2w);
                    mvs[COMP_VAR0][compFixedIdx] = mvs[compFixed][0];
                    mvs[COMP_VAR0][1 - compFixedIdx] = mvs[compVar0][0];
                    refIdxs[COMP_VAR0][compFixedIdx] = compFixed;
                    refIdxs[COMP_VAR0][1 - compFixedIdx] = compVar0;
                    satds[COMP_VAR0] = AV1PP::satd(src, predictions[COMP_VAR0], log2w, log2h);
                    mvbits[COMP_VAR0] = mvbits[compFixed] + mvbits[compVar0];
                    int32_t bits = m_mvRefs.refFrameBitCount[COMP_VAR0] + mvbits[COMP_VAR0];
                    costs[COMP_VAR0] = RD(satds[COMP_VAR0], bits, m_rdLambdaSqrtInt);
                }
                if (costs[compVar1] < ULLONG_MAX) {
                    AV1PP::average(predictions[compFixed], predictions[compVar1], predictions[COMP_VAR1], h, log2w);
                    mvs[COMP_VAR1][compFixedIdx] = mvs[compFixed][0];
                    mvs[COMP_VAR1][1 - compFixedIdx] = mvs[compVar1][0];
                    refIdxs[COMP_VAR1][compFixedIdx] = compFixed;
                    refIdxs[COMP_VAR1][1 - compFixedIdx] = compVar1;
                    satds[COMP_VAR1] = AV1PP::satd(src, predictions[COMP_VAR1], log2w, log2h);
                    mvbits[COMP_VAR1] = mvbits[compFixed] + mvbits[compVar1];
                    int32_t bits = m_mvRefs.refFrameBitCount[COMP_VAR1] + mvbits[COMP_VAR1];
                    costs[COMP_VAR1] = RD(satds[COMP_VAR1], bits, m_rdLambdaSqrtInt);
                }
                const RefIdx bestCompRefIdx = COMP_VAR0 + (costs[COMP_VAR1] < costs[COMP_VAR0]);
                if (costs[bestCompRefIdx] < ULLONG_MAX && m_par->numBiRefineIter > 1) {
                    const RefIdx(&refIdx)[2] = refIdxs[bestCompRefIdx];
                    const AV1MVPair &mvpComp = m_mvRefs.bestMv[bestCompRefIdx];
                    AV1MVPair &mv = mvs[bestCompRefIdx];
                    for (int32_t i = 0; i < m_par->numBiRefineIter - 1; i++)
                        satds[bestCompRefIdx] = RefineBiPred(*this, *meInfo, refIdx, &mv, mvpComp, m_par->hadamardMe >= 2);
                    mvbits[bestCompRefIdx] = MvCost(mv[0], mvpComp[0], m_mvRefs.useMvHp[refIdx[0]])
                        + MvCost(mv[1], mvpComp[1], m_mvRefs.useMvHp[refIdx[1]]);
                    int32_t bits = mvbits[bestCompRefIdx] + m_mvRefs.refFrameBitCount[bestCompRefIdx];
                    costs[bestCompRefIdx] = RD(satds[bestCompRefIdx], bits, m_rdLambdaSqrtInt);
                }
                if (costs[bestRefIdx] > costs[bestCompRefIdx])
                    bestRefIdx = bestCompRefIdx;
            }
        }

        m_interp[0][meInfo->depth][NEWMV_] = predictions[0] - predBufOffset;
        m_interp[1][meInfo->depth][NEWMV_] = predictions[1] - predBufOffset;
        m_interp[2][meInfo->depth][NEWMV_] = predictions[2] - predBufOffset;
        m_interp[3][meInfo->depth][NEWMV_] = predictions[3] - predBufOffset;
        m_interp[4][meInfo->depth][NEWMV_] = predictions[4] - predBufOffset;

        As2B(meInfo->refIdx) = As2B(refIdxs[bestRefIdx]);
        meInfo->mv.asInt64 = mvs[bestRefIdx].asInt64;
        meInfo->refIdxComb = bestRefIdx;
        meInfo->satd = satds[bestRefIdx];
        meInfo->mvBits = mvbits[bestRefIdx];
    }

    template <typename PixType> template <int32_t depth>
    void AV1CU<PixType>::MePuGacc(AV1MEInfo *meInfo)
    {
        const int32_t w = MAX_CU_SIZE >> depth;
        const int32_t h = MAX_CU_SIZE >> depth;
        const int32_t log2w = 4 - depth;
        const int32_t log2h = 4 - depth;

        const int32_t refYColocOffset = m_ctbPelX + meInfo->posx + (m_ctbPelY + meInfo->posy) * m_pitchRecLuma;

        const int32_t predBufOffset = meInfo->posx + meInfo->posy * 64;
        PixType *predictions[5] = {
            vp9scratchpad.interpBufs[0][depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[1][depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[2][depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[3][depth][NEWMV_] + predBufOffset,
            vp9scratchpad.interpBufs[4][depth][NEWMV_] + predBufOffset
        };

        uint64_t costs[5] = { ULLONG_MAX, ULLONG_MAX, ULLONG_MAX, ULLONG_MAX, ULLONG_MAX };
        int32_t satds[5];
        satds[0] = INT_MAX;

        if (depth == 3) {
            const int32_t numBlk = meInfo->posy + (meInfo->posx >> 3);
            const RefIdx *bestRefIdx = m_newMvRefIdx8[numBlk];
            const AV1MVPair &bestMv = m_newMvFromGpu8[numBlk];
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

        }
        else if (depth == 2) {
            const int32_t numBlk = (meInfo->posy >> 2) + (meInfo->posx >> 4);
            const RefIdx *bestRefIdx = m_newMvRefIdx16[numBlk];
            const AV1MVPair &bestMv = m_newMvFromGpu16[numBlk];
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

        }
        else if (depth == 1) {
            const int32_t numBlk = (meInfo->posy >> 5) * 2 + (meInfo->posx >> 5);
            const RefIdx *bestRefIdx = m_newMvRefIdx32[numBlk];
            const AV1MVPair &bestMv = m_newMvFromGpu32[numBlk];
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
        else if (depth == 0) {
            const int32_t numBlk = (meInfo->posy >> 6) + (meInfo->posx >> 6);
            const RefIdx *bestRefIdx = m_newMvRefIdx64[numBlk];
            const AV1MVPair &bestMv = m_newMvFromGpu64[numBlk];
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
    }

    template<typename PixType>
    void InterpolateAv1SingleRefLuma_SwPath(const PixType *refColoc, int32_t refPitch, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp)
    {
        const int32_t intx = mv.mvx >> 3;
        const int32_t inty = mv.mvy >> 3;
        const int32_t dx = (mv.mvx << 1) & 15;
        const int32_t dy = (mv.mvy << 1) & 15;
        refColoc += intx + inty * refPitch;

        const AV1Enc::InterpKernel *kernel = AV1Enc::av1_filter_kernels[EIGHTTAP];
        AV1PP::interp_pitch64_fptr_arr[log2w][dx != 0][dy != 0][0]((const uint8_t*)refColoc, refPitch, (uint8_t*)dst, kernel[dx], kernel[dy], h);
    }

#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
    template <typename PixType> template <int32_t depth>
    void AV1CU<PixType>::MePuGacc_SwPath(AV1MEInfo *meInfo)
    {
        const int32_t w = MAX_CU_SIZE >> depth;
        const int32_t h = MAX_CU_SIZE >> depth;
        const int32_t log2w = 4 - depth;
        const int32_t log2h = 4 - depth;

        const int32_t refYColocOffset = m_ctbPelX + meInfo->posx + (m_ctbPelY + meInfo->posy) * m_pitchRecLuma;

        const int32_t secondRef = m_currFrame->compoundReferenceAllowed
            ? ALTREF_FRAME
            : m_currFrame->isUniq[GOLDEN_FRAME] ? GOLDEN_FRAME : LAST_FRAME;
        const int32_t sizeOfElement = w < 32 ? 8 : 64;
        const int32_t pitch0 = m_currFrame->m_feiInterData[LAST_FRAME][log2w - 1]->m_pitch;
        const int32_t pitch1 = m_currFrame->m_feiInterData[secondRef][log2w - 1]->m_pitch;
        const uint8_t *buf0 = m_currFrame->m_feiInterData[LAST_FRAME][log2w - 1]->m_sysmem;
        const uint8_t *buf1 = m_currFrame->m_feiInterData[secondRef][log2w - 1]->m_sysmem;
        const int32_t x = (m_ctbPelX + meInfo->posx) >> (6 - depth);
        const int32_t y = (m_ctbPelY + meInfo->posy) >> (6 - depth);
        const GpuMeData *meData[2] = {
            (const GpuMeData *)(buf0 + y * pitch0 + x * sizeOfElement),
            (const GpuMeData *)(buf1 + y * pitch1 + x * sizeOfElement)
        };
        const int32_t gpuRefIdx = meData[0]->idx & 3;
        assert(gpuRefIdx < 3);
        if (gpuRefIdx == 2) {
            meInfo->refIdx[0] = LAST_FRAME;
            meInfo->refIdx[1] = ALTREF_FRAME;
            meInfo->refIdxComb = COMP_VAR0;
            meInfo->mv[0].mvx = meData[0]->mv.mvx << 1;
            meInfo->mv[0].mvy = meData[0]->mv.mvy << 1;
            meInfo->mv[1].mvx = meData[1]->mv.mvx << 1;
            meInfo->mv[1].mvy = meData[1]->mv.mvy << 1;
        }
        else {
            meInfo->refIdx[0] = gpuRefIdx ? secondRef : LAST_FRAME;
            meInfo->refIdx[1] = NONE_FRAME;
            meInfo->refIdxComb = meInfo->refIdx[0];
            meInfo->mv[0].mvx = meData[gpuRefIdx]->mv.mvx << 1;
            meInfo->mv[0].mvy = meData[gpuRefIdx]->mv.mvy << 1;
            meInfo->mv[1].asInt = 0;
        }
        meInfo->satd = meData[0]->idx >> 2;
        meInfo->mvBits = MvCost(meInfo->mv[0], m_mvRefs.bestMv[meInfo->refIdxComb][0], 0);
        if (meInfo->refIdxComb == COMP_VAR0)
            meInfo->mvBits += MvCost(meInfo->mv[1], m_mvRefs.bestMv[meInfo->refIdxComb][1], 0);

        m_interp[0][depth][NEWMV_] = NULL;
        m_interp[1][depth][NEWMV_] = NULL;
        m_interp[2][depth][NEWMV_] = NULL;
        m_interp[3][depth][NEWMV_] = NULL;
        m_interp[4][depth][NEWMV_] = NULL;

        PixType *interp = (depth == 3) ? m_newMvInterp8 : (depth == 2) ? m_newMvInterp16 :
            (depth == 1) ? m_newMvInterp32 : m_newMvInterp64;
        m_interp[meInfo->refIdxComb][depth][NEWMV_] = interp;
        interp += (meInfo->posx & 63) + (meInfo->posy & 63) * 64;

        const PixType *refColoc0 = (const PixType *)(m_currFrame->refFramesVp9[meInfo->refIdx[0]]->m_recon->y) + refYColocOffset;
        AV1MVPair clippedMv = meInfo->mv;
        ClipMV(&clippedMv[0]);
        ClipMV(&clippedMv[1]);
        alignas(32) PixType tmp[64 * 64];
        if (meInfo->refIdxComb == COMP_VAR0) {
            const PixType *refColoc1 = (const PixType *)(m_currFrame->refFramesVp9[meInfo->refIdx[1]]->m_recon->y) + refYColocOffset;
            InterpolateAv1SingleRefLuma_SwPath(refColoc0, m_pitchRecLuma, interp, clippedMv[0], h, log2w, EIGHTTAP_EIGHTTAP);
            InterpolateAv1SingleRefLuma_SwPath(refColoc1, m_pitchRecLuma, tmp, clippedMv[1], h, log2w, EIGHTTAP_EIGHTTAP);
            AV1PP::average(interp, tmp, interp, h, log2w);
        }
        else {
            InterpolateAv1SingleRefLuma_SwPath(refColoc0, m_pitchRecLuma, interp, clippedMv[0], h, log2w, EIGHTTAP_EIGHTTAP);
        }
    }
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

    namespace {
        int32_t FetchCandidates(int32_t sbType, int32_t miRow, int32_t miCol, int32_t miColTileStart, const ModeInfo *mi,
            int32_t miPitch, const ModeInfo *miColoc, const ModeInfo *candidates[9], MemCtx *memCtx)
        {
            const int8_t(&mv_ref_search)[MVREF_NEIGHBOURS][2] = mv_ref_blocks[sbType];

            int32_t contextCounter = 0;
            int32_t ncand = 0;
            if (miRow + mv_ref_search[0][0] >= 0 && miCol + mv_ref_search[0][1] >= miColTileStart) {
                const ModeInfo *miCand = mi + mv_ref_search[0][1] + mv_ref_search[0][0] * miPitch;
                candidates[ncand++] = miCand;
                contextCounter = mode_2_counter[miCand->mode];
            }
            if (miRow + mv_ref_search[1][0] >= 0 && miCol + mv_ref_search[1][1] >= miColTileStart) {
                const ModeInfo *miCand = mi + mv_ref_search[1][1] + mv_ref_search[1][0] * miPitch;
                candidates[ncand++] = miCand;
                contextCounter += mode_2_counter[miCand->mode];
            }
            memCtx->interMode = counter_to_context[contextCounter];

            for (int32_t i = 2; i < MVREF_NEIGHBOURS; i++)
                if (miRow + mv_ref_search[i][0] >= 0 && miCol + mv_ref_search[i][1] >= miColTileStart)
                    candidates[ncand++] = mi + mv_ref_search[i][1] + mv_ref_search[i][0] * miPitch;
            if (miColoc)
                candidates[ncand++] = miColoc;

            return ncand;
        }


        AV1MV ScaleMv(const Frame &frame, RefIdx refFrame, AV1MV candMv, RefIdx candFrame) {
            if (frame.refFrameSignBiasVp9[candFrame] != frame.refFrameSignBiasVp9[refFrame]) {
                candMv.mvx = -candMv.mvx;
                candMv.mvy = -candMv.mvy;
            }
            return candMv;
        }

        void ScaleMv(const AV1MV &in, AV1MV *out, int16_t negate) {
            out->mvx = in.mvx ^ -negate;
            out->mvx += negate;
            out->mvy = in.mvy ^ -negate;
            out->mvy += negate;
            // same as:
            //if (frame.refFrameSignBias[candFrame] != frame.refFrameSignBias[refFrame]) {
            //    candMv.mvx = -candMv.mvx;
            //    candMv.mvy = -candMv.mvy;
            //}
            //return candMv;
        }

        void AddMv(MvRefs* refs, int32_t at, AV1MV mv, RefIdx refIdx, PredMode mode) {
            refs->mv[at][0] = mv;
            refs->refIdx[at][0] = refIdx;
            refs->refIdx[at][1] = NONE_FRAME;
            refs->mode[at] = mode;
        }
        void AddMv(MvRefs* refs, int32_t at, AV1MV mv0, AV1MV mv1, RefIdx refIdx0, RefIdx refIdx1, PredMode mode) {
            refs->mv[at][0] = mv0;
            refs->mv[at][1] = mv1;
            refs->refIdx[at][0] = refIdx0;
            refs->refIdx[at][1] = refIdx1;
            refs->mode[at] = mode;
        }
    };

    template void AV1CU<uint8_t>::InitCu_SwPath(const AV1VideoParam &par, ModeInfo *_data, ModeInfo *_dataTemp, int32_t ctbRow, int32_t ctbCol, const Frame &frame, CoeffsType *m_coeffWork);
    template void AV1CU<uint8_t>::ModeDecisionInterTu7_SecondPass(int32_t miRow, int32_t miCol);
#endif

#if ENABLE_10BIT
    template void AV1CU<uint16_t>::RefineDecisionAV1();
    template void AV1CU<uint16_t>::RefineDecisionIAV1();
    template void AV1CU<uint16_t>::InitCu_SwPath(const AV1VideoParam &par, ModeInfo *_data, ModeInfo *_dataTemp, int32_t ctbRow, int32_t ctbCol, const Frame &frame, CoeffsType *m_coeffWork);
    template void AV1CU<uint16_t>::ModeDecisionInterTu7_SecondPass(int32_t miRow, int32_t miCol);
    template void AV1CU<uint16_t>::ModeDecision(int, int);
    template void AV1CU<uint16_t>::InitCu(const AV1VideoParam &_par, ModeInfo *_data, ModeInfo *_dataTemp, int32_t ctbRow, int32_t ctbCol, const Frame &frame, CoeffsType *m_coeffWork, uint8_t* lcuRound);
#endif
}  // namespace AV1Enc

#endif  // MFX_ENABLE_AV1_VIDEO_ENCODE

