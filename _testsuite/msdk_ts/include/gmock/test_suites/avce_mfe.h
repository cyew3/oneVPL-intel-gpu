/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2018 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "ts_encoder.h"


namespace avce_mfe
{

enum QueryMode
{
    NullIn = 0,
    InOut  = 1,
};

enum COpt {
    COptNone           = 0,
    COptMAD            = 1 << 0,
    COptIntRefType     = 1 << 1,
    COptMBQP           = 1 << 2,
    COptDisableSkipMap = 1 << 3,

    COpt2All = COptMAD | COptIntRefType,
    COpt3All = COptMBQP | COptDisableSkipMap,
};

typedef struct
{
    mfxU32    func;
    mfxStatus sts;

    mfxU16    width;
    mfxU16    height;
    mfxU16    cropW;
    mfxU16    cropH;

    mfxU16    numSlice;
    mfxU16    cOpt;

    mfxU16    inMFMode;
    mfxU16    inMaxNumFrames;
    mfxU16    expMFMode;
    mfxU16    expMaxNumFrames;
} tc_struct;


extern tc_struct test_case[];
extern size_t test_case_num;

void set_params(tsVideoEncoder& enc, tc_struct& tc,
        tsExtBufType<mfxVideoParam>& pout);

}

