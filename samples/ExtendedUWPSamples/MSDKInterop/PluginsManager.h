/******************************************************************************\
Copyright (c) 2005-2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once
#include "sample_defs.h"
#include "mfxsession.h"
#include "mfxplugin.h"
#include <vector>

static const mfxPluginUID MSDK_PLUGINGUID_NULL = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

#define MAX_PLUGIN_SHORT_NAME 32
#define MAX_PLUGIN_LONG_NAME 128

enum EPluginSpecification
{
    PD_UNKNOWN=0,
    PD_SOFTWARE=1,
    PD_HARDWARE=2,
    PD_GACC=4,
    PD_ENCODE=8,
    PD_DECODE=16,
};

struct CPluginDescription
{
    mfxPluginUID GUID;
    msdk_char ShortName[MAX_PLUGIN_SHORT_NAME];
    msdk_char LongName[MAX_PLUGIN_LONG_NAME];
    mfxU32 CodecID;
    EPluginSpecification Specification;
};

class CPluginsManager
{
public:
    CPluginsManager(MFXVideoSession& session);
    ~CPluginsManager(void);

    mfxStatus LoadVideoPlugin(mfxU32 codecID,EPluginSpecification spec,mfxPluginUID* foundPluginGUID=NULL, mfxU32 version=1);
    mfxStatus LoadPluginByGUID(mfxPluginUID pluginGUID,mfxU32 version=1);
    mfxStatus UnloadAllPlugins();
    mfxStatus UnloadPluginByGUID(mfxPluginUID pluginGUID);
    mfxStatus UnloadVideoPlugin(mfxU32 codecID,EPluginSpecification spec);
    const CPluginDescription* GetLoadedVideoPlugin(mfxU32 codecID,EPluginSpecification spec=PD_UNKNOWN);
    const CPluginDescription* GetLoadedPluginByGUID(mfxPluginUID pluginGUID);

    //static bool AreGUIDsEqual(const mfxPluginUID& guid1, const mfxPluginUID& guid2);
    static mfxStatus String2GUID(const msdk_string& strGuid, mfxPluginUID & mfxGuid);
    static msdk_tstring GUID2String(const mfxPluginUID & guid);
    static CPluginDescription GetKnownPluginDescription(const mfxPluginUID& guid);
    CPluginDescription GetKnownPluginDescription(mfxU32 codecID,EPluginSpecification spec);
    MFXVideoSession& parentSession;

protected:
    std::vector<CPluginDescription> loadedPlugins;
    static CPluginDescription knownPlugins[];
};

bool operator==(const mfxPluginUID& guid1, const mfxPluginUID& guid2);
bool operator!=(const mfxPluginUID& guid1, const mfxPluginUID& guid2);
