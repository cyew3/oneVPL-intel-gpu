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

void SetHyperMode(mfxVideoParam* par, mfxHyperMode mode = MFX_HYPERMODE_OFF);
void GetHyperMode(mfxVideoParam* par, mfxHyperMode& HyperMode);

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

    virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus Close()
    {
        if (!m_impl.get())
            return MFX_ERR_NOT_INITIALIZED;

        mfxStatus sts = m_impl->Close();
        MFX_CHECK_STS(sts);

        m_impl.reset();

        return MFX_ERR_NONE;
    }

    virtual mfxTaskThreadingPolicy GetThreadingPolicy()
    {
        return m_impl.get()
            ? m_impl->GetThreadingPolicy()
            :  MFX_TASK_THREADING_INTRA;
    }

    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        if (!m_impl.get())
            return MFX_ERR_NOT_INITIALIZED;

        mfxHyperMode realHyperMode;
        GetHyperMode(par, realHyperMode);
        SetHyperMode(par);

        mfxStatus sts = m_impl->Reset(par);

        SetHyperMode(par, realHyperMode);

        return sts;
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        if (!m_impl.get())
            return MFX_ERR_NOT_INITIALIZED;

        mfxHyperMode realHyperMode;
        GetHyperMode(par, realHyperMode);
        SetHyperMode(par);

        mfxStatus sts = m_impl->GetVideoParam(par);

        SetHyperMode(par, realHyperMode);

        return sts;
    }

    virtual mfxStatus GetFrameParam(mfxFrameParam *par)
    {
        return m_impl.get()
            ? m_impl->GetFrameParam(par)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat)
    {
        return m_impl.get()
            ? m_impl->GetEncodeStat(stat)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus EncodeFrame(
        mfxEncodeCtrl *ctrl,
        mfxEncodeInternalParams *internalParams,
        mfxFrameSurface1 *surface,
        mfxBitstream *bs)
    {
        return m_impl.get()
            ? m_impl->EncodeFrame(ctrl, internalParams, surface, bs)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus CancelFrame(
        mfxEncodeCtrl* ctrl,
        mfxEncodeInternalParams* internalParams,
        mfxFrameSurface1* surface,
        mfxBitstream* bs)
    {
        return m_impl.get()
            ? m_impl->CancelFrame(ctrl, internalParams, surface, bs)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus EncodeFrameAsync(
        mfxEncodeCtrl* ctrl,
        mfxFrameSurface1* surface,
        mfxBitstream* bs,
        mfxSyncPoint* syncp)
    {
        return m_impl.get()
            ? m_impl->EncodeFrameAsync(ctrl, surface, bs, syncp)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus Synchronize(
        mfxSession session,
        mfxSyncPoint syncp,
        mfxU32 wait)
    {
        return m_impl.get()
            ? m_impl->Synchronize(session, syncp, wait)
            : MFX_ERR_NOT_INITIALIZED;
    }

public:
    static bool s_isSingleFallback;

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

    virtual mfxStatus Init(mfxVideoParam* par);

    virtual mfxStatus Close();

    virtual mfxStatus Reset(mfxVideoParam* par);

    virtual mfxStatus GetVideoParam(mfxVideoParam* par);

    virtual mfxStatus GetFrameParam(mfxFrameParam* par);

    virtual mfxStatus GetEncodeStat(mfxEncodeStat* stat);

    virtual mfxTaskThreadingPolicy GetThreadingPolicy()
    {
        return mfxTaskThreadingPolicy(MFX_TASK_THREADING_INTRA);
    }

    virtual mfxStatus EncodeFrame(
        mfxEncodeCtrl* ctrl,
        mfxEncodeInternalParams* internalParams,
        mfxFrameSurface1* surface,
        mfxBitstream* bs);

    virtual mfxStatus CancelFrame(
        mfxEncodeCtrl* ctrl,
        mfxEncodeInternalParams* internalParams,
        mfxFrameSurface1* surface,
        mfxBitstream* bs);

    virtual mfxStatus EncodeFrameAsync(
        mfxEncodeCtrl* ctrl,
        mfxFrameSurface1* surface,
        mfxBitstream* bs,
        mfxSyncPoint* syncp);

    virtual mfxStatus Synchronize(
        mfxSession session,
        mfxSyncPoint syncp,
        mfxU32 wait);

protected:
    std::unique_ptr<HyperEncodeBase> m_HyperEncode;
};

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
#endif // _MFX_HYPER_ENCODE_HW_H_
