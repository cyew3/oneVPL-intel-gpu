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

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_bitwriter.h"
#include "mfx_av1_probabilities.h"

namespace AV1Enc {
    template <EnumCodecType CodecType>
    inline void WriteIntraMode(BoolCoder &bc, PredMode mode, const vpx_prob *probs);
    template <EnumCodecType CodecType>
    //inline void WriteMvFr(BoolCoder &bc, int32_t mvFr, const vpx_prob *probs);
    inline void WriteMvFrAV1(BoolCoder &bc, int32_t mvFr, aom_cdf_prob *probs);
    inline void WriteInterModeAV1(BoolCoder &bc, PredMode mode, EntropyContexts &ectx, int32_t modeCtx);
    inline void WriteInterCompoundModeAV1(BoolCoder &bc, PredMode mode, EntropyContexts &ectx, int32_t modeCtx);
    inline void WriteInterpAV1(BoolCoder &bc, InterpFilter interp, EntropyContexts &context, uint32_t ctx);
    inline void WriteMvJointAV1(BoolCoder &bc, int32_t mvJoint, MvContextsAv1 &mvc);
    inline int32_t GetToken(int32_t absCoef);
    inline void GetToken(int32_t absCoef, int32_t *tok, int32_t *numExtra, int32_t *base);


    template <EnumCodecType CodecType>
    inline void WriteIntraMode(BoolCoder &bc, PredMode mode, const vpx_prob *probs) {
        switch (mode) {
        default:
        case DC_PRED:
            WriteBool<CodecType>(bc, 0, probs[0]);
            return;
        case TM_PRED:
            WriteBool<CodecType>(bc, 1, probs[0]);
            WriteBool<CodecType>(bc, 0, probs[1]);
            return;
        case V_PRED:
            WriteBool<CodecType>(bc, 1, probs[0]);
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 0, probs[2]);
            return;
        case H_PRED:
            WriteBool<CodecType>(bc, 1, probs[0]);
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 0, probs[3]);
            WriteBool<CodecType>(bc, 0, probs[4]);
            return;
        case D135_PRED:
            WriteBool<CodecType>(bc, 1, probs[0]);
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 0, probs[3]);
            WriteBool<CodecType>(bc, 1, probs[4]);
            WriteBool<CodecType>(bc, 0, probs[5]);
            return;
        case D117_PRED:
            WriteBool<CodecType>(bc, 1, probs[0]);
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 0, probs[3]);
            WriteBool<CodecType>(bc, 1, probs[4]);
            WriteBool<CodecType>(bc, 1, probs[5]);
            return;
        case D45_PRED:
            WriteBool<CodecType>(bc, 1, probs[0]);
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 1, probs[3]);
            WriteBool<CodecType>(bc, 0, probs[6]);
            return;
        case D63_PRED:
            WriteBool<CodecType>(bc, 1, probs[0]);
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 1, probs[3]);
            WriteBool<CodecType>(bc, 1, probs[6]);
            WriteBool<CodecType>(bc, 0, probs[7]);
            return;
        case D153_PRED:
            WriteBool<CodecType>(bc, 1, probs[0]);
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 1, probs[3]);
            WriteBool<CodecType>(bc, 1, probs[6]);
            WriteBool<CodecType>(bc, 1, probs[7]);
            WriteBool<CodecType>(bc, 0, probs[8]);
            return;
        case D207_PRED:
            WriteBool<CodecType>(bc, 1, probs[0]);
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 1, probs[3]);
            WriteBool<CodecType>(bc, 1, probs[6]);
            WriteBool<CodecType>(bc, 1, probs[7]);
            WriteBool<CodecType>(bc, 1, probs[8]);
            return;
        }
    }

    inline void WriteMvFrAV1(BoolCoder &bc, int32_t mvFr, aom_cdf_prob *probs) {
        WriteSymbol(bc, mvFr, probs, MV_FP_SIZE);
    }

    inline void WriteInterModeAV1(BoolCoder &bc, PredMode mode, EntropyContexts &ectx, int32_t modeCtx) {
        const int16_t newmvCtx = modeCtx & NEWMV_CTX_MASK;
        WriteSymbol(bc, mode != AV1_NEWMV, ectx.newmv_cdf[newmvCtx], 2);
        if (mode != AV1_NEWMV) {
            const int16_t zeromvCtx = (modeCtx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
            WriteSymbol(bc, mode != AV1_ZEROMV, ectx.zeromv_cdf[zeromvCtx], 2);

            if (mode != AV1_ZEROMV) {
                const int32_t refmvCtx = (modeCtx >> REFMV_OFFSET) & REFMV_CTX_MASK;
                WriteSymbol(bc, mode != AV1_NEARESTMV, ectx.refmv_cdf[refmvCtx], 2);
            }
        }
    }

    inline void WriteInterCompoundModeAV1(BoolCoder &bc, PredMode mode, EntropyContexts &ectx, int32_t modeCtx) {
        //assert(mode >= NEAREST_NEARESTMV && mode <= NEW_NEWMV);
        if      (mode == AV1_NEARESTMV) mode = NEAREST_NEARESTMV;
        else if (mode == AV1_NEARMV) mode = NEAR_NEARMV;
        else if (mode == AV1_ZEROMV) mode = ZERO_ZEROMV;
        else if (mode == AV1_NEWMV) mode = NEW_NEWMV;
        else if (mode == NEW_NEARESTMV) mode = NEW_NEARESTMV;
        else if (mode == NEAREST_NEWMV) mode = NEAREST_NEWMV;
        else if (mode == NEAR_NEWMV) mode = NEAR_NEWMV;
        else if (mode == NEW_NEARMV) mode = NEW_NEARMV;
        else    {assert(0);}
        assert(modeCtx < INTER_MODE_CONTEXTS);
        WriteSymbol(bc, mode - NEAREST_NEARESTMV, ectx.inter_compound_mode_cdf[modeCtx], AV1_INTER_COMPOUND_MODES);
    }

    inline void WriteInterpAV1(BoolCoder &bc, InterpFilter interp, EntropyContexts &context, uint32_t ctx)
    {
        // hack for dual filtering due to reordering interp mode
        // original order is EIGHTTAP, EIGHTTAP_SMOOTH, EIGHTTAP_SHARP, EIGHTTAP_SMOOTH2
        InterpFilter av1_switchable_interp_ind[SWITCHABLE_FILTERS] = {EIGHTTAP, EIGHTTAP_SMOOTH, EIGHTTAP_SHARP};
        WriteSymbol(bc, av1_switchable_interp_ind[interp], context.switchable_interp_cdf[ctx], SWITCHABLE_FILTERS);
    }

    template <EnumCodecType CodecType>
    inline int32_t WriteToken(BoolCoder &bc, int32_t coef, const vpx_prob probs[3]) {
        if (coef == 0) {
            WriteBool<CodecType>(bc, 0, probs[1]);
            return ZERO_TOKEN;
        } else if (coef == 1) {
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 0, probs[2]);
            return ONE_TOKEN;
        } else if (coef == 2) {
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 0, pareto(2, probs[2]));
            WriteBool<CodecType>(bc, 0, pareto(3, probs[2]));
            return TWO_TOKEN;
        } else if (coef == 3) {
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 0, pareto(2, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(3, probs[2]));
            WriteBool<CodecType>(bc, 0, pareto(4, probs[2]));
            return THREE_TOKEN;
        } else if (coef == 4) {
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 0, pareto(2, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(3, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(4, probs[2]));
            return FOUR_TOKEN;
        } else if (coef < CAT2_MIN_VAL) {
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 1, pareto(2, probs[2]));
            WriteBool<CodecType>(bc, 0, pareto(5, probs[2]));
            WriteBool<CodecType>(bc, 0, pareto(6, probs[2]));
            return DCT_VAL_CATEGORY1;
        } else if (coef < CAT3_MIN_VAL) {
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 1, pareto(2, probs[2]));
            WriteBool<CodecType>(bc, 0, pareto(5, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(6, probs[2]));
            return DCT_VAL_CATEGORY2;
        } else if (coef < CAT4_MIN_VAL) {
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 1, pareto(2, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(5, probs[2]));
            WriteBool<CodecType>(bc, 0, pareto(7, probs[2]));
            WriteBool<CodecType>(bc, 0, pareto(8, probs[2]));
            return DCT_VAL_CATEGORY3;
        } else if (coef < CAT5_MIN_VAL) {
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 1, pareto(2, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(5, probs[2]));
            WriteBool<CodecType>(bc, 0, pareto(7, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(8, probs[2]));
            return DCT_VAL_CATEGORY4;
        } else if (coef < CAT6_MIN_VAL) {
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 1, pareto(2, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(5, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(7, probs[2]));
            WriteBool<CodecType>(bc, 0, pareto(9, probs[2]));
            return DCT_VAL_CATEGORY5;
        } else { // DCT_VAL_CATEGORY6
            WriteBool<CodecType>(bc, 1, probs[1]);
            WriteBool<CodecType>(bc, 1, probs[2]);
            WriteBool<CodecType>(bc, 1, pareto(2, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(5, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(7, probs[2]));
            WriteBool<CodecType>(bc, 1, pareto(9, probs[2]));
            return DCT_VAL_CATEGORY6;
        }
    }

    inline void WriteMvJointAV1(BoolCoder &bc, int32_t mvJoint, MvContextsAv1 &mvc) {
        WriteSymbol(bc, mvJoint, mvc.joint_cdf, MV_JOINTS);
    }

    inline int32_t GetToken(int32_t absCoef) {
        if (absCoef < CAT1_MIN_VAL) {
            return absCoef;  // ZERO_TOKEN .. FOUR_TOKEN
        } else {
            int32_t tok = BSR(absCoef - 3) + 4;
            return absCoef < CAT6_MIN_VAL ? tok : DCT_VAL_CATEGORY6; // DCT_VAL_CATEGORY1 .. DCT_VAL_CATEGORY6
        }
    }

    inline void GetToken(int32_t absCoef, int32_t *tok, int32_t *numExtra, int32_t *remainder) {
        if (absCoef < CAT1_MIN_VAL) {  // ZERO_TOKEN .. FOUR_TOKEN
            *tok = absCoef;
            *numExtra = 0;
            *remainder = 0;
        } else if (absCoef < CAT6_MIN_VAL) {  // DCT_VAL_CATEGORY1 .. DCT_VAL_CATEGORY5
            int32_t bsr = BSR(absCoef - 3);
            *tok = bsr + 4;
            *numExtra = bsr;
            *remainder = absCoef - 3 + (1 << bsr);
        } else {  // DCT_VAL_CATEGORY6
            *tok = DCT_VAL_CATEGORY6;
            *numExtra = 14;
            *remainder = absCoef - CAT6_MIN_VAL;
        }
    }
};

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
