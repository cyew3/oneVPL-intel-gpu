//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "math.h"
#include "mfx_vp9_encode_hw_utils.h"

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)

namespace MfxHwVP9Encode
{

VP9MfxVideoParam::VP9MfxVideoParam()
{
    Zero(*this);
}

VP9MfxVideoParam::VP9MfxVideoParam(VP9MfxVideoParam const & par)
{
    Construct(par);
}

VP9MfxVideoParam::VP9MfxVideoParam(mfxVideoParam const & par)
{
    Construct(par);
}

VP9MfxVideoParam& VP9MfxVideoParam::operator=(VP9MfxVideoParam const & par)
{
    Construct(par);

    return *this;
}

VP9MfxVideoParam& VP9MfxVideoParam::operator=(mfxVideoParam const & par)
{
    Construct(par);

    return *this;
}

void VP9MfxVideoParam::CalculateInternalParams()
{
    mfxVideoParam & base = *this;

    if (base.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        (base.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (m_extOpaque.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
    {
        m_inMemType = INPUT_SYSTEM_MEMORY;
    }
    else
    {
        m_inMemType = INPUT_VIDEO_MEMORY;
    }
}

void VP9MfxVideoParam::Construct(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;

    Zero(m_extParam);

    InitExtBufHeader(m_extOpt);
    InitExtBufHeader(m_extOpaque);

    if (mfxExtCodingOptionVP9 * opts = GetExtBuffer(par))
        m_extOpt = *opts;

    if (mfxExtOpaqueSurfaceAlloc * opts = GetExtBuffer(par))
        m_extOpaque = *opts;

    m_extParam[0]  = &m_extOpt.Header;
    m_extParam[1]  = &m_extOpaque.Header;

    ExtParam = m_extParam;
    NumExtParam = mfxU16(sizeof m_extParam / sizeof m_extParam[0]);
    assert(NumExtParam == NUM_OF_SUPPORTED_EXT_BUFFERS);

    CalculateInternalParams();
}

bool isVideoSurfInput(mfxVideoParam const & video)
{
    mfxExtOpaqueSurfaceAlloc * pOpaq = GetExtBuffer(video);

    if (video.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
        return true;
    if (isOpaq(video) && pOpaq)
    {
        if (pOpaq->In.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            return true;
        }
    }
    return false;
}

mfxU32 ModifyLoopFilterLevelQPBased(mfxU32 QP, mfxU32 loopFilterLevel)
{
    if (loopFilterLevel)
        return loopFilterLevel;

    if(QP >= 40) {
        return (int)(-18.98682 + 0.3967082*(float) QP + 0.0005054*pow((float) QP-127.5, 2) - 9.692e-6*pow((float) QP-127.5, 3));
    } else {
        return  QP/4;
    }
}

mfxStatus InitVp9SeqLevelParam(VP9MfxVideoParam const &video, VP9SeqLevelParam &param)
{
    video;
    Zero(param);

    param.profile = PROFILE_0;
    param.bitDepth = BITDEPTH_8;
    param.colorSpace = UNKNOWN_COLOR_SPACE;
    param.colorRange = 0; // BT.709-6
    param.subsamplingX = 1;
    param.subsamplingY = 1;

    return MFX_ERR_NONE;
};

mfxStatus SetFramesParams(VP9MfxVideoParam const &par,
                          mfxU16 forcedFrameType,
                          mfxU32 frameOrder,
                          VP9FrameLevelParam &frameParam,
                          mfxCoreInterface const * pCore)
{
    Zero(frameParam);
    frameParam.frameType = (mfxU8)((frameOrder % par.mfx.GopPicSize) == 0 || (forcedFrameType & MFX_FRAMETYPE_I) ? KEY_FRAME : INTER_FRAME);

    mfxExtCodingOptionVP9 const &opt = GetExtBufferRef(par);

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        frameParam.baseQIndex = mfxU8(frameParam.frameType == KEY_FRAME ? par.mfx.QPI : par.mfx.QPP);
    }

    frameParam.lfLevel   = (mfxU8)ModifyLoopFilterLevelQPBased(frameParam.baseQIndex, 0); // always 0 is passes since at the moment there is no LF level in MSDK API
    frameParam.sharpness = (mfxU8)opt.SharpnessLevel;

    frameParam.width  = frameParam.renderWidth = par.mfx.FrameInfo.Width;
    frameParam.height = frameParam.renderHeight = par.mfx.FrameInfo.Height;

    for (mfxU8 i = 0; i < 4; i ++)
    {
        frameParam.lfRefDelta[i] = (mfxI8)opt.LoopFilterRefDelta[i];
    }

    frameParam.lfModeDelta[0] = (mfxI8)opt.LoopFilterModeDelta[0];
    frameParam.lfModeDelta[1] = (mfxI8)opt.LoopFilterModeDelta[1];

    frameParam.qIndexDeltaLumaDC   = (mfxI8)opt.QIndexDeltaLumaDC;
    frameParam.qIndexDeltaChromaAC = (mfxI8)opt.QIndexDeltaChromaAC;
    frameParam.qIndexDeltaChromaDC = (mfxI8)opt.QIndexDeltaChromaDC;

    frameParam.numSegments = 1; // TODO: add segmentation support

    frameParam.showFarme = 1;
    frameParam.intraOnly = 0;

    frameParam.modeRefDeltaEnabled = 0; // TODO: add support of ref and mode LF deltas
    frameParam.errorResilentMode = 0;
    frameParam.resetFrameContext = 0;
#if defined (PRE_SI_TARGET_PLATFORM_GEN11)
    mfxPlatform platform;
    pCore->QueryPlatform(pCore->pthis, &platform);
    if (platform.CodeName == MFX_PLATFORM_ICELAKE)
    {
        frameParam.refreshFrameContext = 0;  // ICL has a problems with HuC, so it's disabled by default. Need to disable refresh of CABAC contexts untill HuC is fixed.
    }
    else
    {
        frameParam.refreshFrameContext = 1;
    }
#else //PRE_SI_TARGET_PLATFORM_GEN11
    pCore;
    frameParam.refreshFrameContext = 1;
#endif //PRE_SI_TARGET_PLATFORM_GEN11

    frameParam.allowHighPrecisionMV = 1;

    mfxU16 alignedWidth = ALIGN_POWER_OF_TWO(par.mfx.FrameInfo.Width, 3); // align to Mode Info block size (8 pixels)
    mfxU16 alignedHeight = ALIGN_POWER_OF_TWO(par.mfx.FrameInfo.Height, 3); // align to Mode Info block size (8 pixels)

    frameParam.modeInfoRows = alignedWidth >> 3;
    frameParam.modeInfoCols = alignedHeight >> 3;

    return MFX_ERR_NONE;
}

mfxStatus DecideOnRefListAndDPBRefresh(mfxVideoParam const & par, Task *pTask, std::vector<sFrameEx*>&dpb, VP9FrameLevelParam &frameParam)
{
    dpb; par;
    bool multiref = (par.mfx.NumRefFrame == 3);
    if (multiref == true)
    {
#if 0
        // idx 2 is dedicated to GOLD, idxs 0 and 1 are for LAST and ALT in round robin mode
        mfxU8 prevLastRefIdx = 1;
        frameParam.refList[REF_GOLD] = 2;
        frameParam.refList[REF_ALT] = prevLastRefIdx;
        frameParam.refList[REF_LAST] = 1 - prevLastRefIdx;
        prevLastRefIdx = frameParam.refList[REF_LAST];
#else
        mfxU8 frameCount = pTask->m_frameOrder % 8;
        frameParam.refList[REF_ALT] = 0;
        frameParam.refList[REF_GOLD] = frameParam.refList[REF_LAST] = frameCount;
        if (frameCount)
        {
            frameParam.refList[REF_LAST] = frameParam.refList[REF_GOLD] = frameCount - 1;
            if (frameCount > 1)
            {
                frameParam.refList[REF_GOLD] = frameCount - 2;
            }
        }
#endif

        if (frameParam.frameType == KEY_FRAME)
        {
            memset(frameParam.refreshRefFrames, 1, DPB_SIZE);
        }
        else
        {
            /*if((pTask->m_frameOrder & 0x7) == 0)
            {
                frameParam.refreshRefFrames[frameParam.refList[REF_GOLD]] = 1;
            }*/
            frameParam.refreshRefFrames[frameCount] = 1;
        }
        frameCount = (frameCount + 1) % 8;
    }
    else
    {
        // single ref
        frameParam.refList[REF_GOLD] = frameParam.refList[REF_ALT] = frameParam.refList[REF_LAST] = 0;
        if (frameParam.frameType == KEY_FRAME)
        {
            memset(frameParam.refreshRefFrames, 1, DPB_SIZE);
        }
        else
        {
            frameParam.refreshRefFrames[0] = 1;
        }

        memset(&frameParam.refBiases[0], 0, REF_TOTAL);
    }

    return MFX_ERR_NONE;
}

mfxStatus UpdateDpb(VP9FrameLevelParam &frameParam, sFrameEx *pRecFrame, std::vector<sFrameEx*>&dpb, mfxCoreInterface* pCore)
{
    for (mfxU8 i = 0; i < dpb.size(); i++)
    {
        if (frameParam.refreshRefFrames[i])
        {
            if (dpb[i])
            {
                dpb[i]->refCount--;
                if (dpb[i]->refCount == 0)
                {
                    mfxStatus sts = FreeSurface(dpb[i], pCore);
                    MFX_CHECK_STS(sts);
                }
            }
            dpb[i] = pRecFrame;
            dpb[i]->refCount++;
        }
    }
    return MFX_ERR_NONE;
}

//---------------------------------------------------------
// service class: MfxFrameAllocResponse
//---------------------------------------------------------

MfxFrameAllocResponse::MfxFrameAllocResponse()
{
    Zero(*this);
}

MfxFrameAllocResponse::~MfxFrameAllocResponse()
{
    Release();
}


mfxStatus MfxFrameAllocResponse::Alloc(
    mfxCoreInterface *     pCore,
    mfxFrameAllocRequest & req)
{
    req.NumFrameSuggested = req.NumFrameMin; // no need in 2 different NumFrames

    mfxCoreParam corePar = {};
    pCore->GetCoreParam(pCore->pthis, &corePar);

    if ((corePar.Impl & 0xF00) == MFX_IMPL_VIA_D3D11)
    {
        mfxFrameAllocRequest tmp = req;
        tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

        m_responseQueue.resize(req.NumFrameMin);
        m_mids.resize(req.NumFrameMin);

        for (int i = 0; i < req.NumFrameMin; i++)
        {
            mfxStatus sts = pCore->FrameAllocator.Alloc(pCore->FrameAllocator.pthis, &tmp, &m_responseQueue[i]);
            MFX_CHECK_STS(sts);

            m_mids[i] = m_responseQueue[i].mids[0];
        }

        mids = &m_mids[0];
        NumFrameActual = req.NumFrameMin;
    }
    else
    {
        mfxStatus sts = pCore->FrameAllocator.Alloc(pCore->FrameAllocator.pthis, &req, this);
        MFX_CHECK_STS(sts);
    }

    if (NumFrameActual < req.NumFrameMin)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_pCore = pCore;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin;
    m_info = req.Info;

    return MFX_ERR_NONE;
}

mfxStatus MfxFrameAllocResponse::Release()
{
    if (m_numFrameActualReturnedByAllocFrames == 0)
    {
        // nothing was allocated, nothig to do
        return MFX_ERR_NONE;
    }

    if (m_pCore == 0)
    {
        return MFX_ERR_NULL_PTR;
    }

    mfxCoreParam corePar = {};
    m_pCore->GetCoreParam(m_pCore->pthis, &corePar);

    if ((corePar.Impl & 0xF00) == MFX_IMPL_VIA_D3D11)
    {
        for (size_t i = 0; i < m_responseQueue.size(); i++)
        {
            mfxStatus sts = m_pCore->FrameAllocator.Free(m_pCore->FrameAllocator.pthis, &m_responseQueue[i]);
            MFX_CHECK_STS(sts);
        }

        m_responseQueue.resize(0);
        m_numFrameActualReturnedByAllocFrames = 0;
    }
    else
    {
        if (mids)
        {
            NumFrameActual = m_numFrameActualReturnedByAllocFrames;
            mfxStatus sts = m_pCore->FrameAllocator.Free(m_pCore->FrameAllocator.pthis, this);
            MFX_CHECK_STS(sts);

            m_numFrameActualReturnedByAllocFrames = 0;
        }
    }

    return MFX_ERR_NONE;
}

//---------------------------------------------------------
// service class: ExternalFrames
//---------------------------------------------------------

void ExternalFrames::Init(mfxU32 numFrames)
{
    m_frames.resize(numFrames);
    {
        mfxU32 i = 0;
        std::list<sFrameEx>::iterator frame = m_frames.begin();
        for ( ;frame!= m_frames.end(); frame++)
        {
            Zero(*frame);
            frame->idInPool = i++;
        }
    }
}

mfxStatus ExternalFrames::GetFrame(mfxFrameSurface1 *pInFrame, sFrameEx *&pOutFrame )
{
    std::list<sFrameEx>::iterator frame = m_frames.begin();
    for ( ;frame!= m_frames.end(); frame++)
    {
        if (frame->pSurface == 0)
        {
            frame->pSurface = pInFrame;
            pOutFrame = &*frame;
            return MFX_ERR_NONE;
        }
        if (frame->pSurface == pInFrame)
        {
            pOutFrame = &*frame;
            return MFX_ERR_NONE;
        }
    }

    sFrameEx newFrame = {};
    newFrame.idInPool = (mfxU32)m_frames.size();
    newFrame.pSurface = pInFrame;

    m_frames.push_back(newFrame);
    pOutFrame = &m_frames.back();

    return MFX_ERR_NONE;
}
//---------------------------------------------------------
// service class: InternalFrames
//---------------------------------------------------------

mfxStatus InternalFrames::Init(mfxCoreInterface *pCore, mfxFrameAllocRequest *pAllocReq)
{
    MFX_CHECK_NULL_PTR2 (pCore, pAllocReq);
    mfxU32 nFrames = pAllocReq->NumFrameMin;

    if (nFrames == 0)
    {
        return MFX_ERR_NONE;
    }

    //printf("internal frames init %d (request)\n", req.NumFrameSuggested);

    mfxStatus sts = MFX_ERR_NONE;
    sts = m_response.Alloc(pCore,*pAllocReq);
    MFX_CHECK_STS(sts);

    //printf("internal frames init %d (%d) [%d](response)\n", m_response.NumFrameActual,Num(),nFrames);

    m_surfaces.resize(nFrames);
    Zero(m_surfaces);

    //printf("internal frames init 1 [%d](response)\n", Num());

    m_frames.resize(nFrames);
    Zero(m_frames);

    //printf("internal frames init 2 [%d](response)\n", Num());

    for (mfxU32 i = 0; i < nFrames; i++)
    {
        m_frames[i].idInPool = i;
        m_frames[i].refCount = 0;
        m_surfaces[i].Data.MemId = m_response.mids[i];
        m_surfaces[i].Info = pAllocReq->Info;
        m_frames[i].pSurface = &m_surfaces[i];
    }
    return sts;
}

sFrameEx * InternalFrames::GetFreeFrame()
{
    std::vector<sFrameEx>::iterator frame = m_frames.begin();
    for (;frame != m_frames.end(); frame ++)
    {
        if (isFreeSurface(&frame[0]))
        {
            return &frame[0];
        }
    }
    return 0;
}

mfxStatus  InternalFrames::GetFrame(mfxU32 numFrame, sFrameEx * &Frame)
{
    MFX_CHECK(numFrame < m_frames.size(), MFX_ERR_UNDEFINED_BEHAVIOR);

    if (isFreeSurface(&m_frames[numFrame]))
    {
        Frame = &m_frames[numFrame];
        return MFX_ERR_NONE;
    }
    return MFX_WRN_DEVICE_BUSY;
}

mfxStatus InternalFrames::Release()
{
    mfxStatus sts = m_response.Release();
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

//---------------------------------------------------------
// service class: Task
//---------------------------------------------------------

mfxStatus GetRealSurface(
    mfxCoreInterface const *pCore,
    VP9MfxVideoParam const &par,
    Task const &task,
    mfxFrameSurface1 *& pSurface)
{
    if (par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        mfxStatus sts = pCore->GetRealSurface(pCore->pthis, task.m_pRawFrame->pSurface, &pSurface);
        MFX_CHECK_STS(sts);
    }
    else
    {
        pSurface = task.m_pRawFrame->pSurface;
    }

    return MFX_ERR_NONE;
}

mfxStatus GetInputSurface(
    mfxCoreInterface const *pCore,
    VP9MfxVideoParam const &par,
    Task const &task,
    mfxFrameSurface1 *& pSurface)
{
    if (task.m_pRawLocalFrame)
    {
        pSurface = task.m_pRawLocalFrame->pSurface;
    }
    else
    {
        mfxStatus sts = GetRealSurface(pCore, par, task, pSurface);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

mfxStatus CopyRawSurfaceToVideoMemory(
    mfxCoreInterface *pCore,
    VP9MfxVideoParam const &par,
    Task const &task)
{
    if (par.m_inMemType == INPUT_SYSTEM_MEMORY)
    {
        MFX_CHECK_NULL_PTR1(task.m_pRawLocalFrame);
        mfxFrameSurface1 *pDd3dSurf = task.m_pRawLocalFrame->pSurface;
        mfxFrameSurface1 *pSysSurface = 0;
        mfxStatus sts = GetRealSurface(pCore, par, task, pSysSurface);

        mfxFrameSurface1 lockedSurf = {};
        lockedSurf.Info = par.mfx.FrameInfo;

        if (pSysSurface->Data.Y == 0)
        {
            pCore->FrameAllocator.Lock(pCore->FrameAllocator.pthis, pSysSurface->Data.MemId, &lockedSurf.Data);
            pSysSurface = &lockedSurf;
        }

        sts = pCore->CopyFrame(pCore->pthis, pDd3dSurf, pSysSurface);
        MFX_CHECK_STS(sts);

        if (pSysSurface == &lockedSurf)
        {
            pCore->FrameAllocator.Unlock(pCore->FrameAllocator.pthis, pSysSurface->Data.MemId, &lockedSurf.Data);
        }
    }

    return MFX_ERR_NONE;
}

} // MfxHwVP9Encode

#endif // PRE_SI_TARGET_PLATFORM_GEN10
