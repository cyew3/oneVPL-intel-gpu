//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#ifndef __MFX_H265_OPTIMIZATION_H__
#define __MFX_H265_OPTIMIZATION_H__

//#include "mfx_h265_defs.h"

#if defined(_WIN32) || defined(_WIN64)
  #define H265_FORCEINLINE __forceinline
  #define H265_NONLINE __declspec(noinline)
#else
  #define H265_FORCEINLINE __attribute__((always_inline))
  #define H265_NONLINE __attribute__((noinline))
#endif

// This better be placed in some general/common header
#ifdef __INTEL_COMPILER
# define H265_RESTRICT __restrict
#elif defined _MSC_VER
# if _MSC_VER >= 1400
#  define H265_RESTRICT __restrict
# else
#  define H265_RESTRICT
# endif
#else
# define H265_RESTRICT
#endif


namespace MFX_HEVC_COMMON
{
    /* transform Inv */
    void IDST_4x4_SSE4(void *destPtr, const short *__restrict coeff, int destStride, int destSize);
    void IDCT_4x4_SSE4(void *destPtr, const short *__restrict coeff, int destStride, int destSize);
    void IDCT_8x8_SSE4(void *destPtr, const short *__restrict coeff, int destStride, int destSize);
    void IDCT_16x16_SSE4(void *destPtr, const short *__restrict coeff, int destStride, int destSize);
    void IDCT_32x32_SSE4(void *destPtr, const short *__restrict coeff, int destStride, int destSize);

    /* interpolation ??? */       
};

namespace MFX_HEVC_ENCODER
{
    /* transform Fwd - NIY */

    /* SAD */
    typedef IppStatus (__STDCALL *SADfunc8u)(
        const Ipp8u* pSrcCur,
        Ipp32s srcCurStep,
        const Ipp8u* pSrcRef,
        Ipp32s srcRefStep,
        Ipp32s* pDst,
        Ipp32s mcType);

    extern SADfunc8u* SAD_8u[17];
};

namespace MFX_HEVC_DECODER
{
};

#endif // __MFX_H265_OPTIMIZATION_H__
#endif // defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */

