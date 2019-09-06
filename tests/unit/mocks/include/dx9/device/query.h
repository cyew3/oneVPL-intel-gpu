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

#include "mocks/include/dx9/winapi/query.hxx"

namespace mocks { namespace dx9
{
    struct query
        : winapi::query_impl
    {
    };

    namespace detail
    {
        inline
        void mock_query(query&, D3DQUERYTYPE)
        {}
        
        template <typename T>
        inline
        void mock_query(query&, T)
        {
            /* If you trap here it means that you passed an argument to [make_query]
                which is not supported by library, you need to overload [mock_query] for that argument's type */
            static_assert(false,
                "Unknown argument was passed to [make_query]"
            );
        }
    }

    template <typename ...Args>
    inline
    query& mock_query(query& q, Args&&... args)
    {
        for_each(
            [&q](auto&& x) { detail::mock_query(q, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return q;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<query>
    make_query(Args&&... args)
    {
        auto q = std::unique_ptr<testing::NiceMock<query> >{
            new testing::NiceMock<query>{}
        };

        mock_query(*q, std::forward<Args>(args)...);

        return q;
    }

} }
