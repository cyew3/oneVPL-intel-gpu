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

#include "mocks/include/dx12/winapi/fence.hxx"

namespace mocks { namespace dx12
{
    struct fence
        : winapi::fence_impl
    {
        HANDLE event = nullptr;

        MOCK_METHOD0(GetCompletedValue, UINT64());
        MOCK_METHOD2(SetEventOnCompletion, HRESULT(UINT64, HANDLE));
        MOCK_METHOD1(Signal, HRESULT(UINT64));
    };

    namespace detail
    {
        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_fence(fence& f, T value)
        {
            //HRESULT Signal(UINT64 /*Value*/)
            EXPECT_CALL(f, Signal(testing::Eq(UINT64(value))))
                .WillRepeatedly(testing::InvokeWithoutArgs(
                    [&f]() { return !f.event || SetEvent(f.event) ? S_OK : E_FAIL; }
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
            mock_fence(f, std::get<0>(params));

            //HRESULT SetEventOnCompletion(UINT64 /*Value*/, HANDLE /*hEvent*/)
            EXPECT_CALL(f, SetEventOnCompletion(
                testing::Eq(UINT64(std::get<0>(params))),
                testing::Truly(
                    [event = std::get<1>(params)](HANDLE handle) { return !event || event == handle; }
                )))
                .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                    [&f, params](UINT64 value, HANDLE event)
                    {
                        f.event = event;
                        return
                            UINT64(std::get<0>(params)) != value || SetEvent(f.event) ? S_OK : E_FAIL;
                    }
                )));

            //HRESULT Signal(UINT64 /*Value*/)
            EXPECT_CALL(f, Signal(testing::Eq(UINT64(std::get<0>(params)))))
                .WillRepeatedly(testing::InvokeWithoutArgs(
                    [&f]() { return !f.event || SetEvent(f.event) ? S_OK : E_FAIL; } //CLARIFY: should Signal return an error if no event is set?
                ));
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

        //HRESULT (UINT64 /*Value*/, HANDLE /*hEvent*/)
        EXPECT_CALL(*f, SetEventOnCompletion(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT Signal(UINT64 /*Value*/)
        EXPECT_CALL(*f, Signal(_))
            .WillRepeatedly(testing::Return(E_FAIL));

        mock_fence(*f, std::forward<Args>(args)...);

        return f;
    }

} }
