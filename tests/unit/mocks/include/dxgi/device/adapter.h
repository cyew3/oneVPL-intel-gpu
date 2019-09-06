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

#include "mocks/include/dxgi/winapi/adapter.hxx"

namespace mocks { namespace dxgi
{
    struct adapter
        : winapi::adapter_impl
    {
        MOCK_METHOD1(GetDesc, HRESULT(DXGI_ADAPTER_DESC*));
        MOCK_METHOD2(CheckInterfaceSupport, HRESULT(REFGUID, LARGE_INTEGER*));
        MOCK_METHOD1(GetDesc1, HRESULT(DXGI_ADAPTER_DESC1*));
    };

    namespace detail
    {
        inline
        void mock_adapter_impl(adapter& a, DXGI_ADAPTER_DESC const& ad)
        {
            //HRESULT GetDesc(DXGI_ADAPTER_DESC*)
            EXPECT_CALL(a, GetDesc(testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(ad),
                    testing::Return(S_OK)
                ));
        }

        inline
        void mock_adapter_impl(adapter& a, DXGI_ADAPTER_DESC1 const& ad)
        {
            //HRESULT GetDesc1(DXGI_ADAPTER_DESC1*)
            EXPECT_CALL(a, GetDesc1(testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(ad),
                    testing::Return(S_OK)
                ));
        }

        template <typename T, typename ...Guids>
        inline
        typename std::enable_if<std::disjunction<
            std::is_same<T, DXGI_ADAPTER_DESC >,
            std::is_same<T, DXGI_ADAPTER_DESC1>,
            std::conjunction<std::is_same<Guids, GUID>...>
        >::value>::type
        mock_adapter(adapter& a, std::tuple<T, LARGE_INTEGER, std::tuple<Guids...> > params)
        {
            mock_adapter_impl(a, std::get<0>(params));

            std::vector<GUID> ids; ids.reserve(sizeof ...(Guids));
            for_each_tuple([&ids](auto x) { ids.push_back(x); }, std::get<2>(params) );

            //HRESULT CheckInterfaceSupport(REFGUID, LARGE_INTEGER*)
            EXPECT_CALL(a, CheckInterfaceSupport(
                testing::Truly([ids](REFGUID id) //TODO: replace 'Truly(<pred>)' w/ 'testing::AnyOfArray(ids)' when GMock is upgraded
                { return std::find(std::begin(ids), std::end(ids), id) != std::end(ids); }),
                testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<1>(std::get<1>(params)),
                    testing::Return(S_OK)
                ));
        }

        inline
        void mock_adapter(adapter& a, DXGI_ADAPTER_DESC ad)
        {
            return mock_adapter(a,
                std::make_tuple(ad, LARGE_INTEGER{}, std::make_tuple(GUID_NULL))
            );
        }

        inline
        void mock_adapter(adapter& a, DXGI_ADAPTER_DESC1 ad)
        {
            return mock_adapter(a,
                std::make_tuple(ad, LARGE_INTEGER{}, std::make_tuple(GUID_NULL))
            );
        }

        template <typename ...Guids>
        inline
        typename std::enable_if<
            std::conjunction<std::is_same<Guids, GUID>...>::value
        >::type
        mock_adapter(adapter& a, std::tuple<LARGE_INTEGER, std::tuple<Guids...> > id)
        {
            return mock_adapter(a,
                std::tuple_cat(std::make_tuple(DXGI_ADAPTER_DESC{}), id)
            );
        }

        inline
        void mock_adapter(adapter& a, GUID id)
        {
            return mock_adapter(a,
                std::make_tuple(LARGE_INTEGER{}, std::make_tuple(id))
            );
        }

        template <typename T>
        inline
        void mock_adapter(adapter&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_adapter]"
            );
        }
    }

    template <typename ...Args>
    inline
    adapter& mock_adapter(adapter& a, Args&&... args)
    {
        for_each(
            [&a](auto&& x) { detail::mock_adapter(a, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return a;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<adapter>
    make_adapter(Args&&... args)
    {
        auto a = std::unique_ptr<testing::NiceMock<adapter> >{
            new testing::NiceMock<adapter>{}
        };

        using testing::_;

        EXPECT_CALL(*a, CheckInterfaceSupport(_, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        EXPECT_CALL(*a, GetDesc(_))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        EXPECT_CALL(*a, GetDesc1(_))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        mock_adapter(*a, std::forward<Args>(args)...);

        return a;
    }

} }
