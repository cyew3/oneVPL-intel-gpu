// Copyright (c) 2019-2020 Intel Corporation
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

#include "mocks/include/dx11/device/base.h"
#include "mocks/include/dx11/context/base.h"

#include "mocks/include/dxgi/device/device.h"
#include "mocks/include/dxgi/device/adapter.h"

namespace mocks { namespace dx11
{
    namespace detail
    {
        template <typename Adapter>
        inline
        typename std::enable_if<
            std::is_base_of<IDXGIAdapter, Adapter>::value
        >::type
        mock_device(device& d, Adapter* adapter)
        {
            //need to hook before we handle NULL (default) adapter
            d.detour(adapter);

            if (adapter)
                d.xadapter = adapter;

            if (!d.xadapter)
                //create default adapter and take ownership here
                d.xadapter.Attach(dxgi::make_adapter().release());

            //check if we already mocked IDXGIDevice
            CComPtr<IDXGIDevice> g;
            if (SUCCEEDED(d.QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&g))))
                //also mock aggregated IDXGIDevice
                mock_device(*static_cast<dxgi::device*>(g.p), d.xadapter.p);
        }

        template <typename Device>
        inline
        typename std::enable_if<
            std::is_base_of<IDXGIDevice, Device>::value
        >::type
        mock_device(device& d, Device* x)
        {
            d.xdevice = x;

            //mimic that we're DXGI device too
            //NOTE: we violate COM rules here since our DXGI devive is not
            //really aggregated object
            mock_unknown(d, d.xdevice.p,
                __uuidof(IDXGIDevice),
                __uuidof(IDXGIDevice1),
                __uuidof(IDXGIDevice2),
                __uuidof(IDXGIDevice3),
                __uuidof(IDXGIDevice4)
            );
        }

        template <typename Context>
        inline
        typename std::enable_if<
            std::is_base_of<ID3D11DeviceContext, Context>::value
        >::type
        mock_device(device& d, Context* context)
        {
            if (context)
                d.context = context;

            if (!d.context)
                //create default context and take ownership here
                d.context.Attach(static_cast<ID3D11DeviceContext*>(make_context().release()));
        }

        template <typename Adapter, typename Context, typename T, typename ...Levels>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_base_of<IDXGIAdapter, Adapter>,
                std::is_integral<T>,
                std::is_base_of<ID3D11DeviceContext, Context>,
                std::is_same<Levels, D3D_FEATURE_LEVEL>...
            >::value
        >::type
        mock_device(device& d, std::tuple<Adapter*, D3D_DRIVER_TYPE, HMODULE, T /*flags*/, T /*version*/, std::tuple<Levels...>, Context*> params)
        {
            mock_device(d, std::get<0>(params));
            mock_device(d, std::get<6>(params));

            std::vector<D3D_FEATURE_LEVEL> levels; levels.reserve(sizeof ...(Levels));
            for_each_tuple([&levels](auto x) { levels.push_back(x); }, std::get<5>(params) );

            using testing::_;

            //HRESULT D3D11CreateDevice(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT flags,
            //        D3D_FEATURE_LEVEL const*, UINT count, UINT version,
            //        ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**)
            EXPECT_CALL(d, CreateDevice(
                testing::AnyOf(
                    testing::IsNull(),
                    testing::Eq(std::get<0>(params))
                ),
                testing::Eq(std::get<1>(params)),
                testing::Eq(std::get<2>(params)), testing::Eq(UINT(std::get<3>(params))),
                testing::NotNull(), _, testing::Eq(UINT(std::get<4>(params))), testing::NotNull(), _, _))
                .With(testing::Args<4, 5>(testing::Contains(
                    testing::Truly([levels](D3D_FEATURE_LEVEL level) //TODO: replace 'Truly(<pred>)' w/ 'testing::AnyOfArray(levels)' when GMock is upgraded
                    { return std::find(std::begin(levels), std::end(levels), level) != std::end(levels); }))))
                .WillRepeatedly(testing::WithArgs<7, 8, 9>(testing::Invoke(
                    [&d, levels](ID3D11Device** result, D3D_FEATURE_LEVEL* level, ID3D11DeviceContext** ctx)
                    {
                        d.AddRef();
                        *result = &d;

                        if (level)
                            *level = *std::max_element(std::begin(levels), std::end(levels));

                        if (ctx)
                            d.GetImmediateContext(ctx);

                        return S_OK;
                    }
                )));
        }

        template <typename Adapter, typename Context, typename ...Levels>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_base_of<IDXGIAdapter, Adapter>,
                std::is_base_of<ID3D11DeviceContext, Context>,
                std::is_same<Levels, D3D_FEATURE_LEVEL>...
            >::value
        >::type
        mock_device(device& d, std::tuple<Adapter*, D3D_DRIVER_TYPE, std::tuple<Levels...>, Context*> params)
        {
            return mock_device(d,
                std::make_tuple(std::get<0>(params), std::get<1>(params), HMODULE(0), 0, D3D11_SDK_VERSION, std::get<2>(params), std::get<3>(params))
            );
        }

        template <typename Adapter, typename ...Levels>
        inline
        typename std::enable_if<
            std::conjunction<std::is_base_of<IDXGIAdapter, Adapter>, std::is_same<Levels, D3D_FEATURE_LEVEL>...>::value
        >::type
        mock_device(device& d, std::tuple<Adapter*, D3D_DRIVER_TYPE, std::tuple<Levels...> > params)
        {
            return mock_device(d,
                std::tuple_cat(params, std::make_tuple(static_cast<ID3D11DeviceContext*>(nullptr)))
            );
        }

        template <typename Adapter, typename ...Levels>
        inline
        typename std::enable_if<
            std::conjunction<std::is_base_of<IDXGIAdapter, Adapter>, std::is_same<Levels, D3D_FEATURE_LEVEL>...>::value
        >::type
        mock_device(device& d, std::tuple<Adapter*, std::tuple<Levels...> > params)
        {
            return mock_device(d,
                std::make_tuple(std::get<0>(params), D3D_DRIVER_TYPE_HARDWARE, std::get<1>(params))
            );
        }

        template <typename ...Levels>
        inline
        typename std::enable_if<
            std::conjunction<std::is_same<Levels, D3D_FEATURE_LEVEL>...>::value
        >::type
        mock_device(device& d, std::tuple<Levels...> params)
        {
            return mock_device(d,
                std::make_tuple(static_cast<IDXGIAdapter*>(nullptr), params)
            );
        }

        template <typename T, typename U>
        inline
        typename std::enable_if<
            std::conjunction<std::is_integral<T>, std::disjunction<std::is_integral<U>, std::is_same<U, D3D11_FENCE_FLAG> > >::value
        >::type
        mock_device(device& d, std::tuple<T, U, ID3D11Fence*> params)
        {
            //HRESULT CreateFence(UINT64 InitialValue, D3D11_FENCE_FLAG Flags, REFIID, void**)
            EXPECT_CALL(d, CreateFence(testing::Eq(UINT64(std::get<0>(params))), testing::Eq(std::get<1>(params)),
                testing::_, testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
                    [f = std::get<2>(params)](REFIID id, void** result)
                    {
                        return
                            f ? f->QueryInterface(id, result) : E_FAIL;
                    }
                )));
        }
    }

    inline
    std::unique_ptr<device>
    make_device()
    {
        return
            make_device(static_cast<IDXGIAdapter*>(nullptr));
    }

} }
