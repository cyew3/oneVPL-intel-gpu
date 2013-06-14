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

void initNonSquareSigLastScan(Ipp32u* pBuffZ, Ipp32s Width, Ipp32s Height);

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
extern Ipp32u g_MaxCUWidth;
extern Ipp32u g_MaxCUHeight;
extern Ipp32u g_MaxCUDepth;
extern Ipp32u g_AddCUDepth;

#define MAX_TS_WIDTH 4
#define MAX_TS_HEIGHT 4

extern Ipp32u g_PUOffset[8];

#define QUANT_IQUANT_SHIFT    20 // Q(QP%6) * IQ(QP%6) = 2^20
#define QUANT_SHIFT           14 // Q(4) = 2^14
#define SCALE_BITS            15 // Inherited from TMuC, pressumably for fractional bit estimates in RDOQ
#define MAX_TR_DYNAMIC_RANGE  15 // Maximum transform dynamic range (excluding sign bit)

#define SHIFT_INV_1ST          7 // Shift after first inverse transform stage
#define SHIFT_INV_2ND         12 // Shift after second inverse transform stage

#if (HEVC_OPT_CHANGES & 64)
// ML: OPT: 16-bit to allow the use of PMULLW/PMULHW instead of PMULLD
extern Ipp16u g_quantScales[6];             // Q(QP%6)
extern Ipp16u g_invQuantScales[6];            // IQ(QP%6)
#else
extern Ipp32u g_quantScales[6];             // Q(QP%6)
extern Ipp32u g_invQuantScales[6];            // IQ(QP%6)
#endif
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
#if (HEVC_OPT_CHANGES & 16)
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
#else
extern Ipp32s g_bitDepthY;
extern Ipp32s g_bitDepthC;

template <typename T> inline T ClipY(T x) { return std::min<T>(T((1 << g_bitDepthY) - 1), std::max<T>( T(0), x)); }
template <typename T> inline T ClipC(T x) { return std::min<T>(T((1 << g_bitDepthC) - 1), std::max<T>( T(0), x)); }
#endif // HEVC_OPT_CHANGES

extern Ipp32u g_PCMBitDepthLuma;
extern Ipp32u g_PCMBitDepthChroma;

// Texture type to integer mapping ------------------------------------------------------------------------------------
extern const Ipp8u g_ConvertTxtTypeToIdx[4];

// Mode-Dependent DST Matrices ----------------------------------------------------------------------------------------
extern const Ipp8u g_DCTDSTMode_Vert[NUM_INTRA_MODE];
extern const Ipp8u g_DCTDSTMode_Hor[NUM_INTRA_MODE];

// Misc. --------------------------------------------------------------------------------------------------------------
__inline Ipp32u xRunLevelInd(Ipp32s lev, Ipp32u run, Ipp32u maxrun, Ipp32u lrg1Pos)
{
    Ipp32u cn;

    if (lrg1Pos > 0)
    {
        if (lev == 0)
        {
            if (run < lrg1Pos)
                cn = run;
            else
                cn = (run << 1) - lrg1Pos + 1;
        }
        else
        {
            if ( run > (maxrun - (Ipp32s)lrg1Pos + 1) )
                cn = maxrun + run + 2 ;
            else
                cn = lrg1Pos + (run << 1);
        }
    }
    else
    {
        cn = (run << 1);
        if ( lev == 0 && run <= maxrun )
        {
            cn++;
        }
    }
    return(cn);
}

/** Function for deriving codeword index in CAVLC run-level coding
 * \param lev a value indicating coefficient level greater than one or not
 * \param run length of run
 * \param maxrun maximum length of run for a given coefficient location
 * \returns the codeword index
 * This function derives codeword index in CAVLC run-level coding .
 */
__inline Ipp32u xRunLevelIndInter(Ipp32s lev, Ipp32s run, Ipp32s maxrun, Ipp32s scale)
{
    Ipp32u cn;

    {
        if (lev == 0)
        {
            cn = run;
            {
                Ipp32s thr = (maxrun + 1) >> scale;
                if (run >= thr)
                cn = (run > maxrun) ? thr : (run + 1);
            }

        }
        else
        {
            cn = maxrun + run + 2;
        }
    }

    return(cn);
}

__inline Ipp32u xLeadingZeros(Ipp32u uiCode)
{
    Ipp32u uiCount = 0;
    Ipp32s iDone = 0;

    if (uiCode)
    {
        while (!iDone)
        {
            uiCode >>= 1;
            if (!uiCode) iDone = 1;
            else uiCount++;
        }
    }
    return uiCount;
}

extern Ipp8s g_ConvertToBit[MAX_CU_SIZE + 1];   // from width to log2(width)-2

/** Function for deriving codeword index in coding last significant position and level information.
 * \param lev a value indicating coefficient level greater than one or not
 * \param last_pos last significant coefficient position
 * \param N block size
 * \returns the codeword index
 * This function derives codeword index in coding last significant position and level information in CAVLC.
 */
__inline Ipp32u xLastLevelInd(Ipp32s lev, Ipp32s last_pos, Ipp32s N)
{
    Ipp32u cx;
    Ipp32u uiConvBit = g_ConvertToBit[N]+2;

    if (lev==0)
    {
        cx = ((last_pos + (last_pos>>uiConvBit))>>uiConvBit)+last_pos;
    }
    else
    {
        if (last_pos<N)
        {
            cx = (last_pos+1)<<uiConvBit;
        }
        else
        {
            cx = (0x01<<(uiConvBit<<1)) + last_pos;
        }
    }
    return(cx);
}

__inline void xLastLevelIndInv(Ipp32s& lev, Ipp32s& last_pos, Ipp32u N, Ipp32u cx)
{
    Ipp32u uiConvBit = g_ConvertToBit[N]+2;
    Ipp32u N2 = 0x01<<(uiConvBit<<1);

    if(cx <= N2+N)
    {
        if(cx && (cx&(N-1))==0)
        {
            lev = 1;
            last_pos = (cx>>uiConvBit)-1;
        }
        else
        {
            lev = 0;
            last_pos = cx - (cx>>uiConvBit);
        }
    }
    else
    {
        lev = 1;
        last_pos = cx - N2;
    }
}

#define ENC_DEC_TRACE 0


#if ENC_DEC_TRACE
extern FILE* g_Trace;
extern bool g_JustDoIt;
extern const bool g_EncDecTraceEnable;
extern const bool g_EncDecTraceDisable;
extern Ipp64u g_SymbolCounter;

#define COUNTER_START 1
#define COUNTER_END 0 //( Ipp64u(1) << 63 )

#define DTRACE_CABAC_F(x) if ( ( g_SymbolCounter >= COUNTER_START && g_SymbolCounter <= COUNTER_END )|| g_JustDoIt ) fprintf( g_Trace, "%f", x );
#define DTRACE_CABAC_V(x) if ( ( g_SymbolCounter >= COUNTER_START && g_SymbolCounter <= COUNTER_END )|| g_JustDoIt ) fprintf( g_Trace, "%d", x );
#define DTRACE_CABAC_VL(x) if ( ( g_SymbolCounter >= COUNTER_START && g_SymbolCounter <= COUNTER_END )|| g_JustDoIt ) fprintf( g_Trace, "%lld", x );
#define DTRACE_CABAC_T(x) if ( ( g_SymbolCounter >= COUNTER_START && g_SymbolCounter <= COUNTER_END )|| g_JustDoIt ) fprintf( g_Trace, "%s", x );
#define DTRACE_CABAC_X(x) if ( ( g_SymbolCounter >= COUNTER_START && g_SymbolCounter <= COUNTER_END )|| g_JustDoIt ) fprintf( g_Trace, "%x", x );
#define DTRACE_CABAC_R( x,y ) if ( ( g_SymbolCounter >= COUNTER_START && g_SymbolCounter <= COUNTER_END )|| g_JustDoIt ) fprintf( g_Trace, x,    y );
#define DTRACE_CABAC_N if ( ( g_SymbolCounter >= COUNTER_START && g_SymbolCounter <= COUNTER_END )|| g_JustDoIt ) fprintf( g_Trace, "\n"    );

#else

#define DTRACE_CABAC_F(x)
#define DTRACE_CABAC_V(x)
#define DTRACE_CABAC_VL(x)
#define DTRACE_CABAC_T(x)
#define DTRACE_CABAC_X(x)
#define DTRACE_CABAC_R( x,y )
#define DTRACE_CABAC_N

#endif

/** Function for codeword adaptation
 * \param uiCodeIdx codeword index of the syntax element being coded
 * \param pucTableCounter pointer to counter array
 * \param rucTableCounterSum sum counter
 * \param puiTableD pointer to table mapping codeword index to syntax element value
 * \param puiTableE pointer to table mapping syntax element value to codeword index
 * \param uiCounterNum number of counters
 * \returns
 * This function performs codeword adaptation.
*/
__inline void adaptCodeword(Ipp32u uiCodeIdx, Ipp8u * pucTableCounter, Ipp8u & rucTableCounterSum, Ipp32u * puiTableD, Ipp32u * puiTableE, Ipp32u uiCounterNum)
{
    bool bSwapping  = false;
    Ipp32u uiCodeword = puiTableD [uiCodeIdx];

    Ipp32u uiPrevCodeIdx   = (uiCodeIdx >= 1)? uiCodeIdx - 1 : 0;
    Ipp32u uiPrevCodeword  = puiTableD[uiPrevCodeIdx];

    if ( uiCodeIdx < uiCounterNum )
    {
        pucTableCounter [uiCodeIdx] ++;

        if (pucTableCounter[uiCodeIdx] >= pucTableCounter[uiPrevCodeIdx])
        {
            bSwapping = true;
            Ipp8u ucTempCounter             = pucTableCounter[uiCodeIdx];
            pucTableCounter[uiCodeIdx]      = pucTableCounter[uiPrevCodeIdx];
            pucTableCounter[uiPrevCodeIdx]  = ucTempCounter;
        }

        if ( rucTableCounterSum >= 15 )
        {
            rucTableCounterSum = 0;
            for (Ipp32u uiIdx = 0; uiIdx < uiCounterNum; uiIdx++)
            {
                pucTableCounter[uiIdx] >>= 1;
            }
        }
        else
        {
            rucTableCounterSum ++;
        }
    }
    else
    {
        bSwapping = true;
    }

    if ( bSwapping )
    {
        puiTableD[uiPrevCodeIdx] = uiCodeword;
        puiTableD[uiCodeIdx    ] = uiPrevCodeword;

        if  (puiTableE != NULL)
        {
            puiTableE[uiCodeword]     = uiPrevCodeIdx;
            puiTableE[uiPrevCodeword] = uiCodeIdx;
        }
    }
}

static const Ipp8u MatrixType[4][6][20] =
{
    {
        "INTRA4X4_LUMA",
        "INTRA4X4_CHROMAU",
        "INTRA4X4_CHROMAV",
        "INTER4X4_LUMA",
        "INTER4X4_CHROMAU",
        "INTER4X4_CHROMAV"
    },
    {
        "INTRA8X8_LUMA",
        "INTRA8X8_CHROMAU",
        "INTRA8X8_CHROMAV",
        "INTER8X8_LUMA",
        "INTER8X8_CHROMAU",
        "INTER8X8_CHROMAV"
    },
    {
        "INTRA16X16_LUMA",
        "INTRA16X16_CHROMAU",
        "INTRA16X16_CHROMAV",
        "INTER16X16_LUMA",
        "INTER16X16_CHROMAU",
        "INTER16X16_CHROMAV"
    },
    {
        "INTRA32X32_LUMA",
        "INTER32X32_LUMA",
    },
};

static const char MatrixType_DC[4][12][22] =
{
    {
    },
    {
    },
    {
        "INTRA16X16_LUMA_DC",
        "INTRA16X16_CHROMAU_DC",
        "INTRA16X16_CHROMAV_DC",
        "INTER16X16_LUMA_DC",
        "INTER16X16_CHROMAU_DC",
        "INTER16X16_CHROMAV_DC"
    },
    {
        "INTRA32X32_LUMA_DC",
        "INTER32X32_LUMA_DC",
    },
};

extern Ipp32s g_quantIntraDefault8x8[64];
extern Ipp32s g_quantInterDefault8x8[64];
extern Ipp32s g_quantTSDefault4x4[16];
extern Ipp32u g_scalingListSize [SCALING_LIST_SIZE_NUM];
extern Ipp32u g_scalingListSizeX[SCALING_LIST_SIZE_NUM];
extern Ipp32u g_scalingListNum  [SCALING_LIST_SIZE_NUM];
extern Ipp32u g_Table[4];

extern const Ipp16s g_lumaInterpolateFilter[4][8];
extern const Ipp16s g_chromaInterpolateFilter[8][4];

} // end namespace UMC_HEVC_DECODER

#endif //h265_global_rom_h
#endif // UMC_ENABLE_H264_VIDEO_DECODER
