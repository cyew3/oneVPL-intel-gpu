//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_VP8_VAAPI_EXT_H__
#define __MFX_VP8_VAAPI_EXT_H__

// entry point for ENC + hybrid PAK
#define VAEntrypointHybridEncSlice          -1

// structure discribing MB data layout returned from driver
typedef struct _VAEncMbDataLayout
{
    unsigned char   MB_CODE_size;
    unsigned int    MB_CODE_offset;
    unsigned int    MB_CODE_stride;
    unsigned char   MV_number;
    unsigned int    MV_offset;
    unsigned int    MV_stride;
} VAEncMbDataLayout;

// this buffer representing VAEncMbDataLayout for function vpgQueryBufferAttributes()
#define VAEncMbDataBufferType               -4

#define FUNC_QUERY_BUFFER_ATTRIBUTES "hybridQueryBufferAttributes"
typedef VAStatus (*hybridQueryBufferAttributes)(
     VADisplay      dpy, 
     VAContextID    context, 
     VABufferType   bufferType,
     void           *outputData,
     unsigned int   *outputDataLen);

#define VAEncMiscParameterTypeVP8HybridFrameUpdate -3

typedef struct _VAEncMiscParameterVP8HybridFrameUpdate
{
    // Previous frame bitstream size in bytes used when BRC is enabled.
    // If it is unknown, like for the first couple frames, it is set to 0.
    unsigned int prev_frame_size;
    // Depending on whether app is running in synchronous or asynchronouse mode,
    // it may have the immediate previous frame size or the 2 prior frame size.
    // BRC algorithm is different for the 2 due to different frame types, below flag
    // indicates if prev_frame_size is from 2 previous frames.
    bool         two_prev_frame_flag;

    // Previous frame costs to use for current frame.  If set to 0 the driver will use default values.
    // MBs encoded by Intra(0), LastRef(1), GoldRef(2), or AltRef(3).
    unsigned short ref_frame_cost[4];
    // Mode costs, one per segment. INTRA_NONPRED(0), INTRA_16x16(1), INTRA_8x8(2), INTRA_4x4(3)
    // Note 0-3 correspond to Universal VME Input Message DW2.0.  See BSPEC for details.
    unsigned short intra_mode_cost[4];
    // MB's encoded by 16x16(0), 16x8(1), 8x8(2), 4x4(3).
    unsigned short inter_mode_cost[4];
    // one per segment
    unsigned short intra_non_dc_penalty_16x16;
    // one per segment
    unsigned short intra_non_dc_penalty_4x4;

    // quantization index for reference frames: 
    //  ref_q_index[0] - LastRef
    //  ref_q_index[1] - GoldRef
    //  ref_q_index[2] - AltRef
    unsigned char  ref_q_index[3];
} VAEncMiscParameterVP8HybridFrameUpdate;

// Segment map parameters needed by BRC when app provides segmentation map.
#define VAEncMiscParameterTypeVP8SegmentMapParams   -4

typedef struct _VAEncMiscParameterVP8SegmentMapParams
{
    // Specifies the QIndex delta of Y1_ac value for BRC to use, one per segment,
    // when the application provides the VP8 MB Segmentation Buffer using
    // VAEncMacroblockMapBufferType.
    char yac_quantization_index_delta[4];
} VAEncMiscParameterVP8SegmentMapParams;

#endif // __MFX_VP8_VAAPI_EXT_H__
