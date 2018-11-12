// Copyright (c) 2012-2018 Intel Corporation
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

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_enc.h"

//#include "mfx_h265_optimization.h"

namespace H265Enc {

template <typename PixType>
void H265CU<PixType>::TransformInv(PixType *pred, Ipp32s pitchPred, CoeffsType *coef, Ipp32s width,
                                    void *dst, Ipp32s pitchDst, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth)
{
    if (isLuma && isIntra && width == 4)
        return MFX_HEVC_PP::NAME(h265_DST4x4Inv_16sT)(pred, pitchPred, dst, coef, pitchDst, pred!=NULL, bitDepth);
    else {
        switch (width) {
        case 4: return MFX_HEVC_PP::NAME(h265_DCT4x4Inv_16sT)(pred, pitchPred, dst, coef, pitchDst, pred!=NULL, bitDepth);
        case 8: return MFX_HEVC_PP::NAME(h265_DCT8x8Inv_16sT)(pred, pitchPred, dst, coef, pitchDst, pred!=NULL, bitDepth);
        case 16: return MFX_HEVC_PP::NAME(h265_DCT16x16Inv_16sT)(pred, pitchPred, dst, coef, pitchDst, pred!=NULL, bitDepth);
        case 32: return MFX_HEVC_PP::NAME(h265_DCT32x32Inv_16sT)(pred, pitchPred, dst, coef, pitchDst, pred!=NULL, bitDepth);
        }
    }
}

template <typename PixType>
void H265CU<PixType>::TransformFwd(CoeffsType *src, Ipp32s srcPitch, CoeffsType *dst, Ipp32s width, Ipp32s bitDepth, Ipp8u isIntra)
{
    __m128i *buffr = m_scratchPad.dct.buffr;
    Ipp16s  *temp = m_scratchPad.dct.temp;
    if (isIntra && width == 4)
        return MFX_HEVC_PP::NAME(h265_DST4x4Fwd_16s)(src, srcPitch, dst, bitDepth);
    else {
        switch (width) {
        case 4:  return MFX_HEVC_PP::NAME(h265_DCT4x4Fwd_16s)(src, srcPitch, dst, bitDepth);
        case 8:  return MFX_HEVC_PP::NAME(h265_DCT8x8Fwd_16s)(src, srcPitch, dst, bitDepth);
        case 16: return MFX_HEVC_PP::NAME(h265_DCT16x16Fwd_16s)(src, srcPitch, dst, bitDepth, temp);
        case 32: return MFX_HEVC_PP::NAME(h265_DCT32x32Fwd_16s)(src, srcPitch, dst, bitDepth, temp, buffr);
        }
    }
}


template void H265CU<Ipp8u>::TransformInv(Ipp8u *pred, Ipp32s pitchPred, CoeffsType *coef, Ipp32s width, void *dst, Ipp32s pitchDst, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);
template void H265CU<Ipp8u>::TransformFwd(CoeffsType *src, Ipp32s srcPitch, CoeffsType *dst, Ipp32s width, Ipp32s bitDepth, Ipp8u isIntra);
template void H265CU<Ipp16u>::TransformInv(Ipp16u *pred, Ipp32s pitchPred, CoeffsType *coef, Ipp32s width, void *dst, Ipp32s pitchDst, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);
template void H265CU<Ipp16u>::TransformFwd(CoeffsType *src, Ipp32s srcPitch, CoeffsType *dst, Ipp32s width, Ipp32s bitDepth, Ipp8u isIntra);

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
