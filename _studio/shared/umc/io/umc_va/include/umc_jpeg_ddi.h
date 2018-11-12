// Copyright (c) 2006-2018 Intel Corporation
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

#endif // __UMC_JPEG_DDI_H
