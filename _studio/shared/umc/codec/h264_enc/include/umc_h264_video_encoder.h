//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2012 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#ifndef __UMC_H264_VIDEO_ENCODER_H__
#define __UMC_H264_VIDEO_ENCODER_H__

#include "ippdefs.h"
#include "ippmsdk.h"
#include "umc_video_data.h"
#include "umc_video_encoder.h"
#include "umc_h264_config.h"
#include "umc_video_brc.h"

namespace UMC
{

// Slice Group definitions
#define MAX_NUM_SLICE_GROUPS 8
#define MAX_SLICE_GROUP_MAP_TYPE 6
#define MAX_CPB_CNT 32

#define MAX_DEP_LAYERS      8
#define MAX_TEMP_LEVELS     8
#define MAX_QUALITY_LEVELS  16
#define MAX_SVC_LAYERS      MAX_DEP_LAYERS * MAX_TEMP_LEVELS * MAX_QUALITY_LEVELS

    typedef enum {
        H264_BASE_PROFILE     = 66,
        H264_MAIN_PROFILE     = 77,
        H264_SCALABLE_BASELINE_PROFILE  = 83,
        H264_SCALABLE_HIGH_PROFILE      = 86,
        H264_EXTENDED_PROFILE = 88,
        H264_HIGH_PROFILE     = 100,
        H264_HIGH10_PROFILE   = 110,
        H264_MULTIVIEWHIGH_PROFILE  = 118,
        H264_STEREOHIGH_PROFILE  = 128,
        H264_HIGH422_PROFILE  = 122,
        H264_HIGH444_PROFILE  = 144
    } H264_PROFILE_IDC;

    typedef Ipp32s H264_Key_Frame_Control_Method;

    const H264_Key_Frame_Control_Method      H264_KFCM_AUTO     = 0;
    // Let the encoder decide when to generate key frames.
    // This method typically causes the least number of key frames to
    // be generated.

    const H264_Key_Frame_Control_Method      H264_KFCM_INTERVAL = 1;
    // Generate key frames at a regular interval

    typedef enum {
        H264_RCM_QUANT     = 0, // Fix quantizer values, no actual rate control.
        H264_RCM_CBR       = 1,
        H264_RCM_VBR       = 2,
        H264_RCM_DEBUG     = 3, // The same as H264_RCM_QUANT
        H264_RCM_CBR_SLICE = 4,
        H264_RCM_VBR_SLICE = 5,
        H264_RCM_AVBR      = 6,
    } H264_Rate_Control_Method;

    typedef struct {
        H264_Rate_Control_Method   method;
        Ipp8s                      quantI;
        Ipp8s                      quantP;
        Ipp8s                      quantB;
        Ipp32s                     accuracy;
        Ipp32s                     convergence;
    } H264_Rate_Controls;

    typedef struct {
        H264_Key_Frame_Control_Method method;
        Ipp32s                        interval;
        Ipp32s                        idr_interval;
        // 'interval' is meaningful only when method == H264_KFCM_INTERVAL.
        // It specifies the frequency of key frames.  A value of 1000,
        // for example, means that a key frame should be generated
        // approximately every 1000 frames.  A value of 1 means that
        // every frame should be a key frame.
        // The interval must always be >= 1.

    } H264_Key_Frame_Controls;

} // namespace UMC

namespace UMC
{

struct SliceGroupInfoStruct
{
    Ipp8u slice_group_map_type;               // 0..6
    // The additional slice group data depends upon map type
    union {
        // type 0
        Ipp32u run_length[MAX_NUM_SLICE_GROUPS];
        // type 2
        struct {
            Ipp32u top_left[MAX_NUM_SLICE_GROUPS-1];
            Ipp32u bottom_right[MAX_NUM_SLICE_GROUPS-1];
        } t1;
        // types 3-5
        struct {
            Ipp8u  slice_group_change_direction_flag;
            Ipp32u slice_group_change_rate;
        } t2;
        // type 6
        struct {
            Ipp32u pic_size_in_map_units;       // number of macroblocks if no field coding
            Ipp8u *pSliceGroupIDMap;            // Id for each slice group map unit
        } t3;
    };
};  // SliceGroupInfoStruct

struct  H264SEIData_BufferingPeriod{
    Ipp8u  seq_parameter_set_id;
    Ipp32u nal_initial_cpb_removal_delay[32];
    Ipp32u nal_initial_cpb_removal_delay_offset[32];
    Ipp32u vcl_initial_cpb_removal_delay[32];
    Ipp32u vcl_initial_cpb_removal_delay_offset[32];
};

struct  H264SEIData_PictureTiming{
    Ipp32u cpb_removal_delay;
    Ipp32u cpb_removal_delay_accumulated;
    Ipp32u dpb_output_delay;
    Ipp8u  initial_dpb_output_delay;
    Ipp8u  pic_struct;
    Ipp16u  clock_timestamp_flag[3];
    Ipp16u  ct_type[3];
    Ipp16u  nuit_field_based_flag[3];
    Ipp16u  counting_type[3];
    Ipp16u  full_timestamp_flag[3];
    Ipp16u  discontinuity_flag[3];
    Ipp16u  cnt_dropped_flag[3];
    Ipp16u  n_frames[3];
    Ipp16u  seconds_value[3];
    Ipp16u  minutes_value[3];
    Ipp16u  hours_value[3];
    Ipp16u  seconds_flag[3];
    Ipp16u  minutes_flag[3];
    Ipp16u  hours_flag[3];
    Ipp32s time_offset[3];
    Ipp32u DeltaTfiAccumulated;
};

struct  H264SEIData_DecRefPicMarkingRepetition{
    bool   isCoded;
    Ipp8u  original_idr_flag;
    Ipp16u original_frame_num; // theoretical max frame_num is 2^16 - 1
    Ipp8u  original_field_pic_flag;
    Ipp8u  original_bottom_field_flag;
    Ipp8u  dec_ref_pic_marking[192]; // buffer size is enough for any dec_ref_pic_marking() syntax
    Ipp8u  sizeInBytes;
    Ipp16u bitOffset;
};

struct H264SEIData
{
    H264SEIData_BufferingPeriod            BufferingPeriod;
    H264SEIData_PictureTiming              PictureTiming;
    H264SEIData_DecRefPicMarkingRepetition DecRefPicMrkRep;
#ifdef H264_HRD_CPB_MODEL
    Ipp64f m_tickDuration;
    Ipp64f tai_n;
    Ipp64f tai_n_minus_1;
    Ipp64f tai_earliest;
    Ipp64f taf_n;
    Ipp64f taf_n_minus_1;
    Ipp64f trn_n;
    Ipp64f trn_nb;
    Ipp32u m_recentFrameEncodedSize;
#endif // H264_HRD_CPB_MODEL
    Ipp8u m_insertedSEI[36];
};

struct H264VUI_HRDParams {
    Ipp8u cpb_cnt_minus1;
    Ipp32u bit_rate_scale;
    Ipp32u cpb_size_scale;

    Ipp32u bit_rate_value_minus1[MAX_CPB_CNT];
    Ipp32u cpb_size_value_minus1[MAX_CPB_CNT];
    Ipp8u cbr_flag[MAX_CPB_CNT];

    Ipp8u initial_cpb_removal_delay_length_minus1;
    Ipp8u cpb_removal_delay_length_minus1;
    Ipp8u dpb_output_delay_length_minus1;
    Ipp8u time_offset_length;
};

//SVC VUI parameter
struct SVCVUIParams {
    Ipp8u  vui_ext_dependency_id;
    Ipp8u  vui_ext_quality_id;
    Ipp8u  vui_ext_temporal_id;
    Ipp8u  vui_ext_timing_info_present_flag;
    //if( vui_ext_timing_info_present_flag ) {
    Ipp32u vui_ext_num_units_in_tick;
    Ipp32u vui_ext_time_scale;
    Ipp8u  vui_ext_fixed_frame_rate_flag;
    //}
    Ipp8u  vui_ext_nal_hrd_parameters_present_flag;
    //if( vui_ext_nal_hrd_parameters_present_flag )
    H264VUI_HRDParams nal_hrd_parameters;
    Ipp8u  vui_ext_vcl_hrd_parameters_present_flag;
    //if( vui_ext_vcl_hrd_parameters_present_flag )
    H264VUI_HRDParams vcl_hrd_parameters;
    //if( vui_ext_nal_hrd_parameters_present_flag || vui_ext_vcl_hrd_parameters_present_flag )
    Ipp8u  vui_ext_low_delay_hrd_flag;
    Ipp8u  vui_ext_pic_struct_present_flag;
    Ipp32u initial_delay_bytes;
};

struct H264VUIParams {
    Ipp8u   aspect_ratio_info_present_flag;
    Ipp8u   aspect_ratio_idc;
    Ipp16u   sar_width;
    Ipp16u   sar_height;

    Ipp8u   overscan_info_present_flag;
    Ipp8u   overscan_appropriate_flag;

    Ipp8u  video_signal_type_present_flag;
    Ipp8u  video_format;
    Ipp8u  video_full_range_flag;
    Ipp8u  colour_description_present_flag;
    Ipp8u  colour_primaries;
    Ipp8u  transfer_characteristics;
    Ipp8u  matrix_coefficients;

    Ipp8u  chroma_loc_info_present_flag;
    Ipp8u  chroma_sample_loc_type_top_field;
    Ipp8u  chroma_sample_loc_type_bottom_field;

    Ipp8u  timing_info_present_flag;
    Ipp32u num_units_in_tick;
    Ipp32u time_scale;
    Ipp8u  fixed_frame_rate_flag;

    Ipp8u  nal_hrd_parameters_present_flag;
    H264VUI_HRDParams hrd_params;
    Ipp8u  vcl_hrd_parameters_present_flag;
    H264VUI_HRDParams vcl_hrd_params;
    Ipp8u  low_delay_hrd_flag;

    Ipp8u  pic_struct_present_flag;
    Ipp8u  bitstream_restriction_flag;
    Ipp8u  motion_vectors_over_pic_boundaries_flag;
    Ipp32u  max_bytes_per_pic_denom;
    Ipp32u  max_bits_per_mb_denom;
    Ipp8u  log2_max_mv_length_horizontal;
    Ipp8u  log2_max_mv_length_vertical;
    Ipp8u  num_reorder_frames;
    Ipp16u  max_dec_frame_buffering;
};

// Sequence parameter set structure, corresponding to the H.264 bitstream definition.
struct H264SeqParamSet
{
    H264_PROFILE_IDC profile_idc;                   // baseline, main, etc.

    Ipp8s       level_idc;
    Ipp8s       constraint_set0_flag;               // nonzero: bitstream obeys all set 0 constraints
    Ipp8s       constraint_set1_flag;               // nonzero: bitstream obeys all set 1 constraints
    Ipp8s       constraint_set2_flag;               // nonzero: bitstream obeys all set 2 constraints
    Ipp8s       constraint_set3_flag;               // nonzero: bitstream obeys all set 3 constraints
    Ipp8s       constraint_set4_flag;               // nonzero: bitstream obeys all set 4 constraints
    Ipp8s       constraint_set5_flag;               // nonzero: bitstream obeys all set 5 constraints
    Ipp8s       chroma_format_idc;

    Ipp8s       seq_parameter_set_id;               // id of this sequence parameter set
    Ipp8s       log2_max_frame_num;                 // Number of bits to hold the frame_num
    Ipp8s       pic_order_cnt_type;                 // Picture order counting method

    Ipp8s       delta_pic_order_always_zero_flag;   // If zero, delta_pic_order_cnt fields are
    // present in slice header.
    Ipp8s       frame_mbs_only_flag;                // Nonzero indicates all pictures in sequence
    // are coded as frames (not fields).
    Ipp8s       gaps_in_frame_num_value_allowed_flag;

    Ipp8s       mb_adaptive_frame_field_flag;       // Nonzero indicates frame/field switch
    // at macroblock level
    Ipp8s       direct_8x8_inference_flag;          // Direct motion vector derivation method
    Ipp8s       vui_parameters_present_flag;        // Zero indicates default VUI parameters
    H264VUIParams vui_parameters;                   // VUI parameters if it is going to be used
    Ipp8s       frame_cropping_flag;                // Nonzero indicates frame crop offsets are present.
    Ipp32s      frame_crop_left_offset;
    Ipp32s      frame_crop_right_offset;
    Ipp32s      frame_crop_top_offset;
    Ipp32s      frame_crop_bottom_offset;
    Ipp32s      log2_max_pic_order_cnt_lsb;         // Value of MaxPicOrderCntLsb.
    Ipp32s      offset_for_non_ref_pic;             // !!! is unused now

    Ipp32s      offset_for_top_to_bottom_field;     // !!! is unused now // Expected pic order count difference from
    // top field to bottom field.

    Ipp32s      num_ref_frames_in_pic_order_cnt_cycle; // !!! is unused now
    Ipp32s      *poffset_for_ref_frame;             // !!! is unused now // pointer to array of stored frame offsets,
    // length num_stored_frames_in_pic_order_cnt_cycle,
    // for pic order cnt type 1
    Ipp32s      num_ref_frames;                     // total number of pics in decoded pic buffer
    Ipp32s      frame_width_in_mbs;
    Ipp32s      frame_height_in_mbs;

    // These fields are calculated from values above.  They are not written to the bitstream
    Ipp32s      MaxMbAddress;
    Ipp32s      MaxPicOrderCntLsb;
    Ipp32s      aux_format_idc;                     // See H.264 standard for details.
    Ipp32s      bit_depth_aux;
    Ipp32s      bit_depth_luma;
    Ipp32s      bit_depth_chroma;
    Ipp32s      alpha_incr_flag;
    Ipp32s      alpha_opaque_value;
    Ipp32s      alpha_transparent_value;

    bool        seq_scaling_matrix_present_flag;
    bool        seq_scaling_list_present_flag[8];
    Ipp8u       seq_scaling_list_4x4[6][16];
    Ipp8u       seq_scaling_list_8x8[2][64];

    Ipp16s      seq_scaling_matrix_4x4[6][6][16];
    Ipp16s      seq_scaling_matrix_8x8[2][6][64];
    Ipp16s      seq_scaling_inv_matrix_4x4[6][6][16];
    Ipp16s      seq_scaling_inv_matrix_8x8[2][6][64];

    bool        pack_sequence_extension;
    Ipp32s      qpprime_y_zero_transform_bypass_flag;
    bool        residual_colour_transform_flag;
    Ipp32s      additional_extension_flag;

    Ipp32u     *mvc_extension_payload;              // Externally allocated and initialized MVC extension payload
    Ipp32u      mvc_extension_size;                 // Size of MVC extension payload in elements;


// SVC extension
    Ipp8u inter_layer_deblocking_filter_control_present_flag;
    Ipp8u extended_spatial_scalability;
    Ipp8u chroma_phase_x_plus1;
    Ipp8u chroma_phase_y_plus1;
    Ipp8u seq_ref_layer_chroma_phase_x_plus1;
    Ipp8u seq_ref_layer_chroma_phase_y_plus1;

    Ipp8u seq_tcoeff_level_prediction_flag;
    Ipp8u adaptive_tcoeff_level_prediction_flag;
    Ipp8u slice_header_restriction_flag;

    Ipp32s seq_scaled_ref_layer_left_offset;
    Ipp32s seq_scaled_ref_layer_top_offset;
    Ipp32s seq_scaled_ref_layer_right_offset;
    Ipp32s seq_scaled_ref_layer_bottom_offset;

    SVCVUIParams *svc_vui_parameters;
    Ipp8u svc_vui_parameters_present_flag;
    Ipp8u vui_ext_num_entries;
};  // H264SeqParamSet

// Picture parameter set structure, corresponding to the H.264 bitstream definition.
struct H264PicParamSet
{
    Ipp8s       pic_parameter_set_id;           // of this picture parameter set
    Ipp8s       seq_parameter_set_id;           // of seq param set used for this pic param set
    Ipp8s       entropy_coding_mode;            // zero: CAVLC, else CABAC

    Ipp8s       pic_order_present_flag;         // Zero indicates only delta_pic_order_cnt[0] is
    // present in slice header; nonzero indicates
    // delta_pic_order_cnt[1] is also present.

    Ipp8s       weighted_pred_flag;             // Nonzero indicates weighted prediction applied to
    // P and SP slices
    Ipp8s       weighted_bipred_idc;            // 0: no weighted prediction in B slices
    // 1: explicit weighted prediction
    // 2: implicit weighted prediction
    Ipp8s       pic_init_qp;                    // default QP for I,P,B slices
    Ipp8s       pic_init_qs;                    // default QP for SP, SI slices

    Ipp8s       chroma_qp_index_offset;         // offset to add to QP for chroma

    Ipp8s       deblocking_filter_variables_present_flag; // If nonzero, deblock filter params are
    // present in the slice header.
    Ipp8s       constrained_intra_pred_flag;    // Nonzero indicates constrained intra mode

    Ipp8s       redundant_pic_cnt_present_flag; // Nonzero indicates presence of redundant_pic_cnt
    // in slice header
    Ipp8s       num_slice_groups;               // Usually 1

    Ipp8s       second_chroma_qp_index_offset;

    SliceGroupInfoStruct SliceGroupInfo;        // Used only when num_slice_groups > 1
    Ipp32s      num_ref_idx_l0_active;          // num of ref pics in list 0 used to decode the picture
    Ipp32s      num_ref_idx_l1_active;          // num of ref pics in list 1 used to decode the picture
    bool        transform_8x8_mode_flag;
    bool        pic_scaling_matrix_present_flag; // Only "false" is supported.
    ////bool        pack_sequence_extension; // unused???
    Ipp32s      chroma_format_idc;              // needed for aux/primary picture switch.
    Ipp32s      bit_depth_luma;                 // needed for aux/primary picture switch.
};  // H264PicParamSet

struct sNALUnitHeaderSVCExtension {
    Ipp8u idr_flag;
    Ipp8u priority_id;
    Ipp8u no_inter_layer_pred_flag;
    Ipp8u dependency_id;
    Ipp8u quality_id;
    //Ipp8u temporal_id; // use only from frame
    Ipp8u use_ref_base_pic_flag;
    Ipp8u discardable_flag;
    Ipp8u output_flag;
    Ipp8u reserved_three_2bits;

    Ipp8u store_ref_base_pic_flag;
    Ipp8u adaptive_ref_base_pic_marking_mode_flag;
};

struct H264SVCScalabilityInfoSEI {
    Ipp8u temporal_id_nesting_flag;
    Ipp8u priority_layer_info_present_flag;
    Ipp8u priority_id_setting_flag;

    Ipp32u num_layers_minus1;
    Ipp32u layer_id[MAX_SVC_LAYERS];

    Ipp32u priority_id[MAX_SVC_LAYERS];
    Ipp8u discardable_flag[MAX_SVC_LAYERS];
    Ipp8u dependency_id[MAX_SVC_LAYERS];
    Ipp8u quality_id[MAX_SVC_LAYERS];
    Ipp8u temporal_id[MAX_SVC_LAYERS];

    Ipp8u sub_pic_layer_flag[MAX_SVC_LAYERS];
    Ipp8u sub_region_layer_flag[MAX_SVC_LAYERS];
    Ipp8u iroi_division_info_present_flag[MAX_SVC_LAYERS];

    Ipp8u profile_level_info_present_flag[MAX_SVC_LAYERS];
    Ipp8u bitrate_info_present_flag[MAX_SVC_LAYERS];
    Ipp8u frm_rate_info_present_flag[MAX_SVC_LAYERS];
    Ipp8u frm_size_info_present_flag[MAX_SVC_LAYERS];
    Ipp8u layer_dependency_info_present_flag[MAX_SVC_LAYERS];
    Ipp8u parameter_sets_info_present_flag[MAX_SVC_LAYERS];
    Ipp8u bitstream_restriction_info_present_flag[MAX_SVC_LAYERS];
    Ipp8u exact_interlayer_pred_flag[MAX_SVC_LAYERS];

    Ipp8u exact_sample_value_match_flag[MAX_SVC_LAYERS];
    Ipp8u layer_conversion_flag[MAX_SVC_LAYERS];

    Ipp8u layer_output_flag[MAX_SVC_LAYERS];
    Ipp32u layer_profile_level_idc[MAX_SVC_LAYERS];

    Ipp16u avg_bitrate[MAX_SVC_LAYERS];
    Ipp16u max_bitrate_layer[MAX_SVC_LAYERS];
    Ipp16u max_bitrate_layer_representation[MAX_SVC_LAYERS];
    Ipp16u max_bitrate_calc_window[MAX_SVC_LAYERS];

    Ipp8u constant_frm_rate_idc[MAX_SVC_LAYERS];
    Ipp16u avg_frm_rate[MAX_SVC_LAYERS];

    Ipp32u frm_width_in_mbs_minus1[MAX_SVC_LAYERS];
    Ipp32u frm_height_in_mbs_minus1[MAX_SVC_LAYERS];

    Ipp32u base_region_layer_id[MAX_SVC_LAYERS];
    Ipp8u dynamic_rect_flag[MAX_SVC_LAYERS];
    Ipp16u horizontal_offset[MAX_SVC_LAYERS];
    Ipp16u vertical_offset[MAX_SVC_LAYERS];
    Ipp16u region_width[MAX_SVC_LAYERS];
    Ipp16u region_height[MAX_SVC_LAYERS];

    Ipp32u roi_id[MAX_SVC_LAYERS];
    Ipp8u iroi_grid_flag[MAX_SVC_LAYERS];
    Ipp32u grid_width_in_mbs_minus1[MAX_SVC_LAYERS];
    Ipp32u grid_height_in_mbs_minus1[MAX_SVC_LAYERS];
    Ipp32u num_rois_minus1[MAX_SVC_LAYERS];

    Ipp32u* first_mb_in_roi[MAX_SVC_LAYERS];
    Ipp32u* roi_width_in_mbs_minus1[MAX_SVC_LAYERS];
    Ipp32u* roi_height_in_mbs_minus1[MAX_SVC_LAYERS];
    Ipp32u num_directly_dependent_layers[MAX_SVC_LAYERS];
    Ipp32u directly_dependent_layer_id_delta_minus1[MAX_SVC_LAYERS][MAX_SVC_LAYERS];

    Ipp32u layer_dependency_info_src_layer_id_delta[MAX_SVC_LAYERS];
    Ipp32u num_seq_parameter_set_minus1[MAX_SVC_LAYERS];
    Ipp32u seq_parameter_set_id_delta[MAX_SVC_LAYERS][32];
    Ipp32u num_subset_seq_parameter_set_minus1[MAX_SVC_LAYERS];
    Ipp32u subset_seq_parameter_set_id_delta[MAX_SVC_LAYERS][32];
    Ipp32u num_pic_parameter_set_minus1[MAX_SVC_LAYERS];
    Ipp32u pic_parameter_set_id_delta[MAX_SVC_LAYERS][256];
    Ipp32u parameter_sets_info_src_layer_id_delta[MAX_SVC_LAYERS];

    Ipp8u motion_vectors_over_pic_boundaries_flag[MAX_SVC_LAYERS];
    Ipp32u max_bytes_per_pic_denom[MAX_SVC_LAYERS];
    Ipp32u max_bits_per_mb_denom[MAX_SVC_LAYERS];
    Ipp32u log2_max_mv_length_horizontal[MAX_SVC_LAYERS];
    Ipp32u log2_max_mv_length_vertical[MAX_SVC_LAYERS];
    Ipp32u num_reorder_frames[MAX_SVC_LAYERS];
    Ipp32u max_dec_frame_buffering[MAX_SVC_LAYERS];

    Ipp32u conversion_type_idc[MAX_SVC_LAYERS];
    Ipp8u rewriting_info_flag[MAX_SVC_LAYERS][2];
    Ipp32u rewriting_profile_level_idc[MAX_SVC_LAYERS][2];
    Ipp16u rewriting_avg_bitrate[MAX_SVC_LAYERS][2];
    Ipp16u rewriting_max_bitrate[MAX_SVC_LAYERS][2];

    Ipp32u pr_num_dId_minus1;
    Ipp8u pr_dependency_id[MAX_DEP_LAYERS];
    Ipp32u pr_num_minus1[MAX_DEP_LAYERS];
    Ipp32u pr_id[MAX_DEP_LAYERS][MAX_QUALITY_LEVELS];
    Ipp32u pr_profile_level_idc[MAX_DEP_LAYERS][MAX_QUALITY_LEVELS];
    Ipp16u pr_avg_bitrate[MAX_DEP_LAYERS][MAX_QUALITY_LEVELS];
    Ipp16u pr_max_bitrate[MAX_DEP_LAYERS][MAX_QUALITY_LEVELS];

    Ipp8s priority_id_setting_uri[20];
};


// SVC Layer resize parameter structure
struct H264LayerResizeParameter {
    Ipp32s extended_spatial_scalability; // (idc) the structure is unused if is 0
    Ipp32s leftOffset;
    Ipp32s topOffset;
    Ipp32s rightOffset;
    Ipp32s bottomOffset;
    Ipp32s ref_layer_width;
    Ipp32s ref_layer_height;
    Ipp32s scaled_ref_layer_width;
    Ipp32s scaled_ref_layer_height;
    Ipp32s shiftX;
    Ipp32s shiftY;
    Ipp32s scaleX;
    Ipp32s scaleY;
    Ipp32s phaseX;
    Ipp32s phaseY;
    Ipp32s refPhaseX;
    Ipp32s refPhaseY;
    Ipp32s addX;
    Ipp32s addY;
    Ipp32s m_spatial_resolution_change;
    Ipp32s m_restricted_spatial_resolution_change;
    IppiUpsampleSVCParams upsampleSVCParamsLuma;
    IppiUpsampleSVCParams upsampleSVCParamsChroma;
    IppiRect mbRegionFull;
};

struct SVCResizeMBPos {
    Ipp32s refMB[2]; // number of prev and next ref MB, -1 for out of frame
    Ipp32s border;   // position of the first pix in next MB, 16 or <0 - no cross, 0 - out of frame
};
// SVC Layer parameter set structure.
struct SVCLayerParams
{
    //H264SeqParamSet seqParams;
    //H264PicParamSet picParams;
    // SVC extension

    Ipp32s isActive;
    struct sNALUnitHeaderSVCExtension svc_ext;
    struct H264LayerResizeParameter resize;
};  // SVCLayerParams

class H264EncoderParams: public VideoEncoderParams
{
    DYNAMIC_CAST_DECL(H264EncoderParams, VideoEncoderParams)

public:
    H264EncoderParams();
    virtual Status ReadParamFile(const vm_char *ParFileName);

    Ipp8u           aspect_ratio_idc;
    Ipp32s          frame_crop_x;
    Ipp32s          frame_crop_w;
    Ipp32s          frame_crop_y;
    Ipp32s          frame_crop_h;
    bool            use_reset_refpiclist_for_intra;
    Ipp32s          chroma_format_idc;
    Ipp32s          coding_type; // 0 - only FRM, 1 - only FLD , 2 - only AFRM, 3  - pure PicAFF(no MBAFF) 4 PicAFF + MBAFF (see EnumCodingType)
    Ipp32s          B_frame_rate;
    Ipp32s          treat_B_as_reference;
    Ipp32s          num_ref_frames;
    Ipp32s          num_ref_to_start_code_B_slice;
    Ipp8s           level_idc;
    H264_Rate_Controls  rate_controls;
    Ipp16s          num_slices; // Number of slices
    Ipp8s           m_do_weak_forced_key_frames;
    Ipp8s           deblocking_filter_idc;
    Ipp32s          deblocking_filter_alpha;
    Ipp32s          deblocking_filter_beta;
    Ipp32s          mv_search_method;
    Ipp32s          mv_subpel_search_method;
    Ipp32s          me_split_mode; // 0 - 16x16 only; 1 - 16x16, 16x8, 8x16, 8x8; 2 - could split 8x8.
    Ipp32s          me_search_x;
    Ipp32s          me_search_y;
    Ipp32s          max_mv_length_x;
    Ipp32s          max_mv_length_y;
    Ipp32s          use_weighted_pred;
    Ipp32s          use_weighted_bipred;
    Ipp32s          use_implicit_weighted_bipred;
    Ipp8s           direct_pred_mode; // 1 - spatial, 0 - temporal
    Ipp32s          use_direct_inference;
    Ipp8s           entropy_coding_mode; // 0 - CAVLC, 1 - CABAC
    Ipp8s           cabac_init_idc; // [0..2] used for CABAC
    Ipp8s           write_access_unit_delimiters; // 1 - write, 0 - do not
    H264_Key_Frame_Controls    key_frame_controls;
    bool            use_transform_for_intra_decision;
    Ipp32s          m_Analyse_on;
    Ipp32s          m_Analyse_restrict;
    Ipp32s          m_Analyse_ex;

    H264SeqParamSet m_SeqParamSet;
    H264PicParamSet m_PicParamSet;
    bool            use_ext_sps;
    bool            use_ext_pps;
    Ipp32u          m_ext_SPS_id;
    Ipp32u          m_ext_PPS_id;
    Ipp8u           m_ext_constraint_flags[6];
    bool            reset_encoding_pipeline;
    bool            is_resol_changed;

public:
    H264_PROFILE_IDC profile_idc; // profile_idc
    bool transform_8x8_mode_flag;
    Ipp32s  qpprime_y_zero_transform_bypass_flag;
    Ipp32s  use_default_scaling_matrix;

    Ipp32s  aux_format_idc;
    bool alpha_incr_flag;
    Ipp32s  alpha_opaque_value;
    Ipp32s  alpha_transparent_value;
    Ipp32s  bit_depth_aux;
    Ipp32s  bit_depth_luma;
    Ipp32s  bit_depth_chroma;

    Ipp32s numFramesToEncode;
    Ipp32s m_QualitySpeed;
    Ipp32u quant_opt_level;
    //Additional parameters
#if defined H264_LOG
    bool m_log;
    vm_char m_log_file[254];
#endif
    bool            m_is_mvc_profile;
    bool            m_is_base_view;
    bool            m_viewOutput;       // Whether to output view bitstreams together or separately
};

} // namespace UMC

namespace UMC_H264_ENCODER {
    struct sH264CoreEncoder_8u16s;
    struct sH264CoreEncoder_16u32s;
    typedef struct sH264CoreEncoder_8u16s H264CoreEncoder_8u16s;
    typedef struct sH264CoreEncoder_16u32s H264CoreEncoder_16u32s;
}

using namespace UMC_H264_ENCODER;

namespace UMC {
    typedef enum {
        H264_VIDEO_ENCODER_8U_16S  = 0,
        H264_VIDEO_ENCODER_16U_32S = 1,
        H264_VIDEO_ENCODER_NONE    = 2
    } EncoderType;

    class H264VideoEncoder : public VideoEncoder
    {
    public:
        H264VideoEncoder();
        ~H264VideoEncoder();

    public:
        // Initialize codec with specified parameter(s)
        virtual Status Init(BaseCodecParams *init);
        // Compress (decompress) next frame
        virtual Status GetFrame(MediaData *in, MediaData *out);
        // Get codec working (initialization) parameter(s)
        virtual Status GetInfo(BaseCodecParams *info);

        const H264PicParamSet* GetPicParamSet();
        const H264SeqParamSet* GetSeqParamSet();

        // Close all codec resources
        virtual Status Close();

        virtual Status Reset();
        virtual Status SetParams(BaseCodecParams* params);

        VideoData* GetReconstructedFrame();

    protected:
        EncoderType                     m_CurrEncoderType;  // Type of the encoder applicable now.
        void* m_pEncoder_8u_16s;  // For 8 bit video.
        VideoBrc *m_pBrc;

#if defined BITDEPTH_9_12
        void* m_pEncoder_16u_32s; // For 9-12 bit video.
#endif // BITDEPTH_9_12
    };

} // namespace UMC

#define ConvertProfileToTable(profile) (profile == H264_BASE_PROFILE) ? H264_LIMIT_TABLE_BASE_PROFILE : \
    (profile == H264_MAIN_PROFILE) ? H264_LIMIT_TABLE_MAIN_PROFILE : \
    (profile == H264_EXTENDED_PROFILE) ? H264_LIMIT_TABLE_EXTENDED_PROFILE : \
    (profile == H264_HIGH_PROFILE) ? H264_LIMIT_TABLE_HIGH_PROFILE : \
    (profile == H264_MULTIVIEWHIGH_PROFILE) ? H264_LIMIT_TABLE_MULTIVIEWHIGH_PROFILE : \
    (profile == H264_STEREOHIGH_PROFILE) ? H264_LIMIT_TABLE_STEREOHIGH_PROFILE : \
    (profile == H264_SCALABLE_BASELINE_PROFILE) ? H264_LIMIT_TABLE_BASE_PROFILE : \
    (profile == H264_SCALABLE_HIGH_PROFILE) ? H264_LIMIT_TABLE_HIGH_PROFILE : \
    (profile == H264_HIGH10_PROFILE) ? H264_LIMIT_TABLE_HIGH10_PROFILE : \
    (profile == H264_HIGH422_PROFILE) ? H264_LIMIT_TABLE_HIGH422_PROFILE : \
    (profile == H264_HIGH444_PROFILE) ?  H264_LIMIT_TABLE_HIGH444_PROFILE : -1;

#define ConvertProfileFromTable(profile) ((profile == H264_LIMIT_TABLE_BASE_PROFILE) ? H264_BASE_PROFILE : \
    (profile == H264_LIMIT_TABLE_MAIN_PROFILE) ? H264_MAIN_PROFILE : \
    (profile == H264_LIMIT_TABLE_EXTENDED_PROFILE) ? H264_EXTENDED_PROFILE : \
    (profile == H264_LIMIT_TABLE_HIGH_PROFILE) ? H264_HIGH_PROFILE : \
    (profile == H264_LIMIT_TABLE_STEREOHIGH_PROFILE) ? H264_STEREOHIGH_PROFILE : \
    (profile == H264_LIMIT_TABLE_MULTIVIEWHIGH_PROFILE) ? H264_MULTIVIEWHIGH_PROFILE : \
    (profile == H264_LIMIT_TABLE_HIGH10_PROFILE) ? H264_HIGH10_PROFILE : \
    (profile == H264_LIMIT_TABLE_HIGH422_PROFILE) ? H264_HIGH422_PROFILE : H264_HIGH444_PROFILE);

#define ConvertLevelToTable(level) (level == 10) ? H264_LIMIT_TABLE_LEVEL_1 : \
    (level == 11) ? H264_LIMIT_TABLE_LEVEL_11 : \
    (level == 12) ? H264_LIMIT_TABLE_LEVEL_12 : \
    (level == 13) ? H264_LIMIT_TABLE_LEVEL_13 : \
    (level == 20) ? H264_LIMIT_TABLE_LEVEL_2 : \
    (level == 21) ? H264_LIMIT_TABLE_LEVEL_21 : \
    (level == 22) ? H264_LIMIT_TABLE_LEVEL_22 : \
    (level == 30) ? H264_LIMIT_TABLE_LEVEL_3 : \
    (level == 31) ? H264_LIMIT_TABLE_LEVEL_31 : \
    (level == 32) ? H264_LIMIT_TABLE_LEVEL_32 : \
    (level == 40) ? H264_LIMIT_TABLE_LEVEL_4 : \
    (level == 41) ? H264_LIMIT_TABLE_LEVEL_41 : \
    (level == 42) ? H264_LIMIT_TABLE_LEVEL_42 : \
    (level == 50) ? H264_LIMIT_TABLE_LEVEL_5 : \
    (level == 51) ? H264_LIMIT_TABLE_LEVEL_51 : \
    (level == 52) ? H264_LIMIT_TABLE_LEVEL_52 : -1;

#define ConvertLevelFromTable(level) ((level == H264_LIMIT_TABLE_LEVEL_1) ? 10 : \
    ((level == H264_LIMIT_TABLE_LEVEL_11) || (level == H264_LIMIT_TABLE_LEVEL_1B)) ? 11 : \
    (level == H264_LIMIT_TABLE_LEVEL_12) ? 12 : \
    (level == H264_LIMIT_TABLE_LEVEL_13) ? 13 : \
    (level == H264_LIMIT_TABLE_LEVEL_2) ? 20 : \
    (level == H264_LIMIT_TABLE_LEVEL_21) ? 21 : \
    (level == H264_LIMIT_TABLE_LEVEL_22) ? 22 : \
    (level == H264_LIMIT_TABLE_LEVEL_3) ? 30 : \
    (level == H264_LIMIT_TABLE_LEVEL_31) ? 31 : \
    (level == H264_LIMIT_TABLE_LEVEL_32) ? 32 : \
    (level == H264_LIMIT_TABLE_LEVEL_4) ? 40 : \
    (level == H264_LIMIT_TABLE_LEVEL_41) ? 41 : \
    (level == H264_LIMIT_TABLE_LEVEL_42) ? 42 : \
    (level == H264_LIMIT_TABLE_LEVEL_5) ? 50 :  \
    (level == H264_LIMIT_TABLE_LEVEL_51) ? 51 : 52);


#endif // __UMC_H264_VIDEO_ENCODER_H__

#endif //UMC_ENABLE_H264_VIDEO_ENCODER
