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

#define SURFACE_IDX_NOT_FOUND 0xFFFFFFFF

class mfxBitstreamWrapperWithLock : public mfxBitstream
{
public:
    mfxBitstreamWrapperWithLock()
        : mfxBitstream(),
        Locked(false)
    {}

    mfxBitstreamWrapperWithLock(mfxU32 n_bytes)
        : mfxBitstream()
    {
        Extend(n_bytes);
    }

    mfxBitstreamWrapperWithLock(const mfxBitstreamWrapperWithLock& bs_wrapper)
        : mfxBitstream(bs_wrapper)
        , m_data(bs_wrapper.m_data)
        , Locked(false)
    {
        Data = m_data.data();
    }

    mfxBitstreamWrapperWithLock& operator=(mfxBitstreamWrapperWithLock const& bs_wrapper)
    {
        mfxBitstreamWrapperWithLock tmp(bs_wrapper);

        *this = std::move(tmp);

        return *this;
    }

    mfxBitstreamWrapperWithLock(mfxBitstreamWrapperWithLock&& bs_wrapper) = default;
    mfxBitstreamWrapperWithLock& operator= (mfxBitstreamWrapperWithLock&& bs_wrapper) = default;
    ~mfxBitstreamWrapperWithLock() = default;

    void Extend(mfxU32 n_bytes)
    {
        if (MaxLength >= n_bytes)
            return;

        m_data.resize(n_bytes);

        Data = m_data.data();
        MaxLength = n_bytes;
    }

    bool Locked;
private:
    std::vector<mfxU8> m_data;
};

class HyperEncodeBase
{
public:
    HyperEncodeBase(mfxSession session, mfxVideoParam* par, mfxStatus* sts)
        :m_appSession(session)
    {
        m_gopSize = par->mfx.GopPicSize;
        *sts = MFX_ERR_NONE;
    }
    virtual ~HyperEncodeBase()
    {
        m_devMngr.reset();
        m_singleGpuEncoders.clear();
    }

    virtual mfxStatus AllocateSurfacePool(mfxVideoParam* par) = 0;
    virtual mfxStatus Init(mfxVideoParam* par) = 0;

    mfxStatus GetVideoParam(mfxVideoParam* par);
    mfxStatus GetFrameParam(mfxFrameParam* par);
    mfxStatus GetEncodeStat(mfxEncodeStat* stat);
    mfxStatus EncodeFrame(
        mfxEncodeCtrl* ctrl,
        mfxEncodeInternalParams* internalParams,
        mfxFrameSurface1* surface,
        mfxBitstream* bs);
    mfxStatus CancelFrame(
        mfxEncodeCtrl* ctrl,
        mfxEncodeInternalParams* internalParams,
        mfxFrameSurface1* surface,
        mfxBitstream* bs);
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
    mfxU32 GetFreeSurfaceIndex(mfxFrameSurface1** pSurfacesPool, mfxU32 nPoolSize);
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
        m_devMngr.reset(new DeviceManagerSys(session, sts));
        if (*sts == MFX_ERR_NONE)
            *sts = CreateEncoders();
    }
    virtual ~HyperEncodeSys() {}

    virtual mfxStatus AllocateSurfacePool(mfxVideoParam*)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Init(mfxVideoParam* par);

protected:
    virtual mfxStatus CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode);
};

class HyperEncodeVideo : public HyperEncodeBase
{
public:
    HyperEncodeVideo(mfxSession session, mfxVideoParam* par, mfxStatus* sts)
        :HyperEncodeBase(session, par, sts)
    {
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
        if (m_vppDoNotUse.AlgList)
            delete[] m_vppDoNotUse.AlgList;

        m_vppExtParams.clear();

        for (int i = 0; i < m_numFrameActual; i++)
            delete m_pMfxSurfaces[i];

        delete m_pMfxSurfaces;
    }

    virtual mfxStatus AllocateSurfacePool(mfxVideoParam* par);
    virtual mfxStatus Init(mfxVideoParam* par);

    virtual mfxStatus Close();

protected:
    mfxStatus CreateVPP(mfxVideoParam* par);
    mfxStatus InitVPPparams(mfxVideoParam* par);

    virtual mfxStatus InitSession(
        mfxSession* appSession, mfxSession* internalSession,
        mfxHandleType type, mfxHDL hdl, mfxIMPL impl, mfxU16 mediaAdapterType);

    virtual mfxStatus CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode);

protected:
    //vpp
    mfxSession m_mfxSessionVPP = nullptr;
    mfxVideoParam m_mfxVppParams = {};
    mfxExtVPPDoNotUse m_vppDoNotUse = {};
    std::vector<mfxExtBuffer*> m_vppExtParams;
    //allocator
    mfxFrameAllocator* m_pFrameAllocator = nullptr;
    mfxFrameAllocResponse m_singleEncResponse = {};
    mfxFrameSurface1** m_pMfxSurfaces = nullptr;
    mfxU16 m_numFrameActual = 0;
};

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
#endif // _MFX_HYPER_ENCODE_HW_MNGR_H_
