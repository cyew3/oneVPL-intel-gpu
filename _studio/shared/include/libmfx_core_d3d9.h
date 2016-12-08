//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined  (MFX_VA_WIN) && defined (MFX_D3D9_ENABLED)
#ifndef __LIBMFX_CORE__D3D9_H__
#define __LIBMFX_CORE__D3D9_H__


#include "libmfx_core.h"
#include "libmfx_allocator_d3d9.h"
#include "libmfx_core_factory.h"

#include "fast_compositing_ddi.h"
#include "auxiliary_device.h"

// disable the "conditional expression is constant" warning
#pragma warning(disable: 4127)

class CmCopyWrapper;

namespace UMC
{
    class DXVA2Accelerator;
    class ProtectedVA;
};

mfxStatus              CreateD3DDevice(IDirect3D9        **pD3D, 
                                       IDirect3DDevice9  **pDirect3DDevice,
                                       const mfxU32       adapterNum,
                                       mfxU16             width, 
                                       mfxU16             height,
                                       mfxU32             DecId);


class AuxiliaryDevice;


class D3D9VideoCORE : public CommonCORE
{
    //friend class VideoVPPHW;
public:
    friend class FactoryCORE;
    class D3D9Adapter : public D3D9Interface
    {
    public:
        D3D9Adapter(D3D9VideoCORE *pD3D9Core):m_pD3D9Core(pD3D9Core)
        {
        };
        virtual mfxStatus  GetD3DService(mfxU16 width,
                                         mfxU16 height,
                                         IDirectXVideoDecoderService **ppVideoService = NULL,
                                         bool isTemporal = false)
        {
            return m_pD3D9Core->GetD3DService(width, height, ppVideoService, isTemporal);
        };

        virtual IDirect3DDeviceManager9 * GetD3D9DeviceManager(void)
        {
            return m_pD3D9Core->m_pDirect3DDeviceManager;
        };

    protected:
        D3D9VideoCORE *m_pD3D9Core;

    };


    class CMEnabledCoreAdapter : public CMEnabledCoreInterface
    {
    public:
        CMEnabledCoreAdapter(D3D9VideoCORE *pD3D9Core) : m_pD3D9Core(pD3D9Core)
        {
        };
        virtual mfxStatus SetCmCopyStatus(bool enable)
        {
            return m_pD3D9Core->SetCmCopyStatus(enable);
        };
    protected:
        D3D9VideoCORE *m_pD3D9Core;
    };

    virtual ~D3D9VideoCORE();

    virtual mfxStatus     SetHandle(mfxHandleType type, mfxHDL handle);
    virtual mfxStatus     AllocFrames(mfxFrameAllocRequest *request, 
                                      mfxFrameAllocResponse *response, bool isNeedCopy = true);
    virtual void          GetVA(mfxHDL* phdl, mfxU16 type) 
    {
        (type & MFX_MEMTYPE_FROM_DECODE)?(*phdl = m_pVA.get()):(*phdl = 0);
    };
    // Get the current working adapter's number
    virtual mfxU32 GetAdapterNumber(void) {return m_adapterNum;}
    virtual eMFXPlatform  GetPlatformType() {return  MFX_PLATFORM_HARDWARE;}

    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType);

    virtual eMFXHWType     GetHWType();

    mfxStatus              CreateVA(mfxVideoParam * param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, UMC::FrameAllocator *allocator);
    // to check HW capatbilities
    mfxStatus              IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncoder = false);
    #if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
    virtual void          GetVideoProcessing(mfxHDL* phdl) 
    {
        *phdl = &m_vpp_hw_resmng;
    };
    #endif
    mfxStatus              CreateVideoProcessing(mfxVideoParam * param);

    virtual eMFXVAType   GetVAType() const {return MFX_HW_D3D9; };

    virtual void* QueryCoreInterface(const MFX_GUID &guid);

    virtual mfxU16 GetAutoAsyncDepth() {return MFX_AUTO_ASYNC_DEPTH_VALUE;};//it can be platform based
    
    virtual bool IsCompatibleForOpaq();
        

protected:
    
    virtual mfxStatus              GetD3DService(mfxU16 width,
                                                 mfxU16 height,
                                                IDirectXVideoDecoderService **ppVideoService = NULL,
                                                bool isTemporal = false);

    D3D9VideoCORE(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = NULL);
    virtual void           Close();

    virtual mfxStatus      DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    mfxStatus              CreateVideoAccelerator(mfxVideoParam * param, int NumOfRenderTarget, IDirect3DSurface9 **RenderTargets, UMC::FrameAllocator *allocator);
    mfxStatus              ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc);

private:

    mfxStatus              InitializeService(bool isTemporalCall);

    mfxStatus              InternalInit();
    mfxStatus              GetIntelDataPrivateReport(const GUID guid, DXVA2_ConfigPictureDecode & config);
    void                   ReleaseHandle();
    // this function should not be virtual
    mfxStatus SetCmCopyStatus(bool enable);

    HANDLE                                     m_hDirectXHandle; // if m_pDirect3DDeviceManager was used
    std::auto_ptr<UMC::DXVA2Accelerator>       m_pVA;
    #if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
    VPPHWResMng                          m_vpp_hw_resmng;
    #endif
    IDirect3D9                          *m_pD3D;
    // Ordinal number of adapter to work
    const mfxU32                         m_adapterNum;
    IDirect3DDeviceManager9             *m_pDirect3DDeviceManager;
    IDirect3DDevice9                    *m_pDirect3DDevice;
    IDirectXVideoDecoderService         *m_pDirectXVideoService;
    bool                                 m_bUseExtAllocForHWFrames;

    s_ptr<mfxDefaultAllocatorD3D9::mfxWideHWFrameAllocator, true> m_pcHWAlloc;

    eMFXHWType                           m_HWType;

    IDirect3DSurface9 *m_pSystemMemorySurface;

    bool m_bCmCopy;
    bool m_bCmCopySwap;
    bool m_bCmCopyAllowed;

    s_ptr<CmCopyWrapper, true>            m_pCmCopy;
    s_ptr<D3D9Adapter, true>              m_pAdapter;
    s_ptr<CMEnabledCoreAdapter, true>     m_pCmAdapter;
};

#endif

#endif

