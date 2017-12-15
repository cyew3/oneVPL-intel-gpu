//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#include "mfx_common_int.h"

#include "umc_defs.h"

#ifndef _MFX_AV1_DEC_DECODE_H_
#define _MFX_AV1_DEC_DECODE_H_

#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)

class mfx_UMC_FrameAllocator;

namespace UMC
{
    class VideoDecoderParams;
}

namespace UMC_AV1_DECODER
{
    class AV1Decoder;
    class AV1DecoderFrame;
}

class VideoDECODEAV1
    : public VideoDECODE
{
    struct TaskInfo
    {
        mfxFrameSurface1 *surface_work;
        mfxFrameSurface1 *surface_out;
    };

public:

    VideoDECODEAV1(VideoCORE*, mfxStatus*);
    virtual ~VideoDECODEAV1();

    static mfxStatus Query(VideoCORE*, mfxVideoParam* in, mfxVideoParam* out);
    static mfxStatus QueryIOSurf(VideoCORE*, mfxVideoParam*, mfxFrameAllocRequest*);
    static mfxStatus DecodeHeader(VideoCORE*, mfxBitstream*, mfxVideoParam*);

    mfxStatus Init(mfxVideoParam*);
    mfxStatus Reset(mfxVideoParam*);
    mfxStatus Close();
    mfxTaskThreadingPolicy GetThreadingPolicy();

    mfxStatus GetVideoParam(mfxVideoParam*);
    mfxStatus GetDecodeStat(mfxDecodeStat*);
    mfxStatus DecodeFrameCheck(mfxBitstream*, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, MFX_ENTRY_POINT*);
    mfxStatus SetSkipMode(mfxSkipMode);
    mfxStatus GetPayload(mfxU64* time_stamp, mfxPayload*);

    mfxStatus SubmitFrame(mfxBitstream*, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, mfxThreadTask*);
    mfxStatus ExecuteFrame(mfxThreadTask, mfxU32, mfxU32);
    mfxStatus CompleteFrame(mfxThreadTask, mfxStatus);

private:

    static mfxStatus FillVideoParam(VideoCORE*, UMC::VideoDecoderParams const*, mfxVideoParam*);
    static mfxStatus DecodeRoutine(void* state, void* param, mfxU32, mfxU32);
    static mfxStatus CompleteProc(void* state, void* param, mfxStatus);

    mfxStatus SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out);

    UMC_AV1_DECODER::AV1DecoderFrame* GetFrameToDisplay();
    void FillOutputSurface(mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, UMC_AV1_DECODER::AV1DecoderFrame*);

    mfxFrameSurface1* GetOriginalSurface(mfxFrameSurface1* surface)
    {
        VM_ASSERT(m_core);

        return m_opaque ?
            m_core->GetNativeSurface(surface) : surface;
    }

private:

    VideoCORE*                                   m_core;
    eMFXPlatform                                 m_platform;

    UMC::Mutex                                   m_guard;
    std::unique_ptr<mfx_UMC_FrameAllocator>      m_allocator;
    std::unique_ptr<UMC_AV1_DECODER::AV1Decoder> m_decoder;

    bool                                         m_opaque;
    bool                                         m_first_run;

    mfxVideoParamWrapper                         m_init_video_par;
    mfxVideoParamWrapper                         m_video_par;

    mfxFrameAllocResponse                        m_response;
};

#endif // MFX_ENABLE_AV1_VIDEO_DECODE

#endif // _MFX_AV1_DEC_DECODE_H_
