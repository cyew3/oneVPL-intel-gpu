/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
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

// Sample aspect ratios by aspect_ratio_idc index. HEVC spec E.3.1
const Ipp16u SAspectRatio[17][2] =
{
    { 0,  0}, { 1,  1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, { 24, 11},
    {20, 11}, {32, 11}, {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99},
    {4,   3}, {3,   2}, {2,   1}
};

// Convert chroma samples width to luma samples with chroma_format_idc index
const Ipp32u SubWidthC[4]  = { 1, 2, 2, 1 };
// Convert chroma samples height to luma samples with chroma_format_idc index
const Ipp32u SubHeightC[4] = { 1, 2, 1, 1 };

// Bit masks for fast extraction of bits from bitstream
const Ipp32u bits_data[] =
{
    (((Ipp32u)0x01 << (0)) - 1),
    (((Ipp32u)0x01 << (1)) - 1),
    (((Ipp32u)0x01 << (2)) - 1),
    (((Ipp32u)0x01 << (3)) - 1),
    (((Ipp32u)0x01 << (4)) - 1),
    (((Ipp32u)0x01 << (5)) - 1),
    (((Ipp32u)0x01 << (6)) - 1),
    (((Ipp32u)0x01 << (7)) - 1),
    (((Ipp32u)0x01 << (8)) - 1),
    (((Ipp32u)0x01 << (9)) - 1),
    (((Ipp32u)0x01 << (10)) - 1),
    (((Ipp32u)0x01 << (11)) - 1),
    (((Ipp32u)0x01 << (12)) - 1),
    (((Ipp32u)0x01 << (13)) - 1),
    (((Ipp32u)0x01 << (14)) - 1),
    (((Ipp32u)0x01 << (15)) - 1),
    (((Ipp32u)0x01 << (16)) - 1),
    (((Ipp32u)0x01 << (17)) - 1),
    (((Ipp32u)0x01 << (18)) - 1),
    (((Ipp32u)0x01 << (19)) - 1),
    (((Ipp32u)0x01 << (20)) - 1),
    (((Ipp32u)0x01 << (21)) - 1),
    (((Ipp32u)0x01 << (22)) - 1),
    (((Ipp32u)0x01 << (23)) - 1),
    (((Ipp32u)0x01 << (24)) - 1),
    (((Ipp32u)0x01 << (25)) - 1),
    (((Ipp32u)0x01 << (26)) - 1),
    (((Ipp32u)0x01 << (27)) - 1),
    (((Ipp32u)0x01 << (28)) - 1),
    (((Ipp32u)0x01 << (29)) - 1),
    (((Ipp32u)0x01 << (30)) - 1),
    (((Ipp32u)0x01 << (31)) - 1),
    ((Ipp32u)0xFFFFFFFF),
};

} // namespace UMC_HEVC_DECODER

#endif //__UMC_H265_DEC_TABLES_H__
#endif // UMC_ENABLE_H265_VIDEO_DECODER
