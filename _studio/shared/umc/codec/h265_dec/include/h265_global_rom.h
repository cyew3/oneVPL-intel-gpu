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

#ifndef H265_GLOBAL_ROM_H
#define H265_GLOBAL_ROM_H

#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_bitstream.h"

#include <stdio.h>
#include <iostream>

namespace UMC_HEVC_DECODER
{

//tables and variables ------------------------------------------------------------------------------------------
// flexible conversion from relative to absolute index ----------------------------------------------------------
extern Ipp32u g_ZscanToRaster[MAX_NUM_SPU_W * MAX_NUM_SPU_W];
extern Ipp32u g_RasterToZscan[MAX_NUM_SPU_W * MAX_NUM_SPU_W];

void initZscanToRaster (Ipp32s MaxDepth, Ipp32s Depth, Ipp32u StartVal, Ipp32u*& CurrIdx);
void initRasterToZscan (Ipp32u MaxCUWidth, Ipp32u MaxCUHeight, Ipp32u MaxDepth);

// conversion of partition index to picture pel position ---------------------------------------------------------
extern Ipp32u g_RasterToPelX[MAX_NUM_SPU_W * MAX_NUM_SPU_W];
extern Ipp32u g_RasterToPelY[MAX_NUM_SPU_W * MAX_NUM_SPU_W];

void initRasterToPelXY(Ipp32u MaxCUWidth, Ipp32u MaxCUHeight, Ipp32u MaxDepth);

// global variables (LCU width/height, max. CU depth) ------------------------------------------------------------

extern Ipp32u g_PUOffset[8];

#define QUANT_IQUANT_SHIFT    20 // Q(QP%6) * IQ(QP%6) = 2^20
#define QUANT_SHIFT           14 // Q(4) = 2^14
#define SCALE_BITS            15 // Inherited from TMuC, pressumably for fractional bit estimates in RDOQ
#define MAX_TR_DYNAMIC_RANGE  15 // Maximum transform dynamic range (excluding sign bit)

#define SHIFT_INV_1ST          7 // Shift after first inverse transform stage
#define SHIFT_INV_2ND         12 // Shift after second inverse transform stage

// ML: OPT: 16-bit to allow the use of PMULLW/PMULHW instead of PMULLD
extern Ipp16u g_quantScales[6];             // Q(QP%6)
extern Ipp16u g_invQuantScales[6];            // IQ(QP%6)

extern const Ipp16s g_T4[4][4];
extern const Ipp16s g_T8[8][8];
extern const Ipp16s g_T16[16][16];
extern const Ipp16s g_T32[32][32];

// Luma QP to chroma QP mapping -------------------------------------------------------------------------------
extern const Ipp8u g_ChromaScale[58];

// Scanning order & context mapping table ---------------------------------------------------------------------

extern const Ipp16u* g_SigLastScan[3][4];  // raster index from scanning index (zigzag, hor, ver, diag)

extern const Ipp32u g_GroupIdx[32];
extern const Ipp32u g_MinInGroup[10];

extern const Ipp32u g_GoRiceRange[5];                  // maximum value coded with Rice codes
extern const Ipp32u g_GoRicePrefixLen[5];              // prefix length for each maximum value

extern const Ipp16u g_sigLastScan8x8[3][4];           //!< coefficient group scan order for 8x8 TUs
extern const Ipp16u g_sigLastScanCG32x32[64];

// ADI table ----------------------------------------------------------------------------------------------------------
extern const Ipp8u g_IntraModeNumFast[7];

// Angular Intra table ------------------------------------------------------------------------------------------------
extern const Ipp8u g_IntraModeNumAng[7];
extern const Ipp8u g_IntraModeBitsAng[7];
extern const Ipp8u g_AngModeMapping[4][34];
extern const Ipp8u g_AngIntraModeOrder[NUM_INTRA_MODE];

// Bit-depth ----------------------------------------------------------------------------------------------------------
// ML: OPT: Replacing with compile time const
const Ipp32s g_bitDepthY = 8;
const Ipp32s g_bitDepthC = 8;

/** clip x, such that 0 <= x <= #g_maxLumaVal */
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

template <typename T> 
static T H265_FORCEINLINE ClipC(T Value, int c_bitDepth = 8) 
{ 
    Value = (Value < 0) ? 0 : Value;
    const int c_Mask = ((1 << c_bitDepth) - 1);
    Value = (Value >= c_Mask) ? c_Mask : Value;
    return ( Value );
}

// Texture type to integer mapping ------------------------------------------------------------------------------------
static const Ipp8u g_ConvertTxtTypeToIdx[4] = { 0, 1, 1, 2 }; // resolved at compile time to avoid tablelookup

// Mode-Dependent DST Matrices ----------------------------------------------------------------------------------------
extern const Ipp8u g_DCTDSTMode_Vert[NUM_INTRA_MODE];
extern const Ipp8u g_DCTDSTMode_Hor[NUM_INTRA_MODE];

extern Ipp8s g_ConvertToBit[MAX_CU_SIZE + 1];   // from width to log2(width)-2

extern Ipp32s g_quantIntraDefault8x8[64];
extern Ipp32s g_quantInterDefault8x8[64];
extern Ipp32s g_quantTSDefault4x4[16];

static const Ipp32u g_Table[4] = {0, 3, 1, 2};
static const Ipp32u g_scalingListSize [4] = {16, 64, 256, 1024};
static const Ipp32u g_scalingListSizeX[4] = { 4,  8,  16,   32};
static const Ipp32u g_scalingListNum[SCALING_LIST_SIZE_NUM]={6, 6, 6, 2};

} // end namespace UMC_HEVC_DECODER

#endif //h265_global_rom_h
#endif // UMC_ENABLE_H264_VIDEO_DECODER
