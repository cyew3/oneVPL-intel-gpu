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
#if defined (AS_VPP_SCD_PLUGIN)
#include "vpp_scd.h"
#endif //defined (AS_VPP_SCD_PLUGIN)
#if defined (AS_ENC_SCD_PLUGIN)
#include "enc_scd.h"
#endif //defined (AS_ENC_SCD_PLUGIN)
#include "plugin_version_linux.h"

#if defined(_WIN32)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#elif defined(LINUX32)
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

#if defined (AS_VPP_SCD_PLUGIN)
MSDK_PLUGIN_API(MFXVPPPlugin*) mfxCreateVPPPlugin()
{
    return MfxVppSCD::Plugin::Create();
}
#endif //defined (AS_VPP_SCD_PLUGIN)

#if defined (AS_ENC_SCD_PLUGIN)
MSDK_PLUGIN_API(MFXEncPlugin*) mfxCreateEncPlugin()
{
    return MfxEncSCD::Plugin::Create();
}
#endif //defined (AS_ENC_SCD_PLUGIN)

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin)
{
    mfxStatus sts = MFX_ERR_NOT_FOUND;

#if defined (AS_VPP_SCD_PLUGIN)
        sts = MfxVppSCD::Plugin::CreateByDispatcher(uid, plugin);
        if (sts != MFX_ERR_NOT_FOUND)
            return sts;
#endif //defined (AS_VPP_SCD_PLUGIN)

#if defined (AS_ENC_SCD_PLUGIN)
        sts = MfxEncSCD::Plugin::CreateByDispatcher(uid, plugin);
        if (sts != MFX_ERR_NOT_FOUND)
            return sts;
#endif //defined (AS_ENC_SCD_PLUGIN)

    return sts;
}