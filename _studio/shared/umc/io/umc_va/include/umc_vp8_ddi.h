//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2017 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VP8_DDI_H
#define __UMC_VP8_DDI_H

/*
D3DDDIFMT_PICTUREPARAMSDATA       = 150
D3DDDIFMT_MACROBLOCKDATA          = 151
D3DDDIFMT_RESIDUALDIFFERENCEDATA  = 152
D3DDDIFMT_DEBLOCKINGDATA          = 153
D3DDDIFMT_INVERSEQUANTIZATIONDATA = 154
D3DDDIFMT_SLICECONTROLDATA        = 155
D3DDDIFMT_BITSTREAMDATA           = 156
*/

// The host decoder will send the following DXVA buffers to the accelerator:
//  One picture parameters buffer.
//  One quantization matrix buffer.
//  One coefficient probability data buffer containing coefficient probabilities.
//  One bitstream data buffer.

typedef enum D3D9_VIDEO_DECODER_BUFFER_VP8_TYPE
{
    D3D9_VIDEO_DECODER_BUFFER_VP8_COEFFICIENT_PROBABILITIES = 159 - 150 + 1,
    D3D9_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX = 154 - 150 + 1,
    D3D9_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS = 150 - 150 + 1,
    D3D9_VIDEO_DECODER_BUFFER_BITSTREAM_DATA = 156 - 150 + 1,

} D3D9_VIDEO_DECODER_BUFFER_VP8_TYPE;

/*
typedef enum D3D11_VIDEO_DECODER_BUFFER_VP8_TYPE
{
    D3D11_VIDEO_DECODER_BUFFER_VP8_COEFFICIENT_PROBABILITIES = 9,
    D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX,
    D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS = 0,
    D3D11_VIDEO_DECODER_BUFFER_BITSTREAM_DATA,

} D3D11_VIDEO_DECODER_BUFFER_VP8_TYPE;
*/

namespace UMC
{
#pragma warning(disable: 4201)

typedef struct tagVP8_DECODE_PICTURE_PARAMETERS
{
    USHORT wFrameWidthInMbsMinus1;
    USHORT wFrameHeightInMbsMinus1;
    UCHAR CurrPicIndex;
    UCHAR LastRefPicIndex;
    UCHAR GoldenRefPicIndex;
    UCHAR AltRefPicIndex;
    UCHAR DeblockedPicIndex;
    UCHAR Reserved8Bits;

    union
    {
        USHORT wPicFlags;
        
        struct
        {
            USHORT key_frame: 1;
            USHORT version: 3;
            USHORT segmentation_enabled: 1;
            USHORT update_mb_segmentation_map: 1;
            USHORT update_segment_feature_data: 1;
            USHORT filter_type: 1;
            USHORT sign_bias_golden: 1;
            USHORT sign_bias_alternate: 1;
            USHORT mb_no_coeff_skip: 1;
            USHORT mode_ref_lf_delta_update: 1;
            USHORT CodedCoeffTokenPartition: 2;
            USHORT LoopFilterDisable: 1;
            USHORT loop_filter_adj_enable: 1;
        };
    };

    UCHAR loop_filter_level[4];
    CHAR ref_lf_delta[4];
    CHAR mode_lf_delta[4];
    UCHAR sharpness_level;
    CHAR mb_segment_tree_probs[3];
    UCHAR prob_skip_false;
    UCHAR prob_intra;
    UCHAR prob_last;
    UCHAR prob_golden;
    UCHAR y_mode_probs[4];
    UCHAR uv_mode_probs[3];
    UCHAR Reserved8Bits1;
    UCHAR mv_update_prob[2][19];
    UCHAR P0EntropyCount;
    UCHAR P0EntropyValue;

    UINT P0EntropyRange;
    UINT FirstMbBitOffset;
    UINT PartitionSize[9];

} VP8_DECODE_PICTURE_PARAMETERS;

typedef 
struct tagVP8_DECODE_QM_TABLE
{
    USHORT Qvalue[4][6];

} VP8_DECODE_QM_TABLE;

/*
Contains the contents of the token probability table,
which may be incrementally modified in the frame header.
There are four dimensions to the token probability array.
The outermost dimension is indexed by the type of plane being decoded;
the next dimension is selected by the position of the coefficient being decoded; 
the third dimension, roughly speaking, measures the "local complexity" 
or extent to which nearby coefficients are non-zero; 
the fourth, and final, dimension of the token probability array is indexed by 
the position in the token tree structure, as are all tree probability arrays. 
The probabilities are adjusted when applicable.
*/

//UCHAR CoeffProbs[4][8][3][11];

}; // namespace UMC

#endif // __UMC_VP8_DDI_H
