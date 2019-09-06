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

#include "mocks/include/dx11/winapi/view.hxx"

namespace mocks { namespace dx11
{
    struct output_view
        : winapi::output_view_impl
    {
        MOCK_METHOD1(GetDesc, void(D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC*));
    };

    namespace detail
    {
        inline
        void mock_view(output_view& v, D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC od)
        {
            //void GetDesc(D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC*)
            EXPECT_CALL(v, GetDesc(testing::NotNull()))
                .WillRepeatedly(testing::SetArgPointee<0>(od))
                ;
        }

        template <typename T>
        inline
        void mock_view(output_view&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_view]"
            );
        }
    }

    template <typename ...Args>
    inline
    output_view& mock_view(output_view& v, Args&&... args)
    {
        for_each(
            [&v](auto&& x) { detail::mock_view(v, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return v;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<output_view>
    make_view(Args&&... args)
    {
        auto v = std::unique_ptr<testing::NiceMock<output_view> >{
            new testing::NiceMock<output_view>{}
        };

        mock_view(*v, std::forward<Args>(args)...);

        return v;
    }

    inline
    std::unique_ptr<output_view>
    make_view()
    {
        return
            make_view(D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC{});
    }

} }
