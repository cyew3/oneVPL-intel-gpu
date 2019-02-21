// Copyright (c) 2003-2019 Intel Corporation
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

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#ifndef __UMC_H264_DEC_IPP_LEVEL_H
#define __UMC_H264_DEC_IPP_LEVEL_H

#include "ippvc.h"

namespace UMC_H264_DECODER
{

extern void ConvertNV12ToYV12(const uint8_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, uint8_t *pSrcDstUPlane, uint8_t *pSrcDstVPlane, const uint32_t _srcdstUStep, mfxSize roi);
extern void ConvertYV12ToNV12(const uint8_t *pSrcDstUPlane, const uint8_t *pSrcDstVPlane, const uint32_t _srcdstUStep, uint8_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, mfxSize roi);

extern void ConvertNV12ToYV12(const uint16_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, uint16_t *pSrcDstUPlane, uint16_t *pSrcDstVPlane, const uint32_t _srcdstUStep, mfxSize roi);
extern void ConvertYV12ToNV12(const uint16_t *pSrcDstUPlane, const uint16_t *pSrcDstVPlane, const uint32_t _srcdstUStep, uint16_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, mfxSize roi);

#define   IPPFUN(type,name,arg)   extern type __STDCALL name arg

IPPAPI(IppStatus, own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s, (uint32_t **ppBitStream,
    int32_t *pOffset,
    int16_t *pNumCoeff,
    int16_t **ppPosCoefbuf,
    uint32_t uVLCSelect,
    int16_t uMaxNumCoeff,
    const int32_t **ppTblCoeffToken,
    const int32_t **ppTblTotalZeros,
    const int32_t **ppTblRunBefore,
    const int32_t *pScanMatrix,
    int32_t scanIdxStart,
    int32_t scanIdxEnd))


}; // UMC_H264_DECODER

using namespace UMC_H264_DECODER;

#endif // __UMC_H264_DEC_IPP_LEVEL_H
#endif // MFX_ENABLE_H264_VIDEO_DECODE
