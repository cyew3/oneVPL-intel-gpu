/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mfx_vpp_plugin.h

\* ****************************************************************************** */

#include "mfx_vpp_plugin.h"
#include "mfx_session.h"
#include "vm_sys_info.h"

//defining module template for decoder plugin
#include "mfx_plugin_module.h"
PluginModuleTemplate g_PluginModule = {
    NULL,
    NULL,
    NULL,
    MFXVPP_Plugin::Create
};

MSDK_PLUGIN_API(MFXVPPPlugin*) mfxCreateVPPPlugin() {
    if (!g_PluginModule.CreateVPPPlugin) {
        return 0;
    }
    return g_PluginModule.CreateVPPPlugin();
}

MFXVPP_Plugin::MFXVPP_Plugin()
{
    m_session = 0;
    m_pmfxCore = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 1;
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
        return mfxRes2;
    }
    else
        return MFX_ERR_NONE;
}

mfxStatus MFXVPP_Plugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}