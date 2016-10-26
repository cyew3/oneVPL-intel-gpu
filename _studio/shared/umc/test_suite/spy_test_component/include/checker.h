//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __CHECKER_H__
#define __CHECKER_H__

#include <memory>
#include "outline.h"

class Checker : public CheckerBase
{
public:
    virtual void CheckSequenceInfo(const UMC::BaseCodecParams *info1, const UMC::BaseCodecParams *info2);
    virtual void CheckFrameInfo(const UMC::MediaData *in1, const UMC::MediaData *in2);
};

#endif //__CHECKER_H__
