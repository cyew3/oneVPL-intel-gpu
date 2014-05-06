/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxplugin.h"

#define SET_CREATE_PLUGIN() SetCreatePluginCallback
#define CREATE_PLUGIN() CreatePlugin

#define MAKE_STRING_NAME(arg) #arg

static const char CreatePluginNameA[] = MAKE_STRING_NAME(CreatePlugin);
static const char SetCreatePluginCallbackNameA[] = MAKE_STRING_NAME(SetCreatePluginCallback);

typedef mfxStatus (MFX_CDECL *MockCreatePluginPtr)(mfxPluginUID uid, mfxPlugin* plugin);
typedef void      (MFX_CDECL *SetCreatePluginCallbackPtr)(MockCreatePluginPtr ptr) ;
