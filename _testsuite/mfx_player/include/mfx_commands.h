/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2017 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_COMMANDS_H
#define __MFX_COMMANDS_H

#pragma once 

#include "mfx_decoder.h"
#include "mfx_ivpp.h"
#include "test_statistics.h"
#include "mfx_ivideo_render.h"

//#include "mfx_io_utils.h"
#include "mfx_itime.h"
#include "mfx_ipipeline_control.h"



#include "mfx_icommand.h"
#include "mfx_extended_buffer.h"

//hold pipeline control only
class commandBase : public ICommand
{
    DECLARE_CLONE(commandBase);
public:
    commandBase(IPipelineControl *pControl = NULL)
        : m_pControl(pControl){}
protected:
    IPipelineControl * m_pControl;
};

//////////////////////////////////////////////////////////////////////////
// only source file seeking to allow memory testing during long time without reseting
class seekSourceCommand : public commandBase
{
    IMPLEMENT_ACCEPT();
    IMPLEMENT_CLONE(seekSourceCommand);
public:
    seekSourceCommand(IPipelineControl *pHolder = NULL);

    virtual mfxStatus    Execute();
    virtual void         SetSeekTime(const mfxF64 &); 
    virtual void         SetSeekPercent(const mfxF64 &);
    virtual void         SetSeekFrames(const mfxU32 &);

protected:
    enum eSeekType
    {
        SEEK_NOT_SET = 0,
        SEEK_TIME,
        SEEK_PERCENT,
        SEEK_NUM_FRAME
    }m_seekType;
    mfxF64  m_fSeekTime;   //time to seek to
    mfxF64  m_fSeekPercent;//absolute position in percents
    mfxU32  m_uiFramesOffset;//offset to seek in frames
};


//////////////////////////////////////////////////////////////////////////
//represents reposition PTSBasedCommandActivator
class seekCommand : public seekSourceCommand
{
    IMPLEMENT_ACCEPT();
    IMPLEMENT_CLONE(seekCommand);
public:
    seekCommand(IPipelineControl *pHolder = NULL);
    virtual mfxStatus    Execute();
};

//////////////////////////////////////////////////////////////////////////
//represents skipping of video frames
class skipCommand : public commandBase
{
    IMPLEMENT_ACCEPT();
    IMPLEMENT_CLONE(skipCommand);    
public:
    skipCommand(IPipelineControl *pHolder = NULL);

    virtual mfxStatus    Execute();
    virtual void         SetSkipLevel(const mfxI32 &); 
protected:

    mfxI32  m_iSkipLevel;      //skip level to set
};

class resetEncCommand : public commandBase
{
    IMPLEMENT_ACCEPT();
    IMPLEMENT_CLONE(resetEncCommand);
public:
    resetEncCommand(IPipelineControl *pHolder = NULL);

    virtual mfxStatus    Execute();
    virtual void         SetResetParams(mfxVideoParam * vParam, mfxVideoParam * vParamMask);
    virtual void         SetResetFileName(const tstring &new_file_name);
    virtual void         SetResetInputFileName(const tstring &new_file_name);
    virtual void         SetVppResizing(bool bUseResize);
protected:

    mfxVideoParam        m_NewParams;//encoder will be reset with this parameters
    mfxVideoParam        m_NewParamsMask;//using mask
    tstring              m_NewFileName;
    tstring              m_NewInputFileName;
    bool                 m_bUseResizing;
};

class addExtBufferCommand : public commandBase
{
    IMPLEMENT_ACCEPT();
    IMPLEMENT_CLONE(addExtBufferCommand);

public:
    addExtBufferCommand(IPipelineControl *pHolder);
    
    //ICommand
    virtual mfxStatus    Execute();
    virtual void         RegisterExtBuffer(const mfxExtBuffer & refBuf );

protected:
    MFXExtBufferVector m_buf;
};

class removeExtBufferCommand : public commandBase
{
    IMPLEMENT_ACCEPT();
    IMPLEMENT_CLONE(removeExtBufferCommand);

public:
    removeExtBufferCommand(IPipelineControl *pHolder) 
        : commandBase(pHolder)
        , m_nBufferToRemove(){
    }

    //ICommand
    virtual mfxStatus    Execute();
    virtual void         RegisterExtBuffer(int nBufferIdToRemove) {
        m_nBufferToRemove = nBufferIdToRemove;
    }

protected:
    mfxU32 m_nBufferToRemove;
};


class reopenFileCommand : public commandBase
{
    IMPLEMENT_ACCEPT();
    IMPLEMENT_CLONE(reopenFileCommand);
public:
    reopenFileCommand(IPipelineControl * pHolder)
        : commandBase(pHolder)
    {
    }
    //ICommand
    virtual mfxStatus Execute()
    {
        IMFXVideoRender *pRender = NULL;
        IFile *pFile;
        MFX_CHECK_STS(m_pControl->GetRender(&pRender));
        MFX_CHECK_STS(pRender->GetDownStream(&pFile));
        MFX_CHECK_STS(pFile->Reopen());

         return MFX_ERR_NONE;
    }
};

#endif//__MFX_COMMANDS_H