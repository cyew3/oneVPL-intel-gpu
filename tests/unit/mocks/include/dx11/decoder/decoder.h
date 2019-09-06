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

#include "mocks/include/common.h"

#include "mocks/include/dx11/winapi/decoder.hxx"

namespace mocks { namespace dx11
{
    struct decoder
        : winapi::decoder_impl
    {
        MOCK_METHOD2(GetCreationParameters, HRESULT(D3D11_VIDEO_DECODER_DESC*, D3D11_VIDEO_DECODER_CONFIG*));
        MOCK_METHOD1(GetDriverHandle, HRESULT(HANDLE*));
    };

    namespace detail
    {
        inline
        void mock_decoder(decoder& d, std::tuple<D3D11_VIDEO_DECODER_DESC, D3D11_VIDEO_DECODER_CONFIG> params)
        {
            // HRESULT GetCreationParameters(D3D11_VIDEO_DECODER_DESC*, D3D11_VIDEO_DECODER_CONFIG*)
            EXPECT_CALL(d, GetCreationParameters(testing::NotNull(), testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(std::get<0>(params)),
                    testing::SetArgPointee<1>(std::get<1>(params)),
                    testing::Return(S_OK))
                );
        }

        inline
        void mock_decoder(decoder& d, D3D11_VIDEO_DECODER_DESC vd)
        {
            mock_decoder(d,
                std::make_tuple(vd, D3D11_VIDEO_DECODER_CONFIG{})
            );
        }

        inline
        void mock_decoder(decoder& d, D3D11_VIDEO_DECODER_CONFIG vc)
        {
            mock_decoder(d,
                std::make_tuple(D3D11_VIDEO_DECODER_DESC{}, vc)
            );
        }

        inline
        void mock_decoder(decoder& d, HANDLE h)
        {
            //HRESULT GetDriverHandle(HANDLE*)
            EXPECT_CALL(d, GetDriverHandle(testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(h),
                    testing::Return(S_OK))
                );
        }

        template <typename T>
        inline
        void mock_decoder(decoder& d, T)
        {
            /* If you trap here it means that you passed an argument to [make_decoder]
               which is not supported by library, you need to overload [make_decoder] for that argument's type */
            static_assert(false,
                "Unknown argument was passed to [make_decoder]"
            );
        }
    }

    template <typename ...Args>
    inline
    decoder& mock_decoder(decoder& d, Args&&... args)
    {
        for_each(
            [&d](auto&& x) { detail::mock_decoder(d, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return d;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<decoder>
    make_decoder(Args&&... args)
    {
        auto d = std::unique_ptr<testing::NiceMock<decoder> >{
            new testing::NiceMock<decoder>{}
        };

        using testing::_;

        // HRESULT GetCreationParameters(D3D11_VIDEO_DECODER_DESC*, D3D11_VIDEO_DECODER_CONFIG*)
        EXPECT_CALL(*d, GetCreationParameters(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT GetDriverHandle(HANDLE*)
        EXPECT_CALL(*d, GetDriverHandle(_))
            .WillRepeatedly(testing::Return(E_FAIL));

        mock_decoder(*d, std::forward<Args>(args)...);

        return d;
    }

} }
