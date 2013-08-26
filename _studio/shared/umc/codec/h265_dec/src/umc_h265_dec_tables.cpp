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

#include "umc_h265_dec_tables.h"

namespace UMC_HEVC_DECODER
{

//////////////////////////////////////////////////////////
// scan matrices, for Run Length Decoding

const
Ipp32s mp_scan4x4[2][16] =
{
    {
        0,  1,  4,  8,
        5,  2,  3,  6,
        9,  12, 13, 10,
        7,  11, 14, 15
    },
    {
        0,  4,  1,  8,
        12, 5,  9,  13,
        2,  6,  10, 14,
        3,  7,  11, 15
    }
};


const Ipp32s hp_scan8x8[2][64] =
{
    //8x8 zigzag scan
    {
        0, 1, 8,16, 9, 2, 3,10,
        17,24,32,25,18,11, 4, 5,
        12,19,26,33,40,48,41,34,
        27,20,13, 6, 7,14,21,28,
        35,42,49,56,57,50,43,36,
        29,22,15,23,30,37,44,51,
        58,59,52,45,38,31,39,46,
        53,60,61,54,47,55,62,63
    },
//8x8 field scan
    {
        0, 8,16, 1, 9,24,32,17,
        2,25,40,48,56,33,10, 3,
        18,41,49,57,26,11, 4,19,
        34,42,50,58,27,12, 5,20,
        35,43,51,59,28,13, 6,21,
        36,44,52,60,29,14,22,37,
        45,53,61,30, 7,15,38,46,
        54,62,23,31,39,47,55,63
    }
};

const Ipp16u SAspectRatio[17][2] =
{
    { 0,  0}, { 1,  1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, { 24, 11},
    {20, 11}, {32, 11}, {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99},
    {4,   3}, {3,   2}, {2,   1}
};

const Ipp32u SubWidthC[4]  = { 1, 2, 2, 1 };
const Ipp32u SubHeightC[4] = { 1, 2, 1, 1 };

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
#endif // UMC_ENABLE_H265_VIDEO_DECODER
