/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)

#define Saturate(min_val, max_val, val) IPP_MAX((min_val), IPP_MIN((max_val), (val)))

namespace MFX_HEVC_ENCODER
{
    void h265_QuantFwd_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift)
    {
        Ipp8s  sign;
        Ipp32s qval;

        for (Ipp32s i = 0; i < len; i++)
        {
            sign = (Ipp8s) (pSrc[i] < 0 ? -1 : 1);

            qval = (sign * pSrc[i] * scale + offset) >> shift;

            pDst[i] = (Ipp16s)Saturate(-32768, 32767, sign*qval);
        }

    } // void h265_QuantFwd_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scaleLevel, int scaleOffset, int scale)


    Ipp32s h265_QuantFwd_SBH_16s(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift)
    {
        Ipp8s  sign;
        Ipp32s qval;
        Ipp32s abs_sum = 0;

        for (Ipp32s i = 0; i < len; i++)
        {
            sign = (Ipp8s) (pSrc[i] < 0 ? -1 : 1);

            qval = (sign * pSrc[i] * scale + offset) >> shift;

            pDst[i] = (Ipp16s)Saturate(-32768, 32767, sign*qval);

            pDelta[i] = (Ipp32s)( ((Ipp64s)abs(pSrc[i]) * scale - (qval<<shift) )>> (shift-8) );

            abs_sum += qval;
        }

        return abs_sum;

    } // Ipp32s h265_QuantFwd_SBH_16s(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scaleLevel, int scaleOffset, int scale)

} // end namespace MFX_HEVC_ENCODER

#endif // #if defined (MFX_TARGET_OPTIMIZATION_PX) || (MFX_TARGET_OPTIMIZATION_SSE4) || (MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */
