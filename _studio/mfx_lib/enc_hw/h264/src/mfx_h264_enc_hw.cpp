//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENC_HW)

#include "mfx_h264_enc_hw.h"
#include "mfx_h264_ex_param_buf.h"

using namespace MfxHwH264Encode;

MFXHWVideoENCH264* CreateMFXHWVideoENCH264(VideoCORE *core, mfxStatus *status)
{
    return new MFXHWVideoENCH264(core, status);
}

mfxStatus MFXHWVideoENCH264::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    if (in == 0)
    {
        memset(out, 0, sizeof(*out));
    }
    else
    {
        MFX_INTERNAL_CPY(out, in, sizeof(*out));
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXHWVideoENCH264::QueryIOSurf(mfxVideoParam *, mfxFrameAllocRequest *request)
{
    request->NumFrameMin = 1;
    request->NumFrameSuggested = request->NumFrameMin;
    return MFX_ERR_NONE;
}

MFXHWVideoENCH264::MFXHWVideoENCH264(VideoCORE *core, mfxStatus *status)
: m_core(core)
{
    memset(&m_videoParam, 0, sizeof(m_videoParam));
    memset(&m_frameParam, 0, sizeof(m_frameParam));
    if (status)
        *status = MFX_ERR_NONE;
}

MFXHWVideoENCH264::~MFXHWVideoENCH264()
{
    Close();
}

mfxStatus MFXHWVideoENCH264::Init(mfxVideoParam *par)
{
    ENCODE_CAPS hwCaps = { 0, };
    QueryHwCaps(m_core->GetAdapterNumber(), hwCaps);

    m_videoParam = *par;
    mfxStatus sts = CheckVideoParam(m_videoParam, hwCaps, m_core->IsExternalFrameAllocator());
    MFX_CHECK_STS(sts);

    memset(&m_frameParam, 0, sizeof(m_frameParam));

    m_videoParam.mfx.reserved[1] = 0;
    m_videoParam.mfx.NumSlice = max(m_videoParam.mfx.NumSlice, 1);

    sts = m_ddi.CreateAuxilliaryDevice(
        m_core,
        DXVA2_Intel_Encode_AVC,
        par->mfx.FrameInfo.Width,
        par->mfx.FrameInfo.Height);
    MFX_CHECK_STS(sts);

    m_ddiData.Init(m_videoParam);

    sts = m_ddi.CreateAccelerationService(m_ddiData);
    MFX_CHECK_STS(sts);

    Initialized<mfxFrameAllocRequest> request;
    request.Info = m_videoParam.mfx.FrameInfo;
    request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;
    request.NumFrameMin = m_videoParam.mfx.NumRefFrame + 1;
    request.NumFrameSuggested = m_videoParam.mfx.NumRefFrame + 1;
    sts = m_core->AllocFrames(&request, &m_recons);
    MFX_CHECK_STS(sts);

    request.Reset();
    sts = m_ddi.QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_MBDATA, request);
    MFX_CHECK_STS(sts);

    request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    request.NumFrameMin = m_recons.NumFrameActual;
    request.NumFrameSuggested = m_recons.NumFrameActual;
    sts = m_core->AllocFrames(&request, &m_mb);
    MFX_CHECK_STS(sts);

    request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    request.NumFrameMin = m_recons.NumFrameActual;
    request.NumFrameSuggested = m_recons.NumFrameActual;
    sts = m_core->AllocFrames(&request, &m_mv);
    MFX_CHECK_STS(sts);

    sts = m_ddi.Register(m_recons, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    sts = m_ddi.Register(m_mb, D3DDDIFMT_INTELENCODE_MBDATA);
    MFX_CHECK_STS(sts);

    sts = m_ddi.QueryMbDataLayout(m_ddiData.m_sps, m_layout);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus MFXHWVideoENCH264::Reset(mfxVideoParam *par)
{
    Close();
    return Init(par);
}

mfxStatus MFXHWVideoENCH264::Close()
{
    m_ddi.Destroy();
    m_core->FreeFrames(&m_mb);
    m_core->FreeFrames(&m_mv);
    m_core->FreeFrames(&m_recons);
    return MFX_ERR_NONE;
}

mfxStatus MFXHWVideoENCH264::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_videoParam;
    return MFX_ERR_NONE;
}

mfxStatus MFXHWVideoENCH264::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_frameParam;
    return MFX_ERR_NONE;
}

template<class T> struct ExtBufId { enum { id = 0 }; };
template<> struct ExtBufId<mfxExtAvcMvData> { enum { id = MFX_CUC_AVC_MV }; };
template<> struct ExtBufId<mfxExtAvcRefList> { enum { id = MFX_CUC_AVC_REFLIST }; };
template<> struct ExtBufId<mfxExtAvcRefFrameParam> { enum { id = MFX_CUC_AVC_FRAMEPARAM }; };

template<class T>
T* GetExtBuffer(mfxExtBuffer** extBuffer, mfxU16 numExtBuffer)
{
    for (mfxU16 i = 0; i < numExtBuffer; i++)
        if (extBuffer[i] && extBuffer[i]->BufferId == ExtBufId<T>::id)
            return (T *)extBuffer[i];
    return 0;
}

mfxStatus MFXHWVideoENCH264::RunFrameVmeENC(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR3(cuc->FrameParam, cuc->SliceParam, cuc->MbParam);
    MFX_CHECK_NULL_PTR3(cuc->FrameSurface, cuc->Bitstream, cuc->ExtBuffer);
    MFX_CHECK_COND(cuc->FrameSurface->NumFrameData >= m_videoParam.mfx.NumRefFrame + 1); //FIXME: tmp solution
    MFX_CHECK_COND(cuc->FrameParam->AVC.CurrFrameLabel < cuc->FrameSurface->NumFrameData);
    MFX_CHECK_NULL_PTR2(cuc->MbParam->Mb, cuc->FrameSurface->Data)
    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data[cuc->FrameParam->AVC.CurrFrameLabel]);
    MFX_CHECK_COND(cuc->NumExtBuffer == 3);

    mfxExtAvcMvData* extMvData = GetExtBuffer<mfxExtAvcMvData>(cuc->ExtBuffer, cuc->NumExtBuffer);
    mfxExtAvcRefFrameParam* extRefFrame = GetExtBuffer<mfxExtAvcRefFrameParam>(cuc->ExtBuffer, cuc->NumExtBuffer);
    MFX_CHECK_NULL_PTR2(extMvData, extRefFrame);

    ENCODE_SET_SEQUENCE_PARAMETERS_H264 oldSps = m_ddiData.m_sps;
    ConvertVideoParamMfx2Ddi(m_videoParam, *cuc->FrameParam, m_ddiData.m_sps/*, m_ddiData.m_vui*/);
    if (memcmp(&oldSps, &m_ddiData.m_sps, sizeof(oldSps)) != 0)
        m_ddiData.SpsUpdated(true);

    ConvertFrameParamMfx2Ddi(m_videoParam, *cuc->FrameParam, m_ddiData.m_pps[0], extRefFrame);

    ConvertSliceParamMfx2Ddi(*cuc->FrameParam, cuc->SliceParam, cuc->NumSlice, &m_ddiData.m_slice[0], &extRefFrame->PicInfo[0]);
    m_ddiData.m_idxMb = cuc->FrameParam->AVC.CurrFrameLabel;
    m_ddiData.m_idxMv = cuc->FrameParam->AVC.CurrFrameLabel;

    mfxStatus sts = m_core->GetFrameHDL(m_recons.mids[cuc->FrameParam->AVC.CurrFrameLabel], (mfxHDL *)&m_ddiData.m_surface);
    if (sts != MFX_ERR_NONE)
        return sts;

    sts = m_ddi.Execute(m_ddiData, 0); // FIXME: field support
    if (sts != MFX_ERR_NONE)
        return sts;

    Initialized<mfxFrameData> mbData, mvData;
    mbData.MemId = m_mb.mids[cuc->FrameParam->AVC.CurrFrameLabel];
    mvData.MemId = m_mv.mids[cuc->FrameParam->AVC.CurrFrameLabel];
    FrameLocker lock0(m_core, mbData);
    if (mbData.Y == 0)
        return MFX_ERR_LOCK_MEMORY;

    ConvertMbDdi2Mfx(
        mbData.Y,
        m_layout,
        0,
        cuc->MbParam->NumMb,
        cuc->MbParam->Mb,
        extMvData->Mv);

    return MFX_ERR_NONE;
}

#endif //MFX_ENABLE_H264_VIDEO_ENC_HW
