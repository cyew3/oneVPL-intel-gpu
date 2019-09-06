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

#include "mocks/include/unknown.h"

#include "mocks/include/dx11/context/base.h"

namespace mocks { namespace dx11
{
    namespace detail
    {
        /* Mocks buffer from given (type), provided functor
           tuple(size, pointer) = F(D3D11_VIDEO_DECODER_BUFFER_TYPE) is invoked
           and its results are used as (type, pointer) */
        template <typename F>
        inline
        void mock_context(context& c, std::tuple<D3D11_VIDEO_DECODER_BUFFER_TYPE, F> buffer)
        {
            //HRESULT GetDecoderBuffer(ID3D11VideoDecoder*, D3D11_VIDEO_DECODER_BUFFER_TYPE, UINT*, void**)
            EXPECT_CALL(c, GetDecoderBuffer(testing::NotNull(), testing::Eq(std::get<0>(buffer)),
                testing::NotNull(), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
                    [buffer](UINT* size, void** ptr)
                    {
                        auto r = std::get<1>(buffer)(std::get<0>(buffer));
                        *size = static_cast<UINT>(std::get<0>(r));
                        *ptr = static_cast<void*>(std::get<1>(r));
                        return S_OK;
                    }))
                );

            //HRESULT ReleaseDecoderBuffer(ID3D11VideoDecoder*, D3D11_VIDEO_DECODER_BUFFER_TYPE)
            EXPECT_CALL(c, ReleaseDecoderBuffer(testing::NotNull(), testing::Eq(std::get<0>(buffer))))
                .WillRepeatedly(testing::Return(S_OK));

            mock_context(c, D3D11_VIDEO_DECODER_BUFFER_DESC{ std::get<0>(buffer) });
        }

        /* Mocks buffer from given (type, size, pointer) */
        inline
        void mock_context(context& c, std::tuple<D3D11_VIDEO_DECODER_BUFFER_TYPE, UINT, void*> buffer)
        {
            mock_context(c,
                std::make_tuple(std::get<0>(buffer),
                    [buffer](D3D11_VIDEO_DECODER_BUFFER_TYPE)
                    { return std::make_tuple(std::get<1>(buffer), std::get<2>(buffer)); }
                )
            );
        }

        /* Mocks buffer from given [type] (implying size = 0, pointer = nullptr) */
        inline
        void mock_context(context& c, D3D11_VIDEO_DECODER_BUFFER_TYPE type)
        {
            mock_context(c,
                std::make_tuple(type, UINT(0), static_cast<void*>(nullptr))
            );
        }

        inline
        void mock_context(context& c, D3D11_VIDEO_DECODER_BUFFER_DESC bd)
        {
            c.buffers.push_back(bd);

            //HRESULT SubmitDecoderBuffers(ID3D11VideoDecoder*, UINT, const D3D11_VIDEO_DECODER_BUFFER_DESC*)
            EXPECT_CALL(c, SubmitDecoderBuffers(testing::NotNull(),
                testing::Truly(
                    [&c](UINT count)
                    { return !(count > c.buffers.size()); }
                ), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
                    [&c](UINT count, D3D11_VIDEO_DECODER_BUFFER_DESC const* xd)
                    {
                        //All incoming buffers must be registred
                        return std::all_of(xd, xd + count,
                            [&](D3D11_VIDEO_DECODER_BUFFER_DESC const& d1)
                            {
                                return std::find_if(std::begin(c.buffers), std::end(c.buffers),
                                    [&d1](D3D11_VIDEO_DECODER_BUFFER_DESC const& d2)
                                    { return equal(d1, d2); }
                                ) != std::end(c.buffers);
                            }
                        ) ? S_OK : E_FAIL;
                    }))
                );
        }

        /* Mocks [DecoderExtension] from given extension, returns HRESULT = F(D3D11_VIDEO_DECODER_EXTENSION const*) */
        template <typename F>
        inline
        void mock_context(context& c, std::tuple<D3D11_VIDEO_DECODER_EXTENSION, F> de)
        {
            //HRESULT DecoderExtension(ID3D11VideoDecoder*, const D3D11_VIDEO_DECODER_EXTENSION*)
            EXPECT_CALL(c, DecoderExtension(testing::NotNull(), testing::AllOf(
                testing::NotNull(),
                testing::Truly(
                    [de](D3D11_VIDEO_DECODER_EXTENSION const* xe)
                    { return xe->Function == std::get<0>(de).Function; }
                ))))
                .WillRepeatedly(testing::WithArgs<1>(testing::Invoke(
                    [de](D3D11_VIDEO_DECODER_EXTENSION const* xe)
                    { return std::get<1>(de)(xe); }
                )));
        }

        inline
        void mock_context(context& c, D3D11_VIDEO_DECODER_EXTENSION de)
        {
            mock_context(c,
                std::make_tuple(de, [](D3D11_VIDEO_DECODER_EXTENSION const*) { return S_OK; })
            );
        }

        template <typename T, typename U>
        inline
        typename std::enable_if<
            std::conjunction<std::is_integral<T>, std::is_integral<U> >::value
        >::type mock_context(context& c, std::tuple<ID3D11VideoProcessor*, T, U, RECT> params)
        {
            //void VideoProcessorSetStreamSourceRect(ID3D11VideoProcessor*, UINT, BOOL, const RECT*)
            EXPECT_CALL(c, VideoProcessorSetStreamRect(
                testing::Eq(std::get<0>(params)),
                testing::Eq(std::get<1>(params)),
                testing::Eq(std::get<2>(params)),
                testing::AllOf(
                    testing::NotNull(),
                    testing::Truly([r = std::get<3>(params)](RECT const* xr) { return equal(*xr, r); }
                ))));

            //void VideoProcessorGetStreamSourceRect(ID3D11VideoProcessor*, UINT, BOOL*, RECT*)
            EXPECT_CALL(c, VideoProcessorGetStreamRect(
                testing::Eq(std::get<0>(params)),
                testing::Eq(std::get<1>(params)),
                testing::NotNull(), testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<2>(std::get<2>(params)),
                    testing::SetArgPointee<3>(std::get<3>(params))
                ));
        }
    }
} }
