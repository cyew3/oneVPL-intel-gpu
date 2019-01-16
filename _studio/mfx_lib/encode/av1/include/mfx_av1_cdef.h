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

#define ALIGN_POWER_OF_TWO(value, n) (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

namespace AV1Enc {
    struct AV1VideoParam;

    static const int32_t CDEF_BLOCKSIZE   = 64;
    static const int32_t CDEF_NBLOCKS     = CDEF_BLOCKSIZE / 8;
    static const int32_t CDEF_BORDER      = 2;
    static const int32_t CDEF_VBORDER     = CDEF_BORDER;
    static const int32_t CDEF_HBORDER     = (CDEF_BORDER + 7) & ~7;
    static const int32_t CDEF_BSTRIDE     = ALIGN_POWER_OF_TWO(CDEF_BLOCKSIZE + 2 * CDEF_HBORDER, 3);
    static const uint16_t CDEF_VERY_LARGE  = 30000;
    static const int32_t CDEF_INBUF_SIZE  = CDEF_BSTRIDE * (CDEF_BLOCKSIZE + 2 * CDEF_VBORDER);

    static const uint32_t CDEF_VERY_LARGE_CHROMA = CDEF_VERY_LARGE + (uint32_t(CDEF_VERY_LARGE) << 16);
    static const int32_t CDEF_INBUF_SIZE_CHROMA = CDEF_BSTRIDE * (CDEF_BLOCKSIZE / 2 + 2 * CDEF_VBORDER);

    struct cdef_list {
        uint8_t by;
        uint8_t bx;
    };

    struct CdefParam {
        int32_t cdef_bits;
        int32_t nb_cdef_strengths;
        int32_t cdef_strengths[CDEF_MAX_STRENGTHS];
        int32_t cdef_uv_strengths[CDEF_MAX_STRENGTHS];
    };

    struct FrameData;
    class CdefLineBuffers
    {
    public:
        CdefLineBuffers();
        ~CdefLineBuffers();
        void Alloc(const AV1VideoParam &par);
        void Free();
        const uint16_t *PrepareSb(const int sbr, const int sbc, FrameData& frame, uint16_t* in0, uint16_t* in1);

        uint8_t* aboveBorder;
        uint8_t* aboveBorderY;
        uint8_t* aboveBorderUv;
        int32_t pitchAbove;

        uint8_t* leftBorder;
        uint8_t* leftBorderUv;
        int32_t pitchLeft;
    };

    void CdefParamInit(CdefParam &param);
    void CdefSearchSb(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc, uint16_t* in0, uint16_t* in1);
    void CdefSearchRow(const AV1VideoParam &par, Frame *frame, int32_t row);
    void CdefSearchFrame(const AV1VideoParam &par, Frame *frame);
    void CdefSearchSync(const AV1VideoParam &par, Frame *frame);
    void CdefStoreBorderSb(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc);
    void CdefApplyFrame(const AV1VideoParam &par, Frame *frame);
    void CdefApplyFrameOld(const AV1VideoParam &par, Frame *frame);
    void CdefApplySb(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc, uint16_t* in0, uint16_t* in1);

    void CdefOnePassSb(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc);
};
#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
