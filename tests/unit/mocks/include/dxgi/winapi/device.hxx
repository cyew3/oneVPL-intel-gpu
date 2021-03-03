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

#include "object.hxx"

#include <dxgi1_5.h>

namespace mocks { namespace dxgi { namespace winapi
{
    struct device_impl
        : object_impl<IDXGIDevice4>
    {
        device_impl()
        {
            mock_unknown<
                IDXGIDevice,
                IDXGIDevice1,
                IDXGIDevice2,
                IDXGIDevice3
                //'IDXGIDevice4' is automatically mocked by default ctor
            >(*this);
        }

        HRESULT GetAdapter(IDXGIAdapter**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateSurface(const DXGI_SURFACE_DESC*, UINT, DXGI_USAGE, const DXGI_SHARED_RESOURCE*, IDXGISurface**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT QueryResourceResidency(IUnknown* const*, DXGI_RESIDENCY*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetGPUThreadPriority(INT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetGPUThreadPriority(INT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //IDXGIDevice1
        HRESULT SetMaximumFrameLatency(UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetMaximumFrameLatency(UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //IDXGIDevice2
        HRESULT OfferResources(UINT /*NumResources*/, IDXGIResource* const*, DXGI_OFFER_RESOURCE_PRIORITY) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT ReclaimResources(UINT /*NumResources*/, IDXGIResource* const*, BOOL* /*pDiscarded*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT EnqueueSetEvent(HANDLE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //IDXGIDevice3
        void Trim() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //IDXGIDevice4
        HRESULT OfferResources1(UINT /*NumResources*/, IDXGIResource* const*, DXGI_OFFER_RESOURCE_PRIORITY, UINT /*Flags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT ReclaimResources1(UINT /*NumResources*/, IDXGIResource* const*, DXGI_RECLAIM_RESOURCE_RESULTS*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
