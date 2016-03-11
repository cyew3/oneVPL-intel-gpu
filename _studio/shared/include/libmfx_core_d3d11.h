/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2015 Intel Corporation. All Rights Reserved.

File Name: libmfx_core_d3d11.h

\* ****************************************************************************** */
#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)
#ifndef __LIBMFX_CORE__D3D11_H__
#define __LIBMFX_CORE__D3D11_H__


#include "d3d11_decode_accelerator.h"
#include "mfx_check_hardware_support.h"


#include "libmfx_core.h"
#include "encoding_ddi.h"
#include "libmfx_allocator_d3d11.h"

#include <d3d11.h>

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
    virtual void          GetVA(mfxHDL* phdl, mfxU16 type) 
    {
        (type & MFX_MEMTYPE_FROM_DECODE)?(*phdl = m_pAccelerator.get()):(*phdl = 0);
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
    void ReleaseHandle();

    virtual mfxU16 GetAutoAsyncDepth() {return MFX_AUTO_ASYNC_DEPTH_VALUE;}; //it can be platform based

    virtual bool IsCompatibleForOpaq();

private:
    D3D11VideoCORE(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = NULL);

    mfxStatus InternalInit();
    mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType);

    std::auto_ptr<D3D11Adapter> m_pid3d11Adapter;

    mfxStatus InitializeDevice(bool isTemporal = false);
    mfxStatus InternalCreateDevice();
    virtual mfxStatus DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc);
    // this function should not be virtual
    mfxStatus SetCmCopyStatus(bool enable);
    
    bool                                                           m_bUseExtAllocForHWFrames;
    s_ptr<mfxDefaultAllocatorD3D11::mfxWideHWFrameAllocator, true> m_pcHWAlloc; 

    // D3D11 services
    CComPtr<ID3D11Device>            m_pD11Device;
    CComPtr<ID3D11DeviceContext>     m_pD11Context;
    CComPtr<IDXGIFactory>            m_pFactory;
    CComPtr<IDXGIAdapter>            m_pAdapter;

    CComQIPtr<ID3D11VideoDevice>     m_pD11VideoDevice;
    CComQIPtr<ID3D11VideoContext>    m_pD11VideoContext;
    

    // D3D11 VideoAccelrator which works with decode components on MFX/UMC levels
    // and providing HW capabilities
    std::auto_ptr<MFXD3D11Accelerator>    m_pAccelerator;
    #if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
    VPPHWResMng                          m_vpp_hw_resmng;
    #endif
    eMFXHWType                           m_HWType;
    // Ordinal number of adapter to work
    const mfxU32                         m_adapterNum;
    ComPtrCore<ID3D11VideoDecoder>       m_comptr;
    bool m_bCmCopy;
    bool m_bCmCopySwap;
    bool m_bCmCopyAllowed;
    s_ptr<CmCopyWrapper, true>           m_pCmCopy;
    s_ptr<CMEnabledCoreAdapter, true>    m_pCmAdapter;
};


#endif
#endif
#endif
