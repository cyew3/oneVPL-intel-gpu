/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE_HW) && defined(MFX_VA)


#ifndef _MFX_VP8_ENCODE_HW_H_
#define _MFX_VP8_ENCODE_HW_H_

class MFXHWVideoENCODEVP8 : public VideoENCODE
{
public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXHWVideoENCODEVP8(VideoCORE *core, mfxStatus *sts)
        : m_core(core)
    {
        if (sts)
        {
            *sts = MFX_ERR_NONE;
        }
    }

    virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus Close()
    {
        m_impl.reset();
        return MFX_ERR_NONE;
    }

    virtual mfxTaskThreadingPolicy GetThreadingPolicy()
    {
        return MFX_TASK_THREADING_DEDICATED_WAIT;
    }

    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        return m_impl.get()
            ? m_impl->Reset(par)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return m_impl.get()
            ? m_impl->GetVideoParam(par)
            : MFX_ERR_NOT_INITIALIZED;
    }
    virtual mfxStatus GetFrameParam(mfxFrameParam *) {return MFX_ERR_UNSUPPORTED;}

    
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat)
    {
        return m_impl.get()
            ? m_impl->GetEncodeStat(stat)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus EncodeFrameCheck(
        mfxEncodeCtrl *ctrl,
        mfxFrameSurface1 *surface,
        mfxBitstream *bs,
        mfxFrameSurface1 **reordered_surface,
        mfxEncodeInternalParams *internalParams)
    {
        return m_impl.get()
            ? m_impl->EncodeFrameCheck(ctrl, surface, bs, reordered_surface, internalParams)
            : MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus EncodeFrameCheck(
        mfxEncodeCtrl *ctrl,
        mfxFrameSurface1 *surface,
        mfxBitstream *bs,
        mfxFrameSurface1 **reordered_surface,
        mfxEncodeInternalParams *pInternalParams,
        MFX_ENTRY_POINT pEntryPoints[],
        mfxU32 &numEntryPoints)
    {
        return m_impl.get()
            ? m_impl->EncodeFrameCheck(ctrl, surface, bs, reordered_surface, pInternalParams, pEntryPoints, numEntryPoints)
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

private:
    VideoCORE*                 m_core;
    std::auto_ptr<VideoENCODE> m_impl;
};


#endif // _MFX_VP8_ENCODE_HW_H_
#endif // MFX_ENABLE_VP8_VIDEO_ENCODE && MFX_VA
/* EOF */
