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

#include "mfx_av1_get_context.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_frame.h"

using namespace H265Enc;

namespace {
    int32_t is_inter_block(const ModeInfo *mi) { return mi->refIdx[0] > INTRA_FRAME; }
    int32_t has_second_ref(const ModeInfo *mi) { return mi->refIdx[1] > INTRA_FRAME; }
    int32_t check_backward_refs(RefIdx ref) { return ref == ALTREF_FRAME/*(ref >= AV1_BWDREF_FRAME) && (ref <= AV1_ALTREF_FRAME)*/; }
    int32_t is_backward_ref_frame(RefIdx ref) { return check_backward_refs(ref); }
    int32_t has_uni_comp_refs(const ModeInfo *mi) {
        return 0/*has_second_ref(mi) && (!((mi->refIdx[0] >= AV1_BWDREF_FRAME) ^ (mi->refIdx[1] >= AV1_BWDREF_FRAME)))*/;
    }
    int32_t check_last2(RefIdx ref) { return 0; }
    int32_t check_golden_or_last3(RefIdx ref) { return ref == GOLDEN_FRAME/*(ref == AV1_GOLDEN_FRAME) || (ref == AV1_LAST3_FRAME)*/; }
    int32_t check_last_or_last2(RefIdx ref) { return ref == LAST_FRAME/*(ref == AV1_LAST_FRAME) || (ref == AV1_LAST2_FRAME)*/; }
};

ModeInfo *H265Enc::GetLeft(ModeInfo *currMi, int32_t miCol, int32_t tileMiColStart) {
    return (miCol > tileMiColStart) ? currMi - 1 : NULL;
}

ModeInfo *H265Enc::GetAbove(ModeInfo *currMi, int32_t miPitch, int32_t miRow, int32_t tileMiRowStart) {
    return (miRow > tileMiRowStart) ? currMi - miPitch : NULL;
}

const ModeInfo *H265Enc::GetLeft(const ModeInfo *currMi, int32_t miCol, int32_t tileMiColStart) {
    return GetLeft(const_cast<ModeInfo *>(currMi), miCol, tileMiColStart);
}

const ModeInfo *H265Enc::GetAbove(const ModeInfo *currMi, int32_t miPitch, int32_t miRow, int32_t tileMiRowStart) {
    return GetAbove(const_cast<ModeInfo *>(currMi), miPitch, miRow, tileMiRowStart);
}

int32_t H265Enc::GetCtxCompMode(const ModeInfo *above, const ModeInfo *left, const Frame &frame)
{
    if (above && left) {
        if (above->refIdx[1] == NONE_FRAME && left->refIdx[1] == NONE_FRAME)
            return (above->refIdx[0] == frame.compFixedRef) ^ (left->refIdx[0] == frame.compFixedRef);
        else if (above->refIdx[1] == NONE_FRAME)
            return 2 + (above->refIdx[0] == frame.compFixedRef || above->refIdx[0] == INTRA_FRAME);
        else if (left->refIdx[1] == NONE_FRAME)
            return 2 + (left->refIdx[0] == frame.compFixedRef || left->refIdx[0] == INTRA_FRAME);
        else
            return 4;
    } else if (above)
        return (above->refIdx[1] == NONE_FRAME) ? (above->refIdx[0] == frame.compFixedRef) : 3;
    else if (left)
        return (left->refIdx[1] == NONE_FRAME) ? (left->refIdx[0] == frame.compFixedRef) : 3;
    else
        return 1;
}

int32_t H265Enc::GetCtxReferenceMode(const ModeInfo *above, const ModeInfo *left)
{
    if (above && left) {  // both edges available
        if (!has_second_ref(above) && !has_second_ref(left)) // neither edge uses comp pred (0/1)
            return is_backward_ref_frame(above->refIdx[0]) ^ is_backward_ref_frame(left->refIdx[0]);
        else if (!has_second_ref(above)) // one of two edges uses comp pred (2/3)
            return 2 + (is_backward_ref_frame(above->refIdx[0]) || !is_inter_block(above));
        else if (!has_second_ref(left)) // one of two edges uses comp pred (2/3)
            return 2 + (is_backward_ref_frame(left->refIdx[0]) || !is_inter_block(left));
        else  // both edges use comp pred (4)
            return 4;
    } else if (above || left) {  // one edge available
        const ModeInfo *edge = above ? above : left;
        if (!has_second_ref(edge)) // edge does not use comp pred (0/1)
            return is_backward_ref_frame(edge->refIdx[0]);
        else // edge uses comp pred (3)
            return 3;
    } else {  // no edges available (1)
        return 1;
    }
}

int32_t H265Enc::GetCtxCompRef(const ModeInfo *above, const ModeInfo *left, const Frame &frame)
{
    int32_t fixRefIdx = frame.refFrameSignBiasVp9[frame.compFixedRef];
    int32_t varRefIdx = !fixRefIdx;
    if (above && left) {
        if (above->refIdx[0] == INTRA_FRAME && left->refIdx[0] == INTRA_FRAME) {
            return 2;
        } else if (left->refIdx[0] == INTRA_FRAME) {
            return (above->refIdx[1] == NONE_FRAME)
                ? 1 + 2 * (above->refIdx[0] != frame.compVarRef[1])
                : 1 + 2 * (above->refIdx[varRefIdx] != frame.compVarRef[1]);
        } else if (above->refIdx[0] == INTRA_FRAME) {
            return (left->refIdx[1] == NONE_FRAME)
                ? 1 + 2 * (left->refIdx[0] != frame.compVarRef[1])
                : 1 + 2 * (left->refIdx[varRefIdx] != frame.compVarRef[1]);
        } else {
            int32_t vrfa = above->refIdx[1] == NONE_FRAME ? above->refIdx[0] : above->refIdx[varRefIdx];
            int32_t vrfl = left->refIdx[1] == NONE_FRAME ? left->refIdx[0] : left->refIdx[varRefIdx];
            if (vrfa == vrfl && frame.compVarRef[1] == vrfa) {
                return 0;
            } else if (left->refIdx[1] == NONE_FRAME && above->refIdx[1] == NONE_FRAME) {
                if ((vrfa == frame.compFixedRef && vrfl == frame.compVarRef[0]) ||
                    (vrfl == frame.compFixedRef && vrfa == frame.compVarRef[0]))
                    return 4;
                else
                    return (vrfa == vrfl) ? 3 : 1;
            } else if (left->refIdx[1] == NONE_FRAME || above->refIdx[1] == NONE_FRAME) {
                int32_t vrfc = left->refIdx[1] == NONE_FRAME ? vrfa : vrfl;
                int32_t rfs = above->refIdx[1] == NONE_FRAME ? vrfa : vrfl;
                return (vrfc == frame.compVarRef[1] && rfs != frame.compVarRef[1])
                    ? 1
                    : (rfs == frame.compVarRef[1] && vrfc != frame.compVarRef[1]) ? 2 : 4;
            } else if (vrfa == vrfl) {
                return 4;
            } else {
                return 2;
            }
        }
    } else if (above) {
        if (above->refIdx[0] == INTRA_FRAME) {
            return 2;
        } else {
            return (above->refIdx[1] == NONE_FRAME)
                ? 3 * (above->refIdx[0] != frame.compVarRef[1])
                : 4 * (above->refIdx[varRefIdx] != frame.compVarRef[1]);
        }
    } else if (left) {
        if (left->refIdx[0] == INTRA_FRAME) {
            return 2;
        } else {
            return (left->refIdx[1] == NONE_FRAME)
                ? 3 * (left->refIdx[0] != frame.compVarRef[1])
                : 4 * (left->refIdx[varRefIdx] != frame.compVarRef[1]);
        }
    } else {
        return 2;
    }
}

int32_t H265Enc::GetCtxInterpVP9(const ModeInfo *above, const ModeInfo *left)
{
    int32_t leftInterp = (left && left->refIdx[0] > INTRA_FRAME) ? left->interp : 3;
    int32_t aboveInterp = (above && above->refIdx[0] > INTRA_FRAME) ? above->interp : 3;
    if (leftInterp == aboveInterp)
        return leftInterp;
    else if (leftInterp == 3 && aboveInterp != 3)
        return aboveInterp;
    else if (leftInterp != 3 && aboveInterp == 3)
        return leftInterp;
    else
        return 3;
}

static const int32_t INTER_FILTER_DIR_OFFSET = (SWITCHABLE_FILTERS + 1) * 2;
static const int32_t INTER_FILTER_COMP_OFFSET = SWITCHABLE_FILTERS + 1;

namespace {
    int32_t get_ref_filter_type(const ModeInfo *ref_mi, int32_t dir, int8_t ref_frame)
    {
        return (ref_mi && (ref_frame == ref_mi->refIdx[0] || ref_frame == ref_mi->refIdx[1]))
            ? ((ref_mi->interp >> (dir << 2)) & 0xf)
            : SWITCHABLE_FILTERS;
    }
};

template <int32_t dir>
int32_t H265Enc::GetCtxInterpAV1(const ModeInfo *above, const ModeInfo *left, const RefIdx refIdx[2])
{
    static_assert(dir == 0 || dir == 1, "dir must be 0 or 1");

    const int32_t filter_type_ctx = INTER_FILTER_COMP_OFFSET * (refIdx[1] > INTRA_FRAME)
                                 + INTER_FILTER_DIR_OFFSET * dir;

    const int32_t left_type = get_ref_filter_type(left, dir, refIdx[0]);
    const int32_t above_type = get_ref_filter_type(above, dir, refIdx[0]);

    if (left_type == above_type)
        return filter_type_ctx + left_type;
    if (left_type == SWITCHABLE_FILTERS)
        return filter_type_ctx + above_type;
    if (above_type == SWITCHABLE_FILTERS)
        return filter_type_ctx + left_type;
    return filter_type_ctx + SWITCHABLE_FILTERS;
}
template int32_t H265Enc::GetCtxInterpAV1<0>(const ModeInfo *above, const ModeInfo *left, const RefIdx refIdx[2]);
template int32_t H265Enc::GetCtxInterpAV1<1>(const ModeInfo *above, const ModeInfo *left, const RefIdx refIdx[2]);

const uint8_t tab[4][4] = {
    { 0, 3, 3, 0 },
    { 3, 1, 3, 1 },
    { 3, 3, 2, 2 },
    { 0, 1, 2, 3 },
};

int32_t H265Enc::GetCtxInterpBothAV1(const ModeInfo *above, const ModeInfo *left, const RefIdx refIdx[2])
{
    int32_t left_type0 = SWITCHABLE_FILTERS;
    int32_t left_type1 = SWITCHABLE_FILTERS;
    int32_t above_type0 = SWITCHABLE_FILTERS;
    int32_t above_type1 = SWITCHABLE_FILTERS;
    int32_t filter_ctx0 = SWITCHABLE_FILTERS;
    int32_t filter_ctx1 = SWITCHABLE_FILTERS;

    if (left && (left->refIdx[0] == refIdx[0] || left->refIdx[1] == refIdx[0])) {
        left_type0 = left->interp & 15;
        left_type1 = left->interp >> 4;
    }
    if (above && (above->refIdx[0] == refIdx[0] || above->refIdx[1] == refIdx[0])) {
        above_type0 = above->interp & 15;
        above_type1 = above->interp >> 4;
    }

    const int32_t comp_offset = (refIdx[1] > INTRA_FRAME) ? INTER_FILTER_COMP_OFFSET * 257 : 0;

    if      (left_type0 == above_type0)         filter_ctx0 = left_type0;
    else if (left_type0 == SWITCHABLE_FILTERS)  filter_ctx0 = above_type0;
    else if (above_type0 == SWITCHABLE_FILTERS) filter_ctx0 = left_type0;

    if      (left_type1 == above_type1)         filter_ctx1 = left_type1;
    else if (left_type1 == SWITCHABLE_FILTERS)  filter_ctx1 = above_type1;
    else if (above_type1 == SWITCHABLE_FILTERS) filter_ctx1 = left_type1;

    return comp_offset + filter_ctx0 + ((filter_ctx1 + INTER_FILTER_DIR_OFFSET) << 8);
}

int32_t H265Enc::GetCtxInterpBothAV1Fast(const ModeInfo *above, const ModeInfo *left, const RefIdx refIdx[2])
{
    const RefIdx refIdx0 = refIdx[0];
    const int32_t left_type  = (left  && (left->refIdx[0]  == refIdx0 ||  left->refIdx[1] == refIdx0)) ? DEF_FILTER : SWITCHABLE_FILTERS;
    const int32_t above_type = (above && (above->refIdx[0] == refIdx0 || above->refIdx[1] == refIdx0)) ? DEF_FILTER : SWITCHABLE_FILTERS;
    const int32_t filter_ctx = (left_type == above_type) ? left_type : DEF_FILTER;
    const int32_t comp_offset = (refIdx[1] > INTRA_FRAME) ? INTER_FILTER_COMP_OFFSET * 257 : 0;

    return comp_offset + filter_ctx + ((filter_ctx + INTER_FILTER_DIR_OFFSET) << 8);
}

int32_t H265Enc::GetCtxIsInter(const ModeInfo *above, const ModeInfo *left)
{
    if (left && above) {
        int32_t leftIntra  = left->refIdx[0] == INTRA_FRAME;
        int32_t aboveIntra = above->refIdx[0] == INTRA_FRAME;
        return (leftIntra && aboveIntra) ? 3 : (leftIntra | aboveIntra);
    } else if (above) {
        int32_t aboveIntra = above->refIdx[0] == INTRA_FRAME;
        return 2 * aboveIntra;
    } else if (left) {
        int32_t leftIntra  = left->refIdx[0] == INTRA_FRAME;
        return 2 * leftIntra;
    } else
        return 0;
}

int32_t H265Enc::GetCtxSingleRefP1(const ModeInfo *above, const ModeInfo *left)
{
    if (above && left) {
        int32_t aboveIntra = above->refIdx[0] == INTRA_FRAME;
        RefIdx aboveRefFrame0 = above->refIdx[0];
        RefIdx aboveRefFrame1 = above->refIdx[1];
        int32_t aboveSingle = (aboveRefFrame0 != INTRA_FRAME && aboveRefFrame1 == NONE_FRAME);
        int32_t leftIntra = left->refIdx[0] == INTRA_FRAME;
        RefIdx leftRefFrame0 = left->refIdx[0];
        RefIdx leftRefFrame1 = left->refIdx[1];
        int32_t leftSingle = (leftRefFrame0 != INTRA_FRAME && leftRefFrame1 == NONE_FRAME);
        if (aboveIntra && leftIntra) {
            return 2;
        } else if (leftIntra) {
            return (aboveSingle)
                ? 4 * (aboveRefFrame0 == LAST_FRAME)
                : 1 + (aboveRefFrame0 == LAST_FRAME || aboveRefFrame1 == LAST_FRAME);
        } else if (aboveIntra) {
            return (leftSingle)
                ? 4 * (leftRefFrame0 == LAST_FRAME)
                : 1 + (leftRefFrame0 == LAST_FRAME || leftRefFrame1 == LAST_FRAME);
        } else {
            if (aboveSingle && leftSingle) {
                return 2 * (aboveRefFrame0 == LAST_FRAME) + 2 * (leftRefFrame0 == LAST_FRAME);
            } else if (!aboveSingle && !leftSingle) {
                return 1 + (aboveRefFrame0 == LAST_FRAME || aboveRefFrame1 == LAST_FRAME
                    || leftRefFrame0 == LAST_FRAME || leftRefFrame1 == LAST_FRAME);
            } else {
                int32_t rfs = aboveSingle ? aboveRefFrame0 : leftRefFrame0;
                int32_t crf1 = aboveSingle ? leftRefFrame0 : aboveRefFrame0;
                int32_t crf2 = aboveSingle ? leftRefFrame1 : aboveRefFrame1;
                return (crf1 == LAST_FRAME || crf2 == LAST_FRAME) + 3 * (rfs == LAST_FRAME);
            }
        }
    } else if (above) {
        int32_t aboveIntra = above->refIdx[0] == INTRA_FRAME;
        RefIdx aboveRefFrame0 = above->refIdx[0];
        RefIdx aboveRefFrame1 = above->refIdx[1];
        int32_t aboveSingle = (aboveRefFrame0 != INTRA_FRAME && aboveRefFrame1 == NONE_FRAME);
        if (aboveIntra) {
            return 2;
        } else {
            return (aboveSingle)
                ? 4 * (aboveRefFrame0 == LAST_FRAME)
                : 1 + (aboveRefFrame0 == LAST_FRAME || aboveRefFrame1 == LAST_FRAME);
        }
    } else if (left) {
        int32_t leftIntra = left->refIdx[0] == INTRA_FRAME;
        RefIdx leftRefFrame0 = left->refIdx[0];
        RefIdx leftRefFrame1 = left->refIdx[1];
        int32_t leftSingle = (leftRefFrame0 != INTRA_FRAME && leftRefFrame1 == NONE_FRAME);
        if (leftIntra) {
            return 2;
        } else {
            return (leftSingle)
                ? 4 * (leftRefFrame0 == LAST_FRAME)
                : 1 + (leftRefFrame0 == LAST_FRAME || leftRefFrame1 == LAST_FRAME);
        }
    } else {
        return 2;
    }
}

int32_t H265Enc::GetCtxSingleRefP2(const ModeInfo *above, const ModeInfo *left)
{
    if (above && left) {
        int32_t aboveIntra = above->refIdx[0] == INTRA_FRAME;
        RefIdx aboveRefFrame0 = above->refIdx[0];
        RefIdx aboveRefFrame1 = above->refIdx[1];
        int32_t aboveSingle = (aboveRefFrame0 != INTRA_FRAME && aboveRefFrame1 == NONE_FRAME);
        int32_t leftIntra = left->refIdx[0] == INTRA_FRAME;
        RefIdx leftRefFrame0 = left->refIdx[0];
        RefIdx leftRefFrame1 = left->refIdx[1];
        int32_t leftSingle = (leftRefFrame0 != INTRA_FRAME && leftRefFrame1 == NONE_FRAME);
        if (aboveIntra && leftIntra) {
            return 2;
        } else if (leftIntra) {
            if (aboveSingle)
                return (aboveRefFrame0 == LAST_FRAME) ? 3 : 4 * (aboveRefFrame0 == GOLDEN_FRAME);
            else
                return 1 + 2 * (aboveRefFrame0 == GOLDEN_FRAME || aboveRefFrame1 == GOLDEN_FRAME);
        } else if (aboveIntra) {
            if (leftSingle)
                return (leftRefFrame0 == LAST_FRAME) ? 3 : 4 * (leftRefFrame0 == GOLDEN_FRAME);
            else
                return 1 + 2 * (leftRefFrame0 == GOLDEN_FRAME || leftRefFrame1 == GOLDEN_FRAME);
        } else {
            if (aboveSingle && leftSingle) {
                if (aboveRefFrame0 == LAST_FRAME && leftRefFrame0 == LAST_FRAME)
                    return 3;
                else if (aboveRefFrame0 == LAST_FRAME)
                    return 4 * (leftRefFrame0 == GOLDEN_FRAME);
                else if (leftRefFrame0 == LAST_FRAME)
                    return 4 * (aboveRefFrame0 == GOLDEN_FRAME);
                else
                    return 2 * (aboveRefFrame0 == GOLDEN_FRAME) + 2 * (leftRefFrame0 == GOLDEN_FRAME);
            } else if (!aboveSingle && !leftSingle) {
                if (aboveRefFrame0 == leftRefFrame0 && aboveRefFrame1 == leftRefFrame1)
                    return 3 * (aboveRefFrame0 == GOLDEN_FRAME || aboveRefFrame1 == GOLDEN_FRAME);
                else
                    return 2;
            } else {
                int32_t rfs = aboveSingle ? aboveRefFrame0 : leftRefFrame0;
                int32_t crf1 = aboveSingle ? leftRefFrame0 : aboveRefFrame0;
                int32_t crf2 = aboveSingle ? leftRefFrame1 : aboveRefFrame1;
                if (rfs == GOLDEN_FRAME)
                    return 3 + (crf1 == GOLDEN_FRAME || crf2 == GOLDEN_FRAME);
                else if (rfs == ALTREF_FRAME)
                    return (crf1 == GOLDEN_FRAME || crf2 == GOLDEN_FRAME);
                else
                    return 1 + 2 * (crf1 == GOLDEN_FRAME || crf2 == GOLDEN_FRAME);
            }
        }
    } else if (above) {
        int32_t aboveIntra = above->refIdx[0] == INTRA_FRAME;
        RefIdx aboveRefFrame0 = above->refIdx[0];
        RefIdx aboveRefFrame1 = above->refIdx[1];
        int32_t aboveSingle = (aboveRefFrame0 != INTRA_FRAME && aboveRefFrame1 == NONE_FRAME);
        if (aboveIntra || (aboveRefFrame0 == LAST_FRAME && aboveSingle))
            return 2;
        else if (aboveSingle)
            return 4 * (aboveRefFrame0 == GOLDEN_FRAME);
        else
            return 3 * (aboveRefFrame0 == GOLDEN_FRAME || aboveRefFrame1 == GOLDEN_FRAME);
    } else if (left) {
        int32_t leftIntra = left->refIdx[0] == INTRA_FRAME;
        RefIdx leftRefFrame0 = left->refIdx[0];
        RefIdx leftRefFrame1 = left->refIdx[1];
        int32_t leftSingle = (leftRefFrame0 != INTRA_FRAME && leftRefFrame1 == NONE_FRAME);
        if (leftIntra || (leftRefFrame0 == LAST_FRAME && leftSingle))
            return 2;
        else if (leftSingle)
            return 4 * (leftRefFrame0 == GOLDEN_FRAME);
        else
            return 3 * (leftRefFrame0 == GOLDEN_FRAME || leftRefFrame1 == GOLDEN_FRAME);
    } else {
        return 2;
    }
}

void H265Enc::GetCtxRefFrames(const RefIdx aboveRefIdx[2], const RefIdx leftRefIdx[2], int32_t fixedRefIdx, int32_t fixedRef, const int32_t varRefs[2], uint8_t (&ctx)[4])
{
    int32_t varRefIdx = 1 - fixedRefIdx;
    if (aboveRefIdx && leftRefIdx) {
        int32_t aboveIntra = aboveRefIdx[0] == INTRA_FRAME;
        RefIdx aboveRefFrame0 = aboveRefIdx[0];
        RefIdx aboveRefFrame1 = aboveRefIdx[1];
        int32_t aboveSingle = (!aboveIntra && aboveRefFrame1 == NONE_FRAME);
        int32_t leftIntra = leftRefIdx[0] == INTRA_FRAME;
        RefIdx leftRefFrame0 = leftRefIdx[0];
        RefIdx leftRefFrame1 = leftRefIdx[1];
        int32_t leftSingle = (!leftIntra && leftRefFrame1 == NONE_FRAME);
        if (aboveIntra && leftIntra) {
            ctx[0] = ctx[1] = ctx[3] = 2;
            ctx[2] = 0;
        } else if (leftIntra) {
            if (aboveSingle) {
                ctx[0] = 4 * (aboveRefFrame0 == LAST_FRAME);
                ctx[1] = (aboveRefFrame0 == LAST_FRAME) ? 3 : 4 * (aboveRefFrame0 == GOLDEN_FRAME);
                ctx[3] = 1 + 2 * (aboveRefIdx[0] != varRefs[1]);
                ctx[2] = (aboveRefIdx[0] == fixedRef);
            } else {
                ctx[0] = 1 + (aboveRefFrame0 == LAST_FRAME || aboveRefFrame1 == LAST_FRAME);
                ctx[1] = 1 + 2 * (aboveRefFrame0 == GOLDEN_FRAME || aboveRefFrame1 == GOLDEN_FRAME);
                ctx[3] = 1 + 2 * (aboveRefIdx[varRefIdx] != varRefs[1]);
                ctx[2] = 3;
            }
        } else if (aboveIntra) {
            if (leftSingle) {
                ctx[0] = 4 * (leftRefFrame0 == LAST_FRAME);
                ctx[1] = (leftRefFrame0 == LAST_FRAME) ? 3 : 4 * (leftRefFrame0 == GOLDEN_FRAME);;
                ctx[3] = 1 + 2 * (leftRefIdx[0] != varRefs[1]);
                ctx[2] = (leftRefIdx[0] == fixedRef);
            } else {
                ctx[0] = 1 + (leftRefFrame0 == LAST_FRAME || leftRefFrame1 == LAST_FRAME);
                ctx[1] = 1 + 2 * (leftRefFrame0 == GOLDEN_FRAME || leftRefFrame1 == GOLDEN_FRAME);
                ctx[3] = 1 + 2 * (leftRefIdx[varRefIdx] != varRefs[1]);
                ctx[2] = 3;
            }
        } else {
            int32_t vrfa = aboveRefFrame1 == NONE_FRAME ? aboveRefIdx[0] : aboveRefIdx[varRefIdx];
            int32_t vrfl = leftRefFrame1  == NONE_FRAME ? leftRefIdx[0]  : leftRefIdx[varRefIdx];
            if (aboveSingle && leftSingle) {
                ctx[2] = (aboveRefIdx[0] == fixedRef) ^ (leftRefIdx[0] == fixedRef);
                ctx[0] = 2 * (aboveRefFrame0 == LAST_FRAME) + 2 * (leftRefFrame0 == LAST_FRAME);
                if (aboveRefFrame0 == LAST_FRAME && leftRefFrame0 == LAST_FRAME)
                    ctx[1] = 3;
                else if (aboveRefFrame0 == LAST_FRAME)
                    ctx[1] = 4 * (leftRefFrame0 == GOLDEN_FRAME);
                else if (leftRefFrame0 == LAST_FRAME)
                    ctx[1] = 4 * (aboveRefFrame0 == GOLDEN_FRAME);
                else
                    ctx[1] = 2 * (aboveRefFrame0 == GOLDEN_FRAME) + 2 * (leftRefFrame0 == GOLDEN_FRAME);
            } else if (!aboveSingle && !leftSingle) {
                ctx[2] = 4;
                ctx[0] = 1 + (aboveRefFrame0 == LAST_FRAME || aboveRefFrame1 == LAST_FRAME
                            || leftRefFrame0 == LAST_FRAME || leftRefFrame1 == LAST_FRAME);
                if (aboveRefFrame0 == leftRefFrame0 && aboveRefFrame1 == leftRefFrame1)
                    ctx[1] = 3 * (aboveRefFrame0 == GOLDEN_FRAME || aboveRefFrame1 == GOLDEN_FRAME);
                else
                    ctx[1] = 2;
            } else {
                int32_t rfs = aboveSingle ? aboveRefFrame0 : leftRefFrame0;
                int32_t crf1 = aboveSingle ? leftRefFrame0 : aboveRefFrame0;
                int32_t crf2 = aboveSingle ? leftRefFrame1 : aboveRefFrame1;
                ctx[0] = (crf1 == LAST_FRAME || crf2 == LAST_FRAME) + 3 * (rfs == LAST_FRAME);
                if (rfs == GOLDEN_FRAME)
                    ctx[1] = 3 + (crf1 == GOLDEN_FRAME || crf2 == GOLDEN_FRAME);
                else if (rfs == ALTREF_FRAME)
                    ctx[1] = (crf1 == GOLDEN_FRAME || crf2 == GOLDEN_FRAME);
                else
                    ctx[1] = 1 + 2 * (crf1 == GOLDEN_FRAME || crf2 == GOLDEN_FRAME);
                if (aboveRefFrame1 == NONE_FRAME)
                    ctx[2] = 2 + (aboveRefIdx[0] == fixedRef);
                else
                    ctx[2] = 2 + (leftRefIdx[0] == fixedRef);
            }
            if (vrfa == vrfl && varRefs[1] == vrfa) {
                ctx[3] = 0;
            } else if (leftRefFrame1 == NONE_FRAME && aboveRefFrame1 == NONE_FRAME) {
                if ((vrfa == fixedRef && vrfl == varRefs[0]) ||
                    (vrfl == fixedRef && vrfa == varRefs[0]))
                    ctx[3] = 4;
                else
                    ctx[3] = (vrfa == vrfl) ? 3 : 1;
            } else if (leftRefFrame1 == NONE_FRAME || aboveRefFrame1 == NONE_FRAME) {
                int32_t vrfc = leftRefFrame1 == NONE_FRAME ? vrfa : vrfl;
                int32_t rfs = aboveRefFrame1 == NONE_FRAME ? vrfa : vrfl;
                ctx[3] = (vrfc == varRefs[1] && rfs != varRefs[1])
                    ? 1
                    : (rfs == varRefs[1] && vrfc != varRefs[1]) ? 2 : 4;
            } else if (vrfa == vrfl) {
                ctx[3] = 4;
            } else {
                ctx[3] = 2;
            }
        }
    } else if (aboveRefIdx) {
        int32_t aboveIntra = aboveRefIdx[0] == INTRA_FRAME;
        RefIdx aboveRefFrame0 = aboveRefIdx[0];
        RefIdx aboveRefFrame1 = aboveRefIdx[1];
        int32_t aboveSingle = (!aboveIntra && aboveRefFrame1 == NONE_FRAME);
        if (aboveIntra) {
            ctx[0] = ctx[3] = 2;
            ctx[2] = 0;
        } else if (aboveSingle) {
            ctx[0] = 4 * (aboveRefFrame0 == LAST_FRAME);
            ctx[3] = 3 * (aboveRefIdx[0] != varRefs[1]);
            ctx[2] = (aboveRefIdx[0] == fixedRef);
        } else {
            ctx[0] = 1 + (aboveRefFrame0 == LAST_FRAME || aboveRefFrame1 == LAST_FRAME);
            ctx[3] = 4 * (aboveRefIdx[varRefIdx] != varRefs[1]);
            ctx[2] = 3;
        }
        if (aboveIntra || (aboveRefFrame0 == LAST_FRAME && aboveSingle))
            ctx[1] = 2;
        else if (aboveSingle)
            ctx[1] = 4 * (aboveRefFrame0 == GOLDEN_FRAME);
        else
            ctx[1] = 3 * (aboveRefFrame0 == GOLDEN_FRAME || aboveRefFrame1 == GOLDEN_FRAME);
    } else if (leftRefIdx) {
        int32_t leftIntra = leftRefIdx[0] == INTRA_FRAME;
        RefIdx leftRefFrame0 = leftRefIdx[0];
        RefIdx leftRefFrame1 = leftRefIdx[1];
        int32_t leftSingle = (!leftIntra && leftRefFrame1 == NONE_FRAME);
        if (leftIntra) {
            ctx[0] = ctx[3] = 2;
            ctx[2] = 0;
        } else if (leftSingle) {
            ctx[0] = 4 * (leftRefFrame0 == LAST_FRAME);
            ctx[3] = 3 * (leftRefIdx[0] != varRefs[1]);
            ctx[2] = (leftRefIdx[0] == fixedRef);
        } else {
            ctx[0] = 1 + (leftRefFrame0 == LAST_FRAME || leftRefFrame1 == LAST_FRAME);
            ctx[3] = 4 * (leftRefIdx[varRefIdx] != varRefs[1]);
            ctx[2] = 3;
        }
        if (leftIntra || (leftRefFrame0 == LAST_FRAME && leftSingle))
            ctx[1] = 2;
        else if (leftSingle)
            ctx[1] = 4 * (leftRefFrame0 == GOLDEN_FRAME);
        else
            ctx[1] = 3 * (leftRefFrame0 == GOLDEN_FRAME || leftRefFrame1 == GOLDEN_FRAME);
    } else {
        ctx[0] = ctx[1] = ctx[3] = 2;
        ctx[2] = 1;
    }
}

int32_t H265Enc::GetCtxSkip(const ModeInfo *above, const ModeInfo *left)
{
    return (left ? (left->skip&1) : 0) + (above ? (above->skip&1) : 0);
}

int32_t H265Enc::GetCtxToken(int32_t c, int32_t x, int32_t y, int32_t num4x4, int32_t txType, const uint8_t *tokenCache, const uint8_t *aboveNz, const uint8_t *leftNz) {
    if (c == 0) {
        int32_t above = 0;
        int32_t left = 0;
        for (int32_t i = 0; i < num4x4; i++) {
            above |= aboveNz[i];
            left  |= leftNz[i];
        }
        return above + left;
    } else {
        int32_t nb[2];
        int32_t a = (y - 1) * 4*num4x4 + x;
        int32_t a2 = y * 4*num4x4 + x - 1;
        if (x > 0 && y > 0) {
            if (txType == DCT_ADST) {
                nb[0] = nb[1] = a;
            } else if (txType == ADST_DCT) {
                nb[0] = nb[1] = a2;
            } else {
                nb[0] = a;
                nb[1] = a2;
            }
        } else if (y > 0) {
            nb[0] = nb[1] = a;
        } else {
            nb[0] = nb[1] = a2;
        }
        return (1 + tokenCache[nb[0]] + tokenCache[nb[1]]) >> 1;
    }
}

int32_t H265Enc::GetCtxTxSizeVP9(const ModeInfo *above, const ModeInfo *left, TxSize maxTxSize)
{
    TxSize a = maxTxSize;
    TxSize l = maxTxSize;
    if (above && above->skip==0)
        a = above->txSize;
    if (left && left->skip==0)
        l = left->txSize;
    if (!left)
        l = a;
    if (!above)
        a = l;
    return (a + l) > maxTxSize;
}

int32_t H265Enc::GetCtxTxSizeAV1(const ModeInfo *above, const ModeInfo *left, uint8_t aboveTxfm, uint8_t leftTxfm, TxSize maxTxSize)
{
    //TxSize a = maxTxSize;
    //TxSize l = maxTxSize;
    //if (above && above->skip==0)
    //    a = txsize_sqr_map[above->txSize];
    //if (left && left->skip==0)
    //    l = txsize_sqr_map[left->txSize];
    //if (!left)
    //    l = a;
    //if (!above)
    //    a = l;
    //return (a + l) > maxTxSize;
    const int max_tx_wide = tx_size_wide[maxTxSize];
    const int max_tx_high = tx_size_high[maxTxSize];

    int a = aboveTxfm >= max_tx_wide;
    int l = leftTxfm >= max_tx_high;

    if (above)
        if (is_inter_block(above))
            a = block_size_wide[above->sbType] >= max_tx_wide;

    if (left)
        if (is_inter_block(left))
            l = block_size_high[left->sbType] >= max_tx_high;

    if (above && left) return a + l;
    else if (above)    return a;
    else if (left)     return l;
    else               return 0;
}

int32_t H265Enc::GetCtxDrlIdx(const H265MVCand *refMvStack, int32_t idx) {
    if (refMvStack[idx].weight >= REF_CAT_LEVEL &&
        refMvStack[idx + 1].weight >= REF_CAT_LEVEL)
        return 0;

    if (refMvStack[idx].weight >= REF_CAT_LEVEL &&
        refMvStack[idx + 1].weight < REF_CAT_LEVEL)
        return 2;

    if (refMvStack[idx].weight < REF_CAT_LEVEL &&
        refMvStack[idx + 1].weight < REF_CAT_LEVEL)
        return 3;

    return 0;
}

const uint8_t LUT_CTX_DLR_IDX[5][3] = {
    { 3, 3, 3 },
    { 2, 3, 3 },
    { 0, 2, 3 },
    { 0, 0, 2 },
    { 0, 0, 0 }
};

int32_t H265Enc::GetCtxDrlIdx(int32_t nearestMvCount, int32_t idx) {
    assert(nearestMvCount >= 0);
    assert(idx >= 0 && idx <= 2);
    return LUT_CTX_DLR_IDX[ MIN(nearestMvCount, 4) ][idx];
}

int32_t H265Enc::GetCtxNmv(const uint8_t refMvCount, const H265MVCand *refMvStack, int32_t ref, int32_t refMvIdx) {
    if (refMvStack[refMvIdx].weight >= REF_CAT_LEVEL && refMvIdx < refMvCount)
        return refMvStack[refMvIdx].predDiff[ref];
    return 0;
}

namespace {
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
}

int32_t H265Enc::GetCtxTxfm(const uint8_t *aboveTxfm, const uint8_t *leftTxfm, BlockSize bsize, TxSize txSize) {
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


int32_t H265Enc::GetCtxSingleRefP1Av1(const uint8_t *counts)
{
    const int32_t fwdCount = counts[AV1_LAST_FRAME] + counts[AV1_LAST2_FRAME] +
                            counts[AV1_LAST3_FRAME] + counts[AV1_GOLDEN_FRAME];
    const int32_t bwdCount = counts[AV1_BWDREF_FRAME] + counts[AV1_ALTREF2_FRAME] +
                            counts[AV1_ALTREF_FRAME];
    return (fwdCount == bwdCount) ? 1 : ((fwdCount < bwdCount) ? 0 : 2);
}

int32_t H265Enc::GetCtxSingleRefP2Av1(const uint8_t *counts)
{
    const int32_t brfArf2Count = counts[AV1_BWDREF_FRAME] + counts[AV1_ALTREF2_FRAME];
    const int32_t arfCount = counts[AV1_ALTREF_FRAME];
    return (brfArf2Count == arfCount) ? 1 : ((brfArf2Count < arfCount) ? 0 : 2);
}

int32_t H265Enc::GetCtxSingleRefP3(const uint8_t *counts) {
    const int32_t lastLast2Count = counts[AV1_LAST_FRAME] + counts[AV1_LAST2_FRAME];
    const int32_t last3GldCount = counts[AV1_LAST3_FRAME] + counts[AV1_GOLDEN_FRAME];
    return (lastLast2Count == last3GldCount) ? 1 : ((lastLast2Count < last3GldCount) ? 0 : 2);
}

int32_t H265Enc::GetCtxSingleRefP4(const uint8_t *counts)
{
    const int32_t lastCount = counts[AV1_LAST_FRAME];
    const int32_t last2Count = counts[AV1_LAST2_FRAME];
    return (lastCount == last2Count) ? 1 : ((lastCount < last2Count) ? 0 : 2);
}

int32_t H265Enc::GetCtxSingleRefP5(const uint8_t *counts)
{
    const int32_t last3Count = counts[AV1_LAST3_FRAME];
    const int32_t gldCount = counts[AV1_GOLDEN_FRAME];
    return (last3Count == gldCount) ? 1 : ((last3Count < gldCount) ? 0 : 2);
}

int32_t H265Enc::GetCtxCompRefType(const ModeInfo *above, const ModeInfo *left) {
    int32_t pred_context;
    const int32_t above_in_image = (above != NULL);
    const int32_t left_in_image = (left != NULL);

    if (above_in_image && left_in_image) {  // both edges available
        const int above_intra = !is_inter_block(above);
        const int left_intra = !is_inter_block(left);

        if (above_intra && left_intra) {  // intra/intra
            pred_context = 2;
        } else if (above_intra || left_intra) {  // intra/inter
            const ModeInfo *inter = above_intra ? left : above;

            if (!has_second_ref(inter))  // single pred
                pred_context = 2;
            else  // comp pred
                pred_context = 1 + 2 * has_uni_comp_refs(inter);
        } else {  // inter/inter
            const int a_sg = !has_second_ref(above);
            const int l_sg = !has_second_ref(left);
            const RefIdx frfa = above->refIdx[0];
            const RefIdx frfl = left->refIdx[0];

            if (a_sg && l_sg) {  // single/single
                pred_context =
                    1 +
                    2 * (!(is_backward_ref_frame(frfa) ^ is_backward_ref_frame(frfl)));
            } else if (l_sg || a_sg) {  // single/comp
                const int uni_rfc =
                    a_sg ? has_uni_comp_refs(left) : has_uni_comp_refs(above);

                if (!uni_rfc)  // comp bidir
                    pred_context = 1;
                else  // comp unidir
                    pred_context = 3 + (!(is_backward_ref_frame(frfa) ^ is_backward_ref_frame(frfl)));
            } else {  // comp/comp
                const int a_uni_rfc = has_uni_comp_refs(above);
                const int l_uni_rfc = has_uni_comp_refs(left);

                if (!a_uni_rfc && !l_uni_rfc)  // bidir/bidir
                    pred_context = 0;
                else if (!a_uni_rfc || !l_uni_rfc)  // unidir/bidir
                    pred_context = 2;
                else  // unidir/unidir
                    pred_context =
                    3 + (!((frfa == AV1_BWDREF_FRAME) ^ (frfl == AV1_BWDREF_FRAME)));
            }
        }
    } else if (above_in_image || left_in_image) {  // one edge available
        const ModeInfo *edge = above_in_image ? above : left;

        if (!is_inter_block(edge)) {  // intra
            pred_context = 2;
        } else {                           // inter
            if (!has_second_ref(edge))  // single pred
                pred_context = 2;
            else  // comp pred
                pred_context = 4 * has_uni_comp_refs(edge);
        }
    } else {  // no edges available
        pred_context = 2;
    }

    assert(pred_context >= 0 && pred_context < COMP_REF_TYPE_CONTEXTS);
    return pred_context;
}

int32_t H265Enc::GetCtxCompRefP0(const uint8_t *counts) {

    const int32_t lastLast2Count = counts[AV1_LAST_FRAME] + counts[AV1_LAST2_FRAME];
    const int32_t last3GldCount = counts[AV1_LAST3_FRAME] + counts[AV1_GOLDEN_FRAME];
    return (lastLast2Count == last3GldCount) ? 1 : ((lastLast2Count < last3GldCount) ? 0 : 2);
}

int32_t H265Enc::GetCtxCompRefP1(const uint8_t *counts)
{
    const int32_t lastCount = counts[AV1_LAST_FRAME];
    const int32_t last2Count = counts[AV1_LAST2_FRAME];
    return (lastCount == last2Count) ? 1 : ((lastCount < last2Count) ? 0 : 2);
}

int32_t H265Enc::GetCtxCompBwdRefP0(const uint8_t *counts)
{
    const int32_t brfArf2Count = counts[AV1_BWDREF_FRAME] + counts[AV1_ALTREF2_FRAME];
    const int32_t arfCount = counts[AV1_ALTREF_FRAME];
    return (brfArf2Count == arfCount) ? 1 : ((brfArf2Count < arfCount) ? 0 : 2);
}

void H265Enc::CollectNeighborsRefCounts(const ModeInfo *above, const ModeInfo *left, uint8_t *neighborsRefCounts)
{
    uint8_t tmp[3] = {};
    if (above && above->refIdx[0] != INTRA_FRAME) {
        tmp[ above->refIdx[0] ]++;
        if (above->refIdx[1] > INTRA_FRAME)
            tmp[ above->refIdx[1] ]++;
    }
    if (left && left->refIdx[0] != INTRA_FRAME) {
        tmp[ left->refIdx[0] ]++;
        if (left->refIdx[1] > INTRA_FRAME)
            tmp[ left->refIdx[1] ]++;
    }
    neighborsRefCounts[AV1_LAST_FRAME] = tmp[LAST_FRAME];
    neighborsRefCounts[AV1_LAST2_FRAME] = 0;
    neighborsRefCounts[AV1_LAST3_FRAME] = 0;
    neighborsRefCounts[AV1_GOLDEN_FRAME] = tmp[GOLDEN_FRAME];
    neighborsRefCounts[AV1_BWDREF_FRAME] = 0;
    neighborsRefCounts[AV1_ALTREF2_FRAME] = 0;
    neighborsRefCounts[AV1_ALTREF_FRAME] = tmp[ALTREF_FRAME];
}
