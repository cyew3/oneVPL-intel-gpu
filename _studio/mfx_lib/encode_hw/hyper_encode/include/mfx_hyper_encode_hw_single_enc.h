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

#ifdef MFX_ENABLE_VIDEO_HYPER_ENCODE_HW

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
        VideoCORE* core,
        mfxVideoParam* in,
        mfxVideoParam* out,
        void*)
    {
        return MFXVideoENCODE_Query(core->GetSession(), in, out);
    }

    static mfxStatus QueryIOSurf(
        VideoCORE* core,
        mfxVideoParam* par,
        mfxFrameAllocRequest* request)
    {
        return MFXVideoENCODE_QueryIOSurf(core->GetSession(), par, request);
    }

    SingleGpuEncode(VideoCORE* core, mfxU16 adapterType = MFX_MEDIA_INTEGRATED)
        : m_adapterType(adapterType)
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
    mfxU16 m_adapterType;
    mfxSession m_session;
};

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
#endif // _MFX_HYPER_ENCODE_HW_REAL_ENC_H_
