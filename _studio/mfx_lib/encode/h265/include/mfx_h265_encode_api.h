// Copyright (c) 2015-2018 Intel Corporation
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

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <memory>

#include "mfxdefs.h"
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfxplugin++.h"
#include "mfx_ext_buffers.h"
#include "mfxbrc.h"

namespace H265Enc {
    class H265Encoder;

    struct ExtBuffers {
        ExtBuffers();
        void CleanUp();
        mfxExtOpaqueSurfaceAlloc extOpaq;
        mfxExtCodingOptionHEVC   extOptHevc;
#ifdef MFX_UNDOCUMENTED_DUMP_FILES
        mfxExtDumpFiles          extDumpFiles;
#endif
        mfxExtHEVCTiles          extTiles;
        mfxExtHEVCRegion         extRegion;
        mfxExtHEVCParam          extHevcParam;
        mfxExtCodingOption       extOpt;
        mfxExtCodingOption2      extOpt2;
        mfxExtCodingOption3      extOpt3;
        mfxExtCodingOptionSPSPPS extSpsPps;
        mfxExtCodingOptionVPS    extVps;
        mfxExtEncoderROI         extRoi;  
        mfxExtBRC                extBRC;
        mfxExtBuffer            *extParamAll[13];

        static const size_t NUM_EXT_PARAM;
    };
}


class MFXCoreInterface1
{
public:
    mfxCoreInterface m_core;
public:
    MFXCoreInterface1()
        : m_core() {
    }
    MFXCoreInterface1(const mfxCoreInterface & pCore)
        : m_core(pCore) {
    }

    MFXCoreInterface1(const MFXCoreInterface1 & that)
        : m_core(that.m_core) {
    }
    MFXCoreInterface1 &operator = (const MFXCoreInterface1 & that)
    { 
        m_core = that.m_core;
        return *this;
    }
    virtual bool IsCoreSet() {
        return m_core.pthis != 0;
    }
    virtual mfxStatus GetCoreParam(mfxCoreParam *par) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetCoreParam(m_core.pthis, par);
    }
    virtual mfxStatus GetHandle (mfxHandleType type, mfxHDL *handle) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetHandle(m_core.pthis, type, handle);
    }
    virtual mfxStatus IncreaseReference (mfxFrameData *fd) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.IncreaseReference(m_core.pthis, fd);
    }
    virtual mfxStatus DecreaseReference (mfxFrameData *fd) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.DecreaseReference(m_core.pthis, fd);
    }
    virtual mfxStatus CopyFrame (mfxFrameSurface1 *dst, mfxFrameSurface1 *src) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.CopyFrame(m_core.pthis, dst, src);
    }
    virtual mfxStatus CopyBuffer(mfxU8 *dst, mfxU32 size, mfxFrameSurface1 *src) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.CopyBuffer(m_core.pthis, dst, size, src);
    }
    virtual mfxStatus MapOpaqueSurface(mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.MapOpaqueSurface(m_core.pthis, num, type, op_surf);
    }
    virtual mfxStatus UnmapOpaqueSurface(mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.UnmapOpaqueSurface(m_core.pthis, num, type, op_surf);
    }
    virtual mfxStatus GetRealSurface(mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetRealSurface(m_core.pthis, op_surf, surf);
    }
    virtual mfxStatus GetOpaqueSurface(mfxFrameSurface1 *surf, mfxFrameSurface1 **op_surf) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.GetOpaqueSurface(m_core.pthis, surf, op_surf);
    }
    virtual mfxStatus CreateAccelerationDevice(mfxHandleType type, mfxHDL *handle) {
        if (!IsCoreSet()) {
            return MFX_ERR_NULL_PTR;
        }
        return m_core.CreateAccelerationDevice(m_core.pthis, type, handle);
    }
    virtual mfxFrameAllocator & FrameAllocator() {
        return m_core.FrameAllocator;
    }
} ;

class MFXVideoENCODEH265 : public VideoENCODE {
public:
    static mfxStatus Query(MFXCoreInterface1 *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(MFXCoreInterface1 *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoENCODEH265(MFXCoreInterface1 *core, mfxStatus *sts);

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
    MFXCoreInterface1 *m_core;
    std::auto_ptr<H265Enc::H265Encoder> m_impl;

    mfxVideoParam       m_mfxParam;
    H265Enc::ExtBuffers m_extBuffers;
};


#endif // MFX_ENABLE_H265_VIDEO_ENCODE
