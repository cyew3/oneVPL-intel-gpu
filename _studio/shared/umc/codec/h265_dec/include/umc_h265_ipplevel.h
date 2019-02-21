// Copyright (c) 2013-2019 Intel Corporation
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

#include "umc_defs.h"
#if defined (MFX_ENABLE_H265_VIDEO_DECODE)

#ifndef __UMC_H265_DEC_IPP_LEVEL_H
#define __UMC_H265_DEC_IPP_LEVEL_H

#include "ippi.h"
#include "ipps.h"
#include "memory.h"
#include <sys/types.h> /* for size_t declaration on Android */
#include <string.h> /* for memset and memcpy on Android */

namespace UMC_HEVC_DECODER
{
#define IPP_UNREFERENCED_PARAMETER(p) (p)=(p)
#define   IPPFUN(type,name,arg)   extern type __CDECL name arg

typedef struct H265InterpolationParams_8u
{
    const uint8_t *pSrc;                                          /* (const uint8_t *) pointer to the source memory */
    size_t srcStep;                                             /* (SIZE_T) pitch of the source memory */
    uint8_t *pDst;                                                /* (uint8_t *) pointer to the destination memory */
    size_t dstStep;                                             /* (SIZE_T) pitch of the destination memory */

    int32_t hFraction;                                           /* (int32_t) horizontal fraction of interpolation */
    int32_t vFraction;                                           /* (int32_t) vertical fraction of interpolation */

    int32_t blockWidth;                                          /* (int32_t) width of destination block */
    int32_t blockHeight;                                         /* (int32_t) height of destination block */

    IppiPoint pointVector;
    IppiPoint pointBlockPos;

    mfxSize frameSize;                                         /* (mfxSize) frame size */

    // filled by internal part
    int32_t iType;                                               /* (int32_t) type of interpolation */

    int32_t xPos;                                                /* (int32_t) x coordinate of source data block */
    int32_t yPos;                                                /* (int32_t) y coordinate of source data block */
    int32_t dataWidth;                                           /* (int32_t) width of the used source data */
    int32_t dataHeight;                                          /* (int32_t) height of the used source data */

} H265InterpolationParams_8u;

typedef struct H265InterpolationParams_16u
{
    const uint16_t *pSrc;                                          /* (const uint16_t *) pointer to the source memory */
    size_t srcStep;                                             /* (SIZE_T) pitch of the source memory */
    uint16_t *pDst;                                                /* (uint16_t *) pointer to the destination memory */
    size_t dstStep;                                             /* (SIZE_T) pitch of the destination memory */

    int32_t hFraction;                                           /* (int32_t) horizontal fraction of interpolation */
    int32_t vFraction;                                           /* (int32_t) vertical fraction of interpolation */

    int32_t blockWidth;                                          /* (int32_t) width of destination block */
    int32_t blockHeight;                                         /* (int32_t) height of destination block */

    IppiPoint pointVector;
    IppiPoint pointBlockPos;

    mfxSize frameSize;                                         /* (mfxSize) frame size */

    // filled by internal part
    int32_t iType;                                               /* (int32_t) type of interpolation */

    int32_t xPos;                                                /* (int32_t) x coordinate of source data block */
    int32_t yPos;                                                /* (int32_t) y coordinate of source data block */
    int32_t dataWidth;                                           /* (int32_t) width of the used source data */
    int32_t dataHeight;                                          /* (int32_t) height of the used source data */

} H265InterpolationParams_16u;

// Check for frame boundaries and expand luma border values if necessary
IPPFUN(IppStatus, ippiInterpolateLumaBlock, (H265InterpolationParams_8u *interpolateInfo, uint8_t *temporary_buffer));
// Check for frame boundaries and expand chroma border values if necessary
IPPFUN(IppStatus, ippiInterpolateChromaBlock, (H265InterpolationParams_8u *interpolateInfo, uint8_t *temporary_buffer));

// Check for frame boundaries and expand luma border values if necessary
IPPFUN(IppStatus, ippiInterpolateLumaBlock, (H265InterpolationParams_8u *interpolateInfo, uint16_t *temporary_buffer));
// Check for frame boundaries and expand chroma border values if necessary
IPPFUN(IppStatus, ippiInterpolateChromaBlock, (H265InterpolationParams_8u *interpolateInfo, uint16_t *temporary_buffer));

// turn off the "unreferenced formal parameter" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4100)
#endif

    // Set plane values inside of region of interest to val
    inline IppStatus SetPlane(uint8_t value, uint8_t* pDst, int32_t dstStep,
                              mfxSize roiSize )
    {
        return ippiSet_8u_C1R(value, pDst, dstStep, roiSize);
    }

    // Copy frame plane inside of region of intereset
    inline IppStatus CopyPlane(uint8_t* pSrc, int32_t srcStep, uint8_t* pDst, int32_t dstStep,
                              mfxSize roiSize )
    {
        return ippiCopy_8u_C1R(pSrc, srcStep, pDst, dstStep,
                              roiSize);
    }

    ///****************************************************************************************/
    // 16 bits functions
    ///****************************************************************************************/

    // Set plane values inside of region of interest to val
    inline IppStatus SetPlane(uint16_t value, uint16_t* pDst, int32_t dstStep,
                              mfxSize roiSize )
    {
        if (!pDst)
            return ippStsNullPtrErr;

        return ippiSet_16s_C1R(value, (int16_t*)pDst, dstStep, roiSize);
    }

    // Copy frame plane inside of region of intereset
    inline IppStatus CopyPlane(const uint16_t* pSrc, int32_t srcStep, uint16_t* pDst, int32_t dstStep,
                              mfxSize roiSize)
    {
        if (!pSrc || !pDst)
            return ippStsNullPtrErr;

        return ippiCopy_16s_C1R((const int16_t*)pSrc, srcStep, (int16_t*)pDst, dstStep,
                        roiSize);
    }

// restore the "unreferenced formal parameter" warning
#ifdef _MSVC_LANG
#pragma warning(default: 4100)
#endif

} /* namespace UMC_H265_DECODER */

using namespace UMC_HEVC_DECODER;

#endif // __UMC_H265_DEC_IPP_LEVEL_H
#endif // MFX_ENABLE_H265_VIDEO_DECODE
