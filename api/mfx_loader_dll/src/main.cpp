// Copyright (c) 2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfxsession.h"
#include "mfx_dispatcher.h"
#include "mfx_load_dll.h"
#include "mfxdefs.h"
#include "mfx_dxva2_device.h"
#include "mfxvideo++.h"

#include <atlbase.h>
#include <tchar.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <memory>
#include <tuple>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <regex>

const TCHAR * default_MSDK_dll_name =

#ifdef _WIN64
    _T("libmfxhw64.dll");
#else
    _T("libmfxhw32.dll");
#endif

// This string switches type according to UNICODE define
using tstring = std::basic_string<TCHAR>;

struct adapter_description
{
    tstring adapter_description_string;
    tstring path_to_msdk;
    mfxU32  extracted_device_id = 0xffffffff;
    mfxU32  device_id           = 0xffffffff;
    mfxU32  adapter_number      = 0xffffffff;
    bool    is_integrated       = false;
};


// RAII class to handle dll libraries

struct deleter_dll_handle
{
    // Need this to call deleter properly, this overrides deleted type from T* to deleter::pointer
    typedef mfxModuleHandle pointer;

    tstring lib_name;

    deleter_dll_handle() = default;

    deleter_dll_handle(const tstring name)
        : lib_name(name)
    {}

    void operator()(mfxModuleHandle handle)
    {
        if (!handle)
            return;

        if (MFX::mfx_dll_free(handle))
        {
            DISPATCHER_LOG_INFO(("Unloaded %S\n", lib_name.c_str()));
        }
        else
        {
            DISPATCHER_LOG_ERROR(("ERROR: Failed to unload %S\n", lib_name.c_str()));
        }
    }
};

using lib_ptr = std::unique_ptr <mfxModuleHandle, deleter_dll_handle>;

class dll_handler : public lib_ptr
{
public:
    dll_handler() = default;

    dll_handler(const tstring path)
        : lib_ptr(MFX::mfx_dll_load(path.c_str()), deleter_dll_handle(path))
    {
        if (get())
        {
            DISPATCHER_LOG_INFO(("Loaded %S\n", path.c_str()));
        }
        else
        {
            DISPATCHER_LOG_ERROR(("ERROR: Failed to load %S\n", path.c_str()));
        }
    }
};

inline void append_lib_name(tstring& path, const TCHAR* name)
{
    if (path.back() != _T('/') && path.back() != _T('\\'))
    {
        path += _T("\\");
    }
    path += name;
}

inline bool exctract_device_id(const tstring descr_string, mfxU32& device_id)
{
    using namespace std;

    DISPATCHER_LOG_INFO(("extract_device_id, descr_string = %S\n", descr_string.c_str()));

    basic_regex<TCHAR> rgx(_T(".*DEV_([0-9A-F]+).*$"), regex_constants::ECMAScript | regex_constants::icase);
    match_results<tstring::const_iterator> match;

    if (!regex_match(descr_string.cbegin(), descr_string.cend(), match, rgx))
    {
        return false;
    }

    tstring id_found(match[1].first, match[1].second);
    TCHAR* end;

    device_id = _tcstoul(id_found.c_str(), &end, 16);

    return true;
}

using adapter_vector = std::vector<adapter_description>;

bool ListAllIntelAdaptersFromRegistry(adapter_vector& a_v)
{
        // Get all Displays info through windows config manager
        TCHAR DisplayGUID[40];
        if (mfxI32 res = StringFromGUID2(GUID_DEVCLASS_DISPLAY, DisplayGUID, sizeof(DisplayGUID)) == 0)
        {
            DISPATCHER_LOG_ERROR(("ERROR: StringFromGUID2 failed with error %ul\n", res));
            return false;
        }

        ULONG DeviceIDListSize;

        CONFIGRET result = CM_Get_Device_ID_List_Size(&DeviceIDListSize, DisplayGUID, CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT);
        if (result != CR_SUCCESS)
        {
            DISPATCHER_LOG_ERROR(("ERROR: CM_Get_Device_ID_List_Size failed with error %ul\n", result));
            return false;
        }

        std::vector<TCHAR> DeviceIDList(DeviceIDListSize, _T('\0'));

        result = CM_Get_Device_ID_List(DisplayGUID, DeviceIDList.data(), DeviceIDListSize, CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT);
        if (result != CR_SUCCESS)
        {
            DISPATCHER_LOG_ERROR(("ERROR: CM_Get_Device_ID_List failed with error %ul\n", result));
            return false;
        }

        // Look for registry entries for Intel devices
        TCHAR intel_dll_path[MFX_MAX_DLL_PATH] = { _T('\0') };

        tstring devices(DeviceIDList.data(), DeviceIDList.data() + DeviceIDList.size());
        std::basic_istringstream<TCHAR> ss(devices);
        std::vector<TCHAR> name(DeviceIDList.size(), _T('\0'));

        // Iterate over device names
        while (ss.getline(name.data(), name.size(), _T('\0')))
        {
            // Use only Intel devices
            if (!_tcsstr(name.data(), _T("VEN_8086")) && !_tcsstr(name.data(), _T("ven_8086")))
                continue;

            DEVINST device_instance;
            result = CM_Locate_DevNode(&device_instance, reinterpret_cast<DEVINSTID>(name.data()), CM_LOCATE_DEVNODE_NORMAL);
            if (result != CR_SUCCESS)
            {
                DISPATCHER_LOG_WRN(("WARNING: CM_Locate_DevNode failed with status %ul\n", result));
                continue;
            }

            CRegKey key;
            CONFIGRET result = CM_Open_DevNode_Key(device_instance, KEY_READ, 0, RegDisposition_OpenExisting, &key.m_hKey, CM_REGISTRY_SOFTWARE);

            if (result != CR_SUCCESS)
            {
                DISPATCHER_LOG_WRN(("WARNING: CM_Open_DevNode_Key for device %ul failed with status %ul\n", device_instance, result));
                continue;
            }


            adapter_description curr_descr;

            // Extract path to MSDK lib
            DWORD path_size = sizeof(intel_dll_path);
            LSTATUS n_error = key.QueryStringValue(_T("DriverStorePathForMediaSDK"), intel_dll_path, &path_size);

            if (n_error != ERROR_SUCCESS)
            {
                DISPATCHER_LOG_WRN(("WARNING: QueryStringValue for \"DriverStorePathForMediaSDK\" key failed with status %l\n", n_error));
                continue;
            }
            curr_descr.path_to_msdk = intel_dll_path;
            append_lib_name(curr_descr.path_to_msdk, default_MSDK_dll_name);


            // Extract description string
            path_size = sizeof(intel_dll_path);
            n_error = key.QueryStringValue(_T("MatchingDeviceId"), intel_dll_path, &path_size);

            if (n_error != ERROR_SUCCESS)
            {
                DISPATCHER_LOG_WRN(("WARNING: RegQueryValueExA for \"MatchingDeviceId\" key failed with status %l\n", n_error));
                continue;
            }
            curr_descr.adapter_description_string = intel_dll_path;

            if (!exctract_device_id(curr_descr.adapter_description_string, curr_descr.extracted_device_id))
            {
                DISPATCHER_LOG_WRN(("Couldn't extract device id from adapter description string: %S\n", curr_descr.adapter_description_string.c_str()));
                continue;
            }

            a_v.emplace_back(std::move(curr_descr));
        }

        if (a_v.empty())
        {
            DISPATCHER_LOG_ERROR(("ERROR: Filed to find information about any Intel adapter\n"));
            return false;
        }

        DISPATCHER_LOG_INFO(("Obtained %zu adapters description from registry\n", a_v.size()));
        return true;
}

static bool UpdateInfoFromDXVAenumeration(adapter_vector& a_v)
{
    bool ret = false;

    MFX::DXVA2Device dxvaDevice;

    mfxU32 adapter_n = 0;
    for (; dxvaDevice.InitDXGI1(adapter_n); ++adapter_n)
    {
        if (dxvaDevice.GetVendorID() != INTEL_VENDOR_ID)
            continue;

        mfxU32 device_id = dxvaDevice.GetDeviceID();
        auto it = std::find_if(std::begin(a_v), std::end(a_v),
            [device_id](const adapter_description& descr)
            {
                return descr.extracted_device_id == device_id;
            });

        if (it == std::end(a_v))
            continue;

        it->device_id      = device_id;
        it->adapter_number = adapter_n;

        ret = true;
    }

    return ret;
}

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list) \
    {#func_name, API_VERSION},

const
FUNCTION_DESCRIPTION video_API_func[eVideoFuncTotal] =
{
    {"MFXInit",           {{0, 1}}},
    {"MFXClose",          {{0, 1}}},
    {"MFXQueryIMPL",      {{0, 1}}},
    {"MFXQueryVersion",   {{0, 1}}},

    {"MFXJoinSession",    {{1, 1}}},
    {"MFXDisjoinSession", {{1, 1}}},
    {"MFXCloneSession",   {{1, 1}}},
    {"MFXSetPriority",    {{1, 1}}},
    {"MFXGetPriority",    {{1, 1}}},

    {"MFXInitEx",         {{1, 14}}},

#include "mfx_exposed_functions_list.h"
};

const
FUNCTION_DESCRIPTION audio_API_func[eAudioFuncTotal] =
{
    {"MFXInit",           {{8, 1}}},
    {"MFXClose",          {{8, 1}}},
    {"MFXQueryIMPL",      {{8, 1}}},
    {"MFXQueryVersion",   {{8, 1}}},

    {"MFXJoinSession",    {{8, 1}}},
    {"MFXDisjoinSession", {{8, 1}}},
    {"MFXCloneSession",   {{8, 1}}},
    {"MFXSetPriority",    {{8, 1}}},
    {"MFXGetPriority",    {{8, 1}}},

#include "mfxaudio_exposed_functions_list.h"
};

class MSDK_lib_handler : public dll_handler
{
public:
    MSDK_lib_handler() = default;

    MSDK_lib_handler(const tstring path, const mfxU32 adapter_num, const bool is_audio)
        : dll_handler(path)
        , adapter_number(adapter_num)
    {
        if (!get())
            throw std::exception();

        if (is_audio)
        {
            // load audio functions: pointers to exposed functions
            std::ignore = std::transform(audio_API_func, audio_API_func + eAudioFuncTotal, callAudioTable,
                [this](FUNCTION_DESCRIPTION const& desc)
                {
                    auto p_proc = MFX::mfx_dll_get_addr(get(), desc.pName);
                    if (!p_proc)
                    {
                        // The library doesn't contain the function
                        DISPATCHER_LOG_WRN((("Can't find API function \"%s\"\n"), desc.pName));
                    }
                    return p_proc;
                }
            );

            actual_table = callAudioTable;
        }
        else
        {
            // load video functions: pointers to exposed functions
            std::ignore = std::transform(video_API_func, video_API_func + eVideoFuncTotal, callTable,
                [this](FUNCTION_DESCRIPTION const& desc)
            {
                auto p_proc = MFX::mfx_dll_get_addr(get(), desc.pName);
                if (!p_proc)
                {
                    // The library doesn't contain the function
                    DISPATCHER_LOG_WRN((("Can't find API function \"%s\"\n"), desc.pName));
                }
                return p_proc;
            }
            );

            actual_table = callTable;
        }

    }

    ~MSDK_lib_handler()
    {
    }

    MSDK_lib_handler& operator=(MSDK_lib_handler&& other)
    {
        dll_handler::operator=(std::move(other));

        std::copy(std::begin(other.callTable),      std::end(other.callTable),      std::begin(callTable));
        std::copy(std::begin(other.callAudioTable), std::end(other.callAudioTable), std::begin(callAudioTable));

        actual_table = other.actual_table == other.callTable ? callTable : callAudioTable;

        adapter_number = other.adapter_number;

        return *this;
    }

    mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session)
    {
        // Call old-style MFXInit init for older libraries and audio library
        bool callOldInit = (par.Implementation & MFX_IMPL_AUDIO) || !actual_table[eMFXInitEx]; // if true call eMFXInit, if false - eMFXInitEx

        mfxFunctionPointer pFunc = actual_table[callOldInit ? eMFXInit : eMFXInitEx];

        if (!pFunc)
        {
            DISPATCHER_LOG_ERROR(("ERROR: Can't find required API function: %s\n", callOldInit ? "MFXInit" : "MFXInitEx"));
            return MFX_ERR_INVALID_HANDLE;
        }

        return callOldInit ? (*reinterpret_cast<mfxStatus(MFX_CDECL *) (mfxIMPL, mfxVersion *, mfxSession *)> (pFunc)) (par.Implementation, &par.Version, session)
                           : (*reinterpret_cast<mfxStatus(MFX_CDECL *) (mfxInitParam,          mfxSession *)> (pFunc)) (par, session);
    }

    mfxFunctionPointer  callTable[eVideoFuncTotal]      = {};
    mfxFunctionPointer  callAudioTable[eAudioFuncTotal] = {};

    mfxFunctionPointer* actual_table                    = nullptr;
    mfxU32              adapter_number                  = 0xffffffff;
};


// Following wrapper class is used only to query MSDK for associated adapter type
class MSDK_lib_wrapper : public MSDK_lib_handler
{
public:
    MSDK_lib_wrapper(const tstring path, const mfxU32 adapter_number, const bool is_audio)
        : MSDK_lib_handler(path, adapter_number, is_audio)
    {
        if (!get())
            throw std::exception();

        MFXInit                    = callTable[eMFXInit];
        MFXClose                   = callTable[eMFXClose];
        MFXVideoCORE_QueryPlatform = callTable[eMFXVideoCORE_QueryPlatform];

        if (!MFXInit || !MFXClose || !MFXVideoCORE_QueryPlatform)
            throw std::exception();

        mfxIMPL implementation;
        switch (adapter_number)
        {
        case 0:
            implementation = MFX_IMPL_HARDWARE;
            break;
        case 1:
            implementation = MFX_IMPL_HARDWARE2;
            break;
        case 2:
            implementation = MFX_IMPL_HARDWARE3;
            break;
        case 3:
            implementation = MFX_IMPL_HARDWARE4;
            break;

        default:
            // try searching on all display adapters
            //initPar.Implementation = MFX_IMPL_HARDWARE_ANY;
            DISPATCHER_LOG_WRN(("Currently only up to 4 adapters supported\n"));
            throw std::exception();
            break;
        }

        // Since this class used only for MFXVideoCORE_QueryPlatform call, we can set up any type
        implementation |= MFX_IMPL_VIA_D3D11;

        mfxStatus sts = (*reinterpret_cast<mfxStatus(MFX_CDECL *) (mfxIMPL, mfxVersion *, mfxSession *)> (MFXInit)) (implementation, nullptr, &session);
        if (sts != MFX_ERR_NONE)
        {
            DISPATCHER_LOG_WRN(("MFXInit returned status: %d\n", sts));

            if (sts < MFX_ERR_NONE)
                throw std::exception();
        }
    }

    ~MSDK_lib_wrapper()
    {
        if (MFXClose && session)
        {
            mfxStatus sts = (*reinterpret_cast<mfxStatus(MFX_CDECL *) (mfxSession)> (MFXClose)) (session);
            if (sts != MFX_ERR_NONE)
            {
                DISPATCHER_LOG_WRN(("MFXClose returned status: %d\n", sts));
            }
        }
    }

    mfxSession         session                    = nullptr;

    mfxFunctionPointer MFXInit                    = nullptr;
    mfxFunctionPointer MFXClose                   = nullptr;
    mfxFunctionPointer MFXVideoCORE_QueryPlatform = nullptr;
};

// This is global instance of loaded MSDK library
MSDK_lib_handler global_msdk_handle;

bool GetAdapterTypeFromMsdkLibrary(adapter_vector& a_v)
{
    bool ret = false;

    for (auto& descr : a_v)
    {
        try
        {
            MSDK_lib_wrapper msdk_lib(descr.path_to_msdk, descr.adapter_number, false);

            mfxPlatform platform_info = {};
            mfxStatus sts = (*reinterpret_cast<mfxStatus(MFX_CDECL *) (mfxSession, mfxPlatform*)> (msdk_lib.MFXVideoCORE_QueryPlatform)) (msdk_lib.session, &platform_info);

            if (sts != MFX_ERR_NONE)
            {
                DISPATCHER_LOG_WRN(("MFXVideoCORE_QueryPlatform call failed\n"));
                continue;
            }

            descr.is_integrated = platform_info.MediaAdapterType == MFX_MEDIA_INTEGRATED;
        }
        catch (const std::exception& ex)
        {
            std::ignore = ex;
            DISPATCHER_LOG_WRN(("Exception catched %s\n", ex.what()));
            continue;
        }
        catch (...)
        {
            DISPATCHER_LOG_WRN(("Exception catched\n"));
            continue;
        }

        ret = true;
    }

    return ret;
}

static bool SetupAdaptersList(adapter_vector& a_v)
{
    if (!ListAllIntelAdaptersFromRegistry(a_v))
    {
        DISPATCHER_LOG_ERROR(("ERROR: Couldn't get information about adapters from Registry\n"));
        return false;
    }

    if (!UpdateInfoFromDXVAenumeration(a_v))
    {
        DISPATCHER_LOG_ERROR(("ERROR: Couldn't get information about adapters from DXVA\n"));
        return false;
    }

    if (!GetAdapterTypeFromMsdkLibrary(a_v))
    {
        DISPATCHER_LOG_ERROR(("ERROR: Couldn't get information about adapters from MSDK\n"));
        return false;
    }

    return true;
}

static bool LoadMSDKLibary(adapter_vector& a_v, mfxIMPL impl)
{
    if (a_v.empty())
        return false;

    // If user requested specific adapter, load MSDK associated with given adapter
    mfxI32 preffered_adapter_number = -1;
    switch (impl & 0xf)
    {
    case MFX_IMPL_HARDWARE:
        preffered_adapter_number = 0;
        break;
    case MFX_IMPL_HARDWARE2:
        preffered_adapter_number = 1;
        break;
    case MFX_IMPL_HARDWARE3:
        preffered_adapter_number = 2;
        break;
    case MFX_IMPL_HARDWARE4:
        preffered_adapter_number = 3;
        break;
    }

    // Try to load integrated Gfx by default
    auto it_to_load = std::find_if(std::begin(a_v), std::end(a_v),
        [preffered_adapter_number](const adapter_description & descr)
        {
            return (preffered_adapter_number == -1) ? descr.is_integrated : (preffered_adapter_number == descr.adapter_number);
        });

    if (it_to_load == std::end(a_v))
    {
        if (preffered_adapter_number != -1)
        {
            DISPATCHER_LOG_ERROR(("ERROR: Couldn't find user specified adapter with index: %d\n", preffered_adapter_number));
            return false;
        }

        // If no integrated Gfx (and used didn't request particular adapter), load first adapter from the list (discrete)
        it_to_load = std::begin(a_v);
    }

    try
    {
        global_msdk_handle = std::move(MSDK_lib_handler(it_to_load->path_to_msdk, it_to_load->adapter_number, !!(impl & MFX_IMPL_AUDIO)));
    }
    catch (const std::exception& ex)
    {
        std::ignore = ex;
        DISPATCHER_LOG_ERROR(("ERROR:Exception catched %s\n", ex.what()));
        return false;
    }
    catch (...)
    {
        DISPATCHER_LOG_ERROR(("ERROR:Exception catched\n"));
        return false;
    }

    return true;
}

//#define FUNC_EXPORT(ret_type) extern "C" __declspec(dllexport) ret_type MFX_CDECL

extern "C" mfxStatus MFX_CDECL MFXInitEx(mfxInitParam par, mfxSession *session)
{
    if (!session)
    {
        DISPATCHER_LOG_ERROR(("ERROR: Session pointer is NULL\n"));
        return MFX_ERR_NULL_PTR;
    }

    // Find all available adapters and corresponding MSDK libraries
    try
    {
        adapter_vector a_v;
        if (!SetupAdaptersList(a_v))
        {
            DISPATCHER_LOG_ERROR(("ERROR: Failed to get list of adapters available on system\n"));
            return MFX_ERR_UNSUPPORTED;
        }

        // Load proper MSDK library

        if (!LoadMSDKLibary(a_v, par.Implementation))
        {
            DISPATCHER_LOG_ERROR(("ERROR: Failed to load MSDK library\n"));
            return MFX_ERR_UNSUPPORTED;
        }

        // Set proper implementation for selected device

        switch (par.Implementation & 0xf)
        {
        case MFX_IMPL_AUTO:
        case MFX_IMPL_AUTO_ANY:
        case MFX_IMPL_HARDWARE_ANY:
            par.Implementation &= ~(0xf);

            switch (global_msdk_handle.adapter_number)
            {
            case 0:
                par.Implementation |= MFX_IMPL_HARDWARE;
                break;
            case 1:
                par.Implementation |= MFX_IMPL_HARDWARE2;
                break;
            case 2:
                par.Implementation |= MFX_IMPL_HARDWARE3;
                break;
            case 3:
                par.Implementation |= MFX_IMPL_HARDWARE4;
                break;
            default:
                DISPATCHER_LOG_ERROR(("ERROR: Only up to 4 adapters supported\n"));
                return MFX_ERR_UNSUPPORTED;
            }
            break;
        }
    }
    catch (const std::exception& ex)
    {
        std::ignore = ex;
        DISPATCHER_LOG_ERROR(("ERROR:Exception catched %s\n", ex.what()));
        return MFX_ERR_UNSUPPORTED;
    }
    catch (...)
    {
        DISPATCHER_LOG_ERROR(("ERROR:Exception catched\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    // Call init for loaded library
    return global_msdk_handle.MFXInitEx(par, session);
}

extern "C" mfxStatus MFX_CDECL MFXInit(mfxIMPL impl, mfxVersion *pVer, mfxSession *session)
{
    mfxInitParam par = {};

    par.Implementation = impl;
    if (pVer)
    {
        par.Version = *pVer;
    }
    else
    {
        par.Version.Major = DEFAULT_API_VERSION_MAJOR;
        par.Version.Minor = DEFAULT_API_VERSION_MINOR;
    }
    par.ExternalThreads = 0;

    return MFXInitEx(par, session);
}

extern "C" mfxStatus MFX_CDECL MFXClose(mfxSession session)
{
    if (!session) {
        DISPATCHER_LOG_ERROR(("ERROR: Invalid session\n"));
        return MFX_ERR_INVALID_HANDLE;
    }

    if (!global_msdk_handle) {
        DISPATCHER_LOG_ERROR(("ERROR: MSDK lib wasn't loaded\n"));
        return MFX_ERR_INVALID_HANDLE;
    }

    mfxFunctionPointer pFunc = global_msdk_handle.actual_table[eMFXClose];

    if (!pFunc)
    {
        DISPATCHER_LOG_ERROR(("ERROR: Can't find required API function: MFXClose\n"));
        return MFX_ERR_INVALID_HANDLE;
    }

    return (*reinterpret_cast<mfxStatus(MFX_CDECL *) (mfxSession)> (pFunc)) (session);
}


//
//
// implement all other calling functions.
// They just call a procedure of DLL library from the table.
//

// define for common functions (from mfxsession.h)
#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list)                      \
extern "C" return_value MFX_CDECL func_name formal_param_list                                        \
{                                                                                                    \
    if (!global_msdk_handle || !global_msdk_handle.actual_table)                                     \
    {                                                                                                \
        DISPATCHER_LOG_ERROR(("ERROR: MSDK lib wasn't loaded\n"));                                   \
        return MFX_ERR_INVALID_HANDLE;                                                               \
    }                                                                                                \
                                                                                                     \
    mfxFunctionPointer pFunc = global_msdk_handle.actual_table[e##func_name];                        \
                                                                                                     \
    if (!pFunc)                                                                                      \
    {                                                                                                \
        DISPATCHER_LOG_ERROR(("ERROR: Can't find required API function: " #func_name "\n"));         \
        return MFX_ERR_INVALID_HANDLE;                                                               \
    }                                                                                                \
                                                                                                     \
    return (*reinterpret_cast<mfxStatus (MFX_CDECL *) formal_param_list> (pFunc)) actual_param_list; \
}

FUNCTION(mfxStatus, MFXQueryIMPL, (mfxSession session, mfxIMPL *impl), (session, impl))
FUNCTION(mfxStatus, MFXQueryVersion, (mfxSession session, mfxVersion *version), (session, version))

// these functions are not necessary in LOADER part of dispatcher and
// need to be included only in in SOLID dispatcher or PROCTABLE part of dispatcher

FUNCTION(mfxStatus, MFXJoinSession, (mfxSession session, mfxSession child_session), (session, child_session))
FUNCTION(mfxStatus, MFXCloneSession, (mfxSession session, mfxSession *clone), (session, clone))

FUNCTION(mfxStatus, MFXDisjoinSession, (mfxSession session), (session))
FUNCTION(mfxStatus, MFXSetPriority, (mfxSession session, mfxPriority priority), (session, priority))
FUNCTION(mfxStatus, MFXGetPriority, (mfxSession session, mfxPriority *priority), (session, priority))

#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list)                      \
extern "C" return_value MFX_CDECL func_name formal_param_list                                        \
{                                                                                                    \
    if (!global_msdk_handle || !global_msdk_handle.callTable)                                        \
    {                                                                                                \
        DISPATCHER_LOG_ERROR(("ERROR: MSDK lib wasn't loaded\n"));                                   \
        return MFX_ERR_INVALID_HANDLE;                                                               \
    }                                                                                                \
                                                                                                     \
    mfxFunctionPointer pFunc = global_msdk_handle.callTable[e##func_name];                           \
                                                                                                     \
    if (!pFunc)                                                                                      \
    {                                                                                                \
        DISPATCHER_LOG_ERROR(("ERROR: Can't find required API function: " #func_name "\n"));         \
        return MFX_ERR_INVALID_HANDLE;                                                               \
    }                                                                                                \
                                                                                                     \
    return (*reinterpret_cast<mfxStatus (MFX_CDECL *) formal_param_list> (pFunc)) actual_param_list; \
}

#include "mfx_exposed_functions_list.h"
#undef FUNCTION
#define FUNCTION(return_value, func_name, formal_param_list, actual_param_list)                      \
extern "C" return_value MFX_CDECL func_name formal_param_list                                        \
{                                                                                                    \
    if (!global_msdk_handle || !global_msdk_handle.callAudioTable)                                   \
    {                                                                                                \
        DISPATCHER_LOG_ERROR(("ERROR: MSDK lib wasn't loaded\n"));                                   \
        return MFX_ERR_INVALID_HANDLE;                                                               \
    }                                                                                                \
                                                                                                     \
    mfxFunctionPointer pFunc = global_msdk_handle.callAudioTable[e##func_name];                      \
                                                                                                     \
    if (!pFunc)                                                                                      \
    {                                                                                                \
        DISPATCHER_LOG_ERROR(("ERROR: Can't find required API function: " #func_name "\n"));         \
        return MFX_ERR_INVALID_HANDLE;                                                               \
    }                                                                                                \
                                                                                                     \
    return (*reinterpret_cast<mfxStatus (MFX_CDECL *) formal_param_list> (pFunc)) actual_param_list; \
}

#include "mfxaudio_exposed_functions_list.h"