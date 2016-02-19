//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_QUANT_H__
#define __MFX_H265_QUANT_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_bitstream.h"
#include "mfx_h265_ctb.h"

namespace H265Enc {

extern const Ipp8u h265_qp_rem[];
extern const Ipp8u h265_qp6[];
extern const Ipp8u h265_quant_table_inv[];
extern const Ipp16u h265_quant_table_fwd[];

Ipp32s h265_quant_calcpattern_sig_ctx(
    const Ipp32u* sig_coeff_group_flag,
    Ipp32u posXCG,
    Ipp32u posYCG,
    Ipp32u width );

Ipp32u h265_quant_getSigCoeffGroupCtxInc  (
    const Ipp32u*   sig_coeff_group_flag,
    const Ipp32u    uCG_pos_x,
    const Ipp32u    uCG_pos_y,
    const Ipp32u    scan_idx,
    Ipp32u          width);

Ipp32s h265_quant_getSigCtxInc(
    Ipp32s pattern_sig_ctx,
    Ipp32s scan_idx,
    Ipp32s pos_x,
    Ipp32s pos_y,
    Ipp32s block_type,
    EnumTextType type);

void h265_quant_inv(
    const CoeffsType *qcoeffs,
    const Ipp16s *scaling_list,
    CoeffsType *coeffs,
    Ipp32s log2_tr_size,
    Ipp32s bit_depth,
    Ipp32s QP);

void h265_quant_fwd_base(
    const CoeffsType *coeffs,
    CoeffsType *qcoeffs,
    Ipp32s log2_tr_size,
    Ipp32s bit_depth,
    Ipp32s is_slice_i,
    Ipp32s QP,
    Ipp32s* delta,
    Ipp32u& abs_sum );

// RDO based quantization
template <typename PixType>
void h265_quant_fwd_rdo(
    H265CU<PixType>* pCU,
    Ipp16s* pSrc,
    Ipp16s* pDst,

    Ipp32s  log2_tr_size,

    Ipp32s  bit_depth,
    Ipp32s  is_slice_i,
#ifdef AMT_DZ_RDOQ
    Ipp32s  is_blk_i,
#endif

    EnumTextType   type,
    Ipp32u  abs_part_idx,

    Ipp32s  QP,
    H265BsFake* bs);

void h265_sign_bit_hiding(
    Ipp16s* levels,
    Ipp16s* coeffs,
    Ipp16u const *scan,
    Ipp32s* delta_u,
    Ipp32s width);

} // namespace

#endif // __MFX_H265_QUANT_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE

