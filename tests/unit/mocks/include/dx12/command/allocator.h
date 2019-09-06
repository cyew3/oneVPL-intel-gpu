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

#include "mocks/include/dx12/winapi/allocator.hxx"

namespace mocks { namespace dx12
{
    struct allocator
        : winapi::allocator_impl
    {
        MOCK_METHOD0(Reset, HRESULT());
    };

    namespace detail
    {
        template <typename T>
        inline
        void mock_allocator(allocator&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_allocator]"
            );
        }
    }

    template <typename ...Args>
    inline
    allocator& mock_allocator(allocator& a, Args&&... args)
    {
        for_each(
            [&a](auto&& x) { detail::allocator(a, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return a;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<allocator>
    make_allocator(Args&&... args)
    {
        auto a = std::unique_ptr<testing::NiceMock<allocator> >{
            new testing::NiceMock<allocator>{}
        };

        //HRESULT Reset()
        EXPECT_CALL(*a, Reset())
            .WillRepeatedly(testing::Return(S_OK));

        mock_allocator(*a, std::forward<Args>(args)...);

        return a;
    }

} }
