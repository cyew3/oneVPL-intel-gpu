// Copyright (c) 2019 Intel Corporation
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

#pragma once

#include "mocks/include/registry.h"

#include <system_error>

namespace mocks { namespace mfx
{
    inline
    std::unique_ptr<registry> dispatch_to(wchar_t const* path, unsigned vendor, unsigned device, unsigned merit)
    {
        auto sub_key = L"Software\\Intel\\MediaSDK\\Dispatch";
        auto registry = make_registry(
            std::make_tuple(HKEY_LOCAL_MACHINE, sub_key)
        );

        CRegKey key;
        auto sts = key.Open(HKEY_LOCAL_MACHINE, sub_key, KEY_READ);
        if (sts != ERROR_SUCCESS)
            throw std::system_error(sts, std::system_category());

        mock_registry(*registry,
            std::make_tuple(HKEY(key), L"VendorID", vendor, REG_DWORD),
            std::make_tuple(HKEY(key), L"DeviceID", device, REG_DWORD),
            std::make_tuple(HKEY(key), L"Merit", merit, REG_DWORD), //(MFX_MAX_MERIT - 1) ?
            std::make_tuple(HKEY(key), L"Path", path, REG_SZ)
        );

        return registry;
    }

    inline
    std::unique_ptr<registry> dispatch_to(unsigned vendor, unsigned device, unsigned merit)
    {
        WCHAR name[1024];
        if (!GetModuleFileNameW(NULL, name, std::extent<decltype(name)>::value))
            throw std::system_error(GetLastError(), std::system_category());

        std::wstring path(name);
        auto idx = path.find_last_of(L"\\");
        if (idx == path.npos)
            throw std::system_error(ERROR_BAD_PATHNAME, std::system_category());
        
        path.erase(path.begin() + idx + 1, path.end()); //+1 for backslash

        auto const lib =
#if !defined(_DEBUG)
            L"libmfxhw64.dll"
#else
            L"libmfxhw64_d.dll"
#endif
        ;
        path += lib;

        return
            dispatch_to(path.c_str(), vendor, device, merit);
    }

} }
