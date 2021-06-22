// Copyright (c) 2007-2021 Intel Corporation
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

#if defined  (MFX_D3D11_ENABLED) && defined (MFX_DX9ON11)
#ifndef __LIBMFX_CORE__D3D9ON11_H__
#define __LIBMFX_CORE__D3D9ON11_H__

#include <d3d9.h>
#include <d3d11.h>
#include <d3d9on12.h>
#include <libmfx_core_d3d11.h>

// DX9ON11 wrapper
template <class Base>
class D3D9ON11VideoCORE_T : public Base
{
    friend class FactoryCORE;

public:

    virtual                      ~D3D9ON11VideoCORE_T();
    virtual mfxStatus            CreateVA(mfxVideoParam* param, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response, UMC::FrameAllocator* allocator) override;

    virtual mfxStatus            SetHandle(mfxHandleType type, mfxHDL handle) override;
    virtual mfxStatus            GetHandle(mfxHandleType type, mfxHDL* handle) override;
    virtual void*                QueryCoreInterface(const MFX_GUID& guid)     override;

    virtual mfxStatus            AllocFrames(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response, bool isNeedCopy = true)  override;

private:
    D3D9ON11VideoCORE_T(
        const mfxU32 adapterNum
        , const mfxU32 numThreadsAvailable
        , const mfxSession session = NULL
    );

    virtual mfxStatus            DoFastCopyExtended(mfxFrameSurface1* pDst, mfxFrameSurface1* pSrc) override;
    virtual mfxStatus            DoFastCopyWrapper(mfxFrameSurface1* pDst, mfxU16 dstMemType, mfxFrameSurface1* pSrc, mfxU16 srcMemType) override;

    void                         ReleaseHandle();

    mfxStatus                    CopyDX11toDX9(mfxFrameSurface1& dst, mfxFrameSurface1& src);
    mfxStatus                    CopyDX9toDX11(mfxFrameSurface1& dst, mfxFrameSurface1& src);

    mfxStatus                    CopyD3D12(CComPtr<ID3D12Resource>& dst, UINT dstSubresourceIdx, CComPtr<ID3D12Resource>& src, UINT srcSubresourceIdx);

    HANDLE                       m_hDirectXHandle; // if m_pDirect3DDeviceManager was used
    IDirect3DDeviceManager9*     m_pDirect3DDeviceManager;

    static bool                  m_IsD3D9On11Core;

    CComPtr<IDirect3DDevice9On12>       m_d3dOn12;
    CComPtr<ID3D12Device>               m_d3d12;
    CComPtr<ID3D12CommandAllocator>     m_commandAllocator;
    CComPtr<ID3D12GraphicsCommandList>  m_commandList;
    CComPtr<ID3D12CommandQueue>         m_commandQueue;
    CComPtr<ID3D12Fence>                m_fence;
    UINT64                              m_signalValue = 1;
    HANDLE                              m_FenceEvent;

    static std::mutex            m_copyMutexD3d9;
    std::mutex                   m_copyMutexD3d12;
};

using D3D9ON11VideoCORE = D3D9ON11VideoCORE_T<D3D11VideoCORE>;

#endif
#endif
