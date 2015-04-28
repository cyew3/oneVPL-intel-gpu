//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <memory>

#include "mfxdefs.h"
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfx_ext_buffers.h"

namespace H265Enc {
    class H265Encoder;

    struct ExtBuffers {
        ExtBuffers();
        void CleanUp();
        mfxExtOpaqueSurfaceAlloc extOpaq;
        mfxExtCodingOptionHEVC   extOptHevc;
        mfxExtDumpFiles          extDumpFiles;
        mfxExtHEVCTiles          extTiles;
        mfxExtHEVCRegion         extRegion;
        mfxExtHEVCParam          extHevcParam;
        mfxExtCodingOption2      extOpt2;
        mfxExtCodingOptionSPSPPS extSpsPps;
        mfxExtBuffer            *extParamAll[8];

        static const size_t NUM_EXT_PARAM;
    };
}

class MFXVideoENCODEH265 : public VideoENCODE {
public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoENCODEH265(VideoCORE *core, mfxStatus *sts);

    virtual ~MFXVideoENCODEH265();

    virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus Close();

    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void) { return MFX_TASK_THREADING_INTRA; }

    virtual mfxStatus Reset(mfxVideoParam *par);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);

    virtual mfxStatus EncodeFrameCheck(
        mfxEncodeCtrl *ctrl,
        mfxFrameSurface1 *surface,
        mfxBitstream *bs,
        mfxFrameSurface1 **reordered_surface,
        mfxEncodeInternalParams *pInternalParams,
        MFX_ENTRY_POINT *pEntryPoints);

    virtual mfxStatus GetFrameParam(mfxFrameParam*) { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl*,mfxFrameSurface1*,mfxBitstream*,mfxFrameSurface1**,mfxEncodeInternalParams*) { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl*,mfxEncodeInternalParams*,mfxFrameSurface1*,mfxBitstream*) { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus CancelFrame(mfxEncodeCtrl*,mfxEncodeInternalParams*,mfxFrameSurface1*,mfxBitstream*) { return MFX_ERR_UNSUPPORTED; }

protected:
    mfxStatus AllocOpaqSurfaces();
    void FreeOpaqSurfaces();

protected:
    VideoCORE *m_core;
    std::auto_ptr<H265Enc::H265Encoder> m_impl;

    mfxVideoParam       m_mfxParam;
    H265Enc::ExtBuffers m_extBuffers;

    mfxFrameAllocResponse m_responseOpaq;
};


#endif // MFX_ENABLE_H265_VIDEO_ENCODE
