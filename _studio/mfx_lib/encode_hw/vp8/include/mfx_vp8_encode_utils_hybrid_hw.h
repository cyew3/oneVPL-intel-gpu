/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE_HW) && defined(MFX_VA)

#ifndef _MFX_VP8_ENCODE_UTILS_HYBRID_HW_H_
#define _MFX_VP8_ENCODE_UTILS_HYBRID_HW_H_

#include "mfx_vp8_encode_utils_hw.h"
#include "mfx_vp8_encode_ddi_hw.h"
#include <queue>
#include "ipps.h"
#include "umc_defs.h"

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }


namespace MFX_VP8ENC
{
    struct sMBData : sDDI_MBDATA
    {
        ENCODE_MV_DATA MV[16];
    };

    class TaskHybrid : public Task
    {
    public:
        TaskHybrid():  Task() {}
        virtual ~TaskHybrid() {}
        virtual  mfxStatus Init(VideoCORE * pCore, mfxVideoParam *par)
        { 
            return Task::Init(pCore,par);
        }
        virtual mfxStatus FreeTask()
        {
            //printf("TaskHybrid::FreeTask()\n");
            return Task::FreeTask();
        }
    };
    struct sDDIFrames
    {
        sFrameEx *m_pMB_hw;
        sFrameEx *m_pMB_sw;

        sFrameEx *m_pDist_hw;
        sFrameEx *m_pDist_sw;

        sFrameEx *m_pSegMap_hw;

    };

    typedef struct 
    {
        mfxU16  RefFrameCost[4];
        mfxU16  IntraModeCost[4];
        mfxU16  InterModeCost[4];
        mfxU16  IntraNonDCPenalty16x16;
        mfxU16  IntraNonDCPenalty4x4;
    } VP8HybridCosts;

    class TaskHybridDDI: public TaskHybrid
    {
    public:
        sDDIFrames ddi_frames;
        VP8HybridCosts m_costs;
        mfxU64         m_prevFrameSize;
        mfxU8          m_brcUpdateDelay;

    public:
        TaskHybridDDI(): 
          TaskHybrid() 
          {
              Zero (ddi_frames);
              m_prevFrameSize = 0;
              m_brcUpdateDelay = 0;
          }
        virtual ~TaskHybridDDI() {}
        virtual   mfxStatus SubmitTask (sFrameEx*  pRecFrame, sDpbVP8 &dpb, sFrameParams* pParams,  sFrameEx* pRawLocalFrame, 
                                        sDDIFrames *pDDIFrames)
        {
            /*printf("TaskHybridDDI::SubmitTask\n");*/
            mfxStatus sts = Task::SubmitTask(pRecFrame,dpb,pParams,pRawLocalFrame);

            MFX_CHECK(isFreeSurface(pDDIFrames->m_pMB_hw),MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(isFreeSurface(pDDIFrames->m_pMB_sw),MFX_ERR_UNDEFINED_BEHAVIOR);            

            //MFX_CHECK(isFreeSurface(pDDIFrames->m_pDist_hw),MFX_ERR_UNDEFINED_BEHAVIOR);
            //MFX_CHECK(isFreeSurface(pDDIFrames->m_pDist_sw),MFX_ERR_UNDEFINED_BEHAVIOR);

            ddi_frames = *pDDIFrames;

            MFX_CHECK_STS(LockSurface(ddi_frames.m_pMB_hw,m_pCore));
            MFX_CHECK_STS(LockSurface(ddi_frames.m_pMB_sw,m_pCore));

            //MFX_CHECK_STS(LockSurface(ddi_frames.m_pDist_hw,m_pCore));
            //MFX_CHECK_STS(LockSurface(ddi_frames.m_pDist_sw,m_pCore));

            return sts;
        }
        virtual  mfxStatus FreeTask()
        {
            /*printf("TaskHybridDDI::FreeTask\n");*/
            MFX_CHECK_STS(FreeSurface(ddi_frames.m_pMB_hw,m_pCore));
            MFX_CHECK_STS(FreeSurface(ddi_frames.m_pMB_sw,m_pCore));

            //MFX_CHECK_STS(FreeSurface(ddi_frames.m_pDist_hw,m_pCore));
            //MFX_CHECK_STS(FreeSurface(ddi_frames.m_pDist_sw,m_pCore));

            return TaskHybrid::FreeTask();
        }
    };

    struct FrameInfoFromPak
    {
        mfxU64         m_frameOrder;
        mfxU8          m_frameType;
        mfxU64         m_encodedFrameSize;
        VP8HybridCosts m_updatedCosts;
    };

    class TaskManagerHybridPakDDI : public TaskManager<TaskHybridDDI>
    {
    public:
        typedef TaskManager<TaskHybridDDI>  BaseClass;

        TaskManagerHybridPakDDI(): BaseClass()    {m_bUseSegMap = false;}
        virtual ~TaskManagerHybridPakDDI() {/*printf("~TaskManagerHybridPakDDI)\n");*/}
    
    // [SE] WA for Windows hybrid VP8 HSW driver (remove 'protected' here)
#if defined (MFX_VA_LINUX)
    protected:
#endif

        InternalFrames  m_MBDataDDI_hw;
        InternalFrames  m_DistDataDDI_hw;

        InternalFrames  m_MBDataDDI_sw;
        InternalFrames  m_DistDataDDI_sw;

        bool            m_bUseSegMap;
        InternalFrames  m_SegMapDDI_hw;

        mfxU16           m_maxBrcUpdateDelay;

        std::queue<FrameInfoFromPak> m_cachedFrameInfoFromPak;
        FrameInfoFromPak             m_latestKeyFrame;
        FrameInfoFromPak             m_latestNonKeyFrame;

    public:

        inline
        mfxStatus Init( VideoCORE* pCore, mfxVideoParam *par, mfxU32 reconFourCC)
        {
            MFX_CHECK_STS(BaseClass::Init(pCore,par,true,reconFourCC));
            m_maxBrcUpdateDelay = par->AsyncDepth > 2 ? 2: par->AsyncDepth; // driver supports maximum 2-frames BRC update delay
            Zero(m_latestNonKeyFrame);
            Zero(m_latestKeyFrame);
            return MFX_ERR_NONE;
        }

        inline
        mfxStatus AllocInternalResources( VideoCORE* pCore, 
                        mfxFrameAllocRequest reqMBData,
                        mfxFrameAllocRequest reqDist,
                        mfxFrameAllocRequest reqSegMap)
        {
            if (reqMBData.NumFrameMin < m_ReconFrames.Num())
                reqMBData.NumFrameMin = (mfxU16)m_ReconFrames.Num();
            MFX_CHECK_STS(m_MBDataDDI_hw.Init(pCore, &reqMBData,true));
            reqMBData.Info.FourCC = MFX_FOURCC_P8;
            MFX_CHECK_STS(m_MBDataDDI_sw.Init(pCore, &reqMBData,false));

            if (reqDist.NumFrameMin)
            {
                MFX_CHECK_STS(m_DistDataDDI_hw.Init(pCore, &reqDist,true));
                reqDist.Info.FourCC = MFX_FOURCC_P8;
                MFX_CHECK_STS(m_DistDataDDI_sw.Init(pCore, &reqDist,false));
            }

            if (reqSegMap.NumFrameMin)
            {
                m_bUseSegMap = true;
                MFX_CHECK_STS(m_SegMapDDI_hw.Init(pCore, &reqSegMap,true));
            }

            return MFX_ERR_NONE;
        }

        inline
        mfxStatus UpdateDpb(sFrameParams *pParams, sFrameEx *pRecFrame)
        {
            mfxU8   freeSurfCnt[REF_TOTAL] = {0, };
            sDpbVP8 dpbBeforeUpdate = m_dpb;

            // reset mapping of ref frames
            m_dpb.m_refMap[REF_BASE] = m_dpb.m_refMap[REF_GOLD] = m_dpb.m_refMap[REF_ALT] = 0;

            if (pParams->bIntra)
            {
                m_dpb.m_refFrames[REF_BASE] = pRecFrame;
                m_dpb.m_refFrames[REF_GOLD] = pRecFrame;
                m_dpb.m_refFrames[REF_ALT] =  pRecFrame;

                // treat all references as free - they all were replaced with current frame
                freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_BASE]] ++;
                freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_GOLD]] ++;
                freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_ALT]]  ++;
            }
            else
            {
                // refresh last_ref with current frame
                m_dpb.m_refFrames[REF_BASE] = pRecFrame;
                m_dpb.m_refMap[REF_BASE] = 0;
                // treat last_ref as free - it was replaced with current frame
                freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_BASE]] ++;

                if (pParams->bGold)
                {
                    // refresh gold_ref with current frame
                    m_dpb.m_refFrames[REF_GOLD] = pRecFrame;
                    m_dpb.m_refMap[REF_GOLD] = 0;
                    // treat gold_ref as free - it was replaced with current frame
                    freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_GOLD]] ++;
                }
                else
                {
                    if (pParams->copyToGoldRef == 1)
                    {
                        // replace gold_ref with last_ref
                        m_dpb.m_refFrames[REF_GOLD] = dpbBeforeUpdate.m_refFrames[REF_BASE];                        
                        // treat gold_ref as free - it was replaced with last_ref
                        freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_GOLD]] ++;
                        // treat last_ref as occupied - gold_ref was replaced with it
                        freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_BASE]] = 0;
                    }
                    else if (pParams->copyToGoldRef == 2)
                    {
                         // replace gold_ref with alt_ref
                        m_dpb.m_refFrames[REF_GOLD] = dpbBeforeUpdate.m_refFrames[REF_ALT];
                        // treat gold_ref as free - it was replaced with alt_ref
                        freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_GOLD]] ++;
                        // treat alt_ref as occupied - gold_ref was replaced with it
                        freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_ALT]] = 0;
                    }
                    m_dpb.m_refMap[REF_GOLD] = 1;
                }

                if (pParams->bAltRef)
                {
                    // refresh alt_ref with current frame
                    m_dpb.m_refFrames[REF_ALT] = pRecFrame;
                    m_dpb.m_refMap[REF_ALT] = 0;
                    // treat alt_ref as free - it was replaced with current frame
                    freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_ALT]] ++;
                }
                else
                {
                    if (pParams->copyToAltRef == 1)
                    {
                        // replace alt_ref with last_ref
                        m_dpb.m_refFrames[REF_ALT] = dpbBeforeUpdate.m_refFrames[REF_BASE];
                        // treat alt_ref as free - it was replaced with last_ref
                        freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_ALT]] ++;
                        // treat last_ref as occupied - alt_ref was replaced with it
                        freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_BASE]] = 0;
                    }
                    else if (pParams->copyToAltRef == 2)
                    {
                        // replace alt_ref with gold_ref
                        m_dpb.m_refFrames[REF_ALT] = dpbBeforeUpdate.m_refFrames[REF_GOLD];
                        // treat alt_ref as free - it was replaced with gold_ref
                        freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_ALT]] ++;
                        // treat gold_ref as occupied - alt_ref was replaced with it
                        freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_GOLD]] = 0;
                    }

                    if (m_dpb.m_refMap[REF_GOLD] == 0) // gold_ref was replaced with current frames
                        m_dpb.m_refMap[REF_ALT] = 1;
                    else
                        m_dpb.m_refMap[REF_ALT] = (m_dpb.m_refFrames[REF_ALT] == m_dpb.m_refFrames[REF_GOLD]) ? 1 : 2;
                }
            }

            if (m_dpb.m_refFrames[REF_GOLD] == dpbBeforeUpdate.m_refFrames[REF_GOLD])
                freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_GOLD]] = 0;
            if (m_dpb.m_refFrames[REF_ALT] == dpbBeforeUpdate.m_refFrames[REF_ALT])
                freeSurfCnt[dpbBeforeUpdate.m_refMap[REF_ALT]] = 0;

            // release reconstruct surfaces that didn't fit into updated dpb
            for (mfxU8 refIdx = 0; refIdx < REF_TOTAL; refIdx ++)
            {
                if (freeSurfCnt[dpbBeforeUpdate.m_refMap[refIdx]])
                {
                    MFX_CHECK_STS(FreeSurface(dpbBeforeUpdate.m_refFrames[refIdx],m_pCore));
                    freeSurfCnt[dpbBeforeUpdate.m_refMap[refIdx]] = 0; // reset counter to 0 to avoid second FreeSurface() call for same surface
                }
            }

            return MFX_ERR_NONE;
        }

        inline 
        mfxStatus Reset(mfxVideoParam *par)
        {
            m_maxBrcUpdateDelay = 2; // driver supports maximum 2-frames BRC update delay
            return BaseClass::Reset(par);
        }
        inline
        mfxStatus InitTask(mfxFrameSurface1* pSurface, mfxBitstream* pBitstream, Task* & pOutTask)
        {
            /*printf("TaskManagerHybridPakDDI InitTask: core %p\n", this->m_pCore);*/
            if (m_frameNum && m_cachedFrameInfoFromPak.size() == 0)
            {
                return MFX_WRN_DEVICE_BUSY;
            }

            mfxStatus sts = BaseClass::InitTask(pSurface,pBitstream,pOutTask);
            TaskHybridDDI *pHybridTask = (TaskHybridDDI*)pOutTask;
            Zero(pHybridTask->m_costs);
            if (pOutTask->m_frameOrder == 0)
            {
                return MFX_ERR_NONE;
            }            

            mfxU64 minFrameOrderToUpdateBrc = 0;
            if (pOutTask->m_frameOrder > m_maxBrcUpdateDelay)
            {
                minFrameOrderToUpdateBrc = pOutTask->m_frameOrder - m_maxBrcUpdateDelay;
            }
            FrameInfoFromPak newestFrame = m_cachedFrameInfoFromPak.back();
            if (newestFrame.m_frameOrder < minFrameOrderToUpdateBrc)
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            pHybridTask->m_prevFrameSize = newestFrame.m_encodedFrameSize;
            pHybridTask->m_brcUpdateDelay = mfxU8(pOutTask->m_frameOrder - newestFrame.m_frameOrder);

            while (m_cachedFrameInfoFromPak.size())
            {
                // store costs from last key and last non-key frames
                if (m_cachedFrameInfoFromPak.front().m_frameType == 0)
                    m_latestKeyFrame = m_cachedFrameInfoFromPak.front();
                else
                    m_latestNonKeyFrame = m_cachedFrameInfoFromPak.front();

                if (m_cachedFrameInfoFromPak.size() > 1 || pHybridTask->m_brcUpdateDelay == m_maxBrcUpdateDelay)
                {
                    // remove cached information if it's not longer required
                    m_cachedFrameInfoFromPak.pop();
                }
            }

            if (pHybridTask->m_frameOrder % m_video.mfx.GopPicSize == 0)
            {
                // key-frame
                pHybridTask->m_costs = m_latestKeyFrame.m_updatedCosts;
            }
            else
            {
                // non key-frame
                pHybridTask->m_costs = m_latestNonKeyFrame.m_updatedCosts;
            }

            return sts;
        }
        inline
        mfxStatus CacheInfoFromPak(Task &task, VP8HybridCosts & updatedCosts)
        {
            if (task.m_status != READY)
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            FrameInfoFromPak infoAboutJustEncodedFrame;
            infoAboutJustEncodedFrame.m_frameOrder = task.m_frameOrder;
            infoAboutJustEncodedFrame.m_encodedFrameSize = task.m_pBitsteam->DataLength;
            infoAboutJustEncodedFrame.m_updatedCosts = updatedCosts;
            infoAboutJustEncodedFrame.m_frameType = !task.m_sFrameParams.bIntra;
            m_cachedFrameInfoFromPak.push(infoAboutJustEncodedFrame);

            return MFX_ERR_NONE;
        }
        inline 
        mfxStatus SubmitTask(Task*  pTask, sFrameParams *pParams)
        {
            sFrameEx* pRecFrame = 0;
            sFrameEx* pRawLocalFrame = 0;
            sDDIFrames ddi_frames = {0};

            mfxStatus sts = MFX_ERR_NONE;
            /*printf("-------Submit task start\n");*/

            MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);

            pRecFrame = m_ReconFrames.GetFreeFrame();
            MFX_CHECK(pRecFrame!=0,MFX_WRN_DEVICE_BUSY);
            /*printf("Submit task, recon frame index = %d\n", pRecFrame->idInPool);*/

            if (m_bHWFrames != m_bHWInput)
            {
                /*printf("Submit task, m_LocalRawFrames\n");*/
                pRawLocalFrame = m_LocalRawFrames.GetFreeFrame();
                MFX_CHECK(pRawLocalFrame!= 0,MFX_WRN_DEVICE_BUSY);
                /*printf("Submit task, m_LocalRawFrames %d\n", pRawLocalFrame->idInPool);*/
            } 
            /*printf("Submit task, m_MBDataDDI_hw\n");*/
            MFX_CHECK_STS(m_MBDataDDI_hw.GetFrame(pRecFrame->idInPool,ddi_frames.m_pMB_hw));
            /*printf("Submit task, m_MBDataDDI_sw\n");*/
            MFX_CHECK_STS(m_MBDataDDI_sw.GetFrame(pRecFrame->idInPool,ddi_frames.m_pMB_sw));

            if (m_bUseSegMap)
            {
                MFX_CHECK_STS(m_SegMapDDI_hw.GetFrame(pRecFrame->idInPool,ddi_frames.m_pSegMap_hw));
            }
            else
                ddi_frames.m_pSegMap_hw = 0;


            /*printf("Submit task, m_DistDataDDI_hw\n");*/
            //MFX_CHECK_STS(m_DistDataDDI_hw.GetFrame(pRecFrame->idInPool,ddi_frames.m_pDist_hw));
            /*printf("Submit task, m_DistDataDDI_sw\n");*/
            //MFX_CHECK_STS(m_DistDataDDI_sw.GetFrame(pRecFrame->idInPool,ddi_frames.m_pDist_sw));

            sts = ((TaskHybridDDI*)pTask)->SubmitTask(  pRecFrame,m_dpb,
                                                        pParams, pRawLocalFrame,
                                                        &ddi_frames);
            MFX_CHECK_STS(sts);

            UpdateDpb(pParams, pRecFrame);

            return sts;
        }
        inline MfxFrameAllocResponse& GetRecFramesForReg()
        {
            return m_ReconFrames.GetFrameAllocReponse();
        }
        inline MfxFrameAllocResponse& GetMBFramesForReg()
        {
            return m_MBDataDDI_hw.GetFrameAllocReponse();
        }

        inline MfxFrameAllocResponse& GetDistFramesForReg()
        {
            return m_DistDataDDI_hw.GetFrameAllocReponse();
        }

        inline MfxFrameAllocResponse& GetSegMapFramesForReg()
        {
            return m_SegMapDDI_hw.GetFrameAllocReponse();
        }
    };
 #ifdef HYBRID_ENCODE

    /*for Hybrid Encode: HW ENC + SW PAK*/

    class TaskManagerHybridEncode
    {
    protected:

        VideoCORE*       m_pCore;
        VP8MfxParam      m_video;

        bool    m_bHWInput;

        ExternalFrames  m_RawFrames;
        InternalFrames  m_LocalRawFrames;
        InternalFrames  m_ReconFramesEnc;
        InternalFrames  m_ReconFramesPak;
        InternalFrames  m_MBDataDDI;

        std::vector<TaskHybridDDI>   m_TasksEnc;
        std::vector<TaskHybrid>      m_TasksPak;

        TaskHybridDDI*   m_pPrevSubmittedTaskEnc;
        TaskHybrid*      m_pPrevSubmittedTaskPak;

        mfxU32           m_frameNum;


    public:
        TaskManagerHybridEncode ():
          m_bHWInput(false),
              m_pCore(0),
              m_pPrevSubmittedTaskEnc(0),
              m_pPrevSubmittedTaskPak(),
              m_frameNum(0)
          {}

          ~TaskManagerHybridEncode()
          {
              FreeTasks(m_TasksEnc);
              FreeTasks(m_TasksPak);
          }

          mfxStatus Init(VideoCORE* pCore, mfxVideoParam *par, mfxFrameInfo* pDDIMBInfo, mfxU32 numDDIMBFrames);
           
          mfxStatus Reset(mfxVideoParam *par);
          mfxStatus InitTaskEnc(mfxFrameSurface1* pSurface, mfxBitstream* pBitstream, TaskHybridDDI* & pOutTask);
          mfxStatus SubmitTaskEnc(TaskHybridDDI*  pTask, sFrameParams *pParams);
          mfxStatus InitAndSubmitTaskPak(TaskHybridDDI* pTaskEnc, TaskHybrid * &pOutTaskPak);
          mfxStatus CompleteTasks(TaskHybridDDI* pTaskEnc, TaskHybrid * pTaskPak);
    };
#endif

}
#endif
#endif