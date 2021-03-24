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

#ifndef _MFX_HYPER_ENCODE_HW_H_
#define _MFX_HYPER_ENCODE_HW_H_

#include "mfx_common.h"

#ifdef MFX_ENABLE_VIDEO_HYPER_ENCODE_HW

#include "mfx_hyper_encode_hw_multi_enc.h"

static inline void SetHyperMode(mfxVideoParam* par, mfxHyperMode mode = MFX_HYPERMODE_OFF)
{
    mfxExtHyperModeParam* hyperMode = 
        (mfxExtHyperModeParam*)GetExtendedBufferInternal(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_HYPER_MODE_PARAM);
    hyperMode->Mode = mode;
}

static inline void GetHyperMode(mfxVideoParam* par, mfxHyperMode& mode)
{
    mode = MFX_HYPERMODE_OFF;
    mfxExtHyperModeParam* hyperMode = 
        (mfxExtHyperModeParam*)GetExtendedBufferInternal(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_HYPER_MODE_PARAM);
    mode = hyperMode->Mode;
}

class MFXHWVideoHyperENCODE : public ExtVideoENCODE
{
public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, void * state = 0);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXHWVideoHyperENCODE(VideoCORE* core, mfxStatus* sts)
        : m_core(core)
    {
        if (sts)
            *sts = MFX_ERR_NONE;
    }

    mfxStatus Init(mfxVideoParam *par) override;

    mfxStatus Close() override
    {
        MFX_CHECK(m_impl.get(), MFX_ERR_NOT_INITIALIZED);

        mfxStatus sts = m_impl->Close();
        MFX_CHECK_STS(sts);

        m_impl.reset();

        return sts;
    }

    mfxTaskThreadingPolicy GetThreadingPolicy() override
    {
        return m_impl.get()
            ? m_impl->GetThreadingPolicy()
            :  MFX_TASK_THREADING_INTRA;
    }

    mfxStatus Reset(mfxVideoParam *par) override
    {
        MFX_CHECK(m_impl.get(), MFX_ERR_NOT_INITIALIZED);

        mfxHyperMode realHyperMode;
        GetHyperMode(par, realHyperMode);
        SetHyperMode(par);

        mfxStatus sts = m_impl->Reset(par);

        SetHyperMode(par, realHyperMode);

        return sts;
    }

    mfxStatus GetVideoParam(mfxVideoParam *par) override
    {
        MFX_CHECK(m_impl.get(), MFX_ERR_NOT_INITIALIZED);

        mfxHyperMode realHyperMode;
        GetHyperMode(par, realHyperMode);
        SetHyperMode(par);

        mfxStatus sts = m_impl->GetVideoParam(par);

        SetHyperMode(par, realHyperMode);

        return sts;
    }

    mfxStatus GetFrameParam(mfxFrameParam *par) override
    {
        return m_impl.get()
            ? m_impl->GetFrameParam(par)
            : MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus GetEncodeStat(mfxEncodeStat *stat) override
    {
        return m_impl.get()
            ? m_impl->GetEncodeStat(stat)
            : MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus EncodeFrame(
        mfxEncodeCtrl *ctrl,
        mfxEncodeInternalParams *internalParams,
        mfxFrameSurface1 *surface,
        mfxBitstream *bs) override
    {
        return m_impl.get()
            ? m_impl->EncodeFrame(ctrl, internalParams, surface, bs)
            : MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus CancelFrame(
        mfxEncodeCtrl* ctrl,
        mfxEncodeInternalParams* internalParams,
        mfxFrameSurface1* surface,
        mfxBitstream* bs) override
    {
        return m_impl.get()
            ? m_impl->CancelFrame(ctrl, internalParams, surface, bs)
            : MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus EncodeFrameAsync(
        mfxEncodeCtrl* ctrl,
        mfxFrameSurface1* surface,
        mfxBitstream* bs,
        mfxSyncPoint* syncp) override
    {
        return m_impl.get()
            ? m_impl->EncodeFrameAsync(ctrl, surface, bs, syncp)
            : MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus Synchronize(
        mfxSession session,
        mfxSyncPoint syncp,
        mfxU32 wait) override
    {
        return m_impl.get()
            ? m_impl->Synchronize(session, syncp, wait)
            : MFX_ERR_NOT_INITIALIZED;
    }

protected:
    VideoCORE* m_core;
    std::unique_ptr<ExtVideoENCODE> m_impl;
};

class ImplementationGopBased : public ExtVideoENCODE
{
public:
    static mfxStatus CheckParams(mfxVideoParam* par);

    static mfxStatus Query(
        VideoCORE* core,
        mfxVideoParam* in,
        mfxVideoParam* out,
        void* state = 0);

    static mfxStatus QueryIOSurf(
        VideoCORE* core,
        mfxVideoParam* par,
        mfxFrameAllocRequest* request);

    ImplementationGopBased(VideoCORE* core, mfxVideoParam* par, mfxStatus* sts);

    virtual ~ImplementationGopBased()
    {
        m_HyperEncode.reset();
    }

    mfxStatus Init(mfxVideoParam* par) override;

    mfxStatus Close() override;

    mfxStatus Reset(mfxVideoParam* par) override;

    mfxStatus GetVideoParam(mfxVideoParam* par) override;

    mfxStatus GetFrameParam(mfxFrameParam* par) override;

    mfxStatus GetEncodeStat(mfxEncodeStat* stat) override;

    mfxTaskThreadingPolicy GetThreadingPolicy() override
    {
        return mfxTaskThreadingPolicy(MFX_TASK_THREADING_INTRA);
    }

    mfxStatus EncodeFrame(
        mfxEncodeCtrl* ctrl,
        mfxEncodeInternalParams* internalParams,
        mfxFrameSurface1* surface,
        mfxBitstream* bs) override;

    mfxStatus CancelFrame(
        mfxEncodeCtrl* ctrl,
        mfxEncodeInternalParams* internalParams,
        mfxFrameSurface1* surface,
        mfxBitstream* bs) override;

    mfxStatus EncodeFrameAsync(
        mfxEncodeCtrl* ctrl,
        mfxFrameSurface1* surface,
        mfxBitstream* bs,
        mfxSyncPoint* syncp) override;

    mfxStatus Synchronize(
        mfxSession session,
        mfxSyncPoint syncp,
        mfxU32 wait) override;

protected:
    std::unique_ptr<HyperEncodeBase> m_HyperEncode;
};

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
#endif // _MFX_HYPER_ENCODE_HW_H_
