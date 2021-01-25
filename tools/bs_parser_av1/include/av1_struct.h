// Copyright (c) 2018-2020 Intel Corporation
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

#ifndef __AV1_STRUCT_H
#define __AV1_STRUCT_H

#include <bs_def.h>

typedef class AV1_BitStream* BS_AV1_hdl;

namespace BS_AV1
{

enum CONTAINER {
    IVF = 0,
    ELEMENTARY_STREAM = 1
};

enum INIT_MODE
{
    INIT_MODE_RAW   = 0x00000000,
    INIT_MODE_IVF   = 0x00000001,
    INIT_MODE_MKV   = 0x00000002,
    INIT_MODE_WebM  = INIT_MODE_MKV,
};

enum TRACE_LEVEL
{
     TRACE_LEVEL_CONTAINER  = 0x00000001,
     TRACE_LEVEL_FRAME      = 0x00000002,
     TRACE_LEVEL_UH         = 0x00000004,
     TRACE_LEVEL_CH         = 0x00000008,
     TRACE_LEVEL_BLOCK      = 0x00000010,
     TRACE_LEVEL_FULL       = 0xFFFFFFFF
};

const Bs8u MAX_NUM_TEMPORAL_LAYERS = 8;
const Bs8u MAX_NUM_SPATIAL_LAYERS = 4;
const Bs8u MAX_NUM_OPERATING_POINTS = MAX_NUM_TEMPORAL_LAYERS * MAX_NUM_SPATIAL_LAYERS;
const Bs8u REF_FRAMES_LOG2 = 3;
const Bs8u NUM_REF_FRAMES = 1 << REF_FRAMES_LOG2;

enum AV1_OBU_TYPE
{
    OBU_SEQUENCE_HEADER = 1,
    OBU_TEMPORAL_DELIMITER = 2,
    OBU_FRAME_HEADER = 3,
    OBU_TILE_GROUP = 4,
    OBU_METADATA = 5,
    OBU_FRAME = 6,
    OBU_REDUNDANT_FRAME_HEADER = 7,
    OBU_PADDING = 15,
};


enum REFERENCE_MODE {
    SINGLE_REFERENCE = 0,
    COMPOUND_REFERENCE = 1,
    REFERENCE_MODE_SELECT = 2,
    REFERENCE_MODES = 3,
};

enum SB_SIZE
{
    BLOCK_64X64 = 0,
    BLOCK_128X128 = 1,
};

enum AOM_COLOR_PRIMARIES
{
    AOM_CICP_CP_RESERVED_0 = 0,  /**< For future use */
    AOM_CICP_CP_BT_709 = 1,      /**< BT.709 */
    AOM_CICP_CP_UNSPECIFIED = 2, /**< Unspecified */
    AOM_CICP_CP_RESERVED_3 = 3,  /**< For future use */
    AOM_CICP_CP_BT_470_M = 4,    /**< BT.470 System M (historical) */
    AOM_CICP_CP_BT_470_B_G = 5,  /**< BT.470 System B, G (historical) */
    AOM_CICP_CP_BT_601 = 6,      /**< BT.601 */
    AOM_CICP_CP_SMPTE_240 = 7,   /**< SMPTE 240 */
    AOM_CICP_CP_GENERIC_FILM =
    8, /**< Generic film (color filters using illuminant C) */
    AOM_CICP_CP_BT_2020 = 9,      /**< BT.2020, BT.2100 */
    AOM_CICP_CP_XYZ = 10,         /**< SMPTE 428 (CIE 1921 XYZ) */
    AOM_CICP_CP_SMPTE_431 = 11,   /**< SMPTE RP 431-2 */
    AOM_CICP_CP_SMPTE_432 = 12,   /**< SMPTE EG 432-1  */
    AOM_CICP_CP_RESERVED_13 = 13, /**< For future use (values 13 - 21)  */
    AOM_CICP_CP_EBU_3213 = 22,    /**< EBU Tech. 3213-E  */
    AOM_CICP_CP_RESERVED_23 = 23  /**< For future use (values 23 - 255)  */
};

enum AOM_TRANSFER_CHARACTERISTICS
{
    AOM_CICP_TC_RESERVED_0 = 0,  /**< For future use */
    AOM_CICP_TC_BT_709 = 1,      /**< BT.709 */
    AOM_CICP_TC_UNSPECIFIED = 2, /**< Unspecified */
    AOM_CICP_TC_RESERVED_3 = 3,  /**< For future use */
    AOM_CICP_TC_BT_470_M = 4,    /**< BT.470 System M (historical)  */
    AOM_CICP_TC_BT_470_B_G = 5,  /**< BT.470 System B, G (historical) */
    AOM_CICP_TC_BT_601 = 6,      /**< BT.601 */
    AOM_CICP_TC_SMPTE_240 = 7,   /**< SMPTE 240 M */
    AOM_CICP_TC_LINEAR = 8,      /**< Linear */
    AOM_CICP_TC_LOG_100 = 9,     /**< Logarithmic (100 : 1 range) */
    AOM_CICP_TC_LOG_100_SQRT10 =
    10,                     /**< Logarithmic (100 * Sqrt(10) : 1 range) */
    AOM_CICP_TC_IEC_61966 = 11, /**< IEC 61966-2-4 */
    AOM_CICP_TC_BT_1361 = 12,   /**< BT.1361 */
    AOM_CICP_TC_SRGB = 13,      /**< sRGB or sYCC*/
    AOM_CICP_TC_BT_2020_10_BIT = 14, /**< BT.2020 10-bit systems */
    AOM_CICP_TC_BT_2020_12_BIT = 15, /**< BT.2020 12-bit systems */
    AOM_CICP_TC_SMPTE_2084 = 16,     /**< SMPTE ST 2084, ITU BT.2100 PQ */
    AOM_CICP_TC_SMPTE_428 = 17,      /**< SMPTE ST 428 */
    AOM_CICP_TC_HLG = 18,            /**< BT.2100 HLG, ARIB STD-B67 */
    AOM_CICP_TC_RESERVED_19 = 19     /**< For future use (values 19-255) */
};

enum  AOM_MATRIX_COEFFICIENTS
{
    AOM_CICP_MC_IDENTITY = 0,    /**< Identity matrix */
    AOM_CICP_MC_BT_709 = 1,      /**< BT.709 */
    AOM_CICP_MC_UNSPECIFIED = 2, /**< Unspecified */
    AOM_CICP_MC_RESERVED_3 = 3,  /**< For future use */
    AOM_CICP_MC_FCC = 4,         /**< US FCC 73.628 */
    AOM_CICP_MC_BT_470_B_G = 5,  /**< BT.470 System B, G (historical) */
    AOM_CICP_MC_BT_601 = 6,      /**< BT.601 */
    AOM_CICP_MC_SMPTE_240 = 7,   /**< SMPTE 240 M */
    AOM_CICP_MC_SMPTE_YCGCO = 8, /**< YCgCo */
    AOM_CICP_MC_BT_2020_NCL =
    9, /**< BT.2020 non-constant luminance, BT.2100 YCbCr  */
    AOM_CICP_MC_BT_2020_CL = 10, /**< BT.2020 constant luminance */
    AOM_CICP_MC_SMPTE_2085 = 11, /**< SMPTE ST 2085 YDzDx */
    AOM_CICP_MC_CHROMAT_NCL =
    12, /**< Chromaticity-derived non-constant luminance */
    AOM_CICP_MC_CHROMAT_CL = 13, /**< Chromaticity-derived constant luminance */
    AOM_CICP_MC_ICTCP = 14,      /**< BT.2100 ICtCp */
    AOM_CICP_MC_RESERVED_15 = 15 /**< For future use (values 15-255)  */
};

enum AOM_COLOR_RANGE
{
    AOM_CR_STUDIO_RANGE = 0, /**< Y [16..235], UV [16..240] */
    AOM_CR_FULL_RANGE = 1    /**< YUV/RGB [0..255] */
};

enum AOM_CHROMA_SAMPLE_POSITION
{
    AOM_CSP_UNKNOWN = 0,          /**< Unknown */
    AOM_CSP_VERTICAL = 1,         /**< Horizontally co-located with luma(0, 0)*/
                                  /**< sample, between two vertical samples */
    AOM_CSP_COLOCATED = 2,        /**< Co-located with luma(0, 0) sample */
    AOM_CSP_RESERVED = 3          /**< Reserved value */
};

const Bs8u SELECT_SCREEN_CONTENT_TOOLS = 2;
const Bs8u SELECT_INTEGER_MV = 2;

struct OBUHeader
{
    AV1_OBU_TYPE obu_type;
    Bs32u obu_has_size_field;
    Bs32u temporal_id;
    Bs32u spatial_id;
};

struct TimingInfo
{
    Bs32u num_units_in_display_tick;
    Bs32u time_scale;
    Bs32u equal_picture_interval;
    Bs32u num_ticks_per_picture_minus_1;
};

struct DecoderModelInfo
{
    Bs32u buffer_delay_length_minus_1;
    Bs32u num_units_in_decoding_tick;
    Bs32u buffer_removal_time_length_minus_1;
    Bs32u frame_presentation_time_length_minus_1;
};

struct OperatingParametersInfo
{
    Bs32u decoder_buffer_delay;
    Bs32u encoder_buffer_delay;
    Bs32u low_delay_mode_flag;
};

struct ColorConfig
{
    Bs32u BitDepth;
    Bs32u mono_chrome;
    Bs32u color_primaries;
    Bs32u transfer_characteristics;
    Bs32u matrix_coefficients;
    Bs32u color_range;
    Bs32u chroma_sample_position;
    Bs32u subsampling_x;
    Bs32u subsampling_y;
    Bs32u separate_uv_delta_q;
};

struct SequenceHeader
{
    //Rev 0.85 parameters (AV1 spec version 1.0) in order of appearance/calculation in sequence_header_obu()
    Bs32u seq_profile;
    Bs32u still_picture;
    Bs32u reduced_still_picture_header;

    Bs32u timing_info_present_flag;
    TimingInfo timing_info;

    Bs32u decoder_model_info_present_flag;
    DecoderModelInfo decoder_model_info;

    Bs32u operating_points_cnt_minus_1;
    Bs32u operating_point_idc[MAX_NUM_OPERATING_POINTS];
    Bs32u seq_level_idx[MAX_NUM_OPERATING_POINTS];
    Bs32u seq_tier[MAX_NUM_OPERATING_POINTS];
    Bs32u decoder_model_present_for_this_op[MAX_NUM_OPERATING_POINTS];
    OperatingParametersInfo operating_parameters_info[MAX_NUM_OPERATING_POINTS];
    Bs32u initial_display_delay_minus_1[MAX_NUM_OPERATING_POINTS];

    Bs32u frame_width_bits;
    Bs32u frame_height_bits;
    Bs32u max_frame_width;
    Bs32u max_frame_height;
    Bs32u frame_id_numbers_present_flag;
    Bs32u delta_frame_id_length;
    Bs32u idLen;
    Bs32u sbSize;
    Bs32u enable_filter_intra;
    Bs32u enable_intra_edge_filter;
    Bs32u enable_interintra_compound;
    Bs32u enable_masked_compound;
    Bs32u enable_warped_motion;
    Bs32u enable_dual_filter;
    Bs32u enable_order_hint;
    Bs32u enable_jnt_comp;
    Bs32u enable_ref_frame_mvs;
    Bs32u seq_force_screen_content_tools;
    Bs32u seq_force_integer_mv;
    Bs32s order_hint_bits_minus1;
    Bs32u enable_superres;
    Bs32u enable_cdef;
    Bs32u enable_restoration;

    ColorConfig color_config;

    Bs32u film_grain_param_present;
};

enum FRAME_TYPE
{
    KEY_FRAME = 0,
    INTER_FRAME = 1,
    INTRA_ONLY_FRAME = 2,  // replaces intra-only
    SWITCH_FRAME = 3,
    FRAME_TYPES,
};

const Bs8u INTER_REFS = 7;

enum INTERP_FILTER {
    EIGHTTAP_REGULAR,
    EIGHTTAP_SMOOTH,
    MULTITAP_SHARP,
    BILINEAR,
    INTERP_FILTERS_ALL,
    SWITCHABLE_FILTERS = BILINEAR,
    SWITCHABLE = SWITCHABLE_FILTERS + 1, /* the last switchable one */
    EXTRA_FILTERS = INTERP_FILTERS_ALL - SWITCHABLE_FILTERS,
};

const Bs32u MAX_TILE_ROWS = 1024;
const Bs32u MAX_TILE_COLS = 1024;

struct TileInfo
{
    Bs32u uniform_tile_spacing_flag;
    Bs32u TileColsLog2;
    Bs32u TileRowsLog2;
    Bs32u TileCols;
    Bs32u TileRows;
    Bs32u SbColStarts[MAX_TILE_COLS + 1];  // valid for 0 <= i <= TileCols
    Bs32u SbRowStarts[MAX_TILE_ROWS + 1];  // valid for 0 <= i <= TileRows
    Bs32u context_update_tile_id;
    Bs32u TileSizeBytes;
};

struct QuantizationParams
{
    Bs32u base_q_idx;
    Bs32s DeltaQYDc;
    Bs32s DeltaQUDc;
    Bs32s DeltaQUAc;
    Bs32s DeltaQVDc;
    Bs32s DeltaQVAc;
    Bs32u using_qmatrix;
    Bs32u qm_y;
    Bs32u qm_u;
    Bs32u qm_v;
};

const Bs8u MAX_NUM_OF_SEGMENTS = 8;
enum SEG_LVL_FEATURES {
    SEG_LVL_ALT_Q,       // Use alternate Quantizer ....
    SEG_LVL_ALT_LF_Y_V,  // Use alternate loop filter value on y plane vertical
    SEG_LVL_ALT_LF_Y_H,  // Use alternate loop filter value on y plane horizontal
    SEG_LVL_ALT_LF_U,    // Use alternate loop filter value on u plane
    SEG_LVL_ALT_LF_V,    // Use alternate loop filter value on v plane
    SEG_LVL_REF_FRAME,   // Optional Segment reference frame
    SEG_LVL_SKIP,        // Optional Segment (0,0) + skip mode
    SEG_LVL_GLOBALMV,
    SEG_LVL_MAX
};

struct SegmentationParams
{
    Bs8u segmentation_enabled;
    Bs8u segmentation_update_map;
    Bs8u segmentation_temporal_update;
    Bs8u segmentation_update_data;

    Bs32s FeatureData[MAX_NUM_OF_SEGMENTS][SEG_LVL_MAX];
    Bs32u FeatureMask[MAX_NUM_OF_SEGMENTS];

};

const Bs8u TOTAL_REFS = 8;
const Bs8u MAX_MODE_LF_DELTAS = 2;

struct LoopFilterParams
{
    Bs32s loop_filter_level[4];
    Bs32s loop_filter_sharpness;
    Bs8u loop_filter_delta_enabled;
    Bs8u loop_filter_delta_update;
    // 0 = Intra, Last, Last2, Last3, GF, BWD, ARF
    Bs8s loop_filter_ref_deltas[TOTAL_REFS];
    // 0 = ZERO_MV, MV
    Bs8s loop_filter_mode_deltas[MAX_MODE_LF_DELTAS];
};

const Bs8u CDEF_MAX_STRENGTHS = 8;
const Bs8u CDEF_STRENGTH_BITS = 6;

struct CdefParams
{
    Bs32u cdef_damping;
    Bs32u cdef_bits;
    Bs32u cdef_y_pri_strength[CDEF_MAX_STRENGTHS];
    Bs32u cdef_y_sec_strength[CDEF_MAX_STRENGTHS];
    Bs32u cdef_uv_pri_strength[CDEF_MAX_STRENGTHS];
    Bs32u cdef_uv_sec_strength[CDEF_MAX_STRENGTHS];
    Bs32u cdef_y_strength[CDEF_MAX_STRENGTHS];
    Bs32u cdef_uv_strength[CDEF_MAX_STRENGTHS];
};

enum RestorationType
{
    RESTORE_NONE,
    RESTORE_WIENER,
    RESTORE_SGRPROJ,
    RESTORE_SWITCHABLE,
    RESTORE_SWITCHABLE_TYPES = RESTORE_SWITCHABLE,
    RESTORE_TYPES = 4,
};

const Bs8u  MAX_MB_PLANE = 3;

struct LRParams
{
    RestorationType lr_type[MAX_MB_PLANE];
    Bs32u lr_unit_shift;
    Bs32u lr_uv_shift;
};

enum TRANSFORMATION_TYPE {
    IDENTITY = 0,      // identity transformation, 0-parameter
    TRANSLATION = 1,   // translational motion 2-parameter
    ROTZOOM = 2,       // simplified affine with rotation + zoom only, 4-parameter
    AFFINE = 3,        // affine, 6-parameter
    HORTRAPEZOID = 4,  // constrained homography, hor trapezoid, 6-parameter
    VERTRAPEZOID = 5,  // constrained homography, ver trapezoid, 6-parameter
    HOMOGRAPHY = 6,    // homography, 8-parameter
    TRANS_TYPES = 7,
};

struct GlobalMotionParams
{
    TRANSFORMATION_TYPE wmtype;
    Bs32s wmmat[8];
    Bs16s alpha;
    Bs16s beta;
    Bs16s gamma;
    Bs16s delta;
    Bs8s invalid;
};

const Bs8u MAX_AUTOREG_COEFFS_LUMA = 24;
const Bs8u MAX_AUTOREG_COEFFS_CHROMA = MAX_AUTOREG_COEFFS_LUMA + 1;
const Bs8u MAX_POINTS_IN_SCALING_FUNCTION_LUMA = 14;
const Bs8u MAX_POINTS_IN_SCALING_FUNCTION_CHROMA = 10;

struct FilmGrainParams {
    Bs32u apply_grain;
    Bs32u grain_seed;
    Bs32u update_grain;

    Bs32u film_grain_params_ref_idx;

    // 8 bit values

    Bs32s num_y_points;  // value: 0..14
    Bs32s point_y_value[MAX_POINTS_IN_SCALING_FUNCTION_LUMA];
    Bs32s point_y_scaling[MAX_POINTS_IN_SCALING_FUNCTION_LUMA];

    Bs32s chroma_scaling_from_luma;

    // 8 bit values
    Bs32s num_cb_points;  // value: 0..10
    Bs32s point_cb_value[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];
    Bs32s point_cb_scaling[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];

    // 8 bit values
    Bs32s num_cr_points;  // value: 0..10
    Bs32s point_cr_value[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];
    Bs32s point_cr_scaling[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];

    Bs32s grain_scaling;

    Bs32s ar_coeff_lag;  // values:  0..3

    // 8 bit values
    Bs32s ar_coeffs_y[MAX_AUTOREG_COEFFS_LUMA];
    Bs32s ar_coeffs_cb[MAX_AUTOREG_COEFFS_CHROMA];
    Bs32s ar_coeffs_cr[MAX_AUTOREG_COEFFS_CHROMA];

    // Shift value: AR coeffs range
    // 6: [-2, 2)
    // 7: [-1, 1)
    // 8: [-0.5, 0.5)
    // 9: [-0.25, 0.25)
    Bs32s ar_coeff_shift;  // values : 6..9
    Bs32s grain_scale_shift;

    Bs32s cb_mult;       // 8 bits
    Bs32s cb_luma_mult;  // 8 bits
    Bs32s cb_offset;     // 9 bits

    Bs32s cr_mult;       // 8 bits
    Bs32s cr_luma_mult;  // 8 bits
    Bs32s cr_offset;     // 9 bits

    Bs32s overlap_flag;

    Bs32s clip_to_restricted_range;

    Bs32s BitDepth;  // video bit depth
};


enum TX_MODE {
    ONLY_4X4 = 0,     // only 4x4 transform used
    TX_MODE_LARGEST,  // transform size is the largest possible for pu size
    TX_MODE_SELECT,   // transform specified for each block
    TX_MODES,
};

struct FrameHeader
{
    //Rev 0.85 parameters (AV1 spec version 1.0) in order of appearance/calculation in uncompressed_header()
    Bs32u show_existing_frame;
    Bs32u frame_to_show_map_idx;
    Bs64u frame_presentation_time;
    Bs32u display_frame_id;
    FRAME_TYPE frame_type;
    Bs32u show_frame;
    Bs32u showable_frame;
    Bs32u error_resilient_mode;
    Bs32u disable_cdf_update;
    Bs32u allow_screen_content_tools;
    Bs32u force_integer_mv;
    Bs32u current_frame_id;
    Bs32u frame_size_override_flag;
    Bs32u order_hint;
    Bs32u primary_ref_frame;

    Bs8u refresh_frame_flags;
    Bs32u ref_order_hint[NUM_REF_FRAMES];

    Bs32u FrameWidth;
    Bs32u FrameHeight;
    Bs32u SuperresDenom;
    Bs32u UpscaledWidth;
    Bs32u MiCols;
    Bs32u MiRows;
    Bs32u RenderWidth;
    Bs32u RenderHeight;

    Bs32u allow_intrabc;
    Bs32s ref_frame_idx[INTER_REFS];
    Bs32u allow_high_precision_mv;
    INTERP_FILTER interpolation_filter;
    Bs32u is_motion_mode_switchable;
    Bs32u use_ref_frame_mvs;
    Bs32u disable_frame_end_update_cdf;

    Bs32u sbCols;
    Bs32u sbRows;
    Bs32u sbSize;

    TileInfo tile_info;
    QuantizationParams quantization_params;
    SegmentationParams segmentation_params;

    Bs32u delta_q_present;
    Bs32u delta_q_res;

    Bs32u delta_lf_present;
    Bs32u delta_lf_res;
    Bs32u delta_lf_multi;

    Bs32u CodedLossless;
    Bs32u AllLossless;

    LoopFilterParams loop_filter_params;
    CdefParams cdef_params;
    LRParams lr_params;

    TX_MODE TxMode;
    Bs32u reference_mode;
    Bs32u skip_mode_present;
    Bs32u allow_warped_motion;
    Bs32u reduced_tx_set;

    GlobalMotionParams global_motion_params[TOTAL_REFS];

    FilmGrainParams film_grain_params;

    Bs32u NumPlanes;

    Bs32u large_scale_tile;

    //Rev 0.5 parameters
    Bs32u enable_interintra_compound;
    Bs32u enable_masked_compound;
    Bs32u enable_intra_edge_filter;
    Bs32u enable_filter_intra;
};

struct Context
{
};

struct CompressedHeader
{
};

struct ivf_header{
    Bs32u signature;
    Bs16u version;
    Bs16u length;
    Bs32u codec_FourCC;
    Bs16u width;
    Bs16u height;
    Bs32u frame_rate;
    Bs32u time_scale;
    Bs32u num_frames;
};

struct ivf_frame_header{
    Bs32u frame_size;
    union{
        Bs64u time_stamp;
        struct{
            Bs32u time_stamp_l;
            Bs32u time_stamp_h;
        };
    };
};

struct Frame
{
    Bs32u FrameOrder;
    Bs32u NumBytes;

    ivf_header          ivf;
    ivf_frame_header    ivf_frame;

    OBUHeader obu_hdr_s;
    OBUHeader obu_hdr_f;

    SequenceHeader      sh;
    FrameHeader         fh;
};

typedef struct{
    ivf_header          ivf;
    ivf_frame_header    ivf_frame;
}header;

}
#endif //__AV1_STRUCT_H
