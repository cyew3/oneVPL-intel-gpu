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
    struct BoolCoder;

    template <EnumCodecType CodecType>
    void WriteProbDiffUpdate(BoolCoder &bc, vpx_prob newp, vpx_prob oldp);
    void GetTreeDistrTx8x8(const uint32_t *counts, uint32_t (*distr)[2]);
    void GetTreeDistrTx16x16(const uint32_t *counts, uint32_t (*distr)[2]);
    void GetTreeDistrTx32x32(const uint32_t *counts, uint32_t (*distr)[2]);
    void GetTreeDistrToken(const uint32_t *counts, uint32_t (*distr)[2]);
    void GetTreeDistrInterMode(const uint32_t *counts, uint32_t (*distr)[2]);
    void GetTreeDistrInterpFilter(const uint32_t *counts, uint32_t (*distr)[2]);
    void GetTreeDistrYMode(const uint32_t *counts, uint32_t (*distr)[2]);
    void GetTreeDistrPartition(const uint32_t *counts, uint32_t (*distr)[2]);
    void GetTreeDistrMvJoint(const uint32_t *counts, uint32_t (*distr)[2]);
    void GetTreeDistrMvClass(const uint32_t *counts, uint32_t (*distr)[2]);
    void GetTreeDistrMvFr(const uint32_t *counts, uint32_t (*distr)[2]);
    template <EnumCodecType CodecType>
    void WriteDiffProb(BoolCoder &bc, const uint32_t (*distr)[2], vpx_prob *probs, int32_t numNodes, const char *tracestr);
    template <EnumCodecType CodecType>
    void WriteDiffProbPivot(BoolCoder &bc, const uint32_t (*distr)[2], vpx_prob *prob, const char *tracestr);
    template <EnumCodecType CodecType>
    void WriteDiffProbMv(BoolCoder &bc, const uint32_t (*distr)[2], vpx_prob *probs, int32_t numNodes, const char *tracestr);
    void AccumulateCounts(const AV1VideoParam &par, Frame *frame, const CoeffsType *coefs, FrameCounts *countsSb, int32_t numCountsSb);
    void ProcessPartition(SuperBlock &sb, int32_t miRow, int32_t miCol, int32_t log2SbSize, FrameCounts *counts);

    uint32_t EstimateCoefDiffProb(const uint32_t countsMoreCoef[2], const uint32_t countsToken[NUM_TOKENS],
                                const vpx_prob oldProbs[3], vpx_prob newProbs[3]);
};


#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
