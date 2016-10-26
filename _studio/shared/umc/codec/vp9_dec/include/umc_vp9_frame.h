//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_VP9_VIDEO_DECODER

#ifndef __UMC_VP9_FRAME_H__
#define __UMC_VP9_FRAME_H__

#include "umc_vp9_dec_defs.h"
#include "umc_frame_allocator.h"

namespace UMC_VP9_DECODER
{

class VP9DecoderFrame
{
public:

    VM_ALIGN16_DECL(Ipp16s) yDequant[QINDEX_RANGE][8];
    VM_ALIGN16_DECL(Ipp16s) uvDequant[QINDEX_RANGE][8];

    COLOR_SPACE color_space;

    Ipp32u width;
    Ipp32u height;

    Ipp32u displayWidth;
    Ipp32u displayHeight;

    Ipp32s subsamplingX;
    Ipp32s subsamplingY;

    // RefCntBuffer frame_bufs[FRAME_BUFFERS];

    UMC::FrameMemID currFrame;

    Ipp32s activeRefIdx[REFS_PER_FRAME];
    UMC::FrameMemID ref_frame_map[NUM_REF_FRAMES]; /* maps fb_idx to reference slot */

    SizeOfFrame sizesOfRefFrame[NUM_REF_FRAMES];

    VP9_FRAME_TYPE frameType;

    Ipp32u show_existing_frame;
    Ipp32u frame_to_show;

    Ipp32u showFrame;
    Ipp32u lastShowFrame;

    Ipp32u intraOnly;

    Ipp32u allowHighPrecisionMv;

    Ipp32u resetFrameContext;

    Ipp8u refreshFrameFlags;

    Ipp32s baseQIndex;
    Ipp32s y_dc_delta_q;
    Ipp32s uv_dc_delta_q;
    Ipp32s uv_ac_delta_q;

    Ipp32u lossless;

    INTERP_FILTER interpFilter;

    LoopFilterInfo lf_info;

    Ipp32u refreshFrameContext;

    Ipp32u refFrameSignBias[MAX_REF_FRAMES];

    Loopfilter lf;
    VP9Segmentation segmentation;

    Ipp32u frameContextIdx;
    // FRAME_COUNTS counts; // ?????????????

    Ipp32u currentVideoFrame;
    Ipp32u profile;
    Ipp32u bit_depth;

    Ipp32u errorResilientMode;
    Ipp32u frameParallelDecodingMode;

    Ipp32u log2TileColumns;
    Ipp32u log2TileRows;

    Ipp32u frameHeaderLength; // in bytes
    Ipp32u firstPartitionSize;
    Ipp32u frameDataSize;

    Ipp32u frameCountInBS;
    Ipp32u currFrameInBS;
};

} // end namespace UMC_VP9_DECODER

#endif // __UMC_VP9_FRAME_H__
#endif // UMC_ENABLE_H265_VIDEO_DECODER
