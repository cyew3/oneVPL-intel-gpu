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

#if UMC_AV1_DECODER_REV == 2520
    #define AV1D_DDI_VERSION 18
    #define DDI_HACKS_FOR_REV_252 // Rev 0.25.2 uses some minor DDI changes in comparison with 0.18
                                  // such changes are handled by macro DDI_HACKS_FOR_REV_252
#elif UMC_AV1_DECODER_REV == 5000 // Rev 0.5
    // TODO: [Rev0.5] change to proper DDI version
    #define AV1D_DDI_VERSION 18
#endif

typedef struct _segmentation_AV1 {
        union {
            struct {
                UCHAR   enabled         : 1;
                UCHAR   update_map      : 1;
                UCHAR   temporal_update : 1;
                UCHAR   update_data     : 1;
                UCHAR   Reserved4Bits   : 3;
            };
            UCHAR wSegmentInfoFlags;
        };
#ifdef DDI_HACKS_FOR_REV_252
        SHORT feature_data[8][7];
#else
        SHORT feature_data[8][8];
#endif
        UCHAR feature_mask[8];

        // CONFIG_Q_SEGMENTATION
        UCHAR q_lvls; // [0..255]
        SHORT q_delta[8]; // [-255..255]
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
            UINT    monochrome : 1;
            UINT    allow_intrabc : 1;
            UINT    seq_force_integer_mv : 2;	// [0..2]
            UINT    ReservedFormatInfo2Bits : 7;
        } fields;
        UINT value;
    } dwFormatAndPictureInfoFlags;

    USHORT    max_frame_width_minus1;     // [0..65535]
    USHORT    max_frame_height_minus1;    // [0..65535]

    USHORT  frame_width_minus1;         // [0..65535]
    USHORT  frame_height_minus1;        // [0..65535]
    UCHAR   superres_scale_denominator; // [9..16]

#ifdef DDI_HACKS_FOR_REV_252
    UCHAR       BitDepthMinus8;          // [0, 2, 4]
#else
    UCHAR       bit_depth;               // [8, 10, 12]
#endif
    UCHAR       frame_interp_filter;              // [0..9]

    DXVA_PicEntry_AV1   ref_frame_map[8];
    //UCHAR       num_ref_idx_active;         // [0..6]
    DXVA_PicEntry_AV1   ref_frame_idx[7];       //
    UCHAR       ref_frame_sign_bias[8];         // [0..1]

    USHORT  current_frame_id;
    USHORT  ref_frame_id[8];
    // UCHAR        refresh_frame_flags;

    // deblocking filter
    UCHAR       filter_level[2];            // [0..63]
    UCHAR       filter_level_u;             // [0..63]
    UCHAR       filter_level_v;             // [0..63]

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
    CHAR        u_dc_delta_q;   // [-63..63]
    CHAR        u_ac_delta_q;   // [-63..63]
    CHAR        v_dc_delta_q;   // [-63..63]
    CHAR        v_ac_delta_q;   // [-63..63]

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

            // CONFIG_LOOPFILTER_LEVEL
            UINT    delta_lf_multi : 1;  	// [0..1]

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

            // tiles
            UINT    uniform_tile_spacing_flag   : 1;    // [0..1]
            UINT    dependent_horz_tiles    : 1;    // [0..1]
            UINT    loop_filter_across_tiles_enabled    : 1;    // [0..1]

            // screen content
            UINT    allow_screen_content_tools  : 1;    // [0..1]

            UINT    skip_mode_present : 1;      // [0..1]
            UINT    ReservedField     : 3;    // [0]
       }    fields;
        UINT  value;
    } dwModeControlFlags;

    DXVA_segmentation_AV1   stAV1Segments;

    UINT                   tg_size_bit_offset;

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
            USHORT  lr_unit_shift : 2;  // [0..2]
            USHORT  lr_uv_shift : 1;    // [0..1]
            USHORT  ReservedField : 7;  // [0]
       }    fields;
        USHORT  value;
    } LoopRestorationFlags;

    // global motion
    UCHAR   gm_type[7];     // [0..3]
    INT     gm_params[7][6];

    UCHAR       UncompressedHeaderLengthInBytes;// [0..255]
    UINT        BSBytesInBuffer;
    UINT        StatusReportFeedbackNumber;
} DXVA_Intel_PicParams_AV1, *LPDXVA_Intel_PicParams_AV1;

//tile group control data buffer
typedef struct _DXVA_Intel_BitStream_AV1_Short
{
    UINT        BSOBUDataLocation;
    UINT        BitStreamBytesInBuffer;
    USHORT      wBadBSBufferChopping;
} DXVA_Intel_BitStream_AV1_Short, *LPDXVA_Intel_BitStream_AV1_Short;

typedef struct _DXVA_Intel_Status_AV1 {
    USHORT                    StatusReportFeedbackNumber;
    DXVA_PicEntry_AV1         current_picture;
    UCHAR                     bBufType;
    UCHAR                     bStatus;
    UCHAR                     bReserved8Bits;
    USHORT                    wNumMbsAffected;
} DXVA_Intel_Status_AV1, *LPDXVA_Intel_Status_AV1;

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
