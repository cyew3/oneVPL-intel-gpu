//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016-2017 Intel Corporation. All Rights Reserved.
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
#include "mfx_vp9_encode_hw_utils.h"
#include "mfx_vp9_encode_hw_ddi.h"

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)
namespace MfxHwVP9Encode
{

class Plugin : public MFXEncoderPlugin
{
public:
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task);
    virtual mfxStatus ConfigTask(Task &task);
    virtual mfxStatus RemoveObsoleteParameters();
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 , mfxU32 );
    virtual mfxStatus FreeResources(mfxThreadTask , mfxStatus );
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *);
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual void Release()
    {
        delete this;
    }
    static MFXEncoderPlugin* Create() {
        return new Plugin(false);
    }

    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
    {
        if (memcmp(& guid , &MFX_PLUGINID_VP9E_HW, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }

        Plugin* tmp_pplg = 0;

        try
        {
            tmp_pplg = new Plugin(false);
        }

        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }

        catch(...)
        {
            return MFX_ERR_UNKNOWN;;
        }

        *mfxPlg = tmp_pplg->m_adapter;
        tmp_pplg->m_createdByDispatcher = true;

        return MFX_ERR_NONE;
    }

    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_ENCODE;
    }

    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNKNOWN;
    }

    virtual mfxStatus UpdateBitstream(Task & task);

protected:
    Plugin(bool CreateByDispatcher);
    virtual ~Plugin(){};

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

    mfxCoreInterface    *m_pmfxCore;

    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
    MFXPluginAdapter<MFXEncoderPlugin> m_adapter;

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

    bool m_isFirstFrameSubmitted;
    mfxU32 m_firstFrameWidth;
    mfxU32 m_firstFrameHeight;
};

} // MfxHwVP9Encode

#endif // PRE_SI_TARGET_PLATFORM_GEN10
