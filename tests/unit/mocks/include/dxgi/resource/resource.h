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

#include "mocks/include/dxgi/winapi/resource.hxx"

namespace mocks { namespace dxgi
{
    struct resource
        : winapi::resource_impl
    {
        MOCK_METHOD4(CreateSharedHandle, HRESULT(SECURITY_ATTRIBUTES const*, DWORD /*access*/, LPCWSTR /*name*/, HANDLE*));
    };

    namespace detail
    {
        template <typename T>
        inline
        typename std::enable_if<std::is_integral<T>::value>::type
        mock_resource(resource& r, std::tuple<SECURITY_ATTRIBUTES, T, LPCWSTR> params)
        {
            //HRESULT CreateSharedHandle(const SECURITY_ATTRIBUTES*, DWORD /*dwAccess*/, LPCWSTR /*lpName*/, HANDLE*)
            EXPECT_CALL(r, CreateSharedHandle(
                testing::AnyOf(
                    testing::IsNull(),
                    testing::AllOf(
                        testing::NotNull(),
                        testing::Truly([params](SECURITY_ATTRIBUTES const* xa)
                        { return equal(*xa, std::get<0>(params)); })
                    )
                ),
                testing::Eq(DWORD(std::get<1>(params))),
                testing::AnyOf(
                    testing::AllOf(testing::NotNull(), testing::StrEq(std::get<2>(params))),
                    testing::IsNull()
                ),
                testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<3>(HANDLE(&r)),
                    testing::Return(S_OK)
                ));
        }

        template <typename T>
        inline
        typename std::enable_if<!std::is_integral<T>::value>::type
        mock_resource(resource&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_resource]"
            );
        }
    }

    template <typename ...Args>
    inline
    resource& mock_resource(resource& r, Args&&... args)
    {
        for_each(
            [&r](auto&& x) { detail::mock_resource(r, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return r;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<resource>
    make_resource(Args&&... args)
    {
        auto r = std::unique_ptr<testing::NiceMock<resource> >{
            new testing::NiceMock<resource>{}
        };

        using testing::_;

        //HRESULT CreateSharedHandle(const SECURITY_ATTRIBUTES*, DWORD /*dwAccess*/, LPCWSTR /*lpName*/, HANDLE*)
        EXPECT_CALL(*r, CreateSharedHandle(_, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        mock_resource(*r, std::forward<Args>(args)...);

        return r;
    }

} }
