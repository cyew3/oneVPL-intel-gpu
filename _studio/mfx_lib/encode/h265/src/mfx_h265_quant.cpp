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

/* [pattern_sig_ctx][posY][posX] */
static const Ipp8u cntTab[4*4*4] = {
    2, 1, 1, 0,
    1, 1, 0, 0,
    1, 0, 0, 0,
    0, 0, 0, 0,

    2, 2, 2, 2,
    1, 1, 1, 1,
    0, 0, 0, 0,
    0, 0, 0, 0,

    2, 1, 0, 0,
    2, 1, 0, 0,
    2, 1, 0, 0,
    2, 1, 0, 0,

    2, 2, 2, 2,
    2, 2, 2, 2,
    2, 2, 2, 2,
    2, 2, 2, 2,
};

/* [type][scan_idx][block_type] */
static const Ipp8u offTab[3*4*8] = {
    /* luma */
    21, 21, 21, 15, 21, 21, 21, 0,
    21, 21, 21, 15, 21, 21, 21, 0,
    21, 21, 21, 15, 21, 21, 21, 0,
    21, 21, 21,  9, 21, 21, 21, 0,

    /* chroma */
    12, 12, 12, 15, 12, 12, 12, 0,
    12, 12, 12, 15, 12, 12, 12, 0,
    12, 12, 12, 15, 12, 12, 12, 0,
    12, 12, 12,  9, 12, 12, 12, 0,

    /* luma with offset - roll in extra +3 */
    24, 24, 24, 18, 24, 24, 24, 0,
    24, 24, 24, 18, 24, 24, 24, 0,
    24, 24, 24, 18, 24, 24, 24, 0,
    24, 24, 24, 12, 24, 24, 24, 0,
};

const Ipp8u h265_qp_rem[90]=
{
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5
};

const Ipp8u h265_qp6[90] =
{
    0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   2,   2,   2,  2,   2,   2,
    3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   5,   5,   5,  5,   5,   5,
    6,   6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   8,   8,   8,  8,   8,   8,
    9,   9,   9,   9,   9,   9,  10,  10,  10,  10,  10,  10,  11,  11,  11, 11,  11,  11,
   12,  12,  12,  12,  12,  12,  13,  13,  13,  13,  13,  13,  14,  14,  14, 14,  14,  14
};

const Ipp8u h265_quant_table_inv[] =
{
    40, 45, 51, 57, 64, 72
};

const Ipp16u h265_quant_table_fwd[] =
{
    26214, 23302, 20560, 18396, 16384, 14564
};

const Ipp64f h265_quant_table_fwd_pow_minus2[] = // 1.0 / (h265_quant_table_fwd[x] * h265_quant_table_fwd[x])
{
    1.4552359327741304e-09, 1.8416775926645421e-09, 2.3656679132159456e-09, 2.954970830655539e-09, 3.725290298461914e-09, 4.7145327773553975e-09
};


static const Ipp32s matrixIdTable[] = {0, 1, 2};


Ipp32s h265_quant_calcpattern_sig_ctx(const Ipp8u *sig_coeff_group_flag, Ipp32u posXCG, Ipp32u posYCG, Ipp32u width)
{
    if( width == 4 ) return -1;

    Ipp32u sig_right = 0;
    Ipp32u sig_lower = 0;

    width >>= 2;
    if( posXCG < width - 1 )
    {
        sig_right = sig_coeff_group_flag[ posYCG * width + posXCG + 1 ];
    }
    if (posYCG < width - 1 )
    {
        sig_lower = sig_coeff_group_flag[ (posYCG  + 1 ) * width + posXCG ];
    }
    return sig_right + (sig_lower<<1);
}

Ipp32u h265_quant_getSigCoeffGroupCtxInc ( const Ipp8u *sig_coeff_group_flag,
                                           const Ipp32u uCG_pos_x,
                                           const Ipp32u uCG_pos_y,
                                           const Ipp32u scanIdx,
                                           Ipp32u       width)
{
    Ipp32u uiRight = 0;
    Ipp32u uiLower = 0;

    width >>= 2;
    if( uCG_pos_x < width - 1 )
    {
        uiRight = sig_coeff_group_flag[ uCG_pos_y * width + uCG_pos_x + 1 ];
    }
    if (uCG_pos_y < width - 1 )
    {
        uiLower = sig_coeff_group_flag[ (uCG_pos_y  + 1 ) * width + uCG_pos_x ];
    }
    return (uiRight | uiLower);

}


/* return context for significant coeff flag - fast lookup table implementation
 * supported ranges of input parameters:
 *   pattern_sig_ctx = [0,3]
 *   scan_idx = [0,3]
 *   posX,posY = [0,31] (position in transform block)
 *   block_type = [1,5] (log2_block_size for 2x2 to 32x32) 
 *   width, height unused (can be removed)
 *   type = 0 (luma) or 1 (chroma)
 */
Ipp32s h265_quant_getSigCtxInc(Ipp32s pattern_sig_ctx,
                               Ipp32s scan_idx,
                               Ipp32s posX,
                               Ipp32s posY,
                               Ipp32s block_type,
                               EnumTextType type)
{
    Ipp32s ret, typeOff, typeNew;

    /* special case - (0,0) */
    if ((posX | posY) == 0)
        return 0;

    /* special case - 4x4 block */
    if (block_type == 2)
       return ctxIndMap[4*posY + posX];

    /* should compile into cmov - verify no branches */
    typeOff  = ((posX | posY) >> 2) ? 2 : 0;
    typeOff &= (type - 1);
    typeNew  = (Ipp32s)type + typeOff;

    /* should compile into successive shifts/adds: ((x << 2) + y) << 3... */
    posX &= 0x03;
    posY &= 0x03;
    ret  = offTab[8*(4*typeNew + scan_idx) + block_type];   /* offset */
    ret += cntTab[4*(4*pattern_sig_ctx + posY) + posX];     /* count */

    return ret;
}

template <typename PixType>
void H265CU<PixType>::QuantInvTu(const CoeffsType *coeff, CoeffsType *resid, Ipp32s width, Ipp32s is_luma)
{
    Ipp32s QP = (is_luma) ? m_lumaQp : m_chromaQp;
    Ipp32s log2TrSize = h265_log2m2[width] + 2;
    Ipp32s bitDepth = is_luma ? m_par->bitDepthLuma : m_par->bitDepthChroma;
    h265_quant_inv(coeff, NULL, resid, log2TrSize, bitDepth, QP);
}


template <class T>
bool IsZero(const T *arr, Ipp32s size)
{
    assert(reinterpret_cast<Ipp64u>(arr) % sizeof(Ipp64u) == 0);
    assert(size * sizeof(T) % sizeof(Ipp64u) == 0);

    const Ipp64u *arr64 = reinterpret_cast<const Ipp64u *>(arr);
    const Ipp64u *arr64end = reinterpret_cast<const Ipp64u *>(arr + size);

    for (; arr64 != arr64end; arr64++)
        if (*arr64)
            return false;
    return true;
}

template <typename PixType>
Ipp8u H265CU<PixType>::QuantFwdTu(CoeffsType *resid, CoeffsType *coeff, Ipp32s absPartIdx, Ipp32s width,
                                  Ipp32s isLuma, Ipp32s isIntra)
{
    Ipp32s isIntraSlice = (m_cslice->slice_type == I_SLICE);
    Ipp32s bitDepth = (isLuma) ? m_par->bitDepthLuma : m_par->bitDepthChroma;
    Ipp32s QP = (isLuma) ? m_lumaQp : m_chromaQp;
    Ipp32s log2TrSize = h265_log2m2[width] + 2;

    if (!((isLuma || m_par->rdoqChromaFlag) && m_isRdoq)) {
        Ipp32s qp_rem = h265_qp_rem[QP];
        Ipp32s qp6 = h265_qp6[QP];
        Ipp32s len = 1 << (log2TrSize << 1);
        Ipp32s scale = 29 + qp6 - bitDepth - log2TrSize;
        Ipp32s scaleLevel = h265_quant_table_fwd[qp_rem];
        Ipp32s scaleOffset = (isIntraSlice ? 171 : 85) << (scale - 9);

        if (!m_par->SBHFlag) {
            return MFX_HEVC_PP::NAME(h265_QuantFwd_16s)(resid, coeff, len, scaleLevel, scaleOffset, scale);
        } else {
            Ipp32s *delta_u = m_scratchPad.quantFwdTuSbh.delta_u;
            assert(sizeof(m_scratchPad) >= sizeof(*delta_u) * 32 * 32);
            assert((size_t(delta_u) & 63) == 0);
            Ipp32u abs_sum = (Ipp32u)MFX_HEVC_PP::NAME(h265_QuantFwd_SBH_16s)(resid, coeff, delta_u, len, scaleLevel, scaleOffset, scale);

            if (abs_sum >= 2) {
                Ipp32u scan_idx = GetCoefScanIdx(absPartIdx, log2TrSize, isLuma, IsIntra(absPartIdx));
                if (scan_idx == COEFF_SCAN_ZIGZAG)
                    scan_idx = COEFF_SCAN_DIAG;

                const Ipp16u *scan = h265_sig_last_scan[scan_idx - 1][log2TrSize - 1];
                h265_sign_bit_hiding(coeff, resid, scan, delta_u, width);
            }
            return !IsZero(coeff, width*width);
        }
    } else {
        EnumTextType textureType = (isLuma) ? TEXT_LUMA : TEXT_CHROMA;
        h265_quant_fwd_rdo<PixType>(this, resid, coeff, log2TrSize, bitDepth, isIntraSlice, isIntra,
                                    textureType, absPartIdx, QP, m_bsf);
        return !IsZero(coeff, width*width);
    }
}

template <typename PixType>
Ipp8u H265CU<PixType>::QuantFwdTuBase(CoeffsType *resid, CoeffsType *coeff, Ipp32s absPartIdx, Ipp32s width, Ipp32s isLuma)
{
    Ipp32s QP = (isLuma) ? m_lumaQp : m_chromaQp;
    Ipp32s log2TrSize = h265_log2m2[width] + 2;
    Ipp32s isIntraSlice = (m_cslice->slice_type == I_SLICE);
    Ipp32s bitDepth = (isLuma) ? m_par->bitDepthLuma : m_par->bitDepthChroma;

    Ipp32s qp_rem = h265_qp_rem[QP];
    Ipp32s qp6 = h265_qp6[QP];
    Ipp32s len = width * width;
    Ipp32s scale = 29 + qp6 - bitDepth - log2TrSize;
    Ipp32s scaleLevel = h265_quant_table_fwd[qp_rem];
    Ipp32s scaleOffset = (isIntraSlice ? 171 : 85) << (scale - 9);

    return MFX_HEVC_PP::NAME(h265_QuantFwd_16s)(resid, coeff, len, scaleLevel, scaleOffset, scale);
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

        MFX_HEVC_PP::NAME(h265_QuantInv_16s)(qcoeffs, coeffs, len, scale, add, shift);
    }
}


template class H265CU<Ipp8u>;
template class H265CU<Ipp16u>;

static const Ipp8u h265_QPtoChromaQP[3][58] = {
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 29, 30, 31, 32,
        33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 39, 40, 41, 42, 43, 44,
        45, 46, 47, 48, 49, 50, 51
    }, {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
        51, 51, 51, 51, 51, 51, 51
    }, {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
        51, 51, 51, 51, 51, 51, 51
    }
};
Ipp8s GetChromaQP(Ipp8s qpy, Ipp32s chromaQpOffset, Ipp8u chromaFormatIdc, Ipp8u bitDepthChroma)
{
    Ipp32s qpBdOffsetC = 6 * (bitDepthChroma - 8);
    Ipp32s qpc = Saturate(-qpBdOffsetC, 57, qpy + chromaQpOffset);
    if (qpc >= 0)
        qpc = h265_QPtoChromaQP[chromaFormatIdc - 1][qpc];
    return (Ipp8s)(qpc + qpBdOffsetC);
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
