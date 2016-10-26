//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __HUC_BASED_DRM_AVC_STRUCTS_H__
#define __HUC_BASED_DRM_AVC_STRUCTS_H__

#define CACHELINE_SIZE 64

#define MAX_NUM_SCHED_SEL_ITEMS 32
#define NUM_MMCO_OPERATIONS 17
#define MAX_PIC_LIST_NUM 8
#define MAX_NUM_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE 256

/**
@brief <H3></H3>
   HRD Parameters found in stream.
   Refer to Annex E.1.2 Section for syntax, or E.2.2 Section for semantics.
*/
typedef struct
{
    /** cpb_cnt_minus1 represents the number of alternative CPB in the bitstream. Range 0 to 31, inclusive */
    uint8_t  cpb_cnt_minus1;
    /** Together with bit_rate_value_minus1[ idx ] specifies the maximum input rate of the idx-th CPB  */
    uint8_t  bit_rate_scale;
    /** Together with cpb_size_value_minus1[ idx ] specifies the CPB size rate of the idx-th CPB  */
    uint8_t  cpb_size_scale;
    /** Extra uint8 to align structure to mantain 4-byte alignment. */
    uint8_t  align1;
    /** Specifies the length in bits of the initial_cpb_removal_delay[idx] and initial_cpb_removal_delay_offset[ idx ]
        syntax elements of the buffering period SEI message. */
    uint8_t  initial_cpb_removal_delay_length_minus1;
    /** Specifies the length in bits of the cpb_removal_delay syntax element. (timing SEI message) */
    uint8_t  cpb_removal_delay_length_minus1;
    /** Specifies the length in bits of the dpb_output_delay syntax element (timing SEI message) */
    uint8_t  dpb_output_delay_length_minus1;
    /** If non-zero, specifies the length in bits of the time_offset syntax element. If zero, specifies that the
        time_offset syntax element is not present. */
    uint8_t  time_offset_length;
} h264_hrd_param_info_t;

/**
@brief <H3></H3>
   HRD Parameters array found in stream.
   Refer to Annex E.1.2 Section for syntax, or E.2.2 Section for semantics.
   Since hrd_parameters() may contain up to 32 items, each item is to be stored in a different workload item.
*/
typedef struct
{
    /** Specifies the maximum input bitrate for this CPB. Shall be in range of 0 to (2^32) - 2, inclusive */
    uint32_t bit_rate_value_minus1;
    /** Together with cpb_size_scale specifies this CPB's size. Shall be in range of 0 to (2^32) - 2, inclusive */
    uint32_t cpb_size_value_minus1;
    /** When equal to 0, specifies that to decode this bitstream by the HRD using this CPBs specifiation, the HSS operates in an intermittent bit rate mode.
        When equal to 1, specifies that the HSS operates in a constant bit rate mode. */
    uint32_t cbr_flag;
} h264_hrd_param_item_t;

/**
@brief <H3></H3>
   Structure for HRD parameters syntax
   Refer to Annex E.1.2 Section for syntax, or E.2.2 Section for semantics.
   This structure encapsulates h264_hrd_param_info_t and h264_hrd_param_item_t.
*/
typedef struct
{
   /** Information data from hrd_parameters() syntax element */
   h264_hrd_param_info_t      info;
   /** Item array from hrd_parameters() syntax element
   Note that this items are only used if present in the bitstream. Currently, the difference of
   having a 1 or 32 array will be seen in the initialization time. If initialization time becomes an issue,
   this could be changed to initialialize-before-use scenario.
   */
   h264_hrd_param_item_t      item[ MAX_NUM_SCHED_SEL_ITEMS ];
} h264_hrd_param_set_t;

/**
@brief <H3></H3>
   Structure for VUI parameters syntax
*/
typedef struct
{
    uint32_t num_units_in_tick;                             // u(32)
    uint32_t time_scale;                                    // u(32)

    int32_t  num_reorder_frames;                               // ue(v), 0 to max_dec_frame_buffering
    int32_t  max_dec_frame_buffering;                          // ue(v), 0 to MaxDpbSize, specified in subclause A.3

    uint16_t sar_width;                                       // u(16)
    uint16_t sar_height;                                      // u(16)

    uint8_t  aspect_ratio_info_present_flag;                  // u(1)
    uint8_t  aspect_ratio_idc;                                // u(8)
    uint8_t  video_signal_type_present_flag;                  // u(1)
    uint8_t  video_format;                                    // u(3)

    uint8_t  colour_description_present_flag;                 // u(1)
    uint8_t  colour_primaries;                                // u(8)
    uint8_t  transfer_characteristics;                        // u(8)
    uint8_t  matrix_coefficients;                             // u(8)

    uint8_t  pad_8b;                        // u(8) 8 bit pad to keep structure 32b aligned
    uint16_t pad_16b;                       // u(16) 16 bit pad to keep structure 32b aligned

    uint8_t  timing_info_present_flag;                        // u(1)

    uint8_t  fixed_frame_rate_flag;                           // u(1)
    uint8_t  low_delay_hrd_flag;                              // u(1)
    uint8_t  bitstream_restriction_flag;                      // u(1)
    uint8_t  pic_struct_present_flag;

    uint8_t  nal_hrd_parameters_present_flag;                 // u(1)
    uint8_t  vcl_hrd_parameters_present_flag;                 // u(1)
    uint16_t alignbits;

    // HRD information
    h264_hrd_param_set_t NalHrd;
    h264_hrd_param_set_t VclHrd;
} vui_seq_parameters_t;

typedef struct
{
    // VUI info
    vui_seq_parameters_t vui_seq_parameters;

    // Resolution
    int16_t  pic_width_in_mbs_minus1;
    int16_t  pic_height_in_map_units_minus1;

    // Cropping
    int16_t  frame_crop_rect_left_offset;
    int16_t  frame_crop_rect_right_offset;

    int16_t  frame_crop_rect_top_offset;
    int16_t  frame_crop_rect_bottom_offset;

    // Flags
    uint8_t  frame_mbs_only_flag;
    uint8_t  mb_adaptive_frame_field_flag;
    uint8_t  direct_8x8_inference_flag;
    uint8_t  frame_cropping_flag;

    uint16_t vui_parameters_present_flag;
    uint16_t chroma_format_idc;
} seq_param_set_disp_t;

typedef struct
{
    uint32_t is_updated;

    //Required for display section
    seq_param_set_disp_t sps_disp;

    int32_t  expectedDeltaPerPOCCycle;
    int32_t  offset_for_non_ref_pic;                           // se(v), -2^31 to (2^31)-1, 32-bit integer
    int32_t  offset_for_top_to_bottom_field;                   // se(v), -2^31 to (2^31)-1, 32-bit integer

    // IDC
    uint8_t  profile_idc;                                      // u(8), 0x77 for MP
    uint8_t  constraint_set_flags;                             // bit 0 to 3 for set0 to set3
    uint8_t  level_idc;                                        // u(8)
    uint8_t  seq_parameter_set_id;                             // ue(v), 0 to 31

    uint8_t  pic_order_cnt_type;                               // ue(v), 0 to 2
    uint8_t  log2_max_frame_num_minus4;                        // ue(v), 0 to 12
    uint8_t  log2_max_pic_order_cnt_lsb_minus4;                // ue(v), 0 to 12
    uint8_t  num_ref_frames_in_pic_order_cnt_cycle;            // ue(v), 0 to 255

    int32_t  offset_for_ref_frame[MAX_NUM_REF_FRAMES_IN_PIC_ORDER_CNT_CYCLE];   // se(v), -2^31 to (2^31)-1, 32-bit integer
    uint8_t  num_ref_frames;                                   // ue(v), 0 to 16,
    uint8_t  gaps_in_frame_num_value_allowed_flag;             // u(1)
    // This is my addition, we should calculate this once and leave it with the sps
    // as opposed to calculating it each time in h264_hdr_decoding_POC()

    uint8_t  delta_pic_order_always_zero_flag;                 // u(1)
    uint8_t  residual_colour_transform_flag;

    uint8_t  bit_depth_luma_minus8;
    uint8_t  bit_depth_chroma_minus8;
    uint8_t  lossless_qpprime_y_zero_flag;
    uint8_t  seq_scaling_matrix_present_flag;

    uint8_t  seq_scaling_list_present_flag[MAX_PIC_LIST_NUM];   //0-7

    // Combine the scaling matrix to word ( 24 + 32)
    uint8_t  ScalingList4x4[6][16];
    uint8_t  ScalingList8x8[2][64];
    uint8_t  UseDefaultScalingMatrix4x4Flag[6];
    uint8_t  UseDefaultScalingMatrix8x8Flag[6];
}seq_param_set_t;

// qm_matrix_set
typedef struct _qm_matrix_set
{
    uint8_t  scaling_list[56];            // 0 to 23 for qm 0 to 5 (4x4), 24 to 55 for qm 6 & 7 (8x8)
} qm_matrix_set, *qm_matrix_set_ptr;

// picture parameter set
typedef struct
{
    int32_t  pic_init_qp_minus26;                             // se(v), -26 to +25
    int32_t  pic_init_qs_minus26;                             // se(v), -26 to +25
    int32_t  chroma_qp_index_offset;                          // se(v), -12 to +12
    int32_t  second_chroma_qp_index_offset;

    uint8_t  pic_parameter_set_id;                            // ue(v), 0 to 255, restricted to 0 to 127 by MPD_CTRL_MAXPPS = 128
    uint8_t  seq_parameter_set_id;                            // ue(v), 0 to 31
    uint8_t  entropy_coding_mode_flag;                        // u(1)
    uint8_t  pic_order_present_flag;                          // u(1)

    uint8_t  num_slice_groups_minus1;                         // ue(v), shall be 0 for MP
    // Below are not relevant for main profile...
    uint8_t  slice_group_map_type;                            // ue(v), 0 to 6
    uint8_t  num_ref_idx_l0_active;                     // ue(v), 0 to 31
    uint8_t  num_ref_idx_l1_active;                     // ue(v), 0 to 31

    uint8_t  weighted_pred_flag;                              // u(1)
    uint8_t  weighted_bipred_idc;                             // u(2)
    uint8_t  deblocking_filter_control_present_flag;          // u(1)
    uint8_t  constrained_intra_pred_flag;                     // u(1)

    uint8_t  redundant_pic_cnt_present_flag;                  // u(1)
    uint8_t  transform_8x8_mode_flag;
    uint8_t  pic_scaling_matrix_present_flag;
    uint8_t  pps_status_flag;

    //Keep here with 32-bits aligned
    uint8_t  pic_scaling_list_present_flag[MAX_PIC_LIST_NUM];

    qm_matrix_set   pps_qm;

    uint8_t  ScalingList4x4[6][16];
    uint8_t  ScalingList8x8[2][64];
    uint8_t  UseDefaultScalingMatrix4x4Flag[6+2];
    uint8_t  UseDefaultScalingMatrix8x8Flag[6+2];
} pic_param_set_t;

typedef struct _h264_Dec_Ref_Pic_Marking //size = 17*4*2 + 17*3 + 4 + 1
{
    int32_t  difference_of_pic_num_minus1[NUM_MMCO_OPERATIONS];
    int32_t  long_term_pic_num[NUM_MMCO_OPERATIONS];

    // MMCO
    uint8_t  memory_management_control_operation[NUM_MMCO_OPERATIONS];
    uint8_t  max_long_term_frame_idx_plus1[NUM_MMCO_OPERATIONS];
    uint8_t  long_term_frame_idx[NUM_MMCO_OPERATIONS];
    uint8_t  long_term_reference_flag;

    uint8_t  adaptive_ref_pic_marking_mode_flag;
    uint8_t  dec_ref_pic_marking_count; // The number of non-zero elements in memory_management_control_operation
    uint8_t  no_output_of_prior_pics_flag;

    uint8_t  pad;
}h264_Dec_Ref_Pic_Marking_t;

#define MAX_CPB_CNT              32

typedef struct _h264_SEI_buffering_period
{
    int32_t  seq_param_set_id;
    int32_t  initial_cpb_removal_delay_nal[MAX_CPB_CNT];
    int32_t  initial_cpb_removal_delay_offset_nal[MAX_CPB_CNT];
    int32_t  initial_cpb_removal_delay_vcl[MAX_CPB_CNT];
    int32_t  initial_cpb_removal_delay_offset_vcl[MAX_CPB_CNT];
}h264_SEI_buffering_period_t;

typedef struct _h264_SEI_pic_timing
{
    int32_t  cpb_removal_delay;
    int32_t  dpb_output_delay;
    int32_t  pic_struct;
}h264_SEI_pic_timing_t;

typedef struct _h264_SEI_recovery_point
{
    int32_t  recovery_frame_cnt;
    int32_t  exact_match_flag;
    int32_t  broken_link_flag;
    int32_t  changing_slice_group_idc;
} h264_SEI_recovery_point_t;
/*
typedef struct _HUC_POC_AVC_CALC_INFO
{
    // POC decoding
    int32_t  PicOrderCntMsb;
    int32_t  PrevPicOrderCntLsb;
    int32_t  PreviousFrameNum;
    int32_t  PreviousFrameNumOffset;
    int32_t  PrevPicPOC;
    uint32_t last_has_mmco_5;
    uint32_t cur_has_mmco_5;
    uint32_t last_pic_bottom_field;
} __attribute__ ((aligned (64))) HUC_POC_AVC_CALC_INFO, *PHUC_POC_AVC_CALC_INFO;

typedef seq_param_set_t             __attribute__ ((aligned (CACHELINE_SIZE))) aligned_seq_param_set_t;
typedef pic_param_set_t             __attribute__ ((aligned (CACHELINE_SIZE))) aligned_pic_param_set_t;
typedef h264_Dec_Ref_Pic_Marking_t  __attribute__ ((aligned (CACHELINE_SIZE))) aligned_h264_Dec_Ref_Pic_Marking_t;
typedef h264_SEI_buffering_period_t __attribute__ ((aligned (CACHELINE_SIZE))) aligned_h264_SEI_buffering_period_t;
typedef h264_SEI_pic_timing_t       __attribute__ ((aligned (CACHELINE_SIZE))) aligned_h264_SEI_pic_timing_t;
typedef h264_SEI_recovery_point_t   __attribute__ ((aligned (CACHELINE_SIZE))) aligned_h264_SEI_recovery_point_t;
*/
#endif  // __HUC_BASED_DRM_AVC_STRUCTS_H__