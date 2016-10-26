//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __HUC_BASED_DRM_VP9_STRUCTS_H__
#define __HUC_BASED_DRM_VP9_STRUCTS_H__

#define CACHELINE_SIZE              64

#define VP9_MAX_REF_LF_DELTAS       4
#define VP9_MAX_MODE_LF_DELTAS      2
#define VP9_MAX_SEGMENTS            8
#define VP9_SEG_TREE_PROBS          (VP9_MAX_SEGMENTS-1)
#define VP9_PREDICTION_PROBS        3

// Segment level features.
typedef enum {
  VP9_SEG_LVL_ALT_Q = 0,               // Use alternate Quantizer ....
  VP9_SEG_LVL_ALT_LF = 1,              // Use alternate loop filter value...
  VP9_SEG_LVL_REF_FRAME = 2,           // Optional Segment reference frame
  VP9_SEG_LVL_SKIP = 3,                // Optional Segment (0,0) + skip mode
  VP9_SEG_LVL_MAX = 4                  // Number of features supported
} VP9_SEG_LVL_FEATURES;

typedef struct
{
    // for context refresh
    uint8_t  intra_only;
    uint8_t  reset_frame_context;
    int8_t   frame_context_idx;
} FRAME_CONTEXT_MANAGE_INFO;

typedef struct
{
    uint16_t width;
    uint16_t height;
} FRAME_SIZE;

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint8_t  frame_type;
    uint8_t  intra_only;
    uint8_t  show_frame_flag;
} FRAME_IMMLAST_FRAME_INFO;

typedef struct
{
    int8_t   ref_deltas[VP9_MAX_REF_LF_DELTAS];      // 0 = Intra, Last, GF, ARF
    int8_t   mode_deltas[VP9_MAX_MODE_LF_DELTAS];    // 0 = ZERO_MV, MV
} LOOP_FILTER_DELTAS;

typedef struct
{
    uint8_t  enabled;
    uint8_t  abs_delta;
    uint8_t  segment_update_map;
    uint8_t  segment_temporal_delta;
    //vp9_prob segment_tree_probs[VP9_SEG_TREE_PROBS];   //dbrazhki: need defenition for vp9_prob. Temporary hide
    //vp9_prob segment_pred_probs[VP9_PREDICTION_PROBS]; //dbrazhki: need defenition for vp9_prob. Temporary hide
    int16_t  feature_data[VP9_MAX_SEGMENTS][VP9_SEG_LVL_MAX];
    uint8_t  feature_mask[VP9_MAX_SEGMENTS];
} SEGMENT_FEATURE;
/*
typedef FRAME_CONTEXT_MANAGE_INFO __attribute__ ((aligned (CACHELINE_SIZE))) ALIGNED_FRAME_CONTEXT_MANAGE_INFO;
typedef FRAME_IMMLAST_FRAME_INFO __attribute__ ((aligned (CACHELINE_SIZE))) ALIGNED_FRAME_IMMLAST_FRAME_INFO;
*/
#endif  // __HUC_BASED_DRM_VP9_STRUCTS_H__