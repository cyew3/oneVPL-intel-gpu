/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#include "mfx_plugin_in_dll_mock.h"

namespace {
    MockCreatePluginPtr g_currentCallback = 0;
}

extern "C" void __declspec(dllexport) MFX_CDECL SET_CREATE_PLUGIN()(MockCreatePluginPtr ptr) {
    g_currentCallback = ptr;
}

extern "C" mfxStatus __declspec(dllexport) MFX_CDECL CREATE_PLUGIN()(mfxPluginUID uid, mfxPlugin* plugin) {
    if (g_currentCallback) {
        return g_currentCallback(uid, plugin);
    }
    return MFX_ERR_NULL_PTR;
}
