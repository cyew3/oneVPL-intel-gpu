//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#if defined(MFX_VA_OSX)

#ifndef __LIBMFX_CORE__VDAAPI_H__
#define __LIBMFX_CORE__VDAAPI_H__

#include <memory>
#include "libmfx_core.h"
#include "libmfx_core_interface.h"

class VDAAPIVideoCORE : public CommonCORE
{
public:
    friend class FactoryCORE;
    class VDAAPIAdapter : public VDAAPIInterface
    {
    public:
        VDAAPIAdapter(VDAAPIVideoCORE *pVDAAPICore):m_pVDAAPICore(pVDAAPICore)
        {
        };
        //    virtual mfxStatus  GetD3DService(mfxU16 width,
        //                                     mfxU16 height,
        //                                     VADisplay** pDisplay = NULL)
        //    {
        //        return m_pVAAPICore->GetD3DService(width, height, pDisplay);
        //    };
        //
    protected:
        VDAAPIVideoCORE *m_pVDAAPICore;
        
    };

//    virtual mfxStatus     SetHandle(mfxHandleType type, mfxHDL handle);
//    virtual mfxStatus     AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
//    virtual void          GetVA(mfxHDL* phdl, mfxU16 type) 
//    {
//        (type & MFX_MEMTYPE_FROM_DECODE)?(*phdl = m_pVA.get()):(*phdl = 0);
//    };
    // Get the current working adapter's number
    virtual mfxU32 GetAdapterNumber(void) {return m_adapterNum;}
    virtual eMFXPlatform  GetPlatformType() {return  MFX_PLATFORM_HARDWARE;}
    virtual eMFXVAType   GetVAType() const {return MFX_HW_VDAAPI; };

//    virtual mfxStatus DoFastCopy(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
//    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
//    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType);

//    virtual mfxStatus InitializeFastCompositingService(void);

//    mfxHDL * GetFastCompositingService();
//    void SetOnFastCompositingService(void);
//    virtual bool      CheckCompatibility() const;

    virtual eMFXHWType     GetHWType() {
        return m_HWType; }

//    mfxStatus              CreateVA(mfxVideoParam * param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    // to check HW capatbilities
    virtual mfxStatus   IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncoder = false);
    
    virtual void* QueryCoreInterface(const MFX_GUID &guid);
    
    virtual mfxStatus GetSWFallbackPolicy()
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

protected:
    VDAAPIVideoCORE(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = NULL);
    virtual ~VDAAPIVideoCORE();
//    virtual void           Close();
//    virtual mfxStatus      DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
   
    //mfxStatus              CreateD3DDevice(mfxU16 width, mfxU16 height);
//    mfxStatus              ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc);
//    mfxStatus              TraceFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxStatus sts);
//    mfxStatus              OnDeblockingInWinRegistry(mfxU32 codecId);

//    void                   ReleaseHandle();   
//    std::auto_ptr<UMC::DXVA2Accelerator>       m_pVA;
//    std::auto_ptr<UMC::ProtectedVA>            m_protectedVA;
   
    // Ordinal number of adapter to work
    const mfxU32                         m_adapterNum;  
//    bool                                 m_bUseExtAllocForHWFrames;
//    s_ptr<mfxWideHWFrameAllocator, true> m_pcHWAlloc;
    eMFXHWType                           m_HWType;
    
public: // aya: FIXME: private???
    
    std::auto_ptr<VDAAPIAdapter>            m_pAdapter;
    
};
#endif  //__LIBMFX_CORE__VDAAPI_H__
#endif  //#if defined  (MFX_VA) && defined(__APPLE__)

