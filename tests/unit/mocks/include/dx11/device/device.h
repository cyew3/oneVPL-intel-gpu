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

#include "mocks/include/dx11/resource/buffer.h"
#include "mocks/include/dx11/resource/texture.h"

#include "mocks/include/dx11/device/base.h"
#include "mocks/include/dx11/context/base.h"

#include "mocks/include/dxgi/device/device.h"
#include "mocks/include/dxgi/device/adapter.h"

namespace mocks { namespace dx11
{
    namespace detail
    {
        template <typename T>
        inline
        typename std::enable_if<
            std::is_base_of<IDXGIAdapter, T>::value
        >::type
        mock_device(device& d, T* adapter)
        {
            //need to hook before we handle NULL (default) adapter
            d.detour(adapter);

            d.adapter = adapter;
            if (!d.adapter)
                //create default adapter and take ownership here
                d.adapter.Attach(dxgi::make_adapter().release());

            //check if we already mocked IDXGIDevice
            CComPtr<IDXGIDevice> g;
            if (SUCCEEDED(d.QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&g))))
                //also mock aggregated IDXGIDevice
                mock_device(*static_cast<dxgi::device*>(g.p), d.adapter.p);
        }

        inline
        void mock_device(device& d, dxgi::device& g)
        {
            mock_composite(g, &d);
            mock_unknown(d, &g, __uuidof(IDXGIDevice));
        }

        inline
        void mock_device(device& d, ID3D11DeviceContext* context)
        {
            d.context = context;
            if (!d.context)
                //create default context and take ownership here
                d.context.Attach(static_cast<ID3D11DeviceContext*>(make_context().release()));
        }

        template <typename T, typename ...Levels>
        inline
        typename std::enable_if<
            std::conjunction<std::is_integral<T>, std::is_same<Levels, D3D_FEATURE_LEVEL>...>::value
        >::type
        mock_device(device& d, std::tuple<IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, T /*flags*/, T /*version*/, std::tuple<Levels...>, ID3D11DeviceContext*> params)
        {
            mock_device(d, std::get<0>(params));
            mock_device(d, std::get<6>(params));

            std::vector<D3D_FEATURE_LEVEL> levels; levels.reserve(sizeof ...(Levels));
            for_each_tuple([&levels](auto x) { levels.push_back(x); }, std::get<5>(params) );

            using testing::_;

            //HRESULT(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT flags,
            //        D3D_FEATURE_LEVEL const*, UINT count, UINT version,
            //        ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**)
            EXPECT_CALL(d, CreateDevice(testing::Eq(std::get<0>(params)), testing::Eq(std::get<1>(params)),
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

        template <typename ...Levels>
        inline
        typename std::enable_if<
            std::conjunction<std::is_same<Levels, D3D_FEATURE_LEVEL>...>::value
        >::type
        mock_device(device& d, std::tuple<IDXGIAdapter*, D3D_DRIVER_TYPE, std::tuple<Levels...>, ID3D11DeviceContext*> params)
        {
            return mock_device(d,
                std::make_tuple(std::get<0>(params), std::get<1>(params), HMODULE(0), 0, D3D11_SDK_VERSION, std::get<2>(params), std::get<3>(params))
            );
        }

        template <typename ...Levels>
        inline
        typename std::enable_if<
            std::conjunction<std::is_same<Levels, D3D_FEATURE_LEVEL>...>::value
        >::type
        mock_device(device& d, std::tuple<IDXGIAdapter*, D3D_DRIVER_TYPE, std::tuple<Levels...> > params)
        {
            return mock_device(d,
                std::tuple_cat(params, std::make_tuple(static_cast<ID3D11DeviceContext*>(nullptr)))
            );
        }

        template <typename ...Levels>
        inline
        typename std::enable_if<
            std::conjunction<std::is_same<Levels, D3D_FEATURE_LEVEL>...>::value
        >::type
        mock_device(device& d, std::tuple<IDXGIAdapter*, std::tuple<Levels...> > params)
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

        inline
        void mock_device(device& d, D3D11_TEXTURE2D_DESC td)
        {
            using testing::_;

            //HRESULT CreateTexture2D(const D3D11_TEXTURE2D_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture2D**));
            EXPECT_CALL(d, CreateTexture2D(
                testing::AllOf(
                    testing::NotNull(),
                    testing::Truly([td](D3D11_TEXTURE2D_DESC const* xd) { return equal(*xd, td); })
                ), _, testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 2>(testing::Invoke(
                    [](D3D11_TEXTURE2D_DESC const* xd, ID3D11Texture2D** t)
                    { *t = make_texture(*xd).release(); return S_OK; }
                )));
        }

        inline
        void mock_device(device& d, D3D11_BUFFER_DESC bd)
        {
            using testing::_;

            EXPECT_CALL(d, CreateBuffer(_, _, _))
                .WillRepeatedly(testing::Return(E_FAIL));

            //HRESULT CreateBuffer(const D3D11_BUFFER_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Buffer**)
            EXPECT_CALL(d, CreateBuffer(testing::AllOf(
                testing::NotNull(),
                testing::Truly([bd](D3D11_BUFFER_DESC const* xd) { return equal(*xd, bd); })
                ), _, testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 2>(testing::Invoke(
                    [](D3D11_BUFFER_DESC const* xd, ID3D11Buffer** b)
                    { *b = make_buffer(*xd).release(); return S_OK; }
                )));
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
