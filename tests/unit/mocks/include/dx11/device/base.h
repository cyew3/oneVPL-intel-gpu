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

#include "mocks/include/hook.h"
#include "mocks/include/unknown.h"

#include "mocks/include/dx11/winapi/device.hxx"
#include "mocks/include/dx11/winapi/video_device.hxx"

#include <array>

namespace std
{
    template <>
    struct less<GUID>
    {
        bool operator()(GUID const& l, GUID const& r) const
        {
            return lexicographical_compare(
                reinterpret_cast<char const*>(&l),
                reinterpret_cast<char const*>(&l) + sizeof(l),
                reinterpret_cast<char const*>(&r),
                reinterpret_cast<char const*>(&r) + sizeof(r)
            );
        }
    };
}

namespace mocks { namespace dx11
{
    struct device
        : unknown_impl<winapi::device_impl, winapi::video_device_impl>
    {
        device()
            : base_type(
                uuidof<ID3D11Device> (),
                uuidof<ID3D11Device1>(),
                uuidof<ID3D11Device2>(),
                uuidof<ID3D11Device3>(),
                uuidof<ID3D11Device4>()
                //'ID3D11Device5' and 'ID3D11VideoDevice' is automatically mocked by default ctor
            )
        {}

        CComPtr<IDXGIAdapter>                                    adapter;
        CComPtr<ID3D11DeviceContext>                             context;
        CComPtr<ID3D11VideoProcessorEnumerator>                  enumerator;

        std::vector<D3D11_VIDEO_DECODER_DESC>                    profiles;
        std::map<GUID, std::vector<D3D11_VIDEO_DECODER_CONFIG> > configs;
        std::vector<D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS>  rate_caps;

        MOCK_METHOD3(CreateBuffer, HRESULT(const D3D11_BUFFER_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Buffer**));
        MOCK_METHOD3(CreateTexture2D, HRESULT(const D3D11_TEXTURE2D_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture2D**));

        MOCK_METHOD10(CreateDevice,
            HRESULT(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, D3D_FEATURE_LEVEL const*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**)
        );
        MOCK_METHOD1(GetImmediateContext, void(ID3D11DeviceContext**));
        MOCK_METHOD4(CreateFence, HRESULT(UINT64, D3D11_FENCE_FLAG, REFIID, void**));

        //ID3D11VideoDevice
        MOCK_METHOD3(CreateVideoDecoder, HRESULT(const D3D11_VIDEO_DECODER_DESC*, const D3D11_VIDEO_DECODER_CONFIG*, ID3D11VideoDecoder**));
        MOCK_METHOD3(CreateVideoProcessor, HRESULT(ID3D11VideoProcessorEnumerator*, UINT, ID3D11VideoProcessor**));
        MOCK_METHOD3(CreateVideoDecoderOutputView, HRESULT(ID3D11Resource*, const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC*, ID3D11VideoDecoderOutputView**));
        MOCK_METHOD3(CheckVideoDecoderFormat, HRESULT(const GUID*, DXGI_FORMAT, BOOL*));
        MOCK_METHOD0(GetVideoDecoderProfileCount, UINT());
        MOCK_METHOD2(GetVideoDecoderProfile, HRESULT(UINT, GUID*));
        MOCK_METHOD2(GetVideoDecoderConfigCount, HRESULT(const D3D11_VIDEO_DECODER_DESC*, UINT*));
        MOCK_METHOD3(GetVideoDecoderConfig, HRESULT(const D3D11_VIDEO_DECODER_DESC*, UINT, D3D11_VIDEO_DECODER_CONFIG*));
        MOCK_METHOD2(CreateVideoProcessorEnumerator, HRESULT(const D3D11_VIDEO_PROCESSOR_CONTENT_DESC*, ID3D11VideoProcessorEnumerator**));

        void detour(IDXGIAdapter* a)
        {
            std::shared_ptr<device> mapper(
                this,
                [a](device* instance)
                { hook<device>::owner()->map[a] = instance; }
            );

            if (thunk)
                return;

            if (hook<device>::owner())
                //copy adapters map from current instance
                map = hook<device>::owner()->map;

            std::array<std::pair<void*, void*>, 1> functions{
                std::make_pair(D3D11CreateDevice, CreateDeviceT)
            };

            thunk.reset(
                new hook<device>(this, std::begin(functions), std::end(functions))
            );
        }

    private:

        std::map<IDXGIAdapter*, device*> map;
        std::shared_ptr<hook<device> >   thunk;

        static
        HRESULT CreateDeviceT(IDXGIAdapter* adapter, D3D_DRIVER_TYPE type, HMODULE module, UINT flags,
            D3D_FEATURE_LEVEL const* features, UINT count, UINT version,
            ID3D11Device** result, D3D_FEATURE_LEVEL* level, ID3D11DeviceContext** context)
        {
            auto instance = hook<device>::owner();
            assert(instance && "No device instance but \'D3D11CreateDevice\' is hooked");

            instance = instance->map[adapter];
            return
                instance ? instance->CreateDevice(adapter, type, module, flags, features, count, version, result, level, context) : E_FAIL;
        }
    };

    namespace detail
    {
        template <typename T>
        inline
        void mock_device(device&, T)
        {
            /* If you trap here it means that you passed an argument to [make_device]
                which is not supported by library, you need to overload [mock_device] for that argument's type */
            static_assert(false,
                "Unknown argument was passed to [make_device]"
            );
        }
    }

    template <typename ...Args>
    inline
    device& mock_device(device& d, Args&&... args)
    {
        for_each(
            [&d](auto&& x) { detail::mock_device(d, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return d;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<device>
    make_device(Args&&... args)
    {
        auto d = std::unique_ptr<testing::NiceMock<device> >{
            new testing::NiceMock<device>{}
        };

        using testing::_;

        EXPECT_CALL(*d, CreateDevice(_, _, _, _, _, _, _, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        EXPECT_CALL(*d, CreateBuffer(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        EXPECT_CALL(*d, CreateTexture2D(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        //HRESULT CreateFence(UINT64, D3D11_FENCE_FLAG, REFIID, void**)
        EXPECT_CALL(*d, CreateFence(_, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //void GetImmediateContext(ID3D11DeviceContext**)
        EXPECT_CALL(*d, GetImmediateContext(testing::NotNull()))
            .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                [d = d.get()](ID3D11DeviceContext** result)
                { d->context.CopyTo(result); }
            )));

        //HRESULT CreateVideoDecoder(const D3D11_VIDEO_DECODER_DESC*, const D3D11_VIDEO_DECODER_CONFIG*, ID3D11VideoDecoder**)
        EXPECT_CALL(*d, CreateVideoDecoder(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT CreateVideoProcessor(ID3D11VideoProcessorEnumerator*, UINT, ID3D11VideoProcessor**)
        EXPECT_CALL(*d, CreateVideoProcessor(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT CreateVideoDecoderOutputView(ID3D11Resource*, const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC*, ID3D11VideoDecoderOutputView**)
        EXPECT_CALL(*d, CreateVideoDecoderOutputView(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT CheckVideoDecoderFormat(const GUID*, DXGI_FORMAT, BOOL*)
        EXPECT_CALL(*d, CheckVideoDecoderFormat(_, _, _))
            .WillRepeatedly(testing::Return(E_INVALIDARG));

        //HRESULT GetVideoDecoderProfile(UINT, GUID*)
        EXPECT_CALL(*d, GetVideoDecoderProfile(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT GetVideoDecoderConfigCount(const D3D11_VIDEO_DECODER_DESC*, UINT*)
        EXPECT_CALL(*d, GetVideoDecoderConfigCount(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        EXPECT_CALL(*d, GetVideoDecoderConfig(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT CreateVideoProcessorEnumerator(const D3D11_VIDEO_PROCESSOR_CONTENT_DESC*, ID3D11VideoProcessorEnumerator**)
        EXPECT_CALL(*d, CreateVideoProcessorEnumerator(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        mock_device(*d, std::forward<Args>(args)...);

        return d;
    }

} }
