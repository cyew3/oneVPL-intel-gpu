/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mfx_hevc_dec_plugin.h

\* ****************************************************************************** */

#if !defined(__MFX_HEVC_VPP_PLUGIN_INCLUDED__)
#define __MFX_HEVC_VPP_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include "mfxvideo.h"
#include "mfxplugin++.h"

#if defined( AS_VPP_PLUGIN )
class MFXVPP_Plugin : public MFXVPPPlugin
{
public:
    MFXVPP_Plugin();
    virtual ~MFXVPP_Plugin();

    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task) 
    {
        return MFXVideoVPP_RunFrameVPPAsync(m_session, surface_in, surface_out, aux, (mfxSyncPoint*) task);
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
        return new MFXVPP_Plugin();
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

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
};
#endif //#if defined( AS_VPP_PLUGIN )

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

#endif  // __MFX_HEVC_VPP_PLUGIN_INCLUDED__
