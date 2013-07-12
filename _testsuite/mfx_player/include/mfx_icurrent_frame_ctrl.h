/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
/*
    Rationale: controls of attached extended buffer, and other settings on current frame only
*/
class ICurrentFrameControl 
{
public:
    virtual ~ICurrentFrameControl(){}
    //if NULL then donot attach any reflists
    virtual void AddExtBuffer(mfxExtBuffer &buffer) = 0;
    virtual void RemoveExtBuffer(mfxU32 mfxExtBufferId) = 0;
};
