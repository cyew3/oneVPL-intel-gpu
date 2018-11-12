// Copyright (c) 2008-2018 Intel Corporation
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

#ifndef _MFX_H264_EX_PARAM_BUF_H_
#define _MFX_H264_EX_PARAM_BUF_H_

#include "mfxvideo.h"

/*------------------- CUCs definition ---------------------------------------*/

/*
    MFX_CUC_VC1_FRAMELABEL
    FrameCUC Extended buffer
    Buffer for storing indexes of inner ME frames.
    Not necessary. Used only for performance optimization.
    Structure: ExtVC1FrameLabel
    MFX_LABEL_FRAMELIST
*/
#define  MFX_CUC_H264_FRAMELABEL MFX_MAKEFOURCC('C','U','1','6')


/*--------------- Additional structures ---------------------------------------*/

typedef struct
{
    mfxI16 index;
    mfxU16 coef;
} MFX_H264_Coefficient;

/*---------Extended buffer descriptions ----------------------------------------*/

struct ExtH264ResidCoeffs
{
    mfxU32         CucId; // MFX_CUC_VC1_FRAMELABEL
    mfxU32         CucSz;
    mfxU32         numIndexes;
    MFX_H264_Coefficient* Coeffs;
};

#if 0

#define MFX_LABEL_CUCREFLIST 40
#define MFX_CUC_AVC_CUC_REFLIST MFX_MAKEFOURCC('C','U','1','9')

typedef struct {
  mfxU16 horz;
  mfxU16 vert;
} MFX_MV;

struct ExtH264MVValues
{
    mfxU32         CucId; // MFX_CUC_VC1_FRAMELABEL
    mfxU32         CucSz;
    mfxU16         numIndexes;
    MFX_MV* mvs;
};

struct AvcReferenceParamSlice
{
    mfxU32    CucId;
    mfxU32    CucSz;
    mfxU16    FirstMBInSlice;
    mfxU16    NumMb;
    mfxU8     RefPicList[2][32];
    mfxI8     Weights[2][32][3][2];
    //mfxU16    RefPicPoc[2][16][2]; //L0/L1, 16 refs, top/bottom field
};

struct mfxFrameReferenceCUC
{
    mfxVersion              Version;
    //mfxU16                  POC[2]; // top/bottom field

    union{
        mfxU8               CodecFlags;
        struct {
            mfxU8           FieldPicFlag    :1;
            mfxU8           MbaffFrameFlag  :1;

            mfxU8           IsTopShortRef : 1;
            mfxU8           IsBottomShortRef : 1;
            mfxU8           IsTopLongRef : 1;
            mfxU8           IsBottomLongRef : 1;
        };
    };

    mfxU16                  NumMb;
    mfxU16                  NumSlice[2];
    mfxMbParam              *MbParam;
    ExtH264MVValues         *MvData;   // needed of we have sub partitions

    mfxExtAvcDxvaPicParam   PicParam[2];
    AvcReferenceParamSlice  *RefList[2]; //pointer to array with size equal to number of slices

    mfxU32 FrameNum;
    mfxU32 LongFrameNum;
};

typedef struct _mfxExtCUCRefList {
    mfxU32 CucId;
    mfxU32 CucSz;
    mfxFrameReferenceCUC* Refs[20];
} mfxExtCUCRefList;

#endif

#endif //_MFX_H264_EX_PARAM_BUF_H_
