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

Ipp32s h265_quant_calcpattern_sig_ctx( const Ipp32u* sig_coeff_group_flag, Ipp32u posXCG, Ipp32u posYCG,
                                    Ipp32u width, Ipp32u height )
{
    if( width == 4 && height == 4 ) return -1;

    Ipp32u sig_right = 0;
    Ipp32u sig_lower = 0;

    width >>= 2;
    height >>= 2;
    if( posXCG < width - 1 )
    {
        sig_right = (sig_coeff_group_flag[ posYCG * width + posXCG + 1 ] != 0);
    }
    if (posYCG < height - 1 )
    {
        sig_lower = (sig_coeff_group_flag[ (posYCG  + 1 ) * width + posXCG ] != 0);
    }
    return sig_right + (sig_lower<<1);
}

Ipp32u h265_quant_getSigCoeffGroupCtxInc  ( const Ipp32u*               sig_coeff_group_flag,
                                           const Ipp32u                      uCG_pos_x,
                                           const Ipp32u                      uCG_pos_y,
                                           const Ipp32u                      scanIdx,
                                           Ipp32u width, Ipp32u height)
{
    Ipp32u uiRight = 0;
    Ipp32u uiLower = 0;

    width >>= 2;
    height >>= 2;
    if( uCG_pos_x < width - 1 )
    {
        uiRight = (sig_coeff_group_flag[ uCG_pos_y * width + uCG_pos_x + 1 ] != 0);
    }
    if (uCG_pos_y < height - 1 )
    {
        uiLower = (sig_coeff_group_flag[ (uCG_pos_y  + 1 ) * width + uCG_pos_x ] != 0);
    }
    return (uiRight || uiLower);

}

static const Ipp32s ctxIndMap[16] =
{
    0, 1, 4, 5,
    2, 3, 4, 5,
    6, 6, 8, 8,
    7, 7, 8, 8
};

Ipp32s h265_quant_getSigCtxInc(Ipp32s pattern_sig_ctx,
                               Ipp32s scan_idx,
                               Ipp32s posX,
                               Ipp32s posY,
                               Ipp32s block_type,
                               Ipp32s width,
                               Ipp32s height,
                               EnumTextType type)
{


    if( posX + posY == 0 )
    {
        return 0;
    }

    if ( block_type == 2 )
    {
        return ctxIndMap[ 4 * posY + posX ];
    }

    Ipp32s offset = block_type == 3 ? (scan_idx == COEFF_SCAN_DIAG ? 9 : 15) : (type == TEXT_LUMA ? 21 : 12);

    Ipp32s posXinSubset = posX-((posX>>2)<<2);
    Ipp32s posYinSubset = posY-((posY>>2)<<2);
    Ipp32s cnt = 0;
    if(pattern_sig_ctx==0)
    {
        cnt = posXinSubset+posYinSubset<=2 ? (posXinSubset+posYinSubset==0 ? 2 : 1) : 0;
    }
    else if(pattern_sig_ctx==1)
    {
        cnt = posYinSubset<=1 ? (posYinSubset==0 ? 2 : 1) : 0;
    }
    else if(pattern_sig_ctx==2)
    {
        cnt = posXinSubset<=1 ? (posXinSubset==0 ? 2 : 1) : 0;
    }
    else
    {
        cnt = 2;
    }

    return (( type == TEXT_LUMA && ((posX>>2) + (posY>>2)) > 0 ) ? 3 : 0) + offset + cnt;
}

static const Ipp32s matrixIdTable[] = {0, 1, 2};

void H265CU::QuantInvTU(Ipp32u abs_part_idx, Ipp32s offset, Ipp32s width, Ipp32s is_luma)
{
    Ipp32s QP = is_luma ? par->QP : par->QPChroma;
    Ipp32s log2TrSize = h265_log2table[width - 4];

    VM_ASSERT(!par->csps->sps_scaling_list_data_present_flag);

    for (Ipp32s c_idx = 0; c_idx < (is_luma ? 1 : 2); c_idx ++) {
        CoeffsType *residuals = is_luma ? residuals_y : (c_idx ? residuals_v : residuals_u);
        CoeffsType *coeff = is_luma ? tr_coeff_y : (c_idx ? tr_coeff_v : tr_coeff_u);
        h265_quant_inv(coeff + offset, NULL, residuals + offset, log2TrSize, BIT_DEPTH_LUMA, QP);
    }
}

void H265CU::QuantFwdTU(
    Ipp32u abs_part_idx,
    Ipp32s offset,
    Ipp32s width,
    Ipp32s is_luma)
{
    Ipp32s QP = is_luma ? par->QP : par->QPChroma;
    Ipp32s log2TrSize = h265_log2table[width - 4];

    VM_ASSERT(!par->csps->sps_scaling_list_data_present_flag);

    for (Ipp32s c_idx = 0; c_idx < (is_luma ? 1 : 2); c_idx ++)
    {
        CoeffsType *residuals = is_luma ? residuals_y : (c_idx ? residuals_v : residuals_u);
        CoeffsType *coeff = is_luma ? tr_coeff_y : (c_idx ? tr_coeff_v : tr_coeff_u);
        Ipp32u abs_sum = 0;

        if(is_luma && IsRDOQ() )
        {
            h265_quant_fwd_rdo(
                this,
                residuals + offset,
                coeff + offset,
                log2TrSize,
                BIT_DEPTH_LUMA,
                par->cslice->slice_type == I_SLICE,
                abs_sum,
                TEXT_LUMA,
                abs_part_idx,
                QP,
                bsf);


        //}
        }
        else
        {
            Ipp32s delta_u[32*32];

            h265_quant_fwd_base(
                residuals + offset,
                coeff + offset,
                log2TrSize,
                BIT_DEPTH_LUMA,
                par->cslice->slice_type == I_SLICE,
                QP,

                this->par->cpps->sign_data_hiding_enabled_flag ? delta_u : NULL,
                abs_sum);

            if(this->par->cpps->sign_data_hiding_enabled_flag && abs_sum >= 2)
            {
                Ipp32u scan_idx = this->get_coef_scan_idx(abs_part_idx, width, is_luma ? true: false, this->isIntra(abs_part_idx) );

                if (scan_idx == COEFF_SCAN_ZIGZAG)
                {
                    scan_idx = COEFF_SCAN_DIAG;
                }

                Ipp32s height = width;
                const Ipp16u *scan = h265_sig_last_scan[ scan_idx -1 ][ log2TrSize - 1 ];

                 h265_sign_bit_hiding(
                     coeff + offset,
                     residuals + offset,
                     scan,
                     delta_u,
                     width,
                     height );
            }
        }
    }
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
