// Copyright (c) 2007-2020 Intel Corporation
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

#if defined  (MFX_D3D11_ENABLED)
#ifndef __LIBMFX_CORE__D3D11_H__
#define __LIBMFX_CORE__D3D11_H__


#include "d3d11_decode_accelerator.h"
#include "mfx_check_hardware_support.h"


#include "libmfx_core.h"
#include "encoding_ddi.h"
#include "libmfx_allocator_d3d11.h"
#include <d3d11.h>

#include <memory>

#if defined (MFX_ENABLE_VPP)
#include "d3d11_video_processor.h"
#endif
// disable the "conditional expression is constant" warning
#pragma warning(disable: 4127)
#include "mfx_session.h"

class CmCopyWrapper;

// DX11 support
template <class Base>
class D3D11VideoCORE_T : public Base
{
    friend class FactoryCORE;
    friend class D3D11Adapter;
    friend class D3D11VideoCORE20;
    friend class deprecate_from_base<D3D11VideoCORE_T<CommonCORE20>>;
    class D3D11Adapter : public D3D11Interface
    {
    public:
        D3D11Adapter(D3D11VideoCORE_T *pD3D11Core)
            : m_pD3D11Core(pD3D11Core)
        {
        }

        ID3D11Device * GetD3D11Device(bool isTemporal = false)
        {
            m_pD3D11Core->InitializeDevice(isTemporal);
            return m_pD3D11Core->m_pD11Device;
        }

        ID3D11VideoDevice * GetD3D11VideoDevice(bool isTemporal = false)
        {
            m_pD3D11Core->InitializeDevice(isTemporal);
            return m_pD3D11Core->m_pD11VideoDevice;
        }
    
        ID3D11DeviceContext * GetD3D11DeviceContext(bool isTemporal = false)
        {
            m_pD3D11Core->InitializeDevice(isTemporal);
            return m_pD3D11Core->m_pD11Context;
        }

        ID3D11VideoContext * GetD3D11VideoContext(bool isTemporal = false)
        {
            m_pD3D11Core->InitializeDevice(isTemporal);
            return m_pD3D11Core->m_pD11VideoContext;
        }
    protected:
        D3D11VideoCORE_T *m_pD3D11Core;
    };

    class CMEnabledCoreAdapter : public CMEnabledCoreInterface
    {
    public:
        CMEnabledCoreAdapter(D3D11VideoCORE_T *pD3D11Core): m_pD3D11Core(pD3D11Core)
        {
        }
        virtual mfxStatus SetCmCopyStatus(bool enable) override
        {
            return m_pD3D11Core->SetCmCopyStatus(enable);
        }
    protected:
        D3D11VideoCORE_T *m_pD3D11Core;
    };

public:

    virtual ~D3D11VideoCORE_T();

    virtual mfxStatus CreateVA(mfxVideoParam * param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, UMC::FrameAllocator *allocator) override;

    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool isNeedCopy = true)                             override;

            mfxStatus ReallocFrame(mfxFrameSurface1 *surf);

    virtual void      GetVA(mfxHDL* phdl, mfxU16 type) override
    {
        if (!phdl) return;

        if (type & MFX_MEMTYPE_FROM_DECODE)
            (*phdl = m_pAccelerator.get());
#if defined (MFX_ENABLE_VPP)
        else if (type & (MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_FROM_VPPOUT))
            (*phdl = &m_vpp_hw_resmng);
#endif
        else
            (*phdl = 0);
    };

    virtual eMFXPlatform GetPlatformType() override { return  MFX_PLATFORM_HARDWARE; }

    virtual eMFXVAType   GetVAType() const override { return MFX_HW_D3D11; }
#if defined (MFX_ENABLE_VPP)
    virtual void         GetVideoProcessing(mfxHDL* phdl) override
    {
        if (!phdl) return;

        *phdl = &m_vpp_hw_resmng;
    }
#endif
    virtual mfxStatus    CreateVideoProcessing(mfxVideoParam * param) override;

    virtual void*        QueryCoreInterface(const MFX_GUID &guid)     override;

    virtual eMFXHWType   GetHWType()                                  override;

    virtual mfxStatus    IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncoder = false) override;
    mfxStatus  GetIntelDataPrivateReport(const GUID guid, mfxVideoParam *par, D3D11_VIDEO_DECODER_CONFIG & config);

    virtual mfxStatus    SetHandle(mfxHandleType type, mfxHDL  handle) override;
    virtual mfxStatus    GetHandle(mfxHandleType type, mfxHDL *handle) override;

    virtual mfxU16       GetAutoAsyncDepth() override { return MFX_AUTO_ASYNC_DEPTH_VALUE; }; //it can be platform based

    virtual bool         IsCompatibleForOpaq() override;

protected:

    D3D11VideoCORE_T(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = nullptr);

    mfxStatus InternalInit();
    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)                                      override;
    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType) override;

    std::unique_ptr<D3D11Adapter> m_pid3d11Adapter;

    mfxStatus InitializeDevice(bool isTemporal = false);
    mfxStatus InternalCreateDevice();
    virtual mfxStatus DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)                      override;
    mfxStatus ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc);
    // this function should not be virtual
    mfxStatus SetCmCopyStatus(bool enable);
    mfxStatus ConvertYV12toNV12SW(mfxFrameSurface1* pDst, mfxFrameSurface1* pSrc);

    D3D11_VIDEO_DECODER_CONFIG* GetConfig(D3D11_VIDEO_DECODER_DESC *video_desc, mfxU32 start, mfxU32 end, const GUID guid);

    bool                                                               m_bUseExtAllocForHWFrames;
    std::unique_ptr<mfxDefaultAllocatorD3D11::mfxWideHWFrameAllocator> m_pcHWAlloc;

    // D3D11 services
    CComPtr<ID3D11Device>                   m_pD11Device;
    CComPtr<ID3D11DeviceContext>            m_pD11Context;
    CComPtr<IDXGIFactory>                   m_pFactory;
    CComPtr<IDXGIAdapter>                   m_pAdapter;

    CComQIPtr<ID3D11VideoDevice>            m_pD11VideoDevice;
    CComQIPtr<ID3D11VideoContext>           m_pD11VideoContext;

    // D3D11 VideoAccelrator which works with decode components on MFX/UMC levels
    // and providing HW capabilities
    std::unique_ptr<MFXD3D11Accelerator>    m_pAccelerator;
    #if defined (MFX_ENABLE_VPP)
    VPPHWResMng                             m_vpp_hw_resmng;
    #endif
    eMFXHWType                              m_HWType;
    eMFXGTConfig                            m_GTConfig;
    // Ordinal number of adapter to work
    const mfxU32                            m_adapterNum;
    ComPtrCore<ID3D11VideoDecoder>          m_comptr;
    bool                                    m_bCmCopy;
    bool                                    m_bCmCopySwap;
    bool                                    m_bCmCopyAllowed;
    std::unique_ptr<CmCopyWrapper>          m_pCmCopy;
    std::unique_ptr<CMEnabledCoreAdapter>   m_pCmAdapter;
    mfxU32                                  m_VideoDecoderConfigCount;
    std::vector<D3D11_VIDEO_DECODER_CONFIG> m_Configs;
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
    bool                                    m_bIsBlockingTaskSyncEnabled;
#endif
};

using D3D11VideoCORE = D3D11VideoCORE_T<CommonCORE>;

using D3D11VideoCORE20_base = deprecate_from_base<D3D11VideoCORE_T<CommonCORE20>>;

class D3D11VideoCORE20 : public D3D11VideoCORE20_base
{
public:
    friend class FactoryCORE;

    virtual ~D3D11VideoCORE20();

    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool isNeedCopy = true)     override;

    mfxStatus ReallocFrame(mfxFrameSurface1 *surf);

    virtual mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* &surf)                           override;

private:
    D3D11VideoCORE20(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = nullptr);

    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)                                      override;
    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType) override;
};

#endif
#endif
