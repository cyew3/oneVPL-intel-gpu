/*===================== begin_copyright_notice ==================================

INTEL CONFIDENTIAL
Copyright 2013 - 2016
Intel Corporation All Rights Reserved.

The source code contained or described herein and all documents related to the
source code ("Material") are owned by Intel Corporation or its suppliers or
licensors. Title to the Material remains with Intel Corporation or its suppliers
and licensors. The Material contains trade secrets and proprietary and confidential
information of Intel or its suppliers and licensors. The Material is protected by
worldwide copyright and trade secret laws and treaty provisions. No part of the
Material may be used, copied, reproduced, modified, published, uploaded, posted,
transmitted, distributed, or disclosed in any way without Intel's prior express
written permission.

No license under any patent, copyright, trade secret or other intellectual
property right is granted to or conferred upon you by disclosure or delivery
of the Materials, either expressly, by implication, inducement, estoppel
or otherwise. Any license under such intellectual property rights must be
express and approved by Intel in writing.
======================= end_copyright_notice ==================================*/
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