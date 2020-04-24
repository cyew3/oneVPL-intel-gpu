// Copyright (c) 2013-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#if defined(WIN64) || defined(LINUX64)

#include "mfx_hevc_enc_plugin.h"
#include "mfx_session.h"
#include "vm_sys_info.h"

//defining module template for decoder plugin
#include "mfx_plugin_module.h"

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
    mfxPlatform platform;

    sts = m_mfxCore.m_core.QueryPlatform(m_mfxCore.m_core.pthis, &platform);
    if (MFX_ERR_NONE != sts || MFX_PLATFORM_UNKNOWN == platform.CodeName)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if (platform.CodeName >= MFX_PLATFORM_TIGERLAKE)
    {
        return MFX_ERR_UNSUPPORTED; //GACC is deprecated for TGL and all upcoming platforms
    }

    m_encoder = new MFXVideoENCODEH265(&m_mfxCore, &sts);

    return MFX_ERR_NONE;
}

mfxStatus MFXHEVCEncoderPlugin::PluginClose()
{
    delete m_encoder;
    m_encoder = nullptr;

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
#endif // WIN64 || LINUX64