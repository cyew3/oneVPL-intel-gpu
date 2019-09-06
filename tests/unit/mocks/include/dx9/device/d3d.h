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

#include "mocks/include/hook.h"

#include "mocks/include/dx9/winapi/d3d.hxx"

#include <array>

namespace mocks { namespace dx9
{
    struct d3d
        : winapi::d3d_impl
    {
        std::vector<D3DDEVTYPE> adapters;

        MOCK_METHOD1(CreateD3D, IDirect3D9*(UINT));

        MOCK_METHOD0(GetAdapterCount, UINT());
        MOCK_METHOD3(GetAdapterIdentifier, HRESULT(UINT, DWORD, D3DADAPTER_IDENTIFIER9*));
        MOCK_METHOD2(GetAdapterLUID, HRESULT(UINT, LUID*));
        MOCK_METHOD4(CheckDeviceFormatConversion, HRESULT(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT));

        void detour()
        {
            if (thunk)
                return;

            std::array<std::pair<void*, void*>, 2> functions{
                std::make_pair(Direct3DCreate9,   CreateDirect3D9),
                std::make_pair(Direct3DCreate9Ex, CreateDirect3D9Ex)
            };

            thunk.reset(
                new hook<d3d>(this, std::begin(functions), std::end(functions))
            );
        }

    private:

        std::shared_ptr<hook<d3d> >     thunk;

        static
        IDirect3D9* CreateDirect3D9(UINT version)
        {
            auto instance = hook<d3d>::owner();
            assert(instance && "No factory instance but \'Direct3DCreate9\' is hooked");

            return
                instance ? instance->CreateD3D(version) : nullptr;
        }

        static
        HRESULT CreateDirect3D9Ex(UINT version, IDirect3D9Ex** result)
        {
            auto d3d = CreateDirect3D9(version);
            return d3d->QueryInterface(__uuidof(IDirect3D9Ex), reinterpret_cast<void**>(result));
        }
    };

    namespace detail
    {
        inline
        void mock_d3d(d3d& d, D3DDEVTYPE type)
        {
            d.adapters.push_back(type);
        }

        inline
        void mock_d3d(d3d& d, std::tuple<D3DDEVTYPE, D3DADAPTER_IDENTIFIER9, LUID> params)
        {
            mock_d3d(d, std::get<0>(params));

            using testing::_;

            //HRESULT GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9*)
            EXPECT_CALL(d, GetAdapterIdentifier(
                testing::Truly([&d](UINT adapter) { return adapter < UINT(d.adapters.size()); }),
                _,
                testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<2>(std::get<1>(params)),
                    testing::Return(S_OK)
                ));

            //HRESULT GetAdapterLUID(UINT Adapter, LUID*)
            EXPECT_CALL(d, GetAdapterLUID(
                testing::Truly([&d](UINT adapter)
                { return adapter < UINT(d.adapters.size()); }),
                testing::NotNull()))
            .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<1>(std::get<2>(params)),
                    testing::Return(S_OK)
                ));
        }

        inline
        void mock_d3d(d3d& d, std::tuple<D3DADAPTER_IDENTIFIER9, LUID> params)
        {
            return mock_d3d(d, std::tuple_cat(
                std::make_tuple(D3DDEVTYPE_HAL), params
            ));
        }

        inline
        void mock_d3d(d3d& d, D3DADAPTER_IDENTIFIER9 id)
        {
            return mock_d3d(d,
                std::make_tuple(id, LUID{ 1 })
            );
        }

        inline
        void mock_d3d(d3d& d, LUID id)
        {
            return mock_d3d(d,
                std::make_tuple(D3DADAPTER_IDENTIFIER9{}, id)
            );
        }

        inline
        void mock_d3d(d3d& d, std::tuple<D3DDEVTYPE, D3DFORMAT, D3DFORMAT> formats)
        {
            //HRESULT CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat)
            EXPECT_CALL(d, CheckDeviceFormatConversion(
                testing::Truly([&d](UINT adapter) { return adapter < UINT(d.adapters.size()); }),
                testing::_,
                testing::Eq(std::get<1>(formats)),
                testing::Eq(std::get<2>(formats))))
                .With(testing::Args<0, 1>(testing::Truly([&d](std::tuple<UINT, D3DDEVTYPE> t)
                { return d.adapters[std::get<0>(t)] == std::get<1>(t); })))
                .WillRepeatedly(testing::Return(S_OK));
        }

        inline
        void mock_d3d(d3d& d, std::tuple<D3DFORMAT, D3DFORMAT> formats)
        {
            return mock_d3d(d, std::tuple_cat(
                std::make_tuple(D3DDEVTYPE_HAL), formats
            ));
        }

        template <typename T>
        inline
        void mock_d3d(d3d&, T)
        {
            /* If you trap here it means that you passed an argument to [make_d3d]
                which is not supported by library, you need to overload [mock_d3d] for that argument's type */
            static_assert(false,
                "Unknown argument was passed to [d3d]"
            );
        }
    }

    template <typename ...Args>
    inline
    d3d& mock_d3d(d3d& d, Args&&... args)
    {
        for_each(
            [&d](auto&& x) { detail::mock_d3d(d, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return d;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<d3d>
    make_d3d(Args&&... args)
    {
        auto d = std::unique_ptr<testing::NiceMock<d3d> >{
            new testing::NiceMock<d3d>{}
        };

        using testing::_;

        d->detour();

        //IDirect3D9* CreateD3D(UINT version)
        EXPECT_CALL(*d, CreateD3D(testing::Eq(UINT(D3D_SDK_VERSION))))
            .WillRepeatedly(testing::InvokeWithoutArgs(
                [d = d.get()]()
                {
                    d->AddRef();
                    return d;
                }
            ));

        //UINT GetAdapterCount()
        EXPECT_CALL(*d, GetAdapterCount())
            .WillRepeatedly(testing::InvokeWithoutArgs(
                [d = d.get()]() { return UINT(d->adapters.size()); }
            ));

        //HRESULT GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9*)
        EXPECT_CALL(*d, GetAdapterIdentifier(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT GetAdapterLUID(UINT Adapter, LUID*)
        EXPECT_CALL(*d, GetAdapterLUID(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat)
        EXPECT_CALL(*d, CheckDeviceFormatConversion(_, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        mock_d3d(*d, std::forward<Args>(args)...);

        return d;
    }

} }
