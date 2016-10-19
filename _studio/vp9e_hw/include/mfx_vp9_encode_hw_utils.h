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
#define ALIGN_POWER_OF_TWO(value, n) \
    (((value)+((1 << (n)) - 1)) & ~((1 << (n)) - 1))
#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

#define DPB_SIZE 8
#define MAX_SEGMENTS 8
#define REF_FRAMES_LOG2 3
#define REF_FRAMES (1 << REF_FRAMES_LOG2)
#define MAX_REF_LF_DELTAS 4
#define MAX_MODE_LF_DELTAS 2
#define SEG_LVL_MAX 4

#define IVF_SEQ_HEADER_SIZE_BYTES 32
#define IVF_PIC_HEADER_SIZE_BYTES 12
#define MAX_IVF_HEADER_SIZE IVF_SEQ_HEADER_SIZE_BYTES + IVF_PIC_HEADER_SIZE_BYTES

#define MAX_Q_INDEX 255
#define MAX_LF_LEVEL 63

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
    enum
    {
        REF_LAST  = 0,
        REF_GOLD  = 1,
        REF_ALT   = 2,
        REF_TOTAL = 3
    };

    enum {
        KEY_FRAME       = 0,
        INTER_FRAME     = 1,
        NUM_FRAME_TYPES
    };

    enum {
        UNKNOWN_COLOR_SPACE = 0,
        BT_601              = 1,
        BT_709              = 2,
        SMPTE_170           = 3,
        SMPTE_240           = 4,
        BT_2020             = 5,
        RESERVED            = 6,
        SRGB                = 7
    };

    enum {
        BITDEPTH_8 = 8,
        BITDEPTH_10 = 10,
        BITDEPTH_12 = 12
    };

    enum {
        PROFILE_0 = 0,
        PROFILE_1 = 1,
        PROFILE_2 = 2,
        PROFILE_3 = 3,
        MAX_PROFILES
    };

    struct VP9SeqLevelParam
    {
        mfxU8  profile;
        mfxU8  bitDepth;;
        mfxU8  colorSpace;
        mfxU8  colorRange;
        mfxU8  subsamplingX;
        mfxU8  subsamplingY;

        mfxU32 initialWidth;
        mfxU32 initialHeight;

        mfxU8  frameParallelDecoding;
    };

    struct VP9FrameLevelParam
    {
        mfxU8  frameType;
        mfxU8  baseQIndex;
        mfxI8  qIndexDeltaLumaDC;
        mfxI8  qIndexDeltaChromaAC;
        mfxI8  qIndexDeltaChromaDC;

        mfxU32 width;
        mfxU32 height;
        mfxU32 renderWidth;
        mfxU32 renderHeight;

        mfxU8  refreshFrameContext;
        mfxU8  resetFrameContext;
        mfxU8  refList[REF_TOTAL]; // indexes of last, gold, alt refs for current frame
        mfxU8  refBiases[REF_TOTAL];
        mfxU8  refreshRefFrames[DPB_SIZE]; // which reference frames are refreshed with current reconstructed frame
        mfxU16 modeInfoCols;
        mfxU16 modeInfoRows;
        mfxU8  allowHighPrecisionMV;

        mfxU8  showFarme;
        mfxU8  intraOnly;

        mfxU8  lfLevel;
        mfxI8  lfRefDelta[4];
        mfxI8  lfModeDelta[2];
        mfxU8  modeRefDeltaEnabled;
        mfxU8  modeRefDeltaUpdate;

        mfxU8  sharpness;
        mfxU8  numSegments;
        mfxU8  errorResilentMode;
        mfxU8  interpFilter;
        mfxU8  frameContextIdx;
        mfxU8  log2TileCols;
        mfxU8  log2TileRows;
    };

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

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }
#define STATIC_ASSERT(ASSERTION, MESSAGE) char MESSAGE[(ASSERTION) ? 1 : -1]; MESSAGE

    template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }
    template<class T> inline void Zero(std::vector<T> & vec)   { memset(&vec[0], 0, sizeof(T) * vec.size()); }
    template<class T> inline void ZeroExtBuffer(T & obj)
    {
        mfxExtBuffer header = obj.Header;
        memset(&obj, 0, sizeof(obj));
        obj.Header = header;
    }
    template<class T, class U> inline void Copy(T & dst, U const & src)
    {
        STATIC_ASSERT(sizeof(T) == sizeof(U), copy_objects_of_different_size);
        memcpy_s(&dst, sizeof(dst), &src, sizeof(dst));
    }

    inline bool IsOn(mfxU16 opt)
    {
        return opt == MFX_CODINGOPTION_ON;
    }

    inline bool IsOff(mfxU16 opt)
    {
        return opt == MFX_CODINGOPTION_OFF;
    }

    template<class T> struct ExtBufTypeToId {};

#define BIND_EXTBUF_TYPE_TO_ID(TYPE, ID) template<> struct ExtBufTypeToId<TYPE> { enum { id = ID }; }
    BIND_EXTBUF_TYPE_TO_ID (mfxExtCodingOptionVP9,  MFX_EXTBUFF_CODING_OPTION_VP9 );
    BIND_EXTBUF_TYPE_TO_ID (mfxExtOpaqueSurfaceAlloc,MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
#undef BIND_EXTBUF_TYPE_TO_ID

    template <class T> inline void InitExtBufHeader(T & extBuf)
    {
        Zero(extBuf);
        extBuf.Header.BufferId = ExtBufTypeToId<T>::id;
        extBuf.Header.BufferSz = sizeof(T);
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

struct mfxExtBufferRefProxy{
public:
    template <typename T> operator T&()
    {
        mfxExtBuffer * p = GetExtBuffer(
            m_extParam,
            m_numExtParam,
            ExtBufTypeToId<typename GetPointedType<T*>::Type>::id);
        assert(p);
        return *(reinterpret_cast<T*>(p));
    }

    template <typename T> friend mfxExtBufferRefProxy GetExtBufferRef(const T & par);

protected:
    mfxExtBufferRefProxy(mfxExtBuffer ** extParam, mfxU32 numExtParam)
        : m_extParam(extParam)
        , m_numExtParam(numExtParam)
    {
    }
private:
    mfxExtBuffer ** m_extParam;
    mfxU32          m_numExtParam;
};

template <typename T> mfxExtBufferRefProxy GetExtBufferRef(T const & par)
{
    return mfxExtBufferRefProxy(par.ExtParam, par.NumExtParam);
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

#define NUM_OF_SUPPORTED_EXT_BUFFERS 2

    class VP9MfxVideoParam : public mfxVideoParam
    {
    public:
        VP9MfxVideoParam();
        VP9MfxVideoParam(VP9MfxVideoParam const &);
        VP9MfxVideoParam(mfxVideoParam const &);

        VP9MfxVideoParam & operator = (VP9MfxVideoParam const &);
        VP9MfxVideoParam & operator = (mfxVideoParam const &);


    protected:
        void Construct(mfxVideoParam const & par);

    private:
        mfxExtBuffer *              m_extParam[NUM_OF_SUPPORTED_EXT_BUFFERS];
        mfxExtCodingOptionVP9       m_extOpt;
        mfxExtOpaqueSurfaceAlloc    m_extOpaque;
    };

    mfxStatus GetVideoParam(mfxVideoParam * parDst, mfxVideoParam *videoSrc);

    mfxStatus SetFramesParams(VP9MfxVideoParam const &par,
                              mfxU16 forcedFrameType,
                              mfxU32 frameOrder,
                              VP9FrameLevelParam &frameParam);

    mfxStatus InitVp9SeqLevelParam(VP9MfxVideoParam const &video, VP9SeqLevelParam &param);

    class Task;
    mfxStatus DecideOnRefListAndDPBRefresh(mfxVideoParam * par,
                                           Task *pTask,
                                           std::vector<sFrameEx*>&dpb,
                                           VP9FrameLevelParam &frameParam);

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
        return video.mfx.NumRefFrame + CalcNumTasks(video);
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

        VP9FrameLevelParam m_frameParam;
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
              m_pRawFrame(NULL),
              m_pRawLocalFrame(NULL),
              m_pBitsteam(0),
              m_pCore(NULL),
              m_pRecFrame(NULL),
              m_pOutBs(NULL),
              m_bOpaqInput(false),
              m_frameOrder(0),
              m_timeStamp(0),
              m_frameOrderOfPreviousFrame(0),
              m_bsDataLength(0)
          {
              Zero(m_pRecRefFrames);
              Zero(m_frameParam);
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
          mfxStatus SubmitTask(sFrameEx*  pRecFrame, std::vector<sFrameEx*> &dpb, VP9FrameLevelParam &frameParam, sFrameEx* pRawLocalFrame, sFrameEx* pOutBs);

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
        VP9MfxVideoParam      m_video;

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
          mfxStatus UpdateDpb(VP9FrameLevelParam &frameParam, sFrameEx *pRecFrame)
          {
              for (mfxU8 i = 0; i < m_dpb.size(); i ++)
              {
                  if (frameParam.refreshRefFrames[i])
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

        TaskManagerVmePlusPak()
            : BaseClass()
#if 0 // segmentation support is disabled
            , m_bUseSegMap(false)
#endif // segmentation support is disabled
            , m_frameNumOfLastArrivedFrame(0)
            , m_frameNumOfLastFrameSubmittedToDriver(0)
            , m_frameNumOfLastEncodedFrame(0)
        {
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
        mfxStatus SubmitTask(mfxVideoParam * par, Task*  pTask, VP9FrameLevelParam &frameParam)
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

            sts = DecideOnRefListAndDPBRefresh(par, pTask, m_dpb, frameParam);

            sts = pTask->SubmitTask(pRecFrame,m_dpb,
                                    frameParam, pRawLocalFrame,
                                    pOutBs);
            MFX_CHECK_STS(sts);

            UpdateDpb(frameParam, pRecFrame);

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

inline bool InsertSeqHeader(Task const &task)
{
    return (task.m_frameOrder == 0); // TODO: fix condition for SH insertion
}

} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN
