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

#include <d3d11.h>

namespace mocks { namespace dx11 { namespace winapi
{
    struct video_device_impl
        : ID3D11VideoDevice
    {
        using type = ID3D11VideoDevice;

        HRESULT CreateVideoDecoder(const D3D11_VIDEO_DECODER_DESC*, const D3D11_VIDEO_DECODER_CONFIG*, ID3D11VideoDecoder**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVideoProcessor(ID3D11VideoProcessorEnumerator*, UINT, ID3D11VideoProcessor**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateAuthenticatedChannel(D3D11_AUTHENTICATED_CHANNEL_TYPE, ID3D11AuthenticatedChannel**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateCryptoSession(const GUID*, const GUID*, const GUID*, ID3D11CryptoSession**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVideoDecoderOutputView(ID3D11Resource*, const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC*, ID3D11VideoDecoderOutputView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVideoProcessorInputView(ID3D11Resource*, ID3D11VideoProcessorEnumerator*, const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC*, ID3D11VideoProcessorInputView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVideoProcessorOutputView(ID3D11Resource*, ID3D11VideoProcessorEnumerator*, const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC*, ID3D11VideoProcessorOutputView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVideoProcessorEnumerator(const D3D11_VIDEO_PROCESSOR_CONTENT_DESC*, ID3D11VideoProcessorEnumerator**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        UINT GetVideoDecoderProfileCount() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVideoDecoderProfile(UINT, GUID*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckVideoDecoderFormat(const GUID*, DXGI_FORMAT, BOOL*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVideoDecoderConfigCount(const D3D11_VIDEO_DECODER_DESC*, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVideoDecoderConfig(const D3D11_VIDEO_DECODER_DESC*, UINT, D3D11_VIDEO_DECODER_CONFIG*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetContentProtectionCaps(const GUID*, const GUID*, D3D11_VIDEO_CONTENT_PROTECTION_CAPS*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckCryptoKeyExchange(const GUID*, const GUID*, UINT, GUID*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPrivateData(REFGUID, UINT, const void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPrivateDataInterface(REFGUID, const IUnknown*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
