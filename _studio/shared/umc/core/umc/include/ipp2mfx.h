// Copyright (c) 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __IPP2MFX_H__
#define __IPP2MFX_H__

#include <ippcore.h>

#if (_MSC_VER >= 1400)
#pragma warning(push)
// disable deprecated warning
#pragma warning( disable: 4996 )
#endif

#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

inline Ipp64u mfxGetCpuClocks ()
{
    return ippGetCpuClocks ();
}

inline void* mfxMalloc (int length)
{
    return ippMalloc (length);
}

inline void mfxFree (void* ptr)
{
    return ippFree (ptr);
}

inline IppStatus mfxsCopy_8u ( const Ipp8u* pSrc, Ipp8u* pDst, int len )
{
    return ippsCopy_8u ( pSrc, pDst, len );
}

inline IppStatus mfxsZero_8u ( Ipp8u* pDst, int len )
{
    return ippsZero_8u ( pDst, len );
}


#include <ipps.h>

inline IppStatus mfxsLShiftC_16s (const Ipp16s* pSrc, int val, Ipp16s* pDst, int len)
{
    return ippsLShiftC_16s (pSrc, val, pDst, len);
}

inline IppStatus mfxsCopy_16s ( const Ipp16s* pSrc, Ipp16s* pDst, int len )
{
    return ippsCopy_16s ( pSrc, pDst, len );
}

inline IppStatus mfxsZero_16s ( Ipp16s* pDst, int len )
{
    return ippsZero_16s ( pDst, len );
}


#include <ippvc.h>

inline IppStatus mfxiRangeMapping_VC1_8u_C1R (Ipp8u* pSrc, Ipp32s srcStep, Ipp8u* pDst, Ipp32s dstStep, IppiSize roiSize, Ipp32s rangeMapParam)
{
    return ippiRangeMapping_VC1_8u_C1R (pSrc, srcStep, pDst, dstStep, roiSize, rangeMapParam);
}

inline IppStatus mfxiDeinterlaceFilterTriangle_8u_C1R ( const Ipp8u* pSrc, Ipp32s srcStep, Ipp8u* pDst, Ipp32s dstStep, IppiSize roiSize, Ipp32u centerWeight, Ipp32u layout)
{
    return ippiDeinterlaceFilterTriangle_8u_C1R ( pSrc, srcStep, pDst, dstStep,  roiSize, centerWeight, layout);
}


#include <ippi.h>

inline IppStatus mfxiCopy_8u_C1R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiCopy_8u_C1R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiCopy_16s_C1R ( const Ipp16s* pSrc, int srcStep, Ipp16s* pDst, int dstStep, IppiSize roiSize )
{
    return ippiCopy_16s_C1R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiCopy_8u_C3P3R ( const Ipp8u* pSrc, int srcStep, Ipp8u* const pDst[3], int dstStep, IppiSize roiSize )
{
    return ippiCopy_8u_C3P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiCopy_16s_C3P3R ( const Ipp16s* pSrc, int srcStep, Ipp16s* const pDst[3], int dstStep, IppiSize roiSize )
{
    return ippiCopy_16s_C3P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiCopy_8u_C4P4R ( const Ipp8u* pSrc, int srcStep, Ipp8u* const pDst[4], int dstStep, IppiSize roiSize )
{
    return ippiCopy_8u_C4P4R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiCopy_16s_C4P4R ( const Ipp16s* pSrc, int srcStep, Ipp16s* const pDst[4], int dstStep, IppiSize roiSize )
{
    return ippiCopy_16s_C4P4R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiZigzagInv8x8_16s_C1 (const Ipp16s* pSrc, Ipp16s* pDst )
{
    return ippiZigzagInv8x8_16s_C1 (pSrc, pDst);
}

inline IppStatus mfxiConvert_8u16u_C1R ( const Ipp8u* pSrc, int srcStep, Ipp16u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiConvert_8u16u_C1R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiAndC_16u_C1IR (Ipp16u value, Ipp16u* pSrcDst, int srcDstStep, IppiSize roiSize)
{
    return ippiAndC_16u_C1IR (value, pSrcDst, srcDstStep, roiSize);
}

inline IppStatus mfxiConvert_16u8u_C1R ( const Ipp16u* pSrc, int srcStep, Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiConvert_16u8u_C1R ( pSrc, srcStep, pDst, dstStep, roiSize );
}


#include <ippj.h>

inline IppStatus mfxiDecodeHuffmanSpecGetBufSize_JPEG_8u ( int* size )
{
    return ippiDecodeHuffmanSpecGetBufSize_JPEG_8u (size);
}

inline IppStatus mfxiDecodeHuffmanSpecInit_JPEG_8u ( const Ipp8u* pListBits, const Ipp8u* pListVals, IppiDecodeHuffmanSpec* pDecHuffSpec )
{
    return ippiDecodeHuffmanSpecInit_JPEG_8u ( pListBits, pListVals, pDecHuffSpec );
}

inline IppStatus mfxiDecodeHuffmanStateGetBufSize_JPEG_8u ( int* size )
{
    return ippiDecodeHuffmanStateGetBufSize_JPEG_8u ( size );
}

inline IppStatus mfxiDecodeHuffmanStateInit_JPEG_8u ( IppiDecodeHuffmanState* pDecHuffState )
{
    return ippiDecodeHuffmanStateInit_JPEG_8u ( pDecHuffState );
}

inline IppStatus mfxiQuantInvTableInit_JPEG_8u16u ( const Ipp8u* pQuantRawTable, Ipp16u* pQuantInvTable )
{
    return ippiQuantInvTableInit_JPEG_8u16u ( pQuantRawTable, pQuantInvTable );
}

inline IppStatus mfxiYCbCrToBGR_JPEG_8u_P3C4R ( const Ipp8u* pYCbCr[3], int srcStep, Ipp8u* pBGR, int dstStep, IppiSize roiSize, Ipp8u aval)
{
    return ippiYCbCrToBGR_JPEG_8u_P3C4R ( pYCbCr, srcStep, pBGR, dstStep, roiSize, aval );
}

inline IppStatus mfxiSampleUpRowH2V1_Triangle_JPEG_8u_C1 ( const Ipp8u* pSrc, int srcWidth, Ipp8u* pDst)
{
    return ippiSampleUpRowH2V1_Triangle_JPEG_8u_C1 ( pSrc, srcWidth, pDst);
}

inline IppStatus mfxiSampleUpRowH2V2_Triangle_JPEG_8u_C1 ( const Ipp8u* pSrc1, const Ipp8u* pSrc2, int srcWidth, Ipp8u* pDst )
{
    return ippiSampleUpRowH2V2_Triangle_JPEG_8u_C1 ( pSrc1, pSrc2, srcWidth, pDst );
}

inline IppStatus mfxiSampleDownH2V2_JPEG_8u_C1R ( const Ipp8u* pSrc, int srcStep, IppiSize srcRoiSize, Ipp8u* pDst, int dstStep, IppiSize dstRoiSize )
{
    return ippiSampleDownH2V2_JPEG_8u_C1R ( pSrc, srcStep, srcRoiSize, pDst, dstStep, dstRoiSize );
}

inline IppStatus mfxiSampleDownH2V1_JPEG_8u_C1R ( const Ipp8u* pSrc, int srcStep, IppiSize srcRoiSize, Ipp8u* pDst, int dstStep, IppiSize dstRoiSize )
{
    return ippiSampleDownH2V1_JPEG_8u_C1R ( pSrc, srcStep, srcRoiSize, pDst,  dstStep, dstRoiSize );
}

inline IppStatus mfxiDecodeHuffman8x8_JPEG_1u16s_C1 ( const Ipp8u* pSrc, int srcLenBytes, int* pSrcCurrPos, Ipp16s* pDst, Ipp16s* pLastDC, int* pMarker, const IppiDecodeHuffmanSpec* pDcTable, const IppiDecodeHuffmanSpec* pAcTable, IppiDecodeHuffmanState* pDecHuffState )
{
    return ippiDecodeHuffman8x8_JPEG_1u16s_C1 ( pSrc, srcLenBytes, pSrcCurrPos, pDst, pLastDC, pMarker, pDcTable, pAcTable, pDecHuffState);
}

inline IppStatus mfxiDecodeHuffmanRow_JPEG_1u16s_C1P4 ( const Ipp8u* pSrc, int nSrcLenBytes, int* pSrcCurrPos, Ipp16s* pDst[4], int nDstLen, int nDstRows, int* pMarker, const IppiDecodeHuffmanSpec* pDecHuffTable[4], IppiDecodeHuffmanState* pDecHuffState )
{
    return ippiDecodeHuffmanRow_JPEG_1u16s_C1P4 ( pSrc, nSrcLenBytes, pSrcCurrPos, pDst, nDstLen, nDstRows, pMarker, pDecHuffTable, pDecHuffState);
}

inline IppStatus mfxiDCTQuantInv8x8LS_1x1_JPEG_16s8u_C1R ( const Ipp16s* pSrc, Ipp8u* pDst, int dstStep, const Ipp16u* pQuantInvTable )
{
    return ippiDCTQuantInv8x8LS_1x1_JPEG_16s8u_C1R ( pSrc, pDst, dstStep, pQuantInvTable);
}

inline IppStatus mfxiDCTQuantInv8x8LS_2x2_JPEG_16s8u_C1R ( const Ipp16s* pSrc, Ipp8u* pDst, int dstStep, const Ipp16u* pQuantInvTable )
{
    return ippiDCTQuantInv8x8LS_2x2_JPEG_16s8u_C1R ( pSrc, pDst, dstStep, pQuantInvTable);
}

inline IppStatus mfxiDCTQuantInv8x8LS_4x4_JPEG_16s8u_C1R ( const Ipp16s* pSrc, Ipp8u* pDst, int dstStep, const Ipp16u* pQuantInvTable )
{
    return ippiDCTQuantInv8x8LS_4x4_JPEG_16s8u_C1R ( pSrc, pDst, dstStep, pQuantInvTable );
}

inline IppStatus mfxiDCTQuantInv8x8LS_JPEG_16s8u_C1R (  Ipp16s* pSrc, Ipp8u* pDst, int dstStep, const Ipp16u* pQuantInvTable)
{
    return ippiDCTQuantInv8x8LS_JPEG_16s8u_C1R ( pSrc, pDst, dstStep, pQuantInvTable );
}

inline IppStatus mfxiDCTQuantInv8x8To4x4LS_JPEG_16s8u_C1R ( const Ipp16s* pSrc, Ipp8u* pDst, int dstStep, const Ipp16u* pQuantInvTable )
{
    return ippiDCTQuantInv8x8To4x4LS_JPEG_16s8u_C1R ( pSrc, pDst, dstStep, pQuantInvTable );
}

inline IppStatus mfxiDCTQuantInv8x8To2x2LS_JPEG_16s8u_C1R ( const Ipp16s* pSrc, Ipp8u* pDst, int dstStep, const Ipp16u* pQuantInvTable )
{
    return ippiDCTQuantInv8x8To2x2LS_JPEG_16s8u_C1R ( pSrc, pDst, dstStep, pQuantInvTable );
}

inline IppStatus mfxiDCTQuantInv8x8LS_JPEG_16s16u_C1R ( const Ipp16s* pSrc, Ipp16u* pDst, int dstStep, const Ipp32f* pQuantInvTable)
{
    return ippiDCTQuantInv8x8LS_JPEG_16s16u_C1R ( pSrc, pDst, dstStep, pQuantInvTable );
}

inline IppStatus mfxiReconstructPredRow_JPEG_16s_C1 ( const Ipp16s* pSrc, const Ipp16s* pPrevRow, Ipp16s* pDst, int width, int predictor)
{
    return ippiReconstructPredRow_JPEG_16s_C1 ( pSrc, pPrevRow, pDst, width, predictor );
}

inline IppStatus mfxiReconstructPredFirstRow_JPEG_16s_C1 ( const Ipp16s* pSrc, Ipp16s* pDst, int width, int P, int Pt )
{
    return ippiReconstructPredFirstRow_JPEG_16s_C1 ( pSrc, pDst, width, P, Pt );
}

inline IppStatus mfxiDecodeHuffman8x8_ACFirst_JPEG_1u16s_C1 ( const Ipp8u* pSrc, int srcLenBytes, int* pSrcCurrPos, Ipp16s* pDst, int* pMarker, int Ss, int Se, int Al, const IppiDecodeHuffmanSpec* pAcTable, IppiDecodeHuffmanState* pDecHuffState)
{
    return ippiDecodeHuffman8x8_ACFirst_JPEG_1u16s_C1 ( pSrc, srcLenBytes, pSrcCurrPos, pDst, pMarker, Ss, Se, Al, pAcTable, pDecHuffState );
}

inline IppStatus mfxiDecodeHuffman8x8_ACRefine_JPEG_1u16s_C1 ( const Ipp8u* pSrc, int srcLenBytes, int* pSrcCurrPos, Ipp16s* pDst, int* pMarker, int Ss, int Se, int Al, const IppiDecodeHuffmanSpec* pAcTable, IppiDecodeHuffmanState* pDecHuffState)
{
    return ippiDecodeHuffman8x8_ACRefine_JPEG_1u16s_C1 ( pSrc, srcLenBytes, pSrcCurrPos, pDst, pMarker, Ss, Se, Al, pAcTable, pDecHuffState );
}

inline IppStatus mfxiDecodeHuffman8x8_DCFirst_JPEG_1u16s_C1 ( const Ipp8u* pSrc, int srcLenBytes, int* pSrcCurrPos, Ipp16s* pDst, Ipp16s* pLastDC, int* pMarker, int Al, const IppiDecodeHuffmanSpec* pDcTable, IppiDecodeHuffmanState* pDecHuffState)
{
    return ippiDecodeHuffman8x8_DCFirst_JPEG_1u16s_C1 ( pSrc, srcLenBytes, pSrcCurrPos, pDst, pLastDC, pMarker, Al, pDcTable, pDecHuffState );
}

inline IppStatus mfxiDecodeHuffman8x8_DCRefine_JPEG_1u16s_C1 ( const Ipp8u* pSrc, int srcLenBytes, int* pSrcCurrPos, Ipp16s* pDst, int* pMarker, int Al, IppiDecodeHuffmanState* pDecHuffState)
{
    return ippiDecodeHuffman8x8_DCRefine_JPEG_1u16s_C1 ( pSrc, srcLenBytes, pSrcCurrPos, pDst, pMarker, Al, pDecHuffState );
}

inline IppStatus mfxiDecodeHuffmanOne_JPEG_1u16s_C1 ( const Ipp8u* pSrc, int nSrcLenBytes, int* pSrcCurrPos, Ipp16s* pDst, int* pMarker, const IppiDecodeHuffmanSpec* pDecHuffTable, IppiDecodeHuffmanState* pDecHuffState)
{
    return ippiDecodeHuffmanOne_JPEG_1u16s_C1 ( pSrc, nSrcLenBytes, pSrcCurrPos, pDst, pMarker, pDecHuffTable, pDecHuffState );
}

inline IppStatus mfxiEncodeHuffmanSpecGetBufSize_JPEG_8u ( int* size )
{
    return ippiEncodeHuffmanSpecGetBufSize_JPEG_8u ( size );
}

inline IppStatus mfxiEncodeHuffmanSpecInit_JPEG_8u ( const Ipp8u* pListBits, const Ipp8u* pListVals, IppiEncodeHuffmanSpec* pEncHuffSpec)
{
    return ippiEncodeHuffmanSpecInit_JPEG_8u ( pListBits, pListVals, pEncHuffSpec);
}

inline IppStatus mfxiEncodeHuffmanStateGetBufSize_JPEG_8u ( int* size )
{
    return ippiEncodeHuffmanStateGetBufSize_JPEG_8u (size);
}

inline IppStatus mfxiEncodeHuffmanStateInit_JPEG_8u ( IppiEncodeHuffmanState* pEncHuffState )
{
    return ippiEncodeHuffmanStateInit_JPEG_8u ( pEncHuffState);
}

inline IppStatus mfxiQuantFwdRawTableInit_JPEG_8u ( Ipp8u* pQuantRawTable, int qualityFactor )
{
    return ippiQuantFwdRawTableInit_JPEG_8u ( pQuantRawTable, qualityFactor );
}

inline IppStatus mfxiQuantFwdTableInit_JPEG_8u16u ( const Ipp8u* pQuantRawTable, Ipp16u* pQuantFwdTable)
{
    return ippiQuantFwdTableInit_JPEG_8u16u ( pQuantRawTable, pQuantFwdTable );
}

inline IppStatus mfxiEncodeHuffman8x8_JPEG_16s1u_C1 ( const Ipp16s* pSrc, Ipp8u* pDst, int dstLenBytes, int* pDstCurrPos, Ipp16s* pLastDC, const IppiEncodeHuffmanSpec* pDcTable, const IppiEncodeHuffmanSpec* pAcTable, IppiEncodeHuffmanState* pEncHuffState, int bFlushState )
{
    return ippiEncodeHuffman8x8_JPEG_16s1u_C1 ( pSrc, pDst, dstLenBytes, pDstCurrPos, pLastDC, pDcTable, pAcTable, pEncHuffState, bFlushState );
}

inline IppStatus mfxiEncodeHuffman8x8_DCFirst_JPEG_16s1u_C1 ( const Ipp16s* pSrc, Ipp8u* pDst, int dstLenBytes, int* pDstCurrPos, Ipp16s* pLastDC, int Al, const IppiEncodeHuffmanSpec* pDcTable, IppiEncodeHuffmanState* pEncHuffState, int bFlushState)
{
    return ippiEncodeHuffman8x8_DCFirst_JPEG_16s1u_C1 ( pSrc, pDst, dstLenBytes, pDstCurrPos, pLastDC, Al, pDcTable, pEncHuffState,  bFlushState );
}

inline IppStatus mfxiEncodeHuffman8x8_DCRefine_JPEG_16s1u_C1 ( const Ipp16s* pSrc, Ipp8u* pDst, int dstLenBytes, int* pDstCurrPos, int Al, IppiEncodeHuffmanState* pEncHuffState, int bFlushState )
{
    return ippiEncodeHuffman8x8_DCRefine_JPEG_16s1u_C1 ( pSrc, pDst, dstLenBytes, pDstCurrPos, Al, pEncHuffState, bFlushState );
}

inline IppStatus mfxiEncodeHuffman8x8_ACFirst_JPEG_16s1u_C1 ( const Ipp16s* pSrc, Ipp8u* pDst, int dstLenBytes, int* pDstCurrPos, int Ss, int Se, int Al, const IppiEncodeHuffmanSpec* pAcTable, IppiEncodeHuffmanState* pEncHuffState, int bFlushState )
{
    return ippiEncodeHuffman8x8_ACFirst_JPEG_16s1u_C1 ( pSrc, pDst, dstLenBytes, pDstCurrPos, Ss, Se, Al, pAcTable, pEncHuffState,  bFlushState );
}

inline IppStatus mfxiEncodeHuffman8x8_ACRefine_JPEG_16s1u_C1 ( const Ipp16s* pSrc, Ipp8u* pDst, int dstLenBytes, int* pDstCurrPos, int Ss, int Se, int Al, const IppiEncodeHuffmanSpec* pAcTable, IppiEncodeHuffmanState* pEncHuffState, int bFlushState)
{
    return ippiEncodeHuffman8x8_ACRefine_JPEG_16s1u_C1 ( pSrc, pDst, dstLenBytes, pDstCurrPos, Ss, Se, Al, pAcTable, pEncHuffState, bFlushState);
}

inline IppStatus mfxiGetHuffmanStatistics8x8_ACFirst_JPEG_16s_C1 ( const Ipp16s* pSrc, int pAcStatistics[256], int Ss, int Se, int Al, IppiEncodeHuffmanState* pEncHuffState, int bFlushState )
{
    return ippiGetHuffmanStatistics8x8_ACFirst_JPEG_16s_C1 ( pSrc, pAcStatistics, Ss, Se, Al, pEncHuffState, bFlushState );
}

inline IppStatus mfxiGetHuffmanStatistics8x8_ACRefine_JPEG_16s_C1 ( const Ipp16s* pSrc, int pAcStatistics[256], int Ss, int Se, int Al, IppiEncodeHuffmanState* pEncHuffState, int bFlushState )
{
    return ippiGetHuffmanStatistics8x8_ACRefine_JPEG_16s_C1 ( pSrc, pAcStatistics, Ss, Se, Al, pEncHuffState, bFlushState );
}

inline IppStatus mfxiEncodeHuffmanOne_JPEG_16s1u_C1 ( const Ipp16s* pSrc, Ipp8u* pDst, int nDstLenBytes, int* pDstCurrPos, const IppiEncodeHuffmanSpec* pEncHuffTable, IppiEncodeHuffmanState* pEncHuffState, int bFlushState)
{
    return ippiEncodeHuffmanOne_JPEG_16s1u_C1 ( pSrc, pDst, nDstLenBytes, pDstCurrPos, pEncHuffTable, pEncHuffState,  bFlushState );
}

inline IppStatus mfxiRGBToY_JPEG_8u_C3C1R ( const Ipp8u* pSrcRGB, int srcStep, Ipp8u* pDstY, int dstStep, IppiSize roiSize )
{
    return ippiRGBToY_JPEG_8u_C3C1R ( pSrcRGB, srcStep, pDstY, dstStep, roiSize );
}

inline IppStatus mfxiRGBToYCbCr_JPEG_8u_C3P3R ( const Ipp8u* pSrcRGB, int srcStep, Ipp8u* pDstYCbCr[3], int dstStep, IppiSize roiSize )
{
    return ippiRGBToYCbCr_JPEG_8u_C3P3R ( pSrcRGB, srcStep, pDstYCbCr, dstStep, roiSize );
}

inline IppStatus mfxiRGBToYCbCr_JPEG_8u_P3R ( const Ipp8u* pSrcRGB[3], int srcStep, Ipp8u* pDstYCbCr[3], int dstStep, IppiSize roiSize )
{
    return ippiRGBToYCbCr_JPEG_8u_P3R ( pSrcRGB, srcStep, pDstYCbCr, dstStep, roiSize );
}

inline IppStatus mfxiBGRToYCbCr_JPEG_8u_C3P3R ( const Ipp8u* pSrcBGR, int srcStep, Ipp8u* pDstYCbCr[3], int dstStep, IppiSize roiSize )
{
    return ippiBGRToYCbCr_JPEG_8u_C3P3R ( pSrcBGR, srcStep, pDstYCbCr, dstStep,  roiSize );
}

inline IppStatus mfxiRGBToYCbCr_JPEG_8u_C4P3R ( const Ipp8u* pRGBA, int srcStep, Ipp8u* pYCbCr[3], int dstStep, IppiSize roiSize )
{
    return ippiRGBToYCbCr_JPEG_8u_C4P3R ( pRGBA, srcStep, pYCbCr, dstStep, roiSize );
}

inline IppStatus mfxiSampleDownRowH2V1_Box_JPEG_8u_C1 ( const Ipp8u* pSrc, int srcWidth, Ipp8u* pDst)
{
    return ippiSampleDownRowH2V1_Box_JPEG_8u_C1 ( pSrc, srcWidth, pDst );
}

inline IppStatus mfxiCMYKToYCCK_JPEG_8u_C4P4R ( const Ipp8u* pSrcCMYK, int srcStep, Ipp8u* pDstYCCK[4], int dstStep, IppiSize roiSize )
{
    return ippiCMYKToYCCK_JPEG_8u_C4P4R ( pSrcCMYK, srcStep, pDstYCCK, dstStep, roiSize);
}

inline IppStatus mfxiSampleDownRowH2V2_Box_JPEG_8u_C1 ( const Ipp8u* pSrc1, const Ipp8u* pSrc2, int srcWidth, Ipp8u* pDst)
{
    return ippiSampleDownRowH2V2_Box_JPEG_8u_C1 ( pSrc1, pSrc2, srcWidth, pDst );
}

inline IppStatus mfxiDCTQuantFwd8x8LS_JPEG_8u16s_C1R ( const Ipp8u* pSrc, int srcStep, Ipp16s* pDst, const Ipp16u* pQuantFwdTable )
{
    return ippiDCTQuantFwd8x8LS_JPEG_8u16s_C1R ( pSrc, srcStep, pDst, pQuantFwdTable );
}

inline IppStatus mfxiDCTQuantFwd8x8LS_JPEG_16u16s_C1R ( const Ipp16u* pSrc, int srcStep, Ipp16s* pDst, const Ipp32f* pQuantFwdTable )
{
    return ippiDCTQuantFwd8x8LS_JPEG_16u16s_C1R ( pSrc, srcStep, pDst, pQuantFwdTable );
}

inline IppStatus mfxiEncodeHuffmanRawTableInit_JPEG_8u ( const int pStatistics[256], Ipp8u* pListBits, Ipp8u* pListVals )
{
    return ippiEncodeHuffmanRawTableInit_JPEG_8u ( pStatistics, pListBits, pListVals );
}

inline IppStatus mfxiGetHuffmanStatistics8x8_DCFirst_JPEG_16s_C1 ( const Ipp16s* pSrc, int pDcStatistics[256], Ipp16s* pLastDC, int Al)
{
    return ippiGetHuffmanStatistics8x8_DCFirst_JPEG_16s_C1 ( pSrc, pDcStatistics, pLastDC, Al );
}

inline IppStatus mfxiGetHuffmanStatisticsOne_JPEG_16s_C1 ( const Ipp16s* pSrc, int pHuffStatistics[256])
{
    return ippiGetHuffmanStatisticsOne_JPEG_16s_C1 ( pSrc, pHuffStatistics );
}

inline IppStatus mfxiGetHuffmanStatistics8x8_JPEG_16s_C1 ( const Ipp16s* pSrc, int pDcStatistics[256], int pAcStatistics[256], Ipp16s* pLastDC)
{
    return ippiGetHuffmanStatistics8x8_JPEG_16s_C1 ( pSrc, pDcStatistics, pAcStatistics, pLastDC);
}

#if (_MSC_VER >= 1400)
// enable deprecated warning
#pragma warning(pop)
#endif

#if __GNUC__
#pragma GCC diagnostic pop
#endif

#endif // __IPP2MFX_H__
