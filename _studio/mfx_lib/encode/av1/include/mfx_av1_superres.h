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
#include "mfx_av1_enc.h"
#include "mfx_av1_frame.h"

namespace AV1Enc {

    void av1_upscale_normative_and_extend_frame(const AV1VideoParam& par, FrameData *in, FrameData *out);

    struct PadBlock {
        int x0;
        int x1;
        int y0;
        int y1;
    };

    struct SubpelParams {
        int xs;
        int ys;
        int subpel_x;
        int subpel_y;
    };

    void calc_subpel_params(
        const AV1MV mv,
        int plane,
        SubpelParams *subpel_params,
        int bw, int bh, PadBlock *block,
        int mi_x, int mi_y,
        int frameWidth, int frameHeight);
    void InterpolateAv1SingleRefLuma_ScaleMode_new(FrameData* ref, uint8_t *dst, AV1MV mv, int32_t x, int32_t y, int32_t w, int32_t h, int32_t interp);

    void InterpolateAv1SingleRefChroma_ScaleMode(const uint8_t *refColoc, int32_t refPitch, uint8_t *dst, AV1MV mv, int32_t w, int32_t h, int32_t interp, SubpelParams *subpel_params);

    void InterpolateAv1FirstRefLuma_ScaleMode_new(FrameData* ref, int16_t *dst, AV1MV mv, int32_t x, int32_t y, int32_t w, int32_t h, int32_t interp);

    void InterpolateAv1SecondRefLuma_ScaleMode_new(FrameData* refFrame, const int16_t* ref0, uint8_t *dst, AV1MV mv, int32_t x, int32_t y, int32_t w, int32_t h, int32_t interp);

    void InterpolateAv1FirstRefChroma_ScaleMode(const uint8_t *refColoc, int32_t refPitch, uint16_t *dst, AV1MV mv, int32_t w, int32_t h, int32_t interp, SubpelParams *subpel_params);

    void InterpolateAv1SecondRefChroma_ScaleMode(const uint8_t *refColoc, int32_t refPitch, uint16_t* ref0, uint8_t *dst, AV1MV mv, int32_t w, int32_t h, int32_t interp, SubpelParams *subpel_params);
};

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
