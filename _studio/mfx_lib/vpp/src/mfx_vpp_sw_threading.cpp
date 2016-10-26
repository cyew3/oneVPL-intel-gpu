//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2016 Intel Corporation. All Rights Reserved.
//

#include "math.h"
#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#include "mfx_enc_common.h"
#include "mfx_session.h"

//#include "mfx_vpp_main.h"
#include "mfx_vpp_utils.h"
#include "mfx_vpp_service.h"
#include "mfx_vpp_sw.h"

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
mfxStatus VideoVPP_SW::PreWork(mfxFrameSurface1* in, mfxFrameSurface1* out,
                                mfxFrameSurface1** pInSurf, mfxFrameSurface1** pOutSurf)
{
    mfxStatus sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    // VPP ignores input surface if there is ready output from any filter
    m_threadModeVPP.bUseInput = true;
    *pInSurf = in;
    *pOutSurf= out;

    /* *********************************************************** */
    /*                      Prework                                   */
    /* *********************************************************** */

    if (!m_bDoFastCopyFlag[VPP_IN])
    {
        sts = m_externalFramesPool[VPP_IN].PreProcessSync();
        MFX_CHECK_STS(sts );
    } else
    {
        sts = m_internalSystemFramesPool[VPP_IN].Lock();
        MFX_CHECK_STS(sts);
    }

    if (!m_bDoFastCopyFlag[VPP_OUT])
    {
        sts = m_externalFramesPool[VPP_OUT].PreProcessSync();
        MFX_CHECK_STS( sts );
    }


    if(IsReadyOutput(MFX_REQUEST_FROM_VPP_PROCESS) || (NULL == in))
    {
        m_threadModeVPP.bUseInput = false;
    }

    if(m_threadModeVPP.bUseInput)
    {
        sts = PreProcessOfInputSurface(in, pInSurf);
        MFX_CHECK_STS(sts);
    }

    sts = PreProcessOfOutputSurface(out, pOutSurf);
    MFX_CHECK_STS(sts);


    /* VPP ignores crop info at init stage. so, current frame gives this information.
    according to the spec, crop info can be changed by every frame */
    sts = SetCrop(*pInSurf, *pOutSurf);
    MFX_CHECK_STS(sts);

//    sts = SetBackGroundColor(*pOutSurf);
//    MFX_CHECK_STS(sts);

    // lock internal surfaces
    mfxU32 connectIndex;
    for(connectIndex = 0; connectIndex < GetNumConnections(); connectIndex++)
    {
        sts = m_connectionFramesPool[connectIndex].Lock();
        MFX_CHECK_STS(sts);
    }

    m_threadModeVPP.fIndx = GetFilterIndexStart(MFX_REQUEST_FROM_VPP_PROCESS);
    m_threadModeVPP.pipelineInSurf = (m_threadModeVPP.fIndx > 0) ? NULL : *pInSurf;

    if(m_threadModeVPP.fIndx >= GetNumConnections())    // the last filter in chain
    {
        m_threadModeVPP.pipelineOutSurf = *pOutSurf;
    } else
    {   // intermediate output surface
        sts = m_connectionFramesPool[m_threadModeVPP.fIndx].GetFreeSurface( &(m_threadModeVPP.pipelineOutSurf) );
        MFX_CHECK_STS(sts);
    }

    m_threadModeVPP.pCurFilter = m_ppFilters[ m_threadModeVPP.fIndx ];

    return sts;

} // mfxStatus VideoVPPBase::PreWork(...)


mfxStatus VideoVPP_SW::PostWork(mfxFrameSurface1* in, mfxFrameSurface1* out,
                                 mfxFrameSurface1* pInSurf, mfxFrameSurface1* pOutSurf,
                                 mfxStatus processingSts)
{
    mfxStatus sts = MFX_ERR_UNDEFINED_BEHAVIOR;
    mfxU32 connectIndex = 0;

    /* *********************************************************** */
    /*                      Postwork                                   */
    /* *********************************************************** */

    if(MFX_ERR_NONE == processingSts)
    {
        sts = SetBackGroundColor(pOutSurf);
        MFX_CHECK_STS(sts);
    }

    if (!m_bDoFastCopyFlag[VPP_IN])
    {
        sts = m_externalFramesPool[VPP_IN].PostProcessSync();
        MFX_CHECK_STS(sts);
    } else
    {
        sts = m_internalSystemFramesPool[VPP_IN].Lock();
        MFX_CHECK_STS(sts);
    }

    if (!m_bDoFastCopyFlag[VPP_OUT])
    {
        sts = m_externalFramesPool[VPP_OUT].PostProcessSync();
        MFX_CHECK_STS(sts);
    }

    if(m_threadModeVPP.bUseInput)
    {
        if (!m_bDoFastCopyFlag[VPP_IN])
        {
            /* for each non zero input frame */
            sts = m_core->DecreaseReference(&in->Data);
            MFX_CHECK_STS(sts);
        }

        /* for each non zero input frame */
        sts = m_core->DecreaseReference(&pInSurf->Data);
        MFX_CHECK_STS(sts);
    }

    sts = PostProcessOfOutputSurface(out, pOutSurf, processingSts);
    MFX_CHECK_STS(sts);

    sts = m_core->DecreaseReference(&pOutSurf->Data);
    MFX_CHECK_STS(sts);

    // unlock internal surfaces
    for(connectIndex = 0; connectIndex < GetNumConnections(); connectIndex++)
    {
        sts = m_connectionFramesPool[connectIndex].Unlock();
        MFX_CHECK_STS(sts);
    }

    /* *********************************************************** */
    /*                      STOP                                   */
    /* *********************************************************** */

    /* once per RunFrameVPP */
    if(out && MFX_ERR_NONE == processingSts )
    {
        sts = m_core->DecreaseReference(&(out->Data));
        MFX_CHECK_STS( sts );
    }

    if( !m_errPrtctState.isFirstFrameProcessed)
    {
        m_errPrtctState.isFirstFrameProcessed = true;
    }

    return sts;

} // mfxStatus VideoVPPBase::PostWork(...)

mfxStatus VideoVPP_SW::RunVPPTask(mfxFrameSurface1* in, mfxFrameSurface1* out, FilterVPP::InternalParam* pParam )
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxI32 preWorkIsStarted_cur = vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>( &(m_threadModeVPP.preWorkIsStarted) ), 1, 0);

    if (!preWorkIsStarted_cur)
    {   // PreWork works only once
        sts = PreWork(in, out, &(m_threadModeVPP.pInSurf), &(m_threadModeVPP.pOutSurf));
        MFX_CHECK_STS(sts);
        m_threadModeVPP.preWorkIsDone = 1;
        m_core->INeedMoreThreadsInside(this);

        m_internalParam.inPicStruct = pParam->inPicStruct;
        m_internalParam.inTimeStamp = pParam->inTimeStamp;
        m_internalParam.aux         = pParam->aux;
    }
    else
    {
        if ( !m_threadModeVPP.preWorkIsDone )
        {
            return MFX_TASK_BUSY;
        }
    }
    if (m_threadModeVPP.fIndx >= GetNumUsedFilters())
    {
        return MFX_TASK_BUSY;
    }

    mfxStatus processingSts = (m_threadModeVPP.pCurFilter)->RunFrameVPPTask(m_threadModeVPP.pipelineInSurf,
                                                                            m_threadModeVPP.pipelineOutSurf,
                                                                            &m_internalParam);

    if (MFX_ERR_UNDEFINED_BEHAVIOR == processingSts)
    {
        return processingSts;
    }
    if ((processingSts == MFX_TASK_WORKING) ||
        (processingSts == MFX_TASK_BUSY))
    {   // not all regions are performed for current filter (MFX_TASK_WORKING) or
        // no extra thread is needed (MFX_TASK_BUSY)
        return processingSts;
    }

    if (!((MFX_ERR_MORE_DATA == processingSts) &&
          (MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION == m_pipelineList[ m_threadModeVPP.fIndx ])))
    {   // break pipeline in case of FRC and ERR_MORE_DATA
        m_threadModeVPP.fIndx++;  // goto the next filter
        if (m_threadModeVPP.fIndx < GetNumUsedFilters())
        {
            if (MFX_ERR_MORE_DATA == processingSts)
            {
                m_threadModeVPP.pipelineInSurf = NULL;
            }
            else
            {
                m_threadModeVPP.pipelineInSurf = m_threadModeVPP.pipelineOutSurf;
            }

            if(m_threadModeVPP.fIndx >= GetNumConnections())
            {
                m_threadModeVPP.pipelineOutSurf = m_threadModeVPP.pOutSurf;
            }
            else
            {
                sts = m_connectionFramesPool[m_threadModeVPP.fIndx].GetFreeSurface( &(m_threadModeVPP.pipelineOutSurf) );
                MFX_CHECK_STS(sts);
            }

            m_threadModeVPP.pCurFilter  = m_ppFilters[m_threadModeVPP.fIndx];
            m_internalParam.inPicStruct = m_internalParam.outPicStruct;
            m_internalParam.inTimeStamp = m_internalParam.outTimeStamp;

            return MFX_TASK_WORKING;
        }
    }

    mfxI32 postWorkIsStarted_cur = vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>( &(m_threadModeVPP.postWorkIsStarted) ), 1, 0);
    if (!postWorkIsStarted_cur)
    {   // PostWork works only once
        sts = PostWork(in, out, m_threadModeVPP.pInSurf, m_threadModeVPP.pOutSurf, processingSts);
        MFX_CHECK_STS(sts);
        m_threadModeVPP.postWorkIsDone = 1;
    }
    else
    {
        if ( !m_threadModeVPP.postWorkIsDone )
        {
            return MFX_TASK_BUSY;
        }
    }

   return MFX_TASK_DONE;

} // mfxStatus VideoVPPBase::RunVPPTask(...)


mfxStatus RunFrameVPPRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    mfxStatus tskRes;

    VideoVPP_SW *pVPP = (VideoVPP_SW *)pState;
    VideoVPP_SW::AsyncParams *pAsyncParams = (VideoVPP_SW::AsyncParams *)pParam;

    FilterVPP::InternalParam internalParam;
    internalParam.aux = pAsyncParams->aux;
    internalParam.inPicStruct = pAsyncParams->inputPicStruct;
    internalParam.inTimeStamp = pAsyncParams->inputTimeStamp;

    threadNumber = threadNumber;
    callNumber = callNumber;

    tskRes = pVPP->RunVPPTask(pAsyncParams->surf_in, pAsyncParams->surf_out, &internalParam);

    return tskRes;

} // mfxStatus RunFrameVPPRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)


mfxStatus VideoVPP_SW::ResetTaskCounters()
{
    m_threadModeVPP.preWorkIsStarted = 0;
    m_threadModeVPP.preWorkIsDone = 0;
    m_threadModeVPP.postWorkIsStarted = 0;
    m_threadModeVPP.postWorkIsDone = 0;

    for(mfxU32 fInd = 0; fInd < GetNumUsedFilters(); fInd++)
    {
        m_ppFilters[fInd]->ResetFilterTaskCounters();
    }

    return MFX_TASK_DONE;

} // mfxStatus VideoVPPBase::ResetTaskCounters()


mfxStatus CompleteFrameVPPRoutine(void *pState, void *pParam, mfxStatus taskRes)
{
    VideoVPP_SW *pVPP = (VideoVPP_SW *)pState;
    taskRes = taskRes;

    if (pParam)
    {
        delete (VideoVPP_SW::AsyncParams *)pParam;   // NOT SAFE !!!
    }

    return pVPP->ResetTaskCounters();

} // mfxStatus CompleteFrameVPPRoutine(void *pState, void *pParam, mfxStatus taskRes)
#endif

#endif // MFX_ENABLE_VPP
/* EOF */