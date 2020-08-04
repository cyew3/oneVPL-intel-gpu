// Copyright (c) 2019-2020 Intel Corporation
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

#pragma once

#if !defined(MFX_VA_LINUX)
#include "encoding_ddi.h"
#else
#include "mfx_platform_headers.h"
#endif

typedef struct tagENCODE_CAPS_AV1
{
    union
    {
        struct
        {
            UINT   CodingLimitSet              : 1;
            UINT   ForcedSegmentationSupport   : 1;
            UINT   AutoSegmentationSupport     : 1;
            UINT   BRCReset                    : 1;
            UINT   TemporalLayerRateCtrl       : 3;
            UINT   DynamicScaling              : 1;
            UINT   NumScalablePipesMinus1      : 4;
            UINT   UserMaxFrameSizeSupport     : 1;
            UINT   DirtyRectSupport            : 1;
            UINT   MoveRectSupport             : 1;
            UINT   TileSizeBytesMinus1         : 2;
            UINT   FrameOBUSupport             : 1;
            UINT   SuperResSupport             : 1;
            UINT   CDEFChannelStrengthSupport  : 1;
            UINT                               : 12;
        };
        UINT       CodingLimits;
    };

    union
    {
        struct
        {
            UCHAR   EncodeFunc    : 1; // Enc+Pak
            UCHAR   HybridPakFunc : 1; // Hybrid Pak function
            UCHAR   EncFunc       : 1; // Enc only function
            UCHAR   reserved      : 5; // 0
        };
        UCHAR       CodingFunction;
    };

    union {
        struct {
            UCHAR   SEG_LVL_ALT_Q      : 1;
            UCHAR   SEG_LVL_ALT_LF_Y_V : 1;
            UCHAR   SEG_LVL_ALT_LF_Y_H : 1;
            UCHAR   SEG_LVL_ALT_LF_U   : 1;
            UCHAR   SEG_LVL_ALT_LF_V   : 1;
            UCHAR   SEG_LVL_REF_FRAME  : 1;
            UCHAR   SEG_LVL_SKIP       : 1;
            UCHAR   SEG_LVL_GLOBALMV   : 1;
        };
        UCHAR SegmentFeatureSupport;
    };

    USHORT reserved16b;
    UINT   MaxPicWidth;
    UINT   MaxPicHeight;

    UCHAR  MaxNum_ReferenceL0_P;  // [1..7]
    UCHAR  MaxNum_ReferenceL0_B;  // [1..7]
    UCHAR  MaxNum_ReferenceL1_B;  // [1..7]
    UCHAR  reserved8b;

    USHORT MaxNumOfDirtyRect;
    USHORT MaxNumOfMoveRect;

    union {
        struct {
            UINT   still_picture              : 1;
            UINT   use_128x128_superblock     : 1;
            UINT   enable_filter_intra        : 1;
            UINT   enable_intra_edge_filter   : 1;

            // read_compound_tools
            UINT   enable_interintra_compound : 1;
            UINT   enable_masked_compound     : 1;
            UINT   enable_warped_motion       : 1;
            UINT   allow_screen_content_tools : 1;
            UINT   enable_dual_filter         : 1;
            UINT   enable_order_hint          : 1;
            UINT   enable_jnt_comp            : 1;
            UINT   enable_ref_frame_mvs       : 1;
            UINT   enable_superres            : 1;
            UINT   enable_cdef                : 1;
            UINT   enable_restoration         : 1;
            UINT   allow_intrabc              : 1;
            UINT   ReservedBits               : 16;
        } fields;
        UINT value;
    } AV1ToolSupportFlags;

    union {
        struct {
            UCHAR   i420        : 1;  // support I420
            UCHAR   i422        : 1;  // support I422
            UCHAR   i444        : 1;  // support I444
            UCHAR   mono_chrome : 1;  // support mono
            UCHAR   reserved    : 4;  // [0]
        } fields;
        UINT value;
    } ChromeSupportFlags;

    union {
        struct {
            UCHAR   eight_bits  : 1;  // support 8 bits
            UCHAR   ten_bits    : 1;  // support 10 bits
            UCHAR   twelve_bits : 1;  // support 12 bits
            UCHAR   reserved    : 5;  // [0]
        } fields;
        UINT value;
    } BitDepthSupportFlags;

    union {
        struct {
            UCHAR  EIGHTTAP        : 1;
            UCHAR  EIGHTTAP_SMOOTH : 1;
            UCHAR  EIGHTTAP_SHARP  : 1;
            UCHAR  BILINEAR        : 1;
            UCHAR  SWITCHABLE      : 1;
            UCHAR  reserved        : 3;  // [0]
        } fields;
        UCHAR value;
    } SupportedInterpolationFilters;

    UCHAR  MinSegIdBlockSizeAccepted;

    union {
        struct {
            UINT  CQP           : 1;
            UINT  CBR           : 1;
            UINT  VBR           : 1;
            UINT  AVBR          : 1;
            UINT  ICQ           : 1;
            UINT  VCM           : 1;
            UINT  QVBR          : 1;
            UINT  CQL           : 1;
            UINT  reserved1     : 8; // [0]
            UINT  SlidingWindow : 1;
            UINT  LowDelay      : 1;
            UINT  reserved2     : 14; // [0]
        } fields;
        UINT value;
    } SupportedRateControlMethods;

    UINT   reserved32b[16];

} ENCODE_CAPS_AV1;

#if !defined(MFX_VA_LINUX)
typedef struct tagFRAMERATE
{
    UINT    Numerator;
    UINT    Denominator;
} FRAMERATE;

#if 0
typedef enum tagENCODE_INPUT_COLORSPACE
{
    eColorSpace_P709    = 0,
    eColorSpace_P601    = 1,
    eColorSpace_P2020   = 2
} ENCODE_INPUT_COLORSPACE;
#else
typedef ENCODE_ARGB_COLOR ENCODE_INPUT_COLORSPACE;
#endif

#if 0
typedef enum tagENCODE_FRAMESIZE_TOLERANCE
{
    eFrameSizeTol_Normal        = 0,
    eFrameSizeTol_Low           = 1, // maps to "sliding window"
    eFrameSizeTol_Extremely_Low = 2  // maps to "low delay"
} ENCODE_FRAMESIZE_TOLERANCE;
#else
typedef ENCODE_FRAME_SIZE_TOLERANCE ENCODE_FRAMESIZE_TOLERANCE;
#endif

typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_AV1
{
    UCHAR  seq_profile; // [0]
    UCHAR  seq_level_idx; // [0..23, 31]

    USHORT GopPicSize;

    UCHAR  TargetUsage;
    UCHAR  RateControlMethod;
    USHORT reserved16b;

    UINT   TargetBitRate[8]; // One per temporal layer
    UINT   MaxBitRate;
    UINT   MinBitRate;
    ULONG  InitVBVBufferFullnessInBit;
    ULONG  VBVBufferSizeInBit;
    ULONG  OptimalVBVBufferLevelInBit;
    ULONG  UpperVBVBufferLevelThresholdInBit;
    ULONG  LowerVBVBufferLevelThresholdInBit;

    union
    {
        struct
        {
            UINT bResetBRC            : 1;
            UINT bStillPicture        : 1;
            UINT bUseRawReconRef      : 1;
            UINT DisplayFormatSwizzle : 1; // [0]
            UINT bReserved            : 28;
        } fields;
        UINT value;
    } SeqFlags;

    UINT      UserMaxFrameSize;
    USHORT    reserved2;
    USHORT    reserved3;
    FRAMERATE FrameRate[8]; // One per temporal layer
    UCHAR     NumTemporalLayersMinus1;
    UCHAR     ICQQualityFactor; // [0..255]

    ENCODE_INPUT_COLORSPACE    InputColorSpace;
    ENCODE_SCENARIO            ScenarioInfo;
    ENCODE_CONTENT             ContentInfo;
    ENCODE_FRAMESIZE_TOLERANCE FrameSizeTolerance;

    USHORT SlidingWindowSize;
    UINT   MaxBitRatePerSlidingWindow;
    UINT   MinBitRatePerSlidingWindow;

    union
    {
        struct
        {
            UINT enable_order_hint    : 1;
            UINT enable_superres      : 1;
            UINT enable_cdef          : 1;
            UINT enable_restoration   : 1;
            UINT enable_warped_motion : 1;
            UINT bReserved            : 27;
        } fields;
        UINT value;
    } CodingToolFlags;

    UCHAR order_hint_bits_minus_1; // [0..7]

    UCHAR reserved8b1;
    UCHAR reserved8b2;
    UCHAR reserved8b3;
    UINT  reserved32b[16];
} ENCODE_SET_SEQUENCE_PARAMETERS_AV1;

typedef struct _DXVA_Intel_Seg_AV1
{
    union
    {
        struct
        {
            UCHAR segmentation_enabled : 1;
            UCHAR SegmentNumber        : 4; //[0..8]
            UCHAR update_map           : 1;
            UCHAR temporal_update      : 1;
            UCHAR Reserved             : 1;
        } fields;
        UCHAR value;
    } SegmentFlags;

    SHORT feature_data[8][8];
    UCHAR feature_mask[8];
    UINT ReservedDWs[4];
} DXVA_Intel_Seg_AV1, *LPDXVA_Intel_Seg_AV1;

typedef enum {
    IDENTITY = 0,    // identity transformation, 0-parameter
    TRANSLATION = 1, // translational motion 2-parameter
    ROTZOOM = 2,     // simplified affine with rotation + zoom only, 4-parameter
    AFFINE = 3,      // affine, 6-parameter
    TRANS_TYPES,
} TransformationType;

typedef enum {
    SINGLE_REFERENCE = 0,
    COMPOUND_REFERENCE = 1,
    REFERENCE_MODE_SELECT = 2,
    REFERENCE_MODES = 3,
} REFERENCE_MODE;

typedef struct {
    TransformationType wmtype;
    int32_t wmmat[8];
    int8_t invalid;
} DXVA_Warped_Motion_Params_AV1;

typedef union{
    struct
    {
        UINT search_idx0 : 3;
        UINT search_idx1 : 3;
        UINT search_idx2 : 3;
        UINT search_idx3 : 3;
        UINT search_idx4 : 3;
        UINT search_idx5 : 3;
        UINT search_idx6 : 3;
        UINT ReservedField : 11; //[0]
    } fields;
    UINT value;
} RefFrameCtrl;

typedef struct tagENCODE_SET_PICTURE_PARAMETERS_AV1
{
    USHORT frame_width_minus1;   // [15..2^16-1]
    USHORT frame_height_minus1;  // [15..2^16-1]
    UCHAR  NumTileGroupsMinus1;  // [0..255]
    UCHAR  reserved8b;           // [0]

    UCHAR  CurrOriginalPic;      // [0..127]
    UCHAR  CurrReconstructedPic; // [0..11]
    UCHAR  RefFrameList[8];      // [0..11, 0xFF]
    UCHAR  ref_frame_idx[7];     // [0..7]
    UCHAR  Reserved8b2;

    UCHAR  primary_ref_frame;    // [0..7]
    UCHAR  reserved8b3;
    UCHAR  reserved8b4;

    UCHAR  order_hint;

    RefFrameCtrl  ref_frame_ctrl_l0;
    RefFrameCtrl  ref_frame_ctrl_l1;

    union
    {
        struct
        {
            UINT frame_type                   : 2; // [0..3]
            UINT error_resilient_mode         : 1; // [0..1]
            UINT disable_cdf_update           : 1; // [0..1]
            UINT use_superres                 : 1; // [0..1]
            UINT allow_high_precision_mv      : 1; // [0..1]
            UINT use_ref_frame_mvs            : 1; // [0..1]
            UINT disable_frame_end_update_cdf : 1; // [0..1]
            UINT reduced_tx_set_used          : 1; // [0..1]
            UINT LosslessFlag                 : 1; // [0..1]
            UINT SegIdBlockSize               : 2; // [0..3]
            UINT EnableFrameOBU               : 1;
            UINT DisableFrameRecon            : 1;
            UINT LongTermReference            : 1;
            UINT ReservedField                : 17;
        } fields;
        UINT value;
    } PicFlags;

    // deblocking filter
    UCHAR filter_level[2]; // [0..63]
    UCHAR filter_level_u;  // [0..63]
    UCHAR filter_level_v;  // [0..63]

    union
    {
        struct
        {
            UCHAR sharpness_level        : 3; // [0..7]
            UCHAR mode_ref_delta_enabled : 1;
            UCHAR mode_ref_delta_update  : 1;
            UCHAR ReservedField          : 3; // [0]
        } fields;
        UCHAR value;
    } cLoopFilterInfoFlags;

    UCHAR superres_scale_denominator; // [9..16]
    UCHAR interp_filter;              // [0..9]
    UCHAR reserved8b1;                // [0]

    CHAR ref_deltas[8];  // [-63..63]
    CHAR mode_deltas[2]; // [-63..63]

    // quantization
    USHORT base_qindex;  // [0..255]
    CHAR   y_dc_delta_q; // [-15..15]
    CHAR   u_dc_delta_q; // [-63..63]
    CHAR   u_ac_delta_q; // [-63..63]
    CHAR   v_dc_delta_q; // [-63..63]
    CHAR   v_ac_delta_q; // [-63..63]

    UCHAR  MinBaseQIndex; // [1..255]
    UCHAR  MaxBaseQIndex; // [1..255]
    UCHAR  reserved8b2;   // [0]

    // quantization_matrix
    union {
        struct {
            USHORT using_qmatrix : 1; // not supported for now
            // valid only when using_qmatrix is 1.
            USHORT qm_y          : 4; // [0..15]
            USHORT qm_u          : 4; // [0..15]
            USHORT qm_v          : 4; // [0..15]
            USHORT ReservedField : 3; // [0]
        } fields;
        USHORT  value;
    } wQMatrixFlags;

    USHORT reserved16b1; // [0]

    union
    {
        struct
        {
            // delta_q parameters
            UINT delta_q_present_flag  : 1; // [0..1]
            UINT log2_delta_q_res      : 2; // [0..3]

            // delta_lf parameters
            UINT delta_lf_present_flag : 1; // [0..1]
            UINT log2_delta_lf_res     : 2; // [0..3]
            UINT delta_lf_multi        : 1; // [0..1]

            // read_tx_mode
            UINT tx_mode               : 2; // [2], Only tx_mode = 2 is allowed for VDEnc encoder

            // read_frame_reference_mode
            UINT reference_mode        : 2; // [0..3]
            UINT reduced_tx_set_used   : 1; // [0..1]

            UINT skip_mode_present     : 1; // [0..1]
            UINT ReservedField         : 19;// [0]
        } fields;
        UINT  value;
    } dwModeControlFlags;

    DXVA_Intel_Seg_AV1 stAV1Segments;

    USHORT tile_cols;
    USHORT width_in_sbs_minus_1[63];
    USHORT tile_rows;
    USHORT height_in_sbs_minus_1[63];

    UCHAR context_update_tile_id; // [0..127]

    UCHAR temporal_id;

    // CDEF
    UCHAR cdef_damping_minus_3; // [0..3]
    UCHAR cdef_bits;            // [0..3]
    UCHAR cdef_y_strengths[8];  // [0..63]
    UCHAR cdef_uv_strengths[8]; // [0..63]

    union
    {
        struct
        {
            USHORT yframe_restoration_type  : 2; // [0..3]
            USHORT cbframe_restoration_type : 2; // [0..3]
            USHORT crframe_restoration_type : 2; // [0..3]
            USHORT lr_unit_shift            : 2; // [0..2]
            USHORT lr_uv_shift              : 1; // [0..1]
            USHORT ReservedField            : 7; // [0]
        } fields;
        USHORT value;
    } LoopRestorationFlags;

    // global motion
    DXVA_Warped_Motion_Params_AV1 wm[7];

    UINT   QIndexBitOffset;
    UINT   SegmentationBitOffset;
    UINT   LoopFilterParamsBitOffset;
    UINT   CDEFParamsBitOffset;
    UCHAR  CDEFParamsSizeInBits;
    UCHAR  reserved8bits0;
    USHORT FrameHdrOBUSizeInBits;
    UINT   FrameHdrOBUSizeByteOffset;

    UINT  StatusReportFeedbackNumber;

    // Tile Group OBU header
    union
    {
        struct
        {
            UCHAR obu_extension_flag : 1; // [0..1]
            UCHAR obu_has_size_field : 1; // [0..1]
            UCHAR temporal_id        : 3; // [0..7]
            UCHAR spatial_id         : 2; // [0..2]
            UCHAR ReservedField      : 1; // [0]
        } fields;
        UCHAR value;
    } TileGroupOBUHdrInfo;

    UCHAR reserved8bs1; // [0]
    UCHAR reserved8bs2; // [0]

    // Skip Frames
    UCHAR NumSkipFrames;
    UINT  SkipFramesSizeInBytes;

    USHORT NumDirtyRects;
    ENCODE_RECT *pDirtyRect;
    USHORT NumMoveRects;
    MOVE_RECT *pMoveRect;

    ENCODE_INPUT_TYPE InputType;

    UINT reserved32b[16];

} ENCODE_SET_PICTURE_PARAMETERS_AV1;

typedef struct tagENCODE_SET_TILE_GROUP_HEADER_AV1
{
    UCHAR tg_start; // [0..127]
    UCHAR tg_end; // [0..127]
    USHORT reserved16b;
    UINT reserved32b[9];
} ENCODE_SET_TILE_GROUP_HEADER_AV1;

typedef struct tagENCODE_QUERY_STATUS_PARAMS_AV1
{
    UINT    StatusReportFeedbackNumber;

    UCHAR   CurrOriginalPic;
    UCHAR   field_pic_flag; // not used for AV1, should be 0;
    UCHAR   bStatus;
    UCHAR   reserved0;

    UINT    Func;
    UINT    bitstreamSize;

    CHAR    QpY; // base_qindex;
    CHAR    SuggestedQpYDelta; // not used for AV1
    UCHAR   NumberPasses;
    UCHAR   AverageQp; // not used for AV1

    union
    {
        struct
        {
            UINT PanicMode : 1;
            UINT SliceSizeOverflow : 1; // not used for AV1
            UINT NumSlicesNonCompliant : 1; // not used for AV1
            UINT LongTermReference : 1; // not used for AV1
            UINT FrameSkipped : 1; // not used for AV1
            UINT SceneChangeDetected : 1;
            UINT FrameSizeOverflow : 1; // used for AV1 instead of PanicMode
            UINT ReservedField : 25;
        };
        UINT QueryStatusFlags;
    };

    UINT    MAD;
    USHORT  NumberSlices; // not used for AV1
    USHORT  PSNRx100[3]; // not used for AV1
    USHORT  NextFrameWidthMinus1;
    USHORT  NextFrameHeightMinus1;

    D3DAES_CTR_IV  HWCounterValue;

    UINT    reserved1;
    UINT    reserved2;
    UINT    reserved3;
    UINT    reserved4;

} ENCODE_QUERY_STATUS_PARAMS_AV1;

typedef struct tagENCODE_SET_SLICE_HEADER_AV1
{
    UINT             slice_segment_address;
    UINT             NumLCUsInSlice;
    ENCODE_PICENTRY  RefPicList[2][15];
    UCHAR            num_ref_idx_l0_active_minus1;   // [0..14]
    UCHAR            num_ref_idx_l1_active_minus1;   // [0..14]
    union
    {
        struct
        {
            UINT    bLastSliceOfPic                         : 1;
            UINT    dependent_slice_segment_flag            : 1;
            UINT    slice_temporal_mvp_enable_flag          : 1;
            UINT    slice_type                              : 2;
            UINT    slice_sao_luma_flag                     : 1;
            UINT    slice_sao_chroma_flag                   : 1;
            UINT    mvd_l1_zero_flag                        : 1;
            UINT    cabac_init_flag                         : 1;
            UINT    slice_deblocking_filter_disable_flag    : 1;
            UINT    collocated_from_l0_flag                 : 1;
            UINT    reserved                                : 21;
        };
        UINT    SliceFlags;
    };

    CHAR    slice_qp_delta;
    CHAR    slice_cb_qp_offset;     // [-12..12]
    CHAR    slice_cr_qp_offset;     // [-12..12]
    CHAR    beta_offset_div2;       // [-6..6]
    CHAR    tc_offset_div2;         // [-6..6]
    UCHAR   luma_log2_weight_denom;
    CHAR    delta_chroma_log2_weight_denom;
    CHAR    luma_offset[2][15];
    CHAR    delta_luma_weight[2][15];
    CHAR    chroma_offset[2][15][2];
    CHAR    delta_chroma_weight[2][15][2];
    UCHAR   MaxNumMergeCand;
    USHORT  slice_id;
    USHORT  BitLengthSliceHeaderStartingPortion;
    UINT    SliceHeaderByteOffset;
    UINT    SliceQpDeltaBitOffset;
    UINT    PredWeightTableBitOffset;
    UINT    PredWeightTableBitLength;
    UINT    SliceSAOFlagBitOffset;
} ENCODE_SET_SLICE_HEADER_AV1;

typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_AV1_REXT : ENCODE_SET_SEQUENCE_PARAMETERS_AV1
{
    union
    {
        struct
        {
            UINT    transform_skip_rotation_enabled_flag    : 1;
            UINT    transform_skip_context_enabled_flag     : 1;
            UINT    implicit_rdpcm_enabled_flag             : 1;
            UINT    explicit_rdpcm_enabled_flag             : 1;
            UINT    extended_precision_processing_flag      : 1;
            UINT    intra_smoothing_disabled_flag           : 1;
            UINT    high_precision_offsets_enabled_flag     : 1;
            UINT    persistent_rice_adaptation_enabled_flag : 1;
            UINT    cabac_bypass_alignment_enabled_flag     : 2; // 2 ???
            UINT    ReservedBits : 22;
        }/* fields*/;
        UINT RextEncodeTools;
    } /*RextEncodeTools*/;
} ENCODE_SET_SEQUENCE_PARAMETERS_AV1_REXT;

typedef struct tagENCODE_SET_PICTURE_PARAMETERS_AV1_REXT : ENCODE_SET_PICTURE_PARAMETERS_AV1
{
    union
    {
        struct
        {
            UINT  cross_component_prediction_enabled_flag : 1;
            UINT  chroma_qp_offset_list_enabled_flag      : 1;
            UINT  diff_cu_chroma_qp_offset_depth          : 3;
            UINT  chroma_qp_offset_list_len_minus1        : 3; // [0..5]
            UINT  log2_sao_offset_scale_luma              : 3; // [0..6]
            UINT  log2_sao_offset_scale_chroma            : 3; // [0..6]
            UINT  ReservedBits5 : 18;
        } /*fields*/;
        UINT RangeExtensionPicFlags;
    } /*RangeExtensionPicFlags*/;

    UCHAR   log2_max_transform_skip_block_size_minus2;
    CHAR    cb_qp_offset_list[6]; // [-12..12]
    CHAR    cr_qp_offset_list[6]; // [-12..12]
} ENCODE_SET_PICTURE_PARAMETERS_AV1_REXT;

typedef struct tagENCODE_SET_SLICE_HEADER_AV1_REXT : ENCODE_SET_SLICE_HEADER_AV1
{
    SHORT   luma_offset_l0[15];
    SHORT   ChromaOffsetL0[15][2];
    SHORT   luma_offset_l1[15];
    SHORT   ChromaOffsetL1[15][2];
} ENCODE_SET_SLICE_HEADER_AV1_REXT;

typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_AV1_SCC : ENCODE_SET_SEQUENCE_PARAMETERS_AV1
{
    union
    {
        struct
        {
            UINT  palette_mode_enabled_flag              : 1;
            UINT  motion_vector_resolution_control_idc   : 2;
            UINT  intra_boundary_filtering_disabled_flag : 1;
            UINT  ReservedBits                           : 28;
        } /* fields */;
        UINT  SccEncodeToolsFlags;
    } /* SccEncodeToolsFlags */;

    UCHAR  palette_max_size;                  // [0..64]
    UCHAR  delta_palette_max_predictor_size;  // [0..127]
} ENCODE_SET_SEQUENCE_PARAMETERS_AV1_SCC;

typedef struct tagENCODE_SET_PICTURE_PARAMETERS_AV1_SCC : ENCODE_SET_PICTURE_PARAMETERS_AV1
{
    union
    {
        struct
        {
           UINT  pps_curr_pic_ref_enabled_flag                   : 1;
           UINT  residual_adaptive_colour_transform_enabled_flag : 1;
           UINT  pps_slice_act_qp_offsets_present_flag           : 1;
           UINT  ReservedBits6                                   : 29;
        } /* fields */;
        UINT  ScreenContentExtensionPicFlags;
    } /* ScreenContentExtensionPicFlags */;

    UCHAR   PredictorPaletteSize;             // [0..127]
    USHORT  PredictorPaletteEntries[3][128];
    CHAR    pps_act_y_qp_offset_plus5;        // [-7..17]
    CHAR    pps_act_cb_qp_offset_plus5;       // [-7..17]
    CHAR    pps_act_cr_qp_offset_plus3;       // [-9..15]
} ENCODE_SET_PICTURE_PARAMETERS_AV1_SCC;

typedef struct tagENCODE_SET_QMATRIX_HEVC
{
    UCHAR   bScalingLists4x4[3][2][16]; // 2 inter/intra 3: YUV
    UCHAR   bScalingLists8x8[3][2][64];
    UCHAR   bScalingLists16x16[3][2][64];
    UCHAR   bScalingLists32x32[2][64];
    UCHAR   bScalingListDC[2][3][2];
} ENCODE_SET_QMATRIX_HEVC;

#define D3DDDIFMT_HEVC_BUFFER_CUDATA (D3DFORMAT)183

typedef struct tagENCODE_SET_CUDATA_HEVC
{
    UINT    BufferSize;
    UINT    CUDataOffset;
    BYTE*   pBuffer;
} ENCODE_SET_CUDATA_HEVC;

#endif // #if !defined(MFX_VA_LINUX)