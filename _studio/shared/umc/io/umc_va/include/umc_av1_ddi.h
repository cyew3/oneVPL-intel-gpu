//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_AV1_DDI_H
#define __UMC_AV1_DDI_H

#pragma warning(disable: 4201)

//////////////////////////////////////////
//Define the compressed buffer structures
//////////////////////////////////////////
#pragma pack(push, 4)//Note: set struct alignment to 4 Bytes, which is same with driver

//AV1 compressed buffer structures
// picture params buffer
typedef struct _DXVA_PicEntry_AV1
{
    union
    {
        struct
        {
            UCHAR       Index7Bits : 7;
            UCHAR       AssociatedFlag : 1;
        };
        UCHAR           bPicEntry;
    };
} DXVA_PicEntry_AV1, *PDXVA_PicEntry_AV1;

#if UMC_AV1_DECODER_REV == 0
#define AV1D_DDI_VERSION 8
#elif UMC_AV1_DECODER_REV == 251
#define AV1D_DDI_VERSION 15
#endif

#if AV1D_DDI_VERSION == 8
// DDI version 0.08
typedef struct _segmentation_AV1 {
    union {
        struct {
            UCHAR   enabled : 1;
            UCHAR   update_map : 1;
            UCHAR   temporal_update : 1;
            UCHAR   abs_delta : 1;
            UCHAR   Reserved4Bits : 4;
        };
        UCHAR wSegmentInfoFlags;
    };
    //UCHAR tree_probs[7];
    //UCHAR pred_probs[3];
    SHORT feature_data[8][4];
    UCHAR feature_mask[8];
} DXVA_segmentation_AV1;

// picture params buffer
typedef struct _DXVA_Intel_PicParams_AV1
{
    DXVA_PicEntry_AV1     CurrPic;
    UCHAR                 profile;                // [0..3]

    union {
        struct {
            USHORT     frame_type : 1;
            USHORT     show_frame : 1;
            USHORT     error_resilient_mode : 1;
            USHORT     subsampling_x : 1;
            USHORT     subsampling_y : 1;    // [0]
            USHORT     extra_plane : 1;
            USHORT     refresh_frame_context : 1;
            USHORT     frame_parallel_decoding_mode : 1;
            USHORT     intra_only : 1;
            USHORT     frame_context_idx : 3;
            USHORT     reset_frame_context : 2;// [0, 1, 2]//TODO: wait for Ce's update
            USHORT     allow_high_precision_mv : 1;
            USHORT     sb_size_128x128 : 1;
            //USHORT     ReservedFormatInfo2Bits       : 2;//TODO: wait for Ce's update
        } fields;
        USHORT value;
    } dwFormatAndPictureInfoFlags;

    USHORT               frame_width_minus1;            // [0..65535]
    USHORT               frame_height_minus1;           // [0..65535]

    UCHAR                BitDepthMinus8;                // [0, 2, 4]
    UCHAR                frame_interp_filter;           // [0..7]

    DXVA_PicEntry_AV1    ref_frame_map[8];
    UCHAR                num_ref_idx_active;            // [0..6]
    DXVA_PicEntry_AV1    ref_frame_idx[6];              //
    UCHAR                ref_frame_sign_bias[7];        // [0..1]
    USHORT               ref_frame_width_minus1[6];     // [0..65535]
    USHORT               ref_frame_height_minus1[6];    // [0..65535]
    UCHAR                found_ref[6];                  // [0..1]

    // deblocking filter
    UCHAR                filter_level;                  // [0..63]
    UCHAR                sharpness_level;               // [0..7]
    union
    {
        struct
        {
            UCHAR     mode_ref_delta_enabled : 1;      // [1]
            UCHAR     mode_ref_delta_update : 1;      // [1]
            UCHAR     use_prev_frame_mvs : 1;      // [1]
            UCHAR     ReservedField : 5;      // [0]
        }    fields;
        UCHAR    value;
    } wControlInfoFlags;

    CHAR        ref_deltas[7];              // [-63..63]
    CHAR        mode_deltas[2];             // [-63..63]

    // quantization
    USHORT      base_qindex;                 // [0..255]
    CHAR        y_dc_delta_q;                // [-63..63]
    CHAR        uv_dc_delta_q;               // [-63..63]
    CHAR        uv_ac_delta_q;               // [-63..63]

    union
    {
        struct
        {
            // quantization_matrix
            UINT     min_qmlevel : 4;      // [0..15]
            UINT     max_qmlevel : 4;      // [0..15]

            // CDEF
            UINT     dering_damping : 1;     // [0..1]
            UINT     clpf_damping : 1;     // [0..1]
            UINT     nb_cdef_strengths : 2;     // [0..3]

            // ext_delta_q
            UINT     delta_q_present_flag : 1;      // [0..1]
            UINT     log2_delta_q_res : 2;      // [0..3]
            UINT     delta_lf_present_flag : 1;      // [0..1]
            UINT     log2_delta_lf_res : 2;      // [0..3]

            // read_tx_mode
            //UINT     tx_mode_select                         : 1;      // [0..1]
            //UINT     frame_tx_mode                          : 2;      // [0..3]
            UINT     tx_mode : 3;      // [0..6]

            // read_frame_reference_mode
            UINT     reference_mode : 2;      // [0..3]

            // read_compound_tools
            UINT     allow_interintra_compound : 1;      // [0..1]
            UINT     allow_masked_compound : 1;      // [0..1]

            UINT     reduced_tx_set_used : 1;      // [0..1]

            // tiles
            UINT     dependent_horz_tiles : 1;     // [0..1]
            UINT     loop_filter_across_tiles_enabled : 1;      // [0..1]

            // screen content
            UINT     allow_screen_content_tools : 1;     // [0..1]

            UINT     ReservedField : 4;      // [0]
        }    fields;
        UINT  value;
    } dwModeControlFlags;

    DXVA_segmentation_AV1 stAV1Segments;
#if 1//DDI v0.08
    UCHAR log2_tile_cols;         // [0..6]
    UCHAR log2_tile_rows;         // [0..6]
#else//TODO: DDI v0.07. keep it here, wait and see future update
    USHORT tile_width;
    USHORT  tile_height;
#endif
    // USHORT    TileWidthInSBMinus1[63];
    // USHORT    TileHeightInSBMinus1[63];
    UCHAR tile_col_size_bytes;            // [1..4]
    UCHAR tile_size_bytes;                // [1..4]

    // CDEF
    //UCHAR     cdef_strengths[4];            // [0..127]
    //UCHAR     cdef_uv_strengths[4];        // [0..127]
    UCHAR     cdef_strengths[16];          // [0..127]//TODO: changed to align with AOM. Wait for Ce's update
    UCHAR     cdef_uv_strengths[16];       // [0..127]//TODO: changed to align with AOM. Wait for Ce's update

    union
    {
        struct
        {
            USHORT     yframe_restore_type : 2;     // [0..3]
            USHORT     cbframe_restore_type : 2;    // [0..3]
            USHORT     crframe_restore_type : 2;    // [0..3]
            USHORT     restore_tilesize : 2;        // [0..3]
            USHORT     ReservedField : 8;           // [0]
        }    fields;
        USHORT    value;
    } LoopRestorationFlags;

    UCHAR       UncompressedHeaderLengthInBytes;    // [0..255]
    USHORT      compressed_header_size;             // [0..65536]

    UCHAR       mb_segment_tree_probs[7];
    UCHAR       segment_pred_probs[3];

    UINT        BSBytesInBuffer;
    UINT        StatusReportFeedbackNumber;
} DXVA_Intel_PicParams_AV1, *LPDXVA_Intel_PicParams_AV1;

#elif AV1D_DDI_VERSION > 10
typedef struct _segmentation_AV1 {
        union {
            struct {
                UCHAR   enabled         : 1;
                UCHAR   update_map      : 1;
                UCHAR   temporal_update : 1;
                UCHAR   abs_delta       : 1;
                UCHAR   update_data     : 1;
                UCHAR   Reserved4Bits   : 3;
            };
            UCHAR wSegmentInfoFlags;
        };
        SHORT feature_data[8][4];
        UCHAR feature_mask[8];
    } DXVA_segmentation_AV1;

typedef struct _DXVA_Intel_PicParams_AV1
{
    DXVA_PicEntry_AV1   CurrPic;
    UCHAR       profile;                // [0..3]
    USHORT      Reserved16b;                // [0]
    union {
        struct {
            UINT    frame_type              : 1;
            UINT    show_frame              : 1;
            UINT    error_resilient_mode        : 1;
            UINT    subsampling_x           : 1;
            UINT    subsampling_y           : 1;
            UINT    extra_plane                 : 1;    // [0]
            UINT    refresh_frame_context       : 1;
            UINT    frame_parallel_decoding_mode    : 1;
            UINT    intra_only              : 1;
            UINT    frame_context_idx           : 3;
            UINT    allow_high_precision_mv         : 1;
            UINT    sb_size_128x128             : 1;
            UINT    large_scale_tile            : 1;
            UINT    frame_id_numbers_present_flag   : 1;
            UINT    use_reference_buffer            : 1;
            UINT    cur_frame_mv_precision_level    : 2;
            UINT    reset_frame_context         : 2;    // [0..2]
            UINT    ReservedFormatInfo2Bits         : 11;
        } fields;
        UINT value;
    } dwFormatAndPictureInfoFlags;

    USHORT  frame_width_minus1;         // [0..65535]
    USHORT  frame_height_minus1;        // [0..65535]

    UCHAR       BitDepthMinus8;            // [0, 2, 4]
    UCHAR       frame_interp_filter;              // [0..9]

    DXVA_PicEntry_AV1   ref_frame_map[8];
    //UCHAR       num_ref_idx_active;         // [0..6]
    DXVA_PicEntry_AV1   ref_frame_idx[7];       //
    UCHAR       ref_frame_sign_bias[8];         // [0..1]
#if AV1D_DDI_VERSION <= 14
    USHORT  ref_frame_width_minus1[7];  // [0..65535]
    USHORT  ref_frame_height_minus1[7]; // [0..65535]
#endif
    USHORT  current_frame_id;
    USHORT  ref_frame_id[8];
    // UCHAR        refresh_frame_flags;

    // deblocking filter
#if 1
    // driver still uses single filter level even after switch to DDI 0.12 - let's mimic this behavior so far
    // TODO: change once driver support for different filter levels will be added
    UCHAR       filter_level;                  // [0..63]
#else
    UCHAR       filter_level[2];            // [0..63]
    UCHAR       filter_level_u;             // [0..63]
    UCHAR       filter_level_v;             // [0..63]
#endif
    UCHAR       sharpness_level;            // [0..7]
    union
    {
        struct
        {
            UCHAR   mode_ref_delta_enabled      : 1;    // [1]
            UCHAR   mode_ref_delta_update       : 1;    // [1]
            UCHAR   use_prev_frame_mvs      : 1;    // [1]
           UCHAR    ReservedField           : 5;    // [0]
       }    fields;
        UCHAR   value;
    } wControlInfoFlags;

    CHAR        ref_deltas[8];          // [-63..63]
    CHAR        mode_deltas[2];             // [-63..63]

    // quantization
    USHORT  base_qindex;                // [0..255]
    CHAR        y_dc_delta_q;           // [-63..63]
    CHAR        uv_dc_delta_q;          // [-63..63]
    CHAR        uv_ac_delta_q;          // [-63..63]

    union
    {
        struct
        {
            // quantization_matrix
            UINT    using_qmatrix       : 1;    // [0..1]
            UINT    min_qmlevel         : 4;    // [0..15]
            UINT    max_qmlevel         : 4;    // [0..15]

             // ext_delta_q
            UINT    delta_q_present_flag        : 1;    // [0..1]
            UINT    log2_delta_q_res        : 2;    // [0..3]
            UINT    delta_lf_present_flag       : 1;    // [0..1]
            UINT    log2_delta_lf_res       : 2;    // [0..3]

            // read_tx_mode
            //UINT  tx_mode_select      : 1;    // [0..1]
            //UINT  frame_tx_mode       : 2;    // [0..3]
            UINT    tx_mode         : 3;    // [0..6]

            // read_frame_reference_mode
            UINT    reference_mode      : 2;    // [0..3]

            // read_compound_tools
            UINT    allow_interintra_compound   : 1;    // [0..1]
            UINT    allow_masked_compound   : 1;    // [0..1]

            UINT    reduced_tx_set_used     : 1;    // [0..1]

            // global motion
            UINT    transformation_type     : 3;    // [0..7]

            // tiles
            UINT    uniform_tile_spacing_flag   : 1;    // [0..1]
            UINT    dependent_horz_tiles    : 1;    // [0..1]
            UINT    loop_filter_across_tiles_enabled    : 1;    // [0..1]

            // screen content
            UINT    allow_screen_content_tools  : 1;    // [0..1]

            UINT    ReservedField       : 2;    // [0]
       }    fields;
        UINT  value;
    } dwModeControlFlags;

    DXVA_segmentation_AV1   stAV1Segments;

    UCHAR       log2_tile_cols;         // [0..6]
    UCHAR       log2_tile_rows;         // [0..6]

    USHORT  tile_cols;
    USHORT  tile_col_start_sb[64];
    USHORT  tile_rows;
    USHORT  tile_row_start_sb[64];

    UCHAR       tile_col_size_bytes;            // [1..4]
    UCHAR       tile_size_bytes;            // [1..4]

    // CDEF
    UCHAR       cdef_pri_damping;           // [3..6]
    UCHAR       cdef_sec_damping;           // [3..6]
    UCHAR       cdef_bits;              // [0..3]
    UCHAR       cdef_strengths[8];          // [0..127]
    UCHAR       cdef_uv_strengths[8];       // [0..127]

    union
    {
        struct
        {
            USHORT  yframe_restoration_type : 2;    // [0..3]
            USHORT  cbframe_restoration_type    : 2;    // [0..3]
            USHORT  crframe_restoration_type    : 2;    // [0..3]
            USHORT  restoration_tilesize_shift_0    : 2;    // [0..2]
            USHORT  restoration_tilesize_shift_1    : 2;    // [0..3]
            USHORT      ReservedField       : 6;    // [0]
       }    fields;
        USHORT  value;
    } LoopRestorationFlags;

    UCHAR       UncompressedHeaderLengthInBytes;// [0..255]
    UINT        BSBytesInBuffer;
    UINT        StatusReportFeedbackNumber;
} DXVA_Intel_PicParams_AV1, *LPDXVA_Intel_PicParams_AV1;
#endif

//tile group control data buffer
#if AV1D_DDI_VERSION >= 15
typedef struct _DXVA_Intel_Tile_Group_AV1_Short
{
    UINT        BSOBUDataLocation;
    UINT        TileGroupBytesInBuffer;
    USHORT      wBadTileGroupChopping;
    UINT        StartTileIdx;
    UINT        EndTileIdx;
    UINT        NumTilesInBuffer;
} DXVA_Intel_Tile_Group_AV1_Short, *LPDXVA_Intel_Tile_Group_AV1_Short;
#else
typedef struct _DXVA_Intel_Tile_Group_AV1_Short
{
    UINT        BSNALunitDataLocation;
    UINT        TileGroupBytesInBuffer;
    USHORT      wBadTileGroupChopping;
} DXVA_Intel_Tile_Group_AV1_Short, *LPDXVA_Intel_Tile_Group_AV1_Short;
#endif

//quantization matrix data buffer
typedef struct _THREE_QM_FACTORS
{
    union
    {
        struct
        {
            UINT        qm_factor0 : 9;
            UINT        reserved0 : 1;
            UINT        qm_factor1 : 9;
            UINT        reserved1 : 1;
            UINT        qm_factor2 : 9;
            UINT        reserved2 : 3;
        }   fields;
        UINT    value;
    } ThreeFactors;
} THREE_QM_FACTORS;

typedef struct _DXVA_Intel_Qmatrix_AV1
{
    THREE_QM_FACTORS    factors[1024];
} DXVA_Intel_Qmatrix_AV1, *LPDXVA_Intel_Qmatrix_AV1;

#pragma pack(pop)

#endif // __UMC_AV1_DDI_H
