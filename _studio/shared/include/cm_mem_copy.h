/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __CM_MEM_COPY_H__
#define __CM_MEM_COPY_H__

#include "mfxdefs.h"
#include "mfxstructures.h"
#include "ippi.h"
#if defined(WIN64) || defined(WIN32)
#include "skl_copy_kernel_genx_isa.h"
#if !(defined(AS_VPP_PLUGIN) || defined(UNIFIED_PLUGIN) || defined(AS_H265FEI_PLUGIN) || defined(AS_H264LA_PLUGIN))
#include "cht_copy_kernel_genx_isa.h"
#endif
#endif
#if !defined(OSX)

#pragma warning(disable: 4505)
#pragma warning(disable: 4100)
#pragma warning(disable: 4201)

//#include "cm_def.h"
//#include "cm_vm.h"
#include "cmrt_cross_platform.h"

#include <algorithm>
#include <set>
#include <map>

typedef mfxI32 cmStatus;

#define BLOCK_PIXEL_WIDTH   (32)
#define BLOCK_HEIGHT        (8)

// CM 3.0 restriction, need to change in future
#define CM_MAX_GPUCOPY_SURFACE_WIDTH_IN_BYTE 65408
#ifdef MFX_VA_WIN
#define CM_MAX_GPUCOPY_SURFACE_HEIGHT        12000
#else
#define CM_MAX_GPUCOPY_SURFACE_HEIGHT        4088
#endif

#define CM_SUPPORTED_COPY_SIZE(ROI) (ROI.width <= CM_MAX_GPUCOPY_SURFACE_WIDTH_IN_BYTE && ROI.height <= CM_MAX_GPUCOPY_SURFACE_HEIGHT )
#define CM_ALIGNED(PTR) (!((Ipp64u(PTR))&0xf))
#define CM_ALIGNED64(PTR) (!((Ipp64u(PTR))&0x3f))

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

class CmDevice;
class CmBuffer;
class CmBufferUP;
class CmSurface2D;
class CmEvent;
class CmQueue;
class CmProgram;
class CmKernel;
class SurfaceIndex;
class CmThreadSpace;
class CmTask;
struct IDirect3DSurface9;
struct IDirect3DDeviceManager9;


class CmCopyWrapper
{
public:
    
    // constructor   
    CmCopyWrapper();

    // destructor
    virtual ~CmCopyWrapper(void);

    template <typename D3DAbstract>
    CmDevice* GetCmDevice(D3DAbstract *pD3D)
    {
        cmStatus cmSts = CM_SUCCESS;
        mfxU32 version;

        if (m_pCmDevice)
            return m_pCmDevice;

        cmSts = ::CreateCmDevice(m_pCmDevice, version, pD3D, CM_DEVICE_CREATE_OPTION_SCRATCH_SPACE_DISABLE);
        if (cmSts != CM_SUCCESS)
            return NULL;

        if (CM_1_0 > version)
        {
            return NULL;
        }

        return m_pCmDevice;
    };

#if defined(MFX_VA_LINUX) || defined(MFX_VA_ANDROID)
    CmDevice* GetCmDevice(VADisplay dpy)
    {
        cmStatus cmSts = CM_SUCCESS;
        mfxU32 version;

        if (m_pCmDevice)
            return m_pCmDevice;

        cmSts = ::CreateCmDevice(m_pCmDevice, version, dpy, CM_DEVICE_CREATE_OPTION_SCRATCH_SPACE_DISABLE);
        if (cmSts != CM_SUCCESS)
            return NULL;

        if (CM_1_0 > version)
        {
            return NULL;
        }
        return m_pCmDevice;
    };
#endif

    // initialize available functionality
    mfxStatus Initialize(eMFXHWType hwtype = MFX_HW_UNKNOWN);
    mfxStatus InitializeSwapKernels(eMFXHWType hwtype = MFX_HW_UNKNOWN);

    // release object
    mfxStatus Release(void);

    // check input parameters
    mfxStatus IsCmCopySupported(mfxFrameSurface1 *pSurface, IppiSize roi);


    mfxStatus CopyVideoToSystemMemoryAPI(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, void *pSrc, mfxU32 srcPitch, IppiSize roi);

    mfxStatus CopySystemToVideoMemoryAPI(void *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi);

    mfxStatus CopyVideoToVideoMemoryAPI(void *pDst, void *pSrc, IppiSize roi);
    
    mfxStatus CopySwapVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, void *pSrc, mfxU32 srcPitch, IppiSize roi, mfxU32 format);
    mfxStatus CopySwapSystemToVideoMemory(void *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 format);
    mfxStatus CopySwapVideoToVideoMemory(void *pDst, void *pSrc, IppiSize roi, mfxU32 format);

    mfxStatus ReleaseCmSurfaces(void);
    mfxStatus EnqueueCopySwapRBGPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride, 
                                    const UINT heightStride,
                                    mfxU32 format, 
                                    const UINT option,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopySwapRBCPUtoGPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride, 
                                    const UINT heightStride,
                                    mfxU32 format, 
                                    const UINT option,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopySwapRBGPUtoGPU(   CmSurface2D* pSurfaceIn,
                                    CmSurface2D* pSurfaceOut,
                                    int width,
                                    int height,
                                    mfxU32 format, 
                                    const UINT option,
                                    CmEvent* & pEvent );
protected:

    CmDevice  *m_pCmDevice;
    CmProgram *m_pCmProgram;

    //std::map<mfxU32, CmThreadSpace *> m_tableThreadSpace;
    CmThreadSpace *m_pThreadSpace;

    CmQueue *m_pCmQueue;
    CmTask  *m_pCmTask1;
    CmTask  *m_pCmTask2;

    CmSurface2D *m_pCmSurface2D;
    CmBufferUP *m_pCmUserBuffer;

    SurfaceIndex *m_pCmSrcIndex;
    SurfaceIndex *m_pCmDstIndex;

    std::set<mfxU8 *> m_cachedObjects;

    std::map<void *, CmSurface2D *> m_tableCmRelations;
    std::map<mfxU8 *, CmBufferUP *> m_tableSysRelations;

    std::map<CmSurface2D *, SurfaceIndex *> m_tableCmIndex;
    std::map<CmBufferUP *,  SurfaceIndex *> m_tableSysIndex;

    std::map<void *, CmSurface2D *> m_tableCmRelations2;
    std::map<mfxU8 *, CmBufferUP *> m_tableSysRelations2;

    std::map<CmSurface2D *, SurfaceIndex *> m_tableCmIndex2;
    std::map<CmBufferUP *,  SurfaceIndex *> m_tableSysIndex2;

    CmSurface2D * CreateCmSurface2D(void *pSrc, mfxU32 width, mfxU32 height, bool isSecondMode, 
                                    std::map<void *, CmSurface2D *> & tableCmRelations,
                                    std::map<CmSurface2D *, SurfaceIndex *> & tableCmIndex);

    CmBufferUP  * CreateUpBuffer(mfxU8 *pDst, mfxU32 memSize, 
                                 std::map<mfxU8 *, CmBufferUP *> & tableSysRelations,
                                 std::map<CmBufferUP *,  SurfaceIndex *> & tableSysIndex);

};

#endif // !defined(MFX_VA_OSX)

#endif // __CM_MEM_COPY_H__
