/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_icommand.h"
#include "mfx_ipipeline_control.h"

#define IMPLEMENT_ACTIVATOR_ACCEPT()\
bool Accept(ICommandInitializer * pVisitor)\
{\
    if (pVisitor->Init(this))\
    {\
        return m_pActual->Accept(pVisitor);\
    }\
    return false;\
}

class BaseCommandActivator : public ICommandActivator
{
public:
    BaseCommandActivator(ICommand * pActual, IPipelineControl *pControl, bool isRepeatable = false)
        : m_pActual(pActual)
        , m_pControl(pControl)
        , m_bReady()
        , m_bExecuted()
        , m_bIsRepeatable(isRepeatable)
    {
    };

    //ICommand
    virtual mfxStatus    Execute()
    {
        if (!isReady())
            return MFX_ERR_NONE;

        m_bExecuted = true;

        return m_pActual->Execute();
    }

    //iCommandsActivator
    virtual bool         isReady(){return m_bReady||m_bExecuted;}
    virtual bool         Activate(){return true;}
    virtual void         Reset(){
        m_bReady = false; 
        m_bExecuted = false;
        OnRepeat();
    }
    virtual bool         isFinished(){return m_bExecuted;}
    virtual void         NotifyEOS() {m_bIsRepeatable = false;}
    virtual bool         isRepeat(){return m_bIsRepeatable;}
    virtual void         MarkAsReady(){m_bReady = true;}

protected:
    //can also acces base members m_isRepeatable to control repeat times 
    virtual void OnRepeat() {}
    //copy ctor available thru Clone
    BaseCommandActivator(const BaseCommandActivator & that)
        : m_pActual(NULL != that.m_pActual.get() ?  that.m_pActual->Clone() : NULL)
        , m_pControl(that.m_pControl)
        , m_bReady()
        , m_bExecuted()
        , m_bIsRepeatable(that.m_bIsRepeatable)
    {
    }

    std::auto_ptr<ICommand> m_pActual;
    IPipelineControl      * m_pControl;

private:
    bool                    m_bReady;
    bool                    m_bExecuted;
    bool                    m_bIsRepeatable;
};
