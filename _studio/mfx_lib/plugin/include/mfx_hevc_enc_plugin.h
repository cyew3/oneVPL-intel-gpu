/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mfx_hevc_dec_plugin.h

\* ****************************************************************************** */

#if !defined(__MFX_HEVC_ENC_PLUGIN_INCLUDED__)
#define __MFX_HEVC_ENC_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include "mfxvideo.h"
#include "mfxplugin++.h"

#if defined( AS_HEVCE_PLUGIN )
class MFXHEVCEncoderPlugin : public MFXEncoderPlugin
{
    static const mfxPluginUID g_HEVCEncoderGuid;
    static std::auto_ptr<MFXHEVCEncoderPlugin> g_singlePlg;
    static std::auto_ptr<MFXPluginAdapter<MFXEncoderPlugin> > g_adapter;
public:
    MFXHEVCEncoderPlugin(bool CreateByDispatcher);
    virtual ~MFXHEVCEncoderPlugin();

    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task)
    {
        return MFXVideoENCODE_EncodeFrameAsync(m_session, ctrl, surface, bs, (mfxSyncPoint*) task);
    }
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 , mfxU32 )
    {
        return MFXVideoCORE_SyncOperation(m_session, (mfxSyncPoint) task, MFX_INFINITE);
    }
    virtual mfxStatus FreeResources(mfxThreadTask , mfxStatus )
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        return MFXVideoENCODE_Query(m_session, in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *)
    {
        return MFXVideoENCODE_QueryIOSurf(m_session, par, in);
    }
    virtual mfxStatus Init(mfxVideoParam *par)
    {
        return MFXVideoENCODE_Init(m_session, par);
    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        return MFXVideoENCODE_Reset(m_session, par);
    }
    virtual mfxStatus Close()
    {
        return MFXVideoENCODE_Close(m_session);
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return MFXVideoENCODE_GetVideoParam(m_session, par);
    }
    virtual void Release()
    {
        delete this;
    }
    static MFXEncoderPlugin* Create() {
        return new MFXHEVCEncoderPlugin(false);
    }
    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {

        if (g_singlePlg.get()) {
            return MFX_ERR_NOT_FOUND;
        } 
        //check that guid match 
        g_singlePlg.reset(new MFXHEVCEncoderPlugin(true));

        if (memcmp(& guid , &g_HEVCEncoderGuid, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }

        g_adapter.reset(new MFXPluginAdapter<MFXEncoderPlugin> (g_singlePlg.get()));
        *mfxPlg = g_adapter->operator mfxPlugin();

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

protected:

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
};
#endif //#if defined( AS_HEVCE_PLUGIN )

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

#endif  // __MFX_HEVC_ENC_PLUGIN_INCLUDED__
