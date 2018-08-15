//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016-2018 Intel Corporation. All Rights Reserved.
//

#pragma once

#include <stdio.h>
#include <memory>
#include <queue>
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfxplugin++.h"
#include "mfx_utils.h"
#include <umc_mutex.h>
#include <mfx_task.h>
#include "mfx_vp9_encode_hw_utils.h"
#include "mfx_vp9_encode_hw_ddi.h"

namespace MfxHwVP9Encode
{
class MFXVideoENCODEVP9_HW : public VideoENCODE
{
public:
    MFXVideoENCODEVP9_HW(mfxCoreInterface *core, mfxStatus *status)
        : m_bStartIVFSequence(false)
        , m_maxBsSize(0)
        , m_initialized(false)
        , m_frameArrivalOrder(0)
        , m_drainState(false)
        , m_resetBrc(false)
        , m_taskIdForDriver(0)
        , m_numBufferedFrames(0)
        , m_initWidth(0)
        , m_initHeight(0)
        , m_frameOrderInGop(0)
        , m_frameOrderInRefStructure(0)
    {
        m_prevSegment.Header.BufferId = MFX_EXTBUFF_VP9_SEGMENTATION;
        m_prevSegment.Header.BufferSz = sizeof(mfxExtVP9Segmentation);
        ZeroExtBuffer(m_prevSegment);

        Zero(m_prevFrameParam);

        if (core)
            m_pCore = core;

        if (status)
            *status = MFX_ERR_NONE;
    }
    virtual ~MFXVideoENCODEVP9_HW()
    {
        Close();
    }

    static mfxStatus QueryIOSurf(mfxCoreInterface *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    static mfxStatus Query(mfxCoreInterface *core, mfxVideoParam *in, mfxVideoParam *out);

    virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus Reset(mfxVideoParam *par);

    virtual mfxStatus Close();

    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void) { return MFX_TASK_THREADING_INTRA; }

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual mfxStatus GetFrameParam(mfxFrameParam * /*par*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus GetEncodeStat(mfxEncodeStat * /*stat*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl * /*ctrl*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/, mfxFrameSurface1 ** /*reordered_surface*/, mfxEncodeInternalParams * /*pInternalParams*/)
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 ** /*reordered_surface*/, mfxEncodeInternalParams * /*pInternalParams*/, MFX_ENTRY_POINT *pEntryPoint)
    {
        mfxThreadTask userParam;

        mfxStatus mfxRes = EncodeFrameSubmit(ctrl, surface, bs, &userParam);
        if (mfxRes >= MFX_ERR_NONE || mfxRes == MFX_ERR_MORE_DATA_SUBMIT_TASK)
        {
            pEntryPoint->pState = this;
            pEntryPoint->pRoutine = Execute;
            pEntryPoint->pCompleteProc = FreeResources;
            pEntryPoint->requiredNumThreads = 1;
            pEntryPoint->pParam = userParam;
        }

        return mfxRes;
    }

    virtual mfxStatus EncodeFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/)
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus CancelFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/)
    {
        return MFX_ERR_NONE;
    }

    static mfxStatus Execute(void *pState, void *task, mfxU32 uid_p, mfxU32 uid_a)
    {
        if (pState)
            return ((MFXVideoENCODEVP9_HW*)pState)->Execute(task, uid_p, uid_a);
        else
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);

    static mfxStatus FreeResources(void *pState, void *task, mfxStatus sts)
    {
        if (pState)
            return ((MFXVideoENCODEVP9_HW*)pState)->FreeResources(task, sts);
        else
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus sts);

    virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task);
    virtual mfxStatus ConfigTask(Task &task);
    virtual mfxStatus RemoveObsoleteParameters();
    virtual mfxStatus UpdateBitstream(Task & task);

    bool isInitialized()
    {
        return m_initialized;
    }

protected:
    VP9MfxVideoParam              m_video;
    std::list<VP9MfxVideoParam>   m_videoForParamChange; // encoder keeps several versions of encoding parameters
                                                         // to allow dynamic parameter change w/o drain of all buffered tasks.
                                                         // Tasks submitted before the change reference to previous parameters version.
    std::auto_ptr <DriverEncoder> m_ddi;
    UMC::Mutex m_taskMutex;
    bool       m_bStartIVFSequence;
    mfxU64     m_maxBsSize;

    std::vector<Task> m_tasks;
    std::list<Task> m_free;
    std::list<Task> m_accepted;
    std::list<Task> m_submitted;
    std::queue<mfxBitstream*> m_outs;

    ExternalFrames  m_rawFrames;
    InternalFrames  m_rawLocalFrames;
    InternalFrames  m_reconFrames;
    InternalFrames  m_outBitstreams;
    InternalFrames  m_segmentMaps;

    std::vector<sFrameEx*> m_dpb;

    mfxCoreInterface    *m_pCore;

    bool m_initialized;

    mfxU32 m_frameArrivalOrder;

    bool m_drainState;
    bool m_resetBrc;

    mfxU32 m_taskIdForDriver; // should be unique among all the frames submitted to driver

    mfxI32 m_numBufferedFrames;

    mfxU16 m_initWidth;
    mfxU16 m_initHeight;

    mfxU32 m_frameOrderInGop;
    mfxU32 m_frameOrderInRefStructure; // reset of ref structure happens at key-frames and after dynamic scaling

    mfxExtVP9Segmentation m_prevSegment;
    VP9FrameLevelParam m_prevFrameParam;
};

class Plugin : public MFXEncoderPlugin
{
public:
    static MFXEncoderPlugin* Create() {
        return new Plugin(false);
    }

    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
    {
        if (memcmp(&guid, &MFX_PLUGINID_VP9E_HW, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }

        Plugin* tmp_pplg = 0;

        try
        {
            tmp_pplg = new Plugin(false);
        }

        catch (std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }

        catch (...)
        {
            return MFX_ERR_UNKNOWN;;
        }

        *mfxPlg = tmp_pplg->m_adapter;
        tmp_pplg->m_createdByDispatcher = true;

        return MFX_ERR_NONE;
    }
    virtual mfxStatus PluginInit(mfxCoreInterface *core)
    {
        if (!core)
            return MFX_ERR_NULL_PTR;

        m_pmfxCore = core;

        return MFX_ERR_NONE;
    }
    virtual mfxStatus PluginClose()
    {
        if (m_createdByDispatcher) {
            delete this;
        }

        return MFX_ERR_NONE;
    }

    virtual mfxStatus GetPluginParam(mfxPluginParam *par)
    {
        if (!par)
            return MFX_ERR_NULL_PTR;
        *par = m_PluginParam;

        return MFX_ERR_NONE;
    }
    virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task)
    {
        if (m_pImpl.get())
            return m_pImpl->EncodeFrameSubmit(ctrl, surface, bs, task);
        else
            return MFX_ERR_NOT_INITIALIZED;
    }
    virtual mfxStatus ConfigTask(Task &task)
    {
        if (m_pImpl.get())
            return m_pImpl->ConfigTask(task);
        else
            return MFX_ERR_NOT_INITIALIZED;
    }
    virtual mfxStatus RemoveObsoleteParameters()
    {
        if (m_pImpl.get())
            return m_pImpl->RemoveObsoleteParameters();
        else
            return MFX_ERR_NOT_INITIALIZED;
    }
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
    {
        if (m_pImpl.get())
            return MFXVideoENCODEVP9_HW::Execute((reinterpret_cast<void*>(m_pImpl.get())), task, uid_p, uid_a);
        else
            return MFX_ERR_NOT_INITIALIZED;
    }
    virtual mfxStatus FreeResources(mfxThreadTask, mfxStatus)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        return MFXVideoENCODEVP9_HW::Query(m_pmfxCore, in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest * /*out*/)
    {
        return MFXVideoENCODEVP9_HW::QueryIOSurf(m_pmfxCore, par, in);

    }
    virtual mfxStatus Init(mfxVideoParam *par)
    {
        mfxStatus sts = MFX_ERR_NONE;
        if (m_pImpl.get() && m_pImpl->isInitialized())
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        m_pImpl.reset(new MFXVideoENCODEVP9_HW(m_pmfxCore, &sts));
        MFX_CHECK_STS(sts);
        return m_pImpl->Init(par);
    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        if (m_pImpl.get())
            return m_pImpl->Reset(par);
        else
            return MFX_ERR_NOT_INITIALIZED;
    }
    virtual mfxStatus Close()
    {
        if (m_pImpl.get())
            return m_pImpl->Close();
        else
            return MFX_ERR_NOT_INITIALIZED;
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        if (m_pImpl.get())
            return m_pImpl->GetVideoParam(par);
        else
            return MFX_ERR_NOT_INITIALIZED;
    }
    virtual void Release()
    {
        delete this;
    }

    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNKNOWN;
    }

    virtual mfxStatus UpdateBitstream(Task & task)
    {
        if (m_pImpl.get())
            return m_pImpl->UpdateBitstream(task);
        else
            return MFX_ERR_NOT_INITIALIZED;
    }

protected:
    explicit Plugin(bool CreateByDispatcher)
        : m_PluginParam()
        , m_createdByDispatcher(CreateByDispatcher)
        , m_adapter(this)
    {
        m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
        m_PluginParam.MaxThreadNum = 1;
        m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
        m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
        m_PluginParam.PluginUID = MFX_PLUGINID_VP9E_HW;
        m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
        m_PluginParam.CodecId = MFX_CODEC_VP9;
        m_PluginParam.PluginVersion = 1;
    }
    virtual ~Plugin() {}

    mfxCoreInterface    *m_pmfxCore;

    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
    MFXPluginAdapter<MFXEncoderPlugin> m_adapter;
    std::unique_ptr<MFXVideoENCODEVP9_HW> m_pImpl;
};

} // MfxHwVP9Encode