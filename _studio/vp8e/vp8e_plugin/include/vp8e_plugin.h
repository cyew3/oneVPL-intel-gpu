//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include <stdio.h>
#include <memory>
#include <vector>
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfxplugin++.h"
#include "mfx_utils.h"
#include "mfx_vp8_enc_common_hw.h"
#include "mfx_vp8_encode_utils_hw.h"
#include "mfx_vp8_encode_hybrid_bse.h"
#include <umc_mutex.h>

class MFX_VP8E_Plugin : public MFXEncoderPlugin
{
    static const mfxPluginUID g_PluginGuid;
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
        return new MFX_VP8E_Plugin(false);
    }
    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {
        if (memcmp(& guid , &g_PluginGuid, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }
        MFX_VP8E_Plugin* tmp_pplg = 0;
        try
        {
            tmp_pplg = new MFX_VP8E_Plugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        catch(...)
        {
            return MFX_ERR_UNKNOWN;;
        }

        try
        {
            tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXEncoderPlugin> (tmp_pplg));
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        *mfxPlg = tmp_pplg->m_adapter->operator mfxPlugin();
        tmp_pplg->m_createdByDispatcher = true;
        return MFX_ERR_NONE;
    }
    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_VPP;
    }
    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNKNOWN;
    }

protected:
    MFX_VP8E_Plugin(bool CreateByDispatcher);
    virtual ~MFX_VP8E_Plugin(){};

    MFX_VP8ENC::VP8MfxParam                     m_video;
    MFX_VP8ENC::TaskManagerHybridPakDDI       * m_pTaskManager;
    MFX_VP8ENC::Vp8CoreBSE                      m_BSE;
    std::auto_ptr <MFX_VP8ENC::DriverEncoder>   m_ddi;

    UMC::Mutex                      m_taskMutex;

    bool                            m_bStartIVFSequence;

    mfxCoreInterface*    m_pmfxCore;

    mfxPluginParam      m_PluginParam;
    mfxVideoParam       m_mfxpar;
    bool                m_createdByDispatcher;
    std::auto_ptr<MFXPluginAdapter<MFXEncoderPlugin> > m_adapter;
};

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#endif