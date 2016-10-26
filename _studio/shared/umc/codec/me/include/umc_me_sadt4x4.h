//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2009 Intel Corporation. All Rights Reserved.
//

#include <umc_defs.h>
#ifdef UMC_ENABLE_ME

#ifndef _ME_SADT_H_
#define _ME_SADT_H_

#include "ippdefs.h"
#include "ippvc.h"

//temporary here: the code is added from H264 encoder

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER)) && !defined(_WIN32_WCE)
#define __ALIGN16 __declspec (align(16))
#else
#define __ALIGN16
#endif

#undef  ABS
#define ABS(a)           (((a) < 0) ? (-(a)) : (a))

//temporary here: the definitions is added from H264 encoder
#define BS_16x16  20
#define BS_16x8   18
#define BS_8x16   12
#define BS_8x8    10
#define BS_8x4     9
#define BS_4x8     6
#define BS_4x4     5
#define BS_4x2     4
#define BS_2x4     3
#define BS_2x2     2

Ipp32u SATD_8u_C1R(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step, Ipp32s width, Ipp32s height);
Ipp32u SATD_16u_C1R(const Ipp16u *pSrc1, Ipp32s src1Step, const Ipp16u *pSrc2, Ipp32s src2Step, Ipp32s width, Ipp32s height);

inline Ipp32s SATD16x16(const Ipp8u *pSource0, Ipp32s pitchBytes0, const Ipp8u *pSource1, Ipp32s pitchBytes1)
{
    return SATD_8u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 16, 16);
}

inline Ipp32s SATD16x8(const Ipp8u *pSource0, Ipp32s pitchBytes0, const Ipp8u *pSource1, Ipp32s pitchBytes1)
{
    return SATD_8u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 16, 8);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD16x8(const Ipp16u *pSource0, Ipp32s pitchBytes0, const Ipp16u *pSource1, Ipp32s pitchBytes1)
{
    return SATD_16u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 16, 8);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD8x16(const Ipp8u *pSource0, Ipp32s pitchBytes0, const Ipp8u *pSource1, Ipp32s pitchBytes1)
{
    return SATD_8u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 8, 16);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD8x16(const Ipp16u *pSource0, Ipp32s pitchBytes0, const Ipp16u *pSource1, Ipp32s pitchBytes1)
{
    return SATD_16u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 8, 16);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD8x8(const Ipp8u* pSrc0, Ipp32s srcStepBytes0, const Ipp8u* pSrc1, Ipp32s  srcStepBytes1)
{
    return SATD_8u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 8, 8);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD8x8(const Ipp16u* pSrc0, Ipp32s srcStepBytes0, const Ipp16u* pSrc1, Ipp32s srcStepBytes1)
{
    return SATD_16u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 8, 8);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD8x4(const Ipp8u* pSrc0, Ipp32s srcStepBytes0, const Ipp8u* pSrc1, Ipp32s  srcStepBytes1)
{
    return SATD_8u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 8, 4);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD8x4(const Ipp16u* pSrc0, Ipp32s srcStepBytes0, const Ipp16u* pSrc1, Ipp32s srcStepBytes1)
{
    return SATD_16u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 8, 4);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD4x8(const Ipp8u* pSrc0, Ipp32s srcStepBytes0, const Ipp8u* pSrc1, Ipp32s  srcStepBytes1)
{
    return SATD_8u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 4, 8);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD4x8(const Ipp16u* pSrc0, Ipp32s srcStepBytes0, const Ipp16u* pSrc1, Ipp32s srcStepBytes1)
{
    return SATD_16u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 4, 8);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD4x4(const Ipp8u *pSource0, Ipp32s pitchBytes0, const Ipp8u *pSource1, Ipp32s pitchBytes1)
{
    return SATD_8u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 4, 4);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD4x4(const Ipp16u *pSource0, Ipp32s pitchBytes0, const Ipp16u *pSource1, Ipp32s pitchBytes1)
{
    return SATD_16u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 4, 4);
}
#endif // BITDEPTH_9_12

template <class PixType> Ipp32s SATD(PixType *pCur, Ipp32s pitchPixelsCur, PixType *pRef, Ipp32s pitchPixelsRef, Ipp32s blockSize);
template <class PixType> Ipp32s SATD(PixType *pCur, Ipp32s pitchPixelsCur, PixType *pRef, Ipp32s pitchPixelsRef, Ipp32s blockSize)
{
    Ipp32s sad;
    switch (blockSize) {
        case BS_16x16:
            sad = SATD16x16(pCur, pitchPixelsCur*sizeof(PixType), pRef, pitchPixelsRef*sizeof(PixType));
            break;
        case BS_16x8:
            sad = SATD16x8(pCur, pitchPixelsCur*sizeof(PixType), pRef, pitchPixelsRef*sizeof(PixType));
            break;
        case BS_8x16:
            sad = SATD8x16(pCur, pitchPixelsCur*sizeof(PixType), pRef, pitchPixelsRef*sizeof(PixType));
            break;
        case BS_8x8:
            sad = SATD8x8(pCur, pitchPixelsCur*sizeof(PixType), pRef, pitchPixelsRef*sizeof(PixType));
            break;
        case BS_8x4:
            sad = SATD8x4(pCur, pitchPixelsCur*sizeof(PixType), pRef, pitchPixelsRef*sizeof(PixType));
            break;
        case BS_4x8:
            sad = SATD4x8(pCur, pitchPixelsCur*sizeof(PixType), pRef, pitchPixelsRef*sizeof(PixType));
            break;
        case BS_4x4:
            sad = SATD4x4(pCur, pitchPixelsCur*sizeof(PixType), pRef, pitchPixelsRef*sizeof(PixType));
            break;
        case BS_4x2:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s a0 = d0 + d2;
            Ipp32s a1 = d1 + d3;
            Ipp32s a2 = d0 - d2;
            Ipp32s a3 = d1 - d3;
            sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            d0 = pCur[2] - pRef[2];
            d1 = pCur[3] - pRef[3];
            d2 = pCur[pitchPixelsCur+2] - pRef[pitchPixelsRef+2];
            d3 = pCur[pitchPixelsCur+3] - pRef[pitchPixelsRef+3];
            a0 = d0 + d2;
            a1 = d1 + d3;
            a2 = d0 - d2;
            a3 = d1 - d3;
            sad += ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            break;
            }
        case BS_2x4:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s a0 = d0 + d2;
            Ipp32s a1 = d1 + d3;
            Ipp32s a2 = d0 - d2;
            Ipp32s a3 = d1 - d3;
            sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            pCur += pitchPixelsCur * 2;
            pRef += pitchPixelsRef * 2;
            d0 = pCur[0] - pRef[0];
            d1 = pCur[1] - pRef[1];
            d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            a0 = d0 + d2;
            a1 = d1 + d3;
            a2 = d0 - d2;
            a3 = d1 - d3;
            sad += ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            break;
            }
        case BS_2x2:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s a0 = d0 + d2;
            Ipp32s a1 = d1 + d3;
            Ipp32s a2 = d0 - d2;
            Ipp32s a3 = d1 - d3;
            sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            break;
            }
        default:
            sad = 0;
            break;
    }
    return sad;
}
#endif//_ME_SADT_H_
#endif
