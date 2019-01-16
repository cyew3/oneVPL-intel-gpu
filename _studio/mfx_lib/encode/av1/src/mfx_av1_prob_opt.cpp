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

#include "numeric"
#include "mfx_av1_enc.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_prob_opt.h"
#include "mfx_av1_bitwriter.h"
#include "mfx_av1_get_context.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_binarization.h"

#include <algorithm>

using namespace AV1Enc;

namespace {
    void ProcessRefFrames(const Frame &frame, const ModeInfo &mi, FrameCounts *counts) {
        int32_t compMode = (mi.refIdx[1] != NONE_FRAME);
        if (frame.referenceMode == REFERENCE_MODE_SELECT)
            counts->comp_mode[ mi.memCtx.compMode ][compMode]++;

        if (compMode == COMPOUND_REFERENCE) {
            const int32_t compRef = mi.refIdxComb - COMP_VAR0;
            counts->vp9_comp_ref[ mi.memCtx.compRef ][compRef]++;
        } else {
            if (mi.refIdx[0] == LAST_FRAME) {
                counts->single_ref[ mi.memCtx.singleRefP1 ][0][0]++; // single_ref_p1
            } else if (mi.refIdx[0] == GOLDEN_FRAME) {
                counts->single_ref[ mi.memCtx.singleRefP1 ][0][1]++; // single_ref_p1
                counts->single_ref[ mi.memCtx.singleRefP2 ][1][0]++; // single_ref_p2
            } else { assert(mi.refIdx[0] == ALTREF_FRAME);
                counts->single_ref[ mi.memCtx.singleRefP1 ][0][1]++; // single_ref_p1
                counts->single_ref[ mi.memCtx.singleRefP2 ][1][1]++; // single_ref_p2
            }
        }
    }

    void ProcessMvComponent(int16_t diffmv, int32_t useHp, int32_t comp, FrameCounts *counts) {
        counts->mv_sign[comp][(diffmv < 0)]++;
        diffmv = abs(diffmv)-1;
        if (diffmv < 16) {
            counts->mv_class[comp][0]++;
            int32_t mv_class0_bit = diffmv >> 3;
            int32_t mv_class0_fr = (diffmv >> 1) & 3;
            int32_t mv_class0_hp = diffmv & 1;
            counts->mv_class0_bit[comp][mv_class0_bit]++;
            counts->mv_class0_fr[comp][mv_class0_bit][mv_class0_fr];
            if (useHp)
                counts->mv_class0_hp[comp][mv_class0_hp]++;
        } else {
            int32_t mvClass = MIN(MV_CLASS_10, BSR(diffmv >> 3));
            counts->mv_class[comp][mvClass]++;
            diffmv -= CLASS0_SIZE << (mvClass + 2);
            int32_t intdiff = diffmv>>3;
            for (int32_t i = 0; i < mvClass; i++, intdiff >>= 1)
                counts->mv_bits[comp][i][intdiff & 1]++;
            int32_t mv_fr = (diffmv >> 1) & 3;
            int32_t mv_hp = diffmv & 1;
            counts->mv_fr[comp][mv_fr]++;
            if (useHp)
                counts->mv_hp[comp][mv_hp]++;
        }
    }

    void ProcessMv(AV1MV mv, AV1MV mvd, int32_t allowHighPrecisionMv, FrameCounts *counts) {
        AV1MV bestMv = { (int16_t)(mv.mvx - mvd.mvx), (int16_t)(mv.mvy - mvd.mvy) };
        int32_t useHp = allowHighPrecisionMv && AV1Enc::UseMvHp(bestMv);

        int32_t mvJoint = (mvd.mvx == 0) ? MV_JOINT_ZERO : MV_JOINT_HNZVZ;
        if (mvd.mvy != 0) mvJoint |= MV_JOINT_HZVNZ;
        counts->mv_joint[mvJoint]++;

        if (mvJoint & MV_JOINT_HZVNZ)
            ProcessMvComponent(mvd.mvy, useHp, MV_COMP_VER, counts);
        if (mvJoint & MV_JOINT_HNZVZ)
            ProcessMvComponent(mvd.mvx, useHp, MV_COMP_HOR, counts);
    }

    void ProcessInterBlockModeInfo(SuperBlock &sb, const ModeInfo *mi, FrameCounts *counts) {
        const AV1VideoParam &par = *sb.par;
        Frame &frame = *sb.frame;

        ProcessRefFrames(frame, *mi, counts);

        if (frame.interpolationFilter == SWITCHABLE) {
            const int32_t miRow = (mi - sb.frame->m_modeInfo) / sb.par->miPitch;
            const int32_t miCol = (mi - sb.frame->m_modeInfo) % sb.par->miPitch;
            const ModeInfo *above = GetAbove(mi, sb.par->miPitch, miRow, sb.par->tileParam.miRowStart[sb.tileRow]);
            const ModeInfo *left = GetLeft(mi, miCol, sb.par->tileParam.miColStart[sb.tileCol]);
            const int32_t ctx = GetCtxInterpBothAV1(above, left, mi->refIdx);

            counts->interp_filter[ ctx & 15 ][mi->interp0]++;
            counts->interp_filter[ ctx >> 8 ][mi->interp1]++;
        }
        if (mi->sbType < BLOCK_8X8) {
            assert(0);
        } else if (mi->mode == NEWMV) {
            ProcessMv(mi->mv[0], mi->mvd[0], sb.par->allowHighPrecisionMv, counts);
            if (mi->refIdx[1] >= 0)
                ProcessMv(mi->mv[1], mi->mvd[1], sb.par->allowHighPrecisionMv, counts);
        }
    }

    void ProcessIntraBlockModeInfo(SuperBlock &sb, const ModeInfo *mi, FrameCounts *counts) {
        if (mi->sbType >= BLOCK_8X8) {
            counts->y_mode[ size_group_lookup[mi->sbType] ][mi->mode]++;
        } else if (mi->sbType == BLOCK_4X8) {
            counts->y_mode[0][mi[0].mode]++;
            counts->y_mode[0][mi[0].mode]++;
        } else if (mi->sbType == BLOCK_8X4) {
            counts->y_mode[0][mi[0].mode]++;
            counts->y_mode[0][mi[0].mode]++;
        } else { assert(mi->sbType == BLOCK_4X4);
            counts->y_mode[0][mi[0].mode]++;
            counts->y_mode[0][mi[0].mode]++;
            counts->y_mode[0][mi[0].mode]++;
            counts->y_mode[0][mi[0].mode]++;
        }
        counts->uv_mode[mi->mode][mi->modeUV]++;
    }

    void ProcessTxSize(int32_t allowSelect, SuperBlock &sb, const ModeInfo *mi, FrameCounts *counts) {
        BlockSize bsize = mi->sbType;
        if (allowSelect && sb.par->txMode == TX_MODE_SELECT && bsize >= BLOCK_8X8) {
            const TxSize maxTxSize = max_txsize_lookup[bsize];
            const int32_t miColStart = 0; // tiles are no supported yet
            const TxSize txSize = mi->txSize;
            const int32_t ctx = mi->memCtx.txSize;
#ifdef AV1_DEBUG_CONTEXT_MEMOIZATION
            const int32_t miRow = (mi - sb.frame->m_modeInfo) / sb.par->miPitch;
            const int32_t miCol = (mi - sb.frame->m_modeInfo) % sb.par->miPitch;
            const ModeInfo *above = GetAbove(mi, sb.par->miPitch, miRow, sb.par->tileMiRowStart[sb.tileRow]);
            const ModeInfo *left = GetLeft(mi, miCol, sb.par->tileMiColStart[sb.tileCol]);
            assert(ctx == GetCtxTxSize(above, left, maxTxSize));
#endif // AV1_DEBUG_CONTEXT_MEMOIZATION
            counts->tx[maxTxSize][ctx][txSize]++;
        }
    }

    void ProcessSkip(SuperBlock &sb, const ModeInfo *mi, FrameCounts *counts) {
        counts->skip[ mi->memCtx.skip ][ mi->skip ]++;
    }

    void ProcessIntraFrameModeInfo(SuperBlock &sb, int32_t miRow, int32_t miCol, FrameCounts *counts) {
        const ModeInfo *mi = sb.frame->m_modeInfo + miCol + miRow * sb.par->miPitch;
        //ProcessIntraSegmentId(*sb.frame);
        ProcessSkip(sb, mi, counts);
        ProcessTxSize(1, sb, mi, counts);
    }

    void ProcessInterFrameModeInfo(SuperBlock &sb, int32_t miRow, int32_t miCol, FrameCounts *counts) {
        const ModeInfo *mi = sb.frame->m_modeInfo + miCol + miRow * sb.par->miPitch;
        //ProcessIntraSegmentId(*sb.frame);

        const int32_t skip = mi->skip;
        const int32_t isInter = (mi->refIdx[0] != INTRA_FRAME);
        counts->skip[ mi->memCtx.skip ][skip]++;
        counts->is_inter[ mi->memCtx.isInter ][isInter]++;

        ProcessTxSize((!skip || !isInter), sb, mi, counts);
        if (isInter)
            ProcessInterBlockModeInfo(sb, mi, counts);
        else
            ProcessIntraBlockModeInfo(sb, mi, counts);
    }

    static const int32_t MAX_NEIGHBORS = 2;
    static const int16_t neighbours_dct_dct_4x4[MAX_NEIGHBORS * 16] = {
        0,0, 0,0, 0,0, 1,4,
        4,4, 1,1, 8,8, 5,8,
        2,2, 2,5, 9,12, 6,9,
        3,6, 10,13, 7,10, 11,14,
    };
    static const int16_t neighbours_adst_dct_4x4[MAX_NEIGHBORS * 16] = {
        0,0, 0,0, 0,0, 1,1,
        4,4, 2,2, 5,5, 4,4,
        8,8, 6,6, 8,8, 9,9,
        12,12, 10,10, 13,13, 14,14,
    };
    static const int16_t neighbours_dct_adst_4x4[MAX_NEIGHBORS * 16] = {
        0,0, 0,0, 4,4, 0,0,
        8,8, 1,1, 5,5, 1,1,
        9,9, 2,2, 6,6, 2,2,
        3,3, 10,10, 7,7, 11,11,
    };
    static const int16_t neighbours_dct_dct_8x8[MAX_NEIGHBORS * 64] = {
        0,0, 0,0, 0,0, 8,8, 1,8, 1,1, 9,16, 16,16,
        2,9, 2,2, 10,17, 17,24, 24,24, 3,10, 3,3, 18,25,
        25,32, 11,18, 32,32, 4,11, 26,33, 19,26, 4,4, 33,40,
        12,19, 40,40, 5,12, 27,34, 34,41, 20,27, 13,20, 5,5,
        41,48, 48,48, 28,35, 35,42, 21,28, 6,6, 6,13, 42,49,
        49,56, 36,43, 14,21, 29,36, 7,14, 43,50, 50,57, 22,29,
        37,44, 15,22, 44,51, 51,58, 30,37, 23,30, 52,59, 45,52,
        38,45, 31,38, 53,60, 46,53, 39,46, 54,61, 47,54, 55,62,
    };
    static const int16_t neighbours_adst_dct_8x8[MAX_NEIGHBORS * 64] = {
        0,0, 0,0, 1,1, 0,0, 8,8, 2,2, 8,8, 9,9,
        3,3, 16,16, 10,10, 16,16, 4,4, 17,17, 24,24, 11,11,
        18,18, 25,25, 24,24, 5,5, 12,12, 19,19, 32,32, 26,26,
        6,6, 33,33, 32,32, 20,20, 27,27, 40,40, 13,13, 34,34,
        40,40, 41,41, 28,28, 35,35, 48,48, 21,21, 42,42, 14,14,
        48,48, 36,36, 49,49, 43,43, 29,29, 56,56, 22,22, 50,50,
        57,57, 44,44, 37,37, 51,51, 30,30, 58,58, 52,52, 45,45,
        59,59, 38,38, 60,60, 46,46, 53,53, 54,54, 61,61, 62,62,
    };
    static const int16_t neighbours_dct_adst_8x8[MAX_NEIGHBORS * 64] = {
        0,0, 0,0, 8,8, 0,0, 16,16, 1,1, 24,24, 9,9,
        1,1, 32,32, 17,17, 2,2, 25,25, 10,10, 40,40, 2,2,
        18,18, 33,33, 3,3, 48,48, 11,11, 26,26, 3,3, 41,41,
        19,19, 34,34, 4,4, 27,27, 12,12, 49,49, 42,42, 20,20,
        4,4, 35,35, 5,5, 28,28, 50,50, 43,43, 13,13, 36,36,
        5,5, 21,21, 51,51, 29,29, 6,6, 44,44, 14,14, 6,6,
        37,37, 52,52, 22,22, 7,7, 30,30, 45,45, 15,15, 38,38,
        23,23, 53,53, 31,31, 46,46, 39,39, 54,54, 47,47, 55,55,
    };
    static const int16_t neighbours_dct_dct_16x16[MAX_NEIGHBORS * 256] = {
        0,0, 0,0, 0,0, 16,16, 1,16, 1,1, 32,32, 17,32, 2,17, 2,2, 48,48, 18,33, 33,48, 3,18, 49,64, 64,64,
        34,49, 3,3, 19,34, 50,65, 4,19, 65,80, 80,80, 35,50, 4,4, 20,35, 66,81, 81,96, 51,66, 96,96, 5,20, 36,51,
        82,97, 21,36, 67,82, 97,112, 5,5, 52,67, 112,112, 37,52, 6,21, 83,98, 98,113, 68,83, 6,6, 113,128, 22,37, 53,68,
        84,99, 99,114, 128,128, 114,129, 69,84, 38,53, 7,22, 7,7, 129,144, 23,38, 54,69, 100,115, 85,100, 115,130, 144,144, 130,145,
        39,54, 70,85, 8,23, 55,70, 116,131, 101,116, 145,160, 24,39, 8,8, 86,101, 131,146, 160,160, 146,161, 71,86, 40,55, 9,24,
        117,132, 102,117, 161,176, 132,147, 56,71, 87,102, 25,40, 147,162, 9,9, 176,176, 162,177, 72,87, 41,56, 118,133, 133,148, 103,118,
        10,25, 148,163, 57,72, 88,103, 177,192, 26,41, 163,178, 192,192, 10,10, 119,134, 73,88, 149,164, 104,119, 134,149, 42,57, 178,193,
        164,179, 11,26, 58,73, 193,208, 89,104, 135,150, 120,135, 27,42, 74,89, 208,208, 150,165, 179,194, 165,180, 105,120, 194,209, 43,58,
        11,11, 136,151, 90,105, 151,166, 180,195, 59,74, 121,136, 209,224, 195,210, 224,224, 166,181, 106,121, 75,90, 12,27, 181,196, 12,12,
        210,225, 152,167, 167,182, 137,152, 28,43, 196,211, 122,137, 91,106, 225,240, 44,59, 13,28, 107,122, 182,197, 168,183, 211,226, 153,168,
        226,241, 60,75, 197,212, 138,153, 29,44, 76,91, 13,13, 183,198, 123,138, 45,60, 212,227, 198,213, 154,169, 169,184, 227,242, 92,107,
        61,76, 139,154, 14,29, 14,14, 184,199, 213,228, 108,123, 199,214, 228,243, 77,92, 30,45, 170,185, 155,170, 185,200, 93,108, 124,139,
        214,229, 46,61, 200,215, 229,244, 15,30, 109,124, 62,77, 140,155, 215,230, 31,46, 171,186, 186,201, 201,216, 78,93, 230,245, 125,140,
        47,62, 216,231, 156,171, 94,109, 231,246, 141,156, 63,78, 202,217, 187,202, 110,125, 217,232, 172,187, 232,247, 79,94, 157,172, 126,141,
        203,218, 95,110, 233,248, 218,233, 142,157, 111,126, 173,188, 188,203, 234,249, 219,234, 127,142, 158,173, 204,219, 189,204, 143,158, 235,250,
        174,189, 205,220, 159,174, 220,235, 221,236, 175,190, 190,205, 236,251, 206,221, 237,252, 191,206, 222,237, 207,222, 238,253, 223,238, 239,254,
    };
    static const int16_t neighbours_adst_dct_16x16[MAX_NEIGHBORS * 256] = {
        0,0, 0,0, 1,1, 0,0, 2,2, 16,16, 3,3, 17,17, 16,16, 4,4, 32,32, 18,18, 5,5, 33,33, 32,32, 19,19,
        48,48, 6,6, 34,34, 20,20, 49,49, 48,48, 7,7, 35,35, 64,64, 21,21, 50,50, 36,36, 64,64, 8,8, 65,65, 51,51,
        22,22, 37,37, 80,80, 66,66, 9,9, 52,52, 23,23, 81,81, 67,67, 80,80, 38,38, 10,10, 53,53, 82,82, 96,96, 68,68,
        24,24, 97,97, 83,83, 39,39, 96,96, 54,54, 11,11, 69,69, 98,98, 112,112, 84,84, 25,25, 40,40, 55,55, 113,113, 99,99,
        12,12, 70,70, 112,112, 85,85, 26,26, 114,114, 100,100, 128,128, 41,41, 56,56, 71,71, 115,115, 13,13, 86,86, 129,129, 101,101,
        128,128, 72,72, 130,130, 116,116, 27,27, 57,57, 14,14, 87,87, 42,42, 144,144, 102,102, 131,131, 145,145, 117,117, 73,73, 144,144,
        88,88, 132,132, 103,103, 28,28, 58,58, 146,146, 118,118, 43,43, 160,160, 147,147, 89,89, 104,104, 133,133, 161,161, 119,119, 160,160,
        74,74, 134,134, 148,148, 29,29, 59,59, 162,162, 176,176, 44,44, 120,120, 90,90, 105,105, 163,163, 177,177, 149,149, 176,176, 135,135,
        164,164, 178,178, 30,30, 150,150, 192,192, 75,75, 121,121, 60,60, 136,136, 193,193, 106,106, 151,151, 179,179, 192,192, 45,45, 165,165,
        166,166, 194,194, 91,91, 180,180, 137,137, 208,208, 122,122, 152,152, 208,208, 195,195, 76,76, 167,167, 209,209, 181,181, 224,224, 107,107,
        196,196, 61,61, 153,153, 224,224, 182,182, 168,168, 210,210, 46,46, 138,138, 92,92, 183,183, 225,225, 211,211, 240,240, 197,197, 169,169,
        123,123, 154,154, 198,198, 77,77, 212,212, 184,184, 108,108, 226,226, 199,199, 62,62, 227,227, 241,241, 139,139, 213,213, 170,170, 185,185,
        155,155, 228,228, 242,242, 124,124, 93,93, 200,200, 243,243, 214,214, 215,215, 229,229, 140,140, 186,186, 201,201, 78,78, 171,171, 109,109,
        156,156, 244,244, 216,216, 230,230, 94,94, 245,245, 231,231, 125,125, 202,202, 246,246, 232,232, 172,172, 217,217, 141,141, 110,110, 157,157,
        187,187, 247,247, 126,126, 233,233, 218,218, 248,248, 188,188, 203,203, 142,142, 173,173, 158,158, 249,249, 234,234, 204,204, 219,219, 174,174,
        189,189, 250,250, 220,220, 190,190, 205,205, 235,235, 206,206, 236,236, 251,251, 221,221, 252,252, 222,222, 237,237, 238,238, 253,253, 254,254,
    };
    static const int16_t neighbours_dct_adst_16x16[MAX_NEIGHBORS * 256] = {
        0,0, 0,0, 16,16, 32,32, 0,0, 48,48, 1,1, 64,64, 17,17, 80,80, 33,33, 1,1, 49,49, 96,96, 2,2, 65,65,
        18,18, 112,112, 34,34, 81,81, 2,2, 50,50, 128,128, 3,3, 97,97, 19,19, 66,66, 144,144, 82,82, 35,35, 113,113, 3,3,
        51,51, 160,160, 4,4, 98,98, 129,129, 67,67, 20,20, 83,83, 114,114, 36,36, 176,176, 4,4, 145,145, 52,52, 99,99, 5,5,
        130,130, 68,68, 192,192, 161,161, 21,21, 115,115, 84,84, 37,37, 146,146, 208,208, 53,53, 5,5, 100,100, 177,177, 131,131, 69,69,
        6,6, 224,224, 116,116, 22,22, 162,162, 85,85, 147,147, 38,38, 193,193, 101,101, 54,54, 6,6, 132,132, 178,178, 70,70, 163,163,
        209,209, 7,7, 117,117, 23,23, 148,148, 7,7, 86,86, 194,194, 225,225, 39,39, 179,179, 102,102, 133,133, 55,55, 164,164, 8,8,
        71,71, 210,210, 118,118, 149,149, 195,195, 24,24, 87,87, 40,40, 56,56, 134,134, 180,180, 226,226, 103,103, 8,8, 165,165, 211,211,
        72,72, 150,150, 9,9, 119,119, 25,25, 88,88, 196,196, 41,41, 135,135, 181,181, 104,104, 57,57, 227,227, 166,166, 120,120, 151,151,
        197,197, 73,73, 9,9, 212,212, 89,89, 136,136, 182,182, 10,10, 26,26, 105,105, 167,167, 228,228, 152,152, 42,42, 121,121, 213,213,
        58,58, 198,198, 74,74, 137,137, 183,183, 168,168, 10,10, 90,90, 229,229, 11,11, 106,106, 214,214, 153,153, 27,27, 199,199, 43,43,
        184,184, 122,122, 169,169, 230,230, 59,59, 11,11, 75,75, 138,138, 200,200, 215,215, 91,91, 12,12, 28,28, 185,185, 107,107, 154,154,
        44,44, 231,231, 216,216, 60,60, 123,123, 12,12, 76,76, 201,201, 170,170, 232,232, 139,139, 92,92, 13,13, 108,108, 29,29, 186,186,
        217,217, 155,155, 45,45, 13,13, 61,61, 124,124, 14,14, 233,233, 77,77, 14,14, 171,171, 140,140, 202,202, 30,30, 93,93, 109,109,
        46,46, 156,156, 62,62, 187,187, 15,15, 125,125, 218,218, 78,78, 31,31, 172,172, 47,47, 141,141, 94,94, 234,234, 203,203, 63,63,
        110,110, 188,188, 157,157, 126,126, 79,79, 173,173, 95,95, 219,219, 142,142, 204,204, 235,235, 111,111, 158,158, 127,127, 189,189, 220,220,
        143,143, 174,174, 205,205, 236,236, 159,159, 190,190, 221,221, 175,175, 237,237, 206,206, 222,222, 191,191, 238,238, 207,207, 223,223, 239,239,
    };
    static const int16_t neighbours_dct_dct_32x32[MAX_NEIGHBORS * 1024] = {
        0,0, 0,0, 0,0, 32,32, 1,32, 1,1, 64,64, 33,64, 2,33, 96,96, 2,2, 65,96, 34,65, 128,128, 97,128, 3,34, 66,97, 3,3, 35,66, 98,129, 129,160, 160,160, 4,35, 67,98, 192,192, 4,4, 130,161, 161,192, 36,67, 99,130, 5,36, 68,99,
        193,224, 162,193, 224,224, 131,162, 37,68, 100,131, 5,5, 194,225, 225,256, 256,256, 163,194, 69,100, 132,163, 6,37, 226,257, 6,6, 195,226, 257,288, 101,132, 288,288, 38,69, 164,195, 133,164, 258,289, 227,258, 196,227, 7,38, 289,320, 70,101, 320,320, 7,7, 165,196,
        39,70, 102,133, 290,321, 259,290, 228,259, 321,352, 352,352, 197,228, 134,165, 71,102, 8,39, 322,353, 291,322, 260,291, 103,134, 353,384, 166,197, 229,260, 40,71, 8,8, 384,384, 135,166, 354,385, 323,354, 198,229, 292,323, 72,103, 261,292, 9,40, 385,416, 167,198, 104,135,
        230,261, 355,386, 416,416, 293,324, 324,355, 9,9, 41,72, 386,417, 199,230, 136,167, 417,448, 262,293, 356,387, 73,104, 387,418, 231,262, 10,41, 168,199, 325,356, 418,449, 105,136, 448,448, 42,73, 294,325, 200,231, 10,10, 357,388, 137,168, 263,294, 388,419, 74,105, 419,450,
        449,480, 326,357, 232,263, 295,326, 169,200, 11,42, 106,137, 480,480, 450,481, 358,389, 264,295, 201,232, 138,169, 389,420, 43,74, 420,451, 327,358, 11,11, 481,512, 233,264, 451,482, 296,327, 75,106, 170,201, 482,513, 512,512, 390,421, 359,390, 421,452, 107,138, 12,43, 202,233,
        452,483, 265,296, 328,359, 139,170, 44,75, 483,514, 513,544, 234,265, 297,328, 422,453, 12,12, 391,422, 171,202, 76,107, 514,545, 453,484, 544,544, 266,297, 203,234, 108,139, 329,360, 298,329, 140,171, 515,546, 13,44, 423,454, 235,266, 545,576, 454,485, 45,76, 172,203, 330,361,
        576,576, 13,13, 267,298, 546,577, 77,108, 204,235, 455,486, 577,608, 299,330, 109,140, 547,578, 14,45, 14,14, 141,172, 578,609, 331,362, 46,77, 173,204, 15,15, 78,109, 205,236, 579,610, 110,141, 15,46, 142,173, 47,78, 174,205, 16,16, 79,110, 206,237, 16,47, 111,142,
        48,79, 143,174, 80,111, 175,206, 17,48, 17,17, 207,238, 49,80, 81,112, 18,18, 18,49, 50,81, 82,113, 19,50, 51,82, 83,114, 608,608, 484,515, 360,391, 236,267, 112,143, 19,19, 640,640, 609,640, 516,547, 485,516, 392,423, 361,392, 268,299, 237,268, 144,175, 113,144,
        20,51, 20,20, 672,672, 641,672, 610,641, 548,579, 517,548, 486,517, 424,455, 393,424, 362,393, 300,331, 269,300, 238,269, 176,207, 145,176, 114,145, 52,83, 21,52, 21,21, 704,704, 673,704, 642,673, 611,642, 580,611, 549,580, 518,549, 487,518, 456,487, 425,456, 394,425, 363,394,
        332,363, 301,332, 270,301, 239,270, 208,239, 177,208, 146,177, 115,146, 84,115, 53,84, 22,53, 22,22, 705,736, 674,705, 643,674, 581,612, 550,581, 519,550, 457,488, 426,457, 395,426, 333,364, 302,333, 271,302, 209,240, 178,209, 147,178, 85,116, 54,85, 23,54, 706,737, 675,706,
        582,613, 551,582, 458,489, 427,458, 334,365, 303,334, 210,241, 179,210, 86,117, 55,86, 707,738, 583,614, 459,490, 335,366, 211,242, 87,118, 736,736, 612,643, 488,519, 364,395, 240,271, 116,147, 23,23, 768,768, 737,768, 644,675, 613,644, 520,551, 489,520, 396,427, 365,396, 272,303,
        241,272, 148,179, 117,148, 24,55, 24,24, 800,800, 769,800, 738,769, 676,707, 645,676, 614,645, 552,583, 521,552, 490,521, 428,459, 397,428, 366,397, 304,335, 273,304, 242,273, 180,211, 149,180, 118,149, 56,87, 25,56, 25,25, 832,832, 801,832, 770,801, 739,770, 708,739, 677,708,
        646,677, 615,646, 584,615, 553,584, 522,553, 491,522, 460,491, 429,460, 398,429, 367,398, 336,367, 305,336, 274,305, 243,274, 212,243, 181,212, 150,181, 119,150, 88,119, 57,88, 26,57, 26,26, 833,864, 802,833, 771,802, 709,740, 678,709, 647,678, 585,616, 554,585, 523,554, 461,492,
        430,461, 399,430, 337,368, 306,337, 275,306, 213,244, 182,213, 151,182, 89,120, 58,89, 27,58, 834,865, 803,834, 710,741, 679,710, 586,617, 555,586, 462,493, 431,462, 338,369, 307,338, 214,245, 183,214, 90,121, 59,90, 835,866, 711,742, 587,618, 463,494, 339,370, 215,246, 91,122,
        864,864, 740,771, 616,647, 492,523, 368,399, 244,275, 120,151, 27,27, 896,896, 865,896, 772,803, 741,772, 648,679, 617,648, 524,555, 493,524, 400,431, 369,400, 276,307, 245,276, 152,183, 121,152, 28,59, 28,28, 928,928, 897,928, 866,897, 804,835, 773,804, 742,773, 680,711, 649,680,
        618,649, 556,587, 525,556, 494,525, 432,463, 401,432, 370,401, 308,339, 277,308, 246,277, 184,215, 153,184, 122,153, 60,91, 29,60, 29,29, 960,960, 929,960, 898,929, 867,898, 836,867, 805,836, 774,805, 743,774, 712,743, 681,712, 650,681, 619,650, 588,619, 557,588, 526,557, 495,526,
        464,495, 433,464, 402,433, 371,402, 340,371, 309,340, 278,309, 247,278, 216,247, 185,216, 154,185, 123,154, 92,123, 61,92, 30,61, 30,30, 961,992, 930,961, 899,930, 837,868, 806,837, 775,806, 713,744, 682,713, 651,682, 589,620, 558,589, 527,558, 465,496, 434,465, 403,434, 341,372,
        310,341, 279,310, 217,248, 186,217, 155,186, 93,124, 62,93, 31,62, 962,993, 931,962, 838,869, 807,838, 714,745, 683,714, 590,621, 559,590, 466,497, 435,466, 342,373, 311,342, 218,249, 187,218, 94,125, 63,94, 963,994, 839,870, 715,746, 591,622, 467,498, 343,374, 219,250, 95,126,
        868,899, 744,775, 620,651, 496,527, 372,403, 248,279, 124,155, 900,931, 869,900, 776,807, 745,776, 652,683, 621,652, 528,559, 497,528, 404,435, 373,404, 280,311, 249,280, 156,187, 125,156, 932,963, 901,932, 870,901, 808,839, 777,808, 746,777, 684,715, 653,684, 622,653, 560,591, 529,560,
        498,529, 436,467, 405,436, 374,405, 312,343, 281,312, 250,281, 188,219, 157,188, 126,157, 964,995, 933,964, 902,933, 871,902, 840,871, 809,840, 778,809, 747,778, 716,747, 685,716, 654,685, 623,654, 592,623, 561,592, 530,561, 499,530, 468,499, 437,468, 406,437, 375,406, 344,375, 313,344,
        282,313, 251,282, 220,251, 189,220, 158,189, 127,158, 965,996, 934,965, 903,934, 841,872, 810,841, 779,810, 717,748, 686,717, 655,686, 593,624, 562,593, 531,562, 469,500, 438,469, 407,438, 345,376, 314,345, 283,314, 221,252, 190,221, 159,190, 966,997, 935,966, 842,873, 811,842, 718,749,
        687,718, 594,625, 563,594, 470,501, 439,470, 346,377, 315,346, 222,253, 191,222, 967,998, 843,874, 719,750, 595,626, 471,502, 347,378, 223,254, 872,903, 748,779, 624,655, 500,531, 376,407, 252,283, 904,935, 873,904, 780,811, 749,780, 656,687, 625,656, 532,563, 501,532, 408,439, 377,408,
        284,315, 253,284, 936,967, 905,936, 874,905, 812,843, 781,812, 750,781, 688,719, 657,688, 626,657, 564,595, 533,564, 502,533, 440,471, 409,440, 378,409, 316,347, 285,316, 254,285, 968,999, 937,968, 906,937, 875,906, 844,875, 813,844, 782,813, 751,782, 720,751, 689,720, 658,689, 627,658,
        596,627, 565,596, 534,565, 503,534, 472,503, 441,472, 410,441, 379,410, 348,379, 317,348, 286,317, 255,286, 969,1000, 938,969, 907,938, 845,876, 814,845, 783,814, 721,752, 690,721, 659,690, 597,628, 566,597, 535,566, 473,504, 442,473, 411,442, 349,380, 318,349, 287,318, 970,1001, 939,970,
        846,877, 815,846, 722,753, 691,722, 598,629, 567,598, 474,505, 443,474, 350,381, 319,350, 971,1002, 847,878, 723,754, 599,630, 475,506, 351,382, 876,907, 752,783, 628,659, 504,535, 380,411, 908,939, 877,908, 784,815, 753,784, 660,691, 629,660, 536,567, 505,536, 412,443, 381,412, 940,971,
        909,940, 878,909, 816,847, 785,816, 754,785, 692,723, 661,692, 630,661, 568,599, 537,568, 506,537, 444,475, 413,444, 382,413, 972,1003, 941,972, 910,941, 879,910, 848,879, 817,848, 786,817, 755,786, 724,755, 693,724, 662,693, 631,662, 600,631, 569,600, 538,569, 507,538, 476,507, 445,476,
        414,445, 383,414, 973,1004, 942,973, 911,942, 849,880, 818,849, 787,818, 725,756, 694,725, 663,694, 601,632, 570,601, 539,570, 477,508, 446,477, 415,446, 974,1005, 943,974, 850,881, 819,850, 726,757, 695,726, 602,633, 571,602, 478,509, 447,478, 975,1006, 851,882, 727,758, 603,634, 479,510,
        880,911, 756,787, 632,663, 508,539, 912,943, 881,912, 788,819, 757,788, 664,695, 633,664, 540,571, 509,540, 944,975, 913,944, 882,913, 820,851, 789,820, 758,789, 696,727, 665,696, 634,665, 572,603, 541,572, 510,541, 976,1007, 945,976, 914,945, 883,914, 852,883, 821,852, 790,821, 759,790,
        728,759, 697,728, 666,697, 635,666, 604,635, 573,604, 542,573, 511,542, 977,1008, 946,977, 915,946, 853,884, 822,853, 791,822, 729,760, 698,729, 667,698, 605,636, 574,605, 543,574, 978,1009, 947,978, 854,885, 823,854, 730,761, 699,730, 606,637, 575,606, 979,1010, 855,886, 731,762, 607,638,
        884,915, 760,791, 636,667, 916,947, 885,916, 792,823, 761,792, 668,699, 637,668, 948,979, 917,948, 886,917, 824,855, 793,824, 762,793, 700,731, 669,700, 638,669, 980,1011, 949,980, 918,949, 887,918, 856,887, 825,856, 794,825, 763,794, 732,763, 701,732, 670,701, 639,670, 981,1012, 950,981,
        919,950, 857,888, 826,857, 795,826, 733,764, 702,733, 671,702, 982,1013, 951,982, 858,889, 827,858, 734,765, 703,734, 983,1014, 859,890, 735,766, 888,919, 764,795, 920,951, 889,920, 796,827, 765,796, 952,983, 921,952, 890,921, 828,859, 797,828, 766,797, 984,1015, 953,984, 922,953, 891,922,
        860,891, 829,860, 798,829, 767,798, 985,1016, 954,985, 923,954, 861,892, 830,861, 799,830, 986,1017, 955,986, 862,893, 831,862, 987,1018, 863,894, 892,923, 924,955, 893,924, 956,987, 925,956, 894,925, 988,1019, 957,988, 926,957, 895,926, 989,1020, 958,989, 927,958, 990,1021, 959,990, 991,1022,
    };


    int32_t GetCtxTokenAc(int32_t c, const int16_t *neigh, const uint8_t *tokenCache) {
        return (1 + tokenCache[neigh[MAX_NEIGHBORS * c + 0]] +
            tokenCache[neigh[MAX_NEIGHBORS * c + 1]]) >>
            1;
    }


    const int16_t *neighboursVp9[4][4] = {
        {neighbours_dct_dct_4x4,   neighbours_adst_dct_4x4,   neighbours_dct_adst_4x4,   neighbours_dct_dct_4x4},
        {neighbours_dct_dct_8x8,   neighbours_adst_dct_8x8,   neighbours_dct_adst_8x8,   neighbours_dct_dct_8x8},
        {neighbours_dct_dct_16x16, neighbours_adst_dct_16x16, neighbours_dct_adst_16x16, neighbours_dct_dct_16x16},
        {neighbours_dct_dct_32x32, neighbours_dct_dct_32x32,  neighbours_dct_dct_32x32,  neighbours_dct_dct_32x32}
    };

    void ProcessResidualZeroBlockVP9(int32_t txSize, int32_t plane, int32_t isInter, int32_t dcCtx, FrameCounts *counts, TokenVP9 **tokens, const FrameContexts &contexts) {
        counts->more_coef[txSize][plane>0][isInter][0][dcCtx][0]++;  // no more_coef
        (*tokens)->ctx = contexts.coef[txSize][plane>0][isInter][0][dcCtx] - (vpx_prob *)&contexts;
        (*tokens)->token = EOB_TOKEN;
        ++(*tokens);
    }

    void ProcessResidualBlockVP9(int32_t txSize, int32_t plane, int32_t isInter, const int16_t *scan, const int16_t *neigh, const int16_t *coefs,
                                 int32_t lastNz, int32_t dcCtx, FrameCounts *counts, TokenVP9 **tokens, const FrameContexts &contexts)
    {
        const uint8_t *band = (txSize == TX_4X4 ? coefband_4x4 : coefband_8x8plus);
        uint8_t tokenCache[1024];
        tokenCache[0] = dcCtx;

        int32_t c = 0;
        do {
            int32_t pos = scan[c];
            int32_t ctx = GetCtxTokenAc(c, neigh, tokenCache);

            counts->more_coef[txSize][plane>0][isInter][band[c]][ctx][1]++;

            int32_t coef = coefs[pos];
            int32_t absCoef = abs(coef);
            int32_t tok = GetToken(absCoef);
            tokenCache[pos] = energy_class[tok];
            counts->token[txSize][plane>0][isInter][band[c]][ctx][tok]++;

            (*tokens)->ctx = contexts.coef[txSize][plane>0][isInter][band[c]][ctx] - (vpx_prob *)&contexts;
            (*tokens)->token = tok;
            (*tokens)->sign = uint32_t(coef) >> 31;
            (*tokens)->extra = absCoef - extra_bits[tok].coef;
            ++(*tokens);

            if (tok == ZERO_TOKEN) {
                int32_t pos = scan[++c];
                while (coefs[pos] == 0) {
                    int32_t ctx = GetCtxTokenAc(c, neigh, tokenCache);
                    tokenCache[pos] = energy_class[ZERO_TOKEN];
                    counts->token[txSize][plane>0][isInter][band[c]][ctx][ZERO_TOKEN]++;
                    (*tokens)->ctx = contexts.coef[txSize][plane>0][isInter][band[c]][ctx] - (vpx_prob *)&contexts;
                    (*tokens)->token = ZERO_TOKEN;
                    ++(*tokens);
                    pos = scan[++c];
                }

                int32_t ctx = GetCtxTokenAc(c, neigh, tokenCache);
                int32_t coef = coefs[pos];
                int32_t absCoef = abs(coef);
                int32_t tok = GetToken(absCoef);
                tokenCache[pos] = energy_class[tok];
                counts->token[txSize][plane>0][isInter][band[c]][ctx][tok]++;
                (*tokens)->ctx = contexts.coef[txSize][plane>0][isInter][band[c]][ctx] - (vpx_prob *)&contexts;
                (*tokens)->token = tok;
                (*tokens)->sign = uint32_t(coef) >> 31;
                (*tokens)->extra = absCoef - extra_bits[tok].coef;
                ++(*tokens);
            }

            ++c;
        } while (c <= lastNz);

        int32_t segEob = 16 << (txSize << 1);
        if (lastNz + 1 < segEob) {
            c = lastNz + 1;
            int32_t pos = scan[c];
            int32_t ctx = GetCtxTokenAc(c, neigh, tokenCache);
            counts->more_coef[txSize][plane>0][isInter][band[c]][ctx][0]++;  // no more_coef

            (*tokens)->ctx = contexts.coef[txSize][plane>0][isInter][band[c]][ctx] - (vpx_prob *)&contexts;
            (*tokens)->token = EOB_TOKEN;
            ++(*tokens);
        }
    }

    void ProcessResidual(SuperBlock &sb, int32_t miRow, int32_t miCol, FrameCounts *counts) {
        const ModeInfo *mi = sb.frame->m_modeInfo + miCol + miRow * sb.par->miPitch;
        BlockSize miSize = mi->sbType;
        BlockSize bsize = std::max((uint8_t)BLOCK_8X8, miSize);
        TxSize txSize = mi->txSize;
        int32_t isInter = mi->refIdx[0] != INTRA_FRAME;
        int32_t baseX = miCol << 1;
        int32_t baseY = miRow << 1;
        int32_t x4 = baseX & 15;
        int32_t y4 = baseY & 15;
        PredMode mode = mi->mode;
        TxSize txSz = txSize;
        int32_t maskPosX = ((1 << (txSz + 2)) - 1);
        BlockSize planeSz = bsize;
        int32_t num4x4w = block_size_wide_4x4[planeSz];
        int32_t num4x4h = block_size_high_4x4[planeSz];
        num4x4w = std::min(num4x4w, (sb.par->miCols << (1 - 0)) - baseX);
        num4x4h = std::min(num4x4h, (sb.par->miRows << (1 - 0)) - baseY);
        int32_t step = 1 << txSz;
        int32_t txType_ = mi->txType;
        const uint8_t *band = (txSz == TX_4X4 ? coefband_4x4 : coefband_8x8plus);


        if (mi->skip) {
            small_memset0(sb.ctx.aboveNonzero[0] + x4, num4x4w);
            small_memset0(sb.ctx.leftNonzero[0]  + y4, num4x4h);
        } else {
            for (int32_t y = 0; y < num4x4h; y += step) {
                for (int32_t x = 0; x < num4x4w; x += step) {
                    int32_t blockIdx = h265_scan_r2z4[(y4 + y) * 16 + (x4 + x)];
                    const CoeffsType *coeffs = sb.coeffs[0] + (blockIdx << (LOG2_MIN_TU_SIZE << 1));
                    const int32_t blkRow = miRow * 2 + y;
                    const int32_t blkCol = miCol * 2 + x;
                    const int32_t index = (blkRow & 15) * 16 + (blkCol & 15);
                    const int32_t txType = sb.frame->m_txkTypes4x4[sb.sbIndex * 256 + index];

                    const ScanOrder *so = &av1_scans[txSz][txType];
                    const int16_t *scan = so->scan;
                    const int16_t *neigh = so->neighbors;

                    int32_t lastNz = (16 << (txSz << 1)) - 1;
                    for (; lastNz >= 0; lastNz--)
                        if (coeffs[scan[lastNz]])
                            break;
                    const int32_t dcCtx = GetDcCtx(sb.ctx.aboveNonzero[0] + x4 + x, sb.ctx.leftNonzero[0] + y4 + y, step);
                    if (lastNz >= 0) {
                        ProcessResidualBlockVP9(txSz, 0, isInter, scan, neigh, coeffs, lastNz, dcCtx, counts, &sb.tokensVP9, sb.frame->m_contexts);
                        small_memset1(sb.ctx.aboveNonzero[0] + x4 + x, step);
                        small_memset1(sb.ctx.leftNonzero[0]  + y4 + y, step);
                    } else {
                        ProcessResidualZeroBlockVP9(txSz, 0, isInter, dcCtx, counts, &sb.tokensVP9, sb.frame->m_contexts);
                        small_memset0(sb.ctx.aboveNonzero[0] + x4 + x, step);
                        small_memset0(sb.ctx.leftNonzero[0]  + y4 + y, step);
                    }
                }
            }
        }

        mode = mi->modeUV;
        planeSz = GetPlaneBlockSize(bsize, 1, *sb.par);
        txSz = max_txsize_rect_lookup[planeSz];
        maskPosX = ((1 << (txSz + 2)) - 1);
        num4x4w = block_size_wide_4x4[planeSz];
        num4x4h = block_size_high_4x4[planeSz];
        int32_t subX = sb.par->subsamplingX;
        int32_t subY = sb.par->subsamplingY;
        baseX >>= subX;
        baseY >>= subY;
        x4 >>= subX;
        y4 >>= subY;
        num4x4w = std::min(num4x4w, (sb.par->miCols << (1 - subX)) - baseX);
        num4x4h = std::min(num4x4h, (sb.par->miRows << (1 - subY)) - baseY);
        step = 1 << txSz;
        band = (txSz == TX_4X4 ? coefband_4x4 : coefband_8x8plus);
        int txType;
        if (isInter) {
            txType = (mi->txType == IDTX && txSz >= TX_32X32) ? DCT_DCT : mi->txType;
        }
        else {
            txType = intra_mode_to_tx_type_context[mi->modeUV];
        }
        const ScanOrder *so = &av1_scans[txSz][txType];
        const int16_t *scan = so->scan;
        const int16_t *neigh = so->neighbors;

        if (mi->skip) {
            small_memset0(sb.ctx.aboveNonzero[1] + x4, num4x4w);
            small_memset0(sb.ctx.aboveNonzero[2] + x4, num4x4w);
            small_memset0(sb.ctx.leftNonzero[1] + y4,  num4x4h);
            small_memset0(sb.ctx.leftNonzero[2] + y4,  num4x4h);
        } else {
            for (int32_t y = 0; y < num4x4h; y += step) {
                for (int32_t x = 0; x < num4x4w; x += step) {
                    int32_t blockIdx = h265_scan_r2z4[((y4 + y) << subY) * 16 + ((x4 + x) << subX)];
                    const CoeffsType *coeffs = sb.coeffs[1] + (blockIdx << (LOG2_MIN_TU_SIZE << 1) - subX - subY);

                    int32_t lastNz = (16 << (txSz << 1)) - 1;
                    for (; lastNz >= 0; lastNz--)
                        if (coeffs[scan[lastNz]])
                            break;
                    int32_t dcCtx = GetDcCtx(sb.ctx.aboveNonzero[1] + x4 + x, sb.ctx.leftNonzero[1] + y4 + y, step);;
                    if (lastNz >= 0) {
                        ProcessResidualBlockVP9(txSz, 1, isInter, scan, neigh, coeffs, lastNz, dcCtx, counts, &sb.tokensVP9, sb.frame->m_contexts);
                        small_memset1(sb.ctx.aboveNonzero[1] + x4 + x, step);
                        small_memset1(sb.ctx.leftNonzero[1]  + y4 + y, step);
                    } else {
                        ProcessResidualZeroBlockVP9(txSz, 1, isInter, dcCtx, counts, &sb.tokensVP9, sb.frame->m_contexts);
                        small_memset0(sb.ctx.aboveNonzero[1] + x4 + x, step);
                        small_memset0(sb.ctx.leftNonzero[1]  + y4 + y, step);
                    }
                }
            }
            for (int32_t y = 0; y < num4x4h; y += step) {
                for (int32_t x = 0; x < num4x4w; x += step) {
                    int32_t blockIdx = h265_scan_r2z4[((y4 + y) << subY) * 16 + ((x4 + x) << subX)];
                    const CoeffsType *coeffs = sb.coeffs[2] + (blockIdx << (LOG2_MIN_TU_SIZE << 1) - subX - subY);

                    int32_t lastNz = (16 << (txSz << 1)) - 1;
                    for (; lastNz >= 0; lastNz--)
                        if (coeffs[scan[lastNz]])
                            break;
                    int32_t dcCtx = GetDcCtx(sb.ctx.aboveNonzero[2] + x4 + x, sb.ctx.leftNonzero[2] + y4 + y, step);
                    if (lastNz >= 0) {
                        ProcessResidualBlockVP9(txSz, 1, isInter, scan, neigh, coeffs, lastNz, dcCtx, counts, &sb.tokensVP9, sb.frame->m_contexts);
                        small_memset1(sb.ctx.aboveNonzero[2] + x4 + x, step);
                        small_memset1(sb.ctx.leftNonzero[2]  + y4 + y, step);
                    } else {
                        ProcessResidualZeroBlockVP9(txSz, 1, isInter, dcCtx, counts, &sb.tokensVP9, sb.frame->m_contexts);
                        small_memset0(sb.ctx.aboveNonzero[2] + x4 + x, step);
                        small_memset0(sb.ctx.leftNonzero[2]  + y4 + y, step);
                    }
                }
            }
        }

        sb.tokensAV1->token = EOSB_TOKEN;
        ++sb.tokensAV1;
    }

    void ProcessModes(SuperBlock &sb, int32_t miRow, int32_t miCol, FrameCounts *counts) {
        if (sb.frame->IsIntra())
            ProcessIntraFrameModeInfo(sb, miRow, miCol, counts);
        else
            ProcessInterFrameModeInfo(sb, miRow, miCol, counts);

        const ModeInfo *mi = sb.frame->m_modeInfo + miCol + miRow * sb.par->miPitch;
        if (mi->refIdx[0] == INTRA_FRAME)
            ProcessResidual(sb, miRow, miCol, counts);
    }

    vpx_prob GetBinProb(const uint32_t binDist[2], vpx_prob oldprob) {
        uint32_t denom = binDist[0] + binDist[1];
        if (denom == 0)
            return oldprob;
        return Saturate(1, 255, (256 * binDist[0] + (denom >> 1)) / denom);
    }

    uint32_t GetBinCost(const uint32_t binDist[2], vpx_prob prob) {
        return binDist[0] * prob2bits[prob] + binDist[1] * prob2bits[256 - prob];
    }

    int32_t RecenterNonneg(int32_t v, int32_t m) {
        if (v > (m << 1)) return v;
        else if (v >= m)  return ((v - m) << 1);
        else              return ((m - v) << 1) - 1;
    }

    int32_t RemapProb(int32_t v, int32_t m) {
        static const uint8_t map_table[MAX_PROB - 1] = {
            // generated by:
            //   map_table[j] = split_index(j, MAX_PROB - 1, MODULUS_PARAM);
            20,  21,  22,  23,  24,  25,   0,  26,  27,  28,  29,  30,  31,  32,  33,
            34,  35,  36,  37,   1,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
            48,  49,   2,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,
            3,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,   4,  74,
            75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,   5,  86,  87,  88,
            89,  90,  91,  92,  93,  94,  95,  96,  97,   6,  98,  99, 100, 101, 102,
            103, 104, 105, 106, 107, 108, 109,   7, 110, 111, 112, 113, 114, 115, 116,
            117, 118, 119, 120, 121,   8, 122, 123, 124, 125, 126, 127, 128, 129, 130,
            131, 132, 133,   9, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144,
            145,  10, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157,  11,
            158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169,  12, 170, 171,
            172, 173, 174, 175, 176, 177, 178, 179, 180, 181,  13, 182, 183, 184, 185,
            186, 187, 188, 189, 190, 191, 192, 193,  14, 194, 195, 196, 197, 198, 199,
            200, 201, 202, 203, 204, 205,  15, 206, 207, 208, 209, 210, 211, 212, 213,
            214, 215, 216, 217,  16, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227,
            228, 229,  17, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241,
            18, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253,  19,
        };
        v--;
        m--;
        int32_t i;
        if ((m << 1) <= MAX_PROB)
            i = RecenterNonneg(v, m) - 1;
        else
            i = RecenterNonneg(MAX_PROB - 1 - v, MAX_PROB - 1 - m) - 1;

        i = map_table[i];
        return i;
    }

    template <EnumCodecType CodecType>
    int write_bit_gte(BoolCoder &bc, int word, int test) {
        WriteLiteral<CodecType>(bc, word >= test, 1);
        return word >= test;
    }

    template <EnumCodecType CodecType>
    void EncodeUniform(BoolCoder &bc, int32_t v) {
        const int32_t l = 8;
        const int32_t m = (1 << l) -  190;
        if (v < m) {
            WriteLiteral<CodecType>(bc, v, l - 1);
        } else {
            WriteLiteral<CodecType>(bc, m + ((v - m) >> 1), l - 1);
            WriteLiteral<CodecType>(bc, (v - m) & 1, 1);
        }
    }



    template <EnumCodecType CodecType>
    void EncodeTermSubexp(BoolCoder &bc, int32_t word) {
        if (!write_bit_gte<CodecType>(bc, word, 16)) {
             WriteLiteral<CodecType>(bc, word, 4);
        } else if (!write_bit_gte<CodecType>(bc, word, 32)) {
            WriteLiteral<CodecType>(bc, word - 16, 4);
        } else if (!write_bit_gte<CodecType>(bc, word, 64)) {
            WriteLiteral<CodecType>(bc, word - 32, 5);
        } else {
            EncodeUniform<CodecType>(bc, word - 64);
        }
    }

    const uint8_t update_bits[255] = {
       5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
       6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
       8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
       8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,  0,
    };
    const uint32_t MIN_DELP_BITS = 5;
    const uint32_t MV_PROB_UPDATE_BITS = 7;

    uint32_t GetProbUpdateCost(vpx_prob newprob, vpx_prob oldprob) {
        int32_t delp = RemapProb(newprob, oldprob);
        return update_bits[delp] << 9;
    }

};

template <EnumCodecType CodecType>
void AV1Enc::WriteProbDiffUpdate(BoolCoder &bc, vpx_prob newp, vpx_prob oldp) {
    const int32_t delp = RemapProb(newp, oldp);
    EncodeTermSubexp<CodecType>(bc, delp);
}
template void AV1Enc::WriteProbDiffUpdate<CODEC_AV1>(BoolCoder &bc, vpx_prob newp, vpx_prob oldp);


void AV1Enc::GetTreeDistrTx8x8(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[TX_4X4];
    distr[0][1] = counts[TX_8X8];
}

void AV1Enc::GetTreeDistrTx16x16(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[TX_4X4];
    distr[0][1] = counts[TX_8X8] + counts[TX_16X16];
    distr[1][0] = counts[TX_8X8];
    distr[1][1] = counts[TX_16X16];
}

void AV1Enc::GetTreeDistrTx32x32(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[TX_4X4];
    distr[0][1] = counts[TX_8X8] + counts[TX_16X16] + counts[TX_32X32];
    distr[1][0] = counts[TX_8X8];
    distr[1][1] = counts[TX_16X16] + counts[TX_32X32];
    distr[2][0] = counts[TX_16X16];
    distr[2][1] = counts[TX_32X32];
}

void AV1Enc::GetTreeDistrToken(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[ZERO_TOKEN];
    distr[0][1] = std::accumulate(counts+ONE_TOKEN, counts+NUM_TOKENS, 0);
    distr[1][0] = counts[ONE_TOKEN];
    distr[1][1] = std::accumulate(counts+TWO_TOKEN, counts+NUM_TOKENS, 0);
    distr[2][0] = counts[TWO_TOKEN] + counts[THREE_TOKEN] + counts[FOUR_TOKEN];
    distr[2][1] = std::accumulate(counts+DCT_VAL_CATEGORY1, counts+NUM_TOKENS, 0);
    distr[3][0] = counts[TWO_TOKEN];
    distr[3][1] = counts[THREE_TOKEN] + counts[FOUR_TOKEN];
    distr[4][0] = counts[THREE_TOKEN];
    distr[4][1] = counts[FOUR_TOKEN];
    distr[5][0] = counts[DCT_VAL_CATEGORY1] + counts[DCT_VAL_CATEGORY2];
    distr[5][1] = std::accumulate(counts+DCT_VAL_CATEGORY3, counts+NUM_TOKENS, 0);
    distr[6][0] = counts[DCT_VAL_CATEGORY1];
    distr[6][1] = counts[DCT_VAL_CATEGORY2];
    distr[7][0] = counts[DCT_VAL_CATEGORY3] + counts[DCT_VAL_CATEGORY4];
    distr[7][1] = counts[DCT_VAL_CATEGORY5] + counts[DCT_VAL_CATEGORY6];
    distr[8][0] = counts[DCT_VAL_CATEGORY3];
    distr[8][1] = counts[DCT_VAL_CATEGORY4];
    distr[9][0] = counts[DCT_VAL_CATEGORY5];
    distr[9][1] = counts[DCT_VAL_CATEGORY6];
}

void AV1Enc::GetTreeDistrInterpFilter(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[EIGHTTAP];
    distr[0][1] = counts[EIGHTTAP_SMOOTH] + counts[EIGHTTAP_SHARP];
    distr[1][0] = counts[EIGHTTAP_SMOOTH];
    distr[1][1] = counts[EIGHTTAP_SHARP];
}

void AV1Enc::GetTreeDistrInterMode(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[ZEROMV-NEARESTMV];
    distr[0][1] = counts[NEARESTMV-NEARESTMV] + counts[NEARMV-NEARESTMV] + counts[NEWMV-NEARESTMV];
    distr[1][0] = counts[NEARESTMV-NEARESTMV];
    distr[1][1] = counts[NEARMV-NEARESTMV] + counts[NEWMV-NEARESTMV];
    distr[2][0] = counts[NEARMV-NEARESTMV];
    distr[2][1] = counts[NEWMV-NEARESTMV];
}

void AV1Enc::GetTreeDistrYMode(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[DC_PRED];
    distr[0][1] = std::accumulate(counts+V_PRED, counts+INTRA_MODES, 0);
    distr[1][0] = counts[TM_PRED];
    distr[1][1] = std::accumulate(counts+V_PRED, counts+TM_PRED, 0);
    distr[2][0] = counts[V_PRED];
    distr[2][1] = std::accumulate(counts+H_PRED, counts+TM_PRED, 0);
    distr[3][0] = counts[H_PRED] + counts[D135_PRED] + counts[D117_PRED];
    distr[3][1] = counts[D45_PRED] + counts[D63_PRED] + counts[D153_PRED] + counts[D207_PRED];
    distr[4][0] = counts[H_PRED];
    distr[4][1] = counts[D135_PRED] + counts[D117_PRED];
    distr[5][0] = counts[D135_PRED];
    distr[5][1] = counts[D117_PRED];
    distr[6][0] = counts[D45_PRED];
    distr[6][1] = counts[D63_PRED] + counts[D153_PRED] + counts[D207_PRED];
    distr[7][0] = counts[D63_PRED];
    distr[7][1] = counts[D153_PRED] + counts[D207_PRED];
    distr[8][0] = counts[D153_PRED];
    distr[8][1] = counts[D207_PRED];
}

void AV1Enc::GetTreeDistrPartition(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[PARTITION_NONE];
    distr[0][1] = counts[PARTITION_HORZ] + counts[PARTITION_VERT] + counts[PARTITION_SPLIT];
    distr[1][0] = counts[PARTITION_HORZ];
    distr[1][1] = counts[PARTITION_VERT] + counts[PARTITION_SPLIT];
    distr[2][0] = counts[PARTITION_VERT];
    distr[2][1] = counts[PARTITION_SPLIT];
}

void AV1Enc::GetTreeDistrMvJoint(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[MV_JOINT_ZERO];
    distr[0][1] = counts[MV_JOINT_HNZVZ] + counts[MV_JOINT_HZVNZ] + counts[MV_JOINT_HNZVNZ];
    distr[1][0] = counts[MV_JOINT_HNZVZ];
    distr[1][1] = counts[MV_JOINT_HZVNZ] + counts[MV_JOINT_HNZVNZ];
    distr[2][0] = counts[MV_JOINT_HZVNZ];
    distr[2][1] = counts[MV_JOINT_HNZVNZ];
}

void AV1Enc::GetTreeDistrMvClass(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[MV_CLASS_0];
    distr[0][1] = std::accumulate(counts+MV_CLASS_1, counts+AV1_MV_CLASSES, 0);
    distr[1][0] = counts[MV_CLASS_1];
    distr[1][1] = std::accumulate(counts+MV_CLASS_2, counts+AV1_MV_CLASSES, 0);
    distr[2][0] = counts[MV_CLASS_2] + counts[MV_CLASS_3];
    distr[2][1] = std::accumulate(counts+MV_CLASS_4, counts+AV1_MV_CLASSES, 0);
    distr[3][0] = counts[MV_CLASS_2];
    distr[3][1] = counts[MV_CLASS_3];
    distr[4][0] = counts[MV_CLASS_4] + counts[MV_CLASS_5];
    distr[4][1] = std::accumulate(counts+MV_CLASS_6, counts+AV1_MV_CLASSES, 0);
    distr[5][0] = counts[MV_CLASS_4];
    distr[5][1] = counts[MV_CLASS_5];
    distr[6][0] = counts[MV_CLASS_6];
    distr[6][1] = std::accumulate(counts+MV_CLASS_7, counts+AV1_MV_CLASSES, 0);
    distr[7][0] = counts[MV_CLASS_7] + counts[MV_CLASS_8];
    distr[7][1] = counts[MV_CLASS_9] + counts[MV_CLASS_10];
    distr[8][0] = counts[MV_CLASS_7];
    distr[8][1] = counts[MV_CLASS_8];
    distr[9][0] = counts[MV_CLASS_9];
    distr[9][1] = counts[MV_CLASS_10];
}

void AV1Enc::GetTreeDistrMvFr(const uint32_t *counts, uint32_t (*distr)[2]) {
    distr[0][0] = counts[0];
    distr[0][1] = counts[1] + counts[2] + counts[3];
    distr[1][0] = counts[1];
    distr[1][1] = counts[2] + counts[3];
    distr[2][0] = counts[2];
    distr[2][1] = counts[3];
}

template <EnumCodecType CodecType>
void AV1Enc::WriteDiffProb(BoolCoder &bc, const uint32_t (*distr)[2], vpx_prob *probs, int32_t numNodes, const char *tracestr) {
    fprintf_trace_prob_update(stderr, "%s: bits saved: [ ", tracestr);

    for (int32_t i = 0; i < numNodes; i++) {
        const vpx_prob oldProb = probs[i];
        const uint32_t oldCost = GetBinCost(distr[i], oldProb);
        const vpx_prob realProb = GetBinProb(distr[i], oldProb);
        const int32_t step = realProb > oldProb ? -1 : 1;
        const uint32_t updateFlagCost = prob2bits[256 - DIFF_UPDATE_PROB] - prob2bits[DIFF_UPDATE_PROB];
        vpx_prob bestProb = oldProb;
        uint32_t bestCost = oldCost;

        if (bestCost > updateFlagCost + (MIN_DELP_BITS << 9)) {
            for (int32_t prob = realProb; prob != oldProb; prob += step) {
                uint32_t newCost = GetBinCost(distr[i], prob);
                uint32_t updateCost = GetProbUpdateCost(prob, oldProb) + updateFlagCost;
                if (bestCost > newCost + updateCost) {
                    bestCost = newCost + updateCost;
                    bestProb = prob;
                }
            }
        }

        fprintf_trace_prob_update(stderr, "%u ", (oldCost - bestCost + 256)>>9);
        if (bestProb != oldProb) {
            WriteBool<CodecType>(bc, 1, DIFF_UPDATE_PROB);
            WriteProbDiffUpdate<CodecType>(bc, bestProb, oldProb);
            probs[i] = bestProb;
        } else {
            WriteBool<CodecType>(bc, 0, DIFF_UPDATE_PROB);
        }
    }

    fprintf_trace_prob_update(stderr, "]\n");
}

template <EnumCodecType CodecType>
void AV1Enc::WriteDiffProbPivot(BoolCoder &bc, const uint32_t (*distr)[2], vpx_prob *prob, const char *tracestr) {
    fprintf_trace_prob_update(stderr, "%s: bits saved: [ ", tracestr);

    const vpx_prob oldProb = *prob;
    uint32_t oldCost = GetBinCost(distr[1], oldProb);
    for (int32_t i = 2; i < NUM_TOKENS-1; i++)
        oldCost += GetBinCost(distr[i], pareto(i, oldProb));
    const vpx_prob realProb = GetBinProb(distr[1], oldProb);
    const int32_t step = realProb > oldProb ? -1 : 1;
    const uint32_t updateFlagCost = prob2bits[256 - DIFF_UPDATE_PROB] - prob2bits[DIFF_UPDATE_PROB];
    vpx_prob bestProb = oldProb;
    uint32_t bestCost = oldCost;

    if (bestCost > updateFlagCost + (MIN_DELP_BITS << 9)) {
        for (int32_t prob = realProb; prob != oldProb; prob += step) {
            uint32_t newCost = GetBinCost(distr[1], prob);
            for (int32_t i = 2; i < NUM_TOKENS-1; i++)
                newCost += GetBinCost(distr[i], pareto(i, prob));
            uint32_t updateCost = GetProbUpdateCost(prob, oldProb) + updateFlagCost;
            if (bestCost > newCost + updateCost) {
                bestCost = newCost + updateCost;
                bestProb = prob;
            }
        }
    }

    fprintf_trace_prob_update(stderr, "%u ", (oldCost - bestCost + 256)>>9);
    if (bestProb != oldProb) {
        WriteBool(bc, 1, DIFF_UPDATE_PROB);
        WriteProbDiffUpdate(bc, bestProb, oldProb);
        *prob = bestProb;
    } else {
        WriteBool(bc, 0, DIFF_UPDATE_PROB);
    }

    fprintf_trace_prob_update(stderr, "]\n");
}

template <EnumCodecType CodecType>
void AV1Enc::WriteDiffProbMv(BoolCoder &bc, const uint32_t (*distr)[2], vpx_prob *probs, int32_t numNodes, const char *tracestr) {
    fprintf_trace_prob_update(stderr, "%s: bits saved: [ ", tracestr);

    for (int32_t i = 0; i < numNodes; i++) {
        const vpx_prob oldProb = probs[i];
        const vpx_prob newProb = GetBinProb(distr[i], oldProb) | 1;
        const uint32_t oldCost = GetBinCost(distr[i], oldProb) + prob2bits[DIFF_UPDATE_PROB];
        const uint32_t newCost = GetBinCost(distr[i], newProb) + prob2bits[256-DIFF_UPDATE_PROB] + (MV_PROB_UPDATE_BITS<<9);

        if (newCost < oldCost) {
            fprintf_trace_prob_update(stderr, "%u ", (oldCost - newCost + 256)>>9);
            WriteBool<CodecType>(bc, 1, DIFF_UPDATE_PROB);
            WriteLiteral<CodecType>(bc, newProb >> 1, 7);
            probs[i] = newProb;
        } else {
            fprintf_trace_prob_update(stderr, "0 ");
            WriteBool<CodecType>(bc, 0, DIFF_UPDATE_PROB);
        }
    }

    fprintf_trace_prob_update(stderr, "]\n");
}

uint32_t AV1Enc::EstimateCoefDiffProb(const uint32_t countsMoreCoef[2], const uint32_t countsToken[NUM_TOKENS],
                                     const vpx_prob oldProbs[3], vpx_prob newProbs[3])
{
    const uint32_t updateFlagCost = prob2bits[256 - DIFF_UPDATE_PROB];
    uint32_t savings = 0;

    // node=0 (more_coef)
    {
        const vpx_prob oldProb = oldProbs[0];
        const uint32_t oldCost = GetBinCost(countsMoreCoef, oldProb) + prob2bits[DIFF_UPDATE_PROB];
        const vpx_prob realProb = GetBinProb(countsMoreCoef, oldProb);
        const int32_t step = realProb > oldProb ? -1 : 1;
        vpx_prob bestProb = oldProb;
        uint32_t bestCost = oldCost;

        if (bestCost > updateFlagCost + (MIN_DELP_BITS << 9)) {
            for (int32_t prob = realProb; prob != oldProb; prob += step) {
                uint32_t newCost = GetBinCost(countsMoreCoef, prob);
                uint32_t updateCost = GetProbUpdateCost(prob, oldProb) + updateFlagCost;
                if (bestCost > newCost + updateCost) {
                    bestCost = newCost + updateCost;
                    bestProb = prob;
                }
            }
        }
        assert(bestCost <= oldCost);
        newProbs[0] = bestProb;
        savings += oldCost - bestCost;
    }

    uint32_t distrToken[NUM_TOKENS-1][2];
    GetTreeDistrToken(countsToken, distrToken);

    // node=1 (token)
    {
        const vpx_prob oldProb = oldProbs[1];
        const uint32_t oldCost = GetBinCost(distrToken[0], oldProb) + prob2bits[DIFF_UPDATE_PROB];
        const vpx_prob realProb = GetBinProb(distrToken[0], oldProb);
        const int32_t step = realProb > oldProb ? -1 : 1;
        vpx_prob bestProb = oldProb;
        uint32_t bestCost = oldCost;

        if (bestCost > updateFlagCost + (MIN_DELP_BITS << 9)) {
            for (int32_t prob = realProb; prob != oldProb; prob += step) {
                uint32_t newCost = GetBinCost(distrToken[0], prob);
                uint32_t updateCost = GetProbUpdateCost(prob, oldProb) + updateFlagCost;
                if (bestCost > newCost + updateCost) {
                    bestCost = newCost + updateCost;
                    bestProb = prob;
                }
            }
        }
        assert(bestCost <= oldCost);
        newProbs[1] = bestProb;
        savings += oldCost - bestCost;
    }

    // node=2 (token, pivot node)
    {
        const vpx_prob oldProb = oldProbs[2];
        uint32_t oldCost = GetBinCost(distrToken[1], oldProb) + prob2bits[DIFF_UPDATE_PROB];
        for (int32_t i = 2; i < NUM_TOKENS-1; i++)
            oldCost += GetBinCost(distrToken[i], pareto(i, oldProb));
        const vpx_prob realProb = GetBinProb(distrToken[1], oldProb);
        const int32_t step = realProb > oldProb ? -1 : 1;
        vpx_prob bestProb = oldProb;
        uint32_t bestCost = oldCost;

        if (bestCost > updateFlagCost + (MIN_DELP_BITS << 9)) {
            for (int32_t prob = realProb; prob != oldProb; prob += step) {
                uint32_t newCost = GetBinCost(distrToken[1], prob);
                for (int32_t i = 2; i < NUM_TOKENS-1; i++)
                    newCost += GetBinCost(distrToken[i], pareto(i, prob));
                uint32_t updateCost = GetProbUpdateCost(prob, oldProb) + updateFlagCost;
                if (bestCost > newCost + updateCost) {
                    bestCost = newCost + updateCost;
                    bestProb = prob;
                }
            }
        }
        assert(bestCost <= oldCost);
        newProbs[2] = bestProb;
        savings += oldCost - bestCost;
    }

    return savings;
}

void AV1Enc::ProcessPartition(SuperBlock &sb, int32_t miRow, int32_t miCol, int32_t log2SbSize, FrameCounts *counts) {
    Frame &frame = *sb.frame;
    if (miRow >= sb.par->miRows || miCol >= sb.par->miCols)
        return;

    const ModeInfo *mi = frame.m_modeInfo + miCol + miRow * sb.par->miPitch;
    const BlockSize currBlockSize = 3 * (log2SbSize - 2);
    const PartitionType partition = GetPartition(currBlockSize, mi->sbType);

    assert(log2SbSize <= 6 && log2SbSize >= 3);
    int32_t num8x8 = 1 << (log2SbSize - 3);
    int32_t halfBlock8x8 = num8x8 >> 1;
    int32_t hasRows = (miRow + halfBlock8x8) < sb.par->miRows;
    int32_t hasCols = (miCol + halfBlock8x8) < sb.par->miCols;

    int32_t bsl = log2SbSize - 3;
    int32_t above = (sb.ctx.abovePartition[miCol & 7] >> bsl) & 1;
    int32_t left = (sb.ctx.leftPartition[miRow & 7] >> bsl) & 1;
    int32_t ctx = bsl * 4 + left * 2 + above;
    counts->partition[ctx][partition]++;

    if (log2SbSize == 3 || partition == PARTITION_NONE) {
        ProcessModes(sb, miRow, miCol, counts);
    } else if (partition == PARTITION_HORZ) {
        ProcessModes(sb, miRow, miCol, counts);
        if (hasRows)
            ProcessModes(sb, miRow + halfBlock8x8, miCol, counts);
    } else if (partition == PARTITION_VERT) {
        ProcessModes(sb, miRow, miCol, counts);
        if (hasCols)
            ProcessModes(sb, miRow, miCol + halfBlock8x8, counts);
    } else {
        ProcessPartition(sb, miRow, miCol, log2SbSize - 1, counts);
        ProcessPartition(sb, miRow, miCol + halfBlock8x8, log2SbSize - 1, counts);
        ProcessPartition(sb, miRow + halfBlock8x8, miCol, log2SbSize - 1, counts);
        ProcessPartition(sb, miRow + halfBlock8x8, miCol + halfBlock8x8, log2SbSize - 1, counts);
    }

    // update partition context
    if (log2SbSize == 3 || partition != PARTITION_SPLIT) {
        memset(sb.ctx.abovePartition + (miCol & 7), partition_context_lookup[mi->sbType].above, num8x8);
        memset(sb.ctx.leftPartition + (miRow & 7),  partition_context_lookup[mi->sbType].left,  num8x8);
    }
}

void AV1Enc::AccumulateCounts(const AV1VideoParam &par, Frame *frame, const CoeffsType *coefs, FrameCounts *countsSb, int32_t numCountsSb)
{
    assert((size_t(countsSb) & 0xf) == 0);
    assert((sizeof(*countsSb) & 0x3f) == 0);

    const int32_t nchunks = sizeof(FrameCounts) >> 6;
    for (int32_t i = 1; i < numCountsSb; i++) {
        char *pacc = reinterpret_cast<char *>(countsSb);
        char *psb = reinterpret_cast<char *>(countsSb + i);
        for (int32_t i = 0; i < nchunks; i++, pacc += 64, psb += 64) {
            storea_si128(pacc+0,  _mm_add_epi32(loada_si128(pacc+0),  loada_si128(psb+0)));
            storea_si128(pacc+16, _mm_add_epi32(loada_si128(pacc+16), loada_si128(psb+16)));
            storea_si128(pacc+32, _mm_add_epi32(loada_si128(pacc+32), loada_si128(psb+32)));
            storea_si128(pacc+48, _mm_add_epi32(loada_si128(pacc+48), loada_si128(psb+48)));
        }
    }

    if (!frame->IsIntra() && frame->interpolationFilter == SWITCHABLE) {
        uint32_t sum[SWITCHABLE_FILTERS] = {};
        int32_t c = 0;
        for (int32_t f = 0; f < SWITCHABLE_FILTERS; f++) {
            for (int32_t i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
                sum[f] += countsSb[0].interp_filter[i][f];
            }
            c += (sum[f] > 0);
        }

        /*if (sum[EIGHTTAP_SMOOTH] == 0 && sum[EIGHTTAP_SHARP] == 0)
            frame->interpolationFilter = EIGHTTAP;
        else if (sum[EIGHTTAP] == 0 && sum[EIGHTTAP_SHARP] == 0)
            frame->interpolationFilter = EIGHTTAP_SMOOTH;
        else if (sum[EIGHTTAP] == 0 && sum[EIGHTTAP_SMOOTH] == 0)
            frame->interpolationFilter = EIGHTTAP_SHARP;*/

        if (c == 1) {
            // Only one filter is used. So set the filter at frame level
            for (int32_t f = 0; f < SWITCHABLE_FILTERS; ++f) {
                if (sum[f]) {
                    frame->interpolationFilter = f;
                    break;
                }
            }
        }
    }
}

static inline int get_unsigned_bits(unsigned int num_values) {
    return num_values > 0 ? get_msb(num_values) + 1 : 0;
}

template
void AV1Enc::WriteDiffProb<CODEC_AV1>(BoolCoder &bc, const uint32_t(*distr)[2], vpx_prob *probs, int32_t numNodes, const char *tracestr);
template
void AV1Enc::WriteDiffProbMv<CODEC_AV1>(BoolCoder &bc, const uint32_t(*distr)[2], vpx_prob *probs, int32_t numNodes, const char *tracestr);

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
