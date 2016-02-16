/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_commands_initializer.h"

ActivatorCommandsFactory::ActivatorCommandsFactory( ICommandActivator *pPrototype
                                     , ICommandInitializer *pInitializer)
    : m_pPrototype(pPrototype)
    , m_pInitializer(pInitializer)
{
}

ICommandActivator * ActivatorCommandsFactory::CreateCommand(int /*nPosition*/)
{
    if  (NULL == m_pPrototype.get())
        return NULL;

    std::auto_ptr<ICommandActivator> pCmd(m_pPrototype->Clone());

    if (!pCmd->Accept(m_pInitializer.get()))
    {
        return NULL;
    }

    return pCmd.release();
}

//////////////////////////////////////////////////////////////////////////

constnumFactory::constnumFactory( int nNumber
                                 , ICommandActivator * pPrototype
                                 , ICommandInitializer *pInitializer)
    : ActivatorCommandsFactory (pPrototype, pInitializer)
{ 
    m_nSamples = nNumber;
}

ICommandActivator * constnumFactory::CreateCommand(int nPosition)
{
    if (nPosition>=0 && nPosition < m_nSamples)
        return ActivatorCommandsFactory::CreateCommand(nPosition);

    return NULL;
}

//////////////////////////////////////////////////////////////////////////

baseCmdsInitializer::baseCmdsInitializer( mfxU32 nActivatedFrame
                                        , double fWarminUpTime
                                        , double fSeekToTime
                                        , int    nMaxSkipLevel
                                        , const mfx_shared_ptr<IRandom> & pRand
                                        , mfxVideoParam * pResetEncParam
                                        , mfxVideoParam * pMaskedEncParam
                                        , mfxExtBuffer  * bufToAttach
                                        , mfxU32 nBufToRemove)
    : m_fWarminUpTime(fWarminUpTime)
    , m_fSeekToTime(fSeekToTime)
    , m_nMaxSkipLevel(nMaxSkipLevel)
    , m_nActivatedFrame(nActivatedFrame)
    , m_nBufferToRemove(nBufToRemove)
    , m_ResetParams()
    , m_maskedParams()
    , m_pBufferToAttach(bufToAttach)
    , m_Randomizer(pRand)
{
    if (NULL != pResetEncParam)
    {
        memcpy(&m_ResetParams, pResetEncParam, sizeof(*pResetEncParam));
    }

    if (NULL != pMaskedEncParam)
    {
        memcpy(&m_maskedParams, pMaskedEncParam, sizeof(*pMaskedEncParam));
    }
    if (NULL != bufToAttach) {
        m_bufferToAttachData.resize(bufToAttach->BufferSz);
        m_pBufferToAttach = (mfxExtBuffer*)&m_bufferToAttachData.front();
        memcpy(m_pBufferToAttach, bufToAttach, bufToAttach->BufferSz);
    }
}

bool baseCmdsInitializer::Init(removeExtBufferCommand * pCmd) {
    pCmd->RegisterExtBuffer(m_nBufferToRemove);
    return true;
}

bool baseCmdsInitializer::Init(seekSourceCommand *pCmd)
{
    pCmd->SetSeekTime(m_fSeekToTime);
    return true;
}

bool baseCmdsInitializer::Init(seekCommand *pCmd)
{
    pCmd->SetSeekTime(m_fSeekToTime);
    return true;
}

bool baseCmdsInitializer::Init(skipCommand *pCmd)
{
    int level = 0;

    if (m_nMaxSkipLevel > 0)
        while (0 == (level = (int)(((double)m_Randomizer->rand() * m_nMaxSkipLevel)/((double)m_Randomizer->rand_max())) - m_nMaxSkipLevel / 2));

    pCmd->SetSkipLevel(level);
    return true;
}

bool baseCmdsInitializer::Init(resetEncCommand* pCmd)
{
    pCmd->SetResetParams(&m_ResetParams, &m_maskedParams);

    return true;
}

bool baseCmdsInitializer::Init(addExtBufferCommand * pCmd)
{
    pCmd->RegisterExtBuffer((mfxExtBuffer&)*m_pBufferToAttach);
    return true;
}

bool baseCmdsInitializer::Init(reopenFileCommand * /*pCmd*/)
{
    return true;
}

bool baseCmdsInitializer::Init(PTSBasedCommandActivator* pActivator)
{
    pActivator->SetWarmingUpTime(m_fWarminUpTime);
    return true;
}

bool baseCmdsInitializer::Init(FrameBasedCommandActivator* pActivator)
{
    pActivator->SetExecuteFrameNumber(m_nActivatedFrame);
    return true;
}

//////////////////////////////////////////////////////////////////////////

randActivationInitializer::randActivationInitializer( double fWarminUpTime
                                                    , double fSeekToTime
                                                    , int    nMaxSkipLevel
                                                    , const mfx_shared_ptr<IRandom> & pRand
                                                    , mfxVideoParam * pResetEncParam
                                                    , mfxVideoParam * pMaskedEncParam)
    : baseCmdsInitializer(0, fWarminUpTime, fSeekToTime, nMaxSkipLevel, pRand, pResetEncParam, pMaskedEncParam)
{
}

bool randActivationInitializer::Init(PTSBasedCommandActivator* pCmd)
{
    pCmd->SetWarmingUpTime(((double)m_Randomizer->rand())/((double)m_Randomizer->rand_max()) * (m_fWarminUpTime));
    return true;
}
//////////////////////////////////////////////////////////////////////////

randSeekCmdInitializer::randSeekCmdInitializer( double fWarminUpTime
                                              , int    nMaxSkipLevel
                                              , const mfx_shared_ptr<IRandom> & pRand)
                                              : randActivationInitializer(fWarminUpTime, 0.0, nMaxSkipLevel, pRand, NULL)
{
}

bool randSeekCmdInitializer::Init(seekCommand *pCmd)
{        
    pCmd->SetSeekPercent(((double)m_Randomizer->rand())/((double)m_Randomizer->rand_max()));
    return true;
}

//////////////////////////////////////////////////////////////////////////

seekFrameCmdInitializer::seekFrameCmdInitializer(int nActivatedFrame, int seek_to_pos, const mfx_shared_ptr<IRandom> & pRand)
    : baseCmdsInitializer(nActivatedFrame, 0, 0, 0, pRand)
    , m_nSeekTo(seek_to_pos)
{
    m_nCurrentActivatedFrame = m_nActivatedFrame;
}

bool seekFrameCmdInitializer::Init(seekSourceCommand *pCmd)
{
    pCmd->SetSeekFrames(m_nSeekTo);
    return true;
}

bool seekFrameCmdInitializer::Init(FrameBasedCommandActivator* pActivator)
{
    pActivator->SetExecuteFrameNumber(m_nCurrentActivatedFrame);
    m_nCurrentActivatedFrame += m_nActivatedFrame;
    
    return true;
}
