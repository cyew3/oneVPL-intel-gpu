// Copyright (c) 2014-2018 Intel Corporation
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

#ifndef __UMC_AV1_DDI_H
#define __UMC_AV1_DDI_H

#include "umc_av1_dec_defs.h"

#pragma warning(disable: 4201)

//////////////////////////////////////////
//Define the compressed buffer structures
//////////////////////////////////////////
#pragma pack(push, 4)//Note: set struct alignment to 4 Bytes, which is same with driver

namespace UMC_AV1_DECODER
{
#if UMC_AV1_DECODER_REV >= 8500
    #define AV1D_DDI_VERSION 26
    #define DDI_HACKS_FOR_REV_85 // Rev 0.85 uses some DDI changes in comparison with 0.26
                                // such changes are handled by macro DDI_HACKS_FOR_REV_85
#else
    #define AV1D_DDI_VERSION 21
    #define DDI_HACKS_FOR_REV_5 // Rev 0.5 uses some essential DDI changes in comparison with 0.21
                                // such changes are handled by macro DDI_HACKS_FOR_REV_252
#endif

    typedef struct _DXVA_PicEntry_AV1
    {
        union
        {
            struct
            {
                USHORT Index15Bits : 15;
                USHORT AssociatedFlag : 1;
            };
            USHORT wPicEntry;
        };
    } DXVA_PicEntry_AV1, *LPDXVA_PicEntry_AV1;

    typedef struct _segmentation_AV1 {
        union
        {
            struct {
                UCHAR enabled : 1;
                UCHAR update_map : 1;
                UCHAR temporal_update : 1;

                UCHAR update_data : 1;
                UCHAR Reserved4Bits : 4;
            };
            UCHAR wSegmentInfoFlags;
        };
        SHORT feature_data[8][8];
        UCHAR feature_mask[8];
        UINT ReservedDWs[4];
    } DXVA_segmentation_AV1;

    typedef struct _film_grain_params_AV1
    {
        union
        {
            struct
            {
                UINT apply_grain : 1;
                UINT update_grain : 1;
                UINT film_grain_params_ref_idx : 3;
                UINT chroma_scaling_from_luma : 1;
                UINT grain_scaling_minus_8 : 2;
                UINT ar_coeff_lag : 2;
                UINT ar_coeff_shift_minus_6 : 2;
                UINT grain_scale_shift : 2;
                UINT overlap_flag : 1;
                UINT clip_to_restricted_range : 1;
                UINT reservedbits : 16;
            } fields;
            UINT value;
        } dwFilmGrainInfoFlags;

        USHORT    random_seed;
        UCHAR     num_y_points;  // [0..14]
        UCHAR     point_y_value[MAX_POINTS_IN_SCALING_FUNCTION_LUMA];
        UCHAR     point_y_scaling[MAX_POINTS_IN_SCALING_FUNCTION_LUMA];
        UCHAR     num_cb_points;  // [0..10]
        UCHAR     point_cb_value[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];
        UCHAR     point_cb_scaling[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];
        UCHAR     num_cr_points;  // [0..10]
        UCHAR     point_cr_value[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];
        UCHAR     point_cr_scaling[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];
        CHAR      ar_coeffs_y[MAX_AOTOREG_COEFFS_LUMA];   // [-128..127]
        CHAR      ar_coeffs_cb[MAX_AOTOREG_COEFFS_CHROMA];  // [-128..127]
        CHAR      ar_coeffs_cr[MAX_AOTOREG_COEFFS_CHROMA];  // [-128..127]
        UCHAR     cb_mult;
        UCHAR     cb_luma_mult;
        USHORT    cb_offset;  // [0..512]
        UCHAR     cr_mult;
        UCHAR     cr_luma_mult;
        USHORT    cr_offset;  // [0..512]

        UINT ReservedDWs[4];
    } DXVA_Film_Grain_Params_AV1;

    typedef struct _DXVA_Warped_Motion_Params_AV1
    {
        TRANSFORMATION_TYPE wmtype;
        int32_t wmmat[8];
#if AV1D_DDI_VERSION < 26
        int16_t alpha, beta, gamma, delta;
        int8_t invalid;
#endif
    } DXVA_Warped_Motion_Params_AV1;

    typedef struct _DXVA_Intel_PicParams_AV1
    {
        DXVA_PicEntry_AV1 CurrPic;
        DXVA_PicEntry_AV1 CurrDisplayPic;
        UCHAR             profile;        // [0..2]
#if !defined(DDI_HACKS_FOR_REV_85)
        USHORT            Reserved16b;    // [0]
#endif

#if AV1D_DDI_VERSION >= 26
        UCHAR        order_hint_bits_minus_1;
        UCHAR        BitDepthIdx;
        USHORT       reserved16b;
#else
        USHORT max_frame_width_minus_1;   // [0..65535]
        USHORT max_frame_height_minus_1;  // [0..65535]
#endif

        union
        {
            struct
            {
                UINT still_picture : 1;
#if AV1D_DDI_VERSION >= 26
                UINT use_128x128_superblock : 1;
#else
                UINT sb_size_128x128 : 1;
#endif
                UINT enable_filter_intra : 1;
                UINT enable_intra_edge_filter : 1;

                UINT enable_interintra_compound : 1;  // [0..1]
                UINT enable_masked_compound : 1;      // [0..1]

                UINT enable_dual_filter : 1;
                UINT enable_order_hint : 1;
                UINT enable_jnt_comp : 1;
                UINT enable_cdef : 1;
                UINT enable_restoration : 1;
#if AV1D_DDI_VERSION >= 26
                UINT reserved2b : 2;
#else
                UINT BitDepthIdx : 2;
#endif
                UINT mono_chrome : 1;
                UINT color_range : 1;
                UINT subsampling_x : 1;
                UINT subsampling_y : 1;
                UINT chroma_sample_position : 1;
                UINT film_grain_params_present : 1;
#if AV1D_DDI_VERSION >= 26
                UINT large_scale_tile : 1;
                UINT ReservedSeqInfoBits : 12;
#elif defined(DDI_HACKS_FOR_REV_5)
                UINT order_hint_bits_minus1 : 3;
                UINT ReservedSeqInfoBits : 10;
#else
                UINT ReservedSeqInfoBits : 13;
#endif
            } fields;
            UINT value;
        } dwSeqInfoFlags;

        // frame info
        union {
            struct {
                UINT frame_type : 2;
                UINT show_frame : 1;
                UINT showable_frame : 1;
                UINT error_resilient_mode : 1;
                UINT disable_cdf_update : 1;
                UINT allow_screen_content_tools : 1;  // [0..1]
                UINT force_integer_mv : 1;            // [0..1]
                UINT allow_intrabc : 1;
                UINT use_superres : 1;
                UINT allow_high_precision_mv : 1;
                UINT is_motion_mode_switchable : 1;
                UINT use_ref_frame_mvs : 1;
                UINT disable_frame_end_update_cdf : 1;
                UINT uniform_tile_spacing_flag : 1;
                UINT allow_warped_motion : 1;
#if AV1D_DDI_VERSION >= 26
                UINT ReservedPicInfoBits : 16;
#else
                UINT refresh_frame_context : 1;
                UINT large_scale_tile : 1;
                UINT ReservedPicInfoBits : 14;
#endif
            } fields;
            UINT value;
        } dwPicInfoFlags;

        USHORT frame_width_minus1;   // [0..65535]
        USHORT frame_height_minus1;  // [0..65535]

        DXVA_PicEntry_AV1 ref_frame_map[8];
        UCHAR ref_frame_idx[7];   // [0..7]
        UCHAR primary_ref_frame;  // [0..7]

        UCHAR order_hint;
#if AV1D_DDI_VERSION >= 26
        USHORT output_frame_width_in_tiles_minus_1;// [0..65535]
        USHORT output_frame_height_in_tiles_minus_1;// [0..65535]

        UINT reserved32b2;
#else
        UCHAR ref_order_hint[8];  // may be removed.
#endif

        UCHAR superres_scale_denominator;  // [9..16]
        UCHAR frame_interp_filter;               // [0..9]

        UCHAR filter_level[2];  // [0..63]
        UCHAR filter_level_u;   // [0..63]
        UCHAR filter_level_v;   // [0..63]
        union
        {
            struct
            {
                UCHAR sharpness_level : 3;  // [0..7]
                UCHAR mode_ref_delta_enabled : 1;
                UCHAR mode_ref_delta_update : 1;
                UCHAR ReservedField : 3;  // [0]
            } fields;
            UCHAR value;
        } cLoopFilterInfoFlags;

        CHAR ref_deltas[8];   // [-63..63]
        CHAR mode_deltas[2];  // [-63..63]

                              // quantization
        USHORT base_qindex;  // [0..255]
        CHAR y_dc_delta_q;// [-63..63]
        CHAR u_dc_delta_q;   // [-63..63]
        CHAR u_ac_delta_q;   // [-63..63]
        CHAR v_dc_delta_q;   // [-63..63]
        CHAR v_ac_delta_q;   // [-63..63]
                             // quantization_matrix
        union
        {
            struct
            {
                USHORT using_qmatrix : 1;
                USHORT qm_y : 4;           // [0..15]
                USHORT qm_u : 4;           // [0..15]
                USHORT qm_v : 4;           // [0..15]
                USHORT ReservedField : 3;  // [0]
            } fields;
            USHORT value;
        } wQMatrixFlags;

        union
        {
            struct
            {
                UINT delta_q_present_flag : 1;    // [0..1]
                UINT log2_delta_q_res : 2;    // [0..3]
                UINT delta_lf_present_flag : 1;    // [0..1]
                UINT log2_delta_lf_res : 2;    // [0..3]
                UINT delta_lf_multi : 1;    // [0..1]
                UINT tx_mode : 2;    // [0..3]
                UINT reference_mode : 2;    // [0..3]  will be replaced by reference_select
                UINT reduced_tx_set_used : 1;    // [0..1]
                UINT skip_mode_present : 1;    // [0..1]
                UINT ReservedField : 19;   // [0]
            } fields;
            UINT value;
        } dwModeControlFlags;

        DXVA_segmentation_AV1 stAV1Segments;

#if AV1D_DDI_VERSION < 26
        UINT tg_size_bit_offset;
        UCHAR log2_tile_cols;  // [0..6]
        UCHAR log2_tile_rows;  // [0..6]
#endif

        USHORT tile_cols;
        USHORT width_in_sbs_minus_1[64];
        USHORT tile_rows;
#if AV1D_DDI_VERSION >= 26
        USHORT height_in_sbs_minus_1[64];

        USHORT tile_count_minus_1;
#else
        USHORT height_in_sbs_minus_1[128];

        USHORT number_of_tiles_in_list_minus_1;

        UCHAR tile_col_size_bytes;  // [1..4] to be removed.
#endif
        USHORT context_update_tile_id;

        // CDEF
        UCHAR cdef_damping_minus_3;   // [0..3]
        UCHAR cdef_bits;              // [0..3]
        UCHAR cdef_y_strengths[8];    // [0..63]
        UCHAR cdef_uv_strengths[8];

        union
        {
            struct
            {
                USHORT yframe_restoration_type : 2;    // [0..3]
                USHORT cbframe_restoration_type : 2;    // [0..3]
                USHORT crframe_restoration_type : 2;    // [0..3]
                USHORT lr_unit_shift : 2;    // [0..2]
                USHORT lr_uv_shift : 1;    // [0..1]
                USHORT ReservedField : 7;    // [0]
            } fields;
            USHORT value;
        } LoopRestorationFlags;

        // global motion
        DXVA_Warped_Motion_Params_AV1 wm[7];

        DXVA_Film_Grain_Params_AV1 stAV1FilmGrainParams;
#ifdef DDI_HACKS_FOR_REV_5
        UCHAR UncompressedHeaderLengthInBytes;  // [0..255]
        UINT BSBytesInBuffer;
#endif
        UINT StatusReportFeedbackNumber;

        UINT ReservedDWs[16];
    } DXVA_Intel_PicParams_AV1, *LPDXVA_Intel_PicParams_AV1;

    typedef struct _DXVA_Intel_Tile_AV1
    {
        UINT      BSTileDataLocation;
        UINT      BSTileBytesInBuffer;
        USHORT    wBadBSBufferChopping;
        USHORT    tile_row;
        USHORT    tile_column;
#if AV1D_DDI_VERSION >= 26
        USHORT TileIndex;
        USHORT reserved16b;
#endif
        USHORT    StartTileIdx;
        USHORT    EndTileIdx;
        DXVA_PicEntry_AV1 anchor_frame_idx;
        UINT      BSTilePayloadSizeInBytes;
#if AV1D_DDI_VERSION >= 26
        UINT      Reserved[3];
#else
        UINT      Reserved[4];
#endif
    } DXVA_Intel_Tile_AV1, *LPDXVA_Intel_Tile_AV1;

    typedef struct _DXVA_Intel_Status_AV1 {
        USHORT                    StatusReportFeedbackNumber;
        DXVA_PicEntry_AV1         current_picture;
        UCHAR                     bBufType;
        UCHAR                     bStatus;
        UCHAR                     bReserved8Bits;
        USHORT                    wNumMbsAffected;
    } DXVA_Intel_Status_AV1, *LPDXVA_Intel_Status_AV1;
}

#pragma pack(pop)

#endif // __UMC_AV1_DDI_H
