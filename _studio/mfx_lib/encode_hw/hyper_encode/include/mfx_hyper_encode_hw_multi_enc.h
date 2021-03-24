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

#ifndef _MFX_HYPER_ENCODE_HW_MNGR_H_
#define _MFX_HYPER_ENCODE_HW_MNGR_H_

#include "mfx_common.h"

#ifdef MFX_ENABLE_VIDEO_HYPER_ENCODE_HW

#include <queue>
#include <mutex>
#include <algorithm>

#include "mfx_hyper_encode_hw_dev_mngr.h"
#include "mfx_hyper_encode_hw_single_enc.h"

class HyperEncodeBase
{
public:
    HyperEncodeBase(mfxSession session, mfxVideoParam* par, mfxStatus* sts)
        :m_appSession(session)
    {
        *sts = MFX_ERR_NONE;
        m_gopSize = par->mfx.GopPicSize;

        if (!m_gopSize)
            *sts = MFX_ERR_NOT_INITIALIZED;
    }
    virtual ~HyperEncodeBase()
    {
        m_devMngr.reset();
        m_singleGpuEncoders.clear();
    }

    virtual mfxStatus AllocateSurfacePool(mfxVideoParam* par) = 0;
    virtual mfxStatus Init(mfxVideoParam* par) = 0;

    mfxStatus GetVideoParam(mfxVideoParam* par);
    mfxStatus GetFrameParam(mfxFrameParam* /*par*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    mfxStatus GetEncodeStat(mfxEncodeStat* /*stat*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    mfxStatus EncodeFrame(
        mfxEncodeCtrl* /*ctrl*/,
        mfxEncodeInternalParams* /*internalParams*/,
        mfxFrameSurface1* /*surface*/,
        mfxBitstream* /*bs*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    mfxStatus CancelFrame(
        mfxEncodeCtrl* /*ctrl*/,
        mfxEncodeInternalParams* /*internalParams*/,
        mfxFrameSurface1* /*surface*/,
        mfxBitstream* /*bs*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    mfxStatus Reset(mfxVideoParam* par);

    virtual mfxStatus EncodeFrameAsync(
        mfxEncodeCtrl* ctrl,
        mfxFrameSurface1* surface,
        mfxBitstream* bs,
        mfxSyncPoint* syncp);

    virtual mfxStatus Synchronize(
        mfxSession session,
        mfxSyncPoint syncp,
        mfxU32 wait);

    virtual mfxStatus Close();

protected:
    virtual mfxStatus InitSession(
        mfxSession* appSession, mfxSession* internalSession,
        mfxHandleType type, mfxHDL hdl, mfxIMPL impl, mfxU16 mediaAdapterType);
    
    mfxStatus CreateEncoders();
    mfxU16 GetAdapterTypeByFrame(mfxU32 frameNum, mfxU16 gopSize);

    virtual mfxStatus CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode) = 0;
    mfxBitstreamWrapperWithLock* GetFreeBitstream(mfxU16 adapterType);

protected:
    mfxSession m_appSession = nullptr;
    std::unique_ptr<DeviceManagerBase> m_devMngr;
    std::vector<std::unique_ptr<SingleGpuEncode>> m_singleGpuEncoders;

    std::map<SingleGpuEncode*, std::vector<mfxBitstreamWrapperWithLock*>> m_bitstreams;
    struct EncodingTasks
    {
        mfxSyncPoint syncp = nullptr;
        mfxBitstreamWrapperWithLock* internalBst = nullptr;
        mfxBitstream* appBst = nullptr;
        mfxSession session = nullptr;
    };
    std::queue<EncodingTasks> m_submittedTasks;
    std::map<mfxSyncPoint, EncodingTasks> m_waitingSyncOpTasks;

    std::mutex m_mutex;

    mfxU32 m_surfaceNum = 0;
    mfxU16 m_gopSize = 0;
};

class HyperEncodeSys : public HyperEncodeBase
{
public:
    HyperEncodeSys(mfxSession session, mfxVideoParam* par, mfxStatus* sts)
        :HyperEncodeBase(session, par, sts)
    {
        if (*sts == MFX_ERR_NONE)
            m_devMngr.reset(new DeviceManagerSys(session, sts));
        if (*sts == MFX_ERR_NONE)
            *sts = CreateEncoders();
    }
    virtual ~HyperEncodeSys() {}

    mfxStatus AllocateSurfacePool(mfxVideoParam*) override
    {
        return MFX_ERR_NONE;
    }
    mfxStatus Init(mfxVideoParam* par) override;

protected:
    mfxStatus CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode) override;
};

class HyperEncodeVideo : public HyperEncodeBase
{
public:
    HyperEncodeVideo(mfxSession session, mfxVideoParam* par, mfxStatus* sts)
        :HyperEncodeBase(session, par, sts)
    {
        if (*sts == MFX_ERR_NONE)
            m_devMngr.reset(new DeviceManagerVideo(session, sts));
        if (*sts == MFX_ERR_NONE) {
            m_pFrameAllocator = m_devMngr->GetInternalAllocator();
            *sts = CreateEncoders();
        }
        if (*sts == MFX_ERR_NONE)
            *sts = InitVPPparams(par);
        if (*sts == MFX_ERR_NONE)
            *sts = CreateVPP(par);
    }
    virtual ~HyperEncodeVideo()
    {
        m_vppExtParams.clear();
        m_pMfxSurfaces.clear();
    }

    mfxStatus AllocateSurfacePool(mfxVideoParam* par) override;
    mfxStatus Init(mfxVideoParam* par) override;

    mfxStatus Close() override;

protected:
    mfxStatus CreateVPP(mfxVideoParam* par);
    mfxStatus InitVPPparams(mfxVideoParam* par);

    mfxStatus InitSession(
        mfxSession* appSession, mfxSession* internalSession,
        mfxHandleType type, mfxHDL hdl, mfxIMPL impl, mfxU16 mediaAdapterType) override;

    mfxStatus CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode) override;

protected:
    mfxSession m_mfxSessionVPP = nullptr;
    
    mfxVideoParam m_mfxVppParams = {};
    std::vector<mfxExtBuffer*> m_vppExtParams;
    std::unique_ptr<mfxU32> m_algList;
    mfxExtVPPDoNotUse m_vppDoNotUse = {};

    mfxFrameAllocator* m_pFrameAllocator = nullptr;
    mfxFrameAllocResponse m_singleEncResponse = {};
    std::vector<std::unique_ptr<mfxFrameSurface1>> m_pMfxSurfaces;
};

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
#endif // _MFX_HYPER_ENCODE_HW_MNGR_H_
