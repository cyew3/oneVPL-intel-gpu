//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfxplugin++.h"

struct PluginModuleTemplate {
    typedef MFXDecoderPlugin* (*fncCreateDecoderPlugin)();
    typedef MFXEncoderPlugin* (*fncCreateEncoderPlugin)();
    typedef MFXGenericPlugin* (*fncCreateGenericPlugin)();
    typedef MFXVPPPlugin*     (*fncCreateVPPPlugin)();
    typedef mfxStatus (MFX_CDECL *CreatePluginPtr_t)(mfxPluginUID uid, mfxPlugin* plugin);
    typedef MFXEncPlugin*     (*fncCreateEncPlugin)();

    fncCreateDecoderPlugin CreateDecoderPlugin;
    fncCreateEncoderPlugin CreateEncoderPlugin;
    
    fncCreateGenericPlugin CreateGenericPlugin;
    fncCreateVPPPlugin     CreateVPPPlugin;
    CreatePluginPtr_t      CreatePlugin;   
    fncCreateEncPlugin       CreateEncPlugin;

};

extern PluginModuleTemplate g_PluginModule;
