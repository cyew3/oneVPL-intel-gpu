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

namespace mocks { namespace dx12 { namespace winapi
{
    struct device_impl
        : object_impl<unknown_impl<ID3D12Device> >
    {
        UINT GetNodeCount() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC*, REFIID, void** /*ppCommandQueue*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE, REFIID, void** /*ppCommandAllocator*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC*, REFIID, void** /*ppPipelineState*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC*, REFIID, void**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateCommandList(UINT /*nodeMask*/, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator*, ID3D12PipelineState*, REFIID, void** /*ppCommandList*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckFeatureSupport(D3D12_FEATURE, void* /*pFeatureSupportData*/, UINT /*FeatureSupportDataSize*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC*, REFIID, void** /*ppvHeap*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateRootSignature(UINT /*nodeMask*/, const void* /*pBlobWithRootSignature*/, SIZE_T /*blobLengthInBytes*/, REFIID, void** /*ppvRootSignature*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CreateShaderResourceView(ID3D12Resource*, const D3D12_SHADER_RESOURCE_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CreateUnorderedAccessView(ID3D12Resource* /*pResource*/, ID3D12Resource* /*pCounterResource*/, const D3D12_UNORDERED_ACCESS_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CreateRenderTargetView(ID3D12Resource*, const D3D12_RENDER_TARGET_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CreateDepthStencilView(ID3D12Resource*, const D3D12_DEPTH_STENCIL_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CreateSampler(const D3D12_SAMPLER_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopyDescriptors(UINT /*NumDestDescriptorRanges*/, const D3D12_CPU_DESCRIPTOR_HANDLE* /*pDestDescriptorRangeStarts*/,
            const UINT* /*pDestDescriptorRangeSizes*/, UINT /*NumSrcDescriptorRanges*/, const D3D12_CPU_DESCRIPTOR_HANDLE* /*pSrcDescriptorRangeStarts*/,
            const UINT* /*pSrcDescriptorRangeSizes*/, D3D12_DESCRIPTOR_HEAP_TYPE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopyDescriptorsSimple(UINT /*NumDescriptors*/, D3D12_CPU_DESCRIPTOR_HANDLE /*DestDescriptorRangeStart*/, D3D12_CPU_DESCRIPTOR_HANDLE /*SrcDescriptorRangeStart*/,
            D3D12_DESCRIPTOR_HEAP_TYPE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo(UINT /*visibleMask*/, UINT /*numResourceDescs*/, const D3D12_RESOURCE_DESC*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        D3D12_HEAP_PROPERTIES GetCustomHeapProperties(UINT /*nodeMask*/, D3D12_HEAP_TYPE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateCommittedResource(const D3D12_HEAP_PROPERTIES*, D3D12_HEAP_FLAGS, const D3D12_RESOURCE_DESC*, D3D12_RESOURCE_STATES,
            const D3D12_CLEAR_VALUE*, REFIID , void** /*ppvResource*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateHeap(const D3D12_HEAP_DESC*, REFIID, void** /*ppvHeap*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreatePlacedResource(ID3D12Heap*, UINT64 /*HeapOffset*/, const D3D12_RESOURCE_DESC*, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE*,
            REFIID, void** /*ppvResource*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateReservedResource(const D3D12_RESOURCE_DESC*, D3D12_RESOURCE_STATES, const D3D12_CLEAR_VALUE*, REFIID, void** /*ppvResource*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateSharedHandle(ID3D12DeviceChild*, const SECURITY_ATTRIBUTES*, DWORD /*Access*/, LPCWSTR /*Name*/, HANDLE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT OpenSharedHandle(HANDLE, REFIID, void** /*ppvObj*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT OpenSharedHandleByName(LPCWSTR /*Name*/, DWORD /*Access*/, HANDLE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT MakeResident(UINT /*NumObjects*/, ID3D12Pageable* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT Evict(UINT /*NumObjects*/, ID3D12Pageable* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateFence(UINT64 /*InitialValue*/, D3D12_FENCE_FLAGS, REFIID, void**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDeviceRemovedReason() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GetCopyableFootprints(const D3D12_RESOURCE_DESC*, UINT /*FirstSubresource*/, UINT /*NumSubresources*/, UINT64 /*BaseOffset*/,
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT*, UINT* /*pNumRows*/, UINT64* /*pRowSizeInBytes*/, UINT64* /*pTotalBytes*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateQueryHeap(const D3D12_QUERY_HEAP_DESC*, REFIID, void** /*ppvHeap*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetStablePowerState(BOOL /*Enable*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC*, ID3D12RootSignature*, REFIID, void** /*ppvCommandSignature*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GetResourceTiling(ID3D12Resource*, UINT* /*pNumTilesForEntireResource*/, D3D12_PACKED_MIP_INFO*, D3D12_TILE_SHAPE*, UINT* /*pNumSubresourceTilings*/,
            UINT /*FirstSubresourceTilingToGet*/, D3D12_SUBRESOURCE_TILING*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        LUID GetAdapterLuid() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
