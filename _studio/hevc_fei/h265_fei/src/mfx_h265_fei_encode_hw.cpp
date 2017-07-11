//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#include "mfx_config.h"
#if defined (MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)
#include "mfx_h265_fei_encode_hw.h"

using namespace MfxHwH265Encode;

namespace MfxHwH265FeiEncode
{

H265FeiEncodePlugin::H265FeiEncodePlugin(bool CreateByDispatcher)
    : Plugin(CreateByDispatcher)
{
}

H265FeiEncodePlugin::~H265FeiEncodePlugin()
{
    Close();
}

MfxHwH265Encode::DriverEncoder* H265FeiEncodePlugin::CreateHWh265Encoder(MFXCoreInterface* core, ENCODER_TYPE type)
{
    core;
    type;

    return new VAAPIh265FeiEncoder;
}

mfxStatus H265FeiEncodePlugin::GetPluginParam(mfxPluginParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    par->PluginUID        = MFX_PLUGINID_HEVC_FEI_ENCODE;
    par->PluginVersion    = 1;
    par->ThreadPolicy     = MFX_THREADPOLICY_SERIAL;
    par->MaxThreadNum     = 1;
    par->APIVersion.Major = MFX_VERSION_MAJOR;
    par->APIVersion.Minor = MFX_VERSION_MINOR;
    par->Type             = MFX_PLUGINTYPE_VIDEO_ENCODE;
    par->CodecId          = MFX_CODEC_HEVC;

    return MFX_ERR_NONE;
}


mfxStatus H265FeiEncodePlugin::Close()
{
    return MFX_ERR_NONE;
}

};
#endif
