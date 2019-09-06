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

#include "mocks/include/dxgi/winapi/device.hxx"

#include "mocks/include/dxgi/device/adapter.h"
#include "mocks/include/dxgi/resource/surface.h"

namespace mocks { namespace dxgi
{
    struct device
        : winapi::device_impl
    {
        MOCK_METHOD1(GetAdapter, HRESULT(IDXGIAdapter**));
        MOCK_METHOD5(CreateSurface, HRESULT(const DXGI_SURFACE_DESC*, UINT, DXGI_USAGE, const DXGI_SHARED_RESOURCE*, IDXGISurface**));
    };

    namespace detail
    {
        template <typename T>
        inline
        typename std::enable_if<
            std::is_base_of<IDXGIAdapter, T>::value
        >::type mock_device(device& d, T* adapter)
        {
            //HRESULT GetAdapter(IDXGIAdapter**)
            EXPECT_CALL(d, GetAdapter(testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                    [adapter](IDXGIAdapter** result)
                    {
                        return
                            adapter->QueryInterface(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(result));
                    }
                )));
        }

        /* Mocks device w/ capability to create surface from given DXGI_SURFACE_DESC & provided functor [F]
           see details for dxgi::mock_surface(surface&, std::tuple<DXGI_SURFACE_DESC, F>&&) */
        template <typename T, typename F>
        inline
        typename std::enable_if<std::is_integral<T>::value>::type
        mock_device(device& d, std::tuple<T, DXGI_SURFACE_DESC, F> params)
        {
            using testing::_;

            //HRESULT CreateSurface(const DXGI_SURFACE_DESC*, UINT, DXGI_USAGE, const DXGI_SHARED_RESOURCE*, IDXGISurface**)
            EXPECT_CALL(d, CreateSurface(
                testing::AllOf(testing::NotNull(), testing::Truly([sd = std::get<1>(params)](DXGI_SURFACE_DESC const* xd) { return equal(*xd, sd); })),
                testing::AllOf(testing::Gt(0U), testing::Le(UINT(std::get<0>(params)))),
                testing::AllOf(
                    testing::Ge(DXGI_USAGE_SHADER_INPUT),
                    testing::Le(DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT |
                                DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_SHARED | DXGI_USAGE_READ_ONLY |
                                DXGI_USAGE_DISCARD_ON_PRESENT | DXGI_USAGE_UNORDERED_ACCESS)
                ),
                testing::IsNull(),
                testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<4>(testing::Invoke(
                    [params = std::make_tuple(std::get<1>(params), std::get<2>(params))](IDXGISurface** s)
                    { *s = make_surface(std::move(params)).release(); return S_OK; }
                )));
        }

        /* Mocks device w/ capability to create surface from given DXGI_SURFACE_DESC & provided functor
           see details for dxgi::mock_surface(surface&, std::tuple<DXGI_SURFACE_DESC, F>&&) */
        template <typename T, typename U>
        inline
        typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value>::type
        mock_device(surface& f, std::tuple<T, DXGI_SURFACE_DESC, U, void*> params)
        {
            return mock_device(f, std::make_tuple(std::get<0>(params),
                [params](UINT) { return std::make_tuple(std::get<1>(params), std::get<2>(params)); })
            );
        }

        template <typename T>
        inline
        typename std::enable_if<std::is_integral<T>::value>::type
        mock_device(device& d, std::tuple<T, DXGI_SURFACE_DESC> params)
        {
            return mock_device(d, std::make_tuple(std::get<0>(params), std::get<1>(params),
                [](UINT) { return std::make_tuple(0, nullptr); }
            ));
        }

        inline
        void mock_device(device& d, DXGI_SURFACE_DESC sd)
        {
            mock_device(d, std::make_tuple(UINT_MAX, sd,
                [](UINT) { return std::make_tuple(0, nullptr); }
            ));
        }

        template <typename T>
        inline
        void mock_device(device& d, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_device]"
            );
        }
    }

    template <typename ...Args>
    inline
    device& mock_device(device& d, Args&&... args)
    {
        for_each(
            [&d](auto&& x) { detail::mock_device(d, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return d;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<device>
    make_device(Args&&... args)
    {
        auto d = std::unique_ptr<testing::NiceMock<device> >{
            new testing::NiceMock<device>{}
        };

        using testing::_;

        //HRESULT GetAdapter(IDXGIAdapter**)
        EXPECT_CALL(*d, GetAdapter(_))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        EXPECT_CALL(*d, CreateSurface(_, _, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        mock_device(*d, std::forward<Args>(args)...);

        return d;
    }

} }
