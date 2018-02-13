//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2018 Intel Corporation. All Rights Reserved.
//

#include "mfx_hevc_dec_plugin.h"

//defining module template for decoder plugin
#include "mfx_plugin_module.h"

#include "plugin_version_linux.h"

#include "mfx_utils.h"

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

#ifdef MFX_VA
static
MFXHEVCDecoderPlugin* GetHEVCDecoderInstance()
{
    static MFXHEVCDecoderPlugin instance{};
    return &instance;
}

MFXDecoderPlugin* MFXHEVCDecoderPlugin::Create()
{ return GetHEVCDecoderInstance(); }
#else
MFXHEVCDecoderPlugin::MFXHEVCDecoderPlugin(bool CreateByDispatcher)
    : MFXStubDecoderPlugin(CreateByDispatcher)
{}

MFXDecoderPlugin* MFXHEVCDecoderPlugin::Create()
{
    return new MFXHEVCDecoderPlugin(false);
}
#endif

mfxStatus  MFXHEVCDecoderPlugin::_GetPluginParam(mfxHDL /*pthis*/, mfxPluginParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    memset(par, 0, sizeof(mfxPluginParam));

    par->CodecId = MFX_CODEC_HEVC;
    par->ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    par->MaxThreadNum = 1;
    par->APIVersion.Major = MFX_VERSION_MAJOR;
    par->APIVersion.Minor = MFX_VERSION_MINOR;
    par->PluginUID = g_HEVCDecoderGuid;
    par->Type = MFX_PLUGINTYPE_VIDEO_DECODE;
    par->PluginVersion = 1;

    return MFX_ERR_NONE;
}

mfxStatus MFXHEVCDecoderPlugin::GetPluginParam(mfxPluginParam* par)
{
    return _GetPluginParam(this, par);
}

#ifndef MFX_VA
mfxStatus MFXHEVCDecoderPlugin::CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
{
    MFX_CHECK_NULL_PTR1(mfxPlg);

    if (memcmp(&guid, &g_HEVCDecoderGuid, sizeof(mfxPluginUID))) {
        return MFX_ERR_NOT_FOUND;
    }

    MFXHEVCDecoderPlugin* plugin = nullptr;
    mfxStatus sts = MFXStubDecoderPlugin::CreateInstance(&plugin);
    MFX_CHECK_STS(sts);

    plugin->m_createdByDispatcher = true;
    *mfxPlg = (mfxPlugin)*plugin->m_adapter;

    return MFX_ERR_NONE;
}
#else
mfxStatus MFXHEVCDecoderPlugin::CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
{
    if (memcmp(& guid , &g_HEVCDecoderGuid, sizeof(mfxPluginUID))) {
        return MFX_ERR_NOT_FOUND;
    }

    MFXStubDecoderPlugin::CreateByDispatcher(guid, mfxPlg);
    mfxPlg->pthis          = reinterpret_cast<mfxHDL>(MFXHEVCDecoderPlugin::Create());
    mfxPlg->GetPluginParam = _GetPluginParam;

    return MFX_ERR_NONE;
}
#endif