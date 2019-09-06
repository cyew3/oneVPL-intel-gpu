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

#include <d3d11_4.h>

namespace mocks { namespace dx11 { namespace winapi
{
    /* Define our stubs for [ID3D11DeviceContext] instead of using [MOCK_METHODx] due to too big object size */
    struct context_impl
        : child_impl<ID3D11DeviceContext4>
    {
        using type = ID3D11DeviceContext4;

        void VSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void PSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void PSSetShader(ID3D11PixelShader*, ID3D11ClassInstance* const*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void PSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VSSetShader(ID3D11VertexShader*, ID3D11ClassInstance* const*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DrawIndexed(UINT, UINT, INT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void Draw(UINT, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT Map(ID3D11Resource*, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void Unmap(ID3D11Resource*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void PSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IASetInputLayout(ID3D11InputLayout*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IASetVertexBuffers(UINT, UINT, ID3D11Buffer* const*, const UINT*, const UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IASetIndexBuffer(ID3D11Buffer*, DXGI_FORMAT, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DrawIndexedInstanced(UINT, UINT, UINT, INT, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DrawInstanced(UINT, UINT, UINT, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GSSetShader(ID3D11GeometryShader*, ID3D11ClassInstance* const*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void Begin(ID3D11Asynchronous*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void End(ID3D11Asynchronous*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetData(ID3D11Asynchronous*, void*, UINT, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetPredication(ID3D11Predicate*, BOOL) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMSetRenderTargets(UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMSetRenderTargetsAndUnorderedAccessViews(UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*,
            UINT, UINT, ID3D11UnorderedAccessView* const*, const UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMSetBlendState(ID3D11BlendState*, const FLOAT[4], UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMSetDepthStencilState(ID3D11DepthStencilState*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SOSetTargets(UINT, ID3D11Buffer* const*, const UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DrawAuto() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DrawIndexedInstancedIndirect(ID3D11Buffer*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DrawInstancedIndirect(ID3D11Buffer*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void Dispatch(UINT, UINT, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DispatchIndirect(ID3D11Buffer*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void RSSetState(ID3D11RasterizerState*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void RSSetViewports(UINT, const D3D11_VIEWPORT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void RSSetScissorRects(UINT, const D3D11_RECT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopySubresourceRegion(ID3D11Resource*, UINT, UINT, UINT, UINT, ID3D11Resource*, UINT, const D3D11_BOX*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopyResource(ID3D11Resource*, ID3D11Resource*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void UpdateSubresource(ID3D11Resource*, UINT, const D3D11_BOX*, const void*, UINT, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopyStructureCount(ID3D11Buffer*, UINT, ID3D11UnorderedAccessView*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearRenderTargetView(ID3D11RenderTargetView*, const FLOAT[4]) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView*, const UINT[4]) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView*, const FLOAT[4]) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearDepthStencilView(ID3D11DepthStencilView*, UINT, FLOAT, UINT8) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GenerateMips(ID3D11ShaderResourceView*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetResourceMinLOD(ID3D11Resource*, FLOAT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        FLOAT GetResourceMinLOD(ID3D11Resource*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ResolveSubresource(ID3D11Resource*, UINT, ID3D11Resource*, UINT, DXGI_FORMAT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ExecuteCommandList(ID3D11CommandList*, BOOL) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void HSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void HSSetShader(ID3D11HullShader*, ID3D11ClassInstance* const*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void HSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void HSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DSSetShader(ID3D11DomainShader*, ID3D11ClassInstance* const*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSSetShaderResources(UINT, UINT, ID3D11ShaderResourceView* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSSetUnorderedAccessViews(UINT, UINT, ID3D11UnorderedAccessView* const*, const UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSSetShader(ID3D11ComputeShader*, ID3D11ClassInstance* const*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSSetSamplers(UINT, UINT, ID3D11SamplerState* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSSetConstantBuffers(UINT, UINT, ID3D11Buffer* const*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void PSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void PSGetShader(ID3D11PixelShader**, ID3D11ClassInstance**, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void PSGetSamplers(UINT, UINT, ID3D11SamplerState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VSGetShader(ID3D11VertexShader**, ID3D11ClassInstance**, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void PSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IAGetInputLayout(ID3D11InputLayout**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IAGetVertexBuffers(UINT, UINT, ID3D11Buffer**, UINT*, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IAGetIndexBuffer(ID3D11Buffer**, DXGI_FORMAT*, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GSGetShader(ID3D11GeometryShader**, ID3D11ClassInstance**, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VSGetSamplers(UINT, UINT, ID3D11SamplerState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GetPredication(ID3D11Predicate**, BOOL*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GSGetSamplers(UINT, UINT, ID3D11SamplerState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMGetRenderTargets(UINT, ID3D11RenderTargetView**, ID3D11DepthStencilView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMGetRenderTargetsAndUnorderedAccessViews(UINT, ID3D11RenderTargetView**, ID3D11DepthStencilView**, UINT, UINT, ID3D11UnorderedAccessView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMGetBlendState(ID3D11BlendState**, FLOAT[4], UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void OMGetDepthStencilState(ID3D11DepthStencilState**, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SOGetTargets(UINT, ID3D11Buffer**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void RSGetState(ID3D11RasterizerState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void RSGetViewports(UINT*, D3D11_VIEWPORT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void RSGetScissorRects(UINT*, D3D11_RECT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void HSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void HSGetShader(ID3D11HullShader**, ID3D11ClassInstance**, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void HSGetSamplers(UINT, UINT, ID3D11SamplerState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void HSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DSGetShader(ID3D11DomainShader**, ID3D11ClassInstance**, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DSGetSamplers(UINT, UINT, ID3D11SamplerState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSGetShaderResources(UINT, UINT, ID3D11ShaderResourceView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSGetUnorderedAccessViews(UINT, UINT, ID3D11UnorderedAccessView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSGetShader(ID3D11ComputeShader**, ID3D11ClassInstance**, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSGetSamplers(UINT, UINT, ID3D11SamplerState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSGetConstantBuffers(UINT, UINT, ID3D11Buffer**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearState() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void Flush() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        D3D11_DEVICE_CONTEXT_TYPE GetType() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        UINT GetContextFlags() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT FinishCommandList(BOOL, ID3D11CommandList**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //ID3D11DeviceContext1
        void CopySubresourceRegion1(ID3D11Resource*, UINT /*DstSubresource*/, UINT /*DstX*/, UINT /*DstY*/, UINT /*DstZ*/, ID3D11Resource*,
            UINT /*SrcSubresource*/, const D3D11_BOX*, UINT /*CopyFlags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void UpdateSubresource1(ID3D11Resource*, UINT /*DstSubresource*/, const D3D11_BOX*, const void*, UINT /*SrcRowPitch*/, UINT /*SrcDepthPitch*/, UINT /*CopyFlags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DiscardResource(ID3D11Resource*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DiscardView(ID3D11View*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VSSetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer* const*, const UINT* /*pFirstConstant*/, const UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void HSSetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer* const*, const UINT* /*pFirstConstant*/, const UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DSSetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer* const*, const UINT* /*pFirstConstant*/, const UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GSSetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer* const*, const UINT* /*pFirstConstant*/, const UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void PSSetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer* const*, const UINT* /*pFirstConstant*/, const UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSSetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer* const*, const UINT* /*pFirstConstant*/, const UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VSGetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer**, UINT* /*pFirstConstant*/, UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void HSGetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer**, UINT* /*pFirstConstant*/, UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DSGetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer**, UINT* /*pFirstConstant*/, UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GSGetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer**, UINT* /*pFirstConstant*/, UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void PSGetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer**, UINT* /*pFirstConstant*/, UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CSGetConstantBuffers1(UINT /*StartSlot*/, UINT /*NumBuffers*/, ID3D11Buffer**, UINT* /*pFirstConstant*/, UINT* /*pNumConstants*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SwapDeviceContextState(ID3DDeviceContextState*, ID3DDeviceContextState** /*ppPreviousState*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ClearView(ID3D11View*, const FLOAT /*Color*/[4], const D3D11_RECT*, UINT /*NumRects*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DiscardView1(ID3D11View*, const D3D11_RECT*, UINT /*NumRects*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //ID3D11DeviceContext2
        HRESULT UpdateTileMappings(ID3D11Resource*, UINT /*NumTiledResourceRegions*/, const D3D11_TILED_RESOURCE_COORDINATE*, const D3D11_TILE_REGION_SIZE*,
            ID3D11Buffer*, UINT /*NumRanges*/, const UINT* /*pRangeFlags*/, const UINT* /*pTilePoolStartOffsets*/, const UINT* /*pRangeTileCounts*/, UINT /*Flags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CopyTileMappings(ID3D11Resource*, const D3D11_TILED_RESOURCE_COORDINATE*, ID3D11Resource*,
            const D3D11_TILED_RESOURCE_COORDINATE*, const D3D11_TILE_REGION_SIZE*, UINT /*Flags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopyTiles(ID3D11Resource*, const D3D11_TILED_RESOURCE_COORDINATE*, const D3D11_TILE_REGION_SIZE*, ID3D11Buffer*, UINT64 /*BufferStartOffsetInBytes*/, UINT /*Flags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void UpdateTiles(ID3D11Resource*, const D3D11_TILED_RESOURCE_COORDINATE*, const D3D11_TILE_REGION_SIZE*, const void*, UINT /*Flags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT ResizeTilePool(ID3D11Buffer*, UINT64 /*NewSizeInBytes*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void TiledResourceBarrier(ID3D11DeviceChild*, ID3D11DeviceChild*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        BOOL IsAnnotationEnabled() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetMarkerInt(LPCWSTR /*pLabel*/, INT /*Data*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void BeginEventInt(LPCWSTR /*pLabel*/, INT /*Data*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void EndEvent() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //ID3D11DeviceContext3
        void Flush1(D3D11_CONTEXT_TYPE, HANDLE /*hEvent*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetHardwareProtectionState(BOOL /*HwProtectionEnable*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GetHardwareProtectionState(BOOL* /*pHwProtectionEnable*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //ID3D11DeviceContext4
        HRESULT Signal(ID3D11Fence*, UINT64 /*Value*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT Wait(ID3D11Fence*, UINT64 /*Value*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

    struct context_multithread_impl
        : ID3D10Multithread
    {
        using type = ID3D10Multithread;

        void Enter() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void Leave() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        BOOL SetMultithreadProtected(BOOL) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        BOOL GetMultithreadProtected() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
