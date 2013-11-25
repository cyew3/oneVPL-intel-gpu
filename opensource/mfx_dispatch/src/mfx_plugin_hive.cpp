/* ****************************************************************************** *\

Copyright (C) 2013 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_plugin_hive.h

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_plugin_hive.h"
#include "mfx_library_iterator.h"
#include "mfx_dispatcher_log.h"

#define TRACE_HIVE_ERROR(str, ...) DISPATCHER_LOG_ERROR((("[HIVE]: "str), __VA_ARGS__))
#define TRACE_HIVE_INFO(str, ...) DISPATCHER_LOG_INFO((("[HIVE]: "str), __VA_ARGS__))
#define TRACE_HIVE_WRN(str, ...) DISPATCHER_LOG_WRN((("[HIVE]: "str), __VA_ARGS__))

namespace 
{
    const wchar_t rootPluginPath[] = L"Software\\Intel\\MediaSDK\\Dispatch\\Plugin";
    const wchar_t TypeKeyName[] = L"Type";
    const wchar_t CodecIDKeyName[] = L"CodecID";
    const wchar_t GUIDKeyName[] = L"GUID";
    const wchar_t PathKeyName[] = L"Path";
    const wchar_t NameKeyName[] = L"Name";
    const wchar_t DefaultKeyName[] = L"Default";
    const wchar_t PlgVerKeyName[] = L"PlgVer";
    const wchar_t APIVerKeyName[] = L"APIVer";
}

MFX::MFXPluginHive::MFXPluginHive( mfxU32 storageID, mfxVersion minAPIVersion )
{
    HKEY rootHKey;
    bool bRes;
    WinRegKey regKey;   

    // open required registry key
    rootHKey = (MFX_LOCAL_MACHINE_KEY == storageID) ? (HKEY_LOCAL_MACHINE) : (HKEY_CURRENT_USER);
    bRes = regKey.Open(rootHKey, rootPluginPath, KEY_READ);
    
    if (false == bRes) {
            return;
    }
    DWORD index = 0;
    if (!regKey.QueryInfo(&index)) {
        return;
    }
    try 
    {
        mRecords.resize(index);
    }
    catch (...) {
        TRACE_HIVE_ERROR("new PluginDescriptionRecord[%d] threw an exception: \n", index);
        return;
    }
    
    for(index = 0; ; index++) 
    {
        wchar_t   subKeyName[MFX_MAX_VALUE_NAME];
        DWORD     subKeyNameSize = sizeof(subKeyName) / sizeof(subKeyName[0]);
        WinRegKey subKey;

        // query next value name
        bool enumRes = regKey.EnumKey(index, subKeyName, &subKeyNameSize);
        if (!enumRes) {
            break;
        }

        // open the sub key
        bRes = subKey.Open(regKey, subKeyName, KEY_READ);
        if (!bRes) {
            continue;
        }

        TRACE_HIVE_INFO("Found Plugin: %s\\%S\\%S\n", (MFX_LOCAL_MACHINE_KEY == storageID) ? ("HKEY_LOCAL_MACHINE") : ("HKEY_CURRENT_USER"),
            rootPluginPath, subKeyName);

        PluginDescriptionRecord descriptionRecord;

        if (!subKey.Query(TypeKeyName, descriptionRecord.Type)) 
        {
            continue;
        }
        TRACE_HIVE_INFO("    %8S : %d\n", TypeKeyName, descriptionRecord.Type);

        if (!subKey.Query(CodecIDKeyName, descriptionRecord.CodecId)) 
        {
            continue;
        }
        TRACE_HIVE_INFO("    %8S : "MFXFOURCCTYPE()" \n", CodecIDKeyName, MFXU32TOFOURCC(descriptionRecord.CodecId));

        if (!subKey.Query(GUIDKeyName, descriptionRecord.PluginUID)) 
        {
            continue;
        }
        TRACE_HIVE_INFO("    %8S : "MFXGUIDTYPE()"\n", GUIDKeyName, MFXGUIDTOHEX(&descriptionRecord.PluginUID));

        mfxU32 nSize = sizeof(descriptionRecord.sPath)/sizeof(*descriptionRecord.sPath);
        if (!subKey.Query(PathKeyName, descriptionRecord.sPath, nSize)) 
        {
            TRACE_HIVE_WRN("no value for : %S\n", PathKeyName);
            continue;
        }
        TRACE_HIVE_INFO("    %8S : %S\n", PathKeyName, descriptionRecord.sPath);

        msdk_disp_char sNameWchar[MAX_PLUGIN_PATH];
        if (!subKey.Query(NameKeyName, sNameWchar,  nSize = sizeof(descriptionRecord.sName)/sizeof(*descriptionRecord.sName))) 
        {
            continue;
        }
        for (mfxU32 i =0;i<nSize;i++) 
        {
            descriptionRecord.sName[i] = (char)sNameWchar[i];
        }
        TRACE_HIVE_INFO("    %8S : %s\n", NameKeyName, descriptionRecord.sName);

        if (!subKey.Query(DefaultKeyName, descriptionRecord.Default)) 
        {
            continue;
        }
        TRACE_HIVE_INFO("    %8S : %s\n", DefaultKeyName, descriptionRecord.Default ? "true" : "false");

        mfxU32 version;
        if (!subKey.Query(PlgVerKeyName, version)) 
        {
            continue;
        }
        descriptionRecord.PluginVersion = static_cast<mfxU16>(version);
        TRACE_HIVE_INFO("    %8S : %d\n", PlgVerKeyName, descriptionRecord.PluginVersion);

        
        if (!subKey.Query(APIVerKeyName, descriptionRecord.APIVersion)) 
        {
            continue;
        }

        if (minAPIVersion.Version < descriptionRecord.APIVersion.Version) 
        {
            TRACE_HIVE_ERROR("    %8S : %d.%d, but current MediasSDK version : %d.%d\n"
                , APIVerKeyName
                , descriptionRecord.APIVersion.Major
                , descriptionRecord.APIVersion.Minor
                , minAPIVersion.Major
                , minAPIVersion.Minor);
            continue;
        }

        TRACE_HIVE_INFO("    %8S : {%d.%d}\n", APIVerKeyName, descriptionRecord.APIVersion.Major, descriptionRecord.APIVersion.Minor);
        
        try 
        {
            mRecords[index] = descriptionRecord;
        }
        catch (...) {
            TRACE_HIVE_ERROR("mRecords[%d] = descriptionRecord; - threw exception \n", index);
        }
    }
} 

#endif