//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_SVC_VIDEO_ENCODE_HW)

#include "mfx_h264_encode_hw_utils.h"
#include "mfx_svc_encode_simulcast_over_avc.h"

using namespace MfxHwH264Encode;

DriverEncoder * MfxHwH264Encode::CreateMultipleAvcEncode(VideoCORE *)
{
    return new MultipleAvcEncoder();
}

MultipleAvcEncoder::MultipleAvcEncoder()
    : m_core(NULL)
    , m_reconRegCnt(0)
    , m_bitsrRegCnt(0)
    , m_forcedCodingFunction(0)
{
}

MultipleAvcEncoder::~MultipleAvcEncoder()
{
}

mfxStatus MultipleAvcEncoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    GUID        /*guid*/,
    mfxU32      width,
    mfxU32      height,
    bool        isTemporal)
{
    m_core = core;
    m_ddi[0].reset(CreatePlatformH264Encoder(core));
    return m_ddi[0]->CreateAuxilliaryDevice(core, DXVA2_Intel_Encode_AVC, width, height, isTemporal);
}

namespace
{
    void CopySpatialLayerParamToAvcParam(
        MfxVideoParam const &   svcPar,
        mfxU32                  did,
        MfxVideoParam &         avcPar)
    {
        mfxExtSVCSeqDesc const *     extSvc = GetExtBuffer(svcPar);
        mfxExtSVCRateControl const * extRc  = GetExtBuffer(svcPar);

        avcPar.mfx.FrameInfo.Width    = extSvc->DependencyLayer[did].Width;
        avcPar.mfx.FrameInfo.Height   = extSvc->DependencyLayer[did].Height;
        avcPar.mfx.FrameInfo.CropX    = extSvc->DependencyLayer[did].CropX;
        avcPar.mfx.FrameInfo.CropY    = extSvc->DependencyLayer[did].CropY;
        avcPar.mfx.FrameInfo.CropW    = extSvc->DependencyLayer[did].CropW;
        avcPar.mfx.FrameInfo.CropH    = extSvc->DependencyLayer[did].CropH;
        avcPar.mfx.GopPicSize         = extSvc->DependencyLayer[did].GopPicSize;
        avcPar.mfx.GopRefDist         = extSvc->DependencyLayer[did].GopRefDist;
        avcPar.mfx.GopOptFlag         = extSvc->DependencyLayer[did].GopOptFlag;
        avcPar.mfx.IdrInterval        = extSvc->DependencyLayer[did].IdrInterval;

        mfxU32 maxTemporalId = extSvc->DependencyLayer[did].TemporalId[extSvc->DependencyLayer[did].TemporalNum - 1];
        for (mfxU32 i = 0; i < extRc->NumLayers; i++)
        {
            if (extRc->Layer[i].DependencyId == did &&
                extRc->Layer[i].TemporalId   == maxTemporalId)
            {
                avcPar.mfx.RateControlMethod = extRc->RateControlMethod;
                if (avcPar.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
                {
                    avcPar.mfx.QPI = extRc->Layer[i].Cqp.QPI;
                    avcPar.mfx.QPP = extRc->Layer[i].Cqp.QPP;
                    avcPar.mfx.QPB = extRc->Layer[i].Cqp.QPB;
                }
                else if (avcPar.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
                {
                    avcPar.mfx.TargetKbps  = mfxU16(extRc->Layer[i].Avbr.TargetKbps);
                    avcPar.mfx.Convergence = mfxU16(extRc->Layer[i].Avbr.Convergence);
                    avcPar.mfx.Accuracy    = mfxU16(extRc->Layer[i].Avbr.Accuracy);
                }
                else
                {
                    avcPar.mfx.TargetKbps       = mfxU16(extRc->Layer[i].CbrVbr.TargetKbps);
                    avcPar.mfx.InitialDelayInKB = mfxU16(extRc->Layer[i].CbrVbr.InitialDelayInKB);
                    avcPar.mfx.BufferSizeInKB   = mfxU16(extRc->Layer[i].CbrVbr.BufferSizeInKB);
                    avcPar.mfx.MaxKbps          = mfxU16(extRc->Layer[i].CbrVbr.MaxKbps);
                }
            }
        }
    }
};

mfxStatus MultipleAvcEncoder::CreateAccelerationService(
    MfxVideoParam const & par)
{
    m_reconRegCnt = 0;
    m_bitsrRegCnt = 0;

    mfxExtSVCSeqDesc const * extSvc   = GetExtBuffer(par);

    MfxVideoParam localPar = par;

    for (mfxU32 i = 0; i < par.calcParam.numDependencyLayer; i++)
    {
        mfxU32 did = par.calcParam.did[i];

        m_ddi[did].reset(CreatePlatformH264Encoder(m_core));
        if (m_ddi[did].get() == 0)
            return MFX_ERR_DEVICE_FAILED;

        mfxStatus sts = m_ddi[did]->CreateAuxilliaryDevice(
            m_core,
            DXVA2_Intel_Encode_AVC,
            extSvc->DependencyLayer[did].Width,
            extSvc->DependencyLayer[did].Height);
        if (sts != MFX_ERR_NONE)
            return sts;

        CopySpatialLayerParamToAvcParam(par, did, localPar);

        sts = m_ddi[did]->CreateAccelerationService(localPar);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    return MFX_ERR_NONE;
}

mfxStatus MultipleAvcEncoder::Reset(
    MfxVideoParam const & par)
{
    MfxVideoParam localPar = par;
    for (mfxU32 i = 0; i < par.calcParam.numDependencyLayer; i++)
    {
        CopySpatialLayerParamToAvcParam(par, par.calcParam.did[i], localPar);

        mfxStatus sts = m_ddi[i]->Reset(localPar);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    return MFX_ERR_NONE;
}

mfxStatus MultipleAvcEncoder::Register(
    mfxFrameAllocResponse & response,
    D3DDDIFORMAT            type)
{
    if (type == D3DDDIFMT_NV12)
        return m_ddi[m_reconRegCnt++]->Register(response, type);
    else if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
        return m_ddi[m_bitsrRegCnt++]->Register(response, type);

    assert(!"invalid type");
    return MFX_ERR_NONE;
}

mfxStatus MultipleAvcEncoder::Execute(
    mfxHDLPair                 pair,
    DdiTask const &            task,
    mfxU32                     fieldId,
    PreAllocatedVector const & sei)
{
    return m_ddi[task.m_did]->Execute(pair, task, fieldId, sei);
}

mfxStatus MultipleAvcEncoder::QueryCompBufferInfo(
    D3DDDIFORMAT           type,
    mfxFrameAllocRequest & request)
{
    type, request;
    assert(!"MultipleAvcEncoder::QueryCompBufferInfo unsupported");
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MultipleAvcEncoder::QueryEncodeCaps(
    ENCODE_CAPS & caps)
{
    for (mfxU32 i = 0; i < 8; i++)
        if (m_ddi[i].get())
            return m_ddi[i]->QueryEncodeCaps(caps);
    assert(!"no initialized avc ddi in QueryEncodeCaps");
    return MFX_ERR_DEVICE_FAILED;
}

mfxStatus MultipleAvcEncoder::QueryMbPerSec(
    mfxVideoParam const & par,
    mfxU32              (&mbPerSec)[16])
{
    par;
    mbPerSec;

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MultipleAvcEncoder::QueryStatus(
    DdiTask & task,
    mfxU32    fieldId)
{
    assert(m_ddi[task.m_did].get() != 0);
    return m_ddi[task.m_did]->QueryStatus(task, fieldId);
}

mfxStatus MultipleAvcEncoder::Destroy()
{
    for (mfxU32 i = 0; i < 8; i++)
        if (m_ddi[i].get())
            m_ddi[i]->Destroy();

    return MFX_ERR_NONE;
}

#endif // #if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_SVC_VIDEO_ENCODE_HW)
