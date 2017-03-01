//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_screen_capture_cm_dirty_rect.h"
#include "mfx_utils.h"
#include "mfxstructures.h"
#include <memory>
#include <list>
#include <algorithm> //min
#include <assert.h>

#include "dirty_rect_genx_hsw_isa.h"
#include "dirty_rect_genx_bdw_isa.h"
#include "dirty_rect_genx_skl_isa.h"
#include "dirty_rect_genx_jit_isa.h"

#define H_BLOCKS 16
#define W_BLOCKS 16

#if !defined(MSDK_ALIGN32)
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 32
#endif
#ifndef ALIGN16
#define ALIGN16(SZ) (((SZ + 15) >> 4) << 4) // round up to a multiple of 16
#define ALIGN16_DOWN(SZ) (((SZ ) >> 4) << 4) // round up to a multiple of 16
#endif

#define MFXSC_CHECK_CM_RESULT_AND_PTR(result, ptr)  \
do {                                                \
    if(CM_SUCCESS != result || 0 == ptr)            \
    {                                               \
        assert(CM_SUCCESS == result);               \
        Close();                                    \
        return MFX_ERR_DEVICE_FAILED;               \
    }                                               \
} while (0,0)

#define MFXSC_CHECK_IPP_RESULT_AND_PTR(result, ptr) \
do {                                                \
    if(ippStsNoErr != result || 0 == ptr)           \
    {                                               \
        assert(ippStsNoErr == result);              \
        Close();                                    \
        return MFX_ERR_DEVICE_FAILED;               \
    }                                               \
} while (0,0)

#define MFXSC_CHECK_MFX_RESULT_AND_PTR(result, ptr) \
do {                                                \
    if(MFX_ERR_NONE != result || 0 == ptr)          \
    {                                               \
        assert(MFX_ERR_NONE == result);             \
        Close();                                    \
        return MFX_ERR_DEVICE_FAILED;               \
    }                                               \
} while (0,0)

namespace MfxCapture
{

template<typename T>
T* GetExtendedBuffer(const mfxU32& BufferId, const mfxFrameSurface1* surf)
{
    if(!surf || !BufferId)
        return 0;
    if(!surf->Data.NumExtParam || !surf->Data.ExtParam)
        return 0;
    for(mfxU32 i = 0; i < surf->Data.NumExtParam; ++i)
    {
        if(!surf->Data.ExtParam[i])
            continue;
        if(BufferId == surf->Data.ExtParam[i]->BufferId && sizeof(T) == surf->Data.ExtParam[i]->BufferSz)
            return (T*) surf->Data.ExtParam[i];
    }

    return 0;
}

CmDirtyRectFilter::CmDirtyRectFilter(const mfxCoreInterface* _core)
    : m_pmfxCore(_core)
    , m_mfxDeviceType(MFX_HANDLE_D3D11_DEVICE)
    , m_mfxDeviceHdl(NULL)
    , m_pCMDevice(NULL)
    , m_pQueue(NULL)
    , m_width(0)
    , m_height(0)
    , m_bSysMem(false)
    , m_bOpaqMem(false)
    , m_mode(TMP_MFXSC_DIRTY_RECT_DEFAULT)
{
    Mode = CM_DR;
}

CmDirtyRectFilter::~CmDirtyRectFilter()
{
    Close();
    FreeSurfs();
}

mfxStatus CmDirtyRectFilter::Init(const mfxVideoParam* par, bool isSysMem, bool isOpaque)
{
    if(!par) return MFX_ERR_NULL_PTR;

    m_bSysMem  = isSysMem;
    m_bOpaqMem = isOpaque;

    mfxStatus mfxRes = MFX_ERR_NONE;
    if(!m_pmfxCore)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxCoreParam param = {0};
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &param);
    MFX_CHECK_STS(mfxRes);

    if(MFX_IMPL_HARDWARE != MFX_IMPL_BASETYPE(param.Impl))
        return MFX_ERR_UNSUPPORTED;

    mfxHandleType type = (MFX_IMPL_VIA_D3D11 == (param.Impl & 0x0F00)) ? MFX_HANDLE_D3D11_DEVICE : MFX_HANDLE_D3D9_DEVICE_MANAGER;
    mfxHDL handle = 0;
    mfxRes = m_pmfxCore->CreateAccelerationDevice(m_pmfxCore->pthis, type, &handle);
    MFX_CHECK_STS(mfxRes);
    if(0 == handle)
        return MFX_ERR_NOT_FOUND;

    m_mfxDeviceType = type;
    m_mfxDeviceHdl  = handle;

    UINT version = 0;
    int result = CM_FAILURE;//CreateCmDevice(pDevice, version);
#if defined(_WIN32) || defined(_WIN64)
    if(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 == m_mfxDeviceType)
        result = CreateCmDevice(m_pCMDevice,version,(IDirect3DDeviceManager9*) m_mfxDeviceHdl);
    else if(MFX_HANDLE_D3D11_DEVICE == m_mfxDeviceType)
        result = CreateCmDevice(m_pCMDevice,version,(ID3D11Device*) m_mfxDeviceHdl);
    else
        return MFX_ERR_UNKNOWN;
#else if defined(LINUX32) || defined (LINUX64)
    unsupported;
#endif

    if(CM_SUCCESS != result || 0 == m_pCMDevice)
        return MFX_ERR_DEVICE_FAILED;

    bool jit = true;
    const unsigned char * buffer = dirty_rect_genx_jit;
    unsigned int len = sizeof(dirty_rect_genx_jit);

    GPU_PLATFORM platform = PLATFORM_INTEL_UNKNOWN;
    size_t cap_size = sizeof(platform);
    m_pCMDevice->GetCaps(CAP_GPU_PLATFORM, cap_size, &platform);

    switch ( platform )
    {
        case PLATFORM_INTEL_HSW:
            jit = false;
            buffer = dirty_rect_genx_hsw;
            len = sizeof(dirty_rect_genx_hsw);
            break;

        case PLATFORM_INTEL_BDW:
        case PLATFORM_INTEL_CHV:
            jit = false;
            buffer = dirty_rect_genx_bdw;
            len = sizeof(dirty_rect_genx_bdw);
            break;

        case PLATFORM_INTEL_SKL:
        case PLATFORM_INTEL_BXT:
            jit = false;
            buffer = dirty_rect_genx_skl;
            len = sizeof(dirty_rect_genx_skl);
            break;

        case PLATFORM_INTEL_VLV:
        case PLATFORM_INTEL_SNB:
        case PLATFORM_INTEL_IVB:
        case PLATFORM_INTEL_CNL:
        default:
            break;
    }

    CmProgram * pProgram = NULL;
    if(jit)
        result = ::ReadProgramJit(m_pCMDevice, pProgram, buffer, len);
    else
        result = ::ReadProgram(m_pCMDevice, pProgram, buffer, len);

    if(CM_SUCCESS != result || 0 == pProgram)
    {
        jit = true;
        buffer = dirty_rect_genx_jit;
        len = sizeof(dirty_rect_genx_jit);

        result = ::ReadProgramJit(m_pCMDevice, pProgram, buffer, len);
        if(CM_SUCCESS != result || 0 == pProgram)
            return MFX_ERR_DEVICE_FAILED;
    }

    m_vPrograms.push_back(pProgram);

    result = m_pCMDevice->CreateQueue(m_pQueue);
    if(CM_SUCCESS != result || 0 == m_pQueue)
        return MFX_ERR_DEVICE_FAILED;

    const mfxU16 width  = m_width  = (par->mfx.FrameInfo.CropW ? par->mfx.FrameInfo.CropW : par->mfx.FrameInfo.Width );
    const mfxU16 height = m_height = (par->mfx.FrameInfo.CropH ? par->mfx.FrameInfo.CropH : par->mfx.FrameInfo.Height);

    m_mode = TMP_MFXSC_DIRTY_RECT_DEFAULT;

    //Init resources for each task
    const mfxU16 AsyncDepth = IPP_MAX(1, par->AsyncDepth);
    TaskContext* pTaskCtx = 0;
    for(mfxU32 i = 0; i < AsyncDepth; ++i)
    {
        try{
            pTaskCtx = new TaskContext;
        } catch(...) {
            Close();
            return MFX_ERR_MEMORY_ALLOC;
        }
        IppStatus resultIPP = ippsSet_8u(0, (Ipp8u*)pTaskCtx, sizeof(TaskContext));
        MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

        m_vpTasks.push_back(pTaskCtx);

        result = m_pCMDevice->CreateQueue(pTaskCtx->pQueue);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pQueue);

        if(MFX_FOURCC_NV12 == par->mfx.FrameInfo.FourCC)
            result = m_pCMDevice->CreateKernel(pProgram, CM_KERNEL_FUNCTION( cmkDirtyRectNV12<16U, 16U> ), pTaskCtx->pKernel, 0);
        else
            result = m_pCMDevice->CreateKernel(pProgram, CM_KERNEL_FUNCTION( cmkDirtyRectRGB4<16U, 16U> ), pTaskCtx->pKernel, 0);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pKernel);

        result = pTaskCtx->pKernel->SetKernelArg(0, sizeof(width), &width);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, width);

        result = pTaskCtx->pKernel->SetKernelArg(1, sizeof(height), &height);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, height);

        result = m_pCMDevice->CreateTask(pTaskCtx->pTask);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pTask);

        result = pTaskCtx->pTask->AddKernel(pTaskCtx->pKernel);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pTask);

        const UINT threadsWidth  = (m_width +  16 - 1) / 16; //from cmut::DivUp
        const UINT threadsHeight = (m_height + 16 - 1) / 16;
        result = m_pCMDevice->CreateThreadSpace(threadsWidth, threadsHeight, pTaskCtx->pThreadSpace);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pThreadSpace);

        result = pTaskCtx->pKernel->SetThreadCount(threadsWidth * threadsHeight);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, threadsWidth * threadsHeight);

        UINT pitch = 0;
        UINT physSize = 0;
        UINT roi_W = 4 * threadsWidth,
             roi_H = 4 * threadsHeight; // 4x4 extension is needed, because CM returns at least 4x4 matrix

        result = m_pCMDevice->GetSurface2DInfo(roi_W, roi_H, CM_SURFACE_FORMAT_P8, pitch, physSize);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pitch + physSize);

        pTaskCtx->roiMap.physBuffer = (mfxU8*) CM_ALIGNED_MALLOC(physSize, 0x1000);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->roiMap.physBuffer);

        pTaskCtx->roiMap.physBufferSz = physSize;
        pTaskCtx->roiMap.physBufferPitch = pitch;

        result = m_pCMDevice->CreateSurface2DUP(roi_W, roi_H, CM_SURFACE_FORMAT_P8, pTaskCtx->roiMap.physBuffer, pTaskCtx->roiMap.cmSurf);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->roiMap.cmSurf);

        SurfaceIndex* surfIndex;
        result = pTaskCtx->roiMap.cmSurf->GetIndex(surfIndex);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->roiMap.cmSurf);

        result = pTaskCtx->pKernel->SetKernelArg(2, sizeof(SurfaceIndex), surfIndex);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->roiMap.cmSurf);

        pTaskCtx->roiMap.width  = ALIGN16(threadsWidth);
        pTaskCtx->roiMap.height = ALIGN16(threadsHeight);

        pTaskCtx->roiMap.roiMAP    = ippsMalloc_8u(pTaskCtx->roiMap.width * pTaskCtx->roiMap.height);
        MFXSC_CHECK_IPP_RESULT_AND_PTR(ippStsNoErr, pTaskCtx->roiMap.roiMAP);

        pTaskCtx->roiMap.distUpper = ippsMalloc_16s(pTaskCtx->roiMap.width * pTaskCtx->roiMap.height);
        MFXSC_CHECK_IPP_RESULT_AND_PTR(ippStsNoErr, pTaskCtx->roiMap.distUpper);

        pTaskCtx->roiMap.distLower = ippsMalloc_16s(pTaskCtx->roiMap.width * pTaskCtx->roiMap.height);
        MFXSC_CHECK_IPP_RESULT_AND_PTR(ippStsNoErr, pTaskCtx->roiMap.distLower);

        pTaskCtx->roiMap.distLeft  = ippsMalloc_16s(pTaskCtx->roiMap.width * pTaskCtx->roiMap.height);
        MFXSC_CHECK_IPP_RESULT_AND_PTR(ippStsNoErr, pTaskCtx->roiMap.distLeft);

        pTaskCtx->roiMap.distRight = ippsMalloc_16s(pTaskCtx->roiMap.width * pTaskCtx->roiMap.height);
        MFXSC_CHECK_IPP_RESULT_AND_PTR(ippStsNoErr, pTaskCtx->roiMap.distRight);

        pTaskCtx->roiMap.dirtyRects.reserve(pTaskCtx->roiMap.width / 2 * pTaskCtx->roiMap.height / 2); //reserve vector for the worst case - chessboard
        MFXSC_CHECK_IPP_RESULT_AND_PTR(ippStsNoErr, (mfxI32)(pTaskCtx->roiMap.width / 2 * pTaskCtx->roiMap.height / 2) == (mfxI32)pTaskCtx->roiMap.dirtyRects.capacity());
    }

    return MFX_ERR_NONE;
}

mfxStatus CmDirtyRectFilter::Init(const mfxFrameInfo& in, const mfxFrameInfo& out)
{
    in;
    out;

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CmDirtyRectFilter::RunFrameVPP(mfxFrameSurface1& in, mfxFrameSurface1& out)
{
    TaskContext* task = GetFreeTask();
    if(!task) return MFX_ERR_DEVICE_FAILED;

    mfxExtDirtyRect* mfxRect = GetExtendedBuffer<mfxExtDirtyRect>(MFX_EXTBUFF_DIRTY_RECTANGLES, &out);
    if(!mfxRect) return MFX_ERR_UNDEFINED_BEHAVIOR;

    Ipp8u* roiMap = task->roiMap.roiMAP;
    if(!roiMap)  return MFX_ERR_UNDEFINED_BEHAVIOR;

    Ipp16s* distUpper = task->roiMap.distUpper;
    Ipp16s* distLower = task->roiMap.distLower;
    Ipp16s* distLeft  = task->roiMap.distLeft;
    Ipp16s* distRight = task->roiMap.distRight;
    if(!distUpper || !distLower || !distLeft || !distRight) return MFX_ERR_UNDEFINED_BEHAVIOR;

    const IppiSize roiMapSize = {(int)(task->roiMap.width), (int)(task->roiMap.height)};
    const IppiSize roiPhysBufferSize = {(int)(task->roiMap.physBufferPitch / 4), (int)(task->roiMap.physBufferSz / task->roiMap.physBufferPitch / 4)};

    std::vector <intRect>& dirtyRects = task->roiMap.dirtyRects;
    dirtyRects.clear();

    IppStatus resultIPP = ippiSet_8u_C1R(0, roiMap, roiMapSize.width, roiMapSize);
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

    resultIPP = ippiSet_32s_C1R(0, (Ipp32s*)task->roiMap.physBuffer, task->roiMap.physBufferPitch * 4, roiPhysBufferSize);
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

    int resultCM = 0;

    task->pTask->Reset();

    resultCM = task->pTask->AddKernel(task->pKernel);
    MFXSC_CHECK_CM_RESULT_AND_PTR(resultCM, 1);

    SurfaceIndex * pSurfIn  = GetCMSurfaceIndex(&in);
    MFXSC_CHECK_CM_RESULT_AND_PTR(CM_SUCCESS, pSurfIn);

    SurfaceIndex * pSurfOut = GetCMSurfaceIndex(&out);
    MFXSC_CHECK_CM_RESULT_AND_PTR(CM_SUCCESS, pSurfOut);

    resultCM = task->pKernel->SetKernelArg(3, sizeof(SurfaceIndex), pSurfIn);
    MFXSC_CHECK_CM_RESULT_AND_PTR(resultCM, 1);

    resultCM = task->pKernel->SetKernelArg(4, sizeof(SurfaceIndex), pSurfOut);
    MFXSC_CHECK_CM_RESULT_AND_PTR(resultCM, 1);

    CmEvent* pEvent = 0;
    resultCM = task->pQueue->Enqueue(task->pTask, pEvent, task->pThreadSpace);
    MFXSC_CHECK_CM_RESULT_AND_PTR(resultCM, 1);

    CM_STATUS status;
    pEvent->GetStatus(status);
    while (status != CM_STATUS_FINISHED) {
        pEvent->GetStatus(status);
    }

    task->pQueue->DestroyEvent(pEvent);
    pEvent = 0;

    resultIPP = ippiConvert_32s8u_C1R((const Ipp32s*)task->roiMap.physBuffer, task->roiMap.physBufferPitch * 4, roiMap, roiMapSize.width, roiPhysBufferSize);
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

    const mfxU32 maxRectsN = 256;
    mfxStatus resultMFX;
    IppiSize roiBlock = {W_BLOCKS, H_BLOCKS};

    mfxU32 blocksMergerN = 0;
    IppiSize roiMapNewSize = roiMapSize;

    switch (m_mode)
    {
    case TMP_MFXSC_DIRTY_RECT_DEFAULT:
        resultMFX = BuildRectangles(roiMap, roiMapSize, maxRectsN, dirtyRects);
        MFXSC_CHECK_MFX_RESULT_AND_PTR(resultMFX, 1);
        break;
    case TMP_MFXSC_DIRTY_RECT_EXT_16x16:
    case TMP_MFXSC_DIRTY_RECT_EXT_32x32:
    case TMP_MFXSC_DIRTY_RECT_EXT_64x64:
    case TMP_MFXSC_DIRTY_RECT_EXT_128x128:
    case TMP_MFXSC_DIRTY_RECT_EXT_256x256:

        if (m_mode == TMP_MFXSC_DIRTY_RECT_EXT_16x16)   { blocksMergerN = 0; roiBlock.width = 16;  roiBlock.height = 16;  }
        if (m_mode == TMP_MFXSC_DIRTY_RECT_EXT_32x32)   { blocksMergerN = 1; roiBlock.width = 32;  roiBlock.height = 32;  }
        if (m_mode == TMP_MFXSC_DIRTY_RECT_EXT_64x64)   { blocksMergerN = 2; roiBlock.width = 64;  roiBlock.height = 64;  }
        if (m_mode == TMP_MFXSC_DIRTY_RECT_EXT_128x128) { blocksMergerN = 3; roiBlock.width = 128; roiBlock.height = 128; }
        if (m_mode == TMP_MFXSC_DIRTY_RECT_EXT_256x256) { blocksMergerN = 4; roiBlock.width = 256; roiBlock.height = 256; }

        for (mfxU32 n = 0; n < blocksMergerN; n++)
        {
            roiMapNewSize.width  /= 2;
            roiMapNewSize.height /= 2;

            for (mfxI32 i = 0; i < roiMapNewSize.height; i++)
                for (mfxI32 j = 0; j < roiMapNewSize.width; j++)
                    roiMap[i * roiMapNewSize.width + j] = roiMap[ i * 2      * 2 * roiMapNewSize.width + j * 2]
                                                       || roiMap[ i * 2      * 2 * roiMapNewSize.width + j * 2 + 1]
                                                       || roiMap[(i * 2 + 1) * 2 * roiMapNewSize.width + j * 2]
                                                       || roiMap[(i * 2 + 1) * 2 * roiMapNewSize.width + j * 2 + 1];
        }

        resultMFX = BuildRectanglesExt(roiMap, distUpper, distLower, distLeft, distRight, roiMapNewSize, dirtyRects);
        MFXSC_CHECK_MFX_RESULT_AND_PTR(resultMFX, 1);
        break;
    default:
        resultMFX = MFX_ERR_UNDEFINED_BEHAVIOR;
        MFXSC_CHECK_MFX_RESULT_AND_PTR(resultMFX, 1);
        break;
    }

    mfxExtDirtyRect tmpRect;
    resultIPP = ippsSet_8u(0, (Ipp8u*)&tmpRect, sizeof(tmpRect));
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

    tmpRect.Header.BufferId = MFX_EXTBUFF_DIRTY_RECTANGLES;
    tmpRect.Header.BufferSz = sizeof(tmpRect);

    if (dirtyRects.size() > maxRectsN)
    {
        mfxI32 left   = roiMapNewSize.width,
               right  = -1,
               top    = roiMapNewSize.height,
               bottom = -1;

        for (mfxU32 i = 0; i < dirtyRects.size(); i++)
        {
            if (left   > (mfxI32)dirtyRects[i].Left  ) left   = dirtyRects[i].Left;
            if (right  < (mfxI32)dirtyRects[i].Right ) right  = dirtyRects[i].Right;
            if (top    > (mfxI32)dirtyRects[i].Top   ) top    = dirtyRects[i].Top;
            if (bottom < (mfxI32)dirtyRects[i].Bottom) bottom = dirtyRects[i].Bottom;
        }

        assert((0 <= left) && (left <= right ) && (right  <= roiMapNewSize.width));
        assert((0 <= top ) && (top  <= bottom) && (bottom <= roiMapNewSize.height));

        tmpRect.Rect[0].Left   = left * roiBlock.width;
        tmpRect.Rect[0].Right  = IPP_MIN(right  * roiBlock.width, m_width);
        tmpRect.Rect[0].Top    = top * roiBlock.height;
        tmpRect.Rect[0].Bottom = IPP_MIN(bottom * roiBlock.height, m_height);

        tmpRect.NumRect = 1;
    }
    else
    {
        for (mfxU32 i = 0; i < dirtyRects.size(); i++)
        {
            tmpRect.Rect[i].Left   = dirtyRects[i].Left * roiBlock.width;
            tmpRect.Rect[i].Right  = IPP_MIN(dirtyRects[i].Right  * roiBlock.width, m_width);
            tmpRect.Rect[i].Top    = dirtyRects[i].Top * roiBlock.height;
            tmpRect.Rect[i].Bottom = IPP_MIN(dirtyRects[i].Bottom * roiBlock.height, m_height);
        }

        tmpRect.NumRect = (mfxU16)dirtyRects.size();
    }

    resultIPP = ippsCopy_8u((const Ipp8u*)&tmpRect, (Ipp8u*)mfxRect, sizeof(mfxExtDirtyRect));
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

    ReleaseTask(task);

    return MFX_ERR_NONE;
}

mfxStatus CmDirtyRectFilter::GetParam(mfxFrameInfo& in, mfxFrameInfo& out)
{
    in;
    out;

    return MFX_ERR_UNSUPPORTED;
}

TaskContext* CmDirtyRectFilter::GetFreeTask()
{
    TaskContext* task = 0;
    if(0 == m_vpTasks.size())
        return 0;
    else
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        for(std::vector<TaskContext*>::iterator it = m_vpTasks.begin(); it != m_vpTasks.end(); ++it)
        {
            if((*it) && 0 == (*it)->busy)
            {
                task = (*it);
                ++(task->busy);
                return task;
            }
        }
    }
    return 0;
}

void CmDirtyRectFilter::ReleaseTask(TaskContext*& task)
{
    if(!task) return;
    --task->busy;
}

CmSurface2D* CmDirtyRectFilter::GetCMSurface(const mfxFrameSurface1* mfxSurf)
{
    int cmSts = 0;

    mfxFrameSurface1* opaqSurf = 0;
    if(m_bOpaqMem)
    {
        mfxStatus mfxSts = m_pmfxCore->GetRealSurface(m_pmfxCore->pthis, (mfxFrameSurface1*) mfxSurf, &opaqSurf);
        if(MFX_ERR_NONE == mfxSts || 0 == opaqSurf)
            return 0;
    }
    const mfxFrameSurface1* realSurf = opaqSurf ? opaqSurf : mfxSurf;

    CmSurface2D*& cmSurf = m_CMSurfaces[realSurf];

    bool sysSurf = false;
    mfxHDLPair native_surf = {};
    mfxStatus mfxSts = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, realSurf->Data.MemId, (mfxHDL*)&native_surf);
    if(MFX_ERR_NONE > mfxSts)
        sysSurf = true;

    if( sysSurf )
    {
        if(!cmSurf)
        {
            if(realSurf->Info.FourCC == MFX_FOURCC_NV12)
                cmSts = m_pCMDevice->CreateSurface2D(realSurf->Info.Width, realSurf->Info.Height, CM_SURFACE_FORMAT_NV12, cmSurf);
            else
                cmSts = m_pCMDevice->CreateSurface2D(realSurf->Info.Width, realSurf->Info.Height, CM_SURFACE_FORMAT_A8R8G8B8, cmSurf);
            assert(cmSts == 0);
            if(CM_SUCCESS != cmSts || 0 == cmSurf)
                return 0;
        }
        mfxSts = CMCopySysToGpu(cmSurf, (mfxFrameSurface1*) realSurf);
        assert(MFX_ERR_NONE == mfxSts);
        if(mfxSts)
            return 0;
    }
    else
    {
        if(!cmSurf)
        {
            cmSts = m_pCMDevice->CreateSurface2D(native_surf.first, cmSurf);
            if(CM_SUCCESS != cmSts || 0 == cmSurf)
                return 0;
        }
    }

    return cmSurf;
}

SurfaceIndex* CmDirtyRectFilter::GetCMSurfaceIndex(const mfxFrameSurface1* mfxSurf)
{
    SurfaceIndex * index = 0;
    CmSurface2D* cmSurf = GetCMSurface(mfxSurf);
    assert(0 != cmSurf);
    if(cmSurf)
        cmSurf->GetIndex(index);
    return index;
}

mfxStatus CmDirtyRectFilter::CMCopySysToGpu(CmSurface2D* cmDst, mfxFrameSurface1* mfxSrc)
{
    if(!mfxSrc)
        return MFX_ERR_NULL_PTR;

    mfxI32 cmSts = CM_SUCCESS;
    CM_STATUS sts;
    mfxStatus mfxSts = MFX_ERR_NONE;

    bool unlock_req = false;
    if(mfxSrc->Data.MemId)
    {
        mfxSts = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, mfxSrc->Data.MemId, &mfxSrc->Data);
        if(MFX_ERR_NONE != mfxSts)
            return mfxSts;
        unlock_req = true;
        if(!mfxSrc->Data.Y || !mfxSrc->Data.UV)
            return MFX_ERR_LOCK_MEMORY;
    }

    mfxU32 srcPitch = mfxSrc->Data.Pitch;
    CmEvent* e = NULL;
    if(MFX_FOURCC_NV12 == mfxSrc->Info.FourCC)
    {
        mfxI64 verticalPitch = (mfxI64)(mfxSrc->Data.UV - mfxSrc->Data.Y);
        verticalPitch = (verticalPitch % mfxSrc->Data.Pitch)? 0 : verticalPitch / mfxSrc->Data.Pitch;
        mfxI64& srcUVOffset = verticalPitch;

        if(srcUVOffset != 0){
            cmSts = m_pQueue->EnqueueCopyCPUToGPUFullStride(cmDst, mfxSrc->Data.Y, srcPitch, (mfxU32) srcUVOffset, 0, e);
        }
        else{
            cmSts = m_pQueue->EnqueueCopyCPUToGPUStride(cmDst, mfxSrc->Data.Y, srcPitch, e);
        }
    }
    else
    {
        mfxU8* src = IPP_MIN(IPP_MIN(mfxSrc->Data.R,mfxSrc->Data.G),mfxSrc->Data.B);
        mfxU32 srcUVOffset = mfxSrc->Info.Height;

        cmSts = m_pQueue->EnqueueCopyCPUToGPUFullStride(cmDst, src, srcPitch, srcUVOffset, 0, e);
    }



    if (CM_SUCCESS == cmSts && e)
    {
        e->GetStatus(sts);

        while (sts != CM_STATUS_FINISHED)
        {
            e->GetStatus(sts);
        }
    }else{
        mfxSts = MFX_ERR_DEVICE_FAILED;
    }
    if(e) m_pQueue->DestroyEvent(e);

    if(mfxSts)
        return mfxSts;

    if(unlock_req)
    {
        mfxSts = m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, mfxSrc->Data.MemId, &mfxSrc->Data);
    }

    return mfxSts;
}

void CmDirtyRectFilter::FreeSurfs()
{
    if(!m_pCMDevice || 0 == m_CMSurfaces.size()) return;
    UMC::AutomaticUMCMutex guard(m_guard);

    int result = 0;
    for(std::map<const mfxFrameSurface1*,CmSurface2D*>::iterator it = m_CMSurfaces.begin(); it != m_CMSurfaces.end(); ++it)
    {
        CmSurface2D*& cm_surf = it->second;
        if(cm_surf)
        {
            try{
                result = m_pCMDevice->DestroySurface(cm_surf);
                assert(result == 0);
                cm_surf = 0;
            }
            catch(...)
            {
            }
        }
    }

    m_CMSurfaces.clear();
    return;
}

mfxStatus CmDirtyRectFilter::Close()
{
    UMC::AutomaticUMCMutex guard(m_guard);
    FreeSurfs();

    for(std::vector<TaskContext*>::iterator it = m_vpTasks.begin(); it != m_vpTasks.end(); ++it)
    {
        TaskContext*& task = (*it);
        if(task)
        {
            if(m_pCMDevice)
            {
                if(task->pKernel)
                {
                    m_pCMDevice->DestroyKernel(task->pKernel);
                    task->pKernel = 0;
                }
                if(task->pTask)
                {
                    m_pCMDevice->DestroyTask(task->pTask);
                    task->pTask = 0;
                }
                if(task->pThreadSpace)
                {
                    m_pCMDevice->DestroyThreadSpace(task->pThreadSpace);
                    task->pThreadSpace = 0;
                }
                if(task->pQueue)
                {
                    //no func to destroy Queue
                    task->pQueue = 0;
                }

                if(task->roiMap.cmSurf)
                {
                    m_pCMDevice->DestroySurface2DUP(task->roiMap.cmSurf);
                    task->roiMap.cmSurf = 0;
                }
            }
            if(task->roiMap.roiMAP)
            {
                ippiFree(task->roiMap.roiMAP);
            }
            if(task->roiMap.distUpper)
            {
                ippiFree(task->roiMap.distUpper);
            }
            task->roiMap.distUpper = 0;
            if(task->roiMap.distLower)
            {
                ippiFree(task->roiMap.distLower);
            }
            task->roiMap.distLower = 0;
            if(task->roiMap.distLeft)
            {
                ippiFree(task->roiMap.distLeft);
            }
            task->roiMap.distLeft = 0;
            if(task->roiMap.distRight)
            {
                ippiFree(task->roiMap.distRight);
            }
            task->roiMap.distRight = 0;

            if(task->roiMap.physBuffer)
            {
                CM_ALIGNED_FREE(task->roiMap.physBuffer);
                task->roiMap.physBuffer = 0;
            }
            task->roiMap.physBufferPitch = 0;
            task->roiMap.physBufferSz = 0;

            task->roiMap.dirtyRects.clear();

            delete task;
            task = 0;
        }
    }
    m_vpTasks.clear();

    for(std::vector<CmProgram*>::iterator it = m_vPrograms.begin(); it != m_vPrograms.end(); ++it)
    {
        CmProgram*& program = (*it);
        if(m_pCMDevice)
            m_pCMDevice->DestroyProgram(program);
        program = 0;
    }
    m_vPrograms.clear();

    if(m_pCMDevice)
    {
        DestroyCmDevice(m_pCMDevice);
        m_pCMDevice = 0;
    }

    return MFX_ERR_NONE;

}

mfxStatus CmDirtyRectFilter::BuildRectangles(Ipp8u* roiMap, IppiSize roiMapSize, const mfxU32 maxRectsN, std::vector <intRect>& dstRects)
{
    Ipp8u* map = roiMap;        // map with '0' and '1'
    if(!map) return MFX_ERR_UNDEFINED_BEHAVIOR;

    Ipp64f res = 0.0f;
    mfxU64 totalRectsN = 0;

    IppStatus resultIPP = ippiSum_8u_C1R(roiMap, roiMapSize.width, roiMapSize, &res);
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

    totalRectsN = (int) (res + 0.5);

    Ipp8u  macroMap[W_BLOCKS * H_BLOCKS];
    Ipp16u densityMap[W_BLOCKS * H_BLOCKS];
    const IppiSize roiBlock16x16 = {W_BLOCKS, H_BLOCKS};

    resultIPP = ippiSet_8u_C1R(0, macroMap, roiBlock16x16.width, roiBlock16x16);
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);
    resultIPP = ippiSet_16u_C1R(0, densityMap, roiBlock16x16.width << 1, roiBlock16x16);
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

    const IppiSize roiBigBlock = {roiMapSize.width / W_BLOCKS, roiMapSize.height / H_BLOCKS};

    mfxU32 macroMapN = 0;

    if(totalRectsN > maxRectsN)
    {
        int    idxX, idxY;
        Ipp16u maxDensity;

        //calc density
        for(mfxU32 i = 0; i < H_BLOCKS; ++i)
        {
            for(mfxU32 j = 0; j < W_BLOCKS; ++j)
            {
                resultIPP = ippiSum_8u_C1R(roiMap + i * roiBigBlock.height * roiMapSize.width + j * roiBigBlock.width, roiMapSize.width, roiBigBlock, &res);
                MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);
                densityMap[i * roiBlock16x16.width + j] = (Ipp16u) (res + 0.5);
            }
        }

        //group cm blocks into big blocks by density
        do
        {
            resultIPP = ippiMax_16u_C1R(densityMap, roiBlock16x16.width << 1, roiBlock16x16, &maxDensity);
            MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

            for (mfxU32 i = 0; i < maxRectsN; ++i)
            {
                if (densityMap[i] + 3 < maxDensity) continue;

                //get idxX and idxY, where i = idxY * 16 + idxX
                idxX = i % 16;
                idxY = i / 16;

                resultIPP = ippiSet_8u_C1R(0, roiMap + idxY * roiBigBlock.height * roiMapSize.width + idxX * roiBigBlock.width, roiMapSize.width, roiBigBlock);
                MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

                totalRectsN -= (densityMap[i] - 1); // instead of all macroblocks we save only one big rectangle
                macroMap  [i] = 1;
                densityMap[i] = 1;

                macroMapN++;
            }
        } while (totalRectsN > maxRectsN);
    }

    intRect rct;

    if (macroMapN > 0)
    {
        for(mfxU32 i = 0; i < H_BLOCKS; ++i)
        {
            for(mfxU32 j = 0; j < W_BLOCKS; ++j)
            {
                if( macroMap[i * roiBlock16x16.width + j] )
                {
                    //roi
                    rct.Top    = i * roiBigBlock.height;
                    rct.Bottom = rct.Top + roiBigBlock.height;
                    rct.Left   = j * roiBigBlock.width;
                    rct.Right  = rct.Left + roiBigBlock.width;

                    dstRects.push_back(rct);
                }
            }
        }
    }

    if(totalRectsN - macroMapN > 0)
    {
        for(mfxI32 i = 0; i < roiMapSize.height; ++i)
        {
            for(mfxI32 j = 0; j < roiMapSize.width; ++j)
            {
                if( roiMap[i * roiMapSize.width + j] )
                {
                    //roi
                    rct.Top    = i;
                    rct.Bottom = rct.Top + 1;
                    rct.Left   = j;
                    rct.Right  = rct.Left + 1;

                    dstRects.push_back(rct);
                }
            }
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CmDirtyRectFilter::BuildRectanglesExt(Ipp8u* roiMap, Ipp16s* distUpper, Ipp16s* distLower, Ipp16s* distLeft, Ipp16s* distRight, IppiSize roiMapSize, std::vector <intRect>& dstRects)
{
    Ipp8u* map = roiMap;        // map with '0' and '1'
    if(!map) return MFX_ERR_UNDEFINED_BEHAVIOR;

    Ipp16s* du = distUpper;     // distance to the upper '1'
    Ipp16s* dd = distLower;     // distance to the lower '1'
    Ipp16s* dl = distLeft;      // distance to the left  '1'
    Ipp16s* dr = distRight;     // distance to the right '1'
    if(!du || !dd || !dl || !dr) return MFX_ERR_UNDEFINED_BEHAVIOR;

    IppiSize roi = roiMapSize;  // region of interest
    if (roi.width <= 0 || roi.height <= 0) return MFX_ERR_UNDEFINED_BEHAVIOR;

    Ipp16s n = (Ipp16s)roi.height,
           m = (Ipp16s)roi.width;

    IppiSize column = {1, roi.height};
    IppiSize line   = {roi.width,  1};

    ippiSet_16s_C1R(-1, dl + roi.width - 1, roi.width << 1, column);
    ippiSet_16s_C1R(m,  dr                , roi.width << 1, column);

    // building distances to the left and right '1'
    for (Ipp16s i = 0; i < m; ++i)
    {
        for (Ipp16s j = 0; j < n; ++j)
            if (0 == map[roi.width * j             + i])
                dl[roi.width * j + (m - 1)] = i;

        for (Ipp16s j = 0; j < n; ++j)
            if (0 == map[roi.width * j + ((m - 1) - i)])
                dr[roi.width * j          ] = (m - 1) - i;

        ippiCopy_16s_C1R(dl + roi.width - 1, roi.width << 1, dl + i            , roi.width << 1, column);
        ippiCopy_16s_C1R(dr                , roi.width << 1, dr + ((m - 1) - i), roi.width << 1, column);
    }

    ippiSet_16s_C1R(-1, du,                                roi.width << 1, line);
    ippiSet_16s_C1R(n,  dd + roi.width * (roi.height - 1), roi.width << 1, line);

    // building distances to the upper '1'
    for (Ipp16s i = 1; i < n; ++i)
        for (Ipp16s j = 0; j < m; ++j)
            du[roi.width * i + j] = (dl[roi.width * i + j] == dl[roi.width * (i - 1) + j] && dr[roi.width * i + j] == dr[roi.width * (i - 1) + j])
                                  ? du[roi.width * (i - 1) + j] // previous value is used only if distances to the left and right '1' are equal to avoid intersections
                                  : i - 1;

    // building distances to the lower '1'
    for (Ipp16s i = n-2; i >= 0; --i)
        for (Ipp16s j = 0; j < m; ++j)
            dd[roi.width * i + j] = (dl[roi.width * i + j] == dl[roi.width * (i + 1) + j] && dr[roi.width * i + j] == dr[roi.width * (i + 1) + j])
                                  ? dd[roi.width * (i + 1) + j] // previous value is used only if distances to the left and right '1' are equal to avoid intersections
                                  : i + 1;

    intRect rct;
    Ipp16u cur, lower, right;
    bool bLowerDif = false,
         bRightDif = false;

    // building rectangles
    for (Ipp16s i = 0; i < n; i++)
    {
        for (Ipp16s j = 0; j < m; j++)
        {
            cur = (Ipp16u)(i*roi.width + j); lower = (Ipp16u)((i+1)*roi.width + j); right = (Ipp16u)(i*roi.width + j + 1);
            bLowerDif = (i < n - 1) ? (du[cur] ^ du[lower] || dd[cur] ^ dd[lower] || dl[cur] ^ dl[lower] || dr[cur] ^ dr[lower]) : true; // compare to the lower rectangle if exists
            bRightDif = (j < m - 1) ? (du[cur] ^ du[right] || dd[cur] ^ dd[right] || dl[cur] ^ dl[right] || dr[cur] ^ dr[right]) : true; // compare to the right rectangle if exists

            if ( bLowerDif && bRightDif) // we should add rectangle only if it is different from lower and right rectangle to avoid rectangle duplication
            {
                rct.Left   = dl[i*roi.width + j] + 1;
                rct.Top    = du[i*roi.width + j] + 1;
                rct.Right  = dr[i*roi.width + j];
                rct.Bottom = i + 1;

                if (rct.Left >= rct.Right || rct.Top >= rct.Bottom) continue; //skipping wrong rectangles

                dstRects.push_back(rct);
            }
        }
    }

    return MFX_ERR_NONE;
}


} //namespace MfxCapture
