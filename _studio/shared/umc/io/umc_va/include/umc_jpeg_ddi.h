//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_JPEG_DDI_H
#define __UMC_JPEG_DDI_H

#define D3DDDIFMT_INTEL_JPEGDECODE_PPSDATA      11
#define D3DDDIFMT_INTEL_JPEGDECODE_QUANTDATA    12
#define D3DDDIFMT_INTEL_JPEGDECODE_HUFFTBLDATA  13
#define D3DDDIFMT_INTEL_JPEGDECODE_SCANDATA     14
#define D3DDDIFMT_BITSTREAMDATA                 7

namespace UMC
{
#pragma warning(disable: 4201)

typedef struct tagJPEG_DECODING_CAPS
{
    union {
        struct {
            UINT    Baseline_DCT_Huffman     : 1;
            UINT    MissingNullBytesHandling : 1;
            UINT                             : 30;
        };
        UINT          CodingLimits;
    };

    UINT    MaxComponents;
    UINT    MaxPicWidth;
    UINT    MaxPicHeight;
    UINT    MinPicWidth;
    UINT    MinPicHeight;
} JPEG_DECODING_CAPS;

typedef struct tagJPEG_DECODE_PICTURE_PARAMETER
{
    USHORT          FrameWidth;
    USHORT          FrameHeight;
    USHORT          NumCompInFrame;
    UCHAR           ComponentIdentifier[4];
    UCHAR           QuantTableSelector[4];
    UCHAR           ChromaType;
    UCHAR           Rotation;
    USHORT          TotalScans;
    UINT            InterleavedData     : 1;
    UINT            DownSamplingMethod  : 2;
    UINT            Reserved            : 29;
    UINT            StatusReportFeedbackNumber;
    UINT            RenderTargetFormat;  // DX11
} JPEG_DECODE_PICTURE_PARAMETERS;

typedef struct tagJPEG_DECODE_QM_TABLE
{
    UCHAR          TableIndex;
    UCHAR          Precision;
    UCHAR          Qm[64];
} JPEG_DECODE_QM_TABLE;

typedef struct tagJPEG_DECODE_HUFFMAN_TABLE
{
    UCHAR          TableClass;
    UCHAR          TableIndex;
    UCHAR          BITS[16];
    UCHAR          HUFFVAL[162];
} JPEG_DECODE_HUFFMAN_TABLE;

typedef struct tagJPEG_DECODE_SCAN_PARAMETER
{
    USHORT        NumComponents;
    UCHAR         ComponentSelector[4];
    UCHAR         DcHuffTblSelector[4];
    UCHAR         AcHuffTblSelector[4];
    USHORT        RestartInterval;
    UINT          MCUCount;
    USHORT        ScanHoriPosition;
    USHORT        ScanVertPosition;
    UINT          DataOffset;
    UINT          DataLength;
} JPEG_DECODE_SCAN_PARAMETER;

typedef struct tagJPEG_DECODE_IMAGE_LAYOUT
{
    UINT          ComponentDataOffset[4];
    UINT          Pitch;
} JPEG_DECODE_IMAGE_LAYOUT;

}; // namespace UMC

typedef struct tagJPEG_DECODE_QUERY_STATUS
{
    UINT        StatusReportFeedbackNumber;
    UCHAR       bStatus;
    UCHAR       reserved8bits;
    USHORT      reserved16bits;
} JPEG_DECODE_QUERY_STATUS;

#endif __UMC_JPEG_DDI_H
