/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2020 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#ifndef MFX_COMPONENT_PARAMS
#define MFX_COMPONENT_PARAMS

#include "mfx_extended_buffer.h"
#include "mfx_base_allocator.h"
#include "mfx_pipeline_types.h"
#include "mfx_decoder.h"
#include "mfx_ihw_device.h"
#include "mfx_ivideo_render.h"
#include "mfx_irandom.h"
#include "mfx_iallocator_factory.h"
#include "mfx_adapters.h"
#include "mfx_ifactory.h"
#include "mfx_shared_ptr.h"
#include "mfx_factory_default.h"
#include "vpl_implementation_loader.h"

#define NOT_ASSIGNED_VALUE 0xFFF

//this factory passed across pipeline
typedef Adapter<IAllocatorFactoryTmpl<MFXFrameAllocatorRW>, IAllocatorFactory> RWAllocatorFactory;

class ComponentParams
{
public:
    ComponentParams(const tstring & name , const tstring & short_name, IMFXPipelineFactory *pFactory)
        : m_Name(name)
        , m_ShortName(short_name)
        , m_params()
        , m_libType(MFX_IMPL_SOFTWARE)
        , m_accelerationMode(MFX_ACCEL_MODE_NA)
        , m_bD39Feat(false)
        , m_pLibVersion()
        , m_bHWStrict(false)
        , m_bPrintTimeStamps()
        , m_bufType(MFX_BUF_UNSPECIFIED)
        , m_NumThread(0)
        , m_bExternalAlloc(false)
        , m_bD3D11SingeTexture()
        , m_nMaxAsync(1)
        , m_fFrameRate()
        , m_bFrameRateUnknown(false)
        , m_adapterNum(-1)
        , m_deviceID(-1)
        , m_uiMaxAsyncReached()
        , m_fAverageAsync()
        , m_zoomx()
        , m_zoomy()
        , m_rotate(0)
        , m_mirroring(0)
        , m_InNominalRange()
        , m_OutNominalRange()
        , m_InTransferMatrix()
        , m_OutTransferMatrix()
        , m_pAllocator()
        , m_nStartSearch()
        , m_nSelectAlgo(USE_FIRST)
        , m_pRandom()
        , m_bOverPS()
        , m_nOverPS()
        , m_extCO()
        , m_bCalcLatency()
        , m_nDropCyle()
        , m_nDropCount()
        , m_bForceMVCDetection()
        , m_OutFourcc()
        , m_VP9_Smooth_DRC(false)
      {
          PipelineObjectDescBase dsc(VIDEO_SESSION_NATIVE);
          m_Session = pFactory->CreateVideoSession(&dsc);
          m_pSession = m_Session.get();
          m_params.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
          if (!m_ShortName.compare(tstring(VM_STRING("enc"))))
              m_params.mfx.FrameInfo.Shift = (mfxU16) -1;    // not set
          m_params.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
          m_bAdaptivePlayback = false;
      }
      virtual ~ComponentParams(){}

      virtual mfxStatus PrintInfo();
      //checks filed and correct them
      virtual mfxStatus CorrectParams();

      virtual mfxU32 GetAdapter();

      virtual mfxU16 GetIoPatternIn();
      virtual mfxU16 GetIoPatternOut();

      //assign buffers from extparams to mfxVideoParam::m_params member
      virtual void AssignExtBuffers();

      virtual mfxStatus AllocFrames( RWAllocatorFactory::root* pFactory
                                   , IHWDevice *hwDevice
                                   , mfxFrameAllocRequest  * pRequest
                                   , bool bIsRawSurfaceLinear
                                   , bool bCreateEncCtl);

      virtual mfxStatus ReallocSurface(mfxMemId midIn, const mfxFrameInfo *info, mfxU16 memType, mfxMemId *midOut);

      virtual mfxStatus FindFreeSurface( mfxU32 sourceId
                                       , SrfEncCtl *ppSurface
                                       , IMFXVideoRender *pRender
                                       , mfxMemId *mid);

      //relase all resources asociated with surfaces
      virtual mfxStatus DestroySurfaces();

      struct SurfacesAllocated;

      virtual SurfacesAllocated & RegisterAlloc(const mfxFrameAllocRequest &request);

      virtual mfxStatus ConfigureLoader();
      virtual void ReleaseLoader();

public:

    std::vector<mfxI32>            m_SkipModes;
    tstring                        m_Name;
    tstring                        m_ShortName;
    mfxVideoParam                  m_params;
    mfxIMPL                        m_libType;//composite variable that combines both library type, and memory type

    mfxAccelerationMode            m_accelerationMode;

    bool                           m_bD39Feat; // to support D3D9 feature levels when work with D3D11. Applicable with external allocator only (ignore for D3D9 and external sys memory)
    mfxVersion                     m_libVersion;
    mfxVersion                   * m_pLibVersion;
    bool                           m_bHWStrict;
    bool                           m_bPrintTimeStamps;
    bool                           m_bAdaptivePlayback;
    MFXBufType                     m_bufType;
    mfxU16                         m_NumThread;
    bool                           m_bExternalAlloc;
    bool                           m_bD3D11SingeTexture;//single texture mode for d3d11 allocator
    mfxU16                         m_nMaxAsync;
    mfxF64                         m_fFrameRate;
    bool                           m_bFrameRateUnknown;
    mfxI32                         m_adapterNum;
    mfxI16                         m_deviceID;
    //real allocated CORE
    //copy ctor required when we put parameters into vector, shared_pointer remove necessity of copy ctor
    //ComponentParams
    mfx_shared_ptr<IVideoSession>  m_Session;
    IVideoSession                * m_pSession;

    // params for vpl dispatcher
    std::shared_ptr<VPLImplementationLoader> m_pLoader;

    //stat params
    mfxU32                         m_uiMaxAsyncReached;
    mfxF64                         m_fAverageAsync;

    //vppParams
    mfxF64                         m_zoomx;
    mfxF64                         m_zoomy;
    mfxU16                         m_rotate;
    mfxU16                         m_mirroring;
    mfxU16                         m_InNominalRange;
    mfxU16                         m_OutNominalRange;
    mfxU16                         m_InTransferMatrix;
    mfxU16                         m_OutTransferMatrix;
    MFXExtBufferVector             m_extParams;


    //
    MFXFrameAllocatorRW          * m_pAllocator;
    //surfaces with same resolution
    struct SurfacesAllocated
    {
        mfxFrameAllocRequest           request;
        mfxFrameAllocResponse          allocResponce;
        std::vector<SrfEncCtl>         surfaces;
        //used if Mediasdk required linear array of allocated surfaces
        std::vector<mfxFrameSurface1*> surfacesLinear;
        SurfacesAllocated()
            : request()
            , allocResponce()
        {
        }
    };
    typedef std::list< SurfacesAllocated > SurfacesContainer ;
    SurfacesContainer  m_Surfaces1;

    //Async Mode Support
    typedef std::pair <mfxSyncPoint, SrfEncCtl> SyncPair;
    std::list<SyncPair>             m_SyncPoints;
    size_t                          m_nStartSearch;
    eSurfacesSearchAlgo             m_nSelectAlgo;
    mfx_shared_ptr<IRandom>         m_pRandom;

    //override concrete fields support
    bool                            m_bOverPS;//override picstruct
    mfxU16                          m_nOverPS;//cmd line version
    mfxU16                          m_extCO;//extcoding option

    bool                            m_bCalcLatency;//used in videoconferencing testing
    int                             m_nDropCyle;//component may want no to pass all frames to downstream component
    int                             m_nDropCount;
    bool                            m_bForceMVCDetection;//cause to insert MVCExtended sequence buffer prior calling to dec header
    mfxU32                          m_OutFourcc;

    bool                            m_VP9_Smooth_DRC;

    std::vector<mfxU32>             m_SkippedFrames; // List of frames to be skipped at encoding

    SurfacesContainer::iterator GetSurfaceAlloc(mfxU32 w, mfxU32 h);
protected:
    std::vector<SurfacesContainer::iterator> m_sufacesByIDx;
};



#endif //MFX_COMPONENT_PARAMS
