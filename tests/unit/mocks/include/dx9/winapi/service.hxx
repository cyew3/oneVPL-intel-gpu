// Copyright (c) 2020 Intel Corporation
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

#include <d3d9.h>
#include <dxva2api.h>

namespace mocks { namespace dx9 { namespace winapi
{
    struct service_impl
        : unknown_impl<IDirectXVideoDecoderService>
    {
        service_impl()
        {
            mock_unknown<IDirectXVideoAccelerationService>(*this);
        }

        //IDirectXVideoAccelerationService
        HRESULT CreateSurface(UINT /*Width*/, UINT /*Height*/, UINT /*BackBuffers*/, D3DFORMAT /*Format*/, D3DPOOL, DWORD /*Usage*/, DWORD /*DxvaType*/,
            IDirect3DSurface9**, HANDLE* /*pSharedHandle*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //IDirectXVideoDecoderService
        HRESULT GetDecoderDeviceGuids(UINT* /*pCount*/, GUID**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDecoderRenderTargets(REFGUID, UINT* /*pCount*/, D3DFORMAT**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDecoderConfigurations(REFGUID, const DXVA2_VideoDesc*, void* /*pReserved*/, UINT* /*pCount*/, DXVA2_ConfigPictureDecode**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVideoDecoder(REFGUID, const DXVA2_VideoDesc*, const DXVA2_ConfigPictureDecode*, IDirect3DSurface9**, UINT /*NumRenderTargets*/, IDirectXVideoDecoder**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
