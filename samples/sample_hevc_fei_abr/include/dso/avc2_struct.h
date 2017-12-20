#pragma once

#include "bs_def.h"


namespace BS_AVC2
{
typedef class Parser* HDL;

enum INIT_MODE
{
    INIT_MODE_DEFAULT          = 0x00,
    INIT_MODE_PARSE_SD         = 0x01,
    INIT_MODE_SAVE_UNKNOWN_SEI = 0x10,
    INIT_MODE_MT_SD            = 0x20 | INIT_MODE_PARSE_SD,
    INIT_MODE_ASYNC_SD         = 0x40 | INIT_MODE_MT_SD
};

enum TRACE_LEVEL
{
    TRACE_MARKERS         = 0x00000001,
    TRACE_NALU            = 0x00000002,
    TRACE_SPS             = 0x00000004,
    TRACE_VUI             = 0x00000008,
    TRACE_HRD             = 0x00000010,
    TRACE_PPS             = 0x00000020,
    TRACE_SEI             = 0x00000040,
    TRACE_SLICE           = 0x00000080,
    TRACE_POC             = 0x00000100,
    TRACE_REF_LISTS       = 0x00000200,
    TRACE_MB_ADDR         = 0x00001000,
    TRACE_MB_LOC          = 0x00002000,
    TRACE_MB_TYPE         = 0x00004000,
    TRACE_SUB_MB_TYPE     = 0x00008000,
    TRACE_PRED_MODE       = 0x00010000,
    TRACE_REF_IDX         = 0x00020000,
    TRACE_MVD             = 0x00040000,
    TRACE_MB_FIELD        = 0x00080000,
    TRACE_MB_TRANS8x8     = 0x00100000,
    TRACE_CBP             = 0x00200000,
    TRACE_MB_QP_DELTA     = 0x00400000,
    TRACE_MB_QP           = 0x00800000,
    TRACE_RESIDUAL_ARRAYS = 0x01000000,
    TRACE_RESIDUAL_FLAGS  = 0x02000000,
    TRACE_MB_EOS          = 0x04000000,
    TRACE_SIZE            = 0x40000000,
    TRACE_OFFSET          = 0x80000000,
    TRACE_DEFAULT = 
          TRACE_MARKERS
        | TRACE_NALU
        | TRACE_SPS
        | TRACE_VUI
        | TRACE_HRD
        | TRACE_PPS
        | TRACE_SEI
        | TRACE_SLICE
        | TRACE_POC
        | TRACE_REF_LISTS
        | TRACE_MB_ADDR
        //| TRACE_MB_LOC
        | TRACE_MB_TYPE
        | TRACE_SUB_MB_TYPE
        | TRACE_PRED_MODE
        | TRACE_REF_IDX
        | TRACE_MVD
        | TRACE_MB_FIELD
        | TRACE_MB_TRANS8x8
        | TRACE_CBP
        | TRACE_MB_QP_DELTA
        | TRACE_MB_QP
        //| TRACE_RESIDUAL_ARRAYS
        //| TRACE_RESIDUAL_FLAGS
        //| TRACE_MB_EOS
        //| TRACE_SIZE
        //| TRACE_OFFSET
};

enum NALU_TYPE
{
    SLICE_NONIDR    = 0x01,
    SLICE_IDR       = 0x05,
    SLICE_AUX       = 0x13,
    SD_PART_A       = 0x02,
    SD_PART_B       = 0x03,
    SD_PART_C       = 0x04,
    SEI_NUT         = 0x06,
    SPS_NUT         = 0x07,
    PPS_NUT         = 0x08,
    AUD_NUT         = 0x09,
    EOSEQ_NUT       = 0x0A,
    EOSTR_NUT       = 0x0B,
    FD_NUT          = 0x0C,
    SPS_EXT_NUT     = 0x0D,
    PREFIX_NUT      = 0x0E,
    SUBSPS_NUT      = 0x0F,
    SLICE_EXT       = 0x14,
};

enum PROFILE
{
    CAVLC_444       =  44,
    BASELINE        =  66,
    MAIN            =  77,
    EXTENDED        =  88,
    SVC_BASELINE    =  83,
    SVC_HIGH        =  86,
    HIGH            = 100,
    HIGH_10         = 110,
    HIGH_422        = 122,
    HIGH_444        = 244,
    MULTIVIEW_HIGH  = 118,
    STEREO_HIGH     = 128,
};

enum AR_IDC
{
    AR_IDC_1_1    =   1,
    AR_IDC_square =   1,
    AR_IDC_12_11  =   2,
    AR_IDC_10_11  =   3,
    AR_IDC_16_11  =   4,
    AR_IDC_40_33  =   5,
    AR_IDC_24_11  =   6,
    AR_IDC_20_11  =   7,
    AR_IDC_32_11  =   8,
    AR_IDC_80_33  =   9,
    AR_IDC_18_11  =  10,
    AR_IDC_15_11  =  11,
    AR_IDC_64_33  =  12,
    AR_IDC_160_99 =  13,
    AR_IDC_4_3    =  14,
    AR_IDC_3_2    =  15,
    AR_IDC_2_1    =  16,
    Extended_SAR  = 255,
};

enum SEI_TYPE
{
      BUFFERING_PERIOD = 0
    , PIC_TIMING
    , PAN_SCAN_RECT
    , FILLER_PAYLOAD
    , USER_DATA_REGISTERED_ITU_T_T35
    , USER_DATA_UNREGISTERED
    , RECOVERY_POINT
    , DEC_REF_PIC_MARKING_REPETITION
    , SPARE_PIC
    , SCENE_INFO
    , SUB_SEQ_INFO
    , SUB_SEQ_LAYER_CHARACTERISTICS
    , SUB_SEQ_CHARACTERISTICS
    , FULL_FRAME_FREEZE
    , FULL_FRAME_FREEZE_RELEASE
    , FULL_FRAME_SNAPSHOT
    , PROGRESSIVE_REFINEMENT_SEGMENT_START
    , PROGRESSIVE_REFINEMENT_SEGMENT_END
    , MOTION_CONSTRAINED_SLICE_GROUP_SET
    , FILM_GRAIN_CHARACTERISTICS
    , DEBLOCKING_FILTER_DISPLAY_PREFERENCE
    , STEREO_VIDEO_INFO
    , POST_FILTER_HINT
    , TONE_MAPPING_INFO
    , SCALABILITY_INFO
    , SUB_PIC_SCALABLE_LAYER
    , NON_REQUIRED_LAYER_REP
    , PRIORITY_LAYER_INFO
    , LAYERS_NOT_PRESENT
    , LAYER_DEPENDENCY_CHANGE
    , SCALABLE_NESTING
    , BASE_LAYER_TEMPORAL_HRD
    , QUALITY_LAYER_INTEGRITY_CHECK
    , REDUNDANT_PIC_PROPERTY
    , TL0_DEP_REP_INDEX
    , TL_SWITCHING_POINT
    , PARALLEL_DECODING_INFO
    , MVC_SCALABLE_NESTING
    , VIEW_SCALABILITY_INFO
    , MULTIVIEW_SCENE_INFO
    , MULTIVIEW_ACQUISITION_INFO
    , NON_REQUIRED_VIEW_COMPONENT
    , VIEW_DEPENDENCY_CHANGE
    , OPERATION_POINTS_NOT_PRESENT
    , BASE_VIEW_TEMPORAL_HRD
    , FRAME_PACKING_ARRANGEMENT
    , MULTIVIEW_VIEW_POSITION
    , DISPLAY_ORIENTATION
    , MVCD_SCALABLE_NESTING
    , MVCD_VIEW_SCALABILITY_INFO
    , DEPTH_REPRESENTATION_INFO
    , THREE_DIMENSIONAL_REFERENCE_DISPLAYS_INFO
    , DEPTH_TIMING
    , DEPTH_SAMPLING_INFO
    , CONSTRAINED_DEPTH_PARAMETER_SET_IDENTIFIER
};

enum MMCO
{
      MMCO_END = 0
    , MMCO_REMOVE_ST
    , MMCO_REMOVE_LT
    , MMCO_ST_TO_LT
    , MMCO_MAX_LT_IDX
    , MMCO_CLEAR_DPB
    , MMCO_CUR_TO_LT
};

enum RPLM
{
      RPLM_ST_SUB = 0
    , RPLM_ST_ADD
    , RPLM_LT
    , RPLM_END
    , RPLM_VIEW_IDX_SUB
    , RPLM_VIEW_IDX_ADD
};

enum SLICE_TYPE
{
      SLICE_P = 0
    , SLICE_B
    , SLICE_I
    , SLICE_SP
    , SLICE_SI
};

enum MB_TYPE
{
      MBTYPE_I_start = 0
    , I_NxN = MBTYPE_I_start
    , I_16x16_0_0_0
    , I_16x16_1_0_0
    , I_16x16_2_0_0
    , I_16x16_3_0_0
    , I_16x16_0_1_0
    , I_16x16_1_1_0
    , I_16x16_2_1_0
    , I_16x16_3_1_0
    , I_16x16_0_2_0
    , I_16x16_1_2_0
    , I_16x16_2_2_0
    , I_16x16_3_2_0
    , I_16x16_0_0_1
    , I_16x16_1_0_1
    , I_16x16_2_0_1
    , I_16x16_3_0_1
    , I_16x16_0_1_1
    , I_16x16_1_1_1
    , I_16x16_2_1_1
    , I_16x16_3_1_1
    , I_16x16_0_2_1
    , I_16x16_1_2_1
    , I_16x16_2_2_1
    , I_16x16_3_2_1
    , I_PCM
    , MBTYPE_SI
    , MBTYPE_P_start
    , P_L0_16x16 = MBTYPE_P_start
    , P_L0_L0_16x8
    , P_L0_L0_8x16
    , P_8x8
    , P_8x8ref0
    , MBTYPE_B_start
    , B_Direct_16x16 = MBTYPE_B_start
    , B_L0_16x16
    , B_L1_16x16
    , B_Bi_16x16
    , B_L0_L0_16x8
    , B_L0_L0_8x16
    , B_L1_L1_16x8
    , B_L1_L1_8x16
    , B_L0_L1_16x8
    , B_L0_L1_8x16
    , B_L1_L0_16x8
    , B_L1_L0_8x16
    , B_L0_Bi_16x8
    , B_L0_Bi_8x16
    , B_L1_Bi_16x8
    , B_L1_Bi_8x16
    , B_Bi_L0_16x8
    , B_Bi_L0_8x16
    , B_Bi_L1_16x8
    , B_Bi_L1_8x16
    , B_Bi_Bi_16x8
    , B_Bi_Bi_8x16
    , B_8x8
    , P_Skip
    , B_Skip
    , MBTYPE_SubP_start
    , P_L0_8x8 = MBTYPE_SubP_start
    , P_L0_8x4
    , P_L0_4x8
    , P_L0_4x4
    , MBTYPE_SubB_start
    , B_Direct_8x8 = MBTYPE_SubB_start
    , B_L0_8x8
    , B_L1_8x8
    , B_Bi_8x8
    , B_L0_8x4
    , B_L0_4x8
    , B_L1_8x4
    , B_L1_4x8
    , B_Bi_8x4
    , B_Bi_4x8
    , B_L0_4x4
    , B_L1_4x4
    , B_Bi_4x4
};

enum MB_PART_PRED_MODE
{
      Intra_4x4 = 0
    , Intra_8x8
    , Intra_16x16
    , Pred_L0
    , Pred_L1
    , BiPred
    , Direct
    , MODE_NA = 127
};

enum INTRA_PRED_MODE
{
      Intra_4x4_Vertical = 0
    , Intra_4x4_Horizontal
    , Intra_4x4_DC
    , Intra_4x4_Diagonal_Down_Left
    , Intra_4x4_Diagonal_Down_Right
    , Intra_4x4_Vertical_Right
    , Intra_4x4_Horizontal_Down
    , Intra_4x4_Vertical_Left
    , Intra_4x4_Horizontal_Up
    , Intra_8x8_Vertical = 0
    , Intra_8x8_Horizontal
    , Intra_8x8_DC
    , Intra_8x8_Diagonal_Down_Left
    , Intra_8x8_Diagonal_Down_Right
    , Intra_8x8_Vertical_Right
    , Intra_8x8_Horizontal_Down
    , Intra_8x8_Vertical_Left
    , Intra_8x8_Horizontal_Up
    , Intra_16x16_Vertical = 0
    , Intra_16x16_Horizontal
    , Intra_16x16_DC
    , Intra_16x16_Plane
    , Intra_Chroma_DC = 0
    , Intra_Chroma_Horizontal
    , Intra_Chroma_Vertical
    , Intra_Chroma_Plane
};

struct ScalingMatrix
{
    Bs8u scaling_list_present_flag[12];
    Bs8u ScalingList4x4[6][16];
    Bs8u UseDefaultScalingMatrix4x4Flag[6];
    Bs8u ScalingList8x8[6][64];
    Bs8u UseDefaultScalingMatrix8x8Flag[6];
};

struct HRD
{
    Bs32u bit_rate_scale                          : 4;
    Bs32u cpb_size_scale                          : 4;
    Bs32u initial_cpb_removal_delay_length_minus1 : 5;
    Bs32u cpb_removal_delay_length_minus1         : 5;
    Bs32u dpb_output_delay_length_minus1          : 5;
    Bs32u time_offset_length                      : 5;
    Bs32u cpb_cnt_minus1                          : 5;
    Bs32u bit_rate_value_minus1[32];
    Bs32u cpb_size_value_minus1[32];
    Bs8u  cbr_flag[32];
};

struct VUI
{
    Bs8u  aspect_ratio_info_present_flag   : 1;
    Bs8u  overscan_info_present_flag       : 1;
    Bs8u  overscan_appropriate_flag        : 1;
    Bs8u  video_signal_type_present_flag   : 1;
    Bs8u  video_full_range_flag            : 1;
    Bs8u  colour_description_present_flag  : 1;
    Bs8u  chroma_loc_info_present_flag     : 1;
    Bs8u  timing_info_present_flag         : 1;

    Bs8u  fixed_frame_rate_flag            : 1;
    Bs8u  nal_hrd_parameters_present_flag  : 1;
    Bs8u  vcl_hrd_parameters_present_flag  : 1;
    Bs8u  low_delay_hrd_flag               : 1;
    Bs8u  pic_struct_present_flag          : 1;
    Bs8u  video_format                     : 3;

    Bs8u  chroma_sample_loc_type_top_field          : 3;
    Bs8u  chroma_sample_loc_type_bottom_field       : 3;
    Bs8u  bitstream_restriction_flag                : 1;
    Bs8u  motion_vectors_over_pic_boundaries_flag   : 1;

    Bs8u  aspect_ratio_idc;

    Bs8u  colour_primaries;
    Bs8u  transfer_characteristics;
    Bs8u  matrix_coefficients;

    Bs16u max_bytes_per_pic_denom       : 5;
    Bs16u max_bits_per_mb_denom         : 5;
    Bs16u log2_max_mv_length_horizontal : 5;

    Bs16u log2_max_mv_length_vertical   : 5;
    Bs16u num_reorder_frames            : 5;
    Bs16u max_dec_frame_buffering       : 5;
    
    Bs16u sar_width;
    Bs16u sar_height;

    Bs32u num_units_in_tick;
    Bs32u time_scale;

    HRD* nal_hrd;
    HRD* vcl_hrd;
};

struct SPS
{
    Bs8u profile_idc;
    Bs8u level_idc;

    union
    {
        struct
        {
            Bs8u constraint_set0_flag : 1;
            Bs8u constraint_set1_flag : 1;
            Bs8u constraint_set2_flag : 1;
            Bs8u constraint_set3_flag : 1;
            Bs8u constraint_set4_flag : 1;
            Bs8u constraint_set5_flag : 1;
        };
        Bs8u constraint_set;
    };

    Bs8u seq_parameter_set_id                 : 5;
    Bs8u chroma_format_idc                    : 2;
    Bs8u separate_colour_plane_flag           : 1;

    Bs8u bit_depth_luma_minus8                : 3;
    Bs8u bit_depth_chroma_minus8              : 3;
    Bs8u qpprime_y_zero_transform_bypass_flag : 1;
    Bs8u seq_scaling_matrix_present_flag      : 1;

    Bs8u log2_max_frame_num_minus4            : 4;
    Bs8u log2_max_pic_order_cnt_lsb_minus4    : 4;

    Bs8u pic_order_cnt_type                   : 2;
    Bs8u delta_pic_order_always_zero_flag     : 1;
    Bs8u frame_mbs_only_flag                  : 1;
    Bs8u mb_adaptive_frame_field_flag         : 1;
    Bs8u gaps_in_frame_num_value_allowed_flag : 1;
    Bs8u direct_8x8_inference_flag            : 1;
    Bs8u frame_cropping_flag                  : 1;

    Bs8u max_num_ref_frames                   : 5;
    Bs8u vui_parameters_present_flag          : 1;

    Bs32s offset_for_non_ref_pic;
    Bs32s offset_for_top_to_bottom_field;

    Bs32u pic_width_in_mbs_minus1;
    Bs32u pic_height_in_map_units_minus1;

    Bs32u frame_crop_left_offset;
    Bs32u frame_crop_right_offset;
    Bs32u frame_crop_top_offset;
    Bs32u frame_crop_bottom_offset;

    Bs8u num_ref_frames_in_pic_order_cnt_cycle;

    Bs32s* offset_for_ref_frame;
    ScalingMatrix* scaling_matrix;
    VUI* vui;
};

struct PPS
{
    Bs8u pic_parameter_set_id;
    Bs8u seq_parameter_set_id                           : 5;
    Bs8u entropy_coding_mode_flag                       : 1;
    Bs8u bottom_field_pic_order_in_frame_present_flag   : 1;
    Bs8u deblocking_filter_control_present_flag         : 1;

    Bs8u constrained_intra_pred_flag        : 1;
    Bs8u redundant_pic_cnt_present_flag     : 1;
    Bs8u transform_8x8_mode_flag            : 1;
    Bs8u pic_scaling_matrix_present_flag    : 1;
    Bs8u weighted_pred_flag                 : 1;
    Bs8u weighted_bipred_idc                : 2;

    Bs8u num_slice_groups_minus1 : 3;
    Bs8u slice_group_map_type    : 3;

    union
    {
        Bs32u* run_length_minus1;
        struct
        {
            Bs32u* top_left;
            Bs32u* bottom_right;
        };
        struct
        {
            Bs32u  slice_group_change_rate_minus1;
            Bs8u   slice_group_change_direction_flag;
        };
        struct
        {
            Bs32u  pic_size_in_map_units_minus1;
            Bs8u*  slice_group_id;
        };
    };

    Bs16u num_ref_idx_l0_default_active_minus1 : 5;
    Bs16u num_ref_idx_l1_default_active_minus1 : 5;
    Bs16s pic_init_qp_minus26                  : 6;

    Bs16s pic_init_qs_minus26           : 6;
    Bs16s chroma_qp_index_offset        : 5;
    Bs16s second_chroma_qp_index_offset : 5;

    ScalingMatrix* scaling_matrix;
    SPS* sps;
};

struct BufferingPeriod
{
    Bs8u seq_parameter_set_id : 5;

    struct HRDBP
    {
        Bs32u initial_cpb_removal_delay;
        Bs32u initial_cpb_removal_delay_offset;
    } *nal, *vcl;
    SPS* sps;
};

struct PicTiming
{
    Bs32u cpb_removal_delay;
    Bs32u dpb_output_delay;

    Bs8u pic_struct : 4;
    Bs8u NumClockTS : 2;

    struct CTS
    {
        Bs8u  clock_timestamp_flag  : 1;
        Bs8u  ct_type               : 2;
        Bs8u  counting_type         : 5;

        Bs8u  nuit_field_based_flag : 1;
        Bs8u  full_timestamp_flag   : 1;
        Bs8u  seconds_value         : 6;

        Bs8u  discontinuity_flag    : 1;
        Bs8u  cnt_dropped_flag      : 1;
        Bs8u  minutes_value         : 6;

        Bs8u  hours_value           : 5;
        Bs8u  seconds_flag          : 1;
        Bs8u  minutes_flag          : 1;
        Bs8u  hours_flag            : 1;

        Bs8u  n_frames;
        Bs32s time_offset;
    }ClockTS[3];

    SPS* sps;
};

struct DRPM
{
    Bs32u memory_management_control_operation;
    union
    {
        Bs32u difference_of_pic_nums_minus1;
        Bs32u max_long_term_frame_idx_plus1;
        Bs32u long_term_pic_num;
    };
    Bs32u long_term_frame_idx;

    DRPM* next;
};

struct DRPMRep
{
    Bs16u original_frame_num;
    Bs8u  original_idr_flag                  : 1;
    Bs8u  original_field_pic_flag            : 1;
    Bs8u  original_bottom_field_flag         : 1;
    Bs8u  no_output_of_prior_pics_flag       : 1;
    Bs8u  long_term_reference_flag           : 1;
    Bs8u  adaptive_ref_pic_marking_mode_flag : 1;
    DRPM* drpm;
    SPS*  sps;
};

struct SEI
{
    Bs32u payloadType;
    Bs32u payloadSize;

    Bs8u* rawData;

    union //parsing may be delayed/canceled due to unresolved dependencies
    {
        BufferingPeriod* bp;
        PicTiming* pt;
        DRPMRep* drpm_rep;
    };

    SEI* next;
};

struct MBLoc
{
    Bs32s Addr;
    Bs16u x;
    Bs16u y;
};

struct MBPred
{
    union
    {
        struct /*intra*/
        {
            struct 
            {
                Bs8u prev_intra_pred_mode_flag : 1;
                Bs8u rem_intra_pred_mode       : 5;
            } Blk[16];
            Bs8u IntraPredMode[16]; //in raster scan
            Bs8u intra_chroma_pred_mode;
        };
        struct /*inter*/
        {
            Bs8u  ref_idx_lX[2][16];
            Bs16s mvd_lX[2][4][4][2];
        };
    };
};

struct CodedBlockFlags
{
    Bs16u lumaDC    : 1;
    Bs16u CbDC      : 1;
    Bs16u CrDC      : 1;
    Bs16u luma8x8   : 4;
    Bs16u Cb8x8     : 4;
    Bs16u Cr8x8     : 4;
    Bs16u luma4x4;
    Bs16u Cb4x4;
    Bs16u Cr4x4;
};

struct CoeffTokens
{
    Bs16u lumaDC;
    Bs16u CbDC;
    Bs16u CrDC;
    Bs16u luma4x4[16];
    Bs16u Cb4x4[16];
    Bs16u Cr4x4[16];
};

struct MB : MBLoc
{
    Bs8u mb_skip_flag            : 1;
    Bs8u mb_field_decoding_flag  : 1;
    Bs8u end_of_slice_flag       : 1;
    Bs8u transform_size_8x8_flag : 1;

    Bs8u MbType;
    Bs8u coded_block_pattern;
    Bs8s mb_qp_delta;
    Bs8u QPy;
    Bs8u SubMbType[4];

    MBPred pred;
    union
    {
        CodedBlockFlags cbf; //CABAC
        CoeffTokens     ct;  //CAVLC
    };

    Bs16u NumBits;
    Bs16u NumBins; //CABAC
    MB*   next;
};

struct RefPicListMod
{
    Bs16u modification_of_pic_nums_idc;

    union
    {
        Bs16u abs_diff_pic_num_minus1;
        Bs16u long_term_pic_num;
        Bs16u abs_diff_view_idx_minus1;
    };

    RefPicListMod* next;
};

struct PredWeightTable
{
    Bs8u luma_log2_weight_denom     : 3;
    Bs8u chroma_log2_weight_denom   : 3;

    struct Entry
    {
        Bs8u luma_weight_lx_flag   : 1;
        Bs8u chroma_weight_lx_flag : 1;
        Bs8s luma_weight_lx;
        Bs8s luma_offset_lx;
        Bs8s chroma_weight_lx[2];
        Bs8s chroma_offset_lx[2];
    } *ListX[2];
};

struct Slice
{
    Bs32u first_mb_in_slice;
    Bs8u  pic_parameter_set_id;

    Bs8u  slice_type        : 4;
    Bs8u  colour_plane_id   : 2;
    Bs8u  field_pic_flag    : 1;
    Bs8u  bottom_field_flag : 1;

    Bs16u frame_num;

    Bs8u  direct_spatial_mv_pred_flag       : 1;
    Bs8u  num_ref_idx_active_override_flag  : 1;
    Bs8u  ref_pic_list_modification_flag_l0 : 1;
    Bs8u  ref_pic_list_modification_flag_l1 : 1;
    Bs8u  sp_for_switch_flag                : 1;
    Bs8u  no_output_of_prior_pics_flag      : 1;
    Bs8u  long_term_reference_flag          : 1;
    Bs8u  adaptive_ref_pic_marking_mode_flag: 1;

    Bs8u redundant_pic_cnt : 7;

    Bs8s  slice_qp_delta;
    Bs8s  slice_qs_delta;

    Bs16u idr_pic_id;
    Bs16u pic_order_cnt_lsb;

    Bs16u num_ref_idx_l0_active_minus1  : 5;
    Bs16u num_ref_idx_l1_active_minus1  : 5;
    Bs16u cabac_init_idc                : 2;
    Bs16u disable_deblocking_filter_idc : 2;

    Bs8s  slice_alpha_c0_offset_div2 : 4;
    Bs8s  slice_beta_offset_div2     : 4;

    Bs32s delta_pic_order_cnt_bottom;
    Bs32s delta_pic_order_cnt[2];

    Bs32u slice_group_change_cycle;
    Bs32s POC;
    Bs32u NumMB;
    Bs64u BinCount;

    struct RefPic
    {
        Bs32s POC;
        Bs8u  bottom_field_flag : 1;
        Bs8u  long_term         : 1;
        Bs8u  non_existing      : 1;
    } *RefPicListX[2];

    RefPicListMod*   rplm[2];
    PredWeightTable* pwt;
    DRPM*            drpm;
    PPS*             pps;
    MB*              mb;
};

struct AUD
{
    Bs8u primary_pic_type;
};

struct NUH_MVC_ext
{
    Bs32u non_idr_flag      : 1;
    Bs32u priority_id       : 6;
    Bs32u inter_view_flag   : 1;
    Bs32u temporal_id       : 3;
    Bs32u anchor_pic_flag   : 1;
    Bs32u view_id           : 10;
};

struct NUH_SVC_ext
{
    Bs8u idr_flag                 : 1;
    Bs8u priority_id              : 6;
    Bs8u no_inter_layer_pred_flag : 1;
    union
    {
        struct
        {
            Bs8u quality_id     : 4;
            Bs8u dependency_id  : 3;
        };
        Bs8u DQId : 7;
    };
    Bs8u use_ref_base_pic_flag  : 1;
    Bs8u temporal_id            : 3;
    Bs8u discardable_flag       : 1;
    Bs8u output_flag            : 1;
};

struct NALU
{
    Bs64u StartOffset;
    Bs32u NumBytesInNalUnit;
    Bs32u NumBytesInRbsp;

    Bs8u nal_ref_idc        : 2;
    Bs8u nal_unit_type      : 5;
    Bs8u svc_extension_flag : 1;

    union
    {
        NUH_MVC_ext mvc_ext;
        NUH_SVC_ext svc_ext;
    };

    union
    {
        void* rbsp;
        AUD*  aud;
        SPS*  sps;
        PPS*  pps;
        SEI*  sei;
        Slice* slice;
    };
};

struct AU
{
    Bs32u  NumUnits;
    NALU** nalu;
};

}