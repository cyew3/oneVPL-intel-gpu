//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

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
        TASK_FREE = 0,
        TASK_INITIALIZED,
        TASK_SUBMITTED
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
        return (pFrame) ? pCore->IncreaseReference(pCore->pthis, &pFrame->pSurface->Data) : MFX_ERR_NONE;
    }
    inline mfxStatus FreeSurface(sFrameEx* &pFrame, mfxCoreInterface* pCore)
    {
        mfxStatus sts = MFX_ERR_NONE;
        if (pFrame && pFrame->pSurface)
        {
            sts = pCore->DecreaseReference(pCore->pthis, &pFrame->pSurface->Data);
            pFrame = 0;
        }
        return sts;
    }
    inline bool isFreeSurface (sFrameEx* pFrame)
    {
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

    mfxStatus UpdateDpb(VP9FrameLevelParam &frameParam,
                        sFrameEx *pRecFrame,
                        std::vector<sFrameEx*>&dpb,
                        mfxCoreInterface *pCore);

    class ExternalFrames
    {
    protected:
        std::vector<sFrameEx>  m_frames;
    public:
        ExternalFrames() {}
        void Init(mfxU32 numFrames);
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

    inline mfxU32 CalcNumSurfRaw(mfxVideoParam const & video)
    {
        return video.AsyncDepth;
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

        VP9FrameLevelParam m_frameParam;
        sFrameEx*         m_pRecFrame;
        sFrameEx*         m_pRecRefFrames[3];
        sFrameEx*         m_pOutBs;
        mfxU32            m_frameOrder;
        mfxU64            m_timeStamp;

        mfxEncodeCtrl     m_ctrl;

        mfxU32 m_bsDataLength;

        mfxStatus CopyInput();

        Task ():
              m_status(TASK_FREE),
              m_pRawFrame(NULL),
              m_pRawLocalFrame(NULL),
              m_pBitsteam(0),
              m_pRecFrame(NULL),
              m_pOutBs(NULL),
              m_frameOrder(0),
              m_timeStamp(0),
              m_bsDataLength(0)
          {
              Zero(m_pRecRefFrames);
              Zero(m_frameParam);
              Zero(m_ctrl);
          }

          ~Task() {};

          mfxStatus GetOriginalSurface(mfxFrameSurface1 *& pSurface, bool &bExternal);
          mfxStatus GetInputSurface(mfxFrameSurface1 *& pSurface, bool &bExternal);
    };


    inline mfxStatus FreeTask(mfxCoreInterface *pCore, Task &task)
    {
        MFX_CHECK_STS(FreeSurface(task.m_pRawFrame, pCore));
        MFX_CHECK_STS(FreeSurface(task.m_pRawLocalFrame, pCore));
        MFX_CHECK_STS(FreeSurface(task.m_pOutBs, pCore));

        task.m_pBitsteam = 0;
        Zero(task.m_frameParam);
        Zero(task.m_ctrl);
        task.m_status = TASK_FREE;

        return MFX_ERR_NONE;
    }

    inline Task *GetFreeTask(std::vector<Task> & tasks)
    {
        for (std::vector<Task>::iterator task = tasks.begin(); task != tasks.end(); task++)
        {
            if (task->m_status == TASK_FREE)
            {
                return &task[0];
            }
        }

        return 0;
    }

    inline mfxStatus FreeTasks(mfxCoreInterface *pCore, std::vector<Task> & tasks)
    {
        mfxStatus sts = MFX_ERR_NONE;
        for (std::vector<Task>::iterator task = tasks.begin(); task != tasks.end(); task++)
        {
            if (task->m_status != TASK_FREE)
            {
                sts = FreeTask(pCore, *task);
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

#if 0 // these two functions are left for reference for future refactiring
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
#endif

inline bool InsertSeqHeader(Task const &task)
{
    return (task.m_frameOrder == 0); // TODO: fix condition for SH insertion
}

} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN
