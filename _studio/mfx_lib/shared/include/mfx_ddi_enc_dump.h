//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2010 Intel Corporation. All Rights Reserved.
//

typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_H264 ENCODE_SET_SEQUENCE_PARAMETERS_H264;
typedef struct tagENCODE_SET_PICTURE_PARAMETERS_H264  ENCODE_SET_PICTURE_PARAMETERS_H264;
typedef struct tagENCODE_SET_SLICE_HEADER_H264        ENCODE_SET_SLICE_HEADER_H264;

void ddiDumpH264SPS(FILE *f,
                    ENCODE_SET_SEQUENCE_PARAMETERS_H264* sps);

void ddiDumpH264PPS(FILE *f,
                    ENCODE_SET_PICTURE_PARAMETERS_H264* pps);

void ddiDumpH264SliceHeader(FILE *f,
                            ENCODE_SET_SLICE_HEADER_H264* sh,
                            int NumSlice);

void ddiDumpH264MBData(FILE     *fText,
                       mfxI32   _PicNum,
                       mfxI32   PicType,
                       mfxI32   PicWidth,
                       mfxI32   PicHeight,
                       mfxI32   NumSlicesPerPic,
                       mfxI32   NumMBs,
                       void     *pMBData,
                       void     *pMVData);
