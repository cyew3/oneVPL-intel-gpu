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

#if defined (AS_VP9E_PLUGIN)

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
                          VP9FrameLevelParam &frameParam)
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
    frameParam.refreshFrameContext = 1;
    frameParam.allowHighPrecisionMV = 1;

    mfxU16 alignedWidth = ALIGN_POWER_OF_TWO(par.mfx.FrameInfo.Width, 3); // align to Mode Info block size (8 pixels)
    mfxU16 alignedHeight = ALIGN_POWER_OF_TWO(par.mfx.FrameInfo.Height, 3); // align to Mode Info block size (8 pixels)

    frameParam.modeInfoRows = alignedWidth >> 3;
    frameParam.modeInfoCols = alignedHeight >> 3;

    return MFX_ERR_NONE;
}

mfxStatus DecideOnRefListAndDPBRefresh(mfxVideoParam * par, Task *pTask, std::vector<sFrameEx*>&dpb, VP9FrameLevelParam &frameParam)
{
    dpb; par;
    bool multiref = (par->mfx.NumRefFrame == 3);
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
                    MFX_CHECK_STS(FreeSurface(dpb[i], pCore));
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
    mfxStatus sts = pCore->FrameAllocator.Alloc(pCore->FrameAllocator.pthis, &req, this);
    MFX_CHECK_STS(sts);

    if (NumFrameActual < req.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;

    m_pCore = pCore;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin;
    m_info = req.Info;

    return MFX_ERR_NONE;
}

mfxStatus MfxFrameAllocResponse::Release()
{
    if (m_pCore == 0)
    {
        return MFX_ERR_NULL_PTR;
    }

    if (mids && m_numFrameActualReturnedByAllocFrames)
    {
        NumFrameActual = m_numFrameActualReturnedByAllocFrames;
        MFX_CHECK_STS(m_pCore->FrameAllocator.Free(m_pCore->FrameAllocator.pthis, this));

        m_numFrameActualReturnedByAllocFrames = 0;
    }

    return MFX_ERR_NONE;
}

//---------------------------------------------------------
// service class: ExternalFrames
//---------------------------------------------------------

void ExternalFrames::Init(mfxU32 numFrames)
{
    m_frames.resize(numFrames);
    Zero(m_frames);
    {
        mfxU32 i = 0;
        std::vector<sFrameEx>::iterator frame = m_frames.begin();
        for ( ;frame!= m_frames.end(); frame++)
        {
            frame->idInPool = i++;
        }
    }
}

mfxStatus ExternalFrames::GetFrame(mfxFrameSurface1 *pInFrame, sFrameEx *&pOutFrame )
{
    mfxStatus sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    std::vector<sFrameEx>::iterator frame = m_frames.begin();
    for ( ;frame!= m_frames.end(); frame++)
    {
        if (frame->pSurface == 0)
        {
            frame->pSurface = pInFrame; /*add frame to pool*/
            pOutFrame = &frame[0];
            return MFX_ERR_NONE;
        }
        if (frame->pSurface == pInFrame)
        {
            pOutFrame = &frame[0];
            return MFX_ERR_NONE;
        }
    }
    return sts;
}
//---------------------------------------------------------
// service class: InternalFrames
//---------------------------------------------------------

mfxStatus InternalFrames::Init(mfxCoreInterface *pCore, mfxFrameAllocRequest *pAllocReq, bool bHW)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR2 (pCore, pAllocReq);
    mfxU32 nFrames = pAllocReq->NumFrameMin;

    if (nFrames == 0) return sts;
    pAllocReq->Type = (mfxU16)(bHW ? MFX_MEMTYPE_D3D_INT: MFX_MEMTYPE_SYS_INT);

    //printf("internal frames init %d (request)\n", req.NumFrameSuggested);

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
    MFX_CHECK_STS(m_response.Release());

    return MFX_ERR_NONE;
}

//---------------------------------------------------------
// service class: Task
//---------------------------------------------------------

mfxStatus Task::GetOriginalSurface(mfxFrameSurface1 *& pSurface, bool &bExternal)
{
    pSurface = m_pRawFrame->pSurface;
    bExternal = true;
    return MFX_ERR_NONE;
}

mfxStatus Task::GetInputSurface(mfxFrameSurface1 *& pSurface, bool &bExternal)
{
    if (m_pRawLocalFrame)
    {
        pSurface = m_pRawLocalFrame->pSurface;
        bExternal = false;
    }
    else
    {
            MFX_CHECK_STS(GetOriginalSurface(pSurface, bExternal)) ;
    }
    return MFX_ERR_NONE;
}

mfxStatus Task::CopyInput()
{
    mfxStatus sts = MFX_ERR_NONE;

#if 0 // no support of system memory as input
    if (m_pRawLocalFrame)
    {
        mfxFrameSurface1 src={};
        mfxFrameSurface1 dst = *(m_pRawLocalFrame->pSurface);

        mfxFrameSurface1 * pInput = 0;
        bool bExternal = true;

        sts = GetOriginalSurface(pInput,  bExternal);
        MFX_CHECK_STS(sts);

        src.Data = pInput->Data;
        src.Info = pInput->Info;

        FrameLocker lockSrc(m_pCore, src.Data);
        FrameLocker lockDst(m_pCore, dst.Data);

        MFX_CHECK(src.Info.FourCC == MFX_FOURCC_YV12 || src.Info.FourCC == MFX_FOURCC_NV12, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(dst.Info.FourCC == MFX_FOURCC_NV12, MFX_ERR_UNDEFINED_BEHAVIOR);

        MFX_CHECK_NULL_PTR1(src.Data.Y);
        if (src.Info.FourCC == MFX_FOURCC_NV12)
        {
            MFX_CHECK_NULL_PTR1(src.Data.UV);
        }
        else
        {
            MFX_CHECK_NULL_PTR2(src.Data.U, src.Data.V);
        }

        MFX_CHECK_NULL_PTR2(dst.Data.Y, dst.Data.UV);

        MFX_CHECK(dst.Info.Width >= src.Info.Width, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(dst.Info.Height >= src.Info.Height, MFX_ERR_UNDEFINED_BEHAVIOR);

        mfxU32 srcPitch = src.Data.PitchLow + ((mfxU32)src.Data.PitchHigh << 16);
        mfxU32 dstPitch = dst.Data.PitchLow + ((mfxU32)dst.Data.PitchHigh << 16);

        mfxU32 roiWidth = src.Info.Width;
        mfxU32 roiHeight = src.Info.Height;

        // copy luma
        mfxU8 * srcLine = src.Data.Y;
        mfxU8 * dstLine = dst.Data.Y;
        for (mfxU32 line = 0; line < roiHeight; line ++)
        {
            memcpy_s(dstLine, roiWidth, srcLine, roiWidth);
            srcLine += srcPitch;
            dstLine += dstPitch;
        }

        // copy chroma (with color conversion if required)
        dstLine = dst.Data.UV;
        roiHeight >>= 1;
        if (src.Info.FourCC == MFX_FOURCC_NV12)
        {
            // for input NV12 just copy chroma
            srcLine = src.Data.UV;
            for (mfxU32 line = 0; line < roiHeight; line ++)
            {
                memcpy_s(dstLine, roiWidth, srcLine, roiWidth);
                srcLine += srcPitch;
                dstLine += dstPitch;
            }
        }
        else
        {
            // for YV12 color conversion is required
            mfxU8 * srcU = src.Data.U;
            mfxU8 * srcV = src.Data.V;
            roiWidth >>= 1;
            srcPitch >>= 1;
            for (mfxU32 line = 0; line < roiHeight; line ++)
            {
                for (mfxU32 pixel = 0; pixel < roiWidth; pixel ++)
                {
                    mfxU32 dstUVPosition = pixel << 1;
                    dstLine[dstUVPosition] = srcU[pixel];
                    dstLine[dstUVPosition + 1] = srcV[pixel];
                }
                srcU += srcPitch;
                srcV += srcPitch;
                dstLine += dstPitch;
            }
        }
    }
#endif
    return sts;
}

} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN
