//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016-2017 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfx_common.h"
#include "mfx_ext_buffers.h"
#include "mfx_trace.h"
#include "umc_mutex.h"
#include <vector>
#include <list>
#include <memory>
#include "mfxstructures.h"
#include "mfx_enc_common.h"
#include "assert.h"

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)

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
#ifndef OPEN_SOURCE
    #define MFX_MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
    #define MFX_MIN( a, b ) ( ((a) < (b)) ? (a) : (b) )
#endif

#define DPB_SIZE 8 // DPB size by VP9 spec
#define DPB_SIZE_REAL 3 // DPB size really used by encoder
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
#define MAX_ICQ_QUALITY_INDEX 255
#define MAX_LF_LEVEL 63
#define MAX_ABS_Q_INDEX_DELTA 15

#define MAX_TASK_ID 0xffff

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

static const mfxU16 MFX_MEMTYPE_SYS_OR_D3D =
MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_SYSTEM_MEMORY;

enum // identifies memory type at encoder input w/o any details
{
    INPUT_SYSTEM_MEMORY,
    INPUT_VIDEO_MEMORY
};


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

    // Sequence level parameters should contain only parameters that will not change during encoding
    struct VP9SeqLevelParam
    {
        mfxU8  profile;
        mfxU8  bitDepth;;
        mfxU8  colorSpace;
        mfxU8  colorRange;
        mfxU8  subsamplingX;
        mfxU8  subsamplingY;

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
        mfxU8  allowSegmentation;
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

    inline mfxU32 CeilDiv(mfxU32 x, mfxU32 y) { return (x + y - 1) / y; }

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
    BIND_EXTBUF_TYPE_TO_ID (mfxExtCodingOption2, MFX_EXTBUFF_CODING_OPTION2);
    BIND_EXTBUF_TYPE_TO_ID (mfxExtCodingOption3, MFX_EXTBUFF_CODING_OPTION3);
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

        mfxStatus Release();

        mfxFrameInfo               m_info;

    private:
        MfxFrameAllocResponse(MfxFrameAllocResponse const &);

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

#define NUM_OF_SUPPORTED_EXT_BUFFERS 4 // mfxExtCodingOptionVP9, mfxExtOpaqueSurfaceAlloc, mfxExtCodingOption2, mfxExtCodingOption3

    class VP9MfxVideoParam : public mfxVideoParam
    {
    public:
        VP9MfxVideoParam();
        VP9MfxVideoParam(VP9MfxVideoParam const &);
        VP9MfxVideoParam(mfxVideoParam const &);

        VP9MfxVideoParam & operator = (VP9MfxVideoParam const &);
        VP9MfxVideoParam & operator = (mfxVideoParam const &);

        mfxU16 m_inMemType;
        mfxU32 m_targetKbps;
        mfxU32 m_maxKbps;
        mfxU32 m_bufferSizeInKb;
        mfxU32 m_initialDelayInKb;

        void CalculateInternalParams();
        void SyncInternalParamToExternal();

    protected:
        void Construct(mfxVideoParam const & par);

    private:
        mfxExtBuffer *              m_extParam[NUM_OF_SUPPORTED_EXT_BUFFERS];
        mfxExtCodingOptionVP9       m_extOpt;
        mfxExtOpaqueSurfaceAlloc    m_extOpaque;
        mfxExtCodingOption2         m_extOpt2;
        mfxExtCodingOption3         m_extOpt3;
    };

    class Task;
    mfxStatus SetFramesParams(VP9MfxVideoParam const &par,
                              Task const & task,
                              VP9FrameLevelParam &frameParam,
                              mfxCoreInterface const * pCore);

    mfxStatus InitVp9SeqLevelParam(VP9MfxVideoParam const &video, VP9SeqLevelParam &param);

    mfxStatus DecideOnRefListAndDPBRefresh(VP9MfxVideoParam const & par,
                                           Task *pTask,
                                           std::vector<sFrameEx*>& dpb,
                                           VP9FrameLevelParam &frameParam);

    mfxStatus UpdateDpb(VP9FrameLevelParam &frameParam,
                        sFrameEx *pRecFrame,
                        std::vector<sFrameEx*>&dpb,
                        mfxCoreInterface *pCore);

    class ExternalFrames
    {
    protected:
        std::list<sFrameEx>  m_frames;
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
        mfxStatus Init(mfxCoreInterface *pCore, mfxFrameAllocRequest *pAllocReq);
        sFrameEx * GetFreeFrame();
        mfxStatus  GetFrame(mfxU32 numFrame, sFrameEx * &Frame);
        mfxStatus Release();

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

    inline mfxU32 CalcNumSurfRecon(mfxVideoParam const & video)
    {
        return video.mfx.NumRefFrame + CalcNumTasks(video);
    }

    inline mfxU32 CalcNumSurfRaw(mfxVideoParam const & video)
    {
        // number of input surfaces is same for VIDEO and SYSTEM memory
        // because so far encoder doesn't support LookAhead and B-frames
        return video.AsyncDepth;
    }

    inline mfxU32 CalcNumMB(mfxVideoParam const & video)
    {
        return (video.mfx.FrameInfo.Width>>4)*(video.mfx.FrameInfo.Height>>4);
    }

    class Task
    {
    public:

        sFrameEx*         m_pRawFrame;
        sFrameEx*         m_pRawLocalFrame;
        mfxBitstream*     m_pBitsteam;

        VP9FrameLevelParam m_frameParam;
        sFrameEx*         m_pRecFrame;
        sFrameEx*         m_pRecRefFrames[3];
        sFrameEx*         m_pOutBs;
        mfxU32            m_frameOrder;
        mfxU64            m_timeStamp;
        mfxU32            m_taskIdForDriver;
        mfxU32            m_frameOrderInGop;

        mfxEncodeCtrl     m_ctrl;

        mfxU32 m_bsDataLength;

        VP9MfxVideoParam* m_pParam;

        bool m_insertIVFSeqHeader;
        bool m_resetBrc;

        Task ():
              m_pRawFrame(NULL),
              m_pRawLocalFrame(NULL),
              m_pBitsteam(0),
              m_pRecFrame(NULL),
              m_pOutBs(NULL),
              m_frameOrder(0),
              m_timeStamp(0),
              m_taskIdForDriver(0),
              m_frameOrderInGop(0),
              m_bsDataLength(0),
              m_pParam(NULL),
              m_insertIVFSeqHeader(false),
            m_resetBrc(false)
          {
              Zero(m_pRecRefFrames);
              Zero(m_frameParam);
              Zero(m_ctrl);
          }

          ~Task() {};
    };


    inline mfxStatus FreeTask(mfxCoreInterface *pCore, Task &task)
    {
        mfxStatus sts = FreeSurface(task.m_pRawFrame, pCore);
        MFX_CHECK_STS(sts);
        sts = FreeSurface(task.m_pRawLocalFrame, pCore);
        MFX_CHECK_STS(sts);
        sts = FreeSurface(task.m_pOutBs, pCore);
        MFX_CHECK_STS(sts);

        task.m_pBitsteam = 0;
        Zero(task.m_frameParam);
        Zero(task.m_ctrl);

        return MFX_ERR_NONE;
    }

    inline bool IsBufferBasedBRC(mfxU16 brcMethod)
    {
        return brcMethod == MFX_RATECONTROL_CBR
            || brcMethod == MFX_RATECONTROL_VBR;
    }

    inline bool IsBitrateBasedBRC(mfxU16 brcMethod)
    {
        return IsBufferBasedBRC(brcMethod);
    }

    inline bool IsResetOfPipelineRequired(const mfxVideoParam& oldPar, const mfxVideoParam& newPar)
    {
        return oldPar.mfx.GopPicSize != newPar.mfx.GopPicSize;
    }

    inline bool isBrcResetRequired(VP9MfxVideoParam const & parBefore, VP9MfxVideoParam const & parAfter)
    {
        mfxU16 brc = parAfter.mfx.RateControlMethod;
        if (brc == MFX_RATECONTROL_CQP)
        {
            return false;
        }

        double frameRateBefore = (double)parBefore.mfx.FrameInfo.FrameRateExtN / (double)parBefore.mfx.FrameInfo.FrameRateExtD;
        double frameRateAfter = (double)parAfter.mfx.FrameInfo.FrameRateExtN / (double)parAfter.mfx.FrameInfo.FrameRateExtD;

        if (parBefore.m_targetKbps != parAfter.m_targetKbps
            || (brc == MFX_RATECONTROL_VBR) && (parBefore.m_maxKbps != parAfter.m_maxKbps)
            || frameRateBefore != frameRateAfter
            || IsBufferBasedBRC(brc) && (parBefore.m_bufferSizeInKb != parAfter.m_bufferSizeInKb)
            || (brc == MFX_RATECONTROL_ICQ) && (parBefore.mfx.ICQQuality != parAfter.mfx.ICQQuality))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    mfxStatus GetRealSurface(
        mfxCoreInterface const *pCore,
        VP9MfxVideoParam const &par,
        Task const &task,
        mfxFrameSurface1 *& pSurface);

    mfxStatus GetInputSurface(
        mfxCoreInterface const *pCore,
        VP9MfxVideoParam const &par,
        Task const &task,
        mfxFrameSurface1 *& pSurface);

    mfxStatus CopyRawSurfaceToVideoMemory(
        mfxCoreInterface *pCore,
        VP9MfxVideoParam const &par,
        Task const &task);

/*mfxStatus ReleaseDpbFrames(mfxCoreInterface* pCore, std::vector<sFrameEx*> & dpb)
{
    for (mfxU8 refIdx = 0; refIdx < dpb.size(); refIdx ++)
    {
        if (dpb[refIdx])
        {
            dpb[refIdx]->refCount = 0;
            MFX_CHECK_STS(FreeSurface(dpb[refIdx], pCore));
        }
    }

    return MFX_ERR_NONE;
}*/

struct FindTaskByRawSurface
{
    FindTaskByRawSurface(mfxFrameSurface1 * pSurf) : m_pSurface(pSurf) {}

    bool operator ()(Task const & task)
    {
        return task.m_pRawFrame->pSurface == m_pSurface;
    }

    mfxFrameSurface1* m_pSurface;
};

struct FindTaskByMfxVideoParam
{
    FindTaskByMfxVideoParam(mfxVideoParam* pPar) : m_pPar(pPar) {}

    bool operator ()(Task const & task)
    {
        return task.m_pParam == m_pPar;
    }

    mfxVideoParam* m_pPar;
};

} // MfxHwVP9Encode

#endif // PRE_SI_TARGET_PLATFORM_GEN10
