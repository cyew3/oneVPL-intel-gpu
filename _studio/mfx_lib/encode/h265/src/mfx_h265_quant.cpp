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
#include "mfx_h265_quant.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_optimization.h"

namespace H265Enc {

static const Ipp32s ctxIndMap[16] =
{
    0, 1, 4, 5,
    2, 3, 4, 5,
    6, 6, 8, 8,
    7, 7, 8, 8
};

static const Ipp8u h265_qp_rem[90]=
{
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5
};

static const Ipp8u h265_qp6[90] =
{
    0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   2,   2,   2,  2,   2,   2,
    3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   5,   5,   5,  5,   5,   5,
    6,   6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   8,   8,   8,  8,   8,   8,
    9,   9,   9,   9,   9,   9,  10,  10,  10,  10,  10,  10,  11,  11,  11, 11,  11,  11,
   12,  12,  12,  12,  12,  12,  13,  13,  13,  13,  13,  13,  14,  14,  14, 14,  14,  14
};

static const Ipp8u h265_quant_table_inv[] =
{
    40, 45, 51, 57, 64, 72
};

static const Ipp16u h265_quant_table_fwd[] =
{
    26214, 23302, 20560, 18396, 16384, 14564
};

static const Ipp32s matrixIdTable[] = {0, 1, 2};


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


void H265CU::QuantInvTu(Ipp32u abs_part_idx, Ipp32s offset, Ipp32s width, Ipp32s is_luma)
{
    Ipp32s QP = is_luma ? m_par->QP : m_par->QPChroma;
    Ipp32s log2TrSize = h265_log2table[width - 4];

    VM_ASSERT(!m_par->csps->sps_scaling_list_data_present_flag);

    for (Ipp32s c_idx = 0; c_idx < (is_luma ? 1 : 2); c_idx ++) {
        CoeffsType *residuals = is_luma ? m_residualsY : (c_idx ? m_residualsV : m_residualsU);
        CoeffsType *coeff = is_luma ? m_trCoeffY : (c_idx ? m_trCoeffV : m_trCoeffU);
        h265_quant_inv(coeff + offset, NULL, residuals + offset, log2TrSize, BIT_DEPTH_LUMA, QP);
    }
}

void H265CU::QuantFwdTu(
    Ipp32u abs_part_idx,
    Ipp32s offset,
    Ipp32s width,
    Ipp32s is_luma)
{
    Ipp32s QP = is_luma ? m_par->QP : m_par->QPChroma;
    Ipp32s log2TrSize = h265_log2table[width - 4];

    VM_ASSERT(!m_par->csps->sps_scaling_list_data_present_flag);

    for (Ipp32s c_idx = 0; c_idx < (is_luma ? 1 : 2); c_idx ++) {
        CoeffsType *residuals = is_luma ? m_residualsY : (c_idx ? m_residualsV : m_residualsU);
        CoeffsType *coeff = is_luma ? m_trCoeffY : (c_idx ? m_trCoeffV : m_trCoeffU);
        Ipp32u abs_sum = 0;

        if ((is_luma || m_par->rdoqChromaFlag) && m_isRdoq) {
            h265_quant_fwd_rdo( this, residuals + offset, coeff + offset, log2TrSize,
                                BIT_DEPTH_LUMA, m_cslice->slice_type == I_SLICE, is_luma ? TEXT_LUMA : TEXT_CHROMA,
                                abs_part_idx, QP, m_bsf );
        }
        else {
            Ipp32s delta_u[32*32];
            h265_quant_fwd_base( residuals + offset, coeff + offset, log2TrSize, BIT_DEPTH_LUMA,
                                 m_cslice->slice_type == I_SLICE, QP,
                                 m_par->cpps->sign_data_hiding_enabled_flag ? delta_u : NULL,
                                 abs_sum );

            if(this->m_par->cpps->sign_data_hiding_enabled_flag && abs_sum >= 2) {
                Ipp32u scan_idx = GetCoefScanIdx( abs_part_idx, width, is_luma, IsIntra(abs_part_idx) );

                if (scan_idx == COEFF_SCAN_ZIGZAG)
                    scan_idx = COEFF_SCAN_DIAG;

                Ipp32s height = width;
                const Ipp16u *scan = h265_sig_last_scan[ scan_idx -1 ][ log2TrSize - 1 ];

                 h265_sign_bit_hiding( coeff + offset, residuals + offset, scan, delta_u,
                                       width, height );
            }
        }
    }
}


void h265_quant_inv(const CoeffsType *qcoeffs,
                    const Ipp16s *scaling_list, // aya: replaced 32s->16s to align with decoder/hevc_pp
                    CoeffsType *coeffs,
                    Ipp32s log2_tr_size,
                    Ipp32s bit_depth,
                    Ipp32s QP)
{
    Ipp32s qp_rem = h265_qp_rem[QP];
    Ipp32s qp6 = h265_qp6[QP];
    Ipp32s shift = bit_depth + log2_tr_size - 9;
    Ipp32s add;
    Ipp32s len = 1 << (log2_tr_size << 1);

    if (scaling_list)
    {
        shift += 4;
        if (shift > qp6)
        {
            add = 1 << (shift - qp6 - 1);
        }
        else
        {
            add = 0;
        }

//        const Ipp32s bitRange = MIN(15, ((Ipp32s)(12 + log2_tr_size + bit_depth - qp6)));
//        const Ipp32s levelLimit = 1 << bitRange;

        if (shift > qp6)
        {
            MFX_HEVC_PP::h265_QuantInv_ScaleList_RShift_16s(qcoeffs, scaling_list, coeffs, len, add, (shift -  qp6));
        }
        else
        {

            MFX_HEVC_PP::h265_QuantInv_ScaleList_LShift_16s(qcoeffs, scaling_list, coeffs, len, (qp6 - shift));
        }
    }
    else
    {
        add = 1 << (shift - 1);
        Ipp32s scale = h265_quant_table_inv[qp_rem] << qp6;

        MFX_HEVC_PP::h265_QuantInv_16s(qcoeffs, coeffs, len, scale, add, shift);
    }
}

void h265_quant_fwd_base(
    const CoeffsType *coeffs,
    CoeffsType *qcoeffs,
    Ipp32s log2_tr_size,
    Ipp32s bit_depth,
    Ipp32s is_slice_i,
    Ipp32s QP,

    Ipp32s*  delta,
    Ipp32u&  abs_sum )
{
    Ipp32s qp_rem = h265_qp_rem[QP];
    Ipp32s qp6 = h265_qp6[QP];
    Ipp32s len = 1 << (log2_tr_size << 1);
    //Ipp8s  sign;
    Ipp32s scaleLevel;
    Ipp32s scaleOffset;
    //Ipp32s qval;

    Ipp32s scale = 29 + qp6 - bit_depth - log2_tr_size;

    scaleLevel = h265_quant_table_fwd[qp_rem];
    scaleOffset = (is_slice_i ? 171 : 85) << (scale - 9);


    abs_sum = 0;

    if( delta )
    {
        abs_sum = (Ipp32u)MFX_HEVC_PP::NAME(h265_QuantFwd_SBH_16s)(coeffs, qcoeffs, delta, len, scaleLevel, scaleOffset, scale);
    }
    else
    {
        MFX_HEVC_PP::NAME(h265_QuantFwd_16s)(coeffs, qcoeffs, len, scaleLevel, scaleOffset, scale);
    }

} // void h265_quant_fwd_base(...)

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
