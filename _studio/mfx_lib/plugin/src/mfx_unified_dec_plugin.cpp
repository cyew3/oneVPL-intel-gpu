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
#include "mfx_vp8_dec_plugin.h"
#include "mfx_vp9_dec_plugin.h"
#include "mfx_h265_encode_hw.h"
#if defined (PRE_SI_TARGET_PLATFORM_GEN10)
#include "mfx_vp9_encode_hw.h"
#endif // PRE_SI_TARGET_PLATFORM_GEN10
#include "mfx_camera_plugin.h"
#if defined (WIN64)
#include "mfx_hevc_enc_plugin.h"
#endif //WIN64

MSDK_PLUGIN_API(MFXDecoderPlugin*) mfxCreateDecoderPlugin() {
    return 0;
}

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {

    if(std::memcmp(uid.Data, MFXHEVCDecoderPlugin::g_HEVCDecoderGuid.Data, sizeof(uid.Data)) == 0)
        return MFXHEVCDecoderPlugin::CreateByDispatcher(uid, plugin);
    else if(std::memcmp(uid.Data, MFXVP8DecoderPlugin::g_VP8DecoderGuid.Data, sizeof(uid.Data)) == 0)
        return MFXVP8DecoderPlugin::CreateByDispatcher(uid, plugin);
    else if(std::memcmp(uid.Data, MFXVP9DecoderPlugin::g_VP9DecoderGuid.Data, sizeof(uid.Data)) == 0)
        return MFXVP9DecoderPlugin::CreateByDispatcher(uid, plugin);
    else if(std::memcmp(uid.Data, MfxHwH265Encode::MFX_PLUGINID_HEVCE_HW.Data, sizeof(uid.Data)) == 0)
        return MfxHwH265Encode::Plugin::CreateByDispatcher(uid, plugin);
#if defined (PRE_SI_TARGET_PLATFORM_GEN10)
    else if(std::memcmp(uid.Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(uid.Data)) == 0)
        return MfxHwVP9Encode::Plugin::CreateByDispatcher(uid, plugin);
#endif // PRE_SI_TARGET_PLATFORM_GEN10
    else if(std::memcmp(uid.Data, MFXCamera_Plugin::g_Camera_PluginGuid.Data, sizeof(uid.Data)) == 0)
        return MFXCamera_Plugin::CreateByDispatcher(uid, plugin);
#if defined (WIN64)
    else if (std::memcmp(uid.Data, MFXHEVCEncoderPlugin::g_HEVCEncoderGuid.Data, sizeof(uid.Data)) == 0)
        return MFXHEVCEncoderPlugin::CreateByDispatcher(uid, plugin);
#endif // WIN34
    else return MFX_ERR_NOT_FOUND;
}
