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

#ifndef H265_GLOBAL_ROM_H
#define H265_GLOBAL_ROM_H

#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_bitstream.h"

#include <stdio.h>
#include <iostream>

namespace UMC_HEVC_DECODER
{

enum
{
    NUM_INTRA_MODE = 36
};

// Prediction unit partition index increment inside of CU
extern Ipp32u g_PUOffset[8];

#define QUANT_IQUANT_SHIFT    20 // Q(QP%6) * IQ(QP%6) = 2^20
#define QUANT_SHIFT           14 // Q(4) = 2^14
#define SCALE_BITS            15 // Inherited from TMuC, pressumably for fractional bit estimates in RDOQ
#define MAX_TR_DYNAMIC_RANGE  15 // Maximum transform dynamic range (excluding sign bit)

#define SHIFT_INV_1ST          7 // Shift after first inverse transform stage
#define SHIFT_INV_2ND         12 // Shift after second inverse transform stage

// Inverse QP scale lookup table
extern Ipp16u g_invQuantScales[6];            // IQ(QP%6)

// Inverse transform 4x4 multiplication matrix
extern const Ipp16s g_T4[4][4];
// Inverse transform 8x8 multiplication matrix
extern const Ipp16s g_T8[8][8];
// Inverse transform 16x16 multiplication matrix
extern const Ipp16s g_T16[16][16];
// Inverse transform 32x32 multiplication matrix
extern const Ipp16s g_T32[32][32];

// Luma to chroma QP scale lookup table. HEVC spec 8.6.1
extern const Ipp8u g_ChromaScale[58];

// Lookup table used for decoding coefficients and groups of coefficients
extern ALIGN_DECL(32) const Ipp8u scanGCZr[128];
// XY coefficient position inside of transform block lookup table
extern const Ipp32u g_GroupIdx[32];
// Group index inside of transfrom block lookup table
extern const Ipp32u g_MinInGroup[10];

// clip x, such that 0 <= x <= maximum luma value
// ML: OPT: called in hot loops, compiler does not seem to always honor 'inline'
// ML: OPT: TODO: Make sure compiler recognizes saturation idiom for vectorization
template <typename T>
static T H265_FORCEINLINE ClipY(T Value, int c_bitDepth = 8)
{
    Value = (Value < 0) ? 0 : Value;
    const int c_Mask = ((1 << c_bitDepth) - 1);
    Value = (Value >= c_Mask) ? c_Mask : Value;
    return ( Value );
}

// clip x, such that 0 <= x <= maximum chroma value
template <typename T>
static T H265_FORCEINLINE ClipC(T Value, int c_bitDepth = 8)
{
    Value = (Value < 0) ? 0 : Value;
    const int c_Mask = ((1 << c_bitDepth) - 1);
    Value = (Value >= c_Mask) ? c_Mask : Value;
    return ( Value );
}

// Lookup table for converting number log2(number)-2
extern Ipp8s g_ConvertToBit[MAX_CU_SIZE + 1];   // from width to log2(width)-2

// Scaling list initialization scan lookup table
extern const Ipp16u g_sigLastScanCG32x32[64];
// Scaling list initialization scan lookup table
extern const Ipp16u ScanTableDiag4x4[16];
// Default scaling list 8x8 for intra prediction
extern Ipp32s g_quantIntraDefault8x8[64];
// Default scaling list 8x8 for inter prediction
extern Ipp32s g_quantInterDefault8x8[64];
// Default scaling list 4x4
extern Ipp32s g_quantTSDefault4x4[16];

// Scaling list table sizes
static const Ipp32u g_scalingListSize [4] = {16, 64, 256, 1024};
// Number of possible scaling lists of different sizes
static const Ipp32u g_scalingListNum[SCALING_LIST_SIZE_NUM]={6, 6, 6, 2};

} // end namespace UMC_HEVC_DECODER

#endif //h265_global_rom_h
#endif // UMC_ENABLE_H265_VIDEO_DECODER
