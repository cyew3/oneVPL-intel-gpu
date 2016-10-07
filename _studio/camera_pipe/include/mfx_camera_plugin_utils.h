/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_utils.h

\* ****************************************************************************** */
#ifndef _MFX_CAMERA_PLUGIN_UTILS_H
#define _MFX_CAMERA_PLUGIN_UTILS_H

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include <vector>

#include "mfxvideo.h"
#include "mfxplugin++.h"
#include "mfx_plugin_module.h"
#include "mfx_session.h"
#include "mfxcamera.h"

#pragma warning(disable:4505)
#include "cm_def.h" // Needed for CM Vector
#pragma warning(push)
#pragma warning(disable:4100)
#include "cm_vm.h"  //
#pragma warning(pop)
#include "cmrt_cross_platform.h"
#include "vm_time.h"
#include "Campipe.h"
//#include "mfx_camera_plugin_cm.h"

#define  BAYER_BGGR  0x0000
#define  BAYER_RGGB  0x0001
#define  BAYER_GRBG  0x0002
#define  BAYER_GBRG  0x0003

#define  MFX_CAMERA_DEFAULT_ASYNCDEPTH 3

// 2 hours of playing at 30fps
#define CAMERA_FRAMES_TILL_HARD_RESET 2*108000

#define CHECK_HRES(hRes) \
    if (FAILED(hRes))\
    return MFX_ERR_DEVICE_FAILED;

//#define CAMERA_DEBUG_PRINTF

#ifdef CAMERA_DEBUG_PRINTF
 #define CAMERA_DEBUG_LOG(fmt, ...)\
{\
    f = fopen("CanonOut.txt", "at");\
    fprintf(f, fmt, __VA_ARGS__);\
    fclose(f);\
}
#else
    #define CAMERA_DEBUG_LOG(fmt, ...)
#endif

//#define WRITE_CAMERA_LOG
#ifdef WRITE_CAMERA_LOG
#define CAMERA_LOG(...) \
{\
    UMC::AutomaticUMCMutex guard(m_log_guard); \
    char fname[128] = "";\
    sprintf(fname, "camera_plugin_%p.log", m_session);\
    FILE *f = fopen(fname, "at");\
    fprintf(f, __VA_ARGS__); \
    fclose(f); \
}
#else
#define CAMERA_LOG(...)
#endif

#if defined( AS_VPP_PLUGIN) || defined(AS_CAMERA_PLUGIN)

#define CAMERA_CLIP(a, l, r) if (a < (l)) a = l; else if (a > (r)) a = r

namespace MfxCameraPlugin
{
static const mfxU32  NO_INDEX      = 0xffffffff;
static const mfxU8   NO_INDEX_U8   = 0xff;
static const mfxU16  NO_INDEX_U16 = 0xffff;

template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }

template<class T, size_t N> inline size_t SizeOf(T(&)[N]) { return N; }

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
    MEM_GPU
};

mfxStatus CheckIOPattern(mfxVideoParam *param, mfxVideoParam *out, mfxU32 mode);
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

    mfxStatus ReallocCmSurfaces(
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

    mfxU16 m_width;
    mfxU16 m_height;
    mfxU32 m_fourCC;
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
            mfxU32    bColorConversionMatrix            : 1;
            mfxU32    bForwardGammaCorrection           : 1;
            mfxU32    bBayerDenoise                     : 1;
            mfxU32    bOutToARGB16                      : 1;
            mfxU32    bNoPadding                        : 1; // must be ON now, zero meaning that padding needs to be done
            mfxU32    bLensCorrection                   : 1;
            mfxU32    b3DLUT                            : 1;
            mfxU32    Reserved                          : 19;
        };
        mfxU32      ModuleConfiguration;
    };
    bool          bGamma3DLUT;
    mfxU32        FrameSurfWidth                    :16;
    mfxU32        FrameSurfHeight                   :16;
    mfxU32        BayerPatternType                  :16;
    union {
      struct {
          mfxU32   InputMemoryOperationMode         : 8;
          mfxU32   OutputMemoryOperationMode        : 8;
      };
      mfxU32       MemoryOperationMode;
    };
    mfxU16    API_Mode;

    bool operator!= (const _mfxCameraCaps &newCaps) const
    {
        if ( this->Version != newCaps.Version ||
             this->ModuleConfiguration != newCaps.ModuleConfiguration)
             return true;

        return false;
    }

    mfxU16 GetFiltersNum(void)
    {
        mfxU16 num = 1; // Demosaic is always on

        if (bBlackLevelCorrection)   num++;
        if (bVignetteCorrection)     num++;
        if (bWhiteBalance)           num++;
        if (bHotPixel)               num++;
        if (bColorConversionMatrix)  num++;
        if (bForwardGammaCorrection) num++;
        if (bBayerDenoise)           num++;
        if (bNoPadding)              num++;
        if (bLensCorrection)         num++;
        if (b3DLUT)                  num++;
        if (bGamma3DLUT)             num++;

        return num;
    }

}   mfxCameraCaps;

typedef struct
{
    mfxU32              TileOffset;
} CameraTileOffset;

//// Gamma parameters
typedef struct
{
    mfxU16     numPoints;
    mfxU16    *gammaPoints;
    mfxU16    *gammaCorrect;
} CameraGammaLut;

typedef struct
{
    mfxU16                 NumSegments;
    mfxCamFwdGammaSegment  Segment[1024];
} CameraPipeForwardGammaParams;


typedef struct _CameraPipeVignette_unsigned_8_8
{
    mfxU16    integer   : 8;
    mfxU16    mantissa  : 8;
} CameraPipeVignette_unsigned_8_8;


typedef struct _CameraPipeVignetteCorrectionElem
{
    CameraPipeVignette_unsigned_8_8   R;
    CameraPipeVignette_unsigned_8_8   G0;
    CameraPipeVignette_unsigned_8_8   B;
    CameraPipeVignette_unsigned_8_8   G1;
    mfxU16                        reserved;
} CameraPipeVignetteCorrectionElem;

typedef struct _CameraPipeVignetteParams
{
    bool        bActive;
    mfxU16      Height;
    mfxU16      Width;
    mfxU16      CmWidth;
    mfxU16      Stride;
    mfxU16      CmStride;
    CameraPipeVignetteCorrectionElem *pCorrectionMap;
    CameraPipeVignetteCorrectionElem *pCmCorrectionMap;
    _CameraPipeVignetteParams() { pCmCorrectionMap = 0; }
} CameraPipeVignetteParams;

typedef struct _CameraParams
{
    mfxU32              frameWidth;
    mfxU32              frameWidth64;
    mfxU32              paddedFrameWidth;
    mfxU32              paddedFrameHeight;
    mfxU32              vSliceWidth;

    mfxU32              BitDepth;
    mfxCameraCaps       Caps;

    CameraPipeForwardGammaParams   GammaParams;
    CameraPipeVignetteParams       VignetteParams;
    // Tile specific data
    mfxU16              tileNumHor;
    mfxU16              tileNumVer;
    mfxU32              TileWidth;
    mfxU32              TileHeight;
    mfxU32              TileHeightPadded;
    CameraTileOffset    *tileOffsets;
    mfxFrameInfo        TileInfo;
    mfxVideoParam       par;
    bool operator!= (const _CameraParams &new_frame) const
    {
        if (this->frameWidth64      != new_frame.frameWidth64      ||
            this->paddedFrameHeight != new_frame.paddedFrameHeight ||
            this->BitDepth          != new_frame.BitDepth          ||
            this->paddedFrameWidth  != new_frame.paddedFrameWidth  ||
            this->TileHeight        != new_frame.TileHeight        ||
            this->TileHeightPadded  != new_frame.TileHeightPadded  ||
            this->tileNumHor        != new_frame.tileNumHor        || 
            this->tileNumVer        != new_frame.tileNumVer)
        {
            return true;
        }

        return false;

    }
} CameraParams;

typedef struct _CameraPipeBlackLevelParams
{
    bool        bActive;
    mfxF32      RedLevel;
    mfxF32      GreenTopLevel;
    mfxF32      BlueLevel;
    mfxF32      GreenBottomLevel;
} CameraPipeBlackLevelParams;


typedef struct _CameraPipeDenoiseParams
{
    bool        bActive;
    mfxU16      Threshold;
} CameraPipeDenoiseParams;

typedef struct _CameraPipeHotPixelParams
{
    bool        bActive;
    mfxU16      PixelThresholdDifference;
    mfxU16      PixelCountThreshold;
} CameraPipeHotPixelParams;


typedef struct _CameraPipeWhiteBalanceParams
{
    bool        bActive;
    mfxU32      Mode;
    mfxF64      RedCorrection;
    mfxF64      GreenTopCorrection;
    mfxF64      BlueCorrection;
    mfxF64      GreenBottomCorrection;
} CameraPipeWhiteBalanceParams;

typedef struct _CameraPipeLensCorrectionParams
{
    bool        bActive;
    mfxF32      a[3];
    mfxF32      b[3];
    mfxF32      c[3];
    mfxF32      d[3];
} CameraPipeLensCorrectionParams;

typedef struct _CameraPipe3x3ColorConversionParams
{
    bool        bActive;
    mfxF64      CCM[3][3];
} CameraPipe3x3ColorConversionParams;

#define MFX_CAM_GAMMA_NUM_POINTS_SKL 64


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

typedef struct
{
    mfxU32 size;
    mfxCam3DLutEntry *lut;
} CameraPipe3DLUTParams;
struct AsyncParams
{
    mfxFrameSurface1 *surf_in;
    mfxFrameSurface1 *surf_out;

    mfxMemId inSurf2D;
    mfxMemId inSurf2DUP;
    mfxU32   tileIDHorizontal;
    mfxU32   tileIDVertical;

    CameraPipeDenoiseParams            DenoiseParams;
    CameraPipeHotPixelParams           HPParams;
    CameraPipeWhiteBalanceParams       WBparams;
    CameraPipeForwardGammaParams       GammaParams;
    CameraPipeVignetteParams           VignetteParams;
    CameraPipePaddingParams            PaddingParams;
    CameraPipeBlackLevelParams         BlackLevelParams;
    CameraPipe3x3ColorConversionParams CCMParams;
    CameraPipeLensCorrectionParams     LensParams;
    CameraPipe3DLUTParams              LUTParams;
    CameraParams                       FrameSizeExtra;
    mfxCameraCaps                      Caps;
    mfxU32                             InputBitDepth;

    mfxMemId outSurf2D;
    mfxMemId outSurf2DUP;

    // DDI specific params
    mfxU32 nDDIIndex;
    mfxU32 surfInIndex;
    mfxU32 surfOutIndex;

    void      *pEvent;
};

class CameraProcessor
{
public:
    CameraProcessor()
        : m_core(0)
        , m_params()
    {};
    virtual ~CameraProcessor() {};
    virtual mfxStatus Init(mfxVideoParam *par) = 0;
    virtual mfxStatus Init(CameraParams  *pipeParams) = 0;
    virtual mfxStatus Reset(mfxVideoParam *par, CameraParams *pipeParams) = 0;
    virtual void SetCore(VideoCORE *core) { m_core = core; };
    virtual mfxStatus AsyncRoutine(AsyncParams *pParam) = 0;
    virtual mfxStatus CompleteRoutine(AsyncParams *pParam) = 0;
    virtual mfxStatus Close() = 0;
protected:
    virtual mfxStatus CheckIOPattern(mfxU16  IOPattern) = 0;
    VideoCORE*    m_core;
    mfxVideoParam m_params;
};

}
#endif //#if defined( AS_VPP_PLUGIN )

#endif
