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
}

MFX::MFXPluginHive::MFXPluginHive( mfxU32 storageID /*= 0*/ )
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
    
    for(DWORD index = 0; ; index++) 
    {
        WinRegKey subKey;
        wchar_t subKeyName[MFX_MAX_VALUE_NAME];
        DWORD   subKeyNameSize = sizeof(subKeyName) / sizeof(subKeyName[0]);

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

        if (!subKey.Query(GUIDKeyName, descriptionRecord.uid)) 
        {
            continue;
        }
        TRACE_HIVE_INFO("    %8S : "MFXGUIDTYPE()"\n", GUIDKeyName, MFXGUIDTOHEX(descriptionRecord.uid));

        if (!subKey.Query(PathKeyName, descriptionRecord.Path)) 
        {
            TRACE_HIVE_WRN("no value for : %S\n", PathKeyName);
            continue;
        }
        TRACE_HIVE_INFO("    %8S : %S\n", PathKeyName, descriptionRecord.Path.c_str());

        if (!subKey.Query(NameKeyName, descriptionRecord.Name)) 
        {
            continue;
        }
        TRACE_HIVE_INFO("    %8S : %s\n", NameKeyName, descriptionRecord.Name.c_str());

        if (!subKey.Query(DefaultKeyName, descriptionRecord.Default)) 
        {
            continue;
        }
        TRACE_HIVE_INFO("    %8S : %s\n", DefaultKeyName, descriptionRecord.Default ? "true" : "false");
        
        try 
        {
            mRecords.push_back(descriptionRecord);
        }
        catch (std::exception &e) 
        {
            e;
            TRACE_HIVE_ERROR("mRecords.push_back() - std::exception: %s\n", e.what());
        }
        catch (...) {
            TRACE_HIVE_ERROR("mRecords.push_back() - unknown exception: \n", 0);
        }
    }

} 

//avoid static runtime dependency from vs2005
#if _MSC_VER == 1400 
    _MRTIMP2_NPURE_NCEEPURE void __CLRCALL_PURE_OR_CDECL std::_String_base::_Xlen() {}
    _MRTIMP2_NPURE_NCEEPURE void __CLRCALL_PURE_OR_CDECL std::_String_base::_Xran() {}
#endif

#endif