/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2016 Intel Corporation. All Rights Reserved.

File Name: mfx_hevc_dec_plugin.cpp

\* ****************************************************************************** */

#include "mfx_hevc_dec_plugin.h"
#include "mfx_session.h"
#include "vm_sys_info.h"

//defining module template for decoder plugin
#include "mfx_plugin_module.h"

#include "plugin_version_linux.h"

#include <mfx_trace.h>

#ifndef UNIFIED_PLUGIN

MSDK_PLUGIN_API(MFXDecoderPlugin*) mfxCreateDecoderPlugin() {
    return MFXHEVCDecoderPlugin::Create();
}

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
    return MFXHEVCDecoderPlugin::CreateByDispatcher(uid, plugin);
}

#endif

#ifndef MFX_VA
const mfxPluginUID MFXHEVCDecoderPlugin::g_HEVCDecoderGuid = {0x15,0xdd,0x93,0x68,0x25,0xad,0x47,0x5e,0xa3,0x4e,0x35,0xf3,0xf5,0x42,0x17,0xa6};
#else
const mfxPluginUID MFXHEVCDecoderPlugin::g_HEVCDecoderGuid = {0x33, 0xa6, 0x1c, 0x0b, 0x4c, 0x27, 0x45, 0x4c, 0xa8, 0xd8, 0x5d, 0xde, 0x75, 0x7c, 0x6f, 0x8e};
#endif

MFXHEVCDecoderPlugin::MFXHEVCDecoderPlugin(bool CreateByDispatcher)
{
    m_session = 0;
    m_pmfxCore = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.CodecId = MFX_CODEC_HEVC;
    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_HEVCDecoderGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_DECODE;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;
}

MFXHEVCDecoderPlugin::~MFXHEVCDecoderPlugin()
{
    if (m_session)
    {
        PluginClose();
    }
}

mfxStatus MFXHEVCDecoderPlugin::PluginInit(mfxCoreInterface *core)
{
    if (!core)
        return MFX_ERR_NULL_PTR;
    mfxCoreParam par;
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_TRACE_INIT(NULL, MFX_TRACE_OUTPUT_TRASH);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXHEVCDecoderPlugin::PluginInit");

    m_pmfxCore = core;
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &par);
    MFX_CHECK_STS(mfxRes);

 #if !defined (MFX_VA) && defined (AS_HEVCD_PLUGIN)
    par.Impl = MFX_IMPL_SOFTWARE;
#endif

    mfxRes = MFXInit(par.Impl, &par.Version, &m_session);
    MFX_CHECK_STS(mfxRes);

    mfxRes = MFXInternalPseudoJoinSession((mfxSession) m_pmfxCore->pthis, m_session);
    MFX_CHECK_STS(mfxRes);

    return mfxRes;
}

mfxStatus MFXHEVCDecoderPlugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxStatus mfxRes2 = MFX_ERR_NONE;
    if (m_session)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXHEVCDecoderPlugin::PluginClose");
        //The application must ensure there is no active task running in the session before calling this (MFXDisjoinSession) function.
        mfxRes = MFXVideoDECODE_Close(m_session);
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
    MFX_TRACE_CLOSE();
    if (m_createdByDispatcher) {
        delete this;
    }

    return mfxRes2;
}

mfxStatus MFXHEVCDecoderPlugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}
