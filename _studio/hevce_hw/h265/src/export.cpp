//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_h265_encode_hw.h"
#include "plugin_version_linux.h"

#if defined(_WIN32)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#elif defined(LINUX32)
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

MSDK_PLUGIN_API(MFXEncoderPlugin*) mfxCreateEncoderPlugin()
{
    return MfxHwH265Encode::Plugin::Create();
}

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin)
{
    return MfxHwH265Encode::Plugin::CreateByDispatcher(uid, plugin);
}
