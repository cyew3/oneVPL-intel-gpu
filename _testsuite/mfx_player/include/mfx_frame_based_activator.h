/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2020 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_activator_base.h"

class FrameBasedCommandActivator : public BaseCommandActivator
{
    IMPLEMENT_ACTIVATOR_ACCEPT();
    IMPLEMENT_CLONE(FrameBasedCommandActivator);
public:

    FrameBasedCommandActivator(ICommand * pActual, IPipelineControl *pControl, bool isRepeatable = false)
        : BaseCommandActivator(pActual, pControl, isRepeatable)
        , m_nExecuteAtInitial()
        , m_nExecuteAt()
    {}

    virtual bool isReady()
    {
        if (BaseCommandActivator::isReady())
            return true;

        return m_pControl->GetNumDecodedFrames() >= m_nExecuteAt;
    }

    virtual void SetExecuteFrameNumber(mfxU32 nFrameOrder)
    {
        m_nExecuteAt = m_nExecuteAtInitial = nFrameOrder;
    }
    
    virtual void OnRepeat()
    {
        m_nExecuteAt += m_nExecuteAtInitial;
    }
protected:
    mfxU32 m_nExecuteAtInitial;
    mfxU32 m_nExecuteAt;
};

