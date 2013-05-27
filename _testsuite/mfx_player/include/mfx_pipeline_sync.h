/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ipipeline_sync.h"

//used for shared access to non threadsafety parts of pipeline
class PipelineSynhro
    : public IPipelineSynhro 
{
public:
    virtual void Enter()
    {
        m_synhro.Lock();
    }
    virtual void Leave()
    {
        m_synhro.Unlock();
    }

protected:
    UMC::Mutex m_synhro;
};
