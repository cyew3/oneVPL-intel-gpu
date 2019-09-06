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

#include "mocks/include/dx9/resource/surface.h"

namespace mocks { namespace dx9
{
    struct service
        : unknown_impl<IDirectXVideoDecoderService>
    {
        service()
            : unknown_impl<IDirectXVideoDecoderService>(uuidof<IDirectXVideoAccelerationService>())
        {}

        MOCK_METHOD9(CreateSurface, HRESULT(UINT, UINT, UINT, D3DFORMAT, D3DPOOL, DWORD, DWORD, IDirect3DSurface9**, HANDLE*));

        MOCK_METHOD2(GetDecoderDeviceGuids, HRESULT(UINT*, GUID**));
        MOCK_METHOD3(GetDecoderRenderTargets, HRESULT(REFGUID, UINT*, D3DFORMAT**));
        MOCK_METHOD5(GetDecoderConfigurations, HRESULT(REFGUID, DXVA2_VideoDesc const*, void*, UINT*, DXVA2_ConfigPictureDecode**));
        MOCK_METHOD6(CreateVideoDecoder, HRESULT(REFGUID, DXVA2_VideoDesc const*, DXVA2_ConfigPictureDecode const*, IDirect3DSurface9**, UINT, IDirectXVideoDecoder**));
    };

    namespace detail
    {
        inline
        void mock_service(service& s, D3DSURFACE_DESC sd)
        {
            using testing::_;

            EXPECT_CALL(s, CreateSurface(testing::Eq(sd.Width), testing::Eq(sd.Height),
                _, //meathd actually creates [BackBuffers + 1] surfaces, so UINT(0) is acceptable
                testing::Eq(sd.Format), testing::Eq(sd.Pool), testing::Eq(sd.Usage),
                testing::AnyOf(DXVA2_VideoDecoderRenderTarget, DXVA2_VideoProcessorRenderTarget, DXVA2_VideoProcessorRenderTarget, DXVA2_VideoSoftwareRenderTarget),
                testing::NotNull(), testing::IsNull()))
                .WillRepeatedly(testing::WithArgs<2, 7>(testing::Invoke(
                    [sd, &s](UINT count, IDirect3DSurface9** surfaces)
                    {
                        std::generate(surfaces, surfaces + count + 1, //BackBuffers + 1
                            [sd]() { return make_surface(sd).release(); }
                        );

                        return S_OK;
                    }
                )));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_service(service& s, std::tuple<D3DFORMAT, T /*width*/, T /*height*/> params)
        {
            D3DSURFACE_DESC sd =
            {
                std::get<0>(params),
                D3DRTYPE_SURFACE,
                0, D3DPOOL_DEFAULT,
                D3DMULTISAMPLE_NONE, 0,
                UINT(std::get<1>(params)), UINT(std::get<2>(params))
            };

            return
                mock_service(s, sd);
        }

        template <typename T>
        inline
        void mock_service(service&, T)
        {
            /* If you trap here it means that you passed an argument (tuple's element) to [make_service]
               which is not supported by library, you need to overload [make_service] for that argument's type */
            static_assert(false,
                "Unknown argument was passed to [make_service]"
            );
        }
    }

    template <typename ...Args>
    inline
    service& mock_service(service& s, Args&&... args)
    {
        for_each(
            [&s](auto&& x) { detail::mock_service(s, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return s;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<service>
    make_service(Args&&... args)
    {
        auto s = std::unique_ptr<testing::NiceMock<service> >{
            new testing::NiceMock<service>{}
        };

        using testing::_;

        EXPECT_CALL(*s, CreateSurface(_, _, _, _, _, _, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        EXPECT_CALL(*s, GetDecoderDeviceGuids(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        EXPECT_CALL(*s, GetDecoderRenderTargets(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        EXPECT_CALL(*s, GetDecoderConfigurations(_, _, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        EXPECT_CALL(*s, CreateVideoDecoder(_, _, _, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        mock_service(*s, std::forward<Args>(args)...);

        return s;
    }

} }
