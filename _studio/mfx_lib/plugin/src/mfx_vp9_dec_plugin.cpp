//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2018 Intel Corporation. All Rights Reserved.
//

#include "mfx_vp9_dec_plugin.h"

//defining module template for decoder plugin
#include "mfx_plugin_module.h"

#include "plugin_version_linux.h"

#include "mfx_utils.h"

#ifndef UNIFIED_PLUGIN 

MSDK_PLUGIN_API(MFXDecoderPlugin*) mfxCreateDecoderPlugin() {
    return MFXVP9DecoderPlugin::Create();
}

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
    return MFXVP9DecoderPlugin::CreateByDispatcher(uid, plugin);
}

#endif

#ifndef MFX_VA
const mfxPluginUID MFXVP9DecoderPlugin::g_VP9DecoderGuid = { 0xa9, 0xa2, 0x76, 0xec, 0x7a, 0x85, 0x4a, 0x59, 0x8c, 0x30, 0xe0, 0xdc, 0x57, 0x48, 0xf1, 0xf6 };
// a9a276ec7a854a598c30e0dc5748f1f6
#else
const mfxPluginUID MFXVP9DecoderPlugin::g_VP9DecoderGuid = { 0xa9, 0x22, 0x39, 0x4d, 0x8d, 0x87, 0x45, 0x2f, 0x87, 0x8c, 0x51, 0xf2, 0xfc, 0x9b, 0x41, 0x31 };
// a922394d8d87452f878c51f2fc9b4131
#endif

#ifdef MFX_VA
static
MFXVP9DecoderPlugin* GetVP9DecoderInstance()
{
    static MFXVP9DecoderPlugin instance{};
    return &instance;
}

MFXDecoderPlugin* MFXVP9DecoderPlugin::Create()
{ return GetVP9DecoderInstance(); }
#else
MFXVP9DecoderPlugin::MFXVP9DecoderPlugin(bool CreateByDispatcher)
    : MFXStubDecoderPlugin(CreateByDispatcher)
{}

MFXDecoderPlugin* MFXVP9DecoderPlugin::Create()
{
    return new MFXVP9DecoderPlugin(false);
}
#endif

mfxStatus  MFXVP9DecoderPlugin::_GetPluginParam(mfxHDL /*pthis*/, mfxPluginParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    memset(par, 0, sizeof(mfxPluginParam));

    par->CodecId = MFX_CODEC_VP9;
    par->ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    par->MaxThreadNum = 1;
    par->APIVersion.Major = MFX_VERSION_MAJOR;
    par->APIVersion.Minor = MFX_VERSION_MINOR;
    par->PluginUID = g_VP9DecoderGuid;
    par->Type = MFX_PLUGINTYPE_VIDEO_DECODE;
    par->PluginVersion = 1;


    return MFX_ERR_NONE;
}

mfxStatus MFXVP9DecoderPlugin::GetPluginParam(mfxPluginParam* par)
{
    return _GetPluginParam(this, par);
}

#ifndef MFX_VA
mfxStatus MFXVP9DecoderPlugin::CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
{
    MFX_CHECK_NULL_PTR1(mfxPlg);

    if (memcmp(&guid, &g_VP9DecoderGuid, sizeof(mfxPluginUID))) {
        return MFX_ERR_NOT_FOUND;
    }

    MFXVP9DecoderPlugin* plugin = nullptr;
    mfxStatus sts = MFXStubDecoderPlugin::CreateInstance(&plugin);
    MFX_CHECK_STS(sts);

    plugin->m_createdByDispatcher = true;
    *mfxPlg = (mfxPlugin)*plugin->m_adapter;

    return MFX_ERR_NONE;
}
#else
mfxStatus MFXVP9DecoderPlugin::CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
{
    if (memcmp(& guid , &g_VP9DecoderGuid, sizeof(mfxPluginUID))) {
        return MFX_ERR_NOT_FOUND;
    }

    MFXStubDecoderPlugin::CreateByDispatcher(guid, mfxPlg);
    mfxPlg->pthis          = reinterpret_cast<mfxHDL>(MFXVP9DecoderPlugin::Create());
    mfxPlg->GetPluginParam = _GetPluginParam;

    return MFX_ERR_NONE;
}
#endif