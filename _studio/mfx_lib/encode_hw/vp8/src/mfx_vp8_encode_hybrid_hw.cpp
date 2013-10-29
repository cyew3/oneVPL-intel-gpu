/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
//          VP8 encoder part
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE) 
#include "mfx_vp8_encode_hybrid_hw.h"
#include "mfx_vp8_enc_common.h"
#include "mfx_task.h"


#ifdef WIN64
    #ifdef _DEBUG
        #define DLL_FILENAME    _T("CmodelVp8enc64_d.dll")
    #else
        #define DLL_FILENAME    _T("CmodelVp8enc64.dll")
    #endif //_DEBUG
#else
#ifdef  WIN32
    #ifdef _DEBUG
        #define DLL_FILENAME    _T("CmodelVp8enc32_d.dll")
    #else
        #define DLL_FILENAME    _T("CmodelVp8enc32.dll")
    #endif //_DEBUG
#else
     #define DLL_FILENAME    _T("CmodelVp8enc.dll")
#endif //WIN64
#endif //WIN32


int (MFX_CDECL *pCMODEL_VP8_ENCODER_Create)(CMODEL_VP8_ENCODER::Implementation *& pImpl);

int (MFX_CDECL *pCMODEL_VP8_ENCODER_Delete)(CMODEL_VP8_ENCODER::Implementation *& pImpl);


int (MFX_CDECL *pCMODEL_VP8_ENCODER_Init)( CMODEL_VP8_ENCODER::Implementation *pImpl,
                                        CMODEL_VP8_ENCODER::SeqParams *pParams, 
                                        CMODEL_VP8_ENCODER::eMode mode,
                                        CMODEL_VP8_ENCODER::MbCodeVp8** ppMBData);

int(MFX_CDECL *pCMODEL_VP8_ENCODER_SetNextFrame)(CMODEL_VP8_ENCODER::Implementation *pImpl,
                                                CMODEL_VP8_ENCODER::Frames *pFrames, 
                                                CMODEL_VP8_ENCODER::PicParam *pFrameParams,
                                                int    FrameNumber);  

int (MFX_CDECL *pCMODEL_VP8_ENCODER_RunENC)(CMODEL_VP8_ENCODER::Implementation *pImpl,
                                            CMODEL_VP8_ENCODER::MbCodeVp8* pOutMBData);

int (MFX_CDECL *pCMODEL_VP8_ENCODER_RunPAK)(CMODEL_VP8_ENCODER::Implementation *pImpl,
                                            CMODEL_VP8_ENCODER::MbCodeVp8* pInOutMBData, 
                                            CMODEL_VP8_ENCODER::Frames *pRecFrames );

int (MFX_CDECL *pCMODEL_VP8_ENCODER_RunBSP)(CMODEL_VP8_ENCODER::Implementation *pImpl,
                                            CMODEL_VP8_ENCODER::MbCodeVp8* pInMBData,                                             
                                            bool   bSeqHeader,
                                            unsigned char *pBitstream, 
                                            unsigned int &len, 
                                            unsigned int maxLen); 

int (MFX_CDECL *pCMODEL_VP8_ENCODER_FinishFrame)(CMODEL_VP8_ENCODER::Implementation *pImpl);

#define LOAD_FUNC(FUNC)                                         \
    *(FARPROC*)&p##FUNC = GetProcAddress(m_pDLL, #FUNC);        \

namespace MFX_VP8ENC
{
    //---------------------------------------------------------
    // service class: CModel
    //---------------------------------------------------------

    mfxStatus CModel::InitCModelSeqParams(const mfxVideoParam &video, CMODEL_VP8_ENCODER::SeqParams &dst)
    {
        mfxExtCodingOptionVP8 *pOpt= GetExtBuffer(video);

        dst.Width  = video.mfx.FrameInfo.CropW!=0 ? video.mfx.FrameInfo.CropW: video.mfx.FrameInfo.Width;
        dst.Height = video.mfx.FrameInfo.CropH!=0 ? video.mfx.FrameInfo.CropH: video.mfx.FrameInfo.Height;
        dst.FRateN = video.mfx.FrameInfo.FrameRateExtN;
        dst.FRateD = video.mfx.FrameInfo.FrameRateExtD;

        MFX_CHECK (video.mfx.RateControlMethod == MFX_RATECONTROL_CQP, MFX_ERR_UNSUPPORTED);
        MFX_CHECK (pOpt->TokenPartitions < 2, MFX_ERR_UNSUPPORTED);

        dst.QPI =  video.mfx.QPI;
        dst.QPP =  video.mfx.QPP;
        dst.bEnableSeg = pOpt->EnableMultipleSegments?1:0;

        return MFX_ERR_NONE;
    }
    mfxStatus CModel::InitFrameParams(  const sFrameParams &srcParams, 
                                        CMODEL_VP8_ENCODER::PicParam &dstParams)
    {
        dstParams.bIntra    = srcParams.bIntra;
        dstParams.bGold     = srcParams.bGold;
        dstParams.bAltRef   = srcParams.bAltRef;

        return MFX_ERR_NONE;
    }
    mfxStatus CModel::CopyMBData(std::vector<sMBData> & pSrc, CMODEL_VP8_ENCODER::MbCodeVp8 *pDst)
    {
        MFX_CHECK(sizeof(sMBData) == sizeof(CMODEL_VP8_ENCODER::MbCodeVp8),MFX_ERR_UNDEFINED_BEHAVIOR);

        std::vector<sMBData>::iterator src = pSrc.begin();
        for ( ;src!= pSrc.end(); src++)
        {
            MFX_INTERNAL_CPY(pDst++,&(src[0]), sizeof(sMBData));
        }
        return MFX_ERR_NONE;
    }
    mfxStatus CModel::CopyMBData( CMODEL_VP8_ENCODER::MbCodeVp8 *pSrc, std::vector<sMBData> & pDst)
    {
        MFX_CHECK(sizeof(sMBData) == sizeof(CMODEL_VP8_ENCODER::MbCodeVp8),MFX_ERR_UNDEFINED_BEHAVIOR)
        std::vector<sMBData>::iterator dst = pDst.begin();
        for ( ;dst!= pDst.end(); dst++)
        {
            MFX_INTERNAL_CPY(&(dst[0]),pSrc++, sizeof(sMBData));
        }
        return MFX_ERR_NONE;
    }
    mfxStatus CModel::InitDLL()
    {
        MFX_CHECK(m_pDLL == 0, MFX_ERR_UNDEFINED_BEHAVIOR);
        HMODULE m_pDLL = LoadLibrary(DLL_FILENAME);
        if (m_pDLL == 0)
        {
            _tprintf(_T("\n\nVP8 encoder error: %s can't be loaded\n\n"), DLL_FILENAME);  
            return MFX_ERR_INVALID_HANDLE;
        }
        LOAD_FUNC(CMODEL_VP8_ENCODER_Create);
        LOAD_FUNC(CMODEL_VP8_ENCODER_Delete);
        LOAD_FUNC(CMODEL_VP8_ENCODER_Init);
        LOAD_FUNC(CMODEL_VP8_ENCODER_SetNextFrame);
        LOAD_FUNC(CMODEL_VP8_ENCODER_RunENC);
        LOAD_FUNC(CMODEL_VP8_ENCODER_RunPAK);
        LOAD_FUNC(CMODEL_VP8_ENCODER_RunBSP);
        LOAD_FUNC(CMODEL_VP8_ENCODER_FinishFrame);

        MFX_CHECK_NULL_PTR3(pCMODEL_VP8_ENCODER_Create,pCMODEL_VP8_ENCODER_Delete, pCMODEL_VP8_ENCODER_Init);
        MFX_CHECK_NULL_PTR3(pCMODEL_VP8_ENCODER_SetNextFrame,pCMODEL_VP8_ENCODER_RunENC, pCMODEL_VP8_ENCODER_RunPAK);
        MFX_CHECK_NULL_PTR2(pCMODEL_VP8_ENCODER_RunBSP,pCMODEL_VP8_ENCODER_FinishFrame);

        return MFX_ERR_NONE;
    }
    mfxStatus CModel::FreeDLL()
    {
        if (m_pDLL)
        {
            FreeLibrary(m_pDLL);
            m_pDLL = 0;
        }
        return MFX_ERR_NONE;
    }
    mfxStatus CModel::Init(const mfxVideoParam &video,CMODEL_VP8_ENCODER::eMode mode, VideoCORE  *pCore)
    {
        mfxStatus  sts = MFX_ERR_NONE;
        CMODEL_VP8_ENCODER::SeqParams   CmodelParams= {0};

        MFX_CHECK(m_pDLL == 0,MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(m_pImpl == 0,MFX_ERR_UNDEFINED_BEHAVIOR); 

        m_pCore = pCore;

        sts = InitDLL();
        MFX_CHECK_STS(sts);

        sts =(mfxStatus)pCMODEL_VP8_ENCODER_Create (m_pImpl);
        MFX_CHECK_STS(sts);

        sts = InitCModelSeqParams(video, CmodelParams);
        MFX_CHECK_STS(sts);

        sts =(mfxStatus)pCMODEL_VP8_ENCODER_Init(m_pImpl,&CmodelParams,mode,&m_pMBData);
        MFX_CHECK_STS(sts);

        m_frameNum = 0;

        return sts;    
    }
    mfxStatus CModel::Close()
    {
        mfxStatus  sts = MFX_ERR_NONE;

        if (pCMODEL_VP8_ENCODER_Delete)
        {
            sts = (mfxStatus)pCMODEL_VP8_ENCODER_Delete(m_pImpl); 
            MFX_CHECK_STS(sts);
        }

        sts = FreeDLL();
        MFX_CHECK_STS(sts);

        pCMODEL_VP8_ENCODER_Create = 0;
        pCMODEL_VP8_ENCODER_Delete = 0;
        pCMODEL_VP8_ENCODER_Init  = 0;
        pCMODEL_VP8_ENCODER_SetNextFrame = 0;
        pCMODEL_VP8_ENCODER_RunENC = 0;
        pCMODEL_VP8_ENCODER_RunPAK = 0;
        pCMODEL_VP8_ENCODER_RunBSP = 0;
        pCMODEL_VP8_ENCODER_FinishFrame = 0;

        m_frameNum = 0;

        return sts;
    }

    mfxStatus CModel::SetNextFrame(mfxFrameSurface1 * pSurface, bool bExternal, const sFrameParams &frameParams, mfxU32 frameNum)
    {
        mfxStatus  sts = MFX_ERR_NONE;
        CMODEL_VP8_ENCODER::Frames      frames = {0};
        CMODEL_VP8_ENCODER::PicParam    CmodelFrameParams = {0};
        mfxFrameSurface1 src={0};

        sts = InitFrameParams(frameParams,CmodelFrameParams);
        MFX_CHECK_STS(sts);

        m_frameNum = frameNum;

        src.Data = pSurface->Data;
        src.Info = pSurface->Info;

        FrameLocker lockSrc(m_pCore, src.Data, bExternal);
        
        frames.pFrame[0] = src.Data.Y;
        frames.pFrame[1] = src.Data.U;
        frames.pFrame[2] = src.Data.V;

        frames.frameStep[0] = src.Data.Pitch;
        frames.frameStep[1] = src.Data.Pitch;
        frames.frameStep[2] = src.Data.Pitch;

        sts = (mfxStatus)pCMODEL_VP8_ENCODER_SetNextFrame( m_pImpl, &frames, &CmodelFrameParams,m_frameNum);
        MFX_CHECK_STS(sts);

        return sts;
    }
    mfxStatus CModel::RunEnc(std::vector<sMBData> & pMBData)
    {
        mfxStatus  sts = MFX_ERR_NONE;

        MFX_CHECK_NULL_PTR1(m_pMBData);

        sts = (mfxStatus)pCMODEL_VP8_ENCODER_RunENC( m_pImpl,m_pMBData);
        MFX_CHECK_STS(sts);

        sts = CopyMBData(m_pMBData, pMBData);
        MFX_CHECK_STS(sts);

        return sts;   
    }
    mfxStatus CModel::RunPak(std::vector<sMBData> & pMBData, mfxFrameSurface1 *pRecSurface)
    {
        mfxStatus  sts = MFX_ERR_NONE;

        CMODEL_VP8_ENCODER::Frames  frames = {0};
        mfxFrameSurface1 dst={0};

        MFX_CHECK_NULL_PTR1(m_pMBData);
        
        sts = CopyMBData(pMBData,m_pMBData);
        MFX_CHECK_STS(sts);

        dst.Data = pRecSurface->Data;
        dst.Info = pRecSurface->Info;

        FrameLocker lockDst(m_pCore, dst.Data, false);
        
        frames.pFrame[0] = dst.Data.Y;
        frames.pFrame[1] = dst.Data.U;
        frames.pFrame[2] = dst.Data.V;

        frames.frameStep[0] = dst.Data.Pitch;
        frames.frameStep[1] = dst.Data.Pitch;
        frames.frameStep[2] = dst.Data.Pitch;

        sts = (mfxStatus)pCMODEL_VP8_ENCODER_RunPAK( m_pImpl,m_pMBData,&frames);
        MFX_CHECK_STS(sts);
        
        sts = CopyMBData(m_pMBData, pMBData);
        MFX_CHECK_STS(sts);

        return sts;   
    }
    mfxStatus CModel::RunBSP(std::vector<sMBData> & pMBData, bool bAddSeqHeader ,mfxBitstream *pBitstream)
    {
        mfxStatus  sts = MFX_ERR_NONE;

        MFX_CHECK_NULL_PTR1(m_pMBData);
        
        sts = CopyMBData(pMBData,m_pMBData);
        MFX_CHECK_STS(sts);

        sts = (mfxStatus)pCMODEL_VP8_ENCODER_RunBSP( m_pImpl,m_pMBData, bAddSeqHeader,
                                                     pBitstream->Data,pBitstream->DataLength,pBitstream->MaxLength);
        MFX_CHECK_STS(sts); 
        

        return sts;   
    }

    mfxStatus CModel::FinishFrame()
    {
        mfxStatus  sts = MFX_ERR_NONE;

        sts = (mfxStatus)pCMODEL_VP8_ENCODER_FinishFrame(m_pImpl);
        MFX_CHECK_STS(sts);  

        return sts;
    }

    //---------------------------------------------------------
    // Implementation via CModel
    //---------------------------------------------------------

    mfxStatus CmodelImpl::Init(mfxVideoParam * par)
    {
        
        mfxStatus sts  = MFX_ERR_NONE;
        mfxStatus sts1 = MFX_ERR_NONE; // to save warnings ater parameters checking
        m_video = *par;
        {  
            mfxExtCodingOptionVP8*       pExtOpt    = GetExtBuffer(m_video);        
            mfxExtOpaqueSurfaceAlloc*    pExtOpaque = GetExtBuffer(m_video);
      
            MFX_CHECK(CheckFrameSize(par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height),MFX_ERR_INVALID_VIDEO_PARAM);

            sts1 = VP8_encoder::CheckParametersAndSetDefault(par,&m_video, pExtOpt, pExtOpaque, m_core->IsExternalFrameAllocator(),false);
            MFX_CHECK(sts1 >=0, sts1);   
        }

        sts = m_taskManager.Init(m_core,&m_video,false);
        MFX_CHECK_STS(sts);        

        sts = m_CModel.Init(m_video,CMODEL_VP8_ENCODER::ENC,m_core);
        MFX_CHECK_STS(sts);

        return sts1;
    }
 
    mfxStatus CmodelImpl::Reset(mfxVideoParam * par)
    {
        mfxStatus sts  = MFX_ERR_NONE;
        mfxStatus sts1 = MFX_ERR_NONE;

        MFX_CHECK_NULL_PTR1(par);
        MFX_CHECK(par->IOPattern == m_video.IOPattern, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        m_video     = *par;

        {
            mfxExtCodingOptionVP8*       pExtOpt = GetExtBuffer(m_video);        
            mfxExtOpaqueSurfaceAlloc*    pExtOpaque = GetExtBuffer(m_video);

            sts1 = VP8_encoder::CheckParametersAndSetDefault(par,&m_video, pExtOpt, pExtOpaque,m_core->IsExternalFrameAllocator(),true);
            MFX_CHECK(sts1>=0, sts1);
        }
        sts = m_taskManager.Reset(&m_video);
        MFX_CHECK_STS(sts);

        sts = m_CModel.Close();(m_video,CMODEL_VP8_ENCODER::ENC,m_core);
        MFX_CHECK_STS(sts);

        sts = m_CModel.Init(m_video,CMODEL_VP8_ENCODER::ENC,m_core);
        MFX_CHECK_STS(sts);
  
        return sts1;
    } 

    mfxStatus CmodelImpl::GetVideoParam(mfxVideoParam * par)
    {
        MFX_CHECK_NULL_PTR1(par);
        return MFX_VP8ENC::GetVideoParam(par,&m_video);
    } 
        

     mfxStatus CmodelImpl::EncodeFrameCheck( mfxEncodeCtrl *ctrl,
                                            mfxFrameSurface1 *surface,
                                            mfxBitstream *bs,
                                            mfxFrameSurface1 ** /*reordered_surface*/,
                                            mfxEncodeInternalParams * /*pInternalParams*/,
                                            MFX_ENTRY_POINT *pEntryPoint)


    {
        Task* pTask = 0;
        mfxStatus sts  = MFX_ERR_NONE;

        mfxStatus checkSts = CheckEncodeFrameParam(
            m_video,
            ctrl,
            surface,
            bs,
            m_core->IsExternalFrameAllocator());

        MFX_CHECK(checkSts >= MFX_ERR_NONE, checkSts);

        sts = m_taskManager.InitTask(surface,bs,pTask);
        MFX_CHECK_STS(sts);        

        pEntryPoint->pState               = this;
        pEntryPoint->pCompleteProc        = 0;
        pEntryPoint->pGetSubTaskProc      = 0;
        pEntryPoint->pCompleteSubTaskProc = 0;
        pEntryPoint->requiredNumThreads   = 1;
        pEntryPoint->pRoutineName         = "Encode Frame";
        
        pEntryPoint->pRoutine            = TaskRoutine;
        pEntryPoint->pParam              = pTask;       

        return checkSts;
    } 
    mfxStatus CmodelImpl::TaskRoutine(  void * state,
                                         void * param,
                                         mfxU32 /*threadNumber*/,
                                         mfxU32 /*callNumber*/)
     {
         CmodelImpl &    impl = *(CmodelImpl *) state;
         Task *          task =  (Task *)param;

         mfxStatus sts = impl.EncodeFrame(task);
         MFX_CHECK_STS(sts);

         return MFX_TASK_DONE;
     } 

     mfxStatus CmodelImpl::EncodeFrame(Task *pTaskInput)
     { 
        TaskHybrid          *pTask = (TaskHybrid*)pTaskInput;
        mfxStatus           sts = MFX_ERR_NONE;
        sFrameParams        frameParams={0};
        mfxFrameSurface1    *pSurface=0;
        bool                bExternalSurface = true;

        sts = SetFramesParams(&m_video,pTask->m_frameOrder, &frameParams);
        MFX_CHECK_STS(sts);

        sts = m_taskManager.SubmitTask(pTask,&frameParams);
        MFX_CHECK_STS(sts);

        sts = pTask->GetInputSurface(pSurface, bExternalSurface);
        MFX_CHECK_STS(sts);

        sts = m_CModel.SetNextFrame(pSurface, bExternalSurface, pTask->m_sFrameParams,pTask->m_frameOrder);
        MFX_CHECK_STS(sts);

        sts = m_CModel.RunEnc(pTask->m_MBData);
        MFX_CHECK_STS(sts);

        sts = m_CModel.RunPak(pTask->m_MBData, pTask->m_pRecFrame->pSurface);
        MFX_CHECK_STS(sts);

        sts = m_CModel.RunBSP(pTask->m_MBData,pTask->m_frameOrder==0, pTask->m_pBitsteam);
        MFX_CHECK_STS(sts);
        
        sts = pTask->CompleteTask();
        MFX_CHECK_STS(sts);

        sts = m_CModel.FinishFrame();
        MFX_CHECK_STS(sts);

        return sts;
     } 

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

    //---------------------------------------------------------
    // Implementation via HybridPakDDIImpl
    //---------------------------------------------------------
    mfxStatus CopyMBData (TaskHybridDDI *pTask, VideoCORE * pCore)
    {
        FrameLocker lockSrc(pCore, pTask->m_pMBFrameDDI->pSurface->Data, false);
        
        // coping from MBFrameDDI into pTask->m_MBData (mapping from DDI MBdata into MediaSDK (C model))
        // not implemented now

        return MFX_ERR_UNSUPPORTED;    
    }
    mfxStatus HybridPakDDIImpl::Init(mfxVideoParam * par)
    {
        
        mfxStatus sts  = MFX_ERR_NONE;
        mfxStatus sts1 = MFX_ERR_NONE; // to save warnings after parameters checking
        m_video = *par;
        {  
            mfxExtCodingOptionVP8*       pExtOpt    = GetExtBuffer(m_video);        
            mfxExtOpaqueSurfaceAlloc*    pExtOpaque = GetExtBuffer(m_video);
      
            MFX_CHECK(CheckFrameSize(par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height),MFX_ERR_INVALID_VIDEO_PARAM);

            sts1 = VP8_encoder::CheckParametersAndSetDefault(par,&m_video, pExtOpt, pExtOpaque, m_core->IsExternalFrameAllocator(),false);
            MFX_CHECK(sts1 >= 0, sts1);   
        }

        m_ddi.reset(CreatePlatformVp8Encoder(m_core));
        MFX_CHECK(m_ddi.get() != 0, MFX_WRN_PARTIAL_ACCELERATION);
   
        sts = m_ddi->CreateAuxilliaryDevice(m_core,DXVA2_Intel_Encode_VP8, 
                                            m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);
        MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

        ENCODE_CAPS_VP8 caps = {0};
        sts = m_ddi->QueryEncodeCaps(caps);
        MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

        sts = CheckVideoParam(m_video, caps);
        MFX_CHECK_STS(sts);

        sts = m_ddi->CreateAccelerationService(m_video);
        MFX_CHECK_STS(sts);

        mfxFrameAllocRequest reqMB      = { 0 };
        mfxFrameAllocRequest reqMV      = { 0 };
        mfxFrameAllocRequest reqDist    = { 0 };

        sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_MBDATA, reqMB);
        MFX_CHECK_STS(sts);
        sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_MOTIONVECTORBUFFER, reqMV);
        MFX_CHECK_STS(sts);
        sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_DISTORTIONDATA, reqDist);
        MFX_CHECK_STS(sts);


        sts = m_taskManager.Init(m_core,&m_video,
                                 &reqMB.Info,&reqMV.Info,&reqDist.Info,reqMB.NumFrameSuggested);
        MFX_CHECK_STS(sts);

        sts = m_ddi->Register(m_taskManager.GetRecFramesForReg(), D3DDDIFMT_NV12);
        MFX_CHECK_STS(sts);

        sts = m_ddi->Register(m_taskManager.GetMBFramesForReg(), D3DDDIFMT_INTELENCODE_MBDATA);
        MFX_CHECK_STS(sts);

        sts = m_ddi->Register(m_taskManager.GetMVFramesForReg(), D3DDDIFMT_MOTIONVECTORBUFFER);
        MFX_CHECK_STS(sts);

        sts = m_ddi->Register(m_taskManager.GetDistFramesForReg(), D3DDDIFMT_INTELENCODE_DISTORTIONDATA);
        MFX_CHECK_STS(sts);

        sts = m_CModel.Init(m_video,CMODEL_VP8_ENCODER::BSP,m_core);
        MFX_CHECK_STS(sts);

        return sts1;
    }
 
    mfxStatus HybridPakDDIImpl::Reset(mfxVideoParam * par)
    {
        mfxStatus sts  = MFX_ERR_NONE;
        mfxStatus sts1 = MFX_ERR_NONE;

        MFX_CHECK_NULL_PTR1(par);
        MFX_CHECK(par->IOPattern == m_video.IOPattern, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        m_video     = *par;

        {
            mfxExtCodingOptionVP8*       pExtOpt = GetExtBuffer(m_video);        
            mfxExtOpaqueSurfaceAlloc*    pExtOpaque = GetExtBuffer(m_video);

            sts1 = VP8_encoder::CheckParametersAndSetDefault(par,&m_video, pExtOpt, pExtOpaque,m_core->IsExternalFrameAllocator(),true);
            MFX_CHECK(sts1>=0, sts1);
        }
        sts = m_taskManager.Reset(&m_video);
        MFX_CHECK_STS(sts);

        sts = m_CModel.Close();
        MFX_CHECK_STS(sts);

        sts = m_CModel.Init(m_video,CMODEL_VP8_ENCODER::BSP,m_core);
        MFX_CHECK_STS(sts);

        sts = m_ddi->Reset(*par);
        MFX_CHECK_STS(sts);
  
        return sts1;
    } 

    mfxStatus HybridPakDDIImpl::GetVideoParam(mfxVideoParam * par)
    {
        MFX_CHECK_NULL_PTR1(par);
        return MFX_VP8ENC::GetVideoParam(par,&m_video);
    } 
        

     mfxStatus HybridPakDDIImpl::EncodeFrameCheck( mfxEncodeCtrl *ctrl,
                                            mfxFrameSurface1 *surface,
                                            mfxBitstream *bs,
                                            mfxFrameSurface1 ** /*reordered_surface*/,
                                            mfxEncodeInternalParams * /*pInternalParams*/,
                                            MFX_ENTRY_POINT *pEntryPoint)


    {
        Task* pTask = 0;
        mfxStatus sts  = MFX_ERR_NONE;

        mfxStatus checkSts = CheckEncodeFrameParam(
            m_video,
            ctrl,
            surface,
            bs,
            m_core->IsExternalFrameAllocator());

        MFX_CHECK(checkSts >= MFX_ERR_NONE, checkSts);

        sts = m_taskManager.InitTask(surface,bs,pTask);
        MFX_CHECK_STS(sts);        

        pEntryPoint->pState               = this;
        pEntryPoint->pCompleteProc        = 0;
        pEntryPoint->pGetSubTaskProc      = 0;
        pEntryPoint->pCompleteSubTaskProc = 0;
        pEntryPoint->requiredNumThreads   = 1;
        pEntryPoint->pRoutineName         = "Encode Frame";
        
        pEntryPoint->pRoutine            = TaskRoutine;
        pEntryPoint->pParam              = pTask;       

        return checkSts;
    } 
    mfxStatus HybridPakDDIImpl::TaskRoutine(  void * state,
                                         void * param,
                                         mfxU32 /*threadNumber*/,
                                         mfxU32 /*callNumber*/)
     {
        HybridPakDDIImpl &    impl = *(HybridPakDDIImpl *) state;
        Task *          task =  (Task *)param;

         mfxStatus sts = impl.EncodeFrame(task);
         MFX_CHECK_STS(sts);

         return MFX_TASK_DONE;
     } 

     mfxStatus HybridPakDDIImpl::EncodeFrame(Task *pTaskInput)
     { 
        TaskHybridDDI       *pTask = (TaskHybridDDI*)pTaskInput;
        mfxStatus           sts = MFX_ERR_NONE;
        sFrameParams        frameParams={0};
        mfxFrameSurface1    *pSurface=0;
        bool                bExternalSurface = true;

       
        mfxHDLPair surfacePair = {0}; // D3D11        
        mfxHDL     surfaceHDL = 0;// others
        mfxHDL *pSurfaceHdl = (mfxHDL *)&surfaceHDL;

        if (MFX_HW_D3D11 == m_core->GetVAType())
            pSurfaceHdl = (mfxHDL *)&surfacePair;

        sts = SetFramesParams(&m_video,pTask->m_frameOrder, &frameParams);
        MFX_CHECK_STS(sts);

        sts = m_taskManager.SubmitTask(pTask,&frameParams);
        MFX_CHECK_STS(sts);

        sts = pTask->GetInputSurface(pSurface, bExternalSurface);
        MFX_CHECK_STS(sts);

        sts = m_CModel.SetNextFrame(pSurface, bExternalSurface, pTask->m_sFrameParams,pTask->m_frameOrder);
        MFX_CHECK_STS(sts);

        sts = bExternalSurface ?
            m_core->GetExternalFrameHDL(pSurface->Data.MemId, pSurfaceHdl):
            m_core->GetFrameHDL(pSurface->Data.MemId, pSurfaceHdl);
        MFX_CHECK_STS(sts);

       if (MFX_HW_D3D11 == m_core->GetVAType())
        {
            MFX_CHECK(surfacePair.first != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            sts = m_ddi->Execute(*pTask, (mfxHDL)pSurfaceHdl);
        }
        else if (MFX_HW_D3D9 == m_core->GetVAType())
        {
            MFX_CHECK(surfaceHDL != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            sts = m_ddi->Execute(*pTask, surfaceHDL);
        }
        else if (MFX_HW_VAAPI == m_core->GetVAType())
        {
            MFX_CHECK(surfaceHDL != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            sts = m_ddi->Execute(*pTask,  surfaceHDL);
        }
        MFX_CHECK_STS(sts); 

        sts = CopyMBData ((TaskHybridDDI *)pTask, m_core);
        MFX_CHECK_STS(sts);

        sts = m_CModel.RunBSP(pTask->m_MBData,pTask->m_frameOrder==0, pTask->m_pBitsteam);
        MFX_CHECK_STS(sts);
        
        sts = pTask->CompleteTask();
        MFX_CHECK_STS(sts);

        sts = m_CModel.FinishFrame();
        MFX_CHECK_STS(sts);

        return sts;
     } 

}

#endif // #if defined(MFX_ENABLE_VP8_VIDEO_ENCODE)
/* EOF */
