// Copyright (c) 2010-2018 Intel Corporation
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

#ifndef __MFX_EVNT_PROV_H

#define _TARGET_WINVER _WIN32_WINNT_WINXP

#pragma push_macro("WINVER")

#undef  WINVER
#define WINVER _TARGET_WINVER

#include <evntprov.h>

//if target is prior to vista we need to add stub functions
#if (_TARGET_WINVER < _WIN32_WINNT_VISTA)

///dll that exported events functions on vista only
#define ADVAPI_DLL "Advapi32.dll"

#define DECLARE_METHOD(ret_type, method_name, formal_params, informal_params, ret_val)\
static ret_type __stdcall method_name formal_params\
{\
    if (!EventsDll<1>::get().hmodule)\
    return ret_val;\
    return (*(ret_type (__stdcall*) formal_params) EventsDll<1>::g_fnc_list[e##method_name].fnc_ptr) informal_params;\
}

#define DEF_FNC(name)\
{#name, NULL}

enum eFunctions
{
    eEventRegister = 0,
    eEventUnregister,
    eEventEnabled,
    eEventProviderEnabled,
    eEventWrite,
    eEventWriteTransfer,
    eEventWriteEx,
    eEventWriteString,
    eEventActivityIdControl,
};

template<int iVal>
class EventsDll
{
    //singletone
    static EventsDll    g_instance;


    EventsDll()
    {
        if (NULL == (hmodule = LoadLibraryA(ADVAPI_DLL)))
            return;

        //setting up pointers to all Event related functions
        for (int i = 0; i<sizeof(g_fnc_list) / sizeof(g_fnc_list[0]); i++)
        {
            g_fnc_list[i].fnc_ptr = GetProcAddress(hmodule, g_fnc_list[i].fnc_name);
            if (NULL == g_fnc_list[i].fnc_ptr)
            {
                FreeLibrary(hmodule);
                hmodule = NULL;
                return;
            }
        }
    }
public:
    static struct       _fnc_name
    {
        char fnc_name[32];
        FARPROC    fnc_ptr;
    } g_fnc_list[];

    //loaded advapi handle
    HMODULE hmodule;

    ~EventsDll()
    {
        if (hmodule)
        {
            FreeLibrary(hmodule);
            hmodule = NULL;
        }
    }
    static EventsDll& get(){return g_instance;}
};

template<int iVal>
EventsDll<iVal> EventsDll<iVal>::g_instance;

template<int iVal>
typename EventsDll<iVal>::_fnc_name EventsDll<iVal>::g_fnc_list[] =
{
    DEF_FNC(EventRegister),
    DEF_FNC(EventUnregister),
    DEF_FNC(EventEnabled),
    DEF_FNC(EventProviderEnabled),
    DEF_FNC(EventWrite),
    DEF_FNC(EventWriteTransfer),
    DEF_FNC(EventWriteEx),
    DEF_FNC(EventWriteString),
    DEF_FNC(EventActivityIdControl),
};


#else // if (_TARGET_WINVER < _WIN32_WINNT_VISTA)

    #define DECLARE_METHOD(ret_type, method_name, formal_params, informal_params, ret_val)

#endif
//uninitialized static functions removed warning
#pragma warning (disable:4505)

DECLARE_METHOD(ULONG, EventRegister,
            ( __in LPCGUID ProviderId,
             __in_opt PENABLECALLBACK EnableCallback,
             __in_opt PVOID CallbackContext,
             __out PREGHANDLE RegHandle),
             ( ProviderId, EnableCallback, CallbackContext, RegHandle),
             ERROR_INVALID_HANDLE);

DECLARE_METHOD(ULONG, EventUnregister,(__in REGHANDLE RegHandle), (RegHandle), ERROR_INVALID_HANDLE);

DECLARE_METHOD(BOOLEAN, EventEnabled, (REGHANDLE RegHandle,PCEVENT_DESCRIPTOR EventDescriptor),(RegHandle, EventDescriptor), FALSE);

DECLARE_METHOD(BOOLEAN, EventProviderEnabled,
            (__in REGHANDLE RegHandle,
             __in UCHAR Level,
             __in ULONGLONG Keyword),
             (RegHandle, Level, Keyword),
             FALSE);

DECLARE_METHOD(ULONG, EventWrite,
            (__in REGHANDLE RegHandle,
             __in PCEVENT_DESCRIPTOR EventDescriptor,
             __in ULONG UserDataCount,
             __in_ecount_opt(UserDataCount) PEVENT_DATA_DESCRIPTOR UserData),
             (RegHandle, EventDescriptor, UserDataCount, UserData),
             ERROR_INVALID_HANDLE);

DECLARE_METHOD(ULONG, EventWriteTransfer,
            (__in REGHANDLE RegHandle,
             __in PCEVENT_DESCRIPTOR EventDescriptor,
             __in_opt LPCGUID ActivityId,
             __in_opt LPCGUID RelatedActivityId,
             __in ULONG UserDataCount,
             __in_ecount_opt(UserDataCount) PEVENT_DATA_DESCRIPTOR UserData),
             (RegHandle,EventDescriptor,ActivityId,RelatedActivityId,UserDataCount,UserData),
             ERROR_INVALID_HANDLE);

DECLARE_METHOD(ULONG, EventWriteEx,
            (__in REGHANDLE RegHandle,
             __in PCEVENT_DESCRIPTOR EventDescriptor,
             __in ULONG64 Filter,
             __in ULONG Flags,
             __in_opt LPCGUID ActivityId,
             __in_opt LPCGUID RelatedActivityId,
             __in ULONG UserDataCount,
             __in_ecount_opt(UserDataCount) PEVENT_DATA_DESCRIPTOR UserData),
             (RegHandle,EventDescriptor,Filter,Flags,ActivityId,RelatedActivityId,UserDataCount,UserData),
             ERROR_INVALID_HANDLE);

DECLARE_METHOD(ULONG, EventWriteString,
            (__in REGHANDLE RegHandle,
             __in UCHAR Level,
             __in ULONGLONG Keyword,
             __in PCWSTR String),
             (RegHandle,Level,Keyword,String),
             ERROR_INVALID_HANDLE);

DECLARE_METHOD(ULONG, EventActivityIdControl,
            (__in ULONG ControlCode,
             __inout LPGUID ActivityId),
             (ControlCode,ActivityId),
             ERROR_INVALID_HANDLE);

#pragma pop_macro("WINVER")

#endif//__MFX_EVNT_PROV_H