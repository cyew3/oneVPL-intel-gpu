/*
 * Copyright (c) 2021 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL INTEL AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file va_private_enc_av1.h
 * \brief AV1 encoding API
 *
 * This file contains the \ref api_enc_av1 "AV1 encoding API".
 *
 */

#include<stdint.h>

#ifndef VA_ENC_AV1_H
#define VA_ENC_AV1_H

#define VAConfigAttribEncAV1      50
#define VAConfigAttribEncAV1Ext1  51
#define VAConfigAttribEncAV1Ext2  52

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup api_enc_AV1 AV1 encoding API
 *
 * @{
 */

/** \brief Attribute value for VAConfigAttribEncAV1. */
typedef union _VAConfigAttribValEncAV1 {
    struct {
        /**
         * \brief A flag indicates whether 128x128 supper block supported,
         * 0 if 128x128 supper block  not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t use_128x128_superblock        : 2;
        /**
         * \brief  A flag indicates whether filter intra is supported.
         * 0 if intra filter not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_filter_intra           : 2;
        /**
         * \brief  A flag indicates whether intra edge filter is supported.
         * 0 if intra edge filter not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_intra_edge_filter      : 2;
        /**
         * \brief  A flag indicates whether interintra compound is supported.
         * 0 if interintra compound not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_interintra_compound    : 2;
        /**
         * \brief  A flag indicates whether masked compound is supported.
         * 0 if masked compound not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_masked_compound        : 2;
        /**
         * \brief  A flag indicates whether warped motion is supported.
         * 0 if warped motion not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_warped_motion          : 2;
        /**
         * \brief  A flag indicates whether screen content tools are supported.
         * 0 if screen content tools not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_screen_content_tools   : 2;
        /**
         * \brief  A flag indicates whether dual filter is supporedt.
         * 0 if dual filter not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_dual_filter            : 2;
        /**
         * \brief  A flag indicates whether jnt comp is supported.
         * 0 if jnt comp not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_jnt_comp               : 2;
        /**
         * \brief  A flag indicates whether ref frame mvs is supported.
         * 0 if ref frames mvs not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_ref_frame_mvs          : 2;
        /**
         * \brief  A flag indicates whether super resolution is supported.
         * 0 if superres not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_superres               : 2;
        /**
         * \brief  A flag indicates whether restoration is supported.
         * 0 if restorationnot supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t enable_restoration            : 2;
        /**
         * \brief  A flag indicates whether intrac is supported.
         * 0 if intrac not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t allow_intrabc                 : 2;
        /**
         * \brief  A flag indicates whether cdef channel strength is supported.
         * 0 if cdef not supported, 1 if supported.
         * 2 if device does not support not enabling this feature.
         */
        uint32_t cdef_channel_strength_support : 2;
        /** \brief Reserved bits for future, must be zero. */
        uint32_t reserved                      : 4;
    } bits;
    uint32_t value;
} VAConfigAttribValEncAV1;

/** \brief Attribute value for VAConfigAttribEncAV1Ext1. */
typedef union _VAConfigAttribValEncAV1Ext1 {
    struct {
        /**
         * \brief Fields indicate which types of interpolation filter are supported.
         * (interp_filter_type & 0x01) == 1: eight_tap filter is supported, 0: not.
         * (interp_filter_type & 0x02) == 1: eight_tap_smooth filter is supported, 0: not.
         * (interp_filter_type & 0x04) == 1: eight_sharp filter is supported, 0: not.
         * (interp_filter_type & 0x08) == 1: bilinear filter is supported, 0: not.
         * (interp_filter_type & 0x10) == 1: switchable filter is supported, 0: not.
         */
        uint32_t interp_filter_type            : 5;
        /**
         * \brief Min segmentId block size accepted.
         */
        uint32_t min_segid_block_size_accepted : 8;
        /**
         * \brief Type of segment feature support.
         * (segment_feature_support & 0x01) == 1: SEG_LVL_ALT_Q is supported, 0: not.
         * (segment_feature_support & 0x02) == 1: SEG_LVL_ALT_LF_Y_V is supported, 0: not.
         * (segment_feature_support & 0x04) == 1: SEG_LVL_ALT_LF_Y_H is supported, 0: not.
         * (segment_feature_support & 0x08) == 1: SEG_LVL_ALT_LF_U is supported, 0: not.
         * (segment_feature_support & 0x10) == 1: SEG_LVL_ALT_LF_V is supported, 0: not.
         * (segment_feature_support & 0x20) == 1: SEG_LVL_REF_FRAME is supported, 0: not.
         * (segment_feature_support & 0x40) == 1: SEG_LVL_SKIP is supported, 0: not.
         * (segment_feature_support & 0x80) == 1: SEG_LVL_GLOBALMV is supported, 0: not.
         */
        uint32_t segment_feature_support       : 8;
        /** \brief Reserved bits for future, must be zero. */
        uint32_t reserved                      : 11;
    } bits;
    uint32_t value;
} VAConfigAttribValEncAV1Ext1;

/** \brief Attribute value for VAConfigAttribEncAV1Ext2. */
typedef union _VAConfigAttribValEncAV1Ext2 {
    struct {
        /**
        * \brief Tile size bytes minus1.
        * Specify the number of bytes needed to code tile size.
        * This value will be set in frame header obu.
        */
        uint32_t tile_size_bytes_minus1        : 2;
        /**
         * \brief tx_mode supported.
         */
        uint32_t tx_mode_support               : 2;
        /**
         * \brief Max tile rows minus1.
         * Specify the max number of tile in row supported by driver.
         */
        uint32_t max_tile_rows_minus1          : 6;
        /**
         * \brief Max tile cols minus1.
         * Specify the max number of tile in col supported by driver.
         */
        uint32_t max_tile_cols_minus1          : 6;
        /**
         * \brief Max tile num minus1.
         * Specify the max number of tile supported bt driver.
         */
        uint32_t max_tile_num_minus1           : 12;
        /** \brief Reserved bits for future, must be zero. */
        uint32_t reserved                      : 4;
    } bits;
    uint32_t value;
} VAConfigAttribValEncAV1Ext2;

/**
 * \brief Packed header types specific to AV1 encoding.
 *
 * Types of packed headers generally used for AV1 encoding.
 *
 */
typedef enum {
    /**
     * \brief Packed Sequence Parameter Set (SPS).
     *
     * The corresponding packed header data buffer shall contain the
     * complete sequence_header_obu() syntax element.
     *
     */
    VAEncPackedHeaderAV1_SPS = VAEncPackedHeaderSequence,
    /**
     * \brief Packed Picture Parameter Set (PPS).
     *
     * The corresponding packed header data buffer shall contain the
     * complete frame_header_obu() syntax element.
     *
     */
    VAEncPackedHeaderAV1_PPS = VAEncPackedHeaderPicture,
} VAEncPackedHeaderTypeAV1;

/**
 * \brief AV1 Encoding Sequence Parameter Buffer Structure
 *
 * This structure conveys sequence level parameters.
 *
 */
typedef struct  _VAEncSequenceParameterBufferAV1 {
    /** \brief AV1 profile setting.
     *  value range [0..2].
     */
    uint8_t     seq_profile;

    /** \brief Level Setting of current operation point.
     *  value range [0..23].
     */
    uint8_t     seq_level_idx;

    /** \brief Tier Setting of current operation point.
     *  value range [0..1].
     */
    uint8_t     seq_tier;

    uint8_t     reserved8b;

    /** \brief Period between intro_only frames. */
    uint32_t    intra_period;

    /** \brief Period between I/P frames.
     *  For hierarchical structure, this is the anchor frame distance. */
    uint32_t    ip_period;

    /* \brief RC related fields. RC modes are set with VAConfigAttribRateControl. */
    /* For AV1, CBR implies HRD conformance and VBR implies no HRD conformance. */

    /**
     * \brief Initial bitrate set for this sequence in CBR or VBR modes.
     *
     * This field represents the initial bitrate value for this
     * sequence if CBR or VBR mode is used, i.e. if the encoder
     * pipeline was created with a #VAConfigAttribRateControl
     * attribute set to either \ref VA_RC_CBR or \ref VA_RC_VBR.
     *
     * The bitrate can be modified later on through
     * #VAEncMiscParameterRateControl buffers.
     */
    uint32_t    bits_per_second;

    union {
        struct {
            /** \brief Still picture encoding, no inter frame referencing. */
            uint32_t    still_picture                               : 1;
            /** \brief Force using 128x128 or 64x64 Supper block */
            uint32_t    use_128x128_superblock                      : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_filter_intra                         : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_intra_edge_filter                    : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_interintra_compound                  : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_masked_compound                      : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_warped_motion                        : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_dual_filter                          : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_order_hint                           : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_jnt_comp                             : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_ref_frame_mvs                        : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_superres                             : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_cdef                                 : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    enable_restoration                          : 1;
            /** \brief Reserved bytes for future use, must be zero. */
            uint32_t    reserved_bits                               : 18;
        } bits;
        uint32_t value;
    } seq_fields;

    /** \brief Corresponds to AV1 syntax element of the same name.
     *  value range [0..7].
     */
    uint8_t     order_hint_bits_minus_1;

    /** \brief Reserved bytes for future use, must be zero */
    uint32_t                va_reserved[VA_PADDING_HIGH];
} VAEncSequenceParameterBufferAV1;

#define VA_AV1_MAXNUM_SEGMENTS                  8
#define VA_AV1_MAXNUM_SEGFEATURES               8

/**
 * \brief Segment parameters
 */
typedef struct _VAEncSegParamAV1 {
    union {
        struct {
            /** \brief Indicates if segmentation is enabled in the current frame.
             *  If disabled, all the below parameters in the structure should
             *  be set to 0, and ignored by driver.
             */
            uint8_t     segmentation_enabled            : 1;

            /** \brief when segmentation_enabled equals 1 and segment_number > 0,
             *  this parameter equals 1 indicates the segmentation map may
             *  come from DDI, and that the Segmentation Map Data Buffer
             *  should be provided with populated segment_id. If equals 0,
             *  segmentation map should be inherited from a reference frame
             *  (specified by primary_ref_frame). When segmentation_enabled or
             *  SegmentNumber equals 0, this parameter should be set to 0
             *  and ignored by driver.
             */
            uint8_t     segmentation_update_map         : 1;
            uint8_t     segmentation_temporal_update    : 1;
            /** \brief Reserved bytes for future use, must be zero. */
            uint8_t     reserved                        : 5;

        } bits;
        uint8_t value;
    } seg_flags;

    /** \brief If segmentation_enabled equals 1, this parameter indicates
     *  the number of segments conveyed through VAAPI. In this case,
     *  if segment_number equals 0, it forces the underline encoder
     *  to determine how many segments would be created as well as
     *  the segmentation map to be generated. Also the underline
     *  encoder shall write the segmentation_params() syntax in the
     *  uncompressed header at segmentation_bitoffset (back-annotation).
     *  In VAAPI, the rest parameters in this structure should be all
     *  set to 0 and ignored by driver. And app should NOT send the
     *  "Segment map data buffer". In packed uncompressed header
     *  bitstream, app should write syntax element segmentation_enabled
     *  as 0 and segmentation_params() should be only 1-bit-long.
     *  If segment_number > 0, and segmentation_update_map = 1, app should provide
     *  the "Segment map data buffer" and populate the rest of the
     *  current data structure. And that underline encoder would honor
     *  the segmentation parameters feature_data[0..segment_number-1][]
     *  and feature_mask[0..segment_number-1], etc.
     *  Value range [0..8].
     */
    uint8_t      segment_number;

    /** \brief segment parameters.
     *  feature_data[][] is equivalent to variable FeatureData[][] in spec,
     *  which is after clip3() operation.
     *  Clip3(x, y, z) = (z<x)? x : ((z > y)? y : z);
     *  The limit is defined in Segmentation_Feature_Max[ SEG_LVL_MAX ] = {
     *  255, MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER,
     *  MAX_LOOP_FILTER, 7, 0, 0 }
     */
    int16_t     feature_data[VA_AV1_MAXNUM_SEGMENTS][VA_AV1_MAXNUM_SEGFEATURES];

    /** \brief Bit field to indicate each feature is enabled or not per
     *  segment_id. Each bit is the feature_id.
     */
    uint8_t     feature_mask[VA_AV1_MAXNUM_SEGMENTS];

    /** \brief Reserved bytes for future use, must be zero. */
    uint32_t                va_reserved[VA_PADDING_LOW];
} VAEncSegParamAV1;

/**
 * \brief Segment map data buffer
 * This buffer is optional depending on the value of av1_segments.segmentation_enabled.
 * If av1_segments.segmentation_enabled in the picture parameters equals 1,
 * and RateControlMethod is not CQP and this surface is not provided by App,
 * the encoder will determine the per block segmentation map. In this case,
 * App should not provide the segmentation parameter data structure
 * in frame header as well. If av1_segments.segmentation_enabled equals 1
 * and the segmentation map buffer is provided, app should embed the
 * segmentation info in frame header, populate the VAEncSegParamAV1 structure with
 * #VAEncMacroblockMapBufferType and the driver as well as the underline encoder
 * should honor what is given by the app.
 */
typedef struct _VAEncSegMapBufferAV1 {
    /** \brief Segment map data size */
    uint32_t    segmentMapDataSize;

    /**
     * \brief Segment map
     * Size of this map is indicated by \ref segmentMapDataSize and each element
	 * in this map contains the segment id of a particular block.
     * The element is indexed by raster scan order.
     * The value of each entry should be in the range [0..7], inclusive.
     */
    uint8_t    *pSegmentMap;
} VAEncSegMapBufferAV1;

typedef enum {
    /** \brief Identity transformation, 0-parameter */
    VAAV1EncTransformationIdentity           = 0,
    /** \brief Translational motion, 2-parameter */
    VAAV1EncTransformationTranslation        = 1,
    /** \brief Simplified affine with rotation + zoom only, 4-parameter */
    VAAV1EncTransformationRotzoom            = 2,
    /** \brief Affine, 6-parameter */
    VAAV1EncTransformationAffine             = 3,
    /** \brief Transformation count */
    VAAV1EncTransformationCount
} VAEncTransformationTypeAV1;

typedef struct _VAEncWarpedMotionParamsAV1{

    /** \brief Specify the type of warped motion */
    VAEncTransformationTypeAV1  wmtype;

    /** \brief Specify warp motion parameters
     *  wm.wmmat[] corresponds to gm_params[][] in spec.
     *  Details in AV1 spec section 5.9.24 or refer to libaom code
     *  https://aomedia.googlesource.com/aom/+/refs/heads/master/av1/decoder/decodeframe.c
     */
    int32_t                 wmmat[8];

    /** \brief Valid or invalid on affine set */
    uint8_t  invalid;

    /** \brief Reserved bytes for future use, must be zero. */
    uint32_t                va_reserved[VA_PADDING_LOW];

} VAEncWarpedMotionParamsAV1;

/**
 * \brief Reference frame control
 * Suggest which frames to be used as reference.
 * search_idx#: index into ref_frame_idx[] to indicate that frame will be included
 * in the reference list if value in range [1..7]. Invalid when value is 0.
 * The order of the search_idx# indicates the preferred search order.
 */
typedef union {
	struct
	{
        /**
         * \brief Specifies preferred search order of reference frame
         * which comes from indexing ref_frame_idx[] by 0.
         */
        uint32_t search_idx0 : 3;
        /**
         * \brief Specifies preferred search order of reference frame
         * which comes from indexing ref_frame_idx[] by 1.
         */
        uint32_t search_idx1 : 3;
        /**
         * \brief Specifies preferred search order of reference frame
         * which comes from indexing ref_frame_idx[] by 2.
         */
        uint32_t search_idx2 : 3;
        /**
         * \brief Specifies preferred search order of reference frame
         * which comes from indexing ref_frame_idx[] by 3.
         */
        uint32_t search_idx3 : 3;
        /**
         * \brief Specifies preferred search order of reference frame
         * which comes from indexing ref_frame_idx[] by 4.
         */
        uint32_t search_idx4 : 3;
        /**
         * \brief Specifies preferred search order of reference frame
         * which comes from indexing ref_frame_idx[] by 5.
         */
        uint32_t search_idx5 : 3;
        /**
         * \brief Specifies preferred search order of reference frame
         * which comes from indexing ref_frame_idx[] by 6.
         */
        uint32_t search_idx6 : 3;

        /** \brief Reserved bytes for future use, must be zero. */
        uint32_t Reserved    : 11;
	} fields;
	uint32_t value;
} VARefFrameCtrlAV1;

/**
 * \brief AV1 Encoding Picture Parameter Buffer Structure
 *
 * This structure conveys picture level parameters.
 *
 */
typedef struct  _VAEncPictureParameterBufferAV1
{
    /** \brief AV1 encoder may support SupRes and dynamic scaling function.
     *  For SupRes, underline encoder is responsible to do downscaling.
     *  For dynamic scaling, app should provide the scaled raw source.
     */
    /** \brief Raw source frame width in pixels */
    uint16_t    frame_width_minus_1;
    /** \brief Raw source frame height in pixels */
    uint16_t    frame_height_minus_1;

    /** \brief Surface to store reconstructed frame, not used for enc only case */
    VASurfaceID reconstructed_frame;

    /** \brief Buffer to store coded data */
    VABufferID  coded_buf;

    /** \brief Reference frame buffers
     *  Each entry of the array specifies the surface index of the picture
     *  that is referred by current picture or will be referred by any future
     *  picture. The valid entries take value from 0 to 127, inclusive.
     *  Non-valid entries, those do not point to pictures which are referred
     *  by current picture or future pictures, should take value 0xFF.
     *  Other values are not allowed.
     *
     *  Application should update this array based on the refreshing
     *  information expected.
     */
    VASurfaceID reference_frames[8];

    /** \brief Reference index list
     *  Contains a list of indices into refernce_frames[].
     *  Indice with refernce frames range: [LAST_FRAME - LAST_FRAME,
     *  LAST2_FRAME - LAST_FRAME, ..., ALTREF2_FRAME - LAST_FRAME].
     *  #define LAST_FRAME 1
     *  #define LAST2_FRAME 2
     *  #define LAST3_FRAME 3
     *  #define GOLDEN_FRAME 4
     *  #define BWDREF_FRAME 5
     *  #define ALTREF_FRAME 6
     *  #define ALTREF2_FRAME 7
     *  value range [0..7].
     */
    uint8_t     ref_frame_idx[7];

    uint8_t     reserved8bits0;

    /** \brief primary reference frame
     *  Index into reference_frames[]
     *  segment id map, context table, etc. come from the reference
     *  frame pointed by this index.
     *  value range [0..7].
     */
    uint8_t     primary_ref_frame;

    /** \brief Corresponds to AV1 syntax element of the same name. */
    uint8_t     order_hint;

    /** \brief Suggest which frames to be used as reference
     *  see struct #VARefFrameCtrl for details.
     */
    VARefFrameCtrlAV1     ref_frame_ctrl_l0;
    VARefFrameCtrlAV1     ref_frame_ctrl_l1;

    union {
        struct {
            /** \brief frame type
             *  0:  key_frame
             *  1:  inter_frame
             *  2:  intra_only frame
             *  3:  switch_frame (app needs to set error_resilient_mode=1,
             *      refresh_frame_flags, etc approperately.)
             */
            uint32_t    frame_type                      : 2;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    error_resilient_mode            : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    disable_cdf_update              : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    use_superres                    : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    allow_high_precision_mv         : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    use_ref_frame_mvs               : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    disable_frame_end_update_cdf    : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    reduced_tx_set                  : 1;

            /** \brief For single tile group, app may choose to use one frame obu
             *  to replace one frame header obu + one tile group obu.
             *  Invalid if num_tile_groups_minus1 > 0.
             */
            uint32_t    enable_frame_obu                : 1;

            /** \brief Indicate the current frame will be used as a long term reference. */
            uint32_t    long_term_reference             : 1;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint32_t    allow_intrabc                   : 1;
            /** \brief Reserved bytes for future use, must be zero. */
            uint32_t    reserved                        : 20;
        } bits;
        uint32_t value;
    } picture_flags;

    /** \brief Block size for each Segment ID in Segment Map.
     *  0: 16x16 block size, default value;
     *  1: 32x32 block size;
     *  2: 64x64 block size;
     *  3: 8x8 block size.
     */
    uint8_t     seg_id_block_size;

    /** \brief Number of tile groups minus 1.
     *  value range [0..255].
     */
    uint8_t     num_tile_groups_minus1;

    /** \brief Temporal id of the frame.*/
    uint8_t     temporal_id;

    /** \brief Deblock filter parameters.
     *  value range [0..63].
     */
    uint8_t     filter_level[2];
    uint8_t     filter_level_u;
    uint8_t     filter_level_v;

    union {
        struct {
            /* \brief Sharpness level for deblock filter.
             * value range [0..7].
             */
            uint8_t     sharpness_level                 : 3;
            uint8_t     mode_ref_delta_enabled          : 1;
            uint8_t     mode_ref_delta_update           : 1;
            /** \brief Reserved bytes for future use, must be zero. */
            uint8_t     reserved                        : 3;
        } bits;
        uint8_t    value;
    } loop_filter_flags;

    /** \brief Super resolution scale denominator.
     *  value range [9..16].
     */
    uint8_t     superres_scale_denominator;
    /** \brief Corresponds to AV1 syntax element of the same name. */
    uint8_t     interpolation_filter;

    /** \brief Loop filter ref deltas.
     *  value range [-63..63].
     */
    int8_t      ref_deltas[8];

    /** \brief Loop filter mode deltas.
     *  value range [-63..63].
     */
    int8_t      mode_deltas[2];

    /** \brief Quantization params*/
    uint8_t     base_qindex;
    int8_t      y_dc_delta_q;
    int8_t      u_dc_delta_q;
    int8_t      u_ac_delta_q;
    int8_t      v_dc_delta_q;
    int8_t      v_ac_delta_q;

    /** \brief Min value for base q index for BRC.
     *  value range [1..255].
     */
    uint8_t     min_base_qindex;

    /** \brief Max value for base q index for BRC.
     *  value range [1..255].
     */
    uint8_t     max_base_qindex;

    /** \brief Quantization matrix. */
    union {
        struct {
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint16_t    using_qmatrix                   : 1;
            /* \brief Following parameters only valid when using_qmatrix == 1. */
            uint16_t    qm_y                            : 4;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint16_t    qm_u                            : 4;
            /** \brief Corresponds to AV1 syntax element of the same name. */
            uint16_t    qm_v                            : 4;
            /** \brief Reserved bytes for future use, must be zero. */
            uint16_t    reserved                        : 3;
        } bits;
        uint16_t    value;
    } qmatrix_flags;

    uint16_t reserved16bits0;

    union {
        struct {
            /** \brief Specify whether quantizer index delta values are present.
             *  value range [0..1]*/
            uint32_t    delta_q_present                 : 1;
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..3]*/
            uint32_t    delta_q_res                     : 2;

            /** \brief Specify whether loop filter delta values are present.
             *  value range [0..1]*/
            uint32_t    delta_lf_present                : 1;
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..3]*/
            uint32_t    delta_lf_res                    : 2;
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..1]*/
            uint32_t    delta_lf_multi                  : 1;

            /** \brief Corresponds to AV1 syntax element of the same name.
             *  0: ONLY_4X4;
             *  1: TX_MODE_LARGEST;
             *  2: TX_MODE_SELECT;
             *  3: Invalid.
             */
            uint32_t    tx_mode                         : 2;

            /** \brief Indicates whether to use single or compound reference prediction.
             *  0: SINGLE_REFERENCE;
             *  1: COMPOUND_REFERENCE;
             *  2: REFERENCE_MODE_SELECT;
             *  3: REFERENCE_MODES.
             */
            uint32_t    reference_mode                  : 2;
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..1].
             */
            uint32_t    reduced_tx_set                  : 1;
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..1].
             */
            uint32_t    skip_mode_present               : 1;
            /** \brief Reserved bytes for future use, must be zero. */
            uint32_t    reserved                        : 19;
        } bits;
        uint32_t    value;
    } mode_control_flags;

    /** \brief Segmentation parameters */
    VAEncSegParamAV1    segments;

    /** \brief Number of tile columns. */
    uint8_t     tile_cols;
    /** \brief Number of tile rows. */
    uint8_t     tile_rows;

    /** \brief The last tile column or row size needs to be derived. */
    uint16_t    width_in_sbs_minus_1[63];
    uint16_t    height_in_sbs_minus_1[63];

    /** \brief specify which tile to use for the CDF update.
     *  value range [0..127]*/
    uint16_t     context_update_tile_id;

    /** \brief Corresponds to AV1 syntax element of the same name.
     *  value range [0..3].
     */
    uint8_t     cdef_damping_minus_3;
    /** \brief Corresponds to AV1 syntax element of the same name.
     *  value range [0..3].
     */
    uint8_t     cdef_bits;
    /** \brief CDEF Y strengths.
     *  value range [0..63]*/
    uint8_t     cdef_y_strengths[8];
    /** \brief CDEF UV strengths.
     *  value range [0..63]*/
    uint8_t     cdef_uv_strengths[8];

    union {
        struct {
            /** \brief Restoration type for Y frame.
             *  value range [0..3].
             */
            uint16_t    yframe_restoration_type         : 2;
            /** \brief Restoration type for Cb frame.
             *  value range [0..3].
             */
            uint16_t    cbframe_restoration_type        : 2;
            /** \brief Restoration type for Cr frame.
             *  value range [0..3].
             */
            uint16_t    crframe_restoration_type        : 2;
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..2].
             */
            uint16_t    lr_unit_shift                   : 2;
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..1].
             */
            uint16_t    lr_uv_shift                     : 1;
            /** \brief Reserved bytes for future use, must be zero. */
            uint16_t    reserved                        : 7;
        } bits;
        uint16_t    value;
    } loop_restoration_flags;

    /** \brief Global motion. */
    VAEncWarpedMotionParamsAV1    wm[7];

    /**
     *  \brief Offset from starting position of output bitstream in bits where
     *  Base Qindex should be inserted.
     */
    uint32_t    bit_offset_qindex;
    /**
     *  \brief Offset from starting position in bits where segmentation_enabled
     *  is located in bitstream. When segmentation is generated by
     *  encoder, GPU uses this offset to locate and update the
     *  segmentation related information.
     */
    uint32_t    bit_offset_segmentation;
    /**
     *  \brief Offset in bits for syntax loop_filter_params( ) in packed frame
     *  header bit stream from the start of the packed header data.
     *  Valid only in BRC mode. In CQP mode, this parameter should
     *  be set to 0 and ignored by driver.
     */
    uint32_t    bit_offset_loopfilter_params;
    /**
     *  \brief In BRC mode, underline encoder should generate the approperiate
     *  CDEF values and write back into uncompressed header. And app
     *  should provide default CDEF values in packed header. This parameter
     *  should point to the starting bit of cdef_params() syntax structure
     *  in packed header.
     *  In CQP mode, this parameter should be set to 0 and ignored by driver.
     */
    uint32_t    bit_offset_cdef_params;
    /**
     *  \brief In BRC mode, this parameter indicates the actual bit usage of
     *  cdef_params() syntax structure in packed uncompressed header.
     *  In CQP mode, this parameter should be set to 0 and ignored by driver.
     */
    uint32_t    size_in_bits_cdef_params;

    /**
     *  \brief Offset in bytes for syntax obu_size of frame header OBU in packed
     *  frame header bit stream. The frame header OBU size depends on the
     *  encoded tile sizes. It applies to both Frame Header OBU and Frame
     *  OBU if obu_size needs to be updated by underline encoder.
     *  Otherwise, app can set it to 0 and ignored by driver.
     */
    uint32_t    byte_offset_frame_hdr_obu_size;

    /**
	 * \brief Frame header OBU bit stream size in bits. The frame header obu packed bit
	 * stream contains an obu header, a 4-byte long obu_size field, frame_header_obu()
	 * syntax chain, and a trailing bit if not inside a frame obu. If EnableFrameOBU == 1,
	 * the value should include and up to the last bit of frame_header_obu() and
	 * excluding the bits generated by byte_alignment(). If EnableFrameOBU == 0,
	 * the value should include and up to the trailing bit at the end of the frame
	 * header obu. The size will be used by encoder to calculate the final frame
	 * header size after bit shifting due to auto segmentation.
     */
    uint32_t    size_in_bits_frame_hdr_obu;

    /** \brief Tile Group OBU header */
    union {
        struct {
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..1].
             */
            uint8_t     obu_extension_flag              : 1;
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..1].
             */
            uint8_t     obu_has_size_field              : 1;
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..7].
             */
            uint8_t     temporal_id                     : 3;
            /** \brief Corresponds to AV1 syntax element of the same name.
             *  value range [0..2].
             */
            uint8_t     spatial_id                      : 2;
            /** \brief Reserved bytes for future use, must be zero. */
            uint8_t     reserved                        : 1;
        } bits;
        uint8_t     value;
    } tile_group_obu_hdr_info;

    uint8_t     reserved8bits1;

    /** \brief Reserved bytes for future use, must be zero */
    uint32_t    va_reserved[VA_PADDING_HIGH];
} VAEncPictureParameterBufferAV1;


typedef struct _VAEncTileGroupBufferAV1 {
    /** \brief Tile group start location.
     *  The position of the first tile in current tile group
     *  in raster scan order across the frame
     *  value range [0..127].
     */
    uint8_t  tg_start;
    /** \brief Tile group end location.
     *  The position of the last tile in current tile group
     *  in raster scan order across the frame.
     *  value range [0..127].
     */
    uint8_t  tg_end;

    /** \brief Reserved bytes for future use, must be zero. */
    uint32_t                va_reserved[VA_PADDING_LOW];
} VAEncTileGroupBufferAV1;

/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* VA_ENC_AV1_H */
