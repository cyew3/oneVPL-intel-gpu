/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

class IStateContext;

class IStateData
{
public:
    virtual ~IStateData() {}
};

class IState
{
public:
    virtual ~IState(){}
    virtual mfxStatus Handle(IStateContext*, IStateData *) = 0;
};

///Context That has states

class IStateContext
{
public:
    virtual ~IStateContext(){}
    virtual mfxStatus SetState(IState * ) = 0;
};
