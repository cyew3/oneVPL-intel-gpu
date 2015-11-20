//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once
#include <memory>
#include <list>
#include "mfx_h265_encode_hw_ddi.h"
#include "mfx_h265_encode_hw_utils.h"
#include "umc_mutex.h"
#include "mfxplugin++.h"

namespace MfxHwH265Encode
{

static const mfxPluginUID  MFX_PLUGINID_HEVCE_HW = {{0x6f, 0xad, 0xc7, 0x91, 0xa0, 0xc2, 0xeb, 0x47, 0x9a, 0xb6, 0xdc, 0xd5, 0xea, 0x9d, 0xa3, 0x47}};

class Plugin : public MFXEncoderPlugin
{
public:
    static MFXEncoderPlugin* Create()
    {
        return new Plugin(false);
    }

    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
    {
        if (memcmp(& guid , &MFX_PLUGINID_HEVCE_HW, sizeof(mfxPluginUID)))
        {
            return MFX_ERR_NOT_FOUND;
        }

        Plugin* tmp_pplg = 0;

        try
        {
            tmp_pplg = new Plugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
        catch(...)
        {
            return MFX_ERR_UNKNOWN;
        }

        *mfxPlg = tmp_pplg->m_adapter;
        tmp_pplg->m_createdByDispatcher = true;

        return MFX_ERR_NONE;
    }

    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
    virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus sts);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task);

    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_ENCODE;
    }

    virtual void Release()
    {
        delete this;
    }

    virtual mfxStatus Close();

    virtual mfxStatus SetAuxParams(void*, int)
    {
        return MFX_ERR_UNSUPPORTED;
    }

protected:
    explicit Plugin(bool CreateByDispatcher);
    virtual ~Plugin();

    mfxStatus InitImpl(mfxVideoParam *par);
    void      FreeResources();
    void      WaitingForAsyncTasks(bool bResetTasks);
    bool m_createdByDispatcher;
    MFXPluginAdapter<MFXEncoderPlugin> m_adapter;

    mfxStatus FreeTask(Task& task);

    std::auto_ptr<DriverEncoder>    m_ddi;
    MFXCoreInterface                m_core;

    MfxVideoParam                   m_vpar;
    ENCODE_CAPS_HEVC                m_caps;

    MfxFrameAllocResponse           m_raw;
    MfxFrameAllocResponse           m_rec;
    MfxFrameAllocResponse           m_bs;

    TaskManager                     m_task;
    Task                            m_lastTask;
    HRD                             m_hrd;
    mfxU32                          m_frameOrder;
    mfxU32                          m_lastIDR;
    mfxU32                          m_baseLayerOrder;
    mfxU32                          m_numBuffered;
    bool                            m_bInit;
};

} //MfxHwH265Encode
