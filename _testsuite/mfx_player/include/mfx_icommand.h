/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#pragma once

#include "mfx_icommand_visitor.h"
#include "mfx_iclonebale.h"

//simple interface for Commands
class ICommand : public ICloneable
{
    DECLARE_CLONE(ICommand);
public:
    virtual ~ICommand() {}
    virtual mfxStatus    Execute()  = 0;;
    virtual bool         Accept(ICommandInitializer * ) = 0;
};

//semantic of accept not changed for derived commands, however type of caller is changed, 
//so it should be directly implemented for every enheritance
#define IMPLEMENT_ACCEPT()\
public:\
bool Accept(ICommandInitializer * pVisitor)\
{\
    if (NULL != pVisitor) \
        return pVisitor->Init(this);\
    return false;\
}

//helper class that manages invocation of actual command at necessary time frame
class ICommandActivator : public ICommand
{
    //cannot not use autoptr<ICommand> for holding cloneable references if ICommand* ICommand::clone() not existed
    //auto_ptr<ICommand> is not covariant for auto_ptr<ICloneable> however following code is compiled normally
    //std::auto_ptr<ICommand> pCmd(m_pPrototype->Clone()); and resulted to incorrectly initialized pCmd
    //http://stackoverflow.com/questions/4199556/usefulness-of-covariant-return-types-in-c-clone-idiom
    DECLARE_CLONE(ICommandActivator);

public:
    
    //triggers command start event for pts based commands gettime() invoked at this stage
    virtual bool         Activate()  = 0;
    //triggers command ir ready for execution - 
    virtual bool         isReady()  = 0;
    
    virtual void         Reset() = 0;
    //TODO: need to remove this
    virtual bool         isFinished() = 0;
    //dont want to wait any more 
    virtual void         MarkAsReady() = 0;
    //indicates that this command shouldn't be removed from command list
    virtual bool         isRepeat() = 0;
    virtual void         NotifyEOS() = 0;
};

//////////////////////////////////////////////////////////////////////////
//factory that creates appropriate commands
class ICommandsFactory
{
public:
    virtual ~ICommandsFactory(){}
    virtual ICommand * CreateCommand(int nPosition) = 0;
};


