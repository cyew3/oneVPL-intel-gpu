/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_tables.h"

namespace UMC_HEVC_DECODER
{
// Scaling list initialization scan lookup table
const Ipp16u g_sigLastScanCG32x32[64] =
{
    0, 8, 1, 16, 9, 2, 24, 17,
    10, 3, 32, 25, 18, 11, 4, 40,
    33, 26, 19, 12, 5, 48, 41, 34,
    27, 20, 13, 6, 56, 49, 42, 35,
    28, 21, 14, 7, 57, 50, 43, 36,
    29, 22, 15, 58, 51, 44, 37, 30,
    23, 59, 52, 45, 38, 31, 60, 53,
    46, 39, 61, 54, 47, 62, 55, 63
};

// Scaling list initialization scan lookup table
const Ipp16u ScanTableDiag4x4[16] =
{
    0, 4, 1, 8,
    5, 2, 12, 9,
    6, 3, 13, 10,
    7, 14, 11, 15
};

// Default scaling list 4x4
const Ipp32s g_quantTSDefault4x4[16] =
{
  16,16,16,16,
  16,16,16,16,
  16,16,16,16,
  16,16,16,16
};

// Default scaling list 8x8 for intra prediction
const Ipp32s g_quantIntraDefault8x8[64] =
{
  16,16,16,16,17,18,21,24,
  16,16,16,16,17,19,22,25,
  16,16,17,18,20,22,25,29,
  16,16,18,21,24,27,31,36,
  17,17,20,24,30,35,41,47,
  18,19,22,27,35,44,54,65,
  21,22,25,31,41,54,70,88,
  24,25,29,36,47,65,88,115
};

// Default scaling list 8x8 for inter prediction
const Ipp32s g_quantInterDefault8x8[64] =
{
  16,16,16,16,17,18,20,24,
  16,16,16,17,18,20,24,25,
  16,16,17,18,20,24,25,28,
  16,17,18,20,24,25,28,33,
  17,18,20,24,25,28,33,41,
  18,20,24,25,28,33,41,54,
  20,24,25,28,33,41,54,71,
  24,25,28,33,41,54,71,91
};

// Inverse QP scale lookup table
const Ipp16u g_invQuantScales[6] =
{
    40,45,51,57,64,72
};

#ifndef MFX_VA
// Lookup table used for decoding coefficients and groups of coefficients
ALIGN_DECL(32) const Ipp8u scanGCZr[128] = {
     0,  0,  0,  0,  3,  1,  2,  0,  3,  2,  1,  0,  3,  1,  2,  0,
    15, 11, 14,  7, 10, 13,  3,  6,  9, 12,  2,  5,  8,  1,  4,  0,
    15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
    15, 11,  7,  3, 14, 10,  6,  2, 13,  9,  5,  1, 12,  8,  4,  0,
    63, 55, 62, 47, 54, 61, 39, 46, 53, 60, 31, 38, 45, 52, 59, 23,
    30, 37, 44, 51, 58, 15, 22, 29, 36, 43, 50, 57,  7, 14, 21, 28,
    35, 42, 49, 56,  6, 13, 20, 27, 34, 41, 48,  5, 12, 19, 26, 33,
    40,  4, 11, 18, 25, 32,  3, 10, 17, 24,  2,  9, 16,  1,  8,  0
};

// XY coefficient position inside of transform block lookup table
const Ipp32u g_MinInGroup[10] = { 0, 1, 2, 3, 4, 6, 8, 12, 16, 24 };

// Group index inside of transfrom block lookup table
const Ipp32u g_GroupIdx[32] = { 0, 1, 2, 3, 4, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9 };

// Prediction unit partition index increment inside of CU
const Ipp32u g_PUOffset[8] = { 0, 8, 4, 4, 2, 10, 1, 5};

// Luma to chroma QP scale lookup table. HEVC spec 8.6.1
const Ipp8u g_ChromaScale[2][58] =
{
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,29,30,31,32,33,33,34,34,35,35,36,36,37,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,51,51,51,51,51,51}
};

// chroma 422 pred mode from Luma IntraPredMode table 8-3 of spec
const Ipp8u g_Chroma422IntraPredModeC[INTRA_DM_CHROMA_IDX] =
//0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, DM
{ 0, 1, 2, 2, 2, 2, 3, 5, 7, 8, 10, 12, 13, 15, 17, 18, 19, 20, 21, 22, 23, 23, 24, 24, 25, 25, 26, 27, 27, 28, 28, 29, 29, 30, 31, INTRA_DM_CHROMA_IDX};


// Lookup table for converting number log2(number)-2
const Ipp8s  g_ConvertToBit[MAX_CU_SIZE + 1] =
{
    -1,
    -1, -1, -1,  0,
    -1, -1, -1,  1,
    -1, -1, -1, -1, -1, -1, -1,  2,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  3,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  4
};

#endif
} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER
