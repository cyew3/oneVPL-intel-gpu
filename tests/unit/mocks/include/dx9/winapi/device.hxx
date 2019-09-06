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
    struct device_impl
        : unknown_impl<IDirect3DDevice9>
    {
        HRESULT TestCooperativeLevel() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        UINT GetAvailableTextureMem() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT EvictManagedResources() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDirect3D(IDirect3D9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDeviceCaps(D3DCAPS9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDisplayMode(UINT /*iSwapChain*/, D3DDISPLAYMODE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetCursorProperties(UINT /*XHotSpot*/, UINT /*YHotSpot*/, IDirect3DSurface9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetCursorPosition(int /*X*/, int /*Y*/, DWORD /*Flags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        BOOL ShowCursor(BOOL /*bShow*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS*, IDirect3DSwapChain9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetSwapChain(UINT /*iSwapChain*/, IDirect3DSwapChain9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        UINT GetNumberOfSwapChains() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT Reset(D3DPRESENT_PARAMETERS*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT Present(CONST RECT* /*pSourceRect*/, CONST RECT* /*pDestRect*/, HWND /*hDestWindowOverride*/, CONST RGNDATA* /*pDirtyRegion*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetBackBuffer(UINT /*iSwapChain*/, UINT /*iBackBuffer*/, D3DBACKBUFFER_TYPE, IDirect3DSurface9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetRasterStatus(UINT /*iSwapChain*/, D3DRASTER_STATUS*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetDialogBoxMode(BOOL /*bEnableDialogs*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetGammaRamp(UINT /*iSwapChain*/, DWORD /*Flags*/, CONST D3DGAMMARAMP*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GetGammaRamp(UINT /*iSwapChain*/, D3DGAMMARAMP*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateTexture(UINT /*Width*/, UINT /*Height*/, UINT /*Levels*/, DWORD /*Usage*/, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE* /*pSharedHandle*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVolumeTexture(UINT /*Width*/, UINT /*Height*/, UINT /*Depth*/, UINT /*Levels*/, DWORD /*Usage*/, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9**, HANDLE* /*pSharedHandle*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateCubeTexture(UINT /*EdgeLength*/, UINT /*Levels*/, DWORD /*Usage*/, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9**, HANDLE* /*pSharedHandle*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVertexBuffer(UINT /*Length*/, DWORD /*Usage*/, DWORD /*FVF*/, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE* /*pSharedHandle*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateIndexBuffer(UINT /*Length*/, DWORD /*Usage*/, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9**, HANDLE* /*pSharedHandle*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateRenderTarget(UINT /*Width*/, UINT /*Height*/, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD /*MultisampleQuality*/, BOOL /*Lockable*/, IDirect3DSurface9**, HANDLE* /*pSharedHandle*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateDepthStencilSurface(UINT /*Width*/, UINT /*Height*/, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD /*MultisampleQuality*/, BOOL /*Discard*/, IDirect3DSurface9**, HANDLE* /*pSharedHandle*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT UpdateSurface(IDirect3DSurface9* /*pSourceSurface*/, CONST RECT* /*pSourceRect*/, IDirect3DSurface9* /*pDestinationSurface*/, CONST POINT* /*pDestPoint*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT UpdateTexture(IDirect3DBaseTexture9* /*pSourceTexture*/, IDirect3DBaseTexture9* /*pDestinationTexture*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetRenderTargetData(IDirect3DSurface9* /*pRenderTarget*/, IDirect3DSurface9* /*pDestSurface*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetFrontBufferData(UINT /*iSwapChain*/, IDirect3DSurface9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT StretchRect(IDirect3DSurface9* /*pSourceSurface*/, CONST RECT* /*pSourceRect*/, IDirect3DSurface9* /*pDestSurface*/, CONST RECT* /*pDestRect*/, D3DTEXTUREFILTERTYPE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT ColorFill(IDirect3DSurface9*, CONST RECT*, D3DCOLOR) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateOffscreenPlainSurface(UINT /*Width*/, UINT /*Height*/, D3DFORMAT, D3DPOOL, IDirect3DSurface9**, HANDLE* /*pSharedHandle*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetRenderTarget(DWORD /*RenderTargetIndex*/, IDirect3DSurface9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetRenderTarget(DWORD /*RenderTargetIndex*/, IDirect3DSurface9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetDepthStencilSurface(IDirect3DSurface9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetDepthStencilSurface(IDirect3DSurface9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT BeginScene() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT EndScene() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT Clear(DWORD /*Count*/, CONST D3DRECT*, DWORD /*Flags*/, D3DCOLOR, float /*Z*/, DWORD /*Stencil*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetTransform(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetTransform(D3DTRANSFORMSTATETYPE, D3DMATRIX*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT MultiplyTransform(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetViewport(CONST D3DVIEWPORT9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetViewport(D3DVIEWPORT9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetMaterial(CONST D3DMATERIAL9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetMaterial(D3DMATERIAL9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetLight(DWORD /*Index*/, CONST D3DLIGHT9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetLight(DWORD /*Index*/, D3DLIGHT9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT LightEnable(DWORD /*Index*/, BOOL /*Enable*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetLightEnable(DWORD /*Index*/, BOOL* /*pEnable*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetClipPlane(DWORD /*Index*/, CONST float* /*pPlane*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetClipPlane(DWORD /*Index*/, float* /*pPlane*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetRenderState(D3DRENDERSTATETYPE, DWORD /*Value*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetRenderState(D3DRENDERSTATETYPE, DWORD* /*pValue*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateStateBlock(D3DSTATEBLOCKTYPE, IDirect3DStateBlock9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT BeginStateBlock() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT EndStateBlock(IDirect3DStateBlock9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetClipStatus(CONST D3DCLIPSTATUS9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetClipStatus(D3DCLIPSTATUS9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetTexture(DWORD /*Stage*/, IDirect3DBaseTexture9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetTexture(DWORD /*Stage*/, IDirect3DBaseTexture9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetTextureStageState(DWORD /*Stage*/, D3DTEXTURESTAGESTATETYPE, DWORD* /*pValue*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetTextureStageState(DWORD /*Stage*/, D3DTEXTURESTAGESTATETYPE, DWORD /*Value*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetSamplerState(DWORD /*Sampler*/, D3DSAMPLERSTATETYPE, DWORD* /*pValue*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetSamplerState(DWORD /*Sampler*/, D3DSAMPLERSTATETYPE, DWORD /*Value*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT ValidateDevice(DWORD* /*pNumPasses*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPaletteEntries(UINT /*PaletteNumber*/, CONST PALETTEENTRY*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetPaletteEntries(UINT /*PaletteNumber*/, PALETTEENTRY*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetCurrentTexturePalette(UINT /*PaletteNumber*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetCurrentTexturePalette(UINT* /*PaletteNumber*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetScissorRect(CONST RECT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetScissorRect(RECT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetSoftwareVertexProcessing(BOOL /*bSoftware*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        BOOL GetSoftwareVertexProcessing() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetNPatchMode(float /*nSegments*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        float GetNPatchMode() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT DrawPrimitive(D3DPRIMITIVETYPE /*PrimitiveType*/, UINT /*StartVertex*/, UINT /*PrimitiveCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE, INT /*BaseVertexIndex*/, UINT /*MinVertexIndex*/, UINT /*NumVertices*/, UINT /*startIndex*/, UINT /*primCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE, UINT /*PrimitiveCount*/, CONST void* /*pVertexStreamZeroData*/, UINT /*VertexStreamZeroStride*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE, UINT /*MinVertexIndex*/, UINT /*NumVertices*/, UINT /*PrimitiveCount*/, CONST void* /*pIndexData*/, D3DFORMAT, CONST void* /*pVertexStreamZeroData*/, UINT /*VertexStreamZeroStride*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT ProcessVertices(UINT /*SrcStartIndex*/, UINT /*DestIndex*/, UINT /*VertexCount*/, IDirect3DVertexBuffer9*, IDirect3DVertexDeclaration9*, DWORD /*Flags*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVertexDeclaration(CONST D3DVERTEXELEMENT9*, IDirect3DVertexDeclaration9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetVertexDeclaration(IDirect3DVertexDeclaration9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVertexDeclaration(IDirect3DVertexDeclaration9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetFVF(DWORD) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetFVF(DWORD*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateVertexShader(CONST DWORD* /*pFunction*/, IDirect3DVertexShader9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetVertexShader(IDirect3DVertexShader9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVertexShader(IDirect3DVertexShader9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetVertexShaderConstantF(UINT /*StartRegister*/, CONST float* /*pConstantData*/, UINT /*Vector4fCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVertexShaderConstantF(UINT /*StartRegister*/, float* /*pConstantData*/, UINT /*Vector4fCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetVertexShaderConstantI(UINT /*StartRegister*/, CONST int* /*pConstantData*/, UINT /*Vector4iCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVertexShaderConstantI(UINT /*StartRegister*/, int* /*pConstantData*/, UINT /*Vector4iCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetVertexShaderConstantB(UINT /*StartRegister*/, CONST BOOL* /*pConstantData*/, UINT  /*BoolCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetVertexShaderConstantB(UINT /*StartRegister*/, BOOL* /*pConstantData*/, UINT /*BoolCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetStreamSource(UINT /*StreamNumber*/, IDirect3DVertexBuffer9* /*pStreamData*/, UINT /*OffsetInBytes*/, UINT /*Stride*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetStreamSource(UINT /*StreamNumber*/, IDirect3DVertexBuffer9** /*ppStreamData*/, UINT* /*pOffsetInBytes*/, UINT* /*pStride*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetStreamSourceFreq(UINT /*StreamNumber*/, UINT /*Setting*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetStreamSourceFreq(UINT /*StreamNumber*/, UINT* /*pSetting*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetIndices(IDirect3DIndexBuffer9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetIndices(IDirect3DIndexBuffer9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreatePixelShader(CONST DWORD* /*pFunction*/, IDirect3DPixelShader9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPixelShader(IDirect3DPixelShader9*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetPixelShader(IDirect3DPixelShader9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPixelShaderConstantF(UINT /*StartRegister*/, CONST float* /*pConstantData*/, UINT /*Vector4fCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetPixelShaderConstantF(UINT /*StartRegister*/, float* /*pConstantData*/, UINT /*Vector4fCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPixelShaderConstantI(UINT /*StartRegister*/, CONST int* /*pConstantData*/, UINT /*Vector4iCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetPixelShaderConstantI(UINT /*StartRegister*/, int* /*pConstantData*/, UINT /*Vector4iCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SetPixelShaderConstantB(UINT /*StartRegister*/, CONST BOOL* /*pConstantData*/, UINT  /*BoolCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetPixelShaderConstantB(UINT /*StartRegister*/, BOOL* /*pConstantData*/, UINT /*BoolCount*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT DrawRectPatch(UINT /*Handle*/, CONST float* /*pNumSegs*/, CONST D3DRECTPATCH_INFO* /*pRectPatchInfo*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT DrawTriPatch(UINT /*Handle*/, CONST float* /*pNumSegs*/, CONST D3DTRIPATCH_INFO* /*pTriPatchInfo*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT DeletePatch(UINT /*Handle*/) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT CreateQuery(D3DQUERYTYPE, IDirect3DQuery9**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };

} } }
