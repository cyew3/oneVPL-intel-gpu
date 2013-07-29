//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
#ifndef __MFX_H265_QUANT_OPT_H__
#define __MFX_H265_QUANT_OPT_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"

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

void h265_sign_bit_hiding(
    Ipp16s* levels,
    Ipp16s* coeffs,
    Ipp16u const *scan,
    Ipp32s* delta_u,
    Ipp32s width,
    Ipp32s height);

// RDO based quantization
void h265_quant_fwd_rdo(
    H265CU* pCU,
    Ipp16s* pSrc,
    Ipp16s* pDst,

    Ipp32s  log2_tr_size,

    Ipp32s  bit_depth,
    Ipp32s  is_slice_i,

    Ipp32u& abs_sum,
    EnumTextType   type,
    Ipp32u  abs_part_idx,

    Ipp32s  QP,
    H265BsFake* bs);

#endif // __MFX_H265_QUANT_OPT_H__
#endif // (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */
