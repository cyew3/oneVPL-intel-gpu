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

#include "mocks/include/dx11/winapi/processor.hxx"

namespace mocks { namespace dx11
{
    struct processor
        : winapi::processor_impl
    {
        MOCK_METHOD1(GetContentDesc, void(D3D11_VIDEO_PROCESSOR_CONTENT_DESC*));
        MOCK_METHOD1(GetRateConversionCaps, void(D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS*));
    };

    namespace detail
    {
        template <typename T>
        inline
        void mock_processor(processor&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_processor]"
            );
        }

        inline
        void mock_processor(processor& p, D3D11_VIDEO_PROCESSOR_CONTENT_DESC cd)
        {
            //void GetContentDesc(D3D11_VIDEO_PROCESSOR_CONTENT_DESC*)
            EXPECT_CALL(p, GetContentDesc(testing::NotNull()))
                .WillRepeatedly(testing::SetArgPointee<0>(cd));
        }

        inline
        void mock_processor(processor& p, D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rc)
        {
            //void GetRateConversionCaps(D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS*)
            EXPECT_CALL(p, GetRateConversionCaps(testing::NotNull()))
                .WillRepeatedly(testing::SetArgPointee<0>(rc));
        }
    }

    template <typename ...Args>
    inline
    processor& mock_processor(processor& p, Args&&... args)
    {
        for_each(
            [&p](auto&& x) { detail::mock_processor(p, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return p;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<processor>
    make_processor(Args&&... args)
    {
        using testing::_;

        auto e = std::unique_ptr<testing::NiceMock<processor> >{
            new testing::NiceMock<processor>{}
        };

        mock_processor(*e, std::forward<Args>(args)...);

        return e;
    }

} }
