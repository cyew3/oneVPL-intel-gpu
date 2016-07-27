/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "mfx_common.h"
#include "mfx_ext_buffers.h"
#include "mfx_trace.h"
#include "umc_mutex.h"
#include <vector>
#include <memory>
#include "mfxstructures.h"
#include "mfx_enc_common.h"
#include "assert.h"

#if defined (AS_VP9E_PLUGIN)

//#define VP9_LOGGING
#ifdef VP9_LOGGING
#define VP9_LOG(string, ...) printf(string, ##__VA_ARGS__); fflush(0);
#else
#define VP9_LOG(string, ...)
#endif

namespace MfxHwVP9Encode
{

#define ALIGN(x, align) (((x) + (align) - 1) & (~((align) - 1)))
#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }


#define DPB_SIZE 8
#define MAX_SEGMENTS 8

enum
{
    MFX_MEMTYPE_D3D_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME,
    MFX_MEMTYPE_D3D_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME,
    MFX_MEMTYPE_SYS_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY        | MFX_MEMTYPE_EXTERNAL_FRAME,
    MFX_MEMTYPE_SYS_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY        | MFX_MEMTYPE_INTERNAL_FRAME
};

static const mfxU16 MFX_IOPATTERN_IN_MASK_SYS_OR_D3D =
    MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY;

static const mfxU16 MFX_IOPATTERN_IN_MASK =
    MFX_IOPATTERN_IN_MASK_SYS_OR_D3D | MFX_IOPATTERN_IN_OPAQUE_MEMORY;


    enum eTaskStatus
    {
        TASK_FREE =0,
        TASK_INITIALIZED,
        TASK_SUBMITTED,
        READY
    };
    enum eRefFrames
    {
        REF_LAST  = 0,
        REF_GOLD  = 1,
        REF_ALT   = 2,
        REF_TOTAL = 3
    };

    typedef struct _sFrameParams
    {
        mfxU8    bIntra;
        mfxU8    Sharpness; //[0,7]

        mfxU8    LFLevel;
        mfxI8   LFRefDelta[4];
        mfxI8   LFModeDelta[2];
        mfxU8    QIndex;
        mfxI8   QIndexDeltaLumaDC;
        mfxI8   QIndexDeltaChromaAC;
        mfxI8   QIndexDeltaChromaDC;

        mfxU8    NumSegments;
        mfxI8    QIndexDeltaSeg[MAX_SEGMENTS];
        mfxI8    LFDeltaSeg[MAX_SEGMENTS];

        mfxU8    refList[REF_TOTAL]; // indexes of last, gold, alt refs for current frame
        mfxU8    refreshRefFrames[DPB_SIZE]; // which reference frames are refreshed with current reconstructed frame

    } sFrameParams;

    struct sFrameEx
    {
        mfxFrameSurface1 *pSurface;
        mfxU32            idInPool;
        mfxU8             QP;
        mfxU8             refCount;
    };

    inline mfxStatus LockSurface(sFrameEx*  pFrame, mfxCoreInterface* pCore)
    {
        //printf("LockSurface: ");
        //if (pFrame) printf("%d\n",pFrame->idInPool); else printf("\n");
        return (pFrame) ? pCore->IncreaseReference(pCore->pthis, &pFrame->pSurface->Data) : MFX_ERR_NONE;
    }
    inline mfxStatus FreeSurface(sFrameEx* &pFrame, mfxCoreInterface* pCore)
    {
        mfxStatus sts = MFX_ERR_NONE;
        //printf("FreeSurface\n");
        //if (pFrame) printf("%d\n",pFrame->idInPool); else printf("\n");
        if (pFrame && pFrame->pSurface)
        {
            //printf("DecreaseReference\n");
            sts = pCore->DecreaseReference(pCore->pthis, &pFrame->pSurface->Data);
            pFrame = 0;
        }
        return sts;
    }
    inline bool isFreeSurface (sFrameEx* pFrame)
    {
        //printf("isFreeSurface %d (%d)\n", pFrame->pSurface->Data.Locked, pFrame->idInPool);
        return (pFrame->pSurface->Data.Locked == 0);
    }

    /*------------------------ Utils------------------------------------------------------------------------*/

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }
#define STATIC_ASSERT(ASSERTION, MESSAGE) char MESSAGE[(ASSERTION) ? 1 : -1]; MESSAGE

    template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }
    template<class T> inline void Zero(std::vector<T> & vec)   { memset(&vec[0], 0, sizeof(T) * vec.size()); }

    /* copied from H.264 */
    //-----------------------------------------------------
    // Helper which checks number of allocated frames and auto-free.
    //-----------------------------------------------------
    template<class T> struct ExtBufTypeToId {};

#define BIND_EXTBUF_TYPE_TO_ID(TYPE, ID) template<> struct ExtBufTypeToId<TYPE> { enum { id = ID }; }
    BIND_EXTBUF_TYPE_TO_ID (mfxExtCodingOptionVP9,  MFX_EXTBUFF_CODING_OPTION_VP9 );
    BIND_EXTBUF_TYPE_TO_ID (mfxExtOpaqueSurfaceAlloc,MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
    BIND_EXTBUF_TYPE_TO_ID (mfxExtEncoderROI,MFX_EXTBUFF_ENCODER_ROI);
#undef BIND_EXTBUF_TYPE_TO_ID

    template <class T> inline void InitExtBufHeader(T & extBuf)
    {
        Zero(extBuf);
        extBuf.Header.BufferId = ExtBufTypeToId<T>::id;
        extBuf.Header.BufferSz = sizeof(T);
    }

    template <> inline void InitExtBufHeader<mfxExtCodingOptionVP9>(mfxExtCodingOptionVP9 & extBuf)
    {
        Zero(extBuf);
        extBuf.Header.BufferId = ExtBufTypeToId<mfxExtCodingOptionVP9>::id;
        extBuf.Header.BufferSz = sizeof(mfxExtCodingOptionVP9);

        // use defaults as in C-model
        /*extBuf.LoopFilterRefDelta[0] = 1;
        extBuf.LoopFilterRefDelta[1] = 0;
        extBuf.LoopFilterRefDelta[2] = -1;
        extBuf.LoopFilterRefDelta[3] = -1;*/
    }


    // temporal application of nonzero defaults to test ROI for VP9
    // TODO: remove this when testing os finished
    template <> inline void InitExtBufHeader<mfxExtEncoderROI>(mfxExtEncoderROI & extBuf)
    {
        Zero(extBuf);
        extBuf.Header.BufferId = ExtBufTypeToId<mfxExtEncoderROI>::id;
        extBuf.Header.BufferSz = sizeof(mfxExtEncoderROI);

        // no default ROI
        /*extBuf.NumROI = 2;
        extBuf.ROI[0].Left     = 16;
        extBuf.ROI[0].Right    = 128;
        extBuf.ROI[0].Top      = 16;
        extBuf.ROI[0].Bottom   = 128;
        extBuf.ROI[0].Priority = 0;

        extBuf.ROI[1].Left     = 64;
        extBuf.ROI[1].Right    = 256;
        extBuf.ROI[1].Top      = 64;
        extBuf.ROI[1].Bottom   = 256;
        extBuf.ROI[1].Priority = 0;*/
    }

    template <class T> struct GetPointedType {};
    template <class T> struct GetPointedType<T *> { typedef T Type; };
    template <class T> struct GetPointedType<T const *> { typedef T Type; };

    struct mfxExtBufferProxy
    {
    public:
        template <typename T> operator T()
        {
            mfxExtBuffer * p = GetExtBuffer(
                m_extParam,
                m_numExtParam,
                ExtBufTypeToId<typename GetPointedType<T>::Type>::id);
            return reinterpret_cast<T>(p);
        }

        template <typename T> friend mfxExtBufferProxy GetExtBuffer(const T & par);

    protected:
        mfxExtBufferProxy(mfxExtBuffer ** extParam, mfxU32 numExtParam)
            : m_extParam(extParam)
            , m_numExtParam(numExtParam)
        {
        }

    private:
        mfxExtBuffer ** m_extParam;
        mfxU32          m_numExtParam;
    };

    template <typename T> mfxExtBufferProxy GetExtBuffer(T const & par)
    {
        return mfxExtBufferProxy(par.ExtParam, par.NumExtParam);
    }

    class MfxFrameAllocResponse : public mfxFrameAllocResponse
    {
    public:
        MfxFrameAllocResponse();

        ~MfxFrameAllocResponse();

        mfxStatus Alloc(
            mfxCoreInterface * pCore,
            mfxFrameAllocRequest & req);

        mfxFrameInfo               m_info;

    private:
        MfxFrameAllocResponse(MfxFrameAllocResponse const &);
        MfxFrameAllocResponse & operator =(MfxFrameAllocResponse const &);

        mfxCoreInterface * m_pCore;
        mfxU16      m_numFrameActualReturnedByAllocFrames;

        std::vector<mfxFrameAllocResponse> m_responseQueue;
        std::vector<mfxMemId>              m_mids;
    };

    struct FrameLocker
    {
        FrameLocker(mfxCoreInterface * pCore, mfxFrameData & data)
            : m_core(pCore)
            , m_data(data)
            , m_memId(data.MemId)
            , m_status(Lock())
        {
        }

        FrameLocker(mfxCoreInterface * pCore, mfxFrameData & data, mfxMemId memId)
            : m_core(pCore)
            , m_data(data)
            , m_memId(memId)
            , m_status(Lock())
        {
        }

        ~FrameLocker() { Unlock(); }

        mfxStatus Unlock()
        {
            mfxStatus mfxSts = MFX_ERR_NONE;
            mfxSts = m_core->FrameAllocator.Unlock(m_core->FrameAllocator.pthis, m_memId, &m_data);
            m_status = LOCK_NO;
            return mfxSts;
        }

    protected:
        enum { LOCK_NO, LOCK_DONE };

        mfxU32 Lock()
        {
            mfxU32 status = LOCK_NO;

            if (m_data.Y == 0 &&
                MFX_ERR_NONE == m_core->FrameAllocator.Lock(m_core->FrameAllocator.pthis, m_memId, &m_data))
                status = LOCK_DONE;

            return status;
        }

    private:
        FrameLocker(FrameLocker const &);
        FrameLocker & operator =(FrameLocker const &);

        mfxCoreInterface * m_core;
        mfxFrameData &     m_data;
        mfxMemId           m_memId;
        mfxU32             m_status;
    };

    class VP9MfxParam : public mfxVideoParam
    {
    public:
        VP9MfxParam();
        VP9MfxParam(VP9MfxParam const &);
        VP9MfxParam(mfxVideoParam const &);

        VP9MfxParam & operator = (VP9MfxParam const &);
        VP9MfxParam & operator = (mfxVideoParam const &);


    protected:
        void Construct(mfxVideoParam const & par);


    private:
        mfxExtBuffer *              m_extParam[3];
        mfxExtCodingOptionVP9       m_extOpt;
        mfxExtOpaqueSurfaceAlloc    m_extOpaque;
        mfxExtEncoderROI            m_extROI;
    };

    mfxStatus GetVideoParam(mfxVideoParam * parDst, mfxVideoParam *videoSrc);

    mfxStatus SetFramesParams(VP9MfxParam const &par,
                              mfxU16 forcedFrameType,
                              mfxU32 frameOrder,
                              sFrameParams *pFrameParams);

    class Task;
    mfxStatus DecideOnRefListAndDPBRefresh(mfxVideoParam * par,
                                           Task *pTask,
                                           std::vector<sFrameEx*>&dpb,
                                           sFrameParams *pFrameParams);

    class ExternalFrames
    {
    protected:
        std::vector<sFrameEx>  m_frames;
    public:
        ExternalFrames() {}
        void Init();
        mfxStatus GetFrame(mfxFrameSurface1 *pInFrame, sFrameEx *&pOutFrame);
     };

    class InternalFrames
    {
    protected:
        std::vector<sFrameEx>           m_frames;
        MfxFrameAllocResponse           m_response;
        std::vector<mfxFrameSurface1>   m_surfaces;
    public:
        InternalFrames() {}
        mfxStatus Init(mfxCoreInterface *pCore, mfxFrameAllocRequest *pAllocReq, bool bHW);
        sFrameEx * GetFreeFrame();
        mfxStatus  GetFrame(mfxU32 numFrame, sFrameEx * &Frame);

        inline mfxU16 Height()
        {
            return m_response.m_info.Height;
        }
        inline mfxU16 Width()
        {
            return m_response.m_info.Width;
        }
        inline mfxU32 Num()
        {
            return m_response.NumFrameActual;
        }
        inline MfxFrameAllocResponse& GetFrameAllocReponse()  {return m_response;}
    };

    bool isVideoSurfInput(mfxVideoParam const & video);

    inline bool isOpaq(mfxVideoParam const & video)
    {
        return (video.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)!=0;
    }

    inline mfxU32 CalcNumTasks(mfxVideoParam const & video)
    {
        return video.AsyncDepth;
    }

    inline mfxU32 CalcNumSurfRawLocal(mfxVideoParam const & video, bool bHWImpl, bool bHWInput,bool bRawReference = false)
    {
        return (bHWInput !=  bHWImpl) ? CalcNumTasks(video) + ( bRawReference ? 3 : 0 ):0;
    }

    inline mfxU32 CalcNumSurfRecon(mfxVideoParam const & video)
    {
        return /*3*/8 + CalcNumTasks(video); // only 3 reference frames are stored in DPB for now. Could be up to 8
    }
    inline mfxU32 CalcNumMB(mfxVideoParam const & video)
    {
        return (video.mfx.FrameInfo.Width>>4)*(video.mfx.FrameInfo.Height>>4);
    }

    class Task
    {
    public:

        eTaskStatus       m_status;
        sFrameEx*         m_pRawFrame;
        sFrameEx*         m_pRawLocalFrame;
        mfxBitstream*     m_pBitsteam;
        mfxCoreInterface* m_pCore;

        sFrameParams      m_sFrameParams;
        sFrameEx*         m_pRecFrame;
        sFrameEx*         m_pRecRefFrames[3];
        sFrameEx*         m_pOutBs;
        bool              m_bOpaqInput;
        mfxU32            m_frameOrder;
        mfxU64            m_timeStamp;

        mfxEncodeCtrl     m_ctrl;

        mfxU64 m_frameOrderOfPreviousFrame;
        mfxU32 m_bsDataLength;

    protected:

        mfxStatus CopyInput();

    public:

        Task ():
          m_status(TASK_FREE),
              m_pRawFrame(0),
              m_pRawLocalFrame(0),
              m_pBitsteam(0),
              m_pCore(0),
              m_pRecFrame(0),
              m_pOutBs(0),
              m_bOpaqInput(false),
              m_frameOrder (0),
              m_frameOrderOfPreviousFrame(0),
              m_bsDataLength(0)
          {
              Zero(m_pRecRefFrames);
              Zero(m_sFrameParams);
              Zero(m_ctrl);
          }
          virtual
          ~Task ()
          {
              FreeTask();
          }
          mfxStatus GetOriginalSurface(mfxFrameSurface1 *& pSurface, bool &bExternal);
          mfxStatus GetInputSurface(mfxFrameSurface1 *& pSurface, bool &bExternal);

          virtual
          mfxStatus Init(mfxCoreInterface * pCore, mfxVideoParam *par);

          virtual
          mfxStatus InitTask( sFrameEx     *pRawFrame,
                              mfxBitstream *pBitsteam,
                              mfxU32        frameOrder);
          virtual
          mfxStatus SubmitTask (sFrameEx*  pRecFrame, std::vector<sFrameEx*> &dpb, sFrameParams* pParams, sFrameEx* pRawLocalFrame, sFrameEx* pOutBs);

          inline mfxStatus CompleteTask ()
          {
              m_status = READY;
              return MFX_ERR_NONE;
          }
          inline bool isReady()
          {
            return m_status == READY;
          }

          virtual
          mfxStatus FreeTask();
          mfxStatus FreeDPBSurfaces();

          inline bool isFreeTask()
          {
              return (m_status == TASK_FREE);
          }
    };

    template <class TTask>
    inline TTask* GetFreeTask(std::vector<TTask> & tasks)
    {
        typename std::vector<TTask>::iterator task = tasks.begin();
        for (;task != tasks.end(); task ++)
        {
            if (task->isFreeTask())
            {
                return &task[0];
            }
        }
        return 0;
    }

    template <class TTask>
    inline mfxStatus FreeTasks(std::vector<TTask> & tasks)
    {
        mfxStatus sts = MFX_ERR_NONE;
        typename std::vector<TTask>::iterator task = tasks.begin();
        for (;task != tasks.end(); task ++)
        {
            if (!task->isFreeTask())
            {
                sts = task->FreeTask();
                MFX_CHECK_STS(sts);
            }
        }
        return sts;
    }

    inline bool IsResetOfPipelineRequired(const mfxVideoParam& oldPar, const mfxVideoParam& newPar)
    {
        return oldPar.mfx.FrameInfo.Width != newPar.mfx.FrameInfo.Width
              || oldPar.mfx.FrameInfo.Height != newPar.mfx.FrameInfo.Height
              ||  oldPar.mfx.GopPicSize != newPar.mfx.GopPicSize;
    }

    /*for Full Encode (SW or HW)*/
    template <class TTask>
    class TaskManager
    {
    protected:

        mfxCoreInterface*       m_pCore;
        VP9MfxParam      m_video;

        bool    m_bHWFrames;
        bool    m_bHWInput;

        ExternalFrames  m_RawFrames;
        InternalFrames  m_LocalRawFrames;
        InternalFrames  m_ReconFrames;

        std::vector<TTask>  m_Tasks;
        std::vector<sFrameEx*> m_dpb;

        mfxU32              m_frameNum;


    public:
        TaskManager ():
          m_pCore(0),
          m_bHWFrames(false),
          m_bHWInput(false),
          m_frameNum(0)
        {
            memset(&m_dpb, 0, sizeof(m_dpb));
        }

          ~TaskManager()
          {
              FreeTasks(m_Tasks);
          }

          mfxStatus Init(mfxCoreInterface* pCore, mfxVideoParam *par, bool bHWImpl, mfxU32 reconFourCC)
          {
              mfxStatus sts = MFX_ERR_NONE;

              MFX_CHECK(!m_pCore, MFX_ERR_UNDEFINED_BEHAVIOR);

              m_pCore   = pCore;
              m_video   = *par;
              m_bHWFrames = bHWImpl;
              m_bHWInput = isVideoSurfInput(m_video);

              m_frameNum = 0;

              m_RawFrames.Init();

              mfxFrameAllocRequest request     = {};
              request.Info = m_video.mfx.FrameInfo;
              request.Info.FourCC = MFX_FOURCC_NV12; // only NV12 is supported as driver input
              request.NumFrameMin = request.NumFrameSuggested = (mfxU16)CalcNumSurfRawLocal(m_video, bHWImpl, m_bHWInput);

              sts = m_LocalRawFrames.Init(pCore, &request, m_bHWFrames);
              MFX_CHECK_STS(sts);

              request.NumFrameMin = request.NumFrameSuggested = (mfxU16)CalcNumSurfRecon(m_video);
              request.Info.FourCC = reconFourCC;

              sts = m_ReconFrames.Init(pCore, &request, m_bHWFrames);
              MFX_CHECK_STS(sts);

              m_dpb.resize(request.NumFrameMin - par->AsyncDepth);

              {
                  mfxU32 numTasks = CalcNumTasks(m_video);
                  m_Tasks.resize(numTasks);

                  for (mfxU32 i = 0; i < numTasks; i++)
                  {
                      sts = m_Tasks[i].Init(m_pCore,&m_video);
                      MFX_CHECK_STS(sts);
                  }
              }

              return sts;
          }

          mfxStatus ReleaseDpbFrames()
          {
              for (mfxU8 refIdx = 0; refIdx < m_dpb.size(); refIdx ++)
              {
                  if (m_dpb[refIdx])
                  {
                      m_dpb[refIdx]->refCount = 0;
                      MFX_CHECK_STS(FreeSurface(m_dpb[refIdx],m_pCore));
                  }
              }

              return MFX_ERR_NONE;
          }

          mfxStatus Reset(mfxVideoParam *par)
          {
              mfxStatus sts = MFX_ERR_NONE;
              MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);

              MFX_CHECK(m_ReconFrames.Height() >= par->mfx.FrameInfo.Height,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
              MFX_CHECK(m_ReconFrames.Width()  >= par->mfx.FrameInfo.Width,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

              MFX_CHECK(mfxU16(CalcNumSurfRawLocal(*par,m_bHWFrames,isVideoSurfInput(*par))) <= m_LocalRawFrames.Num(),MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
              MFX_CHECK(mfxU16(CalcNumSurfRecon(*par)) <= m_ReconFrames.Num(),MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

              m_video = *par;

              if (IsResetOfPipelineRequired(m_video, *par))
              {
                  m_frameNum = 0;
                  ReleaseDpbFrames();
                  memset(&m_dpb, 0, sizeof(m_dpb));
                  sts = FreeTasks(m_Tasks);
                  MFX_CHECK_STS(sts);
              }
              return MFX_ERR_NONE;
          }

          mfxStatus InitTask(mfxFrameSurface1* pSurface, mfxBitstream* pBitstream, Task* & pOutTask)
          {
              mfxStatus sts = MFX_ERR_NONE;
              Task*     pTask = GetFreeTask(m_Tasks);
              sFrameEx* pRawFrame = 0;

              //printf("Init frame\n");

              MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);
              MFX_CHECK(pTask!=0,MFX_WRN_DEVICE_BUSY);

              sts = m_RawFrames.GetFrame(pSurface, pRawFrame);
              MFX_CHECK_STS(sts);

              //printf("Init frame - 1\n");

              sts = pTask->InitTask(pRawFrame,pBitstream, m_frameNum);
              pTask->m_timeStamp =pSurface->Data.TimeStamp;
              MFX_CHECK_STS(sts);

              //printf("Init frame - End\n");

              pOutTask = pTask;

              m_frameNum++;

              return sts;
          }

          inline
          mfxStatus UpdateDpb(sFrameParams *pParams, sFrameEx *pRecFrame)
          {
              for (mfxU8 i = 0; i < m_dpb.size(); i ++)
              {
                  if (pParams->refreshRefFrames[i])
                  {
                      if (m_dpb[i])
                      {
                          m_dpb[i]->refCount--;
                          if (m_dpb[i]->refCount == 0)
                              MFX_CHECK_STS(FreeSurface(m_dpb[i], m_pCore));
                      }
                      m_dpb[i] = pRecFrame;
                      m_dpb[i]->refCount ++;
                  }
              }
              return MFX_ERR_NONE;
          }
    };

    class TaskManagerVmePlusPak : public TaskManager<Task>
    {
    public:
        typedef TaskManager<Task>  BaseClass;

        TaskManagerVmePlusPak(): BaseClass()
        {
#if 0 // segmentation support is disabled
            m_bUseSegMap = false;
#endif // segmentation support is disabled
        }
        virtual ~TaskManagerVmePlusPak() {/*printf("~TaskManagerVmePlusPak)\n");*/}

    // [SE] WA for Windows hybrid VP9 HSW driver (remove 'protected' here)
#if defined (MFX_VA_LINUX)
    protected:
#endif

        InternalFrames  m_OutputBitstreams;

#if 0 // segmentation support is disabled
        bool            m_bUseSegMap;
        InternalFrames  m_SegMapDDI_hw;
#endif // segmentation support is disabled

        mfxU64           m_frameNumOfLastArrivedFrame;
        mfxU64           m_frameNumOfLastFrameSubmittedToDriver;
        mfxU64           m_frameNumOfLastEncodedFrame;

    public:

        inline
        mfxStatus Init( mfxCoreInterface* pCore, mfxVideoParam *par, mfxU32 reconFourCC)
        {
            mfxStatus sts = BaseClass::Init(pCore,par,true,reconFourCC);
            MFX_CHECK_STS(sts);
            m_frameNumOfLastArrivedFrame = 0;
            m_frameNumOfLastFrameSubmittedToDriver = 0;
            m_frameNumOfLastEncodedFrame = 0;
            return MFX_ERR_NONE;
        }

        inline
        mfxStatus AllocInternalResources(mfxCoreInterface *pCore,
                        mfxFrameAllocRequest reqOutBs
#if 0 // segmentation support is disabled
                        mfxFrameAllocRequest reqSegMap
#endif // segmentation support is disabled
                        )
        {
            if (reqOutBs.NumFrameMin < m_ReconFrames.Num())
                reqOutBs.NumFrameMin = (mfxU16)m_ReconFrames.Num();
            MFX_CHECK_STS(m_OutputBitstreams.Init(pCore, &reqOutBs,true));

#if 0 // segmentation support is disabled
            if (reqSegMap.NumFrameMin)
            {
                m_bUseSegMap = true;
                MFX_CHECK_STS(m_SegMapDDI_hw.Init(pCore, &reqSegMap,true));
            }
#endif // segmentation support is disabled

            return MFX_ERR_NONE;
        }

        inline
        mfxStatus Reset(mfxVideoParam *par)
        {
            if (IsResetOfPipelineRequired(m_video, *par))
            {
                m_frameNumOfLastArrivedFrame = 0;
                m_frameNumOfLastFrameSubmittedToDriver = 0;
                m_frameNumOfLastEncodedFrame = 0;
            }
            return BaseClass::Reset(par);
        }
        inline
        mfxStatus InitTask(mfxFrameSurface1* pSurface, mfxBitstream* pBitstream, Task* & pOutTask)
        {
            /*if (m_frameNum >= m_maxBrcUpdateDelay && m_cachedFrameInfoFromPak.size() == 0)
            {
                // MSDK should send frame update to driver but required previous frame isn't encoded yet
                // let's wait for encoding of this frame
                return MFX_WRN_DEVICE_BUSY;
            }*/

            mfxStatus sts = BaseClass::InitTask(pSurface,pBitstream,pOutTask);

            /*Task *pHybridTask = (TaskHybridDDI*)pOutTask;
            Zero(pHybridTask->m_probs);

            if (pOutTask->m_frameOrder < m_maxBrcUpdateDelay && m_cachedFrameInfoFromPak.size() == 0)
            {
                pHybridTask->m_frameOrderOfPreviousFrame = m_frameNumOfLastArrivedFrame;
                m_frameNumOfLastArrivedFrame = m_frameNum - 1;
                // MSDK can't send frame update to driver but it's OK for initial encoding stage
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
            }*/

            pOutTask->m_frameOrderOfPreviousFrame = m_frameNumOfLastArrivedFrame;
            m_frameNumOfLastArrivedFrame = m_frameNum - 1;

            return sts;
        }

        inline
        mfxStatus SubmitTask(mfxVideoParam * par, Task*  pTask, sFrameParams *pParams)
        {
            sFrameEx* pRecFrame = 0;
            sFrameEx* pRawLocalFrame = 0;
            sFrameEx* pOutBs = 0;

            mfxStatus sts = MFX_ERR_NONE;

            MFX_CHECK(m_pCore!=0, MFX_ERR_NOT_INITIALIZED);

            pRecFrame = m_ReconFrames.GetFreeFrame();
            MFX_CHECK(pRecFrame!=0,MFX_WRN_DEVICE_BUSY);

            if (m_bHWFrames != m_bHWInput)
            {
                pRawLocalFrame = m_LocalRawFrames.GetFreeFrame();
                MFX_CHECK(pRawLocalFrame!= 0,MFX_WRN_DEVICE_BUSY);
            }
            MFX_CHECK_STS(m_OutputBitstreams.GetFrame(pRecFrame->idInPool, pOutBs));

#if 0 // segmentation support is disabled
            if (m_bUseSegMap)
            {
                MFX_CHECK_STS(m_SegMapDDI_hw.GetFrame(pRecFrame->idInPool,ddi_frames.m_pSegMap_hw));
            }
            else
                ddi_frames.m_pSegMap_hw = 0;
#endif // segmentation support is disabled

            sts = DecideOnRefListAndDPBRefresh(par, pTask, m_dpb, pParams);

            sts = pTask->SubmitTask(pRecFrame,m_dpb,
                                    pParams, pRawLocalFrame,
                                    pOutBs);
            MFX_CHECK_STS(sts);

            UpdateDpb(pParams, pRecFrame);

            return sts;
        }

        inline
        void RememberSubmittedTask(Task &task)
        {
            m_frameNumOfLastFrameSubmittedToDriver = task.m_frameOrder;
        }

        inline
        void RememberEncodedTask(Task &task)
        {
            m_frameNumOfLastEncodedFrame = task.m_frameOrder;
        }

        inline
        mfxStatus CheckHybridDependencies(Task &task)
        {
            if (task.m_frameOrder == 0)
                return MFX_ERR_NONE;

            if (task.m_status == TASK_INITIALIZED &&
                m_frameNumOfLastFrameSubmittedToDriver >= task.m_frameOrderOfPreviousFrame)
                return MFX_ERR_NONE;

            if (task.m_status == TASK_SUBMITTED &&
                m_frameNumOfLastEncodedFrame >= task.m_frameOrderOfPreviousFrame)
                return MFX_ERR_NONE;

            return MFX_WRN_IN_EXECUTION;
        }

        inline MfxFrameAllocResponse& GetRecFramesForReg()
        {
            return m_ReconFrames.GetFrameAllocReponse();
        }

        inline MfxFrameAllocResponse& GetMBDataFramesForReg()
        {
            return m_OutputBitstreams.GetFrameAllocReponse();
        }

#if 0 // segmentation support is disabled
        inline MfxFrameAllocResponse& GetSegMapFramesForReg()
        {
            return m_SegMapDDI_hw.GetFrameAllocReponse();
        }
#endif // segmentation support is disabled
    };

#define IVF_SEQ_HEADER_SIZE_BYTES 32
#define IVF_PIC_HEADER_SIZE_BYTES 12

inline bool InsertSeqHeader(Task const &task)
{
    return (task.m_frameOrder == 0); // TODO: fix condition for SH insertion
}

} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN
