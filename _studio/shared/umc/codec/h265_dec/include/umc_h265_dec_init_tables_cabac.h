/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_DEC_INIT_TABLES_CABAC_H__
#define __UMC_H265_DEC_INIT_TABLES_CABAC_H__

#include "umc_h265_dec_defs_dec.h"

namespace UMC_HEVC_DECODER
{

extern const
Ipp8u rangeTabLPSH265[128][4];

extern const
Ipp8u transIdxMPSH265[128];

extern const
Ipp8u transIdxLPSH265[128];

extern const
Ipp32u NumBitsToGetTableSmall[4];

} // namespace UMC_HEVC_DECODER

namespace UMC
{
    // FIXME and remove dependency from UMC namespace
extern const
Ipp8u NumBitsToGetTbl[512];
}

#endif //__UMC_H264_DEC_INIT_TABLES_CABAC_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
