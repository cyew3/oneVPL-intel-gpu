/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
*/

/* reference CPU implementation of fast gradient analysis for intra prediction
 * software fallback when Cm version not available
 * original code: mfx_lib\genx\h265_encode\src\test_analyze_gradient2.cpp
 */

#include <string.h>
#include <math.h>

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSSE3)

#ifdef MAX
#undef MAX
#endif
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#ifdef MIN
#undef MIN
#endif
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define CLIPVAL(VAL, MINVAL, MAXVAL) MAX(MINVAL, MIN(MAXVAL, VAL))

namespace MFX_HEVC_PP
{

template <class PixType>
static PixType GetPel(const PixType *frame, Ipp32s x, Ipp32s y, Ipp32s width, Ipp32s height, Ipp32s pitch)
{
    width; height; // source frame is now padded, no need in checking boundaries
    return frame[y * pitch + x];
}

static float atan2_fast(float y, float x)
{
    double a0;
    double a1;
    float atan2;
    const float CM_CONST_PI = 3.14159f;
    const float M_DBL_EPSILON = 0.00001f;

    if (y >= 0) {
        a0 = CM_CONST_PI * 0.5;
        a1 = 0;
    }
    else
    {
        a0 = CM_CONST_PI * 1.5;
        a1 = CM_CONST_PI * 2;
    }

    if (x < 0) {
        a1 = CM_CONST_PI;
    }

    float xy = x * y;
    float x2 = x * x;
    float y2 = y * y;

    a0 -= (xy / (y2 + 0.28f * x2 + M_DBL_EPSILON));
    a1 += (xy / (x2 + 0.28f * y2 + M_DBL_EPSILON));

    if (y2 <= x2) {
        atan2 = a1;
    }
    else {
        atan2 = a0;
    }

    //atan2 = matrix<float,R,C>(atan2, (flags & SAT));
    return atan2;
}

template <class PixType, class HistType>
static void BuildHistogram2(const PixType *frame, Ipp32s blockX, Ipp32s blockY, Ipp32s w, Ipp32s h,
                            Ipp32s p, mfxI32 blockSize, HistType *histogram)
{
    for (mfxI32 y = blockY; y < blockY + blockSize; y++) {
        for (mfxI32 x = blockX; x < blockX + blockSize; x++) {
            mfxI16 dy = GetPel(frame, x-1, y-1, w, h, p) + 2 * GetPel(frame, x-1, y+0, w, h, p) + GetPel(frame, x-1, y+1, w, h, p)
                       -GetPel(frame, x+1, y-1, w, h, p) - 2 * GetPel(frame, x+1, y+0, w, h, p) - GetPel(frame, x+1, y+1, w, h, p);
            mfxI16 dx = GetPel(frame, x-1, y+1, w, h, p) + 2 * GetPel(frame, x+0, y+1, w, h, p) + GetPel(frame, x+1, y+1, w, h, p)
                       -GetPel(frame, x-1, y-1, w, h, p) - 2 * GetPel(frame, x+0, y-1, w, h, p) - GetPel(frame, x+1, y-1, w, h, p);
            //mfxU8  ang = FindNormalAngle(dx, dy);

            float fdx = dx;
            float fdy = dy;
            float y2 = fdy * fdy;
            float x2 = fdx * fdx;
            float angf;

            if (dx == 0 && dy == 0) {
                angf = 0;
            }
            else {
                angf = atan2_fast(fdy, fdx);
            }

            const float CM_CONST_PI = 3.14159f;
            if (y2 > x2 && dy > 0) angf += CM_CONST_PI;
            if (y2 <= x2 && dx > 0 && dy >= 0) angf += CM_CONST_PI;
            if (y2 <= x2 && dx > 0 && dy < 0) angf -= CM_CONST_PI;

            angf -= 0.75 * CM_CONST_PI;
            mfxI8 ang2 = angf * 10.504226 + 2;
            if (dx == 0 && dy == 0) ang2 = 0;

            mfxU16 amp = (mfxI16)(abs(dx) + abs(dy));
            histogram[ang2] += amp;
        }
    }
}

template <class PixType, class HistType>
void MAKE_NAME(h265_AnalyzeGradient)(const PixType *src, Ipp32s pitch, HistType *hist4, HistType *hist8,
                                     Ipp32s width, Ipp32s height)
{
    Ipp32s blockSize = 4;
    for (Ipp32s y = 0; y < height / blockSize; y++, hist4 += (width / blockSize) * 40) {
        for (Ipp32s x = 0; x < width / blockSize; x++) {
            memset(hist4 + x * 40, 0, sizeof(*hist4) * 40);
            BuildHistogram2(src, x * blockSize, y * blockSize, width, height, pitch, blockSize, hist4 + x * 40);
        }
    }

    blockSize = 8;
    for (Ipp32s y = 0; y < height / blockSize; y++, hist8 += (width / blockSize) * 40) {
        for (Ipp32s x = 0; x < width / blockSize; x++) {
            memset(hist8 + x * 40, 0, sizeof(*hist8) * 40);
            BuildHistogram2(src, x * blockSize, y * blockSize, width, height, pitch, blockSize, hist8 + x * 40);
        }
    }
}

template void MAKE_NAME(h265_AnalyzeGradient)<Ipp8u, Ipp16u> (const Ipp8u  *src, Ipp32s pitch, Ipp16u *hist4, Ipp16u *hist8, Ipp32s width, Ipp32s height);
template void MAKE_NAME(h265_AnalyzeGradient)<Ipp16u, Ipp32u>(const Ipp16u *src, Ipp32s pitch, Ipp32u *hist4, Ipp32u *hist8, Ipp32s width, Ipp32s height);

#define MAX_CU_SIZE 64

template <class PixType>
void MAKE_NAME(h265_ComputeRsCs)(const PixType *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height)
{
    for(Ipp32s i=0; i<height; i+=4)
    {
        for(Ipp32s j=0; j<width; j+=4)
        {
            Ipp32s Rs=0;
            Ipp32s Cs=0;
            for(Ipp32s k=0; k<4; k++) 
            {
                for(Ipp32s l=0; l<4; l++)
                {
                    Ipp32s temp = ySrc[(i+k)*pitchSrc+(j+l)]-ySrc[(i+k)*pitchSrc+(j+l-1)];
                    Cs += temp*temp;

                    temp = ySrc[(i+k)*pitchSrc+(j+l)]-ySrc[(i+k-1)*pitchSrc+(j+l)];
                    Rs += temp*temp;
                }
            }
            lcuCs[(i>>2)*pitchRsCs+(j>>2)] = Cs>>4;
            lcuRs[(i>>2)*pitchRsCs+(j>>2)] = Rs>>4;
        }
    }
}

template void MAKE_NAME(h265_ComputeRsCs)<Ipp8u> (const Ipp8u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height);
template void MAKE_NAME(h265_ComputeRsCs)<Ipp16u>(const Ipp16u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height);

}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSSE3)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
