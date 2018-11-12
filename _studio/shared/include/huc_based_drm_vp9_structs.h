// Copyright (c) 2013-2018 Intel Corporation
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