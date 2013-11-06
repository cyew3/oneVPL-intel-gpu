/* ****************************************************************************** *\

Copyright (C) 2012-2013 Intel Corporation.  All rights reserved.

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

File Name: mfx_win_reg_key.h

\* ****************************************************************************** */

#if !defined(__MFX_WIN_REG_KEY_H)
#define __MFX_WIN_REG_KEY_H

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <vector>
#include "mfxplugin.h"
#include "mfx_dispatcher_log.h"

namespace MFX {

template<class T> struct RegKey{};
template<> struct RegKey<bool>{enum {type = REG_DWORD};};
template<> struct RegKey<mfxU32>{enum {type = REG_DWORD};};
template<> struct RegKey<mfxPluginUID>{enum {type = REG_BINARY};};
template<> struct RegKey<std::wstring>{enum {type = REG_SZ};};
template<> struct RegKey<std::string>{enum {type = REG_SZ};};


class WinRegKey
{
public:
    // Default constructor
    WinRegKey(void);
    // Destructor
    ~WinRegKey(void);

    // Open a registry key
    bool Open(HKEY hRootKey, const wchar_t *pSubKey, REGSAM samDesired);
    bool Open(WinRegKey &rootKey, const wchar_t *pSubKey, REGSAM samDesired);

    // Query value
    bool QueryValueSize(const wchar_t *pValueName, DWORD type, LPDWORD pcbData);
    bool Query(const wchar_t *pValueName, DWORD type, LPBYTE pData, LPDWORD pcbData);

    template<class T>
    bool Query(const wchar_t *pValueName, T &data ) {
        DWORD size = sizeof(data);
        return Query(pValueName, RegKey<T>::type, (LPBYTE) &data, &size);
    }

    template<>
    bool Query<bool>(const wchar_t *pValueName, bool &data ) {
        mfxU32 value = 0;
        bool bRes = Query(pValueName, value);
        data = (1 == value);
        return bRes;
    }

    template<>
    bool Query<std::wstring>(const wchar_t *pValueName, std::wstring &data) {
        try {
            DWORD len = 0;
            if (!QueryValueSize(pValueName, RegKey<std::wstring>::type, &len)){
                return false;
            }
            std::vector<wchar_t> v(len);
            if (!Query(pValueName, RegKey<std::wstring>::type, (LPBYTE)&v.front(), &len)){
                return false;
            }
            data.assign(&v.front());
            return true;
        }
        catch (std::exception &e) {
            e;
            DISPATCHER_LOG_ERROR((("std::exception %s\n"), e.what()));
            return false;
        }
    }
    template<>
    bool Query(const wchar_t *pValueName, std::string &data) {
        try {
            DWORD len = 0;
            if (!QueryValueSize(pValueName, RegKey<std::string>::type, &len)){
                return false;
            }
            std::vector<wchar_t> v(len);
            if (!Query(pValueName, RegKey<std::string>::type, (LPBYTE)&v.front(), &len)){
                return false;
            }
            
            for (DWORD i = 0; i < v.size(); i++) {
                data.insert(data.end(), (char)v[i]);
            }
            return true;
        }
        catch (std::exception &e) {
            e;
            DISPATCHER_LOG_ERROR((("std::exception %s\n"), e.what()));
            return false;
        }

    }


    // Enumerate value names
    bool EnumValue(DWORD index, wchar_t *pValueName, LPDWORD pcchValueName, LPDWORD pType);
    bool EnumKey(DWORD index, wchar_t *pValueName, LPDWORD pcchValueName);

protected:

    // Release the object
    void Release(void);

    HKEY m_hKey;                                                // (HKEY) handle to the opened key

private:
    // unimplemented by intent to make this class non-copyable
    WinRegKey(const WinRegKey &);
    void operator=(const WinRegKey &);

};

} // namespace MFX

#endif // #if defined(_WIN32) || defined(_WIN64)

#endif // __MFX_WIN_REG_KEY_H
