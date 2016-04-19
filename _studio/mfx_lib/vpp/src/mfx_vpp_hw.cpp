/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#include <algorithm>
#include <assert.h>
#include <limits>
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

// GPU Copy V.S. Composition hack
#ifdef MFX_VA_LINUX
#include "libmfx_core_vaapi.h"
#endif

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

// aya: quick fix for CM
const int TFF2TFF = 0x0;
const int TFF2BFF = 0x1;
const int BFF2TFF = 0x2;
const int BFF2BFF = 0x3;
const int FRAME2FRAME = 0x16;
////////////////////////////////////////////////////////////////////////////////////
// Utils
////////////////////////////////////////////////////////////////////////////////////

static void MemSetZero4mfxExecuteParams (mfxExecuteParams *pMfxExecuteParams )
{
    memset(&pMfxExecuteParams->targetSurface, 0, sizeof(mfxDrvSurface));
    pMfxExecuteParams->pRefSurfaces = NULL ;
    pMfxExecuteParams->dstRects.clear(); /* NB! Due to STL container memset can not be used */
    memset(&pMfxExecuteParams->customRateData, 0, sizeof(CustomRateData));
    pMfxExecuteParams->targetTimeStamp = 0;
    pMfxExecuteParams->refCount = 0;
    pMfxExecuteParams->bkwdRefCount = 0;
    pMfxExecuteParams->fwdRefCount = 0;
    pMfxExecuteParams->iDeinterlacingAlgorithm = 0;
    pMfxExecuteParams->bFMDEnable = 0;
    pMfxExecuteParams->bDenoiseAutoAdjust = 0;
    pMfxExecuteParams->bDetailAutoAdjust = 0;
    pMfxExecuteParams->denoiseFactor = 0;
    pMfxExecuteParams->detailFactor = 0;
    pMfxExecuteParams->iTargetInterlacingMode = 0;
    pMfxExecuteParams->bEnableProcAmp = false;
    pMfxExecuteParams->Brightness = 0;
    pMfxExecuteParams->Contrast = 0;
    pMfxExecuteParams->Hue = 0;
    pMfxExecuteParams->Saturation = 0;
    pMfxExecuteParams->bSceneDetectionEnable = false;
    pMfxExecuteParams->bVarianceEnable = false;
    pMfxExecuteParams->bImgStabilizationEnable = false;
    pMfxExecuteParams->istabMode = 0;
    pMfxExecuteParams->bFRCEnable = false;
    pMfxExecuteParams->bComposite = false;
    pMfxExecuteParams->iBackgroundColor = 0;
    pMfxExecuteParams->statusReportID = 0;
    pMfxExecuteParams->bFieldWeaving = false;
    pMfxExecuteParams->iFieldProcessingMode = 0;
    pMfxExecuteParams->bCameraPipeEnabled = false;
    pMfxExecuteParams->bCameraBlackLevelCorrection = false;
    pMfxExecuteParams->bCameraGammaCorrection = false;
    pMfxExecuteParams->bCameraHotPixelRemoval = false;
    pMfxExecuteParams->bCameraWhiteBalaceCorrection = false;
    pMfxExecuteParams->bCCM = false;
    pMfxExecuteParams->bCameraLensCorrection = false;
    pMfxExecuteParams->rotation = 0;
    pMfxExecuteParams->scalingMode = MFX_SCALING_MODE_DEFAULT;
    pMfxExecuteParams->bEOS = false;
    pMfxExecuteParams->scene = VPP_SCENE_NO_CHANGE;
} /*void MemSetZero4mfxExecuteParams (mfxExecuteParams *pMfxExecuteParams )*/



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
mfxStatus ValidateParams(mfxVideoParam *par, mfxVppCaps *caps, VideoCORE *core, bool bCorrectionEnable = false);
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

    std::vector<mfxFrameSurface1 *>::iterator iterator;

    if (m_in_stamp + m_in_tick <= m_out_stamp)
    {
        // skip frame
        // request new one input surface
        m_in_stamp += m_in_tick;
        return MFX_ERR_MORE_DATA;
    }
    else if (m_out_stamp + m_out_tick < m_in_stamp)
    {
        // Save current input surface and request more output surfaces
        iterator = std::find(m_LockedSurfacesList.begin(), m_LockedSurfacesList.end(), input);

        if (m_LockedSurfacesList.end() == iterator)
        {
            m_LockedSurfacesList.push_back(input);
        }

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
            m_LockedSurfacesList.erase(m_LockedSurfacesList.begin());
        }
        m_in_stamp += m_in_tick;
    }

    m_out_stamp += m_out_tick;

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
    if ((input->Data.TimeStamp != (mfxU64)-1) && !m_bUpFrameRate && inputTimeStamp < m_expectedTimeStamp)
    {
        m_bDownFrameRate = true;

        // calculate output time stamp
        output->Data.TimeStamp = m_expectedTimeStamp;

        // skip frame
        // request new one input surface
        return MFX_ERR_MORE_DATA;
    }
    else if ((input->Data.TimeStamp != (mfxU64)-1) && !m_bDownFrameRate && (input->Data.TimeStamp > m_expectedTimeStamp || m_timeStampDifference))
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
        (config.m_extConfig.mode & COMPOSITE) ||
        (config.m_extConfig.mode & IS_REFERENCES))
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
            m_EOS = false;
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
            m_EOS = true;
            if (m_surfQueue.size() > 0)
            {
                *intSts = MFX_ERR_NONE;
                m_outputIndexCountPerCycle = 1;
                m_bkwdRefCount = 0;
                m_outputIndex  = 0; // marker task end;

                m_pSubResource = CreateSubResourceForMode30i60p();
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
    /* KW fix*/
    //memset(subRes, 0, sizeof(ReleaseResource));
    subRes->refCount = 0;
    subRes->surfaceListForRelease.clear();

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
    /* KW fix*/
    //memset(subRes, 0, sizeof(ReleaseResource));
    subRes->refCount = 0;
    subRes->surfaceListForRelease.clear();


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
    pTask->bEOS = m_EOS;
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

    bool isAdvGfxMode = (COMPOSITE & m_extMode) || (FRC_INTERPOLATION & m_extMode) ||
                                            (VARIANCE_REPORT & m_extMode) || (IS_REFERENCES & m_extMode)? true : false;

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
    else if (IS_REFERENCES & m_extMode) /* case for ADI with references */
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

    pTask->bAdvGfxEnable     = (COMPOSITE & m_extMode) || (FRC_INTERPOLATION & m_extMode) ||
                                                         (VARIANCE_REPORT & m_extMode) || (IS_REFERENCES & m_extMode)? true : false;

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
        if ((0 != taskIndex % 2) ||
           (NULL == input)) /* Second condition is prevent from incorrect usage */
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

    /* KW fix */
    MemSetZero4mfxExecuteParams(&m_executeParams);

    // sync workload mode by default
    m_workloadMode = VPP_SYNC_WORKLOAD;

    m_ddi = 0;
    m_bMultiView = false;

#if defined(MFX_VA)
    // cm devices
    m_pCmCopy    = NULL;
    m_pCmDevice  = NULL;
    m_pCmProgram = NULL;
    m_pCmKernel  = NULL;
#endif

} // VideoVPPHW::VideoVPPHW(IOMode mode, VideoCORE *core)


VideoVPPHW::~VideoVPPHW()
{
    Close();

} // VideoVPPHW::~VideoVPPHW()


mfxStatus VideoVPPHW::GetVideoParams(mfxVideoParam *par) const
{
    MFX_CHECK_NULL_PTR1(par);

    /* Step 1: Fill filters in use in DOUSE if specified.
     *  This step is skipped since it's done on upper level already
     */
    if( NULL == par->ExtParam || 0 == par->NumExtParam)
    {
        return MFX_ERR_NONE;
    }

    for( mfxU32 i = 0; i < par->NumExtParam; i++ )
    {
        MFX_CHECK_NULL_PTR1(par->ExtParam[i]);
        mfxU32 bufferId = par->ExtParam[i]->BufferId;
        if(MFX_EXTBUFF_VPP_DEINTERLACING == bufferId)
        {
            mfxExtVPPDeinterlacing *bufDI = reinterpret_cast<mfxExtVPPDeinterlacing *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufDI);
            bufDI->Mode = static_cast<mfxU16>(m_executeParams.iDeinterlacingAlgorithm);
        }
        else if (MFX_EXTBUFF_VPP_DENOISE == bufferId)
        {
            mfxExtVPPDenoise *bufDN = reinterpret_cast<mfxExtVPPDenoise *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufDN);
            bufDN->DenoiseFactor = m_executeParams.denoiseFactorOriginal;
        }
        else if (MFX_EXTBUFF_VPP_PROCAMP == bufferId)
        {
            mfxExtVPPProcAmp *bufPA = reinterpret_cast<mfxExtVPPProcAmp *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufPA);
            bufPA->Brightness = m_executeParams.Brightness;
            bufPA->Contrast   = m_executeParams.Contrast;
            bufPA->Hue        = m_executeParams.Hue;
            bufPA->Saturation = m_executeParams.Saturation;
        }
        else if (MFX_EXTBUFF_VPP_DETAIL == bufferId)
        {
            mfxExtVPPDetail *bufDT = reinterpret_cast<mfxExtVPPDetail *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufDT);
            bufDT->DetailFactor = m_executeParams.detailFactorOriginal;
        }
        else if (MFX_EXTBUFF_VIDEO_SIGNAL_INFO == bufferId)
        {
            mfxExtVPPVideoSignalInfo *bufVSI = reinterpret_cast<mfxExtVPPVideoSignalInfo *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufVSI);
            bufVSI->In.NominalRange   = m_executeParams.VideoSignalInfoIn.NominalRange;
            bufVSI->In.TransferMatrix = m_executeParams.VideoSignalInfoIn.TransferMatrix;
            bufVSI->Out.NominalRange   = m_executeParams.VideoSignalInfoOut.NominalRange;
            bufVSI->Out.TransferMatrix = m_executeParams.VideoSignalInfoOut.TransferMatrix;
        }
        else if (MFX_EXTBUFF_VPP_COMPOSITE == bufferId)
        {
            mfxExtVPPComposite *bufComp = reinterpret_cast<mfxExtVPPComposite *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufComp);
            MFX_CHECK_NULL_PTR1(bufComp->InputStream);
            bufComp->NumInputStream = static_cast<mfxU16>(m_executeParams.dstRects.size());
            for (size_t k = 0; k < m_executeParams.dstRects.size(); k++)
            {
                MFX_CHECK_NULL_PTR1(bufComp->InputStream);
                bufComp->InputStream[k].DstX = m_executeParams.dstRects[k].DstX;
                bufComp->InputStream[k].DstY = m_executeParams.dstRects[k].DstY;
                bufComp->InputStream[k].DstW = m_executeParams.dstRects[k].DstW;
                bufComp->InputStream[k].DstH = m_executeParams.dstRects[k].DstH;
                bufComp->InputStream[k].GlobalAlpha       = m_executeParams.dstRects[k].GlobalAlpha;
                bufComp->InputStream[k].GlobalAlphaEnable = m_executeParams.dstRects[k].GlobalAlphaEnable;
                bufComp->InputStream[k].LumaKeyEnable = m_executeParams.dstRects[k].LumaKeyEnable;
                bufComp->InputStream[k].LumaKeyMax    = m_executeParams.dstRects[k].LumaKeyMax;
                bufComp->InputStream[k].LumaKeyMin     = m_executeParams.dstRects[k].LumaKeyMin;
                bufComp->InputStream[k].PixelAlphaEnable = m_executeParams.dstRects[k].PixelAlphaEnable;
            }

            bufComp->R = (m_executeParams.iBackgroundColor >> 16 ) & 0xFF;
            bufComp->G = (m_executeParams.iBackgroundColor >> 8  ) & 0xFF;
            bufComp->B = (m_executeParams.iBackgroundColor >> 0  ) & 0xFF;
        }
        else if (MFX_EXTBUFF_VPP_FIELD_PROCESSING == bufferId)
        {
            mfxExtVPPFieldProcessing *bufFP = reinterpret_cast<mfxExtVPPFieldProcessing *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufFP);
            mfxU32 mode = m_executeParams.iFieldProcessingMode - 1;
            if (FRAME2FRAME == mode)
            {
                bufFP->Mode = MFX_FIELDMODE_FRAME;
                bufFP->InField  = par->vpp.In.PicStruct;
                bufFP->OutField = par->vpp.Out.PicStruct;
            }
            else if (TFF2TFF == mode)
            {
                bufFP->Mode = MFX_VPP_COPY_FIELD;
                bufFP->InField  = MFX_PICSTRUCT_FIELD_TFF;
                bufFP->OutField = MFX_PICSTRUCT_FIELD_TFF;
            }
            else if (TFF2BFF == mode)
            {
                bufFP->Mode = MFX_VPP_COPY_FIELD;
                bufFP->InField  = MFX_PICSTRUCT_FIELD_TFF;
                bufFP->OutField = MFX_PICSTRUCT_FIELD_BFF;
            }
            else if (BFF2TFF == mode)
            {
                bufFP->Mode = MFX_VPP_COPY_FIELD;
                bufFP->InField  = MFX_PICSTRUCT_FIELD_BFF;
                bufFP->OutField = MFX_PICSTRUCT_FIELD_TFF;
            }
            else if (BFF2BFF == mode)
            {
                bufFP->Mode = MFX_VPP_COPY_FIELD;
                bufFP->InField  = MFX_PICSTRUCT_FIELD_BFF;
                bufFP->OutField = MFX_PICSTRUCT_FIELD_BFF;
            }
        }
        else if (MFX_EXTBUFF_VPP_ROTATION == bufferId)
        {
            mfxExtVPPRotation *bufRot = reinterpret_cast<mfxExtVPPRotation *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufRot);
            bufRot->Angle = static_cast<mfxU16>(m_executeParams.rotation);
        }
        else if (MFX_EXTBUFF_VPP_SCALING == bufferId)
        {
            mfxExtVPPScaling *bufSc = reinterpret_cast<mfxExtVPPScaling *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufSc);
            bufSc->ScalingMode = m_executeParams.scalingMode;
        }
        else if (MFX_EXTBUFF_VPP_MIRRORING == bufferId)
        {
            mfxExtVPPMirroring *bufMir = reinterpret_cast<mfxExtVPPMirroring *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufMir);
            bufMir->Type = static_cast<mfxU16>(m_executeParams.mirroring);
        }
        else if (MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION == bufferId)
        {
            mfxExtVPPFrameRateConversion *bufFRC = reinterpret_cast<mfxExtVPPFrameRateConversion *>(par->ExtParam[i]);
            MFX_CHECK_NULL_PTR1(bufFRC);
            bufFRC->Algorithm = m_executeParams.frcModeOrig;
        }
    }

    return MFX_ERR_NONE;
} // mfxStatus VideoVPPHW::GetVideoParams(mfxVideoParam *par) const

mfxStatus VideoVPPHW::Query(VideoCORE *core, mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK_NULL_PTR1(core);

    mfxStatus sts = MFX_ERR_NONE;
    VPPHWResMng *vpp_ddi = 0;
    mfxVideoParam params = *par;
    Config config = {0};
    MfxHwVideoProcessing::mfxExecuteParams executeParams;

    sts = CheckIOMode(par, VideoVPPHW::ALL);
    MFX_CHECK_STS(sts);

    sts = core->CreateVideoProcessing(&params);
    MFX_CHECK_STS(sts);

    core->GetVideoProcessing((mfxHDL*)&vpp_ddi);
    if (0 == vpp_ddi)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    mfxVppCaps caps;
    caps = vpp_ddi->GetCaps();

    sts = ValidateParams(&params, &caps, core, true);
    MFX_CHECK_STS(sts);

    config.m_IOPattern = 0;
    sts = ConfigureExecuteParams(params, caps, executeParams, config);

    return sts;
}

mfxStatus  VideoVPPHW::Init(
    mfxVideoParam *par,
    bool isTemporal)
{
    mfxStatus sts = MFX_ERR_NONE;
    bool bIsFilterSkipped = false;
    isTemporal;

    m_frame_num = 0;
    //-----------------------------------------------------
    // [1] high level check
    //-----------------------------------------------------
    MFX_CHECK_NULL_PTR1(par);

    sts = CheckIOMode(par, m_ioMode);
    MFX_CHECK_STS(sts);

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

    mfxExtBuffer* pHint = NULL;
    GetFilterParam(par, MFX_EXTBUFF_MVC_SEQ_DESC, &pHint);

    if( pHint )
    {
        /* Multi-view processing needs separate devices for each view. Using one device is not possible since 
         * bakward/forward references from different views will be messed up. Thus each VPPHW instance needs to
         * create its own device. Core is not able to handle this like it does for single view case.
         */
        try
        {
            m_ddi = new VPPHWResMng();
        }
        catch(std::bad_alloc)
        {
            return MFX_WRN_PARTIAL_ACCELERATION;
        }

        sts = m_ddi->CreateDevice(m_pCore);
        MFX_CHECK_STS(sts);

        m_bMultiView = true;

    }
    else
    {
        sts = m_pCore->CreateVideoProcessing(&m_params);
        MFX_CHECK_STS(sts);

        m_pCore->GetVideoProcessing((mfxHDL*)&m_ddi);
        if (0 == m_ddi)
        {
            return MFX_WRN_PARTIAL_ACCELERATION;
        }
    }

    mfxVppCaps caps;
    caps = m_ddi->GetCaps();

    sts = ValidateParams(&m_params, &caps, m_pCore);
    if( MFX_WRN_FILTER_SKIPPED == sts )
    {
        bIsFilterSkipped = true;
        sts = MFX_ERR_NONE;
    }
    MFX_CHECK_STS(sts);

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
        sts = (*m_ddi)->ReconfigDevice(m_executeParams.customRateData.indexRateConversion);
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

        if (m_config.m_surfCount[VPP_OUT] != m_internalVidSurf[VPP_OUT].NumFrameActual)
        {
            sts = m_internalVidSurf[VPP_OUT].Alloc(m_pCore, request, par->vpp.Out.FourCC != MFX_FOURCC_YV12);
            MFX_CHECK(MFX_ERR_NONE == sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        m_config.m_IOPattern |= MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

        m_config.m_surfCount[VPP_OUT] = request.NumFrameMin;
    }

    if (SYS_TO_SYS == m_ioMode || SYS_TO_D3D == m_ioMode ) // [IN == SYSTEM_MEMORY]
    {
        //m_config.m_surfCount[VPP_IN] = (mfxU16)(m_config.m_surfCount[VPP_IN] + m_asyncDepth);

        request.Info        = par->vpp.In;
        request.Type        = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_INTERNAL_FRAME;
        request.NumFrameMin = request.NumFrameSuggested = m_config.m_surfCount[VPP_IN] ;

        if (m_config.m_surfCount[VPP_IN] != m_internalVidSurf[VPP_IN].NumFrameActual)
        {
            sts = m_internalVidSurf[VPP_IN].Alloc(m_pCore, request, par->vpp.In.FourCC != MFX_FOURCC_YV12);
            MFX_CHECK(MFX_ERR_NONE == sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

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

    //-----------------------------------------------------
    // [6]  cm device
    //-----------------------------------------------------
#if defined(MFX_VA)
    if(m_pCmDevice) {
        int res = m_pCmDevice->LoadProgram((void *)genx_fcopy_cmcode, sizeof(genx_fcopy_cmcode), m_pCmProgram);
        if(res != 0 ) return MFX_ERR_DEVICE_FAILED;

        res = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(MbCopyFieLd), m_pCmKernel);  // MbCopyFieLd copies TOP or BOTTOM field from inSurf to OutSurf
        if(res != 0 ) return MFX_ERR_DEVICE_FAILED;

        res = m_pCmDevice->CreateQueue(m_pCmQueue);
        if(res != 0 ) return MFX_ERR_DEVICE_FAILED;
    }

#if defined(MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP)
    if (MFX_DEINTERLACING_ADVANCED_SCD == m_executeParams.iDeinterlacingAlgorithm)
    {
        mfxHandleType mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
        mfxHDL mfxDeviceHdl = 0;
        eMFXVAType vaType = m_pCore->GetVAType();
        if(MFX_HW_D3D9 == vaType)
        {
            mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
        }
        else if(MFX_HW_D3D11 == vaType)
        {
            mfxDeviceType = MFX_HANDLE_D3D11_DEVICE;
        }
        else if(MFX_HW_VAAPI == vaType)
        {
            mfxDeviceType = MFX_HANDLE_VA_DISPLAY;
        }
        sts = m_pCore->GetHandle(mfxDeviceType, &mfxDeviceHdl);
        MFX_CHECK_STS(sts);

        sts = m_SCD.Init(m_pCore, par->vpp.In.CropW, par->vpp.In.CropH, par->vpp.In.Width, par->vpp.In.PicStruct, 0, mfxDeviceType, mfxDeviceHdl);
        MFX_CHECK_STS(sts);

        m_SCD.SetGoPSize(HEVC_Gop);
    }
#endif

#ifdef VPP_MIRRORING
    if (m_executeParams.mirroring && ! m_pCmCopy)
    {
        m_pCmCopy = QueryCoreInterface<CmCopyWrapper>(m_pCore, MFXICORECMCOPYWRAPPER_GUID);
        if ( m_pCmCopy)
        {
            sts = m_pCmCopy->Initialize();
            MFX_CHECK_STS(sts);
            sts = m_pCmCopy->InitializeSwapKernels(m_pCore->GetHWType());
            MFX_CHECK_STS(sts);
        }
    }
#endif
#endif

    return (bIsFilterSkipped) ? MFX_WRN_FILTER_SKIPPED : MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::Init(mfxVideoParam *par, mfxU32 *tabUsedFiltersID, mfxU32 numOfFilters)



mfxStatus VideoVPPHW::QueryCaps(VideoCORE* core, MfxHwVideoProcessing::mfxVppCaps& caps)
{

    MFX_CHECK_NULL_PTR1(core);

    VPPHWResMng* ddi = 0;
    mfxStatus sts = MFX_ERR_NONE;
    mfxVideoParam tmpPar = {0};

    sts = core->CreateVideoProcessing(&tmpPar);
    MFX_CHECK_STS( sts );

    core->GetVideoProcessing((mfxHDL*)&ddi);
    if (0 == ddi)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    caps = ddi->GetCaps();

    return sts;

} // mfxStatus VideoVPPHW::QueryCaps(VideoCORE* core, MfxHwVideoProcessing::mfxVppCaps& caps)


mfxStatus VideoVPPHW::QueryIOSurf(
    IOMode ioMode,
    VideoCORE* core,
    mfxVideoParam* par,
    mfxFrameAllocRequest* request)
{
    mfxStatus sts = MFX_ERR_NONE;
    VPPHWResMng *vpp_ddi;
    MFX_CHECK_NULL_PTR1(par);

    sts = CheckIOMode(par, ioMode);
    MFX_CHECK_STS(sts);

    mfxVideoParam tmpPar = {0};
    sts = core->CreateVideoProcessing(&tmpPar);
    MFX_CHECK_STS(sts);

    core->GetVideoProcessing((mfxHDL*)&vpp_ddi);
    if (0 == vpp_ddi)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    mfxVppCaps caps;
    caps = vpp_ddi->GetCaps();

    sts = ValidateParams(par, &caps, core);
    if( MFX_WRN_FILTER_SKIPPED == sts )
    {
        sts = MFX_ERR_NONE;
    }
    MFX_CHECK_STS(sts);

    mfxExecuteParams executeParams;
    MemSetZero4mfxExecuteParams(&executeParams);
    Config  config = {};

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
    /* This is question: Should we free resource here or not?... */
    /* For Dynamic composition we should not to do it !!! */
    if (false == m_executeParams.bComposite)
    {
        m_taskMngr.Close();// all resource free here
    }

    /* Application should close the SDK component and then reinitialize it in case of the parameters change described below */
    if (m_params.vpp.In.PicStruct  != par->vpp.In.PicStruct ||
        m_params.vpp.Out.PicStruct != par->vpp.Out.PicStruct)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    const mfxF64 inFrameRateCur  = (mfxF64) par->vpp.In.FrameRateExtN      / (mfxF64) par->vpp.In.FrameRateExtD,
                 outFrameRateCur = (mfxF64) par->vpp.Out.FrameRateExtN     / (mfxF64) par->vpp.Out.FrameRateExtD,
                 inFrameRate     = (mfxF64) m_params.vpp.In.FrameRateExtN  / (mfxF64) m_params.vpp.In.FrameRateExtD,
                 outFrameRate    = (mfxF64) m_params.vpp.Out.FrameRateExtN / (mfxF64) m_params.vpp.Out.FrameRateExtD;

    if ( fabs(inFrameRateCur / outFrameRateCur - inFrameRate / outFrameRate) > std::numeric_limits<mfxF64>::epsilon() )
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; // Frame Rate ratio check

    if (m_params.vpp.In.FourCC  != par->vpp.In.FourCC ||
        m_params.vpp.Out.FourCC != par->vpp.Out.FourCC)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    return this->Init(par);

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

#if defined (MFX_VA)

#if defined (MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP)
    m_SCD.Close();
#endif

    // CM device
    if(m_pCmDevice) {
        m_pCmDevice->DestroyKernel(m_pCmKernel);
        m_pCmDevice->DestroyProgram(m_pCmProgram);
        //::DestroyCmDevice(device);
    }
#endif

    /* Device close semantic is different for multi-view and single view */
    if ( m_bMultiView )
    {
        /* In case of multi-view, VPPHW created dedicated resource manager with
         * device on Init, so need to close it be deleting resource manager
         */
        delete m_ddi;
        m_bMultiView = false;
    }
    else
    {
       /* In case of single-view, VPPHW needs just close device, but does not
        * need to destroy resource manager. It's still alive inside of the core.
        * Core will take care on its removal.
        */
        m_pCore->GetVideoProcessing((mfxHDL *)&m_ddi);
        if (m_ddi->GetDevice())
        {
            m_ddi->Close();
        }
    }

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
            pEntryPoint[0].pRoutineName = (char *)"VPP Query";

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
            pEntryPoint[1].pRoutineName = (char *)"VPP Query";

            // configure entry point
            pEntryPoint[0].pRoutine = VideoVPPHW::AsyncTaskSubmission;
            pEntryPoint[0].pParam = (void *) pTask;
            pEntryPoint[0].requiredNumThreads = 1;
            pEntryPoint[0].pState = (void *) this;
            pEntryPoint[0].pRoutineName = (char *)"VPP Submit";

            numEntryPoints = 2;
        }
        else
        {
            // configure entry point
            pEntryPoint[0].pRoutine = VideoVPPHW::AsyncTaskSubmission;
            pEntryPoint[0].pParam = (void *) pTask;
            pEntryPoint[0].requiredNumThreads = 1;
            pEntryPoint[0].pState = (void *) this;
            pEntryPoint[0].pRoutineName = (char *)"VPP Submit";

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
        if (m_IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
        {
            MFX_SAFE_CALL(m_pCore->GetFrameHDL( output.pSurf->Data.MemId, (mfxHDL *)&hdl) );
            m_executeParams.targetSurface.memId = output.pSurf->Data.MemId;
            m_executeParams.targetSurface.bExternal = false;
        }
        else
        {
            MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(output.pSurf->Data.MemId, (mfxHDL *)&hdl,false));
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
    mfxStatus sts = (*m_ddi)->Register(&out, 1, TRUE);
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

                mfxFrameSurface1 inputVidSurf = {};
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
            if(m_IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY){
                MFX_SAFE_CALL(m_pCore->GetFrameHDL(surfQueue[i].pSurf->Data.MemId, (mfxHDL *)&hdl));
                bExternal = false;
            }else{
                MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(surfQueue[i].pSurf->Data.MemId, (mfxHDL *)&hdl));
                bExternal = true;
            }
            in = hdl;

            
            memId     = surfQueue[i].pSurf->Data.MemId;
        }

        // register input surface
        sts = (*m_ddi)->Register(&in, 1, TRUE);
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
        m_executeSurf[i].memId = memId;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::PreWorkInputSurface(...)


mfxStatus VideoVPPHW::PostWorkOutSurface(ExtSurface & output)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoVPPHW::PostWorkOutSurface");
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

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "HW_VPP: Copy output (d3d->sys)");
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
    sts = (*m_ddi)->Register( &(m_executeParams.targetSurface.hdl), 1, FALSE);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::PostWorkOutSurface(ExtSurface & output)


mfxStatus VideoVPPHW::PostWorkInputSurface(mfxU32 numSamples)
{
    // unregister input surface(s) (make sense in case DX9 only)
    for (mfxU32 i = 0 ; i < numSamples; i += 1)
    {
        mfxStatus sts = (*m_ddi)->Register( &(m_executeSurf[i].hdl), 1, FALSE);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::PostWorkInputSurface(mfxU32 numSamples)


#if defined(MFX_VA)
#define CHECK_CM_ERR(ERR) if ((ERR) != CM_SUCCESS) return MFX_ERR_DEVICE_FAILED;

int RunGpu(
    void* inD3DSurf, void* outD3DSurf,
    int fieldMask,
    CmDevice *device, CmQueue *queue, CmKernel *kernel, int WIDTH, int HEIGHT)
{
    int res;
    CmSurface2D *input = 0;
    res = device->CreateSurface2D( inD3DSurf, input);
    CHECK_CM_ERR(res);

    CmSurface2D *output = 0;
    res = device->CreateSurface2D(outD3DSurf, output);
    CHECK_CM_ERR(res);

    unsigned int tsWidth = WIDTH / 16;
    unsigned int tsHeight = HEIGHT / 16;
    res = kernel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);

    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInput = 0;
    SurfaceIndex *idxOutput = 0;
    res = input->GetIndex(idxInput);
    CHECK_CM_ERR(res);
    res = output->GetIndex(idxOutput);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxOutput), idxOutput);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(int), &fieldMask);
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    //CmQueue *queue = 0;
    //res = device->CreateQueue(queue);
    //CHECK_CM_ERR(res);

    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);

    // wait result here!!!
    CM_STATUS sts;
    mfxStatus status = MFX_ERR_NONE;
    if (CM_SUCCESS == res && e)
    {
        e->GetStatus(sts);

        while (sts != CM_STATUS_FINISHED)
        {
            e->GetStatus(sts);
        }
    } else {
        status = MFX_ERR_DEVICE_FAILED;
    }

    if(e) queue->DestroyEvent(e);

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);

    //queue->DestroyEvent(e);
    device->DestroySurface(input);
    device->DestroySurface(output);

    return CM_SUCCESS;
}

// aya: quick fix!!! it is expected video->video, external memory only!!!
mfxStatus VideoVPPHW::ProcessFieldCopy(int mask)
{
    //int fieldMask = mask-1; // to get valid Sergey Osipov's kernel mask [0, 1, 2, 3]

    int sts = RunGpu(
        (m_executeSurf[0].hdl.first), (m_executeParams.targetSurface.hdl.first),
        mask-1, m_pCmDevice, m_pCmQueue, m_pCmKernel, m_executeSurf[0].frameInfo.Width, m_executeSurf[0].frameInfo.Height);

    CHECK_CM_ERR(sts);

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPHW::ProcessFieldCopy(std::vector<ExtSurface> & surfQueue /* IN */, ExtSurface & output /* OUT */)
#else
    mfxStatus VideoVPPHW::ProcessFieldCopy(int)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
#endif /* #if  defined(MFX_VA)*/


mfxStatus VideoVPPHW::MergeRuntimeParams(const DdiTask *pTask, MfxHwVideoProcessing::mfxExecuteParams *execParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(pTask);
    MFX_CHECK_NULL_PTR1(execParams);
    mfxU32 numSamples;
    std::vector<ExtSurface> inputSurfs;
    size_t i, indx;

    numSamples = pTask->bkwdRefCount + 1 + pTask->fwdRefCount;
    inputSurfs.resize(numSamples);
    execParams->VideoSignalInfo.assign(numSamples, m_executeParams.VideoSignalInfoIn);

    // bkwdFrames
    for(i = 0; i < pTask->bkwdRefCount; i++)
    {
        indx = i;
        inputSurfs[indx] = pTask->m_refList[i];
    }

    // cur Frame
    indx = pTask->bkwdRefCount;
    inputSurfs[indx] = pTask->input;

    // fwd Frames
    for( i = 0; i < pTask->fwdRefCount; i++ )
    {
        indx = pTask->bkwdRefCount + 1 + i;
        inputSurfs[indx] = pTask->m_refList[pTask->bkwdRefCount + i];
    }

    mfxExtVPPVideoSignalInfo *vsi;
    for (i = 0; i < numSamples; i++) 
    {
        // Update Video Signal info params for output
        vsi = reinterpret_cast<mfxExtVPPVideoSignalInfo *>( GetExtendedBuffer(inputSurfs[i].pSurf->Data.ExtParam, inputSurfs[i].pSurf->Data.NumExtParam, MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO));
        if (vsi)
        {
            /* Check params */
            if (vsi->TransferMatrix != MFX_TRANSFERMATRIX_BT601 &&
                vsi->TransferMatrix != MFX_TRANSFERMATRIX_BT709 &&
                vsi->TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }

            if (vsi->NominalRange != MFX_NOMINALRANGE_0_255 &&
                vsi->NominalRange != MFX_NOMINALRANGE_16_235 &&
                vsi->NominalRange != MFX_NOMINALRANGE_UNKNOWN)
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }

            /* Params look good */
            execParams->VideoSignalInfo[i].enabled        = true;
            execParams->VideoSignalInfo[i].NominalRange   = vsi->NominalRange;
            execParams->VideoSignalInfo[i].TransferMatrix = vsi->TransferMatrix;
        }
    }

    ExtSurface outputSurf = pTask->output;
    // Update Video Signal info params for output
    vsi = reinterpret_cast<mfxExtVPPVideoSignalInfo *>( GetExtendedBuffer(outputSurf.pSurf->Data.ExtParam, outputSurf.pSurf->Data.NumExtParam, MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO));
    if (vsi)
    {
            /* Check params */
            if (vsi->TransferMatrix != MFX_TRANSFERMATRIX_BT601 &&
                vsi->TransferMatrix != MFX_TRANSFERMATRIX_BT709 &&
                vsi->TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }

            if (vsi->NominalRange != MFX_NOMINALRANGE_0_255 &&
                vsi->NominalRange != MFX_NOMINALRANGE_16_235 &&
                vsi->NominalRange != MFX_NOMINALRANGE_UNKNOWN)
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }

            /* Params look good */
            execParams->VideoSignalInfoOut.enabled        = true;
            execParams->VideoSignalInfoOut.NominalRange   = vsi->NominalRange;
            execParams->VideoSignalInfoOut.TransferMatrix = vsi->TransferMatrix;
     }

    return sts;
}

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

    /* in VPP Field Copy mode we just copy field only and ignore driver call!!! */
    mfxU32 imfxFPMode = 0xffffffff;
    mfxExtVPPFieldProcessing *vppFieldProcessingParams = NULL;
    if (m_executeParams.iFieldProcessingMode != 0)
    {
        mfxFrameSurface1 * pInputSurface = surfQueue[0].pSurf;
        /* Mean filter was configured as DOUSE, but no ExtBuf in VPP Init()
         * And ... no any parameters in runtime. This is an error! */

        if ((m_executeParams.iFieldProcessingMode == 0xffffffff) && (pInputSurface->Data.NumExtParam == 0))
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        /* And one more check... If there is no ExtBuffer in RunTime we can use value from Init()*/
        if ((pInputSurface->Data.NumExtParam == 0) || (pInputSurface->Data.ExtParam == NULL))
        {
            imfxFPMode = m_executeParams.iFieldProcessingMode;
        }
        else /* So on looks like ExtBuf is present in runtime */
        {
            /* So on, we skip m_executeParams.iFieldProcessingMode from Init() */
            for ( mfxU32 jj = 0; jj < pInputSurface->Data.NumExtParam; jj++ )
            {
                if ( (pInputSurface->Data.ExtParam[jj]->BufferId == MFX_EXTBUFF_VPP_FIELD_PROCESSING) &&
                     (pInputSurface->Data.ExtParam[jj]->BufferSz == sizeof(mfxExtVPPFieldProcessing)) )
                {
                    vppFieldProcessingParams = (mfxExtVPPFieldProcessing *)(pInputSurface->Data.ExtParam[jj]);

                    if (vppFieldProcessingParams->Mode == MFX_VPP_COPY_FIELD)
                    {
                        if(vppFieldProcessingParams->InField ==  MFX_PICSTRUCT_FIELD_TFF)
                        {
                            imfxFPMode = (vppFieldProcessingParams->OutField == MFX_PICSTRUCT_FIELD_TFF) ?
                                            TFF2TFF : TFF2BFF;
                        } else
                        {
                            imfxFPMode = (vppFieldProcessingParams->OutField == MFX_PICSTRUCT_FIELD_TFF) ?
                                    BFF2TFF : BFF2BFF;
                        }
                    } /* if (vppFieldProcessingParams->Mode == MFX_VPP_COPY_FIELD)*/

                    if (vppFieldProcessingParams->Mode == MFX_VPP_COPY_FRAME)
                        imfxFPMode = FRAME2FRAME;
                } // if ( (pInputSurface->Data.ExtParam[jj]->BufferId == MFX_EXTBUFF_VPP_FIELD_PROCESSING) &&
            } // for ( mfxU32 jj = 0; jj < pInputSurface->Data.NumExtParam; jj++ )

            // "-1" to get valid kernel values is done internally
            // !!! kernel uses "0"  as a "valid" data, but HW_VPP doesn't
            // to prevent issue we increment here and decrement before kernel call
            imfxFPMode++;
        } // if ((pInputSurface->Data.NumExtParam == 0) || (pInputSurface->Data.ExtParam == NULL))
    }

    if ((m_executeParams.iFieldProcessingMode != 0) && /* If Mode is enabled*/
        ((imfxFPMode - 1) != (mfxU32)FRAME2FRAME)) /* And we don't do copy frame to frame lets call our FieldCopy*/
    /* And remember our previous line imfxFPMode++;*/
    {
        sts = PreWorkOutSurface(pTask->output);
        MFX_CHECK_STS(sts);

        sts = PreWorkInputSurface(surfQueue);
        MFX_CHECK_STS(sts);

        sts = ProcessFieldCopy(imfxFPMode);
        m_executeParams.pRefSurfaces = &m_executeSurf[0];


        sts = PostWorkOutSurface(pTask->output);
        MFX_CHECK_STS(sts);

        sts = PostWorkInputSurface(numSamples);
        MFX_CHECK_STS(sts);

        m_executeParams.targetSurface.memId = pTask->output.pSurf->Data.MemId;
        m_executeParams.statusReportID = pTask->taskIndex;
        return sts;
    }

#ifdef VPP_MIRRORING
    /* Temporal solution for mirroring that makes nothing but mirroring 
     * TODO: merge mirroring into pipeline
     */
    if (MFX_MIRRORING_HORIZONTAL == m_executeParams.mirroring &&
        MFX_HW_D3D11  != m_pCore->GetVAType()) // D3D11 has native support
    {
        sts = PreWorkOutSurface(pTask->output);
        MFX_CHECK_STS(sts);

        sts = PreWorkInputSurface(surfQueue);
        MFX_CHECK_STS(sts);

        if( (SYS_TO_SYS == m_ioMode || D3D_TO_SYS == m_ioMode))
        {
            /* Special case when mirroring is combined with GPU copy */
            mfxStatus sts = MFX_ERR_NONE;
            mfxMemId dstMemId;
            mfxFrameSurface1 dstSurface = {0};

            // save original mem ids
            dstMemId = pTask->output.pSurf->Data.MemId;

            dstSurface.Info = pTask->output.pSurf->Info;
            bool isDstLocked = false;

            /* Get destination. It's an external system mem always */
            if (NULL == pTask->output.pSurf->Data.Y)
            {
                sts = m_pCore->LockExternalFrame(dstMemId, &dstSurface.Data);
                MFX_CHECK_STS(sts);
                isDstLocked = true;
            }
            else
            {
                dstSurface.Data = pTask->output.pSurf->Data;
                dstSurface.Data.MemId = 0;
            }

            mfxI64 verticalPitch = (mfxI64)(dstSurface.Data.UV - dstSurface.Data.Y);
            verticalPitch = (verticalPitch % dstSurface.Data.Pitch)? 0 : verticalPitch / dstSurface.Data.Pitch;
            mfxU32 dstPitch = dstSurface.Data.PitchLow + ((mfxU32)dstSurface.Data.PitchHigh << 16);

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "HW_VPP: Mirror (d3d->sys)");
            IppiSize roi = {pTask->output.pSurf->Info.Width, pTask->output.pSurf->Info.Height};
            sts = m_pCmCopy->CopyMirrorVideoToSystemMemory(dstSurface.Data.Y, dstPitch, (mfxU32)verticalPitch, m_executeSurf[0].hdl.first, 0, roi, MFX_FOURCC_NV12);
            MFX_CHECK_STS(sts);

            if (true == isDstLocked)
            {
                sts = m_pCore->UnlockExternalFrame(dstMemId, &dstSurface.Data);
                MFX_CHECK_STS(sts);
            }
        }
        else
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "HW_VPP: Mirror (d3d->d3d)");
            IppiSize roi = {pTask->output.pSurf->Info.CropW, pTask->output.pSurf->Info.CropH};
            sts = m_pCmCopy->CopyMirrorVideoToVideoMemory(m_executeParams.targetSurface.hdl.first, m_executeSurf[0].hdl.first, roi, MFX_FOURCC_NV12);
            MFX_CHECK_STS(sts);

            sts = PostWorkOutSurface(pTask->output);
            MFX_CHECK_STS(sts);
        }

        sts = PostWorkInputSurface(numSamples);
        MFX_CHECK_STS(sts);

        m_executeParams.targetSurface.memId = pTask->output.pSurf->Data.MemId;
        m_executeParams.statusReportID = pTask->taskIndex;
        return sts;
    }
#endif

    sts = PreWorkOutSurface(pTask->output);
    MFX_CHECK_STS(sts);

    sts = PreWorkInputSurface(surfQueue);
    MFX_CHECK_STS(sts);

#if defined(MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP) && defined(MFX_VA)
    m_frame_num++;

    if (MFX_DEINTERLACING_ADVANCED_SCD == m_executeParams.iDeinterlacingAlgorithm)
    {
        BOOL analysisReady = false;

        /* Feed scene change detector with input rame */
        mfxFrameSurface1 * inFrame = surfQueue[pTask->bkwdRefCount].pSurf;
        MFX_CHECK_NULL_PTR1(inFrame);

        mfxU32 scene_change = 0;

        if (m_executeParams.bFMDEnable && m_frame_num % 2 != 0 && m_frame_num > 1)
        {
            // This frame was analyzed already, so just re-use data.
            scene_change = m_scene_change;
        }
        else
        {
            mfxHDL frameHandle;
            mfxFrameSurface1 frameSurface;
            memset(&frameSurface, 0, sizeof(mfxFrameSurface1));
            frameSurface.Info = inFrame->Info;

            if (SYS_TO_D3D == m_ioMode || SYS_TO_SYS == m_ioMode)
            {
                sts = m_pCore->GetFrameHDL(m_internalVidSurf[VPP_IN].mids[surfQueue[pTask->bkwdRefCount].resIdx], &frameHandle);
                MFX_CHECK_STS(sts);
            }
            else
            {
                if(m_IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
                {
                    MFX_SAFE_CALL(m_pCore->GetFrameHDL(surfQueue[pTask->bkwdRefCount].pSurf->Data.MemId, (mfxHDL *)&frameHandle));
                }
                else
                {
                    MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(surfQueue[pTask->bkwdRefCount].pSurf->Data.MemId, (mfxHDL *)&frameHandle));
                }
            }
            frameSurface.Data.MemId = frameHandle;

            sts = m_SCD.MapFrame(&frameSurface);
            MFX_CHECK_STS(sts);

            analysisReady = m_SCD.ProcessField(); // First field
            scene_change += m_SCD.Get_frame_shot_Decision() + m_SCD.Get_frame_last_in_scene();
            analysisReady = m_SCD.ProcessField(); // Second field
            scene_change += m_SCD.Get_frame_shot_Decision() + m_SCD.Get_frame_last_in_scene();
            m_scene_change = scene_change;
        }

        m_executeParams.scene = scene_change ? VPP_SCENE_NEW : VPP_SCENE_NO_CHANGE;
    }
#endif

    m_executeParams.refCount     = numSamples;
    m_executeParams.bkwdRefCount = pTask->bkwdRefCount;
    m_executeParams.fwdRefCount = pTask->fwdRefCount;
    m_executeParams.pRefSurfaces = &m_executeSurf[0];
    m_executeParams.bEOS         = pTask->bEOS;

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

    MfxHwVideoProcessing::mfxExecuteParams  execParams = m_executeParams;
    sts = MergeRuntimeParams(pTask, &execParams);
    MFX_CHECK_STS(sts);

    sts = (*m_ddi)->Execute(&execParams);
    if (sts != MFX_ERR_NONE)
    {
        pTask->SetFree(true);
        return sts;
    }

    MFX_CHECK_STS(sts);

    //aya: wa for variance
    if(execParams.bVarianceEnable)
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
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoVPPHW::AsyncTaskSubmission");
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
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoVPPHW::QueryTaskRoutine");
    mfxStatus sts = MFX_ERR_NONE;

    threadNumber = threadNumber;
    callNumber   = callNumber;

    VideoVPPHW *pHwVpp = (VideoVPPHW *) pState;
    DdiTask *pTask     = (DdiTask*) pParam;

    UMC::AutomaticUMCMutex guard(pHwVpp->m_guard);

    mfxU32 currentTaskIdx = pTask->taskIndex;

    if (0 == pHwVpp->m_executeParams.iFieldProcessingMode && ! pHwVpp->m_executeParams.mirroring) {
        sts = (*pHwVpp->m_ddi)->QueryTaskStatus(currentTaskIdx);
        MFX_CHECK_STS(sts);
    }

    //[2] Variance
    if( pTask->bVariance )
    {
        // aya: windows part
        std::vector<UINT> variance;

        sts = (*pHwVpp->m_ddi)->QueryVariance(
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

mfxStatus ValidateParams(mfxVideoParam *par, mfxVppCaps *caps, VideoCORE *core, bool bCorrectionEnable)
{
    MFX_CHECK_NULL_PTR3(par, caps, core);

    mfxStatus sts = MFX_ERR_NONE;

    /* 1. Check ext param */
    for (mfxU32 i = 0; i < par->NumExtParam; i++)
    {
        mfxU32 id    = par->ExtParam[i]->BufferId;
        void  *data  = par->ExtParam[i];
        MFX_CHECK_NULL_PTR1(data);

        switch(id)
        {
        case MFX_EXTBUFF_VPP_FIELD_PROCESSING:
        {
            mfxExtVPPFieldProcessing* extFP = (mfxExtVPPFieldProcessing*)data;

            if (extFP->Mode != MFX_VPP_COPY_FRAME && extFP->Mode != MFX_VPP_COPY_FIELD)
            {
                // Unsupported mode
                sts = (MFX_ERR_INVALID_VIDEO_PARAM < sts) ? MFX_ERR_INVALID_VIDEO_PARAM : sts;
            }

            if (extFP->Mode == MFX_VPP_COPY_FIELD)
            {
                if(extFP->InField != MFX_PICSTRUCT_FIELD_TFF && extFP->InField != MFX_PICSTRUCT_FIELD_BFF)
                {
                    // Copy field needs specific type of interlace
                    sts = (MFX_ERR_INVALID_VIDEO_PARAM < sts) ? MFX_ERR_INVALID_VIDEO_PARAM : sts;
                }
            }
            break;
        } //case MFX_EXTBUFF_VPP_FIELD_PROCESSING

        case MFX_EXTBUFF_VPP_DEINTERLACING:
        {
            mfxExtVPPDeinterlacing* extDI = (mfxExtVPPDeinterlacing*) data;

            if (extDI->Mode != MFX_DEINTERLACING_ADVANCED &&
#if defined(MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP)
                extDI->Mode != MFX_DEINTERLACING_ADVANCED_SCD &&
#endif
                extDI->Mode != MFX_DEINTERLACING_ADVANCED_NOREF &&
                extDI->Mode != MFX_DEINTERLACING_BOB)
            {
                sts = (MFX_ERR_UNSUPPORTED < sts) ? MFX_ERR_UNSUPPORTED : sts;
            }

            break;
        } //case MFX_EXTBUFF_VPP_DEINTERLACING
        case MFX_EXTBUFF_VPP_DOUSE:
        {
            mfxExtVPPDoUse*   extDoUse  = (mfxExtVPPDoUse*)data;
            MFX_CHECK_NULL_PTR1(extDoUse);
            for( mfxU32 algIdx = 0; algIdx < extDoUse->NumAlg; algIdx++ )
            {
                if( !CheckDoUseCompatibility( extDoUse->AlgList[algIdx] ) )
                {
                    sts = (MFX_ERR_UNSUPPORTED < sts) ? MFX_ERR_UNSUPPORTED : sts;
                    continue; // stop working with ExtParam[i]
                }

                if(MFX_EXTBUFF_VPP_COMPOSITE == extDoUse->AlgList[algIdx])
                {
                    sts = (MFX_ERR_INVALID_VIDEO_PARAM < sts) ? MFX_ERR_INVALID_VIDEO_PARAM : sts;
                    continue; // stop working with ExtParam[i]
                }

                if(MFX_EXTBUFF_VPP_FIELD_PROCESSING == extDoUse->AlgList[algIdx])
                {
                    /* NOTE:
                    * It's legal to use DOUSE for field processing,
                    * but application must attach appropriate ext buffer to mfxFrameData for each input surface
                    */
                    //sts = MFX_ERR_INVALID_VIDEO_PARAM;
                    //continue; // stop working with ExtParam[i]
                }
            }
            break;
        } //case MFX_EXTBUFF_VPP_DOUSE
        case MFX_EXTBUFF_VPP_COMPOSITE:
        {
            mfxExtVPPComposite* extComp = (mfxExtVPPComposite*)data;
            MFX_CHECK_NULL_PTR1(extComp);
            if (MFX_HW_D3D9 == core->GetVAType() && extComp->NumInputStream) // AlphaBlending & LumaKey filters are not supported at first sub-stream on DX9
            {
                if (extComp->InputStream[0].GlobalAlphaEnable || extComp->InputStream[0].LumaKeyEnable || extComp->InputStream[0].PixelAlphaEnable)
                {
                    sts = (MFX_ERR_INVALID_VIDEO_PARAM < sts) ? MFX_ERR_INVALID_VIDEO_PARAM : sts;
                }
            }

            break;
        } //case MFX_EXTBUFF_VPP_COMPOSITE
        } // switch
    }

    /* 2. Check size */
    if (par->vpp.In.Width > caps->uMaxWidth  || par->vpp.In.Height  > caps->uMaxHeight ||
        par->vpp.Out.Width > caps->uMaxWidth || par->vpp.Out.Height > caps->uMaxHeight)
    {
        sts = (MFX_ERR_NONE == sts) ? MFX_WRN_PARTIAL_ACCELERATION : sts;
    }

    /* 3. Check fourcc */
    if ( !(caps->mFormatSupport[par->vpp.In.FourCC] & MFX_FORMAT_SUPPORT_INPUT) || !(caps->mFormatSupport[par->vpp.Out.FourCC] & MFX_FORMAT_SUPPORT_OUTPUT) )
        sts = (MFX_ERR_NONE == sts) ? MFX_WRN_PARTIAL_ACCELERATION : sts;

    /* 4. p010 should be shifted (msdn) */
    if (MFX_FOURCC_P010 == par->vpp.In.FourCC && 0 == par->vpp.In.Shift)
        sts = (MFX_ERR_NONE == sts) ? MFX_WRN_PARTIAL_ACCELERATION : sts;

    if (MFX_FOURCC_P010 == par->vpp.Out.FourCC && 0 == par->vpp.Out.Shift)
        sts = (MFX_ERR_NONE == sts) ? MFX_WRN_PARTIAL_ACCELERATION : sts;

    /* 4. HSD 8159506 */
    if (MFX_FOURCC_YV12 == par->vpp.In.FourCC && MFX_HW_BDW < core->GetHWType() && (par->vpp.In.Width > 6144 || par->vpp.In.Height > 6144))
        sts = (MFX_ERR_NONE == sts) ? MFX_WRN_PARTIAL_ACCELERATION : sts;

    /* 5. Check unsupported filters on RGB */
    if( par->vpp.In.FourCC == MFX_FOURCC_RGB4)
    {
        std::vector<mfxU32> pipelineList;
        mfxStatus internalSts = GetPipelineList( par, pipelineList, true );
        MFX_CHECK_STS(internalSts);

        mfxU32  len   = (mfxU32)pipelineList.size();
        mfxU32* pList = (len > 0) ? (mfxU32*)&pipelineList[0] : NULL;

        if(IsFilterFound(pList, len, MFX_EXTBUFF_VPP_DENOISE)            ||
           IsFilterFound(pList, len, MFX_EXTBUFF_VPP_DETAIL)             ||
           IsFilterFound(pList, len, MFX_EXTBUFF_VPP_PROCAMP)            ||
           IsFilterFound(pList, len, MFX_EXTBUFF_VPP_SCENE_CHANGE)       ||
           IsFilterFound(pList, len, MFX_EXTBUFF_VPP_IMAGE_STABILIZATION) )
        {
            sts = (MFX_ERR_NONE == sts) ? MFX_WRN_FILTER_SKIPPED : sts;
            if(bCorrectionEnable)
            {
                std::vector <mfxU32> tmpZeroDoUseList; //zero
                SignalPlatformCapabilities(*par, tmpZeroDoUseList);
            }
        }
    }

    return sts;
} // mfxStatus VideoVPPHW::ValidateParams(mfxVideoParam *par, mfxVppCaps *caps, VideoCORE *core, bool bCorrectionEnable = false)


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
            if (extDI->Mode != MFX_DEINTERLACING_ADVANCED &&
#if defined(MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP)
                extDI->Mode != MFX_DEINTERLACING_ADVANCED_SCD &&
#endif
                extDI->Mode != MFX_DEINTERLACING_ADVANCED_NOREF &&
                extDI->Mode != MFX_DEINTERLACING_BOB)
            {
                return 0;
            }
            /* To check if driver support desired DI mode*/
            if ((MFX_DEINTERLACING_ADVANCED == extDI->Mode) && (caps.uAdvancedDI) )
                deinterlacingMode = extDI->Mode;
#if defined(MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP)
            if ((MFX_DEINTERLACING_ADVANCED_SCD == extDI->Mode) && (caps.uAdvancedDI) )
                deinterlacingMode = extDI->Mode;
#endif
            if ((MFX_DEINTERLACING_ADVANCED_NOREF == extDI->Mode) && (caps.uAdvancedDI) )
                deinterlacingMode = extDI->Mode;

            if ((MFX_DEINTERLACING_BOB == extDI->Mode) && (caps.uSimpleDI) )
                deinterlacingMode = extDI->Mode;
            /**/
            break;
        }
    }
    /* IF there is no DI ext buffer */
    if (caps.uAdvancedDI)
    {
        if (0 == deinterlacingMode)
        {
            deinterlacingMode = MFX_DEINTERLACING_ADVANCED;
        }
    }
    else if (caps.uSimpleDI)
    {
        if (0 == deinterlacingMode)
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

    if (videoParam.vpp.Out.FourCC == MFX_FOURCC_NV12 ||
        videoParam.vpp.Out.FourCC == MFX_FOURCC_YV12 ||
        videoParam.vpp.Out.FourCC == MFX_FOURCC_NV16 ||
        videoParam.vpp.Out.FourCC == MFX_FOURCC_YUY2 ||
        videoParam.vpp.Out.FourCC == MFX_FOURCC_P010 ||
        videoParam.vpp.Out.FourCC == MFX_FOURCC_P210)
    {
        executeParams.iBackgroundColor = 0xff108080; // black in YUV interpretation
    }
    else
    {
        executeParams.iBackgroundColor = 0xff000000; // black in RGB interpretation
    }

    //-----------------------------------------------------
    for (mfxU32 i = 0; i < pipelineList.size(); i += 1)
    {
        mfxU32 curFilterId = pipelineList[i];

        switch(curFilterId)
        {
            case MFX_EXTBUFF_VPP_DEINTERLACING:
            case MFX_EXTBUFF_VPP_DI:
            {
                /* this function also take into account is DI ext buffer present or does not */
                executeParams.iDeinterlacingAlgorithm = GetDeinterlaceMode( videoParam, caps );
                if (0 == executeParams.iDeinterlacingAlgorithm)
                {
                    /* Tragically case,
                     * Something wrong. User requested DI but MSDK can't do it */
                    return MFX_ERR_UNKNOWN;
                }
                if(MFX_DEINTERLACING_ADVANCED     == executeParams.iDeinterlacingAlgorithm
#if defined(MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP)
                || MFX_DEINTERLACING_ADVANCED_SCD == executeParams.iDeinterlacingAlgorithm
#endif
                )
                {
                    // use motion adaptive ADI with reference frame (quality)
                    config.m_bRefFrameEnable = true;
                    config.m_extConfig.customRateData.fwdRefCount = 0;
                    config.m_extConfig.customRateData.bkwdRefCount = 1; /* ref frame */
                    config.m_extConfig.customRateData.inputFramesOrFieldPerCycle= 1;
                    config.m_extConfig.customRateData.outputIndexCountPerCycle  = 1;
                    config.m_surfCount[VPP_IN]  = IPP_MAX(2, config.m_surfCount[VPP_IN]);
                    config.m_surfCount[VPP_OUT]  = IPP_MAX(1, config.m_surfCount[VPP_OUT]);
                    config.m_extConfig.mode = IS_REFERENCES;
                }
                else if (MFX_DEINTERLACING_ADVANCED_NOREF == executeParams.iDeinterlacingAlgorithm)
                {
                    executeParams.iDeinterlacingAlgorithm = MFX_DEINTERLACING_ADVANCED_NOREF;
                    config.m_bRefFrameEnable = false;
                    config.m_extConfig.customRateData.fwdRefCount = 0;
                    config.m_extConfig.customRateData.bkwdRefCount = 0; /* ref frame */
                    config.m_extConfig.customRateData.inputFramesOrFieldPerCycle= 1;
                    config.m_extConfig.customRateData.outputIndexCountPerCycle  = 1;
                    config.m_surfCount[VPP_IN]  = IPP_MAX(2, config.m_surfCount[VPP_IN]);
                    config.m_surfCount[VPP_OUT]  = IPP_MAX(1, config.m_surfCount[VPP_OUT]);
                    config.m_extConfig.mode = 0;
                }
                else if (0 == executeParams.iDeinterlacingAlgorithm)
                {
                    bIsFilterSkipped = true;
                }


                break;
            }

            case MFX_EXTBUFF_VPP_DI_30i60p:
            {
                /* this function also take into account is DI ext buffer present or does not */
                executeParams.iDeinterlacingAlgorithm = GetDeinterlaceMode( videoParam, caps );
                if (0 == executeParams.iDeinterlacingAlgorithm)
                {
                    /* Tragically case,
                     * Something wrong. User requested DI but MSDK can't do it */
                    return MFX_ERR_UNKNOWN;
                }

                config.m_bMode30i60pEnable = true;
                if(MFX_DEINTERLACING_ADVANCED     == executeParams.iDeinterlacingAlgorithm
#if defined(MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP)
                || MFX_DEINTERLACING_ADVANCED_SCD == executeParams.iDeinterlacingAlgorithm
#endif
                )
                {
                    // use motion adaptive ADI with reference frame (quality)
                    config.m_bRefFrameEnable = true;
                    config.m_extConfig.customRateData.bkwdRefCount = 1;
                    config.m_surfCount[VPP_IN]  = IPP_MAX(2, config.m_surfCount[VPP_IN]);
                    config.m_surfCount[VPP_OUT] = IPP_MAX(2, config.m_surfCount[VPP_OUT]);
                    executeParams.bFMDEnable = true;
                }
                else if(MFX_DEINTERLACING_BOB == executeParams.iDeinterlacingAlgorithm)
                {
                    // use ADI with spatial info, no reference frame (speed)
                    config.m_bRefFrameEnable = false;
                    config.m_surfCount[VPP_IN]  = IPP_MAX(1, config.m_surfCount[VPP_IN]);
                    config.m_surfCount[VPP_OUT] = IPP_MAX(2, config.m_surfCount[VPP_OUT]);
                    executeParams.bFMDEnable = false;
                }
                else if (MFX_DEINTERLACING_ADVANCED_NOREF == executeParams.iDeinterlacingAlgorithm)
                {
                    // use ADI with spatial info, no reference frame (speed)
                    executeParams.iDeinterlacingAlgorithm = MFX_DEINTERLACING_ADVANCED_NOREF;
                    config.m_bRefFrameEnable = false;
                    config.m_surfCount[VPP_IN]  = IPP_MAX(1, config.m_surfCount[VPP_IN]);
                    config.m_surfCount[VPP_OUT] = IPP_MAX(2, config.m_surfCount[VPP_OUT]);
                    executeParams.bFMDEnable = false;
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
                            executeParams.denoiseFactorOriginal = extDenoise->DenoiseFactor;
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
            case MFX_EXTBUFF_VPP_ROTATION:
            {
                if (caps.uRotation)
                {
                    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
                    {
                        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_ROTATION)
                        {
                            mfxExtVPPRotation *extRotate = (mfxExtVPPRotation*) videoParam.ExtParam[i];

                            if ( (MFX_ANGLE_0   != extRotate->Angle) &&
                                 (MFX_ANGLE_90  != extRotate->Angle) &&
                                 (MFX_ANGLE_180 != extRotate->Angle) &&
                                 (MFX_ANGLE_270 != extRotate->Angle) )
                                return MFX_ERR_INVALID_VIDEO_PARAM;

                            if ( ( MFX_PICSTRUCT_FIELD_TFF & videoParam.vpp.Out.PicStruct ) ||
                                 ( MFX_PICSTRUCT_FIELD_BFF & videoParam.vpp.Out.PicStruct ) )
                                return MFX_ERR_INVALID_VIDEO_PARAM;

                            executeParams.rotation = extRotate->Angle;
                        }
                    }
                }
                else
                {
                    bIsFilterSkipped = true;
                }

                break;
            }
            case MFX_EXTBUFF_VPP_SCALING:
            {
                if (caps.uScaling)
                {
                    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
                    {
                        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_SCALING)
                        {
                            mfxExtVPPScaling *extScaling = (mfxExtVPPScaling*) videoParam.ExtParam[i];
                            executeParams.scalingMode = extScaling->ScalingMode;
                        }
                    }
                }
                else
                {
                    bIsFilterSkipped = true;
                }

                break;
            }
#ifdef VPP_MIRRORING
            case MFX_EXTBUFF_VPP_MIRRORING:
            {
                if (caps.uMirroring)
                {
                    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
                    {
                        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_MIRRORING)
                        {
                            mfxExtVPPMirroring *extMirroring = (mfxExtVPPMirroring*) videoParam.ExtParam[i];
                            if (extMirroring->Type != MFX_MIRRORING_DISABLED && extMirroring->Type != MFX_MIRRORING_HORIZONTAL)
                            {
                                return MFX_ERR_INVALID_VIDEO_PARAM;
                            }

                            if (videoParam.vpp.In.FourCC != videoParam.vpp.Out.FourCC || videoParam.vpp.In.FourCC != MFX_FOURCC_NV12)
                            {
                                // Only NV12 as in/out supported by mirroring now
                                return MFX_ERR_INVALID_VIDEO_PARAM;
                            }

                            if (videoParam.vpp.In.CropW != videoParam.vpp.Out.CropW || videoParam.vpp.In.CropH != videoParam.vpp.Out.CropH)
                            {
                                // Scaling is not supported for mirroring now.
                                return MFX_ERR_INVALID_VIDEO_PARAM;
                            }

                            executeParams.mirroring = extMirroring->Type;
                        }
                    }
                }
                else
                {
                    bIsFilterSkipped = true;
                }

                break;
            }
#endif
            case MFX_EXTBUFF_VPP_DETAIL:
            {
                if(caps.uDetailFilter)
                {
                    executeParams.bDetailAutoAdjust = TRUE;
                    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
                    {
                        executeParams.detailFactor = 32;// default
                        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DETAIL)
                        {
                            mfxExtVPPDetail *extDetail = (mfxExtVPPDetail*) videoParam.ExtParam[i];
                            executeParams.detailFactor = MapDNFactor(extDetail->DetailFactor);
                            executeParams.detailFactorOriginal = extDetail->DetailFactor;
                        }
                        executeParams.bDetailAutoAdjust = FALSE;
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
            case MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10:
            {
                break;// aya: make sense for SW_VPP only
            }

            case MFX_EXTBUFF_VPP_RSHIFT_IN:
            case MFX_EXTBUFF_VPP_LSHIFT_OUT:
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
                executeParams.frcModeOrig = static_cast<mfxU16>(GetMFXFrcMode(videoParam));
#if 0
                // Disable interpolated FRC until related issues resolved

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
                            /* case 30->60, or (1->2) */
                            if (20 == floor(10*mfxRatio))
                            {
                                config.m_surfCount[VPP_IN]  = 3;
                                config.m_extConfig.customRateData.fwdRefCount = 2;
                                config.m_extConfig.customRateData.inputFramesOrFieldPerCycle= 1;
                                config.m_extConfig.customRateData.outputIndexCountPerCycle  = 2;
                            }
                            else /* case 24->60 or (2->5)*/
                            {
                                config.m_surfCount[VPP_IN]  = 4;
                                config.m_extConfig.customRateData.fwdRefCount = 3;
                                config.m_extConfig.customRateData.inputFramesOrFieldPerCycle= 2;
                                config.m_extConfig.customRateData.outputIndexCountPerCycle  = 5;
                            }

                            executeParams.bFRCEnable     = true;
                            executeParams.customRateData = caps.frcCaps.customRateData[frcIdx];
                        }
                    }
                }
#endif


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
                //DISABLE IMAGE_STABILIZATION
                /*if(caps.uIStabFilter)
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
                                //if( extIStab->Mode != MFX_IMAGESTAB_MODE_UPSCALE && extIStab->Mode != MFX_IMAGESTAB_MODE_BOXING )
                                //{
                                //   return MFX_ERR_INVALID_VIDEO_PARAM;
                                //}
                        }
                    }
                }
                else
                {
                    executeParams.bImgStabilizationEnable = false;
                    //bIsPartialAccel = true;
                }*/
                executeParams.bImgStabilizationEnable = false;
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

                        if (!executeParams.dstRects.empty())
                        {
                            if (executeParams.dstRects.size() < StreamCount)
                                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
                        }

                        if (executeParams.dstRects.size() != StreamCount)
                        {
                            executeParams.dstRects.clear();
                            executeParams.dstRects.resize(StreamCount);
                        }
                        for (mfxU32 cnt = 0; cnt < StreamCount; ++cnt)
                        {
                            DstRect rec = {0};
                            rec.DstX = extComp->InputStream[cnt].DstX;
                            rec.DstY = extComp->InputStream[cnt].DstY;
                            rec.DstW = extComp->InputStream[cnt].DstW;
                            rec.DstH = extComp->InputStream[cnt].DstH;
                            if ((videoParam.vpp.Out.Width < (rec.DstX + rec.DstW)) || (videoParam.vpp.Out.Height < (rec.DstY + rec.DstH)))
                                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; // sub-stream is out of range
                            if (extComp->InputStream[cnt].GlobalAlphaEnable != 0)
                            {
                                rec.GlobalAlphaEnable = extComp->InputStream[cnt].GlobalAlphaEnable;
                                rec.GlobalAlpha = extComp->InputStream[cnt].GlobalAlpha;
                            }
                            if (extComp->InputStream[cnt].LumaKeyEnable !=0)
                            {
                                rec.LumaKeyEnable = extComp->InputStream[cnt].LumaKeyEnable;
                                rec.LumaKeyMin = extComp->InputStream[cnt].LumaKeyMin;
                                rec.LumaKeyMax = extComp->InputStream[cnt].LumaKeyMax;
                            }
                            if (extComp->InputStream[cnt].PixelAlphaEnable != 0)
                                rec.PixelAlphaEnable = 1;
                            executeParams.dstRects[cnt]= rec ;
                        }
                        /* And now lets calculate background color*/

                        mfxU32 targetFourCC = videoParam.vpp.Out.FourCC;

                        if (targetFourCC == MFX_FOURCC_NV12 ||
                            targetFourCC == MFX_FOURCC_YV12 ||
                            targetFourCC == MFX_FOURCC_NV16 ||
                            targetFourCC == MFX_FOURCC_YUY2)
                        {
                            executeParams.iBackgroundColor  = (0xff << 24)|
                               (VPP_RANGE_CLIP(extComp->Y, 16, 235) << 16)|
                               (VPP_RANGE_CLIP(extComp->U, 16, 240) <<  8)|
                               (VPP_RANGE_CLIP(extComp->V, 16, 240) <<  0);
                        }
                        if (targetFourCC == MFX_FOURCC_RGB4 ||
                            targetFourCC == MFX_FOURCC_BGR4)
                        {
                            executeParams.iBackgroundColor = (0xff << 24)|
                               (VPP_RANGE_CLIP(extComp->R, 0, 255) << 16)|
                               (VPP_RANGE_CLIP(extComp->G, 0, 255) <<  8)|
                               (VPP_RANGE_CLIP(extComp->B, 0, 255) <<  0);
                        }
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
            case MFX_EXTBUFF_VPP_FIELD_PROCESSING:
            {
                /* As this filter for now can be configured via DOUSE way */
                {
                    executeParams.iFieldProcessingMode = 0xffffffff;
                }
                for (mfxU32 jj = 0; jj < videoParam.NumExtParam; jj++)
                {
                    if (videoParam.ExtParam[jj]->BufferId == MFX_EXTBUFF_VPP_FIELD_PROCESSING)
                    {
                        mfxExtVPPFieldProcessing* extFP = (mfxExtVPPFieldProcessing*) videoParam.ExtParam[jj];
                        // aya: quick fix for CM
                        /*const int TFF2TFF = 0x0;
                        const int TFF2BFF = 0x1;
                        const int BFF2TFF = 0x2;
                        const int BFF2BFF = 0x3;*/

                        if (extFP->Mode == MFX_VPP_COPY_FRAME)
                        {
                            executeParams.iFieldProcessingMode = FRAME2FRAME;
                        }

                        if (extFP->Mode == MFX_VPP_COPY_FIELD)
                        {
                            if(extFP->InField ==  MFX_PICSTRUCT_FIELD_TFF)
                            {
                                executeParams.iFieldProcessingMode = (extFP->OutField == MFX_PICSTRUCT_FIELD_TFF) ? TFF2TFF : TFF2BFF;
                            }
                            else
                            {
                                executeParams.iFieldProcessingMode = (extFP->OutField == MFX_PICSTRUCT_FIELD_TFF) ? BFF2TFF : BFF2BFF;
                            }
                        }

                        // aya, al: !!! Sergey Osipov's kernel uses "0"  as a "valid" data, but HW_VPP doesn't
                        // to prevent issue we increment here and decrement before kernel call
                        executeParams.iFieldProcessingMode++;
                    }
                }
                break;
            } //case MFX_EXTBUFF_VPP_FIELD_PROCESSING:

            case MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO:
                for (mfxU32 jj = 0; jj < videoParam.NumExtParam; jj++)
                {
                    if (videoParam.ExtParam[jj]->BufferId == MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO)
                    {
                        mfxExtVPPVideoSignalInfo* extVSI = (mfxExtVPPVideoSignalInfo*) videoParam.ExtParam[jj];
                        MFX_CHECK_NULL_PTR1(extVSI);

                        /* Check params */
                        if (extVSI->In.TransferMatrix != MFX_TRANSFERMATRIX_BT601 &&
                            extVSI->In.TransferMatrix != MFX_TRANSFERMATRIX_BT709 &&
                            extVSI->In.TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }

                        if (extVSI->Out.TransferMatrix != MFX_TRANSFERMATRIX_BT601 &&
                            extVSI->Out.TransferMatrix != MFX_TRANSFERMATRIX_BT709 &&
                            extVSI->Out.TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }

                        if (extVSI->In.NominalRange != MFX_NOMINALRANGE_0_255 &&
                            extVSI->In.NominalRange != MFX_NOMINALRANGE_16_235 &&
                            extVSI->In.NominalRange != MFX_NOMINALRANGE_UNKNOWN)
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }

                        if (extVSI->Out.NominalRange != MFX_NOMINALRANGE_0_255 &&
                            extVSI->Out.NominalRange != MFX_NOMINALRANGE_16_235 &&
                            extVSI->Out.NominalRange != MFX_NOMINALRANGE_UNKNOWN)
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }

                        /* Params look good */
                        executeParams.VideoSignalInfoIn.NominalRange    = extVSI->In.NominalRange;
                        executeParams.VideoSignalInfoIn.TransferMatrix  = extVSI->In.TransferMatrix;
                        executeParams.VideoSignalInfoIn.enabled         = true;

                        executeParams.VideoSignalInfoOut.NominalRange   = extVSI->Out.NominalRange;
                        executeParams.VideoSignalInfoOut.TransferMatrix = extVSI->Out.TransferMatrix;
                        executeParams.VideoSignalInfoOut.enabled        = true;
                        break;
                    }
                }

                break;

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

    if (inDNRatio == outDNRatio && !executeParams.bVarianceEnable && !executeParams.bComposite &&
            !(config.m_extConfig.mode == IS_REFERENCES) )
    {
        // work around
        config.m_extConfig.mode  = FRC_DISABLED;
        config.m_bPassThroughEnable = false;
    }

    if (true == executeParams.bComposite && 0 == executeParams.dstRects.size()) // composition was enabled via DO USE
        return MFX_ERR_INVALID_VIDEO_PARAM;

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

    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
    {
        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DOUSE)
        {
            mfxExtVPPDoUse *extDoUse = (mfxExtVPPDoUse *) videoParam.ExtParam[i];

            for (mfxU32 j = 0; j < extDoUse->NumAlg; j++)
            {
                if (extDoUse->AlgList[j] == MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION)
                {
                    return MFX_FRCALGM_PRESERVE_TIMESTAMP;
                }
            }
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
} // mfxStatus SetMFXISMode(const mfxVideoParam & videoParam, mfxU32 mode)


mfxU32 GetDeinterlacingMode(const mfxVideoParam & videoParam)
{
    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
    {
        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DEINTERLACING)
        {
            mfxExtVPPDeinterlacing *extDeinterlacing = (mfxExtVPPDeinterlacing *) videoParam.ExtParam[i];
            return extDeinterlacing->Mode;
        }
    }

    return 0;//EMPTY

} // mfxStatus GetDeinterlacingMode(const mfxVideoParam & videoParam, mfxU32 mode)


mfxStatus SetDeinterlacingMode(const mfxVideoParam & videoParam, mfxU32 mode)
{
    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
    {
        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DEINTERLACING)
        {
            mfxExtVPPDeinterlacing *extDeinterlacing = (mfxExtVPPDeinterlacing *) videoParam.ExtParam[i];
            extDeinterlacing->Mode = (mfxU16)mode;

            return MFX_ERR_NONE;
        }
    }

    return MFX_WRN_VALUE_NOT_CHANGED;

} // mfxStatus SetDeinterlacingMode(const mfxVideoParam & videoParam, mfxU32 mode)


mfxStatus SetSignalInfo(const mfxVideoParam & videoParam, mfxU32 trMatrix, mfxU32 Range)
{
    for (mfxU32 i = 0; i < videoParam.NumExtParam; i++)
    {
        if (videoParam.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO)
        {
            mfxExtVPPVideoSignalInfo *extDeinterlacing = (mfxExtVPPVideoSignalInfo *) videoParam.ExtParam[i];

            extDeinterlacing->In.TransferMatrix  = (mfxU16)trMatrix;
            extDeinterlacing->In.NominalRange    = (mfxU16)Range;
            extDeinterlacing->Out.TransferMatrix = (mfxU16)trMatrix;
            extDeinterlacing->Out.NominalRange   = (mfxU16)Range;

            return MFX_ERR_NONE;
        }
    }

    return MFX_WRN_VALUE_NOT_CHANGED;

} // mfxStatus SetMFXFrcMode(const mfxVideoParam & videoParam, mfxU32 mode)


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
