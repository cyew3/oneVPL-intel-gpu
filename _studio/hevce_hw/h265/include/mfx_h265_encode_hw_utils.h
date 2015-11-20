//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "mfx_h265_encode_hw_set.h"
#include "mfx_ext_buffers.h"
#include "mfx_common.h"
#include "mfxplugin++.h"
#include "umc_mutex.h"

#define DEBUG_REC_FRAMES_INFO 0   // dependency from fwrite(). Custom writing to file shouldn't be present in MSDK releases w/o documentation and testing
#ifdef DEBUG_REC_FRAMES_INFO
#include "vm_file.h"
#endif
#include <vector>
#include <list>
#include <assert.h>

#define MFX_MIN(x,y) ((x) < (y) ? (x) : (y))
#define MFX_MAX(x,y) ((x) > (y) ? (x) : (y))
#define STATIC_ASSERT(ASSERTION, MESSAGE) char MESSAGE[(ASSERTION) ? 1 : -1]; MESSAGE
#define MFX_SORT_COMMON(_AR, _SZ, _COND)\
    for (mfxU32 _i = 0; _i < (_SZ); _i ++)\
        for (mfxU32 _j = _i; _j < (_SZ); _j ++)\
            if (_COND) std::swap(_AR[_i], _AR[_j]);
#define MFX_SORT(_AR, _SZ, _OP) MFX_SORT_COMMON(_AR, _SZ, _AR[_i] _OP _AR[_j])
#define MFX_SORT_STRUCT(_AR, _SZ, _M, _OP) MFX_SORT_COMMON(_AR, _SZ, _AR[_i]._M _OP _AR[_j]._M)


namespace MfxHwH265Encode
{


template<class T> inline bool Equal(T const & l, T const & r) { return memcmp(&l, &r, sizeof(T)) == 0; }
template<class T> inline void Fill(T & obj, int val)          { memset(&obj, val, sizeof(obj)); }
template<class T> inline void Zero(T & obj)                   { memset(&obj, 0, sizeof(obj)); }
template<class T> inline void Zero(std::vector<T> & vec)      { memset(&vec[0], 0, sizeof(T) * vec.size()); }
template<class T> inline void Zero(T * first, size_t cnt)     { memset(first, 0, sizeof(T) * cnt); }
template<class T, class U> inline void Copy(T & dst, U const & src)
{
    STATIC_ASSERT(sizeof(T) == sizeof(U), copy_objects_of_different_size);
    memcpy_s(&dst, sizeof(dst), &src, sizeof(dst));
}
template<class T> inline T Abs  (T x)               { return (x > 0 ? x : -x); }
template<class T> inline T Min  (T x, T y)          { return MFX_MIN(x, y); }
template<class T> inline T Max  (T x, T y)          { return MFX_MAX(x, y); }
template<class T> inline T Clip3(T min, T max, T x) { return Min(Max(min, x), max); }
template<class T> inline T Align(T value, mfxU32 alignment)
{
    assert((alignment & (alignment - 1)) == 0); // should be 2^n
    return T((value + alignment - 1) & ~(alignment - 1));
}
template<class T> bool IsAligned(T value, mfxU32 alignment)
{
    assert((alignment & (alignment - 1)) == 0); // should be 2^n
    return !(value & (alignment - 1));
}
template<class T> inline T Lsb(T val, mfxU32 maxLSB)
{
    if (val >= 0)
        return val % maxLSB;
    return (maxLSB - ((-val) % maxLSB)) % maxLSB;
}

inline mfxU32 CeilLog2  (mfxU32 x)           { mfxU32 l = 0; while(x > (1U<<l)) l++; return l; }
inline mfxU32 CeilDiv   (mfxU32 x, mfxU32 y) { return (x + y - 1) / y; }
inline mfxU64 CeilDiv   (mfxU64 x, mfxU64 y) { return (x + y - 1) / y; }
inline mfxU32 Ceil      (mfxF64 x)           { return (mfxU32)(.999 + x); }

enum
{
    MFX_IOPATTERN_IN_MASK = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY,
    MFX_MEMTYPE_D3D_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME,
    MFX_MEMTYPE_D3D_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME,
    MFX_MEMTYPE_SYS_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY        | MFX_MEMTYPE_EXTERNAL_FRAME,
    MFX_MEMTYPE_SYS_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY        | MFX_MEMTYPE_INTERNAL_FRAME,
};

enum
{
    MAX_DPB_SIZE        = 15,
    IDX_INVALID         = 0xFF,
    HW_SURF_ALIGN_W     = 32,
    HW_SURF_ALIGN_H     = 32,
    CODED_PIC_ALIGN_W   = 16,
    CODED_PIC_ALIGN_H   = 16,
    DEFAULT_LCU_SIZE    = 32,
    MAX_SLICES          = 200,
    DEFAULT_LTR_INTERVAL= 16
};

enum
{
    FRAME_ACCEPTED      = 0x01,
    FRAME_REORDERED     = 0x02,
    FRAME_SUBMITTED     = 0x04,
    FRAME_ENCODED       = 0x08,
    STAGE_SUBMIT        = FRAME_ACCEPTED    | FRAME_REORDERED,
    STAGE_QUERY         = STAGE_SUBMIT      | FRAME_SUBMITTED,
    STAGE_READY         = STAGE_QUERY       | FRAME_ENCODED,
};

enum
{
    CODING_TYPE_I   = 1,
    CODING_TYPE_P   = 2,
    CODING_TYPE_B   = 3, //regular B, or no reference to other B frames
    CODING_TYPE_B1  = 4, //B1, reference to only I, P or regular B frames
    CODING_TYPE_B2  = 5, //B2, references include B1
};

enum
{
    INSERT_AUD      = 0x01,
    INSERT_VPS      = 0x02,
    INSERT_SPS      = 0x04,
    INSERT_PPS      = 0x08,
    INSERT_BPSEI    = 0x10,
    INSERT_PTSEI    = 0x20,

    INSERT_SEI = (INSERT_BPSEI | INSERT_PTSEI)
};

inline bool IsOn(mfxU32 opt)
{
    return opt == MFX_CODINGOPTION_ON;
}

inline bool IsOff(mfxU32 opt)
{
    return opt == MFX_CODINGOPTION_OFF;
}

class MfxFrameAllocResponse : public mfxFrameAllocResponse
{
public:
    MfxFrameAllocResponse();

    ~MfxFrameAllocResponse();

    mfxStatus Alloc(
        MFXCoreInterface *     core,
        mfxFrameAllocRequest & req,
        bool                   isCopyRequired);

    mfxU32 Lock(mfxU32 idx);

    void Unlock();

    mfxU32 Unlock(mfxU32 idx);

    mfxU32 Locked(mfxU32 idx) const;

    void Free();

    bool isExternal() { return m_isExternal; };

    mfxU32 FindFreeResourceIndex(mfxFrameSurface1* external_surf = 0);

    mfxFrameInfo               m_info;

private:
    MfxFrameAllocResponse(MfxFrameAllocResponse const &);
    MfxFrameAllocResponse & operator =(MfxFrameAllocResponse const &);

    MFXCoreInterface * m_core;
    mfxU16             m_numFrameActualReturnedByAllocFrames;

    std::vector<mfxFrameAllocResponse> m_responseQueue;
    std::vector<mfxMemId>              m_mids;
    std::vector<mfxU32>                m_locked;
    bool m_isExternal;
};

typedef struct _DpbFrame
{
    mfxI32   m_poc;
    mfxU32   m_fo;  // FrameOrder
    mfxU8    m_tid;
    bool     m_ltr; // is "long-term"
    bool     m_ldb; // is "low-delay B"
    mfxU8    m_codingType;
    mfxU8    m_idxRaw;
    mfxU8    m_idxRec;
    mfxMemId m_midRec;
    mfxMemId m_midRaw;
    mfxFrameSurface1*   m_surf; //input surface, may be opaque
}DpbFrame, DpbArray[MAX_DPB_SIZE];

enum
{
    TASK_DPB_ACTIVE = 0, // available during task execution (modified dpb[BEFORE] )
    TASK_DPB_AFTER,      // after task execution (ACTIVE + curTask if ref)
    TASK_DPB_BEFORE,     // after previous task execution (prevTask dpb[AFTER])
    TASK_DPB_NUM
};

struct IntraRefreshState
{
    mfxU16  refrType;
    mfxU16  IntraLocation;
    mfxU16  IntraSize;
    mfxI16  IntRefQPDelta;
    bool    firstFrameInCycle;
};
typedef struct _Task : DpbFrame
{
    mfxBitstream*       m_bs;
    mfxFrameSurface1*   m_surf_real;
    mfxEncodeCtrl       m_ctrl;
    Slice               m_sh;

    mfxU32 m_idxBs;

    mfxU8 m_refPicList[2][MAX_DPB_SIZE];
    mfxU8 m_numRefActive[2];

    mfxU16 m_frameType;
    mfxU16 m_insertHeaders;
    mfxU8  m_shNUT;
    mfxU8  m_qpY;
    mfxI32 m_lastIPoc;

    mfxU32 m_statusReportNumber;
    mfxU32 m_bsDataLength;

    DpbArray m_dpb[TASK_DPB_NUM];

    mfxMemId m_midBs;

    bool m_resetBRC;

    mfxU32 m_initial_cpb_removal_delay;
    mfxU32 m_initial_cpb_removal_offset;
    mfxU32 m_cpb_removal_delay;
    mfxU32 m_dpb_output_delay;

    mfxU32 m_stage;
    IntraRefreshState m_IRState;
}Task;

enum
{
    NO_REFRESH             = 0,
    VERT_REFRESH           = 1,
    HORIZ_REFRESH          = 2
};

typedef std::list<Task> TaskList;

template <typename T>
struct remove_const
{
    typedef T type;
};

template <typename T>
struct remove_const<const T>
{
    typedef T type;
};

namespace ExtBuffer
{
    #define ExtBuffersArray \
     MFX_EXTBUFF_HEVC_PARAM, \
     MFX_EXTBUFF_HEVC_TILES, \
     MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, \
     MFX_EXTBUFF_DPB, \
     MFX_EXTBUFF_AVC_REFLISTS, \
     MFX_EXTBUFF_CODING_OPTION,\
     MFX_EXTBUFF_CODING_OPTION2, \
     MFX_EXTBUFF_CODING_OPTION3, \
     MFX_EXTBUFF_DDI, \
     MFX_EXTBUFF_CODING_OPTION_SPSPPS, \
     MFX_EXTBUFF_AVC_REFLIST_CTRL, \
     MFX_EXTBUFF_AVC_TEMPORAL_LAYERS, \
     MFX_EXTBUFF_DUMP, \
     MFX_EXTBUFF_ENCODER_RESET_OPTION,\
     MFX_EXTBUFF_CODING_OPTION_VPS

    template<class T> struct ExtBufferMap {};

    #define EXTBUF(TYPE, ID) template<> struct ExtBufferMap <TYPE> { enum { Id = ID}; }
        EXTBUF(mfxExtHEVCParam,             MFX_EXTBUFF_HEVC_PARAM);
        EXTBUF(mfxExtHEVCTiles,             MFX_EXTBUFF_HEVC_TILES);
        EXTBUF(mfxExtOpaqueSurfaceAlloc,    MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        EXTBUF(mfxExtDPB,                   MFX_EXTBUFF_DPB);
        EXTBUF(mfxExtAVCRefLists,           MFX_EXTBUFF_AVC_REFLISTS);
        EXTBUF(mfxExtCodingOption2,         MFX_EXTBUFF_CODING_OPTION2);
        EXTBUF(mfxExtCodingOption3,         MFX_EXTBUFF_CODING_OPTION3);
        EXTBUF(mfxExtCodingOptionDDI,       MFX_EXTBUFF_DDI);
        EXTBUF(mfxExtCodingOptionSPSPPS,    MFX_EXTBUFF_CODING_OPTION_SPSPPS);
        EXTBUF(mfxExtAVCRefListCtrl,        MFX_EXTBUFF_AVC_REFLIST_CTRL);
        EXTBUF(mfxExtAvcTemporalLayers,     MFX_EXTBUFF_AVC_TEMPORAL_LAYERS);
        EXTBUF(mfxExtDumpFiles,             MFX_EXTBUFF_DUMP);
        EXTBUF(mfxExtEncoderResetOption,    MFX_EXTBUFF_ENCODER_RESET_OPTION);
        EXTBUF(mfxExtCodingOptionVPS,       MFX_EXTBUFF_CODING_OPTION_VPS);
    #undef EXTBUF

    class Proxy
    {
    private:
        mfxExtBuffer ** m_b;
        mfxU16          m_n;
    public:
        Proxy(mfxExtBuffer ** b, mfxU16 n)
            : m_b(b)
            , m_n(n)
        {
        }

        template <class T> operator T*()
        {
            if (m_b)
                for (mfxU16 i = 0; i < m_n; i ++)
                    if (m_b[i] && m_b[i]->BufferId == ExtBufferMap<T>::Id)
                        return (T*)m_b[i];
            return 0;
        }
    };

    template <class P> Proxy Get(P & par)
    {
        return Proxy(par.ExtParam, par.NumExtParam);
    }

    template<class T> void Init(T& buf)
    {
        Zero(buf);
        mfxExtBuffer& header = *((mfxExtBuffer*)&buf);
        header.BufferId = ExtBufferMap<T>::Id;
        header.BufferSz = sizeof(T);
    }
    
    template<class T> inline void Add(T& buf, mfxExtBuffer* pBuffers[], mfxU16 &numbuffers)
    {
        pBuffers[numbuffers++] = ((mfxExtBuffer*)&buf);
    }


    template<class P, class T> bool Construct(P const & par, T& buf, mfxExtBuffer* pBuffers[], mfxU16 &numbuffers)
    {
        T  * p = Get(par);
        Add(buf,pBuffers,numbuffers);

        if (p)
        {
            buf = *p;
            return true;
        }

        Init(buf);

        return false;
    }

    template<class P, class T> bool Set(P& par, T const & buf)
    {
        T* p = Get(par);

        if (p)
        {
            *p = buf;
            return true;
        }

        return false;
    }

    bool Construct(mfxVideoParam const & par, mfxExtHEVCParam& buf);

    mfxStatus CheckBuffers(mfxVideoParam const & par, const mfxU32 allowed[], mfxU32 notDetected[], mfxU32 size);

    inline mfxStatus CheckBuffers(mfxVideoParam const & par)
    {
        // checked if all buffers are supported, not repeated

        const mfxU32 allowed[] = { ExtBuffersArray };
        mfxU32 notDetected[] = { ExtBuffersArray };
        mfxU32 size = sizeof(allowed) / sizeof(mfxU32);
        if (par.NumExtParam)
            return CheckBuffers(par, allowed, notDetected, size);
        else
            return MFX_ERR_NONE;

    }
    inline mfxStatus CheckBuffers(mfxVideoParam const & par1, mfxVideoParam const & par2)
    {
        // checked if all buffers are supported, not repeated, identical set for both parfiles
        const mfxU32 allowed[] = { ExtBuffersArray };
        mfxU32 notDetected1[] = { ExtBuffersArray };
        mfxU32 notDetected2[] = { ExtBuffersArray };
        mfxU32 size = sizeof(allowed) / sizeof(mfxU32);

        if (par1.NumExtParam && par2.NumExtParam)
        {
            MFX_CHECK_STS(CheckBuffers(par1, allowed, notDetected1, size));
            MFX_CHECK_STS(CheckBuffers(par2, allowed, notDetected2, size));
        }
        else
        {
            if (par1.NumExtParam == par2.NumExtParam)
                return MFX_ERR_NONE;
            else
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        if (memcmp(notDetected1, notDetected2, size) != 0)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        return MFX_ERR_NONE;
    }

    #undef ExtBuffersArray
};

class TemporalLayers
{
public:
    TemporalLayers()
        : m_numTL(1)
    {
        Zero(m_TL);
    }

    ~TemporalLayers(){}

    void SetTL(mfxExtAvcTemporalLayers const & tl)
    {
        m_numTL = 0;
        Zero(m_TL);
        m_TL[0].Scale = 1;

        for (mfxU8 i = 0; i < 7; i++)
        {
            if (tl.Layer[i].Scale)
            {
                m_TL[m_numTL].TId = i;
                m_TL[m_numTL].Scale = (mfxU8)tl.Layer[i].Scale;
                m_numTL++;
            }
        }

        m_numTL = Max<mfxU8>(m_numTL, 1);
    }

    mfxU8 NumTL() const { return m_numTL; }

    mfxU8 GetTId(mfxU32 frameOrder) const
    {
        mfxU16 i;

        if (!m_numTL)
            return 0;

        for (i = 0; i < m_numTL && (frameOrder % (m_TL[m_numTL - 1].Scale / m_TL[i].Scale))  ; i++);

        return (i < m_numTL) ? m_TL[i].TId : 0;
    }

    mfxU8 HighestTId() const { return  m_TL[m_numTL - 1].TId; }

private:
    mfxU8 m_numTL;

    struct
    {
        mfxU8 TId;
        mfxU8 Scale;
    }m_TL[8];
};


class MfxVideoParam : public mfxVideoParam, public TemporalLayers
{
public:
    VPS m_vps;
    SPS m_sps;
    PPS m_pps;

    struct SliceInfo
    {
        mfxU32 SegmentAddress;
        mfxU32 NumLCU;
    };
    std::vector<SliceInfo> m_slice;

    struct
    {
        mfxExtHEVCParam             HEVCParam;
        mfxExtHEVCTiles             HEVCTiles;
        mfxExtOpaqueSurfaceAlloc    Opaque;
        mfxExtCodingOption2         CO2;
        mfxExtCodingOption3         CO3;
        mfxExtCodingOptionDDI       DDI;
        mfxExtAvcTemporalLayers     AVCTL;
        mfxExtDumpFiles             DumpFiles;
        mfxExtBuffer *              m_extParam[9];
    } m_ext;

    mfxU32 BufferSizeInKB;
    mfxU32 InitialDelayInKB;
    mfxU32 TargetKbps;
    mfxU32 MaxKbps;
    mfxU16 NumRefLX[2]; // max num active refs
    mfxU32 LTRInterval;
    mfxU32 LCUSize;
    bool   InsertHRDInfo;
    bool   RawRef;

    MfxVideoParam();
    MfxVideoParam(MfxVideoParam const & par);
    MfxVideoParam(mfxVideoParam const & par);

    MfxVideoParam & operator = (mfxVideoParam const &);
    MfxVideoParam & operator = (MfxVideoParam const &);

    void SyncVideoToCalculableParam();
    void SyncCalculableToVideoParam();
    void SyncMfxToHeadersParam(mfxU32 numSlicesForSTRPSOpt = 0);
    void SyncHeadersToMfxParam();

    mfxStatus FillPar(mfxVideoParam& par, bool query = false);

    void GetSliceHeader(Task const & task, Task const & prevTask, Slice & s) const;

    mfxStatus GetExtBuffers(mfxVideoParam& par, bool query = false);

    bool isBPyramid() const { return m_ext.CO2.BRefType == MFX_B_REF_PYRAMID; }
    bool isLowDelay() const { return ((m_ext.CO3.PRefType == MFX_P_REF_PYRAMID) && !isTL()); }
    bool isTL()       const { return NumTL() > 1; }

private:
    void Construct(mfxVideoParam const & par);
    void CopyCalcParams(MfxVideoParam const & par);
};

class HRD
{
public:
    void Setup(SPS const & sps, mfxU32 InitialDelayInKB);

    void Reset(SPS const & sps);

    void RemoveAccessUnit(
        mfxU32 size,
        mfxU32 bufferingPeriod);

    mfxU32 GetInitCpbRemovalDelay() const;

    mfxU32 GetInitCpbRemovalDelayOffset() const;
    mfxU32 GetMaxFrameSize(mfxU32 bufferingPeriod) const;

private:
    mfxU32 m_bitrate;
    mfxU32 m_rcMethod;
    mfxU32 m_hrdIn90k;  // size of hrd buffer in 90kHz units

    mfxF64 m_tick;      // clock tick
    mfxF64 m_trn_cur;   // nominal removal time
    mfxF64 m_taf_prv;   // final arrival time of prev unit

    bool m_bIsHrdRequired;
};

class TaskManager
{
public:

    void  Reset     (mfxU32 numTask = 0);
    Task* New       ();
    Task* Reorder   (MfxVideoParam const & par, DpbArray const & dpb, bool flush);
    void  Submit    (Task* task);
    Task* GetSubmittedTask();
    void  SubmitForQuery(Task* task);
    bool  isSubmittedForQuery(Task* task);
    Task* GetTaskForQuery();
    void  Ready     (Task* task);

private:
    TaskList   m_free;
    TaskList   m_reordering;
    TaskList   m_encoding;
    TaskList   m_querying;
    UMC::Mutex m_listMutex;
};

class FrameLocker : public mfxFrameData
{
public:
    FrameLocker(MFXCoreInterface* core, mfxMemId mid)
        : m_core(core)
        , m_mid(mid)
    {
        Zero((mfxFrameData&)*this);
        Lock();
    }
    ~FrameLocker()
    {
        Unlock();
    }
    mfxStatus Lock()   { return m_core->FrameAllocator().Lock  (m_core->FrameAllocator().pthis, m_mid, this); }
    mfxStatus Unlock() { return m_core->FrameAllocator().Unlock(m_core->FrameAllocator().pthis, m_mid, this); }

private:
    MFXCoreInterface* m_core;
    mfxMemId m_mid;
};

inline bool isValid(DpbFrame const & frame) { return IDX_INVALID !=  frame.m_idxRec; }
inline bool isDpbEnd(DpbArray const & dpb, mfxU32 idx) { return idx >= MAX_DPB_SIZE || !isValid(dpb[idx]); }

mfxU8 GetFrameType(MfxVideoParam const & video, mfxU32 frameOrder);


void ConstructSTRPS(
    DpbArray const & DPB,
    mfxU8 const (&RPL)[2][MAX_DPB_SIZE],
    mfxU8 const (&numRefActive)[2],
    mfxI32 poc,
    STRPS& rps);

void ConstructRPL(
    MfxVideoParam const & par,
    DpbArray const & DPB,
    bool isB,
    mfxI32 poc,
    mfxU8  tid,
    mfxU8 (&RPL)[2][MAX_DPB_SIZE],
    mfxU8 (&numRefActive)[2],
    mfxExtAVCRefLists * pExtLists,
    mfxExtAVCRefListCtrl * pLCtrl = 0);

inline void ConstructRPL(
    MfxVideoParam const & par,
    DpbArray const & DPB,
    bool isB,
    mfxI32 poc,
    mfxU8  tid,
    mfxU8(&RPL)[2][MAX_DPB_SIZE],
    mfxU8(&numRefActive)[2])
{
    ConstructRPL(par, DPB, isB, poc, tid, RPL, numRefActive, 0, 0);
}

void UpdateDPB(
    MfxVideoParam const & par,
    DpbFrame const & task,
    DpbArray & dpb,
    mfxExtAVCRefListCtrl * pLCtrl = 0);

bool isLTR(
    DpbArray const & dpb,
    mfxU32 LTRInterval,
    mfxI32 poc);

void ConfigureTask(
    Task &                task,
    Task const &          prevTask,
    MfxVideoParam const & video);

mfxU32 FindFreeResourceIndex(
    MfxFrameAllocResponse const & pool,
    mfxU32                        startingFrom = 0);

mfxMemId AcquireResource(
    MfxFrameAllocResponse & pool,
    mfxU32                  index);

void ReleaseResource(
    MfxFrameAllocResponse & pool,
    mfxMemId                mid);

mfxStatus GetNativeHandleToRawSurface(
    MFXCoreInterface &    core,
    MfxVideoParam const & video,
    Task const &          task,
    mfxHDL &              handle);

mfxStatus CopyRawSurfaceToVideoMemory(
    MFXCoreInterface &    core,
    MfxVideoParam const & video,
    Task const &          task);

IntraRefreshState GetIntraRefreshState(
    MfxVideoParam const & video,
    mfxU32                frameOrderInGopDispOrder,
    mfxEncodeCtrl const * ctrl,
    mfxU16                intraStripeWidthInMBs);

#if DEBUG_REC_FRAMES_INFO
    inline vm_file * OpenFile(vm_char const * name, vm_char const * mode)
    {
        return name[0] ? vm_file_fopen(name, mode) : 0;
    }
    void WriteFrameData(
        vm_file *            file,
        mfxFrameData const & data,
        mfxFrameInfo const & info);
#endif // removed dependency from fwrite(). Custom writing to file shouldn't be present in MSDK releases w/o documentation and testing
}; //namespace MfxHwH265Encode
