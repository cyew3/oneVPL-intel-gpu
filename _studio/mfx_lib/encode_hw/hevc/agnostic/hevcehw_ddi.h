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

typedef struct tagFRAMERATE
{
    UINT    Numerator;
    UINT    Denominator;
} FRAMERATE;

typedef ENCODE_ARGB_COLOR ENCODE_INPUT_COLORSPACE;
typedef ENCODE_FRAME_SIZE_TOLERANCE ENCODE_FRAMESIZE_TOLERANCE;

typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_HEVC
{
    USHORT  wFrameWidthInMinCbMinus1;
    USHORT  wFrameHeightInMinCbMinus1;
    UCHAR   general_profile_idc;        // [1..4]
    UCHAR   general_level_idc;
    UCHAR   general_tier_flag;          // [0..1]

    USHORT  GopPicSize;
    UCHAR   GopRefDist;
    UCHAR   GopOptFlag : 2;
    UCHAR : 6;

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
                    UINT    bResetBRC : 1;
                    UINT    GlobalSearch : 2;
                    UINT    LocalSearch : 4;
                    UINT    EarlySkip : 2;
                    UINT    MBBRC : 4;
                    UINT    ParallelBRC : 1;
                    UINT    SliceSizeControl : 1;
                    UINT    SourceFormat : 2;
                    UINT    SourceBitDepth : 2;
                    UINT    QpAdjustment : 1;
                    UINT    ROIValueInDeltaQP : 1;
                    UINT    BlockQPforNonRectROI : 1;
                    UINT    EnableTileBasedEncode : 1;
                    UINT    bAutoMaxPBFrameSizeForSceneChange : 1;
                    UINT    EnableStreamingBufferLLC : 1;
                    UINT    EnableStreamingBufferDDR : 1;
                    UINT    LowDelayMode : 1;
                    UINT    DisableHRDConformance : 1;
                    UINT    HierarchicalFlag : 1;
                    UINT    TCBRCEnable : 1;
                    UINT    bLookAheadPhase : 1;
                    UINT    ReservedBits : 1;
                };
                UINT    EncodeFlags;
            };

            UINT    UserMaxIFrameSize;
            UINT    UserMaxPBFrameSize;
            UCHAR   ICQQualityFactor;   // [1..51]
            UCHAR   StreamBufferSessionID;

            UCHAR   maxAdaptiveMiniGopSize;
            union
            {
                struct
                {
                    UCHAR    	ClosedGop : 1;	// [0..1]
                    UCHAR    	StrictGop : 1;	// [0..1]
                    UCHAR    	AdaptiveGop : 1;	// [0..1]
                    UCHAR    	ReservedBits : 5;	// [0]
                } fields;
                UCHAR	value;
            } GopFlags; //valid only when bLookAheadPhase equals 1


            UINT    NumOfBInGop[3];     // deprecated from Gen12

            union
            {
                struct
                {
                    UINT    scaling_list_enable_flag : 1;
                    UINT    sps_temporal_mvp_enable_flag : 1;
                    UINT    strong_intra_smoothing_enable_flag : 1;
                    UINT    amp_enabled_flag : 1;
                    UINT    SAO_enabled_flag : 1;
                    UINT    pcm_enabled_flag : 1;
                    UINT    pcm_loop_filter_disable_flag : 1;
                    UINT    reserved : 1;
                    UINT    chroma_format_idc : 2;
                    UINT    separate_colour_plane_flag : 1;
                    UINT    palette_mode_enabled_flag : 1;
                    UINT    RGBEncodingEnable : 1;
                    UINT    PrimaryChannelForRGBEncoding : 2;
                    UINT    SecondaryChannelForRGBEncoding : 2;
                    UINT : 15;    // [0]
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

            USHORT  SlidingWindowSize;
            UINT    MaxBitRatePerSlidingWindow;
            UINT    MinBitRatePerSlidingWindow;

            union
            {
                UCHAR   LookaheadDepth;               // [0..100]
                UCHAR   TargetFrameSizeConfidence;    //[0..100]
            };

            UCHAR       minAdaptiveGopPicSize;
            USHORT      maxAdaptiveGopPicSize;

            UINT        reserved32b[16];

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
    UCHAR   HierarchLevelPlus1;
    USHORT  NumSlices;

    union
    {
        struct
        {
            UINT    tiles_enabled_flag : 1;
            UINT    entropy_coding_sync_enabled_flag : 1;
            UINT    sign_data_hiding_flag : 1;
            UINT    constrained_intra_pred_flag : 1;
            UINT    transform_skip_enabled_flag : 1;
            UINT    transquant_bypass_enabled_flag : 1;
            UINT    cu_qp_delta_enabled_flag : 1;
            UINT    weighted_pred_flag : 1;
            UINT    weighted_bipred_flag : 1;
            //  loop filter flags
            UINT    loop_filter_across_slices_flag : 1;
            UINT    loop_filter_across_tiles_flag : 1;
            UINT    scaling_list_data_present_flag : 1;
            UINT    dependent_slice_segments_enabled_flag : 1;
            UINT    bLastPicInSeq : 1;
            UINT    bLastPicInStream : 1;
            UINT    bUseRawPicForRef : 1;
            UINT    bEmulationByteInsertion : 1;
            UINT    BRCPrecision : 2;
            UINT    bEnableSliceLevelReport : 1;    // [0..1]
            UINT    bEnableRollingIntraRefresh : 2;    // [0..2]
            UINT    no_output_of_prior_pics_flag : 1;    // [0..1]
            UINT    bEnableGPUWeightedPrediction : 1;    // [0..1]
            UINT    DisplayFormatSwizzle : 1;
            UINT    deblocking_filter_override_enabled_flag : 1;
            UINT    pps_deblocking_filter_disabled_flag : 1;
            UINT    bEnableCTULevelReport : 1;    // [0..1]
            UINT    bEnablePartialFrameUpdate : 1;
            UINT    reserved1bit : 1;
            UINT    bTileReplayEnable : 1;
            UINT : 1;
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

    UINT        MaxSliceSizeInBytes;

    UCHAR       NumROI;                     // [0..16]
    ENCODE_ROI  ROI[16];
    CHAR        MaxDeltaQp;                 // [-51..51]
    CHAR        MinDeltaQp;                 // [-51..51]
    UCHAR       NumDeltaQpForNonRectROI;    // [0..15]
    CHAR        NonRectROIDeltaQpList[16];

    union
    {
        struct
        {
            UINT    EnableCustomRoudingIntra : 1;
            UINT    RoundingOffsetIntra : 7;
            UINT    EnableCustomRoudingInter : 1;
            UINT    RoundingOffsetInter : 7;
            UINT : 16;
        };
        UINT    CustomRoundingOffsetsParams;
    };

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

    ENCODE_INPUT_TYPE   InputType;

    union
    {
        struct
        {
            UINT    pps_curr_pic_ref_enabled_flag : 1;
            UINT : 31;
        };
        UINT    ExtensionControlFlags;
    };

    UINT    TileOffsetBufferSizeInByte;
    UINT    *pTileOffset;
    USHORT  LcuMaxBitsizeAllowedHigh16b;

    union
    {
        struct
        {
            UCHAR    X16Minus1_X : 4; // scaling ratio = (X16Minus1_X + 1) / 16
            UCHAR    X16Minus1_Y : 4;
        };
        UCHAR    DownScaleRatio;
    };

    UCHAR   QpModulationStrength;

    UINT    TargetFrameSize;
    UINT    reserved32b[16];

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
            UINT    bLastSliceOfPic : 1;
            UINT    dependent_slice_segment_flag : 1;
            UINT    slice_temporal_mvp_enable_flag : 1;
            UINT    slice_type : 2;
            UINT    slice_sao_luma_flag : 1;
            UINT    slice_sao_chroma_flag : 1;
            UINT    mvd_l1_zero_flag : 1;
            UINT    cabac_init_flag : 1;
            UINT    slice_deblocking_filter_disable_flag : 1;
            UINT    collocated_from_l0_flag : 1;
            UINT    reserved : 21;
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
            UINT    transform_skip_rotation_enabled_flag : 1;
            UINT    transform_skip_context_enabled_flag : 1;
            UINT    implicit_rdpcm_enabled_flag : 1;
            UINT    explicit_rdpcm_enabled_flag : 1;
            UINT    extended_precision_processing_flag : 1;
            UINT    intra_smoothing_disabled_flag : 1;
            UINT    high_precision_offsets_enabled_flag : 1;
            UINT    persistent_rice_adaptation_enabled_flag : 1;
            UINT    cabac_bypass_alignment_enabled_flag : 2; // 2 ???
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
            UINT  chroma_qp_offset_list_enabled_flag : 1;
            UINT  diff_cu_chroma_qp_offset_depth : 3;
            UINT  chroma_qp_offset_list_len_minus1 : 3; // [0..5]
            UINT  log2_sao_offset_scale_luma : 3; // [0..6]
            UINT  log2_sao_offset_scale_chroma : 3; // [0..6]
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
    UCHAR ucScalingLists0[6][16];
    UCHAR ucScalingLists1[6][64];
    UCHAR ucScalingLists2[6][64];
    UCHAR ucScalingLists3[2][64];
    UCHAR ucScalingListDCCoefSizeID2[6];
    UCHAR ucScalingListDCCoefSizeID3[2];
} ENCODE_SET_QMATRIX_HEVC;

#endif // !defined(MFX_VA_LINUX)

namespace HEVCEHW
{
typedef struct tagENCODE_CAPS_HEVC
{
    union{
        struct {
            uint32_t CodingLimitSet          : 1;
            uint32_t BitDepth8Only           : 1;
            uint32_t Color420Only            : 1;
            uint32_t SliceStructure          : 3;
            uint32_t SliceIPOnly             : 1;
            uint32_t SliceIPBOnly            : 1;
            uint32_t NoWeightedPred          : 1;
            uint32_t NoMinorMVs              : 1;
            uint32_t RawReconRefToggle       : 1;
            uint32_t NoInterlacedField       : 1;
            uint32_t BRCReset                : 1;
            uint32_t RollingIntraRefresh     : 1;
            uint32_t UserMaxFrameSizeSupport : 1;
            uint32_t FrameLevelRateCtrl      : 1;
            uint32_t SliceByteSizeCtrl       : 1;
            uint32_t VCMBitRateControl       : 1;
            uint32_t ParallelBRC             : 1;
            uint32_t TileSupport             : 1;
            uint32_t SkipFrame               : 1;
            uint32_t MbQpDataSupport         : 1;
            uint32_t SliceLevelWeightedPred  : 1;
            uint32_t LumaWeightedPred        : 1;
            uint32_t ChromaWeightedPred      : 1;
            uint32_t QVBRBRCSupport          : 1;
            uint32_t HMEOffsetSupport        : 1;
            uint32_t YUV422ReconSupport      : 1;
            uint32_t YUV444ReconSupport      : 1;
            uint32_t RGBReconSupport         : 1;
            uint32_t MaxEncodedBitDepth      : 2;
        };
        uint32_t CodingLimits;
    };

    uint32_t MaxPicWidth;
    uint32_t MaxPicHeight;
    uint8_t  MaxNum_Reference0;
    uint8_t  MaxNum_Reference1;
    uint8_t  MBBRCSupport;
    uint8_t  TUSupport;

    union {
        struct {
            uint8_t MaxNumOfROI                : 5; // [0..16]    
            uint8_t ROIBRCPriorityLevelSupport : 1;
            uint8_t BlockSize                  : 2;
        };
        uint8_t ROICaps;
    };

    union {
        struct {
            uint32_t SliceLevelReportSupport         : 1;
            uint32_t CTULevelReportSupport           : 1;
            uint32_t SearchWindow64Support           : 1;
            uint32_t CustomRoundingControl           : 1;
            uint32_t LowDelayBRCSupport              : 1;
            uint32_t IntraRefreshBlockUnitSize       : 2;
            uint32_t LCUSizeSupported                : 3;
            uint32_t MaxNumDeltaQPMinus1             : 4;
            uint32_t DirtyRectSupport                : 1;
            uint32_t MoveRectSupport                 : 1;
            uint32_t FrameSizeToleranceSupport       : 1;
            uint32_t HWCounterAutoIncrementSupport   : 2;
            uint32_t ROIDeltaQPSupport               : 1;
            uint32_t NumScalablePipesMinus1          : 5;
            uint32_t NegativeQPSupport               : 1;
            uint32_t HRDConformanceSupport           : 1;
            uint32_t TileBasedEncodingSupport        : 1;
            uint32_t PartialFrameUpdateSupport       : 1;
            uint32_t RGBEncodingSupport              : 1;
            uint32_t LLCStreamingBufferSupport       : 1;
            uint32_t DDRStreamingBufferSupport       : 1;
        };
        uint32_t CodingLimits2;
    };

    uint8_t  MaxNum_WeightedPredL0;
    uint8_t  MaxNum_WeightedPredL1;
    uint16_t MaxNumOfDirtyRect;
    uint16_t MaxNumOfMoveRect;
    uint16_t MaxNumOfConcurrentFramesMinus1;
    uint16_t LLCSizeInMBytes;

    union {
        struct {
            uint16_t PFrameSupport            : 1;
            uint16_t LookaheadAnalysisSupport : 1;
            uint16_t LookaheadBRCSupport      : 1;
            uint16_t TileReplaySupport        : 1;
            uint16_t TCBRCSupport             : 1;
            uint16_t reservedbits             : 11;
        };
        uint16_t CodingLimits3;
    };

    uint32_t reserved32bits1;
    uint32_t reserved32bits2;
    uint32_t reserved32bits3;

} ENCODE_CAPS_HEVC;
}; //namespace HEVCEHW
