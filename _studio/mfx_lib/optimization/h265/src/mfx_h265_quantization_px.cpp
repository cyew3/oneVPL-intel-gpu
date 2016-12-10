//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

// should be compiled for all platforms
//#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSSE3) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM) || defined(MFX_TARGET_OPTIMIZATION_AUTO)

#define Saturate(min_val, max_val, val) IPP_MAX((min_val), IPP_MIN((max_val), (val)))

// use _px for SSSE3 target (_sse version requires SSE4.1)
#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSSE3)

namespace MFX_HEVC_PP
{
    Ipp8u H265_FASTCALL MAKE_NAME(h265_QuantFwd_16s)(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift)
    {
        Ipp32s cbf = 0;
#ifdef __INTEL_COMPILER
#pragma ivdep
#pragma vector always
#endif
        for (Ipp32s i = 0; i < len; i++) {
            Ipp32s sign = pSrc[i] >> 15;
            Ipp32s aval = abs((Ipp32s)pSrc[i]); // remove sign
            Ipp32s qval = (aval * scale + offset) >> shift;
            qval = (qval ^ sign) - sign; // restore sign
            cbf |= qval;
            pDst[i] = (Ipp16s)Saturate(-32768, 32767, qval);
        }        
        return (Ipp8u)!!cbf;
    }


    Ipp32s H265_FASTCALL MAKE_NAME(h265_QuantFwd_SBH_16s)(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift)
    {
        Ipp32s sign;
        Ipp32s aval;
        Ipp32s qval;
        Ipp32s abs_sum = 0;

        for (Ipp32s i = 0; i < len; i++)
        {
            sign = pSrc[i] >> 15;

            aval = abs((Ipp32s)pSrc[i]);        // remove sign
            qval = (aval * scale + offset) >> shift;

            pDelta[i] = (Ipp32s)( ((Ipp64s)abs(pSrc[i]) * scale - (qval<<shift) )>> (shift-8) );
            abs_sum += qval;

            qval = (qval ^ sign) - sign;        // restore sign
            pDst[i] = (Ipp16s)Saturate(-32768, 32767, qval);
        }

        return abs_sum;

    } // Ipp32s h265_QuantFwd_SBH_16s(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scaleLevel, int scaleOffset, int scale)

    Ipp64s H265_FASTCALL MAKE_NAME(h265_Quant_zCost_16s)(const Ipp16s* pSrc, Ipp16u* qLevels, Ipp64s* zlCosts, Ipp32s len, Ipp32s qScale, Ipp32s qoffset, Ipp32s qbits, Ipp32s rdScale0)
    {
        Ipp32s i, alvl;
        Ipp64s totZlCost = 0;
        for (i = 0; i < len; i ++) 
        {
            alvl          = abs( pSrc[ i ] );
            qLevels[ i ]  = (Ipp16u)(( alvl*qScale + qoffset )>>qbits);
            zlCosts[ i ]  = (Ipp64s)(alvl * alvl) * rdScale0;
            totZlCost += zlCosts[ i ];
        }
        return totZlCost;
    }

} // end namespace MFX_HEVC_PP

#endif // defined(MFX_TARGET_OPTIMIZATION_AUTO) || defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_MAKENAME_SSSE3) && defined(MFX_TARGET_OPTIMIZATION_SSSE3)

namespace MFX_HEVC_PP
{
    void H265_FASTCALL h265_QuantInv_16s_px(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift)
    {
#ifdef __INTEL_COMPILER
        if (0 == (scale >> 15)) // ML: fast path for 16-bit scale
        {
            Ipp16s scale_16s = (Ipp16s)scale;
#pragma ivdep
#pragma vector always
            for (Ipp32s n = 0; n < len; n++)
            {
                // clipped when decoded
                Ipp32s coeffQ = (pSrc[n] * scale_16s + offset) >> shift;
                pDst[n] = (Ipp16s)Saturate(-32768, 32767, coeffQ);
            }
        }
        else
#pragma ivdep
#pragma vector always
#endif
            for (Ipp32s n = 0; n < len; n++)
            {
                // clipped when decoded
                Ipp32s coeffQ = (pSrc[n] * scale + offset) >> shift;
                pDst[n] = (Ipp16s)Saturate(-32768, 32767, coeffQ);
            }

    } // void h265_QuantInv_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift)

    
    void h265_QuantInv_ScaleList_LShift_16s(const Ipp16s* pSrc, const Ipp16s* pScaleList, Ipp16s* pDst, int len, int shift)
    {
#ifdef __INTEL_COMPILER
#pragma ivdep
#pragma vector always
#endif
        for (Ipp32s n = 0; n < len; n++)
        {
            // clipped when decoded
            Ipp32s CoeffQ   = Saturate(-32768, 32767, pSrc[n] * pScaleList[n]); 
            pDst[n] = (Ipp16s)Saturate(-32768, 32767, CoeffQ << shift );
        }

    } // void h265_QuantInv_ScaleList_LShift_16s(const Ipp16s* pSrc, const Ipp16s* pScaleList, Ipp16s* pDst, int len, int shift)


    void h265_QuantInv_ScaleList_RShift_16s(const Ipp16s* pSrc, const Ipp16s* pScaleList, Ipp16s* pDst, int len, int offset, int shift)
    {
#ifdef __INTEL_COMPILER
#pragma ivdep
#pragma vector always
#endif
        for (Ipp32s n = 0; n < len; n++)
        {
            // clipped when decoded
            Ipp32s coeffQ = ((pSrc[n] * pScaleList[n]) + offset) >> shift;
            pDst[n] = (Ipp16s)Saturate(-32768, 32767, coeffQ);
        }

    } // void h265_QuantInv_ScaleList_RShift_16s(const Ipp16s* pSrc, const Ipp16s* pScaleList, Ipp16s* pDst, int len, int offset, int shift)

} // namespace MFX_HEVC_PP

//#endif // #if defined (MFX_TARGET_OPTIMIZATION_PX) || (MFX_TARGET_OPTIMIZATION_SSE4) || (MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */
