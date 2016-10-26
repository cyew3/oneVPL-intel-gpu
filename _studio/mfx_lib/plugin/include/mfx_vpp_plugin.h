//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_HEVC_VPP_PLUGIN_INCLUDED__)
#define __MFX_HEVC_VPP_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include "mfxvideo.h"
#include "mfxplugin++.h"
#include "mfxvideo++int.h"

#if defined( AS_VPP_PLUGIN )
class MFXVPP_Plugin : public MFXVPPPlugin
{
    static const mfxPluginUID g_VPP_PluginGuid;
public:
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task) 
    {
        return MFXVideoVPP_RunFrameVPPAsync(m_session, surface_in, surface_out, aux, (mfxSyncPoint*) task);
    }
    virtual mfxStatus VPPFrameSubmitEx(mfxFrameSurface1 *, mfxFrameSurface1 *, mfxFrameSurface1 **, mfxThreadTask *) 
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
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
        return MFXVideoVPP_Query(m_session, in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *)
    {
        return MFXVideoVPP_QueryIOSurf(m_session, par, in);
    }
    virtual mfxStatus Init(mfxVideoParam *par)
    {
        return MFXVideoVPP_Init(m_session, par);
    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        return MFXVideoVPP_Reset(m_session, par);
    }
    virtual mfxStatus Close()
    {
        return MFXVideoVPP_Close(m_session);
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return MFXVideoVPP_GetVideoParam(m_session, par);
    }
    virtual void Release()
    {
        delete this;
    }
    static MFXVPPPlugin* Create() {
        return new MFXVPP_Plugin(false);
    }
    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {
        if (memcmp(& guid , &g_VPP_PluginGuid, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }
        MFXVPP_Plugin* tmp_pplg = 0;
        try
        {
            tmp_pplg = new MFXVPP_Plugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        catch(MFX_CORE_CATCH_TYPE) 
        { 
            return MFX_ERR_UNKNOWN;; 
        }
        tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXVPPPlugin> (tmp_pplg));
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
    MFXVPP_Plugin(bool CreateByDispatcher);
    virtual ~MFXVPP_Plugin();

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
    std::auto_ptr<MFXPluginAdapter<MFXVPPPlugin> > m_adapter;
};
#endif //#if defined( AS_VPP_PLUGIN )

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type 
#endif

#endif  // __MFX_HEVC_VPP_PLUGIN_INCLUDED__
