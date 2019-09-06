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

#include "mocks/include/dxgi/winapi/factory.hxx"
#include "mocks/include/dxgi/device/adapter.h"

#include <array>
#include <vector>

#include <atlbase.h>

namespace mocks { namespace dxgi
{
    struct factory
        : winapi::factory_impl
    {
        std::vector<CComPtr<IDXGIAdapter> > adapters;

        MOCK_METHOD2(CreateFactory, HRESULT(REFIID, void**));
        MOCK_METHOD2(EnumAdapters, HRESULT(UINT, IDXGIAdapter**));
        MOCK_METHOD2(EnumAdapters1, HRESULT(UINT, IDXGIAdapter1**));

        void detour()
        {
            if (thunk)
                return;

            std::array<std::pair<void*, void*>, 2> functions{
                std::make_pair(CreateDXGIFactory,  CreateFactoryT),
                std::make_pair(CreateDXGIFactory1, CreateFactoryT)
            };

            thunk.reset(
                new hook<factory>(this, std::begin(functions), std::end(functions))
            );
        }

    private:

        std::shared_ptr<hook<factory> >     thunk;

        static
        HRESULT CreateFactoryT(REFIID id, void** result)
        {
            auto instance = hook<factory>::owner();
            assert(instance && "No factory instance but \'CreateDXGIFactory\' is hooked");

            return
                instance ? instance->CreateFactory(id, result) : E_FAIL;
        }
    };

    namespace detail
    {
        inline
        void mock_factory(factory& f, REFIID id)
        {
            using testing::_;

            f.detour();

            //HRESULT CreateFactory(REFIID, void**)
            EXPECT_CALL(f, CreateFactory(testing::Eq(id), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1>(testing::Invoke(
                    [&f](void** result)
                    {
                        f.AddRef();
                        *result = &f;

                        return S_OK;
                    }
                )));
        }

        template <typename T, typename ...Guids>
        inline
        typename std::enable_if<std::disjunction<
            std::is_same<T, DXGI_ADAPTER_DESC >,
            std::is_same<T, DXGI_ADAPTER_DESC1>,
            std::conjunction<std::is_same<Guids, GUID>...>
        >::value>::type
        mock_factory(factory& f, std::tuple<T, LARGE_INTEGER, std::tuple<Guids...> > params)
        {
            //HRESULT EnumAdapters(UINT, IDXGIAdapter**)
            EXPECT_CALL(f, EnumAdapters(testing::Eq(f.adapters.size()), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                    [params, &f](UINT n, IDXGIAdapter** a)
                    {
                        assert(n < f.adapters.size());
                        return f.adapters[n].CopyTo(a);
                    }
                )));

            CComPtr<IDXGIAdapter> adapter;
            adapter.Attach(make_adapter(params).release());
            f.adapters.push_back(adapter);
        }

        template <typename ...Guids>
        inline
        typename std::enable_if<
            std::conjunction<std::is_same<Guids, GUID>...>::value
        >::type
        mock_factory(factory& f, std::tuple<LARGE_INTEGER, std::tuple<Guids...> > id)
        {
            return mock_factory(f,
                std::tuple_cat(std::make_tuple(DXGI_ADAPTER_DESC{}), id)
            );
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::disjunction<std::is_same<T, DXGI_ADAPTER_DESC>, std::is_same<T, DXGI_ADAPTER_DESC1> >::value
        >::type
        mock_factory(factory& f, T ad)
        {
            return mock_factory(f,
                std::make_tuple(ad, LARGE_INTEGER{}, std::make_tuple(GUID_NULL))
            );
        }

        /* Mock from w/ given 'DeviceId'*/
        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_factory(factory& f, T id)
        {
            return mock_factory(f,
                DXGI_ADAPTER_DESC{ L"", 0, UINT(id) }
            );
        }

        inline
        void mock_factory(factory& f, LUID id)
        {
            DXGI_ADAPTER_DESC d{};
            d.AdapterLuid = id;
            return mock_factory(f, d);
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_factory(factory& f, std::tuple<WCHAR, T, T> id)
        {
            return mock_factory(f,
                DXGI_ADAPTER_DESC{ std::get<0>(id), UINT(std::get<1>(id)), UINT(std::get<2>(id)) }
            );
        }

        template <typename T>
        inline
        typename std::enable_if<!std::disjunction<
            std::is_integral<T>,
            std::is_same<T, DXGI_ADAPTER_DESC>,
            std::is_same<T, DXGI_ADAPTER_DESC1>
        >::value>::type
        mock_factory(factory&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_factory]"
            );
        }
    }

    template <typename ...Args>
    inline
    factory& mock_factory(factory& f, Args&&... args)
    {
        for_each(
            [&f](auto&& x) { detail::mock_factory(f, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return f;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<factory>
    make_factory(Args&&... args)
    {
        auto f = std::unique_ptr<testing::NiceMock<factory> >{
            new testing::NiceMock<factory>{}
        };

        using testing::_;

        EXPECT_CALL(*f, CreateFactory(_, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        EXPECT_CALL(*f, EnumAdapters(_, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        mock_factory(*f, std::forward<Args>(args)...);

        ON_CALL(*f, EnumAdapters1(_, _))
            .WillByDefault(testing::Invoke(
            [f = f.get()](UINT n, IDXGIAdapter1** a1)
            {
                CComPtr<IDXGIAdapter> a;
                auto hr = f->EnumAdapters(n, &a);
                return FAILED(hr) ? hr : (*a1 = reinterpret_cast<IDXGIAdapter1*>(a.Detach()), S_OK);
            }
            ));

        return f;
    }

} }
