/******************************************************************************\
Copyright (c) 2005-2017, Intel Corporation
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

#include "pch.h"
#include "PluginsManager.h"

CPluginDescription CPluginsManager::knownPlugins[]=
{
    {MFX_PLUGINID_HEVCD_SW,MSDK_STRING("HEVCD_SW"),MSDK_STRING("Intel (R) Media SDK SW plugin for HEVC DECODE"),MFX_CODEC_HEVC,EPluginSpecification(PD_SOFTWARE|PD_DECODE)},
    {MFX_PLUGINID_HEVCD_HW,MSDK_STRING("HEVCD_HW"),MSDK_STRING("Intel (R) Media SDK HW plugin for HEVC DECODE"),MFX_CODEC_HEVC,EPluginSpecification(PD_HARDWARE|PD_DECODE)},
    {MFX_PLUGINID_HEVCE_SW,MSDK_STRING("HEVCE_SW"),MSDK_STRING("Intel (R) Media SDK SW plugin for HEVC ENCODE"),MFX_CODEC_HEVC,EPluginSpecification(PD_SOFTWARE|PD_ENCODE)},
    {MFX_PLUGINID_HEVCE_GACC,MSDK_STRING("HEVCE_GACC"),MSDK_STRING("Intel (R) Media SDK GACC plugin for HEVC ENCODE"),MFX_CODEC_HEVC,EPluginSpecification(PD_GACC|PD_ENCODE)},
    {MFX_PLUGINID_HEVCE_HW,MSDK_STRING("HEVCE_HW"),MSDK_STRING("Intel (R) Media SDK HW plugin for HEVC ENCODE"),MFX_CODEC_HEVC,EPluginSpecification(PD_HARDWARE|PD_ENCODE)},
    //{MFX_PLUGINID_VP8D_HW,MSDK_STRING("VP8D_HW"),MSDK_STRING("Intel (R) Media SDK HW plugin for VP8 DECODE"),MFX_CODEC_VP8,PD_HARDWARE|PD_DECODE},
    //{MFX_PLUGINID_VP8E_HW,MSDK_STRING("VP8E_HW"),MSDK_STRING("Intel (R) Media SDK HW plugin for VP8 ENCODE"),MFX_CODEC_VP8,PD_HARDWARE|PD_ENCODE},
    //{MFX_PLUGINID_VP9E_HW,MSDK_STRING("VP9D_HW"),MSDK_STRING("Intel (R) Media SDK HW plugin for VP9 DECODE"),MFX_CODEC_VP9,PD_HARDWARE|PD_DECODE},
    //{MFX_PLUGINID_VP9D_HW,MSDK_STRING("VP9E_HW"),MSDK_STRING("Intel (R) Media SDK HW plugin for VP9 ENCODE"),MFX_CODEC_VP9,PD_HARDWARE|PD_ENCODE},
    //{MFX_PLUGINID_CAMERA_HW,MSDK_STRING("camera"),MSDK_STRING("Intel (R) Media SDK HW plugin for Camera"),NULL,PD_HARDWARE|PD_DECODE},
    //{MFX_PLUGINID_CAPTURE_HW,MSDK_STRING("capture"),MSDK_STRING("Intel (R) Media SDK HW plugin for Screen Capture"),NULL,PD_HARDWARE|PD_DECODE},
    {MFX_PLUGINID_ITELECINE_HW,MSDK_STRING("PTIR_HW"),MSDK_STRING("Intel (R) Media SDK PTIR plugin (HW)"),NULL,EPluginSpecification(PD_HARDWARE)},
    {MFX_PLUGINID_H264LA_HW,MSDK_STRING("H264LA_HW"),MSDK_STRING("Intel (R) Media SDK plugin for LA ENC"),NULL,EPluginSpecification(PD_HARDWARE)}
};

CPluginsManager::CPluginsManager(MFXVideoSession& session) :
    parentSession(session)
{
}


CPluginsManager::~CPluginsManager(void)
{
}

mfxStatus CPluginsManager::LoadVideoPlugin(mfxU32 codecID,EPluginSpecification spec,mfxPluginUID* foundPluginGUID,mfxU32 version)
{
    mfxPluginUID pluginGUID = GetKnownPluginDescription(codecID, spec).GUID;
    if (pluginGUID!=MSDK_PLUGINGUID_NULL)
    {
        mfxStatus sts = LoadPluginByGUID(pluginGUID,version);
        if(sts>=MFX_ERR_NONE && foundPluginGUID)
        {
            *foundPluginGUID=pluginGUID;
        }
        return sts;
    }

    //LOG_ERROR("PipelineBase: Cannot load find plugin for video type (0x%X) and specification 0x%X",codecID,spec);
    // This means that system doesn't know about plugin for this type of videocodec, possibly it is not required
    return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
}

mfxStatus CPluginsManager::LoadPluginByGUID(mfxPluginUID pluginGUID,mfxU32 version)
{
    MSDK_CHECK_POINTER((mfxSession)parentSession,MFX_ERR_NULL_PTR);

    mfxStatus sts = MFXVideoUSER_Load((mfxSession)parentSession, &pluginGUID, version);
    CPluginDescription desc = GetKnownPluginDescription(pluginGUID);
    if(sts==MFX_ERR_NONE)
    {
        CPluginDescription desc = GetKnownPluginDescription(pluginGUID);
        loadedPlugins.push_back(desc);
        return MFX_ERR_NONE;
    }

    msdk_printf(MSDK_STRING("PipelineBase: Cannot load plugin: %s (%s)"),desc.LongName,GUID2String(pluginGUID).c_str());
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CPluginsManager::UnloadAllPlugins()
{
    for(int i=0;i<loadedPlugins.size();i++)
    {
        mfxStatus sts = MFXVideoUSER_UnLoad((mfxSession)parentSession, &loadedPlugins[i].GUID);
        if(sts!=MFX_ERR_NONE)
        {
            msdk_printf(MSDK_STRING("Can't unload plugin: %s (%s), error: %d"),loadedPlugins[i].LongName,GUID2String(loadedPlugins[i].GUID).c_str(),sts);
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus CPluginsManager::UnloadPluginByGUID(mfxPluginUID pluginGUID)
{
    mfxStatus sts = MFXVideoUSER_UnLoad((mfxSession)parentSession, &pluginGUID);
    if(sts!=MFX_ERR_NONE)
    {
        CPluginDescription desc = GetKnownPluginDescription(pluginGUID);
        msdk_printf(MSDK_STRING("Can't unload plugin: %s (%s), error: %d"),desc.LongName, GUID2String(pluginGUID).c_str(),sts);
    }
    
    // Removing plugin from list even in case of error - possibly something gone wrong with plugin and it is not loaded anymore
    for(std::vector<CPluginDescription>::iterator it=loadedPlugins.begin(); it!=loadedPlugins.end(); it++)
    {
        if(it->GUID==pluginGUID)
        {
            loadedPlugins.erase(it);
            break;
        }
    }

    return sts;
}

mfxStatus CPluginsManager::UnloadVideoPlugin(mfxU32 codecID,EPluginSpecification spec)
{
    const CPluginDescription* pDesc = GetLoadedVideoPlugin(codecID,spec);
    MSDK_CHECK_POINTER(pDesc,MFX_ERR_NOT_FOUND);
    return UnloadPluginByGUID(pDesc->GUID);
}

const CPluginDescription* CPluginsManager::GetLoadedVideoPlugin(mfxU32 codecID,EPluginSpecification spec)
{
    for(int i=0;i<loadedPlugins.size();i++)
    {
        if(loadedPlugins[i].CodecID==codecID && (knownPlugins[i].Specification & spec) == knownPlugins[i].Specification)
        {
            return &loadedPlugins[i];
        }
    }
    loadedPlugins.clear();
    return NULL;
}

const CPluginDescription* CPluginsManager::GetLoadedPluginByGUID(mfxPluginUID pluginGUID)
{
    for(int i=0;i<loadedPlugins.size();i++)
    {
        if(loadedPlugins[i].GUID==pluginGUID)
        {
            return &loadedPlugins[i];
        }
    }
    return NULL;
}

CPluginDescription CPluginsManager::GetKnownPluginDescription(const mfxPluginUID& guid)
{
    for(int i=0;i<_countof(knownPlugins);i++)
    {
        if(guid==knownPlugins[i].GUID)
        {
            return knownPlugins[i];
        }
    }
    CPluginDescription unknownPlugin={guid,MSDK_STRING("UNKNOWN"),MSDK_STRING("Unknown plugin"),NULL,PD_UNKNOWN};
    return unknownPlugin;
}

CPluginDescription CPluginsManager::GetKnownPluginDescription(mfxU32 codecID,EPluginSpecification spec)
{
    for(int i=0;i<_countof(knownPlugins);i++)
    {
        if(codecID==knownPlugins[i].CodecID && (knownPlugins[i].Specification & spec) == knownPlugins[i].Specification)
        {
            return knownPlugins[i];
        }
    }
    CPluginDescription unknownPlugin={MSDK_PLUGINGUID_NULL,MSDK_STRING("UNKNOWN"),MSDK_STRING("Unknown plugin"),NULL,PD_UNKNOWN};
    return unknownPlugin;
}

mfxStatus CPluginsManager::String2GUID(const msdk_string& strGuid, mfxPluginUID & mfxGuid)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 hex = 0;
    for(size_t i = 0; i != sizeof(mfxPluginUID); i++)
    {
        hex = 0;

#if defined(_WIN32) || defined(_WIN64)
        if (1 != _stscanf_s(strGuid.c_str() + 2*i, MSDK_STRING("%2x"), &hex))
#else
        if (1 != sscanf(strGuid.c_str() + 2*i, MSDK_STRING("%2x"), &hex))
#endif
        {
            sts = MFX_ERR_UNKNOWN;
            break;
        }
        if (hex == 0 && (const msdk_char *)strGuid.c_str() + 2*i != msdk_strstr((const msdk_char *)strGuid.c_str() + 2*i,  MSDK_STRING("00")))
        {
            sts = MFX_ERR_UNKNOWN;
            break;
        }
        mfxGuid.Data[i] = (mfxU8)hex;
    }

    if (sts != MFX_ERR_NONE)
        MSDK_ZERO_MEMORY(mfxGuid);

    return sts;
}

msdk_tstring CPluginsManager::GUID2String(const mfxPluginUID & guid)
{
    msdk_char tempStr[64];
    msdk_sprintf(tempStr,MSDK_STRING("%X%X%X%X-%X%X%X%X-%X%X%X%X-%X%X%X%X"),guid.Data[0],guid.Data[1],guid.Data[2],guid.Data[3]
        ,guid.Data[4],guid.Data[5],guid.Data[6],guid.Data[7],guid.Data[8],guid.Data[9],guid.Data[10],guid.Data[11]
        ,guid.Data[12],guid.Data[13],guid.Data[14],guid.Data[15]);

    return msdk_tstring(tempStr);
}

bool operator==(const mfxPluginUID& guid1, const mfxPluginUID& guid2)
{
    return memcmp(&guid1, &guid2, sizeof(mfxPluginUID)) == 0;
}

bool operator!=(const mfxPluginUID& guid1, const mfxPluginUID& guid2)
{
    return memcmp(&guid1, &guid2, sizeof(mfxPluginUID)) != 0;
}