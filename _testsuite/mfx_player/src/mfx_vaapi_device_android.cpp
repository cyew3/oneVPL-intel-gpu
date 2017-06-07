/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(ANDROID)
#ifdef LIBVA_ANDROID_SUPPORT

#include "mfx_pipeline_defs.h"
#include "mfx_vaapi_device_android.h"
#include "vaapi_utils.h"

static AndroidLibVA g_LibVA;

IHWDevice* CreateVAAPIDevice(int type)
{
    (void)type;
    return new MFXVAAPIDeviceAndroid(&g_LibVA);
}

#endif // #ifdef LIBVA_ANDROID_SUPPORT
#endif // #if defined(ANDROID)
