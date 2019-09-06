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

#include "child.hxx"

namespace mocks { namespace dx11 { namespace winapi
{
    struct processor_impl
        : child_impl<unknown_impl<ID3D11VideoProcessor> >
    {
        void GetContentDesc(D3D11_VIDEO_PROCESSOR_CONTENT_DESC*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GetRateConversionCaps(D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

    struct processor_enum_impl
        : child_impl<unknown_impl<ID3D11VideoProcessorEnumerator> >
    {
        HRESULT GetVideoProcessorContentDesc(D3D11_VIDEO_PROCESSOR_CONTENT_DESC*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckVideoProcessorFormat(DXGI_FORMAT, UINT* /*pFlags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVideoProcessorCaps(D3D11_VIDEO_PROCESSOR_CAPS*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVideoProcessorRateConversionCaps(UINT /*TypeIndex*/, D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVideoProcessorCustomRate(UINT /*TypeIndex*/, UINT /*CustomRateIndex*/, D3D11_VIDEO_PROCESSOR_CUSTOM_RATE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVideoProcessorFilterRange(D3D11_VIDEO_PROCESSOR_FILTER, D3D11_VIDEO_PROCESSOR_FILTER_RANGE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
