//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_CABAC_TABLES_H__
#define __MFX_H265_CABAC_TABLES_H__

#include "mfx_h265_cabac.h"

namespace H265Enc {

extern const Ipp32s tab_cabacPBits[128];
extern const Ipp8u  tab_cabacTransTbl[2][128];
extern const Ipp32u tab_ctxIdxSize[MAIN_SYNTAX_ELEMENT_NUMBER_HEVC];
extern const Ipp32u tab_ctxIdxOffset[MAIN_SYNTAX_ELEMENT_NUMBER_HEVC];
extern const Ipp8u  tab_cabacRangeTabLps[128][4];

} // namespace

#endif // __MFX_H265_CABAC_TABLES_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
