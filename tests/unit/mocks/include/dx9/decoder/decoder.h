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
#include "mocks/include/unknown.h"

#include <d3d9.h>
#include <dxva.h>
#include <dxva2api.h>

namespace mocks
{

    inline
    bool equal(DXVA2_DecodeExecuteParams const& d1, DXVA2_DecodeExecuteParams const& d2)
    {
        return
            d1.NumCompBuffers     == d2.NumCompBuffers &&
            d1.pCompressedBuffers == d2.pCompressedBuffers &&
            d1.pExtensionData     == d2.pExtensionData
            ;
    }
}

namespace mocks { namespace dx9
{
    struct decoder
        : unknown_impl<IDirectXVideoDecoder>
    {
        MOCK_METHOD1(GetVideoDecoderService, HRESULT(IDirectXVideoDecoderService**));
        MOCK_METHOD5(GetCreationParameters, HRESULT(GUID*, DXVA2_VideoDesc*, DXVA2_ConfigPictureDecode*, IDirect3DSurface9***, UINT*));
        MOCK_METHOD3(GetBuffer, HRESULT(UINT, void**, UINT*));
        MOCK_METHOD1(ReleaseBuffer, HRESULT(UINT));
        MOCK_METHOD2(BeginFrame, HRESULT(IDirect3DSurface9*, void*));
        MOCK_METHOD1(EndFrame, HRESULT(HANDLE*));
        MOCK_METHOD1(Execute, HRESULT(const DXVA2_DecodeExecuteParams*));
    };

    namespace detail
    {
        /* Mocks buffer from given (type), provided functor
           tuple(size, pointer) = F(UINT = BufferType) is invoked
           and its results are used as (type, pointer) */
        template <typename T, typename F>
        inline
        typename std::enable_if<std::is_integral<T>::value>::type
        mock_decoder(decoder& d, std::tuple<T, F> buffer)
        {
            //HRESULT GetBuffer(UINT, void**, UINT*)
            EXPECT_CALL(d, GetBuffer(testing::Eq(std::get<0>(buffer)),
                testing::NotNull(), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
                    [buffer](void** ptr, UINT* size)
                    {
                        auto r = std::get<1>(buffer)(std::get<0>(buffer));
                        *ptr   = static_cast<void*>(std::get<1>(r));
                        *size  = static_cast<UINT>(std::get<0>(r));
                        return S_OK;
                    }
                )));

            //HRESULT ReleaseBuffer(UINT)
            EXPECT_CALL(d, ReleaseBuffer(testing::Eq(std::get<0>(buffer))))
                .WillRepeatedly(testing::Return(S_OK));

            EXPECT_CALL(d, Execute(testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                    [buffer](DXVA2_DecodeExecuteParams const* params)
                    {
                        return std::find_if(params->pCompressedBuffers, params->pCompressedBuffers + params->NumCompBuffers,
                            [&](DXVA2_DecodeBufferDesc const& d) { return d.CompressedBufferType == std::get<0>(buffer); }
                        ) != params->pCompressedBuffers + params->NumCompBuffers ? S_OK : E_FAIL;
                    }
                )));
        }

        /* Mocks buffer from given (type, size, pointer) */
        template <typename T, typename S, typename P>
        typename std::enable_if<std::conjunction<
            std::is_integral<T>,
            std::is_integral<S>,
            std::is_pointer<P>
        >::value>::type
        mock_decoder(decoder& d, std::tuple<T, S, P> buffer)
        {
            return mock_decoder(d,
                std::make_tuple(std::get<0>(buffer),
                    [buffer](UINT type)
                    {
                        return std::make_tuple(
                            std::get<1>(buffer),
                            std::get<2>(buffer)
                        );
                    }
                )
            );
        }

        inline
        void mock_decoder(decoder& d, DXVA2_DecodeExecuteParams const& ep)
        {
            EXPECT_CALL(d, Execute(testing::AllOf(
                testing::NotNull(),
                testing::Truly([ep](DXVA2_DecodeExecuteParams const* xp) { return equal(*xp, ep); }
                ))))
                .WillRepeatedly(testing::Return(S_OK));
        }

        /* Mocks buffer from given [type] (implying size = 0, pointer = nullptr) */
        template <typename T>
        inline
        typename std::enable_if<std::is_integral<T>::value>::type
        mock_decoder(decoder& d, T type)
        {
            int dummy = 42;
            mock_decoder(d,
                std::make_tuple(type, 0, reinterpret_cast<void*>(nullptr))
            );
        }

        template <typename T>
        inline
        typename std::enable_if<!std::is_integral<T>::value>::type
        mock_decoder(decoder&, T)
        {
            /* If you trap here it means that you passed an argument (tuple's element) to [make_decoder]
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

        //HRESULT GetBuffer(UINT, void**, UINT*)
        EXPECT_CALL(*d, GetBuffer(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT ReleaseBuffer(UINT)
        EXPECT_CALL(*d, ReleaseBuffer(_))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT BeginFrame(IDirect3DSurface9*, void*)
        EXPECT_CALL(*d, BeginFrame(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));
        EXPECT_CALL(*d, BeginFrame(testing::NotNull(), _))
            .WillRepeatedly(testing::Return(S_OK));

        //HRESULT EndFrame(HANDLE*)
        EXPECT_CALL(*d, EndFrame(_))
            .WillRepeatedly(testing::Return(E_FAIL));
        EXPECT_CALL(*d, EndFrame(testing::IsNull()))
            .WillRepeatedly(testing::Return(S_OK));

        //HRESULT Execute(const DXVA2_DecodeExecuteParams*)
        EXPECT_CALL(*d, Execute(_))
            .WillRepeatedly(testing::Return(E_FAIL));

        mock_decoder(*d, std::forward<Args>(args)...);

        return d;
    }

} }
