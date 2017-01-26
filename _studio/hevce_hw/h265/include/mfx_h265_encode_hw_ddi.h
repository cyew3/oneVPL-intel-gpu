//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "mfx_config.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h264_encode_struct_vaapi.h"
#include "encoding_ddi.h"
#include "mfxplugin++.h"

#include "mfx_h265_encode_hw_utils.h"
#include "mfx_h265_encode_hw_bs.h"

#include <memory>
#include <vector>

//#define HEADER_PACKING_TEST
//#define DDI_TRACE

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    #define HEVCE_DDI_VERSION 966
#else
    #define HEVCE_DDI_VERSION 947
#endif

#define D3DDDIFMT_NV12 (D3DDDIFORMAT)(MFX_MAKEFOURCC('N', 'V', '1', '2'))
#define D3DDDIFMT_YU12 (D3DDDIFORMAT)(MFX_MAKEFOURCC('Y', 'U', '1', '2'))

namespace MfxHwH265Encode
{

// GUIDs from DDI for HEVC Encoder spec 0.956
static const GUID DXVA2_Intel_Encode_HEVC_Main =
{ 0x28566328, 0xf041, 0x4466, { 0x8b, 0x14, 0x8f, 0x58, 0x31, 0xe7, 0x8f, 0x8b } };

#ifndef OPEN_SOURCE
static const GUID DXVA2_Intel_Encode_HEVC_Main10 =
{ 0x6b4a94db, 0x54fe, 0x4ae1, { 0x9b, 0xe4, 0x7a, 0x7d, 0xad, 0x00, 0x46, 0x00 } };

#ifdef PRE_SI_TARGET_PLATFORM_GEN10
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main =
{ 0xb8b28e0c, 0xecab, 0x4217, { 0x8c, 0x82, 0xea, 0xaa, 0x97, 0x55, 0xaa, 0xf0 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main10 =
{ 0x8732ecfd, 0x9747, 0x4897, { 0xb4, 0x2a, 0xe5, 0x34, 0xf9, 0xff, 0x2b, 0x7a } };
#endif  //PRE_SI_TARGET_PLATFORM_GEN10

#ifdef PRE_SI_TARGET_PLATFORM_GEN11
static const GUID DXVA2_Intel_Encode_HEVC_Main422 =
{ 0x056a6e36, 0xf3a8, 0x4d00, { 0x96, 0x63, 0x7e, 0x94, 0x30, 0x35, 0x8b, 0xf9 } };
static const GUID DXVA2_Intel_Encode_HEVC_Main422_10 =
{ 0xe139b5ca, 0x47b2, 0x40e1, { 0xaf, 0x1c, 0xad, 0x71, 0xa6, 0x7a, 0x18, 0x36 } };
static const GUID DXVA2_Intel_Encode_HEVC_Main444 =
{ 0x5415a68c, 0x231e, 0x46f4, { 0x87, 0x8b, 0x5e, 0x9a, 0x22, 0xe9, 0x67, 0xe9 } };
static const GUID DXVA2_Intel_Encode_HEVC_Main444_10 =
{ 0x161be912, 0x44c2, 0x49c0, { 0xb6, 0x1e, 0xd9, 0x46, 0x85, 0x2b, 0x32, 0xa1 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main422 =
{ 0xcee393ab, 0x1030, 0x4f7b, { 0x8d, 0xbc, 0x55, 0x62, 0x9c, 0x72, 0xf1, 0x7e } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main422_10 =
{ 0x580da148, 0xe4bf, 0x49b1, { 0x94, 0x3b, 0x42, 0x14, 0xab, 0x05, 0xa6, 0xff } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main444 =
{ 0x87b2ae39, 0xc9a5, 0x4c53, { 0x86, 0xb8, 0xa5, 0x2d, 0x7e, 0xdb, 0xa4, 0x88 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main444_10 =
{ 0x10e19ac8, 0xbf39, 0x4443, { 0xbe, 0xc3, 0x1b, 0x0c, 0xbf, 0xe4, 0xc7, 0xaa } };
#endif
#endif // #ifndef OPEN_SOURCE

GUID GetGUID(MfxVideoParam const & par);

#ifndef OPEN_SOURCE
const GUID GuidTable[2][2][3] = 
{
    // LowPower = OFF
    {
        // BitDepthLuma = 8
        {
            /*420*/ DXVA2_Intel_Encode_HEVC_Main,
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_Encode_HEVC_Main422,
            /*444*/ DXVA2_Intel_Encode_HEVC_Main444
#endif
        },
        // BitDepthLuma = 10
        {
            /*420*/ DXVA2_Intel_Encode_HEVC_Main10,
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_Encode_HEVC_Main422_10,
            /*444*/ DXVA2_Intel_Encode_HEVC_Main444_10
#endif
        }
    },
    // LowPower = ON
    
#if defined(PRE_SI_TARGET_PLATFORM_GEN10)
    {
        // BitDepthLuma = 8
        {
            /*420*/ DXVA2_Intel_LowpowerEncode_HEVC_Main,
    #if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_LowpowerEncode_HEVC_Main422,
            /*444*/ DXVA2_Intel_LowpowerEncode_HEVC_Main444
    #endif
        },
        // BitDepthLuma = 10
        {
            /*420*/ DXVA2_Intel_LowpowerEncode_HEVC_Main10,
    #if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_LowpowerEncode_HEVC_Main422_10,
            /*444*/ DXVA2_Intel_LowpowerEncode_HEVC_Main444_10
    #endif
        }
    }
#endif
};
#endif

typedef struct tagENCODE_CAPS_HEVC
{
    union
    {
        struct
        {
            UINT    CodingLimitSet              : 1;
            UINT    BitDepth8Only               : 1;
            UINT    Color420Only                : 1;
            UINT    SliceStructure              : 3;
            UINT    SliceIPOnly                 : 1;
            UINT    SliceIPBOnly                : 1;
            UINT    NoWeightedPred              : 1;
            UINT    NoMinorMVs                  : 1;
            UINT    RawReconRefToggle           : 1;
            UINT    NoInterlacedField           : 1;
            UINT    BRCReset                    : 1;
            UINT    RollingIntraRefresh         : 1;
            UINT    UserMaxFrameSizeSupport     : 1;
            UINT    FrameLevelRateCtrl          : 1;
            UINT    SliceByteSizeCtrl           : 1;
            UINT    VCMBitRateControl           : 1;
            UINT    ParallelBRC                 : 1;
            UINT    TileSupport                 : 1;
            UINT    SkipFrame                   : 1;
            UINT    MbQpDataSupport             : 1;
            UINT    SliceLevelWeightedPred      : 1;
            UINT    LumaWeightedPred            : 1;
            UINT    ChromaWeightedPred          : 1;
            UINT    QVBRBRCSupport              : 1; 
            UINT    HMEOffsetSupport            : 1;
            UINT    YUV422ReconSupport          : 1;
            UINT    YUV444ReconSupport          : 1;
            UINT    RGBReconSupport             : 1;
            UINT    MaxEncodedBitDepth          : 2;
        };
        UINT CodingLimits;
    };

    UINT    MaxPicWidth;
    UINT    MaxPicHeight;
    UCHAR   MaxNum_Reference0;
    UCHAR   MaxNum_Reference1;
    UCHAR   MBBRCSupport;
    UCHAR   TUSupport;

    union {
        struct {
            UCHAR    MaxNumOfROI                    : 5; // [0..16]
            UCHAR    ROIBRCPriorityLevelSupport     : 1;
            UCHAR    ROIBlockSize                   : 2;
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

mfxStatus HardcodeCaps(ENCODE_CAPS_HEVC& caps, MFXCoreInterface* pCore, GUID guid);

typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_HEVC
{
    USHORT  wFrameWidthInMinCbMinus1;
    USHORT  wFrameHeightInMinCbMinus1;
    UCHAR   general_profile_idc;        // [1..3]
    UCHAR   general_level_idc;
    UCHAR   general_tier_flag;          // [0..1]

    USHORT  GopPicSize;
    UCHAR   GopRefDist;
    UCHAR   GopOptFlag  : 2;
    UCHAR               : 6;

    UCHAR   TargetUsage;
    UCHAR   RateControlMethod;
    UINT    TargetBitRate;
    UINT    MaxBitRate;
    UINT    MinBitRate;
    USHORT  FramesPer100Sec;
    ULONG   InitVBVBufferFullnessInBit;
    ULONG   VBVBufferSizeInBit;

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
            UINT    BlockQPInDeltaQPIndex   : 1;
            UINT    ReservedBits            : 10;
        }/*fields*/;
        UINT    EncodeFlags;
    }/*EncodeFlags*/;

    UINT    UserMaxFrameSize;
    USHORT  Reserved1;//AVBRAccuracy;
    USHORT  Reserved2;//AVBRConvergence;
    UCHAR   ICQQualityFactor;  // [1..51]

    UINT    NumOfBInGop[3];

    union
    {
        struct
        {
            UINT    scaling_list_enable_flag            : 1;    // [0..1]
            UINT    sps_temporal_mvp_enable_flag        : 1;    // [1]
            UINT    strong_intra_smoothing_enable_flag  : 1;    // [0]
            UINT    amp_enabled_flag                    : 1;    // [0..1]
            UINT    SAO_enabled_flag                    : 1;    // [0..1]
            UINT    pcm_enabled_flag                    : 1;    // [0..1]
            UINT    pcm_loop_filter_disable_flag        : 1;    // [0..1]
            UINT    tiles_fixed_structure_flag          : 1;    // [0]
            UINT    chroma_format_idc                   : 2;    // [1]
            UINT    separate_colour_plane_flag          : 1;    // [0]
            UINT    ReservedBits2                       : 21;   // [0]
        }/*fields*/;
        UINT    EncodeTools;
    }/*EncodeTools*/;

    UCHAR   log2_max_coding_block_size_minus3;      // [2]
    UCHAR   log2_min_coding_block_size_minus3;      // [0]
    UCHAR   log2_max_transform_block_size_minus2;   // [3]
    UCHAR   log2_min_transform_block_size_minus2;   // [0]
    UCHAR   max_transform_hierarchy_depth_intra;    // [2]
    UCHAR   max_transform_hierarchy_depth_inter;    // [2]
    UCHAR   log2_min_PCM_cb_size_minus3;            // [0]
    UCHAR   log2_max_PCM_cb_size_minus3;            // [0]
    UCHAR   bit_depth_luma_minus8;                  // [0]
    UCHAR   bit_depth_chroma_minus8;                // [0]
    UCHAR   pcm_sample_bit_depth_luma_minus1;       // [7]
    UCHAR   pcm_sample_bit_depth_chroma_minus1;     // [7]
    ENCODE_ARGB_COLOR           InputColorSpace;
    ENCODE_SCENARIO             ScenarioInfo;
    ENCODE_CONTENT              ContentInfo;
    ENCODE_FRAME_SIZE_TOLERANCE frame_size_tolerance;
} ENCODE_SET_SEQUENCE_PARAMETERS_HEVC;

typedef struct tagENCODE_SET_PICTURE_PARAMETERS_HEVC
{
    ENCODE_PICENTRY     CurrOriginalPic;
    ENCODE_PICENTRY     CurrReconstructedPic;
    UCHAR               CollocatedRefPicIndex;    // [0..14, 0xFF]
    ENCODE_PICENTRY     RefFrameList[15];
    INT                 CurrPicOrderCnt;
    INT                 RefFramePOCList[15];

    UCHAR   CodingType;
    USHORT  NumSlices;

    union
    {
        struct
        {
            UINT    tiles_enabled_flag                      : 1;    // [0]
            UINT    entropy_coding_sync_enabled_flag        : 1;    // [0]
            UINT    sign_data_hiding_flag                   : 1;    // [0]
            UINT    constrained_intra_pred_flag             : 1;    // [0]
            UINT    transform_skip_enabled_flag             : 1;    // [0]
            UINT    transquant_bypass_enabled_flag          : 1;    // [0]
            UINT    cu_qp_delta_enabled_flag                : 1;    // [0..1]
            UINT    weighted_pred_flag                      : 1;    // [0..1]
            UINT    weighted_bipred_flag                    : 1;    // [0..1]

            //  loop filter flags
            UINT    loop_filter_across_slices_flag          : 1;    // [0]
            UINT    loop_filter_across_tiles_flag           : 1;    // [0]
            UINT    scaling_list_data_present_flag          : 1;    // [0..1]
            UINT    dependent_slice_segments_enabled_flag   : 1;    // [0]
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
        }/*fields*/;
        UINT    PicFlags;
    }/*PicFlags*/;

    CHAR    QpY;
    UCHAR   diff_cu_qp_delta_depth;                 // [0]
    CHAR    pps_cb_qp_offset;                       // [-12..12]
    CHAR    pps_cr_qp_offset;                       // [-12..12]
    UCHAR   num_tile_columns_minus1;
    UCHAR   num_tile_rows_minus1;
    USHORT  tile_column_width[19];
    USHORT  tile_row_height[21];
    UCHAR   log2_parallel_merge_level_minus2;       // [0]
    UCHAR   num_ref_idx_l0_default_active_minus1;   // [0..14]
    UCHAR   num_ref_idx_l1_default_active_minus1;   // [0..14]
    USHORT  LcuMaxBitsizeAllowed;
    USHORT  IntraInsertionLocation;
    USHORT  IntraInsertionSize;
    CHAR    QpDeltaForInsertedIntra;
    UINT    StatusReportFeedbackNumber;

#if (HEVCE_DDI_VERSION >= 960)
    // following parameters are valid for CNL+ only
    UCHAR   slice_pic_parameter_set_id; // [0..63]
    UCHAR   nal_unit_type;              // [0..63]
    UINT    MaxSliceSizeInBytes;

    UCHAR       NumROI;                    // [0..16]
    ENCODE_ROI  ROI[16];
    CHAR        MaxDeltaQp;                 // [-51..51]
    CHAR        MinDeltaQp;                 // [-51..51]
    UCHAR       NumDeltaQpForNonRectROI;    // [0..15]
    CHAR        NonRectROIDeltaQpList[16];

    // Skip Frames
    UCHAR       SkipFrameFlag;
    UCHAR       NumSkipFrames;
    UINT        SizeSkipFrames;

    // Max/Min QP settings for BRC
    UCHAR       BRCMaxQp;
    UCHAR       BRCMinQp;

    // HME Offset
    UCHAR       bEnableHMEOffset;
    SHORT       HMEOffset[15][2];

    USHORT      NumDirtyRects;
    ENCODE_RECT *pDirtyRect;
    USHORT      NumMoveRects;
    MOVE_RECT   *pMoveRect;

    ENCODE_INPUT_TYPE    InputType;
#else
    // following parameters are for LowPower only
    UCHAR   slice_pic_parameter_set_id; // [0..63]
    UCHAR   nal_unit_type;              // [0..63]
    UINT    MaxSliceSizeInBytes;

    UCHAR       NumROI;                    // [0..16]
    ENCODE_ROI  ROI[16];
    CHAR        MaxDeltaQp;                    // [-51..51]
    CHAR        MinDeltaQp;                    // [-51..51]

    // Skip Frames
    UCHAR        SkipFrameFlag;
    UCHAR        NumSkipFrames;
    UINT         SizeSkipFrames;

    // Max/Min QP settings for BRC
    UCHAR        BRCMaxQp;
    UCHAR        BRCMinQp;

    // HME Offset
    UCHAR        bEnableHMEOffset;
    SHORT        HMEOffset[15][2];

    UCHAR        NumDirtyRects;
    ENCODE_RECT  *pDirtyRect;
    UCHAR        NumMoveRects;
    MOVE_RECT    *pMoveRect;
#endif
} ENCODE_SET_PICTURE_PARAMETERS_HEVC;

typedef struct tagENCODE_SET_SLICE_HEADER_HEVC
{
    UINT            slice_segment_address;
    UINT            NumLCUsInSlice;
    ENCODE_PICENTRY RefPicList[2][15];
    UCHAR           num_ref_idx_l0_active_minus1;   // [0..14]
    UCHAR           num_ref_idx_l1_active_minus1;   // [0..14]
    union
    {
        struct
        {
            UINT    bLastSliceOfPic                         : 1;
            UINT    dependent_slice_segment_flag            : 1;
            UINT    slice_temporal_mvp_enable_flag          : 1;    // [1]
            UINT    slice_type                              : 2;
            UINT    slice_sao_luma_flag                     : 1;    // [0..1]
            UINT    slice_sao_chroma_flag                   : 1;    // [0..1]
            UINT    mvd_l1_zero_flag                        : 1;    // [0]
            UINT    cabac_init_flag                         : 1;
            UINT    slice_deblocking_filter_disable_flag    : 1;
            UINT    collocated_from_l0_flag                 : 1;    // [0..1]
            UINT    reserved                                : 21;
        }/*fields*/;
        UINT    SliceFlags;
    }/*SliceFlags*/;

    CHAR    slice_qp_delta;
    CHAR    slice_cb_qp_offset;         // [-12..12]
    CHAR    slice_cr_qp_offset;         // [-12..12]
    CHAR    beta_offset_div2;           // [-6..6]
    CHAR    tc_offset_div2;             // [-6..6]
    UCHAR   luma_log2_weight_denom;
    UCHAR   chroma_log2_weight_denom;
    CHAR    luma_offset[2][15];
    CHAR    delta_luma_weight[2][15];
    CHAR    chroma_offset[2][15][2];
    CHAR    delta_chroma_weight[2][15][2];
    UCHAR   MaxNumMergeCand;
    USHORT  slice_id;
    USHORT  BitLenghtSliceHeaderStartingPortion;
    UINT    SliceHeaderByteOffset;
    UINT    SliceQpDeltaBitOffset;
    UINT    PredWeightTableBitOffset;
    UINT    PredWeightTableBitLength;
    UINT    SliceSAOFlagBitOffset;
} ENCODE_SET_SLICE_HEADER_HEVC;


#if (HEVCE_DDI_VERSION >= 960)

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

#endif //(HEVCE_DDI_VERSION >= 960)

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

class DriverEncoder;

typedef enum tagENCODER_TYPE
{
    ENCODER_DEFAULT = 0,
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    ENCODER_REXT
#endif //defined(PRE_SI_TARGET_PLATFORM_GEN11)
} ENCODER_TYPE;

DriverEncoder* CreatePlatformH265Encoder(MFXCoreInterface* core, ENCODER_TYPE type = ENCODER_DEFAULT);
mfxStatus QueryHwCaps(MFXCoreInterface* core, GUID guid, ENCODE_CAPS_HEVC & caps);
mfxStatus CheckHeaders(MfxVideoParam const & par, ENCODE_CAPS_HEVC const & caps);

#if MFX_EXTBUFF_CU_QP_ENABLE
mfxStatus FillCUQPDataDDI(Task& task, MfxVideoParam &par, MFXCoreInterface& core, mfxFrameInfo &CUQPFrameInfo);
#endif

enum
{
    MAX_DDI_BUFFERS = 4, //sps, pps, slice, bs
};

class DriverEncoder
{
public:

    virtual ~DriverEncoder(){}

    virtual
    mfxStatus CreateAuxilliaryDevice(
                    MFXCoreInterface * core,
                    GUID        guid,
                    mfxU32      width,
                    mfxU32      height) = 0;

    virtual
    mfxStatus CreateAccelerationService(
        MfxVideoParam const & par) = 0;

    virtual
    mfxStatus Reset(
        MfxVideoParam const & par, bool bResetBRC) = 0;

    virtual
    mfxStatus Register(
        mfxFrameAllocResponse & response,
        D3DDDIFORMAT            type) = 0;

    virtual
    mfxStatus Execute(
        Task const &task,
        mfxHDL surface) = 0;

    virtual
    mfxStatus QueryCompBufferInfo(
        D3DDDIFORMAT           type,
        mfxFrameAllocRequest & request) = 0;

    virtual
    mfxStatus QueryEncodeCaps(
        ENCODE_CAPS_HEVC & caps) = 0;


    virtual
    mfxStatus QueryStatus(
        Task & task ) = 0;

    virtual
    mfxStatus Destroy() = 0;

    virtual
    ENCODE_PACKEDHEADER_DATA* PackHeader(Task const & task, mfxU32 nut) = 0;
};

class DDIHeaderPacker
{
public:
    DDIHeaderPacker();
    ~DDIHeaderPacker();

    void Reset(MfxVideoParam const & par);

    ENCODE_PACKEDHEADER_DATA* PackHeader(Task const & task, mfxU32 nut);
    ENCODE_PACKEDHEADER_DATA* PackSliceHeader(Task const & task, mfxU32 id, mfxU32* qpd_offset);

    inline mfxU32 MaxPackedHeaders() { return (mfxU32)m_buf.size(); }

private:
    std::vector<ENCODE_PACKEDHEADER_DATA>           m_buf;
    std::vector<ENCODE_PACKEDHEADER_DATA>::iterator m_cur;
    HeaderPacker m_packer;

    void NewHeader();
};

#if defined(_WIN32) || defined(_WIN64)

inline mfxU32 FeedbackSize(ENCODE_QUERY_STATUS_PARAM_TYPE func, mfxU32 maxSlices)
{
    if (func == QUERY_STATUS_PARAM_FRAME)
        return sizeof(ENCODE_QUERY_STATUS_PARAMS);
    if (func == QUERY_STATUS_PARAM_SLICE)
        return sizeof(ENCODE_QUERY_STATUS_PARAMS) + sizeof(UINT) * 4 + sizeof(USHORT) * maxSlices;
    assert(!"unknown query function");
    return sizeof(ENCODE_QUERY_STATUS_PARAMS);
}

class FeedbackStorage
{
public:
    FeedbackStorage()
        :m_size(0)
    {
    }

    void Reset(size_t cacheSize, mfxU32 feedbackSize)
    {
        m_size = feedbackSize;
        m_buf.resize(m_size * cacheSize);
    }

    inline ENCODE_QUERY_STATUS_PARAMS& operator[] (size_t i) const
    {
        return *(ENCODE_QUERY_STATUS_PARAMS*)&m_buf[i * m_size];
    }

    inline size_t size() const
    {
        return (m_buf.size() / m_size);
    }

    inline void copy(size_t dstIdx, FeedbackStorage const & src, size_t srcIdx)
    {
        CopyN(&m_buf[dstIdx * m_size], &src.m_buf[srcIdx * src.m_size], Min(m_size, src.m_size));
    }

    inline mfxU32 feedback_size()
    {
        return m_size;
    }

private:
    std::vector<mfxU8> m_buf;
    mfxU32 m_size;
};

class CachedFeedback
{
public:
    typedef ENCODE_QUERY_STATUS_PARAMS Feedback;

    void Reset(mfxU32 cacheSize, mfxU32 feedbackSize = sizeof(Feedback));

    mfxStatus Update(FeedbackStorage const & update);

    const Feedback * Hit(mfxU32 feedbackNumber) const;

    mfxStatus Remove(mfxU32 feedbackNumber);

private:
    FeedbackStorage m_cache;
};

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC & sps);

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps);

void FillPpsBuffer(
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps);

void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & sps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice);

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice);

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT & sps);

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT & pps);

void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & sps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC_REXT> & slice);

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC_REXT> & slice);

#endif //defined(PRE_SI_TARGET_PLATFORM_GEN11)

#endif //defined(_WIN32) || defined(_WIN64)

}; // namespace MfxHwH265Encode
#endif
