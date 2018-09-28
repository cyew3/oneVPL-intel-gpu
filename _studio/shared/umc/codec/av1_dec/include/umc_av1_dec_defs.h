//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#ifndef __UMC_AV1_DEC_DEFS_DEC_H__
#define __UMC_AV1_DEC_DEFS_DEC_H__

#include <stdexcept>
#include <vector>
#include "umc_structures.h"
#include "umc_vp9_dec_defs.h"

namespace UMC_AV1_DECODER
{
    class AV1DecoderFrame;
    typedef std::vector<AV1DecoderFrame*> DPBType;

    using UMC_VP9_DECODER::NUM_REF_FRAMES;

    const Ipp8u SYNC_CODE_0 = 0x49;
    const Ipp8u SYNC_CODE_1 = 0x83;
    const Ipp8u SYNC_CODE_2 = 0x43;

    const Ipp8u FRAME_MARKER = 0x2;

    const Ipp8u MINIMAL_DATA_SIZE = 4;

    const Ipp8u INTER_REFS                    = 7;
    const Ipp8u TOTAL_REFS                    = 8;

    const Ipp8u QM_LEVEL_BITS                 = 4;

    const Ipp8u LOG2_SWITCHABLE_FILTERS       = 2;

    const Ipp8u CDEF_MAX_STRENGTHS            = 8;

    const Ipp8u CDEF_STRENGTH_BITS            = 6;

    const Ipp8u  MAX_MB_PLANE                 = 3;
    const Ipp8u MAX_LEB128_SIZE               = 8;
    const Ipp8u LEB128_BYTE_MASK              = 0x7f;
    const Ipp8u MAX_SB_SIZE_LOG2              = 7;

    const Ipp16u RESTORATION_UNITSIZE_MAX     = 256;

    const Ipp8u MI_SIZE_LOG2 = 2;
    const Ipp8u MAX_MIB_SIZE_LOG2 = MAX_SB_SIZE_LOG2 - MI_SIZE_LOG2;

    const Ipp8u SCALE_NUMERATOR = 8;
    const Ipp8u SUPERRES_SCALE_BITS = 3;
    const Ipp8u SUPERRES_SCALE_DENOMINATOR_MIN = SCALE_NUMERATOR + 1;
    const Ipp8u PRIMARY_REF_BITS = 3;
    const Ipp8u PRIMARY_REF_NONE = 7;
    const Ipp8u NO_FILTER_FOR_IBC = 1;

    const Ipp32u MAX_TILE_WIDTH = 4096;        // Max Tile width in pixels
    const Ipp32u MAX_TILE_AREA  = 4096 * 2304;  // Maximum tile area in pixels
    const Ipp32u MAX_TILE_ROWS  = 1024;
    const Ipp32u MAX_TILE_COLS  = 1024;

    const Ipp8u FRAME_CONTEXTS_LOG2           = 3;
    const Ipp8u MAX_MODE_LF_DELTAS            = 2;

    const Ipp8u WARPEDMODEL_PREC_BITS         = 16;

    const Ipp8u MAX_NUM_TEMPORAL_LAYERS       = 8;
    const Ipp8u MAX_NUM_SPATIAL_LAYERS        = 4;
    const Ipp8u MAX_NUM_OPERATING_POINTS      = MAX_NUM_TEMPORAL_LAYERS * MAX_NUM_SPATIAL_LAYERS;
    const Ipp8u SELECT_SCREEN_CONTENT_TOOLS   = 2;
    const Ipp8u SELECT_INTEGER_MV             = 2;

    enum AV1_OBU_TYPE
    {
        OBU_SEQUENCE_HEADER = 1,
        OBU_TEMPORAL_DELIMITER = 2,
        OBU_FRAME_HEADER = 3,
        OBU_TILE_GROUP = 4,
        OBU_METADATA = 5,
        OBU_FRAME = 6,
        OBU_PADDING = 15,
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

    enum FRAME_TYPE
    {
        KEY_FRAME = 0,
        INTER_FRAME = 1,
        INTRA_ONLY_FRAME = 2,  // replaces intra-only
        S_FRAME = 3,
        FRAME_TYPES,
    };

    enum SB_SIZE
    {
        BLOCK_64X64 = 0,
        BLOCK_128X128 = 1,
    };

    using UMC_VP9_DECODER::VP9_MAX_NUM_OF_SEGMENTS;
    using UMC_VP9_DECODER::MAX_LOOP_FILTER;

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

    const Ipp8u SEG_FEATURE_DATA_SIGNED[SEG_LVL_MAX] = { 1, 1, 1, 1, 1, 0, 0};
    const Ipp8u SEG_FEATURE_DATA_MAX[SEG_LVL_MAX] = { UMC_VP9_DECODER::MAXQ,
                                                      MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER,
                                                      3,
                                                      0 };

    struct AV1Segmentation
    {
        Ipp8u segmentation_enabled;
        Ipp8u segmentation_update_map;
        Ipp8u segmentation_temporal_update;
        Ipp8u segmentation_update_data;

        Ipp32s FeatureData[VP9_MAX_NUM_OF_SEGMENTS][SEG_LVL_MAX];
        Ipp32u FeatureMask[VP9_MAX_NUM_OF_SEGMENTS];

    };

    enum {
        RESET_FRAME_CONTEXT_NONE = 0,
        RESET_FRAME_CONTEXT_CURRENT = 1,
        RESET_FRAME_CONTEXT_ALL = 2
    };

    enum INTERP_FILTER{
        EIGHTTAP_REGULAR,
        EIGHTTAP_SMOOTH,
        MULTITAP_SHARP,
        BILINEAR,
        INTERP_FILTERS_ALL,
        SWITCHABLE_FILTERS = BILINEAR,
        SWITCHABLE = SWITCHABLE_FILTERS + 1, /* the last switchable one */
        EXTRA_FILTERS = INTERP_FILTERS_ALL - SWITCHABLE_FILTERS,
    };

    enum TX_MODE{
        ONLY_4X4 = 0,     // only 4x4 transform used
        TX_MODE_LARGEST,  // transform size is the largest possible for pu size
        TX_MODE_SELECT,   // transform specified for each block
        TX_MODES,
    };

    enum REFERENCE_MODE {
        SINGLE_REFERENCE = 0,
        COMPOUND_REFERENCE = 1,
        REFERENCE_MODE_SELECT = 2,
        REFERENCE_MODES = 3,
    };

    enum REFRESH_FRAME_CONTEXT_MODE {
        REFRESH_FRAME_CONTEXT_DISABLED,
        REFRESH_FRAME_CONTEXT_BACKWARD,
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

    enum MV_REFERENCE_FRAME
    {
        NONE = -1,
        INTRA_FRAME = 0,
        LAST_FRAME = 1,
        LAST2_FRAME = 2,
        LAST3_FRAME = 3,
        GOLDEN_FRAME = 4,
        BWDREF_FRAME = 5,
        ALTREF2_FRAME = 6,
        ALTREF_FRAME = 7,
        MAX_REF_FRAMES = 8
    };

    struct TimingInfo
    {
        Ipp32u num_units_in_display_tick;
        Ipp32u time_scale;
        Ipp32u equal_picture_interval;
        Ipp32u num_ticks_per_picture_minus_1;
    };

    struct DecoderModelInfo
    {
        Ipp32u buffer_delay_length_minus_1;
        Ipp32u num_units_in_decoding_tick;
        Ipp32u buffer_removal_time_length_minus_1;
        Ipp32u frame_presentation_time_length_minus_1;
    };

    struct OperatingParametersInfo
    {
        Ipp32u decoder_buffer_delay;
        Ipp32u encoder_buffer_delay;
        Ipp32u low_delay_mode_flag;
    };

    struct ColorConfig
    {
        Ipp32u BitDepth;
        Ipp32u mono_chrome;
        Ipp32u color_primaries;
        Ipp32u transfer_characteristics;
        Ipp32u matrix_coefficients;
        Ipp32u color_range;
        Ipp32u chroma_sample_position;
        Ipp32u subsampling_x;
        Ipp32u subsampling_y;
        Ipp32u separate_uv_delta_q;
    };

    struct SequenceHeader
    {
        //Rev 0.85 parameters (AV1 spec version 1.0) in order of appearance/calculation in sequence_header_obu()
        Ipp32u seq_profile;
        Ipp32u still_picture;
        Ipp32u reduced_still_picture_header;

        Ipp32u timing_info_present_flag;
        TimingInfo timing_info;

        Ipp32u decoder_model_info_present_flag;
        DecoderModelInfo decoder_model_info;

        Ipp32u operating_points_cnt_minus_1;
        Ipp32u operating_point_idc[MAX_NUM_OPERATING_POINTS];
        Ipp32u seq_level_idx[MAX_NUM_OPERATING_POINTS];
        Ipp32u seq_tier[MAX_NUM_OPERATING_POINTS];
        Ipp32u decoder_model_present_for_this_op[MAX_NUM_OPERATING_POINTS];
        OperatingParametersInfo operating_parameters_info[MAX_NUM_OPERATING_POINTS];
        Ipp32u initial_display_delay_minus_1[MAX_NUM_OPERATING_POINTS];

        Ipp32u frame_width_bits;
        Ipp32u frame_height_bits;
        Ipp32u max_frame_width;
        Ipp32u max_frame_height;
        Ipp32u frame_id_numbers_present_flag;
        Ipp32u delta_frame_id_length;
        Ipp32u idLen;
        Ipp32u sbSize;
        Ipp32u enable_filter_intra;
        Ipp32u enable_intra_edge_filter;
        Ipp32u enable_interintra_compound;
        Ipp32u enable_masked_compound;
        Ipp32u enable_warped_motion;
        Ipp32u enable_dual_filter;
        Ipp32u enable_order_hint;
        Ipp32u enable_jnt_comp;
        Ipp32u enable_ref_frame_mvs;
        Ipp32u seq_force_screen_content_tools;
        Ipp32u seq_force_integer_mv;
        Ipp32s order_hint_bits_minus1;
        Ipp32u enable_superres;
        Ipp32u enable_cdef;
        Ipp32u enable_restoration;

        ColorConfig color_config;

        Ipp32u film_grain_param_present;
    };

    struct Loopfilter
    {
        Ipp32s loop_filter_level[4];

        Ipp32s loop_filter_sharpness;

        Ipp8u loop_filter_delta_enabled;
        Ipp8u loop_filter_delta_update;

        // 0 = Intra, Last, Last2, Last3, GF, BWD, ARF
        Ipp8s loop_filter_ref_deltas[TOTAL_REFS];

        // 0 = ZERO_MV, MV
        Ipp8s loop_filter_mode_deltas[MAX_MODE_LF_DELTAS];
    };

    struct WarpedMotionParams {
        TRANSFORMATION_TYPE wmtype;
        Ipp32s wmmat[8];
        Ipp16u alpha;
        Ipp16u beta;
        Ipp16u gamma;
        Ipp16u delta;
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

    struct RestorationInfo
    {
        RestorationType frameRestorationType;
        Ipp32s restorationUnitSize;
    };

    struct FilmGrain{
        Ipp32s apply_grain;
        Ipp32s update_parameters;

        // 8 bit values
        Ipp32s scaling_points_y[14][2];
        Ipp32s num_y_points;  // value: 0..14

        // 8 bit values
        Ipp32s scaling_points_cb[10][2];
        Ipp32s num_cb_points;  // value: 0..10

        // 8 bit values
        Ipp32s scaling_points_cr[10][2];
        Ipp32s num_cr_points;  // value: 0..10

        Ipp32s scaling_shift;  // values : 8..11

        Ipp32s ar_coeff_lag;  // values:  0..3

        // 8 bit values
        Ipp32s ar_coeffs_y[24];
        Ipp32s ar_coeffs_cb[25];
        Ipp32s ar_coeffs_cr[25];

        // Shift value: AR coeffs range
        // 6: [-2, 2)
        // 7: [-1, 1)
        // 8: [-0.5, 0.5)
        // 9: [-0.25, 0.25)
        Ipp32s ar_coeff_shift;  // values : 6..9

        Ipp32s cb_mult;       // 8 bits
        Ipp32s cb_luma_mult;  // 8 bits
        Ipp32s cb_offset;     // 9 bits

        Ipp32s cr_mult;       // 8 bits
        Ipp32s cr_luma_mult;  // 8 bits
        Ipp32s cr_offset;     // 9 bits

        Ipp32s overlap_flag;

        Ipp32s clip_to_restricted_range;

        Ipp32s BitDepth;  // video bit depth

        Ipp32s chroma_scaling_from_luma;

        Ipp32s grain_scale_shift;

        Ipp32u random_seed;
    };

    struct  SizeOfFrame{
        Ipp32u FrameWidth;
        Ipp32u FrameHeight;
    };

    struct FrameHeader
    {
        //Rev 0.85 parameters (AV1 spec version 1.0) in order of appearance/calculation in uncompressed_header()
        Ipp32u show_existing_frame;
        Ipp32u frame_to_show_map_idx;
        Ipp32u display_frame_id;
        FRAME_TYPE frame_type;
        Ipp32u show_frame;
        Ipp32u showable_frame;
        Ipp32u error_resilient_mode;
        Ipp32u disable_cdf_update;
        Ipp32u allow_screen_content_tools;
        Ipp32u seq_force_integer_mv;
        Ipp32u current_frame_id;
        Ipp32u frame_size_override_flag;
        Ipp32u order_hint;
        Ipp32u primary_ref_frame;

        Ipp8u refresh_frame_flags;

        Ipp32u FrameWidth;
        Ipp32u FrameHeight;
        Ipp32u SuperresDenom;
        Ipp32u UpscaledWidth;
        Ipp32u MiCols;
        Ipp32u MiRows;
        Ipp32u RenderWidth;
        Ipp32u RenderHeight;

        Ipp32u allow_intrabc;
        Ipp32s ref_frame_idx[INTER_REFS];
        Ipp32u allow_high_precision_mv;
        INTERP_FILTER interpolation_filter;
        Ipp32u is_motion_mode_switchable;
        Ipp32u use_ref_frame_mvs;

        Ipp32u sbCols;
        Ipp32u sbRows;
        Ipp32u sbSize;
        Ipp32u uniform_tile_spacing_flag;
        Ipp32u TileColsLog2;
        Ipp32u TileRowsLog2;
        Ipp32u TileCols;
        Ipp32u TileRows;
        Ipp32u SbColStarts[MAX_TILE_COLS + 1];  // valid for 0 <= i <= TileCols
        Ipp32u SbRowStarts[MAX_TILE_ROWS + 1];  // valid for 0 <= i <= TileRows
        Ipp32u TileSizeBytes;

        Ipp32u base_q_idx;
        Ipp32s DeltaQYDc;
        Ipp32s DeltaQUDc;
        Ipp32s DeltaQUAc;
        Ipp32s DeltaQVDc;
        Ipp32s DeltaQVac;
        Ipp32u using_qmatrix;
        Ipp32u qm_y;
        Ipp32u qm_u;
        Ipp32u qm_v;

        AV1Segmentation segmentation_params;

        Ipp32u delta_q_present;
        Ipp32u delta_q_res;

        Ipp32u delta_lf_present;
        Ipp32u delta_lf_res;
        Ipp32u delta_lf_multi;

        Ipp32u lossless;

        Loopfilter loop_filter_params;

        Ipp32u cdef_damping;
        Ipp32u cdef_bits;
        Ipp32u cdef_y_strength[CDEF_MAX_STRENGTHS];
        Ipp32u cdef_uv_strength[CDEF_MAX_STRENGTHS];

        RestorationType lr_type[MAX_MB_PLANE];
        Ipp32u lr_unit_shift;
        Ipp32u lr_uv_shift;

        TX_MODE TxMode;
        Ipp32u reference_mode;
        Ipp32u skipModeAllowed;
        Ipp32u reduced_tx_set;

        WarpedMotionParams global_motion_params[TOTAL_REFS];

        FilmGrain film_grain_params;

        Ipp32u NumPlanes;

        Ipp32u large_scale_tile;

        //Rev 0.5 parameters
        Ipp32u refresh_frame_context;
        Ipp32u loop_filter_across_tiles_v_enabled;
        Ipp32u loop_filter_across_tiles_h_enabled;
        Ipp32u enable_interintra_compound;
        Ipp32u enable_masked_compound;
        Ipp32u enable_intra_edge_filter;
        Ipp32u enable_filter_intra;
    };

    struct OBUHeader
    {
        AV1_OBU_TYPE obu_type;
#if UMC_AV1_DECODER_REV >= 8500
        Ipp32u obu_has_size_field;
#endif
        Ipp32u temporal_id;
        Ipp32u spatial_id;
    };

    struct OBUInfo
    {
        OBUHeader header;
        size_t size;
    };

    struct TileGroupInfo
    {
        Ipp32u numTiles;
        Ipp32u startTileIdx;
        Ipp32u endTileIdx;
    };

    class av1_exception
        : public std::runtime_error
    {
    public:

        av1_exception(Ipp32s /*status*/)
            : std::runtime_error("AV1 error")
        {}

        Ipp32s GetStatus() const
        {
            return UMC::UMC_OK;
        }
    };
}

#endif // __UMC_AV1_DEC_DEFS_DEC_H__
#endif // UMC_ENABLE_AV1_VIDEO_DECODER
