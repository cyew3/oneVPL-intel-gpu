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
#include <libmfx_core_d3d11.h>

#include "d3d11_decode_accelerator.h"
#include "mfx_check_hardware_support.h"

#include "libmfx_core.h"
#include "encoding_ddi.h"
#include "libmfx_allocator_d3d11.h"

#include <d3d11.h>
#include <d3d9.h>
#include <atlbase.h>

#include <memory>

#if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
#include "d3d11_video_processor.h"
#endif
// disable the "conditional expression is constant" warning
#pragma warning(disable: 4127)

class CmCopyWrapper;

// DX9ON11 wrapper
template <class Base>
class D3D9ON11VideoCORE_T : public Base
{
    friend class FactoryCORE;

public:

    virtual                      ~D3D9ON11VideoCORE_T();

    virtual mfxStatus            CreateVA(mfxVideoParam* param, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response, UMC::FrameAllocator* allocator) override;
    virtual mfxStatus            SetFrameAllocator(mfxFrameAllocator* allocator) override;
    virtual mfxStatus            SetHandle(mfxHandleType type, mfxHDL handle) override;
    virtual mfxStatus            GetHandle(mfxHandleType type, mfxHDL* handle) override;
    virtual void*                QueryCoreInterface(const MFX_GUID& guid)     override;

    virtual mfxStatus            AllocFrames(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response, bool isNeedCopy = true)  override;

    mfxMemId                     WrapSurface(mfxMemId dx9Surface, const mfxFrameAllocResponse& dx11Surfaces, bool isNeedRewrap = false);
    mfxMemId                     UnWrapSurface(mfxMemId wrappedSurface, bool isNeedUnwrap = false);
    mfxStatus                    CopyDX11toDX9(const mfxMemId dx11MemId, const mfxMemId dx9MemId);
    mfxStatus                    CopyDX9toDX11(const mfxMemId dx9MemId, const mfxMemId dx11MemId);

private:
    D3D9ON11VideoCORE_T(
        const mfxU32 adapterNum
        , const mfxU32 numThreadsAvailable
        , const mfxSession session = NULL
    );

    virtual mfxStatus            DoFastCopyExtended(mfxFrameSurface1* pDst, mfxFrameSurface1* pSrc) override;
    virtual mfxStatus            DoFastCopyWrapper(mfxFrameSurface1* pDst, mfxU16 dstMemType, mfxFrameSurface1* pSrc, mfxU16 srcMemType) override;

    void                         ReleaseHandle();

    HANDLE                       m_hDirectXHandle; // if m_pDirect3DDeviceManager was used
    IDirect3DDeviceManager9*     m_pDirect3DDeviceManager;
    bool                         m_hasDX9FrameAllocator;
    mfxFrameAllocator            m_dx9FrameAllocator;
    std::map<mfxMemId, mfxMemId> m_dx9MemIdMap;
    std::map<mfxMemId, mfxMemId> m_dx9MemIdUsed; //for lightweight opposite-mapping
    static std::mutex            m_copyMutex;

    static bool                  m_IsD3D9On11Core;
};

using D3D9ON11VideoCORE = D3D9ON11VideoCORE_T<D3D11VideoCORE>;


class MfxWrapController
{
public:
    MfxWrapController();
    ~MfxWrapController();
    MfxWrapController(const MfxWrapController&) = delete;
    MfxWrapController& operator=(const MfxWrapController&) = delete;

    mfxStatus Alloc(VideoCORE* core, const mfxU16& numFrameMin, const mfxVideoParam& par, const mfxU16& reqType);
    mfxStatus WrapSurface(mfxFrameSurface1* dx9Surface, mfxHDL* frameHDL);
    mfxStatus UnwrapSurface(mfxFrameSurface1* dx9Surface);
private:
    bool      isDX9ON11Wrapper();
    mfxStatus Lock(mfxU32 idx);
    bool      IsLocked(mfxU32 idx);
    mfxStatus Unlock(mfxU32 idx);
    mfxStatus AcquireResource(mfxMemId& dx11MemId);
    mfxStatus ReleaseResource(mfxMemId dx11MemId);
    mfxStatus ConfigureRequest(mfxFrameAllocRequest& req, const mfxVideoParam& par, const mfxU16& numFrameMin, const mfxU16& reqType);
    bool      IsInputSurface();
    bool      IsOutputSurface();

    D3D9ON11VideoCORE*                 m_pDX9ON11Core;
    std::vector<mfxFrameAllocResponse> m_responseQueue;
    std::vector<mfxMemId>              m_mids;
    std::vector<mfxU32>                m_locked;
    std::map<mfxMemId, mfxMemId>       m_dx9Todx11;
    mfxU16                             m_originalReqType;
};

#endif
#endif
