//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_IPP_LEVEL_H
#define __UMC_H264_DEC_IPP_LEVEL_H

#include "ippvc.h"

namespace UMC_H264_DECODER
{

extern void ConvertNV12ToYV12(const Ipp8u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, Ipp8u *pSrcDstUPlane, Ipp8u *pSrcDstVPlane, const Ipp32u _srcdstUStep, IppiSize roi);
extern void ConvertYV12ToNV12(const Ipp8u *pSrcDstUPlane, const Ipp8u *pSrcDstVPlane, const Ipp32u _srcdstUStep, Ipp8u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, IppiSize roi);

extern void ConvertNV12ToYV12(const Ipp16u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, Ipp16u *pSrcDstUPlane, Ipp16u *pSrcDstVPlane, const Ipp32u _srcdstUStep, IppiSize roi);
extern void ConvertYV12ToNV12(const Ipp16u *pSrcDstUPlane, const Ipp16u *pSrcDstVPlane, const Ipp32u _srcdstUStep, Ipp16u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, IppiSize roi);

#define   IPPFUN(type,name,arg)   extern type __STDCALL name arg

IPPAPI(IppStatus, own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s, (Ipp32u **ppBitStream,
    Ipp32s *pOffset,
    Ipp16s *pNumCoeff,
    Ipp16s **ppPosCoefbuf,
    Ipp32u uVLCSelect,
    Ipp16s uMaxNumCoeff,
    const Ipp32s **ppTblCoeffToken,
    const Ipp32s **ppTblTotalZeros,
    const Ipp32s **ppTblRunBefore,
    const Ipp32s *pScanMatrix,
    Ipp32s scanIdxStart,
    Ipp32s scanIdxEnd))


}; // UMC_H264_DECODER

using namespace UMC_H264_DECODER;

#endif // __UMC_H264_DEC_IPP_LEVEL_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
