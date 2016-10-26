//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

#include "mfxvideo.h"
#include "ippvc.h"
#include <string.h>
#include <algorithm>

#include "mfx_dispatcher.h"

namespace MFX_PP
{

void MFX_Dispatcher_sse::FilterDeblockingChromaEdge(Ipp8u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc,
                                                Ipp32u  isDeblHor)
{
    if (chroma_format_idc == 2)
    {
        MFX_Dispatcher::FilterDeblockingChromaEdge(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth, chroma_format_idc, isDeblHor);
        return;
    }

    IppiFilterDeblock_8u info;
    info.pSrcDstPlane = (Ipp8u*)pSrcDst;
    info.srcDstStep = srcdstStep;
    info.pAlpha = pAlpha;
    info.pBeta = pBeta;
    info.pThresholds = pThresholds;
    info.pBs = pBS;

    if (isDeblHor)
    {
        ippiFilterDeblockingChroma_HorEdge_H264_8u_C2I(&info);
    }
    else
    {
        ippiFilterDeblockingChroma_VerEdge_H264_8u_C2I(&info);
    }
}

} // namespace MFX_PP
