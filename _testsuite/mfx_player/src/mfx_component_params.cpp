/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_component_params.h"
#include "mfx_serializer.h"
#include "mfx_d3d_allocator.h"
#include "vaapi_allocator.h"
#include "mfx_d3d11_allocator.h"

#if defined(_WIN32) || defined(_WIN64)
    #include "d3d9.h"
#else
    #define D3DADAPTER_DEFAULT 0
#endif

mfxU32 ComponentParams::GetAdapter()
{
    mfxU32 nAdapter = D3DADAPTER_DEFAULT;

    struct{
        mfxIMPL impl;
        mfxU32  nAdapter;
    } adapters []= 
    {
        {MFX_IMPL_HARDWARE2, 1},
        {MFX_IMPL_HARDWARE3, 2},
        {MFX_IMPL_HARDWARE4, 3},
    };

    for (unsigned int i =0; i < MFX_ARRAY_SIZE(adapters); i++)
    {
        if (MFX_IMPL_BASETYPE(m_RealImpl) == adapters[i].impl)
        {
            nAdapter = adapters[i].nAdapter;
            break;
        }
    }

    return nAdapter;
}

mfxU16 ComponentParams::GetIoPatternIn()
{
    switch(m_bufType)
    {
        case MFX_BUF_SW   : return MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        case MFX_BUF_HW   : return MFX_IOPATTERN_IN_VIDEO_MEMORY;
        case MFX_BUF_HW_DX11 : return MFX_IOPATTERN_IN_VIDEO_MEMORY;
        case MFX_BUF_OPAQ : return MFX_IOPATTERN_IN_OPAQUE_MEMORY;
        default : 
            return MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }
}

mfxU16 ComponentParams::GetIoPatternOut()
{
    switch(m_bufType)
    {
    case MFX_BUF_SW   : return MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    case MFX_BUF_HW   : return MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    case MFX_BUF_HW_DX11 : return MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    case MFX_BUF_OPAQ : return MFX_IOPATTERN_OUT_OPAQUE_MEMORY;
    default : 
        return MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }
}

void ComponentParams::AssignExtBuffers()
{
    m_params.NumExtParam = (mfxU16)m_extParams.size();
    m_params.ExtParam    = &m_extParams;//vector is stored linear in memory
}

mfxStatus ComponentParams::CorrectParams()
{
    mfxIMPL impl;
    MFX_CHECK_POINTER(m_pSession);
    //query implementation set by mfx_application, not real one
    MFX_CHECK_STS(m_pSession->QueryIMPLExternal(&impl));

    mfxU32 impl_pure = impl & ~(-MFX_IMPL_VIA_ANY);
    mfxU32 via = impl & (-MFX_IMPL_VIA_ANY);
    MFX_CHECK(MFX_IMPL_VIA_ANY != via);

    //in case of d3d11 impl we have to force surfaces to be d3d11 if they are HW
    if (via == MFX_IMPL_VIA_D3D11 && m_bufType == MFX_BUF_HW)
    {
        m_bufType = MFX_BUF_HW_DX11;
    }

    if (MFX_BUF_UNSPECIFIED == m_bufType)
    {
        switch (via)
        {
            case MFX_IMPL_VIA_D3D9 :
                m_bufType = MFX_BUF_HW;
                break;
            case MFX_IMPL_VIA_D3D11 :
                m_bufType = MFX_BUF_HW_DX11;
                break;
            default:
                m_bufType = (MFX_IMPL_SOFTWARE == impl_pure) ? MFX_BUF_SW : MFX_BUF_HW ;
                break;
        }
    }

    if (MFX_BUF_HW == m_bufType || 
        MFX_BUF_HW_DX11 == m_bufType)
    {
        m_bExternalAlloc = true;
    }

    return MFX_ERR_NONE;
}

#define MFX_IMPL_SW_OR_HW(impl) (impl & 0xFF)

mfxStatus ComponentParams::PrintInfo()
{
    mfxIMPL impl;
    MFX_CHECK_POINTER(m_pSession);
    MFX_CHECK_STS(m_pSession->QueryIMPL(&impl));

    mfxVersion version;
    MFX_CHECK_STS(m_pSession->QueryVersion(&version));

    switch (MFX_IMPL_SW_OR_HW(impl))
    {
        case MFX_IMPL_SOFTWARE : ::PrintInfo(m_Name.c_str(), VM_STRING("Software")); break;
        case MFX_IMPL_HARDWARE : ::PrintInfo(m_Name.c_str(), VM_STRING("Hardware")); break;
        default                : ::PrintInfo(m_Name.c_str(), VM_STRING("Unknown"));  break;
    }

    PipelineTrace((SerializeWithKey(VM_STRING("  Surface type"), m_bufType).c_str()));
    PipelineTrace((SerializeWithKey(VM_STRING("  External allocator"), m_bExternalAlloc).c_str()));
    PipelineTrace((SerializeWithKey(VM_STRING("  MFX API version"),version).c_str()));
    PipelineTrace((SerializeWithKey(VM_STRING("  MFXImpl"), m_RealImpl).c_str()));

    PrintDllInfo(VM_STRING("  MediaSDK "), m_mfxLibPath.c_str());
    
    return MFX_ERR_NONE;
}

void ComponentParams::AttachOpaqBuffer( bool bAtOutput , mfxU16 Type , std::vector<mfxFrameSurface1*>& surfaces)
{
    MFXExtBufferPtr<mfxExtOpaqueSurfaceAlloc> extOpaqTest(m_extParams);
    if (NULL == extOpaqTest.get())
    {
        m_extParams.push_back(new mfxExtOpaqueSurfaceAlloc());
    }
    //TODO: again limitation of mfxextbufferptr
    MFXExtBufferPtr<mfxExtOpaqueSurfaceAlloc> extOpaq(m_extParams);

    if (bAtOutput)
    {
        extOpaq->Out.Surfaces   = &surfaces.front();
        extOpaq->Out.NumSurface =  (mfxU16)surfaces.size();
        extOpaq->Out.Type       =  Type;
    }else
    {
        extOpaq->In.Surfaces   = &surfaces.front();
        extOpaq->In.NumSurface =  (mfxU16)surfaces.size();
        extOpaq->In.Type       =  Type;
    }
    AssignExtBuffers();
}

mfxStatus ComponentParams::DestroySurfaces()
{
    for (SurfacesContainer::iterator i = m_Surfaces1.begin(); i != m_Surfaces1.end(); i++)
    {
        if (NULL != m_pAllocator)
        {
            if (!m_bExternalAlloc)
            {
                MFX_CHECK( i->surfaces.end() == std::find_if( i->surfaces.begin()
                    , i->surfaces.end() 
                    , bind1st(SrfUnlocker(), *(mfxFrameAllocator*)m_pAllocator)));
            }

            MFX_CHECK_STS(m_pAllocator->FreeFrames(&i->allocResponce));
        } 

        for_each(i->surfaces.begin(), i->surfaces.end(), SrfEncCtlDelete());
        i->surfaces.clear();
        i->surfacesLinear.clear();
    }
    m_Surfaces1.clear();
    m_sufacesByIDx.clear(); 
    return MFX_ERR_NONE;
}

mfxStatus ComponentParams::AllocFrames( RWAllocatorFactory::root* pFactory
                                      , IHWDevice *hwDevice
                                      , mfxFrameAllocRequest  * pRequest
                                      , bool bCreateEncCtl)
{
    // pointer to allocator parameters structure needed for allocator init
    std::auto_ptr<mfxAllocatorParams> pAllocatorParams;
    
    SurfacesAllocated &refSurfaces = RegisterAlloc(*pRequest);

    // Create allocator
    switch(m_bufType)
    {
        case  MFX_BUF_HW:
        {
            MFX_CHECK_POINTER(hwDevice);
#ifdef D3D_SURFACES_SUPPORT

            if (pRequest->Info.FourCC == MFX_FOURCC_RGB4 && pRequest->Type & MFX_MEMTYPE_FROM_VPPOUT )
            {
                /* VideoProcessBlt that is used in Raw Accelerator requires PROCESSOR_TARGET surfaces as out */
                pRequest->Type |= MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
            }
            /* if pRequest->Type doesn't specify flags the use default MFX_MEMTYPE_DXVA2_DECODER_TARGET*/
            else if ( ( pRequest->Type & (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_DXVA2_DECODER_TARGET)) == 0)
            {
                pRequest->Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
            }

            if (NULL == m_pAllocator)
            {
                MFX_CHECK_WITH_ERR(m_pAllocator = pFactory->CreateD3DAllocator(), MFX_ERR_MEMORY_ALLOC);
                if (m_VP9_Smooth_DRC)
                    m_pAllocator = new AllocatorAdapterRW(m_pAllocator);
            }

            D3DAllocatorParams *pd3dAllocParams;
            MFX_CHECK_WITH_ERR(pd3dAllocParams = new D3DAllocatorParams, MFX_ERR_MEMORY_ALLOC);
#ifdef PAVP_BUILD
            if (0 != m_params.Protected)
                pd3dAllocParams->surfaceUsage = D3DUSAGE_RESTRICTED_CONTENT;
#endif//PAVP_BUILD

            IDirect3DDeviceManager9 * pMgr;

            MFX_CHECK_STS(hwDevice->GetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, (mfxHDL*) &pMgr));
            pd3dAllocParams->pManager = pMgr;
            pAllocatorParams.reset(pd3dAllocParams);  
#endif
#ifdef LIBVA_SUPPORT
            if (pRequest->Info.FourCC == MFX_FOURCC_RGB4)
                pRequest->Type |= MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
            else
                pRequest->Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;

            if (NULL == m_pAllocator)
            {
                MFX_CHECK_WITH_ERR(m_pAllocator = pFactory->CreateVAAPIAllocator(), MFX_ERR_MEMORY_ALLOC);
            }

            vaapiAllocatorParams *p_vaapiAllocParams;
            MFX_CHECK_WITH_ERR(p_vaapiAllocParams = new vaapiAllocatorParams, MFX_ERR_MEMORY_ALLOC);
            VADisplay va_dpy;

            MFX_CHECK_STS(hwDevice->GetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL*) &va_dpy));
            p_vaapiAllocParams->m_dpy = va_dpy;
            p_vaapiAllocParams->bAdaptivePlayback = m_bAdaptivePlayback;
            pAllocatorParams.reset(p_vaapiAllocParams);
#endif
            break;
        } 
        case MFX_BUF_SW :
        {
            pRequest->Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
            if (NULL == m_pAllocator)
            {
                MFX_CHECK_WITH_ERR(m_pAllocator = pFactory->CreateSysMemAllocator(), MFX_ERR_MEMORY_ALLOC);
            }
            break;
        }
#if MFX_D3D11_SUPPORT
    case MFX_BUF_HW_DX11:
        {
            pRequest->Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;

            //TODO: strange if when allocator not created, but reinitialized?
            if (NULL == m_pAllocator)
            {
                MFX_CHECK_WITH_ERR(m_pAllocator = pFactory->CreateD3D11Allocator(), MFX_ERR_MEMORY_ALLOC);
                if (m_VP9_Smooth_DRC)
                    m_pAllocator = new AllocatorAdapterRW(m_pAllocator);
            }

            D3D11AllocatorParams *pd3d11AllocParams;
            MFX_CHECK_WITH_ERR(pd3d11AllocParams = new D3D11AllocatorParams, MFX_ERR_MEMORY_ALLOC);
            pd3d11AllocParams->bUseSingleTexture = m_bD3D11SingeTexture;
#ifdef PAVP_BUILD
            if (0 != m_params.Protected)
                pd3d11AllocParams->uncompressedResourceMiscFlags = D3D11_RESOURCE_MISC_RESTRICTED_CONTENT;
#endif//PAVP_BUILD

            ID3D11Device *pDevice;
            MFX_CHECK_STS(hwDevice->GetHandle(MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&pDevice));

            pd3d11AllocParams->pDevice        = pDevice;
            pAllocatorParams.reset(pd3d11AllocParams);           
            break;
        }
#endif
    case MFX_BUF_OPAQ:
        {
            MFX_CHECK(NULL == m_pAllocator);
            refSurfaces.allocResponce.NumFrameActual = pRequest->NumFrameMin;
            break;
        }
    default:
        {
            MFX_CHECK(NULL != m_pAllocator);
            break;
        }
    }

    if (m_pAllocator)
    {
        MFX_CHECK_STS(m_pAllocator->Init(pAllocatorParams.get()));

        if (m_bExternalAlloc)
        {
            MFX_CHECK_STS(m_pSession->SetFrameAllocator(m_pAllocator));
        }

        //allocating frames
        MFX_CHECK_STS(m_pAllocator->AllocFrames(pRequest, &refSurfaces.allocResponce));
    }

    // Create mfxFrameSurface1 array
    for (int i = 0; i < refSurfaces.allocResponce.NumFrameActual; i++)
    {
        std::auto_ptr<mfxEncodeCtrl> pCtrl;

        if (bCreateEncCtl)
        {
            //MFX_CHECK_POINTER(pCtrl = new mfxEncodeCtrl);
            std::auto_ptr<mfxExtVppAuxData> pAuxVpp;
            MFX_CHECK_POINTER((pAuxVpp.reset(new  mfxExtVppAuxData), pAuxVpp.get()));

            MFX_ZERO_MEM(*pAuxVpp);
            pAuxVpp->Header.BufferId = MFX_EXTBUFF_VPP_AUXDATA;
            pAuxVpp->Header.BufferSz = sizeof(mfxExtVppAuxData);

            MFX_CHECK_POINTER((pCtrl.reset(new mfxEncodeCtrl), pCtrl.get()));
            MFX_ZERO_MEM(*pCtrl);
            pCtrl->NumExtParam = 1;

            MFX_CHECK_POINTER(pCtrl->ExtParam = new mfxExtBuffer*[1]);
            pCtrl->ExtParam[0] = (mfxExtBuffer*)pAuxVpp.release();
        }

        std::auto_ptr<mfxFrameSurface1> pSrf;
        MFX_CHECK_POINTER((pSrf.reset(new mfxFrameSurface1), pSrf.get()));

        memset(pSrf.get(), 0, sizeof(mfxFrameSurface1));

        memcpy(&pSrf->Info, &pRequest->Info, sizeof(mfxFrameInfo));
        
        if (m_pAllocator)
        {
            if (m_bExternalAlloc)
            {
                pSrf->Data.MemId = refSurfaces.allocResponce.mids[i];
            }else
            {
                MFX_CHECK_STS( m_pAllocator->LockFrame( refSurfaces.allocResponce.mids[i]
                                                      , &pSrf->Data));
            }
        }

        refSurfaces.surfacesLinear.push_back(pSrf.get());
        refSurfaces.surfaces.push_back(SrfEncCtl(pSrf.release(), pCtrl.release()));
    }

    return MFX_ERR_NONE;
}

mfxStatus ComponentParams::ReallocSurface(mfxFrameSurface1  * pSurface)
{
    if (!m_pAllocator)
        return MFX_ERR_MEMORY_ALLOC;

    return m_pAllocator->AllocFrame(pSurface);
}

mfxStatus ComponentParams::FindFreeSurface( mfxU32 sourceId
                                          , SrfEncCtl *pSurface
                                          , IMFXVideoRender *pRender)
{
    mfxStatus res = MFX_WRN_IN_EXECUTION;
    Timeout<10> ffstimeout;
    //different surfaces allocated only in svc case, however for mvc case source id might present
    //sourceId = 1;
    std::vector<SrfEncCtl>  & refSurfaces 
        = 1 == m_sufacesByIDx.size() ? m_sufacesByIDx[0]->surfaces : m_sufacesByIDx[sourceId]->surfaces;

    for (; !ffstimeout ; )
    {
        //for(;;)
        if (NULL == m_pRandom)
        {
            for (size_t i = m_nStartSearch, k = 0; k < refSurfaces.size(); k++, i = (mfxU16)((i+1) % refSurfaces.size()))
            {
                int idx = 0;
                switch (m_nSelectAlgo)
                {
                    case USE_FIRST:
                    case USE_OLDEST_DIRECT:
                        idx = (int)i;
                    break;
                    case  USE_OLDEST_REVERSE:
                        idx = (int)(refSurfaces.size() - i - 1);

                        break;
                    default:
                        break;
                }
                if (0 == refSurfaces[idx].pSurface->Data.Locked)
                {
                    *pSurface = refSurfaces[idx];
                    if (m_nSelectAlgo == USE_OLDEST_DIRECT || 
                        m_nSelectAlgo == USE_OLDEST_REVERSE)
                    {
                        m_nStartSearch = (i+1) % (mfxU16)refSurfaces.size();
                        //printf("srf, %d\n",i);
                    }

                    return MFX_ERR_NONE;
                }
            }
        }
        else
        {
            int curIdx;
            std::vector<int> usedNumbers(refSurfaces.size());
            std::generate(usedNumbers.begin(), usedNumbers.end(), sequence<int>());

            for (size_t i = 0; i < refSurfaces.size(); i ++)
            {
                //position in non tested array, last element shouldn be checked for random since it is only one remained
                if (i + 1 < refSurfaces.size())
                {
                    curIdx = (int)i + (int)((double)(refSurfaces.size() - 1 - i) * (double)m_pRandom->rand() / (double)m_pRandom->rand_max());
                }
                else
                {
                    curIdx = (int)(refSurfaces.size() - 1);
                }

                if (0 == refSurfaces[usedNumbers[curIdx]].pSurface->Data.Locked)
                {
                    *pSurface = refSurfaces[usedNumbers[curIdx]];
                    return MFX_ERR_NONE;
                }
                std::swap(usedNumbers[i], usedNumbers[curIdx]);
            }
        }

        if (NULL != pRender)
        {
            MPA_TRACE("WaitTasks");
            MFX_CHECK_STS_SKIP(res = pRender->WaitTasks(ffstimeout.interval), MFX_WRN_IN_EXECUTION);
        }

        //if render returned MFX_WRN_IN_EXECUTION it means that it doesn't have active SP that it can wait
        //or if render=null
        if (MFX_WRN_IN_EXECUTION == res)
        {
            //not wait at all - ???
            //do wee need this?
            ffstimeout.wait("FindFreeSurface");
        }else
        {
            ffstimeout.wait_0();
        }
    }

    return MFX_WRN_IN_EXECUTION;
}

ComponentParams::SurfacesAllocated & ComponentParams::RegisterAlloc(const mfxFrameAllocRequest &request)
{
    SurfacesContainer::iterator i = GetSurfaceAlloc(request.Info.Width, request.Info.Height);

    if(i != m_Surfaces1.end())
    {
        return *i;
    }
    SurfacesAllocated sf;
    
    sf.request = request;

    m_Surfaces1.push_back(sf);
    
    //registering in id map
    m_sufacesByIDx.push_back(m_Surfaces1.end());
    m_sufacesByIDx.back()--;

    return m_Surfaces1.back();
}

ComponentParams::SurfacesContainer::iterator  ComponentParams::GetSurfaceAlloc(mfxU32 w, mfxU32 h)
{
    SurfacesContainer::iterator i;
    for (i  = m_Surfaces1.begin(); i != m_Surfaces1.end(); i++)
    {
        //checking both requests
        if (i->request.Info.Width == w && i->request.Info.Height == h)
        {
            return i;
        }
    }
    return i;
}
