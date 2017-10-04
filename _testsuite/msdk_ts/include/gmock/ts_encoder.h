/////////////////////////////////////////////////////////////////////////////
////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
////     This software is supplied under the terms of a license agreement or
////     nondisclosure agreement with Intel Corporation and may not be copied
////     or disclosed except in accordance with the terms of that agreement.
////          Copyright(c) 2017 Intel Corporation. All Rights Reserved.
////

#pragma once

#include "ts_session.h"
#include "ts_ext_buffers.h"
#include "ts_surface.h"
#include <memory>

class tsSharedCtrl
{
public:
    typedef tsExtBufType<mfxEncodeCtrl> Type;
    std::shared_ptr<Type> m_ptr;
    mfxU32 m_lockCnt;
    mfxU32 m_fo;
    mfxSyncPoint m_sp;
    bool m_unlock;

    tsSharedCtrl() { Reset(); }

    void Reset()
    {
        m_lockCnt = 0;
        m_sp = 0;
        m_ptr.reset();
        m_unlock = false;
    }

    Type* New()
    {
        m_ptr.reset(new Type);
        return m_ptr.get();
    }

    inline Type* Get() { return m_ptr.get(); }
};


class tsVideoEncoder : virtual public tsSession, virtual public tsSurfacePool
{
public:
    bool                        m_default;
    bool                        m_initialized;
    bool                        m_loaded;
    tsExtBufType<mfxVideoParam> m_par;
    tsExtBufType<mfxBitstream>  m_bitstream;
    tsExtBufType<mfxEncodeCtrl> m_ctrl;
    mfxFrameAllocRequest        m_request;
    mfxVideoParam*              m_pPar;
    mfxVideoParam*              m_pParOut;
    mfxBitstream*               m_pBitstream;
    mfxFrameAllocRequest*       m_pRequest;
    mfxSyncPoint*               m_pSyncPoint;
    mfxFrameSurface1*           m_pSurf;
    mfxEncodeCtrl*              m_pCtrl;
    tsSurfaceProcessor*         m_filler;
    tsBitstreamProcessor*       m_bs_processor;
    mfxU32                      m_frames_buffered;
    mfxPluginUID*               m_uid;
    bool                        m_single_field_processing;
    mfxU16                      m_field_processed;
    bool                        m_bUseDefaultFrameType;
    std::list<tsSharedCtrl>     m_ctrl_list;
    std::list<tsSharedCtrl>     m_ctrl_reorder_buffer;
    tsSharedCtrl                m_ctrl_next;

    tsVideoEncoder(mfxU32 CodecId = 0, bool useDefaults = true);
    tsVideoEncoder(mfxFeiFunction func, mfxU32 CodecId = 0, bool useDefaults = true);
    ~tsVideoEncoder();

    mfxStatus Init();
    mfxStatus Init(mfxSession session, mfxVideoParam *par);

    mfxStatus GetCaps(void *pCaps, mfxU32 *pCapsSize);

    mfxStatus Close();
    mfxStatus Close(mfxSession session);

    mfxStatus Query();
    mfxStatus Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);

    mfxStatus QueryIOSurf();
    mfxStatus QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);

    mfxStatus Reset();
    mfxStatus Reset(mfxSession session, mfxVideoParam *par);

    mfxStatus GetVideoParam();
    mfxStatus GetVideoParam(mfxSession session, mfxVideoParam *par);

    mfxStatus EncodeFrameAsync();
    mfxStatus EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp);

    mfxStatus AllocBitstream(mfxU32 size = 0);
    mfxStatus AllocSurfaces();


    mfxStatus SyncOperation();
    mfxStatus SyncOperation(mfxSyncPoint syncp);
    mfxStatus SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);

    mfxStatus EncodeFrames(mfxU32 n, bool check=false);

    mfxStatus InitAndSetAllocator();

    mfxStatus DrainEncodedBitstream();

    mfxStatus Load() { m_loaded = (0 == tsSession::Load(m_session, m_uid, 1)); return g_tsStatus.get(); }
};

class tsAutoFlush : tsAutoFlushFiller
{
public:
    tsAutoFlush(tsVideoEncoder& enc, mfxU32 n_frames)
        : tsAutoFlushFiller(n_frames)
        , m_enc(enc)
    {
        m_enc.m_filler = this;
    }

    ~tsAutoFlush()
    {
        m_enc.m_filler = 0;
    }

private:
    tsVideoEncoder& m_enc;
};


typedef struct tagENCODE_CAPS_HEVC
{
    union {
        struct {
            mfxU32    CodingLimitSet : 1;
            mfxU32    BitDepth8Only : 1;
            mfxU32    Color420Only : 1;
            mfxU32    SliceStructure : 3;
            mfxU32    SliceIPOnly : 1;
            mfxU32    SliceIPBOnly : 1;
            mfxU32    NoWeightedPred : 1;
            mfxU32    NoMinorMVs : 1;
            mfxU32    RawReconRefToggle : 1;
            mfxU32    NoInterlacedField : 1;
            mfxU32    BRCReset : 1;
            mfxU32    RollingIntraRefresh : 1;
            mfxU32    UserMaxFrameSizeSupport : 1;
            mfxU32    FrameLevelRateCtrl : 1;
            mfxU32    SliceByteSizeCtrl : 1;
            mfxU32    VCMBitRateControl : 1;
            mfxU32    ParallelBRC : 1;
            mfxU32    TileSupport : 1;
            mfxU32    SkipFrame : 1;
            mfxU32    MbQpDataSupport : 1;
            mfxU32    SliceLevelWeightedPred : 1;
            mfxU32    LumaWeightedPred : 1;
            mfxU32    ChromaWeightedPred : 1;
            mfxU32    QVBRBRCSupport : 1;
            mfxU32    HMEOffsetSupport : 1;
            mfxU32    YUV422ReconSupport : 1;
            mfxU32    YUV444ReconSupport : 1;
            mfxU32    RGBReconSupport : 1;
            mfxU32    MaxEncodedBitDepth : 2;
        };
        mfxU32    CodingLimits;
    };

    mfxU32    MaxPicWidth;
    mfxU32    MaxPicHeight;
    mfxU8   MaxNum_Reference0;
    mfxU8   MaxNum_Reference1;
    mfxU8   MBBRCSupport;
    mfxU8   TUSupport;

    union {
        struct {
            mfxU8    MaxNumOfROI : 5; // [0..16]
            mfxU8    ROIBRCPriorityLevelSupport : 1;
            mfxU8    BlockSize : 2;
        };
        mfxU8    ROICaps;
    };

    union {
        struct {
            mfxU32    SliceLevelReportSupport : 1;
            mfxU32    CTULevelReportSupport : 1;
            mfxU32    SearchWindow64Support : 1;
            mfxU32    reserved : 2;
            mfxU32    IntraRefreshBlockUnitSize : 2;
            mfxU32    LCUSizeSupported : 3;
            mfxU32    MaxNumDeltaQP : 4;
            mfxU32    DirtyRectSupport : 1;
            mfxU32    MoveRectSupport : 1;
            mfxU32    FrameSizeToleranceSupport : 1;
            mfxU32    HWCounterAutoIncrementSupport : 2;
            mfxU32    ROIDeltaQPSupport : 1;
            mfxU32    MaxNumOfTileColumnsMinus1 : 5;
            mfxU32 : 7; // For future expansion
        };
        mfxU32    CodingLimits2;
    };

    mfxU8    MaxNum_WeightedPredL0;
    mfxU8    MaxNum_WeightedPredL1;
    mfxU16   MaxNumOfDirtyRect;
    mfxU16   MaxNumOfMoveRect;
} ENCODE_CAPS_HEVC;
