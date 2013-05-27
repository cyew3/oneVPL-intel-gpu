/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_factory_default.h"

class MFXPipelineTrancodeFactory 
    : public MFXPipelineFactory
{
public:
    virtual IMFXVideoRender * CreateRender( IPipelineObjectDesc * pParams);
};