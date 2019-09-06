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

#include "mocks/include/dx11/winapi/buffer.hxx"

namespace mocks { namespace dx11
{
    struct buffer
        : winapi::buffer_impl
    {
        MOCK_METHOD1(GetDesc, void(D3D11_BUFFER_DESC*));
    };

    namespace detail
    {
        inline
        void mock_buffer(buffer& b, D3D11_BUFFER_DESC bd)
        {
            //void GetDesc(D3D11_BUFFER_DESC*)
            EXPECT_CALL(b, GetDesc(testing::NotNull()))
                .WillRepeatedly(testing::SetArgPointee<0>(bd))
                ;
        }

        template <typename T>
        inline
        void mock_buffer(buffer&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_buffer]"
            );
        }
    }

    template <typename ...Args>
    inline
    buffer& mock_buffer(buffer& b, Args&&... args)
    {
        for_each(
            [&b](auto&& x) { detail::mock_buffer(b, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return b;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<buffer>
    make_buffer(Args&&... args)
    {
        using testing::_;

        auto b = std::unique_ptr<testing::NiceMock<buffer> >{
            new testing::NiceMock<buffer>{}
        };

        mock_buffer(*b, std::forward<Args>(args)...);

        return b;
    }

} }
