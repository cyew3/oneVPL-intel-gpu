/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_DEC_IPP_WRAP_H
#define __UMC_H265_DEC_IPP_WRAP_H

#include "ippi.h"
#include "ipps.h"
#include "vm_debug.h"
#include "umc_h265_dec_defs_dec.h"

namespace UMC_HEVC_DECODER
{

#pragma warning(disable: 4100)

    inline IppStatus SetPlane_H265(Ipp8u val, Ipp8u* pDst, Ipp32s len)
    {
        if (!pDst)
            return ippStsNullPtrErr;

        return ippsSet_8u(val, pDst, len);
    }

    inline IppStatus CopyPlane_H265(const Ipp16s *pSrc, Ipp16s *pDst, Ipp32s len)
    {
        if (!pSrc || !pDst)
            return ippStsNullPtrErr;

        return ippsCopy_16s(pSrc, pDst, len);
    }

    inline IppStatus SetPlane_H265(Ipp8u value, Ipp8u* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        return ippiSet_8u_C1R(value, pDst, dstStep, roiSize);
    }

    inline IppStatus CopyPlane_H265(Ipp16s* pSrc, Ipp32s srcStep, Ipp16s* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        return ippiCopy_16s_C1R(pSrc, srcStep, pDst, dstStep,
                              roiSize);
    }

    ///****************************************************************************************/
    // 16 bits functions
    ///****************************************************************************************/
    inline void SetPlane_H265(Ipp16u val, Ipp16u* pDst, Ipp32s len)
    {
        ippsSet_16s(val, (Ipp16s *)pDst, len);
    }

    inline void CopyPlane_H265(const Ipp16u *pSrc, Ipp16u *pDst, Ipp32s len)
    {
        ippsCopy_16s((const Ipp16s *)pSrc, (Ipp16s *)pDst, len);
    }


    inline IppStatus SetPlane_H265(Ipp16u value, Ipp16u* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        if (!pDst)
            return ippStsNullPtrErr;

        return ippiSet_16s_C1R(value, (Ipp16s*)pDst, dstStep, roiSize);
    }

    inline IppStatus CopyPlane_H265(const Ipp16u* pSrc, Ipp32s srcStep, Ipp16u* pDst, Ipp32s dstStep,
                              IppiSize roiSize)
    {
        if (!pSrc || !pDst)
            return ippStsNullPtrErr;

        return ippiCopy_16s_C1R((const Ipp16s*)pSrc, srcStep, (Ipp16s*)pDst, dstStep,
                        roiSize);
    }


#pragma warning(default: 4100)

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_DEC_IPP_WRAP_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
