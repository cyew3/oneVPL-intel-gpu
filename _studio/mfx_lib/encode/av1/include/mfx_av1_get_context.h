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
    struct ModeInfo;
    const ModeInfo *GetLeft (const ModeInfo *currMi, int32_t miCol, int32_t tileMiColStart);
    const ModeInfo *GetAbove(const ModeInfo *currMi, int32_t miPitch, int32_t miRow, int32_t tileMiRowStart);

    ModeInfo *GetLeft (ModeInfo *currMi, int32_t miCol, int32_t tileMiColStart);
    ModeInfo *GetAbove(ModeInfo *currMi, int32_t miPitch, int32_t miRow, int32_t tileMiRowStart);

    int32_t GetCtxCompMode(const ModeInfo *above, const ModeInfo *left, const Frame &frame);
    int32_t GetCtxCompRef(const ModeInfo *above, const ModeInfo *left, const Frame &frame);
    int32_t GetCtxReferenceMode(const ModeInfo *above, const ModeInfo *left);
    template <int32_t dir>
    int32_t GetCtxInterpAV1(const ModeInfo *above, const ModeInfo *left, const RefIdx refIdx[2]);
    int32_t GetCtxInterpBothAV1(const ModeInfo *above, const ModeInfo *left, const RefIdx refIdx[2]);
    int32_t GetCtxInterpBothAV1Fast(const ModeInfo *above, const ModeInfo *left, const RefIdx refIdx[2]);
    int32_t GetCtxInterpVP9(const ModeInfo *above, const ModeInfo *left);
    int32_t GetCtxIsInter(const ModeInfo *above, const ModeInfo *left);
    int32_t GetCtxSingleRefP1(const ModeInfo *above, const ModeInfo *left);
    int32_t GetCtxSingleRefP2(const ModeInfo *above, const ModeInfo *left);
    void GetCtxRefFrames(const RefIdx aboveRefIdx[2], const RefIdx leftRefIdx[2], int32_t fixedRefIdx, int32_t fixedRef, const int32_t varRefs[2], uint8_t (&ctx)[4]);
    int32_t GetCtxSkip(const ModeInfo *above, const ModeInfo *left);
    int32_t GetCtxToken(int32_t c, int32_t x, int32_t y, int32_t num4x4, int32_t txType, const uint8_t *tokenCache, const uint8_t *aboveNz, const uint8_t *leftNz);
    int32_t GetCtxTxSizeVP9(const ModeInfo *above, const ModeInfo *left, TxSize maxTxSize);
    int32_t GetCtxTxSizeAV1(const ModeInfo *above, const ModeInfo *left, uint8_t aboveTxfm, uint8_t leftTxfm, TxSize maxTxSize);
    int32_t GetCtxDrlIdx(const AV1MVCand *refMvStack, int32_t idx);
    int32_t GetCtxDrlIdx(int32_t nearestMvCount, int32_t idx);
    uint8_t GetCtxNmv(int32_t refMvCount, const AV1MVCand *refMvStack, int32_t ref, int32_t refMvIdx);
    int32_t GetCtxTxfm(const uint8_t *aboveTxfm, const uint8_t *leftTxfm, BlockSize bsize, TxSize txSize);

    int32_t GetCtxSingleRefP1Av1(const uint8_t *counts);
    int32_t GetCtxSingleRefP2Av1(const uint8_t *counts);
    int32_t GetCtxSingleRefP3(const uint8_t *counts);
    int32_t GetCtxSingleRefP4(const uint8_t *counts);
    int32_t GetCtxSingleRefP5(const uint8_t *counts);
    int32_t GetCtxCompRefType(const ModeInfo *above, const ModeInfo *left);
    int32_t GetCtxCompRefP0(const uint8_t *counts);
    int32_t GetCtxCompRefP1(const uint8_t *counts);
    int32_t GetCtxCompBwdRefP0(const uint8_t *counts);
    void CollectNeighborsRefCounts(const ModeInfo *above, const ModeInfo *left, uint8_t *neighborsRefCounts);
};

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
