/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSSE3) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 

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

#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    #define MAKE_NAME( func ) func ## _px
#else
    #define MAKE_NAME( func ) func ## _px
#endif

void MAKE_NAME(h265_CopyWeighted_S16U8)(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth)
{

    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width; x++)
        {
            pDst[x] = (Ipp16u)ClipY(((w[0] * pSrc[x] + round[0]) >> logWD[0]) + o[0], bit_depth);
        }
        pSrc += SrcStrideY;
        pDst += DstStrideY;
    }

    Width >>= 1;
    Height >>= 1;

    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width * 2; x += 2)
        {
            pDstUV[x] = (Ipp16u)ClipC(((w[1] * pSrcUV[x] + round[1]) >> logWD[1]) + o[1], bit_depth);
            pDstUV[x + 1] = (Ipp16u)ClipC(((w[2] * pSrcUV[x + 1] + round[2]) >> logWD[2]) + o[2], bit_depth);
        }
        pSrcUV += SrcStrideC;
        pDstUV += DstStrideC;
    }
}

void MAKE_NAME(h265_CopyWeightedBidi_S16U8)(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth)
{
    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width; x++)
        {
            pDst[x] = (Ipp16u)ClipY((w0[0] * pSrc0[x] + w1[0] * pSrc1[x] + round[0]) >> logWD[0], bit_depth);
        }
        pSrc0 += SrcStride0Y;
        pSrc1 += SrcStride1Y;
        pDst += DstStrideY;
    }

    Width >>= 1;
    Height >>= 1;

    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width * 2; x += 2)
        {
            pDstUV[x] = (Ipp16u)ClipC((w0[1] * pSrcUV0[x] + w1[1] * pSrcUV1[x] + round[1]) >> logWD[1], bit_depth);
            pDstUV[x + 1] = (Ipp16u)ClipC((w0[2] * pSrcUV0[x + 1] + w1[2] * pSrcUV1[x + 1] + round[2]) >> logWD[2], bit_depth);
        }
        pSrcUV0 += SrcStride0C;
        pSrcUV1 += SrcStride1C;
        pDstUV += DstStrideC;
    }
}

} // end namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSSE3) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
