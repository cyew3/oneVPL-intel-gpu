//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2016 Intel Corporation. All Rights Reserved.
//

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
    if (isIntra && width == 4)
        return MFX_HEVC_PP::NAME(h265_DST4x4Fwd_16s)(src, srcPitch, dst, bitDepth);
    else {
        switch (width) {
        case 4:  return MFX_HEVC_PP::NAME(h265_DCT4x4Fwd_16s)(src, srcPitch, dst, bitDepth);
        case 8:  return MFX_HEVC_PP::NAME(h265_DCT8x8Fwd_16s)(src, srcPitch, dst, bitDepth);
        case 16: return MFX_HEVC_PP::NAME(h265_DCT16x16Fwd_16s)(src, srcPitch, dst, bitDepth);
        case 32: return MFX_HEVC_PP::NAME(h265_DCT32x32Fwd_16s)(src, srcPitch, dst, bitDepth);
        }
    }
}


template void H265CU<Ipp8u>::TransformInv(Ipp8u *pred, Ipp32s pitchPred, CoeffsType *coef, Ipp32s width, void *dst, Ipp32s pitchDst, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);
template void H265CU<Ipp8u>::TransformFwd(CoeffsType *src, Ipp32s srcPitch, CoeffsType *dst, Ipp32s width, Ipp32s bitDepth, Ipp8u isIntra);
template void H265CU<Ipp16u>::TransformInv(Ipp16u *pred, Ipp32s pitchPred, CoeffsType *coef, Ipp32s width, void *dst, Ipp32s pitchDst, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);
template void H265CU<Ipp16u>::TransformFwd(CoeffsType *src, Ipp32s srcPitch, CoeffsType *dst, Ipp32s width, Ipp32s bitDepth, Ipp8u isIntra);

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
