//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2009 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#ifdef MFX_ENABLE_H264_VIDEO_ENC_HW

#ifndef _MFX_H264_ENC_HW_H_
#define _MFX_H264_ENC_HW_H_

#include "mfxvideo++int.h"
#include "mfx_h264_enc_common_hw.h"

class MFXHWVideoENCH264 : public VideoENC
{
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXHWVideoENCH264(VideoCORE *core, mfxStatus *status);
    virtual ~MFXHWVideoENCH264();

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);

    virtual mfxStatus RunFrameVmeENC(mfxFrameCUC *cuc);

private:
    VideoCORE *m_core;
    MfxHwH264Encode::MfxVideoParam m_videoParam;
    mfxFrameParam m_frameParam;

    bool m_reconRegistered;
    MfxHwH264Encode::DdiEncoder m_ddi;
    MfxHwH264Encode::ExecuteBuffers m_ddiData;

    MfxHwH264Encode::Initialized<mfxFrameAllocResponse> m_mb;
    MfxHwH264Encode::Initialized<mfxFrameAllocResponse> m_mv;
    MfxHwH264Encode::Initialized<mfxFrameAllocResponse> m_recons;
    MfxHwH264Encode::Initialized<ENCODE_MBDATA_LAYOUT> m_layout;
};

MFXHWVideoENCH264* CreateMFXHWVideoENCH264(VideoCORE *core, mfxStatus *status);

#endif //_MFX_H264_ENC_HW_H_

#endif //MFX_ENABLE_H264_VIDEO_ENC_HW
