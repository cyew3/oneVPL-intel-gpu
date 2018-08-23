//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined(UMC_ENABLE_VP9_VIDEO_DECODER)

#ifndef __UMC_VP9_FRAME_H__
#define __UMC_VP9_FRAME_H__

#include "umc_vp9_dec_defs.h"
#include "umc_frame_allocator.h"

namespace UMC_VP9_DECODER
{

class VP9DecoderFrame
{
public:

    VM_ALIGN16_DECL(int16_t) yDequant[QINDEX_RANGE][8];
    VM_ALIGN16_DECL(int16_t) uvDequant[QINDEX_RANGE][8];

    COLOR_SPACE color_space;

    uint32_t width;
    uint32_t height;

    uint32_t displayWidth;
    uint32_t displayHeight;

    int32_t subsamplingX;
    int32_t subsamplingY;

    UMC::FrameMemID currFrame;

    int32_t activeRefIdx[REFS_PER_FRAME];
    UMC::FrameMemID ref_frame_map[NUM_REF_FRAMES]; /* maps fb_idx to reference slot */

    SizeOfFrame sizesOfRefFrame[NUM_REF_FRAMES];

    VP9_FRAME_TYPE frameType;

    uint32_t show_existing_frame;
    uint32_t frame_to_show;

    uint32_t showFrame;
    uint32_t lastShowFrame;

    uint32_t intraOnly;

    uint32_t allowHighPrecisionMv;

    uint32_t resetFrameContext;

    uint8_t refreshFrameFlags;

    int32_t baseQIndex;
    int32_t y_dc_delta_q;
    int32_t uv_dc_delta_q;
    int32_t uv_ac_delta_q;

    uint32_t lossless;

    INTERP_FILTER interpFilter;

    LoopFilterInfo lf_info;

    uint32_t refreshFrameContext;

    uint32_t refFrameSignBias[MAX_REF_FRAMES];

    Loopfilter lf;
    VP9Segmentation segmentation;

    uint32_t frameContextIdx;

    uint32_t currentVideoFrame;
    uint32_t profile;
    uint32_t bit_depth;

    uint32_t errorResilientMode;
    uint32_t frameParallelDecodingMode;

    uint32_t log2TileColumns;
    uint32_t log2TileRows;

    uint32_t frameHeaderLength; // in bytes
    uint32_t firstPartitionSize;
    uint32_t frameDataSize;

    uint32_t frameCountInBS;
    uint32_t currFrameInBS;
};

} // end namespace UMC_VP9_DECODER

#endif // __UMC_VP9_FRAME_H__
#endif // UMC_ENABLE_VP9_VIDEO_DECODER
