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
    const wchar_t DefaultKeyName[] = L"Default";
    const wchar_t PlgVerKeyName[] = L"PlgVer";
    const wchar_t APIVerKeyName[] = L"APIVer";
}

namespace 
{
    const wchar_t pluginFileName32[] = L"FileName32";
    const wchar_t pluginFileName64[] = L"FileName64";
    const wchar_t pluginCfgFileName[] = L"plugin.cfg";
    const wchar_t pluginSearchPattern[] = L"????????????????????????????????";
    const mfxU32 pluginCfgFileNameLen = 10;
    const mfxU32 pluginDirNameLen = 32;
    const mfxU32 slashLen = 1;
    enum 
    {
        MAX_PLUGIN_FILE_LINE = 4096
    };
}


MFX::MFXPluginsInHive::MFXPluginsInHive( mfxU32 mfxStorageID, mfxVersion requiredAPIVersion )
{
    HKEY rootHKey;
    bool bRes;
    WinRegKey regKey;   

    // open required registry key
    rootHKey = (MFX_LOCAL_MACHINE_KEY == mfxStorageID) ? (HKEY_LOCAL_MACHINE) : (HKEY_CURRENT_USER);
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

        TRACE_HIVE_INFO("Found Plugin: %s\\%S\\%S\n", (MFX_LOCAL_MACHINE_KEY == mfxStorageID) ? ("HKEY_LOCAL_MACHINE") : ("HKEY_CURRENT_USER"),
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

        if (requiredAPIVersion.Version != descriptionRecord.APIVersion.Version) 
        {
            TRACE_HIVE_ERROR("    %8S : %d.%d, but current MediasSDK version : %d.%d\n"
                , APIVerKeyName
                , descriptionRecord.APIVersion.Major
                , descriptionRecord.APIVersion.Minor
                , requiredAPIVersion.Major
                , requiredAPIVersion.Minor);
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

MFX::MFXPluginsInFS::MFXPluginsInFS(mfxVersion requiredAPIVersion)
{
    WIN32_FIND_DATAW find_data;
    msdk_disp_char currentModuleName[MAX_PLUGIN_PATH];
    
    GetModuleFileNameW(NULL, currentModuleName, MAX_PLUGIN_PATH);
    if (GetLastError() != 0) 
    {
        TRACE_HIVE_ERROR("GetModuleFileName() reported an error: %d\n", GetLastError());
        return;
    }
    msdk_disp_char *lastSlashPos = currentModuleName;
    msdk_disp_char *currSlashPos = 0;
    while(0 != (currSlashPos = wcsstr(lastSlashPos, L"\\")))
    {
        lastSlashPos = currSlashPos + 1;
    }
    mfxU32 executableDirLen = (mfxU32)(lastSlashPos - currentModuleName);
    if (executableDirLen + pluginDirNameLen + pluginCfgFileNameLen + slashLen >= MAX_PLUGIN_PATH) 
    {
        TRACE_HIVE_ERROR("MAX_PLUGIN_PATH which is %d, not enough lo locate plugin path\n", MAX_PLUGIN_PATH);
        return;
    }
    msdk_disp_char_cpy_s(currentModuleName + executableDirLen
        , MAX_PLUGIN_PATH - executableDirLen, pluginSearchPattern);

    HANDLE fileFirst = FindFirstFileW(currentModuleName, &find_data);
    if (INVALID_HANDLE_VALUE == fileFirst) 
    {
        TRACE_HIVE_ERROR("FindFirstFileW() unable to locate any plugins folders\n", 0);
        return;
    }
    do 
    {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) 
        {
            continue;
        }
        if (pluginDirNameLen != wcslen(find_data.cFileName)) 
        {
            continue;
        }
        //converting dirname into guid
        PluginDescriptionRecord descriptionRecord;
        descriptionRecord.APIVersion = requiredAPIVersion;
        descriptionRecord.onlyVersionRegistered = true;

        mfxU32 i = 0;
        for(i = 0; i != 16; i++) 
        {
            mfxU32 hexNum = 0;
            if (1 != swscanf_s(find_data.cFileName + 2*i, L"%2x", &hexNum)) 
            {
                TRACE_HIVE_INFO("folder name \"%S\" is not a valid GUID string\n", find_data.cFileName);
                break;
            }
            if (hexNum == 0 && find_data.cFileName + 2*i != wcsstr(find_data.cFileName + 2*i, L"00"))
            {
                TRACE_HIVE_INFO("folder name \"%S\" is not a valid GUID string\n", find_data.cFileName);
                break;
            }
            descriptionRecord.PluginUID.Data[i] = (mfxU8)hexNum;
        }
        if (i != 16) {
            continue;
        }

        msdk_disp_char_cpy_s(currentModuleName + executableDirLen
            , MAX_PLUGIN_PATH - executableDirLen, find_data.cFileName);

        msdk_disp_char_cpy_s(currentModuleName + executableDirLen + pluginDirNameLen
            , MAX_PLUGIN_PATH - executableDirLen - pluginDirNameLen, L"\\");

        //this is path to plugin directory
        msdk_disp_char_cpy_s(descriptionRecord.sPath
            , sizeof(descriptionRecord.sPath) / sizeof(*descriptionRecord.sPath), currentModuleName);
        
        msdk_disp_char_cpy_s(currentModuleName + executableDirLen + pluginDirNameLen + slashLen
            , MAX_PLUGIN_PATH - executableDirLen - pluginDirNameLen - slashLen, pluginCfgFileName);

        FILE *pluginCfgFile = 0;
        _wfopen_s(&pluginCfgFile, currentModuleName, L"r");
        if (!pluginCfgFile) 
        {
            TRACE_HIVE_INFO("in directory \"%S\" no mandatory \"%S\"\n"
                , find_data.cFileName, pluginCfgFileName);
            continue;
        }
        
        if (ParseFile(pluginCfgFile, descriptionRecord)) 
        {
            try 
            {
                mRecords.push_back(descriptionRecord);
            }
            catch (...) {
                TRACE_HIVE_ERROR("mRecords.push_back(descriptionRecord); - threw exception \n", 0);
            }            
        }

        fclose(pluginCfgFile);
    }while (FindNextFileW(fileFirst, &find_data));
    FindClose(fileFirst);
}

bool MFX::MFXPluginsInFS::ParseFile(FILE * f, PluginDescriptionRecord & descriptionRecord) 
{
    msdk_disp_char line[MAX_PLUGIN_FILE_LINE];
    
    while(NULL != fgetws(line, sizeof(line) / sizeof(*line), f))
    {
        msdk_disp_char *delimiter = wcsstr(line, L"=");
        if (0 == delimiter) 
        {
            TRACE_HIVE_INFO("plugin.cfg contains line \"%S\" which is not in K=V format, skipping \n", line);
            continue;
        }
        *delimiter = 0;
        if (!ParseKVPair(line, delimiter + 1, descriptionRecord)) 
        {
            return false;
        }
    }

    if (!wcslen(descriptionRecord.sPath)) 
    {
        return false;
    }

    return true;
}

bool MFX::MFXPluginsInFS::ParseKVPair( msdk_disp_char * key, msdk_disp_char* value, PluginDescriptionRecord & descriptionRecord)
{
    if (0 != wcsstr(key, PlgVerKeyName))
    {
        mfxU32 version ;
        if (0 == swscanf_s(value, L"%d", &version)) 
        {
            return false;
        }
        descriptionRecord.PluginVersion = (mfxU16)version;
        
        TRACE_HIVE_INFO("%S: %S = %d \n", pluginCfgFileName, PlgVerKeyName, descriptionRecord.PluginVersion);
        return true;
    }
    bool isW32Path = 0!=wcsstr(key, pluginFileName32);
    bool isW64Path = 0!=wcsstr(key, pluginFileName64);
    if (isW64Path || isW32Path)
    {
        msdk_disp_char *startQuoteMark = wcsstr(value, L"\"");
        if (!startQuoteMark)
        {
            return false;
        }
        msdk_disp_char *prevQuoteMark = startQuoteMark++;
        msdk_disp_char *endQuoteMark = 0;
        while (0 != (prevQuoteMark =  wcsstr(prevQuoteMark + 1, L"\""))) {
            endQuoteMark = prevQuoteMark;
        }
        if (!endQuoteMark) 
        {
            return false;
        }
        *endQuoteMark = 0;

        mfxU32 currentPathLen = (mfxU32)wcslen(descriptionRecord.sPath);
        if (currentPathLen + wcslen(startQuoteMark) > sizeof(descriptionRecord.sPath) / sizeof(*descriptionRecord.sPath))
        {
            TRACE_HIVE_ERROR("buffer of MAX_PLUGIN_PATH characters which is %d, not enough lo store plugin path: %S%S\n"
                , MAX_PLUGIN_PATH, descriptionRecord.sPath, startQuoteMark);
            return false;
        }

#ifdef _WIN64
        if (isW64Path) 
        {
            msdk_disp_char_cpy_s(descriptionRecord.sPath + currentPathLen
                , sizeof(descriptionRecord.sPath) / sizeof(*descriptionRecord.sPath) - currentPathLen, startQuoteMark);
            TRACE_HIVE_INFO("%S: %S = \"%S\" \n", pluginCfgFileName, pluginFileName64, startQuoteMark);
        }
#else
        if (isW32Path)
        {
            msdk_disp_char_cpy_s(descriptionRecord.sPath + currentPathLen
                , sizeof(descriptionRecord.sPath) / sizeof(*descriptionRecord.sPath) - currentPathLen, startQuoteMark);
            TRACE_HIVE_INFO("%S: %S = \"%S\" \n", pluginCfgFileName, pluginFileName32, startQuoteMark);
        }
#endif
        return true;
    }
   

    return true;
}
#endif