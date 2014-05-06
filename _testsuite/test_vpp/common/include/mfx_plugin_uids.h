/* *******************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#ifndef __MFX_PLUGIN_UIDS__
#define __MFX_PLUGIN_UIDS__

#include <stdio.h>

#include "sample_defs.h"

#include <mfxdefs.h>
#include <mfxplugin.h>
#include <mfxstructures.h>
#include "mfxplugin.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

union msdkPluginUID {
    struct {
        mfxU32 data1;
        mfxU16 data2;
        mfxU16 data3;
        mfxU8  data4[8];
    } guid;
    mfxU8 raw[16];
    mfxPluginUID mfx;
};

enum {
    MSDK_VDEC = 0x0001,
    MSDK_VENC = 0x0002,
    MSDK_VPP = 0x0004,

    MSDK_IMPL_SW = 0x0100,
    MSDK_IMPL_HW = 0x0200,
    MSDK_IMPL_USR = 0x0100
};


struct msdkPluginDesc {
    mfxU32 type;
    mfxU32 codecid;
    struct {
        msdkPluginUID uid;
        msdk_char path[MSDK_MAX_FILENAME_LEN];
    };
    const msdk_char* name;
};

#define DEFINE_MFX_PLUGIN_ID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const msdkPluginUID name = { { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } } };

DEFINE_MFX_PLUGIN_ID(MSDK_NULL_GUID, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0);

// mfx raw: 15dd936825ad475ea34e35f3f54217a6
DEFINE_MFX_PLUGIN_ID(g_msdk_hevcd_uid, 0x6893dd15, 0xad25, 0x5e47, 0xa3, 0x4e, 0x35, 0xf3, 0xf5, 0x42, 0x17, 0xa6);

// mfx raw: 2fca99749fdb49aeb121a5b63ef568f7
DEFINE_MFX_PLUGIN_ID(g_msdk_hevce_uid, 0x7499ca2f, 0xdb9f, 0xae49, 0xb1, 0x21, 0xa5, 0xb6, 0x3e, 0xf5, 0x68, 0xf7);

//static const mfxPluginUID  MFX_PLUGINID_H264LA_HW =    {{0x58, 0x8f, 0x11, 0x85, 0xd4, 0x7b, 0x42, 0x96, 0x8d, 0xea, 0x37, 0x7b, 0xb5, 0xd0, 0xdc, 0xb4}};

DEFINE_MFX_PLUGIN_ID(g_msdk_h264la_uid, 0x85118f58, 0x7bd4, 0x9642, 0x8d, 0xea, 0x37, 0x7b, 0xb5, 0xd0, 0xdc, 0xb4);

const msdkPluginUID* msdkGetPluginUID(mfxU32 type, mfxU32 codecid);
const msdk_char* msdkGetPluginName(mfxU32 type, mfxU32 codecid);

mfxStatus msdkSetPluginPath(mfxU32 type, mfxU32 codecid, msdk_char path[MSDK_MAX_FILENAME_LEN]);
const msdk_char* msdkGetPluginPath(mfxU32 type, mfxU32 codecid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __MFX_PLUGIN_UIDS__
