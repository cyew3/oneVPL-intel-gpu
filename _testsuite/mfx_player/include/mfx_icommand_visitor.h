/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

//each command knows what method it should Init
class seekCommand;
class skipCommand;
class seekSourceCommand;
class resetEncCommand;
class addExtBufferCommand;
class reopenFileCommand;
class PTSBasedCommandActivator;
class FrameBasedCommandActivator;
class removeExtBufferCommand;

/// Implemented by visitor pattern
class ICommandInitializer
{
public: 
    virtual ~ICommandInitializer(){}
    virtual bool Init(seekSourceCommand *pCmd) = 0;
    virtual bool Init(seekCommand *pCmd)       = 0;
    virtual bool Init(skipCommand *pCmd)       = 0;
    virtual bool Init(resetEncCommand* pCmd)   = 0;
    virtual bool Init(addExtBufferCommand *pCmd) = 0;
    virtual bool Init(removeExtBufferCommand *pCmd) = 0;
    virtual bool Init(reopenFileCommand *pCmd) = 0;
    
    //visiting of activators
    virtual bool Init(FrameBasedCommandActivator * pActivator) = 0;
    virtual bool Init(PTSBasedCommandActivator * pActivator)   = 0;
};