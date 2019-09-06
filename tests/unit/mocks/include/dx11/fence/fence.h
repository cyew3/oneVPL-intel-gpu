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

#include "mocks/include/dx11/winapi/fence.hxx"

namespace mocks { namespace dx11
{
    struct fence
        : winapi::fence_impl
    {
        MOCK_METHOD4(CreateSharedHandle, HRESULT(SECURITY_ATTRIBUTES const*, DWORD, LPCWSTR, HANDLE*));
        MOCK_METHOD0(GetCompletedValue, UINT64());
        MOCK_METHOD2(SetEventOnCompletion, HRESULT(UINT64, HANDLE));
    };

    namespace detail
    {
        inline
        void mock_fence(fence& f, std::tuple<SECURITY_ATTRIBUTES, LPCWSTR> params)
        {
            //HRESULT CreateSharedHandle(const SECURITY_ATTRIBUTES*, DWORD /*dwAccess*/, LPCWSTR /*lpName*/, HANDLE*)
            EXPECT_CALL(f, CreateSharedHandle(
                testing::AnyOf(
                    testing::IsNull(),
                    testing::AllOf(
                        testing::NotNull(),
                        testing::Truly([params](SECURITY_ATTRIBUTES const* xa) { return equal(*xa, std::get<0>(params)); })
                    )
                ),
                testing::Eq(DWORD(GENERIC_ALL)),
                testing::AnyOf(
                    testing::AllOf(testing::NotNull(), testing::StrEq(std::get<1>(params))),
                    testing::IsNull()
                ),
                testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<3>(HANDLE(&f)),
                    testing::Return(S_OK)
                ));
        }

        template <typename T, typename U>
        inline
        typename std::enable_if<std::conjunction<
            std::is_integral<T>,
            std::is_convertible<U, HANDLE>
        >::value>::type
        mock_fence(fence& f, std::tuple<T, U> params)
        {
            //HRESULT SetEventOnCompletion(UINT64 /*Value*/, HANDLE /*hEvent*/)
            EXPECT_CALL(f, SetEventOnCompletion(
                testing::Eq(UINT64(std::get<0>(params))),
                testing::Truly(
                    [event = std::get<1>(params)](HANDLE handle) { return !event || event == handle; }
                )))
                .WillRepeatedly(testing::Return(S_OK));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_fence(fence& f, T value)
        {
            EXPECT_CALL(f, GetCompletedValue())
                .WillRepeatedly(testing::Return(UINT64(value)));
        }

        template <typename T>
        inline
        typename std::enable_if<
            !std::is_integral<T>::value
        >::type
        mock_fence(fence&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_fence]"
            );
        }
    }

    template <typename ...Args>
    inline
    fence& mock_fence(fence& f, Args&&... args)
    {
        for_each(
            [&f](auto&& x) { detail::mock_fence(f, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return f;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<fence>
    make_fence(Args&&... args)
    {
        using testing::_;

        auto f = std::unique_ptr<testing::NiceMock<fence> >{
            new testing::NiceMock<fence>{}
        };

        using testing::_;

        //HRESULT CreateSharedHandle(const SECURITY_ATTRIBUTES*, DWORD /*dwAccess*/, LPCWSTR /*lpName*/, HANDLE*)
        EXPECT_CALL(*f, CreateSharedHandle(_, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT (UINT64 /*Value*/, HANDLE /*hEvent*/)
        EXPECT_CALL(*f, SetEventOnCompletion(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        mock_fence(*f, std::forward<Args>(args)...);

        return f;
    }

} }
