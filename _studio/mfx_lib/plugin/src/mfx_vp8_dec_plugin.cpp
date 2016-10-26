//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_vp8_dec_plugin.h"
#include "mfx_session.h"
#include "vm_sys_info.h"

//defining module template for decoder plugin
#include "mfx_plugin_module.h"

#include "plugin_version_linux.h"
#include <mfx_trace.h>

#ifndef UNIFIED_PLUGIN 

MSDK_PLUGIN_API(MFXDecoderPlugin*) mfxCreateDecoderPlugin() {
    return MFXVP8DecoderPlugin::Create();
}

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
    return MFXVP8DecoderPlugin::CreateByDispatcher(uid, plugin);
}

#endif

#ifndef MFX_VA
const mfxPluginUID MFXVP8DecoderPlugin::g_VP8DecoderGuid = { 0xbc, 0xa2, 0x76, 0xec, 0x7a, 0x85, 0x4a, 0x59, 0x8c, 0x30, 0xe0, 0xdc, 0x57, 0x48, 0xf1, 0xf6 };
#else
const mfxPluginUID MFXVP8DecoderPlugin::g_VP8DecoderGuid = { 0xf6, 0x22, 0x39, 0x4d, 0x8d, 0x87, 0x45, 0x2f, 0x87, 0x8c, 0x51, 0xf2, 0xfc, 0x9b, 0x41, 0x31 };
#endif

MFXVP8DecoderPlugin::MFXVP8DecoderPlugin(bool CreateByDispatcher)
{
    m_session = 0;
    m_pmfxCore = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.CodecId = MFX_CODEC_VP8;
    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_VP8DecoderGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_DECODE;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;
}

MFXVP8DecoderPlugin::~MFXVP8DecoderPlugin()
{
    if (m_session)
    {
        PluginClose();
    }
}

mfxStatus MFXVP8DecoderPlugin::PluginInit(mfxCoreInterface *core)
{
    if (!core)
        return MFX_ERR_NULL_PTR;
    mfxCoreParam par;
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_TRACE_INIT(NULL, MFX_TRACE_OUTPUT_TRASH);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVP8DecoderPlugin::PluginInit");

    m_pmfxCore = core;
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &par);
    MFX_CHECK_STS(mfxRes);

    mfxRes = MFXInit(par.Impl, &par.Version, &m_session);
    MFX_CHECK_STS(mfxRes);

    mfxRes = MFXInternalPseudoJoinSession((mfxSession) m_pmfxCore->pthis, m_session);
    MFX_CHECK_STS(mfxRes);

    return mfxRes;
}

mfxStatus MFXVP8DecoderPlugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxStatus mfxRes2 = MFX_ERR_NONE;
    if (m_session)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVP8DecoderPlugin::PluginClose");
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

mfxStatus MFXVP8DecoderPlugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}
