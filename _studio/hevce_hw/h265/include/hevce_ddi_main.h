//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//
#pragma once

#include "encoding_ddi.h"

#define HEVCE_DDI_VERSION 966

typedef struct tagENCODE_CAPS_HEVC
{
    union{
        struct {
            UINT    CodingLimitSet          : 1;
            UINT    BitDepth8Only           : 1;
            UINT    Color420Only            : 1;
            UINT    SliceStructure          : 3;
            UINT    SliceIPOnly             : 1;
            UINT    SliceIPBOnly            : 1;
            UINT    NoWeightedPred          : 1;
            UINT    NoMinorMVs              : 1;
            UINT    RawReconRefToggle       : 1;
            UINT    NoInterlacedField       : 1;
            UINT    BRCReset                : 1;
            UINT    RollingIntraRefresh     : 1;
            UINT    UserMaxFrameSizeSupport : 1;
            UINT    FrameLevelRateCtrl      : 1;
            UINT    SliceByteSizeCtrl       : 1;
            UINT    VCMBitRateControl       : 1;
            UINT    ParallelBRC             : 1;
            UINT    TileSupport             : 1;
            UINT    SkipFrame               : 1;
            UINT    MbQpDataSupport         : 1;
            UINT    SliceLevelWeightedPred  : 1;
            UINT    LumaWeightedPred        : 1;
            UINT    ChromaWeightedPred      : 1;
            UINT    QVBRBRCSupport          : 1;
            UINT    HMEOffsetSupport        : 1;
            UINT    YUV422ReconSupport      : 1;
            UINT    YUV444ReconSupport      : 1;
            UINT    RGBReconSupport         : 1;
            UINT    MaxEncodedBitDepth      : 2;
        };
        UINT    CodingLimits;
    };

    UINT    MaxPicWidth;
    UINT    MaxPicHeight;
    UCHAR   MaxNum_Reference0;
    UCHAR   MaxNum_Reference1;
    UCHAR   MBBRCSupport;
    UCHAR   TUSupport;

    union {
        struct {
            UCHAR    MaxNumOfROI                : 5; // [0..16]    
            UCHAR    ROIBRCPriorityLevelSupport : 1;
            UCHAR    BlockSize                  : 2;
        };
        UCHAR    ROICaps;
    };

    union {
        struct {
            UINT    SliceLevelReportSupport         : 1;
            UINT    MaxNumOfTileColumnsMinus1       : 4;
            UINT    IntraRefreshBlockUnitSize       : 2;
            UINT    LCUSizeSupported                : 3;
            UINT    MaxNumDeltaQP                   : 4;
            UINT    DirtyRectSupport                : 1;
            UINT    MoveRectSupport                 : 1;
            UINT    FrameSizeToleranceSupport       : 1;
            UINT    HWCounterAutoIncrementSupport   : 2;
            UINT    ROIDeltaQPSupport               : 1;
            UINT                                    : 12; // For future expansion
        };
        UINT    CodingLimits2;
    };

    UCHAR    MaxNum_WeightedPredL0;
    UCHAR    MaxNum_WeightedPredL1;
} ENCODE_CAPS_HEVC;

#if !defined(OPEN_SOURCE)
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

typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_HEVC
{
    USHORT  wFrameWidthInMinCbMinus1;
    USHORT  wFrameHeightInMinCbMinus1;
    UCHAR   general_profile_idc;        // [1..4]
    UCHAR   general_level_idc;
    UCHAR   general_tier_flag;          // [0..1]

    USHORT  GopPicSize;
    UCHAR   GopRefDist;
    UCHAR   GopOptFlag  : 2;
    UCHAR               : 6;

    UCHAR       TargetUsage;
    UCHAR       RateControlMethod;
    UINT        TargetBitRate;
    UINT        MaxBitRate;
    UINT        MinBitRate;
    FRAMERATE   FrameRate;
    ULONG       InitVBVBufferFullnessInBit;
    ULONG       VBVBufferSizeInBit;

    union
    {
        struct
        {
            UINT    bResetBRC               : 1;
            UINT    GlobalSearch            : 2;
            UINT    LocalSearch             : 4;
            UINT    EarlySkip               : 2;
            UINT    MBBRC                   : 4;
            UINT    ParallelBRC             : 1;
            UINT    SliceSizeControl        : 1;
            UINT    SourceFormat            : 2;
            UINT    SourceBitDepth          : 2;
            UINT    QpAdjustment            : 1;
            UINT    ROIValueInDeltaQP       : 1;
            UINT    BlockQPforNonRectROI    : 1;
            UINT    ReservedBits            : 10;
        };
        UINT    EncodeFlags;
    };

    UINT    UserMaxFrameSize;
    USHORT  Reserved1;          // AVBRAccuracy;
    USHORT  Reserved2;          // AVBRConvergence;
    UCHAR   ICQQualityFactor;   // [1..51]
    UINT    NumOfBInGop[3];

    union
    {
        struct
        {
            UINT    scaling_list_enable_flag            : 1;
            UINT    sps_temporal_mvp_enable_flag        : 1;
            UINT    strong_intra_smoothing_enable_flag  : 1;
            UINT    amp_enabled_flag                    : 1;
            UINT    SAO_enabled_flag                    : 1;
            UINT    pcm_enabled_flag                    : 1;
            UINT    pcm_loop_filter_disable_flag        : 1;
            UINT    tiles_fixed_structure_flag          : 1;
            UINT    chroma_format_idc                   : 2;
            UINT    separate_colour_plane_flag          : 1;
            UINT    ReservedBits1                       : 21;
        };
        UINT    EncodeTools;
    };

    UCHAR   log2_max_coding_block_size_minus3;
    UCHAR   log2_min_coding_block_size_minus3;
    UCHAR   log2_max_transform_block_size_minus2;
    UCHAR   log2_min_transform_block_size_minus2;
    UCHAR   max_transform_hierarchy_depth_intra;
    UCHAR   max_transform_hierarchy_depth_inter;
    UCHAR   log2_min_PCM_cb_size_minus3;
    UCHAR   log2_max_PCM_cb_size_minus3;
    UCHAR   bit_depth_luma_minus8;
    UCHAR   bit_depth_chroma_minus8;
    UCHAR   pcm_sample_bit_depth_luma_minus1;
    UCHAR   pcm_sample_bit_depth_chroma_minus1;

    ENCODE_INPUT_COLORSPACE     InputColorSpace;
    ENCODE_SCENARIO             ScenarioInfo;
    ENCODE_CONTENT              ContentInfo;
    ENCODE_FRAMESIZE_TOLERANCE  FrameSizeTolerance;
} ENCODE_SET_SEQUENCE_PARAMETERS_HEVC;

typedef struct tagENCODE_SET_PICTURE_PARAMETERS_HEVC
{
    ENCODE_PICENTRY  CurrOriginalPic;
    ENCODE_PICENTRY  CurrReconstructedPic;
    UCHAR            CollocatedRefPicIndex;    // [0..14, 0xFF]
    ENCODE_PICENTRY  RefFrameList[15];
    INT              CurrPicOrderCnt;
    INT              RefFramePOCList[15];

    UCHAR   CodingType;
    USHORT  NumSlices;

    union
    {
        struct
        {
            UINT    tiles_enabled_flag                      : 1;
            UINT    entropy_coding_sync_enabled_flag        : 1;
            UINT    sign_data_hiding_flag                   : 1;
            UINT    constrained_intra_pred_flag             : 1;
            UINT    transform_skip_enabled_flag             : 1;
            UINT    transquant_bypass_enabled_flag          : 1;
            UINT    cu_qp_delta_enabled_flag                : 1;
            UINT    weighted_pred_flag                      : 1;
            UINT    weighted_bipred_flag                    : 1;
            //  loop filter flags
            UINT    loop_filter_across_slices_flag          : 1;
            UINT    loop_filter_across_tiles_flag           : 1;
            UINT    scaling_list_data_present_flag          : 1;
            UINT    dependent_slice_segments_enabled_flag   : 1;
            UINT    bLastPicInSeq                           : 1;
            UINT    bLastPicInStream                        : 1;
            UINT    bUseRawPicForRef                        : 1;
            UINT    bEmulationByteInsertion                 : 1;
            UINT    BRCPrecision                            : 2;
            UINT    bEnableSliceLevelReport                 : 1;    // [0..1]
            UINT    bEnableRollingIntraRefresh              : 2;    // [0..2]
            UINT    no_output_of_prior_pics_flag            : 1;    // [0..1]
            UINT    bEnableGPUWeightedPrediction            : 1;    // [0..1]
            UINT    DisplayFormatSwizzle                    : 1;
            UINT    reservedbits                            : 7;
        };
        UINT    PicFlags;
    };

    CHAR    QpY;
    UCHAR   diff_cu_qp_delta_depth;
    CHAR    pps_cb_qp_offset;                       // [-12..12]
    CHAR    pps_cr_qp_offset;                       // [-12..12]
    UCHAR   num_tile_columns_minus1;
    UCHAR   num_tile_rows_minus1;
    USHORT  tile_column_width[19];
    USHORT  tile_row_height[21];
    UCHAR   log2_parallel_merge_level_minus2;
    UCHAR   num_ref_idx_l0_default_active_minus1;   // [0..14]
    UCHAR   num_ref_idx_l1_default_active_minus1;   // [0..14]
    USHORT  LcuMaxBitsizeAllowed;
    USHORT  IntraInsertionLocation;
    USHORT  IntraInsertionSize;
    CHAR    QpDeltaForInsertedIntra;                // [-8..7]
    UINT    StatusReportFeedbackNumber;

    // following parameters are valid for CNL+ only
    UCHAR   slice_pic_parameter_set_id; // [0..63]
    UCHAR   nal_unit_type;              // [0..63]

    UINT            MaxSliceSizeInBytes;

    UCHAR       NumROI;                     // [0..16]
    ENCODE_ROI  ROI[16];
    CHAR        MaxDeltaQp;                 // [-51..51]
    CHAR        MinDeltaQp;                 // [-51..51]
    UCHAR       NumDeltaQpForNonRectROI;    // [0..15]
    CHAR        NonRectROIDeltaQpList[16];

    // Skip Frames
    UCHAR   SkipFrameFlag;
    UCHAR   NumSkipFrames;
    UINT    SizeSkipFrames;

    // Max/Min QP settings for BRC
    UCHAR   BRCMaxQp;
    UCHAR   BRCMinQp;

    // HME Offset
    UCHAR   bEnableHMEOffset;
    SHORT   HMEOffset[15][2];

    USHORT      NumDirtyRects;
    ENCODE_RECT *pDirtyRect;
    USHORT      NumMoveRects;
    MOVE_RECT   *pMoveRect;

#if defined(OPEN_SOURCE)
    UINT reserved;
#else
    ENCODE_INPUT_TYPE   InputType;
#endif //defined(OPEN_SOURCE)
} ENCODE_SET_PICTURE_PARAMETERS_HEVC;

typedef struct tagENCODE_SET_SLICE_HEADER_HEVC
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
} ENCODE_SET_SLICE_HEADER_HEVC;

typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT : ENCODE_SET_SEQUENCE_PARAMETERS_HEVC
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
} ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT;

typedef struct tagENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT : ENCODE_SET_PICTURE_PARAMETERS_HEVC
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
} ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT;

typedef struct tagENCODE_SET_SLICE_HEADER_HEVC_REXT : ENCODE_SET_SLICE_HEADER_HEVC
{
    SHORT   luma_offset_l0[15];
    SHORT   ChromaOffsetL0[15][2];
    SHORT   luma_offset_l1[15];
    SHORT   ChromaOffsetL1[15][2];
} ENCODE_SET_SLICE_HEADER_HEVC_REXT;

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

#endif // #if !defined(OPEN_SOURCE)