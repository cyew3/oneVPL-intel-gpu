//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_transform_opt.h"

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

void H265CU::TransformInv(Ipp32s offset, Ipp32s width, Ipp8u is_luma, Ipp8u is_intra)
{
//    Ipp8s uMBQP = par->QP;
//    bool transform_bypass = m_VitState.ImageState.qpprime_y_zero_transform_bypass_flag && uMBQP == 0;

    for (Ipp32s c_idx = 0; c_idx < (is_luma ? 1 : 2); c_idx ++) {
        CoeffsType *residuals = is_luma ? residuals_y : (c_idx ? residuals_v : residuals_u);
        Ipp32s bit_depth = is_luma ? BIT_DEPTH_LUMA : BIT_DEPTH_CHROMA;
        residuals += offset;

/*    if (transform_bypass)
    {
        printf("Error!! FTQ Transform Bypass\n");
        return;

//        DoDirectCopyMxN(in_VitIf);
    }
    else */
        if (is_luma && is_intra && width == 4)
        {
            h265_dst_inv4x4(residuals, bit_depth);
        }
        else
        {
            switch (width) {
            case 4:
                h265_dct_inv4x4(residuals, bit_depth);
                break;
            case 8:
                h265_dct_inv8x8(residuals, bit_depth);
                break;
            case 16:
                h265_dct_inv16x16(residuals, bit_depth);
                break;
            case 32:
                h265_dct_inv32x32(residuals, bit_depth);
                break;
            }
        }
    }
}

void H265CU::TransformFwd(Ipp32s offset, Ipp32s width, Ipp8u is_luma, Ipp8u is_intra)
{
//    Ipp8u uMBQP = par->QP;
//    U8 uMBQP = m_MfdVftState.SliceState.SliceQP;
//    bool transform_bypass = m_MfdVftState.ImageState.qpprime_y_zero_transform_bypass_flag && uMBQP == 0;

/*    if (transform_bypass)
    {
        printf("Error!! FTQ Transform Bypass\n");
        return;

        DoDirectCopyMxN(in_VftIf);
    }
    else */
    for (Ipp32s c_idx = 0; c_idx < (is_luma ? 1 : 2); c_idx ++) {
        CoeffsType *residuals = is_luma ? residuals_y : (c_idx ? residuals_v : residuals_u);
        Ipp32s bit_depth = is_luma ? BIT_DEPTH_LUMA : BIT_DEPTH_CHROMA;

        residuals += offset;

        if (is_luma && is_intra && width == 4)
        {
            h265_dst_fwd4x4(residuals, bit_depth);
        }
        else
        {
            switch (width) {
            case 4:
                h265_dct_fwd4x4(residuals, bit_depth);
                break;
            case 8:
                h265_dct_fwd8x8(residuals, bit_depth);
                break;
            case 16:
                h265_dct_fwd16x16(residuals, bit_depth);
                break;
            case 32:
                h265_dct_fwd32x32(residuals, bit_depth);
                break;
            }
        }
    }
}
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
