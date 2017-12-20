//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 - 2011 Intel Corporation. All Rights Reserved.
//
#ifndef __H264_STRUCT_H
#define __H264_STRUCT_H

#include <bs_def.h>
#define BS_MAX_SEI_PAYLOADS 64

typedef class H264_BitStream* BS_H264_hdl;

enum {
    BS_H264_INIT_MODE_DEFAULT = 0,
    BS_H264_INIT_MODE_CABAC   = 1,
    BS_H264_INIT_MODE_CAVLC   = 2
};

enum {
     BS_H264_TRACE_LEVEL_NALU     = 0x00000001,
     BS_H264_TRACE_LEVEL_SPS      = 0x00000002,
     BS_H264_TRACE_LEVEL_PPS      = 0x00000004,
     BS_H264_TRACE_LEVEL_SLICE    = 0x00000008,
     BS_H264_TRACE_LEVEL_SEI      = 0x00000010,
     BS_H264_TRACE_LEVEL_NALU_EXT = 0x00000020,
     BS_H264_TRACE_LEVEL_REF_LIST = 0x00000040,
     BS_H264_TRACE_LEVEL_AU       = 0x00000080,
     BS_H264_TRACE_LEVEL_MB       = 0x00000100,
     BS_H264_TRACE_LEVEL_FULL     = 0xFFFFFFFF
};

enum {
    BS_H264_PICTURE_MARKING_UNUSED_FOR_REFERENCE            = 0x0001,
    BS_H264_PICTURE_MARKING_USED_FOR_SHORT_TERM_REFERENCE   = 0x0002,
    BS_H264_PICTURE_MARKING_USED_FOR_LONG_TERM_REFERENCE    = 0x0004,
    BS_H264_PICTURE_MARKING_NON_EXISTING                    = 0x0008,
    BS_H264_PICTURE_MARKING_REFERENCE_BASE_PICTURE          = 0x0010,
    BS_H264_PICTURE_MARKING_NOT_AVAILABLE                   = 0x0020,
    BS_H264_PICTURE_MARKING_USED_FOR_REFERENCE              = BS_H264_PICTURE_MARKING_USED_FOR_SHORT_TERM_REFERENCE|BS_H264_PICTURE_MARKING_USED_FOR_LONG_TERM_REFERENCE
};

typedef struct {
    byte   bit_rate_scale : 4;
    byte   cpb_size_scale : 4;
    byte   initial_cpb_removal_delay_length_minus1;
    byte   cpb_removal_delay_length_minus1;
    byte   dpb_output_delay_length_minus1;
    byte   time_offset_length;
    Bs32u  cpb_cnt_minus1;
    Bs32u* bit_rate_value_minus1;
    Bs32u* cpb_size_value_minus1;
    byte*  cbr_flag;
}hrd_params;


struct op_params{
    byte   temporal_id;
    Bs32u  num_target_views_minus1;
    Bs32u* target_view_id;
    Bs32u  num_views_minus1;
};

struct op_vui_params{
    byte   temporal_id                      : 3;
    byte   timing_info_present_flag         : 1;
    byte   fixed_frame_rate_flag            : 1;
    byte   nal_hrd_parameters_present_flag  : 1;
    byte   vcl_hrd_parameters_present_flag  : 1;
    byte   low_delay_hrd_flag               : 1;
    byte   pic_struct_present_flag          : 1;

    Bs32u  num_target_output_views_minus1;
    Bs32u* view_id;

    Bs32u  num_units_in_tick;
    Bs32u  time_scale;   

    hrd_params* nal_hrd;
    hrd_params* vcl_hrd;
};

struct mvc_vui_params{
    Bs32u num_ops_minus1;
    op_vui_params* op;
};

typedef struct{
    byte   aspect_ratio_info_present_flag   : 1;

    byte   overscan_info_present_flag       : 1;
    byte   overscan_appropriate_flag        : 1;

    byte   video_signal_type_present_flag   : 1;
    byte   video_full_range_flag            : 1;
    byte   colour_description_present_flag  : 1;

    byte   chroma_loc_info_present_flag     : 1;
    byte   timing_info_present_flag         : 1;
    byte   fixed_frame_rate_flag            : 1;
    byte   nal_hrd_parameters_present_flag  : 1;
    byte   vcl_hrd_parameters_present_flag  : 1;
    
    byte   low_delay_hrd_flag               : 1;
    byte   pic_struct_present_flag          : 1;

    byte   bitstream_restriction_flag       : 1;
    byte   motion_vectors_over_pic_boundaries_flag : 1;

    byte   video_format;
    byte   colour_primaries;
    byte   transfer_characteristics;
    byte   matrix_coefficients;

    byte   aspect_ratio_idc;
    
    Bs16u  sar_width;
    Bs16u  sar_height;
    
    Bs32u  chroma_sample_loc_type_top_field;
    Bs32u  chroma_sample_loc_type_bottom_field;

    Bs32u  num_units_in_tick;
    Bs32u  time_scale;

    union{
        hrd_params* hrd;
        hrd_params* nal_hrd;
    };
    hrd_params* vcl_hrd;

    Bs32u  max_bytes_per_pic_denom;
    Bs32u  max_bits_per_mb_denom;
    Bs32u  log2_max_mv_length_horizontal;
    Bs32u  log2_max_mv_length_vertical;
    Bs32u  num_reorder_frames;
    Bs32u  max_dec_frame_buffering;
}vui_params;

struct view_params{
    Bs32u view_id;
    Bs32u num_anchor_refs_lx[2];
    Bs32u anchor_ref_lx[2][16];
    Bs32u num_non_anchor_refs_lx[2];
    Bs32u non_anchor_ref_lx[2][16];
};

struct level_params{
    byte        level_idc;
    Bs32u       num_applicable_ops_minus1;
    op_params*  applicable_op;
};

typedef struct{
    Bs32u           num_views_minus1;
    view_params*    view;
    Bs32u           num_level_values_signalled_minus1;
    level_params*   level;
    
    byte            mvc_vui_parameters_present_flag;
    mvc_vui_params* vui;
}sps_mvc_ext;


typedef struct{
    Bs8u dependency_id              : 3;
    Bs8u quality_id                 : 4;
    Bs8u timing_info_present_flag   : 1;

    Bs8u temporal_id                     : 3;
    Bs8u fixed_frame_rate_flag           : 1;
    Bs8u nal_hrd_parameters_present_flag : 1;
    Bs8u vcl_hrd_parameters_present_flag : 1;
    Bs8u low_delay_hrd_flag              : 1;
    Bs8u pic_struct_present_flag         : 1;

    Bs32u num_units_in_tick;
    Bs32u time_scale;

    hrd_params* nal_hrd;
    hrd_params* vcl_hrd;
}svc_vui_entry;

typedef struct{
    Bs8u inter_layer_deblocking_filter_control_present_flag : 1;
    Bs8u extended_spatial_scalability_idc                   : 2;
    Bs8u chroma_phase_x_plus1_flag                          : 1;
    Bs8u chroma_phase_y_plus1                               : 2;
    Bs8u seq_ref_layer_chroma_phase_x_plus1_flag            : 1;
    Bs8u seq_ref_layer_chroma_phase_y_plus1                 : 2;
    Bs8u seq_tcoeff_level_prediction_flag                   : 1;
    Bs8u adaptive_tcoeff_level_prediction_flag              : 1;
    Bs8u slice_header_restriction_flag                      : 1;
    Bs8u svc_vui_parameters_present_flag                    : 1;

    Bs32s seq_scaled_ref_layer_left_offset;
    Bs32s seq_scaled_ref_layer_top_offset;
    Bs32s seq_scaled_ref_layer_right_offset;
    Bs32s seq_scaled_ref_layer_bottom_offset;

    Bs32u vui_ext_num_entries_minus1;
    svc_vui_entry* vui;
}sps_svc_ext;

typedef struct{
    byte  scaling_list_present_flag[12];
    byte  ScalingList4x4[6][16];
    byte  UseDefaultScalingMatrix4x4Flag[6];
    byte  ScalingList8x8[6][64];
    byte  UseDefaultScalingMatrix8x8Flag[6];
}scaling_matrix;

typedef struct {
    byte   profile_idc;

    union{
        struct{
            byte   constraint_set0_flag : 1;
            byte   constraint_set1_flag : 1;
            byte   constraint_set2_flag : 1;
            byte   constraint_set3_flag : 1;
            byte   constraint_set4_flag : 1;
            byte   constraint_set5_flag : 1;
        };
        byte   constraint_set;
    };

    byte   separate_colour_plane_flag           : 1;
    byte   qpprime_y_zero_transform_bypass_flag : 1;
    byte   seq_scaling_matrix_present_flag      : 1;
    byte   delta_pic_order_always_zero_flag     : 1;
    byte   frame_mbs_only_flag                  : 1;
    byte   mb_adaptive_frame_field_flag         : 1;
    byte   gaps_in_frame_num_value_allowed_flag : 1;
    byte   direct_8x8_inference_flag            : 1;
    byte   frame_cropping_flag                  : 1;
    byte   vui_parameters_present_flag          : 1;

    byte   level_idc;
    Bs32u  seq_parameter_set_id;
    Bs32u  chroma_format_idc;
    Bs32u  bit_depth_luma_minus8;
    Bs32u  bit_depth_chroma_minus8;
    Bs32u  log2_max_frame_num_minus4;
    Bs32u  log2_max_pic_order_cnt_lsb_minus4;
    Bs32u  pic_order_cnt_type;
    Bs32s  offset_for_non_ref_pic;
    Bs32s  offset_for_top_to_bottom_field;
    Bs32u  num_ref_frames_in_pic_order_cnt_cycle;
    Bs32u  max_num_ref_frames;
    Bs32u  pic_width_in_mbs_minus1;
    Bs32u  pic_height_in_map_units_minus1;

    Bs32s* offset_for_ref_frame;
    scaling_matrix* seq_scaling_matrix;

    Bs32u  frame_crop_left_offset;
    Bs32u  frame_crop_right_offset; 
    Bs32u  frame_crop_top_offset;
    Bs32u  frame_crop_bottom_offset;

    vui_params* vui;

    sps_mvc_ext*    mvc_ext;
    sps_svc_ext*    svc_ext;

    union{//unused legacy fields
        Bs16u Locked;
        struct{
            Bs16u Active  : 1;
            Bs16u LockCnt : 15;
        };
    };
}seq_param_set;


typedef struct{
    Bs32u  pic_parameter_set_id;
    Bs32u  seq_parameter_set_id;

    byte   entropy_coding_mode_flag                     : 1;
    byte   bottom_field_pic_order_in_frame_present_flag : 1;
    byte   deblocking_filter_control_present_flag       : 1;
    byte   constrained_intra_pred_flag                  : 1;
    byte   redundant_pic_cnt_present_flag               : 1;
    byte   transform_8x8_mode_flag                      : 1;
    byte   pic_scaling_matrix_present_flag              : 1;
    byte   weighted_pred_flag                           : 1;
    byte   weighted_bipred_idc                          : 2;

    Bs32u  num_slice_groups_minus1;
    Bs32u  slice_group_map_type;
    union{
        Bs32u* run_length_minus1;
        struct {
            Bs32u* top_left;
            Bs32u* bottom_right;
        };
        struct {
            Bs32u  slice_group_change_rate_minus1;
            byte   slice_group_change_direction_flag;
        };
        struct{
            Bs32u  pic_size_in_map_units_minus1;
            byte*  slice_group_id;
        };
    };

    Bs32u  num_ref_idx_l0_default_active_minus1;
    Bs32u  num_ref_idx_l1_default_active_minus1;
    Bs32s  pic_init_qp_minus26;
    Bs32s  pic_init_qs_minus26;
    Bs32s  chroma_qp_index_offset;

    scaling_matrix* pic_scaling_matrix;

    Bs32s   second_chroma_qp_index_offset;

    seq_param_set* sps_active;

    union{ //unused legacy fields
        Bs16u Locked;
        struct{
            Bs16u Active  : 1;
            Bs16u LockCnt : 15;
        };
    };
}pic_param_set;


typedef struct dec_ref_pic_marking_params{
    Bs32u  difference_of_pic_nums_minus1;
    Bs32u  long_term_frame_idx;
    Bs32u  max_long_term_frame_idx_plus1;
    Bs32u  long_term_pic_num;

    Bs32u memory_management_control_operation;
    dec_ref_pic_marking_params* next;
}dec_ref_pic_marking_params;

struct nal_unit_header_mvc_extension{
    byte  non_idr_flag    : 1;
    byte  priority_id     : 6;
    byte  inter_view_flag : 1;
    byte  temporal_id     : 3;
    byte  anchor_pic_flag : 1;
    Bs32u view_id : 10;
};

struct nal_unit_header_svc_extension{
    byte idr_flag                 : 1; //All u(1)
    byte priority_id              : 6;// All u(6)
    byte no_inter_layer_pred_flag : 1;// All u(1)
    union{
        struct{
            byte quality_id     : 4; // All u(4)
            byte dependency_id  : 3;//All u(3)
        };
        byte DQId   : 7;
    };
    byte use_ref_base_pic_flag  : 1;//All u(1)
    byte temporal_id        : 3;// All u(3)
    byte discardable_flag   : 1;//All u(1)
    byte output_flag        : 1; //All u(1)
};

typedef struct {
    Bs32u modification_of_pic_nums_idc;
    union{
        Bs32u  abs_diff_pic_num_minus1;
        Bs32u  long_term_pic_num;
        Bs32u  abs_diff_view_idx_minus1;
    };
}ref_pic_list_modification;

struct mb_location{
    Bs32s Addr;
    Bs16u x;
    Bs16u y;
};

typedef struct {
    Bs8u prev_intra_pred_mode_flag : 1;
    Bs8u rem_intra_pred_mode       : 5;
}mb_intra_luma_pred;

struct mb_pred{
    union {
        struct /*intra*/ {
            mb_intra_luma_pred Blk[16];
            Bs8u intra_chroma_pred_mode;
        };
        struct /*inter*/ {
            Bs8u  ref_idx_lX[2][16];
            Bs16s mvd_lX[2][4][4][2];
        };
    };
};

struct coded_block_flags{
    Bs16u lumaDC  : 1;
    Bs16u CbDC    : 1;
    Bs16u CrDC    : 1;
    Bs16u luma8x8 : 4;
    Bs16u Cb8x8   : 4;
    Bs16u Cr8x8   : 4;
    Bs16u luma4x4;
    Bs16u Cb4x4;
    Bs16u Cr4x4;
};

struct coeff_tokens{
    Bs16u lumaDC;
    Bs16u CbDC;
    Bs16u CrDC;
    Bs16u luma4x4[16];
    Bs16u Cb4x4[16];
    Bs16u Cr4x4[16];
};

struct macro_block : mb_location{
    Bs8u  mb_skip_flag            : 1;
    Bs8u  end_of_slice_flag       : 1;
    Bs8u  mb_field_decoding_flag  : 1;
    Bs8u  transform_size_8x8_flag : 1;
    Bs8u  mb_type                 : 6;
    Bs8u  coded_block_pattern;
    Bs8s  mb_qp_delta;
    Bs8s  sub_mb_type[4];

    mb_pred pred;
    union{
        coded_block_flags coded_block_flag; //CABAC
        coeff_tokens coeff_token;           //CAVLC
    };
    Bs16u numBits;
};

typedef struct slice_header{
    Bs32u  first_mb_in_slice;
    Bs32u  slice_type;
    Bs32u  pic_parameter_set_id;
    
    Bs32u  frame_num;

    byte   colour_plane_id                   : 2;
    byte   field_pic_flag                    : 1;
    byte   bottom_field_flag                 : 1;
    
    byte   direct_spatial_mv_pred_flag       : 1;
    byte   num_ref_idx_active_override_flag  : 1;
    
    byte   ref_pic_list_modification_flag_l0 : 1;
    byte   ref_pic_list_modification_flag_l1 : 1;

    byte   sp_for_switch_flag                : 1;
    
    byte   no_output_of_prior_pics_flag      : 1;
    byte   long_term_reference_flag          : 1;
    byte   adaptive_ref_pic_marking_mode_flag: 1;
    
    Bs32u  idr_pic_id;

    Bs32u  pic_order_cnt_lsb;
    Bs32s  delta_pic_order_cnt_bottom;
    Bs32s  delta_pic_order_cnt[2];
    
    Bs32u  redundant_pic_cnt;
    Bs32u  num_ref_idx_l0_active_minus1;
    Bs32u  num_ref_idx_l1_active_minus1;

    ref_pic_list_modification* ref_pic_list_modification_lx[2];

    struct pwt{
        Bs32u luma_log2_weight_denom;
        Bs32u chroma_log2_weight_denom;

        struct pwt_lx{
            byte  luma_weight_lx_flag;
            Bs32s luma_weight_lx;
            Bs32s luma_offset_lx;
            byte  chroma_weight_lx_flag;
            Bs32s chroma_weight_lx[2];
            Bs32s chroma_offset_lx[2];
        }*listX[2];
    }*pred_weight_table;

    dec_ref_pic_marking_params* dec_ref_pic_marking;

    Bs8s   slice_qp_delta; // max 51
    Bs8s   slice_qs_delta; // max 51
    Bs32u  cabac_init_idc;
    Bs32u  slice_group_change_cycle;

    Bs32u  disable_deblocking_filter_idc;
    Bs32s  slice_alpha_c0_offset_div2;
    Bs32s  slice_beta_offset_div2;

    byte IdrPicFlag         : 1;
    byte isReferencePicture : 1;
    byte ExtensionID        : 2; // 0 - no extension, 1 - svc, 2 - mvc
    union{
        nal_unit_header_mvc_extension mvc_ext;
        nal_unit_header_svc_extension svc_ext;
    };

    struct _svc_ext{
        Bs8u  base_pred_weight_table_flag         : 1;
        Bs8u  store_ref_base_pic_flag             : 1;
        Bs8u  constrained_intra_resampling_flag   : 1;
        Bs8u  ref_layer_chroma_phase_x_plus1_flag : 1;
        Bs8u  ref_layer_chroma_phase_y_plus1      : 2;
        Bs8u  slice_skip_flag                     : 1;
        Bs8u  adaptive_ref_base_pic_marking_mode_flag : 1;

        Bs32u idr_pic_id;
        Bs32s ref_layer_dq_id;
        Bs32u disable_inter_layer_deblocking_filter_idc;

        Bs32s inter_layer_slice_alpha_c0_offset_div2;
        Bs32s inter_layer_slice_beta_offset_div2;

        Bs32s scaled_ref_layer_left_offset;
        Bs32s scaled_ref_layer_top_offset;
        Bs32s scaled_ref_layer_right_offset;
        Bs32s scaled_ref_layer_bottom_offset;

        union{
            Bs32u num_mbs_in_slice_minus1;
            struct{
                Bs32u adaptive_base_mode_flag           : 1;
                Bs32u default_base_mode_flag            : 1;
                Bs32u adaptive_motion_prediction_flag   : 1;
                Bs32u default_motion_prediction_flag    : 1;
                Bs32u adaptive_residual_prediction_flag : 1;
                Bs32u default_residual_prediction_flag  : 1;
                Bs32u tcoeff_level_prediction_flag      : 1;
                Bs32u scan_idx_start : 4;
                Bs32u scan_idx_end   : 4;
            };
        };

        dec_ref_pic_marking_params* dec_ref_base_pic_marking;
    }*se;

    Bs32u PicOrderCntMsb;
    Bs32u FrameNumOffset;

    Bs32s TopFieldOrderCnt;
    Bs32s BottomFieldOrderCnt;
    Bs32s PicOrderCnt;

    Bs32s FrameNumWrap;
    byte  PictureMarking;
    Bs32s PicNum;
    Bs32s LongTermPicNum;
    Bs32s LongTermFrameIdx;
    slice_header** RefPicList[2];
    slice_header*  opp_field; // interlace only

    Bs16u ViewID; // 10 bit

    slice_header* prev_pic;
    slice_header* prev_ref_pic;
    pic_param_set* pps_active;
    seq_param_set* sps_active;

    Bs64u BinCount;
    Bs32u NumMb;
    macro_block* mb;
}slice_header;

typedef struct {
    Bs32u slice_id;
    byte  colour_plane_id; //u(2)
    Bs32u redundant_pic_cnt;
}slice_part;

typedef struct {
    Bs32u cpb_removal_delay;
    Bs32u dpb_output_delay;
    byte  pic_struct;

    byte  NumClockTS;
    struct {
        byte  clock_timestamp_flag  : 1;//[ i ] 5 u(1)
        byte  ct_type               : 2;// 5 u(2)
        byte  counting_type         : 5;// 5 u(5)

        byte  nuit_field_based_flag : 1;// 5 u(1)
        byte  full_timestamp_flag   : 1;// 5 u(1)
        byte  seconds_value         : 6;// /* 0..59 */ 5 u(6)

        byte  discontinuity_flag    : 1;// 5 u(1)
        byte  cnt_dropped_flag      : 1;// 5 u(1)
        byte  minutes_value         : 6;// /* 0..59 */ 5 u(6)

        byte  hours_value           : 5;// /* 0..23 */ 5 u(5)
        byte  seconds_flag          : 1;// 5 u(1)
        byte  minutes_flag          : 1;// 5 u(1)
        byte  hours_flag            : 1;// 5 u(1)

        byte  n_frames;// 5 u(8)
        Bs32u time_offset;// 5 i(v)
    }ClockTS[3];

}pic_timing_params;

typedef struct{
    byte  original_idr_flag;// 5 u(1)
    Bs32u original_frame_num;//  5 ue(v)
    byte  original_field_pic_flag;//  5 u(1)
    byte  original_bottom_field_flag;//  5 u(1)
    
    byte   no_output_of_prior_pics_flag;
    byte   long_term_reference_flag;
    byte   adaptive_ref_pic_marking_mode_flag;
    dec_ref_pic_marking_params* dec_ref_pic_marking;
}dec_ref_pic_marking_repetition_params;

typedef struct{
    Bs32u recovery_frame_cnt;       // 5 ue(v)
    byte  exact_match_flag;         // 5 u(1)
    byte  broken_link_flag;         // 5 u(1)
    byte  changing_slice_group_idc; // 5 u(2)
}recovery_point_params;

typedef struct{
    Bs32u seq_parameter_set_id;// 5 ue(v)
    struct{
        Bs32u* initial_cpb_removal_delay;// 5 u(v)
        Bs32u* initial_cpb_removal_delay_offset;// 5 u(v)
    }nal, vcl;

    seq_param_set* sps_active;
}buffering_period_params;

typedef struct{
    Bs8u  temporal_id_nesting_flag          : 1;// 5 u(1)
    Bs8u  priority_layer_info_present_flag  : 1;// 5 u(1)
    Bs8u  priority_id_setting_flag          : 1;// 5 u(1)
    Bs32u num_layers_minus1;// 5 ue(v)

    struct _layer{
        Bs32u layer_id;//[ i ] 5 ue(v)
        Bs8u  priority_id            : 6;//[ i ] 5 u(6)
        Bs8u  discardable_flag       : 1;//[ i ] 5 u(1)
        Bs8u  dependency_id          : 3;//[ i ] 5 u(3)
        Bs8u  quality_id             : 4;//[ i ] 5 u(4)
        Bs8u  temporal_id            : 3;//[ i ] 5 u(3)
        Bs8u  sub_pic_layer_flag     : 1;//[ i ] 5 u(1)
        Bs8u  sub_region_layer_flag  : 1;//[ i ] 5 u(1)
        Bs8u  iroi_division_info_present_flag   : 1;//[ i ] 5 u(1)
        Bs8u  profile_level_info_present_flag   : 1;//[ i ] 5 u(1)
        Bs8u  bitrate_info_present_flag         : 1;//[ i ] 5 u(1)
        Bs8u  frm_rate_info_present_flag        : 1;//[ i ] 5 u(1)
        Bs8u  frm_size_info_present_flag        : 1;//[ i ] 5 u(1)
        Bs8u  layer_dependency_info_present_flag      : 1;//[ i ] 5 u(1)
        Bs8u  parameter_sets_info_present_flag        : 1;//[ i ] 5 u(1)
        Bs8u  bitstream_restriction_info_present_flag : 1;//[ i ] 5 u(1)
        Bs8u  exact_inter_layer_pred_flag             : 1;//[ i ] 5 u(1)
        Bs8u  exact_sample_value_match_flag : 1;//[ i ] 5 u(1)
        Bs8u  layer_conversion_flag         : 1;//[ i ] 5 u(1)
        Bs8u  layer_output_flag             : 1;//[ i ] 5 u(1)
        union {
            Bs32u layer_profile_level_idc       : 24;//[ i ] 5 u(24)
            struct{
                Bs8u level_idc;
                Bs8u reserved_zero_2bits  : 2;
                Bs8u constraint_set5_flag : 1;
                Bs8u constraint_set4_flag : 1;
                Bs8u constraint_set3_flag : 1;
                Bs8u constraint_set2_flag : 1;
                Bs8u constraint_set1_flag : 1;
                Bs8u constraint_set0_flag : 1;
                Bs8u profile_idc;
            };
        };
        
        Bs16u avg_bitrate;//[ i ] 5 u(16)
        Bs16u max_bitrate_layer;//[ i ] 5 u(16)
        Bs16u max_bitrate_layer_representation;//[ i ] 5 u(16)
        Bs16u max_bitrate_calc_window;//[ i ] 5 u(16)
        
        Bs8u  constant_frm_rate_idc : 2;//[ i ] 5 u(2)
        Bs16u avg_frm_rate;//[ i ] 5 u(16)

        Bs32u frm_width_in_mbs_minus1;//[ i ] 5 ue(v)
        Bs32u frm_height_in_mbs_minus1;//[ i ] 5 ue(v)
        
        Bs32u base_region_layer_id;//[ i ] 5 ue(v)
        Bs8u  dynamic_rect_flag : 1;//[ i ] 5 u(1)
        Bs16u horizontal_offset;//[ i ] 5 u(16)
        Bs16u vertical_offset;//[ i ] 5 u(16)
        Bs16u region_width;//[ i ] 5 u(16)
        Bs16u region_height;//[ i ] 5 u(16)

        Bs32u roi_id;//[ i ] 5 ue(v)

        Bs8u  iroi_grid_flag : 1;//[ i ] 5 u(1)
        union{
            struct{
                Bs32u grid_width_in_mbs_minus1;//[ i ] 5 ue(v)
                Bs32u grid_height_in_mbs_minus1;//[ i ] 5 ue(v)
            };
            struct{
                Bs32u num_rois_minus1;//[ i ] 5 ue(v)

                struct{
                    Bs32u first_mb_in_roi;//[ i ][ j ] 5 ue(v)
                    Bs32u roi_width_in_mbs_minus1;//[ i ][ j ] 5 ue(v)
                    Bs32u roi_height_in_mbs_minus1;//[ i ][ j ] 5 ue(v)
                }*roi;
            };
        };

        union{
            struct{
                Bs32u  num_directly_dependent_layers;//[ i ] 5 ue(v)
                Bs32u* directly_dependent_layer_id_delta_minus1;//[ i ][ j ] 5 ue(v)
            };
            Bs32u layer_dependency_info_src_layer_id_delta;//[ i ] 5 ue(v)
        };

        union{
            struct{
                Bs32u  num_seq_parameter_set_minus1;//[ i ] 5 ue(v)
                Bs32u* seq_parameter_set_id_delta;//[ i ][ j ] 5 ue(v)
                Bs32u  num_subset_seq_parameter_set_minus1;//[ i ] 5 ue(v)
                Bs32u* subset_seq_parameter_set_id_delta;//[ i ][ j ] 5 ue(v)
                Bs32u  num_pic_parameter_set_minus1;//[ i ] 5 ue(v)
                Bs32u* pic_parameter_set_id_delta;//[ i ][ j ] 5 ue(v)
            };
            Bs32u parameter_sets_info_src_layer_id_delta;//[ i ] 5 ue(v)
        };

        Bs8u  motion_vectors_over_pic_boundaries_flag : 1;//[ i ] 5 u(1)
        Bs32u max_bytes_per_pic_denom;//[ i ] 5 ue(v)
        Bs32u max_bits_per_mb_denom;//[ i ] 5 ue(v)
        Bs32u log2_max_mv_length_horizontal;//[ i ] 5 ue(v)
        Bs32u log2_max_mv_length_vertical;//[ i ] 5 ue(v)
        Bs32u num_reorder_frames;//[ i ] 5 ue(v)
        Bs32u max_dec_frame_buffering;//[ i ] 5 ue(v)

        Bs32u conversion_type_idc;//[ i ] 5 ue(v)
        struct{
            Bs32u rewriting_info_flag           :  1;//[ i ][ j ] 5 u(1)
            Bs32u rewriting_profile_level_idc   : 24;//[ i ][ j ] 5 u(24)
            Bs16u rewriting_avg_bitrate;//[ i ][ j ] 5 u(16)
            Bs16u rewriting_max_bitrate;//[ i ][ j ] 5 u(16)
        }rw[2];
    }*layer;

    Bs32u pr_num_dId_minus1;// 5 ue(v)
    struct _dl{
        Bs8u  pr_dependency_id;//[ i ] 5 u(3)
        Bs32u pr_num_minus1;//[ i ] 5 ue(v)
        struct _pr {
            Bs32u pr_id;//[ i ][ j ] 5 ue(v)
            union{
                Bs32u pr_profile_level_idc : 24;//[ i ][ j ] 5 u(24)
                struct{
                    Bs8u level_idc;
                    Bs8u reserved_zero_2bits  : 2;
                    Bs8u constraint_set5_flag : 1;
                    Bs8u constraint_set4_flag : 1;
                    Bs8u constraint_set3_flag : 1;
                    Bs8u constraint_set2_flag : 1;
                    Bs8u constraint_set1_flag : 1;
                    Bs8u constraint_set0_flag : 1;
                    Bs8u profile_idc;
                };
            };
            Bs16u pr_avg_bitrate;//[ i ][ j ] 5 u(16)
            Bs16u pr_max_bitrate;//[ i ][ j ] 5 u(16)
        }*pr;
    }*dl;


    Bs8u* priority_id_setting_uri;//[ PriorityIdSettingUriIdx ] 5 b(8)
}scalability_info_params;

typedef struct mvc_scalable_nesting_params mvc_scalable_nesting_params;

typedef struct sei_params{
    struct sei_msg{
        Bs32u payloadType;
        Bs32u payloadSize;
        union{
            Bs8u* payload;
            buffering_period_params* buffering_period;
            pic_timing_params* pic_timing;
            dec_ref_pic_marking_repetition_params* dec_ref_pic_marking_repetition;
            mvc_scalable_nesting_params* mvc_scalable_nesting;
            scalability_info_params* scalability_info;
            recovery_point_params* recovery_point;
        };
    }message[BS_MAX_SEI_PAYLOADS];
    Bs32u numMessages;

    pic_timing_params* pic_timing;
    dec_ref_pic_marking_repetition_params* dec_ref_pic_marking_repetition;
}sei_params;

typedef struct mvc_scalable_nesting_params{
    byte  operation_point_flag              : 1; // 5 u(1)
    byte  all_view_components_in_au_flag    : 1; // 5 u(1)
    byte  sei_op_temporal_id                : 3; // 5 u(3)
    Bs32u num_view_components_minus1;            // 5 ue(v)
    Bs16u *sei_view_id;                          //[ i ] 5 u(10)
    Bs32u num_view_components_op_minus1;         // 5 ue(v)
    Bs16u *sei_op_view_id;                       //[ i ] 5 u(10)
    sei_params::sei_msg message;                 //sei_message( ) 5
}mvc_scalable_nesting_params;

struct access_unit_delimiter{
    byte primary_pic_type;// 6 u(3)
};

typedef struct {
    byte nal_unit_type;
    byte nal_ref_idc;

    seq_param_set* SPS;
    pic_param_set* PPS;
    sei_params*    SEI;
    slice_header*  slice_hdr;
    slice_part* slice_partition;
    slice_header::_svc_ext* se;

    byte svc_extension_flag;
    nal_unit_header_svc_extension* svc_ext;
    nal_unit_header_mvc_extension* mvc_ext;
    access_unit_delimiter* AUD;

    Bs32u NumBytesInNALunit;
    Bs32u NumBytesInRBSP;
}BS_H264_header;

struct BS_H264_au {
    Bs32u NumUnits;
    struct nalu_param{
        byte nal_ref_idc   : 2;
        byte nal_unit_type : 5;
        byte Locked        : 1; //unused legacy field
        
        Bs32u NumBytesInNALunit;
        Bs32u NumBytesInRBSP;

        union{
            access_unit_delimiter* AUD;
            seq_param_set*         SPS;
            pic_param_set*         PPS;
            sei_params*            SEI;
            struct{
                slice_header* slice_hdr;
                slice_part*   slice_partition;
            };
            struct{ // prefix NALU only
                union{
                    nal_unit_header_svc_extension* svc_ext;
                    nal_unit_header_mvc_extension* mvc_ext;
                };
                byte svc_extension_flag;
                slice_header::_svc_ext* se;
            };
        };
    }*NALU;
};

#endif //__H264_STRUCT_H
