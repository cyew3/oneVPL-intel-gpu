//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2010 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_H264_PAK_H_
#define _MFX_H264_PAK_H_

#include "mfx_common.h"
#ifdef MFX_ENABLE_H264_VIDEO_PAK

#include "mfxvideo++int.h"
#include "mfx_h264_pak_utils.h"
#include "mfx_h264_pak_pack.h"

class MFXVideoPAKH264 : public VideoPAK {
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoPAKH264(VideoCORE *core, mfxStatus *status);
    virtual ~MFXVideoPAKH264();

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);

    virtual mfxStatus RunSeqHeader(mfxFrameCUC *bs);
    virtual mfxStatus RunFramePAK(mfxFrameCUC *cuc);

private:
    VideoCORE* m_core;
    mfxVideoParam m_videoParam;
    mfxFrameParam m_frameParam;

    H264Pak::SeqScalingMatrices8x8 m_scaling8x8;
    H264Pak::PredThreadPool m_threads;
    H264Pak::H264PakBitstream m_bitstream;
    H264Pak::PakResources m_res;
};

#endif // MFX_ENABLE_H264_VIDEO_PAK
#endif // _MFX_H264_PAK_H_
