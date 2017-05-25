/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2017 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_commands.h"
#include "mfx_pts_based_activator.h"
#include "mfx_frame_based_activator.h"

//////////////////////////////////////////////////////////////////////////
//factory that uses prototypes
class ActivatorCommandsFactory : public ICommandsFactory
{
public:
    ActivatorCommandsFactory( ICommandActivator *pPrototype = NULL
        , ICommandInitializer *pInitializer = NULL);

    virtual ICommandActivator * CreateCommand(int nPosition);

protected:
    std::auto_ptr<ICommandActivator>  m_pPrototype;
    std::auto_ptr<ICommandInitializer>  m_pInitializer;
};

//creates constant number of samples
class constnumFactory : public ActivatorCommandsFactory
{
public :
    constnumFactory( int nNumber
        , ICommandActivator * pPrototype = NULL
        , ICommandInitializer *pInitializer = NULL);

    virtual ICommandActivator * CreateCommand(int nPosition);

protected:
    int m_nSamples;
};

//////////////////////////////////////////////////////////////////////////
//concrete visitors
//TODO: rename
class baseCmdsInitializer : public ICommandInitializer
{
public: 
    baseCmdsInitializer(
          mfxU32 nActivatedFrame
        , double fWarminUpTime
        , double fSeekToTime
        , int    nMaxSkipLevel
        , const mfx_shared_ptr<IRandom> & pRand
        , mfxVideoParam * pResetEncParam = NULL
        , mfxVideoParam * pMaskedEncParam = NULL
        , mfxExtBuffer  * bufToAttach = NULL
        , mfxU32 nBufToRemove = 0
        , mfxU16 nExtBuf = 0);

    virtual bool Init(seekSourceCommand *pCmd);
    virtual bool Init(seekCommand *pCmd);
    virtual bool Init(skipCommand *pCmd);
    virtual bool Init(resetEncCommand* pCmd);
    virtual bool Init(addExtBufferCommand * pCmd);
    virtual bool Init(removeExtBufferCommand * pCmd);
    virtual bool Init(reopenFileCommand *pCmd);

    virtual bool Init(PTSBasedCommandActivator* pActivator);
    virtual bool Init(FrameBasedCommandActivator* pActivator);

protected:
    double          m_fWarminUpTime;
    double          m_fSeekToTime;
    int             m_nMaxSkipLevel;
    mfxU32          m_nActivatedFrame;
    mfxU32          m_nBufferToRemove;
    mfxVideoParam   m_ResetParams;
    mfxVideoParam   m_maskedParams;
    mfxExtBuffer   *m_pBufferToAttach; 
    std::vector<char> m_bufferToAttachData;
    mfx_shared_ptr<IRandom> m_Randomizer;
};

class randActivationInitializer : public baseCmdsInitializer
{
public:
    randActivationInitializer( double fWarminUpTime
        , double fSeekToTime
        , int    nMaxSkipLevel
        , const mfx_shared_ptr<IRandom> & pRand
        , mfxVideoParam * pResetEncParam = NULL
        , mfxVideoParam * pMaskedEncParam = NULL);

    virtual bool Init(PTSBasedCommandActivator * pCmd);
};

class randSeekCmdInitializer : public randActivationInitializer
{
public: 
    randSeekCmdInitializer( double fWarminUpTime
        , int    nMaxSkipLevel
        , const mfx_shared_ptr<IRandom> & pRand);

    virtual bool Init(seekCommand *pCmd);
};


class seekFrameCmdInitializer : public baseCmdsInitializer
{
public:
    seekFrameCmdInitializer(int nActivatedFrame, int seek_to_pos, const mfx_shared_ptr<IRandom> & pRand);
    virtual bool Init(seekSourceCommand *pCmd);
    virtual bool Init(FrameBasedCommandActivator* pActivator);

protected:
    int m_nCurrentActivatedFrame;
    int m_nSeekTo;
};
