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

//#define CAMP_PIPE_ITT

#if !defined(_MFX_CAMERA_PLUGIN_UTILS_)
#define _MFX_CAMERA_PLUGIN_UTILS_

#if defined( AS_VPP_PLUGIN )

#define CAMERA_CLIP(a, l, r) if (a < (l)) a = l; else if (a > (r)) a = r

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

enum {
    MFX_CAM_QUERY_SET0 = 0,
    MFX_CAM_QUERY_SET1,
    MFX_CAM_QUERY_RETURN_STATUS,
    MFX_CAM_QUERY_SIGNAL,
    MFX_CAM_QUERY_CHECK_RANGE
};

// GPU memory access mode
enum
{
    MEM_GPUSHARED   =  0,
    MEM_FASTGPUCPY,
    MEM_CPUCPY
};

mfxStatus CheckExtBuffers(mfxVideoParam *param, mfxVideoParam *out, mfxU32 mode);

class MfxFrameAllocResponse : public mfxFrameAllocResponse
{
public:
    MfxFrameAllocResponse();

    ~MfxFrameAllocResponse();

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

    void Free();

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


typedef struct 
{
    mfxU32              frameWidth64;
    mfxU32              paddedFrameWidth;
    mfxU32              paddedFrameHeight;
    mfxU32              vSliceWidth;
} CameraFrameSizeExtra;


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
            mfxU32    bColorConversionMatrix            : 1;
            mfxU32    bForwardGammaCorrection           : 1;
            mfxU32    bBayerDenoise                     : 1;
            mfxU32    bOutToARGB8                       : 1;
            mfxU32    bOutToARGB16                      : 1;
            mfxU32    bNoPadding                        : 1; // must be ON now, zero meaning that padding needs to be done
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
    mfxU16    API_Mode;
}   mfxCameraCaps;


typedef struct _CameraPipeWhiteBalanceParams
{
    //bool        bActive;
    //mfxU32        Mode;
    mfxF32      RedCorrection;
    mfxF32      GreenTopCorrection;
    mfxF32      BlueCorrection;
    mfxF32      GreenBottomCorrection;
} CameraPipeWhiteBalanceParams;


// from CanonLog10ToRec709_10_LUT_Ver.1.1, picked up 64 points

#define MFX_CAM_DEFAULT_NUM_GAMMA_POINTS 64
#define MFX_CAM_DEFAULT_GAMMA_DEPTH 10

extern mfxU16 default_gamma_point[MFX_CAM_DEFAULT_NUM_GAMMA_POINTS];
extern mfxU16 default_gamma_correct[MFX_CAM_DEFAULT_NUM_GAMMA_POINTS];

//// Gamma parameters
typedef struct 
{
    mfxU16     numPoints;
    mfxU16    *gammaPoints;
    mfxU16    *gammaCorrect;
} CameraGammaLut;

typedef struct
{
    CameraGammaLut  gamma_lut; // will not be exposed externally (???)
    mfxF64          gamma_value; // 2.2, 1.8 etc
} CameraPipeForwardGammaParams;

// move to mfxcamera.h ???
#define MFX_CAM_MIN_REQUIRED_PADDING_TOP    8
#define MFX_CAM_MIN_REQUIRED_PADDING_BOTTOM 8
#define MFX_CAM_MIN_REQUIRED_PADDING_LEFT   8
#define MFX_CAM_MIN_REQUIRED_PADDING_RIGHT  8

#define MFX_CAM_DEFAULT_PADDING_TOP    MFX_CAM_MIN_REQUIRED_PADDING_TOP
#define MFX_CAM_DEFAULT_PADDING_BOTTOM MFX_CAM_MIN_REQUIRED_PADDING_BOTTOM
#define MFX_CAM_DEFAULT_PADDING_LEFT   MFX_CAM_MIN_REQUIRED_PADDING_LEFT
#define MFX_CAM_DEFAULT_PADDING_RIGHT  MFX_CAM_MIN_REQUIRED_PADDING_RIGHT

typedef struct 
{
    //mfxU16 mode;
    mfxU16 top;
    mfxU16 bottom;
    mfxU16 left;
    mfxU16 right;
} CameraPipePaddingParams;
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