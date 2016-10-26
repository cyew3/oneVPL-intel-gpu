//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

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
    const Ipp8u *pSrc;                                          /* (const Ipp8u *) pointer to the source memory */
    size_t srcStep;                                             /* (SIZE_T) pitch of the source memory */
    Ipp8u *pDst;                                                /* (Ipp8u *) pointer to the destination memory */
    size_t dstStep;                                             /* (SIZE_T) pitch of the destination memory */

    Ipp32s hFraction;                                           /* (Ipp32s) horizontal fraction of interpolation */
    Ipp32s vFraction;                                           /* (Ipp32s) vertical fraction of interpolation */

    Ipp32s blockWidth;                                          /* (Ipp32s) width of destination block */
    Ipp32s blockHeight;                                         /* (Ipp32s) height of destination block */

    IppiPoint pointVector;
    IppiPoint pointBlockPos;

    IppiSize frameSize;                                         /* (IppiSize) frame size */

    // filled by internal part
    Ipp32s iType;                                               /* (Ipp32s) type of interpolation */

    Ipp32s xPos;                                                /* (Ipp32s) x coordinate of source data block */
    Ipp32s yPos;                                                /* (Ipp32s) y coordinate of source data block */
    Ipp32s dataWidth;                                           /* (Ipp32s) width of the used source data */
    Ipp32s dataHeight;                                          /* (Ipp32s) height of the used source data */

} H265InterpolationParams_8u;

typedef struct H265InterpolationParams_16u
{
    const Ipp16u *pSrc;                                          /* (const Ipp16u *) pointer to the source memory */
    size_t srcStep;                                             /* (SIZE_T) pitch of the source memory */
    Ipp16u *pDst;                                                /* (Ipp16u *) pointer to the destination memory */
    size_t dstStep;                                             /* (SIZE_T) pitch of the destination memory */

    Ipp32s hFraction;                                           /* (Ipp32s) horizontal fraction of interpolation */
    Ipp32s vFraction;                                           /* (Ipp32s) vertical fraction of interpolation */

    Ipp32s blockWidth;                                          /* (Ipp32s) width of destination block */
    Ipp32s blockHeight;                                         /* (Ipp32s) height of destination block */

    IppiPoint pointVector;
    IppiPoint pointBlockPos;

    IppiSize frameSize;                                         /* (IppiSize) frame size */

    // filled by internal part
    Ipp32s iType;                                               /* (Ipp32s) type of interpolation */

    Ipp32s xPos;                                                /* (Ipp32s) x coordinate of source data block */
    Ipp32s yPos;                                                /* (Ipp32s) y coordinate of source data block */
    Ipp32s dataWidth;                                           /* (Ipp32s) width of the used source data */
    Ipp32s dataHeight;                                          /* (Ipp32s) height of the used source data */

} H265InterpolationParams_16u;

// Check for frame boundaries and expand luma border values if necessary
IPPFUN(IppStatus, ippiInterpolateLumaBlock, (H265InterpolationParams_8u *interpolateInfo, Ipp8u *temporary_buffer));
// Check for frame boundaries and expand chroma border values if necessary
IPPFUN(IppStatus, ippiInterpolateChromaBlock, (H265InterpolationParams_8u *interpolateInfo, Ipp8u *temporary_buffer));

// Check for frame boundaries and expand luma border values if necessary
IPPFUN(IppStatus, ippiInterpolateLumaBlock, (H265InterpolationParams_8u *interpolateInfo, Ipp16u *temporary_buffer));
// Check for frame boundaries and expand chroma border values if necessary
IPPFUN(IppStatus, ippiInterpolateChromaBlock, (H265InterpolationParams_8u *interpolateInfo, Ipp16u *temporary_buffer));


#pragma warning(disable: 4100)

    // Set plane values inside of region of interest to val
    inline IppStatus SetPlane(Ipp8u value, Ipp8u* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        return ippiSet_8u_C1R(value, pDst, dstStep, roiSize);
    }

    // Copy frame plane inside of region of intereset
    inline IppStatus CopyPlane(Ipp8u* pSrc, Ipp32s srcStep, Ipp8u* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        return ippiCopy_8u_C1R(pSrc, srcStep, pDst, dstStep,
                              roiSize);
    }

    ///****************************************************************************************/
    // 16 bits functions
    ///****************************************************************************************/

    // Set plane values inside of region of interest to val
    inline IppStatus SetPlane(Ipp16u value, Ipp16u* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        if (!pDst)
            return ippStsNullPtrErr;

        return ippiSet_16s_C1R(value, (Ipp16s*)pDst, dstStep, roiSize);
    }

    // Copy frame plane inside of region of intereset
    inline IppStatus CopyPlane(const Ipp16u* pSrc, Ipp32s srcStep, Ipp16u* pDst, Ipp32s dstStep,
                              IppiSize roiSize)
    {
        if (!pSrc || !pDst)
            return ippStsNullPtrErr;

        return ippiCopy_16s_C1R((const Ipp16s*)pSrc, srcStep, (Ipp16s*)pDst, dstStep,
                        roiSize);
    }

#pragma warning(default: 4100)

} /* namespace UMC_H265_DECODER */

using namespace UMC_HEVC_DECODER;

#endif // __UMC_H265_DEC_IPP_LEVEL_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
