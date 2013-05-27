/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE) && defined(MFX_VA)

#ifndef _MFX_VP8_ENCODE_HYBRID_HW_H_
#define _MFX_VP8_ENCODE_HYBRID_HW_H_

//#define HYBRID_ENCODE /*HW ENC + SW PAK + SW BSP*/

#include <vector>
#include <memory>

#include "mfx_vp8_encode_utils_hw.h"
#include "cmodel_vp8.h"
#include "mfx_vp8_encode_ddi_hw.h"

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }


namespace MFX_VP8ENC
{
    typedef union { /* MV type */
        struct {
            short  mvx;
            short  mvy;
        } MV;
        int s32;
    } MvTypeVp8;

    struct sMBData
    {
        union {
            unsigned int     DWord0;
            struct {
                unsigned int  RefFrameMode       : 2;    /* 0 - intra, 1 - last, 2 - gold, 3 - altref */
                unsigned int  SegmentId          : 2;    /* Segment Id */
                unsigned int  CoeffSkip          : 1;    /* Is all coefficient are zero */
                unsigned int  MbXcnt             : 10;   /* MB column */
                unsigned int  MbYcnt             : 10;   /* MB row */
                unsigned int  MbMode             : 3;    /* Intra / Inter MB mode 0 - 4, depends on RefFrameMode */
                unsigned int  MbSubmode          : 2;    /* Intra UV or Inter partition, depends on RefFrameMode */
                unsigned int  Y2AboveNotZero     : 1;    /* Non-zero Y2 block for this or next above Mb with Y2 */
                unsigned int  Y2LeftNotZero      : 1;    /* Non-zero Y2 block for this or next left Mb with Y2 */
            };
        };
        union {
            unsigned int     DWord1;
            struct {
                unsigned short     Y4x4Modes0;             /* IntraY4x4, modes for subblocks 0 - 3 (4 x 4bits)*/
                unsigned short     Y4x4Modes1;             /* IntraY4x4, modes for subblocks 4 - 7 (4 x 4bits)*/
            };
            union {
                unsigned int InterSplitModes;        /* Inter split subblock modes (16 x 2 bits) */
            };
        };
        union {
            struct {
                unsigned short     Y4x4Modes2;             /* IntraY4x4, modes for subblocks 8 - 11 (4 x 4bits)*/
                unsigned short     Y4x4Modes3;             /* IntraY4x4, modes for subblocks 12 - 15 (4 x 4bits)*/
            };
            struct {
                MvTypeVp8   NewMV4x4[16];       /* New inter MVs (un-packed form) */
            };
            struct {
                MvTypeVp8   NewMV16;            /* VP8_MV_NEW vector placeholder */
                short  Y2mbCoeff[16];      /* Y2 Mb coefficients  */
            };
        };
        short mbCoeff[24][16];             /* YUV Mb coefficients */
    };
    class TaskHybrid : public Task
    {
    public:
        std::vector <sMBData>      m_MBData;
    public:
        TaskHybrid():  Task() {}
        virtual ~TaskHybrid() {}
        virtual  mfxStatus Init(VideoCORE * pCore, mfxVideoParam *par)
        { 
            m_MBData.resize(CalcNumMB(*par)); 
            return Task::Init(pCore,par);
        }
        virtual mfxStatus FreeTask()
        {
            Zero(m_MBData);
            return Task::FreeTask();
        }
    };
    class TaskHybridDDI: public TaskHybrid
    {
    public:
        sFrameEx *m_pMBFrameDDI;
        sFrameEx *m_pMVFrameDDI;
        sFrameEx *m_pDistFrameDDI;

    public:
        TaskHybridDDI(): m_pMBFrameDDI (0),  TaskHybrid() {}
        virtual ~TaskHybridDDI() {}
        virtual   mfxStatus SubmitTask (sFrameEx*  pRecFrame, Task* pPrevTask, sFrameParams* pParams,  sFrameEx* pRawLocalFrame, 
                                        sFrameEx *pMBFrameDDI, sFrameEx *pMVFrameDDI,
                                        sFrameEx *pDistFrameDDI)
        {
            mfxStatus sts = Task::SubmitTask(pRecFrame,pPrevTask,pParams,pRawLocalFrame);

            MFX_CHECK(isFreeSurface(pMBFrameDDI),MFX_ERR_UNDEFINED_BEHAVIOR);
            m_pMBFrameDDI = pMBFrameDDI;

            MFX_CHECK(isFreeSurface(pMVFrameDDI),MFX_ERR_UNDEFINED_BEHAVIOR);
            m_pMVFrameDDI = pMVFrameDDI;

            MFX_CHECK(isFreeSurface(pDistFrameDDI),MFX_ERR_UNDEFINED_BEHAVIOR);
            m_pDistFrameDDI = pDistFrameDDI;

            MFX_CHECK_STS(LockSurface(m_pMBFrameDDI,m_pCore));
            MFX_CHECK_STS(LockSurface(m_pMVFrameDDI,m_pCore));
            MFX_CHECK_STS(LockSurface(m_pDistFrameDDI,m_pCore));

            return sts;
        }
        virtual  mfxStatus FreeTask()
        {
            MFX_CHECK_STS(FreeSurface(m_pMBFrameDDI,m_pCore));
            MFX_CHECK_STS(FreeSurface(m_pMVFrameDDI,m_pCore));
            MFX_CHECK_STS(FreeSurface(m_pDistFrameDDI,m_pCore));

            return TaskHybrid::FreeTask();
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

    class CModel
    {
    public:
        CModel():
            m_pDLL(0),
            m_pImpl(0),
            m_pCore(0),
            m_pMBData(0),
            m_frameNum(0)
        {}
        virtual ~CModel() {Close();}
        mfxStatus Init(const mfxVideoParam &video,CMODEL_VP8_ENCODER::eMode mode, VideoCORE  *pCore);
        mfxStatus Close();
        mfxStatus SetNextFrame(mfxFrameSurface1 * pSurface, bool bExternal, const sFrameParams& frameParams, mfxU32 frameNum);
        mfxStatus RunEnc(std::vector<sMBData> &pMBData);
        mfxStatus RunPak(std::vector<sMBData> &pMBData, mfxFrameSurface1 * pRecSurface);
        mfxStatus RunBSP(std::vector<sMBData> &pMBData, bool bAddSH, mfxBitstream *bs);
        mfxStatus FinishFrame();

    protected:
        mfxStatus InitDLL();
        mfxStatus FreeDLL();

        static mfxStatus 
            InitCModelSeqParams(const mfxVideoParam &video, CMODEL_VP8_ENCODER::SeqParams &dst);
        static mfxStatus 
            InitFrameParams(const sFrameParams &srcParams, CMODEL_VP8_ENCODER::PicParam &dstParams);
        static mfxStatus
            CopyMBData(std::vector<sMBData> &pSrc, CMODEL_VP8_ENCODER::MbCodeVp8 *pDst);
        static mfxStatus
            CopyMBData( CMODEL_VP8_ENCODER::MbCodeVp8 *pSrc, std::vector<sMBData> &pDst);
       

    private:
        VideoCORE      *m_pCore;
        HMODULE         m_pDLL;
        CMODEL_VP8_ENCODER::Implementation  *m_pImpl;   
        CMODEL_VP8_ENCODER::MbCodeVp8* m_pMBData;
        mfxU32   m_frameNum;
    
    };
    class CmodelImpl : public VideoENCODE
    {
    public:
        virtual mfxStatus Close() {return MFX_ERR_UNSUPPORTED;}

        CmodelImpl(VideoCORE * core) : m_core(core) {}

        virtual ~CmodelImpl() { }

        virtual mfxStatus Init(mfxVideoParam * par);

        virtual mfxStatus Reset(mfxVideoParam * par);

        virtual mfxStatus GetVideoParam(mfxVideoParam * par);

        virtual mfxStatus EncodeFrameCheck( mfxEncodeCtrl *ctrl,
                                    mfxFrameSurface1 *surface,
                                    mfxBitstream *bs,
                                    mfxFrameSurface1 **reordered_surface,
                                    mfxEncodeInternalParams *pInternalParams,
                                    MFX_ENTRY_POINT *pEntryPoint); 

       virtual mfxStatus GetFrameParam(mfxFrameParam *)  {return MFX_ERR_UNSUPPORTED;}
       virtual mfxStatus GetEncodeStat(mfxEncodeStat *)  {return MFX_ERR_UNSUPPORTED;}
       virtual mfxStatus EncodeFrameCheck( mfxEncodeCtrl *, 
                                           mfxFrameSurface1 *, 
                                           mfxBitstream *, 
                                           mfxFrameSurface1 **, 
                                           mfxEncodeInternalParams *) 
       {return MFX_ERR_UNSUPPORTED;}

       virtual mfxStatus EncodeFrame(mfxEncodeCtrl *, mfxEncodeInternalParams *, mfxFrameSurface1 *, mfxBitstream *)
       {return MFX_ERR_UNSUPPORTED;}
       virtual mfxStatus CancelFrame(mfxEncodeCtrl *, mfxEncodeInternalParams *, mfxFrameSurface1 *, mfxBitstream *)
       {return MFX_ERR_UNSUPPORTED;}

    protected:
        static mfxStatus   TaskRoutine(void * state,  void * param, mfxU32 threadNumber, mfxU32 callNumber);
        mfxStatus          EncodeFrame(Task *pTask);

    protected:
        VideoCORE *                 m_core;
        VP8MfxParam                 m_video;
        TaskManager<TaskHybrid>     m_taskManager;
        CModel                      m_CModel;   
    };
    class TaskManagerHybridPakDDI : public TaskManager<TaskHybridDDI>
    {
    public:
        TaskManagerHybridPakDDI(): TaskManager()    {}
        virtual ~TaskManagerHybridPakDDI() {}
    
    protected:
        InternalFrames  m_MBDataDDI;    
        InternalFrames  m_MVDataDDI;    
        InternalFrames  m_DistDataDDI;    
    
    public: 
        inline 
        mfxStatus Init( VideoCORE* pCore, mfxVideoParam *par, 
                        mfxFrameInfo* pDDIMBInfo, mfxFrameInfo* pDDIMVInfo, mfxFrameInfo* pDDIDistInfo,
                        mfxU32 numFrames)
        {
            mfxU32 num =0;
            
            MFX_CHECK_STS(TaskManager::Init(pCore,par,true));
            num =(numFrames < m_ReconFrames.Num()) ? m_ReconFrames.Num() : numFrames;
            MFX_CHECK_STS(m_MBDataDDI.Init(pCore, pDDIMBInfo,num,true));
            MFX_CHECK_STS(m_MVDataDDI.Init(pCore, pDDIMVInfo,num,true));
            MFX_CHECK_STS(m_DistDataDDI.Init(pCore, pDDIDistInfo,num,true));

            return MFX_ERR_NONE;
        }
        inline 
        mfxStatus Reset(mfxVideoParam *par)
        {
            return TaskManager::Reset(par);
        }
        inline
        mfxStatus InitTask(mfxFrameSurface1* pSurface, mfxBitstream* pBitstream, Task* & pOutTask)
        {
            return TaskManager::InitTask(pSurface,pBitstream,pOutTask);
        }
        inline 
        mfxStatus SubmitTask(Task*  pTask, sFrameParams *pParams)
        {
            sFrameEx* pRecFrame = 0;
            sFrameEx* pRawLocalFrame = 0;
            sFrameEx* pDDIMBFrame = 0;
            sFrameEx* pDDIMVFrame = 0;
            sFrameEx* pDDIDistFrame = 0;

            mfxStatus sts = MFX_ERR_NONE;

            MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);

            pRecFrame = m_ReconFrames.GetFreeFrame();
            MFX_CHECK(pRecFrame!=0,MFX_WRN_DEVICE_BUSY);

            if (m_bHWFrames != m_bHWInput)
            {
                pRawLocalFrame = m_LocalRawFrames.GetFreeFrame();
                MFX_CHECK(pRawLocalFrame!= 0,MFX_WRN_DEVICE_BUSY);
            } 

            sts = m_MBDataDDI.GetFrame(pRecFrame->idInPool,pDDIMBFrame);
            MFX_CHECK_STS(sts);
            sts = m_MVDataDDI.GetFrame(pRecFrame->idInPool,pDDIMVFrame);
            MFX_CHECK_STS(sts);
            sts = m_DistDataDDI.GetFrame(pRecFrame->idInPool,pDDIDistFrame);
            MFX_CHECK_STS(sts);

            sts = ((TaskHybridDDI*)pTask)->SubmitTask(pRecFrame,m_pPrevSubmittedTask,pParams, pRawLocalFrame,pDDIMBFrame,pDDIMVFrame,pDDIDistFrame);
            MFX_CHECK_STS(sts);

            if (m_pPrevSubmittedTask)
            {
                sts = m_pPrevSubmittedTask->FreeTask();
                MFX_CHECK_STS(sts);
            }

            m_pPrevSubmittedTask = pTask;
            return sts;        
        }
        inline MfxFrameAllocResponse& GetRecFramesForReg()
        {
            return m_ReconFrames.GetFrameAllocReponse();
        }
        inline MfxFrameAllocResponse& GetMBFramesForReg()
        {
            return m_MBDataDDI.GetFrameAllocReponse();
        }
        inline MfxFrameAllocResponse& GetMVFramesForReg()
        {
            return m_MVDataDDI.GetFrameAllocReponse();
        }        
        inline MfxFrameAllocResponse& GetDistFramesForReg()
        {
            return m_DistDataDDI.GetFrameAllocReponse();
        }
    };
    class HybridPakDDIImpl : public VideoENCODE
    {
    public:
 
        virtual mfxStatus Close() {return MFX_ERR_UNSUPPORTED;}

        HybridPakDDIImpl(VideoCORE * core) : m_core(core) {}

        virtual ~HybridPakDDIImpl() { }

        virtual mfxStatus Init(mfxVideoParam * par);

        virtual mfxStatus Reset(mfxVideoParam * par);

        virtual mfxStatus GetVideoParam(mfxVideoParam * par);

        virtual mfxStatus EncodeFrameCheck( mfxEncodeCtrl *ctrl,
                                    mfxFrameSurface1 *surface,
                                    mfxBitstream *bs,
                                    mfxFrameSurface1 **reordered_surface,
                                    mfxEncodeInternalParams *pInternalParams,
                                    MFX_ENTRY_POINT *pEntryPoint); 

       virtual mfxStatus GetFrameParam(mfxFrameParam *)  {return MFX_ERR_UNSUPPORTED;}
       virtual mfxStatus GetEncodeStat(mfxEncodeStat *)  {return MFX_ERR_UNSUPPORTED;}
       virtual mfxStatus EncodeFrameCheck( mfxEncodeCtrl *, 
                                           mfxFrameSurface1 *, 
                                           mfxBitstream *, 
                                           mfxFrameSurface1 **, 
                                           mfxEncodeInternalParams *) 
       {return MFX_ERR_UNSUPPORTED;}

       virtual mfxStatus EncodeFrame(mfxEncodeCtrl *, mfxEncodeInternalParams *, mfxFrameSurface1 *, mfxBitstream *)
       {return MFX_ERR_UNSUPPORTED;}
       virtual mfxStatus CancelFrame(mfxEncodeCtrl *, mfxEncodeInternalParams *, mfxFrameSurface1 *, mfxBitstream *)
       {return MFX_ERR_UNSUPPORTED;}

    protected:
        static mfxStatus   TaskRoutine(void * state,  void * param, mfxU32 threadNumber, mfxU32 callNumber);
        mfxStatus          EncodeFrame(Task *pTask);

    protected:
        VideoCORE *                     m_core;
        VP8MfxParam                     m_video;
        TaskManagerHybridPakDDI         m_taskManager;
        CModel                          m_CModel; 
        std::auto_ptr <DriverEncoder>   m_ddi;
    };
}; 
#endif 
#endif 
/* EOF */
