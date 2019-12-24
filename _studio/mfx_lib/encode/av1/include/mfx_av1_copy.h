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
    void CopyCuData(const ModeInfo *src_, int32_t srcMiCols, ModeInfo *dst_, int32_t dstMiCols, int32_t miWidth, int32_t miHeight);
    void CopyPaletteInfo(const PaletteInfo *src, int32_t srcPitch, PaletteInfo *dst, int32_t dstPitch, int32_t miWidth, int32_t miHeight);
    void CopyCoeffs(const int16_t *src_, int16_t *dst_, int32_t numCoeff);
    void ZeroCoeffs(int16_t *dst_, int32_t numCoeff);

    void CopyNxN(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t pitchDst, int32_t N);
    void CopyNxN(const uint16_t *src, int32_t pitchSrc, uint16_t *dst, int32_t pitchDst, int32_t N);

    void CopyNxN(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t N);
    void CopyNxN(const uint16_t *src, int32_t pitchSrc, uint16_t *dst, int32_t N);

    void CopyNxM(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t pitchDst, int32_t N, int32_t M);
    void CopyNxM(const uint16_t *src, int32_t pitchSrc, uint16_t *dst, int32_t pitchDst, int32_t N, int32_t M);

    void CopyNxM(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t N, int32_t M);
    void CopyNxM(const uint16_t *src, int32_t pitchSrc, uint16_t *dst, int32_t N, int32_t M);

    void CopyNxM_unaligned_src(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t pitchDst, int32_t N, int32_t M);
    void CopyNxM_unaligned_src(const uint16_t *src, int32_t pitchSrc, uint16_t *dst, int32_t pitchDst, int32_t N, int32_t M);

    void CopyNxM_unaligned(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t pitchDst, int32_t N, int32_t M);
    void CopyNxM_unaligned(const uint16_t *src, int32_t pitchSrc, uint16_t *dst, int32_t pitchDst, int32_t N, int32_t M);

    template <typename PixType>
    void CopyFromUnalignedNxN(const PixType *src, int pitchSrc, PixType* dst, int pitchDst, int width);
}  // namespace AV1Enc

#endif  // MFX_ENABLE_AV1_VIDEO_ENCODE
