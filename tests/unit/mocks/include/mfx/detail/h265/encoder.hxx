// Copyright (c) 2020 Intel Corporation
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

#if !defined(__ENCODING_DDI_H__)
    #error "\'encoder.hxx\' requires \'encoding_ddi.h\' to be included"
#endif

#if !defined(MOCKS_MFX_ENCODER_HXX)
    #define MOCKS_MFX_ENCODER_HXX
#endif

namespace mocks { namespace mfx { namespace h265
{
    struct ENCODE_CAPS_HEVC
    {
        union {
            struct {
                mfxU32    CodingLimitSet : 1;
                mfxU32    BitDepth8Only : 1;
                mfxU32    Color420Only : 1;
                mfxU32    SliceStructure : 3;
                mfxU32    SliceIPOnly : 1;
                mfxU32    SliceIPBOnly : 1;
                mfxU32    NoWeightedPred : 1;
                mfxU32    NoMinorMVs : 1;
                mfxU32    RawReconRefToggle : 1;
                mfxU32    NoInterlacedField : 1;
                mfxU32    BRCReset : 1;
                mfxU32    RollingIntraRefresh : 1;
                mfxU32    UserMaxFrameSizeSupport : 1;
                mfxU32    FrameLevelRateCtrl : 1;
                mfxU32    SliceByteSizeCtrl : 1;
                mfxU32    VCMBitRateControl : 1;
                mfxU32    ParallelBRC : 1;
                mfxU32    TileSupport : 1;
                mfxU32    SkipFrame : 1;
                mfxU32    MbQpDataSupport : 1;
                mfxU32    SliceLevelWeightedPred : 1;
                mfxU32    LumaWeightedPred : 1;
                mfxU32    ChromaWeightedPred : 1;
                mfxU32    QVBRBRCSupport : 1;
                mfxU32    HMEOffsetSupport : 1;
                mfxU32    YUV422ReconSupport : 1;
                mfxU32    YUV444ReconSupport : 1;
                mfxU32    RGBReconSupport : 1;
                mfxU32    MaxEncodedBitDepth : 2;
            };
            mfxU32    CodingLimits;
        };

        mfxU32  MaxPicWidth;
        mfxU32  MaxPicHeight;
        mfxU8   MaxNum_Reference0;
        mfxU8   MaxNum_Reference1;
        mfxU8   MBBRCSupport;
        mfxU8   TUSupport;

        union {
            struct {
                mfxU8    MaxNumOfROI : 5; // [0..16]
                mfxU8    ROIBRCPriorityLevelSupport : 1;
                mfxU8    BlockSize : 2;
            };
            mfxU8    ROICaps;
        };

        union {
            struct {
                mfxU32    SliceLevelReportSupport : 1;
                mfxU32    CTULevelReportSupport : 1;
                mfxU32    SearchWindow64Support : 1;
                mfxU32    CustomRoundingControl : 1;
                mfxU32    ReservedBit1 : 1;
                mfxU32    IntraRefreshBlockUnitSize : 2;
                mfxU32    LCUSizeSupported : 3;
                mfxU32    MaxNumDeltaQP : 4;
                mfxU32    DirtyRectSupport : 1;
                mfxU32    MoveRectSupport : 1;
                mfxU32    FrameSizeToleranceSupport : 1;
                mfxU32    HWCounterAutoIncrementSupport : 2;
                mfxU32    ROIDeltaQPSupport : 1;
                mfxU32    NumScalablePipesMinus1 : 5;
                mfxU32    NegativeQPSupport : 1;
                mfxU32    ReservedBit2 : 1;
                mfxU32    TileBasedEncodingSupport : 1;
                mfxU32    PartialFrameUpdateSupport : 1;
                mfxU32    RGBEncodingSupport : 1;
                mfxU32    LLCStreamingBufferSupport : 1;
                mfxU32    DDRStreamingBufferSupport : 1;
            };
            mfxU32    CodingLimits2;
        };

        mfxU8    MaxNum_WeightedPredL0;
        mfxU8    MaxNum_WeightedPredL1;
        mfxU16   MaxNumOfDirtyRect;
        mfxU16   MaxNumOfMoveRect;
        mfxU16   MaxNumOfConcurrentFramesMinus1;
        mfxU16   LLCSizeInMBytes;

        union {
            struct {
                mfxU16    PFrameSupport            : 1;
                mfxU16    LookaheadAnalysisSupport : 1;
                mfxU16    LookaheadBRCSupport      : 1;
                mfxU16    reservedbits             : 13;
            };
            mfxU16    CodingLimits3;
        };

        mfxU32   reserved32bits1;
        mfxU32   reserved32bits2;
        mfxU32   reserved32bits3;
    };

    //The newer Gen depends on internals of older ones, so inclusion order does matter
    #include  "g9/encoder.hxx"
    #include "g10/encoder.hxx"
    #include "g11/encoder.hxx"
    #include "g12/encoder.hxx"

    enum ENCODE_INPUT_COLORSPACE
    {
        eColorSpace_P709 = 0,
        eColorSpace_P601 = 1,
        eColorSpace_P2020 = 2
    };

    enum ENCODE_FRAME_SIZE_TOLERANCE
    {
        eFrameSizeTolerance_Normal = 0,//default
        eFrameSizeTolerance_Low = 1,//Sliding Window
        eFrameSizeTolerance_ExtremelyLow = 2//low delay
    };

    enum ENCODE_SCENARIO
    {
        eScenario_Unknown = 0,
        eScenario_DisplayRemoting = 1,
        eScenario_VideoConference = 2,
        eScenario_Archive = 3,
        eScenario_LiveStreaming = 4,
        eScenario_VideoCapture = 5,
        eScenario_VideoSurveillance = 6,
        eScenario_GameStreaming = 7,
        eScenario_RemoteGaming = 8
    };

    enum ENCODE_CONTENT
    {
        eContent_Unknown = 0,
        eContent_FullScreenVideo = 1,
        eContent_NonVideoScreen = 2
    };

    struct FRAMERATE
    {
        UINT    Numerator;
        UINT    Denominator;
    };

    struct ENCODE_SET_SEQUENCE_PARAMETERS_HEVC
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
                UINT    bResetBRC                           : 1;
                UINT    GlobalSearch                        : 2;
                UINT    LocalSearch                         : 4;
                UINT    EarlySkip                           : 2;
                UINT    MBBRC                               : 4;
                UINT    ParallelBRC                         : 1;
                UINT    SliceSizeControl                    : 1;
                UINT    SourceFormat                        : 2;
                UINT    SourceBitDepth                      : 2;
                UINT    QpAdjustment                        : 1;
                UINT    ROIValueInDeltaQP                   : 1;
                UINT    BlockQPforNonRectROI                : 1;
                UINT    EnableTileBasedEncode               : 1;
                UINT    bAutoMaxPBFrameSizeForSceneChange   : 1;
                UINT    EnableStreamingBufferLLC            : 1;
                UINT    EnableStreamingBufferDDR            : 1;
                UINT    LowDelayMode                        : 1;
                UINT    DisableHRDConformance               : 1;
                UINT    HierarchicalFlag                    : 1;
                UINT    ReservedBits                        : 3;
            };
            UINT    EncodeFlags;
        };

        UINT    UserMaxIFrameSize;
        UINT    UserMaxPBFrameSize;
        UCHAR   ICQQualityFactor;   // [1..51]
        UCHAR   StreamBufferSessionID;
        USHORT  Reserved16b;
        UINT    NumOfBInGop[3];     // deprecated from Gen12

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
                UINT    reserved                            : 1;
                UINT    chroma_format_idc                   : 2;
                UINT    separate_colour_plane_flag          : 1;
                UINT    palette_mode_enabled_flag           : 1;
                UINT    RGBEncodingEnable                   : 1;
                UINT    PrimaryChannelForRGBEncoding        : 2;
                UINT    SecondaryChannelForRGBEncoding      : 2;
                UINT                                        : 15;    // [0]
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
        ENCODE_FRAME_SIZE_TOLERANCE FrameSizeTolerance;

        USHORT  SlidingWindowSize;
        UINT    MaxBitRatePerSlidingWindow;
        UINT    MinBitRatePerSlidingWindow;

        UCHAR   LookaheadDepth;               // [0..127]
        UCHAR   reserved8b[3];
    };

    struct ENCODE_SET_PICTURE_PARAMETERS_HEVC
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
                UINT    deblocking_filter_override_enabled_flag : 1;
                UINT    pps_deblocking_filter_disabled_flag     : 1;
                UINT    bEnableCTULevelReport                   : 1;    // [0..1]
                UINT    bEnablePartialFrameUpdate               : 1;
                UINT                                            : 3;
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
                UINT    RoundingOffsetIntra      : 7;
                UINT    EnableCustomRoudingInter : 1;
                UINT    RoundingOffsetInter      : 7;
                UINT                             : 16;
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

    #if defined(OPEN_SOURCE)
        UINT reserved;
    #else
        ENCODE_INPUT_TYPE   InputType;
    #endif //defined(OPEN_SOURCE)

        union
        {
            struct
            {
                UINT    pps_curr_pic_ref_enabled_flag : 1;
                UINT                                  : 31;
            } ;
            UINT    ExtensionControlFlags;
        } ;

        UINT    TileOffsetBufferSizeInByte;
        UINT    *pTileOffset;
    };

    struct ENCODE_SET_SLICE_HEADER_HEVC
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
    };

    template <bool LowPower>
    inline constexpr
    ENCODE_CAPS_HEVC caps(gen0, std::integral_constant<bool, LowPower>) //fallback
    { return {}; }

    template <bool LowPower>
    inline constexpr
    ENCODE_CAPS_HEVC caps(gen9, std::integral_constant<bool, LowPower>)
    { return g9::caps(std::integral_constant<bool, LowPower>{}); }

    template <bool LowPower>
    inline constexpr
    ENCODE_CAPS_HEVC caps(gen10, std::integral_constant<bool, LowPower>)
    { return g10::caps(std::integral_constant<bool, LowPower>{}); }

    inline constexpr
    ENCODE_CAPS_HEVC caps(gen11, std::false_type /*VME*/)
    { return g11::caps(std::false_type{}); }
    inline constexpr
    ENCODE_CAPS_HEVC caps(gen11, std::true_type /*VDENC*/)
    { return g11::caps(std::true_type{}); }

    inline constexpr
    ENCODE_CAPS_HEVC caps(gen12, std::false_type /*VME*/)
    { return g12::caps(std::false_type{}); }
    inline constexpr
    ENCODE_CAPS_HEVC caps(gen12, std::true_type /*VDENC*/)
    { return g12::caps(std::true_type{}); }

    template <int Type, bool LowPower>
    inline constexpr
    ENCODE_CAPS_HEVC caps(std::integral_constant<int, Type>, std::integral_constant<bool, LowPower>)
    { return caps(gen_of<Type>::type{}, std::integral_constant<bool, LowPower>{}); }

    template <typename Gen>
    inline
    typename std::enable_if<is_gen<Gen>::value, ENCODE_CAPS_HEVC>::type
    caps(Gen, mfxVideoParam const& vp)
    {
        return vp.mfx.LowPower == MFX_CODINGOPTION_ON ?
            caps(Gen{}, std::true_type{}) :
            caps(Gen{}, std::false_type{})
        ;
    }

    inline
    ENCODE_CAPS_HEVC caps(int type, mfxVideoParam const& vp)
    {
        return vp.mfx.LowPower == MFX_CODINGOPTION_ON ?
            invoke(type, MOCKS_LIFT(caps), std::true_type{}) :
            invoke(type, MOCKS_LIFT(caps), std::false_type{})
        ;
    }


    int constexpr ENCODE_BITSTREAM_BUFFER_NUM = 3;
    int constexpr ENCODE_MB_BUFFER_NUM        = 6;
    ENCODE_COMP_BUFFER_INFO const comp_buffers[] =
    {
        { D3DDDIFORMAT(D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA),          D3DDDIFORMAT(DXGI_FORMAT_P8), sizeof(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC), 1, 1                           },
        { D3DDDIFORMAT(D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA),          D3DDDIFORMAT(DXGI_FORMAT_P8), sizeof(ENCODE_SET_PICTURE_PARAMETERS_HEVC),  1, 1                           },
        { D3DDDIFORMAT(D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA),        D3DDDIFORMAT(DXGI_FORMAT_P8), sizeof(ENCODE_SET_SLICE_HEADER_HEVC),        1, 1                           },
        { D3DDDIFORMAT(D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA),        D3DDDIFORMAT(DXGI_FORMAT_P8), sizeof(ENCODE_SET_PICTURE_QUANT),            1, 1                           },
        { D3DDDIFORMAT(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA),    D3DDDIFORMAT(DXGI_FORMAT_P8), 1,                                           1, ENCODE_BITSTREAM_BUFFER_NUM },
        { D3DDDIFORMAT(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA), D3DDDIFORMAT(DXGI_FORMAT_P8), sizeof(ENCODE_PACKEDHEADER_DATA),            1, 1                           },
        { D3DDDIFORMAT(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA),  D3DDDIFORMAT(DXGI_FORMAT_P8), sizeof(ENCODE_PACKEDHEADER_DATA),            1, 1                           },
        { D3DDDIFORMAT(D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA),         D3DDDIFORMAT(DXGI_FORMAT_P8), 1,                                           1, ENCODE_MB_BUFFER_NUM        },
        { D3DDDIFORMAT(D3D11_DDI_VIDEO_ENCODER_BUFFER_LOOKAHEADDATA),    D3DDDIFORMAT(DXGI_FORMAT_P8), 32,                                        128, 1                           }
     };

} } }
