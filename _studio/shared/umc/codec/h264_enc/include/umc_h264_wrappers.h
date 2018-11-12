// Copyright (c) 2008-2018 Intel Corporation
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

#include "umc_video_data.h"
#include "umc_video_processing.h"

#define UMC_CWRAPPER_FUNC(RES, TYPE, FUNC, PROTO, PTR, ARGS) \
    inline RES TYPE##_##FUNC PROTO { return ((UMC::TYPE *)PTR)->FUNC ARGS; } \

#define UMC_CWRAPPER_CTOR(TYPE) \
    inline void TYPE##_Create(void* p) { new (p) UMC::TYPE(); } \

#define UMC_CWRAPPER_DTOR(TYPE) \
    inline void TYPE##_Destroy(void* p) { ((UMC::TYPE *)p)->~TYPE(); } \

UMC_CWRAPPER_CTOR(VideoData)
UMC_CWRAPPER_DTOR(VideoData)

UMC_CWRAPPER_CTOR(H264EncoderParams)
UMC_CWRAPPER_DTOR(H264EncoderParams)

UMC_CWRAPPER_FUNC(UMC::Status, VideoData, Init,
         (void* p, Ipp32s iWidth, Ipp32s iHeight, ColorFormat cFormat, Ipp32s iBitDepth),
         p, (iWidth, iHeight, cFormat, iBitDepth))

UMC_CWRAPPER_FUNC(UMC::ColorFormat, VideoData, GetColorFormat,
         (void* p),
         p, ())

UMC_CWRAPPER_FUNC(UMC::FrameType, VideoData, GetFrameType,
         (void* p),
         p, ())

UMC_CWRAPPER_FUNC(UMC::Status, VideoData, SetFrameType,
         (void* p, UMC::FrameType ft),
         p, (ft))

UMC_CWRAPPER_FUNC(Ipp32s, VideoData, GetNumPlanes,
         (void* p),
         p, ())

UMC_CWRAPPER_FUNC(void*, VideoData, GetPlanePointer,
         (void* p, Ipp32s iPlaneNumber),
         p, (iPlaneNumber))

UMC_CWRAPPER_FUNC(UMC::Status, VideoData, SetPlanePointer,
         (void* p, void *pDest, Ipp32s iPlaneNumber),
         p, (pDest, iPlaneNumber))

UMC_CWRAPPER_FUNC(Ipp32s, VideoData, GetPlaneBitDepth,
         (void* p, Ipp32s iPlaneNumber),
         p, (iPlaneNumber))

UMC_CWRAPPER_FUNC(UMC::Status, VideoData, SetPlaneBitDepth,
         (void* p, Ipp32s iBitDepth, Ipp32s iPlaneNumber),
         p, (iBitDepth, iPlaneNumber))

UMC_CWRAPPER_FUNC(UMC::Status, VideoData, SetPlanePitch,
         (void* p, size_t nPitch, Ipp32s iPlaneNumber),
         p, (nPitch, iPlaneNumber))

UMC_CWRAPPER_FUNC(UMC::Status, VideoProcessing, GetFrame,
         (void* p, MediaData *input, MediaData *output),
         p, (input, output))

#undef UMC_CWRAPPER_FUNC
#undef UMC_CWRAPPER_CTOR
#undef UMC_CWRAPPER_DTOR
