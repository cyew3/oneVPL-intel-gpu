//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_QUANT_H__
#define __MFX_H265_QUANT_H__

#include "mfx_h265_defs.h"

Ipp32s h265_quant_calcpattern_sig_ctx(
    const Ipp32u* sig_coeff_group_flag,
    Ipp32u posXCG,
    Ipp32u posYCG,
    Ipp32u width,
    Ipp32u height );

Ipp32u h265_quant_getSigCoeffGroupCtxInc  (
    const Ipp32u*   sig_coeff_group_flag,
    const Ipp32u    uCG_pos_x,
    const Ipp32u    uCG_pos_y,
    const Ipp32u    scan_idx,
    Ipp32u          width,
    Ipp32u          height);

Ipp32s h265_quant_getSigCtxInc(
    Ipp32s pattern_sig_ctx,
    Ipp32s scan_idx,
    Ipp32s pos_x,
    Ipp32s pos_y,
    Ipp32s block_type,
    Ipp32s width,
    Ipp32s height,
    EnumTextType type);

#endif // __MFX_H265_QUANT_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE

