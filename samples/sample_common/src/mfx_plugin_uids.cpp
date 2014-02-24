/* *******************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include <stdio.h>
#include <string.h>

#include "sample_defs.h"
#include "vm/strings_defs.h"
#include "mfx_plugin_uids.h"

static msdkPluginDesc g_msdk_supported_plugins[] = {
    //Supported Decoder Plugins
    { MSDK_VDEC | MSDK_IMPL_SW, MFX_CODEC_HEVC,  { g_msdk_hevcd_uid, {0} }, MSDK_STRING("Intel (R) Media SDK plugin for HEVC DECODE") },
    //{ MSDK_VDEC | MSDK_IMPL_HW, MFX_MFX_CODEC_HEVC, { g_msdk_hevcd_hw_uid, {0} }, "Intel (R) Media SDK HW plugin for HEVC DECODE" },
    { MSDK_VDEC | MSDK_IMPL_USR, MFX_CODEC_HEVC, { MSDK_NULL_GUID, {0} },   MSDK_STRING("User defined HEVC Plug-in") },
    { MSDK_VDEC | MSDK_IMPL_USR, MFX_CODEC_AVC,  { MSDK_NULL_GUID, {0} },   MSDK_STRING("User defined AVC Plug-in") },
    { MSDK_VDEC | MSDK_IMPL_USR, MFX_CODEC_MPEG2,{ MSDK_NULL_GUID, {0} },   MSDK_STRING("User defined MPEG2 Plug-in") },
    { MSDK_VDEC | MSDK_IMPL_USR, MFX_CODEC_VC1,  { MSDK_NULL_GUID, {0} },   MSDK_STRING("User defined VC1 Plug-in") },
    //Supported Encoder Plugins
    { MSDK_VENC | MSDK_IMPL_SW, MFX_CODEC_HEVC,  { g_msdk_hevce_uid, {0} }, MSDK_STRING("Intel (R) Media SDK plugin for HEVC ENCODE") },
    { MSDK_VENC | MSDK_IMPL_USR, MFX_CODEC_HEVC, { MSDK_NULL_GUID, {0} },   MSDK_STRING("User defined HEVC Plug-in") },
    { MSDK_VENC | MSDK_IMPL_USR, MFX_CODEC_AVC,  { MSDK_NULL_GUID, {0} },   MSDK_STRING("User defined AVC Plug-in") },
    { MSDK_VENC | MSDK_IMPL_USR, MFX_CODEC_MPEG2,{ MSDK_NULL_GUID, {0} },   MSDK_STRING("User defined MPEG2 Plug-in") },
    { MSDK_VENC | MSDK_IMPL_USR, MFX_CODEC_VC1,  { MSDK_NULL_GUID, {0} },   MSDK_STRING("User defined VC1 Plug-in") },
    //{ MSDK_VDEC | MSDK_IMPL_USR, MFX_CODEC_VP8,  { MSDK_NULL_GUID, {0} }, "User defined VP8 Plug-in" },
};

const msdkPluginUID* msdkGetPluginUID(mfxU32 type, mfxU32 codecid)
{
    mfxU32 i;

    for (i = 0; i < sizeof(g_msdk_supported_plugins)/sizeof(g_msdk_supported_plugins[0]); ++i) {
        if ((type == g_msdk_supported_plugins[i].type) && (codecid == g_msdk_supported_plugins[i].codecid)) {
            return &(g_msdk_supported_plugins[i].uid);
        }
    }
    return NULL;
}

const msdk_char* msdkGetPluginName(mfxU32 type, mfxU32 codecid)
{
    mfxU32 i;

    for (i = 0; i < sizeof(g_msdk_supported_plugins)/sizeof(g_msdk_supported_plugins[0]); ++i) {
        if (g_msdk_supported_plugins[i].type == type && g_msdk_supported_plugins[i].codecid == codecid) {
            return g_msdk_supported_plugins[i].name;
        }
    }
    return NULL;
}

mfxStatus msdkSetPluginPath(mfxU32 type, mfxU32 codecid, msdk_char path[MSDK_MAX_FILENAME_LEN])
{
    mfxU32 i;

    for (i = 0; i < sizeof(g_msdk_supported_plugins)/sizeof(g_msdk_supported_plugins[0]); ++i) {
        if (g_msdk_supported_plugins[i].type == type && g_msdk_supported_plugins[i].codecid == codecid) {
            msdk_strcopy(g_msdk_supported_plugins[i].path, path);
            return MFX_ERR_NONE;
        }
    }                 
    return MFX_ERR_NOT_FOUND;
}

const msdk_char* msdkGetPluginPath(mfxU32 type, mfxU32 codecid)
{
    mfxU32 i;

    for (i = 0; i < sizeof(g_msdk_supported_plugins)/sizeof(g_msdk_supported_plugins[0]); ++i) {
        if (g_msdk_supported_plugins[i].type == type && g_msdk_supported_plugins[i].codecid == codecid) {
            return g_msdk_supported_plugins[i].path;
        }
    }                 
    return NULL;
}
