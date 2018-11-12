// Copyright (c) 2016-2018 Intel Corporation
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

#include "mfxvideo.h"
#include "ippvc.h"
#include <string.h>
#include <algorithm>

#include "mfx_h264_dispatcher.h"

namespace MFX_H264_PP
{

void H264_Dispatcher_sse::FilterDeblockingChromaEdge(Ipp8u* pSrcDst,
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
        H264_Dispatcher::FilterDeblockingChromaEdge(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth, chroma_format_idc, isDeblHor);
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

} // namespace MFX_H264_PP
