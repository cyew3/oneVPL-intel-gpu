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
#include "mfx_av1_tables.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_enc.h"

using namespace H265Enc;

namespace {

    enum { L = LAST_FRAME, G = GOLDEN_FRAME, A = ALTREF_FRAME, C = COMP_VAR0 };

    H265MVPair ClampedMvPair(const H265MVPair &mvs, H265MV minMv, H265MV maxMv) {
        __m128i minmv = _mm_set1_epi32(minMv.asInt);
        __m128i maxmv = _mm_set1_epi32(maxMv.asInt);
        __m128i cmv = loadl_epi64(&mvs);
        cmv = _mm_min_epi16(cmv, maxmv);
        cmv = _mm_max_epi16(cmv, minmv);
        H265MVPair clamped;
        clamped.asInt64 = _mm_cvtsi128_si64(cmv);
        return clamped;
    }

    H265MV ClampedMv(const H265MV &mv, H265MV minMv, H265MV maxMv) {
        __m128i minmv = loadu_si32(&minMv);
        __m128i maxmv = loadu_si32(&maxMv);
        __m128i cmv = loadu_si32(&mv);
        cmv = _mm_min_epi16(cmv, maxmv);
        cmv = _mm_max_epi16(cmv, minmv);
        H265MV clamped;
        clamped.asInt = _mm_cvtsi128_si32(cmv);
        return clamped;
    }

    H265MV NegateMv(const H265MV in, int32_t negate) {
        __m128i mv = _mm_cvtsi32_si128(in.asInt);
        __m128i pred = _mm_cvtsi32_si128(negate);
        mv = _mm_xor_si128(mv, pred);
        mv = _mm_sub_epi16(mv, pred);
        H265MV out;
        out.asInt = _mm_cvtsi128_si32(mv);
        return out;
    }


    template <bool nearestMv>
    int32_t AddDistinctMvToStack(const H265MV &mv, int32_t weight, H265MVCand *refMvStack, int32_t *refMvCount)
    {
        int32_t index = 0;
        for (; index < *refMvCount; ++index)
            if (refMvStack[index].mv[0].asInt == mv.asInt)
                return refMvStack[index].weight += weight, 0;

        if (*refMvCount == MAX_REF_MV_STACK_SIZE)
            return 0;

        refMvStack[index].mv[0].asInt = mv.asInt;
        refMvStack[index].weight = weight + (nearestMv ? REF_CAT_LEVEL : 0);
        refMvStack[index].predDiff[0] = 2;
        ++(*refMvCount);
        return 1;
    }

    template <bool nearestMv>
    int32_t AddDistinctMvPairToStack(const H265MVPair &mvs, int32_t weight, H265MVCand *refMvStack, int32_t *refMvCount)
    {
        int32_t index = 0;
        for (; index < *refMvCount; ++index)
            if (refMvStack[index].mv.asInt64 == mvs.asInt64)
                return refMvStack[index].weight += weight, 0;

        if (*refMvCount == MAX_REF_MV_STACK_SIZE)
            return 0;

        refMvStack[index].mv.asInt64 = mvs.asInt64;
        refMvStack[index].predDiffAsInt = 0x20002;
        refMvStack[index].weight = weight + (nearestMv ? REF_CAT_LEVEL : 0);
        ++(*refMvCount);
        return 1;
    }

    void AddNearestRefMvCandidateTU7P(const ModeInfo *cand, int32_t *refMvCount, int32_t *newMvCount,
                                      int32_t *refMatchCount, H265MVCand (*refMvStack)[MAX_REF_MV_STACK_SIZE],
                                      int32_t weight)
    {
        const int32_t newMvMode = (cand->mode == AV1_NEWMV);
        if (cand->refIdx[0] == L) {
            newMvCount[L] += newMvMode;
            refMatchCount[L]++;
            AddDistinctMvToStack<true>(cand->mv[0], weight, refMvStack[L], refMvCount + L);
        } else if (cand->refIdx[0] == G) {
            newMvCount[G] += newMvMode;
            refMatchCount[G]++;
            AddDistinctMvToStack<true>(cand->mv[0], weight, refMvStack[G], refMvCount + G);
        }
    }

    void AddTRefMvCandidateTU7B(const ModeInfo *cand, int32_t *refMvCount, int32_t *newMvCount,
        int32_t *refMatchCount, H265MVCand(*refMvStack)[MAX_REF_MV_STACK_SIZE],
        int32_t weight)
    {
        //const int32_t newMvMode = (cand->mode == AV1_NEWMV);
        //const int32_t newMvMode = have_newmv_in_inter_mode(cand->mode);
        if (cand->refIdx[0] == L) {
            //newMvCount[L] += newMvMode;
            //refMatchCount[L]++;
            AddDistinctMvToStack<false>(cand->mv[0], weight, refMvStack[L], refMvCount + L);
            if (cand->refIdx[1] == A) {
                //newMvCount[A] += newMvMode;
                //newMvCount[C] += newMvMode;
                //refMatchCount[A]++;
                //refMatchCount[C]++;
                AddDistinctMvToStack<false>(cand->mv[1], weight, refMvStack[A], refMvCount + A);
                AddDistinctMvPairToStack<false>(cand->mv, weight, refMvStack[C], refMvCount + C);
            }
        }
        else if (cand->refIdx[0] == A) {
            //newMvCount[A] += newMvMode;
            //refMatchCount[A]++;
            AddDistinctMvToStack<false>(cand->mv[0], weight, refMvStack[A], refMvCount + A);
        }
    }

    void AddNearestRefMvCandidateTU7B(const ModeInfo *cand, int32_t *refMvCount, int32_t *newMvCount,
                                      int32_t *refMatchCount, H265MVCand (*refMvStack)[MAX_REF_MV_STACK_SIZE],
                                      int32_t weight)
    {
        //const int32_t newMvMode = (cand->mode == AV1_NEWMV);
        const int32_t newMvMode = have_newmv_in_inter_mode(cand->mode);
        if (cand->refIdx[0] == L) {
            newMvCount[L] += newMvMode;
            refMatchCount[L]++;
            AddDistinctMvToStack<true>(cand->mv[0], weight, refMvStack[L], refMvCount + L);
            if (cand->refIdx[1] == A) {
                newMvCount[A] += newMvMode;
                newMvCount[C] += newMvMode;
                refMatchCount[A]++;
                refMatchCount[C]++;
                AddDistinctMvToStack<true>(cand->mv[1], weight, refMvStack[A], refMvCount + A);
                AddDistinctMvPairToStack<true>(cand->mv, weight, refMvStack[C], refMvCount + C);
            }
        } else if (cand->refIdx[0] == A) {
            newMvCount[A] += newMvMode;
            refMatchCount[A]++;
            AddDistinctMvToStack<true>(cand->mv[0], weight, refMvStack[A], refMvCount + A);
        }
    }

    void AddOuterRefMvCandidateTU7P(const ModeInfo *cand, int32_t *refMvCount, int32_t *refMatchCount,
                                    H265MVCand (*refMvStack)[MAX_REF_MV_STACK_SIZE],
                                    int32_t weight)
    {
        if (cand->refIdx[0] == L) {
            refMatchCount[L]++;
            AddDistinctMvToStack<false>(cand->mv[0], weight, refMvStack[L], refMvCount + L);
        } else if (cand->refIdx[0] == G) {
            refMatchCount[G]++;
            AddDistinctMvToStack<false>(cand->mv[0], weight, refMvStack[G], refMvCount + G);
        }
    }

    void AddTRefMvCandidateTU7P(const ModeInfo *cand, int32_t *refMvCount, int32_t *refMatchCount,
        H265MVCand(*refMvStack)[MAX_REF_MV_STACK_SIZE],
        int32_t weight)
    {
        if (cand->refIdx[0] == L) {
            //refMatchCount[L]++;
            AddDistinctMvToStack<false>(cand->mv[0], weight, refMvStack[L], refMvCount + L);
        }
        else if (cand->refIdx[0] == G) {
            //refMatchCount[G]++;
            AddDistinctMvToStack<false>(cand->mv[0], weight, refMvStack[G], refMvCount + G);
        }
    }

    void AddOuterRefMvCandidateTU7B(const ModeInfo *cand, int32_t *refMvCount, int32_t *refMatchCount,
                                    H265MVCand (*refMvStack)[MAX_REF_MV_STACK_SIZE],
                                    int32_t weight)
    {
        if (cand->refIdx[0] == L) {
            refMatchCount[L]++;
            AddDistinctMvToStack<false>(cand->mv[0], weight, refMvStack[L], refMvCount + L);
            if (cand->refIdx[1] == A) {
                refMatchCount[A]++;
                refMatchCount[C]++;
                AddDistinctMvToStack<false>(cand->mv[1], weight, refMvStack[A], refMvCount + A);
                AddDistinctMvPairToStack<false>(cand->mv, weight, refMvStack[C], refMvCount + C);
            }
        } else if (cand->refIdx[0] == A) {
            refMatchCount[A]++;
            AddDistinctMvToStack<false>(cand->mv[0], weight, refMvStack[A], refMvCount + A);
        }
    }

    int16_t GetModeContext(int32_t nearestMatchCount, int32_t refMatchCount, int32_t newMvCount, int32_t colBlkAvail)
    {
        int16_t modeContext = 0;
        if (colBlkAvail == 0)
            modeContext = (1 << GLOBALMV_OFFSET);

        switch (nearestMatchCount) {
        case 0:
            modeContext |= 0;
            if (refMatchCount >= 1)
                modeContext |= 1;
            if (refMatchCount == 1)
                modeContext |= (1 << REFMV_OFFSET);
            else if (refMatchCount >= 2)
                modeContext |= (2 << REFMV_OFFSET);
            break;
        case 1:
            modeContext |= (newMvCount > 0) ? 2 : 3;
            if (refMatchCount == 1)
                modeContext |= (3 << REFMV_OFFSET);
            else if (refMatchCount >= 2)
                modeContext |= (4 << REFMV_OFFSET);
            break;
        case 2:
        default:
            if (newMvCount > 0)
                modeContext |= 4;
            else
                modeContext |= 6;
            modeContext |= (5 << REFMV_OFFSET);
            break;
        }
        return modeContext;
    }


    void SortStack(H265MVCand *refMvStack, int32_t refMvCount)
    {
        int32_t len, nrLen;
        for (len = refMvCount; len > 0; len = nrLen) {
            nrLen = 0;
            for (int32_t i = 1; i < len; ++i) {
                if (refMvStack[i - 1].weight < refMvStack[i].weight) {
                    std::swap(refMvStack[i - 1], refMvStack[i]);
                    nrLen = i;
                }
            }
        }
    }

    void lower_mv_precision(H265MV *mv)
    {
        if (mv->mvy & 1) mv->mvy += (mv->mvy > 0 ? -1 : 1);
        if (mv->mvx & 1) mv->mvx += (mv->mvx > 0 ? -1 : 1);
    }

    int32_t add_tpl_ref_mv(const TileBorders &tileBorders, const Frame &frame, int32_t orderHintBits, int32_t miPitch,
                          int32_t miRow, int32_t miCol, int32_t ref_frame, int32_t blk_row, int32_t blk_col,
                          int32_t *refMvCount, H265MVCand (*refMvStack)[MAX_REF_MV_STACK_SIZE],
                          int32_t *mode_context)
    {
        int idx;
        int coll_blk_count = 0;
        const int weight_unit = 1;

        if (miCol + blk_col < tileBorders.colStart || miCol + blk_col >= tileBorders.colEnd ||
            miRow + blk_row < tileBorders.rowStart || miRow + blk_row >= tileBorders.rowEnd)
            return 0;

        const TplMvRef *prev_frame_mvs = frame.m_tplMvs + (miRow + blk_row) * miPitch + (miCol + blk_col);
        const int32_t cur_frame_index = frame.curFrameOffset;

        if (ref_frame < C) {
            int32_t frame0_index = frame.refFramesVp9[ref_frame]->curFrameOffset;
            int32_t cur_offset_0 = GetRelativeDist(orderHintBits, cur_frame_index, frame0_index);
            H265MVCand *ref_mv_stack = refMvStack[ref_frame];

            for (int32_t i = 0; i < MFMV_STACK_SIZE; ++i) {
                if (prev_frame_mvs->mfmv0[i] != INVALID_MV) {
                    H265MV this_refmv;

                    GetMvProjection(&this_refmv, prev_frame_mvs->mfmv0[i], cur_offset_0, prev_frame_mvs->ref_frame_offset[i]);

                    lower_mv_precision(&this_refmv);

                    if (blk_row == 0 && blk_col == 0)
                        if (abs(this_refmv.mvx) >= 16 || abs(this_refmv.mvy) >= 16)
                            mode_context[ref_frame] |= (1 << GLOBALMV_OFFSET);

                    for (idx = 0; idx < refMvCount[ref_frame]; ++idx)
                        if (this_refmv == ref_mv_stack[idx].mv[0])
                            break;

                    if (idx < refMvCount[ref_frame])
                        ref_mv_stack[idx].weight += 2 * weight_unit;

                    if (idx == refMvCount[ref_frame] && refMvCount[ref_frame] < MAX_REF_MV_STACK_SIZE) {
                        ref_mv_stack[idx].mv[0] = this_refmv;
                        // TODO(jingning): Hard coded context number. Need to make it better
                        // sense.
                        ref_mv_stack[idx].predDiff[0] = 1;
                        ref_mv_stack[idx].weight = 2 * weight_unit;
                        ++refMvCount[ref_frame];
                    }

                    ++coll_blk_count;
                    return coll_blk_count;
                }
            }
        } else {
            // Process compound inter mode
            int frame0_index = frame.refFramesVp9[L]->curFrameOffset;
            int frame1_index = frame.refFramesVp9[A]->curFrameOffset;
            int cur_offset_0 = GetRelativeDist(orderHintBits, cur_frame_index, frame0_index);
            int cur_offset_1 = GetRelativeDist(orderHintBits, cur_frame_index, frame1_index);
            H265MVCand *ref_mv_stack = refMvStack[C];

            for (int i = 0; i < MFMV_STACK_SIZE; ++i) {
                if (prev_frame_mvs->mfmv0[i] != INVALID_MV) {
                    H265MV this_refmv;
                    H265MV comp_refmv;
                    GetMvProjection(&this_refmv, prev_frame_mvs->mfmv0[i], cur_offset_0, prev_frame_mvs->ref_frame_offset[i]);
                    GetMvProjection(&comp_refmv, prev_frame_mvs->mfmv0[i], cur_offset_1, prev_frame_mvs->ref_frame_offset[i]);

                    lower_mv_precision(&this_refmv);
                    lower_mv_precision(&comp_refmv);

                    if (blk_row == 0 && blk_col == 0)
                        if (abs(this_refmv.mvx) >= 16 || abs(this_refmv.mvy) >= 16 ||
                            abs(comp_refmv.mvx) >= 16 || abs(comp_refmv.mvy) >= 16)
                            mode_context[ref_frame] |= (1 << GLOBALMV_OFFSET);

                    for (idx = 0; idx < refMvCount[ref_frame]; ++idx)
                        if (this_refmv == ref_mv_stack[idx].mv[0] && comp_refmv == ref_mv_stack[idx].mv[1])
                            break;

                    if (idx < refMvCount[ref_frame])
                        ref_mv_stack[idx].weight += 2 * weight_unit;

                    if (idx == refMvCount[ref_frame] && refMvCount[ref_frame] < MAX_REF_MV_STACK_SIZE) {
                        ref_mv_stack[idx].mv[0] = this_refmv;
                        ref_mv_stack[idx].mv[1] = comp_refmv;
                        // TODO(jingning): Hard coded context number. Need to make it better
                        // sense.
                        ref_mv_stack[idx].predDiff[0] = 1;
                        ref_mv_stack[idx].predDiff[1] = 1;
                        ref_mv_stack[idx].weight = 2 * weight_unit;
                        ++refMvCount[ref_frame];
                    }

                    ++coll_blk_count;
                    return coll_blk_count;
                }
            }
        }

        return coll_blk_count;
    }

    int32_t check_sb_border(const int mi_row, const int mi_col, const int row_offset, const int col_offset)
    {
        const int32_t row = mi_row & 7;
        const int32_t col = mi_col & 7;
        return !(row + row_offset < 0 || row + row_offset >= 8 || col + col_offset < 0 || col + col_offset >= 8);
    }

    void ExtendRefMvStack(const Frame &frame, const ModeInfo *mi, int32_t miPitch, int32_t ref_frame, int32_t size8x8,
                          int32_t max_row_offset, int32_t max_col_offset, AV1MvRefs *mvRefs, H265MV minMv, H265MV maxMv,
                          H265MV (*mvRefList)[MAX_MV_REF_CANDIDATES])
    {
        if (ref_frame == C) {
            RefIdx rf[2] = { LAST_FRAME, ALTREF_FRAME };
            if (mvRefs->refMvCount[ref_frame] < 2) {
                H265MV ref_id[2][2], ref_diff[2][2];
                int ref_id_count[2] = { 0 }, ref_diff_count[2] = { 0 };

                for (int idx = 0; abs(max_row_offset) >= 1 && idx < size8x8;) {
                    const ModeInfo *cand = mi - miPitch + idx;

                    for (int rf_idx = 0; rf_idx < 2; ++rf_idx) {
                        RefIdx can_rf = cand->refIdx[rf_idx];

                        for (int cmp_idx = 0; cmp_idx < 2; ++cmp_idx) {
                            if (can_rf == rf[cmp_idx] && ref_id_count[cmp_idx] < 2) {
                                ref_id[cmp_idx][ref_id_count[cmp_idx]] = cand->mv[rf_idx];
                                ++ref_id_count[cmp_idx];
                            } else if (can_rf > INTRA_FRAME && ref_diff_count[cmp_idx] < 2) {
                                H265MV this_mv = cand->mv[rf_idx];
                                if (frame.refFrameSignBiasVp9[can_rf] != frame.refFrameSignBiasVp9[rf[cmp_idx]]) {
                                    this_mv.mvx = -this_mv.mvx;
                                    this_mv.mvy = -this_mv.mvy;
                                }
                                ref_diff[cmp_idx][ref_diff_count[cmp_idx]] = this_mv;
                                ++ref_diff_count[cmp_idx];
                            }
                        }
                    }
                    idx += block_size_wide_8x8[cand->sbType];
                }

                for (int idx = 0; abs(max_col_offset) >= 1 && idx < size8x8;) {
                    const ModeInfo *cand = mi + miPitch * idx - 1;

                    for (int rf_idx = 0; rf_idx < 2; ++rf_idx) {
                        RefIdx can_rf = cand->refIdx[rf_idx];

                        for (int cmp_idx = 0; cmp_idx < 2; ++cmp_idx) {
                            if (can_rf == rf[cmp_idx] && ref_id_count[cmp_idx] < 2) {
                                ref_id[cmp_idx][ref_id_count[cmp_idx]] = cand->mv[rf_idx];
                                ++ref_id_count[cmp_idx];
                            } else if (can_rf > INTRA_FRAME && ref_diff_count[cmp_idx] < 2) {
                                H265MV this_mv = cand->mv[rf_idx];
                                if (frame.refFrameSignBiasVp9[can_rf] != frame.refFrameSignBiasVp9[rf[cmp_idx]]) {
                                    this_mv.mvy = -this_mv.mvy;
                                    this_mv.mvx = -this_mv.mvx;
                                }
                                ref_diff[cmp_idx][ref_diff_count[cmp_idx]] = this_mv;
                                ++ref_diff_count[cmp_idx];
                            }
                        }
                    }
                    idx += block_size_wide_8x8[cand->sbType];
                }

                // Build up the compound mv predictor
                H265MV comp_list[3][2];

                for (int idx = 0; idx < 2; ++idx) {
                    int comp_idx = 0;
                    for (int list_idx = 0; list_idx < ref_id_count[idx] && comp_idx < 3; ++list_idx, ++comp_idx)
                        comp_list[comp_idx][idx] = ref_id[idx][list_idx];
                    for (int list_idx = 0; list_idx < ref_diff_count[idx] && comp_idx < 3; ++list_idx, ++comp_idx)
                        comp_list[comp_idx][idx] = ref_diff[idx][list_idx];
                    for (; comp_idx < 3; ++comp_idx)
                        comp_list[comp_idx][idx] = MV_ZERO;
                }

                if (mvRefs->refMvCount[ref_frame]) {
                    assert(mvRefs->refMvCount[ref_frame] == 1);
                    if (comp_list[0][0] == mvRefs->refMvStack[ref_frame][0].mv[0] &&
                        comp_list[0][1] == mvRefs->refMvStack[ref_frame][0].mv[1])
                    {
                        mvRefs->refMvStack[ref_frame][mvRefs->refMvCount[ref_frame]].mv[0] = comp_list[1][0];
                        mvRefs->refMvStack[ref_frame][mvRefs->refMvCount[ref_frame]].mv[1] = comp_list[1][1];
                    } else {
                        mvRefs->refMvStack[ref_frame][mvRefs->refMvCount[ref_frame]].mv[0] = comp_list[0][0];
                        mvRefs->refMvStack[ref_frame][mvRefs->refMvCount[ref_frame]].mv[1] = comp_list[0][1];
                    }
                    mvRefs->refMvStack[ref_frame][mvRefs->refMvCount[ref_frame]].weight = 2;
                    ++mvRefs->refMvCount[ref_frame];
                } else {
                    for (int idx = 0; idx < MAX_MV_REF_CANDIDATES; ++idx) {
                        mvRefs->refMvStack[ref_frame][mvRefs->refMvCount[ref_frame]].mv[0] = comp_list[idx][0];
                        mvRefs->refMvStack[ref_frame][mvRefs->refMvCount[ref_frame]].mv[1] = comp_list[idx][1];
                        mvRefs->refMvStack[ref_frame][mvRefs->refMvCount[ref_frame]].weight = 2;
                        ++mvRefs->refMvCount[ref_frame];
                    }
                }
            }

            assert(mvRefs->refMvCount[ref_frame] >= 2);

            for (int idx = 0; idx < mvRefs->refMvCount[ref_frame]; ++idx)
                mvRefs->refMvStack[ref_frame][idx].mv = ClampedMvPair(mvRefs->refMvStack[ref_frame][idx].mv, minMv, maxMv);
        } else {
            for (int idx = 0; abs(max_row_offset) >= 1 && idx < size8x8 &&
                                mvRefs->refMvCount[ref_frame] < MAX_MV_REF_CANDIDATES;) {
                const ModeInfo *cand = mi - miPitch + idx;
                for (int rf_idx = 0; rf_idx < 2; ++rf_idx) {
                    if (cand->refIdx[rf_idx] > INTRA_FRAME) {
                        H265MV this_mv = cand->mv[rf_idx];
                        if (frame.refFrameSignBiasVp9[cand->refIdx[rf_idx]] != frame.refFrameSignBiasVp9[ref_frame]) {
                            this_mv.mvy = -this_mv.mvy;
                            this_mv.mvx = -this_mv.mvx;
                        }
                        int stack_idx;
                        for (stack_idx = 0; stack_idx < mvRefs->refMvCount[ref_frame]; ++stack_idx) {
                            H265MV stack_mv = mvRefs->refMvStack[ref_frame][stack_idx].mv[0];
                            if (this_mv == stack_mv)
                                break;
                        }

                        if (stack_idx == mvRefs->refMvCount[ref_frame]) {
                            mvRefs->refMvStack[ref_frame][stack_idx].mv[0] = this_mv;
                            mvRefs->refMvStack[ref_frame][stack_idx].weight = 2; // arbitrary small value
                            ++mvRefs->refMvCount[ref_frame];
                        }
                    }
                }
                idx += block_size_wide_8x8[cand->sbType];
            }
            for (int idx = 0; abs(max_col_offset) >= 1 && idx < size8x8 &&
                                mvRefs->refMvCount[ref_frame] < MAX_MV_REF_CANDIDATES;) {
                const ModeInfo *cand = mi + miPitch * idx - 1;
                for (int rf_idx = 0; rf_idx < 2; ++rf_idx) {
                    if (cand->refIdx[rf_idx] > INTRA_FRAME) {
                        H265MV this_mv = cand->mv[rf_idx];
                        if (frame.refFrameSignBiasVp9[cand->refIdx[rf_idx]] != frame.refFrameSignBiasVp9[ref_frame]) {
                            this_mv.mvy = -this_mv.mvy;
                            this_mv.mvx = -this_mv.mvx;
                        }
                        int stack_idx;
                        for (stack_idx = 0; stack_idx < mvRefs->refMvCount[ref_frame]; ++stack_idx) {
                            H265MV stack_mv = mvRefs->refMvStack[ref_frame][stack_idx].mv[0];
                            if (this_mv == stack_mv)
                                break;
                        }

                        if (stack_idx == mvRefs->refMvCount[ref_frame]) {
                            mvRefs->refMvStack[ref_frame][stack_idx].mv[0] = this_mv;
                            mvRefs->refMvStack[ref_frame][stack_idx].weight = 2; // arbitrary small value
                            ++mvRefs->refMvCount[ref_frame];
                        }
                    }
                }
                idx += block_size_wide_8x8[cand->sbType];
            }

            for (int32_t i = 0; i < mvRefs->refMvCount[ref_frame]; i++)
                mvRefs->refMvStack[ref_frame][i].mv[0] = ClampedMv(mvRefs->refMvStack[ref_frame][i].mv[0], minMv, maxMv);
            if (mvRefs->refMvCount[ref_frame] > 0) {
                mvRefList[ref_frame][0] = mvRefs->refMvStack[ref_frame][0].mv[0];
                if (mvRefs->refMvCount[ref_frame] > 1)
                    mvRefList[ref_frame][1] = mvRefs->refMvStack[ref_frame][1].mv[0];
            }
        }
    }

    void BuildExtendCandidateList(const ModeInfo **cands, const ModeInfo *mi, int32_t miPitch,
                                  int32_t max_row_offset, int32_t max_col_offset, int32_t size8x8)
    {
        if (max_row_offset < 0) {
            const ModeInfo *cand = mi - miPitch;
            for (int idx = 0; idx < size8x8; idx += block_size_wide_8x8[cand[idx].sbType])
                if (cand[idx].refIdx[0] != INTRA_FRAME)
                    *cands++ = cand + idx;
        }
        if (max_col_offset < 0) {
            const ModeInfo *cand = mi - 1;
            for (int idx = 0; idx < size8x8;) {
                if (cand->refIdx[0] != INTRA_FRAME)
                    *cands++ = cand;
                idx += block_size_wide_8x8[cand->sbType];
                cand += block_size_wide_8x8[cand->sbType] * miPitch;
            }
        }
        *cands = nullptr;
    }

    void ExtendStack1(H265MV mv, H265MVCand *stack, int32_t *refMvCount)
    {
        assert(*refMvCount < MAX_MV_REF_CANDIDATES);
        int32_t count = *refMvCount;
        if (count == 1 && stack[0].mv[0] == mv)
            return;
        stack[count].mv[0] = mv;
        *refMvCount = count + 1;
    }

    void ExtendStack2(H265MV mv, H265MVCand *stack, int32_t *refMvCount)
    {
        assert(*refMvCount < MAX_MV_REF_CANDIDATES + 1);
        int32_t count = *refMvCount;
        if (count > 0 && stack[0].mv[0] == mv ||
            count > 1 && stack[1].mv[0] == mv)
            return;
        stack[count].mv[0] = mv;
        *refMvCount = count + 1;
    }

    void ExtendRefMvStackP(const ModeInfo **cands, int32_t ref_frame, AV1MvRefs *mvRefs)
    {
        H265MVCand *stack = mvRefs->refMvStack[ref_frame];
        int32_t &stackSize = mvRefs->refMvCount[ref_frame];

        for (const ModeInfo *cand = *cands; stackSize < MAX_MV_REF_CANDIDATES && cand; cand = *++cands)
            ExtendStack1(cand->mv[0], stack, &stackSize);
    }

    void ExtendRefMvStackBP(const ModeInfo **cands, int32_t ref_frame, AV1MvRefs *mvRefs)
    {
        H265MVCand *stack = mvRefs->refMvStack[ref_frame];
        int32_t &stackSize = mvRefs->refMvCount[ref_frame];

        if (stackSize >= MAX_MV_REF_CANDIDATES)
            return;

        if (ref_frame == C) {
            H265MV ref_id[2][2];
            H265MV ref_diff[2][2];
            int32_t ref_id_count[2] = { 0 };
            int32_t ref_diff_count[2] = { 0 };

            for (const ModeInfo *cand = *cands; cand; cand = *++cands) {
                if (cand->refIdx[0] == L && ref_id_count[0] < 2)
                    ref_id[0][ref_id_count[0]++] = cand->mv[0];
                else if (ref_diff_count[0] < 2)
                    ref_diff[0][ref_diff_count[0]++] = cand->mv[0];

                if (cand->refIdx[0] == A && ref_id_count[1] < 2)
                    ref_id[1][ref_id_count[1]++] = cand->mv[0];
                else if (ref_diff_count[1] < 2)
                    ref_diff[1][ref_diff_count[1]++] = cand->mv[0];

                if (cand->refIdx[1] == A) {
                    if (ref_diff_count[0] < 2)
                        ref_diff[0][ref_diff_count[0]++] = cand->mv[1];
                    if (ref_id_count[1] < 2)
                        ref_id[1][ref_id_count[1]++] = cand->mv[1];
                    else if (ref_diff_count[1] < 2)
                        ref_diff[1][ref_diff_count[1]++] = cand->mv[1];
                }
            }

            // Build up the compound mv predictor
            H265MVPair comp_list[2] = {};

            for (int idx = 0; idx < 2; ++idx) {
                int comp_idx = 0;
                for (int list_idx = 0; list_idx < ref_id_count[idx]; ++list_idx, ++comp_idx)
                    comp_list[comp_idx][idx] = ref_id[idx][list_idx];
                for (int list_idx = 0; list_idx < ref_diff_count[idx] && comp_idx < 2; ++list_idx, ++comp_idx)
                    comp_list[comp_idx][idx] = ref_diff[idx][list_idx];
            }

            if (mvRefs->refMvCount[ref_frame] == 0) {
                mvRefs->refMvStack[ref_frame][0].mv = comp_list[0];
                mvRefs->refMvStack[ref_frame][1].mv = comp_list[1];
            }
            else
                mvRefs->refMvStack[ref_frame][1].mv = comp_list[(comp_list[0] == mvRefs->refMvStack[ref_frame][0].mv)];
            mvRefs->refMvCount[ref_frame] = 2;

        }
        else {

            for (const ModeInfo *cand = *cands; stackSize < MAX_MV_REF_CANDIDATES && cand; cand = *++cands) {
                H265MV mv = cand->mv[0];
                ExtendStack1(mv, stack, &stackSize);
                if (cand->refIdx[1] != NONE_FRAME)
                    ExtendStack2(cand->mv[1], stack, &stackSize);
            }
        }
    }

    const int32_t tab_negate[3][3] = { { 0, 0, -1 }, { 0, 0, -1 }, { -1, 0, 0 } };

    void ExtendRefMvStackB(const ModeInfo **cands, int32_t ref_frame, AV1MvRefs *mvRefs)
    {
        H265MVCand *stack = mvRefs->refMvStack[ref_frame];
        int32_t &stackSize = mvRefs->refMvCount[ref_frame];

        if (stackSize >= MAX_MV_REF_CANDIDATES)
            return;

        if (ref_frame == C) {
            H265MV ref_id[2][2];
            H265MV ref_diff[2][2];
            int32_t ref_id_count[2] = { 0 };
            int32_t ref_diff_count[2] = { 0 };

            for (const ModeInfo *cand = *cands; cand; cand = *++cands) {
                if (cand->refIdx[0] == L && ref_id_count[0] < 2)
                    ref_id[0][ref_id_count[0]++] = cand->mv[0];
                else if (ref_diff_count[0] < 2)
                    ref_diff[0][ref_diff_count[0]++] = NegateMv(cand->mv[0], tab_negate[L][cand->refIdx[0]]);

                if (cand->refIdx[0] == A && ref_id_count[1] < 2)
                    ref_id[1][ref_id_count[1]++] = cand->mv[0];
                else if (ref_diff_count[1] < 2)
                    ref_diff[1][ref_diff_count[1]++] = NegateMv(cand->mv[0], tab_negate[A][cand->refIdx[0]]);

                if (cand->refIdx[1] == A) {
                    if (ref_diff_count[0] < 2)
                        ref_diff[0][ref_diff_count[0]++] = NegateMv(cand->mv[1], -1);
                    if (ref_id_count[1] < 2)
                        ref_id[1][ref_id_count[1]++] = cand->mv[1];
                    else if (ref_diff_count[1] < 2)
                        ref_diff[1][ref_diff_count[1]++] = cand->mv[1];
                }
            }

            // Build up the compound mv predictor
            H265MVPair comp_list[2] = {};

            for (int idx = 0; idx < 2; ++idx) {
                int comp_idx = 0;
                for (int list_idx = 0; list_idx < ref_id_count[idx]; ++list_idx, ++comp_idx)
                    comp_list[comp_idx][idx] = ref_id[idx][list_idx];
                for (int list_idx = 0; list_idx < ref_diff_count[idx] && comp_idx < 2; ++list_idx, ++comp_idx)
                    comp_list[comp_idx][idx] = ref_diff[idx][list_idx];
            }

            if (mvRefs->refMvCount[ref_frame] == 0) {
                mvRefs->refMvStack[ref_frame][0].mv = comp_list[0];
                mvRefs->refMvStack[ref_frame][1].mv = comp_list[1];
            } else
                mvRefs->refMvStack[ref_frame][1].mv = comp_list[ (comp_list[0] == mvRefs->refMvStack[ref_frame][0].mv) ];
            mvRefs->refMvCount[ref_frame] = 2;

        } else {

            const int32_t *negate = tab_negate[ref_frame];

            for (const ModeInfo *cand = *cands; stackSize < MAX_MV_REF_CANDIDATES && cand; cand = *++cands) {
                H265MV mv = NegateMv(cand->mv[0], negate[cand->refIdx[0]]);
                ExtendStack1(mv, stack, &stackSize);
                if (cand->refIdx[1] != NONE_FRAME)
                    ExtendStack2(NegateMv(cand->mv[1], negate[A]), stack, &stackSize);
            }
        }
    }

    void SetupRefMVListTU7P(const TileBorders &tileBorders, const ModeInfo *mi, int32_t miPitch, const Frame &frame,
                            int32_t orderHintBits, int32_t miRow, int32_t miCol, int32_t size8x8, AV1MvRefs *mvRefs,
                            H265MV (*mvRefList)[MAX_MV_REF_CANDIDATES], H265MV minMv, H265MV maxMv, int32_t pass)
    {
        const int32_t minSize8x8 = (size8x8 >= 8) ? 2 : 1;
        const int32_t hasTr = hasTopRight[miRow & 7][(miCol & 7) + size8x8 - 1];
        int32_t len;
        int processed_rows = 0;
        int processed_cols = 0;

        mvRefs->refMvCount[L] = 0;
        mvRefs->refMvCount[G] = 0;

        int32_t newMvCount[2] = {};
        int32_t colMatchCount[2] = {};
        int32_t rowMatchCount[2] = {};
        int32_t nearestMatch[2] = {};
        int32_t refMatch[2] = {};
        const int32_t usePrev = (frame.m_prevFrame != nullptr);
        int32_t coll_blk_avail[2] = { !usePrev, !usePrev };

        int32_t max_row_offset = 0;
        if (miRow > tileBorders.rowStart) {
            max_row_offset = -3;
            max_row_offset = Saturate(tileBorders.rowStart - miRow, tileBorders.rowEnd - miRow - 1, max_row_offset);
        }

        int32_t max_col_offset = 0;
        if (miCol > tileBorders.colStart) {
            max_col_offset = -3;
            max_col_offset = Saturate(tileBorders.colStart - miCol, tileBorders.colEnd - miCol - 1, max_col_offset);
        }


        // Scan nearest area
        ////////////////////

        // Scan the first above row
        int32_t weight = 2;
        if (miRow - 1 >= tileBorders.rowStart)
        {

            const ModeInfo *cand = mi - miPitch;
            for (int32_t i = 0; i < size8x8; i += len, cand += len) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                int row_offset = -1;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-(max_row_offset*2)  + row_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed rows.
                    processed_rows = inc - row_offset - 1;
                }
                AddNearestRefMvCandidateTU7P(cand, mvRefs->refMvCount, newMvCount, rowMatchCount,
                                             mvRefs->refMvStack, 2 * len * weight);
            }
        }

        // Scan the first left column
        if (miCol - 1 >= tileBorders.colStart) {
            const ModeInfo *cand = mi - 1;
            for (int32_t i = 0; i < size8x8; i += len, cand += len * miPitch) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                int col_offset = -1;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-(max_col_offset*2) + col_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed cols.
                    processed_cols = inc - col_offset - 1;
                }
                AddNearestRefMvCandidateTU7P(cand, mvRefs->refMvCount, newMvCount, colMatchCount,
                                             mvRefs->refMvStack, 2 * len * weight);
            }
        }


        weight = 2;
        // Above-right
        if (hasTr && miRow - 1 >= tileBorders.rowStart && miCol + size8x8 < tileBorders.colEnd)
            AddNearestRefMvCandidateTU7P(mi + size8x8 - miPitch, mvRefs->refMvCount, newMvCount,
                                         rowMatchCount, mvRefs->refMvStack, 2 * weight);

        nearestMatch[L] = (rowMatchCount[L] > 0) + (colMatchCount[L] > 0);
        nearestMatch[G] = (rowMatchCount[G] > 0) + (colMatchCount[G] > 0);

        mvRefs->nearestMvCount[L] = mvRefs->refMvCount[L];
        mvRefs->nearestMvCount[G] = mvRefs->refMvCount[G];
        mvRefs->nearestMvCount[A] = 0;
        mvRefs->nearestMvCount[C] = 0;

        // Scan colocated area
        //   <REMOVED>

        // Scan the outer area
        //////////////////////

        // Above-left
        if (miRow - 1 >= tileBorders.rowStart && miCol - 1 >= tileBorders.colStart && MVPRED_FAR)
            AddOuterRefMvCandidateTU7P(mi - 1 - miPitch, mvRefs->refMvCount, rowMatchCount,
                                       mvRefs->refMvStack, 2 * 2);

        int idx = 2;
        int row_offset = -(idx << 1) + 1;
        // Second above row
        if (miRow - 2 >= tileBorders.rowStart && abs(row_offset) > processed_rows && MVPRED_FAR) {
            const ModeInfo *cand = mi - miPitch * 2;
            for (int32_t i = 0; i < size8x8; i += len, cand += len) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-max_row_offset*2 + row_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed rows.
                    processed_rows = inc - row_offset - 1;
                }
                AddOuterRefMvCandidateTU7P(cand, mvRefs->refMvCount, rowMatchCount,
                                           mvRefs->refMvStack, 2 * len * weight);
            }
        }

        int col_offset = -(idx << 1) + 1;
        // Second left column
        if (miCol - 2 >= tileBorders.colStart && abs(col_offset) > processed_cols && MVPRED_FAR) {
            const ModeInfo *cand = mi - 2;
            for (int32_t i = 0; i < size8x8; i += len, cand += len * miPitch) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-max_col_offset*2 + col_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed cols.
                    processed_cols = inc - col_offset - 1;
                }
                AddOuterRefMvCandidateTU7P(cand, mvRefs->refMvCount, colMatchCount,
                                           mvRefs->refMvStack, 2 * len * weight);
            }
        }

        idx = 3;
        row_offset = -(idx << 1) + 1;
        // Third above row
        if (miRow - 3 >= tileBorders.rowStart && abs(row_offset) > processed_rows && MVPRED_FAR) {
            const ModeInfo *cand = mi - miPitch * 3;
            for (int32_t i = 0; i < size8x8; i += len, cand += len) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-max_row_offset*2 + row_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed rows.
                    processed_rows = inc - row_offset - 1;
                }
                AddOuterRefMvCandidateTU7P(cand, mvRefs->refMvCount, rowMatchCount,
                                           mvRefs->refMvStack, 2 * len * weight);
            }
        }

        // Third left column
        col_offset = -(idx << 1) + 1;
        if (miCol - 3 >= tileBorders.colStart && abs(col_offset) > processed_cols && MVPRED_FAR) {
            const ModeInfo *cand = mi - 3;
            for (int32_t i = 0; i < size8x8; i += len, cand += len * miPitch) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-max_col_offset*2 + col_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed cols.
                    processed_cols = inc - col_offset - 1;
                }
                AddOuterRefMvCandidateTU7P(cand, mvRefs->refMvCount, colMatchCount,
                                           mvRefs->refMvStack, 2 * len * weight);
            }
        }

        // Non Causal Non Normative
        if (pass>1 && frame.m_pyramidLayer == 0 && miCol + size8x8 < tileBorders.colEnd && miRow + size8x8 < tileBorders.rowEnd) {
            AddTRefMvCandidateTU7P(mi + size8x8 + size8x8*miPitch, mvRefs->refMvCount, rowMatchCount,
                mvRefs->refMvStack, 2 * 2);
        }

        refMatch[L] = (rowMatchCount[L] > 0) + (colMatchCount[L] > 0);
        refMatch[G] = (rowMatchCount[G] > 0) + (colMatchCount[G] > 0);

        SortStack(mvRefs->refMvStack[L], mvRefs->refMvCount[L]);
        SortStack(mvRefs->refMvStack[G], mvRefs->refMvCount[G]);


        if (pass > 1) {
            const ModeInfo *cands[17];
            BuildExtendCandidateList(cands, mi, miPitch, max_row_offset, max_col_offset, size8x8);
            ExtendRefMvStackP(cands, L, mvRefs);
            ExtendRefMvStackP(cands, G, mvRefs);
        }

        for (int32_t i = 0; i < mvRefs->refMvCount[L]; i++)
            mvRefs->refMvStack[L][i].mv[0] = ClampedMv(mvRefs->refMvStack[L][i].mv[0], minMv, maxMv);
        if (mvRefs->refMvCount[L] > 0) {
            mvRefList[L][0] = mvRefs->refMvStack[L][0].mv[0];
            if (mvRefs->refMvCount[L] > 1)
                mvRefList[L][1] = mvRefs->refMvStack[L][1].mv[0];
        }

        for (int32_t i = 0; i < mvRefs->refMvCount[G]; i++)
            mvRefs->refMvStack[G][i].mv[0] = ClampedMv(mvRefs->refMvStack[G][i].mv[0], minMv, maxMv);
        if (mvRefs->refMvCount[G] > 0) {
            mvRefList[G][0] = mvRefs->refMvStack[G][0].mv[0];
            if (mvRefs->refMvCount[G] > 1)
                mvRefList[G][1] = mvRefs->refMvStack[G][1].mv[0];
        }

        mvRefs->interModeCtx[L] = GetModeContext(nearestMatch[L], refMatch[L], newMvCount[L], coll_blk_avail[L]);
        mvRefs->interModeCtx[G] = GetModeContext(nearestMatch[G], refMatch[G], newMvCount[G], coll_blk_avail[G]);
    }

    void SetupRefMVListTU7B(const TileBorders &tileBorders, const ModeInfo *mi, int32_t miPitch, const Frame &frame,
                            int32_t orderHintBits, int32_t miRow, int32_t miCol, int32_t size8x8, AV1MvRefs *mvRefs,
                            H265MV (*mvRefList)[MAX_MV_REF_CANDIDATES], H265MV minMv, H265MV maxMv, int32_t negate, int32_t pass)
    {
        const int32_t minSize8x8 = (size8x8 >= 8) ? 2 : 1;
        const int32_t hasTr = hasTopRight[miRow & 7][(miCol & 7) + size8x8 - 1];
        int32_t len;
        int processed_rows = 0;
        int processed_cols = 0;

        mvRefs->refMvCount[L] = 0;
        mvRefs->refMvCount[A] = 0;
        mvRefs->refMvCount[C] = 0;

        int32_t newMvCount[4] = {};
        int32_t colMatchCount[4] = {};
        int32_t rowMatchCount[4] = {};
        int32_t nearestMatch[4] = {};
        int32_t refMatch[4] = {};
        const int32_t usePrev = (frame.m_prevFrame != nullptr);
        int32_t coll_blk_avail[4] = { !usePrev, !usePrev, !usePrev, !usePrev};

        int32_t max_row_offset = 0;
        if (miRow > tileBorders.rowStart)
        {
            max_row_offset = -3;
            max_row_offset = Saturate(tileBorders.rowStart - miRow, tileBorders.rowEnd - miRow - 1, max_row_offset);
        }

        int32_t max_col_offset = 0;
        if (miCol > tileBorders.colStart) {
            max_col_offset = -3;
            max_col_offset = Saturate(tileBorders.colStart - miCol, tileBorders.colEnd - miCol - 1, max_col_offset);
        }

        // Scan nearest area
        ////////////////////

        // Scan the first above row
        int32_t weight = 2;
        if (miRow - 1 >= tileBorders.rowStart)
        {
            const ModeInfo *cand = mi - miPitch;
            for (int32_t i = 0; i < size8x8; i += len, cand += len) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                int row_offset = -1;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-(max_row_offset*2)  + row_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed rows.
                    processed_rows = inc - row_offset - 1;
                }
                AddNearestRefMvCandidateTU7B(cand, mvRefs->refMvCount, newMvCount, rowMatchCount,
                                             mvRefs->refMvStack, 2 * len * weight);
            }
        }

        // Scan the first left column
        if (miCol - 1 >= tileBorders.colStart) {
            const ModeInfo *cand = mi - 1;
            for (int32_t i = 0; i < size8x8; i += len, cand += len * miPitch) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                int col_offset = -1;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-(max_col_offset*2) + col_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed cols.
                    processed_cols = inc - col_offset - 1;
                }
                AddNearestRefMvCandidateTU7B(cand, mvRefs->refMvCount, newMvCount, colMatchCount,
                                             mvRefs->refMvStack, 2 * len * weight);
            }
        }

        weight = 2;
        // Above-right
        if (hasTr && miRow - 1 >= tileBorders.rowStart && miCol + size8x8 < tileBorders.colEnd)
            AddNearestRefMvCandidateTU7B(mi + size8x8 - miPitch, mvRefs->refMvCount, newMvCount,
                                         rowMatchCount, mvRefs->refMvStack, 2 * weight);

        nearestMatch[L] = (rowMatchCount[L] > 0) + (colMatchCount[L] > 0);
        nearestMatch[A] = (rowMatchCount[A] > 0) + (colMatchCount[A] > 0);
        nearestMatch[C] = (rowMatchCount[C] > 0) + (colMatchCount[C] > 0);

        mvRefs->nearestMvCount[L] = mvRefs->refMvCount[L];
        mvRefs->nearestMvCount[G] = 0;
        mvRefs->nearestMvCount[A] = mvRefs->refMvCount[A];
        mvRefs->nearestMvCount[C] = mvRefs->refMvCount[C];

        // Scan colocated area
        //   <REMOVED>

        // Scan the outer area
        //////////////////////

        // Above-left
        if (miRow - 1 >= tileBorders.rowStart && miCol - 1 >= tileBorders.colStart && MVPRED_FAR)
            AddOuterRefMvCandidateTU7B(mi - 1 - miPitch, mvRefs->refMvCount, rowMatchCount,
                                       mvRefs->refMvStack, 2 * 2);

        int idx = 2;
        int row_offset = -(idx << 1) + 1;
        // Second above row
        if (miRow - 2 >= tileBorders.rowStart && abs(row_offset) > processed_rows && MVPRED_FAR) {
            const ModeInfo *cand = mi - miPitch * 2;
            for (int32_t i = 0; i < size8x8; i += len, cand += len) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-max_row_offset*2 + row_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed rows.
                    processed_rows = inc - row_offset - 1;
                }
                AddOuterRefMvCandidateTU7B(cand, mvRefs->refMvCount, rowMatchCount,
                                           mvRefs->refMvStack, 2 * len * weight);
            }
        }

        int col_offset = -(idx << 1) + 1;
        // Second left column
        if (miCol - 2 >= tileBorders.colStart && abs(col_offset) > processed_cols && MVPRED_FAR) {
            const ModeInfo *cand = mi - 2;
            for (int32_t i = 0; i < size8x8; i += len, cand += len * miPitch) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-max_col_offset*2 + col_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed cols.
                    processed_cols = inc - col_offset - 1;
                }
                AddOuterRefMvCandidateTU7B(cand, mvRefs->refMvCount, colMatchCount,
                                           mvRefs->refMvStack, 2 * len * weight);
            }
        }

        idx = 3;
        row_offset = -(idx << 1) + 1;
        // Third above row
        if (miRow - 3 >= tileBorders.rowStart && abs(row_offset) > processed_rows && MVPRED_FAR) {
            const ModeInfo *cand = mi - miPitch * 3;
            for (int32_t i = 0; i < size8x8; i += len, cand += len) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-max_row_offset*2 + row_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed rows.
                    processed_rows = inc - row_offset - 1;
                }
                AddOuterRefMvCandidateTU7B(cand, mvRefs->refMvCount, rowMatchCount,
                                           mvRefs->refMvStack, 2 * len * weight);
            }
        }

        // Third left column
        col_offset = -(idx << 1) + 1;
        if (miCol - 3 >= tileBorders.colStart && abs(col_offset) > processed_cols && MVPRED_FAR) {
            const ModeInfo *cand = mi - 3;
            for (int32_t i = 0; i < size8x8; i += len, cand += len * miPitch) {
                len = MAX(minSize8x8, MIN(size8x8, block_size_wide_8x8[cand->sbType]));
                weight = 2;
                if (size8x8 <= block_size_wide_8x8[cand->sbType]) {
                    int32_t inc = IPP_MIN(-max_col_offset*2 + col_offset + 1, 2*block_size_wide_8x8[cand->sbType]);
                    weight = IPP_MAX(weight, inc);
                    // Update processed cols.
                    processed_cols = inc - col_offset - 1;
                }
                AddOuterRefMvCandidateTU7B(cand, mvRefs->refMvCount, colMatchCount,
                                           mvRefs->refMvStack, 2 * len * weight);
            }
        }

        // Non Causal Non Normative
        if (pass>1 && frame.m_pyramidLayer == 0 && miCol+size8x8 < tileBorders.colEnd && miRow+size8x8 < tileBorders.rowEnd) {
            AddTRefMvCandidateTU7B(mi + size8x8 + size8x8*miPitch, mvRefs->refMvCount, newMvCount,
                rowMatchCount, mvRefs->refMvStack, 2 * 2);
        }

        refMatch[L] = (rowMatchCount[L] > 0) + (colMatchCount[L] > 0);
        refMatch[A] = (rowMatchCount[A] > 0) + (colMatchCount[A] > 0);
        refMatch[C] = (rowMatchCount[C] > 0) + (colMatchCount[C] > 0);

        SortStack(mvRefs->refMvStack[L], mvRefs->refMvCount[L]);
        SortStack(mvRefs->refMvStack[A], mvRefs->refMvCount[A]);
        SortStack(mvRefs->refMvStack[C], mvRefs->refMvCount[C]);

        if (pass > 1) {
            const ModeInfo *cands[17];
            BuildExtendCandidateList(cands, mi, miPitch, max_row_offset, max_col_offset, size8x8);
            if (negate) {
                ExtendRefMvStackB(cands, L, mvRefs);
                ExtendRefMvStackB(cands, A, mvRefs);
                ExtendRefMvStackB(cands, C, mvRefs);
            }
            else {
                ExtendRefMvStackBP(cands, L, mvRefs);
                ExtendRefMvStackBP(cands, A, mvRefs);
                ExtendRefMvStackBP(cands, C, mvRefs);
            }
        }

        for (int32_t i = 0; i < mvRefs->refMvCount[L]; i++)
            mvRefs->refMvStack[L][i].mv[0] = ClampedMv(mvRefs->refMvStack[L][i].mv[0], minMv, maxMv);
        if (mvRefs->refMvCount[L] > 0) {
            mvRefList[L][0] = mvRefs->refMvStack[L][0].mv[0];
            if (mvRefs->refMvCount[L] > 1)
                mvRefList[L][1] = mvRefs->refMvStack[L][1].mv[0];
        }

        for (int32_t i = 0; i < mvRefs->refMvCount[A]; i++)
            mvRefs->refMvStack[A][i].mv[0] = ClampedMv(mvRefs->refMvStack[A][i].mv[0], minMv, maxMv);
        if (mvRefs->refMvCount[A] > 0) {
            mvRefList[A][0] = mvRefs->refMvStack[A][0].mv[0];
            if (mvRefs->refMvCount[A] > 1)
                mvRefList[A][1] = mvRefs->refMvStack[A][1].mv[0];
        }

        for (int idx = 0; idx < mvRefs->refMvCount[C]; ++idx)
            mvRefs->refMvStack[C][idx].mv = ClampedMvPair(mvRefs->refMvStack[C][idx].mv, minMv, maxMv);

        mvRefs->interModeCtx[L] = GetModeContext(nearestMatch[L], refMatch[L], newMvCount[L], coll_blk_avail[L]);
        mvRefs->interModeCtx[A] = GetModeContext(nearestMatch[A], refMatch[A], newMvCount[A], coll_blk_avail[A]);
        mvRefs->interModeCtx[C] = GetModeContext(nearestMatch[C], refMatch[C], newMvCount[C], coll_blk_avail[C]);

        const int16_t newmv_ctx = mvRefs->interModeCtx[C] & NEWMV_CTX_MASK;
        const int16_t refmv_ctx = (mvRefs->interModeCtx[C] >> REFMV_OFFSET) & REFMV_CTX_MASK;
        mvRefs->interModeCtx[C] = (refmv_ctx >> 1) * COMP_NEWMV_CTXS + IPP_MIN(newmv_ctx, COMP_NEWMV_CTXS - 1);
    }

};  // anonymous namespace


template <typename PixType>
void H265CU<PixType>::GetMvRefsAV1TU7P(int32_t sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs, int32_t pass)
{
    const int32_t width8 = block_size_high_8x8[sbType]; // in 8x8 blocks
    const int32_t height8 = width8;
    const H265MV minMv = {
        (int16_t)IPP_MAX((-(miCol * MI_SIZE * 8) - MV_BORDER_AV1 - width8  * MI_SIZE * 8), IPP_MIN_16S),
        (int16_t)IPP_MAX((-(miRow * MI_SIZE * 8) - MV_BORDER_AV1 - height8 * MI_SIZE * 8), IPP_MIN_16S)
    };
    const H265MV maxMv = {
        (int16_t)IPP_MIN(((m_par->miCols - miCol) * MI_SIZE * 8 + MV_BORDER_AV1), IPP_MAX_16S),
        (int16_t)IPP_MIN(((m_par->miRows - miRow) * MI_SIZE * 8 + MV_BORDER_AV1), IPP_MAX_16S)
    };

    __ALIGN8 H265MV refListMvAll[GOLDEN_FRAME+1][2] = {};
    const ModeInfo *mi = m_currFrame->cu_data + miCol + miRow * m_par->miPitch;

    SetupRefMVListTU7P(m_tileBorders, mi, m_par->miPitch, *m_currFrame, m_par->seqParams.orderHintBits,
                       miRow, miCol, width8, &mvRefs->mvRefsAV1, refListMvAll, minMv, maxMv, pass);

    mvRefs->mvs[LAST_FRAME][NEARESTMV_][0] = refListMvAll[LAST_FRAME][0];
    mvRefs->mvs[LAST_FRAME][NEARMV_][0] = refListMvAll[LAST_FRAME][1];
    mvRefs->bestMv[LAST_FRAME][0] = refListMvAll[LAST_FRAME][0];

    if (m_currFrame->isUniq[GOLDEN_FRAME]) {
        mvRefs->mvs[GOLDEN_FRAME][NEARESTMV_][0] = refListMvAll[GOLDEN_FRAME][0];
        mvRefs->mvs[GOLDEN_FRAME][NEARMV_][0] = refListMvAll[GOLDEN_FRAME][1];
        mvRefs->bestMv[GOLDEN_FRAME][0] = refListMvAll[GOLDEN_FRAME][0];
    }

    mvRefs->useMvHp[0] = 0;
    mvRefs->useMvHp[1] = 0;
    mvRefs->useMvHp[2] = 0;
}
template void H265CU<uint8_t>::GetMvRefsAV1TU7P(int32_t sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs, int32_t pass);
template void H265CU<uint16_t>::GetMvRefsAV1TU7P(int32_t sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs, int32_t pass);


template <typename PixType>
void H265CU<PixType>::GetMvRefsAV1TU7B(int32_t sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs, int32_t negate, int32_t pass)
{
    const int32_t width8 = block_size_wide_8x8[sbType]; // in 8x8 blocks
    const int32_t height8 = width8;
    const H265MV minMv = {
        (int16_t)IPP_MAX((-(miCol * MI_SIZE * 8) - MV_BORDER_AV1 - width8  * MI_SIZE * 8), IPP_MIN_16S),
        (int16_t)IPP_MAX((-(miRow * MI_SIZE * 8) - MV_BORDER_AV1 - height8 * MI_SIZE * 8), IPP_MIN_16S)
    };
    const H265MV maxMv = {
        (int16_t)IPP_MIN(((m_par->miCols - miCol) * MI_SIZE * 8 + MV_BORDER_AV1), IPP_MAX_16S),
        (int16_t)IPP_MIN(((m_par->miRows - miRow) * MI_SIZE * 8 + MV_BORDER_AV1), IPP_MAX_16S)
    };

    __ALIGN8 H265MV refListMvAll[COMP_VAR0+1][2] = {};
    const ModeInfo *mi = m_currFrame->cu_data + miCol + miRow * m_par->miPitch;

    SetupRefMVListTU7B(m_tileBorders, mi, m_par->miPitch, *m_currFrame, m_par->seqParams.orderHintBits,
                       miRow, miCol, width8, &mvRefs->mvRefsAV1, refListMvAll, minMv, maxMv, negate, pass);

    mvRefs->mvs[LAST_FRAME][NEARESTMV_][0] = refListMvAll[LAST_FRAME][0];
    mvRefs->mvs[LAST_FRAME][NEARESTMV_][1].asInt = 0;
    mvRefs->mvs[LAST_FRAME][NEARMV_][0] = refListMvAll[LAST_FRAME][1];
    mvRefs->mvs[LAST_FRAME][NEARMV_][1].asInt = 0;
    mvRefs->bestMv[LAST_FRAME][0] = refListMvAll[LAST_FRAME][0];

    mvRefs->mvs[ALTREF_FRAME][NEARESTMV_][0] = refListMvAll[ALTREF_FRAME][0];
    mvRefs->mvs[ALTREF_FRAME][NEARESTMV_][1].asInt = 0;
    mvRefs->mvs[ALTREF_FRAME][NEARMV_][0] = refListMvAll[ALTREF_FRAME][1];
    mvRefs->mvs[ALTREF_FRAME][NEARMV_][1].asInt = 0;
    mvRefs->bestMv[ALTREF_FRAME][0] = refListMvAll[ALTREF_FRAME][0];

    mvRefs->mvs[COMP_VAR0][NEARESTMV_][0] = mvRefs->mvs[LAST_FRAME][NEARESTMV_][0];
    mvRefs->mvs[COMP_VAR0][NEARESTMV_][1] = mvRefs->mvs[ALTREF_FRAME][NEARESTMV_][0];
    mvRefs->mvs[COMP_VAR0][NEARMV_][0] = mvRefs->mvs[LAST_FRAME][NEARMV_][0];
    mvRefs->mvs[COMP_VAR0][NEARMV_][1] = mvRefs->mvs[ALTREF_FRAME][NEARMV_][0];
    mvRefs->bestMv[COMP_VAR0][0] = mvRefs->bestMv[LAST_FRAME][0];
    mvRefs->bestMv[COMP_VAR0][1] = mvRefs->bestMv[ALTREF_FRAME][0];
    if (mvRefs->mvRefsAV1.refMvCount[COMP_VAR0] == 1) {
        mvRefs->mvs[COMP_VAR0][NEARESTMV_].asInt64 = mvRefs->mvRefsAV1.refMvStack[COMP_VAR0][0].mv.asInt64;
        if (pass == 1 || pass == 2)
            mvRefs->bestMv[COMP_VAR0] = mvRefs->mvs[COMP_VAR0][NEARESTMV_];
    } else if (mvRefs->mvRefsAV1.refMvCount[COMP_VAR0] > 1) {
        mvRefs->mvs[COMP_VAR0][NEARESTMV_].asInt64 = mvRefs->mvRefsAV1.refMvStack[COMP_VAR0][0].mv.asInt64;
        mvRefs->mvs[COMP_VAR0][NEARMV_].asInt64 = mvRefs->mvRefsAV1.refMvStack[COMP_VAR0][1].mv.asInt64;
        mvRefs->bestMv[COMP_VAR0] = mvRefs->mvs[COMP_VAR0][NEARESTMV_];
    }

    mvRefs->useMvHp[0] = 0;
    mvRefs->useMvHp[1] = 0;
    mvRefs->useMvHp[2] = 0;
}
template void H265CU<uint8_t>::GetMvRefsAV1TU7B(int32_t sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs, int32_t negate, int32_t pass);
template void H265CU<uint16_t>::GetMvRefsAV1TU7B(int32_t sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs, int32_t negate, int32_t pass);

