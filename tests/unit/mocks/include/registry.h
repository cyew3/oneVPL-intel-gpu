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

#include "mocks/include/common.h"
#include "mocks/include/hook.h"

#include <array>

namespace mocks
{
    struct registry
    {
        MOCK_METHOD5(OpenKey, LSTATUS(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY));
        MOCK_METHOD8(EnumKey, LSTATUS (HKEY, DWORD /*dwIndex*/, LPWSTR /*lpName*/, LPDWORD /*lpcchName*/, LPDWORD /*lpReserved*/, LPWSTR /*lpClass*/, LPDWORD /*lpcchClass*/, PFILETIME));
        MOCK_METHOD6(QueryValue, LSTATUS(HKEY, LPCWSTR /*lpValueName*/, LPDWORD /*lpReserved*/, LPDWORD /*lpType*/, LPBYTE /*lpData*/, LPDWORD /*lpcbData*/));
        MOCK_METHOD1(CloseKey, LSTATUS(HKEY));

        size_t count = 0;

        void detour()
        {
            if (thunk)
                return;

            std::array<std::pair<void*, void*>, 4> functions{
                std::make_pair(RegOpenKeyExW,    OpenKeyT),
                std::make_pair(RegEnumKeyExW,    EnumKeyT),
                std::make_pair(RegQueryValueExW, QueryValueT),
                std::make_pair(RegCloseKey,      CloseKeyT)
            };

            thunk.reset(
                new hook<registry>(this, std::begin(functions), std::end(functions))
            );
        }

    private:

        std::shared_ptr<hook<registry> >     thunk;

        static
        LSTATUS OpenKeyT(HKEY key, LPCWSTR name, DWORD options, REGSAM access, PHKEY result)
        {
            auto instance = hook<registry>::owner();
            assert(instance && "No registry instance but \'RegOpenKeyExW\' is hooked");

            return
                instance ? instance->OpenKey(key, name, options, access, result) : ERROR_BAD_ENVIRONMENT;
        }

        static
        LSTATUS EnumKeyT(HKEY key, DWORD index, LPWSTR name, LPDWORD name_size, LPDWORD reserved, LPWSTR cname, LPDWORD cname_size, PFILETIME time)
        {
            auto instance = hook<registry>::owner();
            assert(instance && "No registry instance but \'RegEnumKeyExW\' is hooked");

            return
                instance ? instance->EnumKey(key, index, name, name_size, reserved, cname, cname_size, time) : ERROR_BAD_ENVIRONMENT;
        }

        static
        LSTATUS QueryValueT(HKEY key, LPCWSTR name, LPDWORD reserved, LPDWORD type, LPBYTE value, LPDWORD size)
        {
            auto instance = hook<registry>::owner();
            assert(instance && "No registry instance but \'RegQueryValueExW\' is hooked");

            return
                instance ? instance->QueryValue(key, name, reserved, type, value, size) : ERROR_BAD_ENVIRONMENT;
        }

        static
        LSTATUS CloseKeyT(HKEY key)
        {
            auto instance = hook<registry>::owner();
            assert(instance && "No registry instance but \'RegCloseKey\' is hooked");

            return
                instance ? instance->CloseKey(key) : ERROR_BAD_ENVIRONMENT;
        }
    };

    namespace detail
    {
        inline
        void mock_registry(registry& r, std::tuple<HKEY, LPCWSTR> params)
        {
            using testing::_;

            //LSTATUS RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
            EXPECT_CALL(r, OpenKey(
                testing::Eq(std::get<0>(params)),
                testing::StrCaseEq(std::get<1>(params)),
                testing::AnyOf(0, REG_OPTION_OPEN_LINK),
                testing::AllOf(testing::Gt(REGSAM(0)), testing::Le(REGSAM(KEY_ALL_ACCESS))),
                testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<4>(HKEY(&r)),
                    testing::Return(ERROR_SUCCESS)
                ));

            //LSTATUS RegCloseKey(HKEY hKey);
            EXPECT_CALL(r, CloseKey(
                testing::Eq(std::get<0>(params))))
                .WillRepeatedly(
                    testing::Return(ERROR_SUCCESS)
                );
        }

        template <typename T, typename U>
        inline
        void mock_registry(registry& r, std::tuple<HKEY, LPCWSTR, T /*value*/, U /*registry value type*/ > params)
        {
            using testing::_;

            //also mock 'OpenKey/CloseKey' for given key & value
            mock_registry(r,
                std::make_tuple(std::get<0>(params), std::get<1>(params))
            );

            //LSTATUS RegEnumKeyExW(HKEY, DWORD /*dwIndex*/, LPWSTR /*lpName*/, LPDWORD /*lpcchName*/, LPDWORD /*lpReserved*/, LPWSTR /*lpClass*/, LPDWORD /*lpcchClass*/, PFILETIME));
            EXPECT_CALL(r, EnumKey(
                testing::Eq(std::get<0>(params)), //key
                testing::Eq(r.count),
                testing::NotNull(), testing::NotNull(), testing::IsNull(),
                _, _, _))
                .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
                    [params](LPWSTR result, LPDWORD length)
                    {
                        auto name = std::get<1>(params);
                        *length = DWORD(wcslen(name));
                        std::copy(name, name + *length, result);
                        result[*length] = L'\0';

                        return ERROR_SUCCESS;
                    }
                )));

            ++r.count;

            //LSTATUS RegQueryValueExW(HKEY, LPCWSTR /*lpValueName*/, LPDWORD /*lpReserved*/, LPDWORD /*lpType*/, LPBYTE /*lpData*/, LPDWORD /*lpcbData*/)
            EXPECT_CALL(r, QueryValue(
                testing::Eq(std::get<0>(params)),       //key
                testing::StrCaseEq(std::get<1>(params)), //name
                testing::IsNull(), _, _, _))
                .WillRepeatedly(testing::WithArgs<3, 4, 5>(testing::Invoke(
                    [params](LPDWORD type, LPBYTE data, LPDWORD size)
                    {
                        if (type) *type = std::get<3>(params);
                        if (!data) return ERROR_SUCCESS;
                        if (!size) return ERROR_INVALID_PARAMETER;

                        CComVariant value(std::get<2>(params));
                        ULARGE_INTEGER sz; value.GetSizeMax(&sz);
                        sz.LowPart -= sizeof(VARTYPE); //'GetSizeMax' includes size of 'VARIANT::vt'
                        
                        LPBYTE dst = nullptr;
                        switch (value.vt)
                        {
                            case VT_BSTR:
                                sz.LowPart -= sizeof(ULONG); //'GetStreamSize' includes also size of 'ULONG'
                                dst = LPBYTE(value.bstrVal);
                                break;

                            default:
                                dst = &value.bVal;
                        }

                        *size = sz.LowPart;
                        std::copy(dst, dst + *size, data);

                        return *size ?
                            ERROR_SUCCESS : ERROR_INVALID_DATA;
                    }
                )));
        }

        template <typename T>
        inline
        void mock_registry(registry&, T)
        {
            /* If you trap here it means that you passed an argument to [make_registry]
                which is not supported by library, you need to overload [mock_registry] for that argument's type */
            static_assert(false,
                "Unknown argument was passed to [make_registry]"
            );
        }
    }

    template <typename ...Args>
    inline
    registry& mock_registry(registry& r, Args&&... args)
    {
        for_each(
            [&r](auto&& x) { detail::mock_registry(r, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return r;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<registry>
    make_registry(Args&&... args)
    {
        auto r = std::unique_ptr<testing::NiceMock<registry> >{
            new testing::NiceMock<registry>{}
        };

        using testing::_;

        r->detour();

        //LSTATUS RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
        EXPECT_CALL(*r, OpenKey(_, _, _, _, _))
            .WillRepeatedly(testing::Return(ERROR_INVALID_PARAMETER));

        //LSTATUS RegEnumKeyExW(HKEY, DWORD /*dwIndex*/, LPWSTR /*lpName*/, LPDWORD /*lpcchName*/, LPDWORD /*lpReserved*/, LPWSTR /*lpClass*/, LPDWORD /*lpcchClass*/, PFILETIME));
        EXPECT_CALL(*r, EnumKey(_, _, _, _, _, _, _, _))
            .WillRepeatedly(testing::Return(ERROR_INVALID_PARAMETER));

        //LSTATUS RegQueryValueExW(HKEY, LPCWSTR /*lpValueName*/, LPDWORD /*lpReserved*/, LPDWORD /*lpType*/, LPBYTE /*lpData*/, LPDWORD /*lpcbData*/)
        EXPECT_CALL(*r, QueryValue(_, _, _, _, _, _))
            .WillRepeatedly(testing::Return(ERROR_INVALID_PARAMETER));

        //LSTATUS RegCloseKey(HKEY hKey);
        EXPECT_CALL(*r, CloseKey(_))
            .WillRepeatedly(testing::Return(ERROR_INVALID_HANDLE));

        mock_registry(*r, std::forward<Args>(args)...);

        return r;
    }

}