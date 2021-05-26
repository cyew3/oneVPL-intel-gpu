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
    HyperEncodeBase(mfxSession session, mfxVideoParam* par, bool isEncSupportedOnIntegrated, bool isEncSupportedOnDiscrete, mfxStatus* /*sts*/)
        :m_appSession(session)
        , m_isEncSupportedOnIntegrated(isEncSupportedOnIntegrated)
        , m_isEncSupportedOnDiscrete(isEncSupportedOnDiscrete)
        , m_mfxEncParams(par)
    {};

    virtual ~HyperEncodeBase()
    {
        m_devMngr.reset();
        m_singleGpuEncoders.clear();
        m_bitstreams.clear();

        while (m_submittedTasks.size())
            m_submittedTasks.pop();

        m_waitingSyncOpTasks.clear();
    }

    virtual mfxStatus AllocateSurfacePool() = 0;
    virtual mfxStatus Init() = 0;

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

    virtual mfxStatus Reset(mfxVideoParam* par);

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
        mfxHandleType type, mfxHDL hdl, mfxAccelerationMode accelMode, mfxMediaAdapterType mediaAdapterType, mfxU32 adapterNum);
    
    mfxStatus CreateEncoder(mfxMediaAdapterType adapterType, mfxEncoderNum encoderNum);
    mfxStatus ConfigureEncodersPool();

    virtual mfxStatus CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode) = 0;
    mfxBitstreamWrapperWithLock* GetFreeBitstream(mfxEncoderNum adapterType);

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

    mfxVideoParam* m_mfxEncParams = nullptr;

    mfxSession m_appPlatformInternalSession = nullptr;

    bool m_areAllEncodersOnAppPlatform = true;

    bool m_isEncSupportedOnIntegrated = false;
    bool m_isEncSupportedOnDiscrete = false;
};

class HyperEncodeSys : public HyperEncodeBase
{
public:
    HyperEncodeSys(mfxSession session, mfxVideoParam* par, bool isEncSupportedOnIntegrated, bool isEncSupportedOnDiscrete, mfxStatus* sts)
        :HyperEncodeBase(session, par, isEncSupportedOnIntegrated, isEncSupportedOnDiscrete, sts)
    {
        if (*sts == MFX_ERR_NONE)
            m_devMngr.reset(new DeviceManagerSys(session, sts));

        if (*sts == MFX_ERR_NONE)
            *sts = ConfigureEncodersPool();
    }
    virtual ~HyperEncodeSys() {}

    mfxStatus AllocateSurfacePool() override
    {
        return MFX_ERR_NONE;
    }
    mfxStatus Init() override;

protected:
    mfxStatus CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode) override;
};

class HyperEncodeVideo : public HyperEncodeBase
{
public:
    HyperEncodeVideo(mfxSession session, mfxVideoParam* par, bool isEncSupportedOnIntegrated, bool isEncSupportedOnDiscrete, mfxStatus* sts)
        :HyperEncodeBase(session, par, isEncSupportedOnIntegrated, isEncSupportedOnDiscrete, sts)
    {
        if (*sts == MFX_ERR_NONE)
            m_devMngr.reset(new DeviceManagerVideo(session, sts));

        if (*sts == MFX_ERR_NONE) {
            m_pFrameAllocator = m_devMngr->GetInternalAllocator();
            *sts = ConfigureEncodersPool();
        }
        // if all encoders on app's adapter - we don't need copy surfaces
        if (!m_areAllEncodersOnAppPlatform) {
            if (*sts == MFX_ERR_NONE)
                *sts = InitVPPparams();

            if (*sts == MFX_ERR_NONE)
                *sts = CreateVPP();
        }
    }
    virtual ~HyperEncodeVideo()
    {
        m_vppExtParams.clear();
        m_pMfxSurfaces.clear();
    }

    mfxStatus AllocateSurfacePool() override;
    mfxStatus Init() override;
    mfxStatus Reset(mfxVideoParam* par) override;
    mfxStatus Close() override;

protected:
    mfxStatus CreateVPP();
    mfxStatus InitVPPparams();

    mfxStatus InitSession(
        mfxSession* appSession, mfxSession* internalSession,
        mfxHandleType type, mfxHDL hdl, mfxAccelerationMode accelMode, mfxMediaAdapterType mediaAdapterType, mfxU32 adapterNum) override;

    mfxStatus CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode) override;

protected:
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
