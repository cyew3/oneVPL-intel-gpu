//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

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

    size_t GetFrameDataSize(const IppiSize &lumaSize);

    void deallocateParsedFrameData();

    H264DecoderGlobalMacroblocksDescriptor m_mbinfo; //Global MB Data

protected:
    Ipp8u                 *m_pParsedFrameDataNew;
    // This points to a huge, monolithic buffer that contains data
    // derived from parsing the current frame.  It contains motion
    // vectors,  MB info, reference indices, and slice info for the
    // current frame, among other things. When B slices are used it
    // contains L0 and L1 motion vectors and reference indices.

    IppiSize           m_paddedParsedFrameDataSize;

    virtual void FreeResources();
};


} // end namespace UMC

#endif // __UMC_H264_FRAME_EX_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
