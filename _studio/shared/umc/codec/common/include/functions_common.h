// Copyright (c) 2011-2018 Intel Corporation
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

#if !defined( __FUNCTIONS_COMMON_H__ )
#define __FUNCTIONS_COMMON_H__

#include "ippvc.h"
#include "vm_types.h"

#undef USE_INTRINSIC_OPT
#if defined(__INTEL_COMPILER) || (_MSC_VER >= 1500) || (defined(__GNUC__) && (__GNUC__ > 3) && defined(__SSSE3__))
#define USE_INTRINSIC_OPT
#include "tmmintrin.h" //merom-ssse3
// short names for casting intrinsics
#define _ps2pi _mm_castps_si128
#define _pi2ps _mm_castsi128_ps
#define _pi2pd _mm_castsi128_pd
#define _pd2pi _mm_castpd_si128
#endif

#define SATURATION(L,V,H) (((V) < (L)) ? (L) : (((V) > (H)) ? (H) : (V)))
#define ABS(A) (((A) < 0) ? -(A) : (A))
#define UNREF_PARAM(A) (A)=(A)

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
extern IppStatus cppiConvert_16s8u_C1R(const Ipp16s *pSrc, Ipp32s iSrcStride, Ipp32u iSrcBitsPerSample, Ipp8u *pDst, Ipp32s iDstStride, IppiSize size);
extern IppStatus cppiConvert_8u16s_C1R(const Ipp8u *pSrc, Ipp32s iSrcStride, Ipp32u iDstBitsPerSample, Ipp16s *pDst, Ipp32s iDstStride, IppiSize size);

extern IppStatus cppiSwapChannes_C3_I(Ipp8u *pSrc, Ipp32s iSrcStride, IppiSize srcSize);
extern IppStatus cppiSwapChannes_C4_I(Ipp8u *pSrc, Ipp32s iSrcStride, IppiSize srcSize);

extern IppStatus cppiBGR_8u_AC4C3(const Ipp8u *pSrc, Ipp32s iSrcStride, Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize);
extern IppStatus cppiBGR_8u_C3AC4(const Ipp8u *pSrc, Ipp32s iSrcStride, Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize);
extern IppStatus cppiBGR555ToBGR_16u8u_C3(const Ipp16u *pSrc, Ipp32s iSrcStride, Ipp8u *pDst, Ipp32s iDstStride,IppiSize srcSize);
extern IppStatus cppiBGR565ToBGR_16u8u_C3(const Ipp16u *pSrc, Ipp32s iSrcStride, Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize);

extern IppStatus cppiYCbCr41PTo420_8u_P1P3(const Ipp8u *pSrc, Ipp32s iSrcStride, Ipp8u *pDst[3], Ipp32s iDstStride[3], IppiSize srcSize);
extern IppStatus cppiYCbCr420To41P_8u_P3P1(const Ipp8u *pSrc[3], Ipp32s iSrcStride[3], Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize);
extern IppStatus cppiGrayToBGR_8u_P1C3R(const Ipp8u *pSrc, Ipp32s iSrcStride, Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize);
extern IppStatus cppiGrayToYCbCr420_8u_P1P3R(const Ipp8u *pSrc, Ipp32s iSrcStride, Ipp8u *pDst[3], Ipp32s iDstStride[3], IppiSize srcSize);
extern IppStatus cppiYCbCrToGray_8u_P3P1R(const Ipp8u *pSrc[3], Ipp32s iSrcStride[3], Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize);
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif


#endif /* __FUNCTIONS_COMMON_H__ */
