/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_commands.h"
#include "mfx_pts_based_activator.h"

//////////////////////////////////////////////////////////////////////////

PTSBasedCommandActivator::PTSBasedCommandActivator(ICommand * pActual, IPipelineControl *pControl)
: BaseCommandActivator(pActual, pControl)
{
    Reset();
    m_fWarmingUpTime =  0;
    m_fStartTime     = -1;
    m_fLifeTime      =  0;
    m_pipelineTime   =  0;
    
    if (NULL != pControl)
    {
        pControl->GetTimer(&m_pipelineTime);
    }
}

PTSBasedCommandActivator::PTSBasedCommandActivator(const PTSBasedCommandActivator & that)
: BaseCommandActivator(that)
{
    Reset();
    m_fWarmingUpTime =  0;
    m_fStartTime     = -1;
    m_fLifeTime      =  0;
    m_pipelineTime   =  that.m_pipelineTime;
}

bool PTSBasedCommandActivator::isReady()
{
    if (BaseCommandActivator::isReady())
        return true;

    if (0.0 >= m_fWarmingUpTime)
    {
        BaseCommandActivator::MarkAsReady();
        return true;
    }

    if (-1 == m_fActivating )
        return false;

    bool bisReady = (m_pipelineTime->Current() - m_fActivating) >= m_fWarmingUpTime;
    
    if (bisReady)
    {
        BaseCommandActivator::MarkAsReady();
    }

    return bisReady;
}

bool PTSBasedCommandActivator::Activate()
{
    if (BaseCommandActivator::isReady())
        return true;

    //means direct PTSBasedCommandActivator doesn't matter what is current time in pipeline
    if (0.0 >= m_fWarmingUpTime)
        return true;

    if (-1 != m_fActivating)
        return true;

    m_fActivating = m_pipelineTime->Current();
    
    if (-1 == m_fActivating)
        return false;
    
    return true;
}

mfxStatus PTSBasedCommandActivator::Execute()
{
    if (!BaseCommandActivator::isReady())
    {
        if (-1 == m_fActivating && m_fWarmingUpTime > 0.0)
            return MFX_ERR_NONE;
    }
    
    return BaseCommandActivator::Execute();
}

void PTSBasedCommandActivator::Reset()
{
    BaseCommandActivator::Reset();
    m_fActivating = -1;
}

void PTSBasedCommandActivator::SetWarmingUpTime(const mfxF64 &fVal)
{
    m_fWarmingUpTime = fVal;
}

void PTSBasedCommandActivator::SetStartTime(const mfxF64 &fVal)
{
    m_fStartTime = fVal;
}

void PTSBasedCommandActivator::SetLifeTime(const mfxF64 &fVal)
{
    m_fLifeTime = fVal;
}
