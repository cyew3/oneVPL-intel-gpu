//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_CABAC_TABLES_H__
#define __MFX_H265_CABAC_TABLES_H__

#include "mfx_h265_cabac.h"

namespace H265Enc {

extern const Ipp32s h265_cabac_p_bits[128];
extern const Ipp8u h265_cabac_transTbl[2][128];
extern const Ipp32u h265_ctxIdxSize[MAIN_SYNTAX_ELEMENT_NUMBER_HEVC];
extern const Ipp32u h265_ctxIdxOffset[MAIN_SYNTAX_ELEMENT_NUMBER_HEVC];
extern const Ipp8u h265_cabac_rangeTabLPS[128][4];

extern const Ipp32s h265_cabac_p_bits_AYA[128];

} // namespace

#endif // __MFX_H265_CABAC_TABLES_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
