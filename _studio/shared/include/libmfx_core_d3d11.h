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

#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)
#ifndef __LIBMFX_CORE__D3D11_H__
#define __LIBMFX_CORE__D3D11_H__


#include "d3d11_decode_accelerator.h"
#include "mfx_check_hardware_support.h"


#include "libmfx_core.h"
#include "encoding_ddi.h"
#include "libmfx_allocator_d3d11.h"
#if defined (MFX_ENABLE_MFE)
#include "mfx_mfe_adapter_dxva.h"
#endif
#include <d3d11.h>

#include <memory>

#if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
#include "d3d11_video_processor.h"
#endif
// disable the "conditional expression is constant" warning
#pragma warning(disable: 4127)

class CmCopyWrapper;

// DX11 support
class D3D11VideoCORE : public CommonCORE
{
    friend class FactoryCORE;
    friend class D3D11Adapter;
    class D3D11Adapter : public D3D11Interface
    {
    public:
        D3D11Adapter(D3D11VideoCORE *pD3D11Core) 
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
        D3D11VideoCORE *m_pD3D11Core;
    };

    class CMEnabledCoreAdapter : public CMEnabledCoreInterface
    {
    public:
        CMEnabledCoreAdapter(D3D11VideoCORE *pD3D11Core): m_pD3D11Core(pD3D11Core)
        {
        };
        virtual mfxStatus SetCmCopyStatus(bool enable)
        {
            return m_pD3D11Core->SetCmCopyStatus(enable);
        };
    protected:
        D3D11VideoCORE *m_pD3D11Core;
    };


public:

    virtual ~D3D11VideoCORE();


    virtual mfxStatus  CreateVA(mfxVideoParam * param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, UMC::FrameAllocator *allocator);

   
    // Get the current working adapter's number
    //virtual mfxU32 GetAdapterNumber(void) {return m_adapterNum;}
    


    //virtual mfxStatus     SetHandle(mfxHandleType type, mfxHDL handle);
    virtual mfxStatus     AllocFrames(mfxFrameAllocRequest *request, 
                                      mfxFrameAllocResponse *response, bool isNeedCopy = true);

    virtual mfxStatus ReallocFrame(mfxFrameSurface1 *surf);

    virtual void          GetVA(mfxHDL* phdl, mfxU16 type) 
    {
        if (type & MFX_MEMTYPE_FROM_DECODE)
            (*phdl = m_pAccelerator.get());
        else if ((type & MFX_MEMTYPE_FROM_VPPIN) || (type & MFX_MEMTYPE_FROM_VPPOUT))
            (*phdl = &m_vpp_hw_resmng);
        else
            (*phdl = 0);
    };
    
    virtual eMFXPlatform  GetPlatformType() {return  MFX_PLATFORM_HARDWARE;}
    
    virtual eMFXVAType   GetVAType() const {return MFX_HW_D3D11; };
    #if defined (MFX_ENABLE_VPP)&& !defined(MFX_RT)
    virtual void  GetVideoProcessing(mfxHDL* phdl) 
    {
        *phdl = &m_vpp_hw_resmng;
    };
    #endif
    mfxStatus  CreateVideoProcessing(mfxVideoParam * param);

    virtual void* QueryCoreInterface(const MFX_GUID &guid);

    virtual eMFXHWType     GetHWType();
    
    virtual mfxStatus IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncoder = false);
    mfxStatus GetIntelDataPrivateReport(const GUID guid, mfxVideoParam *par, D3D11_VIDEO_DECODER_CONFIG & config);

    virtual mfxStatus     SetHandle(mfxHandleType type, mfxHDL handle);
    virtual mfxStatus     GetHandle(mfxHandleType type, mfxHDL *handle);
    void ReleaseHandle();

    virtual mfxU16 GetAutoAsyncDepth() {return MFX_AUTO_ASYNC_DEPTH_VALUE;}; //it can be platform based

    virtual bool IsCompatibleForOpaq();

private:
    D3D11VideoCORE(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = NULL);

    mfxStatus InternalInit();
    mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType);

    std::unique_ptr<D3D11Adapter> m_pid3d11Adapter;

    mfxStatus InitializeDevice(bool isTemporal = false);
    mfxStatus InternalCreateDevice();
    virtual mfxStatus DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc);
    // this function should not be virtual
    mfxStatus SetCmCopyStatus(bool enable);

    D3D11_VIDEO_DECODER_CONFIG* GetConfig(D3D11_VIDEO_DECODER_DESC *video_desc, mfxU32 start, mfxU32 end, const GUID guid);

    bool                                                               m_bUseExtAllocForHWFrames;
    std::unique_ptr<mfxDefaultAllocatorD3D11::mfxWideHWFrameAllocator> m_pcHWAlloc; 

    // D3D11 services
    CComPtr<ID3D11Device>            m_pD11Device;
    CComPtr<ID3D11DeviceContext>     m_pD11Context;
    CComPtr<IDXGIFactory>            m_pFactory;
    CComPtr<IDXGIAdapter>            m_pAdapter;

    CComQIPtr<ID3D11VideoDevice>     m_pD11VideoDevice;
    CComQIPtr<ID3D11VideoContext>    m_pD11VideoContext;
    

    // D3D11 VideoAccelrator which works with decode components on MFX/UMC levels
    // and providing HW capabilities
    std::unique_ptr<MFXD3D11Accelerator>    m_pAccelerator;
    #if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
    VPPHWResMng                          m_vpp_hw_resmng;
    #endif
    eMFXHWType                           m_HWType;
    eMFXGTConfig                         m_GTConfig;
    // Ordinal number of adapter to work
    const mfxU32                         m_adapterNum;
    ComPtrCore<ID3D11VideoDecoder>       m_comptr;
#if defined(MFX_ENABLE_MFE) && !defined(STRIP_EMBARGO)
    ComPtrCore<MFEDXVAEncoder>           m_mfeAvc;
    ComPtrCore<MFEDXVAEncoder>           m_mfeHevc;
#endif
    bool m_bCmCopy;
    bool m_bCmCopySwap;
    bool m_bCmCopyAllowed;
    std::unique_ptr<CmCopyWrapper>        m_pCmCopy;
    std::unique_ptr<CMEnabledCoreAdapter> m_pCmAdapter;
    mfxU32                                m_VideoDecoderConfigCount;
    std::vector<D3D11_VIDEO_DECODER_CONFIG>     m_Configs;
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    bool m_bIsBlockingTaskSyncEnabled;
#endif
};


#endif
#endif
#endif
