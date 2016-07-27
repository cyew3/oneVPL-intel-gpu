/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "math.h"
#include "mfx_vp9_encode_hw_utils.h"

#if defined (AS_VP9E_PLUGIN)

namespace MfxHwVP9Encode
{

//---------------------------------------------------------
// service class: VP9MfxParam
//---------------------------------------------------------

VP9MfxParam::VP9MfxParam()
{
    memset(this, 0, sizeof(*this));
}

VP9MfxParam::VP9MfxParam(VP9MfxParam const & par)
{
    Construct(par);
}

VP9MfxParam::VP9MfxParam(mfxVideoParam const & par)
{
    Construct(par);
}

VP9MfxParam& VP9MfxParam::operator=(VP9MfxParam const & par)
{
    Construct(par);

    return *this;
}

VP9MfxParam& VP9MfxParam::operator=(mfxVideoParam const & par)
{
    Construct(par);

    return *this;
}

void VP9MfxParam::Construct(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;

    Zero(m_extParam);

    InitExtBufHeader(m_extOpt);
    InitExtBufHeader(m_extOpaque);
    InitExtBufHeader(m_extROI);

    if (mfxExtCodingOptionVP9 * opts = GetExtBuffer(par))
        m_extOpt = *opts;

    if (mfxExtOpaqueSurfaceAlloc * opts = GetExtBuffer(par))
        m_extOpaque = *opts;

    if (mfxExtEncoderROI * opts = GetExtBuffer(par))
        m_extROI = *opts;

    if (m_extOpt.EnableMultipleSegments == MFX_CODINGOPTION_UNKNOWN && m_extROI.NumROI)
        m_extOpt.EnableMultipleSegments = MFX_CODINGOPTION_ON;

    m_extParam[0]  = &m_extOpt.Header;
    m_extParam[1]  = &m_extOpaque.Header;
    m_extParam[2]  = &m_extROI.Header;

    ExtParam = m_extParam;
    NumExtParam = mfxU16(sizeof m_extParam / sizeof m_extParam[0]);
    assert(NumExtParam == 3);
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

mfxStatus SetFramesParams(VP9MfxParam const &par,
                          mfxU16 const forcedFrameType,
                          mfxU32 frameOrder,
                          sFrameParams *pFrameParams)
{
    memset(pFrameParams, 0, sizeof(sFrameParams));
    pFrameParams->bIntra  = (frameOrder % par.mfx.GopPicSize) == 0 || (forcedFrameType & MFX_FRAMETYPE_I) ? true: false;

    mfxExtCodingOptionVP9 *pOptVP9 = GetExtBuffer(par);
    mfxExtEncoderROI      *pExtRoi = GetExtBuffer(par);

    pFrameParams->QIndex = 0;
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        pFrameParams->QIndex = mfxU8(pFrameParams->bIntra ? par.mfx.QPI : par.mfx.QPP);
    }

    bool bDisableLoopFilter = false; // replacement for mfxExtVP9Tricks
    if (bDisableLoopFilter == false)
    {
        pFrameParams->LFLevel = (mfxU8)ModifyLoopFilterLevelQPBased(pFrameParams->QIndex, 0); // always 0 is passes since at the moment there is no LF level in MSDK API
        pFrameParams->Sharpness = (mfxU8)pOptVP9->SharpnessLevel;

        for (mfxU8 i = 0; i < 4; i ++)
        {
            pFrameParams->LFRefDelta[i] = (mfxI8)pOptVP9->LoopFilterRefDelta[i];
        }

        pFrameParams->LFModeDelta[0] = (mfxI8)pOptVP9->LoopFilterModeDelta[0];
        pFrameParams->LFModeDelta[1] = (mfxI8)pOptVP9->LoopFilterModeDelta[1];
    }

    pFrameParams->QIndexDeltaLumaDC   = (mfxI8)pOptVP9->QIndexDeltaLumaDC;
    pFrameParams->QIndexDeltaChromaAC = (mfxI8)pOptVP9->QIndexDeltaChromaAC;
    pFrameParams->QIndexDeltaChromaDC = (mfxI8)pOptVP9->QIndexDeltaChromaDC;

    if (pOptVP9->EnableMultipleSegments && pExtRoi->NumROI)
    {
        pFrameParams->NumSegments = mfxU8(IPP_MIN(pExtRoi->NumROI, MAX_SEGMENTS));
        for (mfxU8 i = 0; i < pFrameParams->NumSegments; i ++)
        {
            mfxSegmentParamVP9 & segPar = pOptVP9->Segment[i];
            if (pExtRoi->ROI[i].Priority)
            {
                pFrameParams->QIndexDeltaSeg[i] = (mfxI8)pExtRoi->ROI[i].Priority;
            }
            else
            {
                pFrameParams->QIndexDeltaSeg[i] = (mfxI8)segPar.QIndexDelta;
            }

            pFrameParams->LFDeltaSeg[i] = (mfxI8)segPar.LoopFilterLevelDelta;
        }
    }
    else
    {
        pFrameParams->NumSegments = 1;
    }

    return MFX_ERR_NONE;
}

mfxStatus DecideOnRefListAndDPBRefresh(mfxVideoParam * par, Task *pTask, std::vector<sFrameEx*>&dpb, sFrameParams *pFrameParams)
{
    dpb; par;
    bool bDisableMultiref = true; // replacement for mfxExtVP9Tricks
    if (bDisableMultiref == false)
    {
#if 0
        // idx 2 is dedicated to GOLD, idxs 0 and 1 are for LAST and ALT in round robin mode
        mfxU8 prevLastRefIdx = 1;
        pFrameParams->refList[REF_GOLD] = 2;
        pFrameParams->refList[REF_ALT] = prevLastRefIdx;
        pFrameParams->refList[REF_LAST] = 1 - prevLastRefIdx;
        prevLastRefIdx = pFrameParams->refList[REF_LAST];
#else
        mfxU8 frameCount = pTask->m_frameOrder % 8;
        pFrameParams->refList[REF_ALT] = 0;
        pFrameParams->refList[REF_GOLD] = pFrameParams->refList[REF_LAST] = frameCount;
        if (frameCount)
        {
            pFrameParams->refList[REF_LAST] = pFrameParams->refList[REF_GOLD] = frameCount - 1;
            if (frameCount > 1)
            {
                pFrameParams->refList[REF_GOLD] = frameCount - 2;
            }
        }
#endif

        if (pFrameParams->bIntra)
        {
            memset(pFrameParams->refreshRefFrames, 1, DPB_SIZE);
        }
        else
        {
            /*if((pTask->m_frameOrder & 0x7) == 0)
            {
                pFrameParams->refreshRefFrames[pFrameParams->refList[REF_GOLD]] = 1;
            }*/
            pFrameParams->refreshRefFrames[frameCount] = 1;
        }
        frameCount = (frameCount + 1) % 8;
    }
    else
    {
        // single ref
        pFrameParams->refList[REF_GOLD] = pFrameParams->refList[REF_ALT] = pFrameParams->refList[REF_LAST] = 0;
        if (pFrameParams->bIntra)
        {
            memset(pFrameParams->refreshRefFrames, 1, DPB_SIZE);
        }
        else
        {
            pFrameParams->refreshRefFrames[0] = 1;
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
    if (m_pCore == 0)
        return;

    if (mids)
    {
        NumFrameActual = m_numFrameActualReturnedByAllocFrames;
        m_pCore->FrameAllocator.Free(m_pCore->FrameAllocator.pthis, this);
    }
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

//---------------------------------------------------------
// service class: ExternalFrames
//---------------------------------------------------------

void ExternalFrames::Init()
{
    m_frames.resize(1000);
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
            memcpy(dstLine, srcLine, roiWidth);
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
                memcpy(dstLine, srcLine, roiWidth);
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
    return sts;
}

mfxStatus Task::Init(mfxCoreInterface * pCore, mfxVideoParam *par)
{
    MFX_CHECK(m_status == TASK_FREE, MFX_ERR_UNDEFINED_BEHAVIOR);

    m_pCore       = pCore;
    m_bOpaqInput  = isOpaq(*par);

    return MFX_ERR_NONE;
}

mfxStatus Task::InitTask(   sFrameEx     *pRawFrame,
                            mfxBitstream *pBitsteam,
                            mfxU32        frameOrder)
{
    MFX_CHECK_NULL_PTR1(m_pCore);

    m_status        = TASK_FREE;
    m_pRawFrame     = pRawFrame;
    m_pBitsteam     = pBitsteam;
    m_frameOrder    = frameOrder;

    Zero(m_sFrameParams);

    MFX_CHECK_STS(LockSurface(m_pRawFrame,m_pCore));

    m_status = TASK_INITIALIZED;

    return MFX_ERR_NONE;
}

mfxStatus Task::SubmitTask (sFrameEx*  pRecFrame, std::vector<sFrameEx*> &dpb, sFrameParams* pParams, sFrameEx* pRawLocalFrame, sFrameEx* pOutBs)
{
    MFX_CHECK(m_status == TASK_INITIALIZED, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR2(pRecFrame, pParams);
    MFX_CHECK(isFreeSurface(pRecFrame),MFX_ERR_UNDEFINED_BEHAVIOR);

    //printf("Task::SubmitTask\n");

    m_sFrameParams   = *pParams;
    m_pRecFrame      = pRecFrame;
    m_pRawLocalFrame = pRawLocalFrame;
    m_pOutBs         = pOutBs;

    MFX_CHECK_STS (CopyInput());

    m_pRecFrame->pSurface->Data.FrameOrder = m_frameOrder;

    if (!m_sFrameParams.bIntra)
    {
        mfxU8 idxLast = pParams->refList[REF_LAST];
        mfxU8 idxGold = pParams->refList[REF_GOLD];
        mfxU8 idxAlt  = pParams->refList[REF_ALT];
        m_pRecRefFrames[REF_LAST] = dpb[idxLast];
        m_pRecRefFrames[REF_GOLD] = dpb[idxGold] != dpb[idxLast] ? dpb[idxGold] : 0;
        m_pRecRefFrames[REF_ALT]  = dpb[idxAlt]  != dpb[idxLast] && dpb[idxAlt]  != dpb[idxGold] ? dpb[idxAlt] : 0;
    }

    MFX_CHECK_STS(LockSurface(m_pRecFrame, m_pCore));
    MFX_CHECK_STS(LockSurface(m_pRawLocalFrame, m_pCore));
    MFX_CHECK_STS(LockSurface(m_pOutBs, m_pCore));

    m_status = TASK_SUBMITTED;

    return MFX_ERR_NONE;
}

mfxStatus Task::FreeTask()
{
    //printf("Task::FreeTask\n");

    MFX_CHECK_STS(FreeSurface(m_pRawFrame,m_pCore));
    MFX_CHECK_STS(FreeSurface(m_pRawLocalFrame,m_pCore));
    MFX_CHECK_STS(FreeSurface(m_pOutBs, m_pCore));

    m_pBitsteam     = 0;
    Zero(m_sFrameParams);
    Zero(m_ctrl);
    m_status = TASK_FREE;

    return MFX_ERR_NONE;
}

} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN
