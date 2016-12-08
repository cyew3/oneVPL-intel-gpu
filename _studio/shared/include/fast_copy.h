//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __FAST_COPY_H__
#define __FAST_COPY_H__

#include "ippdefs.h"

#if defined(_WIN32) || defined(_WIN64)
// suppress ipp depricated warning
#undef IPP_DEPRECATED
#define IPP_DEPRECATED(comment)
#endif

#include "ippi.h"
#include "mfx_trace.h"
#include "mfxdefs.h"
#include <algorithm>

enum
{
    COPY_SYS_TO_SYS = 0,
    COPY_SYS_TO_VIDEO = 1,
    COPY_VIDEO_TO_SYS = 2,
};

template<typename T>
inline int mfxCopyRect(const T* pSrc, int srcStep, T* pDst, int dstStep, IppiSize roiSize, int )
{
    if (!pDst || !pSrc || roiSize.width < 0 || roiSize.height < 0 || srcStep < 0 || dstStep < 0)
        return -1;

    for(int h = 0; h < roiSize.height; h++ ) {
        std::copy(pSrc, pSrc + roiSize.width, pDst);
        //memcpy_s(pDst, roiSize.width, pSrc,roiSize.width);
        pSrc = (T *)((unsigned char*)pSrc + srcStep);
        pDst = (T *)((unsigned char*)pDst + dstStep);
    }

    return 0;
}

#ifndef OPEN_SOURCE
template<>
inline int mfxCopyRect<mfxU8>(const mfxU8* pSrc, int srcStep, mfxU8* pDst, int dstStep, IppiSize roiSize, int flags)
{
    return ippiCopyManaged_8u_C1R(pSrc, srcStep, pDst, dstStep, roiSize, flags);
}
#endif

class FastCopy
{
public:
    // copy memory by streaming
    static mfxStatus Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi, int flag)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FastCopy::Copy");

        if (NULL == pDst || NULL == pSrc)
        {
            return MFX_ERR_NULL_PTR;
        }

        mfxCopyRect<mfxU8>(pSrc, srcPitch, pDst, dstPitch, roi, flag);

        return MFX_ERR_NONE;
    }
#if defined(_WIN32) || defined(_WIN64)
    // copy memory by streaming with shifting
    static mfxStatus CopyAndShift(mfxU16 *pDst, mfxU32 dstPitch, mfxU16 *pSrc, mfxU32 srcPitch, IppiSize roi, mfxU8 lshift, mfxU8 rshift, int flag)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FastCopy::Copy");

        if (NULL == pDst || NULL == pSrc)
        {
            return MFX_ERR_NULL_PTR;
        }

        mfxCopyRect<mfxU8>((mfxU8*)pSrc, srcPitch, (mfxU8*)pDst, dstPitch, roi, flag);
        if(rshift)
            ippiRShiftC_16u_C1IR(rshift, (mfxU16*)pDst, dstPitch,roi);
        if(lshift)
            ippiLShiftC_16u_C1IR(lshift, (mfxU16*)pDst, dstPitch,roi);

        return MFX_ERR_NONE;
    }
#endif
};

#endif // __FAST_COPY_H__
