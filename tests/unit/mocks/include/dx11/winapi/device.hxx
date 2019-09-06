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

#include <d3d11_4.h>

namespace mocks { namespace dx11 { namespace winapi
{
    struct device_impl
        : ID3D11Device5
    {
        using type = ID3D11Device5;

        HRESULT CreateBuffer(const D3D11_BUFFER_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Buffer**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateTexture1D(const D3D11_TEXTURE1D_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture1D**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateTexture2D(const D3D11_TEXTURE2D_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture2D**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateTexture3D(const D3D11_TEXTURE3D_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture3D**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateShaderResourceView(ID3D11Resource*, const D3D11_SHADER_RESOURCE_VIEW_DESC*, ID3D11ShaderResourceView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateUnorderedAccessView(ID3D11Resource*, const D3D11_UNORDERED_ACCESS_VIEW_DESC*, ID3D11UnorderedAccessView **) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateRenderTargetView(ID3D11Resource*, const D3D11_RENDER_TARGET_VIEW_DESC*, ID3D11RenderTargetView**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDepthStencilView(ID3D11Resource*, const D3D11_DEPTH_STENCIL_VIEW_DESC*, ID3D11DepthStencilView **) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC*, UINT, const void*, SIZE_T, ID3D11InputLayout **) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVertexShader(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11VertexShader **) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateGeometryShader(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11GeometryShader**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateGeometryShaderWithStreamOutput(const void*, SIZE_T, const D3D11_SO_DECLARATION_ENTRY*, UINT, const UINT*, UINT, UINT, ID3D11ClassLinkage*, ID3D11GeometryShader**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreatePixelShader(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11PixelShader**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateHullShader(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11HullShader**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDomainShader(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11DomainShader**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateComputeShader(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11ComputeShader**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateClassLinkage(ID3D11ClassLinkage**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateBlendState(const D3D11_BLEND_DESC*, ID3D11BlendState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC*, ID3D11DepthStencilState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateRasterizerState(const D3D11_RASTERIZER_DESC*, ID3D11RasterizerState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateSamplerState(const D3D11_SAMPLER_DESC*, ID3D11SamplerState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateQuery(const D3D11_QUERY_DESC*, ID3D11Query**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreatePredicate(const D3D11_QUERY_DESC*, ID3D11Predicate**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateCounter(const D3D11_COUNTER_DESC*, ID3D11Counter**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDeferredContext(UINT, ID3D11DeviceContext**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT OpenSharedResource(HANDLE, REFIID, void**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckFormatSupport(DXGI_FORMAT, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckMultisampleQualityLevels(DXGI_FORMAT, UINT, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CheckCounterInfo(D3D11_COUNTER_INFO*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckCounter(const D3D11_COUNTER_DESC*, D3D11_COUNTER_TYPE*, UINT*, LPSTR, UINT*, LPSTR, UINT*, LPSTR, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckFeatureSupport(D3D11_FEATURE, void*, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetPrivateData(REFGUID, UINT*, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPrivateData(REFGUID, UINT, const void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPrivateDataInterface(REFGUID, const IUnknown*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        D3D_FEATURE_LEVEL GetFeatureLevel() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        UINT GetCreationFlags() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDeviceRemovedReason() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GetImmediateContext(ID3D11DeviceContext**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetExceptionMode(UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        UINT GetExceptionMode() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //ID3D11Device1
        void GetImmediateContext1(ID3D11DeviceContext1**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDeferredContext1(UINT /*ContextFlags*/, ID3D11DeviceContext1**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateBlendState1(const D3D11_BLEND_DESC1*, ID3D11BlendState1**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateRasterizerState1(const D3D11_RASTERIZER_DESC1*, ID3D11RasterizerState1**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDeviceContextState(UINT /*Flags*/, const D3D_FEATURE_LEVEL*, UINT /*FeatureLevels*/, UINT /*SDKVersion*/,
            REFIID /*EmulatedInterface*/, D3D_FEATURE_LEVEL* /*pChosenFeatureLevel*/, ID3DDeviceContextState**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT OpenSharedResource1(HANDLE, REFIID /*returnedInterface*/, void**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT OpenSharedResourceByName(LPCWSTR, DWORD /*dwDesiredAccess*/, REFIID /*returnedInterface*/, void**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //ID3D11Device2
        void GetImmediateContext2(ID3D11DeviceContext2**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDeferredContext2(UINT /*ContextFlags*/, ID3D11DeviceContext2**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GetResourceTiling(ID3D11Resource*, UINT* /*pNumTilesForEntireResource*/, D3D11_PACKED_MIP_DESC*, D3D11_TILE_SHAPE*,
            UINT* /*pNumSubresourceTilings*/, UINT /*FirstSubresourceTilingToGet*/, D3D11_SUBRESOURCE_TILING*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CheckMultisampleQualityLevels1(DXGI_FORMAT, UINT /*SampleCount*/, UINT /*Flags*/, UINT* /*pNumQualityLevels*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //ID3D11Device3
        HRESULT CreateTexture2D1(const D3D11_TEXTURE2D_DESC1*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture2D1**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateTexture3D1(const D3D11_TEXTURE3D_DESC1*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture3D1**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateRasterizerState2(const D3D11_RASTERIZER_DESC2*, ID3D11RasterizerState2**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateShaderResourceView1(ID3D11Resource*, const D3D11_SHADER_RESOURCE_VIEW_DESC1*, ID3D11ShaderResourceView1**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateUnorderedAccessView1(ID3D11Resource*, const D3D11_UNORDERED_ACCESS_VIEW_DESC1*, ID3D11UnorderedAccessView1**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateRenderTargetView1(ID3D11Resource*, const D3D11_RENDER_TARGET_VIEW_DESC1*, ID3D11RenderTargetView1**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateQuery1(const D3D11_QUERY_DESC1*, ID3D11Query1**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GetImmediateContext3(ID3D11DeviceContext3**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDeferredContext3(UINT /*ContextFlags*/, ID3D11DeviceContext3**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void WriteToSubresource(ID3D11Resource*, UINT /*DstSubresource*/, const D3D11_BOX*, const void* /*pSrcData*/, UINT /*SrcRowPitch*/, UINT /*SrcDepthPitch*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ReadFromSubresource(void* /*pDstData*/, UINT /*DstRowPitch*/, UINT /*DstDepthPitch*/, ID3D11Resource* /*pSrcResource*/, UINT /*SrcSubresource*/, const D3D11_BOX*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //ID3D11Device4
        HRESULT RegisterDeviceRemovedEvent(HANDLE /*hEvent*/, DWORD* /*pdwCookie*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void UnregisterDeviceRemoved(DWORD /*dwCookie*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }

        //ID3D11Device5
        HRESULT OpenSharedFence(HANDLE /*hFence*/, REFIID, void**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateFence(UINT64 /*InitialValue*/, D3D11_FENCE_FLAG /*Flags*/, REFIID, void**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
