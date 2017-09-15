//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_AUTO)

namespace MFX_HEVC_PP
{

// ML: OPT: called in hot loops, compiler does not seem to always honor 'inline'
// ML: OPT: TODO: Make sure compiler recognizes saturation idiom for vectorization
template <typename T> 
static inline T H265_FORCEINLINE ClipY(T Value, int c_bitDepth = 8) 
{ 
    Value = (Value < 0) ? 0 : Value;
    const int c_Mask = ((1 << c_bitDepth) - 1);
    Value = (Value >= c_Mask) ? c_Mask : Value;
    return ( Value );
}

template <typename T> 
static inline T H265_FORCEINLINE ClipC(T Value, int c_bitDepth = 8) 
{ 
    Value = (Value < 0) ? 0 : Value;
    const int c_Mask = ((1 << c_bitDepth) - 1);
    Value = (Value >= c_Mask) ? c_Mask : Value;
    return ( Value );
}

template <int bitDepth, typename DstCoeffsType>
static void h265_CopyWeighted_Kernel(Ipp16s* pSrc, Ipp16s* pSrcUV, DstCoeffsType* pDst, DstCoeffsType* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth_chroma)
{

    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width; x++)
        {
            pDst[x] = (DstCoeffsType)ClipY(((w[0] * pSrc[x] + round[0]) >> logWD[0]) + o[0], bitDepth);
        }
        pSrc += SrcStrideY;
        pDst += DstStrideY;
    }

    Width >>= 1;
    Height >>= !isChroma422;

    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width * 2; x += 2)
        {
            pDstUV[x] = (DstCoeffsType)ClipC(((w[1] * pSrcUV[x] + round[1]) >> logWD[1]) + o[1], bit_depth_chroma);
            pDstUV[x + 1] = (DstCoeffsType)ClipC(((w[2] * pSrcUV[x + 1] + round[2]) >> logWD[2]) + o[2], bit_depth_chroma);
        }
        pSrcUV += SrcStrideC;
        pDstUV += DstStrideC;
    }
}

void MAKE_NAME(h265_CopyWeighted_S16U8)(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round)
{
    h265_CopyWeighted_Kernel<8, Ipp8u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, 8);
}

void MAKE_NAME(h265_CopyWeighted_S16U16)(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma)
{
    if (bit_depth == 9)
        h265_CopyWeighted_Kernel< 9, Ipp16u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, bit_depth_chroma);
    else if (bit_depth == 10)
        h265_CopyWeighted_Kernel<10, Ipp16u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, bit_depth_chroma);
    else if (bit_depth == 8)
        h265_CopyWeighted_Kernel<8, Ipp16u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, bit_depth_chroma);
    else if (bit_depth == 11)
        h265_CopyWeighted_Kernel<11, Ipp16u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, bit_depth_chroma);
    else if (bit_depth == 12)
        h265_CopyWeighted_Kernel<12, Ipp16u>(pSrc, pSrcUV, pDst, pDstUV, SrcStrideY, DstStrideY, SrcStrideC, DstStrideC, isChroma422, Width, Height, w, o, logWD, round, bit_depth_chroma);
}

template <int bitDepth, typename DstCoeffsType>
static void h265_CopyWeightedBidi_Kernel(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, DstCoeffsType* pDst, DstCoeffsType* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bitDepthChroma)
{
    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width; x++)
        {
            pDst[x] = (DstCoeffsType)ClipY((w0[0] * pSrc0[x] + w1[0] * pSrc1[x] + round[0]) >> logWD[0], bitDepth);
        }
        pSrc0 += SrcStride0Y;
        pSrc1 += SrcStride1Y;
        pDst += DstStrideY;
    }

    Width >>= 1;
    Height >>= !isChroma422;

    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width * 2; x += 2)
        {
            pDstUV[x] = (DstCoeffsType)ClipC((w0[1] * pSrcUV0[x] + w1[1] * pSrcUV1[x] + round[1]) >> logWD[1], bitDepthChroma);
            pDstUV[x + 1] = (DstCoeffsType)ClipC((w0[2] * pSrcUV0[x + 1] + w1[2] * pSrcUV1[x + 1] + round[2]) >> logWD[2], bitDepthChroma);
        }
        pSrcUV0 += SrcStride0C;
        pSrcUV1 += SrcStride1C;
        pDstUV += DstStrideC;
    }
}

void MAKE_NAME(h265_CopyWeightedBidi_S16U8)(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round)
{
    h265_CopyWeightedBidi_Kernel< 8, Ipp8u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, 8);
}

void MAKE_NAME(h265_CopyWeightedBidi_S16U16)(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp8u isChroma422, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma)
{
    if (bit_depth == 9)
        h265_CopyWeightedBidi_Kernel< 9, Ipp16u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, bit_depth_chroma);
    else if (bit_depth == 10)
        h265_CopyWeightedBidi_Kernel<10, Ipp16u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, bit_depth_chroma);
    else if (bit_depth == 8)
        h265_CopyWeightedBidi_Kernel<8, Ipp16u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, bit_depth_chroma);
    else if (bit_depth == 11)
        h265_CopyWeightedBidi_Kernel<11, Ipp16u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, bit_depth_chroma);
    else if (bit_depth == 12)
        h265_CopyWeightedBidi_Kernel<12, Ipp16u>(pSrc0, pSrcUV0, pSrc1, pSrcUV1, pDst, pDstUV, SrcStride0Y, SrcStride1Y, DstStrideY, SrcStride0C, SrcStride1C, DstStrideC, isChroma422, Width, Height, w0, w1, logWD, round, bit_depth_chroma);
}

} // end namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSSE3) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
