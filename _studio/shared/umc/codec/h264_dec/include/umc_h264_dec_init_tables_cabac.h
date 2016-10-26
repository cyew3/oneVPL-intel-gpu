//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_INIT_TABLES_CABAC_H__
#define __UMC_H264_DEC_INIT_TABLES_CABAC_H__

#include "umc_h264_dec_defs_dec.h"

namespace UMC
{

extern const
Ipp8u rangeTabLPS[128][4];

extern const
Ipp8u transIdxMPS[128];

extern const
Ipp8u transIdxLPS[128];

extern const
Ipp32u NumBitsToGetTableSmall[4];

extern const
Ipp8u NumBitsToGetTbl[512];

} // namespace UMC

#endif //__UMC_H264_DEC_INIT_TABLES_CABAC_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
