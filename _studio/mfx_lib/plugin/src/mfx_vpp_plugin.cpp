//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_vpp_plugin.h"
#include "mfx_session.h"
#include "vm_sys_info.h"

#include "mfx_plugin_module.h"

#include "plugin_version_linux.h"

PluginModuleTemplate g_PluginModule = {
    NULL,
    NULL,
    NULL,
    &MFXVPP_Plugin::Create,
    &MFXVPP_Plugin::CreateByDispatcher
};

MSDK_PLUGIN_API(MFXVPPPlugin*) mfxCreateVPPPlugin() {
    if (!g_PluginModule.CreateVPPPlugin) {
        return 0;
    }
    return g_PluginModule.CreateVPPPlugin();
}

MSDK_PLUGIN_API(MFXPlugin*) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
    if (!g_PluginModule.CreatePlugin) {
        return 0;
    }
    return (MFXPlugin*) g_PluginModule.CreatePlugin(uid, plugin);
}

const mfxPluginUID MFXVPP_Plugin::g_VPP_PluginGuid = {0x18,0x40,0xbc,0xe0,0xee,0x64,0x4d,0x09,0x8e,0xd6,0x2f,0x91,0xae,0x4b,0x6b,0x61};

MFXVPP_Plugin::MFXVPP_Plugin(bool CreateByDispatcher)
{
    m_session = 0;
    m_pmfxCore = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_VPP_PluginGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_VPP;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;
}

MFXVPP_Plugin::~MFXVPP_Plugin()
{
    if (m_session)
    {
        PluginClose();
    }
}

mfxStatus MFXVPP_Plugin::PluginInit(mfxCoreInterface *core)
{
    if (!core)
        return MFX_ERR_NULL_PTR;
    mfxCoreParam par;
    mfxStatus mfxRes = MFX_ERR_NONE;

    m_pmfxCore = core;
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &par);
    MFX_CHECK_STS(mfxRes);

#if !defined (MFX_VA) && defined (AS_VPP_PLUGIN)
    par.Impl = MFX_IMPL_SOFTWARE;
#endif

    mfxRes = MFXInit(par.Impl, &par.Version, &m_session);
    MFX_CHECK_STS(mfxRes);

    mfxRes = MFXInternalPseudoJoinSession((mfxSession) m_pmfxCore->pthis, m_session);
    MFX_CHECK_STS(mfxRes);

    return mfxRes;
}

mfxStatus MFXVPP_Plugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxStatus mfxRes2 = MFX_ERR_NONE;
    if (m_session)
    {
        //The application must ensure there is no active task running in the session before calling this (MFXDisjoinSession) function.
        mfxRes = MFXVideoVPP_Close(m_session);
        //Return the first met wrn or error
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED)
            mfxRes2 = mfxRes;
        mfxRes = MFXInternalPseudoDisjoinSession(m_session);
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED && mfxRes2 == MFX_ERR_NONE)
            mfxRes2 = mfxRes;
        mfxRes = MFXClose(m_session);
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED && mfxRes2 == MFX_ERR_NONE)
            mfxRes2 = mfxRes;
        m_session = 0;
    }
    if (m_createdByDispatcher) {
        delete this;
    }

    return mfxRes2;
}

mfxStatus MFXVPP_Plugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}