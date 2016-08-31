/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#ifdef MFX_ENABLE_H264_VIDEO_ENCODE_HW

#include <algorithm>
#include <functional>

#include "vm_time.h"

#include "mfx_session.h"
#include "mfx_task.h"
#include "mfx_ext_buffers.h"
#include "mfx_h264_encode_hw.h"
#include "mfx_h264_enc_common_hw.h"
#include "mfx_h264_encode_hw_utils.h"
#include "mfx_svc_encode_simulcast_over_avc.h"

#include "ipps.h"

using namespace MfxHwH264Encode;

TaskManagerSvc::TaskManagerSvc()
: m_core(0)
, m_video(0)
{
}

TaskManagerSvc::~TaskManagerSvc()
{
    Close();
}

void TaskManagerSvc::Init(
    VideoCORE *           core,
    MfxVideoParam const & video)
{
    m_core    = core;
    m_video   = &video;
    m_taskNum = video.mfx.GopRefDist + (video.AsyncDepth - 1);

    mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(video);
    m_layerNum = 0;
    for (mfxU32 did = 0; did < video.calcParam.numDependencyLayer; did++)
        m_layerNum += extSvc->DependencyLayer[did].QualityNum;

    m_layer.reset(new TaskManager[m_layerNum]);
    m_task.reset(new SvcTask[m_taskNum]);

    for (mfxU32 i = 0; i < m_layerNum; i++)
        m_layer[i].Init(core, video, 0);

    m_dqid.resize(m_layerNum);
    mfxU32 i = 0;
    for (mfxU32 did = 0; did < video.calcParam.numDependencyLayer; did++)
        for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++, i++)
            m_dqid[i] = std::make_pair(did, qid);

    for (mfxU32 i = 0; i < m_taskNum; i++)
        m_task[i].Allocate(m_layerNum);

    m_currentTask = 0;
    m_currentLayerId  = 0;
}

void TaskManagerSvc::Reset(MfxVideoParam const & video)
{
    m_video = &video;

    for (mfxU32 i = 0; i < m_layerNum; i++)
        m_layer[i].Reset(video);
}

void TaskManagerSvc::Close()
{
    m_layer.reset(0);
    m_task.reset(0);
}

mfxStatus TaskManagerSvc::AssignTask(
    mfxEncodeCtrl *    ctrl,
    mfxFrameSurface1 * surface,
    mfxBitstream *     bs,
    SvcTask *&         newTask)
{
    UMC::AutomaticUMCMutex guard(m_mutex);
    mfxStatus sts = MFX_ERR_NONE;

    if (surface == 0 && m_currentLayerId != 0)
        return Error(MFX_ERR_UNDEFINED_BEHAVIOR);

    if (m_currentLayerId == m_video->calcParam.did[0])
    {
        m_currentTask = std::find_if(
            m_task.cptr(),
            m_task.cptr() + m_taskNum,
            std::mem_fun_ref(&DdiTask::IsFree));

        if (m_currentTask == m_task.cptr() + m_taskNum)
            return Warning(MFX_WRN_DEVICE_BUSY);

        for (mfxU32 i = 0; i < m_currentTask->LayerNum(); i++)
            (*m_currentTask)[i] = 0;
    }

    newTask = m_currentTask;

    if (surface == 0)
    {
        for (; m_currentLayerId < m_layerNum; ++m_currentLayerId)
        {
            mfxStatus sts = AssignAndConfirmAvcTask(ctrl, 0, bs, m_currentLayerId);
            MFX_CHECK_STS(sts);
        }
    }
    else
    {
        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(*m_video);
        for (mfxU32 i = 0; i < extSvc->DependencyLayer[surface->Info.FrameId.DependencyId].QualityNum; ++i, ++m_currentLayerId)
            sts = AssignAndConfirmAvcTask(ctrl, surface, bs, m_currentLayerId);
        if (sts == MFX_ERR_MORE_DATA)
            m_currentLayerId %= m_layerNum;
        MFX_CHECK_STS(sts);
    }

    if (m_currentLayerId < m_layerNum)
        return MFX_ERR_MORE_DATA;

    for (mfxU32 i = 0; i < m_currentTask->LayerNum(); i++)
        (*m_currentTask)[i]->m_bs = bs;

    m_currentTask->SetFree(false);
    m_currentLayerId = 0;



    // hack
    mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(*m_video);
    for (mfxU32 layer = 0, did = 0; did < m_video->calcParam.numDependencyLayer; did++)
    {
        SvcTask & task = *m_currentTask;
        if (m_video->IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            task[layer]->m_idx = task[layer]->m_idx + CalcNumSurfRaw(*m_video) * did;
        for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++, layer++)
        {
            task[layer]->m_idxBs[0] = task[layer]->m_idxBs[0] + CalcNumSurfBitstream(*m_video) * layer;
            task[layer]->m_idxRecon = task[layer]->m_idxRecon + CalcNumSurfRecon(*m_video) * layer;

            mfxU32 bestQLayer = did + 1 < m_video->calcParam.numDependencyLayer
                ? m_video->calcParam.numLayerOffset[did + 1] - 1
                : m_video->calcParam.numLayersTotal - 1;
            mfxU32 worstQLayer = m_video->calcParam.numLayerOffset[did];

            mfxU32 refQLayer = (task[layer]->m_type[0] & MFX_FRAMETYPE_KEYPIC) ? worstQLayer : bestQLayer;

            for (mfxU32 i = 0; i < task[layer]->m_dpb[0].Size(); i++)
                task[layer]->m_dpb[0][i].m_frameIdx = mfxU8(task[layer]->m_dpb[0][i].m_frameIdx + CalcNumSurfRecon(*m_video) * refQLayer);
        }
    }

    return MFX_ERR_NONE;
}

void TaskManagerSvc::CompleteTask(SvcTask & task)
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    // hack back
    mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(*m_video);
    for (mfxU32 layer = 0, did = 0; did < m_video->calcParam.numDependencyLayer; did++)
    {
        if (m_video->IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            task[layer]->m_idx = task[layer]->m_idx % CalcNumSurfRaw(*m_video);
        for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++, layer++)
        {
            task[layer]->m_idxBs[0] = task[layer]->m_idxBs[0] % CalcNumSurfBitstream(*m_video);
            task[layer]->m_idxRecon = task[layer]->m_idxRecon % CalcNumSurfRecon(*m_video);
            for (mfxU32 i = 0; i < task[layer]->m_dpb[0].Size(); i++)
                task[layer]->m_dpb[0][i].m_frameIdx = mfxU8(task[layer]->m_dpb[0][i].m_frameIdx % CalcNumSurfRecon(*m_video));
        }
    }

    for (mfxU32 i = 0; i < m_layerNum; i++)
        m_layer[i].CompleteTask(*task[i]);

    task.SetFree(true);
}

namespace
{
    mfxU8 GetSvcQpValue(
        mfxExtSVCRateControl const & extRc,
        DdiTask const &              task,
        mfxU32                       fieldId)
    {
        if (extRc.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            for (mfxU32 i = 0; i < extRc.NumLayers; i++)
            {
                if (extRc.Layer[i].DependencyId == task.m_did &&
                    extRc.Layer[i].QualityId    == task.m_qid &&
                    extRc.Layer[i].TemporalId   == task.m_tid)
                {
                    switch (task.m_type[fieldId] & MFX_FRAMETYPE_IPB)
                    {
                    case MFX_FRAMETYPE_I: return mfxU8(extRc.Layer[i].Cqp.QPI);
                    case MFX_FRAMETYPE_P: return mfxU8(extRc.Layer[i].Cqp.QPP);
                    case MFX_FRAMETYPE_B: return mfxU8(extRc.Layer[i].Cqp.QPB);
                    default: assert(!"bad frame type (GetSvcQpValue)"); return 26;
                    }
                }
            }
        }
        return 26;
    }
};

mfxStatus TaskManagerSvc::AssignAndConfirmAvcTask(
    mfxEncodeCtrl *    ctrl,
    mfxFrameSurface1 * surface,
    mfxBitstream *     bs,
    mfxU32             layerId)
{
    mfxStatus sts = m_layer[layerId].AssignTask(ctrl, surface, bs, (*m_currentTask)[layerId]);
    MFX_CHECK_STS(sts);

    mfxU32 did = m_dqid[layerId].first;
    mfxU32 qid = m_dqid[layerId].second;

    (*m_currentTask)[layerId]->m_did = did;
    (*m_currentTask)[layerId]->m_qid = qid;
    if (layerId > 0)
        (*m_currentTask)[layerId]->m_idxInterLayerRecon = (*m_currentTask)[layerId - 1]->m_idxRecon;

    mfxExtSVCRateControl const * extRc = GetExtBuffer(*m_video);
    (*m_currentTask)[layerId]->m_cqpValue[0] = GetSvcQpValue(*extRc, *(*m_currentTask)[layerId], TFIELD);
    (*m_currentTask)[layerId]->m_cqpValue[1] = GetSvcQpValue(*extRc, *(*m_currentTask)[layerId], BFIELD);

    if (qid == 0 && did != m_video->calcParam.did[0])
        (*m_currentTask)[layerId - 1]->m_nextLayerTask = (*m_currentTask)[layerId];

    (*m_currentTask)[layerId]->m_statusReportNumber[0] = (*m_currentTask)[layerId]->m_statusReportNumber[0] * (mfxU32) m_dqid.size() + layerId;

    m_layer[layerId].ConfirmTask(*(*m_currentTask)[layerId]);

    return MFX_ERR_NONE;
}

mfxStatus ImplementationSvc::Query(
    VideoCORE *     core,
    mfxVideoParam * in,
    mfxVideoParam * out)
{
    core, in, out;

    if (core
     && core->GetVAType() == MFX_HW_VAAPI
     && core->GetHWType() <= MFX_HW_IVB)
    {
        return MFX_ERR_UNSUPPORTED; // no SVC support on Linux IVB
    }

    return MFX_ERR_NONE;
    //return ImplementationAvc::Query(core, in, out);
}

mfxStatus ImplementationSvc::QueryIOSurf(
    VideoCORE *            core,
    mfxVideoParam *        par,
    mfxFrameAllocRequest * request)
{
    mfxStatus sts = MFX_ERR_NONE;

    //need to fix

    GUID guid = DXVA2_Intel_Encode_SVC;    //m_core->IsGuidSupported(DXVA2_Intel_Encode_SVC, par, true) == MFX_ERR_NONE
    //? DXVA2_Intel_Encode_SVC
    //: DXVA2_Intel_Encode_AVC;



    mfxU32 inPattern = par->IOPattern & MFX_IOPATTERN_IN_MASK;
    MFX_CHECK(
        inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY,
        MFX_ERR_INVALID_VIDEO_PARAM);

    ENCODE_CAPS hwCaps = { 0 };
    sts = QueryHwCaps(core, hwCaps, guid);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    MfxVideoParam tmp(*par);

    sts = ReadSpsPpsHeaders(tmp);
    MFX_CHECK_STS(sts);

    sts = CheckWidthAndHeight(tmp);
    MFX_CHECK_STS(sts);

    sts = CopySpsPpsToVideoParam(tmp);
    if (sts < MFX_ERR_NONE)
        return sts;

    mfxStatus checkSts = CheckVideoParamQueryLike(tmp, hwCaps);
    if (checkSts == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_WRN_PARTIAL_ACCELERATION; // return immediately

    SetDefaults(tmp, hwCaps, true);

    if (tmp.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        // in case of system memory
        // encode need enough input frames for reordering
        // then reordered surface is copied into video memory
        request->Type =
            MFX_MEMTYPE_EXTERNAL_FRAME |
            MFX_MEMTYPE_FROM_ENCODE |
            MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else // MFX_IOPATTERN_IN_VIDEO_MEMORY || MFX_IOPATTERN_IN_OPAQUE_MEMORY
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        request->Type |= (inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            ? MFX_MEMTYPE_OPAQUE_FRAME
            : MFX_MEMTYPE_EXTERNAL_FRAME;
    }

    // get FrameInfo from original VideoParam
    request->Info = tmp.mfx.FrameInfo;
    mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(*par);
    mfxU32 lastDid = GetLastDid(*extSvc);
    if (lastDid > 7)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxU16 numDependencyLayer = 0;
    for (mfxU32 i = 0; i < 8; i++)
    if (extSvc->DependencyLayer[i].Active)
        numDependencyLayer++;

    request->Info.Width = extSvc->DependencyLayer[lastDid].Width;
    request->Info.Height = extSvc->DependencyLayer[lastDid].Height;

    request->NumFrameMin = CalcNumFrameMin(*par);
    request->NumFrameSuggested = request->NumFrameMin;

    return MFX_ERR_NONE;
}

ImplementationSvc::ImplementationSvc(VideoCORE * core)
: m_core(core)
, m_video()
, m_videoInit()
, m_hrd()
, m_guid()
, m_manager()
, m_aesCounter()
, m_maxBsSize(0)
, m_layout()
, m_caps()
, m_deviceFailed()
, m_inputFrameType()
, m_sei()
{
}

ImplementationSvc::~ImplementationSvc()
{
}


namespace
{
    mfxU16 GetFrameWidth(MfxVideoParam const & par)
    {
        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(par);
        for (mfxU32 i = 8; i > 0; i--)
            if (extSvc->DependencyLayer[i - 1].Active)
                return extSvc->DependencyLayer[i - 1].Width;
        return 0;
    }

    mfxU16 GetFrameHeight(MfxVideoParam const & par)
    {
        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(par);
        for (mfxU32 i = 8; i > 0; i--)
            if (extSvc->DependencyLayer[i - 1].Active)
                return extSvc->DependencyLayer[i - 1].Height;
        return 0;
    }
};


mfxStatus ImplementationSvc::Init(mfxVideoParam * par)
{
    if (mfxExtSVCRateControl const * extRc = GetExtBuffer(*par))
    {
        par->mfx.RateControlMethod = extRc->RateControlMethod;
        if (extRc->RateControlMethod == MFX_RATECONTROL_CQP)
        {
            par->mfx.QPI = extRc->Layer[0].Cqp.QPI;
            par->mfx.QPP = extRc->Layer[0].Cqp.QPP;
            par->mfx.QPB = extRc->Layer[0].Cqp.QPB;
        }
        else
        {
            par->mfx.TargetKbps       = mfxU16(extRc->Layer[0].CbrVbr.TargetKbps);
            par->mfx.MaxKbps          = mfxU16(extRc->Layer[0].CbrVbr.MaxKbps);
            par->mfx.BufferSizeInKB   = mfxU16(extRc->Layer[0].CbrVbr.BufferSizeInKB);
            par->mfx.InitialDelayInKB = mfxU16(extRc->Layer[0].CbrVbr.InitialDelayInKB);
        }
    }

    if (mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(*par))
    {
        par->mfx.GopPicSize  = extSvc->DependencyLayer[0].GopPicSize;
        par->mfx.GopRefDist  = extSvc->DependencyLayer[0].GopRefDist;
        par->mfx.GopOptFlag  = extSvc->DependencyLayer[0].GopOptFlag;
        par->mfx.IdrInterval = extSvc->DependencyLayer[0].IdrInterval;
    }

    mfxStatus sts = CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    m_video = *par;

    sts = CheckWidthAndHeight(m_video);
    MFX_CHECK_STS(sts);

    //need to fix

    m_guid = DXVA2_Intel_Encode_SVC;    //m_core->IsGuidSupported(DXVA2_Intel_Encode_SVC, par, true) == MFX_ERR_NONE
                                        //? DXVA2_Intel_Encode_SVC
                                        //: DXVA2_Intel_Encode_AVC;

    m_ddi.reset(m_guid == DXVA2_Intel_Encode_SVC
        ? CreatePlatformSvcEncoder(m_core)
        : CreateMultipleAvcEncode(m_core));
    if (m_ddi.get() == 0)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = m_ddi->CreateAuxilliaryDevice(m_core, m_guid, GetFrameWidth(m_video), GetFrameHeight(m_video));
    MFX_CHECK_STS(sts);

    sts = m_ddi->QueryEncodeCaps(m_caps);
    MFX_CHECK_STS(sts);

    mfxStatus checkStatus = CheckVideoParam(m_video, m_caps, m_core->IsExternalFrameAllocator());
    if (checkStatus == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_WRN_PARTIAL_ACCELERATION;
    else if (checkStatus < MFX_ERR_NONE)
        return checkStatus;

    // need it for both ENCODE and ENC
    m_hrd.Setup(m_video);

    mfxExtPAVPOption * extOptPavp = GetExtBuffer(m_video);
    m_aesCounter.Init(*extOptPavp);

    sts = m_ddi->CreateAccelerationService(m_video);
    MFX_CHECK_STS(sts);

    mfxExtOpaqueSurfaceAlloc const * extOpaq = GetExtBuffer(m_video);
    m_inputFrameType =
        m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        (m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            ? MFX_IOPATTERN_IN_SYSTEM_MEMORY
            : MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // Allocate raw surfaces.
    // This is required only in case of system memory at input

    mfxExtSVCSeqDesc * const extSvc  = GetExtBuffer(m_video);
    mfxU32 numDeps = m_video.calcParam.numDependencyLayer;
    mfxU32 lastDid = m_video.calcParam.did[numDeps - 1];

    mfxFrameAllocRequest request = { 0 };
    request.Info        = m_video.mfx.FrameInfo;
    request.Info.Width  = extSvc->DependencyLayer[lastDid].Width;
    request.Info.Height = extSvc->DependencyLayer[lastDid].Height;

    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request.Type        = MFX_MEMTYPE_D3D_INT;
        request.NumFrameMin = mfxU16(CalcNumSurfRaw(m_video) * numDeps);

        sts = m_raw.Alloc(m_core, request);
        MFX_CHECK_STS(sts);
        }
        else if (m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            request.Type        = extOpaq->In.Type;
            request.NumFrameMin = extOpaq->In.NumSurface;

            sts = m_opaqHren.Alloc(m_core, request, extOpaq->In.Surfaces, extOpaq->In.NumSurface);
            MFX_CHECK_STS(sts);

            if (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            {
                request.Type        = MFX_MEMTYPE_D3D_INT;
                request.NumFrameMin = extOpaq->In.NumSurface;
            sts = m_raw.Alloc(m_core, request);
        }
    }

    // ENC+PAK always needs separate chain for reconstructions produced by PAK.
    request.Type        = m_video.Protected ? MFX_MEMTYPE_D3D_SERPENT_INT : MFX_MEMTYPE_D3D_INT;
    request.NumFrameMin = mfxU16(CalcNumSurfRecon(m_video) * m_video.calcParam.numLayersTotal);
    sts = m_recon.Alloc(m_core, request);
    MFX_CHECK_STS(sts);

    // Allocate surfaces for bitstreams.
    // Need at least one such surface and more for async-mode.
    request.Type        = MFX_MEMTYPE_D3D_INT;
    request.Info.FourCC = MFX_FOURCC_P8;
    request.NumFrameMin = mfxU16(CalcNumSurfBitstream(m_video) * m_video.calcParam.numLayersTotal);
    request.Info.Height = request.Info.Height * 3 / 2;
    sts = m_bitstream.Alloc(m_core, request);
    MFX_CHECK_STS(sts);
    m_maxBsSize = request.Info.Width * request.Info.Height;

    sts = m_ddi->Register(m_recon, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_bitstream, D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
    MFX_CHECK_STS(sts);

    m_manager.Init(m_core, m_video);

    if (m_caps.HeaderInsertion == 1 && m_video.Protected == 0)
    {
        m_tmpBsBuf.resize(m_maxBsSize);

        OutputBitstream obs(Begin(m_tmpBsBuf), End(m_tmpBsBuf), true);
        mfxU32 size = PutScalableInfoSeiMessage(obs, m_video);
        m_scalabilityInfo.resize(size / 8);
        std::copy(Begin(m_tmpBsBuf), Begin(m_tmpBsBuf) + size / 8, Begin(m_scalabilityInfo));
    }

    m_deviceFailed = false;

    const mfxU32 MAX_SEI_SIZE    = 10 * 1024;
    const mfxU32 MAX_FILLER_SIZE = m_video.mfx.FrameInfo.Width * m_video.mfx.FrameInfo.Height;
    m_sei.Alloc(MAX_SEI_SIZE + MAX_FILLER_SIZE);

    m_videoInit = m_video;

    return checkStatus;
}

mfxStatus ImplementationSvc::Reset(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);

    sts = CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    mfxExtPAVPOption * optPavp = GetExtBuffer(*par);
    MFX_CHECK(optPavp == 0, MFX_ERR_INVALID_VIDEO_PARAM); // mfxExtPAVPOption should not come to Reset

    MfxVideoParam newPar = *par;

    mfxExtOpaqueSurfaceAlloc * extOpaqNew = GetExtBuffer(newPar);
    mfxExtOpaqueSurfaceAlloc * extOpaqOld = GetExtBuffer(m_video);
    MFX_CHECK(
        extOpaqOld->In.Type       == extOpaqNew->In.Type       &&
        extOpaqOld->In.NumSurface == extOpaqNew->In.NumSurface,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    InheritDefaultValues(m_video, newPar);

    mfxStatus checkStatus = CheckVideoParam(newPar, m_caps, m_core->IsExternalFrameAllocator());
    if (checkStatus == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else if (checkStatus < MFX_ERR_NONE)
        return checkStatus;

    MFX_CHECK(
        m_video.AsyncDepth                 == newPar.AsyncDepth                 &&
        m_videoInit.mfx.GopRefDist         >= newPar.mfx.GopRefDist             &&
        m_video.mfx.NumSlice               == newPar.mfx.NumSlice               &&
        m_video.mfx.NumRefFrame            == newPar.mfx.NumRefFrame            &&
        m_video.mfx.RateControlMethod      == newPar.mfx.RateControlMethod      &&
        m_video.calcParam.initialDelayInKB == newPar.calcParam.initialDelayInKB &&
        m_video.calcParam.bufferSizeInKB   == newPar.calcParam.bufferSizeInKB   &&
        m_videoInit.mfx.FrameInfo.Width    >= newPar.mfx.FrameInfo.Width        &&
        m_videoInit.mfx.FrameInfo.Height   >= newPar.mfx.FrameInfo.Height       &&
        m_video.mfx.FrameInfo.ChromaFormat == newPar.mfx.FrameInfo.ChromaFormat,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    mfxExtCodingOption * extOptNew = GetExtBuffer(newPar);
    mfxExtCodingOption * extOptOld = GetExtBuffer(m_video);
    MFX_CHECK(
        IsOn(extOptOld->FieldOutput) || extOptOld->FieldOutput == extOptNew->FieldOutput,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    if (m_video.mfx.RateControlMethod == MFX_RATECONTROL_CBR && IsOn(extOptNew->NalHrdConformance))
    {
        MFX_CHECK(
            m_video.calcParam.targetKbps == newPar.calcParam.targetKbps &&
            m_video.calcParam.maxKbps    == newPar.calcParam.maxKbps,
            MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    }

    m_video = newPar;
    m_manager.Reset(m_video);
    m_hrd.Reset(m_video);

    m_ddi->Reset(m_video);

    return checkStatus;
}

mfxStatus ImplementationSvc::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    par->mfx = m_video.mfx;
    par->Protected = m_video.Protected;
    par->IOPattern = m_video.IOPattern;
    par->AsyncDepth = m_video.AsyncDepth;

    for (mfxU32 i = 0; i < par->NumExtParam; i++)
    {
        if (mfxExtBuffer * buf = GetExtBuffer(m_video.ExtParam, m_video.NumExtParam, par->ExtParam[i]->BufferId))
        {
            if (par->ExtParam[i]->BufferId == MFX_EXTBUFF_CODING_OPTION_SPSPPS)
            {
                // need to generate sps/pps nal units
                mfxExtCodingOptionSPSPPS * dst = (mfxExtCodingOptionSPSPPS *)par->ExtParam[i];

                mfxExtSpsHeader * sps = GetExtBuffer(m_video);
                mfxExtPpsHeader * pps = GetExtBuffer(m_video);

                try
                {
                    if (dst->SPSBuffer)
                    {
                        MFX_CHECK(dst->SPSBufSize, MFX_ERR_INVALID_VIDEO_PARAM);
                        OutputBitstream writerSps(dst->SPSBuffer, dst->SPSBufSize);
                        WriteSpsHeader(writerSps, *sps);
                        dst->SPSBufSize = mfxU16((writerSps.GetNumBits() + 7) / 8);
                    }
                    if (dst->PPSBuffer)
                    {
                        MFX_CHECK(dst->PPSBufSize, MFX_ERR_INVALID_VIDEO_PARAM);
                        OutputBitstream writerPps(dst->PPSBuffer, dst->PPSBufSize);
                        WritePpsHeader(writerPps, *pps);
                        dst->PPSBufSize = mfxU16((writerPps.GetNumBits() + 7) / 8);
                    }
                }
                catch (std::exception &)
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                dst->SPSId = sps->seqParameterSetId;
                dst->PPSId = pps->picParameterSetId;
            }
            else
            {
                MFX_INTERNAL_CPY(par->ExtParam[i], buf, par->ExtParam[i]->BufferSz);
            }
        }
        else
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus ImplementationSvc::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus ImplementationSvc::GetEncodeStat(mfxEncodeStat *stat)
{
    MFX_CHECK_NULL_PTR1(stat);
    *stat = m_manager.GetEncodeStat();

    return MFX_ERR_NONE;
}

mfxStatus ImplementationSvc::TaskRoutineSubmit(
    void * state,
    void * param,
    mfxU32 /*threadNumber*/,
    mfxU32 /*callNumber*/)
{
    ImplementationSvc & impl = *(ImplementationSvc *) state;
    SvcTask &           task = *(SvcTask *)param;

    if (impl.CheckDevice() != MFX_ERR_NONE)
        return MFX_ERR_DEVICE_FAILED;

    mfxExtSVCSeqDesc * extSvc = GetExtBuffer(impl.m_video);

    // need to call TaskManager::CompleteTask
    // even if error occurs during status query
    CompleteTaskOnExitSvc completer(impl.m_manager, task);

    for (mfxU32 layer = 0, did = 0; did < impl.m_video.calcParam.numDependencyLayer; did++)
    {
        mfxStatus sts = impl.CopyRawSurface(*task[layer]);
        MFX_CHECK_STS(sts);

        mfxHDL rawSurfaceNative = impl.GetRawSurfaceHandle(*task[layer]);

        for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++, layer++)
        {
            PreAllocatedVector sei;

            sts = impl.m_ddi->Execute(rawSurfaceNative, *task[layer], 0, sei);

            if (sts != MFX_ERR_NONE)
                return impl.UpdateDeviceStatus(sts);

            //for (;;)
            //{
            //    //printf("querying frameorder=%d, did=%d, qid=%d", task[layer]->m_frameOrder, task[layer]->m_did, task[layer]->m_qid);
            //    mfxStatus sts = impl.m_ddi->QueryStatus(*task[layer], 0);
            //    //printf(", res=%d\n", sts); fflush(stdout);
            //    if (sts == MFX_WRN_DEVICE_BUSY)
            //        vm_time_sleep(100);
            //    else if (sts == MFX_ERR_NONE)
            //        break;
            //    else
            //        return MFX_ERR_DEVICE_FAILED;
            //}
        }
    }


    completer.Cancel();

    return MFX_TASK_DONE;
}


mfxStatus ImplementationSvc::TaskRoutineQuery(
    void * state,
    void * param,
    mfxU32 /*threadNumber*/,
    mfxU32 /*callNumber*/)
{
    ImplementationSvc & impl = *(ImplementationSvc *)state;
    SvcTask &           task = *(SvcTask *)param;

    if (impl.CheckDevice() != MFX_ERR_NONE)
        return MFX_ERR_DEVICE_FAILED;

    mfxExtSVCSeqDesc * extSvc = GetExtBuffer(impl.m_video);

    mfxU32 layer = 0;

    for (mfxU32 did = 0; did < impl.m_video.calcParam.numDependencyLayer; did++)
    {
        for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++, layer++)
        {
            if (task[layer]->m_bsDataLength[0] == 0)
            {
                mfxStatus sts = impl.m_ddi->QueryStatus(*task[layer], 0);
                if (sts == MFX_WRN_DEVICE_BUSY)
                    return MFX_TASK_BUSY;
            }
        }
    }

    // need to call TaskManager::CompleteTask
    // even if error occurs during status query
    CompleteTaskOnExitSvc completer(impl.m_manager, task);

    mfxStatus sts = impl.UpdateBitstream(task, 0);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}


mfxStatus ImplementationSvc::EncodeFrameCheck(
    mfxEncodeCtrl *           ctrl,
    mfxFrameSurface1 *        surface,
    mfxBitstream *            bs,
    mfxFrameSurface1 **       reordered_surface,
    mfxEncodeInternalParams * internalParams,
    MFX_ENTRY_POINT           entryPoints[],
    mfxU32 &                  numEntryPoints)
{
    internalParams;

    if (m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY &&
        surface)
    {
        mfxFrameSurface1 * opaqSurf = surface;
        surface = m_core->GetNativeSurface(opaqSurf);
        MFX_CHECK(surface, MFX_ERR_UNDEFINED_BEHAVIOR);

        surface->Info            = opaqSurf->Info;
        surface->Data.TimeStamp  = opaqSurf->Data.TimeStamp;
        surface->Data.FrameOrder = opaqSurf->Data.FrameOrder;
        surface->Data.Corrupted  = opaqSurf->Data.Corrupted;
        surface->Data.DataFlag   = opaqSurf->Data.DataFlag;
    }

    mfxStatus checkSts = CheckEncodeFrameParam(
        m_video,
        ctrl,
        surface,
        bs,
        m_core->IsExternalFrameAllocator());
    MFX_CHECK(checkSts >= MFX_ERR_NONE, checkSts);

    SvcTask * task = 0;
    mfxStatus assignSts = AssignTask(ctrl, surface, bs, &task);

    if (assignSts == MFX_ERR_NONE)
    {
        *reordered_surface = surface;

        entryPoints[0].pState               = this;
        entryPoints[0].pParam               = task;
        entryPoints[0].pRoutine             = TaskRoutineSubmit;
        entryPoints[0].pCompleteProc        = 0;
        entryPoints[0].pGetSubTaskProc      = 0;
        entryPoints[0].pCompleteSubTaskProc = 0;
        entryPoints[0].requiredNumThreads   = 1;
        entryPoints[1]                      = entryPoints[0];
        entryPoints[1].pRoutine             = TaskRoutineQuery;
        numEntryPoints                      = 2;

        // TaskRoutineQuery always updates bitstream from base view task
        (*task)[0]->m_bs = bs;

        return checkSts;
    }
    else if (assignSts == MFX_ERR_MORE_DATA)
    {
        *reordered_surface = surface;

        entryPoints[0].pState               = 0;
        entryPoints[0].pParam               = 0;
        entryPoints[0].pRoutine             = TaskRoutineDoNothing;
        entryPoints[0].pCompleteProc        = 0;
        entryPoints[0].pGetSubTaskProc      = 0;
        entryPoints[0].pCompleteSubTaskProc = 0;
        entryPoints[0].requiredNumThreads   = 1;
        numEntryPoints                      = 1;

        return mfxStatus(MFX_ERR_MORE_DATA_RUN_TASK);
    }
    else
    {
        return assignSts;
    }
}

mfxStatus ImplementationSvc::AssignTask(
    mfxEncodeCtrl *    ctrl,
    mfxFrameSurface1 * surface,
    mfxBitstream *     bs,
    SvcTask **         newTask)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, "AssignTask");

    SvcTask * task = 0;
    mfxStatus sts = m_manager.AssignTask(ctrl, surface, bs, task);
    MFX_CHECK_STS(sts);

    *newTask = task;

    return MFX_ERR_NONE;
}

mfxStatus ImplementationSvc::CopyRawSurface(
    DdiTask const & task)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxExtOpaqueSurfaceAlloc * extOpaq = GetExtBuffer(m_video);
    mfxExtSVCSeqDesc *         extSvc  = GetExtBuffer(m_video);

    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        (m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
    {
        mfxFrameData d3dSurf = { 0, };
        mfxFrameData sysSurf = task.m_yuv->Data;

        //FrameLocker lock1(m_core, d3dSurf, m_raw.mids[task.m_idx]);
        //MFX_CHECK(d3dSurf.Y != 0, MFX_ERR_LOCK_MEMORY);
        d3dSurf.MemId = m_raw.mids[task.m_idx];
        FrameLocker lock2(m_core, sysSurf, true);
        MFX_CHECK(sysSurf.Y != 0, MFX_ERR_LOCK_MEMORY);

        mfxFrameInfo frameInfo = m_video.mfx.FrameInfo;
        frameInfo.Width  = extSvc->DependencyLayer[task.m_did].Width;
        frameInfo.Height = extSvc->DependencyLayer[task.m_did].Height;
        frameInfo.CropX  = extSvc->DependencyLayer[task.m_did].CropX;
        frameInfo.CropY  = extSvc->DependencyLayer[task.m_did].CropY;
        frameInfo.CropW  = extSvc->DependencyLayer[task.m_did].CropW;
        frameInfo.CropH  = extSvc->DependencyLayer[task.m_did].CropH;

        mfxStatus sts = CopyFrameDataBothFields(m_core, d3dSurf, sysSurf, frameInfo);
        MFX_CHECK_STS(sts);

        sts = lock2.Unlock();
        MFX_CHECK_STS(sts);
        //sts = lock1.Unlock();
        //MFX_CHECK_STS(sts);
    }

    return sts;
}

mfxHDL ImplementationSvc::GetRawSurfaceHandle(
    DdiTask const & task)
{
    mfxHDL nativeSurface = 0;

    mfxExtOpaqueSurfaceAlloc * extOpaq = GetExtBuffer(m_video);

    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        (m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
    {
        m_core->GetFrameHDL(m_raw.mids[task.m_idx], (mfxHDL *)&nativeSurface);
    }
    else
    {
        if (MFX_IOPATTERN_IN_VIDEO_MEMORY == m_video.IOPattern)
            m_core->GetExternalFrameHDL(task.m_yuv->Data.MemId, (mfxHDL *)&nativeSurface);
        else if (MFX_IOPATTERN_IN_OPAQUE_MEMORY == m_video.IOPattern) // opaq with internal video memory
            m_core->GetFrameHDL(task.m_yuv->Data.MemId, (mfxHDL *)&nativeSurface);
    }

    return nativeSurface;
}

mfxStatus ImplementationSvc::UpdateBitstream(
    SvcTask & task,
    mfxU32    fieldId)
{
    mfxFrameData bitstream = { 0 };

    mfxU32 totalDataLength = 0;

    mfxBitstream & outBits = *task[0]->m_bs;

    if (m_caps.HeaderInsertion && !m_video.Protected)
    {
        mfxU8 * dbegin = outBits.Data + outBits.DataOffset + outBits.DataLength;
        mfxU8 * dend   = outBits.Data + outBits.MaxLength;

        outBits.DataLength += mfxU32(CheckedMFX_INTERNAL_CPY(dbegin, dend, Begin(m_scalabilityInfo), End(m_scalabilityInfo)) - dbegin);
    }

    // Lock d3d surface with compressed picture.
    for (mfxU32 i = 0; i < task.LayerNum(); i++)
    {
        bool needIntermediateBitstreamBuffer =
            IsSlicePatchNeeded(*task[i], fieldId) ||
            m_video.calcParam.numTemporalLayer > 0;

        bool doPatch =
            needIntermediateBitstreamBuffer ||
            IsInplacePatchNeeded(m_video, *task[i], fieldId);

        if (m_caps.HeaderInsertion == 0 || m_video.Protected != 0)
            doPatch = needIntermediateBitstreamBuffer = false;

        //printf("locking bitstream bsidx=%d did=%d qid=%d\n",
        //    task[i]->m_idxBs[fieldId] + CalcNumSurfBitstream(m_video) * task[i]->m_qid,
        //    task[i]->m_did, task[i]->m_qid); fflush(stdout);

        FrameLocker lock(
            m_core,
            bitstream,
            m_bitstream.mids[task[i]->m_idxBs[fieldId]]);

        if (bitstream.Y == 0)
            return Error(MFX_ERR_LOCK_MEMORY);

        mfxU32  bsSize      = task[i]->m_bsDataLength[fieldId];
        mfxU32  bsSizeAvail = outBits.MaxLength - outBits.DataOffset - outBits.DataLength;
        mfxU8 * bsData      = outBits.Data      + outBits.DataOffset + outBits.DataLength;

        if (needIntermediateBitstreamBuffer)
        {
            bsData      = &m_tmpBsBuf[0];
            bsSizeAvail = mfxU32(IPP_MIN(bsSizeAvail, m_tmpBsBuf.size()));
        }

        assert(bsSize <= bsSizeAvail);
        if (bsSize > bsSizeAvail)
            bsSize = bsSizeAvail;

        //printf("fastcopying bitstream bytes=%d\n", task[i]->m_bsDataLength[fieldId]); fflush(stdout);
        // Copy compressed picture from d3d surface to buffer in system memory
        FastCopyBufferVid2Sys(bsData, bitstream.Y, bsSize);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Surface unlock (bitstream)");
            MFX_LTRACE_S(MFX_TRACE_LEVEL_INTERNAL, task[i]->m_FrameName);
            mfxStatus sts = lock.Unlock();
            MFX_CHECK_STS(sts);
        }

        if (doPatch)
        {
            mfxU8 * dbegin = bsData;
            mfxU8 * dend   = bsData + bsSize;

            if (needIntermediateBitstreamBuffer)
            {
                dbegin = outBits.Data + outBits.DataOffset + outBits.DataLength;
                dend   = outBits.Data + outBits.MaxLength;
            }

            mfxU8 * endOfPatchedBitstream =
                PatchBitstream(m_video, *task[i], fieldId, bsData, bsData + bsSize, dbegin, dend);
            //printf("patching bitstream inbytes=%d outbytes=%d\n", bsSize, mfxU32(endOfPatchedBitstream - dbegin)); fflush(stdout);

            outBits.DataLength += (mfxU32)(endOfPatchedBitstream - dbegin);
            totalDataLength += (mfxU32)(endOfPatchedBitstream - dbegin);
        }
        else
        {
            outBits.DataLength += bsSize;
            totalDataLength += bsSize;
        }
    }

    // Update bitstream fields
    bool interlace = (task[0]->GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0;

    //printf("updating bitstream attributes\n"); fflush(stdout);
    task[0]->m_bs->TimeStamp = task[0]->m_yuv->Data.TimeStamp;
    task[0]->m_bs->PicStruct = task[0]->GetPicStructForDisplay();
    task[0]->m_bs->FrameType = task[0]->m_type[task[0]->GetFirstField()];
    if (interlace)
        task[0]->m_bs->FrameType = mfxU16(task[0]->m_bs->FrameType | (task[0]->m_type[!task[0]->GetFirstField()] << 8));

    //printf("updating hrd\n"); fflush(stdout);
    // Update hrd buffer
    m_hrd.RemoveAccessUnit(
        totalDataLength,
        interlace,
        (task[0]->m_type[fieldId] & MFX_FRAMETYPE_IDR) != 0);

    return MFX_ERR_NONE;
}

namespace
{
    void PutRawBytes(bool condition, OutputBitstream & bs, mfxU8 const * begin, mfxU8 const * end)
    {
        if (condition)
            bs.PutRawBytes(begin, end);
    }

    void PutTrailingBits(bool condition, OutputBitstream & bs)
    {
        if (condition)
            bs.PutTrailingBits();
    }
}

namespace
{
    mfxExtPictureTimingSEI const * SelectPicTimingSei(
        MfxVideoParam const & video,
        DdiTask const &       task)
    {
        if (mfxExtPictureTimingSEI const * extPt = GetExtBuffer(task.m_ctrl))
        {
            return extPt;
        }
        else
        {
            mfxExtPictureTimingSEI const * extPtGlobal = GetExtBuffer(video);
            return extPtGlobal;
        }
    }
};

void ImplementationSvc::PrepareSeiMessageBuffer(
    DdiTask const &      task,
    mfxU32               fieldId,
    PreAllocatedVector & sei)
{
    mfxExtCodingOption const *     extOpt = GetExtBuffer(m_video);
    mfxExtSpsHeader const *        extSps = GetExtBuffer(m_video);
    mfxExtPictureTimingSEI const * extPt  = SelectPicTimingSei(m_video, task);

    mfxU32 needDecRefPicMrkRep =
        IsOn(extOpt->RefPicMarkRep) && task.m_decRefPicMrkRep[fieldId].presentFlag;

    mfxU32 needPicTimingSei =
        IsOn(extOpt->PicTimingSEI)             ||
        extPt->TimeStamp[0].ClockTimestampFlag ||
        extPt->TimeStamp[1].ClockTimestampFlag ||
        extPt->TimeStamp[2].ClockTimestampFlag;

    if (task.m_ctrl.NumPayload > 0             ||
        IsOn(extOpt->VuiNalHrdParameters)      ||
        IsOn(extOpt->VuiVclHrdParameters)      ||
        needPicTimingSei                       ||
        needDecRefPicMrkRep)
    {
        OutputBitstream bs(sei.Buffer(), sei.Capacity());

        mfxU8 startcode[5] = { 0, 0, 0, 1, 6 };

        // write start code of common sei nal unit
        PutRawBytes(IsOn(extOpt->SingleSeiNalUnit), bs, startcode, startcode + sizeof(startcode));

        if (IsOn(extOpt->VuiNalHrdParameters) ||
            IsOn(extOpt->VuiVclHrdParameters) ||
            needPicTimingSei)
        {
            mfxU32 fieldPicFlag       = (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE);
            mfxU32 secondFieldPicFlag = (task.GetFirstField() != fieldId);
            mfxU32 idrPicFlag         = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR);

            bool isRecoveryPoint = IsRecoveryPointSeiMessagePresent(
                task.m_ctrl.Payload,
                task.m_ctrl.NumPayload,
                GetPayloadLayout(fieldPicFlag, secondFieldPicFlag));

            if (idrPicFlag || isRecoveryPoint)
            {
                mfxExtAvcSeiBufferingPeriod msgBufferingPeriod;
                PrepareSeiMessage(
                    task,
                    IsOn(extOpt->VuiNalHrdParameters),
                    IsOn(extOpt->VuiVclHrdParameters),
                    extSps->seqParameterSetId,
                    msgBufferingPeriod);

                PutRawBytes(IsOff(extOpt->SingleSeiNalUnit), bs, startcode, startcode + sizeof(startcode));
                PutSeiMessage(bs, msgBufferingPeriod);
                PutTrailingBits(IsOff(extOpt->SingleSeiNalUnit), bs);
            }

            mfxExtAvcSeiPicTiming msgPicTiming;
            PrepareSeiMessage(
                task,
                fieldId,
                IsOn(extOpt->VuiNalHrdParameters) || IsOn(extOpt->VuiVclHrdParameters),
                msgPicTiming);

            PutRawBytes(IsOff(extOpt->SingleSeiNalUnit), bs, startcode, startcode + sizeof(startcode));
            PutSeiMessage(bs, *extPt, msgPicTiming);
            PutTrailingBits(IsOff(extOpt->SingleSeiNalUnit), bs);
        }

        // user-defined messages
        mfxU32 step  = (task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;
        mfxU32 start = (fieldId == task.GetFirstField()) ? 0 : 1;

        for (mfxU32 i = start; i < task.m_ctrl.NumPayload; i += step)
        {
            if (task.m_ctrl.Payload[i] != 0)
            {
                mfxU8 * beg = task.m_ctrl.Payload[i]->Data;
                mfxU8 * end = beg + (task.m_ctrl.Payload[i]->NumBit + 7) / 8;

                PutRawBytes(IsOff(extOpt->SingleSeiNalUnit), bs, startcode, startcode + sizeof(startcode));
                for (; beg != end; beg++)
                    bs.PutBits(*beg, 8);
                PutTrailingBits(IsOff(extOpt->SingleSeiNalUnit), bs);
            }
        }

        // dec_ref_pic_marking repetition sei
        if (needDecRefPicMrkRep)
        {
            mfxU8 frameMbsOnlyFlag = (m_video.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;

            mfxExtAvcSeiDecRefPicMrkRep decRefPicMrkRep;
            PrepareSeiMessage(task, fieldId, frameMbsOnlyFlag, decRefPicMrkRep);
            PutRawBytes(IsOff(extOpt->SingleSeiNalUnit), bs, startcode, startcode + sizeof(startcode));
            PutSeiMessage(bs, decRefPicMrkRep);
            PutTrailingBits(IsOff(extOpt->SingleSeiNalUnit), bs);
        }

        // finalize common sei nal unit
        PutTrailingBits(IsOn(extOpt->SingleSeiNalUnit), bs);

        assert(bs.GetNumBits() / 8 <= sei.Capacity());
        sei.SetSize(bs.GetNumBits() / 8);
    }
}

mfxStatus ImplementationSvc::UpdateDeviceStatus(mfxStatus sts)
{
    if (sts == MFX_ERR_DEVICE_FAILED)
        m_deviceFailed = true;
    return sts;
}

mfxStatus ImplementationSvc::CheckDevice()
{
    return m_deviceFailed
        ? MFX_ERR_DEVICE_FAILED
        : MFX_ERR_NONE;
}

mfxStatus ImplementationSvc::TaskRoutineDoNothing(
    void * /*state*/,
    void * /*param*/,
    mfxU32 /*threadNumber*/,
    mfxU32 /*callNumber*/)
{
    return MFX_TASK_DONE;
}

#endif // MFX_ENABLE_H264_VIDEO_ENCODE_HW
