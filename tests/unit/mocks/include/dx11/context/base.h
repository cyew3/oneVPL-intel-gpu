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

#include "mocks/include/dx11/winapi/context.hxx"
#include "mocks/include/dx11/winapi/video_context.hxx"
#include "mocks/include/dx11/winapi/texture.hxx"

namespace mocks { namespace dx11
{
    struct context
        : unknown_impl<winapi::context_impl, winapi::video_context_impl, winapi::context_multithread_impl>
    {
        std::vector<D3D11_VIDEO_DECODER_BUFFER_DESC> buffers;

        context()
            : base_type(
                uuidof<ID3D11DeviceChild>   (),
                uuidof<ID3D11DeviceContext> (),
                uuidof<ID3D11DeviceContext1>(),
                uuidof<ID3D11DeviceContext2>(),
                uuidof<ID3D11DeviceContext3>()
                //'ID3D11DeviceContext4', 'ID3D11VideoContext' and 'ID3D10Multithread' is automatically mocked by default ctor
            )
        {}

        MOCK_METHOD0(GetType, D3D11_DEVICE_CONTEXT_TYPE());
        MOCK_METHOD5(Map, HRESULT(ID3D11Resource*, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*));
        MOCK_METHOD2(Unmap, void(ID3D11Resource*, UINT));

        MOCK_METHOD1(SetMultithreadProtected, BOOL(BOOL));
        MOCK_METHOD0(GetMultithreadProtected, BOOL());

        MOCK_METHOD4(GetDecoderBuffer, HRESULT(ID3D11VideoDecoder*, D3D11_VIDEO_DECODER_BUFFER_TYPE, UINT*, void**));
        MOCK_METHOD2(ReleaseDecoderBuffer, HRESULT(ID3D11VideoDecoder*, D3D11_VIDEO_DECODER_BUFFER_TYPE));
        MOCK_METHOD4(DecoderBeginFrame, HRESULT(ID3D11VideoDecoder*, ID3D11VideoDecoderOutputView*, UINT, const void*));
        MOCK_METHOD1(DecoderEndFrame, HRESULT(ID3D11VideoDecoder*));
        MOCK_METHOD3(SubmitDecoderBuffers, HRESULT(ID3D11VideoDecoder*, UINT, const D3D11_VIDEO_DECODER_BUFFER_DESC*));
        MOCK_METHOD2(DecoderExtension, HRESULT(ID3D11VideoDecoder*, const D3D11_VIDEO_DECODER_EXTENSION*));

        MOCK_METHOD4(VideoProcessorSetStreamRect, void(ID3D11VideoProcessor*, UINT, BOOL, const RECT*));
        MOCK_METHOD4(VideoProcessorGetStreamRect, void (ID3D11VideoProcessor*, UINT, BOOL*, RECT*));

        void VideoProcessorSetStreamSourceRect(ID3D11VideoProcessor* p, UINT index, BOOL enable, const RECT* rc) override
        { VideoProcessorSetStreamRect(p, index, enable, rc); }
        void VideoProcessorSetStreamDestRect(ID3D11VideoProcessor* p, UINT index, BOOL enable, const RECT* rc) override
        { VideoProcessorSetStreamRect(p, index, enable, rc); }

        void VideoProcessorGetStreamSourceRect(ID3D11VideoProcessor* p, UINT index, BOOL* enabled, RECT* rc) override
        { VideoProcessorGetStreamRect(p, index, enabled, rc); }
        void VideoProcessorGetStreamDestRect(ID3D11VideoProcessor* p, UINT index, BOOL* enabled, RECT* rc) override
        { VideoProcessorGetStreamRect(p, index, enabled, rc); }
    };

    namespace detail
    {
        template <typename T>
        inline
        void mock_context(context&, T)
        {
            /* If you trap here it means that you passed an argument to [make_context]
               which is not supported by library, you need to overload [mock_context] for that argument's type */
            static_assert(false,
                "Unknown argument was passed to [make_context]"
            );
        }
    }

    template <typename ...Args>
    inline
    context& mock_context(context& c, Args&&... args)
    {
        for_each(
            [&c](auto&& x) { detail::mock_context(c, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return c;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<context>
    make_context(Args&&... args)
    {
        auto c = std::unique_ptr<testing::NiceMock<context> >{
            new testing::NiceMock<context>{}
        };

        using testing::_;

        EXPECT_CALL(*c, Map(_, _, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        EXPECT_CALL(*c, SetMultithreadProtected(_))
            .WillRepeatedly(testing::Return(TRUE));
        EXPECT_CALL(*c, GetMultithreadProtected())
            .WillRepeatedly(testing::Return(TRUE));

        //HRESULT GetDecoderBuffer(ID3D11VideoDecoder*, D3D11_VIDEO_DECODER_BUFFER_TYPE, UINT*, void**)
        EXPECT_CALL(*c, GetDecoderBuffer(_, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT ReleaseDecoderBuffer(ID3D11VideoDecoder*, D3D11_VIDEO_DECODER_BUFFER_TYPE)
        EXPECT_CALL(*c, ReleaseDecoderBuffer(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT DecoderBeginFrame(ID3D11VideoDecoder*, ID3D11VideoDecoderOutputView*, UINT, const void*)
        EXPECT_CALL(*c, DecoderBeginFrame(_, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));
        EXPECT_CALL(*c, DecoderBeginFrame(testing::NotNull(), testing::NotNull(), 0, testing::IsNull()))
            .WillRepeatedly(testing::Return(S_OK));

        //HRESULT DecoderEndFrame(ID3D11VideoDecoder*, ID3D11VideoDecoderOutputView*, UINT, const void*)
        EXPECT_CALL(*c, DecoderEndFrame(_))
            .WillRepeatedly(testing::Return(E_FAIL));
        EXPECT_CALL(*c, DecoderEndFrame(testing::NotNull()))
            .WillRepeatedly(testing::Return(S_OK));

        //HRESULT SubmitDecoderBuffers(ID3D11VideoDecoder*, UINT, const D3D11_VIDEO_DECODER_BUFFER_DESC*)
        EXPECT_CALL(*c, SubmitDecoderBuffers(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT DecoderExtension(ID3D11VideoDecoder*, const D3D11_VIDEO_DECODER_EXTENSION*)
        EXPECT_CALL(*c, DecoderExtension(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        mock_context(*c, std::forward<Args>(args)...);

        return c;
    }

} }
