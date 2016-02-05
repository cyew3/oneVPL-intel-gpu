//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_enc.h"

//#include "mfx_h265_optimization.h"

namespace H265Enc {

/*
void h265_transform_direct_copy(const VitIf in_VitIf)
{
    I32 i, j;

    for (i = 0; i < in_VitIf.Height; i++)
    {
        for (j = 0; j < in_VitIf.Width; j++)
        {
            in_VitIf.pTransformResult[i*in_VitIf.Width+j] = in_VitIf.pCoeffBuf[i*in_VitIf.Width+j];
        }
    }
}

void HevcPakMfdVft::DoDirectCopyMxN(const VftIf &in_VftIf)
{
    I32 i, j;

    for (i = 0; i < in_VftIf.Height; i++)
    {
        for (j = 0; j < in_VftIf.Width; j++)
        {
            in_VftIf.pCoeffBuf[i*in_VftIf.Width+j] = in_VftIf.pDiffBuf[i*in_VftIf.Width+j];
        }
    }
}
*/

template <typename PixType>
void H265CU<PixType>::TransformInv(Ipp32s offset, Ipp32s width, Ipp8u is_luma, Ipp8u is_intra, Ipp8u bitDepth)
{
    for (Ipp32s c_idx = 0; c_idx < (is_luma ? 1 : 2); c_idx ++) {
        CoeffsType *residuals = is_luma ? m_residualsY : (c_idx ? m_residualsV : m_residualsU);
        residuals += offset;

        if (is_luma && is_intra && width == 4) {
            MFX_HEVC_PP::NAME(h265_DST4x4Inv_16sT)(residuals, residuals, 4, false, bitDepth);
        } else {
            switch (width) {
            case 4:
                MFX_HEVC_PP::NAME(h265_DCT4x4Inv_16sT)(residuals, residuals, 4, false, bitDepth); break;
            case 8:
                MFX_HEVC_PP::NAME(h265_DCT8x8Inv_16sT)(residuals, residuals, 8, false, bitDepth); break;
            case 16:
                MFX_HEVC_PP::NAME(h265_DCT16x16Inv_16sT)(residuals, residuals, 16, false, bitDepth); break;
            case 32:
                MFX_HEVC_PP::NAME(h265_DCT32x32Inv_16sT)(residuals, residuals, 32, false, bitDepth); break;
            }
        }
    }
}

template <typename PixType>
void H265CU<PixType>::TransformInv2(void *dst, Ipp32s pitch_dst, Ipp32s offset, Ipp32s width, Ipp8u is_luma, Ipp8u is_intra, Ipp8u bitDepth)
{
    for (Ipp32s c_idx = 0; c_idx < (is_luma ? 1 : 2); c_idx ++) {
        CoeffsType *residuals = is_luma ? m_residualsY : (c_idx ? m_residualsV : m_residualsU);
        residuals += offset;

        if (is_luma && is_intra && width == 4) {
            MFX_HEVC_PP::NAME(h265_DST4x4Inv_16sT)(dst, residuals, pitch_dst, true, bitDepth);
        } else {
            switch (width) {
            case 4:
                MFX_HEVC_PP::NAME(h265_DCT4x4Inv_16sT)(dst, residuals, pitch_dst, true, bitDepth); break;
            case 8:
                MFX_HEVC_PP::NAME(h265_DCT8x8Inv_16sT)(dst, residuals, pitch_dst, true, bitDepth); break;
            case 16:
                MFX_HEVC_PP::NAME(h265_DCT16x16Inv_16sT)(dst, residuals, pitch_dst, true, bitDepth); break;
            case 32:
                MFX_HEVC_PP::NAME(h265_DCT32x32Inv_16sT)(dst, residuals, pitch_dst, true, bitDepth); break;
            }
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


template void H265CU<Ipp8u>::TransformInv(Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);
template void H265CU<Ipp8u>::TransformInv2(void *dst, Ipp32s pitch_dst, Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);
template void H265CU<Ipp8u>::TransformFwd(CoeffsType *src, Ipp32s srcPitch, CoeffsType *dst, Ipp32s width, Ipp32s bitDepth, Ipp8u isIntra);
template void H265CU<Ipp16u>::TransformInv(Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);
template void H265CU<Ipp16u>::TransformInv2(void *dst, Ipp32s pitch_dst, Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);
template void H265CU<Ipp16u>::TransformFwd(CoeffsType *src, Ipp32s srcPitch, CoeffsType *dst, Ipp32s width, Ipp32s bitDepth, Ipp8u isIntra);

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
