// Copyright (c) 2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_hyper_encode_hw.h"

#ifdef MFX_ENABLE_VIDEO_HYPER_ENCODE_HW

#include "mfx_session.h"
#include "libmfx_core_interface.h" // for MFXIEXTERNALLOC_GUID
#include "mfx_hyper_encode_hw_adapter.h"

mfxStatus GetDdiVersions(
    VideoCORE* core,
    mfxVideoParam* par,
    mfxU32* ddiVersion1,
    mfxU32* ddiVersion2)
{
    MFX_CHECK_NULL_PTR1(par);
    mfxVideoParamWrapper in_internal = *par, out_internal = *par;

    EncodeDdiVersion* encodeDdiVersion1 = QueryCoreInterface<EncodeDdiVersion>(core);
    mfxStatus sts = encodeDdiVersion1->GetDdiVersion(in_internal.mfx.CodecId, ddiVersion1);

    // if app does not call Query/QueryIOSurf, then DDI version for the first adapter will
    // not be available here. In this case let's call Query and request the version again
    if (sts != MFX_ERR_NONE) {
        sts = SingleGpuEncode::Query(core, &in_internal, &out_internal, 0);
        MFX_CHECK(sts >= MFX_ERR_NONE, sts);

        sts = encodeDdiVersion1->GetDdiVersion(in_internal.mfx.CodecId, ddiVersion1);
    }
    MFX_CHECK_STS(sts);

    // app session can be created on any of the adapters
    // the other adapter must be used for the second Query call
    mfxI32 adapter = -1;
    sts = HyperEncodeImpl::MFXQuerySecondAdapter(core->GetSession()->m_adapterNum, &adapter);
    MFX_CHECK_STS(sts);
    MFX_CHECK(adapter != -1, MFX_ERR_UNSUPPORTED);

    mfxSession dummy_session;
    sts = DummySession::Init(adapter, &dummy_session);
    MFX_CHECK_STS(sts);

    sts = SingleGpuEncode::Query(dummy_session->m_pCORE.get(), &in_internal, &out_internal, 0);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    EncodeDdiVersion* encodeDdiVersion2 = QueryCoreInterface<EncodeDdiVersion>(dummy_session->m_pCORE.get());
    sts = encodeDdiVersion2->GetDdiVersion(in_internal.mfx.CodecId, ddiVersion2);
    MFX_CHECK_STS(sts);

    sts = DummySession::Close(dummy_session);
    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus AreDdiVersionsCompatible(VideoCORE* core, mfxVideoParam* in)
{
#ifdef SINGLE_GPU_DEBUG
    return MFX_ERR_NONE;
#endif

    mfxU32 ddiVersion1 = 0, ddiVersion2 = 0;
    mfxStatus sts = GetDdiVersions(core, in, &ddiVersion1, &ddiVersion2);
    MFX_CHECK_STS(sts);

    if (ddiVersion1 && ddiVersion2) {
        MFX_CHECK(ddiVersion1 == ddiVersion2, MFX_ERR_UNSUPPORTED);
    }
    else {
        sts = MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return sts;
}

mfxStatus MFXHWVideoHyperENCODE::Query(
    VideoCORE *     core,
    mfxVideoParam * in,
    mfxVideoParam * out,
    void *          state)
{
    mfxVideoParamWrapper mfxHyperEncodeParam = *in;
    SetHyperMode(&mfxHyperEncodeParam);

    mfxHyperMode realOutHyperMode;
    GetHyperMode(in, realOutHyperMode);

    mfxStatus sts = ImplementationGopBased::Query(core, &mfxHyperEncodeParam, out, state);

    SetHyperMode(out, realOutHyperMode);

    return sts;
}

mfxStatus MFXHWVideoHyperENCODE::QueryIOSurf(
    VideoCORE *            core,
    mfxVideoParam *        par,
    mfxFrameAllocRequest * request)
{
    mfxVideoParamWrapper mfxHyperEncodeParam = *par;
    SetHyperMode(&mfxHyperEncodeParam);

    return ImplementationGopBased::QueryIOSurf(core, &mfxHyperEncodeParam, request);
}

MFXHWVideoHyperENCODE::MFXHWVideoHyperENCODE(VideoCORE* core, mfxVideoParam* par, mfxStatus* sts)
    : m_core(core)
{
    *sts = MFX_ERR_NONE;

    mfxVideoParamWrapper mfxHyperEncodeParam = *par;
    SetHyperMode(&mfxHyperEncodeParam);

    m_impl.reset(new ImplementationGopBased(m_core, &mfxHyperEncodeParam, sts));
}

mfxStatus MFXHWVideoHyperENCODE::Init(mfxVideoParam* par)
{
    mfxVideoParamWrapper mfxHyperEncodeParam = *par;
    SetHyperMode(&mfxHyperEncodeParam);

    return m_impl->Init(&mfxHyperEncodeParam);
}

mfxStatus ImplementationGopBased::CheckParams(mfxVideoParam* par)
{
    if (!par->mfx.GopPicSize)
        return MFX_ERR_UNSUPPORTED;

    if (MFX_CODEC_AVC != par->mfx.CodecId &&
        MFX_CODEC_HEVC != par->mfx.CodecId)
        return MFX_ERR_UNSUPPORTED;

    if (!IsOn(par->mfx.LowPower))
        return MFX_ERR_UNSUPPORTED;

    // according to IdrInterval member description in manual
    // https://github.com/Intel-Media-SDK/MediaSDK/blob/master/doc/mediasdk-man.md#mfxinfomfx
    if (MFX_CODEC_AVC == par->mfx.CodecId && 0 != par->mfx.IdrInterval)
        return MFX_ERR_UNSUPPORTED;

    if (MFX_CODEC_HEVC == par->mfx.CodecId && 1 != par->mfx.IdrInterval)
        return MFX_ERR_UNSUPPORTED;

    if (MFX_RATECONTROL_VBR != par->mfx.RateControlMethod &&
        MFX_RATECONTROL_CQP != par->mfx.RateControlMethod &&
        MFX_RATECONTROL_ICQ != par->mfx.RateControlMethod)
        return MFX_ERR_UNSUPPORTED;

    if (MFX_FOURCC_NV12 != par->mfx.FrameInfo.FourCC &&
        MFX_FOURCC_P010 != par->mfx.FrameInfo.FourCC &&
        MFX_FOURCC_RGB4 != par->mfx.FrameInfo.FourCC)
        return MFX_ERR_UNSUPPORTED;

    if (MFX_IOPATTERN_IN_SYSTEM_MEMORY != par->IOPattern &&
        MFX_IOPATTERN_IN_VIDEO_MEMORY != par->IOPattern)
        return MFX_ERR_UNSUPPORTED;

    mfxExtCodingOption* codingOption =
        (mfxExtCodingOption*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION);

    if (codingOption && (IsOn(codingOption->NalHrdConformance) || IsOn(codingOption->VuiNalHrdParameters)))
        return MFX_ERR_UNSUPPORTED;

#ifdef SINGLE_GPU_DEBUG
    return MFX_ERR_NONE;
#else
    // get adapters number
    mfxU32 numAdaptersAvailable = 0;
    mfxStatus sts = HyperEncodeImpl::MFXQueryAdaptersNumber(&numAdaptersAvailable);
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(numAdaptersAvailable == 2, MFX_ERR_UNSUPPORTED);

    // determine available adapters
    std::vector<mfxAdapterInfo> intelAdaptersData(numAdaptersAvailable);
    mfxAdaptersInfo intelAdapters = { intelAdaptersData.data(), mfxU32(intelAdaptersData.size()), 0u };

    sts = HyperEncodeImpl::MFXQueryAdapters(nullptr, &intelAdapters);
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_UNSUPPORTED);

    mfxU16 numAdaptersIntegrated = 0, numAdaptersDiscrete = 0;

    for (auto idx = intelAdapters.Adapters; idx != intelAdapters.Adapters + intelAdapters.NumActual; ++idx) {
        if (MFX_MEDIA_INTEGRATED == idx->Platform.MediaAdapterType)
            numAdaptersIntegrated++;
        if (MFX_MEDIA_DISCRETE == idx->Platform.MediaAdapterType)
            numAdaptersDiscrete++;
    }

    MFX_CHECK(numAdaptersIntegrated == 1 && numAdaptersDiscrete == 1, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
#endif
}

mfxStatus ImplementationGopBased::Query(
    VideoCORE* core,
    mfxVideoParam* in,
    mfxVideoParam* out,
    void* state)
{
    mfxStatus mfxRes = CheckParams(in);
    MFX_CHECK_STS(mfxRes);

    mfxRes = AreDdiVersionsCompatible(core, in);
    MFX_CHECK_STS(mfxRes);

    // TODO: query encoder for each adapter
    return SingleGpuEncode::Query(core, in, out, state);
}

mfxStatus ImplementationGopBased::QueryIOSurf(
    VideoCORE* core,
    mfxVideoParam* par,
    mfxFrameAllocRequest* request)
{
    mfxStatus mfxRes = CheckParams(par);
    MFX_CHECK_STS(mfxRes);

    mfxRes = AreDdiVersionsCompatible(core, par);
    MFX_CHECK_STS(mfxRes);

    // TODO: query encoder for each adapter
    return SingleGpuEncode::QueryIOSurf(core, par, request);
}

ImplementationGopBased::ImplementationGopBased(VideoCORE* core, mfxVideoParam* par, mfxStatus* sts)
{
    *sts = CheckParams(par);

    if (*sts == MFX_ERR_NONE)
        *sts = AreDdiVersionsCompatible(core, par);

    if (*sts == MFX_ERR_NONE)
        m_HyperEncode.reset(MFX_IOPATTERN_IN_SYSTEM_MEMORY == par->IOPattern ?
            (HyperEncodeBase*) new HyperEncodeSys(core->GetSession(), par, sts) :
            (HyperEncodeBase*) new HyperEncodeVideo(core->GetSession(), par, sts));
}

mfxStatus ImplementationGopBased::Init(mfxVideoParam* /*par*/)
{
    MFX_CHECK(m_HyperEncode.get(), MFX_ERR_NOT_INITIALIZED);

    // allocate surface pool for 2nd adapter
    mfxStatus sts = m_HyperEncode->AllocateSurfacePool();
    MFX_CHECK_STS(sts);

    return m_HyperEncode->Init();
}

mfxStatus ImplementationGopBased::Close()
{
    return m_HyperEncode.get()
        ? m_HyperEncode->Close()
        : MFX_ERR_NOT_INITIALIZED;
}

mfxStatus ImplementationGopBased::Reset(mfxVideoParam* par)
{
    return m_HyperEncode.get()
        ? m_HyperEncode->Reset(par)
        : MFX_ERR_NOT_INITIALIZED;
}

mfxStatus ImplementationGopBased::GetEncodeStat(mfxEncodeStat* stat)
{
    return m_HyperEncode.get()
        ? m_HyperEncode->GetEncodeStat(stat)
        : MFX_ERR_NOT_INITIALIZED;
}

mfxStatus ImplementationGopBased::GetFrameParam(mfxFrameParam* par)
{
    return m_HyperEncode.get()
        ? m_HyperEncode->GetFrameParam(par)
        : MFX_ERR_NOT_INITIALIZED;
}

mfxStatus ImplementationGopBased::GetVideoParam(mfxVideoParam* par)
{
    return m_HyperEncode.get()
        ? m_HyperEncode->GetVideoParam(par)
        : MFX_ERR_NOT_INITIALIZED;
}

mfxStatus ImplementationGopBased::EncodeFrame(
    mfxEncodeCtrl* ctrl,
    mfxEncodeInternalParams* internalParams,
    mfxFrameSurface1* surface,
    mfxBitstream* bs)
{
    return m_HyperEncode.get()
        ? m_HyperEncode->EncodeFrame(ctrl, internalParams, surface, bs)
        : MFX_ERR_NOT_INITIALIZED;
}

mfxStatus ImplementationGopBased::CancelFrame(
    mfxEncodeCtrl* ctrl,
    mfxEncodeInternalParams* internalParams,
    mfxFrameSurface1* surface,
    mfxBitstream* bs)
{
    return m_HyperEncode.get()
        ? m_HyperEncode->CancelFrame(ctrl, internalParams, surface, bs)
        : MFX_ERR_NOT_INITIALIZED;
}

mfxStatus ImplementationGopBased::EncodeFrameAsync(
    mfxEncodeCtrl* ctrl,
    mfxFrameSurface1* surface,
    mfxBitstream* bs,
    mfxSyncPoint* syncp)
{
    return m_HyperEncode.get()
        ? m_HyperEncode->EncodeFrameAsync(ctrl, surface, bs, syncp)
        : MFX_ERR_NOT_INITIALIZED;
}

mfxStatus ImplementationGopBased::Synchronize(
    mfxSession session,
    mfxSyncPoint syncp,
    mfxU32 wait)
{
    return m_HyperEncode.get()
        ? m_HyperEncode->Synchronize(session, syncp, wait)
        : MFX_ERR_NOT_INITIALIZED;
}

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
