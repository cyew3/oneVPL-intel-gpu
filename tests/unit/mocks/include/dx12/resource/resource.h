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

#include "mocks/include/dx12/winapi/resource.hxx"

namespace mocks { namespace dx12
{
    struct resource
        : winapi::resource_impl
    {
        MOCK_METHOD0(GetDesc, D3D12_RESOURCE_DESC());
    };

    namespace detail
    {
        inline
        void mock_resource(resource& r, D3D12_RESOURCE_DESC rd)
        {
            //D3D12_RESOURCE_DESC GetDesc()
            EXPECT_CALL(r, GetDesc())
                .WillRepeatedly(testing::Return(rd));
        }

        template<typename T>
        inline
        void mock_resource(resource&, T)
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

        mock_resource(*r, std::forward<Args>(args)...);

        return r;
    }

} }
