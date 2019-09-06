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

#include <d3d9.h>

namespace mocks { namespace dx9 { namespace winapi
{
    struct d3d_impl
        : unknown_impl<IDirect3D9Ex>
    {
        d3d_impl()
        {
            mock_unknown(*this, static_cast<IDirect3D9*>(this), uuidof<IDirect3D9>());
        }

        HRESULT RegisterSoftwareDevice(void* /*pInitializeFunction*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        UINT GetAdapterCount() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetAdapterIdentifier(UINT /*Adapter*/, DWORD /*Flags*/, D3DADAPTER_IDENTIFIER9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        UINT GetAdapterModeCount(UINT /*Adapter*/, D3DFORMAT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT EnumAdapterModes(UINT /*Adapter*/, D3DFORMAT, UINT /*Mode*/, D3DDISPLAYMODE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetAdapterDisplayMode(UINT /*Adapter*/, D3DDISPLAYMODE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckDeviceType(UINT /*Adapter*/, D3DDEVTYPE, D3DFORMAT /*DisplayFormat*/, D3DFORMAT /*BackBufferFormat*/, BOOL /*bWindowed*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckDeviceFormat(UINT /*Adapter*/, D3DDEVTYPE, D3DFORMAT /*AdapterFormat*/, DWORD /*Usage*/, D3DRESOURCETYPE, D3DFORMAT /*CheckFormat*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckDeviceMultiSampleType(UINT /*Adapter*/, D3DDEVTYPE, D3DFORMAT, BOOL /*Windowed*/, D3DMULTISAMPLE_TYPE, DWORD* /*pQualityLevels*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckDepthStencilMatch(UINT /*Adapter*/, D3DDEVTYPE, D3DFORMAT /*AdapterFormat*/, D3DFORMAT /*RenderTargetFormat*/, D3DFORMAT /*DepthStencilFormat*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckDeviceFormatConversion(UINT /*Adapter*/, D3DDEVTYPE, D3DFORMAT /*SourceFormat*/, D3DFORMAT /*TargetFormat*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDeviceCaps(UINT /*Adapter*/, D3DDEVTYPE, D3DCAPS9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HMONITOR GetAdapterMonitor(UINT /*Adapter*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDevice(UINT /*Adapter*/, D3DDEVTYPE, HWND /*hFocusWindow*/, DWORD /*BehaviorFlags*/, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //IDirect3D9Ex
        UINT GetAdapterModeCountEx(UINT /*Adapter*/, const D3DDISPLAYMODEFILTER*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT EnumAdapterModesEx(UINT /*Adapter*/, const D3DDISPLAYMODEFILTER*, UINT /*Mode*/, D3DDISPLAYMODEEX*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetAdapterDisplayModeEx(UINT /*Adapter*/, D3DDISPLAYMODEEX*, D3DDISPLAYROTATION*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDeviceEx(UINT /*Adapter*/, D3DDEVTYPE, HWND /*hFocusWindow*/, DWORD /*BehaviorFlags*/, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetAdapterLUID(UINT /*Adapter*/, LUID*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
