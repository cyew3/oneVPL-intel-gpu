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


#ifndef __IPPCC2MFX_H__
#define __IPPCC2MFX_H__

#include "ippcc.h"


#if (_MSC_VER >= 1400)
#pragma warning(push)
// disable deprecated warning
#pragma warning( disable: 4996 )
#endif

#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

inline IppStatus mfxiYCbCr420ToYCbCr422_8u_P3R ( const Ipp8u* pSrc[3], int srcStep[3], Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize )
{
    return ippiYCbCr420ToYCbCr422_8u_P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

// ippcc.h
inline IppStatus mfxiYCrCb420ToYCbCr422_8u_P3C2R ( const Ipp8u* pSrc[3],int srcStep[3], Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiYCrCb420ToYCbCr422_8u_P3C2R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiYCbCr420To411_8u_P3R ( const Ipp8u* pSrc[3], int srcStep[3], Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize)
{
    return ippiYCbCr420To411_8u_P3R ( pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiYCrCb420ToYCbCr420_8u_P3P2R ( const Ipp8u* pSrc[3],int srcStep[3], Ipp8u* pDstY, int dstYStep,Ipp8u* pDstCbCr,int dstCbCrStep, IppiSize roiSize )
{
    return ippiYCrCb420ToYCbCr420_8u_P3P2R ( pSrc, srcStep, pDstY, dstYStep, pDstCbCr, dstCbCrStep, roiSize );
}

inline IppStatus mfxippiYCrCb420ToCbYCr422_8u_P3C2R ( const Ipp8u* pSrc[3],int srcStep[3], Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiYCrCb420ToCbYCr422_8u_P3C2R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiYCbCr420ToBGR_8u_P3C3R (const Ipp8u* pSrc[3],int srcStep[3], Ipp8u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiYCbCr420ToBGR_8u_P3C3R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiYCrCb420ToCbYCr422_8u_P3C2R ( const Ipp8u* pSrc[3],int srcStep[3], Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiYCrCb420ToCbYCr422_8u_P3C2R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiYCrCb420ToBGR_Filter_8u_P3C4R (const Ipp8u* pSrc[3], int srcStep[3], Ipp8u* pDst, int dstStep, IppiSize roiSize, Ipp8u aval )
{
    return ippiYCrCb420ToBGR_Filter_8u_P3C4R (pSrc, srcStep, pDst, dstStep,  roiSize, aval );
}

inline IppStatus mfxiYCbCr420ToBGR565_8u16u_P3C3R (const Ipp8u* pSrc[3], int srcStep[3],Ipp16u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiYCbCr420ToBGR565_8u16u_P3C3R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiYCbCr420ToBGR555_8u16u_P3C3R (const Ipp8u* pSrc[3], int srcStep[3],Ipp16u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiYCbCr420ToBGR555_8u16u_P3C3R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiYCbCr420ToBGR444_8u16u_P3C3R (const Ipp8u* pSrc[3], int srcStep[3],Ipp16u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiYCbCr420ToBGR444_8u16u_P3C3R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiYCbCr422ToYCbCr420_8u_P3R ( const Ipp8u* pSrc[3], int srcStep[3], Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize )
{
    return ippiYCbCr422ToYCbCr420_8u_P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiYCbCr422_8u_P3C2R ( const Ipp8u* pSrc[3], int srcStep[3], Ipp8u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiYCbCr422_8u_P3C2R ( pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiYCbCrToBGR_8u_P3C4R ( const Ipp8u* pSrc[3],int srcStep,Ipp8u* pDst,int dstStep,IppiSize roiSize, Ipp8u aval )
{
    return ippiYCbCrToBGR_8u_P3C4R ( pSrc, srcStep, pDst, dstStep, roiSize, aval );
}

inline IppStatus mfxiYCbCr422ToYCbCr420_8u_C2P3R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize )
{
    return ippiYCbCr422ToYCbCr420_8u_C2P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiYCbCr422_8u_C2P3R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize )
{
    return ippiYCbCr422_8u_C2P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiYCbCr422ToYCbCr420_8u_C2P2R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDstY, int dstYStep,Ipp8u* pDstCbCr,int dstCbCrStep, IppiSize roiSize )
{
    return ippiYCbCr422ToYCbCr420_8u_C2P2R ( pSrc, srcStep, pDstY, dstYStep, pDstCbCr, dstCbCrStep, roiSize );
}

inline IppStatus mfxiYCbCr422ToCbYCr422_8u_C2R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiYCbCr422ToCbYCr422_8u_C2R ( pSrc, srcStep, pDst, dstStep, roiSize );
}


inline IppStatus mfxiYCbCr422ToBGR_8u_C2C3R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst,int dstStep, IppiSize roiSize )
{
    return ippiYCbCr422ToBGR_8u_C2C3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiYCbCr422ToBGR_8u_C2C4R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst,int dstStep, IppiSize roiSize, Ipp8u aval )
{
    return ippiYCbCr422ToBGR_8u_C2C4R ( pSrc, srcStep, pDst, dstStep, roiSize,  aval );
}

inline IppStatus mfxiYCbCr422ToBGR565_8u16u_C2C3R (const Ipp8u* pSrc, int srcStep, Ipp16u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiYCbCr422ToBGR565_8u16u_C2C3R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiYCbCr422ToBGR555_8u16u_C2C3R (const Ipp8u* pSrc, int srcStep, Ipp16u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiYCbCr422ToBGR555_8u16u_C2C3R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiYCbCr422ToBGR444_8u16u_C2C3R (const Ipp8u* pSrc, int srcStep, Ipp16u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiYCbCr422ToBGR444_8u16u_C2C3R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiCbYCr422ToYCrCb420_8u_C2P3R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst[3],int dstStep[3], IppiSize roiSize )
{
    return ippiCbYCr422ToYCrCb420_8u_C2P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiCbYCr422ToYCbCr420_8u_C2P2R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDstY, int dstYStep,Ipp8u* pDstCbCr,int dstCbCrStep, IppiSize roiSize )
{
    return ippiCbYCr422ToYCbCr420_8u_C2P2R ( pSrc, srcStep, pDstY, dstYStep, pDstCbCr, dstCbCrStep, roiSize );
}

inline IppStatus mfxiCbYCr422ToYCbCr422_8u_C2R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiCbYCr422ToYCbCr422_8u_C2R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiCbYCr422ToBGR_8u_C2C4R (const Ipp8u* pSrc, int srcStep, Ipp8u* pDst, int dstStep, IppiSize roiSize, Ipp8u aval)
{
    return ippiCbYCr422ToBGR_8u_C2C4R (pSrc, srcStep, pDst, dstStep, roiSize, aval);
}

inline IppStatus mfxiCbYCr422ToYCbCr422_8u_C2P3R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize )
{
    return ippiCbYCr422ToYCbCr422_8u_C2P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiYCbCr420_8u_P2P3R (const Ipp8u* pSrcY,int srcYStep,const Ipp8u* pSrcCbCr, int srcCbCrStep, Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize )
{
    return ippiYCbCr420_8u_P2P3R (pSrcY, srcYStep, pSrcCbCr, srcCbCrStep, pDst,  dstStep, roiSize );
}

inline IppStatus mfxiYCbCr420ToCbYCr422_8u_P2C2R ( const Ipp8u* pSrcY, int srcYStep,const Ipp8u* pSrcCbCr, int srcCbCrStep, Ipp8u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiYCbCr420ToCbYCr422_8u_P2C2R ( pSrcY, srcYStep, pSrcCbCr, srcCbCrStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiYCbCr420ToYCbCr422_8u_P2C2R (const Ipp8u* pSrcY, int srcYStep,const Ipp8u* pSrcCbCr, int srcCbCrStep, Ipp8u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiYCbCr420ToYCbCr422_8u_P2C2R (pSrcY, srcYStep, pSrcCbCr, srcCbCrStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiBGRToYCrCb420_8u_C3P3R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize )
{
    return ippiBGRToYCrCb420_8u_C3P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiBGRToYCbCr422_8u_C3C2R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiBGRToYCbCr422_8u_C3C2R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiBGRToYCrCb420_8u_AC4P3R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize )
{
    return ippiBGRToYCrCb420_8u_AC4P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiBGRToYCbCr422_8u_AC4C2R ( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiBGRToYCbCr422_8u_AC4C2R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiBGRToCbYCr422_8u_AC4C2R (const Ipp8u* pSrc, int srcStep , Ipp8u* pDst, int dstStep, IppiSize roiSize)
{
    return ippiBGRToCbYCr422_8u_AC4C2R (pSrc, srcStep, pDst, dstStep, roiSize);
}

inline IppStatus mfxiBGR555ToYCrCb420_16u8u_C3P3R ( const Ipp16u* pSrc, int srcStep, Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize )
{
    return ippiBGR555ToYCrCb420_16u8u_C3P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiBGR555ToYCbCr422_16u8u_C3C2R ( const Ipp16u* pSrc, int srcStep, Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiBGR555ToYCbCr422_16u8u_C3C2R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiBGR565ToYCrCb420_16u8u_C3P3R ( const Ipp16u* pSrc, int srcStep, Ipp8u* pDst[3], int dstStep[3], IppiSize roiSize )
{
    return ippiBGR565ToYCrCb420_16u8u_C3P3R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

inline IppStatus mfxiBGR565ToYCbCr422_16u8u_C3C2R ( const Ipp16u* pSrc, int srcStep, Ipp8u* pDst, int dstStep, IppiSize roiSize )
{
    return ippiBGR565ToYCbCr422_16u8u_C3C2R ( pSrc, srcStep, pDst, dstStep, roiSize );
}

#if __GNUC__
#pragma GCC diagnostic pop
#endif

#endif // __IPPCC2MFX_H__
