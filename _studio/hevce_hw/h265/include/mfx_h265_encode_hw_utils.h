//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "mfx_common.h"
#include "mfxplugin++.h"
#include "umc_mutex.h"

#include <vector>
#include <list>

#define MFX_MIN(x,y) ((x) < (y) ? (x) : (y))
#define MFX_MAX(x,y) ((x) > (y) ? (x) : (y))
#define STATIC_ASSERT(ASSERTION, MESSAGE) char MESSAGE[(ASSERTION) ? 1 : -1]; MESSAGE

namespace MfxHwH265Encode
{
    
template<class T> inline void Fill(T & obj, int val)       { memset(&obj, val, sizeof(obj)); }
template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }
template<class T> inline void Zero(std::vector<T> & vec)   { memset(&vec[0], 0, sizeof(T) * vec.size()); }
template<class T> inline void Zero(T * first, size_t cnt)  { memset(first, 0, sizeof(T) * cnt); }
template<class T, class U> inline void Copy(T & dst, U const & src)
{
    STATIC_ASSERT(sizeof(T) == sizeof(U), copy_objects_of_different_size);
    memcpy(&dst, &src, sizeof(dst));
}

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

class MfxFrameAllocResponse : public mfxFrameAllocResponse
{
public:
    MfxFrameAllocResponse();

    ~MfxFrameAllocResponse();

    mfxStatus Alloc(
        MFXCoreInterface *     core,
        mfxFrameAllocRequest & req,
        bool                   isCopyRequired);

    mfxStatus Alloc(
        MFXCoreInterface *     core,
        mfxFrameAllocRequest & req,
        mfxFrameSurface1 **    opaqSurf,
        mfxU32                 numOpaqSurf);
    
    mfxU32 Lock(mfxU32 idx);

    void Unlock();

    mfxU32 Unlock(mfxU32 idx);

    mfxU32 Locked(mfxU32 idx) const;

    mfxFrameInfo               m_info;

private:
    MfxFrameAllocResponse(MfxFrameAllocResponse const &);
    MfxFrameAllocResponse & operator =(MfxFrameAllocResponse const &);

    MFXCoreInterface * m_core;
    mfxU16             m_numFrameActualReturnedByAllocFrames;

    std::vector<mfxFrameAllocResponse> m_responseQueue;
    std::vector<mfxMemId>              m_mids;
    std::vector<mfxU32>                m_locked;
}; 

class MfxVideoParam : public mfxVideoParam
{
public:
    mfxU32 BufferSizeInKB;
    mfxU32 InitialDelayInKB;
    mfxU32 TargetKbps;
    mfxU32 MaxKbps;

    MfxVideoParam();
    MfxVideoParam(MfxVideoParam const & par);
    MfxVideoParam(mfxVideoParam const & par);

    MfxVideoParam & operator = (mfxVideoParam const &);
    mfxVideoParam & operator = (MfxVideoParam const &);

    void SyncVideoToCalculableParam();
    void SyncCalculableToVideoParam();

private:
    void Construct(mfxVideoParam const & par);
};

typedef struct _DpbFrame
{
    mfxU8    m_idxRaw;
    mfxU8    m_idxRec;
    mfxU16   m_frameType;
    mfxI32   m_poc;
    mfxMemId m_midRec;
}DpbFrame, DpbArray[MAX_DPB_SIZE];

typedef struct _Task : DpbFrame
{
    mfxBitstream*       m_bs;
    mfxFrameSurface1*   m_surf;
    mfxEncodeCtrl       m_ctrl;

    mfxU32 m_idxBs;

    mfxU8 m_refPicList[2][15];
    mfxU8 m_numRefActive[2];

    mfxU8  m_codingType;
    mfxU8  m_qpY;

    mfxU32 m_statusReportNumber;
    mfxU32 m_bsDataLength;

    DpbArray m_dpb[2]; //0 - before encoding, 1 - after

    mfxMemId m_midRaw;
    mfxMemId m_midBs;

    mfxU32 m_stage;
}Task;

typedef std::list<Task> TaskList;

class TaskManager
{
public:
    void Reset      (mfxU32 numTask);
    Task* New       ();
    Task* Reorder   (MfxVideoParam const & par, DpbArray const & dpb, bool flush);
    void  Ready     (Task* task);

private:
    TaskList   m_free;
    TaskList   m_reordering;
    TaskList   m_encoding;
    UMC::Mutex m_listMutex;
};

inline bool isValid(DpbFrame const & frame) { return IDX_INVALID !=  frame.m_idxRec; }
inline bool isDpbEnd(DpbArray const & dpb, mfxU32 idx) { return idx >= MAX_DPB_SIZE || !isValid(dpb[idx]); }

mfxU8 GetFrameType(MfxVideoParam const & video, mfxU32 frameOrder);

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
    mfxHDLPair &          handle);

mfxStatus CopyRawSurfaceToVideoMemory(
    MFXCoreInterface &    core,
    MfxVideoParam const & video,
    Task const &          task);
    
}; //namespace MfxHwH265Encode
