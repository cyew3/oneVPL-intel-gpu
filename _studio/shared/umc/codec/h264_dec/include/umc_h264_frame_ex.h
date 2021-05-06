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

#ifndef __UMC_H264_FRAME_EX_H__
#define __UMC_H264_FRAME_EX_H__

#include "umc_h264_frame.h"
#include "umc_h264_dec_structures.h"

namespace UMC
{

class H264DecoderFrameEx : public H264DecoderFrame
{
    DYNAMIC_CAST_DECL(H264DecoderFrameEx, H264DecoderFrame)

public:
    H264DecoderFrameEx(MemoryAllocator *pMemoryAllocator, H264_Heap_Objects * pObjHeap);

    virtual ~H264DecoderFrameEx();

    Status allocateParsedFrameData();
    // Reallocate m_pParsedFrameData, if necessary, and initialize the
    // various pointers that point into it.

    size_t GetFrameDataSize(const mfxSize &lumaSize);

    void deallocateParsedFrameData();

    H264DecoderGlobalMacroblocksDescriptor m_mbinfo; //Global MB Data

protected:
    uint8_t                 *m_pParsedFrameDataNew;
    // This points to a huge, monolithic buffer that contains data
    // derived from parsing the current frame.  It contains motion
    // vectors,  MB info, reference indices, and slice info for the
    // current frame, among other things. When B slices are used it
    // contains L0 and L1 motion vectors and reference indices.

    mfxSize           m_paddedParsedFrameDataSize;

    virtual void FreeResources();
};


} // end namespace UMC

#endif // __UMC_H264_FRAME_EX_H__
#endif // MFX_ENABLE_H264_VIDEO_DECODE
