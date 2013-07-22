//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_SET_H__
#define __MFX_H265_SET_H__

typedef struct
{
    Ipp32s bs_id;
    Ipp32s offset;
    Ipp32s size;
    Ipp32s mt_current_ctb_col;
} H265EncoderRowInfo;

typedef struct sH265VidParameterSet {
    Ipp8u vps_video_parameter_set_id;
    Ipp8u vps_max_layers;
    Ipp8u vps_max_sub_layers;
    Ipp8u vps_temporal_id_nesting_flag;
    Ipp8u vps_sub_layer_ordering_info_present_flag;

    Ipp8u vps_max_dec_pic_buffering[MAX_TEMPORAL_LAYERS];
    Ipp8u vps_max_num_reorder_pics[MAX_TEMPORAL_LAYERS];
    Ipp8s vps_max_latency_increase[MAX_TEMPORAL_LAYERS];
    Ipp8u vps_extension_flag;
    Ipp8u vps_max_layer_id;
    Ipp8u vps_num_layer_sets;
    Ipp8u vps_timing_info_present_flag;
} H265VidParameterSet;

typedef struct sH265ProfileLevelSet{
    Ipp8u general_profile_space;
    Ipp8u general_tier_flag;
    Ipp8u general_profile_idc;
    Ipp8u general_profile_compatibility_flag[32];
    Ipp8u general_progressive_source_flag;
    Ipp8u general_interlaced_source_flag;
    Ipp8u general_non_packed_constraint_flag;
    Ipp8u general_frame_only_constraint_flag;

    Ipp8u general_level_idc;
} H265ProfileLevelSet;

typedef struct sH265SeqParameterSet {
///////// syntax part
    Ipp8u sps_video_parameter_set_id;
    Ipp8u sps_max_sub_layers;
    Ipp8u sps_temporal_id_nesting_flag;
    Ipp8u sps_seq_parameter_set_id;
    Ipp8u chroma_format_idc;

    Ipp32u pic_width_in_luma_samples;
    Ipp32u pic_height_in_luma_samples;
    Ipp8u conformance_window_flag;
    Ipp32u conf_win_left_offset;
    Ipp32u conf_win_right_offset;
    Ipp32u conf_win_top_offset;
    Ipp32u conf_win_bottom_offset;

    Ipp8u bit_depth_luma;
    Ipp8u bit_depth_chroma;

    Ipp8u log2_max_pic_order_cnt_lsb;
    Ipp8u sps_sub_layer_ordering_info_present_flag;
    Ipp8u sps_max_dec_pic_buffering[MAX_TEMPORAL_LAYERS];
    Ipp8u sps_num_reorder_pics[MAX_TEMPORAL_LAYERS];
    Ipp8u sps_max_latency_increase[MAX_TEMPORAL_LAYERS];

    Ipp8u log2_min_coding_block_size_minus3;
    Ipp8u log2_diff_max_min_coding_block_size;
    Ipp8u log2_min_transform_block_size_minus2;
    Ipp8u log2_diff_max_min_transform_block_size;
    Ipp8u max_transform_hierarchy_depth_inter;
    Ipp8u max_transform_hierarchy_depth_intra;

    Ipp8u scaling_list_enable_flag;
    Ipp8u sps_scaling_list_data_present_flag;
    Ipp8u amp_enabled_flag;
    Ipp8u sample_adaptive_offset_enabled_flag;
    Ipp8u pcm_enabled_flag;
    Ipp8u pcm_sample_bit_depth_luma;
    Ipp8u pcm_sample_bit_depth_chroma;
    Ipp8u log2_min_pcm_luma_coding_block_size;
    Ipp8u log2_diff_max_min_pcm_luma_coding_block_size;
    Ipp8u pcm_loop_filter_disabled_flag;

    Ipp8u num_short_term_ref_pic_sets;
//    for( i = 0; i < num_short_term_ref_pic_sets; i++)
//        short_term_ref_pic_set( i )
    Ipp8u long_term_ref_pics_present_flag;
    Ipp8u sps_temporal_mvp_enable_flag;
    Ipp8u strong_intra_smoothing_enabled_flag;
    Ipp8u vui_parameters_present_flag;
    Ipp8u sps_extension_flag;
///////////// vars part

    Ipp32s num_ref_frames;
} H265SeqParameterSet;

typedef struct sH265PicParameterSet {
    Ipp8u pps_pic_parameter_set_id;
    Ipp8u pps_seq_parameter_set_id;
    Ipp8u dependent_slice_segments_enabled_flag;
    Ipp8u output_flag_present_flag;
    Ipp8u num_extra_slice_header_bits;
    Ipp8u sign_data_hiding_enabled_flag;
    Ipp8u cabac_init_present_flag;
    Ipp8u num_ref_idx_l0_default_active;
    Ipp8u num_ref_idx_l1_default_active;
    Ipp8s init_qp;
    Ipp8u constrained_intra_pred_flag;
    Ipp8u transform_skip_enabled_flag;
    Ipp8u cu_qp_delta_enabled_flag;
    Ipp8u diff_cu_qp_delta_depth;
    Ipp8s pps_cb_qp_offset;
    Ipp8s pps_cr_qp_offset;
    Ipp8u pps_slice_chroma_qp_offsets_present_flag;
    Ipp8u weighted_pred_flag;
    Ipp8u weighted_bipred_flag;
    Ipp8u transquant_bypass_enable_flag;
    Ipp8u tiles_enabled_flag;
    Ipp8u entropy_coding_sync_enabled_flag;
    Ipp8u num_tile_columns;
    Ipp8u num_tile_rows;
    Ipp8u uniform_spacing_flag;
    Ipp32u column_width[MAX_NUM_TILE_COLUMNS];
    Ipp32u row_height[MAX_NUM_TILE_ROWS];
    Ipp8u loop_filter_across_tiles_enabled_flag;
    Ipp8u pps_loop_filter_across_slices_enabled_flag;
    Ipp8u deblocking_filter_control_present_flag;
    Ipp8u deblocking_filter_override_enabled_flag;
    Ipp8u pps_deblocking_filter_disabled_flag;
    Ipp32s pps_beta_offset_div2;
    Ipp32s pps_tc_offset_div2;
    Ipp8u pps_scaling_list_data_present_flag;
    Ipp8u lists_modification_present_flag;
    Ipp8u log2_parallel_merge_level;
    Ipp8u slice_segment_header_extension_present_flag;
    Ipp8u pps_extension_flag;
} H265PicParameterSet;

class H265SliceHeader {
public:
    Ipp8u first_slice_segment_in_pic_flag;
    Ipp8u no_output_of_prior_pics_flag;
    Ipp8u slice_pic_parameter_set_id;
    Ipp8u dependent_slice_segment_flag;
    Ipp32s slice_segment_address;
    EnumSliceType slice_type;
    Ipp8u pic_output_flag;
    Ipp8u colour_plane_id;
    Ipp32u slice_pic_order_cnt_lsb;
    Ipp8u short_term_ref_pic_set_sps_flag;
    Ipp32u short_term_ref_pic_set_idx;
    Ipp32u num_long_term_pics;
    Ipp32u poc_lsb_lt[MAX_NUM_LONG_TERM_PICS];
    Ipp8u delta_poc_msb_present_flag[MAX_NUM_LONG_TERM_PICS];
    Ipp8u delta_poc_msb_cycle_lt[MAX_NUM_LONG_TERM_PICS];
    Ipp8u used_by_curr_pic_lt_flag[MAX_NUM_LONG_TERM_PICS];
    Ipp8u slice_temporal_mvp_enabled_flag;
    Ipp8u slice_sao_luma_flag;
    Ipp8u slice_sao_chroma_flag;
    Ipp8u num_ref_idx_active_override_flag;
    Ipp32u num_ref_idx_l0_active;
    Ipp32u num_ref_idx_l1_active;
    Ipp8u mvd_l1_zero_flag;
    Ipp8u cabac_init_flag;
    Ipp8u collocated_from_l0_flag;
    Ipp32u collocated_ref_idx;
    Ipp32u five_minus_max_num_merge_cand;
    Ipp32s slice_qp_delta;
    Ipp8s slice_cb_qp_offset;
    Ipp8s slice_cr_qp_offset;
    Ipp8u deblocking_filter_override_flag;
    Ipp8u slice_deblocking_filter_disabled_flag;
    Ipp32s slice_beta_offset_div2;
    Ipp32s slice_tc_offset_div2;
    Ipp8u slice_loop_filter_across_slices_enabled_flag;
    Ipp32u num_entry_point_offsets;
    Ipp32u offset_len;
    Ipp32u entry_point_offset[MAX_NUM_ENTRY_POINT_OFFSETS];

    Ipp16s weights[9][MAX_NUM_REF_FRAMES][MAX_NUM_REF_FRAMES];
    Ipp16s offsets[9][MAX_NUM_REF_FRAMES][MAX_NUM_REF_FRAMES];
    Ipp8u  luma_log2_weight_denom;             // luma weighting denominator
    Ipp8u  chroma_log2_weight_denom;           // chroma weighting denominator
};

class H265Slice : public H265SliceHeader
{
public:
    Ipp32s num_ref_idx[2];
    Ipp32s DependentSliceCurStartCUAddr;
    Ipp32s DependentSliceCurEndCUAddr;

    Ipp8u IdrPicFlag;
    Ipp8u RapPicFlag;
    Ipp32s slice_num;
    Ipp32u slice_address_last_ctb;
    Ipp32u row_first;
    Ipp32u row_last;
    H265EncoderRowInfo m_row_info;

    Ipp32s m_NumRefsInL0List;
    Ipp32s m_NumRefsInL1List;
    Ipp32u list_entry_l0[MAX_NUM_REF_FRAMES];
    Ipp32u list_entry_l1[MAX_NUM_REF_FRAMES];
    Ipp8u m_ref_pic_list_modification_flag_l0;
    Ipp8u m_ref_pic_list_modification_flag_l1;

    EncoderRefPicList *m_pRefPicList;

    H265Frame *GetRefFrame(EnumRefPicList ref_list, T_RefIdx ref_idx) {
        if (!m_pRefPicList || ref_idx < 0) return NULL;
        if (ref_list == REF_PIC_LIST_0) return m_pRefPicList->m_RefPicListL0.m_RefPicList[ref_idx];
        else m_pRefPicList->m_RefPicListL1.m_RefPicList[ref_idx];
    }
};

#endif // __MFX_H265_SET_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
