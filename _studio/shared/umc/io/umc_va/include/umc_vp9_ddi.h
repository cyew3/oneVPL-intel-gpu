//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VP9_DDI_H
#define __UMC_VP9_DDI_H

#pragma warning(disable: 4201)

#define DXVA2_VP9CoefficientProbabilitiesBufferType 9

typedef enum
{
    D3D9_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS_VP9 = DXVA2_PictureParametersBufferType + 1,
    D3D9_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX_VP9 = DXVA2_InverseQuantizationMatrixBufferType + 1,
    D3D9_VIDEO_DECODER_BUFFER_BITSTREAM_DATA_VP9 = DXVA2_BitStreamDateBufferType + 1,
    D3D9_VIDEO_DECODER_BUFFER_COEFFICIENT_PROBABILITIES = DXVA2_VP9CoefficientProbabilitiesBufferType + 1,
} D3D9_VIDEO_DECODER_BUFFER_VP9_TYPE;

typedef struct _DXVA_Intel_PicParams_VP9
{
    USHORT    FrameHeightMinus1;    // [0..2^16-1]
    USHORT    FrameWidthMinus1;     // [0..2^16-1]

    union
    {
        struct
        {
            UINT    frame_type                      : 1;    // [0..1]
            UINT    show_frame                      : 1;    // [0..1]
            UINT    error_resilient_mode            : 1;    // [0..1]
            UINT    intra_only                      : 1;    // [0..1] 

            UINT    LastRefIdx                      : 3;    // [0..7]
            UINT    LastRefSignBias                 : 1;    // [0..1] 
            UINT    GoldenRefIdx                    : 3;    // [0..7]
            UINT    GoldenRefSignBias               : 1;    // [0..1]
            UINT    AltRefIdx                       : 3;    // [0..7]
            UINT    AltRefSignBias                  : 1;    // [0..1]

            UINT    allow_high_precision_mv         : 1;    // [0..1]
            UINT    mcomp_filter_type               : 3;    // [0..7]
            UINT    frame_parallel_decoding_mode    : 1;    // [0..1]
            UINT    segmentation_enabled            : 1;    // [1]
            UINT    segmentation_temporal_update    : 1;    // [1]
            UINT    segmentation_update_map         : 1;    // [1]
            UINT    reset_frame_context             : 2;    // [0..3]    
            UINT    refresh_frame_context           : 1;    // [0..1]    
            UINT    frame_context_idx               : 2;    // [0..3]    
            UINT    LosslessFlag                    : 1;    // [0..1]    
            UINT    ReservedField                   : 2;    // [0]    
        } fields;
        UINT value;
    } PicFlags;

    UCHAR    RefFrameList [8];                   // [0..127, 0xFF]
    UCHAR    CurrPic;                            // [0..127] 
    UCHAR    filter_level;                       // [0..63]
    UCHAR    sharpness_level;                    // [0..7]
    UCHAR    log2_tile_rows;                     // [0..2]
    UCHAR    log2_tile_columns;                  // [0..255]
    UCHAR    UncompressedHeaderLengthInBytes;    // [0..255]
    USHORT   FirstPartitionSize;                 // [0..65536]

    UCHAR    mb_segment_tree_probs[7];
    UCHAR    segment_pred_probs[3];

    UINT     BSBytesInBuffer;

    UINT     StatusReportFeedbackNumber;

    UCHAR   profile;            // [0..3]
    UCHAR   BitDepthMinus8;     // [0, 2, 4]
    UCHAR   subsampling_x;      // [0..1]
    UCHAR   subsampling_y;	    // [0..1]
} DXVA_Intel_PicParams_VP9, *LPDXVA_Intel_PicParams_VP9;

typedef struct _DXVA_Intel_Seg_VP9
{
    union
    {
        struct
        {
            USHORT    SegmentReferenceEnabled    : 1;    // [0..1]
            USHORT    SegmentReference           : 2;    // [0..3]
            USHORT    SegmentReferenceSkipped    : 1;    // [0..1]
            USHORT    ReservedField3            : 12;    // [0]      
        } fields;
        UINT value;
    } SegmentFlags;

    UCHAR    FilterLevel[4][2];        // [0..63]
    SHORT    LumaACQuantScale;
    SHORT    LumaDCQuantScale;
    SHORT    ChromaACQuantScale;
    SHORT    ChromaDCQuantScale;
} DXVA_Intel_Seg_VP9, *LPDXVA_Intel_Seg_VP9;

typedef struct _DXVA_Intel_Segment_VP9
{
    DXVA_Intel_Seg_VP9    SegData[8];
} DXVA_Intel_Segment_VP9, *LPDXVA_Intel_Segment_VP9;

#endif __UMC_VP9_DDI_H
