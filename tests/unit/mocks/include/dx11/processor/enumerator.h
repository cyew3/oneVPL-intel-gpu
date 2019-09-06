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
    struct processor_enum
        : winapi::processor_enum_impl
    {
        MOCK_METHOD1(GetVideoProcessorContentDesc, HRESULT(D3D11_VIDEO_PROCESSOR_CONTENT_DESC*));
        MOCK_METHOD2(CheckVideoProcessorFormat, HRESULT(DXGI_FORMAT, UINT*));
        MOCK_METHOD1(GetVideoProcessorCaps, HRESULT(D3D11_VIDEO_PROCESSOR_CAPS*));
        MOCK_METHOD2(GetVideoProcessorRateConversionCaps, HRESULT(UINT, D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS*));
    };

    namespace detail
    {
        inline
        void mock_enum(processor_enum& e, D3D11_VIDEO_PROCESSOR_CONTENT_DESC cd)
        {
            //HRESULT GetVideoProcessorContentDesc(D3D11_VIDEO_PROCESSOR_CONTENT_DESC*)
            EXPECT_CALL(e, GetVideoProcessorContentDesc(testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(cd),
                    testing::Return(S_OK)
                ));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::disjunction<std::is_integral<T>, std::is_same<T, D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT> >::value
        >::type
        mock_enum(processor_enum& e, std::tuple<DXGI_FORMAT, T> params)
        {
            //HRESULT CheckVideoProcessorFormat(DXGI_FORMAT, UINT*)
            EXPECT_CALL(e, CheckVideoProcessorFormat(testing::Eq(std::get<0>(params)), testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<1>(std::get<1>(params)),
                    testing::Return(S_OK)
                ));
        }

        inline
        void mock_enum(processor_enum& e, D3D11_VIDEO_PROCESSOR_CAPS pc)
        {
            //HRESULT GetVideoProcessorCaps(D3D11_VIDEO_PROCESSOR_CAPS*)
            EXPECT_CALL(e, GetVideoProcessorCaps(testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(pc),
                    testing::Return(S_OK)
                ));
        }

        inline
        void mock_enum(processor_enum& e, D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rc)
        {
            //HRESULT GetVideoProcessorRateConversionCaps(UINT /*TypeIndex*/, D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS*)
            EXPECT_CALL(e, GetVideoProcessorRateConversionCaps(testing::_, testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<1>(rc),
                    testing::Return(S_OK)
                ));
        }

        template <typename T>
        inline
        void mock_enum(processor_enum&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_enum]"
            );
        }
    }

    template <typename ...Args>
    inline
    processor_enum& mock_enum(processor_enum& e, Args&&... args)
    {
        for_each(
            [&e](auto&& x) { detail::mock_enum(e, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return e;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<processor_enum>
    make_enum(Args&&... args)
    {
        using testing::_;

        auto e = std::unique_ptr<testing::NiceMock<processor_enum> >{
            new testing::NiceMock<processor_enum>{}
        };

        using testing::_;

        //HRESULT GetVideoProcessorContentDesc(D3D11_VIDEO_PROCESSOR_CONTENT_DESC*)
        EXPECT_CALL(*e, GetVideoProcessorContentDesc(_))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        //HRESULT CheckVideoProcessorFormat(DXGI_FORMAT, UINT* /*pFlags*/)
        EXPECT_CALL(*e, CheckVideoProcessorFormat(_, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        //HRESULT GetVideoProcessorCaps(D3D11_VIDEO_PROCESSOR_CAPS*)
        EXPECT_CALL(*e, GetVideoProcessorCaps(_))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;
        //HRESULT GetVideoProcessorRateConversionCaps(UINT TypeIndex, D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS*)
        EXPECT_CALL(*e, GetVideoProcessorRateConversionCaps(_, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        mock_enum(*e, std::forward<Args>(args)...);

        return e;
    }

} }
