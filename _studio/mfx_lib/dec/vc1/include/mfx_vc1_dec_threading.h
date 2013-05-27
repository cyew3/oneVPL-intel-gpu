/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2008 Intel Corporation. All Rights Reserved.
//          MFX VC-1 DEC threading support
//
*/
#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_DEC)

//#include "mfx_vc1_dec.h"
#include "umc_vc1_dec_job.h"
#include "mfx_vc1_dec_common.h"
#include "umc_vc1_dec_seq.h"

#ifndef _MFX_VC1_DEC_THREADING_H_
#define _MFX_VC1_DEC_THREADING_H_


typedef VC1Status (*B_MB_DECODE)(VC1Context* pContext);

//IQT functionality
class VC1DECIQT : public VC1TaskMfxBase
{
public:
    VC1DECIQT();
    virtual mfxStatus ProcessJob();
private:
    VC1Status (*m_pReconstructTbl[2])(VC1Context* pContext, Ipp32s blk_num);
};

//PredDEC functionality
class VC1DECPredDEC : public VC1TaskMfxBase
{
public:
    VC1DECPredDEC();
    virtual mfxStatus ProcessJob();
};

//PredDEC functionality
class VC1DECGetRecon : public VC1TaskMfxBase
{
public:
    VC1DECGetRecon();
    virtual mfxStatus ProcessJob();
};

//PredILDB  functionality
class VC1DECPredILDB : public VC1TaskMfxBase
{
public:
    VC1DECPredILDB();
    virtual mfxStatus Init(MfxVC1ThreadUnitParams*  pUnitParams,
                           MfxVC1ThreadUnitParams*  pPrevUnitParams,
                           Ipp32s                   threadOwn,
                           bool                     isReadyToProcess);
    virtual mfxStatus ProcessJob();
private:
     mfxI32        iPrevDblkStartPos; //need to interlace frames
};

//queue of tasks
template <class Task>
class VC1MfxTQueueCommon : public VC1MfxTQueueBase
{
public:
    VC1MfxTQueueCommon():m_bNoNeedToPerform(false)
    {
    };
    virtual ~VC1MfxTQueueCommon()
    {
    }
    virtual mfxStatus Init()
    {
        return MFX_ERR_NONE;
    };
    virtual VC1TaskMfxBase* GetNextStaticTask(Ipp32s threadID)
    {
        if (!m_bNoNeedToPerform)
        {
            Ipp32s count;
            for(count = 0; count < m_iTasksInQueue; count++)
            {
                if (m_Queue[count].IsReadyToWork()&&
                    (m_Queue[count].GetThreadOwner() == threadID))
                    return &m_Queue[count];
            }
        }
        return 0;
    };
    virtual VC1TaskMfxBase* GetNextDynamicTask(Ipp32s threadID)
    {
        return 0;
    };

    virtual bool IsFuncComplte()
    {
        if (!m_bNoNeedToPerform)
        {
            Ipp32s count;
            for(count = 0; count < m_iTasksInQueue; count++)
            {
                if (!m_Queue[count].IsDone())
                    return false;
            }
            m_iTasksInQueue = 0;
            return true;
        }
        else
        {
            m_iTasksInQueue = 0;
            return true;
        }
    };
    virtual void SetQueueAsPerformed() {m_bNoNeedToPerform = true;}
    virtual void SetQueueAsReady() {m_bNoNeedToPerform = false;}

protected:
    bool m_bNoNeedToPerform;
    Task m_Queue[VC1_MAX_SLICE_NUM];
};


// Queue of IQT and Pred parallel tasks
template<class Task>
class VC1MfxTQueueDecParal : public VC1MfxTQueueCommon<Task>
{
public:
    mfxStatus       FormalizeSliceTaskGroup(MfxVC1ThreadUnitParams* pSliceParams)
    {
        MFX_CHECK_NULL_PTR1(pSliceParams);
        Ipp16u curMBrow = pSliceParams->MBStartRow;
        // parameters fro single task
        MfxVC1ThreadUnitParams TaskParams;

        bool isFirstSlieceDecodeTask = true;
        while (curMBrow  < pSliceParams->MBEndRow)
        {
            TaskParams.isFirstInSlice = isFirstSlieceDecodeTask;
            TaskParams.MBStartRow = curMBrow;
            TaskParams.MBEndRow = curMBrow + VC1MBQUANT;
            TaskParams.MBRowsToDecode = VC1MBQUANT;
            TaskParams.pContext = pSliceParams->pContext;
            TaskParams.pCUC = pSliceParams->pCUC;
            TaskParams.isFullMode = pSliceParams->isFullMode;


            if ((TaskParams.MBRowsToDecode + TaskParams.MBStartRow) > pSliceParams->MBEndRow)
            {
                TaskParams.MBEndRow = pSliceParams->MBEndRow;
                TaskParams.MBRowsToDecode = TaskParams.MBEndRow - pSliceParams->MBStartRow;
            }
            if (TaskParams.MBEndRow != pSliceParams->MBEndRow)
                TaskParams.isLastInSlice = false; // need to correct
            else
                TaskParams.isLastInSlice = true; // need to correct

            m_Queue[m_iTasksInQueue].Init(&TaskParams,
                                          0,
                                          0,
                                          true);

            ++m_iTasksInQueue;
            curMBrow += VC1MBQUANT;
            ++m_iCurrentTaskID;
        }
        return MFX_ERR_NONE;
    };
};

// Queue of GetRecon and Dbl sequence tasks
template <class Task>
class VC1MfxTQueueDecSeq : public VC1MfxTQueueCommon<Task>
{
public:
    virtual  VC1TaskMfxBase* GetNextStaticTask(Ipp32s threadID)
    {
        if (!m_bNoNeedToPerform)
        {
            Ipp32s count;
            for(count = 0; count < m_iTasksInQueue; count++)
            {
                if (m_Queue[count].IsReadyToWork()&&
                    (m_Queue[count].GetThreadOwner() == threadID))
                    return &m_Queue[count];
                else if (count > 0)
                {
                    if ((m_Queue[count-1].IsDone()&&
                        (!m_Queue[count].IsReadyToWork())&&
                        (!m_Queue[count].IsDone())&&
                        (m_Queue[count].GetThreadOwner() == threadID)))
                    {
                        return &m_Queue[count];
                    }
                }
            }
        }
        return 0;
    };
    mfxStatus       FormalizeSliceTaskGroup(MfxVC1ThreadUnitParams* pSliceParams, bool IsSingle)
    {
        MFX_CHECK_NULL_PTR1(pSliceParams);
        Ipp16u curMBrow = pSliceParams->MBStartRow;
        // parameters fro single task
        MfxVC1ThreadUnitParams TaskParams;
        bool isFirstSlieceDecodeTask = true;
        MfxVC1ThreadUnitParams PrevTaskParams = {0};
        while (curMBrow  < pSliceParams->MBEndRow)
        {
            //VC1BSDRunBSD pTask = m_pRunBsdQueue[m_iTasksInQueue];
            TaskParams.isFirstInSlice = isFirstSlieceDecodeTask;

            if (isFirstSlieceDecodeTask)
                isFirstSlieceDecodeTask = false;

            TaskParams.MBStartRow = curMBrow;
            TaskParams.MBEndRow = curMBrow + VC1MBQUANT;
            TaskParams.MBRowsToDecode = VC1MBQUANT;
            TaskParams.pContext = pSliceParams->pContext;
            TaskParams.pCUC = pSliceParams->pCUC;


            if ((TaskParams.MBRowsToDecode + TaskParams.MBStartRow) > pSliceParams->MBEndRow)
            {
                TaskParams.MBEndRow = pSliceParams->MBEndRow;
                TaskParams.MBRowsToDecode = TaskParams.MBEndRow - pSliceParams->MBStartRow;
            }

            if (TaskParams.MBEndRow != pSliceParams->MBEndRow)
                TaskParams.isLastInSlice = false; // need to correct
            else
                TaskParams.isLastInSlice = true; // need to correct


            m_Queue[m_iTasksInQueue].Init(&TaskParams,
                                          &PrevTaskParams,
                                          0,
                                          TaskParams.isFirstInSlice);

            PrevTaskParams = TaskParams;
            ++m_iTasksInQueue;
            curMBrow += VC1MBQUANT;
            ++m_iCurrentTaskID;
        }
        return MFX_ERR_NONE;
    };
};

class VC1MfxTQueueDecFullDec : public VC1MfxTQueueBase
{
public:
    virtual mfxStatus Init()
    {
        return MFX_ERR_NONE;
    };
    virtual         VC1TaskMfxBase* GetNextStaticTask(Ipp32s threadID);
    virtual         VC1TaskMfxBase* GetNextDynamicTask(Ipp32s threadID){return 0;};

    mfxStatus       FormalizeSliceTaskGroup(MfxVC1ThreadUnitParams* pSliceParams);

    virtual bool    IsFuncComplte();

    void SetEntrypoint(MFXVC1DecCommon::VC1DecEntryPoints funcID) {m_CurFuncID = funcID;}
private:
    VC1MfxTQueueDecParal<VC1DECIQT>     m_IqtQ;
    VC1MfxTQueueDecParal<VC1DECPredDEC> m_PredDec;
    VC1MfxTQueueDecSeq<VC1DECGetRecon>  m_GReconQ;
    VC1MfxTQueueDecSeq<VC1DECPredILDB>  m_DeblcQ;

    MFXVC1DecCommon::VC1DecEntryPoints m_CurFuncID;
};




class VC1TaskProcessorDEC:  public UMC::VC1TaskProcessor
{
public:
    VC1TaskProcessorDEC()
    {
    };
    virtual ~VC1TaskProcessorDEC()
    {
    };
    virtual UMC::Status process();
    virtual UMC::Status processMainThread();
};
#endif
#endif
