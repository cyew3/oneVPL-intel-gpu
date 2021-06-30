////////////////////////////////////////////////////////////////////////////
////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
////     This software is supplied under the terms of a license agreement or
////     nondisclosure agreement with Intel Corporation and may not be copied
////     or disclosed except in accordance with the terms of that agreement.
////          Copyright(c) 2017-2021 Intel Corporation. All Rights Reserved.
////

#pragma once

#include "ts_session.h"
#include "ts_ext_buffers.h"
#include "ts_surface.h"
#include "ts_utils.h"
#include <memory>

#if defined(_WIN32) || defined(_WIN64)
#include "guiddef.h"
#elif LIBVA_SUPPORT
#include <vaapi_utils.h>
#include <va/va.h>
#include <va/va_enc_hevc.h>
#include <va/va_enc_vp9.h>
#endif

#ifndef MFX_CHECK
#define MFX_CHECK(EXPR, ERR)    { if (!(EXPR)) return (ERR); }
#endif

enum {
    GMOCK_FOURCC_P012 = MFX_MAKEFOURCC('P', '0', '1', '2'),
    GMOCK_FOURCC_Y212 = MFX_MAKEFOURCC('Y', '2', '1', '2'),
    GMOCK_FOURCC_Y412 = MFX_MAKEFOURCC('Y', '4', '1', '2'),
};

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
        m_fo = 0;
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
    bool                        m_bCheckLowPowerAtInit = true;
    std::list<tsSharedCtrl>     m_ctrl_list;
    std::list<tsSharedCtrl>     m_ctrl_reorder_buffer;
    tsSharedCtrl                m_ctrl_next;

    tsVideoEncoder(mfxU32 CodecId = 0, bool useDefaults = true, MsdkPluginType type = MSDK_PLUGIN_TYPE_NONE);
    ~tsVideoEncoder();

    mfxStatus Init();
    mfxStatus Init(mfxSession session, mfxVideoParam *par);

#if defined(_WIN32) || defined(_WIN64)
    mfxStatus GetGuid(GUID &guid);
#elif LIBVA_SUPPORT
    mfxStatus IsModeSupported(VADisplay& device, mfxU16 codecProfile, mfxU32 lowpower);
    mfxStatus GetVACaps(VADisplay& device, void *pCaps, mfxU32 *pCapsSize);
#endif
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

    mfxU32  MaxPicWidth;
    mfxU32  MaxPicHeight;
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
            mfxU32    CustomRoundingControl : 1;
            mfxU32    ReservedBit1 : 1;
            mfxU32    IntraRefreshBlockUnitSize : 2;
            mfxU32    LCUSizeSupported : 3;
            mfxU32    MaxNumDeltaQP : 4;
            mfxU32    DirtyRectSupport : 1;
            mfxU32    MoveRectSupport : 1;
            mfxU32    FrameSizeToleranceSupport : 1;
            mfxU32    HWCounterAutoIncrementSupport : 2;
            mfxU32    ROIDeltaQPSupport : 1;
            mfxU32    NumScalablePipesMinus1 : 5;
            mfxU32    NegativeQPSupport : 1;
            mfxU32    ReservedBit2 : 1;
            mfxU32    TileBasedEncodingSupport : 1;
            mfxU32    PartialFrameUpdateSupport : 1;
            mfxU32    RGBEncodingSupport : 1;
            mfxU32    LLCStreamingBufferSupport : 1;
            mfxU32    DDRStreamingBufferSupport : 1;
        };
        mfxU32    CodingLimits2;
    };

    mfxU8    MaxNum_WeightedPredL0;
    mfxU8    MaxNum_WeightedPredL1;
    mfxU16   MaxNumOfDirtyRect;
    mfxU16   MaxNumOfMoveRect;
    mfxU16   MaxNumOfConcurrentFramesMinus1;
    mfxU16   LLCSizeInMBytes;
    mfxU16   reserved16bits0;
    mfxU32   reserved32bits1;
    mfxU32   reserved32bits2;
    mfxU32   reserved32bits3;
} ENCODE_CAPS_HEVC;


typedef struct tagENCODE_CAPS_AVC
{
    union
    {
        struct
        {
            mfxU32    CodingLimitSet : 1;
            mfxU32    NoFieldFrame : 1;
            mfxU32    NoCabacSupport : 1;
            mfxU32    NoGapInFrameCount : 1;
            mfxU32    NoGapInPicOrder : 1;
            mfxU32    NoUnpairedField : 1;
            mfxU32    BitDepth8Only : 1;
            mfxU32    ConsecutiveMBs : 1;
            mfxU32    SliceStructure : 3;
            mfxU32    SliceIPOnly : 1;
            mfxU32    SliceIPBOnly : 1;
            mfxU32    NoWeightedPred : 1;
            mfxU32    NoMinorMVs : 1;
            mfxU32    ClosedGopOnly : 1;
            mfxU32    NoFrameCropping : 1;
            mfxU32    FrameLevelRateCtrl : 1;
            mfxU32    HeaderInsertion : 1;
            mfxU32    RawReconRefToggle : 1; // doesn't shpport change reference frame (original vs. recon) at a per frame basis.
            mfxU32    NoPAFF : 1; // doesn't support change frame/field coding at per frame basis.
            mfxU32    NoInterlacedField : 1; // DDI 0.87
            mfxU32    BRCReset : 1;
            mfxU32    EnhancedEncInput : 1;
            mfxU32    RollingIntraRefresh : 1;
            mfxU32    UserMaxFrameSizeSupport : 1;
            mfxU32    LayerLevelRateCtrl : 1;
            mfxU32    SliceLevelRateCtrl : 1;
            mfxU32    VCMBitrateControl : 1;
            mfxU32    NoESS : 1;
            mfxU32    Color420Only : 1;
            mfxU32    ICQBRCSupport : 1;
        };
        mfxU32 CodingLimits;
    };
    union
    {
        struct
        {
            mfxU16 EncFunc : 1;
            mfxU16 PakFunc : 1;
            mfxU16 EncodeFunc : 1;  // Enc+Pak
            mfxU16 InputAnalysisFunc : 1;  // Reserved for now
            mfxU16 reserved : 12;
        };
        mfxU16 CodingFunction;
    };
    mfxU32  MaxPicWidth;
    mfxU32  MaxPicHeight;
    mfxU8   MaxNum_Reference;
    mfxU8   MaxNum_SPS_id;
    mfxU8   MaxNum_PPS_id;
    mfxU8   MaxNum_Reference1;
    mfxU8   MaxNum_QualityLayer;
    mfxU8   MaxNum_DependencyLayer;
    mfxU8   MaxNum_DQLayer;
    mfxU8   MaxNum_TemporalLayer;
    mfxU8   MBBRCSupport;
    union {
        struct {
            mfxU8 MaxNumOfROI : 5; // [0..16]
            mfxU8 : 1;
            mfxU8 ROIBRCDeltaQPLevelSupport : 1;
            mfxU8 ROIBRCPriorityLevelSupport : 1;
        };
        mfxU8 ROICaps;
    };

    union {
        struct {
            mfxU32    SkipFrame : 1;
            mfxU32    MbQpDataSupport : 1;
            mfxU32    SliceLevelWeightedPred : 1;
            mfxU32    LumaWeightedPred : 1;
            mfxU32    ChromaWeightedPred : 1;
            mfxU32    QVBRBRCSupport : 1;
            mfxU32    SliceLevelReportSupport : 1;
            mfxU32    HMEOffsetSupport : 1;
            mfxU32    DirtyRectSupport : 1;
            mfxU32    MoveRectSupport : 1;
            mfxU32    FrameSizeToleranceSupport : 1;
            mfxU32    HWCounterAutoIncrement : 2;
            mfxU32    MBControlSupport : 1;
            mfxU32    ForceRepartitionCheckSupport : 1;
            mfxU32    CustomRoundingControl : 1;
            mfxU32    LLCStreamingBufferSupport : 1;
            mfxU32    DDRStreamingBufferSupport : 1;
            mfxU32    LowDelayBRCSupport : 1;
            mfxU32    MaxNumDeltaQPMinus1 : 4;
            mfxU32    TCBRCSupport : 1;
            mfxU32    HRDConformanceSupport : 1;
            mfxU32    PollingModeSupport : 1;
            mfxU32    LookaheadBRCSupport : 1;
            mfxU32    QpAdjustmentSupport : 1;
            mfxU32    LookaheadAnalysisSupport : 1;
            mfxU32 : 3;
        };
        mfxU32      CodingLimits2;
    };
    mfxU8    MaxNum_WeightedPredL0;
    mfxU8    MaxNum_WeightedPredL1;
// AVC_DDI_VERSION_0952
    mfxU16   reserved16bits0;
    mfxU16   reserved16bits1;
    mfxU16   MaxNumOfConcurrentFramesMinus1;
    mfxU16   LLCSizeInMBytes;
    mfxU16   reserved16bits2;
    mfxU32     reserved32bits1;
    mfxU32     reserved32bits2;
    mfxU32     reserved32bits3;
// AVC_DDI_VERSION_0952
} ENCODE_CAPS_AVC;

typedef struct tagENCODE_CAPS_VP9
{
    union {
        struct {
            mfxU32    CodingLimitSet : 1;
            mfxU32    Color420Only : 1;
            mfxU32    ForcedSegmentationSupport : 1;
            mfxU32    FrameLevelRateCtrl : 1;
            mfxU32    BRCReset : 1;
            mfxU32    AutoSegmentationSupport : 1;
            mfxU32    TemporalLayerRateCtrl : 3;
            mfxU32    DynamicScaling : 1;
            mfxU32    TileSupport : 1;
            mfxU32    NumScalablePipesMinus1 : 3;
            mfxU32    YUV422ReconSupport : 1;
            mfxU32    YUV444ReconSupport : 1;
            mfxU32    MaxEncodedBitDepth : 2;
            mfxU32    UserMaxFrameSizeSupport : 1;
            mfxU32    SegmentFeatureSupport : 4;
            mfxU32    DirtyRectSupport : 1;
            mfxU32    MoveRectSupport : 1;
            mfxU32    : 7;
        };
        mfxU32    CodingLimits;
    };
    union {
        struct {
            mfxU8    EncodeFunc : 1;  // Enc+Pak
            mfxU8    HybridPakFunc : 1;  // Hybrid Pak function
            mfxU8    EncFunc : 1;  // Enc only function
            mfxU8    : 5;
        };
        mfxU8    CodingFunction;
    };
    mfxU32    MaxPicWidth;
    mfxU32    MaxPicHeight;
    mfxU16    MaxNumOfDirtyRect;
    mfxU16    MaxNumOfMoveRect;
} ENCODE_CAPS_VP9;
