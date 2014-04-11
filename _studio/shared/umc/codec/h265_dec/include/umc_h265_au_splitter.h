/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_AU_SPLITTER_H
#define __UMC_H265_AU_SPLITTER_H

#include <vector>
#include "umc_h265_dec_defs_dec.h"
#include "umc_media_data_ex.h"
#include "umc_h265_heap.h"
#include "umc_h265_frame_info.h"

#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_headers.h"

#include "umc_frame_allocator.h"

namespace UMC_HEVC_DECODER
{

class NALUnitSplitter_H265;

// NAL unit splitter wrapper class
class AU_Splitter_H265
{
public:
    AU_Splitter_H265(Heap_Objects *objectHeap);
    virtual ~AU_Splitter_H265();

    void Init(UMC::VideoDecoderParams *init);
    void Close();

    void Reset();

    // Wrapper for NAL unit splitter CheckNalUnitType
    Ipp32s CheckNalUnitType(UMC::MediaData * pSource);

    // Wrapper for NAL unit splitter CheckNalUnitType GetNalUnit
    UMC::MediaDataEx * GetNalUnit(UMC::MediaData * src);
    // Returns internal NAL unit splitter
    NALUnitSplitter_H265 * GetNalUnitSplitter();

protected:

    Headers         m_Headers;
    Heap_Objects   *m_objHeap;

protected:

    std::auto_ptr<NALUnitSplitter_H265> m_pNALSplitter;
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_AU_SPLITTER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
