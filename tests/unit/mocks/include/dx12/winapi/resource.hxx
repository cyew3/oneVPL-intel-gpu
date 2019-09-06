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

namespace mocks { namespace dx12 { namespace winapi
{
    struct resource_impl
        : child_impl<unknown_impl<ID3D12Resource> >
    {
        resource_impl()
        {
            mock_unknown<ID3D12Pageable>(*this);
        }

        HRESULT Map(UINT /*Subresource*/, const D3D12_RANGE*, void** /*ppData*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void Unmap(UINT /*Subresource*/, const D3D12_RANGE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        D3D12_RESOURCE_DESC GetDesc() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT WriteToSubresource(UINT /*DstSubresource*/, const D3D12_BOX* /*pDstBox*/, const void* /*pSrcData*/, UINT /*SrcRowPitch*/, UINT /*SrcDepthPitch*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT ReadFromSubresource(void* /*pDstData*/, UINT /*DstRowPitch*/, UINT /*DstDepthPitch*/, UINT /*SrcSubresource*/, const D3D12_BOX* /*pSrcBox*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetHeapProperties(D3D12_HEAP_PROPERTIES*, D3D12_HEAP_FLAGS*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
