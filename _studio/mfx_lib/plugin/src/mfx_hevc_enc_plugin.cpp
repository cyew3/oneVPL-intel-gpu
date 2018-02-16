//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2018 Intel Corporation. All Rights Reserved.
//

#if defined(WIN64)

#include "mfx_hevc_enc_plugin.h"
#include "mfx_session.h"
#include "vm_sys_info.h"

//defining module template for decoder plugin
#include "mfx_plugin_module.h"

#include "plugin_version_linux.h"

#ifndef UNIFIED_PLUGIN
PluginModuleTemplate g_PluginModule = {
    NULL,
    &MFXHEVCEncoderPlugin::Create,
    NULL,
    NULL,
    &MFXHEVCEncoderPlugin::CreateByDispatcher
};

MSDK_PLUGIN_API(MFXEncoderPlugin*) mfxCreateEncoderPlugin() {
    if (!g_PluginModule.CreateEncoderPlugin) {
        return 0;
    }
    return g_PluginModule.CreateEncoderPlugin();
}

MSDK_PLUGIN_API(MFXPlugin*) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
    if (!g_PluginModule.CreatePlugin) {
        return 0;
    }
    return (MFXPlugin*) g_PluginModule.CreatePlugin(uid, plugin);
}
#endif

#ifdef MFX_VA
const mfxPluginUID MFXHEVCEncoderPlugin::g_HEVCEncoderGuid     = {0xe5,0x40,0x0a,0x06,0xc7,0x4d,0x41,0xf5,0xb1,0x2d,0x43,0x0b,0xba,0xa2,0x3d,0x0b};
#if defined( AS_HEVCE_DP_PLUGIN)
//Plugin inside Default Plugin only
const mfxPluginUID MFXHEVCEncoderDPPlugin::g_HEVCEncoderDPGuid = {0x2b,0xad,0x6f,0x9d,0x77,0x54,0x41,0x2d,0xbf,0x63,0x03,0xed,0x4b,0xb5,0x09,0x68};
#endif //#if defined( AS_HEVCE_DP_PLUGIN)
#else //MFX_VA
const mfxPluginUID MFXHEVCEncoderPlugin::g_HEVCEncoderGuid = {0x2f,0xca,0x99,0x74,0x9f,0xdb,0x49,0xae,0xb1,0x21,0xa5,0xb6,0x3e,0xf5,0x68,0xf7};
#endif //MFX_VA

MFXHEVCEncoderPlugin::MFXHEVCEncoderPlugin(bool CreateByDispatcher)
{
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.CodecId = MFX_CODEC_HEVC;
    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_PARALLEL;
    m_PluginParam.MaxThreadNum = vm_sys_info_get_cpu_num();
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_HEVCEncoderGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;

    m_encoder = NULL;
    MFX_TRACE_INIT();
}

MFXHEVCEncoderPlugin::~MFXHEVCEncoderPlugin()
{
    MFX_TRACE_CLOSE();
    if (m_encoder)
    {
        PluginClose();
    }
}

mfxStatus MFXHEVCEncoderPlugin::PluginInit(mfxCoreInterface *core)
{
    if (!core)
        return MFX_ERR_NULL_PTR;

    m_mfxCore = *core;

    mfxStatus sts;
    m_encoder = new MFXVideoENCODEH265(&m_mfxCore, &sts);

    return MFX_ERR_NONE;
}

mfxStatus MFXHEVCEncoderPlugin::PluginClose()
{
    if (m_encoder) {
        delete m_encoder;
        m_encoder = NULL;
    }
    if (m_createdByDispatcher) {
        delete this;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXHEVCEncoderPlugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}
#endif // WIN64