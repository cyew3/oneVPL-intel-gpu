/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#include <algorithm>
#include <assert.h>
#include "math.h"

#include "libmfx_core.h"
#include "libmfx_core_interface.h"
#include "libmfx_core_factory.h"

#include "mfx_common_int.h"
#include "mfx_enc_common.h" // UMC::FrameRate

#include "mfx_vpp_utils.h"
#include "mfx_task.h"
#include "mfx_vpp_defs.h"
#include "mfx_vpp_hw.h"

using namespace MfxHwVideoProcessing;

enum 
{
    CURRENT_TIME_STAMP              = 100000,
    FRAME_INTERVAL                  = 10000,
    ACCEPTED_DEVICE_ASYNC_DEPTH     = 15, // 

};

enum QueryStatus
{
    VPREP_GPU_READY         =   0,
    VPREP_GPU_BUSY          =   1,
    VPREP_GPU_NOT_REACHED   =   2,
    VPREP_GPU_FAILED        =   3
};

enum 
{
    DEINTERLACE_ENABLE  = 0,
    DEINTERLACE_DISABLE = 1
};


////////////////////////////////////////////////////////////////////////////////////
// Utils
////////////////////////////////////////////////////////////////////////////////////

template<typename T> mfxU32 FindFreeSurface(std::vector<T> const & vec)
{
    for (size_t j = 0; j < vec.size(); j++)
    {
        if (vec[j].IsFree())
        {
            return (mfxU32)j;
        }
    }

    return NO_INDEX;

} // template<typename T> mfxU32 FindFreeSurface(std::vector<T> const & vec)


template<typename T> void Clear(std::vector<T> & v)
{
    std::vector<T>().swap(v);

} // template<typename T> void Clear(std::vector<T> & v)


//mfxU32 GetMFXFrcMode(mfxVideoParam & videoParam);

mfxStatus CheckIOMode(mfxVideoParam *par, VideoVPPHW::IOMode mode);
mfxStatus ConfigureExecuteParams(
    mfxVideoParam & videoParam, // [IN]
    mfxVppCaps & caps,          // [IN]

    mfxExecuteParams & executeParams,// [OUT]
    Config & config);           // [OUT]


mfxStatus CopyFrameDataBothFields(
        VideoCORE *          core,
        mfxFrameData /*const*/ & dst,
        mfxFrameData /*const*/ & src,
        mfxFrameInfo const & info);

////////////////////////////////////////////////////////////////////////////////////
// CpuFRC
////////////////////////////////////////////////////////////////////////////////////

mfxStatus CpuFrc::StdFrc::DoCpuFRC_AndUpdatePTS(
    mfxFrameSurface1 *input, 
    mfxFrameSurface1 *output, 
    mfxStatus *intSts)
{
    mfxF64 localDeltaTime = m_externalDeltaTime + m_timeFrameInterval;
    bool isIncreasedSurface = false;
    std::vector<mfxFrameSurface1 *>::iterator iterator;

    if (localDeltaTime <= -m_outFrameTime)
    {
        m_externalDeltaTime += m_inFrameTime;

        // skip frame
        // request new one input surface
        return MFX_ERR_MORE_DATA;
    }
    else if (localDeltaTime >= m_outFrameTime)
    {
        iterator = std::find(m_LockedSurfacesList.begin(), m_LockedSurfacesList.end(), input);

        if (m_LockedSurfacesList.end() == iterator)
        {
            m_LockedSurfacesList.push_back(input);
        }
        else
        {
            isIncreasedSurface = true;
        }

        m_externalDeltaTime += -m_outFrameTime;

        if (true == m_bDuplication)
        {
            // set output pts equal to -1, because of it is repeated frame
            output->Data.FrameOrder = (mfxU32) MFX_FRAMEORDER_UNKNOWN;
            output->Data.TimeStamp = (mfxU64) MFX_TIME_STAMP_INVALID;
        }
        else
        {
            // KW issue resolved
            if(NULL != input)
            {
                output->Data.TimeStamp = input->Data.TimeStamp;
                output->Data.FrameOrder = input->Data.FrameOrder;
            }
            else
            {                    
                output->Data.FrameOrder = (mfxU32) MFX_FRAMEORDER_UNKNOWN;
                output->Data.TimeStamp = (mfxU64) MFX_TIME_STAMP_INVALID;
            }
        }

        m_bDuplication = true;

        // duplicate this frame
        // request new one output surface
        *intSts = MFX_ERR_MORE_SURFACE;
    }
    else
    {
        iterator = std::find(m_LockedSurfacesList.begin(), m_LockedSurfacesList.end(), input);

        if (m_LockedSurfacesList.end() != iterator)
        {
            isIncreasedSurface = true;
            m_LockedSurfacesList.erase(m_LockedSurfacesList.begin());
        }

        m_externalDeltaTime += m_timeFrameInterval;
    }

    if (MFX_ERR_MORE_SURFACE != *intSts && NULL != input)
    {
        if (true == m_bDuplication)
        {
            output->Data.FrameOrder = (mfxU32) MFX_FRAMEORDER_UNKNOWN;
            output->Data.TimeStamp = (mfxU64) MFX_TIME_STAMP_INVALID;
            m_bDuplication = false;
        }
        else
        {
            output->Data.TimeStamp = input->Data.TimeStamp;
            output->Data.FrameOrder = input->Data.FrameOrder;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus CpuFrc::StdFrc::DoCpuFRC_AndUpdatePTS(...)


mfxStatus CpuFrc::PtsFrc::DoCpuFRC_AndUpdatePTS(
    mfxFrameSurface1 *input, 
    mfxFrameSurface1 *output, 
    mfxStatus *intSts)
{
    bool isIncreasedSurface = false;
    std::vector<mfxFrameSurface1 *>::iterator iterator;

    mfxU64 inputTimeStamp = input->Data.TimeStamp;

    if (false == m_bIsSetTimeOffset)
    {
        // set initial time stamp offset
        m_bIsSetTimeOffset = true;
        m_timeOffset = input->Data.TimeStamp;
    }

    // calculate expected timestamp based on output framerate

    m_expectedTimeStamp = (((mfxU64)m_numOutputFrames * m_frcRational[VPP_OUT].FrameRateExtD * MFX_TIME_STAMP_FREQUENCY) / m_frcRational[VPP_OUT].FrameRateExtN +
        m_timeOffset + m_timeStampJump);

    mfxU32 timeStampDifference = abs((mfxI32)(inputTimeStamp - m_expectedTimeStamp));

    /*
    // process timestamps jumps
    if (timeStampDifference > MAX_ACCEPTED_DIFFERENCE)
    {
    // jump was happened
    m_timeStampJump += input->Data.TimeStamp - m_prevInputTimeStamp;

    // recalculate expected timestamp according happened jump
    m_expectedTimeStamp = ((mfxU64) (m_numOutputFrames - 1) * m_params.vpp.Out.FrameRateExtD * MFX_TIME_STAMP_FREQUENCY) / m_params.vpp.Out.FrameRateExtN +
    m_timeOffset + m_timeStampJump;
    }
    */

    // process irregularity
    if (m_minDeltaTime > timeStampDifference)
    {
        inputTimeStamp = m_expectedTimeStamp;
    }

    // made decision regarding frame rate conversion
    if ((input->Data.TimeStamp != -1) && !m_bUpFrameRate && inputTimeStamp < m_expectedTimeStamp)
    {
        m_bDownFrameRate = true;

        // calculate output time stamp
        output->Data.TimeStamp = m_expectedTimeStamp;

        // skip frame
        // request new one input surface
        return MFX_ERR_MORE_DATA;
    }
    else if ((input->Data.TimeStamp != -1) && !m_bDownFrameRate && (input->Data.TimeStamp > m_expectedTimeStamp || m_timeStampDifference))
    {
        m_bUpFrameRate = true;

        if (inputTimeStamp <= m_expectedTimeStamp)
        {
            m_upCoeff = 0;
            m_timeStampDifference = 0;

            // manage locked surfaces
            iterator = std::find(m_LockedSurfacesList.begin(), m_LockedSurfacesList.end(), input);

            if (m_LockedSurfacesList.end() != iterator)
            {
                isIncreasedSurface = true;
                m_LockedSurfacesList.erase(m_LockedSurfacesList.begin());
            }

            output->Data.TimeStamp = m_expectedTimeStamp;

            // save current time stamp value
            m_prevInputTimeStamp = m_expectedTimeStamp;
        }
        else
        {
            // manage locked surfaces
            iterator = std::find(m_LockedSurfacesList.begin(), m_LockedSurfacesList.end(), input);

            if (m_LockedSurfacesList.end() == iterator)
            {
                m_LockedSurfacesList.push_back(input);
            }
            else
            {
                isIncreasedSurface = true;
            }

            // calculate timestamp increment
            if (false == m_timeStampDifference)
            {
                m_timeStampDifference = abs((mfxI32)(m_expectedTimeStamp - m_prevInputTimeStamp));
            }

            m_upCoeff += 1;

            // calculate output time stamp
            output->Data.TimeStamp = m_prevInputTimeStamp + m_timeStampDifference * m_upCoeff;

            // duplicate this frame
            // request new one output surface
            *intSts = MFX_ERR_MORE_SURFACE;
        }
    }
    else
    {
        // manage locked surfaces
        iterator = std::find(m_LockedSurfacesList.begin(), m_LockedSurfacesList.end(), input);

        if (m_LockedSurfacesList.end() != iterator)
        {
            isIncreasedSurface = true;
            m_LockedSurfacesList.erase(m_LockedSurfacesList.begin());
        }

        output->Data.TimeStamp = m_expectedTimeStamp;

        // save current time stamp value
        m_prevInputTimeStamp = m_expectedTimeStamp;
    }

    output->Data.FrameOrder = m_numOutputFrames;
    // increment number of output frames
    m_numOutputFrames += 1;

    return MFX_ERR_NONE;

} // mfxStatus CpuFrc::PtsFrc::DoCpuFRC_AndUpdatePTS(...)


mfxStatus CpuFrc::DoCpuFRC_AndUpdatePTS(
    mfxFrameSurface1 *input, 
    mfxFrameSurface1 *output, 
    mfxStatus *intSts)
{
    if(NULL == input) return MFX_ERR_MORE_DATA;
    if (FRC_STANDARD & m_frcMode) // standard FRC (input frames count -> output frames count)
    {
        return m_stdFrc.DoCpuFRC_AndUpdatePTS(input, output, intSts);
    }
    else // frame rate conversion by pts
    {
        return m_ptsFrc.DoCpuFRC_AndUpdatePTS(input, output, intSts);
    }

} // mfxStatus VideoVPPHW::DoCpuFRC_AndUpdatePTS(...)


////////////////////////////////////////////////////////////////////////////////////
// Gfx Resource Manager
////////////////////////////////////////////////////////////////////////////////////
mfxStatus ResMngr::Close(void) 
{
    ReleaseSubResource(true);

    Clear(m_surf[VPP_IN]);
    Clear(m_surf[VPP_OUT]);

    return MFX_ERR_NONE;

} // mfxStatus ResMngr::Close(void) 


mfxStatus ResMngr::Init(
    Config & config, 
    VideoCORE* core)
{
    if( config.m_IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        m_surf[VPP_IN].resize( config.m_surfCount[VPP_IN] );
    }

    if( config.m_IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        m_surf[VPP_OUT].resize( config.m_surfCount[VPP_OUT] );
    }

    m_bRefFrameEnable = config.m_bRefFrameEnable;

    if( (config.m_extConfig.mode & FRC_INTERPOLATION) ||
        (config.m_extConfig.mode & VARIANCE_REPORT) ||
        (config.m_extConfig.mode & COMPOSITE) )
    {
        mfxU32 totalInputFramesCount = config.m_extConfig.customRateData.bkwdRefCount + 
                                       1 + 
                                       config.m_extConfig.customRateData.fwdRefCount; 

        m_inputIndexCount   = totalInputFramesCount;
        m_outputIndexCountPerCycle = config.m_extConfig.customRateData.outputIndexCountPerCycle;
        m_bkwdRefCountRequired = config.m_extConfig.customRateData.bkwdRefCount;
        m_fwdRefCountRequired  = config.m_extConfig.customRateData.fwdRefCount;
        m_inputFramesOrFieldPerCycle = config.m_extConfig.customRateData.inputFramesOrFieldPerCycle;
        //m_core                 = core;
    }

    m_core                 = core;

    return MFX_ERR_NONE;

} // mfxStatus ResMngr::Init(...)


mfxStatus ResMngr::DoAdvGfx(
    mfxFrameSurface1 *input, 
    mfxFrameSurface1 *output, 
    mfxStatus *intSts)
{
    input;
    output;

    assert( m_inputIndex  <=  m_inputIndexCount);
    assert( m_outputIndex <   m_outputIndexCountPerCycle);

    if( m_bOutputReady )// MARKER: output is ready, new out surface is need only
    {
         m_outputIndex++;

        if(  m_outputIndex ==  m_outputIndexCountPerCycle )//MARKER: produce last frame for current Task Slot
        {
             // update all state
             m_bOutputReady= false; 
             m_outputIndex = 0;

            *intSts = MFX_ERR_NONE;
        }
        else
        {
            *intSts = MFX_ERR_MORE_SURFACE;
        }

         m_indxOutTimeStamp++;

        return MFX_ERR_NONE;
    }
    else // new task
    {   
        m_fwdRefCount = m_fwdRefCountRequired;

        if(input)
        {
            m_inputIndex++;

            mfxStatus sts = m_core->IncreaseReference( &(input->Data) );
            MFX_CHECK_STS(sts);
            
            ExtSurface surf;
            surf.pSurf = input;
            if(m_surf[VPP_IN].size() > 0) // input in system memory
            {
                surf.bUpdate = true;
                surf.resIdx = FindFreeSurface(m_surf[VPP_IN]);
                if(NO_INDEX == surf.resIdx) return MFX_WRN_DEVICE_BUSY;

                m_surf[VPP_IN][surf.resIdx].SetFree(false);// marks resource as "locked"
            }
            m_surfQueue.push_back(surf);
        }

        bool isEOSSignal = ((NULL == input) && (m_bkwdRefCount == m_bkwdRefCountRequired) && (m_inputIndex > m_bkwdRefCountRequired));

        if(isEOSSignal)
        {
            m_fwdRefCount = IPP_MAX(m_inputIndex - m_bkwdRefCount - 1, 0);
        }

        if( (m_inputIndex - m_bkwdRefCount == m_inputIndexCount - m_bkwdRefCountRequired) ||
            isEOSSignal) // MARKER: start new Task Slot
        {
             m_actualNumber = m_actualNumber + m_inputFramesOrFieldPerCycle; // for driver PTS
             m_outputIndex++;
             m_indxOutTimeStamp = 0;

            if(  m_outputIndex ==  m_outputIndexCountPerCycle ) // MARKER: produce last frame for current Task Slot
            {
                // update all state
                m_outputIndex = 0;
               
                *intSts = MFX_ERR_NONE;
            }
            else
            {
                m_bOutputReady = true;

                *intSts = MFX_ERR_MORE_SURFACE;
            }

             m_pSubResource =  CreateSubResource();

            return MFX_ERR_NONE;
        }
        //-------------------------------------------------
        // MARKER: task couldn't be started.
        else
        {
            *intSts = MFX_ERR_MORE_DATA;

            return MFX_ERR_MORE_DATA;
        }
    }

} // mfxStatus ResMngr::DoAdvGfx(...)


mfxStatus ResMngr::DoMode30i60p(
    mfxFrameSurface1 *input, 
    mfxFrameSurface1 *output, 
    mfxStatus *intSts)
{
    input;
    output;


    if( m_bOutputReady )// MARKER: output is ready, new out surface is need only
    {
         m_inputIndex++;

         m_bOutputReady = false;
         *intSts        = MFX_ERR_NONE;
         m_outputIndex = 0; // marker task end;

        return MFX_ERR_NONE;
    }
    else // new task
    {   
        m_outputIndex = 1; // marker new task;

        if(input)
        {
            if(0 == m_inputIndex)
            {
                *intSts = MFX_ERR_NONE;
                m_outputIndexCountPerCycle = 3;
                m_bkwdRefCount = 0;
            }
            else 
            {
                m_bOutputReady = true;
                *intSts = MFX_ERR_MORE_SURFACE;
                m_outputIndexCountPerCycle = 2;
                if(true == m_bRefFrameEnable)
                {
                    // need one backward reference to enable motion adaptive ADI
                    m_bkwdRefCount = 1;
                }
                else
                {
                    // no reference frame, use ADI with spatial info
                    m_bkwdRefCount = 0;
                }
            }

            mfxStatus sts = m_core->IncreaseReference( &(input->Data) );
            MFX_CHECK_STS(sts);
            
            ExtSurface surf;
            surf.pSurf = input;
            if(m_surf[VPP_IN].size() > 0) // input in system memory
            {
                surf.bUpdate = true;
                surf.resIdx = FindFreeSurface(m_surf[VPP_IN]);
                if(NO_INDEX == surf.resIdx) return MFX_WRN_DEVICE_BUSY;

                m_surf[VPP_IN][surf.resIdx].SetFree(false);// marks resource as "locked"
            }
            m_surfQueue.push_back(surf);
        
            if(1 != m_inputIndex)
            {
                m_pSubResource =  CreateSubResourceForMode30i60p();
            }

            m_inputIndex++;

            return MFX_ERR_NONE;
        }
        else
        {
            //return MFX_ERR_MORE_DATA;

            //assert()
            if(m_surfQueue.size() > 0)
            {
                *intSts = MFX_ERR_NONE;
                m_outputIndexCountPerCycle = 1;
                m_bkwdRefCount = 0;
                m_outputIndex  = 0; // marker task end;

                m_pSubResource =  CreateSubResourceForMode30i60p();

                return MFX_ERR_NONE;
            }
            else
            {
                return MFX_ERR_MORE_DATA;
            }
        }
    }

} // mfxStatus ResMngr::DoMode30i60p(...)


mfxStatus ResMngr::ReleaseSubResource(bool bAll)
{
    //-----------------------------------------------------
    // Release SubResource
    // (1) if all task have been completed (refCount == 0)
    // (2) if common Close()
    std::vector<ReleaseResource*> taskToRemove;
    mfxU32 i;
    for(i = 0; i < m_subTaskQueue.size(); i++)
    {
        if(bAll || (0 == m_subTaskQueue[i]->refCount) )
        {
            for(mfxU32 resIndx = 0; resIndx <  m_subTaskQueue[i]->surfaceListForRelease.size(); resIndx++ )
            {
                mfxStatus sts = m_core->DecreaseReference( &(m_subTaskQueue[i]->surfaceListForRelease[resIndx].pSurf->Data) );
                MFX_CHECK_STS(sts);

                mfxU32 freeIdx = m_subTaskQueue[i]->surfaceListForRelease[resIndx].resIdx;
                if(NO_INDEX != freeIdx && m_surf[VPP_IN].size() > 0)
                {
                    m_surf[VPP_IN][freeIdx].SetFree(true);
                }
            }
            taskToRemove.push_back( m_subTaskQueue[i] );
        }
    }

    size_t removeCount = taskToRemove.size();
    std::vector<ReleaseResource*>::iterator it;
    for(i = 0; i < removeCount; i++)
    {
        it = std::find(m_subTaskQueue.begin(), m_subTaskQueue.end(), taskToRemove[i] );
        if( it != m_subTaskQueue.end())
        {
            m_subTaskQueue.erase(it);
        }

        delete taskToRemove[i];
    }

    return MFX_ERR_NONE;

} // mfxStatus ResMngr::ReleaseSubResource(bool bAll)


ReleaseResource* ResMngr::CreateSubResource(void)
{
    // fill resource to remove after task slot completion
    ReleaseResource* subRes = new ReleaseResource;
    memset(subRes, 0, sizeof(ReleaseResource));

    subRes->refCount = m_outputIndexCountPerCycle;

    mfxU32 numFramesForRemove = GetNumToRemove();

    numFramesForRemove = IPP_MIN(numFramesForRemove, (mfxU32)m_surfQueue.size());
    
    for(mfxU32 i = 0; i < numFramesForRemove; i++)
    {
        subRes->surfaceListForRelease.push_back( m_surfQueue[i] );
    }
    m_subTaskQueue.push_back(subRes);

    return subRes;

} // ReleaseResource* ResMngr::CreateSubResource(void)


ReleaseResource* ResMngr::CreateSubResourceForMode30i60p(void)
{
    // fill resource to remove after task slot completion
    ReleaseResource* subRes = new ReleaseResource;
    memset(subRes, 0, sizeof(ReleaseResource));

    subRes->refCount = m_outputIndexCountPerCycle;

    mfxU32 numFramesForRemove = 1;
    
    for(mfxU32 i = 0; i < numFramesForRemove; i++)
    {
        subRes->surfaceListForRelease.push_back( m_surfQueue[i] );
    }
    m_subTaskQueue.push_back(subRes);

    return subRes;

} // ReleaseResource* ResMngr::CreateSubResourceForMode30i60p(void)


//---------------------------------------------------------
mfxStatus ResMngr::FillTaskForMode30i60p(
    DdiTask* pTask,
    mfxFrameSurface1 *pInSurface,
    mfxFrameSurface1 *pOutSurface)
{
    pInSurface;

    mfxU32 refIndx = 0;
        
    // bkwd
    pTask->bkwdRefCount = m_bkwdRefCount;//aya: we set real bkw frames
    mfxU32 actualNumber = m_actualNumber;
    for(refIndx = 0; refIndx < pTask->bkwdRefCount; refIndx++)
    {
        ExtSurface bkwdSurf = m_surfQueue[refIndx];
        bkwdSurf.timeStamp     = CURRENT_TIME_STAMP + actualNumber * FRAME_INTERVAL;
        bkwdSurf.endTimeStamp  = CURRENT_TIME_STAMP + (actualNumber + 1) * FRAME_INTERVAL;

        if(m_surf[VPP_IN].size() > 0) // input in system memory
        {
            if(m_surfQueue[refIndx].bUpdate)
            {
                m_surfQueue[refIndx].bUpdate = false;
                m_surf[VPP_IN][m_surfQueue[refIndx].resIdx].SetFree(false);
            }
        }
        //

        pTask->m_refList.push_back(bkwdSurf);
        actualNumber++;
    }

    // input
    {
        pTask->input = m_surfQueue[pTask->bkwdRefCount];
        pTask->input.timeStamp     = CURRENT_TIME_STAMP + actualNumber * FRAME_INTERVAL;
        pTask->input.endTimeStamp  = CURRENT_TIME_STAMP + (actualNumber + 1) * FRAME_INTERVAL;
        if(m_surf[VPP_IN].size() > 0) // input in system memory
        {
            if(m_surfQueue[pTask->bkwdRefCount].bUpdate)
            {
                m_surfQueue[pTask->bkwdRefCount].bUpdate = false;
                m_surf[VPP_IN][m_surfQueue[pTask->bkwdRefCount].resIdx].SetFree(false);
            }
        }
    }

    // output
    {
        pTask->output.pSurf     = pOutSurface;
        pTask->output.timeStamp = pTask->input.timeStamp;
        if(m_surf[VPP_OUT].size() > 0) // out in system memory
        {
            m_surf[VPP_OUT][pTask->output.resIdx].SetFree(false);
        }

        actualNumber++;
    }

    if(m_pSubResource)
    {
        pTask->pSubResource = m_pSubResource;
    }

    // MARKER: last frame in current Task Slot
    // after resource are filled we can update generall container and state
    if( m_outputIndex == 0 ) 
    {
        size_t numFramesToRemove = m_pSubResource->surfaceListForRelease.size();

        for(size_t i = 0; i < numFramesToRemove; i++)
        {
            m_surfQueue.erase( m_surfQueue.begin() );
        }

        // correct output pts for target output
        pTask->output.timeStamp = (pTask->input.timeStamp + pTask->input.endTimeStamp) >> 1;
    }

    // update 
    if(pTask->taskIndex > 0 && !(pTask->taskIndex & 1))
    {
        m_actualNumber++;
    }

    return MFX_ERR_NONE;

} // void ResMngr::FillTaskForMode30i60p(...)
//---------------------------------------------------------


mfxStatus ResMngr::FillTask(
    DdiTask* pTask,
    mfxFrameSurface1 *pInSurface,
    mfxFrameSurface1 *pOutSurface)
{
    pInSurface;

    mfxU32 refIndx = 0;
        
    // bkwd
    pTask->bkwdRefCount = m_bkwdRefCount;//aya: we set real bkw frames
    mfxU32 actualNumber = m_actualNumber;
    for(refIndx = 0; refIndx < pTask->bkwdRefCount; refIndx++)
    {
        ExtSurface bkwdSurf = m_surfQueue[refIndx];
        bkwdSurf.timeStamp     = CURRENT_TIME_STAMP + actualNumber * FRAME_INTERVAL;
        bkwdSurf.endTimeStamp  = CURRENT_TIME_STAMP + (actualNumber + 1) * FRAME_INTERVAL;

        if(m_surf[VPP_IN].size() > 0) // input in system memory
        {
            if(m_surfQueue[refIndx].bUpdate)
            {
                m_surfQueue[refIndx].bUpdate = false;
                m_surf[VPP_IN][m_surfQueue[refIndx].resIdx].SetFree(false);
            }
        }
        //

        pTask->m_refList.push_back(bkwdSurf);
        actualNumber++;
    }

    // input
    {
        pTask->input = m_surfQueue[pTask->bkwdRefCount];
        pTask->input.timeStamp     = CURRENT_TIME_STAMP + actualNumber * FRAME_INTERVAL;
        pTask->input.endTimeStamp  = CURRENT_TIME_STAMP + (actualNumber + 1) * FRAME_INTERVAL;
        if(m_surf[VPP_IN].size() > 0) // input in system memory
        {
            if(m_surfQueue[pTask->bkwdRefCount].bUpdate)
            {
                m_surfQueue[pTask->bkwdRefCount].bUpdate = false;
                m_surf[VPP_IN][m_surfQueue[pTask->bkwdRefCount].resIdx].SetFree(false);
            }
        }
    }

    // output
    {
        pTask->output.pSurf     = pOutSurface;
        pTask->output.timeStamp = pTask->input.timeStamp + m_indxOutTimeStamp * (FRAME_INTERVAL / m_outputIndexCountPerCycle);
        if(m_surf[VPP_OUT].size() > 0) // out in system memory
        {
            m_surf[VPP_OUT][pTask->output.resIdx].SetFree(false);
        }

        actualNumber++;
    }

    // fwd
    pTask->fwdRefCount = m_fwdRefCount;
    for(mfxU32 refIndx = 0; refIndx < pTask->fwdRefCount; refIndx++)
    {
        mfxU32 fwdIdx = pTask->bkwdRefCount + 1 + refIndx;
        ExtSurface fwdSurf = m_surfQueue[fwdIdx];
        fwdSurf.timeStamp     = CURRENT_TIME_STAMP + actualNumber * FRAME_INTERVAL;
        fwdSurf.endTimeStamp  = CURRENT_TIME_STAMP + (actualNumber + 1) * FRAME_INTERVAL;

        if(m_surf[VPP_IN].size() > 0) // input in system memory
        {
            if(fwdSurf.bUpdate)
            {
                m_surfQueue[fwdIdx].bUpdate = false;
                m_surf[VPP_IN][m_surfQueue[fwdIdx].resIdx].SetFree(false);
            }
        }
        pTask->m_refList.push_back(fwdSurf);
        actualNumber++;
    }

    if(m_pSubResource)
    {
        pTask->pSubResource = m_pSubResource;
    }

    // MARKER: last frame in current Task Slot
    // after resource are filled we can update generall container and state
    if( m_outputIndex == 0 ) 
    {
        size_t numFramesToRemove = m_pSubResource->surfaceListForRelease.size();

        for(size_t i = 0; i < numFramesToRemove; i++)
        {
            m_surfQueue.erase( m_surfQueue.begin() );
        }

        m_inputIndex -= (mfxU32)numFramesToRemove;
        m_bkwdRefCount = GetNextBkwdRefCount();
    }

    return MFX_ERR_NONE;

} // void ResMngr::FillTask(...)


mfxStatus ResMngr::CompleteTask(DdiTask *pTask)
{
    if(pTask->pSubResource)
    {
        pTask->pSubResource->refCount--;
    }

    mfxStatus sts =  ReleaseSubResource(false); // false mean release subResource with RefCnt == 0 only 
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus ResMngr::CompleteTask(DdiTask *pTask)


////////////////////////////////////////////////////////////////////////////////////
// TaskManager
////////////////////////////////////////////////////////////////////////////////////

TaskManager::TaskManager() 
{     
    m_mode30i60p.m_numOutputFrames    = 0;
    m_mode30i60p.m_prevInputTimeStamp = 0;

    m_taskIndex = 0;
    m_actualNumber = 0;
    m_core = 0;
    
    m_mode30i60p.SetEnable(false);

    m_extMode = 0;

} // TaskManager::TaskManager() 

TaskManager::~TaskManager() 
{ 
    Close();

} // TaskManager::~TaskManager(void) 

mfxStatus TaskManager::Init(
    VideoCORE* core,
    Config & config)
{
    m_taskIndex    = 0;
    m_actualNumber = 0;
    m_core         = core;

    // init param
    m_mode30i60p.SetEnable(config.m_bMode30i60pEnable);
    m_extMode           = config.m_extConfig.mode;

    m_mode30i60p.m_numOutputFrames    = 0;
    m_mode30i60p.m_prevInputTimeStamp = 0;   
    
    m_cpuFrc.Reset(m_extMode, config.m_extConfig.frcRational);

    m_resMngr.Init(config, this->m_core);

    m_tasks.resize(config.m_surfCount[VPP_OUT]);

    return MFX_ERR_NONE;

} // mfxStatus TaskManager::Init(void)


mfxStatus TaskManager::Close(void) 
{ 
    m_actualNumber = m_taskIndex = 0;

    Clear(m_tasks);

    m_core     = NULL;

    m_mode30i60p.SetEnable(false);
    m_extMode = 0;

    RateRational frcRational[2];
    frcRational[VPP_IN].FrameRateExtN = 30;
    frcRational[VPP_IN].FrameRateExtD = 1;
    frcRational[VPP_OUT].FrameRateExtN = 30;
    frcRational[VPP_OUT].FrameRateExtD = 1;
    m_cpuFrc.Reset(m_extMode, frcRational);

    m_resMngr.Close(); // release all resource 

    return MFX_ERR_NONE;

} // mfxStatus TaskManager::Close(void)


mfxStatus TaskManager::DoCpuFRC_AndUpdatePTS(
    mfxFrameSurface1 *input, 
    mfxFrameSurface1 *output, 
    mfxStatus *intSts)
{
    return m_cpuFrc.DoCpuFRC_AndUpdatePTS(input, output, intSts);

} // mfxStatus TaskManager::::DoCpuFRC_AndUpdatePTS(...)


mfxStatus TaskManager::DoAdvGfx(
    mfxFrameSurface1 *input, 
    mfxFrameSurface1 *output, 
    mfxStatus *intSts)
{
    return m_resMngr.DoAdvGfx(input, output, intSts);

} // mfxStatus TaskManager::DoAdvGfx(...)


mfxStatus TaskManager::AssignTask(
    mfxFrameSurface1 *input,
    mfxFrameSurface1 *output,
    mfxExtVppAuxData *aux,
    DdiTask*& pTask,
    mfxStatus& intSts)
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    // no state machine in VPP, so transition to previous state isn't possible.
    // it is the simplest way to resolve issue with MFX_WRN_DEVICE_BUSY, but performance could be affected
    pTask = GetTask();
    if(NULL == pTask)
    {
        return MFX_WRN_DEVICE_BUSY;
    }
    //--------------------------------------------------------
    mfxStatus sts = MFX_ERR_NONE;

    bool isAdvGfxMode = (COMPOSITE & m_extMode) || (FRC_INTERPOLATION & m_extMode) || (VARIANCE_REPORT & m_extMode) ? true : false;

    if( isAdvGfxMode )
    {
        sts = DoAdvGfx(input, output, &intSts);
        MFX_CHECK_STS(sts);
    }
    else if ((FRC_ENABLED & m_extMode) && (!m_mode30i60p.IsEnabled()) )
    {
        sts = DoCpuFRC_AndUpdatePTS(input, output, &intSts);
        MFX_CHECK_STS(sts);
    }
    else if( m_mode30i60p.IsEnabled() )
    {
        //aya: (input == NULL) is OK for 30i60p WITHOUT DISTRIBUTED_TIME_STAMP only.
        if( (NULL == input) && (FRC_DISTRIBUTED_TIMESTAMP & m_extMode) )
        {
            return MFX_ERR_MORE_DATA;
        }
        //-------------------------------------------------

        sts = m_resMngr.DoMode30i60p(input, output, &intSts);
        MFX_CHECK_STS(sts);
    }
    else // simple mode
    {
        if(NULL == input) return MFX_ERR_MORE_DATA;
    }


    /*pTask = GetTask();
    if(NULL == pTask)
    {
        return MFX_WRN_DEVICE_BUSY;
    }*/

    sts = FillTask(pTask, input, output, aux);
    MFX_CHECK_STS(sts);

    if (m_mode30i60p.IsEnabled())
    {
        UpdatePTS_Mode30i60p(
            input, 
            output, 
            pTask->taskIndex,
            &intSts);            
    }
    else if ( !(FRC_ENABLED & m_extMode) && !isAdvGfxMode )
    { 
        UpdatePTS_SimpleMode(input, output);
    }


    //pTask = GetTask();//AYA!!!

    return MFX_ERR_NONE;

} // mfxStatus TaskManager::AssignTask(...)


mfxStatus TaskManager::CompleteTask(DdiTask* pTask)
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    mfxStatus sts = m_core->DecreaseReference( &(pTask->output.pSurf->Data) );
    MFX_CHECK_STS(sts);

    mfxU32 freeIdx = pTask->output.resIdx;
    if(NO_INDEX != freeIdx && m_resMngr.m_surf[VPP_OUT].size() > 0)
    {
        m_resMngr.m_surf[VPP_OUT][freeIdx].SetFree(true);
    }

    if(pTask->bAdvGfxEnable || m_mode30i60p.IsEnabled() )
    {
        sts = m_resMngr.CompleteTask(pTask);
        MFX_CHECK_STS(sts);
    }
    else // simple mode
    {
        mfxU32 freeIdx = pTask->input.resIdx;
        if(NO_INDEX != freeIdx && m_resMngr.m_surf[VPP_IN].size() > 0)
        {
            m_resMngr.m_surf[VPP_IN][freeIdx].SetFree(true);
        }
    }

    sts = m_core->DecreaseReference( &(pTask->input.pSurf->Data) );
    MFX_CHECK_STS(sts);

    FreeTask(pTask);

    return MFX_TASK_DONE;
    

} // mfxStatus TaskManager::CompleteTask(DdiTask* pTask)


DdiTask* TaskManager::GetTask(void)
{
    //DdiTask *pTask = new DdiTask;
    mfxU32 indx = FindFreeSurface(m_tasks);
    if(NO_INDEX == indx) return NULL;

    //memset(m_tasks[indx], 0, sizeof(DdiTask));

    return &(m_tasks[indx]);

} // DdiTask* TaskManager::CreateTask(void)


void TaskManager::FillTask_Mode30i60p(
    DdiTask* pTask,
    mfxFrameSurface1 *pInSurface,
    mfxFrameSurface1 *pOutSurface)
{
    m_resMngr.FillTaskForMode30i60p(pTask, pInSurface, pOutSurface);

} // void TaskManager::FillTask_60i60pMode(DdiTask* pTask)


void TaskManager::FillTask_AdvGfxMode(
    DdiTask* pTask,
    mfxFrameSurface1 *pInSurface,
    mfxFrameSurface1 *pOutSurface)
{
    m_resMngr.FillTask(pTask, pInSurface, pOutSurface);

} // void TaskManager::FillTask_AdvGfxMode(...)


mfxStatus TaskManager::FillTask(
    DdiTask* pTask,
    mfxFrameSurface1 *pInSurface,
    mfxFrameSurface1 *pOutSurface,
    mfxExtVppAuxData *aux)
{
    // [0] Set param
    pTask->input.pSurf   = pInSurface;
    pTask->input.resIdx  = NO_INDEX;
    pTask->output.pSurf  = pOutSurface;
    pTask->output.resIdx = NO_INDEX;

    if(m_resMngr.m_surf[VPP_OUT].size() > 0) // out in system memory
    {
        pTask->output.resIdx     = FindFreeSurface( m_resMngr.m_surf[VPP_OUT] );
        if( NO_INDEX == pTask->output.resIdx) return MFX_WRN_DEVICE_BUSY;
        m_resMngr.m_surf[VPP_OUT][pTask->output.resIdx].SetFree(false);
    }

    pTask->taskIndex    = m_taskIndex++;

    pTask->input.timeStamp     = CURRENT_TIME_STAMP + m_actualNumber * FRAME_INTERVAL;
    pTask->input.endTimeStamp  = CURRENT_TIME_STAMP + (m_actualNumber + 1) * FRAME_INTERVAL;    
    pTask->output.timeStamp    = pTask->input.timeStamp;

    pTask->bAdvGfxEnable     = (COMPOSITE & m_extMode) || (FRC_INTERPOLATION & m_extMode) || (VARIANCE_REPORT & m_extMode) ? true : false;

    pTask->pAuxData = aux;

    if( m_mode30i60p.IsEnabled() )
    {
        FillTask_Mode30i60p(
            pTask,
            pInSurface,
            pOutSurface);
    }
    else if(pTask->bAdvGfxEnable)
    {
        FillTask_AdvGfxMode(
            pTask,
            pInSurface,
            pOutSurface);
    }
    else // simple mode
    {
        if(m_resMngr.m_surf[VPP_IN].size() > 0) // input in system memory
        {
            pTask->input.bUpdate    = true;
            pTask->input.resIdx     = FindFreeSurface( m_resMngr.m_surf[VPP_IN] );
            if( NO_INDEX == pTask->input.resIdx) return MFX_WRN_DEVICE_BUSY;
            m_resMngr.m_surf[VPP_IN][pTask->input.resIdx].SetFree(false);
        }
    }

    m_actualNumber += 1; // make sense for simple mode only
    
    mfxStatus sts = m_core->IncreaseReference( &(pTask->input.pSurf->Data) );
    MFX_CHECK_STS(sts);

    sts = m_core->IncreaseReference( &(pTask->output.pSurf->Data) );
    MFX_CHECK_STS(sts);

    pTask->SetFree(false);

    return MFX_ERR_NONE;

} // mfxStatus TaskManager::FillTask(...)


void TaskManager::UpdatePTS_Mode30i60p(
    mfxFrameSurface1 *input, 
    mfxFrameSurface1 *output, 
    mfxU32 taskIndex,
    mfxStatus *intSts)
{
    if ( (FRC_STANDARD & m_extMode) || (FRC_DISABLED == m_extMode) )
    {
        if (0 != taskIndex % 2)
        {
            output->Data.TimeStamp = (mfxU64) MFX_TIME_STAMP_INVALID;
            output->Data.FrameOrder = (mfxU32) MFX_FRAMEORDER_UNKNOWN;
            *intSts = MFX_ERR_MORE_SURFACE;
        }
        else
        {
            output->Data.TimeStamp = input->Data.TimeStamp;
            output->Data.FrameOrder = input->Data.FrameOrder;
            *intSts = MFX_ERR_NONE;
        }
    }
    else if (FRC_DISTRIBUTED_TIMESTAMP & m_extMode)
    {
        if (0 != taskIndex % 2)
        {
            output->Data.TimeStamp = m_mode30i60p.m_prevInputTimeStamp + (input->Data.TimeStamp - m_mode30i60p.m_prevInputTimeStamp) / 2;
            *intSts = MFX_ERR_MORE_SURFACE;
        }
        else
        {
            m_mode30i60p.m_prevInputTimeStamp = input->Data.TimeStamp;
            output->Data.TimeStamp = input->Data.TimeStamp;
            *intSts = MFX_ERR_NONE;
        }

        output->Data.FrameOrder = m_mode30i60p.m_numOutputFrames;
        m_mode30i60p.m_numOutputFrames += 1;
    }

    return;

} // void TaskManager::UpdatePTS_Mode30i60p(...)


void TaskManager::UpdatePTS_SimpleMode(
    mfxFrameSurface1 *input, 
    mfxFrameSurface1 *output)
{
    output->Data.TimeStamp = input->Data.TimeStamp;
    output->Data.FrameOrder = input->Data.FrameOrder;

} // void UpdatePTS_SimpleMode(...)


////////////////////////////////////////////////////////////////////////////////////
// VideoHWVPP Component: platform independent code
////////////////////////////////////////////////////////////////////////////////////

mfxStatus  VideoVPPHW::CopyPassThrough(mfxFrameSurface1 *pInputSurface, mfxFrameSurface1 *pOutputSurface)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxU16 srcPattern, dstPattern;

    srcPattern = dstPattern = MFX_MEMTYPE_EXTERNAL_FRAME;

    if (m_IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        srcPattern |= MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else
    {
        srcPattern |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    }

    if (m_IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        dstPattern |= MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else
    {
        dstPattern |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    }

    sts = m_pCore->DoFastCopyWrapper(pOutputSurface, 
        dstPattern,
        pInputSurface,
        srcPattern);

    MFX_CHECK_STS(sts);

    sts = m_pCore->DecreaseReference(&pOutputSurface->Data);
    MFX_CHECK_STS(sts);

    sts = m_pCore->DecreaseReference(&pInputSurface->Data);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::CopyPassThrough(mfxFrameSurface1 *pInputSurface, mfxFrameSurface1 *pOutputSurface)


VideoVPPHW::VideoVPPHW(IOMode mode, VideoCORE *core)
{
    m_ioMode = mode; 
    m_pCore = core;

    m_config.m_bRefFrameEnable = false;
    m_config.m_bMode30i60pEnable = false;
    m_config.m_extConfig.mode  = FRC_DISABLED;
    m_config.m_bPassThroughEnable = false;
    m_config.m_surfCount[VPP_IN]   = 1;
    m_config.m_surfCount[VPP_OUT]  = 1;
 
    m_executeSurf.resize(0);

    m_IOPattern = 0;

    m_asyncDepth = ACCEPTED_DEVICE_ASYNC_DEPTH;

    memset(&m_executeParams, 0, sizeof(mfxExecuteParams));

    // sync workload mode by default
    m_workloadMode = VPP_SYNC_WORKLOAD;

} // VideoVPPHW::VideoVPPHW(IOMode mode, VideoCORE *core)


VideoVPPHW::~VideoVPPHW()
{
    Close();

} // VideoVPPHW::~VideoVPPHW()


mfxStatus  VideoVPPHW::Init(
    mfxVideoParam *par, 
    bool isTemporal)
{
    mfxStatus sts = MFX_ERR_NONE;
    bool bIsFilterSkipped = false;

    //-----------------------------------------------------
    // [1] high level check
    //-----------------------------------------------------
    MFX_CHECK_NULL_PTR1(par);

    sts = CheckIOMode(par, m_ioMode);
    MFX_CHECK_STS(sts);

    if (MFX_FOURCC_NV12 != par->vpp.Out.FourCC && MFX_FOURCC_RGB4 != par->vpp.Out.FourCC)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    m_IOPattern = par->IOPattern;

    if(0 == par->AsyncDepth)
    {
        m_asyncDepth = MFX_AUTO_ASYNC_DEPTH_VALUE;
    }
    else if( par->AsyncDepth > MFX_AUTO_ASYNC_DEPTH_VALUE )
    {
        // warning???
        m_asyncDepth = MFX_AUTO_ASYNC_DEPTH_VALUE;
    }
    else
    {
        m_asyncDepth = par->AsyncDepth;
    }


    //-----------------------------------------------------
    // [2] device creation
    //-----------------------------------------------------
    m_params = *par;// PARAMS!!!

    m_ddi.reset( CreateVideoProcessing( m_pCore) );
    if (m_ddi.get() == 0)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }
    
    sts = m_ddi->CreateDevice( m_pCore, par, isTemporal);
    MFX_CHECK_STS( sts );

    mfxVppCaps caps = {0};
    sts = m_ddi->QueryCapabilities( caps );
    MFX_CHECK_STS(sts);

    if (par->vpp.In.Width > caps.uMaxWidth  || par->vpp.In.Height  > caps.uMaxHeight ||
        par->vpp.Out.Width > caps.uMaxWidth || par->vpp.Out.Height > caps.uMaxHeight)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    m_config.m_IOPattern = 0;
    sts = ConfigureExecuteParams(
        m_params,
        caps, 
        m_executeParams,
        m_config);

    if( MFX_WRN_FILTER_SKIPPED == sts )
    {
        bIsFilterSkipped = true;
        sts = MFX_ERR_NONE;
    }
    MFX_CHECK_STS(sts);

    if( m_executeParams.bFRCEnable && 
       (MFX_HW_D3D11 == m_pCore->GetVAType()) && 
       m_executeParams.customRateData.indexRateConversion != 0 )
    {
        sts = m_ddi->ReconfigDevice(m_executeParams.customRateData.indexRateConversion);
        MFX_CHECK_STS(sts);
    }

    // allocate special structure for ::ExecuteBlt()
    {
        m_executeSurf.resize( m_config.m_surfCount[VPP_IN] );
    }

    // count of internal resources based on async_depth
    m_config.m_surfCount[VPP_OUT] = (mfxU16)(m_config.m_surfCount[VPP_OUT] + m_asyncDepth);
    m_config.m_surfCount[VPP_IN]  = (mfxU16)(m_config.m_surfCount[VPP_IN]  + m_asyncDepth);

    //-----------------------------------------------------
    // [3] Opaque pre-work:: moved to high level 
    //-----------------------------------------------------
    
    mfxFrameAllocRequest request;

    //-----------------------------------------------------
    // [4] internal frames allocation (make sense fo SYSTEM_MEMORY only)
    //-----------------------------------------------------
    if (D3D_TO_SYS == m_ioMode || SYS_TO_SYS == m_ioMode) // [OUT == SYSTEM_MEMORY]
    {
        //m_config.m_surfCount[VPP_OUT] = (mfxU16)(m_config.m_surfCount[VPP_OUT] + m_asyncDepth);

        request.Info        = par->vpp.Out;
        request.Type        = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_INTERNAL_FRAME;
        request.NumFrameMin = request.NumFrameSuggested = m_config.m_surfCount[VPP_OUT] ;

        sts = m_internalVidSurf[VPP_OUT].Alloc(m_pCore, request, true);
        MFX_CHECK_STS(sts);

        m_config.m_IOPattern |= MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

        m_config.m_surfCount[VPP_OUT] = request.NumFrameMin;
    }

    if (SYS_TO_SYS == m_ioMode || SYS_TO_D3D == m_ioMode ) // [IN == SYSTEM_MEMORY]
    {
        //m_config.m_surfCount[VPP_IN] = (mfxU16)(m_config.m_surfCount[VPP_IN] + m_asyncDepth);

        request.Info        = par->vpp.In;
        request.Type        = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_INTERNAL_FRAME;
        request.NumFrameMin = request.NumFrameSuggested = m_config.m_surfCount[VPP_IN] ;

        sts = m_internalVidSurf[VPP_IN].Alloc(m_pCore, request, true);
        MFX_CHECK_STS(sts);

         m_config.m_IOPattern |= MFX_IOPATTERN_IN_SYSTEM_MEMORY;

         m_config.m_surfCount[VPP_IN] = request.NumFrameMin;
    }

    // sync workload mode by default
    m_workloadMode = VPP_ASYNC_WORKLOAD;

    //-----------------------------------------------------
    // [5] resource and task manager
    //-----------------------------------------------------
    sts = m_taskMngr.Init(m_pCore, m_config);
    MFX_CHECK_STS(sts);

    return (bIsFilterSkipped) ? MFX_WRN_FILTER_SKIPPED : MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::Init(mfxVideoParam *par, mfxU32 *tabUsedFiltersID, mfxU32 numOfFilters)


mfxStatus VideoVPPHW::QueryCaps(VideoCORE* core, MfxHwVideoProcessing::mfxVppCaps& caps)
{
    std::auto_ptr<MfxHwVideoProcessing::DriverVideoProcessing> ddi;
    ddi.reset( CreateVideoProcessing(core) );
    if (ddi.get() == 0) return MFX_ERR_UNKNOWN;

    mfxVideoParam tmpPar;
    tmpPar.vpp.In.Width = 352;
    tmpPar.vpp.In.Height= 288;
    tmpPar.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    tmpPar.vpp.In.FourCC = MFX_FOURCC_NV12;
    tmpPar.vpp.Out = tmpPar.vpp.In;

    mfxStatus sts = ddi->CreateDevice( core, &tmpPar, true);
    MFX_CHECK_STS( sts );

    //mfxVppCaps caps = {0};
    memset( (void*)&caps, 0, sizeof(mfxVppCaps) );
    sts = ddi->QueryCapabilities(caps);
    MFX_CHECK_STS( sts );

    return sts;
    
} // mfxStatus VideoVPPHW::QueryCaps(VideoCORE* core, MfxHwVideoProcessing::mfxVppCaps& caps)


mfxStatus VideoVPPHW::QueryIOSurf(
    IOMode ioMode,
    VideoCORE* core,
    mfxVideoParam* par,
    mfxFrameAllocRequest* request)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1(par);

    sts = CheckIOMode(par, ioMode);
    MFX_CHECK_STS(sts);

    if (MFX_FOURCC_NV12 != par->vpp.Out.FourCC && MFX_FOURCC_RGB4 != par->vpp.Out.FourCC)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    std::auto_ptr<MfxHwVideoProcessing::DriverVideoProcessing> ddi;
    ddi.reset( CreateVideoProcessing(core) );
    if (ddi.get() == 0)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    sts = ddi->CreateDevice( core, par, true);
    MFX_CHECK_STS( sts );

    mfxVppCaps caps = {0};
    sts = ddi->QueryCapabilities(caps);
    MFX_CHECK_STS( sts );

    if (par->vpp.In.Width > caps.uMaxWidth  || par->vpp.In.Height  > caps.uMaxHeight ||
        par->vpp.Out.Width > caps.uMaxWidth || par->vpp.Out.Height > caps.uMaxHeight)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    mfxExecuteParams executeParams = {0};
    Config  config = {0};
    sts = ConfigureExecuteParams(
        *par,
        caps,
        executeParams,
        config);
    if (MFX_WRN_FILTER_SKIPPED == sts )
    {
        sts = MFX_ERR_NONE;
    }
    MFX_CHECK_STS(sts);

    request[VPP_IN].NumFrameMin  = request[VPP_IN].NumFrameSuggested  = config.m_surfCount[VPP_IN];
    request[VPP_OUT].NumFrameMin = request[VPP_OUT].NumFrameSuggested = config.m_surfCount[VPP_OUT];

    return sts;

} // mfxStatus VideoVPPHW::QueryIOSurf(mfxVideoParam *par, mfxU32 *tabUsedFiltersID, mfxU32 numOfFilters)


mfxStatus VideoVPPHW::Reset(mfxVideoParam *par)
{
    //mfxStatus sts = MFX_ERR_NONE;

    m_params = *par;

    m_taskMngr.Close();// all resource free here

    m_taskMngr.Init(
        m_pCore,
        m_config);

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::Reset(mfxVideoParam *par)


mfxStatus VideoVPPHW::Close()
{
    mfxStatus sts = MFX_ERR_NONE; 

    m_internalVidSurf[VPP_IN].Free();
    m_internalVidSurf[VPP_OUT].Free();

    m_executeSurf.clear();

    m_config.m_extConfig.mode  = FRC_DISABLED;
    m_config.m_bMode30i60pEnable  = false;
    m_config.m_bRefFrameEnable = false;
    m_config.m_bPassThroughEnable = false;
    m_config.m_surfCount[VPP_IN]  = 1;
    m_config.m_surfCount[VPP_OUT] = 1;

    m_IOPattern = 0;
    
    //m_acceptedDeviceAsyncDepth = ACCEPTED_DEVICE_ASYNC_DEPTH;

    m_taskMngr.Close();

    // sync workload mode by default
    m_workloadMode = VPP_SYNC_WORKLOAD;

    return sts;

} // mfxStatus VideoVPPHW::Close()


mfxStatus VideoVPPHW::VppFrameCheck(
                                    mfxFrameSurface1 *input, 
                                    mfxFrameSurface1 *output,
                                    mfxExtVppAuxData *aux,
                                    MFX_ENTRY_POINT pEntryPoint[], 
                                    mfxU32 &numEntryPoints)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    pEntryPoint= pEntryPoint;
    mfxStatus sts;
    mfxStatus intSts = MFX_ERR_NONE;

    DdiTask *pTask = NULL;

    sts = m_taskMngr.AssignTask(input, output, aux, pTask, intSts);
    MFX_CHECK_STS(sts);

    if (VPP_SYNC_WORKLOAD == m_workloadMode)
    {
        // submit task
        SyncTaskSubmission(pTask);

        if (false == m_config.m_bPassThroughEnable || 
            (true == m_config.m_bPassThroughEnable && true == IsRoiDifferent(pTask->input.pSurf, pTask->output.pSurf)))
        {
            // configure entry point
            pEntryPoint[0].pRoutine = VideoVPPHW::QueryTaskRoutine;
            pEntryPoint[0].pParam = (void *) pTask;
            pEntryPoint[0].requiredNumThreads = 1;
            pEntryPoint[0].pState = (void *) this;
            pEntryPoint[0].pRoutineName = "VPP Query";

            numEntryPoints = 1;
        }
    }
    else
    {
        if (false == m_config.m_bPassThroughEnable || 
            (true == m_config.m_bPassThroughEnable && true == IsRoiDifferent(pTask->input.pSurf, pTask->output.pSurf)))
        {
            // configure entry point
            pEntryPoint[1].pRoutine = VideoVPPHW::QueryTaskRoutine;
            pEntryPoint[1].pParam = (void *) pTask;
            pEntryPoint[1].requiredNumThreads = 1;
            pEntryPoint[1].pState = (void *) this;
            pEntryPoint[1].pRoutineName = "VPP Query";

            // configure entry point
            pEntryPoint[0].pRoutine = VideoVPPHW::AsyncTaskSubmission;
            pEntryPoint[0].pParam = (void *) pTask;
            pEntryPoint[0].requiredNumThreads = 1;
            pEntryPoint[0].pState = (void *) this;
            pEntryPoint[0].pRoutineName = "VPP Submit";

            numEntryPoints = 2;
        }
        else
        {
            // configure entry point
            pEntryPoint[0].pRoutine = VideoVPPHW::AsyncTaskSubmission;
            pEntryPoint[0].pParam = (void *) pTask;
            pEntryPoint[0].requiredNumThreads = 1;
            pEntryPoint[0].pState = (void *) this;
            pEntryPoint[0].pRoutineName = "VPP Submit";

            numEntryPoints = 1;
        }
    }

    return intSts;

} // mfxStatus VideoVPPHW::VppFrameCheck(...)


mfxStatus VideoVPPHW::PreWorkOutSurface(ExtSurface & output)
{
    mfxHDLPair out;
    mfxHDLPair hdl;

    if (D3D_TO_D3D == m_ioMode || SYS_TO_D3D == m_ioMode)
    {
        /*if (m_bIsOpaqueMemory[VPP_OUT])
        {
            MFX_SAFE_CALL(m_pCore->GetFrameHDL( output.pSurf->Data.MemId, (mfxHDL *)&hdl) );
            m_executeParams.targetSurface.memId = output.pSurf->Data.MemId;
            m_executeParams.targetSurface.bExternal = false;
        }
        else*/
        {
            MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(output.pSurf->Data.MemId, (mfxHDL *)&hdl));
            m_executeParams.targetSurface.memId = output.pSurf->Data.MemId;
            m_executeParams.targetSurface.bExternal = true;
        }
    }
    else
    {
        mfxU32 resId = output.resIdx;

        MFX_SAFE_CALL(m_pCore->GetFrameHDL(m_internalVidSurf[VPP_OUT].mids[resId], (mfxHDL *)&hdl));
        m_executeParams.targetSurface.memId = m_internalVidSurf[VPP_OUT].mids[resId];
        m_executeParams.targetSurface.bExternal = false;
    }

    out = hdl;

    // register output surface (aya: make sense for DX9 only)
    mfxStatus sts = m_ddi->Register(&out, 1, TRUE);
    MFX_CHECK_STS(sts);

    m_executeParams.targetSurface.hdl       = static_cast<mfxHDLPair>(out);
    m_executeParams.targetSurface.frameInfo = output.pSurf->Info;
    m_executeParams.targetTimeStamp         = output.timeStamp;

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::PreWorkOutSurface(DdiTask* pTask)


mfxStatus VideoVPPHW::PreWorkInputSurface(std::vector<ExtSurface> & surfQueue)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 numSamples = (mfxU32)surfQueue.size();
    mfxHDLPair hdl;
    mfxHDLPair in;

    for (mfxU32 i = 0 ; i < numSamples; i += 1)
    {
        bool bExternal = true;
        mfxMemId memId = 0;

        if (SYS_TO_D3D == m_ioMode || SYS_TO_SYS == m_ioMode)
        {   
            mfxU32 resIdx = surfQueue[i].resIdx;

            if( surfQueue[i].bUpdate )
            {
                //mfxFrameData d3dSurf = { 0 };
                //FrameLocker lock1(m_pCore, d3dSurf, m_internalVidSurf[VPP_IN].mids[ resIdx ]);
                //MFX_CHECK(d3dSurf.Y != 0, MFX_ERR_LOCK_MEMORY);

                //mfxFrameData sysSurf = surfQueue[i].pSurf->Data;
                ////assert( sysSurf.Y == 0 );
                //FrameLocker lock2(m_pCore, sysSurf, true);
                //MFX_CHECK(sysSurf.Y != 0, MFX_ERR_LOCK_MEMORY);

                //{
                //    static int cntCopySys2Vid = 0;
                //    if(cntCopySys2Vid == 0)
                //    {
                //        cntCopySys2Vid++;
                //        assert(!"cntCopySys2Vid");
                //    }

                //    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "HW_VPP: Copy input (sys->d3d)");
                //    sts = CopyFrameDataBothFields(m_pCore, d3dSurf, sysSurf, surfQueue[i].pSurf->Info);
                //    MFX_CHECK_STS(sts);
                //}

                mfxFrameSurface1 inputVidSurf = {0};
                inputVidSurf.Info = surfQueue[i].pSurf->Info;
                inputVidSurf.Data.MemId = m_internalVidSurf[VPP_IN].mids[ resIdx ];

                mfxStatus sts = m_pCore->DoFastCopyWrapper(
                    &inputVidSurf, 
                    MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET,
                    surfQueue[i].pSurf,
                    MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY);
                MFX_CHECK_STS(sts);

            }

            MFX_SAFE_CALL(m_pCore->GetFrameHDL(m_internalVidSurf[VPP_IN].mids[resIdx], (mfxHDL *)&hdl));
            in = hdl;

            bExternal = false;
            memId     = m_internalVidSurf[VPP_IN].mids[resIdx];
        }
        else
        {
            MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(surfQueue[i].pSurf->Data.MemId, (mfxHDL *)&hdl));
            in = hdl;

            bExternal = true;
            memId     = surfQueue[i].pSurf->Data.MemId;
        }

        // register input surface
        sts = m_ddi->Register(&in, 1, TRUE);
        MFX_CHECK_STS(sts);

        memset(&m_executeSurf[i], 0, sizeof(mfxDrvSurface));

        // set input surface
        m_executeSurf[i].hdl = static_cast<mfxHDLPair>(in);
        m_executeSurf[i].frameInfo = surfQueue[i].pSurf->Info;

        // prepare the primary video sample
        m_executeSurf[i].startTimeStamp = surfQueue[i].timeStamp;
        m_executeSurf[i].endTimeStamp   = surfQueue[i].endTimeStamp;

        // debug info
        m_executeSurf[i].bExternal = bExternal;
        m_executeSurf[i].memId     = memId;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::PreWorkInputSurface(...)


mfxStatus VideoVPPHW::PostWorkOutSurface(ExtSurface & output)
{
    mfxStatus sts = MFX_ERR_NONE;

    // code here to resolve issue in case of sync issue with emcoder in case of system memory
    // [3] Copy sys -> vid
    if( SYS_TO_SYS == m_ioMode || D3D_TO_SYS == m_ioMode)
    {
        //mfxFrameData d3dSurf = { 0 };
        //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Surface lock (output frame)");
        ////MFX_LTRACE_S(MFX_TRACE_LEVEL_INTERNAL, pTask->frameNumber);
        /*FrameLocker lock(
            m_pCore,
            d3dSurf,
            m_internalVidSurf[VPP_OUT].mids[ output.resIdx ]);*/

        //MFX_CHECK(d3dSurf.Y != 0, MFX_ERR_LOCK_MEMORY);
        //MFX_AUTO_TRACE_STOP();

        //mfxFrameData sysSurf = output.pSurf->Data;
        //FrameLocker lock2(m_pCore, sysSurf, true);
        //MFX_CHECK(sysSurf.Y != 0, MFX_ERR_LOCK_MEMORY);

        //{
        //    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "HW_VPP: Copy output (d3d->sys)");
        //    sts = CopyFrameDataBothFields(m_pCore, sysSurf, d3dSurf, output.pSurf->Info);
        //    MFX_CHECK_STS(sts);
        //}

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "HW_VPP: Copy output (d3d->sys)");
        mfxFrameSurface1 d3dSurf;
        d3dSurf.Info = output.pSurf->Info;
        d3dSurf.Data.MemId = m_internalVidSurf[VPP_OUT].mids[ output.resIdx ];

        mfxStatus sts = m_pCore->DoFastCopyWrapper(
            output.pSurf, 
            MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
            &d3dSurf,
            MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET
            );
        MFX_CHECK_STS(sts);

    }

    // unregister output surface (make sense in case DX9 only)
    sts = m_ddi->Register( &(m_executeParams.targetSurface.hdl), 1, FALSE);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::PostWorkOutSurface(ExtSurface & output)


mfxStatus VideoVPPHW::PostWorkInputSurface(mfxU32 numSamples)
{
    // unregister input surface(s) (make sense in case DX9 only)
    for (mfxU32 i = 0 ; i < numSamples; i += 1)
    {        
        mfxStatus sts = m_ddi->Register( &(m_executeSurf[i].hdl), 1, FALSE);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::PostWorkInputSurface(mfxU32 numSamples)


mfxStatus VideoVPPHW::SyncTaskSubmission(DdiTask* pTask)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (true == m_config.m_bPassThroughEnable && 
        false == IsRoiDifferent(pTask->input.pSurf, pTask->output.pSurf))
    {
        sts = CopyPassThrough(pTask->input.pSurf, pTask->output.pSurf);
        MFX_CHECK_STS(sts);

        return MFX_ERR_NONE;
    }

    mfxU32 i = 0;
    mfxU32 numSamples   = pTask->bkwdRefCount + 1 + pTask->fwdRefCount;

    std::vector<ExtSurface> surfQueue(numSamples);

    mfxU32 indx = 0;
    // bkwdFrames
    for(i = 0; i < pTask->bkwdRefCount; i++)
    {
        indx = i; 
        surfQueue[indx] = pTask->m_refList[i];
    }

    // cur Frame
    indx = pTask->bkwdRefCount;
    surfQueue[indx] = pTask->input;

    // fwd Frames
    for( i = 0; i < pTask->fwdRefCount; i++ )
    {
        indx = pTask->bkwdRefCount + 1 + i;
        surfQueue[indx] = pTask->m_refList[pTask->bkwdRefCount + i];
    }


    sts = PreWorkOutSurface(pTask->output);
    MFX_CHECK_STS(sts);

    sts = PreWorkInputSurface(surfQueue);
    MFX_CHECK_STS(sts);


    m_executeParams.refCount     = numSamples;
    m_executeParams.bkwdRefCount = pTask->bkwdRefCount;
    m_executeParams.fwdRefCount = pTask->fwdRefCount;
    m_executeParams.pRefSurfaces = &m_executeSurf[0];

    m_executeParams.statusReportID = pTask->taskIndex;

    // aya: logic of deinterlace using is very complex (see internal msdk spec)
    // here is correct code but driver produce artefact. 
    // will be used after driver fix
    //-----------------------------------------------------
    //{
    //    m_executeParams.iTargetInterlacingMode = DEINTERLACE_DISABLE;

    //    mfxU16 dstPic = m_executeParams.targetSurface.frameInfo.PicStruct;
    //    mfxU16 srcPic = pInputSurface->Info.PicStruct;// aya: may be issue in case of multiple input frames

    //    bool bDeinterlaceRequired = ((dstPic & MFX_PICSTRUCT_PROGRESSIVE) && (!(srcPic & MFX_PICSTRUCT_PROGRESSIVE)));
    //        
    //    if( bDeinterlaceRequired )
    //    {
    //        m_executeParams.iTargetInterlacingMode = DEINTERLACE_ENABLE;
    //    }
    //}
    //-----------------------------------------------------

    m_executeParams.iTargetInterlacingMode = DEINTERLACE_ENABLE;

    if( !(m_executeParams.targetSurface.frameInfo.PicStruct & (MFX_PICSTRUCT_PROGRESSIVE)) )
    {
        m_executeParams.iTargetInterlacingMode = DEINTERLACE_DISABLE;
        m_executeParams.iDeinterlacingAlgorithm = 0;
    }


    sts = m_ddi->Execute(&m_executeParams);
    if (sts != MFX_ERR_NONE)
    {
        pTask->SetFree(true);
        return sts;
    }

    MFX_CHECK_STS(sts);

    //aya: wa for variance
    if(m_executeParams.bVarianceEnable)
    {
        //pTask->frameNumber = m_executeParams.frameNumber;
        pTask->bVariance   = true;
    }

    sts = PostWorkOutSurface(pTask->output);
    MFX_CHECK_STS(sts);

    sts = PostWorkInputSurface(numSamples);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::SyncTaskSubmission(DdiTask* pTask)


mfxStatus VideoVPPHW::AsyncTaskSubmission(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "VPP Submission");
    mfxStatus sts;

    // touch unreferenced parameters(s)
    threadNumber = threadNumber;
    callNumber = callNumber;

    sts = ((VideoVPPHW *)pState)->SyncTaskSubmission((DdiTask *)pParam);
    MFX_CHECK_STS(sts);

    return MFX_TASK_DONE;

} // mfxStatus VideoVPPHW::AsyncTaskSubmission(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)


mfxStatus VideoVPPHW::QueryTaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "VPP Query");
    mfxStatus sts = MFX_ERR_NONE;

    threadNumber = threadNumber;
    callNumber   = callNumber;

    VideoVPPHW *pHwVpp = (VideoVPPHW *) pState;
    DdiTask *pTask     = (DdiTask*) pParam;

    UMC::AutomaticUMCMutex guard(pHwVpp->m_guard);

    mfxU32 currentTaskIdx = pTask->taskIndex;

    sts = pHwVpp->m_ddi->QueryTaskStatus(currentTaskIdx);
    MFX_CHECK_STS(sts);    

    //[2] Variance
    if( pTask->bVariance )
    {
        // aya: windows part
        std::vector<UINT> variance;

        sts = pHwVpp->m_ddi->QueryVariance(
            pTask->taskIndex,
            variance);
        MFX_CHECK_STS(sts);

        if(pTask->pAuxData)
        {
            if( MFX_EXTBUFF_VPP_VARIANCE_REPORT == pTask->pAuxData->Header.BufferId)
            {
                mfxExtVppReport* pExtReport = (mfxExtVppReport*)pTask->pAuxData;

                assert(variance.size() <= 11);

                for(mfxU32 i = 0; i < variance.size(); i++)
                {    
                    pExtReport->Variances[i] = variance[i];
                }
            }
            else if( MFX_EXTBUFF_VPP_AUXDATA == pTask->pAuxData->Header.BufferId)
            {
                pTask->pAuxData->PicStruct = EstimatePicStruct( 
                    &variance[0],
                    pHwVpp->m_params.vpp.Out.Width,
                    pHwVpp->m_params.vpp.Out.Height);
            }
        }
    }

    // code moved in Submission() part due to issue with encoder
    //// [3] Copy sys -> vid
    //if( SYS_TO_SYS == pHwVpp->m_ioMode || D3D_TO_SYS == pHwVpp->m_ioMode)
    //{
    //    mfxFrameData d3dSurf = { 0 };
    //    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Surface lock (output frame)");
    //    MFX_LTRACE_S(MFX_TRACE_LEVEL_INTERNAL, pTask->frameNumber);
    //    FrameLocker lock(
    //        pHwVpp->m_pCore,
    //        d3dSurf,
    //        pHwVpp->m_internalVidSurf[VPP_OUT].mids[ pTask->output.resIdx ]);

    //    MFX_CHECK(d3dSurf.Y != 0, MFX_ERR_LOCK_MEMORY);
    //    MFX_AUTO_TRACE_STOP();

    //    mfxFrameData sysSurf = pTask->output.pSurf->Data;
    //    FrameLocker lock2(pHwVpp->m_pCore, sysSurf, true);
    //    MFX_CHECK(sysSurf.Y != 0, MFX_ERR_LOCK_MEMORY);

    //    {
    //        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "HW_VPP: Copy output (d3d->sys)");
    //        sts = CopyFrameDataBothFields(pHwVpp->m_pCore, sysSurf, d3dSurf, pTask->output.pSurf->Info);
    //        MFX_CHECK_STS(sts);
    //    }
    //}

    // [4] Complete task
    sts = pHwVpp->m_taskMngr.CompleteTask(pTask);

    return sts;

} // mfxStatus VideoVPPHW::QueryTaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)

//---------------------------------------------------------------------------------
// Check DI mode set by user, if not set use advanced as default (if supported)
// Returns: 0 - cannot set due to HW limitations or wrong range, 1 - BOB, 2 - ADI )
//---------------------------------------------------------------------------------
mfxI32 GetDeinterlaceMode( const mfxVideoParam& videoParam, const mfxVppCaps& caps )
{
    mfxI32 deinterlacingMode = 0;

    // look for user defined deinterlacing mode
    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
    {
        if (videoParam.ExtParam[i] && videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DEINTERLACING)
        {
            mfxExtVPPDeinterlacing* extDI = (mfxExtVPPDeinterlacing*) videoParam.ExtParam[i];
            if (extDI->Mode != MFX_DEINTERLACING_ADVANCED && extDI->Mode != MFX_DEINTERLACING_BOB)
            {
                return 0;
            }

            deinterlacingMode = extDI->Mode;
            break;
        }
    }

    if (caps.uAdvancedDI)
    {
        if (0 == deinterlacingMode)
        {
            deinterlacingMode = MFX_DEINTERLACING_ADVANCED;
        }
    }
    else if (caps.uSimpleDI)
    {
        if (MFX_DEINTERLACING_ADVANCED == deinterlacingMode)
        {
            deinterlacingMode = 0;
        }
        else
        {
            deinterlacingMode = MFX_DEINTERLACING_BOB;
        }
    }
    else
    {
        deinterlacingMode = 0;
    }

    return deinterlacingMode;
}

//---------------------------------------------------------
// Do internal configuration
//---------------------------------------------------------
mfxStatus ConfigureExecuteParams(
    mfxVideoParam & videoParam, // [IN]
    mfxVppCaps & caps,          // [IN]
    mfxExecuteParams & executeParams, // [OUT]
    Config & config)                  // [OUT]
{
    //----------------------------------------------
    std::vector<mfxU32> pipelineList;
    mfxStatus sts = GetPipelineList( &videoParam, pipelineList, true );
    MFX_CHECK_STS(sts);

    mfxF64 inDNRatio = 0, outDNRatio = 0;
    bool bIsFilterSkipped = false;

    // default
    config.m_extConfig.frcRational[VPP_IN].FrameRateExtN = videoParam.vpp.In.FrameRateExtN;
    config.m_extConfig.frcRational[VPP_IN].FrameRateExtD = videoParam.vpp.In.FrameRateExtD;
    config.m_extConfig.frcRational[VPP_OUT].FrameRateExtN = videoParam.vpp.Out.FrameRateExtN;
    config.m_extConfig.frcRational[VPP_OUT].FrameRateExtD = videoParam.vpp.Out.FrameRateExtD;

    config.m_surfCount[VPP_IN]  = 1;
    config.m_surfCount[VPP_OUT] = 1;

    //-----------------------------------------------------
    for (mfxU32 i = 0; i < pipelineList.size(); i += 1)
    {
        mfxU32 curFilterId = pipelineList[i];

        switch(curFilterId)
        {
            case MFX_EXTBUFF_VPP_DEINTERLACING:
            {
                // see below
                break;
            }

            case MFX_EXTBUFF_VPP_DI:
            {
                // FIXME: add reference frame to use motion adaptive ADI
                executeParams.iDeinterlacingAlgorithm = GetDeinterlaceMode( videoParam, caps );
                if (0 == executeParams.iDeinterlacingAlgorithm)
                {
                    bIsFilterSkipped = true;
                }

                break;
            }

            case MFX_EXTBUFF_VPP_DI_30i60p:
            {
                executeParams.iDeinterlacingAlgorithm = GetDeinterlaceMode( videoParam, caps );

                if(MFX_DEINTERLACING_ADVANCED == executeParams.iDeinterlacingAlgorithm)
                {
                    // use motion adaptive ADI with reference frame (quality)
                    config.m_bMode30i60pEnable = true;
                    config.m_bRefFrameEnable = true;
                    config.m_surfCount[VPP_IN]  = IPP_MAX(2, config.m_surfCount[VPP_IN]);
                    config.m_surfCount[VPP_OUT] = IPP_MAX(2, config.m_surfCount[VPP_OUT]);
                    executeParams.bFMDEnable = true;
                }
                else if(MFX_DEINTERLACING_BOB == executeParams.iDeinterlacingAlgorithm)
                {
                    // use ADI with spatial info, no reference frame (speed)
                    config.m_bMode30i60pEnable = true;
                    config.m_bRefFrameEnable = false;
                    config.m_surfCount[VPP_IN]  = IPP_MAX(1, config.m_surfCount[VPP_IN]);
                    config.m_surfCount[VPP_OUT] = IPP_MAX(2, config.m_surfCount[VPP_OUT]);
                }
                else
                {
                    executeParams.iDeinterlacingAlgorithm = 0;
                    bIsFilterSkipped = true;
                }

                break;
            }

            case MFX_EXTBUFF_VPP_DENOISE:
            {
                if (caps.uDenoiseFilter)
                {
                    executeParams.bDenoiseAutoAdjust = TRUE;
                    // set denoise settings
                    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
                    {
                        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DENOISE)
                        {
                            mfxExtVPPDenoise *extDenoise= (mfxExtVPPDenoise*) videoParam.ExtParam[i];

                            executeParams.denoiseFactor = MapDNFactor(extDenoise->DenoiseFactor );

                            executeParams.bDenoiseAutoAdjust = FALSE;
                        }
                    }
                }
                else
                {
                    bIsFilterSkipped = true;
                }

                break;
            }

            case MFX_EXTBUFF_VPP_DETAIL:
            {
                if(caps.uDetailFilter)
                {
                    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
                    {
                        executeParams.detailFactor = 32;// default
                        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DETAIL)
                        {
                            mfxExtVPPDetail *extDetail = (mfxExtVPPDetail*) videoParam.ExtParam[i];
                            executeParams.detailFactor = MapDNFactor(extDetail->DetailFactor);
                        }
                    }
                }
                else
                {
                    bIsFilterSkipped = true;
                }

                break;
            }

            case MFX_EXTBUFF_VPP_PROCAMP:
            {
                if(caps.uProcampFilter)
                {
                    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
                    {
                        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_PROCAMP)
                        {
                            mfxExtVPPProcAmp *extProcAmp = (mfxExtVPPProcAmp*) videoParam.ExtParam[i];

                            executeParams.Brightness = extProcAmp->Brightness;
                            executeParams.Saturation = extProcAmp->Saturation;
                            executeParams.Hue        = extProcAmp->Hue;
                            executeParams.Contrast   = extProcAmp->Contrast;
                            executeParams.bEnableProcAmp = true;
                        }
                    }
                }
                else
                {
                    bIsFilterSkipped = true;
                }

                break;
            }

            case MFX_EXTBUFF_VPP_SCENE_ANALYSIS:
            {
                // no SW Fall Back
                /*if(caps.uSceneChangeDetection)
                {
                    executeParams.bSceneDetectionEnable = true;
                }
                else
                {
                    bIsPartialAccel = true;
                }*/

                break;
            }

            case MFX_EXTBUFF_VPP_RESIZE:
            {
                break;// aya: make sense for SW_VPP only
            }

            case MFX_EXTBUFF_VPP_CSC:
            case MFX_EXTBUFF_VPP_CSC_OUT_RGB4:
            {
                break;// aya: make sense for SW_VPP only
            }

            case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
            {
                //default mode
                config.m_extConfig.mode = FRC_ENABLED | FRC_STANDARD;
                config.m_extConfig.frcRational[VPP_IN].FrameRateExtN = videoParam.vpp.In.FrameRateExtN;
                config.m_extConfig.frcRational[VPP_IN].FrameRateExtD = videoParam.vpp.In.FrameRateExtD;
                config.m_extConfig.frcRational[VPP_OUT].FrameRateExtN = videoParam.vpp.Out.FrameRateExtN;
                config.m_extConfig.frcRational[VPP_OUT].FrameRateExtD = videoParam.vpp.Out.FrameRateExtD;

                config.m_surfCount[VPP_IN]  = IPP_MAX(2, config.m_surfCount[VPP_IN]);
                config.m_surfCount[VPP_OUT] = IPP_MAX(2, config.m_surfCount[VPP_OUT]);//aya fixme ????

                // aya: driver supports GFX FRC for progressive content only and NV12 input!!!
                bool isProgressiveStream = ((MFX_PICSTRUCT_PROGRESSIVE == videoParam.vpp.In.PicStruct) && 
                    (MFX_PICSTRUCT_PROGRESSIVE == videoParam.vpp.Out.PicStruct)) ? true : false;

                bool isNV12Input = (MFX_FOURCC_NV12 == videoParam.vpp.In.FourCC) ? true : false;
                if(caps.uFrameRateConversion && 
                   (MFX_FRCALGM_FRAME_INTERPOLATION == GetMFXFrcMode(videoParam)) &&
                    isProgressiveStream &&
                    isNV12Input)
                {
                    mfxF64 inFrameRate  = CalculateUMCFramerate(videoParam.vpp.In.FrameRateExtN,  videoParam.vpp.In.FrameRateExtD);
                    mfxF64 outFrameRate = CalculateUMCFramerate(videoParam.vpp.Out.FrameRateExtN, videoParam.vpp.Out.FrameRateExtD);
                    mfxF64 mfxRatio = outFrameRate / inFrameRate;
                    
                    mfxU32 frcCount = (mfxU32)caps.frcCaps.customRateData.size();
                    mfxF64 FRC_EPS = 0.01;

                    for(mfxU32 frcIdx = 0; frcIdx < frcCount; frcIdx++)
                    {
                        CustomRateData* rateData = &(caps.frcCaps.customRateData[frcIdx]);
                        // it is outFrameRate / inputFrameRate.
                        mfxF64 gfxRatio = CalculateUMCFramerate(rateData->customRate.FrameRateExtN, rateData->customRate.FrameRateExtD);

                        if( fabs(gfxRatio - mfxRatio) < FRC_EPS )
                        {
                            config.m_extConfig.mode = FRC_INTERPOLATION;
                            config.m_extConfig.customRateData = caps.frcCaps.customRateData[frcIdx];
                            
                            mfxU32 framesCount = (rateData->bkwdRefCount + 1 + rateData->fwdRefCount);
                            config.m_surfCount[VPP_IN]  = (mfxU16)IPP_MAX(framesCount, config.m_surfCount[VPP_IN]);
                            config.m_surfCount[VPP_OUT] = (mfxU16)IPP_MAX(rateData->outputIndexCountPerCycle, config.m_surfCount[VPP_OUT]);

                            executeParams.bFRCEnable     = true;
                            executeParams.customRateData = caps.frcCaps.customRateData[frcIdx];
                        }
                    }
                }
                
                
                inDNRatio = (mfxF64) videoParam.vpp.In.FrameRateExtD / videoParam.vpp.In.FrameRateExtN;
                outDNRatio = (mfxF64) videoParam.vpp.Out.FrameRateExtD / videoParam.vpp.Out.FrameRateExtN;

                break;
            }

            case MFX_EXTBUFF_VPP_ITC:
            {   
                // simulating inverse telecine by simple frame skipping and bob deinterlacing
                config.m_extConfig.mode = FRC_ENABLED | FRC_STANDARD;

                config.m_extConfig.frcRational[VPP_IN].FrameRateExtN = videoParam.vpp.In.FrameRateExtN;
                config.m_extConfig.frcRational[VPP_IN].FrameRateExtD = videoParam.vpp.In.FrameRateExtD;
                config.m_extConfig.frcRational[VPP_OUT].FrameRateExtN = videoParam.vpp.Out.FrameRateExtN;
                config.m_extConfig.frcRational[VPP_OUT].FrameRateExtD = videoParam.vpp.Out.FrameRateExtD;
                
                inDNRatio  = (mfxF64) videoParam.vpp.In.FrameRateExtD / videoParam.vpp.In.FrameRateExtN;
                outDNRatio = (mfxF64) videoParam.vpp.Out.FrameRateExtD / videoParam.vpp.Out.FrameRateExtN;
                
                executeParams.iDeinterlacingAlgorithm = MFX_DEINTERLACING_BOB;
                config.m_surfCount[VPP_IN]  = IPP_MAX(2, config.m_surfCount[VPP_IN]);
                config.m_surfCount[VPP_OUT] = IPP_MAX(2, config.m_surfCount[VPP_OUT]);

                break;
            }

#if defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)
            case MFX_EXTBUFF_VPP_IMAGE_STABILIZATION:
            {
                if(caps.uIStabFilter)
                {
                    executeParams.bImgStabilizationEnable = true;
                    executeParams.istabMode               = MFX_IMAGESTAB_MODE_BOXING;

                     for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
                    {
                        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_IMAGE_STABILIZATION)
                        {
                            mfxExtVPPImageStab *extIStab = (mfxExtVPPImageStab*) videoParam.ExtParam[i];

                            executeParams.istabMode = extIStab->Mode;

                            // aya: all checks provided on high level 
                            /*if( extIStab->Mode != MFX_IMAGESTAB_MODE_UPSCALE && extIStab->Mode != MFX_IMAGESTAB_MODE_BOXING )
                            {
                                return MFX_ERR_INVALID_VIDEO_PARAM;
                            }*/
                        }
                    }
                }
                else
                {
                    executeParams.bImgStabilizationEnable = false;
                    //bIsPartialAccel = true;
                }
                // no SW Fall Back

                break;
            }
#endif

            case MFX_EXTBUFF_VPP_VARIANCE_REPORT:
            {
                if(caps.uVariance)
                {
                    // aya: experimental!!! FRC/ITC/60i60p should be disbaled
                    config.m_surfCount[VPP_IN]  = IPP_MAX(2, config.m_surfCount[VPP_IN]);
                    config.m_surfCount[VPP_OUT] = IPP_MAX(1, config.m_surfCount[VPP_OUT]);

                    config.m_extConfig.mode = VARIANCE_REPORT;
                    config.m_extConfig.customRateData.bkwdRefCount = 1;//caps.iNumBackwardSamples;
                    config.m_extConfig.customRateData.fwdRefCount  = 0;//caps.iNumForwardSamples;
                    config.m_extConfig.customRateData.inputFramesOrFieldPerCycle= 1;
                    config.m_extConfig.customRateData.outputIndexCountPerCycle  = 1;//

                    executeParams.bVarianceEnable = true;
                }
                // no SW Fall Back

                break;
            }

            case MFX_EXTBUFF_VPP_COMPOSITE:
            {
                mfxU32 StreamCount = 0;
                for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
                {
                    if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_COMPOSITE)
                    {
                        mfxExtVPPComposite* extComp = (mfxExtVPPComposite*) videoParam.ExtParam[i];
                        StreamCount = extComp->NumInputStream;
                        for (mfxU32 cnt = 0; cnt < StreamCount; ++cnt)
                        {
                            DstRect rec = {0};
                            rec.DstX = extComp->InputStream[cnt].DstX;
                            rec.DstY = extComp->InputStream[cnt].DstY;
                            rec.DstW = extComp->InputStream[cnt].DstW;
                            rec.DstH = extComp->InputStream[cnt].DstH;
                            executeParams.dstRects.push_back( rec );
                        }
                        break;
                    }
                }

                config.m_surfCount[VPP_IN]  = (mfxU16)IPP_MAX(StreamCount, config.m_surfCount[VPP_IN]);
                config.m_surfCount[VPP_OUT] = (mfxU16)IPP_MAX(1, config.m_surfCount[VPP_OUT]);

                config.m_extConfig.mode = COMPOSITE;
                config.m_extConfig.customRateData.bkwdRefCount = 0;
                config.m_extConfig.customRateData.fwdRefCount  = StreamCount-1; // count only secondary streams
                config.m_extConfig.customRateData.inputFramesOrFieldPerCycle= StreamCount;
                config.m_extConfig.customRateData.outputIndexCountPerCycle  = 1;

                executeParams.bComposite = true;
                    
                break;
            }

            default:
            {
                // there is no such capabilities
                bIsFilterSkipped = true;
                break;
            }
        }//switch(curFilterId)
    } // for (mfxU32 i = 0; i < filtersNum; i += 1)
    //-----------------------------------------------------

    // Post Correction Params
    if(MFX_FRCALGM_DISTRIBUTED_TIMESTAMP == GetMFXFrcMode(videoParam))
    {
        config.m_extConfig.mode = FRC_ENABLED | FRC_DISTRIBUTED_TIMESTAMP;

        if (config.m_bMode30i60pEnable)
        {
            config.m_extConfig.mode = FRC_DISABLED | FRC_DISTRIBUTED_TIMESTAMP;
        }
    }

    if (true == executeParams.bSceneDetectionEnable && (FRC_ENABLED | config.m_extConfig.mode))
    {
        // disable scene detection
        executeParams.bSceneDetectionEnable = false;
    }

    if ((FRC_ENABLED & config.m_extConfig.mode) && 
        (2 == pipelineList.size()) &&
        IsFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), MFX_EXTBUFF_VPP_RESIZE))
    {
        // check that no roi or scaling was requested
        if ((videoParam.vpp.In.Width == videoParam.vpp.Out.Width && videoParam.vpp.In.Height == videoParam.vpp.Out.Height) &&
            (videoParam.vpp.In.CropW == videoParam.vpp.Out.CropW && videoParam.vpp.In.CropH == videoParam.vpp.Out.CropH) && 
            (videoParam.vpp.In.CropX == videoParam.vpp.Out.CropX && videoParam.vpp.In.CropY == videoParam.vpp.Out.CropY)
            )
        {
            //m_bPassThrough = true;
        }
    }

    if (inDNRatio == outDNRatio && !executeParams.bVarianceEnable && !executeParams.bComposite)
    {
        // work around
        config.m_extConfig.mode  = FRC_DISABLED;
        config.m_bPassThroughEnable = false;
    }

    return (bIsFilterSkipped) ? MFX_WRN_FILTER_SKIPPED : MFX_ERR_NONE;

} // mfxStatus ConfigureExecuteParams(...)


//---------------------------------------------------------
// UTILS
//---------------------------------------------------------
VideoVPPHW::IOMode   VideoVPPHW::GetIOMode(
    mfxVideoParam *par,
    mfxFrameAllocRequest* opaqReq)
{
    IOMode mode = VideoVPPHW::ALL;

    switch (par->IOPattern)
    {
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY:

        mode = VideoVPPHW::SYS_TO_SYS;
        break;

    case MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY:

        mode = VideoVPPHW::D3D_TO_SYS;
        break;

    case MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY:

        mode = VideoVPPHW::SYS_TO_D3D;
        break;

    case MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY:

        mode = VideoVPPHW::D3D_TO_D3D;
        break;

        // OPAQ support: we suggest that OPAQ means D3D for HW_VPP by default
    case MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY:
        {
            mode = VideoVPPHW::D3D_TO_SYS;

            if( opaqReq[VPP_IN].Type & MFX_MEMTYPE_SYSTEM_MEMORY )
            {
                mode = VideoVPPHW::SYS_TO_SYS;
            }
            break;
        }

    case MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY:
        {
            mode = VideoVPPHW::D3D_TO_D3D;

            if( opaqReq[VPP_IN].Type & MFX_MEMTYPE_SYSTEM_MEMORY )
            {
                mode = VideoVPPHW::SYS_TO_D3D;
            }

            break;
        }

    case MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY:
        {
            mode = VideoVPPHW::D3D_TO_D3D;

            if( (opaqReq[VPP_IN].Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (opaqReq[VPP_OUT].Type & MFX_MEMTYPE_SYSTEM_MEMORY) )
            {
                mode = VideoVPPHW::SYS_TO_SYS;
            }
            else if( opaqReq[VPP_IN].Type & MFX_MEMTYPE_SYSTEM_MEMORY )
            {
                mode = VideoVPPHW::SYS_TO_D3D;
            }
            else if( opaqReq[VPP_OUT].Type & MFX_MEMTYPE_SYSTEM_MEMORY )
            {
                mode = VideoVPPHW::D3D_TO_SYS;
            }

            break;
        }

    case MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY:
        {
            mode = VideoVPPHW::SYS_TO_D3D;

            if( opaqReq[VPP_OUT].Type & MFX_MEMTYPE_SYSTEM_MEMORY )
            {
                mode = VideoVPPHW::SYS_TO_SYS;
            }

            break;
        }

    case MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY:
        {
            mode = VideoVPPHW::D3D_TO_D3D;

            if( opaqReq[VPP_OUT].Type & MFX_MEMTYPE_SYSTEM_MEMORY )
            {
                mode = VideoVPPHW::D3D_TO_SYS;
            }

            break;
        }
    }// switch ioPattern

    return mode;

} // IOMode VideoVPPHW::GetIOMode(...)


mfxStatus CheckIOMode(mfxVideoParam *par, VideoVPPHW::IOMode mode)
{
    switch (mode)
    {
    case VideoVPPHW::D3D_TO_D3D:

        if (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY ||
            par->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            return MFX_ERR_UNSUPPORTED;
        break;

    case VideoVPPHW::D3D_TO_SYS:

        if (par->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
            par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
            return MFX_ERR_UNSUPPORTED;
        break;

    case VideoVPPHW::SYS_TO_D3D:

        if (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY ||
            par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
            return MFX_ERR_UNSUPPORTED;
        break;

    case VideoVPPHW::SYS_TO_SYS:

        if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY ||
            par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
            return MFX_ERR_UNSUPPORTED;
        break;

    case VideoVPPHW::ALL:
            return MFX_ERR_NONE;
        break;

    default:

        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;

} // mfxStatus CheckIOMode(mfxVideoParam *par, IOMode mode)


mfxU32 GetMFXFrcMode(const mfxVideoParam & videoParam)
{
    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
    {
        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION)
        {
            mfxExtVPPFrameRateConversion *extFrc = (mfxExtVPPFrameRateConversion *) videoParam.ExtParam[i];
            return extFrc->Algorithm;            
        }
    }

    return 0;//EMPTY

} //  mfxU32 GetMFXFrcMode(mfxVideoParam & videoParam)


mfxStatus SetMFXFrcMode(const mfxVideoParam & videoParam, mfxU32 mode)
{
    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
    {
        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION)
        {
            mfxExtVPPFrameRateConversion *extFrc = (mfxExtVPPFrameRateConversion *) videoParam.ExtParam[i];
            extFrc->Algorithm = (mfxU16)mode;

            return MFX_ERR_NONE;
        }
    }

    return MFX_WRN_VALUE_NOT_CHANGED;

} // mfxStatus SetMFXFrcMode(const mfxVideoParam & videoParam, mfxU32 mode)


mfxStatus SetMFXISMode(const mfxVideoParam & videoParam, mfxU32 mode)
{
    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
    {
        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_IMAGE_STABILIZATION)
        {
            mfxExtVPPImageStab *extFrc = (mfxExtVPPImageStab *) videoParam.ExtParam[i];
            extFrc->Mode = (mfxU16)mode;

            return MFX_ERR_NONE;
        }
    }

    return MFX_WRN_VALUE_NOT_CHANGED;
}


MfxFrameAllocResponse::MfxFrameAllocResponse()
    : m_core (0)
    , m_numFrameActualReturnedByAllocFrames(0)
{
    Zero(dynamic_cast<mfxFrameAllocResponse&>(*this));
} // MfxFrameAllocResponse::MfxFrameAllocResponse()


mfxStatus MfxFrameAllocResponse::Free( void )
{
    if (m_core == 0)
        return MFX_ERR_NONE;

    if (MFX_HW_D3D11  == m_core->GetVAType())
    {
        for (size_t i = 0; i < m_responseQueue.size(); i++)
        {
            m_core->FreeFrames(&m_responseQueue[i]);
        }

        m_responseQueue.resize(0);
    }
    else
    {
        if (mids)
        {
            NumFrameActual = m_numFrameActualReturnedByAllocFrames;
            m_core->FreeFrames(this);
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus MfxFrameAllocResponse::Free( void )


MfxFrameAllocResponse::~MfxFrameAllocResponse()
{
    Free();

} // MfxFrameAllocResponse::~MfxFrameAllocResponse()


mfxStatus MfxFrameAllocResponse::Alloc(
    VideoCORE *            core,
    mfxFrameAllocRequest & req, bool isCopyReqiured)
{
    req.NumFrameSuggested = req.NumFrameMin; // no need in 2 different NumFrames

    if (MFX_HW_D3D11  == core->GetVAType())
    {
        mfxFrameAllocRequest tmp = req;
        tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

        m_responseQueue.resize(req.NumFrameMin);
        m_mids.resize(req.NumFrameMin);

        for (int i = 0; i < req.NumFrameMin; i++)
        {
            mfxStatus sts = core->AllocFrames(&tmp, &m_responseQueue[i],isCopyReqiured);
            MFX_CHECK_STS(sts);
            m_mids[i] = m_responseQueue[i].mids[0];
        }

        mids = &m_mids[0];
        NumFrameActual = req.NumFrameMin;
    }
    else
    {
        mfxStatus sts = core->AllocFrames(&req, this, isCopyReqiured);
        MFX_CHECK_STS(sts);
    }

    if (NumFrameActual < req.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;

    m_core = core;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin; // no need in redundant frames
    return MFX_ERR_NONE;
}

mfxStatus MfxFrameAllocResponse::Alloc(
    VideoCORE *            core,
    mfxFrameAllocRequest & req,
    mfxFrameSurface1 **    opaqSurf,
    mfxU32                 numOpaqSurf)
{
    req.NumFrameSuggested = req.NumFrameMin; // no need in 2 different NumFrames

    mfxStatus sts = core->AllocFrames(&req, this, opaqSurf, numOpaqSurf);
    MFX_CHECK_STS(sts);

    if (NumFrameActual < req.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;

    m_core = core;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin; // no need in redundant frames
    return MFX_ERR_NONE;
}


mfxStatus CopyFrameDataBothFields(
    VideoCORE *          core,
    mfxFrameData /*const*/ & dst,
    mfxFrameData /*const*/ & src,
    mfxFrameInfo const & info)
{
    dst.MemId = 0;
    src.MemId = 0;
    mfxFrameSurface1 surfSrc = { {0,}, info, src };
    mfxFrameSurface1 surfDst = { {0,}, info, dst };

    return core->DoFastCopyExtended(&surfDst, &surfSrc);

} // mfxStatus CopyFrameDataBothFields(

#endif // MFX_ENABLE_VPP
/* EOF */
