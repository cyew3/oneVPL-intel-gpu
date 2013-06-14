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

#ifndef __UMC_H265_DEC_TABLES_H__
#define __UMC_H265_DEC_TABLES_H__

#include "ippdefs.h"

namespace UMC_HEVC_DECODER
{
//////////////////////////////////////////////////////////
// scan matrices
extern const Ipp32s mp_scan4x4[2][16];
extern const Ipp32s hp_scan8x8[2][64];

extern const Ipp16u SAspectRatio[17][2];

extern const Ipp32u SubWidthC[4];
extern const Ipp32u SubHeightC[4];

extern const Ipp32u bits_data[];
} // namespace UMC_HEVC_DECODER

#endif //__UMC_H264_DEC_TABLES_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
