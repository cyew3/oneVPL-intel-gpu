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

static mfxU8 GetPel(const mfxU8 *frame, mfxI32 x, mfxI32 y, mfxI32 width, mfxI32 height, mfxI32 pitch)
{
    x = CLIPVAL(x, 0, width - 1);
    y = CLIPVAL(y, 0, height - 1);
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

static void BuildHistogram2(const mfxU8 *frame, mfxI32 blockX, mfxI32 blockY, mfxI32 w, mfxI32 h, mfxI32 p, mfxI32 blockSize,
                    mfxU16 *histogram)
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

void MAKE_NAME(h265_AnalyzeGradient_8u)(const mfxU8 *inData, mfxU16 *outData4, mfxU16 *outData8, mfxI32 width, mfxI32 height, mfxI32 pitch)
{
    mfxU32 blockSize;

    blockSize = 4;
    for (mfxI32 y = 0; y < height / blockSize; y++, outData4 += (width / blockSize) * 40) {
        for (mfxI32 x = 0; x < width / blockSize; x++) {
            memset(outData4 + x * 40, 0, sizeof(*outData4) * 40);
            BuildHistogram2(inData, x * blockSize, y * blockSize, width, height, pitch, blockSize, outData4 + x * 40);
        }
    }

    blockSize = 8;
    for (mfxI32 y = 0; y < height / blockSize; y++, outData8 += (width / blockSize) * 40) {
        for (mfxI32 x = 0; x < width / blockSize; x++) {
            memset(outData8 + x * 40, 0, sizeof(*outData8) * 40);
            BuildHistogram2(inData, x * blockSize, y * blockSize, width, height, pitch, blockSize, outData8 + x * 40);
        }
    }
}

}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSSE3)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
