/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_utils.h

\* ****************************************************************************** */


#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include <vector>

#include "mfxplugin++.h"
#include "mfx_camera_plugin_cm.h"


#if !defined(_MFX_CAMERA_PLUGIN_UTILS_)
#define _MFX_CAMERA_PLUGIN_UTILS_

#if defined( AS_VPP_PLUGIN )

namespace MfxCameraPlugin
{

static const mfxU32 NO_INDEX      = 0xffffffff;
static const mfxU8  NO_INDEX_U8   = 0xff;
static const mfxU16  NO_INDEX_U16 = 0xffff;

template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }

template<class T, size_t N> inline size_t SizeOf(T(&)[N]) { return N; }


static const mfxU16 MFX_MEMTYPE_SYS_INT =
    MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_INTERNAL_FRAME;

static const mfxU16 MFX_MEMTYPE_SYS_EXT_IN =
    MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME;

static const mfxU16 MFX_MEMTYPE_SYS_EXT_OUT =
    MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME;

static const mfxU16 MFX_MEMTYPE_D3D_INT =
    MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_INTERNAL_FRAME;

static const mfxU16 MFX_MEMTYPE_D3D_EXT_IN =
    MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;

static const mfxU16 MFX_MEMTYPE_D3D_EXT_OUT =
    MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;


// GPU memory access mode
enum
{
    MEM_GPUSHARED   =  0,
    MEM_FASTGPUCPY,
    MEM_CPUCPY
};


class MfxFrameAllocResponse : public mfxFrameAllocResponse
{
public:
    MfxFrameAllocResponse();

    ~MfxFrameAllocResponse();

    mfxStatus Alloc(
        VideoCORE *            core,
        mfxFrameAllocRequest & req,
        bool isCopyRequired = true);

    mfxStatus Alloc(
        VideoCORE *            core,
        mfxFrameAllocRequest & req,
        mfxFrameSurface1 **    opaqSurf,
        mfxU32                 numOpaqSurf);

    mfxStatus AllocCmBuffers(
        CmDevice *             device,
        mfxFrameAllocRequest & req);

    mfxStatus AllocCmBuffersUp(
        CmDevice *             device,
        mfxFrameAllocRequest & req);

    mfxStatus AllocCmSurfaces(
        CmDevice *             device,
        mfxFrameAllocRequest & req);

    void * GetSysmemBuffer(mfxU32 idx);

    mfxU32 Lock(mfxU32 idx);

    void Unlock();

    mfxU32 Unlock(mfxU32 idx);

    mfxU32 Locked(mfxU32 idx) const;
#if USE_AGOP //for debug
    mfxI32 CountNonLocked(){ return std::count_if(m_locked.begin(), m_locked.end(), IsZero); }
    mfxI32 CountLocked(){ return m_locked.size() - std::count_if(m_locked.begin(), m_locked.end(), IsZero); }
#endif

private:
    MfxFrameAllocResponse(MfxFrameAllocResponse const &);
    MfxFrameAllocResponse & operator =(MfxFrameAllocResponse const &);

    static void DestroyBuffer  (CmDevice * device, void * p);
    static void DestroySurface (CmDevice * device, void * p);
    static void DestroyBufferUp(CmDevice * device, void * p);
    void (*m_cmDestroy)(CmDevice *, void *);

    VideoCORE * m_core;
    CmDevice *  m_cmDevice;
    mfxU16      m_numFrameActualReturnedByAllocFrames;

    std::vector<mfxFrameAllocResponse> m_responseQueue;
    std::vector<mfxMemId>              m_mids;
    std::vector<mfxU32>                m_locked;
    std::vector<void *>                m_sysmems;
};


mfxU32 FindFreeResourceIndex(
    MfxFrameAllocResponse const & pool,
    mfxU32                        startingFrom = 0);

mfxMemId AcquireResource(
    MfxFrameAllocResponse & pool,
    mfxU32                  index);

mfxMemId AcquireResource(
    MfxFrameAllocResponse & pool);

mfxHDLPair AcquireResourceUp(
    MfxFrameAllocResponse & pool,
    mfxU32                  index);

mfxHDLPair AcquireResourceUp(
    MfxFrameAllocResponse & pool);

void ReleaseResource(
    MfxFrameAllocResponse & pool,
    mfxMemId                mid);


typedef struct _mfxCameraCaps
{
    mfxU32    Version;
    union {
      struct {
        mfxU32    bBlackLevelCorrection             : 1;
        mfxU32    bVignetteCorrection               : 1;
        mfxU32    bWhiteBalance                     : 1;
        mfxU32    bHotPixel                         : 1;
        mfxU32    bDemosaic                         : 1; // must be ON
        mfxU32    bColorConversionMaxtrix           : 1;
        mfxU32    bForwardGammaCorrection           : 1;
        mfxU32    bColorConversion3D16              : 1;
        mfxU32    bLensDistortionCorrection         : 1;
        mfxU32    bOutToARGB8                       : 1;
        mfxU32    bOutToARGB16                      : 1;
        mfxU32    Reserved                          : 21;
      };
      mfxU32      ModuleConfiguration;
    };
    mfxU32        FrameSurfWidth                    :16;
    mfxU32        FrameSurfHeight                   :16;
    //mfxU32        BayerPatternType                  :16;
    union {
      struct {
          mfxU32   InputMemoryOperationMode         : 8;
          mfxU32   OutputMemoryOperationMode        : 8;
      };
      mfxU32       MemoryOperationMode;
    };
    mfxU16    bNoPadding;                  // must be zero now.
    mfxU16    API_Mode;
}   mfxCameraCaps;


typedef struct _CameraPipeWhiteBalanceParams
{
    bool        bActive;
    //mfxU32        Mode;
    mfxF32      RedCorrection;
    mfxF32      GreenTopCorrection;
    mfxF32      BlueCorrection;
    mfxF32      GreenBottomCorrection;
} CameraPipeWhiteBalanceParams;


// from CanonLog10ToRec709_10_LUT_Ver.1.1, picked up 64 points
extern unsigned int gamma_point[64];

extern unsigned int gamma_correct[64];

//// Gamma parameters
typedef struct {
    mfxU16    Points[64];
    mfxU16    Correct[64];
} CP_FORWARD_GAMMA_HW_PARAMS;

typedef struct {
    mfxU16    Gamma_value;
} CP_FORWARD_GAMMA_SW_PARAMS;

typedef struct
{
    mfxU16    bActive;
    union {
        CP_FORWARD_GAMMA_HW_PARAMS  gamma_hw_params; // will not be exposed externally
        CP_FORWARD_GAMMA_SW_PARAMS  gamma; // 2.2, 1.8 etc
    };
} CameraPipeForwardGammaParams;


/*
class MfxFrameAllocResponse : public mfxFrameAllocResponse
{
public:
    MfxFrameAllocResponse();

    ~MfxFrameAllocResponse();

    mfxStatus Alloc(
        VideoCORE *            core,
        mfxFrameAllocRequest & req,
        bool isCopyRequired = true);

    mfxStatus Alloc(
        VideoCORE *            core,
        mfxFrameAllocRequest & req,
        mfxFrameSurface1 **    opaqSurf,
        mfxU32                 numOpaqSurf);

    mfxStatus Free( void );

private:
    MfxFrameAllocResponse(MfxFrameAllocResponse const &);
    MfxFrameAllocResponse & operator =(MfxFrameAllocResponse const &);

    VideoCORE * m_core;
    mfxU16      m_numFrameActualReturnedByAllocFrames;

    std::vector<mfxFrameAllocResponse> m_responseQueue;
    std::vector<mfxMemId>              m_mids;
};
*/

}
#endif //#if defined( AS_VPP_PLUGIN )
#endif //#if defined( _MFX_CAMERA_PLUGIN_UTILS_ )