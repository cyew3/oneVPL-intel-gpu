//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

#pragma once

#include <stdio.h>
#include <memory>
#include <vector>
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfxplugin++.h"
#include "mfx_utils.h"
#include <umc_mutex.h>
#include "mfx_vp9_encode_hw_utils.h"
#include "mfx_vp9_encode_hw_ddi.h"

#if defined (AS_VP9E_PLUGIN)
namespace MfxHwVP9Encode
{

class Plugin : public MFXEncoderPlugin
{
public:
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task);
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
    TaskManagerVmePlusPak        *m_pTaskManager;
    std::auto_ptr <DriverEncoder> m_ddi;
    UMC::Mutex m_taskMutex;
    bool       m_bStartIVFSequence;
    mfxU64     m_maxBsSize;

    mfxCoreInterface*    m_pmfxCore;

    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
    MFXPluginAdapter<MFXEncoderPlugin> m_adapter;

    bool m_initialized;
};

} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN