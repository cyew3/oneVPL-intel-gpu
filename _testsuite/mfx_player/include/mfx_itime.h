/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

class ITime
{
public:
    virtual ~ITime(){}
    virtual mfxU64 GetTick()         = 0;
    virtual mfxU64 GetFreq()         = 0;
    virtual mfxF64 Current()         = 0;
    virtual void   Wait(mfxU32 nMs)  = 0;
};

