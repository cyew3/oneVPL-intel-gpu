//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2017 Intel Corporation. All Rights Reserved.
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
#include "umc_mutex.h"

enum
{
    COPY_SYS_TO_SYS = 0,
    COPY_SYS_TO_VIDEO = 1,
    COPY_VIDEO_TO_SYS = 2,
};

#include <immintrin.h>
inline void copyVideoToSys(const mfxU8* src, mfxU8* dst, int width)
{
    static const int item_size = 4*sizeof(__m128i);

    int align16 = (0x10 - (reinterpret_cast<size_t>(src) & 0xf)) & 0xf;
    for (int i = 0; i < align16; i++)
        *dst++ = *src++;

    int w = width - align16;
    if (w < 0)
        return;

    int width4 = w & (-item_size);

    __m128i * src_reg = (__m128i *)src;
    __m128i * dst_reg = (__m128i *)dst;

    int i = 0;
    for (; i < width4; i += item_size)
    {
        __m128i xmm0 = _mm_stream_load_si128(src_reg);
        __m128i xmm1 = _mm_stream_load_si128(src_reg+1);
        __m128i xmm2 = _mm_stream_load_si128(src_reg+2);
        __m128i xmm3 = _mm_stream_load_si128(src_reg+3);
        _mm_storeu_si128(dst_reg, xmm0);
        _mm_storeu_si128(dst_reg+1, xmm1);
        _mm_storeu_si128(dst_reg+2, xmm2);
        _mm_storeu_si128(dst_reg+3, xmm3);

        src_reg += 4;
        dst_reg += 4;
    }

    size_t tail_data_sz = w & (item_size - 1);
    if (tail_data_sz)
    {
        for (; tail_data_sz >= sizeof(__m128i); tail_data_sz -= sizeof(__m128i))
        {
            __m128i xmm0 = _mm_stream_load_si128(src_reg);
            _mm_storeu_si128(dst_reg, xmm0);
            src_reg += 1;
            dst_reg += 1;
        }

        src = (const mfxU8 *)src_reg;
        dst = (mfxU8 *)dst_reg;

        for (; tail_data_sz > 0; tail_data_sz--)
            *dst++ = *src++;
    }
}

template<typename T>
inline int mfxCopyRect(const T* pSrc, int srcStep, T* pDst, int dstStep, IppiSize roiSize, int flag)
{
    if (!pDst || !pSrc || roiSize.width < 0 || roiSize.height < 0 || srcStep < 0 || dstStep < 0)
        return -1;

    if (flag & COPY_VIDEO_TO_SYS)
    {
        for(int h = 0; h < roiSize.height; h++ )
        {
            copyVideoToSys((const mfxU8*)pSrc, (mfxU8*)pDst, roiSize.width*sizeof(T));
            pSrc = (T *)((mfxU8*)pSrc + srcStep);
            pDst = (T *)((mfxU8*)pDst + dstStep);
        }
    } else {
        for(int h = 0; h < roiSize.height; h++ )
        {
            std::copy(pSrc, pSrc + roiSize.width, pDst);
            pSrc = (T *)((mfxU8*)pSrc + srcStep);
            pDst = (T *)((mfxU8*)pDst + dstStep);
        }
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

        /* The purpose of mutex here is to make the Copy() atomic.
         * Without it CPU utilization grows dramatically due to cache trashing.
         */
        static UMC::Mutex mutex; // This is thread-safe since C++11
        UMC::AutomaticUMCMutex guard(mutex);

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

        roi.width >>= 1;
        if(rshift)
            ippiRShiftC_16u_C1IR(rshift, (mfxU16*)pDst, dstPitch,roi);
        if(lshift)
            ippiLShiftC_16u_C1IR(lshift, (mfxU16*)pDst, dstPitch,roi);

        return MFX_ERR_NONE;
    }
#endif
};

#endif // __FAST_COPY_H__
