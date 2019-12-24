// Copyright (c) 2014-2019 Intel Corporation
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


#pragma once

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include <memory>
#include "mfx_ext_buffers.h"

namespace AV1Enc {
    class AV1Encoder;

    struct ExtBuffers {
        ExtBuffers();
        void CleanUp();
        mfxExtOpaqueSurfaceAlloc extOpaq;
        mfxExtCodingOptionAV1E   extOptAv1;
        mfxExtDumpFiles          extDumpFiles;
        mfxExtHEVCTiles          extTiles;
        mfxExtHEVCRegion         extRegion;
        mfxExtHEVCParam          extHevcParam;
        mfxExtCodingOption       extOpt;
        mfxExtCodingOption2      extOpt2;
        mfxExtCodingOption3      extOpt3;
        mfxExtCodingOptionSPSPPS extSpsPps;
        mfxExtCodingOptionVPS    extVps;
        mfxExtEncoderROI         extRoi;
        mfxExtAvcTemporalLayers  extTlayers;
        mfxExtVP9Param           extVP9Param;
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        mfxExtAV1Param           extAV1Param;
        mfxExtBuffer            *extParamAll[15];
#else
        mfxExtBuffer            *extParamAll[14];
#endif

        static const size_t NUM_EXT_PARAM;
    };
}

class MFXCoreInterface1;
class MFXVideoENCODEAV1 : public VideoENCODE {
public:
    static mfxStatus Query(MFXCoreInterface1 *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(MFXCoreInterface1 *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoENCODEAV1(MFXCoreInterface1 *core, mfxStatus *sts);

    virtual ~MFXVideoENCODEAV1();

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
    MFXCoreInterface1 *m_core;
    std::unique_ptr<AV1Enc::AV1Encoder> m_impl;

    mfxVideoParam       m_mfxParam;
    AV1Enc::ExtBuffers m_extBuffers;
};

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE

