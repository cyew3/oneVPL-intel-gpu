/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#pragma once

#include "mfx_activator_base.h"

class PTSBasedCommandActivator : public BaseCommandActivator 
{
    IMPLEMENT_ACTIVATOR_ACCEPT();
    IMPLEMENT_CLONE(PTSBasedCommandActivator);
public:
    PTSBasedCommandActivator(ICommand * pActual, IPipelineControl *pControl);

    //TODO: execute is candidate to move to base
    //ICommand
    virtual mfxStatus    Execute();
    
    //iCommandsActivator
    virtual bool         isReady();
    virtual bool         Activate();
    virtual void         Reset();

    //custom methods
    virtual void         SetWarmingUpTime(const mfxF64 &); 
    virtual void         SetStartTime(const mfxF64 &); 
    virtual void         SetLifeTime(const mfxF64 &); 

protected:
    PTSBasedCommandActivator(const PTSBasedCommandActivator & That);

    mfxF64              m_fActivating;    //creation time
    ITime             * m_pipelineTime;   //times a time when PTSBasedCommandActivator was launched
    bool                m_bReady;
    mfxF64              m_fWarmingUpTime; //how long we should wait until execute
    mfxF64              m_fStartTime;     //time at which we should wait before execution
    mfxF64              m_fLifeTime;      //how long we should wait until next PTSBasedCommandActivator
};

