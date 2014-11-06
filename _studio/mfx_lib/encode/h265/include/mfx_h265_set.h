//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_SET_H__
#define __MFX_H265_SET_H__

#include "mfx_h265_defs.h"

namespace H265Enc {

struct H265EncoderRowInfo
{
    Ipp32s bs_id;
    Ipp32s offset;
    Ipp32s size;
    volatile Ipp32s mt_current_ctb_col;
    volatile Ipp32s mt_busy;
};

struct H265VidParameterSet
{
    Ipp8u  vps_video_parameter_set_id;
    Ipp8u  vps_max_layers;
    Ipp8u  vps_max_sub_layers;
    Ipp8u  vps_temporal_id_nesting_flag;
    Ipp8u  vps_sub_layer_ordering_info_present_flag;
    Ipp8u  vps_max_dec_pic_buffering[MAX_TEMPORAL_LAYERS];
    Ipp8u  vps_max_num_reorder_pics[MAX_TEMPORAL_LAYERS];
    Ipp8s  vps_max_latency_increase[MAX_TEMPORAL_LAYERS];
    Ipp8u  vps_extension_flag;
    Ipp8u  vps_max_layer_id;
    Ipp8u  vps_num_layer_sets;
    Ipp8u  vps_timing_info_present_flag;
    Ipp32u vps_num_units_in_tick;
    Ipp32u vps_time_scale;
    Ipp8u  vps_poc_proportional_to_timing_flag;
    Ipp32u vps_num_hrd_parameters;
};

struct H265ProfileLevelSet
{
    Ipp8u general_profile_space;
    Ipp8u general_tier_flag;
    Ipp8u general_profile_idc;
    Ipp8u general_profile_compatibility_flag[32];
    Ipp8u general_progressive_source_flag;
    Ipp8u general_interlaced_source_flag;
    Ipp8u general_non_packed_constraint_flag;
    Ipp8u general_frame_only_constraint_flag;
    Ipp8u general_level_idc;
};

struct H265ShortTermRefPicSet
{
    Ipp8u  inter_ref_pic_set_prediction_flag;
    Ipp32s delta_idx;
    Ipp8u  delta_rps_sign;
    Ipp32s abs_delta_rps;
    Ipp8u  use_delta_flag[MAX_NUM_REF_FRAMES];
    Ipp8u  num_negative_pics;
    Ipp8u  num_positive_pics;
    Ipp32s delta_poc[MAX_NUM_REF_FRAMES]; /* negative+positive */
    Ipp8u  used_by_curr_pic_flag[MAX_NUM_REF_FRAMES]; /* negative+positive */
};

struct H265SeqParameterSet
{
    Ipp8u  sps_video_parameter_set_id;
    Ipp8u  sps_max_sub_layers;
    Ipp8u  sps_temporal_id_nesting_flag;
    Ipp8u  sps_seq_parameter_set_id;
    Ipp8u  chroma_format_idc;
    Ipp32u pic_width_in_luma_samples;
    Ipp32u pic_height_in_luma_samples;
    Ipp8u  conformance_window_flag;
    Ipp32u conf_win_left_offset;
    Ipp32u conf_win_right_offset;
    Ipp32u conf_win_top_offset;
    Ipp32u conf_win_bottom_offset;
    Ipp8u  bit_depth_luma;
    Ipp8u  bit_depth_chroma;
    Ipp8u  log2_max_pic_order_cnt_lsb;
    Ipp8u  sps_sub_layer_ordering_info_present_flag;
    Ipp8u  sps_max_dec_pic_buffering[MAX_TEMPORAL_LAYERS];
    Ipp8u  sps_num_reorder_pics[MAX_TEMPORAL_LAYERS];
    Ipp8u  sps_max_latency_increase[MAX_TEMPORAL_LAYERS];
    Ipp8u  log2_min_coding_block_size_minus3;
    Ipp8u  log2_diff_max_min_coding_block_size;
    Ipp8u  log2_min_transform_block_size_minus2;
    Ipp8u  log2_diff_max_min_transform_block_size;
    Ipp8u  max_transform_hierarchy_depth_inter;
    Ipp8u  max_transform_hierarchy_depth_intra;
    Ipp8u  scaling_list_enable_flag;
    Ipp8u  sps_scaling_list_data_present_flag;
    Ipp8u  amp_enabled_flag;
    Ipp8u  sample_adaptive_offset_enabled_flag;
    Ipp8u  pcm_enabled_flag;
    Ipp8u  pcm_sample_bit_depth_luma;
    Ipp8u  pcm_sample_bit_depth_chroma;
    Ipp8u  log2_min_pcm_luma_coding_block_size;
    Ipp8u  log2_diff_max_min_pcm_luma_coding_block_size;
    Ipp8u  pcm_loop_filter_disabled_flag;
    Ipp8u  num_short_term_ref_pic_sets;
    H265ShortTermRefPicSet m_shortRefPicSet[65];
    Ipp8u  long_term_ref_pics_present_flag;
    Ipp8u  sps_temporal_mvp_enabled_flag;
    Ipp8u  strong_intra_smoothing_enabled_flag;
    Ipp8u  vui_parameters_present_flag;
    Ipp8u  aspect_ratio_info_present_flag;
    Ipp8u  aspect_ratio_idc;
    Ipp16u sar_width;
    Ipp16u sar_height;
    Ipp8u  overscan_info_present_flag;
    Ipp8u  video_signal_type_present_flag;
    Ipp8u  chroma_loc_info_present_flag;
    Ipp8u  neutral_chroma_indication_flag;
    Ipp8u  field_seq_flag;
    Ipp8u  frame_field_info_present_flag;
    Ipp8u  default_display_window_flag;
    Ipp8u  sps_timing_info_present_flag; // use vps*
    Ipp8u  bitstream_restriction_flag;
    Ipp8u  sps_extension_flag;
} ;

struct H265PicParameterSet
{
    Ipp8u  pps_pic_parameter_set_id;
    Ipp8u  pps_seq_parameter_set_id;
    Ipp8u  dependent_slice_segments_enabled_flag;
    Ipp8u  output_flag_present_flag;
    Ipp8u  num_extra_slice_header_bits;
    Ipp8u  sign_data_hiding_enabled_flag;
    Ipp8u  cabac_init_present_flag;
    Ipp8u  num_ref_idx_l0_default_active;
    Ipp8u  num_ref_idx_l1_default_active;
    Ipp8s  init_qp;
    Ipp8u  constrained_intra_pred_flag;
    Ipp8u  transform_skip_enabled_flag;
    Ipp8u  cu_qp_delta_enabled_flag;
    Ipp8u  diff_cu_qp_delta_depth;
    Ipp8s  pps_cb_qp_offset;
    Ipp8s  pps_cr_qp_offset;
    Ipp8u  pps_slice_chroma_qp_offsets_present_flag;
    Ipp8u  weighted_pred_flag;
    Ipp8u  weighted_bipred_flag;
    Ipp8u  transquant_bypass_enable_flag;
    Ipp8u  tiles_enabled_flag;
    Ipp8u  entropy_coding_sync_enabled_flag;
    Ipp8u  num_tile_columns;
    Ipp8u  num_tile_rows;
    Ipp8u  uniform_spacing_flag;
    Ipp32u column_width[MAX_NUM_TILE_COLUMNS];
    Ipp32u row_height[MAX_NUM_TILE_ROWS];
    Ipp8u  loop_filter_across_tiles_enabled_flag;
    Ipp8u  pps_loop_filter_across_slices_enabled_flag;
    Ipp8u  deblocking_filter_control_present_flag;
    Ipp8u  deblocking_filter_override_enabled_flag;
    Ipp8u  pps_deblocking_filter_disabled_flag;
    Ipp32s pps_beta_offset_div2;
    Ipp32s pps_tc_offset_div2;
    Ipp8u  pps_scaling_list_data_present_flag;
    Ipp8u  lists_modification_present_flag;
    Ipp8u  log2_parallel_merge_level;
    Ipp8u  slice_segment_header_extension_present_flag;
    Ipp8u  pps_extension_flag;
};

struct H265SliceHeader
{
    Ipp8u  first_slice_segment_in_pic_flag;
    Ipp8u  no_output_of_prior_pics_flag;
    Ipp8u  slice_pic_parameter_set_id;
    Ipp8u  dependent_slice_segment_flag;
    Ipp32s slice_segment_address;
    Ipp8u  slice_type;
    Ipp8u  pic_output_flag;
    Ipp8u  colour_plane_id;
    Ipp32u slice_pic_order_cnt_lsb;
    Ipp8u  short_term_ref_pic_set_sps_flag;
    H265ShortTermRefPicSet m_shortRefPicSet;
    Ipp32u short_term_ref_pic_set_idx;
    Ipp32u num_long_term_pics;
    Ipp32u poc_lsb_lt[MAX_NUM_LONG_TERM_PICS];
    Ipp8u  delta_poc_msb_present_flag[MAX_NUM_LONG_TERM_PICS];
    Ipp8u  delta_poc_msb_cycle_lt[MAX_NUM_LONG_TERM_PICS];
    Ipp8u  used_by_curr_pic_lt_flag[MAX_NUM_LONG_TERM_PICS];
    Ipp8u  slice_temporal_mvp_enabled_flag;
    Ipp8u  slice_sao_luma_flag;
    Ipp8u  slice_sao_chroma_flag;
    Ipp8u  num_ref_idx_active_override_flag;
    Ipp32s num_ref_idx[2];
    Ipp8u  mvd_l1_zero_flag;
    Ipp8u  cabac_init_flag;
    Ipp8u  collocated_from_l0_flag;
    Ipp32u collocated_ref_idx;
    Ipp32u five_minus_max_num_merge_cand;
    Ipp32s slice_qp_delta;
    Ipp8s  slice_cb_qp_offset;
    Ipp8s  slice_cr_qp_offset;
    Ipp8u  deblocking_filter_override_flag;
    Ipp8u  slice_deblocking_filter_disabled_flag;
    Ipp32s slice_beta_offset_div2;
    Ipp32s slice_tc_offset_div2;
    Ipp8u  slice_loop_filter_across_slices_enabled_flag;
    Ipp32u num_entry_point_offsets;
    Ipp32u offset_len;
    Ipp32u entry_point_offset[MAX_NUM_ENTRY_POINT_OFFSETS];
    Ipp16s weights[9][MAX_NUM_REF_FRAMES][MAX_NUM_REF_FRAMES];
    Ipp16s offsets[9][MAX_NUM_REF_FRAMES][MAX_NUM_REF_FRAMES];
    Ipp8u  luma_log2_weight_denom;             // luma weighting denominator
    Ipp8u  chroma_log2_weight_denom;           // chroma weighting denominator
};

struct H265Slice : public H265SliceHeader
{
    Ipp32s DependentSliceCurStartCUAddr;
    Ipp32s DependentSliceCurEndCUAddr;

    Ipp8u  IdrPicFlag;
    Ipp8u  RapPicFlag;
    Ipp32s slice_num;
    Ipp32u slice_address_last_ctb;
    Ipp32u row_first;
    Ipp32u row_last;
    H265EncoderRowInfo m_row_info;

    Ipp32s m_NumRefsInL0List;
    Ipp32s m_NumRefsInL1List;
    Ipp32u list_entry_l0[MAX_NUM_REF_FRAMES];
    Ipp32u list_entry_l1[MAX_NUM_REF_FRAMES];
    Ipp8u  m_ref_pic_list_modification_flag_l0;
    Ipp8u  m_ref_pic_list_modification_flag_l1;

    Ipp8u  rd_opt_flag;
    Ipp64f rd_lambda_slice;
    Ipp64f rd_lambda_inter_slice;
    Ipp64f rd_lambda_inter_mv_slice;

    // kolya
    // to match HM's lambda in HAD search
    Ipp64f rd_lambda_sqrt_slice;
    Ipp64f ChromaDistWeight_slice;

    EnumIntraAngMode sliceIntraAngMode;
};

} // namespace

#endif // __MFX_H265_SET_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
