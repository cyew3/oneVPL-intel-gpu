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


#include "mfx_av1_defs.h"

namespace AV1Enc {
    struct AV1VideoParam;
    struct LoopFilterThresh {
        alignas(16) uint8_t mblim[16];
        alignas(16) uint8_t lim[16];
        alignas(16) uint8_t hev_thr[16];
    };
    struct LoopFilterMask {
        uint64_t left_y[TX_SIZES];
        uint64_t above_y[TX_SIZES];
        uint64_t int_4x4_y;
        uint16_t left_uv[TX_SIZES];
        uint16_t above_uv[TX_SIZES];
        uint16_t int_4x4_uv;
        uint8_t lfl_y[64];
    };
    struct LoopFilterInfoN {
      uint8_t lvl[MAX_SEGMENTS][MAX_REF_FRAMES][MAX_MODE_LF_DELTAS];
    };


    class LoopFilterFrameParam {
    public:
        uint8_t level;
        uint8_t sharpness;

        uint8_t deltaEnabled;
        uint8_t deltaUpdate;

        // 0 = Intra, Last, GF, ARF
        int8_t refDeltas[MAX_REF_LF_DELTAS];
        // 0 = ZERO_MV, MV
        int8_t modeDeltas[MAX_MODE_LF_DELTAS];
    };

    struct ModeInfo;

    // VP9
    void LoopFilterInitThresh(LoopFilterThresh (*lfts)[MAX_LOOP_FILTER + 1]);
    void LoopFilterResetParams(LoopFilterFrameParam *par);
    void LoopFilterAdjustMask(const int32_t mi_row, const int32_t mi_col, int32_t mi_rows, int32_t mi_cols, LoopFilterMask *lfm);
    void LoopFilterSetupMask(const int32_t mi_row, const int32_t mi_col, int32_t mi_rows, int32_t mi_cols, ModeInfo *mi, int32_t miPitch, LoopFilterMask *lfm, AV1VideoParam &par);
    void LoopFilterFrame(Frame *frame, const AV1VideoParam *par, uint8_t *last_level);
    void LoopFilterRow(Frame *frame, int32_t sbRow, const AV1VideoParam *par);

    //AV1
    void LoopFilterSbAV1(Frame *frame, int32_t sbRow, int32_t sbCol, const AV1VideoParam &par);
    int32_t FilterLevelFromQp(int32_t base_qindex, uint8_t isKeyFrame);
};
#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
