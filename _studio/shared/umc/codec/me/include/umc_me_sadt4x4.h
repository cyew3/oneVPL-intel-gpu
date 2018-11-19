// Copyright (c) 2007-2018 Intel Corporation
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

uint32_t SATD_8u_C1R(const uint8_t *pSrc1, int32_t src1Step, const uint8_t *pSrc2, int32_t src2Step, int32_t width, int32_t height);
uint32_t SATD_16u_C1R(const uint16_t *pSrc1, int32_t src1Step, const uint16_t *pSrc2, int32_t src2Step, int32_t width, int32_t height);

inline int32_t SATD16x16(const uint8_t *pSource0, int32_t pitchBytes0, const uint8_t *pSource1, int32_t pitchBytes1)
{
    return SATD_8u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 16, 16);
}

inline int32_t SATD16x8(const uint8_t *pSource0, int32_t pitchBytes0, const uint8_t *pSource1, int32_t pitchBytes1)
{
    return SATD_8u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 16, 8);
}

#if defined BITDEPTH_9_12
inline int32_t SATD16x8(const uint16_t *pSource0, int32_t pitchBytes0, const uint16_t *pSource1, int32_t pitchBytes1)
{
    return SATD_16u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 16, 8);
}
#endif // BITDEPTH_9_12

inline int32_t SATD8x16(const uint8_t *pSource0, int32_t pitchBytes0, const uint8_t *pSource1, int32_t pitchBytes1)
{
    return SATD_8u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 8, 16);
}

#if defined BITDEPTH_9_12
inline int32_t SATD8x16(const uint16_t *pSource0, int32_t pitchBytes0, const uint16_t *pSource1, int32_t pitchBytes1)
{
    return SATD_16u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 8, 16);
}
#endif // BITDEPTH_9_12

inline int32_t SATD8x8(const uint8_t* pSrc0, int32_t srcStepBytes0, const uint8_t* pSrc1, int32_t  srcStepBytes1)
{
    return SATD_8u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 8, 8);
}

#if defined BITDEPTH_9_12
inline int32_t SATD8x8(const uint16_t* pSrc0, int32_t srcStepBytes0, const uint16_t* pSrc1, int32_t srcStepBytes1)
{
    return SATD_16u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 8, 8);
}
#endif // BITDEPTH_9_12

inline int32_t SATD8x4(const uint8_t* pSrc0, int32_t srcStepBytes0, const uint8_t* pSrc1, int32_t  srcStepBytes1)
{
    return SATD_8u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 8, 4);
}

#if defined BITDEPTH_9_12
inline int32_t SATD8x4(const uint16_t* pSrc0, int32_t srcStepBytes0, const uint16_t* pSrc1, int32_t srcStepBytes1)
{
    return SATD_16u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 8, 4);
}
#endif // BITDEPTH_9_12

inline int32_t SATD4x8(const uint8_t* pSrc0, int32_t srcStepBytes0, const uint8_t* pSrc1, int32_t  srcStepBytes1)
{
    return SATD_8u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 4, 8);
}

#if defined BITDEPTH_9_12
inline int32_t SATD4x8(const uint16_t* pSrc0, int32_t srcStepBytes0, const uint16_t* pSrc1, int32_t srcStepBytes1)
{
    return SATD_16u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 4, 8);
}
#endif // BITDEPTH_9_12

inline int32_t SATD4x4(const uint8_t *pSource0, int32_t pitchBytes0, const uint8_t *pSource1, int32_t pitchBytes1)
{
    return SATD_8u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 4, 4);
}

#if defined BITDEPTH_9_12
inline int32_t SATD4x4(const uint16_t *pSource0, int32_t pitchBytes0, const uint16_t *pSource1, int32_t pitchBytes1)
{
    return SATD_16u_C1R(pSource0, pitchBytes0, pSource1, pitchBytes1, 4, 4);
}
#endif // BITDEPTH_9_12

template <class PixType> int32_t SATD(PixType *pCur, int32_t pitchPixelsCur, PixType *pRef, int32_t pitchPixelsRef, int32_t blockSize);
template <class PixType> int32_t SATD(PixType *pCur, int32_t pitchPixelsCur, PixType *pRef, int32_t pitchPixelsRef, int32_t blockSize)
{
    int32_t sad;
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
            int32_t d0 = pCur[0] - pRef[0];
            int32_t d1 = pCur[1] - pRef[1];
            int32_t d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            int32_t d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            int32_t a0 = d0 + d2;
            int32_t a1 = d1 + d3;
            int32_t a2 = d0 - d2;
            int32_t a3 = d1 - d3;
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
            int32_t d0 = pCur[0] - pRef[0];
            int32_t d1 = pCur[1] - pRef[1];
            int32_t d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            int32_t d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            int32_t a0 = d0 + d2;
            int32_t a1 = d1 + d3;
            int32_t a2 = d0 - d2;
            int32_t a3 = d1 - d3;
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
            int32_t d0 = pCur[0] - pRef[0];
            int32_t d1 = pCur[1] - pRef[1];
            int32_t d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            int32_t d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            int32_t a0 = d0 + d2;
            int32_t a1 = d1 + d3;
            int32_t a2 = d0 - d2;
            int32_t a3 = d1 - d3;
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
