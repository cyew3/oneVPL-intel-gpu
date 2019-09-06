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
    struct list_impl
        : child_impl<unknown_impl<ID3D12GraphicsCommandList> >
    {
        list_impl()
        {
            mock_unknown<ID3D12CommandList>(*this);
        }

        D3D12_COMMAND_LIST_TYPE GetType() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        HRESULT Close() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT Reset(ID3D12CommandAllocator*, ID3D12PipelineState*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearState(ID3D12PipelineState*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DrawInstanced(UINT /*VertexCountPerInstance*/, UINT /*InstanceCount*/, UINT /*StartVertexLocation*/, UINT /*StartInstanceLocation*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DrawIndexedInstanced(UINT /*IndexCountPerInstance*/, UINT /*InstanceCount*/, UINT /*StartIndexLocation*/,
            INT /*BaseVertexLocation*/, UINT /*StartInstanceLocation*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void Dispatch(UINT /*ThreadGroupCountX*/, UINT /*ThreadGroupCountY*/, UINT /*ThreadGroupCountZ*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopyBufferRegion(ID3D12Resource* /*pDstBuffer*/, UINT64 /*DstOffset*/, ID3D12Resource* /*pSrcBuffer*/, UINT64 /*SrcOffset*/, UINT64 /*NumBytes*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* /*pDst*/, UINT /*DstX*/, UINT /*DstY*/, UINT /*DstZ*/,
            const D3D12_TEXTURE_COPY_LOCATION* /*pSrc*/, const D3D12_BOX* /*pSrcBox*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopyResource(ID3D12Resource* /*pDstResource*/, ID3D12Resource* /*pSrcResource*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopyTiles(ID3D12Resource* /*pTiledResource*/, const D3D12_TILED_RESOURCE_COORDINATE*, const D3D12_TILE_REGION_SIZE*,
            ID3D12Resource* /*pBuffer*/, UINT64 /*BufferStartOffsetInBytes*/, D3D12_TILE_COPY_FLAGS /*Flags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ResolveSubresource(ID3D12Resource* /*pDstResource*/, UINT /*DstSubresource*/, ID3D12Resource* /*pSrcResource*/, UINT /*SrcSubresource*/, DXGI_FORMAT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void RSSetViewports(UINT /*NumViewports*/, const D3D12_VIEWPORT* /*pViewports*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void RSSetScissorRects(UINT /*NumRects*/, const D3D12_RECT* /*pRects*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMSetBlendFactor(const FLOAT /*BlendFactor*/[4]) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMSetStencilRef(UINT /*StencilRef*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetPipelineState(ID3D12PipelineState*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ResourceBarrier(UINT /*NumBarriers*/, const D3D12_RESOURCE_BARRIER* /*pBarriers*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ExecuteBundle(ID3D12GraphicsCommandList*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetDescriptorHeaps(UINT /*NumDescriptorHeaps*/, ID3D12DescriptorHeap* const* /*ppDescriptorHeaps*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetComputeRootSignature(ID3D12RootSignature*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetGraphicsRootSignature(ID3D12RootSignature*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetComputeRootDescriptorTable(UINT /*/*RootParameterIndex*/, D3D12_GPU_DESCRIPTOR_HANDLE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetGraphicsRootDescriptorTable(UINT /*RootParameterIndex*/, D3D12_GPU_DESCRIPTOR_HANDLE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetComputeRoot32BitConstant(UINT /*RootParameterIndex*/, UINT /*SrcData*/, UINT /*DestOffsetIn32BitValues*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetGraphicsRoot32BitConstant(UINT /*RootParameterIndex*/, UINT /*SrcData*/, UINT /*DestOffsetIn32BitValues*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetComputeRoot32BitConstants(UINT /*RootParameterIndex*/, UINT /*Num32BitValuesToSet*/, const void* /*pSrcData*/, UINT /*DestOffsetIn32BitValues*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetGraphicsRoot32BitConstants(UINT /*RootParameterIndex*/, UINT /*Num32BitValuesToSet*/, const void* /*pSrcData*/, UINT /*DestOffsetIn32BitValues*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetComputeRootConstantBufferView(UINT /*RootParameterIndex*/, D3D12_GPU_VIRTUAL_ADDRESS /*BufferLocation*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetGraphicsRootConstantBufferView(UINT /*RootParameterIndex*/, D3D12_GPU_VIRTUAL_ADDRESS /*BufferLocation*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetComputeRootShaderResourceView(UINT /*RootParameterIndex*/, D3D12_GPU_VIRTUAL_ADDRESS /*BufferLocation*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetGraphicsRootShaderResourceView(UINT /*RootParameterIndex*/, D3D12_GPU_VIRTUAL_ADDRESS /*BufferLocation*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetComputeRootUnorderedAccessView(UINT /*RootParameterIndex*/, D3D12_GPU_VIRTUAL_ADDRESS /*BufferLocation*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetGraphicsRootUnorderedAccessView(UINT /*RootParameterIndex*/, D3D12_GPU_VIRTUAL_ADDRESS /*BufferLocation*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IASetVertexBuffers(UINT /*StartSlot*/, UINT /*NumViews*/, const D3D12_VERTEX_BUFFER_VIEW* /*pViews*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SOSetTargets(UINT /*StartSlot*/, UINT /*NumViews*/, const D3D12_STREAM_OUTPUT_BUFFER_VIEW* /*pViews*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMSetRenderTargets(UINT /*NumRenderTargetDescriptors*/, const D3D12_CPU_DESCRIPTOR_HANDLE* /*pRenderTargetDescriptors*/,
            BOOL /*RTsSingleHandleToDescriptorRange*/, const D3D12_CPU_DESCRIPTOR_HANDLE* /*pDepthStencilDescriptor*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE /*DepthStencilView*/, D3D12_CLEAR_FLAGS /*ClearFlags*/,
            FLOAT /*Depth*/, UINT8 /*Stencil*/, UINT /*NumRects*/, const D3D12_RECT* /*pRects*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE /*RenderTargetView*/, const FLOAT /*ColorRGBA*/[ 4 ], UINT /*NumRects*/, const D3D12_RECT* /*pRects*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE /*ViewGPUHandleInCurrentHeap*/, D3D12_CPU_DESCRIPTOR_HANDLE /*ViewCPUHandle*/,
            ID3D12Resource*, const UINT /*Values*/[ 4 ], UINT /*NumRects*/, const D3D12_RECT* /*pRects*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE /*ViewGPUHandleInCurrentHeap*/, D3D12_CPU_DESCRIPTOR_HANDLE /*ViewCPUHandle*/,
            ID3D12Resource*, const FLOAT /*Values*/[ 4 ], UINT /*NumRects*/, const D3D12_RECT* /*pRects*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DiscardResource(ID3D12Resource*, const D3D12_DISCARD_REGION*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void BeginQuery(ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT /*Index*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void EndQuery(ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT /*Index*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ResolveQueryData(ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT /*StartIndex*/, UINT /*NumQueries*/,
            ID3D12Resource* /*pDestinationBuffer*/, UINT64 /*AlignedDestinationBufferOffset*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetPredication(ID3D12Resource*, UINT64 /*AlignedBufferOffset*/, D3D12_PREDICATION_OP) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetMarker(UINT /*Metadata*/, const void* /*pData*/, UINT /*Size*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void BeginEvent(UINT /*Metadata*/, const void* /*pData*/, UINT /*Size*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void EndEvent() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ExecuteIndirect(ID3D12CommandSignature*, UINT /*MaxCommandCount*/, ID3D12Resource* /*pArgumentBuffer*/, UINT64 /*ArgumentBufferOffset*/,
            ID3D12Resource* /*pCountBuffer*/, UINT64 /*CountBufferOffset*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
