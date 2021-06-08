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

#ifndef _MFX_HYPER_ENCODE_HW_REAL_ENC_H_
#define _MFX_HYPER_ENCODE_HW_REAL_ENC_H_

#include "mfx_session.h"
#include "mfx_hyper_encode_hw_adapter.h"

#ifdef MFX_ENABLE_VIDEO_HYPER_ENCODE_HW

typedef enum {
    MFX_ENCODER_NUM1,
    MFX_ENCODER_NUM2,
    MFX_ENCODERS_COUNT
} mfxEncoderNum;

class ExtVideoENCODE: public VideoENCODE
{
public:
    virtual mfxStatus EncodeFrameCheck(
        mfxEncodeCtrl*,
        mfxFrameSurface1*,
        mfxBitstream*,
        mfxFrameSurface1**,
        mfxEncodeInternalParams*)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus EncodeFrameAsync(
        mfxEncodeCtrl* ctrl,
        mfxFrameSurface1* surface,
        mfxBitstream* bs,
        mfxSyncPoint* syncp) = 0;

    virtual mfxStatus Synchronize(
        mfxSession session,
        mfxSyncPoint syncp, 
        mfxU32 wait) = 0;
};

class SingleGpuEncode : public ExtVideoENCODE
{
public:
    static mfxStatus Query(
        mfxMediaAdapterType adapterType,
        VideoCORE* core,
        mfxVideoParam* in,
        mfxVideoParam* out,
        void*)
    {
        // get type of app adapter
        mfxAdapterInfo info{};
        mfxStatus mfxRes = HyperEncodeImpl::MFXQueryCorePlatform(core, &info);
        MFX_CHECK_STS(mfxRes);

        if (info.Platform.MediaAdapterType == adapterType) {
            return MFXVideoENCODE_Query(core->GetSession(), in, out);
        }

        // create dummy_session for second adapter 
        mfxU32 adapter;
        mfxRes = HyperEncodeImpl::MFXQuerySecondAdapter(core->GetSession()->m_adapterNum, &adapter);
        MFX_CHECK_STS(mfxRes);

        mfxSession dummy_session;
        mfxRes = DummySession::Init(adapter, &dummy_session);
        MFX_CHECK_STS(mfxRes);

        // check dummy_session adapter type
        mfxRes = HyperEncodeImpl::MFXQueryCorePlatform(dummy_session->m_pCORE.get(), &info);
        if (mfxRes != MFX_ERR_NONE) {
            DummySession::Close(dummy_session);
            return mfxRes;
        }
        if (info.Platform.MediaAdapterType != adapterType) {
            DummySession::Close(dummy_session);
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        mfxRes = MFXVideoENCODE_Query(dummy_session, in, out);

        DummySession::Close(dummy_session);

        return mfxRes;
    }

    static mfxStatus Query(
        VideoCORE* core,
        mfxVideoParam* in,
        mfxVideoParam* out,
        void*)
    {
        return MFXVideoENCODE_Query(core->GetSession(), in, out);
    }

    static mfxStatus QueryIOSurf(
        mfxMediaAdapterType adapterType,
        VideoCORE* core,
        mfxVideoParam* par,
        mfxFrameAllocRequest* request)
    {
        // get type of app adapter
        mfxAdapterInfo info{};
        mfxStatus mfxRes = HyperEncodeImpl::MFXQueryCorePlatform(core, &info);
        MFX_CHECK_STS(mfxRes);

        if (info.Platform.MediaAdapterType == adapterType) {
            return MFXVideoENCODE_QueryIOSurf(core->GetSession(), par, request);
        }

        // create dummy_session for second adapter 
        mfxU32 adapter;
        mfxRes = HyperEncodeImpl::MFXQuerySecondAdapter(core->GetSession()->m_adapterNum, &adapter);
        MFX_CHECK_STS(mfxRes);

        mfxSession dummy_session;
        mfxRes = DummySession::Init(adapter, &dummy_session);
        MFX_CHECK_STS(mfxRes);

        // check dummy_session adapter type
        mfxRes = HyperEncodeImpl::MFXQueryCorePlatform(dummy_session->m_pCORE.get(), &info);
        if (mfxRes != MFX_ERR_NONE) {
            DummySession::Close(dummy_session);
            return mfxRes;
        }
        if (info.Platform.MediaAdapterType != adapterType) {
            DummySession::Close(dummy_session);
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        mfxRes = MFXVideoENCODE_QueryIOSurf(dummy_session, par, request);

        DummySession::Close(dummy_session);

        return mfxRes;
    }

    static mfxStatus QueryIOSurf(
        VideoCORE* core,
        mfxVideoParam* par,
        mfxFrameAllocRequest* request)
    {
        return MFXVideoENCODE_QueryIOSurf(core->GetSession(), par, request);
    }

    SingleGpuEncode(VideoCORE* core, mfxMediaAdapterType adapterType, mfxEncoderNum encoderNum)
        : m_adapterType(adapterType)
        , m_encoderNum(encoderNum)
    {
        m_session = core->GetSession();
    }

    virtual ~SingleGpuEncode() {}

    mfxStatus Init(mfxVideoParam* par) override
    {
        return MFXVideoENCODE_Init(m_session, par);
    }

    mfxStatus Close() override
    {
        return MFXVideoENCODE_Close(m_session);
    }

    mfxStatus Reset(mfxVideoParam* par) override
    {
        MFX_CHECK(m_session, MFX_ERR_NOT_INITIALIZED);
        MFX_CHECK(m_session->m_pENCODE.get(), MFX_ERR_NOT_INITIALIZED);

        mfxStatus sts = m_session->m_pScheduler->WaitForAllTasksCompletion(m_session->m_pENCODE.get());
        MFX_CHECK_STS(sts);

        return m_session->m_pENCODE->Reset(par);
    }

    mfxStatus GetVideoParam(mfxVideoParam* par) override
    {
        return m_session->m_pENCODE.get()
            ? m_session->m_pENCODE->GetVideoParam(par)
            : MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus GetFrameParam(mfxFrameParam* par) override
    {
        return m_session->m_pENCODE.get()
            ? m_session->m_pENCODE->GetFrameParam(par)
            : MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus GetEncodeStat(mfxEncodeStat* stat) override
    {
        return m_session->m_pENCODE.get()
            ? m_session->m_pENCODE->GetEncodeStat(stat)
            : MFX_ERR_NOT_INITIALIZED;
    }

    mfxTaskThreadingPolicy GetThreadingPolicy() override
    {
        return mfxTaskThreadingPolicy(MFX_TASK_THREADING_INTRA);
    }

    mfxStatus EncodeFrame(
        mfxEncodeCtrl* ctrl,
        mfxEncodeInternalParams* internalParams,
        mfxFrameSurface1* surface,
        mfxBitstream* bs) override
    {
        return m_session->m_pENCODE.get()
            ? m_session->m_pENCODE->EncodeFrame(ctrl, internalParams, surface, bs)
            : MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus CancelFrame(
        mfxEncodeCtrl* ctrl,
        mfxEncodeInternalParams* internalParams,
        mfxFrameSurface1* surface,
        mfxBitstream* bs) override
    {
        return m_session->m_pENCODE.get()
            ? m_session->m_pENCODE->CancelFrame(ctrl, internalParams, surface, bs)
            : MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus EncodeFrameAsync(
        mfxEncodeCtrl* ctrl,
        mfxFrameSurface1* surface,
        mfxBitstream* bs,
        mfxSyncPoint* syncp) override
    {
        return MFXVideoENCODE_EncodeFrameAsync(m_session, ctrl, surface, bs, syncp);
    }

    mfxStatus Synchronize(
        mfxSession session,
        mfxSyncPoint syncp,
        mfxU32 wait) override
    {
        return session->m_pScheduler->Synchronize(syncp, wait);
    }

public:
    mfxMediaAdapterType m_adapterType;
    mfxEncoderNum m_encoderNum;
    mfxSession m_session;
};

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
#endif // _MFX_HYPER_ENCODE_HW_REAL_ENC_H_
