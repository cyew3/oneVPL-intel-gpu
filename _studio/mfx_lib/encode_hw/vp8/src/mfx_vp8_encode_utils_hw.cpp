/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_vp8_encode_utils_hybrid_hw.h"

#if defined (MFX_ENABLE_VP8_VIDEO_ENCODE_HW)
#include "assert.h"

namespace MFX_VP8ENC
{ 
    #ifdef HYBRID_ENCODE

    //---------------------------------------------------------
    // service class: TaskManagerHybridEncode
    //---------------------------------------------------------

    mfxStatus TaskManagerHybridEncode::Init(VideoCORE* pCore, mfxVideoParam *par, mfxFrameInfo* pDDIMBInfo, mfxU32 numDDIMBFrames)
    {
        mfxStatus     sts      = MFX_ERR_NONE;
        mfxU32        numTasks = CalcNumTasks(m_video);


        MFX_CHECK(!m_pCore, MFX_ERR_UNDEFINED_BEHAVIOR);

        m_pCore   = pCore;
        m_video   = *par;

        m_frameNum = 0;

        m_bHWInput = isVideoSurfInput(m_video); 

        m_pPrevSubmittedTaskEnc = 0;
        m_pPrevSubmittedTaskPak = 0;

        m_RawFrames.Init(); 

        sts = m_LocalRawFrames.Init(m_pCore, &m_video.mfx.FrameInfo, 
            CalcNumSurfRawLocal(m_video, !m_bHWInput, m_bHWInput), 
            !m_bHWInput);
        MFX_CHECK_STS(sts);

        sts = m_ReconFramesEnc.Init(m_pCore, &m_video.mfx.FrameInfo, CalcNumSurfRecon(m_video), true);
        MFX_CHECK_STS(sts);

        sts = m_ReconFramesPak.Init(m_pCore, &m_video.mfx.FrameInfo, CalcNumSurfRecon(m_video), false);
        MFX_CHECK_STS(sts);

        sts = m_MBDataDDI.Init(m_pCore, pDDIMBInfo, (numDDIMBFrames < numTasks ? numTasks: numDDIMBFrames) , true);
        MFX_CHECK_STS(sts);

        {

            m_TasksEnc.resize(numTasks);
            m_TasksPak.resize(numTasks);

            for (mfxU32 i = 0; i < numTasks; i++)
            {
                sts = m_TasksEnc[i].Init(m_pCore,&m_video);
                MFX_CHECK_STS(sts);

                sts = m_TasksPak[i].Init(m_pCore,&m_video);
                MFX_CHECK_STS(sts);

            }
        }
        return sts;          
    }
    mfxStatus TaskManagerHybridEncode::Reset(mfxVideoParam *par)
    {
        mfxStatus sts = MFX_ERR_NONE;
        MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);  

        m_video     = *par;
        m_frameNum  = 0;

        MFX_CHECK(m_ReconFramesEnc.Height() >= m_video.mfx.FrameInfo.Height,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(m_ReconFramesEnc.Width()  >= m_video.mfx.FrameInfo.Width,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        MFX_CHECK(mfxU16(CalcNumSurfRawLocal(m_video,!isVideoSurfInput(m_video),isVideoSurfInput(m_video))) <= m_LocalRawFrames.Num(),MFX_ERR_INCOMPATIBLE_VIDEO_PARAM); 
        MFX_CHECK(mfxU16(CalcNumSurfRecon(m_video)) <= m_ReconFramesEnc.Num(),MFX_ERR_INCOMPATIBLE_VIDEO_PARAM); 

        m_pPrevSubmittedTaskEnc = 0;
        m_pPrevSubmittedTaskPak = 0;

        sts = FreeTasks(m_TasksEnc);
        MFX_CHECK_STS(sts);

        sts = FreeTasks(m_TasksPak);
        MFX_CHECK_STS(sts);

        return MFX_ERR_NONE;

    }
    mfxStatus TaskManagerHybridEncode::InitTaskEnc(mfxFrameSurface1* pSurface, mfxBitstream* pBitstream, TaskHybridDDI* & pOutTask)
    {
        mfxStatus sts = MFX_ERR_NONE;
        TaskHybridDDI* pTask = GetFreeTask(m_TasksEnc);
        sFrameEx* pRawFrame = 0;

        MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);
        MFX_CHECK(pTask!=0,MFX_WRN_DEVICE_BUSY);

        sts = m_RawFrames.GetFrame(pSurface, pRawFrame);
        MFX_CHECK_STS(sts);              

        sts = pTask->InitTask(pRawFrame,pBitstream,m_frameNum);
        MFX_CHECK_STS(sts);

        pOutTask = pTask;
        m_frameNum ++;
        return sts;
    }
    mfxStatus TaskManagerHybridEncode::SubmitTaskEnc(TaskHybridDDI*  pTask, sFrameParams *pParams)
    {
        sFrameEx* pRecFrame = 0;
        sFrameEx* pDDIMBFrame = 0;
        sFrameEx* pRawLocalFrame = 0;
        mfxStatus sts = MFX_ERR_NONE;

        MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);

        pRecFrame = m_ReconFramesEnc.GetFreeFrame();
        MFX_CHECK(pRecFrame!=0,MFX_WRN_DEVICE_BUSY);

        pDDIMBFrame = m_MBDataDDI.GetFreeFrame();
        MFX_CHECK(pDDIMBFrame!=0,MFX_WRN_DEVICE_BUSY);

        if (!m_bHWInput)
        {
            pRawLocalFrame = m_LocalRawFrames.GetFreeFrame();
            MFX_CHECK(pRawLocalFrame!= 0,MFX_WRN_DEVICE_BUSY);
        }            

        sts = pTask->SubmitTask(pRecFrame,m_pPrevSubmittedTaskEnc,pParams, pRawLocalFrame,pDDIMBFrame);
        MFX_CHECK_STS(sts);

        return sts;          
    }

    mfxStatus TaskManagerHybridEncode::InitAndSubmitTaskPak(TaskHybridDDI* pTaskEnc, TaskHybrid * &pOutTaskPak)
    {
        mfxStatus sts = MFX_ERR_NONE;
        sFrameEx* pRecFrame = 0;
        sFrameEx* pRawLocalFrame = 0;

        TaskHybrid* pTask = GetFreeTask(m_TasksPak);

        MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);
        MFX_CHECK(pTask!=0,MFX_WRN_DEVICE_BUSY);
        MFX_CHECK(pTaskEnc->isReady(),MFX_WRN_DEVICE_BUSY);

        sts = pTask->InitTask(pTaskEnc->m_pRawFrame,pTaskEnc->m_pBitsteam, pTaskEnc->m_frameOrder);
        MFX_CHECK_STS(sts);

        pRecFrame = m_ReconFramesPak.GetFreeFrame();
        MFX_CHECK(pRecFrame!=0,MFX_WRN_DEVICE_BUSY);

        if (m_bHWInput)
        {
            pRawLocalFrame = m_LocalRawFrames.GetFreeFrame();
            MFX_CHECK(pRawLocalFrame!= 0,MFX_WRN_DEVICE_BUSY);
        }            

        sts = pTask->SubmitTask(pRecFrame,m_pPrevSubmittedTaskPak,&pTaskEnc->m_sFrameParams, pRawLocalFrame);
        MFX_CHECK_STS(sts);

        pTask->m_MBData = pTaskEnc->m_MBData;

        pOutTaskPak = pTask;
        return sts;
    }
    mfxStatus TaskManagerHybridEncode::CompleteTasks(TaskHybridDDI* pTaskEnc, TaskHybrid * pTaskPak)
    {
        mfxStatus sts = MFX_ERR_NONE;

        /*Copy reference frames*/
        {
            FrameLocker lockSrc(m_pCore, pTaskEnc->m_pRecFrame->pSurface->Data, false);
            FrameLocker lockDst(m_pCore, pTaskPak->m_pRecFrame->pSurface->Data, false); 

            sts = m_pCore->DoFastCopy(pTaskPak->m_pRecFrame->pSurface, pTaskEnc->m_pRecFrame->pSurface);
            MFX_CHECK_STS(sts);
        }  
        sts = m_pPrevSubmittedTaskEnc->FreeTask();
        MFX_CHECK_STS(sts);

        m_pPrevSubmittedTaskEnc = pTaskEnc;

        sts = m_pPrevSubmittedTaskPak->FreeTask();
        MFX_CHECK_STS(sts);

        m_pPrevSubmittedTaskPak = pTaskPak;

        return sts;
    }
#endif


}
#endif